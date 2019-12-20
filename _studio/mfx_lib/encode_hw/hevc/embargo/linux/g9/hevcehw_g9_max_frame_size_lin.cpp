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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "hevcehw_g9_max_frame_size_lin.h"
#include "hevcehw_g9_legacy.h"
#include "hevcehw_g9_va_packer_lin.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen9;

void HEVCEHW::Linux::Gen9::MaxFrameSize::InitInternal(const FeatureBlocks& /*blocks*/, TPushII Push)
{
    Push(BLK_Init
        , [this](StorageRW& strg, StorageRW& local) -> mfxStatus
    {
        const mfxExtCodingOption2& CO2 = ExtBuffer::Get(Glob::VideoParam::Get(strg));
        m_bPatchNextDDITask = !!CO2.MaxFrameSize;

        auto vaType = Glob::VideoCore::Get(strg).GetVAType();
        MFX_CHECK(vaType == MFX_HW_VAAPI, MFX_ERR_NONE);

        m_vaPerSeqMiscData.clear();

        auto& maxfs = AddVaMisc<VAEncMiscParameterBufferMaxFrameSize>(
            VAEncMiscParameterTypeMaxFrameSize
            , m_vaPerSeqMiscData);

        maxfs.max_frame_size = CO2.MaxFrameSize * 8;

        auto& vaInitPar = Tmp::DDI_InitParam::GetOrConstruct(local);

        std::for_each(m_vaPerSeqMiscData.begin(), m_vaPerSeqMiscData.end()
            , [&](decltype(*m_vaPerSeqMiscData.begin()) data)
        {
            DDIExecParam par = {};
            par.Function = VAEncMiscParameterBufferType;
            par.In.pData = data.data();
            par.In.Size = (mfxU32)data.size();
            par.In.Num = 1;

            vaInitPar.push_back(par);
        });

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)