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
#include "umc_vc1_common_zigzag_tbl.h"
#include "umc_vc1_huffman.h"

typedef void (*DCPrediction)(VC1Context* pContext);

static const DCPrediction PDCPredictionTable[] =
{
        (DCPrediction)(GetPDCPredictors),
        (DCPrediction)(GetPScaleDCPredictors),
        (DCPrediction)(GetPScaleDCPredictors)
};



static VC1Status MBLayer_ProgressivePskipped(VC1Context* pContext)
{
    int32_t blk_num;;
    int16_t X = 0, Y = 0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;

    pContext->m_pCurrMB->m_cbpBits = 0;

    for(blk_num = 0; blk_num < 6; blk_num++)
        pCurrMB->m_pBlocks[blk_num].blkType = VC1_BLK_INTER8X8;

    if((VC1_GET_MBTYPE(pCurrMB->mbType)) == VC1_MB_4MV_INTER)
    {
        Progressive4MVPrediction(pContext);
        for (blk_num=0;blk_num<4;blk_num++)
        {
            CalculateProgressive4MV(pContext,&X, &Y, blk_num);
            ApplyMVPrediction(pContext, blk_num, &X, &Y, 0, 0, 0);
        }
    }
    else
    {
        Progressive1MVPrediction(pContext);
        //MV prediction
        CalculateProgressive1MV(pContext,&X, &Y);
        ApplyMVPrediction(pContext, 0, &X, &Y, 0, 0, 0);
    }

    return VC1_OK;
}

static VC1Status MBLayer_ProgressivePpicture1MV(VC1Context* pContext)
{
    int ret;
    int16_t dmv_x;
    int16_t dmv_y;
    uint16_t last_intra_flag = 0;
    uint8_t blk_type;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t i;
    //MVDATA is a variable sized field present in P picture macroblocks
    //This field encodes the motion vector(s) for the macroblock.

    int16_t hpelfl = (int16_t)((picLayerHeader->MVMODE == VC1_MVMODE_HPEL_1MV) ||
                             (picLayerHeader->MVMODE == VC1_MVMODE_HPELBI_1MV));

    last_intra_flag = DecodeMVDiff(pContext,hpelfl,&dmv_x,&dmv_y);

    if(!(last_intra_flag&0x10))
        pCurrMB->m_cbpBits = 0;

    dmv_x  = dmv_x * (1+hpelfl);
    dmv_y  = dmv_y * (1+hpelfl);

    //set BLK_TYPE
    blk_type = (last_intra_flag&0x1) ?(uint8_t)VC1_BLK_INTRA:(uint8_t)VC1_BLK_INTER8X8;
    for(i = 0; i < 4; i++)
       pCurrMB->m_pBlocks[i].blkType  = blk_type;

    // all blocks are intra
    if (last_intra_flag&0x1)
    {
        pCurrMB->mbType   = VC1_MB_INTRA;

        if(!(last_intra_flag&0x10))
        {
            if(picLayerHeader->m_PQuant_mode>=VC1_ALTPQUANT_MB_LEVEL)
                GetMQUANT(pContext);

            //AC prediction if intra
            VC1_GET_BITS(1, pContext->m_pSingleMB->ACPRED);
        }
        else
        {
            //AC prediction if intra55
            VC1_GET_BITS(1, pContext->m_pSingleMB->ACPRED);

            //CBPCY decoding
            ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                &pContext->m_bitstream.bitOffset,
                &pCurrMB->m_cbpBits,
                picLayerHeader->m_pCurrCBPCYtbl);

            VM_ASSERT(ret == 0);


            //MB quant calculations
            if (picLayerHeader->m_PQuant_mode >= VC1_ALTPQUANT_MB_LEVEL)
                GetMQUANT(pContext);
           }
     }
    else
    {
         //motion vector predictors are calculated only for non-intra blocks, otherwise they are equal to zero (8.3.5.3)
        int16_t X = 0, Y = 0;
        Progressive1MVPrediction(pContext);
        // HYBRIDPRED is decoded in function PredictProgressive1MV
        CalculateProgressive1MV(pContext,&X, &Y);
        ApplyMVPrediction(pContext, 0, &X, &Y, dmv_x, dmv_y, 0);

        if(last_intra_flag&0x10)
        {
            //CBPCY decoding
            ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                &pContext->m_bitstream.bitOffset,
                &pCurrMB->m_cbpBits,
                picLayerHeader->m_pCurrCBPCYtbl);

            VM_ASSERT(ret == 0);


            //MB quant calculations
            if (picLayerHeader->m_PQuant_mode >= VC1_ALTPQUANT_MB_LEVEL)
                GetMQUANT(pContext);
        }
        else
        {
            //nothing more to do
            pCurrMB->m_cbpBits = 0;
        }
    }

    // all blocks in macroblock have one block type. So croma block have the same block type
    pCurrMB->m_pBlocks[4].blkType = pCurrMB->m_pBlocks[0].blkType;
    pCurrMB->m_pBlocks[5].blkType = pCurrMB->m_pBlocks[0].blkType;

    return VC1_OK;
}

static VC1Status MBLayer_ProgressivePpicture4MV(VC1Context* pContext)
{
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t i;
    int ret;
    int32_t Count_inter=0;
    int32_t n_block=0;
    int16_t dmv_x = 0;
    int16_t dmv_y = 0;

    uint32_t LeftTopRightPositionFlag = pCurrMB->LeftTopRightPositionFlag;

    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &pCurrMB->m_cbpBits,
                                     picLayerHeader->m_pCurrCBPCYtbl);
    VM_ASSERT(ret == 0);

    if (ret!=ippStsNoErr)
        return VC1_FAIL;

    Progressive4MVPrediction(pContext);

    for (i=0;i<4;i++)
    {
        if (pCurrMB->m_cbpBits&(1<<(5-i)))
        {
            uint16_t last_intra_flag = 0;

            //BLKMVDATA
            // for 4MV blocks hpelfl = 0
            last_intra_flag = DecodeMVDiff(pContext,0,&dmv_x,&dmv_y);
            //not_last = (uint8_t)(last_intra_flag>>4);
            //intra_flag = (uint8_t)(last_intra_flag & 0x1);

            pCurrMB->m_pBlocks[i].blkType = (last_intra_flag&0x1) ?
                                    (uint8_t)VC1_BLK_INTRA:(uint8_t)VC1_BLK_INTER8X8;

            if(!(last_intra_flag&0x10))
                pCurrMB->m_cbpBits = (uint8_t)(pCurrMB->m_cbpBits & ~(1 << (5 - i)));
        }
        else
        {
            dmv_x = 0;
            dmv_y = 0;
            pCurrMB->m_pBlocks[i].blkType = (uint8_t)picLayerHeader->TTFRM;
        }

        if (!(pCurrMB->m_pBlocks[i].blkType & VC1_BLK_INTRA))
        {
            int16_t X,Y;
            // HYBRIDPRED is decoded in function PredictProgressive4MV
            CalculateProgressive4MV(pContext,&X, &Y, i);
            ApplyMVPrediction(pContext, i, &X, &Y, dmv_x, dmv_y,0);
            // croma MVs are calculated in DeriveProgMV and it is called in function Interpolate block.
            Count_inter++;
            n_block = i;
        }
    }//end for

    //type for chroma blocks
    if (Count_inter>1)
    {
        pCurrMB->m_pBlocks[4].blkType = pCurrMB->m_pBlocks[n_block].blkType;
        pCurrMB->m_pBlocks[5].blkType = pCurrMB->m_pBlocks[n_block].blkType;
    }
    else
    {
        pCurrMB->m_pBlocks[4].blkType = VC1_BLK_INTRA;
        pCurrMB->m_pBlocks[5].blkType = VC1_BLK_INTRA;
    }

    if (Count_inter == 4 && (pCurrMB->m_cbpBits == 0))
        return VC1_OK;

    // MQDIFF, ABSMQ (7.1.3.4)

    if (picLayerHeader->m_PQuant_mode >= VC1_ALTPQUANT_MB_LEVEL)
        GetMQUANT(pContext);

    // if macroblock have predicted => ACPRED (7.1.3.2)
    {
        uint8_t c[6] = {0};
        uint8_t a[6] = {0};
        int32_t count = 0;
        uint32_t MaxWidth = pContext->m_seqLayerHeader.MaxWidthMB;

        pContext->m_pSingleMB->ACPRED =0;

        if (VC1_IS_NO_LEFT_MB(LeftTopRightPositionFlag))
        {
            c[0] = (uint8_t)((pCurrMB - 1)->m_pBlocks[1].blkType & VC1_BLK_INTRA);
            c[2] = (uint8_t)((pCurrMB - 1)->m_pBlocks[3].blkType & VC1_BLK_INTRA);
            c[4] = (uint8_t)((pCurrMB - 1)->m_pBlocks[4].blkType & VC1_BLK_INTRA);
            c[5] = (uint8_t)((pCurrMB - 1)->m_pBlocks[5].blkType & VC1_BLK_INTRA);
        }
        if (VC1_IS_NO_TOP_MB(LeftTopRightPositionFlag))
        {
            a[0] = (uint8_t)((pCurrMB - MaxWidth)->m_pBlocks[2].blkType & VC1_BLK_INTRA);
            a[1] = (uint8_t)((pCurrMB - MaxWidth)->m_pBlocks[3].blkType & VC1_BLK_INTRA);
            a[4] = (uint8_t)((pCurrMB - MaxWidth)->m_pBlocks[4].blkType & VC1_BLK_INTRA);
            a[5] = (uint8_t)((pCurrMB - MaxWidth)->m_pBlocks[5].blkType & VC1_BLK_INTRA);
        }
        c[1] = (uint8_t)(pCurrMB->m_pBlocks[0].blkType & VC1_BLK_INTRA);
        c[3] = (uint8_t)(pCurrMB->m_pBlocks[2].blkType & VC1_BLK_INTRA);
        a[2] = (uint8_t)(pCurrMB->m_pBlocks[0].blkType & VC1_BLK_INTRA);
        a[3] = (uint8_t)(pCurrMB->m_pBlocks[1].blkType & VC1_BLK_INTRA);

        for (i=0;i<VC1_NUM_OF_BLOCKS;i++)
        {
            count+=((pCurrMB->m_pBlocks[i].blkType & VC1_BLK_INTRA)&&((c[i])||(a[i])));
        }

        if (count)
            VC1_GET_BITS(1,pContext->m_pSingleMB->ACPRED);
    }

    return VC1_OK;
}

//Progressive-coded P picture MB
VC1Status MBLayer_ProgressivePpicture(VC1Context* pContext)
{
    int32_t SKIPMBBIT;
    uint32_t blk_num;
    VC1Status vc1Res=VC1_OK;

    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;

    if (picLayerHeader->PQUANT>=9)
        pCurrMB->Overlap = (uint8_t)pContext->m_seqLayerHeader.OVERLAP;
    else
        pCurrMB->Overlap =0;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,
                                        VM_STRING("\t\t\tX: %d, Y: %d\n"),
                                        sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif
    Set_MQuant(pContext);

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

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

    pCurrMB->mbType = VC1_MB_1MV_INTER | VC1_MB_FORWARD;

    if(picLayerHeader->MVMODE == VC1_MVMODE_MIXED_MV)
    {
        int32_t MVMODEBIT;
        //is a 1-bit field present in P frame macroblocks
        //if the frame level field MVTYPEMB (see section 3.2.1.21)
        //indicates that the raw mode is used. If MVMODEBIT = 0
        //then the macroblock is coded in 1MV mode and if
        //MVMODEBIT = 1 then the macroblock is coded in 4MV mode.
        if (VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->MVTYPEMB))
        {
            VC1_GET_BITS(1, MVMODEBIT);
        }
        else
            MVMODEBIT = picLayerHeader->MVTYPEMB.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +
                                                                            sMB->m_currMBXpos];

        if(MVMODEBIT == 1)
            pCurrMB->mbType = VC1_MB_4MV_INTER | VC1_MB_FORWARD;
    }

    if(VC1_IS_BITPLANE_RAW_MODE((&picLayerHeader->SKIPMB)))
    {
        //If SKIPMBBIT = 1 then the macroblock is skipped.
        VC1_GET_BITS(1, SKIPMBBIT);
    }
    else
        SKIPMBBIT = picLayerHeader->SKIPMB.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +
                                                                        sMB->m_currMBXpos];

    pCurrMB->SkipAndDirectFlag = (SKIPMBBIT<<1);

    if(SKIPMBBIT == 1)
    {
        MBLayer_ProgressivePskipped(pContext);
        CalculateIntraFlag(pContext);
    }
    else
    {
        memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

        if((VC1_GET_MBTYPE(pCurrMB->mbType))==VC1_MB_1MV_INTER)//1 MV mode
        {
            MBLayer_ProgressivePpicture1MV(pContext);
        }
        else //(4 MV Mode)
        {
            //3.2.2.3
            //CBPCY is a variable-length field present in both I picture and P
            //picture macroblock layers. Section 4.1.2.1 describes the CBPCY field
            //in I picture macroblocks and section 4.4.5.2 describes the CBPCY field
            //in P picture macroblocks.
            //CBPCY decoding
            MBLayer_ProgressivePpicture4MV(pContext);
        }
        //end 4mv mode

        if(pCurrMB->m_cbpBits && pContext->m_seqLayerHeader.VSTRANSFORM == 1)
        {
            if(picLayerHeader->TTMBF == 0 &&  !(pCurrMB->mbType & VC1_BLK_INTRA) )
            {
                GetTTMB(pContext);
            }
            else
            {
                if(pCurrMB->mbType != VC1_MB_INTRA)
                {
                    for(blk_num = 0; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
                        if(!(pCurrMB->m_pBlocks[blk_num].blkType & VC1_BLK_INTRA))
                            pCurrMB->m_pBlocks[blk_num].blkType = (uint8_t)picLayerHeader->TTFRM;
                }
            }
        }

            CalculateIntraFlag(pContext);

            uint32_t IntraFlag = pCurrMB->IntraFlag;

            if(IntraFlag)
                PDCPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);

            sMB->ZigzagTable = ZigZagTables_PB_luma[sMB->ACPRED];

            for(blk_num = 0; blk_num < VC1_NUM_OF_LUMA; blk_num++)
            {
                if(IntraFlag&1)
                    vc1Res = BLKLayer_Intra_Luma(pContext, blk_num, 128,sMB->ACPRED);
                else
                    vc1Res = BLKLayer_Inter_Luma(pContext, blk_num);

               if(vc1Res != VC1_OK)
                {
                    VM_ASSERT(0);
                    break;
                }
                IntraFlag >>= 1;
             }

            sMB->ZigzagTable = ZigZagTables_PB_chroma[sMB->ACPRED];

            if(IntraFlag & 1)
                for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
                {
                    //all MB intra
                    vc1Res = BLKLayer_Intra_Chroma(pContext, blk_num, 128, sMB->ACPRED);
                    if(vc1Res != VC1_OK)
                    {
                        VM_ASSERT(0);
                        break;
                    }
                }
            else
                for((void)blk_num; blk_num < VC1_NUM_OF_BLOCKS; blk_num++)
                {
                    //all MB inter
                    vc1Res = BLKLayer_Inter_Chroma(pContext, blk_num);
                    if(vc1Res != VC1_OK)
                    {
                        VM_ASSERT(0);
                        break;
                    }
                }

    }//skipmb

    AssignCodedBlockPattern(pCurrMB,sMB);

    return vc1Res;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
