/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder DXVA2
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW)

//#include "libmfx_core_d3d9.h"
#include "mfx_h264_encode_d3d9.h"
#include "libmfx_core_interface.h"

// aya: cyclic ref
#include "mfx_h264_encode_hw_utils.h"


using namespace MfxHwH264Encode;

mfxU8 ConvertProfileMfx2Ddi(mfxU32 profile)
{
    return IsAvcHighProfile(profile) || IsMvcProfile(profile) || IsSvcProfile(profile)
        ? mfxU8(MFX_PROFILE_AVC_HIGH)
        : IsAvcBaseProfile(profile)
            ? mfxU8(MFX_PROFILE_AVC_BASELINE)
            : mfxU8(profile);
}

void MfxHwH264Encode::FillSpsBuffer(
    MfxVideoParam const &                 par,
    ENCODE_SET_SEQUENCE_PARAMETERS_H264 & sps)
{
    mfxExtSpsHeader const *       extSps  = GetExtBuffer(par);
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(par);
    mfxExtCodingOptionDDI const * extDdi  = GetExtBuffer(par);

    Zero(sps);

    sps.FrameWidth                              = par.mfx.FrameInfo.Width;
    sps.FrameHeight                             = par.mfx.FrameInfo.Height;
    sps.Profile                                 = ConvertProfileMfx2Ddi(par.mfx.CodecProfile);
    sps.Level                                   = IsMvcProfile(par.mfx.CodecProfile) ? mfxU8(par.calcParam.mvcPerViewPar.codecLevel) : mfxU8(par.mfx.CodecLevel);
    sps.GopPicSize                              = par.mfx.GopPicSize;
    sps.GopRefDist                              = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag                              = mfxU8(par.mfx.GopOptFlag);
    sps.TargetUsage                             = mfxU8(par.mfx.TargetUsage);
    sps.RateControlMethod                       = mfxU8(par.mfx.RateControlMethod);
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        sps.ICQQualityFactor = par.mfx.ICQQuality;
    sps.TargetBitRate                           = mfxU32(par.calcParam.targetKbps);
    sps.MaxBitRate                              = mfxU32(par.calcParam.maxKbps);
    sps.MinBitRate                              = mfxU32(par.calcParam.targetKbps);
    sps.FramesPer100Sec                         = mfxU16(mfxU64(100) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    sps.InitVBVBufferFullnessInBit              = par.calcParam.initialDelayInKB * 8000;
    sps.VBVBufferSizeInBit                      = par.calcParam.bufferSizeInKB * 8000;
    sps.NumRefFrames                            = mfxU8((extSps->maxNumRefFrames + 1) / 2);//reverted to make IVB encoded stream decodable, need to make decision based on HW type.
    sps.seq_parameter_set_id                    = extSps->seqParameterSetId;
    sps.chroma_format_idc                       = extSps->chromaFormatIdc;
    sps.bit_depth_luma_minus8                   = extSps->bitDepthLumaMinus8;
    sps.bit_depth_chroma_minus8                 = extSps->bitDepthChromaMinus8;
    sps.log2_max_frame_num_minus4               = extSps->log2MaxFrameNumMinus4;
    sps.pic_order_cnt_type                      = extSps->picOrderCntType;
    sps.log2_max_pic_order_cnt_lsb_minus4       = extSps->log2MaxPicOrderCntLsbMinus4;
    sps.num_ref_frames_in_pic_order_cnt_cycle   = extSps->numRefFramesInPicOrderCntCycle;
    sps.offset_for_non_ref_pic                  = extSps->offsetForNonRefPic;
    sps.offset_for_top_to_bottom_field          = extSps->offsetForTopToBottomField;

    Copy(sps.offset_for_ref_frame,                extSps->offsetForRefFrame);

    sps.frame_crop_left_offset                  = mfxU16(extSps->frameCropLeftOffset);
    sps.frame_crop_right_offset                 = mfxU16(extSps->frameCropRightOffset);
    sps.frame_crop_top_offset                   = mfxU16(extSps->frameCropTopOffset);
    sps.frame_crop_bottom_offset                = mfxU16(extSps->frameCropBottomOffset);
    sps.seq_scaling_matrix_present_flag         = extSps->seqScalingMatrixPresentFlag;
    sps.seq_scaling_list_present_flag           = extSps->seqScalingMatrixPresentFlag;
    sps.delta_pic_order_always_zero_flag        = extSps->deltaPicOrderAlwaysZeroFlag;
    sps.frame_mbs_only_flag                     = extSps->frameMbsOnlyFlag;
    sps.direct_8x8_inference_flag               = extSps->direct8x8InferenceFlag;
    sps.vui_parameters_present_flag             = extSps->vuiParametersPresentFlag;
    sps.frame_cropping_flag                     = extSps->frameCroppingFlag;
    sps.bResetBRC                               = 0;
    sps.GlobalSearch                            = extDdi->GlobalSearch;
    sps.LocalSearch                             = extDdi->LocalSearch;
    sps.EarlySkip                               = extDdi->EarlySkip;
    sps.Trellis                                 = extOpt2->Trellis;
    sps.MBBRC                                   = IsOn(extOpt2->MBBRC) ? 1 : IsOff(extOpt2->MBBRC) ? 2 : 0;

    sps.UserMaxFrameSize                        = extOpt2->MaxFrameSize;
    sps.AVBRAccuracy                            = par.mfx.Accuracy;
    sps.AVBRConvergence                         = par.mfx.Convergence * 100;

    // currenly hw knows nothing about MVC
    // so we need to correct brc parameters
    if (IsMvcProfile(par.mfx.CodecProfile))
    {
        sps.TargetBitRate              = par.calcParam.mvcPerViewPar.targetKbps;
        sps.MaxBitRate                 = par.calcParam.mvcPerViewPar.maxKbps;
        sps.MinBitRate                 = par.calcParam.mvcPerViewPar.targetKbps;
        sps.InitVBVBufferFullnessInBit = par.calcParam.mvcPerViewPar.initialDelayInKB * 8000;
        sps.VBVBufferSizeInBit         = par.calcParam.mvcPerViewPar.bufferSizeInKB   * 8000;;
    }
}


void MfxHwH264Encode::FillVuiBuffer(
    MfxVideoParam const &      par,
    ENCODE_SET_VUI_PARAMETER & vui)
{
    mfxExtSpsHeader const *  extSps = GetExtBuffer(par);
    //mfxExtMVCSeqDesc const * extMvc = GetExtBuffer(par);

    vui.AspectRatioInfoPresentFlag          = extSps->vui.flags.aspectRatioInfoPresent;
    vui.OverscanInfoPresentFlag             = extSps->vui.flags.overscanInfoPresent;
    vui.OverscanAppropriateFlag             = extSps->vui.flags.overscanAppropriate;
    vui.VideoSignalTypePresentFlag          = extSps->vui.flags.videoSignalTypePresent;
    vui.VideoFullRangeFlag                  = extSps->vui.flags.videoFullRange;
    vui.ColourDescriptionPresentFlag        = extSps->vui.flags.colourDescriptionPresent;
    vui.ChromaLocInfoPresentFlag            = extSps->vui.flags.chromaLocInfoPresent;
    vui.TimingInfoPresentFlag               = extSps->vui.flags.timingInfoPresent;
    vui.FixedFrameRateFlag                  = extSps->vui.flags.fixedFrameRate;
    vui.NalHrdParametersPresentFlag         = extSps->vui.flags.nalHrdParametersPresent;
    vui.VclHrdParametersPresentFlag         = extSps->vui.flags.vclHrdParametersPresent;
    vui.LowDelayHrdFlag                     = extSps->vui.flags.lowDelayHrd;
    vui.PicStructPresentFlag                = extSps->vui.flags.picStructPresent;
    vui.BitstreamRestrictionFlag            = extSps->vui.flags.bitstreamRestriction;
    vui.MotionVectorsOverPicBoundariesFlag  = extSps->vui.flags.motionVectorsOverPicBoundaries;
    vui.SarWidth                            = extSps->vui.sarWidth;
    vui.SarHeight                           = extSps->vui.sarHeight;
    vui.AspectRatioIdc                      = extSps->vui.aspectRatioIdc;
    vui.VideoFormat                         = extSps->vui.videoFormat;
    vui.ColourPrimaries                     = extSps->vui.colourPrimaries;
    vui.TransferCharacteristics             = extSps->vui.transferCharacteristics;
    vui.MatrixCoefficients                  = extSps->vui.matrixCoefficients;
    vui.ChromaSampleLocTypeTopField         = extSps->vui.chromaSampleLocTypeTopField;
    vui.ChromaSampleLocTypeBottomField      = extSps->vui.chromaSampleLocTypeBottomField;
    vui.MaxBytesPerPicDenom                 = extSps->vui.maxBytesPerPicDenom;
    vui.MaxBitsPerMbDenom                   = extSps->vui.maxBitsPerMbDenom;
    vui.Log2MaxMvLength_horizontal          = extSps->vui.log2MaxMvLengthHorizontal;
    vui.Log2MaxMvLength_vertical            = extSps->vui.log2MaxMvLengthVertical;
    vui.NumReorderFrames                    = extSps->vui.numReorderFrames;
    vui.NumUnitsInTick                      = extSps->vui.numUnitsInTick;
    vui.TimeScale                           = extSps->vui.timeScale;
    vui.MaxDecFrameBuffering                = extSps->vui.maxDecFrameBuffering;
    vui.CpbCnt_minus1                       = extSps->vui.nalHrdParameters.cpbCntMinus1;
    vui.BitRate_scale                       = extSps->vui.nalHrdParameters.bitRateScale;
    vui.CpbSize_scale                       = extSps->vui.nalHrdParameters.cpbSizeScale;

    Copy(vui.BitRateValueMinus1,              extSps->vui.nalHrdParameters.bitRateValueMinus1);
    Copy(vui.CpbSizeValueMinus1,              extSps->vui.nalHrdParameters.cpbSizeValueMinus1);

    for (mfxU32 i = 0; i < 32; i++)
        vui.CbrFlag                        |= extSps->vui.nalHrdParameters.cbrFlag[i] << i;

    vui.InitialCpbRemovalDelayLengthMinus1  = extSps->vui.nalHrdParameters.initialCpbRemovalDelayLengthMinus1;
    vui.CpbRemovalDelayLengthMinus1         = extSps->vui.nalHrdParameters.cpbRemovalDelayLengthMinus1;
    vui.DpbOutputDelayLengthMinus1          = extSps->vui.nalHrdParameters.dpbOutputDelayLengthMinus1;
    vui.TimeOffsetLength                    = extSps->vui.nalHrdParameters.timeOffsetLength;

    // currenly hw knows nothing about MVC
    // so we need to correct brc parameters
    // Update: we can't just divide by 2 to after calculation OverallBitrateMinus1 value. Need subtract 1 from both bitrates for view0, view1
    /* if (IsMvcProfile(par.mfx.CodecProfile))
    {
        vui.BitRateValueMinus1[0] /= extMvc->NumView;
        vui.CpbSizeValueMinus1[0] /= extMvc->NumView;
    }*/
}


void MfxHwH264Encode::FillConstPartOfPpsBuffer(
    MfxVideoParam const &                par,
    ENCODE_CAPS const &                  hwCaps,
    ENCODE_SET_PICTURE_PARAMETERS_H264 & pps)
{
    hwCaps;

    mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
    mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(par);

    pps.pic_parameter_set_id                    = extPps->picParameterSetId;
    pps.seq_parameter_set_id                    = extSps->seqParameterSetId;
    pps.FieldFrameCodingFlag                    = 0;
    pps.InterleavedFieldBFF                     = 0;
    pps.ProgressiveField                        = 0;
    pps.NumSlice                                = mfxU8(par.mfx.NumSlice);
    pps.bUseRawPicForRef                        = IsOn(extDdi->RefRaw);
    pps.bDisableHeaderPacking                   = 0;
    pps.bEnableDVMEChromaIntraPrediction        = 1;
    pps.bEnableDVMEReferenceLocationDerivation  = 1;
    pps.bEnableDVMWSkipCentersDerivation        = 1;
    pps.bEnableDVMEStartCentersDerivation       = 1;
    pps.bEnableDVMECostCentersDerivation        = 1;
    pps.QpY                                     = extPps->picInitQpMinus26 + 26;
    pps.pic_parameter_set_id                    = extPps->picParameterSetId;
    pps.seq_parameter_set_id                    = extPps->seqParameterSetId;
    pps.num_ref_idx_l0_active_minus1            = extPps->numRefIdxL0DefaultActiveMinus1;
    pps.num_ref_idx_l1_active_minus1            = extPps->numRefIdxL1DefaultActiveMinus1;
    pps.chroma_qp_index_offset                  = extPps->chromaQpIndexOffset;
    pps.second_chroma_qp_index_offset           = extPps->secondChromaQpIndexOffset;
    pps.entropy_coding_mode_flag                = extPps->entropyCodingModeFlag;
    pps.pic_order_present_flag                  = extPps->bottomFieldPicOrderInframePresentFlag;
    pps.weighted_pred_flag                      = extPps->weightedPredFlag;
    pps.weighted_bipred_idc                     = extPps->weightedBipredIdc;
    pps.constrained_intra_pred_flag             = extPps->constrainedIntraPredFlag;
    pps.transform_8x8_mode_flag                 = extPps->transform8x8ModeFlag;
    pps.pic_scaling_matrix_present_flag         = extPps->picScalingMatrixPresentFlag;
    pps.pic_scaling_list_present_flag           = extPps->picScalingMatrixPresentFlag;
    pps.BRCPrecision                            = extDdi->BRCPrecision;
}


void MfxHwH264Encode::FillVaringPartOfPpsBuffer(
    DdiTask const &                      task,
    mfxU32                               fieldId,
    ENCODE_SET_PICTURE_PARAMETERS_H264 & pps)
{
    pps.CurrOriginalPic.Index7Bits              = mfxU8(task.m_idxRecon);
    pps.CurrOriginalPic.AssociatedFlag          = mfxU8(fieldId);
    pps.CurrReconstructedPic.Index7Bits         = mfxU8(task.m_idxRecon);
    pps.CurrReconstructedPic.AssociatedFlag     = mfxU8(fieldId);
    pps.CodingType                              = ConvertFrameTypeMfx2Ddi(task.m_type[fieldId]);
    pps.FieldCodingFlag                         = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;
    pps.UsedForReferenceFlags                   = 0;
    pps.frame_num                               = mfxU16(task.m_frameNum);
    pps.bLastPicInSeq                           = 0;
    pps.bLastPicInStream                        = 0;
    pps.StatusReportFeedbackNumber              = task.m_statusReportNumber[fieldId];
    pps.bIdrPic                                 = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    pps.RefPicFlag                              = (task.m_type[fieldId] & MFX_FRAMETYPE_REF ? 1 : 0);
    pps.bDisableSubMBPartition                  = !task.m_subMbPartitionAllowed[fieldId];
    pps.CurrFieldOrderCnt[0]                    = task.GetPoc(TFIELD);
    pps.CurrFieldOrderCnt[1]                    = task.GetPoc(BFIELD);
    //pps.bDisableHeaderPacking                   = task.m_insertSps[fieldId] ? 0 : 1;
//  Intra refresh {
    pps.bEnableRollingIntraRefresh              = task.m_IRState.refrType;
    pps.IntraInsertionLocation                  = task.m_IRState.IntraLocation;
    pps.IntraInsertionSize                      = task.m_IRState.IntraSize;
    pps.QpDeltaForInsertedIntra                 = mfxU8(task.m_IRState.IntRefQPDelta);
//  Intra refresh }

    mfxU32 i = 0;
    for (; i < task.m_dpb[fieldId].Size(); i++)
    {
        pps.RefFrameList[i].Index7Bits     = task.m_dpb[fieldId][i].m_frameIdx;
        pps.RefFrameList[i].AssociatedFlag = task.m_dpb[fieldId][i].m_longterm;
        pps.FieldOrderCntList[i][0]        = task.m_dpb[fieldId][i].m_poc[0];
        pps.FieldOrderCntList[i][1]        = task.m_dpb[fieldId][i].m_poc[1];
    }
    for (; i < 16; i++)
    {
        pps.RefFrameList[i].bPicEntry = 0xff;
        pps.FieldOrderCntList[i][0]   = 0;
        pps.FieldOrderCntList[i][1]   = 0;
    }

    // ROI
    pps.NumROI = (mfxU8)task.m_numRoi;
    if (task.m_numRoi)
    {
        for (i = 0; i < task.m_numRoi; i ++)
        {
            pps.ROI[i].Left = (mfxU16)((task.m_roi[i].Left)>>4);
            pps.ROI[i].Top = (mfxU16)((task.m_roi[i].Top)>>4);
            pps.ROI[i].Right = (mfxU16)((task.m_roi[i].Right)>>4);
            pps.ROI[i].Bottom = (mfxU16)((task.m_roi[i].Bottom)>>4);
            pps.ROI[i].PriorityLevelOrDQp = (mfxU8)task.m_roi[i].Priority;
        }
        pps.MaxDeltaQp = 51;
        pps.MinDeltaQp = -51;
    }
}


void MfxHwH264Encode::FillConstPartOfSliceBuffer(
    MfxVideoParam const &                       par,
    std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice)
{
    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(par);
    mfxExtCodingOption2&    extOpt2 = GetExtBufferRef(par);

    for (size_t i = 0; i < slice.size(); ++i)
    {
        slice[i].slice_id                           = mfxU16(i);
        slice[i].direct_spatial_mv_pred_flag        = IsOn(extDdi->DirectSpatialMvPredFlag);
        slice[i].long_term_reference_flag           = 0;
        slice[i].delta_pic_order_cnt_bottom         = 0;
        slice[i].delta_pic_order_cnt[0]             = 0;
        slice[i].delta_pic_order_cnt[1]             = 0;
        slice[i].luma_log2_weight_denom             = 0;
        slice[i].chroma_log2_weight_denom           = 0;
        slice[i].disable_deblocking_filter_idc      = (mfxU8)extOpt2.DisableDeblockingIdc;
        slice[i].slice_alpha_c0_offset_div2         = 0;
        slice[i].slice_beta_offset_div2             = 0;
        slice[i].cabac_init_idc                     = (mfxU8)extDdi->CabacInitIdcPlus1 - 1;
    }
}

void MfxHwH264Encode::FillVaringPartOfSliceBuffer(
    ENCODE_CAPS const &                         hwCaps,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    ENCODE_SET_SEQUENCE_PARAMETERS_H264 const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_H264 const &  pps,
    std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice)
{
    mfxU32 numPics = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    SliceDivider divider = MakeSliceDivider(
        hwCaps.SliceStructure,
        task.m_numMbPerSlice,
        pps.NumSlice,
        sps.FrameWidth  / 16,
        sps.FrameHeight / 16 / numPics);

    if (divider.GetNumSlice() != slice.size())
    {
        size_t old_size = slice.size();
        slice.resize(divider.GetNumSlice());
        for (size_t i = old_size; i < slice.size(); ++i)
        {
            slice[i] = slice[0];
            slice[i].slice_id = (mfxU32)i;
        } 
    }

    for (size_t i = 0; i < slice.size(); ++i, divider.Next())
    { 
        slice[i].NumMbsForSlice                     = divider.GetNumMbInSlice();
        slice[i].first_mb_in_slice                  = divider.GetFirstMbInSlice();

        mfxU32 ref = 0;
        for (; ref < task.m_list0[fieldId].Size(); ref++)
            slice[i].RefPicList[0][ref].bPicEntry = task.m_list0[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[0][ref].bPicEntry = 0xff;

        ref = 0;
        for (ref = 0; ref < task.m_list1[fieldId].Size(); ref++)
            slice[i].RefPicList[1][ref].bPicEntry = task.m_list1[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[1][ref].bPicEntry = 0xff;

        slice[i].slice_type                         = ConvertMfxFrameType2SliceType(task.m_type[fieldId]);
        slice[i].pic_parameter_set_id               = pps.pic_parameter_set_id;
        slice[i].num_ref_idx_l0_active_minus1       = mfxU8(max(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1       = mfxU8(max(1, task.m_list1[fieldId].Size()) - 1);
        slice[i].num_ref_idx_active_override_flag   =
            slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
            slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id                         = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb                  = mfxU16(task.GetPoc(fieldId));
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.QpY);
    }
}

mfxStatus MfxHwH264Encode::FillVaringPartOfSliceBufferSizeLimited(
    ENCODE_CAPS const &                         /* hwCaps */,
    DdiTask const &                             task,
    mfxU32                                      fieldId,
    ENCODE_SET_SEQUENCE_PARAMETERS_H264 const & /* sps */,
    ENCODE_SET_PICTURE_PARAMETERS_H264 const &  pps,
    std::vector<ENCODE_SET_SLICE_HEADER_H264> & slice)
{

    size_t numSlices = task.m_SliceInfo.size();   
    if (numSlices != slice.size())
    {
        size_t old_size = slice.size();
        slice.resize(numSlices);
        for (size_t i = old_size; i < slice.size(); ++i)
        {
            slice[i] = slice[0];
            slice[i].slice_id = (mfxU32)i;
        } 
    }
    
    for (size_t i = 0; i < slice.size(); ++i)
    { 
        slice[i].first_mb_in_slice  = task.m_SliceInfo[i].startMB;        
        slice[i].NumMbsForSlice  = task.m_SliceInfo[i].numMB;

        mfxU32 ref = 0;
        for (; ref < task.m_list0[fieldId].Size(); ref++)
            slice[i].RefPicList[0][ref].bPicEntry = task.m_list0[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[0][ref].bPicEntry = 0xff;

        ref = 0;
        for (ref = 0; ref < task.m_list1[fieldId].Size(); ref++)
            slice[i].RefPicList[1][ref].bPicEntry = task.m_list1[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[1][ref].bPicEntry = 0xff;

        slice[i].slice_type                         = ConvertMfxFrameType2SliceType(task.m_type[fieldId]);
        slice[i].pic_parameter_set_id               = pps.pic_parameter_set_id;
        slice[i].num_ref_idx_l0_active_minus1       = mfxU8(max(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1       = mfxU8(max(1, task.m_list1[fieldId].Size()) - 1);
        slice[i].num_ref_idx_active_override_flag   =
            slice[i].num_ref_idx_l0_active_minus1 != pps.num_ref_idx_l0_active_minus1 ||
            slice[i].num_ref_idx_l1_active_minus1 != pps.num_ref_idx_l1_active_minus1;

        slice[i].idr_pic_id                         = task.m_idrPicId;
        slice[i].pic_order_cnt_lsb                  = mfxU16(task.GetPoc(fieldId));
        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.QpY);
    }

    return MFX_ERR_NONE;
}
namespace
{
    mfxU32 FindLayerBrcParam(mfxExtSVCRateControl const & extRc, mfxU32 did, mfxU32 qid, mfxU32 tid)
    {
        for (mfxU32 i = 0; i < 1024; i++)
            if (extRc.Layer[i].DependencyId == did &&
                extRc.Layer[i].QualityId    == qid &&
                extRc.Layer[i].TemporalId   == tid)
                return i;
        return 0xffffffff;
    };

    mfxU8 * PackSliceNalUnitPrefix(
        mfxU8 *         begin,
        mfxU8 *         end,
        DdiTask const & task,
        mfxU32          fieldId)
    {
        mfxU32 nalUnitType = (task.m_did == 0 && task.m_qid == 0) ? 14 : 20;

        begin = PackPrefixNalUnitSvc(begin, end, true, task, fieldId, nalUnitType);

        if (nalUnitType == 14)
        {
            mfxU8 nalRefIdc   = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;
            mfxU8 nalUnitType = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 5 : 1;
            *begin++ = 0;
            *begin++ = 0;
            *begin++ = 1;
            *begin++ = (nalRefIdc << 5) + nalUnitType;
        }

        return begin;
    }
}

namespace
{
    mfxU32 PrevDepId(mfxExtSVCSeqDesc const & extSvc, mfxU32 did)
    {
        for (mfxU32 d = did; d > 0; d--)
            if (extSvc.DependencyLayer[d - 1].Active)
                return d - 1;
        return did;
    }

    bool LastLayer(MfxVideoParam const & par, mfxU32 did, mfxU32 qid)
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        return
            did     == par.calcParam.did[par.calcParam.numDependencyLayer - 1] &&
            qid + 1 == extSvc->DependencyLayer[did].QualityNum;
    }
};


void MfxHwH264Encode::FillSpsBuffer(
    MfxVideoParam const &                par,
    ENCODE_SET_SEQUENCE_PARAMETERS_SVC & sps,
    mfxU32                               did,
    mfxU32                               qid,
    mfxU32                               tid)
{
    mfxExtSpsHeader const *  extSps = GetExtBuffer(par);
    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
    //mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(par);
    mfxExtSVCRateControl const *  extRc  = GetExtBuffer(par);

    mfxU32 rcIdx = FindLayerBrcParam(*extRc, did, qid, tid);

    Zero(sps);

    sps.FrameWidth           = extSvc->DependencyLayer[did].Width;
    sps.FrameHeight          = extSvc->DependencyLayer[did].Height;
    sps.Profile              = (qid == 0 && qid == 0) ? 100 : mfxU8(par.mfx.CodecProfile);
    sps.Level                = mfxU8(par.mfx.CodecLevel);
    sps.GopPicSize           = par.mfx.GopPicSize;
    sps.GopRefDist           = mfxU8(par.mfx.GopRefDist);
    sps.GopOptFlag           = mfxU8(par.mfx.GopOptFlag);
    sps.TargetUsage          = mfxU8(par.mfx.TargetUsage);
    sps.NumRefFrames         = mfxU8(extSps->maxNumRefFrames);

    //mfxU16 cropUnitX = CROP_UNIT_X[par.mfx.FrameInfo.ChromaFormat];
    //mfxU16 cropUnitY = CROP_UNIT_Y[par.mfx.FrameInfo.ChromaFormat] * (2 - extSps->frameMbsOnlyFlag);

    //mfxU16 cropX = extSvc->DependencyLayer[did].CropX;
    //mfxU16 cropY = extSvc->DependencyLayer[did].CropY;
    //mfxU16 cropW = extSvc->DependencyLayer[did].CropW;
    //mfxU16 cropH = extSvc->DependencyLayer[did].CropH;

    //sps.frame_crop_left_offset   = (cropX / cropUnitX);
    //sps.frame_crop_right_offset  = (sps.FrameWidth - cropW - cropX) / cropUnitX;
    //sps.frame_crop_top_offset    = (cropY / cropUnitY);
    //sps.frame_crop_bottom_offset = (sps.FrameHeight - cropH - cropY) / cropUnitY;
    if (extRc->RateControlMethod != MFX_RATECONTROL_CQP)
    {
        sps.RateControlMethod                 = mfxU8(extRc->RateControlMethod);
        sps.TargetBitRate                     = mfxU32(extRc->Layer[rcIdx].CbrVbr.TargetKbps);
        sps.MaxBitRate                        = mfxU32(extRc->Layer[rcIdx].CbrVbr.MaxKbps);
        sps.MinBitRate                        = mfxU32(extRc->Layer[rcIdx].CbrVbr.TargetKbps);
        sps.InitVBVBufferFullnessInBit        = mfxU32(extRc->Layer[rcIdx].CbrVbr.InitialDelayInKB) * 8000;
    }
    sps.VBVBufferSizeInBit              = mfxU32(extRc->Layer[rcIdx].CbrVbr.BufferSizeInKB) * 8000;
    sps.bResetBRC                         = 0;
    //sps.UserMaxFrameSize                = 0;
    sps.AVBRAccuracy                      = par.mfx.Accuracy;
    sps.AVBRConvergence                   = par.mfx.Convergence * 100;
    //sps.bUseRawPicForRef                  = IsOn(extDdi->RefRaw);
    sps.fixed_frame_rate_flag             = extSps->vui.flags.fixedFrameRate;
    sps.nalHrdConformanceRequired         = extSps->vui.flags.nalHrdParametersPresent;
    sps.vclHrdConformanceRequired         = extSps->vui.flags.vclHrdParametersPresent;
    sps.low_delay_hrd_flag                = extSps->vui.flags.lowDelayHrd;
    sps.FrameRateNumerator                = mfxU16(extSps->vui.timeScale);
    sps.FrameRateDenominator              = mfxU16(extSps->vui.numUnitsInTick);
    sps.bit_rate_scale                    = extSps->vui.nalHrdParameters.bitRateScale;
    sps.cpb_size_scale                    = extSps->vui.nalHrdParameters.cpbSizeScale;
    sps.bit_rate_value_minus1             = extSps->vui.nalHrdParameters.bitRateValueMinus1[0];
    sps.cpb_size_value_minus1             = extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0];

    sps.chroma_format_idc       = extSps->chromaFormatIdc;
    sps.bit_depth_luma_minus8   = extSps->bitDepthLumaMinus8;
    sps.bit_depth_chroma_minus8 = extSps->bitDepthChromaMinus8;

    sps.separate_colour_plane_flag           = extSps->separateColourPlaneFlag;
    sps.qpprime_y_zero_transform_bypass_flag = extSps->qpprimeYZeroTransformBypassFlag;
    sps.seq_scaling_matrix_present_flag      = extSps->seqScalingMatrixPresentFlag;
    sps.seq_scaling_list_present_flag        = extSps->seqScalingMatrixPresentFlag;
    sps.frame_mbs_only_flag                  = extSps->frameMbsOnlyFlag;
    sps.direct_8x8_inference_flag            = extSps->direct8x8InferenceFlag;

    sps.extended_spatial_scalability_idc     = 1;
    sps.chroma_phase_x_plus1_flag            = 0;
    sps.chroma_phase_y_plus1                 = 0;

    sps.time_scale        = extSps->vui.timeScale;;
    sps.num_units_in_tick = extSps->vui.numUnitsInTick;
    sps.cpb_cnt_minus1    = extSps->vui.nalHrdParameters.cpbCntMinus1;
}



void MfxHwH264Encode::FillConstPartOfPpsBuffer(
    MfxVideoParam const &                par,
    ENCODE_SET_PICTURE_PARAMETERS_SVC &  pps,
    mfxU32                               did,
    mfxU32                               qid,
    mfxU32                               tid)
{
    //mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    mfxExtPpsHeader const *       extPps = GetExtBuffer(par);
    //mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(par);
    mfxExtSVCSeqDesc const *      extSvc = GetExtBuffer(par);
    //mfxExtSVCRateControl const *  extRc  = GetExtBuffer(par);

    //mfxU32 rcIdx = FindLayerBrcParam(*extRc, did, qid, tid);
    //assert(rcIdx != 0xffffffff);

    Zero(pps);

    pps.scaled_ref_layer_left_offset   = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[0];
    pps.scaled_ref_layer_top_offset    = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[1];
    pps.scaled_ref_layer_right_offset  = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[2];
    pps.scaled_ref_layer_bottom_offset = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[3];
    for (mfxU32 i = 0; i < 16; i++)
    {
        pps.refPicScaledRefLayerLeftOffset[i]   = pps.scaled_ref_layer_left_offset;
        pps.refPicScaledRefLayerTopOffset[i]    = pps.scaled_ref_layer_top_offset;
        pps.refPicScaledRefLayerRightOffset[i]  = pps.scaled_ref_layer_right_offset;
        pps.refPicScaledRefLayerBottomOffset[i] = pps.scaled_ref_layer_bottom_offset;
    }

    pps.dependency_id = did;
    pps.quality_id    = qid;
    pps.temporal_id   = tid;

    if (did > 0 || qid > 0)
    {
        pps.ref_layer_dependency_id = qid > 0 ? did     : PrevDepId(*extSvc, did);
        pps.ref_layer_quality_id    = qid > 0 ? qid - 1 : extSvc->DependencyLayer[PrevDepId(*extSvc, did)].QualityNum - 1;
    }

    pps.tcoeff_level_prediction_flag              = extSvc->DependencyLayer[did].QualityLayer[qid].TcoeffPredictionFlag;
    pps.disable_inter_layer_deblocking_filter_idc = did || qid ? 0 : 1;

    pps.next_layer_resolution_change_flag =
        (qid + 1 == extSvc->DependencyLayer[did].QualityNum) &&
        (did != par.calcParam.did[par.calcParam.numDependencyLayer - 1]);

    pps.inter_layer_slice_alpha_c0_offset_div2         = 0;
    pps.inter_layer_slice_beta_offset_div2             = 0;
    pps.ref_layer_chroma_phase_x_plus1_flag            = 0;
    pps.ref_layer_chroma_phase_y_plus1                 = 0;
    pps.constrained_intra_resampling_flag              = 0;

    pps.FieldFrameCodingFlag              = 0;
    pps.InterleavedFieldBFF               = 0;
    pps.ProgressiveField                  = 0;
    pps.NumSlice                          = mfxU8(par.mfx.NumSlice);
    //pps.RateControlMethod                 = mfxU8(extRc->RateControlMethod);
    //pps.TargetBitRate                     = mfxU16(extRc->Layer[rcIdx].CbrVbr.TargetKbps);
    //pps.MaxBitRate                        = mfxU16(extRc->Layer[rcIdx].CbrVbr.MaxKbps);
    //pps.MinBitRate                        = mfxU16(extRc->Layer[rcIdx].CbrVbr.TargetKbps);
    //pps.InitVBVBufferFullnessInBit        = mfxU16(extRc->Layer[rcIdx].CbrVbr.InitialDelayInKB) * 8000;
    //pps.VBVBufferSizeInBit              = mfxU16(extRc->Layer[rcIdx].CbrVbr.BufferSizeInKB) * 8000;
    //pps.bResetBRC                         = 0;
    //pps.UserMaxFrameSize                = 0;
    //pps.AVBRAccuracy                      = par.mfx.Accuracy;
    //pps.AVBRConvergence                   = par.mfx.Convergence * 100;
    //pps.bUseRawPicForRef                  = IsOn(extDdi->RefRaw);
    //pps.bDisableHeaderPacking           = 0;
    pps.QpY  = extPps->picInitQpMinus26 + 26;

    pps.chroma_qp_index_offset            = extPps->chromaQpIndexOffset;
    pps.second_chroma_qp_index_offset     = extPps->secondChromaQpIndexOffset;
    pps.entropy_coding_mode_flag          = extPps->entropyCodingModeFlag;
    pps.weighted_pred_flag                = extPps->weightedPredFlag;
    pps.weighted_bipred_idc               = extPps->weightedBipredIdc;
    pps.constrained_intra_pred_flag       = LastLayer(par, did, qid) ? extPps->constrainedIntraPredFlag : 1;
    pps.transform_8x8_mode_flag           = extPps->transform8x8ModeFlag;
    pps.pic_scaling_matrix_present_flag   = extPps->picScalingMatrixPresentFlag;
    pps.pic_scaling_list_present_flag     = extPps->picScalingMatrixPresentFlag;
    //pps.fixed_frame_rate_flag             = extSps->vui.flags.fixedFrameRate;
    //pps.nalHrdConformanceRequired         = extSps->vui.flags.nalHrdParametersPresent;
    //pps.vclHrdConformanceRequired         = extSps->vui.flags.vclHrdParametersPresent;
    //pps.low_delay_hrd_flag                = extSps->vui.flags.lowDelayHrd;
    //pps.FrameRateNumerator                = mfxU16(extSps->vui.timeScale);
    //pps.FrameRateDenominator              = mfxU16(extSps->vui.numUnitsInTick);
    //pps.bit_rate_scale                    = extSps->vui.nalHrdParameters.bitRateScale;
    //pps.cpb_size_scale                    = extSps->vui.nalHrdParameters.cpbSizeScale;
    //pps.bit_rate_value_minus1             = extSps->vui.nalHrdParameters.bitRateValueMinus1[0];
    //pps.cpb_size_value_minus1             = extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0];
    pps.bEmulationByteInsertion           = 1;
    pps.bLastLayerInPicture               = LastLayer(par, did, qid);
}

void MfxHwH264Encode::FillVaringPartOfPpsBuffer(
    DdiTask const &                     task,
    mfxU32                              fieldId,
    ENCODE_SET_PICTURE_PARAMETERS_SVC & pps)
{
    pps.CurrOriginalPic.Index7Bits              = mfxU8(task.m_idxRecon);
    pps.CurrOriginalPic.AssociatedFlag          = mfxU8(fieldId);
    pps.CurrReconstructedPic.Index7Bits         = mfxU8(task.m_idxRecon);
    pps.CurrReconstructedPic.AssociatedFlag     = mfxU8(fieldId);
    pps.CodingType                              = ConvertFrameTypeMfx2Ddi(task.m_type[fieldId]);
    pps.FieldCodingFlag                         = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;
    pps.UsedForReferenceFlags                   = 0;
    pps.bLastPicInSeq                           = 0;
    pps.bLastPicInStream                        = 0;
    pps.StatusReportFeedbackNumber              = task.m_statusReportNumber[fieldId];
    pps.RefPicFlag                              = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;
    pps.CurrFieldOrderCnt[0]                    = task.GetPoc(TFIELD);
    pps.CurrFieldOrderCnt[1]                    = task.GetPoc(BFIELD);
    pps.use_ref_base_pic_flag                   = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;
    pps.store_ref_base_pic_flag                 = task.m_storeRefBasePicFlag;
    pps.QpY                                     = task.m_cqpValue[fieldId];
    //pps.bDisableHeaderPacking                   = task.m_insertSps[fieldId] ? 0 : 1;

    mfxU32 i = 0;
    for (; i < task.m_dpb[fieldId].Size(); i++)
    {
        pps.RefFrameList[i].bPicEntry = mfxU8(task.m_dpb[fieldId][i].m_frameIdx);
        pps.FieldOrderCntList[i][0]   = task.m_dpb[fieldId][i].m_poc[0];
        pps.FieldOrderCntList[i][1]   = task.m_dpb[fieldId][i].m_poc[1];
    }
    for (; i < 16; i++)
    {
        pps.RefFrameList[i].bPicEntry = 0xff;
        pps.FieldOrderCntList[i][0]   = 0;
        pps.FieldOrderCntList[i][1]   = 0;
    }

    pps.RefLayer.bPicEntry = mfxU8(task.m_idxInterLayerRecon);
}

void MfxHwH264Encode::FillConstPartOfSliceBuffer(
    MfxVideoParam const &                      par,
    std::vector<ENCODE_SET_SLICE_HEADER_SVC> & slice)
{
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(par);

    for (size_t i = 0; i < slice.size(); ++i)
    {
        slice[i].slice_id                               = mfxU16(i);
        slice[i].direct_spatial_mv_pred_flag            = IsOn(extDdi->DirectSpatialMvPredFlag);
        slice[i].luma_log2_weight_denom                 = 0;
        slice[i].chroma_log2_weight_denom               = 0;
        slice[i].disable_deblocking_filter_idc          = 0;
        slice[i].slice_alpha_c0_offset_div2             = 0;
        slice[i].slice_beta_offset_div2                 = 0;
        slice[i].cabac_init_idc                         = GetCabacInitIdc(par.mfx.TargetUsage);
        slice[i].base_pred_weight_table_flag            = 0;
        slice[i].colour_plane_id                        = 0;
    }
}

void MfxHwH264Encode::FillVaringPartOfSliceBuffer(
    ENCODE_CAPS const &                        hwCaps,
    mfxExtSVCSeqDesc const &                   extSvc,
    DdiTask const &                            task,
    mfxU32                                     fieldId,
    ENCODE_SET_SEQUENCE_PARAMETERS_SVC const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_SVC const &  pps,
    std::vector<ENCODE_SET_SLICE_HEADER_SVC> & slice)
{
    mfxU32 numPics = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;

    mfxU32 did = pps.dependency_id;
    mfxU32 qid = pps.quality_id;
    mfxU16 simulcast =    IsOff(extSvc.DependencyLayer[did].BasemodePred) &&
                        IsOff(extSvc.DependencyLayer[did].MotionPred) &&
                        IsOff(extSvc.DependencyLayer[did].ResidualPred) ? 1 : 0;

    SliceDivider divider = MakeSliceDivider(
        hwCaps.SliceStructure,
        task.m_numMbPerSlice,
        pps.NumSlice,
        sps.FrameWidth  / 16,
        sps.FrameHeight / 16 / numPics);

    for (size_t i = 0; i < slice.size(); ++i, divider.Next())
    {
        slice[i].NumMbsForSlice                     = (USHORT)divider.GetNumMbInSlice();
        slice[i].first_mb_in_slice                  = (USHORT)divider.GetFirstMbInSlice();

        mfxU32 ref = 0;
        for (; ref < task.m_list0[fieldId].Size(); ref++)
            slice[i].RefPicList[0][ref].bPicEntry = task.m_list0[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[0][ref].bPicEntry = 0xff;

        ref = 0;
        for (ref = 0; ref < task.m_list1[fieldId].Size(); ref++)
            slice[i].RefPicList[1][ref].bPicEntry = task.m_list1[fieldId][ref];
        for (; ref < 32; ref++)
            slice[i].RefPicList[1][ref].bPicEntry = 0xff;

        slice[i].slice_type                         = ConvertMfxFrameType2SliceType(task.m_type[fieldId]);
        slice[i].num_ref_idx_l0_active_minus1       = mfxU8(max(1, task.m_list0[fieldId].Size()) - 1);
        slice[i].num_ref_idx_l1_active_minus1       = mfxU8(max(1, task.m_list1[fieldId].Size()) - 1);

        slice[i].slice_qp_delta                     = mfxI8(task.m_cqpValue[fieldId] - pps.QpY);
         
        

        slice[i].no_inter_layer_pred_flag           = (qid == 0) ? (did==0 ? 1 : simulcast ): 0;
        slice[i].scan_idx_start                     = extSvc.DependencyLayer[did].QualityLayer[qid].ScanIdxStart;
        slice[i].scan_idx_end                       = extSvc.DependencyLayer[did].QualityLayer[qid].ScanIdxEnd;
    }
}

mfxU32 MfxHwH264Encode::AddEmulationPreventionAndCopy(
    ENCODE_PACKEDHEADER_DATA const & data,
    mfxU8*                           bsDataStart,
    mfxU8*                           bsEnd,
    bool                             bEmulationByteInsertion)
{
    mfxU8 * sbegin = data.pData + data.DataOffset;
    mfxU8 * send   = sbegin + data.DataLength;
    mfxU32  len    = bEmulationByteInsertion ? data.SkipEmulationByteCount : data.DataLength;

    assert(UINT(bsEnd - bsDataStart) > data.DataLength);

    if (len)
    {
        MFX_INTERNAL_CPY(bsDataStart, sbegin, len);
        bsDataStart += len;
        sbegin      += len;
    }
                
    mfxU8 * bsDataEnd = AddEmulationPreventionAndCopy(sbegin, send, bsDataStart, bsEnd);

    len += mfxU32(bsDataEnd - bsDataStart);

    return len;
}

void CachedFeedback::Reset(mfxU32 cacheSize)
{
    Feedback init;
    Zero(init);
    init.bStatus = ENCODE_NOTAVAILABLE;

    m_cache.resize(cacheSize, init);
}

void CachedFeedback::Update(const FeedbackStorage& update)
{
    for (size_t i = 0; i < update.size(); i++)
    {
        if (update[i].bStatus != ENCODE_NOTAVAILABLE)
        {
            Feedback* cache = 0;

            for (size_t j = 0; j < m_cache.size(); j++)
            {
                if (m_cache[j].StatusReportFeedbackNumber == update[i].StatusReportFeedbackNumber)
                {
                    cache = &m_cache[j];
                    break;
                }
                else if (cache == 0 && m_cache[j].bStatus == ENCODE_NOTAVAILABLE)
                {
                    cache = &m_cache[j];
                }
            }

            if (cache == 0)
            {
                assert(!"no space in cache");
                throw std::logic_error(": no space in cache");
            }

            *cache = update[i];
        }
    }
}

const CachedFeedback::Feedback* CachedFeedback::Hit(mfxU32 feedbackNumber) const
{
    for (size_t i = 0; i < m_cache.size(); i++)
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
            return &m_cache[i];

    return 0;
}

void CachedFeedback::Remove(mfxU32 feedbackNumber)
{
    for (size_t i = 0; i < m_cache.size(); i++)
    {
        if (m_cache[i].StatusReportFeedbackNumber == feedbackNumber)
        {
            m_cache[i].bStatus = ENCODE_NOTAVAILABLE;
            return;
        }
    }

    assert(!"wrong feedbackNumber");
}

D3D9Encoder::D3D9Encoder()
: m_core(0)
, m_auxDevice(0)
, m_infoQueried(false)
, m_forcedCodingFunction(0)
, m_numSkipFrames(0)
, m_sizeSkipFrames(0)
, m_skipMode(0)
{
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;

    // Case of MVC dependent view doesn't require special processing for DX9. Just create AVC encoding device for dep. view.
    if (guid == MSDK_Private_Guid_Encode_MVC_Dependent_View
        || guid == MSDK_Private_Guid_Encode_AVC_Query)
        guid = DXVA2_Intel_Encode_AVC;
    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK_WITH_ASSERT(pID3D != 0, MFX_ERR_DEVICE_FAILED);

    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service, isTemporal);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDeviceHlp> auxDevice(new AuxiliaryDeviceHlp());
    sts = auxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    ENCODE_CAPS caps = { 0, };
    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_guid = guid;
    m_width = width;
    m_height = height;
    m_caps = caps;
    m_auxDevice = auxDevice;

    // Below is WA for limited SKUs (on which AVC encoding is supported for WiDi only)
    // On limited platforms driver reports that AVC encoder GUID is supported (IsAccelerationServiceExist() returns OK)
    // but after it driver fails to create AVC encoding acceleration service.
    // To check AVC-E support in Query MSDK should try to create AVC acceleration service here
    // WA start
    if (isTemporal && (m_width<=caps.MaxPicWidth && m_height<=caps.MaxPicHeight))
    {
        DXVADDI_VIDEODESC desc;
        Zero(desc);
        desc.SampleWidth = m_width;
        desc.SampleHeight = m_height;
        desc.Format = D3DDDIFMT_NV12;

        ENCODE_CREATEDEVICE encodeCreateDevice;
        Zero(encodeCreateDevice);
        encodeCreateDevice.pVideoDesc = &desc;
        encodeCreateDevice.CodingFunction = m_forcedCodingFunction ? m_forcedCodingFunction : ENCODE_ENC_PAK;
        encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

        hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }
    // WA end
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    mfxExtPAVPOption const * extPavp = GetExtBuffer(par);
    mfxExtCodingOption2 const * extCO2 = GetExtBuffer(par);

    if (extCO2)
        m_skipMode = extCO2->SkipFrame;

    DXVADDI_VIDEODESC desc;
    Zero(desc);
    desc.SampleWidth = m_width;
    desc.SampleHeight = m_height;
    desc.Format = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice;
    Zero(encodeCreateDevice);
    encodeCreateDevice.pVideoDesc = &desc;
    encodeCreateDevice.CodingFunction = m_forcedCodingFunction ? m_forcedCodingFunction : ENCODE_ENC_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    D3DAES_CTR_IV        initialCounter = { extPavp->CipherCounter.IV, extPavp->CipherCounter.Count };
    PAVP_ENCRYPTION_MODE encryptionMode = { extPavp->EncryptionType,   extPavp->CounterType         };

    if (IsProtectionPavp(par.Protected))
    {
        encodeCreateDevice.EncryptionMode        = DXVA2_INTEL_PAVP;
        encodeCreateDevice.CounterAutoIncrement  = extPavp->CounterIncrement;
        encodeCreateDevice.pInitialCipherCounter = &initialCounter;
        encodeCreateDevice.pPavpEncryptionMode   = &encryptionMode;
    }

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsQuery);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void *)0, m_capsQuery);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    Zero(m_capsGet);
    hr = m_auxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void *)0, m_capsGet);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_slice.resize(par.mfx.NumSlice);

    mfxU32 const MAX_NUM_PACKED_SPS = 9;
    mfxU32 const MAX_NUM_PACKED_PPS = 9;
    m_compBufDesc.resize(11 + MAX_NUM_PACKED_SPS + MAX_NUM_PACKED_PPS + par.mfx.NumSlice);

    Zero(m_sps);
    Zero(m_vui);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_sps);
    FillVuiBuffer(par, m_vui);
    FillConstPartOfPpsBuffer(par, m_caps, m_pps);
    FillConstPartOfSliceBuffer(par, m_slice);

    m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Reset(
    MfxVideoParam const & par)
{
    ENCODE_SET_SEQUENCE_PARAMETERS_H264 oldSps = m_sps;
    ENCODE_SET_VUI_PARAMETER            oldVui = m_vui;
    mfxExtCodingOption2 const * extCO2 = GetExtBuffer(par);

    if (extCO2)
        m_skipMode = extCO2->SkipFrame;


    Zero(m_sps);
    Zero(m_vui);
    Zero(m_pps);

    FillSpsBuffer(par, m_sps);
    FillVuiBuffer(par, m_vui);
    FillConstPartOfPpsBuffer(par, m_caps, m_pps);

    m_sps.bResetBRC = !Equal(m_sps, oldSps) || !Equal(m_vui, oldVui);

    m_slice.resize(par.mfx.NumSlice);

    m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
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
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    par;
    mbPerSec;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    EmulSurfaceRegister surfaceReg;
    Zero(surfaceReg);
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg); // &m_uncompBufInfo[0]
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}

namespace
{
    char FrameTypeToChar(mfxU32 type)
    {
        char c = (type & MFX_FRAMETYPE_I) ? 'i' : (type & MFX_FRAMETYPE_P) ? 'p' : 'b';
        return (type & MFX_FRAMETYPE_REF) ? char(toupper(c)) : c;
    }

    void DumpRefInfo(DdiTask const & task, mfxU32 fieldId)
    {
        char type = FrameTypeToChar(task.m_type[fieldId]);
        printf("\rFrame %3d, poc %3d, type %c, temporal_id %d\n", task.m_frameOrder, task.GetPoc(fieldId), type, task.m_tid);
        printf("DPB:\n");
        ArrayDpbFrame const & dpb = task.m_dpb[fieldId];
        for (mfxU32 i = 0; i < dpb.Size(); i++)
            printf("  poc %3d, longterm %d\n", dpb[i].m_poc[0], dpb[i].m_longterm);
        printf("LIST 0:\n");
        if ((task.m_type[fieldId] & MFX_FRAMETYPE_I) == 0)
        {
            ArrayU8x33 const & list0 = task.m_list0[fieldId];
            for (mfxU32 i = 0; i < list0.Size(); i++)
                printf("  poc %3d, longterm %d\n", dpb[list0[i] & 0x7f].m_poc[list0[i] >> 7], dpb[list0[i] & 0x7f].m_longterm);
        }
        printf("LIST 1:\n");
        if (task.m_type[fieldId] & MFX_FRAMETYPE_B)
        {
            ArrayU8x33 const & list1 = task.m_list1[fieldId];
            for (mfxU32 i = 0; i < list1.Size(); i++)
                printf("  poc %3d, longterm %d\n", dpb[list1[i] & 0x7f].m_poc[list1[i] >> 7], dpb[list1[i] & 0x7f].m_longterm);
        }
        printf("\n");
    }
};

mfxStatus D3D9Encoder::Execute(
    mfxHDL                     surface,
    DdiTask const &            task,
    mfxU32                     fieldId,
    PreAllocatedVector const & sei)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    ENCODE_PACKEDHEADER_DATA packedSei = { 0 };

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = Begin(m_compBufDesc);
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;
    UCHAR  SkipFlag = task.SkipFlag();

    // mvc hack
    // base view has separate sps/pps
    // all other views share another sps/pps
    mfxU8 initSpsId = m_sps.seq_parameter_set_id;
    mfxU8 initPpsId = m_pps.pic_parameter_set_id;
    //if (task.m_viewIdx != 0)
    {
        m_sps.seq_parameter_set_id = mfxU8((initSpsId + !!task.m_viewIdx) & 0x1f);
        m_pps.pic_parameter_set_id = mfxU8((initPpsId + !!task.m_viewIdx));
        m_pps.seq_parameter_set_id = m_sps.seq_parameter_set_id;
    }

    m_sps.bNoAccelerationSPSInsertion = !task.m_insertSps[fieldId];

    if (m_sps.UserMaxFrameSize != task.m_maxFrameSize)
    {
        m_sps.UserMaxFrameSize = (UINT)task.m_maxFrameSize;
        if (task.m_frameOrder)
            m_sps.bResetBRC = true;
    }

    {
        size_t slice_size_old = m_slice.size();
        FillVaringPartOfPpsBuffer(task, fieldId, m_pps);

        if (task.m_SliceInfo.size())
            FillVaringPartOfSliceBufferSizeLimited(m_caps, task, fieldId, m_sps, m_pps, m_slice);
        else
            FillVaringPartOfSliceBuffer(m_caps, task, fieldId, m_sps, m_pps,m_slice);

        if (slice_size_old != m_slice.size())
        {
            m_compBufDesc.resize(10 + m_slice.size());
            m_pps.NumSlice = mfxU8(m_slice.size());
            encodeExecuteParams.pCompressedBuffers = Begin(m_compBufDesc);
        }
    }

    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SPSDATA;
    m_compBufDesc[bufCnt].DataSize = mfxU32(sizeof(m_sps));
    m_compBufDesc[bufCnt].pCompBuffer = &m_sps;
    bufCnt++;

    if (m_sps.vui_parameters_present_flag)
    {
        m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_VUIDATA;
        m_compBufDesc[bufCnt].DataSize = mfxU32(sizeof(m_vui));
        m_compBufDesc[bufCnt].pCompBuffer = &m_vui;
        bufCnt++;
    }

    
    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PPSDATA;
    m_compBufDesc[bufCnt].DataSize = mfxU32(sizeof(m_pps));
    m_compBufDesc[bufCnt].pCompBuffer = &m_pps;
    bufCnt++;

   
    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SLICEDATA;
    m_compBufDesc[bufCnt].DataSize = mfxU32(sizeof(m_slice[0]) * m_slice.size());
    m_compBufDesc[bufCnt].pCompBuffer = &m_slice[0];
    bufCnt++;
    
    mfxU32 bitstream = task.m_idxBs[fieldId];
    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    m_compBufDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    m_compBufDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    if (m_caps.HeaderInsertion == 1 && SkipFlag == 0)
    {
        if (sei.Size() > 0)
        {
            m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SEIDATA;
            m_compBufDesc[bufCnt].DataSize = mfxU32(sei.Size());
            m_compBufDesc[bufCnt].pCompBuffer = (void *)sei.Buffer();
            bufCnt++;
        }
    }
    else
    {
        if (task.m_did == 0 && task.m_qid == 0)
        {
            if (task.m_insertSps[fieldId] && m_headerPacker.GetScalabilitySei().DataLength > 0)
            {
                m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&m_headerPacker.GetScalabilitySei());
                bufCnt++;
            }

            if (task.m_insertAud[fieldId])
            {
                m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&m_headerPacker.PackAud(task, fieldId));
                bufCnt++;
            }

            if (task.m_insertSps[fieldId])
            {
                std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSps = m_headerPacker.GetSps();

                if (m_headerPacker.isMVC())
                {
                    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                    m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                    m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&packedSps[!!task.m_viewIdx]);
                    bufCnt++;
                }
                else
                {
                    for (size_t i = 0; i < packedSps.size(); i++)
                    {
                        m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                        m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                        m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&packedSps[i]);
                        bufCnt++;
                    }
                }
            }

            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPps = m_headerPacker.GetPps();

            if (task.m_insertPps[fieldId])
            {
                if (m_headerPacker.isMVC())
                {
                    m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                    m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                    m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&packedPps[!!task.m_viewIdx]);
                    bufCnt++;
                }
                else
                {
                    for (size_t i = 0; i < packedPps.size(); i++)
                    {
                        m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                        m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                        m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&packedPps[i]);
                        bufCnt++;
                    }
                }
            }
        }

        if (sei.Size() > 0)
        {
            packedSei.pData                  = RemoveConst(sei.Buffer());
            packedSei.BufferSize             = sei.Size();
            packedSei.DataLength             = sei.Size();
            packedSei.SkipEmulationByteCount = 0; // choose not to let accelerator insert emulation byte

            m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
            m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
            m_compBufDesc[bufCnt].pCompBuffer          = &packedSei;
            bufCnt++;
        }

        if (SkipFlag != 0)
        {
            m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
            m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
            m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&m_headerPacker.PackSkippedSlice(task, fieldId));
            bufCnt++;

            if (SkipFlag == 2 && m_skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
            {
                m_sizeSkipFrames = 0;

                for (UINT i = 0; i < bufCnt; i++)
                {
                    if (m_compBufDesc[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
                    {
                        ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_compBufDesc[i].pCompBuffer;
                        m_sizeSkipFrames += data.DataLength;
                    }
                }
            }
        } 
        else
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId,m_slice);
            for (size_t i = 0; i < packedSlices.size(); i++)
            {
                m_compBufDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA;
                m_compBufDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                m_compBufDesc[bufCnt].pCompBuffer          = RemoveConst(&packedSlices[i]);
                bufCnt++;
            }
        }
    }

    assert(bufCnt <= m_compBufDesc.size());

    if (SkipFlag != 1)
    {
        m_pps.SkipFrameFlag  = SkipFlag ? SkipFlag : !!m_numSkipFrames;
        m_pps.NumSkipFrames  = m_numSkipFrames;
        m_pps.SizeSkipFrames = m_sizeSkipFrames;
        m_numSkipFrames      = 0;
        m_sizeSkipFrames     = 0;

        try
        {
            HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

            //DumpExecuteBuffers(encodeExecuteParams, DXVA2_Intel_Encode_AVC);

            hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    
            HANDLE handle;
            m_auxDevice->EndFrame(&handle);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }
    } 
    else 
    {
        ENCODE_QUERY_STATUS_PARAMS feedback = {task.m_statusReportNumber[fieldId], 0,};

        if (m_skipMode != MFX_SKIPFRAME_INSERT_NOTHING)
        {
            mfxFrameData bs = {0};
            FrameLocker lock(m_core, bs, task.m_midBit[fieldId]);
            assert(bs.Y);

            mfxU8 *  bsDataStart = bs.Y;
            mfxU8 *  bsEnd       = bs.Y + m_width*m_height;
            mfxU8 *  bsDataEnd   = bsDataStart;

            for (UINT i = 0; i < bufCnt; i++)
            {
                if (m_compBufDesc[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
                {
                    ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_compBufDesc[i].pCompBuffer;
                    bsDataEnd += AddEmulationPreventionAndCopy(data, bsDataEnd, bsEnd, !!m_pps.bEmulationByteInsertion);
                }
            }
            feedback.bitstreamSize = mfxU32(bsDataEnd - bsDataStart);
        }

        m_feedbackCached.Update( CachedFeedback::FeedbackStorage(1, feedback) );

        m_numSkipFrames ++;
        m_sizeSkipFrames += feedback.bitstreamSize;
    }

    m_sps.bResetBRC = false;

    // mvc hack
    m_sps.seq_parameter_set_id = initSpsId;
    m_pps.seq_parameter_set_id = initSpsId;
    m_pps.pic_parameter_set_id = initPpsId;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
    //WA for d3d9 runtime or driver bug in case of TDR:
    // set default error status, in normal case driver will update it to right status
    // in case if TDR occur driver device failed, runtime always return S_OK and doesn't change anything in fitback structures
    for(int i=0; i< (int)m_feedbackUpdate.size(); i++)
        m_feedbackUpdate[i].bStatus = ENCODE_ERROR;
#ifdef NEW_STATUS_REPORTING_DDI_0915
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
#ifdef NEW_STATUS_REPORTING_DDI_0915
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
#else // NEW_STATUS_REPORTING_DDI_0915
                (void *)0,
                0,
#endif // NEW_STATUS_REPORTING_DDI_0915
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength[fieldId] = feedback->bitstreamSize;
        task.m_qpY[fieldId] = feedback->AverageQP;
        m_feedbackCached.Remove(task.m_statusReportNumber[fieldId]);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_ERROR:
        assert(!"tast status ERROR");
        return MFX_ERR_DEVICE_FAILED;

    case ENCODE_NOTAVAILABLE:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D9Encoder::Destroy()
{
    m_auxDevice.reset(0);
    return MFX_ERR_NONE;
}

namespace
{
    mfxU32 GetPpsId(MfxVideoParam const & video, mfxU32 did, mfxU32 qid, mfxU32 tid)
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(video);
        return video.calcParam.numLayerOffset[did] + qid * extSvc->DependencyLayer[did].TemporalNum + tid;
    }
};

D3D9SvcEncoder::D3D9SvcEncoder()
: m_core(0)
, m_infoQueried(false)
, m_forcedCodingFunction(0)
{
}

D3D9SvcEncoder::~D3D9SvcEncoder()
{
    Destroy();
}

mfxStatus D3D9SvcEncoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;
    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK_WITH_ASSERT(pID3D != 0, MFX_ERR_DEVICE_FAILED);

    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service, isTemporal);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    std::auto_ptr<AuxiliaryDeviceHlp> auxDevice(new AuxiliaryDeviceHlp());
    sts = auxDevice->Initialize(0, service);
    MFX_CHECK_STS(sts);

    sts = auxDevice->IsAccelerationServiceExist(guid);
    MFX_CHECK_STS(sts);

    ENCODE_CAPS caps = { 0, };
    HRESULT hr = auxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, guid, caps);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    caps.HeaderInsertion = 0;
    caps.MaxNum_QualityLayer = 16;
    caps.MaxNum_DependencyLayer = 8;
    caps.MaxNum_TemporalLayer = 8;
    m_guid = guid;
    m_caps = caps;
    m_auxDevice = auxDevice;
    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    mfxExtSVCSeqDesc * extSvc = GetExtBuffer(par);
    m_video = &par;

    m_sps.resize(par.calcParam.numLayersTotal);
    m_pps.resize(par.calcParam.numLayersTotal);
    m_slice.resize(par.mfx.NumSlice);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);

    for (mfxU32 d = 0; d < par.calcParam.numDependencyLayer; d++)
    {
        mfxU32 did = par.calcParam.did[d];
        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++)
        {
            for (mfxU32 t = 0; t < extSvc->DependencyLayer[did].TemporalNum; t++)
            {
                mfxU32 tid   = extSvc->DependencyLayer[did].TemporalId[t];
                mfxU32 ppsid = GetPpsId(par, d, qid, t);

                FillSpsBuffer(par, m_sps[ppsid], did, qid, tid);
                FillConstPartOfPpsBuffer(par, m_pps[ppsid], did, qid, tid);
            }
        }
    }

    FillConstPartOfSliceBuffer(par, m_slice);

    DXVADDI_VIDEODESC desc = { 0 };
    desc.SampleWidth  = par.mfx.FrameInfo.Width;
    desc.SampleHeight = par.mfx.FrameInfo.Height;
    desc.Format       = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice = { 0 };
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = m_forcedCodingFunction ? m_forcedCodingFunction : ENCODE_ENC_PAK;;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_auxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, m_guid, encodeCreateDevice);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_headerPacker.Init(par, m_caps, false);

    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::Reset(
    MfxVideoParam const & par)
{
    par;
    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
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
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;
    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::QueryEncodeCaps(ENCODE_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;
    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::QueryMbPerSec(mfxVideoParam const & par, mfxU32 (&mbPerSec)[16])
{
    par;
    mbPerSec;

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus D3D9SvcEncoder::Register(mfxFrameAllocResponse & response, D3DDDIFORMAT type)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    EmulSurfaceRegister surfaceReg;
    Zero(surfaceReg);
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_auxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg); // &m_uncompBufInfo[0]
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_auxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}

namespace
{
#define MY_OFFSET_OF(S, M) (mfxU32)((char*)&S.M - (char*)&S)
#define DUMP_STRUCT_MEMBER_OFFSET(S, M)    printf("    (+0x%03x) %-40s = %d\n", MY_OFFSET_OF(S, M), #M, S.M)
#define DUMP_STRUCT_MEMBER_NO_OFFSET(S, M) printf("    (bitfld) %-40s = %d\n",                      #M, S.M)

    void Dump(ENCODE_SET_SEQUENCE_PARAMETERS_H264 const & sps)
    {
        DUMP_STRUCT_MEMBER_OFFSET(sps, FrameWidth);
        DUMP_STRUCT_MEMBER_OFFSET(sps, FrameHeight);
        DUMP_STRUCT_MEMBER_OFFSET(sps, Profile);
        DUMP_STRUCT_MEMBER_OFFSET(sps, Level);
        DUMP_STRUCT_MEMBER_OFFSET(sps, GopPicSize);
        DUMP_STRUCT_MEMBER_OFFSET(sps, GopRefDist);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, GopOptFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved2);
        DUMP_STRUCT_MEMBER_OFFSET(sps, TargetUsage);
        DUMP_STRUCT_MEMBER_OFFSET(sps, RateControlMethod);
        DUMP_STRUCT_MEMBER_OFFSET(sps, TargetBitRate);
        DUMP_STRUCT_MEMBER_OFFSET(sps, MaxBitRate);
        DUMP_STRUCT_MEMBER_OFFSET(sps, MinBitRate);
        DUMP_STRUCT_MEMBER_OFFSET(sps, FramesPer100Sec);
        DUMP_STRUCT_MEMBER_OFFSET(sps, InitVBVBufferFullnessInBit);
        DUMP_STRUCT_MEMBER_OFFSET(sps, VBVBufferSizeInBit);
        DUMP_STRUCT_MEMBER_OFFSET(sps, NumRefFrames);
        DUMP_STRUCT_MEMBER_OFFSET(sps, seq_parameter_set_id);
        DUMP_STRUCT_MEMBER_OFFSET(sps, chroma_format_idc);
        DUMP_STRUCT_MEMBER_OFFSET(sps, bit_depth_luma_minus8);
        DUMP_STRUCT_MEMBER_OFFSET(sps, bit_depth_chroma_minus8);
        DUMP_STRUCT_MEMBER_OFFSET(sps, log2_max_frame_num_minus4);
        DUMP_STRUCT_MEMBER_OFFSET(sps, pic_order_cnt_type);
        DUMP_STRUCT_MEMBER_OFFSET(sps, log2_max_pic_order_cnt_lsb_minus4);
        DUMP_STRUCT_MEMBER_OFFSET(sps, num_ref_frames_in_pic_order_cnt_cycle);
        DUMP_STRUCT_MEMBER_OFFSET(sps, offset_for_non_ref_pic);
        DUMP_STRUCT_MEMBER_OFFSET(sps, offset_for_top_to_bottom_field);
        DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_left_offset);
        DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_right_offset);
        DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_top_offset);
        DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_bottom_offset);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, seq_scaling_matrix_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, seq_scaling_list_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, delta_pic_order_always_zero_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, frame_mbs_only_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, direct_8x8_inference_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, vui_parameters_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, frame_cropping_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, MBZ1);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, bResetBRC);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, bNoAccelerationSPSInsertion);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, GlobalSearch);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, LocalSearch);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, EarlySkip);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved0);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Trellis);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, MBBRC);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved1);
        DUMP_STRUCT_MEMBER_OFFSET(sps, sFlags);
        DUMP_STRUCT_MEMBER_OFFSET(sps, UserMaxFrameSize);
        DUMP_STRUCT_MEMBER_OFFSET(sps, AVBRAccuracy);
        DUMP_STRUCT_MEMBER_OFFSET(sps, AVBRConvergence);
        printf("\n");
    }
    void Dump(ENCODE_SET_SEQUENCE_PARAMETERS_SVC const & sps)
    {
        DUMP_STRUCT_MEMBER_OFFSET(sps, FrameWidth);
        DUMP_STRUCT_MEMBER_OFFSET(sps, FrameHeight);
        DUMP_STRUCT_MEMBER_OFFSET(sps, Profile);
        DUMP_STRUCT_MEMBER_OFFSET(sps, Level);
        DUMP_STRUCT_MEMBER_OFFSET(sps, GopPicSize);
        DUMP_STRUCT_MEMBER_OFFSET(sps, GopRefDist);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, GopOptFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved1);
        DUMP_STRUCT_MEMBER_OFFSET(sps, TargetUsage);
        DUMP_STRUCT_MEMBER_OFFSET(sps, NumRefFrames);
        DUMP_STRUCT_MEMBER_OFFSET(sps, chroma_format_idc);
        DUMP_STRUCT_MEMBER_OFFSET(sps, bit_depth_luma_minus8);
        DUMP_STRUCT_MEMBER_OFFSET(sps, bit_depth_chroma_minus8);
        //DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_left_offset);
        //DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_right_offset);
        //DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_top_offset);
       // DUMP_STRUCT_MEMBER_OFFSET(sps, frame_crop_bottom_offset);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, separate_colour_plane_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, qpprime_y_zero_transform_bypass_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, seq_scaling_matrix_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, frame_mbs_only_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, direct_8x8_inference_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, extended_spatial_scalability_idc);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, chroma_phase_x_plus1_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, chroma_phase_y_plus1);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved3);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, seq_scaling_list_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(sps, Reserved4);
        DUMP_STRUCT_MEMBER_OFFSET(sps, num_units_in_tick);
        DUMP_STRUCT_MEMBER_OFFSET(sps, time_scale);
        DUMP_STRUCT_MEMBER_OFFSET(sps, cpb_cnt_minus1);
        printf("\n");
    }

    void Dump(ENCODE_SET_VUI_PARAMETER const & vui)
    {
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, AspectRatioInfoPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, OverscanInfoPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, OverscanAppropriateFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, VideoSignalTypePresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, VideoFullRangeFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, ColourDescriptionPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, ChromaLocInfoPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, TimingInfoPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, FixedFrameRateFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, NalHrdParametersPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, VclHrdParametersPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, LowDelayHrdFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, PicStructPresentFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, BitstreamRestrictionFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(vui, MotionVectorsOverPicBoundariesFlag);
        DUMP_STRUCT_MEMBER_OFFSET(vui, SarWidth);
        DUMP_STRUCT_MEMBER_OFFSET(vui, SarHeight);
        DUMP_STRUCT_MEMBER_OFFSET(vui, AspectRatioIdc);
        DUMP_STRUCT_MEMBER_OFFSET(vui, VideoFormat);
        DUMP_STRUCT_MEMBER_OFFSET(vui, ColourPrimaries);
        DUMP_STRUCT_MEMBER_OFFSET(vui, TransferCharacteristics);
        DUMP_STRUCT_MEMBER_OFFSET(vui, MatrixCoefficients);
        DUMP_STRUCT_MEMBER_OFFSET(vui, ChromaSampleLocTypeTopField);
        DUMP_STRUCT_MEMBER_OFFSET(vui, ChromaSampleLocTypeBottomField);
        DUMP_STRUCT_MEMBER_OFFSET(vui, MaxBytesPerPicDenom);
        DUMP_STRUCT_MEMBER_OFFSET(vui, MaxBitsPerMbDenom);
        DUMP_STRUCT_MEMBER_OFFSET(vui, Log2MaxMvLength_horizontal);
        DUMP_STRUCT_MEMBER_OFFSET(vui, Log2MaxMvLength_vertical);
        DUMP_STRUCT_MEMBER_OFFSET(vui, NumReorderFrames);
        DUMP_STRUCT_MEMBER_OFFSET(vui, NumUnitsInTick);
        DUMP_STRUCT_MEMBER_OFFSET(vui, TimeScale);
        DUMP_STRUCT_MEMBER_OFFSET(vui, MaxDecFrameBuffering);
        DUMP_STRUCT_MEMBER_OFFSET(vui, CpbCnt_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(vui, BitRate_scale);
        DUMP_STRUCT_MEMBER_OFFSET(vui, CpbSize_scale);
        DUMP_STRUCT_MEMBER_OFFSET(vui, BitRateValueMinus1[0]);
        DUMP_STRUCT_MEMBER_OFFSET(vui, CpbSizeValueMinus1[0]);
        DUMP_STRUCT_MEMBER_OFFSET(vui, CbrFlag); // bit 0 represent SchedSelIdx 0 and so on
        DUMP_STRUCT_MEMBER_OFFSET(vui, InitialCpbRemovalDelayLengthMinus1);
        DUMP_STRUCT_MEMBER_OFFSET(vui, CpbRemovalDelayLengthMinus1);
        DUMP_STRUCT_MEMBER_OFFSET(vui, DpbOutputDelayLengthMinus1);
        DUMP_STRUCT_MEMBER_OFFSET(vui, TimeOffsetLength);
        printf("\n");
    }

    void Dump(ENCODE_SET_PICTURE_PARAMETERS_SVC const & pps)
    {
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrOriginalPic.Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrOriginalPic.AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrReconstructedPic.Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrReconstructedPic.AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CodingType);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, FieldCodingFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, FieldFrameCodingFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, InterleavedFieldBFF);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, ProgressiveField);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bLastLayerInPicture);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bLastPicInSeq);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bLastPicInStream);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, Reserved1);
        DUMP_STRUCT_MEMBER_OFFSET(pps, NumSlice);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bResetBRC);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RateControlMethod);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, MBBRCSupport);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, BRCPrecision);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, Reserved2);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, TargetBitRate);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, MaxBitRate);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, MinBitRate);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, InitVBVBufferFullnessInBit);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, FrameRateNumerator);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, FrameRateDenominator);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, UserMaxLayerSize);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, AVBRAccuracy);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, AVBRConvergence);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, bit_rate_scale);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, cpb_size_scale);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, bit_rate_value_minus1);
        //DUMP_STRUCT_MEMBER_OFFSET(pps, cpb_size_value_minus1);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, fixed_frame_rate_flag);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, nalHrdConformanceRequired);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, vclHrdConformanceRequired);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, low_delay_hrd_flag);
        //DUMP_STRUCT_MEMBER_NO_OFFSET(pps, Reserved3);
        DUMP_STRUCT_MEMBER_OFFSET(pps, QpY);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefLayer.Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefLayer.AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, UsedForReferenceFlags);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CurrFieldOrderCnt[0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CurrFieldOrderCnt[1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[0][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[0][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[1][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[1][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[2][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[2][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[3][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[3][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[4][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[4][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[5][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[5][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[6][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[6][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[7][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[7][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[8][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[8][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[9][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[9][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[10][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[10][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[11][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[11][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[12][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[12][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[13][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[13][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[14][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[14][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[15][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[15][1]);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bUseRawPicForRef);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bDisableAcceleratorHeaderPacking);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEChromaIntraPrediction);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEReferenceLocationDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMESkipCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEStartCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMECostCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bDisableSubMBPartition);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEmulationByteInsertion);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableRollingIntraRefresh);
        DUMP_STRUCT_MEMBER_OFFSET(pps, StatusReportFeedbackNumber);
        DUMP_STRUCT_MEMBER_OFFSET(pps, chroma_qp_index_offset);
        DUMP_STRUCT_MEMBER_OFFSET(pps, second_chroma_qp_index_offset);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, entropy_coding_mode_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, weighted_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, weighted_bipred_idc);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, constrained_intra_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, transform_8x8_mode_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, pic_scaling_matrix_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, pic_scaling_list_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefPicFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, dependency_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, quality_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, temporal_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, constrained_intra_resampling_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, ref_layer_dependency_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, ref_layer_quality_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, tcoeff_level_prediction_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, use_ref_base_pic_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, store_ref_base_pic_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, disable_inter_layer_deblocking_filter_idc);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, next_layer_resolution_change_flag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, inter_layer_slice_alpha_c0_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(pps, inter_layer_slice_beta_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(pps, ref_layer_chroma_phase_x_plus1_flag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, ref_layer_chroma_phase_y_plus1);
        DUMP_STRUCT_MEMBER_OFFSET(pps, scaled_ref_layer_left_offset);
        DUMP_STRUCT_MEMBER_OFFSET(pps, scaled_ref_layer_top_offset);
        DUMP_STRUCT_MEMBER_OFFSET(pps, scaled_ref_layer_right_offset);
        DUMP_STRUCT_MEMBER_OFFSET(pps, scaled_ref_layer_bottom_offset);
        printf("\n");
    }

    void Dump(ENCODE_SET_PICTURE_PARAMETERS_H264 const & pps)
    {
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrOriginalPic.Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrOriginalPic.AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrReconstructedPic.Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, CurrReconstructedPic.AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CodingType);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, FieldCodingFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, FieldFrameCodingFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, InterleavedFieldBFF);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, ProgressiveField);
        DUMP_STRUCT_MEMBER_OFFSET(pps, NumSlice);
        DUMP_STRUCT_MEMBER_OFFSET(pps, QpY);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefFrameList[15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(pps, UsedForReferenceFlags);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CurrFieldOrderCnt[0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, CurrFieldOrderCnt[1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[0][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[0][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[1][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[1][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[2][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[2][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[3][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[3][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[4][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[4][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[5][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[5][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[6][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[6][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[7][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[7][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[8][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[8][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[9][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[9][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[10][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[10][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[11][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[11][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[12][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[12][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[13][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[13][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[14][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[14][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[15][0]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, FieldOrderCntList[15][1]);
        DUMP_STRUCT_MEMBER_OFFSET(pps, frame_num);
        DUMP_STRUCT_MEMBER_OFFSET(pps, bLastPicInSeq);
        DUMP_STRUCT_MEMBER_OFFSET(pps, bLastPicInStream);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bUseRawPicForRef);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bDisableSubMBPartition);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEChromaIntraPrediction);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEReferenceLocationDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMWSkipCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMEStartCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableDVMECostCentersDerivation);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bDisableSubMBPartition);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEmulationByteInsertion);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bEnableRollingIntraRefresh);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, bReserved);
        DUMP_STRUCT_MEMBER_OFFSET(pps, UserFlags);
        DUMP_STRUCT_MEMBER_OFFSET(pps, StatusReportFeedbackNumber);
        DUMP_STRUCT_MEMBER_OFFSET(pps, bIdrPic);
        DUMP_STRUCT_MEMBER_OFFSET(pps, pic_parameter_set_id);
        DUMP_STRUCT_MEMBER_OFFSET(pps, seq_parameter_set_id);
        DUMP_STRUCT_MEMBER_OFFSET(pps, num_ref_idx_l0_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(pps, num_ref_idx_l1_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(pps, chroma_qp_index_offset);
        DUMP_STRUCT_MEMBER_OFFSET(pps, second_chroma_qp_index_offset);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, entropy_coding_mode_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, pic_order_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, weighted_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, weighted_bipred_idc);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, constrained_intra_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, transform_8x8_mode_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, pic_scaling_matrix_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, pic_scaling_list_present_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, RefPicFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, BRCPrecision);
        DUMP_STRUCT_MEMBER_NO_OFFSET(pps, MBZ2);
        DUMP_STRUCT_MEMBER_OFFSET(pps, IntraInsertionLocation);
        DUMP_STRUCT_MEMBER_OFFSET(pps, IntraInsertionSize);
        DUMP_STRUCT_MEMBER_OFFSET(pps, QpDeltaForInsertedIntra);
        printf("\n");
    }

    void Dump(ENCODE_SET_SLICE_HEADER_SVC const & slice)
    {
        DUMP_STRUCT_MEMBER_OFFSET(slice, NumMbsForSlice);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][16].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][16].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][17].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][17].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][18].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][18].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][19].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][19].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][20].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][20].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][21].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][21].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][22].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][22].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][23].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][23].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][24].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][24].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][25].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][25].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][26].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][26].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][27].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][27].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][28].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][28].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][29].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][29].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][30].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][30].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][31].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][31].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][16].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][16].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][17].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][17].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][18].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][18].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][19].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][19].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][20].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][20].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][21].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][21].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][22].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][22].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][23].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][23].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][24].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][24].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][25].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][25].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][26].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][26].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][27].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][27].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][28].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][28].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][29].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][29].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][30].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][30].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][31].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][31].AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(slice, luma_log2_weight_denom);
        DUMP_STRUCT_MEMBER_OFFSET(slice, chroma_log2_weight_denom);
        DUMP_STRUCT_MEMBER_OFFSET(slice, first_mb_in_slice);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_type);
        DUMP_STRUCT_MEMBER_OFFSET(slice, num_ref_idx_l0_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(slice, num_ref_idx_l1_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(slice, cabac_init_idc);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_qp_delta);
        DUMP_STRUCT_MEMBER_OFFSET(slice, disable_deblocking_filter_idc);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_alpha_c0_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_beta_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_id);
        DUMP_STRUCT_MEMBER_OFFSET(slice, MaxSliceSize);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, no_inter_layer_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, direct_spatial_mv_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, base_pred_weight_table_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, scan_idx_start);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, scan_idx_end);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, colour_plane_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, Reserved1);
        printf("\n");
    }

    void Dump(ENCODE_SET_SLICE_HEADER_H264 const & slice)
    {
        DUMP_STRUCT_MEMBER_OFFSET(slice, NumMbsForSlice);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][16].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][16].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][17].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][17].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][18].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][18].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][19].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][19].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][20].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][20].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][21].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][21].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][22].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][22].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][23].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][23].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][24].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][24].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][25].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][25].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][26].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][26].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][27].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][27].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][28].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][28].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][29].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][29].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][30].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][30].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][31].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[0][31].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][0].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][0].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][1].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][1].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][2].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][2].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][3].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][3].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][4].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][4].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][5].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][5].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][6].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][6].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][7].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][7].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][8].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][8].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][9].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][9].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][10].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][10].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][11].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][11].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][12].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][12].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][13].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][13].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][14].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][14].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][15].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][15].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][16].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][16].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][17].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][17].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][18].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][18].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][19].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][19].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][20].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][20].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][21].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][21].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][22].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][22].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][23].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][23].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][24].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][24].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][25].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][25].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][26].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][26].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][27].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][27].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][28].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][28].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][29].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][29].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][30].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][30].AssociatedFlag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][31].Index7Bits);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, RefPicList[1][31].AssociatedFlag);
        DUMP_STRUCT_MEMBER_OFFSET(slice, first_mb_in_slice);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_type);
        DUMP_STRUCT_MEMBER_OFFSET(slice, pic_parameter_set_id);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, direct_spatial_mv_pred_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, num_ref_idx_active_override_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, long_term_reference_flag);
        DUMP_STRUCT_MEMBER_NO_OFFSET(slice, MBZ0);
        DUMP_STRUCT_MEMBER_OFFSET(slice, idr_pic_id);
        DUMP_STRUCT_MEMBER_OFFSET(slice, pic_order_cnt_lsb);
        DUMP_STRUCT_MEMBER_OFFSET(slice, delta_pic_order_cnt_bottom);
        DUMP_STRUCT_MEMBER_OFFSET(slice, delta_pic_order_cnt[0]);
        DUMP_STRUCT_MEMBER_OFFSET(slice, delta_pic_order_cnt[1]);
        DUMP_STRUCT_MEMBER_OFFSET(slice, num_ref_idx_l0_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(slice, num_ref_idx_l1_active_minus1);
        DUMP_STRUCT_MEMBER_OFFSET(slice, luma_log2_weight_denom);
        DUMP_STRUCT_MEMBER_OFFSET(slice, chroma_log2_weight_denom);
        DUMP_STRUCT_MEMBER_OFFSET(slice, cabac_init_idc);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_qp_delta);
        DUMP_STRUCT_MEMBER_OFFSET(slice, disable_deblocking_filter_idc);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_alpha_c0_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_beta_offset_div2);
        DUMP_STRUCT_MEMBER_OFFSET(slice, slice_id);
        printf("\n");
    }
    void Dump(ENCODE_PACKEDHEADER_DATA const & packedHeader)
    {
        DUMP_STRUCT_MEMBER_OFFSET(packedHeader, BufferSize);
        DUMP_STRUCT_MEMBER_OFFSET(packedHeader, DataLength);
        DUMP_STRUCT_MEMBER_OFFSET(packedHeader, DataOffset);
        DUMP_STRUCT_MEMBER_OFFSET(packedHeader, SkipEmulationByteCount);
        DUMP_STRUCT_MEMBER_OFFSET(packedHeader, Reserved);
        printf("\n");
    }

