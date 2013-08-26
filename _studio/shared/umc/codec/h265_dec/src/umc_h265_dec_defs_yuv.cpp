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
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER
#include "umc_h265_dec_defs_yuv.h"
#include "umc_h265_frame.h"
#include "h265_global_rom.h"

namespace UMC_HEVC_DECODER
{
H265DecYUVBufferPadded::H265DecYUVBufferPadded()
    : m_pYPlane(NULL)
    , m_pUVPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pAllocatedBuffer(0)
    , m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
}

H265DecYUVBufferPadded::H265DecYUVBufferPadded(UMC::MemoryAllocator *pMemoryAllocator)
    : m_pYPlane(NULL)
    , m_pUVPlane(NULL)
    , m_pUPlane(NULL)
    , m_pVPlane(NULL)
    , m_pAllocatedBuffer(0)
    , m_pitch_luma(0)
    , m_pitch_chroma(0)
    , m_color_format(UMC::NV12)
{
    m_lumaSize.width = 0;
    m_lumaSize.height = 0;

    m_pMemoryAllocator = pMemoryAllocator;
    m_midAllocatedBuffer = 0;
}

H265DecYUVBufferPadded::~H265DecYUVBufferPadded()
{
    deallocate();
}

const UMC::FrameData * H265DecYUVBufferPadded::GetFrameData() const
{
    return &m_frameData;
}

void H265DecYUVBufferPadded::deallocate()
{
    if (m_frameData.GetFrameMID() != UMC::FRAME_MID_INVALID)
    {
        m_frameData.Close();
        return;
    }

    m_pYPlane = m_pUPlane = m_pVPlane = m_pUVPlane = NULL;

    m_lumaSize.width = 0;
    m_lumaSize.height = 0;
    m_pitch_luma = 0;
    m_pitch_chroma = 0;
}

void H265DecYUVBufferPadded::Init(const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);

    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_color_format = info->GetColorFormat();
    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
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

void H265DecYUVBufferPadded::allocate(const UMC::FrameData * frameData, const UMC::VideoDataInfo *info)
{
    VM_ASSERT(info);
    VM_ASSERT(frameData);

    m_frameData = *frameData;

    if (frameData->GetPlaneMemoryInfo(0)->m_planePtr)
        m_frameData.m_locked = true;

    m_color_format = info->GetColorFormat();
    m_bpp = IPP_MAX(info->GetPlaneBitDepth(0), info->GetPlaneBitDepth(1));

    m_chroma_format = GetH265ColorFormat(info->GetColorFormat());
    m_lumaSize = info->GetPlaneInfo(0)->m_ippSize;
    m_pitch_luma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(0)->m_pitch / info->GetPlaneInfo(0)->m_iSampleSize; //this from h264

    m_pYPlane = (H265PlanePtrYCommon)m_frameData.GetPlaneMemoryInfo(0)->m_planePtr; //wtf H265 uses internal 14 bit depth

    if (m_chroma_format > 0 || GetH265ColorFormat(frameData->GetInfo()->GetColorFormat()) > 0)
    {
        if (m_chroma_format == 0)
            info = frameData->GetInfo();
        m_chromaSize = info->GetPlaneInfo(1)->m_ippSize;
        m_pitch_chroma = (Ipp32s)m_frameData.GetPlaneMemoryInfo(1)->m_pitch / info->GetPlaneInfo(1)->m_iSampleSize; //this from h264

        if (m_frameData.GetInfo()->GetColorFormat() == UMC::NV12)
        {
            m_pUVPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr; //wtf H265 uses internal 14 bit depth
            m_pUPlane = 0;
            m_pVPlane = 0;
        }
        else
        {
            m_pUPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(1)->m_planePtr;    //wtf H265 uses internal 14 bit depth
            m_pVPlane = (H265PlanePtrUVCommon)m_frameData.GetPlaneMemoryInfo(2)->m_planePtr; //wtf H265 uses internal 14 bit depth
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

UMC::ColorFormat H265DecYUVBufferPadded::GetColorFormat() const
{
    return m_color_format;
}

//h265
void H265DecYUVBufferPadded::create(Ipp32u PicWidth, Ipp32u PicHeight, Ipp32u ElementSizeY, Ipp32u ElementSizeUV)
{
    m_lumaSize.width = PicWidth;
    m_lumaSize.height = PicHeight;

    m_chromaSize.width = PicWidth >> 1;
    m_chromaSize.height = PicHeight >> 1;

    m_pitch_luma = PicWidth;
    m_pitch_chroma = PicWidth;

    size_t allocationSize = (m_lumaSize.height) * m_pitch_luma * ElementSizeY + 
        (m_chromaSize.height) * m_pitch_chroma * ElementSizeUV*2 + 512;

    m_pAllocatedBuffer = new Ipp8u[allocationSize];
    m_pYPlane = UMC::align_pointer<H265PlanePtrYCommon>(m_pAllocatedBuffer, 64);

    m_pUVPlane = m_pUPlane = UMC::align_pointer<H265PlanePtrYCommon>(m_pYPlane + (m_lumaSize.height) * m_pitch_luma * ElementSizeY + 128, 64);
    m_pVPlane = m_pUPlane + m_chromaSize.height * m_chromaSize.width * ElementSizeUV;
}

void H265DecYUVBufferPadded::destroy()
{
    delete [] m_pAllocatedBuffer;
    m_pAllocatedBuffer = NULL;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
