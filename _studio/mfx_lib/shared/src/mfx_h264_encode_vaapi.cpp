/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder VAAPI
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>
#include "libmfx_core_vaapi.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"

#if !defined(LINUX64)
    #define SKIP_FRAME_SUPPORT
#endif

#ifdef MFX_VA_ANDROID
    #define SKIP_FRAME_SUPPORT
#else
    #define MAX_FRAME_SIZE_SUPPORT
    #define TRELLIS_QUANTIZATION_SUPPORT
    #define ROLLING_INTRA_REFRESH_SUPPORT
#endif

using namespace MfxHwH264Encode;

static
mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
{
    switch (rateControl)
    {
    case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
    case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
    case MFX_RATECONTROL_AVBR: return VA_RC_VBR;
    case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
    default: assert(!"Unsupported RateControl"); return 0;
    }

} // mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)


static
VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type)
{
    switch (type)
    {
        case MFX_PROFILE_AVC_BASELINE: return VAProfileH264Baseline;
        case MFX_PROFILE_AVC_MAIN:     return VAProfileH264Main;
        case MFX_PROFILE_AVC_HIGH:     return VAProfileH264High;
        default: 
            //assert(!"Unsupported profile type");
            return VAProfileH264High; //VAProfileNone;
    }

} // VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)

mfxStatus SetHRD(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *hrd_param;

    if ( hrdBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, hrdBuf_id);
    }
    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                   1,
                   NULL,
                   &hrdBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 hrdBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeHRD;
    hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    hrd_param->initial_buffer_fullness = par.calcParam.initialDelayInKB * 8000;
    hrd_param->buffer_size = par.calcParam.bufferSizeInKB * 8000;

    vaUnmapBuffer(vaDisplay, hrdBuf_id);

    return MFX_ERR_NONE;
} // void SetHRD(...)

void FillBrcStructures(
    MfxVideoParam const & par,
    VAEncMiscParameterRateControl & vaBrcPar,
    VAEncMiscParameterFrameRate   & vaFrameRate)
{
    Zero(vaBrcPar);
    Zero(vaFrameRate);
    vaBrcPar.bits_per_second = par.calcParam.maxKbps * 1000;
    if(par.calcParam.maxKbps)
        vaBrcPar.target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);
    vaFrameRate.framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);
}

mfxStatus SetRateControl(
    MfxVideoParam const & par,
    mfxU32       mbbrc,
    mfxU8        minQP,
    mfxU8        maxQP,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rateParamBuf_id,
    bool         isBrcResetRequired = false)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;

    if ( rateParamBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, rateParamBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                   1,
                   NULL,
                   &rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 rateParamBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRateControl;
    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

    rate_param->bits_per_second = par.calcParam.maxKbps * 1000;
    rate_param->window_size     = par.mfx.Convergence * 100;

    rate_param->min_qp = minQP;

#ifdef MFX_VA_ANDROID   // todo: remove this check after ww22.2014 (full switch to mainline LibVA)
    rate_param->max_qp = maxQP;
#endif

    if(par.calcParam.maxKbps)
        rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);
/*
 * MBBRC control
 * Control VA_RC_MB 0: default, 1: enable, 2: disable, other: reserved
 */
    rate_param->rc_flags.bits.mb_rate_control = mbbrc & 0xf;
    rate_param->rc_flags.bits.reset = isBrcResetRequired;

    vaUnmapBuffer(vaDisplay, rateParamBuf_id);

    return MFX_ERR_NONE;
}

mfxStatus SetFrameRate(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, frameRateBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                   1,
                   NULL,
                   &frameRateBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 frameRateBuf_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    frameRate_param->framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);

    vaUnmapBuffer(vaDisplay, frameRateBuf_id);

    return MFX_ERR_NONE;
}

static mfxStatus SetMaxFrameSize(
    const UINT   userMaxFrameSize,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameSizeBuf_id)
{
    VAEncMiscParameterBuffer             *misc_param;
    VAEncMiscParameterBufferMaxFrameSize *p_maxFrameSize;

    if ( frameSizeBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, frameSizeBuf_id);
    }

    VAStatus vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferMaxFrameSize),
                   1,
                   NULL,
                   &frameSizeBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, frameSizeBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeMaxFrameSize;
    p_maxFrameSize = (VAEncMiscParameterBufferMaxFrameSize *)misc_param->data;

    p_maxFrameSize->max_frame_size = userMaxFrameSize*8;    // in bits for libva

    vaUnmapBuffer(vaDisplay, frameSizeBuf_id);

    return MFX_ERR_NONE;
}

static mfxStatus SetTrellisQuantization(
    mfxU32       trellis,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & trellisBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer        *misc_param;
    VAEncMiscParameterQuantization  *trellis_param;

    if ( trellisBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, trellisBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterQuantization),
                   1,
                   NULL,
                   &trellisBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, trellisBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeQuantization;
    trellis_param = (VAEncMiscParameterQuantization *)misc_param->data;

    trellis_param->quantization_flags.value = trellis;

    vaUnmapBuffer(vaDisplay, trellisBuf_id);

    return MFX_ERR_NONE;
} // void SetTrellisQuantization(...)

