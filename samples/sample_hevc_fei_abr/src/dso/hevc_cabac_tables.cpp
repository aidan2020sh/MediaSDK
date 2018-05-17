/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "hevc_cabac.h"

namespace BS_HEVC
{

const Bs8u CtxOffset[num_SE+1] = {
/*SAO_MERGE_LEFT_FLAG          */          0,
/*SAO_TYPE_IDX_LUMA            */          1,
/*SPLIT_CU_FLAG                */          2,
/*CU_TRANSQUANT_BYPASS_FLAG    */          5,
/*CU_SKIP_FLAG                 */          6,
/*PRED_MODE_FLAG               */          9,
/*PART_MODE                    */         10,
/*PREV_INTRA_LUMA_PRED_FLAG    */         14,
/*INTRA_CHROMA_PRED_MODE       */         15,
/*RQT_ROOT_CBF                 */         16,
/*MERGE_FLAG                   */         17,
/*MERGE_IDX                    */         18,
/*INTER_PRED_IDC               */         19,
/*REF_IDX_LX                   */         24,
/*MVP_LX_FLAG                  */         26,
/*SPLIT_TRANSFORM_FLAG         */         27,
/*CBF_LUMA                     */         30,
/*CBF_CX                       */         32,
/*ABS_MVD_GREATER0_FLAG        */         36 + 1,
/*ABS_MVD_GREATER1_FLAG        */         37 + 1,
/*CU_QP_DELTA_ABS              */         38 + 1,
/*TRANSFORM_SKIP_FLAG0         */         40 + 1,
/*TRANSFORM_SKIP_FLAG1         */         41 + 1,
/*LAST_SIG_COEFF_X_PREFIX      */         42 + 1,
/*LAST_SIG_COEFF_Y_PREFIX      */         60 + 1,
/*CODED_SUB_BLOCK_FLAG         */         78 + 1,
/*SIG_COEFF_FLAG               */         82 + 1,
/*COEFF_ABS_LEVEL_GREATER1_FLAG*/        124 + 1 + 2,
/*COEFF_ABS_LEVEL_GREATER2_FLAG*/        148 + 1 + 2,
/*CU_CHROMA_QP_OFFSET_FLAG     */        154 + 1 + 2,
/*CU_CHROMA_QP_OFFSET_IDX      */        155 + 1 + 2,
/*LOG2_RES_SCALE_ABS_PLUS1     */        156 + 1 + 2,
/*RES_SCALE_SIGN_FLAG          */        164 + 1 + 2,
/*EXPLICIT_RDPCM_FLAG          */        166 + 1 + 2,
/*EXPLICIT_RDPCM_DIR_FLAG      */        168 + 1 + 2,
/*PALETTE_MODE_FLAG                    */170 + 1 + 2,
/*TU_RESIDUAL_ACT_FLAG                 */171 + 1 + 2,
/*PALETTE_RUN_PREFIX                   */172 + 1 + 2,
/*COPY_ABOVE_PALETTE_INDICES_FLAG      */180 + 1 + 2,
/*COPY_ABOVE_INDICES_FOR_FINAL_RUN_FLAG*/180 + 1 + 2,
/*PALETTE_TRANSPOSE_FLAG               */181 + 1 + 2,
/*                             */ CtxTblSize
};

const Bs8u CtxInitTbl[3][CtxTblSize] =
{
    {
        153, 200, 139, 141, 157, 154, 154, 154, 154, 154,
        184, 154, 154, 154, 184,  63, 154, 154, 154, 154,
        154, 154, 154, 154, 154, 154, 154, 153, 138, 138,
        111, 141,  94, 138, 182, 154, 154, 154, 154, 154,
        154, 139, 139, 110, 110, 124, 125, 140, 153, 125,
        127, 140, 109, 111, 143, 127, 111,  79, 108, 123,
         63, 110, 110, 124, 125, 140, 153, 125, 127, 140,
        109, 111, 143, 127, 111,  79, 108, 123,  63,  91,
        171, 134, 141, 111, 111, 125, 110, 110,  94, 124,
        108, 124, 107, 125, 141, 179, 153, 125, 107, 125,
        141, 179, 153, 125, 107, 125, 141, 179, 153, 125,
        140, 139, 182, 182, 152, 136, 152, 136, 153, 136,
        139, 111, 136, 139, 111, 141, 111, 140,  92, 137,
        138, 140, 152, 138, 139, 153,  74, 149,  92, 139,
        107, 122, 152, 140, 179, 166, 182, 140, 227, 122,
        197, 138, 153, 136, 167, 152, 152, 154, 154, 154,
        154, 154, 154, 154, 154, 154, 154, 154, 154, 139,
        139, 139, 139, 154, 154, 154, 154, 154, 154, 154,
        154, 154, 154, 154, 154
    },
    {
        153, 185, 107, 139, 126, 154, 197, 185, 201, 149,
        154, 139, 154, 154, 154, 152,  79, 110, 122,  95,
         79,  63,  31,  31, 153, 153, 168, 124, 138,  94,
        153, 111, 149, 107, 167, 154, 154, 140, 198, 154,
        154, 139, 139, 125, 110,  94, 110,  95,  79, 125,
        111, 110,  78, 110, 111, 111,  95,  94, 108, 123,
        108, 125, 110,  94, 110,  95,  79, 125, 111, 110,
         78, 110, 111, 111,  95,  94, 108, 123, 108, 121,
        140,  61, 154, 155, 154, 139, 153, 139, 123, 123,
         63, 153, 166, 183, 140, 136, 153, 154, 166, 183,
        140, 136, 153, 154, 166, 183, 140, 136, 153, 154,
        170, 153, 123, 123, 107, 121, 107, 121, 167, 151,
        183, 140, 151, 183, 140, 140, 140, 154, 196, 196,
        167, 154, 152, 167, 182, 182, 134, 149, 136, 153,
        121, 136, 137, 169, 194, 166, 167, 154, 167, 137,
        182, 107, 167,  91, 122, 107, 167, 154, 154, 154,
        154, 154, 154, 154, 154, 154, 154, 154, 154, 139,
        139, 139, 139, 154, 154, 154, 154, 154, 154, 154,
        154, 154, 154, 154, 154
    },
    {
        153, 160, 107, 139, 126, 154, 197, 185, 201, 134,
        154, 139, 154, 154, 183, 152,  79, 154, 137,  95,
         79,  63,  31,  31, 153, 153, 168, 224, 167, 122,
        153, 111, 149,  92, 167, 154, 154, 169, 198, 154,
        154, 139, 139, 125, 110, 124, 110,  95,  94, 125,
        111, 111,  79, 125, 126, 111, 111,  79, 108, 123,
         93, 125, 110, 124, 110,  95,  94, 125, 111, 111,
         79, 125, 126, 111, 111,  79, 108, 123,  93, 121,
        140,  61, 154, 170, 154, 139, 153, 139, 123, 123,
         63, 124, 166, 183, 140, 136, 153, 154, 166, 183,
        140, 136, 153, 154, 166, 183, 140, 136, 153, 154,
        170, 153, 138, 138, 122, 121, 122, 121, 167, 151,
        183, 140, 151, 183, 140, 140, 140, 154, 196, 167,
        167, 154, 152, 167, 182, 182, 134, 149, 136, 153,
        121, 136, 122, 169, 208, 166, 167, 154, 152, 167,
        182, 107, 167,  91, 107, 107, 167, 154, 154, 154,
        154, 154, 154, 154, 154, 154, 154, 154, 154, 139,
        139, 139, 139, 154, 154, 154, 154, 154, 154, 154,
        154, 154, 154, 154, 154
    }
};

const Bs8s HEVC_CABAC::CtxIncTbl[num_SE_full][6] = {
/*SAO_MERGE_LEFT_FLAG          */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*SAO_TYPE_IDX_LUMA            */ {         0,   BYPASS,    ERROR,    ERROR,    ERROR,    ERROR },
/*SPLIT_CU_FLAG                */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_TRANSQUANT_BYPASS_FLAG    */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_SKIP_FLAG                 */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PRED_MODE_FLAG               */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PART_MODE                    */ {         0,        1, EXTERNAL,   BYPASS,    ERROR,    ERROR },
/*PREV_INTRA_LUMA_PRED_FLAG    */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*INTRA_CHROMA_PRED_MODE       */ {         0,   BYPASS,   BYPASS,    ERROR,    ERROR,    ERROR },
/*RQT_ROOT_CBF                 */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*MERGE_FLAG                   */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*MERGE_IDX                    */ {         0,   BYPASS,   BYPASS,   BYPASS,    ERROR,    ERROR },
/*INTER_PRED_IDC               */ {  EXTERNAL,        4,    ERROR,    ERROR,    ERROR,    ERROR },
/*REF_IDX_LX                   */ {         0,        1,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*MVP_LX_FLAG                  */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*SPLIT_TRANSFORM_FLAG         */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CBF_LUMA                     */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CBF_CX                       */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*ABS_MVD_GREATER0_FLAG        */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*ABS_MVD_GREATER1_FLAG        */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_QP_DELTA_ABS              */ {         0,        1,        1,        1,        1,   BYPASS },
/*TRANSFORM_SKIP_FLAG0         */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*TRANSFORM_SKIP_FLAG1         */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*LAST_SIG_COEFF_X_PREFIX      */ {  EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL },
/*LAST_SIG_COEFF_Y_PREFIX      */ {  EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL },
/*CODED_SUB_BLOCK_FLAG         */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*SIG_COEFF_FLAG               */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*COEFF_ABS_LEVEL_GREATER1_FLAG*/ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*COEFF_ABS_LEVEL_GREATER2_FLAG*/ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_QP_DELTA_ABS              */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_QP_DELTA_ABS              */ {         0,        0,        0,        0,        0,    ERROR },
/*LOG2_RES_SCALE_ABS_PLUS1     */ {  EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL,    ERROR,    ERROR },
/*RES_SCALE_SIGN_FLAG          */ {  EXTERNAL,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*EXPLICIT_RDPCM_FLAG          */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*EXPLICIT_RDPCM_DIR_FLAG      */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PALETTE_MODE_FLAG            */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*TU_RESIDUAL_ACT_FLAG         */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PALETTE_RUN_PREFIX           */ {  EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL, EXTERNAL },
/*COPY_ABOVE_PALETTE_INDICES_FL*/ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*COPY_ABOVE_INDICES_FOR_FINAL_*/ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PALETTE_TRANSPOSE_FLAG       */ {         0,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*END_OF_SLICE_SEGMENT_FLAG    */ { TERMINATE,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*END_OF_SUB_STREAM_ONE_BIT    */ { TERMINATE,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*SAO_OFFSET_ABS               */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*SAO_OFFSET_SIGN              */ {    BYPASS,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*SAO_BAND_POSITION            */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*SAO_EO_CLASS_LUMA            */ {    BYPASS,   BYPASS,   BYPASS,    ERROR,    ERROR,    ERROR },
/*MPM_IDX                      */ {    BYPASS,   BYPASS,    ERROR,    ERROR,    ERROR,    ERROR },
/*REM_INTRA_LUMA_PRED_MODE     */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*ABS_MVD_MINUS2               */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*MVD_SIGN_FLAG                */ {    BYPASS,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*CU_QP_DELTA_SIGN_FLAG        */ {    BYPASS,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*LAST_SIG_COEFF_X_SUFFIX      */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*LAST_SIG_COEFF_Y_SUFFIX      */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*COEFF_ABS_LEVEL_REMAINING    */ {    BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS,   BYPASS },
/*COEFF_SIGN_FLAG              */ {    BYPASS,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
/*PCM_FLAG                     */ { TERMINATE,    ERROR,    ERROR,    ERROR,    ERROR,    ERROR },
};

const Bs8u rangeTabLps[64][4] = {
    { 128, 176, 208, 240 },
    { 128, 167, 197, 227 },
    { 128, 158, 187, 216 },
    { 123, 150, 178, 205 },
    { 116, 142, 169, 195 },
    { 111, 135, 160, 185 },
    { 105, 128, 152, 175 },
    { 100, 122, 144, 166 },
    {  95, 116, 137, 158 },
    {  90, 110, 130, 150 },
    {  85, 104, 123, 142 },
    {  81,  99, 117, 135 },
    {  77,  94, 111, 128 },
    {  73,  89, 105, 122 },
    {  69,  85, 100, 116 },
    {  66,  80,  95, 110 },
    {  62,  76,  90, 104 },
    {  59,  72,  86,  99 },
    {  56,  69,  81,  94 },
    {  53,  65,  77,  89 },
    {  51,  62,  73,  85 },
    {  48,  59,  69,  80 },
    {  46,  56,  66,  76 },
    {  43,  53,  63,  72 },
    {  41,  50,  59,  69 },
    {  39,  48,  56,  65 },
    {  37,  45,  54,  62 },
    {  35,  43,  51,  59 },
    {  33,  41,  48,  56 },
    {  32,  39,  46,  53 },
    {  30,  37,  43,  50 },
    {  29,  35,  41,  48 },
    {  27,  33,  39,  45 },
    {  26,  31,  37,  43 },
    {  24,  30,  35,  41 },
    {  23,  28,  33,  39 },
    {  22,  27,  32,  37 },
    {  21,  26,  30,  35 },
    {  20,  24,  29,  33 },
    {  19,  23,  27,  31 },
    {  18,  22,  26,  30 },
    {  17,  21,  25,  28 },
    {  16,  20,  23,  27 },
    {  15,  19,  22,  25 },
    {  14,  18,  21,  24 },
    {  14,  17,  20,  23 },
    {  13,  16,  19,  22 },
    {  12,  15,  18,  21 },
    {  12,  14,  17,  20 },
    {  11,  14,  16,  19 },
    {  11,  13,  15,  18 },
    {  10,  12,  15,  17 },
    {  10,  12,  14,  16 },
    {   9,  11,  13,  15 },
    {   9,  11,  12,  14 },
    {   8,  10,  12,  14 },
    {   8,   9,  11,  13 },
    {   7,   9,  11,  12 },
    {   7,   9,  10,  12 },
    {   7,   8,  10,  11 },
    {   6,   8,   9,  11 },
    {   6,   7,   9,  10 },
    {   6,   7,   8,   9 },
    {   2,   2,   2,   2 }
};

const Bs8u transIdxLps[64] = {
     0,  0,  1,  2,  2,  4,  4,  5,  6,  7,  8,  9,  9, 11, 11, 12,
    13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21, 21, 22, 22, 23, 24,
    24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33,
    33, 33, 34, 34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63
};

const Bs8u transIdxMps[64] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
    33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 62, 63
};

};