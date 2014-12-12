/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

File Name: mfx_unified_dec_plugin.cpp

\* ****************************************************************************** */

#include "mfx_hevc_dec_plugin.h"
#include "mfx_vp8_dec_plugin.h"
#include "mfx_vp9_dec_plugin.h"
#include "mfx_h265_encode_hw.h"

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    return 0;
}

MSDK_PLUGIN_API(mfxStatus) CreatePlugin(mfxPluginUID uid, mfxPlugin* plugin) {

    if(std::memcmp(uid.Data, MFXHEVCDecoderPlugin::g_HEVCDecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXHEVCDecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MFXVP8DecoderPlugin::g_VP8DecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXVP8DecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MFXVP9DecoderPlugin::g_VP9DecoderGuid.Data, sizeof(uid.Data)) == 0)
        return MFXVP9DecoderPlugin::CreateByDispatcher(uid, plugin);
    else if(std::memcmp(uid.Data, MfxHwH265Encode::MFX_PLUGINID_HEVCE_HW.Data, sizeof(uid.Data)) == 0)
        return MfxHwH265Encode::Plugin::CreateByDispatcher(uid, plugin);
    else return MFX_ERR_NOT_FOUND;
}
