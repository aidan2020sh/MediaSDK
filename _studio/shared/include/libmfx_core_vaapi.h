//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_VA_LINUX)

#ifndef __LIBMFX_CORE__VAAPI_H__
#define __LIBMFX_CORE__VAAPI_H__

#include <memory>
#include "umc_structures.h"
#include "libmfx_core.h"
#include "libmfx_allocator_vaapi.h"
#include "libmfx_core_interface.h"

#include "mfx_platform_headers.h"

#include "va/va.h"
#include "vaapi_ext_interface.h"

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
#include "mfx_vpp_interface.h"
#endif

class CmCopyWrapper;

// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

namespace UMC
{
    class DXVA2Accelerator;
    class LinuxVideoAccelerator;
};


class VAAPIVideoCORE : public CommonCORE
{    
public:
    friend class FactoryCORE;
    class VAAPIAdapter : public VAAPIInterface
    {
    public:
        VAAPIAdapter(VAAPIVideoCORE *pVAAPICore):m_pVAAPICore(pVAAPICore)
        {
        };
    protected:
        VAAPIVideoCORE *m_pVAAPICore;
    
    };

    class CMEnabledCoreAdapter : public CMEnabledCoreInterface
    {
    public:
        CMEnabledCoreAdapter(VAAPIVideoCORE *pVAAPICore): m_pVAAPICore(pVAAPICore)
        {
        };
        virtual mfxStatus SetCmCopyStatus(bool enable)
        {
            return m_pVAAPICore->SetCmCopyStatus(enable);
        };
    protected:
        VAAPIVideoCORE *m_pVAAPICore;
    };

    virtual ~VAAPIVideoCORE();

    virtual mfxStatus     GetHandle(mfxHandleType type, mfxHDL *handle);
    virtual mfxStatus     SetHandle(mfxHandleType type, mfxHDL handle);

    virtual mfxStatus     AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, bool isNeedCopy = true);
    virtual void          GetVA(mfxHDL* phdl, mfxU16 type) 
    {
        (type & MFX_MEMTYPE_FROM_DECODE)?(*phdl = m_pVA.get()):(*phdl = 0);
    };
    // Get the current working adapter's number
    virtual mfxU32 GetAdapterNumber(void) {return m_adapterNum;}
    virtual eMFXPlatform  GetPlatformType() {return  MFX_PLATFORM_HARDWARE;}

    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *pDst, mfxFrameSurface1 *pSrc);
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *pDst, mfxU16 dstMemType, mfxFrameSurface1 *pSrc, mfxU16 srcMemType);

    mfxHDL * GetFastCompositingService();
    void SetOnFastCompositingService(void);
    bool IsFastCompositingEnabled(void) const;


    virtual eMFXHWType     GetHWType() { return m_HWType; }


    mfxStatus              CreateVA(mfxVideoParam * param, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, UMC::FrameAllocator *allocator);
    // to check HW capatbilities
    virtual mfxStatus IsGuidSupported(const GUID guid, mfxVideoParam *par, bool isEncoder = false);

    virtual eMFXVAType   GetVAType() const {return MFX_HW_VAAPI; };
    virtual void* QueryCoreInterface(const MFX_GUID &guid);

#if defined (MFX_ENABLE_VPP)&& !defined(MFX_RT)
    virtual void  GetVideoProcessing(mfxHDL* phdl)
    {
        *phdl = &m_vpp_hw_resmng;
    };
#endif
    mfxStatus  CreateVideoProcessing(mfxVideoParam * param);

    mfxStatus              GetVAService(VADisplay *pVADisplay);

    // this function should not be virtual
    mfxStatus SetCmCopyStatus(bool enable);

protected:
    VAAPIVideoCORE(const mfxU32 adapterNum, const mfxU32 numThreadsAvailable, const mfxSession session = NULL);
    virtual void           Close();
    virtual mfxStatus      DefaultAllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);

    mfxStatus              CreateVideoAccelerator(mfxVideoParam * param, int profile, int NumOfRenderTarget, _mfxPlatformVideoSurface *RenderTargets, UMC::FrameAllocator *allocator);
    mfxStatus              ProcessRenderTargets(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxBaseWideFrameAllocator* pAlloc);
    mfxStatus              TraceFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxStatus sts);
    mfxStatus              OnDeblockingInWinRegistry(mfxU32 codecId);

    void                   ReleaseHandle();
    s_ptr<UMC::LinuxVideoAccelerator, true> m_pVA;
    VADisplay                            m_Display;
    mfxHDL                               m_VAConfigHandle;
    mfxHDL                               m_VAContextHandle;
    bool                                 m_KeepVAState;

    const mfxU32                         m_adapterNum; // Ordinal number of adapter to work
    bool                                 m_bUseExtAllocForHWFrames;
    s_ptr<mfxDefaultAllocatorVAAPI::mfxWideHWFrameAllocator, true> m_pcHWAlloc;
    eMFXHWType                           m_HWType;

    bool                                 m_bCmCopy;
    bool                                 m_bCmCopyAllowed;
    s_ptr<CmCopyWrapper, true>           m_pCmCopy;

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    VPPHWResMng                          m_vpp_hw_resmng;
#endif

private:

    s_ptr<VAAPIAdapter, true>            m_pAdapter;
    s_ptr<CMEnabledCoreAdapter, true>    m_pCmAdapter;
};

class PointerProxy
{
    public:
        PointerProxy(void* p) { mp = p; }
        template<class T> operator T*() { return reinterpret_cast<T*>(mp); }
    private:
        void* mp;
};

inline bool IsSupported__VAEncMiscParameterPrivate(void)
{
#if !defined(MFX_VAAPI_UPSTREAM)
    return true;
#else
    return false;
#endif
}

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
inline bool IsSupported__VAHDCPEncryptionParameterBuffer(void)
{
#if defined(ANDROID) && !defined(MFX_VAAPI_UPSTREAM)
    return true;
#else
    return false;
#endif
}
#endif

#endif // __LIBMFX_CORE__VAAPI_H__
#endif // MFX_VA_LINUX
/* EOF */
