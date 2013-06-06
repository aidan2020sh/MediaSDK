/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_VA_PACKER_H
#define __UMC_H265_VA_PACKER_H

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_mvc_ddi.h"
#endif

#ifndef UMC_RESTRICTED_CODE_VA

namespace UMC_HEVC_DECODER
{

struct H265PicParamSet;
class H265DecoderFrame;
class H265Slice;
struct H265SliceHeader;
class H265DecoderLocalMacroblockDescriptor;
class H265DecoderFrameInfo;
struct ReferenceFlags_H265;
class TaskSupplier_H265;

#ifdef UMC_VA_DXVA

#define MK_HEVCVER(j, n)    (((j & 0x0000ffff) << 16) | (n & 0x0000ffff))
#define HEVC_SPEC_VER       MK_HEVCVER(0, 82)

typedef struct _DXVA_PicEntry_HEVC
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

} DXVA_PicEntry_HEVC, *LPDXVA_PicEntry_HEVC;

typedef struct _DXVA_Status_HEVC 
{
    USHORT StatusReportFeedbackNumber; 
    DXVA_PicEntry_HEVC current_picture; 
    UCHAR  bBufType; 
    UCHAR  bStatus; 
    UCHAR  bReserved8Bits; 
    USHORT wNumMbsAffected; 
} DXVA_Status_HEVC, *LPDXVA_Status_HEVC;


