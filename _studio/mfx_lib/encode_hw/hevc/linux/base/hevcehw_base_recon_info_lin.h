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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_VA_LINUX)

#include "hevcehw_base.h"
#include "hevcehw_base_data.h"
#include "hevcehw_base_recon_info.h"

namespace HEVCEHW
{
namespace Linux
{
namespace Base
{
    class ReconInfo
        : public HEVCEHW::Base::ReconInfo
    {
    public:
        ReconInfo(mfxU32 FeatureId)
            : HEVCEHW::Base::ReconInfo(FeatureId)
        {
            ModRec[0] =
            { //8b
                {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV444)
                    , [](mfxFrameInfo& rec, eMFXHWType)
                    {
                        rec.FourCC = MFX_FOURCC_AYUV;
                    }
                }
                , {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                    , [](mfxFrameInfo& rec, eMFXHWType)
                    {
                        rec.FourCC = MFX_FOURCC_NV12;
                    }
                }
            };
            ModRec[1] =
            { //10b
                {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV444)
                    , [](mfxFrameInfo& rec, eMFXHWType)
                    {
                        rec.FourCC = MFX_FOURCC_Y410;
                    }
                }
                , {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                    , [](mfxFrameInfo& rec, eMFXHWType)
                    {
                        rec.FourCC = MFX_FOURCC_P010;
                    }
                }
            };
        }
    };

} //Base
} //Linux
} //namespace HEVCEHW

#endif
