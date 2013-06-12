/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_dec_defs_yuv.h"

namespace UMC
{

H264DecYUVBufferPadded::H264DecYUVBufferPadded()
    : m_pYPlane(0)
    , m_pUVPlane(0)
    , m_pUPlane(0)
    , m_pVPlane(0)

    , m_pYPlane_base(0)
    , m_pUVPlane_base(0)
    , m_pUPlane_base(0)
    , m_pVPlane_base(0)

    , m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(NV12)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
}

H264DecYUVBufferPadded::~H264DecYUVBufferPadded()
{
    deallocate();
}

const FrameData * H264DecYUVBufferPadded::GetFrameData() const
{
    return &m_frameData;
}

const FrameData * H264DecYUVBufferPadded::GetFrameDataOFBaseRefPic() const
{
    if (m_frameDataRefBase.GetFrameMID() == FRAME_MID_INVALID)
        return 0;

    return &m_frameDataRefBase;
}

void H264DecYUVBufferPadded::deallocate()
{
    if (m_frameData.GetFrameMID() != FRAME_MID_INVALID)
    {
        m_frameData.Close();
        return;
    }

    if (m_frameDataRefBase.GetFrameMID() != FRAME_MID_INVALID)
    {
        m_frameDataRefBase.Close();
        return;
    }

    m_pYPlane = m_pUPlane = m_pVPlane = m_pUVPlane = 0;
    m_pYPlane_base = m_pUPlane_base = m_pVPlane_base = m_pUVPlane_base = 0;

    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
    m_pitch_luma = 0;
    m_pitch_chroma = 0;
}

void H264DecYUVBufferPadded::Init(const VideoDataInfo *info)
{
    VM_ASSERT(info);

    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_color_format = info->GetColorFormat();
    m_chroma_format = GetH264ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pYPlane = 0;
    m_pUPlane = 0;
    m_pVPlane = 0;
    m_pUVPlane = 0;

    if (m_chroma_format > 0)
    {
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
    }
    else
    {
        m_chromaSize.width = 0;
        m_chromaSize.height = 0;
    }
}

void H264DecYUVBufferPadded::allocate(const FrameData * frameData, const VideoDataInfo *info)
{
    VM_ASSERT(info);
    VM_ASSERT(frameData);

    m_frameData = *frameData;

    if (frameData->GetPlaneMemoryInfo(0)->m_planePtr)
        m_frameData.m_locked = true;

    m_color_format = info->GetColorFormat();
    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_chroma_format = GetH264ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pitch_luma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(0)->m_pitch / info->GetPlaneInfo(0)->m_iSampleSize;

    m_pYPlane = m_frameData.GetPlaneMemoryInfo(0)->m_planePtr;

    if (m_chroma_format > 0 || GetH264ColorFormat(frameData->GetInfo()->GetColorFormat()) > 0)
    {
        if (m_chroma_format == 0)
            info = frameData->GetInfo();
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
        m_pitch_chroma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(1)->m_pitch / info->GetPlaneInfo(1)->m_iSampleSize;

        if (m_frameData.GetInfo()->GetColorFormat() == UMC::NV12)
        {
            m_pUVPlane = m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;
            m_pUPlane = 0;
            m_pVPlane = 0;
        }
        else
        {
            m_pUPlane = m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;
            m_pVPlane = m_frameData.GetPlaneMemoryInfo(2)->m_planePtr;
            m_pUVPlane = 0;
        }
    }
    else
    {
        m_chromaSize.width = 0;
        m_chromaSize.height = 0;
        m_pitch_chroma = 0;
        m_pUPlane = 0;
        m_pVPlane = 0;
    }
}

void H264DecYUVBufferPadded::allocateRefBasePicture()
{
    m_frameDataRefBase = m_frameData;

    if (m_frameDataRefBase.GetPlaneMemoryInfo(0)->m_planePtr)
        m_frameDataRefBase.m_locked = true;

    m_frameData.m_locked = false;
    m_frameData.Close();

    m_pYPlane_base = m_pYPlane;
    m_pUVPlane_base = m_pUVPlane;
    m_pUPlane_base = m_pUPlane;
    m_pVPlane_base = m_pVPlane;
}

ColorFormat H264DecYUVBufferPadded::GetColorFormat() const
{
    return m_color_format;
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
