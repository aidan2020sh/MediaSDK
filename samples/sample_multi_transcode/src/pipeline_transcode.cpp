//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
//
//
//*/
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "pipeline_transcode.h"
#include "transcode_utils.h"
#include "sample_utils.h"
#include "mfx_vpp_plugin.h"

#include "../../sample_user_modules/plugin_api/plugin_loader.h"

using namespace TranscodingSample;

mfxU32 MSDK_THREAD_CALLCONVENTION TranscodingSample::ThranscodeRoutine(void   *pObj)
{
    mfxU64 start = TranscodingSample::GetTick();
    ThreadTranscodeContext *pContext = (ThreadTranscodeContext*)pObj;
    pContext->transcodingSts = MFX_ERR_NONE;
    for(;;)
    {                
        while (MFX_ERR_NONE == pContext->transcodingSts)
        {
            pContext->transcodingSts = pContext->pPipeline->Run();
        }
        if (MFX_ERR_MORE_DATA == pContext->transcodingSts)
        {
            // get next coded data
            mfxStatus bs_sts = pContext->pBSProcessor->PrepareBitstream();
            // we can continue transcoding if input bistream presents
            if (MFX_ERR_NONE == bs_sts)
            {
                MSDK_IGNORE_MFX_STS(pContext->transcodingSts, MFX_ERR_MORE_DATA);
                continue;
            }
            // no need more data, need to get last transcoded frames
            else if (MFX_ERR_MORE_DATA == bs_sts) 
            {
                pContext->transcodingSts = pContext->pPipeline->FlushLastFrames();             
            }           
        }

        break; // exit loop
    }
    
    MSDK_IGNORE_MFX_STS(pContext->transcodingSts, MFX_WRN_VALUE_NOT_CHANGED);

    pContext->working_time = TranscodingSample::GetTime(start);
    pContext->numTransFrames = pContext->pPipeline->GetProcessFrames();

    return 0;
} // mfxU32 __stdcall ThranscodeRoutine(void   *pObj)

// set structure to define values
sInputParams::sInputParams():bIsJoin(false),
                             priority(MFX_PRIORITY_NORMAL),
                             libType(MFX_IMPL_SOFTWARE),
                             bIsPerf(false),
                             EncodeId(0),
                             DecodeId(0),                             
                             nTargetUsage(),
                             dFrameRate(0),
                             nBitRate(0),
                             nQuality(0),
                             nDstWidth(0),
                             nDstHeight(0),
                             nAsyncDepth(0),
                             nTimeout(0),
                             eMode(Native),
                             FrameNumberPreference(0),
                             MaxFrameNumber(0xFFFFFFFF),
                             nSlices(0),
                             bIsMVC(false),
                             nRotationAngle(0),
                             bEnableDeinterlacing(false),
                             bLABRC(false),
                             nLADepth(0)
{    
    
} // sInputParams::sInputParams()

void sInputParams::Reset()
{
    memset(this, 0, sizeof(this));
    priority = MFX_PRIORITY_NORMAL; 
    MaxFrameNumber = 0xFFFFFFFF;
}

CTranscodingPipeline::CTranscodingPipeline():m_pmfxBS(NULL),
                                             m_pMFXAllocator(NULL),                                             
                                             m_AsyncDepth(0),
                                             m_nTimeout(0),
                                             m_nProcessedFramesNum(0),
                                             m_bIsJoinSession(false),
                                             m_bDecodeEnable(true),
                                             m_bEncodeEnable(true),
                                             m_pBuffer(NULL),
                                             m_pParentPipeline(NULL),
                                             m_bIsInit(false),
                                             m_FrameNumberPreference(0xFFFFFFFF),
                                             m_MaxFramesForTranscode(0xFFFFFFFF),
                                             m_pBSProcessor(NULL),                                                                                          
                                             m_bIsVpp(false),
                                             m_bIsPlugin(false),
                                             m_bUseOpaqueMemory(false),
                                             m_DecSurfaceType(0),
                                             m_EncSurfaceType(0), 
                                             m_bOwnMVCSeqDescMemory(true),
                                             m_hdl(NULL)
{ 
    MSDK_ZERO_MEMORY(m_mfxDecParams);
    MSDK_ZERO_MEMORY(m_mfxVppParams);
    MSDK_ZERO_MEMORY(m_mfxEncParams);
    MSDK_ZERO_MEMORY(m_mfxPluginParams);
    MSDK_ZERO_MEMORY(m_RotateParam);

    MSDK_ZERO_MEMORY(m_mfxDecResponse);  
    MSDK_ZERO_MEMORY(m_mfxEncResponse);
    
    MSDK_ZERO_MEMORY(m_Request);

    MSDK_ZERO_MEMORY(m_VppDoNotUse);
    MSDK_ZERO_MEMORY(m_MVCSeqDesc);
    MSDK_ZERO_MEMORY(m_EncOpaqueAlloc);
    MSDK_ZERO_MEMORY(m_VppOpaqueAlloc);
    MSDK_ZERO_MEMORY(m_DecOpaqueAlloc);
    MSDK_ZERO_MEMORY(m_PluginOpaqueAlloc);
    
    m_MVCSeqDesc.Header.BufferId = MFX_EXTBUFF_MVC_SEQ_DESC;
    m_MVCSeqDesc.Header.BufferSz = sizeof(mfxExtMVCSeqDesc);

    m_VppDoNotUse.Header.BufferId = MFX_EXTBUFF_VPP_DONOTUSE;
    m_VppDoNotUse.Header.BufferSz = sizeof(mfxExtVPPDoNotUse);

    m_EncOpaqueAlloc.Header.BufferId = m_VppOpaqueAlloc.Header.BufferId = 
        m_DecOpaqueAlloc.Header.BufferId = m_PluginOpaqueAlloc.Header.BufferId = 
        MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION;
    m_EncOpaqueAlloc.Header.BufferSz = m_VppOpaqueAlloc.Header.BufferSz = 
        m_DecOpaqueAlloc.Header.BufferSz = m_PluginOpaqueAlloc.Header.BufferSz = 
        sizeof(mfxExtOpaqueSurfaceAlloc);

} //CTranscodingPipeline::CTranscodingPipeline()

CTranscodingPipeline::~CTranscodingPipeline()
{
    Close();
} //CTranscodingPipeline::CTranscodingPipeline()

// initialize decode part
mfxStatus CTranscodingPipeline::DecodePreInit(sInputParams *pParams)
{
    // initialize decode pert
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bDecodeEnable)
    {
        const msdk_char* path = NULL;
        const msdkPluginUID* uid = NULL;

        /* Here we actually define the following codec initialization scheme:
         *    a) we check if codec is distributed as a user plugin and load it if yes
         *    b) we check if codec is distributed as a mediasdk plugin and load it if yes
         *    c) if codec is not in the list of user plugins or mediasdk plugins, we assume, that it is supported inside mediasdk library
         */
        
        path = msdkGetPluginPath(MSDK_VDEC | MSDK_IMPL_USR, pParams->DecodeId);
        if (pParams->libType != MFX_IMPL_SOFTWARE) {
            uid = msdkGetPluginUID(MSDK_VDEC | MSDK_IMPL_HW, pParams->DecodeId);
        } else {
            uid = msdkGetPluginUID(MSDK_VDEC | MSDK_IMPL_SW, pParams->DecodeId);
        }
        
        if (path)
        {
            m_pUserDecoderModule.reset(new MFXVideoUSER(*m_pmfxSession.get()));
            m_pUserDecoderPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_DECODE, m_pUserDecoderModule.get(), path));
            if (m_pUserDecoderPlugin.get() == NULL) sts = MFX_ERR_UNSUPPORTED;
        }
        else if (uid)
        {
            sts = MFXVideoUSER_Load((*m_pmfxSession.get()), &(uid->mfx), 1);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create decoder
        m_pmfxDEC.reset(new MFXVideoDECODE(*m_pmfxSession.get()));

        // set video type in parameters
        m_mfxDecParams.mfx.CodecId = pParams->DecodeId;

        // configure specific decoder parameters
        sts = InitDecMfxParams(pParams);
        if (MFX_ERR_MORE_DATA == sts)
        {
            m_pmfxDEC.reset(NULL);
            return sts;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }      
    }
    else
    {
        m_mfxDecParams = m_pParentPipeline->GetDecodeParam();
        m_MVCSeqDesc = m_pParentPipeline->GetDecMVCSeqDesc();
        m_bOwnMVCSeqDescMemory = false;
    }
    return sts;

} //mfxStatus CTranscodingPipeline::Init(sInputParams *pParams)

