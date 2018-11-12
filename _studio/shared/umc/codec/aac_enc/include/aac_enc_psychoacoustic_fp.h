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

#ifndef __PSYCHOACOUSTIC_H
#define __PSYCHOACOUSTIC_H

#include "aaccmn_const.h"
#include "aac_enc_fp.h"
#include "align.h"
#include "aac_filterbank_fp.h"

#include "ipps.h"
#include "ippac.h"

#define MAX_PPT_SHORT 48
#define MAX_PPT_LONG  72
#define ATTENUATION    ((Ipp32f)0.0012589) /* -29 dB */
#define ATTENUATION_DB ((Ipp32f)2.9)

typedef struct {
  Ipp32s bu;
  Ipp32s bo;
  Ipp32f w1;
  Ipp32f w2;
} AACp2sb;

typedef struct
{
  Ipp32s              sampling_frequency;
  Ipp32s              num_ptt;
  Ipp32s*             w_low;
  Ipp32s*             w_high;
  Ipp32f*             w_width;
  Ipp32f*             bval;
  Ipp32f*             qsthr;

} sPsyPartitionTable;

typedef struct
{
  __ALIGN Ipp32f      r[2][__ALIGNED(1024)];
  __ALIGN Ipp32f      nb_long[2][__ALIGNED(MAX_PPT_LONG)];
  __ALIGN Ipp32f      nb_short[8][__ALIGNED(MAX_PPT_SHORT)];

  Ipp32s              block_type;
  Ipp32s              desired_block_type;
  Ipp32s              next_desired_block_type;

  IppsIIRState_32f*   IIRfilterState;
  Ipp32f              avWinEnergy;
  Ipp32f              bitsToPECoeff;
  Ipp32f              scalefactorDataBits;
  Ipp32f              PEtoNeededPECoeff;

  Ipp32f              peMin;
  Ipp32f              peMax;

  Ipp32s              attackIndex;
  Ipp32s              lastAttackIndex;
} sPsychoacousticBlock;

typedef struct
{
  __ALIGN Ipp32f      noiseThr[2][__ALIGNED(MAX_SFB_SHORT * 8)];

  __ALIGN Ipp32f      sprdngf_long[MAX_PPT_LONG * MAX_PPT_LONG];
  __ALIGN Ipp32f      sprdngf_short[MAX_PPT_SHORT * MAX_PPT_SHORT];

  __ALIGN Ipp32f      rnorm_long[MAX_PPT_LONG];
  __ALIGN Ipp32f      rnorm_short[MAX_PPT_SHORT];

  Ipp32f*             input_data[2][3];

  Ipp32s              iblen_long;
  Ipp32s              iblen_short;

  Ipp32s              nb_curr_index;
  Ipp32s              nb_prev_index;

  Ipp32s*             sfb_offset_long;
  Ipp32s*             sfb_offset_short;
  Ipp32s              num_sfb_long;
  Ipp32s              num_sfb_short;

  Ipp32s              ns_mode;

  sPsyPartitionTable* longWindow;
  sPsyPartitionTable* shortWindow;
  AACp2sb*            aacenc_p2sb_l;
  AACp2sb*            aacenc_p2sb_s;

  IppsFFTSpec_R_32f*  pFFTSpecShort;
  IppsFFTSpec_R_32f*  pFFTSpecLong;
  Ipp8u*              pBuffer;

  sFilterbank*        filterbank_block;

  Ipp32s              non_zero_line_long;
  Ipp32s              non_zero_line_short;

  Ipp32f              attackThreshold;
} sPsychoacousticBlockCom;

extern sPsyPartitionTable psy_partition_tables_long[];
extern sPsyPartitionTable psy_partition_tables_short[];
extern AACp2sb *aacenc_p2sb_l[];
extern AACp2sb *aacenc_p2sb_s[];

#ifdef  __cplusplus
extern "C" {
#endif

  AACStatus InitPsychoacousticCom(sPsychoacousticBlockCom* pBlock,
                                  Ipp8u* mem,
                                  Ipp32s sf_index,
                                  Ipp32s ns_mode,
                                  Ipp32s *size_all);
  void InitPsychoacoustic(sPsychoacousticBlockCom* pBlockCom,
                          sPsychoacousticBlock* pBlock);
  void Psychoacoustic(sPsychoacousticBlock** pBlock,
                      sPsychoacousticBlockCom* pBlockCom,
                      Ipp32f **mdct_line,
                      Ipp32s *window_shape,
                      Ipp32s *prev_window_shape,
                      Ipp32s stereo_mode,
                      Ipp32s numCh);

#ifdef  __cplusplus
}
#endif

#ifndef PI
#define PI 3.14159265359f
#endif

#endif//__PSYCHOACOUSTIC_H
