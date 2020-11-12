/******************************************************************************* *\

Copyright (C) 2016-2018 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
[MQ1220] - test for slice size in LCU for HEVCe HW plugin.
    mfxExtCodingOption2.MumMbPerSlice is not supported configuration during runtime,
    only init/reset.
*/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_co2_max_slice_size_in_ctu
{

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVC2
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVC2(BS_HEVC2::PARSE_SSD)
        , m_expected(0)
        , m_nFrame(0)
        , framesToEncode(0)
    {
        srand((mfxU32)time(NULL));
        m_bs_processor = this;
        m_filler = this;
    }

    ~TestSuite()
    {
    }

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        char* skips;
    };

    template<mfxU32 fourcc>
    int RunTest_Subtype(const unsigned int id);

    int RunTest(tc_struct tc, unsigned int fourcc_id);
    static const unsigned int n_cases;

private:
    mfxU16 m_expected;
    mfxU32 m_nFrame;
    mfxU32 m_numLCU; //LCU number in frame
    mfxU32 framesToEncode;

    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
        INIT_ONLY
    };

    enum
    {
        INIT    = 1 << 1,       //to set on initialization
        RESET   = 1 << 2,
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        while (nFrames--)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                if (m_expected == 0) {
                    //not set, so expected value is m_numLCU
                    m_expected = m_numLCU;
                }

                auto& S = *pNALU->slice;
                if (S.NumCTU != m_expected) {
                    if (S.NumCTU > m_expected)
                    {
                        g_tsLog << "ERROR: Slice's CTU num exceeds expected value.\n"
                            << "frame#" << m_nFrame <<": Slice number of CTUs = " << (mfxU32)S.NumCTU
                            << " > " << (mfxU32)m_expected << " (expected value).\n";
                        return MFX_ERR_ABORTED;
                    } else
                    if (pNALU->next)
                    {
                        g_tsLog << "ERROR: Slice's CTU num is not as expected and it is not the last slice.\n"
                            << "frame#" << m_nFrame <<": Slice number of CTUs = " << (mfxU32)S.NumCTU
                            << " != " << (mfxU32)m_expected << " (expected value).\n";
                        return MFX_ERR_ABORTED;
                    }
                }
                m_nFrame++;
            }
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //If NumMbPerSlice is set > LTU_size_in_frame return WRN_INCOMPATIBLE_VIDEO_PARAM
    /*00*/{MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT_ONLY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0xFFFF}},
    /*01*/{MFX_ERR_NONE, 0, {}},
    /*02*/{MFX_ERR_NONE, INIT, {}},
    /*03*/{MFX_ERR_NONE, RESET, {}},
    /*04*/{MFX_ERR_NONE, RESET|INIT, {}},
    /*05*/{MFX_ERR_NONE, INIT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*06*/{MFX_ERR_NONE, INIT|RESET, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                      {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
    /*07*/{MFX_ERR_NONE, INIT|RESET, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                      {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}},
    /*08*/{MFX_ERR_NONE, INIT|RESET, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

template<mfxU32 fourcc>
int TestSuite::RunTest_Subtype(const unsigned int id)
{
    const tc_struct& tc = test_case[id];
    return RunTest(tc, fourcc);
}

int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
{
    int err = 0;
    TS_START;

    if ((0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))))
    {
        if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            Query();
            return 0;
        }
        if ((fourcc_id == MFX_FOURCC_P010)
            && (g_tsHWtype < MFX_HW_KBL)) {
            g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
            fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
            && (g_tsHWtype < MFX_HW_ICL))
        {
            g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == GMOCK_FOURCC_P012 || fourcc_id == GMOCK_FOURCC_Y212 || fourcc_id == GMOCK_FOURCC_Y412)
            && (g_tsHWtype < MFX_HW_TGL))
        {
            g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_YUY2 || fourcc_id == MFX_FOURCC_Y210 || fourcc_id == GMOCK_FOURCC_Y212)
            && (g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 4:2:2 formats are NOT supported on VDENC!\n\n\n";
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == GMOCK_FOURCC_Y412)
            && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
            throw tsSKIP;
        }
    }
    else {
        g_tsLog << "WARNING: only HEVCe HW plugin is tested\n";
        return 0;
    }

    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 704;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 576;

    //This calculation is based on SKL limitation that LCU is always 32x32.
    //If LCU can be configured, the calcuation should be updated with the
    //following values configured at initialization.
    //sps.log2_min_luma_coding_block_size_minus3
    //sps.log2_diff_max_min_luma_coding_block_size
    mfxU32 LCUSize = 32;
    if (g_tsHWtype >= MFX_HW_CNL)
        LCUSize = 64;
    mfxU32 widthLCU  = (m_par.mfx.FrameInfo.CropW + (LCUSize - 1)) / LCUSize;
    mfxU32 heightLCU = (m_par.mfx.FrameInfo.CropH + (LCUSize - 1)) / LCUSize;
    m_numLCU = heightLCU * widthLCU;

    MFXInit();

    SETPARS(m_pPar, MFX_PAR);

    if (fourcc_id == MFX_FOURCC_NV12)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_P010)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_P012)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else if (fourcc_id == MFX_FOURCC_YUY2)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y210)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_Y212)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        m_par.mfx.FrameInfo.Shift = 1;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else if (fourcc_id == MFX_FOURCC_AYUV)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
    }
    else if (fourcc_id == MFX_FOURCC_Y410)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.Shift = 0;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
    }
    else if (fourcc_id == GMOCK_FOURCC_Y412)
    {
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;
    }
    else
    {
        g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
        return 0;
    }

    if (tc.mode == INIT_ONLY) {
        mfxExtCodingOption2& cod2 = m_par;
        SETPARS(&cod2, EXT_COD2);
    } else if (tc.mode & INIT) {
        mfxExtCodingOption2& cod2 = m_par;
        if (g_tsHWtype >= MFX_HW_CNL)   // row aligned
            cod2.NumMbPerSlice = (rand() % heightLCU) * widthLCU;
        else
            cod2.NumMbPerSlice = rand() % m_numLCU;
        m_expected = cod2.NumMbPerSlice;
    }

    //load the plugin in advance.
    if(!m_loaded)
    {
        Load();
    }

    g_tsStatus.expect(tc.sts);
    Init();

    if (tc.mode == INIT_ONLY) {
        throw tsOK;
    }

    framesToEncode = (g_tsConfig.sim) ? 3 : 20;

    if (tc.sts == MFX_ERR_NONE) {
        m_max = framesToEncode;
        m_cur = 0;
        EncodeFrames(framesToEncode);

        if (tc.mode & RESET) {
            mfxExtCodingOption2& cod2 = m_par;
            if (g_tsHWtype >= MFX_HW_CNL)   // row aligned
                cod2.NumMbPerSlice = (rand() % heightLCU + 1) * widthLCU;
            else
                cod2.NumMbPerSlice = rand() % m_numLCU + 1;

            if ((m_par.mfx.NumSlice != 0)
                && (m_par.mfx.NumSlice != CEIL_DIV(m_numLCU, cod2.NumMbPerSlice))) // check alignment. if not aligned - expect WRN
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            m_expected = cod2.NumMbPerSlice;
            Reset();
            m_max = framesToEncode;
            m_cur = 0;
            EncodeFrames(framesToEncode);
        }
    }

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_co2_max_slice_size_in_ctu, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_co2_max_slice_size_in_ctu, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_co2_max_slice_size_in_ctu, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_co2_max_slice_size_in_ctu, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);

};
