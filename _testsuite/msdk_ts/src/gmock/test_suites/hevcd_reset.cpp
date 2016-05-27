/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2003-2016 Intel Corporation. All Rights Reserved.
//
*/
#include "ts_decoder.h"
#include "ts_struct.h"

namespace hevcd_reset
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_HEVC), m_session(0), m_repeat(1){}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxU32 m_repeat;

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

    static const tc_struct test_case[];

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
                    *((mfxU32*)base) = c.par[0];
            }
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
    {/* 3*/ MFX_ERR_NONE, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {REPEAT, 0, {2}}}
    },
    {/* 4*/ MFX_ERR_NONE, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", ""},
       {{INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
        {REPEAT, 0, {2}}}
    },
    {/* 5*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"conformance/hevc/itu/RPS_B_qualcomm_5.bit", "conformance/hevc/itu/PICSIZE_B_Bossen_1.bin"}},
    {/* 6*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_VIDEO_MEMORY}}},
    {/* 7*/ MFX_ERR_INVALID_HANDLE, {"", ""}, {RESET|SESSION}},
    {/* 8*/ MFX_ERR_NOT_INITIALIZED, {"", ""}, {RESET|CLOSE_DEC}},
    {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, { "", "" }, { RESET | MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat, {MFX_CHROMAFORMAT_YUV411} } },
    {/*10*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, {720 + 32}}},
    {/*11*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 + 32}}},
    {/*12*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {10}}},
    {/*13*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}}},
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {1}}},
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.Protected, {2}}},
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*17*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*18*/ MFX_ERR_NONE, {"", ""},
       {{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*19*/ MFX_ERR_NONE, {"", ""},
       {//{INIT|ALLOCATOR, 0, {frame_allocator::SOFTWARE, frame_allocator::ALLOC_MAX}},
        {INIT|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
        {RESET|MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, {MFX_IOPATTERN_OUT_OPAQUE_MEMORY}}}
    },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {720 - 8}}},
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {480 - 8}}},
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  {8192 + 10}}},
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, {4096 + 10}}},
    {/*24*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, {10}}},
    {/*25*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, {10}}},
    {/*26*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, {720 + 10}}},
    {/*27*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, {480 + 10}}},
    {/*28*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, {2}}},
    {/*29*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, {2}}},
    {/*30*/ MFX_ERR_NONE, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, {275}}},
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|MFXVPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, {MFX_FOURCC_YV12}}},
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOptionSPSPPS)}}},
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVideoSignalInfo)}}},
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption)}}},
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtCodingOption2)}}},
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVPPDenoise)}}},
    {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtVppAuxData)}}},
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAVCRefListCtrl)}}},
    {/*39*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtAvcTemporalLayers)}}},
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtPictureTimingSEI)}}},
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderCapability)}}},
    {/*42*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {EXT_BUF_PAR(mfxExtEncoderResetOption)}}},
    {/*43*/ MFX_ERR_INVALID_VIDEO_PARAM, {"", ""}, {RESET|EXT_BUF, 0, {0, 0}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto tc = test_case[id];
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

    if(tc.stream[0] != "")
    {
        if (m_bs_processor)
            delete m_bs_processor;
        m_bs_processor = new tsBitstreamReader(stream0, 100000);
    } else
    {
        m_par.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
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
            m_bs_processor = new tsBitstreamReader(stream1, 100000);
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

TS_REG_TEST_SUITE_CLASS(hevcd_reset);
}
