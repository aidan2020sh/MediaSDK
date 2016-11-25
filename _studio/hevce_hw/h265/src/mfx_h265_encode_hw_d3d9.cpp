//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2016 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_common.h"
#include "mfx_h265_encode_hw_d3d9.h"

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    slice.resize(par.m_slice.size());

    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];
        Zero(cs);

        cs.slice_id              = i;
        cs.slice_segment_address = par.m_slice[i].SegmentAddress;
        cs.NumLCUsInSlice        = par.m_slice[i].NumLCU;
        cs.bLastSliceOfPic       = (i == slice.size() - 1);
    }
}

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice)
{
    for (mfxU16 i = 0; i < slice.size(); i ++)
    {
        ENCODE_SET_SLICE_HEADER_HEVC & cs = slice[i];

        cs.slice_type                           = task.m_sh.type;
        if (cs.slice_type != 2)
        {
            Copy(cs.RefPicList, task.m_refPicList);
            cs.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

            if (cs.slice_type == 0)
                cs.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;
        }

        cs.bLastSliceOfPic                      = (i == slice.size() - 1);
        cs.dependent_slice_segment_flag         = task.m_sh.dependent_slice_segment_flag;
        cs.slice_temporal_mvp_enable_flag       = task.m_sh.temporal_mvp_enabled_flag;
        cs.slice_sao_luma_flag                  = task.m_sh.sao_luma_flag;
        cs.slice_sao_chroma_flag                = task.m_sh.sao_chroma_flag;
        cs.mvd_l1_zero_flag                     = task.m_sh.mvd_l1_zero_flag;
        cs.cabac_init_flag                      = task.m_sh.cabac_init_flag;
        cs.slice_deblocking_filter_disable_flag = task.m_sh.deblocking_filter_disabled_flag;
        cs.collocated_from_l0_flag              = task.m_sh.collocated_from_l0_flag;

        cs.slice_qp_delta       = task.m_sh.slice_qp_delta;
        cs.slice_cb_qp_offset   = task.m_sh.slice_cb_qp_offset;
        cs.slice_cr_qp_offset   = task.m_sh.slice_cr_qp_offset;
        cs.beta_offset_div2     = task.m_sh.beta_offset_div2;
        cs.tc_offset_div2       = task.m_sh.tc_offset_div2;

        //if (pps.weighted_pred_flag || pps.weighted_bipred_flag)
        //{
        //    cs.luma_log2_weight_denom;
        //    cs.chroma_log2_weight_denom;
        //    cs.luma_offset[2][15];
        //    cs.delta_luma_weight[2][15];
        //    cs.chroma_offset[2][15][2];
        //    cs.delta_chroma_weight[2][15][2];
        //}

        cs.MaxNumMergeCand = 5 - task.m_sh.five_minus_max_num_merge_cand;
    }
}

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps)
{
    Zero(sps);

    sps.log2_min_coding_block_size_minus3 = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;

    sps.wFrameWidthInMinCbMinus1 = (mfxU16)(par.m_sps.pic_width_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.wFrameHeightInMinCbMinus1 = (mfxU16)(par.m_sps.pic_height_in_luma_samples / (1 << (sps.log2_min_coding_block_size_minus3 + 3)) - 1);
    sps.general_profile_idc = par.m_sps.general.profile_idc;
    sps.general_level_idc   = par.m_sps.general.level_idc;
    sps.general_tier_flag   = par.m_sps.general.tier_flag;

    sps.GopPicSize          = par.mfx.GopPicSize;
    sps.GopRefDist          = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag          = mfxU8(par.mfx.GopOptFlag);

    sps.TargetUsage         = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod   = mfxU8(par.isSWBRC() ? MFX_RATECONTROL_CQP:par.mfx.RateControlMethod) ;
    if(par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        sps.ContentInfo = eContent_NonVideoScreen;
    }
    if (   par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
        || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        sps.MinBitRate      = 0;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.MaxKbps;
    }   

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        sps.MinBitRate      = par.TargetKbps;
        sps.TargetBitRate   = par.TargetKbps;
        sps.MaxBitRate      = par.TargetKbps;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        sps.TargetBitRate   = par.TargetKbps;
//        sps.AVBRAccuracy = par.mfx.Accuracy;
//        sps.AVBRConvergence = par.mfx.Convergence;
    }

    sps.FramesPer100Sec = (mfxU16)(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    sps.InitVBVBufferFullnessInBit = 8000 * par.InitialDelayInKB;
    sps.VBVBufferSizeInBit         = 8000 * par.BufferSizeInKB;

    sps.bResetBRC        = 0;
    sps.GlobalSearch     = 0;
    sps.LocalSearch      = 0;
    sps.EarlySkip        = 0;
    sps.MBBRC            = IsOn(par.m_ext.CO2.MBBRC);

    //WA:  Parallel BRC is switched on in sync & async mode (quality drop in noParallelBRC in driver)
    sps.ParallelBRC = (par.mfx.GopRefDist > 1) && (par.mfx.GopRefDist <= 8) && par.isBPyramid() && !IsOn(par.mfx.LowPower);

    if (par.m_ext.CO2.MaxSliceSize)
        sps.SliceSizeControl = 1;

    sps.UserMaxFrameSize = par.m_ext.CO2.MaxFrameSize;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        sps.ICQQualityFactor = (mfxU8)par.mfx.ICQQuality;
    else if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
        sps.ICQQualityFactor = (mfxU8)par.m_ext.CO3.QVBRQuality;
    else
        sps.ICQQualityFactor = 0;

    if (sps.ParallelBRC)
    {
        if (!par.isBPyramid())
        {
            sps.NumOfBInGop[0]  = (par.mfx.GopPicSize - 1) - (par.mfx.GopPicSize - 1)/par.mfx.GopRefDist;
            sps.NumOfBInGop[1]  = 0;
            sps.NumOfBInGop[2]  = 0;
        }
        else if (par.mfx.GopRefDist <=  8)
        {
            static UINT B[9]  = {0,0,1,1,1,1,1,1,1};
            static UINT B1[9] = {0,0,0,1,2,2,2,2,2};
            static UINT B2[9] = {0,0,0,0,0,1,2,3,4};

            mfxI32 numBpyr   = par.mfx.GopPicSize/par.mfx.GopRefDist;
            mfxI32 lastBpyrW = par.mfx.GopPicSize%par.mfx.GopRefDist;

            sps.NumOfBInGop[0]  = numBpyr*B[par.mfx.GopRefDist] + B[lastBpyrW];
            sps.NumOfBInGop[1]  = numBpyr*B1[par.mfx.GopRefDist]+ B1[lastBpyrW];
            sps.NumOfBInGop[2]  = numBpyr*B2[par.mfx.GopRefDist]+ B2[lastBpyrW];
        }
        else
        {
            assert(0);
        }
    }

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    switch (par.mfx.FrameInfo.BitDepthLuma)
    {
    case 16: sps.SourceBitDepth = 3; break;
    case 12: sps.SourceBitDepth = 2; break;
    case 10: sps.SourceBitDepth = 1; break;
    default: assert(!"undefined SourceBitDepth");
    case  8: sps.SourceBitDepth = 0; break;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4)
    {
        sps.SourceFormat = 3;
    }
    else
    {
        assert(par.mfx.FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV400 && par.mfx.FrameInfo.ChromaFormat <= MFX_CHROMAFORMAT_YUV444);
        sps.SourceFormat = par.mfx.FrameInfo.ChromaFormat - 1;
    }
#else
    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
        sps.SourceBitDepth = 1; //10b
#endif

    sps.scaling_list_enable_flag           = par.m_sps.scaling_list_enabled_flag;
    sps.sps_temporal_mvp_enable_flag       = par.m_sps.temporal_mvp_enabled_flag;
    sps.strong_intra_smoothing_enable_flag = par.m_sps.strong_intra_smoothing_enabled_flag;
    sps.amp_enabled_flag                   = par.m_sps.amp_enabled_flag;    // SKL: 0, CNL+: 1
    sps.SAO_enabled_flag                   = par.m_sps.sample_adaptive_offset_enabled_flag; // 0, 1
    sps.pcm_enabled_flag                   = par.m_sps.pcm_enabled_flag;
    sps.pcm_loop_filter_disable_flag       = 1;//par.m_sps.pcm_loop_filter_disabled_flag;
    sps.chroma_format_idc                  = par.m_sps.chroma_format_idc;
    sps.tiles_fixed_structure_flag         = par.m_sps.vui.tiles_fixed_structure_flag;
    sps.separate_colour_plane_flag         = par.m_sps.separate_colour_plane_flag;

    sps.log2_max_coding_block_size_minus3       = (mfxU8)(par.m_sps.log2_min_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_luma_coding_block_size);
    sps.log2_min_coding_block_size_minus3       = (mfxU8)par.m_sps.log2_min_luma_coding_block_size_minus3;
    sps.log2_max_transform_block_size_minus2    = (mfxU8)(par.m_sps.log2_min_transform_block_size_minus2
        + par.m_sps.log2_diff_max_min_transform_block_size);
    sps.log2_min_transform_block_size_minus2    = (mfxU8)par.m_sps.log2_min_transform_block_size_minus2;
    sps.max_transform_hierarchy_depth_intra     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_intra;
    sps.max_transform_hierarchy_depth_inter     = (mfxU8)par.m_sps.max_transform_hierarchy_depth_inter;
    sps.log2_min_PCM_cb_size_minus3             = (mfxU8)par.m_sps.log2_min_pcm_luma_coding_block_size_minus3;
    sps.log2_max_PCM_cb_size_minus3             = (mfxU8)(par.m_sps.log2_min_pcm_luma_coding_block_size_minus3
        + par.m_sps.log2_diff_max_min_pcm_luma_coding_block_size);
    sps.bit_depth_luma_minus8                   = (mfxU8)par.m_sps.bit_depth_luma_minus8;
    sps.bit_depth_chroma_minus8                 = (mfxU8)par.m_sps.bit_depth_chroma_minus8;
    sps.pcm_sample_bit_depth_luma_minus1        = (mfxU8)par.m_sps.pcm_sample_bit_depth_luma_minus1;
    sps.pcm_sample_bit_depth_chroma_minus1      = (mfxU8)par.m_sps.pcm_sample_bit_depth_chroma_minus1;

    // ROI
    if (par.m_ext.ROI.NumROI) {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            sps.ROIValueInDeltaQP = 1;
        else
            sps.ROIValueInDeltaQP = 0;  // 0 means Priorities (if supported in caps)
    }

    // if ENCODE_BLOCKQPDATA surface is provided
    //sps.BlockQPInDeltaQPIndex = 1;    // surface contains data for non-rect ROI
    //sps.BlockQPInDeltaQPIndex = 0;    // surface contains data for block level absolute QP value
}

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    Zero(pps);

    pps.tiles_enabled_flag               = par.m_pps.tiles_enabled_flag;
    pps.entropy_coding_sync_enabled_flag = par.m_pps.entropy_coding_sync_enabled_flag;
    pps.sign_data_hiding_flag            = par.m_pps.sign_data_hiding_enabled_flag;
    pps.constrained_intra_pred_flag      = par.m_pps.constrained_intra_pred_flag;
    pps.transform_skip_enabled_flag      = par.m_pps.transform_skip_enabled_flag;
    pps.transquant_bypass_enabled_flag   = par.m_pps.transquant_bypass_enabled_flag;
    pps.cu_qp_delta_enabled_flag         = par.m_pps.cu_qp_delta_enabled_flag;
    pps.weighted_pred_flag               = par.m_pps.weighted_pred_flag;
    pps.weighted_bipred_flag             = par.m_pps.weighted_pred_flag;
    pps.bEnableGPUWeightedPrediction     = 0;

    pps.loop_filter_across_slices_flag        = par.m_pps.loop_filter_across_slices_enabled_flag;
    pps.loop_filter_across_tiles_flag         = par.m_pps.loop_filter_across_tiles_enabled_flag;
    pps.scaling_list_data_present_flag        = par.m_pps.scaling_list_data_present_flag;
    pps.dependent_slice_segments_enabled_flag = par.m_pps.dependent_slice_segments_enabled_flag;
    pps.bLastPicInSeq                         = 0;
    pps.bLastPicInStream                      = 0;
    pps.bUseRawPicForRef                      = 0;
    pps.bEmulationByteInsertion               = 0; //!!!
    pps.bEnableRollingIntraRefresh            = 0;
    pps.BRCPrecision                          = 0;
    //pps.bScreenContent                        = 0;
    //pps.no_output_of_prior_pics_flag          = ;
    //pps.XbEnableRollingIntraRefreshX

    pps.QpY = mfxI8(par.m_pps.init_qp_minus26 + 26);

    pps.diff_cu_qp_delta_depth  = (mfxU8)par.m_pps.diff_cu_qp_delta_depth;
    pps.pps_cb_qp_offset        = (mfxU8)par.m_pps.cb_qp_offset;
    pps.pps_cr_qp_offset        = (mfxU8)par.m_pps.cr_qp_offset;
    pps.num_tile_columns_minus1 = (mfxU8)par.m_pps.num_tile_columns_minus1;
    pps.num_tile_rows_minus1    = (mfxU8)par.m_pps.num_tile_rows_minus1;

    Copy(pps.tile_column_width, par.m_pps.column_width);
    Copy(pps.tile_row_height, par.m_pps.row_height);

    pps.log2_parallel_merge_level_minus2 = (mfxU8)par.m_pps.log2_parallel_merge_level_minus2;

    pps.num_ref_idx_l0_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l0_default_active_minus1;
    pps.num_ref_idx_l1_default_active_minus1 = (mfxU8)par.m_pps.num_ref_idx_l1_default_active_minus1;

 

    pps.LcuMaxBitsizeAllowed       = 0;
    pps.bUseRawPicForRef           = 0;
    pps.NumSlices                  = par.mfx.NumSlice;

    pps.bEnableSliceLevelReport    = !!par.m_ext.CO2.MaxSliceSize;
    if (par.m_ext.CO2.MaxSliceSize)
        pps.MaxSliceSizeInBytes = par.m_ext.CO2.MaxSliceSize;

    // Max/Min QP settings for BRC

    /*if (par.mfx.FrameInfo.Height <= 576 &&
        par.mfx.FrameInfo.Width <= 736 &&
        par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        par.mfx.QPP < 32)
    {
        pps.bScreenContent = 1;
    }*/

    // ROI
    pps.NumROI = (mfxU8)par.m_ext.ROI.NumROI;
    if (pps.NumROI)
    {
        mfxU32 blkshift = par.m_sps.log2_min_luma_coding_block_size_minus3 + 3 +
            par.m_sps.log2_diff_max_min_luma_coding_block_size;

        for (mfxU16 i = 0; i < pps.NumROI; i ++)
        {
            pps.ROI[i].Roi.Left = (mfxU16)((par.m_ext.ROI.ROI[i].Left) >> blkshift);
            pps.ROI[i].Roi.Top = (mfxU16)((par.m_ext.ROI.ROI[i].Top) >> blkshift);
            pps.ROI[i].Roi.Right = (mfxU16)((par.m_ext.ROI.ROI[i].Right) >> blkshift);
            pps.ROI[i].Roi.Bottom = (mfxU16)((par.m_ext.ROI.ROI[i].Bottom) >> blkshift);
            pps.ROI[i].PriorityLevelOrDQp = (mfxU8)par.m_ext.ROI.ROI[i].Priority;
        }
        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

#if (HEVCE_DDI_VERSION >= 960)
    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif

}

void FillPpsBuffer(
    Task const & task,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps)
{
    pps.CurrOriginalPic.Index7Bits      = task.m_idxRec;
    pps.CurrOriginalPic.AssociatedFlag  = !!(task.m_frameType & MFX_FRAMETYPE_REF);

    pps.CurrReconstructedPic.Index7Bits     = task.m_idxRec;
    pps.CurrReconstructedPic.AssociatedFlag = !!task.m_ltr;

    if (task.m_sh.temporal_mvp_enabled_flag)
        pps.CollocatedRefPicIndex = task.m_refPicList[!task.m_sh.collocated_from_l0_flag][task.m_sh.collocated_ref_idx];
    else
        pps.CollocatedRefPicIndex = IDX_INVALID;

    for (mfxU16 i = 0; i < 15; i ++)
    {
        pps.RefFrameList[i].bPicEntry      = task.m_dpb[0][i].m_idxRec;
        pps.RefFrameList[i].AssociatedFlag = !!task.m_dpb[0][i].m_ltr;
        pps.RefFramePOCList[i] = task.m_dpb[0][i].m_poc;
    }

    // ROI
    pps.NumROI = (mfxU8)task.m_numRoi;
    if (pps.NumROI)
    {
        mfxU32 blkshift = 5;    // should be taken from ROICaps.ROIBlockSize

        for (mfxU16 i = 0; i < task.m_numRoi; i ++)
        {
            pps.ROI[i].Roi.Left = (mfxU16)((task.m_roi[i].Left) >> blkshift);
            pps.ROI[i].Roi.Top = (mfxU16)((task.m_roi[i].Top) >> blkshift);
            pps.ROI[i].Roi.Right = (mfxU16)((task.m_roi[i].Right) >> blkshift);
            pps.ROI[i].Roi.Bottom = (mfxU16)((task.m_roi[i].Bottom) >> blkshift);
            pps.ROI[i].PriorityLevelOrDQp = (mfxU8)task.m_roi[i].Priority;
        }
        pps.MaxDeltaQp = 51;    // is used for BRC only
        pps.MinDeltaQp = -51;
    }

#if (HEVCE_DDI_VERSION >= 960)
    // if ENCODE_BLOCKQPDATA surface is provided
    pps.NumDeltaQpForNonRectROI = 0;    // if no non-rect ROI
    // pps.NumDeltaQpForNonRectROI    // total number of different delta QPs for non-rect ROI ( + the same for rect ROI should not exceed MaxNumDeltaQP in caps
    // pps.NonRectROIDeltaQpList[0..pps.NumDeltaQpForNonRectROI-1] // delta QPs for non-rect ROI
#endif

    pps.CodingType      = task.m_codingType;
    pps.CurrPicOrderCnt = task.m_poc;

    pps.bEnableRollingIntraRefresh = task.m_IRState.refrType;

    if (pps.bEnableRollingIntraRefresh)
    {
        pps.IntraInsertionLocation = task.m_IRState.IntraLocation;
        pps.IntraInsertionSize = task.m_IRState.IntraSize;
        pps.QpDeltaForInsertedIntra = mfxU8(task.m_IRState.IntRefQPDelta);
    }
    pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

#if (HEVCE_DDI_VERSION >= 960)
    mfxExtAVCEncoderWiDiUsage* extWiDi = ExtBuffer::Get(task.m_ctrl);

    if (extWiDi)
        pps.InputType = eType_DRM_SECURE;
    else
        pps.InputType = eType_DRM_NONE;
#endif
}

void CachedFeedback::Reset(mfxU32 cacheSize, mfxU32 feedbackSize)
{
    m_cache.Reset(cacheSize, feedbackSize);

    for (size_t i = 0; i < m_cache.size(); i++)
        m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
}

mfxStatus CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        const ENCODE_QUERY_STATUS_PARAMS & u = update[i];

        if (u.bStatus != ENCODE_NOTAVAILABLE)
        {
            size_t cache = m_cache.size();

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                ENCODE_QUERY_STATUS_PARAMS & c = m_cache[j];

                if (c.StatusReportFeedbackNumber == u.StatusReportFeedbackNumber)
                {
                    cache = j;
                    break;
                }
                else if (cache == m_cache.size() && c.bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = j;
                }
            }
            MFX_CHECK(cache != m_cache.size(), MFX_ERR_UNDEFINED_BEHAVIOR);
            m_cache.copy(cache, update, i);
        }
    }
    return MFX_ERR_NONE;
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

