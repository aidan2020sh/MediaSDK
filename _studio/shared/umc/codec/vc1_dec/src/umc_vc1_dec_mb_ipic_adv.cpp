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

typedef void (*IntraPrediction)(VC1Context* pContext);
static const IntraPrediction IntraPredictionTable[] =
{
        (IntraPrediction)(GetIntraDCPredictors),
        (IntraPrediction)(GetIntraScaleDCPredictors),
        (IntraPrediction)(GetIntraScaleDCPredictors)
};

VC1Status MBLayer_ProgressiveIpicture_Adv(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t CBPCY;//decoded_cbpy
    int ret;
    VC1Status vc1Res = VC1_OK;
    uint32_t ACPRED;

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,VM_STRING("\t\t\tX: %d, Y: %d\n"),
        sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif
    memset((pContext->savedMV + (sMB->m_currMBXpos + sMB->m_currMBYpos * pContext->m_seqLayerHeader.MaxWidthMB)*4),VC1_MVINTRA,sizeof(int16_t)*4);

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant(pContext);

    pContext->m_pCurrMB->mbType = VC1_MB_INTRA;

    //CBPCY is a variable-length field present in both I picture and P
    //picture macroblock layers.
    ret = DecodeHuffmanOne(   &pContext->m_bitstream.pBitstream,
                                        &pContext->m_bitstream.bitOffset,
                                        &CBPCY,
                                        pContext->m_vlcTbl->m_pCBPCY_Ipic);
    VM_ASSERT(ret == 0);

    pCurrMB->m_cbpBits = CalculateCBP(pCurrMB, CBPCY, pContext->m_seqLayerHeader.MaxWidthMB);


    // Check ACPRED coding mode
    {
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->ACPRED))
        {
            VC1_GET_BITS(1, ACPRED);
        }
        else
        {
            ACPRED = picLayerHeader->ACPRED.m_databits [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos + sMB->m_currMBXpos];
        }
    }

    // Overlap
    //for smoothing
    pCurrMB->Overlap = (uint8_t)pContext->m_seqLayerHeader.OVERLAP;
    
    if(pCurrMB->Overlap)
    {
        if(picLayerHeader->PQUANT>=9)
        {
            pCurrMB->Overlap=1;
        }
        else if(picLayerHeader->CONDOVER == VC1_COND_OVER_FLAG_NONE)
        {
            pCurrMB->Overlap=0;
        }
        else if( VC1_COND_OVER_FLAG_SOME == picLayerHeader->CONDOVER )
        {
            int32_t OverlapVal;
            if (VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->OVERFLAGS))
            {
                VC1_GET_BITS(1, OverlapVal);
            }
            else
            {
                OverlapVal = picLayerHeader->OVERFLAGS.m_databits
                    [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +  sMB->m_currMBXpos];

            }
            pCurrMB->Overlap = (uint8_t)OverlapVal;
        }
    }

    if (picLayerHeader->m_DQuantFRM)
        Set_Alt_MQUANT(pContext);

    memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

    //all bloks are INTRA
    pCurrMB->IntraFlag=0x3F;

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

    IntraPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);
    sMB->ZigzagTable = AdvZigZagTables_IProgressive_luma[ACPRED];

    for(i = 0; i < VC1_NUM_OF_LUMA; i++)
    {
        //all MB intra in I picture
       vc1Res = BLKLayer_Intra_Luma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    sMB->ZigzagTable = AdvZigZagTables_IProgressive_chroma[ACPRED];
    for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
    {
         //all MB intra in I picture
        vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    pCurrMB->FIELDTX = 0;
    return vc1Res;
}


VC1Status MBLayer_Frame_InterlaceIpicture(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;
    int32_t CBPCY;//decoded_cbpy
    int ret;
    VC1Status vc1Res = VC1_OK;
    uint32_t ACPRED;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,VM_STRING("\t\t\tX: %d, Y: %d\n"),
                        sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif
    memset((pContext->savedMV + (sMB->m_currMBXpos + sMB->m_currMBYpos * pContext->m_seqLayerHeader.MaxWidthMB)*4),VC1_MVINTRA,sizeof(int16_t)*4);
    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant(pContext);

    pCurrMB->mbType = VC1_MB_INTRA;

    //check fieldtx coding mode
    {
        int32_t FIELDTX;
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->FIELDTX))
        {
            VC1_GET_BITS(1, FIELDTX);
        }
        else
        {
            FIELDTX = picLayerHeader->FIELDTX.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos +  sMB->m_currMBXpos];
        }
        pCurrMB->FIELDTX = FIELDTX;
    }

    //CBPCY is a variable-length field present in both I picture and P
    //picture macroblock layers.
    ret = DecodeHuffmanOne(   &pContext->m_bitstream.pBitstream,
                                        &pContext->m_bitstream.bitOffset,
                                        &CBPCY,
                                        pContext->m_vlcTbl->m_pCBPCY_Ipic);
    VM_ASSERT(ret == 0);

    pCurrMB->m_cbpBits = CalculateCBP(pCurrMB, CBPCY, pContext->m_seqLayerHeader.MaxWidthMB);

    // Check ACPRED coding mode
    {
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->ACPRED))
        {
            VC1_GET_BITS(1, ACPRED);
        }
        else
        {
            ACPRED = pContext->m_picLayerHeader->ACPRED.m_databits
                [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos + sMB->m_currMBXpos];
        }
    }

     // Overlap
    //for smoothing
    pCurrMB->Overlap = (uint8_t)pContext->m_seqLayerHeader.OVERLAP;
    if(pCurrMB->Overlap)
    {
        if(picLayerHeader->PQUANT>=9)
        {
            pCurrMB->Overlap=1;
        }
        else if(picLayerHeader->CONDOVER == VC1_COND_OVER_FLAG_NONE)
        {
            pCurrMB->Overlap=0;
        }
        else if(VC1_COND_OVER_FLAG_SOME == picLayerHeader->CONDOVER )
        {
            int32_t OverlapVal;
            if (VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->OVERFLAGS))
            {
                VC1_GET_BITS(1, OverlapVal);
            }
            else
            {
                OverlapVal = picLayerHeader->OVERFLAGS.m_databits
                    [pContext->m_seqLayerHeader.MaxWidthMB * sMB->m_currMBYpos + sMB->m_currMBXpos];
            }
            pCurrMB->Overlap = (uint8_t)OverlapVal;
        }
    }

   if (picLayerHeader->m_DQuantFRM)
            Set_Alt_MQUANT(pContext);

    //all bloks are INTRA
    pCurrMB->IntraFlag=0x3F;

    memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

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

    IntraPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);

    sMB->ZigzagTable = AdvZigZagTables_IInterlace_luma[ACPRED];

    for(i = 0; i < VC1_NUM_OF_LUMA; i++)
    {
        //all MB intra in I picture
       vc1Res = BLKLayer_Intra_Luma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    sMB->ZigzagTable = AdvZigZagTables_IInterlace_chroma[ACPRED];

    for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
    {
         //all MB intra in I picture
        vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    return vc1Res;
}

VC1Status MBLayer_Field_InterlaceIpicture(VC1Context* pContext)
{
    int32_t i;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;
    VC1PictureLayerHeader* picLayerHeader = pContext->m_picLayerHeader;

    int32_t CBPCY;//decoded_cbpy
    int ret;
    VC1Status vc1Res = VC1_OK;
    uint32_t ACPRED;

    uint32_t currFieldMBYpos;
    uint32_t currFieldMBXpos;

   // pContext->m_pSingleMB->m_currMBXpos++;
    pCurrMB->FIELDTX = 0;


#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,VM_STRING("\t\t\tX: %d, Y: %d\n"),
                        sMB->m_currMBXpos,
                        sMB->m_currMBYpos - picLayerHeader->CurrField * sMB->heightMB/2);
#endif
    memset((pContext->savedMV + (sMB->m_currMBXpos + sMB->m_currMBYpos * pContext->m_seqLayerHeader.MaxWidthMB)*4),VC1_MVINTRA,sizeof(int16_t)*4);

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);

    Set_MQuant_Field(pContext);

    pCurrMB->mbType = VC1_MB_INTRA;

    //CBPCY is a variable-length field present in both I picture and P
    //picture macroblock layers.
    ret = DecodeHuffmanOne(   &pContext->m_bitstream.pBitstream,
                                        &pContext->m_bitstream.bitOffset,
                                        &CBPCY,
                                        pContext->m_vlcTbl->m_pCBPCY_Ipic);
    VM_ASSERT(ret == 0);

    pCurrMB->m_cbpBits = CalculateCBP(pCurrMB, CBPCY,pContext->m_seqLayerHeader.MaxWidthMB);


    // Check ACPRED coding mode
    {
        if(VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->ACPRED))
        {
            VC1_GET_BITS(1, ACPRED);
        }
        else {
            ACPRED = picLayerHeader->ACPRED.m_databits[pContext->m_seqLayerHeader.MaxWidthMB * sMB->slice_currMBYpos + sMB->m_currMBXpos];
        }
    }


    // Overlap
    //for smoothing
    pCurrMB->Overlap = (uint8_t)pContext->m_seqLayerHeader.OVERLAP;
    if(pCurrMB->Overlap)
    {
        if(picLayerHeader->PQUANT>=9)
        {
            pCurrMB->Overlap=1;
        }
        else if(picLayerHeader->CONDOVER == VC1_COND_OVER_FLAG_NONE)
        {
            pCurrMB->Overlap=0;
        }
        else if( VC1_COND_OVER_FLAG_SOME == picLayerHeader->CONDOVER )
        {
            int32_t OverlapVal;
            if (VC1_IS_BITPLANE_RAW_MODE(&picLayerHeader->OVERFLAGS))
            {
                VC1_GET_BITS(1, OverlapVal);
            }
            else
            {
               OverlapVal = picLayerHeader->OVERFLAGS.m_databits
                    [pContext->m_seqLayerHeader.MaxWidthMB * sMB->slice_currMBYpos + sMB->m_currMBXpos];
            }
            pContext->m_pCurrMB->Overlap = (uint8_t)OverlapVal;
        }
    }

    if (picLayerHeader->m_DQuantFRM)
        Set_Alt_MQUANT(pContext);

    //all bloks are INTRA
    pCurrMB->IntraFlag=0x3F;

    memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*VC1_NUM_OF_BLOCKS);

    currFieldMBYpos = sMB->m_currMBYpos;
    currFieldMBXpos = sMB->m_currMBXpos;

    if (picLayerHeader->CurrField)
        currFieldMBYpos -= (sMB->heightMB >> 1);

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

   pCurrMB->currYPitch *=2;
   pCurrMB->currUPitch *=2;
   pCurrMB->currVPitch *=2;

    IntraPredictionTable[pContext->m_seqLayerHeader.DQUANT](pContext);

    sMB->ZigzagTable = AdvZigZagTables_IField_luma[ACPRED];

    for(i = 0; i < VC1_NUM_OF_LUMA; i++)
    {
       //all MB intra in I picture
       vc1Res = BLKLayer_Intra_Luma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    sMB->ZigzagTable = AdvZigZagTables_IField_chroma[ACPRED];

    for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
    {
        //all MB intra in I picture
        vc1Res = BLKLayer_Intra_Chroma_Adv(pContext, i, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    return vc1Res;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER
