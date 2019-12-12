/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_vpp.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define TEST_NAME feature_unsupported

namespace TEST_NAME
{

typedef struct {
    mfxU32 BufferId;
    mfxU32 BufferSz;
    std::string string;
} BufferIdToString;

#define EXTBUF(TYPE, ID) {ID, sizeof(TYPE), #TYPE},
static BufferIdToString g_StringsOfBuffers[] =
{
#include "ts_ext_buffers_decl.h"
};
#undef EXTBUF

void GetBufferIdSz(const std::string& name, mfxU32& bufId, mfxU32& bufSz)
{
    //constexpr size_t maxBuffers = g_StringsOfBuffers / g_StringsOfBuffers[0];
    const size_t maxBuffers = sizeof( g_StringsOfBuffers ) / sizeof( g_StringsOfBuffers[0] );

    const std::string& buffer_name = name.substr(0, name.find(":"));

    for(size_t i(0); i < maxBuffers; ++i)
    {
        //if( name.find(g_StringsOfBuffers[i].string) != std::string::npos )
        if( buffer_name == g_StringsOfBuffers[i].string )
        {
            bufId = g_StringsOfBuffers[i].BufferId;
            bufSz = g_StringsOfBuffers[i].BufferSz;

            return;
        }
    }
}

typedef void (*callback)(tsExtBufType<mfxVideoParam>*);

class TestSuite : public tsSession
{
public:
    TestSuite() {};
    ~TestSuite() {};
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct f_pair
    {
        const  tsStruct::Field* f;
        mfxU64 v;
        bool   isInvalid; //if true then check that this field was zero-ed by Query function
    };

private:
    enum
    {
        CORE   = 1,
        DECODE = 1 << 2,
        VPP    = 1 << 3,
        ENCODE = 1 << 4
    };

    typedef enum
    {
        NONE     = MFX_ERR_NONE,
        E_UNSPRT = MFX_ERR_UNSUPPORTED,
        E_INVLID = MFX_ERR_INVALID_VIDEO_PARAM,
        E_INCOMP = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM,
        W_PARACL = MFX_WRN_PARTIAL_ACCELERATION,
        W_INCOMP = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
    } shortSts;

    struct tc_struct
    {
        //string Descr;
        //string Tag;
        mfxU32 component;
        mfxU32 codec;
        union {
            shortSts  QuerySts;
            mfxStatus query_sts;
        };
        union {
            shortSts  InitSts;
            mfxStatus init_sts;
        };
        f_pair set_par[MAX_NPARS]; //main test parameters to be set
        callback func;             //additional parameters that needed to be set as a dependency to main, e.g. bitrate parameters for RateControl tests
    };
    template <typename T>
    int RunTestQueryInitComponent(const tc_struct& tc);

    static const tc_struct test_case[];
};

static const tsStruct::Field* AsyncDepth  (&tsStruct::mfxVideoParam.AsyncDepth);
static const tsStruct::Field* Protected   (&tsStruct::mfxVideoParam.Protected);
static const tsStruct::Field* CodecId     (&tsStruct::mfxVideoParam.mfx.CodecId);
static const tsStruct::Field* CodecProfile(&tsStruct::mfxVideoParam.mfx.CodecProfile);
static const tsStruct::Field* CodecLevel  (&tsStruct::mfxVideoParam.mfx.CodecLevel);
static const tsStruct::Field* RateCtrlMthd(&tsStruct::mfxVideoParam.mfx.RateControlMethod);
static const tsStruct::Field* Width       (&tsStruct::mfxVideoParam.mfx.FrameInfo.Width);
static const tsStruct::Field* Height      (&tsStruct::mfxVideoParam.mfx.FrameInfo.Height);
static const tsStruct::Field* mfxFourCC   (&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC);
static const tsStruct::Field* inFourCC    (&tsStruct::mfxVideoParam.vpp.In.FourCC);
static const tsStruct::Field* outFourCC   (&tsStruct::mfxVideoParam.vpp.Out.FourCC);
static const tsStruct::Field* ChromaFormat(&tsStruct::mfxVideoParam.mfx.FrameInfo.ChromaFormat);
static const tsStruct::Field* InitialDelayInKB(&tsStruct::mfxVideoParam.mfx.InitialDelayInKB);
static const tsStruct::Field* TargetKbps(&tsStruct::mfxVideoParam.mfx.TargetKbps);
static const tsStruct::Field* MaxKbps(&tsStruct::mfxVideoParam.mfx.MaxKbps);

const TestSuite::tc_struct TestSuite::test_case[] =
{//  id   component     codec          Query()   Init()    Parameters to set                            Additional parameters to set
    //MPEG2 rate controls
    {/*00*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA       , true}, set_brc_params },
    {/*01*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_HRD   , true}, set_brc_params },
    {/*02*/ ENCODE, MFX_CODEC_MPEG2, W_INCOMP, W_INCOMP, {{RateCtrlMthd, 0xffffffff, true}, // wrapper corrects unknown rate control value with warning
                                                        { InitialDelayInKB, 150, false },
                                                        { TargetKbps, 600, false },
                                                        { MaxKbps, 900, false }},
                                                        set_brc_params },
#if !(defined(_WIN32) || defined(_WIN64))
    //Protected content
    {/*03*/ DECODE, MFX_CODEC_AVC,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*04*/ DECODE, MFX_CODEC_MPEG2,  E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*05*/ DECODE, MFX_CODEC_VC1,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*06*/ DECODE, MFX_CODEC_JPEG,   E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*07*/ VPP,    0,                E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*08*/ ENCODE, MFX_CODEC_AVC,    E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*09*/ ENCODE, MFX_CODEC_MPEG2,  E_UNSPRT, E_INVLID, {Protected, 1, true} },
    {/*10*/ ENCODE, MFX_CODEC_JPEG,   E_UNSPRT, E_INVLID, {Protected, 1, true} },
    //Rate control
    {/*11*/ ENCODE, MFX_CODEC_MPEG2, NONE,     W_INCOMP,{ RateCtrlMthd, MFX_RATECONTROL_AVBR     , false }, set_brc_params },
    {/*12*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID,{ RateCtrlMthd, MFX_RATECONTROL_RESERVED1, true }, set_brc_params },
    {/*13*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID,{ RateCtrlMthd, MFX_RATECONTROL_RESERVED2, true }, set_brc_params },
    {/*14*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID,{ RateCtrlMthd, MFX_RATECONTROL_RESERVED3, true }, set_brc_params },
    {/*15*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID,{ RateCtrlMthd, MFX_RATECONTROL_RESERVED4, true }, set_brc_params },
    //FourCC
    {/*16*/ VPP,           0,               E_UNSPRT, E_INVLID,{ { inFourCC, MFX_FOURCC_AYUV, true },{ outFourCC, MFX_FOURCC_AYUV, true } }, set_chromaformat_vpp },
    //Fade Detection
    {/*17*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.FadeDetection, MFX_CODINGOPTION_ON, true }, },
    //{/**/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, { {&tsStruct::mfxExtCodingOptionVPS.VPSId,      0xFF },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBuffer,  (mfxU64) &g_StringsOfBuffers },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBufSize, (mfxU64) sizeof(g_StringsOfBuffers) } }},
    //WeightedPred, WeightedBiPred
    {/*18*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_DEFAULT }, },
    {/*19*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_EXPLICIT }, },
    {/*20*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedPred,   MFX_WEIGHTED_PRED_IMPLICIT }, },
    {/*21*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_DEFAULT }, },
    {/*22*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_EXPLICIT }, },
    {/*23*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID,{ &tsStruct::mfxExtCodingOption3.WeightedBiPred, MFX_WEIGHTED_PRED_IMPLICIT }, },
#endif
    {/*03 / 24*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_ICQ      , true}, set_brc_params },
    {/*04 / 25*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_VCM      , true}, set_brc_params },
    {/*05 / 26*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_ICQ   , true}, set_brc_params },
    {/*06 / 27*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_LA_EXT   , true}, set_brc_params },
    {/*07 / 28*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {RateCtrlMthd, MFX_RATECONTROL_QVBR     , true}, set_brc_params },

    //API 1.17
        //MFX_FOURCC_ABGR16
    {/*08 / 29*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_ABGR16, true}, set_chromaformat_mfx },
    {/*09 / 30*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_ABGR16, true}, set_chromaformat_mfx },
    {/*10 / 31*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_ABGR16, true}, {outFourCC, MFX_FOURCC_ABGR16, true} }, set_chromaformat_vpp },
        //MFX_FOURCC_AYUV
    {/*11 / 32*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV, true}, set_chromaformat_mfx },
    {/*12 / 33*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV, true}, set_chromaformat_mfx },
        //MFX_FOURCC_AYUV_RGB4
    {/*13 / 34*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV_RGB4, true}, set_chromaformat_mfx },
    {/*14 / 35*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_AYUV_RGB4, true}, set_chromaformat_mfx },
    {/*15 / 36*/ VPP,           0,               E_UNSPRT, E_INVLID, { {inFourCC, MFX_FOURCC_AYUV_RGB4, true}, {outFourCC, MFX_FOURCC_AYUV_RGB4, true} }, set_chromaformat_vpp },
        //MFX_FOURCC_UYVY
    {/*16 / 37*/ DECODE|ENCODE, MFX_CODEC_AVC,   E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_UYVY, true}, set_chromaformat_mfx },
    {/*17 / 38*/ DECODE|ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {mfxFourCC, MFX_FOURCC_UYVY, true}, set_chromaformat_mfx },
        //Unsupported ExtBuffer
    {/*18 / 39*/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active, 0, false }, },
    //{/**/ ENCODE, MFX_CODEC_MPEG2, E_UNSPRT, E_INVLID, { {&tsStruct::mfxExtCodingOptionVPS.VPSId,      0xFF },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBuffer,  (mfxU64) &g_StringsOfBuffers },
    //                                                       {&tsStruct::mfxExtCodingOptionVPS.VPSBufSize, (mfxU64) sizeof(g_StringsOfBuffers) } }},
    {/*19 / 40*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active,   1, false }, },
    {/*20 / 41*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active,   2, false }, },
    {/*21 / 42*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL0Active,   3, false }, },
    {/*22 / 43*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active,   0, false }, },
    {/*23 / 44*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active,   1, false }, },
    {/*24 / 45*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtAVCRefLists.NumRefIdxL1Active,   2, false }, },
    //AVC ROI, DirtyRect, MoveRect
    {/*25 / 46*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtEncoderROI.NumROI, 1} },
    {/*26 / 47*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtDirtyRect.NumRect, 1} },
    {/*27 / 48*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtMoveRect.NumRect,  1} },

    {/*28 / 49*/ ENCODE, MFX_CODEC_AVC,     NONE, NONE, {&tsStruct::mfxExtEncoderCapability.MBPerSec, 1},},
    {/*30 / 50*/ ENCODE, MFX_CODEC_MPEG2,   E_UNSPRT, E_INVLID, {&tsStruct::mfxExtEncoderCapability.MBPerSec, 1},},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

template <typename TB>
void SetParams(tsExtBufType<TB>& par, const TestSuite::f_pair pairs[], const size_t length = MAX_NPARS)
{
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f)
        {
            void* ptr = 0;


            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    par.AddExtBuffer(bufId, bufSz);
                    ptr = par.GetExtBuffer(bufId);
                }
            }

            tsStruct::set(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}

template <typename TB>
void CheckParams(tsExtBufType<TB>& par, const TestSuite::f_pair pairs[], const size_t length = MAX_NPARS)
{
    for(size_t i(0); i < length; ++i)
    {
        if(pairs[i].f && pairs[i].isInvalid)
        {
            void* ptr = 0;

            if((typeid(TB) == typeid(mfxVideoParam) && ( pairs[i].f->name.find("mfxVideoParam") != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxBitstream)  && ( pairs[i].f->name.find("mfxBitstream")  != std::string::npos) ) || 
               (typeid(TB) == typeid(mfxEncodeCtrl) && ( pairs[i].f->name.find("mfxEncodeCtrl") != std::string::npos) ))
            {
                ptr = (TB*) &par;
            }
            else
            {
                mfxU32 bufId = 0, bufSz = 0;
                GetBufferIdSz(pairs[i].f->name, bufId, bufSz);
                if(0 == bufId + bufSz)
                {
                    EXPECT_NE((mfxU32)0, bufId + bufSz) << "ERROR: (in test) failed to get Ext buffer Id or Size\n";
                    throw tsFAIL;
                }
                ptr = par.GetExtBuffer(bufId);
                if(!ptr)
                {
                    EXPECT_NE(nullptr, ptr) << "ERROR: extended buffer is missing!\n";
                    throw tsFAIL;
                }
            }

            tsStruct::check_ne(ptr, *pairs[i].f, pairs[i].v);
        }
    }
}


template <typename T>
int TestSuite::RunTestQueryInitComponent(const tc_struct& tc)
{
    T component;
    component.m_default = false;
    tsExtBufType<mfxVideoParam> queryParOut;
    tsExtBufType<mfxVideoParam>& queryParIn = component.m_par;
    if(typeid(T) != typeid(tsVideoVPP))
    {
        queryParIn.mfx.CodecId = tc.codec;
        if(MFX_CODEC_AVC == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_AVC_41;
        } else if (MFX_CODEC_MPEG2 == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_MPEG2_SIMPLE;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_MPEG2_LOW;
        } else if (MFX_CODEC_HEVC == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_HEVC_41;
        } else if (MFX_CODEC_VC1 == tc.codec) {
            queryParIn.mfx.CodecProfile = MFX_LEVEL_VC1_MEDIAN;
            queryParIn.mfx.CodecLevel = MFX_LEVEL_VC1_0;
        }
    }

    if(typeid(T) == typeid(tsVideoEncoder))
    {
        if (tc.codec == MFX_CODEC_MPEG2 && g_tsHWtype == MFX_HW_APL) {
            g_tsLog << "[ SKIPPED ] MPEG2 encode not supported on this platform\n";
            return 0;
        }
    }

    SetParams(queryParIn, tc.set_par);
    if(tc.func)
    {
        tc.func(&queryParIn);
    }
    const tsExtBufType<mfxVideoParam> tmpPar = queryParIn;
    queryParOut = tmpPar;
    component.MFXInit();


    //Query(session, in, out) test:
    if ((MFX_CODEC_AVC == tc.codec) && !(g_tsImpl & MFX_IMPL_VIA_D3D11) && (g_tsOSFamily != MFX_OS_FAMILY_LINUX) && (queryParIn.GetExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY)))
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);//MBPerSec supported only by D3D11 or VAAPI
    }
    else
    {
        g_tsStatus.expect(tc.query_sts);
    }
    component.Query(component.m_session, &queryParIn, &queryParOut);
    EXPECT_EQ(tmpPar, queryParIn) << "ERROR: Component should not change input structure in Query()!\n";

    //Check that bad parameters were zero-ed or corrected
    CheckParams(queryParOut, tc.set_par);

    //restore parameters, just in case if anything was changed by component
    queryParIn = tmpPar;

    //Query(session, in, in) test:
    if ((MFX_CODEC_AVC == tc.codec) && !(g_tsImpl & MFX_IMPL_VIA_D3D11) && (g_tsOSFamily != MFX_OS_FAMILY_LINUX) && (queryParIn.GetExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY)))
    {
        g_tsStatus.expect(MFX_ERR_UNSUPPORTED);//MBPerSec supported only by D3D11 or VAAPI
    }
    else
    {
        g_tsStatus.expect(tc.query_sts);
    }
    component.Query(component.m_session, &queryParIn, &queryParIn);

    //Check that bad parameters were zero-ed or corrected
    CheckParams(queryParIn, tc.set_par);

    //restore parameters, just in case if anything was changed by component
    queryParIn = tmpPar;

    //Init(session, in) test
    g_tsStatus.expect(tc.init_sts);
    component.Init(component.m_session, &queryParIn);
    EXPECT_EQ(tmpPar, queryParIn) << "ERROR: Component should not change input structure in Init()!\n";

    if( tc.init_sts >= MFX_ERR_NONE )
    {
        g_tsStatus.expect( MFX_ERR_NONE );
        if (queryParOut.GetExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY))//GetVideoParam() unsupport MFX_EXTBUFF_ENCODER_CAPABILITY
            queryParOut.RemoveExtBuffer(MFX_EXTBUFF_ENCODER_CAPABILITY);
        component.GetVideoParam(component.m_session, &queryParOut);

        //Check that bad parameters were zero-ed or corrected by component through GetVideoParam
        CheckParams(queryParOut, tc.set_par);

        component.Close();
    }

    component.MFXClose();

    return 0;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    if(0 == tc.component)
    {
        EXPECT_NE((mfxU32)0, tc.component) << "ERROR: test is not implemented\n";
        throw tsFAIL;
    }

    if(DECODE & tc.component)
        RunTestQueryInitComponent<tsVideoDecoder>(tc);

    if(ENCODE & tc.component)
        RunTestQueryInitComponent<tsVideoEncoder>(tc);

    if(VPP & tc.component)
        RunTestQueryInitComponent<tsVideoVPP>(tc);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(TEST_NAME);
#undef TEST_NAME

}
