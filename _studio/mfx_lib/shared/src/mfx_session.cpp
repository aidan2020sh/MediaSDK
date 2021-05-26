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

#include <assert.h>
#include "mfx_common.h"
#include <mfx_session.h>

#include <vm_time.h>
#include <vm_sys_info.h>

#include <libmfx_core_factory.h>
#include <libmfx_core.h>

#if defined (MFX_VA_WIN)
#include <libmfx_core_d3d9.h>
#include <atlbase.h>
#include "mfx_dxva2_device.h"
#elif defined(MFX_VA_LINUX)
#include <libmfx_core_vaapi.h>
#endif

// static section of the file
namespace
{

#if !defined(MFX_ONEVPL)
mfxStatus mfxCOREGetCoreParam(mfxHDL pthis, mfxCoreParam *par)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pScheduler, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK(par, MFX_ERR_NULL_PTR);

    try
    {
        MFX_SCHEDULER_PARAM param;

        // reset the parameters
        memset(par, 0, sizeof(mfxCoreParam));

        // get the parameters of the current scheduler
        mfxRes = session->m_pScheduler->GetParam(&param);
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }

        // fill the structure
        mfxRes = MFXQueryIMPL(session, &(par->Impl));
        if (MFX_ERR_NONE != mfxRes)
        {
            return mfxRes;
        }
        par->Version = session->m_version;
        par->NumWorkingThread = param.numberOfThreads;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;

} // mfxStatus mfxCOREGetCoreParam(mfxHDL pthis, mfxCoreParam *par)

#if defined(MFX_ENABLE_OPAQUE_MEMORY)
mfxStatus mfxCOREMapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    CommonCORE *pCore = (CommonCORE *)session->m_pCORE->QueryCoreInterface(MFXIVideoCORE_GUID);
    if (!pCore)
        return MFX_ERR_INVALID_HANDLE;

    try
    {
        if (!op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        if (!*op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        mfxFrameAllocRequest  request;
        mfxFrameAllocResponse response;

        request.Type =        (mfxU16)type;
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)num;
        request.Info = op_surf[0]->Info;

        mfxRes = pCore->AllocFrames(&request, &response, op_surf, num);
        MFX_CHECK_STS(mfxRes);

        pCore->AddPluginAllocResponse(response);

        return mfxRes;

    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session)
        {
            mfxRes = MFX_ERR_INVALID_HANDLE;
        }
        else if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;

} // mfxStatus mfxCOREMapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)

mfxStatus mfxCOREUnmapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  , mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    CommonCORE *pCore = (CommonCORE *)session->m_pCORE->QueryCoreInterface(MFXIVideoCORE_GUID);
    if (!pCore)
        return MFX_ERR_INVALID_HANDLE;

    try
    {
        if (!op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        if (!*op_surf)
            return MFX_ERR_MEMORY_ALLOC;

        mfxFrameAllocResponse response;
        mfxFrameSurface1 *pSurf = NULL;

        std::vector<mfxMemId> mids(num);
        response.mids = &mids[0];
        response.NumFrameActual = (mfxU16) num;
        for (mfxU32 i=0; i < num; i++)
        {
            pSurf = pCore->GetNativeSurface(op_surf[i]);
            if (!pSurf)
                return MFX_ERR_INVALID_HANDLE;

            response.mids[i] = pSurf->Data.MemId;
        }

        if (!pCore->GetPluginAllocResponse(response))
            return MFX_ERR_INVALID_HANDLE;

        mfxRes = session->m_pCORE->FreeFrames(&response);
        return mfxRes;

    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;

} // mfxStatus mfxCOREUnmapOpaqueSurface(mfxHDL pthis, mfxU32  num, mfxU32  type, mfxFrameSurface1 **op_surf)

mfxStatus mfxCOREGetOpaqueSurface(mfxHDL pthis, mfxFrameSurface1 *surf, mfxFrameSurface1 **op_surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    try
    {
        *op_surf = session->m_pCORE->GetOpaqSurface(surf->Data.MemId);
        if (!*op_surf)
            return MFX_ERR_INVALID_HANDLE;

        return mfxRes;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;
}// mfxStatus mfxCOREGetOpaqueSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)
#endif //MFX_ENABLE_OPAQUE_MEMORY

mfxStatus mfxCOREGetRealSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;

    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);

    try
    {
        *surf = session->m_pCORE->GetNativeSurface(op_surf);
        if (!*surf)
            return MFX_ERR_INVALID_HANDLE;

        return mfxRes;
    }
    // handle error(s)
    catch(...)
    {
        // set the default error value
        mfxRes = MFX_ERR_UNKNOWN;
        if (0 == session->m_pScheduler)
        {
            mfxRes = MFX_ERR_NOT_INITIALIZED;
        }
    }

    return mfxRes;
} // mfxStatus mfxCOREGetRealSurface(mfxHDL pthis, mfxFrameSurface1 *op_surf, mfxFrameSurface1 **surf)

mfxStatus mfxCORECreateAccelerationDevice(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)
{
    mfxSession session = (mfxSession) pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);
    try
    {
        mfxRes = session->m_pCORE.get()->GetHandle(type, handle);

        if (mfxRes == MFX_ERR_NOT_FOUND)
        {
#if defined(MFX_VA_WIN)
            if (type == MFX_HANDLE_D3D9_DEVICE_MANAGER)
            {
                D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(session->m_pCORE.get(), MFXICORED3D_GUID);
                if(pID3D == 0)
                    mfxRes = MFX_ERR_UNSUPPORTED;
                else
                {
                    IDirectXVideoDecoderService *service = 0;
                    mfxRes = pID3D->GetD3DService(1920, 1088, &service);

                    *handle = (mfxHDL)pID3D->GetD3D9DeviceManager();
                }
            }
            else if (type == MFX_HANDLE_D3D11_DEVICE)
            {
                D3D11Interface* pID3D = QueryCoreInterface<D3D11Interface>(session->m_pCORE.get());
                if(pID3D == 0)
                    mfxRes = MFX_ERR_UNSUPPORTED;
                else
                {
                    *handle = (mfxHDL)pID3D->GetD3D11Device();
                    if (*handle)
                        mfxRes = MFX_ERR_NONE;
                }
            }
            else
#endif
            {
                mfxRes = MFX_ERR_UNSUPPORTED;
            }
        }
    }
    /* handle error(s) */
    catch(...)
    {
        mfxRes = MFX_ERR_NULL_PTR;
    }
    return mfxRes;
}// mfxStatus mfxCORECreateAccelerationDevice(mfxHDL pthis, mfxHandleType type, mfxHDL *handle)

