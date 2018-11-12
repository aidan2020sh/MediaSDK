// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include <ippvc.h>
#include "umc_mpeg2_dec_tbl.h"

int32_t MBAdressing[] =
{

11, /* max bits */
2, /* total subtables */
3,8,/* subtable sizes */

 1, /* 1-bit codes */
0x0001, 0x0001,
 0, /* 2-bit codes */
 2, /* 3-bit codes */
0x0003, 0x0002, 0x0002, 0x0003,
 2, /* 4-bit codes */
0x0003, 0x0004, 0x0002, 0x0005,
 2, /* 5-bit codes */
0x0003, 0x0006, 0x0002, 0x0007,
 0, /* 6-bit codes */
 2, /* 7-bit codes */
0x0007, 0x0008, 0x0006, 0x0009,
 6, /* 8-bit codes */
0x000b, 0x000a, 0x000a, 0x000b, 0x0009, 0x000c, 0x0008, 0x000d,
0x0007, 0x000e, 0x0006, 0x000f,
 0, /* 9-bit codes */
 6, /* 10-bit codes */
0x0017, 0x0010, 0x0016, 0x0011, 0x0015, 0x0012, 0x0014, 0x0013,
0x0013, 0x0014, 0x0012, 0x0015,
 13, /* 11-bit codes */
0x0023, 0x0016, 0x0022, 0x0017, 0x0021, 0x0018, 0x0020, 0x0019,
0x001f, 0x001a, 0x001e, 0x001b, 0x001d, 0x001c, 0x001c, 0x001d,
0x001b, 0x001e, 0x001a, 0x001f, 0x0019, 0x0020, 0x0018, 0x0021,
0x0008, 0xffff,
-1 /* end of table */
};

int32_t IMBType[] =
{

2 , /* max bits */
1, /* total subtables */
2,/* subtable sizes */

 1, /* 1-bit codes */
0x0001, 0x0001,
 1, /* 2-bit codes */
0x0001, 0x0011,
-1 /* end of table */
};

int32_t PMBType[] =
{

6 , /* max bits */
2, /* total subtables */
1,5,/* subtable sizes */

 1, /* 1-bit codes */
0x0001, 0x000a,
 1, /* 2-bit codes */
0x0001, 0x0002,
 1, /* 3-bit codes */
0x0001, 0x0008,
 0, /* 4-bit codes */
 3, /* 5-bit codes */
0x0003, 0x0001, 0x0002, 0x001a, 0x0001, 0x0012,
 1, /* 6-bit codes */
0x0001, 0x0011,
-1 /* end of table */
};

int32_t BMBType[] =
{

6 , /* max bits */
2, /* total subtables */
3,3,/* subtable sizes */

 0, /* 1-bit codes */
 2, /* 2-bit codes */
0x0002, 0x000c, 0x0003, 0x000e,
 2, /* 3-bit codes */
0x0002, 0x0004, 0x0003, 0x0006,
 2, /* 4-bit codes */
0x0002, 0x0008, 0x0003, 0x000a,
 2, /* 5-bit codes */
0x0003, 0x0001, 0x0002, 0x001e,
 3, /* 6-bit codes */
0x0003, 0x001a, 0x0002, 0x0016, 0x0001, 0x0011,
-1 /* end of table */
};

