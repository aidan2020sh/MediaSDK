/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2006-2014 Intel Corporation. All Rights Reserved.
*/

#pragma once

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA

#include <vector>

#include "umc_jpeg_ddi.h"
#include "umc_svc_ddi.h"
#include "umc_vp8_ddi.h"
#include "umc_mvc_ddi.h"
#define DEFINE_GUID_(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    static const GUID name = { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }

/* The same guids w/o prefix 's' should be in dxva.lib */
DEFINE_GUID_(sDXVA2_ModeMPEG2_IDCT,        0xbf22ad00,0x03ea,0x4690,0x80,0x77,0x47,0x33,0x46,0x20,0x9b,0x7e);
DEFINE_GUID_(sDXVA2_ModeMPEG2_VLD,         0xee27417f,0x5e28,0x4e65,0xbe,0xea,0x1d,0x26,0xb5,0x08,0xad,0xc9);
DEFINE_GUID_(sDXVA2_ModeH264_MoComp_NoFGT, 0x1b81be64,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeH264_VLD_NoFGT,    0x1b81be68,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);

DEFINE_GUID_(sDXVA2_ModeVC1_MoComp,        0x1b81beA1,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeVC1_VLD,           0x1b81beA3,0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID_(sDXVA2_ModeMPEG4_VLD,         0x8ccf025a,0xbacf,0x4e44,0x81,0x16,0x55,0x21,0xd9,0xb3,0x94,0x07);

DEFINE_GUID_(sDXVA2_Intel_ModeVC1_D,       0xBCC5DB6D,0xA2B6,0x4AF0,0xAC, 0xE4,0xAD,0xB1,0xF7,0x87,0xBC,0x89);
DEFINE_GUID_(sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT,    0x604f8e68,0x4951,0x4c54,0x88,0xfe,0xab,0xd2,0x5c,0x15,0xb3,0xd6);
DEFINE_GUID_(sDXVA2_Intel_IVB_ModeJPEG_VLD_NoFGT,    0x91cd2d6e,0x897b,0x4fa1,0xb0,0xd7,0x51,0xdc,0x88,0x01,0x0e,0x0a);
DEFINE_GUID_(sDXVA_Intel_ModeVP8_VLD, 0x442b942a, 0xb4d9, 0x4940, 0xbc, 0x45, 0xa8, 0x82, 0xe5, 0xf9, 0x19, 0xf3);

DEFINE_GUID_(sDXVA_Intel_ModeH264_VLD_MVC, 0xe296bf50, 0x8808, 0x4ff8, 0x92, 0xd4, 0xf1, 0xee, 0x79, 0x9f, 0xc3, 0x3c);

DEFINE_GUID_(sDXVA2_Intel_ModeVC1_D_Super,       0xE07EC519,0xE651,0x4cd6,0xAC, 0x84,0x13,0x70,0xCC,0xEE,0xC8,0x51);

DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline,          0x9b8175d4, 0xd670, 0x4cf2, 0xa9, 0xf0, 0xfa, 0x56, 0xdf, 0x71, 0xa1, 0xae);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Baseline,                      0xc30700c4, 0xe384, 0x43e0, 0xb9, 0x82, 0x2d, 0x89, 0xee, 0x7f, 0x77, 0xc4);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High,              0x8efa5926, 0xbd9e, 0x4b04, 0x8b, 0x72, 0x8f, 0x97, 0x7d, 0xc4, 0x4c, 0x36);
DEFINE_GUID_(sDXVA_ModeH264_VLD_SVC_Scalable_High,                          0x728012c9, 0x66a8, 0x422f, 0x97, 0xe9, 0xb5, 0xe3, 0x9b, 0x51, 0xc0, 0x53);

DEFINE_GUID_(sDXVA_ModeH264_VLD_Multiview_NoFGT,                  0x705b9d82, 0x76cf, 0x49d6, 0xb7, 0xe6, 0xac, 0x88, 0x72, 0xdb, 0x01, 0x3c);
DEFINE_GUID_(sDXVA_ModeH264_VLD_Stereo_NoFGT,                     0xf9aaccbb, 0xc2b6, 0x4cfc, 0x87, 0x79, 0x57, 0x07, 0xb1, 0x76, 0x05, 0x52);
DEFINE_GUID_(sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT,         0xd79be8da, 0x0cf1, 0x4c81, 0xb8, 0x2a, 0x69, 0xa4, 0xe2, 0x36, 0xf4, 0x3d);

DEFINE_GUID(DXVA_Intel_ModeHEVC_VLD_MainProfile,  0x8c56eb1e, 0x2b47, 0x466f, 0x8d, 0x33, 0x7d, 0xbc, 0xd6, 0x3f, 0x3d, 0xf2);

struct GuidProfile
{
    Ipp32s      profile;
    GUID        guid;

    static const GuidProfile * GetGuidProfiles();
    static const GuidProfile * GetGuidProfile(size_t k);
    static size_t GetGuidProfileSize();
};

namespace UMC
{

///////////////////////////////////////////////////////////////////////////////////
class DXVA2Accelerator : public VideoAccelerator
{
    DYNAMIC_CAST_DECL(DXVA2Accelerator, VideoAccelerator);

public:
    DXVA2Accelerator();
    virtual ~DXVA2Accelerator();

