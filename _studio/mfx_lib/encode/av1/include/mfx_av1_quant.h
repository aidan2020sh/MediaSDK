// Copyright (c) 2014-2019 Intel Corporation
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


#pragma once

#include "mfx_common.h"
#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

namespace AV1Enc {
    int16_t vp9_dc_quant(int qindex, int delta, int32_t bit_depth);
    int16_t vp9_ac_quant(int qindex, int delta, int32_t bit_depth);
    struct AV1VideoParam;
    void InitQuantizer(AV1VideoParam &par);
#ifdef ADAPTIVE_DEADZONE
    void adaptDzQuant(const int16_t *coeff_ptr, const int16_t fqpar[10], int32_t txSize, float *roundFAdj);
    void adaptDz(const int16_t *coeff_ptr, const int16_t *reccoeff_ptr, const int16_t fqpar[10], int32_t txSize, float *roundFAdj, int32_t eob);
#endif
}

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE

