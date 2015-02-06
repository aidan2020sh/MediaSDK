/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2014 Intel Corporation. All Rights Reserved.

File Name: libmfx_allocator_d3d11.cpp

\* ****************************************************************************** */
#if defined (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)

#include "mfxvideo++int.h"
#include "libmfx_allocator_d3d11.h"

#include "ippcore.h"
#include "ipps.h"

#include "mfx_utils.h"

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N','V','1','2')
#define D3DFMT_P010 (D3DFORMAT)MAKEFOURCC('P','0','1','0')
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y','V','1','2')
#define D3DFMT_IMC3 (D3DFORMAT)MAKEFOURCC('I','M','C','3')

#define D3DFMT_YUV400   (D3DFORMAT)MAKEFOURCC('4','0','0','P')
#define D3DFMT_YUV411   (D3DFORMAT)MAKEFOURCC('4','1','1','P')
#define D3DFMT_YUV422H  (D3DFORMAT)MAKEFOURCC('4','2','2','H')
#define D3DFMT_YUV422V  (D3DFORMAT)MAKEFOURCC('4','2','2','V')
#define D3DFMT_YUV444   (D3DFORMAT)MAKEFOURCC('4','4','4','P')

#define MFX_FOURCC_P8_MBDATA  (D3DFORMAT)MFX_MAKEFOURCC('P','8','M','B')

#define DXGI_FORMAT_BGGR MAKEFOURCC('I','R','W','0')
#define DXGI_FORMAT_RGGB MAKEFOURCC('I','R','W','1')
#define DXGI_FORMAT_GRBG MAKEFOURCC('I','R','W','2')
#define DXGI_FORMAT_GBRG MAKEFOURCC('I','R','W','3')

template<class T> inline
T align_value(size_t nValue, size_t lAlignValue = DEFAULT_ALIGN_VALUE)
{
    return static_cast<T> ((nValue + (lAlignValue - 1)) &
        ~(lAlignValue - 1));
}


DXGI_FORMAT mfxDefaultAllocatorD3D11::MFXtoDXGI(mfxU32 format)
{
    switch (format)
    {
    case MFX_FOURCC_P010:
        return DXGI_FORMAT_P010;

    case MFX_FOURCC_NV12:
        return DXGI_FORMAT_NV12;

    case D3DFMT_YUY2:
        return DXGI_FORMAT_YUY2;

    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
        return DXGI_FORMAT_420_OPAQUE;

    case MFX_FOURCC_RGB4:
        return DXGI_FORMAT_B8G8R8A8_UNORM;

    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_MBDATA:
        return DXGI_FORMAT_P8;// aya???

    case DXGI_FORMAT_AYUV:
        return DXGI_FORMAT_AYUV;
    case MFX_FOURCC_R16:
    case MFX_FOURCC_R16_BGGR:
    case MFX_FOURCC_R16_RGGB:
    case MFX_FOURCC_R16_GRBG:
    case MFX_FOURCC_R16_GBRG:
        return DXGI_FORMAT_R16_TYPELESS;
    }
    return DXGI_FORMAT_UNKNOWN;

} // mfxDefaultAllocatorD3D11::MFXtoDXGI(mfxU32 format)

// D3D11 surface working
mfxStatus mfxDefaultAllocatorD3D11::AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    // only NV12 and D3DFMT_P8 buffers are supported by HW
    switch(request->Info.FourCC)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_IMC3:
    case MFX_FOURCC_YUV400:
    case MFX_FOURCC_YUV411:
    case MFX_FOURCC_YUV422H:
    case MFX_FOURCC_YUV422V:
    case MFX_FOURCC_YUV444:
    case MFX_FOURCC_RGBP:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_P8:
    case MFX_FOURCC_P8_MBDATA:
    case DXGI_FORMAT_AYUV:
    case MFX_FOURCC_R16_RGGB:
    case MFX_FOURCC_R16_BGGR:
    case MFX_FOURCC_R16_GBRG:
    case MFX_FOURCC_R16_GRBG:
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }

    mfxU16 numAllocated, maxNumFrames;
    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    pSelf->isBufferKeeper = false;
    pSelf->m_StagingSrfPool = NULL;

    // frames were allocated
    // return existent frames
    if (pSelf->NumFrames)
    {
        if (request->NumFrameSuggested > pSelf->NumFrames)
            return MFX_ERR_MEMORY_ALLOC;
        else
        {
            response->mids = &pSelf->m_frameHandles[0];
            return MFX_ERR_NONE;
        }

    }

    UINT    width = request->Info.Width;
    UINT    height = request->Info.Height;

    HRESULT hr = S_OK;

    maxNumFrames = request->NumFrameSuggested;

    //Create Texture
    D3D11_TEXTURE2D_DESC Desc = {0};
    Desc.Width = width;
    Desc.Height =  height;
    Desc.MipLevels = 1;
    Desc.ArraySize = 1;
    Desc.Format = MFXtoDXGI(request->Info.FourCC);
    Desc.SampleDesc.Count = 1;
    Desc.Usage = D3D11_USAGE_DEFAULT;
    Desc.BindFlags = D3D11_BIND_DECODER;

    //aya: P8 with 0
    if(request->Info.FourCC == MFX_FOURCC_P8)
    {
        pSelf->m_NumSurface = 1;

        D3D11_BUFFER_DESC desc = { 0 };

        desc.ByteWidth           = request->Info.Width * request->Info.Height;
        desc.Usage               = D3D11_USAGE_STAGING;
        desc.BindFlags           = 0;
        desc.CPUAccessFlags      = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags           = 0;
        desc.StructureByteStride = 0;

        ID3D11Buffer * buffer = 0;
        hr = pSelf->m_pD11Device->CreateBuffer(&desc, 0, &buffer);
        if (FAILED(hr))
            return MFX_ERR_MEMORY_ALLOC;
        Desc.BindFlags = 0;
        pSelf->m_SrfPool.push_back(reinterpret_cast<ID3D11Texture2D *>(buffer));
        pSelf->isBufferKeeper = true;
        {
            pSelf->m_frameHandles.resize(maxNumFrames);
            for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
            {
                size_t id = (size_t)(numAllocated + 1);
                pSelf->m_frameHandles[numAllocated] = (mfxHDL)id ;
            }
            response->mids = &pSelf->m_frameHandles[0];
            response->NumFrameActual = maxNumFrames;
            pSelf->NumFrames = maxNumFrames;
        }
    }
    else
    {

        if ( (MFX_MEMTYPE_FROM_VPPIN & request->Type) && (DXGI_FORMAT_YUY2 == Desc.Format) ||
            (DXGI_FORMAT_B8G8R8A8_UNORM == Desc.Format) )
        {
            Desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (Desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }

        if ( (MFX_MEMTYPE_FROM_VPPOUT & request->Type) ||
             (MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
        {
            Desc.BindFlags = D3D11_BIND_RENDER_TARGET;

            // change request->Type
            // only MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET is allowed for VPP OUT
            if (!(MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET & request->Type))
            {
                request->Type = request->Type & 0xFF0F; //reset old flags
                request->Type = request->Type | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET;
            }

            if (Desc.ArraySize > 2)
                return MFX_ERR_MEMORY_ALLOC;
        }
        pSelf->m_NumSurface = maxNumFrames;
        pSelf->m_SrfPool.resize(maxNumFrames);

        // aya: d3d11 wo
        if( DXGI_FORMAT_P8 == Desc.Format )
        {
            Desc.BindFlags = 0;
        }

        if ( DXGI_FORMAT_R16_TYPELESS == Desc.Format)
        {
            // R16 Bayer requires having special resource extension reflecting
            // real Bayer format
            Desc.MipLevels = 1;
            Desc.ArraySize = 1;
            Desc.SampleDesc.Count   = 1;
            Desc.SampleDesc.Quality = 0;
            Desc.Usage     = D3D11_USAGE_DEFAULT;
            Desc.BindFlags = 0;
            Desc.CPUAccessFlags = 0;// = D3D11_CPU_ACCESS_WRITE;
            Desc.MiscFlags = 0;
            RESOURCE_EXTENSION extnDesc;
            ZeroMemory( &extnDesc, sizeof(RESOURCE_EXTENSION) );
            memcpy( extnDesc.Key, RESOURCE_EXTENSION_KEY, sizeof(extnDesc.Key) );
            extnDesc.ApplicationVersion = EXTENSION_INTERFACE_VERSION;
            extnDesc.Type    = RESOURCE_EXTENSION_TYPE_4_0::RESOURCE_EXTENSION_CAMERA_PIPE;
            extnDesc.Data[0] = BayerFourCC2FormatFlag(request->Info.FourCC);
            hr = SetResourceExtension(pSelf->m_pD11Device, &extnDesc);
        }

        for (mfxU32 i = 0; i < maxNumFrames; i++)
        {
            hr = pSelf->m_pD11Device->CreateTexture2D(&Desc, NULL, &pSelf->m_SrfPool[i]);
            if (FAILED(hr))
                return MFX_ERR_MEMORY_ALLOC;
        }

        // Create Staging buffers for fast coping (do not need for 420 opaque)
        if(Desc.Format != DXGI_FORMAT_420_OPAQUE)
        {
            Desc.ArraySize = 1;
            Desc.Usage = D3D11_USAGE_STAGING;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            Desc.BindFlags = 0;

            hr = pSelf->m_pD11Device->CreateTexture2D(&Desc, NULL, &pSelf->m_StagingSrfPool);
        }

        // create mapping table
        if (SUCCEEDED(hr))
        {
            pSelf->m_frameHandles.resize(maxNumFrames);
            for (numAllocated = 0; numAllocated < maxNumFrames; numAllocated++)
            {
                size_t id = (size_t)(numAllocated + 1);
                pSelf->m_frameHandles[numAllocated] = (mfxHDL)id ;
            }
            response->mids = &pSelf->m_frameHandles[0];
            response->NumFrameActual = maxNumFrames;
            pSelf->NumFrames = maxNumFrames;
        }
        else
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

        // check the number of allocated frames
        if (numAllocated < request->NumFrameMin)
        {
            FreeFramesHW(pthis, response);
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocatorD3D11::LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    HRESULT hr = S_OK;
    // TBD
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;

    D3D11_TEXTURE2D_DESC Desc = {0};

    D3D11_MAPPED_SUBRESOURCE LockedRect = {0};
    D3D11_MAP MapType = D3D11_MAP_READ;
    UINT MapFlags = D3D11_MAP_FLAG_DO_NOT_WAIT;

    // need to copy surface into staging surface then map
    ID3D11Texture2D *pStagingSurface = pSelf->m_StagingSrfPool;

    if (!pSelf->isBufferKeeper)
    {
        pSelf->m_SrfPool[index]->GetDesc(&Desc);
        pSelf->m_pD11DeviceContext->CopySubresourceRegion(pStagingSurface, 0, 0, 0, 0, pSelf->m_SrfPool[index], 0, NULL);
    }
    else
    {
        pStagingSurface = pSelf->m_SrfPool[index];
        Desc.Format = DXGI_FORMAT_P8;
    }

    do
    {
        hr = pSelf->m_pD11DeviceContext->Map(pStagingSurface, 0, MapType, MapFlags, &LockedRect);
    }
    while (DXGI_ERROR_WAS_STILL_DRAWING == hr);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);



    // not sure about commented formats
    switch (Desc.Format)
    {
    case DXGI_FORMAT_P010:
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pData;
        ptr->U = (mfxU8 *)LockedRect.pData + Desc.Height * LockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;
    case DXGI_FORMAT_NV12:
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pData;
        ptr->U = (mfxU8 *)LockedRect.pData + Desc.Height * LockedRect.RowPitch;
        ptr->V = ptr->U + 1;
        break;
    case  DXGI_FORMAT_420_OPAQUE: // YV12 ????
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pData;
        ptr->V = ptr->Y + Desc.Height * LockedRect.RowPitch;
        ptr->U = ptr->V + (Desc.Height * LockedRect.RowPitch) / 4;
        break;
    case DXGI_FORMAT_YUY2:
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pData;
        ptr->U = ptr->Y + 1;
        ptr->V = ptr->Y + 3;
        break;
    //case D3DFMT_R8G8B8:
    //    ptr->Pitch = (mfxU16)LockedRect.RowPitch;
    //    ptr->B = (mfxU8 *)LockedRect.pData;
    //    ptr->G = ptr->B + 1;
    //    ptr->R = ptr->B + 2;
    //    break;
    //case D3DFMT_A8R8G8B8:
    //    ptr->Pitch = (mfxU16)LockedRect.RowPitch;
    //    ptr->B = (mfxU8 *)LockedRect.pData;
    //    ptr->G = ptr->B + 1;
    //    ptr->R = ptr->B + 2;
    //    break;
    case DXGI_FORMAT_B8G8R8A8_UNORM :
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->B = (mfxU8 *)LockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case DXGI_FORMAT_AYUV :
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->B = (mfxU8 *)LockedRect.pData;
        ptr->G = ptr->B + 1;
        ptr->R = ptr->B + 2;
        ptr->A = ptr->B + 3;
        break;
    case DXGI_FORMAT_P8 :
        ptr->PitchHigh = (mfxU16)(LockedRect.RowPitch / (1 << 16));
        ptr->PitchLow  = (mfxU16)(LockedRect.RowPitch % (1 << 16));
        ptr->Y = (mfxU8 *)LockedRect.pData;
        ptr->U = 0;
        ptr->V = 0;
        break;
    //case D3DFMT_IMC3:
    //    ptr->Pitch = (mfxU16)LockedRect.RowPitch;
    //    ptr->Y = (mfxU8 *)LockedRect.pData;
    //    ptr->U = ptr->Y + desc.Height * LockedRect.RowPitch;
    //    ptr->V = ptr->U + (desc.Height * LockedRect.RowPitch) / 2;
    //    break;
    default:
        return MFX_ERR_LOCK_MEMORY;
    }
    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocatorD3D11::GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    if (0 == mid)
        return MFX_ERR_INVALID_HANDLE;


    if (0 == handle)
        return MFX_ERR_INVALID_HANDLE;

    mfxHDLPair *pPair =  (mfxHDLPair*)handle;
    size_t index      =  (size_t)mid - 1;

    if (index >= pSelf->m_SrfPool.size())
        return MFX_ERR_INVALID_HANDLE;

    // resource pool handle
    pPair->first = pSelf->m_SrfPool[index];
    // index of pool
    pPair->second = (mfxHDL *)0;

    return MFX_ERR_NONE;
}

mfxStatus mfxDefaultAllocatorD3D11::UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData* ptr)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;
    size_t index =  (size_t)mid - 1;
    ID3D11Texture2D    *pStagingSurface  = pSelf->m_StagingSrfPool;
    if (!pSelf->isBufferKeeper)
    {
        pSelf->m_pD11DeviceContext->Unmap(pStagingSurface, 0);
        pSelf->m_pD11DeviceContext->CopySubresourceRegion(pSelf->m_SrfPool[index], 0, 0, 0, 0, pStagingSurface, 0, 0);
    }
    else
    {
        pSelf->m_pD11DeviceContext->Unmap(pSelf->m_SrfPool.back(), 0);
    }

    if (ptr)
    {
        ptr->PitchHigh=0;
        ptr->PitchLow=0;
        ptr->U=ptr->V=ptr->Y=0;
        ptr->A=ptr->R=ptr->G=ptr->B=0;
    }

    return MFX_ERR_NONE;
}
mfxStatus mfxDefaultAllocatorD3D11::FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    if (!pthis)
        return MFX_ERR_INVALID_HANDLE;

    mfxWideHWFrameAllocator *pSelf = (mfxWideHWFrameAllocator*)pthis;

    for (mfxU32 i = 0; i < pSelf->m_NumSurface; i++)
        SAFE_RELEASE(pSelf->m_SrfPool[i]);

    SAFE_RELEASE(pSelf->m_StagingSrfPool);

    response;

    // TBD
    return MFX_ERR_NONE;
}

mfxDefaultAllocatorD3D11::mfxWideHWFrameAllocator::mfxWideHWFrameAllocator(mfxU16 type,
                                                                           ID3D11Device *pD11Device,
                                                                           ID3D11DeviceContext  *pD11DeviceContext):mfxBaseWideFrameAllocator(type),
                                                                                                                    m_pD11Device(pD11Device),
                                                                                                                    m_pD11DeviceContext(pD11DeviceContext)
{
    frameAllocator.Alloc =  &mfxDefaultAllocatorD3D11::AllocFramesHW;
    frameAllocator.Lock =   &mfxDefaultAllocatorD3D11::LockFrameHW;
    frameAllocator.GetHDL = &mfxDefaultAllocatorD3D11::GetHDLHW;
    frameAllocator.Unlock = &mfxDefaultAllocatorD3D11::UnlockFrameHW;
    frameAllocator.Free =   &mfxDefaultAllocatorD3D11::FreeFramesHW;
}


#endif
#endif

