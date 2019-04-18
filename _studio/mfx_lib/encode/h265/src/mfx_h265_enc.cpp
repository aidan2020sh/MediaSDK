// Copyright (c) 2012-2019 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include <assert.h>
#include <numeric>
#include <algorithm>
#include <utility>

#include "ippi.h"
#include "vm_interlocked.h"
#include "vm_event.h"
#include "vm_sys_info.h"

#include "mfx_h265_encode.h"
#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"
#include "mfx_h265_brc.h"

#ifdef AMT_HROI_PSY_AQ
#include "FSapi.h"
#endif

using namespace H265Enc;
using namespace MfxEnumShortAliases;

namespace H265Enc {

#if defined( _WIN32) || defined(_WIN64)
#define thread_sleep(nms) Sleep(nms)
#else
#define thread_sleep(nms)
#endif
#define x86_pause() _mm_pause()


    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_cu_1[3][2][4][2] =
    {
        {
            {{0, 0},{6.068657e+000, 1.955850e-001},{2.794428e+001, 1.643400e-001},{4.697359e+002, 1.113505e-001},},
            {{0, 0},{8.335527e+000, 2.312656e-001},{1.282028e+001, 2.317006e-001},{3.218380e+001, 2.157863e-001},},
        },
        {
            {{0, 0},{1.412660e+001, 1.845380e-001},{1.967989e+001, 1.873910e-001},{3.617037e+002, 1.257173e-001},},
            {{0, 0},{2.319860e+001, 2.194218e-001},{2.436671e+001, 2.260317e-001},{3.785467e+001, 2.252255e-001},},
        },
        {
            {{0, 0},{1.465458e+001, 1.946118e-001},{1.725847e+001, 1.969719e-001},{3.298872e+002, 1.336427e-001},},
            {{0, 0},{3.133788e+001, 2.192709e-001},{3.292004e+001, 2.266248e-001},{3.572242e+001, 2.357623e-001},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_tu_1[3][2][4][2] =
    {
        {
            {{9.861009e+001, 2.014005e-001},{1.472963e+000, 2.407408e-001},{7.306935e+000, 2.073082e-001},{0, 0},},
            {{1.227516e+002, 1.973595e-001},{1.617983e+000, 2.342001e-001},{6.189427e+000, 2.090359e-001},{0, 0},},
        },
        {
            {{9.861009e+001, 2.014005e-001},{1.343445e+000, 2.666583e-001},{4.266056e+000, 2.504071e-001},{0, 0},},
            {{1.227516e+002, 1.973595e-001},{2.443985e+000, 2.427829e-001},{4.778531e+000, 2.343920e-001},{0, 0},},
        },
        {
            {{9.861009e+001, 2.014005e-001},{1.530746e+000, 3.027675e-001},{1.222733e+001, 2.649870e-001},{0, 0},},
            {{1.227516e+002, 1.973595e-001},{2.158767e+000, 2.696284e-001},{3.401860e+000, 2.652995e-001},{0, 0},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_cu_4[3][2][4][2] =
    {
        {
            {{0, 0},{4.218807e+000, 2.066010e-001},{2.560569e+001, 1.677201e-001},{4.697359e+002, 1.113505e-001},},
            {{0, 0},{4.848787e+000, 2.299086e-001},{1.198510e+001, 2.170575e-001},{3.218380e+001, 2.157863e-001},},
        },
        {
            {{0, 0},{5.227898e+000, 2.117919e-001},{1.819586e+001, 1.910210e-001},{3.617037e+002, 1.257173e-001},},
            {{0, 0},{1.098668e+001, 2.226733e-001},{2.273784e+001, 2.157968e-001},{3.785467e+001, 2.252255e-001},},
        },
        {
            {{0, 0},{4.591922e+000, 2.238883e-001},{1.480393e+001, 2.032860e-001},{3.298872e+002, 1.336427e-001},},
            {{0, 0},{1.831528e+001, 2.201327e-001},{2.999500e+001, 2.175826e-001},{3.572242e+001, 2.357623e-001},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_tu_4[3][2][4][2] =
    {
        {
            {{1.008684e+002, 2.003675e-001},{2.278697e+000, 2.226109e-001},{1.767972e+001, 1.733893e-001},{0, 0},},
            {{1.281323e+002, 1.984014e-001},{2.906725e+000, 2.063635e-001},{1.106598e+001, 1.798757e-001},{0, 0},},
        },
        {
            {{1.008684e+002, 2.003675e-001},{2.259367e+000, 2.473857e-001},{3.871648e+001, 1.789534e-001},{0, 0},},
            {{1.281323e+002, 1.984014e-001},{4.119740e+000, 2.092187e-001},{9.931154e+000, 1.907941e-001},{0, 0},},
        },
        {
            {{1.008684e+002, 2.003675e-001},{1.161667e+000, 3.112699e-001},{5.179975e+001, 2.364036e-001},{0, 0},},
            {{1.281323e+002, 1.984014e-001},{4.326205e+000, 2.262409e-001},{2.088369e+001, 1.829562e-001},{0, 0},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_cu_7[3][2][4][2] =
    {
        {
            {{0, 0},{0, 0},{2.868749e+001, 1.759965e-001},{0, 0},},
            {{0, 0},{0, 0},{1.203077e+001, 2.066983e-001},{0, 0},},
        },
        {
            {{0, 0},{0, 0},{2.689497e+001, 1.914970e-001},{0, 0},},
            {{0, 0},{0, 0},{1.856055e+001, 2.129173e-001},{0, 0},},
        },
            {{{0, 0},{0, 0},{2.268480e+001, 2.049253e-001},{0, 0},},
            {{0, 0},{0, 0},{3.414455e+001, 2.067140e-001},{0, 0},},
        },
    };

    //[strength][isNotI][log2w-3][2]
    Ipp32f h265_split_thresholds_tu_7[3][2][4][2] = {{{{7.228049e+001, 1.900956e-001},{2.365207e+000, 2.211628e-001},{4.651502e+001, 1.425675e-001},{0, 0},},
    {{0, 0},{2.456053e+000, 2.100343e-001},{7.537189e+000, 1.888014e-001},{0, 0},},
    },{{{7.228049e+001, 1.900956e-001},{1.293689e+000, 2.672417e-001},{1.633606e+001, 2.076887e-001},{0, 0},},
    {{0, 0},{1.682124e+000, 2.368017e-001},{5.516403e+000, 2.080632e-001},{0, 0},},
    },{{{7.228049e+001, 1.900956e-001},{1.479032e+000, 2.785342e-001},{8.723769e+000, 2.469559e-001},{0, 0},},
    {{0, 0},{1.943537e+000, 2.548912e-001},{1.756701e+001, 1.900659e-001},{0, 0},},
    },};


    typedef Ipp32f thres_tab[2][4][2];

    //static
    CostType h265_calc_split_threshold(Ipp32s tabIndex, Ipp32s isNotCu, Ipp32s isNotI, Ipp32s log2width, Ipp32s strength, Ipp32s QP)
    {
        thres_tab *h265_split_thresholds_tab;

        switch(tabIndex) {
        case 1:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_1 : h265_split_thresholds_cu_1;
            break;
        default:
        case 0:
        case 2:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_4 : h265_split_thresholds_cu_4;
            break;
        case 3:
            h265_split_thresholds_tab = isNotCu ? h265_split_thresholds_tu_7 : h265_split_thresholds_cu_7;
            break;
        }

        if (strength == 0)
            return 0;
        if (strength > 3)
            strength = 3;

        if (isNotCu) {
            if ((log2width < 3 || log2width > 5)) {
                //VM_ASSERT(0);
                return 0;
            }
        }
        else {
            if ((log2width < 4 || log2width > 6)) {
                //VM_ASSERT(0);
                return 0;
            }
        }

        CostType a = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][0];
        CostType b = h265_split_thresholds_tab[strength - 1][isNotI][log2width - 3][1];
        return a * exp(b * QP);
    }


    static const mfxU16 tab_AspectRatio265[17][2] =
    {
        { 1,  1},  // unspecified
        { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, {24, 11}, {20, 11}, {32, 11},
        {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160,99}, { 4,  3}, { 3,  2}, { 2,  1}
    };

    static mfxU32 ConvertSARtoIDC_H265enc(mfxU32 sarw, mfxU32 sarh)
    {
        for (mfxU32 i = 1; i < sizeof(tab_AspectRatio265) / sizeof(tab_AspectRatio265[0]); i++)
            if (sarw * tab_AspectRatio265[i][1] == sarh * tab_AspectRatio265[i][0])
                return i;
        return 0;
    }
}


void H265Encoder::SetVPS()
{
    Zero(m_vps);

    m_vps.vps_max_layers = 1;
    m_vps.vps_max_sub_layers = 1;
    m_vps.vps_temporal_id_nesting_flag = 1;

    m_vps.vps_max_dec_pic_buffering[0] = m_videoParam.MaxDecPicBuffering;
    m_vps.vps_max_num_reorder_pics[0] = 0;
    if (m_videoParam.GopRefDist > 1)
        m_vps.vps_max_num_reorder_pics[0] = MAX(1, m_videoParam.BiPyramidLayers - 1);
    if (m_videoParam.picStruct != PROGR)
        m_vps.vps_max_num_reorder_pics[0] *= 2;

    m_vps.vps_max_latency_increase[0] = 0;
    m_vps.vps_num_layer_sets = 1;

    m_vps.vps_timing_info_present_flag = 0;
    if (m_vps.vps_timing_info_present_flag) {
        m_vps.vps_num_units_in_tick = m_videoParam.FrameRateExtD;
        m_vps.vps_time_scale = m_videoParam.FrameRateExtN;
        m_vps.vps_poc_proportional_to_timing_flag = 0;
        m_vps.vps_num_hrd_parameters = 0;
    }
}

void H265Encoder::SetProfileLevel()
{
    Zero(m_profile_level);

    m_profile_level.general_profile_idc = (mfxU8) m_videoParam.Profile;
    m_profile_level.general_tier_flag = (mfxU8) m_videoParam.Tier;
    m_profile_level.general_level_idc = (mfxU8) (m_videoParam.Level * 3);
    m_profile_level.general_profile_compatibility_flag[MIN(m_videoParam.Profile, 31)] = 1;
    if (m_videoParam.Profile == MFX_PROFILE_HEVC_REXT) {
        m_profile_level.general_max_12bit_constraint_flag        = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_12BIT);
        m_profile_level.general_max_10bit_constraint_flag        = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_10BIT);
        m_profile_level.general_max_8bit_constraint_flag         = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_8BIT);
        m_profile_level.general_max_422chroma_constraint_flag    = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_422CHROMA);
        m_profile_level.general_max_420chroma_constraint_flag    = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_420CHROMA);
        m_profile_level.general_max_monochrome_constraint_flag   = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_MAX_MONOCHROME);
        m_profile_level.general_intra_constraint_flag            = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_INTRA);
        m_profile_level.general_one_picture_only_constraint_flag = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_ONE_PICTURE_ONLY);
        m_profile_level.general_lower_bit_rate_constraint_flag   = !!(m_videoParam.generalConstraintFlags & MFX_HEVC_CONSTR_REXT_LOWER_BIT_RATE);
    }
}

void H265Encoder::SetSPS()
{
    const H265VideoParam &pars = m_videoParam;

    Zero(m_sps);

    m_sps.sps_video_parameter_set_id = m_vps.vps_video_parameter_set_id;
    m_sps.sps_max_sub_layers = 1;
    m_sps.sps_temporal_id_nesting_flag = 1;

    m_sps.chroma_format_idc = pars.chromaFormatIdc;

    m_sps.pic_width_in_luma_samples = pars.Width;
    m_sps.pic_height_in_luma_samples = pars.Height;
    m_sps.bit_depth_luma = pars.bitDepthLuma;
    m_sps.bit_depth_chroma = pars.bitDepthChroma;

    if (pars.CropLeft | pars.CropTop | pars.CropRight | pars.CropBottom) {
        m_sps.conformance_window_flag = 1;
        m_sps.conf_win_left_offset = pars.CropLeft >> m_videoParam.chromaShiftW;
        m_sps.conf_win_top_offset = pars.CropTop >> m_videoParam.chromaShiftH;
        m_sps.conf_win_right_offset = pars.CropRight >> m_videoParam.chromaShiftW;
        m_sps.conf_win_bottom_offset = pars.CropBottom >> m_videoParam.chromaShiftH;
    }

    m_sps.log2_diff_max_min_coding_block_size = (Ipp8u)(pars.MaxCUDepth - pars.AddCUDepth);
    m_sps.log2_min_coding_block_size_minus3 = (Ipp8u)(pars.Log2MaxCUSize - m_sps.log2_diff_max_min_coding_block_size - 3);

    m_sps.log2_min_transform_block_size_minus2 = (Ipp8u)(pars.QuadtreeTULog2MinSize - 2);
    m_sps.log2_diff_max_min_transform_block_size = (Ipp8u)(pars.QuadtreeTULog2MaxSize - pars.QuadtreeTULog2MinSize);

    m_sps.max_transform_hierarchy_depth_intra = (Ipp8u)pars.QuadtreeTUMaxDepthIntra;
    m_sps.max_transform_hierarchy_depth_inter = (Ipp8u)pars.QuadtreeTUMaxDepthInter;

    m_sps.amp_enabled_flag = (pars.partModes==3);
    m_sps.sps_temporal_mvp_enabled_flag = pars.TMVPFlag;
    m_sps.sample_adaptive_offset_enabled_flag = pars.SAOFlag;
    m_sps.strong_intra_smoothing_enabled_flag = pars.strongIntraSmoothingEnabledFlag;

    InitShortTermRefPicSet();

    if (m_videoParam.GopRefDist == 1) {
        m_sps.log2_max_pic_order_cnt_lsb = 4;
    } else {
        Ipp32s log2_max_poc = H265_CeilLog2(m_videoParam.GopRefDist - 1 + m_videoParam.MaxDecPicBuffering) + 3;

        m_sps.log2_max_pic_order_cnt_lsb = (Ipp8u)MAX(log2_max_poc, 4);

        if (m_sps.log2_max_pic_order_cnt_lsb > 16) {
            VM_ASSERT(false);
            m_sps.log2_max_pic_order_cnt_lsb = 16;
        }
    }

    m_sps.sps_num_reorder_pics[0] = m_vps.vps_max_num_reorder_pics[0];
    m_sps.sps_max_dec_pic_buffering[0] = m_vps.vps_max_dec_pic_buffering[0];
    m_sps.sps_max_latency_increase[0] = m_vps.vps_max_latency_increase[0];

    InitLongTermRefPicSet();   // AMT_LTR

    // VUI
    m_sps.vui_parameters_present_flag = pars.vuiParametersPresentFlag;
    if (m_sps.vui_parameters_present_flag) {
        m_sps.aspect_ratio_info_present_flag = !!(pars.AspectRatioW && pars.AspectRatioH);
        if (m_sps.aspect_ratio_info_present_flag) {
            m_sps.aspect_ratio_idc = (Ipp8u)ConvertSARtoIDC_H265enc(pars.AspectRatioW, pars.AspectRatioH);
            if (m_sps.aspect_ratio_idc == 0) {
                m_sps.aspect_ratio_idc = 255; //Extended SAR
                m_sps.sar_width = pars.AspectRatioW;
                m_sps.sar_height = pars.AspectRatioH;
            }
        }
        m_sps.overscan_info_present_flag = 0;
        m_sps.overscan_appropriate_flag = 0;
        m_sps.video_signal_type_present_flag = 0;
        m_sps.chroma_loc_info_present_flag = 0;
        m_sps.neutral_chroma_indication_flag = 0;
        m_sps.field_seq_flag = (pars.picStruct != PROGR);
        m_sps.frame_field_info_present_flag = (pars.picStruct != PROGR);
        m_sps.default_display_window_flag = 0;
        m_sps.vui_timing_info_present_flag = 1;
        if (m_sps.vui_timing_info_present_flag) {
            m_sps.vui_num_units_in_tick = pars.FrameRateExtD;
            m_sps.vui_time_scale = pars.FrameRateExtN;
            m_sps.vui_poc_proportional_to_timing_flag = 0;
            m_sps.vui_hrd_parameters_present_flag = pars.hrdPresentFlag;
            if (m_sps.vui_hrd_parameters_present_flag) {
                m_sps.nal_hrd_parameters_present_flag = 1;
                m_sps.vcl_hrd_parameters_present_flag = 0;
                if (m_sps.nal_hrd_parameters_present_flag || m_sps.vcl_hrd_parameters_present_flag) {
                    m_sps.sub_pic_hrd_params_present_flag = 0;
                    m_sps.bit_rate_scale = 0;
                    m_sps.cpb_size_scale = 2;
                    while (m_sps.bit_rate_scale < 16 && (pars.hrdBitrate & ((1 << 6 + m_sps.bit_rate_scale + 1) - 1)) == 0)
                        m_sps.bit_rate_scale++;
                    while (m_sps.cpb_size_scale < 16 && (pars.cpbSize & ((1 << 4 + m_sps.cpb_size_scale + 1) - 1)) == 0)
                        m_sps.cpb_size_scale++;
                    m_sps.initial_cpb_removal_delay_length_minus1 = 23;
                    m_sps.au_cpb_removal_delay_length_minus1 = 23;
                    m_sps.dpb_output_delay_length_minus1 = 23;
                }
                for (Ipp32s i = 0; i < m_sps.sps_max_sub_layers; i++) {
                    m_sps.fixed_pic_rate_general_flag = 0;
                    m_sps.fixed_pic_rate_within_cvs_flag = 0;
                    m_sps.low_delay_hrd_flag = 0;
                    m_sps.cpb_cnt_minus1 = 0;
                    if (m_sps.nal_hrd_parameters_present_flag) {
                        m_sps.bit_rate_value_minus1 = (pars.hrdBitrate >> (6 + m_sps.bit_rate_scale)) - 1;
                        m_sps.cpb_size_value_minus1 = (pars.cpbSize >> (4 + m_sps.cpb_size_scale)) - 1;
                        m_sps.cbr_flag = pars.cbrFlag;
                    }
                }
            }
        }
        m_sps.bitstream_restriction_flag = 0;
    }

    m_sps.PicSizeInCtbsY = pars.PicWidthInCtbs * pars.PicHeightInCtbs;
}

void H265Encoder::SetPPS()
{
    Zero(m_pps);

    m_pps.pps_seq_parameter_set_id = m_sps.sps_seq_parameter_set_id;
    m_pps.sign_data_hiding_enabled_flag = m_videoParam.SBHFlag;
    m_pps.constrained_intra_pred_flag = m_videoParam.constrainedIntrapredFlag;
    m_pps.transform_skip_enabled_flag = m_videoParam.transformSkipEnabledFlag;

    if (m_videoParam.GopRefDist == 1) { // most frames are P frames
        m_pps.num_ref_idx_l0_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxP[0]);
        m_pps.num_ref_idx_l1_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxP[1]);
    }
    else { // most frames are B frames
        m_pps.num_ref_idx_l0_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxB[0]);
        m_pps.num_ref_idx_l1_default_active = IPP_MAX(1, m_videoParam.MaxRefIdxB[1]);
    }

    m_pps.init_qp = m_brc ? (Ipp8s)m_brc->GetQP(&m_videoParam, NULL, NULL) : m_videoParam.QPI;

    if (m_videoParam.UseDQP) {
        m_pps.cu_qp_delta_enabled_flag = 1;
        m_pps.diff_cu_qp_delta_depth   = m_videoParam.MaxCuDQPDepth;
    }

    m_pps.weighted_pred_flag = m_videoParam.weightedPredFlag;
    m_pps.weighted_bipred_flag = m_videoParam.weightedBipredFlag;
    m_pps.transquant_bypass_enable_flag = m_videoParam.transquantBypassEnableFlag;
    m_pps.tiles_enabled_flag = m_videoParam.NumTiles > 1 ? 1 : 0;
    m_pps.entropy_coding_sync_enabled_flag = m_videoParam.WPPFlag;
    m_pps.num_tile_columns = m_videoParam.NumTileCols;
    m_pps.num_tile_rows = m_videoParam.NumTileRows;
    m_pps.uniform_spacing_flag = 1;
    m_pps.loop_filter_across_tiles_enabled_flag = m_videoParam.NumTiles > 1 ? m_videoParam.deblockBordersFlag : 0;
    m_pps.pps_loop_filter_across_slices_enabled_flag = m_videoParam.NumSlices > 1 ? m_videoParam.deblockBordersFlag : 0;
    m_pps.deblocking_filter_control_present_flag = !m_videoParam.deblockingFlag || m_videoParam.tcOffset || m_videoParam.betaOffset;
    m_pps.deblocking_filter_override_enabled_flag = 0;
    m_pps.pps_deblocking_filter_disabled_flag = !m_videoParam.deblockingFlag;
    m_pps.pps_beta_offset_div2 = m_videoParam.betaOffset;
    m_pps.pps_tc_offset_div2 = m_videoParam.tcOffset;
    m_pps.lists_modification_present_flag = 1;
    m_pps.log2_parallel_merge_level = m_videoParam.log2ParallelMergeLevel;
}

void H265Encoder::SetSeiAps()
{
    Zero(m_seiAps);
    m_seiAps.active_video_parameter_set_id = m_vps.vps_video_parameter_set_id;
    m_seiAps.self_contained_cvs_flag = 0;
    m_seiAps.no_parameter_set_update_flag = 1;
    m_seiAps.num_sps_ids_minus1 = 0;
    m_seiAps.active_seq_parameter_set_id[0] = m_sps.sps_seq_parameter_set_id;
}

const Ipp32f tab_rdLambdaBPyramid5LongGop[5]  = {0.442f, 0.3536f, 0.3536f, 0.4000f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid5[5]         = {0.442f, 0.3536f, 0.3536f, 0.3536f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid4[4]         = {0.442f, 0.3536f, 0.3536f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid_LoCmplx[4]  = {0.442f, 0.3536f, 0.4000f, 0.68f};
const Ipp32f tab_rdLambdaBPyramid_MidCmplx[4] = {0.442f, 0.3536f, 0.3817f, 0.60f};
const Ipp32f tab_rdLambdaBPyramid_HiCmplx[4]  = {0.442f, 0.3536f, 0.3536f, 0.50f}; 


void H265Encoder::SetSlice(H265Slice *slice, Ipp32u curr_slice, Frame *frame)
{
    memset(slice, 0, sizeof(H265Slice));

    slice->slice_segment_address = m_videoParam.m_slices[curr_slice].first_ctb_addr;
    slice->slice_address_last_ctb = m_videoParam.m_slices[curr_slice].last_ctb_addr;

    if (curr_slice) slice->first_slice_segment_in_pic_flag = 0;
    else slice->first_slice_segment_in_pic_flag = 1;

    slice->slice_pic_parameter_set_id = m_pps.pps_pic_parameter_set_id;

    switch (frame->m_picCodeType) {
    case MFX_FRAMETYPE_P:
        slice->slice_type = (m_videoParam.GeneralizedPB) ? B_SLICE : P_SLICE; break;
    case MFX_FRAMETYPE_B:
        slice->slice_type = B_SLICE; break;
    case MFX_FRAMETYPE_I:
    default:
        slice->slice_type = I_SLICE; break;
    }

    slice->sliceIntraAngMode = EnumIntraAngMode((frame->m_picCodeType == MFX_FRAMETYPE_B && !frame->m_isRef)
        ? m_videoParam.intraAngModes[B_NONREF]
        : ((frame->m_picCodeType == MFX_FRAMETYPE_P)
            ? m_videoParam.intraAngModes[SliceTypeIndex(P_SLICE)]
            : m_videoParam.intraAngModes[SliceTypeIndex(slice->slice_type)]));

    if (frame->m_isIdrPic)
        slice->NalUnitType = NAL_IDR_W_RADL;
    else if (frame->m_picCodeType == MFX_FRAMETYPE_I)
        slice->NalUnitType = NAL_CRA;
    else if (frame->m_frameOrder < frame->m_frameOrderOfLastIntraInEncOrder)
        slice->NalUnitType = frame->m_isRef ? NAL_RASL_R : NAL_RASL_N;
    else
        slice->NalUnitType = frame->m_isRef ? NAL_TRAIL_R : NAL_TRAIL_N;

    // hint from lookahead
    if (frame->m_forceTryIntra && (slice->NalUnitType == NAL_RASL_R || slice->NalUnitType == NAL_RASL_N)) {
       slice->sliceIntraAngMode = IPP_MIN(INTRA_ANG_MODE_GRADIENT, slice->sliceIntraAngMode);
    }

    slice->slice_pic_order_cnt_lsb = frame->m_poc & ~(0xffffffff << m_sps.log2_max_pic_order_cnt_lsb);
    slice->deblocking_filter_override_flag = 0;
    slice->slice_deblocking_filter_disabled_flag = m_pps.pps_deblocking_filter_disabled_flag;
    slice->slice_tc_offset_div2 = m_pps.pps_tc_offset_div2;
    slice->slice_beta_offset_div2 = m_pps.pps_beta_offset_div2;
    slice->slice_loop_filter_across_slices_enabled_flag = m_pps.pps_loop_filter_across_slices_enabled_flag;
    slice->slice_cb_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_cr_qp_offset = m_pps.pps_cb_qp_offset;
    slice->slice_temporal_mvp_enabled_flag = m_sps.sps_temporal_mvp_enabled_flag;

    slice->num_ref_idx[0] = frame->m_refPicList[0].m_refFramesCount;
    slice->num_ref_idx[1] = frame->m_refPicList[1].m_refFramesCount;

    if (slice->slice_type == P_SLICE)
        slice->num_ref_idx_active_override_flag =
        (slice->num_ref_idx[0] != m_pps.num_ref_idx_l0_default_active);
    else if (slice->slice_type == B_SLICE)
        slice->num_ref_idx_active_override_flag =
        (slice->num_ref_idx[0] != m_pps.num_ref_idx_l0_default_active) ||
        (slice->num_ref_idx[1] != m_pps.num_ref_idx_l1_default_active);

    slice->short_term_ref_pic_set_sps_flag = 1;
    slice->short_term_ref_pic_set_idx = frame->m_RPSIndex;

    if (m_videoParam.enableLTR)
    {
        slice->pListModif = &frame->m_listModif;
        slice->pStRps = &frame->m_shortRefPicSet[0];
        slice->short_term_ref_pic_set_sps_flag = frame->short_term_ref_pic_set_sps_flag;
        slice->short_term_ref_pic_set_idx = frame->short_term_ref_pic_set_idx;
        slice->pLtRps = &frame->m_longRefPicSet[0];
        slice->num_long_term_sps = frame->num_long_term_sps;
        slice->num_long_term_pics = frame->num_long_term_pics;

        if (slice->slice_type == I_SLICE) {
            slice->num_long_term_sps = 0;
            slice->num_long_term_pics = 0;
            slice->num_ref_idx[0] = 0;
            slice->num_ref_idx[1] = 0;
            slice->pListModif->refPicListModificationFlag[0] = slice->pListModif->refPicListModificationFlag[1] = 0;
            slice->pStRps->num_negative_pics = slice->pStRps->num_positive_pics = 0;
        }
    }
    Ipp32s num_long_term_pics = frame->num_long_term_sps + frame->num_long_term_pics;

    slice->slice_num = curr_slice;

    slice->five_minus_max_num_merge_cand = 5 - MAX_NUM_MERGE_CANDS;

    if (m_pps.entropy_coding_sync_enabled_flag || m_videoParam.NumSlices > 1) {
        slice->row_first = slice->slice_segment_address / m_videoParam.PicWidthInCtbs;
        slice->row_last = slice->slice_address_last_ctb / m_videoParam.PicWidthInCtbs;
        slice->num_entry_point_offsets = slice->row_last - slice->row_first;
    } else if (m_pps.tiles_enabled_flag) {
        slice->row_first = 0;
        slice->row_last = m_videoParam.PicHeightInCtbs - 1;
        slice->num_entry_point_offsets = m_videoParam.NumTiles - 1;
    }
    if(!m_videoParam.SAOFlag
        || (m_videoParam.saoSubOpt==3 && m_videoParam.BiPyramidLayers > 1 && frame->m_pyramidLayer == m_videoParam.BiPyramidLayers - 1)

            ) {
        slice->slice_sao_luma_flag = false;
        slice->slice_sao_chroma_flag = false;
    } else {
        slice->slice_sao_luma_flag = true;
        slice->slice_sao_chroma_flag = m_videoParam.SAOChromaFlag ? true : false;
    }

    slice->CeilLog2NumPocTotalCurr = frame->m_ceilLog2NumPocTotalCurr;
  if (!m_videoParam.enableLTR)
  {
    for (Ipp32s l = 0; l < 2; l++) {
        slice->ref_pic_list_modification_flag[l] = frame->m_refPicList[l].m_listModFlag;
        if (frame->m_refPicList[l].m_listModFlag) {
            for (Ipp32s r = 0; r < frame->m_refPicList[l].m_refFramesCount; r++)
                slice->list_entry[l][r] = frame->m_refPicList[l].m_listMod[r];
        }
    }
  }
}

namespace H265Enc {

    void GetGopComplexity(H265VideoParam const & par, const Frame *frame, bool& IsHiCmplxGOP, bool& IsMedCmplxGOP) 
    {
        Statistics* stats = frame->m_stats[0];
        if (par.DeltaQpMode&AMT_DQP_CAL) {
            Ipp32f SADpp = stats->avgTSC;
            Ipp32f SCpp = stats->avgsqrSCpp;
            if (SCpp > 2.0) {
                Ipp32f minSADpp = 0;
                if (par.GopRefDist > 8) {
                    minSADpp = 1.3f*SCpp - 2.6f;
                    if (minSADpp > 0 && minSADpp < SADpp) IsHiCmplxGOP = true;
                    if (!IsHiCmplxGOP) {
                        Ipp32f minSADpp = 1.1f*SCpp - 2.2f;
                        if (minSADpp > 0 && minSADpp < SADpp) IsMedCmplxGOP = true;
                    }
                }
                else {
                    minSADpp = 1.1f*SCpp - 1.5f;
                    if (minSADpp > 0 && minSADpp < SADpp) IsHiCmplxGOP = true;
                    if (!IsHiCmplxGOP) {
                        Ipp32f minSADpp = 1.0f*SCpp - 2.0f;
                        if (minSADpp > 0 && minSADpp < SADpp) IsMedCmplxGOP = true;
                    }
                }
            }
        }
    }

    bool SliceLambdaMultiplier(CostType &rd_lambda_slice, H265VideoParam const & videoParam, Ipp8u slice_type, const Frame *currFrame, bool isHiCmplxGop, bool isMidCmplxGop, bool useCAL)
    {
        bool mult = false;
        switch (slice_type) {
        case P_SLICE:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                rd_lambda_slice = (currFrame->m_biFramesInMiniGop == 15)
                    ? (videoParam.longGop)
                    ? tab_rdLambdaBPyramid5LongGop[0]
                : tab_rdLambdaBPyramid5[0]
                : tab_rdLambdaBPyramid4[0];
                if (videoParam.DeltaQpMode&AMT_DQP_CAQ) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            else {
                Ipp32s pgopIndex = (currFrame->m_frameOrder - currFrame->m_frameOrderOfLastIntra) % videoParam.PGopPicSize;
                rd_lambda_slice = (videoParam.PGopPicSize == 1 || pgopIndex) ? 0.4624f : 0.578f;
                mult = !!pgopIndex;
                if (!pgopIndex && (videoParam.DeltaQpMode&AMT_DQP_CAQ)) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            break;
        case B_SLICE:
            if (videoParam.BiPyramidLayers > 1 && OPT_LAMBDA_PYRAMID) {
                Ipp8u layer = currFrame->m_pyramidLayer;
                if ((videoParam.DeltaQpMode&AMT_DQP_CAL) && useCAL) {
                    if (isHiCmplxGop)
                        rd_lambda_slice = tab_rdLambdaBPyramid_HiCmplx[layer];
                    else if (isMidCmplxGop)
                        rd_lambda_slice = tab_rdLambdaBPyramid_MidCmplx[layer];
                    else
                        rd_lambda_slice = tab_rdLambdaBPyramid_LoCmplx[layer];
                }
                else 
                {
                    rd_lambda_slice = (currFrame->m_biFramesInMiniGop == 15)
                        ? (videoParam.longGop)
                            ? tab_rdLambdaBPyramid5LongGop[layer]
                            : tab_rdLambdaBPyramid5[layer]
                        : tab_rdLambdaBPyramid4[layer];
                }
                mult = !!layer;
                if (!layer && (videoParam.DeltaQpMode&AMT_DQP_CAQ)) {
                    rd_lambda_slice*=videoParam.LambdaCorrection;
                }
            }
            else {
                rd_lambda_slice = 0.4624f;
                mult = true;
            }
            break;
        case I_SLICE:
        default:
            rd_lambda_slice = 0.57f;
            if (videoParam.GopRefDist > 1)
                rd_lambda_slice *= (1 - MIN(0.5f, 0.05f * (videoParam.GopRefDist - 1)));
            if (videoParam.DeltaQpMode&AMT_DQP_CAQ) {
                rd_lambda_slice*=videoParam.LambdaCorrection;
            }
        }
        return mult;
    }


    void SetSliceLambda(H265VideoParam const & videoParam, H265Slice *slice, Ipp32s qp, const Frame *currFrame, CostType lambdaMult, bool extraMult)
    {
        slice->rd_opt_flag = 1;
        slice->rd_lambda_slice = h265_lambda[qp + 48] * (1.0 / 256.f) * lambdaMult;
        if (extraMult)
            slice->rd_lambda_slice *= Saturate(2, 4, (qp - 12) / 6.f);

        slice->rd_lambda_inter_slice = slice->rd_lambda_slice;
        slice->rd_lambda_inter_mv_slice = slice->rd_lambda_inter_slice;

        slice->rd_lambda_sqrt_slice = sqrt(slice->rd_lambda_slice * 256);
        //no chroma QP offset (from PPS) is implemented yet
        if(!videoParam.rdoqChromaFlag && (videoParam.DeltaQpMode&AMT_DQP_CAQ)) {
            Ipp32s qpCaq = (int)((4.2005f*log(slice->rd_lambda_slice * 256.f)+13.7122f)+0.5f); // est
            Ipp32s qpcCaq = GetChromaQP(qpCaq, 0, videoParam.chromaFormatIdc, 8); // just scaled qPi
            Ipp32f qpcCorr = qpCaq - qpcCaq;
            Ipp32f qpcDiff = IPP_MAX(0,GetChromaQP(qp, 0, videoParam.chromaFormatIdc, 8) - qpcCaq); // just scaled qPi
            qpcCorr -= (qpcDiff/2.f);
            slice->ChromaDistWeight_slice = pow(2.f, qpcCorr / 3.f);
        } else 
        {
            Ipp32s qpc = GetChromaQP(qp, 0, videoParam.chromaFormatIdc, 8); // just scaled qPi
            slice->ChromaDistWeight_slice = h265_lambda[qp - qpc + 60];
        }
    }

#ifdef AMT_HROI_PSY_AQ

    void setRoiDeltaQP(const H265VideoParam *par, Frame *frame, Ipp8u useBrc)
    {
        const Ipp32f l2f = log(2.0f);
        const Ipp32f dB[9]= {1.0f, 0.891f, 0.794f, 0.707f, 0.623f, 0.561f, 0.5f, 0.445f, 0.39f};
        const Ipp32f dF[9]= {1.0f, 1.123f, 1.260f, 1.414f, 1.587f, 1.782f, 2.0f, 2.245f, 2.52f};
        Ipp32s numCtb = par->PicWidthInCtbs * par->PicHeightInCtbs;
        Statistics *stats = frame->m_stats[0];
        CtbRoiStats *ctbStats = stats->ctbStats;
        Ipp32s idxRsCs = par->Log2MaxCUSize-3;
        Ipp32s pitchRsCsQ = stats->m_pitchRsCs4>>(par->Log2MaxCUSize-3);
        Ipp32s nRsCs = (1<<(par->Log2MaxCUSize-3))*(1<<(par->Log2MaxCUSize-3));
        Ipp64f avgMinblkRsCs = 0.0, avgRsCs = 0.0;
        Ipp32s luminanceAvg = stats->roi_pic.luminanceAvg;
        if(luminanceAvg < 32) luminanceAvg = 32;
        Ipp32s noRb = 0, noBb = 0;
        if(par->PicHeightInCtbs*(1<<par->Log2MaxCUSize) > par->Height) noBb = 1;
        if(par->PicWidthInCtbs*(1<<par->Log2MaxCUSize)  > par->Width)  noRb = 1;

        Ipp32s bn = stats->roi_pic.numCtbRoi[0];
        Ipp32s fn = stats->roi_pic.numCtbRoi[1] + stats->roi_pic.numCtbRoi[2];
        Ipp32s bnnz = 0;

        Ipp32s fni = 0;
        Ipp32s numAct = 0;
        Ipp32s qpa = frame->m_sliceQpY - (frame->m_pyramidLayer - ((frame->m_picCodeType == MFX_FRAMETYPE_I) ? 1 : 0));
        if ( (frame->m_picCodeType == MFX_FRAMETYPE_I && !(par->DeltaQpMode&AMT_DQP_PSY)) || (qpa  < 16) ) 
        {
            // reset
            for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++) {
                for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++) {
                    ctbStats[i*par->PicWidthInCtbs+j].dqp = 0;
                    ctbStats[i*par->PicWidthInCtbs+j].roiLevel =0;
                }
            }
            return;
        }

        // Contrast
        for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++)
        {
            for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++)
            {
                // Border and Black Mask
                if(!((j == par->PicWidthInCtbs-1 && noRb) || (i == par->PicHeightInCtbs -1 && noBb)) && ctbStats[i*par->PicWidthInCtbs+j].luminance>=32) {
                    Ipp32f minblkRsCs = INT_MAX;
                    Ipp32f ctbRsCs = 0.0;
                    for(Ipp32s k = 0; k < 2; k++) {
                        for(Ipp32s l = 0; l < 2; l++) {
                            Ipp32f blkRsCs = stats->m_rs[idxRsCs][(i*2+k)*pitchRsCsQ+j*2+l] + stats->m_cs[idxRsCs][(i*2+k)*pitchRsCsQ+2*j+l];
                            blkRsCs /= (1 << (par->bitDepthLumaShift * 2));
                            minblkRsCs = IPP_MIN(blkRsCs, minblkRsCs);
                            ctbRsCs += blkRsCs;
                        }
                    }
                    minblkRsCs /= (nRsCs*2.0); 
                    minblkRsCs += 1.0;
                    ctbStats[i*par->PicWidthInCtbs+j].spatMinQCmplx = minblkRsCs;
                    ctbStats[i*par->PicWidthInCtbs+j].spatCmplx = ctbRsCs / (nRsCs*4.0);
                    avgMinblkRsCs += minblkRsCs;
                    avgRsCs += ctbRsCs;
                    numAct++;
                }
            }
        }

        if(numAct) avgMinblkRsCs /= numAct;
        else       avgMinblkRsCs = 1.0;

        // Control
        Ipp32s maxFq = -1, minBq = 0;
        
        Ipp32s          qrange = 7;
        
        if     (qpa<18) qrange = 2;
        else if(qpa<23) qrange = 3;
        else if(qpa<28) qrange = 4;
        else if(qpa<34) qrange = 5;
        else if(qpa<40) qrange = 6;
        else if(qpa<44) qrange = 4;
        else            qrange = 2;
        
        if(par->BiPyramidLayers >1 && frame->m_pyramidLayer) qrange -= frame->m_pyramidLayer;
        if((par->DeltaQpMode&AMT_DQP_PAQ) && par->BiPyramidLayers >1 && frame->m_pyramidLayer==0) qrange--;
        if(par->BiPyramidLayers >1 && frame->m_pyramidLayer<=par->refLayerLimit[0]) qrange--;
        
        if(qrange<1) 
            qrange = 1;

        if(frame->m_picCodeType == MFX_FRAMETYPE_I) {
            qrange=(qrange+1)/2;
        }
        
        if (useBrc && fn == 0) qrange = ((par->enableCmFlag && par->bitDepthLuma == 8) ? IPP_MIN(qrange, 2) : 1);  // Till we solve MT Slowdown & 10 bit loss (Psy now auto on for BRC)
        
        // VSM (Visual Sensitivity Map)
        Ipp32f mult = pow(2.0f, (Ipp32f)qrange/6.0f);

        for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++)
        {
            for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++)
            {
                Ipp32s val=0;
                Ipp32s edge = 0;
                // Border and Black Mask
                if(!((j == par->PicWidthInCtbs-1 && noRb) || (i == par->PicHeightInCtbs -1 && noBb)) && ctbStats[i*par->PicWidthInCtbs+j].luminance>=32) {
                    Ipp32f minblkRsCs = ctbStats[i*par->PicWidthInCtbs+j].spatMinQCmplx;
                    Ipp32f RsCsRatio = (mult * minblkRsCs + avgMinblkRsCs) / (minblkRsCs + mult * avgMinblkRsCs);
                    edge = (ctbStats[i*par->PicWidthInCtbs+j].spatCmplx / minblkRsCs > 8.0) ? 1 : 0;
                    Ipp32f dDQp = (log(RsCsRatio) / l2f) * 6.0;

                    Ipp32f luminance = ctbStats[i*par->PicWidthInCtbs+j].luminance;
                    Ipp32f lRatio = luminance  / (Ipp32f) luminanceAvg;
                    Ipp32f dDQl = (0.3f * log(lRatio) / l2f) * (Ipp32f)qrange; 

                    val = int(floor( dDQp + dDQl + 0.5 ));
                }
                ctbStats[i*par->PicWidthInCtbs+j].sedge = edge;
                ctbStats[i*par->PicWidthInCtbs+j].sensitivity = val;
            }
        }
        
        if(fn == 0) 
        {
            // VROI (Visual sensitivity ROI)
            for(Ipp32s i = 0; i < (Ipp32s) par->PicHeightInCtbs; i++) {
                for(Ipp32s j = 0; j < (Ipp32s) par->PicWidthInCtbs; j++) {
                    Ipp32s val = ctbStats[i*par->PicWidthInCtbs+j].sensitivity;
                    Ipp32s edge = ctbStats[i*par->PicWidthInCtbs+j].sedge;
                    
                    ctbStats[i*par->PicWidthInCtbs+j].dqp = (val<-qrange)?-qrange:((val>qrange)?qrange:val);
                    ctbStats[i*par->PicWidthInCtbs+j].roiLevel = (val<0)?2:0;
                    
                    if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel > 0) {
                        if(edge) {
                            if(ctbStats[i*par->PicWidthInCtbs+j].dqp<-1) ctbStats[i*par->PicWidthInCtbs+j].dqp+=1;
                            ctbStats[i*par->PicWidthInCtbs+j].roiLevel = 1;
                        }
                        fni++;
                    }
                }
            }
            // EROI = VROI
            fn = fni;
            bn = numCtb - fn;
        } 
        else 
        {
            // HROI
            for(Ipp32s i = 0; i < (Ipp32s) par->PicHeightInCtbs; i++) {
                for(Ipp32s j = 0; j < (Ipp32s) par->PicWidthInCtbs; j++) {
                    Ipp32s val = ctbStats[i*par->PicWidthInCtbs+j].sensitivity;

                    if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel < 1) {
                        ctbStats[i*par->PicWidthInCtbs+j].dqp = (val<minBq)?minBq:((val>qrange)?qrange:val);
                    } else {
                        ctbStats[i*par->PicWidthInCtbs+j].dqp = (val>maxFq)?maxFq:((val<-qrange)?-qrange:val);
                    }
                }
            }

            // EROI (Enhanced) = HROI + VROI'
            const Ipp32s minEroi = 4;
            if((par->DeltaQpMode&AMT_DQP_PSY) && fn < numAct/minEroi) 
            {

                Ipp32s thresh = -5;
                // Add full
                while(fn < numAct/minEroi && thresh < maxFq) 
                {
                    for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++) {
                        for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++) {
                            if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel == 0) {
                                Ipp32s val = ctbStats[i*par->PicWidthInCtbs+j].sensitivity;
                                if(val < thresh && fn < numAct/minEroi && !ctbStats[i*par->PicWidthInCtbs+j].sedge) {
                                    ctbStats[i*par->PicWidthInCtbs+j].roiLevel = 2;
                                    ctbStats[i*par->PicWidthInCtbs+j].dqp = ((val<-qrange)?-qrange:val);
                                    ctbStats[i*par->PicWidthInCtbs+j].dqp -= maxFq;   // rebase to maxfq
                                    fn++;
                                    fni++;
                                }
                            }
                        }
                    }
                    if(fn < numAct/minEroi && thresh < (maxFq-1))
                    {
                        for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++) {
                            for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++) {
                                if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel == 0) {
                                    Ipp32s val  = ctbStats[i*par->PicWidthInCtbs+j].sensitivity;
                                    if(val < thresh && fn < numAct/minEroi && ctbStats[i*par->PicWidthInCtbs+j].sedge) {
                                        ctbStats[i*par->PicWidthInCtbs+j].roiLevel = 1;
                                        ctbStats[i*par->PicWidthInCtbs+j].dqp = ((val<-qrange)?-qrange:val);
                                        ctbStats[i*par->PicWidthInCtbs+j].dqp -= (maxFq-1); // rebase to maxfq
                                        fn++;
                                        fni++;
                                    }
                                }
                            }
                        }
                    }
                    thresh++;
                }

                bn  = numCtb - fn;
            }
        }

        // Qp & Complexity
        Ipp32f sumQpb = 0.0;
        Ipp32f sumQpf = 0.0;
        Ipp32f bcmplx = 1.0;
        Ipp32f fcmplx = 1.0;
        Ipp32f tcmplx = 1.0;
        Ipp32f bcmplxd = 1.0;
        Ipp32f bcmplxnnz = 1.0;
        for(Ipp32s i = 0; i < (int) par->PicHeightInCtbs; i++) {
            for(Ipp32s j = 0; j < (int) par->PicWidthInCtbs; j++) {
                if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel < 1) {
                    sumQpb += ctbStats[i*par->PicWidthInCtbs+j].dqp;
                    bcmplx += ctbStats[i*par->PicWidthInCtbs+j].complexity;
                    bcmplxd += (ctbStats[i*par->PicWidthInCtbs+j].complexity*dB[ctbStats[i*par->PicWidthInCtbs+j].dqp]);
                    if (ctbStats[i*par->PicWidthInCtbs + j].dqp > 0) {
                        bcmplxnnz += ctbStats[i*par->PicWidthInCtbs + j].complexity;
                        bnnz++;
                    }
                } else {
                    sumQpf += ctbStats[i*par->PicWidthInCtbs+j].dqp;
                    fcmplx += ctbStats[i*par->PicWidthInCtbs+j].complexity;
                }
                tcmplx += ctbStats[i*par->PicWidthInCtbs+j].complexity;
            }
        }
        
        // Balance
        if(fn != 0) {
            Ipp32f dqpb_act = bn ? sumQpb / bn : 0;
            Ipp32f dqpb_nnz = bnnz ? sumQpb / bnnz : 0;
            Ipp32f dqpf_act = sumQpf/fn;
            // compute
            Ipp32f fcmplxd = tcmplx - bcmplxd;
            Ipp32f dqf = fcmplxd/fcmplx;
            Ipp32f dqpf = (log(dqf) / l2f) * -6.0f;

            Ipp32s iter = 0;
            Ipp32s num_changed = 1;

            if(abs(dqpf_act - dqpf) > 0.1) {
                // ROI
                Ipp32s fOff = ((dqpf<dqpf_act)?1:-1);
                Ipp32s fOff1 = fOff;
                while(abs(dqpf_act - dqpf) > 0.01 && num_changed != 0 && iter < 14) {
                    Ipp32f sumQpfLast = sumQpf;
                    sumQpf = 0.0;
                    num_changed = 0;
                    fcmplxd = 1.0;

                    for(Ipp32s i = (int) par->PicHeightInCtbs -1; i >= 0 ; i--) {
                        for(Ipp32s j = (int) par->PicWidthInCtbs -1; j >= 0 ; j--) {
                            if(ctbStats[i*par->PicWidthInCtbs+j].roiLevel > 0) {
                                Ipp32s dqpo = ctbStats[i*par->PicWidthInCtbs+j].dqp;
                                Ipp32s dqp =  dqpo - fOff;
                                dqp = IPP_MAX(-1*qrange, dqp);
                                dqp = IPP_MIN(ctbStats[i*par->PicWidthInCtbs+j].segCount?-1:maxFq, dqp);

                                Ipp32f dqpd = dqp - dqpo;
                                ctbStats[i*par->PicWidthInCtbs+j].dqp = dqp;
                                sumQpf += dqp;
                                if(dqp != dqpo) num_changed++;
                                fcmplxd += (ctbStats[i*par->PicWidthInCtbs+j].complexity*dF[-1*ctbStats[i*par->PicWidthInCtbs+j].dqp]);
                                // Run Time Exit
                                sumQpfLast += dqpd;
                                Ipp32f dqpf_run = sumQpfLast/fn;
                                if(abs(dqpf_run - dqpf) <= 0.01) 
                                    fOff = 0;
                            }
                        }
                    }
                    if(fni && dqpf>-1.0 && num_changed==0 && maxFq!=0) {
                        // non HROI aq does not need -1 ROI guarantee
                        maxFq = 0;
                        num_changed = 1;
                    }
                    iter++;
                    dqpf_act = sumQpf/fn;
                    fOff = ((dqpf<dqpf_act)?1:-1);
                    if(fOff!=fOff1) iter=14;
                }
                if (par->bitDepthLumaShift == 0) {
                    // BG 8bit Simple
                    if ((dqpf_act - dqpf) > 0.01 && (num_changed == 0 || iter == 14)) {
                        // compute
                        bcmplxd = tcmplx - fcmplxd;
                        Ipp32f dqb = bcmplxd / bcmplx;
                        Ipp32f dqpb = (log(dqb) / l2f) * -6.0f;

                        iter = 0;
                        num_changed = 1;

                        while ((dqpb_act - dqpb) > 0 && num_changed != 0 & iter < 14) {
                            sumQpb = 0.0;
                            num_changed = 0;
                            for (Ipp32s i = 0; i < (int)par->PicHeightInCtbs; i++) {
                                for (Ipp32s j = 0; j < (int)par->PicWidthInCtbs; j++) {
                                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                                        Ipp32s dqpo = ctbStats[i*par->PicWidthInCtbs + j].dqp;
                                        Ipp32s dqp = dqpo - 1;
                                        dqp = IPP_MAX(minBq, dqp);
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp = dqp;
                                        sumQpb += dqp;
                                        if (dqp != dqpo) num_changed++;
                                    }
                                }
                            }
                            iter++;
                            dqpb_act = sumQpb / bn;
                        }
                    }
                } else {
                    // BG 10bit complex
                    if ((dqpf_act - dqpf) > 0.01 && (num_changed == 0 || iter == 14)) {
                        // compute
                        Ipp32f bcmplxd1 = fcmplxd - fcmplx;
                        Ipp32f dqb = (bcmplxnnz - bcmplxd1) / bcmplxnnz;
                        Ipp32f dqpb = (log(dqb) / l2f) * -6.0f;

                        iter = 0;
                        num_changed = 1;
                        Ipp32s bOff = 1;
                        while (bnnz && (dqpb_nnz - dqpb) > 0 && num_changed != 0 & iter < 14) {
                            Ipp32f sumQpbLast = sumQpb;
                            sumQpb = 0.0;
                            num_changed = 0;
                            for (Ipp32s i = 0; i < (int)par->PicHeightInCtbs; i++) {
                                for (Ipp32s j = 0; j < (int)par->PicWidthInCtbs; j++) {
                                    if (ctbStats[i*par->PicWidthInCtbs + j].roiLevel < 1) {
                                        Ipp32s dqpo = ctbStats[i*par->PicWidthInCtbs + j].dqp;
                                        Ipp32s dqp = dqpo - bOff;
                                        dqp = IPP_MAX(minBq, dqp);
                                        Ipp32f dqpd = dqp - dqpo;
                                        ctbStats[i*par->PicWidthInCtbs + j].dqp = dqp;
                                        sumQpb += dqp;
                                        if (dqp != dqpo) num_changed++;
                                        // Run Time Exit
                                        sumQpbLast += dqpd;
                                        Ipp32f dqpb_run = sumQpbLast / bnnz;
                                        if (dqpb_run - dqpb < 0)
                                            bOff = 0;
                                    }
                                }
                            }
                            iter++;
                            dqpb_nnz = sumQpb / bnnz;
                        }
                    }
                }
            }
        } // Balance
    }

    void ApplyHRoiDeltaQp(Frame* frame, const H265VideoParam &par, Ipp8u useBrc)
    {
        Ipp32s numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs; 
        Statistics* stats = frame->m_stats[0];

        setRoiDeltaQP(&par, frame, useBrc); 

        Ipp32s hasRoi = 0;
        if(par.DeltaQpMode & AMT_DQP_HROI) {
            if(stats->roi_pic.numCtbRoi[1]+stats->roi_pic.numCtbRoi[2])
                hasRoi = 1;
        }

        Ipp32s minqp = (8 - par.bitDepthLuma) * 6;
        Ipp8s *pLcuQP = &(frame->m_lcuQps[0][0]);

        for (Ipp32s ctb = 0; ctb < numCtb; ctb++) 
        {
            Ipp32s dqp = stats->ctbStats[ctb].dqp;
            // assign PAQ deltaQp
            if (par.DeltaQpMode&AMT_DQP_PAQ) {
                Ipp32s padqp = stats->qp_mask[0][ctb];
#ifdef LOW_COMPLX_PAQ
                if (padqp < 0 && frame->m_sliceQpY - MAX_DQP < 16) {
                    padqp = IPP_MIN(IPP_MAX(15, frame->m_sliceQpY + padqp) - frame->m_sliceQpY, 0);
                    stats->qp_mask[0][ctb] = padqp;
                }
#endif
                if(hasRoi) {
                    if(stats->ctbStats[ctb].roiLevel) {
                        dqp = IPP_MIN(0, dqp+padqp);
                    } else {
                        dqp += padqp;
                    }
                } else {
                    dqp += padqp;
                }
            }
            Ipp32s qp;
            qp = frame->m_sliceQpY + dqp;
            qp = Saturate(minqp, 51, qp);

            pLcuQP[ctb] = (Ipp8s)qp;
        }

#ifdef LOW_COMPLX_PAQ
        if (par.DeltaQpMode&AMT_DQP_PAQ) {
            Ipp32f avgDQp = std::accumulate(stats->qp_mask[0].begin(), stats->qp_mask[0].end(), 0);
            avgDQp /= stats->qp_mask[0].size();
            stats->m_avgDPAQ = avgDQp;
        }
#endif

        bool IsHiCplxGOP = false;
        bool IsMedCplxGOP = false;
        GetGopComplexity(par, frame, IsHiCplxGOP, IsMedCplxGOP);

        CostType rd_lamba_multiplier;
        bool extraMult = SliceLambdaMultiplier(rd_lamba_multiplier, par, frame->m_slices[0].slice_type, frame, IsHiCplxGOP, IsMedCplxGOP, true);

        Ipp32s numQpValues = 52 + (par.bitDepthLuma - 8)*6;
        frame->m_roiSlice.resize(numQpValues);
        for (Ipp32s i = 0; i < numQpValues; i++)
            SetSliceLambda(par, &frame->m_roiSlice[i], i -  (par.bitDepthLuma - 8)*6, frame, rd_lamba_multiplier, extraMult);

        ApplyDeltaQpOnRoi(frame, par);

    }

#endif

    void ApplyRoiDeltaQp(Frame* frame, const H265VideoParam & par)
    {
        Ipp32s numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;
        H265Slice slice;
        
        for (Ipp32s i = par.numRoi - 1; i >= 0; i--) {
            Ipp32s start = par.roi[i].left / par.MaxCUSize + par.roi[i].top / par.MaxCUSize * par.PicWidthInCtbs;
            Ipp32s end = par.roi[i].right / par.MaxCUSize + par.roi[i].bottom / par.MaxCUSize * par.PicWidthInCtbs;

            Ipp32s qp = frame->m_sliceQpY + par.roi[i].priority;
            Ipp32s minqp = (8 - par.bitDepthLuma) * 6;
            qp = Saturate(minqp, 51, qp);

            for (Ipp32s ctb = start; ctb <= end; ctb++) {
                Ipp32s col =  (ctb % par.PicWidthInCtbs) * par.MaxCUSize;
                Ipp32s row =  (ctb / par.PicWidthInCtbs) * par.MaxCUSize;
                if (col >= par.roi[i].left && col < par.roi[i].right && row >= par.roi[i].top && row < par.roi[i].bottom) {
                    //Ipp32s qp = frame->m_lcuQps[0][ctb] + par.roi[i].priority;
                    //Ipp32s minqp = (8 - par.bitDepthLuma) * 6;
                    //qp = Saturate(minqp, 51, qp);
                    frame->m_lcuQps[0][ctb] = qp;
                }
            }
        }

        CostType rd_lamba_multiplier;
        bool extraMult = SliceLambdaMultiplier(rd_lamba_multiplier, par, frame->m_slices[0].slice_type, frame, 0, 0, false);

        Ipp32s numQpValues = 52 + (par.bitDepthLuma - 8)*6;
        frame->m_roiSlice.resize(numQpValues);
        for (Ipp32s i = 0; i < numQpValues; i++)
            SetSliceLambda(par, &frame->m_roiSlice[i], i -  (par.bitDepthLuma - 8)*6, frame, rd_lamba_multiplier, extraMult);

        ApplyDeltaQpOnRoi(frame, par);
    }

}  // namespace H265Enc

static Ipp8u dpoc_neg[][16][6] = {
    //GopRefDist 4
    {{2, 4, 2},
    {1, 1},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{4, 8, 2, 2, 4}, //poc 8
    {1, 1},           //poc 1
    {2, 2, 2},
    {2, 1, 2},
    {3, 4, 2, 2},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4}},

    //GopRefDist 16.
    // it isn't BPyramid. it is "anchor & chain"
    {{5, 16, 2, 2, 4, 8}, //poc 16
    {2, 1, 16},           // poc 1
    {3, 2, 4, 12},
    {3, 1, 2, 16},
    {4, 4, 4, 4, 8},
    {2, 1, 4},
    {3, 2, 2, 2},
    {3, 1, 2, 4},
    {4, 4, 2, 2, 16},
    {2, 1, 8},
    {3, 2, 2, 6},
    {3, 1, 2, 8},
    {4, 4, 2, 2, 4},
    {3, 1, 4, 8},
    {4, 2, 2, 2, 8},
    {4, 1, 2, 4, 8}}  // poc 15
};

static Ipp8u dpoc_pos[][16][6] = {
    //GopRefDist 4
    {{0},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 8
    {{0,},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}},

    //GopRefDist 16
    {{0,},        // poc 16
    {3, 1, 2, 12},// poc 1
    {2, 2, 12},
    {2, 1, 12},
    {1, 12},
    {3, 1, 2, 8},
    {2, 2, 8},
    {2, 1, 8},
    {1, 8},
    {3, 1, 2, 4},
    {2, 2, 4},
    {2, 1, 4},
    {1, 4},
    {2, 1, 2},
    {1, 2},
    {1, 1}} // poc 15
};

void H265Encoder::InitShortTermRefPicSet()
{
    Ipp32s gopRefDist = m_videoParam.GopRefDist;
    Ipp32s *maxRefP = m_videoParam.MaxRefIdxP;
    Ipp32s *maxRefB = m_videoParam.MaxRefIdxB;

    Ipp32u numShortTermRefPicSets = 0;

    m_sps.enableLTR = m_videoParam.enableLTR;

    if (m_videoParam.enableLTR)
    {
        if (gopRefDist == 1) {
            Ipp8u numNegPics[4] = { 1, 2, 3, 4};
            m_sps.num_short_term_ref_pic_sets = 4;
            for (Ipp32s i = 0; i < 4; i++) {
                m_sps.m_shortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
                m_sps.m_shortRefPicSet[i].num_negative_pics = numNegPics[i];
                m_sps.m_shortRefPicSet[i].num_positive_pics = 0;
                for (Ipp8u j = 0; j < numNegPics[i]; j++) {
                    m_sps.m_shortRefPicSet[i].delta_poc[j] = 1;
                    m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
                }
            }
            return;
        }
    }

    if (m_videoParam.BiPyramidLayers > 1 && gopRefDist > 1) {
        if (!(gopRefDist ==  4 && maxRefP[0] == 2 && maxRefB[0] == 1 && maxRefB[1] == 1) &&
            !(gopRefDist ==  8 && maxRefP[0] == 4 && maxRefB[0] == 2 && maxRefB[1] == 2) &&
            !(gopRefDist == 16 && maxRefP[0] == 5 && maxRefB[0] == 2 && maxRefB[1] == 2))
        {
            // no rps in sps for non-standard pyramids
            m_sps.num_short_term_ref_pic_sets = 0;
            return;
        }

        Ipp8u r = (gopRefDist == 4) ? 0 : (gopRefDist == 8) ? 1 : 2;
        for (Ipp32s i = 0; i < gopRefDist; i++) {
            m_sps.m_shortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[i].num_negative_pics = dpoc_neg[r][i][0];
            m_sps.m_shortRefPicSet[i].num_positive_pics = dpoc_pos[r][i][0];

            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_negative_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[j] = dpoc_neg[r][i][1 + j];
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_positive_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = dpoc_pos[r][i][1 + j];
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = 1;
            }
            numShortTermRefPicSets++;
        }
    }
    else {
        Ipp32s deltaPoc = gopRefDist;

        for (Ipp32s i = 0; i < m_videoParam.PGopPicSize; i++) {
            Ipp32s idx = i == 0 ? 0 : gopRefDist - 1 + i;
            m_sps.m_shortRefPicSet[idx].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[idx].num_negative_pics = maxRefP[0];
            m_sps.m_shortRefPicSet[idx].num_positive_pics = 0;
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[idx].num_negative_pics; j++) {
                Ipp32s deltaPoc_cur;
                if (j == 0)
                    deltaPoc_cur = 1;
                else if (j == 1)
                    deltaPoc_cur = (m_videoParam.PGopPicSize - 1 + i) % m_videoParam.PGopPicSize;
                else
                    deltaPoc_cur = m_videoParam.PGopPicSize;
                if (deltaPoc_cur == 0)
                    deltaPoc_cur =  m_videoParam.PGopPicSize;
                m_sps.m_shortRefPicSet[idx].delta_poc[j] = deltaPoc_cur * gopRefDist;
                m_sps.m_shortRefPicSet[idx].used_by_curr_pic_flag[j] = 1;
            }
            numShortTermRefPicSets++;
        }

        for (Ipp32s i = 1; i < gopRefDist; i++) {
            m_sps.m_shortRefPicSet[i].inter_ref_pic_set_prediction_flag = 0;
            m_sps.m_shortRefPicSet[i].num_negative_pics = maxRefB[0];
            m_sps.m_shortRefPicSet[i].num_positive_pics = maxRefB[1];

            m_sps.m_shortRefPicSet[i].delta_poc[0] = i;
            m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[0] = 1;
            for (Ipp32s j = 1; j < m_sps.m_shortRefPicSet[i].num_negative_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[j] = deltaPoc;
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[j] = 1;
            }
            for (Ipp32s j = 0; j < m_sps.m_shortRefPicSet[i].num_positive_pics; j++) {
                m_sps.m_shortRefPicSet[i].delta_poc[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = gopRefDist - i;
                m_sps.m_shortRefPicSet[i].used_by_curr_pic_flag[m_sps.m_shortRefPicSet[i].num_negative_pics + j] = 1;
            }
            numShortTermRefPicSets++;
        }
    }

    m_sps.num_short_term_ref_pic_sets = (Ipp8u)numShortTermRefPicSets;
}

