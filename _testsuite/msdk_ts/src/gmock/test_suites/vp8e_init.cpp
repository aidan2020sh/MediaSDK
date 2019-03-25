#include "ts_encoder.h"

namespace
{

typedef struct
{
    mfxSession      session;
    mfxVideoParam*  pPar;
}InitPar;


typedef void (*callback)(tsVideoEncoder&, InitPar&, mfxU32, mfxU32, mfxU32);

void zero_session   (tsVideoEncoder&, InitPar& p, mfxU32, mfxU32, mfxU32) { p.session = 0; }
void zero_param     (tsVideoEncoder&, InitPar& p, mfxU32, mfxU32, mfxU32) { p.pPar = 0; }
void FourCC         (tsVideoEncoder&, InitPar& p, mfxU32 v, mfxU32, mfxU32) { p.pPar->mfx.FrameInfo.FourCC = v; }
void double_init    (tsVideoEncoder& enc, InitPar& , mfxU32, mfxU32, mfxU32) { enc.Init(); }
void IOPattern      (tsVideoEncoder& enc, InitPar& , mfxU32 IOPattern, mfxU32 setAllocator, mfxU32)
{
    enc.m_par.IOPattern = IOPattern;
    if(setAllocator)
    {
        if (enc.m_pVAHandle)
        {
            enc.SetAllocator(enc.m_pVAHandle, true);
        } else
        {
            enc.UseDefaultAllocator(enc.m_impl & MFX_IMPL_SOFTWARE);
            enc.m_pFrameAllocator = enc.GetAllocator();
            enc.SetFrameAllocator();
        }
    }
}
void InitClose_x5(tsVideoEncoder& enc, InitPar&, mfxU32, mfxU32, mfxU32)
{
    for(mfxU32 i = 0; i < 5; i ++)
    {
        enc.Init();
        enc.Close();
    }
}
void set_par(tsVideoEncoder&, InitPar& p, mfxU32 offset, mfxU32 size, mfxU32 value)
{
    mfxU8* p8 = (mfxU8*)p.pPar + offset;
    memcpy(p8, &value, TS_MIN(4, size));
}
void ext_buf(tsVideoEncoder& enc, InitPar&, mfxU32 id, mfxU32 size, mfxU32)
{
    enc.m_par.AddExtBuffer(id, size);
}
void CBR(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    enc.m_par.mfx.InitialDelayInKB  = p0;
    enc.m_par.mfx.TargetKbps        = enc.m_par.mfx.MaxKbps = p1;
}
void VBR(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
    enc.m_par.mfx.InitialDelayInKB  = p0;
    enc.m_par.mfx.TargetKbps        = p1;
    enc.m_par.mfx.MaxKbps           = p2;
}
void CQP(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    enc.m_par.mfx.QPI = p0;
    enc.m_par.mfx.QPP = p1;
    enc.m_par.mfx.QPB = p2;
}
void AVBR(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.RateControlMethod = MFX_RATECONTROL_AVBR;
    enc.m_par.mfx.Accuracy      = p0;
    enc.m_par.mfx.TargetKbps    = p1;
    enc.m_par.mfx.Convergence   = p2;
}
void AR(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    enc.m_par.mfx.FrameInfo.AspectRatioW = p0;
    enc.m_par.mfx.FrameInfo.AspectRatioH = p1;
}
void COVP8(tsVideoEncoder& enc, InitPar&, mfxU32 p0, mfxU32 p1, mfxU32 p2)
{
    mfxExtVP8CodingOption& co = enc.m_par;
    co.EnableMultipleSegments = p2;
}


typedef struct
{
    mfxStatus   sts;
    callback    set_par;
    mfxU32      p0;
    mfxU32      p1;
    mfxU32      p2;
} tc_struct;


#define FRAME_INFO_OFFSET(field) (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, FrameInfo) + offsetof(mfxFrameInfo, field))
#define MFX_OFFSET(field)        (offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, field))
#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

tc_struct test_case[] =
{
    {// 0
        MFX_ERR_INVALID_HANDLE,
        zero_session,
    },
    {// 1
        MFX_ERR_NULL_PTR,
        zero_param,
    },
    {// 2
        MFX_ERR_NONE,
    },
    // supported fourcc: NV12
    {// 3
        MFX_ERR_INVALID_VIDEO_PARAM,
        FourCC, MFX_FOURCC_YV12
    },
    {// 4
        MFX_ERR_INVALID_VIDEO_PARAM,
        FourCC, MFX_FOURCC_YUY2
    },
    {// 5
        MFX_ERR_INVALID_VIDEO_PARAM,
        FourCC, MFX_FOURCC_RGB3
    },
    {// 6
        MFX_ERR_INVALID_VIDEO_PARAM,
        FourCC, MFX_FOURCC_RGB4
    },
    {// 7
        MFX_ERR_NONE,
        IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY, 1
    },
    {// 8
        MFX_ERR_NONE,
        IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY, 1
    },
    {// 9
        MFX_ERR_INVALID_VIDEO_PARAM,
        IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY, 0
    },
    {// 10
        MFX_ERR_UNDEFINED_BEHAVIOR,
        double_init
    },
    {// 11
        MFX_ERR_NONE,
        InitClose_x5,
    },
    {/*12*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(Width),  sizeof(mfxU16), 720 + 8 },
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(Height), sizeof(mfxU16), 480 + 8 },
    // supported chroma format: YUV420
    {/*14*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_MONOCHROME },
    {/*15*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV422 },
    {/*16*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV444 },
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV411 },
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(ChromaFormat), sizeof(mfxU16), MFX_CHROMAFORMAT_YUV422V },
    // supported ext buffers: mfxExtOpaqueSurfaceAlloc, mfxExtVP8CodingOption, mfxExtEncoderROI
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtCodingOption           ), },
    {/*20*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtCodingOptionSPSPPS     ), },
    {/*21*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse            ), },
    {/*22*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVppAuxData             ), },
    {/*23*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise             ), },
    {/*24*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp             ), },
    {/*25*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPDetail              ), },
    {/*26*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo        ), },
    {/*27*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse               ), },
    {/*28*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl         ), },
    {/*29*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion ), },
    {/*30*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtPictureTimingSEI       ), },
    {/*31*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtAvcTemporalLayers      ), },
    {/*32*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtCodingOption2          ), },
    {/*33*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab           ), },
    {/*34*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtEncoderCapability      ), },
    {/*35*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtEncoderResetOption     ), },
    {/*36*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtAVCEncodedFrameInfo    ), },
    {/*37*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPComposite           ), },
    {/*38*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     ), },
    {/*39*/ MFX_ERR_NONE,                ext_buf, EXT_BUF_PAR(mfxExtEncoderROI             ), },
    {/*40*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, EXT_BUF_PAR(mfxExtVPPDeinterlacing       ), },
    {/*41*/ MFX_ERR_INVALID_VIDEO_PARAM, ext_buf, 0, 0,                                       },
    {/*42*/ MFX_ERR_NONE,                     CBR,  0, 5000,    0, },
    {/*43*/ MFX_ERR_NONE,                     VBR,  0, 5000, 6000, },
    {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, VBR,  0, 5000, 4000, },
    // valid values for CQP: 0-127, no B frames - QPB = 0
    {/*45*/ MFX_ERR_NONE,                     CQP, 127, 127,   0, },
    {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CQP,  63,  63,  63, },
    {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CQP, 128,  63,   0, },
    {/*48*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, CQP,  63, 128,   0, },
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(RateControlMethod),  sizeof(mfxU16), MFX_RATECONTROL_LA     },
    {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(RateControlMethod),  sizeof(mfxU16), MFX_RATECONTROL_ICQ    },
    {/*51*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(RateControlMethod),  sizeof(mfxU16), MFX_RATECONTROL_VCM    },
    {/*52*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(RateControlMethod),  sizeof(mfxU16), MFX_RATECONTROL_LA_ICQ },
    {/*53*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(RateControlMethod),  sizeof(mfxU16), 0                      },
    {/*54*/ MFX_ERR_NONE,                     set_par, MFX_OFFSET(CodecProfile),  sizeof(mfxU16), MFX_PROFILE_VP8_0 },
    {/*55*/ MFX_ERR_NONE,                     set_par, MFX_OFFSET(CodecProfile),  sizeof(mfxU16), MFX_PROFILE_VP8_1 },
    {/*56*/ MFX_ERR_NONE,                     set_par, MFX_OFFSET(CodecProfile),  sizeof(mfxU16), MFX_PROFILE_VP8_2 },
    {/*57*/ MFX_ERR_NONE,                     set_par, MFX_OFFSET(CodecProfile),  sizeof(mfxU16), MFX_PROFILE_VP8_3 },
    {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(CodecProfile),  sizeof(mfxU16), MFX_PROFILE_VP8_3 + 1 },
    {/*59*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(CodecLevel),  sizeof(mfxU16), 1 },
    {/*60*/ MFX_ERR_NONE,                     set_par, MFX_OFFSET(GopRefDist),  sizeof(mfxU16), 1 },
    {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(IdrInterval), sizeof(mfxU16), 1 },
    {/*62*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(NumRefFrame), sizeof(mfxU16), 1 },
    {/*63*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, MFX_OFFSET(NumSlice),    sizeof(mfxU16), 1 },
    {/*64*/ MFX_ERR_NONE,                     set_par, FRAME_INFO_OFFSET(PicStruct),     sizeof(mfxU16), MFX_PICSTRUCT_PROGRESSIVE },
    {/*65*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(PicStruct),     sizeof(mfxU16), MFX_PICSTRUCT_FIELD_TFF },
    {/*66*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, set_par, FRAME_INFO_OFFSET(FrameRateExtN), sizeof(mfxU16), 181 },
    {/*67*/ MFX_ERR_NONE,                     AR,  1, 1, },
    {/*68*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, AR,  1, 2, },
    {/*69*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, AR,  2, 1, },
    {/*70*/ MFX_ERR_NONE, set_par, MFX_OFFSET(EncodedOrder), sizeof(mfxU16), 1 }
};

int test(unsigned int id)
{
    TS_START;
    tsVideoEncoder enc(MFX_CODEC_VP8);
    tc_struct& tc =  test_case[id];

    enc.MFXInit();

    if (enc.m_uid) // load plugin
        enc.Load();

    InitPar par = {enc.m_session, &enc.m_par};

    if(tc.set_par)
    {
        (*tc.set_par)(enc, par, tc.p0, tc.p1, tc.p2);
    }

    g_tsStatus.expect(tc.sts);
    enc.Init(par.session, par.pPar);


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE(vp8e_init, test, sizeof(test_case)/sizeof(tc_struct));

};