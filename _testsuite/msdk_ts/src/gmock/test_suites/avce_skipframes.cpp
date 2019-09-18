/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace avce_skipframes
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;

    enum
    {
        MFXPAR = 1,
        EXT_COD2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 nframes;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        std::string skips;
    };

    static tc_struct test_case[];
};

TestSuite::tc_struct TestSuite::test_case[] =
{
    // IPPP, all skipped
    {/*00*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, ""},
    {/*01*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, ""},
    {/*02*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, 3}}, ""},

    // IBBBBP, all skipped
    {/*03*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, ""},
    {/*04*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, ""},
    {/*05*/ MFX_ERR_NONE, 10, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, 3}}, ""},

    // IPPP|IBBBBP, not all skipped
    {/*06*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 3 5 7"},
    {/*07*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 5 7"},
    {/*08*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 3 5 7"},
    {/*09*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_NOTHING}}, "1 5 7"},
    {/*10*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, 3}}, "1 3 5 7"},
    {/*11*/ MFX_ERR_NONE, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, 3}}, "1 5 7"},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class SurfProc : public tsSurfaceProcessor
{
    mfxEncodeCtrl* pCtrl;
    std::vector<mfxU32> m_skip_frames;
    mfxU32 m_curr_frame;
  public:
      SurfProc(mfxU32 n_frames = 0xFFFFFFFF) : pCtrl(0), m_curr_frame(0) {} ;
    ~SurfProc() {} ;

    mfxStatus Init(mfxEncodeCtrl& ctrl, std::vector<mfxU32>& skip_frames)
    {
        pCtrl = &ctrl;
        m_skip_frames = skip_frames;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_skip_frames.size())
        {
            if (std::find(m_skip_frames.begin(), m_skip_frames.end(), m_curr_frame) != m_skip_frames.end())
            {
                pCtrl->SkipFrame = 1;
            }
            else
            {
                pCtrl->SkipFrame = 0;
            }
        }
        else
        {
            pCtrl->SkipFrame = 1;
        }
        m_curr_frame++;
        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    if (!test_case[id].nframes)
    {
        test_case[id].nframes = g_tsConfig.sim? 15 : 30;
    }
    const tc_struct& tc = test_case[id];

    MFXInit();

    std::stringstream stream(tc.skips);
    std::vector<mfxU32> skip_frames;
    mfxU32 f;
    while (stream >> f)
    {
        skip_frames.push_back(f);
    }
    SurfProc surf_proc;
    surf_proc.Init(m_ctrl, skip_frames);
    m_filler = &surf_proc;

    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);

    mfxExtCodingOption2& cod2 = m_par;
    SETPARS(&cod2, EXT_COD2);

    SetFrameAllocator();

    ///////////////////////////////////////////////////////////////////////////
    if ((m_par.mfx.LowPower == MFX_CODINGOPTION_ON) && (m_par.mfx.GopRefDist > 1))
    {
        g_tsLog << "WARNING: CASE WAS SKIPPED\n";
    }
    else
    {

        mfxU32 n = tc.nframes;
        AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 4 * n);

        g_tsStatus.expect(tc.sts);
        EncodeFrames(n);
    }
    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_skipframes);
};
