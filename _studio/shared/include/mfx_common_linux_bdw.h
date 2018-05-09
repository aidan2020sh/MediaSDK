//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2018 Intel Corporation. All Rights Reserved.
//

#ifndef _MFX_COMMON_LINUX_BDW_H_
#define _MFX_COMMON_LINUX_BDW_H_


#define PRE_SI_GEN 9 // SKL

// h264
#define MFX_ENABLE_H264_VIDEO_DECODE

#if defined (MFX_VA)
    #define MFX_ENABLE_H265_VIDEO_DECODE
    #define MFX_ENABLE_VP8_VIDEO_DECODE_HW
    //#define MFX_ENABLE_VP9_VIDEO_DECODE_HW
    #define MFX_ENABLE_H265_VIDEO_ENCODE
#endif

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_DECODE
#endif

#define MFX_ENABLE_H264_VIDEO_ENCODE
#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN)
    #define MFX_ENABLE_H265_VIDEO_ENCODE
#endif
#define MFX_ENABLE_MVC_VIDEO_ENCODE

#if defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024
    #define MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
#endif

#if MFX_VERSION >= 1023
    #define MFX_ENABLE_H264_REPARTITION_CHECK
#endif

    // Unsupported on Linux:
    #define MFX_PROTECTED_FEATURE_DISABLE
    #define MFX_CAMERA_FEATURE_DISABLE

#if (MFX_VERSION < MFX_VERSION_NEXT)
    #define MFX_CLOSED_PLATFORMS_DISABLE
    #define MFX_EXT_DPB_HEVC_DISABLE
    #define MFX_ADAPTIVE_PLAYBACK_DISABLE
    #define MFX_FUTURE_FEATURE_DISABLE
#endif

#if (MFX_VERSION >= 1025)
    #if !defined(AS_H264LA_PLUGIN)
        #define MFX_ENABLE_MFE
    #endif
#endif

//#define MFX_ENABLE_H264_VIDEO_PAK
//#define MFX_ENABLE_H264_VIDEO_ENC
#if defined(LINUX64)
#define MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
#define MFX_ENABLE_H264_VIDEO_FEI_PREENC
#define MFX_ENABLE_H264_VIDEO_FEI_ENC
#define MFX_ENABLE_H264_VIDEO_FEI_PAK
#endif
// mpeg2
#define MFX_ENABLE_MPEG2_VIDEO_DECODE
#define MFX_ENABLE_HW_ONLY_MPEG2_DECODER
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE
#define MFX_ENABLE_MPEG2_VIDEO_PAK
#define MFX_ENABLE_MPEG2_VIDEO_ENC

//// vc1
#define MFX_ENABLE_VC1_VIDEO_DECODE

// mjpeg
#define MFX_ENABLE_MJPEG_VIDEO_DECODE
#define MFX_ENABLE_MJPEG_VIDEO_ENCODE

// vpp
#define MFX_ENABLE_DENOISE_VIDEO_VPP
#define MFX_ENABLE_IMAGE_STABILIZATION_VPP
#define MFX_ENABLE_VPP
#define MFX_ENABLE_MJPEG_WEAVE_DI_VPP
#define MFX_ENABLE_MJPEG_ROTATE_VPP
#define MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP

#define MFX_ENABLE_H264_VIDEO_ENCODE_HW
#define MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW
//#define MFX_ENABLE_H264_VIDEO_ENC_HW
#define MFX_ENABLE_MVC_VIDEO_ENCODE_HW
#if defined(AS_H264LA_PLUGIN)
#define MFX_ENABLE_LA_H264_VIDEO_HW
#endif

// H265 FEI plugin
#if defined(AS_H265FEI_PLUGIN)
#define MFX_ENABLE_H265FEI_HW
#endif

// undefine unsupported features DirtyRect and MoveRect on Linux
#undef MFX_ENABLE_AVCE_DIRTY_RECTANGLE
#undef MFX_ENABLE_AVCE_MOVE_RECTANGLE

//H265 feature
#undef MFX_ENABLE_HEVCE_HDR_SEI

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

#if defined (MFX_VA)
    #undef MFX_ENABLE_MVC_VIDEO_ENCODE // HW limitation
#endif // #if defined (MFX_VA)

#define SYNCHRONIZATION_BY_VA_MAP_BUFFER
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

#if defined(AS_HEVCD_PLUGIN) || defined(AS_HEVCE_PLUGIN) || defined(AS_VP8D_PLUGIN) || defined(AS_VP8E_PLUGIN) || defined(AS_VP9D_PLUGIN) || defined(AS_CAMERA_PLUGIN) || defined (MFX_RT) || (defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024)
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
    #undef MFX_ENABLE_SCENE_CHANGE_DETECTION_VPP
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
    #undef MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
    #undef MFX_ENABLE_VP8_VIDEO_DECODE
    #undef MFX_ENABLE_VP8_VIDEO_DECODE_HW
    #undef MFX_ENABLE_VP9_VIDEO_DECODE
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
#if defined(AS_HEVC_FEI_ENCODE_PLUGIN) && MFX_VERSION >= 1024
    #define MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE
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

#if defined (MFX_RT)
#define MFX_ENABLE_VPP
#define MFX_ENABLE_USER_DECODE
#define MFX_ENABLE_USER_ENCODE
#endif

#endif //_MFX_COMMON_LINUX_BDW_H_