mfxStatus CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNDEFINED_BEHAVIOR;
}


D3D9Encoder::D3D9Encoder()
    : m_core(0)
    , m_auxDevice(0)
    , m_guid()
    , m_width(0)
    , m_height(0)
    , m_caps()
    , m_capsQuery()
    , m_capsGet()
    , m_infoQueried(false)
    , m_widi(false)
    , m_maxSlices(0)
    , m_sps()
    , m_pps()
{
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAuxilliaryDevice");
    m_core = core;

#ifdef HEADER_PACKING_TEST
    m_guid      = guid;
    m_width     = width;
    m_height    = height;
    //m_auxDevice.reset((AuxiliaryDevice*) 0xFFFFFFFF);

    m_caps.CodingLimitSet           = 1;
    m_caps.BitDepth8Only            = 1;
    m_caps.Color420Only             = 1;
    m_caps.SliceStructure           = 4;
    m_caps.SliceIPOnly              = 0;
    m_caps.NoWeightedPred           = 1;
    m_caps.NoMinorMVs               = 1;
    m_caps.RawReconRefToggle        = 1;
    m_caps.NoInterlacedField        = 1;
    m_caps.BRCReset                 = 1;
    m_caps.RollingIntraRefresh      = 0;
    m_caps.UserMaxFrameSizeSupport  = 0;
    m_caps.FrameLevelRateCtrl       = 1;
    m_caps.SliceByteSizeCtrl        = 0;
    m_caps.VCMBitRateControl        = 1;
    m_caps.ParallelBRC              = 1;
    m_caps.TileSupport              = 0;
    m_caps.MaxPicWidth              = 8192;
    m_caps.MaxPicHeight             = 4096;
    m_caps.MaxNum_Reference0        = 3;
    m_caps.MaxNum_Reference1        = 1;
    m_caps.MBBRCSupport             = 1;
    m_caps.TUSupport                = 73;
#else
    IDirect3DDeviceManager9 *device = 0;
    mfxStatus sts = MFX_ERR_NONE;

    sts = core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    if (sts == MFX_ERR_NOT_FOUND)
        sts = core->CreateAccelerationDevice(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&device);

    MFX_CHECK_STS(sts);
    MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
    sts = auxDevice->Initialize(device);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, m_caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    
    m_guid      = guid;
    m_width     = width;
    m_height    = height;
    m_auxDevice = auxDevice;
#endif
    
    Trace(m_caps, 0);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    if (!m_caps.BitDepth8Only && !m_caps.MaxEncodedBitDepth)
        m_caps.MaxEncodedBitDepth = 1;
    if (!m_caps.Color420Only && !(m_caps.YUV444ReconSupport || m_caps.YUV422ReconSupport))
        m_caps.YUV444ReconSupport = 1;
#endif

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::CreateAccelerationService");
    const mfxExtPAVPOption& PAVP = par.m_ext.PAVP;

    m_widi = par.WiDi;

#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc = {};
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = {};
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = m_widi ? (ENCODE_ENC_PAK | ENCODE_WIDI) : ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    D3DAES_CTR_IV        initialCounter = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
    PAVP_ENCRYPTION_MODE encryptionMode = { PAVP.EncryptionType,   PAVP.CounterType         };

    if (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP)
    {
        encodeCreateDevice.EncryptionMode        = DXVA2_INTEL_PAVP;
        encodeCreateDevice.CounterAutoIncrement  = PAVP.CounterIncrement;
        encodeCreateDevice.pInitialCipherCounter = &initialCounter;
        encodeCreateDevice.pPavpEncryptionMode   = &encryptionMode;
    }

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "AUXDEV_CREATE_ACCEL_SERVICE");
        hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsQuery);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_CTRL_CAPS_ID");
        hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsGet);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_CTRL_GET_ID");
        hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    }
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#else
    PAVP;
#endif

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_maxSlices = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize) * CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Reset(MfxVideoParam const & par, bool bResetBRC)
{
    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_sps.bResetBRC = bResetBRC;

    return MFX_ERR_NONE;
}


mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount = 0;

        GUID guid = m_auxDevice->GetCurrentGuid();
        HRESULT hr = m_auxDevice->Execute(ENCODE_FORMAT_COUNT_ID, guid, encodeFormatCount);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats = &m_uncompBufInfo[0];

        hr = m_auxDevice->Execute(ENCODE_FORMATS_ID, (void *)0, encodeFormats);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
        if (m_compBufInfo[i].Type == type)
            break;

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;
#else
    type;
    request.Info.Width  = (mfxU16)m_width;
    request.Info.Height = (mfxU16)m_height;
    request.Info.FourCC = MFX_FOURCC_NV12;
#endif

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_HEVC & caps)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif

    caps = m_caps;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    EmulSurfaceRegister surfaceReg = {};
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i], MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);
#endif

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        mfxU32 feedbackSize = FeedbackSize(m_caps.SliceLevelReportSupport ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME, m_maxSlices);
        m_feedbackUpdate.Reset(response.NumFrameActual, feedbackSize);
        m_feedbackCached.Reset(response.NumFrameActual, feedbackSize);
    }

    return MFX_ERR_NONE;
}

