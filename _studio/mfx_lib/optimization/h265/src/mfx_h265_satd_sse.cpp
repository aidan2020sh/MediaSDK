/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_AUTO) || \
    defined(MFX_MAKENAME_SSE4) && defined(MFX_TARGET_OPTIMIZATION_SSE4)

namespace MFX_HEVC_PP
{

ALIGN_DECL(16) static const Ipp8s shufTab4[16] = {  0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15 };

/* h265_SATD_4x4_8u() - 4x4 SATD adapted from IPP source code
 *
 * multiply-free implementation of sum(abs(T*D*T))
 *   D = difference matrix (cur - ref)
 *   T = transform matrix (values of +/-1, see IPP docs)
 *
 * implemented as sum(abs(T*transpose(T*D)))
 * final matrix is transposed, but doesn't matter since we just sum all the elements
 */
Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_4x4_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    __m128i xmm0, xmm1, xmm2, xmm3;

    /* difference matrix (4x4) = cur - ref */
    xmm0 = _mm_cvtsi32_si128(*(int*)(pSrcCur + 0*srcCurStep));
    xmm0 = _mm_insert_epi32(xmm0, *(int*)(pSrcCur + 1*srcCurStep), 0x01);
    xmm2 = _mm_cvtsi32_si128(*(int*)(pSrcCur + 2*srcCurStep));
    xmm2 = _mm_insert_epi32(xmm2, *(int*)(pSrcCur + 3*srcCurStep), 0x01);
    xmm0 = _mm_cvtepu8_epi16(xmm0);
    xmm2 = _mm_cvtepu8_epi16(xmm2);

    xmm1 = _mm_cvtsi32_si128(*(int*)(pSrcRef + 0*srcRefStep));
    xmm1 = _mm_insert_epi32(xmm1, *(int*)(pSrcRef + 1*srcRefStep), 0x01);
    xmm3 = _mm_cvtsi32_si128(*(int*)(pSrcRef + 2*srcRefStep));
    xmm3 = _mm_insert_epi32(xmm3, *(int*)(pSrcRef + 3*srcRefStep), 0x01);
    xmm1 = _mm_cvtepu8_epi16(xmm1);
    xmm3 = _mm_cvtepu8_epi16(xmm3);

    xmm0 = _mm_sub_epi16(xmm0, xmm1);       /* diff - rows 0,1: [0 1 2 3 0 1 2 3] */
    xmm2 = _mm_sub_epi16(xmm2, xmm3);       /* diff - rows 2,3: [0 1 2 3 0 1 2 3] */

    /* first stage */
    xmm1 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm2);       /* [row0+row2 row1+row3] */
    xmm1 = _mm_sub_epi16(xmm1, xmm2);       /* [row0-row2 row1-row3] */
    
    xmm2 = xmm0;
    xmm2 = _mm_unpacklo_epi64(xmm2, xmm1);  /* row0+row2 row0-row2 */
    xmm0 = _mm_unpackhi_epi64(xmm0, xmm1);  /* row1+row3 row1-row3 */

    xmm3 = xmm2;
    xmm2 = _mm_add_epi16(xmm2, xmm0);       /* row0: 0+1+2+3  row1: 0+1-2-3 */
    xmm3 = _mm_sub_epi16(xmm3, xmm0);       /* row2: 0-1+2-3  row3: 0-1-2+3 */

    /* transpose */
    xmm2 = _mm_shuffle_epi8(xmm2, *(__m128i*)shufTab4);
    xmm3 = _mm_shuffle_epi8(xmm3, *(__m128i*)shufTab4);

    xmm0 = xmm2;
    xmm0 = _mm_unpacklo_epi32(xmm0, xmm3);  /* rows 0-3 [0 0 0 0 1 1 1 1] */
    xmm2 = _mm_unpackhi_epi32(xmm2, xmm3);  /* rows 0-3 [2 2 2 2 3 3 3 3] */

    /* second stage */
    xmm1 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm2);       /* [row0+row2 row1+row3] */
    xmm1 = _mm_sub_epi16(xmm1, xmm2);       /* [row0-row2 row1-row3] */
    
    xmm2 = xmm0;
    xmm2 = _mm_unpacklo_epi64(xmm2, xmm1);  /* row0+row2 row0-row2 */
    xmm0 = _mm_unpackhi_epi64(xmm0, xmm1);  /* row1+row3 row1-row3 */

    xmm3 = xmm2;
    xmm2 = _mm_add_epi16(xmm2, xmm0);       /* row0: 0+1+2+3  row1: 0+1-2-3 */
    xmm3 = _mm_sub_epi16(xmm3, xmm0);       /* row2: 0-1+2-3  row3: 0-1-2+3 */

    /* sum abs values - avoid phadd (faster to shuffle) */
    xmm2 = _mm_abs_epi16(xmm2);
    xmm3 = _mm_abs_epi16(xmm3);
    xmm2 = _mm_add_epi16(xmm2, xmm3);
    xmm3 = _mm_shuffle_epi32(xmm2, 0x0E);
    xmm2 = _mm_add_epi16(xmm2, xmm3);
    xmm3 = _mm_shuffle_epi32(xmm2, 0x01);
    xmm2 = _mm_add_epi16(xmm2, xmm3);
    xmm3 = _mm_shufflelo_epi16(xmm2, 0x01);
    xmm2 = _mm_add_epi16(xmm2, xmm3);
    xmm2 = _mm_cvtepu16_epi32(xmm2);

    return _mm_cvtsi128_si32(xmm2);
}

/* call twice for SSE version (no speed advantage with 128-bit registers) */
void H265_FASTCALL MAKE_NAME(h265_SATD_4x4_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_4x4_8u)(pSrcCur + 4, srcCurStep, pSrcRef + 4, srcRefStep);
}

/* h265_SATD_8x8_8u() - 8x8 SATD adapted from IPP source code
 *
 * multiply-free implementation of sum(abs(T*D*T))
 *   D = difference matrix (cur - ref)
 *   T = transform matrix (values of +/-1, see IPP docs)
 *
 * implemented as sum(abs(T*transpose(T*D)))
 * final matrix is transposed, but doesn't matter since we just sum all the elements
 */
Ipp32s H265_FASTCALL MAKE_NAME(h265_SATD_8x8_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep)
{
    Ipp8u *pCT, *pRT;
    int stepC4, stepR4;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;
    __m128i xmm_s0, xmm_s1, xmm_s2, xmm_s3;

    pCT = (Ipp8u*)pSrcCur;
    pRT = (Ipp8u*)pSrcRef;
    stepC4 = 4*srcCurStep;
    stepR4 = 4*srcRefStep;

    /* calculate differences (8x8) then combine top and bottom halves into two 4x8 matrices (top + bottom, top - bottom) */
    xmm0 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
    xmm0 = _mm_sub_epi16(xmm0, xmm5);
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
    xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
    xmm5 = _mm_sub_epi16(xmm5, xmm4);
    xmm_s0 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm5);
    xmm_s0 = _mm_sub_epi16(xmm_s0, xmm5);
    pCT += srcCurStep;
    pRT += srcRefStep;

    xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
    xmm1 = _mm_sub_epi16(xmm1, xmm5);
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
    xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
    xmm5 = _mm_sub_epi16(xmm5, xmm4);
    xmm_s1 = xmm1;
    xmm1 = _mm_add_epi16(xmm1, xmm5);
    xmm_s1 = _mm_sub_epi16(xmm_s1, xmm5);
    pCT += srcCurStep;
    pRT += srcRefStep;

    xmm2 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
    xmm2 = _mm_sub_epi16(xmm2, xmm5);
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
    xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
    xmm5 = _mm_sub_epi16(xmm5, xmm4);
    xmm6 = xmm2;
    xmm2 = _mm_add_epi16(xmm2, xmm5);
    xmm6 = _mm_sub_epi16(xmm6, xmm5);
    pCT += srcCurStep;
    pRT += srcRefStep;

    xmm3 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT)));
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT)));
    xmm3 = _mm_sub_epi16(xmm3, xmm5);
    xmm5 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pCT + stepC4)));
    xmm4 = _mm_cvtepu8_epi16(MM_LOAD_EPI64((__m128i*)(pRT + stepR4)));
    xmm5 = _mm_sub_epi16(xmm5, xmm4);
    xmm7 = xmm3;
    xmm3 = _mm_add_epi16(xmm3, xmm5);
    xmm7 = _mm_sub_epi16(xmm7, xmm5);

    /* first stage - rows 0-3 */
    xmm4 = xmm2;
    xmm4 = _mm_add_epi16(xmm4, xmm3);
    xmm2 = _mm_sub_epi16(xmm2, xmm3);

    xmm3 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm1);
    xmm3 = _mm_sub_epi16(xmm3, xmm1);

    xmm1 = xmm3;
    xmm1 = _mm_add_epi16(xmm1, xmm2);
    xmm3 = _mm_sub_epi16(xmm3, xmm2);

    xmm2 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm4);
    xmm2 = _mm_sub_epi16(xmm2, xmm4);

    /* transpose and split */
    xmm4 = xmm0;
    xmm0 = _mm_unpacklo_epi64(xmm0, xmm2);
    xmm4 = _mm_unpackhi_epi64(xmm4, xmm2);

    xmm2 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm4);
    xmm2 = _mm_sub_epi16(xmm2, xmm4);

    xmm4 = xmm1;
    xmm1 = _mm_unpacklo_epi64(xmm1, xmm3);
    xmm4 = _mm_unpackhi_epi64(xmm4, xmm3);

    xmm3 = xmm1;
    xmm1 = _mm_add_epi16(xmm1, xmm4);
    xmm3 = _mm_sub_epi16(xmm3, xmm4);

    xmm4 = xmm0;
    xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
    xmm4 = _mm_unpackhi_epi16(xmm4, xmm1);

    xmm5 = xmm2;
    xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm3);

    xmm1 = xmm0;
    xmm0 = _mm_unpacklo_epi32(xmm0, xmm2);
    xmm1 = _mm_unpackhi_epi32(xmm1, xmm2);

    xmm3 = xmm4;
    xmm4 = _mm_unpacklo_epi32(xmm4, xmm5);
    xmm3 = _mm_unpackhi_epi32(xmm3, xmm5);

    /* first stage - rows 4-7 */ 
    xmm_s2 = xmm0;
    xmm_s3 = xmm1;
    xmm0 = xmm_s0;
    xmm1 = xmm_s1;
    xmm_s0 = xmm4;
    xmm_s1 = xmm3;
    xmm2 = xmm6;
    xmm3 = xmm7;

    xmm4 = xmm2;
    xmm4 = _mm_add_epi16(xmm4, xmm3);
    xmm2 = _mm_sub_epi16(xmm2, xmm3);

    xmm3 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm1);
    xmm3 = _mm_sub_epi16(xmm3, xmm1);

    xmm1 = xmm3;
    xmm1 = _mm_add_epi16(xmm1, xmm2);
    xmm3 = _mm_sub_epi16(xmm3, xmm2);

    xmm2 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm4);
    xmm2 = _mm_sub_epi16(xmm2, xmm4);

    /* transpose and split */
    xmm4 = xmm0;
    xmm0 = _mm_unpacklo_epi64(xmm0, xmm2);
    xmm4 = _mm_unpackhi_epi64(xmm4, xmm2);

    xmm2 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm4);
    xmm2 = _mm_sub_epi16(xmm2, xmm4);

    xmm4 = xmm1;
    xmm1 = _mm_unpacklo_epi64(xmm1, xmm3);
    xmm4 = _mm_unpackhi_epi64(xmm4, xmm3);

    xmm3 = xmm1;
    xmm1 = _mm_add_epi16(xmm1, xmm4);
    xmm3 = _mm_sub_epi16(xmm3, xmm4);

    xmm4 = xmm0;
    xmm0 = _mm_unpacklo_epi16(xmm0, xmm1);
    xmm4 = _mm_unpackhi_epi16(xmm4, xmm1);

    xmm5 = xmm2;
    xmm2 = _mm_unpacklo_epi16(xmm2, xmm3);
    xmm5 = _mm_unpackhi_epi16(xmm5, xmm3);

    xmm1 = _mm_unpackhi_epi32(xmm0, xmm2);
    xmm0 = _mm_unpacklo_epi32(xmm0, xmm2);

    xmm3 = xmm4;
    xmm4 = _mm_unpacklo_epi32(xmm4, xmm5);
    xmm3 = _mm_unpackhi_epi32(xmm3, xmm5);

    xmm6 = xmm4;
    xmm7 = xmm3;

    /* second stage - rows 0-3 */
    xmm3 = xmm_s3;
    xmm2 = xmm3;
    xmm2 = _mm_unpacklo_epi64(xmm2, xmm1);
    xmm3 = _mm_unpackhi_epi64(xmm3, xmm1);

    xmm4 = xmm_s2;
    xmm1 = _mm_unpackhi_epi64(xmm4, xmm0);
    xmm0 = _mm_unpacklo_epi64(xmm4, xmm0);

    xmm4 = xmm2;
    xmm4 = _mm_add_epi16(xmm4, xmm3);
    xmm2 = _mm_sub_epi16(xmm2, xmm3);

    xmm3 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm1);
    xmm3 = _mm_sub_epi16(xmm3, xmm1);

    xmm1 = xmm3;
    xmm1 = _mm_add_epi16(xmm1, xmm2);
    xmm3 = _mm_sub_epi16(xmm3, xmm2);

    xmm2 = xmm0;
    xmm0 = _mm_add_epi16(xmm0, xmm4);
    xmm2 = _mm_sub_epi16(xmm2, xmm4);

    xmm0 = _mm_abs_epi16(xmm0);
    xmm1 = _mm_abs_epi16(xmm1);
    xmm2 = _mm_abs_epi16(xmm2);
    xmm3 = _mm_abs_epi16(xmm3);

    xmm0 = _mm_add_epi16(xmm0, xmm1);
    xmm0 = _mm_add_epi16(xmm0, xmm2);
    xmm0 = _mm_add_epi16(xmm0, xmm3);

    /* second stage - rows 4-7 */
    xmm1 = xmm_s0;
    xmm2 = xmm1;
    xmm1 = _mm_unpacklo_epi64(xmm1, xmm6);
    xmm2 = _mm_unpackhi_epi64(xmm2, xmm6);

    xmm3 = xmm_s1;
    xmm6 = _mm_unpacklo_epi64(xmm3, xmm7);
    xmm7 = _mm_unpackhi_epi64(xmm3, xmm7);

    xmm4 = xmm6;
    xmm4 = _mm_add_epi16(xmm4, xmm7);
    xmm6 = _mm_sub_epi16(xmm6, xmm7);

    xmm7 = xmm1;
    xmm1 = _mm_add_epi16(xmm1, xmm2);
    xmm7 = _mm_sub_epi16(xmm7, xmm2);

    xmm2 = xmm7;
    xmm2 = _mm_add_epi16(xmm2, xmm6);
    xmm7 = _mm_sub_epi16(xmm7, xmm6);

    xmm6 = xmm1;
    xmm1 = _mm_add_epi16(xmm1, xmm4);
    xmm6 = _mm_sub_epi16(xmm6, xmm4);

    xmm1 = _mm_abs_epi16(xmm1);
    xmm2 = _mm_abs_epi16(xmm2);
    xmm6 = _mm_abs_epi16(xmm6);
    xmm7 = _mm_abs_epi16(xmm7);

    xmm0 = _mm_add_epi16(xmm0, xmm1);
    xmm0 = _mm_add_epi16(xmm0, xmm2);
    xmm0 = _mm_add_epi16(xmm0, xmm6);
    xmm0 = _mm_add_epi16(xmm0, xmm7);

    /* use single packed 16*16->32 multiply for first step (slightly faster than shuffle/add) */
    xmm0 = _mm_madd_epi16(xmm0, _mm_set1_epi16(1));
    xmm1 = _mm_shuffle_epi32(xmm0, 0x0E);
    xmm0 = _mm_add_epi32(xmm0, xmm1);
    xmm1 = _mm_shuffle_epi32(xmm0, 0x01);
    xmm0 = _mm_add_epi32(xmm0, xmm1);

    return _mm_cvtsi128_si32(xmm0);
}

/* call twice for SSE version (no speed advantage with 128-bit registers) */
void H265_FASTCALL MAKE_NAME(h265_SATD_8x8_Pair_8u)(const Ipp8u* pSrcCur, int srcCurStep, const Ipp8u* pSrcRef, int srcRefStep, Ipp32s* satdPair)
{
    satdPair[0] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 0, srcCurStep, pSrcRef + 0, srcRefStep);
    satdPair[1] = MAKE_NAME(h265_SATD_8x8_8u)(pSrcCur + 8, srcCurStep, pSrcRef + 8, srcRefStep);
}

} // end namespace MFX_HEVC_PP

#endif  // #if defined(MFX_TARGET_OPTIMIZATION_AUTO) ...

#endif  // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
