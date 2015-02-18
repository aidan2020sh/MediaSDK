#include "ts_vpp.h"
#include "ts_struct.h"
#include "mfxplugin++.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace vpp_frame_async_ex
{

class FakePTIR : public MFXVPPPlugin
{
public:
    bool isCorrectFuncCalled;
    FakePTIR()
    {
        isCorrectFuncCalled = false;
    }
    virtual ~FakePTIR() {}
    mfxStatus Init(mfxVideoParam* ) { return MFX_ERR_NONE; }
    mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
    {
        in->NumFrameMin = in->NumFrameSuggested = out->NumFrameMin = out->NumFrameSuggested = par->AsyncDepth ? par->AsyncDepth : 1;
        in->Info = par->vpp.In;
        out->Info = par->vpp.Out;
        if(par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
            in->Type = MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        else
            in->Type = MFX_MEMTYPE_FROM_VPPIN | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;

        if(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
            out->Type = MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
        else
            out->Type = MFX_MEMTYPE_FROM_VPPOUT | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY;

        return MFX_ERR_NONE;
    }
    mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) { return MFX_ERR_NONE; }
    mfxStatus Reset(mfxVideoParam *par) { return MFX_ERR_NONE; }
    mfxStatus Close() { return MFX_ERR_NONE; }
    mfxStatus GetVideoParam(mfxVideoParam *par) { return MFX_ERR_NONE; }
    mfxStatus VPPFrameSubmit(mfxFrameSurface1 *surface_in, mfxFrameSurface1 *surface_out, mfxExtVppAuxData *aux, mfxThreadTask *task) { return MFX_ERR_NONE; }
    mfxStatus VPPFrameSubmitEx(mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxThreadTask *task)
    {
        isCorrectFuncCalled = true;
        return MFX_ERR_NONE;
    }
    mfxStatus PluginInit(mfxCoreInterface *core) { return MFX_ERR_NONE; }
    mfxStatus PluginClose() { return MFX_ERR_NONE; }
    mfxStatus GetPluginParam(mfxPluginParam *par) { return MFX_ERR_NONE; }
    mfxStatus Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a) { return MFX_ERR_NONE; }
    mfxStatus FreeResources(mfxThreadTask task, mfxStatus sts) { return MFX_ERR_NONE; }
    virtual mfxStatus SetAuxParams(void*, int) { return MFX_ERR_NONE; }
    virtual void Release() { return ; }
};

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
        LOAD_PTIR = 1
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
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_UNDEFINED_BEHAVIOR},
    {/* 1*/ MFX_ERR_NONE, LOAD_PTIR, {&tsStruct::mfxVideoParam.vpp.In.PicStruct, MFX_PICSTRUCT_FIELD_TFF}}
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    mfxStatus sts;
    FakePTIR plg;
    MFXPluginAdapter<MFXVPPPlugin> adapter(&plg);

    if (tc.mode == LOAD_PTIR)
    {
        sts = MFXVideoUSER_Register(m_session, MFX_PLUGINTYPE_VIDEO_VPP, (mfxPlugin*)&adapter);
    }

    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
    }

    m_spool_in.UseDefaultAllocator(!!(m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));
    SetFrameAllocator(m_session, m_spool_in.GetAllocator());
    m_spool_out.SetAllocator(m_spool_in.GetAllocator(), true);
    AllocSurfaces();

    Init(m_session, m_pPar);

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);

    sts = MFX_ERR_NONE;
    while (1)
    {
        sts = RunFrameVPPAsyncEx();
        if (g_tsStatus.get() == MFX_ERR_MORE_DATA ||
            g_tsStatus.get() == MFX_ERR_MORE_SURFACE)
            continue;
        else if (g_tsStatus.get() == MFX_ERR_NONE)
        {
            int syncPoint = 0xDEADBEAF;
            if (0 == m_pSyncPoint)
                EXPECT_EQ(0, syncPoint);
            break;
        }
        else
        {
            g_tsStatus.check();
            break;
        }
    }

    Close();
    if (tc.mode == LOAD_PTIR)
    {
        if (!plg.isCorrectFuncCalled)
        {
            g_tsLog << "ERROR: correct function wan't called\n";
            g_tsStatus.check(MFX_ERR_UNKNOWN);
        }

        sts = MFXVideoUSER_Unregister(m_session, MFX_PLUGINTYPE_VIDEO_VPP);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_frame_async_ex);

}