#if HEVC_SPEC_VER == MK_HEVCVER(0, 81)
typedef struct _DXVA_PicParams_HEVC
{
    USHORT          wFrameWidthInMinCbMinus1;
    USHORT          wFrameHeightInMinCbMinus1;
    union
    {
        UINT        dwPicFlags;
        struct
        {
            UINT        chroma_format_idc                   : 2;
            UINT        separate_colour_plane_flag          : 1;
            UINT        pcm_enabled_flag                    : 1;
            UINT        scaling_list_enable_flag            : 1;
            UINT        transform_skip_enabled_flag         : 1;
            UINT        amp_enabled_flag                    : 1;
            UINT        strong_intra_smoothing_enable_flag  : 1;
            UINT        sign_data_hiding_flag               : 1;
            UINT        constrained_intra_pred_flag         : 1;
            UINT        cu_qp_delta_enabled_flag            : 1;
            UINT        weighted_pred_flag                  : 1;
            UINT        weighted_bipred_flag                : 1;
            UINT        transquant_bypass_enable_flag       : 1;
            UINT        tiles_enabled_flag                  : 1;
            UINT        entropy_coding_sync_enabled_flag    : 1;
            UINT        loop_filter_across_slices_flag      : 1;    //  loop filter flags
            UINT        loop_filter_across_tiles_flag       : 1;
            UINT        pcm_loop_filter_disable_flag        : 1;
            UINT        field_pic_flag                      : 1;
            UINT        bottom_field_flag                   : 1;
            UINT        tiles_fixed_structure_flag          : 1;
            UINT        reserved10bits                      : 10;
        };
    };

    DXVA_PicEntry_HEVC  CurrPic; 
    INT                 poc_curr_pic;
    UCHAR               num_ref_frames;
    DXVA_PicEntry_HEVC  RefFrameList[16];
    INT                 poc_list[16];
    USHORT              ref_field_pic_flag;
    USHORT              ref_bottom_field_flag;
    UCHAR               bit_depth_luma_minus8;
    UCHAR               bit_depth_chroma_minus8;
    UCHAR               pcm_bit_depth_luma;
    UCHAR               pcm_bit_depth_chroma;
    UCHAR               log2_min_coding_block_size;
    UCHAR               log2_max_coding_block_size;
    UCHAR               log2_min_transform_block_size;
    UCHAR               log2_max_transform_block_size;
    UCHAR               log2_min_PCM_cb_size;
    UCHAR               log2_max_PCM_cb_size;
    UCHAR               max_transform_hierarchy_depth_intra;
    UCHAR               max_transform_hierarchy_depth_inter;
    CHAR                pic_init_qp_minus26;
    UCHAR               diff_cu_qp_delta_depth;
    UCHAR               pps_cb_qp_offset;
    UCHAR               pps_cr_qp_offset;
    UCHAR               num_tile_columns_minus1;
    UCHAR               num_tile_rows_minus1;
    USHORT              tile_column_width[20];
    USHORT              tile_row_height[22];
    UCHAR               log2_parallel_merge_level_minus2;
    UINT                StatusReportFeedbackNumber;

    UCHAR               ContinuationFlag;

    union
    {
        UINT        dwPicShortFormatFlags;
        struct
        {
            UINT    lists_modification_present_flag         : 1;
            UINT    long_term_ref_pics_present_flag         : 1;
            UINT    sps_temporal_mvp_enable_flag            : 1;
            UINT    cabac_init_present_flag                 : 1;
            UINT    output_flag_present_flag                : 1;
            UINT    dependent_slice_segments_enabled_flag   : 1;
            UINT    slice_chroma_qp_offsets_present_flag    : 1;
            UINT    slice_temporal_mvp_enable_flag          : 1;
            UINT    SAO_enabled_flag                        : 1;
            UINT    deblocking_filter_control_present_flag  : 1;
            UINT    deblocking_filter_override_enabled_flag : 1;
            UINT    pps_disable_deblocking_filter_flag      : 1;
            UINT    reservedbits                            : 20;
        };
    };

    DXVA_PicEntry_HEVC  CollocatedRefIdx;
    UCHAR               log2_max_pic_order_cnt_lsb_minus4;
    UCHAR               num_short_term_ref_pic_sets;
    UCHAR               num_long_term_ref_pic_sps;
    USHORT              lt_ref_pic_poc_lsb_sps[32];
    UINT                used_by_curr_pic_lt_sps_flags;
    UCHAR               num_ref_idx_l0_default_active_minus1;
    UCHAR               num_ref_idx_l1_default_active_minus1;
    CHAR                beta_offset_div2;
    CHAR                tc_offset_div2;
} DXVA_PicParams_HEVC, *LPDXVA_PicParams_HEVC;
#elif HEVC_SPEC_VER == MK_HEVCVER(0, 82)
typedef struct _DXVA_PicParams_HEVC
{
    USHORT  PicWidthInMinCbsY;
    USHORT  PicHeightInMinCbsY;
    union
    {
        UINT        value;
        struct
        {
            UINT    chroma_format_idc                           : 2;
            UINT    separate_colour_plane_flag                  : 1;
            UINT    pcm_enabled_flag                            : 1;
            UINT    scaling_list_enabled_flag                   : 1;
            UINT    transform_skip_enabled_flag                 : 1;
            UINT    amp_enabled_flag                            : 1;
            UINT    strong_intra_smoothing_enabled_flag         : 1;
            UINT    sign_data_hiding_flag                       : 1;
            UINT    constrained_intra_pred_flag                 : 1;
            UINT    cu_qp_delta_enabled_flag                    : 1;
            UINT    weighted_pred_flag                          : 1;
            UINT    weighted_bipred_flag                        : 1;
            UINT    transquant_bypass_enabled_flag              : 1;
            UINT    tiles_enabled_flag                          : 1;
            UINT    entropy_coding_sync_enabled_flag            : 1;
            UINT    pps_loop_filter_across_slices_enabled_flag  : 1;    //  loop filter flags
            UINT    loop_filter_across_tiles_enabled_flag       : 1;
            UINT    pcm_loop_filter_disabled_flag               : 1;
            UINT    field_pic_flag                              : 1;
            UINT    bottom_field_flag                           : 1;
            UINT    NoPicReorderingFlag                         : 1;
            UINT    NoBiPredFlag                                : 1;
            UINT    reserved_bits                               : 9;
        } fields;
    } PicFlags;

    DXVA_PicEntry_HEVC    CurrPic; 
    INT             CurrPicOrderCntVal;
    UCHAR           sps_max_dec_pic_buffering_minus1;    // [0..15]
    DXVA_PicEntry_HEVC    RefFrameList[16];
    INT             PicOrderCntValList[16];
    USHORT          RefFieldPicFlag;
    USHORT          RefBottomFieldFlag;
    UCHAR           bit_depth_luma_minus8;            // [0..6]
    UCHAR           bit_depth_chroma_minus8;            // [0..6]
    UCHAR           pcm_sample_bit_depth_luma_minus1;    // [0..13]
    UCHAR           pcm_sample_bit_depth_chroma_minus1;    // [0..13]
    UCHAR           log2_min_luma_coding_block_size_minus3; //[0..3]
    UCHAR           log2_diff_max_min_luma_coding_block_size;//[0..3]
    UCHAR           log2_min_transform_block_size_minus2;    // [0..3]
    UCHAR           log2_diff_max_min_transform_block_size;// [0..3]
    UCHAR           log2_min_pcm_luma_coding_block_size_minus3; // [0..2]
    UCHAR           log2_diff_max_min_pcm_luma_coding_block_size;        // [0..2]
    UCHAR           max_transform_hierarchy_depth_intra;    // [0..4]
    UCHAR           max_transform_hierarchy_depth_inter;    // [0..4]
    CHAR            init_qp_minus26;            // [-26..25]
    UCHAR           diff_cu_qp_delta_depth;            // [0..3]
    CHAR            pps_cb_qp_offset;
    CHAR            pps_cr_qp_offset;
    UCHAR           num_tile_columns_minus1;
    UCHAR           num_tile_rows_minus1;
    USHORT          column_width_minus1[20];
    USHORT          row_height_minus1[22];
    UCHAR           log2_parallel_merge_level_minus2;
    UINT            StatusReportFeedbackNumber;

    UCHAR           continuation_flag;

    union
    {
        UINT        value;
        struct
        {
            UINT    lists_modification_present_flag             : 1;
            UINT    long_term_ref_pics_present_flag             : 1;
            UINT    sps_temporal_mvp_enabled_flag               : 1;
            UINT    cabac_init_present_flag                     : 1;
            UINT    output_flag_present_flag                    : 1;
            UINT    dependent_slice_segments_enabled_flag       : 1;
            UINT    pps_slice_chroma_qp_offsets_present_flag    : 1;
            UINT    slice_temporal_mvp_enabled_flag             : 1;
            UINT    sample_adaptive_offset_enabled_flag         : 1;
            UINT    deblocking_filter_control_present_flag      : 1;
            UINT    deblocking_filter_override_enabled_flag     : 1;
            UINT    pps_disable_deblocking_filter_flag          : 1;
            UINT    IrapPicFlag                                 : 1;
            UINT    IdrPicFlag                                  : 1;
            UINT    IntraPicFlag                                : 1;
            UINT    reserved_bits                               :17;
        } fields;
    } pic_short_format_flags;

    DXVA_PicEntry_HEVC    CollocatedRefIdx;
    UCHAR           RefPicSetStCurrBefore[16];
    UCHAR           RefPicSetStCurrAfter[16];
    UCHAR           RefPicSetLtCurr[16];
    UCHAR           log2_max_pic_order_cnt_lsb_minus4;    // [0..12]
    UCHAR           num_short_term_ref_pic_sets;
    UCHAR           num_long_term_ref_pics_sps;
    UCHAR           num_ref_idx_l0_default_active_minus1;
    UCHAR           num_ref_idx_l1_default_active_minus1;
    CHAR            pps_beta_offset_div2;
    CHAR            pps_tc_offset_div2;
} DXVA_PicParams_HEVC, *LPDXVA_PicParams_HEVC;
#endif


