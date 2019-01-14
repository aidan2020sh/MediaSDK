/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include <type_traits>
#include <iostream>

#include "ts_encoder.h"
#include "avce_mfe.h"

/*!

\file avce_mfe_init.cpp
\brief Gmock test avce_mfe_init

Description:

This test checks behavior of the encoding Init() for various values of the MFE
parameters (fields MFMode and MaxNumFrames) combined with other parameters that
affect the MFE functionality

Algorithm:
- Initializing Media SDK lib
- Initializing suite and case parameters
- Run Init()
- Check Init() status
- Check that input MFE parameters are not changed

*/

namespace avce_mfe
{

int test_init(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_AVC);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    tsExtBufType<mfxVideoParam> pout;
    mfxExtMultiFrameParam& mfe_out = pout;
    set_params(enc, tc, pout);

    // Update some init specific parameters
    if (tc.cOpt & COptMAD) {
        tc.sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    g_tsStatus.expect(tc.sts);
    enc.Init(enc.m_session, &enc.m_par);

    mfxExtMultiFrameParam& mfe_in = enc.m_par;
    EXPECT_EQ(mfe_in.MaxNumFrames, tc.inMaxNumFrames);
    EXPECT_EQ(mfe_in.MFMode, tc.inMFMode);

    TS_END;
    return 0;
}


TS_REG_TEST_SUITE(avce_mfe_init, test_init, test_case_num);

};