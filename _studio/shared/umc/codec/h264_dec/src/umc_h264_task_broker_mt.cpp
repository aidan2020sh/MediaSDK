// Copyright (c) 2003-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include <cstdarg>
#include "umc_h264_task_supplier.h"
#include "umc_h264_task_broker_mt.h"
#include "umc_h264_slice_ex.h"

#include "umc_h264_dec_debug.h"
#include "mfx_trace.h"

#include "umc_h264_frame_ex.h"

//#define ECHO_DEB
//#define VM_DEBUG

//#define TIME

//static int32_t lock_failed = 0;
//static int32_t sleep_count = 0;

/*__declspec(naked)
int TryRealGet(void *)
{
    __asm
    {
        mov eax, 01h
        xor edx, edx
        mov ecx, dword ptr [esp + 04h]
        lock cmpxchg dword ptr [ecx], edx
        setz al
        movzx eax, al
        ret
    }
}

int32_t TryToGet(void *p)
{
    int32_t res = TryRealGet(p);
    if (!res)
        lock_failed++;
    return res;
}*/

namespace UMC
{

void PrintInfoStatus(H264DecoderFrameInfo * info)
{
    if (!info)
        return;

    //printf("%x, field - %d, POC - (%d, %d), viewId - (%d), refAU - %x\n", info, info == info->m_pFrame->GetAU(0) ? 0 : 1, info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_PicOrderCnt[1], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, info->GetRefAU());

    /*printf("needtoCheck - %d. status - %d\n", info->GetRefAU() != 0, info->GetStatus());
    printf("POC - (%d, %d), \n", info->m_pFrame->m_PicOrderCnt[0], info->m_pFrame->m_PicOrderCnt[1]);
    printf("ViewId - (%d), \n", info->m_pFrame->m_viewId);

    for (uint32_t i = 0; i < info->GetSliceCount(); i++)
    {
        printf("slice - %d\n", i);
        H264Slice * pSlice = info->GetSlice(i);
        printf("cur to dec - %d\n", pSlice->m_iCurMBToDec);
        printf("cur to rec - %d\n", pSlice->m_iCurMBToRec);
        printf("cur to deb - %d\n", pSlice->m_iCurMBToDeb);
        printf("dec, rec, deb vacant - %d, %d, %d\n", pSlice->m_bDecVacant, pSlice->m_bRecVacant, pSlice->m_bDebVacant);
        fflush(stdout);
    }*/
}
/*
void PrintAllInfos(H264DecoderFrame * frame)
{
    printf("all info\n");
    for (; frame; frame = frame->future())
    {
        H264DecoderFrameInfo *slicesInfo = frame->GetAU(0);

        if (slicesInfo->GetSliceCount())
        {
            printf("poc - %d\n", slicesInfo->m_pFrame->m_PicOrderCnt[0]);
            if (slicesInfo->IsField())
            {
                PrintInfoStatus(slicesInfo);
                PrintInfoStatus(frame->GetAU(1));
            }
            else
            {
                PrintInfoStatus(slicesInfo);
            }
        }
    }

    printf("all info end\n");
}*/

static int32_t GetDecodingOffset(H264DecoderFrameInfo * curInfo, int32_t &readyCount)
{
    int32_t offset = 0;

    H264DecoderFrameInfo * refInfo = curInfo->GetRefAU();
    VM_ASSERT(refInfo);

    if (curInfo->m_pFrame != refInfo->m_pFrame)
    {
        switch(curInfo->m_pFrame->m_PictureStructureForDec)
        {
        case FRM_STRUCTURE:
            if (refInfo->m_pFrame->m_PictureStructureForDec == FLD_STRUCTURE)
            {
                readyCount *= 2;
                offset++;
            }
            else
            {
            }
            break;
        case AFRM_STRUCTURE:
            if (refInfo->m_pFrame->m_PictureStructureForDec == FLD_STRUCTURE)
            {
                readyCount *= 2;
                offset++;
            }
            else
            {
            }
            break;
        case FLD_STRUCTURE:
            switch(refInfo->m_pFrame->m_PictureStructureForDec)
            {
            case FLD_STRUCTURE:
                break;
            case AFRM_STRUCTURE:
            case FRM_STRUCTURE:
                readyCount /= 2;
                offset++;
                break;
            }
            break;
        }
    }

    return offset;
}

TaskBrokerSingleThread::TaskBrokerSingleThread(TaskSupplier * pTaskSupplier)
    : TaskBroker(pTaskSupplier)
{
}

bool TaskBrokerSingleThread::Init(int32_t iConsumerNumber)
{
    Release();
    TaskBroker::Init(iConsumerNumber);
    m_localResourses.Init(m_iConsumerNumber, m_pTaskSupplier->GetMemoryAllocator());
    return true;
}

void TaskBrokerSingleThread::Reset()
{
    AutomaticUMCMutex guard(m_mGuard);

    for (FrameQueue::iterator iter = m_decodingQueue.begin(); iter != m_decodingQueue.end(); ++iter)
    {
        m_localResourses.UnlockFrameResource(*iter);
    }

    m_localResourses.Reset();

    TaskBroker::Reset();
}

void TaskBrokerSingleThread::Release()
{
    TaskBroker::Release();
    m_localResourses.Close();
}

void TaskBrokerSingleThread::CompleteFrame(H264DecoderFrame * frame)
{
    if (!frame || m_decodingQueue.empty() || !frame->IsFullFrame())
        return;

    if (!IsFrameCompleted(frame) || frame->IsDecodingCompleted())
        return;

    m_localResourses.UnlockFrameResource(frame);
    TaskBroker::CompleteFrame(frame);
}

int32_t TaskBrokerSingleThread::GetNumberOfTasks(bool details)
{
    int32_t au_count = 0;

    H264DecoderFrameInfo * temp = m_FirstAU;

    for (; temp ; temp = temp->GetNextAU())
    {
        if (temp->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
            continue;

        if (details)
        {
            au_count += temp->GetSliceCount();
        }
        else
        {
            if (!temp->IsField())
            {
                au_count++;
            }

            au_count++;
        }
    }

    return au_count;
}

bool TaskBrokerSingleThread::IsEnoughForStartDecoding(bool force)
{
    AutomaticUMCMutex guard(m_mGuard);

    InitAUs();
    int32_t au_count = GetNumberOfTasks(false);

    int32_t additional_tasks = m_iConsumerNumber;
    if (m_iConsumerNumber > 6)
        additional_tasks -= 1;

    if ((au_count >> 1) >= additional_tasks || (force && au_count))
        return true;

    return false;
}

void TaskBrokerSingleThread::AddPerformedTask(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    H264SliceEx *pSlice = pTask->m_pSlice;
    H264DecoderFrameInfo * info = pTask->m_pSlicesInfo;

#if defined(ECHO)
        switch (pTask->m_iTaskID)
        {
        case TASK_PROCESS:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()),  pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_REC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEC_REC:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) dec_rec done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        case TASK_DEB:
#if defined(ECHO_DEB)
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) (%d) deb done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
#else
        case TASK_DEB_FRAME:
        case TASK_DEB_FRAME_THREADED:
        case TASK_DEB_SLICE:
        case TASK_DEB_THREADED:
#endif // defined(ECHO_DEB)
            break;
        default:
            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) default task done % 4d to % 4d - error - %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess, pTask->m_bError));
            break;
        }
