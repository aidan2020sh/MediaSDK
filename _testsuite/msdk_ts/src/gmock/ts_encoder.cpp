#include "ts_encoder.h"

tsVideoEncoder::tsVideoEncoder(mfxU32 CodecId, bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_par()
    , m_bitstream()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pBitstream(&m_bitstream)
    , m_pRequest(&m_request)
    , m_pSyncPoint(&m_syncpoint)
    , m_pSurf(0)
    , m_pCtrl(&m_ctrl)
{
    m_par.mfx.CodecId = CodecId;
    if(m_default)
    {
        //TODO: add codec specific
        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;

        if (CodecId == MFX_CODEC_VP8)
            m_par.mfx.QPB = 0;

        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
    }
}

tsVideoEncoder::~tsVideoEncoder() 
{
}
    
mfxStatus tsVideoEncoder::Init() 
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();TS_CHECK_MFX;
        }
        if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
        {
            if(!GetAllocator())
            {
                UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();TS_CHECK_MFX;
        }
    }
    return Init(m_session, m_pPar);
}

mfxStatus tsVideoEncoder::Init(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENCODE_Init, session, par);
    g_tsStatus.check( MFXVideoENCODE_Init(session, par) );

    m_initialized = (g_tsStatus.get() >= 0);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::Close()
{
    return Close(m_session);
}

mfxStatus tsVideoEncoder::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoENCODE_Close, session);
    g_tsStatus.check( MFXVideoENCODE_Close(session) );

    m_initialized = false;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::Query()
{
    if(m_default && !m_session)
    {
        MFXInit();TS_CHECK_MFX;
    }
    return Query(m_session, m_pPar, m_pParOut);
}

mfxStatus tsVideoEncoder::Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    TRACE_FUNC3(MFXVideoENCODE_Query, session, in, out);
    g_tsStatus.check( MFXVideoENCODE_Query(session, in, out) );
    TS_TRACE(out);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::QueryIOSurf()
{
    if(m_default && !m_session)
    {
        MFXInit();TS_CHECK_MFX;
    }
    return QueryIOSurf(m_session, m_pPar, m_pRequest);
}

mfxStatus tsVideoEncoder::QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
        
    TRACE_FUNC3(MFXVideoENCODE_QueryIOSurf, session, par, request);
    g_tsStatus.check( MFXVideoENCODE_QueryIOSurf(session, par, request) );
    TS_TRACE(request);

    return g_tsStatus.get();
}

    
mfxStatus tsVideoEncoder::Reset()
{
    return Reset(m_session, m_pPar);
}

mfxStatus tsVideoEncoder::Reset(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENCODE_Reset, session, par);
    g_tsStatus.check( MFXVideoENCODE_Reset(session, par) );

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::GetVideoParam() 
{
    if(m_default && !m_initialized)
    {
        Init();TS_CHECK_MFX;
    }
    return GetVideoParam(m_session, m_pPar); 
}

mfxStatus tsVideoEncoder::GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, session, par);
    g_tsStatus.check( MFXVideoENCODE_GetVideoParam(session, par) );
    TS_TRACE(par);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::EncodeFrameAsync()
{
    if(m_default)
    {
        if(!PoolSize())
        {
            if(m_pFrameAllocator && !GetAllocator())
            {
                SetAllocator(m_pFrameAllocator, true);
            }
            AllocSurfaces();TS_CHECK_MFX;
            if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();TS_CHECK_MFX;
            }
        }
        if(!m_initialized)
        {
            Init();TS_CHECK_MFX;
        }
        if(!m_bitstream.MaxLength)
        {
            AllocBitstream();TS_CHECK_MFX;
        }
        m_pSurf = GetSurface();TS_CHECK_MFX;
    }
    mfxStatus mfxRes = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);

    return mfxRes;
}

mfxStatus tsVideoEncoder::EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    TRACE_FUNC5(MFXVideoENCODE_EncodeFrameAsync, session, ctrl, surface, bs, syncp);
    mfxStatus mfxRes = MFXVideoENCODE_EncodeFrameAsync(session, ctrl, surface, bs, syncp);
    TS_TRACE(mfxRes);
    TS_TRACE(ctrl);
    TS_TRACE(surface);
    TS_TRACE(bs);
    TS_TRACE(syncp);
        
    return g_tsStatus.m_status = mfxRes;
}

mfxStatus tsVideoEncoder::AllocBitstream(mfxU32 size)
{
    if(!size)
    {
        if(m_par.mfx.CodecId == MFX_CODEC_JPEG)
        {
            size = TS_MAX((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height), 1000000);
        }
        else
        {
            if(!m_par.mfx.BufferSizeInKB)
            {
                GetVideoParam();TS_CHECK_MFX;
            }
            size = m_par.mfx.BufferSizeInKB * TS_MAX(m_par.mfx.BRCParamMultiplier, 1) * 1000 * TS_MAX(m_par.AsyncDepth, 1);
        }
    }

    g_tsLog << "ALLOC BITSTREAM OF SIZE " << size << std::endl;

    mfxMemId mid = 0;
    TRACE_FUNC4((*m_buffer_allocator.Alloc), &m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid);
    g_tsStatus.check((*m_buffer_allocator.Alloc)(&m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid));
    TRACE_FUNC3((*m_buffer_allocator.Lock), &m_buffer_allocator, mid, &m_bitstream.Data);
    g_tsStatus.check((*m_buffer_allocator.Lock)(&m_buffer_allocator, mid, &m_bitstream.Data));
    m_bitstream.MaxLength = size;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::AllocSurfaces()
{
    if(m_default && !m_request.NumFrameMin)
    {
        QueryIOSurf();TS_CHECK_MFX;
    }
    return tsSurfacePool::AllocSurfaces(m_request);
}