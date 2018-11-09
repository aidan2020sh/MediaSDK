//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_DEFS_YUV_H__
#define __UMC_H265_DEC_DEFS_YUV_H__

#include "umc_h265_dec_defs.h"
#include "umc_video_decoder.h"
#include "umc_frame_data.h"

namespace UMC_HEVC_DECODER
{

// YUV frame buffer class
class H265DecYUVBufferPadded
{
public:

    int32_t  m_chroma_format; // standard value chroma_format_idc

    PlanePtrY               m_pYPlane;

    PlanePtrUV              m_pUVPlane;  // for NV12 support

    PlanePtrUV              m_pUPlane;
    PlanePtrUV              m_pVPlane;

    H265DecYUVBufferPadded();
    H265DecYUVBufferPadded(UMC::MemoryAllocator *pMemoryAllocator);
    virtual ~H265DecYUVBufferPadded();

    // Initialize variables to default values
    void Init(const UMC::VideoDataInfo *info);

    // Allocate YUV frame buffer planes and initialize pointers to it.
    // Used to contain decoded frames.
    void allocate(const UMC::FrameData * frameData, const UMC::VideoDataInfo *info);

    // Allocate memory and initialize frame plane pointers and pitches.
    // Used for temporary picture buffers, e.g. residuals.
    void createPredictionBuffer(const H265SeqParamSet * sps);
    // Deallocate planes memory
    void destroy();

    // Deallocate all memory
    void deallocate();

    const mfxSize& lumaSize() const { return m_lumaSize; }
    const mfxSize& chromaSize() const { return m_chromaSize; }

    uint32_t pitch_luma() const { return m_pitch_luma; }
    uint32_t pitch_chroma() const { return m_pitch_chroma; }

    // Returns pointer to FrameData instance
    UMC::FrameData * GetFrameData();
    const UMC::FrameData * GetFrameData() const;

    // Returns color formap of allocated frame
    UMC::ColorFormat GetColorFormat() const;

protected:
    UMC::MemoryAllocator *m_pMemoryAllocator;                        // (MemoryAllocator *) pointer to memory allocator
    UMC::MemID m_midAllocatedBuffer;                                 // (MemID) mem id for allocated buffer

    // m_pAllocatedBuffer contains the pointer returned when
    // we allocated space for the data.
    uint8_t                 *m_pAllocatedBuffer;

    mfxSize            m_lumaSize;
    mfxSize            m_chromaSize;

    int32_t  m_pitch_luma;
    int32_t  m_pitch_chroma;

    UMC::FrameData m_frameData;

    UMC::ColorFormat m_color_format;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_DEC_DEFS_YUV_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
