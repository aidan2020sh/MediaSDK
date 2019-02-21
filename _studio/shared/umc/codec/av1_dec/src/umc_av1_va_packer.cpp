// Copyright (c) 2017-2019 Intel Corporation
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

#include "umc_vp9_dec_defs.h"

#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#include <algorithm>
#include "umc_structures.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"
#include "umc_av1_utils.h"

#include "umc_av1_frame.h"
#include "umc_av1_va_packer.h"

#ifdef UMC_VA_DXVA
#include "umc_va_base.h"
#include "umc_va_dxva2_protected.h"
#endif

using namespace UMC;

namespace UMC_AV1_DECODER
{
    Packer * Packer::CreatePacker(UMC::VideoAccelerator * va)
    {
#ifdef UMC_VA_DXVA
        return new PackerIntel(va);
#elif defined(UMC_VA_LINUX)
        return new PackerVA(va);
#else
        return 0;
#endif
    }

    Packer::Packer(UMC::VideoAccelerator * va)
        : m_va(va)
    {}

    Packer::~Packer()
    {}

#ifdef UMC_VA_DXVA

    /****************************************************************************************************/
    // DXVA Windows packer implementation
    /****************************************************************************************************/
    PackerDXVA::PackerDXVA(VideoAccelerator * va)
        : Packer(va)
        , m_report_counter(1)
    {}

    Status PackerDXVA::GetStatusReport(void * pStatusReport, size_t size)
    {
        return
            m_va->ExecuteStatusReportBuffer(pStatusReport, (uint32_t)size);
    }

    void PackerDXVA::BeginFrame()
    {
        m_report_counter++;
    }

    void PackerDXVA::EndFrame()
    {
    }

    PackerIntel::PackerIntel(VideoAccelerator * va)
        : PackerDXVA(va)
    {}

    void PackerIntel::PackAU(std::vector<TileSet>& tileSets, AV1DecoderFrame const& info, bool firtsSubmission)
    {
        if (firtsSubmission)
        {
            // it's first submission for current frame - need to fill and submit picture parameters
            UMC::UMCVACompBuffer* compBufPic = NULL;
            DXVA_Intel_PicParams_AV1 *picParam = (DXVA_Intel_PicParams_AV1*)m_va->GetCompBuffer(DXVA_PICTURE_DECODE_BUFFER, &compBufPic);
            if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < sizeof(DXVA_Intel_PicParams_AV1)))
                throw UMC_VP9_DECODER::vp9_exception(MFX_ERR_MEMORY_ALLOC);

            compBufPic->SetDataSize(sizeof(DXVA_Intel_PicParams_AV1));
            *picParam = DXVA_Intel_PicParams_AV1{};

