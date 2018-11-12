// Copyright (c) 2002-2018 Intel Corporation
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
#if defined (UMC_ENABLE_MP3_AUDIO_ENCODER)

#include "mp3enc_own.h"

VM_ALIGN32_DECL(const scalefac_struct) mp3enc_sfBandIndex[2][3] = {
  {
    { /* 22.05 kHz */
      {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
      {0,4,8,12,18,24,32,42,56,74,100,132,174,192},
      {0,4,8,12,16,20,24,28,32,36,42,48,54,60,66,72,80,88,96,106,116,126,140,154,168,
       186,204,222,248,274,300,332,364,396,438,480,522,540,558,576}
    },
    { /* 24 kHz */
      {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278,332,394,464,540,576},
      {0,4,8,12,18,26,36,48,62,80,104,136,180,192},
      {0,4,8,12,16,20,24,28,32,36,42,48,54,62,70,78,88,98,108,120,132,144,158,172,186,
       204,222,240,264,288,312,344,376,408,452,496,540,552,564,576}
    },
    { /* 16 kHz */
      {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
      {0,4,8,12,18,26,36,48,62,80,104,134,174,192},
      {0,4,8,12,16,20,24,28,32,36,42,48,54,62,70,78,88,98,108,120,132,144,158,172,186,
       204,222,240,264,288,312,342,372,402,442,482,522,540,558,576}
    }
  },
  {
    { /* 44.1 kHz */
      {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
      {0,4,8,12,16,22,30,40,52,66,84,106,136,192},
      {0,4,8,12,16,20,24,28,32,36,40,44,48,54,60,66,74,82,90,100,110,120,132,144,156,
       170,184,198,216,234,252,274,296,318,348,378,408,464,520,576}
    },
    { /* 48 kHz */
      {0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
      {0,4,8,12,16,22,28,38,50,64,80,100,126,192},
      {0,4,8,12,16,20,24,28,32,36,40,44,48,54,60,66,72,78,84,94,104,114,126,138,150,
       164,178,192,208,224,240,260,280,300,326,352,378,444,510,576}
    },
    { /* 32 kHz */
      {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
      {0,4,8,12,16,22,30,42,58,78,104,138,180,192},
      {0,4,8,12,16,20,24,28,32,36,40,44,48,54,60,66,74,82,90,102,114,126,142,158,174,
       194,214,234,260,286,312,346,380,414,456,498,540,552,564,576}
    }
  }
};

VM_ALIGN32_DECL(const Ipp32s) mp3enc_slen1_tab[16]      = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
VM_ALIGN32_DECL(const Ipp32s) mp3enc_slen2_tab[16]      = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };
VM_ALIGN32_DECL(const Ipp32s) mp3enc_pretab[21]         = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 3, 2};

VM_ALIGN32_DECL(const Ipp32s) mp3enc_region01_table[23][2] =
{
  {0, 0},  {0, 0},
  {0, 0},  {0, 0},
  {0, 0},  {0, 1},
  {1, 1},  {1, 1},
  {1, 2},  {2, 2},
  {2, 3},  {2, 3},
  {3, 4},  {3, 4},
  {3, 4},  {4, 5},
  {4, 5},  {4, 6},
  {5, 6},  {5, 6},
  {5, 7},  {6, 7},
  {6, 7},
};

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_XXX
