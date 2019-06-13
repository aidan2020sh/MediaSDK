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

#ifndef __MP3_ALLOC_TAB_H__
#define __MP3_ALLOC_TAB_H__

#include "ippdefs.h"

#ifdef __cplusplus
extern "C" {
#endif


/*
Table 3-B.2a Possible quantization per subband

Fs = 48 kHz Bit rates per channel = 56, 64, 80, 96, 112, 128, 160, 192 kbits/s,
and free format
Fs = 44.1 kHz Bit rates per channel = 56, 64, 80 kbits/s
Fs = 32 kHz Bit rates per channel = 56, 64, 80 kbits/s
*/
extern const Ipp8u mp3_alloc_table1[];

extern const Ipp32s mp3_nbal_alloc_table1[32];

/*
Table 3-B.2b. Possible quantization per subband

Fs = 48 kHz -------------- not relevant --------------
Fs = 44.1 kHz Bitrates per channel = 96, 112, 128, 160, 192 kbits/s
and free format
Fs = 32 kHz Bitrates per channel = 96, 112, 128, 160, 192 kbits/s
and free format
*/

extern const Ipp8u mp3_alloc_table2[];

extern const Ipp32s mp3_nbal_alloc_table2[32];

/*
Table 3-B.2c. Possible quantization per subband


Fs = 48 kHz Bitrates per channel = 32, 48 kbits/s
Fs = 44.1 kHz Bitrates per channel = 32, 48 kbits/s
Fs = 32 kHz -------- not relevant --------
*/
extern const Ipp8u mp3_alloc_table3[];

extern const Ipp32s mp3_nbal_alloc_table3[32];

/*
Table 3-B.2d. Possible quantization per subband


Fs = 48 kHz  ------- not relevant -------
Fs = 44.1kHz  ------- not relevant -------
Fs = 32 kHz  Bitrates per channel = 32, 48 kbits/s
*/

extern const Ipp8u mp3_alloc_table4[];

extern const Ipp32s mp3_nbal_alloc_table4[32];

/*
Table B-1 (MPEG 2). Possible quantization per subband

Fs = 16, 22.05, 24 kHz
*/

extern const Ipp8u  mp3_alloc_table5[];
extern const Ipp32s mp3_nbal_alloc_table5[32];
extern const Ipp32s mp3_cls_quant[17];
extern const Ipp32s mp3_numbits[17];
extern const Ipp32s mp3_sblimit_table[];
extern const Ipp16s *mp3_degroup[];

#ifdef __cplusplus
}
#endif

#endif //__MP3_ALLOC_TAB_H__