/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2018-2019 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"

namespace av1d_reset
{

class TestSuite : public tsVideoDecoder
{
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 4;

protected:

    struct tc_struct
    {
        mfxStatus sts;
        const std::string stream[2];
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

public:

    TestSuite() : tsVideoDecoder(MFX_CODEC_AV1), m_session(0), m_repeat(1){}
    ~TestSuite() { }

    int RunTest(tc_struct const&);

protected:

    static const unsigned int n_cases;
    static const tc_struct test_case[];

    mfxSession m_session;
    mfxU32 m_repeat;

    enum CTRL_TYPE
    {
          STAGE   = 0xFF000000
        , INIT    = 0x01000000
        , RESET   = 0x00000000
        , SESSION = 1
        , MFXVPAR
        , CLOSE_DEC
        , REPEAT
        , ALLOCATOR
        , EXT_BUF
    };

    void apply_par(const tc_struct& p, mfxU32 stage)
    {
        bool use_customized_allocator = true;
#if (defined(LINUX32) || defined(LINUX64))
        use_customized_allocator = false;
#endif

        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type & STAGE))
                continue;

            switch(c.type & ~STAGE)
            {
            case SESSION   : base = (void**)&m_session;      break;
            case MFXVPAR   : base = (void**)&m_pPar;         break;
            case REPEAT    : base = (void**)&m_repeat;       break;
            case ALLOCATOR :
            {
                if (m_pVAHandle && (c.par[0] != frame_allocator::SOFTWARE))
                    SetAllocator(m_pVAHandle, true);
                else
                    SetAllocator(
                                 new frame_allocator(
                                 (frame_allocator::AllocatorType)    c.par[0],
                                 (frame_allocator::AllocMode)        c.par[1],
                                 (frame_allocator::LockMode)         c.par[2],
                                 (frame_allocator::OpaqueAllocMode)  c.par[3]
                                 ),
                                 false
                             );
                m_use_memid = true;
                use_customized_allocator = true;
                break;
            }
            case CLOSE_DEC : Close(); break;
            case EXT_BUF   : m_par.AddExtBuffer(c.par[0], c.par[1]); break;
            default: break;
            }

            if(base)
            {
                if(c.field)
                    tsStruct::set(*base, *c.field, c.par[0]);
                else
                    //no way to have persistent pointers here, the only valid value is NULL
                    *base = NULL;
            }
        }

        // now default allocator is incorrectly setup for this type memory in the base class
        // so we need this fix on per-suite basis
        if(!use_customized_allocator && m_pVAHandle &&
            m_par.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
            SetAllocator(m_pVAHandle, true);
        }
    }
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, {"", ""}},
    {/* 1*/ MFX_ERR_NONE, {"", ""}, {REPEAT, 0, {50}}},
    {/* 2*/ MFX_ERR_NONE, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {50}}}
    },
    {/* 3*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/* 4*/ MFX_ERR_INVALID_HANDLE, {"", ""}, {RESET|SESSION}},
    {/* 5*/ MFX_ERR_NOT_INITIALIZED, {"", ""}, {RESET|CLOSE_DEC}},
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, { "", "" }, { RESET | MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV411} } },
    {/* 7*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, {720 + 32}}},
    {/* 8*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 + 32}}},
    {/* 9*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {10}}},
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {1}}},
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {2}}},
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*14*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*15*/ MFX_ERR_NONE, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*16*/ MFX_ERR_NONE, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {720 - 8}}},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 - 8}}},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {8192 + 10}}},
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {4096 + 10}}},
    {/*21*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, {10}}},
    {/*22*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, {MFX_PICSTRUCT_FIELD_SINGLE}}},
    {/*23*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, {10}}},
    {/*24*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, {720 + 10}}},
    {/*25*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, {480 + 10}}},
    {/*26*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, {2}}},
    {/*27*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, {2}}},
    {/*28*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, {275}}},
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YV12}}},
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOptionSPSPPS)}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVideoSignalInfo)}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption)}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption2)}}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVPPDenoise)}}},
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVppAuxData)}}},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAVCRefListCtrl)}}},
    {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAvcTemporalLayers)}}},
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtPictureTimingSEI)}}},
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderCapability)}}},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderResetOption)}}},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {0, 0}}},
};

unsigned int const TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(tc_struct const& tc)
{
    TS_START;

    const char* stream0 = g_tsStreamPool.Get(tc.stream[0]);
    const char* stream1 = g_tsStreamPool.Get(tc.stream[1] == "" ? tc.stream[0] : tc.stream[1]);
    g_tsStreamPool.Reg();


    MFXInit();
    m_session = tsSession::m_session;

    apply_par(tc, INIT);

    if(m_uid)
    {
        Load();
    }

    const Ipp32u size = 1024 * 1024;
    if(tc.stream[0] != "")
    {
        if (m_bs_processor)
            delete m_bs_processor;
        m_bs_processor = new tsBitstreamReaderIVF(stream0, size);
    } else
    {
        m_par.mfx.CodecProfile = MFX_PROFILE_AV1_MAIN;
        m_par_set = true;
    }

    if (g_tsIsSSW && (m_par.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        g_tsLog<<"WARNING: Case Skipped!\n";
    }
    else
    {
        Init();
        GetVideoParam();

        if(tc.stream[0] != "")
        {
            DecodeFrames(3);
        }

        if(tc.stream[1] != "")
        {
            if (m_bs_processor)
                delete m_bs_processor;
            m_bs_processor = new tsBitstreamReaderIVF(stream1, size);
            m_bitstream.DataLength = 0;
            DecodeHeader();
        }

        apply_par(tc, RESET);
        if (g_tsIsSSW && (m_par.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            g_tsLog<<"WARNING: Reset Skipped!\n";
        }
        else
        {
            for(mfxU32 i = 0; i < m_repeat; i ++)
            {
                g_tsStatus.expect(tc.sts);
                Reset(m_session, m_pPar);
            }

            if((tc.stream[0] != "" || tc.stream[1] != "") && (tc.sts >= MFX_ERR_NONE))
            {
                DecodeFrames(3);
            }
        }
    }

    TS_END;
    return 0;
}

template <unsigned fourcc>
struct TestSuiteExt
    : public TestSuite
{
    static
    int RunTest(unsigned int id);

    static tc_struct const test_cases[];
    static unsigned int const n_cases;
};

char const *stream_av1_8b_inter_basic = "conformance/av1/DVK/MainProfile_8bit420/Syntax_AV1_mainb8ss420_432x240_101_inter_basic_3_1.3_20190124.av1";
char const *stream_av1_10b_inter_basic = "conformance/av1/DVK/MainProfile_10bit420/Syntax_AV1_mainb10ss420_432x240_101_inter_basic_4_1.3_20190124.av1";

/* 8b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_NV12>::test_cases[] =
{
    {/*42*/ MFX_ERR_NONE, {stream_av1_8b_inter_basic, ""},
        {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {2}}}
    },

    {/*43*/ MFX_ERR_NONE, {stream_av1_8b_inter_basic, ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/*44*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {stream_av1_8b_inter_basic, stream_av1_10b_inter_basic }},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_NV12>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_NV12>::test_cases) / sizeof(TestSuite::tc_struct);

/* 10b 420 */
template <>
TestSuite::tc_struct const TestSuiteExt<MFX_FOURCC_P010>::test_cases[] =
{
    {/*42*/ MFX_ERR_NONE, {stream_av1_10b_inter_basic, ""},
        {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {2}}}
    },

    {/*43*/ MFX_ERR_NONE, {stream_av1_10b_inter_basic, ""},
        {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/*44*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {stream_av1_10b_inter_basic, stream_av1_8b_inter_basic}},
};
template <>
unsigned int const TestSuiteExt<MFX_FOURCC_P010>::n_cases = TestSuite::n_cases + sizeof(TestSuiteExt<MFX_FOURCC_P010>::test_cases) / sizeof(TestSuite::tc_struct);

template <unsigned fourcc>
int TestSuiteExt<fourcc>::RunTest(unsigned int id)
{
    auto tc =
        id >= TestSuite::n_cases ?
        test_cases[id - TestSuite::n_cases] : TestSuite::test_case[id];

    TestSuite suite;
    return suite.RunTest(tc);
}

TS_REG_TEST_SUITE(av1d_8b_420_nv12_reset,  TestSuiteExt<MFX_FOURCC_NV12>::RunTest, TestSuiteExt<MFX_FOURCC_NV12>::n_cases);
TS_REG_TEST_SUITE(av1d_10b_420_p010_reset, TestSuiteExt<MFX_FOURCC_P010>::RunTest, TestSuiteExt<MFX_FOURCC_P010>::n_cases);

}
