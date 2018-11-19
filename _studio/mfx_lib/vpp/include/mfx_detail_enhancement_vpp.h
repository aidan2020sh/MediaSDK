// Copyright (c) 2010-2018 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_DETAIL_ENHANCEMENT_VPP_H
#define __MFX_DETAIL_ENHANCEMENT_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_DETAIL_ENHANCEMENT_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_DETAIL_ENHANCEMENT_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPDetailEnhancement : public FilterVPP{
public:

    static mfxU8 GetInFramesCountExt( void ) { return VPP_DETAIL_ENHANCEMENT_IN_NUM_FRAMES_REQUIRED; };
    static mfxU8 GetOutFramesCountExt(void) { return VPP_DETAIL_ENHANCEMENT_OUT_NUM_FRAMES_REQUIRED; };

    // this function is used by VPP Pipeline Query to correct application request
    static mfxStatus Query( mfxExtBuffer* pHint );

#if !defined (MFX_ENABLE_HW_ONLY_VPP)

    MFXVideoVPPDetailEnhancement(VideoCORE *core, mfxStatus* sts);
    virtual ~MFXVideoVPPDetailEnhancement();

    // VideoVPP
    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

    // VideoBase methods
    virtual mfxStatus Close(void);
    virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);
    virtual mfxStatus Reset(mfxVideoParam *par);

    // tuning parameters
    virtual mfxStatus SetParam( mfxExtBuffer* pHint );

    virtual mfxU8 GetInFramesCount( void ){ return GetInFramesCountExt(); };
    virtual mfxU8 GetOutFramesCount( void ){ return GetOutFramesCountExt(); };

    // work buffer management
    virtual mfxStatus GetBufferSize( mfxU32* pBufferSize );
    virtual mfxStatus SetBuffer( mfxU8* pBuffer );

    virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
    virtual bool      IsReadyOutput( mfxRequestType requestType );

protected:
    //mfxStatus PassThrough(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

private:
    uint8_t     m_divTable[256];

    int       m_gainFactor;                     //strength of sharpening
    int       m_internalGainFactor;             //real strength of sharpening

    int       m_strongSobel;                    //strong edge threshold
    int       m_weakSobel;                      //no edge threshold

    int       m_strongWeight;                   //strength of sharpening for strong edge
    int       m_regularWeight;                  //strength of sharpening for regular edge
    int       m_weakWeight;                     //strength of sharpening for weak edge

    // internal buffers
    uint8_t*    m_pSobelBuffer;
    int       m_sobelPitch;

    uint8_t*    m_pDiffBuffer;
    int       m_diffPitch;

    uint8_t*    m_pExtBuffer;
    int       m_extPitch;

    // iin case of default params filter can be excluded from pipeline to high performance
    bool      m_isFilterActive;

    mfxStatus DetailFilterCore( uint8_t* pSrc, int srcPitch,
                                uint8_t* pDst, int dstPitch,
                                uint8_t* pSobel, int sobelPitch,
                                uint8_t* pDiff,  int diffPitch,
                                mfxSize size);
#endif
};

#endif // __MFX_DETAIL_ENHANCEMENT_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */