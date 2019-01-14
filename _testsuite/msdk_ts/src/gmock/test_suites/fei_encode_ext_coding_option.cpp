/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace fei_encode_ext_coding_option
{

class TestSuite : public tsBitstreamProcessor, public tsParserAVC2, public tsVideoEncoder
{
public:
    TestSuite()
        : tsBitstreamProcessor()
        , tsParserAVC2()
        , tsVideoEncoder(MFX_FEI_FUNCTION_ENCODE, MFX_CODEC_AVC, true)
    {
       m_bs_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxExtCodingOption *pExtCodingOpt = NULL;

        for (int i = 0; i < m_par.NumExtParam; ++i)
        {
            if (m_par.ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION)
            {
                pExtCodingOpt = reinterpret_cast<mfxExtCodingOption*>(m_par.ExtParam[i]);
                break;
            }
        }

        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
        {
            nFrames *= 2;
        }

        SetBuffer0(bs);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            bool AUDPresent = false;

            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (au.nalu[i]->nal_unit_type == BS_AVC2::AUD_NUT)
                {
                    AUDPresent = true;
                    break;
                }
            }

            if (pExtCodingOpt->AUDelimiter == MFX_CODINGOPTION_OFF)
            {
                EXPECT_FALSE(AUDPresent);
            }
            else
            {
                EXPECT_TRUE(AUDPresent);
            }
        }

        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }

private:

    struct tc_struct
    {
        mfxStatus sts_init;
        struct f_pair
        {
            mfxU16 aud_delimiter;
        } set_par;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // AUDelimiter cases
    {/*00*/ MFX_ERR_NONE, MFX_CODINGOPTION_ON},
    {/*01*/ MFX_ERR_NONE, MFX_CODINGOPTION_OFF},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxU32 n = 10;
    tsRawReader stream(g_tsStreamPool.Get("YUV/iceage_720x480_491.yuv"), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    m_impl = MFX_IMPL_HARDWARE;
    MFXInit();

    m_pPar->AsyncDepth = 1; //limitation for FEI (from sample_fei)

    mfxU32 numField = 1;
    if ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF) ||
        (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)) {
        numField = 2;
    }

    SetFrameAllocator(m_session, m_pVAHandle);
    m_pFrameAllocator = m_pVAHandle;

    mfxExtCodingOption& extbuffer_CO = m_par;
    extbuffer_CO.AUDelimiter = tc.set_par.aud_delimiter;

    mfxExtBuffer* bufFrCtrl[2];

    mfxExtFeiEncFrameCtrl feiEncCtrl[2];

    mfxU32 fieldId = 0;

    //assign ExtFeiEncFrameCtrl(mfxEncodeCtrl/runtime)
    for (fieldId = 0; fieldId < numField; fieldId++) {

        memset(&feiEncCtrl[fieldId], 0, sizeof(mfxExtFeiEncFrameCtrl));

        feiEncCtrl[fieldId].Header.BufferId = MFX_EXTBUFF_FEI_ENC_CTRL;
        feiEncCtrl[fieldId].Header.BufferSz = sizeof(mfxExtFeiEncFrameCtrl);
        feiEncCtrl[fieldId].SearchPath = 2;
        feiEncCtrl[fieldId].LenSP = 57;
        feiEncCtrl[fieldId].SubMBPartMask = 0;
        feiEncCtrl[fieldId].MultiPredL0 = 0;
        feiEncCtrl[fieldId].MultiPredL1 = 0;
        feiEncCtrl[fieldId].SubPelMode = 3;
        feiEncCtrl[fieldId].InterSAD = 2;
        feiEncCtrl[fieldId].IntraSAD = 2;
        feiEncCtrl[fieldId].DistortionType = 0;
        feiEncCtrl[fieldId].RepartitionCheckEnable = 0;
        feiEncCtrl[fieldId].AdaptiveSearch = 0;
        feiEncCtrl[fieldId].MVPredictor = 0;
        feiEncCtrl[fieldId].NumMVPredictors[0] = 1;
        feiEncCtrl[fieldId].PerMBQp = 0;
        feiEncCtrl[fieldId].PerMBInput = 0;
        feiEncCtrl[fieldId].MBSizeCtrl = 0;
        feiEncCtrl[fieldId].RefHeight = 32;
        feiEncCtrl[fieldId].RefWidth = 32;
        feiEncCtrl[fieldId].SearchWindow = 5;

        bufFrCtrl[fieldId] = (mfxExtBuffer*)&feiEncCtrl[fieldId];
    }

    m_ctrl.NumExtParam = numField;
    m_ctrl.ExtParam = bufFrCtrl;

    /*******************Init() and Encode() test**********************/
    g_tsStatus.expect(tc.sts_init);
    Init(m_session, m_pPar);

    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * 1024 * 1024 * n);
    EncodeFrames(n);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_ext_coding_option);
};