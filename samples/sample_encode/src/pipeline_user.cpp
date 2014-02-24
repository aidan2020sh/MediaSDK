//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#include "pipeline_user.h"
#include "sysmem_allocator.h"

mfxStatus CUserPipeline::InitRotateParam(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);

    MSDK_ZERO_MEMORY(m_pluginVideoParams);

    m_pluginVideoParams.AsyncDepth = m_nAsyncDepth; // the maximum number of tasks that can be submitted before any task execution finishes
    m_pluginVideoParams.vpp.In.FourCC = MFX_FOURCC_NV12;
    m_pluginVideoParams.vpp.In.Width = m_pluginVideoParams.vpp.In.CropW = pInParams->nWidth;
    m_pluginVideoParams.vpp.In.Height = m_pluginVideoParams.vpp.In.CropH = pInParams->nHeight;
    m_pluginVideoParams.vpp.Out.FourCC = MFX_FOURCC_NV12;
    m_pluginVideoParams.vpp.Out.Width = m_pluginVideoParams.vpp.Out.CropW = pInParams->nWidth;
    m_pluginVideoParams.vpp.Out.Height = m_pluginVideoParams.vpp.Out.CropH = pInParams->nHeight;

    m_RotateParams.Angle = pInParams->nRotationAngle;

    return MFX_ERR_NONE;
}

mfxStatus CUserPipeline::AllocFrames()
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameAllocRequest EncRequest, RotateRequest;

    mfxU16 nEncSurfNum = 0; // number of frames at encoder input (rotate output)
    mfxU16 nRotateSurfNum = 0; // number of frames at rotate input

    MSDK_ZERO_MEMORY(EncRequest);

    sts = m_pmfxENC->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Rotation plugin requires 1 frame at input to produce 1 frame at output.
    mfxU16 nRotateReqIn, nRotateReqOut;
    nRotateReqIn = nRotateReqOut = 1;

    // If surfaces are shared by 2 components, c1 and c2. NumSurf = c2_in + (c1_out - 1) + extra
    // When surfaces are shared 1 surface at first component output contains output frame that goes to next component input
    nEncSurfNum = EncRequest.NumFrameSuggested + (nRotateReqOut - 1) + (m_nAsyncDepth - 1);

    // The number of surfaces for plugin input - so that plugin can work at async depth = m_nAsyncDepth
    nRotateSurfNum = nRotateReqIn + (m_nAsyncDepth - 1);

    // prepare allocation requests
    EncRequest.NumFrameMin = EncRequest.NumFrameSuggested = nEncSurfNum;
    RotateRequest.NumFrameMin = RotateRequest.NumFrameSuggested = nRotateSurfNum;
        
    mfxU16 mem_type = MFX_MEMTYPE_EXTERNAL_FRAME;
    mem_type |= (D3D9_MEMORY == m_memType) ? (mfxU16)MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET : (mfxU16)MFX_MEMTYPE_SYSTEM_MEMORY;
    EncRequest.Type = RotateRequest.Type = mem_type;

    EncRequest.Type |= MFX_MEMTYPE_FROM_ENCODE;    
    RotateRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT; // THIS IS A WORKAROUND, NEED TO ADJUST ALLOCATOR

    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
    MSDK_MEMCPY_VAR(RotateRequest.Info, &(m_pluginVideoParams.vpp.In), sizeof(mfxFrameInfo));

    // alloc frames for encoder input
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // alloc frames for rotate input
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &(RotateRequest), &m_PluginResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare mfxFrameSurface1 array for components
    m_pEncSurfaces = new mfxFrameSurface1 [nEncSurfNum];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);
    m_pPluginSurfaces = new mfxFrameSurface1 [nRotateSurfNum];
    MSDK_CHECK_POINTER(m_pPluginSurfaces, MFX_ERR_MEMORY_ALLOC);

    for (int i = 0; i < nEncSurfNum; i++)
    {
        MSDK_ZERO_MEMORY(m_pEncSurfaces[i]);
        MSDK_MEMCPY_VAR(m_pEncSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
        if (D3D9_MEMORY == m_memType)
        {
            // external allocator used - provide just MemIds
            m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];
        }
        else
        {
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_EncResponse.mids[i], &(m_pEncSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    for (int i = 0; i < nRotateSurfNum; i++)
    {
        MSDK_ZERO_MEMORY(m_pPluginSurfaces[i]);
        MSDK_MEMCPY_VAR(m_pPluginSurfaces[i].Info, &(m_pluginVideoParams.vpp.In), sizeof(mfxFrameInfo));
        if (D3D9_MEMORY == m_memType)
        {
            // external allocator used - provide just MemIds
            m_pPluginSurfaces[i].Data.MemId = m_PluginResponse.mids[i];
        }
        else
        {
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, m_PluginResponse.mids[i], &(m_pPluginSurfaces[i].Data));
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    return MFX_ERR_NONE;
}

void CUserPipeline::DeleteFrames()
{
    MSDK_SAFE_DELETE_ARRAY(m_pPluginSurfaces);

    CEncodingPipeline::DeleteFrames();
}

CUserPipeline::CUserPipeline() : CEncodingPipeline()
{
    m_pPluginSurfaces = NULL;
    m_PluginModule = NULL;
    m_pusrPlugin = NULL;
    MSDK_ZERO_MEMORY(m_PluginResponse);

    m_MVCflags = MVC_DISABLED;
}

CUserPipeline::~CUserPipeline()
{
    Close();
}

mfxStatus CUserPipeline::Init(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;

    m_PluginModule = msdk_so_load(pParams->strPluginDLLPath);
    MSDK_CHECK_POINTER(m_PluginModule, MFX_ERR_NOT_FOUND);

    PluginModuleTemplate::fncCreateGenericPlugin pCreateFunc = (PluginModuleTemplate::fncCreateGenericPlugin)msdk_so_get_addr(m_PluginModule, "mfxCreateGenericPlugin");

    MSDK_CHECK_POINTER(pCreateFunc, MFX_ERR_NOT_FOUND);

    m_pusrPlugin = (*pCreateFunc)();
    MSDK_CHECK_POINTER(m_pusrPlugin, MFX_ERR_NOT_FOUND);

    // prepare input file reader
    sts = m_FileReader.Init(pParams->strSrcFile,
                            pParams->ColorFormat,
                            pParams->numViews,
                            pParams->srcFileBuff);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // set memory type
    m_memType = pParams->memType;

    // prepare output file writer
    sts = InitFileWriters(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxIMPL impl = pParams->bUseHWLib ? MFX_IMPL_HARDWARE : MFX_IMPL_SOFTWARE;
    
    // create a session for the second vpp and encode
    sts = m_mfxSession.Init(impl, NULL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // we check if codec is distributed as a mediasdk plugin and load it if yes
    // else if codec is not in the list of mediasdk plugins, we assume, that it is supported inside mediasdk library
    if (pParams->bUseHWLib) {
           m_pUID = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_HW, pParams->CodecId);
        } else {
           m_pUID = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId);
        }

    if (m_pUID)
    {
          sts = MFXVideoUSER_Load(m_mfxSession, &(m_pUID->mfx), 1);
          if (MFX_ERR_NONE != sts)
           {
                printf("error: failed to load Media SDK plugin:\n");
                printf("error:   GUID = { 0x%08x, 0x%04x, 0x%04x, { 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x } }\n",
                       m_pUID->guid.data1, m_pUID->guid.data2, m_pUID->guid.data3,
                       m_pUID->guid.data4[0], m_pUID->guid.data4[1], m_pUID->guid.data4[2], m_pUID->guid.data4[3],
                       m_pUID->guid.data4[4], m_pUID->guid.data4[5], m_pUID->guid.data4[6], m_pUID->guid.data4[7]);
                printf("error:   UID (mfx raw) = %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x\n",
                       m_pUID->raw[0],  m_pUID->raw[1],  m_pUID->raw[2],  m_pUID->raw[3],
                       m_pUID->raw[4],  m_pUID->raw[5],  m_pUID->raw[6],  m_pUID->raw[7],
                       m_pUID->raw[8],  m_pUID->raw[9],  m_pUID->raw[10], m_pUID->raw[11],
                       m_pUID->raw[12], m_pUID->raw[13], m_pUID->raw[14], m_pUID->raw[15]);
                const msdk_char* str = msdkGetPluginName(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId);
                msdk_printf(MSDK_STRING("error:   name = %s\n"), str);
                msdk_printf(MSDK_STRING("error:   You may need to install this plugin separately!\n"));
            }
            else
            {
               msdk_printf(MSDK_STRING("Plugin '%s' loaded successfully\n"), msdkGetPluginName(MSDK_VENC | MSDK_IMPL_SW, pParams->CodecId));
            }
    }

    
    // create encoder
    m_pmfxENC = new MFXVideoENCODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = InitRotateParam(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create and init frame allocator
    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_nAsyncDepth = 4; // this number can be tuned for better performance

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    // register plugin callbacks in Media SDK
    mfxPlugin plg = make_mfx_plugin_adapter(m_pusrPlugin);
    sts = MFXVideoUSER_Register(m_mfxSession, 0, &plg);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // need to call Init after registration because mfxCore interface is needed
    sts = m_pusrPlugin->Init(&m_pluginVideoParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pusrPlugin->SetAuxParams(&m_RotateParams, sizeof(m_RotateParams));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CUserPipeline::Close()
{
    MFXVideoUSER_Unregister(m_mfxSession, 0);

    CEncodingPipeline::Close();

    MSDK_SAFE_DELETE(m_pusrPlugin);
    if (m_PluginModule)
    {
        msdk_so_free(m_PluginModule);
        m_PluginModule = NULL;
    }
}

mfxStatus CUserPipeline::ResetMFXComponents(sInputParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxENC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxENC->Init(&m_mfxEncParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    sts = m_TaskPool.Init(&m_mfxSession, m_FileWriters.first, m_nAsyncDepth * 2, nEncodedDataBufferSize, m_FileWriters.second);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CUserPipeline::Run()
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    sTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0; // index of free surface for encoder input
    mfxU16 nRotateSurfIdx = 0; // ~ for rotation plugin input

    mfxSyncPoint RotateSyncPoint = NULL; // ~ with rotation plugin call
  
    sts = MFX_ERR_NONE;

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        nRotateSurfIdx = GetFreeSurface(m_pPluginSurfaces, m_PluginResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nRotateSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

        mfxFrameSurface1 *rot_surf = &m_pPluginSurfaces[nRotateSurfIdx];
        if (D3D9_MEMORY == m_memType)
        {
            sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, rot_surf->Data.MemId, &(rot_surf->Data));
            MSDK_BREAK_ON_ERROR(sts);
        }

        sts = m_FileReader.LoadNextFrame(&m_pPluginSurfaces[nRotateSurfIdx]);
        MSDK_BREAK_ON_ERROR(sts);

        if (D3D9_MEMORY == m_memType)
        {
            sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, rot_surf->Data.MemId, &(rot_surf->Data));
            MSDK_BREAK_ON_ERROR(sts);        
        }

        nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

        // rotation
        for(;;)
        {
            mfxHDL h1, h2;
            h1 = &m_pPluginSurfaces[nRotateSurfIdx];
            h2 = &m_pEncSurfaces[nEncSurfIdx];
            sts = MFXVideoUSER_ProcessFrameAsync(m_mfxSession, &h1, 1, &h2, 1, &RotateSyncPoint);

            if (MFX_WRN_DEVICE_BUSY == sts)
            {
                MSDK_SLEEP(1); // just wait and then repeat the same call
            }
            else
            {
                break;
            }
        }

        MSDK_BREAK_ON_ERROR(sts);

        // save the id of preceding rotate task which will produce input data for the encode task
        if (RotateSyncPoint)
        {
            pCurrentTask->DependentVppTasks.push_back(RotateSyncPoint);
            RotateSyncPoint = NULL;
        }

        for (;;)
        {
            sts = m_pmfxENC->EncodeFrameAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else
            {
                break;
            }
        }
    }

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // rotate plugin doesn't buffer frames
    // loop to get buffered frames from encoder
    while (MFX_ERR_NONE <= sts)
    {
        // get a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        for (;;)
        {
            sts = m_pmfxENC->EncodeFrameAsync(NULL, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else
            {
                break;
            }
        }
        MSDK_BREAK_ON_ERROR(sts);
    }

    // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
    // indicates that there are no more buffered frames
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // synchronize all tasks that are left in task pool
    while (MFX_ERR_NONE == sts)
    {
        sts = m_TaskPool.SynchronizeFirstTask();
    }

    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with,
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void CUserPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("\nPipeline with rotation plugin"));
    msdk_printf(MSDK_STRING("\nNOTE: Some of command line options may have been ignored as non-supported for this pipeline. For details see readme-encode.rtf.\n\n"));

    CEncodingPipeline::PrintInfo();
}
