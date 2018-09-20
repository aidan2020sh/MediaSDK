//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2018 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VA_BASE_H__
#define __UMC_VA_BASE_H__

#include <vector>
#include "mfx_common.h"
#include "mfxstructures-int.h"

#if defined (UMC_VA) || defined (MFX_VA) || defined (UMC_VA_DXVA) || defined(UMC_VA_LINUX)

#ifndef UMC_VA
#   define UMC_VA
#endif

#ifndef MFX_VA
#   define MFX_VA
#endif

#if defined(LINUX32) || defined(LINUX64)
#   ifndef UMC_VA_LINUX
#       define UMC_VA_LINUX          // HW acceleration through Linux VA
#   endif
#   ifndef SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       ifdef ANDROID
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       else
#           define SYNCHRONIZATION_BY_VA_SYNC_SURFACE
#       endif
#   endif
#elif defined(_WIN32) || defined(_WIN64)
#   ifndef UMC_VA_DXVA
#       define UMC_VA_DXVA           // HW acceleration through DXVA
#   endif
#elif defined(__APPLE__)
#   ifndef UMC_VA_OSX
#      define UMC_VA_OSX
#   endif
#else
    #error unsupported platform
#endif

#endif //  defined (MFX_VA) || defined (UMC_VA)

#ifdef  __cplusplus
#include "umc_structures.h"
#include "umc_dynamic_cast.h"

#ifdef _MSVC_LANG
#pragma warning(disable : 4201)
#endif

#ifdef UMC_VA_LINUX
#include <va/va.h>
#include <va/va_dec_vp8.h>
#include <va/va_vpp.h>
#include <va/va_dec_vp9.h>
#include <va/va_dec_hevc.h>
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
#include <va/va_dec_av1.h>
#endif

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (p);
#endif
#endif


#ifdef UMC_VA_DXVA
#include <windows.h>
#endif

#if defined(_WIN32) || defined(_WIN64)
#include <initguid.h>
#endif

#ifdef UMC_VA_DXVA
#include <d3d9.h>
#include <dxva2api.h>
#include <dxva.h>
#endif


#if defined(LINUX32) || defined(LINUX64) || defined(__APPLE__)
#ifndef GUID_TYPE_DEFINED
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#define GUID_TYPE_DEFINED
#endif
#endif


namespace UMC
{

#define VA_COMBINATIONS(CODEC) \
    CODEC##_VLD     = VA_##CODEC | VA_VLD

enum VideoAccelerationProfile
{
    UNKNOWN         = 0,

    // Codecs
    VA_CODEC        = 0x00ff,
    VA_MPEG2        = 0x0001,
    VA_H264         = 0x0003,
    VA_VC1          = 0x0004,
    VA_JPEG         = 0x0005,
    VA_VP8          = 0x0006,
    VA_H265         = 0x0007,
    VA_VP9          = 0x0008,
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    VA_AV1          = 0x0009,
#endif // PRE_SI_TARGET_PLATFORM_GEN12

    // Entry points
    VA_ENTRY_POINT  = 0xfff00,
    VA_VLD          = 0x00400,

    VA_PROFILE                  = 0xff000,
#ifndef OPEN_SOURCE
    VA_PROFILE_SVC_HIGH         = 0x02000,
    VA_PROFILE_SVC_BASELINE     = 0x03000,
    VA_PROFILE_MVC              = 0x04000,
    VA_PROFILE_MVC_MV           = 0x05000,
    VA_PROFILE_MVC_STEREO       = 0x06000,
    VA_PROFILE_MVC_STEREO_PROG  = 0x07000,
    VA_PROFILE_WIDEVINE         = 0x09000,
#endif
    VA_PROFILE_422              = 0x0a000,
    VA_PROFILE_444              = 0x0b000,
    VA_PROFILE_10               = 0x10000,
    VA_PROFILE_REXT             = 0x20000,
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    VA_PROFILE_12               = 0x40000,
    VA_PROFILE_SCC              = 0x80000,
#endif

    // configurations
    VA_CONFIGURATION            = 0x0ff00000,
    VA_LONG_SLICE_MODE          = 0x00100000,
    VA_SHORT_SLICE_MODE         = 0x00200000,
    VA_ANY_SLICE_MODE           = 0x00300000,
#ifndef OPEN_SOURCE
    VA_PRIVATE_DDI_MODE         = 0x01000000,
#endif

    MPEG2_VLD       = VA_MPEG2 | VA_VLD,
    H264_VLD        = VA_H264 | VA_VLD,
    H265_VLD        = VA_H265 | VA_VLD,
    VC1_VLD         = VA_VC1 | VA_VLD,
    JPEG_VLD        = VA_JPEG | VA_VLD,
    VP8_VLD         = VA_VP8 | VA_VLD,
    HEVC_VLD        = VA_H265 | VA_VLD,
    VP9_VLD         = VA_VP9 | VA_VLD,
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    AV1_VLD         = VA_AV1 | VA_VLD,
#endif // PRE_SI_TARGET_PLATFORM_GEN12

