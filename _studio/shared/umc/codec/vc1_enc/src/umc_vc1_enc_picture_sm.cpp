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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_picture_sm.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_tables.h"
#include "umc_vc1_common_zigzag_tbl.h"
#include "ippvc.h"
#include "ippi.h"
#include "umc_vc1_enc_block.h"
#include "umc_vc1_enc_block_template.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_enc_statistic.h"
#include "umc_vc1_enc_pred.h"
#include "umc_vc1_enc_mode_decision.h"
#include <new.h>

namespace UMC_VC1_ENCODER
{
UMC::Status  VC1EncoderPictureSM::CheckParameters(vm_char* pLastError)
{
    if (!m_pSequenceHeader)
    {
        vm_string_sprintf(pLastError,VM_STRING("Error. pic. header parameter: seq. header is NULL\n"));
        return UMC::UMC_ERR_FAILED;
    }
    if (m_uiPictureType != VC1_ENC_I_FRAME &&
        m_uiPictureType != VC1_ENC_P_FRAME &&
       (!((m_uiPictureType == VC1_ENC_BI_FRAME||m_uiPictureType == VC1_ENC_B_FRAME)&& m_pSequenceHeader->IsBFrames())))
    {
        vm_string_sprintf(pLastError,VM_STRING("Error. pic. header parameter: picture type is incorrect\n"));
        return UMC::UMC_ERR_FAILED;
    }
    if (m_bFrameInterpolation && !m_pSequenceHeader->IsFrameInterpolation())
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: FrameInterpolation\n"));
        m_bFrameInterpolation = false;
    }
    if (m_bRangeRedution && !m_pSequenceHeader->IsRangeRedution())
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: RangeRedution\n"));
        m_bRangeRedution = false;
    }
    if (m_uiFrameCount >3 )
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: FrameCount\n"));
        m_uiFrameCount = 0;
    }
    if (m_uiResolution && m_pSequenceHeader->IsMultiRes() || m_uiResolution>3)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: Resolution\n"));
        m_uiResolution = 0;
    }
    if (m_uiDecTypeAC1>2)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC1\n"));
        m_uiDecTypeAC1 = 0;
    }

    if (m_uiDecTypeDCIntra>1)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC2\n"));
        m_uiDecTypeDCIntra = 0;
    }
    if (!m_uiQuantIndex || m_uiQuantIndex >31)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiQuantIndex\n"));
        m_uiQuantIndex = 1;
    }
    if (m_bHalfQuant && m_uiQuantIndex>8)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_bHalfQuant\n"));
        m_bHalfQuant = false;
    }

    /*if(m_uiMVMode == VC1_ENC_MIXED_QUARTER_BICUBIC && (m_uiPictureType == VC1_ENC_P_FRAME || m_uiPictureType == VC1_ENC_B_FRAME))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiMVMode\n"));
        m_uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;
    }*/

    switch (m_pSequenceHeader->GetQuantType())
    {
    case VC1_ENC_QTYPE_IMPL:
        m_bUniformQuant = (m_uiQuantIndex>8)? false:true;
        m_uiQuant       = quantValue[m_uiQuantIndex];
        break;
    case VC1_ENC_QTYPE_UF:
        m_bUniformQuant = true;
        m_uiQuant       = m_uiQuantIndex;
        break;
    case VC1_ENC_QTYPE_NUF:
        m_bUniformQuant = false;
        m_uiQuant       = m_uiQuantIndex;
        break;
    }
    if (m_uiMVRangeIndex>3 || (!m_uiMVRangeIndex && m_pSequenceHeader->IsExtendedMV()))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: RangeIndex\n"));
        m_uiMVRangeIndex = 0;
    }
    if (m_uiPictureType == VC1_ENC_I_FRAME )
    {
        return CheckParametersI(pLastError);
    }
    return CheckParametersP(pLastError);
}
UMC::Status  VC1EncoderPictureSM::CheckParametersI(vm_char* pLastError)
{
    if (m_uiDecTypeAC2>2)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: DecTypeAC2\n"));
        m_uiDecTypeAC2 = 0;
    }
    if ((m_uiBFraction.denom < 2|| m_uiBFraction.denom >8)&& (m_uiBFraction.denom != 0xff))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.denom\n"));
        m_uiBFraction.denom = 2;
    }
    if ((m_uiBFraction.num  < 1|| m_uiBFraction.num >= m_uiBFraction.denom)&& (m_uiBFraction.num != 0xff))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: m_uiBFraction.num\n"));
        m_uiBFraction.num = 1;
    }
    return UMC::UMC_OK;
}
UMC::Status  VC1EncoderPictureSM::CheckParametersP(vm_char* pLastError)
{

    if (m_bIntensity &&  (m_pSequenceHeader->GetProfile() == VC1_ENC_PROFILE_S))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: Intensity=0\n"));
        m_bIntensity = false;
    }
    if (m_bIntensity)
    {
        if (m_uiIntensityLumaScale>63)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=63\n"));
            m_uiIntensityLumaScale=63;
        }
        if (m_uiIntensityLumaShift>63)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=63\n"));
            m_uiIntensityLumaShift=63;
        }
    }
    else
    {
        if (m_uiIntensityLumaScale>0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaScale=0\n"));
            m_uiIntensityLumaScale=0;
        }
        if (m_uiIntensityLumaShift>0)
        {
            vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: IntensityLumaShift=0\n"));
            m_uiIntensityLumaShift=0;
        }
    }
    if (m_uiMVTab>4)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: MVTab=4\n"));
        m_uiMVTab=4;
    }
    if (m_uiCBPTab>4)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: CBPTab=4\n"));
        m_uiCBPTab=4;
    }
    if (m_uiAltPQuant>0 && m_pSequenceHeader->GetDQuant()==0)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=0\n"));
        m_uiAltPQuant=0;
    }
    else if (m_pSequenceHeader->GetDQuant()!=0 && (m_uiAltPQuant==0||m_uiAltPQuant>31))
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: AltPQuant=Quant\n"));
        m_uiAltPQuant = m_uiQuant;
    }
    if ( m_QuantMode != VC1_ENC_QUANT_SINGLE && m_pSequenceHeader->GetDQuant()==0)
    {
        vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: QuantMode = VC1_ENC_QUANT_SINGLE\n"));
        m_QuantMode = VC1_ENC_QUANT_SINGLE;
    }
    if (m_bVSTransform && !m_pSequenceHeader->IsVSTransform())
    {
       vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: VSTransform = false\n"));
       m_bVSTransform = false;
    }
    if (!m_bVSTransform && m_uiTransformType!= VC1_ENC_8x8_TRANSFORM)
    {
       vm_string_sprintf(pLastError,VM_STRING("Warning. pic. header parameter: TransformType == VC1_ENC_8x8_TRANSFORM\n"));
       m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
    }
    return UMC::UMC_OK;
}

UMC::Status VC1EncoderPictureSM::Init (VC1EncoderSequenceSM* SequenceHeader, VC1EncoderMBs* pMBs,  VC1EncoderCodedMB* pCodedMB)
{
    //UMC::Status err = UMC::UMC_OK;

    if (!SequenceHeader || !pMBs || !pCodedMB)
        return UMC::UMC_ERR_NULL_PTR;

    Close();

    m_pSequenceHeader = SequenceHeader;

    m_pMBs     = pMBs;
    m_pCodedMB = pCodedMB;

    return UMC::UMC_OK;
}

UMC::Status VC1EncoderPictureSM::Close()
{

   Reset();
   ResetPlanes();
   return UMC::UMC_OK;
}

UMC::Status     VC1EncoderPictureSM::SetPlaneParams  (Frame* pFrame, ePlaneType type)
{
    uint8_t** pPlane;
    uint32_t* pPlaneStep;
    uint32_t *padding;
    uint32_t temp;
    switch(type)
    {
    case  VC1_ENC_CURR_PLANE:
        pPlane     = m_pPlane;
        pPlaneStep = m_uiPlaneStep;
        padding    = &temp;
        break;
    case  VC1_ENC_RAISED_PLANE:
        pPlane     = m_pRaisedPlane;
        pPlaneStep = m_uiRaisedPlaneStep;
        padding    = &m_uiRaisedPlanePadding;
        break;
    case  VC1_ENC_FORWARD_PLANE:
        pPlane     = m_pForwardPlane;
        pPlaneStep = m_uiForwardPlaneStep;
        padding    = &m_uiForwardPlanePadding;
        break;
     case  VC1_ENC_BACKWARD_PLANE:
        pPlane     = m_pBackwardPlane;
        pPlaneStep = m_uiBackwardPlaneStep;
        padding    = &m_uiBackwardPlanePadding;
        break;
     default:
         return UMC::UMC_ERR_FAILED;
   }
   pPlane[0]       = pFrame->GetYPlane();
   pPlane[1]       = pFrame->GetUPlane();
   pPlane[2]       = pFrame->GetVPlane();
   pPlaneStep[0]   = pFrame->GetYStep();
   pPlaneStep[1]   = pFrame->GetUStep();
   pPlaneStep[2]   = pFrame->GetVStep();
   *padding        = pFrame->GetPaddingSize();

   return UMC::UMC_OK;
}


UMC::Status VC1EncoderPictureSM::SetInitPictureParams(InitPictureParams* InitPicParam)
{
    m_uiMVMode           = InitPicParam->uiMVMode;
    m_uiBFraction.num    = InitPicParam->uiBFraction.num;
    m_uiBFraction.denom  = InitPicParam->uiBFraction.denom;
    m_uiMVRangeIndex     = InitPicParam->uiMVRangeIndex;

    m_uiMVTab           = InitPicParam->sVLCTablesIndex.uiMVTab;
    m_uiDecTypeAC1      = InitPicParam->sVLCTablesIndex.uiDecTypeAC; 
    m_uiCBPTab          = InitPicParam->sVLCTablesIndex.uiCBPTab; 
    m_uiDecTypeDCIntra  = InitPicParam->sVLCTablesIndex.uiDecTypeDCIntra;

    return UMC::UMC_OK;
}

UMC::Status VC1EncoderPictureSM::SetPictureParams(ePType Type, int16_t* pSavedMV)
{
    m_uiPictureType     =  Type;

    m_pSavedMV  = pSavedMV;

    if (m_pSequenceHeader->IsFrameInterpolation())
        m_bFrameInterpolation   =  false;              // true or false
    else
        m_bFrameInterpolation   =  false;

    if (m_pSequenceHeader->IsRangeRedution())
        m_bRangeRedution      = false;
    else
        m_bRangeRedution      = false;                  // Should be equal to 0 in simple profile

    //if (m_pSequenceHeader->IsExtendedMV())
    //    m_uiMVRangeIndex = 1;                            // [0,3]
    //else
    //    m_uiMVRangeIndex = 0;

    switch (m_uiPictureType)
    {
    case VC1_ENC_I_FRAME:
        return SetPictureParamsI();
    case VC1_ENC_P_FRAME:
        return SetPictureParamsP();
    case VC1_ENC_B_FRAME:
        return SetPictureParamsB();
    case VC1_ENC_SKIP_FRAME:
        return UMC::UMC_OK;
    }
    return UMC::UMC_ERR_NOT_IMPLEMENTED;

}

UMC::Status VC1EncoderPictureSM::SetPictureQuantParams(uint8_t uiQuantIndex, bool bHalfQuant)
{
    m_uiQuantIndex = uiQuantIndex;
    m_bHalfQuant = bHalfQuant;

     if (m_uiQuantIndex > 31)
         return UMC::UMC_ERR_FAILED;

     if (m_uiQuantIndex == 0)
         return UMC::UMC_ERR_FAILED;

    switch (m_pSequenceHeader->GetQuantType())
    {
    case VC1_ENC_QTYPE_IMPL:
        m_bUniformQuant = (m_uiQuantIndex>8)? false:true;
        m_uiQuant       = quantValue[m_uiQuantIndex];
        break;
    case VC1_ENC_QTYPE_EXPL:
        m_bUniformQuant = true;                         // can be true or false
        m_uiQuant       = m_uiQuantIndex;
        break;
    case VC1_ENC_QTYPE_UF:
        m_bUniformQuant = true;
        m_uiQuant       = m_uiQuantIndex;
        break;
    case VC1_ENC_QTYPE_NUF:
        m_bUniformQuant = false;
        m_uiQuant       = m_uiQuantIndex;
        break;
    default:
        return UMC::UMC_ERR_FAILED;
    }

    return UMC::UMC_OK;
}


UMC::Status VC1EncoderPictureSM::SetPictureParamsI()
{
    m_uiDecTypeAC2          = 0;                                // [0,2] - it's used to choose decoding table

    if (m_pSequenceHeader->IsMultiRes())
        m_uiResolution = VC1_ENC_RES_FULL_HOR | VC1_ENC_RES_FULL_VER;
                                                        /*..HALF_HOR|..HALF_VER, ..FULL_HOR |
                                                         ..HALF_VER, ..HALF_HOR | ..FULL_VER*/
    else
        m_uiResolution = VC1_ENC_RES_FULL_HOR | VC1_ENC_RES_FULL_VER;

    return UMC::UMC_OK;
}
UMC::Status VC1EncoderPictureSM::SetPictureParamsP()
{
    //m_uiMVMode = (bMixed)? VC1_ENC_MIXED_QUARTER_BICUBIC: VC1_ENC_1MV_HALF_BILINEAR;
    //                                                            //    VC1_ENC_1MV_HALF_BILINEAR
    //                                                            //    VC1_ENC_1MV_QUARTER_BICUBIC
    //                                                            //    VC1_ENC_1MV_HALF_BICUBIC
    //                                                            //    VC1_ENC_MIXED_QUARTER_BICUBIC
    if (m_pSequenceHeader->IsMultiRes())
        m_uiResolution = VC1_ENC_RES_FULL_HOR | VC1_ENC_RES_FULL_VER;
                                                        /*..HALF_HOR|..HALF_VER, ..FULL_HOR |
                                                         ..HALF_VER, ..HALF_HOR | ..FULL_VER*/
    else
        m_uiResolution = VC1_ENC_RES_FULL_HOR | VC1_ENC_RES_FULL_VER;

    if (m_pSequenceHeader->GetProfile() == VC1_ENC_PROFILE_S)
        m_bIntensity  = false;

    if (m_pSequenceHeader->GetDQuant()==0)
    {
        m_uiAltPQuant=0;
    }
    else
    {
        m_uiAltPQuant = m_uiQuant;          // [1,31]
    }

    switch( m_pSequenceHeader->GetDQuant())
    {
    case 0:
        m_QuantMode = VC1_ENC_QUANT_SINGLE;
        break;
    case 1:
         m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
                                            //        VC1_ENC_QUANT_MB_ANY,
                                            //        VC1_ENC_QUANT_MB_PAIR,
                                            //        VC1_ENC_QUANT_EDGE_ALL,
                                            //        VC1_ENC_QUANT_EDGE_LEFT,
                                            //        VC1_ENC_QUANT_EDGE_TOP,
                                            //        VC1_ENC_QUANT_EDGE_RIGHT,
                                            //        VC1_ENC_QUANT_EDGE_BOTTOM,
                                            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
                                            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
                                            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
                                            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
         break;
    case 2:
        m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
        break;
    }
    if (!m_pSequenceHeader->IsVSTransform())
    {
       m_bVSTransform = false;
    }
    else
    {
        m_bVSTransform = false;                          // true & false

    }
    if (!m_bVSTransform)
    {
       m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
    }
    else
    {
        m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
                                                        //VC1_ENC_8x4_TRANSFORM
                                                        //VC1_ENC_4x8_TRANSFORM
                                                        //VC1_ENC_4x4_TRANSFORM

    }
    return UMC::UMC_OK;


}
UMC::Status VC1EncoderPictureSM::SetPictureParamsB()
{
   //m_uiMVMode = VC1_ENC_1MV_HALF_BILINEAR;                      //    VC1_ENC_1MV_HALF_BILINEAR
   //                                                             //    VC1_ENC_1MV_QUARTER_BICUBIC

    m_uiCBPTab  =0;                                             //[0,4]

    if (m_pSequenceHeader->GetDQuant()==0)
    {
        m_uiAltPQuant=0;
    }
    else
    {
        m_uiAltPQuant = m_uiQuant;          // [1,31]
    }

    switch( m_pSequenceHeader->GetDQuant())
    {
    case 0:
        m_QuantMode = VC1_ENC_QUANT_SINGLE;
        break;
    case 1:
         m_QuantMode = VC1_ENC_QUANT_SINGLE;//        VC1_ENC_QUANT_SINGLE,
                                            //        VC1_ENC_QUANT_MB_ANY,
                                            //        VC1_ENC_QUANT_MB_PAIR,
                                            //        VC1_ENC_QUANT_EDGE_ALL,
                                            //        VC1_ENC_QUANT_EDGE_LEFT,
                                            //        VC1_ENC_QUANT_EDGE_TOP,
                                            //        VC1_ENC_QUANT_EDGE_RIGHT,
                                            //        VC1_ENC_QUANT_EDGE_BOTTOM,
                                            //        VC1_ENC_QUANT_EDGES_LEFT_TOP,
                                            //        VC1_ENC_QUANT_EDGES_TOP_RIGHT,
                                            //        VC1_ENC_QUANT_EDGES_RIGHT_BOTTOM,
                                            //        VC1_ENC_QUANT_EDGSE_BOTTOM_LEFT,
         break;
    case 2:
        m_QuantMode = VC1_ENC_QUANT_MB_PAIR;
        break;
    }
    if (!m_pSequenceHeader->IsVSTransform())
    {
       m_bVSTransform = false;  // true & false
    }
    else
    {
        m_bVSTransform = false;

    }
    if (!m_bVSTransform)
    {
       m_uiTransformType = VC1_ENC_8x8_TRANSFORM;
    }
    else
    {
        m_uiTransformType = VC1_ENC_8x8_TRANSFORM;      //VC1_ENC_8x8_TRANSFORM
                                                        //VC1_ENC_8x4_TRANSFORM
                                                        //VC1_ENC_4x8_TRANSFORM
                                                        //VC1_ENC_4x4_TRANSFORM
    }
    return UMC::UMC_OK;
}

UMC::Status VC1EncoderPictureSM::WriteSkipPictureHeader(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status err = UMC::UMC_OK;
    err = pCodedPicture->MakeBlankSegment(2);
    if (err!= UMC::UMC_OK)
        return err;
    err = pCodedPicture->PutBits(0,8);
    return err;

}
UMC::Status VC1EncoderPictureSM::WriteIPictureHeader(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status err      =   UMC::UMC_OK;
    bool   bBframes =   m_pSequenceHeader->IsBFrames();

    err = pCodedPicture->MakeBlankSegment(2);
    if (err!= UMC::UMC_OK)
        return err;

    if (m_pSequenceHeader->IsFrameInterpolation())
    {
        err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(m_uiFrameCount,2);
    if (err != UMC::UMC_OK)
        return err;
    if (m_pSequenceHeader->IsRangeRedution())
    {
        err = pCodedPicture->PutBits(m_bRangeRedution,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(frameTypeCodesVLC[bBframes][2*0],frameTypeCodesVLC[bBframes][2*0+1]);
    if (err!= UMC::UMC_OK)
        return err;

    err = pCodedPicture->PutBits(0,7);    /*buffer fullness - 7 bits*/
    if (err != UMC::UMC_OK)
        return err;

    err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuantIndex <= 8)
    {
        err = pCodedPicture->PutBits(m_bHalfQuant, 1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
    {
        err = pCodedPicture->PutBits(m_bUniformQuant,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->IsExtendedMV())
    {
        err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->IsMultiRes())
    {
        err = pCodedPicture->PutBits(this->m_uiResolution,2);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC2],ACTableCodesVLC[2*m_uiDecTypeAC2+1]);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);

    return err;
}

UMC::Status VC1EncoderPictureSM::CompletePicture(VC1EncoderBitStreamSM* pCodedPicture, double dPTS, size_t  len)
{
    UMC::Status   err       = UMC::UMC_OK;
    uint32_t   temp           = 0;
    uint32_t   dataLen        = 0;
    uint32_t   uTime          = (uint32_t)(dPTS*1000000.0);




    dataLen = pCodedPicture->GetDataLen() - len - 8;
    assert(dataLen>=0);



    temp = ((m_uiPictureType == VC1_ENC_I_FRAME)<< 31) | (dataLen & 0x00FFFFFF);

    err = pCodedPicture->FillBlankSegment(temp);
    VC1_ENC_CHECK (err)

    err = pCodedPicture->FillBlankSegment(uTime);
    VC1_ENC_CHECK (err)

    pCodedPicture->DeleteBlankSegment();
    return err;
}

UMC::Status  VC1EncoderPictureSM::PAC_IFrame(UMC::MeParams* pME)
{
    UMC::Status                 err = UMC::UMC_OK;
    int32_t                      i=0, j=0, blk = 0;
    bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

    uint8_t*                      pCurMBRow[3] =  {m_pPlane[0],m_pPlane[1],m_pPlane[2]};
    int32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    int32_t                      w = m_pSequenceHeader->GetNumMBInRow();


    //forward transform quantization

    IntraTransformQuantFunction    TransformQuantACFunctionLuma = (m_bUniformQuant) ? 
        ((pME&&pME->UseTrellisQuantization)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
        IntraTransformQuantNonUniform;
    IntraTransformQuantFunction    TransformQuantACFunctionChroma = (m_bUniformQuant) ? 
        ((pME&&pME->UseTrellisQuantizationChroma)? IntraTransformQuantTrellis: IntraTransformQuantUniform) :
        IntraTransformQuantNonUniform;

    IntraTransformQuantFunction    TransformQuantACFunction[6] = 
    {
        TransformQuantACFunctionLuma,
        TransformQuantACFunctionLuma,
        TransformQuantACFunctionLuma,
        TransformQuantACFunctionLuma,
        TransformQuantACFunctionChroma,
        TransformQuantACFunctionChroma
    };

    //inverse transform quantization
    IntraInvTransformQuantFunction InvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
                                                                        IntraInvTransformQuantNonUniform;

    uint8_t                       defDCPredCoeff  = (m_pSequenceHeader->IsOverlap()&& m_uiQuant>=9)? 0:1;
    eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
    bool                        dACPrediction   = true;
    mfxSize                    blkSize     = {8,8};
    mfxSize                    blkSizeLuma = {16,16};

    uint8_t deblkPattern = 0;//3 bits: right 1 bit - left/not left
                           //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row
    uint8_t deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFE : 0;

    uint8_t smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;

    uint8_t smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
                                            //0/1 - top/not top, 0/1 - left/not left
    SmoothInfo_I_SM smoothInfo = {0};

    uint8_t                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
    uint8_t                       DCQuant         =  DCQuantValues[m_uiQuant];
    int16_t                      defPredictor    =  defDCPredCoeff*(1024 + (DCQuant>>1))/DCQuant;


    if (m_pSavedMV)
    {
        memset(m_pSavedMV,0,w*h*2*sizeof(int16_t));
    }
    err = m_pMBs->Reset();
    if (err != UMC::UMC_OK)
        return err;

#ifdef VC1_ENC_DEBUG_ON
        pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
        pDebug->SetVTSFlag(false);
#endif

    for (i=0; i < h; i++)
    {
        uint8_t *pRFrameY = 0;
        uint8_t *pRFrameU = 0;
        uint8_t *pRFrameV = 0;

        if (bRaiseFrame)
        {
            pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
            pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
            pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
        }
        for (j=0; j < w; j++)
        {
            uint8_t               MBPattern  = 0;
            uint8_t               CBPCY      = 0;

            VC1EncoderMBInfo*   pCurMBInfo = 0;
            VC1EncoderMBData*   pCurMBData = 0;
            VC1EncoderMBData*   pRecMBData = 0;

            int32_t              xLuma      = VC1_ENC_LUMA_SIZE*j;
            int32_t              xChroma    = VC1_ENC_CHROMA_SIZE*j;
            NeighbouringMBsData MBs;

            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

            pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
            pCurMBData  =   m_pMBs->GetCurrMBData();
            pRecMBData  =   m_pMBs->GetRecCurrMBData();

            MBs.LeftMB    = m_pMBs->GetLeftMBData();
            MBs.TopMB     = m_pMBs->GetTopMBData();
            MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();

            pCompressedMB ->Init(VC1_ENC_I_MB);
            pCurMBInfo->Init(true);

            /*------------------- Compressing  ------------------------------------------------------*/

            pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);


            for (blk = 0; blk<6; blk++)
            {
                if (!defDCPredCoeff)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],  blkSize, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                }
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                TransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                         DCQuant, doubleQuant,pME->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
            }

            /* intra prediction Result is stored in temp block, becouse
               current block is saved for next prediction*/

STATISTICS_START_TIME(m_TStat->Intra_StartTime);
            dACPrediction = DCACPredictionIntraFrameSM( pCurMBData,&MBs,
                                                        pRecMBData, defPredictor,
                                                        direction);
STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);


            for (blk=0; blk<6; blk++)
            {
                pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                                pRecMBData->m_uiBlockStep[blk],
                                                ZagTables_I[direction[blk]* dACPrediction],
                                                blk);
            }


            MBPattern = pCompressedMB->GetMBPattern();

            VC1EncoderMBInfo* t = m_pMBs->GetTopMBInfo();
            VC1EncoderMBInfo* l = m_pMBs->GetLeftMBInfo();
            VC1EncoderMBInfo* tl = m_pMBs->GetTopLeftMBInfo();

            CBPCY = Get_CBPCY(MBPattern, (t)? t->GetPattern():0, (l)? l->GetPattern():0, (tl)? tl->GetPattern():0);

            pCurMBInfo->SetMBPattern(MBPattern);

            pCompressedMB->SetACPrediction(dACPrediction);
            pCompressedMB->SetMBCBPCY(CBPCY);


#ifdef VC1_ENC_DEBUG_ON
            pDebug->SetMBType(VC1_ENC_I_MB);
            pDebug->SetCPB(MBPattern, CBPCY);
            pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
            /*--------------Reconstruction (if is needed)--------------------------------------------------*/
STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
            if (bRaiseFrame)
            {

                for (blk=0;blk<6; blk++)
                {
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                   InvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                               pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                               DCQuant, doubleQuant);
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);
                   if (!defDCPredCoeff)
                   {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                       _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk], pRecMBData->m_uiBlockStep[blk],  blkSize, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                   }
                }

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

//smoothing
{
    smoothInfo.pCurRData      = pRecMBData;
    smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
    smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
    smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
    smoothInfo.pRFrameY       = pRFrameY;
    smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
    smoothInfo.pRFrameU       = pRFrameU;
    smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
    smoothInfo.pRFrameV       = pRFrameV;
    smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];

    m_pSM_I_MB[smoothPattern](&smoothInfo, j);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
#endif
    smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
}

//deblocking
STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
{
                    uint8_t *DeblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                    m_pDeblk_I_MB[deblkPattern](DeblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, j);

                    deblkPattern = deblkPattern | 0x1;
}
STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            }

STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
            /*------------------------------------------------------------------------------------------------*/
#ifdef VC1_ENC_DEBUG_ON
            pDebug->SetDblkHorEdgeLuma(0, 0, 15, 15);
            pDebug->SetDblkVerEdgeLuma(0, 0, 15, 15);
            pDebug->SetDblkHorEdgeU(0,15);
            pDebug->SetDblkHorEdgeV(0, 15);
            pDebug->SetDblkVerEdgeU(0, 15);
            pDebug->SetDblkVerEdgeV(0, 15);
#endif
            err = m_pMBs->NextMB();
            if (err != UMC::UMC_OK && j < w-1)
                return err;

#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
        }

        deblkPattern = (deblkPattern  | 0x2 | ( (! (uint8_t)((i + 1 - (h -1)) >> 31)<<2 )))& deblkMask;
        smoothPattern = 0x6 & smoothMask;

//Row deblocking
//STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
//        if (m_pSequenceHeader->IsLoopFilter()&& bRaiseFrame && i!=0 )
//        {
//STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
//            uint8_t *planes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
//                                m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
//                                m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
//
//            if(i!=h-1)
//                Deblock_I_FrameRow(planes, m_uiRaisedPlaneStep, w, m_uiQuant);
//            else
//                Deblock_I_FrameBottomRow(planes, m_uiRaisedPlaneStep, w, m_uiQuant);
//STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
//        }
//STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

        err = m_pMBs->NextRow();
        if (err != UMC::UMC_OK)
            return err;

        pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;
    }



#ifdef VC1_ENC_DEBUG_ON
        if(bRaiseFrame)
            pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
                                    m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
                                    m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
    //static FILE* f=0;
    //if (!f)
    //    f=fopen("1.yuv","wb");
    //for (i=0; i < h*16; i++)
    //{
    //    fwrite(m_pRaisedPlane[0]+i*m_uiRaisedPlaneStep[0],1,w*16,f);

    //}
    //for (i=0; i < h*8; i++)
    //{
    //    fwrite(m_pRaisedPlane[1]+i*m_uiRaisedPlaneStep[1],1,w*8,f);

    //}
    //for (i=0; i < h*8; i++)
    //{
    //    fwrite(m_pRaisedPlane[2]+i*m_uiRaisedPlaneStep[2],1,w*8,f);

    //}
    //fflush(f);
#endif

    return err;
}

UMC::Status  VC1EncoderPictureSM::VLC_IFrame(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status  err = UMC::UMC_OK;
    uint32_t       i=0, j=0, blk = 0;

    uint32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t                      w = m_pSequenceHeader->GetNumMBInRow();

    eCodingSet   LumaCodingSet   = LumaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC2];
    eCodingSet   ChromaCodingSet = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];


    const sACTablesSet* pACTablesVLC[6] = {&ACTablesSet[LumaCodingSet],
                                           &ACTablesSet[LumaCodingSet],
                                           &ACTablesSet[LumaCodingSet],
                                           &ACTablesSet[LumaCodingSet],
                                           &ACTablesSet[ChromaCodingSet],
                                           &ACTablesSet[ChromaCodingSet]};

    const uint32_t*        pDCTableVLC[6] = {DCTables[m_uiDecTypeDCIntra][0],
                                           DCTables[m_uiDecTypeDCIntra][0],
                                           DCTables[m_uiDecTypeDCIntra][0],
                                           DCTables[m_uiDecTypeDCIntra][0],
                                           DCTables[m_uiDecTypeDCIntra][1],
                                           DCTables[m_uiDecTypeDCIntra][1]};

    sACEscInfo   ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
                               Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                               0, 0};

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetCurrMBFirst();
#endif

    err = WriteIPictureHeader(pCodedPicture);
    if (err != UMC::UMC_OK)
        return err;

    for (i=0; i < h; i++)
    {
        for (j=0; j < w; j++)
        {
            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
            m_MECurMbStat->MbType = UMC::ME_MbIntra;  //could be not correct
            m_MECurMbStat->whole = 0;
            memset(m_MECurMbStat->MVF, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->MVB, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->coeff, 0, 6*sizeof(uint16_t));
            m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

            pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
#endif

            err = pCompressedMB->WriteMBHeaderI_SM(pCodedPicture, m_bRawBitplanes);
            if (err != UMC::UMC_OK)
                return err;

            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
            for (blk = 0; blk<6; blk++)
            {
                err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLC[blk],m_uiQuant,blk);
                VC1_ENC_CHECK (err)

                    err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesVLC[blk],&ACEscInfo,blk);
                VC1_ENC_CHECK (err)
            }//for
            STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
            m_MECurMbStat++;
#endif
        }
    }
    return err;
}

UMC::Status VC1EncoderPictureSM::SetInterpolationParams (IppVCInterpolateBlock_8u* pY,
                                                          IppVCInterpolateBlock_8u* pU,
                                                          IppVCInterpolateBlock_8u* pV,
                                                          uint8_t* buffer,
                                                          uint32_t w,
                                                          uint32_t h,
                                                          uint8_t **pPlane,
                                                          uint32_t *pStep,
                                                          bool bField)
{
    UMC::Status ret = UMC::UMC_OK;
    uint32_t lumaShift   = (bField)? 1:0;
    uint32_t chromaShift = (bField)? 2:1;

  
    pY->pSrc [0]           = pPlane[0];
    pY->srcStep            = pStep[0];
    pY->pDst[0]            = buffer;
    pY->dstStep            = VC1_ENC_LUMA_SIZE;
    pV->pointVector.x      = 0;
    pV->pointVector.y      = 0;
    pY->sizeBlock.height   = VC1_ENC_LUMA_SIZE;
    pY->sizeBlock.width    = VC1_ENC_LUMA_SIZE;
    pY->sizeFrame.width    = w >> lumaShift;
    pY->sizeFrame.height   = h >> lumaShift;

    pU->pSrc[0]            = pPlane[1] ;
    pU->srcStep            = pStep[1];
    pU->pDst[0]            = buffer + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
    pU->dstStep            = VC1_ENC_CHROMA_SIZE<<1;
    pV->pointVector.x      = 0;
    pV->pointVector.y      = 0;
    pU->sizeBlock.height   = VC1_ENC_CHROMA_SIZE;
    pU->sizeBlock.width    = VC1_ENC_CHROMA_SIZE;
    pU->sizeFrame.width    = w>>chromaShift;
    pU->sizeFrame.height   = h>>chromaShift;

    pV->pSrc[0]             = pPlane[2];
    pV->srcStep             = pStep[2];
    pV->pDst[0]             = pU->pDst[0] + VC1_ENC_CHROMA_SIZE;
    pV->dstStep             = pU->dstStep;
    pV->pointVector.x       = 0;
    pV->pointVector.y       = 0;
    pV->sizeBlock.height    = VC1_ENC_CHROMA_SIZE;
    pV->sizeBlock.width     = VC1_ENC_CHROMA_SIZE;
    pV->sizeFrame.width     = w>>chromaShift;
    pV->sizeFrame.height    = h>>chromaShift;

    return ret;
}
UMC::Status  VC1EncoderPictureSM::SetInterpolationParams(IppVCInterpolate_8u* pY,
                                                         IppVCInterpolate_8u* pU,
                                                         IppVCInterpolate_8u* pV,
                                                         uint8_t* buffer,
                                                         bool bForward)
{
    UMC::Status ret = UMC::UMC_OK;
    uint8_t **pPlane;
    uint32_t *pStep;


    if (bForward)
    {
        pPlane = m_pForwardPlane;
        pStep  = m_uiForwardPlaneStep;
    }
    else
    {
        pPlane = m_pBackwardPlane;
        pStep  = m_uiBackwardPlaneStep;
    }

    pY->pSrc            = pPlane[0];
    pY->srcStep         = pStep[0];
    pY->pDst            = buffer;
    pY->dstStep         = VC1_ENC_LUMA_SIZE;
    pY->dx              = 0;
    pY->dy              = 0;
    pY->roundControl    = m_uiRoundControl;
    pY->roiSize.height  = VC1_ENC_LUMA_SIZE;
    pY->roiSize.width   = VC1_ENC_LUMA_SIZE;

    pU->pSrc            = pPlane[1];
    pU->srcStep         = pStep[1];
    pU->pDst            = buffer + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
    pU->dstStep         = (VC1_ENC_CHROMA_SIZE << 1);
    pU->dx              = 0;
    pU->dy              = 0;
    pU->roundControl    = m_uiRoundControl;
    pU->roiSize.height  = VC1_ENC_CHROMA_SIZE;
    pU->roiSize.width   = VC1_ENC_CHROMA_SIZE;

    pV->pSrc            = pPlane[2];
    pV->srcStep         = pStep[2];
    pV->pDst            = pU->pDst + VC1_ENC_CHROMA_SIZE;
    pV->dstStep         = pU->dstStep;
    pV->dx              = 0;
    pV->dy              = 0;
    pV->roundControl    = m_uiRoundControl;
    pV->roiSize.height  = VC1_ENC_CHROMA_SIZE;
    pV->roiSize.width   = VC1_ENC_CHROMA_SIZE;

    return ret;
}

//motion estimation
UMC::Status  VC1EncoderPictureSM::SetInterpolationParams4MV( IppVCInterpolate_8u* pY,
                                                              IppVCInterpolate_8u* pU,
                                                              IppVCInterpolate_8u* pV,
                                                              uint8_t* buffer,
                                                              bool bForward)
{
    UMC::Status ret = UMC::UMC_OK;
    uint8_t **pPlane;
    uint32_t *pStep;


    if (bForward)
    {
        pPlane = m_pForwardPlane;
        pStep  = m_uiForwardPlaneStep;
    }
    else
    {
        pPlane = m_pBackwardPlane;
        pStep  = m_uiBackwardPlaneStep;
    }

    pY[0].pSrc            = pPlane[0];
    pY[0].srcStep         = pStep[0];
    pY[0].pDst            = buffer;
    pY[0].dstStep         = VC1_ENC_LUMA_SIZE;
    pY[0].dx              = 0;
    pY[0].dy              = 0;
    pY[0].roundControl    = m_uiRoundControl;
    pY[0].roiSize.height  = 8;
    pY[0].roiSize.width   = 8;

    pY[1].pSrc            = pPlane[0];
    pY[1].srcStep         = pStep[0];
    pY[1].pDst            = buffer+8;
    pY[1].dstStep         = VC1_ENC_LUMA_SIZE;
    pY[1].dx              = 0;
    pY[1].dy              = 0;
    pY[1].roundControl    = m_uiRoundControl;
    pY[1].roiSize.height  = 8;
    pY[1].roiSize.width   = 8;

    pY[2].pSrc            = pPlane[0];
    pY[2].srcStep         = pStep[0];
    pY[2].pDst            = buffer+8*VC1_ENC_LUMA_SIZE;
    pY[2].dstStep         = VC1_ENC_LUMA_SIZE;
    pY[2].dx              = 0;
    pY[2].dy              = 0;
    pY[2].roundControl    = m_uiRoundControl;
    pY[2].roiSize.height  = 8;
    pY[2].roiSize.width   = 8;

    pY[3].pSrc            = pPlane[0];
    pY[3].srcStep         = pStep[0];
    pY[3].pDst            = buffer+8*VC1_ENC_LUMA_SIZE+8;
    pY[3].dstStep         = VC1_ENC_LUMA_SIZE;
    pY[3].dx              = 0;
    pY[3].dy              = 0;
    pY[3].roundControl    = m_uiRoundControl;
    pY[3].roiSize.height  = 8;
    pY[3].roiSize.width   = 8;

    pU->pSrc            = pPlane[1];
    pU->srcStep         = pStep[1];
    pU->pDst            = buffer + VC1_ENC_LUMA_SIZE*VC1_ENC_LUMA_SIZE;
    pU->dstStep         = VC1_ENC_CHROMA_SIZE<<1;
    pU->dx              = 0;
    pU->dy              = 0;
    pU->roundControl    = m_uiRoundControl;
    pU->roiSize.height  = VC1_ENC_CHROMA_SIZE;
    pU->roiSize.width   = VC1_ENC_CHROMA_SIZE;

    pV->pSrc            = pPlane[2];
    pV->srcStep         = pStep[2];
    pV->pDst            = pU->pDst + VC1_ENC_CHROMA_SIZE;
    pV->dstStep         = pU->dstStep;
    pV->dx              = 0;
    pV->dy              = 0;
    pV->roundControl    = m_uiRoundControl;
    pV->roiSize.height  = VC1_ENC_CHROMA_SIZE;
    pV->roiSize.width   = VC1_ENC_CHROMA_SIZE;

    return ret;
}

UMC::Status VC1EncoderPictureSM::PAC_PFrame(UMC::MeParams* MEParams)
{
    UMC::Status  err = UMC::UMC_OK;
    int32_t                      i=0, j=0, blk = 0;
    bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

    uint8_t*                      pCurMBRow[3] = {m_pPlane[0],        m_pPlane[1],        m_pPlane[2]};
    uint8_t*                      pFMBRow  [3] = {m_pForwardPlane[0], m_pForwardPlane[1], m_pForwardPlane[2]};

    int32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    int32_t                      w = m_pSequenceHeader->GetNumMBInRow();

    //forward transform quantization

    _SetIntraInterLumaFunctions;
    
    
    //inverse transform quantization
    IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
                                                                        IntraInvTransformQuantNonUniform;

    InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
                                                                        InterInvTransformQuantNonUniform;

    CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?
                                                        GetChromaMVFast:GetChromaMV;

    bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
    InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)?
                                    _own_ippiInterpolateQPBilinear_VC1_8u_C1R:
                                    _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

    InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)?
                                                                    own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R:
                                                                    own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

    uint8_t                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

    IppVCInterpolate_8u         sYInterpolation;
    IppVCInterpolate_8u         sUInterpolation;
    IppVCInterpolate_8u         sVInterpolation;
    IppVCInterpolateBlock_8u    InterpolateBlockY;
    IppVCInterpolateBlock_8u    InterpolateBlockU;
    IppVCInterpolateBlock_8u    InterpolateBlockV;

    eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
    bool                        dACPrediction   = true;
    mfxSize                    blkSize     = {8,8};
    mfxSize                    blkSizeLuma = {16,16};


    uint8_t                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
    uint8_t                       DCQuant         =  DCQuantValues[m_uiQuant];
    int16_t                      defPredictor    =  0;//defDCPredCoeff*(1024 + (DCQuant>>1))/DCQuant;
    int16_t*                     pSavedMV = m_pSavedMV;

    sCoordinate                 MVPredMBMin = {-60,-60};
    sCoordinate                 MVPredMBMax = {(int16_t)w*16*4-4, (int16_t)h*16*4-4};

    sCoordinate                 MVSavedMBMin = {-8,-8};
    sCoordinate                 MVSavedMBMax = {(int16_t)w*8, (int16_t)h*8};
    bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
                                           m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

    bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

    bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
    fGetExternalEdge            pGetExternalEdge    = GetExternalEdge_SM[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
    fGetInternalEdge            pGetInternalEdge    = GetInternalEdge_SM[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

    uint8_t deblkPattern = 0;//4 bits: right 1 bit - 0 - left/1 - not left,
                           //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row
                           //middle 1 bit - 1 - right/0 - not right
    uint8_t deblkExceptoin = 0xFF; //if fist block intra equal to 0

    uint8_t deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

    uint8_t smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
    uint8_t smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
                                            //0/1 - top/not top, 0/1 - left/not left

    SmoothInfo_P_SM smoothInfo = {0};

    IppStatus                  ippSts  = ippStsNoErr;

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetInterpolType(bIsBilinearInterpolation);
    pDebug->SetRounControl(m_uiRoundControl);
    pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
    pDebug->SetVTSFlag(bSubBlkTS);
#endif

    if (m_pSavedMV)
    {
        memset(m_pSavedMV,0,w*h*2*sizeof(int16_t));
    }


   err = m_pMBs->Reset();
   if (err != UMC::UMC_OK)
        return err;

    SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
                           tempInterpolationBuffer,true);

    SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
                            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
                            m_pForwardPlane, m_uiForwardPlaneStep, false);


    if(MEParams->pSrc->MBs[0].MbType == UMC::ME_MbIntra)
        deblkExceptoin = 0;

#ifdef VC1_ENC_CHECK_MV
    err = CheckMEMV_P(MEParams,bMVHalf);
    assert(err == UMC::UMC_OK);
#endif

    for (i=0; i < h; i++)
    {
        uint8_t *pRFrameY = 0;
        uint8_t *pRFrameU = 0;
        uint8_t *pRFrameV = 0;

        if (bRaiseFrame)
        {
            pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
            pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
            pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
        }
        for (j=0; j < w; j++)
        {
            int32_t              xLuma       =  VC1_ENC_LUMA_SIZE*j;
            int32_t              xChroma     =  VC1_ENC_CHROMA_SIZE*j;

            sCoordinate         MVInt       = {0,0};
            sCoordinate         MVQuarter   = {0,0};
            sCoordinate         MV          = {0,0};
            uint8_t               MBPattern   = 0;
            uint8_t               CBPCY       = 0;

            VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
            VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
            VC1EncoderMBData*   pRecMBData  = m_pMBs->GetRecCurrMBData();

            VC1EncoderMBInfo* left          = m_pMBs->GetLeftMBInfo();
            VC1EncoderMBInfo* topLeft       = m_pMBs->GetTopLeftMBInfo();
            VC1EncoderMBInfo* top           = m_pMBs->GetTopMBInfo();
            VC1EncoderMBInfo* topRight      = m_pMBs->GetTopRightMBInfo();

            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

            eMBType MBType;

            switch (MEParams->pSrc->MBs[j + i*w].MbType)
            {
            case UMC::ME_MbIntra:
                {
                NeighbouringMBsData MBs;

                MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;

                MV.x = 0;
                MV.y =0;

                MBType = VC1_ENC_P_MB_INTRA;
                pCompressedMB->Init(VC1_ENC_P_MB_INTRA);
                pCurMBInfo->Init(true);

                /*-------------------------- Compression ----------------------------------------------------------*/

                pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                //only intra blocks:
                for (blk = 0; blk<6; blk++)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                            DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                }

STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                dACPrediction = DCACPredictionFrame( pCurMBData,&MBs,
                                                     pRecMBData, defPredictor,direction);
STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                for (blk=0; blk<6; blk++)
                {
                    pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                                    pRecMBData->m_uiBlockStep[blk],
                                                    VC1_Inter_8x8_Scan,
                                                    blk);
                }

                MBPattern = pCompressedMB->GetMBPattern();
                CBPCY = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetACPrediction(dACPrediction);
                pCompressedMB->SetMBCBPCY(CBPCY);
                pCurMBInfo->SetEdgesIntra(i==0, j==0);

#ifdef VC1_ENC_DEBUG_ON
            pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
            pDebug->SetCPB(MBPattern, CBPCY);
            pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
            pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif

                /*-------------------------- Reconstruction ----------------------------------------------------------*/
STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {

                    for (blk=0;blk<6; blk++)
                    {
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                        pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                        DCQuant, doubleQuant);
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize,0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    }

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

            //smoothing
            {

                smoothInfo.pCurRData      = pRecMBData;
                smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                smoothInfo.pRFrameY       = pRFrameY;
                smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                smoothInfo.pRFrameU       = pRFrameU;
                smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                smoothInfo.pRFrameV       = pRFrameV;
                smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                smoothInfo.curIntra       = 0x3F;
                smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

            #ifdef VC1_ENC_DEBUG_ON
                pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
            #endif
            }

            //deblocking
            {
                STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                uint8_t *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                deblkPattern = deblkPattern | 0x1 | ((!(uint8_t)((j + 1 - (w -1)) >> 31)<<1));
                STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
            }
        }
STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);
                /*------------------------------------------- ----------------------------------------------------------*/
                }
            break;
            case UMC::ME_MbFrw:
            case UMC::ME_MbFrwSkipped:
                {
                sCoordinate                 MVIntChroma     = {0,0};
                sCoordinate                 MVQuarterChroma = {0,0};
                sCoordinate                 MVChroma       =  {0,0};

                sCoordinate                 *mvC=0, *mvB=0, *mvA=0;
                sCoordinate                 mv1, mv2, mv3;
                sCoordinate                 mvPred;
                sCoordinate                 mvDiff;
                eTransformType              BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                                m_uiTransformType, m_uiTransformType,
                                                                m_uiTransformType, m_uiTransformType};

                uint8_t                       hybrid = 0;

                MBType = (UMC::ME_MbFrw == MEParams->pSrc->MBs[j + i*w].MbType)?
                                                            VC1_ENC_P_MB_1MV:VC1_ENC_P_MB_SKIP_1MV;

                MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                pCurMBInfo->Init(false);
                pCompressedMB->Init(MBType);

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                /*MV prediction */
                GetMVPredictionP(true)
                PredictMV(mvA,mvB,mvC, &mvPred);
                ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                hybrid = HybridPrediction(&mvPred,&MV,mvA,mvC,32);
                pCompressedMB->SetHybrid(hybrid);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                {
                    //correct ME results
                    VM_ASSERT(MV.x==mvPred.x && MV.y==mvPred.y);
                    MV.x=mvPred.x;
                    MV.y=mvPred.y;
                }
                pCurMBInfo->SetMV(MV);
                GetIntQuarterMV(MV,&MVInt, &MVQuarter);

                if (VC1_ENC_P_MB_SKIP_1MV != MBType || bRaiseFrame)
                {
                    /*interpolation*/
                    CalculateChroma(MV,&MVChroma);
                    pCurMBInfo->SetMVChroma(MVChroma);
                    GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                    if (m_uiForwardPlanePadding)
                    {
STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        SetInterpolationParamsLuma(&sYInterpolation, pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt, &MVQuarter, j);
                        ippSts = InterpolateLumaFunction(&sYInterpolation);
                        VC1_ENC_IPP_CHECK(ippSts);

                        InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                           &MVIntChroma, &MVQuarterChroma, j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                    }
                    else
                    {
                        bool bOpposite = 0;

                        STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                        IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                        SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                        ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,bOpposite,false,m_uiRoundControl,0);
                        VC1_ENC_IPP_CHECK(ippSts);

                        InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                            bOpposite,false,m_uiRoundControl,0,
                            j, i ,  &MVIntChroma, &MVQuarterChroma);

                        IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                        STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);         
                    
                    }
                } //interpolation
                if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                {

                    if (bCalculateVSTransform)
                    {
#ifndef OWN_VS_TRANSFORM
                        GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                        VC1EncMD_P pIn;
                        pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                        pIn.srcYStep = m_uiPlaneStep[0];
                        pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                        pIn.srcUStep = m_uiPlaneStep[1];
                        pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                        pIn.srcVStep = m_uiPlaneStep[2];

                        pIn.pYRef    = sYInterpolation.pDst; 
                        pIn.refYStep = sYInterpolation.dstStep;      
                        pIn.pURef    = sUInterpolation.pDst; 
                        pIn.refUStep = sUInterpolation.dstStep;
                        pIn.pVRef    = sVInterpolation.pDst; 
                        pIn.refVStep = sVInterpolation.dstStep;

                        pIn.quant         = doubleQuant;
                        pIn.bUniform      = m_bUniformQuant;
                        pIn.intraPattern  = 0;
                        pIn.DecTypeAC1    = m_uiDecTypeAC1;
                        pIn.pScanMatrix   = ZagTables;
                        pIn.bField        = 0;
                        pIn.CBPTab        = m_uiCBPTab;

                        GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                    }
                    pCompressedMB->SetTSType(BlockTSTypes);


                    mvDiff.x = MV.x - mvPred.x;
                    mvDiff.y = MV.y - mvPred.y;
                    pCompressedMB->SetdMV(mvDiff);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                            sYInterpolation.pDst, sYInterpolation.dstStep,
                            sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                            j,0);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    for (blk = 0; blk<6; blk++)
                    {
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                        InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                    BlockTSTypes[blk], doubleQuant, MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                                    pCurMBData->m_uiBlockStep[blk],
                                                    ZagTables[BlockTSTypes[blk]],
                                                    blk);
                    }
                    MBPattern = pCompressedMB->GetMBPattern();
                    CBPCY = MBPattern;
                    pCurMBInfo->SetMBPattern(MBPattern);
                    pCompressedMB->SetMBCBPCY(CBPCY);
                    if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                    {
                        pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                        MBType = VC1_ENC_P_MB_SKIP_1MV;
                    }
                }//VC1_ENC_P_MB_SKIP_1MV != MBType

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(MBType);
                pDebug->SetMVInfo(&MV,  mvPred.x,mvPred.y, 0);
                pDebug->SetIntrpMV(MV.x, MV.y, 0);
                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);

                if (pCompressedMB->isSkip() )
                {
                    pDebug->SetMBAsSkip();
                }
                else
                {
                    pDebug->SetCPB(MBPattern, CBPCY);
                    pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                }
#endif
                /*---------------------------Reconstruction ------------------------------------*/
STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {
                    uint8_t *pRFrameY = 0;
                    uint8_t *pRFrameU = 0;
                    uint8_t *pRFrameV = 0;

                    if (bRaiseFrame)
                    {
                        pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
                        pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
                        pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
                    }
                    if (MBPattern != 0)
                    {
                        pRecMBData->Reset();
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        for (blk=0;blk<6; blk++)
                        {
                            if (MBPattern & (1<<VC_ENC_PATTERN_POS(blk)))
                            {
                                InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                                doubleQuant,BlockTSTypes[blk]);
                            }
                        }
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                    sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                    pRFrameY, m_uiRaisedPlaneStep[0], 
                                                    pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                    0,j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    } //(MBPattern != 0)
                    else
                    {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                        sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        pRFrameY, m_uiRaisedPlaneStep[0], 
                                                        pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    }

                    //smoothing
                    {
                        smoothInfo.pCurRData      = pRecMBData;
                        smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY       = pRFrameY;
                        smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                        smoothInfo.pRFrameU       = pRFrameU;
                        smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                        smoothInfo.pRFrameV       = pRFrameV;
                        smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                        smoothInfo.curIntra       = 0;
                        smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                        smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                        smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                        m_pSM_P_MB[smoothPattern](&smoothInfo, j);
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;

                    #ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
                    #endif
                    }

                    //deblocking
                    if (m_pSequenceHeader->IsLoopFilter())
                    {
                        uint8_t YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                        uint8_t UFlag0 = 0,UFlag1 = 0;
                        uint8_t VFlag0 = 0,VFlag1 = 0;

                        pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                        pCurMBInfo->SetVSTPattern(BlockTSTypes);

                        pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                        YFlag0 = YFlag0&deblkExceptoin;
                        UFlag0 = UFlag0&deblkExceptoin;
                        VFlag0 = VFlag0&deblkExceptoin;

                        pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                        pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                        YFlag0 = YFlag0&deblkExceptoin;
                        UFlag0 = UFlag0&deblkExceptoin;
                        VFlag0 = VFlag0&deblkExceptoin;
                        pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                        pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                        YFlag0 = YFlag0&deblkExceptoin;
                        YFlag1 = YFlag1&deblkExceptoin;
                        pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                        GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);//ver

                        pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);// ver

                        //deblocking
                        {
                            STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                            uint8_t *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};
                            m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                            deblkPattern = deblkPattern | 0x1 | ((!(uint8_t)((j + 1 - (w -1)) >> 31)<<1));
                            STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                        }
                    }
                    }
STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetInterpInfo(&sYInterpolation,  &sUInterpolation, &sVInterpolation, 0, m_uiForwardPlanePadding);
#endif
                }
                break;
            default:
                VM_ASSERT(0);
                return UMC::UMC_ERR_FAILED;
            }

            if (m_pSavedMV)
            {
                //pull back only simple/main profiles
                ScalePredict(&MVInt, j*8,i*8,MVSavedMBMin,MVSavedMBMax);
                *(pSavedMV ++) = (MVInt.x<<2) + MVQuarter.x;
                *(pSavedMV ++) = (MVInt.y<<2) + MVQuarter.y;
            }

#ifdef VC1_ENC_DEBUG_ON
            if(!bSubBlkTS)
            {
                pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
            }
            else
            {
                pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                                           pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                                           pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
            }
#endif

            err = m_pMBs->NextMB();
            if (err != UMC::UMC_OK && j < w-1)
                return err;

#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
        }

        deblkPattern = (deblkPattern | 0x4 | ( (! (uint8_t)((i + 1 - (h -1)) >> 31)<<3))) & deblkMask;
        smoothPattern = 0x6 & smoothMask;

////Row deblocking
//STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
//        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && i!=0)
//        {
//STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
//          uint8_t *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
//                                   m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
//                                   m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
//            m_pMBs->StartRow();
//            if(bSubBlkTS)
//            {
//                if(i < h-1)
//                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//                else
//                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//            }
//            else
//            {
//                if(i < h-1)
//                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//                else
//                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//            }
//STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
//        }
//STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

        err = m_pMBs->NextRow();
        if (err != UMC::UMC_OK)
            return err;

        pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        pFMBRow[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pFMBRow[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pFMBRow[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

    }

#ifdef VC1_ENC_DEBUG_ON
    if(bRaiseFrame)
    pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
                               m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
                               m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif

    return err;
}

UMC::Status VC1EncoderPictureSM::VLC_PFrame(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status  err = UMC::UMC_OK;
    uint32_t       i=0, j=0, blk = 0;

    uint32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t                      w = m_pSequenceHeader->GetNumMBInRow();

    const uint16_t* pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

    eCodingSet    LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet    ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet    CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

    const sACTablesSet* pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
                                                &(ACTablesSet[LumaCodingSetIntra]),
                                                &(ACTablesSet[LumaCodingSetIntra]),
                                                &(ACTablesSet[LumaCodingSetIntra]),
                                                &(ACTablesSet[ChromaCodingSetIntra]),
                                                &(ACTablesSet[ChromaCodingSetIntra])};

    const sACTablesSet*  pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

    const uint32_t*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][1],
                                          DCTables[m_uiDecTypeDCIntra][1]};

    sACEscInfo  ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
                              Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                              0, 0};

    const int16_t    (*pTTMBVLC)[4][6] =  0;
    const uint8_t     (* pTTBlkVLC)[6] = 0;
    const uint8_t     *pSubPattern4x4VLC=0;

    bool  bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
    bool  bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetCurrMBFirst();
#endif

    err = WritePPictureHeader(pCodedPicture);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuant<5)
    {
        pTTMBVLC            =  TTMBVLC_HighRate;
        pTTBlkVLC           =  TTBLKVLC_HighRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
    }
    else if (m_uiQuant<13)
    {
        pTTMBVLC            =  TTMBVLC_MediumRate;
        pTTBlkVLC           =  TTBLKVLC_MediumRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

    }
    else
    {
        pTTMBVLC            =  TTMBVLC_LowRate;
        pTTBlkVLC           =  TTBLKVLC_LowRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
    }

    for (i=0; i < h; i++)
    {
        for (j=0; j < w; j++)
        {
            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
            {
                m_MECurMbStat->whole = 0;
                memset(m_MECurMbStat->MVF, 0,   4*sizeof(uint16_t));
                memset(m_MECurMbStat->MVB, 0,   4*sizeof(uint16_t));
                memset(m_MECurMbStat->coeff, 0, 6*sizeof(uint16_t));
                m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

                pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
            }
#endif
            switch (pCompressedMB->GetMBType())
            {
            case VC1_ENC_P_MB_INTRA:
                {
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->MbType = UMC::ME_MbIntra;
                    uint32_t MBStart = pCodedPicture->GetCurrBit();
#endif
               err = pCompressedMB->WriteMBHeaderP_INTRA    ( pCodedPicture,
                                                              m_bRawBitplanes,
                                                              MVDiffTablesVLC[m_uiMVTab],
                                                              pCBPCYTable);
               VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                //Blocks coding
                for (blk = 0; blk<6; blk++)
                {
                    err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                    VC1_ENC_CHECK (err)
                    err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                    VC1_ENC_CHECK (err)
                }//for
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->whole = (uint16_t)(pCodedPicture->GetCurrBit()- MBStart);
#endif
                break;
                }
            case VC1_ENC_P_MB_1MV:
                {
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->MbType = UMC::ME_MbFrw;
#endif
                err = pCompressedMB->WritePMB1MV(pCodedPicture,
                                                m_bRawBitplanes,
                                                m_uiMVTab,
                                                m_uiMVRangeIndex,
                                                pCBPCYTable,
                                                bCalculateVSTransform,
                                                bMVHalf,
                                                pTTMBVLC,
                                                pTTBlkVLC,
                                                pSubPattern4x4VLC,
                                                pACTablesSetInter,
                                                &ACEscInfo);



                VC1_ENC_CHECK (err)
                break;
                }
            case VC1_ENC_P_MB_SKIP_1MV:
                {
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                err = pCompressedMB->WritePMB_SKIP(pCodedPicture, m_bRawBitplanes);
                VC1_ENC_CHECK (err)
                }
                break;
            }
#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
           m_MECurMbStat++;
#endif
        }
    }
    return err;
}

UMC::Status VC1EncoderPictureSM::PAC_PFrameMixed(UMC::MeParams* MEParams)
{
    UMC::Status                 err = UMC::UMC_OK;

    int32_t                      i=0, j=0, blk = 0;
    bool                        bRaiseFrame = (m_pRaisedPlane[0])&&(m_pRaisedPlane[1])&&(m_pRaisedPlane[2]);

    uint8_t*                      pCurMBRow[3] = {m_pPlane[0],        m_pPlane[1],        m_pPlane[2]};
    uint8_t*                      pFMBRow  [3] = {m_pForwardPlane[0], m_pForwardPlane[1], m_pForwardPlane[2]};

    int32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    int32_t                      w = m_pSequenceHeader->GetNumMBInRow();

    //forward transform quantization

    _SetIntraInterLumaFunctions;

    //inverse transform quantization
    IntraInvTransformQuantFunction IntraInvTransformQuantACFunction = (m_bUniformQuant) ? IntraInvTransformQuantUniform :
                                                                        IntraInvTransformQuantNonUniform;

    InterInvTransformQuantFunction InterInvTransformQuantACFunction = (m_bUniformQuant) ? InterInvTransformQuantUniform :
                                                                        InterInvTransformQuantNonUniform;

    CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?
                                                        GetChromaMVFast:GetChromaMV;

    InterpolateFunction         InterpolateLumaFunction  = _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

    uint8_t                       tempInterpolationBuffer[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];

    InterpolateFunctionPadding  InterpolateLumaFunctionPadding = own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

    IppVCInterpolate_8u         sYInterpolation = {0};
    IppVCInterpolate_8u         sUInterpolation = {0};
    IppVCInterpolate_8u         sVInterpolation = {0};
    IppVCInterpolate_8u         sYInterpolationBlk[4]={0};
    IppVCInterpolateBlock_8u    InterpolateBlockY;
    IppVCInterpolateBlock_8u    InterpolateBlockU;
    IppVCInterpolateBlock_8u    InterpolateBlockV;
    IppVCInterpolateBlock_8u    InterpolateBlockBlk[4];

    eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];
    int8_t                       dACPrediction   = 1;

    mfxSize                    blkSize     = {8,8};
    mfxSize                    blkSizeLuma = {16,16};

    uint8_t                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
    uint8_t                       DCQuant         =  DCQuantValues[m_uiQuant];
    int16_t                      defPredictor    =  0;
    int16_t*                     pSavedMV = m_pSavedMV;

    sCoordinate                 MVPredMBMin = {-60,-60};
    sCoordinate                 MVPredMBMax = {(int16_t)w*16*4-4, (int16_t)h*16*4-4};

    sCoordinate                 MVPredMBMinB = {-28,-28};
    sCoordinate                 MVPredMBMaxB = {(int16_t)w*16*4-4, (int16_t)h*16*4-4};

    sCoordinate                 MVSavedMBMin = {-8,-8};
    sCoordinate                 MVSavedMBMax = {(int16_t)w*8, (int16_t)h*8};

    bool                        bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
                                           m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

    bool                        bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

    bool                        bSubBlkTS           = m_pSequenceHeader->IsVSTransform() && (!(m_bVSTransform &&m_uiTransformType==VC1_ENC_8x8_TRANSFORM));
    fGetExternalEdge            pGetExternalEdge    = GetExternalEdge_SM[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS
    fGetInternalEdge            pGetInternalEdge    = GetInternalEdge_SM[m_uiMVMode==VC1_ENC_MIXED_QUARTER_BICUBIC][bSubBlkTS]; //4 MV, VTS

    uint8_t deblkPattern = 0;//4 bits: right 1 bit - 0 - left/1 - not left,
                           //left 2 bits: 00 - top row, 01-middle row, 11 - bottom row
                           //middle 1 bit - 1 - right/0 - not right
    uint8_t deblkExceptoin = 0xFF; //if fist block intra equal to 0

    uint8_t deblkMask = (m_pSequenceHeader->IsLoopFilter()) ? 0xFC : 0;

    uint8_t smoothMask = (m_pSequenceHeader->IsOverlap() && (m_uiQuant >= 9)) ? 0xFF : 0;
    uint8_t smoothPattern = 0x4 & smoothMask; //3 bits: 0/1 - right/not right,
                                            //0/1 - top/not top, 0/1 - left/not left

    SmoothInfo_P_SM smoothInfo = {0};

    IppStatus                  ippSts  = ippStsNoErr;

#ifdef VC1_ENC_DEBUG_ON
    bool    bIsBilinearInterpolation = false;
    pDebug->SetInterpolType(bIsBilinearInterpolation);
    pDebug->SetRounControl(m_uiRoundControl);
    pDebug->SetDeblkFlag(m_pSequenceHeader->IsLoopFilter());
    pDebug->SetVTSFlag(bSubBlkTS);
#endif

    if (m_pSavedMV)
    {
        memset(m_pSavedMV,0,w*h*2*sizeof(int16_t));
    }


   err = m_pMBs->Reset();
   if (err != UMC::UMC_OK)
        return err;

    SetInterpolationParams(&sYInterpolation,&sUInterpolation,&sVInterpolation,
                           tempInterpolationBuffer,true);
    SetInterpolationParams4MV(sYInterpolationBlk,&sUInterpolation,&sVInterpolation,
                           tempInterpolationBuffer,true);
    SetInterpolationParams(&InterpolateBlockY,&InterpolateBlockU,&InterpolateBlockV, tempInterpolationBuffer,
                            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
                            m_pForwardPlane, m_uiForwardPlaneStep, false);
    SetInterpolationParams4MV (InterpolateBlockBlk, tempInterpolationBuffer,
                            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
                            m_pForwardPlane, m_uiForwardPlaneStep, false);






    if(MEParams->pSrc->MBs[0].MbType == UMC::ME_MbIntra ||
        (MEParams->pSrc->MBs[0].MbType == UMC::ME_MbMixed
            && MEParams->pSrc->MBs[0].BlockType[0] == UMC::ME_MbIntra))
        deblkExceptoin = 0;

#ifdef VC1_ENC_CHECK_MV
    err = CheckMEMV_P_MIXED(MEParams, bMVHalf);
    assert(err == UMC::UMC_OK);
#endif

    for (i=0; i < h; i++)
    {
        uint8_t *pRFrameY = 0;
        uint8_t *pRFrameU = 0;
        uint8_t *pRFrameV = 0;

        if (bRaiseFrame)
        {
            pRFrameY = m_pRaisedPlane[0]+VC1_ENC_LUMA_SIZE*(i*m_uiRaisedPlaneStep[0]);
            pRFrameU = m_pRaisedPlane[1]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[1]);
            pRFrameV = m_pRaisedPlane[2]+VC1_ENC_CHROMA_SIZE*(i*m_uiRaisedPlaneStep[2]);
        }
        for (j=0; j < w; j++)
        {
            //bool                bIntra      = false;
            int32_t              xLuma        =  VC1_ENC_LUMA_SIZE*j;
            int32_t              xChroma      =  VC1_ENC_CHROMA_SIZE*j;

            //int32_t              posY        =  VC1_ENC_LUMA_SIZE*i;
            uint8_t               MBPattern   = 0;
            uint8_t               CBPCY       = 0;

            VC1EncoderMBInfo  * pCurMBInfo  = m_pMBs->GetCurrMBInfo();
            VC1EncoderMBData  * pCurMBData  = m_pMBs->GetCurrMBData();
            VC1EncoderMBData  * pRecMBData  = m_pMBs->GetRecCurrMBData();

            VC1EncoderMBInfo* left          = m_pMBs->GetLeftMBInfo();
            VC1EncoderMBInfo* topLeft       = m_pMBs->GetTopLeftMBInfo();
            VC1EncoderMBInfo* top           = m_pMBs->GetTopMBInfo();
            VC1EncoderMBInfo* topRight      = m_pMBs->GetTopRightMBInfo();

            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);
            uint8_t          intraPattern       = 0; //from UMC_ME: position 1<<VC_ENC_PATTERN_POS(blk) (if not skiped)

            eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                m_uiTransformType, m_uiTransformType,
                                                m_uiTransformType, m_uiTransformType};



            eMBType MBType;

            switch (MEParams->pSrc->MBs[j + i*w].MbType)
            {
                case UMC::ME_MbIntra:
                    MBType = VC1_ENC_P_MB_INTRA;
                    break;
                case UMC::ME_MbFrw:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_4MV:VC1_ENC_P_MB_1MV;
                    break;
                case UMC::ME_MbMixed:
                    MBType = VC1_ENC_P_MB_4MV;
                    intraPattern = (uint8_t)(((UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[0])<<VC_ENC_PATTERN_POS(0)|
                                    (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[1])<<VC_ENC_PATTERN_POS(1)|
                                    (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[2])<<VC_ENC_PATTERN_POS(2)|
                                    (UMC::ME_MbIntra==MEParams->pSrc->MBs[j + i*w].BlockType[3])<<VC_ENC_PATTERN_POS(3)));
                    break;
                case UMC::ME_MbFrwSkipped:
                    MBType = (MEParams->pSrc->MBs[j + i*w].MbPart == UMC::ME_Mb8x8)? VC1_ENC_P_MB_SKIP_4MV:VC1_ENC_P_MB_SKIP_1MV;
                    break;
                default:
                    assert(0);
                    return UMC::UMC_ERR_FAILED;
            }


            pCompressedMB->Init(MBType);

            switch  (MBType)
            {
            case VC1_ENC_P_MB_INTRA:
            {
                NeighbouringMBsData MBs;
                NeighbouringMBsIntraPattern     MBsIntraPattern;

                MBs.LeftMB    = m_pMBs->GetLeftMBData();
                MBs.TopMB     = m_pMBs->GetTopMBData();
                MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();

                MBsIntraPattern.LeftMB      = (left)?       left->GetIntraPattern():0;
                MBsIntraPattern.TopMB       = (top)?        top->GetIntraPattern():0;
                MBsIntraPattern.TopLeftMB   = (topLeft)?    topLeft->GetIntraPattern():0;



                pCurMBInfo->Init(true);

                pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                //only intra blocks:
                for (blk = 0; blk<6; blk++)
                {

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                        IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                            DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                }

STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                dACPrediction = DCACPredictionFrame4MVIntraMB( pCurMBData,&MBs,&MBsIntraPattern,pRecMBData, defPredictor,direction);
STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);


                for (blk=0; blk<6; blk++)
                {
                    pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                                    pRecMBData->m_uiBlockStep[blk],
                                                    VC1_Inter_8x8_Scan,
                                                    blk);
                }

                MBPattern = pCompressedMB->GetMBPattern();
                CBPCY = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetACPrediction(dACPrediction);
                pCompressedMB->SetMBCBPCY(CBPCY);
                pCurMBInfo->SetEdgesIntra(i==0, j==0);

STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {
                    for (blk=0;blk<6; blk++)
                    {
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                        pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                        DCQuant, doubleQuant);
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        _own_Add8x8C_16s(128, pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],blkSize,0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    }

                    pRecMBData->PasteMBProg(pRFrameY,m_uiRaisedPlaneStep[0],pRFrameU,pRFrameV,m_uiRaisedPlaneStep[1],j);

                    //smoothing
                    {
                        smoothInfo.pCurRData      = pRecMBData;
                        smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY       = pRFrameY;
                        smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                        smoothInfo.pRFrameU       = pRFrameU;
                        smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                        smoothInfo.pRFrameV       = pRFrameV;
                        smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                        smoothInfo.curIntra       = 0x3F;
                        smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                        smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                        smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                        m_pSM_P_MB[smoothPattern](&smoothInfo, j);

                    #ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
                    #endif
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
                    }

                    //deblocking
                    {
                        STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                        uint8_t *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                        m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                        deblkPattern = deblkPattern | 0x1 | ((!(uint8_t)((j + 1 - (w -1)) >> 31)<<1));
                        STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                    }

                }
STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                if (m_pSavedMV)
                {
                    *(pSavedMV ++) = 0;
                    *(pSavedMV ++) = 0;
                }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(VC1_ENC_P_MB_INTRA);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
            }
            break;
            case VC1_ENC_P_MB_1MV:
            case VC1_ENC_P_MB_SKIP_1MV:
            {

                sCoordinate    MVIntChroma     = {0,0};
                sCoordinate    MVQuarterChroma = {0,0};
                sCoordinate    MVChroma        = {0,0};
                sCoordinate    MVInt           = {0,0};
                sCoordinate    MVQuarter       = {0,0};
                sCoordinate    MV              = { MEParams->pSrc->MBs[j + i*w].MV[0]->x,MEParams->pSrc->MBs[j + i*w].MV[0]->y};

                sCoordinate    *mvC=0, *mvB=0, *mvA=0;
                sCoordinate     mv1,    mv2,    mv3;
                sCoordinate     mvPred;
                sCoordinate     mvDiff;
                uint8_t           hybrid = 0;

                pCurMBInfo->Init(false);

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                /*MV prediction */
                GetMVPredictionP(true)
                PredictMV(mvA,mvB,mvC, &mvPred);
                ScalePredict(&mvPred, j*16*4,i*16*4,MVPredMBMin,MVPredMBMax);
                hybrid = HybridPrediction(&mvPred,&MV,mvA,mvC,32);
                pCompressedMB->SetHybrid(hybrid);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                if (VC1_ENC_P_MB_SKIP_1MV == MBType)
                {
                    //correct ME results
                    VM_ASSERT(MV.x==mvPred.x && MV.y==mvPred.y);
                    MV.x=mvPred.x;
                    MV.y=mvPred.y;
                }

                pCurMBInfo->SetMV(MV);
                GetIntQuarterMV(MV,&MVInt, &MVQuarter);


                if (MBType != VC1_ENC_P_MB_SKIP_1MV || bRaiseFrame)
                {
                    CalculateChroma(MV,&MVChroma);
                    pCurMBInfo->SetMVChroma(MVChroma);
                    GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                    if (m_uiForwardPlanePadding)
                    {

STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    SetInterpolationParamsLuma(&sYInterpolation, pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt, &MVQuarter, j);
                    ippSts = InterpolateLumaFunction(&sYInterpolation);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                       &MVIntChroma, &MVQuarterChroma, j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                    }
                    else
                    {
                         bool bOpposite = 0;
                         STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                         IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                         SetInterpolationParamsLuma(&InterpolateBlockY, j, i , &MVInt, &MVQuarter);
                         ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockY,0,0,bOpposite,false,m_uiRoundControl,0);
                         VC1_ENC_IPP_CHECK(ippSts);

                         InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                             bOpposite,false,m_uiRoundControl,0,
                             j, i ,  &MVIntChroma, &MVQuarterChroma);

                         IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                         STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);   
                    
                    }
                } //interpolation
                if (VC1_ENC_P_MB_SKIP_1MV != MBType)
                {
                    if (bCalculateVSTransform)
                    {
#ifndef OWN_VS_TRANSFORM
                        GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                        VC1EncMD_P pIn;
                        pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                        pIn.srcYStep = m_uiPlaneStep[0];
                        pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                        pIn.srcUStep = m_uiPlaneStep[1];
                        pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                        pIn.srcVStep = m_uiPlaneStep[2];

                        pIn.pYRef    = sYInterpolation.pDst; 
                        pIn.refYStep = sYInterpolation.dstStep;      
                        pIn.pURef    = sUInterpolation.pDst; 
                        pIn.refUStep = sUInterpolation.dstStep;
                        pIn.pVRef    = sVInterpolation.pDst; 
                        pIn.refVStep = sVInterpolation.dstStep;

                        pIn.quant         = doubleQuant;
                        pIn.bUniform      = m_bUniformQuant;
                        pIn.intraPattern  = 0;
                        pIn.DecTypeAC1    = m_uiDecTypeAC1;
                        pIn.pScanMatrix   = ZagTables;
                        pIn.bField        = 0;
                        pIn.CBPTab        = m_uiCBPTab;

                        GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                    }
                    pCompressedMB->SetTSType(BlockTSTypes);

                    mvDiff.x = MV.x - mvPred.x;
                    mvDiff.y = MV.y - mvPred.y;
                    pCompressedMB->SetdMV(mvDiff);



IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                            sYInterpolation.pDst, sYInterpolation.dstStep,
                            sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                            j,0);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                    for (blk = 0; blk<6; blk++)
                    {
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                        InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                    BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                        pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                                    pCurMBData->m_uiBlockStep[blk],
                                                    ZagTables[BlockTSTypes[blk]],
                                                    blk);
                    }

                    MBPattern = pCompressedMB->GetMBPattern();
                    CBPCY = MBPattern;
                    pCurMBInfo->SetMBPattern(MBPattern);
                    pCompressedMB->SetMBCBPCY(CBPCY);
                    if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                    {
                        pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_1MV);
                        MBType = VC1_ENC_P_MB_SKIP_1MV;
                    }
                }// not skipped



STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {
                    if (MBPattern!=0)
                    {
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        for (blk = 0; blk <6 ; blk++)
                        {
                            InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                            pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                            doubleQuant,BlockTSTypes[blk]);
                        }
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                        pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                    sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                    pRFrameY, m_uiRaisedPlaneStep[0], 
                                                    pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                    0,j);

                    }
                    else
                    {
                        pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                        sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        pRFrameY, m_uiRaisedPlaneStep[0], 
                                                        pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);
                    }

                    //smoothing
                    {
                        smoothInfo.pCurRData      = pRecMBData;
                        smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY       = pRFrameY;
                        smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                        smoothInfo.pRFrameU       = pRFrameU;
                        smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                        smoothInfo.pRFrameV       = pRFrameV;
                        smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                        smoothInfo.curIntra       = intraPattern;
                        smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                        smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                        smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                        m_pSM_P_MB[smoothPattern](&smoothInfo, j);

                    #ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
                    #endif
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
                    }

                    if (m_pSequenceHeader->IsLoopFilter())
                    {
                        uint8_t YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                        uint8_t UFlag0 = 0,UFlag1 = 0;
                        uint8_t VFlag0 = 0,VFlag1 = 0;

                        pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                        pCurMBInfo->SetVSTPattern(BlockTSTypes);

                        pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                        //exception #1
                        YFlag0 = YFlag0 & deblkExceptoin;
                        UFlag0 = UFlag0 & deblkExceptoin;
                        VFlag0 = VFlag0 & deblkExceptoin;

                        pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                        pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                        //exception #1
                        YFlag0 = YFlag0 & deblkExceptoin;
                        UFlag0 = UFlag0 & deblkExceptoin;
                        VFlag0 = VFlag0 & deblkExceptoin;

                        pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                        pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                        //exception #1
                        YFlag0 = YFlag0 & deblkExceptoin;
                        YFlag1 = YFlag1 & deblkExceptoin;

                        pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                        GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);//ver
                        //exception #1
                        YFlag0 = YFlag0 & deblkExceptoin;
                        YFlag1 = YFlag1 & deblkExceptoin;

                        pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);// ver

                        //deblocking
                        {
                            STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                            uint8_t *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                            m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                            deblkPattern = deblkPattern | 0x1 | ((!(uint8_t)((j + 1 - (w -1)) >> 31)<<1));
                            STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                        }

                    }
                }
STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                if (m_pSavedMV)
                {
                    //pull back only simple/main profiles
                    ScalePredict(&MVInt, j*8,i*8,MVSavedMBMin,MVSavedMBMax);
                    *(pSavedMV ++) = (MVInt.x<<2) + MVQuarter.x;
                    *(pSavedMV ++) = (MVInt.y<<2) + MVQuarter.y;
                 }
#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(MBType);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetMVInfo(&MV, mvPred.x, mvPred.y, 0);
                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                if (MBType != VC1_ENC_P_MB_SKIP_1MV)
                {
                    pDebug->SetCPB(MBPattern, CBPCY);
                    pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                }
                else
                {
                    pDebug->SetMBAsSkip();
                }

                pDebug->SetInterpInfo(&sYInterpolation,  &sUInterpolation, &sVInterpolation, 0, m_uiForwardPlanePadding);
#endif

            }
            break;

            case VC1_ENC_P_MB_4MV:
            case VC1_ENC_P_MB_SKIP_4MV:
            {

                sCoordinate    MVInt[4]           = {0};
                sCoordinate    MVQuarter[4]       = {0};
                sCoordinate    MV[4]              = {{MEParams->pSrc->MBs[j + i*w].MV[0][0].x,MEParams->pSrc->MBs[j + i*w].MV[0][0].y},
                                                     {MEParams->pSrc->MBs[j + i*w].MV[0][1].x,MEParams->pSrc->MBs[j + i*w].MV[0][1].y},
                                                     {MEParams->pSrc->MBs[j + i*w].MV[0][2].x,MEParams->pSrc->MBs[j + i*w].MV[0][2].y},
                                                     {MEParams->pSrc->MBs[j + i*w].MV[0][3].x,MEParams->pSrc->MBs[j + i*w].MV[0][3].y}};
                //sCoordinate    MVPred[4]       = {0};
                sCoordinate    MVLuma          = {0,0};
                sCoordinate    MVChroma        = {0,0};
                sCoordinate    MVIntChroma     = {0,0};
                sCoordinate    MVQuarterChroma = {0,0};

                sCoordinate    *mvC=0, *mvB=0, *mvA=0;
                sCoordinate     mv1,    mv2,    mv3;
                sCoordinate     mvPred[4]={0};
                sCoordinate     mvDiff;
                uint8_t           hybrid = 0;
                //bool            bInterpolation  = false;
                bool            bNullMV         = true;
                NeighbouringMBsData             MBs;
                NeighbouringMBsIntraPattern     MBsIntraPattern;

                MBs.LeftMB    = m_pMBs->GetLeftMBData();
                MBs.TopMB     = m_pMBs->GetTopMBData();
                MBs.TopLeftMB = m_pMBs->GetTopLeftMBData();

                MBsIntraPattern.LeftMB      = (left)?       left->GetIntraPattern():0;
                MBsIntraPattern.TopMB       = (top)?        top->GetIntraPattern():0;
                MBsIntraPattern.TopLeftMB   = (topLeft)?    topLeft->GetIntraPattern():0;

                pCurMBInfo->Init(false);

#ifdef VC1_ENC_DEBUG_ON
                if (VC1_ENC_P_MB_SKIP_4MV == MBType  )
                {
                    pDebug->SetMBType(VC1_ENC_P_MB_SKIP_4MV);
                    pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                    pDebug->SetMBAsSkip();
                }
                else
                {
                    pDebug->SetMBType(VC1_ENC_P_MB_4MV);
                    pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                }
#endif
                // --- MV Prediction ---- //
                if (VC1_ENC_P_MB_SKIP_4MV != MBType )
                {
                    if (bCalculateVSTransform)
                    {
#ifndef OWN_VS_TRANSFORM
                        GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                        VC1EncMD_P pIn;
                        pIn.pYSrc    = pCurMBRow[0]+xLuma; 
                        pIn.srcYStep = m_uiPlaneStep[0];
                        pIn.pUSrc    = pCurMBRow[1]+xChroma; 
                        pIn.srcUStep = m_uiPlaneStep[1];
                        pIn.pVSrc    = pCurMBRow[2]+xChroma;  
                        pIn.srcVStep = m_uiPlaneStep[2];

                        pIn.pYRef    = sYInterpolation.pDst; 
                        pIn.refYStep = sYInterpolation.dstStep;      
                        pIn.pURef    = sUInterpolation.pDst; 
                        pIn.refUStep = sUInterpolation.dstStep;
                        pIn.pVRef    = sVInterpolation.pDst; 
                        pIn.refVStep = sVInterpolation.dstStep;

                        pIn.quant         = doubleQuant;
                        pIn.bUniform      = m_bUniformQuant;
                        pIn.intraPattern  = intraPattern;
                        pIn.DecTypeAC1    = m_uiDecTypeAC1;
                        pIn.pScanMatrix   = ZagTables_Adv;
                        pIn.bField        = 0;
                        pIn.CBPTab        = m_uiCBPTab;

                        GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif
                    }

                    if (0 != intraPattern )
                        memset (tempInterpolationBuffer,128,VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS*sizeof(uint8_t));

                    for (blk = 0; blk<4; blk++)
                    {
                        if ((intraPattern&(1<<VC_ENC_PATTERN_POS(blk))) == 0)
                        {
                            uint32_t blkPosX = (blk&0x1)<<3;
                            uint32_t blkPosY = (blk/2)<<3;

                            pCurMBInfo->SetMV(MV[blk],blk, true);

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                            GetMVPredictionPBlk(blk);
                            PredictMV(mvA,mvB,mvC, &mvPred[blk]);

                            ScalePredict(&mvPred[blk], ((j<<4) + blkPosX)<<2,((i<<4) + blkPosY)<<2,MVPredMBMinB,MVPredMBMaxB);
                            hybrid = HybridPrediction(&mvPred[blk],&MV[blk],mvA,mvC,32);
                            pCompressedMB->SetHybrid(hybrid,blk);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                            mvDiff.x = MV[blk].x - mvPred[blk].x;
                            mvDiff.y = MV[blk].y - mvPred[blk].y;

                            bNullMV = ((mvDiff.x!=0)||(mvDiff.y!=0))? false: bNullMV;
                            pCompressedMB->SetBlockdMV(mvDiff,blk);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMVInfo(&MV[blk], mvPred[blk].x,mvPred[blk].y, 0, blk);
                            pDebug->SetMVDiff(mvDiff.x, mvDiff.y, 0, blk);
#endif
                        }
                        else
                        {
                            pCurMBInfo->SetIntraBlock(blk);
                            pCompressedMB->SetIntraBlock(blk);
                            BlockTSTypes[blk]=VC1_ENC_8x8_TRANSFORM;

    #ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetBlockAsIntra(blk);
    #endif
                        }
                    } // for

                    pCompressedMB->SetTSType(BlockTSTypes);

                    if (!pCurMBInfo->GetLumaMV(&MVLuma))
                    {   //chroma intra type
                        intraPattern = intraPattern | (1<<VC_ENC_PATTERN_POS(4))|(1<<VC_ENC_PATTERN_POS(5));
                        pCurMBInfo->SetIntraBlock(4);
                        pCurMBInfo->SetIntraBlock(5);
                        pCompressedMB->SetIntraBlock(4);
                        pCompressedMB->SetIntraBlock(5);
                        BlockTSTypes[4]=VC1_ENC_8x8_TRANSFORM;
                        BlockTSTypes[5]=VC1_ENC_8x8_TRANSFORM;
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetBlockAsIntra(4);
                        pDebug->SetBlockAsIntra(5);
#endif
                    }
                }
                else  // prediction for skip MB
                {
                    for (blk = 0; blk<4; blk++)
                    {
                         uint32_t blkPosX = (blk&0x1)<<3;
                         uint32_t blkPosY = (blk/2)<<3;

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                         GetMVPredictionPBlk(blk);
                         PredictMV(mvA,mvB,mvC, &mvPred[blk]);
                         ScalePredict(&mvPred[blk], ((j<<4) + blkPosX)<<2,((i<<4) + blkPosY)<<2,MVPredMBMinB,MVPredMBMaxB);
                         hybrid = HybridPrediction(&mvPred[blk],&MV[blk],mvA,mvC,32);
                         pCompressedMB->SetHybrid(hybrid,blk);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                        //check MV from ME
                        assert( mvPred[blk].x == MV[blk].x && mvPred[blk].y == MV[blk].y);
                        MV[blk].x = mvPred[blk].x;
                        MV[blk].y = mvPred[blk].y;
                        pCurMBInfo->SetMV(MV[blk],blk, true);
                        GetIntQuarterMV(MV[blk],&MVInt[blk], &MVQuarter[blk]);
#ifdef VC1_ENC_DEBUG_ON
                            pDebug->SetMVInfo(&MV[blk], mvPred[blk].x,mvPred[blk].y, 0, blk);
#endif

                    }
                    pCurMBInfo->GetLumaMV(&MVLuma);
                    intraPattern = 0;
                }

                // --- Interpolation --- //
                if (VC1_ENC_P_MB_SKIP_4MV != MBType   || bRaiseFrame)
                {
                   for (blk = 0; blk<4; blk++)
                   {
                        uint32_t blkPosX = (blk&0x1)<<3;
                        uint32_t blkPosY = (blk/2)<<3;


                       // Luma blocks
                       if ((intraPattern&(1<<VC_ENC_PATTERN_POS(blk))) == 0)
                       {
                           GetIntQuarterMV(MV[blk],&MVInt[blk], &MVQuarter[blk]);
                           if (m_uiForwardPlanePadding)
                           {
                               SetInterpolationParamsLumaBlk(&sYInterpolationBlk[blk], pFMBRow[0], m_uiForwardPlaneStep[0], &MVInt[blk], &MVQuarter[blk], j,blk);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                               ippSts = InterpolateLumaFunction(&sYInterpolationBlk[blk]);
                               VC1_ENC_IPP_CHECK(ippSts);
 IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                           }
                           else
                           {                           
                                bool bOpposite = 0;
 STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                                SetInterpolationParamsLumaBlk(&InterpolateBlockBlk[blk], j, i, &MVInt[blk], &MVQuarter[blk], blk);                                
IPP_STAT_START_TIME(m_IppStat->IppStartTime);                                
                                ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockBlk[blk],0,0,bOpposite,false,m_uiRoundControl,0);
                                if (ippSts != ippStsNoErr)
                                    return UMC::UMC_ERR_OPEN_FAILED;

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                              
                           
                           }
#ifdef VC1_ENC_DEBUG_ON
                            //interpolation
                            pDebug->SetInterpInfoLuma(sYInterpolationBlk[blk].pDst, sYInterpolationBlk[blk].dstStep, blk, 0);
#endif
                       }//if
                   }//for blk

                   if ((intraPattern&(1<<VC_ENC_PATTERN_POS(4))) == 0)
                   {
                       CalculateChroma(MVLuma,&MVChroma);
                       pCurMBInfo->SetMVChroma(MVChroma);
                       GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                       if (m_uiForwardPlanePadding)
                       {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                        InterpolateChroma (&sUInterpolation, &sVInterpolation, pFMBRow[1], pFMBRow[2],m_uiForwardPlaneStep[1], 
                                            &MVIntChroma, &MVQuarterChroma, j);                      
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                       }
                       else
                       {
                       
                         bool bOpposite = 0;
 STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
 IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                        InterpolateChromaPad   (&InterpolateBlockU, &InterpolateBlockV, 
                                                bOpposite,false,m_uiRoundControl,0,
                                                j, i ,  &MVIntChroma, &MVQuarterChroma);
                        
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                       
                       }

#ifdef VC1_ENC_DEBUG_ON
                       pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 4);
                       pDebug->SetMVInfo(&MVChroma,  0, 0, 0, 5);
                       pDebug->SetInterpInfoChroma(sUInterpolation.pDst, sUInterpolation.dstStep,
                           sVInterpolation.pDst, sVInterpolation.dstStep, 0);
#endif
                   } // chroma
                }//if interpolation
                //Chroma blocks

                // --- Compressing --- //
                if (VC1_ENC_P_MB_SKIP_4MV != MBType)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                            sYInterpolation.pDst, sYInterpolation.dstStep,
                            sUInterpolation.pDst,sVInterpolation.pDst, sUInterpolation.dstStep,
                            j,0);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    for (blk = 0; blk<6; blk++)
                    {
                        if (!pCurMBInfo->isIntra(blk))
                        {
                            InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                            BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
                        }
                        else
                        {
                            IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                            DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
                        }
                    }
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);


                    dACPrediction = DCACPredictionFrame4MVBlockMixed   (pCurMBData,
                                                                            pCurMBInfo->GetIntraPattern(),
                                                                            &MBs,
                                                                            &MBsIntraPattern,
                                                                            pRecMBData,
                                                                            defPredictor,
                                                                            direction);

                    for (blk = 0; blk<6; blk++)
                    {
                            pCompressedMB->SaveResidual(pRecMBData->m_pBlock[blk],
                                                        pRecMBData->m_uiBlockStep[blk],
                                                        ZagTables[BlockTSTypes[blk]],
                                                        blk);
                    }
                    MBPattern = pCompressedMB->GetMBPattern();
                    CBPCY = MBPattern;
                    pCurMBInfo->SetMBPattern(MBPattern);
                    pCompressedMB->SetMBCBPCY(CBPCY);
                    pCompressedMB->SetACPrediction(dACPrediction);
                    if (MBPattern==0 && intraPattern==0 && bNullMV)
                    {
                        pCompressedMB->ChangeType(VC1_ENC_P_MB_SKIP_4MV);
                        MBType = VC1_ENC_P_MB_SKIP_4MV;
#ifdef VC1_ENC_DEBUG_ON
                        pDebug->SetMBType(VC1_ENC_P_MB_SKIP_4MV);
                        pDebug->SetMBAsSkip();
#endif
                    }
#ifdef VC1_ENC_DEBUG_ON
                    if (MBType != VC1_ENC_P_MB_SKIP_4MV)
                    {
                        pDebug->SetCPB(MBPattern, CBPCY);
                        pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                    }
#endif
                } // end compressing

#ifdef VC1_ENC_DEBUG_ON
                if (MBType != VC1_ENC_P_MB_SKIP_4MV)
                {
                    pDebug->SetCPB(MBPattern, CBPCY);
                    pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                }
#endif

STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
                if (bRaiseFrame)
                {
                    if (MBPattern!=0 || intraPattern!=0)
                     {
STATISTICS_START_TIME(m_TStat->InvQT_StartTime);
                        for (blk = 0; blk <6 ; blk++)
                        {
                            if (!pCurMBInfo->isIntra(blk))
                            {
                                 InterInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                                    doubleQuant,BlockTSTypes[blk]);
                            }
                            else
                            {
                                IntraInvTransformQuantACFunction(pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                    pRecMBData->m_pBlock[blk],pRecMBData->m_uiBlockStep[blk],
                                                    DCQuant,doubleQuant);
                            }
                        } // for
STATISTICS_END_TIME(m_TStat->InvQT_StartTime, m_TStat->InvQT_EndTime, m_TStat->InvQT_TotalTime);

                        pRecMBData->PasteSumMBProg (sYInterpolation.pDst, sYInterpolation.dstStep, 
                                                    sUInterpolation.pDst, sVInterpolation.pDst,sUInterpolation.dstStep, 
                                                    pRFrameY, m_uiRaisedPlaneStep[0], 
                                                    pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],                             
                                                    0,j);

                    }
                    else
                    {
                        pRecMBData->PasteSumSkipMBProg (sYInterpolation.pDst,sYInterpolation.dstStep,
                                                        sUInterpolation.pDst, sVInterpolation.pDst, sUInterpolation.dstStep,
                                                        pRFrameY, m_uiRaisedPlaneStep[0], 
                                                        pRFrameU, pRFrameV, m_uiRaisedPlaneStep[1],0,j);
                    }

                    //smoothing
                    {
                        smoothInfo.pCurRData      = pRecMBData;
                        smoothInfo.pLeftTopRData  = m_pMBs->GetRecTopLeftMBData();
                        smoothInfo.pLeftRData     = m_pMBs->GetRecLeftMBData();
                        smoothInfo.pTopRData      = m_pMBs->GetRecTopMBData();
                        smoothInfo.pRFrameY       = pRFrameY;
                        smoothInfo.uiRDataStepY   = m_uiRaisedPlaneStep[0];
                        smoothInfo.pRFrameU       = pRFrameU;
                        smoothInfo.uiRDataStepU   = m_uiRaisedPlaneStep[1];
                        smoothInfo.pRFrameV       = pRFrameV;
                        smoothInfo.uiRDataStepV   = m_uiRaisedPlaneStep[2];
                        smoothInfo.curIntra       = intraPattern;
                        smoothInfo.leftIntra      = left ? left->GetIntraPattern():0;
                        smoothInfo.leftTopIntra   = topLeft ? topLeft->GetIntraPattern() : 0;
                        smoothInfo.topIntra       = top ? top->GetIntraPattern() : 0;

                        m_pSM_P_MB[smoothPattern](&smoothInfo, j);

                    #ifdef VC1_ENC_DEBUG_ON
                        pDebug->PrintSmoothingDataFrame(j, i, smoothInfo.pCurRData, smoothInfo.pLeftRData, smoothInfo.pLeftTopRData, smoothInfo.pTopRData);
                    #endif
                        smoothPattern = (smoothPattern | 0x1) & ((j == w - 2) ? 0xFB : 0xFF) & smoothMask;
                    }

                    if (m_pSequenceHeader->IsLoopFilter())
                    {
                        uint8_t YFlag0 = 0,YFlag1 = 0, YFlag2 = 0, YFlag3 = 0;
                        uint8_t UFlag0 = 0,UFlag1 = 0;
                        uint8_t VFlag0 = 0,VFlag1 = 0;

                        pCurMBInfo->SetBlocksPattern (pCompressedMB->GetBlocksPattern());
                        pCurMBInfo->SetVSTPattern(BlockTSTypes);

                        pGetExternalEdge (top,  pCurMBInfo, false, YFlag0,UFlag0,VFlag0);
                        pCurMBInfo->SetExternalEdgeHor(YFlag0,UFlag0,VFlag0);

                        pGetExternalEdge(left, pCurMBInfo, true,  YFlag0,UFlag0,VFlag0);
                        pCurMBInfo->SetExternalEdgeVer(YFlag0,UFlag0,VFlag0);

                        pGetInternalEdge (pCurMBInfo, YFlag0,YFlag1);
                        pCurMBInfo->SetInternalEdge(YFlag0,YFlag1);

                        GetInternalBlockEdge(pCurMBInfo, YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);//ver

                        pCurMBInfo->SetInternalBlockEdge(YFlag0,YFlag1, UFlag0,VFlag0, //hor
                                                         YFlag2,YFlag3, UFlag1,VFlag1);// ver

                        //deblocking
                        {
                            STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
                            uint8_t *pDblkPlanes[3] = {pRFrameY, pRFrameU, pRFrameV};

                            m_pDeblk_P_MB[bSubBlkTS][deblkPattern](pDblkPlanes, m_uiRaisedPlaneStep, m_uiQuant, pCurMBInfo, top, topLeft, left, j);
                            deblkPattern = deblkPattern | 0x1 | ((!(uint8_t)((j + 1 - (w -1)) >> 31)<<1));
                            STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
                        }

                    }
                }
STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime);

                if (m_pSavedMV)
                {
                    sCoordinate MVIntLuma    ={0};
                    sCoordinate MVQuarterLuma={0};
                    //pull back only simple/main profiles
                    GetIntQuarterMV(MVLuma,&MVIntLuma,&MVQuarterLuma);
                    ScalePredict(&MVIntLuma, j*8,i*8,MVSavedMBMin,MVSavedMBMax);
                    *(pSavedMV ++) = (MVIntLuma.x<<2) + MVQuarterLuma.x;
                    *(pSavedMV ++) = (MVIntLuma.y<<2) + MVQuarterLuma.y;

                }
            }
            break;

            }
#ifdef VC1_ENC_DEBUG_ON
            if(!bSubBlkTS)
            {
                pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(), 15, 15);
                pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(), 15, 15);
                pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(),15);
                pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), 15);
                pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), 15);
                pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), 15);
            }
            else
            {
                pDebug->SetDblkHorEdgeLuma(pCurMBInfo->GetLumaExHorEdge(), pCurMBInfo->GetLumaInHorEdge(),
                                           pCurMBInfo->GetLumaAdUppEdge(), pCurMBInfo->GetLumaAdBotEdge() );
                pDebug->SetDblkVerEdgeLuma(pCurMBInfo->GetLumaExVerEdge(), pCurMBInfo->GetLumaInVerEdge(),
                                           pCurMBInfo->GetLumaAdLefEdge(), pCurMBInfo->GetLumaAdRigEdge());
                pDebug->SetDblkHorEdgeU(pCurMBInfo->GetUExHorEdge(), pCurMBInfo->GetUAdHorEdge());
                pDebug->SetDblkHorEdgeV(pCurMBInfo->GetVExHorEdge(), pCurMBInfo->GetVAdHorEdge());
                pDebug->SetDblkVerEdgeU(pCurMBInfo->GetUExVerEdge(), pCurMBInfo->GetUAdVerEdge());
                pDebug->SetDblkVerEdgeV(pCurMBInfo->GetVExVerEdge(), pCurMBInfo->GetVAdVerEdge());
            }
#endif
            err = m_pMBs->NextMB();
            if (err != UMC::UMC_OK && j < w-1)
                return err;

#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
        }

        deblkPattern = (deblkPattern | 0x4 | ( (! (uint8_t)((i + 1 - (h -1)) >> 31)<<3))) & deblkMask;
        smoothPattern = 0x6 & smoothMask;


////Row deblocking
//STATISTICS_START_TIME(m_TStat->Reconst_StartTime);
//        if (m_pSequenceHeader->IsLoopFilter() && bRaiseFrame && i!=0)
//        {
//STATISTICS_START_TIME(m_TStat->Deblk_StartTime);
//          uint8_t *DeblkPlanes[3] = {m_pRaisedPlane[0] + i*m_uiRaisedPlaneStep[0]*VC1_ENC_LUMA_SIZE,
//                            m_pRaisedPlane[1] + i*m_uiRaisedPlaneStep[1]*VC1_ENC_CHROMA_SIZE,
//                            m_pRaisedPlane[2] + i*m_uiRaisedPlaneStep[2]*VC1_ENC_CHROMA_SIZE};
//            m_pMBs->StartRow();
//
//            if(bSubBlkTS)
//            {
//                if(i < h-1)
//                    Deblock_P_RowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//                else
//                    Deblock_P_BottomRowVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//            }
//            else
//            {
//                if(i < h-1)
//                    Deblock_P_RowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//                else
//                    Deblock_P_BottomRowNoVts(DeblkPlanes, m_uiRaisedPlaneStep, w, m_uiQuant, m_pMBs);
//            }
//STATISTICS_END_TIME(m_TStat->Deblk_StartTime, m_TStat->Deblk_EndTime, m_TStat->Deblk_TotalTime);
//        }
//STATISTICS_END_TIME(m_TStat->Reconst_StartTime, m_TStat->Reconst_EndTime, m_TStat->Reconst_TotalTime)
//

        err = m_pMBs->NextRow();
        if (err != UMC::UMC_OK)
            return err;

        pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        pFMBRow[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pFMBRow[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pFMBRow[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

    }

#ifdef VC1_ENC_DEBUG_ON
    if(bRaiseFrame)
    pDebug->PrintRestoredFrame(m_pRaisedPlane[0], m_uiRaisedPlaneStep[0],
                               m_pRaisedPlane[1], m_uiRaisedPlaneStep[1],
                               m_pRaisedPlane[2], m_uiRaisedPlaneStep[2], 0);
#endif

    return err;
}

UMC::Status VC1EncoderPictureSM::VLC_PFrameMixed(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status     err = UMC::UMC_OK;
    uint32_t          i=0, j=0;

    uint32_t                      h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t                      w = m_pSequenceHeader->GetNumMBInRow();

    const uint16_t*   pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

    eCodingSet      LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet      ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet       CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

    const sACTablesSet*  pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[ChromaCodingSetIntra]),
                                                 &(ACTablesSet[ChromaCodingSetIntra])};

    const sACTablesSet* pACTablesSetInter = &(ACTablesSet[CodingSetInter]);

    const uint32_t*  pDCTableVLCIntra[6]  ={DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][0],
                                          DCTables[m_uiDecTypeDCIntra][1],
                                          DCTables[m_uiDecTypeDCIntra][1]};

    sACEscInfo   ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
                               Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                               0, 0};

    bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
    bool bMVHalf = false;

    const int16_t   (*pTTMBVLC)[4][6]   = 0;
    const uint8_t    (* pTTBlkVLC)[6]    = 0;
    const uint8_t     *pSubPattern4x4VLC = 0;

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetCurrMBFirst();
#endif

    err = WritePPictureHeader(pCodedPicture);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuant<5)
    {
        pTTMBVLC            =  TTMBVLC_HighRate;
        pTTBlkVLC           =  TTBLKVLC_HighRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
    }
    else if (m_uiQuant<13)
    {
        pTTMBVLC            =  TTMBVLC_MediumRate;
        pTTBlkVLC           =  TTBLKVLC_MediumRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;
    }
    else
    {
        pTTMBVLC            =  TTMBVLC_LowRate;
        pTTBlkVLC           =  TTBLKVLC_LowRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
    }

    for (i=0; i < h; i++)
    {
        for (j=0; j < w; j++)
        {
            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);
#ifdef VC1_ME_MB_STATICTICS
            m_MECurMbStat->whole = 0;
            memset(m_MECurMbStat->MVF, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->MVB, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->coeff, 0, 6*sizeof(uint16_t));
            m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;

            pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
#endif
            switch  (pCompressedMB->GetMBType())
            {
            case VC1_ENC_P_MB_INTRA:
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif
                err = pCompressedMB->WritePMBMixed_INTRA (pCodedPicture,m_bRawBitplanes,m_uiQuant,
                                                          MVDiffTablesVLC[m_uiMVTab],
                                                          pCBPCYTable,
                                                          pDCTableVLCIntra,
                                                          pACTablesSetIntra,
                                                          &ACEscInfo);
                VC1_ENC_CHECK (err)


                    break;

            case VC1_ENC_P_MB_1MV:
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->MbType = UMC::ME_MbFrw;
#endif
                err = pCompressedMB->WritePMB1MVMixed(pCodedPicture, m_bRawBitplanes,m_uiMVTab,
                                                      m_uiMVRangeIndex, pCBPCYTable,
                                                      bCalculateVSTransform,
                                                      bMVHalf,
                                                      pTTMBVLC,
                                                      pTTBlkVLC,
                                                      pSubPattern4x4VLC,
                                                      pACTablesSetInter,
                                                      &ACEscInfo);
                VC1_ENC_CHECK (err)
                    break;

            case VC1_ENC_P_MB_SKIP_1MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                err = pCompressedMB->WritePMB1MVSkipMixed(pCodedPicture, m_bRawBitplanes);
                VC1_ENC_CHECK (err)
                    break;
            case VC1_ENC_P_MB_SKIP_4MV:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
#endif
                err = pCompressedMB->WritePMB4MVSkipMixed(pCodedPicture, m_bRawBitplanes);
                VC1_ENC_CHECK (err)
                    break;

            case VC1_ENC_P_MB_4MV:
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->MbType = UMC::ME_MbMixed;
#endif
                err = pCompressedMB->WritePMB4MVMixed(pCodedPicture,
                                                m_bRawBitplanes,   m_uiQuant,
                                                m_uiMVTab,         m_uiMVRangeIndex,
                                                pCBPCYTable,       MVDiffTablesVLC[m_uiMVTab],
                                                bCalculateVSTransform, bMVHalf,
                                                pTTMBVLC,          pTTBlkVLC,
                                                pSubPattern4x4VLC, pDCTableVLCIntra,
                                                pACTablesSetIntra, pACTablesSetInter,
                                                &ACEscInfo);
                VC1_ENC_CHECK (err)
                    break;
            }
#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
           m_MECurMbStat++;
#endif
        }
    }

    return err;
}

UMC::Status VC1EncoderPictureSM::PAC_BFrame(UMC::MeParams* MEParams)
{
    UMC::Status err           =   UMC::UMC_OK;
    uint32_t   i = 0, j = 0, blk = 0;
    uint8_t*   pCurMBRow[3],* pFMB[3], *pBMB[3];
    uint32_t   h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t   w = m_pSequenceHeader->GetNumMBInRow();

    _SetIntraInterLumaFunctions;
    CalculateChromaFunction     CalculateChroma      = (m_pSequenceHeader->IsFastUVMC())?
                                                        GetChromaMVFast:GetChromaMV;
    // ------------------------------------------------------------------------------------------------ //

    bool                        bIsBilinearInterpolation = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR);
    InterpolateFunction         InterpolateLumaFunction  = (bIsBilinearInterpolation)?
                                    _own_ippiInterpolateQPBilinear_VC1_8u_C1R:
                                    _own_ippiInterpolateQPBicubic_VC1_8u_C1R;

    InterpolateFunctionPadding  InterpolateLumaFunctionPadding = (bIsBilinearInterpolation)?
                                                                    own_ippiICInterpolateQPBilinearBlock_VC1_8u_P1R:
                                                                    own_ippiICInterpolateQPBicubicBlock_VC1_8u_P1R;

    IppVCInterpolate_8u         sYInterpolationF;
    IppVCInterpolate_8u         sUInterpolationF;
    IppVCInterpolate_8u         sVInterpolationF;
    IppVCInterpolate_8u         sYInterpolationB;
    IppVCInterpolate_8u         sUInterpolationB;
    IppVCInterpolate_8u         sVInterpolationB;

    IppVCInterpolateBlock_8u    InterpolateBlockYF;
    IppVCInterpolateBlock_8u    InterpolateBlockUF;
    IppVCInterpolateBlock_8u    InterpolateBlockVF;

    IppVCInterpolateBlock_8u    InterpolateBlockYB;
    IppVCInterpolateBlock_8u    InterpolateBlockUB;
    IppVCInterpolateBlock_8u    InterpolateBlockVB;

    uint8_t                       tempInterpolationBufferF[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
    uint8_t                       tempInterpolationBufferB[VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS];
    IppStatus                   ippSts = ippStsNoErr;

    eDirection                  direction[VC1_ENC_NUMBER_OF_BLOCKS];

    mfxSize                    blkSize     = {8,8};

    sCoordinate                 MVPredMin = {-28,-28};
    sCoordinate                 MVPredMax = {((int16_t)w*8-1)*4, ((int16_t)h*8-1)*4};

    uint8_t                       doubleQuant     =  2*m_uiQuant + m_bHalfQuant;
    uint8_t                       DCQuant         =  DCQuantValues[m_uiQuant];
    int16_t                      defPredictor    =  0;

    bool  bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR ||
                     m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

    bool  bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);

 #ifdef VC1_ENC_DEBUG_ON
    pDebug->SetHalfCoef(bMVHalf);
    pDebug->SetDeblkFlag(false);
    pDebug->SetVTSFlag(false);
#endif


    pCurMBRow[0]      =   m_pPlane[0];        //Luma
    pCurMBRow[1]      =   m_pPlane[1];        //Cb
    pCurMBRow[2]      =   m_pPlane[2];        //Cr

    pFMB[0]      =   m_pForwardPlane[0];        //Luma
    pFMB[1]      =   m_pForwardPlane[1];        //Cb
    pFMB[2]      =   m_pForwardPlane[2];        //Cr

    pBMB[0]      =   m_pBackwardPlane[0];        //Luma
    pBMB[1]      =   m_pBackwardPlane[1];        //Cb
    pBMB[2]      =   m_pBackwardPlane[2];        //Cr

    err = m_pMBs->Reset();
    if (err != UMC::UMC_OK)
        return err;

    SetInterpolationParams(&sYInterpolationF,&sUInterpolationF,&sVInterpolationF,
                           tempInterpolationBufferF,true);
    SetInterpolationParams(&sYInterpolationB,&sUInterpolationB,&sVInterpolationB,
                           tempInterpolationBufferB,false);

    SetInterpolationParams(&InterpolateBlockYF,&InterpolateBlockUF,&InterpolateBlockVF, tempInterpolationBufferF,
                            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
                            m_pForwardPlane, m_uiForwardPlaneStep, false);
    SetInterpolationParams(&InterpolateBlockYB,&InterpolateBlockUB,&InterpolateBlockVB, tempInterpolationBufferB,
                            m_pSequenceHeader->GetPictureWidth(),m_pSequenceHeader->GetPictureHeight(),
                            m_pBackwardPlane, m_uiBackwardPlaneStep, false);
#ifdef VC1_ENC_CHECK_MV
    err = CheckMEMV_B(MEParams, bMVHalf);
    assert(err == UMC::UMC_OK);
#endif

    for (i=0; i < h; i++)
    {
        for (j=0; j < w; j++)
        {
            VC1EncoderMBInfo  *         pCurMBInfo     = 0;
            VC1EncoderMBData  *         pCurMBData     = 0;
            VC1EncoderMBData  *         pRecMBData     = 0;

            VC1EncoderCodedMB*          pCompressedMB  = &(m_pCodedMB[w*i+j]);
            int32_t                      posX           =  VC1_ENC_LUMA_SIZE*j;
            int32_t                      posXChroma     =  VC1_ENC_CHROMA_SIZE*j;
            uint8_t                       MBPattern       = 0;
            uint8_t                       CBPCY           = 0;
            eMBType                     mbType         =  VC1_ENC_B_MB_F; //From ME

            //int16_t          posY        =  VC1_ENC_LUMA_SIZE*i;

            sCoordinate MVF         ={0,0};
            sCoordinate MVB         ={0,0};

            pCurMBInfo  =   m_pMBs->GetCurrMBInfo();
            pCurMBData  =   m_pMBs->GetCurrMBData();
            pRecMBData  =   m_pMBs->GetRecCurrMBData();
            

            VC1EncoderMBInfo* left        = m_pMBs->GetLeftMBInfo();
            VC1EncoderMBInfo* topLeft     = m_pMBs->GetTopLeftMBInfo();
            VC1EncoderMBInfo* top         = m_pMBs->GetTopMBInfo();
            VC1EncoderMBInfo* topRight    = m_pMBs->GetTopRightMBInfo();

            MVF.x  = MEParams->pSrc->MBs[j + i*w].MV[0][0].x;
            MVF.y  = MEParams->pSrc->MBs[j + i*w].MV[0][0].y;
            MVB.x  = MEParams->pSrc->MBs[j + i*w].MV[1][0].x;
            MVB.y  = MEParams->pSrc->MBs[j + i*w].MV[1][0].y;

            switch (MEParams->pSrc->MBs[(j + i*w)].MbType)
            {
            case UMC::ME_MbIntra:
                  MVF.x = MVF.y = MVB.x = MVB.y = 0;
                  mbType = VC1_ENC_B_MB_INTRA;
                  break;

            case UMC::ME_MbFrw:

                mbType = VC1_ENC_B_MB_F;
                break;

            case UMC::ME_MbFrwSkipped:

                mbType = VC1_ENC_B_MB_SKIP_F;
                break;

            case UMC::ME_MbBkw:

                mbType = VC1_ENC_B_MB_B;
                break;
            case UMC::ME_MbBkwSkipped:

                mbType = VC1_ENC_B_MB_SKIP_B;
                break;
            case UMC::ME_MbBidir:

                mbType = VC1_ENC_B_MB_FB;
                break;

            case UMC::ME_MbBidirSkipped:

                mbType = VC1_ENC_B_MB_SKIP_FB;

                break;

            case UMC::ME_MbDirect:

                mbType = VC1_ENC_B_MB_DIRECT;

                break;
            case UMC::ME_MbDirectSkipped:

                mbType = VC1_ENC_B_MB_SKIP_DIRECT;

                break;
            default:
                assert(0);
                return UMC::UMC_ERR_FAILED;


            }
            switch(mbType)
            {
            case VC1_ENC_B_MB_INTRA:
                {
                bool                dACPrediction   = true;
                NeighbouringMBsData MBs;

                MBs.LeftMB    = ((left)? left->isIntra():0)         ? m_pMBs->GetLeftMBData():0;
                MBs.TopMB     = ((top)? top->isIntra():0)           ? m_pMBs->GetTopMBData():0;
                MBs.TopLeftMB = ((topLeft)? topLeft->isIntra():0)   ? m_pMBs->GetTopLeftMBData():0;

                pCompressedMB->Init(VC1_ENC_B_MB_INTRA);
                pCurMBInfo->Init(true);

                pCurMBData->CopyMBProg(pCurMBRow[0],m_uiPlaneStep[0],pCurMBRow[1], pCurMBRow[2], m_uiPlaneStep[1],j);

                //only intra blocks:
                for (blk = 0; blk<6; blk++)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    _own_Diff8x8C_16s(128, pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk], blkSize, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    IntraTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk], pCurMBData->m_uiBlockStep[blk],
                                         DCQuant, doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                }
STATISTICS_START_TIME(m_TStat->Intra_StartTime);
                    // should be changed on DCACPredictionPFrameSM
                    dACPrediction = DCACPredictionFrame     ( pCurMBData,&MBs,
                                                            pRecMBData, defPredictor,direction);
STATISTICS_END_TIME(m_TStat->Intra_StartTime, m_TStat->Intra_EndTime, m_TStat->Intra_TotalTime);

                for (blk=0; blk<6; blk++)
                {
                    pCompressedMB->SaveResidual(    pRecMBData->m_pBlock[blk],
                                                    pRecMBData->m_uiBlockStep[blk],
                                                    VC1_Inter_8x8_Scan,
                                                    blk);
                }
                MBPattern = pCompressedMB->GetMBPattern();
                CBPCY = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetACPrediction(dACPrediction);
                pCompressedMB->SetMBCBPCY(CBPCY);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(VC1_ENC_B_MB_INTRA);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
#endif
                break;
                }
            case VC1_ENC_B_MB_SKIP_F:
                {
                sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                sCoordinate  mvPred   = {0,0};

                pCompressedMB->Init(mbType);

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                GetMVPredictionP(true)
                PredictMV(mvA,mvB,mvC, &mvPred);
                ScalePredict(&mvPred, j*8*4,i*8*4,MVPredMin,MVPredMax);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                assert (mvPred.x == MVF.x && mvPred.y == MVF.y);

                MVF.x = mvPred.x;  MVF.y = mvPred.y;

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetMBAsSkip();
                pDebug->SetMVInfo(&MVF, mvPred.x, mvPred.y, 0);
                pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
#endif
                break;
                }
            case VC1_ENC_B_MB_SKIP_B:
                {
                sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                sCoordinate  mvPred   = {0,0};

                pCompressedMB->Init(mbType);

STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                GetMVPredictionP(false)
                PredictMV(mvA,mvB,mvC, &mvPred);
                ScalePredict(&mvPred, j*8*4,i*8*4,MVPredMin,MVPredMax);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                assert (mvPred.x == MVB.x && mvPred.y == MVB.y);

                MVB.x = mvPred.x;  MVB.y = mvPred.y;

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetMBAsSkip();
                pDebug->SetMVInfo(&MVF, mvPred.x, mvPred.y, 0);
                pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
#endif
                break;
                }
            case VC1_ENC_B_MB_F:
            case VC1_ENC_B_MB_B:
                {
                sCoordinate  *mvC=0,   *mvB=0,   *mvA=0;
                sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                sCoordinate  mvPred   = {0,0};
                sCoordinate  mvDiff   = {0,0};
                bool         bForward = (mbType == VC1_ENC_B_MB_F);

                IppVCInterpolate_8u*         pYInterpolation= &sYInterpolationF;
                IppVCInterpolate_8u*         pUInterpolation= &sUInterpolationF;
                IppVCInterpolate_8u*         pVInterpolation= &sVInterpolationF;
                IppVCInterpolateBlock_8u*    pInterpolateBlockY = &InterpolateBlockYF;
                IppVCInterpolateBlock_8u*    pInterpolateBlockU = &InterpolateBlockUF;
                IppVCInterpolateBlock_8u*    pInterpolateBlockV = &InterpolateBlockVF;

                bool                         padded = (m_uiForwardPlanePadding!=0);
                uint8_t                        opposite = 0;

                uint8_t**                      pPredMB   = pFMB;
                uint32_t*                      pPredStep = m_uiForwardPlaneStep;

                sCoordinate*                 pMV                = &MVF;

                sCoordinate                  MVInt             = {0,0};
                sCoordinate                  MVQuarter         = {0,0};

                sCoordinate                  MVChroma          = {0,0};
                sCoordinate                  MVIntChroma       = {0,0};
                sCoordinate                  MVQuarterChroma   = {0,0};


                eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType};


                if (!bForward)
                {
                    pYInterpolation= &sYInterpolationB;
                    pUInterpolation= &sUInterpolationB;
                    pVInterpolation= &sVInterpolationB;
                    pInterpolateBlockY = &InterpolateBlockYB;
                    pInterpolateBlockU = &InterpolateBlockUB;
                    pInterpolateBlockV = &InterpolateBlockVB;
                    padded = (m_uiBackwardPlanePadding != 0);
                    pPredMB   = pBMB;
                    pPredStep = m_uiBackwardPlaneStep;
                    pMV       = &MVB;
                }

                pCompressedMB->Init(mbType);

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

 STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                GetMVPredictionP(bForward)
                PredictMV(mvA,mvB,mvC, &mvPred);
                ScalePredict(&mvPred, j*8*4,i*8*4,MVPredMin,MVPredMax);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                mvDiff.x = (pMV->x - mvPred.x);
                mvDiff.y = (pMV->y - mvPred.y);
                pCompressedMB->SetdMV(mvDiff,bForward);

                GetIntQuarterMV(*pMV,&MVInt,&MVQuarter);
                CalculateChroma(*pMV,&MVChroma);
                GetIntQuarterMV(MVChroma,&MVIntChroma,&MVQuarterChroma);
                if (padded)
                {
STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    SetInterpolationParamsLuma(pYInterpolation, pPredMB[0], pPredStep[0], &MVInt, &MVQuarter, j);
                    ippSts = InterpolateLumaFunction(pYInterpolation);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (pUInterpolation, pVInterpolation, pPredMB[1], pPredMB[2],pPredStep[1], 
                                       &MVIntChroma, &MVQuarterChroma, j);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                }
                else
                {

                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                    SetInterpolationParamsLuma(pInterpolateBlockY, j, i , &MVInt, &MVQuarter);
                    ippSts = InterpolateLumaFunctionPadding(pInterpolateBlockY,0,0,opposite,false,m_uiRoundControl,0);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChromaPad   (pInterpolateBlockU, pInterpolateBlockV, 
                                            opposite,false,m_uiRoundControl,0,
                                            j, i ,  &MVIntChroma, &MVQuarterChroma);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime); 
 
                }
                if (bCalculateVSTransform)
                {
#ifndef OWN_VS_TRANSFORM
                    GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                    VC1EncMD_P pIn;
                    pIn.pYSrc    = pCurMBRow[0]+posX; 
                    pIn.srcYStep = m_uiPlaneStep[0];
                    pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                    pIn.srcUStep = m_uiPlaneStep[1];
                    pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                    pIn.srcVStep = m_uiPlaneStep[2];

                    pIn.pYRef    = pYInterpolation->pDst; 
                    pIn.refYStep = pYInterpolation->dstStep;      
                    pIn.pURef    = pUInterpolation->pDst; 
                    pIn.refUStep = pUInterpolation->dstStep;
                    pIn.pVRef    = pVInterpolation->pDst; 
                    pIn.refVStep = pVInterpolation->dstStep;

                    pIn.quant         = doubleQuant;
                    pIn.bUniform      = m_bUniformQuant;
                    pIn.intraPattern  = 0;
                    pIn.DecTypeAC1    = m_uiDecTypeAC1;
                    pIn.pScanMatrix   = ZagTables;
                    pIn.bField        = 0;
                    pIn.CBPTab        = m_uiCBPTab;

                    GetVSTTypeP_RD (&pIn, BlockTSTypes)  ; 
#endif      

                }
                pCompressedMB->SetTSType(BlockTSTypes);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                pCurMBData->CopyDiffMBProg (pCurMBRow[0],m_uiPlaneStep[0], pCurMBRow[1],pCurMBRow[2], m_uiPlaneStep[1], 
                            pYInterpolation->pDst, pYInterpolation->dstStep,
                            pUInterpolation->pDst,pVInterpolation->pDst, pUInterpolation->dstStep,
                            j,0);

IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                MBPattern = 0;
                for (blk = 0; blk<6; blk++)
                {
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                  BlockTSTypes[blk], doubleQuant, MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                    pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                                 pCurMBData->m_uiBlockStep[blk],
                                                 ZagTables[BlockTSTypes[blk]],
                                                 blk);
                }
                MBPattern   = pCompressedMB->GetMBPattern();
                CBPCY       = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetMBCBPCY(CBPCY);
                if (MBPattern==0 && mvDiff.x == 0 && mvDiff.y == 0)
                {
                    if (bForward)
                    {
                        pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_F);
                        mbType = VC1_ENC_B_MB_SKIP_F;
                    }
                    else
                    {
                        pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_B);
                        mbType = VC1_ENC_B_MB_SKIP_B;
                    }
#ifdef VC1_ENC_DEBUG_ON
                    pDebug->SetMBAsSkip();
#endif
                }

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetMVInfo(&MVF,mvPred.x, mvPred.y, 0);
                pDebug->SetMVInfo(&MVB, mvPred.x, mvPred.y, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                pDebug->SetMVInfo(&MVChroma,  0, 0, !bForward, 4);
                pDebug->SetMVInfo(&MVChroma,  0, 0, !bForward, 5);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);

                pDebug->SetInterpInfo(pYInterpolation,  pUInterpolation, pVInterpolation, !bForward, padded);
#endif
                break;
                }
            case VC1_ENC_B_MB_FB:
                {
                sCoordinate  *mvC_F=0,   *mvB_F=0,   *mvA_F=0;
                sCoordinate  *mvC_B=0,   *mvB_B=0,   *mvA_B=0;
                sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                sCoordinate  mv4={0,0}, mv5={0,0}, mv6={0,0};
                sCoordinate  mvPredF    = {0,0};
                sCoordinate  mvPredB    = {0,0};
                sCoordinate  mvDiffF    = {0,0};
                sCoordinate  mvDiffB    = {0,0};
                sCoordinate  MVIntF     = {0,0};
                sCoordinate  MVQuarterF = {0,0};
                sCoordinate  MVIntB     = {0,0};
                sCoordinate  MVQuarterB = {0,0};

                sCoordinate  MVChromaF       = {0,0};
                sCoordinate  MVIntChromaF    = {0,0};
                sCoordinate  MVQuarterChromaF= {0,0};
                sCoordinate  MVChromaB       = {0,0};
                sCoordinate  MVIntChromaB    = {0,0};
                sCoordinate  MVQuarterChromaB= {0,0};


                eTransformType  BlockTSTypes[6] = { m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType,
                                                    m_uiTransformType, m_uiTransformType};

                pCompressedMB->Init(mbType);

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

 STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                GetMVPredictionB
                PredictMV(mvA_F,mvB_F,mvC_F, &mvPredF);
                PredictMV(mvA_B,mvB_B,mvC_B, &mvPredB);
                ScalePredict(&mvPredF, j*8*4,i*8*4,MVPredMin,MVPredMax);
                ScalePredict(&mvPredB, j*8*4,i*8*4,MVPredMin,MVPredMax);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                mvDiffF.x = (MVF.x - mvPredF.x);
                mvDiffF.y = (MVF.y - mvPredF.y);
                pCompressedMB->SetdMV(mvDiffF,true);

                mvDiffB.x = (MVB.x - mvPredB.x);
                mvDiffB.y = (MVB.y - mvPredB.y);
                pCompressedMB->SetdMV(mvDiffB,false);

                GetIntQuarterMV(MVF,&MVIntF,&MVQuarterF);
                CalculateChroma(MVF,&MVChromaF);
                GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                GetIntQuarterMV(MVB,&MVIntB,&MVQuarterB);
                CalculateChroma(MVB,&MVChromaB);
                GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);

                if (m_uiForwardPlanePadding)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    SetInterpolationParamsLuma(&sYInterpolationF, pFMB[0], m_uiForwardPlaneStep[0], &MVIntF, &MVQuarterF, j);
                    ippSts = InterpolateLumaFunction(&sYInterpolationF);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pFMB[1], pFMB[2],m_uiForwardPlaneStep[1], 
                                        &MVIntChromaF, &MVQuarterChromaF, j);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                }
                else
                {                
                    bool bOpposite = 0;

                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                    SetInterpolationParamsLuma(&InterpolateBlockYF, j, i , &MVIntF, &MVQuarterF);
                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF,0,0,bOpposite,false,m_uiRoundControl,0);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChromaPad   (&InterpolateBlockUF, &InterpolateBlockVF, 
                                            bOpposite,false,m_uiRoundControl,0,
                                            j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);

                }
                if (m_uiBackwardPlanePadding)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                    SetInterpolationParamsLuma(&sYInterpolationB, pBMB[0], m_uiBackwardPlaneStep[0], &MVIntB, &MVQuarterB, j);
                    ippSts = InterpolateLumaFunction(&sYInterpolationB);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBMB[1], pBMB[2],m_uiBackwardPlaneStep[1], 
                        &MVIntChromaB, &MVQuarterChromaB, j);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                }
                else
                {
                    bool bOpposite = 0;

                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                    SetInterpolationParamsLuma(&InterpolateBlockYB, j, i , &MVIntB, &MVQuarterB);
                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB,0,0,bOpposite,false,m_uiRoundControl,0);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChromaPad   (&InterpolateBlockUB, &InterpolateBlockVB, 
                                            bOpposite,false,m_uiRoundControl,0,
                                            j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
                
                }

                if (bCalculateVSTransform)
                {
#ifndef OWN_VS_TRANSFORM
                    GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                    VC1EncMD_B pIn;
                    pIn.pYSrc    = pCurMBRow[0]+posX; 
                    pIn.srcYStep = m_uiPlaneStep[0];
                    pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                    pIn.srcUStep = m_uiPlaneStep[1];
                    pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                    pIn.srcVStep = m_uiPlaneStep[2];

                    pIn.pYRef[0]    = sYInterpolationF.pDst; 
                    pIn.refYStep[0] = sYInterpolationF.dstStep;      
                    pIn.pURef[0]    = sUInterpolationF.pDst; 
                    pIn.refUStep[0] = sUInterpolationF.dstStep;
                    pIn.pVRef[0]    = sVInterpolationF.pDst; 
                    pIn.refVStep[0] = sVInterpolationF.dstStep;

                    pIn.pYRef[1]    = sYInterpolationB.pDst; 
                    pIn.refYStep[1] = sYInterpolationB.dstStep;      
                    pIn.pURef[1]    = sUInterpolationB.pDst; 
                    pIn.refUStep[1] = sUInterpolationB.dstStep;
                    pIn.pVRef[1]    = sVInterpolationB.pDst; 
                    pIn.refVStep[1] = sVInterpolationB.dstStep;

                    pIn.quant         = doubleQuant;
                    pIn.bUniform      = m_bUniformQuant;
                    pIn.intraPattern  = 0;
                    pIn.DecTypeAC1    = m_uiDecTypeAC1;
                    pIn.pScanMatrix   = ZagTables;
                    pIn.bField        = 0;
                    pIn.CBPTab        = m_uiCBPTab;

                    GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                }
                pCompressedMB->SetTSType(BlockTSTypes);
STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  m_uiPlaneStep[0], pCurMBRow[1],  pCurMBRow[2],  m_uiPlaneStep[1], 
                                            sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                            sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                            sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                            sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                            j, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);

                for (blk = 0; blk<6; blk++)
                {
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                  BlockTSTypes[blk], doubleQuant,MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                    pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                                 pCurMBData->m_uiBlockStep[blk],
                                                 ZagTables[BlockTSTypes[blk]],
                                                 blk);
                }
                MBPattern   = pCompressedMB->GetMBPattern();
                CBPCY       = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetMBCBPCY(CBPCY);
                if (MBPattern==0 && mvDiffF.x == 0 && mvDiffF.y == 0 && mvDiffB.x == 0 && mvDiffB.y == 0)
                {
                     pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_FB);
                     mbType = VC1_ENC_B_MB_SKIP_FB;
#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBAsSkip();
#endif
                }

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetMVInfo(&MVF, mvPredF.x, mvPredF.y, 0);
                pDebug->SetMVInfo(&MVB, mvPredB.x, mvPredB.y, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);
                pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 4);
                pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 5);
                pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 4);
                pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 5);
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, m_uiForwardPlanePadding);
                pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, m_uiBackwardPlanePadding);
#endif
                break;
                }
            case VC1_ENC_B_MB_SKIP_FB:
                {
                sCoordinate  *mvC_F=0,   *mvB_F=0,   *mvA_F=0;
                sCoordinate  *mvC_B=0,   *mvB_B=0,   *mvA_B=0;
                sCoordinate  mv1={0,0}, mv2={0,0}, mv3={0,0};
                sCoordinate  mv4={0,0}, mv5={0,0}, mv6={0,0};
                sCoordinate  mvPredF    = {0,0};
                sCoordinate  mvPredB    = {0,0};

                pCompressedMB->Init(mbType);

  STATISTICS_START_TIME(m_TStat->Inter_StartTime);
                GetMVPredictionB
                PredictMV(mvA_F,mvB_F,mvC_F, &mvPredF);
                PredictMV(mvA_B,mvB_B,mvC_B, &mvPredB);
                ScalePredict(&mvPredF, j*8*4,i*8*4,MVPredMin,MVPredMax);
                ScalePredict(&mvPredB, j*8*4,i*8*4,MVPredMin,MVPredMax);
STATISTICS_END_TIME(m_TStat->Inter_StartTime, m_TStat->Inter_EndTime, m_TStat->Inter_TotalTime);

                assert (MVF.x == mvPredF.x && MVF.y == mvPredF.y &&
                        MVB.x == mvPredB.x && MVB.y == mvPredB.y);

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(mvPredF,true);
                pCurMBInfo->SetMV(mvPredB,false);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetMBAsSkip();

                pDebug->SetMVInfo(&MVF, mvPredF.x, mvPredF.y, 0);
                pDebug->SetMVInfo(&MVB, mvPredB.x, mvPredB.y, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                //pDebug->SetMVInfo(MVChroma.x, MVChroma.y,  0, 0, !bForward, 4);
                //pDebug->SetMVInfo(MVChroma.x, MVChroma.y,  0, 0, !bForward, 5);

#endif
                break;
                }
            case VC1_ENC_B_MB_DIRECT:
                {
                //sCoordinate  mvPredF    = {0,0};
                //sCoordinate  mvPredB    = {0,0};
                sCoordinate  MVIntF     = {0,0};
                sCoordinate  MVQuarterF = {0,0};
                sCoordinate  MVIntB     = {0,0};
                sCoordinate  MVQuarterB = {0,0};

                sCoordinate  MVChromaF       = {0,0};
                sCoordinate  MVIntChromaF    = {0,0};
                sCoordinate  MVQuarterChromaF= {0,0};
                sCoordinate  MVChromaB       = {0,0};
                sCoordinate  MVIntChromaB    = {0,0};
                sCoordinate  MVQuarterChromaB= {0,0};

                eTransformType  BlockTSTypes[6] = { m_uiTransformType,
                                                    m_uiTransformType,
                                                    m_uiTransformType,
                                                    m_uiTransformType,
                                                    m_uiTransformType,
                                                    m_uiTransformType};

                //direct
                pCompressedMB->Init(mbType);

                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

                GetIntQuarterMV(MVF,&MVIntF,&MVQuarterF);
                CalculateChroma(MVF,&MVChromaF);
                GetIntQuarterMV(MVChromaF,&MVIntChromaF,&MVQuarterChromaF);

                GetIntQuarterMV(MVB,&MVIntB,&MVQuarterB);
                CalculateChroma(MVB,&MVChromaB);
                GetIntQuarterMV(MVChromaB,&MVIntChromaB,&MVQuarterChromaB);

STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                if (m_uiForwardPlanePadding)
                {
IPP_STAT_START_TIME(m_IppStat->IppStartTime);

                    SetInterpolationParamsLuma(&sYInterpolationF, pFMB[0], m_uiForwardPlaneStep[0], &MVIntF, &MVQuarterF, j);
                    ippSts = InterpolateLumaFunction(&sYInterpolationF);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (&sUInterpolationF, &sVInterpolationF, pFMB[1], pFMB[2],m_uiForwardPlaneStep[1], 
                        &MVIntChromaF, &MVQuarterChromaF, j);

IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                }
                else
                {
                    bool bOpposite = 0;

                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                    SetInterpolationParamsLuma(&InterpolateBlockYF, j, i , &MVIntF, &MVQuarterF);
                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYF,0,0,bOpposite,false,m_uiRoundControl,0);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChromaPad   (&InterpolateBlockUF, &InterpolateBlockVF, 
                        bOpposite,false,m_uiRoundControl,0,
                        j, i ,  &MVIntChromaF, &MVQuarterChromaF);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);             
                }

                if (m_uiBackwardPlanePadding)
                {
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    SetInterpolationParamsLuma(&sYInterpolationB, pBMB[0], m_uiBackwardPlaneStep[0], &MVIntB, &MVQuarterB, j);
                    ippSts = InterpolateLumaFunction(&sYInterpolationB);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChroma (&sUInterpolationB, &sVInterpolationB, pBMB[1], pBMB[2],m_uiBackwardPlaneStep[1], 
                        &MVIntChromaB, &MVQuarterChromaB, j);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                }
                else
                {
                    bool bOpposite = 0;
                    STATISTICS_START_TIME(m_TStat->Interpolate_StartTime);
                    IPP_STAT_START_TIME(m_IppStat->IppStartTime);                               

                    SetInterpolationParamsLuma(&InterpolateBlockYB, j, i , &MVIntB, &MVQuarterB);
                    ippSts = InterpolateLumaFunctionPadding(&InterpolateBlockYB,0,0,bOpposite,false,m_uiRoundControl,0);
                    VC1_ENC_IPP_CHECK(ippSts);

                    InterpolateChromaPad   (&InterpolateBlockUB, &InterpolateBlockVB, 
                        bOpposite,false,m_uiRoundControl,0,
                        j, i ,  &MVIntChromaB, &MVQuarterChromaB);

                    IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);
                    STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);                
                }
                if (bCalculateVSTransform)
                {
#ifndef OWN_VS_TRANSFORM
                    GetTSType (MEParams->pSrc->MBs[j + i*w].BlockTrans, BlockTSTypes);
#else
                    VC1EncMD_B pIn;
                    pIn.pYSrc    = pCurMBRow[0]+posX; 
                    pIn.srcYStep = m_uiPlaneStep[0];
                    pIn.pUSrc    = pCurMBRow[1]+posXChroma; 
                    pIn.srcUStep = m_uiPlaneStep[1];
                    pIn.pVSrc    = pCurMBRow[2]+posXChroma;  
                    pIn.srcVStep = m_uiPlaneStep[2];

                    pIn.pYRef[0]    = sYInterpolationF.pDst; 
                    pIn.refYStep[0] = sYInterpolationF.dstStep;      
                    pIn.pURef[0]    = sUInterpolationF.pDst; 
                    pIn.refUStep[0] = sUInterpolationF.dstStep;
                    pIn.pVRef[0]    = sVInterpolationF.pDst; 
                    pIn.refVStep[0] = sVInterpolationF.dstStep;

                    pIn.pYRef[1]    = sYInterpolationB.pDst; 
                    pIn.refYStep[1] = sYInterpolationB.dstStep;      
                    pIn.pURef[1]    = sUInterpolationB.pDst; 
                    pIn.refUStep[1] = sUInterpolationB.dstStep;
                    pIn.pVRef[1]    = sVInterpolationB.pDst; 
                    pIn.refVStep[1] = sVInterpolationB.dstStep;

                    pIn.quant         = doubleQuant;
                    pIn.bUniform      = m_bUniformQuant;
                    pIn.intraPattern  = 0;
                    pIn.DecTypeAC1    = m_uiDecTypeAC1;
                    pIn.pScanMatrix   = ZagTables;
                    pIn.bField        = 0;
                    pIn.CBPTab        = m_uiCBPTab;

                    GetVSTTypeB_RD (&pIn, BlockTSTypes)  ; 
#endif                   
                }
                pCompressedMB->SetTSType(BlockTSTypes);

STATISTICS_END_TIME(m_TStat->Interpolate_StartTime, m_TStat->Interpolate_EndTime, m_TStat->Interpolate_TotalTime);
IPP_STAT_START_TIME(m_IppStat->IppStartTime);
                pCurMBData->CopyBDiffMBProg( pCurMBRow[0],  m_uiPlaneStep[0], pCurMBRow[1],  pCurMBRow[2],  m_uiPlaneStep[1], 
                                            sYInterpolationF.pDst,  sYInterpolationF.dstStep,
                                            sUInterpolationF.pDst,  sVInterpolationF.pDst,   sUInterpolationF.dstStep,
                                            sYInterpolationB.pDst,  sYInterpolationB.dstStep,
                                            sUInterpolationB.pDst,  sVInterpolationB.pDst,   sUInterpolationB.dstStep,
                                            j, 0);
IPP_STAT_END_TIME(m_IppStat->IppStartTime, m_IppStat->IppEndTime, m_IppStat->IppTotalTime);


                for (blk = 0; blk<6; blk++)
                {
STATISTICS_START_TIME(m_TStat->FwdQT_StartTime);
                    InterTransformQuantACFunction[blk](pCurMBData->m_pBlock[blk],pCurMBData->m_uiBlockStep[blk],
                                                  BlockTSTypes[blk], doubleQuant, MEParams->pSrc->MBs[j + i*w].RoundControl[blk],8*sizeof(uint8_t));
STATISTICS_END_TIME(m_TStat->FwdQT_StartTime, m_TStat->FwdQT_EndTime, m_TStat->FwdQT_TotalTime);
                    pCompressedMB->SaveResidual(pCurMBData->m_pBlock[blk],
                                                 pCurMBData->m_uiBlockStep[blk],
                                                 ZagTables[BlockTSTypes[blk]],
                                                 blk);
                }
                MBPattern   = pCompressedMB->GetMBPattern();
                CBPCY       = MBPattern;
                pCurMBInfo->SetMBPattern(MBPattern);
                pCompressedMB->SetMBCBPCY(CBPCY);


                if (MBPattern==0)
                {
                     pCompressedMB->ChangeType(VC1_ENC_B_MB_SKIP_DIRECT);
                     mbType = VC1_ENC_B_MB_SKIP_DIRECT;
#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBAsSkip();
#endif
                }

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetCPB(MBPattern, CBPCY);
                pDebug->SetMBType(mbType);
                pDebug->SetMVInfo(&MVF, 0, 0, 0);
                pDebug->SetMVInfo(&MVB, 0, 0, 1);
                pDebug->SetIntrpMV(MVF.x, MVF.y, 0);
                pDebug->SetIntrpMV(MVB.x, MVB.y, 1);

                pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 4);
                pDebug->SetMVInfo(&MVChromaF,  0, 0, 0, 5);
                pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 4);
                pDebug->SetMVInfo(&MVChromaB,  0, 0, 1, 5);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetBlockDifference(pCurMBData->m_pBlock, pCurMBData->m_uiBlockStep);
                pDebug->SetInterpInfo(&sYInterpolationF,  &sUInterpolationF, &sVInterpolationF, 0, m_uiForwardPlanePadding);
                pDebug->SetInterpInfo(&sYInterpolationB,  &sUInterpolationB, &sVInterpolationB, 1, m_uiBackwardPlanePadding);
#endif
                break;
                }
            case VC1_ENC_B_MB_SKIP_DIRECT:
                {
                pCompressedMB->Init(mbType);
                pCurMBInfo->Init(false);
                pCurMBInfo->SetMV(MVF,true);
                pCurMBInfo->SetMV(MVB,false);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetMBType(mbType);
                pDebug->SetQuant(m_uiQuant,m_bHalfQuant);
                pDebug->SetMBAsSkip();
#endif
                break;
                }
            default:
                return UMC::UMC_ERR_FAILED;
            }

            err = m_pMBs->NextMB();
            if (err != UMC::UMC_OK && j < w-1)
                return err;

#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
        }
        err = m_pMBs->NextRow();
        if (err != UMC::UMC_OK)
            return err;

        pCurMBRow[0]+= m_uiPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pCurMBRow[1]+= m_uiPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pCurMBRow[2]+= m_uiPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        pFMB[0]+= m_uiForwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pFMB[1]+= m_uiForwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pFMB[2]+= m_uiForwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;

        pBMB[0]+= m_uiBackwardPlaneStep[0]*VC1_ENC_LUMA_SIZE;
        pBMB[1]+= m_uiBackwardPlaneStep[1]*VC1_ENC_CHROMA_SIZE;
        pBMB[2]+= m_uiBackwardPlaneStep[2]*VC1_ENC_CHROMA_SIZE;
    }

    return err;
}

UMC::Status VC1EncoderPictureSM::VLC_BFrame(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status    err  = UMC::UMC_OK;
    uint32_t         i = 0, j = 0, blk = 0;

    uint32_t         h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t         w = m_pSequenceHeader->GetNumMBInRow();

    const uint16_t*  pCBPCYTable = VLCTableCBPCY_PB[m_uiCBPTab];

    eCodingSet     LumaCodingSetIntra   = LumaCodingSetsIntra  [m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet     ChromaCodingSetIntra = ChromaCodingSetsIntra[m_uiQuantIndex>8][m_uiDecTypeAC1];
    eCodingSet     CodingSetInter       = CodingSetsInter      [m_uiQuantIndex>8][m_uiDecTypeAC1];

    const sACTablesSet*  pACTablesSetIntra[6] = {&(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[LumaCodingSetIntra]),
                                                 &(ACTablesSet[ChromaCodingSetIntra]),
                                                 &(ACTablesSet[ChromaCodingSetIntra])};

    const sACTablesSet* pACTablesSetInter = &(ACTablesSet[CodingSetInter]);


    const uint32_t*       pDCTableVLCIntra[6] = {DCTables[m_uiDecTypeDCIntra][0],
                                               DCTables[m_uiDecTypeDCIntra][0],
                                               DCTables[m_uiDecTypeDCIntra][0],
                                               DCTables[m_uiDecTypeDCIntra][0],
                                               DCTables[m_uiDecTypeDCIntra][1],
                                               DCTables[m_uiDecTypeDCIntra][1]};

    sACEscInfo    ACEscInfo = {(m_uiQuant<= 7 /*&& !VOPQuant*/)?
                                Mode3SizeConservativeVLC : Mode3SizeEfficientVLC,
                                0, 0};

    bool bCalculateVSTransform = (m_pSequenceHeader->IsVSTransform())&&(!m_bVSTransform);
    bool bMVHalf = (m_uiMVMode == VC1_ENC_1MV_HALF_BILINEAR || m_uiMVMode == VC1_ENC_1MV_HALF_BICUBIC) ? true: false;

    const int16_t  (*pTTMBVLC)[4][6]   = 0;
    const uint8_t   (* pTTBlkVLC)[6]    = 0;
    const uint8_t    *pSubPattern4x4VLC = 0;

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetCurrMBFirst();
#endif

    err = WriteBPictureHeader(pCodedPicture);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuant<5)
    {
        pTTMBVLC            =  TTMBVLC_HighRate;
        pTTBlkVLC           =  TTBLKVLC_HighRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_HighRate;
    }
    else if (m_uiQuant<13)
    {
        pTTMBVLC            =  TTMBVLC_MediumRate;
        pTTBlkVLC           =  TTBLKVLC_MediumRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_MediumRate;

    }
    else
    {
        pTTMBVLC            =  TTMBVLC_LowRate;
        pTTBlkVLC           =  TTBLKVLC_LowRate;
        pSubPattern4x4VLC   =  SubPattern4x4VLC_LowRate;
    }

    for (i=0; i < h; i++)
    {
        for (j=0; j < w; j++)
        {
            VC1EncoderCodedMB*  pCompressedMB = &(m_pCodedMB[w*i+j]);

#ifdef VC1_ME_MB_STATICTICS
            m_MECurMbStat->whole = 0;
            memset(m_MECurMbStat->MVF, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->MVB, 0,   4*sizeof(uint16_t));
            memset(m_MECurMbStat->coeff, 0, 6*sizeof(uint16_t));
            m_MECurMbStat->qp    = 2*m_uiQuant + m_bHalfQuant;
            pCompressedMB->SetMEFrStatPointer(m_MECurMbStat);
#endif

            switch(pCompressedMB->GetMBType())
            {

            case VC1_ENC_B_MB_INTRA:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbIntra;
#endif

                err = pCompressedMB->WriteMBHeaderB_INTRA  (pCodedPicture,
                                                            m_bRawBitplanes,
                                                            MVDiffTablesVLC[m_uiMVTab],
                                                            pCBPCYTable);
                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                for (blk = 0; blk<6; blk++)
                {
                   err = pCompressedMB->WriteBlockDC(pCodedPicture,pDCTableVLCIntra[blk],m_uiQuant,blk);
                   VC1_ENC_CHECK (err)
                   err = pCompressedMB->WriteBlockAC(pCodedPicture,pACTablesSetIntra[blk],&ACEscInfo,blk);
                   VC1_ENC_CHECK (err)
                }
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
                break;
            case VC1_ENC_B_MB_SKIP_F:
            case VC1_ENC_B_MB_SKIP_B:
            case VC1_ENC_B_MB_SKIP_FB:
#ifdef VC1_ME_MB_STATICTICS
                if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_SKIP_F)
                    m_MECurMbStat->MbType = UMC::ME_MbFrwSkipped;
                else if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_SKIP_F)
                    m_MECurMbStat->MbType = UMC::ME_MbBkwSkipped;
                else
                    m_MECurMbStat->MbType = UMC::ME_MbBidirSkipped;
#endif

                err = pCompressedMB->WriteBMB_SKIP_NONDIRECT(  pCodedPicture,
                                                               m_bRawBitplanes,
                                                               2*m_uiBFraction.num < m_uiBFraction.denom);
                VC1_ENC_CHECK (err)
                break;
            case VC1_ENC_B_MB_F:
            case VC1_ENC_B_MB_B:
            case VC1_ENC_B_MB_FB:
#ifdef VC1_ME_MB_STATICTICS
                if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_F)
                    m_MECurMbStat->MbType = UMC::ME_MbFrw;
                else if(pCompressedMB->GetMBType() == VC1_ENC_B_MB_B)
                    m_MECurMbStat->MbType = UMC::ME_MbBkw;
                else
                    m_MECurMbStat->MbType = UMC::ME_MbBidir;
#endif
                err = pCompressedMB->WriteBMB  (pCodedPicture,
                                                m_bRawBitplanes,
                                                m_uiMVTab,
                                                m_uiMVRangeIndex,
                                                pCBPCYTable,
                                                bCalculateVSTransform,
                                                bMVHalf,
                                                pTTMBVLC,
                                                pTTBlkVLC,
                                                pSubPattern4x4VLC,
                                                pACTablesSetInter,
                                                &ACEscInfo,
                                                2*m_uiBFraction.num < m_uiBFraction.denom);
               VC1_ENC_CHECK (err)
                break;
            case VC1_ENC_B_MB_DIRECT:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbDirect;
#endif
               err = pCompressedMB->WriteBMB_DIRECT  (
                            pCodedPicture,
                            m_bRawBitplanes,
                            pCBPCYTable,
                            bCalculateVSTransform,
                            pTTMBVLC,
                            pTTBlkVLC,
                            pSubPattern4x4VLC,
                            pACTablesSetInter,
                            &ACEscInfo);
               VC1_ENC_CHECK (err)
               break;
            case VC1_ENC_B_MB_SKIP_DIRECT:
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->MbType = UMC::ME_MbDirectSkipped;
#endif
                err = pCompressedMB->WriteBMB_SKIP_DIRECT(  pCodedPicture,
                                                            m_bRawBitplanes);
                VC1_ENC_CHECK (err)

                break;
            }
#ifdef VC1_ENC_DEBUG_ON
            pDebug->NextMB();
#endif
#ifdef VC1_ME_MB_STATICTICS
           m_MECurMbStat++;
#endif
        }
    }

    return err;
}

UMC::Status   VC1EncoderPictureSM::WritePPictureHeader(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status     err           =   UMC::UMC_OK;
    bool            bBframes      =   m_pSequenceHeader->IsBFrames();
    int8_t           diff          =   m_uiAltPQuant -  m_uiQuant - 1;

    err = pCodedPicture->MakeBlankSegment(2);
    if (err!= UMC::UMC_OK)
        return err;
    if (m_pSequenceHeader->IsFrameInterpolation())
    {
        err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(m_uiFrameCount,2);
    if (err != UMC::UMC_OK)
        return err;
    if (m_pSequenceHeader->IsRangeRedution())
    {
        err = pCodedPicture->PutBits(m_bRangeRedution,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(frameTypeCodesVLC[bBframes][2*1],frameTypeCodesVLC[bBframes][2*1+1]);
    if (err!= UMC::UMC_OK)
        return err;

    err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuantIndex <= 8)
    {
        err = pCodedPicture->PutBits(m_bHalfQuant, 1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
    {
        err = pCodedPicture->PutBits(m_bUniformQuant,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->IsExtendedMV())
    {
        err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->IsMultiRes())
    {
        err = pCodedPicture->PutBits(m_uiResolution,2);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(MVModeP[m_bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode],MVModeP[m_bIntensity*2 + (m_uiQuant <= 12)][2*m_uiMVMode+1]);
    if (err != UMC::UMC_OK)
       return err;

    if (m_bIntensity)
    {
        err = pCodedPicture->PutBits(m_uiIntensityLumaScale,6);
        if (err != UMC::UMC_OK)
            return err;
        err = pCodedPicture->PutBits(m_uiIntensityLumaShift,6);
        if (err != UMC::UMC_OK)
            return err;
    }
    assert( m_bRawBitplanes == true);
    if (m_uiMVMode == VC1_ENC_MIXED_QUARTER_BICUBIC)
    {
        //raw bitplane for MB type
       err = pCodedPicture->PutBits(0,5);
       if (err != UMC::UMC_OK)
          return err;
    }
    // raw bitplane for skip MB
    err = pCodedPicture->PutBits(0,5);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiMVTab,2);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiCBPTab,2);
    if (err != UMC::UMC_OK)
       return err;

    switch (m_pSequenceHeader->GetDQuant())
    {
    case 2:
        if (diff>=0 && diff<7)
        {
            err = pCodedPicture->PutBits(diff,3);
        }
        else
        {
            err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
        }
        if (err != UMC::UMC_OK)
            return err;
        break;
    case 1:
        err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
        if (err != UMC::UMC_OK)
           return err;
        if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
        {
           err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
           if (err != UMC::UMC_OK)
                return err;
           if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
           {
                if (diff>=0 && diff<7)
                {
                    err = pCodedPicture->PutBits(diff,3);
                }
                else
                {
                    err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                }
                if (err != UMC::UMC_OK)
                    return err;
           }
        }
        break;
    default:
        break;
    }


    if (m_pSequenceHeader->IsVSTransform())
    {
        err = pCodedPicture->PutBits(m_bVSTransform,1);
        if (err != UMC::UMC_OK)
            return err;
        if (m_bVSTransform)
        {
            err = pCodedPicture->PutBits(m_uiTransformType,2);
            if (err != UMC::UMC_OK)
                return err;
        }
    }
    err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
    return err;
}
UMC::Status   VC1EncoderPictureSM::WriteBPictureHeader(VC1EncoderBitStreamSM* pCodedPicture)
{
    UMC::Status     err           =   UMC::UMC_OK;
    int8_t           diff          =   m_uiAltPQuant -  m_uiQuant - 1;

    err = pCodedPicture->MakeBlankSegment(2);
    if (err!= UMC::UMC_OK)
        return err;
   if (m_pSequenceHeader->IsFrameInterpolation())
    {
        err =pCodedPicture->PutBits(m_bFrameInterpolation,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(m_uiFrameCount,2);
    if (err != UMC::UMC_OK)
        return err;
    if (m_pSequenceHeader->IsRangeRedution())
    {
        err = pCodedPicture->PutBits(m_bRangeRedution,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    err = pCodedPicture->PutBits(frameTypeCodesVLC[1][2*3],frameTypeCodesVLC[1][2*3+1]);
    if (err!= UMC::UMC_OK)
        return err;

    err = pCodedPicture->PutBits(BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num],BFractionVLC[m_uiBFraction.denom ][2*m_uiBFraction.num+1]);
    if (err!= UMC::UMC_OK)
        return err;

    err = pCodedPicture->PutBits(m_uiQuantIndex, 5);
    if (err != UMC::UMC_OK)
        return err;

    if (m_uiQuantIndex <= 8)
    {
        err = pCodedPicture->PutBits(m_bHalfQuant, 1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->GetQuantType()== VC1_ENC_QTYPE_EXPL)
    {
        err = pCodedPicture->PutBits(m_bUniformQuant,1);
        if (err != UMC::UMC_OK)
            return err;
    }
    if (m_pSequenceHeader->IsExtendedMV())
    {
        err = pCodedPicture->PutBits(MVRangeCodesVLC[m_uiMVRangeIndex*2],MVRangeCodesVLC[m_uiMVRangeIndex*2+1]);
        if (err != UMC::UMC_OK)
            return err;
    }

    err = pCodedPicture->PutBits(m_uiMVMode == VC1_ENC_1MV_QUARTER_BICUBIC,1);
    if (err != UMC::UMC_OK)
       return err;

    assert( m_bRawBitplanes == true);

    //raw bitplane for DIRECT MB type
    err = pCodedPicture->PutBits(0,5);
    if (err != UMC::UMC_OK)
       return err;

    // raw bitplane for skip MB
    err = pCodedPicture->PutBits(0,5);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiMVTab,2);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiCBPTab,2);
    if (err != UMC::UMC_OK)
       return err;

    switch (m_pSequenceHeader->GetDQuant())
    {
    case 2:
        if (diff>=0 && diff<7)
        {
            err = pCodedPicture->PutBits(diff,3);
        }
        else
        {
            err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
        }
        if (err != UMC::UMC_OK)
            return err;
        break;
    case 1:
        err = pCodedPicture->PutBits( m_QuantMode != VC1_ENC_QUANT_SINGLE, 1);
        if (err != UMC::UMC_OK)
           return err;
        if (m_QuantMode != VC1_ENC_QUANT_SINGLE)
        {
           err =  pCodedPicture->PutBits(QuantProfileTableVLC[2*m_QuantMode], QuantProfileTableVLC[2*m_QuantMode+1]);
           if (err != UMC::UMC_OK)
                return err;
           if (m_QuantMode != VC1_ENC_QUANT_MB_ANY)
           {
                if (diff>=0 && diff<7)
                {
                    err = pCodedPicture->PutBits(diff,3);
                }
                else
                {
                    err = pCodedPicture->PutBits((7<<5)+ m_uiAltPQuant,3+5);
                }
                if (err != UMC::UMC_OK)
                    return err;
           }
        }
        break;
    default:
        break;
    }


    if (m_pSequenceHeader->IsVSTransform())
    {
        err = pCodedPicture->PutBits(m_bVSTransform,1);
        if (err != UMC::UMC_OK)
            return err;
        if (m_bVSTransform)
        {
            err = pCodedPicture->PutBits(m_uiTransformType,2);
            if (err != UMC::UMC_OK)
                return err;
        }
    }
    err = pCodedPicture->PutBits(ACTableCodesVLC[2*m_uiDecTypeAC1],ACTableCodesVLC[2*m_uiDecTypeAC1+1]);
    if (err != UMC::UMC_OK)
       return err;

    err = pCodedPicture->PutBits(m_uiDecTypeDCIntra,1);
    return err;
}


UMC::Status   VC1EncoderPictureSM::WriteMBQuantParameter(VC1EncoderBitStreamSM* pCodedPicture, uint8_t Quant)
{
   UMC::Status err = UMC::UMC_OK;

   if (m_QuantMode == VC1_ENC_QUANT_MB_ANY)
   {
       int16_t diff = Quant - m_uiQuant;
       if (diff < 7 && diff>=0)
       {
            err = pCodedPicture->PutBits(diff,3);
       }
       else
       {
            err = pCodedPicture->PutBits((7<<5)+ Quant,3+5);
       }

   }
   else if (m_QuantMode ==VC1_ENC_QUANT_MB_PAIR)
   {
       err = pCodedPicture->PutBits(m_uiAltPQuant == Quant,1);

   }
   return err;
}

UMC::Status VC1EncoderPictureSM::CheckMEMV_P(UMC::MeParams* MEParams, bool bMVHalf)
{
    UMC::Status UmcSts = UMC::UMC_OK;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t w = m_pSequenceHeader->GetNumMBInRow();

    sCoordinate         MV          = {0,0};

    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            if(MEParams->pSrc->MBs[j + i*w].MbType != UMC::ME_MbIntra)
            {
                MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                if(bMVHalf)
                {
                    if((MV.x>>1)<<1 != MV.x)
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                    if((MV.y >>1)<<1!= MV.y)
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                ////correction MV
                //MV.x  = (bMVHalf)? (MEParams.pSrc->MBs[j + i*w].MVF->x>>1)<<1:MEFrame->MBs[j + i*w].MVF->x;
                //MV.y  = (bMVHalf)? (MEParams.pSrc->MBs[j + i*w].MVF->y>>1)<<1:MEFrame->MBs[j + i*w].MVF->y;

                //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) > (int32_t)(w*VC1_ENC_LUMA_SIZE))
                //{
                //    MV.x = (w - j)*VC1_ENC_LUMA_SIZE + (MV.x & 0x03);
                //}
                //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) > (int32_t)(h*VC1_ENC_LUMA_SIZE))
                //{
                //    MV.y = (h - i)*VC1_ENC_LUMA_SIZE + (MV.y & 0x03);
                //}
                //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < -16)
                //{
                //    MV.x = (1 - (int32_t)j)*VC1_ENC_LUMA_SIZE ;
                //}
                //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < -16)
                //{
                //    MV.y = (1 - (int32_t)i)*VC1_ENC_LUMA_SIZE ;
                //}
                if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }
                if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

            }
        }
    }

    return UmcSts;
}

UMC::Status VC1EncoderPictureSM::CheckMEMV_P_MIXED(UMC::MeParams* MEParams, bool bMVHalf)
{
    UMC::Status UmcSts = UMC::UMC_OK;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t w = m_pSequenceHeader->GetNumMBInRow();
    uint32_t blk = 0;

    sCoordinate         MV          = {0,0};

    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            switch (MEParams->pSrc->MBs[j + i*w].MbType)
            {
            case UMC::ME_MbIntra:
                break;
            case UMC::ME_MbFrw:
            case UMC::ME_MbFrwSkipped:

                MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                if(bMVHalf)
                {
                    if((MV.x>>1)<<1 != MV.x)
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                    if((MV.y >>1)<<1!= MV.y)
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }
                if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_CHROMA_SIZE > MEParams->PicRange.bottom_right.x)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_CHROMA_SIZE > MEParams->PicRange.bottom_right.y)
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                //MV correction
                //MV.x  = (bMVHalf)? (MV.x>>1)<<1:MV.x;
                //MV.y  = (bMVHalf)? (MV.y>>1)<<1:MV.y;

                //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) > (int32_t)(w*VC1_ENC_LUMA_SIZE))
                //{
                //    MV.x = (w - j)*VC1_ENC_LUMA_SIZE + (MV.x & 0x03);
                //}
                //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) > (int32_t)(h*VC1_ENC_LUMA_SIZE))
                //{
                //    MV.y = (h - i)*VC1_ENC_LUMA_SIZE + (MV.y & 0x03);
                //}
                //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < -8)
                //{
                //    MV.x = (1 - (int32_t)j)*VC1_ENC_LUMA_SIZE ;
                //}
                //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < -8)
                //{
                //    MV.y = (1 - (int32_t)i)*VC1_ENC_LUMA_SIZE ;
                //}
                if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }
                if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                {
                    assert(0);
                    UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                }

                break;
            case UMC::ME_MbMixed:
                for (blk = 0; blk<4; blk++)
                {
                    int xShift = (blk & 0x01)<<3;
                    int yShift = (blk>>1)<<3;

                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0][blk].x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0][blk].y;

                   
                    if((MV.x>>2)+(int32_t)(j<<4) + xShift< MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i<<4) + yShift< MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j<<4) + xShift + 8> MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i<<4) + yShift + 8 > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //MV correction
                    //MV.x  = (bMVHalf)? (MV.x>>1)<<1:MV.x;
                    //MV.y  = (bMVHalf)? (MV.y>>1)<<1:MV.y;

                    //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) > (int32_t)(w*VC1_ENC_LUMA_SIZE))
                    //{
                    //    MV.x = (w - j)*VC1_ENC_LUMA_SIZE + (MV.x & 0x03);
                    //}
                    //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) > (int32_t)(h*VC1_ENC_LUMA_SIZE))
                    //{
                    //    MV.y = (h - i)*VC1_ENC_LUMA_SIZE + (MV.y & 0x03);
                    //}
                    //if ((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < -8)
                    //{
                    //    MV.x = (1 - (int32_t)j)*VC1_ENC_LUMA_SIZE ;
                    //}
                    //if ((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < -8)
                    //{
                    //    MV.y = (1 - (int32_t)i)*VC1_ENC_LUMA_SIZE ;
                    //}
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                }
                break;
            default:
                assert(0);
                return UMC::UMC_ERR_FAILED;
            }

        }
    }
return UmcSts;
}
UMC::Status VC1EncoderPictureSM::CheckMEMV_B(UMC::MeParams* MEParams, bool bMVHalf)
{
    UMC::Status UmcSts = UMC::UMC_OK;
    uint32_t i = 0;
    uint32_t j = 0;
    uint32_t h = m_pSequenceHeader->GetNumMBInCol();
    uint32_t w = m_pSequenceHeader->GetNumMBInRow();

    sCoordinate         MV          = {0,0};

    //sCoordinate                 MVMin = {-16*4,-16*4};
    //sCoordinate                 MVMax = {w*16*4,h*16*4};

    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {
            switch (MEParams->pSrc->MBs[j + i*w].MbType)
            {
                case UMC::ME_MbIntra:
                     break;
                case UMC::ME_MbFrw:
                case UMC::ME_MbFrwSkipped:
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                   //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                case UMC::ME_MbBkw:
                case UMC::ME_MbBkwSkipped:
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                   //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                case UMC::ME_MbBidir:
                case UMC::ME_MbBidirSkipped:
                case UMC::ME_MbDirect:
                case UMC::ME_MbDirectSkipped:
                    //forward
                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[0]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[0]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);

                    //backward
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    MV.x  = MEParams->pSrc->MBs[j + i*w].MV[1]->x;
                    MV.y  = MEParams->pSrc->MBs[j + i*w].MV[1]->y;

                    if(bMVHalf)
                    {
                        if((MV.x>>1)<<1 != MV.x)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;

                        if((MV.y >>1)<<1!= MV.y)
                            UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) < MEParams->PicRange.top_left.y )
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.x>>2)+(int32_t)(j*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.x)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    if((MV.y>>2)+(int32_t)(i*VC1_ENC_LUMA_SIZE) + VC1_ENC_LUMA_SIZE > MEParams->PicRange.bottom_right.y)
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    //correction MV
                    //ScalePredict(&MV, j*16*4,i*16*4,MVMin,MVMax);
                    if (MV.x >(MEParams->SearchRange.x<<2) || MV.x <(-(MEParams->SearchRange.x<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }
                    if (MV.y >(MEParams->SearchRange.y<<2) || MV.y <(-(MEParams->SearchRange.y<<2)))
                    {
                        assert(0);
                        UmcSts = UMC::UMC_ERR_INVALID_PARAMS;
                    }

                    break;
                default:
                    assert(0);
                    break;

            }
        }
    }

    return UmcSts;
}
UMC::Status VC1EncoderPictureSM::SetInterpolationParams4MV (   IppVCInterpolateBlock_8u* pY,
                                                                uint8_t* buffer,
                                                                uint32_t w,
                                                                uint32_t h,
                                                                uint8_t **pPlane,
                                                                uint32_t *pStep,
                                                                bool bField)
{
    UMC::Status ret = UMC::UMC_OK;
    uint32_t lumaShift   = (bField)? 1:0;
    
  
    pY[0].pSrc [0]           = pPlane[0];
    pY[0].srcStep            = pStep[0];
    pY[0].pDst[0]            = buffer;
    pY[0].dstStep            = VC1_ENC_LUMA_SIZE;
    pY[0].pointVector.x       = 0;
    pY[0].pointVector.y       = 0;
    pY[0].sizeBlock.height    = 8;
    pY[0].sizeBlock.width     = 8;
    pY[0].sizeFrame.width     = w >> lumaShift;
    pY[0].sizeFrame.height    = h >> lumaShift;

    pY[1].pSrc [0]           = pPlane[0];
    pY[1].srcStep            = pStep[0];
    pY[1].pDst[0]            = buffer + 8;
    pY[1].dstStep            = VC1_ENC_LUMA_SIZE;
    pY[1].pointVector.x       = 0;
    pY[1].pointVector.y       = 0;
    pY[1].sizeBlock.height    = 8;
    pY[1].sizeBlock.width     = 8;
    pY[1].sizeFrame.width     = w >> lumaShift;
    pY[1].sizeFrame.height    = h >> lumaShift;

    pY[2].pSrc [0]           = pPlane[0];
    pY[2].srcStep            = pStep[0];
    pY[2].pDst[0]            = buffer + VC1_ENC_LUMA_SIZE*8;
    pY[2].dstStep            = VC1_ENC_LUMA_SIZE;
    pY[2].pointVector.x       = 0;
    pY[2].pointVector.y       = 0;
    pY[2].sizeBlock.height    = 8;
    pY[2].sizeBlock.width     = 8;
    pY[2].sizeFrame.width     = w >> lumaShift;
    pY[2].sizeFrame.height    = h >> lumaShift;

    pY[3].pSrc [0]           = pPlane[0];
    pY[3].srcStep            = pStep[0];
    pY[3].pDst[0]            = buffer + VC1_ENC_LUMA_SIZE*8 + 8;
    pY[3].dstStep            = VC1_ENC_LUMA_SIZE;
    pY[3].pointVector.x       = 0;
    pY[3].pointVector.y       = 0;
    pY[3].sizeBlock.height    = 8;
    pY[3].sizeBlock.width     = 8;
    pY[3].sizeFrame.width     = w >> lumaShift;
    pY[3].sizeFrame.height    = h >> lumaShift;


    return ret;
}

#ifdef VC1_ME_MB_STATICTICS
void VC1EncoderPictureSM::SetMEStat(UMC::MeMbStat*   stat)
{
    m_MECurMbStat = stat;
}
#endif
}

#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)