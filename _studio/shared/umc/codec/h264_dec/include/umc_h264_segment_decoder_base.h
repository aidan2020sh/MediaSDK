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

#ifndef __UMC_H264_SEGMENT_DECODER_BASE_H
#define __UMC_H264_SEGMENT_DECODER_BASE_H

#include "umc_h264_dec.h"

namespace UMC
{

class H264Slice;
class TaskBroker;

class H264SegmentDecoderBase
{
public:
    H264SegmentDecoderBase(TaskBroker * pTaskBroker)
        : m_iNumber(0)
        , m_pTaskBroker(pTaskBroker)
    {
    }

    virtual ~H264SegmentDecoderBase()
    {
    }

    virtual UMC::Status Init(int32_t iNumber)
    {
        m_iNumber = iNumber;
        return UMC::UMC_OK;
    }

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual void RestoreErrorRect(int32_t , int32_t , H264Slice * )
    {
    }

protected:
    int32_t m_iNumber;                                           // (int32_t) ordinal number of decoder
    TaskBroker * m_pTaskBroker;
};

} // namespace UMC


#endif /* __UMC_H264_SEGMENT_DECODER_BASE_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
