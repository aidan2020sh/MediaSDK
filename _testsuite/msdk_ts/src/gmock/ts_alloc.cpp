/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_alloc.h"
#include "time_defs.h"
#include <algorithm>

tsBufferAllocator::tsBufferAllocator()
{
}

tsBufferAllocator::~tsBufferAllocator()
{
    std::vector<buffer>& b = GetBuffers();
    std::vector<buffer>::iterator it = b.begin();
    while(it != b.end())
    {
        (*Free)(this, it->mid);
        it++;
    }
}

tsSurfacePool::tsSurfacePool(frame_allocator* allocator, bool d3d11)
    : m_allocator(allocator)
    , m_external(true)
    , m_isd3d11(d3d11)
{
}

tsSurfacePool::~tsSurfacePool()
{
    Close();
}

void tsSurfacePool::Close()
{
    if(m_allocator && !m_external)
    {
        delete m_allocator;
    }
}

void tsSurfacePool::SetAllocator(frame_allocator* allocator, bool external)
{
    Close();
    m_allocator = allocator;
    m_external = external;
}

void tsSurfacePool::UseDefaultAllocator(bool isSW)
{
    Close();
    m_allocator = new frame_allocator(
        isSW ? frame_allocator::SOFTWARE : (m_isd3d11 ? frame_allocator::HARDWARE_DX11 : frame_allocator::HARDWARE ),
        frame_allocator::ALLOC_MAX,
        frame_allocator::ENABLE_ALL,
        frame_allocator::ALLOC_EMPTY);
    m_external = false;
}


struct SurfPtr
{
    mfxFrameSurface1* m_surf;
    SurfPtr(mfxFrameSurface1* s) : m_surf(s) {}
    mfxFrameSurface1* operator() () { return m_surf++; }
};

void tsSurfacePool::AllocOpaque(mfxFrameAllocRequest request, mfxExtOpaqueSurfaceAlloc& osa)
{
    mfxFrameSurface1 s = {};

    s.Info = request.Info;
    s.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    if(!s.Info.PicStruct)
    {
        s.Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    m_pool.resize(request.NumFrameSuggested, s);
    m_opaque_pool.resize(request.NumFrameSuggested);
    std::generate(m_opaque_pool.begin(), m_opaque_pool.end(), SurfPtr(m_pool.data()));

    if(request.Type & (MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_FROM_VPPIN))
    {
        osa.In.Type       = request.Type;
        osa.In.NumSurface = request.NumFrameSuggested;
        osa.In.Surfaces   = m_opaque_pool.data();
    }
    else 
    {
        osa.Out.Type       = request.Type;
        osa.Out.NumSurface = request.NumFrameSuggested;
        osa.Out.Surfaces   = m_opaque_pool.data();
    }
}

mfxStatus tsSurfacePool::AllocSurfaces(mfxFrameAllocRequest request, bool direct_pointers)
{
    bool isSW = !(request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET));
    mfxFrameSurface1 s = {};
    mfxFrameAllocRequest* pRequest = &request;

    s.Info = request.Info;
    s.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    if(!s.Info.PicStruct)
    {
        s.Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if(!m_allocator)
    {
        UseDefaultAllocator(isSW);
    }

    TRACE_FUNC3(m_allocator->AllocFrame, m_allocator, pRequest, &m_response);
    g_tsStatus.check( m_allocator->AllocFrame(m_allocator, pRequest, &m_response) );
    TS_CHECK_MFX;

    for(mfxU16 i = 0; i < m_response.NumFrameActual; i++)
    {
        if(m_response.mids)
        {
            s.Data.Y = 0;
            s.Data.MemId = m_response.mids[i];
            if(direct_pointers && isSW)
            {
                LockSurface(s);
                s.Data.MemId = 0;
            }
        }
        m_pool.push_back(s);
    }

    return g_tsStatus.get();
}

mfxStatus tsSurfacePool::FreeSurfaces()
{
    if (m_pool.size() == 0)
        return MFX_ERR_NONE;

    TRACE_FUNC2(m_allocator->Free, m_allocator, &m_response);
    g_tsStatus.check( m_allocator->Free(m_allocator, &m_response) );
    TS_CHECK_MFX;

    m_pool.clear();

    return g_tsStatus.get();
}

mfxStatus tsSurfacePool::LockSurface(mfxFrameSurface1& s)
{
    mfxStatus mfxRes = MFX_ERR_NOT_INITIALIZED;
    if(s.Data.Y)
    {
        return MFX_ERR_NONE;
    }
    if(m_allocator)
    {
        TRACE_FUNC3(m_allocator->LockFrame, m_allocator, s.Data.MemId, &s.Data);
        mfxRes = m_allocator->LockFrame(m_allocator, s.Data.MemId, &s.Data);
    }
    g_tsStatus.check( mfxRes );
    return g_tsStatus.get();
}

mfxStatus tsSurfacePool::UnlockSurface(mfxFrameSurface1& s)
{
    mfxStatus mfxRes = MFX_ERR_NOT_INITIALIZED;
    if(s.Data.MemId)
    {
        return MFX_ERR_NONE;
    }
    if(m_allocator)
    {
        TRACE_FUNC3(m_allocator->UnLockFrame, m_allocator, s.Data.MemId, &s.Data);
        mfxRes = m_allocator->UnLockFrame(m_allocator, s.Data.MemId, &s.Data);
    }
    g_tsStatus.check( mfxRes );
    return g_tsStatus.get();
}

mfxFrameSurface1* tsSurfacePool::GetSurface()
{
    std::vector<mfxFrameSurface1>::iterator it = m_pool.begin();
    mfxU32 timeout = 100;
    mfxU32 step = 5;

    while (timeout)
    {
        while (it != m_pool.end())
        {
            if (!it->Data.Locked)
                return &(*it);
            it++;
        }
        MSDK_SLEEP(step);
        timeout -= step;
        it = m_pool.begin();
    }
    g_tsLog << "ALL SURFACES ARE LOCKED!\n";
    g_tsStatus.check( MFX_ERR_NULL_PTR );

    return 0;
}

mfxFrameSurface1* tsSurfacePool::GetSurface(mfxU32 ind)
{
    if (ind >= m_pool.size())
        return 0;

    return &m_pool[ind];
}

