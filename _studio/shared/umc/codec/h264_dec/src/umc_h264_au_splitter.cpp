/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <memory>

#include "umc_h264_au_splitter.h"
#include "umc_h264_nal_spl.h"

namespace UMC
{

/****************************************************************************************************/
// SeiPayloadArray class routine
/****************************************************************************************************/
SeiPayloadArray::SeiPayloadArray()
{
    m_payloads.reserve(3);
}

SeiPayloadArray::SeiPayloadArray(const SeiPayloadArray & payloads)
{
    size_t count = payloads.GetPayloadCount();
    for (size_t i = 0; i < count; i++)
    {
        AddPayload(payloads.GetPayload(i));
    }
}

SeiPayloadArray::~SeiPayloadArray()
{
    Release();
}

size_t SeiPayloadArray::GetPayloadCount() const
{
    return m_payloads.size();
}

H264SEIPayLoad* SeiPayloadArray::GetPayload(size_t pos) const
{
    if (pos >= m_payloads.size())
        return 0;

    return m_payloads[pos];
}

H264SEIPayLoad* SeiPayloadArray::FindPayload(SEI_TYPE type) const
{
    Ipp32s pos = FindPayloadPos(type);
    return (pos < 0) ? 0 : GetPayload(pos);
}

Ipp32s SeiPayloadArray::FindPayloadPos(SEI_TYPE type) const
{
    size_t count = GetPayloadCount();
    for (size_t i = 0; i < count; i++)
    {
        H264SEIPayLoad* payload = GetPayload(i);
        if (payload->payLoadType == type)
            return (Ipp32s)i;
    }

    return -1;
}

void SeiPayloadArray::MovePayloadsFrom(SeiPayloadArray &payloads)
{
    size_t count = payloads.GetPayloadCount();
    for (size_t i = 0; i < count; i++)
    {
        AddPayload(payloads.GetPayload(i));
    }

    payloads.Release();
}

void SeiPayloadArray::Release()
{
    PayloadArray::iterator iter = m_payloads.begin();
    PayloadArray::iterator iter_end = m_payloads.end();
    for (; iter != iter_end; ++iter)
    {
        H264SEIPayLoad* payload = *iter;
        payload->DecrementReference();
    }

    m_payloads.clear();
}

void SeiPayloadArray::AddPayload(H264SEIPayLoad* payload)
{
    if (!payload)
        return;

    payload->IncrementReference();
    Ipp32s pos = FindPayloadPos(payload->payLoadType);
    if (pos >= 0) // always use last payload
    {
        m_payloads[pos]->DecrementReference();
        m_payloads[pos] = payload;
        return;
    }

    m_payloads.push_back(payload);
}

/****************************************************************************************************/
// SetOfSlices class routine
/****************************************************************************************************/
SetOfSlices::SetOfSlices()
    : m_frame(0)
    , m_isCompleted(false)
{
}

SetOfSlices::SetOfSlices(const SetOfSlices& set)
    : m_frame(set.m_frame)
    , m_isCompleted(set.m_isCompleted)
    , m_payloads(set.m_payloads)
    , m_pSliceQueue(set.m_pSliceQueue)
{
    size_t count = GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * slice = GetSlice(sliceId);
        slice->IncrementReference();
    }
}

SetOfSlices::~SetOfSlices()
{
    Release();
}

SetOfSlices& SetOfSlices::operator=(const SetOfSlices& set)
{
    if (this == &set)
    {
        return *this;
    }

    *this = set;
    size_t count = GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * slice = GetSlice(sliceId);
        slice->IncrementReference();
    }

    return *this;
}

H264Slice * SetOfSlices::GetSlice(size_t pos) const
{
    if (pos >= GetSliceCount())
        return 0;

    return m_pSliceQueue[pos];
}

size_t SetOfSlices::GetSliceCount() const
{
    return m_pSliceQueue.size();
}

void SetOfSlices::AddSlice(H264Slice * slice)
{
    m_pSliceQueue.push_back(slice);
}

void SetOfSlices::Release()
{
    size_t count = GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * slice = GetSlice(sliceId);
        slice->DecrementReference();
    }

    Reset();
}

void SetOfSlices::Reset()
{
    m_frame = 0;
    m_isCompleted = false;
    m_pSliceQueue.clear();
    m_payloads.Release();
}

void SetOfSlices::AddSet(const SetOfSlices *set)
{
    size_t count = set->GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        AddSlice(set->GetSlice(sliceId));
    }
}

