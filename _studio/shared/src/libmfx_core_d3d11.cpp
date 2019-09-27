// Copyright (c) 2007-2019 Intel Corporation
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

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#include <windows.h>
#include <initguid.h>
#include <DXGI.h>

#include "mfxpcp.h"
#include <libmfx_core_d3d11.h>
#include <libmfx_core_d3d9.h>
#include "mfx_utils.h"
#include "mfx_session.h"
#include "libmfx_core_interface.h"
#include "umc_va_dxva2_protected.h"
#include "ippi.h"

#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_decode_int.h"
#include "libmfx_core_hw.h"

#include "cm_mem_copy.h"

DEFINE_GUID(DXVADDI_Intel_Decode_PrivateData_Report,
            0x49761bec, 0x4b63, 0x4349, 0xa5, 0xff, 0x87, 0xff, 0xdf, 0x8, 0x84, 0x66);

D3D11VideoCORE::D3D11VideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session)
    :   CommonCORE(numThreadsAvailable, session)
    ,   m_pid3d11Adapter(nullptr)
    ,   m_bUseExtAllocForHWFrames(false)
    ,   m_pAccelerator(nullptr)
    ,   m_adapterNum(adapterNum)
    ,   m_HWType(MFX_HW_UNKNOWN)
    ,   m_GTConfig(MFX_GT_UNKNOWN)
    ,   m_bCmCopy(false)
    ,   m_bCmCopySwap(false)
    ,   m_bCmCopyAllowed(true)
    ,   m_VideoDecoderConfigCount(0)
    ,   m_Configs()
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    ,   m_bIsBlockingTaskSyncEnabled(false)
#endif
#if defined(MFX_ENABLE_MFE) && defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
    ,   m_mfeAvc()
    ,   m_mfeHevc()
#endif
{
}

D3D11VideoCORE::~D3D11VideoCORE()
{
    if (m_bCmCopy)
    {
        m_pCmCopy->Release();
        m_bCmCopy = false;
        m_bCmCopySwap = false;
    }
};

mfxStatus D3D11VideoCORE::InternalInit()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::InternalInit");
    if (m_HWType != MFX_HW_UNKNOWN)
        return MFX_ERR_NONE;

    mfxU32 platformFromDriver = 0;

    D3D11_VIDEO_DECODER_CONFIG config;
    mfxStatus sts = GetIntelDataPrivateReport(DXVA2_ModeH264_VLD_NoFGT, 0, config);
    if (sts == MFX_ERR_NONE)
        platformFromDriver = config.ConfigBitstreamRaw;

    //need to replace with specific D3D11 approach
    m_HWType = MFX::GetHardwareType(m_adapterNum, platformFromDriver);

#ifndef MFX_CLOSED_PLATFORMS_DISABLE
    if (m_HWType > MFX_HW_TGL_LP && m_HWType != MFX_HW_TGL_HP
                                 && m_HWType != MFX_HW_ADL_S
                                 && m_HWType != MFX_HW_ADL_UH)
        m_bCmCopyAllowed = false;
#endif

    m_deviceId = MFX::GetDeviceId(m_adapterNum);

    return MFX_ERR_NONE;
}

eMFXHWType D3D11VideoCORE::GetHWType()
{
    InternalInit();
    return m_HWType;
}


D3D11_VIDEO_DECODER_CONFIG* D3D11VideoCORE::GetConfig(D3D11_VIDEO_DECODER_DESC *video_desc, mfxU32 start, mfxU32 end, const GUID guid)
{
    HRESULT hr;
    for (mfxU32 i = start; i < end; i++)
    {
        D3D11_VIDEO_DECODER_CONFIG Config = {0};
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfig");
            hr = m_pD11VideoDevice->GetVideoDecoderConfig(video_desc, i, &Config);
        }
        if (FAILED(hr))
            return NULL;

        m_Configs.push_back(Config);

        if (m_Configs[i].guidConfigBitstreamEncryption == guid)
        {
            return &m_Configs[i];
        }
    }
    return NULL;
}

mfxStatus D3D11VideoCORE::GetIntelDataPrivateReport(const GUID guid, mfxVideoParam *par, D3D11_VIDEO_DECODER_CONFIG & config)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::GetIntelDataPrivateReport");
    mfxStatus mfxRes = InternalCreateDevice();
    MFX_CHECK_STS(mfxRes);

    D3D11_VIDEO_DECODER_DESC video_desc = {0};
    video_desc.Guid = DXVADDI_Intel_Decode_PrivateData_Report;
    video_desc.SampleWidth = par ? par->mfx.FrameInfo.Width : 640;
    video_desc.SampleHeight = par ? par->mfx.FrameInfo.Height : 480;
    video_desc.OutputFormat = DXGI_FORMAT_NV12;

    if (!video_desc.SampleWidth)
    {
        video_desc.SampleWidth = 640;
    }

    if (!video_desc.SampleHeight)
    {
        video_desc.SampleHeight = 480;
    }
    mfxU32 cDecoderProfiles = 0;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
        cDecoderProfiles = m_pD11VideoDevice->GetVideoDecoderProfileCount();
    }
    bool isRequestedGuidPresent = false;
    bool isIntelGuidPresent = false;

    for (mfxU32 i = 0; i < cDecoderProfiles; i++)
    {
        GUID decoderGuid;
        HRESULT hr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
            hr = m_pD11VideoDevice->GetVideoDecoderProfile(i, &decoderGuid);
        }
        if (FAILED(hr))
        {
            continue;
        }

        if (guid == decoderGuid)
            isRequestedGuidPresent = true;

        if (DXVADDI_Intel_Decode_PrivateData_Report == decoderGuid)
            isIntelGuidPresent = true;
    }

    if (!isRequestedGuidPresent)
        return MFX_ERR_NOT_FOUND;

    if (!isIntelGuidPresent) // if no required GUID - no acceleration at all
        return MFX_WRN_PARTIAL_ACCELERATION;

    UMC::AutomaticUMCMutex lock(m_guard); // protects m_Configs

    // NOTE: the following functions GetVideoDecoderConfigCount and GetVideoDecoderConfig
    // take too much time (~230us and ~560us respectively). So we cache these values.
    if (0 == m_VideoDecoderConfigCount)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
        HRESULT hr = m_pD11VideoDevice->GetVideoDecoderConfigCount(&video_desc, &m_VideoDecoderConfigCount);
        if (FAILED(hr))
            return MFX_WRN_PARTIAL_ACCELERATION;
    }

    D3D11_VIDEO_DECODER_CONFIG *video_config = NULL;

    for (mfxU32 i=0; i < m_Configs.size(); ++i)
    {
        if (m_Configs[i].guidConfigBitstreamEncryption == guid)
        {
            video_config = &m_Configs[i];
            break;
        }
    }

    if (!video_config)
    {
        video_config = GetConfig(&video_desc, (mfxU32)m_Configs.size(), m_VideoDecoderConfigCount, guid);
        if (!video_config)
            return MFX_WRN_PARTIAL_ACCELERATION;
    }

    config = *video_config;

    if (guid == DXVA2_Intel_Encode_AVC && video_config->ConfigSpatialResid8 != INTEL_AVC_ENCODE_DDI_VERSION)
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    else if (guid == DXVA2_Intel_Encode_MPEG2 && video_config->ConfigSpatialResid8 != INTEL_MPEG2_ENCODE_DDI_VERSION)
    {
        return  MFX_WRN_PARTIAL_ACCELERATION;
    }
    else if (guid == DXVA2_Intel_Encode_JPEG  && video_config->ConfigSpatialResid8 != INTEL_MJPEG_ENCODE_DDI_VERSION)
    {
        return  MFX_WRN_PARTIAL_ACCELERATION;
    }
    /*else if (guid == DXVA2_Intel_Encode_HEVC_Main)
    {
    m_HEVCEncodeDDIVersion = video_config->ConfigSpatialResid8;
    }*/

    return  MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncode)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::IsGuidSupported");
    if (!par)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxStatus sts = InternalInit();
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = InternalCreateDevice();
    if (sts < MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    D3D11_VIDEO_DECODER_CONFIG config;
    sts = GetIntelDataPrivateReport(guid, par, config);

    if (sts < MFX_ERR_NONE || sts == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;

    if (sts == MFX_ERR_NONE && !isEncode)
    {
        return CheckIntelDataPrivateReport<D3D11_VIDEO_DECODER_CONFIG>(&config, par);
    }
    else
    {
        return MFX_ERR_NONE;
    }
}
// DX11 support

mfxStatus D3D11VideoCORE::InitializeDevice(bool isTemporal)
{
    mfxStatus sts = InternalInit();
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = InternalCreateDevice();
    if (sts < MFX_ERR_NONE)
        return sts;

    if (m_pD11Device && !m_hdl && !isTemporal)
    {
        m_hdl = (mfxHDL)m_pD11Device;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::InternalCreateDevice()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::InternalCreateDevice");
    if (m_pD11Device)
        return MFX_ERR_NONE;

    HRESULT hres = S_OK;
    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;
    m_pAdapter = NULL;

    if (m_adapterNum != 0) // not default
    {
        CreateDXGIFactory(__uuidof(IDXGIFactory), (void**) (&m_pFactory));
        m_pFactory->EnumAdapters(m_adapterNum, &m_pAdapter);
        type = D3D_DRIVER_TYPE_UNKNOWN; // !!!!!! See MSDN, how to create device using not default device
    }

    hres =  D3D11CreateDevice(m_pAdapter,    // provide real adapter
        type,
        NULL,
        0,
        FeatureLevels,
        sizeof(FeatureLevels)/sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        &m_pD11Device,
        &pFeatureLevelsOut,
        &m_pD11Context);
    if (FAILED(hres))
    {
        // lets try to create dx11 device with d9 feature level
        static D3D_FEATURE_LEVEL FeatureLevels9[] = {
            D3D_FEATURE_LEVEL_9_1,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_3 };

        hres =  D3D11CreateDevice(m_pAdapter,    // provide real adapter
            D3D_DRIVER_TYPE_HARDWARE,
            NULL,
            0,
            FeatureLevels9,
            sizeof(FeatureLevels9)/sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,
            &m_pD11Device,
            &pFeatureLevelsOut,
            &m_pD11Context);

        if (FAILED(hres))
            return MFX_ERR_DEVICE_FAILED;
    }

    // default device should be thread safe
    CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;

    if (NULL == (m_pD11VideoDevice = m_pD11Device) ||
        NULL == (m_pD11VideoContext = m_pD11Context))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;

} // mfxStatus D3D11VideoCORE::CreateDevice()

mfxStatus D3D11VideoCORE::AllocFrames(mfxFrameAllocRequest *request,
                                      mfxFrameAllocResponse *response, bool isNeedCopy)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::AllocFrames");
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        MFX_CHECK_NULL_PTR2(request, response);
        mfxStatus sts = MFX_ERR_NONE;
        //mfxFrameAllocRequest temp_request = *request;

        // external allocator doesn't know how to allocate opaque surfaces
        // we can treat opaque as internal
        if (request->Type & MFX_MEMTYPE_OPAQUE_FRAME || request->Info.FourCC == MFX_FOURCC_P8)
        {
            request->Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        }

        if (IsBayerFormat(request->Info.FourCC))
        {
            // use internal allocator for R16 since creating requires
            // Intel's internal resource extensions that are not
            // exposed for external application
            request->Type |= MFX_MEMTYPE_INTERNAL_FRAME;
        }

        // Create Service - first call
        sts = InitializeDevice();
        MFX_CHECK_STS(sts);

        if (!m_bCmCopy && m_bCmCopyAllowed && isNeedCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            if (!m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device)){
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy->Release();
                m_pCmCopy.reset();
                //return MFX_ERR_DEVICE_FAILED;
            }else{
                sts = m_pCmCopy->Initialize(GetHWType());
                MFX_CHECK_STS(sts);
                m_bCmCopy = true;
            }
        }else if(m_bCmCopy){
            if(m_pCmCopy)
                m_pCmCopy->ReleaseCmSurfaces();
            else
                m_bCmCopy = false;
        }
        if(m_pCmCopy && !m_bCmCopySwap && (request->Info.FourCC == MFX_FOURCC_BGR4 || request->Info.FourCC == MFX_FOURCC_RGB4 || request->Info.FourCC == MFX_FOURCC_ARGB16 || request->Info.FourCC == MFX_FOURCC_ARGB16 || request->Info.FourCC == MFX_FOURCC_P010))
        {
            sts = m_pCmCopy->InitializeSwapKernels(GetHWType());
            m_bCmCopySwap = true;
        }

        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            return CommonCORE::AllocFrames(request, response);
        else
        {
            // external allocator
            eMFXHWType platform = GetHWType();
            bool useEncodeBindFlag = (request->Type & MFX_MEMTYPE_INTERNAL_FRAME)&&(request->Type & MFX_MEMTYPE_VIDEO_MEMORY_ENCODER_TARGET) && platform>=MFX_HW_SCL;

            //Temporal solution for SKL only to allocate frames with encoder bind flag using internal allocator
            if (m_bSetExtFrameAlloc && ! IsBayerFormat(request->Info.FourCC) && !useEncodeBindFlag && !(request->Type & MFX_MEMTYPE_INTERNAL_FRAME))
            {

                sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis,request, response);

                // if external allocator cannot allocate d3d frames - use default memory allocator
                if (MFX_ERR_UNSUPPORTED == sts || MFX_ERR_MEMORY_ALLOC == sts)
                {
                    // Default Allocator is used for internal memory allocation only
                    if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                        return sts;
                    m_bUseExtAllocForHWFrames = false;
                    sts = DefaultAllocFrames(request, response);
                    MFX_CHECK_STS(sts);

                    return sts;
                }
                // let's create video accelerator
                else if (MFX_ERR_NONE == sts)
                {
                    m_bUseExtAllocForHWFrames = true;
                    //sts = ProcessRenderTargets(request, response, &m_FrameAllocator);
                    //MFX_CHECK_STS(sts);
                    RegisterMids(response, request->Type, false);
                    if (response->NumFrameActual < request->NumFrameMin)
                    {
                        (*m_FrameAllocator.frameAllocator.Free)(m_FrameAllocator.frameAllocator.pthis, response);
                        return MFX_ERR_MEMORY_ALLOC;
                    }
                    return sts;
                }
                // error situation
                else
                {
                    m_bUseExtAllocForHWFrames = false;
                    return sts;
                }
            }
            else
            {
                // Default Allocator is used for internal memory allocation only
                if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                    return MFX_ERR_MEMORY_ALLOC;

                m_bUseExtAllocForHWFrames = false;
                sts = DefaultAllocFrames(request, response);
                MFX_CHECK_STS(sts);

                return sts;
            }
        }
    }
    catch(...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

} // mfxStatus D3D11VideoCORE::AllocFrames

