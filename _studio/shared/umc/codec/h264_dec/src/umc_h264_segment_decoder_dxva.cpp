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

#include <algorithm>

#include "umc_h264_segment_decoder_dxva.h"
#include "umc_h264_task_supplier.h"
#include "mfx_trace.h"
#include "mfxstructures.h"
#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

#include "vm_time.h"

namespace UMC
{
H264_DXVA_SegmentDecoderCommon::H264_DXVA_SegmentDecoderCommon(TaskSupplier * pTaskSupplier)
    : H264SegmentDecoderBase(pTaskSupplier->GetTaskBroker())
    , m_va(0)
    , m_pTaskSupplier(pTaskSupplier)
{
}

void H264_DXVA_SegmentDecoderCommon::SetVideoAccelerator(VideoAccelerator *va)
{
    VM_ASSERT(va);
    m_va = (VideoAccelerator*)va;
}

H264_DXVA_SegmentDecoder::H264_DXVA_SegmentDecoder(TaskSupplier * pTaskSupplier)
    : H264_DXVA_SegmentDecoderCommon(pTaskSupplier)
{
}

H264_DXVA_SegmentDecoder::~H264_DXVA_SegmentDecoder()
{
}

Status H264_DXVA_SegmentDecoder::Init(int32_t iNumber)
{
    return H264SegmentDecoderBase::Init(iNumber);
}

void H264_DXVA_SegmentDecoder::Reset()
{
    if (m_Packer.get())
        m_Packer->Reset();
}

void H264_DXVA_SegmentDecoder::PackAllHeaders(H264DecoderFrame * pFrame, int32_t field)
{
    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va, m_pTaskSupplier));
        if (!m_Packer.get())
            throw h264_exception(UMC_ERR_FAILED);
    }

    m_Packer->BeginFrame(pFrame, field);
    m_Packer->PackAU(pFrame, field);
    m_Packer->EndFrame();
}

Status H264_DXVA_SegmentDecoder::ProcessSegment(void)
{
    try{
        if (m_pTaskBroker->GetNextTask(0))
        {
        }
        else
        {
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
    }catch(h264_exception &ex){
        return ex.GetStatus();
    }
    return UMC_OK;
}


TaskBrokerSingleThreadDXVA::TaskBrokerSingleThreadDXVA(TaskSupplier * pTaskSupplier)
    : TaskBroker(pTaskSupplier)
    , m_lastCounter(0)
    , m_useDXVAStatusReporting(true)
{
    m_counterFrequency = vm_time_get_frequency();
}

bool TaskBrokerSingleThreadDXVA::PrepareFrame(H264DecoderFrame * pFrame)
{
    if (!pFrame || pFrame->m_iResourceNumber < 0)
    {
        return true;
    }

    bool isSliceGroups = pFrame->GetAU(0)->IsSliceGroups() || pFrame->GetAU(1)->IsSliceGroups();
    if (isSliceGroups)
        pFrame->m_iResourceNumber = 0;

    if (pFrame->prepared[0] && pFrame->prepared[1])
        return true;

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

    return true;
}

void TaskBrokerSingleThreadDXVA::Reset()
{
    m_lastCounter = 0;
    TaskBroker::Reset();
    m_reports.clear();
}

void TaskBrokerSingleThreadDXVA::Start()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBroker::Start();
    m_completedQueue.clear();

    H264_DXVA_SegmentDecoder * dxva_sd = static_cast<H264_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (dxva_sd && dxva_sd->GetPacker() && dxva_sd->GetPacker()->GetVideoAccelerator())
    {
        m_useDXVAStatusReporting = dxva_sd->GetPacker()->GetVideoAccelerator()->IsUseStatusReport();
    }

    if (m_useDXVAStatusReporting)
        return;

    for (H264DecoderFrameInfo *pTemp = m_FirstAU; pTemp; pTemp = pTemp->GetNextAU())
    {
        pTemp->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
    }

    for (H264DecoderFrameInfo *pTemp = m_FirstAU; pTemp; )
    {
        CompleteFrame(pTemp->m_pFrame);
        pTemp = m_FirstAU;
    }

    m_FirstAU = 0;
}

enum
{
    NUMBER_OF_STATUS = 512,
};


void TaskBrokerSingleThreadDXVA::SetCompletedAndErrorStatus(uint8_t uiStatus, H264DecoderFrameInfo * au)
{
    switch (uiStatus)
    {
    case 1:
        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MINOR);
        break;
    case 2:
        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
        break;
    case 3:
        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
        break;
    case 4:
        au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
        break;
    }

    au->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
    CompleteFrame(au->m_pFrame);
}


bool TaskBrokerSingleThreadDXVA::CheckCachedFeedbackAndComplete(H264DecoderFrameInfo * au)
{
    for (uint32_t i = 0; i < m_reports.size(); i++)
    {
        if ((m_reports[i].m_index == (uint32_t)au->m_pFrame->m_index) && (au->IsBottom() == (m_reports[i].m_field != 0)))
        {
            SetCompletedAndErrorStatus(m_reports[i].m_status, au);
            m_reports.erase(m_reports.begin() + i);
            return true;
        }
    }
    return false;
}

bool TaskBrokerSingleThreadDXVA::GetNextTaskInternal(H264Task *)
{
    AutomaticUMCMutex guard(m_mGuard);

    if (!m_useDXVAStatusReporting)
        return false;

    H264_DXVA_SegmentDecoder * dxva_sd = static_cast<H264_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (!dxva_sd->GetPacker())
        return false;

#if defined(UMC_VA_DXVA)
    bool wasCompleted = false;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D
    Status waitSts = UMC_OK;
#endif

    for (H264DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D
        //skip second field for sync.
        H264DecoderFrameInfo* prev = au->GetPrevAU();
        bool skip = (prev && prev->m_pFrame == au->m_pFrame);

        if (!skip)
        {
            m_mGuard.Unlock();

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec vaSyncSurface");
            waitSts = dxva_sd->GetPacker()->SyncTask(au->m_pFrame, NULL);
            m_mGuard.Lock();
        }
#endif
        // Regardless of wait status check feedback and query status
        // if found in cache or reported by driver
        // just will mark frame as completed one more time
        if (CheckCachedFeedbackAndComplete(au) == true)
        {
            wasCompleted = true;
        }
        else
        {
            // not in cache. request status then
            //DXVA_Status_H264 pStatusReport[NUMBER_OF_STATUS];
            DXVA_Status_H264 pStatusReport[NUMBER_OF_STATUS];

            for (int i = 0; i < NUMBER_OF_STATUS; i++)
                pStatusReport[i].bStatus = 3;

            dxva_sd->GetPacker()->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Status_H264)* NUMBER_OF_STATUS);

            for (uint32_t i = 0; i < NUMBER_OF_STATUS; i++)
            {
                if (pStatusReport[i].bStatus == 3)
                    throw h264_exception(UMC_ERR_DEVICE_FAILED);
                if (!pStatusReport[i].StatusReportFeedbackNumber || pStatusReport[i].CurrPic.Index7Bits == 127 ||
                    (pStatusReport[i].StatusReportFeedbackNumber & 0x80000000))
                    continue;

                wasCompleted = true;
                if ((pStatusReport[i].CurrPic.Index7Bits == (uint32_t)au->m_pFrame->m_index) && (au->IsBottom() == (pStatusReport[i].CurrPic.AssociatedFlag != 0)))
                {
                    SetCompletedAndErrorStatus(pStatusReport[i].bStatus, au);
                }
                else
                {
                    if (std::find(m_reports.begin(), m_reports.end(), ReportItem(pStatusReport[i].CurrPic.Index7Bits, pStatusReport[i].CurrPic.AssociatedFlag, 0)) == m_reports.end())
                    {
                        m_reports.push_back(ReportItem(pStatusReport[i].CurrPic.Index7Bits, pStatusReport[i].CurrPic.AssociatedFlag, pStatusReport[i].bStatus));
                    }
                }
            }
        }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D
        //check exit from waiting status.
        if (waitSts != UMC_OK && waitSts != UMC_ERR_TIMEOUT)
        {
            // we have a problem wait is failed due to some reason
            SetCompletedAndErrorStatus(2, au); //ERROR_FRAME_MAJOR
            au->m_pFrame->SetError(UMC_ERR_DEVICE_FAILED);
        }
#endif
    }
    SwitchCurrentAU();

    if (!wasCompleted && m_FirstAU)
    {
        unsigned long long currentCounter = (unsigned long long) vm_time_get_tick();

        if (m_lastCounter == 0)
            m_lastCounter = currentCounter;

        unsigned long long diff = (currentCounter - m_lastCounter);
        if (diff >= m_counterFrequency
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_H264D
            || waitSts == UMC_ERR_TIMEOUT
#endif
            )
        {
            Report::iterator iter = std::find(m_reports.begin(), m_reports.end(), ReportItem(m_FirstAU->m_pFrame->m_index, m_FirstAU->IsBottom(), 0));
            if (iter != m_reports.end())
            {
                m_reports.erase(iter);
            }

            m_FirstAU->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
            m_FirstAU->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(m_FirstAU->m_pFrame);

            SwitchCurrentAU();
            m_lastCounter = 0;
        }
    }
    else
    {
        m_lastCounter = 0;
    }
#else
#if defined(UMC_VA_LINUX)
    Status sts = UMC_OK;
    for (H264DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        H264DecoderFrameInfo* prev = au->GetPrevAU();
        //skip second field for sync.
        bool skip = (prev && prev->m_pFrame == au->m_pFrame);

        uint16_t surfCorruption = 0;
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        if (!skip)
        {
            m_mGuard.Unlock();

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec vaSyncSurface");
            sts = dxva_sd->GetPacker()->SyncTask(au->m_pFrame, &surfCorruption);

            m_mGuard.Lock();
        }
#else
        sts = dxva_sd->GetPacker()->QueryTaskStatus(au->m_pFrame, &surfSts, &surfCorruption);
#endif
        //we should complete frame even we got an error
        //this allows to return the error from [RunDecoding]
        au->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
        CompleteFrame(au->m_pFrame);

        if (sts < UMC_OK)
        {
            if (sts != UMC_ERR_GPU_HANG)
                sts = UMC_ERR_DEVICE_FAILED;

            au->m_pFrame->SetError(sts);
        }
#if !defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        else if (surfSts == VASurfaceReady)
#else
        else
#endif
            switch (surfCorruption)
            {
                case MFX_CORRUPTION_MAJOR:
                    au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                    break;

                case MFX_CORRUPTION_MINOR:
                    au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MINOR);
                    break;
            }

        if (sts != UMC_OK)
            throw h264_exception(sts);

        if (!skip)
        {
            //query SO buffer with [SyncTask] only
            sts = dxva_sd->GetPacker()->QueryStreamOut(au->m_pFrame);
            if (sts != UMC_OK)
                throw h264_exception(sts);
        }
    }

    SwitchCurrentAU();
#endif
#endif

    return false;
}

void TaskBrokerSingleThreadDXVA::AwakeThreads()
{
}

} // namespace UMC

#endif // MFX_ENABLE_H264_VIDEO_DECODE
