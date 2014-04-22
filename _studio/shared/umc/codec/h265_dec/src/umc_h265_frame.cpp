/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include <algorithm>
#include "umc_h265_frame.h"
#include "umc_h265_task_supplier.h"
#include "umc_h265_ipplevel.h"

#include "umc_h265_debug.h"


namespace UMC_HEVC_DECODER
{

H265DecoderFrame::H265DecoderFrame(UMC::MemoryAllocator *pMemoryAllocator, Heap_Objects * pObjHeap)
    : H265DecYUVBufferPadded(pMemoryAllocator)
    , m_ErrorType(0)
    , m_pSlicesInfo(0)
    , m_pPreviousFrame(0)
    , m_pFutureFrame(0)
    , m_dFrameTime(-1.0)
    , m_isOriginalPTS(false)
    , m_dpb_output_delay(INVALID_DPB_DELAY_H265)
    , post_procces_complete(false)
    , m_index(-1)
    , m_UID(-1)
    , m_cuOffsetY(0)
    , m_cuOffsetC(0)
    , m_buOffsetY(0)
    , m_buOffsetC(0)
    , m_pObjHeap(pObjHeap)
{
    m_isShortTermRef = false;
    m_isLongTermRef = false;
    m_RefPicListResetCount = 0;
    m_PicOrderCnt = 0;
    m_CodingData = NULL;
    // set memory managment tools
    m_pMemoryAllocator = pMemoryAllocator;

    ResetRefCounter();

    m_pSlicesInfo = new H265DecoderFrameInfo(this, m_pObjHeap);

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_maxUIDWhenWasDisplayed = 0;

    prepared = false;
}

H265DecoderFrame::~H265DecoderFrame()
{
    if (m_pSlicesInfo)
    {
        delete m_pSlicesInfo;
        m_pSlicesInfo = 0;
    }

    // Just to be safe.
    m_pPreviousFrame = 0;
    m_pFutureFrame = 0;
    Reset();
    deallocate();
    deallocateCodingData();
}

// Add target frame to the list of reference frames
void H265DecoderFrame::AddReferenceFrame(H265DecoderFrame * frm)
{
    if (!frm || frm == this)
        return;

    if (std::find(m_references.begin(), m_references.end(), frm) != m_references.end())
        return;

    frm->IncrementReference();
    m_references.push_back(frm);
}

// Clear all references to other frames
void H265DecoderFrame::FreeReferenceFrames()
{
    ReferenceList::iterator iter = m_references.begin();
    ReferenceList::iterator end_iter = m_references.end();

    for (; iter != end_iter; ++iter)
    {
        RefCounter *reference = *iter;
        reference->DecrementReference();
    }

    m_references.clear();
}

// Reinitialize frame structure before reusing frame
void H265DecoderFrame::Reset()
{
    if (m_pSlicesInfo)
        m_pSlicesInfo->Reset();

    ResetRefCounter();

    m_isShortTermRef = false;
    m_isLongTermRef = false;

    post_procces_complete = false;

    m_RefPicListResetCount = 0;
    m_PicOrderCnt = 0;

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_dpb_output_delay = INVALID_DPB_DELAY_H265;
    m_maxUIDWhenWasDisplayed = 0;

    m_dFrameTime = -1;
    m_isOriginalPTS = false;

    m_IsFrameExist = false;
    m_isUsedAsReference = false;

    m_UserData.Reset();

    m_ErrorType = 0;
    m_UID = -1;
    m_index = -1;

    m_MemID = MID_INVALID;

    prepared = false;

    FreeReferenceFrames();

    deallocate();
}

// Returns whether frame has all slices found
bool H265DecoderFrame::IsFullFrame() const
{
    return (m_Flags.isFull == 1);
}

// Set frame flag denoting that all slices for it were found
void H265DecoderFrame::SetFullFrame(bool isFull)
{
    m_Flags.isFull = (Ipp8u) (isFull ? 1 : 0);
}

// Returns whether frame has been decoded
bool H265DecoderFrame::IsDecoded() const
{
    return m_Flags.isDecoded == 1;
}

// Clean up data after decoding is done
void H265DecoderFrame::FreeResources()
{
    FreeReferenceFrames();

    if (m_pSlicesInfo && IsDecoded())
        m_pSlicesInfo->Free();
}

// Flag frame as completely decoded
void H265DecoderFrame::CompleteDecoding()
{
    m_Flags.isDecodingCompleted = 1;
    UpdateErrorWithRefFrameStatus();
}

// Check reference frames for error status and flag this frame if error is found
void H265DecoderFrame::UpdateErrorWithRefFrameStatus()
{
    if (m_pSlicesInfo->CheckReferenceFrameError())
    {
        SetErrorFlagged(UMC::ERROR_FRAME_REFERENCE_FRAME);
    }
}

// Delete unneeded references and set flags after decoding is done
void H265DecoderFrame::OnDecodingCompleted()
{
    UpdateErrorWithRefFrameStatus();

    SetisDisplayable();

    m_Flags.isDecoded = 1;
    DEBUG_PRINT1((VM_STRING("On decoding complete decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
    DecrementReference();

    FreeResources();
}

// Mark frame as short term reference frame
void H265DecoderFrame::SetisShortTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isShortTermRef = true;
    }
    else
    {
        bool wasRef = isShortTermRef() != 0;

        m_isShortTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }
            DecrementReference();
            DEBUG_PRINT1((VM_STRING("On was short term ref decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
        }
    }
}

// Mark frame as long term reference frame
void H265DecoderFrame::SetisLongTermRef(bool isRef)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        m_isLongTermRef = true;
    }
    else
    {
        bool wasRef = isLongTermRef() != 0;

        m_isLongTermRef = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }

            DEBUG_PRINT1((VM_STRING("On was long term reft decrement for POC %d, reference = %d\n"), m_PicOrderCnt, m_refCounter));
            DecrementReference();
        }
    }
}

// Flag frame after it was output
void H265DecoderFrame::setWasOutputted()
{
    m_wasOutputted = 1;
}

// Free resources if possible
void H265DecoderFrame::Free()
{
    if (wasDisplayed() && wasOutputted())
        Reset();
}

// Copy plane data from target frame
void H265DecoderFrame::CopyPlanes(H265DecoderFrame *pRefFrame)
{
    if (pRefFrame->m_pYPlane)
    {
        CopyPlane_H265((Ipp16s*)pRefFrame->m_pYPlane, pRefFrame->m_pitch_luma, (Ipp16s*)m_pYPlane, m_pitch_luma, m_lumaSize);
        CopyPlane_H265((Ipp16s*)pRefFrame->m_pUPlane, pRefFrame->m_pitch_chroma, (Ipp16s*)m_pUPlane, m_pitch_chroma, m_chromaSize);
        CopyPlane_H265((Ipp16s*)pRefFrame->m_pVPlane, pRefFrame->m_pitch_chroma, (Ipp16s*)m_pVPlane, m_pitch_chroma, m_chromaSize);
    }
    else
    {
        DefaultFill(2, false);
    }
}

// Fill frame planes with default values
void H265DecoderFrame::DefaultFill(Ipp32s fields_mask, bool isChromaOnly, Ipp8u defaultValue)
{
    try
    {
        IppiSize roi;

        Ipp32s field_factor = fields_mask == 2 ? 0 : 1;
        Ipp32s field = field_factor ? fields_mask : 0;

        if (!isChromaOnly)
        {
            roi = m_lumaSize;
            roi.height >>= field_factor;

            if (m_pYPlane)
                SetPlane_H265(defaultValue, m_pYPlane + field*pitch_luma(),
                    pitch_luma() << field_factor, roi);
        }

        roi = m_chromaSize;
        roi.height >>= field_factor;

        if (m_pUVPlane) // NV12
        {
            roi.width *= 2;
            SetPlane_H265(defaultValue, m_pUVPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
        }
        else
        {
            if (m_pUPlane)
                SetPlane_H265(defaultValue, m_pUPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
            if (m_pVPlane)
                SetPlane_H265(defaultValue, m_pVPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
        }
    } catch(...)
    {
        // nothing to do
        //VM_ASSERT(false);
    }
}

// Allocate and initialize frame array of CTBs and SAO parameters
void H265DecoderFrame::allocateCodingData(const H265SeqParamSet* sps, const H265PicParamSet *pps)
{
    if (!m_CodingData)
    {
        m_CodingData = new H265FrameCodingData();
    }

    Ipp32u MaxCUSize = sps->MaxCUSize;

    Ipp32u MaxCUDepth   = sps->MaxCUDepth;

    Ipp32u widthInCU = (m_lumaSize.width % MaxCUSize) ? m_lumaSize.width / MaxCUSize + 1 : m_lumaSize.width / MaxCUSize;
    Ipp32u heightInCU = (m_lumaSize.height % MaxCUSize) ? m_lumaSize.height / MaxCUSize + 1 : m_lumaSize.height / MaxCUSize;

    m_CodingData->m_partitionInfo.Init(sps);

    if (m_CodingData->m_MaxCUWidth != MaxCUSize ||
        m_CodingData->m_WidthInCU != widthInCU  || m_CodingData->m_HeightInCU != heightInCU || m_CodingData->m_MaxCUDepth != MaxCUDepth)
    {
        m_CodingData->destroy();
        m_CodingData->create(m_lumaSize.width, m_lumaSize.height, MaxCUSize, MaxCUSize, sps->MaxCUDepth);

        delete[] m_cuOffsetY;
        delete[] m_cuOffsetC;
        delete[] m_buOffsetY;
        delete[] m_buOffsetC;

        Ipp32u pixelSize = (sps->bit_depth_luma > 8 || sps->bit_depth_chroma > 8) ? 2 : 1;

        // Initialize CU offset tables
        Ipp32s NumCUInWidth = m_CodingData->m_WidthInCU;
        Ipp32s NumCUInHeight = m_CodingData->m_HeightInCU;
        m_cuOffsetY = h265_new_array_throw<Ipp32s>(NumCUInWidth * NumCUInHeight);
        m_cuOffsetC = h265_new_array_throw<Ipp32s>(NumCUInWidth * NumCUInHeight);
        for (Ipp32s cuRow = 0; cuRow < NumCUInHeight; cuRow++)
        {
            for (Ipp32s cuCol = 0; cuCol < NumCUInWidth; cuCol++)
            {
                m_cuOffsetY[cuRow * NumCUInWidth + cuCol] = m_pitch_luma * cuRow * MaxCUSize + cuCol * MaxCUSize;
                m_cuOffsetC[cuRow * NumCUInWidth + cuCol] = m_pitch_chroma * cuRow * (MaxCUSize / 2) + cuCol * (MaxCUSize);

                m_cuOffsetY[cuRow * NumCUInWidth + cuCol] *= pixelSize;
                m_cuOffsetC[cuRow * NumCUInWidth + cuCol] *= pixelSize;
            }
        }

        // Initialize partition offsets tables
        m_buOffsetY = h265_new_array_throw<Ipp32s>(1 << (2 * MaxCUDepth));
        m_buOffsetC = h265_new_array_throw<Ipp32s>(1 << (2 * MaxCUDepth));
        for (Ipp32s buRow = 0; buRow < (1 << MaxCUDepth); buRow++)
        {
            for (Ipp32s buCol = 0; buCol < (1 << MaxCUDepth); buCol++)
            {
                m_buOffsetY[(buRow << MaxCUDepth) + buCol] = m_pitch_luma * buRow * (MaxCUSize >> MaxCUDepth) + buCol * (MaxCUSize  >> MaxCUDepth);
                m_buOffsetC[(buRow << MaxCUDepth) + buCol] = m_pitch_chroma * buRow * (MaxCUSize / 2 >> MaxCUDepth) + buCol * (MaxCUSize >> MaxCUDepth);

                m_buOffsetY[(buRow << MaxCUDepth) + buCol] *= pixelSize;
                m_buOffsetC[(buRow << MaxCUDepth) + buCol] *= pixelSize;
            }
        }
    }

    m_CodingData->m_CUOrderMap = pps->m_CtbAddrTStoRS;
    m_CodingData->m_InverseCUOrderMap = pps->m_CtbAddrRStoTS;
    m_CodingData->m_TileIdxMap = pps->m_TileIdx;

    m_CodingData->initSAO(sps);

}

// Free array of CTBs
void H265DecoderFrame::deallocateCodingData()
{
    delete m_CodingData;
    m_CodingData = NULL;

    delete [] m_cuOffsetY;
    m_cuOffsetY = NULL;

    delete [] m_cuOffsetC;
    m_cuOffsetC = NULL;

    delete [] m_buOffsetY;
    m_buOffsetY = NULL;

    delete [] m_buOffsetC;
    m_buOffsetC = NULL;
}

#if !defined(_WIN32) && !defined(_WIN64)
// RefPicList. Returns pointer to start of specified ref pic list.
H265DecoderRefPicList* H265DecoderFrame::GetRefPicList(Ipp32s sliceNumber, Ipp32s list) const
{
    return GetAU()->GetRefPicList(sliceNumber, list);
}
#endif

// Returns a CTB by its raster address
H265CodingUnit* H265DecoderFrame::getCU(Ipp32u CUaddr) const 
{
    return m_CodingData->getCU(CUaddr);
}

// Returns number of CTBs in frame
Ipp32u H265DecoderFrame::getNumCUsInFrame() const
{
    return m_CodingData->m_NumCUsInFrame;
}

// Returns number of minimal partitions in CTB width or height
Ipp32u H265DecoderFrame::getNumPartInCUSize() const
{
    return m_CodingData->m_NumPartInWidth;
}

// Returns number of CTBs in frame width
Ipp32u H265DecoderFrame::getFrameWidthInCU() const
{
    return m_CodingData->m_WidthInCU;
}

// Returns number of CTBs in frame height
Ipp32u H265DecoderFrame::getFrameHeightInCU() const
{
    return m_CodingData->m_HeightInCU;
}

//  Access starting position of original picture for specific coding unit (CU)
H265PlanePtrYCommon H265DecoderFrame::GetLumaAddr(Ipp32s CUAddr) const
{
    return m_pYPlane + m_cuOffsetY[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU)
H265PlanePtrUVCommon H265DecoderFrame::GetCbAddr(Ipp32s CUAddr) const
{
    return m_pUPlane + m_cuOffsetC[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU)
H265PlanePtrUVCommon H265DecoderFrame::GetCrAddr(Ipp32s CUAddr) const
{
    return m_pVPlane + m_cuOffsetC[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU)
H265PlanePtrUVCommon H265DecoderFrame::GetCbCrAddr(Ipp32s CUAddr) const
{
    // Chroma offset is already multiplied to chroma pitch (double for NV12)
    return m_pUVPlane + m_cuOffsetC[CUAddr];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
// ML: OPT: TODO: Make these functions available for inlining 
H265PlanePtrYCommon H265DecoderFrame::GetLumaAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const
{
    return m_pYPlane + m_cuOffsetY[CUAddr] + m_buOffsetY[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
H265PlanePtrUVCommon H265DecoderFrame::GetCbAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const
{
    return m_pUPlane + m_cuOffsetC[CUAddr] + m_buOffsetC[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
H265PlanePtrUVCommon H265DecoderFrame::GetCrAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const
{
    return m_pVPlane + m_cuOffsetC[CUAddr] + m_buOffsetC[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}

//  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
H265PlanePtrUVCommon H265DecoderFrame::GetCbCrAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const
{
    // Chroma offset is already multiplied to chroma pitch (double for NV12)
    return m_pUVPlane + m_cuOffsetC[CUAddr] + m_buOffsetC[getCD()->m_partitionInfo.m_zscanToRaster[AbsZorderIdx]];
}

} // end namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
