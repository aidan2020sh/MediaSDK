/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*!

\file avce_mfe_encode_frame_async.cpp
\brief Gmock test avce_mfe_encode_frame_async

Description:

This test checks behavior of EncodeFrameAsync() when MFE is enabled

Algorithm:
- Initialize SDK session sessions (one parent and two child)
- Set video parameters including MFE for each SDK sessions
- Initialize encoders for each SDK sessions
- Allocate surfaces and bitstreams for each encoder
- Call EncodeFrameAsync() for each encoder
- Sync each encode operations
- Check encode status
- Repeat encoding process for frames_to_encode times
*/

#include "avce_mfe_async_encode.h"

namespace avce_mfe_async_encode {

const tc_struct AsyncEncodeTest::test_case[] =
{
    {/*00*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*01*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*02*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*03*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*04*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*05*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*06*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*07*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/3,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*08*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*09*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*10*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*11*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*12*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*13*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*14*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},
    {/*15*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/4,
            /*bRefType*/0, FlushModeNone, /*frameToFlush*/0},

    {/*16*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*17*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*18*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*19*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*20*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*21*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*22*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*23*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/3,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*24*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*25*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*26*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*27*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*28*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*29*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*30*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},
    {/*31*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_OFF, FlushModeNone, /*frameToFlush*/0},

    {/*32*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*33*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*34*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*35*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*36*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*37*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*38*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},
    {/*39*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/4,
            MFX_B_REF_PYRAMID, FlushModeNone, /*frameToFlush*/0},

    {/*40*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/0},
    {/*41*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/1},
    {/*42*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/2},
    {/*43*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/3},
    {/*44*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/4},
    {/*45*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/5},
    {/*46*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/6},
    {/*47*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/7},
    {/*48*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
          /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/8},
    {/*49*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/0},
    {/*50*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/1},
    {/*51*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc3, /*frameToFlush*/2},
    {/*52*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/3},
    {/*53*/ MFX_MF_AUTO, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/4},
    {/*54*/ MFX_MF_AUTO, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc3, /*frameToFlush*/5},
    {/*55*/ MFX_MF_AUTO, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/6},
    {/*56*/ MFX_MF_AUTO, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/4,
         MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/7},


    {/*57*/ MFX_MF_MANUAL, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/0},
    {/*58*/ MFX_MF_MANUAL, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/1},
    {/*59*/ MFX_MF_MANUAL, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/2},
    {/*60*/ MFX_MF_MANUAL, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/3},
    {/*61*/ MFX_MF_MANUAL, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/4},
    {/*62*/ MFX_MF_MANUAL, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/5},
    {/*63*/ MFX_MF_MANUAL, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc1, /*frameToFlush*/6},
    {/*64*/ MFX_MF_MANUAL, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc2, /*frameToFlush*/7},
    {/*65*/ MFX_MF_MANUAL, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/3,
       /*bRefType*/0, FlushModeEnc3, /*frameToFlush*/8},
    {/*66*/ MFX_MF_MANUAL, /*maxNumFrames*/0, /*asyncDepth*/1, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/0},
    {/*67*/ MFX_MF_MANUAL, /*maxNumFrames*/1, /*asyncDepth*/1, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/1},
    {/*68*/ MFX_MF_MANUAL, /*maxNumFrames*/2, /*asyncDepth*/1, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc3, /*frameToFlush*/2},
    {/*69*/ MFX_MF_MANUAL, /*maxNumFrames*/3, /*asyncDepth*/1, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/3},
    {/*70*/ MFX_MF_MANUAL, /*maxNumFrames*/0, /*asyncDepth*/4, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/4},
    {/*71*/ MFX_MF_MANUAL, /*maxNumFrames*/1, /*asyncDepth*/4, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc3, /*frameToFlush*/5},
    {/*72*/ MFX_MF_MANUAL, /*maxNumFrames*/2, /*asyncDepth*/4, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc1, /*frameToFlush*/6},
    {/*73*/ MFX_MF_MANUAL, /*maxNumFrames*/3, /*asyncDepth*/4, /*gopRefDist*/4,
      MFX_B_REF_PYRAMID, FlushModeEnc2, /*frameToFlush*/7},
};

const unsigned int AsyncEncodeTest::n_cases = sizeof(AsyncEncodeTest::test_case) / sizeof(AsyncEncodeTest::test_case[0]);
const int AsyncEncodeTest::frames_to_encode = 10;

AsyncEncodeTest::AsyncEncodeTest(bool fei_enabled)
: m_enc1(MFX_CODEC_AVC),
  m_enc2(MFX_CODEC_AVC),
  m_enc3(MFX_CODEC_AVC),
  m_fei_enabled(fei_enabled)
{
}

void AsyncEncodeTest::SetParams(tsVideoEncoder& enc, const tc_struct& tc)
{
    enc.m_par.mfx.FrameInfo.Width = enc.m_par.mfx.FrameInfo.CropW = 720;
    enc.m_par.mfx.FrameInfo.Height = enc.m_par.mfx.FrameInfo.CropH = 480;

    enc.m_par.AsyncDepth  = tc.asyncDepth;
    enc.m_par.mfx.GopRefDist = tc.gopRefDist;

    mfxExtMultiFrameParam& mfp = enc.m_par;
    mfp.MFMode = tc.mfMode;

    if (g_tsHWtype != MFX_HW_SKL && tc.maxNumFrames > 1) {
        mfp.MaxNumFrames = 1;
    }
    else {
        mfp.MaxNumFrames = tc.maxNumFrames;
    }

    if (tc.bRefType) {
        mfxExtCodingOption2& copt2 = enc.m_par;
        copt2.BRefType = tc.bRefType;
    }

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
    if (m_fei_enabled) {
        mfxExtFeiParam& fei = enc.m_par;
        fei.Func = MFX_FEI_FUNCTION_ENCODE;
        if (mfp.MaxNumFrames > 2) {
            mfp.MaxNumFrames = 2;
        }
    }
#endif //MFX_ENABLE_H264_VIDEO_FEI_ENCODE
}

void AsyncEncodeTest::InitSession(tsVideoEncoder& enc)
{
    TRACE_FUNC3(MFXInit, enc.m_impl, enc.m_pVersion, enc.m_pSession);
    g_tsStatus.check(::MFXInit(enc.m_impl, enc.m_pVersion, enc.m_pSession));
    TS_TRACE(enc.m_pVersion);
    TS_TRACE(enc.m_pSession);
    enc.tsSession::m_initialized = (g_tsStatus.get() >= 0);
}

void AsyncEncodeTest::SetHandle(tsVideoEncoder& enc, mfxHDL &hdl,
        mfxHandleType &hdl_type)
{
    enc.SetHandle(enc.m_session, hdl_type, hdl);
    enc.m_is_handle_set = (g_tsStatus.get() >= 0);
}

void AsyncEncodeTest::Init(const tc_struct& tc)
{
    // Init parent SDK Session
    m_enc1.MFXInit();

    mfxHDL hdl;
    mfxHandleType hdl_type;
#if (defined(LINUX32) || defined(LINUX64))
    m_enc1.m_pVAHandle->get_hdl(hdl_type, hdl);
#else
    m_enc1.m_pFrameAllocator->get_hdl(hdl_type, hdl);
#endif

    // Init two child SDK sessions
    InitSession(m_enc2);
    SetHandle(m_enc2, hdl, hdl_type);
    m_enc1.MFXJoinSession(m_enc2.m_session);

    InitSession(m_enc3);
    SetHandle(m_enc3, hdl, hdl_type);
    m_enc1.MFXJoinSession(m_enc3.m_session);

    // Initialize encoders
    SetParams(m_enc1, tc);
    m_enc1.Init(m_enc1.m_session, &m_enc1.m_par);

    SetParams(m_enc2, tc);
    m_enc2.Init(m_enc2.m_session, &m_enc2.m_par);

    SetParams(m_enc3, tc);
    m_enc3.Init(m_enc3.m_session, &m_enc3.m_par);

    // Allocate surfaces and bitstreams
    m_enc1.AllocSurfaces();
    m_enc1.AllocBitstream();

    m_enc2.AllocSurfaces();
    m_enc2.AllocBitstream();

    m_enc3.AllocSurfaces();
    m_enc3.AllocBitstream();

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
    if (m_fei_enabled) {
        // Initialize encode control parameters needed for FEI ENCODE
        mfxExtFeiEncFrameCtrl& fei_ctrl1 = m_enc1.m_ctrl;
        fei_ctrl1.SearchWindow = 3;
        mfxExtFeiEncFrameCtrl& fei_ctrl2 = m_enc2.m_ctrl;
        fei_ctrl2.SearchWindow = 3;
        mfxExtFeiEncFrameCtrl& fei_ctrl3 = m_enc3.m_ctrl;
        fei_ctrl3.SearchWindow = 3;
    }
#endif //MFX_ENABLE_H264_VIDEO_FEI_ENCODE
}

void AsyncEncodeTest::WipeBitsreams()
{
    m_enc1.m_bitstream.DataLength = 0;
    m_enc1.m_bitstream.DataOffset = 0;
    m_enc2.m_bitstream.DataLength = 0;
    m_enc2.m_bitstream.DataOffset = 0;
    m_enc3.m_bitstream.DataLength = 0;
    m_enc3.m_bitstream.DataOffset = 0;
}

void AsyncEncodeTest::Flush(tsVideoEncoder& enc)
{
    mfxExtMultiFrameControl& mfc = enc.m_par;
    mfc.Flush = 1;
    mfc.Timeout = 0;
}

int AsyncEncodeTest::RunTest(unsigned int id)
{
    TS_START;

    g_tsStatus.expect(MFX_ERR_NONE);
    const tc_struct& tc = test_case[id];

    Init(tc);

    for (int i = 0; i < frames_to_encode; ++i) {
        mfxStatus sts1 = MFX_ERR_MORE_DATA;
        mfxStatus sts2 = MFX_ERR_MORE_DATA;
        mfxStatus sts3 = MFX_ERR_MORE_DATA;

        for (;;) {
            sts1 = m_enc1.EncodeFrameAsync(m_enc1.m_session,
                    m_enc1.m_pCtrl, m_enc1.GetSurface(),
                    m_enc1.m_pBitstream, m_enc1.m_pSyncPoint);

            sts2 = m_enc2.EncodeFrameAsync(m_enc2.m_session,
                    m_enc2.m_pCtrl, m_enc2.GetSurface(),
                    m_enc2.m_pBitstream, m_enc2.m_pSyncPoint);

            sts3 = m_enc3.EncodeFrameAsync(m_enc3.m_session,
                    m_enc3.m_pCtrl, m_enc3.GetSurface(),
                    m_enc3.m_pBitstream, m_enc3.m_pSyncPoint);

            if (sts1 == MFX_ERR_NONE && sts2 == MFX_ERR_NONE &&
                    sts3 == MFX_ERR_NONE)
            {
                break;
            }
            else if (sts1 == MFX_ERR_MORE_DATA &&
                    sts2 == MFX_ERR_MORE_DATA &&
                    sts3 ==  MFX_ERR_MORE_DATA)
            {
                // Drain remaining data
            }
            else {
                // Only MFX_ERR_MORE_DATA or MFX_ERR_NONE status
                // is expected for all encoders
                std::cout << "ERROR: AsyncEncodeTest() unexpected status\n";
                TS_TRACE(sts1);
                TS_TRACE(sts2);
                TS_TRACE(sts3);
                throw tsFAIL;
            }
        }

        if (tc.frameToFlush == i) {

            if (tc.flushMode == FlushModeEnc1) {
                Flush(m_enc1);
            }
            else if (tc.flushMode == FlushModeEnc2) {
                Flush(m_enc2);
            }
            else if (tc.flushMode == FlushModeEnc3) {
                Flush(m_enc3);
            }
        }

        m_enc1.SyncOperation();
        m_enc2.SyncOperation();
        m_enc3.SyncOperation();

        WipeBitsreams();
    }

    TS_END;
    return 0;
}

class TestSuite : public AsyncEncodeTest {
public:
    //! \brief A constructor
    TestSuite() : AsyncEncodeTest(/*fei_enabled*/false) {};
    //! \brief A destructor
    ~TestSuite() {}
};

TS_REG_TEST_SUITE_CLASS(avce_mfe_encode_frame_async);

};
