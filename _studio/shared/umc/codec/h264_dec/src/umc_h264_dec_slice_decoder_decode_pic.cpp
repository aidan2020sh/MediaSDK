/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2012 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

/*  DEBUG: this file is requred to changing name
    to umc_h264_dec_slice_decode_pic */

#include "umc_h264_slice_decoding.h"
#include "umc_h264_dec.h"
#include "vm_debug.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_task_supplier.h"

//#define ENABLE_LIST_TRACE
#define LIST_TRACE_FILE_NAME "d:/ipp.ref"

namespace UMC
{

static H264DecoderFrame *FindLastValidReference(H264DecoderFrame **pList, Ipp32s iLength)
{
    Ipp32s i;
    H264DecoderFrame *pLast = NULL;

    for (i = 0; i < iLength; i += 1)
    {
        if (pList[i])
            pLast = pList[i];
    }

    return pLast;

} // H264DecoderFrame *FindLastValidReference(H264DecoderFrame **pList,


FactorArrayAFF H264Slice::m_StaticFactorArrayAFF;

Status H264Slice::UpdateReferenceList(ViewList &views,
                                      Ipp32s dIdIndex)
{
    Status ps = UMC_OK;
    RefPicListReorderInfo *pReorderInfo_L0 = &ReorderInfoL0;
    RefPicListReorderInfo *pReorderInfo_L1 = &ReorderInfoL1;
    const H264SeqParamSet *sps = GetSeqParam();
    Ipp32u uMaxFrameNum;
    Ipp32u uMaxPicNum;
    H264DecoderFrame *pFrm;
    H264DBPList *pDecoderFrameList = GetDPB(views, m_SliceHeader.nal_ext.mvc.view_id, dIdIndex);
    H264DecoderFrame *pHead = pDecoderFrameList->head();
    //Ipp32u i;
    H264DecoderFrame **pRefPicList0;
    H264DecoderFrame **pRefPicList1;
    ReferenceFlags *pFields0;
    ReferenceFlags *pFields1;
    Ipp32u NumShortTermRefs, NumLongTermRefs;
    H264RefListInfo rli;
    H264DecoderFrame *(pLastInList[2]) = {NULL, NULL};

    if (m_SliceHeader.is_auxiliary)
    {
        pHead = GetAuxiliaryFrame(pHead);
    }

    VM_ASSERT(m_pCurrentFrame);

    pRefPicList0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_RefPicList;
    pRefPicList1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_RefPicList;
    pFields0 = m_pCurrentFrame->GetRefPicList(m_iNumber, 0)->m_Flags;
    pFields1 = m_pCurrentFrame->GetRefPicList(m_iNumber, 1)->m_Flags;

    // Spec reference: 8.2.4, "Decoding process for reference picture lists
    // construction"

    // get pointers to the picture and sequence parameter sets currently in use
    uMaxFrameNum = (1<<sps->log2_max_frame_num);
    uMaxPicNum = (m_SliceHeader.field_pic_flag == 0) ? uMaxFrameNum : uMaxFrameNum<<1;

    for (pFrm = pHead; pFrm; pFrm = pFrm->future())
    {
        // update FrameNumWrap and picNum if frame number wrap occurred,
        // for short-term frames
        // TBD: modify for fields
        pFrm->UpdateFrameNumWrap((Ipp32s)m_SliceHeader.frame_num,
            uMaxFrameNum,
            m_pCurrentFrame->m_PictureStructureForDec +
            m_SliceHeader.bottom_field_flag);

        // For long-term references, update LongTermPicNum. Note this
        // could be done when LongTermFrameIdx is set, but this would
        // only work for frames, not fields.
        // TBD: modify for fields
        pFrm->UpdateLongTermPicNum(m_pCurrentFrame->m_PictureStructureForDec +
                                   m_SliceHeader.bottom_field_flag);
    }

    for (Ipp32s number = 0; number <= MAX_NUM_REF_FRAMES + 1; number++)
    {
        pRefPicList0[number] = 0;
        pRefPicList1[number] = 0;
        pFields0[number].field = 0;
        pFields0[number].isShortReference = 0;
        pFields1[number].field = 0;
        pFields1[number].isShortReference = 0;
    }

    if ((m_SliceHeader.slice_type != INTRASLICE) && (m_SliceHeader.slice_type != S_INTRASLICE))
    {
        Ipp32u NumInterViewRefs = 0;

        // Detect and report no available reference frames
        pDecoderFrameList->countActiveRefs(NumShortTermRefs, NumLongTermRefs);
        // calculate number of inter-view references
        if (m_SliceHeader.nal_ext.mvc.view_id != views.front().viewId)
        {
            NumInterViewRefs = GetInterViewFrameRefs(views, m_SliceHeader.nal_ext.mvc.view_id, m_pCurrentFrame->m_auIndex, m_SliceHeader.bottom_field_flag);
        }

        if ((NumShortTermRefs + NumLongTermRefs + NumInterViewRefs) == 0)
        {
            VM_ASSERT(0);
            ps = UMC_ERR_INVALID_STREAM;
        }

        if (ps == UMC_OK)
        {
#ifdef ENABLE_LIST_TRACE
            static int kkkk = 0;
            kkkk++;
#endif

            // Initialize the reference picture lists
            // Note the slice header get function always fills num_ref_idx_lx_active
            // fields with a valid value; either the override from the slice
            // header in the bitstream or the values from the pic param set when
            // there is no override.

            if (!m_SliceHeader.IdrPicFlag)
            {
                if ((m_SliceHeader.slice_type == PREDSLICE) ||
                    (m_SliceHeader.slice_type == S_PREDSLICE))
                {
                    pDecoderFrameList->InitPSliceRefPicList(this, pRefPicList0);
                    pLastInList[0] = FindLastValidReference(pRefPicList0,
                                                            m_SliceHeader.num_ref_idx_l0_active);
                }
                else
                {
                    pDecoderFrameList->InitBSliceRefPicLists(this, pRefPicList0, pRefPicList1, rli);
                    pLastInList[0] = FindLastValidReference(pRefPicList0,
                                                            m_SliceHeader.num_ref_idx_l0_active);
                    pLastInList[1] = FindLastValidReference(pRefPicList1,
                                                            m_SliceHeader.num_ref_idx_l1_active);
                }

                // Reorder the reference picture lists
                if (m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                {
                    rli.m_iNumFramesInL0List = AdjustRefPicListForFields(pRefPicList0, pFields0, rli);
                }

                if (BPREDSLICE == m_SliceHeader.slice_type)
                {
                    if (m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                    {
                        rli.m_iNumFramesInL1List = AdjustRefPicListForFields(pRefPicList1, pFields1, rli);
                    }

                    if ((rli.m_iNumFramesInL0List == rli.m_iNumFramesInL1List) &&
                        (rli.m_iNumFramesInL0List > 1))
                    {
                        bool isNeedSwap = true;
                        for (Ipp32s i = 0; i < rli.m_iNumFramesInL0List; i++)
                        {
                            if (pRefPicList1[i] != pRefPicList0[i] ||
                                pFields1[i].field != pFields0[i].field)
                            {
                                isNeedSwap = false;
                                break;
                            }
                        }

                        if (isNeedSwap)
                        {
                            swapValues(pRefPicList1[0], pRefPicList1[1]);
                            swapValues(pFields1[0], pFields1[1]);
                        }
                    }
                }
            }

            // MVC extension. Add inter-view references
            if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == m_pSeqParamSet->profile_idc) ||
                (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == m_pSeqParamSet->profile_idc))
            {
                pDecoderFrameList->AddInterViewRefs(this, pRefPicList0, pFields0, LIST_0, views);
                if (m_SliceHeader.IdrPicFlag)
                {
                    pLastInList[0] = FindLastValidReference(pRefPicList0, m_SliceHeader.num_ref_idx_l0_active);
                }

                if (BPREDSLICE == m_SliceHeader.slice_type)
                {
                    pDecoderFrameList->AddInterViewRefs(this, pRefPicList1, pFields1, LIST_1, views);
                    if (m_SliceHeader.IdrPicFlag)
                        pLastInList[1] = FindLastValidReference(pRefPicList1, m_SliceHeader.num_ref_idx_l1_active);
                }
            }

            pRefPicList0[m_SliceHeader.num_ref_idx_l0_active] = 0;

#ifdef ENABLE_LIST_TRACE
            {
            FILE  * fl = fopen(LIST_TRACE_FILE_NAME, "a+");
            fprintf(fl, "init - l0 - %d\n", kkkk);
            for (Ipp32s k1 = 0; k1 < m_SliceHeader.num_ref_idx_l0_active; k1++)
            {
                if (!pRefPicList0[k1])
                    break;
                fprintf(fl, "i - %d, field - %d, poc %d \n", k1, pFields0[k1].field, pRefPicList0[k1]->PicOrderCnt(pRefPicList0[k1]->GetNumberByParity(pFields0[k1].field)));
            }

            fprintf(fl, "l1 - %d\n", kkkk);
            for (Ipp32s  k1 = 0; k1 < m_SliceHeader.num_ref_idx_l1_active; k1++)
            {
                if (!pRefPicList1[k1])
                    break;
                fprintf(fl, "i - %d, field - %d, poc %d \n", k1, pFields1[k1].field, pRefPicList1[k1]->PicOrderCnt(pRefPicList1[k1]->GetNumberByParity(pFields1[k1].field)));
            }

            fclose(fl);
            }
#endif

            if (pReorderInfo_L0->num_entries > 0)
                ReOrderRefPicList(pRefPicList0, pFields0, pReorderInfo_L0, uMaxPicNum, views, dIdIndex, 0);

            if (BPREDSLICE == m_SliceHeader.slice_type)
            {
                pRefPicList1[m_SliceHeader.num_ref_idx_l1_active] = 0;

                if (pReorderInfo_L1->num_entries > 0)
                    ReOrderRefPicList(pRefPicList1, pFields1, pReorderInfo_L1, uMaxPicNum, views, dIdIndex, 1);
            }


#ifdef ENABLE_LIST_TRACE
            {kkkk++;
            FILE  * fl = fopen(LIST_TRACE_FILE_NAME, "a+");
            fprintf(fl, "reorder - l0 - %d\n", kkkk);
            for (Ipp32s k1 = 0; k1 < m_SliceHeader.num_ref_idx_l0_active; k1++)
            {
                if (!pRefPicList0[k1])
                    break;
                fprintf(fl, "i - %d, field - %d, poc %d \n", k1, pFields0[k1].field, pRefPicList0[k1]->PicOrderCnt(pRefPicList0[k1]->GetNumberByParity(pFields0[k1].field)));
            }

            fprintf(fl, "l1 - %d\n", kkkk);
            for (Ipp32s  k1 = 0; k1 < m_SliceHeader.num_ref_idx_l1_active; k1++)
            {
                if (!pRefPicList1[k1])
                    break;
                fprintf(fl, "i - %d, field - %d, poc %d \n", k1, pFields1[k1].field, pRefPicList1[k1]->PicOrderCnt(pRefPicList1[k1]->GetNumberByParity(pFields1[k1].field)));
            }

            fclose(fl);
            }
#endif

            // set absent references
            {
                Ipp32s i;
                Ipp32s iCurField = 1;

                if (!pLastInList[0])
                {
                    for (;;)
                    {
                        for (H264DecoderFrame *pFrm = pDecoderFrameList->head(); pFrm; pFrm = pFrm->future())
                        {
                            if (pFrm->isShortTermRef()==3)
                                pLastInList[0] = pFrm;
                        }

                        if (pLastInList[0])
                            break;

                        for (H264DecoderFrame *pFrm = pDecoderFrameList->head(); pFrm; pFrm = pFrm->future())
                        {
                            if (pFrm->isLongTermRef()==3)
                                pLastInList[0] = pFrm;
                        }
                        break;
                    }
                }

                if (BPREDSLICE == m_SliceHeader.slice_type && !pLastInList[1])
                {
                    pLastInList[1] = pLastInList[0];
                }

                for (i = 0; i < m_SliceHeader.num_ref_idx_l0_active; i += 1)
                {
                    if (NULL == pRefPicList0[i])
                    {
                        pRefPicList0[i] = pLastInList[0];
                        pFields0[i].field = (Ipp8s) (iCurField ^= 1);
                        if (pRefPicList0[i])
                            pFields0[i].isShortReference = (pRefPicList0[i]->m_viewId == m_pCurrentFrame->m_viewId) ? 1 : 0;
                        else
                            pFields0[i].isShortReference = 1;
                        pFields0[i].isLongReference = 0;
                        m_pCurrentFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                    }
                    else
                    {
                        m_pCurrentFrame->AddReference(pRefPicList0[i]);
                        m_pCurrentFrame->SetErrorFlagged(pRefPicList0[i]->IsFrameExist() ? 0 : ERROR_FRAME_MAJOR);
                        pFields0[i].isShortReference =
                            pRefPicList0[i]->isShortTermRef(pRefPicList0[i]->GetNumberByParity(pFields0[i].field));
                        pFields0[i].isLongReference =
                            pRefPicList0[i]->isLongTermRef(pRefPicList0[i]->GetNumberByParity(pFields0[i].field));

                        if (pRefPicList0[i]->m_viewId != m_pCurrentFrame->m_viewId)
                            pFields0[i].isShortReference = pFields0[i].isLongReference = 0;
                    }
                }

                if (BPREDSLICE == m_SliceHeader.slice_type)
                {
                    iCurField = 1;

                    for (i = 0; i < m_SliceHeader.num_ref_idx_l1_active; i += 1)
                    {
                        if (NULL == pRefPicList1[i])
                        {
                            pRefPicList1[i] = pLastInList[1];
                            pFields1[i].field = (Ipp8s) (iCurField ^= 1);
                            pFields1[i].isShortReference = 1;
                            if (pRefPicList1[i])
                                pFields1[i].isShortReference = (pRefPicList1[i]->m_viewId == m_pCurrentFrame->m_viewId) ? 1 : 0;
                            else
                                pFields1[i].isShortReference = 1;
                            pFields1[i].isLongReference = 0;
                            m_pCurrentFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);
                        }
                        else
                        {
                            m_pCurrentFrame->AddReference(pRefPicList1[i]);
                            m_pCurrentFrame->SetErrorFlagged(pRefPicList1[i]->IsFrameExist() ? 0 : ERROR_FRAME_MAJOR);
                            pFields1[i].isShortReference =
                                pRefPicList1[i]->isShortTermRef(pRefPicList1[i]->GetNumberByParity(pFields1[i].field));
                            pFields1[i].isLongReference =
                                pRefPicList1[i]->isLongTermRef(pRefPicList1[i]->GetNumberByParity(pFields1[i].field));

                            if (pRefPicList1[i]->m_viewId != m_pCurrentFrame->m_viewId)
                                pFields1[i].isShortReference = pFields1[i].isLongReference = 0;
                        }
                    }
                }
            }

            // If B slice, init scaling factors array
            if ((BPREDSLICE == m_SliceHeader.slice_type) && (pRefPicList1[0] != NULL))
            {
                InitDistScaleFactor(m_SliceHeader.num_ref_idx_l0_active,
                                    m_SliceHeader.num_ref_idx_l1_active,
                                    pRefPicList0, pRefPicList1, pFields0, pFields1);
            }
        }
    }

    return ps;

} // Status H264Slice::UpdateRefPicList(H264DecoderFrameList *pDecoderFrameList)

#define CalculateDSF(index, value, value_mv)                                                     \
        /* compute scaling ratio for temporal direct and implicit weighting*/   \
        tb = picCntCur - picCntRef0;    /* distance from previous */            \
        td = picCntRef1 - picCntRef0;    /* distance between ref0 and ref1 */   \
                                                                                \
        /* special rule: if td is 0 or if L0 is long-term reference, use */     \
        /* L0 motion vectors and equal weighting.*/                             \
        if (td == 0 || pRefPicList0[index]->isLongTermRef(pRefPicList0[index]->GetNumberByParity(RefFieldTop)))  \
        {                                                                       \
            /* These values can be used directly in scaling calculations */     \
            /* to get back L0 or can use conditional test to choose L0.    */   \
            value = 128;    /* for equal weighting    */    \
            value_mv = 256;                                  \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            tb = IPP_MAX(-128,tb);                                              \
            tb = IPP_MIN(127,tb);                                               \
            td = IPP_MAX(-128,td);                                              \
            td = IPP_MIN(127,td);                                               \
                                                                                \
            VM_ASSERT(td != 0);                                                 \
                                                                                \
            tx = (16384 + abs(td/2))/td;                                        \
                                                                                \
            DistScaleFactor = (tb*tx + 32)>>6;                                  \
            DistScaleFactor = IPP_MAX(-1024, DistScaleFactor);                  \
            DistScaleFactor = IPP_MIN(1023, DistScaleFactor);                   \
                                                                                \
            if (isL1LongTerm || DistScaleFactor < -256 || DistScaleFactor > 512)                \
                value = 128;    /* equal weighting     */   \
            else                                                                \
                value = (FactorArrayValue)DistScaleFactor;                    \
                                                                                \
            value_mv = (FactorArrayValue)DistScaleFactor;                      \
        }

void H264Slice::InitDistScaleFactor(Ipp32s NumL0RefActive,
                                    Ipp32s NumL1RefActive,
                                    H264DecoderFrame **pRefPicList0,
                                    H264DecoderFrame **pRefPicList1,
                                    ReferenceFlags *pFields0,
                                    ReferenceFlags *pFields1)

{
    Ipp32s L0Index, L1Index;
    Ipp32s picCntRef0;
    Ipp32s picCntRef1;
    Ipp32s picCntCur;
    Ipp32s DistScaleFactor;
    FactorArrayValue *pDistScaleFactor;
    FactorArrayValue *pDistScaleFactorMV;

    Ipp32s tb;
    Ipp32s td;
    Ipp32s tx;

    VM_ASSERT(NumL0RefActive <= MAX_NUM_REF_FRAMES);
    VM_ASSERT(pRefPicList1[0]);

    bool isL1LongTerm = false;

    picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(m_SliceHeader.bottom_field_flag));

    bool isNeedScale = (m_pPicParamSet->weighted_bipred_idc == 2);

    if (!isNeedScale)
        NumL1RefActive = 1;

    for (L1Index = NumL1RefActive - 1; L1Index >=0 ; L1Index--)
    {
        pDistScaleFactor = m_DistScaleFactor.values[L1Index];        //frames or fields
        pDistScaleFactorMV = m_DistScaleFactorMV.values;  //frames or fields

        Ipp32s RefField = m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE ?
            0 : GetReferenceField(pFields1, L1Index);

        picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(RefField));
        isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(RefField));

        for (L0Index = 0; L0Index < NumL0RefActive; L0Index++)
        {
            Ipp32s RefFieldTop = (m_pCurrentFrame->m_PictureStructureForDec >= FRM_STRUCTURE) ?
                0 : GetReferenceField(pFields0, L0Index);
            picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(RefFieldTop));

            CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
        }
    }

    if (m_pCurrentFrame->m_PictureStructureForDec == AFRM_STRUCTURE)
    {
        if (!m_DistScaleFactorAFF && isNeedScale)
            m_DistScaleFactorAFF = (FactorArrayAFF*)m_pObjHeap->Allocate(sizeof(FactorArrayAFF));

        if (!m_DistScaleFactorMVAFF)
            m_DistScaleFactorMVAFF = (FactorArrayMVAFF*)m_pObjHeap->Allocate(sizeof(FactorArrayMVAFF));

        if (!isNeedScale)
        {
            m_DistScaleFactorAFF = &m_StaticFactorArrayAFF;
        }

        for (L1Index = NumL1RefActive - 1; L1Index >=0 ; L1Index--)
        {
            // [curmb field],[ref1field],[ref0field]
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][0][0];      //complementary field pairs, cf=top r1=top,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][0][0];  //complementary field pairs, cf=top r1=top,r0=top

            picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(0), 1);
            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(0), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(0));

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][1][0];        //complementary field pairs, cf=top r1=top,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][1][0];  //complementary field pairs, cf=top r1=top,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);
                Ipp32s RefFieldTop = 1;

                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][0][1];        //complementary field pairs, cf=top r1=top,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][0][1];  //complementary field pairs, cf=top r1=top,r0=top

            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(1), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(1));

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][0][1][1];        //complementary field pairs, cf=top r1=top,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[0][1][1];  //complementary field pairs, cf=top r1=top,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);
                Ipp32s RefFieldTop = 1;

                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1),1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            /*--------------------------------------------------------------------*/
            picCntCur = m_pCurrentFrame->PicOrderCnt(m_pCurrentFrame->GetNumberByParity(1), 1);
            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(0), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(0));

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][0][0];        //complementary field pairs, cf=bottom r1=bottom,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][0][0];  //complementary field pairs, cf=bottom r1=bottom,r0=top

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][1][0];       //complementary field pairs, cf=bottom r1=bottom,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][1][0];   //complementary field pairs, cf=bottom r1=bottom,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 1;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }

            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(1), 1);
            isL1LongTerm = pRefPicList1[L1Index]->isLongTermRef(pRefPicList1[L1Index]->GetNumberByParity(1));

            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][0][1];        //complementary field pairs, cf=bottom r1=bottom,r0=top
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][0][1];  //complementary field pairs, cf=bottom r1=bottom,r0=top

            for (L0Index=0; L0Index<NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 0;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(0), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
            pDistScaleFactor = m_DistScaleFactorAFF->values[L1Index][1][1][1];       //complementary field pairs, cf=bottom r1=bottom,r0=bottom
            pDistScaleFactorMV = m_DistScaleFactorMVAFF->values[1][1][1];   //complementary field pairs, cf=bottom r1=bottom,r0=bottom

            for (L0Index=0; L0Index < NumL0RefActive; L0Index++)
            {
                VM_ASSERT(pRefPicList0[L0Index]);

                Ipp32s RefFieldTop = 1;
                picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList0[L0Index]->GetNumberByParity(1), 1);
                CalculateDSF(L0Index, pDistScaleFactor[L0Index], pDistScaleFactorMV[L0Index]);
            }
        }

        if (!isNeedScale)
        {
            m_DistScaleFactorAFF = 0;
        }
    }
} // void H264Slice::InitDistScaleFactor(Ipp32s NumL0RefActive,

Ipp32s H264Slice::AdjustRefPicListForFields(H264DecoderFrame **pRefPicList,
                                          ReferenceFlags *pFields,
                                          H264RefListInfo &rli)
{
    H264DecoderFrame *TempList[MAX_NUM_REF_FRAMES+1];
    Ipp8u TempFields[MAX_NUM_REF_FRAMES+1];
    //walk through list and set correct indices
    Ipp32s i=0,j=0,numSTR=0,numLTR=0;
    Ipp32s num_same_parity = 0, num_opposite_parity = 0;
    Ipp8s current_parity = m_SliceHeader.bottom_field_flag;
    //first scan the list to determine number of shortterm and longterm reference frames
    while ((numSTR < 16) && pRefPicList[numSTR] && pRefPicList[numSTR]->isShortTermRef()) numSTR++;
    while ((numSTR + numLTR < 16) && pRefPicList[numSTR+numLTR] && pRefPicList[numSTR+numLTR]->isLongTermRef()) numLTR++;

    while (num_same_parity < numSTR ||  num_opposite_parity < numSTR)
    {
        //try to fill shorttermref fields with the same parity first
        if (num_same_parity < numSTR)
        {
            while (num_same_parity < numSTR)
            {
                Ipp32s ref_field = pRefPicList[num_same_parity]->GetNumberByParity(current_parity);
                if (pRefPicList[num_same_parity]->isShortTermRef(ref_field))
                    break;

                num_same_parity++;

            }

            if (num_same_parity<numSTR)
            {
                TempList[i] = pRefPicList[num_same_parity];
                TempFields[i] = current_parity;
                i++;
                num_same_parity++;
            }
        }
        //now process opposite parity
        if (num_opposite_parity < numSTR)
        {
            while (num_opposite_parity < numSTR)
            {
                Ipp32s ref_field = pRefPicList[num_opposite_parity]->GetNumberByParity(!current_parity);
                if (pRefPicList[num_opposite_parity]->isShortTermRef(ref_field))
                    break;
                num_opposite_parity++;
            }

            if (num_opposite_parity<numSTR) //selected field is reference
            {
                TempList[i] = pRefPicList[num_opposite_parity];
                TempFields[i] = !current_parity;
                i++;
                num_opposite_parity++;
            }
        }
    }

    rli.m_iNumShortEntriesInList = (Ipp8u) i;
    num_same_parity = num_opposite_parity = 0;
    //now processing LongTermRef
    while(num_same_parity<numLTR ||  num_opposite_parity<numLTR)
    {
        //try to fill longtermref fields with the same parity first
        if (num_same_parity < numLTR)
        {
            while (num_same_parity < numLTR)
            {
                Ipp32s ref_field = pRefPicList[num_same_parity+numSTR]->GetNumberByParity(current_parity);
                if (pRefPicList[num_same_parity+numSTR]->isLongTermRef(ref_field))
                    break;
                num_same_parity++;
            }
            if (num_same_parity<numLTR)
            {
                TempList[i] = pRefPicList[num_same_parity+numSTR];
                TempFields[i] = current_parity;
                i++;
                num_same_parity++;
            }
        }
        //now process opposite parity
        if (num_opposite_parity<numLTR)
        {
            while (num_opposite_parity < numLTR)
            {
                Ipp32s ref_field = pRefPicList[num_opposite_parity+numSTR]->GetNumberByParity(!current_parity);

                if (pRefPicList[num_opposite_parity+numSTR]->isLongTermRef(ref_field))
                    break;
                num_opposite_parity++;
            }

            if (num_opposite_parity<numLTR) //selected field is reference
            {
                TempList[i] = pRefPicList[num_opposite_parity+numSTR];
                TempFields[i] = !current_parity;
                i++;
                num_opposite_parity++;
            }
        }
    }

    rli.m_iNumLongEntriesInList = (Ipp8u) (i - rli.m_iNumShortEntriesInList);
    j = 0;
    while(j < i)//copy data back to list
    {
        pRefPicList[j] = TempList[j];
        pFields[j].field = TempFields[j];
        pFields[j].isShortReference = (unsigned char) (pRefPicList[j]->isShortTermRef(pRefPicList[j]->GetNumberByParity(TempFields[j])) ? 1 : 0);
        j++;
    }

    while(j < MAX_NUM_REF_FRAMES)//fill remaining entries
    {
        pRefPicList[j] = NULL;
        pFields[j].field = 0;
        pFields[j].isShortReference = 0;
        j++;
    }

    return i;
} // Ipp32s H264Slice::AdjustRefPicListForFields(H264DecoderFrame **pRefPicList, Ipp8s *pFields)

void H264Slice::ReOrderRefPicList(H264DecoderFrame **pRefPicList,
                                  ReferenceFlags *pFields,
                                  RefPicListReorderInfo *pReorderInfo,
                                  Ipp32s MaxPicNum,
                                  ViewList &views,
                                  Ipp32s dIdIndex,
                                  Ipp32u listNum)
{
    bool bIsFieldSlice = (m_SliceHeader.field_pic_flag != 0);
    Ipp32u NumRefActive = (0 == listNum) ?
                          m_SliceHeader.num_ref_idx_l0_active :
                          m_SliceHeader.num_ref_idx_l1_active;
    Ipp32u i;
    Ipp32s picNumNoWrap;
    Ipp32s picNum;
    Ipp32s picNumPred;
    Ipp32s picNumCurr;
    Ipp32s picViewIdxLXPred = -1;
    H264DBPList *pDecoderFrameList = GetDPB(views, m_SliceHeader.nal_ext.mvc.view_id, dIdIndex);

    // Reference: Reordering process for reference picture lists, 8.2.4.3
    if (!bIsFieldSlice)
    {
        picNumCurr = m_pCurrentFrame->PicNum(0,3);
        picNumPred = picNumCurr;

        for (i=0; i<pReorderInfo->num_entries; i++)
        {
            if (pReorderInfo->reordering_of_pic_nums_idc[i] < 2)
            {
                // short-term reorder
                if (pReorderInfo->reordering_of_pic_nums_idc[i] == 0)
                {
                    picNumNoWrap = picNumPred - pReorderInfo->reorder_value[i];
                    if (picNumNoWrap < 0)
                        picNumNoWrap += MaxPicNum;
                }
                else
                {
                    picNumNoWrap = picNumPred + pReorderInfo->reorder_value[i];
                    if (picNumNoWrap >= MaxPicNum)
                        picNumNoWrap -= MaxPicNum;
                }
                picNumPred = picNumNoWrap;

                picNum = picNumNoWrap;
                if (picNum > picNumCurr)
                    picNum -= MaxPicNum;

                H264DecoderFrame* pFrame = pDecoderFrameList->findShortTermPic(picNum, 0);

                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with picNum.
                pRefPicList[i] = pFrame;
                pFields[i].field = 0;
                pFields[i].isShortReference = 1;
                Ipp32s refIdxLX = i + 1;

                for(Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        if (!(pRefPicList[kk]->isShortTermRef(pRefPicList[kk]->GetNumberByParity(pFields[kk].field)) &&
                            pRefPicList[kk]->PicNum(pRefPicList[kk]->GetNumberByParity(pFields[kk].field), 1) == picNum))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }    // short-term reorder
            else if (2 == pReorderInfo->reordering_of_pic_nums_idc[i])
            {
                // long term reorder
                picNum = pReorderInfo->reorder_value[i];

                H264DecoderFrame* pFrame = pDecoderFrameList->findLongTermPic(picNum, 0);

                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with picNum.
                pRefPicList[i] = pFrame;
                pFields[i].field = 0;
                pFields[i].isShortReference = 0;
                Ipp32s refIdxLX = i + 1;

                for(Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        if (!(pRefPicList[kk]->isLongTermRef(pRefPicList[kk]->GetNumberByParity(pFields[kk].field)) &&
                            pRefPicList[kk]->LongTermPicNum(pRefPicList[kk]->GetNumberByParity(pFields[kk].field), 1) == picNum))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }    // long term reorder
            // MVC reordering
            else if ((4 == pReorderInfo->reordering_of_pic_nums_idc[i]) ||
                     (5 == pReorderInfo->reordering_of_pic_nums_idc[i]))
            {
                Ipp32s abs_diff_view_idx = pReorderInfo->reorder_value[i];
                Ipp32s maxViewIdx;
                Ipp32u currVOIdx = 0, viewIdx;
                Ipp32s targetViewID;

                if (!m_pSeqParamSetMvcEx)
                    continue;

                Ipp32u picViewIdxLX;
                H264DecoderFrame* pFrame = NULL;
                Ipp32s curPOC = m_pCurrentFrame->PicOrderCnt(0, 3);

                // set current VO index
                for (viewIdx = 0; viewIdx <= m_pSeqParamSetMvcEx->num_views_minus1; viewIdx += 1)
                {
                    if (m_SliceHeader.nal_ext.mvc.view_id == m_pSeqParamSetMvcEx->viewInfo[viewIdx].view_id)
                    {
                        currVOIdx = viewIdx;
                        break;
                    }
                }

                const H264ViewRefInfo &viewInfo = m_pSeqParamSetMvcEx->viewInfo[currVOIdx];

                // get the maximum number of reference
                if (m_SliceHeader.nal_ext.mvc.anchor_pic_flag)
                {
                    maxViewIdx = viewInfo.num_anchor_refs_lx[listNum];
                }
                else
                {
                    maxViewIdx = viewInfo.num_non_anchor_refs_lx[listNum];
                }

                // get the index of view reference
                if (4 == pReorderInfo->reordering_of_pic_nums_idc[i])
                {
                    if (picViewIdxLXPred - abs_diff_view_idx < 0)
                    {
                        picViewIdxLX = picViewIdxLXPred - abs_diff_view_idx + maxViewIdx;
                    }
                    else
                    {
                        picViewIdxLX = picViewIdxLXPred - abs_diff_view_idx;
                    }
                }
                else
                {
                    if (picViewIdxLXPred + abs_diff_view_idx >= maxViewIdx)
                    {
                        picViewIdxLX = picViewIdxLXPred + abs_diff_view_idx - maxViewIdx;
                    }
                    else
                    {
                        picViewIdxLX = picViewIdxLXPred + abs_diff_view_idx;
                    }
                }
                picViewIdxLXPred = picViewIdxLX;

                // get the target view
                if (m_SliceHeader.nal_ext.mvc.anchor_pic_flag)
                {
                    targetViewID = viewInfo.anchor_refs_lx[listNum][picViewIdxLX];
                }
                else
                {
                    targetViewID = viewInfo.non_anchor_refs_lx[listNum][picViewIdxLX];
                }
                ViewList::iterator iter = views.begin();
                ViewList::iterator iter_end = views.end();
                for (; iter != iter_end; ++iter)
                {
                    if (targetViewID == iter->viewId)
                    {
                        pFrame = iter->GetDPBList(dIdIndex)->findInterViewRef(m_pCurrentFrame->m_auIndex, m_SliceHeader.bottom_field_flag);
                        break;
                    }
                }
                // corresponding frame is found
                if (!pFrame)
                    continue;

                // make some space to insert the reference
                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with cuuPOC.
                pRefPicList[i] = pFrame;
                pFields[i].field = 0;
                pFields[i].isShortReference = 1;
                Ipp32u refIdxLX = i + 1;

                for (Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        if ((pRefPicList[kk]->m_viewId != targetViewID) ||
                            (pRefPicList[kk]->PicOrderCnt(0, 3) != curPOC))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }
        }    // for i
    }
    else
    {
        picNumCurr = m_pCurrentFrame->PicNum(m_pCurrentFrame->GetNumberByParity(m_SliceHeader.bottom_field_flag));
        picNumPred = picNumCurr;

        for (i=0; i<pReorderInfo->num_entries; i++)
        {
            if (pReorderInfo->reordering_of_pic_nums_idc[i] < 2)
            {
                // short-term reorder
                if (pReorderInfo->reordering_of_pic_nums_idc[i] == 0)
                {
                    picNumNoWrap = picNumPred - pReorderInfo->reorder_value[i];
                    if (picNumNoWrap < 0)
                        picNumNoWrap += MaxPicNum;
                }
                else
                {
                    picNumNoWrap = picNumPred + pReorderInfo->reorder_value[i];
                    if (picNumNoWrap >= MaxPicNum)
                        picNumNoWrap -= MaxPicNum;
                }
                picNumPred = picNumNoWrap;

                picNum = picNumNoWrap;
                if (picNum > picNumCurr)
                    picNum -= MaxPicNum;

                Ipp32s frameField;
                H264DecoderFrame* pFrame = pDecoderFrameList->findShortTermPic(picNum, &frameField);

                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with picNum.
                pRefPicList[i] = pFrame;
                pFields[i].field = (char) (pFrame ? pFrame->m_bottom_field_flag[frameField] : 0);
                pFields[i].isShortReference = 1;
                Ipp32s refIdxLX = i + 1;

                for(Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        if (!(pRefPicList[kk]->isShortTermRef(pRefPicList[kk]->GetNumberByParity(pFields[kk].field)) &&
                            pRefPicList[kk]->PicNum(pRefPicList[kk]->GetNumberByParity(pFields[kk].field), 1) == picNum))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }    // short term reorder
            else if (2 == pReorderInfo->reordering_of_pic_nums_idc[i])
            {
                // long term reorder
                picNum = pReorderInfo->reorder_value[i];

                Ipp32s frameField;
                H264DecoderFrame* pFrame = pDecoderFrameList->findLongTermPic(picNum, &frameField);

                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with picNum.
                pRefPicList[i] = pFrame;
                pFields[i].field = (char) (pFrame ? pFrame->m_bottom_field_flag[frameField] : 0);
                pFields[i].isShortReference = 0;
                Ipp32s refIdxLX = i + 1;

                for(Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        if (!(pRefPicList[kk]->isLongTermRef(pRefPicList[kk]->GetNumberByParity(pFields[kk].field)) &&
                            pRefPicList[kk]->LongTermPicNum(pRefPicList[kk]->GetNumberByParity(pFields[kk].field), 1) == picNum))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }    // long term reorder
            // MVC reordering
            else if ((4 == pReorderInfo->reordering_of_pic_nums_idc[i]) ||
                     (5 == pReorderInfo->reordering_of_pic_nums_idc[i]))
            {
                Ipp32s abs_diff_view_idx = pReorderInfo->reorder_value[i];
                Ipp32s maxViewIdx;
                Ipp32u currVOIdx = 0, viewIdx;
                Ipp32s targetViewID;
                Ipp32u picViewIdxLX;
                H264DecoderFrame* pFrame = NULL;
                Ipp32u fieldIdx = m_pCurrentFrame->GetNumberByParity(m_SliceHeader.bottom_field_flag);
                Ipp32s curPOC = m_pCurrentFrame->PicOrderCnt(fieldIdx);

                // set current VO index
                for (viewIdx = 0; viewIdx <= m_pSeqParamSetMvcEx->num_views_minus1; viewIdx += 1)
                {
                    if (m_SliceHeader.nal_ext.mvc.view_id == m_pSeqParamSetMvcEx->viewInfo[viewIdx].view_id)
                    {
                        currVOIdx = viewIdx;
                        break;
                    }
                }

                const H264ViewRefInfo &viewInfo = m_pSeqParamSetMvcEx->viewInfo[currVOIdx];

                // get the maximum number of reference
                if (m_SliceHeader.nal_ext.mvc.anchor_pic_flag)
                {
                    maxViewIdx = viewInfo.num_anchor_refs_lx[listNum];
                }
                else
                {
                    maxViewIdx = viewInfo.num_non_anchor_refs_lx[listNum];
                }

                // get the index of view reference
                if (4 == pReorderInfo->reordering_of_pic_nums_idc[i])
                {
                    if (picViewIdxLXPred - abs_diff_view_idx < 0)
                    {
                        picViewIdxLX = picViewIdxLXPred - abs_diff_view_idx + maxViewIdx;
                    }
                    else
                    {
                        picViewIdxLX = picViewIdxLXPred - abs_diff_view_idx;
                    }
                }
                else
                {
                    if (picViewIdxLXPred + abs_diff_view_idx >= maxViewIdx)
                    {
                        picViewIdxLX = picViewIdxLXPred + abs_diff_view_idx - maxViewIdx;
                    }
                    else
                    {
                        picViewIdxLX = picViewIdxLXPred + abs_diff_view_idx;
                    }
                }
                picViewIdxLXPred = picViewIdxLX;

                // get the target view
                if (m_SliceHeader.nal_ext.mvc.anchor_pic_flag)
                {
                    targetViewID = viewInfo.anchor_refs_lx[listNum][picViewIdxLX];
                }
                else
                {
                    targetViewID = viewInfo.non_anchor_refs_lx[listNum][picViewIdxLX];
                }
                ViewList::iterator iter = views.begin();
                ViewList::iterator iter_end = views.end();
                for (; iter != iter_end; ++iter)
                {
                    if (targetViewID == iter->viewId)
                    {
                        pFrame = iter->GetDPBList(dIdIndex)->findInterViewRef(m_pCurrentFrame->m_auIndex, m_SliceHeader.bottom_field_flag);
                        break;
                    }
                }
                // corresponding frame is found
                if (!pFrame)
                    continue;

                // make some space to insert the reference
                for (Ipp32u k = NumRefActive; k > i; k--)
                {
                    pRefPicList[k] = pRefPicList[k - 1];
                    pFields[k] = pFields[k - 1];
                }

                // Place picture with picNum on list, shifting pictures
                // down by one while removing any duplication of picture with cuuPOC.
                pRefPicList[i] = pFrame;
                pFields[i].field = m_SliceHeader.bottom_field_flag;
                pFields[i].isShortReference = 1;
                Ipp32u refIdxLX = i + 1;

                for (Ipp32u kk = i + 1; kk <= NumRefActive; kk++)
                {
                    if (pRefPicList[kk])
                    {
                        Ipp32u refFieldIdx = pRefPicList[kk]->GetNumberByParity(m_SliceHeader.bottom_field_flag);

                        if ((pRefPicList[kk]->m_viewId != targetViewID) ||
                            (pRefPicList[kk]->PicOrderCnt(refFieldIdx) != curPOC))
                        {
                            pRefPicList[refIdxLX] = pRefPicList[kk];
                            pFields[refIdxLX] = pFields[kk];
                            refIdxLX++;
                        }
                    }
                }
            }
        }    // for i
    }
} // void H264Slice::ReOrderRefPicList(H264DecoderFrame **pRefPicList,

} // namespace UMC

#endif // UMC_ENABLE_H264_VIDEO_DECODER
