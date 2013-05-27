/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 Intel Corporation. All Rights Reserved.
//
//
//          VC-1 (VC1) encoder, functions for deblocking
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _UMC_VC1_ENC_SMOOTHING_SM_H_
#define _UMC_VC1_ENC_SMOOTHING_SM_H_

#include "umc_vc1_enc_mb.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_enc_common.h"

namespace UMC_VC1_ENCODER
{
    typedef struct
    {
        VC1EncoderMBData* pCurRData;
        VC1EncoderMBData* pLeftRData;
        VC1EncoderMBData* pLeftTopRData;
        VC1EncoderMBData* pTopRData;
        Ipp8u* pRFrameY;
        Ipp32s uiRDataStepY;
        Ipp8u* pRFrameU;
        Ipp32s uiRDataStepU;
        Ipp8u* pRFrameV;
        Ipp32s uiRDataStepV;
    }SmoothInfo_I_SM;

    typedef struct
    {
        VC1EncoderMBData* pCurRData;
        VC1EncoderMBData* pLeftRData;
        VC1EncoderMBData* pLeftTopRData;
        VC1EncoderMBData* pTopRData;
        Ipp8u* pRFrameY;
        Ipp32s uiRDataStepY;
        Ipp8u* pRFrameU;
        Ipp32s uiRDataStepU;
        Ipp8u* pRFrameV;
        Ipp32s uiRDataStepV;
        Ipp8u  curIntra;
        Ipp8u  leftIntra;
        Ipp8u  leftTopIntra;
        Ipp8u  topIntra;
    }SmoothInfo_P_SM;

#define VC1_ENC_IS_BLOCK_0_INTRA(value)    (Ipp32u)((value&0x20)!=0)
#define VC1_ENC_IS_BLOCK_1_INTRA(value)    (Ipp32u)((value&0x10)!=0)
#define VC1_ENC_IS_BLOCK_2_INTRA(value)    (Ipp32u)((value&0x08)!=0)
#define VC1_ENC_IS_BLOCK_3_INTRA(value)    (Ipp32u)((value&0x04)!=0)
#define VC1_ENC_IS_BLOCK_4_INTRA(value)    (Ipp32u)((value&0x02)!=0)
#define VC1_ENC_IS_BLOCK_5_INTRA(value)    (Ipp32u)((value&0x01)!=0)
#define VC1_ENC_IS_BLOCK_0_1_INTRA(value)  (Ipp32u)((value&0x30)!=0)
#define VC1_ENC_IS_BLOCK_0_2_INTRA(value)  (Ipp32u)((value&0x28)!=0)
#define VC1_ENC_IS_BLOCK_1_3_INTRA(value)  (Ipp32u)((value&0x18)!=0)
#define VC1_ENC_IS_BLOCK_2_3_INTRA(value)  (Ipp32u)((value&0x0C)!=0)

///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////I frames simple profile///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
    inline void NoSmoothMB_I_SM(SmoothInfo_I_SM* /*pSmothInfo*/, Ipp32s /*j*/)
    {
    }

    //------------------------------------------------------
    //------------ smoothing Main/Simple profile -----------
    inline void Smooth_LeftMB_I_SM_YV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);
        //CHROMA

    }

    inline void Smooth_LeftMB_I_SM_NV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);
        //CHROMA

    }

    inline void Smoth_TopMB_I_SM_YV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6,  pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],       pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                        EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                 EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }

    inline void Smoth_TopMB_I_SM_NV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6,  pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],       pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                        EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                 EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }

    inline void Smoth_TopRightMB_I_SM_YV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo-> pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

    }

    inline void Smoth_TopRightMB_I_SM_NV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo-> pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

    }

    inline void Smoth_MB_I_SM_YV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //left/left top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/ left top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }

    inline void Smoth_MB_I_SM_NV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //left/left top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/ left top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }

    inline void Smoth_RightMB_I_SM_YV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //cur MB/Top horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[2] + 6 * pSmothInfo->pTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pCurRData->m_pBlock[0],     pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //left/ left top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/ left top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //cur/ top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[4] + 6*pSmothInfo->pTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //cur/ top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[5] + 6*pSmothInfo->pTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }

    inline void Smoth_RightMB_I_SM_NV12(SmoothInfo_I_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //cur MB/Top horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[2] + 6 * pSmothInfo->pTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pCurRData->m_pBlock[0],     pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur internal horizontal edge
        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        //left/cur U vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/cur V vertical edge
        Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //left/ left top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //left/ left top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);

        //cur/ top U horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[4] + 6*pSmothInfo->pTopRData->m_uiBlockStep[4]/2,
                                                             pSmothInfo->pTopRData->m_uiBlockStep[4],
                                                             pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                             pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepU);
        assert(Sts == ippStsNoErr);

        //cur/ top V horizontal edge
        Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[5] + 6*pSmothInfo->pTopRData->m_uiBlockStep[5]/2,
                                                             pSmothInfo->pTopRData->m_uiBlockStep[5],
                                                             pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                             pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                             pSmothInfo->uiRDataStepV);
        assert(Sts == ippStsNoErr);
    }
    typedef void(*fSmooth_I_MB_SM)(SmoothInfo_I_SM* pSmothInfo, Ipp32s j);


    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_YV12[8];
    extern fSmooth_I_MB_SM Smooth_I_MBFunction_SM_NV12[8];

///////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////P frames simple profile///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////
   inline void NoSmoothMB_P_SM(SmoothInfo_P_SM* /*pSmothInfo*/, Ipp32s /*j*/)
    {
    }

    inline void Smooth_LeftMB_P_SM_YV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);
        //CHROMA

    }

    inline void Smooth_LeftMB_P_SM_NV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));
        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);
        //CHROMA

    }

    inline void Smoth_TopMB_P_SM_YV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;

        Ipp32u EdgeDisabledFlag = 0;
        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],       pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                        EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {
            //CHROMA
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }
    }

    inline void Smoth_TopMB_P_SM_NV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;

        Ipp32u EdgeDisabledFlag = 0;
        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],       pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                        EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {
            //CHROMA
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }
    }

    inline void Smoth_TopRightMB_P_SM_YV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],    pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,              EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }

     inline void Smoth_TopRightMB_P_SM_NV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],    pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,              EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }
     
    inline void Smoth_MB_P_SM_YV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftTopIntra))
        {  
            //left/left top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/ left top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }

    inline void Smoth_MB_P_SM_NV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = IPPVC_EDGE_HALF_1 | IPPVC_EDGE_HALF_2;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftTopIntra))
        {  
            //left/left top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/ left top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }

    inline void Smoth_RightMB_P_SM_YV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //cur MB/Top horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->topIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->topIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[2] + 6 * pSmothInfo->pTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pCurRData->m_pBlock[0],     pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftTopIntra))
        {  
            //left/ left top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/ left top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->topIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //cur/ top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[4] + 6*pSmothInfo->pTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //cur/ top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[5] + 6*pSmothInfo->pTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }

    inline void Smoth_RightMB_P_SM_NV12(SmoothInfo_P_SM* pSmothInfo, Ipp32s j)
    {
        IppStatus Sts = ippStsNoErr;
        Ipp32u EdgeDisabledFlag = 0;

        //LUMA
        //left/cur vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftIntra) * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[1] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[1],
                                                           pSmothInfo->pCurRData->m_pBlock[0],      pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                       EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //internal vertical edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_1_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_2_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6, pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[1],     pSmothInfo->pCurRData->m_uiBlockStep[1],
                                                           pSmothInfo->pRFrameY + 8 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,
                                                           0,                                      EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[0] + 6 * pSmothInfo->pLeftRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pLeftRData->m_pBlock[2],     pSmothInfo->pLeftRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //left MB/leftTop horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->leftIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->leftTopIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->leftIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[2] + 6 * pSmothInfo->pLeftTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pLeftTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pLeftRData->m_pBlock[0],     pSmothInfo->pLeftRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY - 16 + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,                EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);


        //cur MB/Top horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_2_INTRA(pSmothInfo->topIntra) * VC1_ENC_IS_BLOCK_0_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_3_INTRA(pSmothInfo->topIntra) * VC1_ENC_IS_BLOCK_1_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[2] + 6 * pSmothInfo->pTopRData->m_uiBlockStep[2]/2,
                                                           pSmothInfo->pTopRData->m_uiBlockStep[2],
                                                           pSmothInfo->pCurRData->m_pBlock[0],     pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pRFrameY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //cur internal horizontal edge
        EdgeDisabledFlag = (IPPVC_EDGE_HALF_1 * VC1_ENC_IS_BLOCK_0_2_INTRA(pSmothInfo->curIntra))
            | (IPPVC_EDGE_HALF_2 * VC1_ENC_IS_BLOCK_1_3_INTRA(pSmothInfo->curIntra));

        Sts = _own_ippiSmoothingLuma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pCurRData->m_pBlock[0] + 6 * pSmothInfo->pCurRData->m_uiBlockStep[0]/2,
                                                           pSmothInfo->pCurRData->m_uiBlockStep[0],
                                                           pSmothInfo->pCurRData->m_pBlock[2],     pSmothInfo->pCurRData->m_uiBlockStep[2],
                                                           pSmothInfo->pRFrameY + 8*pSmothInfo->uiRDataStepY + VC1_ENC_LUMA_SIZE * j,
                                                           pSmothInfo->uiRDataStepY,               EdgeDisabledFlag);
        assert(Sts == ippStsNoErr);

        //CHROMA
        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //left/cur U vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[4] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/cur V vertical edge
            Sts = _own_ippiSmoothingChroma_VerEdge_VC1_16s8u_C1R(pSmothInfo->pLeftRData->m_pBlock[5] + 6, pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->leftTopIntra))
        {  
            //left/ left top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[4] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pLeftRData->m_pBlock[4],      pSmothInfo->pLeftRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //left/ left top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pLeftTopRData->m_pBlock[5] + 6*pSmothInfo->pLeftTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pLeftTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pLeftRData->m_pBlock[5],      pSmothInfo->pLeftRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV - 8 + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

        if(VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->topIntra) && VC1_ENC_IS_BLOCK_4_INTRA(pSmothInfo->curIntra))
        {  
            //cur/ top U horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[4] + 6*pSmothInfo->pTopRData->m_uiBlockStep[4]/2,
                                                                 pSmothInfo->pTopRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pCurRData->m_pBlock[4],      pSmothInfo->pCurRData->m_uiBlockStep[4],
                                                                 pSmothInfo->pRFrameU + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepU);
            assert(Sts == ippStsNoErr);

            //cur/ top V horizontal edge
            Sts = _own_ippiSmoothingChroma_HorEdge_VC1_16s8u_C1R(pSmothInfo->pTopRData->m_pBlock[5] + 6*pSmothInfo->pTopRData->m_uiBlockStep[5]/2,
                                                                 pSmothInfo->pTopRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pCurRData->m_pBlock[5],      pSmothInfo->pCurRData->m_uiBlockStep[5],
                                                                 pSmothInfo->pRFrameV + VC1_ENC_CHROMA_SIZE * j*2,
                                                                 pSmothInfo->uiRDataStepV);
            assert(Sts == ippStsNoErr);
        }

    }

    typedef void(*fSmooth_P_MB_SM)(SmoothInfo_P_SM* pSmothInfo, Ipp32s j);


    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_YV12[8];
    extern fSmooth_P_MB_SM Smooth_P_MBFunction_SM_NV12[8];

}
#endif
#endif

