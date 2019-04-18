/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"
#include "ts_fei_warning.h"

namespace hevce_fei_payload
{

// In case you need to verify the output manually, define the MANUAL_DEBUG_MODE macro.
// In this case the encoded bitstream will be written to the debug.hevc file.
// #define MANUAL_DEBUG_MODE

using namespace BS_HEVC;

class TestSuite : tsVideoEncoder, tsSurfaceProcessor, tsBitstreamProcessor, tsParserHEVC
{
public:
    static const unsigned int n_cases;

    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC, true, MSDK_PLUGIN_TYPE_FEI)
        , tsParserHEVC(INIT_MODE_STORE_SEI_DATA)
        , m_nframes(10)
        , m_frm_in(0)
        , m_frm_out(0)
        , m_noiser(10)
#ifdef MANUAL_DEBUG_MODE
        , m_writer("debug.hevc")
#endif
    {
        m_filler = this;
        m_bs_processor = this;
        m_par.AsyncDepth = 1;       // simplify test logic
        m_par.mfx.GopRefDist = 1;   // avoid reordering

        mfxExtFeiHevcEncFrameCtrl & ctrl = m_ctrl;
        ctrl.SubPelMode = 3;
        ctrl.SearchWindow = 5;
        ctrl.NumFramePartitions = 4;
        ctrl.MultiPred[0] = ctrl.MultiPred[1] = 1;
    }
    ~TestSuite() {
        for (auto & i_map : m_per_frame_sei)
        {
            for (auto & i_pl : i_map.second)
            {
                if (i_pl) delete[] i_pl->Data;

                delete i_pl;
            }
            i_map.second.clear();
        }
    }

    int RunTest(unsigned int id);

private:
    std::map<mfxU32, std::vector<mfxPayload*>> m_per_frame_sei;
    std::map<mfxU32, std::vector<mfxPayload*>> m_expected_per_frame_sei;
    mfxU32 m_nframes;
    mfxU32 m_frm_in;
    mfxU32 m_frm_out;
    tsNoiseFiller m_noiser;
#ifdef MANUAL_DEBUG_MODE
    tsBitstreamWriter m_writer;
#endif

    struct payload_types
    {
        mfxU16 types;
        bool is_suffix;
    };
    mfxStatus ProcessSurface(mfxFrameSurface1&);
    mfxStatus ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames);
    void sei_calc_size_type(std::vector<mfxU8>& data, mfxU16 type, mfxU32 size);
    void create_sei_message(mfxPayload* pl, const struct payload_types& pt, mfxU32 size);

    enum {
        MFX_PAR = 1
    };

    struct tc_struct
    {
        struct
        {
            mfxU16 nf;
            mfxU16 num_payloads;
            struct payload_types t_pld[5];
        } per_frm_sei[5];
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
    void InitPayloads(tc_struct tc);
};

mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& pSurf)
{
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (m_frm_in > (m_nframes - 1))
        return mfxSts;

    mfxSts = m_noiser.ProcessSurface(pSurf);

    if (m_per_frame_sei.find(m_frm_in) != m_per_frame_sei.end()) {
        m_ctrl.Payload = m_per_frame_sei[m_frm_in].data();
        m_ctrl.NumPayload = (mfxU16)m_per_frame_sei[m_frm_in].size();
    } else {
        m_ctrl.Payload = NULL;
        m_ctrl.NumPayload = 0;
    }

    m_frm_in++;

    return mfxSts;
}

mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
{
    SetBuffer0(bs);

#ifdef MANUAL_DEBUG_MODE
    m_writer.ProcessBitstream(bs, nFrames);
#endif

    auto& au = ParseOrDie();
    if (m_expected_per_frame_sei.find(m_frm_out) == m_expected_per_frame_sei.end()) {
        m_frm_out++;
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

    struct payload {
        mfxU32 CtrlFlags;
        mfxU16 Type;
        mfxU16 BufSize;
    };

    std::vector<payload> found_pl;

    for (Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++) {
        if (au.nalu == NULL || (au.nalu && au.nalu[au_ind] == NULL))
            return MFX_ERR_NULL_PTR;

        auto nalu = au.nalu[au_ind];

        if (nalu->nal_unit_type != PREFIX_SEI_NUT && nalu->nal_unit_type != SUFFIX_SEI_NUT) {
            continue;
        }

        for (Bs16u sei_ind = 0; sei_ind < nalu->sei->NumMessage; ++sei_ind) {
            payload p = {};
            if (nalu->nal_unit_type == PREFIX_SEI_NUT) {
                p.CtrlFlags = 0;
            } else if (nalu->nal_unit_type == SUFFIX_SEI_NUT) {
                p.CtrlFlags = MFX_PAYLOAD_CTRL_SUFFIX;
            }
            p.BufSize = nalu->sei->message[sei_ind].payloadSize;
            p.Type = nalu->sei->message[sei_ind].payloadType;
            found_pl.push_back(p);
        }
    }

    mfxU32 num_sei = 0;
    for (auto p : found_pl) {
        for (auto exp_p : m_expected_per_frame_sei[m_frm_out]) {
            // recalc real size, because exp_p->BufSize = SEI size + header_size(type + size)
            mfxU16 size = exp_p->BufSize;
            std::vector<mfxU8> data;
            sei_calc_size_type(data, exp_p->Type, exp_p->BufSize);
            size -= (mfxU16)data.size();

            if (p.CtrlFlags == exp_p->CtrlFlags &&
                p.BufSize == size     &&
                p.Type == exp_p->Type) {
                num_sei++;
            }
        }
    }

    EXPECT_EQ(num_sei, m_expected_per_frame_sei[m_frm_out].size())
            << "ERROR: incorrect number of SEI messages\n";

    bs.DataLength = 0;
    m_frm_out++;

    return MFX_ERR_NONE;
}

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ /*just 3, 4, 5, 17, 22, 132, 146 could be suffix */
            { {0, 3, {{3, true}, {4, true}, {5, true}}}, {1, 1, {{17, true}}}, {7, 2, {{22, true}, {146, true}}} }},
    {/*02*/ /*type 1, 2, 45, 47 with true(suffix) should not be added in the bitstream */
            { {1, 2, {{1, true}, {2, true}}}, {9, 2, {{45, true}, {47, true}}} }},
    {/*03*/ { {0, 3, {{0, false}, {22, true}, {17, true}}}, {1, 2, {{1, false}, {2, false}}}, {7, 3, {{3, true}, {4, true}, {5, true}}}, {9, 1, {{2, false}}} }},
    {/*04*/ { {0, 2, {{0, false}, {1, false}}}, {1, 2, {{1, true}, {2, true}}}, {9, 1, {{2, true}}} }},
    // driver doesn't support sum_payload_size for frame > 256 (current limitation), so this case will be FAILED now.
    //{/*05*/ { {0, 4, {{0, false}, {1, false}, {1, false}, {5, false}}}, {1, 2, {{1, false}, {2, false}}} , {9, 5, {{0, false}, {47, false}, {2, false}, {45, false}, {132, false}}} }}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

void TestSuite::sei_calc_size_type(std::vector<mfxU8>& data, mfxU16 type, mfxU32 size)
{
    size_t B = type;

    while (B > 255)
    {
        data.push_back(255);
        B -= 255;
    }
    data.push_back(mfxU8(B));

    B = size;

    while (B > 255)
    {
        data.push_back(255);
        B -= 255;
    }
    data.push_back(mfxU8(B));
}

void TestSuite::create_sei_message(mfxPayload* pl, const struct payload_types& pt, mfxU32 size)
{
    std::vector<mfxU8> data;
    mfxU16 type = pt.types;
    sei_calc_size_type(data, type, size);

    size_t B = data.size();

    data.resize(B + size);

    pl->CtrlFlags = pt.is_suffix ? MFX_PAYLOAD_CTRL_SUFFIX : 0;
    if (pt.is_suffix) {
        EXPECT_EQ(pl->CtrlFlags, MFX_PAYLOAD_CTRL_SUFFIX) << "Error while creating SEI message";
    } else {
        EXPECT_EQ(pl->CtrlFlags, 0U) << "Error while creating SEI message";
    }

    pl->BufSize = mfxU16(B + size);
    EXPECT_EQ(pl->BufSize, mfxU16(B + size)) << "Error while creating SEI message";

    pl->Data = new mfxU8[pl->BufSize];
    EXPECT_EQ(pl->Data != NULL, 1) << "Error while creating SEI message";

    pl->NumBit = pl->BufSize * 8;
    EXPECT_EQ(pl->NumBit, pl->BufSize * 8U) << "Error while creating SEI message";

    pl->Type = mfxU16(type);
    EXPECT_EQ(pl->Type, mfxU16(type)) << "Error while creating SEI message";

    memcpy(pl->Data, &data[0], pl->BufSize);
}

void TestSuite::InitPayloads (tc_struct tc)
{
    for (mfxU16 i = 0; i < 5; ++i) {
        for (mfxU16 p_ind = 0; p_ind < tc.per_frm_sei[i].num_payloads; ++p_ind) {
            mfxPayload* p = new mfxPayload;
            auto par = tc.per_frm_sei[i];
            create_sei_message(p, par.t_pld[p_ind], (par.nf + p_ind + 1) * 6);
            // Acording to ITU-T H.265 (12/2016) only following SEI can be SUFFIX:
            // 3, 4, 5, 17, 22, 132, 146
            // And type 132 is suffix only
            mfxU16 type = par.t_pld[p_ind].types;

            m_per_frame_sei[par.nf].push_back(p);

            if ( (((type == 3) || (type == 4) || (type == 5) || (type == 17) || (type == 22) || (type == 132)) && par.t_pld[p_ind].is_suffix) ||
                    (type != 132 && !par.t_pld[p_ind].is_suffix) ) {
                m_expected_per_frame_sei[par.nf].push_back(p);
            }
        }
    }
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    CHECK_HEVC_FEI_SUPPORT();
    srand(0);
    const tc_struct& tc = test_case[id];
    InitPayloads(tc);

    SETPARS(m_pPar, MFX_PAR);
    set_brc_params(&m_par);

    MFXInit();

    Load();

    Init();

    EncodeFrames(m_nframes, true);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_fei_payload);
}