// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_common_blk_order_tbl.h"
#include "umc_vc1_common_defs.h"

const extern uint32_t VC1_pixel_table[6] =
{
    VC1_PIXEL_IN_LUMA,
    VC1_PIXEL_IN_LUMA,
    VC1_PIXEL_IN_LUMA,
    VC1_PIXEL_IN_LUMA,
    VC1_PIXEL_IN_CHROMA,
    VC1_PIXEL_IN_CHROMA
};

//const extern uint16_t VC1_BlkOrderTbl[6][64] =
//{
//    /*Block 0*/
//    {
//         0,   1,   2,   3,   4,   5,   6,  7,
//        16,  17,  18,  19,  20,  21,  22,  23,
//        32,  33,  34,  35,  36,  37,  38,  39,
//        48,  49,  50,  51,  52,  53,  54,  55,
//        64,  65,  66,  67,  68,  69,  70,  71,
//        80,  81,  82,  83,  84,  85,  86,  87,
//        96,  97,  98,  99, 100, 101, 102, 103,
//        112, 113, 114, 115, 116, 117, 118, 119
//    },
//    /*Block 1*/
//    {
//        8,   9,  10,  11,  12,  13,  14,  15,
//       24,  25,  26,  27,  28,  29,  30,  31,
//       40,  41,  42,  43,  44,  45,  46,  47,
//       56,  57,  58,  59,  60,  61,  62,  63,
//       72,  73,  74,  75,  76,  77,  78,  79,
//       88,  89,  90,  91,  92,  93,  94,  95,
//      104, 105, 106, 107, 108, 109, 110, 111,
//      120, 121, 122, 123, 124, 125, 126, 127
//    },
//    /*Block 2*/
//    {
//      128, 129, 130, 131, 132, 133, 134, 135,
//      144, 145, 146, 147, 148, 149, 150, 151,
//      160, 161, 162, 163, 164, 165, 166, 167,
//      176, 177, 178, 179, 180, 181, 182, 183,
//      192, 193, 194, 195, 196, 197, 198, 199,
//      208, 209, 210, 211, 212, 213, 214, 215,
//      224, 225, 226, 227, 228, 229, 230, 231,
//      240, 241, 242, 243, 244, 245, 246, 247
//    },
//    /*Block 3*/
//    {
//      136, 137, 138, 139, 140, 141, 142, 143,
//      152, 153, 154, 155, 156, 157, 158, 159,
//      168, 169, 170, 171, 172, 173, 174, 175,
//      184, 185, 186, 187, 188, 189, 190, 191,
//      200, 201, 202, 203, 204, 205, 206, 207,
//      216, 217, 218, 219, 220, 221, 222, 223,
//      232, 233, 234, 235, 236, 237, 238, 239,
//      248, 249, 250, 251, 252, 253, 254, 255
//    },
//    /*Block 4*/
//    {
//      256, 257, 258, 259, 260, 261, 262, 263,
//      264, 265, 266, 267, 268, 269, 270, 271,
//      272, 273, 274, 275, 276, 277, 278, 279,
//      280, 281, 282, 283, 284, 285, 286, 287,
//      288, 289, 290, 291, 292, 293, 294, 295,
//      296, 297, 298, 299, 300, 301, 302, 303,
//      304, 305, 306, 307, 308, 309, 310, 311,
//      312, 313, 314, 315, 316, 317, 318, 319
//    },
//    /*Block 5*/
//    {
//      320, 321, 322, 323, 324, 325, 326, 327,
//      328, 329, 330, 331, 332, 333, 334, 335,
//      336, 337, 338, 339, 340, 341, 342, 343,
//      344, 345, 346, 347, 348, 349, 350, 351,
//      352, 353, 354, 355, 356, 357, 358, 359,
//      360, 361, 362, 363, 364, 365, 366, 367,
//      368, 369, 370, 371, 372, 373, 374, 375,
//      376, 377, 378, 379, 380, 381, 382, 383
//    }
//};

const extern uint32_t VC1_PredDCIndex[3][6] =
{
    {6, 7, 0, 1, 10, 12},
    {3, 6, 8, 0, 4, 5},
    {8, 0, 9, 2, 11, 13}
};

const extern uint32_t VC1_QuantIndex [2][6] =
{
    {0, 0, 2, 2, 0, 0}, //A
    {1, 2, 1, 2, 1, 1}  //C
};

const extern uint32_t VC1_BlockTable[50] =
{
  0, 0, 1, 0, 2, 0, 0, 0, 3, 0,
  0, 0, 0, 0, 0, 0, 4, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 5, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 6, 0
};

const extern uint32_t VC1_BlkStart[] =
{
    0, 8, 128, 136, 256, 320
};
#endif //MFX_ENABLE_VC1_VIDEO_DECODE