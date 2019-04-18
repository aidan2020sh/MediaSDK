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

#include "mfx_av1_defs.h"

namespace H265Enc {

    const int32_t intra_mode_to_tx_type_context[AV1_INTRA_MODES] = {
        DCT_DCT,    // DC
        ADST_DCT,   // V
        DCT_ADST,   // H
        DCT_DCT,    // D45
        ADST_ADST,  // D135
        ADST_DCT,   // D117
        DCT_ADST,   // D153
        DCT_ADST,   // D207
        ADST_DCT,   // D63
        ADST_ADST,  // SMOOTH
        ADST_DCT,   // SMOOTH_V
        DCT_ADST,   // SMOOTH_H
        ADST_ADST,  // TM
    };

    extern const int16_t *av1_default_scan[TX_SIZES_ALL];
    extern const ScanOrder av1_scans[TX_SIZES_ALL][TX_TYPES];

    inline int32_t GetTxTypeAV1(int32_t plane, TxSize txSz, int32_t mode, int32_t txType) {
        const int32_t lossless = 0;
        const int32_t isInter = mode >= AV1_INTRA_MODES;

        if (lossless || (txSz >= TX_32X32 && !isInter))
            return DCT_DCT;

        if (plane == PLANE_TYPE_Y)
            return txType;

        if (isInter) {
            return (txType == IDTX && txSz >= TX_32X32)
                 ? DCT_DCT
                 : txType;
        } else {
            assert(plane == PLANE_TYPE_UV);
            return intra_mode_to_tx_type_context[mode];
        }
    }
}