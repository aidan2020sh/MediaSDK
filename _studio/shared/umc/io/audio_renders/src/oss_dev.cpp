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

//
//      This file contains class covering OSS Linux devices interface
//

#include "umc_defs.h"

#ifdef UMC_ENABLE_OSS_AUDIO_RENDER

#include <sys/soundcard.h>
#include "fcntl.h"
#include "oss_dev.h"
#include "vm_debug.h"

UMC::Status UMC::OSSDev::Init(const Ipp32u uiBitsPerSample,
                              const Ipp32u uiNumOfChannels,
                              const Ipp32u uiFreq)
{
    UMC::Status umcRes = UMC_OK;

    if (UMC_OK == umcRes)
    {   umcRes = m_Device.Init(VM_STRING("/dev/dsp"), O_WRONLY);    }

    if (UMC_OK == umcRes)
    {
        m_uiBitsPerSample = uiBitsPerSample;
        m_uiNumOfChannels = uiNumOfChannels;
        m_uiFreq = uiFreq;
        umcRes = Reset();
    }

    if (UMC_OK == umcRes)
    {   vm_debug_trace(VM_DEBUG_INFO, VM_STRING("OSSDev::Init OK\n"));  }
    else
    {   vm_debug_trace(VM_DEBUG_ALL, VM_STRING("OSSDev::Init failed\n"));  }
    return umcRes;
}

UMC::Status UMC::OSSDev::RenderData(Ipp8u* pbData, Ipp32u uiDataSize)
{
    if (0x3 & uiDataSize)
    {
        vm_debug_trace1(VM_DEBUG_ALL, VM_STRING("OSSDev::RenderData data size misalignment: %d\n"),
                        uiDataSize);
    }
    m_uiBytesPlayed += uiDataSize;
    return m_Device.Write(pbData, uiDataSize, NULL);
}

UMC::Status UMC::OSSDev::SetBitsPerSample(const Ipp32u uiBitsPerSample)
{
    Status umcRes = UMC_OK;
    Ipp32u uiFormat;

    switch (uiBitsPerSample)
    {
    case 16:
        uiFormat = AFMT_S16_LE;
        break;
    case 8:
        uiFormat = AFMT_S8;
        break;
    default:
        vm_debug_trace1(VM_DEBUG_ALL, VM_STRING("OSSDev::SetBitsPerSample - unsupported: %dbps requested\n"),
                        uiFormat);
        umcRes = UMC_ERR_UNSUPPORTED;
    }

    Ipp32u uiFormatSet = uiFormat;
    if (UMC_OK == umcRes)
    {   umcRes = m_Device.Ioctl(SNDCTL_DSP_SETFMT, (void *)&uiFormatSet); }

    if (UMC_OK == umcRes && uiFormat != uiFormatSet)
    {
        umcRes = UMC_ERR_UNSUPPORTED;
        vm_debug_trace2(VM_DEBUG_ALL, VM_STRING("OSSDev::SetBitsPerSample - unsupported: %dbps requested, %dbps returned\n"),
                        uiFormat, uiFormatSet);
    }
    return umcRes;
}

UMC::Status UMC::OSSDev::SetNumOfChannels(const Ipp32u uiNumOfChannels)
{
    Status umcRes = UMC_OK;
    Ipp32u uiNumOfChannelsSet = uiNumOfChannels;

    if (UMC_OK == umcRes)
    {
        umcRes = m_Device.Ioctl(SNDCTL_DSP_CHANNELS,
                                (void *)&uiNumOfChannelsSet);
    }

    if (UMC_OK == umcRes && uiNumOfChannels != uiNumOfChannelsSet)
    {
        vm_debug_trace2(VM_DEBUG_ALL,
            VM_STRING("OSSDev::SetNumOfChannels - unsupported: %d channels requested, %d channels returned\n"),
            uiNumOfChannels, uiNumOfChannelsSet);
        umcRes = UMC_ERR_UNSUPPORTED;
    }
    return umcRes;
}

UMC::Status
UMC::OSSDev::SetFreq(const Ipp32u uiFreq)
{
    Status umcRes = UMC_OK;
    Ipp32u uiFreqSet = uiFreq;

    if (UMC_OK == umcRes)
    {   umcRes = m_Device.Ioctl(SNDCTL_DSP_SPEED, (void *)&uiFreqSet);    }

    if (UMC_OK == umcRes && uiFreq != uiFreqSet)
    {
        vm_debug_trace2(VM_DEBUG_ALL,
            VM_STRING("OSSDev::SetFreq - unsupported: %dHz requested, %dHz returned\n"),
            uiFreq, uiFreqSet);
        umcRes = UMC_ERR_UNSUPPORTED;
    }
    return umcRes;
}

UMC::Status UMC::OSSDev::Post()
{
    Status umcRes = m_Device.Ioctl(SNDCTL_DSP_POST);

    if (UMC_OK != umcRes)
    {   vm_debug_trace(VM_DEBUG_ALL, VM_STRING("OSSDev::Post failed\n"));  }
    return umcRes;
}

UMC::Status UMC::OSSDev::Reset()
{
    Status umcRes = m_Device.Ioctl(SNDCTL_DSP_RESET);

    if (UMC_OK == umcRes)
    {   umcRes = SetBitsPerSample(m_uiBitsPerSample);   }

    if (UMC_OK == umcRes)
    {   umcRes = SetNumOfChannels(m_uiNumOfChannels);   }

    if (UMC_OK == umcRes)
    {   umcRes = SetFreq(m_uiFreq);    }

    if (UMC_OK != umcRes)
    {   vm_debug_trace(VM_DEBUG_ALL, VM_STRING("OSSDev::Reset failed\n"));  }

    m_uiBytesPlayed = 0;
    return umcRes;
}

UMC::Status UMC::OSSDev::GetBytesPlayed(Ipp32u& ruiBytesPlayed)
{
    Status umcRes = UMC_OK;

    count_info Info;
    Info.bytes = 0;
    umcRes = m_Device.Ioctl(SNDCTL_DSP_GETODELAY, (void *)&Info);

    if (UMC_OK == umcRes)
    {   ruiBytesPlayed = m_uiBytesPlayed - Info.bytes;    }
    return umcRes;
}

#endif // UMC_ENABLE_OSS_AUDIO_RENDER