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

/*
// HEVC interpolation filters, implemented with AVX2 (256-bit integer instructions)
// 
// NOTES:
// - requires AVX2
// - to avoid SSE/AVX transition penalties, ensure that AVX2 code is wrapped with _mm256_zeroupper() 
//     (include explicitly on function entry, ICL should automatically add before function return)
// - requires that width = multiple of 4 (LUMA) or multiple of 2 (CHROMA)
// - requires that height = multiple of 2 for VERTICAL filters (processes 2 rows at a time to reduce shuffles)
// - horizontal filters can take odd heights, e.g. generating support pixels for vertical pass
// - input data is read in chunks of up to 24 bytes regardless of width, so assume it's safe to read up to 24 bytes beyond right edge of block 
//     (data not used, just must be allocated memory - pad inbuf as needed)
// - might require /Qsfalign option to ensure that ICL keeps 16-byte aligned stack when storing xmm registers
//     - have observed crashes when compiler (ICL 13) copies data from aligned ROM tables to non-aligned location on stack (compiler bug?)
// - templates used to reduce branches, so shift/offset values restricted to currently-used scale factors (see asserts below)
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_AUTO) 

#include <immintrin.h>

#ifdef __INTEL_COMPILER
/* disable warning: unused function parameter (offset not needed with template implementation) */
#pragma warning( disable : 869 )
#endif

namespace MFX_HEVC_PP
{

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

enum EnumPlane
{
    TEXT_LUMA = 0,
    TEXT_CHROMA,
    TEXT_CHROMA_U,
    TEXT_CHROMA_V,
};

/* interleaved luma interp coefficients, 8-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const signed char filtTabLuma_S8[3*4][32] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

};

/* interleaved 4-tap FIR luma interp coefficients, 8-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const signed char filtTabLumaFast_S8[3*2][32] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52 }, 
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    {  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    {  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8 },

    {  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20 },
    {  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4 }
};

/* interleaved chroma interp coefficients, 8-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(32) static const signed char filtTabChroma_S8[7*2][32] = {
    {  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58 }, 
    {  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2 },

    {  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54 },
    {  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2 },

    {  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46 },
    {  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4 },

    {  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36 },
    {  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4 },

    {  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28 },
    {  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6 },

    {  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16 },
    {  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4 },

    {  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10 },
    {  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2 },
};

/* interleaved luma interp coefficients, 16-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const short filtTabLuma_S16[3*4][16] = {
    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58, -10,  58 },
    {  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5,  17,  -5 },
    {   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0 },

    {  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4 },
    { -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40 },
    {  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11,  40, -11 },
    {   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1,   4,  -1 },

    {   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1,   0,   1 },
    {  -5,   17, -5,  17,  -5,  17,  -5,  17,  -5,   17, -5,  17,  -5,  17,  -5,  17 },
    {  58,  -10, 58, -10,  58, -10,  58, -10,  58,  -10, 58, -10,  58, -10,  58, -10 },
    {   4,   -1,  4,  -1,   4,  -1,   4,  -1,   4,   -1,  4,  -1,   4,  -1,   4,  -1 },

};

//kolya
/* interleaved luma fast interp coefficients, 16-bit, for offsets 1/4, 2/4, 3/4 */
ALIGN_DECL(32) static const short filtTabLumaFast_S16[3*2][16] = {
    {  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52 },
    {  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4 },

    {  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40 },
    {  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8,  40,  -8 },

    {  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20,  -4,  20 },
    {  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4,  52,  -4 }
};

/* interleaved chroma interp coefficients, 16-bit, for offsets 1/8, 2/8, ... 7/8 */
ALIGN_DECL(32) static const short filtTabChroma_S16[7*2][16] = {
    {  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58 },
    {  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2 },

    {  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54 },
    {  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2 },

    {  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46 },
    {  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4 },

    {  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36 },
    {  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4,  36,  -4 },

    {  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28,  -4,  28 },
    {  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6,  46,  -6 },

    {  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16,  -2,  16 },
    {  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4,  54,  -4 },

    {  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10,  -2,  10 },
    {  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2,  58,  -2 },
};

/* pshufb table for luma, 8-bit horizontal filtering, identical shuffle within each 128-bit lane */
ALIGN_DECL(32) static const signed char shufTabPlane[4][32] = {
    {  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  0,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8 },
    {  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10,  2,  3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10 },
    {  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12,  4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12 },
    {  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14,  6,  7,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14 },
};

/* pshufb table for luma, 16-bit horizontal filtering */
ALIGN_DECL(32) static const signed char shufTabPlane16[2][32] = {
    {  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9,  0,  1,  2,  3,  2,  3,  4,  5,  4,  5,  6,  7,  6,  7,  8,  9 },
    {  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13,  4,  5,  6,  7,  6,  7,  8,  9,  8,  9, 10, 11, 10, 11, 12, 13 },
};

/* pshufb table for chroma, 8-bit horizontal filtering, identical shuffle within each 128-bit lane */
ALIGN_DECL(32) static const signed char shufTabIntUV[2][32] = {
    {  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9,  0,  2,  1,  3,  2,  4,  3,  5,  4,  6,  5,  7,  6,  8,  7,  9 },
    {  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13,  4,  6,  5,  7,  6,  8,  7,  9,  8, 10,  9, 11, 10, 12, 11, 13 },
};

/* pshufb table for interleaved chroma, 16-bit horizontal filtering */
ALIGN_DECL(32) static const signed char shufTabIntUV16[2][32] = {
    {  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11,  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11 },
    {  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11,  0,  1,  4,  5,  2,  3,  6,  7,  4,  5,  8,  9,  6,  7, 10, 11 },
};

/* luma, horizontal, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
template<int width, int shift>
static void t_InterpLuma_s8_d16_H_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int height)
{
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3;

    _mm256_zeroupper();

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];
    
    /* calculate 16 outputs per inner loop, working horizontally */
//#ifdef __INTEL_COMPILER
//    #pragma unroll(4)
//#endif
    for (int row = 0; row < height; row++) {
        _mm_prefetch((const char *)(pSrc+srcPitch), _MM_HINT_T0);
        for (int col = 0; col < width; col += 16) {

            /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
            ymm3 = _mm256_loadu_si256((__m256i *)(pSrc + col));
            ymm3 = _mm256_permute4x64_epi64(ymm3, _MM_SHUFFLE(2,1,1,0));
            _mm_prefetch((const char *)(pSrc+col), _MM_HINT_T0);

            /* interleave pixels */
            ymm0 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[0]));
            ymm1 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[1]));
            ymm2 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[2]));
            ymm3 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane[3]));

            /* packed (8*8 + 8*8) -> 16 */
            ymm0 = _mm256_maddubs_epi16(ymm0, *(__m256i *)(coeffs +  0));    /* coefs 0,1 */
            ymm1 = _mm256_maddubs_epi16(ymm1, *(__m256i *)(coeffs + 32));    /* coefs 2,3 */
            ymm2 = _mm256_maddubs_epi16(ymm2, *(__m256i *)(coeffs + 64));    /* coefs 4,5 */
            ymm3 = _mm256_maddubs_epi16(ymm3, *(__m256i *)(coeffs + 96));    /* coefs 6,7 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi16(ymm0, ymm1);
            ymm0 = _mm256_add_epi16(ymm0, ymm2);
            ymm0 = _mm256_add_epi16(ymm0, ymm3);
            if (shift > 0) {
                ymm0 = _mm256_mulhrs_epi16(ymm0, _mm256_set1_epi16(1<<(15-shift)));
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i *)(pDst + col), ymm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    }
}

//kolya
template<int shift>
static void t_InterpLumaFast_s8_d16_H_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m256i ymm0, ymm1, ymm2, ymm3;

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];
    shufTab = (signed char *)shufTabPlane;

    _mm256_zeroupper();

    ymm2 = _mm256_load_si256((__m256i *)(coeffs +  0));
    ymm3 = _mm256_load_si256((__m256i *)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 16) {
            /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
            ymm1 = _mm256_loadu_si256((__m256i *)(pSrc + col));
            ymm1 = _mm256_permute4x64_epi64(ymm1, 0x94);

            /* interleave pixels */
            ymm0 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab +  0));
            ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm2);    /* coefs 0,1 */
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi16(ymm0, ymm1);
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i *)(pDst + col), ymm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height); 
}

template<int widthMul, int shift>
static void t_InterpLuma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm3 = _mm_loadu_si128((__m128i *)(pSrc + col));

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane[0]));
            xmm1 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane[1]));
            xmm2 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane[2]));
            xmm3 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane[3]));

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_maddubs_epi16(xmm1, *(__m128i *)(coeffs + 32));    /* coefs 2,3 */
            xmm2 = _mm_maddubs_epi16(xmm2, *(__m128i *)(coeffs + 64));    /* coefs 4,5 */
            xmm3 = _mm_maddubs_epi16(xmm3, *(__m128i *)(coeffs + 96));    /* coefs 6,7 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi16(xmm0, xmm1);
            xmm0 = _mm_add_epi16(xmm0, xmm2);
            xmm0 = _mm_add_epi16(xmm0, xmm3);
            if (shift > 0) {
                xmm0 = _mm_mulhrs_epi16(xmm0, _mm_set1_epi16(1<<(15-shift)));
            }

            if (widthMul == 8) {
                /* store 8 16-bit words */
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 4) {
                /* store 4 16-bit words */
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

//kolya
template<int widthMul, int shift>
static void t_InterpLumaFast_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];
    shufTab = (signed char *)shufTabPlane;

    xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
            xmm1 = xmm0;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, xmm2);    /* coefs 0,1 */
            xmm1 = _mm_maddubs_epi16(xmm1, xmm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi16(xmm0, xmm1);
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            } 
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* luma, horizontal, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0) {
            switch (width) {
            case 16: t_InterpLuma_s8_d16_H_AVX2<16,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 32: t_InterpLuma_s8_d16_H_AVX2<32,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 48: t_InterpLuma_s8_d16_H_AVX2<48,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 64: t_InterpLuma_s8_d16_H_AVX2<64,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            default: while (width) { t_InterpLuma_s8_d16_H_AVX2<16,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); pSrc += 16; pDst += 16; width -= 16; }
            }
        } else {    // shift == 6
            switch (width) {
            case 16: t_InterpLuma_s8_d16_H_AVX2<16,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 32: t_InterpLuma_s8_d16_H_AVX2<32,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 48: t_InterpLuma_s8_d16_H_AVX2<48,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            case 64: t_InterpLuma_s8_d16_H_AVX2<64,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); break;
            default: while (width) { t_InterpLuma_s8_d16_H_AVX2<16,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, height); pSrc += 16; pDst += 16; width -= 16; }
            }
        }
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLuma_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLuma_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

//kolya
void MAKE_NAME(h265_InterpLumaFast_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_H_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_H_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* chroma, horizontal, 8-bit input, 16-bit output 
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpChroma_s8_d16_H_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int plane )
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m256i ymm0, ymm1, ymm2, ymm3;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    _mm256_zeroupper();

    ymm2 = _mm256_load_si256((__m256i *)(coeffs +  0));
    ymm3 = _mm256_load_si256((__m256i *)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working horizontally */
    do {
        _mm_prefetch((const char *)(pSrc + srcPitch), _MM_HINT_T0);
        for (col = 0; col < width; col += 16) {
            /* load 24 8-bit pixels, permute into ymm lanes as [0-7 | 8-15 | 8-15 | 16-23] */
            ymm1 = _mm256_loadu_si256((__m256i *)(pSrc + col));
            ymm1 = _mm256_permute4x64_epi64(ymm1, 0x94);
            _mm_prefetch((const char *)(pSrc + col), _MM_HINT_T0);

            /* interleave pixels */
            ymm0 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab +  0));
            ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            ymm0 = _mm256_maddubs_epi16(ymm0, ymm2);    /* coefs 0,1 */
            ymm1 = _mm256_maddubs_epi16(ymm1, ymm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi16(ymm0, ymm1);
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
            }

            /* store 16 16-bit words */
            _mm256_storeu_si256((__m256i *)(pDst + col), ymm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

template<int widthMul, int shift>
static void t_InterpChroma_s8_d16_H(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height, int plane )
{
    int col;
    const signed char* coeffs;
    const signed char* shufTab;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];
    if (plane == TEXT_CHROMA)
        shufTab = (signed char *)shufTabIntUV;
    else 
        shufTab = (signed char *)shufTabPlane;

    xmm2 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm3 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working horizontally - use shuffle/interleave instead of pshufb on Atom */
    do {
        /* same version for U/V interleaved or separate planes */
        for (col = 0; col < width; col += widthMul) {
            /* load 16 8-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
            xmm1 = xmm0;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTab +  0));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTab + 32));

            /* packed (8*8 + 8*8) -> 16 */
            xmm0 = _mm_maddubs_epi16(xmm0, xmm2);    /* coefs 0,1 */
            xmm1 = _mm_maddubs_epi16(xmm1, xmm3);    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi16(xmm0, xmm1);
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm0 = _mm_srai_epi16(xmm0, shift);
            }

            /* store 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 6) {
                *(int *)(pDst+col+0) = _mm_extract_epi32(xmm0, 0);
                *(int *)(pDst+col+2) = _mm_extract_epi32(xmm0, 1);
                *(int *)(pDst+col+4) = _mm_extract_epi32(xmm0, 2);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 2) {
                *(int *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* chroma, horizontal, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s8_d16_H)(INTERP_S8_D16_PARAMETERS_LIST, int plane)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x01) == 0 );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height, plane);
        pSrc += width;
        pDst += width;
    }

    if (rem == 4) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    } else if (rem == 2) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<2,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<2,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    } else if (rem == 6) {
        if (shift == 0)
            t_InterpChroma_s8_d16_H<6,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
        else if (shift == 6)
            t_InterpChroma_s8_d16_H<6,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height, plane);
    }
}

template<int shift, int offset>
static void t_InterpLuma_s16_d16_H_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3;

    _mm256_zeroupper();

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 8) {
            ymm2 = _mm256_loadu_si256((__m256i *)(pSrc + col));
            ymm0 = _mm256_permute4x64_epi64(ymm2, 0x94);    /* [0-3 | 4-7 | 4-7 | 8-11] */
            ymm1 = ymm0;
            ymm2 = _mm256_permute4x64_epi64(ymm2, 0xe9);    /* [4-7 | 8-11 | 8-11 | 12-15] */
            ymm3 = ymm2;

            /* interleave pixels */
            ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)(shufTabPlane16[0]));
            ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTabPlane16[1]));
            ymm2 = _mm256_shuffle_epi8(ymm2, *(__m256i *)(shufTabPlane16[0]));
            ymm3 = _mm256_shuffle_epi8(ymm3, *(__m256i *)(shufTabPlane16[1]));

            /* packed (16*16 + 16*16) -> 32 */
            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)(coeffs +  0));    /* coefs 0,1 */
            ymm1 = _mm256_madd_epi16(ymm1, *(__m256i *)(coeffs + 32));    /* coefs 2,3 */
            ymm2 = _mm256_madd_epi16(ymm2, *(__m256i *)(coeffs + 64));    /* coefs 4,5 */
            ymm3 = _mm256_madd_epi16(ymm3, *(__m256i *)(coeffs + 96));    /* coefs 6,7 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            ymm0 = _mm256_add_epi32(ymm0, ymm2);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);
            if (shift == 6) {
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(offset));
            } 
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);

            /* store 8 16-bit words */
            ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
            _mm_storeu_si128((__m128i *)(pDst + col), mm128(ymm0));        
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

//kolya
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_H_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m256i ymm0, ymm1;

    _mm256_zeroupper();

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 8) {
            /* load, interleave 8 16-bit pixels */
            ymm0 = _mm256_permute4x64_epi64(*(__m256i *)(pSrc + col), 0x94);    /* [0-3 | 4-7 | 4-7 | 8-11] */
            ymm1 = ymm0;
            ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)(shufTabPlane16[0]));
            ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTabPlane16[1]));

            /* packed (16*16 + 16*16) -> 32 */
            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)(coeffs +  0));    /* coefs 0,1 */
            ymm1 = _mm256_madd_epi16(ymm1, *(__m256i *)(coeffs + 32));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            if (shift == 6) {
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(offset));
            } 
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);

            /* store 8 16-bit words */
            ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
            _mm_storeu_si128((__m128i *)(pDst + col), mm128(ymm0));        
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}


template<int shift, int offset>
static void t_InterpLuma_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3;

    coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load 8 16-bit pixels */
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
            xmm2 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* words 8-11 */
            xmm2 = _mm_alignr_epi8(xmm2, xmm0, 8);                  /* words 4-11 */
            xmm1 = xmm0;
            xmm3 = xmm2;

            /* interleave pixels */
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));
            xmm2 = _mm_shuffle_epi8(xmm2, *(__m128i *)(shufTabPlane16[0]));
            xmm3 = _mm_shuffle_epi8(xmm3, *(__m128i *)(shufTabPlane16[1]));

            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 32));    /* coefs 2,3 */
            xmm2 = _mm_madd_epi16(xmm2, *(__m128i *)(coeffs + 64));    /* coefs 4,5 */
            xmm3 = _mm_madd_epi16(xmm3, *(__m128i *)(coeffs + 96));    /* coefs 6,7 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            xmm0 = _mm_add_epi32(xmm0, xmm2);
            xmm0 = _mm_add_epi32(xmm0, xmm3);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            /* store 4 16-bit words */
            _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

//kolya
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1;

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
            xmm1 = xmm0;
            xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
            xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));

            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 32));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height); 
}

/* luma, horizontal, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
{
    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        switch (shift) {
        case 6:  t_InterpLuma_s16_d16_H_AVX2<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

        case 0:  t_InterpLuma_s16_d16_H_AVX2<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 1:  t_InterpLuma_s16_d16_H_AVX2<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 2:  t_InterpLuma_s16_d16_H_AVX2<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        }
        return;
    }

    switch (shift) {
    case 6:  t_InterpLuma_s16_d16_H<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

    case 0:  t_InterpLuma_s16_d16_H<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 1:  t_InterpLuma_s16_d16_H<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 2:  t_InterpLuma_s16_d16_H<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

//kolya
/* luma, horizontal, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLumaFast_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST)
{
    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x03) == 0 );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        switch (shift) {
        case 6:  t_InterpLumaFast_s16_d16_H_AVX2<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

        case 0:  t_InterpLumaFast_s16_d16_H_AVX2<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 1:  t_InterpLumaFast_s16_d16_H_AVX2<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 2:  t_InterpLumaFast_s16_d16_H_AVX2<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        }
        return;
    }

    switch (shift) {
    case 6:  t_InterpLumaFast_s16_d16_H<6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

    case 0:  t_InterpLumaFast_s16_d16_H<0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 1:  t_InterpLumaFast_s16_d16_H<1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 2:  t_InterpLumaFast_s16_d16_H<2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

template<int plane, int shift, int offset>
static void t_InterpChroma_s16_d16_H_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m256i ymm0, ymm1;

    _mm256_zeroupper();

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    /* calculate 8 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 8) {
            /* load, interleave 8 16-bit pixels */
            if (plane == TEXT_CHROMA) {
                ymm1 = _mm256_loadu_si256((__m256i *)(pSrc + col));
                ymm0 = _mm256_permute4x64_epi64(ymm1, 0x94);            /* bytes [0-7  |  8-15 |  8-15 | 16-23] */
                ymm1 = _mm256_permute4x64_epi64(ymm1, 0xe9);            /* bytes [8-15 | 16-23 | 16-23 | 24-31] */
                ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)(shufTabIntUV16[0]));
                ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTabIntUV16[0]));
            } else {
                ymm0 = _mm256_permute4x64_epi64(*(__m256i *)(pSrc + col), 0x94);    /* [0-3 | 4-7 | 4-7 | 8-11] */
                ymm1 = ymm0;
                ymm0 = _mm256_shuffle_epi8(ymm0, *(__m256i *)(shufTabPlane16[0]));
                ymm1 = _mm256_shuffle_epi8(ymm1, *(__m256i *)(shufTabPlane16[1]));
            }

            /* packed (16*16 + 16*16) -> 32 */
            ymm0 = _mm256_madd_epi16(ymm0, *(__m256i *)(coeffs +  0));    /* coefs 0,1 */
            ymm1 = _mm256_madd_epi16(ymm1, *(__m256i *)(coeffs + 32));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi32(ymm0, ymm1);
            if (shift == 6) {
                ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(offset));
            } 
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);

            /* store 8 16-bit words */
            ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);
            _mm_storeu_si128((__m128i *)(pDst + col), mm128(ymm0));        
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

template<int plane, int widthMul, int shift, int offset>
static void t_InterpChroma_s16_d16_H(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int col;
    const signed char* coeffs;
    __m128i xmm0, xmm1;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    /* calculate 4 outputs per inner loop, working horizontally */
    do {
        for (col = 0; col < width; col += 4) {
            /* load, interleave 8 16-bit pixels */
            if (plane == TEXT_CHROMA) {
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
                xmm1 = _mm_loadl_epi64((__m128i *)(pSrc + col + 8));    /* words 8-11 */
                xmm1 = _mm_alignr_epi8(xmm1, xmm0, 8);                  /* words 4-11 */
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabIntUV16[0]));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabIntUV16[1]));
            } else {
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));        /* words 0-7 */
                xmm1 = xmm0;
                xmm0 = _mm_shuffle_epi8(xmm0, *(__m128i *)(shufTabPlane16[0]));
                xmm1 = _mm_shuffle_epi8(xmm1, *(__m128i *)(shufTabPlane16[1]));
            }

            /* packed (16*16 + 16*16) -> 32 */
            xmm0 = _mm_madd_epi16(xmm0, *(__m128i *)(coeffs +  0));    /* coefs 0,1 */
            xmm1 = _mm_madd_epi16(xmm1, *(__m128i *)(coeffs + 32));    /* coefs 2,3 */

            /* sum intermediate values, add offset, shift off fraction bits */
            xmm0 = _mm_add_epi32(xmm0, xmm1);
            if (shift == 6) {
                xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            } 
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);

            if (widthMul == 4) {
                /* store 4 16-bit words */
                _mm_storel_epi64((__m128i *)(pDst + col), xmm0);
            } else if (widthMul == 2) {
                /* store 2 16-bit words */
                *(int *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
    } while (--height);
}

/* chroma, horizontal, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s16_d16_H)(INTERP_S16_D16_PARAMETERS_LIST, int plane)
{
    int rem;

    VM_ASSERT( (shift <= 2 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( (width & 0x01) == 0 );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        if (plane == TEXT_CHROMA) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
        } else {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA_U, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA_U, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA_U, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H_AVX2<TEXT_CHROMA_U, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
        }
        return;
    }

    rem = (width & 0x03);
    width -= rem;

    if (plane == TEXT_CHROMA) {
        if (width > 0) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   4, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
            pSrc += width;
            pDst += width;
        }

        if (rem == 2) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA,   2, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            }
        }
    } else {
        if (width > 0) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 4, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
            }
            pSrc += width;
            pDst += width;
        }

        if (rem == 2) {
            switch (shift) {
            case 6:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 6, 32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;

            case 0:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 0,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 1:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 1,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            case 2:  t_InterpChroma_s16_d16_H<TEXT_CHROMA_U, 2, 2,  0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
            }
        }
    }
}

/* luma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpLuma_s8_d16_V_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    const signed char* coeffs = filtTabLuma_S8[4 * (tab_index-1)];

    _mm256_zeroupper();

    // process any 32-wide columns
    for (int col = width >> 5; col != 0; col--) {
        const unsigned char *pS = pSrc;
        short *pD = pDst;

        // load 32 8-bit pixels from rows 0-6
        __m256i y0 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 0*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y1 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 1*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y2 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 2*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y3 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 3*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y4 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 4*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y5 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 5*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y6 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 6*srcPitch)), _MM_SHUFFLE(3,1,2,0));

        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (int row = 0; row < height; row++) {

            __m256i y7 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 7*srcPitch)), _MM_SHUFFLE(3,1,2,0));
            _mm_prefetch((const char *)(pS + 8*srcPitch), _MM_HINT_T0);

            // interleave rows
            __m256i t0 = _mm256_unpacklo_epi8(y0, y1);
            __m256i t1 = _mm256_unpacklo_epi8(y2, y3);
            __m256i t2 = _mm256_unpacklo_epi8(y4, y5);
            __m256i t3 = _mm256_unpacklo_epi8(y6, y7);

            __m256i u0 = _mm256_unpackhi_epi8(y0, y1);
            __m256i u1 = _mm256_unpackhi_epi8(y2, y3);
            __m256i u2 = _mm256_unpackhi_epi8(y4, y5);
            __m256i u3 = _mm256_unpackhi_epi8(y6, y7);

            // multiply by interleaved coefs
            t0 = _mm256_maddubs_epi16(t0, *(__m256i*)(coeffs +  0));
            t1 = _mm256_maddubs_epi16(t1, *(__m256i*)(coeffs + 32));
            t2 = _mm256_maddubs_epi16(t2, *(__m256i*)(coeffs + 64));
            t3 = _mm256_maddubs_epi16(t3, *(__m256i*)(coeffs + 96));
            t0 = _mm256_add_epi16(t0, t1);
            t0 = _mm256_add_epi16(t0, t2);
            t0 = _mm256_add_epi16(t0, t3);

            u0 = _mm256_maddubs_epi16(u0, *(__m256i*)(coeffs +  0));
            u1 = _mm256_maddubs_epi16(u1, *(__m256i*)(coeffs + 32));
            u2 = _mm256_maddubs_epi16(u2, *(__m256i*)(coeffs + 64));
            u3 = _mm256_maddubs_epi16(u3, *(__m256i*)(coeffs + 96));
            u0 = _mm256_add_epi16(u0, u1);
            u0 = _mm256_add_epi16(u0, u2);
            u0 = _mm256_add_epi16(u0, u3);

            // add offset and shift
            if (shift > 0) {
                t0 = _mm256_mulhrs_epi16(t0, _mm256_set1_epi16(1<<(15-shift)));
                u0 = _mm256_mulhrs_epi16(u0, _mm256_set1_epi16(1<<(15-shift)));
            }

            // store 32 16-bit values
            _mm256_storeu_si256((__m256i*)(pD +  0), t0);
            _mm256_storeu_si256((__m256i*)(pD + 16), u0);

            // shift rows
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = y4;
            y4 = y5;
            y5 = y6;
            y6 = y7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 32;
        pDst += 32;
    }

    // process any 16-wide columns
    if (width & 0x10) {
        const unsigned char *pS = pSrc;
        short *pD = pDst;

        // load 16 8-bit pixels from rows 0-6
        __m256i y0 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 0*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y1 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 1*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y2 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 2*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y3 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 3*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y4 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 4*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y5 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 5*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y6 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 6*srcPitch)), _MM_SHUFFLE(3,1,2,0));

        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (int row = 0; row < height; row++) {

            __m256i y7 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 7*srcPitch)), _MM_SHUFFLE(3,1,2,0));

            // interleave rows
            __m256i t0 = _mm256_unpacklo_epi8(y0, y1);
            __m256i t1 = _mm256_unpacklo_epi8(y2, y3);
            __m256i t2 = _mm256_unpacklo_epi8(y4, y5);
            __m256i t3 = _mm256_unpacklo_epi8(y6, y7);

            // multiply by interleaved coefs
            t0 = _mm256_maddubs_epi16(t0, *(__m256i*)(coeffs +  0));
            t1 = _mm256_maddubs_epi16(t1, *(__m256i*)(coeffs + 32));
            t2 = _mm256_maddubs_epi16(t2, *(__m256i*)(coeffs + 64));
            t3 = _mm256_maddubs_epi16(t3, *(__m256i*)(coeffs + 96));
            t0 = _mm256_add_epi16(t0, t1);
            t0 = _mm256_add_epi16(t0, t2);
            t0 = _mm256_add_epi16(t0, t3);

            // add offset and shift
            if (shift > 0) {
                t0 = _mm256_mulhrs_epi16(t0, _mm256_set1_epi16(1<<(15-shift)));
            }

            // store 16 16-bit values
            _mm256_storeu_si256((__m256i*)pD, t0);

            // shift rows
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = y4;
            y4 = y5;
            y5 = y6;
            y6 = y7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 16;
        pDst += 16;
    }

    // process any 8-wide columns
    if (width & 0x8) {
        const unsigned char *pS = pSrc;
        short *pD = pDst;

        // load 8 8-bit pixels from rows 0-6
        __m128i x0 = _mm_loadl_epi64((__m128i*)(pS + 0*srcPitch));
        __m128i x1 = _mm_loadl_epi64((__m128i*)(pS + 1*srcPitch));
        __m128i x2 = _mm_loadl_epi64((__m128i*)(pS + 2*srcPitch));
        __m128i x3 = _mm_loadl_epi64((__m128i*)(pS + 3*srcPitch));
        __m128i x4 = _mm_loadl_epi64((__m128i*)(pS + 4*srcPitch));
        __m128i x5 = _mm_loadl_epi64((__m128i*)(pS + 5*srcPitch));
        __m128i x6 = _mm_loadl_epi64((__m128i*)(pS + 6*srcPitch));
        
        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (int row = 0; row < height; row++) {

            __m128i x7 = _mm_loadl_epi64((__m128i*)(pS + 7*srcPitch));

            // interleave rows
            __m128i t0 = _mm_unpacklo_epi8(x0, x1);
            __m128i t1 = _mm_unpacklo_epi8(x2, x3);
            __m128i t2 = _mm_unpacklo_epi8(x4, x5);
            __m128i t3 = _mm_unpacklo_epi8(x6, x7);

            // multiply by interleaved coefs
            t0 = _mm_maddubs_epi16(t0, *(__m128i*)(coeffs +  0));
            t1 = _mm_maddubs_epi16(t1, *(__m128i*)(coeffs + 32));
            t2 = _mm_maddubs_epi16(t2, *(__m128i*)(coeffs + 64));
            t3 = _mm_maddubs_epi16(t3, *(__m128i*)(coeffs + 96));
            t0 = _mm_add_epi16(t0, t1);
            t0 = _mm_add_epi16(t0, t2);
            t0 = _mm_add_epi16(t0, t3);

            // add offset and shift
            if (shift > 0) {
                t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-shift)));
            }

            // store 8 16-bit values
            _mm_storeu_si128((__m128i*)pD, t0);

            // shift rows
            x0 = x1;
            x1 = x2;
            x2 = x3;
            x3 = x4;
            x4 = x5;
            x5 = x6;
            x6 = x7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 8;
        pDst += 8;
    }

    // process any 4-wide columns
    if (width & 0x4) {
        const unsigned char *pS = pSrc;
        short *pD = pDst;

        // load 4 8-bit pixels from rows 0-6
        __m128i x0 = _mm_cvtsi32_si128(*(int*)(pS + 0*srcPitch));
        __m128i x1 = _mm_cvtsi32_si128(*(int*)(pS + 1*srcPitch));
        __m128i x2 = _mm_cvtsi32_si128(*(int*)(pS + 2*srcPitch));
        __m128i x3 = _mm_cvtsi32_si128(*(int*)(pS + 3*srcPitch));
        __m128i x4 = _mm_cvtsi32_si128(*(int*)(pS + 4*srcPitch));
        __m128i x5 = _mm_cvtsi32_si128(*(int*)(pS + 5*srcPitch));
        __m128i x6 = _mm_cvtsi32_si128(*(int*)(pS + 6*srcPitch));
        
        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (int row = 0; row < height; row++) {

            __m128i x7 = _mm_cvtsi32_si128(*(int*)(pS + 7*srcPitch));

            // interleave rows
            __m128i t0 = _mm_unpacklo_epi8(x0, x1);
            __m128i t1 = _mm_unpacklo_epi8(x2, x3);
            __m128i t2 = _mm_unpacklo_epi8(x4, x5);
            __m128i t3 = _mm_unpacklo_epi8(x6, x7);

            // multiply by interleaved coefs
            t0 = _mm_maddubs_epi16(t0, *(__m128i*)(coeffs +  0));
            t1 = _mm_maddubs_epi16(t1, *(__m128i*)(coeffs + 32));
            t2 = _mm_maddubs_epi16(t2, *(__m128i*)(coeffs + 64));
            t3 = _mm_maddubs_epi16(t3, *(__m128i*)(coeffs + 96));
            t0 = _mm_add_epi16(t0, t1);
            t0 = _mm_add_epi16(t0, t2);
            t0 = _mm_add_epi16(t0, t3);

            // add offset and shift
            if (shift > 0) {
                t0 = _mm_mulhrs_epi16(t0, _mm_set1_epi16(1<<(15-shift)));
            }

            // store 4 16-bit values
            _mm_storel_epi64((__m128i*)pD, t0);

            // shift rows
            x0 = x1;
            x1 = x2;
            x2 = x3;
            x3 = x4;
            x4 = x5;
            x5 = x6;
            x6 = x7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 4;
        pDst += 4;
    }
}

//kolya
template<int shift>
static void t_InterpLumaFast_s8_d16_V_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{ 
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = filtTabLumaFast_S8[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 16) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* start by loading 16 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_loadu_si256((__m256i*)(pSrc + 0*srcPitch));
        ymm0 = _mm256_permute4x64_epi64( ymm0, 0x10);

        ymm4 = _mm256_loadu_si256((__m256i*)(pSrc + 1*srcPitch));
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0x10);

        ymm1 = _mm256_loadu_si256((__m256i*)(pSrc + 2*srcPitch));
        ymm1 = _mm256_permute4x64_epi64( ymm1, 0x10);

        ymm0 = _mm256_unpacklo_epi8(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi8(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            ymm5 = _mm256_loadu_si256((__m256i*)(pSrc + 3*srcPitch));
            ymm5 = _mm256_permute4x64_epi64( ymm5, 0x10);

            ymm2 = _mm256_loadu_si256((__m256i*)(pSrc + 4*srcPitch));
            ymm2 = _mm256_permute4x64_epi64( ymm2, 0x10);
            ymm1 = _mm256_unpacklo_epi8(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi8(ymm5, ymm2);

            ymm0 = _mm256_maddubs_epi16(ymm0, ymm6);
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi16(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi16(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
                ymm4 = _mm256_add_epi16(ymm4, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm4 = _mm256_srai_epi16(ymm4, shift);
            }

            _mm256_storeu_si256((__m256i*)(pDst + 0*dstPitch), ymm0);
            _mm256_storeu_si256((__m256i*)(pDst + 1*dstPitch), ymm4);

            /* shift row registers (1->0, 2->1, etc.) */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 16;
        pDstRef += 16;
    }
}

template<int widthMul, int shift>
static void t_InterpLumaFast_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabLumaFast_S8[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi8(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_maddubs_epi16(xmm0, xmm6);
            xmm4 = _mm_maddubs_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi16(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi16(xmm4, xmm3);
            
            /* add offset, shift off fraction bits */
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm4 = _mm_add_epi16(xmm4, _mm_set1_epi16( (1<<shift)>>1 ));
            }
            xmm0 = _mm_srai_epi16(xmm0, shift);
            xmm4 = _mm_srai_epi16(xmm4, shift);

            /* store 2, 4, 6 or 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storeu_si128((__m128i *)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i *)(pDst + 1*dstPitch), xmm4);
            } 
            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}


/* luma, vertical, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    if (shift == 0)
        t_InterpLuma_s8_d16_V_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
    else if (shift == 6)
        t_InterpLuma_s8_d16_V_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
}

//kolya
void MAKE_NAME(h265_InterpLumaFast_s8_d16_V)(INTERP_S8_D16_PARAMETERS_LIST)
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_V_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_V_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_V<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_V<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem > 0) {
        if (shift == 0)
            t_InterpLumaFast_s8_d16_V<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpLumaFast_s8_d16_V<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* chroma, vertical, 8-bit input, 16-bit output
 * AVX2 path used when width = multiple of 16
 */
template<int shift>
static void t_InterpChroma_s8_d16_V_AVX2(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = filtTabChroma_S8[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 16 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 16) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* start by loading 16 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_loadu_si256((__m256i*)(pSrc + 0*srcPitch));
        ymm0 = _mm256_permute4x64_epi64( ymm0, 0x10);

        ymm4 = _mm256_loadu_si256((__m256i*)(pSrc + 1*srcPitch));
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0x10);

        ymm1 = _mm256_loadu_si256((__m256i*)(pSrc + 2*srcPitch));
        ymm1 = _mm256_permute4x64_epi64( ymm1, 0x10);

        ymm0 = _mm256_unpacklo_epi8(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi8(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            ymm5 = _mm256_loadu_si256((__m256i*)(pSrc + 3*srcPitch));
            ymm5 = _mm256_permute4x64_epi64( ymm5, 0x10);

            ymm2 = _mm256_loadu_si256((__m256i*)(pSrc + 4*srcPitch));
            ymm2 = _mm256_permute4x64_epi64( ymm2, 0x10);
            ymm1 = _mm256_unpacklo_epi8(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi8(ymm5, ymm2);

            ymm0 = _mm256_maddubs_epi16(ymm0, ymm6);
            ymm4 = _mm256_maddubs_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi16(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_maddubs_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi16(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            if (shift > 0) {
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm0 = _mm256_srai_epi16(ymm0, shift);
                ymm4 = _mm256_add_epi16(ymm4, _mm256_set1_epi16( (1<<shift)>>1 ));
                ymm4 = _mm256_srai_epi16(ymm4, shift);
            }

            _mm256_storeu_si256((__m256i*)(pDst + 0*dstPitch), ymm0);
            _mm256_storeu_si256((__m256i*)(pDst + 1*dstPitch), ymm4);

            /* shift row registers (1->0, 2->1, etc.) */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 16;
        pDstRef += 16;
    }
}

template<int widthMul, int shift>
static void t_InterpChroma_s8_d16_V(const unsigned char* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const unsigned char *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabChroma_S8[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi8(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi8(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi8(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi8(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_maddubs_epi16(xmm0, xmm6);
            xmm4 = _mm_maddubs_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi16(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_maddubs_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi16(xmm4, xmm3);
            
            /* add offset, shift off fraction bits */
            if (shift > 0) {
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16( (1<<shift)>>1 ));
                xmm4 = _mm_add_epi16(xmm4, _mm_set1_epi16( (1<<shift)>>1 ));
            }
            xmm0 = _mm_srai_epi16(xmm0, shift);
            xmm4 = _mm_srai_epi16(xmm4, shift);

            /* store 2, 4, 6 or 8 16-bit words */
            if (widthMul == 8) {
                _mm_storeu_si128((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storeu_si128((__m128i *)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 6) {
                *(int *)(pDst+0*dstPitch+0) = _mm_extract_epi32(xmm0, 0);
                *(int *)(pDst+0*dstPitch+2) = _mm_extract_epi32(xmm0, 1);
                *(int *)(pDst+0*dstPitch+4) = _mm_extract_epi32(xmm0, 2);
                
                *(int *)(pDst+1*dstPitch+0) = _mm_extract_epi32(xmm4, 0);
                *(int *)(pDst+1*dstPitch+2) = _mm_extract_epi32(xmm4, 1);
                *(int *)(pDst+1*dstPitch+4) = _mm_extract_epi32(xmm4, 2);
            } else if (widthMul == 4) {
                _mm_storel_epi64((__m128i *)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i *)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 2) {
                *(int *)(pDst+0*dstPitch+0) = _mm_cvtsi128_si32(xmm0);
                *(int *)(pDst+1*dstPitch+0) = _mm_cvtsi128_si32(xmm4);
            }

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}

/* chroma, vertical, 8-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s8_d16_V) ( INTERP_S8_D16_PARAMETERS_LIST )
{
    int rem;

    VM_ASSERT( (shift == 0 && offset == 0) || (shift == 6 && offset == (1 << (shift-1))) );
    VM_ASSERT( ((width & 0x01) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 16 */
    if ((width & 0x0f) == 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V_AVX2<0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V_AVX2<6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        return;
    }

    rem = (width & 0x07);

    width -= rem;
    if (width > 0) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<8,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<8,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        pSrc += width;
        pDst += width;
    }

    if (rem == 4) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<4,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<4,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    } else if (rem == 2) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<2,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<2,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    } else if (rem == 6) {
        if (shift == 0)
            t_InterpChroma_s8_d16_V<6,0>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
        else if (shift == 6)
            t_InterpChroma_s8_d16_V<6,6>(pSrc, srcPitch, pDst, dstPitch, tab_index, rem, height);
    }
}

/* luma, vertical, 16-bit input, 16-bit output */
template<int shift, int offset>
static void t_InterpLuma_s16_d16_V_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    const signed char* coeffs = (const signed char *)filtTabLuma_S16[4 * (tab_index-1)];

    _mm256_zeroupper();

    // process any 16-wide columns
    for (int col = width >> 4; col != 0; col--) {
        const short *pS = pSrc;
        short *pD = pDst;

        // load 16 16-bit pixels from rows 0-6
        _mm_prefetch((const char *)(pS + 7*srcPitch), _MM_HINT_T0);
        __m256i y0 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 0*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y1 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 1*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y2 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 2*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y3 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 3*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y4 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 4*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y5 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 5*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y6 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 6*srcPitch)), _MM_SHUFFLE(3,1,2,0));

        // process rows vertically
//#ifdef __INTEL_COMPILER
//        #pragma unroll(8)
//#endif
        for (int row = 0; row < height; row++) {

            __m256i y7 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 7*srcPitch)), _MM_SHUFFLE(3,1,2,0));
            _mm_prefetch((const char *)(pS + 8*srcPitch), _MM_HINT_T0);

            // interleave rows
            __m256i t0 = _mm256_unpacklo_epi16(y0, y1);
            __m256i t1 = _mm256_unpacklo_epi16(y2, y3);
            __m256i t2 = _mm256_unpacklo_epi16(y4, y5);
            __m256i t3 = _mm256_unpacklo_epi16(y6, y7);

            __m256i u0 = _mm256_unpackhi_epi16(y0, y1);
            __m256i u1 = _mm256_unpackhi_epi16(y2, y3);
            __m256i u2 = _mm256_unpackhi_epi16(y4, y5);
            __m256i u3 = _mm256_unpackhi_epi16(y6, y7);

            // multiply by interleaved coefs
            t0 = _mm256_madd_epi16(t0, *(__m256i*)(coeffs +  0));
            t1 = _mm256_madd_epi16(t1, *(__m256i*)(coeffs + 32));
            t2 = _mm256_madd_epi16(t2, *(__m256i*)(coeffs + 64));
            t3 = _mm256_madd_epi16(t3, *(__m256i*)(coeffs + 96));
            t0 = _mm256_add_epi32(t0, t1);
            t0 = _mm256_add_epi32(t0, t2);
            t0 = _mm256_add_epi32(t0, t3);

            u0 = _mm256_madd_epi16(u0, *(__m256i*)(coeffs +  0));
            u1 = _mm256_madd_epi16(u1, *(__m256i*)(coeffs + 32));
            u2 = _mm256_madd_epi16(u2, *(__m256i*)(coeffs + 64));
            u3 = _mm256_madd_epi16(u3, *(__m256i*)(coeffs + 96));
            u0 = _mm256_add_epi32(u0, u1);
            u0 = _mm256_add_epi32(u0, u2);
            u0 = _mm256_add_epi32(u0, u3);

            // add offset and shift
            if (offset) {
                t0 = _mm256_add_epi32(t0, _mm256_set1_epi32(offset));
                u0 = _mm256_add_epi32(u0, _mm256_set1_epi32(offset));
            }
            t0 = _mm256_srai_epi32(t0, shift);
            u0 = _mm256_srai_epi32(u0, shift);

            // store 16 16-bit values
            t0 = _mm256_packs_epi32(t0, u0);
            t0 = _mm256_permute4x64_epi64(t0, _MM_SHUFFLE(3,1,2,0));
            _mm256_storeu_si256((__m256i*)pD, t0);

            // shift rows
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = y4;
            y4 = y5;
            y5 = y6;
            y6 = y7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 16;
        pDst += 16;
    }

    // process any 8-wide columns
    if (width & 0x8) {
        const short *pS = pSrc;
        short *pD = pDst;

        // load 8 16-bit pixels from rows 0-6
        __m256i y0 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 0*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y1 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 1*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y2 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 2*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y3 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 3*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y4 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 4*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y5 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 5*srcPitch)), _MM_SHUFFLE(3,1,2,0));
        __m256i y6 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 6*srcPitch)), _MM_SHUFFLE(3,1,2,0));

        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(8)
