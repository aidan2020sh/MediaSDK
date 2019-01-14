// Copyright (c) 2008-2018 Intel Corporation
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

#include "umc_vc1_enc_mb.h"
#include "ippi.h"
#include "umc_vc1_enc_tables.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

namespace UMC_VC1_ENCODER
{


bool DCACPredictionIntraFrameSM(   VC1EncoderMBData* pCurrMB,
                                   NeighbouringMBsData*  pMBs,
                                   VC1EncoderMBData* pPredBlock,
                                   int16_t defPredictor,
                                   eDirection* direction)
{
    int32_t              z = 0;
    int                 i;
    VC1EncoderMBData    TempBlock;
    int16_t              temp [VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS]={0};

    VC1EncoderMBData* pLeftMB    = (pMBs->LeftMB)   ? pMBs->LeftMB   :&TempBlock;
    VC1EncoderMBData* pTopMB     = (pMBs->TopMB)    ? pMBs->TopMB    :&TempBlock;
    VC1EncoderMBData* pTopLeftMB = (pMBs->TopLeftMB)? pMBs->TopLeftMB:&TempBlock;

    TempBlock.InitBlocks(temp);

    TempBlock.m_pBlock[0][0] = TempBlock.m_pBlock[1][0]= TempBlock.m_pBlock[2][0] =
    TempBlock.m_pBlock[3][0] = TempBlock.m_pBlock[4][0]= TempBlock.m_pBlock[5][0] =
                                                                            defPredictor;

    pLeftMB     = (pLeftMB)     ? pLeftMB   :&TempBlock;
    pTopLeftMB  = (pTopLeftMB)  ? pTopLeftMB:&TempBlock;
    pTopMB      = (pTopMB)      ? pTopMB    :&TempBlock;

    int16_t*             pBlocks[VC1_ENC_NUMBER_OF_BLOCKS*3]=
    {
        pTopMB->m_pBlock[2],    pTopLeftMB->m_pBlock[3], pLeftMB->m_pBlock[1],
        pTopMB->m_pBlock[3],    pTopMB->m_pBlock[2],     pCurrMB->m_pBlock[0],
        pCurrMB->m_pBlock[0],   pLeftMB->m_pBlock[1],    pLeftMB->m_pBlock[3],
        pCurrMB->m_pBlock[1],   pCurrMB->m_pBlock[0],    pCurrMB->m_pBlock[2],
        pTopMB->m_pBlock[4],    pTopLeftMB->m_pBlock[4], pLeftMB->m_pBlock[4],
        pTopMB->m_pBlock[5],    pTopLeftMB->m_pBlock[5], pLeftMB->m_pBlock[5]
    };
    //  Estimation about AC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        if (((uint32_t)((pBlocks[3*i+0][0] - pBlocks[3*i+1][0])*(pBlocks[3*i+0][0] - pBlocks[3*i+1][0]))<=
             (uint32_t)((pBlocks[3*i+2][0] - pBlocks[3*i+1][0])*(pBlocks[3*i+2][0] - pBlocks[3*i+1][0]))))
        {
            direction[i] = VC1_ENC_LEFT;
            z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
        }
        else
        {
            direction[i] = VC1_ENC_TOP;
            z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
        }
    }

        //AC, DC prediction
        for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
        {
            uint32_t numBlk    = (3*i + 2*(!(direction[i] & VC1_ENC_TOP)));
            uint32_t numTable  = (z<0)*direction[i];
            Diff8x8 (pCurrMB->m_pBlock[i],      pCurrMB->m_uiBlockStep[i],
                    pBlocks[numBlk],           pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i],   pPredBlock->m_uiBlockStep[i],numTable);

#ifdef VC1_ENC_DEBUG_ON
            if(z<0)
            {
                pDebug->SetBlockAsIntra(i);
                pDebug->SetDCACInfo(1, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                    direction[i], i);
            }
            else
            {
                pDebug->SetBlockAsIntra(i);
                pDebug->SetDCACInfo(0, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                    direction[i], i);
            }
#endif
        }

    return (z<0);
}


