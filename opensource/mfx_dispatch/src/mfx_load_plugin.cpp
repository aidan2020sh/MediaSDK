/* ****************************************************************************** *\

Copyright (C) 2013 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

File Name: mfx_load_plugin.h

\* ****************************************************************************** */

#include "mfx_load_plugin.h"
#include "mfx_load_dll.h"
#include "mfx_dispatcher_log.h"
#include <algorithm>
#include <xfunctional>

#define TRACE_PLUGIN_ERROR(str, ...) DISPATCHER_LOG_ERROR((("[PLUGIN]: "str), __VA_ARGS__))
#define TRACE_PLUGIN_INFO(str, ...) DISPATCHER_LOG_INFO((("[PLUGIN]: "str), __VA_ARGS__))

MFX::PluginModule::PluginModule()
    : mHmodule()
    , mCreatePluginPtr() {
}

MFX::PluginModule::PluginModule(const msdk_disp_char * path)
    : mCreatePluginPtr() {
    mHmodule = mfx_dll_load(path);
    if (NULL == mHmodule) {
        TRACE_PLUGIN_ERROR("Cannot load module: %S\n", path);
        return ;
    }
    TRACE_PLUGIN_INFO("Plugin loaded at: %S\n", path);
    
    mCreatePluginPtr = (mfxPluginCreateCallback)mfx_dll_get_addr(mHmodule, MFX_CREATE_PLUGIN_FNC);
    if (NULL == mCreatePluginPtr) {
        TRACE_PLUGIN_ERROR("Cannot get procedure address: %s\n", MFX_CREATE_PLUGIN_FNC);
        return ;
    }
    
    return ;
}

MFX::PluginModule::~PluginModule(void) {
    mfx_dll_free(mHmodule);
}

bool MFX::PluginModule::Create( mfxPluginUID uid, mfxPlugin& plg) {
    bool result = false;
    if (mCreatePluginPtr) {
        result = mCreatePluginPtr(uid, &plg);
        if (!result) {
            TRACE_PLUGIN_ERROR("Cannot create plugin, %s returned false\n", MFX_CREATE_PLUGIN_FNC);
        } else {
            TRACE_PLUGIN_INFO("Plugin created\n", 0);
        }
    }
    return result;
}


bool MFX::MFXPluginFactory::RunVerification( mfxPlugin & plg, PluginDescriptionRecord &dsc, mfxPluginParam &pluginParams)
{
    if (plg.PluginInit == 0) {
        TRACE_PLUGIN_ERROR("plg->PluginInit = 0\n", 0);
        return false;
    }
    if (plg.PluginClose == 0) {
        TRACE_PLUGIN_ERROR("plg->PluginClose = 0\n", 0);
        return false;
    }
    if (plg.GetPluginParam == 0) {
        TRACE_PLUGIN_ERROR("plg->GetPluginParam = 0\n", 0);
        return false;
    }
    
    if (plg.Execute == 0) {
        TRACE_PLUGIN_ERROR("plg->Execute = 0\n", 0);
        return false;
    }
    if (plg.FreeResources == 0) {
        TRACE_PLUGIN_ERROR("plg->FreeResources = 0\n", 0);
        return false;
    }

    mfxStatus sts = plg.GetPluginParam(plg.pthis, &pluginParams);
    if (sts != MFX_ERR_NONE) {
        TRACE_PLUGIN_ERROR("plg->GetPluginParam() returned %d\n", sts);
        return false;
    }

    if (pluginParams.CodecId != dsc.CodecId) {
        TRACE_PLUGIN_ERROR("plg->GetPluginParam() returned CodecId="MFXFOURCCTYPE()", but registration has CodecId="MFXFOURCCTYPE()"\n"
            , MFXU32TOFOURCC(pluginParams.CodecId), MFXU32TOFOURCC(dsc.CodecId));
        return false;
    }

    if (pluginParams.Type != dsc.Type) {
        TRACE_PLUGIN_ERROR("plg->GetPluginParam() returned Type=%d, but registration has Type=%d\n", pluginParams.Type, dsc.Type);
        return false;
    }

    if (pluginParams.uid !=  dsc.uid) {
        TRACE_PLUGIN_ERROR("plg->GetPluginParam() returned UID="MFXGUIDTYPE()", but registration has UID="MFXGUIDTYPE()"\n"
            , MFXGUIDTOHEX(pluginParams.uid), MFXGUIDTOHEX(dsc.uid));
        return false;
    }

    switch(pluginParams.Type) {
        case MFX_PLUGINTYPE_VIDEO_DECODE: 
        case MFX_PLUGINTYPE_VIDEO_ENCODE: {
            TRACE_PLUGIN_INFO("plugin type= %d\n", pluginParams.Type);
            if (plg.Video == 0) {
                TRACE_PLUGIN_ERROR("plg->Video = 0\n", 0);
                return false;
            }

            if (!VerifyCodecCommon(*plg.Video))
                return false;
            
            break;
        }
        default: {
            TRACE_PLUGIN_ERROR("unsupported plugin type: %d\n", pluginParams.Type);
            return false;
        }
    }

    switch(pluginParams.Type) {
        case MFX_PLUGINTYPE_VIDEO_DECODE: 
            return VerifyDecoder(*plg.Video);
        case MFX_PLUGINTYPE_VIDEO_ENCODE: 
            return VerifyEncoder(*plg.Video);
    }

    return true;
}

bool MFX::MFXPluginFactory::VerifyEncoder( mfxVideoCodecPlugin &encoder )
{
    if (encoder.EncodeFrameSubmit == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->EncodeFrameSubmit = 0\n", 0);
        return false;
    }
    
    return true;
}

bool MFX::MFXPluginFactory::VerifyDecoder( mfxVideoCodecPlugin &decoder )
{
    if (decoder.DecodeHeader == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->DecodeHeader = 0\n", 0);
        return false;
    }
    if (decoder.GetPayload == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->GetPayload = 0\n", 0);
        return false;
    }
    if (decoder.DecodeFrameSubmit == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->DecodeFrameSubmit = 0\n", 0);
        return false;
    }

    return true;
}

bool MFX::MFXPluginFactory::VerifyCodecCommon( mfxVideoCodecPlugin & videoCodec ) {
    if (videoCodec.Query == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->Query = 0\n", 0);
        return false;
    }

    if (videoCodec.Query == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->Query = 0\n", 0);
        return false;
    }
    if (videoCodec.QueryIOSurf == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->QueryIOSurf = 0\n", 0);
        return false;
    }
    if (videoCodec.Init == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->Init = 0\n", 0);
        return false;
    }
    if (videoCodec.Reset == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->Reset = 0\n", 0);
        return false;
    }
    if (videoCodec.Close == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->Close = 0\n", 0);
        return false;
    }
    if (videoCodec.GetVideoParam == 0) {
        TRACE_PLUGIN_ERROR("plg->Video->GetVideoParam = 0\n", 0);
        return false;
    }

    return true;
}


bool MFX::MFXPluginFactory::Create( PluginDescriptionRecord & rec) {
    PluginModule plgModule(rec.Path.c_str());
    mfxPlugin plg;
    mfxPluginParam plgParams;
    
    if (!plgModule.Create(rec.uid, plg)) {
        return false;
    }
    
    if (!RunVerification(plg, rec, plgParams)) {
        //will do not call plugin close since it is not safe to do that until structure is corrected
        return false;
    }
    
    mfxStatus sts = MFXVideoUSER_Register(mSession, plgParams.Type, &plg);
    if (MFX_ERR_NONE != sts) {
        TRACE_PLUGIN_ERROR(" MFXVideoUSER_Register returned %d\n", sts);
        return false;
    }
    
    mPlugins.push_back(FactoryRecord(plgParams, plgModule, plg));

    return true;
}

MFX::MFXPluginFactory::~MFXPluginFactory() {
    Close();
}

MFX::MFXPluginFactory::MFXPluginFactory( mfxSession session ) {
    mSession = session;
}

bool MFX::MFXPluginFactory::Destroy( const mfxPluginUID & uidToDestroy) {
    for (std::list<FactoryRecord >::iterator i = mPlugins.begin(); i!= mPlugins.end(); i++) {
        if (i->plgParams.uid == uidToDestroy) {
            DestroyPlugin(*i);
            //dll unload should happen here
            mPlugins.erase(i);
            return  true;
        }
    }
    return false;
}

void MFX::MFXPluginFactory::Close() {
    for (std::list<FactoryRecord >::iterator i = mPlugins.begin(); i!= mPlugins.end(); i++) {
        DestroyPlugin(*i);
    }
    mPlugins.clear();
}

void MFX::MFXPluginFactory::DestroyPlugin( FactoryRecord & record)
{
    mfxStatus sts = MFXVideoUSER_Unregister(mSession, record.plgParams.Type);
    TRACE_PLUGIN_INFO(" MFXVideoUSER_Unregister for Type=%d, returned %d\n", record.plgParams.Type, sts);
}