#if HEVC_SPEC_VER == MK_HEVCVER(0, 81)
typedef struct _DXVA_Slice_HEVC_Short
{
    UINT        BSNALunitDataLocation;
    UINT        SliceBytesInBuffer;
} DXVA_Slice_HEVC_Short, *LPDXVA_Slice_HEVC_Short;
#elif HEVC_SPEC_VER == MK_HEVCVER(0, 82)
typedef struct _DXVA_Slice_HEVC_Short
{
    UINT        BSNALunitDataLocation;
    UINT        SliceBytesInBuffer;
    UINT        wBadSliceChopping;
    UINT        StRPSBits;
} DXVA_Slice_HEVC_Short, *LPDXVA_Slice_HEVC_Short;
#endif

#if HEVC_SPEC_VER == MK_HEVCVER(0, 81)
typedef struct _DXVA_Slice_HEVC_Long
{
    UINT        BSNALunitDataLocation;
    UINT        SliceBytesInBuffer;

    UINT        ByteOffsetToSliceData;
    UINT        slice_segment_address;
    UINT        num_LCUs_for_slice;

    DXVA_PicEntry_HEVC    RefPicList[2][16];

    union
    {
        UINT        dwPicFlags;
        struct
        {
            UINT    last_slice_of_pic               : 1;
            UINT    dependent_slice_segment_flag    : 1;
            UINT    slice_type                      : 2;
            UINT    color_plane_id                  : 2;
            UINT    slice_sao_luma_flag             : 1;
            UINT    slice_sao_chroma_flag           : 1;
            UINT    mvd_l1_zero_flag                : 1;
            UINT    cabac_init_flag                 : 1;
            UINT    slice_disable_lf_flag           : 1;
            UINT    collocated_from_l0_flag         : 1;
            UINT    lf_across_slices_enabled_flag   : 1;
            UINT    reserved                        : 19;
        };
    };

    UCHAR   num_ref_idx_l0_active_minus1;
    UCHAR   num_ref_idx_l1_active_minus1;
    CHAR    slice_qp_delta;
    CHAR    slice_cb_qp_offset;
    CHAR    slice_cr_qp_offset;
    CHAR    beta_offset_div2;
    CHAR    tc_offset_div2;
    UCHAR   luma_log2_weight_denom;
    UCHAR   chroma_log2_weight_denom;
    CHAR    luma_offset[2][16];
    CHAR    delta_luma_weight[2][16];
    CHAR    chroma_offset[2][16][2];
    CHAR    delta_chroma_weight[2][16][2];
    UCHAR   max_num_merge_candidates;
    UINT    num_entry_point_offsets;

} DXVA_Slice_HEVC_Long, *LPDXVA_Slice_HEVC_Long;
#elif HEVC_SPEC_VER == MK_HEVCVER(0, 82)
typedef struct _DXVA_Slice_HEVC_Long
{
    UINT            BSNALunitDataLocation;
    UINT            SliceBytesInBuffer;
    UINT            wBadSliceChopping;
    UINT            ByteOffsetToSliceData;
    UINT            slice_segment_address;
    UINT            num_LCUs_for_slice;

    DXVA_PicEntry_HEVC    RefPicList[2][16];

    union
    {
        UINT        dwPicFlags;
        struct
        {
            UINT    last_slice_of_pic               : 1;
            UINT    dependent_slice_segment_flag    : 1;
            UINT    slice_type                      : 2;
            UINT    color_plane_id                  : 2;
            UINT    slice_sao_luma_flag             : 1;
            UINT    slice_sao_chroma_flag           : 1;
            UINT    mvd_l1_zero_flag                : 1;
            UINT    cabac_init_flag                 : 1;
            UINT    slice_disable_lf_flag           : 1;
            UINT    collocated_from_l0_flag         : 1;
            UINT    lf_across_slices_enabled_flag   : 1;
            UINT    reserved                        : 19;
        };
    };

    UCHAR           num_ref_idx_l0_active_minus1;
    UCHAR           num_ref_idx_l1_active_minus1;
    CHAR            slice_qp_delta;
    CHAR            slice_cb_qp_offset;
    CHAR            slice_cr_qp_offset;
    CHAR            beta_offset_div2;
    CHAR            tc_offset_div2;
    UCHAR           luma_log2_weight_denom;
    UCHAR           chroma_log2_weight_denom;
    CHAR            luma_offset[2][16];
    CHAR            delta_luma_weight[2][16];
    CHAR            chroma_offset[2][16][2];
    CHAR            delta_chroma_weight[2][16][2];
    UCHAR           max_num_merge_candidates;
    UINT            num_entry_point_offsets;

} DXVA_Slice_HEVC_Long, *LPDXVA_Slice_HEVC_Long;
#endif

typedef struct _DXVA_Qmatrix_HEVC
{
#if 1 //HEVC_SPEC_VER == MK_HEVCVER(0, 81)
    UCHAR  ucScalingLists0[3][2][16];
    UCHAR  ucScalingLists1[3][2][64];
    UCHAR  ucScalingLists2[3][2][64];
    UCHAR  ucScalingLists3[2][64];
    UCHAR  bScalingListDC[2][3][2];
#else //lif HEVC_SPEC_VER == MK_HEVCVER(0, 82)
    UCHAR   ucScalingLists0[6][16]; // 2 inter/intra 3: YUV
    UCHAR   ucScalingLists1[6][64];
    UCHAR   ucScalingLists2[6][64];
    UCHAR   ucScalingLists3[2][64];
    UCHAR   ucScalingListDCCoefSizeID2[6];
    UCHAR   ucScalingListDCCoefSizeID3[2];
#endif
} DXVA_Qmatrix_HEVC, *LPDXVA_Qmatrix_HEVC;

#endif  // !UMC_VA_LINUX

class Packer
{
public:
    Packer(UMC::VideoAccelerator * va);
    virtual ~Packer();

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier) = 0;

    virtual void PackSliceParams(H265Slice *pSlice, bool isLong, bool isLastSlice) = 0;

    virtual void PackQmatrix(const H265Slice *pSlice) = 0;

    virtual void ExecuteBuffers() = 0;

#if 0
    virtual void PackDeblockingParameters(
        const DeblockingParametersMBAFF *pParams,
        const H265SliceHeader *pSliceHeader,
        const H265DecoderFrame *pCurrentFrame,
        const H265DecoderLocalMacroblockDescriptor *pMBinfo,
        const H265PicParamSet *pPicParamSet) = 0;
#endif

protected:
    UMC::VideoAccelerator *m_va;
};

#ifdef UMC_VA_DXVA

class PackerDXVA2 : public Packer
{
public:
    PackerDXVA2(UMC::VideoAccelerator * va);

    virtual UMC::Status GetStatusReport(void * pStatusReport, size_t size);

    virtual void BeginFrame();
    virtual void EndFrame();

    virtual void PackQmatrix(const H265Slice *pSlice);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual void PackSliceParams(H265Slice *pSlice, bool isLong, bool isLastSlice);

    void GetPicParamVABuffer(DXVA_PicParams_HEVC **ppPicParam, size_t headerSize);

    void GetSliceVABuffers(
        DXVA_Slice_HEVC_Long **ppSliceHeader, size_t headerSize,
        void **ppSliceData, size_t dataSize,
        size_t dataAlignment);

    void GetIQMVABuffer(DXVA_Qmatrix_HEVC **, size_t bffrSize);

    void ExecuteBuffers();

#if 0
    virtual void PackDeblockingParameters(
                const DeblockingParametersMBAFF *pParams,
                const H265SliceHeader *pSliceHeader,
                const H265DecoderFrame *pCurrentFrame,
                const H265DecoderLocalMacroblockDescriptor *pMBinfo,
                const H265PicParamSet *pPicParamSet);
#endif

private:
    void AddReferenceFrame(DXVA_PicParams_HEVC * pPicParams_H264, Ipp32s &pos,
        H265DecoderFrame * pFrame, Ipp32s reference);

    void PackSliceGroups(DXVA_PicParams_HEVC * pPicParams_H264, H265DecoderFrame * frame);

    void PackPicParamsMVC(const H265DecoderFrameInfo * pSliceInfo, DXVA_Intel_PicParams_MVC* pMVCPicParams_H264);
#ifdef ENABLE_DXVA_PICPARAMS_H264_MVC
    void PackPicParamsMVC(const H265DecoderFrameInfo * pSliceInfo, DXVA_PicParams_HEVC_MVC* pMVCPicParams_H264);
#endif
    void SendPAVPStructure(Ipp32s numSlicesOfPrevField, H265Slice *pSlice);
    //check correctness of encrypted data
    void CheckData();
    //pointer to the beginning of offset for next slice in HW buffer
    Ipp8u * pBuf;

    Ipp32u  m_statusReportFeedbackCounter;
};

#endif // UMC_VA_DXVA


#ifdef UMC_VA_LINUX

class PackerVA : public Packer
{
public:
    PackerVA(UMC::VideoAccelerator * va);

    virtual void PackPicParams(const H265DecoderFrame *pCurrentFrame,
                        H265DecoderFrameInfo * pSliceInfo,
                        TaskSupplier_H265 * supplier);

    virtual bool PackSliceParams(H265Slice *pSlice,
                         Ipp32s sliceNum, bool all_data);

    virtual void PackQmatrix(const H265Slice *pSlice);

#if 0
    virtual void PackDeblockingParameters(
        const DeblockingParametersMBAFF *pParams,
        const H265SliceHeader *pSliceHeader,
        const H265DecoderFrame *pCurrentFrame,
        const H265DecoderLocalMacroblockDescriptor *pMBinfo,
        const H265PicParamSet *pPicParamSet);
#endif

private:
    void FillFrame(VAPictureH264 * pic, const H265DecoderFrame *pFrame,
        Ipp32s field, Ipp32s reference, Ipp32s defaultIndex);
    Ipp32s FillRefFrame(VAPictureH264 * pic, const H265DecoderFrame *pFrame,
        ReferenceFlags_H265 flags, bool isField, Ipp32s defaultIndex);

    void FillFrameAsInvalid(VAPictureH264 * pic);

    enum
    {
        VA_FRAME_INDEX_INVALID = 0x7f
    };
};

#endif // UMC_VA_LINUX

inline bool IsVLDProfile (Ipp32s profile)
{
    return (profile & UMC::VA_VLD) != 0;
}

} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif /* __UMC_H264_VA_PACKER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
