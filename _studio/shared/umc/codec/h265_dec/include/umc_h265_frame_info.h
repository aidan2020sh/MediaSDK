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

#ifndef __UMC_H265_FRAME_INFO_H
#define __UMC_H265_FRAME_INFO_H

#include <vector>
#include "umc_h265_frame.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrame;

class H265DecoderFrameInfo
{
public:
    enum FillnessStatus
    {
        STATUS_NONE,
        STATUS_NOT_FILLED,
        STATUS_FILLED,
        STATUS_COMPLETED,
        STATUS_STARTED
    };

    H265DecoderFrameInfo(H265DecoderFrame * pFrame, Heap * heap, Heap_Objects * pObjHeap)
        : m_pFrame(pFrame)
        , m_pHeap(heap)
        , m_pObjHeap(pObjHeap)
        , m_SliceCount(0)
        , m_prepared(0)
    {
        Reset();
    }

    virtual ~H265DecoderFrameInfo()
    {
    }

    H265Slice *GetAnySlice() const
    {
        H265Slice* slice = GetSlice(0);
        if (!slice)
            throw h265_exception(UMC::UMC_ERR_FAILED);
        return slice;
    }

    void AddSlice(H265Slice * pSlice)
    {
        m_refPicList.resize(pSlice->GetSliceNum() + 1);

        m_pSliceQueue.push_back(pSlice);
        m_SliceCount++;

        const H265SliceHeader &sliceHeader = *(pSlice->GetSliceHeader());

        m_IsIntraAU = m_IsIntraAU && (sliceHeader.slice_type == I_SLICE);
        m_isBExist = m_isBExist || (sliceHeader.slice_type == B_SLICE);
        m_isPExist = m_isPExist || (sliceHeader.slice_type == P_SLICE);

        m_IsNeedDeblocking = m_IsNeedDeblocking || (!pSlice->getDeblockingFilterDisable());
    }

    Ipp32u GetSliceCount() const
    {
        return m_SliceCount;
    }

    H265Slice* GetSlice(Ipp32s num) const
    {
        if (num < 0 || num >= m_SliceCount)
            return 0;
        return m_pSliceQueue[num];
    }

    H265Slice* GetSliceByNumber(Ipp32s num) const
    {
        size_t count = m_pSliceQueue.size();
        for (Ipp32u i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return m_pSliceQueue[i];
        }

        return 0;
    }

    Ipp32s GetPositionByNumber(Ipp32s num) const
    {
        size_t count = m_pSliceQueue.size();
        for (Ipp32u i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return i;
        }

        return -1;
    }

    void Reset()
    {
        Free();

        m_iDecMBReady = 0;
        m_iRecMBReady = 0;
        m_IsNeedDeblocking = false;

        m_IsReferenceAU = false;
        m_IsIntraAU = true;
        m_isPExist = false;
        m_isBExist = false;

        m_NextAU = 0;
        m_PrevAU = 0;
        m_RefAU = 0;

        m_Status = STATUS_NONE;
        m_prepared = 0;
    }

    void SetStatus(FillnessStatus status)
    {
        m_Status = status;
    }

    FillnessStatus GetStatus () const
    {
        return m_Status;
    }

    void Free()
    {
        size_t count = m_pSliceQueue.size();
        for (size_t i = 0; i < count; i ++)
        {
            H265Slice * pCurSlice = m_pSliceQueue[i];
            pCurSlice->Release();
            pCurSlice->DecrementReference();
        }

        m_SliceCount = 0;

        m_pSliceQueue.clear();
        m_prepared = 0;
    }

    void RemoveSlice(Ipp32s num)
    {
        H265Slice * pCurSlice = GetSlice(num);

        if (!pCurSlice) // nothing to do
            return;

        for (Ipp32s i = num; i < m_SliceCount - 1; i++)
        {
            m_pSliceQueue[i] = m_pSliceQueue[i + 1];
        }

        m_SliceCount--;
        m_pSliceQueue[m_SliceCount] = pCurSlice;
    }

    bool IsNeedDeblocking () const
    {
        return m_IsNeedDeblocking;
    }

