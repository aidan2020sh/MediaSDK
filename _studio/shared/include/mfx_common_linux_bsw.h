// Copyright (c) 2008-2018 Intel Corporation
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

#ifndef _MFX_COMMON_LINUX_BSW_H_
#define _MFX_COMMON_LINUX_BSW_H_

// h264
#define MFX_ENABLE_H264_VIDEO_DECODE

#if defined (MFX_VA)
    #define MFX_ENABLE_H265_VIDEO_DECODE
    #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
    //#define MFX_ENABLE_VP9_VIDEO_DECODE_HW
    #define MFX_ENABLE_H265_VIDEO_ENCODE
    #define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
#endif

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_DECODE
#endif

#define MFX_ENABLE_H264_VIDEO_ENCODE
#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_ENCODE
#endif
#define MFX_ENABLE_MVC_VIDEO_ENCODE

#if defined(LINUX64)
#define MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
#define MFX_ENABLE_H264_VIDEO_FEI_PREENC
#define MFX_ENABLE_H264_VIDEO_FEI_ENC
#define MFX_ENABLE_H264_VIDEO_FEI_PAK
#endif

// mpeg2
#if 1 // ENABLED
#define MFX_ENABLE_MPEG2_VIDEO_DECODE
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE
#define MFX_ENABLE_MPEG2_VIDEO_PAK
#define MFX_ENABLE_MPEG2_VIDEO_ENC
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
#endif  

//// vc1
#if 1 // ENABLED
#define MFX_ENABLE_VC1_VIDEO_DECODE
#endif

// mjpeg
#if 1 // ENABLED
#define MFX_ENABLE_MJPEG_VIDEO_DECODE
#define MFX_ENABLE_MJPEG_VIDEO_ENCODE
#endif

// vpp
#define MFX_ENABLE_DENOISE_VIDEO_VPP
#define MFX_ENABLE_IMAGE_STABILIZATION_VPP
#define MFX_ENABLE_VPP
#define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
#define MFX_ENABLE_MJPEG_ROTATE_VPP

#define MFX_ENABLE_H264_VIDEO_ENCODE_HW
//#define MFX_ENABLE_H264_VIDEO_ENC_HW

#define MFX_ENABLE_MVC_VIDEO_ENCODE_HW
#if defined(AS_H264LA_PLUGIN)
#define MFX_ENABLE_LA_H264_VIDEO_HW
#endif

// H265 FEI plugin
#if defined(AS_H265FEI_PLUGIN)
#define MFX_ENABLE_H265FEI_HW
#endif

// user plugin for decoder, encoder, and vpp
#define MFX_ENABLE_USER_DECODE
#define MFX_ENABLE_USER_ENCODE
#define MFX_ENABLE_USER_ENC
#define MFX_ENABLE_USER_VPP

// aac
#define MFX_ENABLE_AAC_AUDIO_DECODE
#define MFX_ENABLE_AAC_AUDIO_ENCODE

//mp3
#define MFX_ENABLE_MP3_AUDIO_DECODE
// linux support

//#undef MFX_ENABLE_MP3_AUDIO_DECODE
// HW limitation
#if defined (MFX_VA)
    // h265
    //#undef MFX_ENABLE_H265_VIDEO_DECODE
    //#undef MFX_ENABLE_H265_VIDEO_ENCODE
    // h264
    //#undef MFX_ENABLE_H264_VIDEO_ENCODE
    #undef MFX_ENABLE_MVC_VIDEO_ENCODE
    //#undef MFX_ENABLE_H264_VIDEO_PAK

    //#undef MFX_ENABLE_H264_VIDEO_ENCODE_HW
    //#undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW

    // mpeg2
    //#undef MFX_ENABLE_MPEG2_VIDEO_DECODE
    //#undef MFX_ENABLE_MPEG2_VIDEO_ENCODE
    //#undef MFX_ENABLE_MPEG2_VIDEO_PAK
    //#undef MFX_ENABLE_MPEG2_VIDEO_ENC

    // mjpeg
    //#undef MFX_ENABLE_MJPEG_VIDEO_DECODE
    //#undef MFX_ENABLE_MJPEG_VIDEO_ENCODE

    // vpp
    //#undef MFX_ENABLE_DENOISE_VIDEO_VPP
    //#undef MFX_ENABLE_VPP

    // vc1
    //#undef MFX_ENABLE_VC1_VIDEO_DECODE

    // vp8
    //#undef MFX_ENABLE_VP8_VIDEO_DECODE
    //#undef MFX_ENABLE_VP8_VIDEO_DECODE_HW

    //#undef MFX_ENABLE_H265_VIDEO_ENCODE
// SW limitation
#else // #if defined (MFX_VA)
    //#undef MFX_ENABLE_VP8_VIDEO_DECODE
    //#undef MFX_ENABLE_VP8_VIDEO_DECODE_HW
#endif // #if defined (MFX_VA)

#define SYNCHRONIZATION_BY_VA_SYNC_SURFACE

#if defined(AS_H264LA_PLUGIN)
    #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
    #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
    #undef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
    #undef MFX_ENABLE_H264_VIDEO_FEI_PREENC
    #undef MFX_ENABLE_H264_VIDEO_FEI_ENC
    #undef MFX_ENABLE_H264_VIDEO_FEI_PAK
    #undef MFX_ENABLE_VPP
#endif

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined(AS_VP9D_PLUGIN) || defined(AS_VP9E_PLUGIN) || defined(AS_CAMERA_PLUGIN) || defined (MFX_RT)
    #undef MFX_ENABLE_H265_VIDEO_DECODE
    #undef MFX_ENABLE_H265_VIDEO_ENCODE
    #undef MFX_ENABLE_H264_VIDEO_DECODE
    #undef MFX_ENABLE_H264_VIDEO_ENCODE
    #undef MFX_ENABLE_MVC_VIDEO_ENCODE
    #undef MFX_ENABLE_MPEG2_VIDEO_DECODE
    #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE
    #undef MFX_ENABLE_MPEG2_VIDEO_PAK
    #undef MFX_ENABLE_MPEG2_VIDEO_ENC
    #undef MFX_ENABLE_VC1_VIDEO_DECODE
    #undef MFX_ENABLE_MJPEG_VIDEO_DECODE
    #undef MFX_ENABLE_MJPEG_VIDEO_ENCODE
    #undef MFX_ENABLE_DENOISE_VIDEO_VPP
    #undef MFX_ENABLE_IMAGE_STABILIZATION_VPP
    #undef MFX_ENABLE_VPP
    #undef MFX_ENABLE_H264_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_MVC_VIDEO_ENCODE_HW
    #undef MFX_ENABLE_USER_DECODE
    #undef MFX_ENABLE_USER_ENCODE
    #undef MFX_ENABLE_AAC_AUDIO_DECODE
    #undef MFX_ENABLE_AAC_AUDIO_ENCODE
    #undef MFX_ENABLE_MP3_AUDIO_DECODE
    #undef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
    #undef MFX_ENABLE_H264_VIDEO_FEI_PREENC
    #undef MFX_ENABLE_VP8_VIDEO_DECODE
    #undef MFX_ENABLE_VP8_VIDEO_DECODE_HW
    #undef MFX_ENABLE_VP9_VIDEO_DECODE
    #undef MFX_ENABLE_VP9_VIDEO_DECODE_HW
    #undef MFX_ENABLE_VP9_VIDEO_DECODE_HW
#endif // #if defined(AS_HEVCD_PLUGIN)
#if defined(AS_CAMERA_PLUGIN)
    #define MFX_ENABLE_VPP
#endif
#if defined(AS_HEVCD_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_DECODE
#endif
#if defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_ENCODE
    #if !defined(__APPLE__)
        #define MFX_ENABLE_CM
    #endif
#endif

#if defined(AS_VP8DHW_PLUGIN)
    #define MFX_ENABLE_VP8_VIDEO_DECODE
    #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
#endif

#if defined(AS_VP8D_PLUGIN)
#define MFX_ENABLE_VP8_VIDEO_DECODE
#ifdef MFX_VA
#define MFX_ENABLE_VP8_VIDEO_DECODE_HW
#endif
#endif

#if defined(AS_VP9D_PLUGIN)
//#define MFX_ENABLE_VP9_VIDEO_DECODE
#ifdef MFX_VA
#define MFX_ENABLE_VP9_VIDEO_DECODE_HW
#endif
#endif

#if defined(AS_VP9E_PLUGIN)
#ifdef MFX_VA
#define MFX_ENABLE_VP9_VIDEO_ENCODE_HW
#endif
#endif

#endif //_MFX_COMMON_LINUX_BSW_H_
