/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 Intel Corporation. All Rights Reserved.
//
//
//          VC-1 (VC1) encoder, advance profile
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_ADV_H_
#define _ENCODER_VC1_ADV_H_

#include "umc_vc1_enc_sequence_adv.h"
#include "umc_vc1_enc_picture_adv.h"
#include "umc_vc1_enc_bit_rate_control.h"
#include "umc_vc1_enc_planes.h"
#include "umc_scene_analyzer.h"

#include "umc_me.h"
#include "umc_vme.h"

namespace UMC_VC1_ENCODER
{

#define N_FRAMES    10

class VC1EncoderADV
{
private:

    VC1EncoderBitStreamAdv      *m_pCodedFrame;
    VC1EncoderSequenceADV       *m_SH;
    VC1EncoderPictureADV        *m_pCurrPicture;

    VC1EncoderMBs               *m_pMBs;
    UMC::MemID                  m_MBsID;
    Ipp8u*                      m_MBsBuffer;

    VC1EncoderCodedMB           *m_pCodedMB;

    ///////////////////
    VC1BitRateControl          *m_pBitRateControl;
    Ipp8u                      *m_pBRCBuffer;
    UMC::MemID                  m_BRCID;;

#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
    UMC::SceneAnalyzerBase *m_pSceneAnalyzer;
#endif
    //////////////////

    Ipp32u                   m_iFrameNumber;

    Ipp32u                   m_uiGOPLength;
    Ipp32u                   m_uiBFrmLength;

    Ipp16s*                  m_pSavedMV;
    Ipp8u*                   m_pRefType;

    vm_char                  m_cLastError[VC1_ENC_STR_LEN];

    Ipp32s                   m_iFrameCount;


    bool                     m_bSequenceHeader;
    Ipp8u                    m_uiPictuteQuantIndex;
    bool                     m_bHalfQuant;

    StoredFrames*            m_pStoredFrames;
    UMC::MemID               m_StoredFramesID;
    Ipp8u*                   m_pStoredFramesBuffer;
    GOP*                     m_pGOP;
    WaitingList*             m_pWaitingList;

    //motion estimation
    UMC::MeBase*             m_pME;
    Ipp32s                   m_MESearchSpeed;
    Ipp8u*                   m_pMEBuffer;
    UMC::MemID               m_MEID;

    //MEMORY ALLOCATOR
    UMC::MemoryAllocator *m_pMemoryAllocator;
    bool                  m_bOwnAllocator;
    UMC::MemID            m_SavedMVID;
    UMC::MemID            m_RefTypeID;

    bool                  m_bFrameRecoding;

    bool                   m_bMixedMV;
    Ipp32u                 m_uiOrigFramesUsingFlag;

    Frame*                 m_pPlane;
    Frame*                 m_pForwardMEPlane;
    Frame*                 m_pBackwardMEPlane;
    Frame*                 m_pRaisedPlane;
    Frame*                 m_pForwardPlane;
    Frame*                 m_pBackwardPlane;

    eReferenceFieldType    m_uiReferenceFieldType;
    Ipp8u                  m_nReferenceFrameDist;

    UMC::MeFrame*          m_MeFrame[32];
    MeIndex                m_MeIndex;

    //InitPictureParams      m_InitPicParam;
    //VLCTablesIndex         m_VLCIndex;

    Ipp32u                 m_LastQuant;

    bool                   m_bUseMeFeedback;
    bool                   m_bUseUpdateMeFeedback;
    bool                   m_bUseFastMeFeedback;
    bool                   m_bFastUVMC;
    Ipp32u                 m_uiNumSlices;
    bool                   m_bSelectVLCTables;
    bool                   m_bChangeInterpPixelType;
    bool                   m_bUseTreillisQuantization;
    Ipp32u                 m_uiOverlapSmoothing; 
    bool                   m_bIntensityCompensation;

    pIntensityCompChroma   IntensityCompChroma;
protected:

public:

    VC1EncoderADV():
        m_pCodedFrame(0),
        m_pSavedMV(0),
        m_SH(0),
        m_pRefType(0),
        m_pCurrPicture(0),
        m_bSequenceHeader(false),
        m_uiPictuteQuantIndex(0),
        m_bHalfQuant(false),
        m_iFrameCount(0),
        m_uiGOPLength(0),
        m_uiBFrmLength(0),
        m_pMBs(0),
        m_pBitRateControl(NULL),
        m_pBRCBuffer(NULL),
        m_BRCID((UMC::MemID)-1),
#ifdef UMC_ENABLE_UMC_SCENE_ANALYZER
        m_pSceneAnalyzer(NULL),
#endif
        m_pCodedMB(0),
        m_pME(NULL),
        m_MEID((UMC::MemID)-1),
        m_pStoredFrames(0),
        m_pStoredFramesBuffer(NULL),
        m_StoredFramesID((UMC::MemID)-1),
        m_pGOP(0),
        m_pWaitingList(0),
        m_pMemoryAllocator(NULL),
        m_bOwnAllocator(false),
        m_SavedMVID((UMC::MemID)-1),
        m_pMEBuffer(NULL),
        m_RefTypeID((UMC::MemID)-1),
        m_MESearchSpeed(66),
        m_bFrameRecoding(false),
        m_bMixedMV(false),
        m_uiOrigFramesUsingFlag(0),
        m_uiReferenceFieldType(VC1_ENC_REF_FIELD_FIRST),
        m_nReferenceFrameDist(0),
        m_LastQuant(1),
        m_MBsID((UMC::MemID)-1),
        m_MBsBuffer(NULL),
        m_bUseMeFeedback(true),
        m_bUseUpdateMeFeedback(false),
        m_bUseFastMeFeedback(false),
        m_bFastUVMC(false),
        m_bSelectVLCTables(0),
        m_bChangeInterpPixelType(0),
        m_bUseTreillisQuantization(0),
        m_uiNumSlices(0),
        m_uiOverlapSmoothing (0),
        m_bIntensityCompensation(false)
    {
        IntensityCompChroma = IntensityCompChromaYV12;

        for(Ipp32u i = 0; i < 32; i++)
            m_MeFrame[i] = NULL;

        m_MeIndex.MeCurrIndex = 0;
        m_MeIndex.MeRefFIndex = 0;
        m_MeIndex.MeRefBIndex = 0;
    }
    ~VC1EncoderADV()
    {
        Close();
    }

    UMC::Status     Init(UMC::VC1EncoderParams* pParams);
    UMC::Status     Close();
    UMC::Status     Reset();
    void SetMemoryAllocator(UMC::MemoryAllocator *pMemoryAllocator, bool bOwnAllocator);

    UMC::Status     GetInfo(UMC::VC1EncoderParams* pInfo);
    UMC::Status     GetFrame(UMC::MediaData *in, UMC::MediaData *out);

    UMC::Status     WriteFrame(ePType InputPictureType, bool bSecondField, Ipp32u CodedSize);

private:
    //needed for gathering ME statistics
    UMC::Status     SetMEParams(UMC::MeParams* MEParams, InitPictureParams *pPicParams, Ipp8u doubleQuant, bool Uniform, bool FieldPicture, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_I(UMC::MeParams* MEParams,  Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_P(UMC::MeParams* MEParams,  Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_B(UMC::MeParams* MEParams, sFraction* pBFraction, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_PMixed(UMC::MeParams* MEParams,  Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_I_Field(UMC::MeParams* MEParams, Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_P_Field(UMC::MeParams* MEParams, bool bTopFieldFirst, bool bSecondField, bool bMixed=false,  Ipp32s firstRow=0, Ipp32s nRows=0);
    UMC::Status     SetMEParams_B_Field(UMC::MeParams* MEParams, bool bTopFieldFirst, bool bSecond, bool bMixed ,sFraction* pBFraction, Ipp32s firstRow=0, Ipp32s nRows=0);

    UMC::Status     SetGOPParams(Ipp32u GOPLen, Ipp32u BFrames)
    {
        m_uiBFrmLength = BFrames;
        m_uiGOPLength = GOPLen;
        return UMC::UMC_OK;

    }
    ePType          GetPictureType()
    {
        Ipp32s      nFrameInGOP        =  (m_iFrameCount++) % m_uiGOPLength;

         if (nFrameInGOP)
        {
            if ( nFrameInGOP %(m_uiBFrmLength+1)==0)
                return VC1_ENC_P_FRAME;
            else
                return VC1_ENC_B_FRAME;
        }
        return VC1_ENC_I_FRAME;
    }

    void        ResetPlanes()
    {
       m_pPlane                 = 0;
       m_pForwardMEPlane        = 0;
       m_pBackwardMEPlane       = 0;

       m_pRaisedPlane                 = 0;
       m_pForwardPlane                = 0;
       m_pBackwardPlane               = 0;
    }
    UMC::Status SetInitPictureParams(InitPictureParams *pParams, ePType PicType, bool bSecond);
    UMC::Status MEPictureParamsRefine(UMC::MeParams* MEParams,InitPictureParams* pInitPicParam);

};

}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
