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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE) && !defined (MFX_VA_LINUX)

#include "av1ehw_g12.h"

namespace AV1EHW
{
namespace Windows
{
namespace Gen12
{
    class MFXVideoENCODEAV1_HW
        : public AV1EHW::Gen12::MFXVideoENCODEAV1_HW
    {
    public:
        MFXVideoENCODEAV1_HW(
            VideoCORE& core
            , mfxStatus& status
            , eFeatureMode mode = eFeatureMode::INIT);

        virtual mfxStatus Init(mfxVideoParam *par) override;

        static mfxStatus Query(
            VideoCORE& core
            , mfxVideoParam *in
            , mfxVideoParam& out)
        {
            mfxStatus sts = MFX_ERR_NONE;
            MFXVideoENCODEAV1_HW self(core, sts, in ? eFeatureMode::QUERY1 : eFeatureMode::QUERY0);
            MFX_CHECK_STS(sts);
            return self.InternalQuery(core, in, out);
        }

        static mfxStatus QueryIOSurf(
            VideoCORE& core
            , mfxVideoParam& par
            , mfxFrameAllocRequest& request)
        {
            mfxStatus sts = MFX_ERR_NONE;
            MFXVideoENCODEAV1_HW self(core, sts, eFeatureMode::QUERY_IO_SURF);
            MFX_CHECK_STS(sts);
            return self.InternalQueryIOSurf(core, par, request);
        }
    };

} //Gen12
} //Windows
}// namespace AV1EHW

#endif