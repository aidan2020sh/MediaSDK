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

#include "ippvc.h"
#include "umc_scene_analyzer_p.h"
#include "umc_scene_analyzer_mb_func.h"

namespace UMC
{

void SceneAnalyzerP::AnalyzeIntraMB(const Ipp8u *pSrc, Ipp32s srcStep,
                                    UMC_SCENE_INFO *pMbInfo)
{
    Ipp32u blockDev;

    // get block deviations
    blockDev = ippiGetIntraBlockDeviation_4x4_8u(pSrc, srcStep);
    pMbInfo->sumDev[SA_INTRA] = (blockDev + 8) / 16;

    // get average color
    pMbInfo->sumDev[SA_COLOR] = ippiGetAverage4x4_8u(pSrc, srcStep);

} // void SceneAnalyzerP::AnalyzeIntraMB(const Ipp8u *pSrc, Ipp32s srcStep,

void SceneAnalyzerP::AnalyzeInterMB(const Ipp8u *pRef, Ipp32s refStep,
                                    const Ipp8u *pSrc, Ipp32s srcStep,
                                    UMC_SCENE_INFO *pMbInfo)
{
    Ipp16s residual[4 * 4];
    Ipp32u blockDev;

    // do intra analysis
    AnalyzeIntraMB(pSrc, srcStep, pMbInfo);

    // get block residuals
    ippiGetResidual4x4_8u16s(pRef, refStep,
                             pSrc, srcStep,
                             residual, 4);

    // get block deviation
    blockDev = ippiGetInterBlockDeviation_4x4_16s(residual, 4);

    // get frame mode weight
    pMbInfo->sumDev[SA_INTER] = (blockDev + 8) / 16;
    pMbInfo->sumDev[SA_INTER_ESTIMATED] = pMbInfo->sumDev[SA_INTER];
    // increment number of excelent matches
    if (1 >= pMbInfo->sumDev[SA_INTER])
    {
        pMbInfo->bestMatches += 1;
    }

} // void SceneAnalyzerP::AnalyzeInterMB(const Ipp8u *pRef, Ipp32s refStep,

static
Ipp32u GetMinValue(Ipp16u *pValues, Ipp32u numValues)
{
    Ipp32u value = (Ipp32u) -1;
    Ipp32u x;

    // run over the array and find the lowest
    for (x = 0; x < numValues; x += 1)
    {
        value = (value > pValues[x]) ? (pValues[x]) : (value);
    }

    return value;

} // Ipp32u GetMinValue(Ipp16u *pValues, Ipp32u numValues)

void SceneAnalyzerP::AnalyzeInterMBMotion(const Ipp8u *pRef, Ipp32s refStep,
                                          IppiSize refMbDim,
                                          const Ipp8u *pSrc, Ipp32s srcStep,
                                          Ipp32u mbX, Ipp32u mbY, Ipp16u *pSADs,
                                          UMC_SCENE_INFO *pMbInfo)
{
    Ipp32u numberOfSADs = SA_ESTIMATION_WIDTH;
    Ipp32u y;
    Ipp32u pixels, topRows, bottomRows;

    //
    // analyze working boundaries
    //

    // horizontal direction
    pixels = IPP_MIN(mbX * 4, SA_ESTIMATION_WIDTH / 2);
    pRef -= pixels;
    numberOfSADs -= (SA_ESTIMATION_WIDTH / 2 - pixels);

    pixels = IPP_MIN((refMbDim.width - mbX - 1) * 4, SA_ESTIMATION_WIDTH / 2);
    numberOfSADs -= (SA_ESTIMATION_WIDTH / 2 - pixels);
    numberOfSADs = (numberOfSADs + 7) & -8;

    // vertical direction
    topRows = IPP_MIN(mbY * 4, SA_ESTIMATION_HEIGHT / 2);

    bottomRows = IPP_MIN((refMbDim.height - mbY) * 4 - 3, SA_ESTIMATION_HEIGHT / 2);

    // main working cycle
    for (y = 0; y < SA_ESTIMATION_HEIGHT / 2; y += 1)
    {
        try
        {
        // try to find upper
        if (y < topRows)
        {
            Ipp32u minSAD;

            ippiSAD4x4xN_8u16u_C1R(pSrc, srcStep,
                                   pRef - refStep * (y + 1), refStep,
                                   pSADs,
                                   numberOfSADs);

            // get the lowest SAD
            minSAD = (GetMinValue(pSADs, numberOfSADs) + 8) / 16;
            pMbInfo->sumDev[SA_INTER_ESTIMATED] = IPP_MIN(minSAD, pMbInfo->sumDev[SA_INTER_ESTIMATED]);
            if (1 >= pMbInfo->sumDev[SA_INTER_ESTIMATED])
                break;
        }

        // try to find lower
        if (y < bottomRows)
        {
            Ipp32u minSAD;

            ippiSAD4x4xN_8u16u_C1R(pSrc, srcStep,
                                   pRef + refStep * y, refStep,
                                   pSADs,
                                   numberOfSADs);

            // get the lowest SAD
            minSAD = (GetMinValue(pSADs, numberOfSADs) + 8) / 16;
            pMbInfo->sumDev[SA_INTER_ESTIMATED] = IPP_MIN(minSAD, pMbInfo->sumDev[SA_INTER_ESTIMATED]);
            if (1 >= pMbInfo->sumDev[SA_INTER_ESTIMATED])
                break;
        }
        }
        catch(...)
        {
#if defined(WIN32) && !defined(WIN64)
            __asm int 3;
#endif
        }
    }

} // void SceneAnalyzerP::AnalyzeInterMBMotion(const Ipp8u *pRef, Ipp32s refStep,

} // namespace UMC
#endif
