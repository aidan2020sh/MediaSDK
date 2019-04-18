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

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_RANGE_MAP_VC1_VPP_H
#define __MFX_RANGE_MAP_VC1_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_RM_VC1_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_RM_VC1_OUT_NUM_FRAMES_REQUIRED (1)

#define VPP_RM_VC1_DEFAULT_VALUE           (0)

class MFXVideoVPPRangeMapVC1 : public FilterVPP{
public:
  virtual mfxU8 GetInFramesCount( void ){ return VPP_RM_VC1_IN_NUM_FRAMES_REQUIRED; };
  virtual mfxU8 GetOutFramesCount( void ){ return VPP_RM_VC1_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPRangeMapVC1(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPRangeMapVC1();

  // VideoVPP
  virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

  // VideoBase methods
  virtual mfxStatus Close(void);
  virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);

  // tuning parameters
  virtual mfxStatus SetParam( mfxExtBuffer* pHint );
  virtual mfxStatus Reset(mfxVideoParam *par);

  // work buffer management
  virtual mfxStatus GetBufferSize( mfxU32* pBufferSize );
  virtual mfxStatus SetBuffer( mfxU8* pBuffer );

  virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );
  virtual bool IsReadyOutput( mfxRequestType requestType );

protected:
   mfxStatus PassThrough(mfxFrameSurface1* /*in*/, mfxFrameSurface1* /*out*/) { return MFX_ERR_UNKNOWN; };

private:

  mfxStatus  GetMapCoeffs( void );

  mfxHDL memID;

  mfxU8 RangeMap;

  mfxU8 RANGE_MAPY_FLAG;
  mfxU8 RANGE_MAPY;

  mfxU8 RANGE_MAPUV;
  mfxU8 RANGE_MAPUV_FLAG;
#endif
};

#endif // __MFX_RANGE_MAP_VC1_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */