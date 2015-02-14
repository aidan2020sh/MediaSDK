#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace mpeg2e_mbqp
{
enum
{
    QP_INVALID = 255,
    QP_BY_FRM_TYPE = 254
};

class AUWrap
{
private:
    tsParserMPEG2AU::UnitType& m_hdr;
    mfxU32 m_mb;
    mfxU32 m_slice;
public:

    AUWrap(tsParserMPEG2AU::UnitType& hdr)
        : m_hdr(hdr)
        , m_mb(0)
        , m_slice(0)
    {
    }
    ~AUWrap()
    {
    }

    mfxU32 GetFrameType()
    {
        if (!m_hdr.pic_hdr)
            return 0;

        mfxU32 type = 0;
        switch(m_hdr.pic_hdr->picture_coding_type)
        {
        case 3:
            type = MFX_FRAMETYPE_B;
            break;
        case 2:
            type = MFX_FRAMETYPE_P;
            break;
        case 1:
            type = MFX_FRAMETYPE_I;
            break;
        default:
            break;
        }

        return type;
    }

    mfxU8 NextQP()
    {
        if (m_mb > m_hdr.slice[m_slice].NumMb - 1)
        {
            if (++m_slice > m_hdr.NumSlice - 1)
                return QP_INVALID;

            m_mb = 0;
        }

        mfxU8 quantizer_scale_code = m_hdr.slice[m_slice].mb[m_mb++].quantiser_scale_code;

        if (!quantizer_scale_code)
            quantizer_scale_code = m_hdr.slice[m_slice].quantiser_scale_code;

        return quantizer_scale_code;
    }
};

class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public tsNoiseFiller, public tsParserMPEG2AU
{
private:
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
        std::vector<mfxU8>          buf;
    };

    std::map<mfxU32, Ctrl> m_ctrl;
    mfxU32 m_fo;
    bool mbqp_on;
    mfxU32 mode;
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_MPEG2)
        , tsParserMPEG2AU(BS_MPEG2_INIT_MODE_MB)
        , m_fo(0)
        , mbqp_on(true)
        , mode(0)
    {
        set_trace_level(0);
        srand(0);

        m_filler = this;
        m_bs_processor = this;
    }

    ~TestSuite() {}

    enum
    {
        MFXPAR = 1
    };
    enum
    {
        INIT  = 1,
        QUERY = 1 << 1,
        RESET_ON  = 1 << 2,
        RESET_OFF = 1 << 3,
        RANDOM = 1 << 4
    };

    static const unsigned int n_cases;
    int RunTest(unsigned int id);
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[5];
    };
    static const tc_struct test_case[];


    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        tsNoiseFiller::ProcessSurface(s);
        mfxU32 numMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16) * ((m_par.mfx.FrameInfo.CropH + 15) / 16);
        Ctrl& ctrl = m_ctrl[m_fo];

        ctrl.buf.resize(numMB);
        if (mode & RANDOM)
            mbqp_on = ((rand() % 9000) % 2 == 0);

        for (mfxU32 i = 0; i < ctrl.buf.size(); i++)
        {
            if (mbqp_on)
                ctrl.buf[i] = 1 + rand() % 50;
            else
                ctrl.buf[i] = QP_BY_FRM_TYPE;
        }

        mfxExtMBQP& mbqp = ctrl.ctrl;
        mbqp.QP = &ctrl.buf[0];
        mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());

        if (mbqp_on)
        {
            m_pCtrl = &ctrl.ctrl;
        }
        else
        {
            m_pCtrl->NumExtParam = 0;
        }

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        while (checked++ < nFrames)
        {
            UnitType& hdr = ParseOrDie();
            AUWrap au(hdr);
            mfxU8 qp = au.NextQP();
            mfxU32 i = 0;
            mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
            mfxExtMBQP& mbqp = m_ctrl[(mfxU32)bs.TimeStamp].ctrl;

            g_tsLog << "Frame #"<< bs.TimeStamp <<" QPs:\n";

            while (qp != QP_INVALID)
            {
                g_tsLog << std::setw(2) << mfxU16(qp) << " ";

                mfxU8 expected_qp = mbqp.QP[i];
                if (expected_qp == QP_BY_FRM_TYPE)
                {
                    switch(au.GetFrameType())
                    {
                    case MFX_FRAMETYPE_I:
                        expected_qp = (mfxU8)m_par.mfx.QPI;
                        break;
                    case MFX_FRAMETYPE_P:
                        expected_qp = (mfxU8)m_par.mfx.QPP;
                        break;
                    case MFX_FRAMETYPE_B:
                        expected_qp = (mfxU8)m_par.mfx.QPB;
                        break;
                    default:
                        expected_qp = 0;
                        break;
                    }
                }

                mfxU16 quantiser_scale_code = 0;
                if (hdr.pic_coding_ext->q_scale_type == 0)
                {
                    quantiser_scale_code = expected_qp / 2;
                }
                else
                {
                    if (expected_qp <= 8)
                        quantiser_scale_code = expected_qp;
                    else if (expected_qp <=24)
                        quantiser_scale_code =  8 + (expected_qp-8)/2;
                    else if (expected_qp <= 56)
                        quantiser_scale_code =  16 + (expected_qp-24)/4;
                    else
                        quantiser_scale_code =  24 + (expected_qp-56)/8;
                }

                if (quantiser_scale_code != qp)
                {
                    g_tsLog << "\nERROR: Expected QP converted/msdk (" << mfxU16(quantiser_scale_code)
                            << "/" << mfxU16(expected_qp)
                            << ") != real QP (" << mfxU16(qp) << ")\n";
                    return MFX_ERR_ABORTED;
                }

                if (++i % wMB == 0)
                    g_tsLog << "\n";

                qp = au.NextQP();
            }
            g_tsLog << "\n";
        }

        m_ctrl.erase((mfxU32)bs.TimeStamp);

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

class ps : public tsSurfaceProcessor
{
public:
    
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // not CQP: Init
    {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
    {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
    {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR}},
    {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ}},
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA}},
    {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT}},
    {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD}},
    {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR}},
    {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},
    {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME}},
    // not CQP: Query
    {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}},
    {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR}},
    {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR}},
    {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ}},
    {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA}},
    {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT}},
    {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD}},
    {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ}},
    {/*19*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR}},
    {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM}},
    {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VME}},

    {/*22*/ MFX_ERR_NONE, 0},
    {/*23*/ MFX_ERR_NONE, RESET_ON},
    {/*24*/ MFX_ERR_NONE, RESET_OFF},
    {/*25*/ MFX_ERR_NONE, RANDOM},
    {/*26*/ MFX_ERR_NONE, RANDOM|RESET_ON},
    {/*27*/ MFX_ERR_NONE, RANDOM|RESET_OFF}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];
    mode = tc.mode;

    MFXInit();

    if (!(tc.mode & RESET_ON))
    {
        mfxExtCodingOption3& co3 = m_par;
        co3.EnableMBQP = MFX_CODINGOPTION_ON;
    }
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    SETPARS(m_pPar, MFXPAR);
    set_brc_params(&m_par);

    g_tsStatus.expect(tc.sts);

    if (tc.mode & QUERY)
    {
        Query();
    }
    if (tc.mode & INIT)
    {
        Init();
    }

    if (tc.sts == MFX_ERR_NONE)
    {
        if (tc.mode & RESET_ON)
        {
            mbqp_on = false;
        }
        EncodeFrames(30);

        if ((tc.mode & RESET_ON) || (tc.mode & RESET_OFF))
        {
            if (tc.mode & RESET_ON)
            {
                mfxExtCodingOption3& co3 = m_par;
                co3.EnableMBQP = MFX_CODINGOPTION_ON;
            }
            mbqp_on = !mbqp_on;
            Reset();
            EncodeFrames(30);
        }
    }
    else if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && (tc.mode & QUERY))
    {
        mfxExtCodingOption3* co3 = (mfxExtCodingOption3*)m_par.ExtParam[0];
        if (co3->EnableMBQP != MFX_CODINGOPTION_OFF)
        {
            g_tsLog << "ERROR: EnableMBQP should be set to OFF (" << MFX_CODINGOPTION_OFF
                    << "), real is " << co3->EnableMBQP << "\n";
            g_tsStatus.check(MFX_ERR_ABORTED);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_mbqp);
};
