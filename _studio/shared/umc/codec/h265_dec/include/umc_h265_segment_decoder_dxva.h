/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SEGMENT_DECODER_DXVA_H
#define __UMC_H265_SEGMENT_DECODER_DXVA_H

#include "umc_va_base.h"

#ifndef UMC_RESTRICTED_CODE_VA

#include <memory>
#include "umc_h265_segment_decoder_mt.h"
#include "umc_h265_task_broker.h"
#include "umc_h265_va_packer.h"
#include "umc_h265_frame_info.h"

#define UMC_VA_MAX_FRAME_BUFFER 32 //max number of uncompressed buffers

namespace UMC_HEVC_DECODER
{
class TaskSupplier_H265;

class H265_DXVA_SegmentDecoderCommon : public H265SegmentDecoderBase
{
public:
    H265_DXVA_SegmentDecoderCommon(TaskSupplier_H265 * pTaskSupplier);

    UMC::VideoAccelerator *m_va;

    void SetVideoAccelerator(UMC::VideoAccelerator *va);

protected:
    //virtual void LoadBuffers(Ipp32s FirstMB, Ipp32s MBToProcess) = 0;
    //virtual void StoreBuffers() = 0;

    TaskSupplier_H265 * m_pTaskSupplier;

private:
    H265_DXVA_SegmentDecoderCommon & operator = (H265_DXVA_SegmentDecoderCommon &)
    {
        return *this;

    }
};

class H265_DXVA_SegmentDecoder : public H265_DXVA_SegmentDecoderCommon
{
public:
    H265_DXVA_SegmentDecoder(TaskSupplier_H265 * pTaskSupplier);

    ~H265_DXVA_SegmentDecoder();

    // Initialize object
    virtual UMC::Status Init(Ipp32s iNumber);

    void PackAllHeaders(H265DecoderFrame * pFrame);

    virtual UMC::Status ProcessSegment(void);

    Ipp32s m_CurrentSliceID;

    Packer * GetPacker() { return m_Packer.get();}

protected:

    std::auto_ptr<Packer>  m_Packer;

private:
    H265_DXVA_SegmentDecoder & operator = (H265_DXVA_SegmentDecoder &)
    {
        return *this;

    }
};

/****************************************************************************************************/
// DXVASupport class implementation
/****************************************************************************************************/
template <class BaseClass>
class DXVASupport
{
public:

    DXVASupport()
        : m_va(0)
        , m_Base(0)
    {
    }

    ~DXVASupport()
    {
    }

    void Init()
    {
        m_Base = (BaseClass*)(this);
        if (!m_va)
            return;
    }

    void StartDecodingFrame(H265DecoderFrame * pFrame)
    {
        if (!m_va)
            return;

        UMC::Status sts = m_va->BeginFrame(pFrame->m_index);
        if (sts != UMC::UMC_OK)
            throw h265_exception(sts);

        H265_DXVA_SegmentDecoder * dxva_sd = (H265_DXVA_SegmentDecoder*)(m_Base->m_pSegmentDecoder[0]);
        VM_ASSERT(dxva_sd);

        for (Ipp32u i = 0; i < m_Base->m_iThreadNum; i++)
        {
            ((H265_DXVA_SegmentDecoder *)m_Base->m_pSegmentDecoder[i])->SetVideoAccelerator(m_va);
        }

        dxva_sd->PackAllHeaders(pFrame);
    }

    void EndDecodingFrame()
    {
        if (!m_va)
            return;

        UMC::Status sts = m_va->EndFrame();
        if (sts != UMC::UMC_OK)
            throw h265_exception(sts);
    }

    void SetVideoHardwareAccelerator(UMC::VideoAccelerator * va)
    {
        if (va)
            m_va = (UMC::VideoAccelerator*)va;
    }

protected:
    UMC::VideoAccelerator *m_va;
    BaseClass * m_Base;
};

class TaskBrokerSingleThreadDXVA : public TaskBroker_H265
{
public:
    TaskBrokerSingleThreadDXVA(TaskSupplier_H265 * pTaskSupplier);

    virtual bool PrepareFrame(H265DecoderFrame * pFrame);

    // Get next working task
    virtual bool GetNextTaskInternal(H265Task *pTask);
    virtual void Start();

    virtual void Reset();

    void DXVAStatusReportingMode(bool mode)
    {
        m_useDXVAStatusReporting = mode;
    }

protected:
    virtual void AwakeThreads();

    class ReportItem
    {
    public:
        Ipp32u  m_index;
        Ipp32u  m_field;
        Ipp8u   m_status;

        ReportItem(Ipp32u index, Ipp32u field, Ipp8u status)
            : m_index(index)
            , m_field(field)
            , m_status(status)
        {
        }

        bool operator == (const ReportItem & item)
        {
            return (item.m_index == m_index) && (item.m_field == m_field);
        }

        bool operator != (const ReportItem & item)
        {
            return (item.m_index != m_index) || (item.m_field != m_field);
        }
    };

    typedef std::vector<ReportItem> Report;
    Report m_reports;
    Ipp64u m_lastCounter;
    Ipp64u m_counterFrequency;

    bool   m_useDXVAStatusReporting;
};

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif /* __UMC_H265_SEGMENT_DECODER_DXVA_H */
#endif // UMC_ENABLE_H265_VIDEO_DECODER
