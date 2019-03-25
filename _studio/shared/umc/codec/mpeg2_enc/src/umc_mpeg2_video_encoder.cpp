// Copyright (c) 2002-2018 Intel Corporation
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
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

//#include <vm_debug.h>
//#include <crtdbg.h>

#include "vm_strings.h"
#include "umc_video_data.h"
#include "umc_video_encoder.h"
#include "umc_mpeg2_video_encoder.h"
#include "umc_mpeg2_enc.h"
#include "umc_video_brc.h"

//#include "umc_mpeg2_RTP.h"

using namespace UMC;

//////////////////////////////////////////////////////////////////////
// class MPEG2VideoEncoder: public VideoEncoder implementation
// Just calls MPEG2VideoEncoderBase

//constructor
MPEG2VideoEncoder::MPEG2VideoEncoder()
    : m_pBrc()
{
  vm_debug_trace(VM_DEBUG_INFO, VM_STRING("MPEG2VideoEncoder::MPEG2VideoEncoder"));
  encoder = new MPEG2VideoEncoderBase;
}

//destructor
MPEG2VideoEncoder::~MPEG2VideoEncoder()
{
  vm_debug_trace(VM_DEBUG_INFO, VM_STRING("MPEG2VideoEncoder::~MPEG2VideoEncoder"));
  delete ((MPEG2VideoEncoderBase*)encoder);
}

// Initialize codec with specified parameter(s)
Status MPEG2VideoEncoder::Init(BaseCodecParams *init)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->Init(init);
  return ret;
}

// Compress (decompress) next frame
Status MPEG2VideoEncoder::GetFrame(MediaData *in, MediaData *out)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->GetFrame(in, out);
  return ret;
}

// Compress (decompress) next frame
Status MPEG2VideoEncoderBase::GetFrame(MediaData *in, MediaData *out)
{
  VideoData *vin;
  Ipp32s user_data_size;
  size_t data_size;
  Status ret;

  if (out == NULL) {
    return UMC_ERR_NULL_PTR;
  }

  vin = DynamicCast<VideoData> (in);
  if (in != NULL) {
    if (vin == NULL) {
      m_UserData = *in;
      return UMC_OK;
    }

    user_data_size = (Ipp32s)m_UserData.GetDataSize();
    if(user_data_size > 0) {
      setUserData((Ipp8u*)m_UserData.GetDataPointer(), user_data_size);
    }
  }
  data_size = out->GetDataSize();

  //  VM_ASSERT(_CrtCheckMemory());
  ret = TakeNextFrame(vin);
  if(ret == UMC_OK) {
    LockBuffers();
    PrepareFrame();

    // move to some above functions or new
    out_pointer = (Ipp8u*)out->GetDataPointer() + out->GetDataSize();
    output_buffer_size = (Ipp32s)((Ipp8u*)out->GetBufferPointer() + out->GetBufferSize() - out_pointer);

    ret = EncodeFrameReordered();

    out->SetDataSize(out->GetDataSize() + mEncodedSize);
    out->SetFrameType(picture_coding_type == MPEG2_B_PICTURE ? B_PICTURE
      : (picture_coding_type == MPEG2_P_PICTURE ? P_PICTURE : I_PICTURE));

    UnlockBuffers();
  }
  ComputeTS(vin, out);
  FrameDone(vin);
  //  VM_ASSERT(_CrtCheckMemory());

  if(ret == UMC_OK && m_UserData.GetDataSize() > 0) {
    setUserData(0, 0);
    m_UserData.SetDataSize(0);
  }

  if(data_size == out->GetDataSize() && in == 0)
    return UMC_ERR_NOT_ENOUGH_DATA;

  if (vin != NULL) {
    vin->SetDataSize(0);
  }

#ifndef UMC_RESTRICTED_CODE
  //struct RTPInfo rtp;
  //getRTPInfo(pdata, encoded_size, &rtp);
#endif // UMC_RESTRICTED_CODE

  return ret;
}

// THIS METHOD IS TO BE DELETED!
Status MPEG2VideoEncoder::RepeatLastFrame(Ipp64f PTS, MediaData *out)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->RepeatLastFrame(PTS, out);
  return ret;
}

// THIS METHOD IS TO BE DELETED!
Status MPEG2VideoEncoderBase::RepeatLastFrame(Ipp64f /*PTS*/, MediaData * /*out*/)
{
  return UMC_ERR_NOT_IMPLEMENTED;
}

// THIS METHOD IS TO BE DELETED!
Status MPEG2VideoEncoder::GetNextYUVBuffer(VideoData *data)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->GetNextYUVBuffer(data);
  return ret;
}

// THIS METHOD IS TO BE DELETED!
Status MPEG2VideoEncoderBase::GetNextYUVBuffer(VideoData *data)
{
  VideoData* next = GetNextYUVPointer();
  if(next == NULL)
    return UMC_ERR_FAILED;
  *data = *next;
  return UMC_OK;
}

// Get codec working (initialization) parameter(s)
Status MPEG2VideoEncoder::GetInfo(BaseCodecParams *info)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->GetInfo(info);
  return ret;
}

// Close all codec resources
Status MPEG2VideoEncoder::Close()
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->Close();
  return ret;
}

Status MPEG2VideoEncoder::Reset()
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->Reset();
  return ret;
}

Status MPEG2VideoEncoderBase::Reset()
{
  return SetParams(0); // it made so
}

Status MPEG2VideoEncoder::SetParams(BaseCodecParams* params)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->SetParams(params);
  return ret;
}

Status MPEG2VideoEncoderBase::SetParams(BaseCodecParams* params)
{
  Status ret;
  ret = Init(params);
  return ret;
}

Status MPEG2VideoEncoder::SetBitRate(Ipp32s BitRate)
{
  Status ret;
  if(encoder == 0)
    return UMC_ERR_NOT_INITIALIZED;
  ret = ((MPEG2VideoEncoderBase*)encoder)->SetBitRate(BitRate);
  vm_debug_trace_s(VM_DEBUG_INFO, "SetBitRate()");
  vm_debug_trace_i(VM_DEBUG_INFO, BitRate);
  return ret;
}

Status MPEG2VideoEncoderBase::SetBitRate(Ipp32s BitRate)
{
  InitRateControl(BitRate);
  return UMC_OK;
}

namespace UMC
{
VideoEncoder* CreateMPEG2Encoder()
{
  VideoEncoder* ptr = new MPEG2VideoEncoder;
  return ptr;
}
}

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER