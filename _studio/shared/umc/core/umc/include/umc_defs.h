// Copyright (c) 2003-2018 Intel Corporation
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

#ifndef __UMC_DEFS_H__
#define __UMC_DEFS_H__

#define ALLOW_SW_VC1_FALLBACK
// This file contains defines which switch on/off support of
// codecs and renderers on application level
/*
// Windows
*/
#if defined(WIN64) || defined (WIN32)

    // video decoders
    #define UMC_ENABLE_H264_VIDEO_DECODER
    #define UMC_ENABLE_H265_VIDEO_DECODER
    #define UMC_ENABLE_MPEG2_VIDEO_DECODER
    #define UMC_ENABLE_MJPEG_VIDEO_DECODER
    #define UMC_ENABLE_VC1_VIDEO_DECODER
    //#define UMC_ENABLE_VP8_VIDEO_DECODER
    #define UMC_ENABLE_VP9_VIDEO_DECODER
    #define UMC_ENABLE_AV1_VIDEO_DECODER

    // video encoders
    #define UMC_ENABLE_H264_VIDEO_ENCODER
    #define UMC_ENABLE_MVC_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG2_VIDEO_ENCODER
    #define UMC_ENABLE_MJPEG_VIDEO_ENCODER

    #define UMC_ENABLE_UMC_SCENE_ANALYZER

// audio decoders
    #define UMC_ENABLE_AAC_AUDIO_DECODER
    #define UMC_ENABLE_MP3_AUDIO_DECODER

    // audio encoders
    #define UMC_ENABLE_AAC_AUDIO_ENCODER
    #define UMC_ENABLE_MP3_AUDIO_ENCODER

#endif // Winx64 on EM64T

/*
// Linux on IA32
*/

#if (defined(LINUX32) || defined(__APPLE__)) && !defined(ANDROID)

    // video decoders
    #define UMC_ENABLE_MJPEG_VIDEO_DECODER
    //#define UMC_ENABLE_VP8_VIDEO_DECODER
    #define UMC_ENABLE_VC1_VIDEO_DECODER
    #define UMC_ENABLE_H264_VIDEO_DECODER
    #define UMC_ENABLE_H265_VIDEO_DECODER
    #define UMC_ENABLE_MPEG2_VIDEO_DECODER
    #define UMC_ENABLE_VP9_VIDEO_DECODER
#if defined(LINUX_TARGET_PLATFORM_ATS)
    #define UMC_ENABLE_AV1_VIDEO_DECODER
#endif


    // video encoders
    #define UMC_ENABLE_H264_VIDEO_ENCODER
    #define UMC_ENABLE_MVC_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG2_VIDEO_ENCODER
    #define UMC_ENABLE_MPEG4_VIDEO_ENCODER
    #define UMC_ENABLE_MJPEG_VIDEO_ENCODER

    #define UMC_ENABLE_UMC_SCENE_ANALYZER

    // audio decoders
    #define UMC_ENABLE_AAC_AUDIO_DECODER
    #define UMC_ENABLE_MP3_AUDIO_DECODER

    // audio encoders
    #define UMC_ENABLE_AAC_AUDIO_ENCODER
    //#define UMC_ENABLE_AC3_AUDIO_ENCODER
    //#define UMC_ENABLE_MP3_AUDIO_ENCODER

#endif // Linux on IA32

/*
// Android
*/

#if defined(ANDROID)

    // video decoders
    #define UMC_ENABLE_MJPEG_VIDEO_DECODER
    #define UMC_ENABLE_VC1_VIDEO_DECODER
    #define UMC_ENABLE_H264_VIDEO_DECODER
    #define UMC_ENABLE_H265_VIDEO_DECODER
    #define UMC_ENABLE_MPEG2_VIDEO_DECODER
    #define UMC_ENABLE_VP9_VIDEO_DECODER

    // video encoders
    #define UMC_ENABLE_H264_VIDEO_ENCODER
    #define UMC_ENABLE_MVC_VIDEO_ENCODER
    #define UMC_ENABLE_MJPEG_VIDEO_ENCODER

    #define UMC_ENABLE_UMC_SCENE_ANALYZER

#endif // Android

#ifdef __cplusplus

namespace UMC
{

};

#endif //__cplusplus

#ifndef OPEN_SOURCE

    // readers/writers
    #define UMC_ENABLE_FILE_READER
    #define UMC_ENABLE_FIO_READER
    #define UMC_ENABLE_FILE_WRITER

    // splitters
    #define UMC_ENABLE_AVI_SPLITTER
    #define UMC_ENABLE_MPEG2_SPLITTER
    #define UMC_ENABLE_MP4_SPLITTER
    #define UMC_ENABLE_VC1_SPLITTER
    #define UMC_ENABLE_H264_SPLITTER

#include "ipps.h"
#endif

#include <stdint.h>

#ifdef __cplusplus
#include <algorithm>
#endif //__cplusplus

#define MFX_INTERNAL_CPY_S(dst, dstsize, src, src_size) memcpy_s((uint8_t *)(dst), (size_t)(dstsize), (const uint8_t *)(src), (size_t)(src_size))
#define MFX_INTERNAL_CPY(dst, src, size) memcpy((uint8_t *)(dst), (const uint8_t *)(src), (int)(size))
#define MFX_INTERNAL_ZERO(dst, size) memset((uint8_t *)(dst), 0, (int)(size))

#ifndef MFX_MAX
#define MFX_MAX( a, b ) ( ((a) > (b)) ? (a) : (b) )
#endif

#ifndef MFX_MIN
#define MFX_MIN( a, b ) ( ((a) < (b)) ? (a) : (b) )
#endif

#ifndef MFX_ABS
#define MFX_ABS( a )    ((a) >= 0 ? (a) : -(a))
#endif

#define MFX_MAX_32S    ( 2147483647 )
#define MFX_MAXABS_64F ( 1.7976931348623158e+308 )
#define MFX_MAX_32U    ( 0xFFFFFFFF )

#if defined( _WIN32 ) || defined ( _WIN64 )
  #define MFX_MAX_64S  ( 9223372036854775807i64 )
#else
  #define MFX_MAX_64S  ( 9223372036854775807LL )
#endif

#ifndef OPEN_SOURCE
typedef IppiSize mfxSize;
#else
typedef struct {
    int width;
    int height;
} mfxSize;
#endif

#if defined( _WIN32 ) || defined ( _WIN64 )
  #define __STDCALL  __stdcall
  #define __CDECL    __cdecl
#else
  #define __STDCALL
  #define __CDECL
#endif

#if defined(_WIN32) || defined(_WIN64)
  #define ALIGN_DECL(X) __declspec(align(X))
#else
  #define ALIGN_DECL(X) __attribute__ ((aligned(X)))
#endif

#if (defined(_MSC_VER) && _MSC_VER >= 1900) || \
    (defined(__clang__) && (__clang_major__ == 3 && __clang_minor__ > 4 || __clang_major__ >= 4))
/*
this is a hot fix for C++11 compiler
By default, the compiler generates implicit noexcept(true) specifiers for user-defined destructors and deallocator functions
https://msdn.microsoft.com/en-us/library/84e2zwhh.aspx
Exception Specifications (throw) is deprecated in C++11.
https://msdn.microsoft.com/en-us/library/wfa0edys.aspx
So must use noexcept(true)
http://en.cppreference.com/w/cpp/language/noexcept_spec
http://en.cppreference.com/w/cpp/language/destructor
Non-throwing function are permitted to call potentially-throwing functions.
Whenever an exception is thrown and the search for a handler encounters the outermost block of a non-throwing function, the function std::terminate is called
So we MUST fix this warning
*/
#define THROWSEXCEPTION noexcept(false)
#else
#define THROWSEXCEPTION
#endif
/******************************************************************************/

#endif // __UMC_DEFS_H__
