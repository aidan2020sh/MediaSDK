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

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"


void AssignCodedBlockPattern(VC1MB * pMB,VC1SingletonMB* sMB)
{
    int32_t CBPCY = pMB->m_cbpBits;
    int32_t Count;

    for(Count = 0; Count < VC1_NUM_OF_BLOCKS; Count++)
    {
        VC1Block *pBlk = &pMB->m_pBlocks[Count];
        uint32_t Coded = (0 != (CBPCY & (1 << (5 - Count))));

        sMB->m_pSingleBlock[Count].Coded = (uint8_t)Coded;

        if (!Coded && (pBlk->blkType < VC1_BLK_INTRA_TOP))
            pBlk->blkType = VC1_BLK_INTER8X8;
        pBlk->SBlkPattern = SubBlockPattern(pBlk,&sMB->m_pSingleBlock[Count]);

    }
}
//4.1.2.1    Coded Block Pattern
// ____________________
//|    |    |    |    |
//|    |    | T0 | T1 |
//|____|____|____|____|
//|    |    |    |    |
//|    | LT3| T2 | T3 |
//|___ |____|____|____|
//|    |    |    |    |
//| L0 | L1 | Y0 | Y1 |
//|___ |____|____|____|
//|    |    |    |    |
//| L2 | L3 | Y2 | Y3 |
//|____|____|____|____|
//////////////////////////
//pCBPpredAbove is an 1 dim array for prediction of CBP from above MB
//T3 and T4 for corresponding i th MB are located in
//pCBPpredAbove[2i]   = T3
//pCBPpredAbove[2i+1] = T4
//pCBPpredLeft is an 1 dim array for prediction of CBP from left MB
//2 elements are L2 and L4
//pCBPpredLeft[0] = L2
//pCBPpredLeft[1] = L4
//LT4 could be found in pCBPpredAboveLeft

//For future prediction of the next mbcbp after prediction left
//predictors will be replaced with calculated values
//T4 moved to LT4 and top predictors will be replaced with calculated
//values as well for calculation mbcbp in the same position mb in next row
int32_t CalculateCBP(VC1MB* pCurrMB, uint32_t decoded_cbpy, int32_t width)
{
    int32_t predicted_Y0;
    int32_t predicted_Y1;
    int32_t predicted_Y2;
    int32_t predicted_Y3;

    int32_t calculated_cbpy;

    int32_t LT3;
    int32_t T2 ;
    int32_t T3 ;
    int32_t L1 ;
    int32_t L3 ;

    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;


    if(VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag) && VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        LT3 = ((pCurrMB - width-1)->m_cbpBits & 4) >> 2;
    }
    else
    {
        LT3 = 0;
    }

    if(VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
    {
        T2  = ((pCurrMB - width)->m_cbpBits & 8) >> 3;
        T3  = ((pCurrMB - width)->m_cbpBits & 4) >> 2;
    }
    else
    {
        T2  = 0;
        T3  = 0;
    }

    if(VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
    {
        L1  = ((pCurrMB - 1)->m_cbpBits &16) >> 4;
        L3  = ((pCurrMB - 1)->m_cbpBits & 4) >> 2;
    }
    else
    {
        L1  = 0;
        L3  = 0;
    }

    if(LT3 == T2)
        predicted_Y0 = L1;
    else
        predicted_Y0 = T2;

    predicted_Y0 ^= ((decoded_cbpy >> 5) & 0x01);

    if(T2 == T3)
        predicted_Y1 = predicted_Y0;
    else
        predicted_Y1 = T3;

    predicted_Y1 ^= ((decoded_cbpy >> 4) & 0x01);

    if(L1 == predicted_Y0)
        predicted_Y2 = L3;
    else
        predicted_Y2 = predicted_Y0;

    predicted_Y2 ^= ((decoded_cbpy >> 3) & 0x01);

    if(predicted_Y0 == predicted_Y1)
        predicted_Y3 = predicted_Y2;
    else
        predicted_Y3 = predicted_Y1;

    predicted_Y3 ^= ((decoded_cbpy >> 2) & 0x01);

    calculated_cbpy = (predicted_Y0 << 5) |
                      (predicted_Y1 << 4) |
                      (predicted_Y2 << 3) |
                      (predicted_Y3 << 2) | (decoded_cbpy&3);

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_CBP,
                                            VM_STRING("Read CBPCY: 0x%02X\n"),
                                            decoded_cbpy);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_CBP,
                                            VM_STRING("Predicted CBPCY = 0x%02X\n"),
                                            calculated_cbpy);
#endif

    return calculated_cbpy;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER