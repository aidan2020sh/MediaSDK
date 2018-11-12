// Copyright (c) 2005-2018 Intel Corporation
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

#ifndef __MP3DEC_OWN_FP_H__
#define __MP3DEC_OWN_FP_H__

#include "math.h"
#include "mp3dec_own.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef Ipp32f samplefloat[NUM_CHANNELS][1152];
typedef Ipp32f samplefloatrw[NUM_CHANNELS][36][32];

struct _MP3Dec {
    MP3Dec_com com;
    VM_ALIGN32_DECL(Ipp32u) global[1152 * NUM_CHANNELS * 2 + LEN_MDCT_LINE];
    VM_ALIGN32_DECL(Ipp32f) prevblk[2][LEN_MDCT_LINE];
    VM_ALIGN32_DECL(Ipp32f) mc_pred_buf[2][8][36+9];
    VM_ALIGN32_DECL(Ipp32f) mc_lfe_buf[17];

    samplefloat *smpl_xr;       // out of dequantizer
    samplefloat *smpl_ro;       // out of reordering
    samplefloat *smpl_re;       // out of antialiasing
    samplefloatrw *smpl_rw;     // out of imdct
    samplefloat *smpl_sb;       // out of subband synth

    Ipp32f GlobalScaleFactor[2][2];

    IppsMDCTInvSpec_32f *pMDCTSpecShort;
    IppsMDCTInvSpec_32f *pMDCTSpecLong;
    IppsFilterSpec_PQMF_MP3 *pPQMFSpec[NUM_CHANNELS];
    Ipp8u *pqmf_mem;
    Ipp32s pqmf_size;

    Ipp8u *mdct_buffer;

    Ipp32s dctnum_prev[2];

    Ipp16s m_ptr[NUM_CHANNELS][2];
    Ipp16s m_even[NUM_CHANNELS];
};

extern Ipp32f mp3dec_scale_values[64];

Ipp32s mp3dec_decode_data_LayerI(MP3Dec *state);
Ipp32s mp3dec_decode_data_LayerII(MP3Dec *state);
MP3Status mp3dec_decode_data_LayerIII(MP3Dec *state);

void mp3dec_dequantize_sample_mc_l2(MP3Dec *state);
void mp3dec_mc_denormalize_sample_l2(MP3Dec *state);
void mp3dec_dequantize_sample_mc_l2 (MP3Dec *state);
void mp3dec_mc_lfe_l2(MP3Dec *state, Ipp32f *out);
void mp3dec_mc_dematricing (MP3Dec *state);
void mp3dec_mc_denormalizing (MP3Dec *state);

#ifdef __cplusplus
}
#endif

#endif