#endif
        for (int row = 0; row < height; row++) {

            __m256i y7 = _mm256_permute4x64_epi64(_mm256_loadu_si256((__m256i*)(pS + 7*srcPitch)), _MM_SHUFFLE(3,1,2,0));

            // interleave rows
            __m256i t0 = _mm256_unpacklo_epi16(y0, y1);
            __m256i t1 = _mm256_unpacklo_epi16(y2, y3);
            __m256i t2 = _mm256_unpacklo_epi16(y4, y5);
            __m256i t3 = _mm256_unpacklo_epi16(y6, y7);

            // multiply by interleaved coefs
            t0 = _mm256_madd_epi16(t0, *(__m256i*)(coeffs +  0));
            t1 = _mm256_madd_epi16(t1, *(__m256i*)(coeffs + 32));
            t2 = _mm256_madd_epi16(t2, *(__m256i*)(coeffs + 64));
            t3 = _mm256_madd_epi16(t3, *(__m256i*)(coeffs + 96));
            t0 = _mm256_add_epi32(t0, t1);
            t0 = _mm256_add_epi32(t0, t2);
            t0 = _mm256_add_epi32(t0, t3);

            // add offset and shift
            if (offset) {
                t0 = _mm256_add_epi32(t0, _mm256_set1_epi32(offset));
            }
            t0 = _mm256_srai_epi32(t0, shift);
            t0 = _mm256_packs_epi32(t0, t0);

            // store 8 16-bit values
            t0 = _mm256_permute4x64_epi64(t0, _MM_SHUFFLE(3,1,2,0));
            _mm_storeu_si128((__m128i*)pD, _mm256_castsi256_si128(t0));

            // shift rows
            y0 = y1;
            y1 = y2;
            y2 = y3;
            y3 = y4;
            y4 = y5;
            y5 = y6;
            y6 = y7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 8;
        pDst += 8;
    }

    // process any 4-wide columns
    if (width & 0x4) {
        const short *pS = pSrc;
        short *pD = pDst;

        // load 4 16-bit pixels from rows 0-6
        __m128i x0 = _mm_loadl_epi64((__m128i*)(pS + 0*srcPitch));
        __m128i x1 = _mm_loadl_epi64((__m128i*)(pS + 1*srcPitch));
        __m128i x2 = _mm_loadl_epi64((__m128i*)(pS + 2*srcPitch));
        __m128i x3 = _mm_loadl_epi64((__m128i*)(pS + 3*srcPitch));
        __m128i x4 = _mm_loadl_epi64((__m128i*)(pS + 4*srcPitch));
        __m128i x5 = _mm_loadl_epi64((__m128i*)(pS + 5*srcPitch));
        __m128i x6 = _mm_loadl_epi64((__m128i*)(pS + 6*srcPitch));
        
        // process rows vertically
#ifdef __INTEL_COMPILER
        #pragma unroll(4)
#endif
        for (int row = 0; row < height; row++) {

            __m128i x7 = _mm_loadl_epi64((__m128i*)(pS + 7*srcPitch));

            // interleave rows
            __m128i t0 = _mm_unpacklo_epi16(x0, x1);
            __m128i t1 = _mm_unpacklo_epi16(x2, x3);
            __m128i t2 = _mm_unpacklo_epi16(x4, x5);
            __m128i t3 = _mm_unpacklo_epi16(x6, x7);

            // multiply by interleaved coefs
            t0 = _mm_madd_epi16(t0, *(__m128i*)(coeffs +  0));
            t1 = _mm_madd_epi16(t1, *(__m128i*)(coeffs + 32));
            t2 = _mm_madd_epi16(t2, *(__m128i*)(coeffs + 64));
            t3 = _mm_madd_epi16(t3, *(__m128i*)(coeffs + 96));
            t0 = _mm_add_epi32(t0, t1);
            t0 = _mm_add_epi32(t0, t2);
            t0 = _mm_add_epi32(t0, t3);

            // add offset and shift
            if (offset) {
                t0 = _mm_add_epi32(t0, _mm_set1_epi32(offset));
            }
            t0 = _mm_srai_epi32(t0, shift);
            t0 = _mm_packs_epi32(t0, t0);

            // store 4 16-bit values
            _mm_storel_epi64((__m128i*)pD, t0);

            // shift rows
            x0 = x1;
            x1 = x2;
            x2 = x3;
            x3 = x4;
            x4 = x5;
            x5 = x6;
            x6 = x7;

            pS += srcPitch;
            pD += dstPitch;
        }
        pSrc += 4;
        pDst += 4;
    }
}

//kolya
//TODO: not tested, 10-bit 
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_V_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 8) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_loadu_si256((__m256i*)(pSrc + 0*srcPitch));
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0x10);

        ymm4 = _mm256_loadu_si256((__m256i*)(pSrc + 1*srcPitch));
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0x10);

        ymm1 = _mm256_loadu_si256((__m256i*)(pSrc + 2*srcPitch));
        ymm1 = _mm256_permute4x64_epi64(ymm1, 0x10);

        ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi16(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            /* load row 3 */
            ymm5 = _mm256_loadu_si256((__m256i*)(pSrc + 3*srcPitch));
            ymm5 = _mm256_permute4x64_epi64(ymm5, 0x10);

            ymm2 = _mm256_loadu_si256((__m256i*)(pSrc + 4*srcPitch));
            ymm2 = _mm256_permute4x64_epi64(ymm2, 0x10);

            ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi16(ymm5, ymm2);

            ymm0 = _mm256_madd_epi16(ymm0, ymm6);
            ymm4 = _mm256_madd_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi32(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(offset));
            ymm4 = _mm256_add_epi32(ymm4, _mm256_set1_epi32(offset));
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm4 = _mm256_srai_epi32(ymm4, shift);
            ymm4 = _mm256_packs_epi32(ymm4, ymm4);

            ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
            ymm4 = _mm256_permute4x64_epi64(ymm4, 0x08);

            _mm_storeu_si128((__m128i*)(pDst + 0*dstPitch), _mm256_castsi256_si128(ymm0));
            _mm_storeu_si128((__m128i*)(pDst + 1*dstPitch), _mm256_castsi256_si128(ymm4));

            /* shift interleaved row registers */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 8;
        pDstRef += 8;
    }
}

//kolya
template<int shift, int offset>
static void t_InterpLumaFast_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabLumaFast_S16[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* always calculates 4 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 4) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi16(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi16(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_madd_epi16(xmm0, xmm6);
            xmm4 = _mm_madd_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi32(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi32(xmm4, xmm3);
            
            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(offset));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);
            xmm4 = _mm_srai_epi32(xmm4, shift);
            xmm4 = _mm_packs_epi32(xmm4, xmm4);

            /* store 2 or 4 16-bit values */
            _mm_storel_epi64((__m128i*)(pDst + 0*dstPitch), xmm0);
            _mm_storel_epi64((__m128i*)(pDst + 1*dstPitch), xmm4);

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 4;
        pDstRef += 4;
    }
}

/* luma, vertical, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLuma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    switch (shift) {
    case 6:
        if (offset == 0)        t_InterpLuma_s16_d16_V_AVX2< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (offset == 32)  t_InterpLuma_s16_d16_V_AVX2< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        break;
    
    case 10:                    t_InterpLuma_s16_d16_V_AVX2<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 11:                    t_InterpLuma_s16_d16_V_AVX2<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 12:                    t_InterpLuma_s16_d16_V_AVX2<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
    case  1:                    t_InterpLuma_s16_d16_V_AVX2< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  2:                    t_InterpLuma_s16_d16_V_AVX2< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  0:                    t_InterpLuma_s16_d16_V_AVX2< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

//kolya
//not tested, just added for future 10-bit cases
/* luma short FIR, vertical, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpLumaFast_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x03) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpLumaFast_s16_d16_V_AVX2< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            else if (offset == 32)  t_InterpLumaFast_s16_d16_V_AVX2< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            break;
    
        case 10:                    t_InterpLumaFast_s16_d16_V_AVX2<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 11:                    t_InterpLumaFast_s16_d16_V_AVX2<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 12:                    t_InterpLumaFast_s16_d16_V_AVX2<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
        case  1:                    t_InterpLumaFast_s16_d16_V_AVX2< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  2:                    t_InterpLumaFast_s16_d16_V_AVX2< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  0:                    t_InterpLumaFast_s16_d16_V_AVX2< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        }

        return;
    }

    switch (shift) {
    case 6:
        if (offset == 0)        t_InterpLumaFast_s16_d16_V< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        else if (offset == 32)  t_InterpLumaFast_s16_d16_V< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
        break;
    
    case 10:                    t_InterpLumaFast_s16_d16_V<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 11:                    t_InterpLumaFast_s16_d16_V<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case 12:                    t_InterpLumaFast_s16_d16_V<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
    case  1:                    t_InterpLumaFast_s16_d16_V< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  2:                    t_InterpLumaFast_s16_d16_V< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    case  0:                    t_InterpLumaFast_s16_d16_V< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    }
}

/* chroma, vertical, 16-bit input, 16-bit output
 * AVX2 path used when width = multiple of 8
 */
template<int shift, int offset>
static void t_InterpChroma_s16_d16_V_AVX2(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5, ymm6, ymm7;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    _mm256_zeroupper();

    ymm6 = _mm256_load_si256((__m256i*)(coeffs +  0));
    ymm7 = _mm256_load_si256((__m256i*)(coeffs + 32));

    /* calculate 8 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += 8) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        ymm0 = _mm256_loadu_si256((__m256i*)(pSrc + 0*srcPitch));
        ymm0 = _mm256_permute4x64_epi64(ymm0, 0x10);

        ymm4 = _mm256_loadu_si256((__m256i*)(pSrc + 1*srcPitch));
        ymm4 = _mm256_permute4x64_epi64(ymm4, 0x10);

        ymm1 = _mm256_loadu_si256((__m256i*)(pSrc + 2*srcPitch));
        ymm1 = _mm256_permute4x64_epi64(ymm1, 0x10);

        ymm0 = _mm256_unpacklo_epi16(ymm0, ymm4);
        ymm4 = _mm256_unpacklo_epi16(ymm4, ymm1);

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            /* load row 3 */
            ymm5 = _mm256_loadu_si256((__m256i*)(pSrc + 3*srcPitch));
            ymm5 = _mm256_permute4x64_epi64(ymm5, 0x10);

            ymm2 = _mm256_loadu_si256((__m256i*)(pSrc + 4*srcPitch));
            ymm2 = _mm256_permute4x64_epi64(ymm2, 0x10);

            ymm1 = _mm256_unpacklo_epi16(ymm1, ymm5);
            ymm5 = _mm256_unpacklo_epi16(ymm5, ymm2);

            ymm0 = _mm256_madd_epi16(ymm0, ymm6);
            ymm4 = _mm256_madd_epi16(ymm4, ymm6);

            ymm3 = ymm1;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm0 = _mm256_add_epi32(ymm0, ymm3);

            ymm3 = ymm5;
            ymm3 = _mm256_madd_epi16(ymm3, ymm7);
            ymm4 = _mm256_add_epi32(ymm4, ymm3);

            /* add offset, shift off fraction bits */
            ymm0 = _mm256_add_epi32(ymm0, _mm256_set1_epi32(offset));
            ymm4 = _mm256_add_epi32(ymm4, _mm256_set1_epi32(offset));
            ymm0 = _mm256_srai_epi32(ymm0, shift);
            ymm0 = _mm256_packs_epi32(ymm0, ymm0);
            ymm4 = _mm256_srai_epi32(ymm4, shift);
            ymm4 = _mm256_packs_epi32(ymm4, ymm4);

            ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
            ymm4 = _mm256_permute4x64_epi64(ymm4, 0x08);

            _mm_storeu_si128((__m128i*)(pDst + 0*dstPitch), _mm256_castsi256_si128(ymm0));
            _mm_storeu_si128((__m128i*)(pDst + 1*dstPitch), _mm256_castsi256_si128(ymm4));

            /* shift interleaved row registers */
            ymm0 = ymm1;
            ymm4 = ymm5;
            ymm1 = ymm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += 8;
        pDstRef += 8;
    }
}

template<int widthMul, int shift, int offset>
static void t_InterpChroma_s16_d16_V(const short* pSrc, unsigned int srcPitch, short *pDst, unsigned int dstPitch, int tab_index, int width, int height)
{
    int row, col;
    const short *pSrcRef = pSrc;
    short *pDstRef = pDst;
    const signed char* coeffs;
    __m128i xmm0, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6, xmm7;

    coeffs = (const signed char *)filtTabChroma_S16[2 * (tab_index-1)];

    xmm6 = _mm_load_si128((__m128i *)(coeffs +  0));
    xmm7 = _mm_load_si128((__m128i *)(coeffs + 32));

    /* always calculates 4 outputs per inner loop, working vertically */
    for (col = 0; col < width; col += widthMul) {
        pSrc = pSrcRef;
        pDst = pDstRef;

        /* load 8 8-bit pixels from rows 0-2 */
        xmm0 = _mm_loadl_epi64((__m128i*)(pSrc + 0*srcPitch));
        xmm4 = _mm_loadl_epi64((__m128i*)(pSrc + 1*srcPitch));
        xmm1 = _mm_loadl_epi64((__m128i*)(pSrc + 2*srcPitch));

        xmm0 = _mm_unpacklo_epi16(xmm0, xmm4);    /* interleave 01 */
        xmm4 = _mm_unpacklo_epi16(xmm4, xmm1);    /* interleave 12 */

        /* enough registers to process two rows at a time (even and odd rows in parallel) */
        for (row = 0; row < height; row += 2) {
            xmm5 = _mm_loadl_epi64((__m128i*)(pSrc + 3*srcPitch));
            xmm2 = _mm_loadl_epi64((__m128i*)(pSrc + 4*srcPitch));
            xmm1 = _mm_unpacklo_epi16(xmm1, xmm5);    /* interleave 23 */
            xmm5 = _mm_unpacklo_epi16(xmm5, xmm2);    /* interleave 34 */

            xmm0 = _mm_madd_epi16(xmm0, xmm6);
            xmm4 = _mm_madd_epi16(xmm4, xmm6);

            xmm3 = xmm1;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm0 = _mm_add_epi32(xmm0, xmm3);

            xmm3 = xmm5;
            xmm3 = _mm_madd_epi16(xmm3, xmm7);
            xmm4 = _mm_add_epi32(xmm4, xmm3);
            
            /* add offset, shift off fraction bits, clip from 32 to 16 bits */
            xmm0 = _mm_add_epi32(xmm0, _mm_set1_epi32(offset));
            xmm4 = _mm_add_epi32(xmm4, _mm_set1_epi32(offset));
            xmm0 = _mm_srai_epi32(xmm0, shift);
            xmm0 = _mm_packs_epi32(xmm0, xmm0);
            xmm4 = _mm_srai_epi32(xmm4, shift);
            xmm4 = _mm_packs_epi32(xmm4, xmm4);

            /* store 2 or 4 16-bit values */
            if (widthMul == 4) {
                _mm_storel_epi64((__m128i*)(pDst + 0*dstPitch), xmm0);
                _mm_storel_epi64((__m128i*)(pDst + 1*dstPitch), xmm4);
            } else if (widthMul == 2) {
                *(int *)(pDst + 0*dstPitch) = _mm_cvtsi128_si32(xmm0);
                *(int *)(pDst + 1*dstPitch) = _mm_cvtsi128_si32(xmm4);
            }

            /* shift interleaved row registers */
            xmm0 = xmm1;
            xmm4 = xmm5;
            xmm1 = xmm2;

            pSrc += 2*srcPitch;
            pDst += 2*dstPitch;
        }
        pSrcRef += widthMul;
        pDstRef += widthMul;
    }
}

