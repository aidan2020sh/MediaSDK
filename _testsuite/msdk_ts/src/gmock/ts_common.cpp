#include "ts_common.h"

tsTrace      g_tsLog(std::cout.rdbuf());
tsStatus     g_tsStatus;
mfxIMPL      g_tsImpl    = MFX_IMPL_AUTO;
HWType       g_tsHWtype  = MFX_HW_UNKNOWN;
mfxVersion   g_tsVersion = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
mfxU32       g_tsTrace   = 1;
tsPlugin     g_tsPlugin;
tsStreamPool g_tsStreamPool;

bool operator == (const mfxFrameInfo& v1, const mfxFrameInfo& v2)
{
    return !memcmp(&v1, &v2, sizeof(mfxFrameInfo));
}

#pragma warning(disable:4996)
std::string ENV(const char* name, const char* def)
{
    std::string sv = def;
    char*       cv = getenv(name);

    if(cv)
        sv = cv;

    g_tsLog << "ENV: " << name << " = " << sv << "\n";

    return sv;
}

void set_brc_params(tsExtBufType<mfxVideoParam>* p)
{
    /*
    BitRate for AVC:
        [image width] x [image height] x [framerate] x [motion rank] x 0.07 = [desired bitrate]
        [motion rank]: from 1 (low motion) to 4 (high motion)
    */
    mfxU32 fr = mfxU32(p->mfx.FrameInfo.FrameRateExtN / p->mfx.FrameInfo.FrameRateExtD);
    mfxU16 br = mfxU16(p->mfx.FrameInfo.Width * p->mfx.FrameInfo.Width * fr * 2 * 0.07 / 1000);

    if (p->mfx.CodecId == MFX_CODEC_MPEG2)
        br = (mfxU16)br*1.5;
    else if (p->mfx.CodecId == MFX_CODEC_HEVC)
        br >>= 1;

    if (p->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        p->mfx.QPI = p->mfx.QPP = p->mfx.QPB = 0;
        p->mfx.QPI = 23;
        if (p->mfx.GopPicSize > 1)
            p->mfx.QPP = 25;
        if (p->mfx.GopRefDist > 1)
            p->mfx.QPB = 25;
    } else if (p->mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
               p->mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
               p->mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
               p->mfx.RateControlMethod == MFX_RATECONTROL_LA ||
               p->mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
    {
        p->mfx.TargetKbps = p->mfx.MaxKbps = p->mfx.InitialDelayInKB = 0;

        p->mfx.MaxKbps = p->mfx.TargetKbps = br;
        if (p->mfx.RateControlMethod != MFX_RATECONTROL_CBR)
        {
            p->mfx.MaxKbps = (mfxU16)(p->mfx.TargetKbps * 1.3);
        }
        // buffer = 0.5 sec
        p->mfx.BufferSizeInKB = mfxU16(p->mfx.MaxKbps / fr * mfxU16(fr / 2));
        p->mfx.InitialDelayInKB = mfxU16(p->mfx.BufferSizeInKB / 2);

        if (p->mfx.RateControlMethod == MFX_RATECONTROL_VCM)
            p->mfx.GopRefDist = 1;

        if (p->mfx.RateControlMethod == MFX_RATECONTROL_LA ||
            p->mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
        {
            p->mfx.MaxKbps = 0;
            p->mfx.InitialDelayInKB = 0;

            mfxExtCodingOption2* p_cod2 = (mfxExtCodingOption2*)p->GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
            if (!p_cod2)
            {
                mfxExtCodingOption2& cod2 = *p;
                p_cod2 = &cod2;
            }

            mfxExtCodingOption3* p_cod3 = (mfxExtCodingOption3*)p->GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);
            if (p_cod3)
            {
                p_cod3->WinBRCMaxAvgKbps = p->mfx.MaxKbps;
                p_cod3->WinBRCSize = p->mfx.FrameInfo.FrameRateExtN << 2;
            }
            p_cod2->LookAheadDepth = 20;

            if (p->mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                mfxExtCodingOption* p_cod = (mfxExtCodingOption*)p->GetExtBuffer(MFX_EXTBUFF_CODING_OPTION);
                if (!p_cod)
                {
                    mfxExtCodingOption& cod = *p;
                    p_cod = &cod;
                }
                p_cod->NalHrdConformance = MFX_CODINGOPTION_ON;
            }
        }
    } else if (p->mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        p->mfx.TargetKbps = p->mfx.Convergence = p->mfx.Accuracy = 0;

        p->mfx.TargetKbps = br;
        p->mfx.Convergence = 1;
        p->mfx.Accuracy = 2;
    } else if (p->mfx.RateControlMethod == MFX_RATECONTROL_ICQ ||
               p->mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        p->mfx.TargetKbps = p->mfx.Convergence = p->mfx.Accuracy = 0;

        p->mfx.ICQQuality = 20;
        
        if (p->mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
        {
            mfxExtCodingOption2* p_cod2 = (mfxExtCodingOption2*)p->GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
            if (p_cod2)
            {
                p_cod2->LookAheadDepth = 20;
            } else
            {
                mfxExtCodingOption2& cod2 = *p;
                cod2.LookAheadDepth = 20;
            }
        }
    } else if (p->mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        p->mfx.TargetKbps = p->mfx.Convergence = p->mfx.Accuracy = 0;
        p->mfx.ICQQuality = 0;

        mfxExtCodingOption3* p_cod3 = (mfxExtCodingOption3*)p->GetExtBuffer(MFX_EXTBUFF_CODING_OPTION3);
        if (p_cod3)
        {
            p_cod3->QVBRQuality = 20;
        } else
        {
            mfxExtCodingOption3& cod3 = *p;
            cod3.QVBRQuality = 20;
        }
    }
}

void MFXVideoTest::SetUp()
{
    std::string platform = ENV("TS_PLATFORM", "auto");
    std::string trace    = ENV("TS_TRACE", "1");
    std::string plugins  = ENV("TS_PLUGINS", "");

    if(platform.size())
    {
        if(platform.find("_sw_") != std::string::npos)
        {
            g_tsImpl = MFX_IMPL_SOFTWARE;
        }
        else if(platform == "auto")
        {
            g_tsImpl = MFX_IMPL_AUTO;
        }
        else 
        {
            if(platform.find("snb") != std::string::npos)
                g_tsHWtype = MFX_HW_SNB;
            else if(platform.find("ivb") != std::string::npos)
                g_tsHWtype = MFX_HW_IVB;
            else if(platform.find("vlv") != std::string::npos)
                g_tsHWtype = MFX_HW_VLV;
            else if(platform.find("hsw-ult") != std::string::npos)
                g_tsHWtype = MFX_HW_HSW_ULT;
            else if(platform.find("hsw") != std::string::npos)
                g_tsHWtype = MFX_HW_HSW;
            else if(platform.find("bdw") != std::string::npos)
                g_tsHWtype = MFX_HW_BDW;
            else if(platform.find("skl") != std::string::npos)
                g_tsHWtype = MFX_HW_SKL;
            else
                g_tsHWtype = MFX_HW_UNKNOWN;

            g_tsImpl = MFX_IMPL_HARDWARE;
        }

        if(platform.find("d3d11") != std::string::npos)
        {
            g_tsImpl |= MFX_IMPL_VIA_D3D11;
        }
    }
    sscanf(trace.c_str(), "%d", &g_tsTrace);

    g_tsPlugin.Init(plugins, platform);
}
#pragma warning(default:4996)

void MFXVideoTest::TearDown()
{
}

tsStatus::tsStatus()
        : m_status(MFX_ERR_NONE)
        , m_expected(MFX_ERR_NONE)
        , m_failed(false)
        , m_throw_exceptions(true)
{
}

tsStatus::~tsStatus()
{
}

bool tsStatus::check()
{
    g_tsLog << "CHECK STATUS(expected " << m_expected << "): " << m_status << " -- ";
    if(m_status != m_expected)
    {
        g_tsLog << "FAILED\n";
        m_expected = MFX_ERR_NONE;
        ADD_FAILURE() << "returned status is wrong";
        if(m_throw_exceptions)
            throw tsFAIL;
        return m_failed = true;
    }
    g_tsLog << "OK\n";
    m_expected = MFX_ERR_NONE;
    return m_failed;
}

bool tsStatus::check(mfxStatus status)
{
    m_status = status;
    return check();
}


