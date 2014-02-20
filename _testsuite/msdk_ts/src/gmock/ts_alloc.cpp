#include "ts_alloc.h"

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

tsSurfacePool::tsSurfacePool(frame_allocator* allocator)
    : m_allocator(allocator)
    , m_external(true)
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
        isSW ? frame_allocator::SOFTWARE : frame_allocator::HARDWARE, 
        frame_allocator::ALLOC_MIN, 
        frame_allocator::ENABLE_ALL,
        frame_allocator::ALLOC_EMPTY);
    m_external = false;
}

mfxStatus tsSurfacePool::AllocSurfaces(mfxFrameAllocRequest request, bool direct_pointers)
{
    bool isSW = !(request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET));
    if(!m_allocator)
    {
        UseDefaultAllocator(isSW);
    }
    mfxFrameAllocResponse response = {};
    mfxFrameSurface1 s = {};
    mfxFrameAllocRequest* pRequest = &request;

    s.Info = request.Info;
    s.Data.TimeStamp = MFX_TIMESTAMP_UNKNOWN;

    if(!s.Info.PicStruct)
    {
        s.Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    TRACE_FUNC3(m_allocator->AllocFrame, m_allocator, pRequest, &response);
    g_tsStatus.check( m_allocator->AllocFrame(m_allocator, pRequest, &response) );
    TS_CHECK_MFX;

    for(mfxU16 i = 0; i < response.NumFrameActual; i++)
    {
        if(response.mids)
        {
            s.Data.Y = 0;
            s.Data.MemId = response.mids[i];
            if(direct_pointers && isSW)
            {
                    LockSurface(s);
                    TS_CHECK_MFX;
            }
        }
        m_pool.push_back(s);
    }
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

    while(it != m_pool.end())
    {
        if(!it->Data.Locked)
            return &(*it);
        it++;
    }
    g_tsLog << "ALL SURFACES ARE LOCKED!" << std::endl;
    g_tsStatus.check( MFX_ERR_NULL_PTR );

    return 0;
}
