#include "ts_alloc.h"
#include "ts_decoder.h"
#include "ts_struct.h"


namespace mjpegd_init
{

class TestSuite : tsVideoDecoder
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_JPEG) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 max_num_ctrl     = 3;
    
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 ses_mode;
        mfxU32 par_mode;
        mfxU32 osa_type_mode;
        mfxU32 osa_num_mode;
        mfxU32 num_call;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par;
        } ctrl[max_num_ctrl];
    };

    static const tc_struct test_case[];

    enum
    {
        //modes
        SESSION_VALID,
        SESSION_INVALID,
        PAR_NO_EXT_ALLOCATOR,
        PAR_NO_ZERO,
        PAR_ZERO,
        OSA_TYPE_D3D,
        OSA_TYPE_SYSTEM,
        OSA_NUM_LESS,
        OSA_NUM_BIGGER,
        OSA_NUM_VALID,
    };

    enum CTRL_TYPE
    {
          STAGE = 0xFF000000
        , MFXVPAR
    };

    void apply_par(const tc_struct& p, mfxU32 stage)
    {
        for(mfxU32 i = 0; i < max_num_ctrl; i ++)
        {
            auto c = p.ctrl[i];
            void** base = 0;

            if(stage != (c.type))
                continue;
            switch(c.type)
            {
                case MFXVPAR   : base = (void**)&m_pPar;         break;
                default: break;
            }

            if(base)
            {
                if(c.field)
                    tsStruct::set(*base, *c.field, c.par);
                else
                    *base = (void*)c.par;
            }
        }
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32}, //system
    },
    {/* 1*/ MFX_ERR_INVALID_HANDLE, SESSION_INVALID, PAR_NO_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 1, 
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32}
        }
    },
    {/* 2*/ MFX_ERR_NULL_PTR, SESSION_VALID, PAR_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32},
        }
    },
    {/* 3*/ MFX_ERR_UNDEFINED_BEHAVIOR, SESSION_VALID, PAR_NO_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 2,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32}
        }
    },
    {/* 4*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 16}, //video
        }
    },
    {/* 5*/ MFX_ERR_INVALID_VIDEO_PARAM, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {{MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 16}}
    },
    {/* 6*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_ZERO, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {{MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32}}
    },
    {/* 7*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_SYSTEM, OSA_NUM_VALID, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 64} // opaque
        }
    },
    {/* 8*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_D3D, OSA_NUM_VALID, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 64}
        }
    },
    {/* 9*/ MFX_ERR_INVALID_VIDEO_PARAM, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_SYSTEM, OSA_NUM_LESS, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 64}
        }
    },
    {/* 10*/ MFX_ERR_INVALID_VIDEO_PARAM, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_D3D, OSA_NUM_LESS, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 64}
        }
    },
    {/* 11*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_SYSTEM, OSA_NUM_BIGGER, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 32}
        }
    },
    {/* 12*/ MFX_ERR_NONE, SESSION_VALID, PAR_NO_EXT_ALLOCATOR, OSA_TYPE_D3D, OSA_NUM_BIGGER, 1,
        {
            {MFXVPAR, &tsStruct::mfxVideoParam.IOPattern, 64}
        }
    },
   
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const char* sname = g_tsStreamPool.Get("forBehaviorTest/mjpeg/000");
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 1024);
    m_bs_processor = &reader;
    m_par_set = true;

    const tc_struct& tc = test_case[id];
    mfxStatus expected = tc.sts;
    apply_par(tc, MFXVPAR);
    

    if(tc.ses_mode == SESSION_VALID)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(g_tsImpl == MFX_IMPL_HARDWARE)
        {
            if ((g_tsHWtype < MFX_HW_HSW) && (tc.sts == MFX_ERR_NONE))
            {
                expected = MFX_WRN_PARTIAL_ACCELERATION;
            }


        }
         
        m_pPar->mfx.FrameInfo.Width = 320;
        m_pPar->mfx.FrameInfo.Height = 240;
        m_pPar->mfx.FrameInfo.CropW = 320;
        m_pPar->mfx.FrameInfo.CropH = 240;
        m_pPar->mfx.FrameInfo.PicStruct = 1;
        m_pPar->mfx.FrameInfo.FrameRateExtD = 1;
        m_pPar->mfx.FrameInfo.FrameRateExtN = 30;
        m_pPar->mfx.CodecId = 1195724874;
        m_pPar->mfx.JPEGChromaFormat = 2;

        if(m_default)
        {
            if(!m_session)
            {
                MFXInit();
            }
            if(!m_loaded)
            {
                Load();
            }
            if (
                    (
                        !m_pFrameAllocator 
                        && (
                               (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                               || (m_par.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)              
                               || m_use_memid
                            )
                    )
                    && (tc.par_mode != PAR_NO_EXT_ALLOCATOR)
                )
            {
                if(!GetAllocator())
                {
                    UseDefaultAllocator(
                           (m_par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) 
                        || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                    );
                }
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();
            }
            if(!m_par_set && (m_bs_processor || m_pBitstream && m_pBitstream->DataLength))
            {
                DecodeHeader();
            }
            if(m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
            {
                QueryIOSurf();
                AllocOpaque(m_request, m_par);
            }
        }
 
         if (tc.osa_num_mode == OSA_NUM_LESS)
            ((mfxExtOpaqueSurfaceAlloc*)m_pPar)->Out.NumSurface = (m_request.NumFrameMin - 1);
         if (tc.osa_num_mode == OSA_NUM_BIGGER)
            ((mfxExtOpaqueSurfaceAlloc*)m_pPar)->Out.NumSurface = (m_request.NumFrameSuggested + 1);
         if (tc.osa_type_mode == OSA_TYPE_SYSTEM)
            ((mfxExtOpaqueSurfaceAlloc*)m_pPar)->Out.Type = MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_DECODE|MFX_MEMTYPE_OPAQUE_FRAME;
         if (tc.osa_type_mode == OSA_TYPE_D3D)
            ((mfxExtOpaqueSurfaceAlloc*)m_pPar)->Out.Type = m_request.Type;
    }

    if(tc.par_mode == PAR_ZERO)
    {
       m_pPar = 0;
    }

    
    g_tsStatus.expect(MFX_ERR_NONE);
    for (mfxU32 i = 1; i < tc.num_call; i++)
         Init(m_session, m_pPar);
    g_tsStatus.expect(expected);
    Init(m_session, m_pPar);

    

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mjpegd_init);

}