//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_IPP_LEVEL_H
#define __UMC_H264_DEC_IPP_LEVEL_H

#include "ippvc.h"

namespace UMC_H264_DECODER
{

extern void ConvertNV12ToYV12(const uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint8_t *pSrcDstUPlane, uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi);
extern void ConvertYV12ToNV12(const uint8_t *pSrcDstUPlane, const uint8_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint8_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi);

extern void ConvertNV12ToYV12(const uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, uint16_t *pSrcDstUPlane, uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, mfxSize roi);
extern void ConvertYV12ToNV12(const uint16_t *pSrcDstUPlane, const uint16_t *pSrcDstVPlane, const uint32_t _srcdstUStep, uint16_t *pSrcDstUVPlane, const uint32_t _srcdstUVStep, mfxSize roi);

#define   IPPFUN(type,name,arg)   extern type __STDCALL name arg

IPPAPI(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (uint32_t **ppBitStream,
    int32_t *pOffset,
    int16_t *pNumCoeff,
    int16_t **ppPosCoefbuf,
    uint32_t uVLCSelect,
    int16_t uMaxNumCoeff,
    const int32_t **ppTblCoeffToken,
    const int32_t **ppTblTotalZeros,
    const int32_t **ppTblRunBefore,
    const int32_t *pScanMatrix,
    int32_t scanIdxStart,
    int32_t scanIdxEnd))


}; // UMC_H264_DECODER

using namespace UMC_H264_DECODER;

#endif // __UMC_H264_DEC_IPP_LEVEL_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
