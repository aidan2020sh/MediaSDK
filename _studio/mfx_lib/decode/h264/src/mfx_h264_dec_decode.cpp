/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2014 Intel Corporation. All Rights Reserved.
//
*/

#include <algorithm> /* for std::find on Linux/Android */

#include "mfx_h264_dec_decode.h"

#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#include "mfx_common.h"
#include "mfx_common_decode_int.h"

#include "umc_h264_mfx_supplier.h"

// debug
#include "umc_h264_frame_list.h"
#include "vm_sys_info.h"
#include "mfx_enc_common.h"

#if defined(MFX_VA)
#include "umc_h264_va_supplier.h"
#include "umc_va_dxva2_protected.h"
#include "umc_va_linux_protected.h"
#include "umc_va_video_processing.h"
#endif

#if defined (MFX_VA_OSX)
#include "umc_h264_vda_supplier.h"
#endif

static inline mfxU32 CalculateAsyncDepth(eMFXPlatform platform, mfxVideoParam *par)
{
    mfxU32 asyncDepth = par->AsyncDepth;
    if (!asyncDepth)
    {
        asyncDepth = (platform == MFX_PLATFORM_SOFTWARE) ? vm_sys_info_get_cpu_num() : MFX_AUTO_ASYNC_DEPTH_VALUE;
    }

    return asyncDepth;
}

static inline mfxU32 CalculateNumThread(eMFXPlatform platform, mfxVideoParam *par)
{
    mfxU32 numThread = (MFX_PLATFORM_SOFTWARE == platform) ? vm_sys_info_get_cpu_num() : 1;
    if (!par->AsyncDepth)
        return numThread;

    return IPP_MIN(par->AsyncDepth, numThread);
}

static inline void mfx_memcpy(void * dst, size_t dstLen, void * src, size_t len)
{
    memcpy_s(dst, dstLen, src, len);
}

inline bool IsNeedToUseHWBuffering(eMFXHWType type)
{
    type;return false;
    //return (type != MFX_HW_LAKE); // eagle lake has own workaround!
}

struct ThreadTaskInfo
{
    mfxFrameSurface1 *surface_work;
    mfxFrameSurface1 *surface_out;
    mfxU32            taskID; // for task ordering
};

enum
{
    ENABLE_DELAYED_DISPLAY_MODE = 1
};

typedef std::vector<Ipp32u> ViewIDsList;

inline bool AddDependency(Ipp32u dependOnViewId, const ViewIDsList & targetList, ViewIDsList &dependencyList)
{
    ViewIDsList::const_iterator view_iter = std::find(targetList.begin(), targetList.end(), dependOnViewId);
    if (view_iter == targetList.end())
    {
        ViewIDsList::const_iterator view_iter = std::find(dependencyList.begin(), dependencyList.end(), dependOnViewId);
        if (view_iter == dependencyList.end())
        {
            dependencyList.push_back(dependOnViewId);
        }

        return true;
    }

    return false;
}

mfxStatus Dependency(mfxExtMVCSeqDesc * mvcPoints, ViewIDsList & targetList, ViewIDsList &dependencyList)
{
    for (mfxU32 i = 0; i < targetList.size(); ++i)
    {
        mfxU32 viewId = targetList[i];

        // find view dependency
        mfxMVCViewDependency * refInfo = 0;
        for (mfxU32 k = 0; k < mvcPoints->NumView; k++)
        {
            if (mvcPoints->View[k].ViewId == viewId)
            {
                refInfo = &mvcPoints->View[k];
                break;
            }
        }

        if (!refInfo)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        bool wasAdded = false;

        for (mfxU32 j = 0; j < refInfo->NumAnchorRefsL0; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->AnchorRefL0[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumAnchorRefsL1; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->AnchorRefL1[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumNonAnchorRefsL0; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->NonAnchorRefL0[j], targetList, dependencyList);
        }

        for (mfxU32 j = 0; j < refInfo->NumNonAnchorRefsL1; j++)
        {
            wasAdded = wasAdded || AddDependency(refInfo->NonAnchorRefL1[j], targetList, dependencyList);
        }

        if (wasAdded)
        {
            i = 0;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus Dependency(mfxExtMVCSeqDesc * mvcPoints, mfxExtMVCTargetViews * targetViews, ViewIDsList &targetList, ViewIDsList &dependencyList)
{
    targetList.reserve(targetViews->NumView);
    for (Ipp32u i = 0; i < targetViews->NumView; i++)
    {
        targetList.push_back(targetViews->ViewId[i]);
    }

    mfxStatus sts = Dependency(mvcPoints, targetList, dependencyList);
    if (sts < MFX_ERR_NONE)
        return sts;

    sts = Dependency(mvcPoints, dependencyList, dependencyList);
    if (sts < MFX_ERR_NONE)
        return sts;

    return MFX_ERR_NONE;
}

mfxU32 CalculateRequiredView(mfxVideoParam *par)
{
    if (!IsMVCProfile(par->mfx.CodecProfile))
        return 1;

    mfxExtMVCSeqDesc * mvcPoints = (mfxExtMVCSeqDesc *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (!mvcPoints)
        return 1;

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

    ViewIDsList viewList;
    ViewIDsList dependencyList;

    if (!targetViews)
    {
        return mvcPoints->NumView;
    }

    mfxStatus sts = Dependency(mvcPoints, targetViews, viewList, dependencyList);
    if (sts < MFX_ERR_NONE)
        return 1;

    return (mfxU32)(viewList.size() + dependencyList.size());
}

VideoDECODEH264::VideoDECODEH264(VideoCORE *core, mfxStatus * sts)
    : VideoDECODE()
    , m_core(core)
    , m_isInit(false)
    , m_isOpaq(false)
    , m_frameOrder((mfxU16)MFX_FRAMEORDER_UNKNOWN)
    , m_platform(MFX_PLATFORM_SOFTWARE)
    , m_useDelayedDisplay(false)
    , m_va(0)
    , m_globalTask(false)
    , m_isFirstRun(true)
{
    memset(&m_stat, 0, sizeof(m_stat));

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEH264::~VideoDECODEH264(void)
{
    Close();
}

mfxStatus VideoDECODEH264::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(par);

    m_platform = MFX_Utility::GetPlatform(m_core, par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_Utility::CheckVideoParam(par, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_core->GetVAType() == MFX_HW_VAAPI)
    {
        int codecProfile = par->mfx.CodecProfile & 0xFF;
        if (    (codecProfile == MFX_PROFILE_AVC_STEREO_HIGH) ||
                (codecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH) ||
                (codecProfile == MFX_PROFILE_AVC_SCALABLE_BASELINE) ||
                (codecProfile == MFX_PROFILE_AVC_SCALABLE_HIGH) )
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_vInitPar = *par;
    m_vFirstPar = *par;
    m_vFirstPar.mfx.NumThread = 0;

    m_usePostProcessing = false;

    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        m_usePostProcessing = true;
    }

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);

    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    mfxU32 asyncDepth = CalculateAsyncDepth(m_platform, par);
    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(m_platform, par);

    if (MFX_PLATFORM_SOFTWARE == m_platform)
    {
        m_pH264VideoDecoder.reset(new UMC::MFXTaskSupplier());
        
        if (m_vPar.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422)
            m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_NV12());
        else
            m_FrameAllocator.reset(new mfx_UMC_FrameAllocator());
    }
    else
    {
        m_useDelayedDisplay = ENABLE_DELAYED_DISPLAY_MODE != 0 && IsNeedToUseHWBuffering(m_core->GetHWType()) && (asyncDepth != 1);

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
        m_pH264VideoDecoder.reset(new UMC::VATaskSupplier()); // HW
        m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_D3D());
#elif defined(MFX_VA_OSX)
        m_pH264VideoDecoder.reset(new UMC::VDATaskSupplier()); // HW, OS X VDA version
        m_FrameAllocator.reset(new mfx_UMC_FrameAllocator);
#else
        return MFX_ERR_UNSUPPORTED;
#endif
    }

    Ipp32s useInternal = (MFX_PLATFORM_SOFTWARE == m_platform) ?
        (m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (m_vPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (m_usePostProcessing)
        useInternal = 1;

#if defined (MFX_VA_OSX)
    useInternal = m_vPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY;    //APPLE - forcing this to act like software version
#endif

    if (m_vPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc *pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (!pOpaqAlloc)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        useInternal = (m_platform == MFX_PLATFORM_SOFTWARE) ? !(pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) : (pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
    }

    // allocate memory
    mfxFrameAllocRequest request;
    mfxFrameAllocRequest request_internal;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_alien, 0, sizeof(m_response_alien));
    m_isOpaq = false;

    mfxStatus mfxSts = QueryIOSurfInternal(m_platform, type, &m_vPar, &request);
    if (mfxSts != MFX_ERR_NONE)
        return mfxSts;

    mfxU16 request_type = request.Type;
    request.Type |= useInternal ? MFX_MEMTYPE_INTERNAL_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;
    request_internal = request;

    // allocates external surfaces:
    bool mapOpaq = true;
    mfxExtOpaqueSurfaceAlloc *pOpqAlloc = 0;
    mfxSts = UpdateAllocRequest(par, &request, pOpqAlloc, mapOpaq);
    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    if (m_isOpaq && !m_core->IsCompatibleForOpaq())
        return MFX_ERR_UNDEFINED_BEHAVIOR;


    if (mapOpaq)
    {
        mfxSts = m_core->AllocFrames(&request,
                                      &m_response,
                                      pOpqAlloc->Out.Surfaces,
                                      pOpqAlloc->Out.NumSurface);
    }
#if !defined (MFX_VA_OSX)
    else
    {
        if (m_platform != MFX_PLATFORM_SOFTWARE && !useInternal)
        {
            mfxSts = m_core->AllocFrames(&request, &m_response, false);
        }
    }
#endif

    if (mfxSts < MFX_ERR_NONE)
        return mfxSts;

    // allocates internal surfaces:
    if (useInternal)
    {
        m_response_alien = m_response;
        m_FrameAllocator->SetExternalFramesResponse(&m_response_alien);
        request = request_internal;
        mfxSts = m_core->AllocFrames(&request_internal, &m_response, true);
        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;

        if (m_usePostProcessing)
        {
            // need to substitute output format
            // number of surfaces is same
            request.Info.FourCC = videoProcessing->Out.FourCC;

            request.Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
            request.Info.PicStruct = videoProcessing->Out.PicStruct;
            request.Info.Width = videoProcessing->Out.Width;
            request.Info.Height = videoProcessing->Out.Height;
            request.Info.CropX = videoProcessing->Out.CropX;
            request.Info.CropY = videoProcessing->Out.CropY;
            request.Info.CropW = videoProcessing->Out.CropW;
            request.Info.CropH = videoProcessing->Out.CropH;
            request.Type = request_type;
            request.Type |= MFX_MEMTYPE_EXTERNAL_FRAME;

            mfxSts = m_core->AllocFrames(&request_internal, &m_response, true);
            if (mfxSts < MFX_ERR_NONE)
                return mfxSts;
        }
    }
    else
    {
        m_FrameAllocator->SetExternalFramesResponse(&m_response);
    }

    mfxU16 oldProfile = m_vFirstPar.mfx.CodecProfile;
    m_vFirstPar.mfx.CodecProfile = GetChangedProfile(&m_vFirstPar);

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if (m_platform != MFX_PLATFORM_SOFTWARE)
    {
        mfxSts = m_core->CreateVA(&m_vFirstPar, &request, &m_response);
        if (mfxSts < MFX_ERR_NONE)
            return mfxSts;
    }
#endif

#if defined(__APPLE__)
    UMC::Status umcSts = m_FrameAllocator->InitMfx(0, m_core, &m_vFirstPar, &request, &m_response, !useInternal, true);
#else
    UMC::Status umcSts = m_FrameAllocator->InitMfx(0, m_core, &m_vFirstPar, &request, &m_response, !useInternal, m_platform == MFX_PLATFORM_SOFTWARE);
#endif

    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

    if (m_usePostProcessing)
    {
        m_FrameAllocator->SetDoNotNeedToCopyFlag(true);
    }

    umcSts = m_MemoryAllocator.InitMem(0, m_core);
    if (umcSts != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

    m_pH264VideoDecoder->SetFrameAllocator(m_FrameAllocator.get());

    UMC::H264VideoDecoderParams umcVideoParams;
    ConvertMFXParamsToUMC(&m_vFirstPar, &umcVideoParams);
    umcVideoParams.numThreads = m_vPar.mfx.NumThread;
    umcVideoParams.m_bufferedFrames = IPP_MAX(asyncDepth - umcVideoParams.numThreads, 0);

#if defined(MFX_VA)
    if (MFX_PLATFORM_SOFTWARE != m_platform)
    {
        m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);
        umcVideoParams.pVideoAccelerator = m_va;
        static_cast<UMC::VATaskSupplier*>(m_pH264VideoDecoder.get())->SetVideoHardwareAccelerator(m_va);
#if defined(MFX_VA_WIN) || defined (MFX_VA_LINUX)
        if (m_va->GetProtectedVA())
        {
            if (m_va->GetProtectedVA()->SetModes(par) != UMC::UMC_OK)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (m_va->GetVideoProcessingVA())
        {
            if (m_va->GetVideoProcessingVA()->Init(par, videoProcessing) != UMC::UMC_OK)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
#endif
    }
#endif

    umcVideoParams.lpMemoryAllocator = &m_MemoryAllocator;

    umcSts = m_pH264VideoDecoder->Init(&umcVideoParams);
    if (umcSts != UMC::UMC_OK)
    {
        return ConvertUMCStatusToMfx(umcSts);
    }

    m_vFirstPar.mfx.CodecProfile = oldProfile;
    SetTargetViewList(&m_vFirstPar);

    m_isInit = true;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_globalTask = false;
    m_isFirstRun = true;

#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if (MFX_PLATFORM_SOFTWARE != m_platform && m_useDelayedDisplay)
    {
        ((UMC::VATaskSupplier*)m_pH264VideoDecoder.get())->SetBufferedFramesNumber(NUMBER_OF_ADDITIONAL_FRAMES);
    }
#endif

    m_pH264VideoDecoder->SetVideoParams(&m_vFirstPar);

    if (m_platform != m_core->GetPlatformType())
    {
        VM_ASSERT(m_platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxU16 VideoDECODEH264::GetChangedProfile(mfxVideoParam *par)
{
    if (IsSVCProfile(par->mfx.CodecProfile))
    {
        mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        if (svcTarget && !svcTarget->TargetDependencyID && !svcTarget->TargetQualityID)
        {
            return MFX_PROFILE_AVC_HIGH;
        }
    }

    if (!IsMVCProfile(par->mfx.CodecProfile))
        return par->mfx.CodecProfile;

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    if (targetViews && targetViews->NumView == 1 && targetViews->ViewId[0] == 0)
    {
        return MFX_PROFILE_AVC_HIGH;
    }

    return par->mfx.CodecProfile;
}

mfxStatus VideoDECODEH264::SetTargetViewList(mfxVideoParam *par)
{
    if (IsSVCProfile(par->mfx.CodecProfile))
    {
        mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        if (svcTarget)
        {
            m_pH264VideoDecoder->SetSVCTargetLayer(svcTarget->TargetDependencyID, svcTarget->TargetQualityID, svcTarget->TargetTemporalID);
        }
        else
        {
            mfxExtSVCSeqDesc * svcDesc = (mfxExtSVCSeqDesc*)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
            if (svcDesc)
            {
                mfxU32 maxDependencyId = 0;
                for (size_t i = 0; i < sizeof(svcDesc->DependencyLayer)/sizeof(svcDesc->DependencyLayer[0]); i++)
                {
                    if (svcDesc->DependencyLayer[i].Active)
                        maxDependencyId = (mfxU32)i;
                }

                m_pH264VideoDecoder->SetSVCTargetLayer(maxDependencyId, UMC::H264_MAX_QUALITY_ID, UMC::H264_MAX_TEMPORAL_ID);
            }
        }
    }

    ViewIDsList viewList;
    ViewIDsList dependencyList;

    mfxExtMVCSeqDesc * mvcPoints = (mfxExtMVCSeqDesc *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (!mvcPoints)
    {
        viewList.push_back(0); // base view only
        m_pH264VideoDecoder->SetViewList(viewList, dependencyList);
        return MFX_ERR_NONE;
    }

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

    if (targetViews)
    {
        mfxStatus sts = Dependency(mvcPoints, targetViews, viewList, dependencyList);
        if (sts < MFX_ERR_NONE)
            return sts;

        m_pH264VideoDecoder->SetTemporalId(targetViews->TemporalId);
    }

    m_pH264VideoDecoder->SetViewList(viewList, dependencyList);
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::Reset(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    eMFXPlatform platform = MFX_Utility::GetPlatform(m_core, par);

    if (CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_Utility::CheckVideoParam(par, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!IsSameVideoParam(par, &m_vInitPar, type))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // need to sw acceleration
    if (m_platform != platform)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    m_pH264VideoDecoder->Reset();

    SetTargetViewList(par);

    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_globalTask = false;
    m_isFirstRun = true;

    memset(&m_stat, 0, sizeof(m_stat));
    m_vFirstPar = *par;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vFirstPar);
    m_vPar = m_vFirstPar;
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    m_vPar.CreateExtendedBuffer(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    m_vPar.mfx.NumThread = (mfxU16)CalculateNumThread(m_platform, par);

    if (IS_PROTECTION_ANY(m_vFirstPar.Protected))
    {
#if defined (MFX_VA_WIN) || defined (MFX_VA_LINUX)
        if (m_va->GetProtectedVA())
        {
            if (m_va->GetProtectedVA()->SetModes(par) != UMC::UMC_OK)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
#endif
    }

    m_pH264VideoDecoder->SetVideoParams(&m_vFirstPar);

    if (m_platform != m_core->GetPlatformType())
    {
        VM_ASSERT(m_platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::Close(void)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit || !m_pH264VideoDecoder.get())
        return MFX_ERR_NOT_INITIALIZED;

    m_pH264VideoDecoder->Close();
    m_FrameAllocator->Close();

    if (m_response.NumFrameActual)
        m_core->FreeFrames(&m_response);

    if (m_response_alien.NumFrameActual)
        m_core->FreeFrames(&m_response_alien);

    m_isOpaq = false;
    m_isInit = false;
    m_isFirstRun = true;
    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_va = 0;
    memset(&m_stat, 0, sizeof(m_stat));
    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEH264::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_SHARED;//(m_platform == MFX_PLATFORM_SOFTWARE) ? MFX_TASK_THREADING_SHARED : MFX_TASK_THREADING_DEFAULT;
}

mfxStatus VideoDECODEH264::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out);

    eMFXPlatform platform = MFX_Utility::GetPlatform(core, in);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    return MFX_Utility::Query(core, in, out, type);
}

mfxStatus VideoDECODEH264::GetVideoParam(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    FillVideoParam(&m_vPar, true);

    par->mfx = m_vPar.mfx;

    par->Protected = m_vPar.Protected;
    par->IOPattern = m_vPar.IOPattern;
    par->AsyncDepth = m_vPar.AsyncDepth;

    mfxExtPAVPOption * buffer = (mfxExtPAVPOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
    if (buffer)
    {
        if (!IS_PROTECTION_PAVP_ANY(m_vPar.Protected))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        mfxExtPAVPOption * bufferInternal = m_vPar.GetExtendedBuffer<mfxExtPAVPOption>(MFX_EXTBUFF_PAVP_OPTION);
        *buffer = *bufferInternal;
    }

    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        mfxExtVideoSignalInfo * videoSignalInternal = m_vPar.GetExtendedBuffer<mfxExtVideoSignalInfo>(MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
        *videoSignal = *videoSignalInternal;
    }

    // mvc headers
    mfxExtMVCSeqDesc * mvcSeqDesc = (mfxExtMVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    mfxExtMVCSeqDesc * mvcSeqDescInternal = (mfxExtMVCSeqDesc *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (mvcSeqDesc && mvcSeqDescInternal && mvcSeqDescInternal->NumView)
    {
        mvcSeqDesc->NumView = mvcSeqDescInternal->NumView;
        mvcSeqDesc->NumViewId = mvcSeqDescInternal->NumViewId;
        mvcSeqDesc->NumOP = mvcSeqDescInternal->NumOP;

        if (mvcSeqDesc->NumViewAlloc < mvcSeqDescInternal->NumView ||
            mvcSeqDesc->NumViewIdAlloc < mvcSeqDescInternal->NumViewId ||
            mvcSeqDesc->NumOPAlloc < mvcSeqDescInternal->NumOP)
        {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }

        mfx_memcpy(mvcSeqDesc->View, mvcSeqDesc->NumView * sizeof(mfxMVCViewDependency), mvcSeqDescInternal->View, mvcSeqDescInternal->NumView * sizeof(mfxMVCViewDependency));
        mfx_memcpy(mvcSeqDesc->ViewId, mvcSeqDesc->NumViewId * sizeof(mfxU16), mvcSeqDescInternal->ViewId, mvcSeqDescInternal->NumViewId * sizeof(mfxU16));
        mfx_memcpy(mvcSeqDesc->OP, mvcSeqDesc->NumOP * sizeof(mfxMVCOperationPoint), mvcSeqDescInternal->OP, mvcSeqDescInternal->NumOP * sizeof(mfxMVCOperationPoint));

        mfxU16 * targetView = mvcSeqDesc->ViewId;
        for (mfxU32 i = 0; i < mvcSeqDesc->NumOP; i++)
        {
            mvcSeqDesc->OP[i].TargetViewId = targetView;
            targetView += mvcSeqDesc->OP[i].NumTargetViews;
        }
    }

    mfxExtMVCTargetViews * mvcTarget = (mfxExtMVCTargetViews *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    mfxExtMVCTargetViews * mvcTargetInternal = (mfxExtMVCTargetViews *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    if (mvcTarget && mvcTargetInternal && mvcTargetInternal->NumView)
    {
        *mvcTarget = *mvcTargetInternal;
    }

    mfxExtSVCSeqDesc * svcSeqDesc = (mfxExtSVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    mfxExtSVCSeqDesc * svcSeqDescInternal = (mfxExtSVCSeqDesc *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    if (svcSeqDesc && svcSeqDescInternal)
    {
        *svcSeqDesc = *svcSeqDescInternal;
    }

    mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
    mfxExtSvcTargetLayer * svcTargetInternal = (mfxExtSvcTargetLayer *)GetExtendedBuffer(m_vPar.ExtParam, m_vPar.NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
    if (svcTarget && svcTargetInternal)
    {
        *svcTarget = *svcTargetInternal;
    }

    // sps/pps headers
    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        mfxExtCodingOptionSPSPPS * spsPpsInternal = m_vPar.GetExtendedBuffer<mfxExtCodingOptionSPSPPS>(MFX_EXTBUFF_CODING_OPTION_SPSPPS);

        spsPps->SPSId = spsPpsInternal->SPSId;
        spsPps->PPSId = spsPpsInternal->PPSId;

        if (spsPps->SPSBufSize < spsPpsInternal->SPSBufSize ||
            spsPps->PPSBufSize < spsPpsInternal->PPSBufSize)
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        spsPps->SPSBufSize = spsPpsInternal->SPSBufSize;
        spsPps->PPSBufSize = spsPpsInternal->PPSBufSize;

        mfx_memcpy(spsPps->SPSBuffer, spsPps->SPSBufSize, spsPpsInternal->SPSBuffer, spsPps->SPSBufSize);
        mfx_memcpy(spsPps->PPSBuffer, spsPps->PPSBufSize, spsPpsInternal->PPSBuffer, spsPps->PPSBufSize);
    }

    par->mfx.FrameInfo.FrameRateExtN = m_vFirstPar.mfx.FrameInfo.FrameRateExtN;
    par->mfx.FrameInfo.FrameRateExtD = m_vFirstPar.mfx.FrameInfo.FrameRateExtD;

    if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
    {
        par->mfx.FrameInfo.FrameRateExtD = m_vPar.mfx.FrameInfo.FrameRateExtD;
        par->mfx.FrameInfo.FrameRateExtN = m_vPar.mfx.FrameInfo.FrameRateExtN;

        if (!par->mfx.FrameInfo.FrameRateExtD && !par->mfx.FrameInfo.FrameRateExtN)
        {
            par->mfx.FrameInfo.FrameRateExtN = 30;
            par->mfx.FrameInfo.FrameRateExtD = 1;
        }
    }

    par->mfx.FrameInfo.AspectRatioW = m_vFirstPar.mfx.FrameInfo.AspectRatioW;
    par->mfx.FrameInfo.AspectRatioH = m_vFirstPar.mfx.FrameInfo.AspectRatioH;

    if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
    {
        par->mfx.FrameInfo.AspectRatioH = m_vPar.mfx.FrameInfo.AspectRatioH;
        par->mfx.FrameInfo.AspectRatioW = m_vPar.mfx.FrameInfo.AspectRatioW;

        if (!par->mfx.FrameInfo.AspectRatioH && !par->mfx.FrameInfo.AspectRatioW)
        {
            par->mfx.FrameInfo.AspectRatioH = 1;
            par->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus sts = CheckBitstream(bs);
    if (sts != MFX_ERR_NONE)
        return sts;

    MFXMediaDataAdapter in(bs);

    mfx_UMC_MemAllocator  tempAllocator;
    tempAllocator.InitMem(0, core);

    UMC::H264VideoDecoderParams avcInfo;
    avcInfo.m_pData = &in;

    MFX_AVC_Decoder decoder;

    decoder.SetMemoryAllocator(&tempAllocator);
    UMC::Status umcRes = MFX_Utility::DecodeHeader(&decoder, &avcInfo, bs, par);

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_ERR_MORE_DATA;

    if (umcRes != UMC::UMC_OK)
        return ConvertUMCStatusToMfx(umcRes);

    umcRes = MFX_Utility::FillVideoParamMVCEx(&decoder, par);
    if (umcRes != UMC::UMC_OK)
        return ConvertUMCStatusToMfx(umcRes);

    // sps/pps headers
    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        UMC::RawHeader *sps = decoder.GetSPS();
        UMC::RawHeader *pps = decoder.GetPPS();

        if (sps->GetSize())
        {
            // reserved spsPps->SPSId = (mfxU16)sps->GetID();
            if (spsPps->SPSBufSize < sps->GetSize())
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            spsPps->SPSBufSize = (mfxU16)sps->GetSize();
            mfx_memcpy(spsPps->SPSBuffer, spsPps->SPSBufSize, sps->GetPointer(), spsPps->SPSBufSize);
        }
        else
        {
            // reserved spsPps->SPSId = 0;
            spsPps->SPSBufSize = 0;
        }

        if (pps->GetSize())
        {
            // reserved spsPps->PPSId = (mfxU16)pps->GetID();
            if (spsPps->PPSBufSize < pps->GetSize())
                return MFX_ERR_NOT_ENOUGH_BUFFER;

            spsPps->PPSBufSize = (mfxU16)pps->GetSize();
            mfx_memcpy(spsPps->PPSBuffer, spsPps->PPSBufSize, pps->GetPointer(), spsPps->PPSBufSize);
        }
        else
        {
            // reserved spsPps->PPSId = 0;
            spsPps->PPSBufSize = 0;
        }
    }

    //eMFXPlatform platform = MFX_Utility::GetPlatform(core, par);
    //if (platform != core->GetPlatformType())
    //{
    //    VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
    //    return MFX_WRN_PARTIAL_ACCELERATION;
    //}

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);

    eMFXPlatform platform = MFX_Utility::GetPlatform(core, par);
#if defined(__APPLE__)
    platform = MFX_PLATFORM_SOFTWARE;
#endif

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    mfxVideoParam params;
    params = *par;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    Ipp32s isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    mfxStatus sts = QueryIOSurfInternal(platform, type, &params, request);
    if (sts != MFX_ERR_NONE)
        return sts;

    bool isVideoProcessing = GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING) != 0;

    if (isInternalManaging || isVideoProcessing)
    {
        request->NumFrameSuggested = request->NumFrameMin = (mfxU16)CalculateAsyncDepth(platform, par);

        if (!isVideoProcessing) // need to swap mem type
        {
            if (MFX_PLATFORM_SOFTWARE == platform)
                request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
            else
                request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        request->Type |= MFX_MEMTYPE_OPAQUE_FRAME;
    }
    else
    {
        request->Type |= MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
    if (videoProcessing)
    {
        // need to substitute output format
        // number of surfaces is same
        request->Info.FourCC = videoProcessing->Out.FourCC;

        request->Info.ChromaFormat = videoProcessing->Out.ChromaFormat;
        request->Info.PicStruct = videoProcessing->Out.PicStruct;
        request->Info.Width = videoProcessing->Out.Width;
        request->Info.Height = videoProcessing->Out.Height;
        request->Info.CropX = videoProcessing->Out.CropX;
        request->Info.CropY = videoProcessing->Out.CropY;
        request->Info.CropW = videoProcessing->Out.CropW;
        request->Info.CropH = videoProcessing->Out.CropH;
    }

    if (platform != core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::QueryIOSurfInternal(eMFXPlatform platform, eMFXHWType type, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    request->Info = par->mfx.FrameInfo;

    mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc *)GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    mfxU8 level_idc = (mfxU8)par->mfx.CodecLevel;

    if (IsMVCProfile(par->mfx.CodecProfile) && points && points->OP)
    {
        level_idc = mfxU8 (IPP_MAX(level_idc, points->OP->LevelIdc));
    }

    mfxU32 asyncDepth = CalculateAsyncDepth(platform, par);
    bool useDelayedDisplay = (ENABLE_DELAYED_DISPLAY_MODE != 0) && IsNeedToUseHWBuffering(type) && (asyncDepth != 1);

    mfxI32 dpbSize = UMC::CalculateDPBSize(level_idc, par->mfx.FrameInfo.Width, par->mfx.FrameInfo.Height, 0);
    mfxU32 numMin = dpbSize + 1 + asyncDepth;
    if (platform != MFX_PLATFORM_SOFTWARE && useDelayedDisplay) // equals if (m_useDelayedDisplay) - workaround
        numMin += NUMBER_OF_ADDITIONAL_FRAMES;
    numMin *= CalculateRequiredView(par);

    numMin *= IsSVCProfile(par->mfx.CodecProfile) ? 2 : 1;

    request->NumFrameMin = (mfxU16)numMin;

    request->NumFrameSuggested = request->NumFrameMin;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::GetDecodeStat(mfxDecodeStat *stat)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = m_pH264VideoDecoder->GetSkipInfo().numberOfSkippedFrames;
    m_stat.NumCachedFrame = 0;

    H264DBPList * lst = m_pH264VideoDecoder->GetDPBList(UMC::BASE_VIEW, 0);
    if (lst)
    {
        UMC::H264DecoderFrame *pFrame = lst->head();
        for (; pFrame; pFrame = pFrame->future())
        {
            if (!pFrame->wasOutputted() && !UMC::isAlmostDisposable(pFrame))
                m_stat.NumCachedFrame++;
        }
    }

    m_stat.reserved[0] = m_pH264VideoDecoder->IsExistHeadersError() ? 1 : 0;

    *stat = m_stat;
    return MFX_ERR_NONE;
}

static mfxStatus __CDECL AVCDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 )
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "DecodeFrame_AVC");
    VideoDECODEH264 *decoder = (VideoDECODEH264 *)pState;

    mfxStatus sts = decoder->RunThread(pParam, threadNumber);
    return sts;
}

static mfxStatus AVCCompleteProc(void *, void *pParam, mfxStatus )
{
    delete (ThreadTaskInfo *)pParam;    // NOT SAFE !!!
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEH264::RunThread(void * params, mfxU32 threadNumber)
{
    ThreadTaskInfo * info = (ThreadTaskInfo *)params;

    mfxStatus sts = MFX_TASK_WORKING;

    if (!info->surface_out)
    {
        for (Ipp32s i = 0; i < 2 && sts == MFX_TASK_WORKING; i++)
        {
            sts = m_pH264VideoDecoder->RunThread(threadNumber);
        }

        if (sts == MFX_TASK_BUSY && !m_pH264VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true))
            m_globalTask = false;

        return m_globalTask ? sts : MFX_TASK_DONE;
    }

    UMC::H264DecoderFrame * pFrame = 0;
    bool isDecoded;
    {
        UMC::AutomaticUMCMutex guard(m_mGuard);

        if (!info->surface_work)
            return MFX_TASK_DONE;

        mfxI32 index = m_FrameAllocator->FindSurface(info->surface_out, m_isOpaq);
        pFrame = m_pH264VideoDecoder->FindSurface((UMC::FrameMemID)index);

        if (!pFrame || pFrame->m_UID == -1)
        {
            VM_ASSERT(false);
            return MFX_ERR_NOT_FOUND;
        }

        isDecoded = m_pH264VideoDecoder->CheckDecoding(true, pFrame);
    }

    if (!isDecoded)
    {
        for (Ipp32s i = 0; i < 2 && sts == MFX_TASK_WORKING; i++)
        {
            sts = m_pH264VideoDecoder->RunThread(threadNumber);
        }
    }

    {
        UMC::AutomaticUMCMutex guard(m_mGuard);
        if (!info->surface_work)
            return MFX_TASK_DONE;

        isDecoded = m_pH264VideoDecoder->CheckDecoding(true, pFrame);
        if (isDecoded)
        {
            info->surface_work = 0;
            pFrame->m_UID = -1;
        }
    }

    if (isDecoded)
    {
        if (!pFrame->wasDisplayed())
        {
            mfxStatus sts = DecodeFrame(0, info->surface_work, info->surface_out);

            if (sts != MFX_ERR_NONE && sts != MFX_ERR_NOT_FOUND)
                return sts;
        }

        return MFX_TASK_DONE;
    }

    return sts;
}

mfxStatus VideoDECODEH264::DecodeFrameCheck(mfxBitstream *bs,
                                              mfxFrameSurface1 *surface_work,
                                              mfxFrameSurface1 **surface_out,
                                              MFX_ENTRY_POINT *pEntryPoint)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    mfxStatus mfxSts = DecodeFrameCheck(bs, surface_work, surface_out);

    if (MFX_ERR_NONE == mfxSts) // It can be useful to run threads right after first frame receive
    {
        ThreadTaskInfo * info = new ThreadTaskInfo();
        info->surface_work = GetOriginalSurface(surface_work);
        info->surface_out = GetOriginalSurface(*surface_out);

        pEntryPoint->pRoutine = &AVCDECODERoutine;
        pEntryPoint->pCompleteProc = &AVCCompleteProc;
        pEntryPoint->pState = this;
        pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
        pEntryPoint->pParam = info;
    }
    else
    {
        if (m_pH264VideoDecoder->GetTaskBroker()->IsEnoughForStartDecoding(true) && !m_globalTask)
        {
            ThreadTaskInfo * info = new ThreadTaskInfo();
            info->surface_work = 0;
            info->surface_out = 0;

            pEntryPoint->pRoutine = &AVCDECODERoutine;
            pEntryPoint->pCompleteProc = &AVCCompleteProc;
            pEntryPoint->pState = this;
            pEntryPoint->requiredNumThreads = m_vPar.mfx.NumThread;
            pEntryPoint->pParam = info;

            m_globalTask = true;
        }
    }

    return mfxSts;
}

mfxStatus VideoDECODEH264::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    mfxStatus sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    if (sts != MFX_ERR_NONE)
        return sts;

    UMC::Status umcRes = UMC::UMC_OK;

    *surface_out = 0;

    if (m_isOpaq)
    {
        sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_AVC);
        if (sts != MFX_ERR_NONE)
            return MFX_ERR_UNSUPPORTED;

        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        surface_work = GetOriginalSurface(surface_work);
    }

    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_AVC);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    sts = CheckFrameData(surface_work);
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_isOpaq);
    if (sts != MFX_ERR_NONE)
        return sts;

    sts = MFX_ERR_UNDEFINED_BEHAVIOR;

#if defined(MFX_VA_WIN) || defined (MFX_VA_LINUX)
    if (m_vPar.Protected && bs)
    {
        if (!m_va->GetProtectedVA() || !(bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_va->GetProtectedVA()->SetBitstream(bs);
    }

    if (m_usePostProcessing)
    {
        mfxHDL surfHDL;
        m_core->GetExternalFrameHDL(surface_work->Data.MemId, &surfHDL, false);
        m_va->GetVideoProcessingVA()->SetOutputSurface(surfHDL);
    }
#endif

    try
    {
        bool force = false;

        UMC::Status umcFrameRes = UMC::UMC_OK;
        UMC::Status umcAddSourceRes = UMC::UMC_OK;

        MFXMediaDataAdapter src(bs);

        for (;;)
        {
            if (m_FrameAllocator->FindFreeSurface() == -1)
            {
                umcRes = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }
            else
            {
                umcRes = m_pH264VideoDecoder->AddSource(bs ? &src : 0);
            }

            umcAddSourceRes = umcFrameRes = umcRes;

            //if (umcRes == MFX_ERR_DEVICE_LOST)
              //  return MFX_ERR_DEVICE_LOST;

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION || umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS || umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                FillVideoParam(&m_vPar, true);
            }

            if (umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS)
            {
                if (!m_isFirstRun)
                {
                    sts = MFX_WRN_VIDEO_PARAM_CHANGED;
                }
                else
                {
                    umcAddSourceRes = umcFrameRes = umcRes = UMC::UMC_OK;
                    m_isFirstRun = false;
                }
            }

            if (umcRes == UMC::UMC_ERR_INVALID_STREAM)
            {
                umcAddSourceRes = umcFrameRes = umcRes = UMC::UMC_OK;
            }

            if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION)
            {
                sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }

            if (umcRes == UMC::UMC_OK && m_FrameAllocator->FindFreeSurface() == -1)
            {
                sts = MFX_ERR_MORE_SURFACE;
                umcFrameRes = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY)
            {
                force = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
                sts = MFX_WRN_DEVICE_BUSY;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if (!bs)
                    force = true;
                sts = MFX_ERR_MORE_DATA;
            }

            src.Save(bs);

            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                return sts;

            umcRes = m_pH264VideoDecoder->RunDecoding();

            if (m_vInitPar.mfx.DecodedOrder)
                force = true;

            UMC::H264DecoderFrame *pFrame = GetFrameToDisplay(force);

            // return frame to display
            if (pFrame)
            {
                FillOutputSurface(surface_out, surface_work, pFrame);

                m_frameOrder = (mfxU16)pFrame->m_frameOrder;
                (*surface_out)->Data.FrameOrder = m_frameOrder;
                return MFX_ERR_NONE;
            }

            *surface_out = 0;

            if (umcFrameRes != UMC::UMC_OK)
                break;
        } // for (;;)
    }
    catch(const UMC::h264_exception & ex)
    {
        FillVideoParam(&m_vPar, false);

        if (ex.GetStatus() == UMC::UMC_ERR_ALLOC)
        {
            // check incompatibility of video params
            if (m_vInitPar.mfx.FrameInfo.Width != m_vPar.mfx.FrameInfo.Width ||
                m_vInitPar.mfx.FrameInfo.Height != m_vPar.mfx.FrameInfo.Height)
            {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }
        }

        return ConvertUMCStatusToMfx(ex.GetStatus());
    }
    catch(...)
    {
        return MFX_ERR_UNKNOWN;
    }

    return sts;
}

void VideoDECODEH264::FillVideoParam(mfxVideoParamWrapper *par, bool full)
{
    if (!m_pH264VideoDecoder.get())
        return;

    MFX_Utility::FillVideoParam(m_pH264VideoDecoder.get(), par, full);

    UMC::RawHeader *sps = m_pH264VideoDecoder->GetSPS();
    UMC::RawHeader *pps = m_pH264VideoDecoder->GetPPS();

    mfxExtCodingOptionSPSPPS * spsPps = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (spsPps)
    {
        if (sps->GetSize())
        {
            // reserved spsPps->SPSId = (mfxU16)sps->GetID();
            spsPps->SPSBufSize = (mfxU16)sps->GetSize();
            spsPps->SPSBuffer = sps->GetPointer();
        }
        else
        {
            // reserved spsPps->SPSId = 0;
            spsPps->SPSBufSize = 0;
        }

        if (pps->GetSize())
        {
            // reserved spsPps->PPSId = (mfxU16)pps->GetID();
            spsPps->PPSBufSize = (mfxU16)pps->GetSize();
            spsPps->PPSBuffer = pps->GetPointer();
        }
        else
        {
            // reserved spsPps->PPSId = 0;
            spsPps->PPSBufSize = 0;
        }
    }

    MFX_Utility::FillVideoParamMVCEx(m_pH264VideoDecoder.get(), par);
}

void VideoDECODEH264::CopySurfaceInfo(mfxFrameSurface1 *in, mfxFrameSurface1 *out)
{
    if (IsSVCProfile(m_vFirstPar.mfx.CodecProfile))
    {
        out->Info.FrameId.DependencyId = in->Info.FrameId.DependencyId;
        out->Info.FrameId.TemporalId = in->Info.FrameId.TemporalId;
        out->Info.FrameId.QualityId = in->Info.FrameId.QualityId;
        out->Info.FrameId.PriorityId = in->Info.FrameId.PriorityId;
    }
    else
    {
        out->Info.FrameId.ViewId = in->Info.FrameId.ViewId;
        out->Info.FrameId.TemporalId = in->Info.FrameId.TemporalId;
        out->Info.FrameId.PriorityId = in->Info.FrameId.PriorityId;
    }

    out->Info.CropH = in->Info.CropH;
    out->Info.CropW = in->Info.CropW;
    out->Info.CropX = in->Info.CropX;
    out->Info.CropY = in->Info.CropY;

    out->Info.AspectRatioH = in->Info.AspectRatioH;
    out->Info.AspectRatioW = in->Info.AspectRatioW;

    out->Info.FrameRateExtD = in->Info.FrameRateExtD;
    out->Info.FrameRateExtN = in->Info.FrameRateExtN;
    out->Info.PicStruct = in->Info.PicStruct;

    out->Data.TimeStamp = in->Data.TimeStamp;
    out->Data.FrameOrder = in->Data.FrameOrder;
    out->Data.Corrupted = in->Data.Corrupted;
    out->Data.DataFlag = in->Data.DataFlag;
}

void VideoDECODEH264::FillOutputSurface(mfxFrameSurface1 **surf_out, mfxFrameSurface1 *surface_work, UMC::H264DecoderFrame * pFrame)
{
    m_stat.NumFrame++;
    m_stat.NumError += pFrame->GetError() ? 1 : 0;
    const UMC::FrameData * fd = pFrame->GetFrameData();

    *surf_out = m_FrameAllocator->GetSurface(fd->GetFrameMID(), surface_work, &m_vPar);
    *surf_out = m_core->GetOpaqSurface((*surf_out)->Data.MemId) ? m_core->GetOpaqSurface((*surf_out)->Data.MemId) : (*surf_out);

    VM_ASSERT(*surf_out);

    mfxFrameSurface1 *surface_out = *surf_out;

    if (IsSVCProfile(m_vFirstPar.mfx.CodecProfile))
    {
        surface_out->Info.FrameId.DependencyId = (mfxU16)pFrame->m_maxDId;
        surface_out->Info.FrameId.QualityId = (mfxU16)pFrame->m_maxQId;
        surface_out->Info.FrameId.TemporalId = 0;
    }
    else
    {
        surface_out->Info.FrameId.ViewId = (mfxU16)pFrame->m_viewId;
        surface_out->Info.FrameId.TemporalId = 0;
    }

    surface_out->Info.CropH = (mfxU16)(pFrame->lumaSize().height - pFrame->m_crop_bottom - pFrame->m_crop_top);
    surface_out->Info.CropW = (mfxU16)(pFrame->lumaSize().width - pFrame->m_crop_right - pFrame->m_crop_left);
    surface_out->Info.CropX = (mfxU16)(pFrame->m_crop_left);
    surface_out->Info.CropY = (mfxU16)(pFrame->m_crop_top);

    if (m_usePostProcessing)
    {
        mfxExtDecVideoProcessing * videoProcessing = (mfxExtDecVideoProcessing *)GetExtBuffer(m_vFirstPar.ExtParam, m_vFirstPar.NumExtParam, MFX_EXTBUFF_DEC_VIDEO_PROCESSING);
        VM_ASSERT(videoProcessing);

        surface_out->Info.CropH = videoProcessing->Out.CropH;
        surface_out->Info.CropW = videoProcessing->Out.CropW;
        surface_out->Info.CropX = videoProcessing->Out.CropX;
        surface_out->Info.CropY = videoProcessing->Out.CropY;
    }

    bool isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.AspectRatioH || m_vFirstPar.mfx.FrameInfo.AspectRatioW);

    surface_out->Info.AspectRatioH = isShouldUpdate ? (mfxU16)pFrame->m_aspect_height : m_vFirstPar.mfx.FrameInfo.AspectRatioH;
    surface_out->Info.AspectRatioW = isShouldUpdate ? (mfxU16)pFrame->m_aspect_width : m_vFirstPar.mfx.FrameInfo.AspectRatioW;

    isShouldUpdate = !(m_vFirstPar.mfx.FrameInfo.FrameRateExtD || m_vFirstPar.mfx.FrameInfo.FrameRateExtN);

    surface_out->Info.FrameRateExtD = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtD : m_vFirstPar.mfx.FrameInfo.FrameRateExtD;
    surface_out->Info.FrameRateExtN = isShouldUpdate ? m_vPar.mfx.FrameInfo.FrameRateExtN : m_vFirstPar.mfx.FrameInfo.FrameRateExtN;

    surface_out->Info.PicStruct = 0;
    surface_out->Info.ChromaFormat = (mfxU16)(pFrame->m_chroma_format ? MFX_CHROMAFORMAT_YUV420 : MFX_CHROMAFORMAT_YUV400);

    switch (pFrame->m_displayPictureStruct)
    {
    case UMC::DPS_TOP:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
        break;

    case UMC::DPS_BOTTOM:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        break;

    case UMC::DPS_TOP_BOTTOM:
    case UMC::DPS_BOTTOM_TOP:
        {
            mfxU32 fieldFlag = (pFrame->m_displayPictureStruct == UMC::DPS_TOP_BOTTOM) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
            surface_out->Info.PicStruct = (mfxU16)((pFrame->m_PictureStructureForDec == UMC::FRM_STRUCTURE) ? MFX_PICSTRUCT_PROGRESSIVE : fieldFlag);

            if (m_vPar.mfx.ExtendedPicStruct)
            {
                surface_out->Info.PicStruct |= fieldFlag;
            }
        }
        break;

    case UMC::DPS_TOP_BOTTOM_TOP:
    case UMC::DPS_BOTTOM_TOP_BOTTOM:
        {
            surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

            if (m_vPar.mfx.ExtendedPicStruct)
            {
                surface_out->Info.PicStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
                surface_out->Info.PicStruct |= (pFrame->m_displayPictureStruct == UMC::DPS_TOP_BOTTOM_TOP) ? MFX_PICSTRUCT_FIELD_TFF : MFX_PICSTRUCT_FIELD_BFF;
            }
        }
        break;

    case UMC::DPS_FRAME_DOUBLING:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (m_vPar.mfx.ExtendedPicStruct)
        {
            surface_out->Info.PicStruct |= MFX_PICSTRUCT_FRAME_DOUBLING;
        }
        break;

    case UMC::DPS_FRAME_TRIPLING:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (m_vPar.mfx.ExtendedPicStruct)
        {
            surface_out->Info.PicStruct |= MFX_PICSTRUCT_FRAME_TRIPLING;
        }
        break;

    case UMC::DPS_FRAME:
    default:
        surface_out->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        break;
    }

    surface_out->Data.TimeStamp = GetMfxTimeStamp(pFrame->m_dFrameTime);
    surface_out->Data.FrameOrder = (mfxU32)MFX_FRAMEORDER_UNKNOWN;

    surface_out->Data.DataFlag = (mfxU16)(pFrame->m_isOriginalPTS ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);

    UMC::SEI_Storer * storer = m_pH264VideoDecoder->GetSEIStorer();
    if (storer)
        storer->SetTimestamp(pFrame);
}

mfxStatus VideoDECODEH264::DecodeFrame(mfxFrameSurface1 *surface_out, UMC::H264DecoderFrame * pFrame)
{
    MFX_CHECK_NULL_PTR1(surface_out);

    mfxI32 index;
    if (pFrame)
    {
        index = pFrame->GetFrameData()->GetFrameMID();
    }
    else
    {
        index = m_FrameAllocator->FindSurface(surface_out, m_isOpaq);
        pFrame = m_pH264VideoDecoder->FindSurface((UMC::FrameMemID)index);
        if (!pFrame)
        {
            VM_ASSERT(false);
            return MFX_ERR_NOT_FOUND;
        }
    }

    Ipp32s error = pFrame->GetError();

    surface_out->Data.Corrupted = 0;
    if (error & UMC::ERROR_FRAME_MINOR)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MINOR;

    if (error & UMC::ERROR_FRAME_MAJOR)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

    if (error & UMC::ERROR_FRAME_REFERENCE_FRAME)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_FRAME;

    if (error & UMC::ERROR_FRAME_DPB)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_REFERENCE_LIST;

    if (error & UMC::ERROR_FRAME_RECOVERY)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;

    if (error & UMC::ERROR_FRAME_TOP_FIELD_ABSENT)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_TOP_FIELD;

    if (error & UMC::ERROR_FRAME_BOTTOM_FIELD_ABSENT)
        surface_out->Data.Corrupted |= MFX_CORRUPTION_ABSENT_BOTTOM_FIELD;

    mfxStatus sts = m_FrameAllocator->PrepareToOutput(surface_out, index, &m_vPar, m_isOpaq);

    pFrame->setWasDisplayed();
    return sts;
}

mfxStatus VideoDECODEH264::DecodeFrame(mfxBitstream *, mfxFrameSurface1 *, mfxFrameSurface1 *surface_out)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(surface_out);
    mfxStatus sts = DecodeFrame(surface_out);

    return sts;
}

mfxStatus VideoDECODEH264::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    mfxStatus       MFXSts = MFX_ERR_NONE;

    UMC::MediaData data;
    UMC::Status umcRes = m_pH264VideoDecoder->GetUserData(&data);

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_ERR_MORE_DATA;

    if (*sz < data.GetDataSize())
        return MFX_ERR_NOT_ENOUGH_BUFFER;

    *sz = (mfxU32)data.GetDataSize();
    *ts = GetMfxTimeStamp(data.GetTime());
    mfx_memcpy(ud, *sz, data.GetDataPointer(), data.GetDataSize());

    return MFXSts;
}

mfxStatus VideoDECODEH264::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    UMC::SEI_Storer * storer = m_pH264VideoDecoder->GetSEIStorer();

    if (!storer)
        return MFX_ERR_UNKNOWN;

    const UMC::SEI_Storer::SEI_Message * msg = storer->GetPayloadMessage();

    if (msg)
    {
        if (payload->BufSize < msg->msg_size)
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        *ts = GetMfxTimeStamp(msg->timestamp);
        mfx_memcpy(payload->Data, payload->BufSize, msg->data, msg->msg_size);

        payload->NumBit = (mfxU32)(msg->msg_size * 8);
        payload->Type = (mfxU16)msg->type;
    }
    else
    {
        payload->NumBit = 0;
        *ts = MFX_TIME_STAMP_INVALID;
    }

    return MFX_ERR_NONE;
}

UMC::H264DecoderFrame * VideoDECODEH264::GetFrameToDisplay(bool force)
{
    UMC::H264DecoderFrame * pFrame = 0;
    do
    {
        pFrame = m_pH264VideoDecoder->GetFrameToDisplayInternal(force);
        if (!pFrame)
        {
            break;
        }

        m_pH264VideoDecoder->PostProcessDisplayFrame(pFrame);

        if (pFrame->IsSkipped())
        {
            pFrame->setWasOutputted();
            pFrame->setWasDisplayed();
        }
    } while (pFrame->IsSkipped());

    if (pFrame)
    {
        pFrame->setWasOutputted();
    }

    return pFrame;
}

mfxStatus VideoDECODEH264::SetSkipMode(mfxSkipMode mode)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    if (mode == 0x22)
    {
        m_pH264VideoDecoder->PermanentDisableDeblocking(true);
        return MFX_ERR_NONE;
    }

    if (mode == 0x23)
    {
#if defined MFX_VA_WIN
        UMC::VideoAccelerator *va;
        m_core->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);
        if (va)
            va->SetStatusReportUsing(false);
#endif
        return MFX_ERR_NONE;
    }

    Ipp32s test_num = 0;
    m_pH264VideoDecoder->ChangeVideoDecodingSpeed(test_num);

    Ipp32s num = 0;

    switch(mode)
    {
    case MFX_SKIPMODE_NOSKIP:
        num = -10;
        break;
    case MFX_SKIPMODE_MORE:
        num = 1;
        break;
    case MFX_SKIPMODE_LESS:
        num = -1;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    m_pH264VideoDecoder->ChangeVideoDecodingSpeed(num);

    return test_num == num ? MFX_WRN_VALUE_NOT_CHANGED : MFX_ERR_NONE;
}

bool VideoDECODEH264::IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar, eMFXHWType type)
{
    if ((newPar->IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)) !=
        (oldPar->IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)) )
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.FourCC != oldPar->mfx.FrameInfo.FourCC)
    {
        return false;
    }

    if (CalculateAsyncDepth(m_platform, newPar) != CalculateAsyncDepth(m_platform, oldPar))
    {
        return false;
    }

    mfxFrameAllocRequest requestOld;
    memset(&requestOld, 0, sizeof(requestOld));
    mfxFrameAllocRequest requestNew;
    memset(&requestNew, 0, sizeof(requestNew));

    mfxStatus mfxSts = QueryIOSurfInternal(m_platform, type, oldPar, &requestOld);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    mfxSts = QueryIOSurfInternal(m_platform, type, newPar, &requestNew);
    if (mfxSts != MFX_ERR_NONE)
        return false;

    if (newPar->mfx.FrameInfo.Height > oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width > oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (m_response.NumFrameActual)
    {
        if (requestNew.NumFrameMin > m_response.NumFrameActual)
            return false;
    }
    else
    {
        if (requestNew.NumFrameMin > requestOld.NumFrameMin || requestNew.Type != requestOld.Type)
            return false;
    }

    //420<->400 allowed, other chroma formats are unsupported
    /*if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        if ((newPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && newPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400) ||
            (oldPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && oldPar->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400))
        return false;
    }*/

    if (CalculateRequiredView(newPar) != CalculateRequiredView(oldPar))
        return false;

    if (oldPar->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc * opaqueNew = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc * opaqueOld = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (opaqueNew && opaqueOld)
        {
            if (opaqueNew->In.Type != opaqueOld->In.Type)
                return false;

            if (opaqueNew->In.NumSurface != opaqueOld->In.NumSurface)
                return false;

            for (Ipp32u i = 0; i < opaqueNew->In.NumSurface; i++)
            {
                if (opaqueNew->In.Surfaces[i] != opaqueOld->In.Surfaces[i])
                    return false;
            }

            if (opaqueNew->Out.Type != opaqueOld->Out.Type)
                return false;

            if (opaqueNew->Out.NumSurface != opaqueOld->Out.NumSurface)
                return false;

            for (Ipp32u i = 0; i < opaqueNew->Out.NumSurface; i++)
            {
                if (opaqueNew->Out.Surfaces[i] != opaqueOld->Out.Surfaces[i])
                    return false;
            }
        }
        else
            return false;
    }

#if 0
    // check target views or dependency views
    mfxExtMVCSeqDesc * newMVCPoints = (mfxExtMVCSeqDesc *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    mfxExtMVCSeqDesc * oldMVCPoints = (mfxExtMVCSeqDesc *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

    if ((!newMVCPoints && oldMVCPoints) || (newMVCPoints && !oldMVCPoints))
        return false;

    mfxExtMVCTargetViews * newMVCTargets = (mfxExtMVCTargetViews *)GetExtendedBuffer(newPar->ExtParam, newPar->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    mfxExtMVCTargetViews * oldMVCTargets = (mfxExtMVCTargetViews *)GetExtendedBuffer(oldPar->ExtParam, oldPar->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

    if (newMVCTargets && oldMVCTargets)
    {
        if (newMVCTargets->NumView != oldMVCTargets->NumView)
            return false;
    }
    else
    {
        if ((!newMVCTargets && oldMVCTargets) || (newMVCTargets && !oldMVCTargets))
            return false;
    }
#endif

    return true;
}

mfxStatus VideoDECODEH264::UpdateAllocRequest(mfxVideoParam *par,
                                                mfxFrameAllocRequest *request,
                                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                                bool &mapping)
{
    mapping = false;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_NONE;

    m_isOpaq = true;

    pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    if (!pOpaqAlloc)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (request->NumFrameMin > pOpaqAlloc->Out.NumSurface)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE;
    request->Type |= (pOpaqAlloc->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) ? MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    request->NumFrameMin = request->NumFrameSuggested = pOpaqAlloc->Out.NumSurface;
    mapping = true;
    return MFX_ERR_NONE;
}

mfxFrameSurface1 *VideoDECODEH264::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaq)
        return m_core->GetNativeSurface(surface);
    return surface;
}

#endif // MFX_ENABLE_H264_VIDEO_DECODE
