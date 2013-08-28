/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_AUTO)

namespace MFX_HEVC_PP
{
/* ******************************************************** */
/*                    List of All functions                 */
/* ******************************************************** */
    //[SAD]
    #define SAD_PARAMETERS_LIST_SPECIAL const unsigned char *image,  const unsigned char *block, int img_stride
    // [SSE4]
    int H265_FASTCALL SAD_4x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_sse(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_sse(SAD_PARAMETERS_LIST_SPECIAL);

    // [AVX2]
    int H265_FASTCALL SAD_4x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_4x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_8x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_8x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_12x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_16x4_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x12_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_16x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_24x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_32x8_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x24_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_32x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_48x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    int H265_FASTCALL SAD_64x16_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x32_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x48_avx2(SAD_PARAMETERS_LIST_SPECIAL);
    int H265_FASTCALL SAD_64x64_avx2(SAD_PARAMETERS_LIST_SPECIAL);

    
    #define SAD_PARAMETERS_LIST_GENERAL const unsigned char *image,  const unsigned char *block, int img_stride, int block_stride
    //[SSE4. General]
    int H265_FASTCALL SAD_4x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_sse(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_sse(SAD_PARAMETERS_LIST_GENERAL);

    // [AVX2.General]
    int H265_FASTCALL SAD_4x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_4x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_8x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_8x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_12x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_16x4_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x12_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_16x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_24x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_32x8_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x24_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_32x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_48x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    int H265_FASTCALL SAD_64x16_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x32_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x48_general_avx2(SAD_PARAMETERS_LIST_GENERAL);
    int H265_FASTCALL SAD_64x64_general_avx2(SAD_PARAMETERS_LIST_GENERAL);

    // [transform.forward]
    void H265_FASTCALL h265_DST4x4Fwd_16s_px(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_px(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_px(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_px(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_px(const short *H265_RESTRICT src, short *H265_RESTRICT dest);

    void H265_FASTCALL h265_DST4x4Fwd_16s_sse(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT4x4Fwd_16s_sse(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT8x8Fwd_16s_sse(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT16x16Fwd_16s_sse(const short *H265_RESTRICT src, short *H265_RESTRICT dst);
    void H265_FASTCALL h265_DCT32x32Fwd_16s_sse(const short *H265_RESTRICT src, short *H265_RESTRICT dest);

    // [transform.inv]
    void h265_DST4x4Inv_16sT_px  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT4x4Inv_16sT_px  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT8x8Inv_16sT_px  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT16x16Inv_16sT_px(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT32x32Inv_16sT_px(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);

    void h265_DST4x4Inv_16sT_sse  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT4x4Inv_16sT_sse  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT8x8Inv_16sT_sse  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT16x16Inv_16sT_sse(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT32x32Inv_16sT_sse(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);

    void h265_DST4x4Inv_16sT_avx2  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT4x4Inv_16sT_avx2  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT8x8Inv_16sT_avx2  (void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT16x16Inv_16sT_avx2(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);
    void h265_DCT32x32Inv_16sT_avx2(void *destPtr, const short *H265_RESTRICT coeff, int destStride, int destSize);

    // [deblocking]
    Ipp32s h265_FilterEdgeLuma_8u_I_sse(H265EdgeData *edge, Ipp8u *srcDst, Ipp32s srcDstStride, Ipp32s dir);

    // [SAO]
    void h265_ProcessSaoCuOrg_Luma_8u_sse(
        Ipp8u* pRec,
        Ipp32s stride,
        Ipp32s saoType, 
        Ipp8u* tmpL,
        Ipp8u* tmpU,
        Ipp32u maxCUWidth,
        Ipp32u maxCUHeight,
        Ipp32s picWidth,
        Ipp32s picHeight,
        Ipp32s* pOffsetEo,
        Ipp8u* pOffsetBo,
        Ipp8u* pClipTable,
        Ipp32u CUPelX,
        Ipp32u CUPelY);

    void h265_ProcessSaoCu_Luma_8u_sse(
        Ipp8u* pRec,
        Ipp32s stride,
        Ipp32s saoType, 
        Ipp8u* tmpL,
        Ipp8u* tmpU,
        Ipp32u maxCUWidth,
        Ipp32u maxCUHeight,
        Ipp32s picWidth,
        Ipp32s picHeight,
        Ipp32s* pOffsetEo,
        Ipp8u* pOffsetBo,
        Ipp8u* pClipTable,
        Ipp32u CUPelX,
        Ipp32u CUPelY,
        bool* pbBorderAvail);

    void h265_PredictIntra_Ang_8u_sse(
        Ipp32s mode,
        Ipp8u* PredPel,
        Ipp8u* pels,
        Ipp32s pitch,
        Ipp32s width);

}; // namespace MFX_HEVC_PP

#endif // #if defined (MFX_TARGET_OPTIMIZATION_AUTO)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined (MFX_ENABLE_H265_VIDEO_DECODE)
/* EOF */
