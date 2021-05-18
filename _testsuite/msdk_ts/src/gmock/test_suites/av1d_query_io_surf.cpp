/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_decoder.h"

namespace av1d_query_io_surf
{

enum
{
    ZERO_SESSION = 1,
    ZERO_PARAM = 2,
    ZERO_REQUEST = 3,
};

typedef struct
{
    mfxU32      set_par;
    mfxU32      IOPattern;
    mfxU32      AsyncDepth;
    mfxStatus   sts;
} tc_struct;

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AV1){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;
    static const tc_struct test_case[];
};

const tc_struct TestSuite::test_case[] =
{
    {// 0
        ZERO_SESSION,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
        1,
        MFX_ERR_INVALID_HANDLE
    },
    {// 1
        ZERO_PARAM,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
        1,
        MFX_ERR_NULL_PTR
    },
    {// 2
        ZERO_REQUEST,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
        1,
        MFX_ERR_NULL_PTR
    },
    {// 3
        0,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY,
        5,
        MFX_ERR_NONE
    },
    {// 4
        0,
        MFX_IOPATTERN_OUT_VIDEO_MEMORY,
        5,
        MFX_ERR_NONE
    },
    //IOPattern
    {// 5
        0,
        MFX_IOPATTERN_OUT_VIDEO_MEMORY + 2,
        1,
        MFX_ERR_NONE
    },
    {// 6
        0,
        MFX_IOPATTERN_OUT_VIDEO_MEMORY + 1,
        1,
        MFX_ERR_NONE
    },
    {// 7
        0,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY + 2,
        1,
        MFX_ERR_NONE
    },
    {// 8
        0,
        MFX_IOPATTERN_OUT_SYSTEM_MEMORY + 1,
        1,
        MFX_ERR_NONE
    },
    {// 9
        0,
        6,
        1,
        MFX_ERR_INVALID_VIDEO_PARAM
    },
    {// 10
        0,
        1,
        1,
        MFX_ERR_INVALID_VIDEO_PARAM
    },
    {// 11
        0,
        2,
        1,
        MFX_ERR_INVALID_VIDEO_PARAM
    },
    {// 12
        0,
        0,
        1,
        MFX_ERR_INVALID_VIDEO_PARAM
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];

    MFXInit();

    if(m_uid)
        Load();

    m_par.IOPattern  = tc.IOPattern;
    m_par.AsyncDepth = tc.AsyncDepth;

    g_tsStatus.expect(tc.sts);

    if (tc.set_par == ZERO_SESSION)
        QueryIOSurf(NULL, m_pPar, m_pRequest);
    else if (tc.set_par == ZERO_PARAM)
        QueryIOSurf(m_session, NULL, m_pRequest);
    else if (tc.set_par == ZERO_REQUEST)
        QueryIOSurf(m_session, m_pPar, NULL);
    else
        QueryIOSurf(m_session, m_pPar, m_pRequest);

    if(g_tsStatus.get() >= MFX_ERR_NONE)
    {
        mfxU32 expectedType = MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_EXTERNAL_FRAME;

        if(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
            expectedType |= MFX_MEMTYPE_SYSTEM_MEMORY;
        else if (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            expectedType |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;

        EXPECT_EQ(expectedType, m_request.Type);
        EXPECT_EQ(m_par.mfx.FrameInfo, m_request.Info);
        EXPECT_GT(m_request.NumFrameMin, 0);
        EXPECT_GE(m_request.NumFrameSuggested, m_request.NumFrameMin);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(av1d_query_io_surf);
}