mfxStatus CTranscodingPipeline::VPPPreInit(sInputParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bEncodeEnable)
    {    
        if ( (m_mfxDecParams.mfx.FrameInfo.CropW != pParams->nDstWidth && pParams->nDstWidth) ||
             (m_mfxDecParams.mfx.FrameInfo.CropH != pParams->nDstHeight && pParams->nDstHeight) ||
             (pParams->bEnableDeinterlacing))
        {
            m_bIsVpp = true;    
            sts = InitVppMfxParams(pParams);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);           
        }

        if (pParams->nRotationAngle) // plugin was requested
        {
            m_bIsPlugin = true;
            sts = InitPluginMfxParams(pParams);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);            

            std::auto_ptr<MFXVideoVPPPlugin> pVPPPlugin(new MFXVideoVPPPlugin(*m_pmfxSession.get()));
            MSDK_CHECK_POINTER(pVPPPlugin.get(), MFX_ERR_NULL_PTR);

            sts = pVPPPlugin->LoadDLL(pParams->strPluginDLLPath);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);   

            m_RotateParam.Angle = pParams->nRotationAngle;
            sts = pVPPPlugin->SetAuxParam(&m_RotateParam, sizeof(m_RotateParam)); 
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

            m_pmfxVPP.reset(pVPPPlugin.release());
        } 
        
        if (!m_bIsPlugin && m_bIsVpp) // only VPP was requested
        {
            m_pmfxVPP.reset(new MFXVideoMultiVPP(*m_pmfxSession.get()));
        }      
    }

    return sts;

} //mfxStatus CTranscodingPipeline::VPPInit(sInputParams *pParams)

mfxStatus CTranscodingPipeline::EncodePreInit(sInputParams *pParams)
{
    
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bEncodeEnable)
    {       
        const msdk_char* path = NULL;
        const msdkPluginUID* uid = NULL;

        /* Here we actually define the following codec initialization scheme:
         *    a) we check if codec is distributed as a user plugin and load it if yes
         *    b) we check if codec is distributed as a mediasdk plugin and load it if yes
         *    c) if codec is not in the list of user plugins or mediasdk plugins, we assume, that it is supported inside mediasdk library
         */

        path = msdkGetPluginPath(MSDK_VENC | MSDK_IMPL_USR, pParams->EncodeId);
        if (pParams->libType != MFX_IMPL_SOFTWARE) {
            uid = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_HW, pParams->EncodeId);
        } else {
            uid = msdkGetPluginUID(MSDK_VENC | MSDK_IMPL_SW, pParams->EncodeId);
        }

        if (path)
        {
            m_pUserEncoderModule.reset(new MFXVideoUSER(*m_pmfxSession.get()));
            m_pUserEncoderPlugin.reset(LoadPlugin(MFX_PLUGINTYPE_VIDEO_ENCODE, m_pUserEncoderModule.get(), path));
            if (m_pUserEncoderPlugin.get() == NULL) sts = MFX_ERR_UNSUPPORTED;
        }
        else if (uid)
        {
            sts = MFXVideoUSER_Load((*m_pmfxSession.get()), &(uid->mfx), 1);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // create encoder
        m_pmfxENC.reset(new MFXVideoENCODE(*m_pmfxSession.get()));        

        sts = InitEncMfxParams(pParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);        
    }

    return sts;

} // mfxStatus CTranscodingPipeline::EncodeInit(sInputParams *pParams)

// 1 ms provides better result in range [0..5] ms
enum
{
    TIME_TO_SLEEP = 1
};

mfxStatus CTranscodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface,  MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxFrameSurface1    *pmfxSurface = NULL;
    pExtSurface->pSurface = NULL;
    mfxU32 i = 0;   

    while (MFX_ERR_MORE_DATA == sts || MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE < sts)          
    {        
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(TIME_TO_SLEEP); // just wait and then repeat the same call to DecodeFrameAsync
        }
        else if (MFX_ERR_MORE_DATA == sts)
        {
            sts = m_pBSProcessor->GetInputBitstream(&m_pmfxBS); // read more data to input bit stream            
            MSDK_BREAK_ON_ERROR(sts);            
        }
        else if (MFX_ERR_MORE_SURFACE == sts)
        {
            // find new working surface  
            for (i = 0; i < MSDK_DEC_WAIT_INTERVAL; i += 5)
            {
                pmfxSurface = GetFreeSurface(true);            
                if (pmfxSurface)
                {
                    break;                    
                }
                else
                {
                    MSDK_SLEEP(TIME_TO_SLEEP);                 
                }
            }        

            MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found
        }

        sts = m_pmfxDEC->DecodeFrameAsync(m_pmfxBS, pmfxSurface, &pExtSurface->pSurface, &pExtSurface->Syncp);

        // ignore warnings if output is available,         
        if (MFX_ERR_NONE < sts && pExtSurface->Syncp) 
        {
            sts = MFX_ERR_NONE;
        }
          
    } //while processing   

    return sts;

} // mfxStatus CTranscodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)
mfxStatus CTranscodingPipeline::DecodeLastFrame(ExtendedSurface *pExtSurface)
{
    mfxFrameSurface1    *pmfxSurface = NULL;
    mfxStatus sts = MFX_ERR_MORE_SURFACE;
    mfxU32 i = 0;

    // retrieve the buffered decoded frames
    while (MFX_ERR_MORE_SURFACE == sts || MFX_WRN_DEVICE_BUSY == sts)        
    {        
        if (MFX_WRN_DEVICE_BUSY == sts)
        {
            MSDK_SLEEP(TIME_TO_SLEEP);
        }

        // find new working surface  
        for (i = 0; i < MSDK_DEC_WAIT_INTERVAL; i += 5)
        {
            pmfxSurface = GetFreeSurface(true);            
            if (pmfxSurface)
            {
                break;                    
            }
            else
            {
                MSDK_SLEEP(TIME_TO_SLEEP);                 
            }
        }        

        MSDK_CHECK_POINTER(pmfxSurface, MFX_ERR_MEMORY_ALLOC); // return an error if a free surface wasn't found

        sts = m_pmfxDEC->DecodeFrameAsync(NULL, pmfxSurface, &pExtSurface->pSurface, &pExtSurface->Syncp);
    } 
    return sts;
}

mfxStatus CTranscodingPipeline::VPPOneFrame(mfxFrameSurface1 *pSurfaceIn, ExtendedSurface *pExtSurface)
{
    MSDK_CHECK_POINTER(pExtSurface,  MFX_ERR_NULL_PTR);
    mfxFrameSurface1 *pmfxSurface = NULL;
    // find/wait for a free working surface
    for (mfxU32 i = 0; i < MSDK_WAIT_INTERVAL; i += TIME_TO_SLEEP)
    {
        pmfxSurface= GetFreeSurface(false); 

        if (pmfxSurface)
        {
            break;
        }
        else 
        {
            MSDK_SLEEP(TIME_TO_SLEEP);
        }        
    }     

    MSDK_CHECK_POINTER(pmfxSurface,  MFX_ERR_MEMORY_ALLOC);

    // make sure picture structure has the initial value
    // surfaces are reused and VPP may change this parameter in certain configurations
    pmfxSurface->Info.PicStruct = m_mfxEncParams.mfx.FrameInfo.PicStruct; 

    pExtSurface->pSurface = pmfxSurface;
    mfxStatus sts = MFX_ERR_NONE;
    for(;;)
    {
        sts = m_pmfxVPP->RunFrameVPPAsync(pSurfaceIn, pmfxSurface, NULL, &pExtSurface->Syncp); 

        if (MFX_ERR_NONE < sts && !pExtSurface->Syncp) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts)                
                MSDK_SLEEP(1); // wait if device is busy                
        }
        else if (MFX_ERR_NONE < sts && pExtSurface->Syncp)                 
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
            break;
        }
        else
        {
            break;
        }
    }
    return sts;

} // mfxStatus CTranscodingPipeline::DecodeOneFrame(ExtendedSurface *pExtSurface)

mfxStatus CTranscodingPipeline::EncodeOneFrame(ExtendedSurface *pExtSurface, mfxBitstream *pBS)
{
    mfxStatus sts = MFX_ERR_NONE;
    for (;;)
    {
        // at this point surface for encoder contains either a frame from file or a frame processed by vpp        
        sts = m_pmfxENC->EncodeFrameAsync(NULL, pExtSurface->pSurface, pBS, &pExtSurface->Syncp);        
        
        if (MFX_ERR_NONE < sts && !pExtSurface->Syncp) // repeat the call if warning and no output
        {
            if (MFX_WRN_DEVICE_BUSY == sts)                
                MSDK_SLEEP(TIME_TO_SLEEP); // wait if device is busy                
        }
        else if (MFX_ERR_NONE < sts && pExtSurface->Syncp)                 
        {
            sts = MFX_ERR_NONE; // ignore warnings if output is available                                    
            break;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
        {
            sts = AllocateSufficientBuffer(pBS);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);                
        }
        else
        {
            break;
        }
    }
    return sts;

} //CTranscodingPipeline::EncodeOneFrame(ExtendedSurface *pExtSurface)

// signal that there are no more frames
void CTranscodingPipeline::NoMoreFramesSignal(ExtendedSurface &DecExtSurface)
{
    SafetySurfaceBuffer   *pNextBuffer = m_pBuffer;
    DecExtSurface.pSurface = NULL;
    pNextBuffer->AddSurface(DecExtSurface);
    while (pNextBuffer->m_pNext)
    {
        pNextBuffer = pNextBuffer->m_pNext;
        pNextBuffer->AddSurface(DecExtSurface);
    }
}
mfxStatus CTranscodingPipeline::Decode()
{
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface DecExtSurface = {0};
    SafetySurfaceBuffer   *pNextBuffer = m_pBuffer;
    bool bEndOfFile = false;

    time_t start = time(0);
    while (MFX_ERR_NONE == sts)
    {
        pNextBuffer = m_pBuffer;
        
        if (m_MaxFramesForTranscode == m_nProcessedFramesNum)
        {
            break;
        }

        if (!bEndOfFile)
        {
            sts = DecodeOneFrame(&DecExtSurface);
            if (MFX_ERR_MORE_DATA == sts)
            {
                sts = DecodeLastFrame(&DecExtSurface);            
                bEndOfFile = true;
            }
        }
        else
        {
            sts = DecodeLastFrame(&DecExtSurface);            
        }  

        if (sts == MFX_ERR_NONE)
        {
            m_nProcessedFramesNum++;
        }

        MSDK_BREAK_ON_ERROR(sts);   

        // if session is not join and it is not parent - syncronize
        if (!m_bIsJoinSession && m_pParentPipeline)
        {
            sts = m_pmfxSession->SyncOperation(DecExtSurface.Syncp, MSDK_WAIT_INTERVAL);
            DecExtSurface.Syncp = 0;
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }

        // add surfaces in queue for all sinks
        pNextBuffer->AddSurface(DecExtSurface);
        while (pNextBuffer->m_pNext)
        {
            pNextBuffer = pNextBuffer->m_pNext;
            pNextBuffer->AddSurface(DecExtSurface);
        }

        if (0 == (m_nProcessedFramesNum - 1) % 100)
        {
            msdk_printf(MSDK_STRING(".")); 
        }
        if ((m_nTimeout) && (time(0) - start >= m_nTimeout))
        {
            break;
        }
    }

    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    
    NoMoreFramesSignal(DecExtSurface);

    if (MFX_ERR_NONE == sts)
        sts = MFX_WRN_VALUE_NOT_CHANGED;

    return sts;
} // mfxStatus CTranscodingPipeline::Decode()

mfxStatus CTranscodingPipeline::Encode()
{
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface DecExtSurface = {0};
    ExtendedSurface VppExtSurface = {0};
    ExtendedBS      *pBS = NULL;
    bool isQuit = false;

    time_t start = time(0);
    while (MFX_ERR_NONE == sts ||  MFX_ERR_MORE_DATA == sts)
    {
        while (MFX_ERR_MORE_SURFACE == m_pBuffer->GetSurface(DecExtSurface) && !isQuit)
            MSDK_SLEEP(TIME_TO_SLEEP);

         // if session is not join and it is not parent - synchronize
        if (!m_bIsJoinSession && m_pParentPipeline)
        {
            // if it is not already synchronize
            if (DecExtSurface.Syncp)
            {
                sts = m_pParentPipeline->m_pmfxSession->SyncOperation(DecExtSurface.Syncp, MSDK_WAIT_INTERVAL);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
        }

        if (NULL == DecExtSurface.pSurface)
            isQuit = true;
        
        if (m_pmfxVPP.get())
            sts = VPPOneFrame(DecExtSurface.pSurface, &VppExtSurface);
        else // no VPP - just copy pointers
            VppExtSurface.pSurface = DecExtSurface.pSurface;

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (isQuit)
            {
                // to get buffered VPP or ENC frames
                VppExtSurface.pSurface = NULL;
                sts = MFX_ERR_NONE;
            }
            else
            {
                m_pBuffer->ReleaseSurface(DecExtSurface.pSurface);
                continue;
            }
        } 

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

        pBS = m_pBSStore->GetNext();
        if (!pBS)
            return MFX_ERR_NOT_FOUND;

        m_BSPool.push_back(pBS);

        sts = EncodeOneFrame(&VppExtSurface, &m_BSPool.back()->Bitstream);

        m_pBuffer->ReleaseSurface(DecExtSurface.pSurface);

        // check if we need one more frame from decode
        if (MFX_ERR_MORE_DATA == sts) 
        {
            // the task in not in Encode queue
            m_BSPool.pop_back();
            m_pBSStore->Release(pBS);

            if (NULL == VppExtSurface.pSurface ) // there are no more buffered frames in encoder
            {
                break;
            }
            else
            {
                // get next frame from Decode
                sts = MFX_ERR_NONE;
                continue;
            }
        } 

        // check encoding result
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        
        m_nProcessedFramesNum++;

        m_BSPool.back()->Syncp = VppExtSurface.Syncp;

        if (m_BSPool.size() == m_AsyncDepth)
        {
            sts = PutBS();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
        else
        {
            continue;
        }

        if ((m_nTimeout) && (time(0) - start >= m_nTimeout))
        {
            break;
        }

    }
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // need to get buffered bitstream
    if (MFX_ERR_NONE == sts)
    {
        while(m_BSPool.size())
        {
            sts = PutBS();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    if (MFX_ERR_NONE == sts)
        sts = MFX_WRN_VALUE_NOT_CHANGED;
    return sts;

} // mfxStatus CTranscodingPipeline::Encode()

mfxStatus CTranscodingPipeline::Transcode()
{
    mfxStatus sts = MFX_ERR_NONE;
    ExtendedSurface DecExtSurface = {0};
    ExtendedSurface VppExtSurface = {0};
    ExtendedBS *pBS = NULL;    
    bool bNeedDecodedFrames = true; // indicates if we need to decode frames
    bool bEndOfFile = false; 
    bool bLastCycle = false;

    time_t start = time(0);
    while (MFX_ERR_NONE == sts )
    {
        if (time(0) - start >= m_nTimeout)
            bLastCycle = true;
        if (m_MaxFramesForTranscode == m_nProcessedFramesNum)
        {          
            DecExtSurface.pSurface = NULL;  // to get buffered VPP or ENC frames            
            bNeedDecodedFrames = false; // no more decoded frames needed           
        }
        
        // if need more decoded frames
        // decode a frame
        if (bNeedDecodedFrames)
        {
            if (!bEndOfFile)
            {
                sts = DecodeOneFrame(&DecExtSurface);
                if (MFX_ERR_MORE_DATA == sts)
                {
                    sts = DecodeLastFrame(&DecExtSurface);            
                    bEndOfFile = true;
                }
            }
            else
            {
                sts = DecodeLastFrame(&DecExtSurface);            
            }              

            if (sts == MFX_ERR_MORE_DATA)
            {
                DecExtSurface.pSurface = NULL;  // to get buffered VPP or ENC frames                            
                sts = MFX_ERR_NONE;
            }                

            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
       
        // pre-process a frame
        if (m_pmfxVPP.get())
            sts = VPPOneFrame(DecExtSurface.pSurface, &VppExtSurface);
        else // no VPP - just copy pointers
            VppExtSurface.pSurface = DecExtSurface.pSurface;

        if (sts == MFX_ERR_MORE_DATA)
        {
            sts = MFX_ERR_NONE;
            if (NULL == DecExtSurface.pSurface) // there are no more buffered frames in VPP
            {
                VppExtSurface.pSurface = NULL; // to get buffered ENC frames                
            }
            else
            {                
                continue; // go get next frame from Decode
            }
        }

        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
        
        // encode frame
        pBS = m_pBSStore->GetNext();
        if (!pBS)
            return MFX_ERR_NOT_FOUND;

        m_BSPool.push_back(pBS);      

        // encode frame only if it wasn't encoded enough
        sts = bNeedDecodedFrames ? EncodeOneFrame(&VppExtSurface, &m_BSPool.back()->Bitstream) : MFX_ERR_MORE_DATA;
   
        // check if we need one more frame from decode
        if (MFX_ERR_MORE_DATA == sts) 
        {
            // the task in not in Encode queue
            m_BSPool.pop_back();
            m_pBSStore->Release(pBS);


            if (NULL == VppExtSurface.pSurface) // there are no more buffered frames in encoder
            {
                if (bLastCycle)
                    break;
                static_cast<FileBitstreamProcessor_WithReset*>(m_pBSProcessor)->ResetInput();
                static_cast<FileBitstreamProcessor_WithReset*>(m_pBSProcessor)->ResetOutput();
                bEndOfFile = 0;
            }
            sts = MFX_ERR_NONE;
            continue;
        }        

        // check encoding result
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        
        m_nProcessedFramesNum++;

        if (0 == (m_nProcessedFramesNum - 1) % 100)
        {
            msdk_printf(MSDK_STRING(".")); 
        }

        m_BSPool.back()->Syncp = VppExtSurface.Syncp;

        if (m_BSPool.size() == m_AsyncDepth)
        {
            sts = PutBS();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);

    // need to get buffered bitstream
    if (MFX_ERR_NONE == sts)
    {
        while(m_BSPool.size())
        {
            sts = PutBS();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        }
    }

    if (MFX_ERR_NONE == sts)
        sts = MFX_WRN_VALUE_NOT_CHANGED;
    
    return sts;
} // mfxStatus CTranscodingPipeline::Transcode()

mfxStatus CTranscodingPipeline::PutBS()
{
    mfxStatus       sts = MFX_ERR_NONE;
    ExtendedBS *pBitstreamEx  = m_BSPool.front();
    // get result coded stream
    sts = m_pmfxSession->SyncOperation(pBitstreamEx->Syncp, MSDK_WAIT_INTERVAL);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pBSProcessor->ProcessOutputBitstream(&pBitstreamEx->Bitstream);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    pBitstreamEx->Bitstream.DataLength = 0;
    pBitstreamEx->Bitstream.DataOffset = 0;

    m_BSPool.pop_front();
    m_pBSStore->Release(pBitstreamEx);    

    return sts;
} //mfxStatus CTranscodingPipeline::PutBS()

mfxStatus CTranscodingPipeline::AllocMVCSeqDesc()
{
    mfxU32 i;    

    m_MVCSeqDesc.View = new mfxMVCViewDependency[m_MVCSeqDesc.NumView];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.View, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumView; ++i)
    {
        MSDK_ZERO_MEMORY(m_MVCSeqDesc.View[i]);
    }
    m_MVCSeqDesc.NumViewAlloc = m_MVCSeqDesc.NumView;

    m_MVCSeqDesc.ViewId = new mfxU16[m_MVCSeqDesc.NumViewId];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.ViewId, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumViewId; ++i)
    {
        MSDK_ZERO_MEMORY(m_MVCSeqDesc.ViewId[i]);
    }
    m_MVCSeqDesc.NumViewIdAlloc = m_MVCSeqDesc.NumViewId;

    m_MVCSeqDesc.OP = new mfxMVCOperationPoint[m_MVCSeqDesc.NumOP];
    MSDK_CHECK_POINTER(m_MVCSeqDesc.OP, MFX_ERR_MEMORY_ALLOC);
    for (i = 0; i < m_MVCSeqDesc.NumOP; ++i)
    {
        MSDK_ZERO_MEMORY(m_MVCSeqDesc.OP[i]);
    }
    m_MVCSeqDesc.NumOPAlloc = m_MVCSeqDesc.NumOP;

    return MFX_ERR_NONE;
}

void CTranscodingPipeline::FreeMVCSeqDesc()
{
    if (m_bOwnMVCSeqDescMemory)
    {
        MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.View);
        MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.ViewId);
        MSDK_SAFE_DELETE_ARRAY(m_MVCSeqDesc.OP);  
    }        
}

mfxStatus CTranscodingPipeline::InitDecMfxParams(sInputParams *pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    MSDK_CHECK_POINTER(pInParams, MFX_ERR_NULL_PTR);    

    // configure and attach external parameters 
    if (m_bUseOpaqueMemory)
        m_DecExtParams.push_back((mfxExtBuffer *)&m_DecOpaqueAlloc);

    if (pInParams->bIsMVC)
        m_DecExtParams.push_back((mfxExtBuffer *)&m_MVCSeqDesc);

    if (!m_DecExtParams.empty())
    {
        m_mfxDecParams.ExtParam = &m_DecExtParams[0]; // vector is stored linearly in memory
        m_mfxDecParams.NumExtParam = (mfxU16)m_DecExtParams.size();
    }
    
    // read a portion of data for DecodeHeader function             
    sts = m_pBSProcessor->GetInputBitstream(&m_pmfxBS);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;
    else
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // try to find a sequence header in the stream
    // if header is not found this function exits with error (e.g. if device was lost and there's no header in the remaining stream)       
    for(;;)
    {     
        // trying to find PicStruct information in AVI headers        
        if ( pInParams->DecodeId == MFX_CODEC_JPEG )
            MJPEG_AVI_ParsePicStruct(m_pmfxBS);

        // parse bit stream and fill mfx params
        sts = m_pmfxDEC->DecodeHeader(m_pmfxBS, &m_mfxDecParams);       

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_pmfxBS->MaxLength == m_pmfxBS->DataLength)
            {
                sts = ExtendMfxBitstream(m_pmfxBS, m_pmfxBS->MaxLength * 2); 
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }

            // read a portion of data for DecodeHeader function             
            sts = m_pBSProcessor->GetInputBitstream(&m_pmfxBS);
            if (MFX_ERR_MORE_DATA == sts)
                return sts;
            else
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


            continue;
        }
        else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts && pInParams->bIsMVC)
        {
            sts = AllocMVCSeqDesc();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            continue;
        }
        else
            break;
    }

    // to enable decorative flags, has effect with 1.3 API libraries only
    // (in case of JPEG decoder - it is not valid to use this field)
    if (m_mfxDecParams.mfx.CodecId != MFX_CODEC_JPEG)
        m_mfxDecParams.mfx.ExtendedPicStruct = 1;

    // check DecodeHeader status
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);        
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);       

    // set memory pattern
    if (m_bUseOpaqueMemory)
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
    else if (MFX_IMPL_SOFTWARE == pInParams->libType)
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    else
        m_mfxDecParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    // if input is interlaced JPEG stream
    if (pInParams->DecodeId == MFX_CODEC_JPEG && m_pmfxBS->PicStruct == MFX_PICSTRUCT_FIELD_TFF || m_pmfxBS->PicStruct == MFX_PICSTRUCT_FIELD_BFF)
    {
        m_mfxDecParams.mfx.FrameInfo.CropH *= 2;
        m_mfxDecParams.mfx.FrameInfo.Height = MSDK_ALIGN16(m_mfxDecParams.mfx.FrameInfo.CropH);
        m_mfxDecParams.mfx.FrameInfo.PicStruct = m_pmfxBS->PicStruct;
    }

    // if frame rate specified by user set it for decoder and the whole pipeline 
    if (pInParams->dFrameRate)
    {
        ConvertFrameRate(pInParams->dFrameRate, &m_mfxDecParams.mfx.FrameInfo.FrameRateExtN, &m_mfxDecParams.mfx.FrameInfo.FrameRateExtD);
    }  
    // if frame rate not specified and input stream header doesn't contain valid values use default (30.0)
    else if (!(m_mfxDecParams.mfx.FrameInfo.FrameRateExtN * m_mfxDecParams.mfx.FrameInfo.FrameRateExtD))
    {
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtN = 30;
        m_mfxDecParams.mfx.FrameInfo.FrameRateExtD = 1;
    }
    else
    {
        // use the value from input stream header
    } 

    return MFX_ERR_NONE;
}// mfxStatus CTranscodingPipeline::InitDecMfxParams()

