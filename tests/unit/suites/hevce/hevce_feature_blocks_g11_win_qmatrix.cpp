// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if defined(MFX_ENABLE_UNIT_TEST_HEVCE_DX11_DDI)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"

#include "g9/hevcehw_g9_legacy.h"
#include "g9/hevcehw_g9_qmatrix_win.h"

#include "mocks/include/dxgi/format.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"
#include "mocks/include/mfx/dx11/encoder.h"

namespace hevce { namespace tests
{
    using namespace HEVCEHW;

    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID       = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                        //Actual value doesn't matter here.

    struct FeatureBlocksQMatrix
        : testing::Test
    {
        std::unique_ptr<mocks::registry>      registry;
        std::unique_ptr<mocks::dx11::device>  device;

        MFXVideoSession                       session;

        FeatureBlocks                         blocks{};

        HEVCEHW::Gen9::Legacy                legacy;
        Windows::Gen9::QMatrix               qmatrix;

        StorageRW                             storage;

        FeatureBlocksQMatrix()
            : legacy(HEVCEHW::Gen9::FEATURE_LEGACY)
            , qmatrix(HEVCEHW::Gen9::FEATURE_QMATRIX)
        {}

        void SetUp() override
        {
            //mock registry that MFX Dispatcher finds right 'libmfxhw'
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            mfxVideoParam vp{};
            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp = mocks::mfx::make_param(
                mocks::fourcc::tag<MFX_FOURCC_NV12>{},
                mocks::mfx::make_param(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            //populate session's dx11 core for encoder
            auto s = static_cast<_mfxSession*>(session);
            storage.Insert(Gen9::Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            qmatrix.Init(HEVCEHW::INIT, blocks);

            Gen9::Glob::VideoParam::GetOrConstruct(storage)
                .NewEB(MFX_EXTBUFF_CODING_OPTION3);
            Gen9::Glob::DDI_SubmitParam::GetOrConstruct(storage);
            Gen9::Glob::SPS::GetOrConstruct(storage);
        }
    };

    struct FeatureBlocksQMatrixSCL
        : FeatureBlocksQMatrix
    {
        void SetUp() override
        {
            FeatureBlocksQMatrix::SetUp();

            mfxVideoParam const& vp  = Gen9::Glob::VideoParam::Get(storage);
            device = mocks::mfx::dx11::make_encoder(nullptr, nullptr,
                std::integral_constant<int, mocks::mfx::HW_SKL>{},
                std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );
        }
    };

    TEST_F(FeatureBlocksQMatrixSCL, UpdateSPSUnsupported)
    {
        auto& queueUpdate = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
        auto block = FeatureBlocks::Get(queueUpdate, { Gen9::FEATURE_QMATRIX, Windows::Gen9::QMatrix::BLK_UpdateSPS });

        auto& vp  = Gen9::Glob::VideoParam::Get(storage);
        vp.NewEB(MFX_EXTBUFF_CODING_OPTION3);
        mfxExtCodingOption3& co3  = ExtBuffer::Get(vp);
        co3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;

        auto& sps = Gen9::Glob::SPS::Get(storage);
        ASSERT_EQ(
            sps.scaling_list_enabled_flag, 0
        );
        ASSERT_EQ(
            sps.scaling_list_data_present_flag, 0
        );

        EXPECT_EQ(
            block->Call(storage, storage),
            MFX_ERR_NONE //block returns OK, just doesn't update SPS
        );

        EXPECT_NE(
            sps.scaling_list_enabled_flag, 1
        );
        EXPECT_NE(
            sps.scaling_list_data_present_flag, 1
        );
    }

    struct FeatureBlocksQMatrixICL
        : FeatureBlocksQMatrix
    {
        void SetUp() override
        {
            FeatureBlocksQMatrix::SetUp();

            mfxVideoParam const& vp  = Gen9::Glob::VideoParam::Get(storage);
            device = mocks::mfx::dx11::make_encoder(nullptr, nullptr,
                std::integral_constant<int, mocks::mfx::HW_ICL>{},
                std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );
        }
    };

    TEST_F(FeatureBlocksQMatrixICL, UpdateSPSScenarioNotApplicable)
    {
        auto& queueUpdate = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
        auto block = FeatureBlocks::Get(queueUpdate, { Gen9::FEATURE_QMATRIX, Windows::Gen9::QMatrix::BLK_UpdateSPS });

        auto& vp  = Gen9::Glob::VideoParam::Get(storage);
        mfxExtCodingOption3& co3  = ExtBuffer::Get(vp);
        ASSERT_NE(
            co3.ScenarioInfo,
            MFX_SCENARIO_GAME_STREAMING
        );

        auto& sps = Gen9::Glob::SPS::Get(storage);
        ASSERT_EQ(
            sps.scaling_list_enabled_flag, 0
        );
        ASSERT_EQ(
            sps.scaling_list_data_present_flag, 0
        );

        EXPECT_EQ(
            block->Call(storage, storage),
            MFX_ERR_NONE
        );

        EXPECT_NE(
            sps.scaling_list_enabled_flag, 1
        );
        EXPECT_NE(
            sps.scaling_list_data_present_flag, 1
        );
    }

    TEST_F(FeatureBlocksQMatrixICL, UpdateSPS)
    {
        auto& queueUpdate = FeatureBlocks::BQ<FeatureBlocks::BQ_InitInternal>::Get(blocks);
        auto block = FeatureBlocks::Get(queueUpdate, { Gen9::FEATURE_QMATRIX, Windows::Gen9::QMatrix::BLK_UpdateSPS });

        auto& vp  = Gen9::Glob::VideoParam::Get(storage);
        vp.NewEB(MFX_EXTBUFF_CODING_OPTION3);
        mfxExtCodingOption3& co3  = ExtBuffer::Get(vp);
        co3.ScenarioInfo = MFX_SCENARIO_GAME_STREAMING;

        auto& sps = Gen9::Glob::SPS::Get(storage);
        ASSERT_EQ(
            sps.scaling_list_enabled_flag, 0
        );
        ASSERT_EQ(
            sps.scaling_list_data_present_flag, 0
        );

        EXPECT_EQ(
            block->Call(storage, storage),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            sps.scaling_list_enabled_flag, 1
        );
        EXPECT_EQ(
            sps.scaling_list_data_present_flag, 1
        );
    }

    TEST_F(FeatureBlocksQMatrixICL, UpdateDDISubmitNotEnabled)
    {
        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen9::FEATURE_QMATRIX, Windows::Gen9::QMatrix::BLK_UpdateDDISubmit });

        auto& sps = Gen9::Glob::SPS::Get(storage);
        ASSERT_EQ(
            sps.scaling_list_enabled_flag, 0
        );
        ASSERT_EQ(
            sps.scaling_list_data_present_flag, 0
        );

        EXPECT_EQ(
            block->Call(storage, storage),
            MFX_ERR_NONE
        );

        auto& sp = Gen9::Glob::DDI_SubmitParam::Get(storage);
        EXPECT_TRUE(
            sp.empty()
        );
    }

    TEST_F(FeatureBlocksQMatrixICL, UpdateDDISubmit)
    {
        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_InitAlloc>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen9::FEATURE_QMATRIX, Windows::Gen9::QMatrix::BLK_UpdateDDISubmit });

        auto& sps = Gen9::Glob::SPS::Get(storage);
        sps.scaling_list_enabled_flag = sps.scaling_list_data_present_flag = 1;

        ENCODE_SET_PICTURE_PARAMETERS_HEVC pps{};
        ENCODE_COMPBUFFERDESC cb{
            &pps, static_cast<D3DFORMAT>(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA),
            0, sizeof(pps)
        };
        ENCODE_EXECUTE_PARAMS eep{ 1, &cb };

        auto& sp = Gen9::Glob::DDI_SubmitParam::Get(storage);
        sp.push_back(
            Gen9::DDIExecParam{ ENCODE_ENC_PAK_ID, { &eep, sizeof(eep) } }
        );

        EXPECT_EQ(
            block->Call(storage, storage),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            sp.size(), 2 //we expect that [UpdateDDISubmit] will add yet another [DDIExecParam]
        );

        auto const& ep = sp.back();
        EXPECT_EQ(
            ep.Function, mfxU32(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA)
        );
        EXPECT_NE(
            ep.In.pData, nullptr
        );
        EXPECT_EQ(
            ep.In.Size, sizeof(DXVA_Qmatrix_HEVC)
        );

        EXPECT_EQ(
            pps.scaling_list_data_present_flag, UINT(1)
        );
    }

} }

#endif //MFX_ENABLE_UNIT_TEST_HEVCE_DX11_DDI