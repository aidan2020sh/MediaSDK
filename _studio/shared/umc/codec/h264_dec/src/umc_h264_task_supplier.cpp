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

#include <algorithm>
#include <memory>

#include "umc_h264_task_supplier.h"
#include "umc_h264_frame_list.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_bitstream.h"

#include "umc_h264_dec_defs_dec.h"
#include "vm_sys_info.h"
#include "umc_h264_segment_decoder_mt.h"

#include "umc_h264_task_broker.h"
#include "umc_structures.h"

#include "umc_frame_data.h"

#include "umc_h264_dec_debug.h"

namespace UMC
{

Ipp32u GetDPBLayerIndex(Ipp32u max, Ipp32u dId)
{
    return max - dId; // because it easy for using target layer as zero index.
}

/****************************************************************************************************/
// DPBOutput class routine
/****************************************************************************************************/
DPBOutput::DPBOutput()
{
    Reset();
}

void DPBOutput::Reset(bool disableDelayFeature)
{
    m_isUseFlags.use_payload_sei_delay = 1;
    m_isUseFlags.use_pic_order_cnt_type = 1;

    if (disableDelayFeature)
    {
        m_isUseFlags.use_payload_sei_delay = 0;
        m_isUseFlags.use_pic_order_cnt_type = 0;
    }
}

bool DPBOutput::IsUseDelayOutputValue() const
{
    return IsUseSEIDelayOutputValue() || IsUsePicOrderCnt();
}

bool DPBOutput::IsUseSEIDelayOutputValue() const
{
    return m_isUseFlags.use_payload_sei_delay != 0;
}

bool DPBOutput::IsUsePicOrderCnt() const
{
    return m_isUseFlags.use_pic_order_cnt_type != 0;
}

void DPBOutput::OnNewSps(H264SeqParamSet * sps)
{
    if (sps && sps->pic_order_cnt_type != 2)
    {
        m_isUseFlags.use_pic_order_cnt_type = 0;
    }
}

Ipp32s DPBOutput::GetDPBOutputDelay(H264SEIPayLoad * payload)
{
    if (IsUsePicOrderCnt())
        return 0;

    if (!payload)
    {
        m_isUseFlags.use_payload_sei_delay = 0;
        return INVALID_DPB_OUTPUT_DELAY;
    }

    if (IsUseSEIDelayOutputValue())
    {
        if (payload->SEI_messages.pic_timing.dpb_output_delay != 0)
            m_isUseFlags.use_payload_sei_delay = 0;

        return payload->SEI_messages.pic_timing.dpb_output_delay;
    }

    return INVALID_DPB_OUTPUT_DELAY;
}

/****************************************************************************************************/
// SVC extension class routine
/****************************************************************************************************/
SVC_Extension::SVC_Extension()
    : MVC_Extension()
    , m_dependency_id(H264_MAX_DEPENDENCY_ID)
    , m_quality_id(H264_MAX_QUALITY_ID)
{
}

SVC_Extension::~SVC_Extension()
{
    Reset();
}

void SVC_Extension::Reset()
{
    MVC_Extension::Reset();

    m_quality_id = 15;
    m_dependency_id = 7;
}

void SVC_Extension::Close()
{
    SVC_Extension::Reset();
    MVC_Extension::Close();
}

void SVC_Extension::SetSVCTargetLayer(Ipp32u dependency_id, Ipp32u quality_id, Ipp32u temporal_id)
{
    m_dependency_id = dependency_id;
    m_quality_id = quality_id;
    m_temporal_id = temporal_id;
}

bool SVC_Extension::IsShouldSkipSlice(H264NalExtension *nal_ext)
{
    if (!nal_ext)
        return true;

    if (!nal_ext->svc_extension_flag)
        return MVC_Extension::IsShouldSkipSlice(nal_ext);

    //if (nal_ext->svc.discardable_flag)
      //  return true;

    if (nal_ext->svc.temporal_id > m_temporal_id)
        return true;

    if (nal_ext->svc.priority_id > m_priority_id)
        return true;

    if (nal_ext->svc.quality_id > m_quality_id)
        return true;

    if (nal_ext->svc.dependency_id > m_dependency_id)
        return true;

    return false;
}

bool SVC_Extension::IsShouldSkipSlice(H264Slice * slice)
{
    return IsShouldSkipSlice(&slice->GetSliceHeader()->nal_ext);
}

void SVC_Extension::ChooseLevelIdc(const H264SeqParamSetSVCExtension * extension, Ipp8u baseViewLevelIDC, Ipp8u extensionLevelIdc)
{
    if (m_level_idc)
        return;

    m_level_idc = extension->level_idc;

    Ipp8u maxLevel = IPP_MAX(baseViewLevelIDC, extensionLevelIdc);
    m_level_idc = IPP_MAX(maxLevel, m_level_idc);

    if (!m_level_idc)
    {
        VM_ASSERT(false);
    }
}

/****************************************************************************************************/
// DecReferencePictureMarking
/****************************************************************************************************/
DecReferencePictureMarking::DecReferencePictureMarking()
    : m_isDPBErrorFound(0)
    , m_frameCount(0)
{
}

void DecReferencePictureMarking::Reset()
{
    m_frameCount = 0;
    m_isDPBErrorFound = 0;
    m_commandsList.clear();
}

void DecReferencePictureMarking::ResetError()
{
    m_isDPBErrorFound = 0;
}

Ipp32u DecReferencePictureMarking::GetDPBError() const
{
    return m_isDPBErrorFound;
}

bool IsDecRefPicMarkingEquals(const H264SEIPayLoadBase::SEIMessages::DecRefPicMarkingRepetition *decRefPicMarking1, const H264SEIPayLoadBase::SEIMessages::DecRefPicMarkingRepetition *decRefPicMarking2)
{
    if (!decRefPicMarking1 || !decRefPicMarking2)
        return true;

    if (decRefPicMarking1->original_idr_flag            != decRefPicMarking2->original_idr_flag ||
        decRefPicMarking1->original_frame_num           != decRefPicMarking2->original_frame_num ||
        decRefPicMarking1->original_field_pic_flag      != decRefPicMarking2->original_field_pic_flag ||
        decRefPicMarking1->original_bottom_field_flag   != decRefPicMarking2->original_bottom_field_flag ||
        decRefPicMarking1->long_term_reference_flag     != decRefPicMarking2->long_term_reference_flag)
        return false;

    if (decRefPicMarking1->adaptiveMarkingInfo.num_entries != decRefPicMarking2->adaptiveMarkingInfo.num_entries)
        return false;

    for (Ipp32u i = 0; i < decRefPicMarking1->adaptiveMarkingInfo.num_entries; i++)
    {
        if (decRefPicMarking1->adaptiveMarkingInfo.value[i] != decRefPicMarking2->adaptiveMarkingInfo.value[i] ||
            decRefPicMarking1->adaptiveMarkingInfo.mmco[i]  != decRefPicMarking2->adaptiveMarkingInfo.mmco[i])
            return false;
    }

    return true;
}

Status DecReferencePictureMarking::CheckSEIRepetition(ViewItem &view, H264SEIPayLoad *payload)
{
    Status umcRes = UMC_OK;
    if (!payload || payload->payLoadType != SEI_DEC_REF_PIC_MARKING_TYPE)
        return UMC_OK;

    H264DecoderFrame * pFrame = 0;

    for (H264DecoderFrame *pCurr = view.GetDPBList(0)->head(); pCurr; pCurr = pCurr->future())
    {
        if (pCurr->FrameNum() == payload->SEI_messages.dec_ref_pic_marking_repetition.original_frame_num)
        {
            pFrame = pCurr;
            break;
        }
    }

    if (!pFrame)
        return UMC_OK;

    const H264SEIPayLoadBase::SEIMessages::DecRefPicMarkingRepetition *forCheck = pFrame->GetAU(0)->GetDecRefPicMarking();
    if (payload->SEI_messages.dec_ref_pic_marking_repetition.original_field_pic_flag)
    {
        forCheck = pFrame->GetAU(pFrame->GetNumberByParity(payload->SEI_messages.dec_ref_pic_marking_repetition.original_bottom_field_flag))->GetDecRefPicMarking();
    }

    if (!IsDecRefPicMarkingEquals(&payload->SEI_messages.dec_ref_pic_marking_repetition, forCheck))
    {
        pFrame->SetErrorFlagged(ERROR_FRAME_DPB);
        m_isDPBErrorFound = 1;
        return UMC_ERR_FAILED;
    }

#if 0
    Ipp32s field_index = payload->SEI_messages.dec_ref_pic_marking_repetition.original_bottom_field_flag;
    bool bCurrentisST = true;

    DPBCommandsList commandsList;

    if (payload->SEI_messages.dec_ref_pic_marking_repetition.original_idr_flag)
    {
        // mark all reference pictures as unused
        for (H264DecoderFrame *pCurr = view.pDPB->head(); pCurr; pCurr = pCurr->future())
        {
            if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
            {
                AddItem(commandsList, pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | SHORT_TERM);
                AddItem(commandsList, pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | LONG_TERM);
            }
        }

        if (payload->SEI_messages.dec_ref_pic_marking_repetition.long_term_reference_flag)
        {
            AddItem(commandsList, pFrame, pFrame, SET_REFERENCE | LONG_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
        }
        else
        {
            AddItem(commandsList, pFrame, pFrame, SET_REFERENCE | SHORT_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
        }

        bCurrentisST = false;
    }
    else
    {
        AdaptiveMarkingInfo* pAdaptiveMarkingInfo = &payload->SEI_messages.dec_ref_pic_marking_repetition.adaptiveMarkingInfo;
        // adaptive ref pic marking
        if (pAdaptiveMarkingInfo && pAdaptiveMarkingInfo->num_entries > 0)
        {
            for (Ipp32u arpmmf_idx = 0; arpmmf_idx<pAdaptiveMarkingInfo->num_entries; arpmmf_idx++)
            {
                H264DecoderFrame * pRefFrame = 0;
                Ipp32s field = 0;
                Ipp32s picNum;
                Ipp32s LongTermFrameIdx;

                switch (pAdaptiveMarkingInfo->mmco[arpmmf_idx])
                {
                case 1:
                    // mark a short-term picture as unused for reference
                    // Value is difference_of_pic_nums_minus1
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);

                    pRefFrame = view.pDPB->findShortTermPic(picNum, &field);
                    AddItem(commandsList, pFrame, pRefFrame, UNSET_REFERENCE | SHORT_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
                    break;
                case 2:
                    // mark a long-term picture as unused for reference
                    // value is long_term_pic_num
                    picNum = pAdaptiveMarkingInfo->value[arpmmf_idx*2];
                    pRefFrame = view.pDPB->findLongTermPic(picNum, &field);
                    AddItem(commandsList, pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
                    break;
                case 3:
                    // Assign a long-term frame idx to a short-term picture
                    // Value is difference_of_pic_nums_minus1 followed by
                    // long_term_frame_idx. Only this case uses 2 value entries.
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2+1];

                    pRefFrame = view.pDPB->findShortTermPic(picNum, &field);
                    if (!pRefFrame)
                        break;

                    {
                        H264DecoderFrame * longTerm = view.pDPB->findLongTermRefIdx(LongTermFrameIdx);
                        if (longTerm != pRefFrame)
                            AddItemAndRun(pFrame, longTerm, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);
                    }

                    AddItem(commandsList, pFrame, pRefFrame, SET_REFERENCE | LONG_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
                    AddItem(commandsList, pFrame, pRefFrame, UNSET_REFERENCE | SHORT_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
                    break;
                case 4:
                    // Specify max long term frame idx
                    // Value is max_long_term_frame_idx_plus1
                    // Set to "no long-term frame indices" (-1) when value == 0.
                    view.MaxLongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2] - 1;

                    pRefFrame = view.pDPB->findOldLongTermRef(view.MaxLongTermFrameIdx);
                    while (pRefFrame)
                    {
                        AddItem(commandsList, pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);
                        pRefFrame = view.pDPB->findOldLongTermRef(view.MaxLongTermFrameIdx);
                    }
                    break;
                case 5:
                    // Mark all as unused for reference
                    // no value
                    for (H264DecoderFrame *pCurr = view.pDPB->head(); pCurr; pCurr = pCurr->future())
                    {
                        if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
                        {
                            AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | SHORT_TERM);
                            AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | LONG_TERM);
                        }
                    }

                    view.pDPB->IncreaseRefPicListResetCount(pFrame);
                    view.MaxLongTermFrameIdx = -1;        // no long term frame indices
                    // set "previous" picture order count vars for future

                    if (pFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                    {
                        pFrame->setPicOrderCnt(0, field_index);
                        pFrame->setPicNum(0, field_index);
                    }
                    else
                    {
                        Ipp32s poc = pFrame->PicOrderCnt(0, 3);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(0, 1) - poc, 0);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(1, 1) - poc, 1);
                        pFrame->setPicNum(0, 0);
                        pFrame->setPicNum(0, 1);
                    }

                    view.pPOCDec->Reset(0);
                    // set frame_num to zero for this picture, for correct
                    // FrameNumWrap
                    pFrame->setFrameNum(0);
                    break;
                case 6:
                    // Assign long term frame idx to current picture
                    // Value is long_term_frame_idx
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2];
                    bCurrentisST = false;

                    pRefFrame = view.pDPB->findLongTermRefIdx(LongTermFrameIdx);
                    if (pRefFrame != pFrame)
                        AddItem(commandsList, pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);

                    AddItem(commandsList, pFrame, pFrame, SET_REFERENCE | LONG_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));

                    pFrame->setLongTermFrameIdx(LongTermFrameIdx);
                    break;
                case 0:
                default:
                    // invalid mmco command in bitstream
                    VM_ASSERT(0);
                    umcRes = UMC_ERR_INVALID_STREAM;
                    break;
                }    // switch
            }    // for arpmmf_idx
        }
    }    // not IDR picture

    if (bCurrentisST)
    { // set current as
        if (payload->SEI_messages.dec_ref_pic_marking_repetition.original_field_pic_flag && field_index)
        {
        }
        else
        {
            SlideWindow(view, 0, field_index);
        }

        AddItemAndRun(pFrame, pFrame, SET_REFERENCE | SHORT_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
    }

        struct TempSt
        {
            bool shortRef[2];
            bool longRef[2];
        };

        std::vector<TempSt> tempArr;
        for (H264DecoderFrame *pCurr = view.pDPB->head(); pCurr; pCurr = pCurr->future())
        {
            TempSt temp;
            temp.shortRef[0] = pCurr->m_isShortTermRef[0];
            temp.shortRef[1] = pCurr->m_isShortTermRef[1];
            temp.longRef[0] = pCurr->m_isLongTermRef[0];
            temp.longRef[1] = pCurr->m_isLongTermRef[1];
            tempArr.push_back(temp);
        }

        //CheckSEIRepetition(view, pFrame);

        Ipp32s i = 0;
        for (H264DecoderFrame *pCurr = view.pDPB->head(); pCurr; pCurr = pCurr->future(), ++i)
        {
            TempSt &temp = tempArr[i];
            if (temp.shortRef[0] != pCurr->m_isShortTermRef[0] ||
            temp.shortRef[1] != pCurr->m_isShortTermRef[1] ||
            temp.longRef[0] != pCurr->m_isLongTermRef[0] ||
            temp.longRef[1] != pCurr->m_isLongTermRef[1] )
            __asm int 3;
        }
#endif

    return umcRes;
}

void DecReferencePictureMarking::CheckSEIRepetition(ViewItem &view, H264DecoderFrame * frame)
{
    bool isEqual = false;

    if (isEqual)
        return;

    DPBCommandsList::iterator end_iter = m_commandsList.end();
    H264DecoderFrame * temp = 0;

    bool wasFrame = false;

    for (H264DecoderFrame *pCurr = view.GetDPBList(0)->head(); pCurr; pCurr = pCurr->future())
    {
        pCurr->IncrementReference();
        pCurr->IncrementReference();
    }

    {
    DPBCommandsList::reverse_iterator iter = m_commandsList.rbegin();
    DPBCommandsList::reverse_iterator end_iter = m_commandsList.rend();

    for (; iter != end_iter; ++iter)
    {
        DPBChangeItem& item = *iter;

        if (item.m_pCurrentFrame == frame)
        {
            wasFrame = true;
            Undo(frame);
            break;
        }

        if (temp != item.m_pCurrentFrame)
        {
            temp = 0;
        }

        if (!temp)
        {
            temp = item.m_pCurrentFrame;
            Undo(temp);
        }
    }
    }

    bool start = false;
    temp = 0;
    DPBCommandsList::iterator iter = m_commandsList.begin(); // stl issue. we can't reuse iterator
    for (; iter != end_iter; ++iter)
    {
        DPBChangeItem& item = *iter;

        if (item.m_pCurrentFrame == frame)
        {
            Redo(frame);
            start = true;
            break;
        }

        if (!start && wasFrame)
            continue;

        if (temp != item.m_pCurrentFrame)
        {
            temp = 0;
        }

        if (!temp)
        {
            temp = item.m_pCurrentFrame;
            Redo(temp);
        }
    }

    for (H264DecoderFrame *pCurr = view.GetDPBList(0)->head(); pCurr; pCurr = pCurr->future())
    {
        pCurr->DecrementReference();
        pCurr->DecrementReference();
    }
}

void DecReferencePictureMarking::Undo(const H264DecoderFrame * frame)
{
    MakeChange(true, frame);
}

void DecReferencePictureMarking::Redo(const H264DecoderFrame * frame)
{
    MakeChange(false, frame);
}

void DecReferencePictureMarking::MakeChange(bool isUndo, const H264DecoderFrame * frame)
{
    DPBCommandsList::iterator iter = m_commandsList.begin();
    DPBCommandsList::iterator end_iter = m_commandsList.end();

    for (; iter != end_iter; ++iter)
    {
        DPBChangeItem& item = *iter;

        if (item.m_pCurrentFrame == frame)
        {
            MakeChange(isUndo, &item);
        }
    }
}

void DecReferencePictureMarking::RemoveOld()
{
    if (!m_commandsList.size())
    {
        m_frameCount = 0;
        return;
    }

    Remove(m_commandsList.front().m_pCurrentFrame);
}

void DecReferencePictureMarking::DebugPrintItems()
{
    printf("==================================\n");

    DPBCommandsList::iterator iter = m_commandsList.begin(); // stl issue. we can't reuse iterator
    for (; iter != m_commandsList.end(); ++iter)
    {
        DPBChangeItem& item = *iter;
        printf("current - %d, ref - %d\n", item.m_pCurrentFrame->m_PicOrderCnt[0], item.m_pRefFrame->m_PicOrderCnt[0]);
    }

    printf("==================================\n");
}

void DecReferencePictureMarking::Remove(H264DecoderFrame * frame)
{
    //printf("remove frame - %d\n", frame->m_PicOrderCnt[0]);
    //DebugPrintItems();

    DPBCommandsList::iterator iter = m_commandsList.begin();
    DPBCommandsList::iterator end_iter = m_commandsList.end();

    DPBCommandsList::iterator start = m_commandsList.end();
    DPBCommandsList::iterator end = m_commandsList.end();

    for (; iter != end_iter; ++iter)
    {
        DPBChangeItem& item = *iter;

        if (start == m_commandsList.end())
        {
            if (item.m_pCurrentFrame == frame)
            {
                m_frameCount--;
                start = iter;
            }
        }
        else
        {
            if (item.m_pCurrentFrame != frame)
            {
                end = iter;
                break;
            }
        }
    }

    if (start != m_commandsList.end())
        m_commandsList.erase(m_commandsList.begin(), end);

    {
        DPBCommandsList::iterator iter = m_commandsList.begin();

        for (; iter != m_commandsList.end(); ++iter)
        {
            DPBChangeItem& item = *iter;

            if (item.m_pRefFrame == frame)
            {
                m_commandsList.erase(iter);
                iter = m_commandsList.begin();
                if (iter == m_commandsList.end()) // avoid ++iter operation
                    break;
            }
        }
    }

    //DebugPrintItems();
}

DecReferencePictureMarking::DPBChangeItem* DecReferencePictureMarking::AddItemAndRun(H264DecoderFrame * currentFrame, H264DecoderFrame * refFrame, Ipp32u flags)
{
    DPBChangeItem* item = AddItem(m_commandsList, currentFrame, refFrame, flags);
    if (!item)
        return 0;

    Redo(item);
    return item;
}

DecReferencePictureMarking::DPBChangeItem* DecReferencePictureMarking::AddItem(DPBCommandsList & list, H264DecoderFrame * currentFrame, H264DecoderFrame * refFrame, Ipp32u flags)
{
    if (!currentFrame || !refFrame)
        return 0;

    DPBChangeItem item;
    item.m_pCurrentFrame = currentFrame;
    item.m_pRefFrame = refFrame;

    item.m_type.isShortTerm = (flags & SHORT_TERM) ? 1 : 0;
    item.m_type.isFrame     = (flags & FULL_FRAME) ? 1 : 0;
    item.m_type.isSet       = (flags & SET_REFERENCE) ? 1 : 0;
    item.m_type.isBottom    = (flags & BOTTOM_FIELD) ? 1 : 0;

    if (CheckUseless(&item))
        return 0;

    list.push_back(item);
    return &list.back();
}

bool DecReferencePictureMarking::CheckUseless(DPBChangeItem * item)
{
    if (!item)
        return true;

    if (item->m_type.isShortTerm)
    {
        if (item->m_type.isFrame)
        {
            return item->m_type.isSet ? (item->m_pRefFrame->isShortTermRef() == 3) : (!item->m_pRefFrame->isShortTermRef());
        }
        else
        {
            return item->m_type.isSet ? item->m_pRefFrame->isShortTermRef(item->m_type.isBottom) : !item->m_pRefFrame->isShortTermRef(item->m_type.isBottom);
        }
    }
    else
    {
        if (item->m_type.isFrame)
        {
            return item->m_type.isSet ? item->m_pRefFrame->isLongTermRef() == 3 : !item->m_pRefFrame->isLongTermRef();
        }
        else
        {
            return item->m_type.isSet ? item->m_pRefFrame->isLongTermRef(item->m_type.isBottom) : !item->m_pRefFrame->isLongTermRef(item->m_type.isBottom);
        }
    }
}

void DecReferencePictureMarking::MakeChange(bool isUndo, const DPBChangeItem * item)
{
    if (!item)
        return;

    DPBChangeItem temp = *item;

    if (isUndo)
    {
        temp.m_type.isSet ^= 1;
    }

    item = &temp;

    Ipp32s savePSRef = item->m_pRefFrame->m_PictureStructureForRef;
    item->m_pRefFrame->m_PictureStructureForRef = item->m_pCurrentFrame->m_PictureStructureForDec;

    if (item->m_type.isFrame)
    {
        if (item->m_type.isShortTerm)
        {
            item->m_pRefFrame->SetisShortTermRef(item->m_type.isSet == 1, 0);
            item->m_pRefFrame->SetisShortTermRef(item->m_type.isSet == 1, 1);
        }
        else
        {
            item->m_pRefFrame->SetisLongTermRef(item->m_type.isSet == 1, 0);
            item->m_pRefFrame->SetisLongTermRef(item->m_type.isSet == 1, 1);
        }
    }
    else
    {
        if (item->m_type.isShortTerm)
        {
            item->m_pRefFrame->SetisShortTermRef(item->m_type.isSet == 1, item->m_type.isBottom);
        }
        else
        {
            item->m_pRefFrame->SetisLongTermRef(item->m_type.isSet == 1, item->m_type.isBottom);
        }
    }

    item->m_pRefFrame->m_PictureStructureForRef = savePSRef;
}

void DecReferencePictureMarking::Undo(const DPBChangeItem * item)
{
    MakeChange(true, item);
}

void DecReferencePictureMarking::Redo(const DPBChangeItem * item)
{
    MakeChange(false, item);
}

void DecReferencePictureMarking::SlideWindow(ViewItem &view, H264Slice * pSlice, Ipp32s field_index)
{
    Ipp32u NumShortTermRefs, NumLongTermRefs;
    const H264SeqParamSet* sps = pSlice->GetSeqParam();

    Ipp32s dId = pSlice->GetSliceHeader()->nal_ext.svc.dependency_id;
    Ipp32s index = GetDPBLayerIndex(pSlice->GetCurrentFrame()->m_maxDId, dId);

    // find out how many active reference frames currently in decoded
    // frames buffer
    view.GetDPBList(index)->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
    while (NumShortTermRefs > 0 &&
        (NumShortTermRefs + NumLongTermRefs >= sps->num_ref_frames) &&
        !field_index)
    {
        // mark oldest short-term reference as unused
        VM_ASSERT(NumShortTermRefs > 0);

        H264DecoderFrame * pFrame = view.GetDPBList(index)->findOldestShortTermRef();

        if (!pFrame)
            break;

        AddItemAndRun(pSlice->GetCurrentFrame(), pFrame, UNSET_REFERENCE | FULL_FRAME | SHORT_TERM);
        NumShortTermRefs--;
    }
}

Status DecReferencePictureMarking::UpdateRefPicMarking(ViewItem &view, H264DecoderFrame * pFrame, H264Slice * pSlice, Ipp32s field_index)
{
    Status umcRes = UMC_OK;
    bool bCurrentisST = true;

    m_frameCount++;

    H264SliceHeader const * sliceHeader = pSlice->GetSliceHeader();
    Ipp32s dId = sliceHeader->nal_ext.svc.dependency_id;
    Ipp32s index = GetDPBLayerIndex(pFrame->m_maxDId, dId);

    // set MVC 'inter view flag'
    pFrame->SetInterViewRef(0 != sliceHeader->nal_ext.mvc.inter_view_flag, field_index);

    if (pFrame->m_bIDRFlag)
    {
        // mark all reference pictures as unused
        for (H264DecoderFrame *pCurr = view.GetDPBList(index)->head(); pCurr; pCurr = pCurr->future())
        {
            if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
            {
                AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | SHORT_TERM);
                AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | LONG_TERM);
            }
        }

        if (sliceHeader->long_term_reference_flag)
        {
            AddItemAndRun(pFrame, pFrame, SET_REFERENCE | LONG_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
            //pFrame->SetisLongTermRef(true, field_index);
            pFrame->setLongTermFrameIdx(0);
            view.MaxLongTermFrameIdx[index] = 0;
        }
        else
        {
            AddItemAndRun(pFrame, pFrame, SET_REFERENCE | SHORT_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
            //pFrame->SetisShortTermRef(true, field_index);
            view.MaxLongTermFrameIdx[index] = -1;        // no long term frame indices
        }

        bCurrentisST = false;
    }
    else
    {
        AdaptiveMarkingInfo* pAdaptiveMarkingInfo = pSlice->GetAdaptiveMarkingInfo();
        // adaptive ref pic marking
        if (pAdaptiveMarkingInfo && pAdaptiveMarkingInfo->num_entries > 0)
        {
            for (Ipp32u arpmmf_idx = 0; arpmmf_idx < pAdaptiveMarkingInfo->num_entries; arpmmf_idx++)
            {
                Ipp32s LongTermFrameIdx;
                Ipp32s picNum;
                H264DecoderFrame * pRefFrame = 0;
#if 1
                Ipp32s field = 0;
#endif

                switch (pAdaptiveMarkingInfo->mmco[arpmmf_idx])
                {
                case 1:
                    // mark a short-term picture as unused for reference
                    // Value is difference_of_pic_nums_minus1
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);

#if 1
                    pRefFrame = view.GetDPBList(index)->findShortTermPic(picNum, &field);
                    AddItemAndRun(pFrame, pRefFrame, UNSET_REFERENCE | SHORT_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
#else
                    pRefFrame = view.pDPB->freeShortTermRef(picNum);
#endif
                    break;
                case 2:
                    // mark a long-term picture as unused for reference
                    // value is long_term_pic_num
                    picNum = pAdaptiveMarkingInfo->value[arpmmf_idx*2];
#if 1
                    pRefFrame = view.GetDPBList(index)->findLongTermPic(picNum, &field);
                    AddItemAndRun(pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
#else
                    pRefFrame = view.pDPB->freeLongTermRef(picNum);
#endif
                    break;
                case 3:
                    // Assign a long-term frame idx to a short-term picture
                    // Value is difference_of_pic_nums_minus1 followed by
                    // long_term_frame_idx. Only this case uses 2 value entries.
                    picNum = pFrame->PicNum(field_index) -
                        (pAdaptiveMarkingInfo->value[arpmmf_idx*2] + 1);
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2+1];

#if 1
                    pRefFrame = view.GetDPBList(index)->findShortTermPic(picNum, &field);
                    if (!pRefFrame)
                        break;

                    {
                        H264DecoderFrame * longTerm = view.GetDPBList(index)->findLongTermRefIdx(LongTermFrameIdx);
                        if (longTerm != pRefFrame)
                            AddItemAndRun(pFrame, longTerm, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);
                    }

                    AddItemAndRun(pFrame, pRefFrame, SET_REFERENCE | LONG_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));
                    AddItemAndRun(pFrame, pRefFrame, UNSET_REFERENCE | SHORT_TERM | (field ? BOTTOM_FIELD : TOP_FIELD));

                    pRefFrame->setLongTermFrameIdx(LongTermFrameIdx);
                    pRefFrame->UpdateLongTermPicNum(pRefFrame->m_PictureStructureForRef >= FRM_STRUCTURE ? 2 : pRefFrame->m_bottom_field_flag[field]);
#else
                    // First free any existing LT reference with the LT idx
                    pRefFrame = view.pDPB->freeLongTermRefIdx(LongTermFrameIdx, pRefFrame);

                    view.pDPB->changeSTtoLTRef(picNum, LongTermFrameIdx);
#endif
                    break;
                case 4:
                    // Specify max long term frame idx
                    // Value is max_long_term_frame_idx_plus1
                    // Set to "no long-term frame indices" (-1) when value == 0.
                    view.MaxLongTermFrameIdx[index] = pAdaptiveMarkingInfo->value[arpmmf_idx*2] - 1;

#if 1
                    pRefFrame = view.GetDPBList(index)->findOldLongTermRef(view.MaxLongTermFrameIdx[index]);
                    while (pRefFrame)
                    {
                        AddItemAndRun(pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);
                        pRefFrame = view.GetDPBList(index)->findOldLongTermRef(view.MaxLongTermFrameIdx[index]);
                    }
#else
                    // Mark any long-term reference frames with a larger LT idx
                    // as unused for reference.
                    view.pDPB->freeOldLongTermRef(view.MaxLongTermFrameIdx);
#endif
                    break;
                case 5:
                    // Mark all as unused for reference
                    // no value
                    for (H264DecoderFrame *pCurr = view.GetDPBList(index)->head(); pCurr; pCurr = pCurr->future())
                    {
                        if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
                        {
                            AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | SHORT_TERM);
                            AddItemAndRun(pFrame, pCurr, UNSET_REFERENCE | FULL_FRAME | LONG_TERM);
                        }
                    }

                    view.GetDPBList(index)->IncreaseRefPicListResetCount(pFrame);
                    view.MaxLongTermFrameIdx[index] = -1;        // no long term frame indices
                    // set "previous" picture order count vars for future

                    if (pFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                    {
                        pFrame->setPicOrderCnt(0, field_index);
                        pFrame->setPicNum(0, field_index);
                    }
                    else
                    {
                        Ipp32s poc = pFrame->PicOrderCnt(0, 3);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(0, 1) - poc, 0);
                        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(1, 1) - poc, 1);
                        pFrame->setPicNum(0, 0);
                        pFrame->setPicNum(0, 1);
                    }

                    pFrame->m_bIDRFlag = true;
                    view.GetPOCDecoder(index)->Reset(0);
                    // set frame_num to zero for this picture, for correct
                    // FrameNumWrap
                    pFrame->setFrameNum(0);
                    break;
                case 6:
                    // Assign long term frame idx to current picture
                    // Value is long_term_frame_idx
                    LongTermFrameIdx = pAdaptiveMarkingInfo->value[arpmmf_idx*2];
                    bCurrentisST = false;

#if 1
                    pRefFrame = view.GetDPBList(index)->findLongTermRefIdx(LongTermFrameIdx);
                    if (pRefFrame != pFrame)
                        AddItemAndRun(pFrame, pRefFrame, UNSET_REFERENCE | LONG_TERM | FULL_FRAME);

                    AddItemAndRun(pFrame, pFrame, SET_REFERENCE | LONG_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));

#else
                    // First free any existing LT reference with the LT idx
                    pRefFrame = view.pDPB->freeLongTermRefIdx(LongTermFrameIdx, pFrame);

                    // Mark current
                    pFrame->SetisLongTermRef(true, field_index);
#endif
                    pFrame->setLongTermFrameIdx(LongTermFrameIdx);
                    break;
                case 0:
                default:
                    // invalid mmco command in bitstream
                    VM_ASSERT(0);
                    umcRes = UMC_ERR_INVALID_STREAM;
                }    // switch
            }    // for arpmmf_idx
        }
    }    // not IDR picture

    if (bCurrentisST)
    { // set current as
        if (sliceHeader->field_pic_flag && field_index)
        {
        }
        else
        {
            SlideWindow(view, pSlice, field_index);
        }

#if 1
        AddItemAndRun(pFrame, pFrame, SET_REFERENCE | SHORT_TERM | (field_index ? BOTTOM_FIELD : TOP_FIELD));
#else
        pFrame->SetisShortTermRef(true, field_index);
#endif
    }

    return umcRes;
}

/****************************************************************************************************/
//
/****************************************************************************************************/
void OnSlideWindow(H264DecoderFrame *pRefFrame)
{
    if (pRefFrame && !pRefFrame->IsFrameExist())
    {
        pRefFrame->setWasOutputted();
        pRefFrame->setWasDisplayed();
        return;
    }
}

static
bool IsNeedSPSInvalidate(const H264SeqParamSet *old_sps, const H264SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->frame_width_in_mbs != new_sps->frame_width_in_mbs)
        return true;

    if (old_sps->frame_height_in_mbs != new_sps->frame_height_in_mbs)
        return true;

    if (old_sps->max_dec_frame_buffering < new_sps->max_dec_frame_buffering)
        return true;

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

/****************************************************************************************************/
// MVC_Extension class routine
/****************************************************************************************************/
MVC_Extension::MVC_Extension()
    : m_temporal_id(H264_MAX_TEMPORAL_ID)
    , m_priority_id(63)
    , m_level_idc(0)
    , m_currentDisplayView(BASE_VIEW)
    , m_currentView((Ipp32u)INVALID_VIEW_ID)
    , m_decodingMode(UNKNOWN_DECODING_MODE)
{
    Reset();
}

MVC_Extension::~MVC_Extension()
{
    Close();
}

bool MVC_Extension::IsExtension() const
{
    return m_decodingMode != AVC_DECODING_MODE;
}

Status MVC_Extension::Init()
{
    MVC_Extension::Close();

    Status umcRes = AllocateView((Ipp32u)INVALID_VIEW_ID);
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    return UMC_OK;
}

void MVC_Extension::Close()
{
    MVC_Extension::Reset();
    m_viewIDsList.clear();
    m_views.clear();
}

void MVC_Extension::Reset()
{
    m_temporal_id = H264_MAX_TEMPORAL_ID;
    m_priority_id = 63;
    m_level_idc = 0;
    m_currentDisplayView = BASE_VIEW;
    m_currentView = (Ipp32u)INVALID_VIEW_ID;
    m_decodingMode = UNKNOWN_DECODING_MODE;

    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    for (; iter != iter_end; ++iter)
    {
        iter->Reset();
    }
}

ViewItem * MVC_Extension::FindView(Ipp32s viewId)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    // run over the list and try to find the corresponding view
    for (; iter != iter_end; ++iter)
    {
        ViewItem & item = *iter;
        if (item.viewId == viewId)
        {
            return &item;
        }
    }

    return NULL;
}

ViewItem & MVC_Extension::GetView(Ipp32s viewId)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    // run over the list and try to find the corresponding view
    for (; iter != iter_end; ++iter)
    {
        ViewItem & item = *iter;

        //if (viewId == INVALID_VIEW_ID && item.m_isDisplayable)
          //  return &item;

        if (item.viewId == viewId)
        {
            return item;
        }
    }

    throw h264_exception(UMC_ERR_FAILED);
    //return NULL;
}

void MVC_Extension::MoveViewToHead(Ipp32s view_id)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    // run over the list and try to find the corresponding view
    for (; iter != iter_end; ++iter)
    {
        if (iter->viewId == view_id)
        {
            ViewItem item = *iter;
            ViewItem &front = m_views.front();
            *iter = front;
            front = item;
            break;
        }
    }
}

ViewItem & MVC_Extension::GetViewByNumber(Ipp32s viewNum)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    // run over the list and try to find the corresponding view
    for (Ipp32s i = 0; iter != iter_end; ++iter, i++)
    {
        ViewItem & item = *iter;
        if (i == viewNum)
            return item;
    }

    throw h264_exception(UMC_ERR_FAILED);
    //return NULL;
}

Ipp32s MVC_Extension::GetBaseViewId() const
{
    return BASE_VIEW;
}

Ipp32s MVC_Extension::GetViewCount() const
{
    return (Ipp32s)m_views.size();
}

bool MVC_Extension::IncreaseCurrentView()
{
    bool isWrapped = false;
    for (size_t i = 0; i < m_views.size(); ++i)
    {
        m_currentDisplayView++;
        if (m_views.size() == m_currentDisplayView)
        {
            m_currentDisplayView = BASE_VIEW;
            isWrapped = true;
        }

        ViewItem & view = GetViewByNumber(m_currentDisplayView);
        if (view.m_isDisplayable)
            break;
    }

    return isWrapped;
}

void MVC_Extension::SetTemporalId(Ipp32u temporalId)
{
    m_temporal_id = temporalId;
}

Status MVC_Extension::SetViewList(const std::vector<Ipp32u> & targetView, const std::vector<Ipp32u> & dependencyList)
{
    for (size_t i = 0; i < targetView.size(); i++)
    {
        m_viewIDsList.push_back(targetView[i]);
    }

    /*if (std::find(m_viewIDsList.begin(), m_viewIDsList.end(), (int) BASE_VIEW) == m_viewIDsList.end())
    {
        ViewItem * viewItem = GetView(BASE_VIEW);
        if (!viewItem)
            return UMC_ERR_FAILED;

        viewItem->m_isDisplayable = false;
    }*/

    for (size_t i = 0; i < dependencyList.size(); i++)
    {
        Status umcRes = AllocateView(dependencyList[i]);
        if (UMC_OK != umcRes)
        {
            return umcRes;
        }

        ViewItem & viewItem = GetView(dependencyList[i]);
        viewItem.m_isDisplayable = false;
        m_viewIDsList.push_back(dependencyList[i]);
    }

    m_viewIDsList.sort();
    m_viewIDsList.unique();

    return UMC_OK;
}

Ipp32u MVC_Extension::GetLevelIDC() const
{
    return m_level_idc;
}

ViewItem & MVC_Extension::AllocateAndInitializeView(H264Slice * slice)
{
    ViewItem * view = FindView(slice->GetSliceHeader()->nal_ext.mvc.view_id);
    if (view)
        return *view;

    Status umcRes = AllocateView(slice->GetSliceHeader()->nal_ext.mvc.view_id);
    if (UMC_OK != umcRes)
    {
        throw h264_exception(umcRes);
    }

    ViewItem &viewRef = GetView(slice->GetSliceHeader()->nal_ext.mvc.view_id);
    viewRef.SetDPBSize(const_cast<H264SeqParamSet*>(slice->m_pSeqParamSet), m_level_idc);
    viewRef.maxDecFrameBuffering = slice->m_pSeqParamSet->max_dec_frame_buffering ?
                                  slice->m_pSeqParamSet->max_dec_frame_buffering :
                                  viewRef.dpbSize;

    return viewRef;
}

Status MVC_Extension::AllocateView(Ipp32s view_id)
{
    ViewItem view;

    // check error(s)
    if (((Ipp32s)H264_MAX_NUM_VIEW <= view_id) && (view_id != (Ipp32s)INVALID_VIEW_ID) )
    {
        return UMC_ERR_INVALID_PARAMS;
    }

    // already allocated
    if (FindView(view_id))
    {
        return UMC_OK;
    }

    if (FindView(INVALID_VIEW_ID))
    {
        ViewItem &view = GetView((Ipp32u)INVALID_VIEW_ID);
        view.viewId = view_id;
        return UMC_OK;
    }

    try
    {
        // allocate DPB and POC counter
        for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++)
        {
            view.pDPB[i].reset(new H264DBPList());
            view.pPOCDec[i].reset(new POCDecoder());
        }
        view.viewId = view_id;
        view.dpbSize = 16;
    }
    catch(...)
    {
        return UMC_ERR_ALLOC;
    }

    // reset the variables
    m_views.push_back(view);

    return UMC_OK;

} // Status AllocateView(Ipp32u view_id)

bool MVC_Extension::IsShouldSkipSlice(H264NalExtension *nal_ext)
{
    // check is view at view_list;
    ViewIDsList::const_iterator iter = std::find(m_viewIDsList.begin(), m_viewIDsList.end(), nal_ext->mvc.view_id);
    if (iter == m_viewIDsList.end() && m_viewIDsList.size())
        return true;

    if (nal_ext->mvc.temporal_id > m_temporal_id)
        return true;

    if (nal_ext->mvc.priority_id > m_priority_id)
        return true;

    return false;
}

bool MVC_Extension::IsShouldSkipSlice(H264Slice * slice)
{
    bool isMVC = slice->GetSeqMVCParam() != 0;

    if (!isMVC)
        return false;

    return IsShouldSkipSlice(&slice->GetSliceHeader()->nal_ext);
}

void MVC_Extension::AnalyzeDependencies(const H264SeqParamSetMVCExtension *)
{
#if 0
    ViewIDsList::iterator iter = m_viewIDsList.begin();
    ViewIDsList::iterator iter_end = m_viewIDsList.end();

    std::vector<Ipp32u> newElements;

    for (; iter != iter_end; ++iter)
    {
        Ipp32u viewId = *iter;
        const H264ViewRefInfo & refInfo = extension->viewInfo[viewId];

        bool wasAdded = false;

        for (Ipp32u i = 0; i < 2; i++)
        {
            for (Ipp32u j = 0; j < refInfo.num_anchor_refs_lx[i]; j++)
            {
                Ipp32u dependOnViewId = refInfo.anchor_refs_lx[i][j];

                ViewIDsList::const_iterator view_iter = std::find(m_viewIDsList.begin(), m_viewIDsList.end(), dependOnViewId);
                if (view_iter == m_viewIDsList.end())
                {
                    m_viewIDsList.push_back(dependOnViewId);
                    wasAdded = true;
                    break;
                }
            }

            if (wasAdded)
                break;
        }

        if (wasAdded)
        {
            iter = m_viewIDsList.begin();
        }
    }
#endif
}

void MVC_Extension::ChooseLevelIdc(const H264SeqParamSetMVCExtension * extension, Ipp8u baseViewLevelIDC, Ipp8u extensionLevelIdc)
{
    if (m_level_idc)
        return;

    VM_ASSERT(extension->viewInfo.Size() == extension->num_views_minus1 + 1);

    if (!m_viewIDsList.size())
    {
        for (size_t i = 0; i <= extension->num_views_minus1; i++)
        {
            m_viewIDsList.push_back(extension->viewInfo[i].view_id);
        }

        ViewIDsList::iterator iter = m_viewIDsList.begin();
        ViewIDsList::iterator iter_end = m_viewIDsList.end();
        for (; iter != iter_end; ++iter)
        {
            Ipp32u targetView = *iter;
            ViewItem * view = FindView(targetView);
            if (!view)
                continue;

            view->m_isDisplayable = true;
        }
    }

    AnalyzeDependencies(extension);

    m_level_idc = 0;

    for (Ipp32u i = 0; i <= extension->num_level_values_signalled_minus1; i++)
    {
        const H264LevelValueSignaled & levelInfo = extension->levelInfo[i];

        for (Ipp32u j = 0; j <= levelInfo.num_applicable_ops_minus1; j++)
        {
            const H264ApplicableOp & operationPoint = levelInfo.opsInfo[j];

            ViewIDsList::const_iterator iter = m_viewIDsList.begin();
            ViewIDsList::const_iterator iter_end = m_viewIDsList.end();

            bool foundAll = true;

            for (; iter != iter_end; ++iter)
            {
                bool found = false;
                Ipp32u targetView = *iter;
                for (Ipp32u tv = 0; tv <= operationPoint.applicable_op_num_target_views_minus1; tv++)
                {
                    if (targetView == operationPoint.applicable_op_target_view_id[tv])
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    foundAll = false;
                    break;
                }
            }

            if (!foundAll)
                break;

            if (!m_level_idc || m_level_idc > levelInfo.level_idc)
            {
                m_level_idc = levelInfo.level_idc;
            }
        }
    }

    Ipp8u maxLevel = IPP_MAX(baseViewLevelIDC, extensionLevelIdc);
    m_level_idc = IPP_MAX(maxLevel, m_level_idc);

    if (!m_level_idc)
    {
        VM_ASSERT(false);
    }
}

/****************************************************************************************************/
// Skipping class routine
/****************************************************************************************************/
Skipping::Skipping()
    : m_VideoDecodingSpeed(0)
    , m_SkipCycle(1)
    , m_ModSkipCycle(1)
    , m_PermanentTurnOffDeblocking(0)
    , m_SkipFlag(0)
    , m_NumberOfSkippedFrames(0)
{
}

Skipping::~Skipping()
{
}

void Skipping::Reset()
{
    m_VideoDecodingSpeed = 0;
    m_SkipCycle = 0;
    m_ModSkipCycle = 0;
    m_PermanentTurnOffDeblocking = 0;
    m_NumberOfSkippedFrames = 0;
}

void Skipping::PermanentDisableDeblocking(bool disable)
{
    m_PermanentTurnOffDeblocking = disable ? 3 : 0;
}

bool Skipping::IsShouldSkipDeblocking(H264DecoderFrame * pFrame, Ipp32s field)
{
    return (IS_SKIP_DEBLOCKING_MODE_PREVENTIVE || IS_SKIP_DEBLOCKING_MODE_PERMANENT ||
        (IS_SKIP_DEBLOCKING_MODE_NON_REF && !pFrame->GetAU(field)->IsReference()));
}

bool Skipping::IsShouldSkipFrame(H264DecoderFrame * pFrame, Ipp32s /*field*/)
{
    bool isShouldSkip = false;

    //bool isReference = pFrame->GetAU(field)->IsReference();

    bool isReference0 = pFrame->GetAU(0)->IsReference();
    bool isReference1 = pFrame->GetAU(1)->IsReference();

    bool isReference = isReference0 || isReference1;

    if ((m_VideoDecodingSpeed > 0) && !isReference)
    {
        if ((m_SkipFlag % m_ModSkipCycle) == 0)
        {
            isShouldSkip = true;
        }

        m_SkipFlag++;

        if (m_SkipFlag >= m_SkipCycle)
            m_SkipFlag = 0;
    }

    if (isShouldSkip)
        m_NumberOfSkippedFrames++;

    return isShouldSkip;
}

void Skipping::ChangeVideoDecodingSpeed(Ipp32s & num)
{
    m_VideoDecodingSpeed += num;

    if (m_VideoDecodingSpeed < 0)
        m_VideoDecodingSpeed = 0;
    if (m_VideoDecodingSpeed > 7)
        m_VideoDecodingSpeed = 7;

    num = m_VideoDecodingSpeed;

    Ipp32s deblocking_off = m_PermanentTurnOffDeblocking;

    if (m_VideoDecodingSpeed > 6)
    {
        m_SkipCycle = 1;
        m_ModSkipCycle = 1;
        m_PermanentTurnOffDeblocking = 2;
    }
    else if (m_VideoDecodingSpeed > 5)
    {
        m_SkipCycle = 1;
        m_ModSkipCycle = 1;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 4)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 1;
    }
    else if (m_VideoDecodingSpeed > 3)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 2)
    {
        m_SkipCycle = 2;
        m_ModSkipCycle = 2;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed > 1)
    {
        m_SkipCycle = 3;
        m_ModSkipCycle = 3;
        m_PermanentTurnOffDeblocking = 0;
    }
    else if (m_VideoDecodingSpeed == 1)
    {
        m_SkipCycle = 4;
        m_ModSkipCycle = 4;
        m_PermanentTurnOffDeblocking = 0;
    }
    else
    {
        m_PermanentTurnOffDeblocking = 0;
    }

    if (deblocking_off == 3)
        m_PermanentTurnOffDeblocking = 3;
}

H264VideoDecoder::SkipInfo Skipping::GetSkipInfo() const
{
    H264VideoDecoder::SkipInfo info;
    info.isDeblockingTurnedOff = (m_VideoDecodingSpeed == 5) || (m_VideoDecodingSpeed == 7);
    info.numberOfSkippedFrames = m_NumberOfSkippedFrames;
    return info;
}

/****************************************************************************************************/
// POCDecoder
/****************************************************************************************************/
POCDecoder::POCDecoder()
{
    Reset();
}

POCDecoder::~POCDecoder()
{
}

void POCDecoder::Reset(Ipp32s IDRFrameNum)
{
    m_PicOrderCnt = 0;
    m_PicOrderCntMsb = 0;
    m_PicOrderCntLsb = 0;
    m_FrameNum = IDRFrameNum;
    m_PrevFrameRefNum = IDRFrameNum;
    m_FrameNumOffset = 0;
    m_TopFieldPOC = 0;
    m_BottomFieldPOC = 0;
}

void POCDecoder::DecodePictureOrderCount(const H264Slice *slice, Ipp32s frame_num)
{
    const H264SliceHeader *sliceHeader = slice->GetSliceHeader();
    const H264SeqParamSet* sps = slice->GetSeqParam();

    Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);

    if (sps->pic_order_cnt_type == 0)
    {
        // pic_order_cnt type 0
        Ipp32s CurrPicOrderCntMsb;
        Ipp32s MaxPicOrderCntLsb = sps->MaxPicOrderCntLsb;

        if ((sliceHeader->pic_order_cnt_lsb < m_PicOrderCntLsb) &&
             ((m_PicOrderCntLsb - sliceHeader->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb >> 1)))
            CurrPicOrderCntMsb = m_PicOrderCntMsb + MaxPicOrderCntLsb;
        else if ((sliceHeader->pic_order_cnt_lsb > m_PicOrderCntLsb) &&
                ((sliceHeader->pic_order_cnt_lsb - m_PicOrderCntLsb) > (MaxPicOrderCntLsb >> 1)))
            CurrPicOrderCntMsb = m_PicOrderCntMsb - MaxPicOrderCntLsb;
        else
            CurrPicOrderCntMsb = m_PicOrderCntMsb;

        if (sliceHeader->nal_ref_idc)
        {
            // reference picture
            m_PicOrderCntMsb = CurrPicOrderCntMsb & (~(MaxPicOrderCntLsb - 1));
            m_PicOrderCntLsb = sliceHeader->pic_order_cnt_lsb;
        }
        m_PicOrderCnt = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
        if (sliceHeader->field_pic_flag == 0)
        {
             m_TopFieldPOC = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
             m_BottomFieldPOC = m_TopFieldPOC + sliceHeader->delta_pic_order_cnt_bottom;
        }

    }    // pic_order_cnt type 0
    else if (sps->pic_order_cnt_type == 1)
    {
        // pic_order_cnt type 1
        Ipp32u i;
        Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic
        Ipp32u uPicOrderCycleCnt = 0;
        Ipp32u uFrameNuminPicOrderCntCycle = 0;
        Ipp32s ExpectedPicOrderCnt = 0;
        Ipp32s ExpectedDeltaPerPicOrderCntCycle;
        Ipp32u uNumRefFramesinPicOrderCntCycle = sps->num_ref_frames_in_pic_order_cnt_cycle;

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += uMaxFrameNum;

        if (uNumRefFramesinPicOrderCntCycle != 0)
            uAbsFrameNum = m_FrameNumOffset + frame_num;
        else
            uAbsFrameNum = 0;

        if ((sliceHeader->nal_ref_idc == false)  && (uAbsFrameNum > 0))
            uAbsFrameNum--;

        if (uAbsFrameNum)
        {
            uPicOrderCycleCnt = (uAbsFrameNum - 1) /
                    uNumRefFramesinPicOrderCntCycle;
            uFrameNuminPicOrderCntCycle = (uAbsFrameNum - 1) %
                    uNumRefFramesinPicOrderCntCycle;
        }

        ExpectedDeltaPerPicOrderCntCycle = 0;
        for (i=0; i < uNumRefFramesinPicOrderCntCycle; i++)
        {
            ExpectedDeltaPerPicOrderCntCycle +=
                sps->poffset_for_ref_frame[i];
        }

        if (uAbsFrameNum)
        {
            ExpectedPicOrderCnt = uPicOrderCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
            for (i=0; i<=uFrameNuminPicOrderCntCycle; i++)
            {
                ExpectedPicOrderCnt +=
                    sps->poffset_for_ref_frame[i];
            }
        }
        else
            ExpectedPicOrderCnt = 0;

        if (sliceHeader->nal_ref_idc == false)
            ExpectedPicOrderCnt += sps->offset_for_non_ref_pic;
        m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[0];
        if( sliceHeader->field_pic_flag==0)
        {
            m_TopFieldPOC = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
            m_BottomFieldPOC = m_TopFieldPOC +
                sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 1 ];
        }
        else if( ! sliceHeader->bottom_field_flag)
            m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
        else
            m_PicOrderCnt  = ExpectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 0 ];
    }    // pic_order_cnt type 1
    else if (sps->pic_order_cnt_type == 2)
    {
        // pic_order_cnt type 2
        Ipp32s iMaxFrameNum = (1<<sps->log2_max_frame_num);
        Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += iMaxFrameNum;
        uAbsFrameNum = frame_num + m_FrameNumOffset;
        m_PicOrderCnt = uAbsFrameNum*2;
        if (sliceHeader->nal_ref_idc == false)
            m_PicOrderCnt--;
            m_TopFieldPOC = m_PicOrderCnt;
            m_BottomFieldPOC = m_PicOrderCnt;

    }    // pic_order_cnt type 2

    if (sliceHeader->nal_ref_idc)
    {
        m_PrevFrameRefNum = frame_num;
    }

    m_FrameNum = frame_num;
}    // decodePictureOrderCount

Ipp32s POCDecoder::DetectFrameNumGap(H264Slice *slice)
{
    const H264SeqParamSet* sps = slice->GetSeqParam();

    if (sps->gaps_in_frame_num_value_allowed_flag != 1)
        return 0;

    H264SliceHeader *sliceHeader = slice->GetSliceHeader();

    Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);
    Ipp32s frameNumGap;

    if (sliceHeader->IdrPicFlag)
        return 0;

    // Capture any frame_num gap
    if (sliceHeader->frame_num != m_PrevFrameRefNum &&
        sliceHeader->frame_num != (m_PrevFrameRefNum + 1) % uMaxFrameNum)
    {
        // note this could be negative if frame num wrapped

        if (sliceHeader->frame_num > m_PrevFrameRefNum - 1)
        {
            frameNumGap = (sliceHeader->frame_num - m_PrevFrameRefNum - 1) % uMaxFrameNum;
        }
        else
        {
            frameNumGap = (uMaxFrameNum - (m_PrevFrameRefNum + 1 - sliceHeader->frame_num)) % uMaxFrameNum;
        }
    }
    else
    {
        frameNumGap = 0;
    }

    return frameNumGap;
}

void POCDecoder::DecodePictureOrderCountFrameGap(H264DecoderFrame *pFrame,
                                                 const H264SliceHeader *pSliceHeader,
                                                 Ipp32s frameNum)
{
    m_PrevFrameRefNum = frameNum;
    m_FrameNum = frameNum;

    if (pSliceHeader->field_pic_flag)
    {
        pFrame->setPicOrderCnt(m_PicOrderCnt, 0);
        pFrame->setPicOrderCnt(m_PicOrderCnt, 1);
    }
    else
    {
        pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
        pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
    }
}

void POCDecoder::DecodePictureOrderCountFakeFrames(H264DecoderFrame *pFrame,
                                                   const H264SliceHeader *pSliceHeader)
{
    if (pSliceHeader->field_pic_flag)
    {
        pFrame->setPicOrderCnt(m_PicOrderCnt, 0);
        pFrame->setPicOrderCnt(m_PicOrderCnt, 1);
    }
    else
    {
        pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
        pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
    }

}

void POCDecoder::DecodePictureOrderCountInitFrame(H264DecoderFrame *pFrame,
                                                  Ipp32s fieldIdx)
{
    //transfer previosly calculated PicOrdeCnts into current Frame
    if (pFrame->m_PictureStructureForRef < FRM_STRUCTURE)
    {
        pFrame->setPicOrderCnt(m_PicOrderCnt, fieldIdx);
        if (!fieldIdx) // temporally set same POC for second field
            pFrame->setPicOrderCnt(m_PicOrderCnt, 1);
    }
    else
    {
        pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
        pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
    }
}
/****************************************************************************************************/
// SEI_Storer
/****************************************************************************************************/
SEI_Storer::SEI_Storer()
{
    Reset();
}

SEI_Storer::~SEI_Storer()
{
    Close();
}

void SEI_Storer::Init()
{
    Close();
    m_data.resize(MAX_BUFFERED_SIZE);
    m_payloads.resize(START_ELEMENTS);
    m_offset = 0;
    m_lastUsed = 2;
}

void SEI_Storer::Close()
{
    Reset();
    m_data.clear();
    m_payloads.clear();
}

void SEI_Storer::Reset()
{
    m_lastUsed = 2;
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        m_payloads[i].isUsed = 0;
    }
}

void SEI_Storer::SetFrame(H264DecoderFrame * frame, Ipp32s auIndex)
{
    VM_ASSERT(frame);
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == 0 && m_payloads[i].isUsed && m_payloads[i].auID == auIndex)
        {
            m_payloads[i].frame = frame;
        }
    }
}

void SEI_Storer::SetAUID(Ipp32s auIndex)
{
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].isUsed && m_payloads[i].auID == -1)
        {
            m_payloads[i].auID = auIndex;
        }
    }
}

void SEI_Storer::SetTimestamp(H264DecoderFrame * frame)
{
    VM_ASSERT(frame);
    Ipp64f ts = frame->m_dFrameTime;

    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].frame == frame)
        {
            m_payloads[i].timestamp = ts;
            if (m_payloads[i].isUsed)
                m_payloads[i].isUsed = m_lastUsed;
        }
    }

    m_lastUsed++;
}

const SEI_Storer::SEI_Message * SEI_Storer::GetPayloadMessage()
{
    SEI_Storer::SEI_Message * msg = 0;

    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (m_payloads[i].isUsed > 1)
        {
            if (!msg || msg->isUsed > m_payloads[i].isUsed)
            {
                msg = &m_payloads[i];
            }
        }
    }

    if (msg)
        msg->isUsed = 0;

    return msg;
}

SEI_Storer::SEI_Message* SEI_Storer::AddMessage(UMC::MediaDataEx *nalUnit, SEI_TYPE type, Ipp32s auIndex)
{
    size_t sz = nalUnit->GetDataSize();

    if (sz > (m_data.size() >> 2))
        return 0;

    if (m_offset + sz > m_data.size())
    {
        m_offset = 0;
    }

    // clear overwriting messages:
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (!m_payloads[i].isUsed)
            continue;

        SEI_Message & mmsg = m_payloads[i];

        if ((m_offset + sz > mmsg.offset) &&
            (m_offset < mmsg.offset + mmsg.msg_size))
        {
            m_payloads[i].isUsed = 0;
            return 0;
        }
    }

    size_t freeSlot = 0;
    for (Ipp32u i = 0; i < m_payloads.size(); i++)
    {
        if (!m_payloads[i].isUsed)
        {
            freeSlot = i;
            break;
        }
    }

    if (m_payloads[freeSlot].isUsed)
    {
        if (m_payloads.size() >= MAX_ELEMENTS)
            return 0;

        m_payloads.push_back(SEI_Message());
        freeSlot = m_payloads.size() - 1;
    }

    m_payloads[freeSlot].msg_size = sz;
    m_payloads[freeSlot].offset = m_offset;
    m_payloads[freeSlot].timestamp = 0;
    m_payloads[freeSlot].frame = 0;
    m_payloads[freeSlot].isUsed = 1;
    m_payloads[freeSlot].data = &(m_data.front()) + m_offset;
    m_payloads[freeSlot].type = type;
    m_payloads[freeSlot].auID = auIndex;

    MFX_INTERNAL_CPY(&m_data[m_offset], (Ipp8u*)nalUnit->GetDataPointer(), (Ipp32u)sz);

    m_offset += sz;

    return &m_payloads[freeSlot];
}

ViewItem::ViewItem()
{
    Reset();

} // ViewItem::ViewItem(void)

ViewItem::ViewItem(const ViewItem &src)
{
    Reset();

    viewId = src.viewId;
    for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++) {
        pDPB[i].reset(src.pDPB[i].release());
        pPOCDec[i].reset(src.pPOCDec[i].release());
        MaxLongTermFrameIdx[i] = src.MaxLongTermFrameIdx[i];
    }
    dpbSize = src.dpbSize;
    maxDecFrameBuffering = src.maxDecFrameBuffering;

} // ViewItem::ViewItem(const ViewItem &src)

ViewItem::~ViewItem()
{
    Close();

} // ViewItem::ViewItem(void)

Status ViewItem::Init(Ipp32u view_id)
{
    // release the object before initialization
    Close();

    try
    {
        for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++) {
            // allocate DPB and POC counter
            pDPB[i].reset(new H264DBPList());
            pPOCDec[i].reset(new POCDecoder());
        }
    }
    catch(...)
    {
        return UMC_ERR_ALLOC;
    }

    // save the ID
    viewId = view_id;
    localFrameTime = 0;
    pCurFrame = 0;
    return UMC_OK;

} // Status ViewItem::Init(Ipp32u view_id)

void ViewItem::Close(void)
{
    // Reset the parameters before close
    Reset();

    for (Ipp32s i = MAX_NUM_LAYERS - 1; i >= 0; i--) {
        if (pDPB[i].get())
        {
            pDPB[i].reset();
        }

        if (pPOCDec[i].get())
        {
            pPOCDec[i].reset();
        }

        MaxLongTermFrameIdx[i] = 0;
    }

    viewId = (Ipp32u)INVALID_VIEW_ID;
    dpbSize = 0;
    maxDecFrameBuffering = 1;
} // void ViewItem::Close(void)

void ViewItem::Reset(void)
{
    for (Ipp32s i = MAX_NUM_LAYERS - 1; i >= 0; i--)
    {
        if (pDPB[i].get())
        {
            pDPB[i]->Reset();
        }

        if (pPOCDec[i].get())
        {
            pPOCDec[i]->Reset();
        }
    }

    pCurFrame = 0;
    localFrameTime = 0;
    m_isDisplayable = true;

} // void ViewItem::Reset(void)

void ViewItem::SetDPBSize(H264SeqParamSet *pSps, Ipp8u & level_idc)
{
    Ipp8u level = level_idc ? level_idc : pSps->level_idc;

    // calculate the new DPB size value
    dpbSize = CalculateDPBSize(level,
                               pSps->frame_width_in_mbs * 16,
                               pSps->frame_height_in_mbs * 16,
                               pSps->num_ref_frames);

    if (level_idc)
    {
        level_idc = level;
    }
    else
    {
        pSps->level_idc = level;
    }

    // provide the new value to the DPBList
    for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++)
    {
        if (pDPB[i].get())
        {
            pDPB[i]->SetDPBSize(dpbSize);
        }
    }
} // void ViewItem::SetDPBSize(const H264SeqParamSet *pSps)

void InferredSliceParameterSVC(H264DecoderFrameInfo * au, H264Slice* pSlice)
{
    H264SliceHeader *curHdr = pSlice->GetSliceHeader();

    // G.7.4.3.4  because bitstream contains this information for quality_id == 0 only
    if (curHdr->nal_ext.svc.quality_id > 0)
    {
        Ipp8u curDId = curHdr->nal_ext.svc.dependency_id;

        for (Ipp32u i = 0; i < au->GetSliceCount(); i++)
        {
            H264Slice* slice = au->GetSlice(i);
            H264SliceHeader *hdr = slice->GetSliceHeader();
            Ipp8u baseDId = hdr->nal_ext.svc.dependency_id;
            Ipp8u baseQId = hdr->nal_ext.svc.quality_id;

            VM_ASSERT(!slice->IsSliceGroups());
            if (!(pSlice->GetStreamFirstMB() >= slice->GetStreamFirstMB() && pSlice->GetStreamFirstMB() < slice->GetMaxMB()))
                continue;

            if (!((baseDId == curDId) && (baseQId == 0)))
                continue;

            curHdr->direct_spatial_mv_pred_flag = hdr->direct_spatial_mv_pred_flag;
            curHdr->num_ref_idx_active_override_flag = hdr->num_ref_idx_active_override_flag;
            curHdr->num_ref_idx_l0_active = hdr->num_ref_idx_l0_active;
            curHdr->num_ref_idx_l1_active = hdr->num_ref_idx_l1_active;
            curHdr->base_pred_weight_table_flag = hdr->base_pred_weight_table_flag;
            curHdr->luma_log2_weight_denom = hdr->luma_log2_weight_denom;
            curHdr->chroma_log2_weight_denom = hdr->chroma_log2_weight_denom;
            curHdr->no_output_of_prior_pics_flag = hdr->no_output_of_prior_pics_flag;
            curHdr->long_term_reference_flag = hdr->long_term_reference_flag;
            curHdr->adaptive_ref_pic_marking_mode_flag = hdr->adaptive_ref_pic_marking_mode_flag;
            curHdr->nal_ext.svc.store_ref_base_pic_flag = hdr->nal_ext.svc.store_ref_base_pic_flag;
            curHdr->slice_group_change_cycle = hdr->slice_group_change_cycle;

            RefPicListReorderInfo *ToReorderInfoL0 = &(pSlice->ReorderInfoL0);
            RefPicListReorderInfo *FromReorderInfoL0 = &(slice->ReorderInfoL0);
            RefPicListReorderInfo *ToReorderInfoL1 = &(pSlice->ReorderInfoL1);
            RefPicListReorderInfo *FromReorderInfoL1 = &(slice->ReorderInfoL1);

            ToReorderInfoL0->num_entries = FromReorderInfoL0->num_entries;

            for (Ipp32s j = 0; j < (Ipp32s)ToReorderInfoL0->num_entries; j++)
            {
                ToReorderInfoL0->reordering_of_pic_nums_idc[j] = FromReorderInfoL0->reordering_of_pic_nums_idc[j];
                ToReorderInfoL0->reorder_value[j] = FromReorderInfoL0->reorder_value[j];
            }

            ToReorderInfoL1->num_entries = FromReorderInfoL1->num_entries;

            for (Ipp32s j = 0; j < (Ipp32s)ToReorderInfoL1->num_entries; j++)
            {
                ToReorderInfoL1->reordering_of_pic_nums_idc[j] = FromReorderInfoL1->reordering_of_pic_nums_idc[j];
                ToReorderInfoL1->reorder_value[j]  = FromReorderInfoL1->reorder_value[j];
            }

            Ipp32s end = hdr->num_ref_idx_l0_active;

            if ( (slice->GetPicParam()->weighted_pred_flag && (PREDSLICE == hdr->slice_type || S_PREDSLICE == hdr->slice_type)) ||
                (slice->GetPicParam()->weighted_bipred_idc == 1 && BPREDSLICE == hdr->slice_type))
            {
                //pSlice->GetP
                PredWeightTable *pFromPredWeight[2];
                pFromPredWeight[0] = (PredWeightTable*)slice->GetPredWeigthTable(0);
                pFromPredWeight[1] = (PredWeightTable*)slice->GetPredWeigthTable(1);

                PredWeightTable *pToPredWeight[2];
                pToPredWeight[0] = (PredWeightTable*)pSlice->GetPredWeigthTable(0);
                pToPredWeight[1] = (PredWeightTable*)pSlice->GetPredWeigthTable(1);

                for (Ipp32s jj = 0; jj < 2; jj++)
                {
                    for (Ipp32s j = 0; j < end; j++)
                    {
                        pToPredWeight[jj][j].luma_weight_flag = pFromPredWeight[jj][j].luma_weight_flag;
                        pToPredWeight[jj][j].luma_weight = pFromPredWeight[jj][j].luma_weight;
                        pToPredWeight[jj][j].luma_offset = pFromPredWeight[jj][j].luma_offset;

                        pToPredWeight[jj][j].chroma_weight_flag = pFromPredWeight[jj][j].chroma_weight_flag;
                        pToPredWeight[jj][j].chroma_weight[0] = pFromPredWeight[jj][j].chroma_weight[0];
                        pToPredWeight[jj][j].chroma_offset[0] = pFromPredWeight[jj][j].chroma_offset[0];
                        pToPredWeight[jj][j].chroma_weight[1] = pFromPredWeight[jj][j].chroma_weight[1];
                        pToPredWeight[jj][j].chroma_offset[1] = pFromPredWeight[jj][j].chroma_offset[1];
                    }
                    end = hdr->num_ref_idx_l1_active;
                }
            }

            AdaptiveMarkingInfo *ToAdaptiveMarkingInfo = &(pSlice->m_AdaptiveMarkingInfo);
            AdaptiveMarkingInfo *ToBaseAdaptiveMarkingInfo = &(pSlice->m_BaseAdaptiveMarkingInfo);
            AdaptiveMarkingInfo *FromAdaptiveMarkingInfo = &(slice->m_AdaptiveMarkingInfo);
            ToAdaptiveMarkingInfo->num_entries = FromAdaptiveMarkingInfo->num_entries;

            for (Ipp32s j = 0; j < (Ipp32s)ToAdaptiveMarkingInfo->num_entries; j++)
            {
                ToAdaptiveMarkingInfo->mmco[j] = FromAdaptiveMarkingInfo->mmco[j];
                ToAdaptiveMarkingInfo->value[2*j] = FromAdaptiveMarkingInfo->value[2*j];
                ToAdaptiveMarkingInfo->value[2*j+1] = FromAdaptiveMarkingInfo->value[2*j+1];
            }

            AdaptiveMarkingInfo *FromBaseAdaptiveMarkingInfo = &(slice->m_BaseAdaptiveMarkingInfo);
            ToBaseAdaptiveMarkingInfo->num_entries = FromBaseAdaptiveMarkingInfo->num_entries;

            for (Ipp32s j = 0; j < (Ipp32s)ToBaseAdaptiveMarkingInfo->num_entries; j++)
            {
                ToBaseAdaptiveMarkingInfo->mmco[j] = FromBaseAdaptiveMarkingInfo->mmco[j];
                ToBaseAdaptiveMarkingInfo->value[2*j] = FromBaseAdaptiveMarkingInfo->value[2*j];
                ToBaseAdaptiveMarkingInfo->value[2*j+1] = FromBaseAdaptiveMarkingInfo->value[2*j+1];
            }
        }
    }
    else
    {
        Ipp8u refDId = (Ipp8u)(curHdr->ref_layer_dq_id >> 4);//  nal_ext.svc.dependency_id;

        for (Ipp32u i = 0; i < au->GetSliceCount(); i++)
        {
            H264Slice* slice = au->GetSlice(i);
            H264SliceHeader *hdr = slice->GetSliceHeader();
            Ipp8u baseDId = hdr->nal_ext.svc.dependency_id;
            Ipp8u baseQId = hdr->nal_ext.svc.quality_id;
            Ipp32s j, jj, end;

            if (!((baseDId == refDId) && (baseQId == 0)))
                continue;

            if (curHdr->base_pred_weight_table_flag &&
                pSlice->GetPicParam()->weighted_pred_flag &&
                !curHdr->nal_ext.svc.no_inter_layer_pred_flag &&
                (curHdr->slice_type == PREDSLICE || curHdr->slice_type == BPREDSLICE))
            {

                curHdr->luma_log2_weight_denom = hdr->luma_log2_weight_denom;
                curHdr->chroma_log2_weight_denom = hdr->chroma_log2_weight_denom;

                end = hdr->num_ref_idx_l0_active;

                //pSlice->GetP
                PredWeightTable *pFromPredWeight[2];
                pFromPredWeight[0] = (PredWeightTable*)slice->GetPredWeigthTable(0);
                pFromPredWeight[1] = (PredWeightTable*)slice->GetPredWeigthTable(1);

                PredWeightTable *pToPredWeight[2];
                pToPredWeight[0] = (PredWeightTable*)pSlice->GetPredWeigthTable(0);
                pToPredWeight[1] = (PredWeightTable*)pSlice->GetPredWeigthTable(1);

                for (jj = 0; jj < 2; jj++)
                {
                    for (j = 0; j < end; j++)
                    {
                        pToPredWeight[jj][j].luma_weight_flag = pFromPredWeight[jj][j].luma_weight_flag;
                        pToPredWeight[jj][j].luma_weight = pFromPredWeight[jj][j].luma_weight;
                        pToPredWeight[jj][j].luma_offset = pFromPredWeight[jj][j].luma_offset;

                        pToPredWeight[jj][j].chroma_weight_flag = pFromPredWeight[jj][j].chroma_weight_flag;
                        pToPredWeight[jj][j].chroma_weight[0] = pFromPredWeight[jj][j].chroma_weight[0];
                        pToPredWeight[jj][j].chroma_offset[0] = pFromPredWeight[jj][j].chroma_offset[0];
                        pToPredWeight[jj][j].chroma_weight[1] = pFromPredWeight[jj][j].chroma_weight[1];
                        pToPredWeight[jj][j].chroma_offset[1] = pFromPredWeight[jj][j].chroma_offset[1];
                    }
                    end = hdr->num_ref_idx_l1_active;
                }
            }
            break;
        }
   }

}

/****************************************************************************************************/
// TaskSupplier
/****************************************************************************************************/
TaskSupplier::TaskSupplier()
    : AU_Splitter(&m_ObjHeap)

    , m_pSegmentDecoder(0)
    , m_iThreadNum(0)
    , m_use_external_framerate(false)

    , m_pLastSlice(0)
    , m_pLastDisplayed(0)
    , m_pMemoryAllocator(0)
    , m_pFrameAllocator(0)

    , m_DPBSizeEx(0)
    , m_TrickModeSpeed(1)
    , m_frameOrder(0)
    , m_pTaskBroker(0)

    , m_pPostProcessing(0)
    , m_DefaultNotifyChain(&m_ObjHeap)
    , m_UIDFrameCounter(0)
    , m_sei_messages(0)
    , m_isInitialized(false)
{
}

TaskSupplier::~TaskSupplier()
{
    Close();
}

Status TaskSupplier::Init(BaseCodecParams *pInit)
{
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    m_initializationParams = *init;

    if (ABSOWN(init->dPlaybackRate - 1) > 0.0001)
    {
        m_TrickModeSpeed = 2;
    }
    else
    {
        m_TrickModeSpeed = 1;
    }

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    DPBOutput::Reset();
    AU_Splitter::Init(init);
    Status umcRes = SVC_Extension::Init();
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    switch (m_initializationParams.info.profile) // after MVC_Extension::Init()
    {
    case 0:
        m_decodingMode = UNKNOWN_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:
        m_decodingMode = MVC_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
        m_decodingMode = SVC_DECODING_MODE;
        AllocateView(0);
        break;
    default:
        m_decodingMode = AVC_DECODING_MODE;
        break;
    }

    switch(m_iThreadNum)
    {
    case 1:
        m_pTaskBroker = new TaskBrokerSingleThread(this);
        break;
    case 4:
    case 3:
    case 2:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    default:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    };

    m_pTaskBroker->Init(m_iThreadNum, true);

    // create slice decoder(s)
    m_pSegmentDecoder = new H264SegmentDecoderMultiThreaded *[m_iThreadNum];
    if (NULL == m_pSegmentDecoder)
        return UMC_ERR_ALLOC;
    memset(m_pSegmentDecoder, 0, sizeof(H264SegmentDecoderMultiThreaded *) * m_iThreadNum);

    Ipp32u i;
    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264SegmentDecoderMultiThreaded(m_pTaskBroker);
        if (NULL == m_pSegmentDecoder[i])
            return UMC_ERR_ALLOC;
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC_ERR_INIT;

        if (!i)
            continue;

        H264Thread * thread = new H264Thread();
        if (thread->Init(i, m_pSegmentDecoder[i]) != UMC_OK)
        {
            delete thread;
            return UMC_ERR_INIT;
        }

        m_threadGroup.AddThread(thread);
    }

    m_pPostProcessing = init->pPostProcessing;

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    m_isInitialized = true;

    return UMC_OK;
}

Status TaskSupplier::PreInit(BaseCodecParams *pInit)
{
    if (m_isInitialized)
        return UMC_OK;

    VideoDecoderParams *init = DynamicCast<VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC_ERR_NULL_PTR;

    Close();

    m_DPBSizeEx = 0;

    SVC_Extension::Init();

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    AU_Splitter::Init(init);
    DPBOutput::Reset();

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    m_DPBSizeEx = m_iThreadNum;

    return UMC_OK;
}

Status TaskSupplier::SetParams(BaseCodecParams* params)
{
    VideoDecoderParams *pParams = DynamicCast<VideoDecoderParams>(params);

    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    if (pParams->lTrickModesFlag == 7)
    {
        if (ABSOWN(pParams->dPlaybackRate - 1) > 0.0001)
            m_TrickModeSpeed = 2;
        else
            m_TrickModeSpeed = 1;
    }

    return UMC_OK;
}

void TaskSupplier::Close()
{
    if (m_pTaskBroker)
    {
        m_pTaskBroker->Release();
    }

// from reset

    m_threadGroup.Release();

    {
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    for (; iter != iter_end; ++iter)
    {
        for (H264DecoderFrame *pFrame = iter->GetDPBList(0)->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }
    }

    if (m_pSegmentDecoder)
    {
        for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
        {
            delete m_pSegmentDecoder[i];
            m_pSegmentDecoder[i] = 0;
        }
    }

    SVC_Extension::Close();
    AU_Splitter::Close();
    DPBOutput::Reset();
    DecReferencePictureMarking::Reset();

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_pLastSlice->DecrementReference();
        m_pLastSlice = 0;
    }

    m_accessUnit.Release();
    m_Headers.Reset(false);
    Skipping::Reset();
    m_ObjHeap.Release();

    m_frameOrder               = 0;

    m_WaitForIDR        = true;

    m_pLastDisplayed = 0;

    delete m_sei_messages;
    m_sei_messages = 0;

// from reset

    delete[] m_pSegmentDecoder;
    m_pSegmentDecoder = 0;

    delete m_pTaskBroker;
    m_pTaskBroker = 0;

    m_iThreadNum = 0;

    m_DPBSizeEx = 1;

    m_isInitialized = false;

} // void TaskSupplier::Close()

void TaskSupplier::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    m_threadGroup.Reset();

    m_DefaultNotifyChain.Notify();
    m_DefaultNotifyChain.Reset();

    {
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    for (; iter != iter_end; ++iter)
    {
        for (H264DecoderFrame *pFrame = iter->GetDPBList(0)->head(); pFrame; pFrame = pFrame->future())
        {
            pFrame->FreeResources();
        }
    }
    }

    if (m_sei_messages)
        m_sei_messages->Reset();

    SVC_Extension::Reset();
    AU_Splitter::Reset();
    DPBOutput::Reset();
    DecReferencePictureMarking::Reset();
    m_accessUnit.Release();

    switch (m_initializationParams.info.profile) // after MVC_Extension::Init()
    {
    case 0:
        m_decodingMode = UNKNOWN_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:
        m_decodingMode = MVC_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
        m_decodingMode = SVC_DECODING_MODE;
        AllocateView(0);
        break;
    default:
        m_decodingMode = AVC_DECODING_MODE;
        break;
    }

    if (m_pLastSlice)
    {
        m_pLastSlice->Release();
        m_pLastSlice->DecrementReference();
        m_pLastSlice = 0;
    }

    m_Headers.Reset(true);
    Skipping::Reset();
    m_ObjHeap.Release();

    m_frameOrder               = 0;

    m_WaitForIDR        = true;

    m_pLastDisplayed = 0;

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, true);
}

void TaskSupplier::AfterErrorRestore()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    m_threadGroup.Reset();

    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    for (; iter != iter_end; ++iter)
    {
        for (Ipp32u i = 0; i < MAX_NUM_LAYERS; i++)
            iter->GetDPBList(i)->Reset();
    }

    SVC_Extension::Reset();
    AU_Splitter::Reset();

    Skipping::Reset();
    m_ObjHeap.Release();
    m_accessUnit.Release();
    m_Headers.Reset(true);

    m_pLastDisplayed = 0;

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, true);
}

Status TaskSupplier::GetInfoFromData(BaseCodecParams* params)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams *lpInfo = DynamicCast<VideoDecoderParams> (params);

    if (!m_isInitialized)
    {
        if (params->m_pData)
        {
            if (!m_pMemoryAllocator)
            {
                // DEBUG : allocate allocator or do it in PreInit !!!
            }
            //m_pTaskSupplier->SetMemoryAllocator(m_pMemoryAllocator);
            PreInit(params);

            if (lpInfo->m_pData && lpInfo->m_pData->GetDataSize())
            {
                umcRes = AddOneFrame(lpInfo->m_pData, 0);
            }

            umcRes = GetInfo(lpInfo);
            Close();
            return umcRes;
        }

        return UMC_ERR_NOT_INITIALIZED;
    }

    if (NULL == lpInfo)
        return UMC_ERR_NULL_PTR;

    return GetInfo(lpInfo);
}

Status TaskSupplier::GetInfo(VideoDecoderParams *lpInfo)
{
    lpInfo->pPostProcessing = m_pPostProcessing;

    H264SeqParamSet *sps = m_Headers.m_SeqParams.GetCurrentHeader();
    if (!sps)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    const H264PicParamSet *pps = m_Headers.m_PicParams.GetCurrentHeader();

    lpInfo->info.clip_info.height = sps->frame_height_in_mbs * 16 -
        SubHeightC[sps->chroma_format_idc]*(2 - sps->frame_mbs_only_flag) *
        (sps->frame_cropping_rect_top_offset + sps->frame_cropping_rect_bottom_offset);

    lpInfo->info.clip_info.width = sps->frame_width_in_mbs * 16 - SubWidthC[sps->chroma_format_idc] *
        (sps->frame_cropping_rect_left_offset + sps->frame_cropping_rect_right_offset);

    if (0.0 < m_local_delta_frame_time)
    {
        lpInfo->info.framerate = 1.0 / m_local_delta_frame_time;
    }
    else
    {
        lpInfo->info.framerate = 0;
    }

    lpInfo->info.stream_type     = H264_VIDEO;

    lpInfo->profile = sps->profile_idc;
    lpInfo->level = sps->level_idc;

    lpInfo->numThreads = m_iThreadNum;
    lpInfo->info.color_format = GetUMCColorFormat(sps->chroma_format_idc);

    lpInfo->info.profile = sps->profile_idc;
    lpInfo->info.level = sps->level_idc;

    if (sps->aspect_ratio_idc == 255)
    {
        lpInfo->info.aspect_ratio_width  = sps->sar_width;
        lpInfo->info.aspect_ratio_height = sps->sar_height;
    }

    Ipp32u multiplier = 1 << (6 + sps->bit_rate_scale);
    lpInfo->info.bitrate = sps->bit_rate_value[0] * multiplier;

    if (sps->frame_mbs_only_flag)
        lpInfo->info.interlace_type = PROGRESSIVE;
    else
    {
        if (0 <= sps->offset_for_top_to_bottom_field)
            lpInfo->info.interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
        else
            lpInfo->info.interlace_type = INTERLEAVED_BOTTOM_FIELD_FIRST;
    }

    H264VideoDecoderParams *lpH264Info = DynamicCast<H264VideoDecoderParams> (lpInfo);
    if (lpH264Info)
    {
        // calculate the new DPB size value
        lpH264Info->m_DPBSize = CalculateDPBSize(sps->level_idc,
                                   sps->frame_width_in_mbs * 16,
                                   sps->frame_height_in_mbs * 16,
                                   sps->num_ref_frames) + m_DPBSizeEx;

        IppiSize sz;
        sz.width    = sps->frame_width_in_mbs * 16;
        sz.height   = sps->frame_height_in_mbs * 16;
        lpH264Info->m_fullSize = sz;

        if (pps)
        {
           lpH264Info->m_entropy_coding_type = pps->entropy_coding_mode;
        }

        lpH264Info->m_cropArea.top = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_top_offset * (2 - sps->frame_mbs_only_flag));
        lpH264Info->m_cropArea.bottom = (Ipp16s)(SubHeightC[sps->chroma_format_idc] * sps->frame_cropping_rect_bottom_offset * (2 - sps->frame_mbs_only_flag));
        lpH264Info->m_cropArea.left = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_left_offset);
        lpH264Info->m_cropArea.right = (Ipp16s)(SubWidthC[sps->chroma_format_idc] * sps->frame_cropping_rect_right_offset);

        lpH264Info->m_auxiliary_format_idc = 0;

        const H264SeqParamSetExtension *sps_ex = m_Headers.m_SeqExParams.GetCurrentHeader();
        if (sps_ex)
        {
            lpH264Info->m_auxiliary_format_idc = sps_ex->aux_format_idc;
        }
    }

    return UMC_OK;
}

H264DecoderFrame *TaskSupplier::GetFreeFrame(const H264Slice *pSlice)
{
    AutomaticUMCMutex guard(m_mGuard);
    Ipp32u view_id = pSlice->GetSliceHeader()->nal_ext.mvc.view_id;
    H264DecoderFrame *pFrame = 0;

    ViewItem &view = GetView(view_id);
    H264DBPList *pDPB = view.GetDPBList(0);

    // Traverse list for next disposable frame
    if (pDPB->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        pFrame = pDPB->GetOldestDisposable();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        pDPB->append(pFrame);
    }

    DecReferencePictureMarking::Remove(pFrame);
    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);
    pFrame->IncrementReference();

    if (GetAuxiliaryFrame(pFrame))
    {
        GetAuxiliaryFrame(pFrame)->Reset();
    }

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;
    return pFrame;

} // H264DecoderFrame *TaskSupplier::GetFreeFrame(const H264Slice *pSlice)

Status TaskSupplier::DecodeSEI(MediaDataEx *nalUnit)
{
    H264Bitstream bitStream;

    try
    {
        H264MemoryPiece mem;
        mem.SetData(nalUnit);

        H264MemoryPiece swappedMem;
        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        memset(swappedMem.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem);

        bitStream.Reset((Ipp8u*)swappedMem.GetPointer(), (Ipp32u)swappedMem.GetDataSize());

        NAL_Unit_Type nal_unit_type;
        Ipp32u nal_ref_idc;

        bitStream.GetNALUnitType(nal_unit_type, nal_ref_idc);

        do
        {
            H264SEIPayLoad    m_SEIPayLoads;

            bitStream.ParseSEI(m_Headers, &m_SEIPayLoads);

            DEBUG_PRINT((VM_STRING("debug headers SEI - %d\n"), m_SEIPayLoads.payLoadType));

            if (m_SEIPayLoads.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
            {
                m_UserData = m_SEIPayLoads;
            }
            else
            {
                m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
            }

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC_OK;
}

#if 1
Status TaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    ViewItem *view = GetViewCount() ? &GetViewByNumber(BASE_VIEW) : 0;
    Status umcRes = UMC_OK;

    H264Bitstream bitStream;

    try
    {
        H264MemoryPiece mem;
        mem.SetData(nalUnit);

        H264MemoryPiece swappedMem;

        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        memset(swappedMem.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem);

        bitStream.Reset((Ipp8u*)swappedMem.GetPointer(), (Ipp32u)swappedMem.GetDataSize());

        NAL_Unit_Type nal_unit_type;
        Ipp32u nal_ref_idc;

        bitStream.GetNALUnitType(nal_unit_type, nal_ref_idc);

        switch(nal_unit_type)
        {
        // sequence parameter set
        case NAL_UT_SPS:
            {
                H264SeqParamSet sps;
                umcRes = bitStream.GetSequenceParamSet(&sps);
                if (umcRes != UMC_OK)
                    return UMC_ERR_INVALID_STREAM;

                Ipp8u newDPBsize = (Ipp8u)CalculateDPBSize(sps.level_idc,
                                            sps.frame_width_in_mbs * 16,
                                            sps.frame_height_in_mbs * 16,
                                            sps.num_ref_frames);

                bool isNeedClean = sps.max_dec_frame_buffering == 0;
                sps.max_dec_frame_buffering = sps.max_dec_frame_buffering ? sps.max_dec_frame_buffering : newDPBsize;

                const H264SeqParamSet * old_sps = m_Headers.m_SeqParams.GetCurrentHeader();
                bool newResolution = false;
                if (IsNeedSPSInvalidate(old_sps, &sps))
                {
                    newResolution = true;
                    //return UMC_NTF_NEW_RESOLUTION;
                }

                if (isNeedClean)
                    sps.max_dec_frame_buffering = 0;

                DEBUG_PRINT((VM_STRING("debug headers SPS - %d, num_ref_frames - %d \n"), sps.seq_parameter_set_id, sps.num_ref_frames));

                H264SeqParamSet * temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                m_Headers.m_SeqParams.AddHeader(&sps);

                // Validate the incoming bitstream's image dimensions.
                temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);

                if (view)
                {
                    view->SetDPBSize(&sps, m_level_idc);
                    view->maxDecFrameBuffering = temp->max_dec_frame_buffering ?
                                                temp->max_dec_frame_buffering :
                                                view->dpbSize;

                    temp->max_dec_frame_buffering = (Ipp8u)view->maxDecFrameBuffering;

                    if (m_TrickModeSpeed != 1)
                    {
                        view->maxDecFrameBuffering = 0;
                    }
                }

                m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

                if (!temp->timing_info_present_flag || m_use_external_framerate)
                {
                    temp->num_units_in_tick = 90000;
                    temp->time_scale = (Ipp32u)(2*90000 / m_local_delta_frame_time);
                }

                m_local_delta_frame_time = 1 / ((0.5 * temp->time_scale) / temp->num_units_in_tick);

                if (newResolution)
                    return UMC_NTF_NEW_RESOLUTION;
            }
            break;

        case NAL_UT_SPS_EX:
            {
                H264SeqParamSetExtension sps_ex;
                umcRes = bitStream.GetSequenceParamSetExtension(&sps_ex);

                if (umcRes != UMC_OK)
                    return UMC_ERR_INVALID_STREAM;

                m_Headers.m_SeqExParams.AddHeader(&sps_ex);
            }
            break;

            // picture parameter set
        case NAL_UT_PPS:
            {
                H264PicParamSet pps;
                // set illegal id
                pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

                // Get id
                umcRes = bitStream.GetPictureParamSetPart1(&pps);
                if (UMC_OK != umcRes)
                    return UMC_ERR_INVALID_STREAM;

                H264SeqParamSet *refSps = m_Headers.m_SeqParams.GetHeader(pps.seq_parameter_set_id);
                Ipp32u prevActivePPS = m_Headers.m_PicParams.GetCurrentID();

                if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                {
                    refSps = m_Headers.m_SeqParamsMvcExt.GetHeader(pps.seq_parameter_set_id);
                    if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                    {
                        refSps = m_Headers.m_SeqParamsSvcExt.GetHeader(pps.seq_parameter_set_id);
                        if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                            return UMC_ERR_INVALID_STREAM;
                    }
                }

                // Get rest of pic param set
                umcRes = bitStream.GetPictureParamSetPart2(&pps);
                if (UMC_OK != umcRes)
                    return UMC_ERR_INVALID_STREAM;

                DEBUG_PRINT((VM_STRING("debug headers PPS - %d - SPS - %d\n"), pps.pic_parameter_set_id, pps.seq_parameter_set_id));
                m_Headers.m_PicParams.AddHeader(&pps);

                //m_Headers.m_SeqParams.SetCurrentID(pps.seq_parameter_set_id);
                // in case of MVC restore previous active PPS
                if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == refSps->profile_idc) ||
                    (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == refSps->profile_idc))
                {
                    m_Headers.m_PicParams.SetCurrentID(prevActivePPS);
                }
            }
            break;

        // subset sequence parameter set
        case NAL_UT_SUBSET_SPS:
            {
                if (!IsExtension())
                    break;

                H264SeqParamSet sps;
                umcRes = bitStream.GetSequenceParamSet(&sps);
                if (UMC_OK != umcRes)
                {
                    return UMC_ERR_INVALID_STREAM;
                }

                m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

                DEBUG_PRINT((VM_STRING("debug headers SUBSET SPS - %d, profile_idc - %d, level_idc - %d, num_ref_frames - %d \n"), sps.seq_parameter_set_id, sps.profile_idc, sps.level_idc, sps.num_ref_frames));

                if ((sps.profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE) ||
                    (sps.profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH))
                {
                    H264SeqParamSetSVCExtension spsSvcExt;
                    H264SeqParamSet * sps_temp = &spsSvcExt;

                    *sps_temp = sps;

                    umcRes = bitStream.GetSequenceParamSetSvcExt(&spsSvcExt);
                    if (UMC_OK != umcRes)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }

                    const H264SeqParamSetSVCExtension * old_sps = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
                    bool newResolution = false;
                    if (old_sps && old_sps->profile_idc != spsSvcExt.profile_idc)
                    {
                        newResolution = true;
                    }

                    m_Headers.m_SeqParamsSvcExt.AddHeader(&spsSvcExt);

                    SVC_Extension::ChooseLevelIdc(&spsSvcExt, sps.level_idc, sps.level_idc);

                    if (view)
                    {
                        view->SetDPBSize(&sps, m_level_idc);
                        view->maxDecFrameBuffering = sps.max_dec_frame_buffering ?
                                                    sps.max_dec_frame_buffering :
                                                    view->dpbSize;
                    }

                    if (newResolution)
                        return UMC_NTF_NEW_RESOLUTION;
                }

                // decode additional parameters
                if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == sps.profile_idc) ||
                    (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == sps.profile_idc))
                {
                    H264SeqParamSetMVCExtension spsMvcExt;
                    H264SeqParamSet * sps_temp = &spsMvcExt;

                    *sps_temp = sps;

                    // skip one bit
                    bitStream.Get1Bit();
                    // decode  MVC extension
                    umcRes = bitStream.GetSequenceParamSetMvcExt(&spsMvcExt);
                    if (UMC_OK != umcRes)
                    {
                        return UMC_ERR_INVALID_STREAM;
                    }

                    const H264SeqParamSetMVCExtension * old_sps = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
                    bool newResolution = false;
                    if (old_sps && old_sps->profile_idc != spsMvcExt.profile_idc)
                    {
                        newResolution = true;
                    }

                    DEBUG_PRINT((VM_STRING("debug headers SUBSET SPS MVC ext - %d \n"), sps.seq_parameter_set_id));
                    m_Headers.m_SeqParamsMvcExt.AddHeader(&spsMvcExt);

                    MVC_Extension::ChooseLevelIdc(&spsMvcExt, sps.level_idc, sps.level_idc);

                    if (view)
                    {
                        view->SetDPBSize(&sps, m_level_idc);
                        view->maxDecFrameBuffering = sps.max_dec_frame_buffering ?
                                                    sps.max_dec_frame_buffering :
                                                    view->dpbSize;
                    }

                    if (newResolution)
                        return UMC_NTF_NEW_RESOLUTION;
                }
            }
            break;

        // decode a prefix nal unit
        case NAL_UT_PREFIX:
            {
                umcRes = bitStream.GetNalUnitPrefix(&m_Headers.m_nalExtension, nal_ref_idc);
                if (UMC_OK != umcRes)
                {
                    m_Headers.m_nalExtension.extension_present = 0;
                    return UMC_ERR_INVALID_STREAM;
                }
            }
            break;

        default:
            break;
        }
    }
    catch(const h264_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;

} // Status TaskSupplier::DecodeHeaders(MediaDataEx::_MediaDataEx *pSource, H264MemoryPiece * pMem)
#endif

//////////////////////////////////////////////////////////////////////////////
// ProcessFrameNumGap
//
// A non-sequential frame_num has been detected. If the sequence parameter
// set field gaps_in_frame_num_value_allowed_flag is non-zero then the gap
// is OK and "non-existing" frames will be created to correctly fill the
// gap. Otherwise the gap is an indication of lost frames and the need to
// handle in a reasonable way.
//////////////////////////////////////////////////////////////////////////////
Status TaskSupplier::ProcessFrameNumGap(H264Slice *pSlice, Ipp32s field, Ipp32s dId, Ipp32s maxDId)
{
    ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
    Status umcRes = UMC_OK;
    const H264SeqParamSet* sps = pSlice->GetSeqParam();
    H264SliceHeader *sliceHeader = pSlice->GetSliceHeader();

    Ipp32s index = GetDPBLayerIndex(maxDId, dId);
    Ipp32u frameNumGap = view.GetPOCDecoder(index)->DetectFrameNumGap(pSlice);

    if (!frameNumGap)
        return UMC_OK;

    if (dId == maxDId)
    {
        if (frameNumGap > view.dpbSize)
        {
            frameNumGap = view.dpbSize;
        }
    }
    else
    {
        if (frameNumGap > sps->num_ref_frames)
        {
            frameNumGap = sps->num_ref_frames;
        }
    }

    Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);

    DEBUG_PRINT((VM_STRING("frame gap - %d\n"), frameNumGap));

    // Fill the frame_num gap with non-existing frames. For each missing
    // frame:
    //  - allocate a frame
    //  - set frame num and pic num
    //  - update FrameNumWrap for all reference frames
    //  - use sliding window frame marking to free oldest reference
    //  - mark the frame as short-term reference
    // The picture part of the generated frames is unimportant -- it will
    // not be used for reference.

    // set to first missing frame. Note that if frame number wrapped during
    // the gap, the first missing frame_num could be larger than the
    // current frame_num. If that happened, FrameNumGap will be negative.
    //VM_ASSERT((Ipp32s)sliceHeader->frame_num > frameNumGap);
    Ipp32s frame_num = sliceHeader->frame_num - frameNumGap;

    while ((frame_num != sliceHeader->frame_num) && (umcRes == UMC_OK))
    {
        H264DecoderFrame *pFrame;

        if (dId == maxDId)
        {
            // allocate a frame
            // Traverse list for next disposable frame
            pFrame = GetFreeFrame(pSlice);

            // Did we find one?
            if (!pFrame)
            {
                return UMC_ERR_NOT_ENOUGH_BUFFER;
            }

            Status res = InitFreeFrame(pFrame, pSlice);

            if (res != UMC_OK)
            {
                return UMC_ERR_NOT_ENOUGH_BUFFER;
            }
        }
        else
        {
            pFrame = AddLayerFrameSVC(view.GetDPBList(index), pSlice);
            if (!pFrame)
            {
                return UMC_ERR_NOT_ENOUGH_BUFFER;
            }
        }

        pFrame->m_maxDId = maxDId;

        frameNumGap--;

        if (sps->pic_order_cnt_type != 0)
        {
            Ipp32s tmp1 = sliceHeader->delta_pic_order_cnt[0];
            Ipp32s tmp2 = sliceHeader->delta_pic_order_cnt[1];
            sliceHeader->delta_pic_order_cnt[0] = sliceHeader->delta_pic_order_cnt[1] = 0;

            view.GetPOCDecoder(index)->DecodePictureOrderCount(pSlice, frame_num);

            sliceHeader->delta_pic_order_cnt[0] = tmp1;
            sliceHeader->delta_pic_order_cnt[1] = tmp2;
        }

        // Set frame num and pic num for the missing frame
        pFrame->setFrameNum(frame_num);
        view.GetPOCDecoder(index)->DecodePictureOrderCountFrameGap(pFrame, sliceHeader, frame_num);
        DEBUG_PRINT((VM_STRING("frame gap - frame_num = %d, poc = %d,%d added\n"), frame_num, pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1]));

        if (sliceHeader->field_pic_flag == 0)
        {
            pFrame->setPicNum(frame_num, 0);
        }
        else
        {
            pFrame->setPicNum(frame_num*2+1, 0);
            pFrame->setPicNum(frame_num*2+1, 1);
        }

        // Update frameNumWrap and picNum for all decoded frames

        H264DecoderFrame *pFrm;
        H264DecoderFrame * pHead = view.GetDPBList(index)->head();
        for (pFrm = pHead; pFrm; pFrm = pFrm->future())
        {
            // TBD: modify for fields
            pFrm->UpdateFrameNumWrap(frame_num,
                uMaxFrameNum,
                pFrame->m_PictureStructureForRef+
                pFrame->m_bottom_field_flag[field]);
        }

        // sliding window ref pic marking
        ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
        H264DecoderFrame * saveFrame = pFrame;
        std::swap(pSlice->m_pCurrentFrame, saveFrame);
        DecReferencePictureMarking::SlideWindow(view, pSlice, 0);
        std::swap(pSlice->m_pCurrentFrame, saveFrame);

        pFrame->SetisShortTermRef(true, 0);
        pFrame->SetisShortTermRef(true, 1);

        // next missing frame
        frame_num++;
        if (frame_num >= uMaxFrameNum)
            frame_num = 0;

        // Set current as displayable and was outputted.
        pFrame->SetisDisplayable();
        pFrame->SetSkipped(true);
        pFrame->SetFrameExistFlag(false);
        pFrame->DecrementReference();
        //pFrame->OnDecodingCompleted();

        if (dId != maxDId)
        {
            pFrame->setWasOutputted();
            pFrame->setWasDisplayed();
        }
    }   // while

//    return view.pDPB->IsDisposableExist() ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;
//    return view.GetDPBList(maxDId-dId)->IsDisposableExist() ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;
    return UMC_OK;
}   // ProcessFrameNumGap

bool TaskSupplier::GetFrameToDisplay(MediaData *dst, bool force)
{
    // Perform output color conversion and video effects, if we didn't
    // already write our output to the application's buffer.
    VideoData *pVData = DynamicCast<VideoData> (dst);
    if (!pVData)
        return false;

    m_pLastDisplayed = 0;

    H264DecoderFrame * pFrame = 0;

    do
    {
        pFrame = GetFrameToDisplayInternal(force);
        if (!pFrame || !pFrame->IsDecoded())
        {
            return false;
        }

        PostProcessDisplayFrame(dst, pFrame);

        if (pFrame->IsSkipped())
        {
            SetFrameDisplayed(pFrame->PicOrderCnt(0, 3));
        }
    } while (pFrame->IsSkipped());

    m_pLastDisplayed = pFrame;
    pVData->SetInvalid(pFrame->GetError());

    VideoData data;

    InitColorConverter(pFrame, &data, 0);

    if (m_pPostProcessing->GetFrame(&data, pVData) != UMC_OK)
    {
        pFrame->setWasOutputted();
        pFrame->setWasDisplayed();
        return false;
    }

    pVData->SetDataSize(pVData->GetMappingSize());

    pFrame->setWasOutputted();
    pFrame->setWasDisplayed();
    return true;
}

bool TaskSupplier::IsWantToShowFrame(bool force)
{
    for (Ipp32s i = 0; i < GetViewCount(); i++)
    {
        ViewItem &view = GetViewByNumber(i);

        if ((view.GetDPBList(0)->countNumDisplayable() > view.maxDecFrameBuffering) ||
            force)
        {
            H264DecoderFrame * pTmp;

            pTmp = view.GetDPBList(0)->findOldestDisplayable(view.dpbSize);
            return !!pTmp;
        }
    }

    return false;
}

void TaskSupplier::PostProcessDisplayFrame(MediaData *dst, H264DecoderFrame *pFrame)
{
    if (!pFrame || pFrame->post_procces_complete)
        return;

    ViewItem &view = GetView(pFrame->m_viewId);

    pFrame->m_isOriginalPTS = pFrame->m_dFrameTime > -1.0;
    if (pFrame->m_isOriginalPTS)
    {
        view.localFrameTime = pFrame->m_dFrameTime;
    }
    else
    {
        pFrame->m_dFrameTime = view.localFrameTime;
    }

    pFrame->m_frameOrder = m_frameOrder;
    switch (pFrame->m_displayPictureStruct)
    {
    case UMC::DPS_TOP_BOTTOM_TOP:
    case UMC::DPS_BOTTOM_TOP_BOTTOM:
        if (m_initializationParams.lFlags & UMC::FLAG_VDEC_TELECINE_PTS)
        {
            view.localFrameTime += (m_local_delta_frame_time / 2);
        }
        break;

    case UMC::DPS_FRAME_DOUBLING:
    case UMC::DPS_FRAME_TRIPLING:
    case UMC::DPS_TOP:
    case UMC::DPS_BOTTOM:
    case UMC::DPS_TOP_BOTTOM:
    case UMC::DPS_BOTTOM_TOP:
    case UMC::DPS_FRAME:
    default:
        break;
    }

    view.localFrameTime += m_local_delta_frame_time;

    bool wasWrapped = IncreaseCurrentView();
    if (wasWrapped)
        m_frameOrder++;

    if (view.GetDPBList(0)->GetRecoveryFrameCnt() != -1)
        view.GetDPBList(0)->SetRecoveryFrameCnt(-1);

    pFrame->post_procces_complete = true;

    DEBUG_PRINT((VM_STRING("Outputted POC - %d, busyState - %d, uid - %d, viewId - %d, pppp - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->GetRefCounter(), pFrame->m_UID, pFrame->m_viewId, pppp++));

    /*if (!CutPlanes(pFrame, m_Headers.GetSeqParamSet(m_CurrentSeqParamSet)))
    {
        m_pDecodedFramesList->DebugPrint();
    }*/
    dst->SetTime(pFrame->m_dFrameTime);
}

H264DecoderFrame *TaskSupplier::GetFrameToDisplayInternal(bool force)
{
    H264DecoderFrame *pFrame = GetAnyFrameToDisplay(force);
    return pFrame;
}

H264DecoderFrame *TaskSupplier::GetAnyFrameToDisplay(bool force)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    for (Ipp32u i = 0; iter != iter_end; ++i, ++iter)
    {
        ViewItem &view = *iter;

        if (i != m_currentDisplayView || !view.m_isDisplayable)
        {
            if (i == m_currentDisplayView)
            {
                IncreaseCurrentView();
            }
            continue;
        }

        for (;;)
        {
            // show oldest frame
            if (view.GetDPBList(0)->countNumDisplayable() > view.maxDecFrameBuffering || force)
            {
                H264DecoderFrame *pTmp;

                if (view.maxDecFrameBuffering)
                {
                    pTmp = view.GetDPBList(0)->findOldestDisplayable(view.dpbSize);
                }
                else
                {
                    pTmp = view.GetDPBList(0)->findFirstDisplayable();
                }

                if (pTmp)
                {
                    Ipp32s recovery_frame_cnt = view.GetDPBList(0)->GetRecoveryFrameCnt(); // DEBUG !!! need to think about recovery and MVC

                    if (!pTmp->IsFrameExist() || (recovery_frame_cnt != -1 && pTmp->m_FrameNum != recovery_frame_cnt))
                    {
                        pTmp->SetErrorFlagged(ERROR_FRAME_RECOVERY);
                    }

                    return pTmp;
                }

                break;
            }
            else
            {
                if (DPBOutput::IsUseDelayOutputValue())
                {
                    H264DecoderFrame *pTmp = view.GetDPBList(0)->findDisplayableByDPBDelay();
                    if (pTmp)
                        return pTmp;
                }
                break;
            }
        }
    }

    return 0;
}

void TaskSupplier::SetFrameDisplayed(Ipp32s poc)
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    H264DecoderFrame *(toDisplay[2]) = {0, 0};
    int idx = 0;

    for (; iter != iter_end; iter++)
    {
        ViewItem &view = *iter;
        H264DecoderFrame *pTmp;

        if (view.maxDecFrameBuffering)
        {
            pTmp = view.GetDPBList(0)->findOldestDisplayable(view.dpbSize);
        }
        else
        {
            pTmp = view.GetDPBList(0)->findFirstDisplayable();
        }

        if ((pTmp) &&
            (pTmp->PicOrderCnt(0, 3) == poc))
        {
            if (2 > idx)
            {
                toDisplay[idx++] = pTmp;
            }

            pTmp->setWasOutputted();
            pTmp->setWasDisplayed();
        }
    }


} // void TaskSupplier::SetFrameDisplayed(Ipp32s poc)

void TaskSupplier::SetMBMap(const H264Slice * slice, H264DecoderFrame *frame, LocalResources * localRes)
{
    Ipp32u mbnum, i;
    Ipp32s prevMB;
    Ipp32u uNumMBCols;
    Ipp32u uNumMBRows;
    Ipp32u uNumSliceGroups;
    Ipp32u uNumMapUnits;
   
    if (!slice)
        slice = frame->GetAU(0)->GetSlice(0);

    const H264PicParamSet *pps = slice->GetPicParam();
    const H264SliceHeader * sliceHeader = slice->GetSliceHeader();
    Ipp32s PrevMapUnit[MAX_NUM_SLICE_GROUPS];
    Ipp32s SliceGroup, FirstMB;
    const Ipp8u *pMap = 0;
    bool bSetFromMap = false;

    uNumMBCols = slice->GetSeqParam()->frame_width_in_mbs;
    uNumMBRows = slice->GetSeqParam()->frame_height_in_mbs;

    FirstMB = 0;
    // TBD: update for fields:
    uNumMapUnits = uNumMBCols*uNumMBRows;
    uNumSliceGroups = pps->num_slice_groups;

    VM_ASSERT(frame->m_iResourceNumber >= 0);

    Ipp32s resource = frame->m_iResourceNumber;

    Ipp32s additionalTable = resource + 1;

    if (uNumSliceGroups == 1)
    {
        localRes->GetMBInfo(resource).active_next_mb_table = localRes->GetDefaultMBMapTable();
    }
    else
    {
        Ipp32s times = frame->m_PictureStructureForDec < FRM_STRUCTURE ? 2 : 1;
        for (Ipp32s j = 0; j < times; j++)
        {
            if (frame->m_PictureStructureForDec < FRM_STRUCTURE)
            {
                if (j)
                {
                    FirstMB = frame->totalMBs;
                    uNumMapUnits <<= 1;
                }
                else
                {
                    uNumMapUnits >>= 1;
                    uNumMBRows >>= 1;
                }
            }

            //since main profile doesn't allow slice groups to be >1 and in baseline no fields (or mbaffs) allowed
            //the following memset is ok.
            // > 1 slice group
            switch (pps->SliceGroupInfo.slice_group_map_type)
            {
            case 0:
                {
                    // interleaved slice groups: run_length for each slice group,
                    // repeated until all MB's are assigned to a slice group
                    Ipp32u NumThisGroup;

                    // Init PrevMapUnit to -1 (none), for first unit of each slice group
                    for (i=0; i<uNumSliceGroups; i++)
                        PrevMapUnit[i] = -1;

                    SliceGroup = 0;
                    NumThisGroup = 0;
                    prevMB = -1;
                    for (mbnum = FirstMB; mbnum < uNumMapUnits; mbnum++)
                    {
                        if (NumThisGroup == pps->SliceGroupInfo.run_length[SliceGroup])
                        {
                            // new slice group
                            PrevMapUnit[SliceGroup] = prevMB;
                            SliceGroup++;
                            if (SliceGroup == (Ipp32s)uNumSliceGroups)
                                SliceGroup = 0;
                            prevMB = PrevMapUnit[SliceGroup];
                            NumThisGroup = 0;
                        }
                        if (prevMB >= 0)
                        {
                            // new
                            localRes->next_mb_tables[additionalTable][prevMB] = mbnum;
                        }
                        prevMB = mbnum;
                        NumThisGroup++;
                    }
                }
                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];
                break;

            case 1:
                // dispersed
                {
                    Ipp32u row, col;

                    // Init PrevMapUnit to -1 (none), for first unit of each slice group
                    for (i=0; i<uNumSliceGroups; i++)
                        PrevMapUnit[i] = -1;

                    mbnum = FirstMB;
                    for (row = 0; row < uNumMBRows; row++)
                    {
                        SliceGroup = ((row * uNumSliceGroups)/2) % uNumSliceGroups;
                        for (col=0; col<uNumMBCols; col++)
                        {
                            prevMB = PrevMapUnit[SliceGroup];
                            if (prevMB != -1)
                            {
                                localRes->next_mb_tables[additionalTable][prevMB]  = mbnum;
                            }
                            PrevMapUnit[SliceGroup] = mbnum;
                            mbnum++;
                            SliceGroup++;
                            if (SliceGroup == (Ipp32s)uNumSliceGroups)
                                SliceGroup = 0;
                        }    // col
                    }    // row
                }

                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];
                break;

            case 2:
                {
                    // foreground + leftover: Slice groups are rectangles, any MB not
                    // in a defined rectangle is in the leftover slice group, a MB within
                    // more than one rectangle is in the lower-numbered slice group.

                    // Two steps:
                    // 1. Set m_pMBMap with slice group for all MBs.
                    // 2. Set nextMB fields of MBInfo from m_pMBMap.

                    Ipp32u RectUpper, RectLeft, RectRight, RectLower;
                    Ipp32u RectRows, RectCols;
                    Ipp32u row, col;

                    // First init all as leftover
                    for (mbnum = FirstMB; mbnum<uNumMapUnits; mbnum++)
                        localRes->m_pMBMap[mbnum] = (Ipp8u)(uNumSliceGroups - 1);

                    // Next set those in slice group rectangles, from back to front
                    for (SliceGroup = (Ipp32s)(uNumSliceGroups - 2); SliceGroup >= 0; SliceGroup--)
                    {
                        mbnum = pps->SliceGroupInfo.t1.top_left[SliceGroup];
                        RectUpper = pps->SliceGroupInfo.t1.top_left[SliceGroup] / uNumMBCols;
                        RectLeft = pps->SliceGroupInfo.t1.top_left[SliceGroup] % uNumMBCols;
                        RectLower = pps->SliceGroupInfo.t1.bottom_right[SliceGroup] / uNumMBCols;
                        RectRight = pps->SliceGroupInfo.t1.bottom_right[SliceGroup] % uNumMBCols;
                        RectRows = RectLower - RectUpper + 1;
                        RectCols = RectRight - RectLeft + 1;

                        for (row = 0; row < RectRows; row++)
                        {
                            for (col=0; col < RectCols; col++)
                            {
                                localRes->m_pMBMap[mbnum + col] = (Ipp8u)SliceGroup;
                            }    // col

                            mbnum += uNumMBCols;
                        }    // row
                    }    // SliceGroup
                }
                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];

                pMap = localRes->m_pMBMap;
                bSetFromMap = true;        // to cause step 2 to occur below
                break;
            case 3:
                {
                    // Box-out, clockwise or counterclockwise. Result is two slice groups,
                    // group 0 included by the box, group 1 excluded.

                    // Two steps:
                    // 1. Set m_pMBMap with slice group for all MBs.
                    // 2. Set nextMB fields of MBInfo from m_pMBMap.

                    Ipp32u x, y, leftBound, topBound, rightBound, bottomBound;
                    Ipp32s xDir, yDir;
                    Ipp32u mba;
                    Ipp32u dir_flag = pps->SliceGroupInfo.t2.slice_group_change_direction_flag;
                    Ipp32u uNumInGroup0;
                    Ipp32u uGroup0Count = 0;

                    SliceGroup = 1;        // excluded group

                    uNumInGroup0 = IPP_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate *
                                    sliceHeader->slice_group_change_cycle, uNumMapUnits - FirstMB);

                    uNumInGroup0 = IPP_MIN(uNumInGroup0, uNumMapUnits);

                    if (uNumInGroup0 == uNumMapUnits)
                    {
                        // all units in group 0
                        SliceGroup = 0;
                        uGroup0Count = uNumInGroup0;    // to skip box out
                    }

                    // First init all
                    for (mbnum = FirstMB; mbnum < uNumMapUnits; mbnum++)
                        localRes->m_pMBMap[mbnum] = (Ipp8u)SliceGroup;

                    // Next the box-out algorithm to change included MBs to group 0

                    // start at center
                    x = (uNumMBCols - dir_flag)>>1;
                    y = (uNumMBRows - dir_flag)>>1;
                    leftBound = rightBound = x;
                    topBound = bottomBound = y;
                    xDir = dir_flag - 1;
                    yDir = dir_flag;

                    // expand out from center until group 0 includes the required number
                    // of units
                    while (uGroup0Count < uNumInGroup0)
                    {
                        mba = x + y*uNumMBCols;
                        if (localRes->m_pMBMap[mba + FirstMB] == 1)
                        {
                            // add MB to group 0
                            localRes->m_pMBMap[mba + FirstMB] = 0;
                            uGroup0Count++;
                        }
                        if (x == leftBound && xDir == -1)
                        {
                            if (leftBound > 0)
                            {
                                leftBound--;
                                x--;
                            }
                            xDir = 0;
                            yDir = dir_flag*2 - 1;
                        }
                        else if (x == rightBound && xDir == 1)
                        {
                            if (rightBound < uNumMBCols - 1)
                            {
                                rightBound++;
                                x++;
                            }
                            xDir = 0;
                            yDir = 1 - dir_flag*2;
                        }
                        else if (y == topBound && yDir == -1)
                        {
                            if (topBound > 0)
                            {
                                topBound--;
                                y--;
                            }
                            xDir = 1 - dir_flag*2;
                            yDir = 0;
                        }
                        else if (y == bottomBound && yDir == 1)
                        {
                            if (bottomBound < uNumMBRows - 1)
                            {
                                bottomBound++;
                                y++;
                            }
                            xDir = dir_flag*2 - 1;
                            yDir = 0;
                        }
                        else
                        {
                            x += xDir;
                            y += yDir;
                        }
                    }    // while
                }

                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];

                pMap = localRes->m_pMBMap;
                bSetFromMap = true;        // to cause step 2 to occur below
                break;
            case 4:
                // raster-scan: 2 slice groups. Both groups contain units ordered
                // by raster-scan, so initializing nextMB for simple raster-scan
                // ordering is all that is required.
                localRes->GetMBInfo(resource).active_next_mb_table = localRes->GetDefaultMBMapTable();
                break;
            case 5:
                // wipe: 2 slice groups, the vertical version of case 4. Init
                // nextMB by processing the 2 groups as two rectangles (left
                // and right); to allow for the break between groups occurring
                // not at a column boundary, the rectangles also have an upper
                // and lower half (same heights both rectangles) that may vary
                // in width from one another by one macroblock, for example:
                //  L L L L R R R R R
                //  L L L L R R R R R
                //  L L L R R R R R R
                //  L L L R R R R R R
                {
                    Ipp32u uNumInGroup0;
                    Ipp32u uNumInLGroup;
                    Ipp32s SGWidth;
                    Ipp32s NumUpperRows;
                    Ipp32s NumRows;
                    Ipp32s row, col;
                    Ipp32s iMBNum;

                    uNumInGroup0 = IPP_MIN(pps->SliceGroupInfo.t2.slice_group_change_rate *
                                    sliceHeader->slice_group_change_cycle, uNumMapUnits - FirstMB);
                    if (uNumInGroup0 >= uNumMapUnits)
                    {
                        // all units in group 0
                        uNumInGroup0 = uNumMapUnits;
                    }
                    if (pps->SliceGroupInfo.t2.slice_group_change_direction_flag == 0)
                        uNumInLGroup = uNumInGroup0;
                    else
                        uNumInLGroup = uNumMapUnits - uNumInGroup0;

                    if (uNumInLGroup > 0)
                    {
                        // left group
                        NumUpperRows = uNumInLGroup % uNumMBRows;
                        NumRows = uNumMBRows;
                        SGWidth = uNumInLGroup / uNumMBRows;        // lower width, left
                        if (NumUpperRows)
                        {
                            SGWidth++;            // upper width, left

                            // zero-width lower case
                            if (SGWidth == 1)
                                NumRows = NumUpperRows;
                        }
                        iMBNum = FirstMB;

                        for (row = 0; row < NumRows; row++)
                        {
                            col = 0;
                            while (col < SGWidth-1)
                            {
                                localRes->next_mb_tables[additionalTable][iMBNum + col] = (iMBNum + col + 1);
                                col++;
                            }    // col

                            // next for last MB on row
                            localRes->next_mb_tables[additionalTable][iMBNum + col] = (iMBNum + uNumMBCols);
                            iMBNum += uNumMBCols;

                            // time to switch to lower?
                            NumUpperRows--;
                            if (NumUpperRows == 0)
                                SGWidth--;
                        }    // row
                    }    // left group

                    if (uNumInLGroup < uNumMapUnits)
                    {
                        // right group
                        NumUpperRows = uNumInLGroup % uNumMBRows;
                        NumRows = uNumMBRows;
                        // lower width, right:
                        SGWidth = uNumMBCols - uNumInLGroup / uNumMBRows;
                        if (NumUpperRows)
                            SGWidth--;            // upper width, right
                        if (SGWidth > 0)
                        {
                            // first MB is on first row
                            iMBNum = uNumMBCols - SGWidth;
                        }
                        else
                        {
                            // zero-width upper case
                            SGWidth = 1;
                            iMBNum = (NumUpperRows + 1)*uNumMBCols - 1;
                            NumRows = uNumMBRows - NumUpperRows;
                            NumUpperRows = 0;
                        }

                        for (row = 0; row < NumRows; row++)
                        {
                            col = 0;
                            while (col < SGWidth-1)
                            {
                                localRes->next_mb_tables[additionalTable][iMBNum + col] = (iMBNum + col + 1);
                                col++;
                            }    // col

                            // next for last MB on row
                            localRes->next_mb_tables[additionalTable][iMBNum + col] = (iMBNum + uNumMBCols);

                            // time to switch to lower?
                            NumUpperRows--;
                            if (NumUpperRows == 0)
                            {
                                SGWidth++;
                                // fix next for last MB on row
                                localRes->next_mb_tables[additionalTable][iMBNum + col]= (iMBNum+uNumMBCols - 1);
                                iMBNum--;
                            }

                            iMBNum += uNumMBCols;

                        }    // row
                    }    // right group
                }

                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];
                break;
            case 6:
                // explicit map read from bitstream, contains slice group id for
                // each map unit
                localRes->GetMBInfo(resource).active_next_mb_table = localRes->next_mb_tables[additionalTable];
                pMap = &pps->SliceGroupInfo.pSliceGroupIDMap[0];
                bSetFromMap = true;
                break;
            default:
                // can't happen
                VM_ASSERT(0);

            }    // switch map type

            if (bSetFromMap)
            {
                // Set the nextMB MBInfo field of a set of macroblocks depending upon
                // the slice group information in the map, to create an ordered
                // (raster-scan) linked list of MBs for each slice group. The first MB
                // of each group will be identified by the first slice header for each
                // group.

                // For each map unit get assigned slice group from the map
                // For all except the first unit in each
                // slice group, set the next field of the previous MB in that
                // slice group.

                // Init PrevMapUnit to -1 (none), for first unit of each slice group
                for (i=0; i<uNumSliceGroups; i++)
                    PrevMapUnit[i] = -1;

                for (mbnum=FirstMB; mbnum<uNumMapUnits; mbnum++)
                {
                    SliceGroup = pMap[mbnum];
                    prevMB = PrevMapUnit[SliceGroup];
                    if (prevMB != -1)
                    {
                        localRes->next_mb_tables[additionalTable][prevMB] = mbnum;
                    }
                    PrevMapUnit[SliceGroup] = mbnum;
                }
            }
        }    // >1 slice group
    }
}    // setMBMap

void TaskSupplier::PreventDPBFullness()
{
    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    for (; iter != iter_end; ++iter)
    {
        ViewItem &view = *iter;
        H264DBPList *pDPB = view.GetDPBList(0);

    try
    {
        for (;;)
        {
            // force Display or ... delete long term
            const H264SeqParamSet* sps = m_Headers.m_SeqParams.GetCurrentHeader();
            if (sps)
            {
                Ipp32u NumShortTermRefs, NumLongTermRefs;

                // find out how many active reference frames currently in decoded
                // frames buffer
                pDPB->countActiveRefs(NumShortTermRefs, NumLongTermRefs);

                if (NumLongTermRefs == sps->num_ref_frames)
                {
                    H264DecoderFrame *pFrame = pDPB->findOldestLongTermRef();
                    if (pFrame)
                    {
                        pFrame->SetisLongTermRef(false, 0);
                        pFrame->SetisLongTermRef(false, 1);
                        pFrame->Reset();
                    }
                }

                if (pDPB->IsDisposableExist())
                    break;

                /*while (NumShortTermRefs > 0 &&
                    (NumShortTermRefs + NumLongTermRefs >= (Ipp32s)sps->num_ref_frames))
                {
                    H264DecoderFrame * pFrame = view.pDPB->findOldestShortTermRef();

                    if (!pFrame)
                        break;

                    pFrame->SetisShortTermRef(false, 0);
                    pFrame->SetisShortTermRef(false, 1);

                    NumShortTermRefs--;
                };*/

                if (pDPB->IsDisposableExist())
                    break;
            }

            /*H264DecoderFrame *pCurr = pDPB->head();
            while (pCurr)
            {
                if (pCurr->IsDecoded())
                {
                    pCurr->SetBusyState(0);
                }

                pCurr = pCurr->future();
            }*/

            break;
        }
    } catch(...)
    {
    }

    if (!pDPB->IsDisposableExist())
        AfterErrorRestore();

    }
}

Status TaskSupplier::CompleteDecodedFrames(H264DecoderFrame ** decoded)
{
    bool existCompleted = false;

    if (decoded)
    {
        *decoded = 0;
    }

    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();
    for (; iter != iter_end; ++iter)
    {
        ViewItem &view = *iter;
        H264DBPList *pDPB = view.GetDPBList(0);

        for (;;) //add all ready to decoding
        {
            bool isOneToAdd = true;
            H264DecoderFrame * frameToAdd = 0;

            for (H264DecoderFrame * frame = pDPB->head(); frame; frame = frame->future())
            {
                if (frame->IsFrameExist() && !frame->IsDecoded())
                {
                    if (!frame->IsDecodingStarted() && frame->IsFullFrame())
                    {
                        if (frameToAdd)
                        {
                            isOneToAdd = false;
                            if (frameToAdd->m_UID < frame->m_UID) // add first with min UID
                                continue;
                        }

                        frameToAdd = frame;
                    }

                    if (!frame->IsDecodingCompleted())
                    {
                        continue;
                    }

                    DEBUG_PRINT((VM_STRING("Decode POC - %d, uid - %d, viewId - %d \n"), frame->m_PicOrderCnt[0], frame->m_UID, frame->m_viewId));
                    frame->OnDecodingCompleted();
                    existCompleted = true;
                }
            }

            if (frameToAdd)
            {
                // frameToAdd->m_pLayerFrames[m_maxDId] == frameToAdd
                for (Ipp32s i = frameToAdd->m_minDId; i < frameToAdd->m_maxDId; i++)
                {
                    m_pTaskBroker->AddFrameToDecoding(frameToAdd->m_pLayerFrames[i]);
                }

                if (m_pTaskBroker->AddFrameToDecoding(frameToAdd))
                {
                    if (decoded)
                    {
                        *decoded = frameToAdd;
                    }
                }
                else
                    break;
            }

            if (isOneToAdd)
                break;
        }

    }

    return existCompleted ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;
}

Status TaskSupplier::AddSource(MediaData * pSource, MediaData *dst)
{
    Status umcRes = UMC_OK;

    CompleteDecodedFrames(0);

    umcRes = AddOneFrame(pSource, dst); // construct frame

    if (UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
    {
        ViewItem &view = GetView(m_currentView);
        H264DBPList *pDPB = view.GetDPBList(0);

        // in the following case(s) we can lay on the base view only,
        // because the buffers are managed synchronously.

        // frame is being processed. Wait for asynchronous end of operation.
        if (pDPB->IsDisposableExist())
        {
            return UMC_WRN_INFO_NOT_READY;
        }

        // frame is finished, but still referenced.
        // Wait for asynchronous complete.
        if (pDPB->IsAlmostDisposableExist())
        {
            return UMC_WRN_INFO_NOT_READY;
        }

        // some more hard reasons of frame lacking.
        if (!m_pTaskBroker->IsEnoughForStartDecoding(true))
        {
            if (CompleteDecodedFrames(0) == UMC_OK)
                return UMC_WRN_INFO_NOT_READY;

            if (!m_DefaultNotifyChain.IsEmpty())
            {
                m_DefaultNotifyChain.Notify();
                return UMC_WRN_INFO_NOT_READY;
            }

            if (GetFrameToDisplayInternal(true))
                return UMC_ERR_NOT_ENOUGH_BUFFER;

            PreventDPBFullness();
            return UMC_WRN_INFO_NOT_READY;
        }

        return UMC_WRN_INFO_NOT_READY;
    }

    return umcRes;
}

Status TaskSupplier::GetFrame(MediaData * pSource, MediaData *dst)
{
    Status umcRes = AddSource(pSource, dst);

    if (umcRes == UMC_WRN_REPOSITION_INPROGRESS)
        umcRes = UMC_OK;

    if (UMC_OK != umcRes && UMC_ERR_SYNC != umcRes && UMC_ERR_NOT_ENOUGH_BUFFER != umcRes && UMC_ERR_NOT_ENOUGH_DATA != umcRes && umcRes != UMC_WRN_INFO_NOT_READY)
        return umcRes;

    if (!dst)
        return umcRes;

    bool force = (umcRes == UMC_WRN_INFO_NOT_READY) || (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER) || !pSource;

    if (umcRes == UMC_ERR_NOT_ENOUGH_BUFFER)
        return UMC_ERR_NOT_ENOUGH_BUFFER;

    if (!pSource)
    {
        do
        {
            umcRes = RunDecoding(force);
        } while (umcRes == UMC_OK);

        return umcRes;
    }
    else
    {
        return RunDecoding(force);
    }
}

Status TaskSupplier::ProcessNalUnit(MediaDataEx *nalUnit)
{
    Status umcRes = UMC_OK;

    MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

    switch ((NAL_Unit_Type)pMediaDataEx->values[pMediaDataEx->index])
    {
    case NAL_UT_IDR_SLICE:
    case NAL_UT_SLICE:
    case NAL_UT_AUXILIARY:
    case NAL_UT_CODED_SLICE_EXTENSION:
        {
            H264Slice * pSlice = DecodeSliceHeader(nalUnit);
            if (pSlice)
            {
                umcRes = AddSlice(pSlice, false);
            }
        }
        break;

    case NAL_UT_SPS:
    case NAL_UT_PPS:
    case NAL_UT_SPS_EX:
    case NAL_UT_SUBSET_SPS:
    case NAL_UT_PREFIX:
        umcRes = DecodeHeaders(nalUnit);
        break;

    case NAL_UT_SEI:
        umcRes = DecodeSEI(nalUnit);
        break;
    case NAL_UT_AUD:
        umcRes = AddSlice(0, false);
        break;

    case NAL_UT_DPA: //ignore it
    case NAL_UT_DPB:
    case NAL_UT_DPC:
    case NAL_UT_FD:
    case NAL_UT_UNSPECIFIED:
        break;

    case NAL_UT_END_OF_STREAM:
    case NAL_UT_END_OF_SEQ:
        {
            umcRes = AddSlice(0, false);
            m_WaitForIDR = true;
        }
        break;

    default:
        break;
    };

    return umcRes;
}

Status TaskSupplier::AddOneFrame(MediaData * pSource, MediaData *dst)
{
    Status umsRes = UMC_OK;

    if (m_pLastSlice)
    {
        Status sts = AddSlice(m_pLastSlice, !pSource);
        if (sts == UMC_ERR_NOT_ENOUGH_BUFFER)
        {
            return sts;
        }

        if (sts == UMC_OK)
            return sts;
    }

    bool is_header_readed = false;

    do
    {
        if (!dst && is_header_readed)
        {
            Ipp32s iCode = m_pNALSplitter->CheckNalUnitType(pSource);
            switch (iCode)
            {
            case NAL_UT_IDR_SLICE:
            case NAL_UT_SLICE:
            case NAL_UT_AUXILIARY:
            case NAL_UT_CODED_SLICE_EXTENSION:

            case NAL_UT_DPA: //ignore it
            case NAL_UT_DPB:
            case NAL_UT_DPC:

            case NAL_UT_SEI:
                return UMC_OK;
            }
        }

        MediaDataEx *nalUnit = m_pNALSplitter->GetNalUnits(pSource);

        if (!nalUnit && pSource)
        {
            Ipp32u flags = pSource->GetFlags();

            if (!(flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
            {
                VM_ASSERT(!m_pLastSlice);
                return AddSlice(0, true);
            }

            return is_header_readed && !dst ? UMC_OK : UMC_ERR_SYNC;
        }

        if (!nalUnit)
        {
            if (!pSource)
                return AddSlice(0, true);

            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();

        for (Ipp32s i = 0; i < (Ipp32s)pMediaDataEx->count; i++, pMediaDataEx->index ++)
        {
            if (!dst)
            {
                switch ((NAL_Unit_Type)pMediaDataEx->values[i]) // skip data at DecodeHeader mode
                {
                case NAL_UT_IDR_SLICE:
                case NAL_UT_SLICE:
                case NAL_UT_AUXILIARY:
                case NAL_UT_CODED_SLICE_EXTENSION:

                case NAL_UT_DPA: //ignore it
                case NAL_UT_DPB:
                case NAL_UT_DPC:

                case NAL_UT_SEI:
                    if (!is_header_readed)
                    {
                        continue;
                    }
                    else
                    {
                        return UMC_OK;
                    }
                case NAL_UT_UNSPECIFIED:
                case NAL_UT_SPS:
                case NAL_UT_PPS:
                case NAL_UT_AUD:
                case NAL_UT_END_OF_SEQ:
                case NAL_UT_END_OF_STREAM:
                case NAL_UT_FD:
                case NAL_UT_SPS_EX:
                case NAL_UT_PREFIX:
                case NAL_UT_SUBSET_SPS:
                default:
                    break;
                }
            }

            if ((NAL_UT_IDR_SLICE != (NAL_Unit_Type)pMediaDataEx->values[i]) &&
                (NAL_UT_SLICE != (NAL_Unit_Type)pMediaDataEx->values[i]))
            {
                // Reset last prefix NAL unit 
                m_Headers.m_nalExtension.extension_present = 0;
            }

            switch ((NAL_Unit_Type)pMediaDataEx->values[i])
            {
            case NAL_UT_IDR_SLICE:
            case NAL_UT_SLICE:
            case NAL_UT_AUXILIARY:
            case NAL_UT_CODED_SLICE_EXTENSION:
                {
                H264Slice * pSlice = DecodeSliceHeader(nalUnit);
                if (pSlice)
                {
                    umsRes = AddSlice(pSlice, !pSource);
                    if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK)
                    {
                        return umsRes;
                    }
                }
                }
                break;

            case NAL_UT_SPS:
            case NAL_UT_PPS:
            case NAL_UT_SPS_EX:
            case NAL_UT_SUBSET_SPS:
            case NAL_UT_PREFIX:
                umsRes = DecodeHeaders(nalUnit);
                if (umsRes != UMC_OK)
                {
                    if (umsRes == UMC_NTF_NEW_RESOLUTION)
                    {
                        Ipp32s nalIndex = pMediaDataEx->index;
                        Ipp32s size = pMediaDataEx->offsets[nalIndex + 1] - pMediaDataEx->offsets[nalIndex];

                        pSource->MoveDataPointer(- size - 3);
                    }
                    return umsRes;
                }
                is_header_readed = true;
                break;

            case NAL_UT_SEI:
                DecodeSEI(nalUnit);
                break;
            case NAL_UT_AUD:  //ignore it
/*                {
                    Status sts = AddSlice(0, !pSource);
                    if (sts == UMC_OK)
                    {
                        return sts;
                    }
                }*/
                break;

            case NAL_UT_END_OF_STREAM:
            case NAL_UT_END_OF_SEQ:
                {
                    umsRes = AddSlice(0, !pSource);
                    m_WaitForIDR = true;
                    if (umsRes == UMC_OK)
                    {
                        return umsRes;
                    }
                }
                break;

            case NAL_UT_DPA: //ignore it
            case NAL_UT_DPB:
            case NAL_UT_DPC:
            case NAL_UT_FD:
            default:
                break;
            };
        }

    } while ((pSource) && (MINIMAL_DATA_SIZE < pSource->GetDataSize()));

    if (!pSource)
    {
        return AddSlice(0, true);
    }
    else
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            return AddSlice(0, true);
        }
    }

    return UMC_ERR_NOT_ENOUGH_DATA;
}

static
bool IsFieldOfOneFrame(const H264DecoderFrame *pFrame, const H264Slice * pSlice1, const H264Slice *pSlice2)
{
    if (!pFrame)
        return false;

    if (pFrame && pFrame->GetAU(0)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED
        && pFrame->GetAU(1)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return false;

    if ((pSlice1->GetSliceHeader()->nal_ref_idc && !pSlice2->GetSliceHeader()->nal_ref_idc) ||
        (!pSlice1->GetSliceHeader()->nal_ref_idc && pSlice2->GetSliceHeader()->nal_ref_idc))
        return false;

    if (pSlice1->GetSliceHeader()->field_pic_flag != pSlice2->GetSliceHeader()->field_pic_flag)
        return false;

//    if (pSlice1->GetSliceHeader()->frame_num != pSlice2->GetSliceHeader()->frame_num)
  //      return false;

    if (pSlice1->GetSliceHeader()->bottom_field_flag == pSlice2->GetSliceHeader()->bottom_field_flag)
        return false;

    return true;
}

H264Slice * TaskSupplier::DecodeSliceHeader(MediaDataEx *nalUnit)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    if (m_Headers.m_nalExtension.extension_present)
    {
        if (SVC_Extension::IsShouldSkipSlice(&m_Headers.m_nalExtension))
        {
            m_Headers.m_nalExtension.extension_present = 0;
            return 0;
        }
    }

    H264Slice * pSlice = m_ObjHeap.AllocateObject<H264Slice>();
    pSlice->SetHeap(&m_ObjHeap);
    pSlice->IncrementReference();

    notifier0<H264Slice> memory_leak_preventing_slice(pSlice, &H264Slice::DecrementReference);

    H264MemoryPiece memCopy;
    memCopy.SetData(nalUnit);

    pSlice->m_pSource.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

    notifier0<H264MemoryPiece> memory_leak_preventing(&pSlice->m_pSource, &H264MemoryPiece::Release);
    memset(pSlice->m_pSource.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

    SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    swapper->SwapMemory(&pSlice->m_pSource, &memCopy);

    Ipp32s pps_pid = pSlice->RetrievePicParamSetNumber();
    if (pps_pid == -1)
    {
        return 0;
    }

    H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);

    if (m_WaitForIDR)
    {
        if (pSlice->GetSliceHeader()->slice_type != INTRASLICE && !spl)
        {
            return 0;
        }
    }

    pSlice->m_pPicParamSet = m_Headers.m_PicParams.GetHeader(pps_pid);
    if (!pSlice->m_pPicParamSet)
    {
        return 0;
    }

    Ipp32s seq_parameter_set_id = pSlice->m_pPicParamSet->seq_parameter_set_id;

    if (NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type)
    {
        if (pSlice->GetSliceHeader()->nal_ext.svc_extension_flag == 0)
        {
            pSlice->m_pSeqParamSetSvcEx = 0;
            pSlice->m_pSeqParamSet = pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetHeader(seq_parameter_set_id);

            if (NULL == pSlice->m_pSeqParamSet)
            {
                return 0;
            }

            m_Headers.m_SeqParamsMvcExt.SetCurrentID(pSlice->m_pSeqParamSetMvcEx->seq_parameter_set_id);
            m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
        }
        else
        {
            pSlice->m_pSeqParamSetMvcEx = 0;
            pSlice->m_pSeqParamSet = pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetHeader(seq_parameter_set_id);

            if (NULL == pSlice->m_pSeqParamSet)
            {
                return 0;
            }

            m_Headers.m_SeqParamsSvcExt.SetCurrentID(pSlice->m_pSeqParamSetSvcEx->seq_parameter_set_id);
            m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
        }
    }
    else
    {
        pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
        pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
        pSlice->m_pSeqParamSet = m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id);

        if (!pSlice->m_pSeqParamSet)
        {
            return 0;
        }

        m_Headers.m_SeqParams.SetCurrentID(pSlice->m_pPicParamSet->seq_parameter_set_id);
        m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
    }

    Status sts = InitializePictureParamSet(m_Headers.m_PicParams.GetHeader(pps_pid), pSlice->m_pSeqParamSet, NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type);
    if (sts != UMC_OK)
    {
        return 0;
    }

    pSlice->m_pSeqParamSetEx = m_Headers.m_SeqExParams.GetHeader(seq_parameter_set_id);
    pSlice->m_pCurrentFrame = 0;

    memory_leak_preventing.ClearNotification();
    pSlice->m_dTime = pSlice->m_pSource.GetTime();

    if (!pSlice->Reset(&m_Headers.m_nalExtension))
    {
        return 0;
    }

    if (IsShouldSkipSlice(pSlice))
        return 0;

    if (!IsExtension())
    {
        memset(&pSlice->GetSliceHeader()->nal_ext, 0, sizeof(H264NalExtension));
    }

    if (spl)
    {
        if (m_WaitForIDR)
        {
            AllocateAndInitializeView(pSlice);
            ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
            Ipp32u recoveryFrameNum = (spl->SEI_messages.recovery_point.recovery_frame_cnt + pSlice->GetSliceHeader()->frame_num) % (1 << pSlice->m_pSeqParamSet->log2_max_frame_num);
            view.GetDPBList(0)->SetRecoveryFrameCnt(recoveryFrameNum);
        }

        if ((pSlice->GetSliceHeader()->slice_type != INTRASLICE))
        {
            H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);
            m_Headers.m_SEIParams.RemoveHeader(spl);
        }
    }

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    //DEBUG_PRINT((VM_STRING("debug headers slice - view - %d \n"), pSlice->GetSliceHeader()->nal_ext.mvc.view_id));

    return pSlice;
}

Status TaskSupplier::AddSlice(H264Slice * pSlice, bool force)
{
    if (!m_accessUnit.GetLayersCount() && pSlice)
    {
        if (m_sei_messages)
            m_sei_messages->SetAUID(m_accessUnit.GetAUIndentifier());
    }

    if ((!pSlice || m_accessUnit.IsFullAU() || !m_accessUnit.AddSlice(pSlice)) && m_accessUnit.GetLayersCount())
    {
        m_pLastSlice = pSlice;
        if (!m_accessUnit.m_isInitialized)
        {
            InitializeLayers(&m_accessUnit, 0, 0);

            size_t layersCount = m_accessUnit.GetLayersCount();
            Ipp32u maxDId = layersCount ? m_accessUnit.GetLayer(layersCount - 1)->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id : 0;
            for (size_t i = 0; i < layersCount; i++)
            {
                SetOfSlices * setOfSlices = m_accessUnit.GetLayer(i);
                H264Slice * slice = setOfSlices->GetSlice(0);

                AllocateAndInitializeView(slice);

                ViewItem &view = GetView(slice->GetSliceHeader()->nal_ext.mvc.view_id);
                Ipp32s dId = slice->GetSliceHeader()->nal_ext.svc.dependency_id;
                Ipp32s index = GetDPBLayerIndex(maxDId, dId);

                if (view.pCurFrame && view.pCurFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                {
                    H264Slice * pFirstFrameSlice = view.pCurFrame->GetAU(0)->GetSlice(0);
                    if (!pFirstFrameSlice || !IsFieldOfOneFrame(view.pCurFrame, pFirstFrameSlice, slice))
                    {
                        ProcessNonPairedField(view.pCurFrame);
                        OnFullFrame(view.pCurFrame);
                        view.pCurFrame = 0;
                    }
                }

                if (view.GetPOCDecoder(index)->DetectFrameNumGap(slice))
                {
                    view.pCurFrame = 0;

                    Status umsRes = ProcessFrameNumGap(slice, 0, dId, maxDId);
                    if (umsRes != UMC_OK)
                        return umsRes;
                }
            }

            m_accessUnit.m_isInitialized = true;
        }

        size_t layersCount = m_accessUnit.GetLayersCount();

        if (m_decodingMode == SVC_DECODING_MODE && layersCount)
        { // allocates primary frame
            Ipp32u maxDId = layersCount ? m_accessUnit.GetLayer(layersCount - 1)->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id : 0;
            SetOfSlices * setOfSlices = m_accessUnit.GetLayer(layersCount-1);

            if (!setOfSlices->m_frame)
            {
                H264Slice * slice = setOfSlices->GetSlice(0);
                setOfSlices->m_frame = AllocateNewFrame(slice);
                if (!setOfSlices->m_frame)
                    return UMC_ERR_NOT_ENOUGH_BUFFER;

                H264DecoderFrame *frame = setOfSlices->m_frame;
                frame->m_maxDId = maxDId;

                size_t sliceCount = setOfSlices->GetSliceCount();
                frame->m_maxQId = m_accessUnit.GetLayer(layersCount - 1)->GetSlice(sliceCount - 1)->GetSliceHeader()->nal_ext.svc.quality_id;
                frame->m_minDId = m_accessUnit.GetLayer(0)->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id;
                frame->m_pLayerFrames[maxDId] = frame;

                InitFrameCounter(frame, slice);
                return UMC_OK;
            }

            H264DecoderFrame *primaryFrame = setOfSlices->m_frame;

            for (size_t i = 0; i < layersCount; i++)
            {
                SetOfSlices * setOfSlices = m_accessUnit.GetLayer(i);
                Ipp32u dId = setOfSlices->GetSlice(0)->GetSliceHeader()->nal_ext.svc.dependency_id;
                if (setOfSlices->m_frame)
                {
                    if (setOfSlices->m_frame->m_firstSliceInBaseRefLayer != -1)
                        continue;

                    size_t count = setOfSlices->GetSliceCount();
                    for (size_t sliceId = 0; sliceId < count; sliceId++)
                    { // find and allocate base ref frame
                        H264Slice * slice = setOfSlices->GetSlice(sliceId);

                        if (slice->GetSliceHeader()->nal_ext.svc.quality_id || !slice->GetSliceHeader()->nal_ext.svc.store_ref_base_pic_flag || !setOfSlices->m_frame->m_maxQId)
                            continue;

                        // allocate surface for base ref pic 
                        Status umcRes = AllocateDataForFrameSVC(setOfSlices->m_frame, slice);
                        if (umcRes != UMC_OK)
                        {
                            return umcRes;
                        }

                        setOfSlices->m_frame->m_firstSliceInBaseRefLayer = 0;
                        for (size_t k = 0; k < i; k++)
                            setOfSlices->m_frame->m_firstSliceInBaseRefLayer += (Ipp32s)m_accessUnit.GetLayer(k)->GetSliceCount();

                        setOfSlices->m_frame->allocateRefBasePicture();
                        setOfSlices->m_frame->m_firstSliceInBaseRefLayer += (Ipp32s)sliceId;
                        return UMC_OK;
                    }
                }

                if (setOfSlices->m_frame)
                    continue;

                H264Slice * slice = setOfSlices->GetSlice(0);

                if (i < layersCount - 1)
                {
                    GetView(BASE_VIEW).pCurFrame = primaryFrame;
                    Ipp32s index = GetDPBLayerIndex(primaryFrame->m_maxDId, dId);

                    H264DecoderFrame *frame = AddLayerFrameSVC(GetView(BASE_VIEW).GetDPBList(index), slice);
                    if (!frame)
                    {
                        return UMC_ERR_NOT_ENOUGH_BUFFER;
                    }

                    primaryFrame->m_pLayerFrames[dId] = frame;
                    setOfSlices->m_frame = frame;

                    frame->m_maxDId = primaryFrame->m_maxDId;
                    frame->m_minDId = primaryFrame->m_minDId;
                    frame->m_UID = primaryFrame->m_UID;

                    InitFrameCounter(frame, slice);

                    if (frame != primaryFrame)
                    {
                        size_t allSliceCount = 0;

                        size_t layersCount = m_accessUnit.GetLayersCount();
                        for (size_t i = 0; i < layersCount; i++)
                        {
                            SetOfSlices * setOfSlices = m_accessUnit.GetLayer(i);
                            allSliceCount += setOfSlices->GetSliceCount();
                        }

                        frame->GetAU(0)->IncreaseRefPicList(2*allSliceCount + 1); // 2* - for fields
                        frame->GetAU(1)->IncreaseRefPicList(2*allSliceCount + 1); // 2* - for fields
                    }

                    setOfSlices->m_frame = primaryFrame;
                }
                else
                {
                    setOfSlices->m_frame = AllocateNewFrame(slice);
                    if (!setOfSlices->m_frame)
                        return UMC_ERR_NOT_ENOUGH_BUFFER;
                }

                ApplyPayloadsToFrame(setOfSlices->m_frame, slice, &setOfSlices->m_payloads);
                setOfSlices->m_frame->m_auIndex = m_accessUnit.GetAUIndentifier();
                return UMC_OK;
            }
        }
        else
        {
            for (size_t i = 0; i < layersCount; i++)
            {
                SetOfSlices * setOfSlices = m_accessUnit.GetLayer(i);
                H264Slice * slice = setOfSlices->GetSlice(0);

                if (setOfSlices->m_frame)
                    continue;

                setOfSlices->m_frame = AllocateNewFrame(slice);
                if (!setOfSlices->m_frame)
                    return UMC_ERR_NOT_ENOUGH_BUFFER;

                setOfSlices->m_frame->m_auIndex = m_accessUnit.GetAUIndentifier();
                ApplyPayloadsToFrame(setOfSlices->m_frame, slice, &setOfSlices->m_payloads);
                setOfSlices->m_frame->m_pLayerFrames[0] = setOfSlices->m_frame;
                return UMC_OK;
            }
        }

        if (m_decodingMode == SVC_DECODING_MODE)
        {
            m_accessUnit.CombineSets();
            layersCount = m_accessUnit.GetLayersCount();
        }

        for (size_t i = 0; i < layersCount; i++)
        {
            SetOfSlices * setOfSlices = m_accessUnit.GetLayer(i);
            if (setOfSlices->m_isCompleted)
                continue;

            H264Slice * slice = setOfSlices->GetSlice(setOfSlices->GetSliceCount() - 1);

            m_currentView = slice->GetSliceHeader()->nal_ext.mvc.view_id;
            ViewItem &view = GetView(m_currentView);
            view.pCurFrame = setOfSlices->m_frame;

            const H264SliceHeader *sliceHeader = slice->GetSliceHeader();
            Ipp32u field_index = setOfSlices->m_frame->GetNumberByParity(sliceHeader->bottom_field_flag);
    
            if (!setOfSlices->m_frame->GetAU(field_index)->GetSliceCount())
            {
                size_t count = setOfSlices->GetSliceCount();
                for (size_t sliceId = 0; sliceId < count; sliceId++)
                {
                    H264Slice * slice = setOfSlices->GetSlice(sliceId);
                    slice->m_pCurrentFrame = setOfSlices->m_frame;
                    AddSliceToFrame(setOfSlices->m_frame, slice);

                    if (slice->GetSliceHeader()->slice_type != INTRASLICE)
                    {
                        Ipp32u NumShortTermRefs, NumLongTermRefs, NumInterViewRefs = 0;
                        view.GetDPBList(0)->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
                        // calculate number of inter-view references
                        if (slice->GetSliceHeader()->nal_ext.mvc.view_id)
                        {
                            NumInterViewRefs = GetInterViewFrameRefs(m_views,
                                                                     slice->GetSliceHeader()->nal_ext.mvc.view_id,
                                                                     setOfSlices->m_frame->m_auIndex,
                                                                     slice->GetSliceHeader()->bottom_field_flag);
                        }

                        if (NumShortTermRefs + NumLongTermRefs + NumInterViewRefs == 0)
                            AddFakeReferenceFrame(slice);
                    }

                    if (m_decodingMode != SVC_DECODING_MODE)
                    {
                        slice->UpdateReferenceList(m_views, 0);
                    }
                }
            }

            Status umcRes;
            if ((umcRes = CompleteFrame(setOfSlices->m_frame, field_index)) != UMC_OK)
            {
                return umcRes;
            }

            if (setOfSlices->m_frame->m_PictureStructureForDec < FRM_STRUCTURE && force && !pSlice)
            {
                if (ProcessNonPairedField(setOfSlices->m_frame))
                {
                    OnFullFrame(setOfSlices->m_frame);
                    view.pCurFrame = 0;
                }
            }

            if (setOfSlices->m_frame->m_PictureStructureForDec >= FRM_STRUCTURE || field_index)
            {
                OnFullFrame(setOfSlices->m_frame);
                view.pCurFrame = 0;
            }

            setOfSlices->m_isCompleted = true;
        }

        m_accessUnit.Reset();
        return layersCount ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;
    }

    m_pLastSlice = 0;
    return UMC_ERR_NOT_ENOUGH_DATA;
}

void TaskSupplier::AddFakeReferenceFrame(H264Slice * pSlice)
{
    H264DecoderFrame *pFrame = GetFreeFrame(pSlice);
    if (!pFrame)
        return;

    Status umcRes = InitFreeFrame(pFrame, pSlice);
    if (umcRes != UMC_OK)
    {
        return;
    }

    umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pFrame->m_bpp, pFrame->GetColorFormat());
    if (umcRes != UMC_OK)
    {
        return;
    }

    Ipp32s frame_num = pSlice->GetSliceHeader()->frame_num;
    if (pSlice->GetSliceHeader()->field_pic_flag == 0)
    {
        pFrame->setPicNum(frame_num, 0);
    }
    else
    {
        pFrame->setPicNum(frame_num*2+1, 0);
        pFrame->setPicNum(frame_num*2+1, 1);
    }

    pFrame->SetisShortTermRef(true, 0);
    pFrame->SetisShortTermRef(true, 1);

    pFrame->SetSkipped(true);
    pFrame->SetFrameExistFlag(false);
    pFrame->SetisDisplayable();

    pFrame->DefaultFill(2, false);

    H264SliceHeader* sliceHeader = pSlice->GetSliceHeader();
    ViewItem &view = GetView(sliceHeader->nal_ext.mvc.view_id);
    Ipp32s index = GetDPBLayerIndex(0, sliceHeader->nal_ext.svc.dependency_id); // DEBUG: set maxDid !!!

    if (pSlice->GetSeqParam()->pic_order_cnt_type != 0)
    {
        Ipp32s tmp1 = sliceHeader->delta_pic_order_cnt[0];
        Ipp32s tmp2 = sliceHeader->delta_pic_order_cnt[1];
        sliceHeader->delta_pic_order_cnt[0] = sliceHeader->delta_pic_order_cnt[1] = 0;

        view.GetPOCDecoder(index)->DecodePictureOrderCount(pSlice, frame_num);

        sliceHeader->delta_pic_order_cnt[0] = tmp1;
        sliceHeader->delta_pic_order_cnt[1] = tmp2;
    }

    view.GetPOCDecoder(index)->DecodePictureOrderCountFakeFrames(pFrame, sliceHeader);

    // mark generated frame as short-term reference
    {
        // reset frame global data
        H264DecoderMacroblockGlobalInfo *pMBInfo = pFrame->m_mbinfo.mbs;
        memset(pMBInfo, 0, pFrame->totalMBs*sizeof(H264DecoderMacroblockGlobalInfo));
    }
}

bool TaskSupplier::ProcessNonPairedField(H264DecoderFrame * pFrame)
{
    if (pFrame && pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_NOT_FILLED)
    {
        H264Slice *firstSliceInDepLevel[MAX_NUM_LAYERS];
        H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(0);
        Ipp32s sliceCount = slicesInfo->GetSliceCount();
        Ipp32s dId = pFrame->m_minDId;

        for (Ipp32s i = 0; i < sliceCount; i++)
        {
            H264Slice * pSlice = slicesInfo->GetSlice(i);

            if (pSlice->m_bIsFirstSliceInDepLayer)
            {
                firstSliceInDepLevel[dId] = pSlice;
            }
        }

        for (Ipp32s i = pFrame->m_minDId; i <= pFrame->m_maxDId; i++)
        {
            H264DecoderFrame *layerFrame = pFrame->m_pLayerFrames[i];

            layerFrame->setPicOrderCnt(layerFrame->PicOrderCnt(0, 1), 1);
            layerFrame->m_bottom_field_flag[1] = !layerFrame->m_bottom_field_flag[0];
            layerFrame->setPicNum(firstSliceInDepLevel[i]->GetSliceHeader()->frame_num*2 + 1, 1);
        }

        pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_NONE);

        Ipp32s isBottom = firstSliceInDepLevel[pFrame->m_maxDId]->IsBottomField() ? 0 : 1;
        pFrame->DefaultFill(isBottom, false);
        return true;
    }

    return false;
}

void TaskSupplier::OnFullFrame(H264DecoderFrame * pFrame)
{
    pFrame->SetFullFrame(true);

    ViewItem &view = GetView(pFrame->m_viewId);

    if (!view.m_isDisplayable)
    {
        pFrame->setWasOutputted();
        pFrame->setWasDisplayed();
    }

    if (pFrame->IsSkipped())
        return;

    if (pFrame->m_bIDRFlag && !(pFrame->GetError() & ERROR_FRAME_DPB))
    {
        DecReferencePictureMarking::ResetError();
    }

    if (DecReferencePictureMarking::GetDPBError())
    {
        pFrame->SetErrorFlagged(ERROR_FRAME_DPB);
    }
}

void TaskSupplier::CalculateResizeParameters(H264DecoderFrame * pFrame, Ipp32s field)
{
    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);
    Ipp32s sliceCount = slicesInfo->GetSliceCount();

    if (!sliceCount)
        return;

    for (Ipp32s i = 0; i < sliceCount; i++)
    {
        H264Slice *pSlice = slicesInfo->GetSlice(i);
        H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();

        if (!pSlice->m_bIsFirstILPredSliceInDepLayer || pSliceHeader->ref_layer_dq_id < 0 || pSlice->m_bDecoded)
            continue;

        H264Slice * refSlice = 0;
        for (Ipp32u k = 0; k < slicesInfo->GetSliceCount(); k++)
        {
            H264Slice * slice = slicesInfo->GetSlice(k);
            Ipp32s dqId = (slice->GetSliceHeader()->nal_ext.svc.dependency_id << 4) + slice->GetSliceHeader()->nal_ext.svc.quality_id;
            if (pSliceHeader->ref_layer_dq_id == dqId)
            {
                refSlice = slice;
                break;
            }

            if (pSliceHeader->ref_layer_dq_id >= dqId)
            {
                refSlice = slice;
            }
        }

        H264DecoderLayerDescriptor *layerMbs = pSlice->GetMBInfo().layerMbs;
        H264DecoderLayerResizeParameter *lrp = &layerMbs[pSliceHeader->nal_ext.svc.dependency_id].lrp[field];
        lrp->UpdateLayerParams(pSlice);
        lrp->UpdateRefLayerParams(refSlice);
        lrp->EvaluateSpatialResolutionChange();

        layerMbs[pSliceHeader->ref_layer_dq_id >> 4].lrp[field].next_spatial_resolution_change =
            lrp->spatial_resolution_change;
    }
}

Status TaskSupplier::InitializeLayers(AccessUnit *accessUnit, H264DecoderFrame * pFrame, Ipp32s field)
{
    pFrame;
    field;

    accessUnit->SortforASO();
    size_t layersCount = accessUnit->GetLayersCount();

    if (!layersCount)
        return UMC_OK;

    if (layersCount == 1 && !accessUnit->GetLayer(0)->GetSlice(0)->IsSliceGroups())
    {
        H264Slice * slice = accessUnit->GetLayer(0)->GetSlice(0);
        if (slice->m_iFirstMB)
        {
            m_pSegmentDecoder[0]->RestoreErrorRect(0, slice->m_iFirstMB, slice);
        }
    }

    if ((m_decodingMode == MVC_DECODING_MODE || m_decodingMode == SVC_DECODING_MODE) && layersCount > 1)
    {
        H264Slice * sliceExtension = 0;
        for (size_t i = layersCount - 1; i >= 1; i--)
        {
            SetOfSlices * setOfSlices = accessUnit->GetLayer(i);
            size_t sliceCount = setOfSlices->GetSliceCount();
            for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
            {
                H264Slice * slice = setOfSlices->GetSlice(sliceId);
                if (NAL_UT_CODED_SLICE_EXTENSION == slice->GetSliceHeader()->nal_unit_type)
                {
                    sliceExtension = slice;
                    break;
                }
            }

            if (sliceExtension)
                break;
        }

        if (sliceExtension)
        {
            SetOfSlices * setOfSlices = accessUnit->GetLayer(0);
            size_t sliceCount = setOfSlices->GetSliceCount();
            for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
            {
                H264Slice * slice = setOfSlices->GetSlice(sliceId);
                if (NAL_UT_CODED_SLICE_EXTENSION != slice->GetSliceHeader()->nal_unit_type)
                {
                    if (!slice->m_pSeqParamSetMvcEx)
                        slice->SetSeqMVCParam(sliceExtension->m_pSeqParamSetMvcEx);
                    if (!slice->m_pSeqParamSetSvcEx)
                        slice->SetSeqSVCParam(sliceExtension->m_pSeqParamSetSvcEx);
                }
            }
        }
    }

    if (m_decodingMode != SVC_DECODING_MODE)
    {
        for (size_t i = 0; i < layersCount; i++)
        {
            SetOfSlices * setOfSlices = accessUnit->GetLayer(i);
            size_t sliceCount = setOfSlices->GetSliceCount();
            for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
            {
                H264Slice * slice = setOfSlices->GetSlice(sliceId);
                slice->m_firstILPredSliceInDepLayer = slice;
                slice->m_bIsFirstILPredSliceInDepLayer = true;
                slice->m_bIsMaxDId = true;
                slice->m_bIsMaxQId = true;

                slice->m_bIsFirstSliceInDepLayer = true;
                slice->m_bIsFirstSliceInLayer = true;
                slice->m_bIsLastSliceInDepLayer = true;
            }
        }

        return UMC_OK;
    }

    for (size_t i = 0; i < layersCount; i++)
    {
        SetOfSlices * setOfSlices = accessUnit->GetLayer(i);
        if (!setOfSlices->GetSliceCount())
            continue;

        size_t sliceCount = setOfSlices->GetSliceCount();
        for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
        {
            H264Slice * slice = setOfSlices->GetSlice(sliceId);
            slice->m_firstILPredSliceInDepLayer = slice;
            slice->m_bIsFirstILPredSliceInDepLayer = false;
            slice->m_bIsMaxDId = i == (layersCount - 1);
            slice->m_bIsMaxQId = false;
        }

        setOfSlices->GetSlice(0)->m_firstILPredSliceInDepLayer = setOfSlices->GetSlice(0);
        setOfSlices->GetSlice(0)->m_bIsFirstILPredSliceInDepLayer = true;

        if (i != 0) // if it is not first layer
        {
            for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
            {
                H264Slice * slice = setOfSlices->GetSlice(sliceId);
                if (slice->GetSliceHeader()->nal_ext.svc.no_inter_layer_pred_flag == 0 && slice->GetSliceHeader()->nal_ext.svc.quality_id == 0)
                {
                    setOfSlices->GetSlice(0)->m_firstILPredSliceInDepLayer = slice;
                    setOfSlices->GetSlice(0)->m_bIsFirstILPredSliceInDepLayer = true;
                    slice->m_bIsFirstILPredSliceInDepLayer = true;
                    break;
                }
            }
        }

        Ipp32u quality_id = 100;
        for (size_t sliceId = 0; sliceId < sliceCount; sliceId++)
        {
            H264Slice * slice = setOfSlices->GetSlice(sliceId);
            slice->m_bIsFirstSliceInDepLayer = false;
            slice->m_bIsFirstSliceInLayer = false;
            slice->m_bIsLastSliceInDepLayer = false;
            if (slice->GetSliceHeader()->nal_ext.svc.quality_id != quality_id)
            {
                slice->m_bIsFirstSliceInLayer = true;
                quality_id = slice->GetSliceHeader()->nal_ext.svc.quality_id;
            }
        }

        for (Ipp32s sliceId = (Ipp32s)sliceCount - 1; sliceId >= 0; --sliceId)
        {
            H264Slice * slice = setOfSlices->GetSlice(sliceId);
            if (slice->GetSliceHeader()->nal_ext.svc.quality_id != quality_id)
                break;
            slice->m_bIsMaxQId = true;
        }

        setOfSlices->GetSlice(0)->m_bIsFirstSliceInDepLayer = true;
        setOfSlices->GetSlice(sliceCount - 1)->m_bIsLastSliceInDepLayer = true;
    }

    /* Non reference slice should be marked as Decoded and Deblocked */
    Ipp32s curLayer = 0;
    if (layersCount)
    {
        SetOfSlices * setOfSlices = m_accessUnit.GetLayer(layersCount - 1);
        size_t count = setOfSlices->GetSliceCount();
        curLayer = (setOfSlices->GetSlice(count - 1)->GetSliceHeader()->nal_ext.svc.dependency_id << 4) + 
            setOfSlices->GetSlice(count - 1)->GetSliceHeader()->nal_ext.svc.quality_id;
    }

    for(;;)
    {
        Ipp32s currDLayer = curLayer >> 4;
        Ipp32s layerPos = m_accessUnit.FindLayerByDependency(currDLayer);
        if (layerPos < 0)
        {
            VM_ASSERT(layerPos >= 0);
            break;
        }

        SetOfSlices * setOfSlices = m_accessUnit.GetLayer(layerPos);
        Ipp32s baseLayer = -1;

        for (size_t sliceId = 0; sliceId < setOfSlices->GetSliceCount(); sliceId++)
        {
            H264Slice * slice = setOfSlices->GetSlice(sliceId);
            
            Ipp32s refDLayer = slice->GetSliceHeader()->ref_layer_dq_id >> 4;
            if (slice->GetSliceHeader()->ref_layer_dq_id != -1 && refDLayer != currDLayer)
            {
                baseLayer = slice->GetSliceHeader()->ref_layer_dq_id;
                break;
            }
        }

        Ipp32s startToRemove = baseLayer < 0 ? 0 :  (baseLayer >> 4) + 1;

        // remove useless dependencies
        for (Ipp32s depIter = startToRemove; depIter < currDLayer; depIter++)
        {
            Ipp32s layerPos = m_accessUnit.FindLayerByDependency(depIter);
            if (layerPos < 0)
                continue;

            SetOfSlices * set = m_accessUnit.GetLayer(layerPos);
            for (size_t k = 0; k < set->GetSliceCount(); ++k)
            {
                H264Slice * baseSlice = set->GetSlice(k);
                baseSlice->m_bDecoded = true;
                baseSlice->m_bDeblocked = true;
            }
        }

        if (baseLayer < 0 || currDLayer == (baseLayer >> 4))
            break;

        layerPos = m_accessUnit.FindLayerByDependency(baseLayer >> 4);
        if (layerPos < 0)
        {
            VM_ASSERT(layerPos >= 0);
            break;
        }

        SetOfSlices * baseSetOfSlices = m_accessUnit.GetLayer(layerPos);

        H264Slice *pSlice_last_in_dep = 0;
        size_t baseSliceCount = baseSetOfSlices->GetSliceCount();
        for (size_t i = 0; i < baseSliceCount; i++)
        {
            H264Slice * baseSlice = baseSetOfSlices->GetSlice(i);
            Ipp32s currDQ = (baseSlice->GetSliceHeader()->nal_ext.svc.dependency_id << 4) + baseSlice->GetSliceHeader()->nal_ext.svc.quality_id;

            if (currDQ == baseLayer)
            {
                baseSlice->m_bIsMaxQId = true;
                pSlice_last_in_dep = baseSlice;
            }

            if (currDQ > baseLayer)
            {
                for (Ipp32s k = (Ipp32s)baseSliceCount - 1; k >= (Ipp32s)i; --k)
                {
                    H264Slice * baseSlice = baseSetOfSlices->GetSlice(k);
                    baseSlice->m_bDecoded = true;
                    baseSlice->m_bDeblocked = true;
                }
                break;
            }
        }

        if (pSlice_last_in_dep)
            pSlice_last_in_dep->m_bIsLastSliceInDepLayer = true;

        curLayer = baseLayer;
    }

    m_accessUnit.CleanUseless();
    return UMC_OK;
}

Status TaskSupplier::InitializeLayers(H264DecoderFrame * pFrame, Ipp32s field)
{
    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);
    Ipp32s sliceCount = slicesInfo->GetSliceCount();

    if (m_decodingMode == SVC_DECODING_MODE)
    {
        for (Ipp32s i = 0; i < sliceCount; i++)
        {
            H264Slice * pSlice = slicesInfo->GetSlice(i);
            Ipp32s dId = (pSlice->GetSliceHeader())->nal_ext.svc.dependency_id;
            Ipp32s index = GetDPBLayerIndex(pFrame->m_maxDId, dId);

            H264DecoderFrame * temp = pSlice->m_pCurrentFrame;
            pSlice->m_pCurrentFrame = pFrame->m_pLayerFrames[dId];
            pSlice->UpdateReferenceList(m_views, index);
            pSlice->m_pCurrentFrame = temp;
        }
    }

    if (!field)
    {
        Ipp32s count = slicesInfo->GetSliceCount();
        H264Slice *pSlice = slicesInfo->GetSlice(count-1);

        Status umcRes = AllocateDataForFrameSVC(pFrame, pSlice);
        if (umcRes != UMC_OK)
        {
            return umcRes;
        }

        for (Ipp32s i = 0; i <= pFrame->m_maxDId; i++)
        {
            if (!pFrame->m_pLayerFrames[i])
                continue;

            pFrame->m_pLayerFrames[i]->m_index = pFrame->m_index;
        }

        // fill chroma planes in case of 4:0:0
        if (pFrame->m_chroma_format == 0)
        {
            pFrame->DefaultFill(2, true);
        }
    }

    // Find last slice with dependency_id == quality_id == 0
    pFrame->m_lastSliceInAVC = -1;
    for (Ipp32s i = sliceCount - 1; i >= 0; i--)
    {
        H264Slice * pSlice = slicesInfo->GetSlice(i);
        if ((pSlice->GetSliceHeader()->nal_ext.svc.dependency_id == 0) &&
            (pSlice->GetSliceHeader()->nal_ext.svc.quality_id == 0))
        {
            pFrame->m_lastSliceInAVC = i;
            break;
        }
    }

    /* Base ref layer */
    if (pFrame->m_maxQId)
    {
        for (Ipp32s i = sliceCount - 1; i >= 0; i--)
        {
            H264Slice * pSlice = slicesInfo->GetSlice(i);
            if ((pSlice->GetSliceHeader()->nal_ext.svc.dependency_id == pFrame->m_maxDId) &&
                (pSlice->GetSliceHeader()->nal_ext.svc.quality_id == 0))
            {
                pFrame->m_lastSliceInBaseRefLayer = i;
                pFrame->m_storeRefBasePic = pSlice->GetSliceHeader()->nal_ext.svc.store_ref_base_pic_flag;
                break;
            }
        }
    }

    return UMC_OK;
}

Status TaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    if (!pFrame)
        return UMC_OK;

    Status umcRes = CompleteFrame(UMC::GetAuxiliaryFrame(pFrame), field);
    if (umcRes != UMC_OK)
    {
        return umcRes;
    }

    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);

    if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return UMC_OK;

    Status umsRes = InitializeLayers(pFrame, field);
    if (umsRes != UMC_OK)
        return umsRes;

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d,%d) type - %d, picStruct - %d, field - %d, count - %d, m_uid - %d, IDR - %d, viewId - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], pFrame->m_FrameType, pFrame->m_displayPictureStruct, field, pFrame->GetAU(field)->GetSliceCount(), pFrame->m_UID, pFrame->m_bIDRFlag, pFrame->m_viewId));

    bool is_auxiliary_exist = slicesInfo->GetAnySlice()->GetSeqParamEx() &&
        (slicesInfo->GetAnySlice()->GetSeqParamEx()->aux_format_idc != 0);

    if (is_auxiliary_exist)
    {
        if (!pFrame->IsAuxiliaryFrame())
            DBPUpdate(pFrame, field);
    }
    else
    {
        DBPUpdate(pFrame, field);
    }

    pFrame->MoveToNotifiersChain(m_DefaultNotifyChain);

    // skipping algorithm
    {
        if (((slicesInfo->IsField() && field) || !slicesInfo->IsField()) &&
            IsShouldSkipFrame(pFrame, field))
        {
            if (slicesInfo->IsField())
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
                pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }
            else
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }

            for (Ipp32s i = 0; i <= pFrame->m_maxDId; i++)
            {
                if (!pFrame->m_pLayerFrames[i])
                    continue;

                pFrame->m_pLayerFrames[i]->SetisShortTermRef(false, 0);
                pFrame->m_pLayerFrames[i]->SetisShortTermRef(false, 1);
                pFrame->m_pLayerFrames[i]->SetisLongTermRef(false, 0);
                pFrame->m_pLayerFrames[i]->SetisLongTermRef(false, 1);
            }

            pFrame->SetSkipped(true);
            pFrame->OnDecodingCompleted();
            return UMC_OK;
        }
        else
        {
            if (IsShouldSkipDeblocking(pFrame, field))
            {
                pFrame->GetAU(field)->SkipDeblocking();
            }
        }
    }

    slicesInfo->SetStatus(H264DecoderFrameInfo::STATUS_FILLED);
    return UMC_OK;
}

Status TaskSupplier::AllocateDataForFrameSVC(H264DecoderFrame * pFrame, H264Slice *pSlice)
{
    const H264SeqParamSet *pSeqParam = pSlice->GetSeqParam();
    IppiSize dimensions;
    Ipp8u bit_depth_luma, bit_depth_chroma, chroma_format_idc;

    if (pFrame->IsAuxiliaryFrame())
    {
        bit_depth_luma = pSlice->GetSeqParamEx()->bit_depth_aux;
        bit_depth_chroma = 8;
        chroma_format_idc = 0;
    } else {
        bit_depth_luma = pSeqParam->bit_depth_luma;
        bit_depth_chroma = pSeqParam->bit_depth_chroma;
        chroma_format_idc = pSeqParam->chroma_format_idc;
    }

    dimensions.width = pSeqParam->frame_width_in_mbs * 16;
    dimensions.height = pSeqParam->frame_height_in_mbs * 16;

    if (m_decodingMode != SVC_DECODING_MODE)
    {
        return UMC_OK;
    }

    // Allocate the frame data
    return AllocateFrameData(pFrame, dimensions,
                             IPP_MAX(bit_depth_luma, bit_depth_chroma),
                             pFrame->GetColorFormat());
}

Status TaskSupplier::InitFreeFrame(H264DecoderFrame * pFrame, const H264Slice *pSlice)
{
    Status umcRes = UMC_OK;
    const H264SeqParamSet *pSeqParam = pSlice->GetSeqParam();

    pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);
    pFrame->m_dFrameTime = pSlice->m_dTime;
    pFrame->m_crop_left = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_left_offset;
    pFrame->m_crop_right = SubWidthC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_right_offset;
    pFrame->m_crop_top = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_top_offset * (2 - pSeqParam->frame_mbs_only_flag);
    pFrame->m_crop_bottom = SubHeightC[pSeqParam->chroma_format_idc] * pSeqParam->frame_cropping_rect_bottom_offset * (2 - pSeqParam->frame_mbs_only_flag);
    pFrame->m_crop_flag = pSeqParam->frame_cropping_flag;

    pFrame->setFrameNum(pSlice->GetSliceHeader()->frame_num);
    if (pSlice->GetSliceHeader()->nal_ext.extension_present && 
        0 == pSlice->GetSliceHeader()->nal_ext.svc_extension_flag)
    {
        pFrame->setViewId(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
    }

    pFrame->m_aspect_width  = pSeqParam->sar_width;
    pFrame->m_aspect_height = pSeqParam->sar_height;

    if (pSlice->GetSliceHeader()->field_pic_flag)
    {
        pFrame->m_bottom_field_flag[0] = pSlice->GetSliceHeader()->bottom_field_flag;
        pFrame->m_bottom_field_flag[1] = !pSlice->GetSliceHeader()->bottom_field_flag;

        pFrame->m_PictureStructureForRef =
        pFrame->m_PictureStructureForDec = FLD_STRUCTURE;
    }
    else
    {
        pFrame->m_bottom_field_flag[0] = 0;
        pFrame->m_bottom_field_flag[1] = 1;

        if (pSlice->GetSliceHeader()->MbaffFrameFlag)
        {
            pFrame->m_PictureStructureForRef =
            pFrame->m_PictureStructureForDec = AFRM_STRUCTURE;
        }
        else
        {
            pFrame->m_PictureStructureForRef =
            pFrame->m_PictureStructureForDec = FRM_STRUCTURE;
        }
    }

    Ipp32s iMBCount = pSeqParam->frame_width_in_mbs * pSeqParam->frame_height_in_mbs;
    pFrame->totalMBs = iMBCount;
    if (pFrame->m_PictureStructureForDec < FRM_STRUCTURE)
        pFrame->totalMBs /= 2;

    Ipp32s chroma_format_idc = pFrame->IsAuxiliaryFrame() ? 0 : pSeqParam->chroma_format_idc;

    Ipp8u bit_depth_luma, bit_depth_chroma;
    if (pFrame->IsAuxiliaryFrame())
    {
        bit_depth_luma = pSlice->GetSeqParamEx()->bit_depth_aux;
        bit_depth_chroma = 8;
    }
    else
    {
        bit_depth_luma = pSeqParam->bit_depth_luma;
        bit_depth_chroma = pSeqParam->bit_depth_chroma;
    }

    Ipp32s bit_depth = IPP_MAX(bit_depth_luma, bit_depth_chroma);

    Ipp32s iMBWidth = pSeqParam->frame_width_in_mbs;
    Ipp32s iMBHeight = pSeqParam->frame_height_in_mbs;
    IppiSize dimensions = {iMBWidth * 16, iMBHeight * 16};

    ColorFormat cf = GetUMCColorFormat(chroma_format_idc);

    if (cf == YUV420) //  msdk !!!
        cf = NV12;

    if (pSlice->GetSeqParamEx() && pSlice->GetSeqParamEx()->aux_format_idc)
    {
        cf = ConvertColorFormatToAlpha(cf);
    }

    VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, cf, bit_depth);

    pFrame->Init(&info);

    return umcRes;
}

Status TaskSupplier::AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat color_format)
{
    VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, color_format, bit_depth);

    FrameMemID frmMID;
    Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC_OK)
    {
        throw h264_exception(UMC_ERR_ALLOC);
    }

    const FrameData *frmData = m_pFrameAllocator->Lock(frmMID);

    if (!frmData)
        throw h264_exception(UMC_ERR_LOCK);

    pFrame->allocate(frmData, &info);
    pFrame->m_index = frmMID;

    Status umcRes = pFrame->allocateParsedFrameData();

    return umcRes;
}

void TaskSupplier::InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice)
{
    const H264SliceHeader *sliceHeader = pSlice->GetSliceHeader();
    ViewItem &view = GetView(sliceHeader->nal_ext.mvc.view_id);
    Ipp32s dId = sliceHeader->nal_ext.svc.dependency_id;
    Ipp32s index = GetDPBLayerIndex(pFrame->m_maxDId, dId);

    if (sliceHeader->IdrPicFlag)
    {
        view.GetPOCDecoder(index)->Reset(sliceHeader->frame_num);
    }

    view.GetPOCDecoder(index)->DecodePictureOrderCount(pSlice, sliceHeader->frame_num);

    pFrame->m_bIDRFlag = (sliceHeader->IdrPicFlag != 0);

    if (pFrame->m_bIDRFlag)
    {
        view.GetDPBList(index)->IncreaseRefPicListResetCount(pFrame);
    }

    pFrame->setFrameNum(sliceHeader->frame_num);

    Ipp32u field_index = pFrame->GetNumberByParity(sliceHeader->bottom_field_flag);

    if (sliceHeader->field_pic_flag == 0)
        pFrame->setPicNum(sliceHeader->frame_num, 0);
    else
        pFrame->setPicNum(sliceHeader->frame_num*2+1, field_index);

    view.GetPOCDecoder(index)->DecodePictureOrderCountInitFrame(pFrame, field_index);

    DEBUG_PRINT((VM_STRING("Init frame POC - %d, %d, field - %d, uid - %d, frame_num - %d, viewId - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], field_index, pFrame->m_UID, pFrame->m_FrameNum, pFrame->m_viewId));

    pFrame->SetInterViewRef(0 != pSlice->GetSliceHeader()->nal_ext.mvc.inter_view_flag, field_index);
    pFrame->InitRefPicListResetCount(field_index);

} // void TaskSupplier::InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice)

void TaskSupplier::AddSliceToFrame(H264DecoderFrame *pFrame, H264Slice *pSlice)
{
    const H264SliceHeader *sliceHeader = pSlice->GetSliceHeader();

    if (pFrame->m_FrameType < SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type))
        pFrame->m_FrameType = SliceTypeToFrameType(pSlice->GetSliceHeader()->slice_type);

    Ipp32u field_index = pFrame->GetNumberByParity(sliceHeader->bottom_field_flag);

    if (m_decodingMode == SVC_DECODING_MODE)
    {
        if (sliceHeader->nal_ext.svc.dependency_id < pFrame->m_maxDId)
            field_index = 0;
    }

    H264DecoderFrameInfo * au_info = pFrame->GetAU(field_index);
    Ipp32s iSliceNumber = au_info->GetSliceCount() + 1;

    if (field_index)
    {
        iSliceNumber += pFrame->m_TopSliceCount;
    }
    else
    {
        pFrame->m_TopSliceCount++;
    }

    pFrame->m_iNumberOfSlices++;

    pSlice->SetSliceNumber(iSliceNumber);
    pSlice->m_pCurrentFrame = pFrame;
    au_info->AddSlice(pSlice);
    InferredSliceParameterSVC(au_info, pSlice);
}

void TaskSupplier::DBPUpdate(H264DecoderFrame * pFrame, Ipp32s field)
{
    Ipp32s lastdId = -1;
    H264DecoderFrameInfo *slicesInfo = pFrame->GetAU(field);

    for (Ipp32u i = 0; i < slicesInfo->GetSliceCount(); i++)
    {
        H264Slice * slice = slicesInfo->GetSlice(i);
        Ipp32s dId = slice->GetSliceHeader()->nal_ext.svc.dependency_id;

        if (dId == lastdId)
            continue;

        lastdId = dId;

        if (!slice->IsReference())
            continue;

        ViewItem &view = GetView(slice->GetSliceHeader()->nal_ext.mvc.view_id);

        DecReferencePictureMarking::UpdateRefPicMarking(view, pFrame->m_pLayerFrames[dId], slice, field);

        if (GetAuxiliaryFrame(pFrame))
        {
            ViewItem &view = GetView(m_currentView);

            // store marking results to auxiliary frames
            Ipp32s index = GetDPBLayerIndex(pFrame->m_maxDId, dId);
            H264DecoderFrame *pHead = view.GetDPBList(index)->head();

            for (H264DecoderFrame *pTmp = pHead; pTmp; pTmp = pTmp->future())
            {
                H264DecoderFrameExtension * frame = DynamicCast<H264DecoderFrameExtension>(pTmp);
                VM_ASSERT(frame);
                frame->FillInfoToAuxiliary();
            }
        }
    }
}

H264DecoderFrame * TaskSupplier::FindSurface(FrameMemID id)
{
    AutomaticUMCMutex guard(m_mGuard);

    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    for (; iter != iter_end; ++iter)
    {
        H264DecoderFrame *pFrame = iter->GetDPBList(0)->head();
        for (; pFrame; pFrame = pFrame->future())
        {
            if (pFrame->GetFrameData()->GetFrameMID() == id)
                return pFrame;
        }
    }

    return 0;
}

Status TaskSupplier::RunDecoding_1()
{
    CompleteDecodedFrames(0);

    ViewList::iterator iter = m_views.begin();
    ViewList::iterator iter_end = m_views.end();

    H264DecoderFrame *pFrame = 0;
    for (; iter != iter_end; ++iter)
    {
        pFrame = iter->GetDPBList(0)->head();

        for (; pFrame; pFrame = pFrame->future())
        {
            if (!pFrame->IsDecodingCompleted())
            {
                break;
            }
        }

        if (pFrame)
            break;
    }

    m_pTaskBroker->Start();

    if (!pFrame)
        return UMC_OK;

    //DEBUG_PRINT((VM_STRING("Decode POC - %d\n"), pFrame->m_PicOrderCnt[0]));

    return UMC_OK;
}

Status TaskSupplier::RunDecoding(bool force, H264DecoderFrame ** )
{
    Status umcRes = UMC_OK;

    CompleteDecodedFrames(0);

    m_pTaskBroker->Start();

    if (!m_pTaskBroker->IsEnoughForStartDecoding(force))
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    umcRes = m_pSegmentDecoder[0]->ProcessSegment();

    CompleteDecodedFrames(0);
    if (umcRes == UMC_ERR_NOT_ENOUGH_DATA)
        return UMC_OK;

    return umcRes;
}

static ColorFormat color_formats[2][4] =
{
    {YUV420,
    NV12,
    YUV422,
    YUV444},

    {YUV420A,
    YUV420A,
    YUV422A,
    YUV444A},
};

void TaskSupplier::InitColorConverter(H264DecoderFrame *source, VideoData * videoData, Ipp8u force_field)
{
    if (!videoData || ! source)
        return;

    // set correct size in color converter
    Ipp32s crop_left   = source->m_crop_left;
    Ipp32s crop_right  = source->m_crop_right;
    Ipp32s crop_top    = source->m_crop_top;
    Ipp32s crop_bottom = source->m_crop_bottom;
    Ipp32s width  = source->lumaSize().width;
    Ipp32s height = source->lumaSize().height;

    if (force_field)
    {
        height >>= 1;
        crop_top >>= 1;
        crop_bottom >>= 1;
    }

    H264DecoderFrame * auxiliary = GetAuxiliaryFrame(source);

    if (videoData->GetColorFormat() != source->GetColorFormat() ||
        videoData->GetWidth() != width ||
        videoData->GetHeight() != height)
    {
        videoData->Init(width,
                        height,
                        source->GetColorFormat(),
                        source->m_bpp);
    }

    Ipp32s pixel_sz = source->m_bpp > 8 ? 2 : 1;


    if (source->m_pYPlane)
    {
        videoData->SetPlanePointer((void*)source->m_pYPlane, 0);
    }
    else
    {
        videoData->SetPlanePointer((void*)1, 0);
    }

    if (source->m_pUPlane)
    {
        videoData->SetPlanePointer((void*)source->m_pUPlane, 1);
        videoData->SetPlanePointer((void*)source->m_pVPlane, 2);
    }
    else
    {
        if (source->m_pUVPlane) // NV12
        {
            videoData->SetPlanePointer((void*)source->m_pUVPlane, 1);
        }
        else
        {
            videoData->SetPlanePointer((void*)1, 1);
            videoData->SetPlanePointer((void*)1, 2);
        }
    }

    Ipp32s pitch = source->pitch_luma()*pixel_sz;
    videoData->SetPlanePitch(pitch, 0);
    pitch = source->pitch_chroma()*pixel_sz;
    videoData->SetPlanePitch(pitch, 1);
    videoData->SetPlanePitch(pitch, 2);

    if (auxiliary)
    {
        Ipp32s pixel_size = auxiliary->m_bpp > 8 ? 2 : 1;
        Ipp32s step = auxiliary->pitch_luma()*pixel_size;
        videoData->SetPlanePointer((void*)auxiliary->m_pYPlane, 3);
        videoData->SetPlanePitch(step, 3);
    }

    switch (source->m_displayPictureStruct)
    {
    case DPS_BOTTOM:
    case DPS_BOTTOM_TOP:
    case DPS_BOTTOM_TOP_BOTTOM:
        videoData->SetPictureStructure(PS_BOTTOM_FIELD_FIRST);
        break;
    case DPS_TOP_BOTTOM:
    case DPS_TOP:
    case DPS_TOP_BOTTOM_TOP:
        videoData->SetPictureStructure(PS_TOP_FIELD_FIRST);
        break;

    case DPS_FRAME:
    case DPS_FRAME_DOUBLING:
    case DPS_FRAME_TRIPLING:
    default:
        videoData->SetPictureStructure(PS_FRAME);
        break;
    }

    videoData->SetDataSize(videoData->GetMappingSize());
    videoData->SetFrameType(source->m_FrameType);
    videoData->SetAspectRatio(source->m_aspect_width, source->m_aspect_height);

    m_LastNonCropDecodedFrame = *videoData;

    if (crop_left | crop_right | crop_top | crop_bottom)
    {
        UMC::sRECT SrcCropArea;
        SrcCropArea.left = (Ipp16s) (crop_left);
        SrcCropArea.top  = (Ipp16s) (crop_top);
        SrcCropArea.right = (Ipp16s) (width - crop_right);
        SrcCropArea.bottom = (Ipp16s) (height - crop_bottom);

        videoData->Crop(SrcCropArea);
    }

    videoData->SetTime(source->m_dFrameTime);
}

Status TaskSupplier::GetUserData(MediaData * pUD)
{
    if(!pUD)
        return UMC_ERR_NULL_PTR;

    if (!m_pLastDisplayed)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (m_pLastDisplayed->m_UserData.user_data.size() && m_pLastDisplayed->m_UserData.payLoadSize &&
        m_pLastDisplayed->m_UserData.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
    {
        pUD->SetTime(m_pLastDisplayed->m_dFrameTime);
        pUD->SetBufferPointer(&m_pLastDisplayed->m_UserData.user_data[0],
            m_pLastDisplayed->m_UserData.payLoadSize);
        pUD->SetDataSize(m_pLastDisplayed->m_UserData.payLoadSize);
        //m_pLastDisplayed->m_UserData.Reset();
        return UMC_OK;
    }

    return UMC_ERR_NOT_ENOUGH_DATA;
}

bool TaskSupplier::IsShouldSuspendDisplay()
{
    AutomaticUMCMutex guard(m_mGuard);

    if (m_iThreadNum <= 1)
        return true;

    for (Ipp32s i = 0; i < GetViewCount(); i++)
    {
        ViewItem &view = GetViewByNumber(i);

        if (view.GetDPBList(0)->GetDisposable() || view.GetDPBList(0)->countAllFrames() < view.dpbSize + m_DPBSizeEx)
            return false;
    }

    return true;
}

void TaskSupplier::ApplyPayloadsToFrame(H264DecoderFrame * frame, H264Slice *slice, SeiPayloadArray * payloads)
{
    if (!payloads || !frame)
        return;

    if (m_sei_messages)
    {
        m_sei_messages->SetFrame(frame, frame->m_auIndex);
    }

    H264SEIPayLoad * payload = payloads->FindPayload(SEI_PIC_TIMING_TYPE);

    if (frame->m_displayPictureStruct == DPS_UNKNOWN)
    {
        if (payload && slice->GetSeqParam()->pic_struct_present_flag)
        {
            frame->m_displayPictureStruct = payload->SEI_messages.pic_timing.pic_struct;
        }
        else
        {
            if (frame->m_PictureStructureForDec == FLD_STRUCTURE)
            {
                frame->m_displayPictureStruct = frame->GetNumberByParity(0) ? DPS_BOTTOM : DPS_TOP;
            }
            else
            {
                if (frame->PicOrderCnt(0, 1) == frame->PicOrderCnt(1, 1))
                {
                    frame->m_displayPictureStruct = DPS_FRAME;
                }
                else
                {
                    frame->m_displayPictureStruct = (frame->PicOrderCnt(0, 1) < frame->PicOrderCnt(1, 1)) ? DPS_TOP_BOTTOM : DPS_BOTTOM_TOP;
                }
            }
        }
    }

    frame->m_dpb_output_delay = DPBOutput::GetDPBOutputDelay(payload);

    payload = payloads->FindPayload(SEI_DEC_REF_PIC_MARKING_TYPE);
    if (payload)
    {
        ViewItem &view = GetViewByNumber(BASE_VIEW);
        DecReferencePictureMarking::CheckSEIRepetition(view, payload);
    }
}

H264DecoderFrame * TaskSupplier::AllocateNewFrame(const H264Slice *slice)
{
    H264DecoderFrame *pFrame = 0;

    m_currentView = slice->GetSliceHeader()->nal_ext.mvc.view_id;
    ViewItem &view = GetView(m_currentView);

    if (view.pCurFrame && view.pCurFrame->m_PictureStructureForDec < FRM_STRUCTURE)
    {
        H264Slice * pFirstFrameSlice = view.pCurFrame->GetAU(0)->GetSlice(0);
        if (pFirstFrameSlice && IsFieldOfOneFrame(view.pCurFrame, pFirstFrameSlice, slice))
        {
            pFrame = view.pCurFrame;
            InitFrameCounter(pFrame, slice);
        }
        else
        {
            //ProcessNonPairedField(view.pCurFrame);
            //OnFullFrame(view.pCurFrame);
        }

        view.pCurFrame = 0;
    }

    if (pFrame)
        return pFrame;

    if (!slice)
    {
        return NULL;
    }

    if (slice->IsAuxiliary())
    {
        VM_ASSERT(view.pCurFrame && !view.pCurFrame->IsAuxiliaryFrame());
        ((H264DecoderFrameExtension *)view.pCurFrame)->AllocateAuxiliary();
        ((H264DecoderFrameExtension *)view.pCurFrame)->FillInfoToAuxiliary();
        pFrame = UMC::GetAuxiliaryFrame(view.pCurFrame);
    }
    else
    {
        pFrame = GetFreeFrame(slice);
    }

    if (NULL == pFrame)
    {
        return NULL;
    }

    pFrame->m_Flags.isActive = 1;

    Status umcRes = InitFreeFrame(pFrame, slice);
    if (umcRes != UMC_OK)
    {
        return 0;
    }

    if (m_decodingMode != SVC_DECODING_MODE)
    {
        umcRes = AllocateFrameData(pFrame, pFrame->lumaSize(), pFrame->m_bpp, pFrame->GetColorFormat());
        if (umcRes != UMC_OK)
        {
            return 0;
        }
    }

    if (slice->IsAuxiliary())
    {
        for (H264DecoderFrame *pTmp = view.GetDPBList(0)->head(); pTmp; pTmp = pTmp->future())
        {
            H264DecoderFrameExtension * frame = DynamicCast<H264DecoderFrameExtension>(pTmp);
            VM_ASSERT(frame);
            frame->FillInfoToAuxiliary();
        }
    }

    if (slice->IsField())
    {
        pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_NOT_FILLED);
    }

    //fill chroma planes in case of 4:0:0
    if (pFrame->m_chroma_format == 0)
    {
        pFrame->DefaultFill(2, true);
    }

    if (m_decodingMode != SVC_DECODING_MODE)
        InitFrameCounter(pFrame, slice);

    return pFrame;
} // H264DecoderFrame * TaskSupplier::AddFrame(H264Slice *pSlice)


H264DecoderFrame * TaskSupplier::AddLayerFrameSVC(H264DBPList *dpb, H264Slice *pSlice)
{
    if (!pSlice)
        return 0;

    // Traverse list for next disposable frame
    H264DecoderFrame *pFrame = dpb->GetOldestDisposable();

    // Did we find one?
    if (NULL == pFrame)
    {
        if (dpb->countAllFrames() >= dpb->GetDPBSize() + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        dpb->append(pFrame);
    }

    pFrame->Reset();

    pFrame->setWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->IncrementReference();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);

    InitFreeFrame(pFrame, pSlice);

    /* is needed for Direct prediction */
    //if (dId == 0)
    {
        const H264SeqParamSet *pSeqParam = pSlice->GetSeqParam();
        Ipp32s iMBWidth = pSeqParam->frame_width_in_mbs;
        Ipp32s iMBHeight = pSeqParam->frame_height_in_mbs;
        Ipp32s chroma_format = pSeqParam->chroma_format_idc;
        IppiSize dimensions;

        dimensions.width = iMBWidth * 16;
        dimensions.height = iMBHeight * 16;

        pFrame->setImageSize(dimensions, chroma_format);

        if (UMC_OK != pFrame->allocateParsedFrameData())
            return 0;
    }

    return pFrame;
}


H264DecoderLayerResizeParameter::H264DecoderLayerResizeParameter()
{
    Reset();
}

void H264DecoderLayerResizeParameter::Reset()
{
    extended_spatial_scalability = 0;
    scaled_ref_layer_width = 0;
    scaled_ref_layer_height = 0;
    frame_mbs_only_flag = 0;

    cur_layer_width = 0;
    cur_layer_height = 0;

    leftOffset = 0;
    topOffset = 0;
    rightOffset = 0;
    bottomOffset = 0;

    phaseX = 0;
    phaseY = 0;
    refPhaseX = 0;
    refPhaseY = 0;

    spatial_resolution_change = 0;
    next_spatial_resolution_change = 0;

    level_idc = 0;

    shiftX = 0;
    shiftY = 0;
    scaleX = 0;
    scaleY = 0;
    addX = 0;
    addY = 0;

    ref_layer_width = 0;
    ref_layer_height = 0;

    ref_frame_mbs_only_flag = 0;
    field_pic_flag = 0;
    RefLayerFieldPicFlag = 0;

    MbaffFrameFlag = 0;
    RefLayerMbaffFrameFlag = 0;
}

void H264DecoderLayerResizeParameter::UpdateRefLayerParams(H264Slice *refSlice)
{
    const H264SeqParamSet *sps = refSlice->GetSeqParam();

    ref_layer_width = sps->frame_width_in_mbs << 4;
    ref_layer_height = sps->frame_height_in_mbs << 4;

    ref_frame_mbs_only_flag = refSlice->GetSeqParam()->frame_mbs_only_flag;
    RefLayerFieldPicFlag = refSlice->GetSliceHeader()->field_pic_flag;
    RefLayerMbaffFrameFlag = refSlice->GetSliceHeader()->MbaffFrameFlag;
}

void H264DecoderLayerResizeParameter::UpdateLayerParams(H264Slice *slice)
{
    const H264SeqParamSetSVCExtension *curSps = slice->GetSeqSVCParam();
    const H264SliceHeader *sliceHeader = slice->GetSliceHeader();

    sliceHeader = slice->m_firstILPredSliceInDepLayer ? slice->m_firstILPredSliceInDepLayer->GetSliceHeader() : sliceHeader;

    if (curSps->chroma_format_idc > 0 && (curSps->chroma_format_idc != 3 || curSps->residual_colour_transform_flag == 0))
    {
        phaseX = curSps->chroma_phase_x;
        phaseY = curSps->chroma_phase_y;
    } else {
        phaseX = 0;
        phaseY = -1;
    }

    if (sliceHeader->nal_ext.svc.quality_id || sliceHeader->nal_ext.svc.no_inter_layer_pred_flag)
    {
        extended_spatial_scalability = 0;
        leftOffset = 0;
        topOffset = 0;
        rightOffset = 0;
        bottomOffset = 0;
        refPhaseX = phaseX;
        refPhaseY = phaseY;
    }
    else
    {
        extended_spatial_scalability = curSps->extended_spatial_scalability;

        if (curSps->extended_spatial_scalability == 1) {
            leftOffset = 2 * curSps->seq_scaled_ref_layer_left_offset;
            topOffset = 2 * curSps->seq_scaled_ref_layer_top_offset *
                (2 - curSps->frame_mbs_only_flag);

            rightOffset = 2 * curSps->seq_scaled_ref_layer_right_offset;
            bottomOffset = 2 * curSps->seq_scaled_ref_layer_bottom_offset *
                (2 - curSps->frame_mbs_only_flag);

            refPhaseX = curSps->seq_ref_layer_chroma_phase_x;
            refPhaseY = curSps->seq_ref_layer_chroma_phase_y;
        } else if (curSps->extended_spatial_scalability == 2) {
            leftOffset = 2 * sliceHeader->scaled_ref_layer_left_offset;
            topOffset = 2 * sliceHeader->scaled_ref_layer_top_offset *
                (2 - curSps->frame_mbs_only_flag);

            rightOffset = 2 * sliceHeader->scaled_ref_layer_right_offset;
            bottomOffset = 2 * sliceHeader->scaled_ref_layer_bottom_offset *
                (2 - curSps->frame_mbs_only_flag);

            refPhaseX = sliceHeader->ref_layer_chroma_phase_x;
            refPhaseY = sliceHeader->ref_layer_chroma_phase_y;
        } else {
            leftOffset = 0;
            topOffset = 0;
            rightOffset = 0;
            bottomOffset = 0;
            refPhaseX = curSps->chroma_phase_x;
            refPhaseY = curSps->chroma_phase_y;
        }
    }

    scaled_ref_layer_width = (curSps->frame_width_in_mbs << 4) - leftOffset - rightOffset;
    scaled_ref_layer_height = (curSps->frame_height_in_mbs << 4) - topOffset - bottomOffset;

    cur_layer_width = (curSps->frame_width_in_mbs << 4);
    cur_layer_height = (curSps->frame_height_in_mbs << 4);

    field_pic_flag = sliceHeader->field_pic_flag;
    frame_mbs_only_flag = curSps->frame_mbs_only_flag;
    MbaffFrameFlag = sliceHeader->MbaffFrameFlag;

    level_idc = curSps->level_idc;
}

void H264DecoderLayerResizeParameter::EvaluateSpatialResolutionChange()
{
    spatial_resolution_change = 0;

    if (scaled_ref_layer_width != ref_layer_width ||
        scaled_ref_layer_height != ref_layer_height ||
        field_pic_flag != RefLayerFieldPicFlag ||
        MbaffFrameFlag != RefLayerMbaffFrameFlag ||
        (leftOffset & 0xf) != 0 ||
        (topOffset & (0xf * (1 + field_pic_flag + MbaffFrameFlag))) != 0 ||
        refPhaseX != phaseX ||
        refPhaseY != phaseY ||
        extended_spatial_scalability == 2)
    {
        spatial_resolution_change = 1;
    }

    Ipp32u temp_scaled_ref_layer_height = scaled_ref_layer_height >> field_pic_flag;
    Ipp32u temp_ref_layer_height = ref_layer_height >> RefLayerFieldPicFlag;

    shiftX = ((level_idc <= 30) ? 16 : countShift(ref_layer_width));
    shiftY = ((level_idc <= 30) ? 16 : countShift(temp_ref_layer_height));
    scaleX = (Ipp32s)((((Ipp32u)ref_layer_width << shiftX ) +
        ((Ipp32u)scaled_ref_layer_width >> 1 )) /
        (Ipp32u)scaled_ref_layer_width);

    scaleY = (Ipp32s)((((Ipp32u)temp_ref_layer_height << shiftY ) +
        ((Ipp32u)temp_scaled_ref_layer_height >> 1 )) /
        (Ipp32u)temp_scaled_ref_layer_height);

    addX = (1 << (shiftX - 1)) - leftOffset * scaleX;
    addY = (1 << (shiftY - 1)) - topOffset * scaleY;
}

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
