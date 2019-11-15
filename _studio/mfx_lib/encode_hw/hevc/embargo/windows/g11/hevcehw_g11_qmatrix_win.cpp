// Copyright (c) 2019 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX) && defined (MFX_ENABLE_HEVC_CUSTOM_QMATRIX)

#include "hevcehw_g11_qmatrix_win.h"
#include "hevcehw_g11_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;
using namespace HEVCEHW::Windows;
using namespace HEVCEHW::Windows::Gen11;

const uint8_t scalingList0[16] =
{
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16,
    16, 16, 16, 16
};

const uint8_t scalingList_intra[64] =
{
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,16
};

const uint8_t scalingList_inter[64] =
{
    16,16,16,16,17,18,20,24,
    16,16,16,17,18,20,24,25,
    16,16,17,18,20,24,25,28,
    16,17,18,20,24,25,28,33,
    17,18,20,24,25,28,33,41,
    18,20,24,25,28,33,41,54,
    20,24,25,28,33,41,54,71,
    24,25,28,33,41,54,71,91
};

template<typename F>
void ProcessUpRight(size_t size, F&& f)
{
    int y = 0, x = 0;
    size_t i = 0;
    while (i < size * size)
    {
        while (y >= 0)
        {
            if ((((size_t)x) < size) && (((size_t)y) < size))
            {
                std::forward<F>(f)(x, y, i);
                ++i;
            }
            --y;
            ++x;
        }
        y = x;
        x = 0;
    }
}

/// 1 2 3    1 2 4
/// 4 5 6 -> 3 5 7
/// 7 8 9    6 8 9
/// For scanning from plane to up-right
template<typename T>
void MakeUpRight(T const * in, size_t size, T * out)
{
    ProcessUpRight(size, [=](size_t x, size_t y, size_t i) { out[x * size + y] = in[i]; });
}

/// 1 2 4    1 2 3
/// 3 5 7 -> 4 5 6
/// 6 8 9    7 8 9
/// For scanning from up-right order to plane
template<typename T> void UpRightToPlane(T const * in, size_t size, T * out)
{
    ProcessUpRight(size, [=](size_t x, size_t y, size_t i) { out[i] = in[x * size + y]; });
}

void QMatrix::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_UpdateSPS
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& par  = Glob::VideoParam::Get(global);
        auto& CO3  = (const mfxExtCodingOption3&)ExtBuffer::Get(par);
        auto& core = Glob::VideoCore::Get(global);
        bool bNeedQM =
            core.GetHWType() >= MFX_HW_ICL
            && (CO3.ScenarioInfo == MFX_SCENARIO_GAME_STREAMING
                || (CO3.ScenarioInfo == MFX_SCENARIO_REMOTE_GAMING && IsOn(par.mfx.LowPower)));

        MFX_CHECK(bNeedQM, MFX_ERR_NONE);

        auto& sps = Glob::SPS::Get(global);

        sps.scaling_list_data_present_flag = 1;
        sps.scaling_list_enabled_flag      = 1;

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[0]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists1[0]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists2[0]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists3[0]);
        sps.scl.scalingListDCCoefSizeID3[0] = sps.scl.scalingLists3[0][0];
        sps.scl.scalingListDCCoefSizeID2[0] = sps.scl.scalingLists2[0][0];

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[1]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists1[1]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists2[1]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists3[1]);
        sps.scl.scalingListDCCoefSizeID3[1] = sps.scl.scalingLists3[1][0];
        sps.scl.scalingListDCCoefSizeID2[1] = sps.scl.scalingLists2[1][0];

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[2]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists1[2]);
        UpRightToPlane(scalingList_intra, 8, sps.scl.scalingLists2[2]);
        sps.scl.scalingListDCCoefSizeID2[2] = sps.scl.scalingLists2[2][0];

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[3]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists1[3]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists2[3]);
        sps.scl.scalingListDCCoefSizeID2[3] = sps.scl.scalingLists2[3][0];

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[4]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists1[4]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists2[4]);
        sps.scl.scalingListDCCoefSizeID2[4] = sps.scl.scalingLists2[4][0];

        UpRightToPlane(scalingList0, 4, sps.scl.scalingLists0[5]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists1[5]);
        UpRightToPlane(scalingList_inter, 8, sps.scl.scalingLists2[5]);
        sps.scl.scalingListDCCoefSizeID2[5] = sps.scl.scalingLists2[5][0];

        return MFX_ERR_NONE;
    });
}

void QMatrix::InitAlloc(const FeatureBlocks& /*blocks*/, TPushIA Push)
{
    Push(BLK_UpdateDDISubmit
        , [this](StorageRW& global, StorageRW&) -> mfxStatus
    {
        auto& sps = Glob::SPS::Get(global);

        MFX_CHECK(sps.scaling_list_data_present_flag && sps.scaling_list_enabled_flag, MFX_ERR_NONE);

        for (mfxU8 i = 0; i < 6; ++i)
        {
            MakeUpRight(sps.scl.scalingLists0[i], 4, m_qMatrix.ucScalingLists0[i]);
            MakeUpRight(sps.scl.scalingLists1[i], 8, m_qMatrix.ucScalingLists1[i]);
            MakeUpRight(sps.scl.scalingLists2[i], 8, m_qMatrix.ucScalingLists2[i]);
            m_qMatrix.ucScalingListDCCoefSizeID2[i] = sps.scl.scalingListDCCoefSizeID2[i];
        }

        MakeUpRight(sps.scl.scalingLists3[0], 8, m_qMatrix.ucScalingLists3[0]);
        m_qMatrix.ucScalingListDCCoefSizeID3[0] = sps.scl.scalingListDCCoefSizeID3[0];

        MakeUpRight(sps.scl.scalingLists3[1], 8, m_qMatrix.ucScalingLists3[1]);
        m_qMatrix.ucScalingListDCCoefSizeID3[1] = sps.scl.scalingListDCCoefSizeID3[1];

        auto         vaType = Glob::VideoCore::Get(global).GetVAType();
        auto&        par    = Glob::DDI_SubmitParam::Get(global);
        auto&        pps    = Deref(GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_HEVC>(ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));
        DDIExecParam ddiQM;

        ddiQM.In.pData = &m_qMatrix;
        ddiQM.In.Size  = sizeof(m_qMatrix);

        SetIf(ddiQM.Function, vaType == MFX_HW_D3D11, mfxU32(D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA));
        SetIf(ddiQM.Function, vaType == MFX_HW_D3D9, mfxU32(D3DDDIFMT_INTELENCODE_QUANTDATA));
        par.emplace_back(std::move(ddiQM));

        pps.scaling_list_data_present_flag = (sps.scaling_list_enabled_flag && sps.scaling_list_data_present_flag);

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)