#undef MY_OFFSET_OF
#undef DUMP_STRUCT_MEMBER_OFFSET
#undef DUMP_STRUCT_MEMBER_NO_OFFSET

    void Dump(ENCODE_COMPBUFFERDESC const & buffer, const char * name = 0)
    {
        printf("Buffer\n");
        printf("    type   = %d (%s)\n", buffer.CompressedBufferType, name ? name : "<UNKNOWN>");
        printf("    offset = %d\n",      buffer.DataOffset);
        printf("    size   = %d\n",      buffer.DataSize);
    }

    void Dump(ENCODE_EXECUTE_PARAMS const & params, GUID guid)
    {
        for (mfxU32 i = 0; i < params.NumCompBuffers; i++)
        {
            ENCODE_COMPBUFFERDESC const & buffer = params.pCompressedBuffers[i];
            switch ((mfxU32)buffer.CompressedBufferType)
            {
            case D3DDDIFMT_INTELENCODE_SPSDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA:
                if (guid == DXVA2_Intel_Encode_AVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_SPSDATA AVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_H264); i++)
                        Dump(((ENCODE_SET_SEQUENCE_PARAMETERS_H264 *)buffer.pCompBuffer)[i]);
                }
                else if (guid == DXVA2_Intel_Encode_SVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_SPSDATA SVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_SEQUENCE_PARAMETERS_SVC); i++)
                        Dump(((ENCODE_SET_SEQUENCE_PARAMETERS_SVC *)buffer.pCompBuffer)[i]);
                }
                break;

            case D3DDDIFMT_INTELENCODE_VUIDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA:
                Dump(buffer, "D3DDDIFMT_INTELENCODE_VUIDATA AVC");
                for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_VUI_PARAMETER); i++)
                    Dump(((ENCODE_SET_VUI_PARAMETER *)buffer.pCompBuffer)[i]);
                break;

            case D3DDDIFMT_INTELENCODE_PPSDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA:
                if (guid == DXVA2_Intel_Encode_AVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_PPSDATA AVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_H264); i++)
                        Dump(((ENCODE_SET_PICTURE_PARAMETERS_H264 *)buffer.pCompBuffer)[i]);
                }
                else if (guid == DXVA2_Intel_Encode_SVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_PPSDATA SVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_PICTURE_PARAMETERS_SVC); i++)
                        Dump(((ENCODE_SET_PICTURE_PARAMETERS_SVC *)buffer.pCompBuffer)[i]);
                }
                break;

            case D3DDDIFMT_INTELENCODE_SLICEDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA:
                if (guid == DXVA2_Intel_Encode_AVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_SLICEDATA AVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_H264); i++)
                        Dump(((ENCODE_SET_SLICE_HEADER_H264 *)buffer.pCompBuffer)[i]);
                }
                else if (guid == DXVA2_Intel_Encode_SVC)
                {
                    Dump(buffer, "D3DDDIFMT_INTELENCODE_SLICEDATA SVC");
                    for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_SET_SLICE_HEADER_SVC); i++)
                        Dump(((ENCODE_SET_SLICE_HEADER_SVC *)buffer.pCompBuffer)[i]);
                }
                break;
            case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA:
                Dump(buffer, "D3DDDIFMT_INTELENCODE_BITSTREAMDATA");
                printf("    BitstreamId = %d\n", *(mfxU32 *)buffer.pCompBuffer);
                printf("\n");
                break;
            case D3DDDIFMT_INTELENCODE_SEIDATA:
                Dump(buffer, "D3DDDIFMT_INTELENCODE_SEIDATA");
                printf("\n");
                break;
            case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA:
                Dump(buffer, "D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA");
                for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_PACKEDHEADER_DATA); i++)
                    Dump(((ENCODE_PACKEDHEADER_DATA *)buffer.pCompBuffer)[i]);
                break;
            case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
            case D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA:
                Dump(buffer, "D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA");
                for (mfxU32 i = 0; i < buffer.DataSize / sizeof(ENCODE_PACKEDHEADER_DATA); i++)
                    Dump(((ENCODE_PACKEDHEADER_DATA *)buffer.pCompBuffer)[i]);
                break;
            default:
                Dump(buffer);
                printf("\n");
                break;
            }
        }
    }
};

void MfxHwH264Encode::DumpExecuteBuffers(ENCODE_EXECUTE_PARAMS const & params, GUID guid)
{
    ::Dump(params, guid);
}


mfxStatus D3D9SvcEncoder::Execute(
    mfxHDL                     surface,
    DdiTask const &            task,
    mfxU32                     fieldId,
    PreAllocatedVector const & sei)
{
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    sei;

    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(*m_video);

    mfxU32 ppsid = GetPpsId(*m_video, task.m_did, task.m_qid, task.m_tid);

    FillVaringPartOfPpsBuffer(task, fieldId, m_pps[ppsid]);
    //FillVaringPartOfSliceBuffer(*extSvc, task, fieldId, m_sps[m_pps[ppsid].seq_parameter_set_id], m_pps[ppsid], m_slice);
    FillVaringPartOfSliceBuffer(m_caps, *extSvc, task, fieldId, m_sps[ppsid], m_pps[ppsid], m_slice);
    //printf("filled varing part of slice buffer slice.ppsid=%d .did=%d .qid=%d .tid=%d\n",
    //    m_slice[0].pic_parameter_set_id, m_pps[m_slice[0].pic_parameter_set_id].dependency_id,
    //    m_pps[m_slice[0].pic_parameter_set_id].quality_id, m_pps[m_slice[0].pic_parameter_set_id].temporal_id);

    std::vector<ENCODE_COMPBUFFERDESC> compBufferDesc(10 + 18 + m_slice.size() * 2);
    Zero(compBufferDesc);

    ENCODE_EXECUTE_PARAMS encodeExecuteParams = { 0 };
    encodeExecuteParams.NumCompBuffers                     = 0;
    encodeExecuteParams.pCompressedBuffers                 = Begin(compBufferDesc);
    encodeExecuteParams.pCipherCounter                     = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode    = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT & bufCnt = encodeExecuteParams.NumCompBuffers;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_SPSDATA);
    compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SEQUENCE_PARAMETERS_SVC);
    //compBufferDesc[bufCnt].pCompBuffer          = &m_sps[m_pps[ppsid].seq_parameter_set_id];
    compBufferDesc[bufCnt].pCompBuffer          = &m_sps[ppsid];
    bufCnt++;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_PPSDATA);
    compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_PICTURE_PARAMETERS_SVC);
    compBufferDesc[bufCnt].pCompBuffer          = &m_pps[ppsid];
    bufCnt++;

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_SLICEDATA);
    compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SLICE_HEADER_SVC * m_slice.size());
    compBufferDesc[bufCnt].pCompBuffer          = Begin(m_slice);
    bufCnt++;


    std::vector<ENCODE_SET_SLICE_HEADER_SVC> nextLayerSlice(m_slice.size());
    if (task.m_nextLayerTask)
    {
        FillVaringPartOfPpsBuffer(*task.m_nextLayerTask, fieldId, m_pps[ppsid + 1]);
        FillConstPartOfSliceBuffer(*m_video, nextLayerSlice);
        FillVaringPartOfSliceBuffer(m_caps, *extSvc, *task.m_nextLayerTask, fieldId, m_sps[ppsid + 1], m_pps[ppsid + 1], nextLayerSlice);

        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_SPSDATA);
        compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SEQUENCE_PARAMETERS_SVC);
        compBufferDesc[bufCnt].pCompBuffer          = &m_sps[ppsid + 1];
        bufCnt++;

        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_PPSDATA);
        compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_PICTURE_PARAMETERS_SVC);
        compBufferDesc[bufCnt].pCompBuffer          = &m_pps[ppsid + 1];
        bufCnt++;

        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_SLICEDATA);
        compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_SET_SLICE_HEADER_SVC * nextLayerSlice.size());
        compBufferDesc[bufCnt].pCompBuffer          = Begin(nextLayerSlice);
        bufCnt++;
    }

    mfxU32 bitstream = task.m_idxBs[fieldId];
    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(bitstream));
    compBufferDesc[bufCnt].pCompBuffer          = &bitstream;
    bufCnt++;

    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
    ENCODE_PACKEDHEADER_DATA packedPrefix = { 0 };
    ENCODE_PACKEDHEADER_DATA packedSlice  = { 0 };
    packedSlice = packedSlices[0];
    packedSlice.SkipEmulationByteCount = 7;

    //printf("task.m_did=%d task.m_qid=%d m_insertSps=%d\n", task.m_did, task.m_qid, task.m_insertSps[fieldId]);
    if (task.m_did == 0 && task.m_qid == 0)
    {
        if (task.m_insertAud[fieldId])
        {
            compBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
            compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
            compBufferDesc[bufCnt].pCompBuffer          = RemoveConst(&m_headerPacker.PackAud(task, fieldId));
            bufCnt++;
        }   
        if (task.m_insertSps[fieldId])
        {
            compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA);
            compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof ENCODE_PACKEDHEADER_DATA);
            compBufferDesc[bufCnt].pCompBuffer          = RemoveConst(&m_headerPacker.GetScalabilitySei());
            bufCnt++;
            //printf("scalable info sei attached\n");
        }

        if (task.m_insertSps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSps = m_headerPacker.GetSps();
            for (size_t i = 0; i < packedSps.size(); i++)
            {
                compBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                compBufferDesc[bufCnt].pCompBuffer          = RemoveConst(&packedSps[i]);
                bufCnt++;
                //printf("attached sps %d bytes\n", packedSps[i].DataLength);
            }

            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPps = m_headerPacker.GetPps();
            for (size_t i = 0; i < packedPps.size(); i++)
            {
                compBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
                compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
                compBufferDesc[bufCnt].pCompBuffer          = RemoveConst(&packedPps[i]);
                bufCnt++;
                //printf("attached pps %d bytes\n", packedPps[i].DataLength);
            }
        }

        // parse prefix nal unit (should present)
        NaluIterator iter(
            packedSlices[0].pData,
            packedSlices[0].pData + (packedSlices[0].DataLength + 7) / 8);

        packedPrefix.pData                  = iter->begin;
        packedPrefix.DataLength             = mfxU32(iter->end - iter->begin);
        packedPrefix.BufferSize             = packedPrefix.DataLength;
        packedPrefix.SkipEmulationByteCount = 7;

        packedSlice.pData                  += packedPrefix.BufferSize;
        packedSlice.DataLength             -= packedPrefix.DataLength*8;
        packedSlice.BufferSize             -= packedPrefix.BufferSize;
        packedSlice.SkipEmulationByteCount  = 3;
        
        compBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
        compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        compBufferDesc[bufCnt].pCompBuffer          = &packedPrefix;
        bufCnt++;
    }

    compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA);
    compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
    compBufferDesc[bufCnt].pCompBuffer          = &packedSlice;
    bufCnt++;

    for (unsigned int i = 1; i < m_slice.size(); i++)
    {
        compBufferDesc[bufCnt].CompressedBufferType = (D3DDDIFORMAT)(D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA);
        compBufferDesc[bufCnt].DataSize             = mfxU32(sizeof(ENCODE_PACKEDHEADER_DATA));
        compBufferDesc[bufCnt].pCompBuffer          = RemoveConst(&packedSlices[i]);
        bufCnt++;
    
    }
    //printf("slice header attached\n");

    assert(bufCnt <= compBufferDesc.size());

    try
    {
        HRESULT hr = m_auxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        //printf("BeginFrame(%p), hr = 0x%08x\n\n", surface, hr); fflush(stdout);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        //printf("Execute\n"); fflush(stdout);
        //DumpExecuteBuffers(encodeExecuteParams, DXVA2_Intel_Encode_SVC);
        //DumpRefInfo(task, fieldId);
        fflush(stdout);

        hr = m_auxDevice->Execute(ENCODE_ENC_PAK_ID, encodeExecuteParams, (void *)0);
        //printf("Execute hr = 0x%08x\n\n", hr);; fflush(stdout);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_auxDevice->EndFrame(&handle);
        //printf("EndFrame(%p)\n\n", &handle); fflush(stdout);
    }
    catch (...)
    {
        fflush(stdout);
        return MFX_ERR_DEVICE_FAILED;
    }


    return MFX_ERR_NONE;
}

mfxStatus D3D9SvcEncoder::QueryStatus(
    DdiTask & task,
    mfxU32    fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_auxDevice.get(), MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);

#ifdef NEW_STATUS_REPORTING_DDI_0915
    ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr;
    feedbackDescr.SizeOfStatusParamStruct = sizeof(m_feedbackUpdate[0]);
    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
#endif // NEW_STATUS_REPORTING_DDI_0915

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            HRESULT hr = m_auxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
#ifdef NEW_STATUS_REPORTING_DDI_0915
                (void *)&feedbackDescr,
                sizeof(feedbackDescr),
#else // NEW_STATUS_REPORTING_DDI_0915
                (void *)0,
                0,
#endif // NEW_STATUS_REPORTING_DDI_0915
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber[fieldId]);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength[fieldId] = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_statusReportNumber[fieldId]);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D9SvcEncoder::Destroy()
{
    m_auxDevice.reset(0);
    return MFX_ERR_NONE;
}

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE_HW)
/* EOF */
