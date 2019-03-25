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
#include "umc_vc1_common_interlaced_cbpcy_tables.h"

//VC-1 Table 125: interlaced CBPCY table 0
//Coded Block VLC Codeword VLC Codeword Coded Block  VLC Codeword VLC Codeword
// Pattern                 Size         Pattern                     Size
//    1          12058      15          33              686         11
//    2          12059      15          34              687         11
//    3           6028      14          35              1506        12
//    4            144       9          36              310         10
//    5            680      11          37              622         11
//    6            681      11          38              623         11
//    7           3015      13          39              765         11
//    8            145       9          40              158          9
//    9            682      11          41              318         10
//    10           683      11          42              319         10
//    11          1504      12          43              383         10
//    12            74       8          44              80           8
//    13           150       9          45              66           8
//    14           151       9          46              67           8
//    15           189       9          47              44           7
//    16           146       9          48              81           8
//    17           684      11          49              164          9
//    18           685      11          50              165          9
//    19          1505      12          51              190          9
//    20           152       9          52              83           8
//    21           306      10          53              68           8
//    22           307      10          54              69           8
//    23           377      10          55              45           7
//    24           308      10          56              84           8
//    25           618      11          57              70           8
//    26           619      11          58              71           8
//    27           764      11          59              46           7
//    28            78       8          60              3            3
//    29            64       8          61              0            3
//    30            65       8          62              1            3
//    31            43       7          63              1            1
//    32           147       9

//VC-1 Table 125: interlaced CBPCY table 0
const extern int32_t VC1_InterlacedCBPCYTable0[] =
{
    15, /* max bits */
    2,  /* total subtables */
    8, 7,/* subtable sizes */

    1, /* 1-bit codes */
        1, 63,
    0, /* 2-bit codes */
    3, /* 3-bit codes */
        3, 60,     0, 61,     1, 62,
    0, /* 4-bit codes */
    0, /* 5-bit codes */
    0, /* 6-bit codes */
    4, /* 7-bit codes */
        43, 31,    44, 47,    45, 55,     46, 59,
    14, /* 8-bit codes */
        74, 12,    78, 28,    64, 29,     65, 30,
        80, 44,    66, 45,    67, 46,     81, 48,
        83, 52,    68, 53,    69, 54,     84, 56,
        70, 57,    71, 58,
    12, /* 9-bit codes */
        144, 4,    145, 8,    150, 13,    151, 14,
        189, 15,   146, 16,   152, 20,    147, 32,
        158, 40,   164, 49,   165, 50,    190, 51,
    8, /* 10-bit codes */
        306, 21,   307, 22,   377, 23,    308, 24,
        310, 36,   318, 41,   319, 42,    383, 43,
    14, /* 11-bit codes */
        680, 5,    681, 6,    682, 9,     683, 10,
        684, 17,   685, 18,   618, 25,    619, 26,
        764, 27,   686, 33,   687, 34,    622, 37,
        623, 38,   765, 39,
    3, /* 12-bit codes */
        1504, 11,  1505, 19,  1506, 35,
    1, /* 13-bit codes */
        3015, 7,
    1, /* 14-bit codes */
        6028, 3,
    2, /* 15-bit codes */
        12058, 1,   12059, 2,
-1 /* end of table */
};


//VC-1 Table 126: interlaced CBPCY table 1
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword VLC Codeword
// Pattern                  Size        Pattern                     Size
//      1       65           7            33            20           7
//      2       66           7            34            21           7
//      3       256          9            35            44           8
//      4       67           7            36            92           8
//      5       136          8            37            93           9
//      6       137          8            38            94           9
//      7       257          9            39            95           9
//      8       69           7            40            38           7
//      9       140          8            41            93           8
//      10      141          8            42            94           8
//      11      258          9            43            95           8
//      12      16           6            44            13           6
//      13      34           7            45            52           7
//      14      35           7            46            53           7
//      15      36           7            47            27           6
//      16      71           7            48            20           6
//      17      16           7            49            39           7
//      18      17           7            50            42           7
//      19      259          9            51            43           7
//      20      37           7            52            14           6
//      21      88           8            53            56           7
//      22      89           8            54            57           7
//      23      90           8            55            29           6
//      24      91           8            56            15           6
//      25      90           9            57            60           7
//      26      91           9            58            61           7
//      27      92           9            59            31           6
//      28      12           6            60            5            3
//      29      48           7            61            9            4
//      30      49           7            62            0            3
//      31      25           6            63            3            2
//      32      9            6

//VC-1 Table 126: interlaced CBPCY table 1
const extern int32_t VC1_InterlacedCBPCYTable1[] =
{
        9, /* max bits */
        1,  /* total subtables */
        9,/* subtable sizes */

        0, /* 1-bit codes */
        1, /* 2-bit codes */
            3, 63,
        2, /* 3-bit codes */
            5, 60,     0, 62,
        1, /* 4-bit codes */
            9, 61,
        0, /* 5-bit codes */
        11, /* 6-bit codes */
            16, 12,   12, 28,   25, 31,    9,  32,
            13, 44,   27, 47,   20, 48,    14, 52,
            29, 55,   15, 56,   31, 59,
        25, /* 7-bit codes */
            65, 1,    66, 2,    67, 4,     69, 8,
            34, 13,   35, 14,   36, 15,    71, 16,
            16, 17,   17, 18,   37, 20,    48, 29,
            49, 30,   20, 33,   21, 34,    38, 40,
            52, 45,   53, 46,   39, 49,    42, 50,
            43, 51,   56, 53,   57, 54,    60, 57,
            61, 58,
        13, /* 8-bit codes */
            136, 5,   137, 6,   140, 9,    141, 10,
            88,  21,  89,  22,  90,  23,   91,  24,
            44,  35,  92,  36,  93,  41,   94,  42,
            95,  43,
        10, /* 9-bit codes */
            256, 3,   257, 7,   258, 11,   259, 19,
            90,  25,  91,  26,  92,  27,   93,  37,
            94,  38,  95,  39,
-1 /* end of table */
};

//VC-1 Table 127: interlaced CBPCY table 2
//Coded Block VLC Codeword VLC Codeword Coded Block  VLC Codeword VLC Codeword
// Pattern                  Size        Pattern                      Size
//     1         50          6             33              234         8
//     2         51          6             34              235         8
//     3         26          5             35              489         9
//     4         38          6             36              74          7
//     5         228         8             37              442         9
//     6         229         8             38              443         9
//     7         486         9             39              475         9
//     8         39          6             40              32          6
//     9         230         8             41              222         8
//     10        231         8             42              223         8
//     11        487         9             43              242         8
//     12        14          5             44              34          6
//     13        99          7             45              85          7
//     14        108         7             46              88          7
//     15        119         7             47              45          6
//     16        40          6             48              15          5
//     17        232         8             49              112         7
//     18        233         8             50              113         7
//     19        488         9             51              120         7
//     20        123         7             52              35          6
//     21        218         8             53              89          7
//     22        219         8             54              92          7
//     23        236         8             55              47          6
//     24        245         8             56              36          6
//     25        440         9             57              93          7
//     26        441         9             58              98          7
//     27        474         9             59              48          6
//     28        33          6             60              2           3
//     29        75          7             61              31          5
//     30        84          7             62              6           4
//     31        43          6             63              0           2
//     32        41          6

//VC-1 Table 127: interlaced CBPCY table 2
const extern int32_t VC1_InterlacedCBPCYTable2[] =
{
        9, /* max bits */
        1,  /* total subtables */
        9,/* subtable sizes */

        0, /* 1-bit codes */
        1, /* 2-bit codes */
            0, 63,
        1, /* 3-bit codes */
            2, 60,
        1, /* 4-bit codes */
            6, 62,
        4, /* 5-bit codes */
            26, 3,    14, 12,    15, 48,     31, 61,
        15, /* 6-bit codes */
            50, 1,    51, 2,     38, 4,      39, 8,
            32, 40,   40, 16,    33, 28,     43, 31,
            41, 32,   34, 44,    45, 47,     35, 52,
            47, 55,   36, 56,    48, 59,
        16, /* 7-bit codes */
            74, 36,   99, 13,    108, 14,    119, 15,
            123, 20,  75, 29,    84, 30,     85, 45,
            88, 46,   112, 49,   113, 50,    120, 51,
            89, 53,   92, 54,    93, 57,     98, 58,
        15, /* 8-bit codes */
            228, 5,    229, 6,   230, 9,     231, 10,
            234, 33,   235, 34,  222, 41,    223, 42,
            232, 17,   233, 18,  218, 21,    219, 22,
            236, 23,   245, 24,  242, 43,
        10, /* 9-bit codes */
            486, 7,    489, 35,  442, 37,    443, 38,
            475, 39,   487, 11,  488, 19,    440, 25,
            441, 26,   474, 27,
-1 /* end of table */
};


//VC-1 Table 128: interlaced CBPCY table 3
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword VLC Codeword
// Pattern                     Size        Pattern                    Size
//   1              40          6            33           499           9
//   2              41          6            34           500           9
//   3              157         8            35           501           9
//   4              0           4            36           17            6
//   5              490         9            37           978          10
//   6              491         9            38           979          10
//   7              492         9            39           305           9
//   8              1           4            40           9             5
//   9              493         9            41           350           9
//   10             494         9            42           351           9
//   11             495         9            43           156           8
//   12             5           4            44           16            5
//   13             240         8            45           168           8
//   14             241         8            46           169           8
//   15             59          7            47           56            7
//   16             2           4            48           6             4
//   17             496         9            49           242           8
//   18             497         9            50           243           8
//   19             498         9            51           77            7
//   20             63          6            52           17            5
//   21             348         9            53           170           8
//   22             349         9            54           171           8
//   23             153         8            55           57            7
//   24             16          6            56           18            5
//   25             976        10            57           172           8
//   26             977        10            58           173           8
//   27             304         9            59           58            7
//   28             15          5            60           6             3
//   29             158         8            61           22            5
//   30             159         8            62           23            5
//   31             251         8            63           14            4
//   32             3           4

const extern int32_t VC1_InterlacedCBPCYTable3[] =
{
    10, /* max bits */
    1,  /* total subtables */
    10,/* subtable sizes */

    0, /* 1-bit codes */
     0, /* 2-bit codes */
    1, /* 3-bit codes */
        6, 60,
    7, /* 4-bit codes */
        0, 4,    1, 8,     5, 12,     2, 16,
        6, 48,   14, 63,   3, 32,
    7, /* 5-bit codes */
        9, 40,   16, 44,   17, 52,    18, 56,
        15, 28,  22, 61,   23, 62,
    5, /* 6-bit codes */
        40, 1,   41, 2,    17, 36,    63, 20,
        16, 24,
    5, /* 7-bit codes */
        59, 15,  56, 47,   77, 51,    57, 55,
        58, 59,
    16, /* 8-bit codes */
        157, 3,  240, 13,  241, 14,   156, 43,
        168, 45, 169, 46,  242, 49,   243, 50,
        170, 53, 171, 54,  153, 23,   158, 29,
        159, 30, 251, 31,  172, 57,   173, 58,
    18, /* 9-bit codes */
        499, 33,  500, 34,   501, 35,   490, 5,
        491, 6,   492, 7,    305, 39,   493, 9,
        350, 41,  494, 10,   351, 42,   495, 11,
        496, 17,  497, 18,   498, 19,   348, 21,
        349, 22,  304, 27,
    4, /* 10-bit codes */
        978, 37,  979, 38,   976, 25,   977, 26,
-1 /* end of table */
};


//VC-1 Table 129: interlaced CBPCY table 4
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword VLC Codeword
// Pattern                      Size    Pattern                     Size
//   1             60            6       33           105             7
//   2             61            6       34           108             7
//   3             31            5       35           5               7
//   4             10            5       36           96              7
//   5             97            7       37           26              8
//   6             98            7       38           27              8
//   7              2            7       39           53              8
//   8             11            5       40           19              6
//   9             99            7       41           14              7
//   10           100            7       42           15              7
//   11             3            7       43           21              7
//   12             7            5       44           45              6
//   13             3            6       45           109             7
//   14             4            6       46           110             7
//   15            11            6       47           56              6
//   16            12            5       48           8               5
//   17           101            7       49           8               6
//   18           102            7       50           9               6
//   19             4            7       51           12              6
//   20            18            6       52           46              6
//   21            10            7       53           111             7
//   22            11            7       54           114             7
//   23            20            7       55           58              6
//   24            27            7       56           47              6
//   25            24            8       57           115             7
//   26            25            8       58           0               6
//   27            52            8       59           59              6
//   28            44            6       60           7               4
//   29           103            7       61           20              5
//   30           104            7       62           21              5
//   31            53            6       63           4               3
//   32            13            5

const extern int32_t VC1_InterlacedCBPCYTable4[] =
{
    8, /* max bits */
    1,  /* total subtables */
    8,/* subtable sizes */

    0, /* 1-bit codes */
    0, /* 2-bit codes */
    1, /* 3-bit codes */
        4, 63,
    1, /* 4-bit codes */
        7, 60,
    9, /* 5-bit codes */
        31, 3,    10, 4,    11, 8,    7, 12,
        12, 16,   8, 48,    21, 62,   13, 32,
        20, 61,
    19, /* 6-bit codes */
        60, 1,    61, 2,    19, 40,   45, 44,
        3, 13,    4, 14,    11, 15,   56, 47,
        8, 49,    9, 50,    12, 51,   18, 20,
        46, 52,   58, 55,   47, 56,   0,  58,
        59, 59,   44, 28,   53, 31,
    27, /* 7-bit codes */
        105, 33,  108, 34,  5, 35,    96, 36,
        97, 5,    98, 6,    2, 7,     99, 9,
        14, 41,   100, 10,  15, 42,   3, 11,
        21, 43,   109, 45,  110, 46,  101, 17,
        102, 18,  4, 19,    10, 21,   111, 53,
        11, 22,   114, 54,  20, 23,   27, 24,
        115, 57,  103, 29,  104, 30,
    6, /* 8-bit codes */
        26, 37,   27, 38,   53, 39,   24, 25,
        25, 26,   52, 27,
-1 /* end of table */
};


//VC-1 Table 130: interlaced CBPCY table 5
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword VLC Codeword
// Pattern                  Size        Pattern                     Size
//   1           56          6            33            154           8
//   2           57          6            34            155           8
//   3          157          8            35            156           8
//   4           10          4            36             25           6
//   5          145          8            37            974          10
//   6          146          8            38            975          10
//   7          147          8            39            215           9
//   8           11          4            40              9           5
//   9          148          8            41            488           9
//   10         149          8            42            489           9
//   11         150          8            43            144           8
//   12           3          4            44             15           5
//   13         238          8            45            232           8
//   14         239          8            46            233           8
//   15          54          7            47            246           8
//   16          12          4            48              5           4
//   17         151          8            49            240           8
//   18         152          8            50            241           8
//   19         153          8            51             55           7
//   20           8          5            52             16           5
//   21         484          9            53            234           8
//   22         485          9            54            235           8
//   23         106          8            55            247           8
//   24          24          6            56             17           5
//   25         972         10            57            236           8
//   26         973         10            58            237           8
//   27         214          9            59             52           7
//   28          14          5            60              0           3
//   29         158          8            61             62           6
//   30         159          8            62             63           6
//   31         245          8            63              2           4
//   32          13          4

const extern int32_t VC1_InterlacedCBPCYTable5[] =
{
    10, /* max bits */
    1,  /* total subtables */
    10,/* subtable sizes */

    0, /* 1-bit codes */
     0, /* 2-bit codes */
    1, /* 3-bit codes */
        0, 60,
    7, /* 4-bit codes */
        10, 4,    2, 63,    13, 32,    11,    8,
        3, 12,    12, 16,   5, 48,
    6, /* 5-bit codes */
        9, 40,    15, 44,   8, 20,     16, 52,
        17, 56,   14, 28,
    6, /* 6-bit codes */
        56, 1,    57, 2,    63, 62,    62, 61,
        24, 24,   25, 36,
    3, /* 7-bit codes */
        54, 15,   55, 51,   52, 59,
    30, /* 8-bit codes */
        154, 33,  155, 34,  157, 3,    156, 35,
        145, 5,   146, 6,   147, 7,    148, 9,
        149, 10,  150, 11,  144, 43,   238, 13,
        232, 45,  239, 14,  233, 46,   246, 47,
        151, 17,  240, 49,  152, 18,   241, 50,
        235, 54,  106, 23,  247, 55,   234, 53,
        158, 29,  159, 30,  245, 31,   153, 19,
        237, 58,  236, 57,
    6, /* 9-bit codes */
        215, 39,  488, 41,  484, 21,   485, 22,
        489, 42,  214, 27,
    4, /* 10-bit codes */
        974, 37,  975, 38,  972, 25,   973, 26,

-1 /* end of table */
};

//VC-1 Table 131: interlaced CBPCY table 6
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword  VLC Codeword
// Pattern                   Size        Pattern                     Size
//   1           60            6            33         229             8
//   2           61            6            34         230             8
//   3          463            9            35         128             8
//   4            0            3            36         46              6
//   5          191            8            37         2021            11
//   6          224            8            38         2022            11
//   7          508            9            39         2023            11
//   8            1            3            40         22              5
//   9          225            8            41         1012            10
//   10         226            8            42         1013            10
//   11         509            9            43         1014            10
//   12           9            4            44         25              5
//   13         497            9            45         258             9
//   14         498            9            46         259             9
//   15         499            9            47         260             9
//   16           2            3            48         10              4
//   17         227            8            49         500             9
//   18         228            8            50         501             9
//   19         510            9            51         502             9
//   20          17            5            52         26              5
//   21        1006           10            53         261             9
//   22        1007           10            54         262             9
//   23        1008           10            55         263             9
//   24          33            6            56         27              5
//   25        2018           11            57         376             9
//   26        2019           11            58         377             9
//   27        2020           11            59         462             9
//   28          24            5            60         29              5
//   29        1015           10            61         189             8
//   30        1022           10            62         190             8
//   31        1023           10            63         496             9
//   32           3            3

const extern int32_t VC1_InterlacedCBPCYTable6[] =
{
    11, /* max bits */
    1,  /* total subtables */
    11,/* subtable sizes */

    0, /* 1-bit codes */
     0, /* 2-bit codes */
    4, /* 3-bit codes */
        0,4,   1,8,    2,16,   3,32,
    2, /* 4-bit codes */
        9,12,  10,48,
    7, /* 5-bit codes */
        17,20,   24,28,   22,40,    25,44,
        26,52,   27,56,   29,60,
    4, /* 6-bit codes */
        60,1,    61,2,    33,24,    46,36,
    0, /* 7-bit codes */
    11, /* 8-bit codes */
        191,5,   224,6,   225,9,    226,10,
        227,17,  228,18,  229,33,   230,34,
        128,35,  189,61,  190,62,
    20, /* 9-bit codes */
        463,3,   508,7,   509,11,   497,13,
        498,14,  499,15,  510,19,   258,45,
        259,46,  260,47,  500,49,   501,50,
        502,51,  261,53,  262,54,   263,55,
        376,57,  377,58,  462,59,
        496,63,
    9, /* 10-bit codes */
        1006,21,  1007,22,  1008,23,  1015,29,
        1022,30,  1023,31,  1012,41,  1013,42,
        1014,43,
    6, /* 11-bit codes */
        2018,25,  2019,26,  2020,27,  2021,37,
        2022,38,  2023,39,
-1 /* end of table */
};

//VC-1 Table 132: interlaced CBPCY table 7
//Coded Block VLC Codeword VLC Codeword Coded Block VLC Codeword VLC Codeword
// Pattern                   Size        Pattern                     Size
//   1            3            6            33          52             7
//   2            4            6            34          53             7
//   3          438           10            35          17             7
//   4            4            3            36          22             6
//   5           46            7            37         105             10
//   6           47            7            38         106             10
//   7           14            7            39         107             10
//   8            5            3            40          10             5
//   9           48            7            41          54             9
//   10          49            7            42          55             9
//   11          15            7            43         216             9
//   12           3            4            44          30             6
//   13          10            8            45         442             10
//   14          11            8            46         443             10
//   15          20            8            47         444             10
//   16           6            3            48           4             4
//   17          50            7            49          21             8
//   18          51            7            50          22             8
//   19          16            7            51          23             8
//   20           5            5            52          31             6
//   21          48            9            53         445             10
//   22          49            9            54         446             10
//   23          50            9            55         447             10
//   24           9            6            56           0             5
//   25         102           10            57          16             9
//   26         103           10            58          17             9
//   27         104           10            59          18             9
//   28          29            6            60          28             6
//   29         439           10            61         217             9
//   30         440           10            62         218             9
//   31         441           10            63          19             9
//   32           7            3

const extern int32_t VC1_InterlacedCBPCYTable7[] =
{
    10, /* max bits */
    1,  /* total subtables */
    10,/* subtable sizes */

    0, /* 1-bit codes */
     0, /* 2-bit codes */
    4, /* 3-bit codes */
        4,4,   5,8,    6,16,    7,32,
    2, /* 4-bit codes */
        3,12,  4,48,
    3, /* 5-bit codes */
        5,20,  10,40,  0,56,
    8, /* 6-bit codes */
        3,1,   4,2,    9,24,    29,28,
        22,36, 30,44,  31,52,   28,60,
    12, /* 7-bit codes */
        46,5,  47,6,   14,7,    48,9,
        49,10, 15,11,  50,17,   51,18,
        16,19, 52,33,  53,34,   17,35,
    6, /* 8-bit codes */
        10,13, 11,14,  20,15,   21,49,
        22,50, 23,51,
    12, /* 9-bit codes */
        48,21, 49,22,  50,23,   54,41,
        55,42, 216,43, 16,57,   17,58,
        18,59, 217,61, 218,62,  19,63,
    16, /* 10-bit codes */
        438,3,   102,25,   103,26,   104,27,
        439,29,  440,30,   441,31,   105,37,
        106,38,  107,39,   442,45,   443,46,
        444,47,  445,53,    446,54,  447,55,

-1 /* end of table */
};

#endif //MFX_ENABLE_VC1_VIDEO_DECODE