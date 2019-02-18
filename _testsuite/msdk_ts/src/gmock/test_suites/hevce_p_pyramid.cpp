/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

//#define DUMP_BS
#define MAX_P_PYRAMID_WIDTH 3

namespace hevce_p_pyramid
{
class TestSuite : public tsVideoEncoder, public tsBitstreamProcessor, public tsParserHEVC
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVC()
        , m_anchorPOC(0)
#ifdef DUMP_BS
        , m_writer("debug_hevce_p_pyramid.265")
#endif
    {
        m_bs_processor = this;
        set_trace_level(0);
    }
    ~TestSuite() { }

    int RunTest(unsigned int id);
    mfxU16 FindMax(mfxU16 arr[], mfxU16 size);
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);

    static const unsigned int n_cases;

private:
    struct DPBPic
    {
        Bs32s POC;
        Bs16u layer;
    };
    std::list<DPBPic> m_dpb;
    mfxI32 m_anchorPOC;
    mfxI32 m_pyramidWidth;
#ifdef DUMP_BS
    tsBitstreamWriter m_writer;
#endif

    struct tc_struct
    {
        mfxU16 GopPicSize;
        mfxI16 QPOffset[8];
        mfxU16 NumRefActive[8];
    };
    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
/*00*/ {0, {}, {}},
/*01*/ {30, {-2, 1, 3}, {3, 2, 1}},
/*02*/ {30, {}, {1, 2, 2}},
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

mfxU16 TestSuite::FindMax(mfxU16 arr[], mfxU16 size)
{
    mfxU16 max = arr[0];

    for (mfxU16 i = 1; i < size; i++)
        if (arr[i] > max) max = arr[i];

    return max;
}

using namespace BS_HEVC;

mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
{
    const mfxU16 pattern[3][3] = 
    {
        {0, 0, 0}, //pattern for pyramid width = 1
        {0, 1, 0}, //pattern for pyramid width = 2
        {0, 2, 1}  //pattern for pyramid width = 3
    };
    mfxExtCodingOption3& CO3 = m_par;

    SetBuffer0(bs);

#ifdef DUMP_BS
    m_writer.ProcessBitstream(bs, nFrames);
#endif

    auto& AU = ParseOrDie();
    auto& S = AU.pic->slice[0]->slice[0];

    mfxI16 QP  = S.slice_qp_delta + S.pps->init_qp_minus26 + 26;
    mfxI16 QPX = m_par.mfx.QPP;
    mfxI32 idx = abs(AU.pic->PicOrderCntVal - m_anchorPOC) % m_pyramidWidth;
    mfxU16 layer = pattern[m_pyramidWidth - 1][idx];
    g_tsLog << "POC: " << AU.pic->PicOrderCntVal << " Layer: "<< layer << "\n";

    if (S.type == I)
    {
        EXPECT_EQ(m_par.mfx.QPI, QP);
        m_anchorPOC = AU.pic->PicOrderCntVal;
    }
    else
    {
        if (CO3.EnableQPOffset == MFX_CODINGOPTION_ON)
            EXPECT_EQ(QPX + CO3.QPOffset[layer], QP) << " (layer = " << layer << ")";
        else
            EXPECT_EQ(QPX, QP);

        EXPECT_EQ(TS_MIN(CO3.NumRefActiveP[layer], m_dpb.size()), (size_t)S.num_ref_idx_l0_active_minus1+1);

        std::list<DPBPic> L0(m_dpb);
        
        while (L0.size() > CO3.NumRefActiveP[layer])
        {
            auto weak = L0.begin();

            for (auto it = L0.begin(); it != L0.end(); it ++)
            {
                if (weak->layer < it->layer && it->POC != m_dpb.back().POC)
                    weak = it;
            }

            L0.erase(weak);
        }

        L0.reverse();

        mfxU16 idx = 0;
        for (auto expected : L0)
        {
            g_tsLog << " - Actual: " << AU.pic->RefPicList0[idx]->PicOrderCntVal
                    << " - Expected: " << expected.POC << "\n";
            EXPECT_EQ(expected.POC, AU.pic->RefPicList0[idx]->PicOrderCntVal);

            if (++idx > S.num_ref_idx_l0_active_minus1)
                break;
        }
    }

    if (bs.FrameType & MFX_FRAMETYPE_REF)
    {
        if (m_dpb.size() == m_par.mfx.NumRefFrame)
        {
            auto weak = m_dpb.begin();

            for (auto it = m_dpb.begin(); it != m_dpb.end(); it ++)
            {
                if (weak->layer < it->layer)
                {
                    weak = it; 
                    break;
                }
            }

            m_dpb.erase(weak);
        }

        if (bs.FrameType & MFX_FRAMETYPE_I)
            m_dpb.resize(0);

        DPBPic pic = {AU.pic->PicOrderCntVal, layer};
        m_dpb.push_back(pic);
    }

    bs.DataLength = 0;

    return MFX_ERR_NONE;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    mfxExtCodingOption3& CO3 = m_par;

    m_par.AsyncDepth = 1; // to simplify test logic

    CO3.PRefType = MFX_P_REF_PYRAMID;
    m_par.mfx.GopRefDist = 1;
    m_par.mfx.GopPicSize = tc.GopPicSize;
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.QPI = 26;
    m_par.mfx.QPP = 28;
    m_par.mfx.QPI = 28;

    Init();
    GetVideoParam();
    Close();

    EXPECT_NE(m_par.mfx.NumRefFrame, 0);
    m_pyramidWidth = TS_MIN(m_par.mfx.NumRefFrame, MAX_P_PYRAMID_WIDTH);

    mfxU16 maxActual = FindMax(CO3.NumRefActiveP, sizeof(CO3.NumRefActiveP) / sizeof(mfxU16));

    CO3.EnableQPOffset = MFX_CODINGOPTION_ON;
    memcpy(CO3.QPOffset, tc.QPOffset, sizeof(CO3.QPOffset));
    memcpy(CO3.NumRefActiveP, tc.NumRefActive, sizeof(CO3.NumRefActiveP));

    mfxU16 maxTest = FindMax(CO3.NumRefActiveP, sizeof(CO3.NumRefActiveP) / sizeof(mfxU16));

    if (maxActual < maxTest)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    Init();
    GetVideoParam();

    EncodeFrames(50);
    
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_p_pyramid);
}
