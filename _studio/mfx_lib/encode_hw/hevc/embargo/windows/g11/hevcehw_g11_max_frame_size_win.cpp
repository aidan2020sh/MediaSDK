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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "hevcehw_g11_max_frame_size_win.h"
#include "hevcehw_g11_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;

void Windows::Gen11::MaxFrameSize::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& /*s_task*/) -> mfxStatus
    {
        MFX_CHECK(!m_bPatchNextDDITask, MFX_ERR_NONE);
        const mfxExtCodingOption2& CO2 = ExtBuffer::Get(Glob::VideoParam::Get(global));

        m_bPatchNextDDITask = false;

        auto& par = Glob::DDI_SubmitParam::Get(global);
        auto vaType = Glob::VideoCore::Get(global).GetVAType();

        auto& ddiSPS = Deref(GetDDICB<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC>(
            ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));

        ddiSPS.UserMaxIFrameSize = CO2.MaxFrameSize;
        ddiSPS.UserMaxPBFrameSize = CO2.MaxFrameSize;

        return MFX_ERR_NONE;
    });
}


#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)