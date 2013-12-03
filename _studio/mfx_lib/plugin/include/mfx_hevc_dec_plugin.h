/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_hevc_dec_plugin.h

\* ****************************************************************************** */

#if !defined(__MFX_HEVC_DEC_PLUGIN_INCLUDED__)
#define __MFX_HEVC_DEC_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"

#if defined( AS_HEVCD_PLUGIN )
class MFXHEVCDecoderPlugin : public MFXDecoderPlugin
{
    static const mfxPluginUID g_HEVCDecoderGuid;
    static std::auto_ptr<MFXHEVCDecoderPlugin> g_singlePlg;
    static std::auto_ptr<MFXPluginAdapter<MFXDecoderPlugin> > g_adapter;
public:
    MFXHEVCDecoderPlugin(bool CreateByDispatcher);
    virtual ~MFXHEVCDecoderPlugin();

    virtual mfxStatus PluginInit(mfxCoreInterface *core); //Init plugin as a component (~MFXInit)
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
    {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, (mfxSyncPoint*) task);
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 )
    {
        return MFXVideoCORE_SyncOperation(m_session, (mfxSyncPoint) task, MFX_INFINITE);
    }
    virtual mfxStatus FreeResources(mfxThreadTask , mfxStatus )
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        return MFXVideoDECODE_Query(m_session, in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *, mfxFrameAllocRequest *out)
    {
        return MFXVideoDECODE_QueryIOSurf(m_session, par, out);
    }
    virtual mfxStatus Init(mfxVideoParam *par)
    {
        return MFXVideoDECODE_Init(m_session, par);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        return MFXVideoDECODE_Reset(m_session, par);
    }
    virtual mfxStatus Close()
    {
        return MFXVideoDECODE_Close(m_session);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par)
    {
        return MFXVideoDECODE_GetVideoParam(m_session, par);
    }
    virtual mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par)
    {
        return MFXVideoDECODE_DecodeHeader(m_session, bs, par);
    }
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload)
    {
        return MFXVideoDECODE_GetPayload(m_session, ts, payload);
    }
    virtual void Release()
    {
        delete this;
    }
    static MFXDecoderPlugin* Create() {
        return new MFXHEVCDecoderPlugin(false);
    }
    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg) {

        if (g_singlePlg.get()) {
            return MFX_ERR_NOT_FOUND;
        } 
        //check that guid match 
        g_singlePlg.reset(new MFXHEVCDecoderPlugin(true));

        if (memcmp(& guid , &g_HEVCDecoderGuid, sizeof(mfxPluginUID))) {
            return MFX_ERR_NOT_FOUND;
        }

        g_adapter.reset(new MFXPluginAdapter<MFXDecoderPlugin> (g_singlePlg.get()));
        *mfxPlg = g_adapter->operator mfxPlugin();

        return MFX_ERR_NONE;
    }
    virtual mfxU32 GetPluginType()
    {
        return MFX_PLUGINTYPE_VIDEO_DECODE;
    }
    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
};
#endif //#if defined( AS_HEVCD_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_HEVC_DEC_PLUGIN_INCLUDED__