// AMT_LTR
Ipp32s computeLSB(Ipp32s poc, Ipp32s pocLsbMax)
{
    if (poc >= 0) {
        return (poc % pocLsbMax);
    }
    else {
        return  ((pocLsbMax - ((-poc) % pocLsbMax)) % pocLsbMax);
    }
}

void setDeltaPocMsbCycleLtr(Ipp32s pocCurr, Ipp32s pocLtr, Ipp32s pocLsbMax, Ipp8u &deltaPocMsbPresentFlag, Ipp32s &deltaPocMsbCycleLtr)
{
    Ipp32s pocLsbLtr = computeLSB(pocLtr, pocLsbMax);
    Ipp32s pocMsbLtr = pocLtr - pocLsbLtr;
    Ipp32s pocLsbCurr = computeLSB(pocCurr, pocLsbMax);
    Ipp32s pocMsbCurr = pocCurr - pocLsbCurr;
    Ipp32s deltaPocMsbLtr = pocMsbCurr - pocMsbLtr;

    deltaPocMsbCycleLtr = (pocMsbCurr - pocMsbLtr) / pocLsbMax;
//    if (deltaPocMsbCycleLtr < 0) printf("ERROR: POC of LTR is greater than current pic\n");
    deltaPocMsbPresentFlag = (pocLsbLtr == pocLtr) ? 0 : 1;  // (pocMsbLtr > 0) ? 1 : 0;
}

void H265Encoder::InitLongTermRefPicSet()
{
    m_sps.long_term_ref_pics_present_flag = m_videoParam.enableLTR ? 1 : 0;

    m_sps.num_long_term_ref_pics_sps = 1;
    m_sps.m_LtRpsSps[0].lt_ref_pic_poc_lsb = 0;
    m_sps.m_LtRpsSps[0].used_by_curr_pic_lt_flag = 1;

    m_currLtrStatus = m_videoParam.enableLTR ? 1 : 0;
}

Ipp32s getMatchingLtrIdxFromRpsSet(H265LongTermRefPicSetSps *longRefPicSet, Ipp32s N, Ipp32s pocLsbLtr, Ipp8u usedFlag )
{
    for (Ipp32s i = 0; i < N; i++) {
        if (longRefPicSet[i].lt_ref_pic_poc_lsb == pocLsbLtr  && longRefPicSet[i].used_by_curr_pic_lt_flag == usedFlag)
            return i;
    }

    return -1;
}

DispatchSaoApplyFilter::DispatchSaoApplyFilter()
{
    m_bitDepth = 0;//unknown
    m_sao     = NULL;
    m_sao10bit = NULL;
}

DispatchSaoApplyFilter::~DispatchSaoApplyFilter()
{
    Close();
}

mfxStatus DispatchSaoApplyFilter::Init(int maxCUWidth, int format, int maxDepth, int bitDepth, int num)
{
    if (m_bitDepth > 0) { //was inited!!!
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_bitDepth = bitDepth;
    if (m_bitDepth == 8) {
        m_sao = new SaoApplier<Ipp8u>;
        MFX_CHECK_NULL_PTR1(m_sao);
        m_sao->Init(maxCUWidth, format, maxDepth, bitDepth, num);

    } else if (m_bitDepth == 10) {
        m_sao10bit = new SaoApplier<Ipp16u>;
        MFX_CHECK_NULL_PTR1(m_sao10bit);
        m_sao10bit->Init(maxCUWidth, format, maxDepth, bitDepth, num);

    } else {
        m_bitDepth = 0;
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

void DispatchSaoApplyFilter::Close()
{
    if (m_bitDepth == 8) {
        delete m_sao;
        m_sao = NULL;
    } else if(m_bitDepth == 10) {
        delete m_sao10bit;
        m_sao10bit = NULL;
    }

    m_bitDepth = 0;//unknown
}


H265FrameEncoder::H265FrameEncoder(H265Encoder& topEnc)
    : m_topEnc(topEnc)
    , m_videoParam(topEnc.m_videoParam)
    , m_bsf(topEnc.m_bsf)
    , data_temp(topEnc.data_temp)
    , memBuf(NULL)
    , m_bs(NULL)
    , m_saoParam(NULL)
    , m_context_array_wpp_enc(NULL)
    , m_context_array_wpp(NULL)
    , m_costStat(NULL)
    , m_coeffWork(0)
    , m_frame(NULL)
    , data_temp_size(0)
    , m_numTasksPerCu(0)
{
}

mfxStatus H265FrameEncoder::Init()
{
    Ipp32u numCtbs = m_videoParam.PicWidthInCtbs*m_videoParam.PicHeightInCtbs;
    Ipp32s sizeofH265CU = (m_videoParam.bitDepthLuma > 8 ? sizeof(H265CU<Ipp16u>) : sizeof(H265CU<Ipp8u>));
    data_temp_size = ((MAX_TOTAL_DEPTH * 2 + 2) << m_videoParam.Log2NumPartInCU);

    // [1] calc mem size to allocate
    Ipp32s bitPerPixel = m_videoParam.bitDepthLuma + (8 * m_videoParam.bitDepthChroma >> m_videoParam.chromaShift) / 4;
    Ipp32u streamBufSizeMain = m_videoParam.Width * m_videoParam.Height * bitPerPixel / 8 + DATA_ALIGN;
    Ipp32u streamBufSize;
    if (m_videoParam.NumTiles > 1)
        streamBufSize = (m_videoParam.MaxCUSize * m_videoParam.MaxCUSize * m_videoParam.tileColWidthMax * m_videoParam.tileRowHeightMax) * bitPerPixel / 8 + DATA_ALIGN;
    else
        streamBufSize = (m_videoParam.Width * (m_videoParam.WPPFlag ? m_videoParam.MaxCUSize :
            ((m_videoParam.PicHeightInCtbs / m_videoParam.NumSlices + 1) * m_videoParam.MaxCUSize))) * bitPerPixel * 2 / 8 + DATA_ALIGN;


    Ipp32u memSize = 0;
    // m_bs
    memSize += sizeof(H265BsReal)*(m_videoParam.num_bs_subsets + 1) + DATA_ALIGN;
    memSize += streamBufSize*m_videoParam.num_bs_subsets + streamBufSizeMain; // data of m_bs* and m_bsFake*
    memSize += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs * 2; // cabac
    // m_costStat
    memSize += numCtbs * sizeof(costStat) + DATA_ALIGN;
    // coeffWork
    memSize += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;
    // sao
    if (m_videoParam.SAOFlag) {
        memSize += sizeof(SaoCtuParam) * m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs + DATA_ALIGN;
        //m_cu = UMC::align_pointer<void*>(ptr, 0x1000);
        //ptr += sizeofH265CU * m_videoParam.num_thread_structs + 0x1000;
    }


    // allocation
    memBuf = (Ipp8u *)H265_Malloc(memSize);
    MFX_CHECK_STS_ALLOC(memBuf);


    // memory setting/mapping
    Ipp8u *ptr = memBuf;
    // m_bs
    m_bs = UMC::align_pointer<H265BsReal*>(ptr, DATA_ALIGN);
    ptr += sizeof(H265BsReal)*(m_videoParam.num_bs_subsets + 1) + DATA_ALIGN;

    // m_bs data
    for (Ipp32u i = 0; i < m_videoParam.num_bs_subsets+1; i++) {
        m_bs[i].m_base.m_pbsBase = ptr;
        m_bs[i].m_base.m_maxBsSize = streamBufSize;
        m_bs[i].Reset();
        ptr += streamBufSize;
    }
    m_bs[m_videoParam.num_bs_subsets].m_base.m_maxBsSize = streamBufSizeMain;
    ptr += Ipp32s(streamBufSizeMain - streamBufSize);

    for (Ipp32u i = 0; i < m_videoParam.num_thread_structs; i++) {
        m_bsf[i].Reset();
    }

    m_context_array_wpp = ptr;
    ptr += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;

    m_context_array_wpp_enc = ptr;
    ptr += sizeof(CABAC_CONTEXT_H265) * NUM_CABAC_CONTEXT * m_videoParam.PicHeightInCtbs;

    m_costStat = (costStat *)ptr;
    ptr += numCtbs * sizeof(costStat);

    m_coeffWork = UMC::align_pointer<CoeffsType*>(ptr, DATA_ALIGN);
    //ptr += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;

    if (m_videoParam.SAOFlag) {
        ptr += sizeof(CoeffsType) * (numCtbs << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift) + DATA_ALIGN;
        m_saoParam = (SaoCtuParam*)ptr;
        ptr += sizeof(SaoCtuParam) * m_videoParam.PicHeightInCtbs * m_videoParam.PicWidthInCtbs + DATA_ALIGN;
    }


    if (m_videoParam.SAOFlag) {
        Ipp32s compCount = m_videoParam.SAOChromaFlag ? 3 : 1;
        for (Ipp32s compId = 0; compId < compCount; compId++) {
            m_saoApplier[compId].Init(m_videoParam.MaxCUSize, m_videoParam.chromaFormatIdc, 0,
                compId < 1 ? m_videoParam.bitDepthLuma : m_videoParam.bitDepthChroma,
                (m_videoParam.PicWidthInCtbs + 1) * (m_videoParam.PicHeightInCtbs + 1));
        }
    }

    return MFX_ERR_NONE;
}


void H265FrameEncoder::Close()
{

    if (m_videoParam.SAOFlag) {
        Ipp32s compCount = m_videoParam.SAOChromaFlag ? 3 : 1;
        for (Ipp32s compId = 0; compId < compCount; compId++) {
            m_saoApplier[compId].Close();
        }
    }

    if (memBuf) {
        H265_Free(memBuf);
        memBuf = NULL;
    }

} // void H265FrameEncoder::Close()

mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
{
    VM_ASSERT(displayOrder >= begin);
    VM_ASSERT(displayOrder <  end);

    ref = (end - begin > 1);

    mfxU32 pivot = (begin + end) / 2;
    if (displayOrder == pivot)
        return counter;
    else if (displayOrder < pivot)
        return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
    else
        return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
};

//TO BE DONE
// in display order
Ipp32s TAB_BPYRAMID_LAYER[16][15] = {
    {1},
    {1},
    {2,1},//2
    {2,1,2},//3
    {3,2,1,2},//4
    {3,2,1,3,2},//5

    {3,2,3,1,3,2},//6
    {3,2,3,1,3,2,3},//7

    {4,3,2,3,1,3,2,3},//8

    {4,3,2,3,1,4,3,2,3},//9
    {4,3,2,4,3,1,4,3,2,3},//10

    {4,3,2,3,1,4,3,2,3},//11
    {4,3,2,4,3,1,4,3,4,2,4,3},//12

    {4,3,2,4,3,4,1,4,3,4,2,4,3},//13
    {4,3,4,2,4,3,4,1,4,3,4,2,4,3},//14
    {4,3,4,2,4,3,4,1,4,3,4,2,4,3,4}//15
};

struct isEqual
{
    isEqual(Ipp32s frameOrder): m_frameOrder(frameOrder)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_frameOrder == m_frameOrder;
    }

    Ipp32s m_frameOrder;
};

void H265Encoder::ConfigureInputFrame(Frame* frame, bool bEncOrder, bool bEncCtrl) const
{
    if (!bEncOrder)
        frame->m_frameOrder = m_frameOrder;

    frame->m_frameOrderOfLastIdr = m_frameOrderOfLastIdr;
    frame->m_frameOrderOfLastIntra = m_frameOrderOfLastIntra;
    frame->m_frameOrderOfLastAnchor = m_frameOrderOfLastAnchor;

    if (bEncOrder || !bEncCtrl) {
        frame->m_poc = frame->m_frameOrder - frame->m_frameOrderOfLastIdr;
        frame->m_isIdrPic = false;
    }

    if (!bEncOrder && !bEncCtrl) {
        Ipp32s idrDist = m_videoParam.GopPicSize * m_videoParam.IdrInterval;
        if (m_videoParam.picStruct == PROGR) {
            if (frame->m_frameOrder == 0 || idrDist > 0 && (frame->m_frameOrder - frame->m_frameOrderOfLastIdr) % idrDist == 0) {
                frame->m_isIdrPic = true;
                frame->m_picCodeType = MFX_FRAMETYPE_I;
                frame->m_poc = 0;
            } else if (m_videoParam.GopPicSize > 0 && (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.GopPicSize == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_I;
            } else if ((frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_P;
            } else {
                frame->m_picCodeType = MFX_FRAMETYPE_B;
            }
        } else {
            if (frame->m_frameOrder/2 == 0 || idrDist > 0 && (frame->m_frameOrder/2 - frame->m_frameOrderOfLastIdr/2) % idrDist == 0) {
                if (frame->m_frameOrder & 1) {
                    frame->m_picCodeType = MFX_FRAMETYPE_P;
                } else {
                    frame->m_isIdrPic = true;
                    frame->m_picCodeType = MFX_FRAMETYPE_I;
                    frame->m_poc = 0;
                }
            } else if (m_videoParam.GopPicSize > 0 && (frame->m_frameOrder/2 - frame->m_frameOrderOfLastIntra/2) % m_videoParam.GopPicSize == 0) {
                if (m_videoParam.GopRefDist == 1) {
                    if (frame->m_frameOrder & 1)
                        frame->m_picCodeType = MFX_FRAMETYPE_P;
                    else
                        frame->m_picCodeType = MFX_FRAMETYPE_I;
                }
                else {
                    if (frame->m_frameOrder & 1)
                        frame->m_picCodeType = MFX_FRAMETYPE_I;
                    else
                        frame->m_picCodeType = MFX_FRAMETYPE_P;
                }
            } else if ((frame->m_frameOrder/2 - frame->m_frameOrderOfLastAnchor/2) % m_videoParam.GopRefDist == 0) {
                frame->m_picCodeType = MFX_FRAMETYPE_P;
            } else {
                frame->m_picCodeType = MFX_FRAMETYPE_B;
            }
        }
    } else if (bEncOrder) {
        if (frame->m_picCodeType & MFX_FRAMETYPE_IDR) {
            frame->m_picCodeType = MFX_FRAMETYPE_I;
            frame->m_isIdrPic = true;
            frame->m_poc = 0;
            frame->m_isRef = 1;
        } else if (frame->m_picCodeType & MFX_FRAMETYPE_REF) {
            frame->m_picCodeType &= ~MFX_FRAMETYPE_REF;
            frame->m_isRef = 1;
        }
    }

    if (!bEncOrder) {
        Ipp64f frameDuration = (Ipp64f)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000;
        if (frame->m_timeStamp == MFX_TIMESTAMP_UNKNOWN)
            frame->m_timeStamp = (m_lastTimeStamp == MFX_TIMESTAMP_UNKNOWN) ? 0 : Ipp64s(m_lastTimeStamp + frameDuration);

        frame->m_RPSIndex = (m_videoParam.GopRefDist > 1)
            ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist
            : (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.PGopPicSize;
        frame->m_miniGopCount = m_miniGopCount + !(frame->m_picCodeType == MFX_FRAMETYPE_B);

        if (m_videoParam.PGopPicSize > 1) {
            frame->m_pyramidLayer = h265_pgop_layers[frame->m_RPSIndex];
        }
    } else {
        Ipp64f frameDuration = (Ipp64f)m_videoParam.FrameRateExtD / m_videoParam.FrameRateExtN * 90000;
        if (frame->m_timeStamp == MFX_TIMESTAMP_UNKNOWN)
            frame->m_timeStamp = (m_lastTimeStamp == MFX_TIMESTAMP_UNKNOWN) ? 0 : Ipp64s(m_lastTimeStamp + frameDuration * (frame->m_frameOrder - m_frameOrder));

        if (frame->m_picCodeType != MFX_FRAMETYPE_B) {
            frame->m_miniGopCount = m_miniGopCount - (frame->m_picCodeType == MFX_FRAMETYPE_B);
        } else {
            Ipp32s miniGopCountOfAnchor = m_miniGopCount;
            FrameList* queue[3] = {(FrameList*)&m_inputQueue, (FrameList*)&m_reorderedQueue, (FrameList*)&m_dpb};
            for (Ipp32s idx = 0; idx < 3; idx++) {
                FrameIter it = std::find_if(queue[idx]->begin(), queue[idx]->end(), isEqual(frame->m_frameOrderOfLastAnchor));
                if (it != queue[idx]->end()) {
                    miniGopCountOfAnchor = (*it)->m_miniGopCount;
                    break;
                }
            }
            frame->m_miniGopCount = miniGopCountOfAnchor + !(frame->m_picCodeType == MFX_FRAMETYPE_B);
        }

        frame->m_biFramesInMiniGop = m_LastbiFramesInMiniGop;

        //frame->m_RPSIndex = (frame->m_picCodeType == MFX_FRAMETYPE_B) ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) : 0;
        frame->m_RPSIndex = (m_videoParam.GopRefDist > 1)
            ? (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist
            : (frame->m_frameOrder - frame->m_frameOrderOfLastIntra) % m_videoParam.PGopPicSize;
        if (m_videoParam.PGopPicSize > 1) {
            frame->m_pyramidLayer = h265_pgop_layers[frame->m_RPSIndex];
        }

        Ipp32s biPyramid = m_videoParam.BiPyramidLayers > 1;
        Ipp32s posB = (frame->m_frameOrder - frame->m_frameOrderOfLastAnchor) % m_videoParam.GopRefDist;
        if (posB && frame->m_picCodeType == MFX_FRAMETYPE_B && biPyramid) {
            //printf("\n B frame: frameOrder =  %i numB = %i \n", frame->m_frameOrder, posB);
            frame->m_pyramidLayer = TAB_BPYRAMID_LAYER[m_videoParam.GopRefDist-1][posB-1];
        }
    }
}


void H265Encoder::UpdateGopCounters(Frame *inputFrame, bool bEncOrder)
{
    if (!bEncOrder) {
        m_frameOrder++;
        m_lastTimeStamp = inputFrame->m_timeStamp;

        if (inputFrame->m_isIdrPic)
            m_frameOrderOfLastIdr = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType == MFX_FRAMETYPE_I)
            m_frameOrderOfLastIntra = inputFrame->m_frameOrder;
        if (inputFrame->m_picCodeType != MFX_FRAMETYPE_B) {
            m_frameOrderOfLastAnchor = inputFrame->m_frameOrder;
            m_miniGopCount++;
        }
    }
    else {
        m_frameOrder    = inputFrame->m_frameOrder;
        m_lastTimeStamp = inputFrame->m_timeStamp;

        if (!(inputFrame->m_picCodeType & MFX_FRAMETYPE_B)) {
            if (inputFrame->m_isIdrPic)
                m_frameOrderOfLastIdrB = inputFrame->m_frameOrder;
            if (inputFrame->m_picCodeType & MFX_FRAMETYPE_I)
                m_frameOrderOfLastIntraB = inputFrame->m_frameOrder;

            m_frameOrderOfLastAnchorB = inputFrame->m_frameOrder;
            m_LastbiFramesInMiniGop = m_frameOrderOfLastAnchorB  -  m_frameOrderOfLastAnchor - 1;
            assert(m_LastbiFramesInMiniGop < m_videoParam.GopRefDist);
            m_miniGopCount++;
        }
    }

}


void H265Encoder::RestoreGopCountersFromFrame(Frame *frame, bool bEncOrder)
{
    assert (!bEncOrder);
    // restore global state
    m_frameOrder = frame->m_frameOrder;
    m_frameOrderOfLastIdr = frame->m_frameOrderOfLastIdr;
    m_frameOrderOfLastIntra = frame->m_frameOrderOfLastIntra;
    m_frameOrderOfLastAnchor = frame->m_frameOrderOfLastAnchor;
    m_miniGopCount = frame->m_miniGopCount - !(frame->m_picCodeType == MFX_FRAMETYPE_B);
}

namespace {
    bool PocIsLess(const Frame *f1, const Frame *f2) { return f1->m_poc < f2->m_poc; }
    bool PocIsGreater(const Frame *f1, const Frame *f2) { return f1->m_poc > f2->m_poc; }

    void ModifyRefList(const H265Enc::H265VideoParam &par, Frame &currFrame, Ipp32s listIdx)
    {
        RefPicList &list = currFrame.m_refPicList[listIdx];
        Frame **refs = list.m_refFrames;
        Ipp8s *mod = list.m_listMod;
        Ipp8s *pocs = list.m_deltaPoc;
        Ipp8u *lterms = list.m_isLongTermRef;

        list.m_listModFlag = 0;
        for (Ipp32s i = 0; i < list.m_refFramesCount; i++)
            mod[i] = i;

        if (par.AdaptiveRefs) {
            // stable sort by pyramidLayer
            for (Ipp32s i = 2; i < list.m_refFramesCount; i++) {
                Ipp32s layerI = refs[i]->m_pyramidLayer;
                Ipp32s j = i;
                while (j > 1 && refs[j-1]->m_pyramidLayer > layerI) j--;
                if (j < i) {
                    std::rotate(refs + j, refs + i, refs + i + 1);
                    std::rotate(mod + j, mod + i, mod + i + 1);
                    std::rotate(pocs + j, pocs + i, pocs + i + 1);
                    std::rotate(lterms + j, lterms + i, lterms + i + 1);
                    list.m_listModFlag = 1;
                }
            }

            if (par.picStruct == PROGR) {
                // cut ref lists for non-ref frames
                if (!currFrame.m_isRef && list.m_refFramesCount > 1) {
                    Ipp32s layer0 = refs[0]->m_pyramidLayer;
                    for (Ipp32s i = 1; i < list.m_refFramesCount; i++) {
                        if (refs[i]->m_pyramidLayer >= layer0) {
                            list.m_refFramesCount = i;
                            break;
                        }
                    }
                }

                if (par.BiPyramidLayers == 4) {
                    // list is already sorted by m_pyramidLayer (ascending)
                    Ipp32s refLayerLimit = par.refLayerLimit[currFrame.m_pyramidLayer];
                    for (Ipp32s i = 1; i < list.m_refFramesCount; i++) {
                        if (refs[i]->m_pyramidLayer > refLayerLimit) {
                            list.m_refFramesCount = i;
                            break;
                        }
                    }
                }
            }
        }
    }
}

Ipp32s CountUniqueRefs(const RefPicList refLists[2]) {
    Ipp32s numUniqRefs = refLists[0].m_refFramesCount;
    for (Ipp32s r = 0; r < refLists[1].m_refFramesCount; r++)
        if (std::find(refLists[0].m_refFrames, refLists[0].m_refFrames + refLists[0].m_refFramesCount, refLists[1].m_refFrames[r])
            == refLists[0].m_refFrames + refLists[0].m_refFramesCount)
            numUniqRefs++;
    return numUniqRefs;
}

// AMT_LTR
void swapRefPicListData(RefPicList *pRefPicList, Ipp32s idx1, Ipp32s idx2)
{
    Frame *pFrame = pRefPicList->m_refFrames[idx1];
                    pRefPicList->m_refFrames[idx1] = pRefPicList->m_refFrames[idx2];
                    pRefPicList->m_refFrames[idx2] = pFrame;
    Ipp8s deltaPoc = pRefPicList->m_deltaPoc[idx1];
                    pRefPicList->m_deltaPoc[idx1] = pRefPicList->m_deltaPoc[idx2];
                    pRefPicList->m_deltaPoc[idx2] = deltaPoc;
    Ipp8u isLongTermRef = pRefPicList->m_isLongTermRef[idx1];
                    pRefPicList->m_isLongTermRef[idx1] = pRefPicList->m_isLongTermRef[idx2];
                    pRefPicList->m_isLongTermRef[idx2] = isLongTermRef;
}

// reduce one frame from frameList which has biggest pyramidLayer (keep picture order in the list)
Ipp8u reduceRefPicList(Frame **frameList, Ipp32s &numRef)
{
    if (numRef <= 1 )
        return 0;

    Ipp32s N0 = numRef;
    for (Ipp32s k = 1; k < N0; k++)
    {
        Ipp32s layer = frameList[k]->m_pyramidLayer;
        if (layer > 1) //  && frameList[i]->m_pyramidLayer < biPyramidLayersSize)
        {
            // shift the frames being followed
            for (Ipp32s i = k + 1; i < numRef; i++)
            {
                frameList[i - 1] = frameList[i];
            }

            numRef--;
        }
    }

    return 1;
}


// reduce one frame from frameList which has biggest pyramidLayer (keep picture order in the list)
Ipp8u reduceRefPicList(Frame **frameList, Ipp32s &numRef, Ipp32s maxNumRef)
{
    if (numRef <= 1 || numRef <= maxNumRef)
        return 0;

//    while (numRef > maxNumRef)
    for (Ipp32s k = numRef - 1; k > 0; k--)
    {
        if (numRef <= maxNumRef)
            break;

        Ipp32s layer = frameList[k]->m_pyramidLayer;
        if (layer > 0) //  && frameList[i]->m_pyramidLayer < biPyramidLayersSize)
        {
            // set the conditions for selected frame
            //    frameList[frameIdx]->m_isRef = 0;
            //    frameList[frameIdx]->m_isShortTermRef = 0;

            // shift the frames being followed
            for (Ipp32s i = k + 1; i < numRef; i++)
            {
                frameList[i - 1] = frameList[i];
            }

            numRef--;
        }
    }

    if (numRef > maxNumRef)
        numRef = maxNumRef;

    return 1;
}

Frame* reduce_ref_frame(Frame **frameList, Ipp32s &numRef)
{
    Frame *pRef = 0;

    // check condition to reduce
    if (numRef <= 1 )
        return pRef;

    for (Ipp32s k = numRef - 1; k > 0; k--)
    {
        Ipp32s layer = frameList[k]->m_pyramidLayer;
        if (layer > 1) //  && frameList[i]->m_pyramidLayer < biPyramidLayersSize)
        {
            pRef = frameList[k];
            // shift the frames being followed
            for (Ipp32s i = k + 1; i < numRef; i++)
            {
                frameList[i - 1] = frameList[i];
            }

            numRef--;
            break;
        }
    }

    return pRef;
}

void initRefPicListModification(RefPicListsModification &listModif, Ipp32s numTotalRefPicsUsed)
{
    for (Ipp32s listIdx = 0; listIdx < 2; listIdx++) {
        listModif.numBitsListEntry[listIdx] = H265_CeilLog2(numTotalRefPicsUsed);
        listModif.refPicListModificationFlag[listIdx] = 0;
        for (Ipp32s i = 0; i < numTotalRefPicsUsed; i++) {
            listModif.listEntry[listIdx][i] = i;
        }
    }
}

void setRefPicListModification(Frame *currFrame, Ipp32s numTotalRefPicsUsed, Ipp32s numLongtermPics, Ipp32s gopRefDist, Ipp8u isSliceB)
{
    RefPicListsModification *pListModif = &currFrame->m_listModif;
    Ipp32s ltrIdx = numTotalRefPicsUsed - 1;

    // modify refPicList
    if (gopRefDist == 1)
    {
        if (isSliceB)
        {
            pListModif->refPicListModificationFlag[1] = 1;
            Ipp32s k1 = 0;
            for (Ipp32s i = ltrIdx; i > k1; i--)
            {
                swapRefPicListData(&(currFrame->m_refPicList[1]), i - 1, i);
                pListModif->listEntry[1][i] = pListModif->listEntry[1][i - 1];
            }
            pListModif->listEntry[1][k1] = ltrIdx;
        }
        else
        {
            pListModif->refPicListModificationFlag[0] = 1;
            Ipp32s k0 = 1;
            for (Ipp32s i = ltrIdx; i > k0; i--)
            {
                swapRefPicListData(&(currFrame->m_refPicList[0]), i - 1, i);
                pListModif->listEntry[0][i] = pListModif->listEntry[0][i - 1];
            }
            pListModif->listEntry[0][k0] = ltrIdx;
        }
    }
    else if (gopRefDist == 8)
    {
        if (numLongtermPics)
        {
            if (currFrame->m_picCodeType == MFX_FRAMETYPE_P)
            {
                pListModif->refPicListModificationFlag[1] = 1;
                Ipp32s k1 = 0;
                for (Ipp32s i = ltrIdx; i > k1; i--)
                {
                    swapRefPicListData(&(currFrame->m_refPicList[1]), i-1, i);
                    pListModif->listEntry[1][i] = pListModif->listEntry[1][i-1];
                }
                pListModif->listEntry[1][k1] = ltrIdx;
            }
        }
    }
}

void H265Encoder::CreateRefPicList(Frame *in, H265ShortTermRefPicSet *rps)
{
    Frame *currFrame = in;
    Frame **list0 = currFrame->m_refPicList[0].m_refFrames;
    Frame **list1 = currFrame->m_refPicList[1].m_refFrames;
    Ipp32s currPoc = currFrame->m_poc;
    Ipp8s *dpoc0 = currFrame->m_refPicList[0].m_deltaPoc;
    Ipp8s *dpoc1 = currFrame->m_refPicList[1].m_deltaPoc;

    Ipp32s numStBefore = 0;
    Ipp32s numStAfter = 0;

    // constuct initial reference lists
    Ipp32s num_long_term_pics = 0;
    currFrame->m_pLtrFrame = NULL;

    // setup delta pocs and long-term flags
    memset(currFrame->m_refPicList[0].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[0].m_isLongTermRef));
    memset(currFrame->m_refPicList[1].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[1].m_isLongTermRef));

    currFrame->m_refPicList[0].m_refFramesCount = 0;
    currFrame->m_refPicList[1].m_refFramesCount = 0;
    currFrame->m_dpbSize = 0;

    if (m_videoParam.enableLTR)
    {
        if (currFrame->m_frameOrder < 1)
            currFrame->m_ltrConfidenceLevel = 1;

        if (m_videoParam.GopRefDist < 2 ) {
            if (currFrame->m_avgTcScRatio > 6.0)
                currFrame->m_ltrConfidenceLevel = 0;
            else if (currFrame->m_avgTcScRatio > 5.0)
                currFrame->m_ltrConfidenceLevel = 1;
            else if (currFrame->m_avgTcScRatio > 4.0)
                currFrame->m_ltrConfidenceLevel = 2;
            else
                currFrame->m_ltrConfidenceLevel = 3;
        }
        else if (m_videoParam.GopRefDist == 8) {
            if (currFrame->m_avgTcScRatio > 17)
                currFrame->m_ltrConfidenceLevel = 0;
            else if (currFrame->m_avgTcScRatio > 14)
                currFrame->m_ltrConfidenceLevel = 1;
            else if (currFrame->m_avgTcScRatio > 11)
                currFrame->m_ltrConfidenceLevel = 2;
            else
                currFrame->m_ltrConfidenceLevel = 3;
        }

        if (currFrame->m_frameOrder == 0)
            m_currLtrStatus = 1;

        // IDR
        if (currFrame->m_isIdrPic)
            return;

        // set one unique LTR frame
        Ipp32s pocLtrStart = (m_videoParam.GopRefDist == 1) ? 2 : m_videoParam.GopRefDist;

        Ipp32s isLtrOK = currFrame->m_ltrConfidenceLevel ? 1 : 0;
        if (currFrame->m_picCodeType == MFX_FRAMETYPE_I)
           m_currLtrStatus = isLtrOK;

        if (currPoc > pocLtrStart && m_currLtrStatus) {
            if (currFrame->m_picCodeType != MFX_FRAMETYPE_I) {
                Ipp32s frameOrderLTR = -1;
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    Frame *ref = (*i);
                    if (currFrame->m_frameOrder > currFrame->m_frameOrderOfLastIntraInEncOrder &&
                        ref->m_frameOrder < currFrame->m_frameOrderOfLastIntraInEncOrder)
                        continue; // trailing pictures can't predict from leading pictures

                    Ipp8u ltrCond = (ref->m_picCodeType == MFX_FRAMETYPE_I || ref->m_isIdrPic) ? 1 : 0;
                    if (ref->m_poc >= currFrame->m_poc)  ltrCond = 0;
                    if (ltrCond) {
                        if (num_long_term_pics == 0) {
                            frameOrderLTR = ref->m_frameOrder;
                            currFrame->m_pLtrFrame = ref;
                            num_long_term_pics = 1;
                        }
                        else
                        {
                            if (ref->m_frameOrder > frameOrderLTR) {
                                frameOrderLTR = ref->m_frameOrder;
                                currFrame->m_pLtrFrame = ref;
                                num_long_term_pics = 1;
                            }
                        }
                    }
               }
            }
        }
    }  // if (m_videoParam.enableLTR)

    // constuct initial reference lists
    numStBefore = numStAfter = 0;
    in->m_dpbSize = 0;
    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
        Frame *ref = (*i);
        if (currFrame->m_frameOrder > currFrame->m_frameOrderOfLastIntraInEncOrder &&
            ref->m_frameOrder < currFrame->m_frameOrderOfLastIntraInEncOrder)
            continue; // trailing pictures can't predict from leading pictures
        if (currFrame->m_pLtrFrame) {
            if (ref == currFrame->m_pLtrFrame)  // AMT_LTR
                continue;
        }

        in->m_dpb[in->m_dpbSize++] = ref;
        if (ref->m_poc < currFrame->m_poc)
            list0[numStBefore++] = ref;
        else
            list1[numStAfter++] = ref;
    }
    std::sort(list0, list0 + numStBefore, PocIsGreater);
    std::sort(list1, list1 + numStAfter,  PocIsLess);

    const Ipp32s MAX_UNIQ_REFS = 4;

    if (m_videoParam.enableLTR)
    {
        if (m_videoParam.GopRefDist == 1) {
            Ipp32s maxNumStBefore = m_videoParam.MaxDecPicBuffering - 1;
            maxNumStBefore -= num_long_term_pics;
            if (numStBefore > maxNumStBefore) {
                for (Ipp32s i = 0; i < maxNumStBefore; i++) {
                    currFrame->m_dpb[i] = list0[i];
                }
                numStBefore = maxNumStBefore;
                currFrame->m_dpbSize = numStBefore;
            }
        }
    }

    // merge lists
    small_memcpy(list0 + numStBefore, list1, sizeof(*list1) * numStAfter);
    small_memcpy(list1 + numStAfter,  list0, sizeof(*list0) * numStBefore);

    // setup delta pocs and long-term flags
//    memset(currFrame->m_refPicList[0].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[0].m_isLongTermRef));
//    memset(currFrame->m_refPicList[1].m_isLongTermRef, 0, sizeof(currFrame->m_refPicList[1].m_isLongTermRef));
    for (Ipp32s i = 0; i < numStBefore + numStAfter; i++) {
        dpoc0[i] = Ipp8s(currPoc - list0[i]->m_poc);
        dpoc1[i] = Ipp8s(currPoc - list1[i]->m_poc);
    }

    // cut lists
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_I) {
        currFrame->m_refPicList[0].m_refFramesCount = 0;
        currFrame->m_refPicList[1].m_refFramesCount = 0;
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_P) {
        currFrame->m_refPicList[0].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[0]);
        currFrame->m_refPicList[1].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxP[1]);
    }
    else if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        if(m_videoParam.NumRefLayers>2 && m_videoParam.MaxRefIdxB[0]>1) {
            Ipp32s refCount=0, refWindow=0;
            for(Ipp32s j=0;j<numStBefore;j++) {
                if(currFrame->m_refPicList[0].m_refFrames[j]->m_pyramidLayer<=m_videoParam.refLayerLimit[currFrame->m_pyramidLayer]) {
                    refCount++;
                    if(refCount<=m_videoParam.MaxRefIdxB[0]) refWindow = j;
                }
            }
            currFrame->m_refPicList[0].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, IPP_MAX(refWindow+1, m_videoParam.MaxRefIdxB[0]));
            currFrame->m_refPicList[1].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[1]);
        } else
        {
            currFrame->m_refPicList[0].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[0]);
            currFrame->m_refPicList[1].m_refFramesCount = IPP_MIN(numStBefore + numStAfter, m_videoParam.MaxRefIdxB[1]);
        }
    }

    Ipp32s numUniqRefs = CountUniqueRefs(currFrame->m_refPicList);
    while (numUniqRefs > MAX_UNIQ_REFS) {
        (currFrame->m_refPicList[0].m_refFramesCount > currFrame->m_refPicList[1].m_refFramesCount)
            ? currFrame->m_refPicList[0].m_refFramesCount--
            : currFrame->m_refPicList[1].m_refFramesCount--;
        numUniqRefs = CountUniqueRefs(currFrame->m_refPicList);
    }

    // create RPS syntax
    // Ipp32s numL0 = currFrame->m_refPicList[0].m_refFramesCount;
    // Ipp32s numL1 = currFrame->m_refPicList[1].m_refFramesCount;
    rps->inter_ref_pic_set_prediction_flag = 0;
    rps->num_negative_pics = numStBefore;
    rps->num_positive_pics = numStAfter;
    Ipp32s deltaPocPred = 0;
    Ipp32s numShortTermPicsUsed = 0;
    for (Ipp32s i = 0; i < numStBefore; i++) {
        rps->delta_poc[i] = dpoc0[i] - deltaPocPred;
        //rps->used_by_curr_pic_flag[i] = i < numL0 || i < IPP_MAX(0, numL1 - numStAfter);
        rps->used_by_curr_pic_flag[i] = (currFrame->m_picCodeType != MFX_FRAMETYPE_I); // mimic current behavior
        deltaPocPred = dpoc0[i];
        if (rps->used_by_curr_pic_flag[i])
            numShortTermPicsUsed++;
    }
    deltaPocPred = 0;
    for (Ipp32s i = 0; i < numStAfter; i++) {
        rps->delta_poc[numStBefore + i] = deltaPocPred - dpoc1[i];
        //rps->used_by_curr_pic_flag[numStBefore + i] = i < numL1 || i < IPP_MAX(0, numL0 - numStBefore);
        rps->used_by_curr_pic_flag[numStBefore + i] = (currFrame->m_picCodeType != MFX_FRAMETYPE_I);
        deltaPocPred = dpoc1[i];
        if(rps->used_by_curr_pic_flag[numStBefore + i])
            numShortTermPicsUsed++;
    }
    Ipp32s numTotalRefPicsUsed = numShortTermPicsUsed + num_long_term_pics;

//    currFrame->m_numPocTotalCurr = std::accumulate(rps->used_by_curr_pic_flag, rps->used_by_curr_pic_flag + numStBefore + numStAfter, 0);
    currFrame->m_numPocTotalCurr = numTotalRefPicsUsed;
    currFrame->m_ceilLog2NumPocTotalCurr = H265_CeilLog2(currFrame->m_numPocTotalCurr);

    initRefPicListModification(currFrame->m_listModif, numTotalRefPicsUsed);

    ModifyRefList(m_videoParam, *currFrame, 0);
    ModifyRefList(m_videoParam, *currFrame, 1);

    if (m_videoParam.enableLTR)
    {
        for (Ipp32s listIdx = 0; listIdx < 2; listIdx++) {
            currFrame->m_listModif.refPicListModificationFlag[listIdx] = currFrame->m_refPicList[listIdx].m_listModFlag;
            for (Ipp32s i = 0; i < numShortTermPicsUsed; i++) {
                currFrame->m_listModif.listEntry[listIdx][i] = currFrame->m_refPicList[listIdx].m_listMod[i];
            }
        }

        currFrame->num_long_term_sps = 0;
        currFrame->num_long_term_pics = 0;

        if (num_long_term_pics) {
            // Add LTR to refPicList
            Ipp32s numSTR = numStBefore + numStAfter;
            numTotalRefPicsUsed = numSTR + num_long_term_pics;

            // DPB
            in->m_dpbSize = numSTR;
            in->m_dpb[in->m_dpbSize++] = currFrame->m_pLtrFrame;

            // long-term reference pictures set
            H265LongTermRefPicSet *pLtRps = &currFrame->m_longRefPicSet[0];

            // set LTR bit writing
            Ipp32s pocLsbMax = 1 << m_sps.log2_max_pic_order_cnt_lsb;
            Ipp32s pocLtr = currFrame->m_pLtrFrame->m_poc;
            Ipp32s pocLsbLtr = computeLSB(pocLtr, pocLsbMax);;
            Ipp32s pocMsbLtr = pocLtr - pocLsbLtr;

            Ipp8u  deltaPocMsbPresentFlag = 0;
            Ipp32s deltaPocMsbCycleLtr = 0;

            Ipp32s pocLsbCurr = computeLSB(currFrame->m_poc, pocLsbMax);
            Ipp32s pocMsbCurr = currFrame->m_poc - pocLsbCurr;

            deltaPocMsbCycleLtr = (pocMsbCurr - pocMsbLtr) / pocLsbMax;
            if(deltaPocMsbCycleLtr) deltaPocMsbPresentFlag = 1;

            pLtRps->used_by_curr_pic_lt_flag = 1;
            pLtRps->delta_poc_msb_present_flag = deltaPocMsbPresentFlag;
            pLtRps->delta_poc_msb_cycle_lt = deltaPocMsbCycleLtr;

            pLtRps->poc_lsb_ltr = pocLsbLtr;
            pLtRps->poc_msb_ltr = pocMsbLtr;

            pLtRps->lt_idx_sps = 0;

            Ipp32s setIdx = getMatchingLtrIdxFromRpsSet(m_sps.m_LtRpsSps, m_sps.num_long_term_ref_pics_sps, pocLsbLtr, pLtRps->used_by_curr_pic_lt_flag);
            if (setIdx >= 0) {
                pLtRps->lt_idx_sps = setIdx;
                currFrame->num_long_term_sps = 1;
                currFrame->num_long_term_pics = 0;
            }
            else {
                pLtRps->lt_idx_sps = -1;
                currFrame->num_long_term_sps = 0;
                currFrame->num_long_term_pics = 1;
            }

            // modify refPicList for LTR
            Ipp32s ltrIdx = numTotalRefPicsUsed - 1;
            Ipp32s deltaPocLtr = currFrame->m_poc - currFrame->m_pLtrFrame->m_poc;
            for (Ipp32s listIdx = 0; listIdx < 2; listIdx++) {
                currFrame->m_refPicList[listIdx].m_refFrames[ltrIdx] = currFrame->m_pLtrFrame;
                currFrame->m_refPicList[listIdx].m_deltaPoc[ltrIdx] = (Ipp8s)deltaPocLtr;
                currFrame->m_refPicList[listIdx].m_isLongTermRef[ltrIdx] = 1;
            }

            Ipp32s LtrOK = currFrame->m_ltrConfidenceLevel ? 1 : 0;

            if (m_currLtrStatus && LtrOK) {
                Ipp8u isSliceB = (currFrame->m_picCodeType == MFX_FRAMETYPE_P && m_videoParam.GeneralizedPB || currFrame->m_picCodeType == MFX_FRAMETYPE_B) ? 1 : 0;
                setRefPicListModification(currFrame, numTotalRefPicsUsed, num_long_term_pics, m_videoParam.GopRefDist, isSliceB);
            }
        }
    }  //if (m_videoParam.enableLTR)
}

