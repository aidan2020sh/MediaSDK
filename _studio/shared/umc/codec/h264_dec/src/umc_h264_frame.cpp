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

#include <ippi.h>
#include <algorithm>
#include "umc_h264_frame.h"
#include "umc_h264_task_supplier.h"

#include "umc_h264_log.h"

#define NON_LAYER_FRAME

namespace UMC
{

H264DecoderFrame g_GlobalFakeFrame(0, 0, 0);

H264DecoderFrameInfo::FakeFrameInitializer H264DecoderFrameInfo::g_FakeFrameInitializer;

H264DecoderFrameInfo::FakeFrameInitializer::FakeFrameInitializer()
{
    g_GlobalFakeFrame.m_ID[0] = (size_t)-1;
    g_GlobalFakeFrame.m_ID[1] = (size_t)-1;
};

H264DecoderFrame::H264DecoderFrame(MemoryAllocator *pMemoryAllocator, H264_Heap * heap, H264_Heap_Objects * pObjHeap)
    : H264DecYUVBufferPadded()
    , m_TopSliceCount(0)
    , m_ErrorType(0)
    , m_pSlicesInfo(0)
    , m_pSlicesInfoBottom(0)
    , m_pPreviousFrame(0)
    , m_pFutureFrame(0)
    , m_NotifiersChain(pObjHeap)
    , m_dFrameTime(-1.0)
    , m_isOriginalPTS(false)
    , m_dpb_output_delay(INVALID_DPB_OUTPUT_DELAY)
    , post_procces_complete(false)
    , m_auIndex(-1)
    , m_index(-1)
    , m_UID(-1)
    , m_minDId(0)
    , m_maxDId(0)
    , m_maxQId(0)
    , m_pObjHeap(pObjHeap)
    , m_pHeap(heap)
{

    for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++)
    {
        m_pLayerFrames[i] = 0;
    }

    m_isShortTermRef[0] = m_isShortTermRef[1] = false;
    m_isLongTermRef[0] = m_isLongTermRef[1] = false;
    m_isInterViewRef[0] = m_isInterViewRef[1] = false;
    m_LongTermPicNum[0] = m_LongTermPicNum[1] = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_LongTermFrameIdx = -1;
    m_viewId = 0;
    m_RefPicListResetCount[0] = m_RefPicListResetCount[1] = 0;
    m_PicNum[0] = m_PicNum[1] = -1;
    m_LongTermFrameIdx = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_LongTermPicNum[0] = m_PicNum[1] = -1;
    m_PicOrderCnt[0] = m_PicOrderCnt[1] = 0;
    m_bIDRFlag = false;

    // set memory managment tools
    m_pMemoryAllocator = pMemoryAllocator;
    m_midParsedFrameDataNew = 0;
    m_pParsedFrameDataNew = 0;
    m_paddedParsedFrameDataSize.width = 0;
    m_paddedParsedFrameDataSize.height = 0;

    ResetRefCounter();

    m_pSlicesInfo = new H264DecoderFrameInfo(this, m_pHeap, m_pObjHeap);
    m_pSlicesInfoBottom = new H264DecoderFrameInfo(this, m_pHeap, m_pObjHeap);

    m_ID[0] = (Ipp32s) ((Ipp8u *) this - (Ipp8u *) 0);
    m_ID[1] = m_ID[0] + 1;

    m_PictureStructureForRef = FRM_STRUCTURE;

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_Flags.isActive = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;

    prepared[0] = false;
    prepared[1] = false;

    m_iResourceNumber = -1;
}

H264DecoderFrame::~H264DecoderFrame()
{
    if (m_pSlicesInfo)
    {
        delete m_pSlicesInfo;
        delete m_pSlicesInfoBottom;
        m_pSlicesInfo = 0;
        m_pSlicesInfoBottom = 0;
    }

    // Just to be safe.
    m_pPreviousFrame = 0;
    m_pFutureFrame = 0;
    Reset();
    deallocate();
    deallocateParsedFrameData();
}

void H264DecoderFrame::AddReference(RefCounter * reference)
{
    if (!reference || reference == this)
        return;

    if (std::find(m_references.begin(), m_references.end(), reference) != m_references.end())
        return;

    reference->IncrementReference();
    m_references.push_back(reference);
}

void H264DecoderFrame::FreeReferenceFrames()
{
    ReferenceList::iterator iter = m_references.begin();
    ReferenceList::iterator end_iter = m_references.end();

    for (; iter != end_iter; ++iter)
    {
        RefCounter * reference = *iter;
        reference->DecrementReference();

    }

    m_references.clear();
}

void H264DecoderFrame::Reset()
{
    for (Ipp32s i = m_minDId; i < m_maxDId; i++)
    {
        if (!m_pLayerFrames[i] || m_pLayerFrames[i] == this)
            continue;

#ifdef NON_LAYER_FRAME
        //m_pLayerFrames[i]->Reset();
#endif
        m_pLayerFrames[i] = 0;
    }

    m_TopSliceCount = 0;

    if (m_pSlicesInfo)
    {
        m_pSlicesInfo->Reset();
        m_pSlicesInfoBottom->Reset();
    }

    ResetRefCounter();

    for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++)
    {
        m_pLayerFrames[i] = 0;
    }

    m_minDId = 0;
    m_maxDId = 0;
    m_maxQId = 0;
    m_lastSliceInAVC = 0;
    m_firstSliceInBaseRefLayer = -1;
    m_lastSliceInBaseRefLayer = -1;
    m_use_base_residual = 0;
    m_storeRefBasePic = 0;
    m_lastSliceInBaseRefLayer = 0;

    m_isShortTermRef[0] = m_isShortTermRef[1] = false;
    m_isLongTermRef[0] = m_isLongTermRef[1] = false;
    m_isInterViewRef[0] = m_isInterViewRef[1] = false;

    post_procces_complete = false;
    m_bIDRFlag = false;

    m_RefPicListResetCount[0] = m_RefPicListResetCount[1] = 0;
    m_PicNum[0] = m_PicNum[1] = -1;
    m_LongTermPicNum[0] = m_LongTermPicNum[1] = -1;
    m_PictureStructureForRef = FRM_STRUCTURE;
    m_PicOrderCnt[0] = m_PicOrderCnt[1] = 0;

    m_viewId = 0;
    m_LongTermFrameIdx = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_Flags.isActive = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_dpb_output_delay = INVALID_DPB_OUTPUT_DELAY;

    m_bottom_field_flag[0] = m_bottom_field_flag[1] = -1;

    m_dFrameTime = -1;
    m_isOriginalPTS = false;

    m_IsFrameExist = false;
    m_iNumberOfSlices = 0;

    m_NotifiersChain.Reset();
    m_UserData.Reset();

    m_ErrorType = 0;
    m_UID = -1;
    m_index = -1;
    m_auIndex = -1;
    m_iResourceNumber = -1;

    m_MemID = MID_INVALID;

    m_minDId = 0;
    m_maxDId = 0;
    m_maxQId = 0;

    prepared[0] = false;
    prepared[1] = false;

    m_displayPictureStruct = DPS_UNKNOWN;

    FreeReferenceFrames();

    deallocate();
}

bool H264DecoderFrame::IsFullFrame() const
{
    return (m_Flags.isFull == 1);
}

void H264DecoderFrame::SetFullFrame(bool isFull)
{
    m_Flags.isFull = (Ipp8u) (isFull ? 1 : 0);
}

bool H264DecoderFrame::IsDecoded() const
{
    return m_Flags.isDecoded == 1;
}

void H264DecoderFrame::FreeResources()
{
    GetNotifiersChain()->Notify();
    FreeReferenceFrames();

    if (m_pSlicesInfo && IsDecoded())
    {
        m_pSlicesInfo->Free();
        m_pSlicesInfoBottom->Free();
    }

    if (GetRefCounter() == 0 && !isShortTermRef() && !isLongTermRef())
    {
        deallocateParsedFrameData();
    }
}

void H264DecoderFrame::CompleteDecoding()
{
    m_Flags.isDecodingCompleted = 1;
    UpdateErrorWithRefFrameStatus();
}

void H264DecoderFrame::UpdateErrorWithRefFrameStatus()
{
    if (m_pSlicesInfo->CheckReferenceFrameError() || m_pSlicesInfoBottom->CheckReferenceFrameError())
    {
        SetErrorFlagged(ERROR_FRAME_REFERENCE_FRAME);
    }
}

void H264DecoderFrame::OnDecodingCompleted()
{
#ifdef ENABLE_LOGGING
    if (GetLogging())
        GetLogging()->LogFrame(this);
#endif

    UpdateErrorWithRefFrameStatus();

    SetisDisplayable();

    m_Flags.isDecoded = 1;

#ifdef NON_LAYER_FRAME
    for (Ipp32s i = m_minDId; i < m_maxDId; i++)
    {
        if (!m_pLayerFrames[i] || m_pLayerFrames[i] == this)
            continue;

        m_pLayerFrames[i]->OnDecodingCompleted();
        m_pLayerFrames[i]->setWasOutputted();
        m_pLayerFrames[i]->setWasDisplayed();

        if (m_pLayerFrames[i]->isShortTermRef() || m_pLayerFrames[i]->isLongTermRef())
            m_pLayerFrames[i]->AddReference(this);
    }
#endif

    DecrementReference();

    FreeResources();
}

void H264DecoderFrame::SetisShortTermRef(bool isRef, Ipp32s WhichField)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            m_isShortTermRef[0] = m_isShortTermRef[1] = true;
        else
            m_isShortTermRef[WhichField] = true;
    }
    else
    {
        bool wasRef = isShortTermRef() != 0;

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            m_isShortTermRef[0] = m_isShortTermRef[1] = false;
        }
        else
            m_isShortTermRef[WhichField] = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }
            DecrementReference();
        }
    }
}

void H264DecoderFrame::SetisLongTermRef(bool isRef, Ipp32s WhichField)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            m_isLongTermRef[0] = m_isLongTermRef[1] = true;
        else
            m_isLongTermRef[WhichField] = true;
    }
    else
    {
        bool wasRef = isLongTermRef() != 0;

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            m_isLongTermRef[0] = m_isLongTermRef[1] = false;
        }
        else
            m_isLongTermRef[WhichField] = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }

            DecrementReference();
        }
    }
}

void H264DecoderFrame::setWasOutputted()
{
    m_wasOutputted = 1;
}

void H264DecoderFrame::Free()
{
    if (wasDisplayed() && wasOutputted())
        Reset();
}

void H264DecoderFrame::CopyPlanes(H264DecoderFrame *pRefFrame)
{
    if (pRefFrame->m_pYPlane)
    {
        CopyPlane(pRefFrame->m_pYPlane, pRefFrame->m_pitch_luma, m_pYPlane, m_pitch_luma, m_lumaSize);
        CopyPlane(pRefFrame->m_pUPlane, pRefFrame->m_pitch_chroma, m_pUPlane, m_pitch_chroma, m_chromaSize);
        CopyPlane(pRefFrame->m_pVPlane, pRefFrame->m_pitch_chroma, m_pVPlane, m_pitch_chroma, m_chromaSize);
    }
    else
    {
        DefaultFill(2, false);
    }
}

void H264DecoderFrame::DefaultFill(Ipp32s fields_mask, bool isChromaOnly, Ipp8u defaultValue)
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
                SetPlane(defaultValue, m_pYPlane + field*pitch_luma(),
                    pitch_luma() << field_factor, roi);
        }

        roi = m_chromaSize;
        roi.height >>= field_factor;

        if (m_pUVPlane) // NV12
        {
            roi.width *= 2;
            SetPlane(defaultValue, m_pUVPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
        }
        else
        {
            if (m_pUPlane)
                SetPlane(defaultValue, m_pUPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
            if (m_pVPlane)
                SetPlane(defaultValue, m_pVPlane + field*pitch_chroma(), pitch_chroma() << field_factor, roi);
        }
    } catch(...)
    {
        // nothing to do
        //VM_ASSERT(false);
    }
}

void H264DecoderFrame::deallocateParsedFrameData()
{
    // new structure(s) hold pointer
    if (m_pParsedFrameDataNew)
    {
        // Free the old buffer.
        m_pObjHeap->Free(m_pParsedFrameDataNew);

        m_midParsedFrameDataNew = 0;
        m_pParsedFrameDataNew = 0;

        m_mbinfo.MV[0] = 0;
        m_mbinfo.MV[1] = 0;
        m_mbinfo.mbs = 0;
    }

    m_paddedParsedFrameDataSize.width = 0;
    m_paddedParsedFrameDataSize.height = 0;

}    // deallocateParsedFrameData

size_t H264DecoderFrame::GetFrameDataSize(const IppiSize &lumaSize)
{
    size_t nMBCount = (lumaSize.width >> 4) * (lumaSize.height >> 4);

    // allocate buffer
    size_t nMemSize = (sizeof(H264DecoderMacroblockMVs) +
                       sizeof(H264DecoderMacroblockMVs) +
                       sizeof(H264DecoderMacroblockGlobalInfo)) * nMBCount + 16 * 5;

    return nMemSize;
}

Status H264DecoderFrame::allocateParsedFrameData()
{
    Status      umcRes = UMC_OK;

    // If our buffer and internal pointers are already set up for this
    // image size, then there's nothing more to do.

    if (!m_pParsedFrameDataNew || m_paddedParsedFrameDataSize.width != m_lumaSize.width ||
        m_paddedParsedFrameDataSize.height != m_lumaSize.height)
    {
        if (m_pParsedFrameDataNew)
        {
            deallocateParsedFrameData();
        }

        // allocate new MB structure(s)
        size_t nMBCount = (m_lumaSize.width >> 4) * (m_lumaSize.height >> 4);

        // allocate buffer
        size_t nMemSize = GetFrameDataSize(m_lumaSize);

        m_pParsedFrameDataNew = m_pObjHeap->Allocate<Ipp8u>(nMemSize);

        // set pointer(s)
        m_mbinfo.MV[0] = align_pointer<H264DecoderMacroblockMVs *> (m_pParsedFrameDataNew, ALIGN_VALUE);
        m_mbinfo.MV[1] = align_pointer<H264DecoderMacroblockMVs *> (m_mbinfo.MV[0]+ nMBCount, ALIGN_VALUE);
        m_mbinfo.mbs = align_pointer<H264DecoderMacroblockGlobalInfo *> (m_mbinfo.MV[1] + nMBCount, ALIGN_VALUE);

        m_paddedParsedFrameDataSize.width = m_lumaSize.width;
        m_paddedParsedFrameDataSize.height = m_lumaSize.height;
    }

    return umcRes;

} // H264DecoderFrame::allocateParsedFrameData(const IppiSize &size)

H264DecoderRefPicList* H264DecoderFrame::GetRefPicList(Ipp32s sliceNumber, Ipp32s list)
{
    H264DecoderRefPicList *pList;
    pList = GetAU(sliceNumber > m_TopSliceCount ? 1 : 0)->GetRefPicList(sliceNumber, list);
    return pList;
}   // RefPicList. Returns pointer to start of specified ref pic list.

//////////////////////////////////////////////////////////////////////////////
// updateFrameNumWrap
//  Updates m_FrameNumWrap and m_PicNum if the frame is a short-term
//  reference and a frame number wrap has occurred.
//////////////////////////////////////////////////////////////////////////////
void H264DecoderFrame::UpdateFrameNumWrap(Ipp32s  CurrFrameNum, Ipp32s  MaxFrameNum, Ipp32s CurrPicStruct)
{
    if (isShortTermRef())
    {
        m_FrameNumWrap = m_FrameNum;
        if (m_FrameNum > CurrFrameNum)
            m_FrameNumWrap -= MaxFrameNum;
        if (CurrPicStruct >= FRM_STRUCTURE)
        {
            setPicNum(m_FrameNumWrap, 0);
            setPicNum(m_FrameNumWrap, 1);
            m_PictureStructureForRef = FRM_STRUCTURE;
        }
        else
        {
            m_PictureStructureForRef = FLD_STRUCTURE;
            if (m_bottom_field_flag[0])
            {
                //1st - bottom, 2nd - top
                if (isShortTermRef(0)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == BOTTOM_FLD_STRUCTURE), 0);
                if (isShortTermRef(1)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == TOP_FLD_STRUCTURE), 1);
            }
            else
            {
                //1st - top , 2nd - bottom
                if (isShortTermRef(0)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == TOP_FLD_STRUCTURE), 0);
                if (isShortTermRef(1)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == BOTTOM_FLD_STRUCTURE), 1);
            }
        }
    }

}    // updateFrameNumWrap