mfxStatus CTranscodingPipeline::InitEncMfxParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);   
    m_mfxEncParams.mfx.CodecId                 = pInParams->EncodeId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.RateControlMethod       = (pInParams->bLABRC || pInParams->nLADepth) ?
        (mfxU16)MFX_RATECONTROL_LA : (mfxU16)MFX_RATECONTROL_CBR;
    
    m_mfxEncParams.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order
    m_mfxEncParams.mfx.NumSlice                = pInParams->nSlices;     

    if (m_bIsVpp)
    {
        MSDK_MEMCPY_VAR(m_mfxEncParams.mfx.FrameInfo, &m_mfxVppParams.vpp.Out, sizeof(mfxFrameInfo));
    }
    else if (m_bIsPlugin)
    {
        MSDK_MEMCPY_VAR(m_mfxEncParams.mfx.FrameInfo, &m_mfxPluginParams.vpp.Out, sizeof(mfxFrameInfo));
    }
    else
    {        
        MSDK_MEMCPY_VAR(m_mfxEncParams.mfx.FrameInfo, &m_mfxDecParams.mfx.FrameInfo, sizeof(mfxFrameInfo));        
    }   

    // leave PAR unset to avoid MPEG2 encoder rejecting streams with unsupported DAR
    m_mfxEncParams.mfx.FrameInfo.AspectRatioW = m_mfxEncParams.mfx.FrameInfo.AspectRatioH = 0;  

    // calculate default bitrate based on resolution and framerate

    // set framerate if specified 
    // NOTE: no frame rate conversion is performed in this sample
    if (pInParams->dFrameRate)
    {
        ConvertFrameRate(pInParams->dFrameRate, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtN, &m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);
    }

    MSDK_CHECK_ERROR(m_mfxEncParams.mfx.FrameInfo.FrameRateExtN * m_mfxEncParams.mfx.FrameInfo.FrameRateExtD, 
        0, MFX_ERR_INVALID_VIDEO_PARAM);
    if (pInParams->nBitRate == 0)
    {        
        pInParams->nBitRate = CalculateDefaultBitrate(pInParams->EncodeId, 
            pInParams->nTargetUsage, m_mfxEncParams.mfx.FrameInfo.Width, m_mfxEncParams.mfx.FrameInfo.Height, 
            1.0 * m_mfxEncParams.mfx.FrameInfo.FrameRateExtN / m_mfxEncParams.mfx.FrameInfo.FrameRateExtD);         
    }

    m_mfxEncParams.mfx.TargetKbps = (mfxU16)(pInParams->nBitRate); // in Kbps

   
    m_mfxEncParams.mfx.FrameInfo.CropX = 0; 
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
 
    mfxU16 InPatternFromParent = (mfxU16) ((MFX_IOPATTERN_OUT_VIDEO_MEMORY == m_mfxDecParams.IOPattern) ? 
MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    // set memory pattern
    if (m_bUseOpaqueMemory)
        m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY;
    else 
        m_mfxEncParams.IOPattern = InPatternFromParent;    
    
    // we don't specify profile and level and let the encoder choose those basing on parameters
    // we must specify profile only for MVC codec
    if (pInParams->bIsMVC)
    {
        m_mfxEncParams.mfx.CodecProfile = m_mfxDecParams.mfx.CodecProfile;
    }

    // JPEG encoder settings overlap with other encoders settings in mfxInfoMFX structure
    if (MFX_CODEC_JPEG == pInParams->EncodeId)
    {
        m_mfxEncParams.mfx.Interleaved = 1;
        m_mfxEncParams.mfx.Quality = pInParams->nQuality;
        m_mfxEncParams.mfx.RestartInterval = 0;
        MSDK_ZERO_MEMORY(m_mfxEncParams.mfx.reserved5);
    }

    // configure and attach external parameters 
    if (m_bUseOpaqueMemory)
        m_EncExtParams.push_back((mfxExtBuffer *)&m_EncOpaqueAlloc);

    if (pInParams->bIsMVC)
        m_EncExtParams.push_back((mfxExtBuffer *)&m_MVCSeqDesc);

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }
    
    return MFX_ERR_NONE;
}// mfxStatus CTranscodingPipeline::InitEncMfxParams(sInputParams *pInParams)

mfxStatus CTranscodingPipeline::InitVppMfxParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR); 

    mfxU16 InPatternFromParent = (mfxU16)((MFX_IOPATTERN_OUT_VIDEO_MEMORY == m_mfxDecParams.IOPattern) ? 
MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);
       
    // set memory pattern
    if (m_bUseOpaqueMemory)
        m_mfxVppParams.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
    else if (MFX_IMPL_SOFTWARE == pInParams->libType)
        m_mfxVppParams.IOPattern = (mfxU16)(InPatternFromParent|MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
    else
        m_mfxVppParams.IOPattern = (mfxU16)(InPatternFromParent|MFX_IOPATTERN_OUT_VIDEO_MEMORY);  

    // input frame info  
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.In, &m_mfxDecParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
    
    // fill output frame info
    MSDK_MEMCPY_VAR(m_mfxVppParams.vpp.Out, &m_mfxVppParams.vpp.In, sizeof(mfxFrameInfo));
    
   
    if (pInParams->bEnableDeinterlacing)
        m_mfxVppParams.vpp.Out.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    

    // only resizing is supported
    if (pInParams->nDstWidth) 
    {
        m_mfxVppParams.vpp.Out.CropW = pInParams->nDstWidth;
        m_mfxVppParams.vpp.Out.Width     = MSDK_ALIGN16(pInParams->nDstWidth);
    }

    if (pInParams->nDstHeight)
    {
        m_mfxVppParams.vpp.Out.CropH = pInParams->nDstHeight;
        m_mfxVppParams.vpp.Out.Height    = (MFX_PICSTRUCT_PROGRESSIVE == m_mfxVppParams.vpp.Out.PicStruct)?
            MSDK_ALIGN16(pInParams->nDstHeight) : MSDK_ALIGN32(pInParams->nDstHeight);    
    }

    
    if (pInParams->bEnableDeinterlacing)
    {
        // If stream were interlaced before then 32 bit alignment were applied.
        // Discard 32 bit alignment as progressive doesn't require it.
        m_mfxVppParams.vpp.Out.Height = MSDK_ALIGN16(m_mfxVppParams.vpp.Out.CropH);
        m_mfxVppParams.vpp.Out.Width  = MSDK_ALIGN16(m_mfxVppParams.vpp.Out.CropW);
    }
    

    // configure and attach external parameters 
    mfxStatus sts = AllocAndInitVppDoNotUse();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    m_VppExtParams.push_back((mfxExtBuffer *)&m_VppDoNotUse);

    if (m_bUseOpaqueMemory)
        m_VppExtParams.push_back((mfxExtBuffer *)&m_VppOpaqueAlloc);  
    if (pInParams->bIsMVC)
        m_VppExtParams.push_back((mfxExtBuffer *)&m_MVCSeqDesc);
    
    m_mfxVppParams.ExtParam = &m_VppExtParams[0]; // vector is stored linearly in memory
    m_mfxVppParams.NumExtParam = (mfxU16)m_VppExtParams.size();
   
    return MFX_ERR_NONE;

} //mfxStatus CTranscodingPipeline::InitMfxVppParams(sInputParams *pInParams)

mfxStatus CTranscodingPipeline::InitPluginMfxParams(sInputParams *pInParams)
{
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);  
    
    mfxU16 parentPattern = m_bIsVpp ? m_mfxVppParams.IOPattern : m_mfxDecParams.IOPattern;
    mfxU16 InPatternFromParent = (mfxU16)((MFX_IOPATTERN_OUT_VIDEO_MEMORY == parentPattern) ? 
MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    // set memory pattern
    if (m_bUseOpaqueMemory)
        m_mfxPluginParams.IOPattern = MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY;
    else if (MFX_IMPL_SOFTWARE == pInParams->libType)
        m_mfxPluginParams.IOPattern = (mfxU16)(InPatternFromParent|MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
    else
        m_mfxPluginParams.IOPattern = (mfxU16)(InPatternFromParent|MFX_IOPATTERN_OUT_VIDEO_MEMORY);         

    m_mfxPluginParams.AsyncDepth = m_AsyncDepth;

    // input frame info
    if (m_bIsVpp)
    {
        MSDK_MEMCPY_VAR(m_mfxPluginParams.vpp.In, &m_mfxVppParams.vpp.Out, sizeof(mfxFrameInfo));
    }
    else
    {
        MSDK_MEMCPY_VAR(m_mfxPluginParams.vpp.In, &m_mfxDecParams.mfx.FrameInfo, sizeof(mfxFrameInfo));
    }   

    // fill output frame info
    // in case of rotation plugin sample output frameinfo is same as input
    MSDK_MEMCPY_VAR(m_mfxPluginParams.vpp.Out, &m_mfxPluginParams.vpp.In, sizeof(mfxFrameInfo)); 

    // configure and attach external parameters 
    if (m_bUseOpaqueMemory)
        m_PluginExtParams.push_back((mfxExtBuffer *)&m_PluginOpaqueAlloc);    

    if (!m_PluginExtParams.empty())
    {
        m_mfxPluginParams.ExtParam = &m_PluginExtParams[0]; // vector is stored linearly in memory
        m_mfxPluginParams.NumExtParam = (mfxU16)m_PluginExtParams.size();
    }

    return MFX_ERR_NONE;

} //mfxStatus CTranscodingPipeline::InitMfxVppParams(sInputParams *pInParams)

mfxStatus CTranscodingPipeline::AllocFrames(mfxFrameAllocRequest *pRequest, bool isDecAlloc)
{    
    mfxStatus sts = MFX_ERR_NONE;
    
    mfxU16 nSurfNum = 0; // number of surfaces
    mfxU16 i;

    if (m_FrameNumberPreference <= pRequest->NumFrameMin) nSurfNum = pRequest->NumFrameMin;
    else if (m_FrameNumberPreference >= pRequest->NumFrameSuggested) nSurfNum = pRequest->NumFrameSuggested;
    else nSurfNum = (mfxU16)m_FrameNumberPreference;
    msdk_printf(MSDK_STRING("Pipeline surfaces number: min=%d, max=%d, chosen=%d\n"),
        pRequest->NumFrameMin, pRequest->NumFrameSuggested, nSurfNum),

    pRequest->NumFrameMin = nSurfNum;
    pRequest->NumFrameSuggested = nSurfNum; 

    mfxFrameAllocResponse *pResponse = isDecAlloc ? &m_mfxDecResponse : &m_mfxEncResponse;

    // no actual memory is allocated if opaque memory type is used
    if (!m_bUseOpaqueMemory)
    {
        sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, pRequest, pResponse);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }    

    for (i = 0; i < nSurfNum; i++)
    {
        mfxFrameSurface1 *surface = new mfxFrameSurface1;
        MSDK_CHECK_POINTER(surface, MFX_ERR_MEMORY_ALLOC);

        MSDK_ZERO_MEMORY(*surface);
        MSDK_MEMCPY_VAR(surface->Info, &(pRequest->Info), sizeof(mfxFrameInfo));

        // no actual memory is allocated if opaque memory type is used (surface pointers and MemId field remain 0)
        if (!m_bUseOpaqueMemory)
        {
            surface->Data.MemId = pResponse->mids[i];    
        }
                
        (isDecAlloc) ? m_pSurfaceDecPool.push_back(surface):m_pSurfaceEncPool.push_back(surface);
    }

    (isDecAlloc) ? m_DecSurfaceType = pRequest->Type : m_EncSurfaceType = pRequest->Type;
   
    return MFX_ERR_NONE;

} // mfxStatus CTranscodingPipeline::AllocFrames(Component* pComp, mfxFrameAllocResponse* pMfxResponse, mfxVideoParam* pMfxVideoParam)

mfxStatus CTranscodingPipeline::AllocFrames()
{
    mfxStatus sts;    
    mfxFrameAllocRequest DecRequest = {0};
    
    if (m_pmfxDEC.get())
    {
        sts = m_pmfxDEC.get()->QueryIOSurf(&m_mfxDecParams, &DecRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // add frames required for connected pipelines
        DecRequest.NumFrameMin = DecRequest.NumFrameMin + m_Request.NumFrameMin;
        DecRequest.NumFrameSuggested = DecRequest.NumFrameSuggested + m_Request.NumFrameSuggested;
    }    

    if (m_pmfxVPP.get())
    {
        mfxFrameAllocRequest VppRequest[2] = {0};

        if (m_bIsPlugin && m_bIsVpp)
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxPluginParams, &(VppRequest[0]), &m_mfxVppParams);
        else if (m_bIsPlugin)
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxPluginParams, &(VppRequest[0]));
        else
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxVppParams, &(VppRequest[0]));  
        
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        mfxFrameAllocRequest EncRequest = {0};
        sts = m_pmfxENC.get()->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        EncRequest.NumFrameMin = EncRequest.NumFrameMin + VppRequest[1].NumFrameMin + (m_AsyncDepth - 1) - 1;
        EncRequest.NumFrameSuggested = EncRequest.NumFrameSuggested + VppRequest[1].NumFrameSuggested + (m_AsyncDepth - 1) - 1;
        EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;                

        sts = AllocFrames(&EncRequest, false);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        
        DecRequest.NumFrameMin = DecRequest.NumFrameMin + VppRequest[0].NumFrameMin + (m_AsyncDepth - 1) - 1;
        DecRequest.NumFrameSuggested = DecRequest.NumFrameSuggested + VppRequest[0].NumFrameSuggested + (m_AsyncDepth - 1) - 1;
        DecRequest.Type |= MFX_MEMTYPE_FROM_VPPIN;                                              
    }
    else if (m_pmfxENC.get()) 
    {
        mfxFrameAllocRequest EncRequest = {0};       
        
        sts = m_pmfxENC.get()->QueryIOSurf(&m_mfxEncParams, &EncRequest);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);       
                
        DecRequest.NumFrameMin = DecRequest.NumFrameMin + EncRequest.NumFrameMin + (m_AsyncDepth - 1) - 1;
        DecRequest.NumFrameSuggested = DecRequest.NumFrameSuggested + EncRequest.NumFrameSuggested + (m_AsyncDepth - 1) - 1;   
        DecRequest.Type |= MFX_MEMTYPE_FROM_ENCODE;
    }

    if (m_pmfxDEC.get())
    {
        sts = AllocFrames(&DecRequest, true);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }   
    
    return MFX_ERR_NONE;
}
mfxStatus CTranscodingPipeline::CalculateNumberOfReqFrames(mfxFrameAllocRequest  *pRequest)
{
    mfxStatus sts;

    if (m_pmfxVPP.get())
    {
        mfxFrameAllocRequest VppRequest[2] = {0};
        if (m_bIsPlugin && m_bIsVpp)
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxPluginParams, &(VppRequest[0]), &m_mfxVppParams);
        else if (m_bIsPlugin)
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxPluginParams, &(VppRequest[0]));
        else
            sts = m_pmfxVPP.get()->QueryIOSurf(&m_mfxVppParams, &(VppRequest[0])); 
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);        
        
        mfxFrameAllocRequest EncRequest = {0};
        sts = m_pmfxENC.get()->QueryIOSurf(&m_mfxEncParams, &EncRequest);       
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);  
        
        EncRequest.NumFrameMin = EncRequest.NumFrameMin + VppRequest[1].NumFrameMin + (m_AsyncDepth - 1) - 1; 
        EncRequest.NumFrameSuggested = EncRequest.NumFrameSuggested + VppRequest[1].NumFrameSuggested + (m_AsyncDepth - 1) - 1;
        EncRequest.Type |= MFX_MEMTYPE_FROM_VPPOUT;   

        // allocate frames to be shared by vpp output and encode input
        sts = AllocFrames(&EncRequest, false);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        
        // calculate and save the additional number for deferred allocation of frames shared by decode output and vpp input        
        pRequest->NumFrameMin = VppRequest[0].NumFrameMin;
        pRequest->NumFrameSuggested = VppRequest[0].NumFrameSuggested;       
        pRequest->Type = MFX_MEMTYPE_FROM_VPPIN;
    }
    else if (m_pmfxENC.get())
    {
        mfxFrameAllocRequest EncRequest = {0};
        
        sts = m_pmfxENC.get()->QueryIOSurf(&m_mfxEncParams, &EncRequest);       
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);        
        
        // calculate and save the additional number for deferred allocation of frames shared by decode output and encode input        
        pRequest->NumFrameMin = EncRequest.NumFrameMin;
        pRequest->NumFrameSuggested = EncRequest.NumFrameSuggested;        
        pRequest->Type = MFX_MEMTYPE_FROM_ENCODE;
    }

    return MFX_ERR_NONE;
}
void CTranscodingPipeline::CorrectNumberOfAllocatedFrames(mfxFrameAllocRequest  *pRequest)
{
    m_Request.NumFrameMin = MSDK_MAX(m_Request.NumFrameMin, pRequest->NumFrameMin + (m_AsyncDepth - 1) - 1);
    m_Request.NumFrameSuggested = pRequest->NumFrameSuggested + m_Request.NumFrameSuggested + (m_AsyncDepth - 1) - 1;
    m_Request.Type |= pRequest->Type;
}

mfxStatus CTranscodingPipeline::InitOpaqueAllocBuffers()
{    
    if (m_pmfxDEC.get())
    {
        m_DecOpaqueAlloc.Out.Surfaces = &m_pSurfaceDecPool[0]; // vestor is stored linearly in memory
        m_DecOpaqueAlloc.Out.NumSurface = (mfxU16)m_pSurfaceDecPool.size();
        m_DecOpaqueAlloc.Out.Type = (mfxU16)(MFX_MEMTYPE_BASE(m_DecSurfaceType) | MFX_MEMTYPE_FROM_DECODE);
    }
    else 
    {
        // if no decoder in the pipeline we need to query m_DecOpaqueAlloc structure from parent sink pipeline
        m_DecOpaqueAlloc = m_pParentPipeline->GetDecOpaqueAlloc();
    }

    if (m_pmfxVPP.get())
    {
        m_EncOpaqueAlloc.In.Surfaces = &m_pSurfaceEncPool[0];
        m_EncOpaqueAlloc.In.NumSurface = (mfxU16)m_pSurfaceEncPool.size();
        m_EncOpaqueAlloc.In.Type = (mfxU16)(MFX_MEMTYPE_BASE(m_EncSurfaceType) | MFX_MEMTYPE_FROM_ENCODE);

        // decode will be connected with either VPP or Plugin
        if (m_bIsVpp)
            m_VppOpaqueAlloc.In = m_DecOpaqueAlloc.Out;
        else if (m_bIsPlugin)
            m_PluginOpaqueAlloc.In = m_DecOpaqueAlloc.Out;
        else
            return MFX_ERR_UNSUPPORTED;

        // encode will be connected with either Plugin or VPP
        if (m_bIsPlugin)
            m_PluginOpaqueAlloc.Out = m_EncOpaqueAlloc.In;
        else if (m_bIsVpp)
            m_VppOpaqueAlloc.Out = m_EncOpaqueAlloc.In;
        else
            return MFX_ERR_UNSUPPORTED;                             
    }
    else if (m_pmfxENC.get())
    {
        m_EncOpaqueAlloc.In = m_DecOpaqueAlloc.Out;
    }

    return MFX_ERR_NONE;    
}

void CTranscodingPipeline::FreeFrames()
{
    // free mfxFrameSurface structures and arrays of pointers
    mfxU32 i;
    for (i = 0; i < m_pSurfaceDecPool.size(); i++)
    {
        MSDK_SAFE_DELETE(m_pSurfaceDecPool[i]);
    }

    m_pSurfaceDecPool.clear();

    for (i = 0; i < m_pSurfaceEncPool.size(); i++)
    {
        MSDK_SAFE_DELETE(m_pSurfaceEncPool[i]);
    }

    m_pSurfaceEncPool.clear();

    if (m_pMFXAllocator && !m_bUseOpaqueMemory)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_mfxEncResponse);        
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_mfxDecResponse);
    }
} // CTranscodingPipeline::FreeFrames()

mfxStatus CTranscodingPipeline::Init(sInputParams *pParams,                 
                                     MFXFrameAllocator *pMFXAllocator,      
                                     void* hdl,                            
                                     CTranscodingPipeline *pParentPipeline, 
                                     SafetySurfaceBuffer  *pBuffer,
                                     BitstreamProcessor   *pBSProc,
                                     mfxVersion version)       
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pMFXAllocator, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;
    m_MaxFramesForTranscode = pParams->MaxFrameNumber;
    // use external allocator
    m_pMFXAllocator = pMFXAllocator;

    m_pParentPipeline = pParentPipeline;    

    m_nTimeout = pParams->nTimeout;
    (0 == pParams->nAsyncDepth)?m_AsyncDepth=1:m_AsyncDepth=pParams->nAsyncDepth;
    m_FrameNumberPreference = pParams->FrameNumberPreference;

    m_pBSStore.reset(new ExtendedBSStore(m_AsyncDepth));

    if (pBSProc)
    {
        sts = CheckExternalBSProcessor(pBSProc);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_pBSProcessor = pBSProc;
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // Determine processing mode
    switch(pParams->eMode)
    {
    case Native:
        break;
    case Sink:
        m_bEncodeEnable = false; // decode only
        break;
    case Source:
        // for heterogeneous pipeline use parent allocator
        MSDK_CHECK_POINTER(pParentPipeline, MFX_ERR_NULL_PTR);
        m_pMFXAllocator = pParentPipeline->m_pMFXAllocator;
        m_bDecodeEnable = false; // encode only
        break;
    default:
        // unknown mode
        return MFX_ERR_UNSUPPORTED;
    }

    m_pBuffer = pBuffer;
    
    // init session
    m_pmfxSession.reset(new MFXVideoSession);

    if (pParams->libType & MFX_IMPL_HARDWARE_ANY)
    {
        // try search for MSDK on all display adapters
        sts = m_pmfxSession->Init(pParams->libType, &version);

        // MSDK API version may have no support for multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts)
            sts = m_pmfxSession->Init(pParams->libType & !MFX_IMPL_HARDWARE_ANY | MFX_IMPL_HARDWARE, &version);                     
    }
    else
        sts = m_pmfxSession->Init(pParams->libType, &version);    

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // check the API version of actually loaded library
    sts = m_pmfxSession->QueryVersion(&version);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxIMPL impl = 0;
    m_pmfxSession->QueryIMPL(&impl);

    // opaque memory feature is available starting with API 1.3 and 
    // can be used within 1 intra session or joined inter sessions only
    if (version.Major >= 1 && version.Minor >= 3 && 
        (pParams->eMode == Native || pParams->bIsJoin))
        m_bUseOpaqueMemory = true;

    // Media SDK session doesn't require external allocator if the application uses opaque memory 
    if (!m_bUseOpaqueMemory)
    {
        sts = m_pmfxSession->SetFrameAllocator(m_pMFXAllocator);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    bool bIsInterOrJoined = pParams->eMode == Sink || pParams->eMode == Source || pParams->bIsJoin;

    mfxHandleType handleType = (mfxHandleType)0;
    bool bIsMustSetExternalHandle = 0;

    if (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl))
    {
        handleType = MFX_HANDLE_D3D11_DEVICE;
        bIsMustSetExternalHandle = false;
    }
    else if (MFX_IMPL_VIA_D3D9 == MFX_IMPL_VIA_MASK(impl))
    {
        handleType = MFX_HANDLE_D3D9_DEVICE_MANAGER;
        bIsMustSetExternalHandle = false;
    }
#ifdef LIBVA_SUPPORT 
    else if (MFX_IMPL_VIA_VAAPI == MFX_IMPL_VIA_MASK(impl))
    {
        handleType = MFX_HANDLE_VA_DISPLAY;
        bIsMustSetExternalHandle = true;
    }
#endif

    if (hdl && (bIsMustSetExternalHandle || (bIsInterOrJoined || !m_bUseOpaqueMemory)))
    {
      sts = m_pmfxSession->SetHandle(handleType, hdl);
      MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
      m_hdl = hdl; // save handle
    }

    // Initialize pipeline components following downstream direction
    // Pre-init methods fill parameters and create components

    // Decode component initialization  
    sts = DecodePreInit(pParams);
    if (MFX_ERR_MORE_DATA == sts)
        return sts;
    else
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // VPP component initialization  
    sts = VPPPreInit(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Encode component initialization  
    sts = EncodePreInit(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // Frames allocation for all component
    if (Native == pParams->eMode)
    {
        sts = AllocFrames();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    else if (Source == pParams->eMode)// need allocate frames only for VPP and Encode if VPP exist
    {
        mfxFrameAllocRequest  Request = {0};
        sts = CalculateNumberOfReqFrames(&Request);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_pParentPipeline->CorrectNumberOfAllocatedFrames(&Request);
    }
    // if sink - suspended allocation
    
    // common session settings
    if (pParams->bIsJoin && pParentPipeline)
    {
        sts = pParentPipeline->Join(m_pmfxSession.get());
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
        m_bIsJoinSession = true;
    }

    if (version.Major >= 1 && version.Minor >= 1)
        sts = m_pmfxSession->SetPriority(pParams->priority);

    // if sink - suspended allocation
    if (Native !=  pParams->eMode)
        return sts;

    // after surfaces arrays are allocated configure mfxOpaqueAlloc buffers to be passed to components' Inits
    if (m_bUseOpaqueMemory)
    {
        sts = InitOpaqueAllocBuffers();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    
    // Init decode
    if (m_pmfxDEC.get())
    {
        sts = m_pmfxDEC->Init(&m_mfxDecParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // Init VPP
    if (m_pmfxVPP.get())
    {
        if (m_bIsPlugin && m_bIsVpp)
            sts = m_pmfxVPP->Init(&m_mfxPluginParams, &m_mfxVppParams);
        else if (m_bIsPlugin)
            sts = m_pmfxVPP->Init(&m_mfxPluginParams);
        else
            sts = m_pmfxVPP->Init(&m_mfxVppParams);        
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // Init encode
    if (m_pmfxENC.get())
    {    
        sts = m_pmfxENC->Init(&m_mfxEncParams);        
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    m_bIsInit = true;
    return sts;

} //mfxStatus CTranscodingPipeline::Init(sInputParams *pParams)


mfxStatus CTranscodingPipeline::CompleteInit()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_bIsInit)
        return MFX_ERR_NONE;

    // need to allocate remaining frames
    if (m_bDecodeEnable)
    {
        sts = AllocFrames();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // after surfaces arrays are allocated configure mfxOpaqueAlloc buffers to be passed to components' Inits
    if (m_bUseOpaqueMemory)
    {
        sts = InitOpaqueAllocBuffers();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

        // Init decode
    if (m_pmfxDEC.get())
    {
        sts = m_pmfxDEC->Init(&m_mfxDecParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);        
        }
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // Init vpp
    if (m_pmfxVPP.get())
    {
        if (m_bIsPlugin && m_bIsVpp)
            sts = m_pmfxVPP->Init(&m_mfxPluginParams, &m_mfxVppParams);
        else if (m_bIsPlugin)
            sts = m_pmfxVPP->Init(&m_mfxPluginParams);
        else
            sts = m_pmfxVPP->Init(&m_mfxVppParams);        
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
    }

    // Init encode
    if (m_pmfxENC.get())
    {
        sts = m_pmfxENC->Init(&m_mfxEncParams);
        if (MFX_WRN_PARTIAL_ACCELERATION == sts)
        {
            msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
            MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);        
        }
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 
    }

    m_bIsInit = true;

    return sts;
} // mfxStatus CTranscodingPipeline::CompleteInit()
mfxFrameSurface1* CTranscodingPipeline::GetFreeSurface(bool isDec)
{
    SurfPointersArray *pArray;
    if (isDec)
        pArray = &m_pSurfaceDecPool;
    else
        pArray = &m_pSurfaceEncPool;

    for(mfxU32 i = 0; i < pArray->size(); i++)
    {
        if (!(*pArray)[i]->Data.Locked)
            return ((*pArray)[i]);
    }
    return NULL;
} // mfxFrameSurface1* CTranscodingPipeline::GetFreeSurface(bool isDec)

void CTranscodingPipeline::Close()
{   
    if (m_pmfxDEC.get())
        m_pmfxDEC->Close();
    
    if (m_pmfxENC.get())
        m_pmfxENC->Close();

    if (m_pmfxVPP.get())
        m_pmfxVPP->Close(); 
    
    if (m_pUserDecoderPlugin.get())
        m_pUserDecoderPlugin.reset();
    
    if (m_pUserEncoderPlugin.get())
        m_pUserEncoderPlugin.reset();
    
    FreeVppDoNotUse();
    FreeMVCSeqDesc();

    m_EncExtParams.clear();
    m_DecExtParams.clear();
    m_VppExtParams.clear();    
           
    if (m_bIsJoinSession)
    {
        //m_pmfxSession->DisjoinSession();
        m_bIsJoinSession = false;
    }

    // free allocated surfaces AFTER closing components
    FreeFrames();
    
    m_bIsInit = false;
 
} // void CTranscodingPipeline::Close()

mfxStatus CTranscodingPipeline::AllocAndInitVppDoNotUse()
{    
    m_VppDoNotUse.NumAlg = 2;

    m_VppDoNotUse.AlgList = new mfxU32 [m_VppDoNotUse.NumAlg];    
    MSDK_CHECK_POINTER(m_VppDoNotUse.AlgList,  MFX_ERR_MEMORY_ALLOC);

    m_VppDoNotUse.AlgList[0] = MFX_EXTBUFF_VPP_DENOISE; // turn off denoising (on by default)
    m_VppDoNotUse.AlgList[1] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS; // turn off scene analysis (on by default)
    
    return MFX_ERR_NONE;

} // CTranscodingPipeline::AllocAndInitVppDoNotUse()

void CTranscodingPipeline::FreeVppDoNotUse()
{
    MSDK_SAFE_DELETE_ARRAY(m_VppDoNotUse.AlgList);
}

mfxStatus CTranscodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{    
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts); 

    mfxU32 new_size = 0;
    // if encoder provided us information about buffer size
    if (0 != par.mfx.BufferSizeInKB)
    {
        new_size = par.mfx.BufferSizeInKB * 1000;
    }
    else
    {
        // trying to guess the size (e.g. for JPEG encoder)
        new_size = (0 == pBS->MaxLength)
            // some heuristic init value
            ? 4 + (par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 3 + 1023)
            // double existing size
            : 2 * pBS->MaxLength;
    }

    sts = ExtendMfxBitstream(pBS, new_size);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
} // CTranscodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)

mfxStatus CTranscodingPipeline::Join(MFXVideoSession *pChildSession)
{
    mfxStatus sts = MFX_ERR_NONE;
    MSDK_CHECK_POINTER(pChildSession, MFX_ERR_NULL_PTR); 
    sts = m_pmfxSession->JoinSession(*pChildSession);
    m_bIsJoinSession = (MFX_ERR_NONE == sts);
    return sts;
} // CTranscodingPipeline::Join(MFXVideoSession *pChildSession)

 mfxStatus CTranscodingPipeline::Run()
{
    if (m_bDecodeEnable && m_bEncodeEnable)
        return Transcode();
    else if (m_bDecodeEnable)
        return Decode();
    else if (m_bEncodeEnable)
        return Encode();
    else
        return MFX_ERR_UNSUPPORTED;
}

mfxStatus CTranscodingPipeline::CheckExternalBSProcessor(BitstreamProcessor   *pBSProc)
{
    FileBitstreamProcessor *pProc = dynamic_cast<FileBitstreamProcessor*>(pBSProc);
    if (!pProc)
        return MFX_ERR_UNSUPPORTED;
    
    return MFX_ERR_NONE;
}//  mfxStatus CTranscodingPipeline::CheckExternalBSProcessor()

void IncreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_inc16((volatile mfxU16 *)(&ptr->Locked));
}

void DecreaseReference(mfxFrameData *ptr)
{
    msdk_atomic_dec16((volatile mfxU16 *)&ptr->Locked);
}

SafetySurfaceBuffer::SafetySurfaceBuffer(SafetySurfaceBuffer *pNext):m_pNext(pNext)
{
} // SafetySurfaceBuffer::SafetySurfaceBuffer

SafetySurfaceBuffer::~SafetySurfaceBuffer()
{
} //SafetySurfaceBuffer::~SafetySurfaceBuffer()
 
void SafetySurfaceBuffer::AddSurface(ExtendedSurface Surf)
{
    AutomaticMutex  guard(m_mutex);

    SurfaceDescriptor sDescriptor;
    // Locked is used to signal when we can free surface
    sDescriptor.Locked     = 1;
    sDescriptor.ExtSurface = Surf;
    
    if (Surf.pSurface)
    {
        IncreaseReference(&Surf.pSurface->Data);
    }

    m_SList.push_back(sDescriptor);

} // SafetySurfaceBuffer::AddSurface(mfxFrameSurface1 *pSurf)

mfxStatus SafetySurfaceBuffer::GetSurface(ExtendedSurface &Surf)
{
    // no ready surfaces
    if (0 == m_SList.size())
    {
        return MFX_ERR_MORE_SURFACE;
    }

    AutomaticMutex guard(m_mutex);
    SurfaceDescriptor sDescriptor = m_SList.front();

    Surf.pSurface = sDescriptor.ExtSurface.pSurface;
    Surf.Syncp = sDescriptor.ExtSurface.Syncp;

    
    return MFX_ERR_NONE;

} // SafetySurfaceBuffer::GetSurface()

mfxStatus SafetySurfaceBuffer::ReleaseSurface(mfxFrameSurface1* pSurf)
{
    AutomaticMutex guard(m_mutex);
    std::list<SurfaceDescriptor>::iterator it;
    for (it = m_SList.begin(); it != m_SList.end(); it++)
    {
        if (pSurf == it->ExtSurface.pSurface)
        {
            it->Locked--;
            if (it->ExtSurface.pSurface)
                DecreaseReference(&it->ExtSurface.pSurface->Data);
            if (0 == it->Locked)
                m_SList.erase(it);
            return MFX_ERR_NONE;
        }
    }
    return MFX_ERR_UNKNOWN;

} // mfxStatus SafetySurfaceBuffer::ReleaseSurface(mfxFrameSurface1* pSurf)

FileBitstreamProcessor::FileBitstreamProcessor()
{
    MSDK_ZERO_MEMORY(m_Bitstream);
} // FileBitstreamProcessor::FileBitstreamProcessor()

FileBitstreamProcessor::~FileBitstreamProcessor()
{
    if (m_pFileReader.get())
        m_pFileReader->Close();
    if (m_pFileWriter.get())
        m_pFileWriter->Close();
    WipeMfxBitstream(&m_Bitstream);
} // FileBitstreamProcessor::~FileBitstreamProcessor()

mfxStatus FileBitstreamProcessor::Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile)
{
    mfxStatus sts;
    if (pStrSrcFile)
    {
        m_pFileReader.reset(new CSmplBitstreamReader());
        sts = m_pFileReader->Init(pStrSrcFile);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    
    if (pStrDstFile)
    {
        m_pFileWriter.reset(new CSmplBitstreamWriter);
        sts = m_pFileWriter->Init(pStrDstFile);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    sts = InitMfxBitstream(&m_Bitstream, 1024 * 1024);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);  

    return MFX_ERR_NONE;

} // FileBitstreamProcessor::Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile)

mfxStatus FileBitstreamProcessor::GetInputBitstream(mfxBitstream **pBitstream)
{
    mfxStatus sts = m_pFileReader->ReadNextFrame(&m_Bitstream);
    if (MFX_ERR_NONE == sts)
    {
        *pBitstream = &m_Bitstream;
        return sts;
    }
    return sts;

} //  FileBitstreamProcessor::GetInputBitstream(mfxBitstream* pBitstream)

mfxStatus FileBitstreamProcessor::ProcessOutputBitstream(mfxBitstream* pBitstream)
{
    if (m_pFileWriter.get())
        return m_pFileWriter->WriteNextFrame(pBitstream, false);
    else
        return MFX_ERR_NONE;

} // mfxStatus FileBitstreamProcessor::ProcessOutputBitstream(mfxBitstream* pBitstream)

mfxStatus FileBitstreamProcessor_WithReset::Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile)
{
    mfxStatus sts;
    if (pStrSrcFile)
    {
        size_t SrcFileNameSize = msdk_strlen(pStrSrcFile);
        m_pSrcFile.assign(pStrSrcFile, pStrSrcFile + SrcFileNameSize + 1);
        m_pFileReader.reset(new CSmplBitstreamReader());
        sts = m_pFileReader->Init(pStrSrcFile);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    } else
    {
        m_pSrcFile.resize(1, 0);
    }

    if (pStrDstFile)
    {
        size_t DstFileNameSize = msdk_strlen(pStrDstFile);
        m_pDstFile.assign(pStrDstFile, pStrDstFile + DstFileNameSize + 1);
        m_pFileWriter.reset(new CSmplBitstreamWriter);
        sts = m_pFileWriter->Init(pStrDstFile);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }
    else
    {
        m_pDstFile.resize(1, 0);
    }

    sts = InitMfxBitstream(&m_Bitstream, 1024 * 1024);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);  

    return MFX_ERR_NONE;

} // FileBitstreamProcessor_Benchmark::Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile)

mfxStatus FileBitstreamProcessor_WithReset::ResetInput()
{
    mfxStatus sts = m_pFileReader->Init(&m_pSrcFile.front());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    return MFX_ERR_NONE;
} // FileBitstreamProcessor_Benchmark::ResetInput()

mfxStatus FileBitstreamProcessor_WithReset::ResetOutput()
{
    mfxStatus sts = m_pFileWriter->Init(&m_pDstFile.front());
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);   
    return MFX_ERR_NONE;
} // FileBitstreamProcessor_Benchmark::ResetOutput()