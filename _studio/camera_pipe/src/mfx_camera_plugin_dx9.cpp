/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2015 Intel Corporation. All Rights Reserved.

File Name: mfx_camera_plugin_dx9.cpp

\* ****************************************************************************** */
#if defined (_WIN32) || defined (_WIN64)
#include "mfx_camera_plugin_dx9.h"


mfxStatus DXVAHDVideoProcessor::EnumerateCaps()
{
    HRESULT                hr = S_OK;
    DXVAHD_VPDEVCAPS    VPDeviceCaps;
    DXVAHD_VPCAPS        *pVPCaps = NULL;

    memset(&VPDeviceCaps, 0, sizeof(DXVAHD_VPDEVCAPS));
    hr = m_pDXVADevice->GetVideoProcessorDeviceCaps(&VPDeviceCaps);
    if (FAILED(hr))
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    m_dxva_caps.pInputFormats = m_dxva_caps.pOutputFormats = NULL;
    m_dxva_caps.uiVideoProcessorCount = VPDeviceCaps.VideoProcessorCount;
    m_dxva_caps.uiInputFormatCount = VPDeviceCaps.InputFormatCount;
    m_dxva_caps.uiOutputFormatCount = VPDeviceCaps.OutputFormatCount;
    m_dxva_caps.FilterCaps = VPDeviceCaps.FilterCaps;

    m_dxva_caps.bLumaKey = (VPDeviceCaps.FeatureCaps & DXVAHD_FEATURE_CAPS_LUMA_KEY);
    m_dxva_caps.bAlphaBlending = (VPDeviceCaps.FeatureCaps & DXVAHD_FEATURE_CAPS_ALPHA_FILL);
    m_dxva_caps.bConstriction = (VPDeviceCaps.FeatureCaps & DXVAHD_FEATURE_CAPS_CONSTRICTION);
    m_dxva_caps.bAlphaPalette = (VPDeviceCaps.FeatureCaps & DXVAHD_FEATURE_CAPS_ALPHA_PALETTE);

    pVPCaps = new DXVAHD_VPCAPS[m_dxva_caps.uiVideoProcessorCount];
    if (!pVPCaps)
    {
        return MFX_ERR_DEVICE_FAILED;    
    }

    memset((void*)&pVPCaps[0], 0, sizeof(DXVAHD_VPCAPS)* m_dxva_caps.uiVideoProcessorCount);
    hr = m_pDXVADevice->GetVideoProcessorCaps(m_dxva_caps.uiVideoProcessorCount,pVPCaps);
    if (FAILED(hr))
    {        
        SAFE_DELETE_ARRAY(pVPCaps);
        return MFX_ERR_DEVICE_FAILED;
    }

    m_dxva_caps.pInputFormats = new D3DFORMAT[m_dxva_caps.uiInputFormatCount];
    memset((void*)&m_dxva_caps.pInputFormats[0], 0, sizeof(D3DFORMAT)*m_dxva_caps.uiInputFormatCount);

    m_dxva_caps.pOutputFormats = new D3DFORMAT[m_dxva_caps.uiOutputFormatCount];
    memset((void*)&m_dxva_caps.pOutputFormats[0], 0, sizeof(D3DFORMAT)*m_dxva_caps.uiOutputFormatCount);

    if (!m_dxva_caps.pOutputFormats || !m_dxva_caps.pInputFormats)
    {
        SAFE_DELETE_ARRAY(pVPCaps);
        SAFE_DELETE_ARRAY(m_dxva_caps.pInputFormats);
        SAFE_DELETE_ARRAY(m_dxva_caps.pOutputFormats);
        return MFX_ERR_DEVICE_FAILED;
    }
    
    // Get the formats and filter ranges for this device.
    hr = GetDxvahdVPFormatsAndRanges(&m_dxva_caps,
                                     m_dxva_caps.pInputFormats,
                                     m_dxva_caps.pOutputFormats,
                                     &m_dxva_caps.BrightnessRange,
                                     &m_dxva_caps.ContrastRange,
                                     &m_dxva_caps.HueRange,
                                     &m_dxva_caps.SaturationRange,
                                     &m_dxva_caps.NoiseReductionRange,
                                     &m_dxva_caps.EdgeEnhancementRange);
    if (FAILED(hr))
    {
        SAFE_DELETE_ARRAY(pVPCaps);
        SAFE_DELETE_ARRAY(m_dxva_caps.pInputFormats);
        SAFE_DELETE_ARRAY(m_dxva_caps.pOutputFormats);
        return MFX_ERR_DEVICE_FAILED;;
    }

    m_dxva_caps.pVPCaps = new dxvahdvpcaps[m_dxva_caps.uiVideoProcessorCount];
    if (!m_dxva_caps.pVPCaps)
    {
        SAFE_DELETE_ARRAY(pVPCaps);
        SAFE_DELETE_ARRAY(m_dxva_caps.pInputFormats);
        SAFE_DELETE_ARRAY(m_dxva_caps.pOutputFormats);
        return MFX_ERR_DEVICE_FAILED;
    }
    memset((void*)&m_dxva_caps.pVPCaps[0], 0, sizeof(dxvahdvpcaps)*m_dxva_caps.uiVideoProcessorCount);

    for (UINT uiVPIndex = 0; uiVPIndex != m_dxva_caps.uiVideoProcessorCount; uiVPIndex++)
    {
        m_dxva_caps.pVPCaps[uiVPIndex].VPGuid = pVPCaps[uiVPIndex].VPGuid;
        m_dxva_caps.pVPCaps[uiVPIndex].uiPastFrames = pVPCaps[uiVPIndex].PastFrames;
        m_dxva_caps.pVPCaps[uiVPIndex].uiFutureFrames = pVPCaps[uiVPIndex].FutureFrames;
        m_dxva_caps.pVPCaps[uiVPIndex].ProcessorCaps = pVPCaps[uiVPIndex].ProcessorCaps;
        m_dxva_caps.pVPCaps[uiVPIndex].ITelecineCaps = pVPCaps[uiVPIndex].ITelecineCaps;        
        m_dxva_caps.pVPCaps[uiVPIndex].uiCustomRateCount = pVPCaps[uiVPIndex].CustomRateCount;        

        if (pVPCaps[uiVPIndex].CustomRateCount > 0)
        {                    
            m_dxva_caps.pVPCaps[uiVPIndex].pCustomRates = new DXVAHD_CUSTOM_RATE_DATA[m_dxva_caps.pVPCaps[uiVPIndex].uiCustomRateCount];
            if (!m_dxva_caps.pVPCaps[uiVPIndex].pCustomRates)
            {
                
                SAFE_DELETE_ARRAY(pVPCaps);
                SAFE_DELETE_ARRAY(m_dxva_caps.pInputFormats);
                SAFE_DELETE_ARRAY(m_dxva_caps.pOutputFormats);
                SAFE_DELETE_ARRAY(m_dxva_caps.pVPCaps);
                return MFX_ERR_DEVICE_FAILED;;
            }

            GUID VPGuid = pVPCaps[uiVPIndex].VPGuid;
            hr = m_pDXVADevice->GetVideoProcessorCustomRates(&VPGuid,
                                                       m_dxva_caps.pVPCaps[uiVPIndex].uiCustomRateCount,
                                                       m_dxva_caps.pVPCaps[uiVPIndex].pCustomRates); 
            if (FAILED(hr))
            {
                return MFX_ERR_DEVICE_FAILED;    
            }
        } 
    } 

    SAFE_DELETE_ARRAY(pVPCaps);

    if (FAILED(hr))
    {
        SAFE_DELETE_ARRAY(m_dxva_caps.pInputFormats);
        SAFE_DELETE_ARRAY(m_dxva_caps.pOutputFormats);
        SAFE_DELETE_ARRAY(m_dxva_caps.pVPCaps);
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

HRESULT DXVAHDVideoProcessor::GetDxvahdVPFormatsAndRanges
(
    dxvahddevicecaps          *pDeviceCaps,
    D3DFORMAT                 *pInputFormats,
    D3DFORMAT                 *pOutputFormats,
    DXVAHD_FILTER_RANGE_DATA  *pBrightnessRange,
    DXVAHD_FILTER_RANGE_DATA  *pContrastRange,
    DXVAHD_FILTER_RANGE_DATA  *pHueRange,
    DXVAHD_FILTER_RANGE_DATA  *pSaturationRange,
    DXVAHD_FILTER_RANGE_DATA  *pNoiseReductionRange,
    DXVAHD_FILTER_RANGE_DATA  *pEdgeEnhancementRange
)
{
    HRESULT               hr = S_OK;
    UINT  uiInputFormatCount = 0;
    UINT uiOutputFormatCount = 0;
    UINT        uiFilterCaps = 0;

    if (!pInputFormats || !pOutputFormats || !pBrightnessRange || !pContrastRange 
        || !pHueRange || !pSaturationRange || !pNoiseReductionRange || !pEdgeEnhancementRange
        )
    {
        return E_INVALIDARG;        
    }    
        
    uiInputFormatCount = pDeviceCaps->uiInputFormatCount;
    uiOutputFormatCount = pDeviceCaps->uiOutputFormatCount;
    uiFilterCaps = pDeviceCaps->FilterCaps;

    hr = m_pDXVADevice->GetVideoProcessorInputFormats(uiInputFormatCount,
                                                pInputFormats);
    if (FAILED(hr))
    {
        goto CleanUp;
    }    

    hr = m_pDXVADevice->GetVideoProcessorOutputFormats(uiOutputFormatCount,
                                                 pOutputFormats);
    if (FAILED(hr))
    {    
        goto CleanUp;
    }
    
    if (uiFilterCaps & DXVAHD_FILTER_CAPS_BRIGHTNESS)
    {
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_BRIGHTNESS,
                                                   pBrightnessRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bBrightnessFilter = TRUE;
    }

    if (uiFilterCaps & DXVAHD_FILTER_CAPS_CONTRAST)
    {
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_CONTRAST,
                                                   pContrastRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bContrastFilter = TRUE;
    }

    if (uiFilterCaps & DXVAHD_FILTER_CAPS_HUE)
    {
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_HUE,
                                                   pHueRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bHueFilter = TRUE;
    }

    if (uiFilterCaps & DXVAHD_FILTER_CAPS_SATURATION)
    {    
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_SATURATION,
                                                   pSaturationRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bSaturationFilter = TRUE;
    }

    if (uiFilterCaps & DXVAHD_FILTER_CAPS_NOISE_REDUCTION)
    {    
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_NOISE_REDUCTION,
                                                   pNoiseReductionRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bNoiseFilter = TRUE;
    }

    if (uiFilterCaps & DXVAHD_FILTER_CAPS_EDGE_ENHANCEMENT)
    {    
        hr = m_pDXVADevice->GetVideoProcessorFilterRange(DXVAHD_FILTER_EDGE_ENHANCEMENT,
                                                   pEdgeEnhancementRange);
        if (FAILED(hr))
        {
            goto CleanUp;
        }
        pDeviceCaps->bEdgeEnhancementFilter = TRUE;
    }

CleanUp:
    return hr;
}

HRESULT DXVAHDVideoProcessor::GetSetOutputExtension(DXVAHD_BLT_STATE state, UINT uiDataSize,
    void *pData, BOOL bSet, BOOL bCheckOnSet)
{
    HRESULT hr = S_OK;
    char *pDataToGet = NULL;

    if (bSet)
    {
        hr = m_pDXVAVideoProcessor->SetVideoProcessBltState(state, uiDataSize, pData);

        pDataToGet = new char[uiDataSize];
        if (!pDataToGet)
        {
            return E_FAIL;
        }
    }
    else
    {
        pDataToGet = (char*)pData;
    }

    // doing a GET even after a SET call to validate the call
    if (SUCCEEDED(hr) && (!bSet || bCheckOnSet))
    {
        hr = m_pDXVAVideoProcessor->GetVideoProcessBltState(state, uiDataSize, pDataToGet);
    
        if (SUCCEEDED(hr) && bSet)
        {
            if (memcmp((PVOID)pDataToGet, (PVOID)pData, uiDataSize) != 0)
            {
                hr = E_FAIL;
            }
        }
    }

    if (bSet)
    {
        SAFE_DELETE_ARRAY(pDataToGet);
    }

    return hr;
}

void DXVAHDVideoProcessor::QueryVPEGuids()
{
    VPE_GUID_ENUM guidArray;
    memset((PVOID)&guidArray, 0, sizeof(guidArray));

    guidArray.Function = VPE_FN_GUID_ENUM;
    guidArray.Version = 0;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_QUERY_GUID;
    dxvahdPData.DataSize = sizeof(guidArray);
    dxvahdPData.pData = &guidArray;

    HRESULT hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, FALSE, FALSE);
    if FAILED(hr)
    {
        return;
    }

    m_uiVPExtGuidCount = guidArray.GuidCount;

    if (!guidArray.GuidCount)
        return;

    m_pVPEGuids = (VPE_GUID_INFO*) new BYTE[guidArray.AllocSize];
    
    guidArray.pGuidArray = m_pVPEGuids;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, FALSE, FALSE);
}

HRESULT DXVAHDVideoProcessor::ExecuteVPrepFunction(TD3DHD_VPREP_FUNCTION vprepFunction, void* pData, UINT uiDataSize)
{
    HRESULT hr = S_OK;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = uiDataSize;
    dxvahdPData.pData = pData;

    switch (vprepFunction)
    {
        // not a query - do a 'Set' operation 
    case VPE_FN_SET_VERSION_PARAM:
    case VPE_FN_SET_STATUS_PARAM:
    case VPE_FN_MODE_PARAM:
    case VPE_FN_VPREP_ISTAB_PARAM:
    case VPE_FN_VPREP_SET_VARIANCE_PARAM:
        hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);
        break;

        // query caps only involve a 'Get' operation
    case VPE_FN_VPREP_QUERY_CAPS:
    case VPE_FN_VPREP_QUERY_VARIANCE_CAPS:
    case VPE_FN_GET_STATUS_PARAM:
    case VPE_FN_VPREP_GET_VARIANCE_PARAM:
        hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, FALSE, FALSE);
        break;

    default:
        hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);
        break;
    }

    return hr;
}

HRESULT DXVAHDVideoProcessor::InitVersion()
{
    // set vpe version
    VPE_FUNCTION vprepParams;
    HRESULT hr;
    memset((PVOID)&vprepParams, 0, sizeof(vprepParams));

    VPE_VERSION SetVersion;
    memset((PVOID)&SetVersion, 0, sizeof(SetVersion));
    SetVersion.Version = m_pVPEGuids[0].pVersion[0];

    vprepParams.Function = VPE_FN_SET_VERSION_PARAM;
    vprepParams.pVersion = &SetVersion;

    hr = ExecuteVPrepFunction(VPE_FN_SET_VERSION_PARAM, &vprepParams, sizeof(vprepParams));
    return hr;
}

mfxStatus DXVAHDVideoProcessor::CreateInternalDevice()
{
    D3DPRESENT_PARAMETERS d3dpp;
    memset(&d3dpp, 0, sizeof(D3DPRESENT_PARAMETERS));

    d3dpp.BackBufferWidth  = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
    d3dpp.BackBufferCount = 1;

    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = 0;
    d3dpp.Windowed = TRUE;
    d3dpp.Flags = D3DPRESENTFLAG_VIDEO;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT hres;
    {
        IDirect3D9Ex *pD3DEx = NULL;
        IDirect3DDevice9Ex  *pDirect3DDeviceEx = NULL;

        Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx); 
        if(NULL == pD3DEx)
            return MFX_ERR_DEVICE_FAILED;

        // let try to create DeviceEx ahead of regular Device
        // for details - see MSDN
        hres = pD3DEx->CreateDeviceEx(
            0,
            D3DDEVTYPE_HAL,
            0,
            D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
            &d3dpp,
            NULL,
            &pDirect3DDeviceEx);

        if (S_OK!=hres)
        {
            pD3DEx->Release();

            //1 create DX device
            m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
            if (0 == m_pD3D)
                return MFX_ERR_DEVICE_FAILED; 

            // let try to create regulare device
            hres = m_pD3D->CreateDevice(
                0,
                D3DDEVTYPE_HAL,
                0,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE | D3DCREATE_MULTITHREADED,
                &d3dpp,
                &m_pD3DDevice);
            
            if (S_OK!=hres)
            {
                m_pD3D->Release();
                return MFX_ERR_DEVICE_FAILED;
            }
            else
                return MFX_ERR_NONE;
        }
        else 
        {
            m_pD3D = pD3DEx;
            m_pD3DDevice = pDirect3DDeviceEx;
            return MFX_ERR_NONE;
        }

    }
}

mfxStatus DXVAHDVideoProcessor::CreateDevice(VideoCORE *core, mfxVideoParam *par, bool temporary)
{
    mfxStatus sts;
    HRESULT   hr;
    UNREFERENCED_PARAMETER(par);
    UNREFERENCED_PARAMETER(temporary);
    MFX_CHECK_NULL_PTR1(core);
    HANDLE DirectXHandle = 0;
    m_pD3Dmanager = 0;
    core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL *)&m_pD3Dmanager);

    if ( m_pD3Dmanager )
    {
        hr = m_pD3Dmanager->OpenDeviceHandle(&DirectXHandle);
        hr = m_pD3Dmanager->LockDevice(DirectXHandle, &(IDirect3DDevice9 *)m_pD3DDevice, true);
    }
    else
        {
        // No device handle provided from app? Forget about video out :(
        sts = CreateInternalDevice();
        MFX_CHECK_STS(sts);
    }
    memset((PVOID)&m_dxva_caps, 0, sizeof(m_dxva_caps));
    m_videoDesc.InputFrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE    ;
    m_videoDesc.InputFrameRate.Numerator = 30;
    m_videoDesc.InputFrameRate.Denominator = 1;
    m_videoDesc.InputWidth  = 720;
    m_videoDesc.InputHeight = 480;
    m_videoDesc.OutputFrameRate.Numerator = 30;
    m_videoDesc.OutputFrameRate.Denominator = 1;
    m_videoDesc.OutputWidth  = 720;
    m_videoDesc.OutputHeight = 480;

    // create the dxvahd device
    hr = DXVAHD_CreateDevice(reinterpret_cast<IDirect3DDevice9Ex*>(m_pD3DDevice),
                             &m_videoDesc,
                             DXVAHD_DEVICE_USAGE_PLAYBACK_NORMAL,
                             NULL,
                             &m_pDXVADevice);
    if ( m_pD3Dmanager && DirectXHandle)
    {
        m_pD3Dmanager->UnlockDevice(DirectXHandle, false);
        m_pD3DDevice->Release();
    }
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    hr = EnumerateCaps();
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    hr = m_pDXVADevice->CreateVideoProcessor(&(m_dxva_caps.pVPCaps[0].VPGuid), &m_pDXVAVideoProcessor);
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    memset((PVOID)&m_vprepCaps, 0, sizeof(m_vprepCaps));
    memset((void*)&m_vprepVarianceCaps, 0, sizeof(m_vprepVarianceCaps));

    VPE_GUID_ENUM guidArray;
    memset((PVOID)&guidArray, 0, sizeof(guidArray));

    guidArray.Function = VPE_FN_GUID_ENUM;
    guidArray.Version = 0;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_QUERY_GUID;
    dxvahdPData.DataSize = sizeof(guidArray);
    dxvahdPData.pData = &guidArray;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, FALSE, FALSE);
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    VPE_GUID_INFO *_pVPEGuids = (VPE_GUID_INFO*) new BYTE[guidArray.AllocSize];
    guidArray.pGuidArray = _pVPEGuids;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, FALSE, FALSE);
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    VPE_FUNCTION vprepParams;
    memset((PVOID)&vprepParams, 0, sizeof(vprepParams));

    VPE_VERSION SetVersion;
    memset((PVOID)&SetVersion, 0, sizeof(SetVersion));
    SetVersion.Version = _pVPEGuids[0].pVersion[0];

    vprepParams.Function = VPE_FN_SET_VERSION_PARAM;
    vprepParams.pVersion = &SetVersion;

    hr = ExecuteVPrepFunction(VPE_FN_SET_VERSION_PARAM, &vprepParams, sizeof(vprepParams));
    if ( FAILED(hr) )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus DXVAHDVideoProcessor::QueryCapabilities(MfxHwVideoProcessing::mfxVppCaps &caps)
{
    UNREFERENCED_PARAMETER(caps);
    // TODO: add caps 
    return MFX_ERR_NONE;
}

HRESULT DXVAHDVideoProcessor::SetVideoProcessBltState(DXVAHD_BLT_STATE State, 
                                                             UINT uiDataSize, 
                                                             const void *pData)
{
    HRESULT                                                     hr = E_FAIL;
    DXVAHD_BLT_STATE_TARGET_RECT_DATA              dxvahdBltStateTargetRect;
    DXVAHD_BLT_STATE_BACKGROUND_COLOR_DATA    dxvahdBltStateBackgroundColor;
    DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA dxvahdBltStateOutputColorSpace;
    DXVAHD_BLT_STATE_ALPHA_FILL_DATA                dxvahdBltStateAlphaFill;
    DXVAHD_BLT_STATE_CONSTRICTION_DATA           dxvahdBltStateConstriction;
    DXVAHD_BLT_STATE_PRIVATE_DATA                     dxvahdBltStatePrivate;
    UINT                                                       uiGetDataSize = 0;
    void                                                           *pGetData = NULL;

    switch(State)
    {
        case DXVAHD_BLT_STATE_TARGET_RECT:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_TARGET_RECT_DATA);
            memset(&dxvahdBltStateTargetRect, 0, uiGetDataSize);
            pGetData = &dxvahdBltStateTargetRect;
            break;
        case DXVAHD_BLT_STATE_BACKGROUND_COLOR:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_BACKGROUND_COLOR_DATA);
            memset(&dxvahdBltStateBackgroundColor, 0, uiGetDataSize);
            pGetData = &dxvahdBltStateBackgroundColor;
            break;
        case DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA);
            memset(&dxvahdBltStateOutputColorSpace, 0, uiGetDataSize);
            pGetData = &dxvahdBltStateOutputColorSpace;
            break;
        case DXVAHD_BLT_STATE_ALPHA_FILL:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_ALPHA_FILL_DATA);
            memset(&dxvahdBltStateAlphaFill, 0, uiGetDataSize);
            pGetData = &dxvahdBltStateAlphaFill;
            break;
        case DXVAHD_BLT_STATE_CONSTRICTION:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_CONSTRICTION_DATA);
            memset(&dxvahdBltStateConstriction, 0, uiGetDataSize);
            pGetData = &dxvahdBltStateConstriction;
            break;
        case DXVAHD_BLT_STATE_PRIVATE:
            uiGetDataSize = sizeof(DXVAHD_BLT_STATE_PRIVATE_DATA);
            memset(&dxvahdBltStatePrivate, 0, uiGetDataSize);
            pGetData = &dxvahdBltStatePrivate;
            break;
        default:
           return E_FAIL;
    }

    //
    // First set given Blt state
    //
    hr = m_pDXVAVideoProcessor->SetVideoProcessBltState(State,
                                                  uiDataSize,
                                                  pData);
    if (FAILED(hr))
    {
        return hr;
    }

    //
    // Next get the updated BltState
    //
    hr = m_pDXVAVideoProcessor->GetVideoProcessBltState(State,
                                                  uiGetDataSize,
                                                  pGetData);
    return hr;
}

HRESULT DXVAHDVideoProcessor::SetVideoProcessStreamState(UINT uiStreamIndex, 
                                                         DXVAHD_STREAM_STATE State,
                                                         UINT uiDataSize,
                                                         const void *pData)
{
    HRESULT                                                         hr = E_FAIL;
    DXVAHD_STREAM_STATE_D3DFORMAT_DATA               dxvahdStreamStateD3DFormat;
    DXVAHD_STREAM_STATE_FRAME_FORMAT_DATA          dxvahdStreamStateFrameFormat;
    DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA dxvahdStreamStateInputColorSpace;
    DXVAHD_STREAM_STATE_OUTPUT_RATE_DATA            dxvahdStreamStateOutputRate;
    DXVAHD_STREAM_STATE_SOURCE_RECT_DATA            dxvahdStreamStateSourceRect;
    DXVAHD_STREAM_STATE_DESTINATION_RECT_DATA  dxvahdStreamStateDestinationRect;
    DXVAHD_STREAM_STATE_ALPHA_DATA                       dxvahdStreamStateAlpha;
    DXVAHD_STREAM_STATE_PALETTE_DATA                   dxvahdStreamStatePalette = {0};
    DXVAHD_STREAM_STATE_LUMA_KEY_DATA                  dxvahdStreamStateLumaKey;
    DXVAHD_STREAM_STATE_ASPECT_RATIO_DATA          dxvahdStreamStateAspectRatio;
    DXVAHD_STREAM_STATE_FILTER_DATA                     dxvahdStreamStateFilter;
    DXVAHD_STREAM_STATE_PRIVATE_DATA                   dxvahdStreamStatePrivate;
    UINT                                                           uiGetDataSize = 0;
    void                                                               *pGetData = NULL;


    switch (State)
    {
        case DXVAHD_STREAM_STATE_D3DFORMAT:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_D3DFORMAT_DATA);
            memset(&dxvahdStreamStateD3DFormat, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateD3DFormat;
            break;
        case DXVAHD_STREAM_STATE_FRAME_FORMAT:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_FRAME_FORMAT_DATA);
            memset(&dxvahdStreamStateFrameFormat, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateFrameFormat;
            break;
        case DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA);
            memset(&dxvahdStreamStateInputColorSpace, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateInputColorSpace;
            break;
        case DXVAHD_STREAM_STATE_OUTPUT_RATE:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_OUTPUT_RATE_DATA);
            memset(&dxvahdStreamStateOutputRate, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateOutputRate;
            break;
        case DXVAHD_STREAM_STATE_SOURCE_RECT:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_SOURCE_RECT_DATA);
            memset(&dxvahdStreamStateSourceRect, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateSourceRect;
            break;
        case DXVAHD_STREAM_STATE_DESTINATION_RECT:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_DESTINATION_RECT_DATA);
            memset(&dxvahdStreamStateDestinationRect, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateDestinationRect;
            break;
        case DXVAHD_STREAM_STATE_ALPHA:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_ALPHA_DATA);
            memset(&dxvahdStreamStateAlpha, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateAlpha;
            break;
        case DXVAHD_STREAM_STATE_PALETTE:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_PALETTE_DATA);
            memset(&dxvahdStreamStatePalette, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStatePalette;
            if (((DXVAHD_STREAM_STATE_PALETTE_DATA*)pData)->Count > 0)
            {
                UINT count = ((DXVAHD_STREAM_STATE_PALETTE_DATA*)pData)->Count;
                dxvahdStreamStatePalette.Count      = count;    
                dxvahdStreamStatePalette.pEntries = (D3DCOLOR*) malloc(sizeof(UINT) * count);
            }

            break;
        case DXVAHD_STREAM_STATE_LUMA_KEY:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_LUMA_KEY_DATA);
            memset(&dxvahdStreamStateLumaKey, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateLumaKey;
            break;
        case DXVAHD_STREAM_STATE_ASPECT_RATIO:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_ASPECT_RATIO_DATA);
            memset(&dxvahdStreamStateAspectRatio, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateAspectRatio;
            break;
        case DXVAHD_STREAM_STATE_FILTER_BRIGHTNESS:
        case DXVAHD_STREAM_STATE_FILTER_CONTRAST:
        case DXVAHD_STREAM_STATE_FILTER_HUE:
        case DXVAHD_STREAM_STATE_FILTER_SATURATION:
        case DXVAHD_STREAM_STATE_FILTER_NOISE_REDUCTION:
        case DXVAHD_STREAM_STATE_FILTER_EDGE_ENHANCEMENT:
        case DXVAHD_STREAM_STATE_FILTER_ANAMORPHIC_SCALING:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_FILTER_DATA);
            memset(&dxvahdStreamStateFilter, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStateFilter;
            break;
        case DXVAHD_STREAM_STATE_PRIVATE:
            uiGetDataSize = sizeof(DXVAHD_STREAM_STATE_PRIVATE_DATA);
            memset(&dxvahdStreamStatePrivate, 0, uiGetDataSize);
            pGetData = &dxvahdStreamStatePrivate;
            break;
        default:    
            return E_FAIL;
    }

    // set given Stream state
    hr = m_pDXVAVideoProcessor->SetVideoProcessStreamState(uiStreamIndex,
                                                     State,
                                                     uiDataSize,
                                                     pData);
    if (FAILED(hr))
    {
        if (dxvahdStreamStatePalette.pEntries) 
          free(dxvahdStreamStatePalette.pEntries);
        return hr;
    }

    //
    // Next get the updated StreamState
    //
    hr = m_pDXVAVideoProcessor->GetVideoProcessStreamState(uiStreamIndex,
                                                     State,
                                                     uiGetDataSize,
                                                     pGetData);
    if (FAILED(hr))
    {
        if (dxvahdStreamStatePalette.pEntries) 
          free(dxvahdStreamStatePalette.pEntries);
        return hr;
    }

    if (dxvahdStreamStatePalette.pEntries) 
      free(dxvahdStreamStatePalette.pEntries);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeHotPixel(CameraHotPixelRemovalParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;
    THotPixel  hpParams;
    hpParams.bActive = 1;
    hpParams.PixelCountThreshold = params->uPixelCountThreshold;
    hpParams.PixelThresholdDifference = params->uPixelThresholdDifference;
    memset((PVOID)&camMode, 0, sizeof(camMode));

    camMode.Function = VPE_FN_CP_HOT_PIXEL;
    camMode.pHotPixel = &hpParams;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);
    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeVignetteCorrection(CameraVignetteCorrectionParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;
    TVignetteCorrection vignette = {0};
    vignette.bActive = 1;
    vignette.Height = params->Height;
    vignette.Width  = params->Width;
    vignette.Stride = params->Stride;

    // It's critical that app must allocate vignette correction map correctly in the memory.
    // Or maybe it's better to allocate it properly here? 
    vignette.pCorrectionMap = (TVignetteCorrectionElem *)params->pCorrectionMap;
    memset((PVOID)&camMode, 0, sizeof(camMode));

    camMode.Function = VPE_FN_CP_VIGNETTE_CORRECTION;
    camMode.pVignette = &vignette;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeBlackLevelCorrection(CameraBlackLevelParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;

    TBlackLevelCorrection  blcParams;
    blcParams.bActive = 1;
    blcParams.B  = params->uB;
    blcParams.G0 = params->uG0;
    blcParams.G1 = params->uG1;
    blcParams.R  = params->uR;

    memset((PVOID)&camMode, 0, sizeof(camMode));

    camMode.Function = VPE_FN_CP_BLACK_LEVEL_CORRECTION;
    camMode.pBlackLevel = &blcParams;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeColorCorrection(CameraCCMParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;

    memset((PVOID)&camMode, 0, sizeof(camMode));

    TColorCorrection ccmParams;
    ccmParams.bActive = 1;
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            ccmParams.CCM[i][j] = params->CCM[i][j];
        }
    }
    camMode.Function = VPE_FN_CP_COLOR_CORRECTION;
    camMode.pColorCorrection = &ccmParams;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeForwardGamma(CameraForwardGammaCorrectionParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;
    memset((PVOID)&camMode, 0, sizeof(camMode));

    m_cameraFGC.bActive = 1;
    for(int i = 0; i < 64; i++)
    {
        m_cameraFGC.pSegment[i].BlueChannelCorrectedValue  = params->Segment[i].BlueChannelCorrectedValue;
        m_cameraFGC.pSegment[i].GreenChannelCorrectedValue = params->Segment[i].GreenChannelCorrectedValue;
        m_cameraFGC.pSegment[i].RedChannelCorrectedValue   = params->Segment[i].RedChannelCorrectedValue;
        m_cameraFGC.pSegment[i].PixelValue                 = params->Segment[i].PixelValue;
    }

    camMode.Function = VPE_FN_CP_FORWARD_GAMMA_CORRECTION;
    camMode.pForwardGamma = &m_cameraFGC;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeEnable(TCameraPipeEnable bActive, BOOL bCheckOnSet/*=FALSE*/)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;
    //static GUID guid = { 0xEDD1D4B9, 0x8659, 0x4CBC, { 0xA4, 0xD6, 0x98, 0x31, 0xA2, 0x16, 0x3A, 0xC3 } };
    UNREFERENCED_PARAMETER(bCheckOnSet);
    memset((PVOID)&camMode, 0, sizeof(camMode));

    camMode.Function = VPE_FN_CP_ACTIVATE_CAMERA_PIPE;
    camMode.pActive = &bActive;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

HRESULT DXVAHDVideoProcessor::SetCameraPipeWhiteBalance(CameraWhiteBalanceParams *params)
{
    HRESULT hr = S_OK;
    TCameraPipeMode_2_6 camMode;
    TWhiteBalance  wbParams;
    wbParams.bActive = 1;
    wbParams.Mode    = WHITE_BALANCE_MODE_MANUAL; // Force manual mode
    wbParams.BlueCorrection        = params->fB;
    wbParams.RedCorrection         = params->fR;
    wbParams.GreenBottomCorrection = params->fG0;
    wbParams.GreenTopCorrection    = params->fG1;
    memset((PVOID)&camMode, 0, sizeof(camMode));

    camMode.Function = VPE_FN_CP_WHITE_BALANCE;
    camMode.pWhiteBalance = &wbParams;

    DXVAHD_BLT_STATE_PRIVATE_DATA dxvahdPData;
    memset((PVOID)&dxvahdPData, 0, sizeof(dxvahdPData));
    dxvahdPData.Guid = DXVAHD_VPE_EXEC_GUID;
    dxvahdPData.DataSize = sizeof(camMode);
    dxvahdPData.pData = &camMode;

    hr = GetSetOutputExtension(DXVAHD_BLT_STATE_PRIVATE, sizeof(dxvahdPData), &dxvahdPData, TRUE, FALSE);

    return hr;
}

mfxStatus DXVAHDVideoProcessor::Execute(MfxHwVideoProcessing::mfxExecuteParams *executeParams)
{
    HRESULT hr;
    if ( ! m_bIsSet )
    {
    DXVAHD_BLT_STATE_TARGET_RECT_DATA targetRectData;
    targetRectData.Enable = TRUE;
    targetRectData.TargetRect.bottom = executeParams->targetSurface.frameInfo.Height;
    targetRectData.TargetRect.right  = executeParams->targetSurface.frameInfo.Width;
    targetRectData.TargetRect.top  = 0;
    targetRectData.TargetRect.left = 0;
    hr = SetVideoProcessBltState(DXVAHD_BLT_STATE_TARGET_RECT, sizeof(targetRectData), &targetRectData);

    DXVAHD_BLT_STATE_BACKGROUND_COLOR_DATA    BackgroundColor = {0};
    hr = SetVideoProcessBltState(DXVAHD_BLT_STATE_BACKGROUND_COLOR, sizeof(BackgroundColor), (void*)&(BackgroundColor));

    DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA bltColorSpace;
    memset((void*) &bltColorSpace, 0, sizeof(DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE_DATA));
    hr = SetVideoProcessBltState(DXVAHD_BLT_STATE_OUTPUT_COLOR_SPACE, sizeof(bltColorSpace), (void*)&(bltColorSpace));

    DXVAHD_STREAM_STATE_FRAME_FORMAT_DATA frameFmtData;
    frameFmtData.FrameFormat = DXVAHD_FRAME_FORMAT_PROGRESSIVE;
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_FRAME_FORMAT, sizeof(frameFmtData), &frameFmtData);

    DXVAHD_STREAM_STATE_SOURCE_RECT_DATA srcRectData = {0};
    srcRectData.SourceRect.bottom = executeParams->pRefSurfaces[0].frameInfo.Height;
    srcRectData.SourceRect.right  = executeParams->pRefSurfaces[0].frameInfo.Width;
    srcRectData.Enable = TRUE;
    memcpy_s((PVOID) &srcRectData.SourceRect, sizeof(RECT), (PVOID)&targetRectData.TargetRect, sizeof(RECT));    
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_SOURCE_RECT, sizeof(srcRectData), &srcRectData);

    DXVAHD_STREAM_STATE_DESTINATION_RECT_DATA dstRectData;
    dstRectData.Enable = TRUE;
    memcpy_s((PVOID) &dstRectData.DestinationRect, sizeof(RECT), &targetRectData.TargetRect, sizeof(RECT));    
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_DESTINATION_RECT, sizeof(dstRectData), &dstRectData);

    DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA streamColorSpace;
    memset((void*) &streamColorSpace, 0, sizeof(DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE_DATA));
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_INPUT_COLOR_SPACE, sizeof(streamColorSpace), (void*)&(streamColorSpace));

    DXVAHD_STREAM_STATE_ALPHA_DATA alphaData;    
    memset((void*) &alphaData, 0, sizeof(alphaData));    
 
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_ALPHA, sizeof(alphaData), (void*)&(alphaData));

    TCameraPipeEnable cameraEnable = {0};
    cameraEnable.bActive = 1;

    hr = SetCameraPipeEnable(cameraEnable, true);

    if ( executeParams->bCameraBlackLevelCorrection )
    {
        hr = SetCameraPipeBlackLevelCorrection(&executeParams->CameraBlackLevel);
    }

    if ( executeParams->bCameraWhiteBalaceCorrection )
    {
        hr = SetCameraPipeWhiteBalance(&executeParams->CameraWhiteBalance);
    }

    if ( executeParams->bCCM )
    {
        hr = SetCameraPipeColorCorrection(&executeParams->CCMParams);
    }

    if ( executeParams->bCameraHotPixelRemoval )
    {
        hr = SetCameraPipeHotPixel(&executeParams->CameraHotPixel);
    }

    if ( executeParams->bCameraGammaCorrection )
    {
        hr = SetCameraPipeForwardGamma(&executeParams->CameraForwardGammaCorrection);
    }

    if ( executeParams->bCameraVignetteCorrection )
    {
        hr = SetCameraPipeVignetteCorrection(&executeParams->CameraVignetteCorrection);
    } 
    m_bIsSet = true;
    }
    DXVAHD_STREAM_STATE_D3DFORMAT_DATA d3dFormatData;
    d3dFormatData.Format = (D3DFORMAT)executeParams->pRefSurfaces[0].frameInfo.FourCC;
    hr = SetVideoProcessStreamState(0, DXVAHD_STREAM_STATE_D3DFORMAT, sizeof(d3dFormatData), &d3dFormatData);

    DXVAHD_STREAM_DATA* pStreams = new DXVAHD_STREAM_DATA[1];
    pStreams->Enable            = 1;
    pStreams->FutureFrames      = 0;
    pStreams->InputFrameOrField = 0;
    pStreams->OutputIndex       = 0;
    pStreams->PastFrames        = 0;
    pStreams->ppFutureSurfaces  = 0;
    pStreams->ppPastSurfaces    = 0;
    pStreams->pInputSurface     = (IDirect3DSurface9 *)executeParams->pRefSurfaces[0].hdl.first;

    IDirect3DSurface9 *output = (IDirect3DSurface9 *)executeParams->targetSurface.hdl.first;
    hr = m_pDXVAVideoProcessor->VideoProcessBltHD(output,
                                                  0,
                                                  1,
                                                  pStreams);

    SAFE_DELETE_ARRAY(pStreams);
    return MFX_ERR_NONE;
}

mfxStatus DXVAHDVideoProcessor::Register(mfxHDLPair* pSurfaces, mfxU32 num, BOOL bRegister)
{
    pSurfaces; num; bRegister;

    return MFX_ERR_NONE;

} 

IDirect3DSurface9* DXVAHDVideoProcessor::SurfaceCreate(unsigned int width, 
                                                       unsigned int height, 
                                                       D3DFORMAT fmt, 
                                                       mfxU32 type)
{
  HRESULT hr;
  DWORD dxvaType; // DXVA2_VideoProcessorRenderTarget;
  IDirect3DSurface9 * surf = 0;
  
  switch (type)
  {
  case MFX_MEMTYPE_FROM_VPPOUT:
    dxvaType = DXVA2_VideoProcessorRenderTarget; 
    break;
  case MFX_MEMTYPE_FROM_VPPIN:
    dxvaType = DXVA2_VideoDecoderRenderTarget;
    break;
  case DXVAHD_SURF_OUTPUT:
    dxvaType = DXVA2_VideoSoftwareRenderTarget;
    break;
  case DXVAHD_SURF_INPUT:
    dxvaType = DXVA2_VideoSoftwareRenderTarget;
    break;
  default:
    type = DXVAHD_SURF_VIDEO_INPUT;
    dxvaType = DXVA2_VideoDecoderRenderTarget;
  }

    switch (type)
    {
        case DXVAHD_SURF_OUTPUT:
            //
            // Create the RT Surface 
            //
            hr = m_pD3DDevice->CreateRenderTarget
            (
                width,
                height,
                fmt,
                D3DMULTISAMPLE_NONE,
                0,
                TRUE,
                &surf,
                NULL
            );
            break;
        case DXVAHD_SURF_INPUT:
            //
            // Create an Offscreen Plain surface for input surface type
            //
            hr = m_pD3DDeviceEx->CreateOffscreenPlainSurface
            (
                width,
                height,
                fmt,
                D3DPOOL_DEFAULT,
                &surf,
                NULL
            );
            break;
        case MFX_MEMTYPE_FROM_VPPIN:
            hr = m_pDXVADevice->CreateVideoSurface
            (
                width,
                height,
                fmt,
                D3DPOOL_DEFAULT,
                0,
                DXVAHD_SURFACE_TYPE_VIDEO_INPUT,
                1,
                &surf,
                NULL
            );
            break;
        case MFX_MEMTYPE_FROM_VPPOUT:
            hr = m_pDXVADevice->CreateVideoSurface
            (
                width,
                height,
                fmt,
                D3DPOOL_DEFAULT,
                0,
                DXVAHD_SURFACE_TYPE_VIDEO_OUTPUT,
                1,
                &surf,
                NULL
            );
            break;
        default:
            hr = E_FAIL;
    }

    if (FAILED(hr))
    {
        return 0;        
    }    

  return surf;    
}


mfxStatus D3D9CameraProcessor::Init(CameraParams *CameraParams)
{
    MFX_CHECK_NULL_PTR1(CameraParams);
    mfxStatus sts = MFX_ERR_NONE;
    if ( ! m_core )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if ( MFX_HW_D3D9 != m_core->GetVAType() )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    m_CameraParams = *CameraParams;
    m_params       = m_CameraParams.par;
    if ( ! m_ddi.get() )
    {
    m_ddi.reset( new DXVAHDVideoProcessor() );
    if (0 == m_ddi.get())
    {
        return MFX_WRN_PARTIAL_ACCELERATION;
    }
    sts = m_ddi->CreateDevice( m_core, &m_params, false);
    MFX_CHECK_STS( sts );

    MfxHwVideoProcessing::mfxVppCaps caps = {0};
    sts = m_ddi->QueryCapabilities( caps );
    MFX_CHECK_STS( sts );

        m_pCmCopy.reset(new CmCopyWrapper);
        m_pD3Dmanager = 0;
        m_core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL *)&m_pD3Dmanager);
        if ( m_pD3Dmanager )
        {
            if (!m_pCmCopy.get()->GetCmDevice<IDirect3DDeviceManager9>(m_pD3Dmanager))
            {
                m_pCmCopy.get()->Release();
                m_pCmCopy.reset();
                return MFX_WRN_PARTIAL_ACCELERATION;
            }
            else
            {
                sts = m_pCmCopy.get()->Initialize();
                MFX_CHECK_STS(sts);
            }
        }
    }
    m_AsyncDepth = CameraParams->par.AsyncDepth;

    // Camera Pipe expects a system memory as input. Need to allocate
    m_executeParams.resize(m_AsyncDepth);
    m_executeSurf.resize( m_AsyncDepth );

    // Directly create DXVAHD surfaces that will be used as input for Blt
    m_inputSurf = new IDirect3DSurface9 *[m_AsyncDepth];
    m_lock_map  = new bool[m_AsyncDepth];
    for(int i = 0; i < m_AsyncDepth; i++)
    {
        m_inputSurf[i] = m_ddi->SurfaceCreate(m_params.vpp.In.Width, m_params.vpp.In.Height, (D3DFORMAT)BayerToFourCC(CameraParams->Caps.BayerPatternType), MFX_MEMTYPE_FROM_VPPIN);
        MFX_CHECK_NULL_PTR1(m_inputSurf[i]);
        m_lock_map[i] = false;
    }

    m_systemMemOut = false;
    if ( CameraParams->par.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY )
    {
        m_systemMemOut = true;
        m_outputSurf   = m_ddi->SurfaceCreate(m_params.vpp.Out.Width, m_params.vpp.Out.Height, (CameraParams->par.vpp.Out.FourCC == MFX_FOURCC_ARGB16 ) ? D3DFMT_A16B16G16R16 : D3DFMT_A8R8G8B8, MFX_MEMTYPE_FROM_VPPOUT);
        MFX_CHECK_NULL_PTR1(m_outputSurf);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D9CameraProcessor::AsyncRoutine(AsyncParams *pParam)
{
    mfxStatus sts;

    MfxHwVideoProcessing::mfxExecuteParams tmpParams = {};//(MfxHwVideoProcessing::mfxExecuteParams *)malloc(sizeof(MfxHwVideoProcessing::mfxExecuteParams));
    MfxHwVideoProcessing::mfxDrvSurface    tmpSurf = {};
    
    // [1] Make required actions for the input surface
    mfxU32 surfInIndex;
    sts = PreWorkInSurface(pParam->surf_in, &surfInIndex, &tmpSurf);
    MFX_CHECK_STS(sts);

    // [2] Make required actions for the output surface
    mfxU32 surfOutIndex = 0;
    sts = PreWorkOutSurface(pParam->surf_out, &surfOutIndex, &tmpParams);
    MFX_CHECK_STS(sts);
    
    // Turn on camera
    tmpParams.bCameraPipeEnabled      = true;

    // [3] Fill in params
    if ( pParam->Caps.bBlackLevelCorrection )
    {
        tmpParams.bCameraBlackLevelCorrection = true;
        tmpParams.CameraBlackLevel.uB  = (mfxU32)pParam->BlackLevelParams.BlueLevel;
        tmpParams.CameraBlackLevel.uG0 = (mfxU32)pParam->BlackLevelParams.GreenBottomLevel;
        tmpParams.CameraBlackLevel.uG1 = (mfxU32)pParam->BlackLevelParams.GreenTopLevel;
        tmpParams.CameraBlackLevel.uR  = (mfxU32)pParam->BlackLevelParams.RedLevel;
    }

    if ( pParam->Caps.bWhiteBalance )
    {
        tmpParams.bCameraWhiteBalaceCorrection = true;
        tmpParams.CameraWhiteBalance.fB  = (mfxF32)pParam->WBparams.BlueCorrection;
        tmpParams.CameraWhiteBalance.fG0 = (mfxF32)pParam->WBparams.GreenBottomCorrection;
        tmpParams.CameraWhiteBalance.fG1 = (mfxF32)pParam->WBparams.GreenTopCorrection;
        tmpParams.CameraWhiteBalance.fR  = (mfxF32)pParam->WBparams.RedCorrection;
    }

    if ( pParam->Caps.bHotPixel )
    {
        tmpParams.bCameraHotPixelRemoval = true;
        tmpParams.CameraHotPixel.uPixelCountThreshold = pParam->HPParams.PixelCountThreshold;
        tmpParams.CameraHotPixel.uPixelThresholdDifference = pParam->HPParams.PixelThresholdDifference;
    }

    if ( pParam->Caps.bColorConversionMatrix )
    {
        tmpParams.bCCM = true;
        for(int i = 0; i < 3; i++)
        {
            for(int j = 0; j < 3; j++)
            {
                tmpParams.CCMParams.CCM[i][j] = (mfxF32)pParam->CCMParams.CCM[i][j];
            }
        }
    }

    if ( pParam->Caps.bForwardGammaCorrection )
    {
        tmpParams.bCameraGammaCorrection = true;
        for(int i = 0; i < 64; i++)
        {
            tmpParams.CameraForwardGammaCorrection.Segment[i].PixelValue = pParam->GammaParams.gamma_lut.gammaPoints[i];
            tmpParams.CameraForwardGammaCorrection.Segment[i].BlueChannelCorrectedValue  = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            tmpParams.CameraForwardGammaCorrection.Segment[i].GreenChannelCorrectedValue = pParam->GammaParams.gamma_lut.gammaCorrect[i];
            tmpParams.CameraForwardGammaCorrection.Segment[i].RedChannelCorrectedValue   = pParam->GammaParams.gamma_lut.gammaCorrect[i];
        }
    }

    if ( pParam->Caps.bVignetteCorrection )
    {
        MFX_CHECK_NULL_PTR1(pParam->VignetteParams.pCorrectionMap);
        tmpParams.bCameraVignetteCorrection = 1;
        tmpParams.CameraVignetteCorrection.Height = pParam->VignetteParams.Height;
        tmpParams.CameraVignetteCorrection.Width  = pParam->VignetteParams.Width;
        tmpParams.CameraVignetteCorrection.Stride = pParam->VignetteParams.Stride;
        tmpParams.CameraVignetteCorrection.pCorrectionMap = (CameraVignetteCorrectionElem *)pParam->VignetteParams.pCorrectionMap;
    }
    

    tmpParams.refCount       = 1;
    tmpParams.bkwdRefCount   = tmpParams.fwdRefCount = 0;
    tmpParams.statusReportID = surfInIndex;

    memcpy_s(&m_executeParams[surfInIndex], sizeof(MfxHwVideoProcessing::mfxExecuteParams), &tmpParams, sizeof(MfxHwVideoProcessing::mfxExecuteParams));
    memcpy_s(&m_executeSurf[surfInIndex], sizeof(MfxHwVideoProcessing::mfxDrvSurface), &tmpSurf, sizeof(MfxHwVideoProcessing::mfxDrvSurface));

    m_executeParams[surfInIndex].pRefSurfaces = &m_executeSurf[surfInIndex];
    
    pParam->surfInIndex = surfInIndex;
    pParam->surfOutIndex = surfOutIndex;

    return sts;
}

mfxStatus D3D9CameraProcessor::CompleteRoutine(AsyncParams * pParam)
{
    mfxStatus sts = MFX_ERR_NONE;
    UNREFERENCED_PARAMETER(pParam);
    
    mfxU16 inIndex  = pParam->surfInIndex;
    mfxU16 outIndex = pParam->surfOutIndex;
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        sts = m_ddi->Execute(&m_executeParams[inIndex]);
        m_lock_map[inIndex] = false;
    }
    m_InSurfacePool->Unlock(inIndex);
    m_OutSurfacePool->Unlock(outIndex);
    m_core->DecreaseReference(&(pParam->surf_out->Data));
    m_core->DecreaseReference(&(pParam->surf_in->Data));
    return sts;
}

