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

#include "umc_scene_analyzer_mb_func.h"

namespace UMC
{

#define temp eax
#define counter ecx
#define src_step edx
#define src esi

uint32_t ippiGetIntraBlockDeviation_4x4_8u(const uint8_t *pSrc, int32_t srcStep)
{
#if defined(_WIN32) && !defined(_WIN64)
#if defined (__ICL)
//missing return statement at end of non-void function
#pragma warning(disable:1011)
#endif
    __asm
    {
        mov src, dword ptr [pSrc]
        mov src_step, dword ptr [srcStep]
        lea temp, [src_step + src_step * 02h]
        pxor mm4, mm4
        pxor mm5, mm5
        pxor mm6, mm6
        pxor mm7, mm7

        movd mm0, qword ptr [src]
        movd mm1, qword ptr [src + src_step]
        movd mm2, qword ptr [src + src_step * 02h]
        movd mm3, qword ptr [src + temp]
        psadbw mm4, mm0
        psadbw mm5, mm1
        psadbw mm6, mm2
        psadbw mm7, mm3
        paddd mm4, mm5
        paddd mm4, mm6
        paddd mm4, mm7

        mov temp, 08h
        movd mm5, temp
        paddsw mm4, mm5
        psraw mm4, 04h
        punpcklbw mm4, mm4
        punpcklbw mm4, mm4
        movq mm5, mm4
        movq mm6, mm4
        movq mm7, mm4

        psadbw mm4, mm0
        psadbw mm5, mm1
        psadbw mm6, mm2
        psadbw mm7, mm3
        paddd mm4, mm5
        paddd mm4, mm6
        paddd mm4, mm7

        movd temp, mm4

        emms
    }
#else
    const uint8_t *pCur;
    uint32_t average;
    uint32_t deviation;
    uint32_t x, y;

    // check error(s)
    if (0 == pSrc)
        return 0;

    // reset variables
    average = 0;
    deviation = 0;

    //
    // get 4x4 block average
    //

    // cycle over rows
    pCur = pSrc;
    for (y = 0; y < 4; y += 1)
    {
        // cycle in the row
        for (x = 0; x < 4; x += 1)
        {
            // getting frame mode sum of components
            average += pCur[x];
        }

        // advance source pointer
        pCur += srcStep;
    }

    // get the average
    average = (average + 8) / 16;

    //
    // get 4x4 block deviation
    //

    // cycle over rows
    pCur = pSrc;
    for (y = 0; y < 4; y += 1)
    {
        // cycle in the row
        for (x = 0; x < 4; x += 1)
        {
            int32_t temp;
            int32_t signExtended;

            // getting sum of components
            temp = pCur[x] - average;
            signExtended = temp >> 31;

            // we use simple scheme of abs calculation,
            // using one ADD and one XOR operation
            deviation += (temp ^ signExtended) - signExtended;
        }

        // advance source pointer
        pCur += srcStep;
    }

    return deviation;
#endif
} // uint32_t ippiGetIntraBlockDeviation_4x4_8u(const uint8_t *pSrc, int32_t srcStep)

uint32_t ippiGetAverage4x4_8u(const uint8_t *pSrc, int32_t srcStep)
{
#if defined(_WIN32) && !defined(_WIN64)
    __asm
    {
        mov src, dword ptr [pSrc]
        mov src_step, dword ptr [srcStep]
        pxor mm4, mm4

        movd mm0, qword ptr [src]
        movd mm1, qword ptr [src + src_step]
        lea src, [src + src_step * 02h]
        movd mm2, qword ptr [src]
        movd mm3, qword ptr [src + src_step]
        psadbw mm0, mm4
        psadbw mm1, mm4
        psadbw mm2, mm4
        psadbw mm3, mm4
        paddd mm0, mm1
        paddd mm0, mm2
        paddd mm0, mm3

        movd temp, mm0
        add temp, 08h
        shr temp, 04h
        emms
    }
#else
    uint32_t y;
    uint32_t sum;

    sum = 0;

    // cycle over rows
    for (y = 0; y < 4; y += 1)
    {
        uint32_t x;

        // cycle in the row
        for (x = 0; x < 4; x += 1)
        {
            sum += pSrc[x];
        }

        // set the next line
        pSrc += srcStep;
    }

    return (sum + 8) / 16;
#endif
} // uint32_t ippiGetAverage4x4_8u(const uint8_t *pSrc, int32_t srcStep)

#undef temp
#undef counter
#undef src_step
#undef src

#define ref eax
#define counter ecx
#define dst_step edx
#define src esi
#define dst edi

IppStatus ippiGetResidual4x4_8u16s(const uint8_t *pRef, int32_t refStep,
                                   const uint8_t *pSrc, int32_t srcStep,
                                   int16_t *pDst, int32_t dstStep)
{
#if defined(_WIN32) && !defined(_WIN64)
    __asm
    {
        mov ref, dword ptr [pRef]
        mov src, dword ptr [pSrc]
        mov dst, dword ptr [pDst]
        mov dst_step, dword ptr [dstStep]

        pxor mm7, mm7
        mov counter, 04h

RESTART:
        movd mm0, dword ptr [src]
        movd mm2, dword ptr [ref]
        punpcklbw mm0, mm7
        punpcklbw mm2, mm7
        psubsw mm0, mm2
        movq qword ptr [dst], mm0

        add src, dword ptr [srcStep]
        add ref, dword ptr [refStep]
        lea dst, [dst + dst_step * 02h]
        sub counter, 01h
        jnz RESTART

        emms
    }
#else
    uint32_t y;

    // cycle over the rows
    for (y = 0; y < 4; y += 1)
    {
        uint32_t x;

        // cycle in the row
        for (x = 0; x < 4; x += 1)
        {
            pDst[x] = (int16_t) (pSrc[x] - pRef[x]);
        }

        // set the next line
        pRef += refStep;
        pSrc += srcStep;
        pDst += dstStep;
    }

    return ippStsOk;
#endif
} // IppStatus ippiGetResidual4x4_8u16s(const uint8_t *pRef, int32_t refStep,

#undef ref
#undef counter
#undef dst_step
#undef src
#undef dst

uint32_t ippiGetInterBlockDeviation_4x4_16s(const int16_t *pSrc, int32_t srcStep)
{
    const int16_t *pCur;
    uint32_t deviation;
    uint32_t x, y;

    // check error(s)
    if (0 == pSrc)
        return 0;

    // reset variables
    deviation = 0;

    //
    // get 4x4 block deviation
    //

    // cycle over rows
    pCur = pSrc;
    for (y = 0; y < 4; y += 1)
    {
        // cycle in the row
        for (x = 0; x < 4; x += 1)
        {
            int32_t temp;
            int32_t signExtended;

            // getting sum of components
            temp = pCur[x];
            signExtended = temp >> 31;

            // we use simple scheme of abs calculation,
            // using one ADD and one XOR operation
            deviation += (temp ^ signExtended) - signExtended;
        }

        // advance source pointer
        pCur += srcStep;
    }

    return deviation;

} // uint32_t ippiGetInterBlockDeviation_4x4_16s(const int16_t *pSrc, int32_t srcStep)

} // namespace UMC
#endif