bool DCACPredictionFrame   (VC1EncoderMBData*       pCurrMB,
                            NeighbouringMBsData*    pMBs,
                            VC1EncoderMBData*       pPredBlock,
                            int16_t                  defPredictor,
                            eDirection*             direction)
{
    int32_t              z = 0;
    int                 i;
    VC1EncoderMBData    TempBlock;
    int16_t              temp [VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS]={0};

    VC1EncoderMBData* pLeftMB    = pMBs->LeftMB;
    VC1EncoderMBData* pTopMB     = pMBs->TopMB;
    VC1EncoderMBData* pTopLeftMB = (pMBs->TopLeftMB)? pMBs->TopLeftMB:&TempBlock;

    TempBlock.InitBlocks(temp);
    TempBlock.m_pBlock[0][0] = TempBlock.m_pBlock[1][0]= TempBlock.m_pBlock[2][0] =
    TempBlock.m_pBlock[3][0] = TempBlock.m_pBlock[4][0]= TempBlock.m_pBlock[5][0] =
                                                                            defPredictor;
    pTopLeftMB  = (pTopLeftMB)  ? pTopLeftMB:&TempBlock;

    int16_t*             pBlocks[VC1_ENC_NUMBER_OF_BLOCKS*3]=
    {
        0,                      pTopLeftMB->m_pBlock[3], 0,                   //0..2
        0,                      0,                       pCurrMB->m_pBlock[0],//3..5
        pCurrMB->m_pBlock[0],   0,                       0,                   //6..8
        pCurrMB->m_pBlock[1],   pCurrMB->m_pBlock[0],    pCurrMB->m_pBlock[2],//9..11
        0,                      pTopLeftMB->m_pBlock[4], 0,                   //12..14
        0,                      pTopLeftMB->m_pBlock[5], 0                    //15..17
    };
    if (pTopMB)
    {
        pBlocks[0]  = pTopMB->m_pBlock[2];
        pBlocks[3]  = pTopMB->m_pBlock[3];
        pBlocks[4]  = pTopMB->m_pBlock[2];
        pBlocks[12] = pTopMB->m_pBlock[4];
        pBlocks[15] = pTopMB->m_pBlock[5];
    }
    if (pLeftMB)
    {
        pBlocks[2]   = pLeftMB->m_pBlock[1];
        pBlocks[7]   = pLeftMB->m_pBlock[1];
        pBlocks[8]   = pLeftMB->m_pBlock[3];
        pBlocks[14]  = pLeftMB->m_pBlock[4];
        pBlocks[17]  = pLeftMB->m_pBlock[5];
    }

    //  Estimation about AC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        switch (((!pBlocks[3*i+0])<<1)|(!pBlocks[3*i+2]))
        {
        case 0:
            if (vc1_abs_32s(pBlocks[3*i+0][0] - pBlocks[3*i+1][0])<= vc1_abs_32s(pBlocks[3*i+2][0] - pBlocks[3*i+1][0]))
            {
                direction[i] = VC1_ENC_LEFT;
                z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            }
            else
            {
                direction[i] = VC1_ENC_TOP;
                z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            }
            break;
        case 1:
            direction[i] = VC1_ENC_TOP;
            z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            break;
        case 2:
            direction[i] = VC1_ENC_LEFT;
            z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            break;
        default:
            direction[i]     = VC1_ENC_LEFT;
            pBlocks[3*i+2]   = TempBlock.m_pBlock[0];
            break;
        }
    }

    //AC, DC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        uint32_t numBlk    = 3*i + 2*(!(direction[i] & VC1_ENC_TOP));
        uint32_t numTable  = (z<0)*direction[i];
        Diff8x8 (pCurrMB->m_pBlock[i],      pCurrMB->m_uiBlockStep[i],
                pBlocks[numBlk],            pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i],    pPredBlock->m_uiBlockStep[i],numTable);

#ifdef VC1_ENC_DEBUG_ON
        if(z<0)
        {
            pDebug->SetBlockAsIntra(i);
            pDebug->SetDCACInfo(1, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                direction[i], i);
        }
        else
        {
            pDebug->SetBlockAsIntra(i);
            pDebug->SetDCACInfo(0, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                direction[i], i);
        }
#endif
    }
    return (z<0);
}


bool DCACPredictionFrame4MVIntraMB   (  VC1EncoderMBData*               pCurrMB,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        int16_t                          defPredictor,
                                        eDirection*                     direction)
{
    int32_t              z = 0;
    int                 i;
    VC1EncoderMBData    TempBlock;
    int16_t              temp [VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS]={0};

    VC1EncoderMBData* pLeftMB    = pMBs->LeftMB;
    VC1EncoderMBData* pTopMB     = pMBs->TopMB;
    VC1EncoderMBData* pTopLeftMB = (pMBs->TopLeftMB)? pMBs->TopLeftMB:&TempBlock;

    TempBlock.InitBlocks(temp);
    TempBlock.m_pBlock[0][0] = TempBlock.m_pBlock[1][0]= TempBlock.m_pBlock[2][0] =
    TempBlock.m_pBlock[3][0] = TempBlock.m_pBlock[4][0]= TempBlock.m_pBlock[5][0] =
                                                                            defPredictor;
    pTopLeftMB  = (pTopLeftMB)  ? pTopLeftMB:&TempBlock;

    int16_t*             pBlocks[VC1_ENC_NUMBER_OF_BLOCKS*3]=
    {
        0,                      (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(3)))? pTopLeftMB->m_pBlock[3]:TempBlock.m_pBlock[3],  0,                   //0..2
        0,                      0,                                                                                      pCurrMB->m_pBlock[0],//3..5
        pCurrMB->m_pBlock[0],   0,                                                                                      0,                   //6..8
        pCurrMB->m_pBlock[1],   pCurrMB->m_pBlock[0],                                                                   pCurrMB->m_pBlock[2],//9..11
        0,                      (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(4)))? pTopLeftMB->m_pBlock[4]:TempBlock.m_pBlock[4],  0,                   //12..14
        0,                      (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(5)))? pTopLeftMB->m_pBlock[5]:TempBlock.m_pBlock[5],  0                    //15..17
    };
    if (pTopMB)
    {
        pBlocks[0]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(2)))? pTopMB->m_pBlock[2]: 0;
        pBlocks[3]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(3)))? pTopMB->m_pBlock[3]: 0;
        pBlocks[4]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(2)))? pTopMB->m_pBlock[2]: TempBlock.m_pBlock[2];
        pBlocks[12] = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(4)))? pTopMB->m_pBlock[4]: 0;
        pBlocks[15] = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(5)))? pTopMB->m_pBlock[5]: 0;
    }
    if (pLeftMB)
    {
        pBlocks[2]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(1)))? pLeftMB->m_pBlock[1]: 0;
        pBlocks[7]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(1)))? pLeftMB->m_pBlock[1]: TempBlock.m_pBlock[1];
        pBlocks[8]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(3)))? pLeftMB->m_pBlock[3]: 0;
        pBlocks[14]  = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(4)))? pLeftMB->m_pBlock[4]: 0;
        pBlocks[17]  = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(5)))? pLeftMB->m_pBlock[5]: 0;
    }

    //  Estimation about AC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        switch (((!pBlocks[3*i+0])<<1)|(!pBlocks[3*i+2]))
        {
        case 0:
            if (vc1_abs_32s(pBlocks[3*i+0][0] - pBlocks[3*i+1][0])<= vc1_abs_32s(pBlocks[3*i+2][0] - pBlocks[3*i+1][0]))
            {
                direction[i] = VC1_ENC_LEFT;
                z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            }
            else
            {
                direction[i] = VC1_ENC_TOP;
                z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            }
            break;
        case 1:
            direction[i] = VC1_ENC_TOP;
            z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            break;
        case 2:
            direction[i] = VC1_ENC_LEFT;
            z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            break;
        default:
            direction[i]     = VC1_ENC_LEFT;
            pBlocks[3*i+2]   = TempBlock.m_pBlock[0];
            break;
        }
    }

    //AC, DC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        uint32_t numBlk    = 3*i + 2*(!(direction[i] & VC1_ENC_TOP));
        uint32_t numTable  = (z<0)*direction[i];
        Diff8x8 (pCurrMB->m_pBlock[i],      pCurrMB->m_uiBlockStep[i],
                pBlocks[numBlk],            pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i],    pPredBlock->m_uiBlockStep[i],numTable);

#ifdef VC1_ENC_DEBUG_ON
        if(z<0)
        {
            pDebug->SetBlockAsIntra(i);
            pDebug->SetDCACInfo(1, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                direction[i], i);
        }
        else
        {
            pDebug->SetBlockAsIntra(i);
            pDebug->SetDCACInfo(0, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                direction[i], i);
        }
#endif
    }
    return (z<0);
}

int8_t
DCACPredictionFrame4MVBlockMixed(  VC1EncoderMBData*               pCurrMB,
                                        uint32_t                          currIntraPattern,
                                        NeighbouringMBsData*            pMBs,
                                        NeighbouringMBsIntraPattern*    pMBsIntraPattern,
                                        VC1EncoderMBData*               pPredBlock,
                                        int16_t                          defPredictor,
                                        eDirection*                     direction)
{
    int32_t              z = 0;
    int                 i;
    VC1EncoderMBData    TempBlock;
    int16_t              temp [VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS]={0};
    bool                bPredicted = false;
    bool                PredictedBlocks[VC1_ENC_NUMBER_OF_BLOCKS]={false};


    VC1EncoderMBData* pLeftMB    = pMBs->LeftMB;
    VC1EncoderMBData* pTopMB     = pMBs->TopMB;
    VC1EncoderMBData* pTopLeftMB = (pMBs->TopLeftMB)? pMBs->TopLeftMB:&TempBlock;

    TempBlock.InitBlocks(temp);
    TempBlock.m_pBlock[0][0] = TempBlock.m_pBlock[1][0]= TempBlock.m_pBlock[2][0] =
    TempBlock.m_pBlock[3][0] = TempBlock.m_pBlock[4][0]= TempBlock.m_pBlock[5][0] =
                                                                            defPredictor;
    pTopLeftMB  = (pTopLeftMB)  ? pTopLeftMB:&TempBlock;

    int16_t*             pBlocks[VC1_ENC_NUMBER_OF_BLOCKS*3]=
    {
        0,                                                                          (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(3)))? pTopLeftMB->m_pBlock[3]:TempBlock.m_pBlock[3],  0,                   //0..2
        0,                                                                          0,                                                                                      (currIntraPattern& (1<<VC_ENC_PATTERN_POS(0)))? pCurrMB->m_pBlock[0]:0,//3..5
        (currIntraPattern& (1<<VC_ENC_PATTERN_POS(0)))? pCurrMB->m_pBlock[0]:0,     0,                                                                                      0,                                                                      //6..8
        (currIntraPattern& (1<<VC_ENC_PATTERN_POS(1)))? pCurrMB->m_pBlock[1]:0,    (currIntraPattern& (1<<VC_ENC_PATTERN_POS(0)))? pCurrMB->m_pBlock[0]:TempBlock.m_pBlock[0],                  (currIntraPattern& (1<<VC_ENC_PATTERN_POS(2)))? pCurrMB->m_pBlock[2]:0,//9..11
        0,                                                                         (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(4)))? pTopLeftMB->m_pBlock[4]:TempBlock.m_pBlock[4],  0,                   //12..14
        0,                                                                         (pMBsIntraPattern->TopLeftMB & (1<<VC_ENC_PATTERN_POS(5)))? pTopLeftMB->m_pBlock[5]:TempBlock.m_pBlock[5],  0                    //15..17
    };
    if (pTopMB)
    {
        pBlocks[0]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(2)))? pTopMB->m_pBlock[2]: 0;
        pBlocks[3]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(3)))? pTopMB->m_pBlock[3]: 0;
        pBlocks[4]  = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(2)))? pTopMB->m_pBlock[2]: TempBlock.m_pBlock[2];
        pBlocks[12] = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(4)))? pTopMB->m_pBlock[4]: 0;
        pBlocks[15] = (pMBsIntraPattern->TopMB & (1<<VC_ENC_PATTERN_POS(5)))? pTopMB->m_pBlock[5]: 0;
    }
    if (pLeftMB)
    {
        pBlocks[2]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(1)))? pLeftMB->m_pBlock[1]: 0;
        pBlocks[7]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(1)))? pLeftMB->m_pBlock[1]: TempBlock.m_pBlock[1];
        pBlocks[8]   = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(3)))? pLeftMB->m_pBlock[3]: 0;
        pBlocks[14]  = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(4)))? pLeftMB->m_pBlock[4]: 0;
        pBlocks[17]  = (pMBsIntraPattern->LeftMB & (1<<VC_ENC_PATTERN_POS(5)))? pLeftMB->m_pBlock[5]: 0;
    }

    //  Estimation about AC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        if ((currIntraPattern& (1<<VC_ENC_PATTERN_POS(i)))==0)
            continue;

        switch (((!pBlocks[3*i+0])<<1)|(!pBlocks[3*i+2]))
        {
        case 0:
            if (vc1_abs_32s(pBlocks[3*i+0][0] - pBlocks[3*i+1][0])<= vc1_abs_32s(pBlocks[3*i+2][0] - pBlocks[3*i+1][0]))
            {
                direction[i] = VC1_ENC_LEFT;
                z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            }
            else
            {
                direction[i] = VC1_ENC_TOP;
                z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            }
            PredictedBlocks[i]  = true;
            bPredicted          = true;
            break;
        case 1:
            direction[i] = VC1_ENC_TOP;
            z += SumSqDiff_7x1_16s(pCurrMB->m_pBlock[i], pBlocks[3*i+0]);
            PredictedBlocks[i]  = true;
            bPredicted          = true;
            break;
        case 2:
            direction[i] = VC1_ENC_LEFT;
            z += SumSqDiff_1x7_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],pBlocks[3*i+2]);
            PredictedBlocks[i]  = true;
            bPredicted          = true;
            break;
        default:
            direction[i]     = VC1_ENC_LEFT;
            pBlocks[3*i+2]   = TempBlock.m_pBlock[0];
            break;
        }
    }

    //AC, DC prediction
    for (i = 0; i<VC1_ENC_NUMBER_OF_BLOCKS; i++)
    {
        if (PredictedBlocks[i])
        {
            uint32_t numBlk    = 3*i + 2*(!(direction[i] & VC1_ENC_TOP));
            uint32_t numTable  = (z<0)*direction[i];
            Diff8x8 (pCurrMB->m_pBlock[i],      pCurrMB->m_uiBlockStep[i],
                    pBlocks[numBlk],            pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i],    pPredBlock->m_uiBlockStep[i],numTable);

#ifdef VC1_ENC_DEBUG_ON
            if(z<0)
            {
                //pDebug->SetBlockAsIntra(i);
                pDebug->SetDCACInfo(1, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                    direction[i], i);
            }
            else
            {
                //pDebug->SetBlockAsIntra(i);
                pDebug->SetDCACInfo(0, pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                    direction[i], i);
            }
#endif
        }
        else
        {
            Copy8x8_16s(pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                pPredBlock->m_pBlock[i],    pPredBlock->m_uiBlockStep[i]);

#ifdef VC1_ENC_DEBUG_ON
            pDebug->SetDCACInfo((z < 0), pCurrMB->m_pBlock[i], pCurrMB->m_uiBlockStep[i],
                    pPredBlock->m_pBlock[i], pPredBlock->m_uiBlockStep[i],
                    direction[i], i);
#endif
        }
    }//for

#ifdef VC1_ENC_DEBUG_ON
    if(!bPredicted)
    {
        pDebug->SetACDirection(bPredicted,0);
        pDebug->SetACDirection(bPredicted,1);
        pDebug->SetACDirection(bPredicted,2);
        pDebug->SetACDirection(bPredicted,3);
        pDebug->SetACDirection(bPredicted,4);
        pDebug->SetACDirection(bPredicted,5);
    }
#endif

    return (uint8_t)((bPredicted)?(z<0):-1);
}

}
#endif 
