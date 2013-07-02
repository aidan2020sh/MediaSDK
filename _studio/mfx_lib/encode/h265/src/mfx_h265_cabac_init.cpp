//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"

#define CNU                          154      ///< dummy initialization value for unused context models 'Context model Not Used'

static
Ipp8u initVal_for_transquant_bypass_flag[3][1] =
{
    {154},
    {154},
    {154}
};

//split_coding_unit_flag   INIT_SPLIT_FLAG
static
Ipp8u initVal_for_split_coding_unit_flag[3][3] =
{
    { 139,  141,  157, },
    { 107,  139,  126, },
    { 107,  139,  126, }
};

//skip_flag  INIT_SKIP_FLAG
static
Ipp8u initVal_for_skip_flag[3][3] =
{
    { CNU,  CNU,  CNU, },
    { 197,  185,  201, },
    { 197,  185,  201, }
};

//merge_flag  INIT_MERGE_FLAG_EXT
static
Ipp8u initVal_for_merge_flag[3][1] =
{
    { CNU, },
    { 110, },
    { 154, }
};
//merge_idx  INIT_MERGE_IDX_EXT

static
Ipp8u initVal_for_merge_idx[3][1] =
{
    { CNU, },
    { 122, },
    { 137, }
};
//PU size   INIT_PART_SIZE
static
Ipp8u initVal_for_PU_size[3][4] =
{
    { 184,  CNU,  CNU,  CNU, },
    { 154,  139,  CNU,  CNU, },
    { 154,  139,  CNU,  CNU, }
};

static
Ipp8u initVal_for_amp_split_position[3][1] =
{
    { CNU, },
    { 154, },
    { 154, },
};
//prediction mode  INIT_PRED_MODE
static
Ipp8u initVal_for_prediction_mode[3][1] =
{
    { CNU, },
    { 149, },
    { 134, }
};
//intra direction of luma INIT_INTRA_PRED_MODE
static
Ipp8u initVal_for_intra_direction_luma[3][1] =
{
    { 184, },
    { 154, },
    { 183, }
};
//intra direction of chroma  INIT_CHROMA_PRED_MODE
static
Ipp8u initVal_for_intra_direction_chroma[3][2] =
{
    {  63,  139, },
    { 152,  139, },
    { 152,  139, }
};
//temporal direction  INIT_INTER_DIR
static
Ipp8u initVal_for_temporal_direction[3][5] =
{
    { CNU,  CNU,  CNU,  CNU,  CNU, },
    {  95,   79,   63,   31,   31, },
    {  95,   79,   63,   31,   31, },
};
//motion vector difference   INIT_MVD
static
Ipp8u initVal_for_mvd[3][2] =
{
    { CNU,  CNU, },
    { 140,  198, },
    { 169,  198, }
};
//reference frame index  INIT_REF_PIC
static
Ipp8u initVal_for_ref_frame_idx[3][2] =
{
    { CNU,  CNU,},
    { 153,  153,},
    { 153,  153,}
};
//delta QP  INIT_DQP
static
Ipp8u initVal_for_dqp[3][4] =
{
    { 154,  154,  154, },
    { 154,  154,  154, },
    { 154,  154,  154, }
};
//INIT_QT_CBF
static
Ipp8u initVal_for_qt_cbf[3][10] =
{
    { 111,  141,  CNU,  CNU,  CNU,   94,  138,  182,  CNU,  CNU, },
    { 153,  111,  CNU,  CNU,  CNU,  149,  107,  167,  CNU,  CNU, },
    { 153,  111,  CNU,  CNU,  CNU,  149,   92,  167,  CNU,  CNU, }
};
//INIT_QT_ROOT_CBF
static
Ipp8u initVal_for_qt_root_cbf[3][1] =
{
    { CNU, },
    {  79, },
    {  79, }
};

static
Ipp8u initVal_for_last[3][30] =
{
    { 110,  110,  124,  125,  140,  153,  125,  127,  140,  109,  111,  143,  127,  111,   79,
    108,  123,   63,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,
    },
    { 125,  110,   94,  110,   95,   79,  125,  111,  110,   78,  110,  111,  111,   95,   94,
    108,  123,  108,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,
    },
    { 125,  110,  124,  110,   95,   94,  125,  111,  111,   79,  125,  126,  111,  111,   79,
    108,  123,   93,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,  CNU,
    },
};

//Sig Coeff Group INIT_SIG_CG_FLAG
static
Ipp8u initVal_for_sig_coeff_group[3][4] =
{
    { 91,  171, 134,  141,},
    {121,  140,  61,  154,},
    {121,  140,  61,  154,}
};

//Sig SC Model
static
Ipp8u initVal_for_sig_flag[3][42] =
{
  { 111,  111,  125,  110,  110,   94,  124,  108,  124,  107,  125,  141,  179,  153,
  125,  107,  125,  141,  179,  153,  125,  107,  125,  141,  179,  153,  125,  140,
  139,  182,  182,  152,  136,  152,  136,  153,  136,  139,  111,  136,  139,  111,  },
  { 155,  154,  139,  153,  139,  123,  123,   63,  153,  166,  183,  140,  136,  153,
  154,  166,  183,  140,  136,  153,  154,  166,  183,  140,  136,  153,  154,  170,
  153,  123,  123,  107,  121,  107,  121,  167,  151,  183,  140,  151,  183,  140,  },
  { 170,  154,  139,  153,  139,  123,  123,   63,  124,  166,  183,  140,  136,  153,
  154,  166,  183,  140,  136,  153,  154,  166,  183,  140,  136,  153,  154,  170,
  153,  138,  138,  122,  121,  122,  121,  167,  151,  183,  140,  151,  183,  140,  },
};

static
Ipp8u initVal_for_one_flag[3][24] =
{
    {140,   92,  137,  138,  140,  152,  138,  139,  153,   74,  149,   92,
    139,  107,  122,  152,  140,  179,  166,  182,  140,  227,  122,  197, },
    {154,  196,  196,  167,  154,  152,  167,  182,  182,  134,  149,  136,
    153,  121,  136,  137,  169,  194,  166,  167,  154,  167,  137,  182, },
    {154,  196,  167,  167,  154,  152,  167,  182,  182,  134,  149,  136,
    153,  121,  136,  122,  169,  208,  166,  167,  154,  152,  167,  182, }
};

static
Ipp8u initVal_for_abs_flag[3][6] =
{
    {138,  153,  136,  167,  152,  152,},
    {107,  167,   91,  122,  107,  167,},
    {107,  167,   91,  107,  107,  167,}
};

//motion vector predictor index  INIT_MVP_IDX
static
Ipp8u initVal_for_mvp_idx[3][2] =
{
    {CNU,  CNU, },
    {168,  CNU, },
    {168,  CNU, }
};

static
Ipp8u initVal_for_sao_merge_flag[3][1] =
{
    { 153, },
    { 153, },
    { 153, },
};

static
Ipp8u initVal_for_sao_type_idx[3][1] =
{
    {160, },
    {185, },
    {200, }
};

//Trans Subdiv Flag SC Model   INIT_TRANS_SUBDIV_FLAG
Ipp8u initVal_for_trans_subdiv_flag[3][3] =
{
  { 153,  138,  138, },
  { 124,  138,   94, },
  { 224,  167,  122, },
};

static
Ipp8u initVal_for_transform_skip_flag[3][2] =
{
    { 139,  139},
    { 139,  139},
    { 139,  139},
};

const Ipp32u h265_ctxIdxSize[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC] =
{
    10,
    1,
    30,
    30,
    4,
    42,
    24,
    6,
    3,
    2,
    1,
    3,
    3,
    1,
    1,
    4,
    1,
    1,
    1,
    2,
    5,
    2,
    2,
    4,
    2,
    1,
    1,
    1,
};

const
Ipp32u h265_ctxIdxOffset[MAIN_SYNTAX_ELEMENT_NUMBER_HEVC] =
{
    0,
    10,
    11,
    41,
    71,
    75,
    117,
    141,
    147,
    150,
    152,
    153,
    156,
    159,
    160,
    161,
    165,
    166,
    167,
    168,
    170,
    175,
    177,
    179,
    183,
    185,
    186,
    187,
};

