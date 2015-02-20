#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_chromaloc
{
class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);

    static const mfxU32 max_num_ctrl = 11;
    static const mfxU32 max_num_ctrl_par = 4;
    static const unsigned int n_cases;
private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        MFXPAR = 1,
        CHROMALOC,
    };

    enum
    {
        INIT  = 1,
        QUERY = 1 << 1,
        RESET = 1 << 2,
        VUI   = 1 << 3,
        OFF_CHROMA   = 1 << 4
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
    };

    static const tc_struct test_case[];
};

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU16 chroma_loc_present;
    mfxU16 chroma_type_bottom;
    mfxU16 chroma_type_top;
    mfxU16 cod2_vui;

public:
    BsDump() :
        tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
    {
        set_trace_level(0);
    }
    ~BsDump() {}

    mfxStatus Init(mfxExtChromaLocInfo& chromaloc, mfxU16 vui)
    {
        chroma_loc_present = chromaloc.ChromaLocInfoPresentFlag;
        chroma_type_bottom = chromaloc.ChromaSampleLocTypeBottomField;
        chroma_type_top = chromaloc.ChromaSampleLocTypeTopField;
        cod2_vui = vui;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);
        UnitType& au = ParseOrDie();


        for (Bs32u i = 0; i < au.NumUnits; i++)
        {
            if (!(au.NALU[i].nal_unit_type == 0x07))
            {
                continue;
            }

            if (cod2_vui == 1)
            {
                if (au.NALU[i].SPS->vui_parameters_present_flag != 0)
                {
                    byte fl = au.NALU[i].SPS->vui_parameters_present_flag;
                    g_tsLog << "ERROR: vui_parameters_present_flag is " << fl 
                             << " (expected 0)\n";
                    return MFX_ERR_ABORTED;
                }
            }
            else
            {
                if (au.NALU[i].SPS->vui_parameters_present_flag != 1)
                {
                    byte fl = au.NALU[i].SPS->vui_parameters_present_flag;
                    g_tsLog << "ERROR: vui_parameters_present_flag is " << fl 
                             << " (expected 1)\n";
                    return MFX_ERR_ABORTED;
                }

                if (au.NALU[i].SPS->vui->chroma_loc_info_present_flag != chroma_loc_present)
                {
                    byte fl = au.NALU[i].SPS->vui->chroma_loc_info_present_flag;
                    g_tsLog << "ERROR: chroma_loc_info_present_flag is " << fl
                            << " (expected " << chroma_loc_present
                            << ")\n";
                    return MFX_ERR_ABORTED;
                }

                if (au.NALU[i].SPS->vui->chroma_sample_loc_type_bottom_field != chroma_type_bottom)
                {
                    byte fl = au.NALU[i].SPS->vui->chroma_sample_loc_type_bottom_field;
                    g_tsLog << "ERROR: chroma_sample_loc_type_bottom_field is " << fl
                            << " (expected " << chroma_type_bottom
                            << ")\n";
                    return MFX_ERR_ABORTED;
                }

                if (au.NALU[i].SPS->vui->chroma_sample_loc_type_top_field != chroma_type_top)
                {
                    byte fl = au.NALU[i].SPS->vui->chroma_sample_loc_type_top_field;
                    g_tsLog << "ERROR: chroma_sample_loc_type_top_field != " << fl
                            << " (expected " << chroma_type_top
                            << ")\n";
                    return MFX_ERR_ABORTED;
                }
            }
        }

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // just attach buffer, default values
    {/* 0*/ MFX_ERR_NONE, INIT, {}
    },
    // ChromaLoc on, fields = 5; 5
    {/* 1*/ MFX_ERR_NONE, INIT, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    // ChromaLoc on, fields = 0; 0
    {/* 2*/ MFX_ERR_NONE, INIT, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 0},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 0}
       },
    },
    // ChromaLoc off, fields = 5; 5
    {/* 3*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 0},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    // ChromaLoc on, invalid fields = 2; 2
    {/* 4*/ MFX_ERR_NONE, INIT, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 2},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 2}
       },
    },
    // ChromaLoc on, fields = 5; 5
    {/* 5*/ MFX_ERR_NONE, INIT|QUERY, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    // ChromaLoc on, fields = 0; 0
    {/* 6*/ MFX_ERR_NONE, INIT|QUERY, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 0},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 0}
       },
    },
    // ChromaLoc off, fields = 5; 5
    {/* 7*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT|QUERY, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 0},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    // ChromaLoc on, invalid fields = 2; 2
    {/* 8*/ MFX_ERR_NONE, INIT|QUERY, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 2},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 2}
       },
    },

    // Cases with Reset
    // ChromaLoc on, fields = 5; 5
    {/* 9*/ MFX_ERR_NONE, INIT|RESET, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    // ChromaLoc on, fields = 0; 0
    {/*10*/ MFX_ERR_NONE, INIT|RESET, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 0},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 0}
       },
    },

    // Cases with DisableVUI
    {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT|QUERY|VUI, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT|RESET|VUI, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
    {/*13*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT|RESET|OFF_CHROMA|VUI, {
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaLocInfoPresentFlag, 1},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeTopField, 5},
        {CHROMALOC, &tsStruct::mfxExtChromaLocInfo.ChromaSampleLocTypeBottomField, 5}
       },
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    std::vector<mfxExtBuffer*> buffs;
    std::vector<mfxExtBuffer*> buffs_cpy;

    MFXInit();

    BsDump bs;
    m_bs_processor = &bs;

    mfxExtChromaLocInfo chroma = {0};
    chroma.Header.BufferId = MFX_EXTBUFF_CHROMA_LOC_INFO;
    chroma.Header.BufferSz = sizeof(mfxExtChromaLocInfo);
    SETPARS(&chroma, CHROMALOC);
    buffs.push_back((mfxExtBuffer*)&chroma);
    mfxExtChromaLocInfo chroma_cpy(chroma);
    buffs_cpy.push_back((mfxExtBuffer*)&chroma_cpy);

    mfxExtCodingOption2 cod2 = {0};
    cod2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    cod2.Header.BufferSz = sizeof(mfxExtCodingOption2);

    if (tc.mode & VUI)
    {
        cod2.DisableVUI = 1;
        mfxExtCodingOption2 cod2_cpy(cod2);
        buffs.push_back((mfxExtBuffer*)&cod2);
        buffs_cpy.push_back((mfxExtBuffer*)&cod2_cpy);
    }

    // no buffers were added to m_par before
    tsExtBufType<mfxVideoParam> par_cpy(m_par);

    // adding buffers
    if (buffs.size())
    {
        m_par.NumExtParam = par_cpy.NumExtParam = (mfxU16)buffs.size();
        m_par.ExtParam = &buffs[0];
        par_cpy.ExtParam = &buffs_cpy[0];
    }

    if (tc.mode & QUERY)
    {
        g_tsStatus.expect(tc.sts);
        Query(m_session, m_pPar, &par_cpy);
    }

    g_tsStatus.expect(tc.sts);
    if (tc.mode & INIT)
    {
        Init();
    }

    bs.Init(chroma, cod2.DisableVUI);

    if (tc.sts == MFX_ERR_NONE)
    {
        EncodeFrames(3);
    }

    if (tc.mode & RESET) 
    {
        for(mfxU32 i = 0; i < 2; i++)
        {
            g_tsStatus.expect(tc.sts);
            if (tc.mode & OFF_CHROMA)
            {
                ((mfxExtChromaLocInfo*)buffs[0])->ChromaLocInfoPresentFlag = 0;
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }
            else
            {
                if (chroma.ChromaSampleLocTypeBottomField == 0)
                {
                    ((mfxExtChromaLocInfo*)buffs[0])->ChromaSampleLocTypeBottomField = 5;
                    ((mfxExtChromaLocInfo*)buffs[0])->ChromaSampleLocTypeTopField = 5;
                }
                else
                {
                    ((mfxExtChromaLocInfo*)buffs[0])->ChromaSampleLocTypeBottomField = 0;
                    ((mfxExtChromaLocInfo*)buffs[0])->ChromaSampleLocTypeTopField = 0;
                }
            }
            bs.Init(*((mfxExtChromaLocInfo*)buffs[0]), cod2.DisableVUI);
            Reset(m_session, m_pPar);
            EncodeFrames(3);
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_chromaloc);
};
