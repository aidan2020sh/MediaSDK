/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2012 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <limits>

//application can provide either generic mid from surface or this wrapper
//wrapper distinguishes from generic mid by highest 1 bit
//if it set then remained pointer points to extended structure of memid
//64 bits system layout
/*----+-----------------------------------------------------------+
|b63=1|63 bits remained for pointer to extended structure of memid|
|b63=0|63 bits from original mfxMemId                             |
+-----+----------------------------------------------------------*/
//32 bits system layout
/*--+---+--------------------------------------------+
|b31=1|31 bits remained for pointer to extended memid|
|b31=0|31 bits remained for surface pointer          |
+---+---+-------------------------------------------*/
//#pragma warning (disable:4293)
class MFXReadWriteMid
{
    static const uintptr_t bits_offset = std::numeric_limits<uintptr_t>::digits - 1;
    static const uintptr_t clear_mask = ~((uintptr_t)1 << bits_offset);
public:
    enum
    {
        //if flag not set it means that read and write
        not_set = 0,
        reuse   = 1,
        read    = 2,
        write   = 4,
    };
    //here mfxmemid might be as MFXReadWriteMid or mfxMemId memid
    MFXReadWriteMid(mfxMemId mid, mfxU8 flag = not_set)
    {
        //setup mid
        m_mid_to_report = (mfxMemId)((uintptr_t)&m_mid | ((uintptr_t)1 << bits_offset));
        if (0 != ((uintptr_t)mid >> bits_offset))
        {
            //it points to extended structure
            mfxMedIdEx * pMemIdExt = reinterpret_cast<mfxMedIdEx *>((uintptr_t)mid & clear_mask);
            m_mid.pId = pMemIdExt->pId;
            if (reuse == flag)
            {
                m_mid.read_write = pMemIdExt->read_write;
            }
            else
            {
                m_mid.read_write = flag;
            }
        }
        else
        {
            m_mid.pId = mid;
            if (reuse == flag)
                m_mid.read_write = not_set;
            else
                m_mid.read_write = flag;
        }

    }
    bool isRead() const
    {
        return 0 != (m_mid.read_write & read) || !m_mid.read_write;
    }
    bool isWrite() const
    {
        return 0 != (m_mid.read_write & write) || !m_mid.read_write;
    }
    /// returns original memid without read write flags
    mfxMemId raw() const
    {
        return m_mid.pId;
    }
    operator mfxMemId() const
    {
        return m_mid_to_report;
    }

private:
    struct mfxMedIdEx
    {
        mfxMemId pId;
        mfxU8 read_write;
    };

    mfxMedIdEx m_mid;
    mfxMemId   m_mid_to_report;
};

#if MFX_D3D11_SUPPORT

#include <d3d11.h>
#include <vector>
#include <map>
#include "mfx_base_allocator.h"
#include <atlbase.h>

struct ID3D11VideoDevice;
struct ID3D11VideoContext;

const char RESOURCE_EXTENSION_KEY[16] = {
    'I','N','T','C',
    'E','X','T','N',
    'R','E','S','O',
    'U','R','C','E' };

#define EXTENSION_INTERFACE_VERSION 0x00040000

struct EXTENSION_BASE
{
    // Input
    char   Key[16];
    UINT   ApplicationVersion;
};

struct RESOURCE_EXTENSION_1_0: EXTENSION_BASE
{
    // Enumeration of the extension
    UINT  Type;   //RESOURCE_EXTENSION_TYPE

    // Extension data
    union
    {
        UINT    Data[16];
        UINT64  Data64[8];
    };
};

typedef RESOURCE_EXTENSION_1_0 RESOURCE_EXTENSION;

struct STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_RESERVED = 0;
};

struct STATE_EXTENSION_TYPE_4_0: STATE_EXTENSION_TYPE_1_0
{
    static const UINT STATE_EXTENSION_CONSERVATIVE_PASTERIZATION = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_RESERVED      = 0;
    static const UINT RESOURCE_EXTENSION_DIRECT_ACCESS = 1;
};

struct RESOURCE_EXTENSION_TYPE_4_0: RESOURCE_EXTENSION_TYPE_1_0
{
    static const UINT RESOURCE_EXTENSION_CAMERA_PIPE = 1 + EXTENSION_INTERFACE_VERSION;
};

struct RESOURCE_EXTENSION_CAMERA_PIPE
{
    enum {
        INPUT_FORMAT_IRW0 = 0x0,
        INPUT_FORMAT_IRW1 = 0x1,
        INPUT_FORMAT_IRW2 = 0x2,
        INPUT_FORMAT_IRW3 = 0x3
    };
};

struct D3D11AllocatorParams : mfxAllocatorParams
{
    ID3D11Device *pDevice;
    bool bUseSingleTexture;
    DWORD uncompressedResourceMiscFlags;

    D3D11AllocatorParams()
        : pDevice()
        , bUseSingleTexture()
        , uncompressedResourceMiscFlags()
    {
    }
};

class D3D11FrameAllocator: public BaseFrameAllocator
{
public:

    D3D11FrameAllocator();
    virtual ~D3D11FrameAllocator();

    virtual mfxStatus Init(mfxAllocatorParams *pParams);
    virtual mfxStatus Close();
    virtual ID3D11Device * GetD3D11Device()
    {
        return m_initParams.pDevice;
    };
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);

protected:
    static  DXGI_FORMAT ConverColortFormat(mfxU32 fourcc);
    virtual mfxStatus CheckRequestType(mfxFrameAllocRequest *request);
    virtual mfxStatus ReleaseResponse(mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus AllocImpl(mfxFrameSurface1 *surface);

    template<typename Type>
    inline HRESULT SetResourceExtension(
        const Type* pExtnDesc )
    {
        D3D11_BUFFER_DESC desc;
        ZeroMemory( &desc, sizeof(desc) );
        desc.ByteWidth      = sizeof(Type);
        desc.Usage          = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        D3D11_SUBRESOURCE_DATA initData;
        ZeroMemory( &initData, sizeof(initData) );
        initData.pSysMem     = pExtnDesc;
        initData.SysMemPitch = sizeof(Type);
        initData.SysMemSlicePitch = 0;

        ID3D11Buffer* pBuffer = NULL;
        HRESULT result = m_initParams.pDevice->CreateBuffer(
                            &desc,
                            &initData,
                            &pBuffer );

        if( pBuffer )
            pBuffer->Release();

        return result;
    }

    D3D11AllocatorParams m_initParams;
    CComPtr<ID3D11DeviceContext> m_pDeviceContext;

    struct TextureResource
    {
        std::vector<mfxMemId> outerMids;
        std::vector<ID3D11Texture2D*> textures;
        std::vector<ID3D11Texture2D*> stagingTexture;
        bool             bAlloc;

        TextureResource()
            : bAlloc(true)
        {
        }

        static bool isAllocated (TextureResource & that)
        {
            return that.bAlloc;
        }
        ID3D11Texture2D* GetTexture(mfxMemId id)
        {
            if (outerMids.empty())
                return NULL;

            return textures[((uintptr_t)id - (uintptr_t)outerMids.front()) % textures.size()];
        }
        UINT GetSubResource(mfxMemId id)
        {
            if (outerMids.empty())
                return NULL;

            return (UINT)(((uintptr_t)id - (uintptr_t)outerMids.front()) / textures.size());
        }
        void Release()
        {
            size_t i = 0;
            for(i = 0; i < textures.size(); i++)
            {
                textures[i]->Release();
            }
            textures.clear();

            for(i = 0; i < stagingTexture.size(); i++)
            {
                stagingTexture[i]->Release();
            }
            stagingTexture.clear();

            //marking texture as deallocated
            bAlloc = false;
        }
    };
    class TextureSubResource
    {
        TextureResource * m_pTarget;
        ID3D11Texture2D * m_pTexture;
        ID3D11Texture2D * m_pStaging;
        UINT m_subResource;
    public:
        TextureSubResource(TextureResource * pTarget = NULL, mfxMemId id = 0)
            : m_pTarget(pTarget)
            , m_pTexture()
            , m_subResource()
            , m_pStaging(NULL)
        {
            if (NULL != m_pTarget && !m_pTarget->outerMids.empty())
            {
                ptrdiff_t idx = (uintptr_t)MFXReadWriteMid(id).raw() - (uintptr_t)m_pTarget->outerMids.front();
                m_pTexture = m_pTarget->textures[idx % m_pTarget->textures.size()];
                m_subResource = (UINT)(idx / m_pTarget->textures.size());
                m_pStaging = m_pTarget->stagingTexture.empty() ? NULL : m_pTarget->stagingTexture[idx];
            }
        }
        ID3D11Texture2D* GetStaging()const
        {
            return m_pStaging;
        }
        ID3D11Texture2D* GetTexture()const
        {
            return m_pTexture;
        }
        UINT GetSubResource()const
        {
            return m_subResource;
        }
        void Release()
        {
            if (NULL != m_pTarget)
                m_pTarget->Release();
        }
    };

    TextureSubResource GetResourceFromMid(mfxMemId);

    std::list <TextureResource> m_resourcesByRequest;//each alloc request generates new item in list

    typedef std::list <TextureResource>::iterator referenceType;
    std::vector<referenceType> m_memIdMap;
};

#endif // #if MFX_D3D11_SUPPORT