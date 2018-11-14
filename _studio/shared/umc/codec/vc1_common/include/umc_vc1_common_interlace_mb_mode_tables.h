// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER) || defined (UMC_ENABLE_VC1_SPLITTER) || defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef __UMC_VC1_COMMON_INTERLACE_MB_MODE_TABLES_H__
#define __UMC_VC1_COMMON_INTERLACE_MB_MODE_TABLES_H__

#include "umc_vc1_common_defs.h"

//VC-1 Table 145: mixed MV MB Mode Table 0
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable0[];

//VC-1 Table 146: mixed MV MB Mode Table 1
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable1[];

//VC-1 Table 147: mixed MV MB Mode Table 2
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable2[];

//VC-1 Table 148: mixed MV MB Mode Table 3
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable3[];

//VC-1 Table 149: mixed MV MB Mode Table 4
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable4[];

//VC-1 Table 150: mixed MV MB Mode Table 5
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable5[];

//VC-1 Table 151: mixed MV MB Mode Table 6
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable6[];

//VC-1 Table 152: mixed MV MB Mode Table 7
const extern Ipp32s VC1_Mixed_MV_MB_ModeTable7[];

//VC-1 Table 153: 1-MV MB Mode Table 0
const extern Ipp32s VC1_1MV_MB_ModeTable0[];

//VC-1 Table 154: 1-MV MB Mode Table 1
const extern Ipp32s VC1_1MV_MB_ModeTable1[];

//VC-1 Table 155: 1-MV MB Mode Table 2
const extern Ipp32s VC1_1MV_MB_ModeTable2[];

//VC-1 Table 156: 1-MV MB Mode Table 3
const extern Ipp32s VC1_1MV_MB_ModeTable3[];

//VC-1 Table 157: 1-MV MB Mode Table 4
const extern Ipp32s VC1_1MV_MB_ModeTable4[];

//VC-1 Table 158: 1-MV MB Mode Table 5
const extern Ipp32s VC1_1MV_MB_ModeTable5[];

//VC-1 Table 159: 1-MV MB Mode Table 6
const extern Ipp32s VC1_1MV_MB_ModeTable6[];

//VC-1 Table 160: 1-MV MB Mode Table 7
const extern Ipp32s VC1_1MV_MB_ModeTable7[];

//VC-1 Table 161: P/B picture 1-MV MB Mode Table 0
const extern Ipp32s VC1_4MV_MB_Mode_PBPic_Table0[];

//VC-1 Table 162: P/B picture 1-MV MB Mode Table 1
const extern Ipp32s VC1_4MV_MB_Mode_PBPic_Table1[];

//VC-1 Table 163: P/B picture 1-MV MB Mode Table 2
const extern Ipp32s VC1_4MV_MB_Mode_PBPic_Table2[];

//VC-1 Table 164: P/B picture 1-MV MB Mode Table 3
const extern Ipp32s VC1_4MV_MB_Mode_PBPic_Table3[];

//VC-1 Table 165 Interlace Frame P/B Pictures Non 4-MV MBMODE Table 0
const extern Ipp32s VC1_Non4MV_MB_Mode_PBPic_Table0[];

//VC-1 Table 166 Interlace Frame P/B Pictures Non 4-MV MBMODE Table 1
const extern Ipp32s VC1_Non4MV_MB_Mode_PBPic_Table1[];

//VC-1 Table 167 Interlace Frame P/B Pictures Non 4-MV MBMODE Table 2
const extern Ipp32s VC1_Non4MV_MB_Mode_PBPic_Table2[];

//VC-1 Table 168 Interlace Frame P/B Pictures Non 4-MV MBMODE Table 3
const extern Ipp32s VC1_Non4MV_MB_Mode_PBPic_Table3[];

//VC1 Tables 161-168
const extern Ipp8u VC1_MB_Mode_PBPic_Transform_Table[];
const extern Ipp8u VC1_MB_Mode_PBPic_MBtype_Table[];
const extern Ipp8s VC1_MB_Mode_PBPic_MVPresent_Table[];
const extern Ipp8u VC1_MB_Mode_PBPic_FIELDTX_Table[];

//VC1 Table 111, 112
const extern Ipp8u VC1_MB_Mode_PBFieldPic_MBtype_Table[];
const extern Ipp8u VC1_MB_Mode_PBFieldPic_CBPPresent_Table[];
const extern Ipp8s VC1_MB_Mode_PBFieldPic_MVData_Table[];

#endif //__umc_vc1_common_interlace_mb_mode_tables_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
