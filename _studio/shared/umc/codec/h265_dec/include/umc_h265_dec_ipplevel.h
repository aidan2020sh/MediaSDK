/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#ifndef __UMC_H265_DEC_IPP_LEVEL_H
#define __UMC_H265_DEC_IPP_LEVEL_H

#include "ippvc.h"
#include "memory.h"
#include <sys/types.h> /* for size_t declaration on Android */
#include <string.h> /* for memset and memcpy on Android */

namespace UMC_HEVC_DECODER
{
#define IPP_UNREFERENCED_PARAMETER(p) (p)=(p)
#define   IPPFUN(type,name,arg)   extern type __CDECL name arg

 /* declare struct-parameter */
typedef struct H264InterpolationParams_8u
{
    const Ipp8u *pSrc;                                          /* (const Ipp8u *) pointer to the source memory */
    /*SIZE_T*/ size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp8u *pDst;                                                /* (Ipp8u *) pointer to the destination memory */
    /*SIZE_T*/ size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    const Ipp8u *pSrcComplementary;                             /* (const Ipp8u *) pointer to the complementary source memory */
    Ipp8u *pDstComplementary;                                   /* (Ipp8u *) pointer to the complementary destination memory */

} H264InterpolationParams_8u;

typedef struct H264InterpolationParams_16u
{
    const Ipp16u *pSrc;                                          /* (const Ipp8u *) pointer to the source memory */
    /*SIZE_T*/ size_t srcStep;                                             /* (SIZE_T) pitch of the source memory */
    Ipp16u *pDst;                                                /* (Ipp8u *) pointer to the destination memory */
    /*SIZE_T*/ size_t dstStep;                                             /* (SIZE_T) pitch of the destination memory */

    Ipp32s hFraction;                                           /* (Ipp32s) horizontal fraction of interpolation */
    Ipp32s vFraction;                                           /* (Ipp32s) vertical fraction of interpolation */

    Ipp32s blockWidth;                                          /* (Ipp32s) width of destination block */
    Ipp32s blockHeight;                                         /* (Ipp32s) height of destination block */

    Ipp32s iType;                                               /* (Ipp32s) type of interpolation */

    Ipp32s xPos;                                                /* (Ipp32s) x coordinate of source data block */
    Ipp32s yPos;                                                /* (Ipp32s) y coordinate of source data block */
    Ipp32s dataWidth;                                           /* (Ipp32s) width of the used source data */
    Ipp32s dataHeight;                                          /* (Ipp32s) height of the used source data */

    IppiSize frameSize;                                         /* (IppiSize) frame size */

    const Ipp16u *pSrcComplementary;                             /* (const Ipp8u *) pointer to the complementary source memory */
    Ipp16u *pDstComplementary;                                   /* (Ipp8u *) pointer to the complementary destination memory */

} H264InterpolationParams_16u;

IPPFUN(IppStatus, ippiInterpolateLumaBlock_H265, (IppVCInterpolateBlock_8u *interpolateInfo, Ipp8u *temporary_buffer));
IPPFUN(IppStatus, ippiInterpolateChromaBlock_H264, (IppVCInterpolateBlock_8u *interpolateInfo, Ipp8u *temporary_buffer));

IPPFUN(IppStatus, ippiInterpolateLumaBlock_H265, (IppVCInterpolateBlock_8u *interpolateInfo, Ipp16u *temporary_buffer));
IPPFUN(IppStatus, ippiInterpolateChromaBlock_H264, (IppVCInterpolateBlock_8u *interpolateInfo, Ipp16u *temporary_buffer));

} /* namespace UMC_H265_DECODER */

using namespace UMC_HEVC_DECODER;

#endif // __UMC_H265_DEC_IPP_LEVEL_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
