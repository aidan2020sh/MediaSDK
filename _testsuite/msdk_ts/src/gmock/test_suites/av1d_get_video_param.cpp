/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <memory>
#include "ts_decoder.h"
#include "ts_struct.h"

#define TEST_NAME av1d_get_video_param
namespace TEST_NAME
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AV1) { }
    ~TestSuite() {}

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        mfxU8  stage;
        mfxU8  isInvalid;
    };
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32    mode;
        f_pair    set_par[MAX_NPARS];
        mfxU32    n_frames;
    };
    static const tc_struct test_case[];      //base cases

    template<mfxU32 fourcc>
    int RunTest_fourcc(const unsigned int id);

    static const unsigned int n_cases;

private:

    enum
    {
        NULL_SESSION = 1,
        NULL_PAR,
        MULTIPLE_INIT,
        FAILED_INIT,
        CLOSED,
    };

    enum
    {
        INIT = 0,
        CHECK,
    };

    void AllocOpaque();
    void ReadStream();

    int RunTest(const tc_struct& tc);
    std::unique_ptr<tsBitstreamProcessor> m_bs_reader;
    std::string m_input_stream_name;
    bool m_have_stream;
};

static const tsStruct::Field* const PicStruct     (&tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct);
static const tsStruct::Field* const IOPattern     (&tsStruct::mfxVideoParam.IOPattern);
static const tsStruct::Field* const Width         (&tsStruct::mfxVideoParam.mfx.FrameInfo.Width);
static const tsStruct::Field* const Height        (&tsStruct::mfxVideoParam.mfx.FrameInfo.Height);
static const tsStruct::Field* const CropX         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropX);
static const tsStruct::Field* const CropY         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropY);
static const tsStruct::Field* const CropW         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW);
static const tsStruct::Field* const CropH         (&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH);
static const tsStruct::Field* const FourCC        (&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC);
static const tsStruct::Field* const ChromaFormat  (&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat);
static const tsStruct::Field* const AspectRatioW  (&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW);
static const tsStruct::Field* const AspectRatioH  (&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH);
static const tsStruct::Field* const FrameRateExtN (&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN);
static const tsStruct::Field* const FrameRateExtD (&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD);
static const tsStruct::Field* const BitDepthLuma  (&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma);
static const tsStruct::Field* const BitDepthChroma(&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma);

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair (&pairs)[MAX_NPARS], mfxU32 stage)
{
    const size_t length = sizeof(pairs)/sizeof(pairs[0]);
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f && stage == pairs[i].stage)
        {
            void* ptr = 0;

            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    const std::string& buffer_name = pairs[i].f->name.substr(0, pairs[i].f->name.find(":"));
                    EXPECT_NE(nullptr, ptr) << "ERROR: extended buffer is missing!\n" << "Missing buffer: " << buffer_name <<"\n";
                    throw tsFAIL;
                }
            }

            if(pairs[i].isInvalid)
                tsStruct::check_ne(ptr, *pairs[i].f, pairs[i].v);
            else
                tsStruct::check_eq(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}

const mfxU32 MAX_WIDTH = 16384;
const mfxU32 MAX_HEIGHT = 16384;

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_INVALID_HANDLE,  NULL_SESSION},
    {/*01*/ MFX_ERR_NULL_PTR,        NULL_PAR},
    {/*02*/ MFX_ERR_NOT_INITIALIZED, CLOSED},
    {/*03*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT},

    {/*04*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY}, 5},
    {/*05*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_VIDEO_MEMORY} , 5},
    {/*06*/ MFX_ERR_NONE, 0, {IOPattern, MFX_IOPATTERN_OUT_OPAQUE_MEMORY}, 5},
    {/*07*/ MFX_ERR_NONE, 0, {{IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
                              {IOPattern, MFX_IOPATTERN_OUT_SYSTEM_MEMORY, CHECK}}, 0},

    {/*08*/ MFX_ERR_NONE, 0, {{Width, 1920}, {Height, 1088}}, 0},
    {/*09*/ MFX_ERR_NONE, 0, {{Width, 3840}, {Height, 2160}}, 0},
    {/*10*/ MFX_ERR_NONE, 0, {{CropX, MAX_WIDTH + 1}, {CropY, 2160}, {CropW, 23}, {CropH, 15}, //should be ignored during init
                              {CropX, 0, CHECK}, {CropY, 0, CHECK}, {CropW, 0, CHECK, 1}, {CropH,  0, CHECK, 1}}, 5}, //real values from stream should be reported
    // Test case is required to check that frame_width/frame_height from stream are mapped to Crops and properly returned by GetVideoParam.
    // There is no such AV1 stream so far. Implementation of stream re-packer to write non-zero frame_width/frame_height is expensive.
    // So there is no such test case so far.
    {/*11*/ MFX_ERR_NONE, 0, {{PicStruct, MFX_PICSTRUCT_FRAME_TRIPLING}, {PicStruct, MFX_PICSTRUCT_PROGRESSIVE, CHECK}}, 5}, //should be ignored during init and reported then
    {/*12*/ MFX_ERR_NONE, 0, {{AspectRatioW, 0}, {AspectRatioH, 0}, {AspectRatioW, 0, CHECK, 1}, {AspectRatioH, 0, CHECK, 1},}, 5},
    {/*13*/ MFX_ERR_NONE, 0, {{AspectRatioW, 16}, {AspectRatioH, 9}, {AspectRatioW, 16, CHECK, 0}, {AspectRatioH, 9, CHECK, 0}}},
    {/*14*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 0}, {FrameRateExtD, 0}, {FrameRateExtN, 0, CHECK, 1}, {FrameRateExtD, 0, CHECK, 1}}},
    {/*15*/ MFX_ERR_NONE, 0, {{FrameRateExtN, 30}, {FrameRateExtD, 1}, {FrameRateExtN, 30, CHECK, 0}, {FrameRateExtD, 1, CHECK, 0}}},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

struct streamDesc
{
    mfxU16 w;
    mfxU16 h;
    const char *name;
};

const streamDesc NO_STREAM = { 0, 0, "" };

const streamDesc streams[] = {
    { 128, 64, "conformance/av1/avp_rev25_1_akiyo0_128x64_2frm_zeromv.ivf"},
};

const streamDesc& getStreamDesc(const mfxU32& fourcc)
{
    switch(fourcc)
    {
        case MFX_FOURCC_NV12:
            return streams[0];
        case MFX_FOURCC_P010:
            return NO_STREAM;
        case MFX_FOURCC_AYUV:
        case MFX_FOURCC_Y410:
        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y416:
        default:
            assert(0); 
            return NO_STREAM;
    }
}

const mfxU32 DEFAULT_WIDTH = 128;
const mfxU32 DEFAULT_HEIGHT = 64;

template<mfxU32 fourcc>
int TestSuite::RunTest_fourcc(const unsigned int id)
{
    const streamDesc& bsDesc = getStreamDesc(fourcc);
    if (memcmp(&bsDesc, &NO_STREAM, sizeof(streamDesc)))
    { 
        m_have_stream = true;
        m_input_stream_name = bsDesc.name;

        m_par.mfx.FrameInfo.Width = (bsDesc.w + 15) & ~15;
        m_par.mfx.FrameInfo.Height = (bsDesc.h + 15) & ~15;
    }
    else
    {
        m_have_stream = false;

        m_par.mfx.FrameInfo.Width = DEFAULT_WIDTH;
        m_par.mfx.FrameInfo.Height = DEFAULT_HEIGHT;
    }

    m_par.mfx.FrameInfo.FourCC = fourcc;
    set_chromaformat_mfx(&m_par);

    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_AYUV: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 8; break;

        case MFX_FOURCC_P010:
        case MFX_FOURCC_Y410: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 10; break;

        case MFX_FOURCC_P016:
        case MFX_FOURCC_Y416: m_par.mfx.FrameInfo.BitDepthChroma = m_par.mfx.FrameInfo.BitDepthLuma = 12; break;
    };

    if (   fourcc == MFX_FOURCC_P010
        || fourcc == MFX_FOURCC_P016
        || fourcc == MFX_FOURCC_Y416)
        m_par.mfx.FrameInfo.Shift = 1;

    m_par_set = true; //we are not testing DecodeHeader here

    const tc_struct& tc = test_case[id];
    return RunTest(tc);
}

int TestSuite::RunTest(const tc_struct& tc)
{
    TS_START;

    if (m_have_stream)
        ReadStream();

    MFXInit();

    tsStruct::SetPars(m_par, tc, INIT);

    bool isSW = !!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);
    UseDefaultAllocator(isSW);
    m_pFrameAllocator = GetAllocator();
    if (!isSW && !m_is_handle_set)
    {
        mfxHDL hdl = 0;
        mfxHandleType type = static_cast<mfxHandleType>(0);
        GetAllocator()->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);
    }
    if(!(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        SetFrameAllocator(m_session, GetAllocator());
    }
    else
    {
        AllocOpaque();
    }

    if (FAILED_INIT == tc.mode)
    {
        tsExtBufType<mfxVideoParam> tmpPar(m_par);
        tmpPar.mfx.FrameInfo.Width = tmpPar.mfx.FrameInfo.Height = 5;
        g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        Init(m_session, &tmpPar);
    }
    else
    {
        tsExtBufType<mfxVideoParam> tmpPar(m_par); //component must not rely on provided buffer after Init function exit
        Init(m_session, &tmpPar);

        if (m_have_stream)
            DecodeFrames(tc.n_frames);
    }

    if (CLOSED == tc.mode)
    {
        g_tsStatus.expect(MFX_ERR_NONE);
        Close();
    }

    //test section
    {
        tsExtBufType<mfxVideoParam> outPar;
        mfxSession ses     = (NULL_SESSION  == tc.mode) ? nullptr : m_session;
        mfxVideoParam* par = (NULL_PAR      == tc.mode) ? nullptr : &outPar;

        g_tsStatus.expect(tc.sts);
        GetVideoParam(ses, par);

        if(tc.sts >= MFX_ERR_NONE)
        {
            CheckParams(outPar, tc.set_par, CHECK);

            EXPECT_EQ(m_par.mfx.FrameInfo.BitDepthLuma  ,outPar.mfx.FrameInfo.BitDepthLuma  );
            EXPECT_EQ(m_par.mfx.FrameInfo.BitDepthChroma,outPar.mfx.FrameInfo.BitDepthChroma);
            EXPECT_EQ(m_par.mfx.FrameInfo.Shift         ,outPar.mfx.FrameInfo.Shift         );
            EXPECT_EQ(m_par.mfx.FrameInfo.FourCC        ,outPar.mfx.FrameInfo.FourCC        );
            EXPECT_EQ(m_par.mfx.FrameInfo.Width         ,outPar.mfx.FrameInfo.Width         );
            EXPECT_EQ(m_par.mfx.FrameInfo.Height        ,outPar.mfx.FrameInfo.Height        );
            EXPECT_EQ(m_par.mfx.FrameInfo.ChromaFormat  ,outPar.mfx.FrameInfo.ChromaFormat  );
        }
    }

    g_tsStatus.disable_next_check();
    Close();

    TS_END;
    return 0;
}

void TestSuite::ReadStream()
{
    const char* sname = g_tsStreamPool.Get(m_input_stream_name);
    g_tsStreamPool.Reg();
    m_bs_reader.reset(new tsBitstreamReaderIVF(sname, 1024*1024) );
    m_bs_processor = m_bs_reader.get();

    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);
}

void TestSuite::AllocOpaque()
{
    if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        AllocOpaqueSurfaces();
    }
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(av1d_8b_420_nv12_get_video_param,  RunTest_fourcc<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(av1d_10b_420_p010_get_video_param,  RunTest_fourcc<MFX_FOURCC_P010>, n_cases);
}
#undef TEST_NAME
