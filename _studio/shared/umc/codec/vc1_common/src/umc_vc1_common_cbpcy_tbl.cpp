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

#include "umc_vc1_common_defs.h"
#include "umc_vc1_common_cbpcy_tbl.h"

//VC-1 Table 169: I-Picture CBPCY VLC Table
////////////////////////////////////////////////////////////////////
//CBPCY  VLC Codeword VLC Size    CBPCY    VLC Codeword    VLC Size
//0         1            1        32            6            4
//1         23           6        33            3            9
//2         9            5        34            30           7
//3         5            5        35            28           6
//4         6            5        36            18           7
//5         71           9        37            904          12
//6         32           7        38            68           9
//7         16           7        39            112          9
//8         2            5        40            31           6
//9         124          9        41            574          11
//10        58           7        42            57           8
//11        29           7        43            142          9
//12        2            6        44            1            7
//13        236          9        45            454          11
//14        119          8        46            182          9
//15        0            8        47            69           9
//16        3            5        48            20           6
//17        183          9        49            575          11
//18        44           7        50            125          9
//19        19           7        51            24           9
//20        1            6        52            7            7
//21        360         10        53            455          11
//22        70           8        54            134          9
//23        63           8        55            25           9
//24        30           6        56            21           6
//25        1810        13        57            475          10
//26        181          9        58            2            9
//27        66           8        59            70           9
//28        34           7        60            13           8
//29        453         11        61            1811         13
//30        286         10        62            474          10
//31        135          9        63            361          10
///////////////////////////////////////////////////////////////

const extern int32_t VC1_CBPCY_Ipic[] =
{
 13, /* max bits */
 2,  /* total subtables */
 8, 5 ,/* subtable sizes */

 1, /* 1-bit codes */
     0x01, 0,
 0, /* 2-bit codes */
 0, /* 3-bit codes */
 1, /* 4-bit codes */
     0x06, 32,
 5, /* 5-bit codes */
    0x02, 8, 0x03, 16, 0x05, 3, 0x06, 4, 0x09, 2,

 8, /* 6-bit codes */
    0x01, 20,  0x02, 12,  0x14, 48,  0x15, 56,
    0x17, 1,   0x1c, 35,  0x1e, 24,  0x1f, 40,


 11,/* 7-bit codes */
    0x01, 44,  0x07, 52,  0x10, 7,   0x12, 36,
    0x13, 19,  0x1d, 11,  0x1e, 34,  0x20, 6,
    0x22, 28,  0x2c, 18,  0x3a, 10,

 7, /* 8-bit codes */
    0x00, 15,  0x0d, 60,  0x39, 42,  0x3f, 23,
    0x42, 27,  0x46, 22,  0x77, 14,

 18,/* 9-bit codes */
    0x02, 58,  0x03, 33,  0x18, 51,  0x19, 55,
    0x44, 38,  0x45, 47,  0x46, 59,  0x47, 5,
    0x70, 39,  0x7c, 9,   0x7d, 50,  0x86, 54,
    0x87, 31,  0x8e, 43,  0xb5, 26,  0xb6, 46,
    0xb7, 17,  0xec, 13,

 5, /* 10-bit codes */
    0x11e, 30, 0x168, 21, 0x169, 63, 0x1da, 62, 0x1db, 57,

 5, /* 11-bit codes */
    0x1c5, 29, 0x1c6, 45, 0x1c7, 53, 0x23e, 41, 0x23f, 49,

 1, /* 12-bit codes */
    0x388, 37,

 2, /* 13-bit codes */
    0x712, 25, 0x713, 61,

-1 /* end of table */
};


//VC-1 Table 170: P and B-Picture CBPCY VLC Table 0
//CBPCY     VLC Codeword    VLC Size    CBPCY    VLC Codeword    VLC Size
///////////////////////////////////////////////////////////////
//0         0           13          1           6           13
//32        1           6           33          7           13
//16        1           5           17          54          7
//48        4           6           49          103         8
//8         5           6           9           8           13
//40        1           7           41          9           13
//24        12          7           25          10          13
//56        4           5           57          110         8
//4         13          7           5           11          13
//36        14          7           37          12          13
//20        10          6           21          111         8
//52        11          6           53          56          7
//12        12          6           13          114         8
//44        7           5           45          58          7
//28        13          6           29          115         8
//60        2           3           61          5           3
//2         15          7           3           13          13
//34        1           8           35          7           12
//18        96          8           19          8           12
//50        1           13          51          9           12
//10        49          7           11          10          12
//42        97          8           43          11          12
//26        2           13          27          12          12
//58        100         8           59          30          6
//6           3         13          7           13          12
//38          4         13          39          14          12
//22          5         13          23          15          12
//54        101         8           55          118          8
//14        102         8           15          119          8
//46         52         7           47          62           7
//30         53         7           31          63           7
//62         4          3           63           3           2
///////////////////////////////////////////////////////////////

const extern int32_t VC1_CBPCY_PBpic_tbl0[] =
{
 13, /* max bits */
 2,  /* total subtables */
 8, 5 ,/* subtable sizes */

 0, /* 1-bit codes */
 1, /* 2-bit codes */
    3,63,
 3, /* 3-bit codes */
    2, 60,    4, 62,    5, 61,
 0, /* 4-bit codes */
 3, /* 5-bit codes */
     1, 16,     4, 56,     7, 44,

 8, /* 6-bit codes */
      1, 32,    4, 48,     5, 8 ,      10,  20,
      11, 52,   12, 12,    13, 28,     30,  59,

 13,/* 7-bit codes */
      1, 40,      12, 24,      13, 4 ,      14, 36,
      15, 2,      49, 10,      52, 46,      53, 30,
      54, 17,     56, 53,      58, 45,      62, 47,
      63, 31,

 13, /* 8-bit codes */
     1, 34,     96, 18,     97, 42,     100, 58,
     101, 54,   102, 14,    103, 49,    110, 57,
     111, 21,   114, 13,    115, 29,    118, 55,
     119, 15,

 0,/* 9-bit codes */

 0, /* 10-bit codes */

 0, /* 11-bit codes */

 9, /* 12-bit codes */
        7, 35,   8, 19,     9, 51,     10, 11,
        11, 43,  12, 27,    13, 7,     14, 39,
        15, 23,

 14, /* 13-bit codes */
       1, 50,    2, 26,     3, 6,     4, 38,
       5, 22,    0, 0,      6, 1,     7, 33,
       8, 9,     9, 41,    10, 25,    11, 5,
       12, 37,   13, 3,

-1 /* end of table */
};

//VC-1 Table 171: P and B-Picture CBPCY VLC Table 1
//CBPCY VLC Codeword  VLC Size    CBPCY   VLC Codeword  VLC Size
///////////////////////////////////////////////////////////////
//0         0           14          1           9           13
//32        1           3           33          240          8
//16        2           3           17          10          13
//48        1           5           49          11          13
//8         3           3           9           121          7
//40        1           4           41          122          7
//24        16          5           25          12          13
//56        17          5           57          13          13
//4         5           3           5           14          13
//36        18          5           37          15          13
//20        12          4           21          241          8
//52        19          5           53          246          8
//12        13          4           13          16          13
//44        1           6           45          17          13
//28        28          5           29          124          7
//60        58          6           61          63          6
//2         1           8           3           18          13
//34        1           14          35          19          13
//18        1           13          19          20          13
//50        2           8           51          21          13
//10        3           8           11          22          13
//42        2           13          43          23          13
//26        3           13          27          24          13
//58        236         8           59          25          13
//6         237         8           7           26          13
//38        4           13          39          27          13
//22        5           13          23          28          13
//54        238         8           55          29          13
//14        6           13          15          30          13
//46        7           13          47          31          13
//30        239         8           31          247          8
//62        8           13          63          125          7
///////////////////////////////////////////////////////////////

const extern int32_t VC1_CBPCY_PBpic_tbl1[] =
{
 14, /* max bits */
 2,  /* total subtables */
 8, 6 ,/* subtable sizes */

 0, /* 1-bit codes */
 0, /* 2-bit codes */
 4, /* 3-bit codes */
    1, 32,    2, 16,    3, 8 ,    5, 4 ,

 3, /* 4-bit codes */
    1, 40,    12, 20,   13, 12,

 6, /* 5-bit codes */
    1, 48,    16, 24,    17, 56,    18, 36,
    19, 52,   28, 28,

 3, /* 6-bit codes */
    1, 44,    58, 60,    63, 61,

 4,/* 7-bit codes */
    121, 9,    122, 41,    124, 29,    125, 63,

 11, /* 8-bit codes */
    1, 2,    2, 50,    3, 10,    236, 58,
    237, 6,  238, 54,  239, 30,  240, 33,
    241, 21, 246, 53,  247, 31,

 0,/* 9-bit codes */

 0, /* 10-bit codes */

 0, /* 11-bit codes */

 0, /* 12-bit codes */

 31, /* 13-bit codes */
    1, 18,    2, 42,    3, 26,    4, 38,
    5, 22,    8, 62,    9, 1,     6, 14,
    7, 46,    10, 17,   11, 49,   12, 25,
    13, 57,   14, 5,    15, 37,   16, 13,
    17, 45,   18, 3,    19, 35,   20, 19,
    21, 51,   22, 11,   23, 43,   24, 27,
    25, 59,   26, 7,    27, 39,   28, 23,
    29, 55,   30, 15,   31, 47,

 2, /* 14-bit codes */
    0, 0,
    1, 34,

-1 /* end of table */
};


//VC-1 Table 172: P and B-Picture CBPCY VLC Table 2
//CBPCY  VLC Codeword    VLC Size    CBPCY    VLC Codeword    VLC Size
///////////////////////////////////////////////////////////////
//0         0               13           1          201         8
//32        1               5           33          102         7
//16        2               5           17          412         9
//48        3               5           49          413         9
//8         2               4           9           414         9
//40        3               4           41          54          6
//24        1               6           25          220         8
//56        4               4           57          111         7
//4         5               4           5           221         8
//36        24              6           37          3           13
//20        7               4           21          224         8
//52        13              5           53          113         7
//12        16              5           13          225         8
//44        17              5           45          114         7
//28        9               4           29          230         8
//60        5               3           61          29          5
//2         25              6           3           231         8
//34        1               8           35          415         9
//18        1               10          19          240         8
//50        1               9           51          4           13
//10        2               8           11          241         8
//42        3               8           43          484         9
//26        96              7           27          5           13
//58        194             8           59          243         8
//6         1               13           7          3           12
//38        2               13          39          244         8
//22        98              7           23          245         8
//54        99              7           55          485         9
//14        195             8           15          492         9
//46        200             8           47          493         9
//30        101             7           31          247         8
//62        26              5           63          31          5
///////////////////////////////////////////////////////////////

const extern int32_t VC1_CBPCY_PBpic_tbl2[] =
{
 13, /* max bits */
 2,  /* total subtables */
 8, 5 ,/* subtable sizes */

 0, /* 1-bit codes */
 0, /* 2-bit codes */
 1, /* 3-bit codes */
    5,       60,
 6, /* 4-bit codes */
    2, 8,    3, 40,    4, 56,    5, 4,
    7, 20,   9, 28,
 9, /* 5-bit codes */
    1, 32,   2, 16,    3, 48,    13, 52,
    16, 12,  17, 44,   26, 62,   31, 63,
    29, 61,

 4, /* 6-bit codes */
    1, 24,    24, 36,    25 , 2,    54, 41,

 8,/* 7-bit codes */
    96 , 26,  98 , 22,   99 , 54,   101, 30,
    102, 33,  111, 57,   113, 53,   114, 45,

 19, /* 8-bit codes */
    1, 34,    2, 10,    3, 42,    194, 58,
    195, 14,  200, 46,  201, 1,   220, 25,
    221, 5,   224, 21,  225, 13,  230, 29,
    231, 3,   240, 19,  241, 11,  243, 59,
    244, 39,  245, 23,  247, 31,

 9,/* 9-bit codes */
    1, 50,    412, 17,    413, 49,    414, 9,
    415, 35,  484, 43,    485, 55,    492, 15,
    493, 47,

 1, /* 10-bit codes */
    1, 18,

 0, /* 11-bit codes */

 1, /* 12-bit codes */
    3, 7,

 6, /* 13-bit codes */
    0, 0,    1, 6,    2, 38,    3, 37,
    4, 51,   5, 27,

-1 /* end of table */
};

//VC-1 Table 173: P and B-Picture CBPCY VLC Table 3
//CBPCY  VLC Codeword    VLC Size    CBPCY    VLC Codeword    VLC Size
///////////////////////////////////////////////////////////////
//0         0               9           1           28          9
//32        1               2           33          29          9
//16        1               3           17          30          9
//48        1               9           49          31          9
//8         2               2           9           32          9
//40        2               9           41          33          9
//24        3               9           25          34          9
//56        4               9           57          35          9
//4         3               2           5           36          9
//36        5               9           37          37          9
//20        6               9           21          38          9
//52        7               9           53          39          9
//12        8               9           13          40          9
//44        9               9           45          41          9
//28        10              9           29          42          9
//60        11              9           61          43          9
//2         12              9           3           44          9
//34        13              9           35          45          9
//18        14              9           19          46          9
//50        15              9           51          47          9
//10        16              9           11          48          9
//42        17              9           43          49          9
//26        18              9           27          50          9
//58        19              9           59          51          9
//6         20              9           7           52          9
//38        21              9           39          53          9
//22        22              9           23          54          9
//54        23              9           55          55          9
//14        24              9           15          28          8
//46        25              9           47          29          8
//30        26              9           31          30          8
//62        27              9           63          31          8
///////////////////////////////////////////////////////////////

const extern int32_t VC1_CBPCY_PBpic_tbl3[] =
{
 9, /* max bits */
 2,  /* total subtables */
 3, 6 ,/* subtable sizes */

 0, /* 1-bit codes */
 3, /* 2-bit codes */
    1, 32,    2, 8,    3, 4,

 1, /* 3-bit codes */
    1, 16,

 0, /* 4-bit codes */
 0, /* 5-bit codes */
 0, /* 6-bit codes */
 0,/* 7-bit codes */
 4, /* 8-bit codes */
    28, 15,    29, 47,    30, 31,    31, 63,

 56,/* 9-bit codes */
    0, 0,    1, 48,    2, 40,    3, 24,
    4, 56,   5, 36,    6, 20,    7, 52,
    8, 12,   9, 44,    10, 28,   11, 60,
    12, 2,   13, 34,   14, 18,   15, 50,
    16, 10,  17, 42,   18, 26,   19, 58,
    20, 6,   21, 38,   22, 22,   23, 54,
    24, 14,  25, 46,   26, 30,   27, 62,
    28, 1,   29, 33,   30, 17,   31, 49,
    32, 9,   33, 41,   34, 25,   35, 57,
    36, 5,   37, 37,   38, 21,   39, 53,
    40, 13,  41, 45,   42, 29,   43, 61,
    44, 3,   45, 35,   46, 19,   47, 51,
    48, 11,  49, 43,   50, 27,   51, 59,
    52, 7,   53, 39,   54, 23,   55, 55,

-1 /* end of table */
};
#endif //MFX_ENABLE_VC1_VIDEO_DECODE
