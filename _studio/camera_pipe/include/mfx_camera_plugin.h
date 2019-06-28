// Copyright (c) 2014-2019 Intel Corporation
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

#if !defined(__MFX_CAMERA_PLUGIN_INCLUDED__)
#define __MFX_CAMERA_PLUGIN_INCLUDED__

#include <stdio.h>
#include <string.h>
#include <memory>
#include <iostream>
#include <ippi.h>

#include "mfx_camera_plugin_utils.h"
#include "mfx_camera_plugin_cpu.h"
#include "mfx_camera_plugin_cm.h"
#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx11.h"
#include "mfx_camera_plugin_dx9.h"
#endif


#define MFX_CAMERA_DEFAULT_ASYNCDEPTH 3
#define MAX_CAMERA_SUPPORTED_WIDTH  16280
#define MAX_CAMERA_SUPPORTED_HEIGHT 15952

enum CameraFallback{
    FALLBACK_NONE = 0,
    FALLBACK_CPU  = 1,
    FALLBACK_CM   = 2,
};

#if defined( AS_VPP_PLUGIN ) || defined(AS_CAMERA_PLUGIN)
class MFXCamera_Plugin : public MFXVPPPlugin
{
public:
    static const mfxPluginUID g_Camera_PluginGuid;

    virtual mfxStatus PluginInit(mfxCoreInterface *core);
    virtual mfxStatus PluginClose();
    virtual mfxStatus GetPluginParam(mfxPluginParam *par);
    virtual mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task);
    virtual mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *, mfxFrameSurface1 *, mfxFrameSurface1 **, mfxThreadTask *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a);
    virtual mfxStatus FreeResources(mfxThreadTask task, mfxStatus );
    virtual mfxStatus Query(mfxVideoParam *, mfxVideoParam *);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual void Release()
    {
        delete this;
    }

    static MFXVPPPlugin* Create() {
        return new MFXCamera_Plugin(false);
    }

    static mfxStatus CreateByDispatcher(mfxPluginUID guid, mfxPlugin* mfxPlg)
    {
        if (guid != g_Camera_PluginGuid)
        {
            return MFX_ERR_NOT_FOUND;
        }

        MFXCamera_Plugin* tmp_pplg = 0;
        try
        {
            tmp_pplg = new MFXCamera_Plugin(false);
        }
        catch(std::bad_alloc&)
        {
            return MFX_ERR_MEMORY_ALLOC;;
        }
        catch(...)
        {
            return MFX_ERR_UNKNOWN;
        }

        try
        {
            tmp_pplg->m_adapter.reset(new MFXPluginAdapter<MFXVPPPlugin> (tmp_pplg));
        }
        catch(std::bad_alloc&)
        {
            delete tmp_pplg;
            return MFX_ERR_MEMORY_ALLOC;
        }

        *mfxPlg = (mfxPlugin)*tmp_pplg->m_adapter;
        tmp_pplg->m_createdByDispatcher = true;

        return MFX_ERR_NONE;
    }

    virtual mfxStatus SetAuxParams(void* , int )
    {
        return MFX_ERR_UNKNOWN;
    }

protected:
    MFXCamera_Plugin(bool CreateByDispatcher);
    virtual ~MFXCamera_Plugin();
    std::unique_ptr<MFXPluginAdapter<MFXVPPPlugin> > m_adapter;

    UMC::Mutex m_guard1;

#ifdef WRITE_CAMERA_LOG
    UMC::Mutex m_log_guard;
#endif

    mfxCoreInterface*   m_pmfxCore;

    mfxSession          m_session;
    mfxPluginParam      m_PluginParam;
    bool                m_createdByDispatcher;
    CameraProcessor *   m_CameraProcessor;

    mfxStatus           CameraAsyncRoutine(AsyncParams *pParam);
    mfxStatus           CompleteCameraAsyncRoutine(AsyncParams *pParam);

    static mfxStatus    CameraRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus    CompleteCameraRoutine(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus           ProcessExtendedBuffers(mfxVideoParam *par);

    mfxStatus           WaitForActiveThreads();

    mfxVideoParam       m_mfxVideoParam;

    // Width and height provided at Init
    mfxU16              m_InitWidth;
    mfxU16              m_InitHeight;

    VideoCORE*          m_core;
    eMFXHWType          m_platform;

    bool                m_isInitialized;
    bool                m_useSW;

    mfxU16              m_activeThreadCount;

    mfxCameraCaps       m_Caps;

    //Filter specific parameters
    CameraPipeDenoiseParams            m_DenoiseParams;
    CameraPipeHotPixelParams           m_HPParams;
    CameraPipeWhiteBalanceParams       m_WBparams;
    CameraPipeTotalColorControlParams  m_TCCParams;
    CameraPipeRGBToYUVParams           m_RGBToYUVParams;
    CameraPipeForwardGammaParams       m_GammaParams;
    CameraPipeVignetteParams           m_VignetteParams;
    CameraPipePaddingParams            m_PaddingParams;
    CameraPipe3DLUTParams              m_3DLUTParams;
    CameraPipeBlackLevelParams         m_BlackLevelParams;
    CameraPipe3x3ColorConversionParams m_CCMParams;
    CameraPipeLensCorrectionParams     m_LensParams;
    CameraParams                       m_PipeParams;
    CameraFallback                     m_fallback;

    mfxU32                       m_InputBitDepth;
    mfxU16                       m_nTilesVer;                       
    mfxU16                       m_nTilesHor;

    mfxU16  BayerPattern_API2CM(mfxU16 api_bayer_type)
    {
        switch(api_bayer_type)
        {
          case MFX_CAM_BAYER_BGGR:
            return BGGR;
          case MFX_CAM_BAYER_GBRG:
            return GBRG;
          case MFX_CAM_BAYER_GRBG:
            return GRBG;
          case MFX_CAM_BAYER_RGGB:
            return RGGB;
        }

        return 0;
    };

private:


};
#endif //#if defined( AS_VPP_PLUGIN )

#if defined(_WIN32) || defined(_WIN64)
#define MSDK_PLUGIN_API(ret_type) extern "C" __declspec(dllexport)  ret_type __cdecl
#else
#define MSDK_PLUGIN_API(ret_type) extern "C"  ret_type  __cdecl
#endif

#endif  // __MFX_CAMERA_PLUGIN_INCLUDED__
