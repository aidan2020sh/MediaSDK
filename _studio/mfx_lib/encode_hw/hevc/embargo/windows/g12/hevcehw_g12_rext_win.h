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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && (MFX_VERSION >= 1031)

#include "hevcehw_base.h"
#include "hevcehw_g12_rext.h"

namespace HEVCEHW
{
namespace Windows
{
namespace Gen12
{
    using namespace HEVCEHW::Gen12;

    class RExt
        : public HEVCEHW::Gen12::RExt
    {
    public:

        RExt(mfxU32 FeatureId)
            : HEVCEHW::Gen12::RExt(FeatureId)
        {
            mUpdateRecInfo =
            {
                {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV444)
                    , [](mfxFrameInfo& rec, mfxU16& type, bool /*bVDEnc*/)
                    {
                        rec.FourCC = MFX_FOURCC_Y416;
                        rec.Width = mfx::align2_value<mfxU16>(rec.Width, 256 / 4);
                        rec.Height = mfx::align2_value<mfxU16>(rec.Height * 3 / 2, 8);

                        type = (
                            MFX_MEMTYPE_FROM_ENCODE
                            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                            | MFX_MEMTYPE_INTERNAL_FRAME);
                    }
                }
                , {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV422)
                    , [](mfxFrameInfo& rec, mfxU16& type, bool /*bVDEnc*/)
                    {
                        rec.FourCC = MFX_FOURCC_Y216;
                        rec.Width /= 2;
                        rec.Height *= 2;

                        type = (
                            MFX_MEMTYPE_FROM_ENCODE
                            | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                            | MFX_MEMTYPE_INTERNAL_FRAME);
                    }
                }
                , {
                    mfxU16(1 + MFX_CHROMAFORMAT_YUV420)
                    , [](mfxFrameInfo& rec, mfxU16& type, bool bVDEnc)
                    {
                        if (bVDEnc)
                        {
                            rec.FourCC = MFX_FOURCC_P016;
                            rec.Width = mfx::align2_value(rec.Width, 32);

                            type = (
                                MFX_MEMTYPE_FROM_ENCODE
                                | MFX_MEMTYPE_DXVA2_DECODER_TARGET
                                | MFX_MEMTYPE_INTERNAL_FRAME);
                        }
                        else
                        {
                            rec.FourCC = MFX_FOURCC_NV12;
                            rec.Width = mfx::align2_value(rec.Width, 32) * 2;
                        }
                    }
                }
            };
        }
    };

} //Gen12
} //Windows
} //namespace HEVCEHW

#endif