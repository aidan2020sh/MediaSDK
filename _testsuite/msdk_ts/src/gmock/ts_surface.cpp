#include "ts_surface.h"

tsFrame::tsFrame(mfxFrameSurface1 s)
    : m_pFrame(0)
    , m_info(s.Info)
{
    switch(s.Info.FourCC)
    {
    case MFX_FOURCC_NV12: m_pFrame = new tsFrameNV12(s.Data); break;
    case MFX_FOURCC_YV12: m_pFrame = new tsFrameYV12(s.Data); break;
    case MFX_FOURCC_YUY2: m_pFrame = new tsFrameYUY2(s.Data); break;
    case MFX_FOURCC_BGR4: std::swap(s.Data.B, s.Data.R);
    case MFX_FOURCC_RGB4: m_pFrame = new tsFrameRGB4(s.Data); break;
        break;
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }
}

tsFrame::~tsFrame()
{
    if(m_pFrame)
    {
        delete m_pFrame;
    }
}

inline mfxU32 NumComponents(mfxFrameInfo fi)
{
    if(    fi.FourCC == MFX_FOURCC_RGB4
        || fi.FourCC == MFX_FOURCC_BGR4)
    {
        return 4;
    }

    if(fi.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
    {
        return 1;
    }

    return 3;
}

tsFrame& tsFrame::operator=(tsFrame& src)
{
    mfxU32 n = TS_MIN(NumComponents(m_info), NumComponents(src.m_info));

    if(    src.m_info.Width > m_info.Width
        || src.m_info.Height > m_info.Height)
    {
        g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if(isYUV() && src.isYUV())
    {
        for(mfxU32 h = 0; h < m_info.Height; h ++)
        {
            for(mfxU32 w = 0; w < m_info.Width; w ++)
            {
                Y(w, h) = src.Y(w, h);
                if(n == 3)
                {
                    U(w, h) = src.U(w, h);
                    V(w, h) = src.V(w, h);
                }
            }
        }
    }
    else if(isRGB() && src.isRGB())
    {
        for(mfxU32 h = 0; h < m_info.Height; h ++)
        {
            for(mfxU32 w = 0; w < m_info.Width; w ++)
            {
                R(w, h) = src.R(w, h);
                G(w, h) = src.G(w, h);
                B(w, h) = src.B(w, h);
                A(w, h) = src.A(w, h);
            }
        }
    }
    else
    {
        g_tsStatus.check(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    return *this;
}

tsSurfaceFiller::tsSurfaceFiller(mfxU32 n_frames) 
    : m_max(n_frames)
    , m_cur(0)
    , m_eos(false)
{
}

mfxFrameSurface1* tsSurfaceFiller::FillSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
{
    if(ps)
    {
        if(m_cur++ >= m_max)
        {
            return 0;
        }
        bool useAllocator = pfa && !ps->Data.Y;

        if(useAllocator)
        {
            TRACE_FUNC3(mfxFrameAllocator::Lock, pfa->pthis, ps->Data.MemId, &(ps->Data));
            g_tsStatus.check(pfa->Lock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
        }

        TRACE_FUNC1(tsSurfaceFiller::FillSurface, *ps);
        g_tsStatus.check(FillSurface(*ps));

        if(useAllocator)
        {
            TRACE_FUNC3(mfxFrameAllocator::Unlock, pfa->pthis, ps->Data.MemId, &(ps->Data));
            g_tsStatus.check(pfa->Unlock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
        }
    }

    if(m_eos)
    {
        m_max = m_cur;
        return 0;
    }

    return ps;
}

tsNoiseFiller::tsNoiseFiller(mfxU32 n_frames) 
    : tsSurfaceFiller(n_frames)
{
}

tsNoiseFiller::~tsNoiseFiller()
{
}

mfxStatus tsNoiseFiller::FillSurface(mfxFrameSurface1& s)
{
    tsFrame d(s);

    if(d.isYUV())
    {
        for(mfxU32 h = 0; h < s.Info.Height; h++)
        {
            for(mfxU32 w = 0; w < s.Info.Width; w++)
            {
                d.Y(w,h) = rand() % (1 << 8);
                d.U(w,h) = rand() % (1 << 8);
                d.V(w,h) = rand() % (1 << 8);
            }
        }
    }
    else if(d.isRGB())
    {
        for(mfxU32 h = 0; h < s.Info.Height; h++)
        {
            for(mfxU32 w = 0; w < s.Info.Width; w++)
            {
                d.R(w,h) = rand() % (1 << 8);
                d.G(w,h) = rand() % (1 << 8);
                d.B(w,h) = rand() % (1 << 8);
                d.A(w,h) = rand() % (1 << 8);
            }
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

tsRawReader::tsRawReader(const char* fname, mfxFrameInfo fi, mfxU32 n_frames)
    : tsReader(fname)
    , tsSurfaceFiller(n_frames)
    , m_fsz(0)
    , m_buf(0)
    , m_surf()
{
    Init(fi);
}

tsRawReader::tsRawReader(mfxBitstream bs, mfxFrameInfo fi, mfxU32 n_frames)
    : tsReader(bs)
    , tsSurfaceFiller(n_frames)
    , m_fsz(0)
    , m_buf(0)
    , m_surf()
{
    Init(fi);
}

void tsRawReader::Init(mfxFrameInfo fi)
{
    mfxU32 fsz = fi.Width * fi.Height;
    mfxU32 pitch = 0;
    mfxFrameData& m_data = m_surf.Data;

    m_surf.Info = fi;

    switch(fi.ChromaFormat)
    {
    case MFX_CHROMAFORMAT_YUV400 : m_fsz = fsz; break;
    case MFX_CHROMAFORMAT_YUV420 : m_fsz = fsz * 3 / 2; break;
    case MFX_CHROMAFORMAT_YUV422 : 
    case MFX_CHROMAFORMAT_YUV422V: m_fsz = fsz * 2; break;
    case MFX_CHROMAFORMAT_YUV444 : m_fsz = fsz * 4; break;
    case MFX_CHROMAFORMAT_YUV411 : 
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }

    m_buf = new mfxU8[m_fsz];

    switch(fi.FourCC)
    {
    case MFX_FOURCC_NV12:
        m_data.Y     = m_buf;
        m_data.UV    = m_data.Y + fsz;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_YV12:
        m_data.Y     = m_buf;
        m_data.U     = m_data.Y + fsz;
        m_data.V     = m_data.U + fsz / 4;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_YUY2:
        m_data.Y     = m_buf;
        m_data.U     = m_data.Y + 1;
        m_data.V     = m_data.U + 3;
        pitch        = fi.Width * 2;
        break;
    case MFX_FOURCC_RGB4:
        m_data.R     = m_buf;
        m_data.G     = m_data.R + fsz;
        m_data.B     = m_data.G + fsz;
        m_data.A     = m_data.B + fsz;
        pitch        = fi.Width;
        break;
    case MFX_FOURCC_BGR4:
        m_data.B     = m_buf;
        m_data.G     = m_data.B + fsz;
        m_data.R     = m_data.G + fsz;
        m_data.A     = m_data.R + fsz;
        pitch        = fi.Width;
        break;
    default: g_tsStatus.check(MFX_ERR_UNSUPPORTED);
    }

    m_data.PitchLow  = (pitch & 0xFFFF);
    m_data.PitchHigh = (pitch >> 16);
}

tsRawReader::~tsRawReader()
{
    if(m_buf)
    {
        delete[] m_buf;
    }
}

mfxStatus tsRawReader::FillSurface(mfxFrameSurface1& s)
{
    m_eos = !Read(m_buf, m_fsz);

    if(!m_eos)
    {
        tsFrame src(m_surf);
        tsFrame dst(s);

        dst = src;
        s.Data.FrameOrder = m_cur - 1;
    }

    return MFX_ERR_NONE;
}
