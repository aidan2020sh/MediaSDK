// Copyright (c) 2007-2018 Intel Corporation
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

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#include "ippdefs.h"
#include "umc_scene_info.h"

namespace UMC
{

// Declaration of block layer functions
Ipp32u ippiGetIntraBlockDeviation_4x4_8u(const Ipp8u *pSrc, Ipp32s srcStep);
Ipp32u ippiGetAverage4x4_8u(const Ipp8u *pSrc, Ipp32s srcStep);
IppStatus ippiGetResidual4x4_8u16s(const Ipp8u *pRef, Ipp32s refStep,
                                   const Ipp8u *pSrc, Ipp32s srcStep,
                                   Ipp16s *pDst, Ipp32s dstStep);
Ipp32u ippiGetInterBlockDeviation_4x4_16s(const Ipp16s *pSrc, Ipp32s srcStep);

} // namespace UMC
#endif
