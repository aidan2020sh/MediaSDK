/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: libmfxsw_pak.cpp

\* ****************************************************************************** */

#include <mfxvideo.h>
#include <mfxfei.h>

#include <mfx_session.h>
#include <mfx_common.h>

// sheduling and threading stuff
#include <mfx_task.h>

#ifdef MFX_ENABLE_VC1_VIDEO_PAK
#include "mfx_vc1_enc_defs.h"
#include "mfx_vc1_enc_pak_adv.h"
#include "mfx_vc1_enc_pak_sm.h"
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_PAK
#include "mfx_mpeg2_pak.h"
#endif

#ifdef MFX_ENABLE_H264_VIDEO_PAK
#include "mfx_h264_pak.h"
#endif

#if !defined (MFX_RT)
VideoPAK *CreatePAKSpecificClass(mfxU32 codecId, mfxU32 codecProfile, VideoCORE *pCore)
{
    VideoPAK *pPAK = (VideoPAK *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    // touch unreferenced parameters
    codecProfile = codecProfile;

    switch (codecId)
    {
#ifdef MFX_ENABLE_H264_VIDEO_PAK
    case MFX_CODEC_AVC:
        pPAK = new MFXVideoPAKH264(pCore, &mfxRes);
        break;
#endif // MFX_ENABLE_H264_VIDEO_PAK

#ifdef MFX_ENABLE_VC1_VIDEO_PAK
    case MFX_CODEC_VC1:
        if (codecProfile == MFX_PROFILE_VC1_ADVANCED)
        {
            pPAK = new MFXVideoPakVc1ADV(pCore, &mfxRes);
        }
        else
        {
            pPAK = new MFXVideoPakVc1SM(pCore, &mfxRes);
        }
        break;
#endif // MFX_ENABLE_VC1_VIDEO_PAK

#ifdef MFX_ENABLE_MPEG2_VIDEO_PAK
    case MFX_CODEC_MPEG2:
        pPAK = new MFXVideoPAKMPEG2(pCore, &mfxRes);
        break;
#endif // MFX_ENABLE_MPEG2_VIDEO_PAK

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pPAK;
        pPAK = (VideoPAK *) 0;
    }

    return pPAK;

} // VideoPAK *CreatePAKSpecificClass(mfxU32 codecId, mfxU32 codecProfile, VideoCORE *pCore)
#endif

mfxStatus MFXVideoPAK_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_PAK
        case MFX_CODEC_VC1:
            if (out->mfx.CodecProfile == MFX_PROFILE_VC1_ADVANCED)
            {
                mfxRes = MFXVideoPakVc1ADV::Query(in, out);
            }
            else
            {
                mfxRes = MFXVideoPakVc1SM::Query(in, out);
            }
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_PAK
        case MFX_CODEC_AVC:
            mfxRes = MFXVideoPAKH264::Query(in, out);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_PAK
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoPAKMPEG2::Query(in, out);
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoPAK_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes;
    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_PAK
        case MFX_CODEC_VC1:
            if (par->mfx.CodecProfile == MFX_PROFILE_VC1_ADVANCED)
            {
                mfxRes = MFXVideoPakVc1ADV::QueryIOSurf(par, request);
            }
            else
            {
                mfxRes = MFXVideoPakVc1SM::QueryIOSurf(par, request);
            }
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_PAK
        case MFX_CODEC_AVC:
            mfxRes = MFXVideoPAKH264::QueryIOSurf(par, request);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_PAK
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoPAKMPEG2::QueryIOSurf(par, request);
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    try
    {
        // close the existing PAK unit,
        // if it is initialized.
        if (session->m_pPAK.get())
        {
            MFXVideoPAK_Close(session);
        }

        // create a new instance
        session->m_pPAK.reset(CreatePAKSpecificClass(par->mfx.CodecId,
                                                     par->mfx.CodecProfile,
                                                     session->m_pCORE.get()));
        MFX_CHECK(session->m_pPAK.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        mfxRes = session->m_pPAK->Init(par);
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pPAK.get())
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoPAK_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXVideoPAK_Close(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);

    try
    {
        if (!session->m_pPAK.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pPAK.get());

        mfxRes = session->m_pPAK->Close();
        // delete the codec's instance
        session->m_pPAK.reset((VideoPAK *) 0);
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoPAK_Close(mfxSession session)

static
mfxStatus MFXVideoPAKLegacyRoutine(void *pState, void *pParam,
                                   mfxU32 threadNumber, mfxU32 callNumber)
{
    VideoPAK *pPAK = (VideoPAK *) pState;
    VideoBRC *pBRC;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;
    mfxStatus mfxRes;

    // touch unreferenced parameter(s)
    callNumber = callNumber;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }

    // get the BRC pointer
    pBRC = (VideoBRC *) pTaskParam->pak.pBRC;

    // call the obsolete method
    mfxRes = pBRC->FramePAKRefine(pTaskParam->pak.cuc);
    if (MFX_ERR_NONE == mfxRes)
    {
        mfxRes = pPAK->RunFramePAK(pTaskParam->pak.cuc);
    }
    if (MFX_ERR_NONE == mfxRes)
    {
        mfxRes = pBRC->FramePAKRecord(pTaskParam->pak.cuc);
    }

    return mfxRes;

} // mfxStatus MFXVideoPAKLegacyRoutine(void *pState, void *pParam,

mfxStatus MFXVideoPAK_ProcessFrameAsync(mfxSession session, mfxPAKInput *in, mfxPAKOutput *out, mfxSyncPoint *syncp)
{
    return MFX_ERR_UNSUPPORTED;
}

mfxStatus MFXVideoPAK_RunFramePAKAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pPAK.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pPAK->RunFramePAKCheck(cuc, &task.entryPoint);
        // source data is OK, go forward
        if (MFX_ERR_NONE == mfxRes)
        {
            // prepare the absolete kind of task.
            // it is absolete and must be removed.
            if (NULL == task.entryPoint.pRoutine)
            {
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                // fill task info
                task.entryPoint.pRoutine = &MFXVideoPAKLegacyRoutine;
                task.entryPoint.pState = session->m_pPAK.get();
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.pak.cuc = cuc;
                task.obsolete_params.pak.pBRC = session->m_pBRC.get();

            } // END OF OBSOLETE PART

            task.pOwner = session->m_pPAK.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pPAK->GetThreadingPolicy();
            // fill dependencies
            task.pSrc[0] = cuc;
            task.pDst[0] = cuc;
            task.pDst[1] = cuc->Bitstream;

            // register input and call the task
            mfxRes = session->m_pScheduler->AddTask(task, &syncPoint);

        }

        // return pointer to synchronization point
        *syncp = syncPoint;
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pPAK.get())
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    return mfxRes;

} // mfxStatus MFXVideoPAK_RunFramePAKAsync(mfxSession session, mfxFrameCUC *cuc, mfxSyncPoint *syncp)

//
// THE OTHER PAK FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(PAK, Reset, (mfxSession session, mfxVideoParam *par), (par))

FUNCTION_IMPL(PAK, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(PAK, GetFrameParam, (mfxSession session, mfxFrameParam *par), (par))
FUNCTION_IMPL(PAK, RunSeqHeader, (mfxSession session, mfxFrameCUC *cuc), (cuc))
