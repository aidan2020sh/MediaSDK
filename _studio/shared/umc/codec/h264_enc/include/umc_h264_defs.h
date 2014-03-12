/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#ifndef __UMC_H264_DEFS_H__
#define __UMC_H264_DEFS_H__

#include <vector>

#include "ippdefs.h"
#include "ippvc.h"
#include "ipps.h"

#include "umc_h264_config.h"
//#include "umc_video_encoder.h"
#include "umc_h264_video_encoder.h"

using namespace UMC;

#define H264ENC_UNREFERENCED_PARAMETER(X) X=X

#if (defined(__INTEL_COMPILER) || defined(_MSC_VER)) && !defined(_WIN32_WCE)
#define __ALIGN16 __declspec (align(16))
#define __ALIGN8 __declspec (align(8))
#elif defined(__GNUC__)
#define __ALIGN16 __attribute__ ((aligned (16)))
#define __ALIGN8 __attribute__ ((aligned (8)))
#else
#define __ALIGN16
#define __ALIGN8
#endif

#define BASE_MODE_FLAG_OFFSET   460
#define CABAC_CONTEXT_ARRAY_LEN (BASE_MODE_FLAG_OFFSET + 7)

#define GetMBFieldDecodingFlag(x) (((x).mb_aux_fields)&1)
#define GetMB8x8TSFlag(x) (((x).mb_aux_fields&2)>>1)
#define GetMB8x8TSPackFlag(x) (((x).mb_aux_fields&4)>>2)
#define GetMBBaseModeFlag(x) (((x).mb_aux_fields&8)>>3)
#define GetMBResidualPredictionFlag(x) (((x).mb_aux_fields&0x10)>>4)
#define GetMBCropFlag(x) (((x).mb_aux_fields&0x20)>>5)
#define GetMBIBLFlag(x) (((x).mb_aux_fields&0x40)>>6)
#define GetMB8x8TSBaseFlag(x) (((x).mb_aux_fields&0x80)>>7)

#define pGetMBFieldDecodingFlag(x) (((x)->mb_aux_fields)&1)
#define pGetMB8x8TSFlag(x) (((x)->mb_aux_fields&2)>>1)
#define pGetMB8x8TSPackFlag(x) (((x)->mb_aux_fields&4)>>2)
#define pGetMBBaseModeFlag(x) (((x)->mb_aux_fields&8)>>3)
#define pGetMBResidualPredictionFlag(x) (((x)->mb_aux_fields&0x10)>>4)
#define pGetMBCropFlag(x) (((x)->mb_aux_fields&0x20)>>5)
#define pGetMBIBLFlag(x) (((x)->mb_aux_fields&0x40)>>6)
#define pGetMB8x8TSBaseFlag(x) (((x)->mb_aux_fields&0x80)>>7)

#define pSetMBFieldDecodingFlag(x,y) {  \
    (((x)->mb_aux_fields) &= ~1);       \
    (((x)->mb_aux_fields) |= (y)); }

#define SetMBFieldDecodingFlag(x,y) {   \
    (((x).mb_aux_fields) &= ~1);        \
    (((x).mb_aux_fields) |= (y)); }

#define pSetMB8x8TSFlag(x,y)            \
  (((x)->mb_aux_fields) = (Ipp8u)((x)->mb_aux_fields & ~2));  {     \
  (((x)->mb_aux_fields) = (Ipp8u)((x)->mb_aux_fields | ((y)<<1))); }

#define SetMB8x8TSFlag(x,y) {            \
    (((x).mb_aux_fields) &= ~2);        \
    (((x).mb_aux_fields) |= ((y)<<1)); }

#define pSetMB8x8TSPackFlag(s,val)  {    \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~4);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<2)); }

#define SetMB8x8TSPackFlag(s,val) {   \
    (s).mb_aux_fields &= ~4;          \
    (s).mb_aux_fields |= (val)<<2; }

#define pSetMBBaseModeFlag(s,val) {    \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~8);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<3)); }

#define SetMBBaseModeFlag(s,val) {      \
    (s).mb_aux_fields &= ~8;          \
    (s).mb_aux_fields |= (val)<<3; }

#define pSetMBResidualPredictionFlag(s,val) {     \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~0x10);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<4)); }

#define SetMBResidualPredictionFlag(s,val) {     \
    (s).mb_aux_fields &= ~0x10;          \
    (s).mb_aux_fields |= (val)<<4; }

#define pSetMBCropFlag(s,val) {     \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~0x20);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<5)); }

#define SetMBCropFlag(s,val) {      \
    (s).mb_aux_fields &= ~0x20;          \
    (s).mb_aux_fields |= (val)<<5; }

#define pSetMBIBLFlag(s,val) {     \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~0x40);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<6)); }

#define SetMBIBLFlag(s,val)  {     \
    (s).mb_aux_fields &= ~0x40;          \
    (s).mb_aux_fields |= (val)<<6; }

#define pSetMB8x8TSBaseFlag(s,val) {     \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields & ~0x80);           \
    (s)->mb_aux_fields = (Ipp8u)((s)->mb_aux_fields | ((val)<<7)); }

#define SetMB8x8TSBaseFlag(s,val)  {     \
    (s).mb_aux_fields &= ~0x80;          \
    (s).mb_aux_fields |= (val)<<7; }

#define pSetMBSkippedFlag(s) { \
  (s)->mbtype = MBTYPE_SKIPPED; }

#define BLOCK_IS_ON_LEFT_EDGE(x) (!((x)&3))
#define BLOCK_IS_ON_RIGHT_EDGE(x) (!((x + 1)&3))
#define BLOCK_IS_ON_TOP_EDGE(x) ((x)<4)

#define CHROMA_BLOCK_IS_ON_LEFT_EDGE(x) (!((x)&1))
#define CHROMA_BLOCK_IS_ON_TOP_EDGE(x) ((x)<18 || (x)==20 || (x)==21)
#define CHROMA_BLOCK_IS_ON_TOP_EDGE_C(x) (!((x)&2))

#define H264ENC_CALL_NEW(STS, TYPE, PTR) {      \
    STS = UMC::UMC_ERR_ALLOC;                   \
    PTR = (H264ENC_MAKE_NAME(TYPE) *)H264_Malloc(sizeof(H264ENC_MAKE_NAME(TYPE)));  \
    if (PTR)                                    \
        STS = H264ENC_MAKE_NAME(TYPE##_Create)(PTR);               \
}

#define H264ENC_CALL_DELETE(TYPE, PTR)  \
    if (PTR) {                          \
        H264ENC_MAKE_NAME(TYPE##_Destroy)(PTR);            \
        H264_Free(PTR);                  \
        PTR = 0;                        \
    }

#define H264ENC_CALL_NEW_ARR(STS, TYPE, PTR, ITEMS) {   \
    STS = UMC::UMC_ERR_ALLOC;                           \
    size_t* _alloc_ptr = (size_t *)H264_Malloc(       \
        ITEMS * sizeof(H264ENC_MAKE_NAME(TYPE)) + sizeof(size_t));         \
    PTR = (H264ENC_MAKE_NAME(TYPE) *)(_alloc_ptr + 1);                               \
    _alloc_ptr[0] = (size_t)ITEMS;                      \
    if (PTR) {                                          \
        for (size_t ii = 0; ii < (size_t)ITEMS; ii++) {    \
            STS = H264ENC_MAKE_NAME(TYPE##_Create)((H264ENC_MAKE_NAME(TYPE) *)PTR + ii);       \
            if (STS != UMC::UMC_OK) {                   \
                for (ii++; ii > 0; ii--)                   \
                    H264ENC_MAKE_NAME(TYPE##_Destroy)((H264ENC_MAKE_NAME(TYPE) *)PTR + ii - 1);    \
                H264_Free(_alloc_ptr);                   \
                PTR = 0;                                \
                break;                                  \
            }                                           \
        }                                               \
    }                                                   \
}

#define H264ENC_CALL_DELETE_ARR(TYPE, PTR) {        \
    if (PTR) {                                      \
        size_t* _alloc_ptr = (size_t *)PTR - 1;     \
        for (size_t ii = _alloc_ptr[0]; ii > 0; ii--)  \
            H264ENC_MAKE_NAME(TYPE##_Destroy)((H264ENC_MAKE_NAME(TYPE) *)PTR + ii - 1);        \
        H264_Free(_alloc_ptr);                       \
        PTR = 0;                                    \
    }                                               \
}

namespace UMC_H264_ENCODER
{

#define COEFFICIENTS_BUFFER_SIZE 16 * 35

#define SUB_PEL_SHIFT 2

#define SubPelFactor 4  // Don't change this!!!

#undef  ABS
#define ABS(a)     (((a) < 0) ? (-(a)) : (a))
#undef  MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#undef  MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define H264_QP_MIN 0
#define H264_QP_MAX 51
#define H264_QP_RANGE (H264_QP_MAX - H264_QP_MIN + 1)

#define FLD_STRUCTURE 0
#define TOP_FLD_STRUCTURE 0
#define BOTTOM_FLD_STRUCTURE 1
#define FRM_STRUCTURE 2
#define AFRM_STRUCTURE 3

#define MVADJUST(ptr, pitchPixels, x, y)  ((ptr) + Ipp32s((pitchPixels)*(y) + (x)))

#define LIST_0     0
#define LIST_1     1

#define MAX_NUM_REF_FRAMES  32

#define __CABAC_FILE__ VM_STRING("cabac_enc.ipp")
#define __PICLIST_FILE__ VM_STRING("piclist_enc.ipp")

#define ALIGN_VALUE 16

#define ME_MAX_CANDIDATES 16

#define MAX_SAD 0x07FFFFFF

#define MAX_MV_INT   127

#define H264_Malloc  ippMalloc
#define H264_Free    ippFree

// Possible values for disable_deblocking_filter_idc:
enum DeblockingModes_t
{
    DEBLOCK_FILTER_ON                   = 0,
    DEBLOCK_FILTER_OFF                  = 1,
    DEBLOCK_FILTER_ON_NO_SLICE_EDGES             = 2,
    DEBLOCK_FILTER_ON_2_PASS                     = 3,
    DEBLOCK_FILTER_ON_NO_CHROMA                  = 4,
    DEBLOCK_FILTER_ON_NO_SLICE_EDGES_NO_CHROMA   = 5,
    DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA           = 6
};

// Number of bits (header plus residual) that triggers a MB to be recoded with a higher QP (up to twice) or in PCM mode.
#define MB_RECODE_THRESH        2750

#define DATA_ALIGN 64
#ifdef NO_PADDING
#define LUMA_PADDING 0
#else // NO_PADDING
#define LUMA_PADDING 32
#endif // NO_PADDING

#ifdef H264_INTRA_REFRESH
// define types of Intra refresh
enum IntraRefreshType
{
    NO_REFRESH     = 0,
    VERT_REFRESH   = 1,
    HORIZ_REFRESH  = 2,
    CUSTOM_REFRESH = 3
};
#endif // H264_INTRA_REFRESH

enum EnumPicCodType     // bits : Permitted Slice Types
{                       // ----------------------------
    INTRAPIC       = 0, // 000  : I (Intra)
    PREDPIC        = 1, // 001  : I, P (Pred)
    BPREDPIC       = 2, // 010  : I, P, B (BiPred)
    S_INTRAPIC     = 3, // 011  : SI (SIntra)
    S_PREDPIC      = 4, // 100  : SI, SP (SPred)
    MIXED_INTRAPIC = 5, // 101  : I, SI
    MIXED_PREDPIC  = 6, // 110  : I, SI, P, SP
    MIXED_BPREDPIC = 7  // 111  : I, SI, P, SP, B
};

typedef enum {
    MBTYPE_INTRA            = 0,  // 4x4 or 8x8
    MBTYPE_INTRA_16x16      = 1,
    //MBTYPE_INTRA_BL         =
    MBTYPE_PCM              = 2,  // Raw Pixel Coding, qualifies as a INTRA type...
    MBTYPE_INTER            = 3,  // 16x16
    MBTYPE_INTER_16x8       = 4,
    MBTYPE_INTER_8x16       = 5,
    MBTYPE_INTER_8x8        = 6,
    MBTYPE_INTER_8x8_REF0   = 7,  // same as MBTYPE_INTER_8x8, with all RefIdx=0
    MBTYPE_FORWARD          = 8,
    MBTYPE_BACKWARD         = 9,
    MBTYPE_SKIPPED          = 10,
    MBTYPE_DIRECT           = 11,
    MBTYPE_BIDIR            = 12,
    MBTYPE_FWD_FWD_16x8     = 13,
    MBTYPE_FWD_FWD_8x16     = 14,
    MBTYPE_BWD_BWD_16x8     = 15,
    MBTYPE_BWD_BWD_8x16     = 16,
    MBTYPE_FWD_BWD_16x8     = 17,
    MBTYPE_FWD_BWD_8x16     = 18,
    MBTYPE_BWD_FWD_16x8     = 19,
    MBTYPE_BWD_FWD_8x16     = 20,
    MBTYPE_BIDIR_FWD_16x8   = 21,
    MBTYPE_BIDIR_FWD_8x16   = 22,
    MBTYPE_BIDIR_BWD_16x8   = 23,
    MBTYPE_BIDIR_BWD_8x16   = 24,
    MBTYPE_FWD_BIDIR_16x8   = 25,
    MBTYPE_FWD_BIDIR_8x16   = 26,
    MBTYPE_BWD_BIDIR_16x8   = 27,
    MBTYPE_BWD_BIDIR_8x16   = 28,
    MBTYPE_BIDIR_BIDIR_16x8 = 29,
    MBTYPE_BIDIR_BIDIR_8x16 = 30,
    MBTYPE_B_8x8            = 31,
    NUMBER_OF_MBTYPES       = 32
} MB_Type;

#define IS_INTRA_MBTYPE(mbtype) ((mbtype) < MBTYPE_INTER)
#define IS_TRUEINTRA_MBTYPE(mbtype) ((mbtype) < MBTYPE_PCM)
#define IS_INTER_MBTYPE(mbtype) ((mbtype) >= MBTYPE_INTER)
#define IS_SKIP_MBTYPE(mbtype) ((mbtype == MBTYPE_SKIPPED) || (mbtype == MBTYPE_DIRECT))
#define IS_INTRA_MBTYPE_NOT_BL(x) (IS_INTRA_MBTYPE((x).mbtype) && !GetMBIBLFlag(x))
#define IS_INTRA_MBTYPE_NOT_BASE(mbtype, base) (((mbtype) < MBTYPE_INTER) && ((base) != 1))
#define IS_INTRA_MBTYPE_BL(x) (IS_INTRA_MBTYPE((x).mbtype) && GetMBIBLFlag(x))

#define pIS_INTRA_MBTYPE_NOT_BL(x) (IS_INTRA_MBTYPE((x)->mbtype) && !pGetMBIBLFlag(x))
#define pIS_INTRA_MBTYPE_BL(x) (IS_INTRA_MBTYPE((x)->mbtype) && pGetMBIBLFlag(x))


// 8x8 Macroblock subblock type definitions
typedef enum {
    SBTYPE_8x8          = 0,     // P slice modes
    SBTYPE_8x4          = 1,
    SBTYPE_4x8          = 2,
    SBTYPE_4x4          = 3,
    SBTYPE_DIRECT       = 4,     // B Slice modes
    SBTYPE_FORWARD_8x8  = 5,     // Subtract 4 for mode #
    SBTYPE_BACKWARD_8x8 = 6,
    SBTYPE_BIDIR_8x8    = 7,
    SBTYPE_FORWARD_8x4  = 8,
    SBTYPE_FORWARD_4x8  = 9,
    SBTYPE_BACKWARD_8x4 = 10,
    SBTYPE_BACKWARD_4x8 = 11,
    SBTYPE_BIDIR_8x4    = 12,
    SBTYPE_BIDIR_4x8    = 13,
    SBTYPE_FORWARD_4x4  = 14,
    SBTYPE_BACKWARD_4x4 = 15,
    SBTYPE_BIDIR_4x4    = 16
} SB_Type;

typedef enum {  /* UNITs from 1 till 5 inclusive are VCL units, other UNITs - not */
    NAL_UT_RESERVED  = 0x00, // Reserved / Unspecified in the spec
    NAL_UT_SLICE     = 0x01, // Coded Slice - slice_layer_no_partioning_rbsp
    NAL_UT_DPA       = 0x02, // Coded Data partition A - dpa_layer_rbsp
    NAL_UT_DPB       = 0x03, // Coded Data partition B - dpa_layer_rbsp
    NAL_UT_DPC       = 0x04, // Coded Data partition C - dpa_layer_rbsp
    NAL_UT_IDR_SLICE = 0x05, // Coded Slice of a IDR Picture - slice_layer_no_partioning_rbsp
    NAL_UT_SEI       = 0x06, // Supplemental Enhancement Information - sei_rbsp
    NAL_UT_SPS       = 0x07, // Sequence Parameter Set - seq_parameter_set_rbsp
    NAL_UT_PPS       = 0x08, // Picture Parameter Set - pic_parameter_set_rbsp
    NAL_UT_AUD       = 0x09, // Access Unit Delimiter - access_unit_delimiter_rbsp
    NAL_UT_EOSEQ     = 0x0a, // End of sequence - end_of_seq_rbsp
    NAL_UT_EOSTREAM  = 0x0b, // End of stream - end_of_stream_rbsp
    NAL_UT_FILL      = 0x0c, // Filler data - filler_data_rbsp
    NAL_UT_SEQEXT    = 0x0d, // Sequence parameter set extension - seq_parameter_set_extension_rbsp
    NAL_UT_PREFIX     = 0x0e, // Prefix NAL unit in scalable extension - prefix_nal_unit_rbsp
    NAL_UT_SUBSET_SPS = 0x0f, // Subset sequence parameter set RBSP - subset_seq_parameter_set_rbsp
    /* 0x10..0x12 - Reserved */
    NAL_UT_LAYERNOPART = 0x13, // Coded slice of an auxiliary coded picture without partitioning - slice_layer_without_partitioning_rbsp
    NAL_UT_CODED_SLICE_EXTENSION = 0x14, // Coded slice extension - slice_layer_extension_rbsp
    /* 0x15..0x17 - Reserved */
    /* 0x18..0x1f - Unspecified */
    //NAL_UT_IDR_SLICE_SCALABLE = 0x15 // Coded slice in scalable extension - slice_layer_in_scalable_extension_rbsp
    NAL_UT_DEPENDENCY_DELIMETER = 0x18 // NAL unit which starts dependency layer when view output is on
} NAL_Unit_Type;

typedef enum {
    SEI_TYPE_BUFFERING_PERIOD = 0,
    SEI_TYPE_PIC_TIMING,
    SEI_TYPE_PAN_SCAN_RECT,
    SEI_TYPE_FILLER_PAYLOAD,
    SEI_TYPE_USER_DATA_REGISTERED_ITU_T_T35,
    SEI_TYPE_USER_DATA_UNREGISTERED,
    SEI_TYPE_RECOVERY_POINT,
    SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION,
    SEI_TYPE_SPARE_PIC,
    SEI_TYPE_SCENE_INFO,
    SEI_TYPE_SUB_SEQ_INFO,
    SEI_TYPE_SUB_SEQ_LAYER_CHARACTERISTICS,
    SEI_TYPE_SUB_SEQ_CHARACTERISTICS,
    SEI_TYPE_FULL_FRAME_FREEZE,
    SEI_TYPE_FULL_FRAME_FREEZE_RELEASE,
    SEI_TYPE_FULL_FRAME_SNAPSHOT,
    SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_START,
    SEI_TYPE_PROGRESSIVE_REFINEMENT_SEGMENT_END,
    SEI_TYPE_MOTION_CONSTRAINED_SLICE_GROUP_SET,
    SEI_TYPE_FILM_GAIN_CHARACTERISTICS,
    SEI_TYPE_DEBLOCKING_FILTER_DISPLAY_PREFERENCE,
    SEI_TYPE_STEREO_VIDEO_INFO,
    SEI_TYPE_POST_FILTER_HINT,
    SEI_TYPE_TONE_MAPPING_INFO,
    SEI_TYPE_SCALABILITY_INFO,
    SEI_TYPE_SUB_PIC_SCALABLE_LAYER,
    SEI_TYPE_NON_REQUIRED_LAYER_REP,
    SEI_TYPE_PRIORITY_LAYER_INFO,
    SEI_TYPE_LAYERS_NOT_PRESENT,
    SEI_TYPE_LAYER_DEPENDENCY_CHANGE,
    SEI_TYPE_SCALABLE_NESTING,
    SEI_TYPE_BASE_LAYER_INTEGRITY_CHECK,
    SEI_TYPE_QUALITY_LAYER_INTEGRITY_CHECK,
    SEI_TYPE_REDUNDANT_PIC_PROPERTY,
    SEI_TYPE_TL0_PICTURE_INDEX,
    SEI_TYPE_TL_SWITCHING_POINT,
    SEI_TYPE_PARALLEL_DECODING_INFO,
    SEI_TYPE_MVC_SCALABLE_NESTING,
    SEI_TYPE_VIEW_SCALABILITY_INFO,
    SEI_TYPE_MULTIVIEW_SCENE_INFO,
    SEI_TYPE_MULTIVIEW_ACQUISITION_INFO,
    SEI_TYPE_NON_REQUIRED_VIEW_COMPONENT,
    SEI_TYPE_VIEW_DEPENDENCY_CHANGE,
    SEI_TYPE_OPERATION_POINTS_NOT_PRESENT,
    SEI_TYPE_BASE_VIEW_TEMPORAL_HRD,
    SEI_TYPE_FRAME_PACKING_ARRANGEMENT
} SEI_Type;

enum EnumPicClass {
    DISPOSABLE_PIC = 0, // No references to this picture from others, need not be decoded
    REFERENCE_PIC  = 1, // Other pictures may refer to this one, must be decoded
    IDR_PIC        = 2  // Instantaneous Decoder Refresh.  All previous references discarded.
};

enum MB_Neighbourings {
    MB_A = 0,
    MB_B,
    MB_C,
    MB_D,
    MB_ALL_NEIGHBOURS
};

typedef enum {
    D_DIR_FWD = 0,
    D_DIR_BWD,
    D_DIR_BIDIR,
    D_DIR_DIRECT,
    D_DIR_DIRECT_SPATIAL_FWD,
    D_DIR_DIRECT_SPATIAL_BWD,
    D_DIR_DIRECT_SPATIAL_BIDIR
} Direction_t;

enum EnumSliceType {
    PREDSLICE      = 0, // I (Intra), P (Pred)
    BPREDSLICE     = 1, // I, P, B (BiPred)
    INTRASLICE     = 2, // I
    S_PREDSLICE    = 3, // SP (SPred), I
    S_INTRASLICE   = 4  // SI (SIntra), I
};

#define IS_INTRA_SLICE(SliceType) ((SliceType) == INTRASLICE)
#define IS_P_SLICE(SliceType) ((SliceType) == PREDSLICE)
#define IS_B_SLICE(SliceType) ((SliceType) == BPREDSLICE)

enum EnumCodingType {
    CODINGTYPE_PROGRESSIVE   = 0,
    CODINGTYPE_INTERLACE     = 1,
    CODINGTYPE_MBAFF         = 2,
    CODINGTYPE_PICAFF        = 3
};

typedef Ipp32s H264MBAddr;
typedef Ipp8s T_RefIdx;
typedef Ipp8s T_AIMode;
typedef Ipp8u MBTypeValue;
typedef Ipp8u SBTypeValue;
typedef Ipp8s T_NumCoeffs;

typedef struct {
    Ipp32u uLumaOffset[2][2];    // from start of luma plane to upper left of MB (pixels)
    Ipp32u uChromaOffset[2][2];  // from start of first chroma plane to upper left (pixels) of chroma MB
    Ipp32s uMVLimits_L, uMVLimits_R, uMVLimits_U, uMVLimits_D;
} T_EncodeMBOffsets;


struct H264MotionVector
{
    Ipp16s  mvx;
    Ipp16s  mvy;

    bool is_zero() const
    { return ((mvx|mvy) == 0); }

    Ipp32s operator == (const H264MotionVector &mv) const
    { return mvx == mv.mvx && mvy == mv.mvy; };

    Ipp32s operator != (const H264MotionVector &mv) const
    { return !(*this == mv); };
};//4bytes

struct H264MacroblockRefIdxs
{
    T_RefIdx RefIdxs[16];                    // 16 bytes
};//16bytes

struct H264MacroblockMVs
{
    H264MotionVector MotionVectors[16];      //64 bytes
};//64bytes

// Advanced Intra mode
struct H264MacroblockIntraTypes
{
    T_AIMode intra_types[16];
};

struct H264MacroblockCoeffsInfo
{
    Ipp8u  numCoeff[48];
    Ipp8u  lumaAC;
    Ipp8u  chromaNC;
};

#ifdef USE_PSYINFO
typedef struct sPsyInfo {
    Ipp32s dc;       // sum (brightness
    Ipp32s bgshift;  // first order - smooth changing
    Ipp32s details;  // contrast details, mostly vertical
    Ipp32s interl;   // remained contrast details, mostly horizontal, reflects interlacing
} PsyInfo;
#endif // USE_PSYINFO

struct H264MacroblockGlobalInfo
{
    Ipp8u                sbtype[4];
    Ipp16s               slice_id;
    MBTypeValue          mbtype;
    union {
        Ipp8u                mb_aux_fields; // 2nd bit is transform_8x8_size_flag.
        struct {
            Ipp8u field:1;
            Ipp8u f8x8ts:1;
            Ipp8u f8x8tspack:1;
            Ipp8u basemode:1;
            Ipp8u respred:1;
            Ipp8u crop:1;
            Ipp8u ibl:1;
            Ipp8u f8x8tsbase:1;
        } mb_aux_fields_bits;
    };
    Ipp8u                i_pred[4];     // intra pred in inter MB
#ifdef USE_PSYINFO
    PsyInfo              psyStaticMB;
    PsyInfo              psyStaticBl[16]; // 16 luma blocks in raster order
    PsyInfo              psyDynaMB[2];    // 0 - with prev, 1 - with next
#endif // USE_PSYINFO
};

struct H264MacroblockLocalInfo
{
    Ipp32u               cbp_luma;
    Ipp32u               cbp_chroma;
    Ipp32u               cbp_bits;
    Ipp32u               cbp_bits_chroma;
    Ipp32u               cbp_luma_coeffs;
    Ipp32u               cbp_luma_residual;
    Ipp8u                cbp;
    Ipp8u                intra_16x16_mode;
    Ipp8u                intra_chroma_mode;
    Ipp8s                QP, QP_deblock;
    Ipp32u               cost;
    Ipp8u                m_mvsBudget;
    Ipp32s               m_mv_prediction_flag[2];
    //f Ipp32u               header_bits;
    //f Ipp32u               texture_bits;

    Ipp32s               lumaQP_ref, chromaQP_ref, lumaQP_ref_new, chromaQP_ref_new;
    MBTypeValue          mbtype_ref;
    // data for SVC IL MV prediction, located here for MB threading
    H264MacroblockMVs     m_RefLayerMVs[2];
    H264MacroblockRefIdxs m_RefLayerRefIdxs[2];
};

struct H264BlockLocation
{
    H264MBAddr           mb_num;
    H264MBAddr           block_num;
};//8 bytes

struct H264MacroblockNeighboursInfo
{
    H264MBAddr           mb_A;
    H264MBAddr           mb_B;
    H264MBAddr           mb_C;
    H264MBAddr           mb_D;
};// 16 bytea

struct H264BlockNeighboursInfo
{
    H264BlockLocation mbs_left[4];
    H264BlockLocation mb_above;
    H264BlockLocation mb_above_right;
    H264BlockLocation mb_above_left;
    H264BlockLocation mbs_left_chroma[2][2];
    H264BlockLocation mb_above_chroma[2];
};

//this structure is present in each  frame
struct H264GlobalMacroblocksDescriptor
{
    H264MacroblockMVs *MV[2];//MotionVectors L0 L1
    H264MacroblockRefIdxs *RefIdxs[2];//Reference Indexes L0 l1
    H264MacroblockGlobalInfo *mbs;//macroblocks
};

//this structure is one for all
struct H264LocalMacroblockDescriptor
{
    H264MacroblockMVs *MVDeltas[2];//MotionVectors Deltas L0 and L1
    H264MacroblockIntraTypes* intra_types;
    H264MacroblockCoeffsInfo *MacroblockCoeffsInfo; //info about num_coeffs in each block in the current  picture
    H264MacroblockLocalInfo *mbs;//reconstruction info
};

// MVC NAL header structure
struct H264MVCNALHeader {
    Ipp8u non_idr_flag;
    Ipp8u priority_id;
    Ipp8u view_id;
    Ipp8u temporal_id;
    Ipp8u anchor_pic_flag;
    Ipp8u inter_view_flag;
    Ipp8u reserved_one_bit;
};

// Slice header structure, corresponding to the H.264 bitstream definition.
struct H264SliceHeader
{
    Ipp8u       pic_parameter_set_id;               // of pic param set used for this slice
    Ipp8u       field_pic_flag;                     // zero: frame picture, else field picture
    Ipp8u       MbaffFrameFlag;
    Ipp8u       bottom_field_flag;                  // zero: top field, else bottom field
    Ipp8u       direct_spatial_mv_pred_flag;        // zero: temporal direct, else spatial direct
    // instead of those from pic param set
    Ipp8u       no_output_of_prior_pics_flag;       // nonzero: remove previously decoded pictures
    // from decoded picture buffer
    Ipp8u       long_term_reference_flag;           // nonzero: use as long term reference
    Ipp8u       adaptive_ref_pic_marking_mode_flag; // Ref pic marking mode of current picture
    Ipp8u       ref_pic_list_reordering_flag_l0;    // nonzero: ref pic list L0 reordering is applied for current picture
    Ipp8u       ref_pic_list_reordering_flag_l1;    // nonzero: ref pic list L1 reordering is applied for current picture
    Ipp8u       sp_for_switch_flag;                 // SP slice decoding control
    Ipp8s       slice_qs_delta;                     // to calculate default SP,SI slice QS
    Ipp8u       luma_log2_weight_denom;             // luma weighting denominator
    Ipp8u       chroma_log2_weight_denom;           // chroma weighting denominator
    Ipp32u      frame_num;
    Ipp32u      idr_pic_id;                         // ID of an IDR picture
    Ipp32u      pic_order_cnt_lsb;                  // picture order count (mod MaxPicOrderCntLsb)
    Ipp32s      delta_pic_order_cnt_bottom;         // Pic order count difference, top & bottom fields
    Ipp32s      delta_pic_order_cnt[2];             // picture order count differences
    Ipp32u      redundant_pic_cnt;                  // for redundant slices
    Ipp32u      slice_group_change_cycle;           // for slice group map types 3..5

};  // H264SliceHeader

// Weight table struct for weights for list 0 or list 1 of a slice
struct PredWeightTable
{
    Ipp8u       luma_weight_flag;           // nonzero: luma weight and offset in bitstream
    Ipp8u       chroma_weight_flag;         // nonzero: chroma weight and offset in bitstream
    Ipp8s       luma_weight;                // luma weighting factor
    Ipp8s       luma_offset;                // luma weighting offset
    Ipp8s       chroma_weight[2];           // chroma weighting factor (Cb,Cr)
    Ipp8s       chroma_offset[2];           // chroma weighting offset (Cb,Cr)
};  // PredWeightTable

// Reference picture list reordering data in the slice header.
struct RefPicListReorderInfo
{
    Ipp32u      num_entries;                // number of currently valid idc,value pairs
    Ipp8u       reordering_of_pic_nums_idc[MAX_NUM_REF_FRAMES];
    Ipp32u      reorder_value[MAX_NUM_REF_FRAMES];  // abs_diff_pic_num or long_term_pic_num
};

// Adaptive reference picture marking commands in the slice header
struct DecRefPicMarkingInfo
{
    Ipp8u       no_output_of_prior_pics_flag;
    Ipp8u       long_term_reference_flag;
    Ipp32u      num_entries;                // number of currently valid mmco,value pairs
    Ipp8u       mmco[MAX_NUM_REF_FRAMES];   // memory management control operation id
    Ipp32u      value[MAX_NUM_REF_FRAMES*2];// operation-dependent data, max 2 per operation
};


typedef Ipp32u H264_Encoder_Compression_Flags;
// This set of flags gets passed into each compress invocation.

typedef Ipp32u H264_Encoder_Compression_Notes;
// This set of flags gets returned from each compress invocation.

const H264_Encoder_Compression_Flags       H264_ECF_MORE_FRAMES      = 0x1;
// This flag indicates that the H264_Encoder's Encode method is being
// called to retrieve the second (or subsequent) output image
// corresponding to a single input image.

const H264_Encoder_Compression_Flags       H264_ECF_LAST_FRAME       = 0x2;
// This flag indicates that the H264_Encoder's Encode method is being
// called with the final frame of the sequence.  If this flag is not used
// and B frames are in use, then frames which are queued for encoding waiting
// for a future reference frame may not be encoded.  This flag forces the last
// frame to be a reference frame if it wouldn't have already been so.

const H264_Encoder_Compression_Notes       H264_ECN_KEY_FRAME        = 0x1;
// Indicates that the compressed image is a key frame.
// Note that enhancement layer EI frames are not key frames, in the
// traditional sense, because they have dependencies on lower layer
// frames.

const H264_Encoder_Compression_Notes       H264_ECN_B_FRAME          = 0x2;
// Indicates that the compressed image is a B frame.
// At most one of H264_ECN_KEY_FRAME and H264_ECN_B_FRAME will be set.

const H264_Encoder_Compression_Notes       H264_ECN_MORE_FRAMES      = 0x4;
// Indicates that the encoder has more frames to emit.  This flag is
// used by an encoder to inform the Hive layer that multiple output
// frames are to be emitted for a given input frame.  The Hive layer
// will loop until this flag is no longer set.
// This flag should only be returned when operating in video environments
// that support multiple output frames per input frame.

const H264_Encoder_Compression_Notes       H264_ECN_NO_FRAME         = 0x8;
// Indicates that the encoder has no frames to emit.  This is typically
// used when encoding a B frame, since the B frame cannot be encoded
// until a subsequent frame is encoded.
// This flag should only be used in encoder environments for which
// "Hive::Supports_Multiple_Encoder_Output_Images()" returns true.
// When used, this flag causes the Hive layer to suppress returning
// an encoded frame to its environment.
// For other environments, the encoder should not set this flag, but
// rather should return some sort of placeholder frame.

extern const H264MotionVector null_mv;

inline Ipp32u CalcPitchFromWidth(Ipp32u width, Ipp32s pixSize)
{
#ifdef NO_PADDING
    return align_value<Ipp32u> (((width + LUMA_PADDING * 4) * pixSize), 16);
#else // NO_PADDING
    return align_value<Ipp32u> (((width + LUMA_PADDING * 4) * pixSize), DATA_ALIGN);
#endif
}


#define PIXBITS 8
#include "umc_h264_defs_tmpl.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_defs_tmpl.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} // end namespace UMC_H264_ENCODER

#endif // __UMC_H264_DEFS_H__

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
