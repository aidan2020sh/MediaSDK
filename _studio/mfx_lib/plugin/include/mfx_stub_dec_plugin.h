//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2018 Intel Corporation. All Rights Reserved.
//

#if !defined(__MFX_STUB_DEC_PLUGIN_INCLUDED__)
#define __MFX_STUB_DEC_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include "mfxvideo.h"
#include "mfxplugin++.h"
#include "mfxvideo++int.h"

#ifndef MFX_VA
struct MFXStubDecoderPlugin : MFXDecoderPlugin
{
    /* MFXPlugin */
    mfxStatus PluginInit(mfxCoreInterface*) override;
    mfxStatus PluginClose() override;
    mfxStatus Execute(mfxThreadTask task, mfxU32 , mfxU32 ) override
    {
        return MFXVideoCORE_SyncOperation(m_session, (mfxSyncPoint) task, MFX_INFINITE);
    }
    mfxStatus FreeResources(mfxThreadTask , mfxStatus ) override
    {
        return MFX_ERR_NONE;
    }
    void Release() override
    {
        delete this;
    }
    mfxStatus Close() override
    {
        return MFXVideoDECODE_Close(m_session);
    }
    mfxStatus SetAuxParams(void* , int ) override
    {
        return MFX_ERR_UNKNOWN;
    }

    /* MFXCodecPlugin */
    mfxStatus Init(mfxVideoParam *par) override
    {
        return MFXVideoDECODE_Init(m_session, par);
    }
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *, mfxFrameAllocRequest *out) override
    {
        return MFXVideoDECODE_QueryIOSurf(m_session, par, out);
    }
    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override
    {
        return MFXVideoDECODE_Query(m_session, in, out);
    }
    mfxStatus Reset(mfxVideoParam *par) override
    {
        return MFXVideoDECODE_Reset(m_session, par);
    }
    mfxStatus GetVideoParam(mfxVideoParam *par) override
    {
        return MFXVideoDECODE_GetVideoParam(m_session, par);
    }

    /* MFXDecoderPlugin */
    mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) override
    {
        return MFXVideoDECODE_DecodeHeader(m_session, bs, par);
    }
    mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) override
    {
        return MFXVideoDECODE_GetPayload(m_session, ts, payload);
    }
    mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task)
    {
        return MFXVideoDECODE_DecodeFrameAsync(m_session, bs, surface_work, surface_out, (mfxSyncPoint*) task);
    }

protected:

    template <typename T>
    static mfxStatus CreateInstance(T**);

protected:

    MFXStubDecoderPlugin(bool CreateByDispatcher);
    virtual ~MFXStubDecoderPlugin();

    mfxSession          m_session;
    mfxCoreInterface*   m_pmfxCore;
    bool                m_createdByDispatcher;
};
#else
struct MFXStubDecoderPlugin : MFXDecoderPlugin
{
    /* mfxPlugin */
    static mfxStatus _PluginInit(mfxHDL /*pthis*/, mfxCoreInterface*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _PluginClose(mfxHDL /*pthis*/) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    static mfxStatus _Submit(mfxHDL /*pthis*/, const mfxHDL* /*in*/, mfxU32 /*in_num*/, const mfxHDL* /*out*/, mfxU32 /*out_num*/, mfxThreadTask*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _Execute(mfxHDL /*pthis*/, mfxThreadTask, mfxU32 /*thread_id*/, mfxU32 /*call_count*/) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _FreeResources(mfxHDL /*pthis*/, mfxThreadTask, mfxStatus) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    /* mfxVideoCodecPlugin */
    static mfxStatus _Query(mfxHDL /*pthis*/, mfxVideoParam* /*in*/, mfxVideoParam* /*out*/) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _QueryIOSurf(mfxHDL /*pthis*/, mfxVideoParam*, mfxFrameAllocRequest* /*in*/, mfxFrameAllocRequest* /*out*/) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _Init(mfxHDL /*pthis*/, mfxVideoParam*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _Reset(mfxHDL /*pthis*/, mfxVideoParam*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _Close(mfxHDL /*pthis*/) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _GetVideoParam(mfxHDL /*pthis*/, mfxVideoParam*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    static mfxStatus _DecodeHeader(mfxHDL /*pthis*/, mfxBitstream*, mfxVideoParam*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _GetPayload(mfxHDL /*pthis*/, mfxU64* /*ts*/, mfxPayload*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    static mfxStatus _DecodeFrameSubmit(mfxHDL /*pthis*/, mfxBitstream*, mfxFrameSurface1* /*surface_work*/, mfxFrameSurface1** /*surface_out*/,  mfxThreadTask*) {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID, mfxPlugin*);

private:

    /* MFXPlugin */
    mfxStatus PluginInit(mfxCoreInterface* core) override
    {
        return _PluginInit(this, core);
    }
    mfxStatus PluginClose() override
    {
        return _PluginClose(this);
    }

    mfxStatus Execute(mfxThreadTask task, mfxU32 thread_id, mfxU32 call_count) override
    {
        return _Execute(this, task, thread_id, call_count);
    }
    mfxStatus FreeResources(mfxThreadTask task, mfxStatus status) override
    {
        return _FreeResources(this, task, status);
    }
    void Release() override
    {}
    mfxStatus Close() override
    {
        return _Close(this);
    }
    mfxStatus SetAuxParams(void* , int ) override
    {
        return MFX_ERR_UNKNOWN;
    }

    /* MFXCodecPlugin */
    mfxStatus Init(mfxVideoParam *par) override
    {
        return _Init(this, par);
    }
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out) override
    {
        return _QueryIOSurf(this, par, in, out);
    }
    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) override
    {
        return _Query(this, in, out);
    }
    mfxStatus Reset(mfxVideoParam *par) override
    {
        return _Reset(this, par);
    }
    mfxStatus GetVideoParam(mfxVideoParam *par) override
    {
        return _GetVideoParam(this, par);
    }

    mfxStatus DecodeHeader(mfxBitstream *bs, mfxVideoParam *par) override
    {
        return _DecodeHeader(this, bs, par);
    }
    mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload) override
    {
        return _GetPayload(this, ts, payload);
    }
    mfxStatus DecodeFrameSubmit(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,  mfxThreadTask *task) override
    {
        return _DecodeFrameSubmit(this, bs, surface_work, surface_out, task);
    }
};
#endif

#endif  // __MFX_STUB_DEC_PLUGIN_INCLUDED__
