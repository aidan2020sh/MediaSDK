//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw_bs.h"
#include <assert.h>

namespace MfxHwH265Encode
{

BitstreamWriter::BitstreamWriter(mfxU8* bs, mfxU32 size, mfxU8 bitOffset)
    : m_bsStart(bs)
    , m_bsEnd(bs + size)
    , m_bs(bs)
    , m_bitOffset(bitOffset & 7)
    , m_bitStart(bitOffset & 7)
{
    assert(bitOffset < 8);
    *m_bs &= 0xFF << (8 - m_bitOffset);
}

BitstreamWriter::~BitstreamWriter()
{
}

void BitstreamWriter::Reset(mfxU8* bs, mfxU32 size, mfxU8 bitOffset)
{
    if (bs)
    {
        m_bsStart   = bs;
        m_bsEnd     = bs + size;
        m_bs        = bs;
        m_bitOffset = (bitOffset & 7);
        m_bitStart  = (bitOffset & 7);
    }
    else
    {
        m_bs        = m_bsStart;
        m_bitOffset = m_bitStart;
    }
}

void BitstreamWriter::PutBits(mfxU32 n, mfxU32 b)
{
    assert(n <= 24);

    b <<= (32 - n);

    if (!m_bitOffset)
    {
        m_bs[0] = (mfxU8)(b >> 24);
        m_bs[1] = (mfxU8)(b >> 16);
    }
    else
    {
        b >>= m_bitOffset;
        n  += m_bitOffset;
        
        m_bs[0] |= (mfxU8)(b >> 24);
        m_bs[1]  = (mfxU8)(b >> 16);
    }

    if (n > 16)
    {
        m_bs[2] = (mfxU8)(b >> 8);
        m_bs[3] = (mfxU8)b;
    }

    m_bs += (n >> 3);
    m_bitOffset = (n & 7);
}

void BitstreamWriter::PutBit(mfxU32 b)
{
    switch(m_bitOffset)
    {
    case 0: 
        m_bs[0] = (mfxU8)(b << 7);
        m_bitOffset = 1;
        break;
    case 7:
        m_bs[0] |= (mfxU8)(b & 1);
        m_bs ++;
        m_bitOffset = 0;
        break;
    default:
        if (b & 1)
            m_bs[0] |= (mfxU8)(1 << (7 - m_bitOffset));
        m_bitOffset ++;
        break;
    }
}


void BitstreamWriter::PutGolomb(mfxU32 b)
{
    if (!b)
    {
        PutBit(1);
    }
    else
    {
        mfxU32 n = 1;

        b ++;

        while (b >> n)
            n ++;

        PutBits(n - 1, 0);
        PutBits(n, b);
    }
}

void BitstreamWriter::PutTrailingBits()
{
    PutBit(1);

    if (m_bitOffset)
    {
        *(++m_bs)   = 0;
        m_bitOffset = 0;
    }
}

mfxStatus HeaderPacker::PackRBSP(mfxU8* dst, mfxU8* rbsp, mfxU32& dst_size, mfxU32 rbsp_size)
{
    mfxU32 rest_bytes = dst_size - rbsp_size;
    mfxU8* rbsp_end = rbsp + rbsp_size;

    if(dst_size < rbsp_size)
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    //skip start-code
    if (rbsp_size > 3)
    {
        dst[0] = rbsp[0];
        dst[1] = rbsp[1];
        dst[2] = rbsp[2];
        dst  += 3;
        rbsp += 3;
    }

    while (rbsp_end - rbsp > 2)
    {
        *dst++ = *rbsp++;

        if ( !(*(rbsp-1) || *rbsp || (*(rbsp+1) & 0xFC)) )
        {
            if (!--rest_bytes)
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            *dst++ = *rbsp++;
            *dst++ = 0x03;
        }
    }

    while (rbsp_end > rbsp)
        *dst++ = *rbsp++;

    dst_size -= rest_bytes;

    return MFX_ERR_NONE;
}

void HeaderPacker::PackNALU(BitstreamWriter& bs, NALU const & h)
{
    if (   h.nal_unit_type == VPS_NUT 
        || h.nal_unit_type == SPS_NUT
        || h.nal_unit_type == PPS_NUT)
    {
        bs.PutBits(8, 0); //zero_byte
    }
    
    bs.PutBits( 24, 0x000001);//start_code
    
    bs.PutBit(h.forbidden_zero_bit);
    bs.PutBits(6, h.nal_unit_type);
    bs.PutBits(6, h.nuh_layer_id);
    bs.PutBits(3, h.nuh_temporal_id_plus1);
}

void HeaderPacker::PackPTL(BitstreamWriter& bs, LayersInfo const & ptl, mfxU16 max_sub_layers_minus1)
{
    bs.PutBits(2, ptl.general.profile_space);
    bs.PutBit(ptl.general.tier_flag);
    bs.PutBits(5, ptl.general.profile_idc);
    bs.PutBits(24,(ptl.general.profile_compatibility_flags >> 8));
    bs.PutBits(8 ,(ptl.general.profile_compatibility_flags & 0xFF));
    bs.PutBit(ptl.general.progressive_source_flag);
    bs.PutBit(ptl.general.interlaced_source_flag);
    bs.PutBit(ptl.general.non_packed_constraint_flag);
    bs.PutBit(ptl.general.frame_only_constraint_flag);
    bs.PutBits(24, 0); //general_reserved_zero_44bits
    bs.PutBits(20, 0); //general_reserved_zero_44bits
    bs.PutBits(8, ptl.general.level_idc);

    for (mfxU32 i = 0; i < max_sub_layers_minus1; i++ )
    {
        bs.PutBit(ptl.sub_layer[i].profile_present_flag);
        bs.PutBit(ptl.sub_layer[i].level_present_flag);
    }

    if (max_sub_layers_minus1 > 0)
        for (mfxU32 i = max_sub_layers_minus1; i < 8; i++)
            bs.PutBits(2, 0); // reserved_zero_2bits[ i ]

    for (mfxU32 i = 0; i < max_sub_layers_minus1; i++)
    {
        if (ptl.sub_layer[i].profile_present_flag)
        {
            bs.PutBits(2,  ptl.sub_layer[i].profile_space);
            bs.PutBit(ptl.sub_layer[i].tier_flag);
            bs.PutBits(5,  ptl.sub_layer[i].profile_idc);
            bs.PutBits(24,(ptl.sub_layer[i].profile_compatibility_flags >> 8));
            bs.PutBits(8 ,(ptl.sub_layer[i].profile_compatibility_flags & 0xFF));
            bs.PutBit(ptl.sub_layer[i].progressive_source_flag);
            bs.PutBit(ptl.sub_layer[i].interlaced_source_flag);
            bs.PutBit(ptl.sub_layer[i].non_packed_constraint_flag);
            bs.PutBit(ptl.sub_layer[i].frame_only_constraint_flag);
            bs.PutBits(24, 0); //general_reserved_zero_44bits
            bs.PutBits(20, 0); //general_reserved_zero_44bits
        }

        if (ptl.sub_layer[i].level_present_flag)
            bs.PutBits(8, ptl.sub_layer[i].level_idc);
    }
}

void HeaderPacker::PackSLO(BitstreamWriter& bs, LayersInfo const & slo, mfxU16 max_sub_layers_minus1)
{
    bs.PutBit(slo.sub_layer_ordering_info_present_flag);

    for (mfxU32 i = ( slo.sub_layer_ordering_info_present_flag ? 0 : max_sub_layers_minus1 );
            i  <=  max_sub_layers_minus1; i++ )
    {
        bs.PutUE(slo.sub_layer[i].max_dec_pic_buffering_minus1);
        bs.PutUE(slo.sub_layer[i].max_num_reorder_pics);
        bs.PutUE(slo.sub_layer[i].max_latency_increase_plus1);
    }
}

void HeaderPacker::PackVPS(BitstreamWriter& bs, VPS const &  vps)
{
    NALU nalu = {0, VPS_NUT, 0, 1};

    PackNALU(bs, nalu);

    bs.PutBits(4, vps.video_parameter_set_id);
    bs.PutBits(2, vps.reserved_three_2bits);
    bs.PutBits(6, vps.max_layers_minus1);
    bs.PutBits(3, vps.max_sub_layers_minus1);
    bs.PutBit(vps.temporal_id_nesting_flag);
    bs.PutBits(16, vps.reserved_0xffff_16bits);
    
    PackPTL(bs, vps, vps.max_sub_layers_minus1);
    PackSLO(bs, vps, vps.max_sub_layers_minus1);

    bs.PutBits(6, vps.max_layer_id);
    bs.PutUE(vps.num_layer_sets_minus1);

    assert(0 == vps.num_layer_sets_minus1);

    bs.PutBit(vps.timing_info_present_flag);

    if (vps.timing_info_present_flag)
    {
        bs.PutBits(24, (vps.num_units_in_tick >> 8));
        bs.PutBits( 8, (vps.num_units_in_tick & 0xFF));
        bs.PutBits(24, (vps.time_scale >> 8));
        bs.PutBits( 8, (vps.time_scale & 0xFF));

        bs.PutBit(vps.poc_proportional_to_timing_flag);

        if (vps.poc_proportional_to_timing_flag)
            bs.PutUE(vps.num_ticks_poc_diff_one_minus1);

        bs.PutUE(vps.num_hrd_parameters);
        
        assert(0 == vps.num_hrd_parameters);
    }

    bs.PutBit(0); //vps.extension_flag
    bs.PutTrailingBits();
}

void HeaderPacker::PackSPS(BitstreamWriter& bs, SPS const & sps)
{
    NALU nalu = {0, SPS_NUT, 0, 1};

    PackNALU(bs, nalu);

    bs.PutBits(4, sps.video_parameter_set_id);
    bs.PutBits(3, sps.max_sub_layers_minus1);
    bs.PutBit(sps.temporal_id_nesting_flag);

    PackPTL(bs, sps, sps.max_sub_layers_minus1);

    bs.PutUE(sps.seq_parameter_set_id);
    bs.PutUE(sps.chroma_format_idc);

    if(sps.chroma_format_idc  ==  3)
        bs.PutBit(sps.separate_colour_plane_flag);

    bs.PutUE(sps.pic_width_in_luma_samples);
    bs.PutUE(sps.pic_height_in_luma_samples);
    bs.PutBit(sps.conformance_window_flag);

    if (sps.conformance_window_flag)
    {
        bs.PutUE(sps.conf_win_left_offset);
        bs.PutUE(sps.conf_win_right_offset);
        bs.PutUE(sps.conf_win_top_offset);
        bs.PutUE(sps.conf_win_bottom_offset);
    }

    bs.PutUE(sps.bit_depth_luma_minus8);
    bs.PutUE(sps.bit_depth_chroma_minus8);
    bs.PutUE(sps.log2_max_pic_order_cnt_lsb_minus4);

    PackSLO(bs, sps, sps.max_sub_layers_minus1);

    bs.PutUE(sps.log2_min_luma_coding_block_size_minus3);
    bs.PutUE(sps.log2_diff_max_min_luma_coding_block_size);
    bs.PutUE(sps.log2_min_transform_block_size_minus2);
    bs.PutUE(sps.log2_diff_max_min_transform_block_size);
    bs.PutUE(sps.max_transform_hierarchy_depth_inter);
    bs.PutUE(sps.max_transform_hierarchy_depth_intra);
    bs.PutBit(sps.scaling_list_enabled_flag);

    assert(0 == sps.scaling_list_enabled_flag);

    bs.PutBit(sps.amp_enabled_flag);
    bs.PutBit(sps.sample_adaptive_offset_enabled_flag);
    bs.PutBit(sps.pcm_enabled_flag);

    if (sps.pcm_enabled_flag)
    {
        bs.PutBits(4, sps.pcm_sample_bit_depth_luma_minus1);
        bs.PutBits(4, sps.pcm_sample_bit_depth_chroma_minus1);
        bs.PutUE(sps.log2_min_pcm_luma_coding_block_size_minus3);
        bs.PutUE(sps.log2_diff_max_min_pcm_luma_coding_block_size);
        bs.PutBit(sps.pcm_loop_filter_disabled_flag);
    }

    bs.PutUE(sps.num_short_term_ref_pic_sets);

    for (mfxU32 i = 0; i < sps.num_short_term_ref_pic_sets; i++)
        PackSTRPS(bs, sps.strps, sps.num_short_term_ref_pic_sets, i);

    
    bs.PutBit(sps.long_term_ref_pics_present_flag);

    if (sps.long_term_ref_pics_present_flag)
    {
        bs.PutUE(sps.num_long_term_ref_pics_sps);

        for (mfxU32 i = 0; i < sps.num_long_term_ref_pics_sps; i++)
        {
            bs.PutBits(sps.log2_max_pic_order_cnt_lsb_minus4, sps.lt_ref_pic_poc_lsb_sps[i] );
            bs.PutBit(sps.used_by_curr_pic_lt_sps_flag[i] );
        }
    }

    bs.PutBit(sps.temporal_mvp_enabled_flag);
    bs.PutBit(sps.strong_intra_smoothing_enabled_flag);

    bs.PutBit(sps.vui_parameters_present_flag);

    assert(0 == sps.vui_parameters_present_flag);
    
    bs.PutBit(0); //sps.extension_flag

    bs.PutTrailingBits();
}

void HeaderPacker::PackPPS(BitstreamWriter& bs, PPS const &  pps)
{
    NALU nalu = {0, PPS_NUT, 0, 1};

    PackNALU(bs, nalu);

    bs.PutUE(pps.pic_parameter_set_id);
    bs.PutUE(pps.seq_parameter_set_id);
    bs.PutBit(pps.dependent_slice_segments_enabled_flag);
    bs.PutBit(pps.output_flag_present_flag);
    bs.PutBits(3, pps.num_extra_slice_header_bits);
    bs.PutBit(pps.sign_data_hiding_enabled_flag);
    bs.PutBit(pps.cabac_init_present_flag);
    bs.PutUE(pps.num_ref_idx_l0_default_active_minus1);
    bs.PutUE(pps.num_ref_idx_l1_default_active_minus1);
    bs.PutSE(pps.init_qp_minus26);
    bs.PutBit(pps.constrained_intra_pred_flag);
    bs.PutBit(pps.transform_skip_enabled_flag);
    bs.PutBit(pps.cu_qp_delta_enabled_flag);

    if (pps.cu_qp_delta_enabled_flag)
        bs.PutUE(pps.diff_cu_qp_delta_depth);
    
    bs.PutSE(pps.cb_qp_offset);
    bs.PutSE(pps.cr_qp_offset);
    bs.PutBit(pps.slice_chroma_qp_offsets_present_flag);
    bs.PutBit(pps.weighted_pred_flag);
    bs.PutBit(pps.weighted_bipred_flag);
    bs.PutBit(pps.transquant_bypass_enabled_flag);
    bs.PutBit(pps.tiles_enabled_flag);
    bs.PutBit(pps.entropy_coding_sync_enabled_flag);

    if (pps.tiles_enabled_flag )
    {
        bs.PutUE(pps.num_tile_columns_minus1);
        bs.PutUE(pps.num_tile_rows_minus1);
        bs.PutBit(pps.uniform_spacing_flag);

        if(!pps.uniform_spacing_flag)
        {
            for (mfxU32 i = 0; i < pps.num_tile_columns_minus1; i++)
                bs.PutUE(pps.column_width_minus1[i]);

            for (mfxU32 i = 0; i < pps.num_tile_rows_minus1; i++)
                bs.PutUE(pps.row_height_minus1[i]);
        }

        bs.PutBit(pps.loop_filter_across_tiles_enabled_flag);
    }

    bs.PutBit(pps.loop_filter_across_slices_enabled_flag);
    bs.PutBit(pps.deblocking_filter_control_present_flag);

    if(pps.deblocking_filter_control_present_flag)
    {
        bs.PutBit(pps.deblocking_filter_override_enabled_flag);
        bs.PutBit(pps.deblocking_filter_disabled_flag);

        if(!pps.deblocking_filter_disabled_flag)
        {
            bs.PutSE(pps.beta_offset_div2);
            bs.PutSE(pps.tc_offset_div2);
        }
    }

    bs.PutBit(pps.scaling_list_data_present_flag);
    assert(0 == pps.scaling_list_data_present_flag);

    bs.PutBit(pps.lists_modification_present_flag);
    bs.PutUE(pps.log2_parallel_merge_level_minus2);
    bs.PutBit(pps.slice_segment_header_extension_present_flag);
    bs.PutBit(0); //pps.extension_flag 
    
    bs.PutTrailingBits();
}

void HeaderPacker::PackSSH(
    BitstreamWriter& bs, 
    NALU    const & nalu,
    SPS     const & sps, 
    PPS     const & pps, 
    Slice   const & slice)
{
    const mfxU8 B = 0, P = 1/*, I = 2*/;
    mfxU32 MaxCU = (1<<(sps.log2_min_luma_coding_block_size_minus3+3 + sps.log2_diff_max_min_luma_coding_block_size));
    mfxU32 PicSizeInCtbsY = ((sps.pic_width_in_luma_samples + MaxCU - 1) / MaxCU) * ((sps.pic_height_in_luma_samples + MaxCU - 1) / MaxCU);

    PackNALU(bs, nalu);

    bs.PutBit(slice.first_slice_segment_in_pic_flag);

    if (nalu.nal_unit_type >=  BLA_W_LP  && nalu.nal_unit_type <=  RSV_IRAP_VCL23)
        bs.PutBit(slice.no_output_of_prior_pics_flag);

    bs.PutUE(slice.pic_parameter_set_id);

    if (!slice.first_slice_segment_in_pic_flag)
    {
        if (pps.dependent_slice_segments_enabled_flag)
            bs.PutBit(slice.dependent_slice_segment_flag);

        bs.PutBits(CeilLog2(PicSizeInCtbsY), slice.segment_address);
    }

    if( !slice.dependent_slice_segment_flag )
    {
        if(pps.num_extra_slice_header_bits)
            bs.PutBits(pps.num_extra_slice_header_bits, slice.reserved_flags);

        bs.PutUE(slice.type);

        if (pps.output_flag_present_flag)
            bs.PutBit(slice.pic_output_flag);

        if (sps.separate_colour_plane_flag == 1)
            bs.PutBits(2, slice.colour_plane_id);

        if (nalu.nal_unit_type != IDR_W_RADL && nalu.nal_unit_type !=  IDR_N_LP)
        {
            bs.PutBits(sps.log2_max_pic_order_cnt_lsb_minus4+4, slice.pic_order_cnt_lsb);
            bs.PutBit(slice.short_term_ref_pic_set_sps_flag);

            if (!slice.short_term_ref_pic_set_sps_flag)
            {
                STRPS strps[65];
                Copy(strps, sps.strps);
                strps[sps.num_short_term_ref_pic_sets] = slice.strps;

                PackSTRPS(bs, strps, sps.num_short_term_ref_pic_sets, sps.num_short_term_ref_pic_sets);
            }
            else
            {
                if (sps.num_short_term_ref_pic_sets > 1)
                    bs.PutBits(CeilLog2(sps.num_short_term_ref_pic_sets), slice.short_term_ref_pic_set_idx);
            }

            //if (sps.long_term_ref_pics_present_flag)
            //{
            //}
            assert(0 == sps.long_term_ref_pics_present_flag);

            if (sps.temporal_mvp_enabled_flag)
                bs.PutBit(slice.temporal_mvp_enabled_flag);
        }

        if (sps.sample_adaptive_offset_enabled_flag)
        {
            bs.PutBit(slice.sao_luma_flag);
            bs.PutBit(slice.sao_chroma_flag);
        }

        if (slice.type == P || slice.type == B)
        {
            bs.PutBit(slice.num_ref_idx_active_override_flag);

            if (slice.num_ref_idx_active_override_flag)
            {
                bs.PutUE(slice.num_ref_idx_l0_active_minus1 );

                if (slice.type == B)
                    bs.PutUE(slice.num_ref_idx_l1_active_minus1 );
            }

            assert(0 == pps.lists_modification_present_flag);

            if (slice.type  ==  B)
                bs.PutBit(slice.mvd_l1_zero_flag );

            if (pps.cabac_init_present_flag)
                bs.PutBit(slice.cabac_init_flag);

            if (slice.temporal_mvp_enabled_flag)
            {
                //slice.collocated_from_l0_flag  = 1;

                if (slice.type == B)
                    bs.PutBit(1); //slice.collocated_from_l0_flag

                if ((  slice.collocated_from_l0_flag  &&  slice.num_ref_idx_l0_active_minus1 > 0 )  ||
                    ( !slice.collocated_from_l0_flag  &&  slice.num_ref_idx_l1_active_minus1 > 0 ) )
                    bs.PutUE(slice.collocated_ref_idx);
            }

            assert(0 == pps.weighted_pred_flag);
            assert(0 == pps.weighted_bipred_flag);


            bs.PutUE(slice.five_minus_max_num_merge_cand);
        }

        bs.PutSE(slice.slice_qp_delta);

        if (pps.slice_chroma_qp_offsets_present_flag)
        {
            bs.PutSE(slice.slice_cb_qp_offset);
            bs.PutSE(slice.slice_cr_qp_offset);
        }

        if (pps.deblocking_filter_override_enabled_flag)
            bs.PutBit(slice.deblocking_filter_override_flag);

        if (slice.deblocking_filter_override_flag)
        {
            bs.PutBit(slice.deblocking_filter_disabled_flag);

            if (!slice.deblocking_filter_disabled_flag)
            {
                bs.PutSE(slice.beta_offset_div2);
                bs.PutSE(slice.tc_offset_div2);
            }
        }

        if (   pps.loop_filter_across_slices_enabled_flag
            && (slice.sao_luma_flag
             || slice.sao_chroma_flag
             || !slice.deblocking_filter_disabled_flag))
        {
            bs.PutBit(slice.loop_filter_across_slices_enabled_flag);
        }
    }

    if (pps.tiles_enabled_flag || pps.entropy_coding_sync_enabled_flag)
    {
        bs.PutUE(slice.num_entry_point_offsets);

        if (slice.num_entry_point_offsets > 0)
        {
            bs.PutUE(slice.offset_len_minus1);

            for (mfxU32 i = 0; i < slice.num_entry_point_offsets; i++)
                bs.PutBits(slice.offset_len_minus1+1, slice.entry_point_offset_minus1[i]);
        }
    }

    assert(0 == pps.slice_segment_header_extension_present_flag);

    bs.PutTrailingBits();
}

void HeaderPacker::PackSTRPS(BitstreamWriter& bs, const STRPS* sets, mfxU32 num, mfxU32 idx)
{
    STRPS const & strps = sets[idx];

    if (idx != 0)
    {
        bs.PutBit(strps.inter_ref_pic_set_prediction_flag);
    }

    if (strps.inter_ref_pic_set_prediction_flag)
    {

        if(idx == num)
        {
            bs.PutUE(strps.delta_idx_minus1);
        }

        bs.PutBit(strps.delta_rps_sign);
        bs.PutUE(strps.abs_delta_rps_minus1);

        mfxU32 RefRpsIdx = idx - ( strps.delta_idx_minus1 + 1 );
        mfxU32 NumDeltaPocs = sets[RefRpsIdx].num_negative_pics + sets[RefRpsIdx].num_positive_pics;

        for (mfxU32 j = 0; j <=  NumDeltaPocs; j++)
        {
            bs.PutBit(strps.pic[j].used_by_curr_pic_flag);

            if (!strps.pic[j].used_by_curr_pic_flag)
            {
                bs.PutBit(strps.pic[j].use_delta_flag);
            }
        }
    }
    else
    {
        bs.PutUE(strps.num_negative_pics);
        bs.PutUE(strps.num_positive_pics);

        for (mfxU32 i = 0; i < strps.num_negative_pics; i++)
        {
            bs.PutUE(strps.pic[i].delta_poc_s0_minus1);
            bs.PutBit(strps.pic[i].used_by_curr_pic_s0_flag);
        }

        for(mfxU32 i = strps.num_negative_pics; i < mfxU32(strps.num_positive_pics + strps.num_negative_pics); i++)
        {
            bs.PutUE(strps.pic[i].delta_poc_s1_minus1);
            bs.PutBit(strps.pic[i].used_by_curr_pic_s1_flag);
        }
    }
}

HeaderPacker::HeaderPacker()
    : m_par(0)
{
}

HeaderPacker::~HeaderPacker() 
{
}

mfxStatus HeaderPacker::Reset(MfxVideoParam const & par)
{
    mfxStatus sts = MFX_ERR_NONE;
    BitstreamWriter rbsp(m_rbsp, sizeof(m_rbsp));

    m_sz_vps = sizeof(m_bs_vps);
    m_sz_sps = sizeof(m_bs_sps);
    m_sz_pps = sizeof(m_bs_pps);
    m_sz_ssh = sizeof(m_bs_ssh);

    PackVPS(rbsp, par.m_vps);
    sts = PackRBSP(m_bs_vps, m_rbsp, m_sz_vps, (rbsp.GetOffset() + 7) / 8);
    assert(!sts);
    if (sts)
        return sts;

    rbsp.Reset();
    
    PackSPS(rbsp, par.m_sps);
    sts = PackRBSP(m_bs_sps, m_rbsp, m_sz_sps, (rbsp.GetOffset() + 7) / 8);
    assert(!sts);
    if (sts)
        return sts;

    rbsp.Reset();
    
    PackPPS(rbsp, par.m_pps);
    sts = PackRBSP(m_bs_pps, m_rbsp, m_sz_pps, (rbsp.GetOffset() + 7) / 8);
    assert(!sts);

    m_par = &par;

    return sts;
}

void HeaderPacker::GetSSH(Task const & task, mfxU8*& buf, mfxU32& sizeInBytes)
{
    BitstreamWriter rbsp(m_bs_ssh, sizeof(m_bs_ssh));
    NALU nalu = {0, task.m_shNUT, 0, 1};

    //TODO: add multi-slice

    assert(m_par);

    PackSSH(rbsp, nalu, m_par->m_sps, m_par->m_pps, task.m_sh);

    buf         = m_bs_ssh;
    sizeInBytes = (rbsp.GetOffset() + 7) / 8;
}


} //MfxHwH265Encode