/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.

File Name: avce_mb_per_sec.cpp
\* ****************************************************************************** */

/*!
\file avce_mb_per_sec.cpp
\brief Gmock test avce_mb_per_sec

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK lib
- Set suite params
- Set case params
- Set expected status (depends on D3D9, D3D11 implementation on Windows, rate control method on Linux and HW on all)
- Call Query() function
- Check MBPerSec value (if Query returns MFX_ERR_NONE MBPerSec should changed to non zero and non 0xFFFFFFFF value,
else should be zeroed)

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_mb_per_sec{

    enum{
        MFX_PAR = 1,
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct{
        /*! \brief Structure contains params for some fields of encoder */
        mfxStatus sts;
        struct f_pair{

            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];

    };

    //!\brief Main test class
    class TestSuite:tsVideoEncoder{
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) { }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        tsExtBufType<mfxVideoParam> initParams();
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    tsExtBufType<mfxVideoParam> TestSuite::initParams() {
        tsExtBufType<mfxVideoParam> par;
        par.mfx.CodecId = MFX_CODEC_AVC;
        par.AddExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY, sizeof (mfxExtEncoderCapability));
        return par;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR }}},
        {/*01*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR }}},
        {/*02*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP }}},
        {/*03*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ }}},
        {/*04*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF}, // supported with va_version >= 1.3.0
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR }}},
        {/*05*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF}, // supported with va_version >= 1.3.0
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR }}},
        {/*06*/ MFX_ERR_UNSUPPORTED, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA }}},
        {/*07*/ MFX_ERR_UNSUPPORTED, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VCM }}},
        {/*08*/ MFX_ERR_UNSUPPORTED, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_ICQ }}},
        {/*09*/ MFX_ERR_UNSUPPORTED, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_EXT }}},
        {/*10*/ MFX_ERR_UNSUPPORTED, {{MFX_PAR, &tsStruct::mfxExtEncoderCapability.MBPerSec, 0xFFFFFFFF},
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD }}}
    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id){
        TS_START;
        auto& tc = test_case[id];

        MFXInit();

        m_par = initParams();
        SETPARS(&m_par, MFX_PAR);

        mfxExtEncoderCapability *m_ec = reinterpret_cast <mfxExtEncoderCapability*> (m_par.GetExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY));

        mfxStatus exp = tc.sts;
        if (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
        {
            if (!(g_tsImpl & MFX_IMPL_VIA_D3D11))
                exp = MFX_ERR_UNSUPPORTED;
            else
                exp = MFX_ERR_NONE;
        }
        g_tsStatus.expect(exp);

        Query();

        if (g_tsImpl!=MFX_IMPL_SOFTWARE){
            if (exp == MFX_ERR_NONE){
                EXPECT_NE(0U, m_ec->MBPerSec) << "MBPerSec is not a valid value";
                EXPECT_NE(0xFFFFFFFF, m_ec->MBPerSec) << "MBPerSec is not a valid value";
            }
            else
                EXPECT_EQ(0U, m_ec->MBPerSec) << "MBPerSec wasn't zeroed";
        }
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_mb_per_sec);
}