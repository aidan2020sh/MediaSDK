//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_optimization.h"

using namespace MFX_HEVC_COMMON;

//========================================================
void InterpolateHor(
    EnumTextType plane_type,
    const PixType *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset);

void InterpolateVert(
    EnumTextType plane_type,
    const Ipp16s *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset);

void InterpolateVert0(
    EnumTextType plane_type,
    const PixType *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset);

//=========================================================

bool H265CU::CheckIdenticalMotion(Ipp32u abs_part_idx)
{
    EncoderRefPicListStruct *pList[2];
    T_RefIdx ref_idx[2];
    pList[0] = &(par->cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(par->cslice->m_pRefPicList[0].m_RefPicListL1);
    ref_idx[0] = data[abs_part_idx].ref_idx[0];
    ref_idx[1] = data[abs_part_idx].ref_idx[1];
// TODO optimize: check B_SLISE && !weighted on entrance, POC after mv matched
    if(par->cslice->slice_type == B_SLICE && !par->cpps->weighted_bipred_flag)
    {
        if(ref_idx[0] >= 0 && ref_idx[1] >= 0)
        {
            Ipp32s RefPOCL0 = pList[0]->m_RefPicList[ref_idx[0]]->PicOrderCnt();
            Ipp32s RefPOCL1 = pList[1]->m_RefPicList[ref_idx[1]]->PicOrderCnt();
            if(RefPOCL0 == RefPOCL1 &&
                data[abs_part_idx].mv[0] == data[abs_part_idx].mv[1])
            {
                return true;
            }
        }
    }
    return false;
}

void H265CU::clipMV(H265MV& rcMv)
{
    Ipp32s MvShift = 2;
    Ipp32s offset = 8;
    Ipp32s HorMax = (par->Width + offset - ctb_pelx - 1) << MvShift;
    Ipp32s HorMin = ( - (Ipp32s) par->MaxCUSize - offset - (Ipp32s) ctb_pelx + 1) << MvShift;

    Ipp32s VerMax = (par->Height + offset - ctb_pely - 1) << MvShift;
    Ipp32s VerMin = ( - (Ipp32s) par->MaxCUSize - offset - (Ipp32s) ctb_pely + 1) << MvShift;

    rcMv.mvx = (Ipp16s) IPP_MIN(HorMax, IPP_MAX(HorMin, rcMv.mvx));
    rcMv.mvy = (Ipp16s) IPP_MIN(VerMax, IPP_MAX(VerMin, rcMv.mvy));
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

void InterpolateHor(
    EnumTextType plane_type,
    const PixType *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset)
{
    const PixType *pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k, tap;

    if (plane_type == TEXT_LUMA)
    {
        tap = 8;
        coeffs = &g_lumaInterpolateFilter[tab_index][0];
    }
    else
    {
        tap = 4;
        coeffs = &g_chromaInterpolateFilter[tab_index][0];
    }

    pSrc -= ((tap >> 1) - 1);

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i+k]) * coeffs[k];

            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void InterpolateVert(
    EnumTextType plane_type,
    const Ipp16s *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset)
{
    const Ipp16s *pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k, tap;

    if (plane_type == TEXT_LUMA)
    {
        tap = 8;
        coeffs = &g_lumaInterpolateFilter[tab_index][0];
    }
    else
    {
        tap = 4;
        coeffs = &g_chromaInterpolateFilter[tab_index][0];
    }

    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i+k*in_SrcPitch]) * coeffs[k];

            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

void InterpolateVert0(
    EnumTextType plane_type,
    const PixType *in_pSrc,
    Ipp32u in_SrcPitch, // in samples
    Ipp16s* in_pDst,
    Ipp32u in_DstPitch, // in samples
    Ipp32s tab_index,
    Ipp32s width,
    Ipp32s height,
    Ipp32s shift,
    Ipp32s offset)
{
    const PixType *pSrc = (Ipp8u*)in_pSrc;
    Ipp16s *pDst = in_pDst;
    const Ipp16s *coeffs;
    Ipp32s i, j, k, tap;

    if (plane_type == TEXT_LUMA)
    {
        tap = 8;
        coeffs = &g_lumaInterpolateFilter[tab_index][0];
    }
    else
    {
        tap = 4;
        coeffs = &g_chromaInterpolateFilter[tab_index][0];
    }

    pSrc -= ((tap >> 1) - 1) * in_SrcPitch;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            Ipp32s tmp = 0;

            for (k = 0; k < tap; k++)
            {
                tmp += ((Ipp32s)pSrc[i+k*in_SrcPitch]) * coeffs[k];

            }
            pDst[i] = (Ipp16s)((tmp + offset) >> shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}

static void CopyPU(const PixType *in_pSrc,
                          Ipp32u in_SrcPitch, // in samples
                          Ipp16s* in_pDst,
                          Ipp32u in_DstPitch, // in samples
                          Ipp32s width,
                          Ipp32s height,
                          Ipp32s shift)
{
    const PixType *pSrc = in_pSrc;
    Ipp16s *pDst = in_pDst;
    Ipp32s i, j;

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            pDst[i] = (Ipp16s)(((Ipp32s)pSrc[i]) << shift);
        }

        pSrc += in_SrcPitch;
        pDst += in_DstPitch;
    }
}


void H265CU::PredInterUni(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height,
                          EnumRefPicList RefPicList, Ipp32s PartIdx, bool bi, Ipp8u is_luma)
{
    EncoderRefPicListStruct *pList[2];
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][PartAddr];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    pList[0] = &(par->cslice->m_pRefPicList[0].m_RefPicListL0);
    pList[1] = &(par->cslice->m_pRefPicList[0].m_RefPicListL1);
    Ipp32s RefIdx = data[PartAddr].ref_idx[RefPicList];
    VM_ASSERT(RefIdx >= 0);

    H265MV MV = data[PartAddr].mv[RefPicList];
    clipMV(MV);

    H265Frame *PicYUVRef = pList[RefPicList]->m_RefPicList[RefIdx];

    Ipp32s in_SrcPitch, in_DstPitch, refOffset;
    PixType *in_pSrc;
    Ipp16s *in_pDst;

    Ipp32s in_dx, in_dy;
    Ipp32s bitDepth, shift, offset, tap;

    if (is_luma) {
        // LUMA
        in_SrcPitch = PicYUVRef->pitch_luma;
        refOffset = ctb_pelx + (MV.mvx >> 2) + (ctb_pely + (MV.mvy >> 2)) * in_SrcPitch;
        in_pSrc = PicYUVRef->y +
            ((PUStartRow * pitch_rec_luma + PUStartColumn) << par->Log2MinTUSize) + refOffset;

        in_DstPitch = par->MaxCUSize;
        in_pDst = pred_buf_y[RefPicList] + ((PUStartRow * par->MaxCUSize + PUStartColumn) << par->Log2MinTUSize);

        bitDepth = BIT_DEPTH_LUMA;
        tap = 8;
        in_dx = MV.mvx & 3;
        in_dy = MV.mvy & 3;

        shift = 6;
        offset = 32;

        if (bi)
        {
            shift = bitDepth - 8;
            offset = 0;
        }

        if ((in_dx == 0) && (in_dy == 0))
        {
            shift = 0;

            if (bi)
                shift = 14 - bitDepth;

            CopyPU(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height, shift);
        }
        else if (in_dy == 0)
        {
            /*InterpolateHor_opt(
                TEXT_LUMA,
                in_pSrc,
                in_SrcPitch,
                in_pDst,
                in_DstPitch,
                in_dx,
                Width,
                Height,
                shift,
                offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, (Ipp16s)offset);
        }
        else if (in_dx == 0)
        {
            /*InterpolateVert0_opt(
                TEXT_LUMA,
                in_pSrc,
                in_SrcPitch,
                in_pDst,
                in_DstPitch,
                in_dy,
                Width,
                Height,
                shift,
                offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
        }
        else
        {
            Ipp16s tmp[80 * 80];
            Ipp16s *tmpBuf = tmp + 80 * 8 + 8;

            shift = 20 - bitDepth;
            offset = 1 << (19 - bitDepth);

            if (bi)
            {
                shift = 6;
                offset = 0;
            }

            /*InterpolateHor_opt(
                TEXT_LUMA,
                in_pSrc - ((tap >> 1) - 1) * in_SrcPitch,
                in_SrcPitch,
                tmpBuf,
                80,
                in_dx,
                Width,
                Height + tap,
                bitDepth - 8,
                0);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80, in_dx, Width, Height + tap, bitDepth - 8, (Ipp16s)0);

            /*InterpolateVert_opt(
                TEXT_LUMA,
                tmpBuf + ((tap >> 1) - 1) * 80,
                80,
                in_pDst,
                in_DstPitch,
                in_dy,
                Width,
                Height,
                shift,
                offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
        }
    } else {
        // CHROMA
        Width >>= 1;
        Height >>= 1;
        in_SrcPitch = PicYUVRef->pitch_chroma;
        in_DstPitch = par->MaxCUSize >> 1;
        refOffset = (ctb_pelx >> 1) + (MV.mvx >> 3) + ((MV.mvy >> 3) + (ctb_pely >> 1)) * in_SrcPitch;

        in_pSrc = PicYUVRef->u +
            ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1)) + refOffset;
        in_pDst = pred_buf_u[RefPicList] + ((PUStartRow * in_DstPitch + PUStartColumn) << (par->Log2MinTUSize - 1));

        bitDepth = BIT_DEPTH_CHROMA;
        tap = 4;
        in_dx = MV.mvx & 7;
        in_dy = MV.mvy & 7;

        // CHROMA U

        shift = 6;
        offset = 32;

        if (bi)
        {
            shift = bitDepth - 8;
            offset = 0;
        }

        if ((in_dx == 0) && (in_dy == 0))
        {
            shift = 0;

            if (bi)
                shift = 14 - bitDepth;

            CopyPU(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height, shift);
        }
        else if (in_dy == 0)
        {
            /*InterpolateHor_opt(TEXT_CHROMA, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                           in_dx, Width, Height, shift, offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_U>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, (Ipp16s)offset);
        }
        else if (in_dx == 0)
        {
            /*InterpolateVert0_opt(TEXT_CHROMA, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy,
                             Width, Height, shift, offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_U>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
            
        }
        else
        {
            Ipp16s tmp[80 * 80];
            Ipp16s *tmpBuf = tmp + 80 * 8 + 8;

            shift = 20 - bitDepth;
            offset = 1 << (19 - bitDepth);

            if (bi)
            {
                shift = 6;
                offset = 0;
            }

            /*InterpolateHor_opt(TEXT_CHROMA, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                           in_dx, Width, Height + tap, bitDepth - 8, 0);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_U>( INTERP_HOR, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80, in_dx, Width, Height + tap, bitDepth - 8, 0);
            
            /*InterpolateVert_opt(TEXT_CHROMA, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                            in_dy, Width, Height, shift, offset);*/
            
            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_U>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
        }

        // CHROMA V
        in_pSrc = PicYUVRef->v +
            ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1)) + refOffset;
        in_pDst = pred_buf_v[RefPicList] + ((PUStartRow * in_DstPitch + PUStartColumn) << (par->Log2MinTUSize - 1));

        if ((in_dx == 0) && (in_dy == 0))
        {
            CopyPU(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, Width, Height, shift);
        }
        else if (in_dy == 0)
        {
            /*InterpolateHor_opt(TEXT_CHROMA, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch,
                           in_dx, Width, Height, shift, offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_V>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, Width, Height, shift, (Ipp16s)offset);
        }
        else if (in_dx == 0)
        {
            /*InterpolateVert0_opt(TEXT_CHROMA, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy,
                             Width, Height, shift, offset);*/

            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_V>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
        }
        else
        {
            Ipp16s tmp[80 * 80];
            Ipp16s *tmpBuf = tmp + 80 * 8 + 8;

            /*InterpolateHor_opt(TEXT_CHROMA, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80,
                           in_dx, Width, Height + tap, bitDepth - 8, 0);*/
            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_V>( INTERP_HOR, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80, in_dx, Width, Height + tap, bitDepth - 8, 0);

            /*InterpolateVert_opt(TEXT_CHROMA, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch,
                            in_dy, Width, Height, shift, offset);*/
            Interpolate<UMC_HEVC_DECODER::TEXT_CHROMA_V>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch, in_dy, Width, Height, shift, (Ipp16s)offset);
        }
    }
}

void H265CU::CopySaturate(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, EnumRefPicList RefPicList, Ipp8u is_luma)
{
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][PartAddr];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    Ipp32s SrcStride, DstStride;
    Ipp16s *pSrc0;
    PixType *pDst;

    if (is_luma) {
        SrcStride = par->MaxCUSize;
        pSrc0 = pred_buf_y[RefPicList] + ((PUStartRow * par->MaxCUSize + PUStartColumn) << par->Log2MinTUSize);

        DstStride = pitch_rec_luma;
        pDst = y_rec + ((PUStartRow * pitch_rec_luma + PUStartColumn) << par->Log2MinTUSize);

        for (Ipp32s y = 0; y < Height; y++)
        {
            for (Ipp32s x = 0; x < Width; x++)
            {
                pDst[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1, pSrc0[x]);
            }
            pSrc0 += SrcStride;
            pDst += DstStride;
        }
    } else {
        SrcStride = par->MaxCUSize >> 1;
        Ipp16s *pSrcU0 = pred_buf_u[RefPicList] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        Ipp16s *pSrcV0 = pred_buf_v[RefPicList] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        PixType *pDstU = u_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));
        PixType *pDstV = v_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));

        DstStride = pitch_rec_chroma;
        Width >>= 1;
        Height >>= 1;

        for (Ipp32s y = 0; y < Height; y++)
        {
            for (Ipp32s x = 0; x < Width; x++)
            {
                pDstU[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_CHROMA) - 1, pSrcU0[x]);
                pDstV[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_CHROMA) - 1, pSrcV0[x]);
            }
            pSrcU0 += SrcStride;
            pDstU += DstStride;
            pSrcV0 += SrcStride;
            pDstV += DstStride;
        }
    }
}


void H265CU::AddAverage(Ipp32u PartAddr, Ipp32s Width, Ipp32s Height, Ipp8u is_luma)
{
    Ipp32s maxDepth = par->Log2MaxCUSize - par->Log2MinTUSize;
    Ipp32s numMinTUInLCU = 1 << maxDepth;
    Ipp32s PURasterIdx = h265_scan_z2r[maxDepth][PartAddr];
    Ipp32s PUStartRow = PURasterIdx >> maxDepth;
    Ipp32s PUStartColumn = PURasterIdx & (numMinTUInLCU - 1);

    Ipp32s SrcStride, DstStride, shift, offset;

    // LUMA

    if (is_luma) {
        SrcStride = par->MaxCUSize;
        Ipp16s *pSrc0 = pred_buf_y[0] + ((PUStartRow * par->MaxCUSize + PUStartColumn) << par->Log2MinTUSize);
        Ipp16s *pSrc1 = pred_buf_y[1] + ((PUStartRow * par->MaxCUSize + PUStartColumn) << par->Log2MinTUSize);

        DstStride = pitch_rec_luma;
        PixType *pDst = y_rec + ((PUStartRow * pitch_rec_luma + PUStartColumn) << par->Log2MinTUSize);

        shift = 15 - BIT_DEPTH_LUMA;
        offset = 1 << (shift - 1);

        for (Ipp32s y = 0; y < Height; y++)
        {
            for (Ipp32s x = 0; x < Width; x++)
            {
                pDst[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_LUMA) - 1, ((pSrc0[x] + pSrc1[x] + offset) >> shift));
            }
            pSrc0 += SrcStride;
            pSrc1 += SrcStride;
            pDst += DstStride;
        }
    } else {
        SrcStride = par->MaxCUSize >> 1;
        Ipp16s *pSrcU0 = pred_buf_u[0] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        Ipp16s *pSrcU1 = pred_buf_u[1] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        Ipp16s *pSrcV0 = pred_buf_v[0] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        Ipp16s *pSrcV1 = pred_buf_v[1] + ((PUStartRow * SrcStride + PUStartColumn) << (par->Log2MinTUSize - 1));
        PixType *pDstU = u_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));
        PixType *pDstV = v_rec + ((PUStartRow * pitch_rec_chroma + PUStartColumn) << (par->Log2MinTUSize - 1));

        DstStride = pitch_rec_chroma;
        Width >>= 1;
        Height >>= 1;

        shift = 15 - BIT_DEPTH_CHROMA;
        offset = 1 << (shift - 1);

        for (Ipp32s y = 0; y < Height; y++)
        {
            for (Ipp32s x = 0; x < Width; x++)
            {
                pDstU[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_CHROMA) - 1, ((pSrcU0[x] + pSrcU1[x] + offset) >> shift));
                pDstV[x] = (PixType)Saturate(0, (1 << BIT_DEPTH_CHROMA) - 1,((pSrcV0[x] + pSrcV1[x] + offset) >> shift));
            }
            pSrcU0 += SrcStride;
            pSrcU1 += SrcStride;
            pDstU += DstStride;
            pSrcV0 += SrcStride;
            pSrcV1 += SrcStride;
            pDstV += DstStride;
        }
    }
}

void H265CU::InterPredCU(Ipp32s abs_part_idx, Ipp8u depth, Ipp8u is_luma)
{
    Ipp32s PartAddr;
    Ipp32s PartX, PartY, Width, Height;
    Ipp32u num_parts = ( par->NumPartInCU >> (depth<<1) );
    Ipp8u weighted_prediction = par->cslice->slice_type == P_SLICE ? par->cpps->weighted_pred_flag :
        par->cpps->weighted_bipred_flag;

    for (Ipp32s PartIdx = 0; PartIdx < getNumPartInter(abs_part_idx); PartIdx++)
    {
        GetPartOffsetAndSize(PartIdx, data[abs_part_idx].part_size,
            data[abs_part_idx].size, PartX, PartY, Width, Height);
        GetPartAddr(PartIdx, data[abs_part_idx].part_size, num_parts, PartAddr);

        PartAddr += abs_part_idx;

        if (CheckIdenticalMotion(PartAddr))
            PredInterUni(PartAddr, Width, Height, REF_PIC_LIST_0, PartIdx, false, is_luma);
        else
        {
            Ipp32s RefIdx[2] = {-1, -1};
            bool bipred = weighted_prediction ||
                (data[PartAddr].ref_idx[REF_PIC_LIST_0] >= 0 && data[PartAddr].ref_idx[REF_PIC_LIST_1] >= 0);

            for (Ipp32s RefList = 0; RefList < 2; RefList++)
            {
                EnumRefPicList RefPicList = (RefList ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
                RefIdx[RefList] = data[PartAddr].ref_idx[RefPicList];

                if (RefIdx[RefList] < 0)
                    continue;

                VM_ASSERT(RefIdx[RefList] < par->cslice->num_ref_idx[RefPicList]);

                // FIXME: Unneeded copy is done to temporary buffer, instead it is possible to do interpolation right to
                // pPredYUV in case only one reference is used
                PredInterUni(PartAddr, Width, Height, RefPicList, PartIdx, bipred, is_luma);
            }

            if (!weighted_prediction)
            {
                if (RefIdx[0] >= 0 && RefIdx[1] < 0)
                    CopySaturate(PartAddr, Width, Height, REF_PIC_LIST_0, is_luma);
                else if (RefIdx[0] < 0 && RefIdx[1] >= 0)
                    CopySaturate(PartAddr, Width, Height, REF_PIC_LIST_1, is_luma);
                else
                    AddAverage(PartAddr, Width, Height, is_luma);
            }
            else
            {
/*                Ipp32s w0[3], w1[3], o0[3], o1[3], logWD[3], round[3], bitDepth = g_BitDepth;

                for (Ipp32s plane = 0; plane < 3; plane++)
                {
                    w0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iWeight;
                    w1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iWeight;
                    o0[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iOffset;
                    o1[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_1][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].iOffset;

                    logWD[plane] = pCU->m_SliceHeader->m_weightPredTable[REF_PIC_LIST_0][RefIdx[0] >=0 ? RefIdx[0] : RefIdx[1]][plane].uiLog2WeightDenom;
                    logWD[plane] += 14 - bitDepth;
                    round[plane] = 0;

                    if (logWD[plane] >= 1)
                        round[plane] = 1 << (logWD[plane] - 1);
                    else
                        logWD[plane] = 0;
                }

                if (RefIdx[0] >= 0)
                    pPredYUV->CopyWeighted(&m_YUVPred[0], PartIdx, Width, Height, w0, o0, logWD, round);
                else if (RefIdx[1] >= 0)
                    pPredYUV->CopyWeighted(&m_YUVPred[1], PartIdx, Width, Height, w1, o1, logWD, round);
                else
                {
                    for (Ipp32s plane = 0; plane < 3; plane++)
                    {
                        logWD[plane] += 1;
                        round[plane] = (o0[plane] + o1[plane] + 1) << (logWD[plane] - 1);
                    }
                    pPredYUV->CopyWeightedBidi(&m_YUVPred[0], &m_YUVPred[1], PartIdx, Width, Height, w0, w1, logWD, round);
                }*/
            }
        }
    }
}


void H265CU::ME_Interpolate_old(H265MEInfo* me_info, H265MV* MV, PixType *in_pSrc, Ipp32s in_SrcPitch, Ipp16s *buf, Ipp32s buf_pitch) const
{
    Ipp32s width = me_info->width;
    Ipp32s height = me_info->height;

    Ipp16s *in_pDst = buf;
    Ipp32s in_DstPitch = buf_pitch;

    Ipp32s in_dx, in_dy;
    Ipp32s bitDepth, shift, offset, tap;

    Ipp32s refOffset = ctb_pelx + me_info->posx + (MV->mvx >> 2) + (ctb_pely + me_info->posy + (MV->mvy >> 2)) * in_SrcPitch;
    in_pSrc += refOffset;

    bitDepth = BIT_DEPTH_LUMA;
    tap = 8;
    in_dx = MV->mvx & 3;
    in_dy = MV->mvy & 3;

    shift = 6;
    offset = 32;

    //if (bi)
    //{
    //    shift = bitDepth - 8;
    //    offset = 0;
    //}

    if ((in_dx == 0) && (in_dy == 0))
    {
        shift = 0;

        //    if (bi)
        //        shift = 14 - bitDepth;

        CopyPU(in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, width, height, shift);
    }
    else if (in_dy == 0)
    {
        /*InterpolateHor_opt(
            TEXT_LUMA,
            in_pSrc,
            in_SrcPitch,
            in_pDst,
            in_DstPitch,
            in_dx,
            width,
            height,
            shift,
            offset);*/
        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dx, width, height, shift, (Ipp16s)offset);
    }
    else if (in_dx == 0)
    {
        /*InterpolateVert0_opt(
            TEXT_LUMA,
            in_pSrc,
            in_SrcPitch,
            in_pDst,
            in_DstPitch,
            in_dy,
            width,
            height,
            shift,
            offset);*/

        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, in_pSrc, in_SrcPitch, in_pDst, in_DstPitch, in_dy, width, height, shift, (Ipp16s)offset);
    }
    else
    {
        Ipp16s tmp[80 * 80];
        Ipp16s *tmpBuf = tmp + 80 * 8 + 8;

        shift = 20 - bitDepth;
        offset = 1 << (19 - bitDepth);

        //    if (bi)
        //    {
        //        shift = 6;
        //        offset = 0;
        //    }

        /*InterpolateHor_opt(
            TEXT_LUMA,
            in_pSrc - ((tap >> 1) - 1) * in_SrcPitch,
            in_SrcPitch,
            tmpBuf,
            80,
            in_dx,
            width,
            height + tap,
            bitDepth - 8,
            0);*/

        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, in_pSrc - ((tap >> 1) - 1) * in_SrcPitch, in_SrcPitch, tmpBuf, 80, in_dx, width, height + tap, bitDepth - 8, 0);

        /*InterpolateVert_opt(
            TEXT_LUMA,
            tmpBuf + ((tap >> 1) - 1) * 80,
            80,
            in_pDst,
            in_DstPitch,
            in_dy,
            width,
            height,
            shift,
            offset);*/

        Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, tmpBuf + ((tap >> 1) - 1) * 80, 80, in_pDst, in_DstPitch, in_dy, width, height, shift, (Ipp16s)offset);
    }

    return;
} // void H265CU::ME_Interpolate(...)


void H265CU::ME_Interpolate(H265MEInfo* me_info, H265MV* MV, PixType *src, Ipp32s srcPitch, Ipp8u *dst, Ipp32s dstPitch) const
{
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx = MV->mvx & 3;
    Ipp32s dy = MV->mvy & 3;
    Ipp32s bitDepth = BIT_DEPTH_LUMA;

    src += ctb_pelx + me_info->posx + (MV->mvx >> 2) + (ctb_pely + me_info->posy + (MV->mvy >> 2)) * srcPitch;

    if (dx == 0 && dy == 0)
    {
    }
    else if (dy == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32);
    }
    else if (dx == 0)
    {
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32);
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR, src - 3 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 8, bitDepth - 8, 0);

        Ipp32s shift  = 20 - bitDepth;
        Ipp16s offset = 1 << (shift - 1);
         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,  tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset);
    }

    return;
} // void H265CU::ME_Interpolate(...)

void WriteAverageToPic(
    const Ipp8u * pSrc0,
    Ipp32u        in_Src0Pitch,      // in samples
    const Ipp8u * pSrc1,
    Ipp32u        in_Src1Pitch,      // in samples
    Ipp8u * H265_RESTRICT pDst,
    Ipp32u        in_DstPitch,       // in samples
    Ipp32s        width,
    Ipp32s        height)
{
#ifdef __INTEL_COMPILER
    #pragma unroll(2)
    #pragma ivdep
#endif
    for (int j = 0; j < height; j++)
    {

#ifdef __INTEL_COMPILER
        #pragma ivdep
        #pragma vector always
#endif
        for (int i = 0; i < width; i++)
             pDst[i] = (((Ipp16u)pSrc0[i] + (Ipp16u)pSrc1[i] + 1) >> 1);

        pSrc0 += in_Src0Pitch;
        pSrc1 += in_Src1Pitch;
        pDst += in_DstPitch;
    }
}

void H265CU::ME_Interpolate_new_need_debug(H265MEInfo* me_info, H265MV* MV1, PixType *src1, Ipp32s srcPitch1, H265MV* MV2, PixType *src2, Ipp32s srcPitch2, Ipp8u *dst, Ipp32s dstPitch) const
{
    Ipp32s w = me_info->width;
    Ipp32s h = me_info->height;
    Ipp32s dx1 = MV1->mvx & 3;
    Ipp32s dy1 = MV1->mvy & 3;
    Ipp32s dx2 = MV2->mvx & 3;
    Ipp32s dy2 = MV2->mvy & 3;

    Ipp32s intPelL0 = (dx1 == 0 && dy1 == 0);
    Ipp32s intPelL1 = (dx2 == 0 && dy2 == 0);

    Ipp16s tmpL0[MAX_CU_SIZE * MAX_CU_SIZE];
    Ipp32s tmpL0Pitch = MAX_CU_SIZE;

    src1 += ctb_pelx + me_info->posx + (MV1->mvx >> 2) + (ctb_pely + me_info->posy + (MV1->mvy >> 2)) * srcPitch1;
    src2 += ctb_pelx + me_info->posx + (MV2->mvx >> 2) + (ctb_pely + me_info->posy + (MV2->mvy >> 2)) * srcPitch2;

    if (intPelL0)
    {
        if (intPelL1)
            WriteAverageToPic(src1, srcPitch1, src2, srcPitch2, dst, dstPitch, w, h);
    }
    else if (dy1 == 0)
    {
        if (intPelL1)
            Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>(INTERP_HOR,
                src1, srcPitch1, dst, dstPitch, dx1, w, h, 6, 0,
                AVERAGE_FROM_PIC, src2, srcPitch2);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR,
                src1, srcPitch1, tmpL0, tmpL0Pitch, dx1, w, h, 6, 0);
    }
    else if (dx1 == 0)
    {
        if (intPelL1)
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                src1, srcPitch1, dst, dstPitch, dy1, w, h, 6, 0,
                 AVERAGE_FROM_PIC, src2, srcPitch2);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                src1, srcPitch1, tmpL0, tmpL0Pitch, dy1, w, h, 6, 0);
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR,
            src1 - 3 * srcPitch1, srcPitch1, tmp, tmpPitch, dx1, w, h + 8, 0, 0);

        if (intPelL1)
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy1, w, h, 6, 0,
                 AVERAGE_FROM_PIC, src2, srcPitch2);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                tmp + 3 * tmpPitch, tmpPitch, tmpL0, tmpL0Pitch, dy1, w, h, 6, 0);
    }

    if (intPelL1)
    {
    }
    else if (dy2 == 0)
    {
        if (intPelL0)
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR,
                src2, srcPitch2, dst, dstPitch, dx2, w, h, 6, 0,
                 AVERAGE_FROM_PIC, src1, srcPitch1);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR,
                src2, srcPitch2, dst, dstPitch, dx2, w, h, 6, 0,
                 AVERAGE_FROM_BUF, tmpL0, tmpL0Pitch);
    }
    else if (dx2 == 0)
    {
        if (intPelL0)
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                src2, srcPitch2, dst, dstPitch, dy2, w, h, 6, 0,
                 AVERAGE_FROM_PIC, src1, srcPitch1);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                src2, srcPitch2, dst, dstPitch, dy2, w, h, 6, 0,
                 AVERAGE_FROM_BUF, tmpL0, tmpL0Pitch);
    }
    else
    {
        Ipp16s tmpBuf[80 * 80];
        Ipp16s *tmp = tmpBuf + 80 * 8 + 8;
        Ipp32s tmpPitch = 80;

         Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_HOR,
            src2 - 3 * srcPitch2, srcPitch2, tmp, tmpPitch, dx2, w, h + 8, 0, 0);

        if (intPelL0)
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy2, w, h, 6, 0,
                 AVERAGE_FROM_PIC, src1, srcPitch1);
        else
             Interpolate<UMC_HEVC_DECODER::TEXT_LUMA>( INTERP_VER,
                tmp + 3 * tmpPitch, tmpPitch, dst, dstPitch, dy2, w, h, 6, 0,
                 AVERAGE_FROM_BUF, tmpL0, tmpL0Pitch);
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
