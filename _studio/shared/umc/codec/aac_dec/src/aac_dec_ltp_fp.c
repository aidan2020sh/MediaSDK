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

#include "align.h"
#include "aac_dec_ltp_fp.h"

#include "ipps.h"

/********************************************************************/

static Ipp32f g_ltp_coef[8] = {
  0.570829f,
  0.696616f,
  0.813004f,
  0.911304f,
  0.984900f,
  1.067894f,
  1.194601f,
  1.369533f
};

/********************************************************************/

void ics_apply_ltp_I(sLtp *p_data,
                     s_SE_Individual_channel_stream *p_stream,
                     Ipp32f *p_spectrum)
{
  Ipp32s  i, j;
#if !defined(ANDROID)
  IPP_ALIGNED_ARRAY(32, Ipp32f, filterbank_in, 2048);
  IPP_ALIGNED_ARRAY(32, Ipp32f, filterbank_out, 1024);
#else
  static IPP_ALIGNED_ARRAY(32, Ipp32f, filterbank_in, 2048);
  static IPP_ALIGNED_ARRAY(32, Ipp32f, filterbank_out, 1024);
#endif
  Ipp32f *pTmp;
  Ipp32f  real_coef;
  Ipp32s  lag_size;
  Ipp32s  lag;
  Ipp32s  num;
  Ipp32s  start;
  Ipp32s  in_counter;
  Ipp32s  sfb;

  if (0 == p_stream->ltp_data_present)
    return;

  real_coef = g_ltp_coef[p_stream->ltp_coef];
  lag = p_stream->ltp_lag;

  if (EIGHT_SHORT_SEQUENCE == p_stream->window_sequence) {
    Ipp32s  max_sfb = p_stream->max_sfb;

    if (MAX_LTP_SFB_SHORT < max_sfb)
      max_sfb = MAX_LTP_SFB_SHORT;

    for (i = 0; i < 8; i++) {
      if (p_stream->ltp_short_used[i]) {
        Ipp32s  delay = lag + p_stream->ltp_short_lag[i] - 8;

        start = 3 * 1024 - 2 * 128 - delay;
        lag_size = 2 * 128;

        pTmp = filterbank_in;

        if (start < 1024) {
          num = 1024 - start;
          if (num > lag_size)
            num = lag_size;

          ippsMulC_32f(p_data->p_samples_1st_part + start, real_coef, pTmp,
                       num);
          lag_size -= num;
          start = 1024;
          pTmp += num;
        }

        if ((start < 2 * 1024) && (lag_size > 0)) {
          num = 2 * 1024 - start;
          if (num > lag_size)
            num = lag_size;

          ippsMulC_32f(p_data->p_samples_2nd_part + start - 1024, real_coef,
                       pTmp, num);
          lag_size -= num;
          start = 2 * 1024;
          pTmp += num;
        }

        if (lag_size > 0) {
          num = 3 * 1024 - start;
          if (num > lag_size)
            num = lag_size;

          ippsMulC_32f(p_data->p_samples_3rd_part + start - 2 * 1024, real_coef,
                       pTmp, num);
          lag_size -= num;
        }

        if (lag_size > 0) {
          ippsZero_32f(pTmp, lag_size);
        }

        FilterbankEnc(p_data->p_filterbank_data, filterbank_in,
                      &filterbank_in[1024], p_stream->window_sequence,
                      p_stream->window_shape, p_data->prev_windows_shape,
                      filterbank_out, 1);

        if (0 != p_stream->tns_data_present) {
          ics_apply_tns_enc_I(p_data->p_tns_data, filterbank_out);
        }

        for (sfb = 0; sfb < max_sfb; sfb++) {
          Ipp32s  begin = p_stream->sfb_offset_short_window[sfb];
          Ipp32s  end = p_stream->sfb_offset_short_window[sfb + 1];

          for (j = begin; j < end; j++) {
            p_spectrum[i * 128 + j] += filterbank_out[j];
          }
        }
      }
    }
  } else {
    Ipp32s  delay = lag;

    lag_size = 2048;
    in_counter = 0;
    start = 2048 - delay;
    pTmp = filterbank_in;

    if (start < 1024) {
      num = 1024 - start;

      ippsMulC_32f(p_data->p_samples_1st_part + start, real_coef, pTmp, num);
      lag_size -= num;
      start = 1024;
      pTmp += num;
    }

    num = 2 * 1024 - start;
    ippsMulC_32f(p_data->p_samples_2nd_part + start - 1024, real_coef, pTmp,
                 num);

    lag_size -= num;
    pTmp += num;

    if (lag_size > 0) {
      num = lag_size > 1024 ? 1024 : lag_size;

      ippsMulC_32f(p_data->p_samples_3rd_part, real_coef, pTmp, num);

      lag_size -= num;
      pTmp += num;
    }

    if (lag_size > 0) {
      ippsZero_32f(pTmp, lag_size);
    }

    FilterbankEnc(p_data->p_filterbank_data, filterbank_in,
                  &filterbank_in[1024], p_stream->window_sequence,
                  p_stream->window_shape, p_data->prev_windows_shape,
                  filterbank_out, 1);

    if (0 != p_stream->tns_data_present) {
      ics_apply_tns_enc_I(p_data->p_tns_data, filterbank_out);
    }

    for (sfb = 0; sfb < p_stream->max_sfb; sfb++) {
      if (p_stream->ltp_long_used[sfb]) {
        Ipp32s  begin = p_stream->sfb_offset_long_window[sfb];
        Ipp32s  end = p_stream->sfb_offset_long_window[sfb + 1];

        for (i = begin; i < end; i++) {
          p_spectrum[i] += filterbank_out[i];
        }
      }
    }
  }
}

/********************************************************************/

#else
# pragma warning( disable: 4206 )
#endif //UMC_ENABLE_AAC_AUDIO_DECODER