    virtual Status FindConfiguration(VideoStreamInfo *pVideoInfo);
    virtual Status Init(VideoAcceleratorParams *pParams);
    virtual Status GetInfo(VideoAcceleratorParams* pInfo);
    virtual Status CloseDirectXDecoder();

    virtual Status BeginFrame(Ipp32s index);
    virtual void* GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf = NULL, Ipp32s size = -1, Ipp32s index = -1);
    virtual Status Execute();
    virtual Status ExecuteExtensionBuffer(void * buffer);
    virtual Status ExecuteStatusReportBuffer(void * buffer, Ipp32s size);
    virtual Status SyncTask(Ipp32s index) { index; return UMC_ERR_UNSUPPORTED;}
    virtual Status QueryTaskStatus(Ipp32s , void *, void * ) { return UMC_ERR_UNSUPPORTED;}
    virtual Status ReleaseBuffer(Ipp32s type);
    virtual Status EndFrame(void * handle = 0);
    virtual Status DisplayFrame(Ipp32s index, VideoData *pOutputVideoData);
    virtual Status Reset();
    Status InternalReset();

    virtual bool IsIntelCustomGUID() const;

    void SetDeviceManager(IDirect3DDeviceManager9 *dm)
    {
        m_bIsExtManager = true;
        m_pDirect3DDeviceManager9=dm;
    };
    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDXVAVideoDecoder;
    };

protected:

    HRESULT CreateDecoder(IDirectXVideoDecoderService   *m_pDecoderService,
                          GUID                          guid,
                          DXVA2_VideoDesc               *pVideoDesc,
                          DXVA2_ConfigPictureDecode     *pDecoderConfig,
                          int                           cNumberSurfaces);

    VideoStreamInfo         m_VideoStreamInfo;
    BOOL                    m_bInitilized;
    BOOL                    m_bAllocated;
    GUID                    m_guidDecoder;
    DXVA2_ConfigPictureDecode m_Config;
    DXVA2_VideoDesc         m_videoDesc;
    UMCVACompBuffer         m_pCompBuffer[MAX_BUFFER_TYPES];

    IDirect3DDeviceManager9 *m_pDirect3DDeviceManager9;
    IDirectXVideoDecoderService *m_pDecoderService;
    IDirectXVideoDecoder    *m_pDXVAVideoDecoder;
    HANDLE                  m_hDevice;
    std::vector<IDirect3DSurface9*>  m_surfaces;

protected:
    bool    m_bIsExtSurfaces;
    bool    m_bIsExtManager;

    std::vector<Ipp32s>  m_bufferOrder;
};

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#endif

#define UMC_CHECK_HRESULT(hr, DESCRIPTION)  \
{                                           \
    vm_trace_x(hr);                         \
    /*assert(SUCCEEDED(hr));*/              \
    if (FAILED(hr)) return UMC_ERR_FAILED;  \
}

#define CHECK_HR(hr)  UMC_CHECK_HRESULT(hr, #hr)

template <typename T>
bool CheckDXVAConfig(Ipp32s profile_flags, T *config, ProtectedVA * protectedVA)
{
    bool res = false;

    Ipp32s profile = (profile_flags & (VA_ENTRY_POINT | VA_CODEC));

    if (profile == JPEG_VLD || profile == VP8_VLD)
    {
        //JPEG_DECODING_CAPS * info = (JPEG_DECODING_CAPS *)pConfig;
        return true;
    }

    if (protectedVA)
    {
        if (config->guidConfigBitstreamEncryption != protectedVA->GetEncryptionGUID())
            return false;
    }
    else
    {
        /*if (pConfig->guidConfigBitstreamEncryption != DXVA_NoEncrypt ||
            pConfig->guidConfigMBcontrolEncryption != DXVA_NoEncrypt ||
            pConfig->guidConfigResidDiffEncryption != DXVA_NoEncrypt)
            return false;*/
    }

    switch(profile)
    {
    case VC1_MC:
    case JPEG_VLD:
        res = true;
        break;
    case MPEG2_VLD:
    case VC1_VLD:
        res = (1 == config->ConfigBitstreamRaw);
        break;

    case H264_VLD:
    case H264_VLD_MVC:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw);
        break;
    case H264_VLD_MVC_MULTIVIEW:
    case H264_VLD_MVC_STEREO:
    case H264_VLD_MVC_STEREO_PROG:
    case H264_VLD_SVC_BASELINE:
    case H264_VLD_SVC_HIGH:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw) || (3 == config->ConfigBitstreamRaw) || (5 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw) || (4 == config->ConfigBitstreamRaw) || (6 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw) || (3 == config->ConfigBitstreamRaw || 4 == config->ConfigBitstreamRaw) ||
             (5 == config->ConfigBitstreamRaw || 6 == config->ConfigBitstreamRaw);
        break;

    case H265_VLD:
        if (profile_flags & VA_LONG_SLICE_MODE)
            res = (2 == config->ConfigBitstreamRaw);
        else if (profile_flags & VA_SHORT_SLICE_MODE)
            res = (1 == config->ConfigBitstreamRaw);
        else
            res = (1 == config->ConfigBitstreamRaw || 2 == config->ConfigBitstreamRaw);
        break;

    default:
        break;
    }

    return res;
};

} //namespace UMC

#endif // UMC_VA_DXVA
