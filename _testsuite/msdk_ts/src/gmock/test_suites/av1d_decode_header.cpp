/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace av1d_decode_header
{

class TestSuite
    : public tsVideoDecoder
{
public:

    TestSuite()
        : tsVideoDecoder(MFX_CODEC_AV1, true)
    {}
    ~TestSuite()
    {}

    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    static const char path[];

    static const mfxU32 max_num_ctrl     = 10;

    struct tc_struct
    {
        mfxStatus sts;
        std::string stream;
        struct tctrl{
            const tsStruct::Field* field;
            mfxU32 par;
        } ctrl[max_num_ctrl];
    };
    static const tc_struct test_case[];

    void apply_par(const tc_struct& p)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            if (c.field)
                tsStruct::set(m_pPar, *c.field, c.par);
        }
    }
};

const char TestSuite::path[] = "conformance/av1/";

#define EXT_BUF(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE,  "from_fulsim/akiyo0_176x144_8b_420_LowLatency_qp12.ivf",
      { { &tsStruct::mfxVideoParam.mfx.CodecProfile,             MFX_PROFILE_AV1_0 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma,   8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,         MFX_FOURCC_NV12 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,      MFX_PICSTRUCT_PROGRESSIVE },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat,   MFX_CHROMAFORMAT_YUV420 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,          176 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,         144 } },
    },

    {/* 1*/ MFX_ERR_NONE,  "self_coded/Kimono1_352x288_422.av1",
      { { &tsStruct::mfxVideoParam.mfx.CodecProfile,             MFX_PROFILE_AV1_1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma,   8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,         MFX_FOURCC_YUY2 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,      MFX_PICSTRUCT_PROGRESSIVE },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat,   MFX_CHROMAFORMAT_YUV422 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,          352 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,         288 } },
    },

    {/* 2*/ MFX_ERR_NONE,  "from_fulsim/carphone_176x144_8b_444_LowLatency_qp12.ivf",
      { { &tsStruct::mfxVideoParam.mfx.CodecProfile,             MFX_PROFILE_AV1_1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma,   8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 8 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,         MFX_FOURCC_AYUV },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,      MFX_PICSTRUCT_PROGRESSIVE },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat,   MFX_CHROMAFORMAT_YUV444 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,          176 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,         144 } },
    },

    {/* 3*/ MFX_ERR_NONE,  "from_fulsim/rain2_640x360_10b_420_LowLatency_qp12.ivf",
      { { &tsStruct::mfxVideoParam.mfx.CodecProfile,             MFX_PROFILE_AV1_2 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma,   10 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 10 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC,         MFX_FOURCC_P010 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,      MFX_PICSTRUCT_PROGRESSIVE },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat,   MFX_CHROMAFORMAT_YUV420 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,   1 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,          640 },
        { &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,         368 } },
    },
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

void CheckParams(mfxVideoParam const & expected, mfxVideoParam const & actual)
{
    EXPECT_EQ(expected.mfx.CodecProfile, actual.mfx.CodecProfile);
    EXPECT_EQ(expected.mfx.FrameInfo.BitDepthLuma, actual.mfx.FrameInfo.BitDepthLuma);
    EXPECT_EQ(expected.mfx.FrameInfo.BitDepthChroma, actual.mfx.FrameInfo.BitDepthChroma);
    EXPECT_EQ(expected.mfx.FrameInfo.FourCC, actual.mfx.FrameInfo.FourCC);
    EXPECT_EQ(expected.mfx.FrameInfo.PicStruct, actual.mfx.FrameInfo.PicStruct);
    EXPECT_EQ(expected.mfx.FrameInfo.ChromaFormat, actual.mfx.FrameInfo.ChromaFormat);
    EXPECT_EQ(expected.mfx.FrameInfo.Width, actual.mfx.FrameInfo.Width);
    EXPECT_EQ(expected.mfx.FrameInfo.Height, actual.mfx.FrameInfo.Height);

    mfxVideoParam tmp = expected;

    tmp.mfx.FrameInfo = actual.mfx.FrameInfo;
    tmp.mfx.CodecProfile = actual.mfx.CodecProfile;
    tmp.mfx.CodecLevel = actual.mfx.CodecLevel;
    tmp.mfx.DecodedOrder = actual.mfx.DecodedOrder;
    tmp.mfx.MaxDecFrameBuffering = actual.mfx.MaxDecFrameBuffering;

    if (memcmp(&actual, &tmp, sizeof(mfxVideoParam)))
    {
        ADD_FAILURE() << "FAILED: DecodeHeader should not change fields other than FrameInfo, CodecProfile, CodecLevel, DecodedOrder";
    }
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    auto tc = test_case[id];
    auto name = g_tsStreamPool.Get(tc.stream, path);
    g_tsStreamPool.Reg();

    tsBitstreamReaderIVF reader(name, 100000);
    m_bs_processor = &reader;
    m_pBitstream = m_bs_processor->ProcessBitstream(m_bitstream);

    memset(m_pPar, 0xF0, sizeof(mfxVideoParam));
    m_pPar->mfx.CodecLevel = 0;
    m_pPar->mfx.CodecId = MFX_CODEC_AV1;
    m_par.RefreshBuffers();

    MFXInit();
    if (m_uid)
    {
        Load();
    }

    apply_par(tc);
    mfxVideoParam expectedPar = m_par;

    g_tsStatus.expect(tc.sts);
    DecodeHeader(m_session, m_pBitstream, m_pPar);

    if(g_tsStatus.get() >= 0)
        CheckParams(expectedPar, m_par);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(av1d_decode_header);
}