mfxStatus D3D9CameraProcessor::CheckIOPattern(mfxU16  IOPattern)
{
    if (IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if (IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY ||
        IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}

mfxStatus D3D9CameraProcessor::PreWorkOutSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex,  MfxHwVideoProcessing::mfxExecuteParams *params)
{

    mfxHDLPair hdl;
    mfxStatus sts;
    mfxMemId  memIdOut = (mfxMemId)1;
    bool      bExternal = true;
    
    if ( m_systemMemOut )
    {
        // Request surface from internal pool
        memIdOut = AcquireResource(*m_OutSurfacePool, poolIndex);
        sts = m_core->GetFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = false;
    }
    else
    {
        memIdOut = surf->Data.MemId; 
        // [1] Get external surface handle
        sts = m_core->GetExternalFrameHDL(memIdOut, (mfxHDL *)&hdl);
        MFX_CHECK_STS(sts);
        bExternal = true;
    }

    // [2] Get surface handle
    sts = m_ddi->Register(&hdl, 1, TRUE);
    MFX_CHECK_STS(sts);

    // [3] Fill in Drv surfaces
    params->targetSurface.memId     = memIdOut;
    params->targetSurface.bExternal = bExternal;
    params->targetSurface.hdl       = static_cast<mfxHDLPair>(hdl);
    params->targetSurface.frameInfo = surf->Info;

    return MFX_ERR_NONE;
}

mfxStatus D3D9CameraProcessor::PreWorkInSurface(mfxFrameSurface1 *surf, mfxU32 *poolIndex, MfxHwVideoProcessing::mfxDrvSurface *drvSurf)
{
    mfxStatus sts;
    int input_index = m_AsyncDepth;

    // Request surface from internal pool
    while(input_index == m_AsyncDepth )
    {
        {
            UMC::AutomaticUMCMutex guard(m_guard);
            for ( int i = 0; i < m_AsyncDepth; i++)
                if ( m_lock_map[i] == false )
                {
                    input_index = i;
                    m_lock_map[i] = true;
                    break;
                }
        }
        vm_time_sleep(10);
    }
    *poolIndex = input_index;
    mfxFrameSurface1 InSurf = {0};
    
    InSurf.Info       = surf->Info;
    InSurf.Info.FourCC =  BayerToFourCC(m_CameraParams.Caps.BayerPatternType);

    // [1] Copy from system mem to the internal video frame
    IppiSize roi = {surf->Info.Width, surf->Info.Height};
    mfxI64 verticalPitch = (mfxI64)(surf->Data.UV - surf->Data.Y);
    verticalPitch = (verticalPitch % surf->Data.Pitch)? 0 : verticalPitch / surf->Data.Pitch;
    sts = m_pCmCopy.get()->CopySystemToVideoMemoryAPI(m_inputSurf[*poolIndex], 0, surf->Data.Y, surf->Data.Pitch, (mfxU32)verticalPitch, roi);
    MFX_CHECK_STS(sts);

    // Fill in Drv surfaces
    InSurf.Info       = surf->Info;
    InSurf.Info.FourCC = BayerToFourCC(m_CameraParams.Caps.BayerPatternType);
    drvSurf->hdl.first = m_inputSurf[*poolIndex];
    drvSurf->bExternal = false;
    drvSurf->frameInfo = InSurf.Info;
    drvSurf->memId     = 0;

    return MFX_ERR_NONE;
}

mfxU32 D3D9CameraProcessor::BayerToFourCC(mfxU32 Bayer)
{
    switch(Bayer)
    {
    case BGGR:
        return D3DFMT_IRW0;
    case RGGB:
        return D3DFMT_IRW1;
    case GRBG:
        return D3DFMT_IRW2;
    case GBRG:
        return D3DFMT_IRW3;
    }
    return 0;
}
#endif