//////////////////////////////////////////////////////////////////////////////
// updateLongTermPicNum
//  Updates m_LongTermPicNum for if long term reference, based upon
//  m_LongTermFrameIdx.
//////////////////////////////////////////////////////////////////////////////
void H264DecoderFrame::UpdateLongTermPicNum(Ipp32s CurrPicStruct)
{
    if (isLongTermRef())
    {
        if (CurrPicStruct >= FRM_STRUCTURE)
        {
            m_LongTermPicNum[0] = m_LongTermFrameIdx;
            m_LongTermPicNum[1] = m_LongTermFrameIdx;
            m_PictureStructureForRef = FRM_STRUCTURE;
        }
        else
        {
            m_PictureStructureForRef = FLD_STRUCTURE;
            if (m_bottom_field_flag[0])
            {
                //1st - bottom, 2nd - top
                m_LongTermPicNum[0] = 2*m_LongTermFrameIdx + (CurrPicStruct == BOTTOM_FLD_STRUCTURE);
                m_LongTermPicNum[1] = 2*m_LongTermFrameIdx + (CurrPicStruct == TOP_FLD_STRUCTURE);
            }
            else
            {
                //1st - top , 2nd - bottom
                m_LongTermPicNum[0] = 2*m_LongTermFrameIdx + (CurrPicStruct == TOP_FLD_STRUCTURE);
                m_LongTermPicNum[1] = 2*m_LongTermFrameIdx + (CurrPicStruct == BOTTOM_FLD_STRUCTURE);
            }
        }
    }
}    // updateLongTermPicNum



//////////////////////////////////////////////////////////////////////////////
//  H264DecoderFrameExtension class implementation
//////////////////////////////////////////////////////////////////////////////
H264DecoderFrameExtension::H264DecoderFrameExtension(MemoryAllocator *pMemoryAllocator, H264_Heap * heap, H264_Heap_Objects * pObjHeap)
    : H264DecoderFrame(pMemoryAllocator, heap, pObjHeap)
    , m_pAuxiliaryFrame(0)
{
    is_auxiliary_frame = false;
    primary_picture = 0;
}

H264DecoderFrameExtension::~H264DecoderFrameExtension()
{
    delete m_pAuxiliaryFrame;
}

H264DecoderFrame * H264DecoderFrameExtension::GetAuxiliaryFrame()
{
    return m_pAuxiliaryFrame;
}

void H264DecoderFrameExtension::AllocateAuxiliary()
{
    if (m_pAuxiliaryFrame)
        return;

    m_pAuxiliaryFrame = new H264DecoderFrame(m_pMemoryAllocator, m_pHeap, m_pObjHeap);

    if (!m_pAuxiliaryFrame)
    {
        throw h264_exception(UMC_ERR_ALLOC);
    }

    m_pAuxiliaryFrame->primary_picture = this;
    m_pAuxiliaryFrame->is_auxiliary_frame = true;
}

void H264DecoderFrameExtension::FillInfoToAuxiliary()
{
    if (!m_pAuxiliaryFrame)
        return;

    m_pAuxiliaryFrame->m_bIDRFlag = m_bIDRFlag;
    m_pAuxiliaryFrame->m_PicNum[0] = m_PicNum[0];
    m_pAuxiliaryFrame->m_PicNum[1] = m_PicNum[1];
    m_pAuxiliaryFrame->m_LongTermPicNum[0] = m_LongTermPicNum[0];
    m_pAuxiliaryFrame->m_LongTermPicNum[1] = m_LongTermPicNum[1];
    m_pAuxiliaryFrame->m_FrameNum = m_FrameNum;
    m_pAuxiliaryFrame->m_FrameNumWrap = m_FrameNumWrap;
    m_pAuxiliaryFrame->m_LongTermFrameIdx = m_LongTermFrameIdx;
    m_pAuxiliaryFrame->m_RefPicListResetCount[0] = m_RefPicListResetCount[0];
    m_pAuxiliaryFrame->m_RefPicListResetCount[1] = m_RefPicListResetCount[1];
    m_pAuxiliaryFrame->m_PicOrderCnt[0] = m_PicOrderCnt[0];
    m_pAuxiliaryFrame->m_PicOrderCnt[1] = m_PicOrderCnt[1];
    m_pAuxiliaryFrame->m_isShortTermRef[0] = m_isShortTermRef[0];
    m_pAuxiliaryFrame->m_isShortTermRef[1] = m_isShortTermRef[1];
    m_pAuxiliaryFrame->m_isLongTermRef[0] = m_isLongTermRef[0];
    m_pAuxiliaryFrame->m_isLongTermRef[1] = m_isLongTermRef[1];
    m_pAuxiliaryFrame->m_dFrameTime = m_dFrameTime;

    m_pAuxiliaryFrame->setPrevious(UMC::GetAuxiliaryFrame(m_pPreviousFrame));
    m_pAuxiliaryFrame->setFuture(UMC::GetAuxiliaryFrame(m_pFutureFrame));
}

} // end namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