#define ADD_CBD(id, buf, num)\
    assert(executeParams.NumCompBuffers < m_cbd.size());\
    m_cbd[executeParams.NumCompBuffers].CompressedBufferType = (id); \
    m_cbd[executeParams.NumCompBuffers].DataSize = (UINT)(sizeof(buf) * (num));\
    m_cbd[executeParams.NumCompBuffers].pCompBuffer = &buf; \
    executeParams.NumCompBuffers++;

mfxStatus D3D9Encoder::Execute(Task const & task, mfxHDL surface)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::Execute");
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif

    ENCODE_PACKEDHEADER_DATA * pPH = 0;
    ENCODE_EXECUTE_PARAMS executeParams = {};

    executeParams.pCompressedBuffers = &m_cbd[0];
    Zero(m_cbd);

    mfxU32 bitstream = task.m_idxBs;

    if (!m_sps.bResetBRC)
        m_sps.bResetBRC = task.m_resetBRC;

    FillPpsBuffer(task, m_pps);
    FillSliceBuffer(task, m_sps, m_pps, m_slice);
    m_pps.NumSlices = (USHORT)(m_slice.size());

    ADD_CBD(D3DDDIFMT_INTELENCODE_SPSDATA,          m_sps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_PPSDATA,          m_pps,      1);
    ADD_CBD(D3DDDIFMT_INTELENCODE_SLICEDATA,        m_slice[0], m_slice.size());
    ADD_CBD(D3DDDIFMT_INTELENCODE_BITSTREAMDATA,    bitstream,  1);

#if MFX_EXTBUFF_CU_QP_ENABLE
    if (m_pps.cu_qp_delta_enabled_flag && m_sps.RateControlMethod == MFX_RATECONTROL_CQP )
    {
        mfxU32 idxCUQp  = task.m_idxCUQp;
        ADD_CBD(D3DDDIFMT_INTELENCODE_MBQPDATA, idxCUQp,  1);
    }
#endif

    if (task.m_insertHeaders & INSERT_AUD)
    {
        pPH = PackHeader(task, AUD_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_VPS)
    {
        pPH = PackHeader(task, VPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_SPS)
    {
        pPH = PackHeader(task, SPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_PPS)
    {
        pPH = PackHeader(task, PPS_NUT); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
    }

    if ((task.m_insertHeaders & INSERT_SEI) || (task.m_ctrl.NumPayload > 0))
    {
        pPH = PackHeader(task, PREFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
        }
    }

    for (mfxU32 i = 0; i < m_slice.size(); i ++)
    {
        pPH = PackSliceHeader(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
        ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA, *pPH, 1);
    }
    
    /*if (task.m_ctrl.NumPayload > 0)
    {
        pPH = PackHeader(task, SUFFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA, *pPH, 1);
        }
    }*/

    Trace(executeParams, 0);

    try
    {
#ifdef HEADER_PACKING_TEST
        surface;
        ENCODE_QUERY_STATUS_PARAMS fb = {task.m_statusReportNumber,};
        FrameLocker bs(m_core, task.m_midBs);

        for (mfxU32 i = 0; i < executeParams.NumCompBuffers; i ++)
        {
            pPH = (ENCODE_PACKEDHEADER_DATA*)executeParams.pCompressedBuffers[i].pCompBuffer;

            if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
            {
                memcpy(bs.Y + fb.bitstreamSize, pPH->pData, pPH->DataLength);
                fb.bitstreamSize += pPH->DataLength;
            }
            else if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA)
            {
                mfxU32 sz = m_width * m_height * 3 / 2 - fb.bitstreamSize;
                HeaderPacker::PackRBSP(bs.Y + fb.bitstreamSize, pPH->pData, sz, CeilDiv(pPH->DataLength, 8));
                fb.bitstreamSize += sz;
            }
        }

        m_feedbackUpdate[0] = fb;
        m_feedbackCached.Update(m_feedbackUpdate);
#else
        HANDLE handle;
        HRESULT hr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "BeginFrame");
            hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ENCODE_ENC_PAK_ID");
            hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, executeParams, (void *)0);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "EndFrame");
            m_auxDevice->EndFrame(&handle);
        }
#endif
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    m_sps.bResetBRC = 0;


    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryStatus(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D9Encoder::QueryStatus");
#ifndef HEADER_PACKING_TEST
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);
#endif

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr = {};
        feedbackDescr.StatusParamType = (m_caps.SliceLevelReportSupport && m_pps.bEnableSliceLevelReport) ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME;

        for (;;)
        {
            HRESULT hr;
            feedbackDescr.SizeOfStatusParamStruct = FeedbackSize((ENCODE_QUERY_STATUS_PARAM_TYPE)feedbackDescr.StatusParamType, m_maxSlices);

            m_feedbackUpdate.Reset(m_feedbackUpdate.size(), feedbackDescr.SizeOfStatusParamStruct);

            try
            {
                hr = m_auxDevice->Execute(
                    ENCODE_QUERY_STATUS_ID,
                    (void *)&feedbackDescr,
                    sizeof(feedbackDescr),
                    &m_feedbackUpdate[0],
                    (mfxU32)m_feedbackUpdate.size() * m_feedbackUpdate.feedback_size());
            }
            catch (...)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            if (hr == E_INVALIDARG && feedbackDescr.StatusParamType == QUERY_STATUS_PARAM_SLICE)
            {
                feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
                continue;
            }

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            break;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        Trace(*feedback, 0);
        task.m_bsDataLength = feedback->bitstreamSize;

        if (m_widi && m_caps.HWCounterAutoIncrementSupport)
        {
            task.m_aes_counter.Count = feedback->aes_counter.Counter;
            task.m_aes_counter.IV    = feedback->aes_counter.IV;
        }

        {
            mfxExtEncodedSlicesInfo* pESI = ExtBuffer::Get(task.m_ctrl);

            if (pESI)
            {
                pESI->NumEncodedSlice     = feedback->NumberSlices;
                pESI->NumSliceNonCopliant = feedback->NumSliceNonCompliant;
                pESI->SliceSizeOverflow   = feedback->SliceSizeOverflow;

                if (pESI->NumSliceSizeAlloc && pESI->SliceSize && m_caps.SliceLevelReportSupport)
                {
                    mfxU16* pSize = (mfxU16*)((mfxU8*)feedback + sizeof(ENCODE_QUERY_STATUS_PARAMS) + sizeof(UINT) * 4);
                    CopyN(pESI->SliceSize, pSize, Min(feedback->NumberSlices, pESI->NumSliceSizeAlloc));
                }
            }
        }

        m_feedbackCached.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        Trace(*feedback, 0);
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D9Encoder::Destroy()
{
    m_auxDevice.reset(0);
    return MFX_ERR_NONE;
}

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)
