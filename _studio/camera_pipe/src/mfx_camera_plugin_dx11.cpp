/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx11.cpp

\* ****************************************************************************** */
#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx11.h"

#ifdef CAMP_PIPE_ITT
#include "ittnotify.h"
__itt_domain* CamPipeDX11     = __itt_domain_create(L"CamPipeDX11");
__itt_string_handle* GPUCOPY  = __itt_string_handle_create(L"GPUCOPY");
__itt_string_handle* DDIEXEC  = __itt_string_handle_create(L"DDIEXEC");
__itt_string_handle* STALL    = __itt_string_handle_create(L"STALL");
__itt_string_handle* ASYNC    = __itt_string_handle_create(L"ASYNC");
__itt_string_handle* COMPLETE = __itt_string_handle_create(L"COMPLETE");
#endif

mfxStatus D3D11CameraProcessor::Init(CameraParams *CameraParams)
{
    MFX_CHECK_NULL_PTR1(CameraParams);
    mfxStatus sts = MFX_ERR_NONE;
    if ( ! m_core )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if ( MFX_HW_D3D11 != m_core->GetVAType() )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_CameraParams = *CameraParams;
    m_ddi.reset( new D3D11VideoProcessor() );
    if (0 == m_ddi.get())
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    m_params = CameraParams->par;
    sts = m_ddi->CreateDevice( m_core, &m_params, false);
    MFX_CHECK_STS( sts );

    mfxVppCaps caps = {0};
    sts = m_ddi->QueryCapabilities( caps );
    MFX_CHECK_STS( sts );

    m_AsyncDepth = ( CameraParams->par.AsyncDepth ? CameraParams->par.AsyncDepth : MFX_CAMERA_DEFAULT_ASYNCDEPTH );

    if ( m_executeParams )
        delete [] m_executeParams; 
    if ( m_executeSurf )
        delete [] m_executeSurf;

    m_executeParams = new MfxHwVideoProcessing::mfxExecuteParams[m_AsyncDepth];
    ZeroMemory(m_executeParams, sizeof(MfxHwVideoProcessing::mfxExecuteParams)*m_AsyncDepth);
    m_executeSurf   = new MfxHwVideoProcessing::mfxDrvSurface[m_AsyncDepth];
    ZeroMemory(m_executeParams, sizeof(MfxHwVideoProcessing::mfxDrvSurface)*m_AsyncDepth);

    mfxFrameAllocRequest request;
    request.Info        = m_params.vpp.In;

    // Initial frameinfo contains just R16 that should be updated to the
    // internal FourCC representing
    request.Info.FourCC = BayerToFourCC(CameraParams->Caps.BayerPatternType);
    request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_INTERNAL_FRAME;
    request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth;

    sts = m_InSurfacePool->AllocateSurfaces(m_core, request);
    MFX_CHECK_STS( sts );

    m_systemMemOut = false;
    if ( CameraParams->par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        m_systemMemOut = true;
        // Output is in system memory. Need to allocate temporary video surf
        mfxFrameAllocRequest request;
        request.Info        = m_params.vpp.Out;
        request.Type        = MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_INTERNAL_FRAME;
        request.NumFrameMin = request.NumFrameSuggested = m_AsyncDepth; // Fixme
        sts = m_OutSurfacePool->AllocateSurfaces(m_core, request);
        MFX_CHECK_STS( sts );
    }

    return sts;
}

mfxStatus D3D11CameraProcessor::AsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts;
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, ASYNC);
#endif    
    m_core->IncreaseReference(&(pParam->surf_out->Data));
    m_core->IncreaseReference(&(pParam->surf_in->Data));

    MfxHwVideoProcessing::mfxDrvSurface    tmpSurf   = {0};
    mfxU32 surfInIndex;
    sts = PreWorkInSurface(pParam->surf_in, &surfInIndex, &tmpSurf);
    MFX_CHECK_STS(sts);

    mfxU32 surfOutIndex = 0;
    sts = PreWorkOutSurface(pParam->surf_out, &surfOutIndex, &m_executeParams[surfInIndex]);
    MFX_CHECK_STS(sts);

    m_executeParams[surfInIndex].bCameraPipeEnabled  = true;
    if ( pParam->Caps.bBlackLevelCorrection )
    {
        m_executeParams[surfInIndex].bCameraBlackLevelCorrection = true;
        m_executeParams[surfInIndex].CameraBlackLevel.uB  = (mfxU32)pParam->BlackLevelParams.BlueLevel;
        m_executeParams[surfInIndex].CameraBlackLevel.uG0 = (mfxU32)pParam->BlackLevelParams.GreenBottomLevel;
        m_executeParams[surfInIndex].CameraBlackLevel.uG1 = (mfxU32)pParam->BlackLevelParams.GreenTopLevel;
        m_executeParams[surfInIndex].CameraBlackLevel.uR  = (mfxU32)pParam->BlackLevelParams.RedLevel;
    }

    if ( pParam->Caps.bWhiteBalance )
    {
        m_executeParams[surfInIndex].bCameraWhiteBalaceCorrection = true;
        m_executeParams[surfInIndex].CameraWhiteBalance.fB  = (mfxF32)pParam->WBparams.BlueCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fG0 = (mfxF32)pParam->WBparams.GreenBottomCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fG1 = (mfxF32)pParam->WBparams.GreenTopCorrection;
        m_executeParams[surfInIndex].CameraWhiteBalance.fR  = (mfxF32)pParam->WBparams.RedCorrection;
    }

    if ( pParam->Caps.bHotPixel )
    {
        m_executeParams[surfInIndex].bCameraHotPixelRemoval = true;
        m_executeParams[surfInIndex].CameraHotPixel.uPixelCountThreshold = pParam->HPParams.PixelCountThreshold;
        m_executeParams[surfInIndex].CameraHotPixel.uPixelThresholdDifference = pParam->HPParams.PixelThresholdDifference;
    }

    if ( pParam->Caps.bColorConversionMatrix )
    {
        m_executeParams[surfInIndex].bCCM = true;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                m_executeParams[surfInIndex].CCMParams.CCM[i][j] = (mfxF32)pParam->CCMParams.CCM[i][j];
            }
        }
    }

    if ( pParam->Caps.bLensCorrection )
    {
        m_executeParams[surfInIndex].bCameraLensCorrection = true;
        for(int i = 0; i < 3; i++)
        {
            m_executeParams[surfInIndex].CameraLensCorrection.a[i] = pParam->LensParams.a[i];
            m_executeParams[surfInIndex].CameraLensCorrection.b[i] = pParam->LensParams.b[i];
            m_executeParams[surfInIndex].CameraLensCorrection.c[i] = pParam->LensParams.c[i];
            m_executeParams[surfInIndex].CameraLensCorrection.d[i] = pParam->LensParams.d[i];
        }
    }

    if ( pParam->Caps.bForwardGammaCorrection )
    {
        m_executeParams[surfInIndex].bCameraGammaCorrection = true;
        for(int i = 0; i < 64; i++)
        {
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].PixelValue = pParam->GammaParams.gamma_lut.gammaPoints[i];
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].BlueChannelCorrectedValue  = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].GreenChannelCorrectedValue = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            m_executeParams[surfInIndex].CameraForwardGammaCorrection.Segment[i].RedChannelCorrectedValue   = pParam->GammaParams.gamma_lut.gammaCorrect[i];
        }
    }

    if ( pParam->Caps.bVignetteCorrection )
    {
        MFX_CHECK_NULL_PTR1(pParam->VignetteParams.pCorrectionMap);
        m_executeParams[surfInIndex].bCameraVignetteCorrection = 1;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Height = pParam->VignetteParams.Height;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Width  = pParam->VignetteParams.Width;
        m_executeParams[surfInIndex].CameraVignetteCorrection.Stride = pParam->VignetteParams.Stride;
        m_executeParams[surfInIndex].CameraVignetteCorrection.pCorrectionMap = (CameraVignetteCorrectionElem *)pParam->VignetteParams.pCorrectionMap;
    }

    m_executeParams[surfInIndex].refCount       = 1;
    m_executeParams[surfInIndex].bkwdRefCount   = m_executeParams[surfInIndex].fwdRefCount = 0;
    m_executeParams[surfInIndex].statusReportID = surfInIndex;
    m_executeSurf[surfInIndex].bExternal = tmpSurf.bExternal;
    m_executeSurf[surfInIndex].endTimeStamp = tmpSurf.endTimeStamp;
    memcpy_s(&m_executeSurf[surfInIndex].frameInfo, sizeof(mfxFrameInfo), &tmpSurf.frameInfo, sizeof(mfxFrameInfo));
    m_executeSurf[surfInIndex].hdl = tmpSurf.hdl;
    m_executeSurf[surfInIndex].memId = tmpSurf.memId;
    m_executeSurf[surfInIndex].startTimeStamp = tmpSurf.startTimeStamp;
    m_executeParams[surfInIndex].pRefSurfaces = &m_executeSurf[surfInIndex];

    pParam->surfInIndex  = surfInIndex;
    pParam->surfOutIndex = surfOutIndex;
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
    return sts;
}

mfxStatus D3D11CameraProcessor::CompleteRoutine(AsyncParams * pParam)
{
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, COMPLETE);
#endif
    MFX_CHECK_NULL_PTR1(pParam);
    mfxU32 ddiIndex = pParam->surfInIndex;
    mfxStatus sts   = MFX_ERR_NONE;

    {
        // DDI execs
        sts = m_ddi->Execute(&m_executeParams[ddiIndex]);
    }

    if ( m_systemMemOut )
    {
        mfxFrameSurface1 OutSurf = {0};
        OutSurf.Data.MemId = m_OutSurfacePool->mids[pParam->surfOutIndex];
        OutSurf.Info       = pParam->surf_out->Info;

        // [1] Copy from system mem to the internal video frame
        sts = m_core->DoFastCopyWrapper(pParam->surf_out,
                                        MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                        &OutSurf,
                                        MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                        );
        MFX_CHECK_STS(sts);
    }

    {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, STALL);
#endif
        UMC::AutomaticUMCMutex guard(m_guard_exec);
        m_InSurfacePool->Unlock(pParam->surfInIndex );
        m_OutSurfacePool->Unlock(pParam->surfOutIndex);
        m_core->DecreaseReference(&(pParam->surf_out->Data));
        m_core->DecreaseReference(&(pParam->surf_in->Data));
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
}
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
    return sts;
}

mfxStatus D3D11CameraProcessor::CheckIOPattern(mfxU16  IOPattern)
{
    if (IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if (IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D11CameraProcessor::PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxExecuteParams *params)
{
    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId  memIdOut;
    bool      bExternal = true;

    if ( m_systemMemOut )
    {
        // Request surface from internal pool
        memIdOut = AcquireResource(*m_OutSurfacePool, poolIndex);
        sts = m_core->GetFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = false;
    }
    else
    {
        memIdOut = surf->Data.MemId;
        // [1] Get external surface handle
        sts = m_core->GetExternalFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = true;
    }

    // [2] Get surface handle
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [3] Fill in Drv surfaces
    params->targetSurface.memId     = memIdOut;
    params->targetSurface.bExternal = bExternal;
    params->targetSurface.hdl       = static_cast<mfxHDLPair>(hdl);
    params->targetSurface.frameInfo = surf->Info;

    return sts;
}

mfxStatus D3D11CameraProcessor::PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, mfxDrvSurface *DrvSurf)
{
    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId memIdIn;
    {
        // Request surface from internal pool
        UMC::AutomaticUMCMutex guard(m_guard);
        while( (memIdIn = AcquireResource(*m_InSurfacePool, poolIndex)) == 0 )
        {
#ifdef CAMP_PIPE_ITT
    __itt_task_begin(CamPipeDX11, __itt_null, __itt_null, STALL);
#endif
            vm_time_sleep(10);
#ifdef CAMP_PIPE_ITT
    __itt_task_end(CamPipeDX11);
#endif
        }
    }
    mfxFrameSurface1 InSurf = {0};

    InSurf.Data.MemId = memIdIn;
    InSurf.Info       = surf->Info;
    InSurf.Info.FourCC =  BayerToFourCC(m_CameraParams.Caps.BayerPatternType);

    // [1] Copy from system mem to the internal video frame
    sts = m_core->DoFastCopyWrapper(&InSurf,
                                    MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET,
                                    surf,
                                    MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY);
    MFX_CHECK_STS(sts);

    // [2] Get surface handle
    sts = m_core->GetFrameHDL(memIdIn, (mfxHDL *)&hdl);
    MFX_CHECK_STS(sts);

    // [3] Register surfaece. Not needed really for DX11
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [4] Fill in Drv surfaces
    DrvSurf->hdl       = static_cast<mfxHDLPair>(hdl);
    DrvSurf->bExternal = false;
    DrvSurf->frameInfo = InSurf.Info;
    DrvSurf->memId     = memIdIn;

    return sts;
}

mfxU32 D3D11CameraProcessor::BayerToFourCC(mfxU32 Bayer)
{
    switch(Bayer)
    {
    case BGGR:
        return MFX_FOURCC_R16_BGGR;
    case RGGB:
        return MFX_FOURCC_R16_RGGB;
    case GRBG:
        return MFX_FOURCC_R16_GRBG;
    case GBRG:
        return MFX_FOURCC_R16_GBRG;
    }
    return 0;
}
#endif