/* chroma, vertical, 16-bit input, 16-bit output */
void MAKE_NAME(h265_InterpChroma_s16_d16_V)(INTERP_S16_D16_PARAMETERS_LIST)
{
    int rem;

    if (shift == 6) {
        VM_ASSERT(offset == 0 || offset == 32);
    } else if (shift >= 10 && shift <= 12) {
        VM_ASSERT(offset == (1 << (shift-1)));
    } else if (shift <= 2) {
        VM_ASSERT(offset == 0);
    } else {
        VM_ASSERT(0);    /* error - not supported */
    }
    VM_ASSERT( ((width & 0x01) == 0) && ((height & 0x01) == 0) );

    /* fast path - width multiple of 8 */
    if ((width & 0x07) == 0) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpChroma_s16_d16_V_AVX2< 6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            else if (offset == 32)  t_InterpChroma_s16_d16_V_AVX2< 6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            break;
    
        case 10:                    t_InterpChroma_s16_d16_V_AVX2<10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 11:                    t_InterpChroma_s16_d16_V_AVX2<11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 12:                    t_InterpChroma_s16_d16_V_AVX2<12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
        case  1:                    t_InterpChroma_s16_d16_V_AVX2< 1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  2:                    t_InterpChroma_s16_d16_V_AVX2< 2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  0:                    t_InterpChroma_s16_d16_V_AVX2< 0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        }
        return;
    }

    rem = (width & 0x03);

    width -= rem;
    if (width > 0) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpChroma_s16_d16_V<4,  6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            else if (offset == 32)  t_InterpChroma_s16_d16_V<4,  6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);
            break;
    
        case 10:                    t_InterpChroma_s16_d16_V<4, 10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 11:                    t_InterpChroma_s16_d16_V<4, 11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case 12:                    t_InterpChroma_s16_d16_V<4, 12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
    
        case  1:                    t_InterpChroma_s16_d16_V<4,  1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  2:                    t_InterpChroma_s16_d16_V<4,  2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        case  0:                    t_InterpChroma_s16_d16_V<4,  0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, width, height);  break;
        }
        pSrc += width;
        pDst += width;
    }

    if (rem == 2) {
        switch (shift) {
        case 6:
            if (offset == 0)        t_InterpChroma_s16_d16_V<2,  6,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);
            else if (offset == 32)  t_InterpChroma_s16_d16_V<2,  6,   32>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);
            break;
    
        case 10:                    t_InterpChroma_s16_d16_V<2, 10,  512>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case 11:                    t_InterpChroma_s16_d16_V<2, 11, 1024>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case 12:                    t_InterpChroma_s16_d16_V<2, 12, 2048>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
    
        case  1:                    t_InterpChroma_s16_d16_V<2,  1,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case  2:                    t_InterpChroma_s16_d16_V<2,  2,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        case  0:                    t_InterpChroma_s16_d16_V<2,  0,    0>(pSrc, srcPitch, pDst, dstPitch, tab_index, 2, height);  break;
        }
    }
}

/* single kernel for all 3 averaging modes 
 * template parameter avgBytes = [0,1,2] determines mode at compile time (no branches in inner loop)
 */
template <int widthMul, int avgBytes>
static void t_AverageMode(short *pSrc, unsigned int srcPitch, void *vpAvg, unsigned int avgPitch, unsigned char *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    unsigned char *pAvgC;
    short *pAvgS;
    __m128i xmm0, xmm1, xmm7;
    __m256i ymm0, ymm1, ymm7;

    _mm256_zeroupper();

    if (avgBytes == 1)
        pAvgC = (unsigned char *)vpAvg;
    else if (avgBytes == 2)
        pAvgS = (short *)vpAvg;

    do {
        if (widthMul == 16) {
            /* multiple of 16 */
            ymm7 = _mm256_set1_epi16(1 << 6);
            for (col = 0; col < width; col += widthMul) {
                /* load 16 16-bit pixels from source */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + col));

                if (avgBytes == 1) {
                    /* load 16 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvgC + col));
                    ymm1 = _mm256_cvtepu8_epi16(xmm1);
                    ymm1 = _mm256_slli_epi16(ymm1, 6);

                    ymm1 = _mm256_adds_epi16(ymm1, ymm0);
                    ymm0 = _mm256_adds_epi16(ymm7, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, 7);
                } else if (avgBytes == 2) {
                    /* load 16 16-bit pixels from from avg buffer */
                    ymm1 = _mm256_loadu_si256((__m256i *)(pAvgS + col));

                    ymm1 = _mm256_adds_epi16(ymm1, ymm0);
                    ymm0 = _mm256_adds_epi16(ymm7, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, 7);
                }
                
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0x08);
                _mm_storeu_si128((__m128i*)(pDst + col), _mm256_castsi256_si128(ymm0));
            }

        } else {
            /* multiple of 8 or less */
            xmm7 = _mm_set1_epi16(1 << 6);
            for (col = 0; col < width; col += widthMul) {
                /* load 8 16-bit pixels from source */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
                
                if (avgBytes == 1) {
                    /* load 8 8-bit pixels from avg buffer, zero extend to 16-bit, normalize fraction bits */
                    xmm1 = _mm_cvtepu8_epi16(MM_LOAD_EPI64(pAvgC + col));    
                    xmm1 = _mm_slli_epi16(xmm1, 6);

                    xmm1 = _mm_adds_epi16(xmm1, xmm0);
                    xmm0 = _mm_adds_epi16(xmm7, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, 7);
                } else if (avgBytes == 2) {
                    /* load 8 16-bit pixels from from avg buffer */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvgS + col));

                    xmm1 = _mm_adds_epi16(xmm1, xmm0);
                    xmm0 = _mm_adds_epi16(xmm7, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, 7);
                }

                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                /* store 8 pixels */
                if (widthMul == 8) {
                    _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
                } else if (widthMul == 6) {
                    *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                    *(short *)(pDst+col+2) = (short)_mm_extract_epi16(xmm0, 1);
                    *(short *)(pDst+col+4) = (short)_mm_extract_epi16(xmm0, 2);
                } else if (widthMul == 4) {
                    *(int   *)(pDst+col+0) = _mm_cvtsi128_si32(xmm0);
                } else if (widthMul == 2) {
                    *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                }
            }
        }

        pSrc += srcPitch;
        pDst += dstPitch;

        if (avgBytes == 1)
            pAvgC += avgPitch;
        else if (avgBytes == 2)
            pAvgS += avgPitch;
    } while (--height);
}
    
/* mode: AVERAGE_NO, just clip/pack 16-bit output to 8-bit */
void MAKE_NAME(h265_AverageModeN)(INTERP_AVG_NONE_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, 0>(pSrc,   srcPitch, 0, 0, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, 0>(pSrc+8, srcPitch, 0, 0, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 6, height); 
        return;
    }
}

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeP)(INTERP_AVG_PIC_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, sizeof(unsigned char)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, sizeof(unsigned char)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, sizeof(unsigned char)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}


/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeB)(INTERP_AVG_BUF_PARAMETERS_LIST)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode<16, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode< 8, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode< 4, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height); 
        return;
    case 12: 
        t_AverageMode< 8, sizeof(short)>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height); 
        t_AverageMode< 4, sizeof(short)>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height); 
        return;
    case  2: 
        t_AverageMode< 2, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height); 
        return;
    case  6: 
        t_AverageMode< 6, sizeof(short)>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height); 
        return;
    }
}

/* single kernel for all 3 averaging modes 
 * template parameter avgMode = [0,1,2] determines mode at compile time (no branches in inner loop)
 */
template <int widthMul, int avgMode, int bitDepth>
static void t_AverageMode_U16_Kernel(short *pSrc, unsigned int srcPitch, unsigned short *pAvg, unsigned int avgPitch, unsigned short *pDst, unsigned int dstPitch, int width, int height)
{
    int col;
    const int shift = 15 - bitDepth;
    __m128i xmm0, xmm1, xmm4, xmm5, xmm7;
    __m256i ymm0, ymm1, ymm4, ymm5, ymm7;

    _mm256_zeroupper();

    if (widthMul == 16) {
        ymm4 = _mm256_setzero_si256();                   /* min */
        ymm5 = _mm256_set1_epi16((1 << bitDepth) - 1);   /* max */
        ymm7 = _mm256_set1_epi16(1 << (shift - 1));      /* round */
    } else {
        xmm4 = _mm_setzero_si128();                   /* min */
        xmm5 = _mm_set1_epi16((1 << bitDepth) - 1);   /* max */
        xmm7 = _mm_set1_epi16(1 << (shift - 1));      /* round */
    }

    do {
        if (widthMul == 16) {
            /* multiple of 16 */
            for (col = 0; col < width; col += widthMul) {
                /* load 16 16-bit pixels from source */
                ymm0 = _mm256_loadu_si256((__m256i *)(pSrc + col));

                if (avgMode == 1) {
                    /* mode P: load 16 16-bit pixels from avg buffer, normalize and add */
                    ymm1 = _mm256_loadu_si256((__m256i *)(pAvg + col));
                    ymm1 = _mm256_slli_epi16(ymm1, shift - 1);
                    ymm1 = _mm256_adds_epi16(ymm1, ymm0);
                    ymm0 = _mm256_adds_epi16(ymm7, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, shift);
                } else if (avgMode == 2) {
                    /* mode B: load 16 16-bit pixels from from avg buffer */
                    ymm1 = _mm256_loadu_si256((__m256i *)(pAvg + col));
                    ymm1 = _mm256_adds_epi16(ymm1, ymm0);
                    ymm0 = _mm256_adds_epi16(ymm7, ymm1);
                    ymm0 = _mm256_srai_epi16(ymm0, shift);
                }
                ymm0 = _mm256_max_epi16(ymm0, ymm4); 
                ymm0 = _mm256_min_epi16(ymm0, ymm5);
                _mm256_storeu_si256((__m256i*)(pDst + col), ymm0);
            }
        } else {
            /* multiple of 8 or less */
            for (col = 0; col < width; col += widthMul) {
                /* load 8 16-bit pixels from source */
                xmm0 = _mm_loadu_si128((__m128i *)(pSrc + col));
                
                if (avgMode == 1) {
                    /* mode P: load 8 16-bit pixels from avg buffer, normalize and add */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvg + col));
                    xmm1 = _mm_slli_epi16(xmm1, shift - 1);
                    xmm1 = _mm_adds_epi16(xmm1, xmm0);
                    xmm0 = _mm_adds_epi16(xmm7, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, shift);
                } else if (avgMode == 2) {
                    /* mode B: load 8 16-bit pixels from from avg buffer */
                    xmm1 = _mm_loadu_si128((__m128i *)(pAvg + col));
                    xmm1 = _mm_adds_epi16(xmm1, xmm0);
                    xmm0 = _mm_adds_epi16(xmm7, xmm1);
                    xmm0 = _mm_srai_epi16(xmm0, shift);
                }
                xmm0 = _mm_max_epi16(xmm0, xmm4); 
                xmm0 = _mm_min_epi16(xmm0, xmm5);

                /* store 2,4,6,8 16-bit pixels */
                if (widthMul == 8) {
                    _mm_storeu_si128((__m128i*)(pDst + col), xmm0);
                } else if (widthMul == 6) {
                    _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
                    *(short *)(pDst+col+4) = (short)_mm_extract_epi16(xmm0, 4);
                    *(short *)(pDst+col+5) = (short)_mm_extract_epi16(xmm0, 5);
                } else if (widthMul == 4) {
                    _mm_storel_epi64((__m128i*)(pDst + col), xmm0);
                } else if (widthMul == 2) {
                    *(short *)(pDst+col+0) = (short)_mm_extract_epi16(xmm0, 0);
                    *(short *)(pDst+col+1) = (short)_mm_extract_epi16(xmm0, 1);
                }
            }
        }
        pSrc += srcPitch;
        pDst += dstPitch;
        pAvg += avgPitch;

    } while (--height);
}

/* wrapper to make bit depth const, allow >> by immediate in kernel (faster) */
template <int widthMul, int avgMode>
static void t_AverageMode_U16(short *pSrc, unsigned int srcPitch, unsigned short *pAvg, unsigned int avgPitch, unsigned short *pDst, unsigned int dstPitch, int width, int height, int bitDepth)
{
    if (bitDepth == 9)
        t_AverageMode_U16_Kernel<widthMul, avgMode,  9>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 10)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 10>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 8)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 8>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 11)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 11>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else if (bitDepth == 12)
        t_AverageMode_U16_Kernel<widthMul, avgMode, 12>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height);
    else
        VM_ASSERT(false);

}

/* mode: AVERAGE_NO, just clip/pack 16-bit output to [0, 2^bitDepth) */
void MAKE_NAME(h265_AverageModeN_U16)(INTERP_AVG_NONE_PARAMETERS_LIST_U16)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode_U16<16, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height, bit_depth);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 0>(pSrc,   srcPitch, 0, 0, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 0>(pSrc+8, srcPitch, 0, 0, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 0>(pSrc, srcPitch, 0, 0, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}

/* mode: AVERAGE_FROM_PIC, load 8-bit pixels, extend to 16-bit, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeP_U16)(INTERP_AVG_PIC_PARAMETERS_LIST_U16)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode_U16<16, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 1>(pSrc,   srcPitch, pAvg,   avgPitch, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 1>(pSrc+8, srcPitch, pAvg+8, avgPitch, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 1>(pSrc, srcPitch, pAvg, avgPitch, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}


/* mode: AVERAGE_FROM_BUF, load 16-bit pixels, add to current output, clip/pack 16-bit to 8-bit */
void MAKE_NAME(h265_AverageModeB_U16)(INTERP_AVG_BUF_PARAMETERS_LIST_U16)
{
    if ( (width & 0x0f) == 0 ) {
        /* very fast path - multiple of 16 */
        t_AverageMode_U16<16, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    } else if ( (width & 0x07) == 0 ) {
        /* fast path - multiple of 8 */
        t_AverageMode_U16<8, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, width, height, bit_depth);
        return;
    }

    switch (width) {
    case  4: 
        t_AverageMode_U16<4, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 4, height, bit_depth); 
        return;
    case 12: 
        t_AverageMode_U16<8, 2>(pSrc,   srcPitch, (unsigned short *)pAvg,   avgPitch, pDst,   dstPitch, 8, height, bit_depth); 
        t_AverageMode_U16<4, 2>(pSrc+8, srcPitch, (unsigned short *)pAvg+8, avgPitch, pDst+8, dstPitch, 4, height, bit_depth); 
        return;
    case  2: 
        t_AverageMode_U16<2, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 2, height, bit_depth); 
        return;
    case  6: 
        t_AverageMode_U16<6, 2>(pSrc, srcPitch, (unsigned short *)pAvg, avgPitch, pDst, dstPitch, 6, height, bit_depth); 
        return;
    }
}

template <int shift, typename PixType>
static void h265_InterpLumaPack_kernel(const short *src, int pitchSrc, PixType *dst, int pitchDst, int width, int height, int bitDepth)
{
    width = (width + 15) & ~15; // 256-bit registers
    //VM_ASSERT(width <= pitchSrc);
    //VM_ASSERT(width <= pitchDst);

    __m256i line, minPel, maxPel;
    __m256i offset = _mm256_set1_epi16(1 << (shift - 1));

    if (sizeof(PixType) == 2) {
        minPel = _mm256_setzero_si256();
        maxPel = _mm256_set1_epi16((1 << bitDepth) - 1);
    }

    for (; height > 0; height--, src += pitchSrc, dst += pitchDst) {
        for (Ipp32s col = 0; col < width; col += 16) {
            line = _mm256_loadu_si256((__m256i *)(src + col));

            line = _mm256_add_epi16(line, offset);
            line = _mm256_srai_epi16(line, shift);

            if (sizeof(PixType) == 1) {
                line = _mm256_packus_epi16(line, line);
                line = _mm256_permute4x64_epi64(line, 0x08);
                _mm_storeu_si128((__m128i*)(dst + col), _mm256_castsi256_si128(line));
            }
            else {
                line = _mm256_max_epi16(line, minPel); 
                line = _mm256_min_epi16(line, maxPel);
                _mm256_storeu_si256((__m256i*)(dst + col), line);
            }
        }
    }
}

// pack intermediate interpolation pels s16 to u8/u16 [ dst = Saturate( (src + 32) >> 6, 0, (1 << bitDepth) - 1 ) ]
template <typename PixType>
void MAKE_NAME(h265_InterpLumaPack)(const short *src, int pitchSrc, PixType *dst, int pitchDst, int width, int height, int bitDepth)
{
    switch (bitDepth) {
    case  8: h265_InterpLumaPack_kernel<6>(src, pitchSrc, dst, pitchDst, width, height,  8); break; // (+32) >> 6
    case  9: h265_InterpLumaPack_kernel<5>(src, pitchSrc, dst, pitchDst, width, height,  9); break; // (+16) >> 5
    case 10: h265_InterpLumaPack_kernel<4>(src, pitchSrc, dst, pitchDst, width, height, 10); break; // ( +8) >> 4
    default: VM_ASSERT(!"unexpected shift");
    }
}
template void MAKE_NAME(h265_InterpLumaPack)<unsigned char >(const short*, int, unsigned char  *dst, int, int, int, int);
template void MAKE_NAME(h265_InterpLumaPack)<unsigned short>(const short*, int, unsigned short *dst, int, int, int, int);

void MAKE_NAME(h265_ConvertShiftR)(const short *src, int pitchSrc, unsigned char *dst, int pitchDst, int width, int height, int rshift)
{
    width = (width + 15) & ~15; // 256-bit registers
    VM_ASSERT(width <= pitchSrc);
    VM_ASSERT(width <= pitchDst);

    __m256i line;
    __m256i offset = _mm256_set1_epi16(1 << (rshift - 1));

    for (; height > 0; height--, src += pitchSrc, dst += pitchDst) {
        for (Ipp32s col = 0; col < width; col += 16) {
            line = _mm256_loadu_si256((__m256i *)(src + col));

            line = _mm256_add_epi16(line, offset);
            line = _mm256_srai_epi16(line, rshift);

            line = _mm256_packus_epi16(line, line);
            line = _mm256_permute4x64_epi64(line, 0x08);
            _mm_storeu_si128((__m128i*)(dst + col), _mm256_castsi256_si128(line));
        }
    }
}

} // end namespace MFX_HEVC_PP

#endif //#if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
