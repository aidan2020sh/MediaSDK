/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: mfx_plugin.h

\* ****************************************************************************** */

#include "mfx_plugin.h"
#include "mfx_session.h"
#include "vm_sys_info.h"

#define MSDK_CHECK_POINTER(P, ...)               {if (!(P)) {return __VA_ARGS__;}}
#define MSDK_CHECK_NOT_EQUAL(P, X, ERR)          {if ((X) != (P)) {return ERR;}}

//defining module template for decoder plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    MFXLibPlugin::Create,
    NULL,
    NULL
};

MSDK_PLUGIN_API(MFXDecoderPlugin*) mfxCreateDecoderPlugin() {
    if (!g_PluginModule.CreateDecoderPlugin) {
        return 0;
    }
    return g_PluginModule.CreateDecoderPlugin();
}

MFXLibPlugin::MFXLibPlugin()
{
    Init();
}

void MFXLibPlugin::Init()
{
    m_session = 0;
    m_pmfxCore.reset();
    m_pFrameAllocator = 0;
    m_pExternalSurfaceAllocator = 0;
    memset(&m_PluginParam, 0, sizeof(mfxPluginParam));
    m_PluginParam.CodecId = MFX_CODEC_HEVC;
    max_AsyncDepth = vm_sys_info_get_cpu_num();
    m_syncp = 0;
    m_p_surface_out = 0;
    m_device.type = MFX_HANDLE_D3D9_DEVICE_MANAGER;
    m_device.handle = 0;
}

MFXLibPlugin::~MFXLibPlugin()
{
    if (m_session)
    {
        MFXVideoDECODE_Close(m_session);
        MFXClose(m_session);
        m_session = 0;
    }
}

mfxStatus MFXLibPlugin::PluginInit(mfxCoreInterface *core)
{
    if (!core)
        return MFX_ERR_NULL_PTR;
    mfxCoreParam par;
    mfxStatus mfxRes = MFX_ERR_NONE;

    m_pmfxCore.reset(new MFXCoreInterface(*core));
    m_pCore = core;
    mfxRes = m_pmfxCore->GetCoreParam(&par);
    //std::cout << "PLUGIN_WRN: mfxRes = m_pmfxCore->GetCoreParam(&CorePar) = " << mfxRes << "\n";
    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;
    
    
    mfxRes = MFXInit(par.Impl, &par.Version, &m_session);
    MFX_CHECK_STS(mfxRes);
        
    mfxRes = MFXInternalPseudoJoinSession((mfxSession) m_pCore->pthis, m_session);
    MFX_CHECK_STS(mfxRes);
        


    //if (!m_pFrameAllocator)
    //{
    //    m_pFrameAllocator = &(m_pmfxCore->FrameAllocator());
    //}
    //if (!m_pExternalSurfaceAllocator)
    //{
    //    m_pExternalSurfaceAllocator = &(m_pmfxCore->ExternalSurfaceAllocator());
    //}
    //    mfxRes = MFXVideoCORE_SetFrameAllocator(m_session, m_pExternalSurfaceAllocator);
    //    //std::cout << "PLUGIN_WRN: MFXVideoCORE_SetFrameAllocator = " << mfxRes << "\n";

    return mfxRes;
}

mfxStatus MFXLibPlugin::PluginClose()
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    if (m_session)
    {
        //The application must ensure there is no active task running in the session before calling this (MFXDisjoinSession) function.
        mfxRes = MFXVideoDECODE_Close(m_session);
        MFX_CHECK_STS(mfxRes);
        mfxRes = MFXInternalPseudoDisjoinSession(m_session);
        MFX_CHECK_STS(mfxRes);
        mfxRes = MFXClose(m_session);
        MFX_CHECK_STS(mfxRes);
        m_session = 0;
        return mfxRes;
    }
    else
        return MFX_ERR_NONE;
}
mfxStatus MFXLibPlugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;

    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
    m_PluginParam.MaxThreadNum = 1;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus MFXLibPlugin::DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
{
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;
    m_syncp = 0;
    mfxRes = MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, &m_syncp);
    if (MFX_ERR_NONE == mfxRes)
    {
        //TODO: make syncp handling safer
        mfxSyncPoint* syncp = new mfxSyncPoint;
        *syncp = m_syncp;
        *task = (mfxHDL*) syncp;
        mfxRes = MFX_ERR_NONE;
    }
    return mfxRes;
}

mfxStatus MFXLibPlugin::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{
    if(uid_p || uid_a)
        return MFX_WRN_IN_EXECUTION; //Cannot call SyncOperation several times for the same syncp
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;
    mfxSyncPoint* syncp = 0;
    syncp = (mfxSyncPoint*) task;
    mfxRes = MFXVideoCORE_SyncOperation(m_session, *syncp, MFX_INFINITE);
    return mfxRes;
}

mfxStatus MFXLibPlugin::FreeResources(mfxThreadTask task, mfxStatus sts)
{
    m_syncp = 0;
    m_p_surface_out = 0;
    mfxSyncPoint* syncp = 0;
    syncp = (mfxSyncPoint*) task;
    delete syncp;
    return MFX_ERR_NONE;
}

