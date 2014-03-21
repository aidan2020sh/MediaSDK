/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#include <math.h>

#include "sample_vpp_utils.h"
#include "sysmem_allocator.h"

#ifdef D3D_SURFACES_SUPPORT
#include "d3d_allocator.h"
#endif

#ifdef LIBVA_SUPPORT
#include "vaapi_allocator.h"
#endif

/* ******************************************************************* */

static
void WipeFrameProcessor(sFrameProcessor* pProcessor);

static
void WipeMemoryAllocator(sMemoryAllocator* pAllocator);

/* ******************************************************************* */

static const msdk_char*
FourCC2Str( mfxU32 FourCC )
{
  switch ( FourCC )
  {
  case MFX_FOURCC_NV12:
    return MSDK_STRING("NV12");
    break;

  case MFX_FOURCC_YV12:
    return MSDK_STRING("YV12");
    break;

  case MFX_FOURCC_YUY2:
    return MSDK_STRING("YUY2");
    break;

  case MFX_FOURCC_RGB3:
    return MSDK_STRING("RGB3");
    break;

  case MFX_FOURCC_RGB4:
    return MSDK_STRING("RGB4");
    break;

  default:
    return MSDK_STRING("UNKN");
    break;
  }

  return MSDK_STRING("UNKN");

} // msdk_char* FourCC2Str( mfxU32 FourCC )

/* ******************************************************************* */

static const msdk_char*
PicStruct2Str( mfxU16  PicStruct )
{
  if (PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
  {
    return MSDK_STRING("progressive");
  }
  else
  {
    return MSDK_STRING("interleave");
  }

  return MSDK_STRING("unknown");

} // msdk_char* PicStruct2Str( mfxU16  PicStruct )

/* ******************************************************************* */

void PrintInfo(sInputParams* pParams, mfxVideoParam* pMfxParams, MFXVideoSession *pMfxSession)
{
  mfxFrameInfo Info;

  MSDK_CHECK_POINTER_NO_RET(pParams);
  MSDK_CHECK_POINTER_NO_RET(pMfxParams);

  msdk_printf(MSDK_STRING("Intel(R) Media SDK VPP Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

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
  msdk_printf(MSDK_STRING("Denoise\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->denoiseParam.mode) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
  msdk_printf(MSDK_STRING("VideoAnalysis\t%s\n"), (VPP_FILTER_DISABLED != pParams->vaParam.mode)      ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
  msdk_printf(MSDK_STRING("ProcAmp\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->procampParam.mode) ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
  msdk_printf(MSDK_STRING("Detail\t\t%s\n"),      (VPP_FILTER_DISABLED != pParams->detailParam.mode)  ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
  msdk_printf(MSDK_STRING("ImgStab\t\t%s\n"),     (VPP_FILTER_DISABLED != pParams->istabParam.mode)   ? MSDK_STRING("ON"): MSDK_STRING("OFF"));
  msdk_printf(MSDK_STRING("\n"));

  const msdk_char* sMemType = pParams->bd3dAlloc ? MSDK_STRING("video") : MSDK_STRING("system");
  msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);
  msdk_printf(MSDK_STRING("\n"));

  mfxIMPL impl;
  pMfxSession->QueryIMPL(&impl);

  const msdk_char* sImpl = (MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl)) ? MSDK_STRING("hw") : MSDK_STRING("sw");
  msdk_printf(MSDK_STRING("MediaSDK impl\t%s\n"), sImpl);

  mfxVersion ver;
  pMfxSession->QueryVersion(&ver);
  msdk_printf(MSDK_STRING("MediaSDK ver\t%d.%d\n"), ver.Major, ver.Minor);

  msdk_printf(MSDK_STRING("\n"));

  return;

} // void PrintInfo(...)

/* ******************************************************************* */

mfxStatus InitParamsVPP(mfxVideoParam* pParams, sInputParams* pInParams)
{
  mfxU16 maxWidth = 0, maxHeight = 0, i;
  MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);
  MSDK_CHECK_POINTER(pInParams,  MFX_ERR_NULL_PTR);

  if (pInParams->inFrameInfo[VPP_IN].nWidth == 0 || pInParams->inFrameInfo[VPP_IN].nHeight == 0 ){
    return MFX_ERR_UNSUPPORTED;
  }
  if (pInParams->outFrameInfo.nWidth == 0 || pInParams->outFrameInfo.nHeight == 0 ){
    return MFX_ERR_UNSUPPORTED;
  }

  memset(pParams, 0, sizeof(mfxVideoParam));

  /* input data */
  pParams->vpp.In.FourCC          = pInParams->inFrameInfo[VPP_IN].FourCC;

  pParams->vpp.In.CropX = pInParams->inFrameInfo[VPP_IN].CropX;
  pParams->vpp.In.CropY = pInParams->inFrameInfo[VPP_IN].CropY;
  pParams->vpp.In.CropW = pInParams->inFrameInfo[VPP_IN].CropW;
  pParams->vpp.In.CropH = pInParams->inFrameInfo[VPP_IN].CropH;

  // width must be a multiple of 16
  // height must be a multiple of 16 in case of frame picture and
  // a multiple of 32 in case of field picture
  for (i = 0; i < pInParams->numStreams; i++)
  {
    pInParams->inFrameInfo[i].nWidth = MSDK_ALIGN16(pInParams->inFrameInfo[i].nWidth);
    pInParams->inFrameInfo[i].nHeight = (MFX_PICSTRUCT_PROGRESSIVE == pInParams->inFrameInfo[i].PicStruct)?
                                         MSDK_ALIGN16(pInParams->inFrameInfo[i].nHeight) : MSDK_ALIGN32(pInParams->inFrameInfo[i].nHeight);
    if (pInParams->inFrameInfo[i].nWidth > maxWidth)
      maxWidth = pInParams->inFrameInfo[i].nWidth;
    if (pInParams->inFrameInfo[i].nHeight > maxHeight)
      maxHeight = pInParams->inFrameInfo[i].nHeight;
  }
  pParams->vpp.In.Width = maxWidth;
  pParams->vpp.In.Height= maxHeight;

  pParams->vpp.In.PicStruct = pInParams->inFrameInfo[VPP_IN].PicStruct;
  pParams->vpp.In.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

  ConvertFrameRate(pInParams->inFrameInfo[VPP_IN].dFrameRate,
                   &pParams->vpp.In.FrameRateExtN,
                   &pParams->vpp.In.FrameRateExtD);

  /* output data */
  pParams->vpp.Out.FourCC          = pInParams->outFrameInfo.FourCC;

  pParams->vpp.Out.CropX = pInParams->outFrameInfo.CropX;
  pParams->vpp.Out.CropY = pInParams->outFrameInfo.CropY;
  pParams->vpp.Out.CropW = pInParams->outFrameInfo.CropW;
  pParams->vpp.Out.CropH = pInParams->outFrameInfo.CropH;

  // width must be a multiple of 16
  // height must be a multiple of 16 in case of frame picture and
  // a multiple of 32 in case of field picture
  pParams->vpp.Out.Width = MSDK_ALIGN16(pInParams->outFrameInfo.nWidth);
  pParams->vpp.Out.Height= (MFX_PICSTRUCT_PROGRESSIVE == pInParams->outFrameInfo.PicStruct)?
                           MSDK_ALIGN16(pInParams->outFrameInfo.nHeight) : MSDK_ALIGN32(pInParams->outFrameInfo.nHeight);

  pParams->vpp.Out.PicStruct = pInParams->outFrameInfo.PicStruct;
  pParams->vpp.Out.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

  ConvertFrameRate(pInParams->outFrameInfo.dFrameRate,
                   &pParams->vpp.Out.FrameRateExtN,
                   &pParams->vpp.Out.FrameRateExtD);


  // this pattern is checked by VPP
  if( pInParams->bd3dAlloc )
  {
    pParams->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
  }
  else
  {
    pParams->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
  }

  return MFX_ERR_NONE;

} // mfxStatus InitParamsVPP(mfxVideoParam* pParams, sInputParams* pInParams)

/* ******************************************************************* */

mfxStatus CreateFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams, mfxIMPL impl)
{
  mfxStatus  sts = MFX_ERR_NONE;
  mfxVersion version = {{3, 1}}; // as this version of sample demonstrates the new DOUSE structure used to turn on VPP filters

  MSDK_CHECK_POINTER(pProcessor, MFX_ERR_NULL_PTR);
  MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

  WipeFrameProcessor(pProcessor);

  //MFX session
  if (MFX_IMPL_HARDWARE == impl)
  {
    // try searching on all display adapters
    sts = pProcessor->mfxSession.Init(MFX_IMPL_HARDWARE_ANY, &version);
  }
  else
    sts = pProcessor->mfxSession.Init(MFX_IMPL_SOFTWARE, &version);

  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeFrameProcessor(pProcessor));

  // VPP
  pProcessor->pmfxVPP = new MFXVideoVPP(pProcessor->mfxSession);

  return MFX_ERR_NONE;

} // mfxStatus CreateFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams)

/* ******************************************************************* */

#ifdef D3D_SURFACES_SUPPORT
mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager, mfxU32 nAdapterNum)
{
  MSDK_CHECK_POINTER(ppManager, MFX_ERR_NULL_PTR);

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
                                nAdapterNum,
                                D3DDEVTYPE_HAL,
                                window,
                                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                                &d3dParams,
                                &d3dDevice);

  if (FAILED(hr) || !d3dDevice)
  {
    return MFX_ERR_NULL_PTR;
  }

  UINT resetToken = 0;
  CComPtr<IDirect3DDeviceManager9> d3dDeviceManager = 0;
  hr = DXVA2CreateDirect3DDeviceManager9(&resetToken, &d3dDeviceManager);

  if (FAILED(hr) || !d3dDeviceManager)
  {
    return MFX_ERR_NULL_PTR;
  }

  hr = d3dDeviceManager->ResetDevice(d3dDevice, resetToken);
  if (FAILED(hr))
  {
    return MFX_ERR_UNDEFINED_BEHAVIOR;
  }

  *ppManager = d3dDeviceManager.Detach();

  if (NULL == *ppManager)
  {
    return MFX_ERR_NULL_PTR;
  }

  return MFX_ERR_NONE;

} // mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager)
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
  MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
  MSDK_CHECK_RESULT(sts,   MFX_ERR_NONE, sts);

  return MFX_ERR_NONE;

} // mfxStatus InitFrameProcessor(sFrameProcessor* pProcessor, mfxVideoParam* pParams)

/* ******************************************************************* */

mfxStatus InitSurfaces(sMemoryAllocator* pAllocator, mfxFrameAllocRequest* pRequest, mfxFrameInfo* pInfo, mfxU32 indx)
{
  mfxStatus sts = MFX_ERR_NONE;
  mfxU16    nFrames, i;

  sts = pAllocator->pMfxAllocator->Alloc(pAllocator->pMfxAllocator->pthis, pRequest, &(pAllocator->response[indx]));
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

  nFrames = pAllocator->response[indx].NumFrameActual;
  pAllocator->pSurfaces[indx] = new mfxFrameSurface1 [nFrames];

  for (i = 0; i < nFrames; i++)
  {
    memset(&(pAllocator->pSurfaces[indx][i]), 0, sizeof(mfxFrameSurface1));
    pAllocator->pSurfaces[indx][i].Info = *pInfo;

    if( pAllocator->bUsedAsExternalAllocator )
    {
      pAllocator->pSurfaces[indx][i].Data.MemId = pAllocator->response[indx].mids[i];
    }
    else
    {
      sts = pAllocator->pMfxAllocator->Lock(pAllocator->pMfxAllocator->pthis,
                                            pAllocator->response[indx].mids[i],
                                            &(pAllocator->pSurfaces[indx][i].Data));
      MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));
    }
  }

  return sts;

} // mfxStatus InitSurfaces(...)

/* ******************************************************************* */

mfxStatus InitMemoryAllocator(sFrameProcessor* pProcessor, sMemoryAllocator* pAllocator, mfxVideoParam* pParams)
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
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

  if( (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY) == pParams->IOPattern)
  {
    pAllocator->bd3dAlloc = true;
  }
  else
  {
    pAllocator->bd3dAlloc = false;
  }

  if( pAllocator->bd3dAlloc )
  {
#ifdef D3D_SURFACES_SUPPORT

    mfxU32 adapterNum = MSDKAdapter::GetNumber(pProcessor->mfxSession);
    // prepare device manager
    sts = CreateDeviceManager(&(pAllocator->pd3dDeviceManager), adapterNum);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, pAllocator->pd3dDeviceManager);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    // prepare allocator
    pAllocator->pMfxAllocator = new D3DFrameAllocator;

    D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;

    pd3dAllocParams->pManager = pAllocator->pd3dDeviceManager;
    pAllocator->pAllocatorParams = pd3dAllocParams;

    /* In case of video memory we must provide mediasdk with external allocator
    thus we demonstrate "external allocator" usage model.
    Call SetAllocator to pass allocator to mediasdk */
    sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    pAllocator->bUsedAsExternalAllocator = true;
#endif
#ifdef LIBVA_SUPPORT
    pAllocator->pVALibrary = CreateLibVA();
    if (!pAllocator->pVALibrary) sts = MFX_ERR_MEMORY_ALLOC;
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    /* It's possible to skip failed result here and switch to SW implementation,
    but we don't process this way */

    sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)pAllocator->pVALibrary->GetVADisplay());
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

    // prepare allocator
    pAllocator->pMfxAllocator = new vaapiFrameAllocator;

    vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;

    p_vaapiAllocParams->m_dpy = pAllocator->pVALibrary->GetVADisplay();
    pAllocator->pAllocatorParams = p_vaapiAllocParams;

    /* In case of video memory we must provide mediasdk with external allocator

    thus we demonstrate "external allocator" usage model.
    Call SetAllocator to pass allocator to mediasdk */
    sts = pProcessor->mfxSession.SetFrameAllocator(pAllocator->pMfxAllocator);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));
    pAllocator->bUsedAsExternalAllocator = true;
#endif
  }
  else
  {
#ifdef LIBVA_SUPPORT
    //in case of system memory allocator we also have to pass MFX_HANDLE_VA_DISPLAY to HW library
    mfxIMPL impl;
    pProcessor->mfxSession.QueryIMPL(&impl);

    if(MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(impl))
    {
      pAllocator->pVALibrary = CreateLibVA();
      if (!pAllocator->pVALibrary) sts = MFX_ERR_MEMORY_ALLOC;
      MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

      sts = pProcessor->mfxSession.SetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL)pAllocator->pVALibrary->GetVADisplay());
      MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));
    }
#endif

    // prepare allocator
    pAllocator->pMfxAllocator = new SysMemFrameAllocator;

    /* In case of system memory we demonstrate "no external allocator" usage model.
    We don't call SetAllocator, mediasdk uses internal allocator.
    We use software allocator object only as a memory manager for application */
  }

  sts = pAllocator->pMfxAllocator->Init(pAllocator->pAllocatorParams);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

  sts = pProcessor->pmfxVPP->QueryIOSurf(pParams, request);
  MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);

  // alloc frames for vpp
  // [IN]
  sts = InitSurfaces(pAllocator, &(request[VPP_IN]), &(pParams->vpp.In), VPP_IN);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

  // [OUT]
  sts = InitSurfaces(pAllocator, &(request[VPP_OUT]), &(pParams->vpp.Out), VPP_OUT);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMemoryAllocator(pAllocator));

  return MFX_ERR_NONE;

} // mfxStatus InitMemoryAllocator(...)

/* ******************************************************************* */

