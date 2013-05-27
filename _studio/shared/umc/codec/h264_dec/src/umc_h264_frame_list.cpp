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

#include "umc_h264_frame_list.h"
#include "umc_h264_dec_debug.h"
#include "umc_h264_task_supplier.h"

namespace UMC
{

H264DecoderFrameList::H264DecoderFrameList(void)
{
    m_pHead = NULL;
    m_pTail = NULL;
} // H264DecoderFrameList::H264DecoderFrameList(void)

H264DecoderFrameList::~H264DecoderFrameList(void)
{
    Release();

} // H264DecoderFrameList::~H264DecoderFrameList(void)

void H264DecoderFrameList::Release(void)
{
    // destroy frame list
    while (m_pHead)
    {
        H264DecoderFrame *pNext = m_pHead->future();
        delete m_pHead;
        m_pHead = pNext;
    }

    m_pHead = NULL;
    m_pTail = NULL;

} // void H264DecoderFrameList::Release(void)


//////////////////////////////////////////////////////////////////////////////
// append
//   Appends a new decoded frame buffer to the "end" of the linked list
//////////////////////////////////////////////////////////////////////////////
void H264DecoderFrameList::append(H264DecoderFrame *pFrame)
{
    // Error check
    if (!pFrame)
    {
        // Sent in a NULL frame
        return;
    }

    // Has a list been constructed - is their a head?
    if (!m_pHead)
    {
        // Must be the first frame appended
        // Set the head to the current
        m_pHead = pFrame;
        m_pHead->setPrevious(0);
    }

    if (m_pTail)
    {
        // Set the old tail as the previous for the current
        pFrame->setPrevious(m_pTail);

        // Set the old tail's future to the current
        m_pTail->setFuture(pFrame);
    }
    else
    {
        // Must be the first frame appended
        // Set the tail to the current
        m_pTail = pFrame;
    }

    // The current is now the new tail
    m_pTail = pFrame;
    m_pTail->setFuture(0);
    //
}

void H264DecoderFrameList::swapFrames(H264DecoderFrame *pFrame1, H264DecoderFrame *pFrame2)
{
    H264DecoderFrame * temp1 = pFrame1->previous();
    H264DecoderFrame * temp2 = pFrame2->previous();

    if (temp1)
    {
        temp1->setFuture(pFrame2);
    }

    if (temp2)
    {
        temp2->setFuture(pFrame1);
    }

    pFrame1->setPrevious(temp2);
    pFrame2->setPrevious(temp1);

    temp1 = pFrame1->future();
    temp2 = pFrame2->future();

    if (temp1)
    {
        temp1->setPrevious(pFrame2);
    }

    if (temp2)
    {
        temp2->setPrevious(pFrame1);
    }

    pFrame1->setFuture(temp2);
    pFrame2->setFuture(temp1);


    if (m_pHead == pFrame1)
    {
        m_pHead = pFrame2;
    }
    else
    {
        if (m_pHead == pFrame2)
        {
            m_pHead = pFrame1;
        }
    }

    if (m_pTail == pFrame1)
    {
        m_pTail = pFrame2;
    }
    else
    {
        if (m_pTail == pFrame2)
        {
            m_pTail = pFrame1;
        }
    }
}

H264DBPList::H264DBPList()
    : m_dpbSize(0)
    , m_recovery_frame_cnt(-1)
    , m_wasRecoveryPointFound(false)
{
}

H264DecoderFrame * H264DBPList::GetOldestDisposable(void)
{
    H264DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32u  LargestRefPicListResetCount = 0;

    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            if (pTmp->RefPicListResetCount(0) > LargestRefPicListResetCount)
            {
                pOldest = pTmp;
                SmallestPicOrderCnt = pTmp->PicOrderCnt(0,3);
                LargestRefPicListResetCount = pTmp->RefPicListResetCount(0);
            }
            else if ((pTmp->PicOrderCnt(0,3) < SmallestPicOrderCnt) &&
                     (pTmp->RefPicListResetCount(0) == LargestRefPicListResetCount))
            {
                pOldest = pTmp;
                SmallestPicOrderCnt = pTmp->PicOrderCnt(0,3);
            }
        }
    }

    return pOldest;
} // H264DecoderFrame *H264DBPList::GetDisposable(void)

H264DecoderFrame * H264DBPList::GetLastDisposable(void)
{
    H264DecoderFrame *pOldest = NULL;

    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            pOldest = pTmp;
        }
    }
    return pOldest;
} // H264DecoderFrame *H264DBPList::GetDisposable(void)

void H264DBPList::MoveToTail(H264DecoderFrame * pFrame)
{
    if (m_pHead == m_pTail || pFrame == m_pTail)
    {
        return;
    }

    // move to tail
    if (pFrame == m_pHead)
    {
        m_pHead = pFrame->m_pFutureFrame;
        m_pHead->m_pPreviousFrame = 0;
    }
    else
    {
        pFrame->m_pPreviousFrame->m_pFutureFrame = pFrame->m_pFutureFrame;
        pFrame->m_pFutureFrame->m_pPreviousFrame = pFrame->m_pPreviousFrame;
    }

    m_pTail->m_pFutureFrame = pFrame;
    pFrame->m_pPreviousFrame = m_pTail;
    m_pTail = pFrame;
    pFrame->m_pFutureFrame = 0;
}

bool H264DBPList::IsDisposableExist()
{
    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            return true;
        }
    }

    return false;
}