mfxStatus mfxCOREGetFrameHDL(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)
{
    mfxSession session = (mfxSession)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(handle);
    VideoCORE *pCore = session->m_pCORE.get();

    try
    {
        if (   pCore->IsExternalFrameAllocator()
            && !(fd->MemType & MFX_MEMTYPE_OPAQUE_FRAME))
        {
            mfxRes = pCore->GetExternalFrameHDL(fd->MemId, handle);
        }
        else
        {
            mfxRes = pCore->GetFrameHDL(fd->MemId, handle);
        }
    }
    catch (...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxStatus mfxCOREGetFrameHDL(mfxHDL pthis, mfxFrameData *fd, mfxHDL *handle)

mfxStatus mfxCOREQueryPlatform(mfxHDL pthis, mfxPlatform *platform)
{
    mfxSession session = (mfxSession)pthis;
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE);
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(platform);
    VideoCORE *pCore = session->m_pCORE.get();

    try
    {
        IVideoCore_API_1_19 * pInt = QueryCoreInterface<IVideoCore_API_1_19>(pCore, MFXICORE_API_1_19_GUID);
        if (pInt)
        {
            mfxRes = pInt->QueryPlatform(platform);
        }
        else
        {
            mfxRes = MFX_ERR_UNSUPPORTED;
            memset(platform, 0, sizeof(mfxPlatform));
        }
    }
    catch (...)
    {
        mfxRes = MFX_ERR_UNKNOWN;
    }

    return mfxRes;
} // mfxCOREQueryPlatform(mfxHDL pthis, mfxPlatform *platform)

#define CORE_FUNC_IMPL(func_name, formal_param_list, actual_param_list) \
mfxStatus mfxCORE##func_name formal_param_list \
{ \
    mfxSession session = (mfxSession) pthis; \
    mfxStatus mfxRes; \
    MFX_CHECK(session, MFX_ERR_INVALID_HANDLE); \
    MFX_CHECK(session->m_pCORE.get(), MFX_ERR_NOT_INITIALIZED); \
    try \
    { \
        /* call the method */ \
        mfxRes = session->m_pCORE->func_name actual_param_list; \
    } \
    /* handle error(s) */ \
    catch(...) \
    { \
        mfxRes = MFX_ERR_NULL_PTR; \
    } \
    return mfxRes; \
} /* mfxStatus mfxCORE##func_name formal_param_list */

CORE_FUNC_IMPL(GetHandle, (mfxHDL pthis, mfxHandleType type, mfxHDL *handle), (type, handle))
CORE_FUNC_IMPL(IncreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL(DecreaseReference, (mfxHDL pthis, mfxFrameData *fd), (fd))
CORE_FUNC_IMPL(CopyFrame, (mfxHDL pthis, mfxFrameSurface1 *dst, mfxFrameSurface1 *src), (dst, src))
CORE_FUNC_IMPL(CopyBuffer, (mfxHDL pthis, mfxU8 *dst, mfxU32 dst_size, mfxFrameSurface1 *src), (dst, dst_size, src))

#undef CORE_FUNC_IMPL

// exposed default allocator
mfxStatus mfxDefAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc?pExtAlloc->Alloc(pExtAlloc->pthis,request,response):pCore->AllocFrames(request,response);

} // mfxStatus mfxDefAllocFrames(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
mfxStatus mfxDefLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;

    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->LockExternalFrame(mid,ptr);
    }

    return pCore->LockFrame(mid,ptr);


} // mfxStatus mfxDefLockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
mfxStatus mfxDefGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->GetExternalFrameHDL(mid, handle);
    }
    return pCore->GetFrameHDL(mid, handle);

} // mfxStatus mfxDefGetHDL(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
mfxStatus mfxDefUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;

    if (pCore->IsExternalFrameAllocator())
    {
        return pCore->UnlockExternalFrame(mid, ptr);
    }

    return pCore->UnlockFrame(mid, ptr);

} // mfxStatus mfxDefUnlockFrame(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0)
mfxStatus mfxDefFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    MFX_CHECK_NULL_PTR1(pthis);
    VideoCORE *pCore = (VideoCORE*)pthis;
    mfxFrameAllocator* pExtAlloc = (mfxFrameAllocator*)pCore->QueryCoreInterface(MFXIEXTERNALLOC_GUID);
    return pExtAlloc?pExtAlloc->Free(pExtAlloc->pthis,response):pCore->FreeFrames(response);

} // mfxStatus mfxDefFreeFrames(mfxHDL pthis, mfxFrameAllocResponse *response)

