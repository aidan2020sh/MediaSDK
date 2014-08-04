/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2014 Intel Corporation. All Rights Reserved.
*/

#ifndef __UMC_HEVC_DDI_H
#define __UMC_HEVC_DDI_H

//#define version_0_89

#pragma warning(disable: 4201)

#pragma pack(1)

typedef struct _DXVA_Intel_PicEntry_HEVC
{
    union
    {
        struct
        {
            UCHAR   Index7bits          : 7;
            UCHAR   long_term_ref_flag  : 1;
        };

        UCHAR   bPicEntry;
    };

} DXVA_Intel_PicEntry_HEVC, *LPDXVA_Intel_PicEntry_HEVC;

typedef struct _DXVA_Intel_Status_HEVC {
  USHORT                    StatusReportFeedbackNumber;
  DXVA_PicEntry_HEVC        current_picture; /* flag is bot field flag */
  UCHAR                     bBufType;
  UCHAR                     bStatus;
  UCHAR                     bReserved8Bits;
  USHORT                    wNumMbsAffected;
} DXVA_Intel_Status_HEVC, *LPDXVA_Intel_Status_HEVC;

typedef struct _DXVA_Intel_PicParams_HEVC
{
    USHORT  PicWidthInMinCbsY;
    USHORT  PicHeightInMinCbsY;

    union
    {
        struct
        {

            USHORT  chroma_format_idc                   : 2;
            USHORT  separate_colour_plane_flag          : 1;
            USHORT  bit_depth_luma_minus8               : 3;     // [0]
            USHORT  bit_depth_chroma_minus8             : 3;     // [0]
            USHORT  log2_max_pic_order_cnt_lsb_minus4   : 4;     // [0..12]
            USHORT  NoPicReorderingFlag                 : 1;
            USHORT  NoBiPredFlag                        : 1;
            USHORT  ReservedBits1                       : 1;

        } fields;

        USHORT  wFormatAndSequenceInfoFlags;

    } PicFlags;

    DXVA_Intel_PicEntry_HEVC    CurrPic; 

    UCHAR   sps_max_dec_pic_buffering_minus1;        // [0..15]
    UCHAR   log2_min_luma_coding_block_size_minus3;     //[0..3]
    UCHAR   log2_diff_max_min_luma_coding_block_size;    //[0..3]
    UCHAR   log2_min_transform_block_size_minus2;        // [0..3]
    UCHAR   log2_diff_max_min_transform_block_size;        // [0..3]
    UCHAR   max_transform_hierarchy_depth_inter;    // [0..4]
    UCHAR   max_transform_hierarchy_depth_intra;    // [0..4]
    UCHAR   num_short_term_ref_pic_sets;
    UCHAR   num_long_term_ref_pics_sps;
    UCHAR   num_ref_idx_l0_default_active_minus1;
    UCHAR   num_ref_idx_l1_default_active_minus1;
    CHAR    init_qp_minus26;                // [-26..25]
    UCHAR   ucNumDeltaPocsOfRefRpsIdx;        // [0]
    USHORT  wNumBitsForShortTermRPSInSlice;
    USHORT  TotalNumEntryPointOffsets;

    union
    {
        struct
        {
            UINT32  scaling_list_enabled_flag                       : 1;
            UINT32  amp_enabled_flag                                : 1;
            UINT32  sample_adaptive_offset_enabled_flag             : 1;
            UINT32  pcm_enabled_flag                                : 1;
            UINT32  pcm_sample_bit_depth_luma_minus1                : 4;    // [0..7]
            UINT32  pcm_sample_bit_depth_chroma_minus1              : 4;    // [0..7]
            UINT32  log2_min_pcm_luma_coding_block_size_minus3      : 2;// [0..2]
            UINT32  log2_diff_max_min_pcm_luma_coding_block_size    : 2;// [0..2]
            UINT32  pcm_loop_filter_disabled_flag                   : 1;
            UINT32  long_term_ref_pics_present_flag                 : 1;
            UINT32  sps_temporal_mvp_enabled_flag                   : 1;
            UINT32  strong_intra_smoothing_enabled_flag             : 1;
            UINT32  dependent_slice_segments_enabled_flag           : 1;
            UINT32  output_flag_present_flag                        : 1;
            UINT32  num_extra_slice_header_bits                     : 3;    // [0..7]
            UINT32  sign_data_hiding_flag                           : 1;
            UINT32  cabac_init_present_flag                         : 1;
            UINT32  ReservedBits3                                   : 5;
        } fields;

        UINT32  dwCodingParamToolFlags;
    };

    union
    {
        struct
        {
            UINT32  constrained_intra_pred_flag                 : 1;
            UINT32  transform_skip_enabled_flag                 : 1;
            UINT32  cu_qp_delta_enabled_flag                    : 1;
            UINT32  pps_slice_chroma_qp_offsets_present_flag    : 1;
            UINT32  weighted_pred_flag                          : 1;
            UINT32  weighted_bipred_flag                        : 1;
            UINT32  transquant_bypass_enabled_flag              : 1;
            UINT32  tiles_enabled_flag                          : 1;
            UINT32  entropy_coding_sync_enabled_flag            : 1;
            UINT32  uniform_spacing_flag                        : 1;
            UINT32  loop_filter_across_tiles_enabled_flag       : 1;
            UINT32  pps_loop_filter_across_slices_enabled_flag  : 1;
            UINT32  deblocking_filter_override_enabled_flag     : 1;
            UINT32  pps_deblocking_filter_disabled_flag         : 1;
            UINT32  lists_modification_present_flag             : 1;
            UINT32  slice_segment_header_extension_present_flag : 1;
            UINT32  IrapPicFlag                                 : 1;
            UINT32  IdrPicFlag                                  : 1;
            UINT32  IntraPicFlag                                : 1;
            UINT32  ReservedBits4                               : 13;
        } fields;

        UINT    dwCodingSettingPicturePropertyFlags;

    } PicShortFormatFlags;

    CHAR    pps_cb_qp_offset;
    CHAR    pps_cr_qp_offset;
    UCHAR   num_tile_columns_minus1;
    UCHAR   num_tile_rows_minus1;
    USHORT  column_width_minus1[19];
    USHORT  row_height_minus1[21];
    UCHAR   diff_cu_qp_delta_depth;            // [0..3]
    CHAR    pps_beta_offset_div2;
    CHAR    pps_tc_offset_div2;
    UCHAR   log2_parallel_merge_level_minus2;
    INT32   CurrPicOrderCntVal;

    DXVA_Intel_PicEntry_HEVC    RefFrameList[15];

    UCHAR   ReservedBits5;
    INT32   PicOrderCntValList[15];
    UCHAR   RefPicSetStCurrBefore[8];
    UCHAR   RefPicSetStCurrAfter[8];
    UCHAR   RefPicSetLtCurr[8];
    USHORT  RefFieldPicFlag;
    USHORT  RefBottomFieldFlag;
    UINT32  StatusReportFeedbackNumber;

} DXVA_Intel_PicParams_HEVC, *LPDXVA_Intel_PicParams_HEVC;


/****************************************************************************/


typedef struct _DXVA_Intel_Slice_HEVC_Long
{
    UINT    BSNALunitDataLocation;
    UINT    SliceBytesInBuffer;
    USHORT  wBadSliceChopping;
    USHORT  ReservedBits;
    UINT    ByteOffsetToSliceData;
    UINT    slice_segment_address;

    DXVA_Intel_PicEntry_HEVC    RefPicList[2][15];

    union
    {
        UINT    value;

        struct
        {
            UINT    LastSliceOfPic                                  : 1;
            UINT    dependent_slice_segment_flag                    : 1;
            UINT    slice_type                                      : 2;
            UINT    color_plane_id                                  : 2;
            UINT    slice_sao_luma_flag                             : 1;
            UINT    slice_sao_chroma_flag                           : 1;
            UINT    mvd_l1_zero_flag                                : 1;
            UINT    cabac_init_flag                                 : 1;
            UINT    slice_temporal_mvp_enabled_flag                 : 1;
            UINT    slice_deblocking_filter_disabled_flag           : 1;
            UINT    collocated_from_l0_flag                         : 1;
            UINT    slice_loop_filter_across_slices_enabled_flag    : 1;
            UINT    reserved                                        : 18;
        }
        fields;
    } LongSliceFlags;

    UCHAR   collocated_ref_idx;
    UCHAR   num_ref_idx_l0_active_minus1;
    UCHAR   num_ref_idx_l1_active_minus1;
    CHAR    slice_qp_delta;
    CHAR    slice_cb_qp_offset;
    CHAR    slice_cr_qp_offset;
    CHAR    slice_beta_offset_div2;                // [-6..6]
    CHAR    slice_tc_offset_div2;                    // [-6..6]
    UCHAR   luma_log2_weight_denom;
    UCHAR   delta_chroma_log2_weight_denom;
    CHAR    delta_luma_weight_l0[15];
    CHAR    luma_offset_l0[15];
    CHAR    delta_chroma_weight_l0[15][2];
    CHAR    ChromaOffsetL0[15][2];
    CHAR    delta_luma_weight_l1[15];
    CHAR    luma_offset_l1[15];
    CHAR    delta_chroma_weight_l1[15][2];
    CHAR    ChromaOffsetL1[15][2];
    UCHAR   five_minus_max_num_merge_cand;

#ifdef version_0_89
    USHORT  num_entry_point_offsets;               // [0..540]
    USHORT  EntryOffsetToSubsetArray;              // [0..540]
#endif
} DXVA_Intel_Slice_HEVC_Long, *LPDXVA_Intel_Slice_HEVC_Long;

typedef struct _DXVA_Intel_Slice_HEVC_Short
{
    UINT    BSNALunitDataLocation;
    UINT    SliceBytesInBuffer;
    USHORT  wBadSliceChopping;
} DXVA_Intel_Slice_HEVC_Short, *LPDXVA_Intel_Slice_HEVC_Short;


typedef struct _DXVA_Intel_Qmatrix_HEVC
{
    UCHAR   ucScalingLists0[6][16]; // 2 inter/intra 3: YUV
    UCHAR   ucScalingLists1[6][64];
    UCHAR   ucScalingLists2[6][64];
    UCHAR   ucScalingLists3[2][64];
    UCHAR   ucScalingListDCCoefSizeID2[6];
    UCHAR   ucScalingListDCCoefSizeID3[2];
} DXVA_Intel_Qmatrix_HEVC, *LPDXVA_Intel_Qmatrix_HEVC;

#pragma pack()

#endif __UMC_HEVC_DDI_H