mfxStatus MFXLibPlugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    if (!in)
        return MFX_ERR_NULL_PTR;
    bool bWarnIsNeeded = false;
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;

    //if ((MFX_IOPATTERN_OUT_OPAQUE_MEMORY == in->IOPattern) || (MFX_IOPATTERN_OUT_OPAQUE_MEMORY == in->IOPattern))
    //    return MFX_ERR_UNSUPPORTED;
    //if (!((1 == in->AsyncDepth) || (0 == in->AsyncDepth)))
    //{
    //    //std::cout << "PLUGIN_WRN: application tried to Query decoder plugin with AsyncDepth > 1\n";
    //    bWarnIsNeeded = true;
    //}
    //in->AsyncDepth = 1;

    if(!m_device.handle)
    {
        mfxRes = GetHandle();
        if (MFX_ERR_NONE == mfxRes)
            mfxRes = MFXVideoCORE_SetHandle(m_session, m_device.type, m_device.handle);
    }

    if(0 == in->AsyncDepth)
        in->AsyncDepth = max_AsyncDepth;
    mfxRes = MFXVideoDECODE_Query(m_session, in, out);

    if (bWarnIsNeeded && (MFX_ERR_NONE == mfxRes))
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else
        return mfxRes;
}

mfxStatus MFXLibPlugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    bool bWarnIsNeeded = false;
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;

    //if ((MFX_IOPATTERN_OUT_OPAQUE_MEMORY == par->IOPattern) || (MFX_IOPATTERN_OUT_OPAQUE_MEMORY == par->IOPattern))
    //    return MFX_ERR_UNSUPPORTED;
    //if (!((1 == par->AsyncDepth) || (0 == par->AsyncDepth)))
    //{
    //    //std::cout << "PLUGIN_WRN: application tried to QueryIOSurf decoder plugin with AsyncDepth > 1\n";
    //    bWarnIsNeeded = true;
    //}
    //par->AsyncDepth = 1;

    if(!m_device.handle)
    {
        mfxRes = GetHandle();
        if (MFX_ERR_NONE == mfxRes)
            mfxRes = MFXVideoCORE_SetHandle(m_session, m_device.type, m_device.handle);
    }
    if(0 == par->AsyncDepth)
        par->AsyncDepth = max_AsyncDepth;
    mfxRes = MFXVideoDECODE_QueryIOSurf(m_session, par, request);

    if (bWarnIsNeeded && (MFX_ERR_NONE == mfxRes))
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else
        return mfxRes;
}

mfxStatus MFXLibPlugin::Init(mfxVideoParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    bool bWarnIsNeeded = false;
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;
    mfxCoreParam CorePar;

    //if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY == par->IOPattern)
    //    return MFX_ERR_UNSUPPORTED;
    //if (!((1 == par->AsyncDepth) || (0 == par->AsyncDepth)))
    //{
    //    //std::cout << "PLUGIN_WRN: application tried to initialize decoder plugin with AsyncDepth > 1\n";
    //    bWarnIsNeeded = true;
    //}
    //par->AsyncDepth = 1;

    mfxRes = m_pmfxCore->GetCoreParam(&CorePar);
    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    if(!m_device.handle)
    {
        mfxRes = GetHandle();
        if (MFX_ERR_NONE == mfxRes)
            mfxRes = MFXVideoCORE_SetHandle(m_session, m_device.type, m_device.handle);
    }

    if(0 == par->AsyncDepth)
        par->AsyncDepth = max_AsyncDepth;
    mfxRes = MFXVideoDECODE_Init(m_session, par);

    if (bWarnIsNeeded && (MFX_ERR_NONE == mfxRes))
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else
        return mfxRes;
}

mfxStatus MFXLibPlugin::Reset(mfxVideoParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    bool bWarnIsNeeded = false;
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;

    //if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY == par->IOPattern)
    //    return MFX_ERR_UNSUPPORTED;
    //if (!((1 == par->AsyncDepth) || (0 == par->AsyncDepth)))
    //{
    //    //std::cout << "PLUGIN_WRN: application tried to reset decoder plugin with AsyncDepth > 1\n";
    //    bWarnIsNeeded = true;
    //}
    //par->AsyncDepth = 1;

    mfxRes = MFXVideoDECODE_Reset(m_session, par);

    if (bWarnIsNeeded && (MFX_ERR_NONE == mfxRes))
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    else
        return mfxRes;
}

mfxStatus MFXLibPlugin::GetHandle()
{
    mfxStatus mfxRes = MFX_ERR_UNKNOWN;
    mfxCoreParam CorePar;

    m_device_handle = 0;
    mfxRes = m_pmfxCore->GetCoreParam(&CorePar);
    if (MFX_ERR_NONE != mfxRes)
        return mfxRes;

    if(MFX_IMPL_VIA_D3D9  & CorePar.Impl)
        m_device.type = MFX_HANDLE_D3D9_DEVICE_MANAGER;
    if(MFX_IMPL_VIA_D3D11 & CorePar.Impl)
        m_device.type = MFX_HANDLE_D3D11_DEVICE;

    mfxRes = m_pmfxCore->GetHandle(m_device.type, &m_device.handle);

    return mfxRes;
}