void InitCoreInterface(mfxCoreInterface *pCoreInterface,
                       const mfxSession session)
{
    // reset the structure
    memset(pCoreInterface, 0, sizeof(mfxCoreInterface));


     // fill external allocator
    pCoreInterface->FrameAllocator.pthis = session->m_pCORE.get();
    pCoreInterface->FrameAllocator.Alloc = &mfxDefAllocFrames;
    pCoreInterface->FrameAllocator.Lock = &mfxDefLockFrame;
    pCoreInterface->FrameAllocator.GetHDL = &mfxDefGetHDL;
    pCoreInterface->FrameAllocator.Unlock = &mfxDefUnlockFrame;
    pCoreInterface->FrameAllocator.Free = &mfxDefFreeFrames;

    // fill the methods
    pCoreInterface->pthis = (mfxHDL) session;
    pCoreInterface->GetCoreParam = &mfxCOREGetCoreParam;
    pCoreInterface->GetHandle = &mfxCOREGetHandle;
    pCoreInterface->GetFrameHandle = &mfxCOREGetFrameHDL;
    pCoreInterface->IncreaseReference = &mfxCOREIncreaseReference;
    pCoreInterface->DecreaseReference = &mfxCOREDecreaseReference;
    pCoreInterface->CopyFrame = &mfxCORECopyFrame;
    pCoreInterface->CopyBuffer = &mfxCORECopyBuffer;
#if defined(MFX_ENABLE_OPAQUE_MEMORY)
    pCoreInterface->MapOpaqueSurface = &mfxCOREMapOpaqueSurface;
    pCoreInterface->UnmapOpaqueSurface = &mfxCOREUnmapOpaqueSurface;
#endif //MFX_ENABLE_OPAQUE_MEMORY
    pCoreInterface->GetRealSurface = &mfxCOREGetRealSurface;
    pCoreInterface->GetOpaqueSurface = &mfxCOREGetOpaqueSurface;
    pCoreInterface->CreateAccelerationDevice = &mfxCORECreateAccelerationDevice;
    pCoreInterface->QueryPlatform = &mfxCOREQueryPlatform;

} // void InitCoreInterface(mfxCoreInterface *pCoreInterface,
#endif //!MFX_ONEVPL

} // namespace


#define TRY_GET_SESSION(verMax,verMin) MFXIPtr<MFXISession_##verMax##_##verMin> TryGetSession_##verMax##_##verMin(mfxSession session) \
{ \
    if (session == NULL)\
    { \
        return MFXIPtr<MFXISession_##verMax##_##verMin>(); \
    } \
    return MFXIPtr<MFXISession_##verMax##_##verMin>(static_cast<_mfxVersionedSessionImpl *>(session)->QueryInterface(MFXISession_##verMax##_##verMin##_GUID)); \
}

TRY_GET_SESSION(1,10)
TRY_GET_SESSION(2,1)

//////////////////////////////////////////////////////////////////////////
//  _mfxSession members
//////////////////////////////////////////////////////////////////////////

_mfxSession::_mfxSession(const mfxU32 adapterNum)
#if !defined(MFX_ONEVPL)
    : m_coreInt() ,
#else
    :
#endif
      m_currentPlatform()
    , m_adapterNum(adapterNum)
    , m_implInterface()
    , m_pScheduler()
    , m_priority()
    , m_version()
#if defined(MFX_ONEVPL)
    , m_versionToReport()
#endif
    , m_pOperatorCore()
    , m_bIsHWENCSupport()
    , m_bIsHWDECSupport()
{
    m_currentPlatform = MFX_PLATFORM_HARDWARE;

#if defined(MFX_ONEVPL)
    m_versionToReport.Major = MFX_VERSION_MAJOR;
    m_versionToReport.Minor = MFX_VERSION_MINOR;
#endif

    Clear();
} // _mfxSession::_mfxSession(const mfxU32 adapterNum) :

_mfxSession::~_mfxSession(void)
{
    Cleanup();

} // _mfxSession::~_mfxSession(void)

void _mfxSession::Clear(void)
{
    m_pScheduler = NULL;
    m_pSchedulerAllocated = NULL;

    m_priority = MFX_PRIORITY_NORMAL;
    m_bIsHWENCSupport = false;

} // void _mfxSession::Clear(void)

void _mfxSession::Cleanup(void)
{
    // wait until all task are processed
    if (m_pScheduler)
    {
        if (m_pDECODE.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pDECODE.get());
        if (m_pVPP.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pVPP.get());
        if (m_pENCODE.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pENCODE.get());

#if !defined(MFX_ONEVPL)
        if (m_pENC.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pENC.get());
        if (m_pPAK.get())
            m_pScheduler->WaitForAllTasksCompletion(m_pPAK.get());
#endif //!MFX_ONEVPL
    }

    // release the components the excplicit way.
    // do not relay on default deallocation order,
    // somebody could change it.
#if !defined(MFX_ONEVPL)
    m_pPAK.reset();
    m_pENC.reset();
#endif //!MFX_ONEVPL
    m_pVPP.reset();
    m_pDECODE.reset();
    m_pENCODE.reset();

#if defined(MFX_ONEVPL)
        m_pDVP.reset();
#endif

    // release m_pScheduler and m_pSchedulerAllocated
    ReleaseScheduler();

    // release core
    m_pCORE.reset();

    //delete m_coreInt.ExternalSurfaceAllocator;
    Clear();

} // void _mfxSession::Release(void)

mfxStatus _mfxSession::Init(mfxIMPL implInterface, mfxVersion *ver)
{
    mfxStatus mfxRes;
    MFX_SCHEDULER_PARAM schedParam;
    mfxU32 maxNumThreads;
#if defined(MFX_ENABLE_SINGLE_THREAD)
    bool isExternalThreading = (implInterface & MFX_IMPL_EXTERNAL_THREADING)?true:false;
    implInterface &= ~MFX_IMPL_EXTERNAL_THREADING;
#endif
    // release the object before initialization
    Cleanup();

    if (ver)
    {
        m_version = *ver;
    }
    else
    {
        mfxStatus sts = MFXQueryVersion(this, &m_version);
        if (sts != MFX_ERR_NONE)
            return sts;
    }

    // save working HW interface
    switch (implInterface&-MFX_IMPL_VIA_ANY)
    {
    case MFX_IMPL_UNSUPPORTED:
        assert(!"MFXInit(Ex) was supposed to correct zero-impl to MFX_IMPL_VIA_ANY");
        return MFX_ERR_UNDEFINED_BEHAVIOR;
#if defined(MFX_VA_LINUX)
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;
#else
#if defined(MFX_VA_WIN)
        // D3D9 is only one supported interface
    case MFX_IMPL_VIA_ANY:
#endif
    case MFX_IMPL_VIA_D3D9:
        m_implInterface = MFX_IMPL_VIA_D3D9;
        break;
    case MFX_IMPL_VIA_D3D11:
        m_implInterface = MFX_IMPL_VIA_D3D11;
        break;
#endif

    // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // get the number of available threads
    maxNumThreads = vm_sys_info_get_cpu_num();
    if (maxNumThreads == 1) {
        maxNumThreads = 2;
    }

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, maxNumThreads, this));
    }
#if defined(MFX_VA_WIN)
    else
    {
        if (MFX_IMPL_VIA_D3D11 == m_implInterface)
        {
#if defined (MFX_D3D11_ENABLED)
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D11, m_adapterNum, maxNumThreads, this));
#else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
#endif
        }
        else
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D9, m_adapterNum, maxNumThreads, this));

    }
#elif defined(MFX_VA_LINUX)
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, maxNumThreads, this));
    }

#endif

#if !defined(MFX_ONEVPL)
    // initialize the core interface
    InitCoreInterface(&m_coreInt, this);
#endif

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }
    memset(&schedParam, 0, sizeof(schedParam));
    schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_ENABLE_SINGLE_THREAD)
    if (isExternalThreading)
        schedParam.flags = MFX_SINGLE_THREAD;
