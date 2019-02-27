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

#ifndef _ENCODER_VC1_SEQUENCE_SM_H_
#define _ENCODER_VC1_SEQUENCE_SM_H_

#include "umc_vc1_enc_bitstream.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_video_encoder.h"

namespace UMC_VC1_ENCODER
{

class VC1EncoderSequenceSM
{
private:
    uint32_t                 m_uiPictureWidth;
    uint32_t                 m_uiPictureHeight;

    uint32_t                 m_nMBRow;
    uint32_t                 m_nMBCol;

    uint8_t                  m_uiProfile;         //VC1_ENC_PROFILE_S , VC1_ENC_PROFILE_M
    uint32_t                 m_uiNumberOfFrames;  //Number of frames (0xffffff - if unknown)
    uint8_t                  m_uiFrameRateQ;      // [0, 7]
    uint8_t                  m_uiBitRateQ;        // [0,31]
    bool                   m_bLoopFilter;       // Should be equal to 0 in simple profile
    bool                   m_bMultiRes;         // Multi resolution coding
    bool                   m_bFastUVMC;         // Should be equal to 1 in simple profile
    bool                   m_bExtendedMV;       // Should be equal to 0 in simple profile
    uint8_t                  m_uiDQuant;          // [0] - simple profile,
                                                // [0,1,2] - main profile (if m_bMultiRes then only 0)
    bool                   m_bVSTransform;      // variable-size transform
    bool                   m_bOverlap;
    bool                   m_bSyncMarkers;      // Should be equal to 0 in simple profile
    bool                   m_bRangeRedution;    // Should be equal to 0 in simple profile
    uint32_t                 m_uiMaxBFrames;      // Number of B frames between I,P frames[0,7]
    eQuantType             m_uiQuantazer;       // [0,3] - quantizer specification
    bool                   m_bFrameInterpolation;

    uint8_t                  m_uiLevel;           //VC1_ENC_LEVEL_S, VC1_ENC_LEVEL_M, VC1_ENC_LEVEL_H
    bool                   m_bConstBitRate;
    uint32_t                 m_uiHRDBufferSize;   // buffersize in milliseconds [1,65536]
    uint32_t                 m_uiHRDFrameRate;    // rate: bits per seconds [1,65536]
    uint32_t                 m_uiFrameRate;       // 0xffffffff - if unknown
    uint8_t                  m_uiRoundControl;    // 0,1;


public:
    VC1EncoderSequenceSM():
        m_uiPictureWidth(0),
        m_uiPictureHeight(0),
        m_uiProfile (0),
        m_uiLevel(0),
        m_uiNumberOfFrames(0),
        m_uiFrameRateQ(0),
        m_uiBitRateQ(0),
        m_bLoopFilter(false),
        m_bMultiRes(false),
        m_bFastUVMC(false),
        m_bExtendedMV(false),
        m_uiDQuant(0),
        m_bVSTransform(false),
        m_bOverlap(false),
        m_bSyncMarkers(false),
        m_bRangeRedution(false),
        m_uiMaxBFrames(0),
        m_uiQuantazer(VC1_ENC_QTYPE_UF),
        m_bFrameInterpolation(0),
        m_bConstBitRate (false),
        m_uiHRDBufferSize(0),
        m_uiHRDFrameRate(0),
        m_uiFrameRate(0),
        m_nMBRow(0),
        m_nMBCol(0),
        m_uiRoundControl(0)
        {}


    bool                IsFrameInterpolation()  {return m_bFrameInterpolation;}
    bool                IsRangeRedution()       {return m_bRangeRedution;}
    bool                IsFastUVMC()            {return m_bFastUVMC;}
    bool                IsMultiRes()            {return m_bMultiRes;}
    bool                IsBFrames()             {return m_uiMaxBFrames>0;}
    bool                IsExtendedMV()          {return m_bExtendedMV;}
    bool                IsLoopFilter()          {return m_bLoopFilter;}
    bool                IsVSTransform()         {return m_bVSTransform;}
    bool                IsOverlap()             {return m_bOverlap;}
    uint8_t               GetDQuant()             {return m_uiDQuant;}

    uint32_t              GetPictureWidth()        {return m_uiPictureWidth;}
    uint32_t              GetPictureHeight()       {return m_uiPictureHeight;}
    uint32_t              GetNumMBInRow()          {return m_nMBRow;}
    uint32_t              GetNumMBInCol()          {return m_nMBCol;}

    eQuantType          GetQuantType()          {return m_uiQuantazer;}

    UMC::Status         WriteSeqHeader(VC1EncoderBitStreamSM* pCodedSH);
    UMC::Status         CheckParameters(vm_char* pLastError);
    UMC::Status         Init(UMC::VC1EncoderParams* pParams);
    uint8_t               GetProfile()             {return m_uiProfile;}
    uint8_t               GetLevel()               {return m_uiLevel;}
    void                SetLevel(uint8_t level)    {m_uiLevel = level;}

    void        SetMaxBFrames(uint32_t n)
    {
        assert(n<=7);
        m_uiMaxBFrames = n;

        if(m_uiMaxBFrames >7) m_uiMaxBFrames = 7;

    }


};

}
#endif
#endif // defined (UMC_ENABLE_VC1_VIDEO_ENCODER)