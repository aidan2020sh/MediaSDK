#include "ts_encoder.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace avce_constraint_vbr
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC, false) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        MFXPAR = 1,
        CODPAR,
        COD3PAR
    };

    enum
    {
        LA     = 0x000000001,
        LA_HRD = 0x000000010,

        SD_LOW  = 0x00000100,
        SD_HIGH = 0x00001000,
        HD_LOW  = 0x00010000,
        HD_HIGH = 0x00100000,

        WINDOW =  0x01000000
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // "Good"
    {/*00*/ MFX_ERR_NONE, LA|SD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|SD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|SD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|SD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|HD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|HD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_LOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|SD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|SD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|HD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|HD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_LOW|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_1}},
    {/*31*/ MFX_ERR_NONE, LA_HRD|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetUsage, MFX_TARGETUSAGE_4}},

    // Only one parameter for sliding window
    {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, LA|SD_LOW, {COD3PAR, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 2000}},
    {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, LA|SD_LOW, {COD3PAR, &tsStruct::mfxExtCodingOption3.WinBRCSize, 15}},
    {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, LA_HRD|SD_LOW, {COD3PAR, &tsStruct::mfxExtCodingOption3.WinBRCMaxAvgKbps, 2000}},
    {/*00*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, LA_HRD|SD_LOW, {COD3PAR, &tsStruct::mfxExtCodingOption3.WinBRCSize, 15}},

    // + BRCParamMultiplier
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 2}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.BRCParamMultiplier, 4}},

    // NalHrdConformance
    {/*38*/ MFX_ERR_NONE, LA|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_UNKNOWN}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_UNKNOWN}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_OFF}},
    {/*00*/ MFX_ERR_NONE, LA|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_OFF}},
    {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, LA|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_ON}},
    {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, LA|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_ON}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_UNKNOWN}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_UNKNOWN}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_ON}},
    {/*00*/ MFX_ERR_NONE, LA_HRD|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_ON}},
    {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, LA_HRD|HD_HIGH, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_OFF}},
    {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, LA_HRD|HD_HIGH|WINDOW, {CODPAR, &tsStruct::mfxExtCodingOption.NalHrdConformance, MFX_CODINGOPTION_OFF}},

    // Non LA BRC
    {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
    {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
    {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR}},
    {/*53*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, SD_HIGH|WINDOW, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_par.mfx.TargetUsage = MFX_TARGETUSAGE_1;

    if (tc.mode & LA)
    {
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_LA;
    }
    else if (tc.mode & LA_HRD)
    {
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_LA_HRD;
    }

    if (tc.mode & SD_LOW)
    {
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 576;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.TargetKbps = 1300;
        m_par.mfx.MaxKbps = 2000;

    }
    else if (tc.mode & SD_HIGH)
    {
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 576;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.TargetKbps = 2000;
        m_par.mfx.MaxKbps = 4000;

    }
    else if (tc.mode & HD_LOW)
    {
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 1280;
        m_par.mfx.FrameInfo.Height = 736;
        m_par.mfx.FrameInfo.CropH = 720;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.TargetKbps = 4000;
        m_par.mfx.MaxKbps = 8000;

    }
    else if (tc.mode & HD_HIGH)
    {
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 1920;
        m_par.mfx.FrameInfo.Height = 1088;
        m_par.mfx.FrameInfo.CropH = 1080;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        m_par.mfx.TargetKbps = 6000;
        m_par.mfx.MaxKbps = 12000;
    }

    //m_par.AddExtBuffer(EXT_BUF_PAR(mfxExtCodingOption));
    mfxExtCodingOption& cod = m_par;

    //m_par.AddExtBuffer(EXT_BUF_PAR(mfxExtCodingOption3));
    mfxExtCodingOption3& cod3 = m_par;

    if (tc.mode & WINDOW)
    {
        cod3.WinBRCMaxAvgKbps = m_par.mfx.MaxKbps;
        cod3.WinBRCSize = m_par.mfx.FrameInfo.FrameRateExtN << 2;
    }

    if (tc.mode & LA_HRD)
    {
        mfxU32 fr = mfxU32(m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD);
        // buffer = 0.5 sec
        m_par.mfx.BufferSizeInKB = mfxU16(m_par.mfx.MaxKbps / fr * mfxU16(fr / 2));
        m_par.mfx.InitialDelayInKB = mfxU16(m_par.mfx.BufferSizeInKB / 2);
    }

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        switch (tc.set_par[i].type)
        {
        case 0:
            break;
        case CODPAR:
            tsStruct::set(&cod, *tc.set_par[i].f, tc.set_par[i].v);
            break;
        case COD3PAR:
            tsStruct::set(&cod3, *tc.set_par[i].f, tc.set_par[i].v);
            break;
        case MFXPAR:
        default:
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            break;
        }
    }

    if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 24;
    }
    else if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        m_par.mfx.Accuracy = 10;
        m_par.mfx.Convergence = 50;
    }

    bool hw_surf = m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;

    UseDefaultAllocator(!hw_surf);
    SetFrameAllocator(m_session, GetAllocator());

    // set handle
    mfxHDL hdl;
    mfxHandleType type;
    if (hw_surf)
    {
        GetAllocator()->get_hdl(type, hdl);
    }
    else
    {
        tsSurfacePool alloc;
        alloc.UseDefaultAllocator(false);
        alloc.GetAllocator()->get_hdl(type, hdl);
    }

//    SetHandle(m_session, type, hdl);

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    tsExtBufType<mfxVideoParam> out_par;
    memcpy(&out_par, m_pPar, sizeof(mfxVideoParam));
    mfxExtCodingOption& out_cod = out_par;
    mfxExtCodingOption3& out_cod3 = out_par;m_par;
    Query(m_session, m_pPar, &out_par);

    g_tsStatus.expect(tc.sts);
    /*if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
    {
        g_tsStatus.expect(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }*/
    Init(m_session, m_pPar);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_constraint_vbr);
};