Ipp8u SameRps(const H265ShortTermRefPicSet *rps1, const H265ShortTermRefPicSet *rps2)
{
    // check number of refs
    if (rps1->num_negative_pics != rps2->num_negative_pics ||
        rps1->num_positive_pics != rps2->num_positive_pics)
        return 0;
    // check delta pocs and "used" flags
    for (Ipp32s i = 0; i < rps1->num_negative_pics + rps1->num_positive_pics; i++)
        if (rps1->delta_poc[i] != rps2->delta_poc[i] ||
            rps1->used_by_curr_pic_flag[i] != rps2->used_by_curr_pic_flag[i])
            return 0;
    return 1;
}

//struct isEqual
//{
//    isEqual(Ipp32s frameOrder): m_frameOrder(frameOrder)
//    {}
//
//    template<typename T>
//    bool operator()(const T& l)
//    {
//        return (*l).m_frameOrder == m_frameOrder;
//    }
//
//    Ipp32s m_frameOrder;
//};

void H265Encoder::ConfigureEncodeFrame(Frame* frame)
{
    Frame *currFrame = frame;

    // update frame order of most recent intra frame in encoding order
    currFrame->m_frameOrderOfLastIntraInEncOrder = m_frameOrderOfLastIntraInEncOrder;
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_I)
        m_frameOrderOfLastIntraInEncOrder = currFrame->m_frameOrder;

    // setup ref flags
    currFrame->m_isLongTermRef = 0;
    currFrame->m_isShortTermRef = 1;
    if (currFrame->m_picCodeType == MFX_FRAMETYPE_B) {
        if (m_videoParam.BiPyramidLayers > 1) {
            if (currFrame->m_pyramidLayer == m_videoParam.BiPyramidLayers - 1)
                currFrame->m_isShortTermRef = 0;
        }
    }
    currFrame->m_isRef = currFrame->m_isShortTermRef | currFrame->m_isLongTermRef;

    // create reference lists and RPS syntax
    H265ShortTermRefPicSet *pRPS = &currFrame->m_shortRefPicSet[0];
    H265ShortTermRefPicSet rps = { 0 };
    Ipp32s useSpsRps = 0;
    if (m_videoParam.enableLTR) {
        if (currFrame->m_picCodeType == MFX_FRAMETYPE_I)
            currFrame->m_isIdrPic = 1;   // to avoid CRA

        CreateRefPicList(currFrame, pRPS);

        pRPS->inter_ref_pic_set_prediction_flag = 0;
        currFrame->short_term_ref_pic_set_sps_flag = 0;
        currFrame->short_term_ref_pic_set_idx = 0;
        for (Ipp8u i = 0; i < m_sps.num_short_term_ref_pic_sets; i++) {
            useSpsRps = 1;
            // check number of refs
            if (m_sps.m_shortRefPicSet[i].num_negative_pics != pRPS->num_negative_pics || m_sps.m_shortRefPicSet[i].num_positive_pics != pRPS->num_positive_pics)
                useSpsRps = 0;
            if (useSpsRps) {
                // check delta pocs and "used" flags
                for (Ipp32s j = 0; j < (m_sps.m_shortRefPicSet[i].num_negative_pics + m_sps.m_shortRefPicSet[i].num_positive_pics); j++) {
                    if (m_sps.m_shortRefPicSet[i].delta_poc[j] != pRPS->delta_poc[j]) {
                        useSpsRps = 0;
                        break;
                    }
                }
            }

            if (useSpsRps) {
                currFrame->short_term_ref_pic_set_sps_flag = 1;
                currFrame->short_term_ref_pic_set_idx = i;
                break;
            }
        }
    }
   else {
   CreateRefPicList(currFrame, &rps);
   useSpsRps = SameRps(m_sps.m_shortRefPicSet + currFrame->m_RPSIndex, &rps);
    }

    // setup map list1->list0, used during Motion Estimation
    Frame **list0 = currFrame->m_refPicList[0].m_refFrames;
    Frame **list1 = currFrame->m_refPicList[1].m_refFrames;
    Ipp32s numRefIdx0 = currFrame->m_refPicList[0].m_refFramesCount;
    Ipp32s numRefIdx1 = currFrame->m_refPicList[1].m_refFramesCount;
    for (Ipp32s idx1 = 0; idx1 < numRefIdx1; idx1++) {
        Ipp32s idxInList0 = (Ipp32s)(std::find(list0, list0 + numRefIdx0, list1[idx1]) - list0);
        currFrame->m_mapRefIdxL1ToL0[idx1] = (idxInList0 < numRefIdx0) ? idxInList0 : -1;
    }

    // Map to unique
    Ipp32s uniqueIdx = 0;
    for (Ipp32s idx = 0; idx < numRefIdx0; idx++) {
        currFrame->m_mapListRefUnique[0][idx] = uniqueIdx;
        uniqueIdx++;
    }
    for (Ipp32s idx = numRefIdx0; idx < MAX_NUM_REF_IDX; idx++) {
        currFrame->m_mapListRefUnique[0][idx] = -1;
    }
    for (Ipp32s idx1 = 0; idx1 < numRefIdx1; idx1++) {
        if(currFrame->m_mapRefIdxL1ToL0[idx1] == -1) {
            currFrame->m_mapListRefUnique[1][idx1] = uniqueIdx;
            uniqueIdx++;
        } else {
            currFrame->m_mapListRefUnique[1][idx1] = currFrame->m_mapListRefUnique[0][currFrame->m_mapRefIdxL1ToL0[idx1]];
        }
    }
    for (Ipp32s idx = numRefIdx1; idx < MAX_NUM_REF_IDX; idx++) {
        currFrame->m_mapListRefUnique[1][idx] = -1;
    }
    //assert(uniqueIdx <= MAX_NUM_REF_IDX);
    currFrame->m_numRefUnique = uniqueIdx;

    // setup flag m_allRefFramesAreFromThePast used for TMVP
    currFrame->m_allRefFramesAreFromThePast = true;
    for (Ipp32s i = 0; i < numRefIdx0 && currFrame->m_allRefFramesAreFromThePast; i++)
        if (currFrame->m_refPicList[0].m_deltaPoc[i] < 0)
            currFrame->m_allRefFramesAreFromThePast = false;
    for (Ipp32s i = 0; i < numRefIdx1 && currFrame->m_allRefFramesAreFromThePast; i++)
        if (currFrame->m_refPicList[1].m_deltaPoc[i] < 0)
            currFrame->m_allRefFramesAreFromThePast = false;

    // setup slices
    bool doSao = false;
    H265Slice *currSlices = &frame->m_slices[0];

    Ipp32s sliceRowStart = 0;
    for (Ipp8u i = 0; i < m_videoParam.NumSlices; i++) {
        Ipp32s sliceHeight = ((i + 1) * m_videoParam.PicHeightInCtbs) / m_videoParam.NumSlices -
            (i * m_videoParam.PicHeightInCtbs / m_videoParam.NumSlices);
        SetSlice(currSlices + i, i, currFrame);
        sliceRowStart += sliceHeight;

      if (!m_videoParam.enableLTR) {
        currSlices[i].short_term_ref_pic_set_sps_flag = useSpsRps;
        if (currSlices[i].short_term_ref_pic_set_sps_flag)
            currSlices[i].short_term_ref_pic_set_idx = currFrame->m_RPSIndex;
        else
            currSlices[i].m_shortRefPicSet = rps;
      }

        if(m_sps.sample_adaptive_offset_enabled_flag &&
            (currSlices[i].slice_sao_luma_flag || currSlices[i].slice_sao_chroma_flag))
            doSao = true;
    }

    // check if deblocking or sao needed
    currFrame->m_doPostProc = doSao || (m_videoParam.deblockingFlag && (currFrame->m_isRef || m_videoParam.doDumpRecon));

    currFrame->m_encOrder = m_lastEncOrder + 1;

    // set task param to simplify bitstream writing logic
    currFrame->m_frameType= currFrame->m_picCodeType |
        (currFrame->m_isIdrPic ? MFX_FRAMETYPE_IDR : 0) |
        (currFrame->m_isRef ? MFX_FRAMETYPE_REF : 0);

    // take ownership over reference frames
    for (Ipp32s i = 0; i < currFrame->m_dpbSize; i++)
        currFrame->m_dpb[i]->AddRef();

    UpdateDpb(currFrame);

    // hint from lookahead
    if (currFrame->m_slices[0].NalUnitType != NAL_RASL_R && currFrame->m_slices[0].NalUnitType != NAL_RASL_N) {
        currFrame->m_forceTryIntra = 0;
    } /*else if (currFrame->m_forceTryIntra){
        FILE *fp = fopen("markedRASL.txt", "a+");
        fprintf(fp, "FrameOrder %i \n", currFrame->m_frameOrder);
        fclose(fp);
    }*/
#if 0
    else if (currFrame->m_forceTryIntra) { // means RASL with pendingSceneCut
        Ipp32s targetScene = currFrame->m_sceneOrder;
        Ipp32s isTargetSceneInRef[2] = {0};
        for (Ipp32s dir = 0; dir < 2; dir++) {
            for (Ipp32s refIdx = 0; refIdx < currFrame->m_refPicList[dir].m_refFramesCount; refIdx++) {
                Frame* ref = currFrame->m_refPicList[dir].m_refFrames[refIdx];
                if (targetScene == ref->m_sceneOrder) {
                    isTargetSceneInRef[dir] = 1;
                    break;
                }
            }
        }
        if (isTargetSceneInRef[0] || isTargetSceneInRef[1]) {
            currFrame->m_forceTryIntra = 0; // it is expected prediction from ref is good enough
        }
    }
#endif

}

// expect that frames in m_dpb is ordered by m_encOrder
void H265Encoder::UpdateDpb(Frame *currTask)
{

    Frame *currFrame = currTask;

    // first of all remove frames not appeared in current frame RPS
    for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end();) {
        bool found = std::find(currFrame->m_dpb, currFrame->m_dpb + currFrame->m_dpbSize, *i)
            != currFrame->m_dpb + currFrame->m_dpbSize;
        FrameIter toRemove = i++;

        if (!found) {
            (*toRemove)->Release();
            m_actualDpb.erase(toRemove);
        }
    }

    if (!currFrame->m_isRef)
        return; // non-ref frame doesn't change dpb

    if (currFrame->m_isIdrPic) {
        // IDR frame removes all reference frames from dpb and becomes the only reference
        for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i)
            (*i)->Release();
        m_actualDpb.clear();
    }
    else if ((Ipp32s)m_actualDpb.size() == m_videoParam.MaxDecPicBuffering) {
        // dpb is full
        // need to make a place in dpb for a new reference frame
        FrameIter toRemove = m_actualDpb.end();
        if (m_videoParam.GopRefDist > 1) {
            // B frames with or without pyramid
            // search for oldest refs from previous minigop
            Ipp32s currMiniGop = currFrame->m_miniGopCount;
            for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                if (currFrame->m_pLtrFrame && (*i) == currFrame->m_pLtrFrame)   // AMT_LTR
                    continue;
                if ((*i)->m_miniGopCount < currMiniGop)
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                        toRemove = i;
            }
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for oldest B frame in current minigop
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    if (currFrame->m_pLtrFrame && (*i) == currFrame->m_pLtrFrame)   // AMT_LTR
                        continue;
                    if ((*i)->m_picCodeType == MFX_FRAMETYPE_B &&
                        (*i)->m_miniGopCount == currMiniGop)
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                            toRemove = i;
                }
            }
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for any oldest ref in current minigop
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    if (currFrame->m_pLtrFrame && (*i) == currFrame->m_pLtrFrame)   // AMT_LTR
                        continue;
                    if ((*i)->m_miniGopCount == currMiniGop)
                        if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                            toRemove = i;
                }
            }
        }
        else {
            // P frames with or without pyramid
            // search for oldest non-anchor ref
            for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                if (currFrame->m_pLtrFrame && (*i) == currFrame->m_pLtrFrame)   // AMT_LTR
                    continue;
                if ((*i)->m_pyramidLayer > 0)
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                        toRemove = i;
            }
            if (toRemove == m_actualDpb.end()) {
                // if nothing found
                // search for any oldest ref
                for (FrameIter i = m_actualDpb.begin(); i != m_actualDpb.end(); ++i) {
                    if (currFrame->m_pLtrFrame && (*i) == currFrame->m_pLtrFrame)   // AMT_LTR
                        continue;
                    if (toRemove == m_actualDpb.end() || (*toRemove)->m_poc > (*i)->m_poc)
                        toRemove = i;
                }
            }
        }

        VM_ASSERT(toRemove != m_actualDpb.end());
        (*toRemove)->Release();
        m_actualDpb.erase(toRemove);
    }

    VM_ASSERT((Ipp32s)m_actualDpb.size() < m_videoParam.MaxDecPicBuffering);
    m_actualDpb.push_back(currTask);
    currFrame->AddRef();
}


void H265Encoder::CleanGlobalDpb()
{
    for (FrameIter i = m_dpb.begin(); i != m_dpb.end();) {
        FrameIter curI = i++;
        if ((*curI)->m_refCounter == 0) {
            SafeRelease((*curI)->m_recon);
            SafeRelease((*curI)->m_feiRecon);
            SafeRelease((*curI)->m_feiCuData);
            m_free.splice(m_free.end(), m_dpb, curI);
        }
    }
}

void H265Encoder::OnEncodingQueried(Frame* encoded)
{
    Dump(&m_videoParam, encoded, m_dpb);

    // release reference frames
    for (Ipp32s i = 0; i < encoded->m_dpbSize; i++)
        encoded->m_dpb[i]->Release();

    // release source frame
    SafeRelease(encoded->m_origin);
    SafeRelease(encoded->m_feiOrigin);

    // release FEI resources
    for (Ipp32s i = 0; i < 4; i++)
        SafeRelease(encoded->m_feiIntraAngModes[i]);
    for (Ipp32s i = 0; i < 4; i++) {
        for (Ipp32s j = 0; j < 4; j++) {
            SafeRelease(encoded->m_feiInterData[i][j]);
        }
    }
    for (Ipp32s i = 2; i < 4; i++)
        SafeRelease(encoded->m_feiBirefData[i]);

    SafeRelease(encoded->m_feiSaoModes);
    SafeRelease(encoded->m_lowres);

    if (m_la.get()) {
        for (Ipp32s idx = 0; idx < 2; idx++)
            SafeRelease(encoded->m_stats[idx]);
        SafeRelease(encoded->m_sceneStats);
    }

    // release encoded frame
    encoded->Release();

    m_outputQueue.remove(encoded);
    CleanGlobalDpb();
}


template <typename PixType>
mfxStatus H265FrameEncoder::ApplySaoCtu(Ipp32u ctbRow, Ipp32u ctbCol)
{
    FrameData* recon = m_frame->m_recon;

    PixType* pRec[2] = {(PixType*)recon->y, (PixType*)recon->uv};
    PixType* pRecTmp;
    Ipp32s pitch[2] = {recon->pitch_luma_pix, recon->pitch_chroma_pix};

    const Ipp32u uiMaxY  = (1 << 10) - 1;
    const Ipp32u iCRangeExt = uiMaxY >> 1;

    Ipp32s offsetEo[32];
    PixType offsetBo[uiMaxY+2*iCRangeExt];

    Ipp32s offsetEo2[32];
    PixType offsetBo2[uiMaxY+2*iCRangeExt];

    Ipp32u ctuAddr = ctbRow * m_videoParam.PicWidthInCtbs + ctbCol;
    Ipp32u ctuRecAddr = (ctbRow) * (m_videoParam.PicWidthInCtbs + 1) + ctbCol + 1;
    Ipp32u ctuRecAddrInv = (ctbCol) * (m_videoParam.PicHeightInCtbs + 1) + ctbRow + 1;

    Ipp32u ctuRecSaveAddr = (ctbRow + 1) * (m_videoParam.PicWidthInCtbs + 1) + ctbCol + 1;
    Ipp32u ctuRecSaveAddrInv = (ctbCol + 1) * (m_videoParam.PicHeightInCtbs + 1) + ctbRow + 1;

    IppiSize roiSize;
    Ipp32u regionCtbColFirst, regionCtbColLast, regionCtbRowFirst, regionCtbRowLast;

    if (m_videoParam.NumTiles > 1 && !m_videoParam.deblockBordersFlag) {
        Ipp32s tile_row = m_videoParam.m_tile_row_ids[ctuAddr];
        Ipp32s tile_col = m_videoParam.m_tile_col_ids[ctuAddr];

        Ipp32u tile_border_left = m_videoParam.tileColStart[tile_col] << m_videoParam.Log2MaxCUSize;
        Ipp32u tile_border_right = (m_videoParam.tileColStart[tile_col] + m_videoParam.tileColWidth[tile_col]) << m_videoParam.Log2MaxCUSize;
        if (tile_border_right > m_videoParam.Width)
            tile_border_right = m_videoParam.Width;

        Ipp32u tile_border_top = m_videoParam.tileRowStart[tile_row] << m_videoParam.Log2MaxCUSize;
        Ipp32u tile_border_bottom = (m_videoParam.tileRowStart[tile_row] + m_videoParam.tileRowHeight[tile_row]) << m_videoParam.Log2MaxCUSize;
        if (tile_border_bottom > m_videoParam.Height)
            tile_border_bottom = m_videoParam.Height;

        regionCtbColFirst = m_videoParam.tileColStart[tile_col];
        regionCtbColLast = m_videoParam.tileColStart[tile_col] + m_videoParam.tileColWidth[tile_col] - 1;
        regionCtbRowFirst = m_videoParam.tileRowStart[tile_row];
        regionCtbRowLast = m_videoParam.tileRowStart[tile_row] + m_videoParam.tileRowHeight[tile_row] - 1;

        roiSize.width = tile_border_right - tile_border_left;
        roiSize.height = tile_border_bottom - tile_border_top;
    } else if (m_videoParam.NumSlices > 1 && !m_videoParam.deblockBordersFlag) {
        Ipp32s slice_id = m_videoParam.m_slice_ids[ctuAddr];
        regionCtbColFirst = 0;
        regionCtbColLast = m_videoParam.PicWidthInCtbs - 1;
        regionCtbRowFirst = m_frame->m_slices[slice_id].row_first;
        regionCtbRowLast = m_frame->m_slices[slice_id].row_last;

        Ipp32u border_top = regionCtbRowFirst << m_videoParam.Log2MaxCUSize;
        Ipp32u border_bottom = (regionCtbRowLast + 1) << m_videoParam.Log2MaxCUSize;
        if (border_bottom > m_videoParam.Height)
            border_bottom = m_videoParam.Height;

        roiSize.width = m_videoParam.Width;
        roiSize.height = border_bottom - border_top;
    } else {
        regionCtbColFirst = regionCtbRowFirst = 0;
        regionCtbColLast = m_videoParam.PicWidthInCtbs - 1;
        regionCtbRowLast = m_videoParam.PicHeightInCtbs - 1;
        roiSize.width = m_videoParam.Width;
        roiSize.height = m_videoParam.Height;
    }

    // LUMA:
    Ipp32s compId = 0;
    {
        SaoApplier<PixType>* sao = (m_videoParam.bitDepthLuma == 8 )
            ? (SaoApplier<PixType>*)m_saoApplier[compId].m_sao
            : (SaoApplier<PixType>*)m_saoApplier[compId].m_sao10bit;
        Ipp32s ctb_pelx = ctbCol << m_videoParam.Log2MaxCUSize;
        Ipp32s ctb_pely = ctbRow << m_videoParam.Log2MaxCUSize;
        Ipp32s offset = (ctb_pelx) + (ctb_pely) * pitch[compId];

        if (ctbRow < regionCtbRowLast) {
            Ipp32s size = m_videoParam.MaxCUSize;
            if (ctbCol < regionCtbColLast) {
                size++;
            }
            pRecTmp = pRec[compId] + offset + (m_videoParam.MaxCUSize - 1)*pitch[compId];
            small_memcpy(sao->m_TmpU + (ctuRecSaveAddr << m_videoParam.Log2MaxCUSize), pRecTmp, sizeof(PixType) * size);
        }
        if (ctbCol < regionCtbColLast) {
            Ipp32s size = m_videoParam.MaxCUSize;
            if (ctbRow < regionCtbRowLast)
                size++;

            for (Ipp32s i = 0; i < size; i++)
                sao->m_TmpL[(ctuRecSaveAddrInv << m_videoParam.Log2MaxCUSize) + i] = pRec[compId][offset + i*pitch[compId] + (m_videoParam.MaxCUSize)-1];
        }
        if (ctbRow == regionCtbRowFirst) {
            Ipp32s size = m_videoParam.MaxCUSize;
            if (ctbCol < regionCtbColLast)
                size++;
            pRecTmp = pRec[compId] + offset;
            small_memcpy(sao->m_TmpU + ((ctuRecSaveAddr - (m_videoParam.PicWidthInCtbs + 1)) << m_videoParam.Log2MaxCUSize), pRecTmp, sizeof(PixType) * size);
        }
        if (ctbCol == regionCtbColFirst) {
            Ipp32s size = m_videoParam.MaxCUSize;
            if (ctbRow < regionCtbRowLast)
                size++;

            for (Ipp32s i = 0; i < size; i++)
                sao->m_TmpL[((ctuRecSaveAddrInv - (m_videoParam.PicHeightInCtbs + 1)) << m_videoParam.Log2MaxCUSize) + i] = pRec[compId][offset + i*pitch[compId]];
        }

        ctb_pelx -= (regionCtbColFirst << m_videoParam.Log2MaxCUSize);
        ctb_pely -= (regionCtbRowFirst << m_videoParam.Log2MaxCUSize);

        if (m_saoParam[ctuAddr][compId].mode_idx != SAO_MODE_OFF) {

            sao[compId].SetOffsetsLuma(m_saoParam[ctuAddr][compId], m_saoParam[ctuAddr][compId].type_idx, offsetEo, offsetBo);

            h265_ProcessSaoCuOrg_Luma(
                pRec[compId] + offset,
                pitch[compId],

                m_saoParam[ctuAddr][compId].type_idx,

                sao->m_TmpL + (ctuRecAddrInv << m_videoParam.Log2MaxCUSize),
                sao->m_TmpU + (ctuRecAddr << m_videoParam.Log2MaxCUSize),

                m_videoParam.MaxCUSize,
                m_videoParam.MaxCUSize,

                roiSize.width,
                roiSize.height,

                offsetEo,
                offsetBo,

                sao->m_ClipTable,
                ctb_pelx,
                ctb_pely);
        }
    }


    if (m_videoParam.SAOChromaFlag == 0)
        return MFX_ERR_NONE;


    //Chroma
    compId = 1;
    {
        SaoApplier<PixType>* sao = (m_videoParam.bitDepthLuma == 8 )
            ? (SaoApplier<PixType>*)m_saoApplier[compId].m_sao
            : (SaoApplier<PixType>*)m_saoApplier[compId].m_sao10bit;
        Ipp32s ctb_pelx = ctbCol << m_videoParam.Log2MaxCUSize;
        Ipp32s ctb_pely = ctbRow << m_videoParam.Log2MaxCUSize;
        Ipp32s offset = (ctb_pelx << m_videoParam.chromaShiftWInv) + (ctb_pely * pitch[compId] >> m_videoParam.chromaShiftH);

        if (ctbRow < regionCtbRowLast) {
            Ipp32s size = sao->m_ctbCromaWidthInPix;
            if (ctbCol < regionCtbColLast) {
                size++;
                size++;
            }
            pRecTmp = pRec[compId] + offset + (sao->m_ctbCromaHeightInRow - 1)*pitch[compId];
            small_memcpy(sao->m_TmpU_Chroma + (ctuRecSaveAddr * sao->m_ctbCromaWidthInPix), pRecTmp, sizeof(PixType) * size);
        }
        if (ctbCol < regionCtbColLast) {
            Ipp32s size = sao->m_ctbCromaHeightInPix;//m_videoParam.MaxCUSize;
            if (ctbRow < regionCtbRowLast) {
                size++;
                size++;
            }
            for (Ipp32s i = 0; i < size/2; i++)
            {
                sao->m_TmpL_Chroma[(ctuRecSaveAddrInv * sao->m_ctbCromaHeightInPix) + 2*i]   = pRec[compId][offset + i*pitch[compId] + (sao->m_ctbCromaWidthInPix)-1 - 1];
                sao->m_TmpL_Chroma[(ctuRecSaveAddrInv * sao->m_ctbCromaHeightInPix) + 2*i+1] = pRec[compId][offset + i*pitch[compId] + (sao->m_ctbCromaWidthInPix)-1];
            }
        }
        if (ctbRow == regionCtbRowFirst) {
            Ipp32s size = sao->m_ctbCromaWidthInPix;
            if (ctbCol < regionCtbColLast) {
                size++;
                size++;
            }
            pRecTmp = pRec[compId] + offset;
            small_memcpy(sao->m_TmpU_Chroma + ((ctuRecSaveAddr - (m_videoParam.PicWidthInCtbs + 1)) * sao->m_ctbCromaWidthInPix), pRecTmp, sizeof(PixType) * size);
        }
        if (ctbCol == regionCtbColFirst) {
            Ipp32s size = sao->m_ctbCromaHeightInPix;//m_videoParam.MaxCUSize;
            if (ctbRow < regionCtbRowLast) {
                size++;
                size++;
            }
            for (Ipp32s i = 0; i < size/2; i++)
            {
                sao->m_TmpL_Chroma[((ctuRecSaveAddrInv - (m_videoParam.PicHeightInCtbs + 1)) *sao->m_ctbCromaHeightInPix)  + 2*i]   = pRec[compId][offset + i*pitch[compId] + 0];
                sao->m_TmpL_Chroma[((ctuRecSaveAddrInv - (m_videoParam.PicHeightInCtbs + 1)) *sao->m_ctbCromaHeightInPix)  + 2*i+1] = pRec[compId][offset + i*pitch[compId] + 1];
            }
        }

        ctb_pelx = ctbCol * sao->m_ctbCromaWidthInPix;
        ctb_pely = ctbRow  * sao->m_ctbCromaHeightInPix / 2;

        ctb_pelx -= (regionCtbColFirst * sao->m_ctbCromaWidthInPix);
        ctb_pely -= (regionCtbRowFirst * sao->m_ctbCromaHeightInPix / 2);

        if (m_saoParam[ctuAddr][compId].mode_idx != SAO_MODE_OFF) {

            sao->SetOffsetsLuma(m_saoParam[ctuAddr][compId], m_saoParam[ctuAddr][compId].type_idx, offsetEo, offsetBo);
            sao->SetOffsetsLuma(m_saoParam[ctuAddr][compId+1], m_saoParam[ctuAddr][compId+1].type_idx, offsetEo2, offsetBo2);

            // correct params
            roiSize.width  = (m_videoParam.chromaFormatIdc == MFX_CHROMAFORMAT_YUV444) ? roiSize.width * 2 : roiSize.width;
            roiSize.height = (m_videoParam.chromaFormatIdc != MFX_CHROMAFORMAT_YUV420) ? roiSize.height : roiSize.height / 2;

            sao->h265_ProcessSaoCuChroma(
                pRec[compId] + offset,
                pitch[compId],

                m_saoParam[ctuAddr][compId].type_idx,

                sao->m_TmpL_Chroma + (ctuRecAddrInv * sao->m_ctbCromaHeightInPix),
                sao->m_TmpU_Chroma + (ctuRecAddr * sao->m_ctbCromaWidthInPix),

                sao->m_ctbCromaWidthInPix,
                sao->m_ctbCromaHeightInPix / 2,

                roiSize.width,
                roiSize.height,

                offsetEo,
                offsetBo,
                offsetEo2,
                offsetBo2,

                sao->m_ClipTable,

                ctb_pelx,
                ctb_pely);
        }
    }

    return MFX_ERR_NONE;
}


class OnExitHelper
{
public:
    OnExitHelper(Ipp32u* arg) : m_arg(arg)
    {}
    ~OnExitHelper()
    {
        if (m_arg)
            vm_interlocked_cas32(reinterpret_cast<volatile Ipp32u *>(m_arg), 0, 1);
    }

private:
    Ipp32u* m_arg;
};

template <typename PixType>
mfxStatus H265FrameEncoder::PerformThreadingTask(ThreadingTaskSpecifier action, Ipp32u ctb_row, Ipp32u ctb_col_orig)
{
    H265VideoParam *pars = &m_videoParam;
    Ipp8u nz[2];
    // assign local thread id
    Ipp32s ithread = -1;
    for(Ipp32u idx = 0; idx < m_videoParam.num_thread_structs; idx++) {
        Ipp32u stage = vm_interlocked_cas32( & m_topEnc.m_ithreadPool[idx], 1, 0);
        if(stage == 0) {
            ithread = idx;
            break;
        }
    }
    if(ithread == -1) {
        return MFX_TASK_BUSY;
    }
    OnExitHelper exitHelper( &(m_topEnc.m_ithreadPool[ithread]) );

    H265CU<PixType> *cu_ithread = (H265CU<PixType> *)m_topEnc.m_cu + ithread;

    for (Ipp32u ctb_col = (action == TT_POST_PROC_ROW ? 0 : ctb_col_orig); ctb_col <= ctb_col_orig; ctb_col++) {

    Ipp32u ctb_addr = ctb_row * pars->PicWidthInCtbs + ctb_col;

    Ipp8u curr_slice_id = m_videoParam.m_slice_ids[ctb_addr];
    Ipp16u curr_tile_id = m_topEnc.m_tile_ids[ctb_addr];
    Ipp8u start_of_slice_flag = (Ipp32s)ctb_addr == m_frame->m_slices[curr_slice_id].slice_segment_address;
    Ipp8u end_of_slice_flag = ctb_addr == m_frame->m_slices[curr_slice_id].slice_address_last_ctb;
    Ipp8u start_of_tile_flag = ctb_addr == pars->m_tiles[curr_tile_id].first_ctb_addr;
    Ipp8u end_of_tile_flag = ctb_addr == pars->m_tiles[curr_tile_id].last_ctb_addr;

    Ipp32s bs_id, bsf_id;
    bsf_id = ithread;
    bs_id = pars->NumTiles > 1 ? curr_tile_id : pars->WPPFlag ? ctb_row : curr_slice_id;

    cu_ithread->InitCu(pars,
                m_frame->cu_data + (ctb_addr << pars->Log2NumPartInCU),
                data_temp + ithread * data_temp_size,
                ctb_row,
                ctb_col,
                &m_bsf[bsf_id], &(m_frame->m_slices[0]) + curr_slice_id,
                action,
                m_costStat,
                m_frame,
                m_coeffWork + (ctb_addr << (m_videoParam.Log2MaxCUSize << 1)) * 6 / (2 + m_videoParam.chromaShift));

    switch (action) {
    case TT_ENCODE_CTU:
        if (start_of_slice_flag ||
            (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0) ||
            (m_topEnc.m_pps.tiles_enabled_flag && start_of_tile_flag)) {

            if (!m_frame->m_doPostProc) {
                ippiCABACInit_H265(&m_bs[bs_id].cabacState,
                    m_bs[bs_id].m_base.m_pbsBase,
                    m_bs[bs_id].m_base.m_bitOffset + (Ipp32u)(m_bs[bs_id].m_base.m_pbs - m_bs[bs_id].m_base.m_pbsBase) * 8,
                    m_bs[bs_id].m_base.m_maxBsSize);
            }

            if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_frame->m_slices[curr_slice_id].slice_segment_address) {
                if (m_frame->m_doPostProc) {
                    m_bsf[bsf_id].CtxRestoreWPP(m_context_array_wpp_enc + NUM_CABAC_CONTEXT * (ctb_row - 1));
                } else {
                    m_bs[bs_id].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
                }
            } else {
                InitializeContextVariablesHEVC_CABAC(
                    m_frame->m_doPostProc ? m_bsf[bsf_id].m_base.context_array : m_bs[bs_id].m_base.context_array,
                    2-m_frame->m_slices[curr_slice_id].slice_type, m_frame->m_sliceQpY);
            }
        } else if (m_frame->m_doPostProc) {
                m_bsf[bsf_id].CtxRestore(m_bs[bs_id].m_base.context_array_enc);
        }

        __ALIGN64 CABAC_CONTEXT_H265 context_array_save[NUM_CABAC_CONTEXT];

        if (m_frame->m_doPostProc) {
            m_bsf[bsf_id].CtxSave(context_array_save);
        } else {
            m_bsf[bsf_id].CtxRestore(m_bs[bs_id].m_base.context_array);
        }

        m_bsf[bsf_id].Reset();

#ifdef AMT_HROI_PSY_AQ
        if (m_videoParam.UseDQP && (m_videoParam.numRoi > 0 || (m_videoParam.DeltaQpMode&AMT_DQP_PSY_HROI))) {
#else
        if (m_videoParam.UseDQP && m_videoParam.numRoi > 0) {
#endif
            cu_ithread->SetCuLambdaRoi(m_frame);
        } 
        else if(m_videoParam.UseDQP && m_videoParam.DeltaQpMode) {
            cu_ithread->SetCuLambda(m_frame);
        }
        cu_ithread->GetInitAvailablity();
        cu_ithread->ModeDecision(0, 0);
        //cu_ithread->FillRandom(0, 0);
        //cu_ithread->FillZero(0, 0);
        {
            bool interUpdate = false;

            if(!cu_ithread->m_isRdoq) {
                cu_ithread->m_isRdoq = true;
                m_bsf[bsf_id].CtxRestore(m_frame->m_doPostProc ? context_array_save : m_bs[bs_id].m_base.context_array);
                interUpdate = cu_ithread->EncAndRecLuma(0, 0, 0, NULL);
            }


            if(cu_ithread->UpdateChromaRec() || !cu_ithread->HaveChromaRec())
                cu_ithread->EncAndRecChromaUpdate(0, 0, 0, interUpdate);

        }
        if (pars->UseDQP)
            m_frame->m_lastCodedQp[ctb_addr] = cu_ithread->UpdateCuQp(0, 0, GetLastCodedQP(cu_ithread, 0));
        else
            m_frame->m_lastCodedQp[ctb_addr] = cu_ithread->m_sliceQpY;

        if (m_frame->m_doPostProc) {
            if (m_videoParam.RDOQFlag) {
                small_memcpy(m_bsf[bsf_id].m_base.context_array + tab_ctxIdxOffset[QT_CBF_HEVC], context_array_save + tab_ctxIdxOffset[QT_CBF_HEVC],
                    sizeof(CABAC_CONTEXT_H265) * (tab_ctxIdxOffset[LAST_X_HEVC] - tab_ctxIdxOffset[QT_CBF_HEVC]));
                small_memcpy(m_bsf[bsf_id].m_base.context_array + tab_ctxIdxOffset[TRANS_SUBDIV_FLAG_HEVC], context_array_save + tab_ctxIdxOffset[TRANS_SUBDIV_FLAG_HEVC],
                    sizeof(CABAC_CONTEXT_H265) * (NUM_CABAC_CONTEXT - tab_ctxIdxOffset[TRANS_SUBDIV_FLAG_HEVC]));

                cu_ithread->EncodeCU(&m_bsf[bsf_id], 0, 0, RD_CU_ALL_EXCEPT_COEFFS);
            } else {
                m_bsf[bsf_id].CtxRestore(context_array_save);
                cu_ithread->EncodeCU(&m_bsf[bsf_id], 0, 0, RD_CU_ALL);
            }

            if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
                m_bsf[bsf_id].CtxSaveWPP(m_context_array_wpp_enc + NUM_CABAC_CONTEXT * ctb_row);

            m_bsf[bsf_id].CtxSave(m_bs[bs_id].m_base.context_array_enc);
        } else {
            cu_ithread->EncodeCU(&m_bs[bs_id], 0, 0, RD_CU_ALL);

            if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
                m_bs[bs_id].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);

            m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

            if ((m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
                (m_topEnc.m_pps.tiles_enabled_flag && end_of_tile_flag) ||
                end_of_slice_flag) {
#ifdef DEBUG_CABAC
                    int d = DEBUG_CABAC_PRINT;
                    DEBUG_CABAC_PRINT = 1;
#endif
                    if (!end_of_slice_flag)
                        m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), 1);
                    m_bs[bs_id].TerminateEncode_CABAC();
                    m_bs[bs_id].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                    DEBUG_CABAC_PRINT = d;
#endif
            }

            if (m_frame->m_isRef || pars->doDumpRecon)
                PadRectLumaAndChroma(*m_frame->m_recon, m_videoParam.fourcc, ctb_col*m_videoParam.MaxCUSize,
                    ctb_row*m_videoParam.MaxCUSize, m_videoParam.MaxCUSize, m_videoParam.MaxCUSize);

            // for frame threading (no slices, no tiles)
            if (ctb_col == pars->PicWidthInCtbs - 1)
                vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *> (&(m_frame->m_codedRow)));
        }
        break;

    case TT_POST_PROC_ROW:
    case TT_POST_PROC_CTU: {
        if (start_of_slice_flag ||
            (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == 0) ||
            (m_topEnc.m_pps.tiles_enabled_flag && start_of_tile_flag)) {

            ippiCABACInit_H265(&m_bs[bs_id].cabacState,
                m_bs[bs_id].m_base.m_pbsBase,
                m_bs[bs_id].m_base.m_bitOffset + (Ipp32u)(m_bs[bs_id].m_base.m_pbs - m_bs[bs_id].m_base.m_pbsBase) * 8,
                m_bs[bs_id].m_base.m_maxBsSize);

            if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && pars->PicWidthInCtbs > 1 &&
                (Ipp32s)ctb_addr - (Ipp32s)pars->PicWidthInCtbs + 1 >= m_frame->m_slices[curr_slice_id].slice_segment_address) {
                    m_bs[bs_id].CtxRestoreWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * (ctb_row - 1));
            } else {
                InitializeContextVariablesHEVC_CABAC(m_bs[bs_id].m_base.context_array, 2-m_frame->m_slices[curr_slice_id].slice_type, m_frame->m_sliceQpY);
            }
        }

        bool isRef = m_frame->m_isRef;
        bool doSao = m_topEnc.m_sps.sample_adaptive_offset_enabled_flag;

        if(doSao && !m_frame->m_slices[curr_slice_id].slice_sao_luma_flag && !m_frame->m_slices[curr_slice_id].slice_sao_chroma_flag)
            doSao = false;

        bool doDbl = pars->deblockingFlag;
        // subopt sao works (cpu only)
        if (doDbl && (isRef || (doSao && pars->saoSubOpt!=2) || pars->doDumpRecon)) {
            if (!pars->enableCmPostProc) {
                cu_ithread->Deblock();
            }
        }

        if (doSao) {
            m_bsf[bsf_id].CtxRestore(m_bs[bs_id].m_base.context_array);
            // here should be slice lambda always ?? Why?
            cu_ithread->m_rdLambda = m_frame->m_slices[curr_slice_id].rd_lambda_slice;

#ifdef AMT_DQP_FIX
#ifdef AMT_HROI_PSY_AQ
            if (m_videoParam.UseDQP && (m_videoParam.numRoi > 0 || (m_videoParam.DeltaQpMode & AMT_DQP_PSY_HROI))) {
#else
            if (m_videoParam.UseDQP && m_videoParam.numRoi > 0) {
#endif
                cu_ithread->SetCuLambdaRoi(m_frame);
            } else if(m_videoParam.UseDQP && m_videoParam.DeltaQpMode) {
                cu_ithread->SetCuLambda(m_frame);
            }
#endif

            if (m_videoParam.enableCmPostProc) {
                Ipp32s numCtbs = m_videoParam.PicWidthInCtbs * m_videoParam.PicHeightInCtbs;
                Ipp32s compCount = m_videoParam.SAOChromaFlag ? 3 : 1;

                for (Ipp32s compIdx = 0; compIdx < compCount; compIdx++) 
                //Ipp32s compIdx = 0;
                {
                    const SaoOffsetOut_gfx * saoModes = ((SaoOffsetOut_gfx *)m_frame->m_feiSaoModes->m_sysmem + numCtbs*compIdx + ctb_addr);
                    if (saoModes->mode_idx == 2) {
                        m_saoParam[ctb_addr][compIdx].mode_idx = SAO_MODE_MERGE;
                        m_saoParam[ctb_addr][compIdx].type_idx = SAO_MERGE_LEFT;
                    } else if (saoModes->mode_idx == 3) {
                        m_saoParam[ctb_addr][compIdx].mode_idx = SAO_MODE_MERGE;
                        m_saoParam[ctb_addr][compIdx].type_idx = SAO_MERGE_ABOVE;
                    } else {
                        m_saoParam[ctb_addr][compIdx].mode_idx = saoModes->mode_idx;
                        m_saoParam[ctb_addr][compIdx].type_idx = saoModes->type_idx;
                    }
                    int startIdx = m_saoParam[ctb_addr][compIdx].typeAuxInfo = saoModes->startBand_idx;
                    m_saoParam[ctb_addr][compIdx].saoMaxOffsetQVal = 7;
                    if (m_saoParam[ctb_addr][compIdx].type_idx == SAO_TYPE_BO) {
                        for (int idx = 0; idx < 4; idx++)
                            m_saoParam[ctb_addr][compIdx].offset[startIdx + idx] = saoModes->offset[idx];
                    } else {
                        m_saoParam[ctb_addr][compIdx].offset[0] = saoModes->offset[0];
                        m_saoParam[ctb_addr][compIdx].offset[1] = saoModes->offset[1];
                        m_saoParam[ctb_addr][compIdx].offset[2] = 0;
                        m_saoParam[ctb_addr][compIdx].offset[3] = saoModes->offset[2];
                        m_saoParam[ctb_addr][compIdx].offset[4] = saoModes->offset[3];
                    }
                }
            } else {
                cu_ithread->EstimateSao(&m_bs[bs_id], m_saoParam);
            }
            cu_ithread->PackSao(&m_bs[bs_id], m_saoParam);
        }

#ifdef DEBUG_CABAC
    printf("\n");
    if (ctb_addr == 0) printf("Start POC %d\n",m_frame->m_poc);
    printf("CTB %d\n",ctb_addr);

    if (ctb_addr == 0 && m_frame->m_poc == 4)
        printf("");
#endif

        cu_ithread->EncodeCU(&m_bs[bs_id], 0, 0, 0);

        if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == 1)
            m_bs[bs_id].CtxSaveWPP(m_context_array_wpp + NUM_CABAC_CONTEXT * ctb_row);

        m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), end_of_slice_flag);

        if ((m_topEnc.m_pps.entropy_coding_sync_enabled_flag && ctb_col == pars->PicWidthInCtbs - 1) ||
            (m_topEnc.m_pps.tiles_enabled_flag && end_of_tile_flag) ||
            end_of_slice_flag) {
#ifdef DEBUG_CABAC
                int d = DEBUG_CABAC_PRINT;
                DEBUG_CABAC_PRINT = 1;
#endif
                if (!end_of_slice_flag)
                    m_bs[bs_id].EncodeSingleBin_CABAC(CTX(&m_bs[bs_id],END_OF_SLICE_FLAG_HEVC), 1);
                m_bs[bs_id].TerminateEncode_CABAC();
                m_bs[bs_id].ByteAlignWithZeros();
#ifdef DEBUG_CABAC
                DEBUG_CABAC_PRINT = d;
#endif
        }

        if ( (isRef || pars->doDumpRecon) && m_videoParam.enableCmPostProc == 0) {
            if (doSao)
                ApplySaoCtu<PixType>(ctb_row, ctb_col);
            PadRectLumaAndChroma(*m_frame->m_recon, m_videoParam.fourcc, ctb_col*m_videoParam.MaxCUSize,
                ctb_row*m_videoParam.MaxCUSize, m_videoParam.MaxCUSize, m_videoParam.MaxCUSize);
        }

        // for frame threading (no slices, no tiles)
        if (ctb_col == pars->PicWidthInCtbs - 1)
            vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *> (&(m_frame->m_codedRow)));

        } break;
    default:
        VM_ASSERT(0);
    }
    }

    vm_interlocked_inc32(&m_frame->m_numFinishedThreadingTasks);

    return MFX_TASK_DONE;
}


//inline
void H265Enc::AddTaskDependency(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool, bool threaded)
{
    // if dep num exceeded
    if (!threaded)
        assert(upstream->finished == 0);
    assert(downstream->finished == 0);

#ifdef DEBUG_NTM
//    char after_all_changes_need_to_rethink_this_part[-1];
    // if dep num exceeded
//    assert(downstream->numUpstreamDependencies < MAX_NUM_DEPENDENCIES);
    if (downstream->numUpstreamDependencies < MAX_NUM_DEPENDENCIES)
        downstream->upstreamDependencies[downstream->numUpstreamDependencies] = upstream;
#endif

    if (ttHubPool && upstream->numDownstreamDependencies == MAX_NUM_DEPENDENCIES) {
        assert(MAX_NUM_DEPENDENCIES > 1);
        ThreadingTask *hub = ttHubPool->Allocate();
        Copy(hub->downstreamDependencies, upstream->downstreamDependencies);
        hub->numDownstreamDependencies = upstream->numDownstreamDependencies;
        upstream->numDownstreamDependencies = 0;
        AddTaskDependency(hub, upstream, NULL, threaded);
        AddTaskDependency(downstream, upstream, NULL, threaded);
    } else {
        assert(upstream->numDownstreamDependencies < MAX_NUM_DEPENDENCIES);
        if (threaded) {
            vm_interlocked_inc32(&downstream->numUpstreamDependencies);
        } else {
            downstream->numUpstreamDependencies++;
        }
        upstream->downstreamDependencies[upstream->numDownstreamDependencies++] = downstream;
    }
}

void H265Enc::AddTaskDependencyThreaded(ThreadingTask *downstream, ThreadingTask *upstream, ObjectPool<ThreadingTask> *ttHubPool)
{
    assert(downstream->finished == 0);

    if (vm_interlocked_cas32(&upstream->finished, 1, 0) == 0) {
        AddTaskDependency(downstream, upstream, ttHubPool, true);
        vm_interlocked_dec32(&upstream->finished);
    }
}

// no thread safe !!!
void H265FrameEncoder::SetEncodeFrame_GpuPostProc(Frame* frame, std::deque<ThreadingTask *> *m_pendingTasks)
{
    m_frame = frame;

    Ipp32u startRow = 0, endRow = m_videoParam.PicHeightInCtbs;
    if (m_videoParam.RegionIdP1 > 0) {
        startRow = m_frame->m_slices[m_videoParam.RegionIdP1 - 1].row_first;
        endRow = m_frame->m_slices[m_videoParam.RegionIdP1 - 1].row_last + 1;
    }

    Ipp32u pp_by_row = m_videoParam.m_framesInParallel > 1 && !m_videoParam.enableCmFlag &&
                       m_videoParam.NumTiles == 1 && m_videoParam.NumSlices == 1;

    m_frame->m_numFinishedThreadingTasks = 0;
    m_frame->m_numThreadingTasks = pp_by_row ? (m_videoParam.PicWidthInCtbs + 1) * (endRow - startRow) : 
        m_videoParam.PicWidthInCtbs * (endRow - startRow) * 2;

    m_frame->m_codedRow = m_videoParam.RegionIdP1 > 0 ? m_frame->m_slices[m_videoParam.RegionIdP1 - 1].row_first : 0;

    for (Ipp32u i = 0; i < m_videoParam.num_bs_subsets; i++)
        m_bs[i].Reset();

    Ipp32u ctb_addr;
    m_numTasksPerCu = 2;
    for (Ipp32u ctb_addr = m_videoParam.PicWidthInCtbs * startRow; ctb_addr < m_videoParam.PicWidthInCtbs * endRow; ctb_addr++) {
        ThreadingTask *task_enc = m_frame->m_threadingTasks + m_numTasksPerCu * ctb_addr;
        task_enc[0].numDownstreamDependencies = 0;
        task_enc[1].numDownstreamDependencies = 0;
        task_enc[0].numUpstreamDependencies = 0;
        task_enc[1].numUpstreamDependencies = 0;
        task_enc[0].finished = 0;
        task_enc[1].finished = 0;
    }
    m_topEnc.m_ttRootTasks.resize(0);

    ctb_addr = m_videoParam.PicWidthInCtbs * startRow;

    // MAIN LOOP
    // FOR EACH REGION IN FRAME
    // FOR EACH CTB IN REGION
    for (Ipp32u ctb_row = startRow; ctb_row < endRow; ctb_row++) {
        for (Ipp32u ctb_col = 0; ctb_col < m_videoParam.PicWidthInCtbs; ctb_col++) {
            Ipp32s tile_id = m_videoParam.m_tile_ids[ctb_addr];
            Ipp32s slice_id = m_videoParam.m_slice_ids[ctb_addr];
            Ipp32u regionCtbColFirst, regionCtbColLast, regionCtbRowFirst, regionCtbRowLast;

            // slice/tile configuration
            {
                if (m_videoParam.NumTiles > 1) {
                    Ipp32s tile_row = m_videoParam.m_tile_row_ids[ctb_addr];
                    Ipp32s tile_col = m_videoParam.m_tile_col_ids[ctb_addr];

                    Ipp32u tile_border_left = m_videoParam.tileColStart[tile_col] << m_videoParam.Log2MaxCUSize;
                    Ipp32u tile_border_right = (m_videoParam.tileColStart[tile_col] + m_videoParam.tileColWidth[tile_col]) << m_videoParam.Log2MaxCUSize;
                    if (tile_border_right > m_videoParam.Width)
                        tile_border_right = m_videoParam.Width;

                    Ipp32u tile_border_top = m_videoParam.tileRowStart[tile_row] << m_videoParam.Log2MaxCUSize;
                    Ipp32u tile_border_bottom = (m_videoParam.tileRowStart[tile_row] + m_videoParam.tileRowHeight[tile_row]) << m_videoParam.Log2MaxCUSize;
                    if (tile_border_bottom > m_videoParam.Height)
                        tile_border_bottom = m_videoParam.Height;

                    regionCtbColFirst = m_videoParam.tileColStart[tile_col];
                    regionCtbColLast = m_videoParam.tileColStart[tile_col] + m_videoParam.tileColWidth[tile_col] - 1;
                    regionCtbRowFirst = m_videoParam.tileRowStart[tile_row];
                    regionCtbRowLast = m_videoParam.tileRowStart[tile_row] + m_videoParam.tileRowHeight[tile_row] - 1;

                } else if (m_videoParam.NumSlices > 1) {
                    H265Slice *slice = &m_frame->m_slices[slice_id];
                    regionCtbColFirst = 0;
                    regionCtbColLast = m_videoParam.PicWidthInCtbs - 1;
                    regionCtbRowFirst = slice->row_first;
                    regionCtbRowLast = slice->row_last;
                } else {
                    regionCtbColFirst = regionCtbRowFirst = 0;
                    regionCtbColLast = m_videoParam.PicWidthInCtbs - 1;
                    regionCtbRowLast = m_videoParam.PicHeightInCtbs - 1;
                }
            }

            // m_threadingTasks = [enc0, pp0] [enc1, pp1] [enc2, pp2] ... [encN, ppN]
            ThreadingTask *task_enc = m_frame->m_threadingTasks + m_numTasksPerCu * ctb_addr;
            ThreadingTask *task_pp = m_frame->m_threadingTasks + m_numTasksPerCu * ctb_addr + 1;
            task_enc->action = TT_ENCODE_CTU;
            task_pp->action = pp_by_row ? TT_POST_PROC_ROW : TT_POST_PROC_CTU;
            task_enc->row = task_pp->row = ctb_row;
            task_enc->col = task_pp->col = ctb_col;
            task_enc->fenc = task_pp->fenc = this;
            task_enc->poc = task_pp->poc = m_frame->m_frameOrder;
            bool isTaskStored = false;

            // FRAME THREADING & PP
#if defined(FRAME_PARALLEL_TILES)
            if (m_videoParam.m_framesInParallel > 1 && (ctb_col == 0 || ctb_col == regionCtbColFirst) && !m_videoParam.enableCmFlag)
#else
            if (m_videoParam.m_framesInParallel > 1 && ctb_col == 0 && !m_videoParam.enableCmFlag)
#endif
            {

                Ipp32u refRowLag = m_videoParam.m_lagBehindRefRows;
                H265Slice* slice = &m_frame->m_slices[0];

                for (int list = 0; list < 2; list++) {
                    RefPicList* refList = &(m_frame->m_refPicList[list]);
                    for (int refIdx = 0; refIdx < slice->num_ref_idx[list]; refIdx++) {
                        if (list == 1 && m_frame->m_mapRefIdxL1ToL0[refIdx] > -1)
                            continue;

                        Frame *ref = refList->m_refFrames[refIdx];
                        Ipp32u refRow = ctb_row + refRowLag;
                        if (refRow > endRow && ctb_row == regionCtbRowFirst && ctb_col == regionCtbColFirst)
                            refRow = endRow;

#if defined(FRAME_PARALLEL_TILES)
                        assert(m_videoParam.NumTiles == 1 || (ref->m_doPostProc && m_videoParam.deblockBordersFlag));
#endif
                        if (refRow <= endRow && ref->m_codedRow < (Ipp32s)refRow) {

                            Ipp32u refCtbAddr = refRow * m_videoParam.PicWidthInCtbs - 1;
                            ThreadingTask *task_ref = ref->m_threadingTasks + m_numTasksPerCu * refCtbAddr;

                            if (ref->m_doPostProc)
                                task_ref++;

                            if (!isTaskStored) {
                                vm_interlocked_inc32(&task_enc->numUpstreamDependencies);
                                m_topEnc.m_ttRootTasks.push_back(task_enc);
                                isTaskStored = true;
                            }
#if !defined(FRAME_PARALLEL_TILES)
                            AddTaskDependencyThreaded(task_enc, task_ref); // ENCODE_CTU(poc, N, 0) <- PP_CTU(refpoc, N+lag, widthInCtu-1)
#else
                            AddTaskDependencyThreaded(task_enc, task_ref, &m_topEnc.m_ttHubPool); // ENCODE_CTU(poc, N, 0) <- PP_CTU(refpoc, N+lag, widthInCtu-1)
#endif
                        }
                    }
                }
            }


            // FIRST BLOCK IN REGION
            if (ctb_col == regionCtbColFirst && ctb_row == regionCtbRowFirst)
            {
                if (!isTaskStored) {
                    vm_interlocked_inc32(&task_enc->numUpstreamDependencies);
                    m_topEnc.m_ttRootTasks.push_back(task_enc);
                    isTaskStored = true;
                }
                AddTaskDependencyThreaded(task_enc, &frame->m_ttInitNewFrame, &m_topEnc.m_ttHubPool); // ENCODE_CTU(0,0) <- INIT_NEW_FRAME
                if (m_videoParam.enableCmFlag) {

                    if (m_videoParam.enableCmPostProc) {
                        for (Ipp32s i = 0; i < 2; i++) {
                            const RefPicList &list = frame->m_refPicList[i];
                            for (Ipp32s j = 0; j < list.m_refFramesCount; j++) {
                                if (i == 1 && frame->m_mapRefIdxL1ToL0[j] != -1)
                                    continue;
                                Frame *ref = list.m_refFrames[j];
                                AddTaskDependencyThreaded(task_enc, &ref->m_ttPadRecon); // ENCODE_CTU(0, 0) <- PAD_RECON(ref)
                            }
                        }
                    }

                    if (frame->m_slices[0].sliceIntraAngMode == INTRA_ANG_MODE_GRADIENT)
                        AddTaskDependencyThreaded(task_enc, &frame->m_ttWaitGpuIntra, &m_topEnc.m_ttHubPool); // ENCODE_CTU(0,0) <- GPU_INTRA

                    for (Ipp32s i = 0; i < frame->m_numRefUnique; i++)
                        AddTaskDependencyThreaded(task_enc, &frame->m_ttWaitGpuMe[i], &m_topEnc.m_ttHubPool); // ENCODE_CTU(0,0) <- GPU_ME(allrefs)

                    if (m_videoParam.CmBirefineFlag && frame->m_slices[0].slice_type == B_SLICE)
                        AddTaskDependencyThreaded(task_enc, &frame->m_ttWaitGpuBiref, &m_topEnc.m_ttHubPool); // ENCODE_CTU(0,0) <- GPU_BIREF

                    if (m_videoParam.enableCmPostProc && m_frame->m_doPostProc)
                        AddTaskDependencyThreaded(task_pp, &frame->m_ttWaitGpuPostProc, &m_topEnc.m_ttHubPool); // PP_CTU(0,0) <- GPU_PP

                }
            }
            // REMAINDER BLOCKS IN REGION
            else
            {
                if (m_topEnc.m_pps.entropy_coding_sync_enabled_flag) {
                    if (ctb_col > regionCtbColFirst) {
                        AddTaskDependency(task_enc, task_enc - m_numTasksPerCu);

                        if (m_frame->m_doPostProc && !pp_by_row)
                            AddTaskDependency(task_pp, task_pp - m_numTasksPerCu);
                    }
                    if (ctb_row > regionCtbRowFirst) {
                        if (regionCtbColFirst == regionCtbColLast) { // special case: frame width <= 1 CTB, frame height > 1 CTB
                            AddTaskDependency(task_enc, task_enc - m_videoParam.PicWidthInCtbs * m_numTasksPerCu);
                            if (m_videoParam.enableCmFlag)
                                AddTaskDependency(task_pp, task_pp - m_videoParam.PicWidthInCtbs * m_numTasksPerCu);
                        } else if (ctb_col < regionCtbColLast) {
                            AddTaskDependency(task_enc, task_enc - (m_videoParam.PicWidthInCtbs - 1) * m_numTasksPerCu);

                            if (m_frame->m_doPostProc && !pp_by_row)
                                AddTaskDependency(task_pp, task_pp - (m_videoParam.PicWidthInCtbs - 1) * m_numTasksPerCu);
                        }
                    }
                } else {
                    if (ctb_col > regionCtbColFirst) {
                        AddTaskDependency(task_enc, task_enc - m_numTasksPerCu);

                        if (m_frame->m_doPostProc && !pp_by_row)
                            AddTaskDependency(task_pp, task_pp - m_numTasksPerCu);
                    } else {
                        AddTaskDependency(task_enc, task_enc - (m_videoParam.PicWidthInCtbs - (regionCtbColLast - regionCtbColFirst)) * m_numTasksPerCu);

                        if (m_frame->m_doPostProc && !pp_by_row)
                            AddTaskDependency(task_pp, task_pp - (m_videoParam.PicWidthInCtbs - (regionCtbColLast - regionCtbColFirst)) * m_numTasksPerCu);
                    }
                }
#if !defined(FRAME_PARALLEL_TILES)
                if(m_frame->m_doPostProc && pp_by_row && ctb_row > regionCtbRowFirst && ctb_col == regionCtbColLast)    //!! Seems wrong !!
#else
                if (m_frame->m_doPostProc && pp_by_row && ctb_row > 0 && ctb_col == m_videoParam.PicWidthInCtbs - 1)
#endif
                {
                    assert(m_videoParam.NumSlices == 1);
                    AddTaskDependency(task_pp, task_pp - m_videoParam.PicWidthInCtbs * m_numTasksPerCu);
                }
            }


            // LAST BLOCK IN REGION
            if (ctb_row == regionCtbRowLast && ctb_col == regionCtbColLast)
            {
                if (m_videoParam.enableCmPostProc && m_frame->m_doPostProc) {
                    AddTaskDependency(&m_frame->m_ttSubmitGpuPostProc, task_enc);
                }
            }


            if (m_frame->m_doPostProc && m_videoParam.deblockBordersFlag && !pp_by_row) {
                if (ctb_col > 0 && ctb_col == regionCtbColFirst) {
                    AddTaskDependency(task_pp, task_pp - m_numTasksPerCu);

                    if (ctb_row > 0 && ctb_row == regionCtbRowFirst) {
                        AddTaskDependency(task_pp, task_pp - (m_videoParam.PicWidthInCtbs + 1) * m_numTasksPerCu);
                    }
                }
                if (ctb_row > 0 && ctb_row == regionCtbRowFirst) {
                    AddTaskDependency(task_pp, task_pp - m_videoParam.PicWidthInCtbs * m_numTasksPerCu);
                }

                // CAREFULLY!!! SUBSTITUTION!!!!
                regionCtbColFirst = regionCtbRowFirst = 0;
                regionCtbColLast = m_videoParam.PicWidthInCtbs - 1;
                regionCtbRowLast = m_videoParam.PicHeightInCtbs - 1;
            }


            // possible to make less dependencies here but need to add extra checks
#if !defined(FRAME_PARALLEL_TILES)
            if (m_frame->m_doPostProc && (!pp_by_row || ctb_col == regionCtbColLast)) {
#else
            if (m_frame->m_doPostProc && !pp_by_row) {
#endif
                AddTaskDependency(task_pp, task_enc);

                if (ctb_row < regionCtbRowLast && ctb_col < regionCtbColLast)
                    AddTaskDependency(task_pp, task_enc + (m_videoParam.PicWidthInCtbs + 1) * m_numTasksPerCu);

                if (ctb_row < regionCtbRowLast)
                    AddTaskDependency(task_pp, task_enc + (m_videoParam.PicWidthInCtbs) * m_numTasksPerCu);

                if (ctb_col < regionCtbColLast)
                    AddTaskDependency(task_pp, task_enc + m_numTasksPerCu);
            }
#if defined(FRAME_PARALLEL_TILES)
            else if (m_frame->m_doPostProc && pp_by_row && ctb_col == m_videoParam.PicWidthInCtbs - 1)
            {
                AddTaskDependency(task_pp, task_enc);

                if (ctb_row < m_videoParam.PicHeightInCtbs - 1)
                    AddTaskDependency(task_pp, task_enc + m_videoParam.PicWidthInCtbs * m_numTasksPerCu);

            }
#endif


            // HERE regionCtbRowLast & regionCtbColLast could be changed!!!!
#if !defined(FRAME_PARALLEL_TILES)
            if (ctb_row == regionCtbRowLast && ctb_col == regionCtbColLast) {
#else
            if (((!m_frame->m_doPostProc || !m_videoParam.deblockBordersFlag) && ctb_row == regionCtbRowLast && ctb_col == regionCtbColLast)
                || (ctb_row == m_videoParam.PicHeightInCtbs - 1 && ctb_col == m_videoParam.PicWidthInCtbs - 1)) {
#endif
                AddTaskDependency(&m_frame->m_ttEncComplete, m_frame->m_doPostProc ? task_pp : task_enc);

                if (m_videoParam.enableCmFlag && !m_videoParam.enableCmPostProc && frame->m_isRef)
                    AddTaskDependency(&m_frame->m_ttSubmitGpuCopyRec, m_frame->m_doPostProc ? task_pp : task_enc);
            }

            ctb_addr ++;
        }
    }

    std::lock_guard<std::mutex> guard(m_topEnc.m_critSect);
    for (Ipp32u i = 0; i < m_topEnc.m_ttRootTasks.size(); i++)
    {
        if (vm_interlocked_dec32(&m_topEnc.m_ttRootTasks[i]->numUpstreamDependencies) == 0) {
            m_pendingTasks->push_back(m_topEnc.m_ttRootTasks[i]);
        }
    } 
}


template mfxStatus H265FrameEncoder::PerformThreadingTask<Ipp8u>(ThreadingTaskSpecifier action, Ipp32u ctb_row, Ipp32u ctb_col);
template mfxStatus H265FrameEncoder::PerformThreadingTask<Ipp16u>(ThreadingTaskSpecifier action, Ipp32u ctb_row, Ipp32u ctb_col);

//} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE