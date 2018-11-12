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
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_COEFF_TOKEN_MAP_H__
#define __UMC_H264_DEC_COEFF_TOKEN_MAP_H__

namespace UMC
{

//Table 9 5 - coeff_token mapping to TotalCoeff( coeff_token )
// and TrailingOnes( coeff_token ), 0  <=  nC  <  2, sorted by code len

/*
0    0    1

1    1    01

2    2    001

3    3    0001 1

3    4    0000 11
1    2    0001 00
0    1    0001 01

3    5    0000 100
2    3    0000 101

3    6    0000 0100
2    4    0000 0101
1    3    0000 0110
0    2    0000 0111

3    7    0000 0010 0
2    5    0000 0010 1
1    4    0000 0011 0
0    3    0000 0011 1

3    8    0000 0001 00
2    6    0000 0001 01
1    5    0000 0001 10
0    4    0000 0001 11

3    9    0000 0000 100
2    7    0000 0000 101
1    6    0000 0000 110
0    5    0000 0000 111

0    8    0000 0000 0100 0
2    9    0000 0000 0100 1
1    8    0000 0000 0101 0
0    7    0000 0000 0101 1
3    10    0000 0000 0110 0
2    8    0000 0000 0110 1
1    7    0000 0000 0111 0
0    6    0000 0000 0111 1

3    12    0000 0000 0010 00
2    11    0000 0000 0010 01
1    10    0000 0000 0010 10
0    10    0000 0000 0010 11
3    11    0000 0000 0011 00
2    10    0000 0000 0011 01
1    9    0000 0000 0011 10
0    9    0000 0000 0011 11

1    13    0000 0000 0000 001
3    14    0000 0000 0001 000
2    13    0000 0000 0001 001
1    12    0000 0000 0001 010
0    12    0000 0000 0001 011
3    13    0000 0000 0001 100
2    12    0000 0000 0001 101
1    11    0000 0000 0001 110
0    11    0000 0000 0001 111

0    16    0000 0000 0000 0100
2    16    0000 0000 0000 0101
1    16    0000 0000 0000 0110
0    15    0000 0000 0000 0111
3    16    0000 0000 0000 1000
2    15    0000 0000 0000 1001
1    15    0000 0000 0000 1010
0    14    0000 0000 0000 1011
3    15    0000 0000 0000 1100
2    14    0000 0000 0000 1101
1    14    0000 0000 0000 1110
0    13    0000 0000 0000 1111

*/

static
int32_t coeff_token_map_02[] =
{
16, /* max bits */
3,  /* total subtables */
6,3,7,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 0, 0,

1, /* 2-bit codes */
0x0001, 1, 1,

1, /* 3-bit codes */
0x0001, 2, 2,

0, /* 4-bit codes */

1, /* 5-bit codes */
0x0003, 3, 3,

3, /* 6-bit codes */
0x0003, 3, 4, 0x0004, 1, 2, 0x0005, 0, 1,

2, /* 7-bit codes */
0x0004, 3, 5, 0x0005, 2, 3,

4, /* 8-bit codes */
0x0004, 3, 6, 0x0005, 2, 4, 0x0006, 1, 3, 0x0007, 0, 2,

4, /* 9-bit codes */
0x0004, 3, 7, 0x0005, 2, 5, 0x0006, 1, 4, 0x0007, 0, 3,

4, /* 10-bit codes */
0x0004, 3, 8, 0x0005, 2, 6, 0x0006, 1, 5, 0x0007, 0, 4,

4, /* 11-bit codes */
0x0004, 3, 9, 0x0005, 2, 7, 0x0006, 1, 6, 0x0007, 0, 5,

0, /* 12-bit codes */

8, /* 13-bit codes */
0x0008, 0, 8, 0x0009, 2, 9, 0x000a, 1, 8, 0x000b, 0, 7,
0x000c, 3, 10,0x000d, 2, 8, 0x000e, 1, 7, 0x000f, 0, 6,

8, /* 14-bit codes */
0x0008, 3, 12, 0x0009, 2, 11, 0x000a, 1, 10, 0x000b, 0, 10,
0x000c, 3, 11, 0x000d, 2, 10, 0x000e, 1, 9, 0x000f, 0, 9,

9, /* 15-bit codes */
0x0001, 1, 13, 0x0008, 3, 14, 0x0009, 2, 13, 0x000a, 1, 12,
0x000b, 0, 12, 0x000c, 3, 13, 0x000d, 2, 12, 0x000e, 1, 11,
0x000f, 0, 11,

12, /* 16-bit codes */
0x0004, 0, 16, 0x0005, 2, 16, 0x0006, 1, 16, 0x0007, 0, 15,
0x0008, 3, 16, 0x0009, 2, 15, 0x000a, 1, 15, 0x000b, 0, 14,
0x000c, 3, 15, 0x000d, 2, 14, 0x000e, 1, 14, 0x000f, 0, 13,

-1
};
/*
#undef OFF
#define OFF 120//60//

#undef SZCF
#define SZCF 4//2//

#undef SHIFT1
#define SHIFT1 16//11//

#undef SHIFT2
#define SHIFT2 8//5//

#undef TABLE_TYPE
#define TABLE_TYPE int32_t //uint16_t
*/

//Table 9 5 - coeff_token mapping to TotalCoeff( coeff_token )
// and TrailingOnes( coeff_token ), 2  <=  nC  <  4, sorted by code len
/*
1    1    10
0    0    11

2    2    011

3    4    0100
3    3    0101

3    5    0011 0
1    2    0011 1

3    7    0001 00
2    4    0001 01
1    4    0001 10
0    2    0001 11
3    6    0010 00
2    3    0010 01
1    3    0010 10
0    1    0010 11

3    8    0000 100
2    5    0000 101
1    5    0000 110
0    3    0000 111

0    5    0000 0100
2    6    0000 0101
1    6    0000 0110
0    4    0000 0111

3    9    0000 0010 0
2    7    0000 0010 1
1    7    0000 0011 0
0    6    0000 0011 1

3    11    0000 0001 000
2    9    0000 0001 001
1    9    0000 0001 010
0    8    0000 0001 011
3    10    0000 0001 100
2    8    0000 0001 101
1    8    0000 0001 110
0    7    0000 0001 111

0    11    0000 0000 1000
2    11    0000 0000 1001
1    11    0000 0000 1010
0    10    0000 0000 1011
3    12    0000 0000 1100
2    10    0000 0000 1101
1    10    0000 0000 1110
0    9    0000 0000 1111

3    15    0000 0000 0000 1
2    14    0000 0000 0011 0
0    14    0000 0000 0011 1
3    14    0000 0000 0100 0
2    13    0000 0000 0100 1
1    13    0000 0000 0101 0
0    13    0000 0000 0101 1
3    13    0000 0000 0110 0
2    12    0000 0000 0110 1
1    12    0000 0000 0111 0
0    12    0000 0000 0111 1

3    16    0000 0000 0001 00
2    16    0000 0000 0001 01
1    16    0000 0000 0001 10
0    16    0000 0000 0001 11
1    15    0000 0000 0010 00
0    15    0000 0000 0010 01
2    15    0000 0000 0010 10
1    14    0000 0000 0010 11
*/

static
int32_t coeff_token_map_24[] =
{

14, /* max bits */
2,  /* total subtables */
7,7,/* subtable sizes */

0, /* 1-bit codes */

2, /* 2-bit codes */
0x0002, 1, 1, 0x0003, 0, 0,

1, /* 3-bit codes */
0x0003, 2, 2,

2, /* 4-bit codes */
0x0004, 3, 4, 0x0005, 3, 3,

2, /* 5-bit codes */
0x0006, 3, 5, 0x0007, 1, 2,

8, /* 6-bit codes */
0x0004, 3, 7, 0x0005, 2, 4, 0x0006, 1, 4, 0x0007, 0, 2,
0x0008, 3, 6, 0x0009, 2, 3, 0x000a, 1, 3, 0x000b, 0, 1,

4, /* 7-bit codes */
0x0004, 3, 8, 0x0005, 2, 5, 0x0006, 1, 5, 0x0007, 0, 3,

4, /* 8-bit codes */
0x0004, 0, 5, 0x0005, 2, 6, 0x0006, 1, 6, 0x0007, 0, 4,

4, /* 9-bit codes */
0x0004, 3, 9, 0x0005, 2, 7, 0x0006, 1, 7, 0x0007, 0, 6,

0, /* 10-bit codes */

8, /* 11-bit codes */
0x0008, 3, 11, 0x0009, 2, 9, 0x000a, 1, 9,  0x000b, 0, 8,
0x000c, 3, 10, 0x000d, 2, 8, 0x000e, 1, 8,  0x000f, 0, 7,

8, /* 12-bit codes */
0x0008, 0, 11, 0x0009, 2, 11, 0x000a, 1, 11, 0x000b, 0, 10,
0x000c, 3, 12, 0x000d, 2, 10, 0x000e, 1, 10, 0x000f, 0, 9,

11, /* 13-bit codes */
0x0001, 3, 15, 0x0006, 2, 14, 0x0007, 0, 14, 0x0008, 3, 14,
0x0009, 2, 13, 0x000a, 1, 13, 0x000b, 0, 13, 0x000c, 3, 13,
0x000d, 2, 12, 0x000e, 1, 12, 0x000f, 0, 12,

8, /* 14-bit codes */
0x0004, 3, 16, 0x0005, 2, 16, 0x0006, 1, 16, 0x0007, 0, 16,
0x0008, 1, 15, 0x0009, 0, 15, 0x000a, 2, 15, 0x000b, 1, 14,

-1
};

//Table 9 5 - coeff_token mapping to TotalCoeff( coeff_token )
// and TrailingOnes( coeff_token ), 4  <=  nC  <  8, sorted by code len
/*
3    7    1000
3    6    1001
3    5    1010
3    4    1011
3    3    1100
2    2    1101
1    1    1110
0    0    1111

1    5    0100 0
2    5    0100 1
1    4    0101 0
2    4    0101 1
1    3    0110 0
3    8    0110 1
2    3    0111 0
1    2    0111 1

0    3    0010 00
2    7    0010 01
1    7    0010 10
0    2    0010 11
3    9    0011 00
2    6    0011 01
1    6    0011 10
0    1    0011 11

0    7    0001 000
0    6    0001 001
2    9    0001 010
0    5    0001 011
3    10    0001 100
2    8    0001 101
1    8    0001 110
0    4    0001 111

3    12    0000 1000
2    11    0000 1001
1    10    0000 1010
0    9    0000 1011
3    11    0000 1100
2    10    0000 1101
1    9    0000 1110
0    8    0000 1111

1    13    0000 0011 1
0    12    0000 0100 0
2    13    0000 0100 1
1    12    0000 0101 0
0    11    0000 0101 1
3    13    0000 0110 0
2    12    0000 0110 1
1    11    0000 0111 0
0    10    0000 0111 1

0    16    0000 0000 01
3    16    0000 0000 10
2    16    0000 0000 11
1    16    0000 0001 00
0    15    0000 0001 01
3    15    0000 0001 10
2    15    0000 0001 11
1    15    0000 0010 00
0    14    0000 0010 01
3    14    0000 0010 10
2    14    0000 0010 11
1    14    0000 0011 00
0    13    0000 0011 01
*/

static
int32_t coeff_token_map_48[] =
{

10, /* max bits */
2,  /* total subtables */
7,3,/* subtable sizes */

0, /* 1-bit codes */
0, /* 2-bit codes */
0, /* 3-bit codes */
8, /* 4-bit codes */
0x0008, 3, 7, 0x0009, 3, 6, 0x000a, 3, 5, 0x000b, 3, 4,
0x000c, 3, 3, 0x000d, 2, 2, 0x000e, 1, 1, 0x000f, 0, 0,

8, /* 5-bit codes */
0x0008, 1, 5, 0x0009, 2, 5, 0x000a, 1, 4, 0x000b, 2, 4,
0x000c, 1, 3, 0x000d, 3, 8, 0x000e, 2, 3, 0x000f, 1, 2,

8, /* 6-bit codes */
0x0008, 0, 3, 0x0009, 2, 7, 0x000a, 1, 7, 0x000b, 0, 2,
0x000c, 3, 9, 0x000d, 2, 6, 0x000e, 1, 6, 0x000f, 0, 1,

8, /* 7-bit codes */
0x0008, 0, 7, 0x0009, 0, 6, 0x000a, 2, 9, 0x000b, 0, 5,
0x000c, 3, 10,0x000d, 2, 8, 0x000e, 1, 8, 0x000f, 0, 4,

8, /* 8-bit codes */
0x0008, 3, 12, 0x0009, 2, 11, 0x000a, 1, 10, 0x000b, 0, 9,
0x000c, 3, 11, 0x000d, 2, 10, 0x000e, 1, 9,  0x000f, 0, 8,

9, /* 9-bit codes */
0x0007, 1, 13, 0x0008, 0, 12, 0x0009, 2, 13, 0x000a, 1, 12,
0x000b, 0, 11, 0x000c, 3, 13, 0x000d, 2, 12, 0x000e, 1, 11,
0x000f, 0, 10,

13, /* 10-bit codes */
0x0001, 0, 16, 0x0002, 3, 16, 0x0003, 2, 16, 0x0004, 1, 16,
0x0005, 0, 15, 0x0006, 3, 15, 0x0007, 2, 15, 0x0008, 1, 15,
0x0009, 0, 14, 0x000a, 3, 14, 0x000b, 2, 14, 0x000c, 1, 14,
0x000d, 0, 13,

-1
};

//Table 9 5 - coeff_token mapping to TotalCoeff( coeff_token )
// and TrailingOnes( coeff_token ),  nC  = -1, sorted by code len
/*
1    1    1

0    0    01

2    2    001

0    4    0000 10
0    3    0000 11
0    2    0001 00
3    3    0001 01
1    2    0001 10
0    1    0001 11

3    4    0000 000
2    3    0000 010
1    3    0000 011

2    4    0000 0010
1    4    0000 0011
*/

static
int32_t coeff_token_map_cr[] =
{

8, /* max bits */
2,  /* total subtables */
6,2,/* subtable sizes */

1, /* 1-bit codes */
0x0001, 1, 1,

1, /* 2-bit codes */
0x0001, 0, 0,

1, /* 3-bit codes */
0x0001, 2, 2,

0, /* 4-bit codes */
0, /* 5-bit codes */

6, /* 6-bit codes */
0x0002, 0, 4, 0x0003, 0, 3, 0x0004, 0, 2, 0x0005, 3, 3,
0x0006, 1, 2, 0x0007, 0, 1,

3, /* 7-bit codes */
0x0000, 3, 4, 0x0002, 2, 3, 0x0003, 1, 3,

2, /* 8-bit codes */
0x0002, 2, 4, 0x0003, 1, 4,

-1
};

static
int32_t coeff_token_map_cr2[] =
{
    13, /* max bits */
    2,  /* total subtables */
    7,6,/* subtable sizes */

    1, /* 1-bit codes */
    0x0001, 0, 0,

    1, /* 2-bit codes */
    0x0001, 1, 1,

    1, /* 3-bit codes */
    0x0001, 2, 2,

    0, /* 4-bit codes */
    1, /* 5-bit codes */
    0x0001, 3, 3,

    1, /* 6-bit codes */
    0x0001, 3, 4,

    8, /* 7-bit codes */
    0x0008, 3, 6, 0x0009, 3, 5, 0x000A, 2, 4,
    0x000B, 2, 3, 0x000C, 1, 3, 0x000D, 1, 2,
    0x000E, 0, 2, 0x000F, 0, 1,

    0, /* 8-bit codes */
    4, /* 9-bit codes */
    0x0004, 2, 5, 0x0005, 1, 4, 0x0006, 0, 4,
    0x0007, 0, 3,
    4, /* 10-bit codes */
    0x0004, 3, 7, 0x0005, 2, 6, 0x0006, 1, 5,
    0x0007, 0, 5,
    4, /* 11-bit codes */
    0x0004, 3, 8, 0x0005, 2, 7, 0x0006, 1, 6,
    0x0007, 0, 6,
    4, /* 12-bit codes */
    0x0004, 2, 8, 0x0005, 1, 8, 0x0006, 1, 7,
    0x0007, 0, 7,
    1, /* 13-bit codes */
    0x0007, 0, 8,

    -1
};

} // namespace UMC

#endif //__UMC_H264_DEC_COEFF_TOKEN_MAP_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
