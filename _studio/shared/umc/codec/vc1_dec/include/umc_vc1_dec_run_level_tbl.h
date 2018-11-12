// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#ifndef __UMC_VC1_DEC_RUN_LEVEL_TBL_H__
#define __UMC_VC1_DEC_RUN_LEVEL_TBL_H__


#include "umc_vc1_common_defs.h"

extern const Ipp32s VC1_DQScaleTbl[64];

const extern Ipp32s VC1_HighMotionIntraAC[];
const extern Ipp32s VC1_HighMotionInterAC[];
const extern Ipp32s VC1_LowMotionIntraAC[];
const extern Ipp32s VC1_LowMotionInterAC[];
const extern Ipp32s VC1_MidRateIntraAC[];
const extern Ipp32s VC1_MidRateInterAC[];
const extern Ipp32s VC1_HighRateIntraAC[];
const extern Ipp32s VC1_HighRateInterAC[];

#endif //__umc_vc1_dec_run_level_tbl_H__
#endif //UMC_ENABLE_VC1_VIDEO_DECODER
