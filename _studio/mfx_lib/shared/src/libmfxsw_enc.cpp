// Copyright (c) 2008-2020 Intel Corporation
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

#include <mfxvideo.h>

#if defined(MFX_ONEVPL)
#include "mfxstructures-int.h"

#define FUNCTION_DEPRECATED_IMPL(component, func_name, formal_param_list) \
mfxStatus APIImpl_MFXVideo##component##_##func_name formal_param_list \
{ \
    return MFX_ERR_UNSUPPORTED; \
}

FUNCTION_DEPRECATED_IMPL(ENC, QueryIOSurf,       (mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request))
FUNCTION_DEPRECATED_IMPL(ENC, Init,              (mfxSession session, mfxVideoParam *par))
FUNCTION_DEPRECATED_IMPL(ENC, Close,             (mfxSession session))
FUNCTION_DEPRECATED_IMPL(ENC, Reset,             (mfxSession session, mfxVideoParam *par))
FUNCTION_DEPRECATED_IMPL(ENC, Query,             (mfxSession session, mfxVideoParam *in, mfxVideoParam *out))
FUNCTION_DEPRECATED_IMPL(ENC, GetVideoParam,     (mfxSession session, mfxVideoParam *par))
FUNCTION_DEPRECATED_IMPL(ENC, ProcessFrameAsync, (mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp))
#undef FUNCTION_DEPRECATED_IMPL
#else
#include <mfx_session.h>
#include <mfx_common.h>

// sheduling and threading stuff
#include <mfx_task.h>

#include "mfxenc.h"
#include "mfx_enc_ext.h"

#ifdef MFX_VA

#ifdef MFX_ENABLE_H264_VIDEO_ENC_HW
#include "mfx_h264_enc_hw.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_LA_H264_VIDEO_HW)
#include "mfx_h264_la.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
#include "mfx_h264_preenc.h"
#endif

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
#include "mfx_h264_enc.h"
#endif

#ifdef MFX_ENABLE_H265FEI_HW
#include "mfx_h265_enc_cm_plugin.h"
#endif

#else //MFX_VA

#ifdef MFX_ENABLE_MPEG2_VIDEO_ENC
#include "mfx_mpeg2_enc.h"
#endif

#ifdef MFX_ENABLE_H264_VIDEO_ENC
#include "mfx_h264_enc_enc.h"
#endif


#endif //MFX_VA

template<>
VideoENC* _mfxSession::Create<VideoENC>(mfxVideoParam& par)
{
    VideoENC* pENC = nullptr;
#if defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA) || (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW)|| defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_ENC))&& defined (MFX_VA) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined(MFX_VA)
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;
    mfxU32 codecId = par.mfx.CodecId;

    switch (codecId)
    {
#if defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA) || (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_ENC))&& defined (MFX_VA)
    case MFX_CODEC_AVC:
#ifdef MFX_VA
#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
        pENC = new MFXHWVideoENCH264(m_pCORE.get(), &mfxRes);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(&par))
            pENC = (VideoENC*) new VideoENC_LA(m_pCORE.get(), &mfxRes);
#endif
#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
        if (bEnc_PREENC(&par))
            pENC = (VideoENC*) new VideoENC_PREENC(m_pCORE.get(), &mfxRes);
#endif
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW)
        if (bEnc_ENC(&par))
            pENC = (VideoENC*) new VideoENC_ENC(m_pCORE.get(), &mfxRes);
#endif
#else //MFX_VA
        pENC = new MFXVideoEncH264(m_pCORE.get(), &mfxRes);
#endif //MFX_VA
        break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_H

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
    case MFX_CODEC_HEVC:
        pENC = (VideoENC*) new VideoENC_H265FEI(m_pCORE.get(), &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined(MFX_VA)
    case MFX_CODEC_MPEG2:
        pENC = new MFXVideoENCMPEG2(m_pCORE.get(), &mfxRes);
        break;
#endif

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pENC;
        pENC = nullptr;
    }
#else
    (void)par;
#endif

    return pENC;
}

mfxStatus MFXVideoENC_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    (void)in;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    try
    {

#ifdef MFX_ENABLE_USER_ENC
        mfxRes = MFX_ERR_UNSUPPORTED;

        MFXIPtr<MFXISession_1_10> newSession = TryGetSession_1_10(session);

        if (newSession && newSession->GetPreEncPlugin().get())
        {
            mfxRes = newSession->GetPreEncPlugin()->Query(session->m_pCORE.get(), in, out);
        }
        // unsupported reserved to codecid != requested codecid
        if (MFX_ERR_UNSUPPORTED == mfxRes)
#endif
        switch (out->mfx.CodecId)
        {
#if (defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA)) || \
    (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW) || defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC))&& defined (MFX_VA)
        case MFX_CODEC_AVC:
#ifdef MFX_VA
#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
            mfxRes = MFXHWVideoENCH264::Query(in, out);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(in))
            mfxRes = VideoENC_LA::Query(session->m_pCORE.get(), in, out);
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
        if (bEnc_PREENC(out))
            mfxRes = VideoENC_PREENC::Query(session->m_pCORE.get(),in, out);
#endif
#else //MFX_VA
            mfxRes = MFXVideoEncH264::Query(in, out);
#endif //MFX_VA
            break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_H

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
        case MFX_CODEC_HEVC:
            mfxRes = VideoENC_H265FEI::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined (MFX_VA)
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoENCMPEG2::Query(in, out);
            break;
#endif // MFX_ENABLE_MPEG2_VIDEO_ENC && !MFX_VA

        case 0:
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoENC_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    mfxStatus mfxRes = MFX_ERR_UNSUPPORTED;
    try
    {
#ifdef MFX_ENABLE_USER_ENC
        mfxRes = MFX_ERR_UNSUPPORTED;
        MFXIPtr<MFXISession_1_10> newSession=TryGetSession_1_10(session);
        if (newSession && newSession->GetPreEncPlugin().get())
        {
            mfxRes = newSession->GetPreEncPlugin()->QueryIOSurf(session->m_pCORE.get(), par, request, 0);
        }
        // unsupported reserved to codecid != requested codecid
        if (MFX_ERR_UNSUPPORTED == mfxRes)
#endif
        switch (par->mfx.CodecId)
        {

#if defined (MFX_ENABLE_H264_VIDEO_ENC) && !defined (MFX_VA) || \
    (defined (MFX_ENABLE_H264_VIDEO_ENC_HW) || defined(MFX_ENABLE_LA_H264_VIDEO_HW) || \
     defined (MFX_ENABLE_H264_VIDEO_FEI_PREENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)) && \
     defined (MFX_VA)
        case MFX_CODEC_AVC:
#ifdef MFX_VA

#if defined (MFX_ENABLE_H264_VIDEO_ENC_HW)
            mfxRes = MFXHWVideoENCH264::QueryIOSurf(par, request);
#endif
#if defined(MFX_ENABLE_LA_H264_VIDEO_HW)
        if (bEnc_LA(par))
            mfxRes = VideoENC_LA::QueryIOSurf(session->m_pCORE.get(), par, request);
#endif
#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW)
        if (bEnc_ENC(par))
            mfxRes = VideoENC_ENC::QueryIOSurf(session->m_pCORE.get(), par, request);
#endif

#else //MFX_VA
            mfxRes = MFXVideoEncH264::QueryIOSurf(par, request);
#endif //MFX_VA
            break;
#endif // MFX_ENABLE_H264_VIDEO_ENC || MFX_ENABLE_H264_VIDEO_ENC_HW

#if defined (MFX_VA) && defined (MFX_ENABLE_H265FEI_HW)
        case MFX_CODEC_HEVC:
            mfxRes = VideoENC_H265FEI::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENC) && !defined (MFX_VA)
        case MFX_CODEC_MPEG2:
            mfxRes = MFXVideoENCMPEG2::QueryIOSurf(par, request);
            break;
#endif // MFX_ENABLE_MPEG2_VIDEO_ENC && !MFX_VA

        case 0:
        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}

mfxStatus MFXVideoENC_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    try
    {
#if !defined (MFX_RT)
        // check existence of component
        if (!session->m_pENC)
        {
            // create a new instance
            session->m_pENC.reset(session->Create<VideoENC>(*par));
            MFX_CHECK(session->m_pENC.get(), MFX_ERR_INVALID_VIDEO_PARAM);
        }
#endif

        mfxRes = session->m_pENC->Init(par);
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
}

mfxStatus MFXVideoENC_Close(mfxSession session)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    try
    {
        if (!session->m_pENC)
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForAllTasksCompletion(session->m_pENC.get());

        mfxRes = session->m_pENC->Close();
        // delete the codec's instance
        session->m_pENC.reset(nullptr);
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
}

static
mfxStatus MFXVideoENCLegacyRoutineExt(void *pState, void *pParam,
                                   mfxU32 threadNumber, mfxU32 callNumber)
{
    (void)callNumber;

    VideoENC_Ext * pENC = (VideoENC_Ext  *) pState;
    MFX_THREAD_TASK_PARAMETERS *pTaskParam = (MFX_THREAD_TASK_PARAMETERS *) pParam;

    // check error(s)
    if ((NULL == pState) ||
        (NULL == pParam) ||
        (0 != threadNumber))
    {
        return MFX_ERR_NULL_PTR;
    }
    return pENC->RunFrameVmeENC(pTaskParam->enc.in, pTaskParam->enc.out);
}

enum
{
    MFX_NUM_ENTRY_POINTS = 2
};


mfxStatus  MFXVideoENC_ProcessFrameAsync(mfxSession session, mfxENCInput *in, mfxENCOutput *out, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pENC.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    VideoENC_Ext *pEnc = dynamic_cast<VideoENC_Ext *>(session->m_pENC.get());
    MFX_CHECK(pEnc, MFX_ERR_INVALID_HANDLE);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_ENTRY_POINT entryPoints[MFX_NUM_ENTRY_POINTS];
        mfxU32 numEntryPoints = MFX_NUM_ENTRY_POINTS;
        memset(&entryPoints, 0, sizeof(entryPoints));

        mfxRes = pEnc->RunFrameVmeENCCheck(in,out,entryPoints,numEntryPoints);

        if ((MFX_ERR_NONE == mfxRes) ||
            (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxRes) ||
            (MFX_WRN_OUT_OF_RANGE == mfxRes) ||
            (MFX_ERR_MORE_DATA_SUBMIT_TASK == static_cast<int>(mfxRes)) ||
            (MFX_ERR_MORE_BITSTREAM == mfxRes))
        {
            // prepare the absolete kind of task.
            // it is absolete and must be removed.
            if (NULL == entryPoints[0].pRoutine)
            {
                MFX_TASK task;
                memset(&task, 0, sizeof(task));
                // BEGIN OF OBSOLETE PART
                task.bObsoleteTask = true;
                // fill task info
                task.entryPoint.pRoutine = &MFXVideoENCLegacyRoutineExt;
                task.entryPoint.pState = pEnc;
                task.entryPoint.requiredNumThreads = 1;

                // fill legacy parameters
                task.obsolete_params.enc.in = in;
                task.obsolete_params.enc.out = out;

                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = in;
                task.pDst[0] = out;
                task.pOwner= pEnc;
                mfxRes = session->m_pScheduler->AddTask(task, &syncPoint);

            } // END OF OBSOLETE PART
            else if (1 == numEntryPoints)
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies

                task.pSrc[0] =  out; // for LA plugin
                task.pSrc[1] =  in->InSurface;
                task.pDst[0] =  out? out->ExtParam : 0;

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }
            else
            {
                MFX_TASK task;

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[0];
                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies

                task.pSrc[0] = in->InSurface;
                task.pDst[0] = entryPoints[0].pParam;

                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));

                memset(&task, 0, sizeof(task));
                task.pOwner = pEnc;
                task.entryPoint = entryPoints[1];
                task.priority = session->m_priority;
                task.threadingPolicy = pEnc->GetThreadingPolicy();
                // fill dependencies
                task.pSrc[0] = entryPoints[0].pParam;
                task.pDst[0] = (MFX_ERR_NONE == mfxRes) ? out:0; // sync point for LA plugin
                task.pDst[1] = in->InSurface;


                // register input and call the task
                MFX_CHECK_STS(session->m_pScheduler->AddTask(task, &syncPoint));
            }

            // IT SHOULD BE REMOVED
            if (MFX_ERR_MORE_DATA_SUBMIT_TASK == static_cast<int>(mfxRes))
            {
                mfxRes = MFX_ERR_MORE_DATA;
                syncPoint = NULL;
            }

        }

        // return pointer to synchronization point
        *syncp = syncPoint;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;

} // MFXVideoENC_ProcessFrameAsync

//
// THE OTHER ENC FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(ENC, Reset, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENC, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(ENC, GetFrameParam, (mfxSession session, mfxFrameParam *par), (par))
#endif //MFX_ONEVPL
