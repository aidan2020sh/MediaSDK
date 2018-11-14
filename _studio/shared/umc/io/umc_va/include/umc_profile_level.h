// Copyright (c) 2011-2018 Intel Corporation
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

#ifndef __UMC_PROFILE_LEVEL_H__
#define __UMC_PROFILE_LEVEL_H__

#include <umc_video_decoder.h>

namespace UMC
{
    enum
    {
        MPEG2_PROFILE_SIMPLE  = 5,
        MPEG2_PROFILE_MAIN    = 4,
        MPEG2_PROFILE_SNR     = 3,
        MPEG2_PROFILE_SPATIAL = 2,
        MPEG2_PROFILE_HIGH    = 1
    };

    enum
    {
        MPEG2_LEVEL_LOW  = 10,
        MPEG2_LEVEL_MAIN = 8,
        MPEG2_LEVEL_H14  = 6,
        MPEG2_LEVEL_HIGH = 4
    };

    enum
    {
        MPEG4_PROFILE_SIMPLE                    =  0,
        MPEG4_PROFILE_SIMPLE_SCALABLE           =  1,
        MPEG4_PROFILE_CORE                      =  2,
        MPEG4_PROFILE_MAIN                      =  3,
        MPEG4_PROFILE_NBIT                      =  4,
        MPEG4_PROFILE_SCALABLE_TEXTURE          =  5,
        MPEG4_PROFILE_SIMPLE_FACE               =  6,
        MPEG4_PROFILE_BASIC_ANIMATED_TEXTURE    =  7,
        MPEG4_PROFILE_HYBRID                    =  8,
        MPEG4_PROFILE_ADVANCED_REAL_TIME_SIMPLE =  9,
        MPEG4_PROFILE_CORE_SCALABLE             = 10,
        MPEG4_PROFILE_ADVANCED_CODE_EFFICIENCY  = 11,
        MPEG4_PROFILE_ADVANCED_CORE             = 12,
        MPEG4_PROFILE_ADVANCED_SCALABLE_TEXTURE = 13,
        MPEG4_PROFILE_STUDIO                    = 14,
        MPEG4_PROFILE_ADVANCED_SIMPLE           = 15,
        MPEG4_PROFILE_FGS                       = 16
    };

    enum
    {
        MPEG4_LEVEL_0  = 0,
        MPEG4_LEVEL_1  = 1,
        MPEG4_LEVEL_2  = 2,
        MPEG4_LEVEL_3  = 3,
        MPEG4_LEVEL_4  = 4,
        MPEG4_LEVEL_5  = 5,
        MPEG4_LEVEL_3B = 13
    };

    enum
    {
        H264_PROFILE_BASELINE           = 66,
        H264_PROFILE_MAIN               = 77,
        H264_PROFILE_EXTENDED           = 88,
        H264_PROFILE_HIGH               = 100,
        H264_PROFILE_HIGH10             = 110,
        H264_PROFILE_HIGH422            = 122,
        H264_PROFILE_HIGH444            = 144,
        H264_PROFILE_ADVANCED444_INTRA  = 166,
        H264_PROFILE_ADVANCED444        = 188
    };

    enum
    {
        H264_LEVEL_1    = 10,
        H264_LEVEL_11   = 11,
        H264_LEVEL_1b   = 11,
        H264_LEVEL_12   = 12,
        H264_LEVEL_13   = 13,

        H264_LEVEL_2    = 20,
        H264_LEVEL_21   = 21,
        H264_LEVEL_22   = 22,

        H264_LEVEL_3    = 30,
        H264_LEVEL_31   = 31,
        H264_LEVEL_32   = 32,

        H264_LEVEL_4    = 40,
        H264_LEVEL_41   = 41,
        H264_LEVEL_42   = 42,

        H264_LEVEL_5    = 50,
        H264_LEVEL_51   = 51
    };

    enum
    {
        VC1_PROFILE_SIMPLE   = 0,
        VC1_PROFILE_MAIN     = 1,
        VC1_PROFILE_RESERVED = 2,
        VC1_PROFILE_ADVANCED = 3
    };

    enum
    {
        VC1_LEVEL_0 = 0,
        VC1_LEVEL_1 = 1,
        VC1_LEVEL_2 = 2,
        VC1_LEVEL_3 = 3,
        VC1_LEVEL_4 = 4,
        VC1_LEVEL_LOW    = 0,
        VC1_LEVEL_MEDIAN = 2,
        VC1_LEVEL_HIGH   = 4
    };

    bool CheckProfileLevelMenlow(UMC::VideoDecoderParams *pInfo);

}; // namespace UMC

#endif // __UMC_PROFILE_LEVEL_H__
