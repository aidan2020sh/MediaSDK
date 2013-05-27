/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

File Name: libmfxsw_decode.cpp

\* ****************************************************************************** */

#include <mfxvideo.h>

#include <mfx_session.h>
#include <mfx_tools.h>
#include <mfx_common.h>

// sheduling and threading stuff
#include <mfx_task.h>

#include <libmfx_core.h>

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
#include "mfx_vc1_decode.h"
#endif

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)
#include "mfx_h264_dec_decode.h"
#endif

#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
#include "mfx_mpeg2_decode.h"
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
#include "mfx_mjpeg_dec_decode.h"
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
#include "mfx_vp8_dec_decode.h"
#if defined(MFX_VA) && defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW) 
#include "mfx_vp8_dec_decode_hw.h"
#endif
#endif

VideoDECODE *CreateDECODESpecificClass(mfxU32 CodecId, VideoCORE *core, mfxSession session)
{
    VideoDECODE *pDECODE = (VideoDECODE *) 0;
    mfxStatus mfxRes = MFX_ERR_MEMORY_ALLOC;

    // touch unreferenced parameter
    session = session;

    // create a codec instance
    switch (CodecId)
    {
#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)
    case MFX_CODEC_MPEG2:
        pDECODE = new VideoDECODEMPEG2(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
    case MFX_CODEC_VC1:
        pDECODE = new MFXVideoDECODEVC1(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)
    case MFX_CODEC_AVC:
        pDECODE = new VideoDECODEH264(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)
    case MFX_CODEC_JPEG:
        pDECODE = new VideoDECODEMJPEG(core, &mfxRes);
        break;
#endif

#if defined (MFX_ENABLE_VP8_VIDEO_DECODE)
     case MFX_CODEC_VP8:
#if defined(MFX_VA) && defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW)
        if (session->m_bIsHWDECSupport)
        {
            pDECODE = new VideoDECODEVP8_HW(core, &mfxRes);
        }
        else
        {
            pDECODE = new VideoDECODEVP8(core, &mfxRes);
        }
#else // MFX_VA
        pDECODE = new VideoDECODEVP8(core, &mfxRes);

#endif // MFX_VA && MFX_ENABLE_VP8_VIDEO_DECODE_HW

        break;
#endif

    default:
        break;
    }

    // check error(s)
    if (MFX_ERR_NONE != mfxRes)
    {
        delete pDECODE;
        pDECODE = (VideoDECODE *) 0;
    }

    return pDECODE;

} // VideoDECODE *CreateDECODESpecificClass(mfxU32 CodecId, VideoCORE *core)

mfxStatus MFXVideoDECODE_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(out, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, in);

    mfxStatus mfxRes;

    bool bIsHWDECSupport = false;
    bIsHWDECSupport = bIsHWDECSupport;

    try
    {
        switch (out->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::Query(session->m_pCORE.get(), in, out);
            break;
#endif

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE
        case MFX_CODEC_VP8:
#if defined(MFX_VA) && defined (MFX_ENABLE_VP8_VIDEO_DECODE_HW)
            mfxRes = VideoDECODEVP8_HW::Query(session->m_pCORE.get(), in, out);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
            {
                mfxRes = VideoDECODEVP8::Query(session->m_pCORE.get(), in, out);
            }
            else
            {
                bIsHWDECSupport = true;
            }
#else
            mfxRes = VideoDECODEVP8::Query(session->m_pCORE.get(), in, out);
#endif // MFX_VA && MFX_ENABLE_VP8_VIDEO_DECODE_HW
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, out);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);
    MFX_CHECK(request, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    bool bIsHWDECSupport = false;
    bIsHWDECSupport = bIsHWDECSupport;

    mfxStatus mfxRes;
    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::QueryIOSurf(session->m_pCORE.get(), par, request);
            break;
#endif

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE
        case MFX_CODEC_VP8:
#if defined(MFX_VA) && defined (MFX_ENABLE_VP8_VIDEO_DECODE_HW)
            mfxRes = VideoDECODEVP8_HW::QueryIOSurf(session->m_pCORE.get(), par, request);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
            {
                mfxRes = VideoDECODEVP8::QueryIOSurf(session->m_pCORE.get(), par, request);
            }
            else
            {
                bIsHWDECSupport = true;
            }
#else
            mfxRes = VideoDECODEVP8::QueryIOSurf(session->m_pCORE.get(), par, request);
#endif // MFX_VA && MFX_ENABLE_VP8_VIDEO_DECODE_HW
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, request);
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_DecodeHeader(mfxSession session, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(bs, MFX_ERR_NULL_PTR);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    mfxStatus mfxRes;
    try
    {
        switch (par->mfx.CodecId)
        {
#ifdef MFX_ENABLE_VC1_VIDEO_DECODE
        case MFX_CODEC_VC1:
            mfxRes = MFXVideoDECODEVC1::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_H264_VIDEO_DECODE
        case MFX_CODEC_AVC:
            mfxRes = VideoDECODEH264::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_MPEG2_VIDEO_DECODE
        case MFX_CODEC_MPEG2:
            mfxRes = VideoDECODEMPEG2::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_MJPEG_VIDEO_DECODE
        case MFX_CODEC_JPEG:
            mfxRes = VideoDECODEMJPEG::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE
        case MFX_CODEC_VP8:
            mfxRes = VideoDECODEVP8::DecodeHeader(session->m_pCORE.get(), bs, par);
            break;
#endif

        default:
            mfxRes = MFX_ERR_UNSUPPORTED;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;
}

mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)
{
    mfxStatus mfxRes;
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, par);

    try
    {
        // check existence of component
        if (!session->m_pDECODE.get())
        {
            // create a new instance
            session->m_bIsHWDECSupport = true;
            session->m_pDECODE.reset(CreateDECODESpecificClass(par->mfx.CodecId, session->m_pCORE.get(), session));
        }
        
        mfxRes = session->m_pDECODE->Init(par);

        if (MFX_CODEC_VP8 == par->mfx.CodecId)
        {
            if (MFX_WRN_PARTIAL_ACCELERATION == mfxRes)
            {
                session->m_bIsHWDECSupport = false;
                session->m_pDECODE.reset(CreateDECODESpecificClass(par->mfx.CodecId, session->m_pCORE.get(), session));
                mfxRes = session->m_pDECODE->Init(par);
            }

            // SW fallback if EncodeGUID is absence
            if (MFX_PLATFORM_HARDWARE == session->m_currentPlatform &&
                !session->m_bIsHWDECSupport &&
                MFX_ERR_NONE <= mfxRes)
            {
                mfxRes = MFX_WRN_PARTIAL_ACCELERATION;
            }
        }
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
        else if (0 == session->m_pDECODE.get())
        {
            mfxRes = MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else if (0 == par)
        {
            mfxRes = MFX_ERR_NULL_PTR;
        }
    }

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoDECODE_Init(mfxSession session, mfxVideoParam *par)

mfxStatus MFXVideoDECODE_Close(mfxSession session)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_AUTO_LTRACE_FUNC(MFX_TRACE_LEVEL_API);

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);

    try
    {
        if (!session->m_pDECODE.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }

        // wait until all tasks are processed
        session->m_pScheduler->WaitForTaskCompletion(session->m_pDECODE.get());

        mfxRes = session->m_pDECODE->Close();
        // delete the codec's instance
        session->m_pDECODE.reset((VideoDECODE *) 0);
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

    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);
    return mfxRes;

} // mfxStatus MFXVideoDECODE_Close(mfxSession session)

mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    mfxStatus mfxRes;

#ifdef MFX_TRACE_ENABLE
    MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_API, "MFX_DecodeFrameAsync");
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, bs);
    MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, surface_work);
#endif

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(session->m_pDECODE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(syncp, MFX_ERR_NULL_PTR);

    try
    {
        mfxSyncPoint syncPoint = NULL;
        MFX_TASK task;

        // Wait for the bit stream
        mfxRes = session->m_pScheduler->WaitForDependencyResolved(bs);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // reset the sync point
        *syncp = NULL;

        memset(&task, 0, sizeof(MFX_TASK));
        mfxRes = session->m_pDECODE->DecodeFrameCheck(bs, surface_work, surface_out, &task.entryPoint);
        // source data is OK, go forward
        if (task.entryPoint.pRoutine)
        {
            mfxStatus mfxAddRes;

            task.pOwner = session->m_pDECODE.get();
            task.priority = session->m_priority;
            task.threadingPolicy = session->m_pDECODE->GetThreadingPolicy();
            // fill dependencies
            task.pDst[0] = *surface_out;

#ifdef MFX_TRACE_ENABLE
            task.nParentId = MFX_AUTO_TRACE_GETID();
            task.nTaskId = MFX::CreateUniqId() + MFX_TRACE_ID_DECODE;
#endif

            // register input and call the task
            mfxAddRes = session->m_pScheduler->AddTask(task, &syncPoint);
            if (MFX_ERR_NONE != mfxAddRes)
            {
                return mfxAddRes;
            }
        }

        // return pointer to synchronization point
        if (MFX_ERR_NONE == mfxRes)
        {
            *syncp = syncPoint;
        }
    }
    // handle error(s)
    catch(MFX_CORE_CATCH_TYPE)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            return MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pDECODE.get())
        {
            return MFX_ERR_NOT_INITIALIZED;
        }
        else if (0 == syncp)
        {
            return MFX_ERR_NULL_PTR;
        }
    }

    if (mfxRes == MFX_ERR_NONE)
    {
        if (surface_out && *surface_out)
        {
            MFX_LTRACE_BUFFER(MFX_TRACE_LEVEL_API, *surface_out);
        }
        if (syncp)
        {
            MFX_LTRACE_P(MFX_TRACE_LEVEL_API, *syncp);
        }
#if 0 // disabled to not overwrite DXVA2 output
        MFX_TRACE_1("^Output^MemId", "%p", (*surface_out)->Data.MemId);
#endif
    }
    MFX_LTRACE_I(MFX_TRACE_LEVEL_API, mfxRes);

    return mfxRes;

} // mfxStatus MFXVideoDECODE_DecodeFrameAsync(mfxSession session, mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_dec, mfxFrameSurface1 **surface_disp, mfxSyncPoint *syncp)

//
// THE OTHER DECODE FUNCTIONS HAVE IMPLICIT IMPLEMENTATION
//

FUNCTION_RESET_IMPL(DECODE, Reset, (mfxSession session, mfxVideoParam *par), (par))

FUNCTION_IMPL(DECODE, GetVideoParam, (mfxSession session, mfxVideoParam *par), (par))
FUNCTION_IMPL(DECODE, GetDecodeStat, (mfxSession session, mfxDecodeStat *stat), (stat))
FUNCTION_IMPL(DECODE, SetSkipMode, (mfxSession session, mfxSkipMode mode), (mode))
FUNCTION_IMPL(DECODE, GetPayload, (mfxSession session, mfxU64 *ts, mfxPayload *payload), (session, ts, payload))
