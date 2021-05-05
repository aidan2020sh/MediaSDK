/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2016-2018 Intel Corporation. All Rights Reserved.
//
//
*/

#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxstructures.h"

namespace vpp_mctf_reset
{

    class TestSuite : protected tsVideoVPP, public tsSurfaceProcessor
    {
    public:
        TestSuite()
            : tsVideoVPP()
        {
            m_surf_in_processor = this;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            m_par.AsyncDepth = 1;
        }
        ~TestSuite() {}
        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        struct do_not_use
        {
            mfxU32 alg_num;
            mfxU32 alg_list[6];
        };

        enum
        {
            RESET = 1,
            INIT,
        };

    private:

        tsNoiseFiller m_noise;

        struct tc_struct
        {
            mfxStatus sts;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
            do_not_use dnu_struct;
            mfxU32 platforms_n;
            // suppose 19 platforms can be mentioned
            HWType platform[19];
        };

        static const tc_struct test_case[];

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            mfxFrameAllocator* pfa = (mfxFrameAllocator*)m_spool_in.GetAllocator();
            m_noise.tsSurfaceProcessor::ProcessSurface(&s, pfa);

            return MFX_ERR_NONE;
        }

        bool isTcToSkip(const tc_struct& tc, tsExtBufType<mfxVideoParam>& par);
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        //// seems to be "negative" test; needs to be reviewed
        //{/*00*/ MFX_ERR_INVALID_VIDEO_PARAM,
        //{
        //    { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,       0 },
        //},
        //},
        {/*00*/ MFX_ERR_NONE,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,        20 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,        MFX_FOURCC_NV12 },
                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,       0 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_NV12 },
            },
        },

        {/*01*/ MFX_ERR_NONE,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,        0 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,        MFX_FOURCC_NV12 },
                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,       20 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_NV12 },
            },
        },

        {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,        20 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,        MFX_FOURCC_NV12 },
                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,       21 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,       MFX_FOURCC_NV12 },
            },
        },

        {/*03*/ MFX_ERR_INVALID_VIDEO_PARAM,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,      20 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,      MFX_FOURCC_NV12 },
                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,     19 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,     MFX_FOURCC_Y210 },
            },
        },
        {/*04*/ MFX_ERR_NONE,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,        20 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_NV12 },

                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,        10 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,        MFX_FOURCC_NV12 },
            },
        },

        {/*05*/ MFX_ERR_NONE,
            {
                { INIT, &tsStruct::mfxExtVppMctf.FilterStrength,        15 },
                { INIT, &tsStruct::mfxVideoParam.vpp.Out.FourCC,         MFX_FOURCC_NV12 },

                { RESET, &tsStruct::mfxExtVppMctf.FilterStrength,        4 },
                { RESET, &tsStruct::mfxVideoParam.vpp.Out.FourCC,        MFX_FOURCC_NV12 },
            },
        },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);


    void SetParamsDoNotUse(tsExtBufType<mfxVideoParam>& par, const TestSuite::do_not_use dnu)
    {
        if (dnu.alg_num != 0)
        {
            mfxExtVPPDoNotUse* vpp_dnu;
            par.AddExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE, sizeof(mfxExtVPPDoNotUse));
            vpp_dnu = (mfxExtVPPDoNotUse*)par.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);
            vpp_dnu->NumAlg = dnu.alg_num;
            vpp_dnu->AlgList = new mfxU32[dnu.alg_num];

            memset(vpp_dnu->AlgList, 0, sizeof(mfxU32)*vpp_dnu->NumAlg);

            for (mfxU32 i = 0; i < dnu.alg_num; i++)
                vpp_dnu->AlgList[i] = dnu.alg_list[i];
        }
    }

    void CreateEmptyBuffers(tsExtBufType<mfxVideoParam>& par, tsExtBufType<mfxVideoParam>& pattern)
    {
        for (mfxU32 i = 0; i < pattern.NumExtParam; i++)
        {
            if (pattern.ExtParam[i]->BufferId != MFX_EXTBUFF_VPP_DONOTUSE)
                par.AddExtBuffer(pattern.ExtParam[i]->BufferId, pattern.ExtParam[i]->BufferSz);
        }
    }

    bool TestSuite::isTcToSkip(const tc_struct& tc, tsExtBufType<mfxVideoParam>& par)
    {
        if (par.vpp.In.FourCC == MFX_FOURCC_RGB4)
        {
            for (auto& sp : tc.set_par)
            {
                if (!sp.f)
                    break;

                if (sp.f->name.find("mfxExtVPPDenoise") != std::string::npos
                    || sp.f->name.find("mfxExtVPPDetail") != std::string::npos
                    || sp.f->name.find("mfxExtVPPProcAmp") != std::string::npos)
                {
                    g_tsLog << "Filter incompatible with RGB - skip test case\n";
                    return true;
                }
            }
        }

        if (tc.sts == MFX_ERR_NONE
            || !CompareParams<const tc_struct&, decltype(RESET), decltype(par)>(tc, par, RESET, LOG_NOTHING))
            return false;

        g_tsLog << "Init parameters not changed - skip test case\n";
        return true;
    }

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        const tc_struct& tc = test_case[id];

        tsExtBufType<mfxVideoParam> def(m_par);

        SETPARS(&m_par, INIT);

        if (isTcToSkip(tc, m_par))
            throw tsSKIP;

        MFXInit();

        CreateAllocators();
        SetFrameAllocator();
        SetHandle();

        g_tsStatus.expect(MFX_ERR_NONE);
        Init(m_session, &m_par);

        ////added from HEAD - begin
        //SETPARS(&m_par, RESET);
        //g_tsStatus.expect(tc.sts);
        //Reset(m_session, &m_par);
        //// end

        tsExtBufType<mfxVideoParam> par_init(def);
        CreateEmptyBuffers(*&par_init, *&m_par);

        g_tsStatus.expect(MFX_ERR_NONE);
        GetVideoParam(m_session, &par_init);

        tsExtBufType<mfxVideoParam> par_reset(def);
        SETPARS(&par_reset, RESET);
        SetParamsDoNotUse(*&par_reset, tc.dnu_struct);

        //adjust expected status for
        mfxStatus expected = tc.sts;
        if (tc.platforms_n && MFX_HW_UNKNOWN != g_tsHWtype)
        {
            printf("g_tsHWtype: %d\n", g_tsHWtype);
            bool ValidHWPlatform(false);
            for (mfxU32 i = 0; i < tc.platforms_n; ++i)
            {
                if (MFX_HW_UNKNOWN == tc.platform[i])
                    continue;

                if (g_tsHWtype == tc.platform[i])
                {
                    ValidHWPlatform = true;
                    break;
                }
            };
            if (!ValidHWPlatform)
                expected = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        g_tsStatus.expect(expected);
        Reset(m_session, &par_reset);

        if (MFX_ERR_NONE == expected) // GetVideoParam checks are valid only in case of succesfull reset
        {
            EXPECT_EQ(m_par, par_init) << "ERROR: Init parameters and parameters from GetVideoParams are not equal\n";

            tsExtBufType<mfxVideoParam> par_after_reset(def);
            CreateEmptyBuffers(*&par_after_reset, *&par_init);
            CreateEmptyBuffers(*&par_after_reset, *&par_reset);

            g_tsStatus.expect(MFX_ERR_NONE);
            GetVideoParam(m_session, &par_after_reset);

            mfxExtVPPDoNotUse* dnu = (mfxExtVPPDoNotUse*)par_reset.GetExtBuffer(MFX_EXTBUFF_VPP_DONOTUSE);

            for (mfxU32 i = 0; i< par_init.NumExtParam; i++)
            {
                mfxExtBuffer* reset = 0;
                mfxExtBuffer* after_reset = 0;

                reset = par_reset.GetExtBuffer(par_init.ExtParam[i]->BufferId);
                after_reset = par_after_reset.GetExtBuffer(par_init.ExtParam[i]->BufferId);

                // Creating empty buffer
                def.AddExtBuffer(par_init.ExtParam[i]->BufferId, par_init.ExtParam[i]->BufferSz);
                mfxExtBuffer* empty = def.GetExtBuffer(par_init.ExtParam[i]->BufferId);

                bool in_dnu = false;
                if (dnu != 0)
                {
                    for (mfxU32 j = 0; j < dnu->NumAlg; j++)
                    {
                        if (par_init.ExtParam[i]->BufferId == dnu->AlgList[j])
                        {
                            in_dnu = true;
                            break;
                        }
                    }
                }

                EXPECT_FALSE(in_dnu && (0 != memcmp(empty, after_reset, std::max(empty->BufferSz, after_reset->BufferSz))))
                    << "ERROR: Filter from Init was disabled in Reset but exists after Reset \n";

                EXPECT_FALSE(reset != 0 && (0 != memcmp(reset, after_reset, std::max(reset->BufferSz, after_reset->BufferSz))))
                    << "ERROR: Filter from Init was changed in Reset but parameters from Reset and after Reset are not equal \n";

                EXPECT_FALSE(!in_dnu && reset == 0 && (0 != memcmp(par_init.ExtParam[i], after_reset, std::max(par_init.ExtParam[i]->BufferSz, after_reset->BufferSz))))
                    << "ERROR: Filter's parameters from Init and after Reset are not equal \n";
            }

            for (mfxU32 i = 0; i< par_reset.NumExtParam; i++)
            {
                if (par_reset.ExtParam[i]->BufferId == MFX_EXTBUFF_VPP_DONOTUSE) continue; // VPP should not configure mfxExtVPPDoNotUse buffer!

                mfxExtBuffer* after_reset = 0;
                after_reset = par_after_reset.GetExtBuffer(par_reset.ExtParam[i]->BufferId);

                EXPECT_FALSE(after_reset == 0) << "ERROR: Filter specified in Reset does not exists after Reset \n";

                EXPECT_FALSE(0 != memcmp(par_reset.ExtParam[i], after_reset, std::max(par_reset.ExtParam[i]->BufferSz, after_reset->BufferSz)))
                    << "ERROR: Filter's parameters from Reset and after Reset are not equal \n";
            }

            if (dnu && dnu->AlgList) delete[] dnu->AlgList;
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(vpp_mctf_reset);
}