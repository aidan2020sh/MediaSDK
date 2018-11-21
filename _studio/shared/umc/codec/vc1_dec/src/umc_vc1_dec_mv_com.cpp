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
#include "umc_vc1_common_mvdiff_tbl.h"

void PackDirectMVProgressive(VC1MB* pCurrMB, int16_t* pSavedMV)
{
    if (VC1_GET_MBTYPE(pCurrMB->mbType) == VC1_MB_1MV_INTER)
    {
        pSavedMV[0] = pCurrMB->m_pBlocks[0].mv[0][0];
        pSavedMV[1] = pCurrMB->m_pBlocks[0].mv[0][1];
    }
    else
    {
        uint32_t blk_num;
        int16_t x[4];
        int16_t y[4];
        for (blk_num = 0; blk_num < 4; blk_num++)
        {
            if(pCurrMB->m_pBlocks[blk_num].blkType & VC1_BLK_INTRA)
                x[blk_num] = VC1_MVINTRA;
            else
            {
                x[blk_num] = pCurrMB->m_pBlocks[blk_num].mv[0][0];
                y[blk_num] = pCurrMB->m_pBlocks[blk_num].mv[0][1];
            }
        }
        CalculateMV(x, y, pSavedMV, &pSavedMV[1]);
    }

}
void PackDirectMVIField(VC1MB* pCurrMB, int16_t* pSavedMV, uint8_t isBottom, uint8_t* bRefField)
{

    if (VC1_MB_INTRA == pCurrMB->mbType)
    {
        pSavedMV[0] = 0;
        pSavedMV[1] = 0;
        bRefField[0] = 1;
    }
    else
    {
        int32_t count=0;
        int16_t* xLuMV;
        int16_t* yLuMV;

        int16_t xLuMVT[VC1_NUM_OF_LUMA];
        int16_t yLuMVT[VC1_NUM_OF_LUMA];

        int16_t xLuMVB[VC1_NUM_OF_LUMA];
        int16_t yLuMVB[VC1_NUM_OF_LUMA];



        uint32_t MVcount = 0;
        uint32_t MVBcount = 0;
        uint32_t MVTcount = 0;

        for (count=0;count<4;count++)
        {

            if (((pCurrMB->m_pBlocks[count].mv_s_polarity[0])&&(isBottom))||
                ((!pCurrMB->m_pBlocks[count].mv_s_polarity[0])&&(!isBottom)))
            {
                xLuMVB[MVBcount] = pCurrMB->m_pBlocks[count].mv[0][0];
                yLuMVB[MVBcount] = pCurrMB->m_pBlocks[count].mv[0][1];
                ++MVBcount;
            }
            else
            {
                xLuMVT[MVTcount] = pCurrMB->m_pBlocks[count].mv[0][0];
                yLuMVT[MVTcount] = pCurrMB->m_pBlocks[count].mv[0][1];
                ++MVTcount;
            }
        }
        if (MVBcount == MVTcount)
        {
            MVcount = 2;
            bRefField[0] = 1;
            if (isBottom)
            {
                xLuMV = xLuMVB;
                yLuMV = yLuMVB;
            } else
            {
                xLuMV = xLuMVT;
                yLuMV = yLuMVT;
            }
        } else if (MVBcount > MVTcount)
        {
            if (!isBottom)
                bRefField[0] = 0;
            else
                bRefField[0] = 1;

            MVcount = MVBcount;
            xLuMV = xLuMVB;
            yLuMV = yLuMVB;
        }
        else
        {
            if (isBottom)
            {
                 bRefField[0] = 0;
            }
            else
            {
                 bRefField[0] = 1;
            }
            MVcount = MVTcount;
            xLuMV = xLuMVT;
            yLuMV = yLuMVT;
        }
        Derive4MV_Field(MVcount,pSavedMV,&pSavedMV[1],xLuMV,yLuMV);
    }
}
void PackDirectMVIFrame(VC1MB* pCurrMB, int16_t* pSavedMV)
{
    if (VC1_MB_INTRA == pCurrMB->mbType)
    {
        pSavedMV[0] = 0;
        pSavedMV[1] = 0;
        pSavedMV[2] = 0;
        pSavedMV[3] = 0;
    }
    else
    {
        pSavedMV[0] = pCurrMB->m_pBlocks[0].mv[0][0];
        pSavedMV[1] = pCurrMB->m_pBlocks[0].mv[0][1];
        pSavedMV[2] = pCurrMB->m_pBlocks[0].mv_bottom[0][0];
        pSavedMV[3] = pCurrMB->m_pBlocks[0].mv_bottom[0][1];

    }

}


void PackDirectMVs(VC1MB*  pCurrMB,
                   int16_t* pSavedMV,
                   uint8_t   isBottom,
                   uint8_t*  bRefField,
                   uint32_t  FCM)
{
    switch (FCM)
    {
    case VC1_Progressive:
        PackDirectMVProgressive(pCurrMB, pSavedMV);
        break;
    case VC1_FieldInterlace:
        PackDirectMVIField(pCurrMB, pSavedMV, isBottom, bRefField);
        break;
    case VC1_FrameInterlace:
        PackDirectMVIFrame(pCurrMB, pSavedMV);
        break;
    default:
        break;
    }
}

void Derive4MV_Field(uint32_t _MVcount,
                     int16_t* xMV, int16_t* yMV,
                     int16_t* xLuMV, int16_t* yLuMV)
{
    switch(_MVcount)
    {
    case 0:
    case 1:
        {
            VM_ASSERT(0);
            return;
        }
    case 2:
        {
            *xMV = (xLuMV[0] + xLuMV[1]) / 2;
            *yMV = (yLuMV[0] + yLuMV[1]) / 2;
            return;
        }
    case 3:
        {
            *xMV = median3(xLuMV);
            *yMV = median3(yLuMV);
            return;
        }
    case 4:
        {
            *xMV = median4(xLuMV);
            *yMV = median4(yLuMV);
            return;
        }
    }
}

void GetPredictProgressiveMV(VC1Block *pA,VC1Block *pB,VC1Block *pC,
                             int16_t *pX, int16_t *pY,int32_t Back)
{
    int16_t X=0,  Y=0;
    int16_t MV_px[] = {0,0,0};
    int16_t MV_py[] = {0,0,0};

    if (pA && (pA->blkType & VC1_BLK_INTER))
    {
        MV_px[0] = pA->mv[Back][0];
        MV_py[0] = pA->mv[Back][1];
    }

    if (pB && (pB->blkType & VC1_BLK_INTER))
    {
        MV_px[1] = pB->mv[Back][0];
        MV_py[1] = pB->mv[Back][1];
    }

    if (pC && (pC->blkType & VC1_BLK_INTER))
    {
        MV_px[2] = pC->mv[Back][0];
        MV_py[2] = pC->mv[Back][1];
    }

    if (pA)
    {
        if (pB == NULL)
        {
            X = MV_px[0];
            Y = MV_py[0];
        }
        else
        {
            X = median3(MV_px);
            Y = median3(MV_py);
        }
    }
    else if (pC)
    {
        X = MV_px[2];
        Y = MV_py[2];
    }

    (*pX) = X;
    (*pY) = Y;
}


void HybridMV(VC1Context* pContext,VC1Block *pA,VC1Block *pC, int16_t *pPredMVx,int16_t *pPredMVy, int32_t Back)
{
    int16_t MV_px[] = {0,0};
    int16_t MV_py[] = {0,0};
    int16_t x,  y;
    uint16_t sum;
    int32_t eHybridPred;

    if ((pA == NULL) || (pC == NULL) || (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME))
    {
        return;
    }

    x = (*pPredMVx);
    y = (*pPredMVy);

    if (pA && (pA->blkType & VC1_BLK_INTER))
    {
        MV_px[0] = pA->mv[Back][0];
        MV_py[0] = pA->mv[Back][1];
    }

    if (pC && (pC->blkType & VC1_BLK_INTER))
    {
        MV_px[1] = pC->mv[Back][0];
        MV_py[1] = pC->mv[Back][1];
    }

    sum = vc1_abs_16s(x-MV_px[0]) + vc1_abs_16s(y-MV_py[0]);
    if (sum <= 32)
    {
        sum = vc1_abs_16s(x-MV_px[1]) + vc1_abs_16s(y-MV_py[1]);
    }
    if (sum <= 32)
    {
        return;
    }

    VC1_GET_BITS(1,eHybridPred );
    if (eHybridPred)
    {
        x = MV_px[0];
        y = MV_py[0];
    }
    else
    {
        x = MV_px[1];
        y = MV_py[1];
    }

    (*pPredMVx)= x;
    (*pPredMVy)= y;
}

void CalculateMV(int16_t x[],int16_t y[], int16_t *X, int16_t* Y)
{
    int16_t temp_x[] = {0,0,0};
    int16_t temp_y[] = {0,0,0};

    uint16_t n_intra = ((uint16_t)(x[0])==VC1_MVINTRA) +
        (((uint16_t)(x[1])== VC1_MVINTRA)<<1) +
        (((uint16_t)(x[2])== VC1_MVINTRA)<<2) +
        (((uint16_t)(x[3])== VC1_MVINTRA)<<3);

    switch(n_intra)
    {
    case 0x00: //all blocks are inter
        *X = median4(x);
        *Y = median4(y);
        break;
    case 0x01:
        *X = median3(&x[1]);
        *Y = median3(&y[1]);
        break;
    case 0x02:
        temp_x[0] = x[0];
        temp_x[1] = x[2];
        temp_x[2] = x[3];
        temp_y[0] = y[0];
        temp_y[1] = y[2];
        temp_y[2] = y[3];
        *X = median3(temp_x);
        *Y = median3(temp_y);
        break;
    case 0x04:
        temp_x[0] = x[0];
        temp_x[1] = x[1];
        temp_x[2] = x[3];
        temp_y[0] = y[0];
        temp_y[1] = y[1];
        temp_y[2] = y[3];
        *X = median3(temp_x);
        *Y = median3(temp_y);
        break;
    case 0x08:
        *X = median3(x);
        *Y = median3(y);
        break;
    case 0x03:
        *X = (x[2]+ x[3])/2;
        *Y = (y[2]+ y[3])/2;
        break;
    case 0x05:
        *X = (x[1]+ x[3])/2;
        *Y = (y[1]+ y[3])/2;
        break;
    case 0x06:
        *X = (x[0]+ x[3])/2;
        *Y = (y[0]+ y[3])/2;
        break;
    case 0x09:
        *X = (x[1]+ x[2])/2;
        *Y = (y[1]+ y[2])/2;
        break;
    case 0x0C:
        *X = (x[0]+ x[1])/2;
        *Y = (y[0]+ y[1])/2;
        break;
    case 0x0A:
        *X = (x[0]+ x[2])/2;
        *Y = (y[0]+ y[2])/2;
        break;

    default:
        (*X)=VC1_MVINTRA;
        (*Y)=VC1_MVINTRA;
        break;
    }
}
void CalculateMV_Interlace(int16_t x[],int16_t y[], int16_t x_bottom[],int16_t y_bottom[],int16_t *Xt, int16_t* Yt,int16_t *Xb, int16_t* Yb )
{
    int8_t n_intra = ((uint16_t)(x[0])==VC1_MVINTRA);
    if (n_intra) // intra co-located MB
    {
        *Xt = 0;
        *Yt = 0;
        *Xb = 0;
        *Yb = 0;
    }
    else
    {
        *Xt = x[0];
        *Yt = y[0];
        *Xb = x_bottom[0];
        *Yb = y_bottom[0];
    }

}

void CalculateMV_InterlaceField(VC1Context* pContext, int16_t *X, int16_t* Y)
{
    uint8_t* samePolarity = pContext->savedMVSamePolarity_Curr +
                          (pContext->m_seqLayerHeader.MaxWidthMB*pContext->m_pSingleMB->m_currMBYpos
                           + pContext->m_pSingleMB->m_currMBXpos);

    VC1MB* pMB = pContext->m_pCurrMB;
    pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = pContext->m_picLayerHeader->BottomField;

    if (*X == VC1_MVINTRA)
    {
        *X = 0;
        *Y = 0;
        pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0] = 1;
        pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[1] = 1;
        pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = pContext->m_picLayerHeader->BottomField;
    }
    else
    {
        if(samePolarity[0])
        {
            if (pContext->m_picLayerHeader->BottomField)
            {
                pMB->m_pBlocks[0].mv_s_polarity[0] = 1;
                pMB->m_pBlocks[0].mv_s_polarity[1] = 1;
                pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = 1;
            }
            else
            {
                pMB->m_pBlocks[0].mv_s_polarity[0] = 1;
                pMB->m_pBlocks[0].mv_s_polarity[1] = 1;
                pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = 0;

            }

        }
        else
        {
            if (pContext->m_picLayerHeader->BottomField)
            {
                pMB->m_pBlocks[0].mv_s_polarity[0] = 0;
                pMB->m_pBlocks[0].mv_s_polarity[1] = 0;
                pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = 0;
            }
            else
            {
                pMB->m_pBlocks[0].mv_s_polarity[0] = 0;
                pMB->m_pBlocks[0].mv_s_polarity[1] = 0;
                pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0] = 1;

            }
        }

    }
     pContext->m_pCurrMB->m_pBlocks[1].mv_s_polarity[0] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];
     pContext->m_pCurrMB->m_pBlocks[2].mv_s_polarity[0] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];
     pContext->m_pCurrMB->m_pBlocks[3].mv_s_polarity[0] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];
     pContext->m_pCurrMB->m_pBlocks[1].mv_s_polarity[1] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];
     pContext->m_pCurrMB->m_pBlocks[2].mv_s_polarity[1] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];
     pContext->m_pCurrMB->m_pBlocks[3].mv_s_polarity[1] = pContext->m_pCurrMB->m_pBlocks[0].mv_s_polarity[0];

     pContext->m_pCurrMB->m_pBlocks[1].fieldFlag[0] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];
     pContext->m_pCurrMB->m_pBlocks[2].fieldFlag[0] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];
     pContext->m_pCurrMB->m_pBlocks[3].fieldFlag[0] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];
     pContext->m_pCurrMB->m_pBlocks[1].fieldFlag[1] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];
     pContext->m_pCurrMB->m_pBlocks[2].fieldFlag[1] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];
     pContext->m_pCurrMB->m_pBlocks[3].fieldFlag[1] =  pContext->m_pCurrMB->m_pBlocks[0].fieldFlag[0];

}

void Decode_BMVTYPE(VC1Context* pContext)
{
    int32_t value=0;
    VC1_GET_BITS(1, value);
    if (value)
    {
        VC1_GET_BITS(1, value);
        if (value)
        {
            pContext->m_pCurrMB->mbType=VC1_MB_1MV_INTER|VC1_MB_INTERP;
        }
        else
        {
            pContext->m_pCurrMB->mbType = (pContext->m_picLayerHeader->BFRACTION)?
                VC1_MB_1MV_INTER|VC1_MB_FORWARD:VC1_MB_1MV_INTER|VC1_MB_BACKWARD;
        }
    }
    else
    {
        pContext->m_pCurrMB->mbType=(pContext->m_picLayerHeader->BFRACTION)?
            VC1_MB_1MV_INTER|VC1_MB_BACKWARD:VC1_MB_1MV_INTER|VC1_MB_FORWARD;
    }
}

void PullBack_PPred4MV(VC1SingletonMB* sMB, int16_t *pMVx, int16_t* pMVy, int32_t blk_num)
{
    int32_t Min=-28;
    int32_t X = *pMVx;
    int32_t Y = *pMVy;
    int32_t currMBXpos = sMB->m_currMBXpos<<6;
    int32_t currMBYpos = sMB->m_currMBYpos<<6;

    uint32_t Xblk = ((blk_num&1) << 5);
    uint32_t Yblk = ((blk_num&2) << 4);

    int32_t IX = currMBXpos + X + Xblk;
    int32_t IY = currMBYpos + Y + Yblk;

    int32_t Width  =(sMB->widthMB<<6) - 4;
    int32_t Height =(sMB->heightMB<<6) - 4;


    if (IX < Min)
    {
        X = Min - currMBXpos - Xblk;
    }
    else if (IX > Width)
    {
        X = Width - currMBXpos - Xblk;
    }

    if (IY < Min)
    {
        Y = Min - currMBYpos - Yblk;
    }
    else if (IY > Height)
    {
        Y = Height - currMBYpos - Yblk;
    }

    (*pMVx) = (int16_t)X;
    (*pMVy) = (int16_t)Y;
}

void Progressive1MVPrediction(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1MB *pA = NULL, *pB = NULL, *pC = NULL;

    uint32_t LeftTopRight = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_seqLayerHeader.MaxWidthMB;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    if(LeftTopRight == VC1_COMMON_MB)
    {
        //all predictors are available
        pA = pCurrMB - width;
        pB = pCurrMB - width + 1;
        pC = pCurrMB - 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pB->m_pBlocks[2];
        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_TOP_MB(LeftTopRight))
    {
        //A and B predictors are unavailable
        pC = pCurrMB - 1;

        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_LEFT_MB(LeftTopRight))
    {
        //C predictor is unavailable
        pA = pCurrMB - width;
        pB = pCurrMB - width + 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pB->m_pBlocks[2];
    }
    else if(VC1_IS_RIGHT_MB(LeftTopRight))
    {
        //all predictors are available
        pA = pCurrMB - width;
        pB = pCurrMB - width - 1;
        pC = pCurrMB - 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pB->m_pBlocks[3];
        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if(VC1_IS_TOP_RIGHT_MB(LeftTopRight))
    {
        pC = pCurrMB - 1;

        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
    }
    else if (VC1_IS_LEFT_RIGHT_MB(LeftTopRight))
    {
        pA = pCurrMB - width;
        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
    }
    memcpy_s(&pContext->MVPred,sizeof(VC1MVPredictors),&MVPred,sizeof(VC1MVPredictors));
}


void Progressive4MVPrediction(VC1Context* pContext)
{
    VC1MVPredictors MVPred;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1MB *pA = NULL, *pB0 = NULL,*pB1 = NULL, *pC = NULL;

    uint32_t LeftTopRight = pCurrMB->LeftTopRightPositionFlag;
    int32_t width = pContext->m_seqLayerHeader.MaxWidthMB;

    memset(&MVPred,0,sizeof(VC1MVPredictors));

    if(LeftTopRight == VC1_COMMON_MB)
    {
        //all predictors are available
        pA  = pCurrMB - width;
        pB0 = pCurrMB - width - 1;
        pB1 = pCurrMB - width + 1;
        pC  = pCurrMB - 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pB0->m_pBlocks[3];
        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
        MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        MVPred.BMVPred[1] = &pB1->m_pBlocks[2];
        MVPred.CMVPred[2] = &pC->m_pBlocks[3];
    }
    else if(VC1_IS_TOP_MB(LeftTopRight))
    {
        pC = pCurrMB - 1;

        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
        MVPred.CMVPred[2] = &pC->m_pBlocks[3];
    }
    else if(VC1_IS_LEFT_MB(LeftTopRight))
    {
        pA = pCurrMB - width;
        pB1 = pCurrMB - width + 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pA->m_pBlocks[3];
        MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        MVPred.BMVPred[1] = &pB1->m_pBlocks[2];
    }
    else if (VC1_IS_RIGHT_MB(LeftTopRight))
    {
        pA = pCurrMB - width;
        pB0 = pCurrMB - width - 1;
        pC = pCurrMB - 1;

        MVPred.AMVPred[0] = &pA->m_pBlocks[2];
        MVPred.BMVPred[0] = &pB0->m_pBlocks[3];
        MVPred.CMVPred[0] = &pC->m_pBlocks[1];
        MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        MVPred.BMVPred[1] = &pA->m_pBlocks[2];
        MVPred.CMVPred[2] = &pC->m_pBlocks[3];
    }
    else
    {
        if(VC1_IS_TOP_RIGHT_MB(LeftTopRight))
        {
            pC = pCurrMB - 1;

            MVPred.CMVPred[0] = &pC->m_pBlocks[1];
            MVPred.CMVPred[2] = &pC->m_pBlocks[3];
        }
        else if (VC1_IS_LEFT_RIGHT_MB(LeftTopRight))
        {
            pA = pCurrMB - width;

            MVPred.AMVPred[0] = &pA->m_pBlocks[2];
            MVPred.BMVPred[0] = &pA->m_pBlocks[3];
            MVPred.BMVPred[1] = &pA->m_pBlocks[2];
            MVPred.AMVPred[1] = &pA->m_pBlocks[3];
        }
    }

    MVPred.CMVPred[1] = &pCurrMB->m_pBlocks[0];
    MVPred.AMVPred[2] = &pCurrMB->m_pBlocks[0];
    MVPred.BMVPred[3] = &pCurrMB->m_pBlocks[0];

    MVPred.BMVPred[2] = &pCurrMB->m_pBlocks[1];
    MVPred.AMVPred[3] = &pCurrMB->m_pBlocks[1];

    MVPred.CMVPred[3] = &pCurrMB->m_pBlocks[2];

    memcpy_s(&pContext->MVPred,sizeof(VC1MVPredictors),&MVPred,sizeof(VC1MVPredictors));
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
