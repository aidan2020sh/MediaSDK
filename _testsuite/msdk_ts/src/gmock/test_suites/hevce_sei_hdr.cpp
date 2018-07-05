/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#include <cmath>

namespace hevce_sei_hdr
{
    using namespace BS_HEVC;

    class TestSuite : tsVideoEncoder, tsSurfaceProcessor, tsBitstreamProcessor, tsParserHEVC
    {
    public:
        static const unsigned int n_cases;
        static const unsigned int n_frames = 20;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC(INIT_MODE_STORE_SEI_DATA)
            , m_noiser(10)
            , m_nframes(n_frames)
            , m_frameId(0)
        {
            m_filler = this;
            m_bs_processor = this;
        }
        ~TestSuite() {}

        int RunTest(unsigned int id);

    private:
#ifdef SEI_DEBUG
        tsBitstreamWriter* m_writer;
#endif
        mfxExtMasteringDisplayColourVolume displayColour;
        mfxExtContentLightLevelInfo lightLevel;
        std::vector<mfxExtBuffer*> m_rtExtParams;
        std::vector<mfxExtBuffer*> m_parExtParams;
        tsNoiseFiller m_noiser;
        mfxU32 m_nframes;
        mfxU32 m_frameId;
        mfxU32 dynamicChange;
        mfxStatus ProcessSurface(mfxFrameSurface1&);
        mfxStatus ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames);
        enum CTRL_TYPE
        {
              INIT = 0x1
            , QUERY = 0x2
            , RESET = 0x4
            , STATIC = 0x8
            , DYNAMIC_ALL = 0x10
            , DYNAMIC_PART = 0x20
        };
        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 type;
            struct {
                mfxU16 InsertPayloadToggle;
                mfxU16 DisplayPrimariesX[3];
                mfxU16 DisplayPrimariesY[3];
                mfxU16 WhitePointX;
                mfxU16 WhitePointY;
                mfxU32 MaxDisplayMasteringLuminance;
                mfxU32 MinDisplayMasteringLuminance;
            } masteringDisplayColourVolume;
            struct {
                mfxU16 InsertPayloadToggle;
                mfxU16 MaxContentLightLevel;
                mfxU16 MaxPicAverageLightLevel;
            } contentLightLevelInfo;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxI32 v;
            } set_par[MAX_NPARS];
        };
        enum
        {
            MFX_PAR
        };
        static const tc_struct test_case[];
    };

    mfxU16 compareDisplayColourVolumeData(mfxU8* res, mfxExtMasteringDisplayColourVolume& src) {
        mfxU16 it = 0;
        mfxU16 difference = 0;
        for (int i = 0; i < 3; i++) {
            if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.DisplayPrimariesX[i])
                difference++;
            it += 2;
            if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.DisplayPrimariesY[i])
                difference++;
            it += 2;
        }
        if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.WhitePointX)
            difference++;
        it += 2;
        if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.WhitePointY)
            difference++;
        it += 2;
        if ((res[it] * std::pow(2, 24)) + (res[it + 1] * std::pow(2, 16)) + (res[it + 2] * std::pow(2, 8)) + res[it + 3] != src.MaxDisplayMasteringLuminance)
            difference++;
        it += 4;
        if ((res[it] * std::pow(2, 24)) + (res[it + 1] * std::pow(2, 16)) + (res[it + 2] * std::pow(2, 8)) + res[it + 3] != src.MinDisplayMasteringLuminance)
            difference++;

        return difference;
    }

    mfxU16 compareContentLightLevelInfo(mfxU8* res, mfxExtContentLightLevelInfo& src) {
        mfxU16 it = 0;
        mfxU16 difference = 0;
        if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.MaxContentLightLevel)
            difference++;
        it += 2;
        if ((res[it] * std::pow(2, 8)) + res[it + 1] != src.MaxPicAverageLightLevel)
            difference++;
        return difference;
    }


    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& pSurf)
    {
        if (m_frameId > (m_nframes - 1))
            return MFX_ERR_NONE;

        bool insert = false;
        memset(&m_ctrl, 0, sizeof(m_ctrl));
        m_rtExtParams.clear();

        if (dynamicChange == 1) {
            insert = true;
        }
        else
            if (dynamicChange == 2) {
                if (m_frameId < m_nframes / 4 || m_frameId > m_nframes * 3 / 4) {
                    insert = true;
                }
            }
        if (insert) {
            m_rtExtParams.push_back((mfxExtBuffer *)& displayColour);
            m_rtExtParams.push_back((mfxExtBuffer *)& lightLevel);
            m_ctrl.ExtParam = &m_rtExtParams[0];
            m_ctrl.NumExtParam = (mfxU16)m_rtExtParams.size();
        }
        m_frameId++;
        m_noiser.ProcessSurface(pSurf);
        return MFX_ERR_NONE;
    }

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
    {
        SetBuffer0(bs);
#ifdef SEI_DEBUG
        m_writer->ProcessBitstream(bs, nFrames);
#endif
        auto& au = ParseOrDie();

        int foundSEI = 0;
        for (Bs32u au_ind = 0; au_ind < au.NumUnits; au_ind++) {
            auto nalu = au.nalu[au_ind];
            if (nalu->nal_unit_type == PREFIX_SEI_NUT) {
                for (Bs16u sei_ind = 0; sei_ind < nalu->sei->NumMessage; ++sei_ind) {
                    if (nalu->sei->message[sei_ind].payloadType == 137) {
                        foundSEI++;
                        mfxU8* res = nalu->sei->message[sei_ind].rawData;
                        EXPECT_EQ(0, compareDisplayColourVolumeData(res, displayColour))
                            << "Wrong MasteringDisplayColourVolume parametrs \n";
                    }
                    else if (nalu->sei->message[sei_ind].payloadType == 144) {
                        foundSEI++;
                        mfxU8* res = nalu->sei->message[sei_ind].rawData;
                        EXPECT_EQ(0, compareContentLightLevelInfo(res, lightLevel))
                            << "Wrong ContentLightLevelInfo parametrs \n";
                    }
                }
            }
        }
        if (m_rtExtParams.size() > 0)
            EXPECT_EQ(m_rtExtParams.size(), (unsigned int)foundSEI) << "Wrong count of SEI \n";
        else if (m_parExtParams.size() > 0) {
            mfxU16 size = 0;
            for (mfxU16 i = 0; i < m_parExtParams.size(); i++) {
                if (m_parExtParams[i]->BufferId == MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME) {
                    mfxExtMasteringDisplayColourVolume* src = (mfxExtMasteringDisplayColourVolume*)m_parExtParams[i];
                    if (src->InsertPayloadToggle == MFX_PAYLOAD_IDR && bs.FrameType & MFX_FRAMETYPE_IDR)
                        size++;
                }
                if (m_parExtParams[i]->BufferId == MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO) {
                    mfxExtContentLightLevelInfo* src = (mfxExtContentLightLevelInfo*)m_parExtParams[i];
                    if (src->InsertPayloadToggle == MFX_PAYLOAD_IDR && bs.FrameType & MFX_FRAMETYPE_IDR)
                        size++;
                }
            }
            EXPECT_EQ(size, foundSEI) << "Wrong count of SEI \n";
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ MFX_ERR_NONE,                     INIT | STATIC, { MFX_PAYLOAD_OFF, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 }, { MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*01*/ MFX_ERR_NONE,                     INIT | STATIC,{ MFX_PAYLOAD_IDR, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*02*/ MFX_ERR_NONE,                     INIT, { MFX_PAYLOAD_OFF, 50001, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*03*/ MFX_ERR_NONE, INIT | STATIC, { MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 300, 400, 65535, 100 }, { MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*04*/ MFX_ERR_NONE, INIT | STATIC, { MFX_PAYLOAD_OFF, 1, 0, 2, 200, 3, 300, 4, 400, 200, 200 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*05*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART, { MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 }, { MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*06*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART, { MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 }, { MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*07*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART, { MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 }, { MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*08*/ MFX_ERR_NONE, INIT | DYNAMIC_ALL,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*09*/ MFX_ERR_NONE, INIT | RESET | STATIC, { MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*10*/ MFX_ERR_NONE, INIT | RESET | STATIC, { MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*11*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*12*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*13*/ MFX_ERR_NONE, INIT | RESET | STATIC, { MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 2, 1 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP } },
        {/*14*/ MFX_ERR_NONE, INIT | STATIC,{ MFX_PAYLOAD_IDR, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*15*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*16*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*17*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR } },
        {/*18*/ MFX_ERR_NONE, INIT | STATIC,{ MFX_PAYLOAD_IDR, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*19*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*20*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*21*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR } },
        {/*22*/ MFX_ERR_NONE, INIT | STATIC,{ MFX_PAYLOAD_IDR, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
        {/*23*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
        {/*24*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
        {/*25*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_ICQ } },
        {/*26*/ MFX_ERR_NONE, INIT | STATIC,{ MFX_PAYLOAD_IDR, 50000, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
        {/*27*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_IDR, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_IDR, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
        {/*28*/ MFX_ERR_NONE, INIT | RESET | STATIC | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
        {/*29*/ MFX_ERR_NONE, INIT | DYNAMIC_PART,{ MFX_PAYLOAD_OFF, 1, 100, 2, 200, 3, 300, 4, 400, 200, 100 },{ MFX_PAYLOAD_OFF, 100, 150 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR } },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        srand(0);
        const tc_struct& tc = test_case[id];
        m_par.AsyncDepth = 1;
        m_par.mfx.GopRefDist = 1;
        SETPARS(m_par, MFX_PAR);
        set_brc_params(&m_par);
#ifdef SEI_DEBUG
        std::string name = "debug" + std::to_string(id);
        m_writer = new tsBitstreamWriter(name.c_str());
#endif

        MFXInit();
        Load();

        memset(&displayColour, 0, sizeof(displayColour));
        memset(&lightLevel, 0, sizeof(lightLevel));
        displayColour.Header.BufferId = MFX_EXTBUFF_MASTERING_DISPLAY_COLOUR_VOLUME;
        displayColour.Header.BufferSz = sizeof(displayColour);
        displayColour.InsertPayloadToggle = tc.masteringDisplayColourVolume.InsertPayloadToggle;
        for (int i = 0; i < 3; i++) {
            displayColour.DisplayPrimariesX[i] = tc.masteringDisplayColourVolume.DisplayPrimariesX[i];
            displayColour.DisplayPrimariesY[i] = tc.masteringDisplayColourVolume.DisplayPrimariesY[i];
        }
        displayColour.WhitePointX = tc.masteringDisplayColourVolume.WhitePointX;
        displayColour.WhitePointY = tc.masteringDisplayColourVolume.WhitePointY;
        displayColour.MaxDisplayMasteringLuminance = tc.masteringDisplayColourVolume.MaxDisplayMasteringLuminance;
        displayColour.MinDisplayMasteringLuminance = tc.masteringDisplayColourVolume.MinDisplayMasteringLuminance;


        lightLevel.Header.BufferId = MFX_EXTBUFF_CONTENT_LIGHT_LEVEL_INFO;
        lightLevel.Header.BufferSz = sizeof(lightLevel);
        lightLevel.InsertPayloadToggle = tc.contentLightLevelInfo.InsertPayloadToggle;
        lightLevel.MaxContentLightLevel = tc.contentLightLevelInfo.MaxContentLightLevel;
        lightLevel.MaxPicAverageLightLevel = tc.contentLightLevelInfo.MaxPicAverageLightLevel;

        if (tc.type & STATIC) {
            m_parExtParams.push_back((mfxExtBuffer*)&displayColour);
            m_parExtParams.push_back((mfxExtBuffer*)&lightLevel);
            m_par.NumExtParam = m_parExtParams.size();
            m_par.ExtParam = &m_parExtParams[0];
        }
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.GopPicSize = 3;

        g_tsStatus.expect(tc.sts);
        mfxStatus checkSts = MFX_ERR_NONE;

        if (tc.type & INIT)
        {
            if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR && g_tsHWtype < MFX_HW_ICL)
            {
                g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
                g_tsLog << "WARNING: QVBR is unsupported on platforms older than ICL\n";
                checkSts = Init();
                return 0;
            }
            checkSts = Init();
        }

        if (checkSts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
            g_tsStatus.expect(tc.sts);
            Query();
        }

        AllocBitstream(1024 * 1024 * m_nframes);

        if (tc.type & DYNAMIC_ALL) {
            dynamicChange = 1;
        }
        else if (tc.type & DYNAMIC_PART) {
            dynamicChange = 2;
        }
        else dynamicChange = 0;

        EncodeFrames(m_nframes / 2, true);

        if (tc.type & RESET) {
            displayColour.DisplayPrimariesX[0] -= 1;
            displayColour.DisplayPrimariesY[2] += 1;
            displayColour.MinDisplayMasteringLuminance -= 1;
            lightLevel.MaxContentLightLevel += 1;
            checkSts = Reset();
            if (checkSts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
                g_tsStatus.expect(checkSts);
                Query();
            }
            EncodeFrames(m_nframes / 2, true);
        }
#ifdef SEI_DEBUG
        delete m_writer;
#endif
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_sei_hdr);
}
