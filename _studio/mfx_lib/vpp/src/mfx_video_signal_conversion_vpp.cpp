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

#if defined (MFX_ENABLE_VPP) && !defined(MFX_ENABLE_HW_ONLY_VPP)

#include "mfx_video_signal_conversion_vpp.h"

MFXVideoVPPVideoSignalConversion::MFXVideoVPPVideoSignalConversion(VideoCORE *core, mfxStatus* sts) : FilterVPP()
{

  if( core ){
    // something
  }

  VPP_CLEAN;

  *sts = MFX_ERR_NONE;

  return;

} // MFXVideoVPPVideoSignalConversion::MFXVideoVPPVideoSignalConversion(...)

MFXVideoVPPVideoSignalConversion::~MFXVideoVPPVideoSignalConversion(void)
{
  Close();

  return;

} // MFXVideoVPPVideoSignalConversion::~MFXVideoVPPVideoSignalConversion(void)

mfxStatus MFXVideoVPPVideoSignalConversion::SetParam( mfxExtBuffer* pHint )
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  if( pHint ){
    // mfxSts = something
  }

  return mfxSts;

} // mfxStatus MFXVideoVPPVideoSignalConversion::SetParam( mfxExtBuffer* pHint )

mfxStatus MFXVideoVPPVideoSignalConversion::Reset(mfxVideoParam *par)
{
  MFX_CHECK_NULL_PTR1( par );

  VPP_CHECK_NOT_INITIALIZED;

  m_errPrtctState.In  = par->vpp.In;
  m_errPrtctState.Out = par->vpp.Out;

  VPP_RESET;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPVideoSignalConversion::Reset(mfxVideoParam *par)

mfxStatus MFXVideoVPPVideoSignalConversion::RunFrameVPP(mfxFrameSurface1 *in,
                                                        mfxFrameSurface1 *out,
                                                        InternalParam* pParam)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  VPP_CHECK_NOT_INITIALIZED;

  if( NULL == in )
  {
    return MFX_ERR_MORE_DATA;
  }
  if( NULL == out )
  {
    return MFX_ERR_NULL_PTR;
  }
  if( NULL == pParam )
  {
      return MFX_ERR_NULL_PTR;
  }

  mfxSts = MFX_ERR_UNDEFINED_BEHAVIOR; //not implemented yet

  return mfxSts;

} // mfxStatus MFXVideoVPPVideoSignalConversion::RunFrameVPP(...)

mfxStatus MFXVideoVPPVideoSignalConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)
{
  mfxI32    srcW   = 0, srcH = 0;//, srcPitch = 0;
  mfxI32    dstW   = 0, dstH = 0;//, dstPitch = 0;

  MFX_CHECK_NULL_PTR2( In, Out );

  VPP_CHECK_MULTIPLE_INIT;

  /* IN */
  VPP_GET_REAL_WIDTH(In, srcW);
  VPP_GET_REAL_HEIGHT(In, srcH);

  /* OUT */
  VPP_GET_REAL_WIDTH(Out, dstW);
  VPP_GET_REAL_HEIGHT(Out, dstH);

  /* robustness check */
  if( srcW != dstW || srcH != dstH ){
    return MFX_ERR_INVALID_VIDEO_PARAM;
  }

  /* save init params to prevent core crash */
  m_errPrtctState.In  = *In;
  m_errPrtctState.Out = *Out;

  VPP_INIT_SUCCESSFUL;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPVideoSignalConversion::Init(mfxFrameInfo* In, mfxFrameInfo* Out)

mfxStatus MFXVideoVPPVideoSignalConversion::Close(void)
{
  mfxStatus mfxSts = MFX_ERR_NONE;

  VPP_CHECK_NOT_INITIALIZED;

  VPP_CLEAN;

  return mfxSts;

} // mfxStatus MFXVideoVPPVideoSignalConversion::Close(void)


// work buffer management - nothing for VSC filter
mfxStatus MFXVideoVPPVideoSignalConversion::GetBufferSize( mfxU32* pBufferSize )
{
  VPP_CHECK_NOT_INITIALIZED;

  MFX_CHECK_NULL_PTR1(pBufferSize);

  *pBufferSize = 0;

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPVideoSignalConversion::GetBufferSize( mfxU32* pBufferSize )

mfxStatus MFXVideoVPPVideoSignalConversion::SetBuffer( mfxU8* pBuffer )
{
  mfxU32 bufSize = 0;
  mfxStatus sts;

  VPP_CHECK_NOT_INITIALIZED;

  sts = GetBufferSize( &bufSize );
  MFX_CHECK_STS( sts );

  MFX_CHECK_NULL_PTR1(pBuffer);

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPVideoSignalConversion::SetBuffer( mfxU8* pBuffer )

// function is called from sync part of VPP only
mfxStatus MFXVideoVPPVideoSignalConversion::CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out )
{
  if( NULL == in )
  {
    return MFX_ERR_MORE_DATA;
  }
  else if( NULL == out )
  {
    return MFX_ERR_NULL_PTR;
  }

  PassThrough(in, out);

  return MFX_ERR_NONE;

} // mfxStatus MFXVideoVPPVideoSignalConversion::CheckProduceOutput(...)

bool MFXVideoVPPVideoSignalConversion::IsReadyOutput( mfxRequestType requestType )
{
    if( MFX_REQUEST_FROM_VPP_CHECK == requestType )
    {
        // VSC is simple algorithm and need IN to produce OUT
    }

    return false;

} // bool MFXVideoVPPVideoSignalConversion::IsReadyOutput( mfxRequestType requestType )


#endif // MFX_ENABLE_VPP
/* EOF */