/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.

File Name: vpp_composition.cpp
\* ****************************************************************************** */

/*!
\file vpp_composition.cpp
\brief Gmock test vpp_composition

Description:
This suite tests VPP\n

Algorithm:
- Initialize session
- Allocate ext buffers
- Call Query()
- Check returned status
- Call Init()
- Check returned status

*/
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace vpp_composition
{

    enum
    {
        MFX_PAR  = 1,
        COMP_PAR = 2,
    };

    //! The number of max algs in mfxExtVPPDoUse structure
    const mfxU32 MAX_ALG_NUM = 3;

    /*!\brief Structure of test suite parameters*/
    struct tc_struct
    {
        //! Expected Query() return status
        mfxStatus q_sts;
        //! Expected Init() return status
        mfxStatus i_sts;

        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;

        }set_par[MAX_NPARS];
        //! Array with algs for mfxExtVPPDoUse structure
        mfxU32 algs[MAX_ALG_NUM];

    };

    //!\brief Main test class
    class TestSuite : protected tsVideoVPP
    {
    public:
        //! \brief A constructor
        TestSuite(): tsVideoVPP() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        int RunTest(const tc_struct& tc);
        //! \brief Sets required External buffers, return ID of extbuff allocated
        //! \param tc - test case params structure
        mfxU32 setExtBuff(tc_struct tc);
        //! \brief Deallocate external buffer 
        //! \param type - external buffer ID
        void cleanExtBuff(mfxU32 type);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
    };

    mfxU32 TestSuite::setExtBuff(tc_struct tc)
    {
        mfxExtVPPDoUse* ext_douse = (mfxExtVPPDoUse*) m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
        if(ext_douse != nullptr) {
            ext_douse->AlgList = new mfxU32[ext_douse->NumAlg];
            memcpy(ext_douse->AlgList, tc.algs, sizeof(mfxU32)*ext_douse->NumAlg);
            return MFX_EXTBUFF_VPP_DOUSE;
        }

        mfxExtVPPComposite* ext_composite = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);
        if (ext_composite != nullptr) {
            ext_composite->InputStream = new mfxVPPCompInputStream[ext_composite->NumInputStream];
            int x = 0, y = 0;
            for (int i = 0; i < ext_composite->NumInputStream; i++) {
                mfxVPPCompInputStream* str = &(ext_composite->InputStream[i]);
                SETPARS(str, COMP_PAR);
                if ((str->DstX == 0) && (str->DstY == 0)) {
                    str->DstX = str->DstW * x;
                    str->DstY = str->DstH * y;
                    x++;
                    if (str->DstW * (x + 1) > m_par.mfx.FrameInfo.Width) {
                        x = 0;
                        y++;
                    }
                }
                str = nullptr;

            }
            return MFX_EXTBUFF_VPP_COMPOSITE;
        }
        return 0;
    }

    void TestSuite::cleanExtBuff(mfxU32 type)
    {
        switch(type) {
            case MFX_EXTBUFF_VPP_DOUSE:
            {
                mfxExtVPPDoUse* ext_douse = (mfxExtVPPDoUse*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_DOUSE);
                delete[] ext_douse->AlgList;
                break;
            }
            case MFX_EXTBUFF_VPP_COMPOSITE:
            {
                mfxExtVPPComposite* ext_composite = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);
                delete[] ext_composite->InputStream;
                break;
            }
        }
    }
  
  const tc_struct TestSuite::test_case[] =
    {
        {/*00*/MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_INVALID_VIDEO_PARAM,
            {{MFX_PAR, &tsStruct::mfxExtVPPDoUse.NumAlg, 1}},
            { MFX_EXTBUFF_VPP_COMPOSITE }
        },

        {/*01*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50},
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 721 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 481 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*02*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 2 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 2 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 719 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 479 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*03*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 1 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 5 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*04*/ MFX_ERR_NONE, MFX_ERR_NONE,
        { { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, 720 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, 480 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW, 720 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH, 480 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 64 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 90 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 60 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

        {/*05*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM,
        { { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.Width, 720 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.Height, 480 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.CropW, 720 },
          { MFX_PAR,  &tsStruct::mfxVideoParam.vpp.Out.CropH, 480 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.NumInputStream, 160 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.Y, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.U, 50 },
          { MFX_PAR,  &tsStruct::mfxExtVPPComposite.V, 50 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstX, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstY, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstW, 45 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.DstH, 48 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.GlobalAlphaEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.LumaKeyEnable, 0 },
          { COMP_PAR, &tsStruct::mfxVPPCompInputStream.PixelAlphaEnable, 0 },
        }
        },

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        return RunTest(test_case[id]);
    }

    int TestSuite::RunTest(const tc_struct& tc)
    {
        TS_START;
        MFXInit();

        SETPARS(&m_par, MFX_PAR);
        mfxU32 MFX_PAR_type = setExtBuff(tc);

        mfxStatus sts_query = tc.q_sts,
                  sts_init  = tc.i_sts;

        if (   g_tsHWtype < MFX_HW_CNL
            && (   (MFX_FOURCC_UYVY == m_par.vpp.In.FourCC || MFX_FOURCC_UYVY == m_par.vpp.Out.FourCC)
                || (MFX_FOURCC_AYUV == m_par.vpp.In.FourCC || MFX_FOURCC_AYUV == m_par.vpp.Out.FourCC)
                || (MFX_FOURCC_P010 == m_par.vpp.In.FourCC || MFX_FOURCC_P010 == m_par.vpp.Out.FourCC)
                || (MFX_FOURCC_Y210 == m_par.vpp.In.FourCC || MFX_FOURCC_Y210 == m_par.vpp.Out.FourCC)
                || (MFX_FOURCC_Y410 == m_par.vpp.In.FourCC || MFX_FOURCC_Y410 == m_par.vpp.Out.FourCC)))
        {
            if ((m_par.vpp.In.FourCC == MFX_FOURCC_AYUV || m_par.vpp.Out.FourCC == MFX_FOURCC_AYUV)
                && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS && g_tsImpl & MFX_IMPL_VIA_D3D11)
            {
                // AYUV is supported on windows with DX11
            }
            else
            {
                sts_query = MFX_ERR_UNSUPPORTED;
                sts_init  = MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
        if ((MFX_PAR_type == MFX_EXTBUFF_VPP_COMPOSITE) && (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS) && ((g_tsImpl & 0x0F00) != MFX_IMPL_VIA_D3D11))
        {
            mfxExtVPPComposite* ext_composite = (mfxExtVPPComposite*)m_par.GetExtBuffer(MFX_EXTBUFF_VPP_COMPOSITE);
            if (ext_composite->NumInputStream > 8)
            {
                sts_query = MFX_ERR_UNSUPPORTED;
                sts_init = MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }

        g_tsStatus.expect(sts_query);
        Query();

        g_tsStatus.expect(sts_init);
        Init();

        cleanExtBuff(MFX_PAR_type);
        Close();

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(vpp_composition);
}

namespace vpp_rext_composition
{
    using namespace tsVPPInfo;

    template<eFmtId FmtID>
    class TestSuite : public vpp_composition::TestSuite
    {
    public:
        TestSuite()
            : vpp_composition::TestSuite()
        {}

        int RunTest(unsigned int id)
        {
            m_par.vpp.In.FourCC         = m_par.vpp.Out.FourCC         = Formats[FmtID].FourCC;
            m_par.vpp.In.BitDepthLuma   = m_par.vpp.Out.BitDepthLuma   = Formats[FmtID].BdY;
            m_par.vpp.In.BitDepthChroma = m_par.vpp.Out.BitDepthChroma = Formats[FmtID].BdC;
            m_par.vpp.In.ChromaFormat   = m_par.vpp.Out.ChromaFormat   = Formats[FmtID].ChromaFormat;
            m_par.vpp.In.Shift          = m_par.vpp.Out.Shift          = Formats[FmtID].Shift;

            vpp_composition::tc_struct tc = test_case[id];
            if (MFX_ERR_UNSUPPORTED == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_ERR_INVALID_VIDEO_PARAM;
                tc.q_sts = MFX_ERR_UNSUPPORTED;
            }
            else if (!tc.i_sts && MFX_WRN_PARTIAL_ACCELERATION == CCSupport()[FmtID][FmtID])
            {
                tc.i_sts = MFX_WRN_PARTIAL_ACCELERATION;
                tc.q_sts = MFX_WRN_PARTIAL_ACCELERATION;
            }

            return  vpp_composition::TestSuite::RunTest(tc);
        }
    };

#define REG_TEST(NAME, FMT_ID)                                     \
    namespace NAME                                                 \
    {                                                              \
        typedef vpp_rext_composition::TestSuite<FMT_ID> TestSuite; \
        TS_REG_TEST_SUITE_CLASS(NAME);                             \
    }

    REG_TEST(vpp_8b_420_yv12_composition, FMT_ID_8B_420_YV12);
    REG_TEST(vpp_8b_422_uyvy_composition, FMT_ID_8B_422_UYVY);
    REG_TEST(vpp_8b_422_yuy2_composition, FMT_ID_8B_422_YUY2);
    REG_TEST(vpp_8b_444_ayuv_composition, FMT_ID_8B_444_AYUV);
    REG_TEST(vpp_8b_444_rgb4_composition, FMT_ID_8B_444_RGB4);
    REG_TEST(vpp_10b_420_p010_composition, FMT_ID_10B_420_P010);
    REG_TEST(vpp_10b_422_y210_composition, FMT_ID_10B_422_Y210);
    REG_TEST(vpp_10b_444_y410_composition, FMT_ID_10B_444_Y410);
    REG_TEST(vpp_12b_420_p016_composition, FMT_ID_12B_420_P016);
    REG_TEST(vpp_12b_422_y216_composition, FMT_ID_12B_422_Y216);
    REG_TEST(vpp_12b_444_y416_composition, FMT_ID_12B_444_Y416);
#undef REG_TEST
}