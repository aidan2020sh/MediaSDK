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

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_g11_alloc.h"
#include "mfxstructures.h"
#include <vector>

namespace HEVCEHW
{
namespace Windows
{
namespace Gen11
{
class MfxFrameAllocResponse
    : public HEVCEHW::Gen11::MfxFrameAllocResponse
{
public:
    MfxFrameAllocResponse(VideoCORE& core)
        : HEVCEHW::Gen11::MfxFrameAllocResponse(core)
    {}
    ~MfxFrameAllocResponse()
    {
        Free();
    }

    virtual mfxStatus Alloc(
        mfxFrameAllocRequest & req,
        bool                   isCopyRequired) override;

    void Free() override;
};

class Allocator
    : public HEVCEHW::Gen11::Allocator
{
public:
    Allocator(mfxU32 FeatureId)
        : HEVCEHW::Gen11::Allocator(FeatureId)
    {}

protected:
    virtual void InitAlloc(const FeatureBlocks& blocks, TPushIA Push) override; 
};

} //Gen11
} //namespace Windows
} //namespace HEVCEHW

#endif
