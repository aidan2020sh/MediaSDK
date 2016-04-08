//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008 - 2016 Intel Corporation. All Rights Reserved.
//

#include <string>
#include <algorithm>
#include <assert.h>


#include "sample_vpp_utils.h"
#include "mfxvideo++.h"
#include "vm/time_defs.h"
#include "sample_utils.h"

#include "sample_vpp_pts.h"

#include "sysmem_allocator.h"
#include "d3d_allocator.h"

#include "d3d11_allocator.h"



#define MFX_CHECK_STS(sts) {if (MFX_ERR_NONE != sts) return sts;}

#undef min

/* ******************************************************************* */

static
void WipeFrameProcessor(sFrameProcessor* pProcessor);

static
void WipeMemoryAllocator(sMemoryAllocator* pAllocator);

/* ******************************************************************* */

static
const msdk_char* FourCC2Str( mfxU32 FourCC )
{
    switch ( FourCC )
    {
    case MFX_FOURCC_NV12:
        return MSDK_STRING("NV12");
    case MFX_FOURCC_YV12:
        return MSDK_STRING("YV12");
    case MFX_FOURCC_YUY2:
        return MSDK_STRING("YUY2");
    case MFX_FOURCC_RGB3:
        return MSDK_STRING("RGB3");
    case MFX_FOURCC_RGB4:
        return MSDK_STRING("RGB4");
    case MFX_FOURCC_YUV400:
        return MSDK_STRING("YUV400");
    case MFX_FOURCC_YUV411:
        return MSDK_STRING("YUV411");
    case MFX_FOURCC_YUV422H:
        return MSDK_STRING("YUV422H");
    case MFX_FOURCC_YUV422V:
        return MSDK_STRING("YUV422V");
    case MFX_FOURCC_YUV444:
        return MSDK_STRING("YUV444");
    case MFX_FOURCC_P010:
        return MSDK_STRING("P010");
    case MFX_FOURCC_P210:
        return MSDK_STRING("P210");
    case MFX_FOURCC_NV16:
        return MSDK_STRING("NV16");
    case MFX_FOURCC_A2RGB10:
        return MSDK_STRING("A2RGB10");
    case MFX_FOURCC_UYVY:
        return MSDK_STRING("UYVY");
    default:
        return MSDK_STRING("Unknown");
    }
}

const msdk_char* IOpattern2Str( mfxU32 IOpattern)
{
    switch ( IOpattern )
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        return MSDK_STRING("sys_to_sys");
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        return MSDK_STRING("sys_to_d3d");
    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY:
        return MSDK_STRING("d3d_to_sys");
    case MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_OUT_VIDEO_MEMORY:
        return MSDK_STRING("d3d_to_d3d");
    default:
        return MSDK_STRING("Not defined");
    }
}

static const
msdk_char* sptr2Str( mfxU32 sptr)
{
    switch ( sptr)
    {
    case NO_PTR:
        return MSDK_STRING("MemID is used for in and out");
    case INPUT_PTR:
        return MSDK_STRING("Ptr is used for in, MemID for out");
    case OUTPUT_PTR:
        return MSDK_STRING("Ptr is used for out, MemID for in");
    case ALL_PTR:
        return MSDK_STRING("Ptr is used for in and out");
    default:
        return MSDK_STRING("Not defined");
    }
}

/* ******************************************************************* */

//static
const msdk_char* PicStruct2Str( mfxU16  PicStruct )
{
    switch (PicStruct)
    {
        case MFX_PICSTRUCT_PROGRESSIVE:
            return MSDK_STRING("progressive");
        case MFX_PICSTRUCT_FIELD_TFF:
            return MSDK_STRING("interlace (TFF)");
        case MFX_PICSTRUCT_FIELD_BFF:
            return MSDK_STRING("interlace (BFF)");
        case MFX_PICSTRUCT_UNKNOWN:
            return MSDK_STRING("unknown");
        default:
            return MSDK_STRING("interlace (no detail)");
    }
}

/* ******************************************************************* */

