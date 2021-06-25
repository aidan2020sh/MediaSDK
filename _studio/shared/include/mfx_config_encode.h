// Copyright (c) 2021 Intel Corporation
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

#ifndef _MFX_CONFIG_ENCODE_H_
#define _MFX_CONFIG_ENCODE_H_

#define MFX_ENABLE_PARTIAL_BITSTREAM_OUTPUT

// closed source fixed-style defines
#if !defined(OPEN_SOURCE) && !defined(ANDROID)

    #if (MFX_VERSION >= MFX_VERSION_NEXT)
        #define MFX_ENABLE_AV1_VIDEO_ENCODE
    #endif

    #if defined(MFX_VA_LINUX)
        #if (MFX_VERSION < MFX_VERSION_NEXT)
            #define MFX_EXT_DPB_HEVC_DISABLE
        #endif
    #endif

#endif // #ifndef OPEN_SOURCE

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

    //#define MFX_ENABLE_H264_PRIVATE_CTRL
    #define MFX_ENABLE_H264_REPARTITION_CHECK
    #if defined(MFX_VA_WIN)
        #define ENABLE_H264_MBFORCE_INTRA
    #endif

    #define MFX_ENABLE_H264_ROUNDING_OFFSET

    #ifndef OPEN_SOURCE
        #define MFX_ENABLE_AVCE_DIRTY_RECTANGLE
        #define MFX_ENABLE_AVCE_MOVE_RECTANGLE
        #if defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
            #define MFX_ENABLE_AVCE_VDENC_B_FRAMES
        #endif
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
            #if defined(MFX_VA_WIN)
                #define MFX_ENABLE_AVC_CUSTOM_QMATRIX
                #define MFX_ENABLE_GPU_BASED_SYNC
            #endif
        #endif
    #endif
    #if defined(MFX_ENABLE_MCTF) && defined(MFX_ENABLE_KERNELS)
        #define MFX_ENABLE_MCTF_IN_AVC
    #endif
#endif

#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)
    #define MFX_ENABLE_HEVCE_INTERLACE
    #define MFX_ENABLE_HEVCE_ROI
    #if !defined(OPEN_SOURCE)
        #define MFX_ENABLE_HEVCE_DIRTY_RECT
        #define MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION
        #if defined (MFX_ENABLE_HEVCE_WEIGHTED_PREDICTION)
            #define MFX_ENABLE_HEVCE_FADE_DETECTION
        #endif
        #define MFX_ENABLE_HEVCE_HDR_SEI
        #if (MFX_VERSION >= MFX_VERSION_NEXT)
            #define MFX_ENABLE_HEVCE_UNITS_INFO
            #if defined(MFX_VA_WIN)
                #define MFX_ENABLE_HEVC_CUSTOM_QMATRIX
            #endif
        #endif
        #if !defined(SKIP_EMBARGO)
            #define MFX_ENABLE_HEVCE_SCC
        #endif
    #endif
#endif

#define MFX_ENABLE_QVBR

#if defined(MFX_VA_WIN) && !defined(STRIP_EMBARGO)
    #define MFX_ENABLE_VIDEO_HYPER_ENCODE_HW
#endif

#ifdef MFX_ENABLE_USER_ENCTOOLS
    #define MFX_ENABLE_ENCTOOLS
    #if defined(MFX_VA_WIN)
        #define MFX_ENABLE_ENCTOOLS_LPLA
        #define MFX_ENABLE_LP_LOOKAHEAD
    #endif
#endif

#endif // _MFX_CONFIG_ENCODE_H_
