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

#ifndef __UMC_H264_FRAME_H__
#define __UMC_H264_FRAME_H__

#include <stdlib.h>
#include "umc_h264_dec_defs_yuv.h"
#include "umc_h264_notify.h"
#include "umc_h264_slice_decoding.h"

namespace UMC
{
class H264DecoderFrameInfo;

extern H264DecoderFrame g_GlobalFakeFrame;

// Struct containing list 0 and list 1 reference picture lists for one slice.
// Length is plus 1 to provide for null termination.
class H264DecoderRefPicList
{
public:
    H264DecoderFrame **m_RefPicList;
    ReferenceFlags    *m_Flags;

    H264DecoderRefPicList()
    {
        memset(this, 0, sizeof(H264DecoderRefPicList));

        m_RefPicList = &(m_refPicList1[1]);
        m_Flags = &(m_flags1[1]);
        m_flags1[0].field = 0;
        m_flags1[0].isShortReference = 1;

        m_refPicList1[0] = &g_GlobalFakeFrame;
    }

    H264DecoderRefPicList (const H264DecoderRefPicList& copy)
    {
        m_RefPicList = &(m_refPicList1[1]);
        m_Flags = &(m_flags1[1]);

        memcpy(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
        memcpy(&m_flags1, &copy.m_flags1, sizeof(m_flags1));
    }

    H264DecoderRefPicList& operator=(const H264DecoderRefPicList & copy)
    {
        memcpy(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
        memcpy(&m_flags1, &copy.m_flags1, sizeof(m_flags1));
        return *this;
    }

private:
    H264DecoderFrame *m_refPicList1[MAX_NUM_REF_FRAMES * 2 + 3];
    ReferenceFlags    m_flags1[MAX_NUM_REF_FRAMES * 2 + 3];
};

enum BusyStates
{
    BUSY_STATE_FREE = 0,
    BUSY_STATE_LOCK1 = 1,
    BUSY_STATE_LOCK2 = 2,
    BUSY_STATE_NOT_DECODED = 3
};

class H264DecoderFrame : public H264DecYUVBufferPadded, public RefCounter
{
    DYNAMIC_CAST_DECL(H264DecoderFrame, H264DecYUVBufferPadded)

    Ipp32s  m_PictureStructureForRef;
    Ipp32s  m_PicOrderCnt[2];    // Display order picture count mod MAX_PIC_ORDER_CNT.
    Ipp32s  m_bottom_field_flag[2];
    Ipp32s  m_PicNum[2];
    Ipp32s  m_LongTermPicNum[2];
    Ipp32s  m_FrameNum;
    Ipp32s  m_FrameNumWrap;
    Ipp32s  m_LongTermFrameIdx;
    // ID of view the frame belongs to
    Ipp32s m_viewId;

    Ipp32s  m_TopSliceCount;

    Ipp32s  m_frameOrder;
    Ipp32s  m_ErrorType;

    H264DecoderFrameInfo * m_pSlicesInfo;
    H264DecoderFrameInfo * m_pSlicesInfoBottom;

    bool prepared[2];

    H264DecoderFrameInfo * GetAU(Ipp32s field = 0) const
    {
        if (field)
            return m_pSlicesInfoBottom;
        else
            return m_pSlicesInfo;
    }

    H264DecoderFrame *m_pPreviousFrame;
    H264DecoderFrame *m_pFutureFrame;

    NotifiersChain   m_NotifiersChain;

    H264SEIPayLoad m_UserData;

    Ipp64f           m_dFrameTime;
    bool             m_isOriginalPTS;

    bool             m_IsFrameExist;

    Ipp32s           m_dpb_output_delay;
    Ipp32s           m_PictureStructureForDec;
    DisplayPictureStruct  m_displayPictureStruct;

#ifdef USE_SEI
    Ipp8u            m_PictureStructureFromSEI;
    Ipp8s            m_RepeatCount;
#endif
    Ipp32s           totalMBs;

    // For type 1 calculation of m_PicOrderCnt. m_FrameNum is needed to
    // be used as previous frame num.

    bool post_procces_complete;
    bool is_auxiliary_frame;
    H264DecoderFrame * primary_picture;

    Ipp32s m_iNumberOfSlices;
    Ipp32s m_iResourceNumber;

    Ipp32s m_auIndex;
    Ipp32s m_index;
    Ipp32s m_UID;
    size_t m_ID[2];
    FrameType m_FrameType;

    MemID m_MemID;

    Ipp32s           m_RefPicListResetCount[2];
    Ipp32s           m_crop_left;
    Ipp32s           m_crop_right;
    Ipp32s           m_crop_top;
    Ipp32s           m_crop_bottom;
    Ipp32s           m_crop_flag;

    Ipp32s           m_aspect_width;
    Ipp32s           m_aspect_height;

    Ipp32s           m_minDId;
    Ipp32s           m_maxDId;
    Ipp32s           m_maxQId;
    Ipp32s           m_lastSliceInAVC;
    Ipp32s           m_firstSliceInBaseRefLayer;
    Ipp32s           m_lastSliceInBaseRefLayer;
    H264DecoderFrame *m_pLayerFrames[MAX_NUM_LAYERS];
    Ipp32s           m_offsets[MAX_NUM_LAYERS][4];
    Ipp32s           m_use_base_residual;
    Ipp32s           m_storeRefBasePic;

    bool             m_isShortTermRef[2];
    bool             m_isLongTermRef[2];
    bool             m_isInterViewRef[2];

    H264DecoderGlobalMacroblocksDescriptor m_mbinfo; //Global MB Data

    bool             m_bIDRFlag;

    Ipp8u                 *m_pParsedFrameDataNew;
    // This points to a huge, monolithic buffer that contains data
    // derived from parsing the current frame.  It contains motion
    // vectors,  MB info, reference indices, and slice info for the
    // current frame, among other things. When B slices are used it
    // contains L0 and L1 motion vectors and reference indices.

    IppiSize           m_paddedParsedFrameDataSize;

    bool IsFullFrame() const;
    void SetFullFrame(bool isFull);

    struct
    {
        Ipp8u  isFull    : 1;
        Ipp8u  isDecoded : 1;
        Ipp8u  isDecodingStarted : 1;
        Ipp8u  isDecodingCompleted : 1;
        Ipp8u  isSkipped : 1;
        Ipp8u  isActive : 1;
    } m_Flags;

    Ipp8u  m_isDisplayable;
    Ipp8u  m_wasDisplayed;
    Ipp8u  m_wasOutputted;

    typedef std::list<RefCounter *>  ReferenceList;
    ReferenceList m_references;


    void FreeReferenceFrames();

    void Reset();
    void FreeResources();

public:

    void AddReference(RefCounter * reference);

    void OnDecodingCompleted();

    virtual void Free();

    void SetSkipped(bool isSkipped)
    {
        m_Flags.isSkipped = (Ipp8u) (isSkipped ? 1 : 0);
    }

    bool IsSkipped() const
    {
        return m_Flags.isSkipped != 0;
    }

    NotifiersChain * GetNotifiersChain()
    {
        return &m_NotifiersChain;
    }

    void MoveToNotifiersChain(NotifiersChain & notify)
    {
        m_NotifiersChain.MoveNotifiers(&notify);
    }

    bool IsDecoded() const;

    bool IsFrameExist() const
    {
        return m_IsFrameExist;
    }

    void SetFrameExistFlag(bool isFrameExist)
    {
        m_IsFrameExist = isFrameExist;
    }

    // m_pParsedFrameData's allocated size is remembered so that a
    // re-allocation is done only if size requirements exceed the
    // existing allocation.
    // m_paddedParsedFrameDataSize contains the image dimensions,
    // rounded up to a multiple of 16, that were used.

    H264DecoderFrame(MemoryAllocator *pMemoryAllocator, H264_Heap * heap, H264_Heap_Objects * pObjHeap);

    virtual ~H264DecoderFrame();

    // The following methods provide access to the H264Decoder's doubly
    // linked list of H264DecoderFrames.  Note that m_pPreviousFrame can
    // be non-NULL even for an I frame.
    H264DecoderFrame *previous() { return m_pPreviousFrame; }
    H264DecoderFrame *future()   { return m_pFutureFrame; }

    const H264DecoderFrame *previous() const { return m_pPreviousFrame; }
    const H264DecoderFrame *future() const { return m_pFutureFrame; }

    void setPrevious(H264DecoderFrame *pPrev)
    {
        m_pPreviousFrame = pPrev;
    }

    void setFuture(H264DecoderFrame *pFut)
    {
        m_pFutureFrame = pFut;
    }

    bool        isDisplayable() const { return m_isDisplayable != 0; }

    void        SetisDisplayable()
    {
        m_isDisplayable = 1;
    }

    bool IsDecodingStarted() const { return m_Flags.isDecodingStarted != 0;}
    void StartDecoding() { m_Flags.isDecodingStarted = 1;}

    bool IsDecodingCompleted() const { return m_Flags.isDecodingCompleted != 0;}
    void CompleteDecoding();

    void UpdateErrorWithRefFrameStatus();

    void        unSetisDisplayable() { m_isDisplayable = 0; }

    bool        wasDisplayed()    { return m_wasDisplayed != 0; }
    void        setWasDisplayed() { m_wasDisplayed = 1; }
    void        unsetWasDisplayed() { m_wasDisplayed = 0; }

    bool        wasOutputted()    { return m_wasOutputted != 0; }
    void        setWasOutputted();
    void        unsetWasOutputted() { m_wasOutputted = 0; }

    bool        isDisposable()    { return (!m_isShortTermRef[0] &&
                                            !m_isShortTermRef[1] &&
                                            !m_isLongTermRef[0] &&
                                            !m_isLongTermRef[1] &&
                                            ((m_wasOutputted != 0 && m_wasDisplayed != 0) || (m_isDisplayable == 0)) &&
                                            !GetRefCounter()); }

    // A decoded frame can be "disposed" if it is not an active reference
    // and it is not locked by the calling application and it has been
    // output for display.
    Ipp32s PicNum(Ipp32s f, Ipp32s force=0) const
    {
        if ((m_PictureStructureForRef >= FRM_STRUCTURE && force==0) || force==3)
        {
            return IPP_MIN(m_PicNum[0],m_PicNum[1]);
        }

        return m_PicNum[f];
    }

    void setPicNum(Ipp32s picNum, Ipp32s f)
    {
        if (m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            m_PicNum[0] = m_PicNum[1] = picNum;
        }
        else
            m_PicNum[f] = picNum;
    }

    // Updates m_LongTermPicNum for if long term reference, based upon
    // m_LongTermFrameIdx.
    Ipp32s FrameNum() const
    {
        return m_FrameNum;
    }

    void setFrameNum(Ipp32s FrameNum)
    {
        m_FrameNum = FrameNum;
    }

    Ipp32s FrameNumWrap() const
    {
        return m_FrameNumWrap;
    }

    void setFrameNumWrap(Ipp32s FrameNumWrap)
    {
        m_FrameNumWrap = FrameNumWrap;
    }

    void UpdateFrameNumWrap(Ipp32s CurrFrameNum, Ipp32s MaxFrameNum, Ipp32s CurrPicStruct);
    // Updates m_FrameNumWrap and m_PicNum if the frame is a short-term
    // reference and a frame number wrap has occurred.

    Ipp32s LongTermFrameIdx() const
    {
        return m_LongTermFrameIdx;
    }

    void setLongTermFrameIdx(Ipp32s LongTermFrameIdx)
    {
        m_LongTermFrameIdx = LongTermFrameIdx;
    }

    bool isShortTermRef(Ipp32s WhichField) const
    {
        if (m_PictureStructureForRef>=FRM_STRUCTURE )
            return m_isShortTermRef[0] && m_isShortTermRef[1];
        else
            return m_isShortTermRef[WhichField];
    }

    Ipp32s isShortTermRef() const
    {
        return m_isShortTermRef[0] + m_isShortTermRef[1]*2;
    }

    void SetisShortTermRef(bool isRef, Ipp32s WhichField);

    Ipp32s PicOrderCnt(Ipp32s index, Ipp32s force=0) const
    {
        if ((m_PictureStructureForRef>=FRM_STRUCTURE && force==0) || force==3)
        {
            return IPP_MIN(m_PicOrderCnt[0],m_PicOrderCnt[1]);
        }
        else if (force==2)
        {
            if (isShortTermRef(0) && isShortTermRef(1))
                return IPP_MIN(m_PicOrderCnt[0],m_PicOrderCnt[1]);
            else if (isShortTermRef(0))
                return m_PicOrderCnt[0];
            else
                return m_PicOrderCnt[1];
        }
        return m_PicOrderCnt[index];
    }

    size_t DeblockPicID(Ipp32s index) const
    {
        return m_ID[index];
    }

    void setPicOrderCnt(Ipp32s PicOrderCnt, Ipp32s index) {m_PicOrderCnt[index] = PicOrderCnt;}
    bool isLongTermRef(Ipp32s WhichField) const
    {
        if (m_PictureStructureForRef>=FRM_STRUCTURE)
            return m_isLongTermRef[0] && m_isLongTermRef[1];
        else
            return m_isLongTermRef[WhichField];
    }

    Ipp32s isLongTermRef() const
    {
        return m_isLongTermRef[0] + m_isLongTermRef[1]*2;
    }

    void SetisLongTermRef(bool isRef, Ipp32s WhichField);

    Ipp32s LongTermPicNum(Ipp32s f, Ipp32s force=0) const
    {
        if ((m_PictureStructureForRef>=FRM_STRUCTURE && force==0) || force==3)
        {
            return IPP_MIN(m_LongTermPicNum[0],m_LongTermPicNum[1]);
        }
        else if (force==2)
        {
            if (isLongTermRef(0) && isLongTermRef(1))
                return IPP_MIN(m_LongTermPicNum[0],m_LongTermPicNum[1]);
            else if (isLongTermRef(0))
                return m_LongTermPicNum[0];
            else return m_LongTermPicNum[1];
        }
        return m_LongTermPicNum[f];
    }

    void setLongTermPicNum(Ipp32s LongTermPicNum,Ipp32s f) {m_LongTermPicNum[f] = LongTermPicNum;}
    void UpdateLongTermPicNum(Ipp32s CurrPicStruct);

    bool isInterViewRef(Ipp32u WhichField) const
    {
        if (m_PictureStructureForDec>=FRM_STRUCTURE )
            return m_isInterViewRef[0];
        else
            return m_isInterViewRef[WhichField];
    }

    void SetInterViewRef(bool bInterViewRef, Ipp32u WhichField)
    {
        if (m_PictureStructureForDec >= FRM_STRUCTURE)
            m_isInterViewRef[0] = m_isInterViewRef[1] = bInterViewRef;
        else
            m_isInterViewRef[WhichField] = bInterViewRef;
    }

    // set the view_id the frame belongs to
    void setViewId(Ipp32u view_id)
    {
        m_viewId = view_id;
    }

    void IncreaseRefPicListResetCount(Ipp32u f)
    {
        m_RefPicListResetCount[f]++;
    }

    void InitRefPicListResetCount(Ipp32u f)
    {
        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            m_RefPicListResetCount[0]=m_RefPicListResetCount[1]=0;
        else
            m_RefPicListResetCount[f]=0;
    }

    Ipp32u RefPicListResetCount(Ipp32s f) const
    {
        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            return IPP_MAX(m_RefPicListResetCount[0], m_RefPicListResetCount[1]);
        else
            return m_RefPicListResetCount[f];
    }

    Ipp8s GetNumberByParity(Ipp32s parity) const
    {
        VM_ASSERT(!parity || parity == 1);
        return m_bottom_field_flag[1]==parity ? 1 : 0;
    }

    //////////////////////////////////////////////////////////////////////////////
    // GetRefPicList
    // Returns pointer to start of specified ref pic list.
    //////////////////////////////////////////////////////////////////////////////
    H264DecoderRefPicList* GetRefPicList(Ipp32s sliceNumber, Ipp32s list);

    Status allocateParsedFrameData();
    // Reallocate m_pParsedFrameData, if necessary, and initialize the
    // various pointers that point into it.

    size_t GetFrameDataSize(const IppiSize &lumaSize);

    void deallocateParsedFrameData();

    bool IsAuxiliaryFrame() const
    {
        return is_auxiliary_frame;
    }

    void CopyPlanes(H264DecoderFrame *pRefFrame);
    void DefaultFill(Ipp32s field, bool isChromaOnly, Ipp8u defaultValue = 128);

    Ipp32s GetError() const
    {
        return m_ErrorType;
    }

    void SetError(Ipp32s errorType)
    {
        m_ErrorType = errorType;
    }

    void SetErrorFlagged(Ipp32s errorType)
    {
        m_ErrorType |= errorType;
    }


protected:
    // Declare memory management tools
    MemoryAllocator *m_pMemoryAllocator;
    MemID m_midParsedFrameDataNew;

    H264_Heap_Objects * m_pObjHeap;
    H264_Heap * m_pHeap;
};

class H264DecoderFrameExtension : public H264DecoderFrame
{
    DYNAMIC_CAST_DECL(H264DecoderFrameExtension, H264DecoderFrame)

public:
    H264DecoderFrameExtension(MemoryAllocator *pMemoryAllocator, H264_Heap * heap, H264_Heap_Objects * pObjHeap);

    virtual ~H264DecoderFrameExtension();

    H264DecoderFrame * GetAuxiliaryFrame();

    void AllocateAuxiliary();

    void FillInfoToAuxiliary();

private:

    H264DecoderFrame * m_pAuxiliaryFrame;
};

inline H264DecoderFrame * GetAuxiliaryFrame(H264DecoderFrame * frame)
{
    if (frame)
    {
        H264DecoderFrameExtension * fr = DynamicCast<H264DecoderFrameExtension>(frame);
        return fr ? fr->GetAuxiliaryFrame() : 0;
    }

    return 0;
}

inline bool isAlmostDisposable(H264DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef[0] &&
        !pTmp->m_isShortTermRef[1] &&
        !pTmp->m_isLongTermRef[0] &&
        !pTmp->m_isLongTermRef[1] &&
        ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)) &&
        !pTmp->GetRefCounter());
}

} // end namespace UMC

#endif // __UMC_H264_FRAME_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
