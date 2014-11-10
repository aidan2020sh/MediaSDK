/*
Per-frame Deblocking disabling disable_deblocking_filter_idc in slice_header
mfxExtCodingOption2::DisableDeblockingIdc = 0|1
slice_header:: disable_deblocking_filter_idc = 0|1
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_co2_disable_deblocking
{

enum
{
    FEATURE_ENABLED = 42
};

class TestSuite : tsVideoEncoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
        , mode(0)
        , n_frame(0)
    {
        m_filler = this;
        memset(buffers, 0, sizeof(mfxExtBuffer*)*100);
    }
    ~TestSuite()
    {
        for (mfxU32 i = 0; i < 100; i++)
            delete [] buffers[i];
    }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxU32 mode;
    mfxU32 n_frame;
    mfxExtBuffer* buffers[100];
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
    };

    enum
    {
        QUERY = 1,

        RUNTIME_ONLY = 1 << 1,
        EVERY_OTHER  = 1 << 2,

        RESET_ON  = 1 << 3,
        RESET_OFF = 1 << 4
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        char* skips;
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        assert(n_frame < 100);
        buffers[n_frame] = (mfxExtBuffer*) new mfxExtCodingOption2;
        mfxExtCodingOption2* buf_a = (mfxExtCodingOption2*) buffers[n_frame];
        memset(buf_a, 0, sizeof(mfxExtCodingOption2));
        buf_a->Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
        buf_a->Header.BufferSz = sizeof(mfxExtCodingOption2);

        if (mode & EVERY_OTHER) buf_a->DisableDeblockingIdc = n_frame % 2 ? 0 : 1;
        else                    buf_a->DisableDeblockingIdc = 1;

        if (mode & EVERY_OTHER || mode & RUNTIME_ONLY)
        {
            m_ctrl.ExtParam = &buffers[n_frame];
            m_ctrl.NumExtParam = 1;
            s.Data.TimeStamp = buf_a->DisableDeblockingIdc ? FEATURE_ENABLED : 0;
        }
        else if (m_par.NumExtParam)
        {
            mfxExtCodingOption2* co2 = (mfxExtCodingOption2*)m_par.ExtParam[0];
            s.Data.TimeStamp = co2->DisableDeblockingIdc ? FEATURE_ENABLED : 0;
        }

        n_frame++;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*00*/{MFX_ERR_NONE, 0, {}},
    /*01*/{MFX_ERR_NONE, QUERY, {}},
    /*02*/{MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY, {EXT_COD2, &tsStruct::mfxExtCodingOption2.DisableDeblockingIdc, 2}},
    /*03*/{MFX_ERR_NONE, RUNTIME_ONLY, {}},
    /*04*/{MFX_ERR_NONE, EVERY_OTHER, {}},
    /*05*/{MFX_ERR_NONE, RUNTIME_ONLY|EVERY_OTHER, {}},
    /*06*/{MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*07*/{MFX_ERR_NONE, 0, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                             {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
    /*08*/{MFX_ERR_NONE, EVERY_OTHER, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}},
    /*09*/{MFX_ERR_NONE, EVERY_OTHER, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}}},
    /*10*/{MFX_ERR_NONE, RESET_ON, {}},
    /*11*/{MFX_ERR_NONE, RESET_ON|RUNTIME_ONLY, {}},
    /*12*/{MFX_ERR_NONE, RESET_ON|EVERY_OTHER, {}},
    /*13*/{MFX_ERR_NONE, RESET_OFF, {}},
    /*14*/{MFX_ERR_NONE, RESET_OFF|RUNTIME_ONLY, {}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#define SETPARS(p, type)\
for(mfxU32 i = 0; i < n_par; i++) \
{ \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type) \
    { \
        tsStruct::set(p, *tc.set_par[i].f, tc.set_par[i].v); \
    } \
}

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU32 n_frame;
public:
    BsDump() :
        tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , n_frame(0)
    {
        set_trace_level(0);
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        g_tsLog << "CHECK: slice_hdr->disable_deblocking_filter_idc\n";
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);
        UnitType& au = ParseOrDie();
        for (Bs32u i = 0; i < au.NumUnits; i++)
        {
            if (!(au.NALU[i].nal_unit_type == 0x01 || au.NALU[i].nal_unit_type == 0x05))
            {
                continue;
            }

            Bs32u expected = bs.TimeStamp == FEATURE_ENABLED ? 1 : 0;
            if (au.NALU[i].slice_hdr->disable_deblocking_filter_idc != expected)
            {
                g_tsLog << "ERROR: frame#" << n_frame << " slice_hdr->disable_deblocking_filter_idc="
                        << au.NALU[i].slice_hdr->disable_deblocking_filter_idc
                        << " != " << expected << " (expected value)\n";
                return MFX_ERR_UNKNOWN;
            }
        }
        n_frame++;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    int err = 0;
    TS_START;
    const tc_struct& tc = test_case[id];
    mode = tc.mode;

    MFXInit();

    BsDump bs;
    m_bs_processor = &bs;

    if (!(tc.mode & RUNTIME_ONLY) && !(tc.mode & RESET_ON))
    {
        mfxExtCodingOption2& cod2 = m_par;
        cod2.DisableDeblockingIdc = 1;
        SETPARS(&cod2, EXT_COD2);
    }

    SETPARS(m_pPar, MFX_PAR);

    g_tsStatus.expect(tc.sts);

    if (tc.mode & QUERY)
    {
        Query();
    }

    if (tc.sts == MFX_ERR_NONE)
    {
        if (tc.mode & RESET_ON)
        {
            mode = 0;
            m_par.ExtParam = 0;
            m_par.NumExtParam = 0;
        }
        EncodeFrames(2);

        if (tc.mode & RESET_ON)
        {
            mode = tc.mode;
            if (!(tc.mode & RUNTIME_ONLY))
            {
                mfxExtCodingOption2& cod2 = m_par;
                cod2.DisableDeblockingIdc = 1;
                SETPARS(&cod2, EXT_COD2);
            }
            Reset();
            EncodeFrames(2);
        }
        else if (tc.mode & RESET_OFF)
        {
            mode = 0;
            m_par.ExtParam = 0;
            m_par.NumExtParam = 0;
            Reset();
            EncodeFrames(40);
        }
    }

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(avce_co2_disable_deblocking);
};
