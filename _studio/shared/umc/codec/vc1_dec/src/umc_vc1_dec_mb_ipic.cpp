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

typedef void (*IntraPrediction)(VC1Context* pContext);
static const IntraPrediction IntraPredictionTable[] =
{
        (IntraPrediction)(GetIntraDCPredictors),
        (IntraPrediction)(GetIntraScaleDCPredictors),
        (IntraPrediction)(GetIntraScaleDCPredictors)
};

//Figure 15: Syntax diagram for macroblock layer bitstream in
//Progressive-coded I picture
VC1Status MBLayer_ProgressiveIpicture(VC1Context* pContext)
{
    int32_t i;
    int32_t CBPCY;//decoded_cbpy
    int ret;
    VC1Status vc1Res = VC1_OK;
    uint32_t ACPRED = 0;
    VC1MB* pCurrMB = pContext->m_pCurrMB;
    VC1SingletonMB* sMB = pContext->m_pSingleMB;

    pCurrMB->bias = 0;

    //pContext->m_pSingleMB->m_currMBXpos++;
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_POSITION,
                                            VM_STRING("\t\t\tX: %d, Y: %d\n"),
                                            sMB->m_currMBXpos, sMB->m_currMBYpos);
#endif

    memset((pContext->savedMV + (sMB->m_currMBXpos + sMB->m_currMBYpos * pContext->m_seqLayerHeader.MaxWidthMB)*4),VC1_MVINTRA,sizeof(int16_t)*4);

    pCurrMB->mbType = VC1_MB_INTRA;

    if (pContext->m_picLayerHeader->PQUANT >= 9)
    {
        pCurrMB->Overlap = (uint8_t)pContext->m_seqLayerHeader.OVERLAP;
        pCurrMB->bias = 128 * pCurrMB->Overlap;
    }
    else
        pCurrMB->Overlap =0;

    //CBPCY is a variable-length field present in both I picture and P
    //picture macroblock layers.
    ret = DecodeHuffmanOne(&pContext->m_bitstream.pBitstream,
                                     &pContext->m_bitstream.bitOffset,
                                     &CBPCY,
                                     pContext->m_vlcTbl->m_pCBPCY_Ipic);

    VM_ASSERT(ret == 0);

    pCurrMB->LeftTopRightPositionFlag = CalculateLeftTopRightPositionFlag(sMB);


    pCurrMB->m_cbpBits = CalculateCBP(pCurrMB, CBPCY, pContext->m_seqLayerHeader.MaxWidthMB);

    //3.2.2.4
    //The ACPRED field is present in all I picture macroblocks and in 1MV
    //Intra macroblocks in P pictures (see section 4.4.5.1 for a description
    //of the macroblock types). This is a 1-bit field that specifies whether
    //the blocks were coded using AC prediction. ACPRED = 0 indicates that
    //AC prediction is not used. ACPRED = 1 indicates that AC prediction is
    //used.  See section 4.1.2.2 for a description of the ACPRED field in I
    //pictures and section 4.4.6.1 for a description of the ACPRED field in
    //P pictures.
    VC1_GET_BITS(1, ACPRED);

    memset(pContext->m_pBlock, 0, sizeof(int16_t)*8*8*6);

    pContext->CurrDC->DoubleQuant = (uint8_t)(2*pContext->m_picLayerHeader->PQUANT +
                                                pContext->m_picLayerHeader->HALFQP);

    pContext->CurrDC->DCStepSize = GetDCStepSize(pContext->m_picLayerHeader->PQUANT);

#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_QUANT,
                    VM_STRING("MB Quant = %d\n"),
                    pContext->m_picLayerHeader->PQUANT);
    VM_Debug::GetInstance(VC1DebugRoutine).vm_debug_frame(-1,VC1_QUANT,
                    VM_STRING("HalfQ = %d\n"),
                    pContext->m_picLayerHeader->HALFQP);
#endif

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
    sMB->ZigzagTable = ZigZagTables_I_luma[ACPRED];

    for(i = 0; i < VC1_NUM_OF_LUMA; i++)
    {
        //all MB intra in I picture
        vc1Res = BLKLayer_Intra_Luma(pContext, i, pCurrMB->bias, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    sMB->ZigzagTable = ZigZagTables_I_chroma[ACPRED];

    for((void)i; i < VC1_NUM_OF_BLOCKS; i++)
    {
        //all MB intra in I picture
        vc1Res = BLKLayer_Intra_Chroma(pContext, i, pCurrMB->bias, ACPRED);
        if(vc1Res != VC1_OK)
        {
            VM_ASSERT(0);
            break;
        }
    }

    return vc1Res;
}

#endif //MFX_ENABLE_VC1_VIDEO_DECODE