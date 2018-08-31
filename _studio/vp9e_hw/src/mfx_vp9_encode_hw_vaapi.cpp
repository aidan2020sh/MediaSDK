//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#if !defined (_WIN32) && !defined (_WIN64)

#include "mfx_vp9_encode_hw_utils.h"
#include "mfx_vp9_encode_hw_par.h"
#include "mfx_vp9_encode_hw_vaapi.h"
#include "mfx_common_int.h"

namespace MfxHwVP9Encode
{
#if defined(MFX_VA_LINUX)

 /* ----------- Functions to convert MediaSDK into DDI --------------------- */

    DriverEncoder* CreatePlatformVp9Encoder(mfxCoreInterface*)
    {
        return new VAAPIEncoder;
    }

    mfxU8 ConvertRTFormatMFX2VAAPI(mfxU8 chromaFormat)
    {
        VP9_LOG("ConvertRTFormatMFX2VAAPI \n");
        switch (chromaFormat)
        {
            case MFX_CHROMAFORMAT_YUV420:
                return VA_RT_FORMAT_YUV420;
            case MFX_CHROMAFORMAT_YUV444:
                return VA_RT_FORMAT_YUV444;
            default: assert(!"Unsupported ChromaFormat"); return 0;
        }
    } // mfxU8 ConvertRTFormatMFX2VAAPI(mfxU8 chromaFormat)

    mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
    {
        VP9_LOG("ConvertRateControlMFX2VAAPI \n");
        switch (rateControl)
        {
            case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
            case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
            case MFX_RATECONTROL_AVBR: return VA_RC_VBR;
            case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
            default: assert(!"Unsupported RateControl"); return 0;
        }
    } // mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)

    mfxU16 ConvertSegmentRefControlToVAAPI(mfxU16 refFrameControl)
    {
        VP9_LOG("ConvertSegmentRefControlToVAAPI \n");
        mfxU16 refControl = refFrameControl & 0x0F;//4 bits
        switch (refControl)
        {
            case MFX_VP9_REF_LAST:   return 1;
            case MFX_VP9_REF_GOLDEN: return 2;
            case MFX_VP9_REF_ALTREF: return 3;
            default:                 return 0;
        }
    } // mfxU16 ConvertSegmentRefControlToVAAPI(mfxU16 refFrameControl)

    void FillSpsBuffer(mfxVideoParam const & par, VAEncSequenceParameterBufferVP9 & sps)
    {
        VP9_LOG("FillSpsBuffer \n");
        Zero(sps);

        sps.max_frame_width  = par.mfx.FrameInfo.CropW!=0 ? par.mfx.FrameInfo.CropW :  par.mfx.FrameInfo.Width;
        sps.max_frame_height = par.mfx.FrameInfo.CropH!=0 ? par.mfx.FrameInfo.CropH :  par.mfx.FrameInfo.Height;

        sps.kf_auto         = 0;
        sps.kf_min_dist     = 1;
        sps.kf_max_dist     = par.mfx.GopRefDist;
        sps.bits_per_second = par.mfx.TargetKbps * 1000;
        sps.intra_period    = par.mfx.GopPicSize;
    } // void FillSpsBuffer(mfxVideoParam const & par, VAEncSequenceParameterBufferVP9 & sps)

    mfxStatus FillPpsBuffer(
        Task const & task,
        mfxVideoParam const & /*par*/,
        VAEncPictureParameterBufferVP9 & pps,
        std::vector<ExtVASurface> const & reconQueue,
        BitOffsets const &offsets)
    {
        VP9_LOG("FillPpsBuffer \n");

        Zero(pps);

        pps.frame_width_dst     = pps.frame_width_src  = static_cast<mfxU16>(task.m_frameParam.width);
        pps.frame_height_dst    = pps.frame_height_src = static_cast<mfxU16>(task.m_frameParam.height);

        MFX_CHECK(task.m_pRecFrame->idInPool < reconQueue.size(), MFX_ERR_UNDEFINED_BEHAVIOR);

        pps.reconstructed_frame = reconQueue[task.m_pRecFrame->idInPool].surface;

        pps.ref_flags.bits.force_kf = 0;

        VP9FrameLevelParam const &framePar = task.m_frameParam;
        {
            mfxU16 ridx = 0;
            while (ridx < 8)
                pps.reference_frames[ridx++] = VA_INVALID_SURFACE;
            ridx = 0;

            pps.refresh_frame_flags = 0;

            for (mfxU8 i = 0; i < DPB_SIZE; i++)
                pps.refresh_frame_flags |= (framePar.refreshRefFrames[i] << i);

            if (task.m_pRecRefFrames[REF_LAST] && task.m_pRecRefFrames[REF_LAST]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_LAST]->idInPool].surface;
                pps.ref_flags.bits.ref_last_idx = ridx;
                //(ref_frame_ctrl & 0x01 == 1) indicates that last frame is used as reference frame
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x01;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x01;
                ridx ++;
            }

            if (task.m_pRecRefFrames[REF_GOLD] && task.m_pRecRefFrames[REF_GOLD]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_GOLD]->idInPool].surface;
                pps.ref_flags.bits.ref_gf_idx = ridx;
                //(ref_frame_ctrl & 0x02 == 1) indicates that golden frame is used as reference frame
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x02;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x02;
                ridx ++;
            }

            if (task.m_pRecRefFrames[REF_ALT] && task.m_pRecRefFrames[REF_ALT]->idInPool < reconQueue.size())
            {
                pps.reference_frames[ridx] = reconQueue[task.m_pRecRefFrames[REF_ALT]->idInPool].surface;
                pps.ref_flags.bits.ref_arf_idx = ridx;
                //(ref_frame_ctrl & 0x04 == 1) indicates that alt frame is used as reference frame
                pps.ref_flags.bits.ref_frame_ctrl_l0 |= 0x04;
                pps.ref_flags.bits.ref_frame_ctrl_l1 |= 0x04;
                ridx ++;
            }
        }

        pps.pic_flags.bits.frame_type               = framePar.frameType;
        pps.pic_flags.bits.show_frame               = framePar.showFrame;
        pps.pic_flags.bits.error_resilient_mode     = framePar.errorResilentMode;
        pps.pic_flags.bits.intra_only               = framePar.intraOnly;
        pps.pic_flags.bits.segmentation_enabled     = framePar.segmentation != NO_SEGMENTATION;
        pps.pic_flags.bits.refresh_frame_context    = framePar.refreshFrameContext;
        pps.pic_flags.bits.frame_context_idx        = framePar.frameContextIdx;
        pps.pic_flags.bits.allow_high_precision_mv  = framePar.allowHighPrecisionMV;

        if (pps.pic_flags.bits.show_frame == 0)
                pps.pic_flags.bits.super_frame_flag = 1;

        //segmentation functionality is not fully implemented yet
        if (framePar.segmentation == APP_SEGMENTATION)
        {
            //pps.pic_flags.bits.seg_id_block_size = BLOCK_16x16; //no such parameter in va
            pps.pic_flags.bits.segmentation_update_map = task.m_frameParam.segmentationUpdateMap;
            pps.pic_flags.bits.segmentation_temporal_update = task.m_frameParam.segmentationTemporalUpdate;
            pps.pic_flags.bits.auto_segmentation = false; //not supported for now
        }

        pps.luma_ac_qindex         = framePar.baseQIndex;
        pps.luma_dc_qindex_delta   = framePar.qIndexDeltaLumaDC;
        pps.chroma_ac_qindex_delta = framePar.qIndexDeltaChromaAC;
        pps.chroma_dc_qindex_delta = framePar.qIndexDeltaChromaDC;

        pps.filter_level = framePar.lfLevel;
        pps.sharpness_level = framePar.sharpness;

        for (mfxU16 i = 0; i < 4; i ++)
            pps.ref_lf_delta[i] = framePar.lfRefDelta[i];

        for (mfxU16 i = 0; i < 2; i ++)
            pps.mode_lf_delta[i] = framePar.lfModeDelta[i];

        pps.ref_flags.bits.temporal_id = static_cast<mfxU8>(framePar.temporalLayer); //Temporal scalability is not fully implemented yet

        pps.bit_offset_ref_lf_delta         = offsets.BitOffsetForLFRefDelta;
        pps.bit_offset_mode_lf_delta        = offsets.BitOffsetForLFModeDelta;
        pps.bit_offset_lf_level             = offsets.BitOffsetForLFLevel;
        pps.bit_offset_qindex               = offsets.BitOffsetForQIndex;
        pps.bit_offset_first_partition_size = offsets.BitOffsetForFirstPartitionSize;
        pps.bit_offset_segmentation         = offsets.BitOffsetForSegmentation;
        pps.bit_size_segmentation           = offsets.BitSizeForSegmentation;

        //Tiles functionality is not fully implemented yet
        pps.log2_tile_columns   = framePar.log2TileCols;
        pps.log2_tile_rows      = framePar.log2TileRows;

        return MFX_ERR_NONE;
    } // mfxStatus FillPpsBuffer(...)

    mfxStatus FillSegMap(
        Task const & task,
        mfxVideoParam const & /*par*/,
        mfxCoreInterface *    pCore,
        VAEncMiscParameterTypeVP9PerSegmantParam & segPar)
    {
        VP9_LOG("FillSegMap \n");

        if (task.m_frameParam.segmentation == 0)
            return MFX_ERR_NONE; // segment map isn't required

        mfxFrameData segMap = {};

        FrameLocker lock(pCore, segMap, task.m_pSegmentMap->pSurface->Data.MemId);
        mfxU8 *pBuf = segMap.Y;
        if (pBuf == 0)
            return MFX_ERR_LOCK_MEMORY;


        mfxExtVP9Segmentation const & seg = GetActualExtBufferRef(*task.m_pParam, task.m_ctrl);
        Zero(segPar);

        mfxFrameInfo const & dstFi = task.m_pSegmentMap->pSurface->Info;
        mfxU32 dstW = dstFi.Width;
        mfxU32 dstH = dstFi.Height;
        mfxU32 dstPitch = segMap.Pitch;

        mfxFrameInfo const & srcFi = task.m_pRawFrame->pSurface->Info;
        mfxU16 srcBlockSize = MapIdToBlockSize(seg.SegmentIdBlockSize);
        mfxU32 srcW = (srcFi.Width + srcBlockSize - 1)  / srcBlockSize;
        mfxU32 srcH = (srcFi.Height + srcBlockSize - 1) / srcBlockSize;
        // driver seg map is always in 16x16 blocks because of HW limitation
        const mfxU16 dstBlockSize = 16;
        mfxU16 ratio = srcBlockSize / dstBlockSize;

        if (seg.NumSegmentIdAlloc < srcW * srcH || seg.SegmentId == 0 ||
            srcW != (dstW + ratio - 1) / ratio || srcH != (dstH + ratio - 1) / ratio)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        for (mfxI8 i = seg.NumSegments - 1; i >= 0; i --)
        {
            VAEncSegParamVP9 & segva  = segPar.seg_data[int(i)];
            mfxVP9SegmentParam const & segmfx = seg.Segment[int(i)];

            mfxI16 qIndexDelta = segmfx.QIndexDelta;
            CheckAndFixQIndexDelta(qIndexDelta, task.m_frameParam.baseQIndex);
            segva.segment_lf_level_delta = static_cast<mfxI8>(segmfx.LoopFilterLevelDelta);
            segva.segment_qindex_delta = qIndexDelta;

            if (IsFeatureEnabled(segmfx.FeatureEnabled, FEAT_REF))
            {
                    segva.seg_flags.bits.segment_reference_enabled = 1;
                    segva.seg_flags.bits.segment_reference = ConvertSegmentRefControlToVAAPI(segmfx.ReferenceFrame);
            }

            if (IsFeatureEnabled(segmfx.FeatureEnabled, FEAT_SKIP))
            {
                    segva.seg_flags.bits.segment_reference_skipped = 1;
            }
        }

        // for now application seg map is accepted in 32x32 and 64x64 blocks
        // and driver seg map is always in 16x16 blocks
        // need to map one to another

        for (mfxU32 i = 0; i < dstH; i++)
        {
            for (mfxU32 j = 0; j < dstW; j++)
            {
                segMap.Y[i * dstPitch + j] = seg.SegmentId[(i / ratio) * srcW + j / ratio];
            }
        }

        return MFX_ERR_NONE;
    } //mfxStatus FillSegMap(..)

void FillBrcStructures(
    mfxVideoParam const & par,
    VAEncMiscParameterRateControl & vaBrcPar,
    VAEncMiscParameterFrameRate   & vaFrameRate)
{
    VP9_LOG("FillBrcStructures \n");

    Zero(vaBrcPar);
    Zero(vaFrameRate);

    vaBrcPar.bits_per_second = par.mfx.MaxKbps * 1000;
    if(par.mfx.MaxKbps)
        vaBrcPar.target_percentage = (unsigned int)(100.0 * (mfxF64)par.mfx.TargetKbps / (mfxF64)par.mfx.MaxKbps);
    PackMfxFrameRate(par.mfx.FrameInfo.FrameRateExtN, par.mfx.FrameInfo.FrameRateExtD, vaFrameRate.framerate);
} // void FillBrcStructures(

mfxStatus SetRateControl(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & rateParamBuf_id,
    bool         isBrcResetRequired = false)
{
    VP9_LOG("SetRateControl \n");

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterRateControl *rate_param;

    if ( rateParamBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, rateParamBuf_id);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
                    1,
                    NULL,
                    &rateParamBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    vaSts = vaMapBuffer(m_vaDisplay,
                        rateParamBuf_id,
                        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeRateControl;
    rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        rate_param->bits_per_second = par.mfx.MaxKbps * 1000;

        if(par.mfx.MaxKbps)
            rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.mfx.TargetKbps / (mfxF64)par.mfx.MaxKbps);

        rate_param->rc_flags.bits.reset = isBrcResetRequired;
    }
    vaUnmapBuffer(m_vaDisplay, rateParamBuf_id);

    return MFX_ERR_NONE;
} //mfxStatus SetRateControl(..)

mfxStatus SetHRD(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & hrdBuf_id)
{
    VP9_LOG("SetHRD \n");

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterHRD *hrd_param;

    if ( hrdBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, hrdBuf_id);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterHRD),
                    1,
                    NULL,
                    &hrdBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                        hrdBuf_id,
                        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeHRD;
    hrd_param = (VAEncMiscParameterHRD *)misc_param->data;

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        hrd_param->initial_buffer_fullness = par.mfx.InitialDelayInKB * 8000;
        hrd_param->buffer_size = par.mfx.BufferSizeInKB * 8000;
    }
    else
    {
        hrd_param->initial_buffer_fullness = 0;
        hrd_param->buffer_size = 0;
    }

    vaUnmapBuffer(m_vaDisplay, hrdBuf_id);

    return MFX_ERR_NONE;
} //mfxStatus SetHRD(..)

mfxStatus SetQualityLevel(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & qualityLevelBuf_id)
{
    VP9_LOG("SetQualityLevel \n");

    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterBufferQualityLevel *quality_param;

    if ( qualityLevelBuf_id != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, qualityLevelBuf_id);
    }
    vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterBufferQualityLevel),
                    1,
                    NULL,
                    &qualityLevelBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                        qualityLevelBuf_id,
                        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeQualityLevel;
    quality_param = (VAEncMiscParameterBufferQualityLevel *)misc_param->data;

    quality_param->quality_level = par.mfx.TargetUsage;

    vaUnmapBuffer(m_vaDisplay, qualityLevelBuf_id);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} //mfxStatus SetQualityLevel(..)

