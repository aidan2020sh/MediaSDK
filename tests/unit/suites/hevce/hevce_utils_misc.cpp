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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "base/hevcehw_base_data.h"

namespace hevce { namespace tests
{
    TEST(UtilsGUID, GUIDLess)
    {
        const GUID GuidsAsc[] =
        {
              {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}}
            , {0, 0, 0, {0, 0, 0, 0, 0, 0, 0, 1}}
            , {0, 0, 0, {0, 0, 0, 0, 0, 0, 1, 0}}
            , {0, 0, 0, {0, 0, 0, 0, 0, 1, 0, 0}}
            , {0, 0, 0, {0, 0, 0, 0, 1, 0, 0, 0}}
            , {0, 0, 0, {0, 0, 0, 1, 0, 0, 0, 0}}
            , {0, 0, 0, {0, 0, 1, 0, 0, 0, 0, 0}}
            , {0, 0, 0, {0, 1, 0, 0, 0, 0, 0, 0}}
            , {0, 0, 0, {1, 0, 0, 0, 0, 0, 0, 0}}
            , {0, 0, 1, {0, 0, 0, 0, 0, 0, 0, 0}}
            , {0, 1, 0, {0, 0, 0, 0, 0, 0, 0, 0}}
            , {1, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}}
        };

        const size_t nGUIDS = std::extent<decltype(GuidsAsc)>::value;

        for (size_t lIdx = 0; lIdx < nGUIDS; ++lIdx)
        {
            for (size_t rIdx = 0; rIdx < nGUIDS; ++rIdx)
            {
                EXPECT_EQ(HEVCEHW::Base::LessGUID()(GuidsAsc[lIdx], GuidsAsc[rIdx]), lIdx < rIdx);
                EXPECT_EQ(HEVCEHW::Base::LessGUID()(GuidsAsc[rIdx], GuidsAsc[lIdx]), rIdx < lIdx);
            }
        }
    }
} }