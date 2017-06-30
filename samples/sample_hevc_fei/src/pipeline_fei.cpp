/******************************************************************************\
Copyright (c) 2017, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "pipeline_fei.h"

CEncodingPipeline::CEncodingPipeline(sInputParams& userInput)
    : m_inParams(userInput)
    , m_impl(MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_VAAPI)
{
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::Init()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_mfxSession.Init(m_impl, NULL);
    MSDK_CHECK_STATUS(sts, "m_mfxSession.Init failed");

    sts = LoadFEIPlugin();
    MSDK_CHECK_STATUS(sts, "LoadPlugin failed");

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_STATUS(sts, "CreateAllocator failed");

    return sts;
}

void CEncodingPipeline::Close()
{
    m_pPlugin.reset(); // Unload plugin by destructing object PluginLoader
    m_mfxSession.Close();
}

mfxStatus CEncodingPipeline::LoadFEIPlugin()
{
    // m_pluginGuid = msdkGetPluginUID(m_impl, MSDK_VENC, MFX_CODEC_HEVC);
    m_pluginGuid = msdkGetPluginUID(m_impl, MSDK_VENCODE, MFX_CODEC_HEVC); // use HW HEVC for debug
    MSDK_CHECK_ERROR(AreGuidsEqual(m_pluginGuid, MSDK_PLUGINGUID_NULL), true, MFX_ERR_NOT_FOUND);

    // m_pPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENC, m_mfxSession, m_pluginGuid, 1));
    m_pPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENCODE, m_mfxSession, m_pluginGuid, 1));
    MSDK_CHECK_POINTER(m_pPlugin.get(), MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = CreateHWDevice();
    MSDK_CHECK_STATUS(sts, "CreateHWDevice failed");

    mfxHDL hdl = NULL;
    sts = m_pHWdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl);
    MSDK_CHECK_STATUS(sts, "m_pHWdev->GetHandle failed");

    // provide device manager to MediaSDK
    sts = m_mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, hdl);
    MSDK_CHECK_STATUS(sts, "m_mfxSession.SetHandle failed");

    /* Only video memory is supported now. So application must provide MediaSDK
    with external allocator and call SetFrameAllocator */
    // create VAAPI allocator
    m_pMFXAllocator.reset(new vaapiFrameAllocator);

    std::auto_ptr<vaapiAllocatorParams> p_vaapiAllocParams(new vaapiAllocatorParams);

    p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
    m_pMFXAllocatorParams = p_vaapiAllocParams;

    // Call SetAllocator to pass external allocator to MediaSDK
    sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator.get());
    MSDK_CHECK_STATUS(sts, "m_mfxSession.SetFrameAllocator failed");

    // initialize memory allocator
    sts = m_pMFXAllocator->Init(m_pMFXAllocatorParams.get());
    MSDK_CHECK_STATUS(sts, "m_pMFXAllocator->Init failed");

    return sts;
}

mfxStatus CEncodingPipeline::CreateHWDevice()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_pHWdev.reset(CreateVAAPIDevice());
    MSDK_CHECK_POINTER(m_pHWdev.get(), MFX_ERR_MEMORY_ALLOC);

    sts = m_pHWdev->Init(NULL, 0, MSDKAdapter::GetNumber(m_mfxSession));
    MSDK_CHECK_STATUS(sts, "m_pHWdev->Init failed");

    return sts;
}
