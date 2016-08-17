/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//
//
//          ReSize Video Pre\Post Processing
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_RESIZE_VPP_H
#define __MFX_RESIZE_VPP_H

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"

#define VPP_RS_IN_NUM_FRAMES_REQUIRED  (1)
#define VPP_RS_OUT_NUM_FRAMES_REQUIRED (1)

class MFXVideoVPPResize : public FilterVPP{
public:

  static mfxU8 GetInFramesCountExt( void ) { return VPP_RS_IN_NUM_FRAMES_REQUIRED; };
  static mfxU8 GetOutFramesCountExt(void) { return VPP_RS_OUT_NUM_FRAMES_REQUIRED; };

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
  MFXVideoVPPResize(VideoCORE *core, mfxStatus* sts);
  virtual ~MFXVideoVPPResize();

  // VideoVPP
  virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam);

  // VideoBase methods
  virtual mfxStatus Close(void);
  virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out);

  // tuning parameters
  virtual mfxStatus SetParam( mfxExtBuffer* pHint );
  virtual mfxStatus Reset(mfxVideoParam *par);

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

  /* work buffer is required by ippiResizeSqrPixel */
  mfxU8*        m_pWorkBuf;
#endif
};

#endif // __MFX_RESIZE_VPP_H

#endif // MFX_ENABLE_VPP
/* EOF */