static
void InitializeContext(CABAC_CONTEXT_H265 *pContext, Ipp8u initVal, Ipp32s SliceQPy)
{
    SliceQPy = IPP_MIN(IPP_MAX(0, SliceQPy), 51);

    Ipp32s slope      = (initVal >> 4) * 5 - 45;
    Ipp32s offset     = ((initVal & 15) << 3) - 16;
    Ipp32s initState  =  IPP_MIN(IPP_MAX(1, (((slope * SliceQPy) >> 4) + offset)), 126);
    Ipp32u mpState    = (initState >= 64);
    *pContext = Ipp8u(((mpState? (initState - 64) : (63 - initState)) << 0) + (mpState << 6));
} //void InitializeContext(CABAC_CONTEXT_H265 *pContext, Ipp8u initVal, Ipp32s SliceQPy)

void InitializeContextVariablesHEVC_CABAC(CABAC_CONTEXT_H265 *context_hevc, Ipp32s initializationType, Ipp32s SliceQPy)
{
    Ipp32u l = 0;
    SliceQPy = IPP_MAX(0, SliceQPy);

    //transquant_bypass_flag
    for (l = 0; l < h265_ctxIdxSize[CU_TRANSQUANT_BYPASS_FLAG_CTX]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[CU_TRANSQUANT_BYPASS_FLAG_CTX] + l,
            initVal_for_transquant_bypass_flag[initializationType][l], SliceQPy);
    }

    //split_coding_unit_flag
    for (l = 0; l < h265_ctxIdxSize[SPLIT_CODING_UNIT_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SPLIT_CODING_UNIT_FLAG_HEVC] + l,
            initVal_for_split_coding_unit_flag[initializationType][l], SliceQPy);
    }

    //skip_flag
    for (l = 0; l < h265_ctxIdxSize[SKIP_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SKIP_FLAG_HEVC] + l,
            initVal_for_skip_flag[initializationType][l], SliceQPy);
    }

    //merge_flag
    for (l = 0; l < h265_ctxIdxSize[MERGE_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[MERGE_FLAG_HEVC] + l,
            initVal_for_merge_flag[initializationType][l], SliceQPy);
    }

    //merge_idx
    for (l = 0; l < h265_ctxIdxSize[MERGE_IDX_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[MERGE_IDX_HEVC] + l,
            initVal_for_merge_idx[initializationType][l], SliceQPy);
    }

    //PU size
    for (l = 0; l < h265_ctxIdxSize[PART_SIZE_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[PART_SIZE_HEVC] + l,
            initVal_for_PU_size[initializationType][l], SliceQPy);
    }

    //AMP split position
    for (l = 0; l < h265_ctxIdxSize[AMP_SPLIT_POSITION_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[AMP_SPLIT_POSITION_HEVC] + l,
            initVal_for_amp_split_position[initializationType][l], SliceQPy);
    }

    //prediction mode
    for (l = 0; l < h265_ctxIdxSize[PRED_MODE_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[PRED_MODE_HEVC] + l,
            initVal_for_prediction_mode[initializationType][l], SliceQPy);
    }

    //intra direction of luma
    for (l = 0; l < h265_ctxIdxSize[INTRA_LUMA_PRED_MODE_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[INTRA_LUMA_PRED_MODE_HEVC] + l,
            initVal_for_intra_direction_luma[initializationType][l], SliceQPy);
    }

    //intra direction of chroma
    for (l = 0; l < h265_ctxIdxSize[INTRA_CHROMA_PRED_MODE_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[INTRA_CHROMA_PRED_MODE_HEVC] + l,
            initVal_for_intra_direction_chroma[initializationType][l], SliceQPy);
    }

    //temporal direction
    for (l = 0; l < h265_ctxIdxSize[INTER_DIR_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[INTER_DIR_HEVC] + l,
            initVal_for_temporal_direction[initializationType][l], SliceQPy);
    }

    //motion vector difference
    for (l = 0; l < h265_ctxIdxSize[MVD_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[MVD_HEVC] + l,
            initVal_for_mvd[initializationType][l], SliceQPy);
    }

    //reference frame index
    for (l = 0; l < h265_ctxIdxSize[REF_FRAME_IDX_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[REF_FRAME_IDX_HEVC] + l,
            initVal_for_ref_frame_idx[initializationType][l], SliceQPy);
    }

    //delta QP
    for (l = 0; l < h265_ctxIdxSize[DQP_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[DQP_HEVC] + l,
            initVal_for_dqp[initializationType][l], SliceQPy);
    }

    //INIT_QT_CBF
    for (l = 0; l < h265_ctxIdxSize[QT_CBF_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[QT_CBF_HEVC] + l,
            initVal_for_qt_cbf[initializationType][l], SliceQPy);
    }

    //INIT_QT_ROOT_CBF
    for (l = 0; l < h265_ctxIdxSize[QT_ROOT_CBF_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[QT_ROOT_CBF_HEVC] + l,
            initVal_for_qt_root_cbf[initializationType][l], SliceQPy);
    }

    //Last X
    for (l = 0; l < h265_ctxIdxSize[LAST_X_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[LAST_X_HEVC] + l,
            initVal_for_last[initializationType][l], SliceQPy);
    }

    //Last Y
    for (l = 0; l < h265_ctxIdxSize[LAST_Y_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[LAST_Y_HEVC] + l,
            initVal_for_last[initializationType][l], SliceQPy);
    }

    //Sig Coeff Group
    for (l = 0; l < h265_ctxIdxSize[SIG_COEFF_GROUP_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SIG_COEFF_GROUP_FLAG_HEVC] + l,
            initVal_for_sig_coeff_group[initializationType][l], SliceQPy);
    }

    //Sig SC Model Luma
    for (l = 0; l < h265_ctxIdxSize[SIG_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SIG_FLAG_HEVC] + l,
            initVal_for_sig_flag[initializationType][l], SliceQPy);
    }

    //One SC Model flag
    for (l = 0; l < h265_ctxIdxSize[ONE_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[ONE_FLAG_HEVC] + l,
            initVal_for_one_flag[initializationType][l], SliceQPy);
    }

    //Abs SC Model flag
    for (l = 0; l < h265_ctxIdxSize[ABS_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[ABS_FLAG_HEVC] + l,
            initVal_for_abs_flag[initializationType][l], SliceQPy);
    }

    //motion vector predictor index
    for (l = 0; l < h265_ctxIdxSize[MVP_IDX_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[MVP_IDX_HEVC] + l,
            initVal_for_mvp_idx[initializationType][l], SliceQPy);
    }

    //SAO merge flag
    for (l = 0; l < h265_ctxIdxSize[SAO_MERGE_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SAO_MERGE_FLAG_HEVC] + l,
            initVal_for_sao_merge_flag[initializationType][l], SliceQPy);
    }

    //SAO type idx
    for (l = 0; l < h265_ctxIdxSize[SAO_TYPE_IDX_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[SAO_TYPE_IDX_HEVC] + l,
            initVal_for_sao_type_idx[initializationType][l], SliceQPy);
    }

    //Trans Subdiv Flag SC Model
    for (l = 0; l < h265_ctxIdxSize[TRANS_SUBDIV_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[TRANS_SUBDIV_FLAG_HEVC] + l,
            initVal_for_trans_subdiv_flag[initializationType][l], SliceQPy);
    }

    //Transform skip flag
    for (l = 0; l < h265_ctxIdxSize[TRANSFORMSKIP_FLAG_HEVC]; l++)
    {
        InitializeContext(context_hevc + h265_ctxIdxOffset[TRANSFORMSKIP_FLAG_HEVC] + l,
            initVal_for_transform_skip_flag[initializationType][l], SliceQPy);
    }
    context_hevc[h265_ctxIdxOffset[END_OF_SLICE_FLAG_HEVC]] = 63;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE