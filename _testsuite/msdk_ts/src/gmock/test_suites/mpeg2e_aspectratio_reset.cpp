/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace mpeg2e_aspectratio_reset
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2), m_session() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxSession m_session;

    enum
    {
        MFXINIT = 1,
        MFXRESET
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#endif

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200 },
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(200)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},

    {/* 1*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,198},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(198)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)}}},

    {/* 2*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,196},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(196)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,16},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,9},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,180 },
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,180 },
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(180)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(180)}}},

    {/* 3*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,198},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(198)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,180},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(180)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},

    {/* 4*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1920},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,1080},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1920)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(1080)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)}}},

    {/* 5*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1920},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,1080},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1920)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(1080)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,221},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,100},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,500},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,500},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(500)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(500)}}},

    {/* 6*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)}}},

    {/* 7*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,600},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,450},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(600)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(450)}}},

    {/* 8*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,352},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,288},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(352)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(288)}}},

    {/* 9*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,663},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,400},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(663)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(400)}}},

    {/*10*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,194},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(194)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)}}},

    {/*11*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,194},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(194)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,16},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,9},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,150},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(150)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class Verifier : public tsBitstreamProcessor, public tsParserMPEG2AU
{
    mfxU16 m_aspectratioW;
    mfxU16 m_aspectratioH;
    mfxU16 width;
    mfxU16 height;
public:
    Verifier() : m_aspectratioW(0), m_aspectratioH(0), width(0), height(0) {}
    mfxStatus Init(mfxU16 ar_w, mfxU16 ar_h, mfxU16 w, mfxU16 h)
    {
        m_aspectratioW = ar_w;
        m_aspectratioH = ar_h;
        width          = w;
        height         = h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer(bs);

        while (1)
        {
            BSErr sts = parse_next_unit();
            if (sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if (sts) break;
        }

        bs.DataLength = 0;

        BS_MPEG2_header* pHdr = (BS_MPEG2_header*)get_header();
        if (pHdr->seq_hdr != 0)
        {
            mfxU8 aspectratio_info = 0;
            aspectratio_info |= pHdr->seq_hdr->aspect_ratio_information;
            if (aspectratio_info < 1 || aspectratio_info > 4)
            {
                g_tsLog << "ERROR: not defined aspect_ratio_information :" << aspectratio_info << "\n";
                return MFX_ERR_ABORTED;
            }

            if (aspectratio_info == 1)
            {
                if ((m_aspectratioW == 1 && m_aspectratioH == 1) || (m_aspectratioW == 0 && m_aspectratioH == 0))
                {
                    return MFX_ERR_NONE;
                }
                else
                {
                    g_tsLog << "ERROR: aspect_ratio_information is not set correctly." << "\n";
                    return MFX_ERR_ABORTED;
                }
            }

            mfxU64 k = ((mfxU64)(m_aspectratioW != 0 ? m_aspectratioW : 1) * width * 1000) / ((m_aspectratioH != 0 ? m_aspectratioH : 1) * height);
            
            //according to Table 6-3 in ITU-T H.262
            if (k >= 1332 && k <= 1334)
            {
                if (aspectratio_info == 2)
                {
                    return MFX_ERR_NONE;
                }
                else
                {
                    g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 2 here. " << "\n";
                    return MFX_ERR_ABORTED;
                }
            }
            else if (k >= 1776 && k <= 1778)
            {
                if (aspectratio_info == 3)
                {
                    return MFX_ERR_NONE;
                }
                else
                {
                    g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 3 here. " << "\n";
                    return MFX_ERR_ABORTED;
                }
            }
            else if (k >= 2209 && k <= 2211)
            {
                if (aspectratio_info == 4)
                {
                    return MFX_ERR_NONE;
                }
                else
                {
                    g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 4 here. " << "\n";
                    return MFX_ERR_ABORTED;
                }
            }
            else
            {
                g_tsLog << "ERROR: aspect_ratio_information is not set correctly." << "\n";
                return MFX_ERR_ABORTED;
            }
        }
        return MFX_ERR_NONE;
    }
};

class ResChange : public tsSurfaceProcessor
{
private:
    mfxU32 m_dynamic_width, m_dynamic_height;
public:
    ResChange() : m_dynamic_width(0),m_dynamic_height(0) {}
    mfxStatus Init(mfxU16 res_w, mfxU16 res_h)
    {
        m_dynamic_width = res_w;
        m_dynamic_height = res_h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if(m_dynamic_width == 0 && m_dynamic_height == 0)
        {
            return MFX_ERR_NONE;
        }
        if(s.Info.CropW != m_dynamic_width)
        {
            s.Info.Width = MSDK_ALIGN16(m_dynamic_width);
            s.Info.CropW = m_dynamic_width;
        }
        if(s.Info.CropH != m_dynamic_height)
        {
            s.Info.Height = MSDK_ALIGN16(m_dynamic_height);
            s.Info.CropH = m_dynamic_height;
        }
        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    int nframes = g_tsConfig.sim ? 3 : 10;

    MFXInit();

    //default frame size is different for hw and sim - set explicitly
    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 720;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;

    SETPARS(&m_par, MFXINIT);
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * nframes);
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE; // if PicStruct is not progressive, the FrameInfo.Height should be aligned by 32 but not 16.
    Init();

    Verifier verifier;
    m_bs_processor = &verifier;
    ResChange resolution;
    m_filler = &resolution;

    verifier.Init(m_pPar->mfx.FrameInfo.AspectRatioW, m_pPar->mfx.FrameInfo.AspectRatioH,
        (m_pPar->mfx.FrameInfo.CropW != 0) ? m_pPar->mfx.FrameInfo.CropW : m_pPar->mfx.FrameInfo.Width,
        (m_pPar->mfx.FrameInfo.CropH != 0) ? m_pPar->mfx.FrameInfo.CropH : m_pPar->mfx.FrameInfo.Height);

    EncodeFrames(nframes);

    bool b_reset = false;
    for(unsigned int i = 0; i < MAX_NPARS; i++)
    {
        if(tc.set_par[i].ext_type & MFXRESET)
        {
            b_reset = true;
            break;
        }
    }
    if(b_reset) {
        SETPARS(&m_par, MFXRESET);
        resolution.Init(m_pPar->mfx.FrameInfo.Width, m_pPar->mfx.FrameInfo.Height);
        verifier.Init(m_pPar->mfx.FrameInfo.AspectRatioW,m_pPar->mfx.FrameInfo.AspectRatioH,
            (m_pPar->mfx.FrameInfo.CropW != 0) ? m_pPar->mfx.FrameInfo.CropW : m_pPar->mfx.FrameInfo.Width,
            (m_pPar->mfx.FrameInfo.CropH != 0) ? m_pPar->mfx.FrameInfo.CropH : m_pPar->mfx.FrameInfo.Height);

        Reset();
        EncodeFrames(nframes);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_aspectratio_reset);
};
