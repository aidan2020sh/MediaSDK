//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(AS_VP9E_PLUGIN)
#include "mfx_vp9_encode_plugin_hw.h"
#include "plugin_version_linux.h"

#if defined(_WIN32)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#elif defined(LINUX32)
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

MSDK_PLUGIN_API(MFXEncoderPlugin*) mfxCreateEncoderPlugin()
{
    return MfxHwVP9Encode::Plugin::Create();
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin)
{
    return MfxHwVP9Encode::Plugin::CreateByDispatcher(uid, plugin);
}
#endif