#endif
    schedParam.numberOfThreads = maxNumThreads;
    schedParam.pCore = m_pCORE.get();
    mfxRes = m_pScheduler->Initialize(&schedParam);
    if (MFX_ERR_NONE != mfxRes)
    {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::Init(mfxIMPL implInterface)

mfxStatus _mfxSession::RestoreScheduler(void)
{
    if(m_pSchedulerAllocated)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    // leave the current scheduler
    if (m_pScheduler)
    {
        m_pScheduler->Release();
        m_pScheduler = NULL;
    }

    // query the scheduler interface
    m_pScheduler = QueryInterface<MFXIScheduler> (m_pSchedulerAllocated,
                                                  MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

mfxStatus _mfxSession::ReleaseScheduler(void)
{
    if(m_pScheduler)
        m_pScheduler->Release();

    if(m_pSchedulerAllocated)
        m_pSchedulerAllocated->Release();

    m_pScheduler = nullptr;
    m_pSchedulerAllocated = nullptr;

    return MFX_ERR_NONE;

} // mfxStatus _mfxSession::RestoreScheduler(void)

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl own members
//////////////////////////////////////////////////////////////////////////

_mfxVersionedSessionImpl::_mfxVersionedSessionImpl(mfxU32 adapterNum)
    : _mfxSession(adapterNum)
    , m_refCounter(1)
    , m_externalThreads(0)
{
}

_mfxVersionedSessionImpl::~_mfxVersionedSessionImpl(void)
{
}

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl::MFXISession_1_10 members
//////////////////////////////////////////////////////////////////////////


void _mfxVersionedSessionImpl::SetAdapterNum(const mfxU32 adapterNum)
{
    m_adapterNum = adapterNum;
}

//////////////////////////////////////////////////////////////////////////
// _mfxVersionedSessionImpl::MFXIUnknown members
//////////////////////////////////////////////////////////////////////////

void *_mfxVersionedSessionImpl::QueryInterface(const MFX_GUID &guid)
{
    // Specific interface is required
    if (MFXISession_1_10_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXISession_1_10 *) this;
    }

    // Specific interface is required
    if (MFXISession_2_1_GUID == guid)
    {
        // increment reference counter
        vm_interlocked_inc32(&m_refCounter);

        return (MFXISession_2_1 *)this;
    }

    // it is unsupported interface
    return NULL;
} // void *_mfxVersionedSessionImpl::QueryInterface(const MFX_GUID &guid)

void _mfxVersionedSessionImpl::AddRef(void)
{
    // increment reference counter
    vm_interlocked_inc32(&m_refCounter);

} // void mfxSchedulerCore::AddRef(void)

void _mfxVersionedSessionImpl::Release(void)
{
    // decrement reference counter
    vm_interlocked_dec32(&m_refCounter);

    if (0 == m_refCounter)
    {
        delete this;
    }

} // void _mfxVersionedSessionImpl::Release(void)

mfxU32 _mfxVersionedSessionImpl::GetNumRef(void) const
{
    return m_refCounter;

} // mfxU32 _mfxVersionedSessionImpl::GetNumRef(void) const

#ifdef MFX_VA_WIN
static inline mfxStatus HasNativeDX9Support(mfxU32 adapter_n, bool& hasSupport)
{
    MFX::DXVA2Device dxvaDevice;
    hasSupport = true;

    MFX_CHECK(dxvaDevice.InitDXGI1(adapter_n), MFX_ERR_DEVICE_FAILED);
    mfxU32 DeviceID = dxvaDevice.GetDeviceID();

    auto const* listLegalDevEnd = listLegalDevIDs + (sizeof(listLegalDevIDs) / sizeof(mfx_device_item));
    auto devItem = std::find_if(listLegalDevIDs, listLegalDevEnd, [DeviceID](mfx_device_item devItem) {
        return static_cast<mfxU32>(devItem.device_id) == DeviceID;
    });

    MFX_CHECK(devItem != listLegalDevEnd, MFX_ERR_DEVICE_FAILED);

    if (devItem->platform >= MFX_HW_ADL_S)
        hasSupport = false;

    return MFX_ERR_NONE;
}
#endif

mfxStatus _mfxVersionedSessionImpl::InitEx(mfxInitParam& par)
{
    mfxStatus mfxRes;
    mfxU32 maxNumThreads;
#if defined(MFX_ENABLE_SINGLE_THREAD)
    bool isSingleThreadMode = (par.Implementation & MFX_IMPL_EXTERNAL_THREADING) ? true : false;
    par.Implementation &= ~MFX_IMPL_EXTERNAL_THREADING;
#endif
    // release the object before initialization
    Cleanup();

    m_version = par.Version;

    // save working HW interface
    switch (par.Implementation&-MFX_IMPL_VIA_ANY)
    {
    case MFX_IMPL_UNSUPPORTED:
        assert(!"MFXInit(Ex) was supposed to correct zero-impl to MFX_IMPL_VIA_ANY");
        return MFX_ERR_UNDEFINED_BEHAVIOR;
#if defined(MFX_VA_LINUX)
        // VAAPI is only one supported interface
    case MFX_IMPL_VIA_ANY:
    case MFX_IMPL_VIA_VAAPI:
        m_implInterface = MFX_IMPL_VIA_VAAPI;
        break;
#else
#if defined(MFX_VA_WIN)
        // D3D9 is only one supported interface
    case MFX_IMPL_VIA_ANY:
#endif
    case MFX_IMPL_VIA_D3D9:
        m_implInterface = MFX_IMPL_VIA_D3D9;
        break;
    case MFX_IMPL_VIA_D3D11:
        m_implInterface = MFX_IMPL_VIA_D3D11;
        break;
#endif

    // unknown hardware interface
    default:
        if (MFX_PLATFORM_HARDWARE == m_currentPlatform)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // only mfxExtThreadsParam is allowed
    if (par.NumExtParam)
    {
        if ((par.NumExtParam > 1) || !par.ExtParam)
        {
            return MFX_ERR_UNSUPPORTED;
        }
        if ((par.ExtParam[0]->BufferId != MFX_EXTBUFF_THREADS_PARAM) ||
            (par.ExtParam[0]->BufferSz != sizeof(mfxExtThreadsParam)))
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    // get the number of available threads
    maxNumThreads = 0;
    if (par.ExternalThreads == 0)
    {
        maxNumThreads = vm_sys_info_get_cpu_num();
        if (maxNumThreads == 1)
        {
            maxNumThreads = 2;
        }
    }

    // allocate video core
    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform)
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_NO, 0, maxNumThreads, this));
    }
#if defined(MFX_VA_WIN)
    else
    {
        if (MFX_IMPL_VIA_D3D11 == m_implInterface)
        {
#if defined (MFX_D3D11_ENABLED)
            m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_D3D11, m_adapterNum, maxNumThreads, this));
#else
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
#endif
        }
        else
        {
            D3D9DllCallHelper d3d9hlp;
            if (d3d9hlp.isD3D9Available() == false)
                return  MFX_ERR_UNSUPPORTED;

            bool hasNativeSupport = true;
            mfxRes = HasNativeDX9Support(m_adapterNum, hasNativeSupport);
            MFX_CHECK_STS(mfxRes);

            m_pCORE.reset(FactoryCORE::CreateCORE(hasNativeSupport ? MFX_HW_D3D9 : MFX_HW_D3D9ON11, m_adapterNum, maxNumThreads, this));
        }

    }
