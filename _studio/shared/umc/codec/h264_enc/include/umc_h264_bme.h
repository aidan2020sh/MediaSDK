// Copyright (c) 2004-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef UMC_H264_BME_H__
#define UMC_H264_BME_H__

#include "ippdefs.h"
#include "umc_h264_core_enc.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_tables.h"

#if _MSC_VER>1000
#pragma warning (disable:4244)
#endif
namespace UMC_H264_ENCODER {

#define LUMA_MB_MAX_COST 6
#define CHROMA_COEFF_MAX_COST 7
#define LUMA_8X8_MAX_COST 4
#define LUMA_COEFF_8X8_MAX_COST 4
#define LUMA_COEFF_MB_8X8_MAX_COST 6

extern const Ipp8u QPtoChromaQP[52];

inline Ipp32s getLumaQP(Ipp32s QPy, Ipp32s bit_depth_luma)
{
    Ipp32s QP = (QPy + 6*(bit_depth_luma - 8));
    return (QP);
}

inline Ipp32s getLumaQP51(Ipp32s QPy, Ipp32s bit_depth_luma)
{
    Ipp32s QP = (QPy + 6*(bit_depth_luma - 8));
    return ((QP>51)? 51: QP);
}

inline Ipp32s getChromaQP(Ipp32s QPy, Ipp32s qPOffset, Ipp32s bit_depth_chroma)
{
    Ipp32s QPc;
    Ipp32s qPI = QPy + qPOffset;
    Ipp32s QpBdOffsetC = 6*(bit_depth_chroma - 8);
    qPI = (qPI < -QpBdOffsetC) ? -QpBdOffsetC : (qPI > 51) ? 51 : qPI;
    QPc = (qPI >= 0)? QPtoChromaQP[qPI]: qPI;
    return (QPc + QpBdOffsetC);
}

#define MVBITS(v) (BitsForMV[(v)+BITSFORMV_OFFSET])

inline Ipp32u BITS_COST(Ipp32s bits, Ipp16s *pRDQM)
{
    return pRDQM[bits];
}

#define MVConstraintDelta0( x, y ) pRDQM[pBitsX[(x)] + pBitsY[(y)]]
#define MVConstraintDeltaPr( x, y ) pRDQM[pBitsXpr[(x)] + pBitsYpr[(y)]]
#define MVConstraintDelta( dst, x, y ) { \
    dst = MVConstraintDelta0( x, y ); \
    meInfo->bestPredMV = meInfo->predictedMV; \
    if (meInfo->useScPredMV) { \
        Ipp32s prSAD = MVConstraintDeltaPr( x, y ); \
        if (prSAD < dst || meInfo->useScPredMVAlways) { \
            dst = prSAD; \
            meInfo->bestPredMV = meInfo->scPredMV; \
        } \
    } \
}

#ifdef WIN64 //cl8 Itanium (IA64) optimization bug
inline Ipp32u MVConstraint(Ipp16s x, Ipp16s y, Ipp16s *pRDQM)
{
    return (pRDQM[MVBITS(x) + MVBITS(y)]);
}
#else
inline Ipp32u MVConstraint(Ipp32s x, Ipp32s y, Ipp16s *pRDQM)
{
    return (pRDQM[MVBITS(x) + MVBITS(y)]);
}
#endif

inline Ipp32s RefConstraint(Ipp32s ref_idx, Ipp32s active_refs, Ipp16s *pRDQM)
{
    if (active_refs == 1)
        return pRDQM[1];

    Ipp32s cost;
    if (active_refs > 2)
        cost = pRDQM[MVBITS(ref_idx)];
    else
        cost = pRDQM[ref_idx ?  2 : 1];
    return cost;
}

inline void Diff4x4(Ipp8u* pPred, Ipp8u* pSrc, Ipp32s pitchPixels, Ipp16s* pDiff)
{
    ippiSub4x4_8u16s_C1R(pSrc, pitchPixels, pPred, 16, pDiff, 8);
}

inline void Diff4x4Residual(const Ipp16s* in_pSrc1,
                       Ipp32s in_Src1Pitch,
                       const Ipp16s* in_pSrc2,
                       Ipp32s in_Src2Pitch,
                       Ipp16s* in_pDst,
                       Ipp32s in_DstPitch)
{
    Ipp8u rows, cols;

    for(rows = 0; rows <4; rows++)
    {
        for(cols = 0; cols <4; cols++)
        {
            in_pDst[in_DstPitch*rows + cols] = in_pSrc2[in_Src2Pitch*rows + cols] - in_pSrc1[in_Src1Pitch*rows + cols];
        }
    }
}


inline void Diff4x4_ResPred(const Ipp16s* pRes,
                            const Ipp8u* pPred,
                            Ipp16s* pDst)
{
  Ipp8u rows, cols;
  for (rows = 0; rows < 4; rows++) {
    for (cols = 0; cols < 4; cols++) {
      pDst[4*rows + cols] = pRes[16*rows + cols] - pPred[16*rows + cols];
    }
  }
}

inline void Diff8x8_ResPred(const Ipp16s* pRes,
                            const Ipp8u* pPred,
                            Ipp16s* pDst)
{
  Ipp8u rows, cols;
  for (rows = 0; rows < 8; rows++) {
    for (cols = 0; cols < 8; cols++) {
      pDst[8*rows + cols] = pRes[16*rows + cols] - pPred[16*rows + cols];
    }
  }
}


inline void Diff8x8Residual(const Ipp16s* in_pSrc1,
                            Ipp32s in_Src1Pitch,
                            const Ipp16s* in_pSrc2,
                            Ipp32s in_Src2Pitch,
                            Ipp16s* in_pDst,
                            Ipp32s in_DstPitch)
{
    Ipp8u rows, cols;

    for(rows = 0; rows < 8; rows++)
    {
        for(cols = 0; cols < 8; cols++)
        {
            in_pDst[in_DstPitch*rows + cols] = in_pSrc2[in_Src2Pitch*rows + cols] - in_pSrc1[in_Src1Pitch*rows + cols];
        }
    }
}

#if defined BITDEPTH_9_12
inline void Diff4x4(Ipp16u* pPred, Ipp16u* pSrc, Ipp32s pitchPixels, Ipp16s* pDiff)
{
    ippiSub4x4_16u16s_C1R(pSrc, pitchPixels*sizeof(Ipp16u), pPred, 16*sizeof(Ipp16u), pDiff, 8);
}
#endif // BITDEPTH_9_12

inline void Diff8x8(Ipp8u* pPred, Ipp8u* pSrc, Ipp32s pitchPixels, Ipp16s* pDiff)
{
    ippiSub8x8_8u16s_C1R(pSrc, pitchPixels, pPred, 16, pDiff, 16);
}

#if defined BITDEPTH_9_12
inline void Diff8x8(Ipp16u* pPred, Ipp16u* pSrc, Ipp32s pitchPixels, Ipp16s* pDiff)
{
    ippiSub8x8_16u16s_C1R(pSrc, pitchPixels*sizeof(Ipp16u), pPred, sizeof(Ipp16u)*16, pDiff, 16);
}
#endif // BITDEPTH_9_12

inline void Copy4x4(const Ipp8u* pSrc, Ipp32s srcPitchPixels, Ipp8u* pDst, Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 4; i ++) {
        pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3];
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}

#if defined BITDEPTH_9_12
inline void Copy4x4(const Ipp16u* pSrc, Ipp32s srcPitchPixels, Ipp16u* pDst, Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 4; i ++) {
        pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3];
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif // BITDEPTH_9_12
inline void Copy16x16(const Ipp8u* pSrc, const Ipp32s srcPitchPixels, Ipp8u* pDst, const Ipp32s dstPitchPixels)
{
    ippiCopy16x16_8u_C1R(pSrc, srcPitchPixels, pDst, dstPitchPixels);
}

#if defined BITDEPTH_9_12
inline void Copy16x16(const Ipp16u* pSrc, const Ipp32s srcPitchPixels, Ipp16u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 16; i ++) {
        MFX_INTERNAL_CPY( pDst, pSrc, 16*sizeof( Ipp16u ));
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif

inline void Copy16x8(const Ipp8u* pSrc, const Ipp32s srcPitchPixels, Ipp8u* pDst, const Ipp32s dstPitchPixels)
{
    ippiCopy8x8_8u_C1R(pSrc, srcPitchPixels, pDst, dstPitchPixels);
    ippiCopy8x8_8u_C1R(pSrc+8, srcPitchPixels, pDst+8, dstPitchPixels);
}

#if defined BITDEPTH_9_12
inline void Copy16x8(const Ipp16u* pSrc, const Ipp32s srcPitchPixels, Ipp16u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 8; i ++) {
        MFX_INTERNAL_CPY( pDst, pSrc, 16*sizeof( Ipp16u ));
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif

inline void Copy8x16(const Ipp8u* pSrc, const Ipp32s srcPitchPixels, Ipp8u* pDst, const Ipp32s dstPitchPixels)
{
    ippiCopy8x8_8u_C1R(pSrc, srcPitchPixels, pDst, dstPitchPixels);
    ippiCopy8x8_8u_C1R(pSrc+8*srcPitchPixels, srcPitchPixels, pDst+8*dstPitchPixels, dstPitchPixels);
}

#if defined BITDEPTH_9_12
inline void Copy8x16(const Ipp16u* pSrc, const Ipp32s srcPitchPixels, Ipp16u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 16; i ++) {
        MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof( Ipp16u ));
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif

inline void Copy8x8(const Ipp8u* pSrc, Ipp32s srcPitchPixels, Ipp8u* pDst, Ipp32s dstPitchPixels)
{
    ippiCopy8x8_8u_C1R(pSrc, srcPitchPixels, pDst, dstPitchPixels);
}

#if defined BITDEPTH_9_12
inline void Copy8x8(const Ipp16u* pSrc, Ipp32s srcPitchPixels, Ipp16u* pDst, Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 8; i ++) {
        pDst[0] = pSrc[0]; pDst[1] = pSrc[1]; pDst[2] = pSrc[2]; pDst[3] = pSrc[3]; pDst[4] = pSrc[4]; pDst[5] = pSrc[5]; pDst[6] = pSrc[6]; pDst[7] = pSrc[7];
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif // BITDEPTH_9_12

inline void Copy8x4(const Ipp8u* pSrc, const Ipp32s srcPitchPixels, Ipp8u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;
    for (i = 0; i < 4; i ++) {
        pDst[0] = pSrc[0];
        pDst[1] = pSrc[1];
        pDst[2] = pSrc[2];
        pDst[3] = pSrc[3];
        pDst[4] = pSrc[4];
        pDst[5] = pSrc[5];
        pDst[6] = pSrc[6];
        pDst[7] = pSrc[7];
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}

#if defined BITDEPTH_9_12
inline void Copy8x4(const Ipp16u* pSrc, const Ipp32s srcPitchPixels, Ipp16u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 4; i ++) {
        MFX_INTERNAL_CPY( pDst, pSrc, 8*sizeof( Ipp16u ));
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif

inline void Copy4x8(const Ipp8u* pSrc, const Ipp32s srcPitchPixels, Ipp8u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;
    for (i = 0; i < 8; i ++) {
        pDst[0] = pSrc[0];
        pDst[1] = pSrc[1];
        pDst[2] = pSrc[2];
        pDst[3] = pSrc[3];
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}

#if defined BITDEPTH_9_12
inline void Copy4x8(const Ipp16u* pSrc, const Ipp32s srcPitchPixels, Ipp16u* pDst, const Ipp32s dstPitchPixels)
{
    Ipp32s i;

    for (i = 0; i < 8; i ++) {
        MFX_INTERNAL_CPY( pDst, pSrc, 4*sizeof( Ipp16u ));
        pSrc += srcPitchPixels;
        pDst += dstPitchPixels;
    }
}
#endif



inline Ipp32s SubpelMVAdjust(const H264MotionVector *pMV, Ipp32s pitchPixels, Ipp32s& iXType, Ipp32s& iYType)
{
    Ipp32s iXOffset;
    Ipp32s iYOffset;
    Ipp32s iPelOffset = 0;

    iXType = 0;
    iYType = 0;
    if (pMV->mvx) {
        iXOffset = pMV->mvx >> 2;
        iXType = pMV->mvx & 3;
        iPelOffset += iXOffset;
    }
    if (pMV->mvy) {
        iYOffset = pMV->mvy >> 2;
        iYType = pMV->mvy & 3;
        iPelOffset += iYOffset*pitchPixels;
    }
    return iPelOffset;
}

#ifdef USE_NV12
inline Ipp32s SubpelChromaMVAdjust(const H264MotionVector *pMV, Ipp32s pitchPixels, Ipp32s& iXType, Ipp32s& iYType, Ipp32s chroma_format_idc)
{
    Ipp32s iXOffset = 0;
    Ipp32s iYOffset = 0;
    Ipp32s iPelOffset = 0;

    iXType = 0;
    iYType = 0;
    if (pMV->mvx) {
        switch( chroma_format_idc ) {
            case 1:
            case 2:
                iXOffset = pMV->mvx >> 3;
                iXType = pMV->mvx&7;
                break;
            case 3:
                iXOffset = pMV->mvx >>2;
                iXType = (pMV->mvx & 3)<<1;
                break;
        }
        iPelOffset += iXOffset*2;
    }
    if (pMV->mvy) {
        switch( chroma_format_idc ) {
            case 1:
                iYOffset = pMV->mvy >> 3;
                iYType = pMV->mvy&7;
                break;
            case 2:
            case 3:
                iYOffset = pMV->mvy >> 2;
                iYType = (pMV->mvy & 3)<<1;
                break;
        }
        iPelOffset += iYOffset*pitchPixels;
    }
    return iPelOffset;
}
#else
inline Ipp32s SubpelChromaMVAdjust(const H264MotionVector *pMV, Ipp32s pitchPixels, Ipp32s& iXType, Ipp32s& iYType, Ipp32s chroma_format_idc)
{
    Ipp32s iXOffset = 0;
    Ipp32s iYOffset = 0;
    Ipp32s iPelOffset = 0;

    iXType = 0;
    iYType = 0;
    if (pMV->mvx) {
        switch( chroma_format_idc ) {
            case 1:
            case 2:
                iXOffset = pMV->mvx >> 3;
                iXType = pMV->mvx&7;
                break;
            case 3:
                iXOffset = pMV->mvx >>2;
                iXType = (pMV->mvx & 3)<<1;
                break;
        }
        iPelOffset += iXOffset;
    }
    if (pMV->mvy) {
        switch( chroma_format_idc ) {
            case 1:
                iYOffset = pMV->mvy >> 3;
                iYType = pMV->mvy&7;
                break;
            case 2:
            case 3:
                iYOffset = pMV->mvy >> 2;
                iYType = (pMV->mvy & 3)<<1;
                break;
        }
        iPelOffset += iYOffset*pitchPixels;
    }
    return iPelOffset;
}
#endif

inline
Ipp32s CalculateCoeffsCost_8u16s(
    Ipp16s *coeffs,
    Ipp32s count,
    const Ipp32s *scan)
{
    Ipp32s k, cost = 0;
    const Ipp8u* coeff_cost = &coeff_importance[0];
    if( count == 64 ){
        coeff_cost = &coeff_importance8x8[0];
    }

    for (k = 0; k < count; k++) {
        Ipp32s run = 0;
        for (; k < count && coeffs[scan[k]] == 0; k ++, run ++);
        if (k == count)
            break;
        if (ABS(coeffs[scan[k]]) > 1)
            return 9;
        cost += coeff_cost[run];
    }
    return cost;
}

inline
Ipp32s CalculateCoeffsCost_16u32s(
    Ipp32s *coeffs,
    Ipp32s count,
    const Ipp32s *scan)
{
    Ipp32s k, cost = 0;
    const Ipp8u* coeff_cost = &coeff_importance[0];
    if( count == 64 ){
        coeff_cost = &coeff_importance8x8[0];
    }

    for (k = 0; k < count; k++) {
        Ipp32s run = 0;
        for (; k < count && coeffs[scan[k]] == 0; k ++, run ++);
        if (k == count)
            break;
        if (ABS(coeffs[scan[k]]) > 1)
            return 9;
        cost += coeff_cost[run];
    }
    return cost;
}

Ipp32u SATD_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height);
Ipp32u SATD_16u_C1R(const Ipp16u *pSrc1, Ipp32s src1Step, const Ipp16u *pSrc2, Ipp32s src2Step, Ipp32s width, Ipp32s height);
Ipp32u SAT8x8D(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SAT8x8D(const Ipp16u *pSrc1, Ipp32s src1Step, const Ipp16u *pSrc2, Ipp32s src2Step);

Ipp32u SATD4x4_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD4x8_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD8x4_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD8x8_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD8x16_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD16x8_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);
Ipp32u SATD16x16_8u_C1R(const Ipp8u *pSrc1, Ipp32s src1Step, const Ipp8u *pSrc2, Ipp32s src2Step);


Ipp32u SATD16x16_16_ResPred(const Ipp16s *pResidualDiff, const Ipp8u *pPrediction);
Ipp32u SATD8x8_16_ResPred(const Ipp16s *pResidualDiff, const Ipp8u *pPrediction);
Ipp32u SATD4x4_16_ResPred(const Ipp16s *pResidualDiff, const Ipp8u *pPrediction);
Ipp32u SATD8x16_16_ResPred(const Ipp16s *pResidualDiff, const Ipp8u *pPrediction);
Ipp32u SATD16x8_16_ResPred(const Ipp16s *pResidualDiff, const Ipp8u *pPrediction);

Ipp32u Hadamard8x8_16_AbsSum(Ipp16s *diff);
Ipp32u Hadamard4x4_16_AbsSum(Ipp16s *diff);
Ipp32u Hadamard4x4_AbsSum(Ipp16s diffBuff[4][4]);
Ipp32u Hadamard4x2_AbsSum(Ipp16s diffBuff[4][4]);
Ipp32u Hadamard2x4_AbsSum(Ipp16s diffBuff[4][4]);
Ipp32u Hadamard2x2_AbsSum(Ipp16s diffBuff[4][4]);

void __STDCALL SAD8x16_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD8x4_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD4x8_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD8x4_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD4x8_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD4x2_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD2x4_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);
void __STDCALL SAD2x2_8u32s_C1R(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* pDst, Ipp32s mc);


inline Ipp32s SATD16x16(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    return SATD16x16_8u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD16x16(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    return SATD_16u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, 16, 16);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD16x8(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    return SATD16x8_8u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD16x8(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    return SATD_16u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, 16, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x16(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    return SATD8x16_8u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x16(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    return SATD_16u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, 8, 16);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x8(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD8x8_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x8(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD8x4(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD8x4_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD8x4(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 8, 4);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD4x8(const Ipp8u* pSrc0, Ipp32s srcStepBytes0, const Ipp8u* pSrc1, Ipp32s  srcStepBytes1)
{
    return SATD4x8_8u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD4x8(const Ipp16u* pSrc0, Ipp32s srcStepBytes0, const Ipp16u* pSrc1, Ipp32s srcStepBytes1)
{
    return SATD_16u_C1R(pSrc0, srcStepBytes0, pSrc1, srcStepBytes1, 4, 8);
}
#endif // BITDEPTH_9_12

inline Ipp32s SATD4x4(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    return SATD4x4_8u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1);
}

#if defined BITDEPTH_9_12
inline Ipp32s SATD4x4(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    return SATD_16u_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, 4, 4);
}
#endif // BITDEPTH_9_12


inline Ipp32s SAD16x16(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD;
    ippiSAD16x16_8u32s(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    return SAD;
}


inline Ipp32s SAD16x16_16_ResPred(const Ipp16s *pResDiff, const Ipp8u *pPred)
{
  Ipp32s i, j, sad = 0;
  for (i = 0; i < 16; i++)
    for (j = 0; j < 16; j++)
      sad += ABS(pResDiff[i*16+j] - pPred[i*16+j]);
  return sad;
}

inline Ipp32s SAD8x8_16_ResPred(const Ipp16s *pResDiff, const Ipp8u *pPred)
{
  Ipp32s i, j, sad = 0;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 8; j++)
      sad += ABS(pResDiff[i*16+j] - pPred[i*16+j]);
  return sad;
}


inline Ipp32s SAD8x16_16_ResPred(const Ipp16s *pResDiff, const Ipp8u *pPred)
{
  Ipp32s i, j, sad = 0;
  for (i = 0; i < 16; i++)
    for (j = 0; j < 8; j++)
      sad += ABS(pResDiff[i*16+j] - pPred[i*16+j]);
  return sad;
}

inline Ipp32s SAD16x8_16_ResPred(const Ipp16s *pResDiff, const Ipp8u *pPred)
{
  Ipp32s i, j, sad = 0;
  for (i = 0; i < 8; i++)
    for (j = 0; j < 16; j++)
      sad += ABS(pResDiff[i*16+j] - pPred[i*16+j]);
  return sad;
}

inline Ipp32s SAD4x4_16_ResPred(const Ipp16s *pResDiff, const Ipp8u *pPred)
{
  Ipp32s i, j, sad = 0;
  for (i = 0; i < 4; i++)
    for (j = 0; j < 4; j++)
      sad += ABS(pResDiff[i*16+j] - pPred[i*16+j]);
  return sad;
}


#if defined BITDEPTH_9_12
inline Ipp32s SAD16x16(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD;
    ippiSAD16x16_16u32s_C1R(pSource0, pitchPixels0*sizeof(Ipp16u), pSource1, pitchPixels1*sizeof(Ipp16u), &SAD, 0);
    return SAD;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD16x8(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD;
    ippiSAD16x8_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    return SAD;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD16x8(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD1, SAD2;
    ippiSAD8x8_16u32s_C1R(pSource0, pitchPixels0*sizeof(Ipp16u), pSource1, pitchPixels1*sizeof(Ipp16u), &SAD1, 0);
    ippiSAD8x8_16u32s_C1R(pSource0 + 8, pitchPixels0*sizeof(Ipp16u), pSource1 + 8, pitchPixels1*sizeof(Ipp16u), &SAD2, 0);
    return SAD1 + SAD2;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD8x16(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    //Ipp32s SAD1, SAD2;
    //ippiSAD8x8_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD1, 0);
    //ippiSAD8x8_8u32s_C1R(pSource0 + 8 * pitchPixels0, pitchPixels0, pSource1 + 8 * pitchPixels1, pitchPixels1, &SAD2, 0);
    //return SAD1 + SAD2;
    Ipp32s SAD;
    //SAD8x16_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    ippiSAD8x16_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    return SAD;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD8x16(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD1, SAD2;
    ippiSAD8x8_16u32s_C1R(pSource0, pitchPixels0*sizeof(Ipp16u), pSource1, pitchPixels1*sizeof(Ipp16u), &SAD1, 0);
    ippiSAD8x8_16u32s_C1R(pSource0 + 8 * pitchPixels0, pitchPixels0*sizeof(Ipp16u), pSource1 + 8 * pitchPixels1, pitchPixels1*sizeof(Ipp16u), &SAD2, 0);
    return SAD1 + SAD2;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD8x8(const Ipp8u* pSrc0, Ipp32s pitchPixels0, const Ipp8u* pSrc1, Ipp32s pitchPixels1)
{
    Ipp32s s;
    ippiSAD8x8_8u32s_C1R(pSrc0, pitchPixels0, pSrc1, pitchPixels1, &s, 0);
    return s;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD8x8(const Ipp16u* pSrc0, Ipp32s pitchPixels0, const Ipp16u* pSrc1, Ipp32s pitchPixels1)
{
    Ipp32s s;
    ippiSAD8x8_16u32s_C1R(pSrc0, pitchPixels0*sizeof(Ipp16u), pSrc1, pitchPixels1*sizeof(Ipp16u), &s, 0);
    return s;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD8x4(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    //Ipp32s SAD1, SAD2;
    //ippiSAD4x4_8u32s(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD1, 0);
    //ippiSAD4x4_8u32s(pSource0 + 4, pitchPixels0, pSource1 + 4, pitchPixels1, &SAD2, 0);
    //return SAD1 + SAD2;
    Ipp32s SAD;
    SAD8x4_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    return SAD;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD8x4(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD1, SAD2;
    ippiSAD4x4_16u32s_C1R(pSource0, pitchPixels0*sizeof(Ipp16u), pSource1, pitchPixels1*sizeof(Ipp16u), &SAD1, 0);
    ippiSAD4x4_16u32s_C1R(pSource0 + 4, pitchPixels0*sizeof(Ipp16u), pSource1 + 4, pitchPixels1*sizeof(Ipp16u), &SAD2, 0);
    return SAD1 + SAD2;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD4x8(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    //Ipp32s SAD1, SAD2;
    //ippiSAD4x4_8u32s(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD1, 0);
    //ippiSAD4x4_8u32s(pSource0 + 4 * pitchPixels0, pitchPixels0, pSource1 + 4 * pitchPixels1, pitchPixels1, &SAD2, 0);
    //return SAD1 + SAD2;
    Ipp32s SAD;
    SAD4x8_8u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD, 0);
    return SAD;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD4x8(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s SAD1, SAD2;
    ippiSAD4x4_16u32s_C1R(pSource0, pitchPixels0, pSource1, pitchPixels1, &SAD1, 0);
    ippiSAD4x4_16u32s_C1R(pSource0 + 4 * pitchPixels0, pitchPixels0*sizeof(Ipp16u), pSource1 + 4 * pitchPixels1, pitchPixels1*sizeof(Ipp16u), &SAD2, 0);
    return SAD1 + SAD2;
}
#endif // BITDEPTH_9_12

inline Ipp32s SAD4x4(const Ipp8u *pSource0, Ipp32s pitchPixels0, const Ipp8u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s s;
    ippiSAD4x4_8u32s(pSource0, pitchPixels0, pSource1, pitchPixels1, &s, 0);
    return s;
}

#if defined BITDEPTH_9_12
inline Ipp32s SAD4x4(const Ipp16u *pSource0, Ipp32s pitchPixels0, const Ipp16u *pSource1, Ipp32s pitchPixels1)
{
    Ipp32s s;
    ippiSAD4x4_16u32s_C1R(pSource0, pitchPixels0*sizeof(Ipp16u), pSource1, pitchPixels1*sizeof(Ipp16u), &s, 0);
    return s;
}
#endif // BITDEPTH_9_12


inline Ipp32s SSD4x4( const Ipp8u* p1, Ipp32s pitch1, const Ipp8u* p2, Ipp32s pitch2 )
{
    Ipp32s ssd;
    ippiSSD4x4_8u32s_C1R( p1, pitch1, p2, pitch2, &ssd, 0 );
    return ssd;
}

inline Ipp32s SSD8x8( const Ipp8u* p1, Ipp32s pitch1, const Ipp8u* p2, Ipp32s pitch2 )
{
    Ipp32s ssd;
    ippiSSD8x8_8u32s_C1R( p1, pitch1, p2, pitch2, &ssd, 0 );
    return ssd;
}

inline Ipp32s SSD16x16( const Ipp8u* p1, Ipp32s pitch1, const Ipp8u* p2, Ipp32s pitch2 )
{
    Ipp32s ssd;
    ippiSqrDiff16x16_8u32s( p1, pitch1, p2, pitch2, 0, &ssd );
    return ssd;
}

#if defined BITDEPTH_9_12
inline Ipp32s SSD4x4( const Ipp16u* p1, Ipp32s pitch1, const Ipp16u* p2, Ipp32s pitch2 )
{
    p1;
    pitch1;
    p2;
    pitch2;
    //TODO fix me
    return 0;
}

inline Ipp32s SSD8x8( const Ipp16u* p1, Ipp32s pitch1, const Ipp16u* p2, Ipp32s pitch2 )
{
    p1;
    pitch1;
    p2;
    pitch2;
    //TODO fix me
    return 0;
}

inline Ipp32s SSD16x16( const Ipp16u* p1, Ipp32s pitch1, const Ipp16u* p2, Ipp32s pitch2 )
{
    p1;
    pitch1;
    p2;
    pitch2;
    //TODO fix me
    return 0;
}
#endif


#ifdef TABLE_FUNC
typedef IppStatus (__STDCALL *SADfunc8u)(const Ipp8u* pSrcCur, Ipp32s srcCurStep, const Ipp8u* pSrcRef, Ipp32s srcRefStep, Ipp32s* pDst, Ipp32s mcType);
extern SADfunc8u SAD_8u[];
typedef Ipp32u (*SATDfunc8u)(const Ipp8u* pSrcCur, Ipp32s srcCurStep, const Ipp8u* pSrcRef, Ipp32s srcRefStep);
extern SATDfunc8u SATD_8u[];
typedef Ipp32u (*HadamardFunc_16s)(Ipp16s diffBuff[4][4]);
extern HadamardFunc_16s Hadamard_16s[];
#endif


void InterpolateLuma_8u16s(const Ipp8u *pY, Ipp32s pitchPixels, Ipp8u *interpBuff, Ipp32s bStep, Ipp32s dx, Ipp32s dy, IppiSize block, Ipp32s bitDepth, Ipp32s planeSize, const Ipp8u **pI = NULL, Ipp32s *sI = NULL);
void InterpolateLuma_16u32s(const Ipp16u *pY, Ipp32s pitchPixels, Ipp16u *interpBuff, Ipp32s bStep, Ipp32s dx, Ipp32s dy, IppiSize block, Ipp32s bitDepth, Ipp32s planeSize, const Ipp16u **pI = NULL, Ipp32s *sI = NULL);

} //namespace UMC_H264_ENCODER

#endif // UMC_H264_BME_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
