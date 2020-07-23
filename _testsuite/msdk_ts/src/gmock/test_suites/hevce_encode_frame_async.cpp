/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2020 Intel Corporation. All Rights Reserved.

File Name: hevce_encode_frame_async.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_aux_dev.h"
#include <map>

namespace hevce_encode_frame_async
{
    // {CodecLevel, MaxNumSlices} values are from HEVC SPEC
    const std::map<int, int> LEVEL_LIMITS = {
        {MFX_LEVEL_HEVC_4, 75},
        {MFX_LEVEL_HEVC_5, 200},
        {MFX_LEVEL_HEVC_6, 600}
    };

    class TestSuite : tsVideoEncoder
    {
    public:
        static const unsigned int n_cases;

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            mfxU32 sub_type;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxI32 v;
            } set_par[MAX_NPARS];
        };

        mfxU32 getRawBytes(const mfxU16& target_chroma)
        {
            mfxU64 s = 0;
            s = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height;
            switch (target_chroma)
            {
            case MFX_CHROMAFORMAT_YUV420:
                s = s * 3 / 2;
                break;
            case MFX_CHROMAFORMAT_YUV422:
                s *= 2;
                break;
            case MFX_CHROMAFORMAT_YUV444:
                s *= 3;
                break;
            default: break;
            }
            s = (s * m_par.mfx.FrameInfo.BitDepthLuma + 7) / 8;
            return s;
        }

        mfxU32 getBufferSizeInKb(const mfxU16& target_chroma)
        {
            mfxU32 expectedBufferSizeInKB = 0;
            bool bUseMaxKbps =
                m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
                || m_par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR
                || m_par.mfx.RateControlMethod == MFX_RATECONTROL_VCM
                || m_par.mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT;

            if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || bUseMaxKbps)
            {
                if (m_par.mfx.BRCParamMultiplier == 0) m_par.mfx.BRCParamMultiplier = 1;
                mfxU32 maxKbps = 0;
                if (m_par.mfx.MaxKbps && m_par.mfx.RateControlMethod != MFX_RATECONTROL_CBR)
                    maxKbps = m_par.mfx.MaxKbps * m_par.mfx.BRCParamMultiplier;
                else if (m_par.mfx.TargetKbps == 0)
                    //150 is an average compression ratio of the encoded stream to the origin in HEVC
                    //It is based on the theoretical effectiveness of HEVC
                    maxKbps = getRawBytes(target_chroma) * 8 * m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD / 150000;
                else
                    maxKbps = m_par.mfx.TargetKbps * m_par.mfx.BRCParamMultiplier;

                expectedBufferSizeInKB = maxKbps / 4; //2 sec: the same as H264
            }
            else
            {
                expectedBufferSizeInKB = getRawBytes(target_chroma) / 1000;
            }
            if (m_par.mfx.InitialDelayInKB && (bUseMaxKbps || m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR))
                expectedBufferSizeInKB = std::max<mfxU32>(expectedBufferSizeInKB, m_par.mfx.InitialDelayInKB * m_par.mfx.BRCParamMultiplier);
            return expectedBufferSizeInKB;
        }

        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        enum
        {
            MFX_PAR,
            MFX_BS,
            MFX_SURF,
            NULL_SESSION,
            NULL_BS,
            NULL_SURF,
            NOT_INIT,
            FAILED_INIT,
            CLOSED,
            CROP_XY,
            NSLICE_EQ_MAX,
            NSLICE_GT_MAX,
            NSLICE_LT_MAX,
            DELTA,
            PROFILE,
            BUFFER_SIZE,
            EQ_MAXLENGTH,
            L_MAXLENGTH,
            G_MAXLENGTH,
            NONE
        };

        static const tc_struct test_case[];
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION, NONE },
        {/*01*/ MFX_ERR_NULL_PTR, NULL_BS, NONE },
        {/*02*/ MFX_ERR_MORE_DATA, NULL_SURF, NONE },
        {/*03*/ MFX_ERR_NOT_INITIALIZED, NOT_INIT, NONE },
        {/*04*/ MFX_ERR_NOT_INITIALIZED, FAILED_INIT, NONE },
        {/*05*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 10 }
        },
        {/*06*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE, NONE,
            { MFX_BS, &tsStruct::mfxBitstream.DataOffset, (mfxI32)0xFFFFFFFF }
        },
        {/*07*/ MFX_ERR_NOT_ENOUGH_BUFFER, NONE, NONE,
            {
                { MFX_BS, &tsStruct::mfxBitstream.MaxLength, 0 },
                { MFX_BS, &tsStruct::mfxBitstream.Data, 0 }
            }
        },
        {/*08*/ MFX_ERR_UNDEFINED_BEHAVIOR, NONE, NONE,
            { MFX_SURF, &tsStruct::mfxFrameSurface1.Data.Y, 0 }
        },
        {/*09*/ MFX_ERR_NONE, NONE, NONE },
        {/*10*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 }
            }
        },
        {/*11*/ MFX_ERR_NONE, NONE, NONE,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 80 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 33 }
            }
        },

        {/*12*/ MFX_ERR_NONE, NSLICE_GT_MAX, NONE },  // NumSlice > MAX
        {/*13*/ MFX_ERR_NONE, NSLICE_LT_MAX, NONE },  // NumSlice < MAX
        {/*14*/ MFX_ERR_NONE, NSLICE_EQ_MAX, NONE },  // NumSlice == MAX
        {/*15*/ MFX_ERR_NONE, CROP_XY, DELTA,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, -1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropX, 0 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropY, 1 },
            }
        },
        {/*16*/ MFX_ERR_NONE, PROFILE, NONE,    // profile and level are set explicitely
        },
        {/*17*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },}
        },
        {/*18*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },}
        },
        {/*19*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },}
        },
        {/*20*/ MFX_ERR_NONE, BUFFER_SIZE, L_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },}
        },
        {/*21*/ MFX_ERR_NONE, BUFFER_SIZE, L_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },}
        },
        {/*22*/ MFX_ERR_NONE, BUFFER_SIZE, L_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },}
        },
        {/*23*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },}
        },
        {/*24*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },}
        },
        {/*25*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },}
        },
        {/*26*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*27*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*28*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*29*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*30*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*31*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 9 },}
        },
        {/*32*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        },
        {/*33*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        },
        {/*34*/ MFX_ERR_NONE, BUFFER_SIZE, EQ_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        },
        {/*35*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        },
        {/*36*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        },
        {/*37*/ MFX_ERR_NOT_ENOUGH_BUFFER, BUFFER_SIZE, G_MAXLENGTH, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 0 },}
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        const char* stream = nullptr;
        tsSurfaceProcessor *reader;
        mfxStatus sts;
        bool run_encode = true;

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
            if (fourcc_id == GMOCK_FOURCC_Y212 && g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
            {
                g_tsLog << "\n\nWARNING: Y212 format is NOT supported on VDENC!\n\n\n";
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
            if (tc.type == CROP_XY)
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }

        if (tc.set_par->ext_type == MFX_SURF && fourcc_id == MFX_FOURCC_Y410)
        {
            if (tc.set_par->f == &tsStruct::mfxFrameSurface1.Data.Y)
            {
                g_tsLog << "\n\nWARNING: Skipped, Y410 surfaces do not use Y pointer\n\n\n";
                throw tsSKIP;
            }
        }

        //set default param
        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            if (g_tsConfig.sim) {
                stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else {
                stream = g_tsStreamPool.Get("YUV/720x480p_30.00_4mb_h264_cabac_180s.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
            }
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            if (g_tsConfig.sim) {
                stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_176x144_24_p010_shifted.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else {
                stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
            }
        }
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_176x144_240_p016.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_176x144_24_yuy2.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_176x144_24_y210.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_176x144_240_y216.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_176x144_24_ayuv.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_176x144_24_y410.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_176x144_240_y416.yuv");

            m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        g_tsStreamPool.Reg();

        MFXInit();
        Load();

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        //set handle
        if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }

        if (tc.sub_type != DELTA) {
            SETPARS(m_pPar, MFX_PAR);
        }
        else {  // params dependend on current resolution
            for (mfxU32 i = 0; i < MAX_NPARS; i++) {
                if (tc.set_par[i].f && tc.set_par[i].ext_type == MFX_PAR) {
                    if (tc.type == CROP_XY)
                    {
                        if (tc.set_par[i].f->name.find("CropX") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropX += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropY") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropY += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropW") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropW += tc.set_par[i].v;
                        if (tc.set_par[i].f->name.find("CropH") != std::string::npos)
                            m_pPar->mfx.FrameInfo.CropH += tc.set_par[i].v;
                    }
                }
            }
        }

        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

        sts = GetCaps(&caps, &capSize);
        if (sts != MFX_ERR_UNSUPPORTED)
            g_tsStatus.check(sts);

        //g_tsLog << "CAPS: LCUSizeSupported = " << caps.LCUSizeSupported << "\n";
        //g_tsLog << "CAPS: SliceStructure = " << caps.SliceStructure << "\n";
        //g_tsLog << "CAPS: BlockSize = " << (mfxU32)caps.BlockSize << "\n";
        //g_tsLog << "CAPS: ROICaps = " << (mfxU32)caps.ROICaps << "\n";

        //init
        if (tc.type == FAILED_INIT)
        {
            g_tsStatus.expect(MFX_ERR_NULL_PTR);
            Init(m_session, NULL);
        }
        else if (tc.type != NOT_INIT)
        {
            //check current supported MAX number of slices
            if (g_tsHWtype < MFX_HW_SKL)
            {
                m_pPar->mfx.CodecLevel = MFX_LEVEL_HEVC_4;
            }
            else if (g_tsHWtype < MFX_HW_ICL)
            {
                m_pPar->mfx.CodecLevel = MFX_LEVEL_HEVC_5;
            }
            else
            {
                m_pPar->mfx.CodecLevel = MFX_LEVEL_HEVC_6;
            }
            // without this params we can't check and correct codec level in init
            // CorrectLevel func doesn't check and correct level if frame rate or GOP structure is unspecified
            m_pPar->mfx.GopRefDist = 1;
            m_pPar->mfx.FrameInfo.FrameRateExtN = 30;
            m_pPar->mfx.FrameInfo.FrameRateExtD = 1;
            try
            {
                m_pPar->mfx.NumSlice = LEVEL_LIMITS.at(m_pPar->mfx.CodecLevel);
            }
            catch (std::out_of_range)
            {
                g_tsLog << "Error: Undefined NumSlice for codec level " << m_pPar->mfx.CodecLevel << "!\n";
                g_tsStatus.check(MFX_ERR_ABORTED);
            }
            g_tsStatus.disable_next_check();
            Query();

            if (tc.type == NSLICE_GT_MAX) {
                m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice + 1;
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            } else if (tc.type == NSLICE_LT_MAX) {
                /*
                In case of test type = NSLICE_LT_MAX, value for NumSlices was calculated like (maxSlices - 1),
                but it is incorrect for VDEnc (SliceStructure = 2), because for this encoder mode we can specify for NumSlice
                only those values, which are composed of any number of rows, but all must have the same size, except last one, which can be
                smaller or equal to previous slices. For example,if maxSlices = 8 (resolution is 720x480), we cannot specify NumSlice = 7
                to check test case NSLICE_LT_MAX. The most closed correct value, that is less than max value, will be 8/2 = 4.
                For odd values (i.e. maxSlices = 5) correct value for NumSlice will be the result of a similar division, but with rounding(5/2 ~= 3).
                Thus, it is most convenient to use CEIL_DIV.
                */
                if (caps.SliceStructure == 2 || (m_pPar->mfx.LowPower & MFX_CODINGOPTION_ON)) {
                    m_pPar->mfx.NumSlice = CEIL_DIV(m_pParOut->mfx.NumSlice, 2);
                } else {
                    m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice - 1;
                }
            } else if (tc.type == NSLICE_EQ_MAX) {
                m_pPar->mfx.NumSlice = m_pParOut->mfx.NumSlice;
            } else if (tc.type == PROFILE) {
                m_pPar->mfx.CodecLevel =  51;
                if (fourcc_id == MFX_FOURCC_NV12) {
                    m_pPar->mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
                } else
                if (fourcc_id == MFX_FOURCC_P010) {
                    m_pPar->mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN10;
                } else
                if ((fourcc_id == GMOCK_FOURCC_P012) ||
                    (fourcc_id == MFX_FOURCC_YUY2) ||
                    (fourcc_id == MFX_FOURCC_Y210) ||
                    (fourcc_id == GMOCK_FOURCC_Y212) ||
                    (fourcc_id == MFX_FOURCC_AYUV) ||
                    (fourcc_id == MFX_FOURCC_Y410) ||
                    (fourcc_id == GMOCK_FOURCC_Y412)) {
                    m_pPar->mfx.CodecProfile = MFX_PROFILE_HEVC_REXT;
                }
            }

            Init();

            mfxVideoParam get_par = {};
            GetVideoParam(m_session, &get_par);
            // we set max available codec level, check that wasn't increased
            if (m_pPar->mfx.CodecLevel < get_par.mfx.CodecLevel)
            {
                // to avoid futher encode, because new level aren't supported
                run_encode = false;
            }

            //set test param

            mfxU32 expectedBufferSizeInKB = 0;
            if (tc.type == BUFFER_SIZE)
            {
                expectedBufferSizeInKB = getBufferSizeInKb(m_par.mfx.FrameInfo.ChromaFormat);
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = 0;
                SETPARS(m_pPar, MFX_PAR);
            }
            AllocBitstream(); TS_CHECK_MFX;
            SETPARS(m_pBitstream, MFX_BS);
            if (tc.type == BUFFER_SIZE)
            {
                if (tc.sub_type == EQ_MAXLENGTH)
                    m_bitstream.MaxLength = m_bitstream.DataOffset + m_bitstream.DataLength + expectedBufferSizeInKB * 1000;
                else if (tc.sub_type == L_MAXLENGTH)
                    m_bitstream.MaxLength = m_bitstream.DataOffset + m_bitstream.DataLength + expectedBufferSizeInKB * 1000 + 1;
                else if (tc.sub_type == G_MAXLENGTH)
                    m_bitstream.MaxLength = m_bitstream.DataOffset + m_bitstream.DataLength + expectedBufferSizeInKB * 1000 - 1;
            }
            AllocSurfaces(); TS_CHECK_MFX;
            m_pSurf = GetSurface(); TS_CHECK_MFX;
            if (m_filler)
            {
                m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
            }
            SETPARS(m_pSurf, MFX_SURF);
        }

        if (tc.type == CLOSED)
        {
            Close();
        }

        if (run_encode)
        {
            //call test function
            if (tc.sts >= MFX_ERR_NONE)
            {
                int encoded = 0;
                while (encoded < 1)
                {
                    if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                    {
                        continue;
                    }

                    g_tsStatus.check(); TS_CHECK_MFX;
                    SyncOperation(); TS_CHECK_MFX;
                    encoded++;
                }
                sts = tc.sts;
            }
            else if (tc.type == NULL_SESSION)
            {
                sts = EncodeFrameAsync(NULL, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
            }
            else if (tc.type == NULL_BS)
            {
                sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, NULL, m_pSyncPoint);
            }
            else if (tc.type == NULL_SURF)
            {
                sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, NULL, m_pBitstream, m_pSyncPoint);
            }
            else
            {
                sts = EncodeFrameAsync(m_session, m_pSurf ? m_pCtrl : 0, m_pSurf, m_pBitstream, m_pSyncPoint);
            }
            g_tsStatus.expect(tc.sts);
            g_tsStatus.check(sts);
        }
        if (tc.sts != MFX_ERR_NOT_INITIALIZED)
            g_tsStatus.expect(MFX_ERR_NONE);
        else
            g_tsStatus.expect(MFX_ERR_NOT_INITIALIZED);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_encode_frame_async, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_encode_frame_async, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_encode_frame_async, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_encode_frame_async, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_encode_frame_async, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_encode_frame_async, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_encode_frame_async, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_encode_frame_async, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_encode_frame_async, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
};
