//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include <string.h>
#include "umc_video_decoder.h"

namespace UMC
{

VideoDecoderParams::VideoDecoderParams(void)
{
    m_pData = NULL;
    memset(&info, 0, sizeof(sVideoStreamInfo));
    lFlags = 0;
    pPostProcessing = NULL;
    lpMemoryAllocator = NULL;
    pVideoAccelerator = NULL;
} // VideoDecoderParams::VideoDecoderParams(void)

VideoDecoderParams::~VideoDecoderParams(void)
{

} // VideoDecoderParams::~VideoDecoderParams(void)

VideoDecoder::~VideoDecoder(void)
{
  if (m_allocatedPostProcessing) {
    delete m_allocatedPostProcessing;
    m_allocatedPostProcessing = NULL;
  }
} // VideoDecoder::~VideoDecoder(void)

Status VideoDecoder::GetInfo(BaseCodecParams *info)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams> (info);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    pParams->info = m_ClipInfo;

    return umcRes;

} // Status VideoDecoder::GetInfo(BaseCodecParams *info)

Status VideoDecoder::SetParams(BaseCodecParams* params)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams>(params);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    m_ClipInfo = pParams->info;

    return umcRes;

} // Status VideoDecoder::SetParams(BaseCodecParams* params)

} // end namespace UMC
