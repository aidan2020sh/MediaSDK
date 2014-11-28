//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "ippi.h"
#include "mfx_h265_defs.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_optimization.h"

using namespace MFX_HEVC_PP;

namespace H265Enc {

template <typename PixType>
bool H265CU<PixType>::CheckIdenticalMotion(const Ipp8s refIdx[2], const H265MV mvs[2]) const
{
    if (m_cslice->slice_type == B_SLICE && !m_par->cpps->weighted_bipred_flag) {
        if (refIdx[0] >= 0 && refIdx[1] >= 0) {
            H265Frame *refL0 = m_currFrame->m_refPicList[0].m_refFrames[refIdx[0]];
            H265Frame *refL1 = m_currFrame->m_refPicList[1].m_refFrames[refIdx[1]];
            if (refL0 == refL1 && mvs[0] == mvs[1])
                return true;
        }
    }
    return false;
}

template <typename PixType>
Ipp32s H265CU<PixType>::ClipMV(H265MV& rcMv) const
{
    Ipp32s change = 0;
    if (rcMv.mvx < HorMin) {
        change = 1;
        rcMv.mvx = (Ipp16s)HorMin;
    } else if (rcMv.mvx > HorMax) {
        change = 1;
        rcMv.mvx = (Ipp16s)HorMax;
    }
    if (rcMv.mvy < VerMin) {
        change = 1;
        rcMv.mvy = (Ipp16s)VerMin;
    } else if (rcMv.mvy > VerMax) {
        change = 1;
        rcMv.mvy = (Ipp16s)VerMax;
    }

    return change;
}

const Ipp16s g_lumaInterpolateFilter[4][8] =
{
    {  0, 0,   0, 64,  0,   0, 0,  0 },
    { -1, 4, -10, 58, 17,  -5, 1,  0 },
    { -1, 4, -11, 40, 40, -11, 4, -1 },
    {  0, 1,  -5, 17, 58, -10, 4, -1 }
};

const Ipp16s g_chromaInterpolateFilter[8][4] =
{
    {  0, 64,  0,  0 },
    { -2, 58, 10, -2 },
    { -4, 54, 16, -2 },
    { -6, 46, 28, -4 },
    { -4, 36, 36, -4 },
    { -4, 28, 46, -6 },
    { -2, 16, 54, -4 },
    { -2, 10, 58, -2 }
};

//template <typename PixType>
//static void CopyPU(const PixType *in_pSrc,
//                          Ipp32u in_SrcPitch, // in samples
//                          Ipp16s* in_pDst,
//                          Ipp32u in_DstPitch, // in samples
//                          Ipp32s width,
//                          Ipp32s height,
//                          Ipp32s shift)
//{
//    const PixType *pSrc = in_pSrc;
//    Ipp16s *pDst = in_pDst;
//    Ipp32s i, j;
//
//    for (j = 0; j < height; j++)
//    {
//        for (i = 0; i < width; i++)
//        {
//            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << shift);
//        }
//
//        pSrc += in_SrcPitch;
//        pDst += in_DstPitch;
//    }
//}

template <typename PixType>
void WriteAverageToPic(
    const PixType * pSrc0,
    Ipp32u        in_Src0Pitch,      // in samples
    const PixType * pSrc1,
    Ipp32u        in_Src1Pitch,      // in samples
    PixType * H265_RESTRICT pDst,
    Ipp32u        in_DstPitch,       // in samples
    Ipp32s        width,
    Ipp32s        height)
{
#ifdef __INTEL_COMPILER
    #pragma ivdep
#endif // __INTEL_COMPILER
    for (int j = 0; j < height; j++)
    {
#ifdef __INTEL_COMPILER
        #pragma ivdep
        #pragma vector always
#endif // __INTEL_COMPILER
        for (int i = 0; i < width; i++)
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

template <typename PixType, EnumTextType PLANE_TYPE>
PixType *GetRefPointer(H265Frame *refFrame, Ipp32s blockX, Ipp32s blockY, const H265MV &mv, Ipp32s chromaShiftH)
{
    return (PLANE_TYPE == TEXT_LUMA)
        ? (PixType*)refFrame->y  + blockX + (mv.mvx >> 2) + (blockY + (mv.mvy >> 2)) * refFrame->pitch_luma_pix
        : (PixType*)refFrame->uv + blockX + (mv.mvx >> 3 << 1) + ((blockY >> chromaShiftH) + (mv.mvy >> (2+chromaShiftH))) * refFrame->pitch_chroma_pix;
}

template <typename PixType>
template <EnumTextType PLANE_TYPE>
void H265CU<PixType>::PredInterUni(Ipp32s puX, Ipp32s puY, Ipp32s puW, Ipp32s puH, Ipp32s listIdx,
                          const Ipp8s refIdx[2], const H265MV mvs[2], PixType *dst, Ipp32s dstPitch,
                          Ipp32s isBiPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage, Ipp32s isFast)
{
    const Ipp32s isLuma = (PLANE_TYPE == TEXT_LUMA);
    RefPicList *refPicList = m_currFrame->m_refPicList;
    H265Frame *ref = refPicList[listIdx].m_refFrames[refIdx[listIdx]];
    Ipp32s listIdx2 = !listIdx;

    Ipp32s bitDepth, tap, dx, dy;
    if (PLANE_TYPE == TEXT_LUMA) {
        bitDepth = m_par->bitDepthLuma;
        isFast ? tap = 4 : tap = 8;
        dx = mvs[listIdx].mvx & 3;
        dy = mvs[listIdx].mvy & 3;
    }
    else {
        puW >>= m_par->chromaShiftW;
        puH >>= m_par->chromaShiftH;
        bitDepth = m_par->bitDepthChroma;
        tap = 4;
        Ipp32s shiftHor = 2 + m_par->chromaShiftW;
        Ipp32s shiftVer = 2 + m_par->chromaShiftH;
        Ipp32s maskHor = (1 << shiftHor) - 1;
        Ipp32s maskVer = (1 << shiftVer) - 1;
        dx = mvs[listIdx].mvx & maskHor;
        dy = mvs[listIdx].mvy & maskVer;
        dx <<= m_par->chromaShiftWInv;
        dy <<= m_par->chromaShiftHInv;
    }

    Ipp32s srcPitch2 = 0;
    PixType *src2 = NULL;
    if (eAddAverage == AVERAGE_FROM_PIC) {
        Ipp8s refIdx2 = refIdx[listIdx2];
        H265Frame *ref2 = refPicList[listIdx2].m_refFrames[refIdx2];
        srcPitch2 = isLuma ? ref2->pitch_luma_pix : ref2->pitch_chroma_pix;
        src2 = GetRefPointer<PixType, PLANE_TYPE>(ref2, (Ipp32s)m_ctbPelX + puX, (Ipp32s)m_ctbPelY + puY, mvs[listIdx2], m_par->chromaShiftH);
    }

    Ipp32s srcPitch = isLuma ? ref->pitch_luma_pix : ref->pitch_chroma_pix;
    PixType *src = GetRefPointer<PixType, PLANE_TYPE>(ref, (Ipp32s)m_ctbPelX + puX, (Ipp32s)m_ctbPelY + puY, mvs[listIdx], m_par->chromaShiftH);

    Ipp32s dstBufPitch = MAX_CU_SIZE << (!isLuma ? m_par->chromaShiftWInv : 0);
    Ipp16s *dstBuf = m_interpBuf;

    Ipp32s dstPicPitch = dstPitch;
    PixType *dstPic = dst;

    Ipp32s shift = 6;
    Ipp32s offset = 32;

    if (isBiPred) {
        shift = bitDepth - 8;
        offset = 0;
    }

    const UMC_HEVC_DECODER::EnumTextType PLANE_TYPE_ = (UMC_HEVC_DECODER::EnumTextType)PLANE_TYPE;

    if (dx == 0 && dy == 0) {
        if (PLANE_TYPE == TEXT_CHROMA)
            puW <<= 1;
        shift = isBiPred ? 14 - bitDepth : 0;

        if (!isBiPred) {
            IppiSize roi = { puW, puH };
            _ippiCopy_C1R(src, srcPitch, dstPic, dstPicPitch, roi);
        }
        else {
            if (eAddAverage == MFX_HEVC_PP::AVERAGE_FROM_PIC) {
                WriteAverageToPic(src, srcPitch, src2, srcPitch2, dstPic, dstPicPitch, puW, puH);
            } else {
                VM_ASSERT(0); // should not be here
            }
        }
    }
    else if (dy == 0) {
        if (!isBiPred) // Write directly into buffer
            Interpolate<PLANE_TYPE_>(INTERP_HOR, src, srcPitch, dstPic, dstPicPitch, dx, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<PLANE_TYPE_>(INTERP_HOR, src, srcPitch, dstBuf, dstBufPitch, dx, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<PLANE_TYPE_>(INTERP_HOR, src, srcPitch, dstPic, dstPicPitch, dx, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_BUF, dstBuf, dstBufPitch);
        else if (eAddAverage == AVERAGE_FROM_PIC)
            Interpolate<PLANE_TYPE_>(INTERP_HOR, src, srcPitch, dstPic, dstPicPitch, dx, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_PIC, src2, srcPitch2);
    }
    else if (dx == 0) {
        if (!isBiPred) // Write directly into buffer
            Interpolate<PLANE_TYPE_>(INTERP_VER, src, srcPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<PLANE_TYPE_>(INTERP_VER, src, srcPitch, dstBuf, dstBufPitch, dy, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<PLANE_TYPE_>(INTERP_VER, src, srcPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_BUF, dstBuf, dstBufPitch);
        else if (eAddAverage == AVERAGE_FROM_PIC)
            Interpolate<PLANE_TYPE_>(INTERP_VER, src, srcPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_PIC, src2, srcPitch2);
    }
    else {
        Ipp16s horBuf[80 * 80];
        const Ipp32s horBufPitch = 80;
        Ipp16s *horBufPtr = horBuf + 8 + 8 * horBufPitch;

        Interpolate<PLANE_TYPE_>(INTERP_HOR,  src - ((tap >> 1) - 1) * srcPitch, srcPitch, horBufPtr, horBufPitch, dx, puW, puH + tap - 1, bitDepth - 8, 0, bitDepth, isFast);

        shift = 20 - bitDepth;
        offset = 1 << (19 - bitDepth);
        if (isBiPred) {
            shift = 6;
            offset = 0;
        }

        horBufPtr += ((tap >> 1) - 1) * horBufPitch;
        if (!isBiPred) // Write directly into buffer
            Interpolate<PLANE_TYPE_>(INTERP_VER, horBufPtr, horBufPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_NO)
            Interpolate<PLANE_TYPE_>(INTERP_VER, horBufPtr, horBufPitch, dstBuf, dstBufPitch, dy, puW, puH, shift, offset, bitDepth, isFast);
        else if (eAddAverage == AVERAGE_FROM_BUF)
            Interpolate<PLANE_TYPE_>(INTERP_VER, horBufPtr, horBufPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_BUF, dstBuf, dstBufPitch);
        else // eAddAverage == AVERAGE_FROM_PIC
            Interpolate<PLANE_TYPE_>(INTERP_VER, horBufPtr, horBufPitch, dstPic, dstPicPitch, dy, puW, puH, shift, offset, bitDepth, isFast, AVERAGE_FROM_PIC, src2, srcPitch2);
    }
}

template <typename PixType>
template <EnumTextType PLANE_TYPE>
void H265CU<PixType>::InterPredCu(Ipp32s absPartIdx, Ipp8u depth, PixType *dst, Ipp32s dstPitch)
{
    const H265CUData *dataCu = m_data + absPartIdx;
    const Ipp32s isLuma = (PLANE_TYPE == TEXT_LUMA);
    Ipp32u numParts = (m_par->NumPartInCU >> (depth << 1));

    Ipp32s cuRasterIdx = h265_scan_z2r4[absPartIdx];
    Ipp32s cuY = (cuRasterIdx >> 4) << m_par->QuadtreeTULog2MinSize;
    Ipp32s cuX = (cuRasterIdx & 15) << m_par->QuadtreeTULog2MinSize;

    Ipp32s numPu = h265_numPu[dataCu->partSize];
    for (Ipp32s partIdx = 0; partIdx < numPu; partIdx++) {
        Ipp32s puX, puY, puW, puH;
        GetPartOffsetAndSize(partIdx, dataCu->partSize, dataCu->size, puX, puY, puW, puH);
        puX += cuX;
        puY += cuY;
        PixType *dstPtr = dst;
        if (isLuma) dstPtr += puX + (puY * dstPitch);
        else dstPtr += (puX << m_par->chromaShiftWInv) + (puY * dstPitch >> m_par->chromaShiftH);

        Ipp32s partAddr;
        GetPartAddr(partIdx, dataCu->partSize, numParts, partAddr);
        const H265CUData *dataPu = dataCu + partAddr;

        const Ipp8s *refIdx = dataPu->refIdx;
        H265MV mvs[2] = { dataPu->mv[0], dataPu->mv[1] };

        if (CheckIdenticalMotion(refIdx, mvs) || refIdx[0] < 0 || refIdx[1] < 0) {
            Ipp32s listIdx = !!(refIdx[0] < 0);
            ClipMV(mvs[listIdx]);
            PredInterUni<PLANE_TYPE>(puX, puY, puW, puH, listIdx, refIdx, mvs, dstPtr, dstPitch, false, AVERAGE_NO, 0);
        }
        else {
            ClipMV(mvs[0]);
            ClipMV(mvs[1]);
            Ipp32s shiftHor = isLuma ? 2 : (2 + m_par->chromaShiftW);
            Ipp32s shiftVer = isLuma ? 2 : (2 + m_par->chromaShiftH);
            Ipp32s maskHor = (1 << shiftHor) - 1;
            Ipp32s maskVer = (1 << shiftVer) - 1;

            Ipp32s interpFlag0 = (mvs[0].mvx & maskHor) | (mvs[0].mvy & maskVer);
            Ipp32s interpFlag1 = (mvs[1].mvx & maskHor) | (mvs[1].mvy & maskVer);
            bool onlyOneIterp = !(interpFlag0 && interpFlag1);

            if (onlyOneIterp) {
                Ipp32s listIdx = !interpFlag0;
                PredInterUni<PLANE_TYPE>(puX, puY, puW, puH, listIdx, refIdx, mvs, dstPtr, dstPitch, true, AVERAGE_FROM_PIC, 0);
            }
            else {
                PredInterUni<PLANE_TYPE>(puX, puY, puW, puH, 0, refIdx, mvs, dstPtr, dstPitch, true, AVERAGE_NO, 0);
                PredInterUni<PLANE_TYPE>(puX, puY, puW, puH, 1, refIdx, mvs, dstPtr, dstPitch, true, AVERAGE_FROM_BUF, 0);
            }
        }
    }
}

template
void H265CU<Ipp8u>::InterPredCu<TEXT_LUMA>(Ipp32s absPartIdx, Ipp8u depth, Ipp8u *dst, Ipp32s dstPitch);
template
void H265CU<Ipp8u>::InterPredCu<TEXT_CHROMA>(Ipp32s absPartIdx, Ipp8u depth, Ipp8u *dst, Ipp32s dstPitch);
template
void H265CU<Ipp16u>::InterPredCu<TEXT_LUMA>(Ipp32s absPartIdx, Ipp8u depth, Ipp16u *dst, Ipp32s dstPitch);
template
void H265CU<Ipp16u>::InterPredCu<TEXT_CHROMA>(Ipp32s absPartIdx, Ipp8u depth, Ipp16u *dst, Ipp32s dstPitch);

#ifdef MEMOIZE_SUBPEL
template <typename PixType>
void H265CU<PixType>::MeInterpolateSave(const H265MEInfo* me_info, const H265MV *MV, PixType *src,
                                    Ipp32s srcPitch, PixType *dst, Ipp32s dstPitch, PixType *hdst, Ipp16s *dstHi, Ipp16s *hdstHi) const
{
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx = MV->mvx & 3;
    Ipp32s dy = MV->mvy & 3;
    Ipp32s bitDepth = m_par->bitDepthLuma;
    Ipp32s isFast = false;
    Ipp32s refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 2) + (m_ctbPelY + me_info->posy + (MV->mvy >> 2)) * srcPitch;
    src += refOffset;

    VM_ASSERT (!(dx == 0 && dy == 0));
    if (dy == 0)
    {
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dstHi, dstPitch, dx, w, h, bitDepth - 8, 0, bitDepth, isFast);
        h265_InterpLumaPack(dstHi, dstPitch, dst, dstPitch, w, h, bitDepth);
    }
    else if (dx == 0)
    {
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dstHi, dstPitch, dy, w, h, bitDepth - 8, 0, bitDepth, isFast);
        h265_InterpLumaPack(dstHi, dstPitch, dst, dstPitch, w, h, bitDepth);
    }
    else
    {
        if(hdstHi) {
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, hdstHi, dstPitch, dx, w, h + 8, bitDepth - 8, 0, bitDepth, isFast);
            h265_InterpLumaPack(hdstHi, dstPitch, hdst, dstPitch, w, h+6, bitDepth);

            Ipp32s shift  = 6;
            Ipp16s offset = 0;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  hdstHi + 3 * dstPitch, dstPitch, dstHi, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
            h265_InterpLumaPack(dstHi, dstPitch, dst, dstPitch, w, h, bitDepth);
        } else {
            Ipp16s tmpBuf[((64+8)+8+8) * ((64+8)+8+8)];
            Ipp16s *tmp = tmpBuf + ((64+8)+8+8) * 8 + 8;
            Ipp32s tmpPitch = ((64+8)+8+8);
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 8, bitDepth - 8, 0, bitDepth, isFast);

            Ipp32s shift  = 6;
            Ipp16s offset = 0;
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dstHi, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
            h265_InterpLumaPack(dstHi, dstPitch, dst, dstPitch, w, h, bitDepth);
        }

    }
    return;
} // void H265CU::MeInterpolate(...)
#endif

template <typename PixType>
void H265CU<PixType>::MeInterpolate(const H265MEInfo* me_info, const H265MV *MV, PixType *src,
                                    Ipp32s srcPitch, PixType *dst, Ipp32s dstPitch, Ipp32s isFast) const
{
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx = MV->mvx & 3;
    Ipp32s dy = MV->mvy & 3;
    Ipp32s bitDepth = m_par->bitDepthLuma;

    Ipp32s refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 2) + (m_ctbPelY + me_info->posy + (MV->mvy >> 2)) * srcPitch;
    src += refOffset;

    VM_ASSERT (!(dx == 0 && dy == 0));
    if (dy == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32, bitDepth, isFast);
    }
    else if (dx == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32, bitDepth, isFast);
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

        if (isFast)
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 1 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 3, bitDepth - 8, 0, bitDepth, isFast);
        else
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 7, bitDepth - 8, 0, bitDepth, isFast);

        Ipp32s shift  = 20 - bitDepth;
        Ipp16s offset = 1 << (shift - 1);

        if (isFast)
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 1 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);
        else
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, isFast);

    }

    return;
} // void H265CU::MeInterpolate(...)

template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