mfxStatus SetFrameRate(
    mfxVideoParam const & par,
    VADisplay    m_vaDisplay,
    VAContextID  m_vaContextEncode,
    VABufferID & frameRateBufId)
{
    VP9_LOG("SetFrameRate \n");
    VAStatus vaSts;
    VAEncMiscParameterBuffer *misc_param;
    VAEncMiscParameterFrameRate *frameRate_param;

    if ( frameRateBufId != VA_INVALID_ID)
    {
        vaDestroyBuffer(m_vaDisplay, frameRateBufId);
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
                    1,
                    NULL,
                    &frameRateBufId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaMapBuffer(m_vaDisplay,
                        frameRateBufId,
                        (void **)&misc_param);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    misc_param->type = VAEncMiscParameterTypeFrameRate;
    frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

    PackMfxFrameRate(par.mfx.FrameInfo.FrameRateExtN, par.mfx.FrameInfo.FrameRateExtD, frameRate_param->framerate);

    vaUnmapBuffer(m_vaDisplay, frameRateBufId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // mfxStatus SetFrameRate(..)

VAAPIEncoder::VAAPIEncoder()
: m_pmfxCore(NULL)
, m_vaDisplay(NULL)
, m_vaContextEncode(VA_INVALID_ID)
, m_vaConfig(VA_INVALID_ID)
, m_spsBufferId(VA_INVALID_ID)
, m_ppsBufferId(VA_INVALID_ID)
, m_segMapBufferId(VA_INVALID_ID)
, m_segParBufferId(VA_INVALID_ID)
, m_frameRateBufferId(VA_INVALID_ID)
, m_rateCtrlBufferId(VA_INVALID_ID)
, m_hrdBufferId(VA_INVALID_ID)
, m_qualityLevelBufferId(VA_INVALID_ID)
, m_packedHeaderParameterBufferId(VA_INVALID_ID)
, m_packedHeaderDataBufferId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)


VAAPIEncoder::~VAAPIEncoder()
{
    VP9_LOG(" \n~VAAPIEncoder");
    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

VAProfile ConvertGuidToVAAPIProfile(const GUID& guid)
{
    if (guid == MfxHwVP9Encode::DXVA2_Intel_LowpowerEncode_VP9_Profile0)
        return VAProfileVP9Profile0;
    if (guid == MfxHwVP9Encode::DXVA2_Intel_LowpowerEncode_VP9_Profile1)
        return VAProfileVP9Profile1;
    if (guid == MfxHwVP9Encode::DXVA2_Intel_LowpowerEncode_VP9_10bit_Profile2)
        return VAProfileVP9Profile2;
    if (guid == MfxHwVP9Encode::DXVA2_Intel_LowpowerEncode_VP9_10bit_Profile3)
        return VAProfileVP9Profile3;
    return VAProfileNone; /// Lowpower == OFF is not supported yet.
}


#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    mfxCoreInterface* pCore,
    GUID guid,
    mfxU32 width,
    mfxU32 height)
{
    VP9_LOG(" \nCreateAuxilliaryDevice");

    MFX_CHECK_WITH_ASSERT(pCore != 0, MFX_ERR_NULL_PTR);
    m_pmfxCore = pCore;

    mfxStatus mfxSts = pCore->GetHandle(pCore->pthis, MFX_HANDLE_VA_DISPLAY, &m_vaDisplay);
    MFX_CHECK_STS(mfxSts);

    mfxStatus sts = pCore->QueryPlatform(pCore->pthis, &m_platform);
    MFX_CHECK_STS(sts);

    m_width  = width;
    m_height = height;

    // set encoder CAPS on our own for now
    memset(&m_caps, 0, sizeof(m_caps));

    std::map<VAConfigAttribType, int> idx_map;
    VAConfigAttribType attr_types[] = {
        VAConfigAttribRTFormat,
        VAConfigAttribEncDirtyRect,
        VAConfigAttribMaxPictureWidth,
        VAConfigAttribMaxPictureHeight,
        VAConfigAttribEncTileSupport,
        VAConfigAttribEncRateControlExt,
        VAConfigAttribEncParallelRateControl,
        VAConfigAttribFrameSizeToleranceSupport,
        VAConfigAttribProcessingRate,
        VAConfigAttribEncDynamicScaling,
    };
    std::vector<VAConfigAttrib> attrs;

    for (size_t i = 0; i < sizeof(attr_types) / sizeof(attr_types[0]); i++) {
        attrs.push_back({attr_types[i], 0});
        idx_map[ attr_types[i] ] = i;
    }

    VAStatus vaSts = vaGetConfigAttributes(m_vaDisplay,
                          ConvertGuidToVAAPIProfile(guid),
                          VAEntrypointEncSliceLP,
                          attrs.data(),
                          (int)attrs.size());
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_caps.CodingLimitSet = 1;
    m_caps.Color420Only =  1; // See DDI

    if (m_platform.CodeName >= MFX_PLATFORM_ICELAKE)
    {
        m_caps.Color420Only = 0;
        m_caps.MaxEncodedBitDepth = 1; //0: 8bit, 1: 8 and 10 bit;
        m_caps.NumScalablePipesMinus1 = 1;
    }
    if (attrs[idx_map[VAConfigAttribRTFormat]].value != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.YUV422ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV422 ? 1 : 0;
        m_caps.YUV444ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV444 ? 1 : 0;
    }

    if (attrs[idx_map[VAConfigAttribEncDirtyRect]].value != VA_ATTRIB_NOT_SUPPORTED &&
        attrs[idx_map[VAConfigAttribEncDirtyRect]].value != 0)
    {
        m_caps.DirtyRectSupport = 1;
        m_caps.MaxNumOfDirtyRect =attrs[idx_map[VAConfigAttribEncDirtyRect]].value;
    }

    if (attrs[idx_map[VAConfigAttribMaxPictureWidth]].value != VA_ATTRIB_NOT_SUPPORTED)
        m_caps.MaxPicWidth = attrs[idx_map[VAConfigAttribMaxPictureWidth]].value;

    if (attrs[idx_map[VAConfigAttribMaxPictureHeight]].value != VA_ATTRIB_NOT_SUPPORTED)
        m_caps.MaxPicHeight = attrs[idx_map[VAConfigAttribMaxPictureHeight]].value;

    if (attrs[idx_map[VAConfigAttribEncTileSupport]].value != VA_ATTRIB_NOT_SUPPORTED)
        m_caps.TileSupport = attrs[idx_map[VAConfigAttribEncTileSupport]].value;

    if (attrs[idx_map[VAConfigAttribEncRateControlExt]].value != VA_ATTRIB_NOT_SUPPORTED)
    {
        VAConfigAttribValEncRateControlExt rateControlConf;
        rateControlConf.value = attrs[idx_map[VAConfigAttribEncRateControlExt]].value;
        m_caps.TemporalLayerRateCtrl = rateControlConf.bits.max_num_temporal_layers_minus1;
    }

    m_caps.ForcedSegmentationSupport = 1;
    m_caps.AutoSegmentationSupport = 1;

    if (attrs[idx_map[VAConfigAttribEncMacroblockInfo]].value != VA_ATTRIB_NOT_SUPPORTED &&
        attrs[idx_map[VAConfigAttribEncMacroblockInfo]].value)
        m_caps.SegmentFeatureSupport &= 0b0001;

    if (attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value != VA_ATTRIB_NOT_SUPPORTED &&
        attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value)
        m_caps.SegmentFeatureSupport &= 0b0100;

    if (attrs[idx_map[VAConfigAttribEncSkipFrame]].value != VA_ATTRIB_NOT_SUPPORTED &&
        attrs[idx_map[VAConfigAttribEncSkipFrame]].value)
        m_caps.SegmentFeatureSupport &= 0b1000;

    if (attrs[idx_map[VAConfigAttribEncDynamicScaling]].value != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.DynamicScaling = attrs[idx_map[VAConfigAttribEncDynamicScaling]].value;
    }

    if (attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value != VA_ATTRIB_NOT_SUPPORTED)
        m_caps.UserMaxFrameSizeSupport  = attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value;

    if (attrs[idx_map[VAConfigAttribProcessingRate]].value != VA_ATTRIB_NOT_SUPPORTED)
    {
        m_caps.FrameLevelRateCtrl = attrs[idx_map[VAConfigAttribProcessingRate]].value == VA_PROCESSING_RATE_ENCODE;
        m_caps.BRCReset = attrs[idx_map[VAConfigAttribProcessingRate]].value == VA_PROCESSING_RATE_ENCODE;
    }
    m_caps.EncodeFunc = 1;
    m_caps.HybridPakFunc = 1;

    VP9_LOG(" \n CreateAuxilliaryDevice return ERR_NONE \n");
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(VideoCORE* core, GUID guid, mfxU32 width, mfxU32 height)


mfxStatus VAAPIEncoder::CreateAccelerationService(VP9MfxVideoParam const & par)
{
    VP9_LOG(" \n CreateAccelerationService");
    if(0 == m_reconQueue.size())
    {
    /* We need to pass reconstructed surfaces when call vaCreateContext().
     * Here we don't have this info.
     */
        m_video = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    VAProfile va_profile = ConvertGuidToVAAPIProfile(GetGuid(par));
    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                va_profile,
                pEntrypoints.data(),
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        if( VAEntrypointEncSliceLP == pEntrypoints[entrypointsIndx] )
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

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          va_profile,
                          (VAEntrypoint)VAEntrypointEncSliceLP,
                          &attrib[0], 2);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU8 vaRTFormat = ConvertRTFormatMFX2VAAPI(par.mfx.FrameInfo.ChromaFormat);
    if ((attrib[0].value & vaRTFormat) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = vaRTFormat;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        va_profile,
        (VAEntrypoint)VAEntrypointEncSliceLP,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> reconSurf;
    for(unsigned int i = 0; i < m_reconQueue.size(); i++)
        reconSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        VA_PROGRESSIVE,
        reconSurf.data(),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK(VA_STATUS_ERROR_RESOLUTION_NOT_SUPPORTED != vaSts, MFX_ERR_UNSUPPORTED);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    Zero(m_sps);
    Zero(m_pps);

    //------------------------------------------------------------------

    FillSpsBuffer(par, m_sps);
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    m_isBrcResetRequired = false;

    mfxStatus mfxSts;
    mfxSts = SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateCtrlBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);

    m_frameHeaderBuf.resize(VP9_MAX_UNCOMPRESSED_HEADER_SIZE + MAX_IVF_HEADER_SIZE);
    InitVp9SeqLevelParam(par, m_seqParam);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)


mfxStatus VAAPIEncoder::Reset(VP9MfxVideoParam const & par)
{
    VP9_LOG(" \nReset");
    m_video = par;

    FillSpsBuffer(par, m_sps);
    VAEncMiscParameterRateControl oldBrcPar = m_vaBrcPar;
    VAEncMiscParameterFrameRate oldFrameRate = m_vaFrameRate;
    FillBrcStructures(par, m_vaBrcPar, m_vaFrameRate);
    m_isBrcResetRequired = memcmp(&m_vaBrcPar, &oldBrcPar, sizeof(oldBrcPar)) || memcmp(&m_vaFrameRate, &oldFrameRate, sizeof (oldFrameRate));

    mfxStatus mfxSts;
    mfxSts = SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateCtrlBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetQualityLevel(par, m_vaDisplay, m_vaContextEncode, m_qualityLevelBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);
    mfxSts = SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateBufferId);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == mfxSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Reset(MfxVideoParam const & par)

mfxU32 VAAPIEncoder::GetReconSurfFourCC()
{
    VP9_LOG(" \nGetReconSurfFourCC");
    return MFX_FOURCC_VP9_NV12;
} // mfxU32 VAAPIEncoder::GetReconSurfFourCC()

mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)
{
    VP9_LOG(" \nQueryCompBufferInfo");
    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        request.Info.FourCC = MFX_FOURCC_P8;
    }
    else if (type == D3DDDIFMT_INTELENCODE_MBSEGMENTMAP)
    {
        request.Info.FourCC = MFX_FOURCC_VP9_SEGMAP;
        request.Info.Width  = ALIGN(frameWidth >> 5, 64);
        request.Info.Height = frameHeight >> 5;//VDENC 32x32 block size ??
    }

    request.AllocId = m_vaContextEncode;

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request, mfxU32 frameWidth, mfxU32 frameHeight)


mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS_VP9& caps)
{
    VP9_LOG(" \nQueryEncodeCaps");
    caps = m_caps;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)

mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    VP9_LOG(" \nRegister");
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_MBSEGMENTMAP == type)
        pQueue = &m_segMapQueue;
    else if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
        pQueue = &m_bsQueue;
    else
        pQueue = &m_reconQueue;

    // we should register allocated HW bitstreams and recon surfaces
    MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

    ExtVASurface extSurf {VA_INVALID_SURFACE, 0, 0};
    VASurfaceID *pSurface = NULL;

    mfxFrameAllocator & allocator = m_pmfxCore->FrameAllocator;

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        sts = allocator.GetHDL(allocator.pthis, response.mids[i], (mfxHDL *)&pSurface);
        MFX_CHECK_STS(sts);

        extSurf.number  = i;
        extSurf.surface = *pSurface;

        pQueue->push_back( extSurf );
    }

    if( D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type &&
        D3DDDIFMT_INTELENCODE_MBSEGMENTMAP != type)
    {
        sts = CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIEncoder::Register(mfxMemId /*memId*/, D3DDDIFORMAT /*type*/)
{
    VP9_LOG(" \nRegister");
    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VAAPIEncoder::Register(mfxMemId memId, D3DDDIFORMAT type)

mfxStatus VAAPIEncoder::Execute(
    Task const & task,
    mfxHDLPair pair)
{
    VAStatus vaSts;
    VP9_LOG("\nVAAPIEncoder::Execute");

    VASurfaceID *inputSurface = (VASurfaceID*)pair.first;
    VABufferID  codedBuffer;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT);
    mfxU16 buffersCount = 0;

    // prepare frame header: write IVF and uncompressed header, calculate bit offsets
    BitOffsets offsets;
    mfxU8 * pBuf = &m_frameHeaderBuf[0];
    Zero(m_frameHeaderBuf);

    mfxU16 bytesWritten = PrepareFrameHeader(*task.m_pParam, pBuf, (mfxU32)m_frameHeaderBuf.size(), task, m_seqParam, offsets);

    // update params
    FillPpsBuffer(task, m_video, m_pps, m_reconQueue, offsets);
    FillSegMap(task, m_video, m_pmfxCore, m_segPar);

//===============================================================================================

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxInPool = task.m_pOutBs->idInPool;
    if( idxInPool < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[idxInPool].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    m_pps.coded_buf = codedBuffer;

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
        //segmentation functionality is not fully implemented yet
        if (task.m_frameParam.segmentation == APP_SEGMENTATION)
        {
            // 4. Segmentation map

            // segmentation map buffer is already allocated and filled. Need just to attach it
            configBuffers[buffersCount++] = m_segMapBufferId;

            // 5. Per-segment parameters
            MFX_DESTROY_VABUFFER(m_segParBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAQMatrixBufferType,
                                   sizeof(m_segPar),
                                   1,
                                   &m_segPar,
                                   &m_segParBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_segParBufferId;
        }

        //packed header data

        ENCODE_PACKEDHEADER_DATA packedData = MakePackedByteBuffer(pBuf, bytesWritten);
        VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

        packed_header_param_buffer.type = VAEncPackedHeaderRawData;
        packed_header_param_buffer.has_emulation_bytes = 1;
        packed_header_param_buffer.bit_length = packedData.DataLength*8;

        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedHeaderParameterBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_packedHeaderParameterBufferId;

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedData.DataLength,
                            1,
                            packedData.pData,
                            &m_packedHeaderDataBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_packedHeaderDataBufferId;

//*********

        // 8. hrd parameters
        configBuffers[buffersCount++] = m_hrdBufferId;
        // 9. RC parameters
        SetRateControl(m_video, m_vaDisplay, m_vaContextEncode, m_rateCtrlBufferId, m_isBrcResetRequired);
        m_isBrcResetRequired = false; // reset BRC only once
        configBuffers[buffersCount++] = m_rateCtrlBufferId;
        // 10. frame rate
        configBuffers[buffersCount++] = m_frameRateBufferId;
        // 11. quality level
        configBuffers[buffersCount++] = m_qualityLevelBufferId;
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        vaSts = vaBeginPicture(
            m_vaDisplay,
            m_vaContextEncode,
            *inputSurface);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        vaSts = vaRenderPicture(
            m_vaDisplay,
            m_vaContextEncode,
            configBuffers.data(),
            buffersCount);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL_VTUNE, "vaEndPicture");//??
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        ExtVASurface currentFeedback;
        currentFeedback.surface = *inputSurface;
        currentFeedback.number = task.m_frameOrder;
        currentFeedback.idxBs    = idxInPool;
        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIEncoder::QueryPlatform(mfxPlatform& platform)
{
    VP9_LOG(" \nVAAPIEncoder::QueryPlatform");
    platform = m_platform;
    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryStatus(
    Task & task)
{
    VAStatus vaSts;
    VP9_LOG("\nVAAPIEncoder::QueryStatus +");

    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & mb data buffer
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;
    mfxStatus sts = MFX_ERR_NONE;

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
        {
            ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];

            if( currentFeedback.number == task.m_frameOrder)
            {
                waitSurface = currentFeedback.surface;
                waitIdxBs   = currentFeedback.idxBs;

                isFound  = true;

                break;
            }
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    VABufferID codedBuffer;
    if( waitIdxBs >= m_bsQueue.size())
    {
        return MFX_ERR_UNKNOWN;
    }
    else
        codedBuffer = m_bsQueue[waitIdxBs].surface;

    VASurfaceStatus surfSts = VASurfaceSkipped;
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        m_feedbackCache.erase( m_feedbackCache.begin() + indxSurf );
        guard.Unlock();
    }
    surfSts = VASurfaceReady;
#else

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);

    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (VASurfaceReady == surfSts)
    {
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
        guard.Unlock();
    }
#endif

    switch (surfSts)
    {
        case VASurfaceReady:

            VACodedBufferSegment *codedBufferSegment;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        codedBuffer,
                        (void **)(&codedBufferSegment));
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }

                task.m_bsDataLength = codedBufferSegment->size;

                if (codedBufferSegment->status & VA_CODED_BUF_STATUS_BAD_BITSTREAM)
                    sts = MFX_ERR_GPU_HANG;
                else if (!codedBufferSegment->size || !codedBufferSegment->buf)
                    sts = MFX_ERR_DEVICE_FAILED;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
                    vaSts = vaUnmapBuffer( m_vaDisplay, codedBuffer );
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                return sts;

        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
        default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
    }
} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)


mfxStatus VAAPIEncoder::Destroy()
{
    VP9_LOG(" \nDestroy");

    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_segMapBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_segParBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_frameRateBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_rateCtrlBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_hrdBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_qualityLevelBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedHeaderParameterBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedHeaderDataBufferId, m_vaDisplay);

    if(m_vaContextEncode != VA_INVALID_ID)
    {
        vaDestroyContext(m_vaDisplay, m_vaContextEncode);
        m_vaContextEncode = VA_INVALID_ID;
    }

    if(m_vaConfig != VA_INVALID_ID)
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = VA_INVALID_ID;
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Destroy()
#endif // (MFX_VA_LINUX)

} // MfxHwVP9Encode

#endif // !(_WIN32) && !(_WIN64)
