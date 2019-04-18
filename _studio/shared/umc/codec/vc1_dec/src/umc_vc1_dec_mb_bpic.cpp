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
static VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Decode(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    pCurrMB->m_cbpBits = 0;
    Decode_BMVTYPE(pContext); // FORWARD, BACKWARD OR INTER
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
    {
        pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTER8X8;
    }
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Prediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    switch (pContext->m_pCurrMB->mbType)
    {
    case (VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B(pContext,&X,&Y,0);
        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 0);
        // backward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf,Yf,Xb=0,Yb=0;
            int16_t* savedMV = pContext->savedMV_Curr
                              + (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                PullBack_BDirect(pContext,  &X, &Y);
                Scale_Direct_MV(pContext->m_picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xb,&Yb,-1);//_MAYBE_ mismatch number of parameters
            }
            for (i=0;i<4;i++)
            {
                pContext->m_pCurrMB->m_pBlocks[i].mv[1][0]=Xb;
                pContext->m_pCurrMB->m_pBlocks[i].mv[1][1]=Yb;
            }
        }
        break;
    case (VC1_MB_1MV_INTER|VC1_MB_BACKWARD):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B(pContext,&X,&Y,1);
        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 1);
        // forward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf=0,Yf=0,Xb,Yb;
            int16_t* savedMV = pContext->savedMV_Curr + (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                PullBack_BDirect(pContext,  &X, &Y);
                Scale_Direct_MV(pContext->m_picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xf,&Yf,-1);//_MAYBE_ mismatch number of parameters
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
        CalculateProgressive1MV_B(pContext,&X,&Y,0);
        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 0);

        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B(pContext,&X,&Y,1);
        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 1);
        break;
    }
    return VC1_OK;
}
VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_Decode(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    pCurrMB->m_cbpBits = 0;
    pCurrMB->mbType= VC1_MB_1MV_INTER|VC1_MB_DIRECT;
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pContext->m_pCurrMB->m_pBlocks[i].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;
    return VC1_OK;
}

VC1Status MBLayer_ProgressiveBpicture_SKIP_DIRECT_Prediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    int32_t i;
    int16_t Xf,Yf,Xb,Yb;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    int16_t* savedMV = pContext->savedMV_Curr + (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos)*2*2;
    X = savedMV[0];
    Y = savedMV[1];

    pCurrMB->m_cbpBits = 0;
    pCurrMB->mbType= VC1_MB_1MV_INTER|VC1_MB_DIRECT;
    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;

    if ((uint16_t)X!=VC1_MVINTRA)
    {
        PullBack_BDirect(pContext,  &X, &Y);
        Scale_Direct_MV(pContext->m_picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
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

static VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_Decode(VC1Context* pContext)
{
    int16_t tmp_dx = 0,tmp_dy = 0;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    int32_t ret = 0;

    pCurrMB->dmv_x[0][0]= 0;
    pCurrMB->dmv_x[1][0]= 0;
    pCurrMB->dmv_y[0][0]= 0;
    pCurrMB->dmv_y[1][0]= 0;


    uint16_t last_intra_flag = 0;
    int32_t i;
    int16_t hpelfl = (int16_t)((picLayerHeader->MVMODE==VC1_MVMODE_HPEL_1MV) ||
        (picLayerHeader->MVMODE==VC1_MVMODE_HPELBI_1MV));

    // decodes BMV1 (variable size)
    last_intra_flag = DecodeMVDiff(pContext,hpelfl,&tmp_dx,&tmp_dy);

    pCurrMB->m_cbpBits = (!(last_intra_flag&0x10))?0:pCurrMB->m_cbpBits;

    if (last_intra_flag&0x1)
    {
        //intra
        pCurrMB->mbType = VC1_MB_INTRA;
        for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
            pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTRA;
        if ((!(last_intra_flag&0x10))&&
            picLayerHeader->m_PQuant_mode>=VC1_ALTPQUANT_MB_LEVEL)
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
        for (i=0;i<4;i++)
        {
            pCurrMB->m_pBlocks[i].blkType = VC1_BLK_INTER8X8;
        }
        pCurrMB->dmv_x[value][0] = tmp_dx * (1 + hpelfl);
        pCurrMB->dmv_y[value][0] = tmp_dy * (1 + hpelfl);

        pCurrMB->m_pBlocks[4].blkType = VC1_BLK_INTER8X8;
        pCurrMB->m_pBlocks[5].blkType = VC1_BLK_INTER8X8;
        if ((pCurrMB->mbType==(VC1_MB_1MV_INTER|VC1_MB_INTERP))&&(last_intra_flag&0x10))
        {
            // decodes BMV2 (variable size)
            last_intra_flag = DecodeMVDiff(pContext,hpelfl,&tmp_dx,&tmp_dy);
            //not_last = (uint8_t)(last_intra_flag>>4);
            //intra_flag = (uint8_t)(last_intra_flag & 0x1);
            VM_ASSERT(!(last_intra_flag&0x1));


            pCurrMB->m_cbpBits = (last_intra_flag&0x10)?0:pCurrMB->m_cbpBits;
            pCurrMB->dmv_x[0][0] = tmp_dx * (1 + hpelfl);
            pCurrMB->dmv_y[0][0] = tmp_dy * (1 + hpelfl);
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

VC1Status MBLayer_ProgressiveBpicture_NONDIRECT_Prediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    switch (pCurrMB->mbType)
    {
    case (VC1_MB_1MV_INTER|VC1_MB_FORWARD):
        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B(pContext,&X,&Y,0);
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
                PullBack_BDirect(pContext,  &X, &Y);
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xb,&Yb,-1);//_MAYBE_ mismatch number of parameters
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
        CalculateProgressive1MV_B(pContext,&X,&Y,1);
        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[1][0], pCurrMB->dmv_y[1][0],1);
        // forward MV can be used for MV Prediction, so it should be calculated
        {
            int32_t i;
            int16_t Xf=0,Yf=0,Xb=0,Yb=0;
            int16_t* savedMV = pContext->savedMV_Curr
                + (pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos+ sMB->m_currMBXpos)*2*2;

            X = savedMV[0];
            Y = savedMV[1];

            if ((uint16_t)X!=VC1_MVINTRA)
            {
                PullBack_BDirect(pContext,  &X, &Y);
                Scale_Direct_MV(picLayerHeader,X,Y,&Xf,&Yf,&Xb,&Yb);
                PullBack_PPred(pContext, &Xf,&Yf,-1);//_MAYBE_ mismatch number of parameters
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
        CalculateProgressive1MV_B(pContext,&X,&Y,0);
        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[0][0], pCurrMB->dmv_y[0][0], 0);

        Progressive1MVPrediction(pContext);
        CalculateProgressive1MV_B(pContext,&X,&Y,1);
        ApplyMVPrediction(pContext, 0, &X, &Y, pCurrMB->dmv_x[1][0], pCurrMB->dmv_y[1][0], 1);
        break;
    }

    return VC1_OK;
}
VC1Status MBLayer_ProgressiveBpicture_DIRECT_Prediction(VC1Context* pContext)
{
    int16_t X = 0, Y = 0;
    uint32_t i;
    int16_t* savedMV = pContext->savedMV_Curr +
        (pContext->m_seqLayerHeader.MaxWidthMB*pContext->m_pSingleMB->m_currMBYpos
                + pContext->m_pSingleMB->m_currMBXpos)*2*2;

    X = savedMV[0];
    Y = savedMV[1];

    int16_t Xf,Yf,Xb,Yb;

    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    if ((uint16_t)X!=VC1_MVINTRA)
    {
        PullBack_BDirect(pContext,  &X, &Y);
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

static VC1Status MBLayer_ProgressiveBpicture_DIRECT_Decode(VC1Context* pContext)
{
    int32_t i;
    int32_t ret = 0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        pCurrMB->m_pBlocks[i].blkType = (uint8_t)pContext->m_picLayerHeader->TTFRM;
    pCurrMB->mbType=(VC1_MB_1MV_INTER|VC1_MB_DIRECT);
    //CBPCY
    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
        &pContext->m_bitstream.bitOffset,
        &pCurrMB->m_cbpBits,
        pContext->m_picLayerHeader->m_pCurrCBPCYtbl);

    VM_ASSERT(ret == 0);

    //MQUANT
    if (picLayerHeader->m_PQuant_mode>=VC1_ALTPQUANT_MB_LEVEL)
        GetMQUANT(pContext);

    //TTMB
    DecodeTransformInfo(pContext);
    return VC1_OK;
}

static const B_MB_DECODE B_MB_Dispatch_table[] = {
        (B_MB_DECODE)(MBLayer_ProgressiveBpicture_NONDIRECT_Decode),
        (B_MB_DECODE)(MBLayer_ProgressiveBpicture_SKIP_NONDIRECT_Decode)
};

//vc-1: Figure 25: Syntax diagram for macroblock layer bitstream in
//Progressive-coded B picture
VC1Status MBLayer_ProgressiveBpicture(VC1Context* pContext)
{
    int32_t DIRECTBBIT=0;
    int32_t SKIPMBBIT=0;
    int32_t i;
    VC1Status vc1Res = VC1_OK;

    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    pCurrMB->Overlap =0;

    pCurrMB->m_cbpBits = 0;
    sMB->m_ubNumFirstCodedBlk = 0;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,
        VM_STRING("\t\t\tX: %d, Y: %d\n"),
        sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant(pContext);
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
        SKIPMBBIT = pContext->m_picLayerHeader->SKIPMB.m_databits
            [pContext->m_seqLayerHeader.MaxWidthMB*sMB->m_currMBYpos + sMB->m_currMBXpos];
    }

    pCurrMB->SkipAndDirectFlag = (DIRECTBBIT+(SKIPMBBIT<<1));

    if (!DIRECTBBIT)
        B_MB_Dispatch_table[SKIPMBBIT](pContext);
    else if (!SKIPMBBIT)
        MBLayer_ProgressiveBpicture_DIRECT_Decode(pContext);
    else
        MBLayer_ProgressiveBpicture_SKIP_DIRECT_Decode(pContext);

    CalculateIntraFlag(pContext);

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

    if(SKIPMBBIT == 0)
    {
        memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

        sMB->ZigzagTable = ZigZagTables_PB_luma[sMB->ACPRED];

        if(!pCurrMB->IntraFlag)
        {
            for(i = 0; i < VC1_NUM_OF_LUMA; i++)
            {
                //luma
                vc1Res = BLKLayer_Inter_Luma(pContext, i);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
            //chroma
            sMB->ZigzagTable = ZigZagTables_PB_chroma[sMB->ACPRED];

            for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
            {
                vc1Res = BLKLayer_Inter_Chroma(pContext, i);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
        }
        else
        {
            PDCPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);
            for(i = 0; i < VC1_NUM_OF_LUMA; i++)
            {
                vc1Res = BLKLayer_Intra_Luma(pContext, i, 128, sMB->ACPRED);
                if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
            }
            //chroma
            sMB->ZigzagTable = ZigZagTables_PB_chroma[sMB->ACPRED];

            for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
            {
                vc1Res = BLKLayer_Intra_Chroma(pContext, i, 128, sMB->ACPRED);
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

#endif //MFX_ENABLE_VC1_VIDEO_DECODE