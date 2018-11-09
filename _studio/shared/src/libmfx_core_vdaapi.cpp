// Copyright (c) 2012-2018 Intel Corporation
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

#include "mfx_common.h"

#if defined(MFX_VA_OSX)

#include "umc_va_dxva2.h"
#include "libmfx_core_vdaapi.h"
#include "mfx_utils.h"
#include "mfx_session.h"
#include "mfx_check_hardware_support.h"
#include "mfx_common_decode_int.h"
#include "ippi.h"

using namespace std;
using namespace UMC;

VDAAPIVideoCORE::VDAAPIVideoCORE(
                                 const mfxU32 adapterNum,
                                 const mfxU32 numThreadsAvailable,
                                 const mfxSession session)
                                 : CommonCORE(numThreadsAvailable, session),
                                    m_adapterNum(adapterNum)
{
    
    m_HWType = MFX_HW_IVB; //MFX::GetHardwareType(m_adapterNum);
    
    if (m_HWType == MFX_HW_UNKNOWN)
    {
        m_HWType = MFX_HW_IVB;
    }

}

VDAAPIVideoCORE::~VDAAPIVideoCORE()
{
    Close();
}

mfxStatus VDAAPIVideoCORE::IsGuidSupported(const GUID,
                                          mfxVideoParam *par, bool isEncoder)
{

    if (!par)   {
        return GetSWFallbackPolicy();
    }
    
    if (IsMVCProfile(par->mfx.CodecProfile) || IsSVCProfile(par->mfx.CodecProfile)) {
        return GetSWFallbackPolicy();
    }
    
    switch (par->mfx.CodecId)
    {
        case MFX_CODEC_VC1:
            return GetSWFallbackPolicy();
        case MFX_CODEC_AVC:
            break;
        case MFX_CODEC_MPEG2:
            return GetSWFallbackPolicy();
        case MFX_CODEC_JPEG:
            return GetSWFallbackPolicy();
        case MFX_CODEC_VP8:
            return GetSWFallbackPolicy();
        default:
            return MFX_ERR_UNSUPPORTED;
    }
    
    if (MFX_HW_IVB == m_HWType || MFX_HW_HSW == m_HWType)
    {
        
        if (par->mfx.FrameInfo.Width > 4096 || par->mfx.FrameInfo.Height > 4096)
        {
            return GetSWFallbackPolicy();
        }
        else
        {
            return MFX_ERR_NONE;
        }
    }
    else // for other platforms decision is based on SNB/ELK assumption
    {
        if (par->mfx.FrameInfo.Width > 1920 || par->mfx.FrameInfo.Height > 1200)
        {
            return GetSWFallbackPolicy();
        }
    }
    return MFX_ERR_NONE;
}

void* VDAAPIVideoCORE::QueryCoreInterface(const MFX_GUID &guid)
{
    if(MFXIVideoCORE_GUID == guid)
    {
        return (void*) this;
    }
    else if( MFXICOREVDAAPI_GUID == guid )
    {
        return (void*) m_pAdapter.get();
    }
    else if (MFXICORE_API_1_19_GUID == guid)
    {
        return &m_API_1_19;
    }
    else
    {
        return NULL;
    }
    
} // void* VAAPIVideoCORE::QueryCoreInterface(const MFX_GUID &guid)

#if 0
mfxStatus
VDAAPIVideoCORE::TraceFrames(
                            mfxFrameAllocRequest* request,
                            mfxFrameAllocResponse* response,
                            mfxStatus sts)
{
    return sts;
}

mfxStatus
VDAAPIVideoCORE::AllocFrames(
                            mfxFrameAllocRequest* request,
                            mfxFrameAllocResponse* response)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    try
    {
        MFX_CHECK_NULL_PTR2(request, response);
        mfxStatus sts = MFX_ERR_NONE;
        
        // use common core for sw surface allocation
        if (request->Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            sts = CommonCORE::AllocFrames(request, response);
            return TraceFrames(request, response, sts);
        } else
        {
            // external allocator
            if (m_bSetExtFrameAlloc)
            {
                // Default allocator should be used if D3D manager is not set and internal D3D buffers are required
                if (!m_Display && request->Type & MFX_MEMTYPE_INTERNAL_FRAME)
                {
                    m_bUseExtAllocForHWFrames = false;
                    sts = DefaultAllocFrames(request, response);
                    MFX_CHECK_STS(sts);
                    
                    return TraceFrames(request, response, sts);
                }
                
                sts = (*m_FrameAllocator.frameAllocator.Alloc)(m_FrameAllocator.frameAllocator.pthis,request, response);
                
                // if external allocator cannot allocate d3d frames - use default memory allocator
                if (MFX_ERR_UNSUPPORTED == sts)
                {
                    // Default Allocator is used for internal memory allocation only
                    if (request->Type & MFX_MEMTYPE_EXTERNAL_FRAME)
                        return sts;
                    m_bUseExtAllocForHWFrames = false;
                    sts = DefaultAllocFrames(request, response);
                    MFX_CHECK_STS(sts);
                    
                    return TraceFrames(request, response, sts);
                }
                // let's create video accelerator
                else if (MFX_ERR_NONE == sts)
                {
                    // Checking for unsupported mode - external allocator exist but Device handle doesn't set
                    if (!m_Display)
                        return MFX_ERR_UNSUPPORTED;
                    
                    m_bUseExtAllocForHWFrames = true;
                    sts = ProcessRenderTargets(request, response, &m_FrameAllocator);
                    MFX_CHECK_STS(sts);
                    
                    return TraceFrames(request, response, sts);
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
                
                return TraceFrames(request, response, sts);
            }
        }
    }
    catch(...)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    
} // mfxStatus VDAAPIVideoCORE::AllocFrames(...)


mfxStatus
VDAAPIVideoCORE::DefaultAllocFrames(
                                   mfxFrameAllocRequest* request,
                                   mfxFrameAllocResponse* response)
{
    mfxStatus sts = MFX_ERR_NONE;
    
    if ((request->Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET)||
        (request->Type & MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET)) // SW - TBD !!!!!!!!!!!!!!
    {
        if (!m_Display)
            return MFX_ERR_NOT_INITIALIZED;
        
        mfxBaseWideFrameAllocator* pAlloc = GetAllocatorByReq(request->Type);
        // VPP, ENC, PAK can request frames for several times
        if (pAlloc && (request->Type & MFX_MEMTYPE_FROM_DECODE))
            return MFX_ERR_MEMORY_ALLOC;
        
        if (!pAlloc)
        {
            m_pcHWAlloc.reset(new mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator(request->Type, m_Display));
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
    
} // mfxStatus VDAAPIVideoCORE::DefaultAllocFrames(...)

#endif

bool IsHwMvcEncSupported()
{
    return false;
}

#endif //#if defined (MFX_VA_OSX)