int32_t MBPattern[] =
{

9 , /* max bits */
2, /* total subtables */
3,6,/* subtable sizes */

 0, /* 1-bit codes */
 0, /* 2-bit codes */
 1, /* 3-bit codes */
0x0007, 0x003c,
 4, /* 4-bit codes */
0x000d, 0x0004, 0x000c, 0x0008, 0x000b, 0x0010, 0x000a, 0x0020,

 12, /* 5-bit codes */
0x0013, 0x000c, 0x0012, 0x0030, 0x0011, 0x0014, 0x0010, 0x0028,
0x000f, 0x001c, 0x000e, 0x002c, 0x000d, 0x0034, 0x000c, 0x0038,
0x000b, 0x0001, 0x000a, 0x003d, 0x0009, 0x0002, 0x0008, 0x003e,

 4, /* 6-bit codes */
0x000f, 0x0018, 0x000e, 0x0024, 0x000d, 0x0003, 0x000c, 0x003f,

 8, /* 7-bit codes */
0x0017, 0x0005, 0x0016, 0x0009, 0x0015, 0x0011, 0x0014, 0x0021,
0x0013, 0x0006, 0x0012, 0x000a, 0x0011, 0x0012, 0x0010, 0x0022,

 28, /* 8-bit codes */
0x001f, 0x0007, 0x001e, 0x000b, 0x001d, 0x0013, 0x001c, 0x0023,
0x001b, 0x000d, 0x001a, 0x0031, 0x0019, 0x0015, 0x0018, 0x0029,
0x0017, 0x000e, 0x0016, 0x0032, 0x0015, 0x0016, 0x0014, 0x002a,
0x0013, 0x000f, 0x0012, 0x0033, 0x0011, 0x0017, 0x0010, 0x002b,
0x000f, 0x0019, 0x000e, 0x0025, 0x000d, 0x001a, 0x000c, 0x0026,
0x000b, 0x001d, 0x000a, 0x002d, 0x0009, 0x0035, 0x0008, 0x0039,
0x0007, 0x001e, 0x0006, 0x002e, 0x0005, 0x0036, 0x0004, 0x003a,

 7, /* 9-bit codes */
0x0007, 0x001f, 0x0006, 0x002f, 0x0005, 0x0037, 0x0004, 0x003b,
0x0003, 0x001b, 0x0002, 0x0027, 0x0001, 0x0000,
-1 /* end of table */
};

int32_t MotionVector[] =
{

11, /* max bits */
2, /* total subtables */
3,8,/* subtable sizes */

 1, /* 1-bit codes */
0x00000001, 0x00000000,
 0, /* 2-bit codes */
 2, /* 3-bit codes */
0x00000002, 0x00000001, 0x00000003, static_cast<int32_t>(0xffffffff),
 2, /* 4-bit codes */
0x00000002, 0x00000002, 0x00000003, static_cast<int32_t>(0xfffffffe),
 2, /* 5-bit codes */
0x00000002, 0x00000003, 0x00000003, static_cast<int32_t>(0xfffffffd),
 0, /* 6-bit codes */
 2, /* 7-bit codes */
0x00000006, 0x00000004, 0x00000007, static_cast<int32_t>(0xfffffffc),
 6, /* 8-bit codes */
0x0000000a, 0x00000005, 0x0000000b, static_cast<int32_t>(0xfffffffb), 0x00000008, 0x00000006, 0x00000009, static_cast<int32_t>(0xfffffffa),
0x00000006, 0x00000007, 0x00000007, static_cast<int32_t>(0xfffffff9),
 0, /* 9-bit codes */
 6, /* 10-bit codes */
0x00000016, 0x00000008, 0x00000017, static_cast<int32_t>(0xfffffff8), 0x00000014, 0x00000009, 0x00000015, static_cast<int32_t>(0xfffffff7),
0x00000012, 0x0000000a, 0x00000013, static_cast<int32_t>(0xfffffff6),
 12, /* 11-bit codes */
0x00000022, 0x0000000b, 0x00000020, 0x0000000c, 0x0000001e, 0x0000000d, 0x0000001c, 0x0000000e,
0x0000001a, 0x0000000f, 0x00000018, 0x00000010, 0x00000019, static_cast<int32_t>(0xfffffff0), 0x0000001b, static_cast<int32_t>(0xfffffff1),
0x0000001d, static_cast<int32_t>(0xfffffff2), 0x0000001f, static_cast<int32_t>(0xfffffff3), 0x00000021, static_cast<int32_t>(0xfffffff4), 0x00000023, static_cast<int32_t>(0xfffffff5),

-1 /* end of table */
};

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
