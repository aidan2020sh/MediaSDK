// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "umc_h264_dec_init_tables_cabac.h"
#include "umc_h264_dec_internal_cabac.h"

namespace UMC
{

const
uint8_t rangeTabLPS[128][4]=
{
    { 128, 176, 208, 240},
    { 128, 176, 208, 240},
    { 128, 167, 197, 227},
    { 128, 167, 197, 227},
    { 128, 158, 187, 216},
    { 128, 158, 187, 216},
    { 123, 150, 178, 205},
    { 123, 150, 178, 205},
    { 116, 142, 169, 195},
    { 116, 142, 169, 195},
    { 111, 135, 160, 185},
    { 111, 135, 160, 185},
    { 105, 128, 152, 175},
    { 105, 128, 152, 175},
    { 100, 122, 144, 166},
    { 100, 122, 144, 166},
    {  95, 116, 137, 158},
    {  95, 116, 137, 158},
    {  90, 110, 130, 150},
    {  90, 110, 130, 150},
    {  85, 104, 123, 142},
    {  85, 104, 123, 142},
    {  81,  99, 117, 135},
    {  81,  99, 117, 135},
    {  77,  94, 111, 128},
    {  77,  94, 111, 128},
    {  73,  89, 105, 122},
    {  73,  89, 105, 122},
    {  69,  85, 100, 116},
    {  69,  85, 100, 116},
    {  66,  80,  95, 110},
    {  66,  80,  95, 110},
    {  62,  76,  90, 104},
    {  62,  76,  90, 104},
    {  59,  72,  86,  99},
    {  59,  72,  86,  99},
    {  56,  69,  81,  94},
    {  56,  69,  81,  94},
    {  53,  65,  77,  89},
    {  53,  65,  77,  89},
    {  51,  62,  73,  85},
    {  51,  62,  73,  85},
    {  48,  59,  69,  80},
    {  48,  59,  69,  80},
    {  46,  56,  66,  76},
    {  46,  56,  66,  76},
    {  43,  53,  63,  72},
    {  43,  53,  63,  72},
    {  41,  50,  59,  69},
    {  41,  50,  59,  69},
    {  39,  48,  56,  65},
    {  39,  48,  56,  65},
    {  37,  45,  54,  62},
    {  37,  45,  54,  62},
    {  35,  43,  51,  59},
    {  35,  43,  51,  59},
    {  33,  41,  48,  56},
    {  33,  41,  48,  56},
    {  32,  39,  46,  53},
    {  32,  39,  46,  53},
    {  30,  37,  43,  50},
    {  30,  37,  43,  50},
    {  29,  35,  41,  48},
    {  29,  35,  41,  48},
    {  27,  33,  39,  45},
    {  27,  33,  39,  45},
    {  26,  31,  37,  43},
    {  26,  31,  37,  43},
    {  24,  30,  35,  41},
    {  24,  30,  35,  41},
    {  23,  28,  33,  39},
    {  23,  28,  33,  39},
    {  22,  27,  32,  37},
    {  22,  27,  32,  37},
    {  21,  26,  30,  35},
    {  21,  26,  30,  35},
    {  20,  24,  29,  33},
    {  20,  24,  29,  33},
    {  19,  23,  27,  31},
    {  19,  23,  27,  31},
    {  18,  22,  26,  30},
    {  18,  22,  26,  30},
    {  17,  21,  25,  28},
    {  17,  21,  25,  28},
    {  16,  20,  23,  27},
    {  16,  20,  23,  27},
    {  15,  19,  22,  25},
    {  15,  19,  22,  25},
    {  14,  18,  21,  24},
    {  14,  18,  21,  24},
    {  14,  17,  20,  23},
    {  14,  17,  20,  23},
    {  13,  16,  19,  22},
    {  13,  16,  19,  22},
    {  12,  15,  18,  21},
    {  12,  15,  18,  21},
    {  12,  14,  17,  20},
    {  12,  14,  17,  20},
    {  11,  14,  16,  19},
    {  11,  14,  16,  19},
    {  11,  13,  15,  18},
    {  11,  13,  15,  18},
    {  10,  12,  15,  17},
    {  10,  12,  15,  17},
    {  10,  12,  14,  16},
    {  10,  12,  14,  16},
    {   9,  11,  13,  15},
    {   9,  11,  13,  15},
    {   9,  11,  12,  14},
    {   9,  11,  12,  14},
    {   8,  10,  12,  14},
    {   8,  10,  12,  14},
    {   8,   9,  11,  13},
    {   8,   9,  11,  13},
    {   7,   9,  11,  12},
    {   7,   9,  11,  12},
    {   7,   9,  10,  12},
    {   7,   9,  10,  12},
    {   7,   8,  10,  11},
    {   7,   8,  10,  11},
    {   6,   8,   9,  11},
    {   6,   8,   9,  11},
    {   6,   7,   9,  10},
    {   6,   7,   9,  10},
    {   6,   7,   8,   9},
    {   6,   7,   8,   9},
    {   2,   2,   2,   2},
    {   2,   2,   2,   2}
};

#define DECL(val) \
    (val) * 2, (val) * 2 + 1

const
uint8_t transIdxMPS[] =
{
    DECL( 1), DECL( 2), DECL( 3), DECL( 4), DECL( 5), DECL( 6), DECL( 7), DECL( 8),
    DECL( 9), DECL(10), DECL(11), DECL(12), DECL(13), DECL(14), DECL(15), DECL(16),
    DECL(17), DECL(18), DECL(19), DECL(20), DECL(21), DECL(22), DECL(23), DECL(24),
    DECL(25), DECL(26), DECL(27), DECL(28), DECL(29), DECL(30), DECL(31), DECL(32),
    DECL(33), DECL(34), DECL(35), DECL(36), DECL(37), DECL(38), DECL(39), DECL(40),
    DECL(41), DECL(42), DECL(43), DECL(44), DECL(45), DECL(46), DECL(47), DECL(48),
    DECL(49), DECL(50), DECL(51), DECL(52), DECL(53), DECL(54), DECL(55), DECL(56),
    DECL(57), DECL(58), DECL(59), DECL(60), DECL(61), DECL(62), DECL(62), DECL(63)
};

const
uint8_t transIdxLPS[] =
{
    1,   0,   DECL( 0), DECL( 1), DECL( 2), DECL( 2), DECL( 4), DECL( 4), DECL( 5),
    DECL( 6), DECL( 7), DECL( 8), DECL( 9), DECL( 9), DECL(11), DECL(11), DECL(12),
    DECL(13), DECL(13), DECL(15), DECL(15), DECL(16), DECL(16), DECL(18), DECL(18),
    DECL(19), DECL(19), DECL(21), DECL(21), DECL(22), DECL(22), DECL(23), DECL(24),
    DECL(24), DECL(25), DECL(26), DECL(26), DECL(27), DECL(27), DECL(28), DECL(29),
    DECL(29), DECL(30), DECL(30), DECL(30), DECL(31), DECL(32), DECL(32), DECL(33),
    DECL(33), DECL(33), DECL(34), DECL(34), DECL(35), DECL(35), DECL(35), DECL(36),
    DECL(36), DECL(36), DECL(37), DECL(37), DECL(37), DECL(38), DECL(38), DECL(63)
};

const
uint32_t NumBitsToGetTableSmall[4] =
{
    2, 1, 0, 0
};

const
uint8_t NumBitsToGetTbl[512] =
{
    0,                                                                 // 0
    8,                                                                 // 1
    7,7,                                                               // 2..3
    6,6,6,6,                                                           // 4..7
    5,5,5,5,5,5,5,5,                                                   // 9..16
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,   // 17..31
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,   // 32..63
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,   // 127..255
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
};

// See table 9-11 of H.264 standard
const
uint32_t ctxIdxOffset[MAIN_SYNTAX_ELEMENT_NUMBER] =
{
    11,
    24,
    70,
    0,
    3,
    14,
    27,
    73,
    77,
    60,
    68,
    69,
    64,
    54,
    54,
    40,
    40,
    47,
    47,
    21,
    36,
    399,
// SVC
    460,
    465,
    463,
    464
};

// See table 9-24 of H.264 standard
const
uint32_t ctxIdxOffset4x4FrameCoded[SYNTAX_ELEMENT_NUMBER] =
{
    85,
    105,
    166,
    227
};

const
uint32_t ctxIdxOffset8x8FrameCoded[SYNTAX_ELEMENT_NUMBER] =
{
    85,//unused
    402,
    417,
    426
};

// See table 9-24 of H.264 standard
const
uint32_t ctxIdxOffset4x4FieldCoded[SYNTAX_ELEMENT_NUMBER] =
{
    85,
    277,
    338,
    227
};

const
uint32_t ctxIdxOffset8x8FieldCoded[SYNTAX_ELEMENT_NUMBER] =
{
    85,//unused
    436,
    451,
    426
};

// See table 9-30 of H.264 standard
const
uint32_t ctxIdxBlockCatOffset[SYNTAX_ELEMENT_NUMBER][BLOCK_CATEGORY_NUMBER] =
{
   //BLOCK_LUMA_DC_LEVELS,
      //BLOCK_LUMA_AC_LEVELS
          //BLOCK_LUMA_LEVELS
              //BLOCK_CHROMA_DC420_LEVELS
                  //BLOCK_CHROMA_DC422_LEVELS
                      //BLOCK_CHROMA_DC444_LEVELS
                            //BLOCK_CHROMA_AC_LEVELS
                                //BLOCK_LUMA_8X8_LEVELS
    {0,  4,  8, 12, 12, 12, 16, 0},//CODED_BLOCK_FLAG
    {0, 15, 29, 44, 44, 44, 47, 0},//SIGNIFICANT_COEFF_FLAG
    {0, 15, 29, 44, 44, 44, 47, 0},//LAST_SIGNIFICANT_COEFF_FLAG
    {0, 10, 20, 30, 30, 30, 39, 0},//COEFF_ABS_LEVEL_MINUS1
};

} // namespace UMC

#endif // MFX_ENABLE_H264_VIDEO_DECODE