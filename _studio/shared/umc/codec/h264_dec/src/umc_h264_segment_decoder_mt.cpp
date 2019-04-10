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

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_segment_decoder_templates.h"
#include "umc_h264_task_broker_mt.h"
#include "umc_h264_frame_info.h"

#include "umc_h264_task_supplier.h"
#include "umc_h264_frame_list.h"

#include "umc_h264_timing.h"

#include "mfx_trace.h"

namespace UMC
{
static
#ifdef __ICL
    __declspec(align(16))
#endif
const uint8_t BlkOrder[16] =
{
    0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15
};

H264SegmentDecoderMultiThreaded::H264SegmentDecoderMultiThreaded(TaskBroker * pTaskBroker)
    : H264SegmentDecoder(pTaskBroker)
    , m_psBuffer(0)
    , m_SD(0)
{
} // H264SegmentDecoderMultiThreaded::H264SegmentDecoderMultiThreaded(H264SliceStore *Store)

H264SegmentDecoderMultiThreaded::~H264SegmentDecoderMultiThreaded(void)
{
    Release();

} // H264SegmentDecoderMultiThreaded::~H264SegmentDecoderMultiThreaded(void)

Status H264SegmentDecoderMultiThreaded::Init(int32_t iNumber)
{
    // release object before initialization
    Release();

    // call parent's Init
    return H264SegmentDecoder::Init(iNumber);
} // Status H264SegmentDecoderMultiThreaded::Init(uint32_t nNumber)

void H264SegmentDecoderMultiThreaded::StartProcessingSegment(H264Task &Task)
{
    m_pSlice = Task.m_pSlice;
    m_pCurrentFrame = m_pSlice->GetCurrentFrameEx();
    H264DecoderFrame *pCurrentFrame = m_pCurrentFrame;

    m_pSliceHeader = m_pSlice->GetSliceHeader();

    m_field_index = m_pSliceHeader->bottom_field_flag;
    m_bError = Task.m_bError;

    // reset decoding variables
    m_pBitStream = &m_pSlice->m_SliceDataBitStream;
    mb_height = m_pSlice->GetMBHeight();
    mb_width = m_pSlice->GetMBWidth();
    m_pPicParamSet = m_pSlice->GetPicParam();
    m_pScalingPicParams = &m_pPicParamSet->scaling[NAL_UT_CODED_SLICE_EXTENSION == m_pSliceHeader->nal_unit_type ? 1 : 0];
    m_pPredWeight[0] = m_pSlice->GetPredWeigthTable(0);
    m_pPredWeight[1] = m_pSlice->GetPredWeigthTable(1);
    m_pSeqParamSet = m_pSlice->GetSeqParam();
    m_pCoeffBlocksRead = m_pCoeffBlocksWrite = m_psBuffer = Task.m_pBuffer;
    m_pMBIntraTypes = m_pSlice->GetMBIntraTypes();

    m_bNeedToCheckMBSliceEdges = m_pSlice->NeedToCheckSliceEdges();
    m_mbinfo = m_pSlice->GetMBInfo();

    bit_depth_luma = m_pSeqParamSet->bit_depth_luma;
    bit_depth_chroma = m_pSeqParamSet->bit_depth_chroma;

    m_MVDistortion[0] = 0;
    m_MVDistortion[1] = 0;

    m_PairMBAddr = 0;
    m_CurMBAddr = 0;
    m_iSkipNextMacroblock = 0;

    m_gmbinfo = &(m_pCurrentFrame->m_mbinfo);
    if (m_pSlice->GetSliceHeader()->nal_ext.svc.quality_id)
    {
        m_gmbinfo = &(m_pSlice->GetCurrentFrameEx()->m_mbinfo);
    }

    m_isSliceGroups = m_pSlice->IsSliceGroups();

    if (Task.m_iTaskID == TASK_DEB_FRAME)
    {
        m_pSlice = 0;
    }

    m_SD = CreateSegmentDecoder();

    bool is_field = m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;

    m_uPitchLuma = pCurrentFrame->pitch_luma();
    m_uPitchChroma = pCurrentFrame->pitch_chroma();

    m_pYPlane = pCurrentFrame->m_pYPlane;
    m_pUPlane = pCurrentFrame->m_pUPlane;
    m_pVPlane = pCurrentFrame->m_pVPlane;
    m_pUVPlane = pCurrentFrame->m_pUVPlane;

    m_cur_mb.isInited = false;

    if (!m_pUPlane || !m_pVPlane)
    {
        m_pUPlane = pCurrentFrame->m_pUVPlane;
        m_pVPlane = pCurrentFrame->m_pUVPlane;
    }

    if (is_field)
    {
        if(m_field_index)
        {
            int32_t pixel_luma_sz    = bit_depth_luma > 8 ? 2 : 1;
            int32_t pixel_chroma_sz  = bit_depth_chroma > 8 ? 2 : 1;

            m_pYPlane += m_uPitchLuma*pixel_luma_sz;
            m_pUPlane += m_uPitchChroma*pixel_chroma_sz;
            m_pVPlane += m_uPitchChroma*pixel_chroma_sz;
            m_pUVPlane += m_uPitchChroma*pixel_chroma_sz;
        }

        m_uPitchLuma *= 2;
        m_uPitchChroma *= 2;
    }

    if (!m_pSlice) // TASK_DEB_FRAME
        return;

    // InitContext
    m_iFirstSliceMb = m_pSlice->GetFirstMBNumber();
    m_iSliceNumber = m_pSlice->GetSliceNum();

    m_isMBAFF = 0 != m_pSliceHeader->MbaffFrameFlag;

    m_IsUseSpatialDirectMode = (m_pSliceHeader->direct_spatial_mv_pred_flag != 0);

    int32_t sliceNum = m_pSlice->GetSliceNum();
    m_pFields[0] = m_pCurrentFrame->GetRefPicList(sliceNum, 0)->m_Flags;
    m_pFields[1] = m_pCurrentFrame->GetRefPicList(sliceNum, 1)->m_Flags;
    m_pRefPicList[0] = m_pCurrentFrame->GetRefPicList(sliceNum, 0)->m_RefPicList;
    m_pRefPicList[1] = m_pCurrentFrame->GetRefPicList(sliceNum, 1)->m_RefPicList;

    if (TASK_PROCESS == Task.m_iTaskID || TASK_DEC == Task.m_iTaskID || TASK_DEC_REC == Task.m_iTaskID)
        m_pSlice->GetStateVariables(m_MBSkipCount, m_QuantPrev, m_prev_dquant);

    m_IsUseConstrainedIntra = (m_pPicParamSet->constrained_intra_pred_flag != 0);
    m_IsUseDirect8x8Inference = (m_pSeqParamSet->direct_8x8_inference_flag != 0);
    m_IsBSlice = (m_pSliceHeader->slice_type == BPREDSLICE);

    // reset neighbouringing blocks numbers
    m_cur_mb.CurrentBlockNeighbours.m_bInited = 0;
}

void H264SegmentDecoderMultiThreaded::EndProcessingSegment(H264Task &Task)
{
    // return performed task
    Task.m_WrittenSize = (uint8_t*)m_pCoeffBlocksWrite - (uint8_t*)Task.m_pBuffer;
    if (Task.m_bError)
    {
        Task.m_WrittenSize += COEFFICIENTS_BUFFER_SIZE;
    }

    if (TASK_DEC == Task.m_iTaskID)
    {
        m_MVDistortion[0] = (m_MVDistortion[0] + 6) >> INTERP_SHIFT;
        m_MVDistortion[1] = (m_MVDistortion[1] + 6) >> INTERP_SHIFT;

        if (m_pSlice->IsField() || m_pSlice->GetSliceHeader()->MbaffFrameFlag)
        {
            m_MVDistortion[0] <<= 1;
            m_MVDistortion[1] <<= 1;
        }

        m_MVDistortion[0] = MFX_MAX(m_MVDistortion[0], m_MVDistortion[1]);
        Task.m_mvsDistortion = MFX_MAX(m_MVDistortion[0], m_pSlice->m_MVsDistortion);
    }

    m_pTaskBroker->AddPerformedTask(&Task);
}

Status H264SegmentDecoderMultiThreaded::ProcessSegment(void)
{
    AutomaticUMCMutex guard(m_mGuard);

    H264Task Task(m_iNumber);
    try
    {
        if (m_pTaskBroker->GetNextTask(&Task))
        {
            Status umcRes = UMC_OK;

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "AVCDec_work");

            StartProcessingSegment(Task);

            if (!m_pCurrentFrame->IsSkipped() && (!m_pSlice || !m_pSlice->IsError()))
            {
                try // do decoding
                {
                    umcRes = ProcessTask(&Task);

                    if (UMC_ERR_END_OF_STREAM == umcRes)
                    {
                        Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                        VM_ASSERT(m_pSlice);
                        // if we decode less macroblocks if we need try to recovery:
                        RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
                        umcRes = UMC_OK;
                    }
                    else if (UMC_OK != umcRes)
                    {
                        umcRes = UMC_ERR_INVALID_STREAM;
                    }

                    Task.m_bDone = true;

                } catch(const h264_exception & ex)
                {
                    umcRes = ex.GetStatus();
                } catch(...)
                {
                    umcRes = UMC_ERR_INVALID_STREAM;
                }
            }

            if (umcRes != UMC_OK)
            {
                Task.m_bError = true;
                Task.m_iMaxMB = Task.m_iFirstMB + Task.m_iMBToProcess;
                if (m_pSlice)
                    RestoreErrorRect(Task.m_iFirstMB + Task.m_iMBToProcess, m_pSlice->GetMaxMB(), m_pSlice);
            }

            EndProcessingSegment(Task);

            MFX_LTRACE_I(MFX_TRACE_LEVEL_INTERNAL, umcRes);
        }
        else
        {
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
    }
    catch(h264_exception ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    return UMC_OK;
} // Status H264SegmentDecoderMultiThreaded::ProcessSegment(void)

Status H264SegmentDecoderMultiThreaded::ProcessTask(H264Task *task)
{
    int32_t firstMB = task->m_iFirstMB;
    if (m_field_index)
    {
        firstMB += mb_width*mb_height / 2;
    }

    switch(task->m_iTaskID)
    {
    case TASK_PROCESS:
        return ProcessSlice(firstMB, task->m_iMBToProcess);
    case TASK_DEB_FRAME:
        return DeblockSegmentTask(firstMB, task->m_iMBToProcess);
    case TASK_DEB_SLICE:
        return DeblockSegmentTask(firstMB, task->m_iMBToProcess);
    case TASK_DEC:
        return DecodeSegment(firstMB, task->m_iMBToProcess);
    case TASK_REC:
        return ReconstructSegment(firstMB, task->m_iMBToProcess);
    case TASK_DEC_REC:
        return DecRecSegment(firstMB, task->m_iMBToProcess);
    case TASK_DEB:
        return DeblockSegmentTask(firstMB, task->m_iMBToProcess);
    }

    return UMC_ERR_FAILED;
}

void H264SegmentDecoderMultiThreaded::RestoreErrorRect(int32_t startMb, int32_t endMb, H264Slice * pSlice)
{
    if (startMb >= endMb || !pSlice || pSlice->IsSliceGroups())
        return;

    AutomaticUMCMutex guard(m_mGuard);

    m_pSlice = (H264SliceEx *)pSlice;
    m_isSliceGroups = pSlice->IsSliceGroups();
    m_pSeqParamSet = pSlice->GetSeqParam();
    m_pPicParamSet = m_pSlice->GetPicParam();

    try
    {
        H264DecoderFrameEx * pCurrentFrame = m_pSlice->GetCurrentFrameEx();

        if (!pCurrentFrame || !pCurrentFrame->m_pYPlane)
        {
            return;
        }

        H264DecoderFrame * pRefFrame = pCurrentFrame->GetRefPicList(m_pSlice->GetSliceNum(), 0)->m_RefPicList[0];

        pCurrentFrame->SetErrorFlagged(ERROR_FRAME_MAJOR);

        if (!pRefFrame || pRefFrame->IsSkipped())
        {
            H264DBPList * lst = m_pTaskBroker->m_pTaskSupplier->GetDPBList(BASE_VIEW, 0);
            if (lst)
            {
                pRefFrame = lst->FindClosest(pCurrentFrame);
            }
        }

        m_pCurrentFrame = pCurrentFrame;
        bit_depth_luma = m_pSeqParamSet->bit_depth_luma;
        bit_depth_chroma = m_pSeqParamSet->bit_depth_chroma;
        m_SD = CreateSegmentDecoder();

        mb_height = m_pSlice->GetMBHeight();
        mb_width = m_pSlice->GetMBWidth();
        m_gmbinfo = &(pCurrentFrame->m_mbinfo);

        m_SD->RestoreErrorRect(startMb, endMb, pRefFrame, this);
    } catch (...)
    {
        // nothing to do
    }
}

Status H264SegmentDecoderMultiThreaded::DecodeSegment(int32_t iCurMBNumber, int32_t &iMBToProcess)
{
    Status umcRes = UMC_OK;
    int32_t iMaxMBNumber = iCurMBNumber + iMBToProcess;
    int32_t iMBRowSize = m_pSlice->GetMBRowWidth();
    int32_t iFirstMB = iCurMBNumber;
    START_TICK

    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        uint32_t nBorder;
        int32_t iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = MFX_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
            if (m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = DecodeMacroBlockCABAC(iCurMBNumber, nBorder);
            }
            else
            {
                umcRes = DecodeMacroBlockCAVLC(iCurMBNumber, nBorder);
            }
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((UMC_OK != umcRes) || (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(decode_time)

    if (UMC_ERR_END_OF_STREAM == umcRes)
    {
        iMBToProcess = m_CurMBAddr - iFirstMB;
    }

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::DecodeSegment(int32_t iCurMBNumber, int32_t &iMBToDecode)

Status H264SegmentDecoderMultiThreaded::ReconstructSegment(int32_t iCurMBNumber, int32_t &iMBToProcess)
{
    Status umcRes = UMC_OK;
    int32_t iMaxMBNumber = iCurMBNumber + iMBToProcess;
    int32_t iMBRowSize = m_pSlice->GetMBRowWidth();
    int32_t iFirstMB = iCurMBNumber;
    START_TICK

    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        uint32_t nBorder;
        int32_t iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = MFX_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
            if (m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = ReconstructMacroBlockCABAC(iCurMBNumber, nBorder);
            }
            else
            {
                umcRes = ReconstructMacroBlockCAVLC(iCurMBNumber, nBorder);
            }
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((UMC_OK != umcRes) || (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(decode_time)

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::ReconstructSegment(int32_t iCurMBNumber, int32_t &iMBToReconstruct)

Status H264SegmentDecoderMultiThreaded::DecRecSegment(int32_t iCurMBNumber, int32_t &iMBToProcess)
{
    Status umcRes = UMC_OK;
    int32_t iBorder = iCurMBNumber + iMBToProcess;

    m_psBuffer = align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer, DEFAULT_ALIGN_VALUE);

    try
    {
        if (m_pPicParamSet->entropy_coding_mode)
        {
            umcRes = m_SD->DecodeSegmentCABAC_Single(iCurMBNumber, iBorder, this);

        }
        else
        {
            umcRes = m_SD->DecodeSegmentCAVLC_Single(iCurMBNumber, iBorder, this);
        }
    }
    catch(...)
    {
        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        throw;
    }

    iMBToProcess = m_CurMBAddr - iCurMBNumber;

    return umcRes;

} // Status H264SegmentDecoderMultiThreaded::DecRecSegment(int32_t iCurMBNumber, int32_t &iMBToReconstruct)

Status H264SegmentDecoderMultiThreaded::DeblockSegmentTask(int32_t iCurMBNumber, int32_t &iMBToProcess)
{
    START_TICK
    // when there is slice groups or threaded deblocking
    if (NULL == m_pSlice)
    {
        H264SegmentDecoder::DeblockFrame(iCurMBNumber, iMBToProcess);
        return UMC_OK;
    }

    int32_t iMaxMBNumber = iCurMBNumber + iMBToProcess;
    int32_t iMBRowSize = m_pSlice->GetMBRowWidth();
    int32_t iFirstMB = iCurMBNumber;

    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        uint32_t nBorder;
        int32_t iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = MFX_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
            DeblockSegment(iCurMBNumber, nBorder);
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // check error(s)
        if ((m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;
    }

    END_TICK(deblocking_time)

    return UMC_OK;

} // Status H264SegmentDecoderMultiThreaded::DeblockSegmentTask(int32_t iCurMBNumber, int32_t &iMBToDeblock)

void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CABAC(void)
{
    int32_t curmb_fdf = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    // initialize blocks coding pattern maps

    int32_t type = m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD;
    if(type >= 0 && type < 5)
    {
        if (m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR) // direct or skip MB
        {
            return;
        }

        // get all ref_idx_L0
        GetRefIdx4x4_CABAC(
                    m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                    pCodFBD[type][0],
                    0);
        // get all ref_idx_L1
        if (m_pSliceHeader->slice_type == BPREDSLICE)
        GetRefIdx4x4_CABAC(
                    m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                    pCodFBD[type][1],
                    1);
        // get all mvd_L0
        GetMVD4x4_CABAC(
                    pCodFBD[type][2],
                    0);
        // get all mvd_L1
        if (m_pSliceHeader->slice_type == BPREDSLICE)
        GetMVD4x4_CABAC(
                    pCodFBD[type][3],
                    1);
    }
    else
    {
#ifdef __ICL
        __declspec(align(16))uint8_t pCodMVdL0[16];
        __declspec(align(16))uint8_t pCodMVdL1[16];
#else
        uint8_t pCodMVdL0[16];
        uint8_t pCodMVdL1[16];
#endif
        uint8_t *pMVdL0 = pCodMVdL0;
        uint8_t *pMVdL1 = pCodMVdL1;
        uint8_t cL0;
        uint8_t cL1;
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_INTER_16x8:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[0] = cL0;
                pMVdL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[8] = cL0;
                pMVdL1[8] = cL1;

                // get all ref_idx_L0
                GetRefIdx4x4_16x8_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            pMVdL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_16x8_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            pMVdL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_16x8_CABAC(
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_16x8_CABAC(
                            pMVdL1,
                            1);
                break;
            case MBTYPE_INTER_8x16:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[0] = cL0;
                pMVdL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pMVdL0[2] = cL0;
                pMVdL1[2] = cL1;

                GetRefIdx4x4_8x16_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            pMVdL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_8x16_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            pMVdL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_8x16_CABAC(
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_8x16_CABAC(
                            pMVdL1,
                            1);
                break;
            case MBTYPE_INTER_8x8:
            case MBTYPE_INTER_8x8_REF0:
                MFX_INTERNAL_CPY(pCodMVdL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                {
                for (int32_t i = 0; i < 4; i ++)
                {
                    int32_t j = subblock_block_mapping[i];

                    if (m_cur_mb.LocalMacroblockInfo->sbdir[i] > D_DIR_BIDIR)
                    {
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        continue;
                    }

                    switch (m_cur_mb.GlobalMacroblockInfo->sbtype[i])
                    {
                    case SBTYPE_8x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 4] = CodAbov;
                        pCodMVdL1[j + 4] = CodAbov;
                        break;
                    case SBTYPE_8x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 4] = cL0;
                        pCodMVdL1[j + 4] = cL1;
                        break;
                    case SBTYPE_4x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 1] = cL0;
                        pCodMVdL1[j + 1] = cL1;
                        pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = CodAbov;
                        pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = CodAbov;
                        break;
                    case SBTYPE_4x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    case SBTYPE_DIRECT:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    default:
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                // get all ref_idx_L0
                GetRefIdx4x4_CABAC(
                            m_pSliceHeader->num_ref_idx_l0_active<<curmb_fdf,
                            BlkOrder,
                            pMVdL0,
                            0);

                // get all ref_idx_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetRefIdx4x4_CABAC(
                            m_pSliceHeader->num_ref_idx_l1_active<<curmb_fdf,
                            BlkOrder,
                            pMVdL1,
                            1);

                // get all mvd_L0
                GetMVD4x4_CABAC(
                            BlkOrder,
                            pMVdL0,
                            0);

                // get all mvd_L1
                if (m_pSliceHeader->slice_type == BPREDSLICE)
                GetMVD4x4_CABAC(
                            BlkOrder,
                            pMVdL1,
                            1);
                break;
                }
            default:
                throw h264_exception(UMC_ERR_INVALID_STREAM);
        }    // switch

    }

    ReconstructMotionVectors();

} // void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CABAC(void)

#define RECONSTRUCT_MOTION_VECTORS(sub_block_num, vector_offset, position) \
    switch (pSBDir[sub_block_num]) \
    { \
    case D_DIR_FWD: \
        ReconstructMVs##position(0); \
        ResetMVs8x8(1, vector_offset); \
        break; \
    case D_DIR_BIDIR: \
        ReconstructMVs##position(0); \
        ReconstructMVs##position(1); \
        break; \
    case D_DIR_BWD: \
        ResetMVs8x8(0, vector_offset); \
        ReconstructMVs##position(1); \
        break; \
    case D_DIR_DIRECT_SPATIAL_FWD: \
        ResetMVs8x8(1, vector_offset); \
        break; \
    case D_DIR_DIRECT_SPATIAL_BWD: \
        ResetMVs8x8(0, vector_offset); \
        break; \
    default: \
        break; \
    }

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors(void)
{
    /* DEBUG : temporary put off MBAFF */
    if (false == m_isMBAFF)
    {
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
            // forward predicted macroblock
        case MBTYPE_FORWARD:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR) // DEBUG : what check did?
            {
                // set forward vectors
                ReconstructMVs16x16(0);

                // set backward vectors
                ResetMVs16x16(1);
            }
            break;

            // backward predicted macroblock
        case MBTYPE_BACKWARD:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR)
            {
                // set forward vectors
                ResetMVs16x16(0);

                // set backward vectors
                ReconstructMVs16x16(1);
            }
            break;

            // bi-directional predicted macroblock
        case MBTYPE_BIDIR:
            //if (m_cur_mb.LocalMacroblockInfo->sbdir[0] <= D_DIR_BIDIR)
            {
                // set forward vectors
                ReconstructMVs16x16(0);

                // set backward vectors
                ReconstructMVs16x16(1);
            }
            break;

        case MBTYPE_INTER_16x8:
            {
                // set forward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD)
                    ResetMVs16x8(0, 0);
                else
                    ReconstructMVs16x8(0, 0);

                // set backward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD)
                    ResetMVs16x8(1, 0);
                else
                    ReconstructMVs16x8(1, 0);

                // set forward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD)
                    ResetMVs16x8(0, 8);
                else
                    ReconstructMVs16x8(0, 1);

                // set backward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD)
                    ResetMVs16x8(1, 8);
                else
                    ReconstructMVs16x8(1, 1);
            }
            break;

        case MBTYPE_INTER_8x16:
            {
                // set forward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD)
                    ResetMVs8x16(0, 0);
                else
                    ReconstructMVs8x16(0, 0);

                // set backward vectors for the first sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD)
                    ResetMVs8x16(1, 0);
                else
                    ReconstructMVs8x16(1, 0);

                // set forward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD)
                    ResetMVs8x16(0, 2);
                else
                    ReconstructMVs8x16(0, 1);

                // set backward vectors for the second sub-block
                if (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD)
                    ResetMVs8x16(1, 2);
                else
                    ReconstructMVs8x16(1, 1);

                CopyMVs8x16(0);
                CopyMVs8x16(1);
            }
            break;

        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            {
                int8_t *pSBDir = m_cur_mb.LocalMacroblockInfo->sbdir;

                RECONSTRUCT_MOTION_VECTORS(0, 0, External);
                RECONSTRUCT_MOTION_VECTORS(1, 2, Top);
                RECONSTRUCT_MOTION_VECTORS(2, 8, Left);
                RECONSTRUCT_MOTION_VECTORS(3, 10, Internal);
            }
            break;

            // all other
        default:
            return;
            break;
        }

        return;
    }


    // initialize blocks coding pattern maps

    int32_t type = m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD;
    if(type >= 0)
    {
        if (m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
            return;

        // get all mvd_L0
        ReconstructMotionVectors4x4(pCodFBD[type][2], 0);
        // get all mvd_L1
        ReconstructMotionVectors4x4(pCodFBD[type][3], 1);
    }
    else
    {
        uint32_t i, j;
        const uint8_t *pRIxL0, *pRIxL1, *pMVdL0, *pMVdL1;
#ifdef __ICL
        __declspec(align(16))uint8_t pCodRIxL0[16];
        __declspec(align(16))uint8_t pCodRIxL1[16];
        __declspec(align(16))uint8_t pCodMVdL0[16];
        __declspec(align(16))uint8_t pCodMVdL1[16];
#else
        uint8_t pCodRIxL0[16];
        uint8_t pCodRIxL1[16];
        uint8_t pCodMVdL0[16];
        uint8_t pCodMVdL1[16];
#endif
        pRIxL0 = pCodRIxL0;
        pRIxL1 = pCodRIxL1;
        pMVdL0 = pCodMVdL0;
        pMVdL1 = pCodMVdL1;

        uint8_t cL0;
        uint8_t cL1;
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_INTER_16x8:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[0] = cL0;
                pCodRIxL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[8] = cL0;
                pCodRIxL1[8] = cL1;

                // get all mvd_L0
                ReconstructMotionVectors16x8(pRIxL0, 0);

                // get all mvd_L1
                ReconstructMotionVectors16x8(pRIxL1, 1);
                break;
            case MBTYPE_INTER_8x16:
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[0] = cL0;
                pCodRIxL1[0] = cL1;
                cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) ? CodNone : CodInBS;
                cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) ? CodNone : CodInBS;
                pCodRIxL0[2] = cL0;
                pCodRIxL1[2] = cL1;

                // get all mvd_L0
                ReconstructMotionVectors8x16(pRIxL0, 0);

                // get all mvd_L1
                ReconstructMotionVectors8x16(pRIxL1, 1);
                break;
            case MBTYPE_INTER_8x8:
            case MBTYPE_INTER_8x8_REF0:
                MFX_INTERNAL_CPY(pCodRIxL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodRIxL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL0, pCodTemplate, sizeof(pCodTemplate[0])*16);
                MFX_INTERNAL_CPY(pCodMVdL1, pCodTemplate, sizeof(pCodTemplate[0])*16);
                {
                for (i = 0; i < 4; i ++)
                {
                    j = subblock_block_mapping[i];

                    int32_t sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[i] < D_DIR_DIRECT) ?
                        m_cur_mb.GlobalMacroblockInfo->sbtype[i] : SBTYPE_DIRECT;

                    switch (sbtype)
                    {
                    case SBTYPE_8x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = CodAbov;
                        pCodMVdL1[j + 4] = CodAbov;
                        break;
                    case SBTYPE_8x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = cL0;
                        pCodMVdL1[j + 4] = cL1;
                        break;
                    case SBTYPE_4x8:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodMVdL0[j] = cL0;
                        pCodMVdL1[j] = cL1;
                        pCodMVdL0[j + 1] = cL0;
                        pCodMVdL1[j + 1] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = CodAbov;
                        pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = CodAbov;
                        break;
                    case SBTYPE_4x4:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_BWD) ? CodNone : CodInBS;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_FWD) ? CodNone : CodInBS;
                        pCodRIxL0[j] = cL0;
                        pCodRIxL1[j] = cL1;
                        pCodRIxL0[j + 4] = CodAbov;
                        pCodRIxL1[j + 4] = CodAbov;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    case SBTYPE_DIRECT:
                        cL0 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_BWD) ? CodNone : CodSkip;
                        cL1 = (m_cur_mb.LocalMacroblockInfo->sbdir[i] == D_DIR_DIRECT_SPATIAL_FWD) ? CodNone : CodSkip;
                        pCodRIxL0[j] = pCodRIxL0[j + 1] = pCodRIxL0[j + 4] = pCodRIxL0[j + 5] = cL0;
                        pCodRIxL1[j] = pCodRIxL1[j + 1] = pCodRIxL1[j + 4] = pCodRIxL1[j + 5] = cL1;
                        pCodMVdL0[j] = pCodMVdL0[j + 1] = pCodMVdL0[j + 4] = pCodMVdL0[j + 5] = cL0;
                        pCodMVdL1[j] = pCodMVdL1[j + 1] = pCodMVdL1[j + 4] = pCodMVdL1[j + 5] = cL1;
                        break;
                    default:
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                // get all mvd_L0
                ReconstructMotionVectors4x4(BlkOrder,
                                            pMVdL0,
                                            0);

                // get all mvd_L1
                ReconstructMotionVectors4x4(BlkOrder,
                                            pMVdL1,
                                            1);
                break;
                }
            default:
                VM_ASSERT(false);
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
        }    // switch

    }
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors(void)

bool IsColocatedSame(uint32_t cur_pic_struct, uint32_t ref_pic_struct)
{
    if (cur_pic_struct == ref_pic_struct && cur_pic_struct != AFRM_STRUCTURE)
        return true;

    return false;
}

void H264SegmentDecoderMultiThreaded::ReconstructDirectMotionVectorsSpatial(bool isDirectMB)
{
    if (!m_pRefPicList[1][0])
    {
        memset(m_cur_mb.GetReferenceIndexStruct(0)->refIndexs, 0, 4);
        memset(m_cur_mb.GetReferenceIndexStruct(1)->refIndexs, 0, 4);
        fill_n<H264DecoderMotionVector>(m_cur_mb.MVs[0]->MotionVectors, 16, zeroVector);
        fill_n<H264DecoderMotionVector>(m_cur_mb.MVs[1]->MotionVectors, 16, zeroVector);
        return;
    }

    H264DecoderMotionVector  mvL0, mvL1;
    int32_t field = m_pFields[1][0].field;
    bool bL1RefPicisShortTerm = m_pFields[1][0].isShortReference;

    // set up pointers to where MV and RefIndex will be stored
    H264DecoderMotionVector *pFwdMV = m_cur_mb.MVs[0]->MotionVectors;
    H264DecoderMotionVector *pBwdMV = m_cur_mb.MVs[1]->MotionVectors;

    int32_t bAll4x4AreSame;
    int32_t bAll8x8AreSame = 0;

    int32_t refIdxL0, refIdxL1;

    // find the reference indexes
    ComputeDirectSpatialRefIdx(&refIdxL0, &refIdxL1);
    RefIndexType RefIndexL0 = (RefIndexType)refIdxL0;
    RefIndexType RefIndexL1 = (RefIndexType)refIdxL1;

    // set up reference index array
    {
        RefIndexType *pFwdRefInd = m_cur_mb.GetReferenceIndexStruct(0)->refIndexs;
        RefIndexType *pBwdRefInd = m_cur_mb.GetReferenceIndexStruct(1)->refIndexs;

        if (0 <= (RefIndexL0 & RefIndexL1))
        {
            memset(pFwdRefInd, RefIndexL0, 4);
            memset(pBwdRefInd, RefIndexL1, 4);
        }
        else
        {
            memset(pFwdRefInd, 0, 4);
            memset(pBwdRefInd, 0, 4);
        }
    }

    // Because predicted MV is computed using 16x16 block it is likely
    // that all 4x4 blocks will use the same MV and reference frame.
    // It is possible, however, for the MV for any 4x4 block to be set
    // to 0,0 instead of the computed MV. This possibility by default
    // forces motion compensation to be performed for each 4x4, the slowest
    // possible option. These booleans are used to detect when all of the
    // 4x4 blocks in an 8x8 can be combined for motion comp, and even better,
    // when all of the 8x8 blocks in the macroblock can be combined.

    // Change mbtype to any INTER 16x16 type, for computeMV function,
    // required for the 8x8 DIRECT case to force computeMV to get MV
    // using 16x16 type instead.
    m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_FORWARD;

    if (RefIndexL0 != -1) // Forward MV (L0)
    {
        // L0 ref idx exists, use to obtain predicted L0 motion vector
        // for the macroblock
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL0);
        else
            ReconstructMVPredictorExternalBlockMBAFF(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL0);
    }
    else
    {
        mvL0 = zeroVector;
    }

    if (RefIndexL1 != -1)    // Backward MV (L1)
    {
        // L0 ref idx exists, use to obtain predicted L0 motion vector
        // for the macroblock
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(1, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL1);
        else
            ReconstructMVPredictorExternalBlockMBAFF(1, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvL1);
    }
    else
    {
        mvL1 = zeroVector;
    }

    if (mvL0.mvy > m_MVDistortion[0])
        m_MVDistortion[0] = mvL0.mvy;

    if (mvL1.mvy > m_MVDistortion[1])
        m_MVDistortion[1] = mvL1.mvy;

    // set direction for MB
    uint32_t uPredDir;    // prediction direction of macroblock
    if ((0 <= RefIndexL0) && (0 > RefIndexL1))
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_FWD;
        m_cur_mb.GlobalMacroblockInfo->mbtype= MBTYPE_FORWARD;
    }
    else if ((0 > RefIndexL0) && (0 <= RefIndexL1))
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_BWD;
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BACKWARD;
    }
    else
    {
        uPredDir = D_DIR_DIRECT_SPATIAL_BIDIR;
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_BIDIR;
    }

    union UmvL
    {
        H264DecoderMotionVector mvL;
        uint32_t ippMvL;
    };

    UmvL uMvL0 = {mvL0};
    UmvL uMvL1 = {mvL1};

    if (isDirectMB && !uMvL0.ippMvL && !uMvL1.ippMvL)
    {
        fill_n<H264DecoderMotionVector>(pFwdMV, 16, mvL0);
        fill_n<H264DecoderMotionVector>(pBwdMV, 16, mvL1);
        return;
    }

    H264DecoderFrameEx *firstRefBackFrame = (H264DecoderFrameEx *)m_pRefPicList[1][0];
    bool allColocatedSame = IsColocatedSame(m_pCurrentFrame->m_PictureStructureForDec, firstRefBackFrame->m_PictureStructureForDec);

    // In loops below, set MV and RefIdx for all subblocks. Conditionally
    // change MV to 0,0 and RefIndex to 0 (doing so called UseZeroPred here).
    // To select UseZeroPred for a part:
    //  (RefIndexLx < 0) ||
    //  (bL1RefPicisShortTerm && RefIndexLx==0 && ColocatedRefIndex==0 &&
    //    (colocated motion vectors in range -1..+1)
    // When both RefIndexLx are -1, ZeroPred is used and both RefIndexLx
    // are changed to zero.

    // It is desirable to avoid checking the colocated motion vectors and
    // colocated ref index, bools and orders of conditional testing are
    // set up to do so.

    // At the MB level, the colocated do not need to be checked if:
    //  - both RefIndexLx < 0
    //  - colocated is INTRA (know all colocated RefIndex are -1)
    //  - L1 Ref Pic is not short term
    // set bMaybeUseZeroPred to true if any of the above are false

    // Set MV for all DIRECT 4x4 subblocks
    int32_t sb, sbcolpartoffset;
    int8_t colocatedRefIndex;
    H264DecoderMotionVector *pRefMV; // will be colocated L0 or L1
    int32_t MBsCol[4];
    int32_t sbColPartOffset[4];

    if (m_IsUseDirect8x8Inference)
    {
        sbColPartOffset[0] = 0;
        sbColPartOffset[2] = 12;
        MBsCol[0] = GetColocatedLocation(firstRefBackFrame, field, sbColPartOffset[0]);
        sbColPartOffset[1] = sbColPartOffset[0] + 3;
        sbColPartOffset[3] = sbColPartOffset[2] + 3;
    }
    else
    {
        sbColPartOffset[0] = 0;
        sbColPartOffset[2] = 8;
        MBsCol[0] = GetColocatedLocation(firstRefBackFrame, field, sbColPartOffset[0]);
        sbColPartOffset[1] = sbColPartOffset[0] + 2;
        sbColPartOffset[3] = sbColPartOffset[2] + 2;
    }

    uint32_t MBCol = MBsCol[0];
    H264DecoderMotionVector *pRefMVL0 = firstRefBackFrame->m_mbinfo.MV[0][MBCol].MotionVectors;
    H264DecoderMotionVector *pRefMVL1 = firstRefBackFrame->m_mbinfo.MV[1][MBCol].MotionVectors;
    bool isInter = IS_INTER_MBTYPE(firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype);
    RefIndexType * refIndexes = GetRefIdxs(&firstRefBackFrame->m_mbinfo, 0, MBCol);
    bool isNeedToCheckMVSBase = isInter && bL1RefPicisShortTerm && ((RefIndexL0 != -1) || (RefIndexL1 != -1));

    bool isAll4x4RealSame1[4];
    if (allColocatedSame)
    {
        switch(firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype)
        {
        case MBTYPE_INTER_16x8:
        case MBTYPE_INTER_8x16:
            isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            break;
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = false;
            break;

        case MBTYPE_INTRA:
        case MBTYPE_INTRA_16x16:
        case MBTYPE_PCM:
            if (isDirectMB)
            {
                fill_n<H264DecoderMotionVector>(pFwdMV, 16, mvL0);
                fill_n<H264DecoderMotionVector>(pBwdMV, 16, mvL1);
                return;
            }
            else
            {
                isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            }
            break;

        default: // 16x16
            if (isDirectMB)
            {
                colocatedRefIndex = GetReferenceIndex(refIndexes, 0);
                if (colocatedRefIndex >= 0)
                {
                    // use L0 colocated
                    pRefMV = pRefMVL0;
                }
                else
                {
                    // use L1 colocated
                    colocatedRefIndex = GetReferenceIndex(&firstRefBackFrame->m_mbinfo, 1, MBCol, 0);
                    pRefMV = pRefMVL1;
                }

                bool isNeedToCheckMVS = isNeedToCheckMVSBase && (colocatedRefIndex == 0);

                bool bUseZeroPredL0 = false;
                bool bUseZeroPredL1 = false;

                if (isNeedToCheckMVS)
                {
                    VM_ASSERT(pRefMV);
                    if (pRefMV[0].mvx >= -1 &&
                        pRefMV[0].mvx <= 1 &&
                        pRefMV[0].mvy >= -1 &&
                        pRefMV[0].mvy <= 1)
                    {
                        // All subpart conditions for forcing zero pred are met,
                        // use final RefIndexLx==0 condition for L0,L1.
                        bUseZeroPredL0 = (RefIndexL0 == 0);
                        bUseZeroPredL1 = (RefIndexL1 == 0);
                    }
                }

                fill_n<H264DecoderMotionVector>(pFwdMV, 16, bUseZeroPredL0 ? zeroVector : mvL0);
                fill_n<H264DecoderMotionVector>(pBwdMV, 16, bUseZeroPredL1 ? zeroVector : mvL1);
                return;
            }
            else
            {
                isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
            }
            break;
        };
    }
    else
    {
        MBsCol[1] = MBsCol[0];
        MBsCol[2] = GetColocatedLocation(firstRefBackFrame, field, sbColPartOffset[2]);
        MBsCol[3] = MBsCol[2];
        sbColPartOffset[3] = sbColPartOffset[2] + (m_IsUseDirect8x8Inference ? 3 : 2);
        isAll4x4RealSame1[0] = isAll4x4RealSame1[1] = isAll4x4RealSame1[2] = isAll4x4RealSame1[3] = true;
    }

    for (sb = 0; sb < 4; sb++)
    {
        if (m_cur_mb.GlobalMacroblockInfo->sbtype[sb] != SBTYPE_DIRECT)
        {
            bAll8x8AreSame = 17;
            continue;
        }

        // DIRECT 8x8 block
        bAll4x4AreSame = 0;

        if (!allColocatedSame)
        {
            MBCol = MBsCol[sb];
            pRefMVL0 = firstRefBackFrame->m_mbinfo.MV[0][MBCol].MotionVectors;
            pRefMVL1 = firstRefBackFrame->m_mbinfo.MV[1][MBCol].MotionVectors;
            isInter = IS_INTER_MBTYPE(firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype);
            isNeedToCheckMVSBase = isInter && bL1RefPicisShortTerm && ((RefIndexL0 != -1) || (RefIndexL1 != -1));
            refIndexes = GetRefIdxs(&firstRefBackFrame->m_mbinfo, 0, MBCol);
        }

        sbcolpartoffset = sbColPartOffset[sb];

        colocatedRefIndex = GetReferenceIndex(refIndexes, sbcolpartoffset);
        if (colocatedRefIndex >= 0)
        {
            // use L0 colocated
            pRefMV = pRefMVL0;
        }
        else
        {
            // use L1 colocated
            colocatedRefIndex = GetReferenceIndex(&firstRefBackFrame->m_mbinfo, 1, MBCol, sbcolpartoffset);
            pRefMV = pRefMVL1;
        }

        bool isNeedToCheckMVS = isNeedToCheckMVSBase && (colocatedRefIndex == 0);

        if (isNeedToCheckMVS)
        {
            bool isAll4x4Same = isAll4x4RealSame1[sb];

            if (!allColocatedSame)
            {
                int32_t sbtype;
                switch(firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype)
                {
                case MBTYPE_INTER_8x8:
                case MBTYPE_INTER_8x8_REF0:
                    sbtype = firstRefBackFrame->m_mbinfo.mbs[MBCol].sbtype[sb];
                    isAll4x4Same = (sbtype == SBTYPE_8x8);
                    break;
                };
            }

            if (isAll4x4Same)
            {
                sbcolpartoffset = sbColPartOffset[sb];
                bool bUseZeroPredL0 = false;
                bool bUseZeroPredL1 = false;
                int32_t sbpartoffset = ((sb&2))*4+(sb&1)*2;

                VM_ASSERT(pRefMV);
                if (pRefMV[sbcolpartoffset].mvx >= -1 &&
                    pRefMV[sbcolpartoffset].mvx <= 1 &&
                    pRefMV[sbcolpartoffset].mvy >= -1 &&
                    pRefMV[sbcolpartoffset].mvy <= 1)
                {
                    // All subpart conditions for forcing zero pred are met,
                    // use final RefIndexLx==0 condition for L0,L1.
                    bUseZeroPredL0 = (RefIndexL0 == 0);
                    bUseZeroPredL1 = (RefIndexL1 == 0);
                    bAll4x4AreSame = 4;
                }

                storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[sbpartoffset],
                    bUseZeroPredL0 ? zeroVector : mvL0);

                storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[sbpartoffset],
                    bUseZeroPredL1 ? zeroVector : mvL1);
            }
            else
            {
                for (uint32_t ypos = 0; ypos < 2; ypos++)
                {
                    for (uint32_t xpos = 0; xpos < 2; xpos++)
                    {
                        sbcolpartoffset = m_IsUseDirect8x8Inference ? sbColPartOffset[sb] : sbColPartOffset[sb] + ypos*4 + xpos;
                        bool bUseZeroPredL0 = false;
                        bool bUseZeroPredL1 = false;
                        int32_t sbpartoffset = (ypos+(sb&2))*4+(sb&1)*2;

                        VM_ASSERT(pRefMV);
                        if (pRefMV[sbcolpartoffset].mvx >= -1 &&
                            pRefMV[sbcolpartoffset].mvx <= 1 &&
                            pRefMV[sbcolpartoffset].mvy >= -1 &&
                            pRefMV[sbcolpartoffset].mvy <= 1)
                        {
                            // All subpart conditions for forcing zero pred are met,
                            // use final RefIndexLx==0 condition for L0,L1.
                            bUseZeroPredL0 = (RefIndexL0 == 0);
                            bUseZeroPredL1 = (RefIndexL1 == 0);
                            bAll4x4AreSame ++;
                        }

                        pFwdMV[xpos + sbpartoffset] = bUseZeroPredL0 ? zeroVector : mvL0;
                        pBwdMV[xpos + sbpartoffset] = bUseZeroPredL1 ? zeroVector : mvL1;
                    }   // xpos
                }   // ypos // next row of subblocks
            }
        }
        else
        {
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pFwdMV[subblock_block_mapping[sb]], mvL0);
            storeStructInformationInto8x8<H264DecoderMotionVector>(&pBwdMV[subblock_block_mapping[sb]], mvL1);
        }

        // set direction for 8x8 block
        m_cur_mb.LocalMacroblockInfo->sbdir[sb] = (uint8_t)uPredDir;

        // set type for 8x8 block
        if (!bAll4x4AreSame || bAll4x4AreSame == 4)
            m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_8x8;
        else
            m_cur_mb.GlobalMacroblockInfo->sbtype[sb] = SBTYPE_4x4;

        bAll8x8AreSame += bAll4x4AreSame;
    }   // for sb

    // set mbtype to 8x8 if it was not; use larger type if possible
    if (bAll8x8AreSame && bAll8x8AreSame != 16)
    {
        m_cur_mb.GlobalMacroblockInfo->mbtype = MBTYPE_INTER_8x8;

        if (allColocatedSame && isDirectMB &&
            (firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype == MBTYPE_INTER_16x8 ||
            firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype == MBTYPE_INTER_8x16))
        {
            m_cur_mb.GlobalMacroblockInfo->mbtype = firstRefBackFrame->m_mbinfo.mbs[MBCol].mbtype;
            m_cur_mb.LocalMacroblockInfo->sbdir[1] = m_cur_mb.LocalMacroblockInfo->sbdir[3];
        }
    }
} // void H264SegmentDecoderMultiThreaded::ReconstructDirectMotionVectorsSpatial(H264DecoderFrame **

void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const uint8_t *pBlkIdx,
                                                        const uint8_t *pCodMVd,
                                                        uint32_t ListNum)
{
    uint32_t i, j, m = 0;

    // new
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

    for (i = 0; i < 16; i ++)
    {
        j = pBlkIdx[i];

        if ((m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
             m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) &&
            !(i & 3))
            m = i;

        // new
        H264DecoderMotionVector mvd;

        switch (pCodMVd[j])
        {
        case CodNone:
            pMVd[j] = zeroVector;
            m++;
            break;

        case CodInBS:
            mvd = GetSE_MVD_CABAC(ListNum, j);
            pMVd[j] = mvd;
            break;

        case CodLeft:
            pMVd[j] = pMVd[j - 1];
            break;

        case CodAbov:
            pMVd[j]  = pMVd[j - 4];
            break;

        case CodSkip:
            pMVd[j] = zeroVector;
            break;
        }
    } // for i
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const uint8_t *pBlkIdx,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const uint8_t *pBlkIdx,
                                                                    const uint8_t *pCodMVd,
                                                                    uint32_t ListNum)
{
    uint32_t i, j, m = 0;

    // new
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    // old

    for (i = 0; i < 16; i ++)
    {
        j = pBlkIdx[i];

        if ((m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
             m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0) &&
            !(i & 3))
            m = i;

        // new
        H264DecoderMotionVector mv;
        H264DecoderMotionVector mvd;

        switch (pCodMVd[j])
        {
        case CodNone:
            pMV[j] = zeroVector;
            m++;
            break;

        case CodInBS:
            // TBD optimization - modify to compute predictors here instead of calling this method
            ComputeMotionVectorPredictors((uint8_t) ListNum,
                                          m_cur_mb.GetReferenceIndex(ListNum, j),
                                          m++,
                                          &mv);
            mvd = pMVd[j];
            // new & old
            mv.mvx = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
            mv.mvy = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

            if (mv.mvy > m_MVDistortion[ListNum])
                m_MVDistortion[ListNum] = mv.mvy;

            // new
            pMV[j] = mv;
            break;

        case CodLeft:
            pMV[j] = pMV[j - 1];
            break;

        case CodAbov:
            pMV[j]  = pMV[j - 4];
            break;

        case CodSkip:
            break;
        }
    } // for i
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const uint8_t *pBlkIdx,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const uint8_t pCodMVd,
                                                        uint32_t ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd == CodNone)
    {
        memset(&pMVd[0], 0, 16*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (int32_t k = 0; k < 16; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_CABAC(const uint8_t pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const uint8_t pCodMVd,
                                                                    uint32_t ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd == CodNone)
    {
        memset(&pMV[0],0,16*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mv;

        ComputeMotionVectorPredictors( (uint8_t) ListNum,
                                        pRIx[0],
                                        0, &mv);

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
        mv.mvy = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (int32_t k = 0; k < 16; k ++)
        {
            pMV[k] = mv;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors4x4(const uint8_t pCodMVd,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_16x8_CABAC(const uint8_t *pCodMVd,
                                                             uint32_t ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        memset(&pMVd[0],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (int32_t k = 0; k < 8; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }

    if(pCodMVd[8] == CodNone)
    {
        memset(&pMVd[8],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 8);

        for (int32_t k = 8; k < 16; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_16x8_CABAC(const uint8_t *pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors16x8(const uint8_t *pCodMVd,
                                                                     uint32_t ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        memset(&pMV[0],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mv;

        ComputeMotionVectorPredictors( (uint8_t) ListNum,
                                        pRIx[0],
                                        0, &mv);

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx  = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
        mv.mvy  = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (int32_t k = 0; k < 8; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

    if(pCodMVd[8] == CodNone)
    {
        memset(&pMV[8],0,8*sizeof(H264DecoderMotionVector));
    }
    else
    {
        H264DecoderMotionVector mv;
        ComputeMotionVectorPredictors( (uint8_t) ListNum,
                                        pRIx[2],
                                        1, &mv);

        H264DecoderMotionVector mvd = pMVd[8];
        mv.mvx  = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
        mv.mvy  = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (int32_t k = 8; k < 16; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs16x8(const uint8_t *pCodMVd,

void H264SegmentDecoderMultiThreaded::GetMVD4x4_8x16_CABAC(const uint8_t *pCodMVd,
                                                             uint32_t ListNum)
{
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        pMVd[0] = pMVd[1] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 0);

        for (int32_t k = 0; k < 2; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }

    if(pCodMVd[2] == CodNone)
    {
        pMVd[3] = pMVd[2] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mvd = GetSE_MVD_CABAC(ListNum, 2);

        for (int32_t k = 2; k < 4; k ++)
        {
            pMVd[k] = mvd;
        } // for k
    }
    MFX_INTERNAL_CPY(&pMVd[4], &pMVd[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMVd[8], &pMVd[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMVd[12], &pMVd[0], 4*sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::GetMVD4x4_8x16_CABAC(const uint8_t *pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructMotionVectors8x16(const uint8_t *pCodMVd,
                                                                     uint32_t ListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[ListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[ListNum]->MotionVectors;
    RefIndexType *pRIx = m_cur_mb.GetReferenceIndexStruct(ListNum)->refIndexs;

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif
    if(pCodMVd[0] == CodNone)
    {
        pMV[0] = pMV[1] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mv;

        ComputeMotionVectorPredictors( (uint8_t) ListNum,
                                        pRIx[0],
                                        0, &mv);

        H264DecoderMotionVector mvd = pMVd[0];
        mv.mvx  = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
        mv.mvy  = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (int32_t k = 0; k < 2; k ++)
        {
            pMV[k] = mv;
        } // for k
    }

    if(pCodMVd[2] == CodNone)
    {
        pMV[2] = pMV[3] = zeroVector;
    }
    else
    {
        H264DecoderMotionVector mv;

        ComputeMotionVectorPredictors( (uint8_t) ListNum,
                                        pRIx[1],
                                        1, &mv);

        H264DecoderMotionVector mvd = pMVd[2];
        mv.mvx  = (int16_t) (mv.mvx + (int32_t) mvd.mvx);
        mv.mvy  = (int16_t) (mv.mvy + (int32_t) mvd.mvy);

        if (mv.mvy > m_MVDistortion[ListNum])
            m_MVDistortion[ListNum] = mv.mvy;

        for (int32_t k = 2; k < 4; k ++)
        {
            pMV[k] = mv;
        } // for k
    }
    MFX_INTERNAL_CPY(&pMV[4], &pMV[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMV[8], &pMV[0], 4*sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(&pMV[12], &pMV[0], 4*sizeof(H264DecoderMotionVector));

} // void Status H264SegmentDecoderMultiThreaded::ReconstructMotionVectors8x16(const uint8_t* pCodMVd,

void H264SegmentDecoderMultiThreaded::ReconstructSkipMotionVectors(void)
{
    // check "zero motion vectors" condition
    H264DecoderBlockLocation mbAddrA, mbAddrB;

    // select neighbours
    mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[0];
    mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;

    if ((0 <= mbAddrA.mb_num) &&
        (0 <= mbAddrB.mb_num))
    {
        int32_t refIdxA, refIdxB;

        // select motion vectors & reference indexes
        H264DecoderMotionVector mvA = GetMV(m_gmbinfo, 0, mbAddrA.mb_num, mbAddrA.block_num);
        H264DecoderMotionVector mvB = GetMV(m_gmbinfo, 0, mbAddrB.mb_num, mbAddrB.block_num);
        refIdxA = GetReferenceIndex(m_gmbinfo, 0, mbAddrA.mb_num, mbAddrA.block_num);
        refIdxB = GetReferenceIndex(m_gmbinfo, 0, mbAddrB.mb_num, mbAddrB.block_num);

        // adjust vectors & reference in the MBAFF case
        if (m_isMBAFF)
        {
            int32_t iCurStruct = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
            int32_t iNeighbourStruct;

            iNeighbourStruct = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrA.mb_num]);
            mvA.mvy <<= iNeighbourStruct;
            refIdxA <<= (1 - iNeighbourStruct);

            iNeighbourStruct = GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrB.mb_num]);
            mvB.mvy <<= iNeighbourStruct;
            refIdxB <<= (1 - iNeighbourStruct);

            if (iCurStruct)
            {
                mvA.mvy /= 2;
                mvB.mvy /= 2;
            }
            else
            {
                refIdxA >>= 1;
                refIdxB >>= 1;
            }
        }

        // check condition
        if ((mvA.mvx | mvA.mvy | refIdxA) &&
            (mvB.mvx | mvB.mvy | refIdxB))
        {
            H264DecoderMotionVector mvPred;

            if (false == m_isMBAFF)
                ReconstructMVPredictorExternalBlock(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvPred);
            else
                ReconstructMVPredictorExternalBlockMBAFF(0, m_cur_mb.CurrentBlockNeighbours.mb_above_right, &mvPred);

            if (mvPred.mvy > m_MVDistortion[0])
                m_MVDistortion[0] = mvPred.mvy;

            // fill motion vectors
            for (int32_t i = 0; i < 16; i += 1)
                m_cur_mb.MVs[0]->MotionVectors[i] = mvPred;

            return;
        }
    }

    // in oposite case we clean motion vectors
    memset((void *) m_cur_mb.MVs[0], 0, sizeof(H264DecoderMacroblockMVs));
} // void H264SegmentDecoderMultiThreaded::ReconstructSkipMotionVectors(void)

inline
int32_t CreateMVFromCode(int32_t code)
{
    int32_t val, sign;

    val = (code + 1) >> 1;
    sign = (code & 1) - 1;
    val ^= sign;
    val -= sign;

    return val;

} // int32_t CreateMVFromCode(int32_t code)

#define DecodeMVDelta_CAVLC(list_num, block_num) \
{ \
    H264DecoderMotionVector mvD; \
    mvD.mvx = (int16_t) CreateMVFromCode(m_pBitStream->GetVLCElement(false)); \
    mvD.mvy = (int16_t) CreateMVFromCode(m_pBitStream->GetVLCElement(false)); \
    m_cur_mb.MVDelta[list_num]->MotionVectors[block_num] = mvD; \
}

void H264SegmentDecoderMultiThreaded::DecodeMotionVectorsPSlice_CAVLC(void)
{
    if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
        && (m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD >= 0))
    {
        return;
    }

    int32_t mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;

    switch (mbtype)
    {
    case MBTYPE_FORWARD:
        {
            // decode reference indexes
            {
                int32_t num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                                               pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
                int32_t refIdx = 0;

                if (2 == num_ref_idx_l0_active)

                    refIdx = m_pBitStream->Get1Bit() ^ 1;
                else if (2 < num_ref_idx_l0_active)
                    refIdx = m_pBitStream->GetVLCElement(false);

                if (refIdx >= num_ref_idx_l0_active || refIdx < 0)
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }

                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                memset(refs->refIndexs, refIdx, sizeof(RefIndexType) * 4);
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
        }
        break;

    case MBTYPE_INTER_16x8:
        {
            // decode reference indexes
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                int32_t num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                                               pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
                if (2 == num_ref_idx_l0_active)
                {
                    refs->refIndexs[0] =
                    refs->refIndexs[1] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                    refs->refIndexs[2] =
                    refs->refIndexs[3] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                }
                else if (2 < num_ref_idx_l0_active)
                {
                    refs->refIndexs[0] =
                    refs->refIndexs[1] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                    refs->refIndexs[2] =
                    refs->refIndexs[3] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                }
                else
                    memset(refs, 0, sizeof(H264DecoderMacroblockRefIdxs));

                if (refs->refIndexs[1] >= num_ref_idx_l0_active || refs->refIndexs[1] < 0 || refs->refIndexs[3] >= num_ref_idx_l0_active || refs->refIndexs[3] < 0)
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
            DecodeMVDelta_CAVLC(0, 8);
        }
        break;

    case MBTYPE_INTER_8x16:
        {
            // decode reference indexes
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);
                int32_t num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                                               pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
                if (2 == num_ref_idx_l0_active)
                {
                    refs->refIndexs[0] =
                    refs->refIndexs[2] = (RefIndexType) (m_pBitStream->Get1Bit() ^ 1);
                    refs->refIndexs[1] =
                    refs->refIndexs[3] = (RefIndexType) (m_pBitStream->Get1Bit() ^ 1);
                }
                else if (2 < num_ref_idx_l0_active)
                {
                    refs->refIndexs[0] =
                    refs->refIndexs[2] = (RefIndexType) m_pBitStream->GetVLCElement(false);
                    refs->refIndexs[1] =
                    refs->refIndexs[3] = (RefIndexType) m_pBitStream->GetVLCElement(false);
                }
                else
                    memset(refs, 0, sizeof(H264DecoderMacroblockRefIdxs));

                if (refs->refIndexs[2] >= num_ref_idx_l0_active || refs->refIndexs[2] < 0 || refs->refIndexs[3] >= num_ref_idx_l0_active || refs->refIndexs[3] < 0)
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
            }

            // decode motion vector deltas
            DecodeMVDelta_CAVLC(0, 0);
            DecodeMVDelta_CAVLC(0, 2);
        }
        break;

        // 8x8 devision and smaller
    default:
        {
            int8_t *pSubBlockType = m_cur_mb.GlobalMacroblockInfo->sbtype;
            int8_t *pSubDirType = m_cur_mb.LocalMacroblockInfo->sbdir;

            if ((SBTYPE_8x8 == pSubBlockType[0]) &&
                (SBTYPE_8x8 == pSubBlockType[1]) &&
                (SBTYPE_8x8 == pSubBlockType[2]) &&
                (SBTYPE_8x8 == pSubBlockType[3]) &&

                (pSubDirType[0] < D_DIR_DIRECT) &&
                (pSubDirType[1] < D_DIR_DIRECT) &&
                (pSubDirType[2] < D_DIR_DIRECT) &&
                (pSubDirType[3] < D_DIR_DIRECT) )
            {
                H264DecoderMacroblockRefIdxs * refs = m_cur_mb.GetReferenceIndexStruct(0);

                // decode reference indexes
                {
                    int32_t num_ref_idx_l0_active = m_pSliceHeader->num_ref_idx_l0_active <<
                                                   pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);

                    if ((MBTYPE_INTER_8x8 == mbtype) &&
                        (1 < num_ref_idx_l0_active))
                    {
                        if (2 == num_ref_idx_l0_active)
                        {
                            refs->refIndexs[0] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                            refs->refIndexs[1] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                            refs->refIndexs[2] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                            refs->refIndexs[3] = (RefIndexType)(m_pBitStream->Get1Bit() ^ 1);
                        }
                        else
                        {
                            refs->refIndexs[0] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                            refs->refIndexs[1] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                            refs->refIndexs[2] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                            refs->refIndexs[3] = (RefIndexType)m_pBitStream->GetVLCElement(false);
                        }
                    }
                    else
                        memset(refs, 0, sizeof(H264DecoderMacroblockRefIdxs));

                    for (int i = 0; i < 4; i++)
                    {
                        if (refs->refIndexs[i] >= num_ref_idx_l0_active || refs->refIndexs[i] < 0)
                        {
                            throw h264_exception(UMC_ERR_INVALID_STREAM);
                        }
                    }
                }

                // decode motion vector deltas
                DecodeMVDelta_CAVLC(0, 0);
                DecodeMVDelta_CAVLC(0, 2);
                DecodeMVDelta_CAVLC(0, 8);
                DecodeMVDelta_CAVLC(0, 10);

            }
            else
            {
                DecodeMotionVectors_CAVLC(false);
                return;
            }
        }
        break;
    };

    ReconstructMotionVectors();
    AdujstMvsAndType();
} // void H264SegmentDecoderMultiThreaded::DecodeMotionVectorsPSlice_CAVLC(void)

void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CAVLC(bool bIsBSlice)
{
    if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] > D_DIR_BIDIR)
        && (m_cur_mb.GlobalMacroblockInfo->mbtype - MBTYPE_FORWARD >= 0))
    {
        return;
    }

    int32_t num_vectorsL0, block, subblock, i;
    int32_t mvdx_l0, mvdy_l0, mvdx_l1 = 0, mvdy_l1 = 0;
    int32_t num_vectorsL1;
    int32_t dec_vectorsL0;
    int32_t dec_vectorsL1;
    int32_t num_refIxL0;
    int32_t num_refIxL1;
    int32_t dec_refIxL0;
    int32_t dec_refIxL1;
    uint32_t uVLCCodes[64+MAX_NUM_REF_FRAMES*2];  // sized for max possible:
                                                //  16 MV * 2 codes per MV * 2 directions
                                                //  plus L0 and L1 ref indexes
    uint32_t *pVLCCodes;        // pointer to pass to VLC function
    uint32_t *pVLCCodesL0MV;
    uint32_t *pVLCCodesL1MV;
    uint32_t *pVLCCodesL0RefIndex;
    uint32_t *pVLCCodesL1RefIndex;

    uint32_t dirPart;    // prediction direction of current MB partition
    int32_t numParts;
    int32_t partCtr;
    int8_t RefIxL0 = 0;
    int8_t RefIxL1 = 0;
    RefIndexType *pRefIndexL0 = 0;
    RefIndexType *pRefIndexL1 = 0;

    H264DecoderMotionVector *pMVDeltaL0 = 0;
    H264DecoderMotionVector *pMVDeltaL1 = 0;

    int32_t uNumRefIdxL0Active;
    int32_t uNumRefIdxL1Active;
    int32_t uNumVLCCodes;

    switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
    {
    case MBTYPE_FORWARD:
        num_vectorsL0 = 1;
        num_refIxL0 = 1;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        dirPart = D_DIR_FWD;
        numParts = 1;
        break;
    case MBTYPE_BACKWARD:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 1;
        num_refIxL1 = 1;
        dirPart = D_DIR_BWD;
        numParts = 1;
        break;
    case MBTYPE_BIDIR:
        num_vectorsL0 = 1;
        num_refIxL0 = 1;
        num_vectorsL1 = 1;
        num_refIxL1 = 1;
        dirPart = D_DIR_BIDIR;
        numParts = 1;
        break;
    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        numParts = 0;
        for (block = 0; block<4; block++)
        {
            int32_t sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[block] < D_DIR_DIRECT) ?
                m_cur_mb.GlobalMacroblockInfo->sbtype[block] : SBTYPE_DIRECT;
            switch (sbtype)
            {
            case SBTYPE_8x8:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0++;
                    num_refIxL0++;
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1++;
                    num_refIxL1++;
                }
                numParts++;
                break;
            case SBTYPE_8x4:
            case SBTYPE_4x8:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0 += 2;
                    num_refIxL0++;        // only one per refIx per 8x8
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1 += 2;
                    num_refIxL1++;
                }
                numParts += 2;
                break;
            case SBTYPE_4x4:
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_FWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL0 += 4;
                    num_refIxL0++;
                }
                if ((m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BWD) ||
                    (m_cur_mb.LocalMacroblockInfo->sbdir[block] == D_DIR_BIDIR))
                {
                    num_vectorsL1 += 4;
                    num_refIxL1++;
                }
                numParts += 4;
                break;
            case SBTYPE_DIRECT:
                numParts++;
                break;
            default:
                throw h264_exception(UMC_ERR_INVALID_STREAM);
                break;
            }
        }
        dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[0];
        break;
    case MBTYPE_INTER_16x8:
    case MBTYPE_INTER_8x16:
        num_vectorsL0 = 0;
        num_refIxL0 = 0;
        num_vectorsL1 = 0;
        num_refIxL1 = 0;
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_FWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR))
        {
            num_vectorsL0++;
            num_refIxL0++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[0] == D_DIR_BIDIR))
        {
            num_vectorsL1++;
            num_refIxL1++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_FWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR))
        {
            num_vectorsL0++;
            num_refIxL0++;
        }
        if ((m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BWD) || (m_cur_mb.LocalMacroblockInfo->sbdir[1] == D_DIR_BIDIR))
        {
            num_vectorsL1++;
            num_refIxL1++;
        }
        numParts = 2;
        dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[0];
        break;
    default:
        VM_ASSERT(false);
        return; // DEBUG: may be throw ???
    }

    // Get all of the reference index and MV VLC codes from the bitstream
    // The bitstream contains them in the following order, which is the
    // order they are returned in uVLCCodes:
    //   L0 reference index for all MB parts using L0 prediction
    //   L1 reference index for all MB parts using L1 prediction
    //   L0 MV delta for all MB parts using L0 prediction
    //   L1 MV delta for all MB parts using L1 prediction
    // Reference index data is present only when the number of active
    // reference frames is greater than one. Also, MBTYPE_INTER_8x8_REF0
    // type MB has no ref index info in the bitstream (use 0 for all).
    uNumRefIdxL0Active = m_pSliceHeader->num_ref_idx_l0_active<<pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    uNumRefIdxL1Active = m_pSliceHeader->num_ref_idx_l1_active<<pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    if (uNumRefIdxL0Active == 1 || MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype)
        num_refIxL0 = 0;
    if (uNumRefIdxL1Active == 1 || MBTYPE_INTER_8x8_REF0 == m_cur_mb.GlobalMacroblockInfo->mbtype)
        num_refIxL1 = 0;

    // set pointers into VLC codes array
    pVLCCodes = uVLCCodes;
    pVLCCodesL0RefIndex = uVLCCodes;
    pVLCCodesL1RefIndex = pVLCCodesL0RefIndex + num_refIxL0;
    pVLCCodesL0MV = pVLCCodesL1RefIndex + num_refIxL1;
    pVLCCodesL1MV = pVLCCodesL0MV + num_vectorsL0*2;

    uNumVLCCodes = (num_vectorsL0+num_vectorsL1)*2 + num_refIxL0 + num_refIxL1;

    // When possible ref index range is 0..1, the reference index is coded
    // as 1 bit. When that occurs, get the ref index codes separate from
    // the motion vectors. Otherwise get all of the codes at once.
    if (uNumRefIdxL0Active >= 2 || uNumRefIdxL1Active >= 2)
    {
        // 1 bit codes for at least one set of ref index codes
        if (uNumRefIdxL0Active == 2)
        {
            for (i=0; i<num_refIxL0; i++)
            {
                pVLCCodesL0RefIndex[i] = !m_pBitStream->Get1Bit();
            }
        }
        else if (uNumRefIdxL0Active > 2)
        {
            for (i=0;i<num_refIxL0;i++)
            {
                pVLCCodesL0RefIndex[i]=m_pBitStream->GetVLCElement(false);
            }
        }

        if (uNumRefIdxL1Active == 2)
        {
            for (i=0; i<num_refIxL1; i++)
            {
                pVLCCodesL1RefIndex[i] = !m_pBitStream->Get1Bit();
            }
        }
        else if (uNumRefIdxL1Active > 2)
        {
            for (i=0;i<num_refIxL1;i++)
            {
                pVLCCodesL1RefIndex[i]=m_pBitStream->GetVLCElement(false);
            }
        }

        // to get the MV for the MB
        uNumVLCCodes -= num_refIxL0 + num_refIxL1;
        pVLCCodes = pVLCCodesL0MV;
    }

    // get all MV and possibly Ref Index codes for the MB
    for (i=0;i<uNumVLCCodes;i++,pVLCCodes++)
    {
        *pVLCCodes = m_pBitStream->GetVLCElement(false);
    }

    block = 0;
    subblock = 0;
    dec_vectorsL0 = 0;
    dec_vectorsL1 = 0;
    dec_refIxL0 = 0;
    dec_refIxL1 = 0;
    uint32_t sboffset;
    for (partCtr = 0,sboffset=0; partCtr < numParts; partCtr++)
    {
        pMVDeltaL0 = &m_cur_mb.MVDelta[0]->MotionVectors[sboffset];
        pRefIndexL0 = &m_cur_mb.GetReferenceIndexStruct(0)->refIndexs[subblock_block_membership[sboffset]];

        if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
        {    // L0
            if (num_refIxL0)
            {
                RefIxL0 = (int8_t)pVLCCodesL0RefIndex[dec_refIxL0];
                // dec_refIxL0 is incremented in mbtype-specific code below
                // instead of here because there is only one refIx for each
                // 8x8 block, so number of refIx values is not the same as
                // numParts.
                if (RefIxL0 >= (int8_t)uNumRefIdxL0Active || RefIxL0 < 0)
                {
                    throw h264_exception(UMC_ERR_INVALID_STREAM);
                }
            }
            else
                RefIxL0 = 0;

            mvdx_l0 = (pVLCCodesL0MV[dec_vectorsL0*2]+1) >> 1;
            if (!(pVLCCodesL0MV[dec_vectorsL0*2]&1))
                mvdx_l0 = -mvdx_l0;

            mvdy_l0 = (pVLCCodesL0MV[dec_vectorsL0*2+1]+1) >> 1;
            if (!(pVLCCodesL0MV[dec_vectorsL0*2+1]&1))
                mvdy_l0 = -mvdy_l0;
            dec_vectorsL0++;
        }    // L0
        else
        {
            mvdx_l0 = mvdy_l0 = 0;
            RefIxL0 = -1;
        }

        if (bIsBSlice)
        {
            pMVDeltaL1 = &m_cur_mb.MVDelta[1]->MotionVectors[sboffset];
            pRefIndexL1 = &m_cur_mb.GetReferenceIndexStruct(1)->refIndexs[subblock_block_membership[sboffset]];

            if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
            {
                if (num_refIxL1)
                {
                    RefIxL1 = (int8_t)pVLCCodesL1RefIndex[dec_refIxL1];
                    // dec_refIxL1 is incremented in mbtype-specific code below
                    // instead of here because there is only one refIx for each
                    // 8x8 block, so number of refIx values is not the same as
                    // numParts.
                    if (RefIxL1 >= (int8_t)uNumRefIdxL1Active || RefIxL1 < 0)
                    {
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    }
                }
                else
                    RefIxL1 = 0;

                mvdx_l1 = (pVLCCodesL1MV[dec_vectorsL1*2]+1)>>1;
                if (!(pVLCCodesL1MV[dec_vectorsL1*2]&1))
                    mvdx_l1 = -mvdx_l1;

                mvdy_l1 = (pVLCCodesL1MV[dec_vectorsL1*2+1]+1)>>1;
                if (!(pVLCCodesL1MV[dec_vectorsL1*2+1]&1))
                    mvdy_l1 = -mvdy_l1;
                dec_vectorsL1++;
            }    // L1
            else
            {
                mvdx_l1 = mvdy_l1 = 0;
                RefIxL1 = -1;
            }
        }    // B slice

        // Store motion vectors and reference indexes into this frame's buffers
        switch (m_cur_mb.GlobalMacroblockInfo->mbtype)
        {
        case MBTYPE_FORWARD:
        case MBTYPE_BACKWARD:
        case MBTYPE_BIDIR:

            pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
            pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

            fill_n(pRefIndexL0, 4, RefIxL0);

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                fill_n(pRefIndexL1, 4, RefIxL1);
            }
            break;
        case MBTYPE_INTER_16x8:
            pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
            pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

            // store in 8 top or bottom half blocks
            fill_n(pRefIndexL0, 2, RefIxL0);

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                fill_n(pRefIndexL1, 2, RefIxL1);

                if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL1++;
            }
            block++;
            sboffset=8;
            if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                dec_refIxL0++;
            dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block];        // next partition
            break;
        case MBTYPE_INTER_8x16:
            pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
            pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

            // store in 8 left or right half blocks
            pRefIndexL0[0] = RefIxL0;
            pRefIndexL0[2] = RefIxL0;

            if (bIsBSlice)
            {
                pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                // store in 8 left or right half blocks
                pRefIndexL1[0] = RefIxL1;
                pRefIndexL1[2] = RefIxL1;

                if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL1++;
            }
            block++;
            sboffset=2;
            if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                dec_refIxL0++;
            dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block];        // next partition
            break;
        case MBTYPE_INTER_8x8:
        case MBTYPE_INTER_8x8_REF0:
            {
                int32_t sbtype = (m_cur_mb.LocalMacroblockInfo->sbdir[block>>2] < D_DIR_DIRECT) ?
                    m_cur_mb.GlobalMacroblockInfo->sbtype[block>>2] : SBTYPE_DIRECT;

            switch (sbtype)
            {
            case SBTYPE_8x8:
                pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
                pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;

                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                    pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                if (block == 4)
                {
                    sboffset += 8 - 2;
                }
                else
                {
                    sboffset += 2;
                }
                block += 4;
                if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                    dec_refIxL0++;
                break;
            case SBTYPE_8x4:
                pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
                pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                    pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }

                if (subblock == 1)
                {
                    if (block == 4)
                    {
                        sboffset+=2;
                    }
                    else
                    {
                        sboffset-=2;
                    }
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                    sboffset  += 4;
                }
                break;
            case SBTYPE_4x8:
                pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
                pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                    pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }
                if (subblock == 1)
                {
                    if (block == 4)
                    {
                        sboffset += 8 - 3;
                    }
                    else
                    {
                        sboffset++;
                    }
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                    sboffset++;
                }
                break;
            case SBTYPE_4x4:
                pMVDeltaL0[0].mvx = (int16_t)mvdx_l0;
                pMVDeltaL0[0].mvy = (int16_t)mvdy_l0;

                pRefIndexL0[0] = RefIxL0;
                if (bIsBSlice)
                {
                    pMVDeltaL1[0].mvx = (int16_t)mvdx_l1;
                    pMVDeltaL1[0].mvy = (int16_t)mvdy_l1;

                    pRefIndexL1[0] = RefIxL1;
                }
                sboffset += (xyoff[block+subblock][0]>>2) +
                    ((xyoff[block+subblock][1])>>2)*4;
                if (subblock == 3)
                {
                    block += 4;
                    subblock = 0;
                    if (dirPart == D_DIR_FWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL0++;
                    if (dirPart == D_DIR_BWD || dirPart == D_DIR_BIDIR)
                        dec_refIxL1++;
                }
                else
                {
                    subblock++;
                }
                break;
            case SBTYPE_DIRECT:
                // nothing to do except advance to next 8x8 partition
                if (block == 4)
                {
                    sboffset += 8 - 2;
                }
                else
                {
                    sboffset += 2;
                }
                block += 4;
                break;
            }    // 8x8 switch sbtype
            }

            if (block<16)
                dirPart = m_cur_mb.LocalMacroblockInfo->sbdir[block>>2];        // next partition
            break;
        }    // switch mbtype
    }    // for partCtr

    ReconstructMotionVectors();
    return;
}  // void H264SegmentDecoderMultiThreaded::DecodeMotionVectors_CAVLC(bool bIsBSlice)

void H264SegmentDecoderMultiThreaded::DecodeDirectMotionVectors(bool isDirectMB)
{
    if (m_IsUseSpatialDirectMode)
    {
        ReconstructDirectMotionVectorsSpatial(isDirectMB);
    }   // temporal DIRECT
    else
    {
        // temporal DIRECT prediction
        if (!m_IsUseDirect8x8Inference)
        {
            DecodeDirectMotionVectorsTemporal(isDirectMB);
        }
        else
        {
            DecodeDirectMotionVectorsTemporal_8x8Inference();
        }
    }
}

Status H264SegmentDecoderMultiThreaded::ProcessSlice(int32_t iCurMBNumber, int32_t &iMBToProcess)
{
    Status umcRes = UMC_OK;
    int32_t iFirstMB = iCurMBNumber;
    int32_t iMBRowSize = m_pSlice->GetMBRowWidth();
    int32_t iMaxMBNumber = iCurMBNumber + iMBToProcess;

    bool bDoDeblocking;
    int32_t iFirstMBToDeblock;
    int32_t iAvailableMBToDeblock = 0;

     // set deblocking condition
    bDoDeblocking = m_pSlice->GetDeblockingCondition();
    iFirstMBToDeblock = iCurMBNumber;

    m_psBuffer = align_pointer<UMC::CoeffsPtrCommon> (m_pCoefficientsBuffer, DEFAULT_ALIGN_VALUE);

    // handle a slice group case
    if (m_isSliceGroups)
    {
        // perform decoding on current row(s)
        if (0 == m_pPicParamSet->entropy_coding_mode)
        {
            umcRes = m_SD->DecodeSegmentCAVLC_Single(iFirstMB, iMaxMBNumber, this);
        }
        else
        {
            umcRes = m_SD->DecodeSegmentCABAC_Single(iFirstMB, iMaxMBNumber, this);
        }

        if ((UMC_OK == umcRes) ||
            (UMC_ERR_END_OF_STREAM == umcRes))
            umcRes = UMC_ERR_END_OF_STREAM;

        iMBToProcess = m_CurMBAddr - iCurMBNumber;
        return umcRes;
    }

    m_CurMBAddr = iCurMBNumber;

    // this is a cicle for rows of MBs
    for (; iCurMBNumber < iMaxMBNumber;)
    {
        uint32_t nBorder;
        int32_t iPreCallMBNumber;

        // calculate the last MB in a row
        nBorder = MFX_MIN(iMaxMBNumber,
                      iCurMBNumber -
                      (iCurMBNumber % iMBRowSize) +
                      iMBRowSize);

        // perform the decoding on the current row
        iPreCallMBNumber = iCurMBNumber;

        try
        {
            if (0 == m_pPicParamSet->entropy_coding_mode)
            {
                umcRes = m_SD->DecodeSegmentCAVLC_Single(iCurMBNumber, nBorder, this);
            }
            else
            {
                umcRes = m_SD->DecodeSegmentCABAC_Single(iCurMBNumber, nBorder, this);
            }
        } catch(...)
        {
            iMBToProcess = m_CurMBAddr - iFirstMB;
            throw;
        }

        // sum count of MB to deblock
        iAvailableMBToDeblock += nBorder - iPreCallMBNumber;
        // check error(s)
        if ((UMC_OK != umcRes) ||
            (m_CurMBAddr >= iMaxMBNumber))
            break;

        // correct the MB number in a MBAFF case
        iCurMBNumber = iPreCallMBNumber -
                       (iPreCallMBNumber % iMBRowSize) +
                       iMBRowSize;

        // perform a deblocking on previous row(s)
        if (iCurMBNumber < iMaxMBNumber)
        {
            if ((bDoDeblocking) &&
                (iMBRowSize < iAvailableMBToDeblock))
            {
                int32_t iToDeblock = iAvailableMBToDeblock - iMBRowSize;

                DeblockSegmentTask(iFirstMBToDeblock, iToDeblock);
                iFirstMBToDeblock += iToDeblock;
                iAvailableMBToDeblock -= iToDeblock;
                m_cur_mb.isInited = false; // reset variables
            }
        }
    }

    if ((UMC_OK == umcRes) ||
        (UMC_ERR_END_OF_STREAM == umcRes))
    {
        iMBToProcess = m_CurMBAddr - iFirstMB;

        // perform deblocking of remain MBs
        if (bDoDeblocking)
            DeblockSegmentTask(iFirstMBToDeblock, iAvailableMBToDeblock);

        // in any case it is end of slice
        umcRes = UMC_ERR_END_OF_STREAM;
    }

    return umcRes;
}

SegmentDecoderHPBase* H264SegmentDecoderMultiThreaded::CreateSegmentDecoder()
{
    // It is against baseline/main constraints, but just for robustness
    bool faked_high_profile = m_pPicParamSet->transform_8x8_mode_flag || m_pPicParamSet->pic_scaling_matrix_present_flag ||
        m_pPicParamSet->chroma_qp_index_offset[1] != m_pPicParamSet->chroma_qp_index_offset[0];

    return CreateSD(
        bit_depth_luma,
        bit_depth_chroma,
        m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE,
        m_pCurrentFrame->m_chroma_format,
        (m_pSeqParamSet->profile_idc >= H264VideoDecoderParams::H264_PROFILE_HIGH ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_CAVLC444_INTRA ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE ||
        m_pSeqParamSet->profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH) ||
        faked_high_profile);
}

} // namespace UMC
#endif // MFX_ENABLE_H264_VIDEO_DECODE
