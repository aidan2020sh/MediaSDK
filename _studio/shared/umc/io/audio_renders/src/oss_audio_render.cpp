// Copyright (c) 2003-2018 Intel Corporation
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

#ifdef UMC_ENABLE_OSS_AUDIO_RENDER

#include "oss_audio_render.h"
#include "vm_time.h"

UMC::OSSAudioRender::OSSAudioRender():
    m_tickStartTime(0),
    m_tickPrePauseTime(0),
    m_dfStartPTS(-1.0)
{}

UMC::Status UMC::OSSAudioRender::Init(MediaReceiverParams* pInit)
{
    Status umcRes = UMC_OK;
    AudioRenderParams* pParams =
        DynamicCast<AudioRenderParams, MediaReceiverParams>(pInit);
    if (NULL == pParams)
    {   umcRes = UMC_ERR_NULL_PTR;  }

    m_tickStartTime = 0;
    m_tickPrePauseTime = 0;
    m_dfStartPTS = -1.0;

    if (UMC_OK == umcRes)
    {
        umcRes = m_Dev.Init(pParams->info.bitPerSample,
                            pParams->info.channels,
                            pParams->info.sample_frequency);
    }

    if (UMC_OK == umcRes)
    {   umcRes = BasicAudioRender::Init(pInit); }

    return umcRes;
}

UMC::Status UMC::OSSAudioRender::SendFrame(MediaData* in)
{
    Status umcRes = UMC_OK;

    if (UMC_OK == umcRes && (NULL == in->GetDataPointer()))
    {    umcRes = UMC_ERR_NULL_PTR;    }

    if (-1.0 == m_dfStartPTS)
    {   m_dfStartPTS = in->GetTime();   }

    if (UMC_OK == umcRes)
    {
        if (0 == m_tickStartTime)
        {   m_tickStartTime = vm_time_get_tick();   }
        umcRes = m_Dev.RenderData((Ipp8u*)in->GetDataPointer(), in->GetDataSize());
    }
    return umcRes;
}

UMC::Status UMC::OSSAudioRender::Pause(bool pause)
{
    Status umcRes = UMC_OK;
    if (pause)
    {   umcRes = m_Dev.Post();  }

    m_tickPrePauseTime += vm_time_get_tick() - m_tickStartTime;
    m_tickStartTime = 0;
    return umcRes;
}

Ipp32f UMC::OSSAudioRender::SetVolume(Ipp32f volume)
{
    return -1;
}

Ipp32f UMC::OSSAudioRender::GetVolume()
{
    return -1;
}

UMC::Status UMC::OSSAudioRender::Close()
{
    m_Dev.Reset();
    m_dfStartPTS = -1.0;
    return BasicAudioRender::Close();
}

UMC::Status UMC::OSSAudioRender::Reset()
{
    Status umcRes = m_Dev.Reset();
    m_tickStartTime = 0;
    m_tickPrePauseTime = 0;
    m_dfStartPTS = -1.0;
    return BasicAudioRender::Reset();
}

Ipp64f UMC::OSSAudioRender::GetTimeTick()
{
    Ipp64f dfRes = -1;
    Ipp32u uiBytesPlayed = 0;

    if (0 != m_tickStartTime)
    {
        dfRes = m_dfStartPTS +
            ((Ipp64f)(m_tickPrePauseTime +
            vm_time_get_tick() - m_tickStartTime)) / vm_time_get_frequency();
    }
    else
    {
        dfRes = m_dfStartPTS + ((Ipp64f)m_tickPrePauseTime) / vm_time_get_frequency();
    }
    Status umcRes = BasicAudioRender::GetTimeTick(dfRes);
    return dfRes;
}

UMC::Status UMC::OSSAudioRender::SetParams(MediaReceiverParams *pMedia,
                                           Ipp32u  trickModes)
{
    UMC::Status umcRes = UMC::UMC_OK;
    AudioRenderParams* pParams =
        DynamicCast<AudioRenderParams, MediaReceiverParams>(pMedia);
    if (NULL == pParams)
    {   return UMC_ERR_NULL_PTR;  }

    if (UMC_OK == umcRes)
    {
        sAudioStreamInfo* info = NULL;
        info = &pParams->info;

        if (info->channels != m_wInitedChannels)
        {
            m_Dev.SetNumOfChannels(info->channels);
            m_wInitedChannels = info->channels;
        }
        if (info->sample_frequency != m_dwInitedFrequency)
        {
            m_Dev.SetFreq(info->sample_frequency);
            m_dwInitedFrequency = info->sample_frequency;
        }
    }
    else
    {
         umcRes = UMC_ERR_FAILED;
    }
    return umcRes;
}

#endif // UMC_ENABLE_OSS_AUDIO_RENDER

