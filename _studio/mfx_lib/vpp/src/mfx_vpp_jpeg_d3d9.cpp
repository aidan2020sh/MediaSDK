/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA)
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include <algorithm>

#include "umc_va_base.h"
#include "libmfx_core.h"
#include "mfx_task.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_jpeg_d3d9.h"
#include <umc_automatic_mutex.h>

using namespace MfxHwVideoProcessing;

static void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )
{
    memset(&pMfxExecuteParams->targetSurface, 0, sizeof(mfxDrvSurface));
    pMfxExecuteParams->pRefSurfaces = NULL;
    pMfxExecuteParams->dstRects.clear(); /* NB! Due to STL container memset can not be used */
    memset(&pMfxExecuteParams->customRateData, 0, sizeof(CustomRateData));
    pMfxExecuteParams->targetTimeStamp = 0;
    pMfxExecuteParams->refCount = 0;
    pMfxExecuteParams->bkwdRefCount = 0;
    pMfxExecuteParams->fwdRefCount = 0;
    pMfxExecuteParams->iDeinterlacingAlgorithm = 0;
    pMfxExecuteParams->bFMDEnable = 0;
    pMfxExecuteParams->bDenoiseAutoAdjust = 0;
    pMfxExecuteParams->bDetailAutoAdjust = 0;
    pMfxExecuteParams->denoiseFactor = 0;
    pMfxExecuteParams->detailFactor = 0;
    pMfxExecuteParams->iTargetInterlacingMode = 0;
    pMfxExecuteParams->bEnableProcAmp = false;
    pMfxExecuteParams->Brightness = 0;
    pMfxExecuteParams->Contrast = 0;
    pMfxExecuteParams->Hue = 0;
    pMfxExecuteParams->Saturation = 0;
    pMfxExecuteParams->bSceneDetectionEnable = false;
    pMfxExecuteParams->bVarianceEnable = false;
    pMfxExecuteParams->bImgStabilizationEnable = false;
    pMfxExecuteParams->istabMode = 0;
    pMfxExecuteParams->bFRCEnable = false;
    pMfxExecuteParams->bComposite = false;
    pMfxExecuteParams->iBackgroundColor = 0;
    pMfxExecuteParams->statusReportID = 0;
    pMfxExecuteParams->bFieldWeaving = false;
    pMfxExecuteParams->iFieldProcessingMode = 0;
    pMfxExecuteParams->bCameraPipeEnabled = false;
    pMfxExecuteParams->bCameraBlackLevelCorrection =false;
    pMfxExecuteParams->bCameraGammaCorrection = false;
    pMfxExecuteParams->bCameraHotPixelRemoval = false;
    pMfxExecuteParams->bCameraWhiteBalaceCorrection = false;
    pMfxExecuteParams->bCCM = false;
    pMfxExecuteParams->bCameraLensCorrection = false;
    pMfxExecuteParams->rotation = 0;
    pMfxExecuteParams->scalingMode = MFX_SCALING_MODE_DEFAULT;
    pMfxExecuteParams->bEOS = false;
} /*void MemSetZero4mfxExecuteParams (mfxExecuteParams *pMfxExecuteParams )*/


enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

enum 
{
    CURRENT_TIME_STAMP              = 100000,
    FRAME_INTERVAL                  = 10000,
    ACCEPTED_DEVICE_ASYNC_DEPTH     = 0x06,

};

VideoVppJpegD3D9::VideoVppJpegD3D9(VideoCORE *core, bool isD3DToSys, bool isOpaq)
{
    m_pCore = core;
    m_isD3DToSys = isD3DToSys;
    
    m_isOpaq = isOpaq;

    m_taskId = 1;

} // VideoVppJpegD3D9::VideoVppJpegD3D9(VideoCORE *core)


VideoVppJpegD3D9::~VideoVppJpegD3D9()
{
    Close();

} // VideoVppJpegD3D9::~VideoVppJpegD3D9()

mfxStatus VideoVppJpegD3D9::Init(const mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_isD3DToSys)
    {
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        memset(&m_response, 0, sizeof(m_response));
        
        mfxU32 asyncDepth = (par->AsyncDepth ? par->AsyncDepth : m_pCore->GetAutoAsyncDepth());
        
        request.Info = par->mfx.FrameInfo;        
        request.NumFrameMin = mfxU16 (asyncDepth);
        request.NumFrameSuggested = request.NumFrameMin;
        request.Type = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;

        sts = m_pCore->AllocFrames(&request, &m_response, true);
        MFX_CHECK_STS(sts);

        m_surfaces.resize(m_response.NumFrameActual);

        for (mfxU32 i = 0; i < m_response.NumFrameActual; i++)
        {
            m_surfaces[i].Data.MemId = m_response.mids[i];
            memcpy_s(&m_surfaces[i].Info, sizeof(mfxFrameInfo), &request.Info, sizeof(mfxFrameInfo));
        }
    }

    m_pCore->GetVideoProcessing((mfxHDL*)&m_ddi);

    if (m_ddi == 0)
    {
#if defined (MFX_VA_WIN)
        sts = m_pCore->CreateVideoProcessing((mfxVideoParam *)par);
        MFX_CHECK_STS( sts );

        m_pCore->GetVideoProcessing((mfxHDL*)&m_ddi);
        if(m_ddi == 0)
        {
            return MFX_ERR_UNSUPPORTED;
        }
#else
        m_ddi = CreateVideoProcessing(m_pCore);
        if(m_ddi == 0)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        sts = m_ddi->CreateDevice(m_pCore, (mfxVideoParam *)par, false);
        MFX_CHECK_STS( sts );
#endif
    }

    mfxVppCaps caps;
    sts = m_ddi->QueryCapabilities(caps);
    MFX_CHECK_STS( sts );

    if(par->vpp.In.Width > caps.uMaxWidth || par->vpp.In.Height > caps.uMaxHeight ||
        par->vpp.Out.Width > caps.uMaxWidth || par->vpp.Out.Height > caps.uMaxHeight)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    if( par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        FALSE == caps.uFieldWeavingControl)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return sts;

} // mfxStatus VideoVppJpegD3D9::Init(void)

mfxStatus VideoVppJpegD3D9::Close()
{
    mfxStatus sts = MFX_ERR_NONE;

    m_taskId = 1;

#if !defined (MFX_VA_WIN)
    if (m_ddi) {
        delete m_ddi;
        m_ddi = NULL;
    }
#endif

    if(m_isD3DToSys)
    {
        m_surfaces.clear();

        if (m_response.NumFrameActual)
        {
            sts = m_pCore->FreeFrames(&m_response, true);
            MFX_CHECK_STS(sts);
        }

        m_isD3DToSys = false;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpegD3D9::Close()

mfxStatus VideoVppJpegD3D9::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurface, 
                                                  mfxFrameSurface1 *pOutputSurface, 
                                                  mfxU16* taskId)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair in;
    mfxHDLPair out;

    MfxHwVideoProcessing::mfxDrvSurface m_pExecuteSurface;
    MfxHwVideoProcessing::mfxExecuteParams m_executeParams;

    memset(&m_pExecuteSurface, 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));
    /* KW fix */
    //memset(&m_executeParams, 0, sizeof(MfxHwVideoProcessing::mfxExecuteParams));
    MemSetZero4mfxExecuteParams(&m_executeParams);

    if(m_isD3DToSys)
    {
        mfxI32 index = -1; 
        for (mfxU32 i = 0; i < m_surfaces.size(); i++)
        {
            if (!m_surfaces[i].Data.Locked)
            {
                index = i;
            }
        }
        if (index == -1)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        m_pCore->IncreasePureReference(m_surfaces[index].Data.Locked);
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index].Data.MemId, (mfxHDL *)&hdl));

        {
            UMC::AutomaticUMCMutex guard(m_guard);
            AssocIdx.insert(std::pair<mfxMemId, mfxI32>(pInputSurface->Data.MemId, index));
        }
    }
    else
    {
       if(m_isOpaq)
       {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
       else
       {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
    }

    out = hdl;

    // register output surface
    sts = m_ddi->Register(&out, 1, TRUE);
    MFX_CHECK_STS(sts);
    
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurface->Data.MemId, (mfxHDL *)&hdl));
    in = hdl;

    // register input surface
    sts = m_ddi->Register(&in, 1, TRUE);
    MFX_CHECK_STS(sts);
 
    // set input surface
    m_pExecuteSurface.hdl = static_cast<mfxHDLPair>(in);
    m_pExecuteSurface.frameInfo = pInputSurface->Info;

    // prepare the primary video sample
    m_pExecuteSurface.startTimeStamp = m_taskId * CURRENT_TIME_STAMP;
    m_pExecuteSurface.endTimeStamp  = m_taskId * CURRENT_TIME_STAMP + FRAME_INTERVAL;
    
    m_executeParams.targetTimeStamp = m_taskId * CURRENT_TIME_STAMP;

    m_executeParams.targetSurface.hdl = static_cast<mfxHDLPair>(out);
    m_executeParams.targetSurface.frameInfo = pOutputSurface->Info;

    m_executeParams.refCount = 1;
    m_executeParams.pRefSurfaces = &m_pExecuteSurface;

    m_executeParams.statusReportID = m_taskId;
    *taskId = m_taskId;
    m_taskId += 1;

    sts = m_ddi->Execute(&m_executeParams);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpegD3D9::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface, mfxU16* taskId)

mfxStatus VideoVppJpegD3D9::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, 
                                                  mfxFrameSurface1 *pInputSurfaceBottom, 
                                                  mfxFrameSurface1 *pOutputSurface, 
                                                  mfxU16* taskId)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair inTop,inBottom;
    mfxHDLPair out;

    MfxHwVideoProcessing::mfxDrvSurface m_pExecuteSurface[2];
    MfxHwVideoProcessing::mfxExecuteParams m_executeParams;

    memset(&m_pExecuteSurface[0], 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));
    memset(&m_pExecuteSurface[1], 0, sizeof(MfxHwVideoProcessing::mfxDrvSurface));
    /* KW fix */
    //memset(&m_executeParams, 0, sizeof(MfxHwVideoProcessing::mfxExecuteParams));
    MemSetZero4mfxExecuteParams(&m_executeParams);

    if(m_isD3DToSys)
    {
        mfxI32 index = -1; 
        for (mfxU32 i = 0; i < m_surfaces.size(); i++)
        {
            if (!m_surfaces[i].Data.Locked)
            {
                index = i;
            }
        }
        if (index == -1)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
        m_pCore->IncreasePureReference(m_surfaces[index].Data.Locked);
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index].Data.MemId, (mfxHDL *)&hdl));

        {
            UMC::AutomaticUMCMutex guard(m_guard);
            AssocIdx.insert(std::pair<mfxMemId, mfxI32>(pInputSurfaceTop->Data.MemId, index));
        }
    }
    else
    {
       if(m_isOpaq)
       {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
       else
       {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
    }

    out = hdl;

    // register output surface
    sts = m_ddi->Register(&out, 1, TRUE);
    MFX_CHECK_STS(sts);

    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurfaceTop->Data.MemId, (mfxHDL *)&hdl));
    inTop = hdl;
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurfaceBottom->Data.MemId, (mfxHDL *)&hdl));
    inBottom = hdl;

    // register input surfaces
    sts = m_ddi->Register(&inTop, 1, TRUE);
    MFX_CHECK_STS(sts);
    sts = m_ddi->Register(&inBottom, 1, TRUE);
    MFX_CHECK_STS(sts);

    // set input surface
    m_pExecuteSurface[0].hdl = static_cast<mfxHDLPair>(inBottom);
    m_pExecuteSurface[0].frameInfo = pInputSurfaceBottom->Info;
    m_pExecuteSurface[1].hdl = static_cast<mfxHDLPair>(inTop);
    m_pExecuteSurface[1].frameInfo = pInputSurfaceTop->Info;

    // prepare the primary video sample
    m_pExecuteSurface[0].startTimeStamp = m_taskId + 1;
    m_pExecuteSurface[0].endTimeStamp  = m_taskId + 1;
    m_pExecuteSurface[1].startTimeStamp = m_taskId;
    m_pExecuteSurface[1].endTimeStamp  = m_taskId;

    m_executeParams.targetTimeStamp = m_taskId;

    m_executeParams.targetSurface.hdl = static_cast<mfxHDLPair>(out);
    m_executeParams.targetSurface.frameInfo = pOutputSurface->Info;

    m_executeParams.refCount = 2;
    m_executeParams.pRefSurfaces = m_pExecuteSurface;

    m_executeParams.statusReportID = m_taskId;

    m_executeParams.bFieldWeaving = true;
    m_executeParams.bkwdRefCount = 1;

    *taskId = m_taskId;
    m_taskId += 1;

    sts = m_ddi->Execute(&m_executeParams);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpegD3D9::BeginHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, mfxFrameSurface1 *pInputSurfaceBottom, mfxFrameSurface1 *pOutputSurface, mfxU16* taskId)

mfxStatus VideoVppJpegD3D9::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair in;
    mfxHDLPair out;
    mfxI32 index = -1;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "EndHwJpegProcessing");

    if(m_isD3DToSys)
    {
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            std::map<mfxMemId, mfxI32>::iterator it;
            it = AssocIdx.find(pInputSurface->Data.MemId);
            index = it->second;
            AssocIdx.erase(it);
        }
        
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index].Data.MemId, (mfxHDL *)&hdl));
    }
    else
    {
       if(m_isOpaq)
       {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
       else
       {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
    }
    out = hdl;
    
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurface->Data.MemId, (mfxHDL *)&hdl));
    in = hdl;

    if (m_isD3DToSys)
    {
        sts = m_pCore->DoFastCopyWrapper(pOutputSurface, 
                                         MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                         &m_surfaces[index],
                                         MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                         );
        MFX_CHECK_STS(sts);
    }
    
    // unregister output surface
    sts = m_ddi->Register(&out, 1, FALSE);
    MFX_CHECK_STS(sts);

    // unregister input surface
    sts = m_ddi->Register(&in, 1, FALSE);
    MFX_CHECK_STS(sts);

    if(m_isD3DToSys)
    {
        m_pCore->DecreasePureReference(m_surfaces[index].Data.Locked);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpegD3D9::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurface, mfxFrameSurface1 *pOutputSurface)

mfxStatus VideoVppJpegD3D9::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, 
                                                mfxFrameSurface1 *pInputSurfaceBottom, 
                                                mfxFrameSurface1 *pOutputSurface)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxHDLPair hdl;
    mfxHDLPair inTop,inBottom;
    mfxHDLPair out;
    mfxI32 index = -1;

    if(m_isD3DToSys)
    {
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            std::map<mfxMemId, mfxI32>::iterator it;
            it = AssocIdx.find(pInputSurfaceTop->Data.MemId);
            index = it->second;
            AssocIdx.erase(it);
        }
        
        MFX_SAFE_CALL(m_pCore->GetFrameHDL(m_surfaces[index].Data.MemId, (mfxHDL *)&hdl));
    }
    else
    {
       if(m_isOpaq)
       {
            MFX_SAFE_CALL(m_pCore->GetFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
       else
       {
            MFX_SAFE_CALL(m_pCore->GetExternalFrameHDL(pOutputSurface->Data.MemId, (mfxHDL *)&hdl));
       }
    }
    out = hdl;
    
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurfaceTop->Data.MemId, (mfxHDL *)&hdl));
    inTop = hdl;
    MFX_SAFE_CALL(m_pCore->GetFrameHDL(pInputSurfaceBottom->Data.MemId, (mfxHDL *)&hdl));
    inBottom = hdl;

    if (m_isD3DToSys)
    {
        sts = m_pCore->DoFastCopyWrapper(pOutputSurface, 
                                         MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                         &m_surfaces[index],
                                         MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                         );
        MFX_CHECK_STS(sts);
    }
    
    // unregister output surface
    sts = m_ddi->Register(&out, 1, FALSE);
    MFX_CHECK_STS(sts);

    // unregister input surface
    sts = m_ddi->Register(&inTop, 1, FALSE);
    MFX_CHECK_STS(sts);
    sts = m_ddi->Register(&inBottom, 1, FALSE);
    MFX_CHECK_STS(sts);

    if(m_isD3DToSys)
    {
        m_pCore->DecreasePureReference(m_surfaces[index].Data.Locked);
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoVppJpegD3D9::EndHwJpegProcessing(mfxFrameSurface1 *pInputSurfaceTop, mfxFrameSurface1 *pInputSurfaceBottom, mfxFrameSurface1 *pOutputSurface)

mfxStatus VideoVppJpegD3D9::QueryTaskRoutine(mfxU16 taskId)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_ddi->QueryTaskStatus(taskId);
    
    return sts;

} // mfxStatus VideoVppJpegD3D9::QueryTask(mfxU16 taskId)

#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
#endif // MFX_VA
#endif // MFX_ENABLE_VPP
/* EOF */