bool H264DBPList::IsAlmostDisposableExist()
{
    Ipp32s count = 0;
    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        count++;
        if (isAlmostDisposable(pTmp))
        {
            return true;
        }
    }

    return count < m_dpbSize;
}

H264DecoderFrame * H264DBPList::GetDisposable(void)
{
    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            return pTmp;
        }
    }

    // We never found one
    return NULL;
} // H264DecoderFrame *H264DBPList::GetDisposable(void)

H264DecoderFrame * H264DBPList::findDisplayableByDPBDelay(void)
{
    H264DecoderFrame *pCurr = m_pHead;
    H264DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32u  LargestRefPicListResetCount = 0;

    Ipp32s count = 0;
    while (pCurr)
    {
        if ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted() && !pCurr->m_dpb_output_delay)
        {
            // corresponding frame
            if (pCurr->RefPicListResetCount(0) > LargestRefPicListResetCount)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt(0,3);
                LargestRefPicListResetCount = pCurr->RefPicListResetCount(0);
            }
            else if (pCurr->RefPicListResetCount(0) == LargestRefPicListResetCount && 
                pCurr->PicOrderCnt(0,3) <= SmallestPicOrderCnt)
            {
                VM_ASSERT(pCurr->m_UID != -1);
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt(0,3);
            }

            if (!pOldest->IsFrameExist() && pCurr->IsFrameExist())
            {
                if (pCurr->PicOrderCnt(0,3) == SmallestPicOrderCnt &&
                    pCurr->RefPicListResetCount(0) == LargestRefPicListResetCount)
                    pOldest = pCurr;
            }

            //if (count == dbpSize + 1)
              //  break;
            count++;
        }

        pCurr = pCurr->future();
    }

    // may be OK if NULL
    return pOldest;
}

///////////////////////////////////////////////////////////////////////////////
// findOldestDisplayable
// Search through the list for the oldest displayable frame. It must be
// not disposable, not outputted, and have smallest PicOrderCnt.
///////////////////////////////////////////////////////////////////////////////
H264DecoderFrame * H264DBPList::findOldestDisplayable(Ipp32s /*dbpSize*/ )
{
    H264DecoderFrame *pCurr = m_pHead;
    H264DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32u  LargestRefPicListResetCount = 0;

    Ipp32s count = 0;
    while (pCurr)
    {
        if ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted())
        {
            // corresponding frame
            if (pCurr->RefPicListResetCount(0) > LargestRefPicListResetCount)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt(0,3);
                LargestRefPicListResetCount = pCurr->RefPicListResetCount(0);
            }
            else if ((pCurr->PicOrderCnt(0,3) <= SmallestPicOrderCnt) &&
                     (pCurr->RefPicListResetCount(0) == LargestRefPicListResetCount))
            {
                VM_ASSERT(pCurr->m_UID != -1);
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt(0,3);
            }

            if (!pOldest->IsFrameExist() && pCurr->IsFrameExist())
            {
                if (pCurr->PicOrderCnt(0,3) == SmallestPicOrderCnt &&
                    pCurr->RefPicListResetCount(0) == LargestRefPicListResetCount)
                    pOldest = pCurr;
            }

            //if (count == dbpSize + 1)
              //  break;
            count++;
        }

        pCurr = pCurr->future();
    }

    // may be OK if NULL
    return pOldest;

}    // findOldestDisplayable

H264DecoderFrame * H264DBPList::findFirstDisplayable()
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if ((pCurr->isDisplayable() || !pCurr->IsDecoded()) && !pCurr->wasOutputted())
        {
            return pCurr;
        }

        pCurr = pCurr->future();
    }

    return 0;
}    // findFirstDisplayable

Ipp32u H264DBPList::countAllFrames()
{
    H264DecoderFrame *pCurr = head();
    Ipp32u count = 0;

    while (pCurr)
    {
        count++;
        pCurr = pCurr->future();
    }

    return count;
}

///////////////////////////////////////////////////////////////////////////////
// countNumDisplayable
//  Return number of displayable frames.
///////////////////////////////////////////////////////////////////////////////
Ipp32u H264DBPList::countNumDisplayable()
{
    H264DecoderFrame *pCurr = head();
    Ipp32u NumDisplayable = 0;

    while (pCurr)
    {
        if (((pCurr->isShortTermRef() || pCurr->isLongTermRef()) && pCurr->IsFullFrame()) || ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted()))
            NumDisplayable++;
        pCurr = pCurr->future();
    }

    return NumDisplayable;
}    // countNumDisplayable

///////////////////////////////////////////////////////////////////////////////
// countActiveRefs
//  Return number of active Ipp16s and long term reference frames.
///////////////////////////////////////////////////////////////////////////////
void H264DBPList::countActiveRefs(Ipp32u &NumShortTerm, Ipp32u &NumLongTerm)
{
    H264DecoderFrame *pCurr = m_pHead;
    NumShortTerm = 0;
    NumLongTerm = 0;

    while (pCurr)
    {
        if (pCurr->isShortTermRef())
            NumShortTerm++;
        else if (pCurr->isLongTermRef())
            NumLongTerm++;
        pCurr = pCurr->future();
    }

}    // countActiveRefs

///////////////////////////////////////////////////////////////////////////////
// removeAllRef
// Marks all frames as not used as reference frames.
///////////////////////////////////////////////////////////////////////////////
void H264DBPList::removeAllRef()
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
        {
            pCurr->SetisLongTermRef(false, 0);
            pCurr->SetisLongTermRef(false, 1);
            pCurr->SetisShortTermRef(false, 0);
            pCurr->SetisShortTermRef(false, 1);

            OnSlideWindow(pCurr);
        }

        pCurr = pCurr->future();
    }

}    // removeAllRef

void H264DBPList::IncreaseRefPicListResetCount(H264DecoderFrame *ExcludeFrame)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr!=ExcludeFrame)
        {
            pCurr->IncreaseRefPicListResetCount(0);
            pCurr->IncreaseRefPicListResetCount(1);
        }
        pCurr = pCurr->future();
    }

}    // IncreaseRefPicListResetCount

H264DecoderFrame * H264DBPList::findOldestShortTermRef()
{
    H264DecoderFrame *pCurr = m_pHead;
    H264DecoderFrame *pOldest = 0;
    Ipp32s  SmallestFrameNumWrap = 0x0fffffff;    // very large positive

    while (pCurr)
    {
        if (pCurr->isShortTermRef() && (pCurr->FrameNumWrap() < SmallestFrameNumWrap))
        {
            pOldest = pCurr;
            SmallestFrameNumWrap = pCurr->FrameNumWrap();
        }
        pCurr = pCurr->future();
    }

    return pOldest;
}    // findOldestShortTermRef

H264DecoderFrame * H264DBPList::findOldestLongTermRef()
{
    H264DecoderFrame *pCurr = m_pHead;
    H264DecoderFrame *pOldest = 0;
    Ipp32s  SmallestFrameNumWrap = 0x0fffffff;    // very large positive

    while (pCurr)
    {
        if (pCurr->isLongTermRef() && (pCurr->FrameNumWrap() < SmallestFrameNumWrap))
        {
            pOldest = pCurr;
            SmallestFrameNumWrap = pCurr->FrameNumWrap();
        }
        pCurr = pCurr->future();
    }

    return pOldest;
}    // findOldestLongTermRef

///////////////////////////////////////////////////////////////////////////////
// freeShortTermRef
// Mark the short-term reference frame with specified picNum as not used
///////////////////////////////////////////////////////////////////////////////
H264DecoderFrame * H264DBPList::freeShortTermRef(Ipp32s  picNum)
{
    H264DecoderFrame *pCurr = m_pHead;
    bool found = false;
    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if (pCurr->isShortTermRef() && (pCurr->PicNum(0) == picNum))
            {
                pCurr->SetisShortTermRef(false, 0);
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isShortTermRef(0) && (pCurr->PicNum(0) == picNum))
            {
                pCurr->SetisShortTermRef(false, 0);
                found = true;
            }

            if (pCurr->isShortTermRef(1) && (pCurr->PicNum(1) == picNum))
            {
                pCurr->SetisShortTermRef(false, 1);
                found = true;
            }

            if (found)
                return pCurr;
        }

        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);
    return 0;
}    // freeShortTermRef

///////////////////////////////////////////////////////////////////////////////
// freeLongTermRef
// Mark the long-term reference frame with specified LongTermPicNum as not used
///////////////////////////////////////////////////////////////////////////////
H264DecoderFrame * H264DBPList::freeLongTermRef(Ipp32s  LongTermPicNum)
{
    H264DecoderFrame *pCurr = m_pHead;
    bool found = false;

    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef>=FRM_STRUCTURE)
        {
            if (pCurr->isLongTermRef() && (pCurr->LongTermPicNum(0) == LongTermPicNum))
            {
                pCurr->SetisLongTermRef(false, 0);
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isLongTermRef(0) && (pCurr->LongTermPicNum(0) == LongTermPicNum))
            {
                pCurr->SetisLongTermRef(false, 0);
                found = true;
            }

            if (pCurr->isLongTermRef(1) && (pCurr->LongTermPicNum(1) == LongTermPicNum))
            {
                pCurr->SetisLongTermRef(false, 1);
                found = true;
            }

            if (found)
                return pCurr;
        }

        pCurr = pCurr->future();
    }


    //VM_ASSERT(false);    // No match found, should not happen.
    return 0;
}    // freeLongTermRef

///////////////////////////////////////////////////////////////////////////////
// freeLongTermRef
// Mark the long-term reference frame with specified LongTermFrameIdx
// as not used
///////////////////////////////////////////////////////////////////////////////
H264DecoderFrame * H264DBPList::freeLongTermRefIdx(Ipp32s  LongTermFrameIdx, H264DecoderFrame * pCurrentFrame)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if (pCurr->isLongTermRef() && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                if (pCurrentFrame == pCurr)
                    return 0;
                pCurr->SetisLongTermRef(false, 0);
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isLongTermRef(0) && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                if (pCurrentFrame == pCurr)
                    return 0;
                pCurr->SetisLongTermRef(false, 0);
                pCurr->SetisLongTermRef(false, 1);
                return pCurr;
            }

            if (pCurr->isLongTermRef(1) && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                if (pCurrentFrame == pCurr)
                    return 0;
                pCurr->SetisLongTermRef(false, 0);
                pCurr->SetisLongTermRef(false, 1);
                return pCurr;
            }
        }

        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);
    return 0;
}    // freeLongTermRefIdx

H264DecoderFrame * H264DBPList::findLongTermRefIdx(Ipp32s LongTermFrameIdx)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if (pCurr->isLongTermRef() && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isLongTermRef(0) && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                return pCurr;
            }

            if (pCurr->isLongTermRef(1) && (pCurr->LongTermFrameIdx() == LongTermFrameIdx ))
            {
                return pCurr;
            }
        }

        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// freeOldLongTermRef
// Mark any long-term reference frame with LongTermFrameIdx greater
// than MaxLongTermFrameIdx as not used. When MaxLongTermFrameIdx is -1, this
// indicates no long-term frame indices and all long-term reference
// frames should be freed.
///////////////////////////////////////////////////////////////////////////////
H264DecoderFrame * H264DBPList::freeOldLongTermRef(Ipp32s  MaxLongTermFrameIdx)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->isLongTermRef(0) && (pCurr->LongTermFrameIdx() > MaxLongTermFrameIdx))
        {
            pCurr->SetisLongTermRef(false, 0);
            pCurr->SetisLongTermRef(false, 1);

            OnSlideWindow(pCurr);
        }

        pCurr = pCurr->future();
    }

    return 0;
    // OK to not find any to free

}    // freeOldLongTermRef

H264DecoderFrame * H264DBPList::findOldLongTermRef(Ipp32s MaxLongTermFrameIdx)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->isLongTermRef(0) && (pCurr->LongTermFrameIdx() > MaxLongTermFrameIdx))
        {
            return pCurr;
        }

        pCurr = pCurr->future();
    }

    return 0;
    // OK to not find any to free

}

///////////////////////////////////////////////////////////////////////////////
// changeSTtoLTRef
//    Mark the short-term reference frame with specified PicNum as long-term
//  with specified long term idx.
///////////////////////////////////////////////////////////////////////////////
void H264DBPList::changeSTtoLTRef(Ipp32s  picNum, Ipp32s  longTermFrameIdx)
{
    H264DecoderFrame *pCurr = m_pHead;
    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if (pCurr->isShortTermRef() && (pCurr->PicNum(0) == picNum))
            {
                pCurr->SetisShortTermRef(false, 0);
                pCurr->setLongTermFrameIdx(longTermFrameIdx);
                pCurr->SetisLongTermRef(true, 0);
                pCurr->UpdateLongTermPicNum(2);
                return;
            }
        }
        else
        {
            if (pCurr->isShortTermRef(0) && (pCurr->PicNum(0) == picNum))
            {
                pCurr->SetisShortTermRef(false, 0);
                pCurr->setLongTermFrameIdx(longTermFrameIdx);
                pCurr->SetisLongTermRef(true, 0);
                pCurr->UpdateLongTermPicNum(pCurr->m_bottom_field_flag[0]);
                return;
            }

            if (pCurr->isShortTermRef(1) && (pCurr->PicNum(1) == picNum))
            {
                pCurr->SetisShortTermRef(false, 1);
                pCurr->setLongTermFrameIdx(longTermFrameIdx);
                pCurr->SetisLongTermRef(true, 1);
                pCurr->UpdateLongTermPicNum(pCurr->m_bottom_field_flag[1]);
                return;
            }
        }
        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);    // No match found, should not happen.
    return;
}    // changeSTtoLTRef

H264DecoderFrame *H264DBPList::findShortTermPic(Ipp32s  picNum, Ipp32s * field)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if ((pCurr->isShortTermRef() == 3) && (pCurr->PicNum(0) == picNum))
            {
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isShortTermRef(0) && (pCurr->PicNum(0) == picNum))
            {
                if (field)
                    *field = 0;
                return pCurr;
            }

            if (pCurr->isShortTermRef(1) && (pCurr->PicNum(1) == picNum))
            {
                if (field)
                    *field = 1;
                return pCurr;
            }
        }

        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);    // No match found, should not happen.
    return 0;
}    // findShortTermPic

H264DecoderFrame *H264DBPList::findLongTermPic(Ipp32s  picNum, Ipp32s * field)
{
    H264DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            if ((pCurr->isLongTermRef() == 3) && (pCurr->LongTermPicNum(0) == picNum))
            {
                return pCurr;
            }
        }
        else
        {
            if (pCurr->isLongTermRef(0) && (pCurr->LongTermPicNum(0) == picNum))
            {
                *field = 0;
                return pCurr;
            }

            if (pCurr->isLongTermRef(1) && (pCurr->LongTermPicNum(1) == picNum))
            {
                *field = 1;
                return pCurr;
            }
        }

        pCurr = pCurr->future();
    }

    //VM_ASSERT(false);    // No match found, should not happen.
    return 0;
}    // findLongTermPic

H264DecoderFrame *H264DBPList::findInterViewRef(Ipp32s auIndex, Ipp32u bottomFieldFlag)
{
    H264DecoderFrame *pCurr = m_pHead;
    while (pCurr)
    {
        if (pCurr->m_auIndex == auIndex && pCurr->m_Flags.isActive)
        {
            Ipp32u fieldIdx = pCurr->GetNumberByParity(bottomFieldFlag);
            return pCurr->isInterViewRef(fieldIdx) ? pCurr : 0;
        }

        // get the next frame
        pCurr = pCurr->future();
    }

    return 0;

}

H264DecoderFrame * H264DBPList::FindClosest(H264DecoderFrame * pFrame)
{
    Ipp32s originalPOC = pFrame->PicOrderCnt(0, 3);
    Ipp32u originalResetCount = pFrame->RefPicListResetCount(0);

    H264DecoderFrame * pOldest = 0;

    Ipp32s  SmallestPicOrderCnt = 0;    // very large positive
    Ipp32u  SmallestRefPicListResetCount = 0x7fffffff;

    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->IsSkipped() || pTmp == pFrame || pTmp->GetRefCounter() >= 2)
            continue;

        if (pTmp->m_chroma_format != pFrame->m_chroma_format ||
            pTmp->lumaSize().width != pFrame->lumaSize().width ||
            pTmp->lumaSize().height != pFrame->lumaSize().height)
            continue;

        if (pTmp->RefPicListResetCount(0) < SmallestRefPicListResetCount)
        {
            pOldest = pTmp;
            SmallestPicOrderCnt = pTmp->PicOrderCnt(0,3);
            SmallestRefPicListResetCount = pTmp->RefPicListResetCount(0);
        }
        else if (pTmp->RefPicListResetCount(0) == SmallestRefPicListResetCount)
        {
            if (pTmp->RefPicListResetCount(0) == originalResetCount)
            {
                if (SmallestRefPicListResetCount != originalResetCount)
                {
                    SmallestPicOrderCnt = 0x7fff;
                }

                if (abs(pTmp->PicOrderCnt(0,3) - originalPOC) < SmallestPicOrderCnt)
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt(0,3);
                    SmallestRefPicListResetCount = pTmp->RefPicListResetCount(0);
                }
            }
            else
            {
                if (pTmp->PicOrderCnt(0,3) > SmallestPicOrderCnt)
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt(0,3);
                    SmallestRefPicListResetCount = pTmp->RefPicListResetCount(0);
                }
            }
        }
    }

    return pOldest;
}

H264DecoderFrame * H264DBPList::FindByIndex(Ipp32s index)
{
    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->m_index == index)
            return pTmp;
    }

    return 0;
}

Ipp32s H264DBPList::GetRecoveryFrameCnt() const
{
    return m_recovery_frame_cnt;
}

void H264DBPList::SetRecoveryFrameCnt(Ipp32s recovery_frame_cnt)
{
    if (m_wasRecoveryPointFound && recovery_frame_cnt != -1)
    {
        return;
    }

    m_recovery_frame_cnt = recovery_frame_cnt;
    m_wasRecoveryPointFound = true;
}

void H264DBPList::Reset(void)
{
    H264DecoderFrame *pFrame ;

    for (pFrame = head(); pFrame; pFrame = pFrame->future())
    {
        pFrame->FreeResources();
    }

    for (pFrame = head(); pFrame; pFrame = pFrame->future())
    {
        pFrame->Reset();
    }

    m_wasRecoveryPointFound = false;
    m_recovery_frame_cnt = -1;

} // void H264DBPList::Reset(void)

void H264DBPList::InitPSliceRefPicList(H264Slice *slice, H264DecoderFrame **pRefPicList)
{
    Ipp32s j, k;
    Ipp32s NumFramesInList;
    H264DecoderFrame *pHead = head();
    H264DecoderFrame *pFrm;
    Ipp32s picNum;
    bool bError = false;
    bool bIsFieldSlice = (slice->GetSliceHeader()->field_pic_flag != 0);

    VM_ASSERT(pRefPicList);

    if (slice->GetSliceHeader()->is_auxiliary)
    {
        pHead = GetAuxiliaryFrame(pHead);
    }

    NumFramesInList = 0;

    if (!bIsFieldSlice)
    {
        // Frame. Ref pic list ordering: Short term largest pic num to
        // smallest, followed by long term, largest long term pic num to
        // smallest. Note that ref pic list has one extra slot to assist
        // with re-ordering.
        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            if (pFrm->isShortTermRef()==3)
            {
                // add to ordered list
                picNum = pFrm->PicNum(0);

                // find insertion point
                j=0;
                while (j<NumFramesInList &&
                        pRefPicList[j]->isShortTermRef() &&
                        pRefPicList[j]->PicNum(0) > picNum)
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k=NumFramesInList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the short-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
            else if (pFrm->isLongTermRef()==3)
            {
                // add to ordered list
                picNum = pFrm->LongTermPicNum(0,3);

                // find insertion point
                j=0;
                // Skip past short-term refs and long term refs with smaller
                // long term pic num
                while (j<NumFramesInList &&
                        (pRefPicList[j]->isShortTermRef() ||
                        (pRefPicList[j]->isLongTermRef() &&
                        pRefPicList[j]->LongTermPicNum(0,2) < picNum)))
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k=NumFramesInList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the long-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
            if (bError) break;
        }
    }
    else
    {
        // TBD: field
        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            if (pFrm->isShortTermRef())
            {
                // add to ordered list
                picNum = pFrm->FrameNumWrap();

                // find insertion point
                j=0;
                while (j<NumFramesInList &&
                    pRefPicList[j]->isShortTermRef() &&
                    pRefPicList[j]->FrameNumWrap() > picNum)
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k=NumFramesInList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the short-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
            else if (pFrm->isLongTermRef())
            {
                // long term reference
                picNum = pFrm->LongTermPicNum(0,2);

                // find insertion point
                j=0;
                // Skip past short-term refs and long term refs with smaller
                // long term pic num
                while (j<NumFramesInList &&
                    (pRefPicList[j]->isShortTermRef() ||
                    (pRefPicList[j]->isLongTermRef() &&
                    pRefPicList[j]->LongTermPicNum(0,2) < picNum)))
                    j++;

                // make room if needed
                if (pRefPicList[j])
                {
                    for (k=NumFramesInList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        pRefPicList[k] = pRefPicList[k-1];
                    }
                }

                // add the long-term reference
                pRefPicList[j] = pFrm;
                NumFramesInList++;
            }
            if (bError) break;
        }
    }
}

void H264DBPList::InitBSliceRefPicLists(H264Slice *slice, H264DecoderFrame **pRefPicList0, H264DecoderFrame **pRefPicList1,
                                      H264RefListInfo &rli)
{
    bool bIsFieldSlice = (slice->GetSliceHeader()->field_pic_flag != 0);
    Ipp32s i, j, k;
    Ipp32s NumFramesInL0List;
    Ipp32s NumFramesInL1List;
    Ipp32s NumFramesInLTList;
    H264DecoderFrame *pHead = head();
    H264DecoderFrame *pFrm;
    Ipp32s FrmPicOrderCnt;
    H264DecoderFrame *LTRefPicList[MAX_NUM_REF_FRAMES];    // temp storage for long-term ordered list
    Ipp32s LongTermPicNum;
    bool bError = false;

    if (slice->GetSliceHeader()->is_auxiliary)
    {
        pHead = GetAuxiliaryFrame(pHead);
    }

    for (i=0; i<MAX_NUM_REF_FRAMES; i++)
    {
        LTRefPicList[i] = 0;
    }

    NumFramesInL0List = 0;
    NumFramesInL1List = 0;
    NumFramesInLTList = 0;

    if (!bIsFieldSlice)
    {
        Ipp32s CurrPicOrderCnt = slice->GetCurrentFrame()->PicOrderCnt(0);
        // Short term references:
        // Need L0 and L1 lists. Both contain 2 sets of reference frames ordered
        // by PicOrderCnt. The "previous" set contains the reference frames with
        // a PicOrderCnt < current frame. The "future" set contains the reference
        // frames with a PicOrderCnt > current frame. In both cases the ordering
        // is from closest to current frame to farthest. L0 has the previous set
        // followed by the future set; L1 has the future set followed by the previous set.
        // Accomplish this by one pass through the decoded frames list creating
        // the ordered previous list in the L0 array and the ordered future list
        // in the L1 array. Then copy from both to the other for the second set.

        // Long term references:
        // The ordered list is the same for L0 and L1, is ordered by ascending
        // LongTermPicNum. The ordered list is created using local temp then
        // appended to the L0 and L1 lists after the short-term references.

        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            if (pFrm->isShortTermRef()==3)
            {
                // add to ordered list
                FrmPicOrderCnt = pFrm->PicOrderCnt(0,3);

                if (FrmPicOrderCnt < CurrPicOrderCnt)
                {
                    // Previous reference to L0, order large to small
                    j=0;
                    while (j < NumFramesInL0List &&
                        (pRefPicList0[j]->PicOrderCnt(0,3) > FrmPicOrderCnt))
                        j++;

                    // make room if needed
                    if (pRefPicList0[j])
                    {
                        for (k = NumFramesInL0List; k > j; k--)
                        {
                            // Avoid writing beyond end of list
                            if (k > MAX_NUM_REF_FRAMES-1)
                            {
                                VM_ASSERT(0);
                                bError = true;
                                break;
                            }
                            pRefPicList0[k] = pRefPicList0[k-1];
                        }
                    }

                    // add the short-term reference
                    pRefPicList0[j] = pFrm;
                    NumFramesInL0List++;
                }
                else
                {
                    // Future reference to L1, order small to large
                    j=0;
                    while (j<NumFramesInL1List &&
                            pRefPicList1[j]->PicOrderCnt(0,3) < FrmPicOrderCnt)
                        j++;

                    // make room if needed
                    if (pRefPicList1[j])
                    {
                        for (k=NumFramesInL1List; k>j; k--)
                        {
                            // Avoid writing beyond end of list
                            if (k > MAX_NUM_REF_FRAMES-1)
                            {
                                VM_ASSERT(0);
                                bError = true;
                                break;
                            }
                            pRefPicList1[k] = pRefPicList1[k-1];
                        }
                    }

                    // add the short-term reference
                    pRefPicList1[j] = pFrm;
                    NumFramesInL1List++;
                }
            }    // short-term B
            else if (pFrm->isLongTermRef()==3)
            {
                // long term reference
                LongTermPicNum = pFrm->LongTermPicNum(0,3);

                // order smallest to largest
                j=0;
                while (j<NumFramesInLTList &&
                        LTRefPicList[j]->LongTermPicNum(0) < LongTermPicNum)
                    j++;

                // make room if needed
                if (LTRefPicList[j])
                {
                    for (k=NumFramesInLTList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        LTRefPicList[k] = LTRefPicList[k-1];
                    }
                }

                // add the long-term reference
                LTRefPicList[j] = pFrm;
                NumFramesInLTList++;

            }    // long term reference

            if (bError) break;

        }    // for pFrm

        if ((NumFramesInL0List + NumFramesInL1List + NumFramesInLTList) < MAX_NUM_REF_FRAMES)
        {
            // Complete L0 and L1 lists
            // Add future short-term references to L0 list, after previous
            for (i=0; i<NumFramesInL1List; i++)
                pRefPicList0[NumFramesInL0List+i] = pRefPicList1[i];

            // Add previous short-term references to L1 list, after future
            for (i=0; i<NumFramesInL0List; i++)
                pRefPicList1[NumFramesInL1List+i] = pRefPicList0[i];

            // Add long term list to both L0 and L1
            for (i=0; i<NumFramesInLTList; i++)
            {
                pRefPicList0[NumFramesInL0List+NumFramesInL1List+i] = LTRefPicList[i];
                pRefPicList1[NumFramesInL0List+NumFramesInL1List+i] = LTRefPicList[i];
            }

            // Special rule: When L1 has more than one entry and L0 == L1, all entries,
            // swap the first two entries of L1.
            // They can be equal only if there are no future or no previous short-term
            // references.
            if ((NumFramesInL0List == 0 || NumFramesInL1List == 0) &&
                ((NumFramesInL0List+NumFramesInL1List+NumFramesInLTList) > 1))
            {
                pRefPicList1[0] = pRefPicList0[1];
                pRefPicList1[1] = pRefPicList0[0];
            }
        }
        else
        {
            // too many reference frames
            VM_ASSERT(0);
        }

    }    // not field slice
    else
    {
        Ipp32s CurrPicOrderCnt = slice->GetCurrentFrame()->PicOrderCnt(slice->GetCurrentFrame()->GetNumberByParity(slice->GetSliceHeader()->bottom_field_flag));
        // Short term references:
        // Need L0 and L1 lists. Both contain 2 sets of reference frames ordered
        // by PicOrderCnt. The "previous" set contains the reference frames with
        // a PicOrderCnt < current frame. The "future" set contains the reference
        // frames with a PicOrderCnt > current frame. In both cases the ordering
        // is from closest to current frame to farthest. L0 has the previous set
        // followed by the future set; L1 has the future set followed by the previous set.
        // Accomplish this by one pass through the decoded frames list creating
        // the ordered previous list in the L0 array and the ordered future list
        // in the L1 array. Then copy from both to the other for the second set.

        // Long term references:
        // The ordered list is the same for L0 and L1, is ordered by ascending
        // LongTermPicNum. The ordered list is created using local temp then
        // appended to the L0 and L1 lists after the short-term references.

        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            if (pFrm->isShortTermRef())
            {
                // add to ordered list
                FrmPicOrderCnt = pFrm->PicOrderCnt(0,2);//returns POC of reference field (or min if both are reference)

                if (FrmPicOrderCnt <= CurrPicOrderCnt)
                {
                    // Previous reference to L0, order large to small
                    j=0;
                    while (j < NumFramesInL0List &&
                        (pRefPicList0[j]->PicOrderCnt(0,2) > FrmPicOrderCnt))
                        j++;

                    // make room if needed
                    if (pRefPicList0[j])
                    {
                        for (k = NumFramesInL0List; k > j; k--)
                        {
                            // Avoid writing beyond end of list
                            if (k > MAX_NUM_REF_FRAMES-1)
                            {
                                VM_ASSERT(0);
                                bError = true;
                                break;
                            }
                            pRefPicList0[k] = pRefPicList0[k-1];
                        }
                    }
                    // add the short-term reference
                    pRefPicList0[j] = pFrm;
                    NumFramesInL0List++;
                }
                else
                {
                    // Future reference to L1, order small to large
                    j=0;
                    while (j<NumFramesInL1List &&
                        pRefPicList1[j]->PicOrderCnt(0,2) < FrmPicOrderCnt)
                        j++;

                    // make room if needed
                    if (pRefPicList1[j])
                    {
                        for (k=NumFramesInL1List; k>j; k--)
                        {
                            // Avoid writing beyond end of list
                            if (k > MAX_NUM_REF_FRAMES-1)
                            {
                                VM_ASSERT(0);
                                bError = true;
                                break;
                            }
                            pRefPicList1[k] = pRefPicList1[k-1];
                        }
                    }

                    // add the short-term reference
                    pRefPicList1[j] = pFrm;
                    NumFramesInL1List++;
                }
            }    // short-term B
            else if (pFrm->isLongTermRef())
            {
                // long term reference
                LongTermPicNum = pFrm->LongTermPicNum(0,2);

                // order smallest to largest
                j=0;
                while (j < NumFramesInLTList &&
                    LTRefPicList[j]->LongTermPicNum(0,2) < LongTermPicNum)
                    j++;

                // make room if needed
                if (LTRefPicList[j])
                {
                    for (k=NumFramesInLTList; k>j; k--)
                    {
                        // Avoid writing beyond end of list
                        if (k > MAX_NUM_REF_FRAMES-1)
                        {
                            VM_ASSERT(0);
                            bError = true;
                            break;
                        }
                        LTRefPicList[k] = LTRefPicList[k-1];
                    }
                }

                // add the long-term reference
                LTRefPicList[j] = pFrm;
                NumFramesInLTList++;

            }    // long term reference

            if (bError) break;
        }    // for pFrm

        if ((NumFramesInL0List+NumFramesInL1List+NumFramesInLTList) < MAX_NUM_REF_FRAMES)
        {
            // Complete L0 and L1 lists
            // Add future short-term references to L0 list, after previous
            for (i=0; i<NumFramesInL1List; i++)
                pRefPicList0[NumFramesInL0List+i] = pRefPicList1[i];


            // Add previous short-term references to L1 list, after future
            for (i=0; i<NumFramesInL0List; i++)
                pRefPicList1[NumFramesInL1List+i] = pRefPicList0[i];


            // Add long term list to both L0 and L1
            for (i=0; i<NumFramesInLTList; i++)
            {
                pRefPicList0[NumFramesInL0List+NumFramesInL1List+i] = LTRefPicList[i];
                pRefPicList1[NumFramesInL0List+NumFramesInL1List+i] = LTRefPicList[i];
            }

            // Special rule: When L1 has more than one entry and L0 == L1, all entries,
            // swap the first two entries of L1.
            // They can be equal only if there are no future or no previous short-term
            // references. For fields will be done later.
            /*
            if ((NumFramesInL0List == 0 || NumFramesInL1List == 0) &&
                ((NumFramesInL0List+NumFramesInL1List+NumFramesInLTList) > 1))
            {
                pRefPicList1[0] = pRefPicList0[1];
                pRefPicList1[1] = pRefPicList0[0];
            }*/
        }
        else
        {
            // too many reference frames
            VM_ASSERT(0);
        }
    }

    rli.m_iNumFramesInL0List = NumFramesInL0List;
    rli.m_iNumFramesInL1List = NumFramesInL1List;
    rli.m_iNumFramesInLTList = NumFramesInLTList;
}

void H264DBPList::AddInterViewRefs(H264Slice *slice, H264DecoderFrame **pRefPicList,
                                      ReferenceFlags *pFields, Ipp32u listNum, ViewList &views)
{
    Ipp32u curRefIdx = 0;
    Ipp32u num_refs_lx;
    const Ipp16u *refs_lx;

    // get the number of entries in the list
    while ((MAX_NUM_REF_FIELDS > curRefIdx) &&
           (pRefPicList[curRefIdx]))
    {
        curRefIdx += 1;
    }

    // get the number of inter-view references and
    // corresponding view Id array
    {
        Ipp32u VOIdx = GetVOIdx(slice->GetSeqMVCParam(), slice->GetSliceHeader()->nal_ext.mvc.view_id);
        const H264ViewRefInfo &refInfo = slice->GetSeqMVCParam()->viewInfo[VOIdx];

        if (slice->GetSliceHeader()->nal_ext.mvc.anchor_pic_flag)
        {
            num_refs_lx = refInfo.num_anchor_refs_lx[listNum];
            refs_lx = refInfo.anchor_refs_lx[listNum];
        }
        else
        {
            num_refs_lx = refInfo.num_non_anchor_refs_lx[listNum];
            refs_lx = refInfo.non_anchor_refs_lx[listNum];
        }
    }

    // append inter-view references to ref list
    for (Ipp32u j = 0; j < num_refs_lx; j += 1)
    {
        // get reference view Id
        Ipp32u refViewIdx = refs_lx[j];
        // get the corresponding view
        H264DBPList *pDPB = GetDPB(views, refViewIdx);
        if (pDPB)
        {
            // get the reference from the same access unit
            H264DecoderFrame *pRef = pDPB->findInterViewRef(slice->GetCurrentFrame()->m_auIndex, slice->GetSliceHeader()->bottom_field_flag);
            if (pRef)
            {
                pRefPicList[curRefIdx] = pRef;
                pFields[curRefIdx].field = slice->GetSliceHeader()->bottom_field_flag;
                pFields[curRefIdx].isShortReference = 1;
                curRefIdx += 1;
            }
        }
    }

} // void AddInterViewRefs(H264DecoderFrame **pRefPicList,

