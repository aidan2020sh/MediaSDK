// Copyright (c) 2003-2019 Intel Corporation
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

#include "basic_audio_render.h"
#include "umc_audio_codec.h"

Ipp32u VM_CALLCONVENTION BasicAudioRenderThreadProc(void* vpParam)
{
  VM_ASSERT(NULL != vpParam);
  UMC::BasicAudioRender* pThis =
      reinterpret_cast<UMC::BasicAudioRender*>(vpParam);
  pThis->ThreadProc();
  return 0;
}

UMC::BasicAudioRender::BasicAudioRender():
  m_tStartTime(0),
  m_tStopTime(0),
  m_bStop(false),
  m_bPause(false),
  m_dfNorm(1)
{
  m_tFreq = vm_time_get_frequency();
  m_eventSyncPoint.Init(1, 1);
  m_eventSyncPoint1.Init(1, 0);
  m_dDynamicChannelPTS = -1;
  m_wDynamicChannels   = -1;
}

UMC::BasicAudioRender::~BasicAudioRender()
{
  Close();
}

UMC::Status UMC::BasicAudioRender::Init(MediaReceiverParams* pInit)
{
  Status umcRes = UMC_OK;
  AudioRenderParams* pParams =
      DynamicCast<AudioRenderParams, MediaReceiverParams>(pInit);

  m_RendererIsStopped = false;

  if (NULL == pParams) {
    umcRes = UMC_ERR_NULL_PTR;
  }

  if (UMC_OK == umcRes) {
    MediaBufferParams Params;
    Ipp32s iSamples = 0;
    sAudioStreamInfo* pAudioInfo = &pParams->info;

    m_dfNorm = 1.0 / pAudioInfo->sample_frequency
                   / (pAudioInfo->bitPerSample >> 3)
                   / pAudioInfo->channels;

    m_wInitedChannels   = pAudioInfo->channels;
    m_dwInitedFrequency = pAudioInfo->sample_frequency ;
    switch (pAudioInfo->stream_type) {
      case AC3_AUDIO:
        iSamples = 256*6;
        break;
      case MP1L2_AUDIO:
      case MP1L3_AUDIO:
        iSamples = 1152;
        break;
      case MP2L3_AUDIO:
        iSamples = 576;
        break;
      default: iSamples = 1024;
    }

    Params.m_prefInputBufferSize = //2304;
                                   pAudioInfo->channels *
                                   (pAudioInfo->bitPerSample >> 3) *
                                   iSamples*2;
    Params.m_numberOfFrames = 100;
    Params.m_prefOutputBufferSize = Params.m_prefInputBufferSize;
                                    //2304;//4 * 1024;

    umcRes = m_DataBuffer.Init(&Params);
  }

  if (UMC_OK == umcRes) {
    m_bStop = false;
    umcRes = m_Thread.Create(BasicAudioRenderThreadProc, this);
  }

  return umcRes;
}

void UMC::BasicAudioRender::ThreadProc()
{
  Status umcRes = UMC_OK;
  AudioData data;

  vm_debug_trace(VM_DEBUG_INFO, VM_STRING("BasicAudioRender::ThreadProc: Start\n"));
  while (UMC_OK == umcRes && !m_bStop) {
    if (UMC_OK == umcRes) {
      umcRes = m_DataBuffer.LockOutputBuffer(&data);
    }

    if (UMC_ERR_END_OF_STREAM == umcRes) {
      break;
    } else
    if (UMC_ERR_NOT_ENOUGH_DATA == umcRes) {
      umcRes = UMC_OK;
      vm_time_sleep(100);
      continue;
    }

    if (data.GetTime() == -1) { // first frame
      data.SetTime(0.0);
    }

    Ipp64f dfEndTime = data.GetTime() + m_dfNorm * data.GetDataSize();
    VM_ASSERT(UMC_OK == umcRes || UMC_ERR_TIMEOUT == umcRes || UMC_WRN_REPOSITION_INPROGRESS == umcRes);
    if (UMC_OK == umcRes) {
      data.SetTime(data.GetTime(), dfEndTime);
      umcRes = SendFrame(&data);
      if (umcRes == UMC_ERR_FAILED) {
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("BasicAudioRender::RendererIsStopped\n"));
        umcRes = UMC_OK;
        m_RendererIsStopped = true;
      } else
      if (umcRes != UMC_OK) {
        vm_debug_trace(VM_DEBUG_INFO, VM_STRING("BasicAudioRender::Failed in SendFrame!!!\n"));
        break;
      }
    }

    if (UMC_OK == umcRes) {
      data.SetTime(dfEndTime);
      data.MoveDataPointer((Ipp32s)data.GetDataSize());
      umcRes = m_DataBuffer.UnLockOutputBuffer(&data);
    }

    if (m_RendererIsStopped) {
      m_bStop = true;
      break;
    }

    if (umcRes == UMC_WRN_REPOSITION_INPROGRESS) {
      break;
    }
  }

  if (!m_bStop && (umcRes == UMC_ERR_END_OF_STREAM))
  {
    umcRes = DrainBuffer();
    m_bStop = true;
  }
  vm_debug_trace(VM_DEBUG_INFO, VM_STRING("BasicAudioRender::ThreadProc: Exit\n"));
}

// Audio Reference Clock
Ipp64f UMC::BasicAudioRender::GetTime()
{
  m_tStartTime = vm_time_get_tick();
  Ipp64f dfTime = -1.0;
  if (!m_bStop) {
    dfTime = GetTimeTick();
  } else {
    if (m_Thread.IsValid()) {
      m_Thread.Wait();
    }
  }

  m_tStopTime = vm_time_get_tick();

  // we must return -1 if playback is already stopped, so zero GetDelay()
  // result is required
  if (-1.0 == dfTime) {
    m_tStartTime = m_tStopTime;
  }

  return dfTime >= 0 ? dfTime : -1.0;
}

UMC::Status UMC::BasicAudioRender::GetTimeTick(Ipp64f pts)
{
  Status umcRes = UMC::UMC_ERR_FAILED;

  if((m_dDynamicChannelPTS > 0) && (pts >= m_dDynamicChannelPTS)) {
    AudioRenderParams par;

    memset(&par.info, 0, sizeof(AudioStreamInfo));
    par.pModuleContext = NULL;
    par.info.sample_frequency = m_dwInitedFrequency;
    par.info.channels         = m_wDynamicChannels;
    par.info.bitPerSample     = 16;

    umcRes = SetParams(&par);

    if(UMC_OK == umcRes) {
      m_dDynamicChannelPTS = -1;
    }
  }
  return umcRes;
}

// Estimated value of device latency
Ipp64f UMC::BasicAudioRender::GetDelay()
{
  return (Ipp64f)(Ipp64s)(m_tStopTime - m_tStartTime) / (Ipp64f)(Ipp64s)m_tFreq;
}

UMC::Status
UMC::BasicAudioRender::LockInputBuffer(MediaData *in)
{
  vm_debug_trace(VM_DEBUG_NONE, VM_STRING("BasicAudioRender::LockInputBuffer\n"));
  if (m_RendererIsStopped) {
    return UMC_ERR_FAILED;
  } else
  return m_DataBuffer.LockInputBuffer(in);
}

UMC::Status UMC::BasicAudioRender::UnLockInputBuffer(MediaData *in,
                                                     Status StreamStatus)
{
  Status umcRes = UMC_OK;

  if (UMC_OK == umcRes) {
    umcRes = m_DataBuffer.UnLockInputBuffer(in, StreamStatus);
  }

  AudioData* pData = DynamicCast<AudioData, MediaData>(in);

  if(pData) {
    if(m_wInitedChannels == pData->m_info.channels &&
       m_dwInitedFrequency != pData->m_info.sample_frequency) {
      AudioRenderParams param;
      param.info = pData->m_info;
      if(UMC_OK == SetParams(&param)) {
        m_wInitedChannels   = pData->m_info.channels;
        m_dwInitedFrequency = pData->m_info.sample_frequency;
      }
    } else
    if((m_wInitedChannels != pData->m_info.channels) &&
       (m_dDynamicChannelPTS == -1)) {
      m_dDynamicChannelPTS = in->GetTime();
      m_wDynamicChannels   = pData->m_info.channels;
    }
  }
  return umcRes;
}

UMC::Status UMC::BasicAudioRender::Stop()
{
  m_bStop = true;
  return UMC_OK;
}

UMC::Status UMC::BasicAudioRender::Pause(bool pause)
{
  if (pause)
    m_bPause = true;
  else
    m_bPause = false;
  return UMC_OK;
}

UMC::Status UMC::BasicAudioRender::Close()
{
  if (m_Thread.IsValid()) {
    if (m_bPause)
      m_bStop = true;
      m_Thread.Wait();
  }
  m_DataBuffer.Close();
  return UMC_OK;
}

UMC::Status UMC::BasicAudioRender::Reset()
{
  m_Thread.Wait();
  m_DataBuffer.Reset();
  return UMC_OK;
}

UMC::Status UMC::BasicAudioRender::SetParams(MediaReceiverParams *pMedia,
                                             Ipp32u  /* trickModes */)
{
  Status umcRes = 0;

  AudioRenderParams* pParams = DynamicCast<AudioRenderParams, MediaReceiverParams>(pMedia);
  if (NULL == pParams) {
    umcRes = UMC_ERR_NULL_PTR;
  }

  if (UMC_OK == umcRes) {
    sAudioStreamInfo* info = NULL;
    info = &pParams->info;
    if (info->sample_frequency &&
        info->bitPerSample &&
        info->channels) {
      m_dfNorm = 1.0 / info->sample_frequency
                     / (info->bitPerSample >> 3)
                     / info->channels;
      m_wInitedChannels = info->channels;
      m_dwInitedFrequency = info->sample_frequency;
    } else {
      umcRes = UMC::UMC_ERR_FAILED;
    }
  }
  return umcRes;
}