mfxStatus D3D11VideoCORE::ReallocFrame(mfxFrameSurface1 *surf)
{
    MFX_CHECK_NULL_PTR1(surf);

    mfxMemId memid = surf->Data.MemId;

    if (!(surf->Data.MemType & MFX_MEMTYPE_INTERNAL_FRAME &&
        (!(surf->Data.MemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET) ||
        !(surf->Data.MemType & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))))
        return MFX_ERR_MEMORY_ALLOC;

    mfxFrameAllocator *pFrameAlloc = GetAllocatorAndMid(memid);
    if (!pFrameAlloc)
        return MFX_ERR_MEMORY_ALLOC;

    return mfxDefaultAllocatorD3D11::ReallocFrameHW(pFrameAlloc->pthis, memid, &(surf->Info));
}

mfxStatus D3D11VideoCORE::DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts = MFX_ERR_NONE;

    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
        (request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) // SW - TBD !!!!!!!!!!!!!!
    {
        // Create Service - first call
        sts = InitializeDevice();
        MFX_CHECK_STS(sts);
        mfxBaseWideFrameAllocator* pAlloc = GetAllocatorByReq(request->Type);

        // VPP, ENC, PAK can request frames for several times
        if (pAlloc && (request->Type & MFX_MEMTYPE_FROM_DECODE))
            return MFX_ERR_MEMORY_ALLOC;

        if (!pAlloc)
        {
            m_pcHWAlloc.reset(new mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator(request->Type,
                m_pD11Device,
                m_pD11Context));
            pAlloc = m_pcHWAlloc.get();
        }
        // else ???

        pAlloc->frameAllocator.pthis = pAlloc;
        sts = (*pAlloc->frameAllocator.Alloc)(pAlloc->frameAllocator.pthis,request, response);
        MFX_CHECK_STS(sts);
        sts = ProcessRenderTargets(request, response, pAlloc);
        MFX_CHECK_STS(sts);

    }
    else
    {
        return CommonCORE::DefaultAllocFrames(request, response);
    }
    ++m_NumAllocators;
    return sts;
}

mfxStatus D3D11VideoCORE::CreateVA(mfxVideoParam *param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::CreateVA");
    mfxStatus sts = MFX_ERR_NONE;
    param; request;

    m_pAccelerator.reset(new MFXD3D11Accelerator(m_pD11VideoDevice.p, m_pD11VideoContext));

    UMC::VideoStreamInfo VideoInfo;
    ConvertMFXParamsToUMC(param, &VideoInfo);

    MFXD3D11AcceleratorParams vaParams;
    vaParams.m_protectedVA = param->Protected;
    vaParams.m_pVideoStreamInfo = &VideoInfo;
    vaParams.m_iNumberSurfaces = request->NumFrameMin;
    vaParams.m_allocator = allocator;

    if (UMC::UMC_OK != m_pAccelerator->Init(&vaParams))
    {
        m_pAccelerator.reset();
        return MFX_ERR_UNSUPPORTED;
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
    auto pScheduler = (MFXIScheduler2 *)m_session->m_pScheduler->QueryInterface(MFXIScheduler2_GUID);
    MFX_CHECK(pScheduler, MFX_ERR_UNDEFINED_BEHAVIOR);
    m_pAccelerator->SetGlobalHwEvent(pScheduler->GetHwEvent());
    pScheduler->Release();
#endif


    mfxU32 hwProfile = ChooseProfile(param, GetHWType());
    MFX_CHECK(hwProfile, MFX_ERR_UNSUPPORTED);

    sts = m_pAccelerator->CreateVideoAccelerator(hwProfile, param, allocator);
    MFX_CHECK_STS(sts);

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_ANY(param->Protected) &&
        !IS_PROTECTION_CENC(param->Protected) &&
        !IS_PROTECTION_WIDEVINE(param->Protected))
    {
        HRESULT hr = m_pAccelerator->GetVideoDecoderDriverHandle(&m_DXVA2DecodeHandle);
        if (FAILED(hr))
        {
            m_pAccelerator->Close();
            return MFX_ERR_UNSUPPORTED;
        }
    }
    else
#endif
        m_DXVA2DecodeHandle = NULL;

    m_pAccelerator->GetVideoDecoder(&m_D3DDecodeHandle);
    m_pAccelerator->m_HWPlatform = m_HWType;
    return sts;
}

mfxStatus D3D11VideoCORE::CreateVideoProcessing(mfxVideoParam * param)
{
    mfxStatus sts = MFX_ERR_NONE;
#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    if (!m_vpp_hw_resmng.GetDevice()){
        sts = m_vpp_hw_resmng.CreateDevice(this);
    }
#else
    param;
    sts = MFX_ERR_UNSUPPORTED;
#endif
    return sts;
}

mfxStatus D3D11VideoCORE::ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc)
{
    RegisterMids(response, request->Type, !m_bUseExtAllocForHWFrames, pAlloc);
    m_pcHWAlloc.release();
    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::SetCmCopyStatus(bool enable)
{
    m_bCmCopyAllowed = enable;
    if (!enable)
    {
        if (m_pCmCopy)
        {
            m_pCmCopy->Release();
        }
        m_bCmCopy = false;
    }
    return MFX_ERR_NONE;
}

void* D3D11VideoCORE::QueryCoreInterface(const MFX_GUID &guid)
{
    // single instance
    if (MFXIVideoCORE_GUID == guid)
    {
        return (void*) this;
    }
    if (MFXICORED3D11_GUID == guid)
    {
        if (!m_pid3d11Adapter.get())
        {
            m_pid3d11Adapter.reset(new D3D11Adapter(this));
        }
        return (void*)m_pid3d11Adapter.get();
    }
    else if (MFXICORE_GT_CONFIG_GUID == guid)
    {
        return (void*)&m_GTConfig;
    }
    else if (MFXIHWCAPS_GUID == guid)
    {
        return (void*) &m_encode_caps;
    }
    else if (MFXIHWMBPROCRATE_GUID == guid)
    {
        return (void*) &m_encode_mbprocrate;
    }
    else if (MFXID3D11DECODER_GUID == guid)
    {
        return (void*)&m_comptr;
    }
#if defined(MFX_ENABLE_MFE) && defined(PRE_SI_TARGET_PLATFORM_GEN12P5)
    else if (MFXMFEAVCENCODER_SEARCH_GUID == guid)
    {
        if (!m_mfeAvc.get())
        {
            m_mfeAvc = (MFEDXVAEncoder*)m_session->m_pOperatorCore->QueryGUID<ComPtrCore<MFEDXVAEncoder> >(&VideoCORE::QueryCoreInterface, MFXMFEAVCENCODER_GUID);
            if (m_mfeAvc.get())
                m_mfeAvc.get()->AddRef();
        }
        return (void*)&m_mfeAvc;
    }
    else if (MFXMFEAVCENCODER_GUID == guid)
    {
        return (void*)&m_mfeAvc;
    }
    else if (MFXMFEHEVCENCODER_SEARCH_GUID == guid)
    {
        if (!m_mfeHevc.get())
        {
            m_mfeHevc = (MFEDXVAEncoder*)m_session->m_pOperatorCore->QueryGUID<ComPtrCore<MFEDXVAEncoder> >(&VideoCORE::QueryCoreInterface, MFXMFEHEVCENCODER_GUID);
            if (m_mfeHevc.get())
                m_mfeHevc.get()->AddRef();
        }
        return (void*)&m_mfeHevc;
    }
    else if (MFXMFEHEVCENCODER_GUID == guid)
    {
        return (void*)&m_mfeHevc;
    }
#endif
    else if (MFXICORECM_GUID == guid)
    {
        CmDevice* pCmDevice = NULL;
        if (!m_bCmCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            pCmDevice = m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device);
            if (!pCmDevice)
                return NULL;
            if (MFX_ERR_NONE != m_pCmCopy->Initialize(GetHWType()))
                return NULL;
            m_bCmCopy = true;
        }
        else
        {
            pCmDevice =  m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device);
        }
        return (void*)pCmDevice;
    }
    else if (MFXICORECMCOPYWRAPPER_GUID == guid)
    {
        if (!m_pCmCopy)
        {
            m_pCmCopy.reset(new CmCopyWrapper);
            if (!m_pCmCopy->GetCmDevice<ID3D11Device>(m_pD11Device)){
                //!!!! WA: CM restricts multiple CmDevice creation from different device managers.
                //if failed to create CM device, continue without CmCopy
                m_bCmCopy = false;
                m_bCmCopyAllowed = false;
                m_pCmCopy->Release();
                m_pCmCopy.reset();
                return NULL;
            }else{
                if(MFX_ERR_NONE != m_pCmCopy->Initialize(GetHWType()))
                    return NULL;
                else
                    m_bCmCopy = true;
            }
        }
        return (void*)m_pCmCopy.get();
    }
    else if (MFXICMEnabledCore_GUID == guid)
    {
        if (!m_pCmAdapter)
        {
            m_pCmAdapter.reset(new CMEnabledCoreAdapter(this));
        }
        return (void*)m_pCmAdapter.get();
    }
    else if (MFXIEXTERNALLOC_GUID == guid && m_bSetExtFrameAlloc)
        return &m_FrameAllocator.frameAllocator;
    else if (MFXICORE_API_1_19_GUID == guid)
    {
        return &m_API_1_19;
    }
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    else if (MFXBlockingTaskSyncEnabled_GUID == guid)
    {
        m_bIsBlockingTaskSyncEnabled = m_HWType > MFX_HW_SCL;
        return &m_bIsBlockingTaskSyncEnabled;
    }
#endif
    else if (MFXIFEIEnabled_GUID == guid)
        return const_cast<bool*>(&s_bHEVCFEIEnabled);

    return NULL;
}

mfxStatus D3D11VideoCORE::DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11VideoCORE::DoFastCopyWrapper");
    mfxStatus sts;

    mfxHDLPair srcHandle = {}, dstHandle = {};
    mfxMemId srcMemId, dstMemId;

    mfxFrameSurface1 srcTempSurface, dstTempSurface;

    memset(&srcTempSurface, 0, sizeof(mfxFrameSurface1));
    memset(&dstTempSurface, 0, sizeof(mfxFrameSurface1));

    // save original mem ids
    srcMemId = pSrc->Data.MemId;
    dstMemId = pDst->Data.MemId;

    mfxU8* srcPtr = GetFramePointer(pSrc->Info.FourCC, pSrc->Data);
    mfxU8* dstPtr = GetFramePointer(pDst->Info.FourCC, pDst->Data);

    srcTempSurface.Info = pSrc->Info;
    dstTempSurface.Info = pDst->Info;

    bool isSrcLocked = false;
    bool isDstLocked = false;

    if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockExternalFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }
    else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (srcMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == srcPtr)
            {
                sts = LockFrame(srcMemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);

                isSrcLocked = true;
            }
            else
            {
                srcTempSurface.Data = pSrc->Data;
                srcTempSurface.Data.MemId = 0;
            }
        }
        else if (srcMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(srcMemId, (mfxHDL*)&srcHandle);
            MFX_CHECK_STS(sts);

            srcTempSurface.Data.MemId = &srcHandle;
        }
    }

    if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockExternalFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetExternalFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }
    else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
    {
        if (dstMemType & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            if (NULL == dstPtr)
            {
                sts = LockFrame(dstMemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);

                isDstLocked = true;
            }
            else
            {
                dstTempSurface.Data = pDst->Data;
                dstTempSurface.Data.MemId = 0;
            }
        }
        else if (dstMemType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)
        {
            sts = GetFrameHDL(dstMemId, (mfxHDL*)&dstHandle);
            MFX_CHECK_STS(sts);

            dstTempSurface.Data.MemId = &dstHandle;
        }
    }

    sts = DoFastCopyExtended(&dstTempSurface, &srcTempSurface);
    MFX_CHECK_STS(sts);

    if (true == isSrcLocked)
    {
        if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
        else if (srcMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(srcMemId, &srcTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
    }

    if (true == isDstLocked)
    {
        if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
        {
            sts = UnlockExternalFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
        else if (dstMemType & MFX_MEMTYPE_INTERNAL_FRAME)
        {
            sts = UnlockFrame(dstMemId, &dstTempSurface.Data);
            MFX_CHECK_STS(sts);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D11VideoCORE::DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU8* srcPtr;
    mfxU8* dstPtr;
    pDst; pSrc;

    sts = GetFramePointerChecked(pSrc->Info, pSrc->Data, &srcPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);
    sts = GetFramePointerChecked(pDst->Info, pDst->Data, &dstPtr);
    MFX_CHECK(MFX_SUCCEEDED(sts), MFX_ERR_UNDEFINED_BEHAVIOR);

    // check that only memId or pointer are passed
    // otherwise don't know which type of memory copying is requested
    if (
        (NULL != dstPtr && NULL != pDst->Data.MemId) ||
        (NULL != srcPtr && NULL != pSrc->Data.MemId)
        )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    IppiSize roi = {IPP_MIN(pSrc->Info.Width, pDst->Info.Width), IPP_MIN(pSrc->Info.Height, pDst->Info.Height)};

    // check that region of interest is valid
    if (0 == roi.width || 0 == roi.height)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    HRESULT hRes;

    D3D11_TEXTURE2D_DESC sSurfDesc = {0};
    D3D11_MAPPED_SUBRESOURCE sLockRect = {0};

    mfxU32 srcPitch = pSrc->Data.PitchLow + ((mfxU32)pSrc->Data.PitchHigh << 16);
    mfxU32 dstPitch = pDst->Data.PitchLow + ((mfxU32)pDst->Data.PitchHigh << 16);

    bool canUseCMCopy = m_bCmCopy ? CmCopyWrapper::CanUseCmCopy(pDst, pSrc) : false;

    if (NULL != pSrc->Data.MemId && NULL != pDst->Data.MemId)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurfaceSrc = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);
            ID3D11Texture2D * pSurfaceDst = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

            size_t indexSrc = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;
            size_t indexDst = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;


            m_pD11Context->CopySubresourceRegion(pSurfaceDst, (mfxU32)indexDst, 0, 0, 0, pSurfaceSrc, (mfxU32)indexSrc, NULL);
        }

    }
    else if (NULL != pSrc->Data.MemId && NULL != dstPtr)
    {
        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopyVideoToSys(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurface = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pSrc->Data.MemId)->first);

            pSurface->GetDesc(&sSurfDesc);

            D3D11_TEXTURE2D_DESC desc = {0};

            desc.Width = sSurfDesc.Width;
            desc.Height = sSurfDesc.Height;
            desc.MipLevels = 1;
            desc.Format = sSurfDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.ArraySize = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            desc.BindFlags = 0;
            ID3D11Texture2D *pStaging;
            if ( DXGI_FORMAT_R16_TYPELESS == sSurfDesc.Format )
            {
                RESOURCE_EXTENSION extnDesc;
                ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
                static_assert (sizeof RESOURCE_EXTENSION_KEY <= sizeof extnDesc.Key,
                        "sizeof RESOURCE_EXTENSION_KEY > sizeof extnDesc.Key");
                std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);
                extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
                extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
                extnDesc.Data[0] = BayerFourCC2FormatFlag(pDst->Info.FourCC);

                hRes = SetResourceExtension(m_pD11Device, &extnDesc);
            }
            hRes = m_pD11Device->CreateTexture2D(&desc, NULL, &pStaging);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);

            size_t index = (size_t)((mfxHDLPair*)pSrc->Data.MemId)->second;

            m_pD11Context->CopySubresourceRegion(pStaging, 0, 0, 0, 0, pSurface, (mfxU32)index, NULL);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Dx11 Core Fast Copy SSE");

            do
            {
                hRes = m_pD11Context->Map(pStaging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &sLockRect);
            }
            while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);

            srcPitch = sLockRect.RowPitch;
            sts = mfxDefaultAllocatorD3D11::SetFrameData(sSurfDesc, sLockRect, &pSrc->Data);
            MFX_CHECK_STS(sts);

            mfxMemId saveMemId = pSrc->Data.MemId;
            pSrc->Data.MemId = 0;

            if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
                pDst->Info.FourCC = MFX_FOURCC_AYUV;

            sts = CoreDoSWFastCopy(pDst, pSrc, COPY_VIDEO_TO_SYS); // sw copy
            MFX_CHECK_STS(sts);

            pSrc->Data.MemId = saveMemId;
            MFX_CHECK_STS(sts);

            m_pD11Context->Unmap(pStaging, 0);

            SAFE_RELEASE(pStaging);
        }
    }
    else if (NULL != srcPtr && NULL != dstPtr)
    {
        // system memories were passed
        // use common way to copy frames

        if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
            pDst->Info.FourCC = MFX_FOURCC_AYUV;

        sts = CoreDoSWFastCopy(pDst, pSrc, COPY_SYS_TO_SYS); // sw copy
        MFX_CHECK_STS(sts);
       
    }
    else if (NULL != srcPtr && NULL != pDst->Data.MemId)
    {
        // source are placed in system memory, destination is in video memory
        // use common way to copy frames from system to video, most faster
        mfxI64 verticalPitch = (mfxI64)(pSrc->Data.UV - pSrc->Data.Y);
        verticalPitch = (verticalPitch % pSrc->Data.Pitch)? 0 : verticalPitch / pSrc->Data.Pitch;
        if ( IsBayerFormat(pSrc->Info.FourCC) )
        {
            // Only one plane is used for Bayer and vertial pitch calulation is not correct for it.
            verticalPitch = pDst->Info.Height;
        }

        if (canUseCMCopy)
        {
            sts = m_pCmCopy->CopySysToVideo(pDst, pSrc);
            MFX_CHECK_STS(sts);
        }
        else
        {
            ID3D11Texture2D * pSurface = reinterpret_cast<ID3D11Texture2D *>(((mfxHDLPair*)pDst->Data.MemId)->first);

            pSurface->GetDesc(&sSurfDesc);

            D3D11_TEXTURE2D_DESC desc = {0};

            desc.Width = sSurfDesc.Width;
            desc.Height = sSurfDesc.Height;
            desc.MipLevels = 1;
            desc.Format = sSurfDesc.Format;
            desc.SampleDesc.Count = 1;
            desc.ArraySize = 1;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            if ( DXGI_FORMAT_R16_TYPELESS == sSurfDesc.Format )
            {
                RESOURCE_EXTENSION extnDesc;
                ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
                static_assert (sizeof RESOURCE_EXTENSION_KEY <= sizeof extnDesc.Key,
                        "sizeof RESOURCE_EXTENSION_KEY > sizeof extnDesc.Key");
                std::copy(std::begin(RESOURCE_EXTENSION_KEY), std::end(RESOURCE_EXTENSION_KEY), extnDesc.Key);
                extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
                extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
                extnDesc.Data[0] = BayerFourCC2FormatFlag(pDst->Info.FourCC);
                hRes = SetResourceExtension(m_pD11Device, &extnDesc);
                MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);
            }
            ID3D11Texture2D *pStaging;
            hRes = m_pD11Device->CreateTexture2D(&desc, NULL, &pStaging);
            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_MEMORY_ALLOC);
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Dx11 Core Fast Copy ippiCopy");

            do
            {
                hRes = m_pD11Context->Map(pStaging, 0, D3D11_MAP_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT, &sLockRect);
            }
            while (DXGI_ERROR_WAS_STILL_DRAWING == hRes);

            MFX_CHECK(SUCCEEDED(hRes), MFX_ERR_DEVICE_FAILED);
            dstPitch = sLockRect.RowPitch;

            switch (pDst->Info.FourCC)
            {
            case MFX_FOURCC_R16_BGGR:
            case MFX_FOURCC_R16_RGGB:
            case MFX_FOURCC_R16_GRBG:
            case MFX_FOURCC_R16_GBRG:
                ippiCopy_16u_C1R(pSrc->Data.Y16, srcPitch, (mfxU16 *)sLockRect.pData, dstPitch, roi);
                break;
            default:
                {
                    sts = mfxDefaultAllocatorD3D11::SetFrameData(sSurfDesc, sLockRect, &pDst->Data);
                    MFX_CHECK_STS(sts);

                    mfxMemId saveMemId = pDst->Data.MemId;
                    pDst->Data.MemId = 0;

                    if (pDst->Info.FourCC == DXGI_FORMAT_AYUV)
                        pDst->Info.FourCC = MFX_FOURCC_AYUV;

                    sts = CoreDoSWFastCopy(pDst, pSrc, COPY_SYS_TO_VIDEO); // sw copy
                    MFX_CHECK_STS(sts);

                    pDst->Data.MemId = saveMemId;
                    MFX_CHECK_STS(sts);
                }
                break;
            }

            size_t index = (size_t)((mfxHDLPair*)pDst->Data.MemId)->second;

            m_pD11Context->Unmap(pStaging, 0);
            m_pD11Context->CopySubresourceRegion(pSurface, (mfxU32)index, 0, 0, 0, pStaging, 0, NULL);

            SAFE_RELEASE(pStaging);
        }
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return sts;
}

mfxStatus D3D11VideoCORE::SetHandle(mfxHandleType type, mfxHDL handle)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        // SetHandle should be first since 1.6 version
        bool isRequeredEarlySetHandle = (m_session->m_version.Major > 1 ||
            (m_session->m_version.Major == 1 && m_session->m_version.Minor >= 6));

        if (type != MFX_HANDLE_D3D11_DEVICE)
            return MFX_ERR_INVALID_HANDLE;

        if (isRequeredEarlySetHandle && m_pD11Device)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        mfxStatus sts = CommonCORE::SetHandle(type, handle);
        MFX_CHECK_STS(sts);

        m_pD11Device = (ID3D11Device*)m_hdl;
        CComPtr<ID3D11DeviceContext> pImmediateContext;
        m_pD11Device->GetImmediateContext(&pImmediateContext);
        m_pD11Context = pImmediateContext;

        m_pD11VideoDevice = m_pD11Device;
        m_pD11VideoContext = m_pD11Context;


        if (!m_pD11Context)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        CComQIPtr<ID3D10Multithread> p_mt(m_pD11Context);
        if (p_mt)
        {
            if (!p_mt->GetMultithreadProtected())
            {
                ReleaseHandle();
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else {
            ReleaseHandle();
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    catch(...)
    {
        ReleaseHandle();
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    }

    return MFX_ERR_NONE;

}


void D3D11VideoCORE::ReleaseHandle()
{
    if (m_hdl)
    {
        ((IUnknown*)m_hdl)->Release();
        m_bUseExtManager = false;
        m_hdl = 0;
    }
}

bool D3D11VideoCORE::IsCompatibleForOpaq()
{
    if (!m_bUseExtManager)
    {
        return m_session->m_pOperatorCore->HaveJoinedSessions() == false;
    }
    return true;
}

#endif
#endif
