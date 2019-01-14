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

#include "null_audio_render.h"

UMC::NULLAudioRenderParams::NULLAudioRenderParams(Ipp64f m_PTS)
{
    m_InitPTS = m_PTS;
}

UMC::NULLAudioRender::NULLAudioRender(Ipp64f dfDelay,
                                      UMC_FFMODE_MULTIPLIER mult):
    m_Freq(0),
    m_LastStartTime(0),
    m_PrePauseTime(0),
    m_bPaused(true),
    m_uiFFModeMult(1)
{
    m_Freq = vm_time_get_frequency();

    m_dInitialPTS = dfDelay;

    switch(mult)
    {
    case UMC_1X_FFMODE:
        m_uiFFModeMult  = 1;
        break;
    case UMC_2X_FFMODE:
        m_uiFFModeMult  = 2;
        break;
    case UMC_4X_FFMODE:
        m_uiFFModeMult  = 4;
        break;
    case UMC_8X_FFMODE:
        m_uiFFModeMult  = 8;
        break;
    case UMC_16X_FFMODE:
    default:
        m_uiFFModeMult  = 16;
        break;
   }
}

Ipp64f UMC::NULLAudioRender::GetTimeTick()
{
    Ipp64f dfRes = ((m_bPaused) ? (Ipp64f)(Ipp64s)(m_PrePauseTime):
        (Ipp64f)(Ipp64s)(vm_time_get_tick() - m_LastStartTime +
        m_PrePauseTime)) / (Ipp64s)m_Freq;

    dfRes += m_dInitialPTS;
    return dfRes * m_uiFFModeMult;
}

UMC::Status UMC::NULLAudioRender::Init(MediaReceiverParams* pInit)
{
    Status umcRes = UMC_OK;
    umcRes = BasicAudioRender::Init(pInit);
    Reset();
    return umcRes;
}

UMC::Status UMC::NULLAudioRender::Pause(bool bPause)
{
    if (bPause && !m_bPaused)
    {
        m_PrePauseTime += vm_time_get_tick() - m_LastStartTime;
        m_bPaused = true;
    }
    else if (!bPause && m_bPaused)
    {
        m_LastStartTime = vm_time_get_tick();
        m_bPaused = false;
    }
    return UMC_OK;
}

Ipp32f UMC::NULLAudioRender::SetVolume(Ipp32f /*volume*/)
{   return -1;    }

Ipp32f UMC::NULLAudioRender::GetVolume()
{   return -1;    }

UMC::Status UMC::NULLAudioRender::Close ()
{
    m_LastStartTime = 0;
    m_PrePauseTime = 0;
    m_bPaused = true;
    return BasicAudioRender::Close();
}

UMC::Status UMC::NULLAudioRender::Reset ()
{
    m_LastStartTime = 0;
    m_PrePauseTime = 0;
    m_bPaused = true;

    //return BasicAudioRender::Reset();
    return m_DataBuffer.Reset();
}

UMC::Status UMC::NULLAudioRender::SendFrame(MediaData* in)
{
    Ipp64f dfStart = 0.0;
    Ipp64f dfEnd = 0.0;
    Status umcRes = UMC_OK;

    if (UMC_OK == umcRes)
    {   umcRes = in->GetTime(dfStart, dfEnd);   }

    //  If we "render" first frame and PTS starts
    if (0 == m_LastStartTime && 0 == m_PrePauseTime)
    {
        if (0 != dfStart)
        {   m_PrePauseTime = (vm_tick)(dfStart * (Ipp64s)m_Freq); }
    }

    if (UMC_OK == umcRes)
    {   umcRes = Pause(false);  }

    while ((GetTimeTick() < dfEnd - 0.1) && (!m_bPaused))
    {   vm_time_sleep(500);    }
    return umcRes;
}

UMC::Status UMC::NULLAudioRender::SetParams(MediaReceiverParams *pMedia,
                                            Ipp32u  trickModes)
{
    UMC::Status umcRes = UMC::UMC_OK;
    AudioRenderParams* pParams =
        DynamicCast<AudioRenderParams, MediaReceiverParams>(pMedia);
    if (NULL == pParams)
    {   umcRes = UMC_ERR_NULL_PTR;  }

    NULLAudioRenderParams* pNULLParams = DynamicCast<NULLAudioRenderParams>(pMedia);

    if ((pNULLParams) && ((pNULLParams->m_InitPTS) > 0)) {
        m_dInitialPTS = pNULLParams->m_InitPTS;
     }

    switch(trickModes)
    {
    case UMC_TRICK_MODES_FFW_FAST:
        m_uiFFModeMult  = 2;
        break;
    case UMC_TRICK_MODES_FFW_FASTER:
        m_uiFFModeMult  = 4;
        break;
    case UMC_TRICK_MODES_SFW_SLOW:
        m_uiFFModeMult  = 0.5;
        break;
    case UMC_TRICK_MODES_SFW_SLOWER:
        m_uiFFModeMult  = 0.25;
        break;
    default:
        m_uiFFModeMult  = 1;
        break;
   }
    return UMC_OK;
}