#endif // defined(ECHO)

#ifdef TIME
    timer.ThreadFinished(pTask->m_iThreadNumber, pTask->m_iTaskID);
#endif // TIME

    // when whole slice was processed
    if (TASK_PROCESS == pTask->m_iTaskID)
    {
        // it is possible only in "slice group" mode
        if (pSlice->IsSliceGroups())
        {
            pSlice->m_iMaxMB = pTask->m_iMaxMB;
            pSlice->m_iAvailableMB -= pTask->m_iMBToProcess;

            if (pTask->m_bError)
            {
                pSlice->m_bError = true;
                pSlice->m_bDeblocked = true;
            }

            /*
            // correct remain uncompressed macroblock count.
            // we can't relay on slice number cause of field pictures.
            if (pSlice->m_iAvailableMB)
            {
                int32_t pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                VM_ASSERT(pos >= 0);
                H264Slice * pNextSlice = info->GetSlice(pos + 1);
                if (pNextSlice)
                {
                    //pNextSlice->m_iAvailableMB = pSlice->m_iAvailableMB;
                }
            }*/
        }
        else
        { // slice is deblocked only when deblocking was available

            // error handling
            pSlice->m_iMaxMB = pTask->m_iMaxMB;
            if (pTask->m_bError)
            {
                pSlice->m_bError = true;
            }

            if (false == pSlice->m_bDeblocked)
                pSlice->m_bDeblocked = pSlice->m_bPrevDeblocked;
        }
        // slice is decoded
        pSlice->m_bDecVacant = 0;
        pSlice->m_bRecVacant = 0;
        pSlice->m_bDebVacant = 1;
        pSlice->m_bInProcess = false;
        pSlice->CompleteDecoding();
        pSlice->m_bDecoded = true;
    }
    else if (TASK_DEB_SLICE == pTask->m_iTaskID)
    {
        pSlice->m_bDebVacant = 1;
        pSlice->m_bDeblocked = 1;
        pSlice->m_bInProcess = false;
    }
    else if (TASK_DEB_FRAME == pTask->m_iTaskID)
    {
        int32_t sliceCount = m_FirstAU->GetSliceCount();

        // frame is deblocked
        for (int32_t i = 0; i < sliceCount; i += 1)
        {
            H264SliceEx *pTemp = (H264SliceEx *)m_FirstAU->GetSlice(i);

            pTemp->m_bDebVacant = 1;
            pTemp->m_bInProcess = false;
            pTemp->CompleteDecoding();
            pTemp->m_bDeblocked = true;
        }
    }
    else
    {
        switch (pTask->m_iTaskID)
        {
        case TASK_DEC:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDec);

            pSlice->m_iCurMBToDec += pTask->m_iMBToProcess;
            // move filled buffer to reconstruct queue

            VM_ASSERT(pSlice->GetCoeffsBuffers() || pSlice->m_bError);

            if (pSlice->GetCoeffsBuffers())
                pSlice->GetCoeffsBuffers()->UnLockInputBuffer(pTask->m_WrittenSize);

            pSlice->m_MVsDistortion = pTask->m_mvsDistortion;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iDecMBReady);
            if (isReadyIncrease)
            {
                info->m_iDecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                    info->m_iDecMBReady = pSlice->m_iMaxMB;
                pSlice->m_iMaxMB = std::min(pSlice->m_iCurMBToDec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }
            else
            {
                pSlice->m_iMaxMB = pTask->m_iMaxMB;
            }

            if (pSlice->m_iCurMBToDec >= pSlice->m_iMaxMB)
            {
                pSlice->m_bDecVacant = 0;
                if (isReadyIncrease)
                {
                    int32_t pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H264SliceEx * pNextSlice = (H264SliceEx *)info->GetSlice(++pos);
                        for (; pNextSlice; pNextSlice = (H264SliceEx *)info->GetSlice(++pos))
                        {
                           info->m_iDecMBReady = pNextSlice->m_iCurMBToDec;
                           if (pNextSlice->m_iCurMBToDec < pNextSlice->m_iMaxMB)
                               break;
                        }
                    }
                }
            }
            else
            {
                pSlice->m_bDecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), info->m_iDecMBReady));
            }
            break;

        case TASK_REC:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToRec);

            pSlice->m_iCurMBToRec += pTask->m_iMBToProcess;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iRecMBReady) && pSlice->m_bDeblocked;
            if (isReadyIncrease)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncrease)
                {
                    info->m_iRecMBReady = pSlice->m_iMaxMB;
                }

                //pSlice->m_iMaxMB = std::min(pSlice->m_iCurMBToRec, pSlice->m_iMaxMB);
                pSlice->m_bError = true;
            }

            VM_ASSERT(pSlice->GetCoeffsBuffers() || pSlice->m_bError);
            if (pSlice->GetCoeffsBuffers())
                pSlice->GetCoeffsBuffers()->UnLockOutputBuffer();

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToRec)
            {
                if (isReadyIncrease)
                {
                    int32_t pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (pos >= 0)
                    {
                        H264SliceEx * pNextSlice = (H264SliceEx *)info->GetSlice(pos + 1);
                        if (pNextSlice)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;

                            info->RemoveSlice(pos);
                        }
                    }
                }

                pSlice->m_bRecVacant = 0;
                pSlice->CompleteDecoding();
                pSlice->m_bDecoded = true;
            }
            else
            {
                pSlice->m_bRecVacant = 1;
            }
            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) rec ready %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), info->m_iRecMBReady));
            }
            break;

        case TASK_DEC_REC:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDec);
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToRec);

            pSlice->m_iCurMBToDec += pTask->m_iMBToProcess;
            pSlice->m_iCurMBToRec += pTask->m_iMBToProcess;

            bool isReadyIncreaseDec = (pTask->m_iFirstMB == info->m_iDecMBReady);
            bool isReadyIncreaseRec = (pTask->m_iFirstMB == info->m_iRecMBReady);

            pSlice->m_iMaxMB = pTask->m_iMaxMB;

            if (isReadyIncreaseDec)
                info->m_iDecMBReady += pTask->m_iMBToProcess;

            if (isReadyIncreaseRec && pSlice->m_bDeblocked)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                if (isReadyIncreaseDec)
                    info->m_iDecMBReady = pSlice->m_iMaxMB;

                if (isReadyIncreaseRec && pSlice->m_bDeblocked)
                    info->m_iRecMBReady = pSlice->m_iMaxMB;

                pSlice->m_iMaxMB = std::min(pSlice->m_iCurMBToDec, pSlice->m_iMaxMB);
                pSlice->m_iCurMBToRec = pSlice->m_iCurMBToDec;
                pSlice->m_bError = true;
            }

            if (pSlice->m_iCurMBToDec >= pSlice->m_iMaxMB)
            {
                VM_ASSERT(pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB);

                if (isReadyIncreaseDec || isReadyIncreaseRec)
                {
                    int32_t pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    VM_ASSERT(pos >= 0);
                    H264SliceEx * pNextSlice = (H264SliceEx *)info->GetSlice(pos + 1);
                    if (pNextSlice)
                    {
                        if (isReadyIncreaseDec)
                        {
                            int32_t pos1 = pos;
                            H264SliceEx * pTmpSlice = (H264SliceEx *)info->GetSlice(++pos1);

                            for (; pTmpSlice; pTmpSlice = (H264SliceEx *)info->GetSlice(++pos1))
                            {
                               info->m_iDecMBReady = pTmpSlice->m_iCurMBToDec;
                               if (pTmpSlice->m_iCurMBToDec < pTmpSlice->m_iMaxMB)
                                   break;
                            }
                        }

                        if (isReadyIncreaseRec && pSlice->m_bDeblocked)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;

                            info->RemoveSlice(pos);
                        }
                    }
                }

                pSlice->m_bDecVacant = 0;
                pSlice->m_bRecVacant = 0;
                pSlice->CompleteDecoding();
                pSlice->m_bDecoded = true;
            }
            else
            {
                pSlice->m_bDecVacant = 1;
                pSlice->m_bRecVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (%d) (viewId - %d) (%d) dec_rec ready %d %d\n"), pTask->m_iThreadNumber, (int32_t)(info != m_FirstAU), info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;

        case TASK_DEB:
            {
            VM_ASSERT(pTask->m_iFirstMB == pSlice->m_iCurMBToDeb);

            pSlice->m_iCurMBToDeb += pTask->m_iMBToProcess;

            bool isReadyIncrease = (pTask->m_iFirstMB == info->m_iRecMBReady);
            if (isReadyIncrease)
            {
                info->m_iRecMBReady += pTask->m_iMBToProcess;
            }

            // error handling
            if (pTask->m_bError || pSlice->m_bError)
            {
                pSlice->m_iCurMBToDeb = pSlice->m_iMaxMB;
                info->m_iRecMBReady = pSlice->m_iCurMBToRec;
                pSlice->m_bError = true;
            }

            if (pSlice->m_iMaxMB <= pSlice->m_iCurMBToDeb)
            {
                if (pSlice->m_bDecoded)
                {
                    int32_t pos = info->GetPositionByNumber(pSlice->GetSliceNum());
                    if (isReadyIncrease)
                    {
                        VM_ASSERT(pos >= 0);
                        H264SliceEx * pNextSlice = (H264SliceEx *)info->GetSlice(pos + 1);
                        if (pNextSlice)
                        {
                            if (pNextSlice->m_bDeblocked)
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToRec;
                            else
                                info->m_iRecMBReady = pNextSlice->m_iCurMBToDeb;
                        }
                    }

                    info->RemoveSlice(pos);

                    pSlice->m_bInProcess = false;
                    pSlice->m_bDecVacant = 0;
                    pSlice->m_bRecVacant = 0;
                    pSlice->CompleteDecoding();
                }
                pSlice->m_bDeblocked = true;
            }
            else
            {
                pSlice->m_bDebVacant = 1;
            }

            DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) after deb ready %d %d\n"), pTask->m_iThreadNumber, info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), info->m_iDecMBReady, info->m_iRecMBReady));
            }
            break;
#if 0
        case TASK_DEB_THREADED:
            pSlice->m_DebTools.SetProcessedMB(pTask);
            if (pSlice->m_DebTools.IsDeblockingDone())
                pSlice->m_bDeblocked = true;

            break;

        case TASK_DEB_FRAME_THREADED:
            m_DebTools.SetProcessedMB(pTask);
            if (m_DebTools.IsDeblockingDone())
            {
                int32_t i;

                // frame is deblocked
                for (i = 0; i < GetNumberOfSlicesFromCurrentFrame(); i += 1)
                {
                    H264Slice *pTemp = GetSliceFromCurrentFrame(i);

                    pTemp->m_bDeblocked = true;
                }
            }

            break;
#endif

        default:
            // illegal case
            VM_ASSERT(false);
            break;
        }
    }
}

// Get next working task
bool TaskBrokerSingleThread::GetNextTaskInternal(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    while (false == m_IsShouldQuit)
    {
        if (m_FirstAU)
        {
            if (GetNextSlice(m_FirstAU, pTask))
                return true;

            if (m_FirstAU->IsCompleted())
            {
                SwitchCurrentAU();
            }

            if (GetNextSlice(m_FirstAU, pTask))
                return true;
        }

        break;
    }

    return false;
}

bool TaskBrokerSingleThread::PrepareFrame(H264DecoderFrame * pFrame)
{
    if (!pFrame)
    {
        return true;
    }

    if (pFrame->prepared[0] && pFrame->prepared[1])
        return true;

    if (pFrame->m_iResourceNumber == -1)
        pFrame->m_iResourceNumber = m_localResourses.GetCurrentResourceIndex();

    H264DecoderFrame * resourceHolder = m_localResourses.IsBusyByFrame(pFrame->m_iResourceNumber);
    if (resourceHolder && resourceHolder != pFrame)
        return false;

    if (!m_localResourses.LockFrameResource(pFrame))
        return false;

    if (!pFrame->prepared[0] &&
        (pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(0)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[0] = true;
    }

    if (!pFrame->prepared[1] &&
        (pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_FILLED || pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_STARTED))
    {
        pFrame->prepared[1] = true;
    }

    DEBUG_PRINT((VM_STRING("Prepare frame - %d, %d, m_viewId - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_iResourceNumber, pFrame->m_viewId));

    return true;
}

bool TaskBrokerSingleThread::GetPreparationTask(H264DecoderFrameInfo * info)
{
    H264DecoderFrameEx * pFrame = DynamicCast<H264DecoderFrameEx>(info->m_pFrame);

    if (!pFrame || info->m_prepared)
        return false;

    if (info != pFrame->GetAU(0) && pFrame->GetAU(0)->m_prepared != 2)
        return false;

    info->m_prepared = 1;

    if (info == pFrame->GetAU(0))
    {
        VM_ASSERT(pFrame->m_iResourceNumber != -1);
        int32_t iMBCount = pFrame->totalMBs << (pFrame->m_PictureStructureForDec < FRM_STRUCTURE ? 1 : 0);

        m_localResourses.AllocateBuffers(iMBCount);
    }

    Unlock();

    try
    {
        if (info == pFrame->GetAU(0))
        {
            VM_ASSERT(pFrame->m_iResourceNumber != -1);
            int32_t iMBCount = pFrame->totalMBs << (pFrame->m_PictureStructureForDec < FRM_STRUCTURE ? 1 : 0);

            // allocate decoding data
            m_localResourses.AllocateMBInfo(pFrame->m_iResourceNumber, iMBCount);

            // allocate macroblock intra types
            m_localResourses.AllocateMBIntraTypes(pFrame->m_iResourceNumber, iMBCount);

            H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(0);
            int32_t sliceCount = slicesInfo->GetSliceCount();
            for (int32_t i = 0; i < sliceCount; i++)
            {
                H264SliceEx * pSlice = (H264SliceEx *)slicesInfo->GetSlice(i);
                pSlice->InitializeContexts();
                pSlice->m_mbinfo = &m_localResourses.GetMBInfo(pFrame->m_iResourceNumber);
                pSlice->m_pMBIntraTypes = m_localResourses.GetIntraTypes(pFrame->m_iResourceNumber);
            }

            // reset frame global data
            if (slicesInfo->IsSliceGroups())
            {
                memset(pFrame->m_mbinfo.mbs, 0, iMBCount*sizeof(H264DecoderMacroblockGlobalInfo));
                memset(pFrame->m_mbinfo.MV[0], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
                memset(pFrame->m_mbinfo.MV[1], 0, iMBCount*sizeof(H264DecoderMacroblockMVs));
            }
            else
            {
                if (slicesInfo->m_isBExist && slicesInfo->m_isPExist)
                {
                    for (int32_t i = 0; i < sliceCount; i++)
                    {
                        H264Slice * pSlice = slicesInfo->GetSlice(i);
                        if (pSlice->GetSliceHeader()->slice_type == PREDSLICE)
                            memset(pFrame->m_mbinfo.MV[1] + pSlice->GetFirstMBNumber(), 0, pSlice->GetMBCount()*sizeof(H264DecoderMacroblockMVs));
                    }
                }
            }

            if (slicesInfo->IsSliceGroups())
            {
                Lock();
                m_pTaskSupplier->SetMBMap(0, pFrame, &m_localResourses);
                Unlock();
            }
        }
        else
        {
            int32_t sliceCount = info->GetSliceCount();
            for (int32_t i = 0; i < sliceCount; i++)
            {
                H264SliceEx * pSlice = (H264SliceEx *)info->GetSlice(i);
                pSlice->InitializeContexts();
                pSlice->m_mbinfo = &m_localResourses.GetMBInfo(pFrame->m_iResourceNumber);
                pSlice->m_pMBIntraTypes = m_localResourses.GetIntraTypes(pFrame->m_iResourceNumber);
            }

            if (!info->IsSliceGroups() && info->m_isBExist && info->m_isPExist)
            {
                for (int32_t i = 0; i < sliceCount; i++)
                {
                    H264Slice * pSlice = info->GetSlice(i);
                    if (pSlice->GetSliceHeader()->slice_type == PREDSLICE)
                        memset(pFrame->m_mbinfo.MV[1] + pSlice->GetFirstMBNumber(), 0, pSlice->GetMBCount()*sizeof(H264DecoderMacroblockMVs));
                }
            }
        }
    }
    catch(...)
    {
        Lock();

        int32_t sliceCount = info->GetSliceCount();
        for (int32_t i = 0; i < sliceCount; i++)
        {
            H264SliceEx * pSlice = (H264SliceEx *)info->GetSlice(i);
            pSlice->m_bInProcess = false;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;
            pSlice->CompleteDecoding();
            pSlice->m_bDecoded = true;
            pSlice->m_bDeblocked = true;
        }

        info->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);

        info->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);

        info->m_prepared = 2;
        return false;

    }

    Lock();
    info->m_prepared = 2;
    return false;
}

bool TaskBrokerSingleThread::GetNextSlice(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    // check error(s)
    if (!info)
        return false;

    if (info->GetRefAU() != 0)
        return false;

    if (GetPreparationTask(info))
        return true;

    if (info->m_prepared != 2)
        return false;

    if (GetNextSliceToDecoding(info, pTask))
        return true;

    // try to get slice to decode
    /*if ((false == GetSliceFromCurrentFrame(0)->IsSliceGroups()) ||
        (0 == pTask->m_iThreadNumber))
    {
        if (GetNextSliceToDecoding(pTask))
            return true;
    }*/

    // try to get slice to deblock
    //if ((false == GetSliceFromCurrentFrame(0)->IsSliceGroups()) ||
      //  (0 == pTask->m_iThreadNumber))
    if (info->IsNeedDeblocking())
        return GetNextSliceToDeblocking(info, pTask);

    return false;
}

void TaskBrokerSingleThread::InitTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice)
{
    pTask->m_bDone = false;
    pTask->m_bError = false;
    pTask->m_iMaxMB = pSlice->m_iMaxMB;
    pTask->m_pSlice = pSlice;
    pTask->m_pSlicesInfo = info;
}

bool TaskBrokerSingleThread::GetNextSliceToDecoding(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    int32_t i;
    bool bDoDeblocking;

    // skip some slices, more suitable for first thread
    // and first slice is always reserved for first slice decoder
    /*if (pTask->m_iThreadNumber)
    {
        i = std::max(1, GetNumberOfSlicesFromCurrentFrame() / m_iConsumerNumber);
        bDoDeblocking = false;
    }
    else
    {
        i = 0;
        bDoDeblocking = true;
    }*/

    i = 0;
    bDoDeblocking = false;

    // find first uncompressed slice
    int32_t sliceCount = info->GetSliceCount();
    for (; i < sliceCount; i += 1)
    {
        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

        if ((false == pSlice->m_bInProcess) &&
            (false == pSlice->m_bDecoded))
        {
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iFirstMB;
            pTask->m_iMBToProcess = std::min(pSlice->m_iMaxMB - pSlice->m_iFirstMB, pSlice->m_iAvailableMB);
            pTask->m_iTaskID = TASK_PROCESS;
            pTask->m_pBuffer = NULL;
            // we can do deblocking only on independent slices or
            // when all previous slices are deblocked
            if (DEBLOCK_FILTER_ON != pSlice->GetSliceHeader()->disable_deblocking_filter_idc)
                bDoDeblocking = true;
            pSlice->m_bPrevDeblocked = bDoDeblocking;
            pSlice->m_bInProcess = true;
            pSlice->m_bDecVacant = 0;
            pSlice->m_bRecVacant = 0;
            pSlice->m_bDebVacant = 0;

#ifdef ECHO
            DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) slice dec - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt[0],
                pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

            return true;
        }
    }

    return false;
}

bool TaskBrokerSingleThread::GetNextSliceToDeblocking(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    int32_t sliceCount = info->GetSliceCount();
    H264Slice* slice = info->GetSlice(sliceCount - 1);
    VM_ASSERT(slice);
    if (!slice)
       return false;

    bool bSliceGroups = slice->IsSliceGroups();

    // slice group deblocking
    if (bSliceGroups)
    {
        bool isError = false;
        for (int32_t i = 0; i < sliceCount; i += 1)
        {
            H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

            if (pSlice->m_bInProcess || !pSlice->m_bDecoded)
                return false;

            if (pSlice->m_bError)
                isError = true;
        }

        bool bNothingToDo = true;
        for (int32_t i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if (pSlice && pSlice->m_bDeblocked)
                continue;

            bNothingToDo = false;
        }

        // we already deblocked
        if (bNothingToDo)
            return false;

        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(sliceCount - 1);
        pSlice->m_bInProcess = true;
        pSlice->m_bDebVacant = 0;
        InitTask(info, pTask, pSlice);
        pTask->m_bError = isError;
        pTask->m_iFirstMB = 0;
        int32_t iMBInFrame = (pSlice->m_iMBWidth * pSlice->m_iMBHeight) /
                            ((pSlice->GetSliceHeader()->field_pic_flag) ? (2) : (1));
        pTask->m_iMaxMB = iMBInFrame;
        pTask->m_iMBToProcess = iMBInFrame;

        pTask->m_iTaskID = TASK_DEB_FRAME;
        pTask->m_pBuffer = 0;

#ifdef ECHO_DEB
        DEBUG_PRINT((VM_STRING("(%d) (%d) (m_viewId - %d) frame deb - % 4d to % 4d\n"), pTask->m_iThreadNumber, pSlice->m_pCurrentFrame->m_PicOrderCnt[0],
                pSlice->GetSliceHeader()->nal_ext.mvc.view_id, pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

        return true;
    }
    else
    {
        int32_t i;
        bool bPrevDeblocked = true;

        for (i = 0; i < sliceCount; i += 1)
        {
            H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

            // we can do deblocking only on vacant slices
            if ((false == pSlice->m_bInProcess) &&
                (true == pSlice->m_bDecoded) &&
                (false == pSlice->m_bDeblocked))
            {
                // we can do this only when previous slice was deblocked or
                // deblocking isn't going through slice boundaries
                if ((true == bPrevDeblocked) ||
                    (false == pSlice->DeblockThroughBoundaries()))
                {
                    InitTask(info, pTask, pSlice);
                    pTask->m_iFirstMB = pSlice->m_iFirstMB;
                    pTask->m_iMBToProcess = pSlice->m_iMaxMB - pSlice->m_iFirstMB;
                    pTask->m_iTaskID = TASK_DEB_SLICE;
                    pTask->m_pBuffer = NULL;

                    pSlice->m_bPrevDeblocked = true;
                    pSlice->m_bInProcess = true;
                    pSlice->m_bDebVacant = 0;

#ifdef ECHO_DEB
                    DEBUG_PRINT((VM_STRING("(%d) slice deb - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                            pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

                    return true;
                }
            }

            // save previous slices deblocking condition
            if (false == pSlice->m_bDeblocked)
                bPrevDeblocked = false;
        }
    }

    return false;
}


TaskBrokerTwoThread::TaskBrokerTwoThread(TaskSupplier * pTaskSupplier)
    : TaskBrokerSingleThread(pTaskSupplier)
{
}

bool TaskBrokerTwoThread::Init(int32_t iConsumerNumber)
{
    if (!TaskBrokerSingleThread::Init(iConsumerNumber))
        return false;

    return true;
}

void TaskBrokerTwoThread::Reset()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Reset();
}

void TaskBrokerTwoThread::Release()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Release();
}

void TaskBrokerTwoThread::CompleteFrame()
{
    if (!m_FirstAU)
        return;

    DEBUG_PRINT((VM_STRING("frame completed - poc - %d\n"), m_FirstAU->m_pFrame->m_PicOrderCnt[0]));
    SwitchCurrentAU();
}

bool TaskBrokerTwoThread::GetNextTaskInternal(H264Task *pTask)
{
#ifdef TIME
    CStarter start(pTask->m_iThreadNumber);
#endif // TIME

    AutomaticUMCMutex guard(m_mGuard);

    if (m_IsShouldQuit)
    {
        return false;
    }

    pTask->m_taskPreparingGuard = &guard;

    while (false == m_IsShouldQuit)
    {
        if (m_FirstAU)
        {
            for (H264DecoderFrameInfo *pTemp = m_FirstAU; pTemp; pTemp = pTemp->GetNextAU())
            {
                if (GetNextTaskManySlices(pTemp, pTask))
                {
                    return true;
                }
            }
        }

        //SleepThread(pTask->m_iThreadNumber);
        break;
    };

    return false;
}

bool TaskBrokerTwoThread::WrapDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice)
{
    VM_ASSERT(pSlice);

    if (!pSlice->GetCoeffsBuffers())
        pSlice->m_coeffsBuffers = m_localResourses.AllocateCoeffBuffer(pSlice);

    // this is guarded function, safe to touch any variable
    if (1 == pSlice->m_bDecVacant && pSlice->GetCoeffsBuffers()->IsInputAvailable())
    {
        pSlice->m_bInProcess = true;
        pSlice->m_bDecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        int32_t iMBWidth = pSlice->GetMBRowWidth();

        // error handling
        /*if (0 >= ((signed) pSlice->m_BitStream.BytesLeft()))
        {
            pSlice->m_iMaxMB = pSlice->m_iCurMBToDec;
            return false;
        }*/

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToDec;
        pTask->m_WrittenSize = 0;
        pTask->m_iMBToProcess = std::min(pSlice->m_iCurMBToDec -
                                    (pSlice->m_iCurMBToDec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToDec;

        pTask->m_iMBToProcess = std::min(pTask->m_iMBToProcess, pSlice->m_iAvailableMB);
        pTask->m_iTaskID = TASK_DEC;
        pTask->m_pBuffer = (UMC::CoeffsPtrCommon)pSlice->GetCoeffsBuffers()->LockInputBuffer();

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapDecodingTask(H264Task *pTask, H264Slice *pSlice)

bool TaskBrokerTwoThread::WrapReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

    if (1 == pSlice->m_bRecVacant && pSlice->GetCoeffsBuffers()->IsOutputAvailable())
    {
        pSlice->m_bRecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        int32_t iMBWidth = pSlice->GetMBRowWidth();

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToRec;
        pTask->m_iMBToProcess = std::min(pSlice->m_iCurMBToRec -
                                    (pSlice->m_iCurMBToRec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToRec;

        pTask->m_iTaskID = TASK_REC;
        uint8_t* pointer = NULL;
        size_t size;
        pSlice->GetCoeffsBuffers()->LockOutputBuffer(pointer, size);
        pTask->m_pBuffer = ((UMC::CoeffsPtrCommon) pointer);

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d)  (%d) rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapReconstructTask(H264Task *pTaskm H264Slice *pSlice)

bool TaskBrokerTwoThread::WrapDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask, H264SliceEx *pSlice)
{
    VM_ASSERT(pSlice);
    // this is guarded function, safe to touch any variable

    if (1 == pSlice->m_bRecVacant && 1 == pSlice->m_bDecVacant &&
        pSlice->m_iCurMBToDec == pSlice->m_iCurMBToRec)
    {
        pSlice->m_bRecVacant = 0;
        pSlice->m_bDecVacant = 0;

        pTask->m_taskPreparingGuard->Unlock();
        int32_t iMBWidth = pSlice->GetMBRowWidth();

        InitTask(info, pTask, pSlice);
        pTask->m_iFirstMB = pSlice->m_iCurMBToDec;
        pTask->m_WrittenSize = 0;
        pTask->m_iMBToProcess = std::min(pSlice->m_iCurMBToDec -
                                    (pSlice->m_iCurMBToDec % iMBWidth) +
                                    iMBWidth,
                                    pSlice->m_iMaxMB) - pSlice->m_iCurMBToDec;

        pTask->m_iTaskID = TASK_DEC_REC;
        pTask->m_pBuffer = 0;

#ifdef ECHO
        DEBUG_PRINT((VM_STRING("(%d) (%d) (viewId - %d) (%d) dec_rec - % 4d to % 4d\n"), pTask->m_iThreadNumber,
            info->m_pFrame->m_PicOrderCnt[0], pSlice->GetSliceHeader()->nal_ext.mvc.view_id, (int32_t)(pSlice->IsBottomField()), pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO

        return true;
    }

    return false;

} // bool TaskBrokerTwoThread::WrapDecRecTask(H264Task *pTaskm H264Slice *pSlice)

bool TaskBrokerTwoThread::GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    int32_t i;

    H264DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    int32_t readyCount = 0;
    int32_t additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
        {
            is_need_check = false;
        }
        else
        {
            readyCount = refAU->m_iDecMBReady;
            additionalLines = GetDecodingOffset(info, readyCount);
        }
    }

    int32_t sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

        if (pSlice->m_bDecVacant != 1)
            continue;

        if (is_need_check)
        {
            if (pSlice->m_iCurMBToDec + (1 + additionalLines)*pSlice->GetMBRowWidth() > readyCount)
                break;
        }

        if (WrapDecodingTask(info, pTask, pSlice))
            return true;
    }

    return false;

} // bool TaskBrokerTwoThread::GetDecodingTask(H264DecoderFrameInfo * info, H264Task *pTask)

bool TaskBrokerTwoThread::GetReconstructTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable

    H264DecoderFrameInfo * refAU = info->GetRefAU();
    bool is_need_check = refAU != 0;
    int32_t readyCount = 0;
    int32_t additionalLines = 0;

    if (is_need_check)
    {
        if (refAU->GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
        {
            is_need_check = false;
        }
        else
        {
            readyCount = refAU->m_iRecMBReady;
            additionalLines = GetDecodingOffset(info, readyCount);
        }
    }

    int32_t i;
    int32_t sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

        if (pSlice->m_bRecVacant && !pSlice->GetCoeffsBuffers())
            break;

        if (pSlice->m_bRecVacant != 1 || !pSlice->GetCoeffsBuffers() ||
            !pSlice->GetCoeffsBuffers()->IsOutputAvailable())
        {
            continue;
        }

        if (is_need_check)
        {
            int32_t distortion = pSlice->m_MVsDistortion + (refAU->IsNeedDeblocking() ? 4 : 0);
            int32_t k = ( (distortion + 15) / 16) + 1; // +2 - (1 - for padding, 2 - current line)
            if (pSlice->m_iCurMBToRec + (k + additionalLines)*pSlice->GetMBRowWidth() >= readyCount)
                break;
        }

        if (WrapReconstructTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;

} // bool TaskBrokerTwoThread::GetReconstructTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetDecRecTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    if (info->GetRefAU() || info->GetSliceCount() == 1)
        return false;

    int32_t i;
    int32_t sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

        if (pSlice->m_bRecVacant != 1 || pSlice->m_bDecVacant != 1)
        {
            continue;
        }

        if (WrapDecRecTask(info, pTask, pSlice))
        {
            return true;
        }
    }

    return false;
}

bool TaskBrokerTwoThread::GetDeblockingTask(H264DecoderFrameInfo * info, H264Task *pTask)
{
    // this is guarded function, safe to touch any variable
    int32_t i;
    bool bPrevDeblocked = true;

    int32_t sliceCount = info->GetSliceCount();
    for (i = 0; i < sliceCount; i += 1)
    {
        H264SliceEx *pSlice = (H264SliceEx *)info->GetSlice(i);

        int32_t iMBWidth = pSlice->GetMBRowWidth(); // DEBUG : always deblock two lines !!!
        int32_t iAvailableToDeblock;
        int32_t iDebUnit = (pSlice->GetSliceHeader()->MbaffFrameFlag) ? (2) : (1);

        iAvailableToDeblock = pSlice->m_iCurMBToRec -
                              pSlice->m_iCurMBToDeb;

        if ((false == pSlice->m_bDeblocked) &&
            ((true == bPrevDeblocked) || (false == pSlice->DeblockThroughBoundaries())) &&
            (1 == pSlice->m_bDebVacant) &&
            ((iAvailableToDeblock > iMBWidth) || (pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB)))
        {
            pSlice->m_bDebVacant = 0;

            pTask->m_taskPreparingGuard->Unlock();
            InitTask(info, pTask, pSlice);
            pTask->m_iFirstMB = pSlice->m_iCurMBToDeb;

            {
                pTask->m_iMBToProcess = std::min(iMBWidth - (pSlice->m_iCurMBToDeb % iMBWidth),
                                            iAvailableToDeblock);
                pTask->m_iMBToProcess = std::max(pTask->m_iMBToProcess,
                                            iDebUnit);
                pTask->m_iMBToProcess = mfx::align2_value(pTask->m_iMBToProcess, iDebUnit);
            }

            pTask->m_iTaskID = TASK_DEB;

#ifdef ECHO_DEB
            DEBUG_PRINT((VM_STRING("(%d) deb - % 4d to % 4d\n"), pTask->m_iThreadNumber,
                pTask->m_iFirstMB, pTask->m_iFirstMB + pTask->m_iMBToProcess));
#endif // ECHO_DEB

            return true;
        }
        else
        {
            if ((0 >= iAvailableToDeblock) && (pSlice->m_iCurMBToRec >= pSlice->m_iMaxMB))
            {
                pSlice->m_bDeblocked = true;
                bPrevDeblocked = true;
            }
        }

        // save previous slices deblocking condition
        if (false == pSlice->m_bDeblocked || pSlice->m_iCurMBToRec < pSlice->m_iMaxMB)
        {
            bPrevDeblocked = false;
            break; // for mbaff deblocking could be performaed outside boundaries.
        }
    }

    return false;

} // bool TaskBrokerTwoThread::GetDeblockingTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetNextTaskManySlices(H264DecoderFrameInfo * info, H264Task *pTask)
{
    if (!info)
        return false;

    if (GetPreparationTask(info))
        return true;

    if (info->m_prepared != 2)
        return false;

    if (info->IsSliceGroups())
    {
        return GetNextSlice(info, pTask);
    }

    if (info->IsNeedDeblocking())
    {
        if (true == GetDeblockingTask(info, pTask))
            return true;
    }

    // try to get reconstruct task from main queue
    if (GetReconstructTask(info, pTask))
    {
        return true;
    }

    if (GetDecRecTask(info, pTask))
    {
        return true;
    }

    // try to get decoding task from main frame
    if (GetDecodingTask(info, pTask))
    {
        return true;
    }

    return false;
}

void TaskBrokerTwoThread::AddPerformedTask(H264Task *pTask)
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::AddPerformedTask(pTask);

    if (pTask->m_pSlicesInfo->IsCompleted())
    {
        pTask->m_pSlicesInfo->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
    }

    SwitchCurrentAU();
} // void TaskBrokerTwoThread::AddPerformedTask(H264Task *pTask)

bool TaskBrokerTwoThread::GetFrameDeblockingTaskThreaded(H264DecoderFrameInfo * , H264Task *)
{
#if 1
    return false;
#else
    // this is guarded function, safe to touch any variable
    int32_t sliceCount = info->GetSliceCount();
    H264Slice * firstSlice = info->GetSlice(0);

    if (m_bFirstDebThreadedCall)
    {
        int32_t i;
        int32_t iFirstMB = -1, iMaxMB = -1;
        int32_t iMBWidth = firstSlice->GetMBRowWidth();
        int32_t iDebUnit = (firstSlice->GetSliceHeader()->MbaffFrameFlag) ? (2) : (1);
        bool bError = false;

        // check all other threads are sleep
        for (i = 0; i < sliceCount; i++)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if ((pSlice) && (0 == pSlice->m_bDebVacant))
                return false;
            if (pSlice->m_bError)
                bError = true;
        }

        // handle little slices
        if ((iDebUnit * 2 > iMBWidth / m_iConsumerNumber) || bError)
            return GetDeblockingTask(pTask);

        // calculate deblocking range
        for (i = 0; i < sliceCount; i++)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if ((pSlice) &&
                (false == pSlice->m_bDeblocked))
            {
                // find deblocking range
                if (-1 == iFirstMB)
                    iFirstMB = pSlice->m_iCurMBToDeb;
                if (iMaxMB < pSlice->m_iMaxMB)
                    iMaxMB = pSlice->m_iMaxMB;
            }
        }

        m_DebTools.Reset(iFirstMB,
                         iMaxMB,
                         iDebUnit,
                         iMBWidth);

        m_bFirstDebThreadedCall = false;
    }

    // get next piece to deblock
    if (false == m_DebTools.GetMBToProcess(pTask))
        return false;

    // correct task to slice range
    {
        int32_t i;

        for (i = 0; i < sliceCount; i += 1)
        {
            H264Slice *pSlice = info->GetSlice(i);

            if ((pTask->m_iFirstMB >= pSlice->m_iFirstMB) &&
                (pTask->m_iFirstMB < pSlice->m_iMaxMB))
            {
                pTask->m_iTaskID = TASK_DEB_FRAME_THREADED;
                pTask->m_pSlice = pSlice;
                if (pTask->m_iFirstMB + pTask->m_iMBToProcess > pSlice->m_iMaxMB)
                    pTask->m_iMBToProcess = pSlice->m_iMaxMB - pTask->m_iFirstMB;
                break;
            }
        }
    }

    return true;
#endif
} // bool TaskBrokerTwoThread::GetFrameDeblockingTaskThreaded(H264Task *pTask)


/****************************************************************************************************/
// Resources
/****************************************************************************************************/

LocalResources::LocalResources()
    : m_pMBMap(0)
    , next_mb_tables(0)
    , m_ppMBIntraTypes(0)
    , m_piMBIntraProp(0)
    , m_pMBInfo(0)
    , m_numberOfBuffers(0)
    , m_pMemoryAllocator(0)
    , m_parsedDataLength(0)
    , m_pParsedData(0)
    , m_midParsedData(0)
    , m_currentResourceIndex(0)
{
}

LocalResources::~LocalResources()
{
    Close();
}

Status LocalResources::Init(int32_t numberOfBuffers, MemoryAllocator *pMemoryAllocator)
{
    Close();

    m_numberOfBuffers = numberOfBuffers;
    m_pMemoryAllocator = pMemoryAllocator;

    m_pMBInfo = new H264DecoderLocalMacroblockDescriptor[numberOfBuffers];
    if (NULL == m_pMBInfo)
        return UMC_ERR_ALLOC;

    m_ppMBIntraTypes = new uint32_t *[numberOfBuffers];
    if (NULL == m_ppMBIntraTypes)
        return UMC_ERR_ALLOC;
    memset(m_ppMBIntraTypes, 0, sizeof(uint32_t *)*numberOfBuffers);

    // allocate intra MB types array's sizes
    m_piMBIntraProp = new H264IntraTypesProp[numberOfBuffers];
    if (NULL == m_piMBIntraProp)
        return UMC_ERR_ALLOC;
    memset(m_piMBIntraProp, 0, sizeof(H264IntraTypesProp) * numberOfBuffers);

    next_mb_tables = new H264DecoderMBAddr *[numberOfBuffers + 1];
    return UMC_OK;
}

void LocalResources::Reset()
{
    for (int32_t i = 0; i < m_numberOfBuffers; i++)
    {
        m_pMBInfo[i].m_isBusy = false;
        m_pMBInfo[i].m_pFrame = 0;
    }

    m_currentResourceIndex = 0;
}

void LocalResources::Close()
{
    if (m_ppMBIntraTypes)
    {
        delete[] m_ppMBIntraTypes;
        m_ppMBIntraTypes = 0;
    }

    if (m_piMBIntraProp)
    {
        for (int32_t i = 0; i < m_numberOfBuffers; i++)
        {
            if (m_piMBIntraProp[i].m_nSize == 0)
                continue;
            m_pMemoryAllocator->Unlock(m_piMBIntraProp[i].m_mid);
            m_pMemoryAllocator->Free(m_piMBIntraProp[i].m_mid);
        }

        delete[] m_piMBIntraProp;
        m_piMBIntraProp = 0;
    }

    delete[] m_pMBInfo;
    m_pMBInfo = 0;

    delete[] next_mb_tables;
    next_mb_tables = 0;

    DeallocateBuffers();

    m_numberOfBuffers = 0;
    m_pMemoryAllocator = 0;
    m_currentResourceIndex = 0;
}

uint32_t LocalResources::GetCurrentResourceIndex()
{
    int32_t index = m_currentResourceIndex % (m_numberOfBuffers);
    m_currentResourceIndex++;
    return index;
}

bool LocalResources::LockFrameResource(H264DecoderFrame * frame)
{
    int32_t number = frame->m_iResourceNumber;
    if (m_pMBInfo[number].m_isBusy)
        return m_pMBInfo[number].m_pFrame == frame;

    m_pMBInfo[number].m_isBusy = true;
    m_pMBInfo[number].m_pFrame = frame;
    return true;
}

void LocalResources::UnlockFrameResource(H264DecoderFrame * frame)
{
    int32_t number = frame->m_iResourceNumber;
    if (number < 0 || m_pMBInfo[number].m_pFrame != frame)
        return;

    m_pMBInfo[number].m_isBusy = false;
    m_pMBInfo[number].m_pFrame = 0;
}

H264DecoderFrame * LocalResources::IsBusyByFrame(int32_t number)
{
    return m_pMBInfo[number].m_pFrame;
}

H264CoeffsBuffer * LocalResources::AllocateCoeffBuffer(H264Slice * slice)
{
    H264CoeffsBuffer * coeffsBuffers = m_ObjHeap.AllocateObject<H264CoeffsBuffer>();
    coeffsBuffers->IncrementReference();


    int32_t iMBRowSize = slice->GetMBRowWidth();
    int32_t iMBRowBuffers;
    int32_t bit_depth_luma, bit_depth_chroma;
    if (slice->GetSliceHeader()->is_auxiliary)
    {
        bit_depth_luma = slice->GetSeqParamEx()->bit_depth_aux;
        bit_depth_chroma = 8;
    } else {
        bit_depth_luma = slice->GetSeqParam()->bit_depth_luma;
        bit_depth_chroma = slice->GetSeqParam()->bit_depth_chroma;
    }

    int32_t isU16Mode = (bit_depth_luma > 8 || bit_depth_chroma > 8) ? 2 : 1;

    // decide number of buffers
    if (slice->m_iMaxMB - slice->m_iFirstMB > iMBRowSize)
    {
        iMBRowBuffers = (slice->m_iMaxMB - slice->m_iFirstMB + iMBRowSize - 1) / iMBRowSize;
        iMBRowBuffers = std::min(int32_t(MINIMUM_NUMBER_OF_ROWS), iMBRowBuffers);
    }
    else
    {
        iMBRowSize = slice->m_iMaxMB - slice->m_iFirstMB;
        iMBRowBuffers = 1;
    }

    coeffsBuffers->Init(iMBRowBuffers, (int32_t)sizeof(int16_t) * isU16Mode * (iMBRowSize * COEFFICIENTS_BUFFER_SIZE + DEFAULT_ALIGN_VALUE));
    coeffsBuffers->Reset();
    return coeffsBuffers;
}

void LocalResources::FreeCoeffBuffer(H264CoeffsBuffer *buffer)
{
    if (buffer)
    {
        buffer->Reset();
        m_ObjHeap.FreeObject(buffer);
    }
}

void LocalResources::AllocateMBIntraTypes(int32_t iIndex, int32_t iMBNumber)
{
    if ((0 == m_ppMBIntraTypes[iIndex]) ||
        (m_piMBIntraProp[iIndex].m_nSize < iMBNumber))
    {
        size_t nSize;
        // delete previously allocated array
        if (m_ppMBIntraTypes[iIndex])
        {
            m_pMemoryAllocator->Unlock(m_piMBIntraProp[iIndex].m_mid);
            m_pMemoryAllocator->Free(m_piMBIntraProp[iIndex].m_mid);
        }
        m_ppMBIntraTypes[iIndex] = NULL;
        m_piMBIntraProp[iIndex].Reset();

        nSize = iMBNumber * NUM_INTRA_TYPE_ELEMENTS * sizeof(IntraType);
        if (UMC_OK != m_pMemoryAllocator->Alloc(&(m_piMBIntraProp[iIndex].m_mid),
                                                nSize,
                                                UMC_ALLOC_PERSISTENT))
            throw h264_exception(UMC_ERR_ALLOC);
        m_piMBIntraProp[iIndex].m_nSize = (int32_t) iMBNumber;
        m_ppMBIntraTypes[iIndex] = (uint32_t *) m_pMemoryAllocator->Lock(m_piMBIntraProp[iIndex].m_mid);
    }
}

H264DecoderLocalMacroblockDescriptor & LocalResources::GetMBInfo(int32_t number)
{
    return m_pMBInfo[number];
}

void LocalResources::AllocateMBInfo(int32_t number, uint32_t iMBCount)
{
    if (!m_pMBInfo[number].Allocate(iMBCount, m_pMemoryAllocator))
        throw h264_exception(UMC_ERR_ALLOC);
}

IntraType * LocalResources::GetIntraTypes(int32_t number)
{
    return m_ppMBIntraTypes[number];
}

H264DecoderMBAddr * LocalResources::GetDefaultMBMapTable() const
{
    return next_mb_tables[0];
}

void LocalResources::AllocateBuffers(int32_t mb_count)
{
    int32_t     next_mb_size = (int32_t)(mb_count*sizeof(H264DecoderMBAddr));
    int32_t     totalSize = (m_numberOfBuffers + 1)*next_mb_size + mb_count + 128; // 128 used for alignments

    // Reallocate our buffer if its size is not appropriate.
    if (m_parsedDataLength < totalSize)
    {
        DeallocateBuffers();

        if (m_pMemoryAllocator->Alloc(&m_midParsedData,
                                      totalSize,
                                      UMC_ALLOC_PERSISTENT))
            throw h264_exception(UMC_ERR_ALLOC);

        m_pParsedData = (uint8_t *) m_pMemoryAllocator->Lock(m_midParsedData);
        ippsZero_8u(m_pParsedData, totalSize);

        m_parsedDataLength = totalSize;
    }
    else
        return;

    size_t     offset = 0;

    m_pMBMap = align_pointer<uint8_t *> (m_pParsedData);
    offset += mb_count;

    if (offset & 0x7)
        offset = (offset + 7) & ~7;
    next_mb_tables[0] = align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);

    //initialize first table
    for (int32_t i = 0; i < mb_count; i++)
        next_mb_tables[0][i] = i + 1; // simple linear scan

    offset += next_mb_size;

    for (int32_t i = 1; i <= m_numberOfBuffers; i++)
    {
        if (offset & 0x7)
            offset = (offset + 7) & ~7;

        next_mb_tables[i] = align_pointer<H264DecoderMBAddr *> (m_pParsedData + offset);
        offset += next_mb_size;
    }
}

void LocalResources::DeallocateBuffers()
{
    if (m_pParsedData)
    {
        // Free the old buffer.
        m_pMemoryAllocator->Unlock(m_midParsedData);
        m_pMemoryAllocator->Free(m_midParsedData);
        m_pParsedData = 0;
        m_midParsedData = 0;
    }

    m_parsedDataLength = 0;
}

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