void H264DBPList::DebugPrint()
{
#if 1
    Trace(VM_STRING("-==========================================\n"));
    for (H264DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        Trace(VM_STRING("\n\nUID - %d POC - %d %d  - resetcount - %d\n"), pTmp->m_UID, pTmp->m_PicOrderCnt[0], pTmp->m_PicOrderCnt[1], pTmp->RefPicListResetCount(0));
        Trace(VM_STRING("ViewId - %d, m_isInterViewRef - %d \n"), pTmp->m_viewId, pTmp->m_isInterViewRef[0]);
        Trace(VM_STRING("Short - %d %d \n"), pTmp->isShortTermRef(0), pTmp->isShortTermRef(1));
        Trace(VM_STRING("Long - %d %d \n"), pTmp->isLongTermRef(0), pTmp->isLongTermRef(1));
        Trace(VM_STRING("Busy - %d \n"), pTmp->GetRefCounter());
        Trace(VM_STRING("Skipping - %d, FrameExist - %d \n"), pTmp->IsSkipped(), pTmp->IsFrameExist());
        Trace(VM_STRING("FrameNum - %d \n"), pTmp->m_FrameNum);
        Trace(VM_STRING("PicNum - (%d, %d) \n"), pTmp->m_PicNum[0], pTmp->m_PicNum[1]);
        Trace(VM_STRING("LongPicNum - (%d, %d) \n"), pTmp->m_LongTermPicNum[0], pTmp->m_LongTermPicNum[1]);
        Trace(VM_STRING("Disp - %d , wasOutput - %d wasDisplayed - %d\n"), pTmp->isDisplayable(), pTmp->wasOutputted(), pTmp->wasDisplayed());
        Trace(VM_STRING("frame ID - %d, this - %d, decoded - %d, %d \n"), pTmp->GetFrameData()->GetFrameMID(), pTmp, pTmp->m_Flags.isDecoded, pTmp->m_Flags.isDecodingCompleted);
    }

    Trace(VM_STRING("-==========================================\n"));
    //fflush(stdout);
#endif
}


} // end namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
