/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*
    Test verifies that application-defined weighted prediction table is written
    in bitstream and there's no crash in encoding. Test can't verify that app WP parameters
    applied by encoder.
*/

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <random>
#include <algorithm>

#define DEBUG

template<class T> inline T Clip3(T min, T max, T x) { return std::min<T>(std::max<T>(min, x), max); }

namespace hevce_explicit_weight_pred
{
    using namespace BS_HEVC;

    const mfxU8 L0 = 0, L1 = 1, Y = 0, Cb = 1, Cr = 2, Weight = 0, Offset = 1;
    const mfxU16 nframes_to_encode = 10;

    enum
    {
        MFX_PAR = 1,
        EXT_COD3,
        MAX_ACT_REF = 4
    };
    enum
    {
        PREDEFINED  = 1,
        RAND        = 1 << 1
    };

    class TestSuite : public tsVideoEncoder, tsSurfaceProcessor, tsBitstreamProcessor, tsParserHEVC2
    {
    private:
        struct tc_struct
        {
            mfxU8 mode;
            struct
            {
                mfxU8 luma_denom;
                mfxI16 w_luma;
                mfxI16 o_luma;
            };
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };
        static const tc_struct test_case[];


        mfxU8  m_mode;
        mfxU8  m_def_luma_denom;
        mfxI16 m_def_w_luma;
        mfxI16 m_def_o_luma;

        mfxU8 max_wp_l0;
        mfxU8 max_wp_l1;

        std::mt19937 m_Gen;
        std::unique_ptr <tsRawReader> m_reader;
        std::vector <tsExtBufType<mfxEncodeCtrl>> m_ctrls;
#ifdef DEBUG
        tsBitstreamWriter m_writer;
#endif

    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC2()
            , m_mode(RAND)
#ifdef DEBUG
            , m_writer("debug_wp.bin")
#endif
        {
            m_filler = this;
            m_bs_processor = this;

            m_par.AsyncDepth = 1;
            m_par.mfx.GopPicSize = 32;
            m_par.mfx.GopRefDist = 4;
            m_par.mfx.NumRefFrame = 8;

            mfxExtCodingOption3& cod3 = m_par;
            cod3.WeightedPred = MFX_WEIGHTED_PRED_EXPLICIT;
            cod3.WeightedBiPred = MFX_WEIGHTED_PRED_EXPLICIT;

            m_ctrls.resize(2 * nframes_to_encode);

            m_Gen.seed(0);
        }

        ~TestSuite()
        {}

        static const unsigned int n_cases;
        template<mfxU32 fourcc>
        int RunTest(unsigned int id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxI32    GetRandomNumber(mfxI32 min, mfxI32 max);
        void      GeneratePredTable(mfxExtPredWeightTable& pwt, mfxU8 num_l0, mfxU8 num_l1, bool gpb);
        void      HardcodeRefLists(mfxExtHEVCRefLists& rl, mfxI32 fo);
    };

    // set ref lists for GopPicSize=32, GopRefDist=4, NumRefFrame=8
    // maximum number of active references can be 4/2,
    // number is adjusted according to MSDK settings
    void TestSuite::HardcodeRefLists(mfxExtHEVCRefLists& rl, mfxI32 fo)
    {
        mfxExtHEVCRefLists::mfxRefPic def_ref;
        def_ref.FrameOrder = MFX_FRAMEORDER_UNKNOWN;
        def_ref.PicStruct = MFX_PICSTRUCT_UNKNOWN;
        memset(def_ref.reserved, 0, sizeof(def_ref.reserved));
        std::fill_n(rl.RefPicList0, 32, def_ref);
        std::fill_n(rl.RefPicList1, 32, def_ref);

        // description for reference frames in minigop
        // described as distance between reference frame and current frame starting from closest
        // 0 means no reference frame
        // first 4 values for l0, the following 2 values for l1
        const mfxI32 ref_dist4[4][6] = { {-4, -6, -8, -10, -4, -6},
                                         {-1, -3, -5, -7,   1,  3},
                                         {-2, -4, -6, -8,   2,  0},
                                         {-1, -3, -5, -7,   1,  0}
                                        };
        const mfxU8 end_l0 = 4;
        const mfxU8 end_l1 = 6;

        mfxExtCodingOption3& cod3 = m_par;
        mfxU16 max_l0 = cod3.NumRefActiveP[0];
        mfxU16 max_l1 = cod3.NumRefActiveBL1[1];
        bool pframe = !!(fo % m_par.mfx.GopRefDist == 0);
        bool gpb = !!(pframe && cod3.GPB != MFX_CODINGOPTION_OFF);

        mfxU8 pl_idx = fo % m_par.mfx.GopRefDist;

        for (mfxU8 ref = 0; ref < max_l0; ref++)
        {
            if (ref < end_l0 && (fo + ref_dist4[pl_idx][ref] >= 0) && (ref_dist4[pl_idx][ref] != 0))
            {
                rl.RefPicList0[ref].FrameOrder = fo + ref_dist4[pl_idx][ref];
                rl.NumRefIdxL0Active += 1;
            }
        }

        if (gpb || !pframe)
        {
            for (mfxU8 ref = 0; ref < max_l1; ref++)
            {
                if (ref < end_l1 && (fo + ref_dist4[pl_idx][ref + end_l0] >= 0) && (ref_dist4[pl_idx][ref + end_l0] != 0))
                {
                    rl.RefPicList1[ref].FrameOrder = fo + ref_dist4[pl_idx][ref + end_l0];
                    rl.NumRefIdxL1Active += 1;
                }
            }
        }
    }

    mfxI32 TestSuite::GetRandomNumber(mfxI32 min, mfxI32 max)
    {
        std::uniform_int_distribution<mfxI32> distr(min, max);
        return distr(m_Gen);
    }

    void TestSuite::GeneratePredTable(mfxExtPredWeightTable& pwt, mfxU8 num_l0, mfxU8 num_l1, bool gpb)
    {
        if (m_mode & PREDEFINED)
        {
            pwt.LumaLog2WeightDenom = m_def_luma_denom;
            for (mfxU8 ref = 0; ref < std::min(max_wp_l0, num_l0); ref++)
            {
                pwt.LumaWeightFlag[L0][ref] = 1;
                pwt.Weights[L0][ref][Y][Weight] = m_def_w_luma;
                pwt.Weights[L0][ref][Y][Offset] = m_def_o_luma;
            }
            for (mfxU8 ref = 0; ref < std::min(max_wp_l1, num_l1); ref++)
            {
                if (gpb)
                {
                    pwt.LumaWeightFlag[L1][ref] = pwt.LumaWeightFlag[L0][ref];
                    pwt.Weights[L1][ref][Y][Weight] = pwt.Weights[L0][ref][Y][Weight];
                    pwt.Weights[L1][ref][Y][Offset] = pwt.Weights[L0][ref][Y][Offset];
                }
                else
                {
                    pwt.LumaWeightFlag[L1][ref] = 1;
                    pwt.Weights[L1][ref][Y][Weight] = m_def_w_luma;
                    pwt.Weights[L1][ref][Y][Offset] = m_def_o_luma;
                }
            }
            return;
        }

        // driver's limitation: kernel in VME HEVC for non-ATS platforms has hard-coded luma denom=6, so others are not allowed, VDEnc - ?
        if (g_tsHWtype == MFX_HW_ATS)
            pwt.LumaLog2WeightDenom = GetRandomNumber(1, 7);
        else
            pwt.LumaLog2WeightDenom = 6;
        mfxI16 wY = 1 << pwt.LumaLog2WeightDenom;

        // HEVC encoder writes not weights itself, but delta = weight - (1 << luma_denom) and delta
        // should be in range [-128, 127]. See HEVC specification for details
        mfxI16 minYWeight = -128 + wY;
        mfxI16 maxYWeight = 127 + wY;
        // MSDK doesn't set high_precision_offsets_enabled_flag so 8-bit precision for offsets
        // for any formats. TODO: change 8 on BitDepth if will be enabled.
        mfxI16 WpOffsetHalfRangeY = 1 << (8 - 1);
        mfxI16 minOffsetY = -1 * WpOffsetHalfRangeY;
        mfxI16 maxOffsetY = WpOffsetHalfRangeY - 1;

        for (mfxU8 ref = 0; ref < std::min(max_wp_l0, num_l0); ref++)
        {
            pwt.LumaWeightFlag[L0][ref] = GetRandomNumber(0, 1);
            if (pwt.LumaWeightFlag[L0][ref] == 1)
            {
                pwt.Weights[L0][ref][Y][Weight] = GetRandomNumber(minYWeight, maxYWeight);
                pwt.Weights[L0][ref][Y][Offset] = GetRandomNumber(minOffsetY, maxOffsetY);
            }
            else
            {
                pwt.Weights[L0][ref][Y][Weight] = 0;
                pwt.Weights[L0][ref][Y][Offset] = 0;
            }
        }
        for (mfxU8 ref = 0; ref < std::min(max_wp_l1, num_l1); ref++)
        {
            if (gpb)
            {
                pwt.LumaWeightFlag[L1][ref] = pwt.LumaWeightFlag[L0][ref];
                pwt.Weights[L1][ref][Y][Weight] = pwt.Weights[L0][ref][Y][Weight];
                pwt.Weights[L1][ref][Y][Offset] = pwt.Weights[L0][ref][Y][Offset];
            }
            else
            {
                pwt.LumaWeightFlag[L1][ref] = GetRandomNumber(0, 1);
                if (pwt.LumaWeightFlag[L1][ref] == 1)
                {
                    pwt.Weights[L1][ref][Y][Weight] = GetRandomNumber(minYWeight, maxYWeight);
                    pwt.Weights[L1][ref][Y][Offset] = GetRandomNumber(minOffsetY, maxOffsetY);
                }
                else
                {
                    pwt.Weights[L1][ref][Y][Weight] = 0;
                    pwt.Weights[L1][ref][Y][Offset] = 0;
                }
            }
        }
    }

    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        s.Data.TimeStamp = s.Data.FrameOrder;


        if (s.Data.FrameOrder % m_par.mfx.GopPicSize != 0)
        {
            mfxExtPredWeightTable& pwt = m_ctrls[s.Data.FrameOrder];
            mfxExtCodingOption3& cod3 = m_par;
            bool gpb = !!(s.Data.FrameOrder % m_par.mfx.GopRefDist == 0 && cod3.GPB != MFX_CODINGOPTION_OFF);
            mfxExtHEVCRefLists& ref_lists = m_ctrls[s.Data.FrameOrder];
            HardcodeRefLists(ref_lists, s.Data.FrameOrder);
            GeneratePredTable(pwt, ref_lists.NumRefIdxL0Active, ref_lists.NumRefIdxL1Active, gpb);

 #ifdef DEBUG
             printf("Frame #%d\n", s.Data.FrameOrder);
             printf("LumaLog2Denom %d\n", pwt.LumaLog2WeightDenom);
             printf("ChromaLog2Denom %d\n", pwt.ChromaLog2WeightDenom);
             printf("L0:\n");
             for (mfxU8 ref = 0; ref < 4; ref++)
             {
                 printf("  %d(%d): WY=%d, OY=%d, WCb=%d, OCb=%d, WCr=%d, OCr=%d\n", ref, pwt.LumaWeightFlag[L0][ref],
                         pwt.Weights[L0][ref][Y][Weight], pwt.Weights[L0][ref][Y][Offset],
                         pwt.Weights[L0][ref][Cb][Weight], pwt.Weights[L0][ref][Cb][Offset],
                         pwt.Weights[L0][ref][Cr][Weight], pwt.Weights[L0][ref][Cr][Offset]);
             }
             printf("L1:\n");
             for (mfxU8 ref = 0; ref < 2; ref++)
             {
                 printf("  %d(%d): WY=%d, OY=%d, WCb=%d, OCb=%d, WCr=%d, OCr=%d\n", ref, pwt.LumaWeightFlag[L1][ref],
                         pwt.Weights[L1][ref][Y][Weight], pwt.Weights[L1][ref][Y][Offset],
                         pwt.Weights[L1][ref][Cb][Weight], pwt.Weights[L1][ref][Cb][Offset],
                         pwt.Weights[L1][ref][Cr][Weight], pwt.Weights[L1][ref][Cr][Offset]);
             }
 #endif
         }

