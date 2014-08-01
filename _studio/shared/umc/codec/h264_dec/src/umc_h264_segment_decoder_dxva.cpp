/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA

#include <algorithm>

#include "umc_h264_segment_decoder_dxva.h"
#include "umc_h264_task_supplier.h"
#include "mfx_trace.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2_protected.h"
#endif

namespace UMC
{
H264_DXVA_SegmentDecoderCommon::H264_DXVA_SegmentDecoderCommon(TaskSupplier * pTaskSupplier)
    : H264SegmentDecoderMultiThreaded(pTaskSupplier->GetTaskBroker())
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

Status H264_DXVA_SegmentDecoder::Init(Ipp32s iNumber)
{
    return H264SegmentDecoderMultiThreaded::Init(iNumber);
}

void H264_DXVA_SegmentDecoder::Reset()
{
    if (m_Packer.get())
        m_Packer->Reset();
}

void H264_DXVA_SegmentDecoder::PackAllHeaders(H264DecoderFrame * pFrame, Ipp32s field)
{
    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va, m_pTaskSupplier));
        VM_ASSERT(m_Packer.get());
    }

    m_Packer->BeginFrame();
    m_Packer->PackAU(pFrame, field);
    m_Packer->EndFrame();
}

Status H264_DXVA_SegmentDecoder::ProcessSegment(void)
{
    H264Task Task(m_iNumber);
    try{
    if (m_pTaskBroker->GetNextTask(&Task))
    {
    }
    else
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }
    }catch(h264_exception ex){
        return ex.GetStatus();
    }
    return UMC_OK;
}


TaskBrokerSingleThreadDXVA::TaskBrokerSingleThreadDXVA(TaskSupplier * pTaskSupplier)
    : TaskBrokerSingleThread(pTaskSupplier)
    , m_lastCounter(0)
    , m_useDXVAStatusReporting(true)
{
    m_counterFrequency = vm_time_get_frequency();
}

void TaskBrokerSingleThreadDXVA::WaitFrameCompletion()
{
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

    /*H264DecoderFrame * resourceHolder = m_pTaskSupplier->IsBusyByFrame(pFrame->m_iResourceNumber);
    if (resourceHolder && resourceHolder != pFrame)
        return false;

    if (!m_pTaskSupplier->LockFrameResource(pFrame))
        return false;*/

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

    // m_pTaskSupplier->SetMBMap(pFrame);
    return true;
}

void TaskBrokerSingleThreadDXVA::Reset()
{
    m_lastCounter = 0;
    TaskBrokerSingleThread::Reset();
    m_reports.clear();
}

void TaskBrokerSingleThreadDXVA::Start()
{
    AutomaticUMCMutex guard(m_mGuard);

    TaskBrokerSingleThread::Start();
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

bool TaskBrokerSingleThreadDXVA::GetNextTaskInternal(H264Task *)
{
    if (!m_useDXVAStatusReporting)
        return false;

    H264_DXVA_SegmentDecoder * dxva_sd = static_cast<H264_DXVA_SegmentDecoder *>(m_pTaskSupplier->m_pSegmentDecoder[0]);

    if (!dxva_sd->GetPacker())
        return false;

#ifdef UMC_VA_DXVA
    DXVA_Status_H264 pStatusReport[NUMBER_OF_STATUS];

    bool wasCompleted = false;

    if (m_reports.size() && m_FirstAU)
    {
        H264DecoderFrameInfo * au = m_FirstAU;
        bool wasFound = false;
        while (au)
        {
            for (Ipp32u i = 0; i < m_reports.size(); i++)
            {
                if ((m_reports[i].m_index == (Ipp32u)au->m_pFrame->m_index) && (au->IsBottom() == (m_reports[i].m_field != 0)))
                {
                    switch (m_reports[i].m_status)
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
                    wasFound = true;
                    wasCompleted = true;

                    m_reports.erase(m_reports.begin() + i);
                    break;
                }
            }

            if (!wasFound)
                break;

            au = au->GetNextAU();
        }

        SwitchCurrentAU();
    }

    //memset (&pStatusReport, 0, sizeof(pStatusReport));
    for(int i = 0; i < NUMBER_OF_STATUS; i++)
        pStatusReport[i].bStatus = 3;

    dxva_sd->GetPacker()->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Status_H264)* NUMBER_OF_STATUS);

    for (Ipp32u i = 0; i < NUMBER_OF_STATUS; i++)
    {
        if(pStatusReport[i].bStatus == 3)
            throw h264_exception(UMC_ERR_DEVICE_FAILED);
        if (!pStatusReport[i].StatusReportFeedbackNumber || pStatusReport[i].CurrPic.Index7Bits == 127 ||
            (pStatusReport[i].StatusReportFeedbackNumber & 0x80000000))
            continue;

        wasCompleted = true;
        bool wasFound = false;
        for (H264DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
        {
            if (pStatusReport[i].field_pic_flag)
            {
                if ((pStatusReport[i].CurrPic.AssociatedFlag == 1) != au->IsBottom())
                    continue;
            }

            if (pStatusReport[i].CurrPic.Index7Bits == au->m_pFrame->m_index)
            {
                switch (pStatusReport[i].bStatus)
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
                wasFound = true;
                break;
            }

            break;
        }

        if (!wasFound)
        {
            if (std::find(m_reports.begin(), m_reports.end(), ReportItem(pStatusReport[i].CurrPic.Index7Bits, pStatusReport[i].CurrPic.AssociatedFlag, 0)) == m_reports.end())
            {
                m_reports.push_back(ReportItem(pStatusReport[i].CurrPic.Index7Bits, pStatusReport[i].CurrPic.AssociatedFlag, pStatusReport[i].bStatus));
            }
        }
    }

    SwitchCurrentAU();

    if (!wasCompleted && m_FirstAU)
    {
        Ipp64u currentCounter = (Ipp64u) vm_time_get_tick();

        if (m_lastCounter == 0)
            m_lastCounter = currentCounter;

        Ipp64u diff = (currentCounter - m_lastCounter);
        if (diff >= m_counterFrequency)
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

#endif
#ifdef UMC_VA_LINUX
#ifdef ANDROID
    Status sts = UMC_OK;
    Ipp32s index;

    for (H264DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        index = au->m_pFrame->m_index;
        au->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
        CompleteFrame(au->m_pFrame);

        m_mGuard.Unlock();
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Dec vaSyncSurface");
            sts = dxva_sd->GetPacker()->SyncTask(index);
        }
        m_mGuard.Lock();
        if (sts != UMC_OK)
            throw h264_exception(sts);
    }
    SwitchCurrentAU();
#else

    Status sts = UMC_OK;
    VASurfaceStatus surfSts = VASurfaceSkipped;
    VAStatus        surfErr = VA_STATUS_SUCCESS;
    for (H264DecoderFrameInfo * au = m_FirstAU; au; au = au->GetNextAU())
    {
        sts = dxva_sd->GetPacker()->QueryTaskStatus(au->m_pFrame->m_index, &surfSts, &surfErr);
        if (sts != UMC_OK)
            throw h264_exception(sts);

        if (surfSts == VASurfaceReady)
        {
            if(VA_STATUS_ERROR_DECODING_ERROR == surfErr)
            {
                au->m_pFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
            }

            au->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            CompleteFrame(au->m_pFrame);
            break;
        }

        break;
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

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H264_VIDEO_DECODER