static mfxStatus SetRollingIntraRefresh(
    IntraRefreshState const & rirState,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & rirBuf_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRIR    *rir_param;

    if ( rirBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, rirBuf_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRIR),
                   1,
                   NULL,
                   &rirBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay, rirBuf_id, (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRIR;
    rir_param = (VAEncMiscParameterRIR *)misc_param->data;

    rir_param->rir_flags.value             = rirState.refrType;
    rir_param->intra_insertion_location    = rirState.IntraLocation;
    rir_param->intra_insert_size           = rirState.IntraSize;
    rir_param->qp_delta_for_inserted_intra = mfxU8(rirState.IntRefQPDelta);

    vaUnmapBuffer(vaDisplay, rirBuf_id);

    return MFX_ERR_NONE;
} // void SetRollingIntraRefresh(...)

mfxStatus SetPrivateParams(
    MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & privateParams_id)
{
    if (!IsSupported__VAEncMiscParameterPrivate()) return MFX_ERR_UNSUPPORTED;

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterPrivate *private_param;

    if ( privateParams_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, privateParams_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
                   vaContextEncode,
                   VAEncMiscParameterBufferType,
                   sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterPrivate),
                   1,
                   NULL,
                   &privateParams_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
                 privateParams_id,
                (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypePrivate;
    private_param = (VAEncMiscParameterPrivate *)misc_param->data;

    private_param->target_usage = (unsigned int)(par.mfx.TargetUsage);

    vaUnmapBuffer(vaDisplay, privateParams_id);

    return MFX_ERR_NONE;
} // void SetPrivateParams(...)

mfxStatus SetSkipFrame(
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & skipParam_id,
    mfxU8 skipFlag,
    mfxU8 numSkipFrames,
    mfxU32 sizeSkipFrames)
{
#ifdef SKIP_FRAME_SUPPORT
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterSkipFrame *skipParam;

    if (skipParam_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, skipParam_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
        vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterSkipFrame),
        1,
        NULL,
        &skipParam_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
        skipParam_id,
        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeSkipFrame;
    skipParam = (VAEncMiscParameterSkipFrame *)misc_param->data;

    skipParam->skip_frame_flag = skipFlag;
    skipParam->num_skip_frames = numSkipFrames;
    skipParam->size_skip_frames = sizeSkipFrames;

    vaUnmapBuffer(vaDisplay, skipParam_id);

    return MFX_ERR_NONE;
#else
    return MFX_ERR_UNSUPPORTED;
#endif
}

static mfxStatus SetROI(
    DdiTask const & task,
    std::vector<VAEncROI> & arrayVAEncROI,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & roiParam_id)
{
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferROI *roi_Param;

    if (roiParam_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(vaDisplay, roiParam_id);
    }

    vaSts = vaCreateBuffer(vaDisplay,
        vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferROI),
        1,
        NULL,
        &roiParam_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(vaDisplay,
        roiParam_id,
        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypeROI;
    roi_Param = (VAEncMiscParameterBufferROI *)misc_param->data;
    memset(roi_Param, 0, sizeof(VAEncMiscParameterBufferROI));

    if (task.m_numRoi)
    {
        roi_Param->num_roi = task.m_numRoi;

        if (arrayVAEncROI.size() < task.m_numRoi)
        {
            arrayVAEncROI.resize(task.m_numRoi);
        }
        roi_Param->ROI = Begin(arrayVAEncROI);
        memset(roi_Param->ROI, 0, task.m_numRoi*sizeof(VAEncROI));

        for (mfxU32 i = 0; i < task.m_numRoi; i ++)
        {
            roi_Param->ROI[i].roi_rectangle.x = task.m_roi[i].Left;
            roi_Param->ROI[i].roi_rectangle.y = task.m_roi[i].Top;
            roi_Param->ROI[i].roi_rectangle.width = task.m_roi[i].Right - task.m_roi[i].Left;
            roi_Param->ROI[i].roi_rectangle.height = task.m_roi[i].Top - task.m_roi[i].Bottom;
            roi_Param->ROI[i].roi_value = task.m_roi[i].Priority;
        }
        roi_Param->max_delta_qp = 51;
        roi_Param->min_delta_qp = -51;
    }
    vaUnmapBuffer(vaDisplay, roiParam_id);

    return MFX_ERR_NONE;
}

void FillConstPartOfPps(
    MfxVideoParam const & par,
    VAEncPictureParameterBufferH264 & pps)
{
    mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
    mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    assert( extPps != 0 );
    assert( extSps != 0 );

    if (!extPps || !extSps)
        return;

    pps.seq_parameter_set_id = extSps->seqParameterSetId;
    pps.pic_parameter_set_id = extPps->picParameterSetId;

    pps.last_picture = 0;// aya???
    pps.frame_num = 0;   // aya???

    pps.pic_init_qp = extPps->picInitQpMinus26 + 26;
    pps.num_ref_idx_l0_active_minus1            = extPps->numRefIdxL0DefaultActiveMinus1;
    pps.num_ref_idx_l1_active_minus1            = extPps->numRefIdxL1DefaultActiveMinus1;
    pps.chroma_qp_index_offset                  = extPps->chromaQpIndexOffset;
    pps.second_chroma_qp_index_offset           = extPps->secondChromaQpIndexOffset;

    pps.pic_fields.bits.deblocking_filter_control_present_flag  = 1;//aya: ???
    pps.pic_fields.bits.entropy_coding_mode_flag                = extPps->entropyCodingModeFlag;

    pps.pic_fields.bits.pic_order_present_flag                  = extPps->bottomFieldPicOrderInframePresentFlag;
    pps.pic_fields.bits.weighted_pred_flag                      = extPps->weightedPredFlag;
    pps.pic_fields.bits.weighted_bipred_idc                     = extPps->weightedBipredIdc;
    pps.pic_fields.bits.constrained_intra_pred_flag             = extPps->constrainedIntraPredFlag;
    pps.pic_fields.bits.transform_8x8_mode_flag                 = extPps->transform8x8ModeFlag;
    pps.pic_fields.bits.pic_scaling_matrix_present_flag         = extPps->picScalingMatrixPresentFlag;

    mfxU32 i = 0;
    for( i = 0; i < 16; i++ )
    {
        pps.ReferenceFrames[i].picture_id = VA_INVALID_ID;
    }

} // void FillConstPartOfPps(...)

void UpdatePPS(
    DdiTask const & task,
    mfxU32          fieldId,
    VAEncPictureParameterBufferH264 & pps,
    std::vector<ExtVASurface> const & reconQueue)
{
    pps.frame_num = task.m_frameNum;

    pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;

    pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
    pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);
    if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
        pps.CurrPic.flags = TFIELD == fieldId ? VA_PICTURE_H264_TOP_FIELD : VA_PICTURE_H264_BOTTOM_FIELD;
    else
        pps.CurrPic.flags = 0;

    mfxU32 i = 0;
    mfxU32 idx = 0;
    mfxU32 ref = 0;
#if 0
    for( i = 0; i < task.m_dpb[fieldId].Size(); i++ )
    {
        pps.ReferenceFrames[i].frame_idx = idx  = task.m_dpb[fieldId][i].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = task.m_dpb[fieldId][i].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = task.m_dpb[fieldId][i].m_poc[1];
    }
#else
    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];
    assert(task.m_list0[fieldId].Size() + task.m_list1[fieldId].Size() <= 16);

    for (ref = 0; ref < list0.Size() && i < 16; i++, ref++)
    {
        pps.ReferenceFrames[i].frame_idx = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = dpb[list0[ref] & 0x7f].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = dpb[list0[ref] & 0x7f].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = dpb[list0[ref] & 0x7f].m_poc[1];
    }

    for (ref = 0; ref < list1.Size() && i < 16; i++, ref++)
    {
        pps.ReferenceFrames[i].frame_idx = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
        pps.ReferenceFrames[i].picture_id       = reconQueue[idx].surface;
        pps.ReferenceFrames[i].flags            = dpb[list1[ref] & 0x7f].m_longterm ? VA_PICTURE_H264_LONG_TERM_REFERENCE : VA_PICTURE_H264_SHORT_TERM_REFERENCE;
        pps.ReferenceFrames[i].TopFieldOrderCnt = dpb[list1[ref] & 0x7f].m_poc[0];
        pps.ReferenceFrames[i].BottomFieldOrderCnt = dpb[list1[ref] & 0x7f].m_poc[1];
    }
#endif
    for (; i < 16; i++)
    {
        pps.ReferenceFrames[i].picture_id = VA_INVALID_ID;
        pps.ReferenceFrames[i].frame_idx = 0xff;
        pps.ReferenceFrames[i].flags = VA_PICTURE_H264_INVALID;
        pps.ReferenceFrames[i].TopFieldOrderCnt   = 0;
        pps.ReferenceFrames[i].BottomFieldOrderCnt   = 0;
    }
} // void UpdatePPS(...)

void UpdateSlice(
    ENCODE_CAPS const &                         hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par,
    std::vector<ExtVASurface> const & reconQueue)
{
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    if (task.m_numSlice[fieldId])
        slice.resize(task.m_numSlice[fieldId]);
    mfxU32 numSlice = slice.size();
    mfxU32 idx = 0, ref = 0;

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(par);
    assert( extDdi != 0 );

    SliceDivider divider = MakeSliceDivider(
        hwCaps.SliceStructure,
        task.m_numMbPerSlice,
        numSlice,
        sps.picture_width_in_mbs,
        sps.picture_height_in_mbs / numPics);

    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];

    for( size_t i = 0; i < slice.size(); ++i, divider.Next() )
    {
        slice[i].macroblock_address = divider.GetFirstMbInSlice();
        slice[i].num_macroblocks = divider.GetNumMbInSlice();
        slice[i].macroblock_info = VA_INVALID_ID;

        for (ref = 0; ref < list0.Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        for (ref = 0; ref < list1.Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList1[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = mfxU8(IPP_MAX(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1 = mfxU8(IPP_MAX(1, task.m_list1[fieldId].Size()) - 1);
        slice[i].num_ref_idx_active_override_flag   =
                    slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
                    slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));

        slice[i].delta_pic_order_cnt_bottom         = 0;
        slice[i].delta_pic_order_cnt[0]             = 0;
        slice[i].delta_pic_order_cnt[1]             = 0;
        slice[i].luma_log2_weight_denom             = 0;
        slice[i].chroma_log2_weight_denom           = 0;

        slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

        slice[i].disable_deblocking_filter_idc = 0;
        slice[i].slice_alpha_c0_offset_div2 = 0;
        slice[i].slice_beta_offset_div2 = 0;
    }

} // void UpdateSlice(...)

void UpdateSliceSizeLimited(
    ENCODE_CAPS const &                         hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    VAEncSequenceParameterBufferH264 const     & sps,
    VAEncPictureParameterBufferH264 const      & pps,
    std::vector<VAEncSliceParameterBufferH264> & slice,
    MfxVideoParam const                        & par,
    std::vector<ExtVASurface> const & reconQueue)
{
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    mfxU32 idx = 0, ref = 0;

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(par);
    assert( extDdi != 0 );

    size_t numSlices = task.m_SliceInfo.size();   
    if (numSlices != slice.size())
    {
        size_t old_size = slice.size();
        slice.resize(numSlices);
        for (size_t i = old_size; i < slice.size(); ++i)
        {
            slice[i] = slice[0];
            //slice[i].slice_id = (mfxU32)i;
        } 
    }

    ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
    ArrayU8x33 const & list0 = task.m_list0[fieldId];
    ArrayU8x33 const & list1 = task.m_list1[fieldId];

    for( size_t i = 0; i < slice.size(); ++i)
    {
        slice[i].macroblock_address = task.m_SliceInfo[i].startMB;
        slice[i].num_macroblocks = task.m_SliceInfo[i].numMB;
        slice[i].macroblock_info = VA_INVALID_ID;

        for (ref = 0; ref < list0.Size(); ref++)
        {
            slice[i].RefPicList0[ref].frame_idx    = idx  = dpb[list0[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList0[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList0[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList0[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList0[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        for (ref = 0; ref < list1.Size(); ref++)
        {
            slice[i].RefPicList1[ref].frame_idx    = idx  = dpb[list1[ref] & 0x7f].m_frameIdx & 0x7f;
            slice[i].RefPicList1[ref].picture_id          = reconQueue[idx].surface;
            if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
                slice[i].RefPicList1[ref].flags               = list0[ref] >> 7 ? VA_PICTURE_H264_BOTTOM_FIELD : VA_PICTURE_H264_TOP_FIELD;
        }
        for (; ref < 32; ref++)
        {
            slice[i].RefPicList1[ref].picture_id = VA_INVALID_ID;
            slice[i].RefPicList1[ref].flags      = VA_PICTURE_H264_INVALID;
        }

        slice[i].pic_parameter_set_id = pps.pic_parameter_set_id;
        slice[i].slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId] );

        slice[i].direct_spatial_mv_pred_flag = 1;

        slice[i].num_ref_idx_l0_active_minus1 = mfxU8(IPP_MAX(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1 = mfxU8(IPP_MAX(1, task.m_list1[fieldId].Size()) - 1);
        slice[i].num_ref_idx_active_override_flag   =
                    slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
                    slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));

        slice[i].delta_pic_order_cnt_bottom         = 0;
        slice[i].delta_pic_order_cnt[0]             = 0;
        slice[i].delta_pic_order_cnt[1]             = 0;
        slice[i].luma_log2_weight_denom             = 0;
        slice[i].chroma_log2_weight_denom           = 0;

        slice[i].cabac_init_idc                     = extDdi ? (mfxU8)extDdi->CabacInitIdcPlus1 - 1 : 0;
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.pic_init_qp);

        slice[i].disable_deblocking_filter_idc = 0;
        slice[i].slice_alpha_c0_offset_div2 = 0;
        slice[i].slice_beta_offset_div2 = 0;
    }

} // void UpdateSlice(...)


////////////////////////////////////////////////////////////////////////////////////////////
//                    HWEncoder based on VAAPI
////////////////////////////////////////////////////////////////////////////////////////////

using namespace MfxHwH264Encode;

VAAPIEncoder::VAAPIEncoder()
: m_core(NULL)
, m_vaDisplay(0)
, m_vaContextEncode(VA_INVALID_ID)
, m_vaConfig(VA_INVALID_ID)
, m_spsBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_rateParamBufferId(VA_INVALID_ID)
, m_frameRateId(VA_INVALID_ID)
, m_maxFrameSizeId(VA_INVALID_ID)
, m_quantizationId(VA_INVALID_ID)
, m_rirId(VA_INVALID_ID)
, m_privateParamsId(VA_INVALID_ID)
, m_miscParameterSkipBufferId(VA_INVALID_ID)
, m_roiBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_packedAudHeaderBufferId(VA_INVALID_ID)
, m_packedAudBufferId(VA_INVALID_ID)
, m_packedSpsHeaderBufferId(VA_INVALID_ID)
, m_packedSpsBufferId(VA_INVALID_ID)
, m_packedPpsHeaderBufferId(VA_INVALID_ID)
, m_packedPpsBufferId(VA_INVALID_ID)
, m_packedSeiHeaderBufferId(VA_INVALID_ID)
, m_packedSeiBufferId(VA_INVALID_ID)
, m_packedSkippedSliceHeaderBufferId(VA_INVALID_ID)
, m_packedSkippedSliceBufferId(VA_INVALID_ID)
, m_userMaxFrameSize(0)
, m_curTrellisQuantization(0)
, m_newTrellisQuantization(0)
, m_mbbrc(0)
, m_numSkipFrames(0)
, m_sizeSkipFrames(0)
, m_skipMode(0)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()


void VAAPIEncoder::FillSps(
    MfxVideoParam const & par,
    VAEncSequenceParameterBufferH264 & sps)
{
        mfxExtSpsHeader const * extSps = GetExtBuffer(par);
        assert( extSps != 0 );
        if (!extSps)
            return;

        sps.picture_width_in_mbs  = par.mfx.FrameInfo.Width >> 4;
        sps.picture_height_in_mbs = par.mfx.FrameInfo.Height >> 4;

        sps.level_idc   = par.mfx.CodecLevel;

        sps.intra_period = par.mfx.GopPicSize;
        sps.ip_period    = par.mfx.GopRefDist;

        sps.bits_per_second     = par.calcParam.targetKbps * 1000;

        sps.time_scale      = extSps->vui.timeScale;
        sps.num_units_in_tick = extSps->vui.numUnitsInTick;

        sps.max_num_ref_frames   =  mfxU8((extSps->maxNumRefFrames + 1) / 2);
        sps.seq_parameter_set_id = extSps->seqParameterSetId;
        sps.seq_fields.bits.chroma_format_idc    = extSps->chromaFormatIdc;
        sps.bit_depth_luma_minus8    = extSps->bitDepthLumaMinus8;
        sps.bit_depth_chroma_minus8  = extSps->bitDepthChromaMinus8;

        sps.seq_fields.bits.log2_max_frame_num_minus4               = extSps->log2MaxFrameNumMinus4;
        sps.seq_fields.bits.pic_order_cnt_type                      = extSps->picOrderCntType;
        sps.seq_fields.bits.log2_max_pic_order_cnt_lsb_minus4       = extSps->log2MaxPicOrderCntLsbMinus4;

        sps.num_ref_frames_in_pic_order_cnt_cycle   = extSps->numRefFramesInPicOrderCntCycle;
        sps.offset_for_non_ref_pic                  = extSps->offsetForNonRefPic;
        sps.offset_for_top_to_bottom_field          = extSps->offsetForTopToBottomField;

        Copy(sps.offset_for_ref_frame,  extSps->offsetForRefFrame);

        sps.frame_crop_left_offset                  = mfxU16(extSps->frameCropLeftOffset);
        sps.frame_crop_right_offset                 = mfxU16(extSps->frameCropRightOffset);
        sps.frame_crop_top_offset                   = mfxU16(extSps->frameCropTopOffset);
        sps.frame_crop_bottom_offset                = mfxU16(extSps->frameCropBottomOffset);
        sps.seq_fields.bits.seq_scaling_matrix_present_flag         = extSps->seqScalingMatrixPresentFlag;

        sps.seq_fields.bits.delta_pic_order_always_zero_flag        = extSps->deltaPicOrderAlwaysZeroFlag;

        sps.seq_fields.bits.frame_mbs_only_flag                     = extSps->frameMbsOnlyFlag;
        sps.seq_fields.bits.mb_adaptive_frame_field_flag            = extSps->mbAdaptiveFrameFieldFlag;
        sps.seq_fields.bits.direct_8x8_inference_flag               = extSps->direct8x8InferenceFlag;

        sps.vui_parameters_present_flag                 = extSps->vuiParametersPresentFlag;
        sps.vui_fields.bits.timing_info_present_flag    = extSps->vui.flags.timingInfoPresent;
        sps.vui_fields.bits.bitstream_restriction_flag  = extSps->vui.flags.bitstreamRestriction;
        sps.vui_fields.bits.log2_max_mv_length_horizontal  = extSps->vui.log2MaxMvLengthHorizontal;
        sps.vui_fields.bits.log2_max_mv_length_vertical  = extSps->vui.log2MaxMvLengthVertical;

        sps.frame_cropping_flag                     = extSps->frameCroppingFlag;

//#if VA_CHECK_VERSION(0, 34, 0)
//after backport of va to OTC staging 0.33 also has the same fields. TODO: fully remove after validation
        sps.sar_height        = extSps->vui.sarHeight;
        sps.sar_width         = extSps->vui.sarWidth;
        sps.aspect_ratio_idc  = extSps->vui.aspectRatioIdc;
//#endif
/*
 *  In Windows DDI Trellis Quantization in SPS, while for VA in miscEnc.
 *  keep is here to have processed in Execute
 */
        mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(par);
        assert( extOpt2 != 0 );
        m_newTrellisQuantization = extOpt2 ? extOpt2->Trellis : 0;

} // void FillSps(...)

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE* core,
    GUID /*guid*/,
    mfxU32 width,
    mfxU32 height,
    bool /*isTemporal*/)
{
    m_core = core;

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);
    if(hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    m_width  = width;
    m_height = height;

    memset(&m_caps, 0, sizeof(m_caps));

    m_caps.BRCReset = 1; // no bitrate resolution control
    m_caps.HeaderInsertion = 0; // we will privide headers (SPS, PPS) in binary format to the driver

    //VAConfigAttrib attrs[5];
    std::vector<VAConfigAttrib> attrs;

    attrs.push_back( {VAConfigAttribRTFormat, 0});
    attrs.push_back( {VAConfigAttribRateControl, 0});
    attrs.push_back( {VAConfigAttribEncQuantization, 0});
    attrs.push_back( {VAConfigAttribEncIntraRefresh, 0});
    attrs.push_back( {VAConfigAttribMaxPictureHeight, 0});
    attrs.push_back( {VAConfigAttribMaxPictureWidth, 0});
    attrs.push_back( {VAConfigAttribEncInterlaced, 0});
    attrs.push_back( {VAConfigAttribEncMaxRefFrames, 0});
    attrs.push_back( {VAConfigAttribEncMaxSlices, 0});
#ifdef SKIP_FRAME_SUPPORT
    attrs.push_back( {VAConfigAttribEncSkipFrame, 0});
#endif

    VAStatus vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile),
                          VAEntrypointEncSlice,
                          Begin(attrs), attrs.size());
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_caps.VCMBitrateControl = attrs[1].value & VA_RC_VCM ? 1 : 0; //Video conference mode
    m_caps.TrelisQuantization  = (attrs[2].value & (~VA_ATTRIB_NOT_SUPPORTED));
    m_caps.vaTrellisQuantization = attrs[2].value;
    m_caps.RollingIntraRefresh = (attrs[3].value & (~VA_ATTRIB_NOT_SUPPORTED)) ? 1 : 0 ;
    m_caps.vaRollingIntraRefresh = attrs[3].value;
#ifdef SKIP_FRAME_SUPPORT
    m_caps.SkipFrame = (attrs[9].value & (~VA_ATTRIB_NOT_SUPPORTED)) ? 1 : 0 ;
#endif

    m_caps.UserMaxFrameSizeSupport = 1; // no request on support for libVA
    m_caps.MBBRCSupport = 1;            // starting 16.3 Beta, enabled in driver by default for TU-1,2

    vaExtQueryEncCapabilities pfnVaExtQueryCaps = NULL;
    pfnVaExtQueryCaps = (vaExtQueryEncCapabilities)vaGetLibFunc(m_vaDisplay,VPG_EXT_QUERY_ENC_CAPS);
    /* This is for 16.3.* approach.
     * It was used private libVA function to get information which feature is supported
     * */
    if (pfnVaExtQueryCaps)
    {
        VAEncQueryCapabilities VaEncCaps;
        memset(&VaEncCaps, 0, sizeof(VaEncCaps));
        VaEncCaps.size = sizeof(VAEncQueryCapabilities);
        vaSts = pfnVaExtQueryCaps(m_vaDisplay, VAProfileH264Baseline, &VaEncCaps);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        m_caps.MaxPicWidth  = VaEncCaps.MaxPicWidth;
        m_caps.MaxPicHeight = VaEncCaps.MaxPicHeight;
        m_caps.SliceStructure = VaEncCaps.EncLimits.bits.SliceStructure;
        m_caps.NoInterlacedField = VaEncCaps.EncLimits.bits.NoInterlacedField;
        m_caps.MaxNum_Reference = VaEncCaps.MaxNum_ReferenceL0;
        m_caps.MaxNum_Reference1 = VaEncCaps.MaxNum_ReferenceL1;
    }
    else /* this is LibVA legacy approach. Should be supported from 16.4 driver */
    {
#ifdef MFX_VA_ANDROID
        // To replace by vaQueryConfigAttributes()
        // when the driver starts to support VAConfigAttribMaxPictureWidth/Height
        m_caps.MaxPicWidth  = 1920;
        m_caps.MaxPicHeight = 1200;
#else
        if ((attrs[5].value != VA_ATTRIB_NOT_SUPPORTED) && (attrs[5].value != 0))
            m_caps.MaxPicWidth  = attrs[5].value;
        else
            m_caps.MaxPicWidth = 1920;

        if ((attrs[4].value != VA_ATTRIB_NOT_SUPPORTED) && (attrs[4].value != 0))
            m_caps.MaxPicHeight = attrs[4].value;
        else
            m_caps.MaxPicHeight = 1088;
#endif
        if (attrs[8].value != VA_ATTRIB_NOT_SUPPORTED)
            m_caps.SliceStructure = attrs[8].value;
        else
            m_caps.SliceStructure = m_core->GetHWType() == MFX_HW_HSW ? 2 : 1; // 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice

        if (attrs[6].value != VA_ATTRIB_NOT_SUPPORTED)
            m_caps.NoInterlacedField = attrs[6].value;
        else
            m_caps.NoInterlacedField = 0;

        if (attrs[7].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            m_caps.MaxNum_Reference = attrs[7].value & 0xffff;
            m_caps.MaxNum_Reference1 = (attrs[7].value >>16) & 0xffff;
        }
        else
        {
            m_caps.MaxNum_Reference = 1;
            m_caps.MaxNum_Reference1 = 1;
        }
    } /* if (pfnVaExtQueryCaps) */

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    if(IsMvcProfile(par.mfx.CodecProfile))
        return MFX_WRN_PARTIAL_ACCELERATION;

    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces wheh call vaCreateContext().
     * Here we don't have this info.
     */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                Begin(pEntrypoints),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        if( VAEntrypointEncSlice == pEntrypoints[entrypointsIndx] )
        {
            bEncodeEnable = true;
            break;
        }
    }
    if( !bEncodeEnable )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // Configuration
    VAConfigAttrib attrib[2];
    mfxU32 flag = VA_PROGRESSIVE;

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;

    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
                          VAEntrypointEncSlice,
                          &attrib[0], sizeof(attrib)/sizeof(attrib[0]));
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    if( NULL == extOpt2 )
    {
        assert( extOpt2 );
        return (MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
        VAEntrypointEncSlice,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(unsigned int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    if (m_videoParam.Protected && IsSupported__VAHDCPEncryptionParameterBuffer())
        flag |= VA_HDCP_ENABLED;

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        flag,
        Begin(reconSurf),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU16 maxNumSlices = GetMaxNumSlices(par);

    m_slice.resize(maxNumSlices);
    m_sliceBufferId.resize(maxNumSlices);
    m_packeSliceHeaderBufferId.resize(maxNumSlices);
    m_packedSliceBufferId.resize(maxNumSlices);
    for(int i = 0; i < maxNumSlices; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId), MFX_ERR_DEVICE_FAILED);

    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)
{
//    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Reset");

    m_videoParam = par;
    mfxExtCodingOption2 const *extOpt2 = GetExtBuffer(par);
    if( NULL == extOpt2 ) {
        assert( extOpt2 );
        return (MFX_ERR_UNKNOWN);
    }
    m_mbbrc = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;
    m_skipMode = extOpt2->SkipFrame;

    FillSps(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    bool isBrcResetRequired = !Equal(m_vaBrcPar, oldBrcPar) || !Equal(m_vaFrameRate, oldFrameRate);

    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(par, m_mbbrc, 0, 0, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId, isBrcResetRequired), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId), MFX_ERR_DEVICE_FAILED);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId), MFX_ERR_DEVICE_FAILED);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    type;

    // request linear bufer
    request.Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    request.reserved[0] = m_vaContextEncode;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)


mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)
{

    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    par;
    mbPerSec;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VAAPIEncoder::QueryHWGUID(VideoCORE * core, GUID guid, bool isTemporal)
{
    core;
    guid;
    isTemporal;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type )
    {
        pQueue = &m_bsQueue;
    }
    else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++)
        {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number  = i;
            extSurf.surface = *pSurface;

            pQueue->push_back( extSurf );
        }
    }
    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type )
    {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)
{
    memId;
    type;

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)


bool operator==(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return memcmp(&l, &r, sizeof(ENCODE_ENC_CTRL_CAPS)) == 0;
}

bool operator!=(const ENCODE_ENC_CTRL_CAPS& l, const ENCODE_ENC_CTRL_CAPS& r)
{
    return !(l == r);
}

mfxStatus VAAPIEncoder::Execute(
    mfxHDL          surface,
    DdiTask const & task,
    mfxU32          fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");

    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;
    VASurfaceID reconSurface;
    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VABufferID  codedBuffer;
    std::vector<VABufferID> configBuffers;
    std::vector<mfxU32> packedBufferIndexes;
    mfxU32      i;
    mfxU16      buffersCount = 0;
    mfxU32      packedDataSize = 0;
    VAStatus    vaSts;
    mfxU8 skipFlag = task.SkipFlag();

    // update params
    {
        size_t slice_size_old = m_slice.size();
        //Destroy old buffers
        for(int i=0; i<slice_size_old; i++){
            MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
            MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
        }

        UpdatePPS(task, fieldId, m_pps, m_reconQueue);

        if (task.m_SliceInfo.size())
            UpdateSliceSizeLimited(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);
        else
            UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

        if (slice_size_old != m_slice.size())
        {
            m_sliceBufferId.resize(m_slice.size());
            m_packeSliceHeaderBufferId.resize(m_slice.size());
            m_packedSliceBufferId.resize(m_slice.size());
            
        }

        configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    }

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxBs = task.m_idxBs[fieldId];
    if( idxBs < m_bsQueue.size() )
    {
        codedBuffer = m_bsQueue[idxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    // find reconstructed surface
    unsigned int idxRecon = task.m_idxRecon;
    if( idxRecon < m_reconQueue.size())
    {
        reconSurface = m_reconQueue[ idxRecon ].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    m_pps.coded_buf = codedBuffer;
    m_pps.CurrPic.picture_id = reconSurface;

    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        {
            MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),
                                   1,
                                   &m_sps,
                                   &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_spsBufferId;
        }

        // 2. Picture level
        {
            MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPictureParameterBufferType,
                                   sizeof(m_pps),
                                   1,
                                   &m_pps,
                                   &m_ppsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_ppsBufferId;
        }

        // 3. Slice level
        for( i = 0; i < m_slice.size(); i++ )
        {
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    if (m_caps.HeaderInsertion == 1 && skipFlag == NO_SKIP)
    {
        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }
    }
    else
    {
        // AUD
        if (task.m_insertAud[fieldId])
        {
            ENCODE_PACKEDHEADER_DATA const & packedAud = m_headerPacker.PackAud(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedAud.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedAud.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedAudHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedAud.DataLength, 1, packedAud.pData,
                                &m_packedAudBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedAudHeaderBufferId;
            configBuffers[buffersCount++] = m_packedAudBufferId;
        }
        // SPS
        if (task.m_insertSps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
            ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderSequence;
            packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSps.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedSps.DataLength, 1, packedSps.pData,
                                &m_packedSpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSpsBufferId;
        }

        if (task.m_insertPps[fieldId])
        {
            // PPS
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
            ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderPicture;
            packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedPps.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedPpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                packedPps.DataLength, 1, packedPps.pData,
                                &m_packedPpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedPpsBufferId;
        }

        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                sei.Size(), 1, RemoveConst(sei.Buffer()),
                                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }

        if (skipFlag != NO_SKIP)
        {
            // The whole slice
            ENCODE_PACKEDHEADER_DATA const & packedSkippedSlice = m_headerPacker.PackSkippedSlice(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedSkippedSlice.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSkippedSlice.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSkippedSliceHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSkippedSliceHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSkippedSliceBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                packedSkippedSlice.DataLength, 1, packedSkippedSlice.pData,
                &m_packedSkippedSliceBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            packedBufferIndexes.push_back(buffersCount);
            packedDataSize += packed_header_param_buffer.bit_length;
            configBuffers[buffersCount++] = m_packedSkippedSliceHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSkippedSliceBufferId;

            if (PAVP_MODE == skipFlag)
            {
                m_numSkipFrames = 1;
                m_sizeSkipFrames = (m_skipMode != MFX_SKIPFRAME_INSERT_NOTHING) ? packedDataSize : 0;
            }
        }
        else
        {
            if (m_core->GetHWType() >= MFX_HW_HSW)
            {
                //Slice headers only
                std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId, m_slice);
                for (size_t i = 0; i < packedSlices.size(); i++)
                {
                    ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

                    packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
                    packed_header_param_buffer.has_emulation_bytes = !packedSlice.SkipEmulationByteCount;
                    packed_header_param_buffer.bit_length = packedSlice.DataLength; // DataLength is already in bits !

                    //MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
                    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderParameterBufferType,
                            sizeof(packed_header_param_buffer),
                            1,
                            &packed_header_param_buffer,
                            &m_packeSliceHeaderBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    //MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
                    vaSts = vaCreateBuffer(m_vaDisplay,
                                        m_vaContextEncode,
                                        VAEncPackedHeaderDataBufferType,
                                        (packedSlice.DataLength + 7) / 8, 1, packedSlice.pData,
                                        &m_packedSliceBufferId[i]);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                    configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
                    configBuffers[buffersCount++] = m_packedSliceBufferId[i];
                }
            }
        }
    }
    
    configBuffers[buffersCount++] = m_hrdBufferId;
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRateControl(m_videoParam, m_mbbrc, task.m_minQP, task.m_maxQP,
                                                         m_vaDisplay, m_vaContextEncode, m_rateParamBufferId), MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_rateParamBufferId;
    configBuffers[buffersCount++] = m_frameRateId;

#ifdef MAX_FRAME_SIZE_SUPPORT
/*
 * Limit frame size by application/user level
 */
    if (m_userMaxFrameSize != task.m_maxFrameSize)
    {
        m_userMaxFrameSize = (UINT)task.m_maxFrameSize;
//        if (task.m_frameOrder)
//            m_sps.bResetBRC = true;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetMaxFrameSize(m_userMaxFrameSize, m_vaDisplay,
                                                              m_vaContextEncode, m_maxFrameSizeId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_maxFrameSizeId;
    }
#endif

#ifdef TRELLIS_QUANTIZATION_SUPPORT
/*
 *  By default (0) - driver will decide.
 *  1 - disable trellis quantization
 *  x0E - enable for any type of frames
 */
    if (m_newTrellisQuantization != m_curTrellisQuantization)
    {
        m_curTrellisQuantization = m_newTrellisQuantization;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetTrellisQuantization(m_curTrellisQuantization, m_vaDisplay,
                                                                     m_vaContextEncode, m_quantizationId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_quantizationId;
    }
#endif

#ifdef ROLLING_INTRA_REFRESH_SUPPORT
 /*
 *   RollingIntraRefresh
 */
    if (memcmp(&task.m_IRState, &m_RIRState, sizeof(m_RIRState)))
    {
        m_RIRState = task.m_IRState;
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetRollingIntraRefresh(m_RIRState, m_vaDisplay,
                                                                     m_vaContextEncode, m_rirId), MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_rirId;
    }
#endif

    if (task.m_numRoi)
    {
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetROI(task, m_arrayVAEncROI, m_vaDisplay, m_vaContextEncode, m_roiBufferId),
                              MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = m_roiBufferId;
    }

    if (VA_INVALID_ID != m_privateParamsId) configBuffers[buffersCount++] = m_privateParamsId;

    assert(buffersCount <= configBuffers.size());

    mfxU32 storedSize = 0;

    if (skipFlag != NORMAL_MODE)
    {
#ifdef SKIP_FRAME_SUPPORT
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == SetSkipFrame(m_vaDisplay, m_vaContextEncode, m_miscParameterSkipBufferId,
                                                           skipFlag ? skipFlag : !!m_numSkipFrames,
                                                           m_numSkipFrames, m_sizeSkipFrames), MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_miscParameterSkipBufferId;

        m_numSkipFrames  = 0;
        m_sizeSkipFrames = 0;
#endif        
        //------------------------------------------------------------------
        // Rendering
        //------------------------------------------------------------------
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaBeginPicture");

            vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaRenderPicture");
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                Begin(configBuffers),
                buffersCount);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            for( i = 0; i < m_slice.size(); i++)
            {
                vaSts = vaRenderPicture(
                    m_vaDisplay,
                    m_vaContextEncode,
                    &m_sliceBufferId[i],
                    1);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaEndPicture");

            vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    else
    {
        VACodedBufferSegment *codedBufferSegment;
        {
            codedBuffer = m_bsQueue[task.m_idxBs[fieldId]].surface;
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
            vaSts = vaMapBuffer(
                m_vaDisplay,
                codedBuffer,
                (void **)(&codedBufferSegment));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        codedBufferSegment->next = 0;
        codedBufferSegment->reserved = 0;
        codedBufferSegment->status = 0;

        MFX_CHECK_WITH_ASSERT(codedBufferSegment->buf, MFX_ERR_DEVICE_FAILED);

        mfxU8 *  bsDataStart = (mfxU8 *)codedBufferSegment->buf;
        mfxU8 *  bsDataEnd   = bsDataStart;

        if (m_skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            for (size_t i = 0; i < packedBufferIndexes.size(); i++)
            {
                size_t headerIndex = packedBufferIndexes[i];

                void *pBufferHeader;
                vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex], &pBufferHeader);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                void *pData;
                vaSts = vaMapBuffer(m_vaDisplay, configBuffers[headerIndex + 1], &pData);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                if (pBufferHeader && pData)
                {
                    VAEncPackedHeaderParameterBuffer const & header = *(VAEncPackedHeaderParameterBuffer const *)pBufferHeader;

                    mfxU32 lenght = (header.bit_length+7)/8;

                    assert(mfxU32(bsDataStart + m_width*m_height - bsDataEnd) > lenght);
                    assert(header.has_emulation_bytes);

                    MFX_INTERNAL_CPY(bsDataEnd, pData, lenght);
                    bsDataEnd += lenght;
                }

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                vaSts = vaUnmapBuffer(m_vaDisplay, configBuffers[headerIndex + 1]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }

        storedSize = mfxU32(bsDataEnd - bsDataStart);
        codedBufferSegment->size = storedSize;

        m_numSkipFrames ++;
        m_sizeSkipFrames += (m_skipMode != MFX_SKIPFRAME_INSERT_NOTHING) ? packedDataSize : 0;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
            vaUnmapBuffer( m_vaDisplay, codedBuffer );
        }
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = (NORMAL_MODE == skipFlag) ? VA_INVALID_SURFACE : *inputSurface;
        currentFeedback.idxBs   = task.m_idxBs[fieldId];
        currentFeedback.size    = storedSize;
        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 waitSize;
    mfxU32 indxSurf;
    
    std::auto_ptr<UMC::AutomaticUMCMutex> guard( new UMC::AutomaticUMCMutex(m_guard) );

    for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
    {
        ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

        if( currentFeedback.number == task.m_statusReportNumber[fieldId] )
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            waitSize    = currentFeedback.size;

            isFound  = true;
            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    // find used bitstream
    VABufferID codedBuffer;
    if( waitIdxBs < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    if (waitSurface != VA_INVALID_SURFACE) // Not skipped frame 
    {
        VASurfaceStatus surfSts = VASurfaceSkipped;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
        guard.reset();

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaSyncSurface");
            vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        surfSts = VASurfaceReady;

#else

        vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        if (VASurfaceReady == surfSts)
        {
            m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
            guard.reset();
        }

#endif
        switch (surfSts)
        {
            case VASurfaceReady:
                VACodedBufferSegment *codedBufferSegment;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        codedBuffer,
                        (void **)(&codedBufferSegment));
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }

                task.m_bsDataLength[fieldId] = codedBufferSegment->size;

                if (m_videoParam.Protected && IsSupported__VAHDCPEncryptionParameterBuffer() && codedBufferSegment->next)
                {
                    VACodedBufferSegment *nextSegment = (VACodedBufferSegment*)codedBufferSegment->next;
                    VAHDCPEncryptionParameterBuffer *HDCPParam = NULL;
                    mfxAES128CipherCounter CipherCounter = {0, 0};
                    bool isProtected = false;

                    if (nextSegment->status & VA_CODED_BUF_STATUS_PRIVATE_DATA_HDCP &&
                        NULL != nextSegment->buf)
                    {
                        HDCPParam = (VAHDCPEncryptionParameterBuffer*)nextSegment->buf;
                        mfxU64* Count = (mfxU64*)&HDCPParam->counter[0];
                        mfxU64* IV = (mfxU64*)&HDCPParam->counter[2];
                        CipherCounter.Count = *Count;
                        CipherCounter.IV = *IV;

                        isProtected = HDCPParam->bEncrypted;
                    }

                    task.m_aesCounter[0] = CipherCounter;
                    task.m_notProtected = !isProtected;
                }

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                    vaUnmapBuffer( m_vaDisplay, codedBuffer );
                }

                return MFX_ERR_NONE;

            case VASurfaceRendering:
            case VASurfaceDisplaying:
                return MFX_WRN_DEVICE_BUSY;

            case VASurfaceSkipped:
            default:
                assert(!"bad feedback status");
                return MFX_ERR_DEVICE_FAILED;
        }
    }
    else
    {
        task.m_bsDataLength[fieldId] = waitSize;
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Destroy()
{
    if( m_vaContextEncode )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextEncode );
        m_vaContextEncode = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }

    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_hrdBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rateParamBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frameRateId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_maxFrameSizeId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_quantizationId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rirId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_privateParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_miscParameterSkipBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_roiBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    for( mfxU32 i = 0; i < m_slice.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
    }
    MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSkippedSliceBufferId, m_vaDisplay);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Destroy()

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
/* EOF */

