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
#if defined (UMC_ENABLE_AAC_AUDIO_DECODER)

#include "aaccmn_huff.h"

static Ipp32u sf_table[] =
{
  0,((32)<<8)+29,((32+4)<<8)+29,((32+8)<<8)+29,
  ((32+ 12)<<8) + 28,((32+ 20)<<8) + 28,((32+ 28)<<8) + 28,((32+ 36)<<8) + 28,
  ((32+ 44)<<8) + 28,((32+ 52)<<8) + 28,((32+ 60)<<8) + 28,((32+ 68)<<8) + 28,
    ((32+ 76)<<8) + 27,((32+ 92)<<8) + 26,((32+124)<<8) + 27,((32+140)<<8) + 28,
  ((32+148)<<8) + 29,((32+152)<<8) + 30,((32+154)<<8) + 31,((32+155)<<8) + 31,

  ((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,
  ((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,
  ((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,((32+155)<<8) + 31,

  ///  1st :   0
  (( 59<<8)+ 3),(( 59<<8)+ 3),(( 61<<8)+ 4),(( 58<<8)+ 4),
    ///  2nd :   4
  (( 62<<8)+ 4),(( 62<<8)+ 4),(( 57<<8)+ 5),(( 63<<8)+ 5),
  ///  3rd :   8
  (( 56<<8)+ 6),(( 64<<8)+ 6),(( 55<<8)+ 6),(( 65<<8)+ 6),
    ///  4th :  12
  (( 66<<8)+ 7),(( 66<<8)+ 7),(( 54<<8)+ 7),(( 54<<8)+ 7),
  (( 67<<8)+ 7),(( 67<<8)+ 7),(( 53<<8)+ 8),(( 68<<8)+ 8),
    ///  5th :  20
  (( 52<<8)+ 8),(( 52<<8)+ 8),(( 69<<8)+ 8),(( 69<<8)+ 8),
  (( 51<<8)+ 8),(( 51<<8)+ 8),(( 70<<8)+ 9),(( 50<<8)+ 9),
    ///  6th :  28
  (( 49<<8)+ 9),(( 49<<8)+ 9),(( 71<<8)+ 9),(( 71<<8)+ 9),
  (( 72<<8)+10),(( 48<<8)+10),(( 73<<8)+10),(( 47<<8)+10),
  ///  7th :  36
  (( 74<<8)+10),(( 74<<8)+10),(( 46<<8)+10),(( 46<<8)+10),
  (( 76<<8)+11),(( 75<<8)+11),(( 77<<8)+11),(( 78<<8)+11),
  ///  8th :  44
  (( 45<<8)+11),(( 45<<8)+11),(( 43<<8)+11),(( 43<<8)+11),
  (( 44<<8)+12),(( 79<<8)+12),(( 42<<8)+12),(( 41<<8)+12),
  ///  9th :  52
  (( 80<<8)+12),(( 80<<8)+12),(( 40<<8)+12),(( 40<<8)+12),
  (( 81<<8)+13),(( 39<<8)+13),(( 82<<8)+13),(( 38<<8)+13),
  /// 10th :  60
  (( 83<<8)+13),(( 83<<8)+13),(( 37<<8)+14),(( 35<<8)+14),
  (( 85<<8)+14),(( 33<<8)+14),(( 36<<8)+14),(( 34<<8)+14),
  /// 11th :  68
  (( 84<<8)+14),(( 84<<8)+14),(( 32<<8)+14),(( 32<<8)+14),
  (( 87<<8)+15),(( 89<<8)+15),(( 30<<8)+15),(( 31<<8)+15),
  /// 12th :  76
  (( 86<<8)+16),(( 86<<8)+16),(( 29<<8)+16),(( 29<<8)+16),
  (( 26<<8)+16),(( 26<<8)+16),(( 27<<8)+16),(( 27<<8)+16),
  (( 28<<8)+16),(( 28<<8)+16),(( 24<<8)+16),(( 24<<8)+16),
  (( 88<<8)+16),(( 88<<8)+16),(( 25<<8)+17),(( 22<<8)+17),
  /// 13th :  92
  (( 23<<8)+17),(( 23<<8)+17),(( 23<<8)+17),(( 23<<8)+17),
  (( 90<<8)+18),(( 90<<8)+18),(( 21<<8)+18),(( 21<<8)+18),
  (( 19<<8)+18),(( 19<<8)+18),((  3<<8)+18),((  3<<8)+18),
  ((  1<<8)+18),((  1<<8)+18),((  2<<8)+18),((  2<<8)+18),
  ((  0<<8)+18),((  0<<8)+18),(( 98<<8)+19),(( 99<<8)+19),
  ((100<<8)+19),((101<<8)+19),((102<<8)+19),((117<<8)+19),
  (( 97<<8)+19),(( 91<<8)+19),(( 92<<8)+19),(( 93<<8)+19),
  (( 94<<8)+19),(( 95<<8)+19),(( 96<<8)+19),((104<<8)+19),
  /// 14th : 124
  ((111<<8)+19),((112<<8)+19),((113<<8)+19),((114<<8)+19),
  ((115<<8)+19),((116<<8)+19),((110<<8)+19),((105<<8)+19),
  ((106<<8)+19),((107<<8)+19),((108<<8)+19),((109<<8)+19),
  ((118<<8)+19),((  6<<8)+19),((  8<<8)+19),((  9<<8)+19),
  /// 15th : 140
  (( 10<<8)+19),((  5<<8)+19),((103<<8)+19),((120<<8)+19),
  ((119<<8)+19),((  4<<8)+19),((  7<<8)+19),(( 15<<8)+19),
  /// 16th : 148
  (( 16<<8)+19),(( 18<<8)+19),(( 20<<8)+19),(( 17<<8)+19),
  /// 17th : 152
  (( 11<<8)+19),(( 12<<8)+19),
  /// 18th : 154
  (( 14<<8)+19),
  /// 19th : 155
  (( 13<<8)+19)
};

static Ipp32s clz(Ipp32u in)
{
  Ipp32s i;
  Ipp32u f;

  for(f = 0x80000000, i = 0; i < 32; f >>= 1, i ++)
  {
    if (in & f) break;
  }
  return i;
}

Ipp32u
aac_huff_decode_sf(Ipp8u** pp_bs, Ipp32s* p_offset)
{
  Ipp32u data;
  Ipp32s index;
  Ipp32s shift;

  data = 0;

  data  += (pp_bs[0])[0];
  data <<= 8;

  data  += (pp_bs[0])[1];
  data <<= 8;

  data  += (pp_bs[0])[2];
  data <<= 8;

  data  += (pp_bs[0])[3];

  data <<= p_offset[0];

  if (!(data & 0x80000000))
  {
    p_offset[0] ++;
    pp_bs[0] += (p_offset[0] / 8);
    p_offset[0] %= 8;
    return 60;
  }

  index = clz(~data);
  data <<= index;
  shift = sf_table[index] & 0xFF;
  index = sf_table[index] >> 8;

  data >>= shift;
  data = sf_table[index+data];
  shift = data & 0xFF;
  data >>= 8;
  p_offset[0] += shift;
  pp_bs[0] += (p_offset[0] / 8);
  p_offset[0] %= 8;
  return data;
}

#else

#ifdef _MSVC_LANG
#pragma warning( disable: 4206 )
#endif

#endif