            PackPicParams(*picParam, info);
        }

        UMC::UMCVACompBuffer* compBufBs = nullptr;
        uint8_t* const bistreamData = (uint8_t *)m_va->GetCompBuffer(DXVA_BITSTREAM_DATA_BUFFER, &compBufBs);
        if (!bistreamData || !compBufBs)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::vector<DXVA_Intel_Tile_AV1> tileControlParams;

        size_t offsetInBuffer = 0;
        for (auto& tileSet : tileSets)
        {
            const size_t spaceInBuffer = compBufBs->GetBufferSize() - offsetInBuffer;
            TileLayout layout;
            const size_t bytesSubmitted = tileSet.Submit(bistreamData, spaceInBuffer, offsetInBuffer, layout);

            if (bytesSubmitted)
            {
                offsetInBuffer += bytesSubmitted;

                for (auto& loc : layout)
                {
                    tileControlParams.emplace_back();
                    PackTileControlParams(tileControlParams.back(), loc);
                }
            }
        }
        compBufBs->SetDataSize(static_cast<uint32_t>(offsetInBuffer));

        UMCVACompBuffer* compBufTile = nullptr;
        const int32_t tileControlInfoSize = static_cast<int32_t>(sizeof(DXVA_Intel_Tile_AV1) * tileControlParams.size());
        DXVA_Intel_Tile_AV1 *tileControlParam = (DXVA_Intel_Tile_AV1*)m_va->GetCompBuffer(DXVA_SLICE_CONTROL_BUFFER, &compBufTile);
        if (!tileControlParam || !compBufTile || (compBufTile->GetBufferSize() < tileControlInfoSize))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::copy(tileControlParams.begin(), tileControlParams.end(), tileControlParam);
        compBufTile->SetDataSize(tileControlInfoSize);
    }

    void PackerIntel::PackPicParams(DXVA_Intel_PicParams_AV1& picParam, AV1DecoderFrame const& frame)
    {
        SequenceHeader const& sh = frame.GetSeqHeader();

        picParam.frame_width_minus1 = (USHORT)frame.GetUpscaledWidth() - 1;
        picParam.frame_height_minus1 = (USHORT)frame.GetHeight() - 1;

        FrameHeader const& info =
            frame.GetFrameHeader();

        // fill seq parameters
        picParam.profile = (UCHAR)sh.seq_profile;

        auto& ddiSeqParam = picParam.dwSeqInfoFlags.fields;

#if AV1D_DDI_VERSION >= 26
        picParam.order_hint_bits_minus_1 = (UCHAR)sh.order_hint_bits_minus1;

        picParam.BitDepthIdx = (sh.color_config.BitDepth == 10) ? 1 :
            (sh.color_config.BitDepth == 12) ? 2 : 0;

        ddiSeqParam.use_128x128_superblock = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        ddiSeqParam.enable_filter_intra = sh.enable_filter_intra;
        ddiSeqParam.enable_intra_edge_filter = sh.enable_intra_edge_filter;

        ddiSeqParam.enable_interintra_compound = sh.enable_interintra_compound;
        ddiSeqParam.enable_masked_compound = sh.enable_masked_compound;

        ddiSeqParam.enable_cdef = sh.enable_cdef;
#if AV1D_DDI_VERSION < 33
        ddiSeqParam.enable_restoration = sh.enable_restoration;
#endif

#if AV1D_DDI_VERSION < 30
        ddiSeqParam.large_scale_tile = info.large_scale_tile;
#endif

#else // AV1D_DDI_VERSION >= 26
        ddiSeqParam.sb_size_128x128 = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        ddiSeqParam.enable_filter_intra = info.enable_filter_intra;
        ddiSeqParam.enable_intra_edge_filter = info.enable_intra_edge_filter;

        ddiSeqParam.enable_interintra_compound = info.enable_interintra_compound;
        ddiSeqParam.enable_masked_compound = info.enable_masked_compound;

        ddiSeqParam.enable_cdef = 1;
        ddiSeqParam.enable_restoration = 1;
        ddiSeqParam.BitDepthIdx = (sh.color_config.BitDepth == 10) ? 1 :
            (sh.color_config.BitDepth == 12) ? 2 : 0;
#endif // AV1D_DDI_VERSION >= 26

        ddiSeqParam.still_picture = 0;
        ddiSeqParam.enable_dual_filter = sh.enable_dual_filter;
        ddiSeqParam.enable_order_hint = sh.enable_order_hint;
        ddiSeqParam.enable_jnt_comp = sh.enable_jnt_comp;

        ddiSeqParam.mono_chrome = sh.color_config.mono_chrome;
        ddiSeqParam.color_range = sh.color_config.color_range;
        ddiSeqParam.subsampling_x = sh.color_config.subsampling_x;
        ddiSeqParam.subsampling_y = sh.color_config.subsampling_y;
        ddiSeqParam.chroma_sample_position = sh.color_config.chroma_sample_position;
        ddiSeqParam.film_grain_params_present = sh.film_grain_param_present;
#ifdef DDI_HACKS_FOR_REV_5
        ddiSeqParam.order_hint_bits_minus1 = sh.order_hint_bits_minus1;
#endif

        // fill pic params
        auto& ddiPicParam = picParam.dwPicInfoFlags.fields;

        ddiPicParam.frame_type = info.frame_type;
        ddiPicParam.show_frame = info.show_frame;
        ddiPicParam.showable_frame = info.showable_frame;
        ddiPicParam.error_resilient_mode = info.error_resilient_mode;
        ddiPicParam.disable_cdf_update = info.disable_cdf_update;
        ddiPicParam.allow_screen_content_tools = info.allow_screen_content_tools;
        ddiPicParam.force_integer_mv = info.force_integer_mv;
        ddiPicParam.allow_intrabc = info.allow_intrabc;
        ddiPicParam.use_superres = (info.SuperresDenom == SCALE_NUMERATOR) ? 0 : 1;
        ddiPicParam.allow_high_precision_mv = info.allow_high_precision_mv;
        ddiPicParam.is_motion_mode_switchable = info.is_motion_mode_switchable;
        ddiPicParam.use_ref_frame_mvs = info.use_ref_frame_mvs;
        ddiPicParam.disable_frame_end_update_cdf = info.disable_frame_end_update_cdf;
        ddiPicParam.uniform_tile_spacing_flag = info.tile_info.uniform_tile_spacing_flag;
        ddiPicParam.allow_warped_motion = info.allow_warped_motion;

#if AV1D_DDI_VERSION >= 30
        ddiPicParam.large_scale_tile = info.large_scale_tile;
#endif

#if AV1D_DDI_VERSION < 26
        ddiPicParam.refresh_frame_context = info.disable_frame_end_update_cdf ? REFRESH_FRAME_CONTEXT_DISABLED : REFRESH_FRAME_CONTEXT_BACKWARD;
        ddiPicParam.large_scale_tile = info.large_scale_tile;
#endif

        picParam.order_hint = (UCHAR)info.order_hint;
        picParam.superres_scale_denominator = (UCHAR)info.SuperresDenom;

        picParam.frame_interp_filter = (UCHAR)info.interpolation_filter;

        picParam.stAV1Segments.enabled = info.segmentation_params.segmentation_enabled;
        picParam.stAV1Segments.temporal_update = info.segmentation_params.segmentation_temporal_update;
        picParam.stAV1Segments.update_map = info.segmentation_params.segmentation_update_map;
        picParam.stAV1Segments.update_data = info.segmentation_params.segmentation_update_data;
        picParam.stAV1Segments.Reserved4Bits = 0;

        if (picParam.stAV1Segments.enabled)
        {
            for (uint8_t i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
            {
                picParam.stAV1Segments.feature_mask[i] = (UCHAR)info.segmentation_params.FeatureMask[i];
                for (uint8_t j = 0; j < SEG_LVL_MAX; j++)
                    picParam.stAV1Segments.feature_data[i][j] = (SHORT)info.segmentation_params.FeatureData[i][j];
            }
        }

        if (KEY_FRAME == info.frame_type)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
#if AV1D_DDI_VERSION >= 31
                picParam.ref_frame_map[i].bPicEntry = UCHAR_MAX;
#else
                picParam.ref_frame_map[i].wPicEntry = USHRT_MAX;
#endif
        }
        else
        {
            for (uint8_t ref = 0; ref < NUM_REF_FRAMES; ++ref)
#if AV1D_DDI_VERSION >= 31
                picParam.ref_frame_map[ref].bPicEntry = (UCHAR)frame.frame_dpb[ref]->GetMemID(SURFACE_RECON);
#else
                picParam.ref_frame_map[ref].wPicEntry = (USHORT)frame.frame_dpb[ref]->GetMemID(SURFACE_RECON);
#endif

            for (uint8_t ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                uint8_t idxInDPB = (uint8_t)info.ref_frame_idx[ref_idx];

                picParam.ref_frame_idx[ref_idx] = idxInDPB;
#if AV1D_DDI_VERSION < 26
                picParam.ref_order_hint[ref_idx] = (UCHAR)frame.frame_dpb[idxInDPB]->GetFrameHeader().order_hint;
#endif
            }
        }

#if AV1D_DDI_VERSION >= 31
        picParam.CurrPic.bPicEntry = (UCHAR)frame.GetMemID(SURFACE_RECON);
#else
        picParam.CurrPic.wPicEntry = (USHORT)frame.GetMemID(SURFACE_RECON);
#endif
        if (!frame.FilmGrainDisabled())
#if AV1D_DDI_VERSION >= 31
            picParam.CurrDisplayPic.bPicEntry = (UCHAR)frame.GetMemID();
#else
            picParam.CurrDisplayPic.wPicEntry = (USHORT)frame.GetMemID();
#endif
        picParam.primary_ref_frame = (UCHAR)info.primary_ref_frame;

        picParam.filter_level[0] = (UCHAR)info.loop_filter_params.loop_filter_level[0];
        picParam.filter_level[1] = (UCHAR)info.loop_filter_params.loop_filter_level[1];
        picParam.filter_level_u = (UCHAR)info.loop_filter_params.loop_filter_level[2];
        picParam.filter_level_v = (UCHAR)info.loop_filter_params.loop_filter_level[3];

        auto& ddiLFInfoFlags = picParam.cLoopFilterInfoFlags.fields;
        ddiLFInfoFlags.sharpness_level = (UCHAR)info.loop_filter_params.loop_filter_sharpness;
        ddiLFInfoFlags.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        ddiLFInfoFlags.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;

        picParam.StatusReportFeedbackNumber = ++m_report_counter;

        for (uint8_t i = 0; i < TOTAL_REFS; i++)
        {
            picParam.ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (uint8_t i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam.mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }

        picParam.base_qindex = (SHORT)info.quantization_params.base_q_idx;
        picParam.y_dc_delta_q = (CHAR)info.quantization_params.DeltaQYDc;
        picParam.u_dc_delta_q = (CHAR)info.quantization_params.DeltaQUDc;
        picParam.v_dc_delta_q = (CHAR)info.quantization_params.DeltaQVDc;
        picParam.u_ac_delta_q = (CHAR)info.quantization_params.DeltaQUAc;
        picParam.v_ac_delta_q = (CHAR)info.quantization_params.DeltaQVAc;

        picParam.cdef_damping_minus_3 = (UCHAR)(info.cdef_params.cdef_damping - 3);
        picParam.cdef_bits = (UCHAR)info.cdef_params.cdef_bits;

        for (uint8_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
#if UMC_AV1_DECODER_REV >= 8500
            picParam.cdef_y_strengths[i] = (UCHAR)((info.cdef_params.cdef_y_pri_strength[i] << 2) + info.cdef_params.cdef_y_sec_strength[i]);
            picParam.cdef_uv_strengths[i] = (UCHAR)((info.cdef_params.cdef_uv_pri_strength[i] << 2) + info.cdef_params.cdef_uv_sec_strength[i]);
#else
            picParam.cdef_y_strengths[i] = (UCHAR)info.cdef_params.cdef_y_strength[i];
            picParam.cdef_uv_strengths[i] = (UCHAR)info.cdef_params.cdef_uv_strength[i];
#endif
        }

        auto& ddiQMFlags = picParam.wQMatrixFlags.fields;
        ddiQMFlags.using_qmatrix = info.quantization_params.using_qmatrix;
        ddiQMFlags.qm_y = info.quantization_params.qm_y;
        ddiQMFlags.qm_u = info.quantization_params.qm_u;;
        ddiQMFlags.qm_v = info.quantization_params.qm_v;;

        picParam.dwModeControlFlags.fields.delta_q_present_flag = info.delta_q_present;
        picParam.dwModeControlFlags.fields.log2_delta_q_res = CeilLog2(info.delta_q_res);
        picParam.dwModeControlFlags.fields.delta_lf_present_flag = info.delta_lf_present;
        picParam.dwModeControlFlags.fields.log2_delta_lf_res = CeilLog2(info.delta_lf_res);
        picParam.dwModeControlFlags.fields.delta_lf_multi = info.delta_lf_multi;
        picParam.dwModeControlFlags.fields.tx_mode = info.TxMode;
        picParam.dwModeControlFlags.fields.reference_mode = info.reference_mode;
        picParam.dwModeControlFlags.fields.reduced_tx_set_used = info.reduced_tx_set;
        picParam.dwModeControlFlags.fields.skip_mode_present = info.skip_mode_present;
        picParam.dwModeControlFlags.fields.ReservedField = 0;

        picParam.LoopRestorationFlags.fields.yframe_restoration_type = info.lr_params.lr_type[0];
        picParam.LoopRestorationFlags.fields.cbframe_restoration_type = info.lr_params.lr_type[1];
        picParam.LoopRestorationFlags.fields.crframe_restoration_type = info.lr_params.lr_type[2];
        picParam.LoopRestorationFlags.fields.lr_unit_shift = info.lr_params.lr_unit_shift;
        picParam.LoopRestorationFlags.fields.lr_uv_shift = info.lr_params.lr_uv_shift;

        for (uint8_t i = 0; i < INTER_REFS; i++)
        {
            picParam.wm[i].wmtype = info.global_motion_params[i + 1].wmtype;
            for (uint8_t j = 0; j < 8; j++)
                picParam.wm[i].wmmat[j] = info.global_motion_params[i + 1].wmmat[j];
        }

        auto& ddiFilmGrain = picParam.stAV1FilmGrainParams;
        auto& ddiFGInfoFlags = ddiFilmGrain.dwFilmGrainInfoFlags.fields;

        if (!frame.FilmGrainDisabled())
        {
            ddiFGInfoFlags.apply_grain = info.film_grain_params.apply_grain;

#if AV1D_DDI_VERSION < 30
            ddiFGInfoFlags.update_grain = info.film_grain_params.update_grain;
            ddiFGInfoFlags.film_grain_params_ref_idx = info.film_grain_params.film_grain_params_ref_idx;
#endif

            ddiFGInfoFlags.chroma_scaling_from_luma = info.film_grain_params.chroma_scaling_from_luma;
            ddiFGInfoFlags.grain_scaling_minus_8 = info.film_grain_params.grain_scaling - 8;
            ddiFGInfoFlags.ar_coeff_lag = info.film_grain_params.ar_coeff_lag;
            ddiFGInfoFlags.ar_coeff_shift_minus_6 = info.film_grain_params.ar_coeff_shift - 6;
            ddiFGInfoFlags.grain_scale_shift = info.film_grain_params.grain_scale_shift;
            ddiFGInfoFlags.overlap_flag = info.film_grain_params.overlap_flag;
            ddiFGInfoFlags.clip_to_restricted_range = info.film_grain_params.clip_to_restricted_range;

            ddiFilmGrain.random_seed = (USHORT)info.film_grain_params.grain_seed;
            ddiFilmGrain.num_y_points = (UCHAR)info.film_grain_params.num_y_points;

            for (uint8_t i = 0; i < MAX_POINTS_IN_SCALING_FUNCTION_LUMA; i++)
            {
                ddiFilmGrain.point_y_value[i] = (UCHAR)info.film_grain_params.point_y_value[i];
                ddiFilmGrain.point_y_scaling[i] = (UCHAR)info.film_grain_params.point_y_scaling[i];
            }

            ddiFilmGrain.num_cb_points = (UCHAR)info.film_grain_params.num_cb_points;
            ddiFilmGrain.num_cr_points = (UCHAR)info.film_grain_params.num_cr_points;

            for (uint8_t i = 0; i < MAX_POINTS_IN_SCALING_FUNCTION_CHROMA; i++)
            {
                ddiFilmGrain.point_cb_value[i] = (UCHAR)info.film_grain_params.point_cb_value[i];
                ddiFilmGrain.point_cb_scaling[i] = (UCHAR)info.film_grain_params.point_cb_scaling[i];
                ddiFilmGrain.point_cr_value[i] = (UCHAR)info.film_grain_params.point_cr_value[i];
                ddiFilmGrain.point_cr_scaling[i] = (UCHAR)info.film_grain_params.point_cr_scaling[i];
            }

            for (uint8_t i = 0; i < MAX_AUTOREG_COEFFS_LUMA; i++)
                ddiFilmGrain.ar_coeffs_y[i] = (CHAR)info.film_grain_params.ar_coeffs_y[i];

            for (uint8_t i = 0; i < MAX_AUTOREG_COEFFS_CHROMA; i++)
            {
                ddiFilmGrain.ar_coeffs_cb[i] = (CHAR)info.film_grain_params.ar_coeffs_cb[i];
                ddiFilmGrain.ar_coeffs_cr[i] = (CHAR)info.film_grain_params.ar_coeffs_cr[i];
            }

            ddiFilmGrain.cb_mult = (UCHAR)info.film_grain_params.cb_mult;
            ddiFilmGrain.cb_luma_mult = (UCHAR)info.film_grain_params.cb_luma_mult;
            ddiFilmGrain.cb_offset = (USHORT)info.film_grain_params.cb_offset;
            ddiFilmGrain.cr_mult = (UCHAR)info.film_grain_params.cr_mult;
            ddiFilmGrain.cr_luma_mult = (UCHAR)info.film_grain_params.cr_luma_mult;
            ddiFilmGrain.cr_offset = (USHORT)info.film_grain_params.cr_offset;
        }

#if AV1D_DDI_VERSION < 26
        if (info.tile_info.uniform_tile_spacing_flag)
        {
            picParam.log2_tile_rows = (UCHAR)info.tile_info.TileRowsLog2;
            picParam.log2_tile_cols = (UCHAR)info.tile_info.TileColsLog2;
        }
        else
        {
#endif
            picParam.tile_cols = (USHORT)info.tile_info.TileCols;
            picParam.tile_rows = (USHORT)info.tile_info.TileRows;

            for (uint32_t i = 0; i < picParam.tile_cols; i++)
            {
                picParam.width_in_sbs_minus_1[i] =
                    (USHORT)(info.tile_info.SbColStarts[i + 1] - info.tile_info.SbColStarts[i] - 1);
            }

            for (int i = 0; i < picParam.tile_rows; i++)
            {
                picParam.height_in_sbs_minus_1[i] =
                    (USHORT)(info.tile_info.SbRowStarts[i + 1] - info.tile_info.SbRowStarts[i] - 1);
            }
#if AV1D_DDI_VERSION < 26
        }
#endif

#if UMC_AV1_DECODER_REV >= 8500
        picParam.context_update_tile_id = (USHORT)info.tile_info.context_update_tile_id;
#endif
    }

    void PackerIntel::PackTileControlParams(DXVA_Intel_Tile_AV1& tileControlParam, TileLocation const& loc)
    {
        tileControlParam.BSTileDataLocation = (UINT)loc.offset;
        tileControlParam.BSTileBytesInBuffer = (UINT)loc.size;
        tileControlParam.wBadBSBufferChopping = 0;
        tileControlParam.tile_row = (USHORT)loc.row;
        tileControlParam.tile_column = (USHORT)loc.col;
#if AV1D_DDI_VERSION >= 26
        tileControlParam.TileIndex = 0; // valid for large_scale_tile only
#endif
        tileControlParam.StartTileIdx = (USHORT)loc.startIdx;
        tileControlParam.EndTileIdx = (USHORT)loc.endIdx;
#if AV1D_DDI_VERSION >= 31
        tileControlParam.anchor_frame_idx.bPicEntry = 0;
#else
        tileControlParam.anchor_frame_idx.wPicEntry = 0;
#endif
        tileControlParam.BSTilePayloadSizeInBytes = (UINT)loc.size;
    }

#endif // UMC_VA_DXVA

#ifdef UMC_VA_LINUX
/****************************************************************************************************/
// Linux VAAPI packer implementation
/****************************************************************************************************/

    PackerVA::PackerVA(UMC::VideoAccelerator * va)
        : Packer(va)
    {}

    UMC::Status PackerVA::GetStatusReport(void * pStatusReport, size_t size)
    {
        return UMC_OK;
    }

    void PackerVA::BeginFrame() {}
    void PackerVA::EndFrame() {}

    void PackerVA::PackAU(std::vector<TileSet>& tileSets, AV1DecoderFrame const& info, bool firstSubmission)
    {
        if (firstSubmission)
        {
            // it's first submission for current frame - need to fill and submit picture parameters
            UMC::UMCVACompBuffer* compBufPic = nullptr;
            VADecPictureParameterBufferAV1 *picParam =
                (VADecPictureParameterBufferAV1*)m_va->GetCompBuffer(VAPictureParameterBufferType, &compBufPic, sizeof(VADecPictureParameterBufferAV1));

            if (!picParam || !compBufPic || (compBufPic->GetBufferSize() < static_cast<int32_t>(sizeof(VADecPictureParameterBufferAV1))))
                throw av1_exception(MFX_ERR_MEMORY_ALLOC);

            compBufPic->SetDataSize(sizeof(VADecPictureParameterBufferAV1));
            *picParam = VADecPictureParameterBufferAV1{};
            PackPicParams(*picParam, info);
        }

        UMC::UMCVACompBuffer* compBufBs = nullptr;
        uint8_t* const bitstreamData = (uint8_t *)m_va->GetCompBuffer(VASliceDataBufferType, &compBufBs, CalcSizeOfTileSets(tileSets));
        if (!bitstreamData || !compBufBs)
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::vector<VABitStreamParameterBufferAV1> tileControlParams;

        for (auto& tileSet : tileSets)
        {
            const size_t offsetInBuffer = compBufBs->GetDataSize();
            const size_t spaceInBuffer = compBufBs->GetBufferSize() - offsetInBuffer;
            TileLayout layout;
            const size_t bytesSubmitted = tileSet.Submit(bitstreamData, spaceInBuffer, offsetInBuffer, layout);

            if (bytesSubmitted)
            {
                compBufBs->SetDataSize(static_cast<uint32_t>(offsetInBuffer + bytesSubmitted));

                for (auto& loc : layout)
                {
                    tileControlParams.emplace_back();
                    PackTileControlParams(tileControlParams.back(), loc);
                }
            }
        }

        UMCVACompBuffer* compBufTile = nullptr;
        const int32_t tileControlInfoSize = static_cast<int32_t>(sizeof(VABitStreamParameterBufferAV1) * tileControlParams.size());
        VABitStreamParameterBufferAV1 *tileControlParam = (VABitStreamParameterBufferAV1*)m_va->GetCompBuffer(VASliceParameterBufferType, &compBufTile, tileControlInfoSize);
        if (!tileControlParam || !compBufTile || (compBufTile->GetBufferSize() < tileControlInfoSize))
            throw av1_exception(MFX_ERR_MEMORY_ALLOC);

        std::copy(tileControlParams.begin(), tileControlParams.end(), tileControlParam);
        compBufTile->SetDataSize(tileControlInfoSize);
    }

    void PackerVA::PackPicParams(VADecPictureParameterBufferAV1& picParam, AV1DecoderFrame const& frame)
    {
        SequenceHeader const& sh = frame.GetSeqHeader();

        picParam.frame_width_minus1 = (uint16_t)frame.GetUpscaledWidth() - 1;
        picParam.frame_height_minus1 = (uint16_t)frame.GetHeight() - 1;

        FrameHeader const& info =
            frame.GetFrameHeader();

        // fill seq parameters
        picParam.profile = (uint8_t)sh.seq_profile;

        auto& seqInfo = picParam.seq_info_fields.fields;

        seqInfo.still_picture = 0;
        seqInfo.sb_size_128x128 = (sh.sbSize == BLOCK_128X128) ? 1 : 0;
        seqInfo.enable_filter_intra = info.enable_filter_intra;
        seqInfo.enable_intra_edge_filter = info.enable_intra_edge_filter;
        seqInfo.enable_interintra_compound = info.enable_interintra_compound;
        seqInfo.enable_masked_compound = info.enable_masked_compound;
        seqInfo.enable_dual_filter = sh.enable_dual_filter;
        seqInfo.enable_order_hint = sh.enable_order_hint;
        seqInfo.enable_jnt_comp = sh.enable_jnt_comp;
        seqInfo.enable_cdef = 1;
        seqInfo.enable_restoration = 1;
        seqInfo.mono_chrome = sh.color_config.mono_chrome;
        seqInfo.color_range = sh.color_config.color_range;
        seqInfo.subsampling_x = sh.color_config.subsampling_x;
        seqInfo.subsampling_y = sh.color_config.subsampling_y;
        seqInfo.chroma_sample_position = sh.color_config.chroma_sample_position;
        seqInfo.film_grain_params_present = sh.film_grain_param_present;

        picParam.bit_depth_idx = (sh.color_config.BitDepth == 10) ? 1 :
            (sh.color_config.BitDepth == 12) ? 2 : 0;
        picParam.order_hint_bits_minus_1 = sh.order_hint_bits_minus1;

        // fill pic params
        auto& picInfo = picParam.pic_info_fields.bits;

        picInfo.frame_type = info.frame_type;
        picInfo.show_frame = info.show_frame;
        picInfo.showable_frame = info.showable_frame;
        picInfo.error_resilient_mode = info.error_resilient_mode;
        picInfo.disable_cdf_update = info.disable_cdf_update;
        picInfo.allow_screen_content_tools = info.allow_screen_content_tools;
        picInfo.force_integer_mv = info.force_integer_mv;
        picInfo.allow_intrabc = info.allow_intrabc;
        picInfo.use_superres = (info.SuperresDenom == SCALE_NUMERATOR) ? 0 : 1;
        picInfo.allow_high_precision_mv = info.allow_high_precision_mv;
        picInfo.is_motion_mode_switchable = info.is_motion_mode_switchable;
        picInfo.use_ref_frame_mvs = info.use_ref_frame_mvs;
        picInfo.disable_frame_end_update_cdf = info.disable_frame_end_update_cdf;
        picInfo.uniform_tile_spacing_flag = info.tile_info.uniform_tile_spacing_flag;
        picInfo.allow_warped_motion = info.allow_warped_motion;
        picInfo.refresh_frame_context = info.disable_frame_end_update_cdf ? REFRESH_FRAME_CONTEXT_DISABLED : REFRESH_FRAME_CONTEXT_BACKWARD;;
        picInfo.large_scale_tile = info.large_scale_tile;

        picParam.order_hint = (uint8_t)info.order_hint;
        picParam.superres_scale_denominator = (uint8_t)info.SuperresDenom;

        picParam.interp_filter = (uint8_t)info.interpolation_filter;

        // fill segmentation params and map
        auto& seg = picParam.seg_info;
        seg.segment_info_fields.bits.enabled = info.segmentation_params.segmentation_enabled;;
        seg.segment_info_fields.bits.temporal_update = info.segmentation_params.segmentation_temporal_update;
        seg.segment_info_fields.bits.update_map = info.segmentation_params.segmentation_update_map;
        seg.segment_info_fields.bits.update_data = info.segmentation_params.segmentation_update_data;

        // set current and reference frames
        picParam.current_frame = (VASurfaceID)m_va->GetSurfaceID(frame.GetMemID());
        picParam.current_display_picture = (VASurfaceID)m_va->GetSurfaceID(frame.GetMemID());

        if (seg.segment_info_fields.bits.enabled)
        {
            for (uint8_t i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; i++)
            {
                seg.feature_mask[i] = (uint8_t)info.segmentation_params.FeatureMask[i];
                for (uint8_t j = 0; j < SEG_LVL_MAX; j++)
                    seg.feature_data[i][j] = (int16_t)info.segmentation_params.FeatureData[i][j];
            }
        }

        if (KEY_FRAME == info.frame_type)
        {
            for (int i = 0; i < NUM_REF_FRAMES; i++)
            {
                picParam.ref_frame_map[i] = VA_INVALID_SURFACE;
            }
        }
        else
        {
            for (uint8_t ref = 0; ref < NUM_REF_FRAMES; ++ref)
                picParam.ref_frame_map[ref] = (VASurfaceID)m_va->GetSurfaceID(frame.frame_dpb[ref]->GetMemID());

            for (uint8_t ref_idx = 0; ref_idx < INTER_REFS; ref_idx++)
            {
                const uint8_t idxInDPB = (uint8_t)info.ref_frame_idx[ref_idx];
                picParam.ref_order_hint[ref_idx] = frame.frame_dpb[idxInDPB]->GetFrameHeader().order_hint;
                picParam.ref_frame_idx[ref_idx] = idxInDPB;
            }
        }

        picParam.primary_ref_frame = (uint8_t)info.primary_ref_frame;

        // fill loop filter params
        picParam.filter_level[0] = (uint8_t)info.loop_filter_params.loop_filter_level[0];
        picParam.filter_level[1] = (uint8_t)info.loop_filter_params.loop_filter_level[1];
        picParam.filter_level_u = (uint8_t)info.loop_filter_params.loop_filter_level[2];
        picParam.filter_level_v = (uint8_t)info.loop_filter_params.loop_filter_level[3];

        auto& lfInfo = picParam.loop_filter_info_fields.bits;
        lfInfo.sharpness_level = (uint8_t)info.loop_filter_params.loop_filter_sharpness;
        lfInfo.mode_ref_delta_enabled = info.loop_filter_params.loop_filter_delta_enabled;
        lfInfo.mode_ref_delta_update = info.loop_filter_params.loop_filter_delta_update;


        for (uint8_t i = 0; i < TOTAL_REFS; i++)
        {
            picParam.ref_deltas[i] = info.loop_filter_params.loop_filter_ref_deltas[i];
        }
        for (uint8_t i = 0; i < UMC_VP9_DECODER::MAX_MODE_LF_DELTAS; i++)
        {
            picParam.mode_deltas[i] = info.loop_filter_params.loop_filter_mode_deltas[i];
        }

        // fill quantization params
        picParam.base_qindex = (int16_t)info.quantization_params.base_q_idx;
        picParam.y_dc_delta_q = (int8_t)info.quantization_params.DeltaQYDc;
        picParam.u_dc_delta_q = (int8_t)info.quantization_params.DeltaQUDc;
        picParam.v_dc_delta_q = (int8_t)info.quantization_params.DeltaQVDc;
        picParam.u_ac_delta_q = (int8_t)info.quantization_params.DeltaQUAc;
        picParam.v_ac_delta_q = (int8_t)info.quantization_params.DeltaQVAc;

        // fill CDEF
        picParam.cdef_damping_minus_3 = (uint8_t)(info.cdef_params.cdef_damping - 3);
        picParam.cdef_bits = (uint8_t)info.cdef_params.cdef_bits;

        for (uint8_t i = 0; i < CDEF_MAX_STRENGTHS; i++)
        {
#if UMC_AV1_DECODER_REV >= 8500
            picParam.cdef_y_strengths[i] = (uint8_t)((info.cdef_params.cdef_y_pri_strength[i] << 2) + info.cdef_params.cdef_y_sec_strength[i]);
            picParam.cdef_uv_strengths[i] = (uint8_t)((info.cdef_params.cdef_uv_pri_strength[i] << 2) + info.cdef_params.cdef_uv_sec_strength[i]);
#else
            picParam.cdef_y_strengths[i] = (uint8_t)info.cdef_params.cdef_y_strength[i];
            picParam.cdef_uv_strengths[i] = (uint8_t)info.cdef_params.cdef_uv_strength[i];
#endif
        }

        // fill quantization matrix params
        auto& qmFlags = picParam.qmatrix_fields.bits;
        qmFlags.using_qmatrix = info.quantization_params.using_qmatrix;
        qmFlags.qm_y = info.quantization_params.qm_y;
        qmFlags.qm_u = info.quantization_params.qm_u;
        qmFlags.qm_v = info.quantization_params.qm_v;


        auto& modeCtrl = picParam.mode_control_fields.bits;
        modeCtrl.delta_q_present_flag = info.delta_q_present;
        modeCtrl.log2_delta_q_res = CeilLog2(info.delta_q_res);
        modeCtrl.delta_lf_present_flag = info.delta_lf_present;
        modeCtrl.log2_delta_lf_res = CeilLog2(info.delta_lf_res);
        modeCtrl.delta_lf_multi = info.delta_lf_multi;
        modeCtrl.tx_mode = info.TxMode;
        modeCtrl.reference_mode = info.reference_mode;
        modeCtrl.reduced_tx_set_used = info.reduced_tx_set;
        modeCtrl.skip_mode_present = info.skip_mode_present;

        // fill loop restoration params
        auto& lrInfo = picParam.loop_restoration_fields.bits;
        lrInfo.yframe_restoration_type = info.lr_params.lr_type[0];
        lrInfo.cbframe_restoration_type = info.lr_params.lr_type[1];
        lrInfo.crframe_restoration_type = info.lr_params.lr_type[2];;
        lrInfo.lr_unit_shift = info.lr_params.lr_unit_shift;
        lrInfo.lr_uv_shift = info.lr_params.lr_uv_shift;

        // fill global motion params
        for (uint8_t i = 0; i < INTER_REFS; i++)
        {
            picParam.wm[i].wmtype = static_cast<VATransformationType>(info.global_motion_params[i + 1].wmtype);
            for (uint8_t j = 0; j < 8; j++)
            {
                picParam.wm[i].wmmat[j] = info.global_motion_params[i + 1].wmmat[j];
                // TODO: [Rev0.5] implement processing of alpha, beta, gamma, delta.
            }
        }

        // fill tile params
        picParam.tile_cols = (uint16_t)info.tile_info.TileCols;
        picParam.tile_rows = (uint16_t)info.tile_info.TileRows;

        if (!info.tile_info.uniform_tile_spacing_flag)
        {
            for (uint32_t i = 0; i < picParam.tile_cols; i++)
            {
                picParam.width_in_sbs_minus_1[i] =
                    (uint16_t)(info.tile_info.SbColStarts[i + 1] - info.tile_info.SbColStarts[i] - 1);
            }

            for (int i = 0; i < picParam.tile_rows; i++)
            {
                picParam.height_in_sbs_minus_1[i] =
                    (uint16_t)(info.tile_info.SbRowStarts[i + 1] - info.tile_info.SbRowStarts[i] - 1);
            }
        }

#if UMC_AV1_DECODER_REV >= 8500
        picParam.context_update_tile_id = (uint16_t)info.tile_info.context_update_tile_id;
#endif
    }

    void PackerVA::PackTileControlParams(VABitStreamParameterBufferAV1& tileControlParam, TileLocation const& loc)
    {
        tileControlParam.bit_stream_data_offset = (uint32_t)loc.offset;
        tileControlParam.bit_stream_data_size = (uint32_t)loc.size;
        tileControlParam.bit_stream_data_flag = 0;
        tileControlParam.tile_row = (uint16_t)loc.row;
        tileControlParam.tile_column = (uint16_t)loc.col;
        tileControlParam.start_tile_idx = (uint16_t)loc.startIdx;
        tileControlParam.end_tile_idx = (uint16_t)loc.endIdx;
        tileControlParam.anchor_frame_idx = 0;
    }

#endif // UMC_VA_LINUX

} // namespace UMC_AV1_DECODER

#endif // UMC_ENABLE_AV1_VIDEO_DECODER
