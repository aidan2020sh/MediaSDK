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

#ifndef _ENCODER_VC1_SEQUENCE_ADV_H_
#define _ENCODER_VC1_SEQUENCE_ADV_H_

#include "umc_vc1_enc_bitstream.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_video_encoder.h"

namespace UMC_VC1_ENCODER
{

enum
{
    VC1_ENC_COND_OVERLAP_NO   = 0,
    VC1_ENC_COND_OVERLAP_SOME = 1,
    VC1_ENC_COND_OVERLAP_ALL  = 2,
};

class VC1EncoderSequenceADV
{
private:
    uint32_t                 m_uiPictureWidth;
    uint32_t                 m_uiPictureHeight;

    bool                   m_bFrameInterpolation;

    uint8_t                  m_uiFrameRateQ;      // [0, 7]
    uint8_t                  m_uiBitRateQ;        // [0,31]
    bool                   m_bLoopFilter;       // Should be equal to 0 in simple profile
    bool                   m_bFastUVMC;         // Should be equal to 1 in simple profile
    bool                   m_bExtendedMV;       // Should be equal to 0 in simple profile
    uint8_t                  m_uiDQuant;          // [0] - simple profile,
                                                // [0,1,2] - main profile (if m_bMultiRes then only 0)
    bool                   m_bVSTransform;      // variable-size transform
    bool                   m_bOverlap;

    eQuantType             m_uiQuantazer;       // [0,3] - quantizer specification


    uint8_t                  m_uiLevel;           //VC1_ENC_LEVEL_S, VC1_ENC_LEVEL_M, VC1_ENC_LEVEL_H
    bool                   m_bConstBitRate;
    uint32_t                 m_uiHRDBufferSize;   // buffersize in milliseconds [1,65536]
    uint32_t                 m_uiHRDFrameRate;    // rate: bits per seconds [1,65536]
    uint32_t                 m_uiFrameRate;       // 0xffffffff - if unknown

    //-------------------- for advance profile ------------------------------------------------------------

    bool                    m_bClosedEnryPoint ;
    bool                    m_bBrokenLink      ; // if !m_bClosedEnryPoint -> true or false
    bool                    m_bPanScan         ;
    bool                    m_bReferenceFrameDistance;
    bool                    m_bExtendedDMV     ;
    bool                    m_bSizeInEntryPoint;
    int8_t                   m_iRangeMapY       ;
    int8_t                   m_iRangeMapUV      ;

    bool                    m_bPullDown         ;
    bool                    m_bInterlace        ;
    bool                    m_bFrameCounter     ;
    bool                    m_bDisplayExtention ;
    bool                    m_bHRDParams        ;
    bool                    m_bPostProc         ;

public:
    VC1EncoderSequenceADV():
        m_uiPictureWidth(0),
        m_uiPictureHeight(0),
        m_uiLevel(0),
        //m_uiNumberOfFrames(0),
        m_uiFrameRateQ(0),
        m_uiBitRateQ(0),
        m_bLoopFilter(false),
        //m_bMultiRes(false),
        m_bFastUVMC(false),
        m_bExtendedMV(false),
        m_uiDQuant(0),
        m_bVSTransform(false),
        m_bOverlap(false),
        //m_bSyncMarkers(false),
        //m_bRangeRedution(false),
        //m_uiMaxBFrames(0),
        m_uiQuantazer(VC1_ENC_QTYPE_UF),
        m_bFrameInterpolation(0),
        m_bConstBitRate (false),
        m_uiHRDBufferSize(0),
        m_uiHRDFrameRate(0),
        m_uiFrameRate(0),
        m_bClosedEnryPoint(true),
        m_bBrokenLink(false),
        m_bPanScan(false),
        m_bReferenceFrameDistance(false),
        m_bExtendedDMV(false),
        m_bSizeInEntryPoint(false),
        m_iRangeMapY(-1),
        m_iRangeMapUV(-1) ,
        m_bPullDown(false),
        m_bInterlace(false),
        m_bFrameCounter(false),
        m_bDisplayExtention(false),
        m_bHRDParams(false),
        m_bPostProc (false)

        {}

     bool                IsFrameInterpolation()   {return m_bFrameInterpolation;}
     bool                IsFastUVMC()             {return m_bFastUVMC;}
     bool                IsExtendedMV()           {return m_bExtendedMV;}
     bool                IsLoopFilter()           {return m_bLoopFilter;}
     bool                IsVSTransform()          {return m_bVSTransform;}
     bool                IsOverlap()              {return m_bOverlap;}
     bool                IsInterlace()            {return m_bInterlace;}
     bool                IsPullDown()             {return m_bPullDown;}
     bool                IsPostProc()             {return m_bPostProc;}
     bool                IsFrameCounter()         {return m_bFrameCounter;}
     bool                IsPanScan()              {return m_bPanScan;}
     bool                IsReferenceFrameDistance(){return m_bReferenceFrameDistance;}
     uint8_t               GetDQuant()              {return m_uiDQuant;}
     bool                IsExtendedDMV()          {return m_bExtendedDMV;}

     uint32_t              GetPictureWidth()        {return m_uiPictureWidth;}
     uint32_t              GetPictureHeight()       {return m_uiPictureHeight;}

     eQuantType          GetQuantType()           {return m_uiQuantazer;}

    UMC::Status         WriteSeqHeader(VC1EncoderBitStreamAdv* pCodedSH);
    UMC::Status         CheckParameters(vm_char* pLastError);
    UMC::Status         Init(UMC::VC1EncoderParams* pParams);
    UMC::Status         WriteEntryPoint(VC1EncoderBitStreamAdv* pCodedSH);
    uint8_t               GetProfile()             {return VC1_ENC_PROFILE_A;}
    uint8_t               GetLevel()               {return m_uiLevel;}
    void                SetLevel(uint8_t level)    {m_uiLevel = level;}
    inline uint8_t        GetRefDist(uint8_t  refDist)
    {
        if (m_bReferenceFrameDistance == 0)
            return 0;
        return (refDist>16)? 16: refDist;   
    }

};

}
#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)