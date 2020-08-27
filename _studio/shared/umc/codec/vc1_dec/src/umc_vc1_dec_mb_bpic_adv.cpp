// Copyright (c) 2004-2019 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common_interlace_mb_mode_tables.h"
#include "umc_vc1_common_zigzag_tbl.h"
#include "umc_vc1_huffman.h"

typedef VC1Status (*B_MB_DECODE)(VC1Context* pContext);
typedef void (*DCPrediction)(VC1Context* pContext);
static const DCPrediction PDCPredictionTable[] =
{
        (DCPrediction)(GetPDCPredictors),
        (DCPrediction)(GetPScaleDCPredictors),
        (DCPrediction)(GetPScaleDCPredictors)
};


inline static void writeMV(VC1MB* pCurrMB,int16_t Xt, int16_t Yt,int16_t Xb, int16_t Yb, int32_t back)
{
    int32_t blk_num;

    for (blk_num =0; blk_num <4; blk_num ++)
    {
        pCurrMB->m_pBlocks[blk_num].mv[back][0] = Xt;
        pCurrMB->m_pBlocks[blk_num].mv[back][1] = Yt;

        pCurrMB->m_pBlocks[blk_num].mv_bottom[back][0] = Xb;
        pCurrMB->m_pBlocks[blk_num].mv_bottom[back][1] = Yb;
    }

    pCurrMB->fieldFlag[back] = pCurrMB->m_pBlocks[0].fieldFlag[back];
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("Back = %d\n"), back);
#endif
}
inline static void writeMV_w_predict(VC1Context* pContext,
                                     int16_t Xt, int16_t Yt,
                                     int16_t Xb, int16_t Yb,
                                     int32_t back,
                                     uint8_t predictor)
{
    int32_t blk_num;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    for (blk_num =0; blk_num <4; blk_num ++)
    {
        pCurrMB->m_pBlocks[blk_num].mv[back][0] = Xt;
        pCurrMB->m_pBlocks[blk_num].mv[back][1] = Yt;

        pCurrMB->m_pBlocks[blk_num].mv_bottom[back][0] = Xb;
        pCurrMB->m_pBlocks[blk_num].mv_bottom[back][1] = Yb;
        pCurrMB->m_pBlocks[blk_num].mv_s_polarity[back]    = 1- predictor;
        pCurrMB->m_pBlocks[blk_num].fieldFlag[back] = pCurrMB->fieldFlag[back];
    }
}
static VC1Status MBLayer_InterlaceFieldBpicture4MV_Decode(VC1Context* pContext)
{
    int32_t blk_num;
    int32_t BlkMVP;
    int32_t back =0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    if (VC1_GET_PREDICT(pCurrMB->mbType) == VC1_MB_BACKWARD)
        back = 1;


    for( blk_num = 0;  blk_num < 4;  blk_num++)
    {
        pCurrMB->predictor_flag[blk_num] = 0;
        BlkMVP = ((0 != ( (1 << (3 - blk_num)) & pCurrMB->MVBP) ) ? 1 : 0);
        if (BlkMVP)
            pCurrMB->predictor_flag[blk_num] = DecodeMVDiff_TwoReferenceField_Adv(pContext,
                                                                                  &pCurrMB->dmv_x[back][blk_num],
                                                                                  &pCurrMB->dmv_y[back][blk_num]);
        else
        {
            pCurrMB->dmv_x[back][blk_num] = 0;
            pCurrMB->dmv_y[back][blk_num] = 0;
        }
    }

    return VC1_OK;
}


static VC1Status MBLayer_InterlaceFieldBpicture4MV_Prediction(VC1Context* pContext)
{
    int32_t blk_num;
    int16_t X =0;
    int16_t Y = 0;
    int32_t back =0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    Field4MVPrediction(pContext);


    if (VC1_GET_PREDICT(pCurrMB->mbType) == VC1_MB_BACKWARD)
        back = 1;


    for( blk_num = 0;  blk_num < 4;  blk_num++)
    {
        CalculateField4MVTwoReferenceBPic(pContext, &X,&Y,blk_num,back,&pCurrMB->predictor_flag[blk_num]);
        ApplyMVPredictionCalculateTwoReference(picLayerHeader,&X,&Y,
                                               pCurrMB->dmv_x[back][blk_num],
                                               pCurrMB->dmv_y[back][blk_num],
                                               pCurrMB->predictor_flag[blk_num]);


        pCurrMB->m_pBlocks[blk_num].mv[back][0] = X;
        pCurrMB->m_pBlocks[blk_num].mv[back][1] = Y;
        pCurrMB->m_pBlocks[blk_num].mv_s_polarity[back] = 1 - pCurrMB->predictor_flag[blk_num];

#ifdef VC1_DEBUG_ON
        VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
#endif
    }

    return VC1_OK;
}



static VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvDecode(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    pCurrMB->m_cbpBits = 0;
    Decode_BMVTYPE(pContext); // FORWARD, BACKWARD OR INTER
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
    {
        pCurrMB->m_pBlocks[i].blkType= VC1_BLK_INTER8X8;
    }
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvPrediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    switch (pCurrMB->mbType)
    {
    case (VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,0);

        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 0);
        // backward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf,Yf,Xb=0,Yb=0;
            int16_t* savedMV = pContext->savedMV_Curr +
                (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xb,&Yb,-1);
            }

            for (i=0;i<4;i++)
            {
                pCurrMB->m_pBlocks[i].mv[1][0]=Xb;
                pCurrMB->m_pBlocks[i].mv[1][1]=Yb;
            }
        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_BACKWARD):

        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,1);

        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 1);
        // forward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf=0,Yf=0,Xb,Yb;
            int16_t* savedMV = pContext->savedMV_Curr +
                (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xf,&Yf,-1);
            }
            for (i=0;i<4;i++)
            {
                pCurrMB->m_pBlocks[i].mv[0][0]=Xf;
                pCurrMB->m_pBlocks[i].mv[0][1]=Yf;
            }
        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_INTERP):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,0);

        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 0);
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,1);

        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 1);
        break;
    }
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvDecode(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    pCurrMB->m_cbpBits = 0;
    pCurrMB->mbType= VC1_MB_1MV_INTER|VC1_MB_DIRECT;
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvPrediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    int32_t i;
    int16_t Xf,Yf,Xb,Yb;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int16_t* savedMV = pContext->savedMV_Curr +  (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos
        + sMB->m_currMBXpos)*2*2;

     X = savedMV[0];
     Y = savedMV[1];

    pCurrMB->m_cbpBits = 0;
    pCurrMB->mbType= VC1_MB_1MV_INTER|VC1_MB_DIRECT;
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)picLayerHeader->TTFRM;

    if ((uint16_t)X!=VC1_MVINTRA)
    {
        Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
        PullBack_PPred(pContext, &Xf,&Yf,-1);
        PullBack_PPred(pContext, &Xb,&Yb,-1);
    }
    else
    {
        Xf=0;
        Yf=0;
        Xb=0;
        Yb=0;
    }
    for (i=0;i<4;i++)
    {
        pCurrMB->m_pBlocks[i].mv[0][0]=Xf;
        pCurrMB->m_pBlocks[i].mv[0][1]=Yf;
        pCurrMB->m_pBlocks[i].mv[1][0]=Xb;
        pCurrMB->m_pBlocks[i].mv[1][1]=Yb;
    }
    return VC1_OK;
}

static VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_AdvDecode(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int16_t tmp_dx = 0,    tmp_dy = 0;
    uint16_t last_intra_flag = 0;
    //uint8_t not_last=0,intra_flag=0;
    int32_t i;
    int32_t ret = 0;

    int16_t hpelfl = (int16_t)((picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV) ||
        (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV));

    pCurrMB->dmv_x[0][0] = 0;
    pCurrMB->dmv_y[0][0] = 0;
    pCurrMB->dmv_x[1][0] = 0;
    pCurrMB->dmv_y[1][0] = 0;


    // decodes BMV1 (variable size)
    last_intra_flag = DecodeMVDiff(pContext,hpelfl,&tmp_dx,&tmp_dy);
    pCurrMB->m_cbpBits = (!(last_intra_flag&0x10))?0:pCurrMB->m_cbpBits;

    if (last_intra_flag&0x1)
    {
        pCurrMB->mbType = VC1_MB_INTRA;
        for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
            pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTRA;
        if ((!(last_intra_flag&0x10))&&
            picLayerHeader->m_PQuant_mode >= VC1_ALTPQUANT_MB_LEVEL)
        {
            GetMQUANT(pContext);
        }
        VC1_GET_BITS(1, sMB->ACPRED);

    }
    else //non intra
    {
        int32_t value;
        // BMVTYPE (variable size)
        Decode_BMVTYPE(pContext);
        value = (pCurrMB->mbType==(VC1_MB_1MV_INTER|VC1_MB_FORWARD))? 0:1;

        pCurrMB->dmv_x[value][0] = tmp_dx * (1+hpelfl);
        pCurrMB->dmv_y[value][0] = tmp_dy * (1+hpelfl);


        for (i=0;i<4;i++)
        {
            pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTER8X8;
        }

        pCurrMB->m_pBlocks[4].blkType = VC1_BLK_INTER8X8;
        pCurrMB->m_pBlocks[5].blkType = VC1_BLK_INTER8X8;

        if ((pCurrMB->mbType==(VC1_MB_1MV_INTER|VC1_MB_INTERP))&&(last_intra_flag&0x10))
        {
            // decodes BMV2 (variable size)
            last_intra_flag = DecodeMVDiff(pContext,hpelfl,&tmp_dx,&tmp_dy);

            pCurrMB->m_cbpBits = (last_intra_flag&0x10)?0:pCurrMB->m_cbpBits;
            pCurrMB->dmv_x[0][0] = tmp_dx * (1+hpelfl);
            pCurrMB->dmv_y[0][0] = tmp_dy * (1+hpelfl);
        }
    }//non intra

    if (last_intra_flag&0x10) //intra or inter
    {
        //CBPCY
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &pCurrMB->m_cbpBits,
            picLayerHeader->m_pCurrCBPCYtbl);

        VM_ASSERT(ret == 0);

        //MQUANT
        if (picLayerHeader->m_PQuant_mode>=VC1_ALTPQUANT_MB_LEVEL)
        {
            GetMQUANT(pContext);
        }
        DecodeTransformInfo(pContext);
    } // not last (intra or inter)
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_AdvPrediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;

    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    switch (pCurrMB->mbType)
    {
    case (VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,0);

        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[0][0], pCurrMB->dmv_y[0][0], 0);
        // backward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf,Yf,Xb=0,Yb=0;

            int16_t* savedMV = pContext->savedMV_Curr +
                (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];


            if ((uint16_t)X!=VC1_MVINTRA)
            {
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xb,&Yb,-1);
            }

            for (i=0;i<4;i++)
            {
                pCurrMB->m_pBlocks[i].mv[1][0]=Xb;
                pCurrMB->m_pBlocks[i].mv[1][1]=Yb;
            }
        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_BACKWARD):

        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,1);

        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[1][0], pCurrMB->dmv_y[1][0], 1);
        // forward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf=0,Yf=0,Xb=0,Yb=0;
            int16_t* savedMV = pContext->savedMV_Curr +
                (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xf,&Yf,-1);
            }

            for (i=0;i<4;i++)
            {
                pCurrMB->m_pBlocks[i].mv[0][0]=Xf;
                pCurrMB->m_pBlocks[i].mv[0][1]=Yf;
            }
        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_INTERP):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,0);
        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[0][0], pCurrMB->dmv_y[0][0],0);

        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B_Adv(pContext,&X,&Y,1);
        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[1][0], pCurrMB->dmv_y[1][0], 1);
        break;
    }
    return VC1_OK;
}

static VC1Status MBLayer_ProgressiveBpicture_DIRECT_AdvDecode(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    int32_t i;
    int32_t ret = 0;

    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)picLayerHeader->TTFRM;
    pCurrMB->mbType=(VC1_MB_1MV_INTER|VC1_MB_DIRECT);

    //CBPCY
    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &pCurrMB->m_cbpBits,
        picLayerHeader->m_pCurrCBPCYtbl);

    VM_ASSERT(ret == 0);

    //MQUANT
    if (picLayerHeader->m_PQuant_mode >= VC1_ALTPQUANT_MB_LEVEL)
        GetMQUANT(pContext);

    DecodeTransformInfo(pContext);
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_DIRECT_AdvPrediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    int16_t* savedMV = pContext->savedMV_Curr +
                (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

    X = savedMV[0];
    Y = savedMV[1];

    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int16_t Xf,Yf,Xb,Yb;

    if ((uint16_t)X!=VC1_MVINTRA)
    {
        Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
        PullBack_PPred(pContext, &Xf,&Yf,-1);//_MAYBE_ mismatch number of parameters
        PullBack_PPred(pContext, &Xb,&Yb,-1);//_MAYBE_ mismatch number of parameters
    }
    else
    {
        Xf=0;
        Yf=0;
        Xb=0;
        Yb=0;
    }
    for (i=0;i<4;i++)
    {
        pCurrMB->m_pBlocks[i].mv[0][0]=Xf;
        pCurrMB->m_pBlocks[i].mv[0][1]=Yf;
        pCurrMB->m_pBlocks[i].mv[1][0]=Xb;
        pCurrMB->m_pBlocks[i].mv[1][1]=Yb;
    }
    return VC1_OK;
}

static const B_MB_DECODE B_MB_Dispatch_table[] = {
        (B_MB_DECODE)(MBLayer_ProgressiveBpicture_NONDIRECT_AdvDecode),
        (B_MB_DECODE)(MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_AdvDecode),
};

//vc-1: Figure 25: Syntax diagram for macroblock layer bitstream in
//Progressive-coded B picture
VC1Status MBLayer_ProgressiveBpicture_Adv(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    int32_t DIRECTBBIT=0;
    int32_t SKIPMBBIT=0;
    int32_t blk_num;
    VC1Status vc1Res = VC1_OK;
    pCurrMB->m_cbpBits = 0;
    sMB ->m_ubNumFirstCodedBlk = 0;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,VM_STRING("\t\t\tX: %d, Y: %d\n"),
        sMB->m_currMBXpos, sMB ->m_currMBYpos);
#endif
    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant(pContext);
    pCurrMB->FIELDTX = 0;
    // DIRECTBBIT
    if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->m_DirectMB))
    {
        VC1_GET_BITS(1, DIRECTBBIT);
    }
    else
    {
        DIRECTBBIT = picLayerHeader->m_DirectMB.m_databits
            [pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos];

    }
    // SKIPMBBIT
    if(VC1_IS_BITPLANE_RAW_MODE((&picLayerHeader->SKIPMB)))
    {
        VC1_GET_BITS(1, SKIPMBBIT);
    }
    else
    {
        SKIPMBBIT = picLayerHeader->SKIPMB.m_databits
            [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +  sMB->m_currMBXpos];
    }

    pCurrMB->SkipAndDirectFlag = (DIRECTBBIT+(SKIPMBBIT<<1));

    if (!DIRECTBBIT)
        B_MB_Dispatch_table[SKIPMBBIT](pContext);
    else if (!SKIPMBBIT)
        MBLayer_ProgressiveBpicture_DIRECT_AdvDecode(pContext);
    else
        MBLayer_ProgressiveBpicture_SKIP_DIRECT_AdvDecode(pContext);

    CalculateIntraFlag(pContext);

      //Y
    pCurrMB->currYPitch = sMB->currYPitch;
    pCurrMB->currYPlane = sMB->currYPlane + pCurrMB->currYPitch * (sMB->m_currMBYpos << 4)//*VC1_PIXEL_IN_LUMA
                                          + (sMB->m_currMBXpos << 4); //*VC1_PIXEL_IN_LUMA;

    //U
    pCurrMB->currUPitch = sMB->currUPitch;
    pCurrMB->currUPlane = sMB->currUPlane   + pCurrMB->currUPitch*(sMB->m_currMBYpos << 3) // * VC1_PIXEL_IN_CHROMA
                                            + (sMB->m_currMBXpos << 3); //* VC1_PIXEL_IN_CHROMA;

    //V
    pCurrMB->currVPitch = sMB->currVPitch;
    pCurrMB->currVPlane = sMB->currVPlane + pCurrMB->currVPitch*(sMB->m_currMBYpos << 3) // * VC1_PIXEL_IN_CHROMA
                                          + (sMB->m_currMBXpos << 3); //* VC1_PIXEL_IN_CHROMA;

    if(SKIPMBBIT == 0)
    {
        memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);
        if(pCurrMB->IntraFlag)
            PDCPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);

        sMB->ZigzagTable = AdvZigZagTables_PBProgressive_luma[sMB->ACPRED];
        if(pCurrMB->m_pBlocks[0].blkType & VC1_BLK_INTER)
        {
            //luma
            for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
            {
                vc1Res = BLKLayer_Inter_Luma_Adv(pContext, blk_num);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
            //chroma
            sMB->ZigzagTable = AdvZigZagTables_PBProgressive_chroma[sMB->ACPRED];

            for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
            {
                vc1Res = BLKLayer_Inter_Chroma_Adv(pContext, blk_num);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
        }
        else
        {
            for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
            {
                //luma
                vc1Res = BLKLayer_Intra_Luma_Adv(pContext, blk_num, sMB->ACPRED);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }

            //chroma
            sMB->ZigzagTable = AdvZigZagTables_PBProgressive_chroma[sMB->ACPRED];
            for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
            {
                vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, blk_num, sMB->ACPRED);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
        }
    }
    AssignCodedBlockPattern(pCurrMB,sMB);
    return vc1Res;
}


static VC1Status MBLayer_InterlaceFrameBpicture_SKIP_NONDIRECT_Decode(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    pCurrMB->m_cbpBits = 0;
    pCurrMB->FIELDTX = 0;
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
    {
        pCurrMB->m_pBlocks[i].blkType= VC1_BLK_INTER8X8;
    }

    Decode_InterlaceFrame_BMVTYPE(pContext); // FORWARD, BACKWARD OR INTER

    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
    {
        pCurrMB->m_pBlocks[i].blkType= VC1_BLK_INTER8X8;
    }
    return VC1_OK;
}

VC1Status MBLayer_InterlaceFrameBpicture_SKIP_NONDIRECT_Prediction(VC1Context* pContext)
{
    int16_t fX = 0, fY = 0, bX = 0, bY = 0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    PredictInterlaceFrame1MV(pContext);
    switch (pCurrMB->mbType)
    {
    case(VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        {
            CalculateInterlaceFrame1MV_B(&pContext->MVPred, &fX, &fY, &bX, &bY, 0);
            writeMV(pCurrMB,fX,fY,fX,fY,0);
            writeMV(pCurrMB,bX,bY,bX,bY,1);
        }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_BACKWARD):
        {
            CalculateInterlaceFrame1MV_B(&pContext->MVPred, &fX, &fY, &bX, &bY, 1);
            writeMV(pCurrMB,fX,fY,fX,fY,1);
            writeMV(pCurrMB,bX,bY,bX,bY,0);
      }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_INTERP):
        {
            CalculateInterlaceFrame1MV_B_Interpolate(&pContext->MVPred, &fX, &fY, &bX, &bY);
            writeMV(pCurrMB,fX,fY,fX,fY,0);
            writeMV(pCurrMB,bX,bY,bX,bY,1);

        }
        break;
    }
    return VC1_OK;

}

static VC1Status MBLayer_InterlaceFrameBpicture_SKIP_DIRECT_Decode(VC1Context* pContext)
{
    int32_t blk_num;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    pCurrMB->FIELDTX = 0;
    pCurrMB->m_cbpBits = 0;
    pCurrMB->mbType= VC1_MB_1MV_INTER|VC1_MB_DIRECT;

    for (blk_num=0;blk_num<VC1_NUM_OF_BLOCKS;blk_num++)
        pCurrMB->m_pBlocks[blk_num].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;
    return VC1_OK;
}
VC1Status MBLayer_InterlaceFrameBpicture_SKIP_DIRECT_Prediction(VC1Context* pContext)
{
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    int16_t* savedMV = pContext->savedMV_Curr
                      + (pContext->m_seqLayerHeader.MaxWidthMB* sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

    int16_t Xt,Yt, Xb, Yb;
    int16_t Xtopf,Ytopf,Xbottomf,Ybottomf;
    int16_t Xtopb,Ytopb,Xbottomb,Ybottomb;

    Xt = (savedMV[0] == VC1_MVINTRA)?0:savedMV[0];
    Yt = (savedMV[1] == VC1_MVINTRA)?0:savedMV[1];
    Xb = (savedMV[2] == VC1_MVINTRA)?0:savedMV[2];
    Yb = (savedMV[3] == VC1_MVINTRA)?0:savedMV[3];

    Scale_Direct_MV_Interlace(pContext->m_picLayerHeader,Xt,Yt,&Xtopf,&Ytopf,&Xtopb,&Ytopb);
    Scale_Direct_MV_Interlace(pContext->m_picLayerHeader,Xb,Yb,&Xbottomf,&Ybottomf,&Xbottomb,&Ybottomb);

    writeMV(pCurrMB,Xtopf,Ytopf,Xbottomf,Ybottomf,0);
    writeMV(pCurrMB,Xtopb,Ytopb,Xbottomb,Ybottomb,1);
    return VC1_OK;
}
static VC1Status MBLayer_InterlaceFrameBpicture_NONDIRECT_Decode(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t MVBP2 = 0;
    int32_t MVBP4 = 0;

    int32_t MVSW = 0;
    pCurrMB->MVSW = 0;
    int32_t i;
    int32_t ret = 0;

    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;

    Decode_InterlaceFrame_BMVTYPE(pContext); // FORWARD, BACKWARD OR INTER

    if((VC1_IS_MVFIELD(pCurrMB->mbType)) && (VC1_GET_PREDICT(pCurrMB->mbType)!=VC1_MB_INTERP))
    {
        //MVSW
        //If MVSW == 1, then it shall indicate that the MV type and
        //prediction type changes from forward to backward (or backward
        //to forward) in going from the top to the bottom field. If MVSW == 0,
        //then the prediction type shall not change in going from the top
        //to the bottom field.
        VC1_GET_BITS(1, MVSW);
        pCurrMB->MVSW = MVSW;
    }

    if(VC1_MB_Mode_PBPic_Transform_Table[sMB->MBMODEIndex]!= VC1_NO_CBP_TRANSFORM)
    {
        //CBPCY
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                    &pContext->m_bitstream.bitOffset,
                                    &pCurrMB->m_cbpBits,
                                    picLayerHeader->m_pCurrCBPCYtbl);
        VM_ASSERT(ret == 0);
    }
    else
    {
        pCurrMB->m_cbpBits = 0;
    }


    if(((VC1_IS_MVFIELD(pCurrMB->mbType)) &&(VC1_GET_PREDICT(pCurrMB->mbType)!=VC1_MB_INTERP))
        || (((VC1_GET_MBTYPE(pCurrMB->mbType) == VC1_MB_1MV_INTER)
                    && (VC1_GET_PREDICT(pCurrMB->mbType)==VC1_MB_INTERP))))
    {
        //2MVBP
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                        &pContext->m_bitstream.bitOffset,
                                        &MVBP2,
                                        picLayerHeader->m_pMV2BP);

        VM_ASSERT(ret == 0);
    }
    else if((VC1_IS_MVFIELD(pCurrMB->mbType))&& (pCurrMB->mbType&VC1_MB_INTERP))
    {
        //4MVBP
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &MVBP4,
            picLayerHeader->m_pMV4BP);

        VM_ASSERT(ret == 0);
    }


    //PredictInterlaceFrame1MV(pContext);

    switch (pCurrMB->mbType)
    {
    case(VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        {
            pCurrMB->dmv_x[0][0] = 0;
            pCurrMB->dmv_y[0][0] = 0;
            if(VC1_MB_Mode_PBPic_MVPresent_Table[sMB->MBMODEIndex])
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][0],&pCurrMB->dmv_y[0][0]);

        }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_BACKWARD):
        {
            pCurrMB->dmv_x[1][0] = 0;
            pCurrMB->dmv_y[1][0] = 0;

            if(VC1_MB_Mode_PBPic_MVPresent_Table[sMB->MBMODEIndex])
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][0],&pCurrMB->dmv_y[1][0]);
        }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_INTERP):
        {
            pCurrMB->dmv_x[0][0] = 0;
            pCurrMB->dmv_y[0][0] = 0;
            pCurrMB->dmv_x[1][0] = 0;
            pCurrMB->dmv_y[1][0] = 0;


            if (MVBP2 & 2)
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][0],&pCurrMB->dmv_y[0][0]);
            if (MVBP2 & 1)
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][0],&pCurrMB->dmv_y[1][0]);
        }
        break;

    case(VC1_MB_2MV_INTER|VC1_MB_FORWARD):
        {


            pCurrMB->dmv_x[0][0] = 0;
            pCurrMB->dmv_y[0][0] = 0;
            pCurrMB->dmv_x[0][1] = 0;
            pCurrMB->dmv_y[0][1] = 0;

            if (MVBP2 & 2)
            {
                //calculate top field MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][0],&pCurrMB->dmv_y[0][0]);
            }

            if (MVBP2 & 1)
            {

                //calculate botom field MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][1],&pCurrMB->dmv_y[0][1]);
            }
        }
        break;

    case(VC1_MB_2MV_INTER|VC1_MB_BACKWARD):
        {
            pCurrMB->dmv_x[1][0] = 0;
            pCurrMB->dmv_y[1][0] = 0;
            pCurrMB->dmv_x[1][1] = 0;
            pCurrMB->dmv_y[1][1] = 0;

            if (MVBP2 & 2)
            {
                // calculate top field backward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][0],&pCurrMB->dmv_y[1][0]);
            }
            if (MVBP2 & 1)
            {
                // calculate bottom field backward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][1],&pCurrMB->dmv_y[1][1]);
            }

        }
        break;
    case(VC1_MB_2MV_INTER|VC1_MB_INTERP):
        {
            pCurrMB->dmv_x[0][0] = 0;
            pCurrMB->dmv_y[0][0] = 0;
            pCurrMB->dmv_x[0][1] = 0;
            pCurrMB->dmv_y[0][1] = 0;

            pCurrMB->dmv_x[1][0] = 0;
            pCurrMB->dmv_y[1][0] = 0;
            pCurrMB->dmv_x[1][1] = 0;
            pCurrMB->dmv_y[1][1] = 0;

            //calculate top field MV difference
            if (MVBP4 & 8)
            {
                // calculate top field forward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][0],&pCurrMB->dmv_y[0][0]);
            }
            if (MVBP4 & 4)
            {
                // calculate top field backward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][0],&pCurrMB->dmv_y[1][0]);
            }

            //calculate botom field MV difference
            if (MVBP4 & 2)
            {
                // calculate bottom field forward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[0][1],&pCurrMB->dmv_y[0][1]);
            }

            if (MVBP4 & 1)
            {
                // calculate bottom field backward MV difference
                DecodeMVDiff_Adv(pContext,&pCurrMB->dmv_x[1][1],&pCurrMB->dmv_y[1][1]);
            }
        }
        break;
    }

    return VC1_OK;
}
VC1Status MBLayer_InterlaceFrameBpicture_NONDIRECT_Prediction(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;

    int16_t fX = 0, fY = 0;
    int16_t bX = 0, bY = 0;

    PredictInterlaceFrame1MV(pContext);
    switch (pCurrMB->mbType)
    {
    case(VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        {
            CalculateInterlaceFrame1MV_B(&pContext->MVPred, &fX, &fY, &bX, &bY, 0);

            ApplyMVPredictionCalculate(pContext,&fX,&fY,pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0]);
            writeMV(pCurrMB,fX,fY,fX,fY,0);
            writeMV(pCurrMB,bX,bY,bX,bY,1);
         }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_BACKWARD):
        {
            CalculateInterlaceFrame1MV_B(&pContext->MVPred, &fX, &fY, &bX, &bY, 1);
            ApplyMVPredictionCalculate(pContext,&fX,&fY,pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0]);
            writeMV(pCurrMB,fX,fY,fX,fY,1);
            writeMV(pCurrMB,bX,bY,bX,bY,0);

        }
        break;

    case(VC1_MB_1MV_INTER|VC1_MB_INTERP):
        {

            CalculateInterlaceFrame1MV_B_Interpolate(&pContext->MVPred, &fX, &fY, &bX, &bY);
            ApplyMVPredictionCalculate(pContext,&fX,&fY,pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0]);
            writeMV(pCurrMB,fX,fY,fX,fY,0);
            ApplyMVPredictionCalculate(pContext,&bX,&bY,pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0]);
            writeMV(pCurrMB,bX,bY,bX,bY,1);
        }
        break;

    case(VC1_MB_2MV_INTER|VC1_MB_FORWARD):
        {

            int16_t pMVx[2] = {0,0}; //0 - top, 1 - bottom
            int16_t pMVy[2] = {0,0};

            if (pCurrMB->MVSW)
            {
                PredictInterlace2MV_Field_Adv(pCurrMB,
                                        pMVx,pMVy,0, 1, pContext->m_seqLayerHeader.MaxWidthMB);
                ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0]);
                ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[0][1],pCurrMB->dmv_y[0][1]);
                writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[0],pMVy[0],0);
                writeMV(pCurrMB,pMVx[1],pMVy[1],pMVx[1],pMVy[1],1);

            }
            else
            {
                PredictInterlace2MV_Field_Adv(pCurrMB,
                                        pMVx,pMVy,0, 0, sMB->MaxWidthMB);
                ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0]);
                ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[0][1],pCurrMB->dmv_y[0][1]);
                writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],0);
                PredictInterlace2MV_Field_Adv(pCurrMB,  pMVx,pMVy,1, 1, sMB->MaxWidthMB);
                writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],1);
            }
        }
        break;

    case(VC1_MB_2MV_INTER|VC1_MB_BACKWARD):
        {
            int16_t pMVx[2] = {0,0}; //0 - top, 1 - bottom
            int16_t pMVy[2] = {0,0};

            if (pCurrMB->MVSW)
            {
                PredictInterlace2MV_Field_Adv(pCurrMB,
                                        pMVx,pMVy,1, 0, pContext->m_seqLayerHeader.MaxWidthMB);

                ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0]);
                ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[1][1],pCurrMB->dmv_y[1][1]);
                writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[0],pMVy[0],1);
                writeMV(pCurrMB,pMVx[1],pMVy[1],pMVx[1],pMVy[1],0);

            }
            else
            {
                PredictInterlace2MV_Field_Adv(pCurrMB, pMVx,pMVy,1, 1, pContext->m_seqLayerHeader.MaxWidthMB);


                ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0]);
                ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[1][1],pCurrMB->dmv_y[1][1]);
                writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],1);
                PredictInterlace2MV_Field_Adv(pCurrMB, pMVx,pMVy,0, 0, pContext->m_seqLayerHeader.MaxWidthMB);
                writeMV(pContext->m_pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],0);
            }
        }
        break;
    case(VC1_MB_2MV_INTER|VC1_MB_INTERP):
        {
            int16_t pMVx[2] = {0,0}; //0 - top, 1 - bottom
            int16_t pMVy[2] = {0,0};

            PredictInterlace2MV_Field_Adv(pCurrMB,
                                    pMVx,pMVy,1, 1, pContext->m_seqLayerHeader.MaxWidthMB);
            ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0]);
            ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[1][1],pCurrMB->dmv_y[1][1]);
            writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],1);
            PredictInterlace2MV_Field_Adv(pCurrMB,
                                    pMVx,pMVy,0, 0, pContext->m_seqLayerHeader.MaxWidthMB);
            ApplyMVPredictionCalculate(pContext,&pMVx[0],&pMVy[0],pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0]);
            ApplyMVPredictionCalculate(pContext,&pMVx[1],&pMVy[1],pCurrMB->dmv_x[0][1],pCurrMB->dmv_y[0][1]);
            writeMV(pCurrMB,pMVx[0],pMVy[0],pMVx[1],pMVy[1],0);
        }
        break;
    }

    return VC1_OK;
}
static VC1Status MBLayer_InterlaceFrameBpicture_DIRECT_Decode(VC1Context* pContext)
{
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    int32_t blk_num;
    int32_t ret = 0;

    for (blk_num=0;blk_num<VC1_NUM_OF_BLOCKS;blk_num++)
        pCurrMB->m_pBlocks[blk_num].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;

    if(VC1_MB_Mode_PBPic_Transform_Table[sMB->MBMODEIndex]!= VC1_NO_CBP_TRANSFORM)
    {
        //CBPCY
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &pCurrMB->m_cbpBits,
            pContext->m_picLayerHeader->m_pCurrCBPCYtbl);

        VM_ASSERT(ret == 0);
    }
    else
    {
        pCurrMB->m_cbpBits = 0;
    }
    return VC1_OK;
}
VC1Status MBLayer_InterlaceFrameBpicture_DIRECT_Prediction(VC1Context* pContext)
{
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int16_t* savedMV = pContext->savedMV_Curr + (pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos
                                    + sMB->m_currMBXpos)*2*2;

    int16_t Xt,Yt, Xb, Yb;
    int16_t Xtopf,Ytopf,Xbottomf,Ybottomf;
    int16_t Xtopb,Ytopb,Xbottomb,Ybottomb;

    Xt = (savedMV[0] == VC1_MVINTRA)?0:savedMV[0];
    Yt = (savedMV[1] == VC1_MVINTRA)?0:savedMV[1];
    Xb = (savedMV[2] == VC1_MVINTRA)?0:savedMV[2];
    Yb = (savedMV[3] == VC1_MVINTRA)?0:savedMV[3];

    Scale_Direct_MV_Interlace(picLayerHeader,Xt,Yt,&Xtopf,&Ytopf,&Xtopb,&Ytopb);
    Scale_Direct_MV_Interlace(picLayerHeader,Xb,Yb,&Xbottomf,&Ybottomf,&Xbottomb,&Ybottomb);

    writeMV(pCurrMB,Xtopf,Ytopf,Xbottomf,Ybottomf,0);
    writeMV(pCurrMB,Xtopb,Ytopb,Xbottomb,Ybottomb,1);
    return VC1_OK;
}

static const B_MB_DECODE B_InterlaceFrame_MB_Dispatch_table[] =
{
    (B_MB_DECODE)(MBLayer_InterlaceFrameBpicture_NONDIRECT_Decode),
        (B_MB_DECODE)(MBLayer_InterlaceFrameBpicture_SKIP_NONDIRECT_Decode),
};
//Figure 20: Syntax diagram for frame MB layer in interlace-coded B picture
VC1Status MBLayer_Frame_InterlacedBpicture(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t DIRECTBBIT=0;
    int32_t SKIPMBBIT=0;
    int32_t blk_num;
    uint32_t ACPRED = 0;

    uint32_t tempValue;
    VC1Status vc1Res = VC1_OK;
    pCurrMB->m_cbpBits = 0;
    sMB->m_ubNumFirstCodedBlk = 0;

    pCurrMB->SkipAndDirectFlag = 0;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,VM_STRING("\t\t\tX: %d, Y: %d\n"),
        sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);
    Set_MQuant(pContext);

      //Y
    pCurrMB->currYPitch = sMB->currYPitch;
    pCurrMB->currYPlane = sMB->currYPlane + pCurrMB->currYPitch * (sMB->m_currMBYpos << 4)
                                          + (sMB->m_currMBXpos << 4);

    //U
    pCurrMB->currUPitch = sMB->currUPitch;
    pCurrMB->currUPlane = sMB->currUPlane   + pCurrMB->currUPitch*(sMB->m_currMBYpos << 3)
                                            + (sMB->m_currMBXpos << 3);

    //V
    pCurrMB->currVPitch = sMB->currVPitch;
    pCurrMB->currVPlane = sMB->currVPlane + pCurrMB->currVPitch*(sMB->m_currMBYpos << 3)
                                          + (sMB->m_currMBXpos << 3);

    //check SKIPMB mode
    {
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->SKIPMB))
        {
            VC1_GET_BITS(1, SKIPMBBIT);
        }
        else
        {
            SKIPMBBIT = picLayerHeader->SKIPMB.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +  sMB->m_currMBXpos];
        }
    }

    if(SKIPMBBIT == 1)
    {
        pCurrMB->mbType = VC1_MB_1MV_INTER;
    }
    else
    {
        memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

        int32_t ret;
        ret = DecodeHuffmanOne(
            &pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &sMB->MBMODEIndex,
            picLayerHeader->m_pMBMode
            );
        VM_ASSERT(ret == 0);

        pCurrMB->mbType = VC1_MB_Mode_PBPic_MBtype_Table[sMB->MBMODEIndex];
    }

    //INTRA
    if(pCurrMB->mbType == VC1_MB_INTRA)
    {
        for (blk_num = 0; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
            pCurrMB->m_pBlocks[blk_num].blkType = VC1_BLK_INTRA;

        //check fieldtx coding mode
        {
            int32_t FIELDTX;
            if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->FIELDTX))
            {
                VC1_GET_BITS(1, FIELDTX);
            } else {
                FIELDTX = picLayerHeader->FIELDTX.m_databits
                    [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos + sMB->m_currMBXpos];
            }
            pCurrMB->FIELDTX = FIELDTX;
        }

        VC1_GET_BITS(1, tempValue);  //CBPRESENT

        if(tempValue == 1)       //CBPRESENT
        {
            //CBPCY decoding
            int32_t ret;
            ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                            &pContext->m_bitstream.bitOffset,
                                            &pCurrMB->m_cbpBits,
                                            picLayerHeader->m_pCurrCBPCYtbl);

            VM_ASSERT(ret == 0);
        }

        VC1_GET_BITS(1, ACPRED);

        if (picLayerHeader->m_DQuantFRM)
            Set_Alt_MQUANT(pContext);

        CalculateIntraFlag(pContext);

        PDCPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);
        sMB->ZigzagTable = AdvZigZagTables_PBInterlace_luma[ACPRED];

        for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
        {
            vc1Res = BLKLayer_Intra_Luma_Adv(pContext, blk_num,ACPRED);
            if(vc1Res != VC1_OK)
            {
                VM_ASSERT(0);
                break;
            }
        }

        sMB->ZigzagTable = AdvZigZagTables_PBInterlace_chroma[ACPRED];

        for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
        {
            vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, blk_num,ACPRED);
            if(vc1Res != VC1_OK)
            {
                VM_ASSERT(0);
                break;
            }
        }
    }
    else
    {
        //inter
        if(SKIPMBBIT == 0)
            pCurrMB->FIELDTX = VC1_MB_Mode_PBPic_FIELDTX_Table[sMB->MBMODEIndex];

        // DIRECTBBIT
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->m_DirectMB))
        {
            VC1_GET_BITS(1, DIRECTBBIT);
        }
        else
        {
            DIRECTBBIT = picLayerHeader->m_DirectMB.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos + sMB->m_currMBXpos];
        }
        pContext->m_pCurrMB->SkipAndDirectFlag = (DIRECTBBIT+(SKIPMBBIT<<1));

        if (!DIRECTBBIT)
            B_InterlaceFrame_MB_Dispatch_table[SKIPMBBIT](pContext);
        else if (!SKIPMBBIT)
            MBLayer_InterlaceFrameBpicture_DIRECT_Decode(pContext);
        else
            MBLayer_InterlaceFrameBpicture_SKIP_DIRECT_Decode(pContext);

        if((picLayerHeader->m_DQuantFRM == 1) && (pCurrMB->m_cbpBits!=0))
            Set_Alt_MQUANT(pContext);

        if(picLayerHeader->TTMBF==0 && pCurrMB->m_cbpBits &&
            pContext->m_seqLayerHeader.VSTRANSFORM)
        {
            //TTMB
            GetTTMB(pContext);
        }

        CalculateIntraFlag(pContext);

        if(SKIPMBBIT == 0)
        {
            sMB->ZigzagTable = AdvZigZagTables_PBInterlace_luma[ACPRED];

            for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
            {
                vc1Res = BLKLayer_Inter_Luma_Adv(pContext, blk_num);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }

            sMB->ZigzagTable = AdvZigZagTables_PBInterlace_chroma[ACPRED];

            for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
            {
                vc1Res = BLKLayer_Inter_Chroma_Adv(pContext, blk_num);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
        }
    }
    //if (pContext->m_seqLayerHeader.LOOPFILTER)
        AssignCodedBlockPattern(pCurrMB,sMB);
    return vc1Res;
}

VC1Status MBLayer_InterlaceFieldBpicture_NONDIRECT_Decode(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    int32_t i=0;
    int32_t ret = 0;

    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;



    pCurrMB->dmv_x[1][0] = 0;
    pCurrMB->dmv_x[0][0] = 0;
    pCurrMB->dmv_y[1][0] = 0;
    pCurrMB->dmv_y[0][0] = 0;

    pCurrMB->predictor_flag[0] = 0;
    pCurrMB->predictor_flag[1] = 0;


    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
    {
        pCurrMB->m_pBlocks[i].blkType= VC1_BLK_INTER8X8;;
    }

    CalculateIntraFlag(pContext);

    if(VC1_GET_MBTYPE(pCurrMB->mbType) == VC1_MB_1MV_INTER)
    {
        if(VC1_MB_Mode_PBFieldPic_MVData_Table[sMB->MBMODEIndex])
        {
            pCurrMB->predictor_flag[0] = DecodeMVDiff_TwoReferenceField_Adv(pContext,&pCurrMB->dmv_x[0][0],&pCurrMB->dmv_y[0][0]);//BMV1
        }

        if((VC1_GET_PREDICT(pCurrMB->mbType)==VC1_MB_INTERP)
                                        && sMB->INTERPMVP)
        {
            pCurrMB->predictor_flag[1] = DecodeMVDiff_TwoReferenceField_Adv(pContext,&pCurrMB->dmv_x[1][0],&pCurrMB->dmv_y[1][0]);
            //BMV2
        } else if ( VC1_GET_PREDICT(pCurrMB->mbType)!=VC1_MB_INTERP)
        {
            pCurrMB->dmv_x[1][0] = pCurrMB->dmv_x[0][0];
            pCurrMB->dmv_y[1][0] = pCurrMB->dmv_y[0][0];
        }
    }
    else
    {
        //4 MV type
        {
            //4MVBP
            ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                &pContext->m_bitstream.bitOffset,
                &pCurrMB->MVBP,
                picLayerHeader->m_pMV4BP);

            VM_ASSERT(ret == 0);
            MBLayer_InterlaceFieldBpicture4MV_Decode(pContext);
        }
    }

    if(VC1_MB_Mode_PBFieldPic_CBPPresent_Table[sMB->MBMODEIndex])
    {
        //CBPCY
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &pCurrMB->m_cbpBits,
            picLayerHeader->m_pCurrCBPCYtbl);

        VM_ASSERT(ret == 0);
    }
    else
    {
        pCurrMB->m_cbpBits = 0;
    }
    return vc1Res;
}
VC1Status MBLayer_InterlaceFieldBpicture_NONDIRECT_Predicition(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    int16_t X=0,Y=0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    if(VC1_GET_MBTYPE(pCurrMB->mbType) != VC1_MB_1MV_INTER)
    {
        MBLayer_InterlaceFieldBpicture4MV_Prediction(pContext);
        pCurrMB->predictor_flag[0] = 0;
    }

    Field1MVPrediction(pContext);

    switch (pCurrMB->mbType)
    {
    case (VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        {
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[0]);
#endif
            CalculateField1MVTwoReferenceBPic(pContext,&X,&Y,0,&pCurrMB->predictor_flag[0]);
            ApplyMVPredictionCalculateTwoReference(pContext->m_picLayerHeader,&X,&Y,pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0],pCurrMB->predictor_flag[0]);
            writeMV_w_predict(pContext,X,Y,X,Y,0,pCurrMB->predictor_flag[0]);
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
#endif
            {
                int16_t Xbtop = 0;
                int16_t Ybtop = 0;
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[1]);
#endif
                CalculateField1MVTwoReferenceBPic(pContext, &Xbtop, &Ybtop,1,&pCurrMB->predictor_flag[1]);
                writeMV_w_predict(pContext,Xbtop,Ybtop,Xbtop,Ybtop,1,pCurrMB->predictor_flag[1]);
            }
        }
        break;
    case (VC1_MB_4MV_INTER|VC1_MB_FORWARD):
        {
            // need to fill backward population
            int32_t blk_num;

#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[0]);
#endif

            CalculateField1MVTwoReferenceBPic(pContext, &X,&Y,1,&pCurrMB->predictor_flag[0]);

            pCurrMB->m_pBlocks[0].mv[1][0] = X;
            pCurrMB->m_pBlocks[0].mv[1][1] = Y;
            pCurrMB->m_pBlocks[0].mv_s_polarity[1] = 1 - pCurrMB->predictor_flag[0];
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
#endif
            for( blk_num = 1;  blk_num < 4;  blk_num++)
            {
                pCurrMB->m_pBlocks[blk_num].fieldFlag[1] = pCurrMB->fieldFlag[1];
                pCurrMB->m_pBlocks[blk_num].mv[1][0] = X;
                pCurrMB->m_pBlocks[blk_num].mv[1][1] = Y;
                pCurrMB->m_pBlocks[blk_num].mv_s_polarity[1] = 1 - pCurrMB->predictor_flag[0];
            }


        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_BACKWARD):
        {
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[0]);
#endif
            CalculateField1MVTwoReferenceBPic(pContext,&X,&Y,1,&pCurrMB->predictor_flag[0]);
            ApplyMVPredictionCalculateTwoReference(pContext->m_picLayerHeader,&X,&Y,pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0],pCurrMB->predictor_flag[0]);
            writeMV_w_predict(pContext,X,Y,X,Y,1,pCurrMB->predictor_flag[0]);

            // calculate backward MVs
            {
                int16_t Xbtop = 0;
                int16_t Ybtop = 0;
#ifdef VC1_DEBUG_ON
                VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[1]);
#endif
                CalculateField1MVTwoReferenceBPic(pContext, &Xbtop, &Ybtop,0,&pCurrMB->predictor_flag[1]);
                writeMV_w_predict(pContext,Xbtop,Ybtop,Xbtop,Ybtop,0,pCurrMB->predictor_flag[1]);
            }
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
#endif
        }
        break;
    case (VC1_MB_4MV_INTER|VC1_MB_BACKWARD):
        {
            // need to fill forward population
            int32_t blk_num;
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[0]);
#endif
            CalculateField1MVTwoReferenceBPic(pContext,&X,&Y,0,&pCurrMB->predictor_flag[0]);

            pCurrMB->m_pBlocks[0].mv[0][0] = X;
            pCurrMB->m_pBlocks[0].mv[0][1] = Y;
            pCurrMB->m_pBlocks[0].mv_s_polarity[0] = 1 - pCurrMB->predictor_flag[0];

#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,
                                                                VM_STRING("\n\n"));
#endif

            for( blk_num = 1;  blk_num < VC1_NUM_OF_LUMA;  blk_num++)
            {
                pCurrMB->m_pBlocks[blk_num].fieldFlag[0] = pCurrMB->fieldFlag[0];

                pCurrMB->m_pBlocks[blk_num].mv[0][0] = X;
                pCurrMB->m_pBlocks[blk_num].mv[0][1] = Y;
                pCurrMB->m_pBlocks[blk_num].mv_s_polarity[0] = 1 - pCurrMB->predictor_flag[0];
            }
        }
        break;
    case(VC1_MB_1MV_INTER|VC1_MB_INTERP):
        {
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[0]);
#endif
            CalculateField1MVTwoReferenceBPic(pContext,&X,&Y,0,&pCurrMB->predictor_flag[0]);
            ApplyMVPredictionCalculateTwoReference(pContext->m_picLayerHeader,&X,&Y,pCurrMB->dmv_x[0][0],pCurrMB->dmv_y[0][0],pCurrMB->predictor_flag[0]);
            writeMV_w_predict(pContext,X,Y,X,Y,0,pCurrMB->predictor_flag[0]);

#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("predictor_flag = %d\n"), pCurrMB->predictor_flag[1]);
#endif

            CalculateField1MVTwoReferenceBPic(pContext,&X,&Y,1,&pCurrMB->predictor_flag[1]);
            ApplyMVPredictionCalculateTwoReference(pContext->m_picLayerHeader,&X,&Y,pCurrMB->dmv_x[1][0],pCurrMB->dmv_y[1][0],pCurrMB->predictor_flag[1]);
            writeMV_w_predict(pContext,X,Y,X,Y,1,pCurrMB->predictor_flag[1]);
#ifdef VC1_DEBUG_ON
            VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,VM_STRING("\n\n"));
#endif
        }
        break;
    }

    return vc1Res;
}
VC1Status MBLayer_InterlaceFieldBpicture_DIRECT_Prediction(VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    int16_t X,Y,Xf,Yf,Xb,Yb;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    int16_t* savedMV = pContext->savedMV_Curr
        + (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

    X = savedMV[0];
    Y = savedMV[1];

    CalculateMV_InterlaceField(pContext, &X, &Y);
    Scale_Direct_MV(pContext->m_picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
    writeMV(pCurrMB,Xf,Yf,0,0,0);
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,(VM_STRING("CurrFlag1 = %d\n")), pCurrMB->fieldFlag[0]);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,(VM_STRING("IX = %d IY = %d\n")), Xf, Yf);
#endif

    writeMV(pCurrMB,Xb,Yb,0,0,1);

#ifdef VC1_DEBUG_ON
   VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,(VM_STRING("CurrFlag1 = %d\n")), pCurrMB->fieldFlag[1]);
   VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_MV_FIELD,(VM_STRING("IX = %d IY = %d\n")), Xb, Yb);
#endif
    return vc1Res;
}



VC1Status MBLayer_InterlaceFieldBpicture_DIRECT_Decode(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    VC1Status vc1Res = VC1_OK;
    int32_t blk_num = 0;
    int32_t ret = 0;

    for (blk_num=0;blk_num<VC1_NUM_OF_BLOCKS;blk_num++)
        pCurrMB->m_pBlocks[blk_num].blkType = (uint8_t)picLayerHeader->TTFRM;

     CalculateIntraFlag(pContext);


    if(VC1_MB_Mode_PBFieldPic_CBPPresent_Table[sMB->MBMODEIndex])
    {
        //CBPCY
        ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
            &pContext->m_bitstream.bitOffset,
            &pCurrMB->m_cbpBits,
            pContext->m_picLayerHeader->m_pCurrCBPCYtbl);

        VM_ASSERT(ret == 0);
    }
    else
    {
        pCurrMB->m_cbpBits = 0;
    }
    return vc1Res;
}
VC1Status MBLayer_Field_InterlacedBpicture (VC1Context* pContext)
{
    VC1Status vc1Res = VC1_OK;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    int32_t blk_num;
    uint32_t currFieldMBYpos;
    uint32_t currFieldMBXpos;
    uint32_t ACPRED = 0;

#ifdef VC1_DEBUG_ON
     VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,
                                                            VM_STRING("\t\t\tX: %d, Y: %d\n"),
                                                            sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif

    for (blk_num=0; blk_num < VC1_NUM_OF_BLOCKS ; blk_num++)
    {
        pCurrMB->m_pBlocks[blk_num].fieldFlag[0] = picLayerHeader->BottomField;
        pCurrMB->m_pBlocks[blk_num].fieldFlag[1] = picLayerHeader->BottomField;
    }

    pCurrMB->fieldFlag[0] = picLayerHeader->BottomField;
    pCurrMB->fieldFlag[1] = picLayerHeader->BottomField;
    currFieldMBYpos = sMB->m_currMBYpos;
    currFieldMBXpos = sMB->m_currMBXpos;

    pCurrMB->SkipAndDirectFlag = 0;


    if (picLayerHeader->CurrField)
        currFieldMBYpos -= sMB->heightMB/2;

    //Y
    pCurrMB->currYPitch = sMB->currYPitch;
    pCurrMB->currYPlane = sMB->currYPlane + pCurrMB->currYPitch * (currFieldMBYpos << 5)//*VC1_PIXEL_IN_LUMA
                                          + (currFieldMBXpos << 4); //*VC1_PIXEL_IN_LUMA;

    //U
    pCurrMB->currUPitch = sMB->currUPitch;
    pCurrMB->currUPlane = sMB->currUPlane   + pCurrMB->currUPitch*(currFieldMBYpos << 4) // * VC1_PIXEL_IN_CHROMA
                                            + (currFieldMBXpos << 3); //* VC1_PIXEL_IN_CHROMA;

    //V
    pCurrMB->currVPitch = sMB->currVPitch;
    pCurrMB->currVPlane = sMB->currVPlane + pCurrMB->currVPitch*(currFieldMBYpos <<4) // * VC1_PIXEL_IN_CHROMA
                                          + (currFieldMBXpos << 3); //* VC1_PIXEL_IN_CHROMA;

   pCurrMB->currYPlane = pCurrMB->currYPlane + pCurrMB->currYPitch * picLayerHeader->BottomField;
   pCurrMB->currUPlane = pCurrMB->currUPlane + pCurrMB->currUPitch * picLayerHeader->BottomField;
   pCurrMB->currVPlane = pCurrMB->currVPlane + pCurrMB->currVPitch * picLayerHeader->BottomField;

   pCurrMB->currYPitch <<= 1;
   pCurrMB->currUPitch <<= 1;
   pCurrMB->currVPitch <<= 1;

   memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant_Field(pContext);
    pCurrMB->FIELDTX = 0;

    {
        int32_t ret;
        ret = DecodeHuffmanOne(  &pContext->m_bitstream.pBitstream,
                                            &pContext->m_bitstream.bitOffset,
                                            &sMB->MBMODEIndex,
                                            picLayerHeader->m_pMBMode);
        VM_ASSERT(ret == 0);
    }

    pCurrMB->mbType = VC1_MB_Mode_PBFieldPic_MBtype_Table[sMB->MBMODEIndex];

    if(pCurrMB->mbType == VC1_MB_INTRA)
    {
        for(blk_num = 0; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
        {
            pCurrMB->m_pBlocks[blk_num].blkType = VC1_BLK_INTRA;
        }

        CalculateIntraFlag(pContext);

        if (picLayerHeader->m_DQuantFRM)
            Set_Alt_MQUANT(pContext);

        VC1_GET_BITS(1, ACPRED);

        if(VC1_MB_Mode_PBFieldPic_CBPPresent_Table[sMB->MBMODEIndex] != 0)
        {
            //CBPCY decoding
            int32_t ret;
            ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                            &pContext->m_bitstream.bitOffset,
                                            &pCurrMB->m_cbpBits,
                                            picLayerHeader->m_pCurrCBPCYtbl);

            VM_ASSERT(ret == 0);
        }
        else
            pCurrMB->m_cbpBits = 0;

        CalculateIntraFlag(pContext);


        PDCPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);
        sMB->ZigzagTable = AdvZigZagTables_PBField_luma[ACPRED];

        for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
        {
           vc1Res = BLKLayer_Intra_Luma_Adv(pContext, blk_num, ACPRED);
           if(vc1Res != VC1_OK)
           {
              VM_ASSERT(0);
               break;
           }
        }

        sMB->ZigzagTable = AdvZigZagTables_PBField_chroma[ACPRED];

       for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
       {
           //all MB inter
           vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, blk_num, ACPRED);
           if(vc1Res != VC1_OK)
           {
               VM_ASSERT(0);
               break;
           }
       }
    }
    else
    {
        int32_t FORWARDBIT;
        //inter
        {
            if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->FORWARDMB))
            {
                VC1_GET_BITS(1, FORWARDBIT);
            }
            else {
                FORWARDBIT = picLayerHeader->FORWARDMB.m_databits
                    [pContext->m_seqLayerHeader.MaxWidthMB * sMB->slice_currMBYpos +
                    sMB->m_currMBXpos];
            }
        }


        if(!FORWARDBIT)
        {
            if( VC1_GET_MBTYPE(pCurrMB->mbType) == VC1_MB_4MV_INTER)
            {
                pCurrMB->mbType = (uint8_t)(pCurrMB->mbType | VC1_MB_BACKWARD);
            }
            else
            {
                Decode_InterlaceField_BMVTYPE(pContext);
            }

            if(VC1_GET_PREDICT(pCurrMB->mbType) == VC1_MB_INTERP)
            {
                VC1_GET_BITS(1, sMB->INTERPMVP);
            }
        }
        else
        {
            pCurrMB->mbType = (uint8_t)(pCurrMB->mbType | VC1_MB_FORWARD);
        }

        if(pCurrMB->mbType == (VC1_MB_DIRECT|VC1_MB_1MV_INTER) ||
           pCurrMB->mbType == (VC1_MB_DIRECT|VC1_MB_4MV_INTER))
        {
            MBLayer_InterlaceFieldBpicture_DIRECT_Decode(pContext);
            pCurrMB->SkipAndDirectFlag = 1;
        }
        else
        {
            MBLayer_InterlaceFieldBpicture_NONDIRECT_Decode(pContext);
            pCurrMB->SkipAndDirectFlag = 0;
        }

        if((picLayerHeader->m_DQuantFRM == 1) && (pCurrMB->m_cbpBits!=0))
            Set_Alt_MQUANT(pContext);

        DecodeTransformInfo(pContext);
        CalculateIntraFlag(pContext);
        sMB->ZigzagTable = AdvZigZagTables_PBField_luma[ACPRED];

        for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
        {
           vc1Res = BLKLayer_Inter_Luma_Adv(pContext, blk_num);
           if(vc1Res != VC1_OK)
           {
               VM_ASSERT(0);
               break;
           }
        }

        sMB->ZigzagTable = AdvZigZagTables_PBField_chroma[ACPRED];

       for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
       {
           //all MB inter
           vc1Res = BLKLayer_Inter_Chroma_Adv(pContext, blk_num);
           if(vc1Res != VC1_OK)
           {
               VM_ASSERT(0);
               break;
           }
       }
    }
    //if (pContext->m_seqLayerHeader.LOOPFILTER)
        AssignCodedBlockPattern(pCurrMB,sMB);

    return vc1Res;
}
#endif //MFX_ENABLE_VC1_VIDEO_DECODE