#else
    else
    {
        m_pCORE.reset(FactoryCORE::CreateCORE(MFX_HW_VAAPI, m_adapterNum, maxNumThreads, this));
    }
#endif

#if !defined(MFX_ONEVPL)
    // initialize the core interface
    InitCoreInterface(&m_coreInt, this);
#endif

    // query the scheduler interface
    m_pScheduler = ::QueryInterface<MFXIScheduler>(m_pSchedulerAllocated, MFXIScheduler_GUID);
    if (NULL == m_pScheduler)
    {
        return MFX_ERR_UNKNOWN;
    }

    MFXIScheduler2* pScheduler2 = ::QueryInterface<MFXIScheduler2>(m_pSchedulerAllocated, MFXIScheduler2_GUID);

    if (par.NumExtParam && !pScheduler2) {
        return MFX_ERR_UNKNOWN;
    }

    if (pScheduler2) {
        MFX_SCHEDULER_PARAM2 schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_ENABLE_SINGLE_THREAD)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        if (par.NumExtParam) {
            schedParam.params = *((mfxExtThreadsParam*)par.ExtParam[0]);
        }
        mfxRes = pScheduler2->Initialize2(&schedParam);

        m_pScheduler->Release();
    }
    else {
        MFX_SCHEDULER_PARAM schedParam;
        memset(&schedParam, 0, sizeof(schedParam));
        schedParam.flags = MFX_SCHEDULER_DEFAULT;
#if defined(MFX_ENABLE_SINGLE_THREAD)
        if (isSingleThreadMode)
            schedParam.flags = MFX_SINGLE_THREAD;
#endif
        schedParam.numberOfThreads = maxNumThreads;
        schedParam.pCore = m_pCORE.get();
        mfxRes = m_pScheduler->Initialize(&schedParam);
    }

    if (MFX_ERR_NONE != mfxRes) {
        return mfxRes;
    }

    m_pOperatorCore = new OperatorCORE(m_pCORE.get());

    if (MFX_PLATFORM_SOFTWARE == m_currentPlatform && MFX_GPUCOPY_ON == par.GPUCopy)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    // Windows: By default CM Copy enabled on HW cores, so only need to handle explicit OFF value
    // Linux: By default CM Copy disabled on HW cores so only need to handle explicit ON value
    //        Also see the logic in SetHandle from VAAPI core
    const bool disableGpuCopy = (m_pCORE->GetVAType() == MFX_HW_VAAPI )
        ? (MFX_GPUCOPY_ON != par.GPUCopy)
        : (MFX_GPUCOPY_OFF == par.GPUCopy);

    if (disableGpuCopy)
    {
        CMEnabledCoreInterface* pCmCore = QueryCoreInterface<CMEnabledCoreInterface>(m_pCORE.get());
        if (pCmCore)
        {
            mfxRes = pCmCore->SetCmCopyStatus(false);
        }
        if (MFX_ERR_NONE != mfxRes) {
            return mfxRes;
        }
    }

    return InitEx_2_1(par);
} // mfxStatus _mfxVersionedSessionImpl::InitEx(mfxInitParam& par);

mfxStatus _mfxVersionedSessionImpl::InitEx_2_1(mfxInitParam& par)
{
    //--- Initialization of stuff related to 1.33 interface
    if (par.ExtParam)
    {
        mfxExtBuffer** found = std::find_if(par.ExtParam, par.ExtParam + par.NumExtParam,
            [](mfxExtBuffer* x) {
            return x->BufferId == MFX_EXTBUFF_THREADS_PARAM && x->BufferSz == sizeof(mfxExtThreadsParam); });
        MFX_CHECK(found!= par.ExtParam + par.NumExtParam, MFX_ERR_UNSUPPORTED);
    }

    return MFX_ERR_NONE;
} // mfxStatus _mfxVersionedSessionImpl::InitEx2_1(mfxInitParam& par);


//explicit specification of interface creation
template<> MFXISession_1_10*  CreateInterfaceInstance<MFXISession_1_10>(const MFX_GUID &guid)
{
    if (MFXISession_1_10_GUID == guid)
        return (MFXISession_1_10*) (new _mfxVersionedSessionImpl(0));

    return NULL;
}

template<> MFXISession_2_1*  CreateInterfaceInstance<MFXISession_2_1>(const MFX_GUID &guid)
{
    if (MFXISession_2_1_GUID == guid)
        return (MFXISession_2_1*)(new _mfxVersionedSessionImpl(0));

    return NULL;
}

namespace MFX
{
    unsigned int CreateUniqId()
    {
        static volatile mfxU32 g_tasksId = 0;
        return (unsigned int)vm_interlocked_inc32(&g_tasksId);
    }
}
