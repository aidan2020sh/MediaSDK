/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
//#define UMC_ENABLE_H265_VIDEO_DECODER
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "vm_debug.h"
#include "umc_h265_bitstream.h"
#include "umc_h265_bitstream_inlines.h"
#include "umc_h265_slice_decoding.h"
#include "umc_h265_headers.h"

namespace UMC_HEVC_DECODER
{

#if INSTRUMENTED_CABAC
    Ipp32u H265Bitstream::m_c = 0;
    FILE* H265Bitstream::cabac_bits = 0;
#endif

H265BaseBitstream::H265BaseBitstream()
{
    Reset(0, 0);
}

H265BaseBitstream::H265BaseBitstream(Ipp8u * const pb, const Ipp32u maxsize)
{
    Reset(pb, maxsize);
}

H265BaseBitstream::~H265BaseBitstream()
{
}

// Reset the bitstream with new data pointer
void H265BaseBitstream::Reset(Ipp8u * const pb, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = 31;
    m_maxBsSize    = maxsize;

} // void Reset(Ipp8u * const pb, const Ipp32u maxsize)

// Reset the bitstream with new data pointer and bit offset
void H265BaseBitstream::Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)
{
    m_pbs       = (Ipp32u*)pb;
    m_pbsBase   = (Ipp32u*)pb;
    m_bitOffset = offset;
    m_maxBsSize = maxsize;

} // void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)

// Check that position in bitstream didn't move outside the limit
bool H265BaseBitstream::CheckBSLeft()
{
    size_t bitsDecoded = BitsDecoded();
    return (bitsDecoded > m_maxBsSize*8);
}

// Check whether more data is present
bool H265BaseBitstream::More_RBSP_Data()
{
    Ipp32s code, tmp;
    Ipp32u* ptr_state = m_pbs;
    Ipp32s  bit_state = m_bitOffset;

    VM_ASSERT(m_bitOffset >= 0 && m_bitOffset <= 31);

    Ipp32s remaining_bytes = (Ipp32s)BytesLeft();

    if (remaining_bytes <= 0)
        return false;

    // get top bit, it can be "rbsp stop" bit
    ippiGetNBits(m_pbs, m_bitOffset, 1, code);

    // get remain bits, which is less then byte
    tmp = (m_bitOffset + 1) % 8;

    if(tmp)
    {
        ippiGetNBits(m_pbs, m_bitOffset, tmp, code);
        if ((code << (8 - tmp)) & 0x7f)    // most sig bit could be rbsp stop bit
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }
    }

    remaining_bytes = (Ipp32s)BytesLeft();

    // run through remain bytes
    while (0 < remaining_bytes)
    {
        ippiGetBits8(m_pbs, m_bitOffset, code);

        if (code)
        {
            m_pbs = ptr_state;
            m_bitOffset = bit_state;
            // there are more data
            return true;
        }

        remaining_bytes -= 1;
    }

    return false;
}

H265HeadersBitstream::H265HeadersBitstream()
    : H265BaseBitstream()
{
}

H265HeadersBitstream::H265HeadersBitstream(Ipp8u * const pb, const Ipp32u maxsize)
    : H265BaseBitstream(pb, maxsize)
{
}

// Parse HRD information in VPS or in VUI block of SPS
void H265HeadersBitstream::parseHrdParameters(H265HRD *hrd, Ipp8u cprms_present_flag, Ipp32u vps_max_sub_layers)
{
    hrd->initial_cpb_removal_delay_length = 23 + 1;
    hrd->au_cpb_removal_delay_length = 23 + 1;
    hrd->dpb_output_delay_length = 23 + 1;

    if (cprms_present_flag)
    {
        hrd->nal_hrd_parameters_present_flag = Get1Bit();
        hrd->vcl_hrd_parameters_present_flag = Get1Bit();

        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag)
        {
            hrd->sub_pic_hrd_params_present_flag = Get1Bit();
            if (hrd->sub_pic_hrd_params_present_flag)
            {
                hrd->tick_divisor = GetBits(8) + 2;
                hrd->du_cpb_removal_delay_increment_length = GetBits(5) + 1;
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = Get1Bit();
                hrd->dpb_output_delay_du_length = GetBits(5) + 1;
            }

            hrd->bit_rate_scale = GetBits(4);
            hrd->cpb_size_scale = GetBits(4);

            if (hrd->sub_pic_cpb_params_in_pic_timing_sei_flag)
            {
                hrd->cpb_size_du_scale = GetBits(4);
            }

            hrd->initial_cpb_removal_delay_length = GetBits(5) + 1;
            hrd->au_cpb_removal_delay_length = GetBits(5) + 1;
            hrd->dpb_output_delay_length = GetBits(5) + 1;
        }
    }

    for (Ipp32u i = 0; i < vps_max_sub_layers; i++)
    {
        H265HrdSubLayerInfo * hrdSubLayerInfo = hrd->GetHRDSubLayerParam(i);
        hrdSubLayerInfo->fixed_pic_rate_general_flag = Get1Bit();
        
        if (!hrdSubLayerInfo->fixed_pic_rate_general_flag)
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = Get1Bit();
        }
        else
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = 1;
        }

        // Infered to be 0 when not present
        hrdSubLayerInfo->low_delay_hrd_flag = 0;
        hrdSubLayerInfo->cpb_cnt = 1;

        if (hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag)
        {
            hrdSubLayerInfo->elemental_duration_in_tc = GetVLCElementU() + 1;
        }
        else
        {
            hrdSubLayerInfo->low_delay_hrd_flag = Get1Bit();
        }

        if (!hrdSubLayerInfo->low_delay_hrd_flag)
        {
            hrdSubLayerInfo->cpb_cnt = GetVLCElementU() + 1;

            if (hrdSubLayerInfo->cpb_cnt > 32)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        for (Ipp32u nalOrVcl = 0; nalOrVcl < 2; nalOrVcl++)
        {
            if((nalOrVcl == 0 && hrd->nal_hrd_parameters_present_flag) ||
               (nalOrVcl == 1 && hrd->vcl_hrd_parameters_present_flag))
            {
                for (Ipp32u j = 0 ; j < hrdSubLayerInfo->cpb_cnt; j++)
                {
                    hrdSubLayerInfo->bit_rate_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    hrdSubLayerInfo->cpb_size_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    if (hrd->sub_pic_hrd_params_present_flag)
                    {
                        hrdSubLayerInfo->bit_rate_du_value[j][nalOrVcl] = GetVLCElementU() + 1;
                        hrdSubLayerInfo->cpb_size_du_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    }

                    hrdSubLayerInfo->cbr_flag[j][nalOrVcl] = Get1Bit();
                }
            }
        }
    }
}

// Part VPS header
UMC::Status H265HeadersBitstream::GetVideoParamSet(H265VideoParamSet *pcVPS)
{
    UMC::Status ps = UMC::UMC_OK;

    pcVPS->vps_video_parameter_set_id = GetBits(4);

    Ipp32s vps_reserved_three_2bits = GetBits(2);
    VM_ASSERT(vps_reserved_three_2bits == 3);
    if (vps_reserved_three_2bits != 3)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcVPS->vps_max_layers = GetBits(6) + 1; // vps_max_layers_minus1

    Ipp32u vps_max_sub_layers_minus1 = GetBits(3);
    if (vps_max_sub_layers_minus1 > 6)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcVPS->vps_max_sub_layers = vps_max_sub_layers_minus1 + 1;
    pcVPS->vps_temporal_id_nesting_flag = Get1Bit();

    if (pcVPS->vps_max_sub_layers == 1 && !pcVPS->vps_temporal_id_nesting_flag)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    
    Ipp32u vps_reserved_ffff_16bits = GetBits(16);
    VM_ASSERT(vps_reserved_ffff_16bits == 0xffff);
    if (vps_reserved_ffff_16bits != 0xffff)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    parsePTL(pcVPS->getPTL(), pcVPS->vps_max_sub_layers - 1);

    Ipp32u vps_sub_layer_ordering_info_present_flag = Get1Bit();;
    for(Ipp32u i = 0; i < pcVPS->vps_max_sub_layers; i++)
    {
        pcVPS->vps_max_dec_pic_buffering[i] = GetVLCElementU() + 1;

        if (pcVPS->vps_max_dec_pic_buffering[i] > 16)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcVPS->vps_num_reorder_pics[i] = GetVLCElementU();

        if (pcVPS->vps_num_reorder_pics[i] > pcVPS->vps_max_dec_pic_buffering[i])
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcVPS->vps_max_latency_increase[i] = GetVLCElementU() - 1;

        if (!vps_sub_layer_ordering_info_present_flag)
        {
            for (i++; i < pcVPS->vps_max_sub_layers; i++)
            {
                pcVPS->vps_max_dec_pic_buffering[i] = pcVPS->vps_max_dec_pic_buffering[0];
                pcVPS->vps_num_reorder_pics[i] = pcVPS->vps_num_reorder_pics[0];
                pcVPS->vps_max_latency_increase[i] = pcVPS->vps_max_latency_increase[0];
            }

            break;
        }

        if (i > 0)
        {
            if (pcVPS->vps_max_dec_pic_buffering[i] < pcVPS->vps_max_dec_pic_buffering[i - 1] ||
                pcVPS->vps_num_reorder_pics[i] < pcVPS->vps_num_reorder_pics[i - 1])
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    pcVPS->vps_max_layer_id = GetBits(6);
    if (pcVPS->vps_max_layer_id >= MAX_NUH_LAYER_ID)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcVPS->vps_num_layer_sets = GetVLCElementU() + 1;
    if (pcVPS->vps_num_layer_sets > MAX_VPS_NUM_LAYER_SETS)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    for (Ipp32u opsIdx = 1; opsIdx < pcVPS->vps_num_layer_sets; opsIdx++)
    {
        for (Ipp32u i = 0; i <= pcVPS->vps_max_layer_id; i++) // Operation point set
        {
            pcVPS->layer_id_included_flag[opsIdx][i] = Get1Bit();
        }
    }

    H265TimingInfo *timingInfo = pcVPS->getTimingInfo();
    timingInfo->vps_timing_info_present_flag = Get1Bit();

    if (timingInfo->vps_timing_info_present_flag)
    {
        timingInfo->vps_num_units_in_tick = GetBits(32);
        timingInfo->vps_time_scale = GetBits(32);
        timingInfo->vps_poc_proportional_to_timing_flag = Get1Bit();

        if (timingInfo->vps_poc_proportional_to_timing_flag)
        {
            timingInfo->vps_num_ticks_poc_diff_one = GetVLCElementU() + 1;
        }

        pcVPS->vps_num_hrd_parameters = GetVLCElementU();
        if (pcVPS->vps_num_hrd_parameters > MAX_VPS_NUM_LAYER_SETS)  // MAX_VPS_NUM_LAYER_SETS also equals 1024
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pcVPS->vps_num_hrd_parameters > 0)
        {
            pcVPS->createHrdParamBuffer();

            pcVPS->cprms_present_flag[0] = 1;

            for (Ipp32u i = 0; i < pcVPS->vps_num_hrd_parameters; i++)
            {
                pcVPS->hrd_layer_set_idx[i] = GetVLCElementU();
                if (i > 0)
                {
                    pcVPS->cprms_present_flag[i] = Get1Bit();
                }

                parseHrdParameters(pcVPS->getHrdParameters(i), pcVPS->cprms_present_flag[i], pcVPS->vps_max_sub_layers);
            }
        }
    }

    Ipp8u vps_extension_flag = Get1Bit();
    if (vps_extension_flag)
    {
        while (MoreRbspData())
        {
            Get1Bit(); // vps_extension_data_flag
        }
    }

    return ps;
}

// Parse scaling list data block
void H265HeadersBitstream::xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId)
{
    int i,coefNum = IPP_MIN(MAX_MATRIX_COEF_NUM,(int)g_scalingListSize[sizeId]);
    int nextCoef = SCALING_LIST_START_VALUE;
    const Ipp16u *scan  = (sizeId == 0) ? ScanTableDiag4x4 : g_sigLastScanCG32x32;
    int *dst = scalingList->getScalingListAddress(sizeId, listId);

    if( sizeId > SCALING_LIST_8x8 )
    {
        Ipp32s scaling_list_dc_coef_minus8 = GetVLCElementS();
        if (scaling_list_dc_coef_minus8 < -7 || scaling_list_dc_coef_minus8 > 247)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        scalingList->setScalingListDC(sizeId, listId, scaling_list_dc_coef_minus8 + 8);
        nextCoef = scalingList->getScalingListDC(sizeId,listId);
    }

    for(i = 0; i < coefNum; i++)
    {
        Ipp32s scaling_list_delta_coef = GetVLCElementS();

        if (scaling_list_delta_coef < -128 || scaling_list_delta_coef > 127)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        nextCoef = (nextCoef + scaling_list_delta_coef + 256 ) % 256;
        dst[scan[i]] = nextCoef;
    }
}

// Parse scaling list information in SPS or PPS
void H265HeadersBitstream::parseScalingList(H265ScalingList *scalingList)
{
    //for each size
    for(Ipp32u sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(Ipp32u listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            Ipp8u scaling_list_pred_mode_flag = Get1Bit();
            if(!scaling_list_pred_mode_flag) //Copy Mode
            {
                Ipp32u scaling_list_pred_matrix_id_delta = GetVLCElementU();
                if (scaling_list_pred_matrix_id_delta > listId)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                scalingList->setRefMatrixId (sizeId, listId, listId-scaling_list_pred_matrix_id_delta);
                if (sizeId > SCALING_LIST_8x8)
                {
                    scalingList->setScalingListDC(sizeId,listId,((listId == scalingList->getRefMatrixId (sizeId,listId))? 16 :scalingList->getScalingListDC(sizeId, scalingList->getRefMatrixId (sizeId,listId))));
                }

                scalingList->processRefMatrix( sizeId, listId, scalingList->getRefMatrixId (sizeId,listId));
            }
            else //DPCM Mode
            {
                xDecodeScalingList(scalingList, sizeId, listId);
            }
        }
    }
}

// Parse profile tier layers header part in VPS or SPS
void H265HeadersBitstream::parsePTL(H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1 )
{
    parseProfileTier(rpcPTL->GetGeneralPTL());

    Ipp32s level_idc = GetBits(8);
    level_idc = ((level_idc*10) / 30);
    rpcPTL->GetGeneralPTL()->level_idc = level_idc;

    for(int i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (Get1Bit())
            rpcPTL->sub_layer_profile_present_flags |= 1 << i;
        if (Get1Bit())
            rpcPTL->sub_layer_level_present_flag |= 1 << i;
    }

    if (maxNumSubLayersMinus1 > 0)
    {
        for (int i = maxNumSubLayersMinus1; i < 8; i++)
        {
            Ipp32u reserved_zero_2bits = GetBits(2);
            if (reserved_zero_2bits)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    for(int i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (rpcPTL->sub_layer_profile_present_flags & (1 << i))
        {
            parseProfileTier(rpcPTL->GetSubLayerPTL(i));
        }

        if (rpcPTL->sub_layer_level_present_flag & (1 << i))
        {
            Ipp32s level_idc = GetBits(8);
            level_idc = ((level_idc*10) / 30);
            rpcPTL->GetSubLayerPTL(i)->level_idc = level_idc;
        }
    }
}

// Parse one profile tier layer
void H265HeadersBitstream::parseProfileTier(H265PTL *ptl)
{
    ptl->profile_space = GetBits(2);
    if (ptl->profile_space)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    ptl->tier_flag = Get1Bit();
    ptl->profile_idc = GetBits(5);

    for(int j = 0; j < 32; j++)
    {
        if (Get1Bit())
            ptl->profile_compatibility_flags |= 1 << j;
    }

    if (!ptl->profile_idc)
    {
        ptl->profile_idc = H265_PROFILE_MAIN;
        for(int j = 1; j < 32; j++)
        {
            if (ptl->profile_compatibility_flags & (1 << j))
            {
                ptl->profile_idc = j;
                break;
            }
        }
    }

    if (ptl->profile_idc > H265_PROFILE_FREXT)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    ptl->progressive_source_flag    = Get1Bit();
    ptl->interlaced_source_flag     = Get1Bit();
    ptl->non_packed_constraint_flag = Get1Bit();
    ptl->frame_only_constraint_flag = Get1Bit();

    if (ptl->profile_idc == H265_PROFILE_FREXT || (ptl->profile_compatibility_flags & (1 << 4)))
    {
        Ipp8u max_12bit_constraint_flag = Get1Bit();
        Ipp8u max_10bit_constraint_flag = Get1Bit();
        Ipp8u max_8bit_constraint_flag = Get1Bit();
        Ipp8u max_422chroma_constraint_flag = Get1Bit();
        Ipp8u max_420chroma_constraint_flag = Get1Bit();
        Ipp8u max_monochrome_constraint_flag = Get1Bit();
        Ipp8u intra_constraint_flag = Get1Bit();
        Ipp8u one_picture_only_constraint_flag = Get1Bit();
        Ipp8u lower_bit_rate_constraint_flag = Get1Bit();

        Ipp32u XXX_reserved_zero_35bits = GetBits(32);
        XXX_reserved_zero_35bits = GetBits(3);
    }
    else
    {
        Ipp32u XXX_reserved_zero_44bits = GetBits(32);
        XXX_reserved_zero_44bits = GetBits(12);
        //if (XXX_reserved_zero_44bits)
          //  throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
}

// Parse SPS header
UMC::Status H265HeadersBitstream::GetSequenceParamSet(H265SeqParamSet *pcSPS)
{
    pcSPS->sps_video_parameter_set_id = GetBits(4);

    pcSPS->sps_max_sub_layers = GetBits(3) + 1;
    
    if (pcSPS->sps_max_sub_layers > 7)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->sps_temporal_id_nesting_flag = Get1Bit();
    
    if (pcSPS->sps_max_sub_layers == 1)
    {
        // sps_temporal_id_nesting_flag must be 1 when sps_max_sub_layers_minus1 is 0
        if (pcSPS->sps_temporal_id_nesting_flag != 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    parsePTL(pcSPS->getPTL(), pcSPS->sps_max_sub_layers - 1);

    pcSPS->sps_seq_parameter_set_id = (Ipp8u)GetVLCElementU();
    if (pcSPS->sps_seq_parameter_set_id > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->chroma_format_idc = (Ipp8u)GetVLCElementU();
    if (pcSPS->chroma_format_idc > 3)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (pcSPS->chroma_format_idc == 3)
    {
        pcSPS->separate_colour_plane_flag = Get1Bit();
    }

    pcSPS->ChromaArrayType = pcSPS->separate_colour_plane_flag ? 0 : pcSPS->chroma_format_idc;
    pcSPS->chromaShiftW = 1;
    pcSPS->chromaShiftH = pcSPS->ChromaArrayType == CHROMA_FORMAT_422 ? 0 : 1;

    pcSPS->pic_width_in_luma_samples  = GetVLCElementU();
    pcSPS->pic_height_in_luma_samples = GetVLCElementU();
    pcSPS->conformance_window_flag = Get1Bit();

    if (pcSPS->conformance_window_flag)
    {
        pcSPS->conf_win_left_offset  = GetVLCElementU()*pcSPS->SubWidthC();
        pcSPS->conf_win_right_offset = GetVLCElementU()*pcSPS->SubWidthC();
        pcSPS->conf_win_top_offset   = GetVLCElementU()*pcSPS->SubHeightC();
        pcSPS->conf_win_bottom_offset = GetVLCElementU()*pcSPS->SubHeightC();

        if (pcSPS->conf_win_left_offset + pcSPS->conf_win_right_offset >= pcSPS->pic_width_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pcSPS->conf_win_top_offset + pcSPS->conf_win_bottom_offset >= pcSPS->pic_height_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    pcSPS->bit_depth_luma = GetVLCElementU() + 8;
    if (pcSPS->bit_depth_luma > 14)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->setQpBDOffsetY(6*(pcSPS->bit_depth_luma - 8));

    pcSPS->bit_depth_chroma = GetVLCElementU() + 8;
    if (pcSPS->bit_depth_chroma > 14)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->setQpBDOffsetC(6*(pcSPS->bit_depth_chroma - 8));

    if ((pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8) && pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc < H265_PROFILE_MAIN10)
        pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc = H265_PROFILE_MAIN10;

    if (pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc == H265_PROFILE_MAIN10 || pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8)
        pcSPS->need16bitOutput = 1;

    pcSPS->log2_max_pic_order_cnt_lsb = 4 + GetVLCElementU();
    if (pcSPS->log2_max_pic_order_cnt_lsb > 16)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->sps_sub_layer_ordering_info_present_flag = Get1Bit();

    for (Ipp32u i = 0; i < pcSPS->sps_max_sub_layers; i++)
    {
        pcSPS->sps_max_dec_pic_buffering[i] = GetVLCElementU() + 1;

        if (pcSPS->sps_max_dec_pic_buffering[i] > 16)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->sps_max_num_reorder_pics[i] = GetVLCElementU();

        if (pcSPS->sps_max_num_reorder_pics[i] > pcSPS->sps_max_dec_pic_buffering[i])
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->sps_max_latency_increase[i] = GetVLCElementU() - 1;

        if (!pcSPS->sps_sub_layer_ordering_info_present_flag)
        {
            for (i++; i <= pcSPS->sps_max_sub_layers-1; i++)
            {
                pcSPS->sps_max_dec_pic_buffering[i] = pcSPS->sps_max_dec_pic_buffering[0];
                pcSPS->sps_max_num_reorder_pics[i] = pcSPS->sps_max_num_reorder_pics[0];
                pcSPS->sps_max_latency_increase[i] = pcSPS->sps_max_latency_increase[0];
            }
            break;
        }

        if (i > 0)
        {
            if (pcSPS->sps_max_dec_pic_buffering[i] < pcSPS->sps_max_dec_pic_buffering[i - 1] ||
                pcSPS->sps_max_num_reorder_pics[i] < pcSPS->sps_max_num_reorder_pics[i - 1])
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    pcSPS->log2_min_luma_coding_block_size = GetVLCElementU() + 3;

    Ipp32u MinCbLog2SizeY = pcSPS->log2_min_luma_coding_block_size;
    Ipp32u MinCbSizeY = 1 << pcSPS->log2_min_luma_coding_block_size;

    if ((pcSPS->pic_width_in_luma_samples % MinCbSizeY) || (pcSPS->pic_height_in_luma_samples % MinCbSizeY))
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    Ipp32u log2_diff_max_min_coding_block_size = GetVLCElementU();
    pcSPS->log2_max_luma_coding_block_size = log2_diff_max_min_coding_block_size + pcSPS->log2_min_luma_coding_block_size;
    pcSPS->MaxCUSize =  1 << pcSPS->log2_max_luma_coding_block_size;

    pcSPS->log2_min_transform_block_size = GetVLCElementU() + 2;

    if (pcSPS->log2_min_transform_block_size >= MinCbLog2SizeY)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    Ipp32u log2_diff_max_min_transform_block_size = GetVLCElementU();
    pcSPS->log2_max_transform_block_size = log2_diff_max_min_transform_block_size + pcSPS->log2_min_transform_block_size;
    pcSPS->m_maxTrSize = 1 << pcSPS->log2_max_transform_block_size;

    Ipp32u CtbLog2SizeY = pcSPS->log2_max_luma_coding_block_size;
    if (pcSPS->log2_max_transform_block_size > IPP_MIN(5, CtbLog2SizeY))
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->max_transform_hierarchy_depth_inter = GetVLCElementU();
    pcSPS->max_transform_hierarchy_depth_intra = GetVLCElementU();

    if (pcSPS->max_transform_hierarchy_depth_inter > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    
    if (pcSPS->max_transform_hierarchy_depth_intra > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    Ipp32u addCUDepth = 0;
    while((pcSPS->MaxCUSize >> log2_diff_max_min_coding_block_size) > (Ipp32u)( 1 << ( pcSPS->log2_min_transform_block_size + addCUDepth)))
    {
        addCUDepth++;
    }

    pcSPS->AddCUDepth = addCUDepth;
    pcSPS->MaxCUDepth = log2_diff_max_min_coding_block_size + addCUDepth;
    pcSPS->MinCUSize = pcSPS->MaxCUSize >> pcSPS->MaxCUDepth;
    // BB: these parameters may be removed completly and replaced by the fixed values
    pcSPS->scaling_list_enabled_flag = Get1Bit();
    if(pcSPS->scaling_list_enabled_flag)
    {
        pcSPS->sps_scaling_list_data_present_flag = Get1Bit();
        if(pcSPS->sps_scaling_list_data_present_flag)
        {
            parseScalingList( pcSPS->getScalingList() );
        }
    }

    pcSPS->amp_enabled_flag = Get1Bit();
    pcSPS->sample_adaptive_offset_enabled_flag = Get1Bit();
    pcSPS->pcm_enabled_flag = Get1Bit();

    if(pcSPS->pcm_enabled_flag)
    {
        pcSPS->pcm_sample_bit_depth_luma = GetBits(4) + 1;
        pcSPS->pcm_sample_bit_depth_chroma = GetBits(4) + 1;

        if (pcSPS->pcm_sample_bit_depth_luma > pcSPS->bit_depth_luma ||
            pcSPS->pcm_sample_bit_depth_chroma > pcSPS->bit_depth_chroma)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->log2_min_pcm_luma_coding_block_size = GetVLCElementU() + 3;

        if (pcSPS->log2_min_pcm_luma_coding_block_size < IPP_MIN(MinCbLog2SizeY, 5) || pcSPS->log2_min_pcm_luma_coding_block_size > IPP_MIN(CtbLog2SizeY, 5))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcSPS->log2_max_pcm_luma_coding_block_size = GetVLCElementU() + pcSPS->log2_min_pcm_luma_coding_block_size;

        if (pcSPS->log2_max_pcm_luma_coding_block_size > IPP_MIN(CtbLog2SizeY, 5))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        
        pcSPS->pcm_loop_filter_disabled_flag = Get1Bit();
    }

    pcSPS->num_short_term_ref_pic_sets = GetVLCElementU();
    if (pcSPS->num_short_term_ref_pic_sets > 64)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcSPS->createRPSList(pcSPS->num_short_term_ref_pic_sets);

    ReferencePictureSetList* rpsList = pcSPS->getRPSList();
    ReferencePictureSet* rps;

    for(Ipp32u i = 0; i < rpsList->getNumberOfReferencePictureSets(); i++)
    {
        rps = rpsList->getReferencePictureSet(i);
        parseShortTermRefPicSet(pcSPS, rps, i);
    }

    pcSPS->long_term_ref_pics_present_flag = Get1Bit();
    if (pcSPS->long_term_ref_pics_present_flag)
    {
        pcSPS->num_long_term_ref_pics_sps = GetVLCElementU();

        if (pcSPS->num_long_term_ref_pics_sps > 32)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        for (Ipp32u k = 0; k < pcSPS->num_long_term_ref_pics_sps; k++)
        {
            pcSPS->lt_ref_pic_poc_lsb_sps[k] = GetBits(pcSPS->log2_max_pic_order_cnt_lsb);
            pcSPS->used_by_curr_pic_lt_sps_flag[k] = Get1Bit();
        }
    }

    pcSPS->sps_temporal_mvp_enabled_flag = Get1Bit();
    pcSPS->sps_strong_intra_smoothing_enabled_flag = Get1Bit();
    pcSPS->vui_parameters_present_flag = Get1Bit();

    if (pcSPS->vui_parameters_present_flag)
    {
        parseVUI(pcSPS);
    }

    Ipp8u sps_extension_present_flag = Get1Bit();
    if (sps_extension_present_flag)
    {
        pcSPS->sps_range_extension_flag = Get1Bit();
        Ipp32u sps_extension_7bits = GetBits(7);

        if (pcSPS->sps_range_extension_flag)
        {
            pcSPS->transform_skip_rotation_enabled_flag = Get1Bit();
            pcSPS->transform_skip_context_enabled_flag = Get1Bit();
            pcSPS->implicit_residual_dpcm_enabled_flag = Get1Bit();
            pcSPS->explicit_residual_dpcm_enabled_flag = Get1Bit();
            pcSPS->extended_precision_processing_flag = Get1Bit();
            pcSPS->intra_smoothing_disabled_flag = Get1Bit();
            pcSPS->high_precision_offsets_enabled_flag = Get1Bit();
            pcSPS->fast_rice_adaptation_enabled_flag = Get1Bit();
            pcSPS->cabac_bypass_alignment_enabled_flag = Get1Bit();
        }

        if (sps_extension_7bits)
        {
            while (MoreRbspData())
            {
                /*Ipp8u sps_extension_data_flag =*/ Get1Bit();
            }
        }
    }

    return UMC::UMC_OK;
}    // GetSequenceParamSet

// Parse video usability information block in SPS
void H265HeadersBitstream::parseVUI(H265SeqParamSet *pSPS)
{
    pSPS->aspect_ratio_info_present_flag = Get1Bit();
    if (pSPS->aspect_ratio_info_present_flag)
    {
        pSPS->aspect_ratio_idc = GetBits(8);
        if (pSPS->aspect_ratio_idc == 255)
        {
            pSPS->sar_width = GetBits(16);
            pSPS->sar_height = GetBits(16);
        }
        else
        {
            if (!pSPS->aspect_ratio_idc || pSPS->aspect_ratio_idc >= sizeof(SAspectRatio)/sizeof(SAspectRatio[0]))
            {
                pSPS->aspect_ratio_idc = 0;
                pSPS->aspect_ratio_info_present_flag = 0;
            }
            else
            {
                pSPS->sar_width = SAspectRatio[pSPS->aspect_ratio_idc][0];
                pSPS->sar_height = SAspectRatio[pSPS->aspect_ratio_idc][1];
            }
        }
    }

    pSPS->overscan_info_present_flag = Get1Bit();
    if (pSPS->overscan_info_present_flag)
    {
        pSPS->overscan_appropriate_flag = Get1Bit();
    }

    pSPS->video_signal_type_present_flag = Get1Bit();
    if (pSPS->video_signal_type_present_flag)
    {
        pSPS->video_format = GetBits(3);
        pSPS->video_full_range_flag = Get1Bit();
        pSPS->colour_description_present_flag = Get1Bit();
        if (pSPS->colour_description_present_flag)
        {
            pSPS->colour_primaries = GetBits(8);
            pSPS->transfer_characteristics = GetBits(8);
            pSPS->matrix_coeffs = GetBits(8);
        }
    }

    pSPS->chroma_loc_info_present_flag = Get1Bit();
    if (pSPS->chroma_loc_info_present_flag)
    {
        pSPS->chroma_sample_loc_type_top_field = GetVLCElementU();
        pSPS->chroma_sample_loc_type_bottom_field = GetVLCElementU();
    }

    pSPS->neutral_chroma_indication_flag = Get1Bit();
    pSPS->field_seq_flag = Get1Bit();
    pSPS->frame_field_info_present_flag = Get1Bit();

    pSPS->default_display_window_flag = Get1Bit();
    if (pSPS->default_display_window_flag)
    {
        pSPS->def_disp_win_left_offset   = GetVLCElementU()*pSPS->SubWidthC();
        pSPS->def_disp_win_right_offset  = GetVLCElementU()*pSPS->SubWidthC();
        pSPS->def_disp_win_top_offset    = GetVLCElementU()*pSPS->SubHeightC();
        pSPS->def_disp_win_bottom_offset = GetVLCElementU()*pSPS->SubHeightC();

        if (pSPS->def_disp_win_left_offset + pSPS->def_disp_win_right_offset >= pSPS->pic_width_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pSPS->def_disp_win_top_offset + pSPS->def_disp_win_bottom_offset >= pSPS->pic_height_in_luma_samples)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    H265TimingInfo *timingInfo = pSPS->getTimingInfo();
    timingInfo->vps_timing_info_present_flag = Get1Bit();
    if (timingInfo->vps_timing_info_present_flag)
    {
        timingInfo->vps_num_units_in_tick = GetBits(32);
        timingInfo->vps_time_scale = GetBits(32);

        timingInfo->vps_poc_proportional_to_timing_flag = Get1Bit();
        if (timingInfo->vps_poc_proportional_to_timing_flag)
        {
            timingInfo->vps_num_ticks_poc_diff_one = GetVLCElementU() + 1;
        }

        pSPS->vui_hrd_parameters_present_flag = Get1Bit();
        if (pSPS->vui_hrd_parameters_present_flag)
        {
            parseHrdParameters( pSPS->getHrdParameters(), 1, pSPS->sps_max_sub_layers);
        }
    }

    pSPS->bitstream_restriction_flag = Get1Bit();
    if (pSPS->bitstream_restriction_flag)
    {
        pSPS->tiles_fixed_structure_flag = Get1Bit();
        pSPS->motion_vectors_over_pic_boundaries_flag = Get1Bit();
        pSPS->min_spatial_segmentation_idc = GetBits(8);
        pSPS->max_bytes_per_pic_denom = GetVLCElementU();
        pSPS->max_bits_per_min_cu_denom = GetVLCElementU();
        pSPS->log2_max_mv_length_horizontal = GetVLCElementU();
        pSPS->log2_max_mv_length_vertical = GetVLCElementU();
    }
}

// Reserved for future header extensions
bool H265HeadersBitstream::MoreRbspData()
{
    return false;
}

// Parse PPS header
UMC::Status H265HeadersBitstream::GetPictureParamSetFull(H265PicParamSet  *pcPPS)
{
    pcPPS->pps_pic_parameter_set_id = GetVLCElementU();
    pcPPS->pps_seq_parameter_set_id = GetVLCElementU();
    pcPPS->dependent_slice_segments_enabled_flag = Get1Bit();
    pcPPS->output_flag_present_flag = Get1Bit();
    pcPPS->num_extra_slice_header_bits = GetBits(3);
    pcPPS->sign_data_hiding_enabled_flag =  Get1Bit();
    pcPPS->cabac_init_present_flag =  Get1Bit();
    pcPPS->num_ref_idx_l0_default_active = GetVLCElementU() + 1;

    if (pcPPS->num_ref_idx_l0_default_active > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->num_ref_idx_l1_default_active = GetVLCElementU() + 1;

    if (pcPPS->num_ref_idx_l1_default_active > 15)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->init_qp = (Ipp8s)(GetVLCElementS() + 26);

    pcPPS->constrained_intra_pred_flag = Get1Bit();
    pcPPS->transform_skip_enabled_flag = Get1Bit();

    pcPPS->cu_qp_delta_enabled_flag = Get1Bit();
    if( pcPPS->cu_qp_delta_enabled_flag )
    {
        pcPPS->diff_cu_qp_delta_depth = GetVLCElementU();
    }
    else
    {
        pcPPS->diff_cu_qp_delta_depth = 0;
    }

    pcPPS->pps_cb_qp_offset = GetVLCElementS();
    if (pcPPS->pps_cb_qp_offset < -12 || pcPPS->pps_cb_qp_offset > 12)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->pps_cr_qp_offset = GetVLCElementS();

    if (pcPPS->pps_cr_qp_offset < -12 || pcPPS->pps_cr_qp_offset > 12)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    pcPPS->pps_slice_chroma_qp_offsets_present_flag = Get1Bit();

    pcPPS->weighted_pred_flag = Get1Bit();
    pcPPS->weighted_bipred_flag = Get1Bit();

    pcPPS->transquant_bypass_enabled_flag = Get1Bit();
    pcPPS->tiles_enabled_flag = Get1Bit();
    pcPPS->entropy_coding_sync_enabled_flag = Get1Bit();

    if (pcPPS->tiles_enabled_flag)
    {
        pcPPS->num_tile_columns = GetVLCElementU() + 1;
        pcPPS->num_tile_rows = GetVLCElementU() + 1;

        if (pcPPS->num_tile_columns == 1 && pcPPS->num_tile_rows == 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        pcPPS->uniform_spacing_flag = Get1Bit();

        if (!pcPPS->uniform_spacing_flag)
        {
            pcPPS->column_width = h265_new_array_throw<Ipp32u>(pcPPS->num_tile_columns);
            for (Ipp32u i=0; i < pcPPS->num_tile_columns - 1; i++)
            {
                pcPPS->column_width[i] = GetVLCElementU() + 1;
            }

            pcPPS->row_height = h265_new_array_throw<Ipp32u>(pcPPS->num_tile_rows);

            for (Ipp32u i=0; i < pcPPS->num_tile_rows - 1; i++)
            {
                pcPPS->row_height[i] = GetVLCElementU() + 1;
            }
        }

        if (pcPPS->num_tile_columns - 1 != 0 || pcPPS->num_tile_rows - 1 != 0)
        {
            pcPPS->loop_filter_across_tiles_enabled_flag = Get1Bit();
        }
    }
    else
    {
        pcPPS->num_tile_columns = 1;
        pcPPS->num_tile_rows = 1;
    }

    pcPPS->pps_loop_filter_across_slices_enabled_flag = Get1Bit();
    pcPPS->deblocking_filter_control_present_flag = Get1Bit();

    if (pcPPS->deblocking_filter_control_present_flag)
    {
        pcPPS->deblocking_filter_override_enabled_flag = Get1Bit();
        pcPPS->pps_deblocking_filter_disabled_flag = Get1Bit();
        if (!pcPPS->pps_deblocking_filter_disabled_flag)
        {
            pcPPS->pps_beta_offset = GetVLCElementS() << 1;
            pcPPS->pps_tc_offset = GetVLCElementS() << 1;

            if (pcPPS->pps_beta_offset < -12 || pcPPS->pps_beta_offset > 12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            if (pcPPS->pps_tc_offset < -12 || pcPPS->pps_tc_offset > 12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }
    }

    pcPPS->pps_scaling_list_data_present_flag = Get1Bit();
    if(pcPPS->pps_scaling_list_data_present_flag)
    {
        parseScalingList( pcPPS->getScalingList() );
    }

    pcPPS->lists_modification_present_flag = Get1Bit();
    pcPPS->log2_parallel_merge_level = GetVLCElementU() + 2;
    pcPPS->slice_segment_header_extension_present_flag = Get1Bit();

    Ipp8u pps_extension_present_flag = Get1Bit();
    if (pps_extension_present_flag)
    {
        Ipp8u pps_range_extensions_flag = Get1Bit();
        Ipp8u pps_extension_7bits = (Ipp8u)GetBits(7);

        if (pps_range_extensions_flag)
        {
            if (pcPPS->transform_skip_enabled_flag)
            {
                pcPPS->log2_max_transform_skip_block_size = GetVLCElementU() + 2;
            }

            pcPPS->cross_component_prediction_enabled_flag = Get1Bit();
            pcPPS->chroma_qp_offset_list_enabled_flag = Get1Bit();
            if (pcPPS->chroma_qp_offset_list_enabled_flag)
            {
                pcPPS->diff_cu_chroma_qp_offset_depth = GetVLCElementU();
                pcPPS->chroma_qp_offset_list_len = GetVLCElementU() + 1;
                for (Ipp32u i = 0; i < pcPPS->chroma_qp_offset_list_len; i++)
                {
                    Ipp32u cb_qp_offset_list = GetVLCElementS();
                    Ipp32u cr_qp_offset_list = GetVLCElementS();
                }
            }

            pcPPS->log2_sao_offset_scale_luma = GetVLCElementU();
            pcPPS->log2_sao_offset_scale_chroma = GetVLCElementU();
        }

        if (pps_extension_7bits)
        {
            while (MoreRbspData())
            {
                Get1Bit();// "pps_extension_data_flag"
            }
        }
    }

    return UMC::UMC_OK;
}   // H265HeadersBitstream::GetPictureParamSetFull

// Parse weighted prediction table in slice header
void H265HeadersBitstream::xParsePredWeightTable(const H265SeqParamSet *sps, H265SliceHeader * sliceHdr)
{
    wpScalingParam* wp;
    SliceType       eSliceType  = sliceHdr->slice_type;
    int             iNbRef      = (eSliceType == B_SLICE ) ? (2) : (1);

    sliceHdr->luma_log2_weight_denom = GetVLCElementU(); // used in HW decoder
    if (sliceHdr->luma_log2_weight_denom > 7)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    if (sps->ChromaArrayType)
    {
        Ipp32s delta_chroma_log2_weight_denom = GetVLCElementS();
        if (delta_chroma_log2_weight_denom + (int)sliceHdr->luma_log2_weight_denom < 0)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        sliceHdr->chroma_log2_weight_denom = (Ipp32u)(delta_chroma_log2_weight_denom + sliceHdr->luma_log2_weight_denom);
    }
    else
        sliceHdr->chroma_log2_weight_denom = 0;

    if (sliceHdr->chroma_log2_weight_denom > 7)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    for (Ipp32s iNumRef = 0; iNumRef < iNbRef; iNumRef++ )
    {
        unsigned uTotalSignalledWeightFlags = 0;
        EnumRefPicList  eRefPicList = ( iNumRef ? REF_PIC_LIST_1 : REF_PIC_LIST_0 );
        for (Ipp32s iRefIdx = 0; iRefIdx < sliceHdr->m_numRefIdx[eRefPicList]; iRefIdx++ )
        {
            wp = sliceHdr->pred_weight_table[eRefPicList][iRefIdx];

            wp[0].log2_weight_denom = sliceHdr->luma_log2_weight_denom;
            wp[1].log2_weight_denom = sliceHdr->chroma_log2_weight_denom;
            wp[2].log2_weight_denom = sliceHdr->chroma_log2_weight_denom;

            wp[0].present_flag = Get1Bit(); // luma_weight_lX_flag
            uTotalSignalledWeightFlags += wp[0].present_flag;
        }

        if (sps->ChromaArrayType)
        {
            for (Ipp32s iRefIdx=0 ; iRefIdx < sliceHdr->m_numRefIdx[eRefPicList] ; iRefIdx++ )
            {
                wp = sliceHdr->pred_weight_table[eRefPicList][iRefIdx];
                wp[1].present_flag = wp[2].present_flag = Get1Bit(); // chroma_weight_lX_flag
                uTotalSignalledWeightFlags += 2*wp[1].present_flag;
            }
        }

        for (Ipp32s iRefIdx=0 ; iRefIdx < sliceHdr->m_numRefIdx[eRefPicList]; iRefIdx++ )
        {
            wp = sliceHdr->pred_weight_table[eRefPicList][iRefIdx];
            if ( wp[0].present_flag )
            {
                wp[0].delta_weight = GetVLCElementS();  // se(v): delta_luma_weight_l0[i]

                if (wp[0].delta_weight < -128 || wp[0].delta_weight > 127)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                wp[0].weight = (wp[0].delta_weight + (1<<wp[0].log2_weight_denom));
                wp[0].offset = GetVLCElementS();       // se(v): luma_offset_l0[i]
            }
            else
            {
                wp[0].delta_weight = 0;
                wp[0].weight = (1 << wp[0].log2_weight_denom);
                wp[0].offset = 0;
            }

            if (sps->ChromaArrayType)
            {
                if (wp[1].present_flag)
                {
                    for (Ipp32s j = 1; j < 3; j++)
                    {
                        wp[j].delta_weight = GetVLCElementS();   // se(v): chroma_weight_l0[i][j]
                        if (wp[j].delta_weight < -128 || wp[j].delta_weight > 127)
                            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                        wp[j].weight = (wp[j].delta_weight + (1<<wp[1].log2_weight_denom));

                        Ipp32s delta_chroma_offset_lX = GetVLCElementS();  // se(v): delta_chroma_offset_l0[i][j]
                        int pred = ( 128 - ( ( 128*wp[j].weight)>>(wp[j].log2_weight_denom) ) );
                        wp[j].offset = Clip3(-128, 127, (delta_chroma_offset_lX + pred) );
                    }
                }
                else
                {
                    for (Ipp32s j = 1; j < 3; j++)
                    {
                        wp[j].weight = (1 << wp[j].log2_weight_denom);
                        wp[j].offset = 0;
                    }
                }
            }
        }

        for (int iRefIdx = sliceHdr->m_numRefIdx[eRefPicList]; iRefIdx < MAX_NUM_REF_PICS; iRefIdx++)
        {
            wp = sliceHdr->pred_weight_table[eRefPicList][iRefIdx];
            wp[0].present_flag = false;
            wp[1].present_flag = false;
            wp[2].present_flag = false;
        }

        if (uTotalSignalledWeightFlags > 24)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
}

// Parse slice header part which contains PPS ID
UMC::Status H265HeadersBitstream::GetSliceHeaderPart1(H265SliceHeader * sliceHdr)
{
    sliceHdr->IdrPicFlag = (sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP) ? 1 : 0;
    sliceHdr->first_slice_segment_in_pic_flag = Get1Bit();

    if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_IDR_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
      || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_CRA )
    {
        sliceHdr->no_output_of_prior_pics_flag = Get1Bit();
    }

    sliceHdr->slice_pic_parameter_set_id = (Ipp16u)GetVLCElementU();

    if (sliceHdr->slice_pic_parameter_set_id > 63)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    return UMC::UMC_OK;
}

// Parse remaining of slice header after GetSliceHeaderPart1
void H265HeadersBitstream::decodeSlice(H265Slice *pSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    if (!pps || !sps)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    H265SliceHeader * sliceHdr = pSlice->GetSliceHeader();
    sliceHdr->collocated_from_l0_flag = 1;

    if (pps->dependent_slice_segments_enabled_flag && !sliceHdr->first_slice_segment_in_pic_flag)
    {
        sliceHdr->dependent_slice_segment_flag = Get1Bit();
    }
    else
    {
        sliceHdr->dependent_slice_segment_flag = 0;
    }

    Ipp32u numCTUs = ((sps->pic_width_in_luma_samples + sps->MaxCUSize-1)/sps->MaxCUSize)*((sps->pic_height_in_luma_samples + sps->MaxCUSize-1)/sps->MaxCUSize);
    Ipp32s maxParts = 1<<(sps->MaxCUDepth<<1);
    Ipp32u bitsSliceSegmentAddress = 0;

    while (numCTUs > Ipp32u(1<<bitsSliceSegmentAddress))
    {
        bitsSliceSegmentAddress++;
    }

    if (!sliceHdr->first_slice_segment_in_pic_flag)
    {
        sliceHdr->slice_segment_address = GetBits(bitsSliceSegmentAddress);

        if (sliceHdr->slice_segment_address >= numCTUs)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    //set uiCode to equal slice start address (or dependent slice start address)
    Ipp32s startCuAddress = maxParts*sliceHdr->slice_segment_address;
    sliceHdr->m_sliceSegmentCurStartCUAddr = startCuAddress;
    sliceHdr->m_sliceSegmentCurEndCUAddr = (numCTUs - 1) *maxParts;
    // DO NOT REMOVE THIS LINE !!!!!!!!!!!!!!!!!!!!!!!!!!

    if (!sliceHdr->dependent_slice_segment_flag)
    {
        sliceHdr->SliceCurStartCUAddr = startCuAddress;
    }

    if (!sliceHdr->dependent_slice_segment_flag)
    {
        for (Ipp32u i = 0; i < pps->num_extra_slice_header_bits; i++)
        {
            Get1Bit(); //slice_reserved_undetermined_flag
        }

        sliceHdr->slice_type = (SliceType)GetVLCElementU();

        if (sliceHdr->slice_type > I_SLICE || sliceHdr->slice_type < B_SLICE)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    }

    if(!sliceHdr->dependent_slice_segment_flag)
    {
        if (pps->output_flag_present_flag)
        {
            sliceHdr->pic_output_flag = Get1Bit();
        }
        else
        {
            sliceHdr->pic_output_flag = 1;
        }

        if (sps->chroma_format_idc > 2)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (sps->separate_colour_plane_flag  ==  1)
        {
            sliceHdr->colour_plane_id = GetBits(2);
        }

        if (sliceHdr->IdrPicFlag)
        {
            sliceHdr->slice_pic_order_cnt_lsb = 0;
            ReferencePictureSet* rps = pSlice->getRPS();
            rps->num_negative_pics = 0;
            rps->num_positive_pics = 0;
            rps->setNumberOfLongtermPictures(0);
            rps->num_pics = 0;
        }
        else
        {
            sliceHdr->slice_pic_order_cnt_lsb = GetBits(sps->log2_max_pic_order_cnt_lsb);

            sliceHdr->short_term_ref_pic_set_sps_flag = Get1Bit();
            if (!sliceHdr->short_term_ref_pic_set_sps_flag) // short term ref pic is signalled
            {
                size_t bitsDecoded = BitsDecoded();

                ReferencePictureSet* rps = pSlice->getRPS();
                parseShortTermRefPicSet(sps, rps, sps->getRPSList()->getNumberOfReferencePictureSets());

                sliceHdr->wNumBitsForShortTermRPSInSlice = (Ipp32s)(BitsDecoded() - bitsDecoded);
            }
            else // reference to ST ref pic set in PPS
            {
                Ipp32s numBits = 0;
                while ((Ipp32u)(1 << numBits) < sps->getRPSList()->getNumberOfReferencePictureSets())
                    numBits++;

                Ipp32u short_term_ref_pic_set_idx = numBits > 0 ? GetBits(numBits) : 0;
                if (short_term_ref_pic_set_idx >= sps->getRPSList()->getNumberOfReferencePictureSets())
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                *pSlice->getRPS() = *sps->getRPSList()->getReferencePictureSet(short_term_ref_pic_set_idx);

                sliceHdr->wNumBitsForShortTermRPSInSlice = 0;
            }

            if (sps->long_term_ref_pics_present_flag)
            {
                ReferencePictureSet* rps = pSlice->getRPS();
                int offset = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
                if (sps->num_long_term_ref_pics_sps > 0)
                {
                    rps->num_long_term_sps = GetVLCElementU();
                    if (rps->num_long_term_sps > sps->num_long_term_ref_pics_sps)
                        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
                    rps->setNumberOfLongtermPictures(rps->num_long_term_sps);
                }

                rps->num_long_term_pics = GetVLCElementU();
                rps->setNumberOfLongtermPictures(rps->num_long_term_sps + rps->num_long_term_pics);

                if (offset + rps->num_long_term_sps + rps->num_long_term_pics > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1])
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                for(Ipp32u j = offset, k = 0; k < rps->num_long_term_sps + rps->num_long_term_pics; j++, k++)
                {
                    int pocLsbLt;
                    if (k < rps->num_long_term_sps)
                    {
                        Ipp32u lt_idx_sps = 0;
                        if (sps->num_long_term_ref_pics_sps > 1)
                        {
                            Ipp32u bitsForLtrpInSPS = 0;
                            while (sps->num_long_term_ref_pics_sps > (Ipp32u)(1 << bitsForLtrpInSPS))
                                bitsForLtrpInSPS++;

                            lt_idx_sps = GetBits(bitsForLtrpInSPS);
                        }

                        pocLsbLt = sps->lt_ref_pic_poc_lsb_sps[lt_idx_sps];   
                        rps->used_by_curr_pic_flag[j] = sps->used_by_curr_pic_lt_sps_flag[lt_idx_sps];
                    }
                    else
                    {
                        Ipp32u poc_lsb_lt = GetBits(sps->log2_max_pic_order_cnt_lsb);
                        pocLsbLt = poc_lsb_lt;
                        rps->used_by_curr_pic_flag[j] = Get1Bit();
                    }

                    rps->poc_lbs_lt[j] = pocLsbLt;

                    rps->delta_poc_msb_present_flag[j] = Get1Bit();
                    if (rps->delta_poc_msb_present_flag[j])
                    {
                        rps->delta_poc_msb_cycle_lt[j] = (Ipp8u)GetVLCElementU();
                    }
                }
                offset += rps->getNumberOfLongtermPictures();
                rps->num_pics = offset;
            }

            if ( sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_LP
                || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_W_RADL
                || sliceHdr->nal_unit_type == NAL_UT_CODED_SLICE_BLA_N_LP )
            {
                ReferencePictureSet* rps = pSlice->getRPS();
                rps->num_negative_pics = 0;
                rps->num_positive_pics = 0;
                rps->setNumberOfLongtermPictures(0);
                rps->num_pics = 0;
            }

            if (sps->sps_temporal_mvp_enabled_flag)
            {
                sliceHdr->slice_temporal_mvp_enabled_flag = Get1Bit();
            }
        }

        if (sps->sample_adaptive_offset_enabled_flag)
        {
            sliceHdr->slice_sao_luma_flag = Get1Bit();
            if (sps->ChromaArrayType)
                sliceHdr->slice_sao_chroma_flag = Get1Bit();
        }

        if (sliceHdr->slice_type != I_SLICE)
        {
            Ipp8u num_ref_idx_active_override_flag = Get1Bit();
            if (num_ref_idx_active_override_flag)
            {
                sliceHdr->m_numRefIdx[REF_PIC_LIST_0] = GetVLCElementU() + 1;
                if (sliceHdr->slice_type == B_SLICE)
                {
                    sliceHdr->m_numRefIdx[REF_PIC_LIST_1] = GetVLCElementU() + 1;
                }

                if (sliceHdr->m_numRefIdx[REF_PIC_LIST_0] > 15 || sliceHdr->m_numRefIdx[REF_PIC_LIST_1] > 15)
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            else
            {
                sliceHdr->m_numRefIdx[REF_PIC_LIST_0] = pps->num_ref_idx_l0_default_active;
                if (sliceHdr->slice_type == B_SLICE)
                {
                    sliceHdr->m_numRefIdx[REF_PIC_LIST_1] = pps->num_ref_idx_l1_default_active;
                }
            }
        }

        RefPicListModification* refPicListModification = &sliceHdr->m_RefPicListModification;
        if(sliceHdr->slice_type != I_SLICE)
        {
            if (!pps->lists_modification_present_flag || pSlice->getNumRpsCurrTempList() <= 1 )
            {
                refPicListModification->ref_pic_list_modification_flag_l0 = 0;
            }
            else
            {
                refPicListModification->ref_pic_list_modification_flag_l0 = Get1Bit();
            }

            if(refPicListModification->ref_pic_list_modification_flag_l0)
            {
                int i = 0;
                int numRpsCurrTempList0 = pSlice->getNumRpsCurrTempList();
                if ( numRpsCurrTempList0 > 1 )
                {
                    int length = 1;
                    numRpsCurrTempList0 --;
                    while ( numRpsCurrTempList0 >>= 1)
                    {
                        length ++;
                    }
                    for (i = 0; i < pSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
                    {
                        refPicListModification->list_entry_l0[i] = GetBits(length);
                    }
                }
                else
                {
                    for (i = 0; i < pSlice->getNumRefIdx(REF_PIC_LIST_0); i ++)
                    {
                        refPicListModification->list_entry_l0[i] = 0;
                    }
                }
            }
        }
        else
        {
            refPicListModification->ref_pic_list_modification_flag_l0 = 0;
        }

        if (sliceHdr->slice_type == B_SLICE)
        {
            if( !pps->lists_modification_present_flag || pSlice->getNumRpsCurrTempList() <= 1 )
            {
                refPicListModification->ref_pic_list_modification_flag_l1 = 0;
            }
            else
            {
                refPicListModification->ref_pic_list_modification_flag_l1 = Get1Bit();
            }

            if(refPicListModification->ref_pic_list_modification_flag_l1)
            {
                int i = 0;
                int numRpsCurrTempList1 = pSlice->getNumRpsCurrTempList();
                if ( numRpsCurrTempList1 > 1 )
                {
                    int length = 1;
                    numRpsCurrTempList1 --;
                    while ( numRpsCurrTempList1 >>= 1)
                    {
                        length ++;
                    }
                    for (i = 0; i < pSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
                    {
                        refPicListModification->list_entry_l1[i] = GetBits(length);
                    }
                }
                else
                {
                    for (i = 0; i < pSlice->getNumRefIdx(REF_PIC_LIST_1); i ++)
                    {
                        refPicListModification->list_entry_l1[i] = 0;
                    }
                }
            }
        }
        else
        {
            refPicListModification->ref_pic_list_modification_flag_l1 = 0;
        }

        if (sliceHdr->slice_type == B_SLICE)
        {
            sliceHdr->mvd_l1_zero_flag = Get1Bit();
        }

        sliceHdr->cabac_init_flag = false; // default
        if(pps->cabac_init_present_flag && sliceHdr->slice_type != I_SLICE)
        {
            sliceHdr->cabac_init_flag = Get1Bit();
        }

        if (sliceHdr->slice_temporal_mvp_enabled_flag)
        {
            if ( sliceHdr->slice_type == B_SLICE )
            {
                sliceHdr->collocated_from_l0_flag = Get1Bit();
            }
            else
            {
                sliceHdr->collocated_from_l0_flag = 1;
            }

            if (sliceHdr->slice_type != I_SLICE &&
                ((sliceHdr->collocated_from_l0_flag==1 && pSlice->getNumRefIdx(REF_PIC_LIST_0)>1)||
                (sliceHdr->collocated_from_l0_flag ==0 && pSlice->getNumRefIdx(REF_PIC_LIST_1)>1)))
            {
                sliceHdr->collocated_ref_idx = GetVLCElementU();
                if (sliceHdr->collocated_ref_idx >= (Ipp32u)sliceHdr->m_numRefIdx[sliceHdr->collocated_from_l0_flag ? REF_PIC_LIST_0 : REF_PIC_LIST_1])
                    throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
            }
            else
            {
                sliceHdr->collocated_ref_idx = 0;
            }
        }

        if ( (pps->weighted_pred_flag && sliceHdr->slice_type == P_SLICE) || (pps->weighted_bipred_flag && sliceHdr->slice_type == B_SLICE) )
        {
            xParsePredWeightTable(sps, sliceHdr);
        }

        if (sliceHdr->slice_type != I_SLICE)
        {
            sliceHdr->max_num_merge_cand = MERGE_MAX_NUM_CAND - GetVLCElementU();
            if (sliceHdr->max_num_merge_cand < 1 || sliceHdr->max_num_merge_cand > 5)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        sliceHdr->SliceQP = pps->init_qp + GetVLCElementS();

        if (sliceHdr->SliceQP < -sps->getQpBDOffsetY() || sliceHdr->SliceQP >  51)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pps->pps_slice_chroma_qp_offsets_present_flag)
        {
            sliceHdr->slice_cb_qp_offset =  GetVLCElementS();
            if (sliceHdr->slice_cb_qp_offset < -12 || sliceHdr->slice_cb_qp_offset >  12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            if (pps->pps_cb_qp_offset + sliceHdr->slice_cb_qp_offset < -12 || pps->pps_cb_qp_offset + sliceHdr->slice_cb_qp_offset >  12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            sliceHdr->slice_cr_qp_offset = GetVLCElementS();
            if (sliceHdr->slice_cr_qp_offset < -12 || sliceHdr->slice_cr_qp_offset >  12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

            if (pps->pps_cr_qp_offset + sliceHdr->slice_cr_qp_offset < -12 || pps->pps_cr_qp_offset + sliceHdr->slice_cr_qp_offset >  12)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        if (pps->chroma_qp_offset_list_enabled_flag)
        {
            sliceHdr->cu_chroma_qp_offset_enabled_flag = Get1Bit();
        }

        if (pps->deblocking_filter_control_present_flag)
        {
            if (pps->deblocking_filter_override_enabled_flag)
            {
                sliceHdr->deblocking_filter_override_flag = Get1Bit();
            }
            else
            {
                sliceHdr->deblocking_filter_override_flag = 0;
            }
            if (sliceHdr->deblocking_filter_override_flag)
            {
                sliceHdr->slice_deblocking_filter_disabled_flag = Get1Bit();
                if(!sliceHdr->slice_deblocking_filter_disabled_flag)
                {
                    sliceHdr->slice_beta_offset =  GetVLCElementS() << 1;
                    sliceHdr->slice_tc_offset = GetVLCElementS() << 1;

                    if (sliceHdr->slice_beta_offset < -12 || sliceHdr->slice_beta_offset > 12)
                        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

                    if (sliceHdr->slice_tc_offset < -12 || sliceHdr->slice_tc_offset > 12)
                        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
                }
            }
            else
            {
                sliceHdr->slice_deblocking_filter_disabled_flag =  pps->pps_deblocking_filter_disabled_flag;
                sliceHdr->slice_beta_offset = pps->pps_beta_offset;
                sliceHdr->slice_tc_offset = pps->pps_tc_offset;
            }
        }
        else
        {
            sliceHdr->slice_deblocking_filter_disabled_flag = 0;
            sliceHdr->slice_beta_offset = 0;
            sliceHdr->slice_tc_offset = 0;
        }

        bool isSAOEnabled = sliceHdr->slice_sao_luma_flag || sliceHdr->slice_sao_chroma_flag;
        bool isDBFEnabled = !sliceHdr->slice_deblocking_filter_disabled_flag;

        if (pps->pps_loop_filter_across_slices_enabled_flag && ( isSAOEnabled || isDBFEnabled ))
        {
            sliceHdr->slice_loop_filter_across_slices_enabled_flag = Get1Bit();
        }
        else
        {
            sliceHdr->slice_loop_filter_across_slices_enabled_flag = pps->pps_loop_filter_across_slices_enabled_flag;
        }
    }

    if (!pps->tiles_enabled_flag)
    {
        pSlice->allocateTileLocation(1);
        pSlice->m_tileByteLocation[0] = 0;
    }

    if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag)
    {
        Ipp32u *entryPointOffset  = 0;
        int offsetLenMinus1 = 0;

        sliceHdr->num_entry_point_offsets = GetVLCElementU();

        Ipp32u PicHeightInCtbsY = sps->HeightInCU;

        if (!pps->tiles_enabled_flag && pps->entropy_coding_sync_enabled_flag && sliceHdr->num_entry_point_offsets > PicHeightInCtbsY)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pps->tiles_enabled_flag && !pps->entropy_coding_sync_enabled_flag && sliceHdr->num_entry_point_offsets > pps->num_tile_columns*pps->num_tile_rows)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (pps->tiles_enabled_flag && pps->entropy_coding_sync_enabled_flag && sliceHdr->num_entry_point_offsets > pps->num_tile_columns*PicHeightInCtbsY)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        if (sliceHdr->num_entry_point_offsets > 0)
        {
            offsetLenMinus1 = GetVLCElementU();
            if (offsetLenMinus1 > 31)
                throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
        }

        entryPointOffset = new Ipp32u[sliceHdr->num_entry_point_offsets];
        for (Ipp32u idx = 0; idx < sliceHdr->num_entry_point_offsets; idx++)
        {
            entryPointOffset[idx] = GetBits(offsetLenMinus1 + 1) + 1;
        }

        if (pps->tiles_enabled_flag)
        {
            pSlice->allocateTileLocation(sliceHdr->num_entry_point_offsets + 1);

            unsigned prevPos = 0;
            pSlice->m_tileByteLocation[0] = 0;
            for (int idx=1; idx < pSlice->getTileLocationCount() ; idx++)
            {
                pSlice->m_tileByteLocation[idx] = prevPos + entryPointOffset[idx - 1];
                prevPos += entryPointOffset[ idx - 1 ];
            }
        }
        else if (pps->entropy_coding_sync_enabled_flag)
        {
            // we don't use wpp offsets
        }

        if (entryPointOffset)
        {
            delete[] entryPointOffset;
        }
    }
    else
    {
        sliceHdr->num_entry_point_offsets = 0;
    }

    if(pps->slice_segment_header_extension_present_flag)
    {
        Ipp32u slice_header_extension_length = GetVLCElementU();

        if (slice_header_extension_length > 256)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        for (Ipp32u i = 0; i < slice_header_extension_length; i++)
        {
            GetBits(8); // slice_header_extension_data_byte
        }
    }

    readOutTrailingBits();
    return;
}

// Parse full slice header
UMC::Status H265HeadersBitstream::GetSliceHeaderFull(H265Slice *rpcSlice, const H265SeqParamSet *sps, const H265PicParamSet *pps)
{
    UMC::Status sts = GetSliceHeaderPart1(rpcSlice->GetSliceHeader());
    if (UMC::UMC_OK != sts)
        return sts;
    decodeSlice(rpcSlice, sps, pps);

    if (CheckBSLeft())
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);
    return UMC::UMC_OK;
}

// Return bitstream array base address and size
void H265Bitstream::GetOrg(Ipp32u **pbs, Ipp32u *size)
{
    *pbs       = m_pbsBase;
    *size      = m_maxBsSize;
}

// Return current bitstream address and bit offset
void H265Bitstream::GetState(Ipp32u** pbs,Ipp32u* bitOffset)
{
    *pbs       = m_pbs;
    *bitOffset = m_bitOffset;

} // H265Bitstream::GetState()

// Set current bitstream address and bit offset
void H265Bitstream::SetState(Ipp32u* pbs, Ipp32u bitOffset)
{
    m_pbs = pbs;
    m_bitOffset = bitOffset;

} // H265Bitstream::GetState()

H265Bitstream::H265Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
     : H265HeadersBitstream(pb, maxsize)
{
} // H265Bitstream::H265Bitstream(Ipp8u * const pb,

H265Bitstream::H265Bitstream()
    : H265HeadersBitstream()
{
#if INSTRUMENTED_CABAC
    if (!cabac_bits)
    {
        cabac_bits=fopen(__CABAC_FILE__,"w+t");
        m_c = 0;
    }
#endif
} // H265Bitstream::H265Bitstream(void)

H265Bitstream::~H265Bitstream()
{
#if INSTRUMENTED_CABAC
    fflush(cabac_bits);
#endif
} // H265Bitstream::~H265Bitstream()

// Read and return NAL unit type and NAL storage idc.
// Bitstream position is expected to be at the start of a NAL unit.
UMC::Status H265Bitstream::GetNALUnitType(NalUnitType &nal_unit_type, Ipp32u &nuh_temporal_id)
{
    Ipp32u forbidden_zero_bit = Get1Bit();
    if (forbidden_zero_bit)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    nal_unit_type = (NalUnitType)GetBits(6);
    Ipp32u nuh_layer_id = GetBits(6);
    if (nuh_layer_id)
        throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

    // nuh_temporal_id
    nuh_temporal_id = GetBits(3) - 1;

    if (nuh_temporal_id)
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_RADL
            && nal_unit_type != NAL_UT_CODED_SLICE_BLA_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR_W_RADL
            && nal_unit_type != NAL_UT_CODED_SLICE_IDR_N_LP
            && nal_unit_type != NAL_UT_CODED_SLICE_CRA
            && nal_unit_type != NAL_UT_VPS
            && nal_unit_type != NAL_UT_SPS
            && nal_unit_type != NAL_UT_EOS
            && nal_unit_type != NAL_UT_EOB );
    }
    else
    {
        VM_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_TLA_R
            && nal_unit_type != NAL_UT_CODED_SLICE_TSA_N
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_R
            && nal_unit_type != NAL_UT_CODED_SLICE_STSA_N );
    }

    return UMC::UMC_OK;
}

// Read optional access unit delimiter from bitstream.
UMC::Status H265Bitstream::GetAccessUnitDelimiter(Ipp32u &PicCodType)
{
    PicCodType = GetBits(3);
    return UMC::UMC_OK;
}    // GetAccessUnitDelimiter

// Set current decoding position
void H265Bitstream::SetDecodedBytes(size_t nBytes)
{
    m_pbs = m_pbsBase + (nBytes / 4);
    m_bitOffset = 31 - ((Ipp32s) ((nBytes % sizeof(Ipp32u)) * 8));
} // void H265Bitstream::SetDecodedBytes(size_t nBytes)

// Parse RPS part in SPS or slice header
void H265Bitstream::parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* rps, Ipp32u idx)
{
    if (idx > 0)
    {
        rps->inter_ref_pic_set_prediction_flag = Get1Bit();
    }
    else
        rps->inter_ref_pic_set_prediction_flag = 0;

    if (rps->inter_ref_pic_set_prediction_flag)
    {
        Ipp32u delta_idx_minus1 = 0;

        if (idx == sps->getRPSList()->getNumberOfReferencePictureSets())
        {
            delta_idx_minus1 = GetVLCElementU();
        }

        if (delta_idx_minus1 > idx - 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        Ipp32u rIdx = idx - 1 - delta_idx_minus1;

        if (rIdx > idx - 1)
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        ReferencePictureSet*   rpsRef = const_cast<H265SeqParamSet *>(sps)->getRPSList()->getReferencePictureSet(rIdx);
        int k = 0, k0 = 0, k1 = 0;
        Ipp32u delta_rps_sign = Get1Bit();
        Ipp32u abs_delta_rps_minus1 = GetVLCElementU();

        int deltaRPS = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
        Ipp32u num_pics = rpsRef->getNumberOfPictures();
        for(Ipp32u j = 0 ;j <= num_pics; j++)
        {
            Ipp32u used_by_curr_pic_flag = Get1Bit();
            Ipp32s refIdc = used_by_curr_pic_flag;
            if (refIdc == 0)
            {
                Ipp32u use_delta_flag = Get1Bit();
                refIdc = use_delta_flag << 1;
            }

            if (refIdc == 1 || refIdc == 2)
            {
                int deltaPOC = deltaRPS + ((j < rpsRef->getNumberOfPictures())? rpsRef->getDeltaPOC(j) : 0);
                rps->setDeltaPOC(k, deltaPOC);
                rps->used_by_curr_pic_flag[k] = refIdc == 1;

                if (deltaPOC < 0)
                {
                    k0++;
                }
                else
                {
                    k1++;
                }
                k++;
            }
        }
        rps->num_pics = k;
        rps->num_negative_pics = k0;
        rps->num_positive_pics = k1;
        rps->sortDeltaPOC();
    }
    else
    {
        rps->num_negative_pics = GetVLCElementU();
        rps->num_positive_pics = GetVLCElementU();

        if (((Ipp32u)rps->getNumberOfNegativePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1]) ||
            ((Ipp32u)rps->getNumberOfPositivePictures() > sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers - 1] - (Ipp32u)rps->getNumberOfNegativePictures()))
            throw h265_exception(UMC::UMC_ERR_INVALID_STREAM);

        Ipp32s prev = 0;
        Ipp32s poc;
        for(Ipp32u j=0 ; j < rps->getNumberOfNegativePictures(); j++)
        {
            Ipp32u delta_poc_s0_minus1 = GetVLCElementU();
            poc = prev - delta_poc_s0_minus1 - 1;
            prev = poc;
            rps->setDeltaPOC(j,poc);
            rps->used_by_curr_pic_flag[j] = Get1Bit();
        }

        prev = 0;
        for(Ipp32u j=rps->getNumberOfNegativePictures(); j < rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures(); j++)
        {
            Ipp32u delta_poc_s1_minus1 = GetVLCElementU();
            poc = prev + delta_poc_s1_minus1 + 1;
            prev = poc;
            rps->setDeltaPOC(j,poc);
            rps->used_by_curr_pic_flag[j] = Get1Bit();
        }
        rps->num_pics = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
