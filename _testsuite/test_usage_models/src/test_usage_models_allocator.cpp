//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2010 Intel Corporation. All Rights Reserved.
//

#include "test_usage_models_allocator.h"
#include "test_usage_models_utils.h"

GeneralAllocator::GeneralAllocator() 
{
    m_D3DAllocator.reset(new D3DFrameAllocator);
    m_SYSAllocator.reset(new SysMemFrameAllocator);

} // GeneralAllocator::GeneralAllocator() 


GeneralAllocator::~GeneralAllocator() 
{

} // GeneralAllocator::~GeneralAllocator() 


mfxStatus GeneralAllocator::Init(mfxAllocatorParams *pParams)
{
    mfxStatus sts;

    if( pParams )
    {
        //_tprintf(_T("\n Required HW Allocator Init \n"));
        sts = m_D3DAllocator.get()->Init(pParams);
        //_tprintf(_T("\n sts (m_D3DAllocator.get()->Init) = %i\n"), sts);
        MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);
    }
    
    //_tprintf(_T("\n Required SYS Allocator Init \n"));
    sts = m_SYSAllocator.get()->Init(0);
    //_tprintf(_T("\n sts (m_SYSAllocator.get()->Init) = %i\n"), sts);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    return sts;

} // mfxStatus GeneralAllocator::Init(mfxAllocatorParams *pParams)


mfxStatus GeneralAllocator::Close()
{
    mfxStatus sts;

    sts = m_D3DAllocator.get()->Close();
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    sts = m_SYSAllocator.get()->Close();
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

   return sts;

} // mfxStatus GeneralAllocator::Close()


mfxStatus GeneralAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->Lock(m_D3DAllocator.get(), mid, ptr):
                         m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);

} // mfxStatus GeneralAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)


mfxStatus GeneralAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->Unlock(m_D3DAllocator.get(), mid, ptr):
                         m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);

} // mfxStatus GeneralAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)


mfxStatus GeneralAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->GetHDL(m_D3DAllocator.get(), mid, handle):
                         m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);

} // mfxStatus GeneralAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)


mfxStatus GeneralAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    // try to ReleaseResponsevia D3D allocator
    return isD3DMid(response->mids[0])?m_D3DAllocator.get()->Free(m_D3DAllocator.get(),response):
                                       m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);

} // mfxStatus GeneralAllocator::ReleaseResponse(mfxFrameAllocResponse *response)


mfxStatus GeneralAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts;
    if (request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET || request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)
    {
        sts = m_D3DAllocator.get()->Alloc(m_D3DAllocator.get(), request, response);
        StoreFrameMids(true, response);
    }
    else
    {
        sts = m_SYSAllocator.get()->Alloc(m_SYSAllocator.get(), request, response);
        StoreFrameMids(false, response);
    }

    return sts;

} // mfxStatus GeneralAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)


void    GeneralAllocator::StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response)
{
    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
    {
        m_Mids.insert(std::pair<mfxHDL, bool>(response->mids[i], isD3DFrames));
    }

} // void    GeneralAllocator::StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response)


bool GeneralAllocator::isD3DMid(mfxHDL mid)
{
    std::map<mfxHDL, bool>::iterator it;
    it = m_Mids.find(mid);

    return it->second;

} // bool GeneralAllocator::isD3DMid(mfxHDL mid)


mfxStatus CreateDeviceManager(IDirect3DDeviceManager9** ppManager)
{
  MSDK_CHECK_POINTER(ppManager, MFX_ERR_NULL_PTR);

  CComPtr<IDirect3D9> d3d = Direct3DCreate9(D3D_SDK_VERSION);
  if( !d3d )
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


mfxStatus CreateEnvironmentD3D( sMemoryAllocator* pAllocator )
{
    mfxStatus sts = MFX_ERR_NONE;

    MSDK_CHECK_POINTER(pAllocator, MFX_ERR_NULL_PTR);

    D3DAllocatorParams *pd3dAllocParams = new D3DAllocatorParams;  
    // prepare device manager
    sts = CreateDeviceManager( &(pAllocator->pd3dDeviceManager) );
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);      

    // prepare allocator
    pd3dAllocParams->pManager    = pAllocator->pd3dDeviceManager;
    pAllocator->pAllocatorParams = pd3dAllocParams;

    return sts;

} // mfxStatus CreateEnvironmentD3D( sMemoryAllocator* pAllocator )
/* EOF */