    H265_VLD_REXT               = VA_H265 | VA_VLD | VA_PROFILE_REXT,
    H265_10_VLD_REXT            = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10,
    H265_10_VLD                 = VA_H265 | VA_VLD | VA_PROFILE_10,
    H265_VLD_422                = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_422,
    H265_VLD_444                = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_444,
    H265_10_VLD_422             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10 | VA_PROFILE_422,
    H265_10_VLD_444             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_10 | VA_PROFILE_444,

#ifndef OPEN_SOURCE
    H264_VLD_MVC                = VA_H264 | VA_VLD | VA_PROFILE_MVC,
    H264_VLD_SVC_BASELINE       = VA_H264 | VA_VLD | VA_PROFILE_SVC_BASELINE,
    H264_VLD_SVC_HIGH           = VA_H264 | VA_VLD | VA_PROFILE_SVC_HIGH,

    H264_VLD_MVC_MULTIVIEW      = VA_H264 | VA_VLD | VA_PROFILE_MVC_MV,
    H264_VLD_MVC_STEREO         = VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO,
    H264_VLD_MVC_STEREO_PROG    = VA_H264 | VA_VLD | VA_PROFILE_MVC_STEREO_PROG,

#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    H265_12_VLD_420             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12,
    H265_12_VLD_422             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12 | VA_PROFILE_422,
    H265_12_VLD_444             = VA_H265 | VA_VLD | VA_PROFILE_REXT | VA_PROFILE_12 | VA_PROFILE_444,

    H265_VLD_SCC                = VA_H265 | VA_VLD | VA_PROFILE_SCC,
    H265_VLD_444_SCC            = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_444,
    H265_10_VLD_SCC             = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_10,
    H265_10_VLD_444_SCC         = VA_H265 | VA_VLD | VA_PROFILE_SCC  | VA_PROFILE_10 | VA_PROFILE_444,
#endif //PRE_SI_TARGET_PLATFORM_GEN12

    VP9_10_VLD                  = VA_VP9 | VA_VLD | VA_PROFILE_10,
    VP9_VLD_422                 = VA_VP9 | VA_VLD | VA_PROFILE_422,
    VP9_VLD_444                 = VA_VP9 | VA_VLD | VA_PROFILE_444,
    VP9_10_VLD_422              = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_422,
    VP9_10_VLD_444              = VA_VP9 | VA_VLD | VA_PROFILE_10 | VA_PROFILE_444,
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
    VP9_12_VLD_420              = VA_VP9 | VA_VLD | VA_PROFILE_12,
    VP9_12_VLD_444              = VA_VP9 | VA_VLD | VA_PROFILE_12 | VA_PROFILE_444,
#endif //PRE_SI_TARGET_PLATFORM_GEN12

#endif // OPEN_SOURCE
};

#define MAX_BUFFER_TYPES    32
enum VideoAccelerationPlatform
{
    VA_UNKNOWN_PLATFORM = 0,

    VA_PLATFORM  = 0x0f0000,
    VA_DXVA2     = 0x020000,
    VA_LINUX     = 0x030000,
};

class UMCVACompBuffer;
class ProtectedVA;
class VideoProcessingVA;

enum eUMC_VA_Status
{
    UMC_ERR_DEVICE_FAILED         = -2000,
    UMC_ERR_DEVICE_LOST           = -2001,
    UMC_ERR_FRAME_LOCKED          = -2002
};

///////////////////////////////////////////////////////////////////////////////////
class FrameAllocator;
class VideoAcceleratorParams
{
    DYNAMIC_CAST_DECL_BASE(VideoAcceleratorParams);
public:

    VideoAcceleratorParams(void)
    {
        m_pVideoStreamInfo = NULL;
        m_iNumberSurfaces  = 0; // 0 means use default value
        m_protectedVA = 0;
        m_needVideoProcessingVA = false;
        m_surf = 0;
        m_allocator = 0;
    }

    virtual ~VideoAcceleratorParams(void){}

    VideoStreamInfo *m_pVideoStreamInfo;
    int32_t          m_iNumberSurfaces;
    int32_t          m_protectedVA;
    bool            m_needVideoProcessingVA;

    FrameAllocator  *m_allocator;

    // if extended surfaces exist
    void** m_surf;
};


class VideoAccelerator
{
    DYNAMIC_CAST_DECL_BASE(VideoAccelerator);

public:
    VideoAccelerator() :
        m_Profile(UNKNOWN),
        m_Platform(VA_UNKNOWN_PLATFORM),
        m_HWPlatform(MFX_HW_UNKNOWN),
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        m_protectedVA(0),
#endif
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        m_videoProcessingVA(0),
#endif
        m_allocator(0),
        m_bH264ShortSlice(false),
        m_bH264MVCSupport(false),
        m_isUseStatuReport(true),
        m_H265ScalingListScanOrder(1)
    {
    }

    virtual ~VideoAccelerator()
    {
        Close();
    }

    virtual Status Init(VideoAcceleratorParams* pInfo) = 0; // Initilize and allocate all resources
    virtual Status Close();
    virtual Status Reset();

    virtual Status BeginFrame   (int32_t  index) = 0; // Begin decoding for specified index

    // Begin decoding for specified index, keep in mind fieldId to sync correctly on task in next SyncTask call.
    // By default just calls BeginFrame(index). must be overridden by child class
    virtual Status BeginFrame(int32_t  index, uint32_t fieldId )
    {
        // by default just calls BeginFrame(index)
        (void)fieldId;
        return BeginFrame(index);
    }
    virtual void*  GetCompBuffer(int32_t            buffer_type,
                                 UMCVACompBuffer** buf   = NULL,
                                 int32_t            size  = -1,
                                 int32_t            index = -1) = 0; // request buffer
    virtual Status Execute      () = 0;          // execute decoding
    virtual Status ExecuteExtensionBuffer(void * buffer) = 0;
    virtual Status ExecuteStatusReportBuffer(void * buffer, int32_t size) = 0;
    virtual Status SyncTask(int32_t index, void * error = NULL) = 0;
    virtual Status QueryTaskStatus(int32_t index, void * status, void * error) = 0;
    virtual Status ReleaseBuffer(int32_t type) = 0;   // release buffer
    virtual Status EndFrame     (void * handle = 0) = 0;          // end frame

    virtual bool IsIntelCustomGUID() const = 0;
    /* TODO: is used on Linux only? On Linux there are isues with signed/unsigned return value. */
    virtual int32_t GetSurfaceID(int32_t idx) const { return idx; }

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    virtual ProtectedVA * GetProtectedVA() {return m_protectedVA;}
#endif
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    virtual VideoProcessingVA * GetVideoProcessingVA() {return m_videoProcessingVA;}
#endif

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    bool IsGPUSyncEventDisable() const
    {
        VideoAccelerationProfile codec = (VideoAccelerationProfile)(m_Profile & VA_CODEC);
        VideoAccelerationProfile profile = (VideoAccelerationProfile)(m_Profile & VA_PROFILE);
        return ((codec == VA_H265 && profile == VA_PROFILE_10) || codec == VA_VP9) && (m_HWPlatform < MFX_HW_APL);
    }
#endif

    bool IsLongSliceControl() const { return (!m_bH264ShortSlice); };
    bool IsMVCSupport() const {return m_bH264MVCSupport; };
    bool IsUseStatusReport() const { return m_isUseStatuReport; }
    void SetStatusReportUsing(bool isUseStatuReport) { m_isUseStatuReport = isUseStatuReport; }

    int32_t ScalingListScanOrder() const
    { return m_H265ScalingListScanOrder; }

    virtual void GetVideoDecoder(void **handle) = 0;

    VideoAccelerationProfile    m_Profile;          // entry point
    VideoAccelerationPlatform   m_Platform;         // DXVA, LinuxVA, etc
    eMFXHWType                  m_HWPlatform;

protected:
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    ProtectedVA       *  m_protectedVA;
#endif
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    VideoProcessingVA *  m_videoProcessingVA;
#endif
    FrameAllocator    *  m_allocator;

    bool            m_bH264ShortSlice;
    bool            m_bH264MVCSupport;
    bool            m_isUseStatuReport;
    int32_t          m_H265ScalingListScanOrder; //0 - up-right, 1 - raster . Default is 1 (raster).
};

///////////////////////////////////////////////////////////////////////////////////

class UMCVACompBuffer //: public MediaData
{
public:
    UMCVACompBuffer()
    {
        type = -1;
        BufferSize = 0;
        DataSize = 0;
        ptr = NULL;
        PVPState = NULL;

        FirstMb = -1;
        NumOfMB = -1;
        FirstSlice = -1;
    }
    virtual ~UMCVACompBuffer(){}

    // Set
    virtual Status SetBufferPointer(uint8_t *_ptr, size_t bytes)
    {
        ptr = _ptr;
        BufferSize = (int32_t)bytes;
        return UMC_OK;
    }
    virtual void SetDataSize(int32_t size);
    virtual void SetNumOfItem(int32_t num);
    virtual Status SetPVPState(void *buf, uint32_t size);

    // Get
    int32_t  GetType()       const { return type; }
    void*   GetPtr()        const { return ptr; }
    int32_t  GetBufferSize() const { return BufferSize; }
    int32_t  GetDataSize()   const { return DataSize; }
    void*   GetPVPStatePtr()const { return PVPState; }
    int32_t   GetPVPStateSize()const { return (NULL == PVPState ? 0 : sizeof(PVPStateBuf)); }

    // public fields
    int32_t      FirstMb;
    int32_t      NumOfMB;
    int32_t      FirstSlice;
    int32_t      type;

protected:
    uint8_t       PVPStateBuf[16];
    void*       ptr;
    void*       PVPState;
    int32_t      BufferSize;
    int32_t      DataSize;
};

} // namespace UMC

#endif // __cplusplus

#ifdef _MSVC_LANG
#pragma warning(default : 4201)
#endif

#endif // __UMC_VA_BASE_H__