void SetOfSlices::CleanUseless()
{
    size_t count = GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * curSlice = GetSlice(sliceId);
        if (curSlice->m_bDecoded)
        {
            m_pSliceQueue.erase(m_pSliceQueue.begin() + sliceId); // remove
            count = GetSliceCount();
            --sliceId;
            curSlice->Release();
            curSlice->DecrementReference();
        }
    }
}

void SetOfSlices::SortSlices()
{
    static Ipp32s MAX_MB_NUMBER = 0x7fffffff;

    if (GetSlice(0) && GetSlice(0)->IsSliceGroups())
        return;

    size_t count = GetSliceCount();
    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * curSlice = GetSlice(sliceId);
        Ipp32s minFirst = MAX_MB_NUMBER;
        size_t minSlice = 0;

        for (size_t j = sliceId; j < count; j++)
        {
            H264Slice * slice = GetSlice(j);
            if (slice->GetStreamFirstMB() < curSlice->GetStreamFirstMB() && minFirst > slice->GetStreamFirstMB() &&
                curSlice->GetSliceHeader()->nal_ext.svc.dependency_id == slice->GetSliceHeader()->nal_ext.svc.dependency_id &&
                curSlice->GetSliceHeader()->nal_ext.svc.quality_id == slice->GetSliceHeader()->nal_ext.svc.quality_id)
            {
                minFirst = slice->GetStreamFirstMB();
                minSlice = j;
            }
        }

        if (minFirst != MAX_MB_NUMBER)
        {
            H264Slice * temp = m_pSliceQueue[sliceId];
            m_pSliceQueue[sliceId] = m_pSliceQueue[minSlice];
            m_pSliceQueue[minSlice] = temp;
        }
    }

    for (size_t sliceId = 0; sliceId < count; sliceId++)
    {
        H264Slice * slice = GetSlice(sliceId);
        H264Slice * nextSlice = GetSlice(sliceId + 1);
        if (nextSlice && !nextSlice->IsSliceGroups() && !slice->IsSliceGroups())
        {
            if (nextSlice->GetSliceHeader()->nal_ext.svc.quality_id == slice->GetSliceHeader()->nal_ext.svc.quality_id)
                slice->SetMaxMB(nextSlice->GetStreamFirstMB());
        }
    }
}

/****************************************************************************************************/
// AccessUnit class routine
/****************************************************************************************************/
AccessUnit::AccessUnit()
    : m_isInitialized(false)
    , m_isFullAU(false)
    , m_auCounter(0)
{
}

AccessUnit::~AccessUnit()
{
}

Ipp32u AccessUnit::GetAUIndentifier() const
{
    return m_auCounter;
}

size_t AccessUnit::GetLayersCount() const
{
    return m_layers.size();
}

void AccessUnit::CleanUseless()
{
    size_t count = GetLayersCount();
    for (size_t pos = 0; pos < count; pos++)
    {
        SetOfSlices * set = GetLayer(pos);
        set->CleanUseless();
        if (!set->GetSliceCount())
        {
            m_layers.erase(m_layers.begin() + pos);
            count = GetLayersCount();
            pos--;
        }
    }
}

Ipp32s AccessUnit::FindLayerByDependency(Ipp32s dependency)
{
    size_t count = GetLayersCount();
    for (size_t i = 0; i < count; i++)
    {
        SetOfSlices * set = GetLayer(i);
        if (set->GetSlice(0) && set->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id == dependency)
            return (Ipp32s)i;
    }

    return -1;
}

SetOfSlices * AccessUnit::GetLayer(size_t pos)
{
    if (pos >= m_layers.size())
        return 0;

    return &m_layers[pos];
}

void AccessUnit::CombineSets()
{
    if (!m_layers.size())
        return;

    SetOfSlices * setOfSlices = GetLayer(0);
    size_t count = GetLayersCount();
    for (size_t i = 1; i < count; i++)
    {
        SetOfSlices * set = GetLayer(i);
        setOfSlices->AddSet(set);
        set->Reset();
    }

    m_layers.resize(1);
}

bool AccessUnit::IsFullAU() const
{
    return m_isFullAU;
}

void AccessUnit::CompleteLayer()
{
}

bool AccessUnit::AddSlice(H264Slice * slice)
{
    SetOfSlices * setOfSlices = GetLayerBySlice(slice);
    if (!setOfSlices)
    {
        if (GetLayersCount())
        {
            SetOfSlices * setOfSlices = GetLayer(GetLayersCount() - 1);
            if (setOfSlices->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id > slice->GetSliceHeader()->nal_ext.svc.dependency_id)
            {
                m_isFullAU = true;
                return false;
            }
        }

        setOfSlices = AddLayer(slice);
        setOfSlices->m_payloads.MovePayloadsFrom(m_payloads);
    }

    H264Slice * lastSlice = setOfSlices->GetSlice(setOfSlices->GetSliceCount() - 1);
    if (!IsPictureTheSame(lastSlice, slice))
    {
        m_isFullAU = true;
        return false;
    }

    m_payloads.Release();

    setOfSlices->AddSlice(slice);
    return true;
}

void AccessUnit::Release()
{
    size_t count = GetLayersCount();
    for (size_t i = 0; i < count; i++)
    {
        GetLayer(i)->Release();
    }

    Reset();
    m_payloads.Release();
}

void AccessUnit::Reset()
{
    size_t count = GetLayersCount();
    for (size_t i = 0; i < count; i++)
    {
        GetLayer(i)->Reset();
    }

    m_layers.clear();
    m_auCounter++;
    m_isFullAU = false;
    m_isInitialized = false;
}

void AccessUnit::SortforASO()
{
    size_t count = GetLayersCount();
    for (size_t i = 0; i < count; i++)
    {
        GetLayer(i)->SortSlices();
    }
}

SetOfSlices * AccessUnit::AddLayer(H264Slice * slice)
{
    if (slice->IsAuxiliary())
    {
    }

    SetOfSlices setofSlices;
    m_layers.push_back(setofSlices);
    return &m_layers.back();
}

bool AccessUnit::IsItSliceOfThisAU(H264Slice * slice)
{
    SetOfSlices * setOfSlices = GetLayerBySlice(slice);
    if (!setOfSlices)
        return false;

    H264Slice * lastSlice = setOfSlices->GetSlice(0);
    return IsPictureTheSame(lastSlice, slice);
}

SetOfSlices * AccessUnit::GetLayerBySlice(H264Slice * slice)
{
    if (!slice)
        return 0;

    if (slice->IsAuxiliary())
    {
        return 0;
    }

    size_t count = m_layers.size();
    for (size_t i = 0; i < count; i++)
    {
        H264Slice * temp = m_layers[i].GetSlice(0);
        if (!temp)
            continue;

        if (temp->GetSliceHeader()->nal_ext.mvc.view_id == slice->GetSliceHeader()->nal_ext.mvc.view_id &&
            temp->GetSliceHeader()->nal_ext.svc.dependency_id == slice->GetSliceHeader()->nal_ext.svc.dependency_id)
            return &m_layers[i];
    }

    return 0;
}

#if 0
static bool IsNeedSPSInvalidate(const H264SeqParamSet *old_sps, const H264SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->frame_width_in_mbs != new_sps->frame_width_in_mbs)
        return true;

    if (old_sps->frame_height_in_mbs != new_sps->frame_height_in_mbs)
        return true;

    //if (old_sps->max_dec_frame_buffering != new_sps->max_dec_frame_buffering)
      //  return true;

    /*if (old_sps->frame_cropping_rect_bottom_offset != new_sps->frame_cropping_rect_bottom_offset)
        return true;

    if (old_sps->frame_cropping_rect_left_offset != new_sps->frame_cropping_rect_left_offset)
        return true;

    if (old_sps->frame_cropping_rect_right_offset != new_sps->frame_cropping_rect_right_offset)
        return true;

    if (old_sps->frame_cropping_rect_top_offset != new_sps->frame_cropping_rect_top_offset)
        return true;

    if (old_sps->aspect_ratio_idc != new_sps->aspect_ratio_idc)
        return true; */

    return false;
}
#endif

AU_Splitter::AU_Splitter(H264_Heap_Objects *objectHeap)
    : m_Headers(objectHeap)
    , m_objHeap(objectHeap)
{
}

AU_Splitter::~AU_Splitter()
{
    Close();
}

void AU_Splitter::Init(VideoDecoderParams *init)
{
    Close();

    if (init->info.stream_subtype == AVC1_VIDEO)
    {
        m_pNALSplitter.reset(new NALUnitSplitterMP4());
    }
    else
    {
        m_pNALSplitter.reset(new NALUnitSplitter());
    }

    m_pNALSplitter->Init();
}

void AU_Splitter::Close()
{
    m_pNALSplitter.reset(0);
}

void AU_Splitter::Reset()
{
    if (m_pNALSplitter.get())
        m_pNALSplitter->Reset();

    m_Headers.Reset();
}

MediaDataEx * AU_Splitter::GetNalUnit(MediaData * src)
{
    return m_pNALSplitter->GetNalUnits(src);
}

NALUnitSplitter * AU_Splitter::GetNalUnitSplitter()
{
    return m_pNALSplitter.get();
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