    void SkipDeblocking()
    {
        m_IsNeedDeblocking = false;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H265Slice *pSlice = m_pSliceQueue[i];

            pSlice->m_bDeblocked = true;
            pSlice->m_bDebVacant = 0;
            pSlice->m_iCurMBToDeb = pSlice->m_iMaxMB;
            pSlice->setDeblockingFilterDisable(true);
        }
    }

    void SkipSAO()
    {
        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H265Slice *pSlice = m_pSliceQueue[i];
            pSlice->m_bSAOed = true;
        }
    }

    bool IsCompleted() const
    {
        if (GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
            return true;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            const H265Slice *pSlice = m_pSliceQueue[i];

            if (!pSlice->m_bDecoded || !pSlice->m_bDeblocked || !pSlice->m_bSAOed)
                return false;
        }

        return true;
    }

    bool IsIntraAU() const
    {
        return m_IsIntraAU;
    }

    bool IsReference() const
    {
        return m_IsReferenceAU;
    }

#if (HEVC_OPT_CHANGES & 2)
// ML: OPT: compilers do not seem to honor implicit 'inline' for this one
    __forceinline 
#endif
    H265DecoderRefPicList* GetRefPicList(Ipp32u sliceNumber, Ipp32s list)
    {
        VM_ASSERT(list <= LIST_1 && list >= 0);

        if (sliceNumber >= m_refPicList.size())
        {
            return 0;
        }

        return &m_refPicList[sliceNumber].m_refPicList[list];
    };

    bool CheckReferenceFrameError()
    {
        Ipp32u checkedErrorMask = UMC::ERROR_FRAME_MINOR | UMC::ERROR_FRAME_MAJOR | UMC::ERROR_FRAME_REFERENCE_FRAME;
        for (size_t i = 0; i < m_refPicList.size(); i ++)
        {
            H265DecoderRefPicList* list = &m_refPicList[i].m_refPicList[LIST_0];
            for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
            {
                if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                    return true;
            }

            list = &m_refPicList[i].m_refPicList[LIST_1];
            for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
            {
                if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                    return true;
            }
        }

        return false;
    }

    H265DecoderFrameInfo * GetNextAU() {return m_NextAU;}
    H265DecoderFrameInfo * GetPrevAU() {return m_PrevAU;}
    H265DecoderFrameInfo * GetRefAU() {return m_RefAU;}

    void SetNextAU(H265DecoderFrameInfo *au) {m_NextAU = au;}
    void SetPrevAU(H265DecoderFrameInfo *au) {m_PrevAU = au;}
    void SetRefAU(H265DecoderFrameInfo *au) {m_RefAU = au;}

    Ipp32s m_iDecMBReady;
    Ipp32s m_iRecMBReady;
    H265DecoderFrame * m_pFrame;
    Ipp32s m_prepared;

    bool m_isBExist;
    bool m_isPExist;

private:

    FillnessStatus m_Status;

    std::vector<H265Slice*> m_pSliceQueue;

    Ipp32s m_SliceCount;

    Heap *m_pHeap;
    Heap_Objects * m_pObjHeap;
    bool m_IsNeedDeblocking;

    bool m_IsReferenceAU;
    bool m_IsIntraAU;

    H265DecoderFrameInfo *m_NextAU;
    H265DecoderFrameInfo *m_RefAU;
    H265DecoderFrameInfo *m_PrevAU;

    struct H264DecoderRefPicListPair
    {
    public:
        H265DecoderRefPicList m_refPicList[2];
    };

    std::vector<H264DecoderRefPicListPair> m_refPicList;

    class FakeFrameInitializer
    {
    public:
        FakeFrameInitializer();
    };

    static FakeFrameInitializer g_FakeFrameInitializer;
};

#if (HEVC_OPT_CHANGES & 2)
// ML: OPT: called in hopspot loops
// ML: OPT: moved here from umc_h265_frame.cpp to allow inlining, cannot be decalred in umc_h265_frame.h due to forward H265DecoderRefPicList dependency
__forceinline H265DecoderRefPicList* H265DecoderFrame::GetRefPicList(Ipp32s sliceNumber, Ipp32s list)
{
    H265DecoderRefPicList *pList;
    pList = GetAU()->GetRefPicList(sliceNumber, list);
    return pList;
}   // RefPicList. Returns pointer to start of specified ref pic list.

#endif
} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_FRAME_INFO_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
