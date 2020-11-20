// Copyright (c) 2020 Intel Corporation
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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX) && defined(MFX_ENABLE_HEVCE_SCC)

#include "hevcehw_g12_scc.h"
#include "hevcehw_base_ddi_packer_win.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12
{
    using namespace HEVCEHW::Gen12;

    class SCC
        : public HEVCEHW::Gen12::SCC
    {
    public:
        SCC(mfxU32 FeatureId)
            : HEVCEHW::Gen12::SCC(FeatureId)
        {}
    protected:

        void SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push) override
        {
            Push(BLK_PatchDDITask
                , [this](StorageW& global, StorageW& /*s_task*/) -> mfxStatus
            {
                MFX_CHECK(m_bPatchNextDDITask || m_bPatchDDISlices, MFX_ERR_NONE);
                auto& par = Glob::DDI_SubmitParam::Get(global);
                auto  vaType = Glob::VideoCore::Get(global).GetVAType();
                auto& ddiPPS = Deref(Base::GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_HEVC>(
                    ENCODE_ENC_PAK_ID, Base::DDIPar_In, vaType, par));
                auto  pDdiSlice = Base::GetDDICB<ENCODE_SET_SLICE_HEADER_HEVC>(
                    ENCODE_ENC_PAK_ID, Base::DDIPar_In, vaType, par);

                auto UpdateSlice = [&](ENCODE_SET_SLICE_HEADER_HEVC& s)
                {
                    s.num_ref_idx_l0_active_minus1 -= (ddiPPS.pps_curr_pic_ref_enabled_flag && s.num_ref_idx_l0_active_minus1);
                };

                std::for_each(pDdiSlice, pDdiSlice + ddiPPS.NumSlices, UpdateSlice);

                if (m_bPatchNextDDITask)
                {
                    m_bPatchNextDDITask = false;

                    auto& ddiSPS = Deref(Base::GetDDICB<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC>(
                        ENCODE_ENC_PAK_ID, Base::DDIPar_In, vaType, par));
                    auto& sccflags = Glob::SCCFlags::Get(global);

                    if (sccflags.IBCEnable)
                    {
                        ddiPPS.pps_curr_pic_ref_enabled_flag = PpsExt::Get(global).curr_pic_ref_enabled_flag;
                    }
                    if (sccflags.PaletteEnable)
                    {
                        ddiSPS.palette_mode_enabled_flag = SpsExt::Get(global).palette_mode_enabled_flag;
                    }
                }

                return MFX_ERR_NONE;
            });
        }
    };

} //Gen12
} //Windows
} //namespace HEVCEHW

#endif