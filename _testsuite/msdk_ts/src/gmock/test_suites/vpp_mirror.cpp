/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2018 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_mirror
{
class TestSuite : tsVideoVPP
{
public:
    TestSuite()
        : tsVideoVPP()
    { }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:

    enum
    {
        MFX_PAR = 1,
        EXTBUFF_VPP_MIRRORING,
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
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    /* normal flow test cases */
    {/*00*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_MIRRORING, &tsStruct::mfxExtVPPMirroring.Type, MFX_MIRRORING_HORIZONTAL}}
    },
    {/*01*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1280},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 720},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1280},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 720},
        {EXTBUFF_VPP_MIRRORING, &tsStruct::mfxExtVPPMirroring.Type, MFX_MIRRORING_HORIZONTAL}}
    },
    {/*02*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.In.Height, 1088},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Width, 1920},
        {MFX_PAR, &tsStruct::mfxVideoParam.vpp.Out.Height, 1088},
        {EXTBUFF_VPP_MIRRORING, &tsStruct::mfxExtVPPMirroring.Type, MFX_MIRRORING_HORIZONTAL}}
    },
    {/*03*/ MFX_ERR_NONE, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY},
        {EXTBUFF_VPP_MIRRORING, &tsStruct::mfxExtVPPMirroring.Type, MFX_MIRRORING_HORIZONTAL}}
    },
    /*  bad parameters test cases */
    {/*04*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {MFX_PAR, &tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY},
        {EXTBUFF_VPP_MIRRORING, &tsStruct::mfxExtVPPMirroring.Type, MFX_MIRRORING_HORIZONTAL + 100}}
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxStatus init_sts = tc.sts;
    mfxU32 mode = tc.mode;

    MFXInit();

    // set parameters for video processing
    SETPARS(m_pPar, MFX_PAR);

    CreateAllocators();
    SetFrameAllocator();
    SetHandle();

    // prepare extended buffers with parameters and set
    std::vector<mfxExtBuffer*> ext_buffers;
    mfxExtVPPMirroring ext_buffer_mirroring;
    memset(&ext_buffer_mirroring, 0, sizeof(mfxExtVPPMirroring));
    ext_buffer_mirroring.Header.BufferId = MFX_EXTBUFF_VPP_MIRRORING;
    ext_buffer_mirroring.Header.BufferSz = sizeof(mfxExtVPPMirroring);
    SETPARS(&ext_buffer_mirroring, EXTBUFF_VPP_MIRRORING);
    ext_buffers.push_back((mfxExtBuffer*)&ext_buffer_mirroring);

    m_par.NumExtParam = (mfxU16)ext_buffers.size();
    m_par.ExtParam = &ext_buffers[0];

    // check Query behaviour
    g_tsStatus.expect((init_sts == MFX_ERR_INVALID_VIDEO_PARAM) ? MFX_ERR_UNSUPPORTED : init_sts);
    Query();

    // check Init behaviour
    g_tsStatus.expect(init_sts);
    Init(m_session, m_pPar);

    if (MFX_ERR_NONE == init_sts)
    {
        // run VPP and check its behaviour
        g_tsStatus.expect(MFX_ERR_NONE);
        AllocSurfaces();
        ProcessFrames(10);
    }

    // close the session and check surfaces to be freed
    Close(true);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_mirror);

}