mfxStatus InitResources(sAppResources* pResources, mfxVideoParam* pParams, mfxIMPL impl)
{
  mfxStatus sts = MFX_ERR_NONE;

  MSDK_CHECK_POINTER(pResources, MFX_ERR_NULL_PTR);
  MSDK_CHECK_POINTER(pParams,    MFX_ERR_NULL_PTR);

  sts = CreateFrameProcessor(pResources->pProcessor, pParams, impl);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeResources(pResources));

  sts = InitMemoryAllocator(pResources->pProcessor, pResources->pAllocator, pParams);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeResources(pResources));

  sts = InitFrameProcessor(pResources->pProcessor, pParams);
  MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeResources(pResources));

  return sts;

} // mfxStatus InitResources(sAppResources* pResources, mfxVideoParam* pParams)

/* ******************************************************************* */

void WipeFrameProcessor(sFrameProcessor* pProcessor)
{
  MSDK_CHECK_POINTER_NO_RET(pProcessor);

  MSDK_SAFE_DELETE(pProcessor->pmfxVPP);

  if (pProcessor->mfxSession.operator mfxSession())
  {
      pProcessor->mfxSession.Close();
  }

} // void WipeFrameProcessor(sFrameProcessor* pProcessor)

void WipeMemoryAllocator(sMemoryAllocator* pAllocator)
{
  MSDK_CHECK_POINTER_NO_RET(pAllocator);

  MSDK_SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_IN]);
  MSDK_SAFE_DELETE_ARRAY(pAllocator->pSurfaces[VPP_OUT]);

  // delete frames
  if (pAllocator->pMfxAllocator)
  {
    pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_IN]);
    pAllocator->pMfxAllocator->Free(pAllocator->pMfxAllocator->pthis, &pAllocator->response[VPP_OUT]);
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
#ifdef LIBVA_SUPPORT
  if (pAllocator->pVALibrary)
  {
      MSDK_SAFE_DELETE(pAllocator->pVALibrary);
      pAllocator->pVALibrary = NULL;
  }
#endif

  // delete allocator parameters
  MSDK_SAFE_DELETE(pAllocator->pAllocatorParams);

} // void WipeMemoryAllocator(sMemoryAllocator* pAllocator)

/* ******************************************************************* */

void WipeResources(sAppResources* pResources)
{
  MSDK_CHECK_POINTER_NO_RET(pResources);

  WipeFrameProcessor(pResources->pProcessor);

  WipeMemoryAllocator(pResources->pAllocator);

  for (int i = 0; i < pResources->numSrcFiles; i++)
  {
    if (pResources->pSrcFileReaders[i])
    {
      pResources->pSrcFileReaders[i]->Close();
    }
  }

  if (pResources->compositeConfig.InputStream)
  {
      delete[] pResources->compositeConfig.InputStream;
      pResources->compositeConfig.InputStream = NULL;
  }

  if (pResources->pDstFileWriter)
  {
    pResources->pDstFileWriter->Close();
  }

} // void WipeResources(sAppResources* pResources)

/* ******************************************************************* */

CRawVideoReader::CRawVideoReader()
{
  m_fSrc = 0;

} // CRawVideoReader::CRawVideoReader()

mfxStatus CRawVideoReader::Init(const msdk_char *strFileName)
{
  Close();

  MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

  MSDK_FOPEN(m_fSrc, strFileName, MSDK_STRING("rb"));
  MSDK_CHECK_POINTER(m_fSrc, MFX_ERR_ABORTED);

  return MFX_ERR_NONE;

} // mfxStatus CRawVideoReader::Init(const msdk_char *strFileName)

CRawVideoReader::~CRawVideoReader()
{
  Close();

} // CRawVideoReader::~CRawVideoReader()

void CRawVideoReader::Close()
{
  if (m_fSrc != 0)
  {
    fclose(m_fSrc);
    m_fSrc = 0;
  }

} // void CRawVideoReader::Close()

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
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
    }

    w     >>= 1;
    h     >>= 1;
    pitch >>= 1;
    // load V
    ptr  = pData->V + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
    }
    // load U
    ptr  = pData->U + (pInfo->CropX >> 1) + (pInfo->CropY >> 1) * pitch;
    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
    }

  }
  else if( pInfo->FourCC == MFX_FOURCC_NV12 )
  {
    ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

    // read luminance plane
    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
    }

    // load UV
    h     >>= 1;
    ptr = pData->UV + pInfo->CropX + (pInfo->CropY >> 1) * pitch;
    for (i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, w, MFX_ERR_MORE_DATA);
    }
  }
  else if (pInfo->FourCC == MFX_FOURCC_RGB3)
  {
    MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);

    ptr = MSDK_MIN( MSDK_MIN(pData->R, pData->G), pData->B );
    ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 3*w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 3*w, MFX_ERR_MORE_DATA);
    }
  }
  else if (pInfo->FourCC == MFX_FOURCC_RGB4)
  {
    MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);
    // there is issue with A channel in case of d3d, so A-ch is ignored
    //MSDK_CHECK_POINTER(pData->A, MFX_ERR_NOT_INITIALIZED);

    ptr = MSDK_MIN( MSDK_MIN(pData->R, pData->G), pData->B );
    ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 4*w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 4*w, MFX_ERR_MORE_DATA);
    }
  }
  else if (pInfo->FourCC == MFX_FOURCC_YUY2)
  {
    ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

    for(i = 0; i < h; i++)
    {
      nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, 2*w, m_fSrc);
      IOSTREAM_CHECK_NOT_EQUAL(nBytesRead, 2*w, MFX_ERR_MORE_DATA);
    }
  }
  else
  {
    return MFX_ERR_UNSUPPORTED;
  }

  return MFX_ERR_NONE;

} // mfxStatus CRawVideoReader::LoadNextFrame(...)

/* ******************************************************************* */

CRawVideoWriter::CRawVideoWriter()
{
  m_fDst = 0;

  return;

} // CRawVideoWriter::CRawVideoWriter()

mfxStatus CRawVideoWriter::Init(const msdk_char *strFileName)
{
  Close();

  MSDK_CHECK_POINTER(strFileName, MFX_ERR_NULL_PTR);

  MSDK_FOPEN(m_fDst, strFileName, MSDK_STRING("wb"));
  MSDK_CHECK_POINTER(m_fDst, MFX_ERR_ABORTED);

  return MFX_ERR_NONE;

} // mfxStatus CRawVideoWriter::Init(const msdk_char *strFileName)

CRawVideoWriter::~CRawVideoWriter()
{
  Close();

  return;

} // CRawVideoWriter::~CRawVideoWriter()

void CRawVideoWriter::Close()
{
  if (m_fDst != 0){

    fclose(m_fDst);
    m_fDst = 0;
  }

  return;

} // void CRawVideoWriter::Close()

mfxStatus CRawVideoWriter::WriteFrame(mfxFrameData* pData, mfxFrameInfo* pInfo)
{
  mfxU32 nBytesRead   = 0;

  mfxU32 i, h, w, pitch;
  mfxU8* ptr;

  MSDK_CHECK_POINTER(pData, MFX_ERR_NOT_INITIALIZED);
  MSDK_CHECK_POINTER(pInfo, MFX_ERR_NOT_INITIALIZED);

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
  else if( pInfo->FourCC == MFX_FOURCC_NV12 )
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
  else if( pInfo->FourCC == MFX_FOURCC_YUY2 )
  {
      ptr = pData->Y + pInfo->CropX + pInfo->CropY * pitch;

      for(i = 0; i < h; i++)
      {
          MSDK_CHECK_NOT_EQUAL( fwrite(ptr+ i * pitch, 1, 2*w, m_fDst), 2*w, MFX_ERR_UNDEFINED_BEHAVIOR);
      }
  }
  else if( pInfo->FourCC == MFX_FOURCC_RGB4 )
  {
      MSDK_CHECK_POINTER(pData->R, MFX_ERR_NOT_INITIALIZED);
      MSDK_CHECK_POINTER(pData->G, MFX_ERR_NOT_INITIALIZED);
      MSDK_CHECK_POINTER(pData->B, MFX_ERR_NOT_INITIALIZED);

      ptr = MSDK_MIN( MSDK_MIN(pData->R, pData->G), pData->B );
      ptr = ptr + pInfo->CropX + pInfo->CropY * pitch;

      for(i = 0; i < h; i++)
      {
          MSDK_CHECK_NOT_EQUAL( fwrite(ptr + i * pitch, 1, 4*w, m_fDst), 4*w, MFX_ERR_UNDEFINED_BEHAVIOR);
      }
  }
  else
  {
    return MFX_ERR_UNSUPPORTED;
  }

  return MFX_ERR_NONE;

} // mfxStatus CRawVideoWriter::WriteFrame(...)

mfxStatus GetFreeSurface(mfxFrameSurface1* pSurfacesPool, mfxU16 nPoolSize, mfxFrameSurface1** ppSurface)
{
  MSDK_CHECK_POINTER(pSurfacesPool, MFX_ERR_NULL_PTR);
  MSDK_CHECK_POINTER(ppSurface,     MFX_ERR_NULL_PTR);

  mfxU32 timeToSleep = 10; // milliseconds
  mfxU32 numSleeps = MSDK_VPP_WAIT_INTERVAL / timeToSleep + 1; // at least 1

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

} // mfxStatus GetFreeSurface(...)

/* ******************************************************************* */

mfxStatus ConfigVideoEnhancementFilters( sInputParams* pParams, sAppResources* pResources )
{
    mfxVideoParam*   pVppParam = pResources->pVppParams;
    mfxU32  enabledFilterCount = 0;

    // [0] common tuning params
    pVppParam->NumExtParam = 0;
    // to simplify logic
    pVppParam->ExtParam    = (mfxExtBuffer**)pResources->pExtBuf;

    pResources->extDoUse.Header.BufferId = MFX_EXTBUFF_VPP_DOUSE;
    pResources->extDoUse.Header.BufferSz = sizeof(mfxExtVPPDoUse);
    pResources->extDoUse.NumAlg  = 0;
    pResources->extDoUse.AlgList = NULL;

    // [1] video enhancement algorithms can be enabled with default parameters
    if( VPP_FILTER_DISABLED != pParams->denoiseParam.mode )
    {
        pResources->tabDoUseAlg[enabledFilterCount++] = MFX_EXTBUFF_VPP_DENOISE;
    }
    if( VPP_FILTER_DISABLED != pParams->vaParam.mode )
    {
        pResources->tabDoUseAlg[enabledFilterCount++] = MFX_EXTBUFF_VPP_SCENE_ANALYSIS;
    }
    if( VPP_FILTER_DISABLED != pParams->procampParam.mode )
    {
        pResources->tabDoUseAlg[enabledFilterCount++] = MFX_EXTBUFF_VPP_PROCAMP;
    }
    if( VPP_FILTER_DISABLED != pParams->detailParam.mode )
    {
        pResources->tabDoUseAlg[enabledFilterCount++] = MFX_EXTBUFF_VPP_DETAIL;
    }
    if( VPP_FILTER_DISABLED != pParams->istabParam.mode )
    {
        pResources->tabDoUseAlg[enabledFilterCount++] = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
    }

    if( enabledFilterCount > 0 )
    {
        pResources->extDoUse.NumAlg  = enabledFilterCount;
        pResources->extDoUse.AlgList = pResources->tabDoUseAlg;
        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->extDoUse);
    }

    // [2] video enhancement algorithms can be configured
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->denoiseParam.mode )
    {
        pResources->denoiseConfig.Header.BufferId = MFX_EXTBUFF_VPP_DENOISE;
        pResources->denoiseConfig.Header.BufferSz = sizeof(mfxExtVPPDenoise);

        pResources->denoiseConfig.DenoiseFactor   = pParams->denoiseParam.factor;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->denoiseConfig);
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->vaParam.mode )
    {
        // video analysis filters isn't configured
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->procampParam.mode )
    {
        pResources->procampConfig.Header.BufferId = MFX_EXTBUFF_VPP_PROCAMP;
        pResources->procampConfig.Header.BufferSz = sizeof(mfxExtVPPProcAmp);

        pResources->procampConfig.Hue        = pParams->procampParam.hue;
        pResources->procampConfig.Saturation = pParams->procampParam.saturation;
        pResources->procampConfig.Contrast   = pParams->procampParam.contrast;
        pResources->procampConfig.Brightness = pParams->procampParam.brightness;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->procampConfig);
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->detailParam.mode )
    {
        pResources->detailConfig.Header.BufferId = MFX_EXTBUFF_VPP_DETAIL;
        pResources->detailConfig.Header.BufferSz = sizeof(mfxExtVPPDetail);

        pResources->detailConfig.DetailFactor   = pParams->detailParam.factor;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->detailConfig);
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->istabParam.mode )
    {
        pResources->istabConfig.Header.BufferId = MFX_EXTBUFF_VPP_IMAGE_STABILIZATION;
        pResources->istabConfig.Header.BufferSz = sizeof(mfxExtVPPImageStab);
        pResources->istabConfig.Mode            = pParams->istabParam.istabMode;

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->istabConfig);
    }
    if( VPP_FILTER_ENABLED_CONFIGURED == pParams->compositionParam.mode )
    {
        pResources->compositeConfig.Header.BufferId = MFX_EXTBUFF_VPP_COMPOSITE;
        pResources->compositeConfig.Header.BufferSz = sizeof(mfxExtVPPComposite);
        pResources->compositeConfig.NumInputStream  = pParams->numStreams;
        pResources->compositeConfig.InputStream     = new mfxVPPCompInputStream[pResources->compositeConfig.NumInputStream];

        for (int i = 0; i < pResources->compositeConfig.NumInputStream; i++)
        {
            pResources->compositeConfig.InputStream[i].DstX = pParams->compositionParam.streamInfo[i].compStream.DstX;
            pResources->compositeConfig.InputStream[i].DstY = pParams->compositionParam.streamInfo[i].compStream.DstY;
            pResources->compositeConfig.InputStream[i].DstW = pParams->compositionParam.streamInfo[i].compStream.DstW;
            pResources->compositeConfig.InputStream[i].DstH = pParams->compositionParam.streamInfo[i].compStream.DstH;
            if (pParams->compositionParam.streamInfo[i].compStream.GlobalAlphaEnable != 0 )
            {
                pResources->compositeConfig.InputStream[i].GlobalAlphaEnable = pParams->compositionParam.streamInfo[i].compStream.GlobalAlphaEnable;
                pResources->compositeConfig.InputStream[i].GlobalAlpha = pParams->compositionParam.streamInfo[i].compStream.GlobalAlpha;
            }
            if (pParams->compositionParam.streamInfo[i].compStream.LumaKeyEnable != 0 )
            {
                pResources->compositeConfig.InputStream[i].LumaKeyEnable = pParams->compositionParam.streamInfo[i].compStream.LumaKeyEnable;
                pResources->compositeConfig.InputStream[i].LumaKeyMin = pParams->compositionParam.streamInfo[i].compStream.LumaKeyMin;
                pResources->compositeConfig.InputStream[i].LumaKeyMax = pParams->compositionParam.streamInfo[i].compStream.LumaKeyMax;

            }


        }

        pVppParam->ExtParam[pVppParam->NumExtParam++] = (mfxExtBuffer*)&(pResources->compositeConfig);
    }

    // confirm configuration
    if( 0 == pVppParam->NumExtParam )
    {
        pVppParam->ExtParam = NULL;
    }

    return MFX_ERR_NONE;

} // mfxStatus ConfigVideoEnhancementFilters( sAppResources* pResources, mfxVideoParam* pParams )

/* EOF */
