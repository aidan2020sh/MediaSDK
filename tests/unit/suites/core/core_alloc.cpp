// Copyright (c) 2020-2021 Intel Corporation
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

#if defined(MFX_ENABLE_UNIT_TEST_CORE)

#if (defined(_WIN32) || defined(_WIN64))
    #include "core_base_windows.h"
#elif defined(__linux__)
    #include "core_base_linux.h"
#endif

namespace test
{
    struct coreAlloc : public coreMain
    {
        mfxFrameAllocRequest  req{};
        mfxFrameAllocResponse res{};
        void SetUp() override
        {
            coreMain::SetUp();
            req.AllocId = 1;
            req.NumFrameMin = req.NumFrameSuggested = 1;
            req.Info.Width = info.Width;
            req.Info.Height = info.Height;
            req.Info.FourCC = MFX_FOURCC_NV12;
        }
    };

    TEST_F(coreAlloc, AllocFrames)
    {
        EXPECT_EQ(
            core->AllocFrames(&req, &res),
            MFX_ERR_NONE
        );
    }

    TEST_F(coreAlloc, AllocFramesNeedntCopy)
    {
        EXPECT_EQ(
            core->AllocFrames(&req, &res, false),
            MFX_ERR_NONE
        );
    }

    TEST_F(coreAlloc, AllocFramesNullptrRequest)
    {
        EXPECT_EQ(
            core->AllocFrames(nullptr, &res),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreAlloc, AllocFramesNullptrResponse)
    {
        EXPECT_EQ(
            core->AllocFrames(&req, nullptr),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(coreAlloc, Free)
    {
        ASSERT_EQ(
            core->AllocFrames(&req, &res),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            core->FreeFrames(&res),
            MFX_ERR_NONE
        );
    }

    TEST_F(coreAlloc, FreeFramesWithoutAlloc)
    {
        EXPECT_EQ(
            core->FreeFrames(&res),
            MFX_ERR_NULL_PTR
        );
    }

    // AllocBuffer and FreeBuffer will be removed later
    TEST_F(coreAlloc, DISABLED_AllocBufferIsUnsupported)
    {
        mfxMemId memId{};
        EXPECT_EQ(
            core->AllocBuffer(50, 50, &memId),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(coreAlloc, DISABLED_FreeBufferIsUnsupported)
    {
        mfxMemId memId{};
        EXPECT_EQ(
            core->FreeBuffer(&memId),
            MFX_ERR_UNSUPPORTED
        );
    }
}

#endif // MFX_ENABLE_UNIT_TEST_CORE