void PrintInfo(sInputParams* pParams, mfxVideoParam* pMfxParams, MFXVideoSession *pMfxSession)
{
    mfxFrameInfo Info;

    MSDK_CHECK_POINTER_NO_RET(pParams);
    MSDK_CHECK_POINTER_NO_RET(pMfxParams);

    Info = pMfxParams->vpp.In;
    msdk_printf(MSDK_STRING("Input format\t%s\n"), FourCC2Str( Info.FourCC ));
    msdk_printf(MSDK_STRING("Resolution\t%dx%d\n"), Info.Width, Info.Height);
    msdk_printf(MSDK_STRING("Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);
    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), (mfxF64)Info.FrameRateExtN / Info.FrameRateExtD);
    msdk_printf(MSDK_STRING("PicStruct\t%s\n"), PicStruct2Str(Info.PicStruct));

    Info = pMfxParams->vpp.Out;
    msdk_printf(MSDK_STRING("Output format\t%s\n"), FourCC2Str( Info.FourCC ));
    msdk_printf(MSDK_STRING("Resolution\t%dx%d\n"), Info.Width, Info.Height);
    msdk_printf(MSDK_STRING("Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);
    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), (mfxF64)Info.FrameRateExtN / Info.FrameRateExtD);
    msdk_printf(MSDK_STRING("PicStruct\t%s\n"), PicStruct2Str(Info.PicStruct));

    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Video Enhancement Algorithms\n"));
    msdk_printf(MSDK_STRING("Deinterlace\t%s\n"), (pParams->frameInfoIn[0].PicStruct != pParams->frameInfoOut[0].PicStruct) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    msdk_printf(MSDK_STRING("Signal info\t%s\n"),   (VPP_FILTER_DISABLED != pParams->videoSignalInfoParam[0].mode) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    msdk_printf(MSDK_STRING("Scaling\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->bScaling) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    msdk_printf(MSDK_STRING("Denoise\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->denoiseParam[0].mode) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));

    msdk_printf(MSDK_STRING("ProcAmp\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->procampParam[0].mode) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    msdk_printf(MSDK_STRING("DetailEnh\t%s\n"),      (VPP_FILTER_DISABLED != pParams->detailParam[0].mode)  ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    if(VPP_FILTER_DISABLED != pParams->frcParam[0].mode)
    {
        if(MFX_FRCALGM_FRAME_INTERPOLATION == pParams->frcParam[0].algorithm)
        {
            msdk_printf(MSDK_STRING("FRC:Interp\tON\n"));
        }
        else if(MFX_FRCALGM_DISTRIBUTED_TIMESTAMP == pParams->frcParam[0].algorithm)
        {
            msdk_printf(MSDK_STRING("FRC:AdvancedPTS\tON\n"));
        }
        else
        {
            msdk_printf(MSDK_STRING("FRC:\t\tON\n"));
        }
    }
    //msdk_printf(MSDK_STRING("FRC:Advanced\t%s\n"),   (VPP_FILTER_DISABLED != pParams->frcParam.mode)  ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    // MSDK 3.0
    msdk_printf(MSDK_STRING("GamutMapping \t%s\n"),       (VPP_FILTER_DISABLED != pParams->gamutParam[0].mode)      ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    msdk_printf(MSDK_STRING("ColorSaturation\t%s\n"),     (VPP_FILTER_DISABLED != pParams->tccParam[0].mode)   ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    msdk_printf(MSDK_STRING("ContrastEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->aceParam[0].mode)   ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    msdk_printf(MSDK_STRING("SkinToneEnh  \t%s\n"),       (VPP_FILTER_DISABLED != pParams->steParam[0].mode)       ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    msdk_printf(MSDK_STRING("MVC mode    \t%s\n"),       (VPP_FILTER_DISABLED != pParams->multiViewParam[0].mode)  ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    // MSDK 6.0
    msdk_printf(MSDK_STRING("ImgStab    \t%s\n"),            (VPP_FILTER_DISABLED != pParams->istabParam[0].mode)  ? MSDK_STRING("ON"): MSDK_STRING("OFF") );
    msdk_printf(MSDK_STRING("\n"));

    msdk_printf(MSDK_STRING("IOpattern type               \t%s\n"), IOpattern2Str( pParams->IOPattern ));
    msdk_printf(MSDK_STRING("Pointers and MemID settings  \t%s\n"), sptr2Str( pParams->sptr ));
    msdk_printf(MSDK_STRING("Default allocator            \t%s\n"), pParams->bDefAlloc ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
    msdk_printf(MSDK_STRING("Number of asynchronious tasks\t%d\n"), pParams->asyncNum);
    if ( pParams->bInitEx )
    {
    msdk_printf(MSDK_STRING("GPU Copy mode                \t%d\n"), pParams->GPUCopyValue);
    }
    msdk_printf(MSDK_STRING("Time stamps checking         \t%s\n"), pParams->ptsCheck ? MSDK_STRING("ON"): MSDK_STRING("OFF"));

    // info abour ROI testing
    if( ROI_FIX_TO_FIX == pParams->roiCheckParam.mode )
    {
        msdk_printf(MSDK_STRING("ROI checking                 \tOFF\n"));
    }
    else
    {
        msdk_printf(MSDK_STRING("ROI checking                 \tON (seed1 = %i, seed2 = %i)\n"),pParams->roiCheckParam.srcSeed, pParams->roiCheckParam.dstSeed );
    }

    msdk_printf(MSDK_STRING("\n"));

    //-------------------------------------------------------
    mfxIMPL impl;
    pMfxSession->QueryIMPL(&impl);
    bool isHWlib = (MFX_IMPL_HARDWARE & impl) ? true:false;

    const msdk_char* sImpl = (isHWlib) ? MSDK_STRING("hw") : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("MediaSDK impl\t%s"), sImpl);

#ifndef LIBVA_SUPPORT
    if (isHWlib || (pParams->vaType & (ALLOC_IMPL_VIA_D3D9 | ALLOC_IMPL_VIA_D3D11)))
    {
        bool  isD3D11 = (( ALLOC_IMPL_VIA_D3D11 == pParams->vaType) || (pParams->ImpLib == (MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11))) ?  true : false;
        const msdk_char* sIface = ( isD3D11 ) ? MSDK_STRING("VIA_D3D11") : MSDK_STRING("VIA_D3D9");
        msdk_printf(MSDK_STRING(" | %s"), sIface);
    }
#endif
    msdk_printf(MSDK_STRING("\n"));
    //-------------------------------------------------------

    if (isHWlib && !pParams->bPartialAccel)
        msdk_printf(MSDK_STRING("HW accelaration is enabled\n"));
    else
        msdk_printf(MSDK_STRING("HW accelaration is disabled\n"));

    mfxVersion ver;
    pMfxSession->QueryVersion(&ver);
    msdk_printf(MSDK_STRING("MediaSDK ver\t%d.%d\n"), ver.Major, ver.Minor);

    return;
}

/* ******************************************************************* */

mfxStatus ParseGUID(msdk_char strPlgGuid[MSDK_MAX_FILENAME_LEN], mfxU8 DataGUID[16])
{
    const msdk_char *uid = strPlgGuid;
    mfxU32 i   = 0;
    mfxU32 hex = 0;
    for(i = 0; i != 16; i++)
    {
        hex = 0;
#if defined(_WIN32) || defined(_WIN64)
        if (1 != _stscanf_s(uid + 2*i, L"%2x", &hex))
#else
        if (1 != sscanf(uid + 2*i, "%2x", &hex))
#endif
        {
            msdk_printf(MSDK_STRING("Failed to parse plugin uid: %s"), uid);
            return MFX_ERR_UNKNOWN;
        }
#if defined(_WIN32) || defined(_WIN64)
        if (hex == 0 && (uid + 2*i != _tcsstr(uid + 2*i, L"00")))
#else
        if (hex == 0 && (uid + 2*i != strstr(uid + 2*i, "00")))
#endif
        {
            msdk_printf(MSDK_STRING("Failed to parse plugin uid: %s"), uid);
            return MFX_ERR_UNKNOWN;
        }
        DataGUID[i] = (mfxU8)hex;
    }

    return MFX_ERR_NONE;
}
mfxStatus InitParamsVPP(mfxVideoParam* pParams, sInputParams* pInParams, mfxU32 paramID)
{
    MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

    if (pInParams->frameInfoIn[paramID].nWidth == 0 || pInParams->frameInfoIn[paramID].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }
    if (pInParams->frameInfoOut[paramID].nWidth == 0 || pInParams->frameInfoOut[paramID].nHeight == 0 ){
        return MFX_ERR_UNSUPPORTED;
    }

    memset(pParams, 0, sizeof(mfxVideoParam));

    /* input data */
    pParams->vpp.In.Shift           = pInParams->frameInfoIn[paramID].Shift;
    pParams->vpp.In.FourCC          = pInParams->frameInfoIn[paramID].FourCC;
    pParams->vpp.In.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;

    pParams->vpp.In.CropX = pInParams->frameInfoIn[paramID].CropX;
    pParams->vpp.In.CropY = pInParams->frameInfoIn[paramID].CropY;
    pParams->vpp.In.CropW = pInParams->frameInfoIn[paramID].CropW;
    pParams->vpp.In.CropH = pInParams->frameInfoIn[paramID].CropH;

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and
    // a multiple of 32 in case of field picture
    pParams->vpp.In.Width = MSDK_ALIGN16(pInParams->frameInfoIn[paramID].nWidth);
    pParams->vpp.In.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfoIn[paramID].PicStruct)?
        MSDK_ALIGN16(pInParams->frameInfoIn[paramID].nHeight) : MSDK_ALIGN32(pInParams->frameInfoIn[paramID].nHeight);

    pParams->vpp.In.PicStruct = pInParams->frameInfoIn[paramID].PicStruct;

    ConvertFrameRate(pInParams->frameInfoIn[paramID].dFrameRate,
        &pParams->vpp.In.FrameRateExtN,
        &pParams->vpp.In.FrameRateExtD);

    /* output data */
    pParams->vpp.Out.Shift           = pInParams->frameInfoOut[paramID].Shift;
    pParams->vpp.Out.FourCC          = pInParams->frameInfoOut[paramID].FourCC;
    pParams->vpp.Out.ChromaFormat    = MFX_CHROMAFORMAT_YUV420;

    pParams->vpp.Out.CropX = pInParams->frameInfoOut[paramID].CropX;
    pParams->vpp.Out.CropY = pInParams->frameInfoOut[paramID].CropY;
    pParams->vpp.Out.CropW = pInParams->frameInfoOut[paramID].CropW;
    pParams->vpp.Out.CropH = pInParams->frameInfoOut[paramID].CropH;

    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and
    // a multiple of 32 in case of field picture
    pParams->vpp.Out.Width = MSDK_ALIGN16(pInParams->frameInfoOut[paramID].nWidth);
    pParams->vpp.Out.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->frameInfoOut[paramID].PicStruct)?
        MSDK_ALIGN16(pInParams->frameInfoOut[paramID].nHeight) : MSDK_ALIGN32(pInParams->frameInfoOut[paramID].nHeight);
    if(pInParams->need_plugin)
    {
        mfxPluginUID mfxGuid;
        ParseGUID(pInParams->strPlgGuid, mfxGuid.Data);
        if(!memcmp(&mfxGuid,&MFX_PLUGINID_ITELECINE_HW,sizeof(mfxPluginUID)))
        {
            //CM PTIR require equal input and output frame sizes
            pParams->vpp.Out.Height = pParams->vpp.In.Height;
        }
    }

    pParams->vpp.Out.PicStruct = pInParams->frameInfoOut[paramID].PicStruct;

    ConvertFrameRate(pInParams->frameInfoOut[paramID].dFrameRate,
        &pParams->vpp.Out.FrameRateExtN,
        &pParams->vpp.Out.FrameRateExtD);

    pParams->IOPattern = pInParams->IOPattern;

    // async depth
    pParams->AsyncDepth = pInParams->asyncNum;

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

mfxStatus CreateFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus  sts = MFX_ERR_NONE;
    mfxVersion version = {MFX_VERSION_MINOR, MFX_VERSION_MAJOR};
    mfxIMPL    impl    = pInParams->ImpLib;

    MSDK_CHECK_POINTER(pProcessor, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

    WipeFrameProcessor(pProcessor);

    //MFX session
    if ( ! pInParams->bInitEx )
        sts = pProcessor->mfxSession.Init(impl, &version);
    else
    {
        mfxInitParam initParams;
        initParams.ExternalThreads = 0;
        initParams.GPUCopy         = pInParams->GPUCopyValue;
        initParams.Implementation  = impl;
        initParams.Version         = version;
        initParams.NumExtParam     = 0;
        sts = pProcessor->mfxSession.InitEx(initParams);
    }
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to Init mfx session\n"));  WipeFrameProcessor(pProcessor);});

    // Plug-in
    if ( pInParams->need_plugin )
    {
        pProcessor->plugin = true;
        ParseGUID(pInParams->strPlgGuid, pProcessor->mfxGuid.Data);
    }

    if ( pProcessor->plugin )
    {
        sts = MFXVideoUSER_Load(pProcessor->mfxSession, &(pProcessor->mfxGuid), 1);
        if (MFX_ERR_NONE != sts)
        {
            msdk_printf(MSDK_STRING("Failed to load plugin\n"));
            return sts;
        }
    }

    // VPP
    pProcessor->pmfxVPP = new MFXVideoVPP(pProcessor->mfxSession);

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

#ifdef D3D_SURFACES_SUPPORT

#ifdef MFX_D3D11_SUPPORT

mfxStatus CreateD3D11Device(ID3D11Device** ppD3D11Device, ID3D11DeviceContext** ppD3D11DeviceContext)
{

    HRESULT hRes = S_OK;

    static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL pFeatureLevelsOut;

    hRes = D3D11CreateDevice(NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        FeatureLevels,
        sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
        D3D11_SDK_VERSION,
        ppD3D11Device,
        &pFeatureLevelsOut,
        ppD3D11DeviceContext);

    if (FAILED(hRes))
    {
        return MFX_ERR_DEVICE_FAILED;
    }


    CComQIPtr<ID3D10Multithread> p_mt(*ppD3D11DeviceContext);

    if (p_mt)
        p_mt->SetMultithreadProtected(true);
    else
        return MFX_ERR_DEVICE_FAILED;


    return MFX_ERR_NONE;
}

#endif

mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager)
{
    MSDK_CHECK_POINTER(ppManager, MFX_ERR_NULL_PTR);

    //CComPtr<IDirect3D9> d3d = Direct3DCreate9(D3D_SDK_VERSION);
    CComPtr<IDirect3D9> d3d;
    d3d.Attach(Direct3DCreate9(D3D_SDK_VERSION));

    if (!d3d)
    {
        return MFX_ERR_NULL_PTR;
    }

    POINT point = {0, 0};
    HWND window = WindowFromPoint(point);

    D3DPRESENT_PARAMETERS d3dParams;
    memset(&d3dParams, 0, sizeof(d3dParams));
    d3dParams.Windowed = TRUE;
    d3dParams.hDeviceWindow = window;
    d3dParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    d3dParams.Flags = D3DPRESENTFLAG_VIDEO;
    d3dParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dParams.BackBufferCount = 1;
    d3dParams.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dParams.BackBufferWidth = 0;
    d3dParams.BackBufferHeight = 0;

    CComPtr<IDirect3DDevice9> d3dDevice = 0;
    HRESULT hr = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
        &d3dParams,
        &d3dDevice);
    if (FAILED(hr) || !d3dDevice)
        return MFX_ERR_NULL_PTR;

    UINT resetToken = 0;
    CComPtr<IDirect3DDeviceManager9> d3dDeviceManager = 0;
    hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &d3dDeviceManager);
    if (FAILED(hr) || !d3dDeviceManager)
        return MFX_ERR_NULL_PTR;

    hr = d3dDeviceManager->ResetDevice(d3dDevice, resetToken);
    if (FAILED(hr))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    *ppManager = d3dDeviceManager.Detach();

    if (NULL == *ppManager)
    {
        return MFX_ERR_NULL_PTR;
    }

    return MFX_ERR_NONE;
}
#endif

mfxStatus InitFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(pProcessor,          MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pParams,             MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pProcessor->pmfxVPP, MFX_ERR_NULL_PTR);

    // close VPP in case it was initialized
    sts = pProcessor->pmfxVPP->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts,   MFX_ERR_NONE, sts);


    // init VPP
    sts = pProcessor->pmfxVPP->Init(pParams);
    return sts;
}

/* ******************************************************************* */

mfxStatus InitSurfaces(
    sMemoryAllocator* pAllocator,
    mfxFrameAllocRequest* pRequest,
    mfxFrameInfo* pInfo,
    mfxU32 indx,
    bool isPtr)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16    nFrames, i;

    sts = pAllocator->pMfxAllocator->Alloc(pAllocator->pMfxAllocator->pthis, pRequest, &(pAllocator->response[indx]));
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to Alloc frames\n"));  WipeMemoryAllocator(pAllocator);});

    nFrames = pAllocator->response[indx].NumFrameActual;
    pAllocator->pSurfaces[indx] = new mfxFrameSurface1 [nFrames];

    for (i = 0; i < nFrames; i++)
    {
        memset(&(pAllocator->pSurfaces[indx][i]), 0, sizeof(mfxFrameSurface1));
        pAllocator->pSurfaces[indx][i].Info = *pInfo;

        if( !isPtr )
        {
            pAllocator->pSurfaces[indx][i].Data.MemId = pAllocator->response[indx].mids[i];
        }
        else
        {
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis,
                pAllocator->response[indx].mids[i],
                &(pAllocator->pSurfaces[indx][i].Data));
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to lock frames\n"));  WipeMemoryAllocator(pAllocator);});
        }
    }

    return sts;
}

/* ******************************************************************* */

mfxStatus InitSvcSurfaces(
    sMemoryAllocator* pAllocator,
    mfxFrameAllocRequest* pRequest,
    mfxFrameInfo* pInfo,
    mfxU32 /*indx*/,
    bool isPtr,
    sSVCLayerDescr* pSvcDesc
    )
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16    nFrames = pAllocator->response->NumFrameActual, i, did;
    mfxFrameAllocRequest layerRequest[8];

    for( did = 0; did < 8; did++ )
    {
        if( pSvcDesc[did].active )
        {
            layerRequest[did] = *pRequest;

            layerRequest[did].Info.CropX  = pSvcDesc[did].cropX;
            layerRequest[did].Info.CropY  = pSvcDesc[did].cropY;
            layerRequest[did].Info.CropW  = pSvcDesc[did].cropW;
            layerRequest[did].Info.CropH  = pSvcDesc[did].cropH;

            layerRequest[did].Info.Height = pSvcDesc[did].height;
            layerRequest[did].Info.Width  = pSvcDesc[did].width;

            layerRequest[did].Info.PicStruct = pInfo->PicStruct;

            layerRequest[did].Info.FrameId.DependencyId = did;

            sts = pAllocator->pMfxAllocator->Alloc(
                pAllocator->pMfxAllocator->pthis,
                &(layerRequest[did]),
                &(pAllocator->svcResponse[did]));
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to alloc frames\n"));  WipeMemoryAllocator(pAllocator);});

            pAllocator->pSvcSurfaces[did] = new mfxFrameSurface1 [nFrames];
        }
    }


    for( did = 0; did < 8; did++ )
    {
        if( 0 == pSvcDesc[did].active ) continue;

        for (i = 0; i < nFrames; i++)
        {
            memset(&(pAllocator->pSvcSurfaces[did][i]), 0, sizeof(mfxFrameSurface1));

            pAllocator->pSvcSurfaces[did][i].Info = layerRequest[did].Info;

            if( !isPtr )
            {
                pAllocator->pSvcSurfaces[did][i].Data.MemId = pAllocator->svcResponse[did].mids[i];
            }
            else
            {
                sts = pAllocator->pMfxAllocator->Lock(
                    pAllocator->pMfxAllocator->pthis,
                    pAllocator->svcResponse[did].mids[i],
                    &(pAllocator->pSvcSurfaces[did][i].Data));
                MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to lock frames\n"));  WipeMemoryAllocator(pAllocator);});
            }
        }
    }

    return sts;
}

/* ******************************************************************* */

mfxStatus InitMemoryAllocator(
    sFrameProcessor* pProcessor,
    sMemoryAllocator* pAllocator,
    mfxVideoParam* pParams,
    sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request[2];// [0] - in, [1] - out

    MSDK_CHECK_POINTER(pProcessor,          MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pAllocator,          MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pParams,             MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pProcessor->pmfxVPP, MFX_ERR_NULL_PTR);

    MSDK_ZERO_MEMORY(request[VPP_IN]);
    MSDK_ZERO_MEMORY(request[VPP_OUT]);

    // VppRequest[0] for input frames request, VppRequest[1] for output frames request
    //sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    //aya; async take into consediration by VPP
    /*  request[0].NumFrameMin = request[0].NumFrameMin + pInParams->asyncNum;
    request[1].NumFrameMin = request[1].NumFrameMin  + pInParams->asyncNum;

    request[0].NumFrameSuggested = request[0].NumFrameSuggested + pInParams->asyncNum;
    request[1].NumFrameSuggested = request[1].NumFrameSuggested + pInParams->asyncNum;*/

    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    bool isInPtr = (pInParams->sptr & INPUT_PTR)?true:false;
    bool isOutPtr = (pInParams->sptr & OUTPUT_PTR)?true:false;

    pAllocator->pMfxAllocator =  new GeneralAllocator;

    bool isHWLib       = (MFX_IMPL_HARDWARE & pInParams->ImpLib) ? true : false;
    bool isExtVideoMem = (pInParams->IOPattern != (MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ? true : false;

    bool isNeedExtAllocator = (isHWLib || isExtVideoMem);

    if( isNeedExtAllocator )
    {
#ifdef D3D_SURFACES_SUPPORT
        if( ((MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D9) == pInParams->ImpLib) || (ALLOC_IMPL_VIA_D3D9 == pInParams->vaType) )
        {
            D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;
            // prepare device manager
            sts = CreateDeviceManager(&(pAllocator->pd3dDeviceManager));
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to CreateDeviceManager\n")); WipeMemoryAllocator(pAllocator);});

            sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_DIRECT3D_DEVICE_MANAGER9, pAllocator->pd3dDeviceManager);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetHandle\n"));  WipeMemoryAllocator(pAllocator);});

            // prepare allocator
            pd3dAllocParams->pManager = pAllocator->pd3dDeviceManager;
            pAllocator->pAllocatorParams = pd3dAllocParams;

            /* In case of video memory we must provide mediasdk with external allocator
            thus we demonstrate "external allocator" usage model.
            Call SetAllocator to pass allocator to mediasdk */
            sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
        }
        else if ( ((MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11) == pInParams->ImpLib) || ((ALLOC_IMPL_VIA_D3D11 == pInParams->vaType)))
        {
#ifdef MFX_D3D11_SUPPORT

            D3D11AllocatorParams *pd3d11AllocParams = new D3D11AllocatorParams;

            // prepare device manager
            sts = CreateD3D11Device(&(pAllocator->pD3D11Device), &(pAllocator->pD3D11DeviceContext));
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to CreateD3D11Device\n"));  WipeMemoryAllocator(pAllocator);});

            sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_D3D11_DEVICE, pAllocator->pD3D11Device);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetHandle\n"));  WipeMemoryAllocator(pAllocator);});

            // prepare allocator
            pd3d11AllocParams->pDevice = pAllocator->pD3D11Device;
            pAllocator->pAllocatorParams = pd3d11AllocParams;

            sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
            MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});

            ((GeneralAllocator *)(pAllocator->pMfxAllocator))->SetDX11();

#endif
        }
#endif
#ifdef LIBVA_SUPPORT
        vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;

        p_vaapiAllocParams->m_dpy = pAllocator->libvaKeeper->GetVADisplay();
        pAllocator->pAllocatorParams = p_vaapiAllocParams;

        sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)pAllocator->libvaKeeper->GetVADisplay());
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        /* In case of video memory we must provide mediasdk with external allocator
        thus we demonstrate "external allocator" usage model.
        Call SetAllocator to pass allocator to mediasdk */
        sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
#endif
    }
    else if (pAllocator->bUsedAsExternalAllocator)
    {
        sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to SetFrameAllocator\n"));  WipeMemoryAllocator(pAllocator);});
    }

    //((GeneralAllocator *)(pAllocator->pMfxAllocator))->setDxVersion(pInParams->ImpLib);

    sts = pAllocator->pMfxAllocator->Init(pAllocator->pAllocatorParams);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to pMfxAllocator->Init\n"));  WipeMemoryAllocator(pAllocator);});

    // see the spec
    // (1) MFXInit()
    // (2) MFXQueryIMPL()
    // (3) MFXVideoCORE_SetHandle()
    // after (1-3), call of any MSDK function is OK
    sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed in QueryIOSurf\n"));  WipeMemoryAllocator(pAllocator);});

    // alloc frames for vpp
    // [IN]
    sts = InitSurfaces(pAllocator, &(request[VPP_IN]), &(pParams->vpp.In), VPP_IN, isInPtr);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    // [OUT]
    sts = InitSurfaces(
        pAllocator,
        &(request[VPP_OUT]),
        &(pParams->vpp.Out),
        VPP_OUT,
        isOutPtr);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to InitSurfaces\n"));  WipeMemoryAllocator(pAllocator);});

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

mfxStatus InitResources(sAppResources* pResources, mfxVideoParam* pParams, sInputParams* pInParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
    sts = CreateFrameProcessor(pResources->pProcessor, pParams, pInParams);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to CreateFrameProcessor\n")); WipeResources(pResources); WipeParams(pInParams);});

    sts = InitMemoryAllocator(pResources->pProcessor, pResources->pAllocator, pParams, pInParams);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to InitMemoryAllocator\n")); WipeResources(pResources); WipeParams(pInParams);});

    sts = InitFrameProcessor(pResources->pProcessor, pParams);

    if (MFX_WRN_PARTIAL_ACCELERATION == sts || MFX_WRN_FILTER_SKIPPED == sts)
        return sts;
    else
    {
        MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, { msdk_printf(MSDK_STRING("Failed to InitFrameProcessor\n")); WipeResources(pResources); WipeParams(pInParams);});
    }

    return sts;
}

/* ******************************************************************* */

void WipeFrameProcessor(sFrameProcessor* pProcessor)
{
    MSDK_CHECK_POINTER_NO_RET(pProcessor);

    MSDK_SAFE_DELETE(pProcessor->pmfxVPP);

    if ( pProcessor->plugin )
    {
        MFXVideoUSER_UnLoad(pProcessor->mfxSession, &(pProcessor->mfxGuid));
    }

    pProcessor->mfxSession.Close();
}

void WipeMemoryAllocator(sMemoryAllocator* pAllocator)
{
    MSDK_CHECK_POINTER_NO_RET(pAllocator);

    MSDK_SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_IN]);
    MSDK_SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_OUT]);

    mfxU32 did;
    for(did = 0; did < 8; did++)
    {
        MSDK_SAFE_DELETE_ARRAY(pAllocator->pSvcSurfaces[did]);
    }

    // delete frames
    if (pAllocator->pMfxAllocator)
    {
        pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_IN]);
        pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_OUT]);

        for(did = 0; did < 8; did++)
        {
            pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->svcResponse[did]);
        }
    }

    // delete allocator
    MSDK_SAFE_DELETE(pAllocator->pMfxAllocator);

#ifdef D3D_SURFACES_SUPPORT
    // release device manager
    if (pAllocator->pd3dDeviceManager)
    {
        pAllocator->pd3dDeviceManager->Release();
        pAllocator->pd3dDeviceManager = NULL;
    }
#endif

    // delete allocator parameters
    MSDK_SAFE_DELETE(pAllocator->pAllocatorParams);

} // void WipeMemoryAllocator(sMemoryAllocator* pAllocator)


void WipeConfigParam( sAppResources* pResources )
{

    if( pResources->multiViewConfig.View )
    {
        delete [] pResources->multiViewConfig.View;
    }

} // void WipeConfigParam( sAppResources* pResources )


void WipeResources(sAppResources* pResources)
{
    MSDK_CHECK_POINTER_NO_RET(pResources);

    WipeFrameProcessor(pResources->pProcessor);

    WipeMemoryAllocator(pResources->pAllocator);

    if (pResources->pSrcFileReader)
    {
        pResources->pSrcFileReader->Close();
    }

    if (pResources->pDstFileWriters)
    {
        for (mfxU32 i = 0; i < pResources->dstFileWritersN; i++)
        {
            pResources->pDstFileWriters[i].Close();
        }
        delete[] pResources->pDstFileWriters;
        pResources->dstFileWritersN = 0;
        pResources->pDstFileWriters = 0;
    }

    WipeConfigParam( pResources );

} // void WipeResources(sAppResources* pResources)

/* ******************************************************************* */

void WipeParams(sInputParams* pParams)
{
    pParams->strDstFiles.clear();

} // void WipeParams(sInputParams* pParams)

/* ******************************************************************* */

CRawVideoReader::CRawVideoReader()
{
    m_fSrc = 0;
    m_isPerfMode = false;
    m_Repeat = 0;
    m_pPTSMaker = 0;
}

mfxStatus CRawVideoReader::Init(const msdk_char *strFileName, PTSMaker *pPTSMaker)
{
    Close();

    MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    MSDK_FOPEN(m_fSrc,strFileName, MSDK_STRING("rb"));
    MSDK_CHECK_POINTER(m_fSrc, MFX_ERR_ABORTED);

    m_pPTSMaker = pPTSMaker;

    return MFX_ERR_NONE;
}

CRawVideoReader::~CRawVideoReader()
{
    Close();
}

void CRawVideoReader::Close()
{
    if (m_fSrc != 0)
    {
        fclose(m_fSrc);
        m_fSrc = 0;
    }
    m_SurfacesList.clear();

}

mfxStatus CRawVideoReader::LoadNextFrame(mfxFrameData* pData, mfxFrameInfo* pInfo)
{
    MSDK_CHECK_POINTER(pData, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pInfo, MFX_ERR_NOT_INITIALIZED);

    mfxU32 w, h, i, pitch;
    mfxU32 nBytesRead;
    mfxU8 *ptr;

    if (pInfo->CropH > 0 && pInfo->CropW > 0)
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    }
    else
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = pData->Pitch;

    if(pInfo->FourCC == MFX_FOURCC_YV12)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;
        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else   if(pInfo->FourCC == MFX_FOURCC_YUV400)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        int i = 0;
        i;
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV411)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w /= 4;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422H)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        w     >>= 1;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422V)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        h     >>= 1;

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV444)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load V
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV12 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load UV
        h     >>= 1;
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV16 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        // load UV
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P010 )
    {
        ptr = pData->Y + pInfo->CropX * 2 + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w * 2, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }

        // load UV
        h     >>= 1;
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w*2, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P210 )
    {
        ptr = pData->Y + pInfo->CropX * 2 + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w * 2, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }

        // load UV
        ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
        for (i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w*2, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w*2, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_RGB3)
    {
        MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);

        ptr = std::min(std::min(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 3*w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, 3*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_RGB4)
    {
        MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);
        // there is issue with A channel in case of d3d, so A-ch is ignored
        //MSDK_CHECK_POINTER(pData->A, MFX_ERR_NOT_INITIALIZED);

        ptr = std::min(std::min(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 4*w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_YUY2)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 2*w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, 2*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_UYVY)
    {
        ptr = pData->U + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 2*w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, 2*w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_IMC3)
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        // read luminance plane
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }

        h     >>= 1;

        // load V
        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
        // load U
        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
            IOSTREAM_MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}


mfxStatus CRawVideoReader::GetNextInputFrame(sMemoryAllocator* pAllocator, mfxFrameInfo* pInfo, mfxFrameSurface1** pSurface)
{
    mfxStatus sts;
    if (!m_isPerfMode)
    {
        GetFreeSurface(pAllocator->pSurfaces[VPP_IN], pAllocator->response[VPP_IN].NumFrameActual, pSurface);
        mfxFrameSurface1* pCurSurf = *pSurface;
        if (pCurSurf->Data.MemId)
        {
            // get YUV pointers
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pCurSurf->Data.MemId, &pCurSurf->Data);
            MFX_CHECK_STS(sts);
            sts = LoadNextFrame(&pCurSurf->Data, pInfo);
            MFX_CHECK_STS(sts);
            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pCurSurf->Data.MemId, &pCurSurf->Data);
            MFX_CHECK_STS(sts);
        }
        else
        {
            sts = LoadNextFrame( &pCurSurf->Data, pInfo);
            MFX_CHECK_STS(sts);
        }
    }
    else
    {
        sts = GetPreAllocFrame(pSurface);
        MFX_CHECK_STS(sts);
    }

    if (m_pPTSMaker)
    {
        if (!m_pPTSMaker->SetPTS(*pSurface))
            return MFX_ERR_UNKNOWN;
    }

    return MFX_ERR_NONE;
}


mfxStatus  CRawVideoReader::GetPreAllocFrame(mfxFrameSurface1 **pSurface)
{
    if (m_it == m_SurfacesList.end())
    {
        m_Repeat--;
        m_it = m_SurfacesList.begin();
    }

    if (m_it->Data.Locked)
        return MFX_ERR_ABORTED;

    *pSurface = &(*m_it);
    m_it++;
    if (0 == m_Repeat)
        return MFX_ERR_MORE_DATA;

    return MFX_ERR_NONE;

}


mfxStatus  CRawVideoReader::PreAllocateFrameChunk(mfxVideoParam* pVideoParam,
                                                  sInputParams* pParams,
                                                  MFXFrameAllocator* pAllocator)
{
    mfxStatus sts;
    mfxFrameAllocRequest  request;
    mfxFrameAllocResponse response;
    mfxFrameSurface1      surface;
    m_isPerfMode = true;
    m_Repeat = pParams->numRepeat;
    request.Info = pVideoParam->vpp.In;
    request.Type = (pParams->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)?(MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET):
        (MFX_MEMTYPE_FROM_VPPIN|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY);
    request.NumFrameSuggested = request.NumFrameMin = (mfxU16)pParams->numFrames;
    sts = pAllocator->Alloc(pAllocator, &request, &response);
    MFX_CHECK_STS(sts);
    for(;m_SurfacesList.size() < pParams->numFrames;)
    {
        surface.Data.Locked = 0;
        surface.Data.MemId = response.mids[m_SurfacesList.size()];
        surface.Info = pVideoParam->vpp.In;
        sts = pAllocator->Lock(pAllocator->pthis, surface.Data.MemId, &surface.Data);
        MFX_CHECK_STS(sts);
        sts = LoadNextFrame(&surface.Data, &pVideoParam->vpp.In);
        MFX_CHECK_STS(sts);
        sts = pAllocator->Unlock(pAllocator->pthis, surface.Data.MemId, &surface.Data);
        MFX_CHECK_STS(sts);
        m_SurfacesList.push_back(surface);
    }
    m_it = m_SurfacesList.begin();
    return MFX_ERR_NONE;
}
/* ******************************************************************* */

CRawVideoWriter::CRawVideoWriter()
{
    m_fDst = 0;
    m_pPTSMaker = 0;

    return;
}

mfxStatus CRawVideoWriter::Init(const msdk_char *strFileName, PTSMaker *pPTSMaker, bool outYV12 )
{
    Close();

    m_pPTSMaker = pPTSMaker;
    // no need to generate output
    if (0 == strFileName)
        return MFX_ERR_NONE;

    //CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

    MSDK_FOPEN(m_fDst,strFileName, MSDK_STRING("wb"));
    MSDK_CHECK_POINTER(m_fDst, MFX_ERR_ABORTED);
    m_outYV12  = outYV12;

    return MFX_ERR_NONE;
}

CRawVideoWriter::~CRawVideoWriter()
{
    Close();

    return;
}

void CRawVideoWriter::Close()
{
    if (m_fDst != 0){

        fclose(m_fDst);
        m_fDst = 0;
    }

    return;
}

mfxStatus CRawVideoWriter::PutNextFrame(
                                        sMemoryAllocator* pAllocator,
                                        mfxFrameInfo* pInfo,
                                        mfxFrameSurface1* pSurface)
{
    mfxStatus sts;
    if (m_fDst)
    {
        if (pSurface->Data.MemId)
        {
            // get YUV pointers
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

            sts = WriteFrame( &(pSurface->Data), pInfo);
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);

            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
        else
        {
            sts = WriteFrame( &(pSurface->Data), pInfo);
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
    }
    else // performance mode
    {
        if (pSurface->Data.MemId)
        {
            sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
            sts = pAllocator->pMfxAllocator->Unlock(pAllocator->pMfxAllocator->pthis, pSurface->Data.MemId, &(pSurface->Data));
            MSDK_CHECK_NOT_EQUAL(sts, MFX_ERR_NONE, MFX_ERR_ABORTED);
        }
    }
    if (m_pPTSMaker)
        return m_pPTSMaker->CheckPTS(pSurface)?MFX_ERR_NONE:MFX_ERR_ABORTED;

    return MFX_ERR_NONE;
}
mfxStatus CRawVideoWriter::WriteFrame(
                                      mfxFrameData* pData,
                                      mfxFrameInfo* pInfo)
{
    mfxI32 nBytesRead   = 0;

    mfxI32 i, pitch;
    mfxU16 h, w;
    mfxU8* ptr;

    MSDK_CHECK_POINTER(pData, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pInfo, MFX_ERR_NOT_INITIALIZED);
    //-------------------------------------------------------
    mfxFrameData outData = *pData;

    if (pInfo->CropH > 0 && pInfo->CropW > 0)
    {
        w = pInfo->CropW;
        h = pInfo->CropH;
    }
    else
    {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = outData.Pitch;

    if(pInfo->FourCC == MFX_FOURCC_YV12)
    {

        ptr   = outData.Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL(fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;

        ptr  = outData.V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = outData.U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV400)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        w     >>= 1;
        h     >>= 1;
        pitch >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV411)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        w     /= 4;
        //pitch /= 4;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422H)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        w     >>= 1;
        //pitch >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV422V)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        h     >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if(pInfo->FourCC == MFX_FOURCC_YUV444)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV12 && !m_outYV12)
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // write UV data
        h     >>= 1;
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV12 && m_outYV12 )
    {
        int j=0;
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // write V plane first, then U plane
        h >>= 1;
        w >>= 1;
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch;

        for(i = 0; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                fputc(ptr[i*pitch + j*2 + 1],  m_fDst);
            }
        }
        for(i = 0; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                fputc(ptr[i*pitch + j*2],  m_fDst);
            }
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_NV16 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // write UV data
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P010 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w * 2, m_fDst), w * 2u, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // write UV data
        h     >>= 1;
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch ;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w*2, m_fDst), w*2u, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_P210 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w * 2, m_fDst), w * 2u, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // write UV data
        ptr  = pData->UV + (pInfo->CropX ) + (pInfo->CropY >> 1) * pitch ;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w*2, m_fDst), w*2u, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else if( pInfo->FourCC == MFX_FOURCC_YUY2 )
    {
        ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, 2*w, m_fDst), 2u*w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else if ( pInfo->FourCC == MFX_FOURCC_IMC3 )
    {
        ptr   = pData->Y + (pInfo->CropX ) + (pInfo->CropY ) * pitch;

        for (i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        w     >>= 1;
        h     >>= 1;

        ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, w, m_fDst), w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
        for(i = 0; i < h; i++)
        {
            nBytesRead = (mfxU32)fwrite(ptr + i * pitch, 1, w, m_fDst);
            MSDK_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
        }
    }
    else if (pInfo->FourCC == MFX_FOURCC_RGB4 || pInfo->FourCC == MFX_FOURCC_A2RGB10)
    {
        MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
        MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);
        // there is issue with A channel in case of d3d, so A-ch is ignored
        //MSDK_CHECK_POINTER(pData->A, MFX_ERR_NOT_INITIALIZED);

        ptr = std::min(std::min(pData->R, pData->G), pData->B );
        ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

        for(i = 0; i < h; i++)
        {
            MSDK_CHECK_NOT_EQUAL( fwrite(ptr + i * pitch, 1, 4*w, m_fDst), 4u*w, MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }
    else
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

/* ******************************************************************* */

GeneralWriter::GeneralWriter()
{
};


GeneralWriter::~GeneralWriter()
{
    Close();
};


void GeneralWriter::Close()
{
    for(mfxU32 did = 0; did < 8; did++)
    {
        m_ofile[did].reset();
    }
};


mfxStatus GeneralWriter::Init(
    const msdk_char *strFileName,
    PTSMaker *pPTSMaker,
    sSVCLayerDescr*  pDesc,
    bool outYV12)
{
    mfxStatus sts = MFX_ERR_UNKNOWN;;

    mfxU32 didCount = (pDesc) ? 8 : 1;
    m_svcMode = (pDesc) ? true : false;

    for(mfxU32 did = 0; did < didCount; did++)
    {
        if( (1 == didCount) || (pDesc[did].active) )
        {
            m_ofile[did].reset(new CRawVideoWriter() );
            if(0 == m_ofile[did].get())
            {
                return MFX_ERR_UNKNOWN;
            }

            msdk_char out_buf[MSDK_MAX_FILENAME_LEN*4+20];
            msdk_char fname[MSDK_MAX_FILENAME_LEN];

#if defined(_WIN32) || defined(_WIN64)
            {
                msdk_char drive[MSDK_MAX_FILENAME_LEN];
                msdk_char dir[MSDK_MAX_FILENAME_LEN];
                msdk_char ext[MSDK_MAX_FILENAME_LEN];

                _tsplitpath_s(
                            strFileName,
                            drive,
                            dir,
                            fname,
                            ext);

                msdk_sprintf(out_buf, MSDK_STRING("%s%s%s_layer%i.yuv"), drive, dir, fname, did);
            }
#else
            {
                msdk_strcopy(fname,strFileName);
                char* pFound = strrchr(fname,'.');
                if(pFound)
                {
                    *pFound=0;
                }
                msdk_sprintf(out_buf, MSDK_STRING("%s_layer%i.yuv"), fname, did);
            }
#endif

            sts = m_ofile[did]->Init(
                (1 == didCount) ? strFileName : out_buf,
                pPTSMaker,
                outYV12);

            if(sts != MFX_ERR_NONE) break;
        }
    }

    return sts;
};

mfxStatus  GeneralWriter::PutNextFrame(
        sMemoryAllocator* pAllocator,
        mfxFrameInfo* pInfo,
        mfxFrameSurface1* pSurface)
{
    mfxU32 did = (m_svcMode) ? pSurface->Info.FrameId.DependencyId : 0;//aya: for MVC we have 1 out file only

    mfxStatus sts = m_ofile[did]->PutNextFrame(pAllocator, pInfo, pSurface);

    return sts;
};

/* ******************************************************************* */

mfxStatus UpdateSurfacePool(mfxFrameInfo SurfacesInfo, mfxU16 nPoolSize, mfxFrameSurface1* pSurface)
{
    MSDK_CHECK_POINTER(pSurface,     MFX_ERR_NULL_PTR);
    if (pSurface)
    {
        for (mfxU16 i = 0; i < nPoolSize; i++)
        {
            pSurface[i].Info = SurfacesInfo;
        }
    }
    return MFX_ERR_NONE;
}
mfxStatus GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxFrameSurface1** ppSurface)
{
    MSDK_CHECK_POINTER(pSurfacesPool, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(ppSurface,     MFX_ERR_NULL_PTR);

    mfxU32 timeToSleep = 10; // milliseconds
    mfxU32 numSleeps = MSDK_SURFACE_WAIT_INTERVAL / timeToSleep + 1; // at least 1

    mfxU32 i = 0;

    //wait if there's no free surface
    while ((MSDK_INVALID_SURF_IDX == GetFreeSurfaceIndex(pSurfacesPool, nPoolSize)) && (i < numSleeps))
    {
        MSDK_SLEEP(timeToSleep);
        i++;
    }

    mfxU16 index = GetFreeSurfaceIndex(pSurfacesPool, nPoolSize);

    if (index < nPoolSize)
    {
        *ppSurface = &(pSurfacesPool[index]);
        return MFX_ERR_NONE;
    }

    return MFX_ERR_NOT_ENOUGH_BUFFER;
}

// Wrapper on standard allocator for concurrent allocation of
// D3D and system surfaces
GeneralAllocator::GeneralAllocator()
{
#ifdef MFX_D3D11_SUPPORT
    m_D3D11Allocator.reset(new D3D11FrameAllocator);
#endif
#ifdef D3D_SURFACES_SUPPORT
    m_D3DAllocator.reset(new D3DFrameAllocator);
#endif
#ifdef LIBVA_SUPPORT
    m_vaapiAllocator.reset(new vaapiFrameAllocator);
#endif
    m_SYSAllocator.reset(new SysMemFrameAllocator);

    m_isDx11 = false;

};
GeneralAllocator::~GeneralAllocator()
{
};
mfxStatus GeneralAllocator::Init(mfxAllocatorParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        sts = m_D3D11Allocator.get()->Init(pParams);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        sts = m_D3DAllocator.get()->Init(pParams);
#endif

    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

#ifdef LIBVA_SUPPORT
    sts = m_vaapiAllocator.get()->Init(pParams);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);
#endif

    sts = m_SYSAllocator.get()->Init(0);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    return sts;
}
mfxStatus GeneralAllocator::Close()
{
    mfxStatus sts = MFX_ERR_NONE;

#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        sts = m_D3D11Allocator.get()->Close();
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        sts = m_D3DAllocator.get()->Close();
#endif
#ifdef LIBVA_SUPPORT
    sts = m_vaapiAllocator.get()->Close();
#endif
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    sts = m_SYSAllocator.get()->Close();
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    m_isDx11 = false;

    return sts;
}

mfxStatus GeneralAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid) ? m_D3D11Allocator.get()->Lock(m_D3D11Allocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid) ? m_D3DAllocator.get()->Lock(m_D3DAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->Lock(m_vaapiAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#else
        return m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
#endif
}
mfxStatus GeneralAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid)?m_D3D11Allocator.get()->Unlock(m_D3D11Allocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid)?m_D3DAllocator.get()->Unlock(m_D3DAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->Unlock(m_vaapiAllocator.get(), mid, ptr):
        m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#else
        return m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
#endif
}

mfxStatus GeneralAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(mid)?m_D3D11Allocator.get()->GetHDL(m_D3D11Allocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(mid)?m_D3DAllocator.get()->GetHDL(m_D3DAllocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);

#elif LIBVA_SUPPORT
        return isD3DMid(mid)?m_vaapiAllocator.get()->GetHDL(m_vaapiAllocator.get(), mid, handle):
        m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
#else
        return m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
#endif
}

mfxStatus GeneralAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    // try to ReleaseResponsevia D3D allocator
#ifdef MFX_D3D11_SUPPORT
    if (true == m_isDx11)
        return isD3DMid(response->mids[0])?m_D3D11Allocator.get()->Free(m_D3D11Allocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
    else
#endif
#ifdef D3D_SURFACES_SUPPORT
        return isD3DMid(response->mids[0])?m_D3DAllocator.get()->Free(m_D3DAllocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#elif LIBVA_SUPPORT
        return isD3DMid(response->mids[0])?m_vaapiAllocator.get()->Free(m_vaapiAllocator.get(),response):
        m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#else
        return m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
#endif
}
mfxStatus GeneralAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts;
    if (request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET || request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)
    {
#ifdef MFX_D3D11_SUPPORT
        if (true == m_isDx11)
            sts = m_D3D11Allocator.get()->Alloc(m_D3D11Allocator.get(), request, response);
        else
#endif
#ifdef D3D_SURFACES_SUPPORT
            sts = m_D3DAllocator.get()->Alloc(m_D3DAllocator.get(), request, response);
#endif
#ifdef LIBVA_SUPPORT
        sts = m_vaapiAllocator.get()->Alloc(m_vaapiAllocator.get(), request, response);
#endif
        StoreFrameMids(true, response);
    }
    else
    {
        sts = m_SYSAllocator.get()->Alloc(m_SYSAllocator.get(), request, response);
        StoreFrameMids(false, response);
    }
    return sts;
}
void GeneralAllocator::StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response)
{
    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        m_Mids.insert(std::pair<mfxHDL, bool>(response->mids[i], isD3DFrames));
}
bool GeneralAllocator::isD3DMid(mfxHDL mid)
{
    std::map<mfxHDL, bool>::iterator it;
    it = m_Mids.find(mid);
    return it->second;

}

void GeneralAllocator::SetDX11(void)
{
    m_isDx11 = true;
}

//---------------------------------------------------------

void PrintDllInfo()
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE   hCurrent = GetCurrentProcess();
    HMODULE *pModules;
    DWORD    cbNeeded;
    int      nModules;
    if (NULL == EnumProcessModules(hCurrent, NULL, 0, &cbNeeded))
        return;

    nModules = cbNeeded / sizeof(HMODULE);

    pModules = new HMODULE[nModules];
    if (NULL == pModules)
    {
        return;
    }
    if (NULL == EnumProcessModules(hCurrent, pModules, cbNeeded, &cbNeeded))
    {
        delete []pModules;
        return;
    }

    for (int i = 0; i < nModules; i++)
    {
        msdk_char buf[2048];
        GetModuleFileName(pModules[i], buf, ARRAYSIZE(buf));
        if (_tcsstr(buf, MSDK_STRING("libmfx")))
        {
            msdk_printf(MSDK_STRING("MFX dll         %s\n"),buf);
        }
    }
    delete []pModules;
#endif
} // void PrintDllInfo()

/* ******************************************************************* */

/* EOF */
