// Copyright (c) 2017-2018 Intel Corporation
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
#pragma once

#include "mfx_config.h"
#include "scd_tools.h"
#include "mfxplugin++.h"
#include "mfx_scd.h"

namespace MfxVppSCD
{
using namespace SCDTools;

static const mfxPluginUID MFX_PLUGINID_VPP_SCD = {{ 0x4f, 0x4e, 0x2f, 0x82, 0x90, 0x07, 0x9c, 0x47, 0xbf, 0x7a, 0xff, 0x37, 0xdd, 0x5f, 0xcd, 0x4f } };
#if defined(MFX_ENABLE_VPP_SCD_PARALLEL_COPY)
static const mfxU16 VPP_SCD_MAX_THREADS = 2;
#else
static const mfxU16 VPP_SCD_MAX_THREADS = 1;
#endif

enum eTaskStage
{
      COPY_IN_TO_INTERNAL  = 0b0001
    , COPY_IN_TO_OUT       = 0b0010
    , COPY_INTERNAL_TO_OUT = 0b0100
    , DO_SCD               = 0b1000
};

struct Task
{
    void Reset()
    {
        m_surfIn = 0;
        memset(&m_stages, 0, sizeof(m_stages));
    }

    mfxU32 m_stages[VPP_SCD_MAX_THREADS];
    mfxFrameSurface1* m_surfIn;
    mfxFrameSurface1* m_surfOut;
    mfxFrameSurface1* m_surfRealIn;
    mfxFrameSurface1* m_surfRealOut;
    mfxFrameSurface1 m_surfNative;
};

typedef SceneChangeDetector SCD;

class Plugin
    : public MFXVPPPlugin
    , protected MFXCoreInterface
    , protected SCD
    , protected InternalSurfaces
    , protected TaskManager<Task>
{
public:
    static MFXVPPPlugin* Create();
    static mfxStatus CreateByDispatcher(const mfxPluginUID& guid, mfxPlugin* mfxPlg);

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in,
                                     mfxFrameSurface1 *surface_out,
                                     mfxExtVppAuxData *aux, mfxThreadTask *task);
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *surface_in,
                                       mfxFrameSurface1 *surface_work,
                                       mfxFrameSurface1 **surface_out, mfxThreadTask *task);
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32, mfxU32);
    virtual mfxStatus FreeResources(mfxThreadTask, mfxStatus);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();
    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual void Release()
    {
        delete this;
    }
    virtual mfxStatus SetAuxParams(void*, int)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

protected:
    explicit Plugin(bool CreateByDispatcher);
    virtual ~Plugin();

    inline bool isSysIn()
    {
        return (m_vpar.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || ((m_vpar.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                && (m_osa.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY));
    }
    inline bool isSysOut()
    {
        return (m_vpar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            || ((m_vpar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                && (m_osa.Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY));
    }

    bool m_createdByDispatcher;
    MFXPluginAdapter<MFXVPPPlugin> m_adapter;
    bool m_bInit;
    mfxVideoParam m_vpar;
    mfxVideoParam m_vpar_init;
    mfxExtOpaqueSurfaceAlloc m_osa;
};

}