        m_pCtrl = &m_ctrls[s.Data.FrameOrder];

        return sts;
    }


    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxStatus sts = MFX_ERR_NONE;
        SetBuffer0(bs);

#ifdef DEBUG
        sts = m_writer.ProcessBitstream(bs, nFrames);
#endif

        g_tsLog << "Check frame #" << bs.TimeStamp << "\n";
        mfxExtPredWeightTable& exp_pwt = m_ctrls[bs.TimeStamp];
        mfxU32 checked = 0;
        while (checked++ < nFrames)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                if (pNALU->slice == nullptr)
                {
                    g_tsLog << "ERROR: pNALU->slice is null in BS parsing\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }

                auto& slice = *pNALU->slice;

                if (slice.type == I)
                    continue;

                if (slice.pps != nullptr)
                {
                    EXPECT_EQ((mfxU8)1, slice.pps->weighted_pred_flag);
                    EXPECT_EQ((mfxU8)1, slice.pps->weighted_bipred_flag);
                }

                if (slice.pwt == nullptr)
                {
                    g_tsLog << "ERROR: predWeightTable is null in BS parsing\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }

                auto pwt  = slice.pwt;

                EXPECT_EQ(exp_pwt.LumaLog2WeightDenom, slice.luma_log2_weight_denom);
                EXPECT_EQ(exp_pwt.ChromaLog2WeightDenom, slice.chroma_log2_weight_denom);

                for (mfxU8 lx = 0; lx < (slice.type == B ? 2 : 1); lx++) // L0, L1
                {
                    for (mfxU8 ref = 0; ref < (lx ? slice.num_ref_idx_l1_active : slice.num_ref_idx_l0_active); ref++)
                    {
                        g_tsLog << "Check List" << lx << " for reference #" << ref << "\n";
                        if (exp_pwt.LumaWeightFlag[lx][ref])
                        {
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Y][Weight], pwt[lx][ref][Y][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Y][Offset], pwt[lx][ref][Y][Offset]);
                        }
                        else
                        {
                            EXPECT_EQ(1 << slice.luma_log2_weight_denom, pwt[lx][ref][Y][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Y][Offset]);
                        }
                        if (exp_pwt.ChromaWeightFlag[lx][ref])
                        {
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Cb][Weight], pwt[lx][ref][Cb][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Cb][Offset], pwt[lx][ref][Cb][Offset]);
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Cr][Weight], pwt[lx][ref][Cr][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[lx][ref][Cr][Offset], pwt[lx][ref][Cr][Offset]);
                        }
                        else
                        {
                            EXPECT_EQ(1 << slice.chroma_log2_weight_denom, pwt[lx][ref][Cb][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Cb][Offset]);
                            EXPECT_EQ(1 << slice.chroma_log2_weight_denom, pwt[lx][ref][Cr][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Cr][Offset]);
                        }
                    } // for ref
                } // for lx

            }
        }

        bs.DataLength = 0;
        return sts;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ PREDEFINED, {6, 60, -128}, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4}}
        },
        {/*01*/ PREDEFINED, {6, -34, 127}, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 1}}
        },
        {/*02*/ RAND, {0, 0, 0}, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4}}
        },
        {/*03*/ PREDEFINED, {6, 191, 0}, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, 4},
                                           {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB, MFX_CODINGOPTION_OFF} }
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    struct StreamDescr
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
        mfxU16 chroma_format;
        mfxU16 bd;
        mfxU16 shift;
    };

    const StreamDescr streams[] = {
        { 352, 288, "forBehaviorTest/foreman_cif.nv12",                   MFX_CHROMAFORMAT_YUV420, 8,  0},
        { 352, 288, "YUV8bit422/Kimono1_352x288_24_yuy2.yuv",             MFX_CHROMAFORMAT_YUV422, 8,  0},
        { 352, 288, "YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv", MFX_CHROMAFORMAT_YUV420, 10, 1},
        { 352, 288, "YUV10bit422/Kimono1_352x288_24_y210.yuv",            MFX_CHROMAFORMAT_YUV422, 10, 1},
        { 176, 144, "YUV16bit420/FruitStall_176x144_240_p016.yuv",        MFX_CHROMAFORMAT_YUV420, 12, 1},
        { 176, 144, "YUV16bit422/FruitStall_176x144_240_y216.yuv",        MFX_CHROMAFORMAT_YUV422, 12, 1},
    };

    const StreamDescr& get_stream_descr(const mfxU32& fourcc)
    {
        switch(fourcc)
        {
            case MFX_FOURCC_NV12:   return streams[0];
            case MFX_FOURCC_YUY2:   return streams[1];
            case MFX_FOURCC_P010:   return streams[2];
            case MFX_FOURCC_Y210:   return streams[3];
            case GMOCK_FOURCC_P012: return streams[4];
            case GMOCK_FOURCC_Y212: return streams[5];

            default: assert(0); return streams[0];
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        m_mode = tc.mode;
        m_def_luma_denom = tc.luma_denom;
        m_def_w_luma = tc.w_luma;
        m_def_o_luma = tc.o_luma;

        SETPARS(&m_par, MFX_PAR);
        mfxExtCodingOption3& cod3 = m_par;
        SETPARS(&cod3, EXT_COD3);

        MFXInit();

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 caps_size = sizeof(ENCODE_CAPS_HEVC);
        g_tsStatus.check(GetCaps(&caps, &caps_size));

        max_wp_l0 = caps.MaxNum_WeightedPredL0 ? caps.MaxNum_WeightedPredL0 : 1;
        max_wp_l1 = caps.MaxNum_WeightedPredL1 ? caps.MaxNum_WeightedPredL1 : 1;

        if (cod3.GPB == MFX_CODINGOPTION_OFF && (g_tsHWtype > MFX_HW_ICL || m_par.mfx.LowPower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: HEVCe does not support P-slices on VDEnc and after ICL!\n\n\n";
            throw tsSKIP;
        }

        const StreamDescr& yuv_descr = get_stream_descr(fourcc);
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = yuv_descr.w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = yuv_descr.h;
        m_par.mfx.FrameInfo.ChromaFormat = yuv_descr.chroma_format;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = yuv_descr.bd;
        m_par.mfx.FrameInfo.Shift = yuv_descr.shift;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26 + (m_par.mfx.FrameInfo.BitDepthLuma - 8) * 6;
        switch(fourcc)
        {
            case GMOCK_FOURCC_P012: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016; break;
            case GMOCK_FOURCC_Y212: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216; break;
            case GMOCK_FOURCC_Y412: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416; break;

            default: m_par.mfx.FrameInfo.FourCC = fourcc;
        }

        auto stream = g_tsStreamPool.Get(yuv_descr.name);
        m_reader.reset(new tsRawReader(stream, m_par.mfx.FrameInfo));
        m_reader->m_disable_shift_hack = true; // disable shift for 10b+ streams
        g_tsStreamPool.Reg();

        Init();
        EncodeFrames(nframes_to_encode);
        DrainEncodedBitstream();

        Close(); // encode

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_explicit_weight_pred,  RunTest<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_explicit_weight_pred, RunTest<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_explicit_weight_pred, RunTest<GMOCK_FOURCC_P012>, n_cases);

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_explicit_weight_pred,  RunTest<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_explicit_weight_pred, RunTest<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_explicit_weight_pred, RunTest<GMOCK_FOURCC_Y212>, n_cases);
};
