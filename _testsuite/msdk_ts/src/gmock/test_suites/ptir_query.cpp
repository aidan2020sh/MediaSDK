#include "ts_vpp.h"
#include "ts_struct.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace ptir_query
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

    enum
    {
        NULL_SESSION = 1,
        NULL_PARAMS,
        TFF_INPUT,
        ALLOC_OPAQUE,
        ALLOC_OPAQUE_LESS,
        ALLOC_OPAQUE_MORE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct 
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_INVALID_HANDLE, NULL_SESSION},
    {/* 1*/ MFX_ERR_NONE, NULL_PARAMS},

    // Should not be loaded with default parameters (PROGRESSIVE input is unsupported)
    {/* 2*/ MFX_ERR_UNSUPPORTED, 0, {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}},

    // No resize
    {/* 3*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.Width, 320}},
    {/* 4*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.Height, 320}},
    // No crop
    {/* 5*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.CropX, 320}},
    {/* 6*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.CropY, 320}},
    {/* 7*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.CropW, 320}},
    {/* 8*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.Out.CropH, 320}},

    // FourCC cases (only NV12 supported)
    {/* 9*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_NV12}},
    {/*10*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YV12}},
    {/*11*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8}},
    {/*12*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_RGB4}},
    {/*13*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_YUY2}},
    {/*14*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P8_TEXTURE}},

    // ChromaFormat cases (only 420 supported)
    {/*15*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV420}},
    {/*16*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME}},
    {/*17*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422}},
    {/*18*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV444}},
    {/*19*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV400}},
    {/*20*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV411}},
    {/*21*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422H}},
    {/*22*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.vpp.In.ChromaFormat, MFX_CHROMAFORMAT_YUV422V}},

    // Only PROGRESSIVE as output
    {/*23*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                                             {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    {/*24*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                                             {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}},
    
    // Autodetection
    {/*25*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 0},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 0},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 0},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 0},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*26*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*27*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    // Reverse Telecine
    {/*28*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*29*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*30*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 2},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 48},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 2},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*31*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*32*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 24},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},

    // DeInterlace
    {/*33*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*34*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*35*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*36*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 60},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*37*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 2},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*38*/ MFX_ERR_NONE, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 120},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 2},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_BFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*39*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_UNKNOWN},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*40*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_PROGRESSIVE},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    {/*41*/ MFX_ERR_UNSUPPORTED, 0, {{&tsStruct::mfxVideoParam.vpp.In.FrameRateExtN, 60},
                              {&tsStruct::mfxVideoParam.vpp.In.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtN, 30},
                              {&tsStruct::mfxVideoParam.vpp.Out.FrameRateExtD, 1},
                              {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF},
                              {&tsStruct::mfxVideoParam.vpp.Out.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
    // IOPattern cases
    {/*42*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*43*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*44*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*45*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*46*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*47*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*48*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*49*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY}},
    {/*50*/ MFX_ERR_NONE, ALLOC_OPAQUE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY}},
    {/*51*/ MFX_ERR_UNSUPPORTED, ALLOC_OPAQUE_LESS, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*52*/ MFX_ERR_UNSUPPORTED, ALLOC_OPAQUE_MORE, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    {/*53*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.IOPattern, MFX_IOPATTERN_IN_OPAQUE_MEMORY|MFX_IOPATTERN_OUT_OPAQUE_MEMORY}},
    
    // ext buffers
    {/*54*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoNotUse            )}},
    {/*55*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVppAuxData             )}},
    {/*56*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDenoise             )}},
    {/*57*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPProcAmp             )}},
    {/*58*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDetail              )}},
    {/*59*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVideoSignalInfo        )}},
    {/*60*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDoUse               )}},
    {/*61*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCRefListCtrl         )}},
    {/*62*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPFrameRateConversion )}},
    {/*63*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPImageStab           )}},
    {/*64*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPComposite           )}},
    {/*65*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPVideoSignalInfo     )}},
    {/*66*/  MFX_ERR_UNSUPPORTED, TFF_INPUT, {}, {ext_buf, EXT_BUF_PAR(mfxExtVPPDeinterlacing       )}},

    // async
    {/*67*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.AsyncDepth, 0}},
    {/*68*/ MFX_ERR_NONE, TFF_INPUT, {&tsStruct::mfxVideoParam.AsyncDepth, 1}},
    {/*69*/ MFX_ERR_UNSUPPORTED, TFF_INPUT, {&tsStruct::mfxVideoParam.AsyncDepth, 2}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    if (tc.mode != NULL_SESSION)
    {
        MFXInit();

        // always load plug-in
        mfxPluginUID* ptir = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_VPP, MFX_MAKEFOURCC('P','T','I','R'));
        //Load(m_session, ptir, 1);
    }

    if (tc.mode == NULL_PARAMS)
        m_pPar = 0;

    if (tc.mode == TFF_INPUT)
        m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    if (tc.mode != NULL_SESSION)
    {
        SetFrameAllocator(m_session, m_spool_in.GetAllocator());
        m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    }

    if (tc.mode == ALLOC_OPAQUE || tc.mode == ALLOC_OPAQUE_LESS || tc.mode == ALLOC_OPAQUE_MORE)
    {
        m_par.vpp.In.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        mfxExtOpaqueSurfaceAlloc& osa = m_par;
        QueryIOSurf();
        if (tc.mode == ALLOC_OPAQUE_LESS)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin - 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin - 1;
        }
        else if (tc.mode == ALLOC_OPAQUE_MORE)
        {
            m_request[0].NumFrameSuggested = m_request[0].NumFrameMin = m_request[0].NumFrameMin + 1;
            m_request[1].NumFrameSuggested = m_request[1].NumFrameMin = m_request[1].NumFrameMin + 1;
        }
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            m_spool_in.AllocOpaque(m_request[0], osa);
        if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            m_spool_out.AllocOpaque(m_request[1], osa);
    }

    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    mfxVideoParam out_par;
    memset(&out_par, 0, sizeof(mfxVideoParam));
    if (m_pPar)
        memcpy(&out_par, m_pPar, sizeof(mfxVideoParam));
    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    Query(m_session, m_pPar, &out_par);

    if (tc.sts == MFX_ERR_UNSUPPORTED)
    {
        for(mfxU32 i = 0; i < n_par; i++)
        {
            if(tc.set_par[i].f)
            {
                tsStruct::check_eq(&out_par, *tc.set_par[i].f, 0);
            }
        }
    }

    if (tc.mode == NULL_PARAMS)
    {
        EXPECT_NE(0, out_par.IOPattern);
        EXPECT_NE(0, out_par.vpp.In.ChromaFormat);
        EXPECT_NE(0, out_par.vpp.Out.ChromaFormat);
        EXPECT_NE(0, out_par.vpp.In.FourCC);
        EXPECT_NE(0, out_par.vpp.Out.FourCC);
        EXPECT_NE(0, out_par.vpp.In.Width);
        EXPECT_NE(0, out_par.vpp.Out.Width);
        EXPECT_NE(0, out_par.vpp.In.Height);
        EXPECT_NE(0, out_par.vpp.Out.Height);
        EXPECT_NE(0, out_par.vpp.In.FrameRateExtN);
        EXPECT_NE(0, out_par.vpp.In.FrameRateExtD);
        EXPECT_NE(0, out_par.vpp.Out.FrameRateExtN);
        EXPECT_NE(0, out_par.vpp.Out.FrameRateExtD);

        // PTIR specific: no PROGRESSIVE as input, only PROGRESSIVE as output
        EXPECT_NE(1, out_par.vpp.In.PicStruct);
        EXPECT_EQ(1, out_par.vpp.Out.PicStruct);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(ptir_query);

}

