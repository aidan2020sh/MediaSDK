#include "ts_common.h"

std::ostream g_tsLog(std::cout.rdbuf());
tsStatus     g_tsStatus;
mfxIMPL      g_tsImpl    = MFX_IMPL_AUTO;
mfxVersion   g_tsVersion = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
mfxU32       g_tsTrace   = 1;

#pragma warning(disable:4996)
std::string ENV(const char* name, const char* def)
{
    std::string sv = def;
    char*       cv = getenv(name);

    if(cv)
        sv = cv;

    g_tsLog << "ENV: " << name << " = " << sv << std::endl;

    return sv;
}

void MFXVideoTest::SetUp()
{
    std::string platform = ENV("TS_PLATFORM", "auto");
    std::string trace    = ENV("TS_TRACE", "1");

    if(platform.find("_sw_") != std::string::npos)
    {
        g_tsImpl = MFX_IMPL_SOFTWARE;
    }
    else if(platform != "auto")
    {
        g_tsImpl = MFX_IMPL_AUTO;
    }
    else 
    {
        g_tsImpl = MFX_IMPL_HARDWARE;
    }
        
    if(platform.find("d3d11") != std::string::npos)
    {
        g_tsImpl |= MFX_IMPL_VIA_D3D11;
    }
    
    sscanf(trace.c_str(), "%d", &g_tsTrace);
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
        g_tsLog << "FAILED" << std::endl;
        m_expected = MFX_ERR_NONE;
        ADD_FAILURE() << "returned status is wrong";
        if(m_throw_exceptions)
            throw tsFAIL;
        return m_failed = true;
    }
    g_tsLog << "OK" << std::endl;
    m_expected = MFX_ERR_NONE;
    return m_failed;
}

bool tsStatus::check(mfxStatus status)
{
    m_status = status;
    return check();
}
