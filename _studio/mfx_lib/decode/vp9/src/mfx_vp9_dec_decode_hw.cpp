/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW) && defined(MFX_VA)

#include "mfx_session.h"
#include "mfx_vp9_dec_decode.h"
#include "mfx_vp9_dec_decode_hw.h"

#include "umc_va_dxva2.h"

#include "umc_vp9_utils.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_va_packer.h"

#if defined (_WIN32) || defined (_WIN64)
#pragma warning(disable : 4189)
#pragma warning(disable : 4101)
#include <iostream>
#include <dxva.h>
#endif

//#define VP9_STATUS_REPORTING_ENABLED //need to implement different code for Intel/MSFT guids, disable for now

using namespace UMC_VP9_DECODER;

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar);

inline
void mfx_memcpy(void * dst, size_t dstLen, void * src, size_t len)
{
    memcpy_s(dst, dstLen, src, len);
}

inline
bool CheckHardwareSupport(VideoCORE *p_core, mfxVideoParam *p_video_param)
{
#ifdef MFX_VA_WIN

    if (p_core->IsGuidSupported(DXVA_Intel_ModeVP9_Profile0_VLD, p_video_param) != MFX_ERR_NONE &&
        p_core->IsGuidSupported(DXVA_Intel_ModeVP9_Profile2_10bit_VLD, p_video_param) != MFX_ERR_NONE &&
#if defined(NTDDI_WIN10_TH2)
        p_core->IsGuidSupported(DXVA_ModeVP9_VLD_Profile0, p_video_param) != MFX_ERR_NONE &&
        p_core->IsGuidSupported(DXVA_ModeVP9_VLD_10bit_Profile2_private_copy, p_video_param) != MFX_ERR_NONE)
#else
       TRUE)
#endif
    {
        return false;
    }

    // todo : VA API alternative ?
#endif

    return true;
}

VideoDECODEVP9_HW::VideoDECODEVP9_HW(VideoCORE *p_core, mfxStatus *sts)
    : m_isInit(false),
      m_core(p_core),
      m_platform(MFX_PLATFORM_HARDWARE),
      m_va(NULL),
      m_baseQIndex(0),
      m_y_dc_delta_q(0),
      m_uv_dc_delta_q(0),
      m_uv_ac_delta_q(0),
      m_index(0),
      m_adaptiveMode(false)
{
    memset(&m_frameInfo.ref_frame_map, -1, sizeof(m_frameInfo.ref_frame_map)); // TODO: move to another place
    ResetFrameInfo();

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEVP9_HW::~VideoDECODEVP9_HW(void)
{
    Close();
}

mfxStatus VideoDECODEVP9_HW::Init(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    mfxStatus sts   = MFX_ERR_NONE;
    UMC::Status umcSts   = UMC::UMC_OK;
    eMFXHWType type = m_core->GetHWType();

    m_platform = MFX_VP9_Utility::GetPlatform(m_core, par);

    if (MFX_ERR_NONE > CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!CheckHardwareSupport(m_core, par))
        return MFX_ERR_UNSUPPORTED;

    if (!MFX_VP9_Utility::CheckVideoParam(par, type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_FrameAllocator.reset(new mfx_UMC_FrameAllocator_D3D());

    m_vInitPar = *par;
    m_vPar = m_vInitPar;

    if (0 == m_vInitPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vInitPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vInitPar.mfx.FrameInfo.FrameRateExtD = 1000;
        m_vInitPar.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_vInitPar.mfx.FrameInfo.FrameRateExtD / m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    // allocate memory
    mfxFrameAllocRequest request;
    memset(&request, 0, sizeof(request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_firstSizes, 0, sizeof(m_firstSizes));
    ResetFrameInfo();

    sts = QueryIOSurfInternal(m_platform, &m_vInitPar, &request);
    MFX_CHECK_STS(sts);

    request.AllocId = par->AllocId;

    sts = m_core->AllocFrames(&request, &m_response, false);
    MFX_CHECK_STS(sts);

    sts = m_core->CreateVA(&m_vInitPar, &request, &m_response, m_FrameAllocator.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&m_va, MFX_MEMTYPE_FROM_DECODE);

    bool isUseExternalFrames = (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) != 0;
    m_adaptiveMode = isUseExternalFrames && par->mfx.SliceGroupsPresent;

    if (isUseExternalFrames && !m_adaptiveMode)
    {
        m_FrameAllocator->SetExternalFramesResponse(&m_response);
    }

    umcSts = m_FrameAllocator->InitMfx(0, m_core, par, &request, &m_response, isUseExternalFrames, false);
    if (UMC::UMC_OK != umcSts)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = 0;
    m_statusReportFeedbackNumber = 0;
    m_isInit = true;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Reset(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    eMFXHWType type = m_core->GetHWType();

    if (MFX_ERR_NONE > CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!MFX_VP9_Utility::CheckVideoParam(par, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!IsSameVideoParam(par, &m_vInitPar))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    // need to sw acceleration
    if (m_platform != m_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    ResetFrameInfo();
    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameOrder = 0;
    m_statusReportFeedbackNumber = 0;
    memset(&m_stat, 0, sizeof(m_stat));
    memset(&m_firstSizes, 0, sizeof(m_firstSizes));

    m_vInitPar = *par;

    if (0 == m_vInitPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vInitPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vInitPar.mfx.FrameInfo.FrameRateExtD = 1000;
        m_vInitPar.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_vInitPar.mfx.FrameInfo.FrameRateExtD / m_vInitPar.mfx.FrameInfo.FrameRateExtN;
    
    if (!CheckHardwareSupport(m_core, par))
        return MFX_ERR_UNSUPPORTED;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::Close()
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    ResetFrameInfo();
    m_FrameAllocator->Close();

    if (0 < m_response.NumFrameActual)
    {
        m_core->FreeFrames(&m_response);
    }

    m_isInit = false;
    m_adaptiveMode = false;

    m_frameOrder = (mfxU16)MFX_FRAMEORDER_UNKNOWN;
    m_statusReportFeedbackNumber = 0;

    m_va = NULL;

    memset(&m_stat, 0, sizeof(m_stat));

    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::ResetFrameInfo()
{
    for (mfxU8 i = 0; i < sizeof(m_frameInfo.ref_frame_map)/sizeof(m_frameInfo.ref_frame_map[0]); i++)
    {
        const UMC::FrameMemID oldMid = m_frameInfo.ref_frame_map[i];
        if (oldMid >= 0)
        {
            m_FrameAllocator->DecreaseReference(oldMid);
        }
    }

    memset(&m_frameInfo, 0, sizeof(m_frameInfo));
    m_frameInfo.currFrame = -1;
    m_frameInfo.frameCountInBS = 0;
    m_frameInfo.currFrameInBS = 0;
    memset(&m_frameInfo.ref_frame_map, -1, sizeof(m_frameInfo.ref_frame_map)); // TODO: move to another place
}

mfxStatus VideoDECODEVP9_HW::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    return
        MFX_VP9_Utility::DecodeHeader(core, bs, par);
}

static bool IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
{
    if (newPar->IOPattern != oldPar->IOPattern)
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    Ipp32s asyncDepth = IPP_MIN(newPar->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
    if (asyncDepth != oldPar->AsyncDepth)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Height != oldPar->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.Width != oldPar->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (newPar->mfx.FrameInfo.ChromaFormat != oldPar->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0) //  need more surfaces for efficient decoding
    {
        return false;
    }

    return true;
}

mfxTaskThreadingPolicy VideoDECODEVP9_HW::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEVP9_HW::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = p_core->GetHWType();

    if (!CheckHardwareSupport(p_core, p_in))
        return MFX_ERR_UNSUPPORTED;

    return
        MFX_VP9_Utility::Query(p_core, p_in, p_out, type);
}

mfxStatus VideoDECODEVP9_HW::QueryIOSurfInternal(eMFXPlatform /* platform */, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)
{
    p_request->Info = p_params->mfx.FrameInfo;

    p_request->NumFrameMin = mfxU16 (8);

    p_request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    p_request->NumFrameSuggested = p_request->NumFrameMin;

    if(p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        // Opaq is not supported yet.
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_video_param, mfxFrameAllocRequest *p_request)
{

    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR3(p_core, p_video_param, p_request);

    eMFXPlatform platform = MFX_VP9_Utility::GetPlatform(p_core, p_video_param);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = p_core->GetHWType();
    }

    mfxVideoParam p_params = *p_video_param;

    if (!(p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (p_params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->NumFrameMin = 1;
        p_request->NumFrameSuggested = p_request->NumFrameMin + (p_params.AsyncDepth ? p_params.AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        p_request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        sts = QueryIOSurfInternal(platform, p_video_param, p_request);
    }

    if (!CheckHardwareSupport(p_core, p_video_param))
        return MFX_ERR_UNSUPPORTED;

    return sts;
}

mfxStatus VideoDECODEVP9_HW::GetVideoParam(mfxVideoParam *par)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vPar.mfx;

    par->Protected = m_vInitPar.Protected;
    par->IOPattern = m_vInitPar.IOPattern;
    par->AsyncDepth = m_vInitPar.AsyncDepth;

    par->mfx.FrameInfo.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    par->mfx.FrameInfo.AspectRatioH = m_vInitPar.mfx.FrameInfo.AspectRatioH;
    par->mfx.FrameInfo.AspectRatioW = m_vInitPar.mfx.FrameInfo.AspectRatioW;

    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::UpdateVideoParam(mfxVideoParam *par, VP9DecoderFrame const & frameInfo)
{
    par->mfx.FrameInfo.AspectRatioW = 1;
    par->mfx.FrameInfo.AspectRatioH = 1;

    par->mfx.FrameInfo.CropW = (mfxU16)frameInfo.width;
    par->mfx.FrameInfo.CropH = (mfxU16)frameInfo.height;

    par->mfx.FrameInfo.Width = (frameInfo.width + 15) & ~0x0f;
    par->mfx.FrameInfo.Height = (frameInfo.height + 15) & ~0x0f;

    par->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    par->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
}

mfxStatus VideoDECODEVP9_HW::GetDecodeStat(mfxDecodeStat *pStat)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(pStat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    mfx_memcpy(pStat, sizeof(m_stat), &m_stat, sizeof(m_stat));

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface = m_FrameAllocator->GetSurface(index, surface_work, &m_vInitPar);
    if (pNativeSurface)
    {
        *surface_out = m_core->GetOpaqSurface(pNativeSurface->Data.MemId) ? m_core->GetOpaqSurface(pNativeSurface->Data.MemId) : pNativeSurface;
    }
    else
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pUserData, pSize, pTimeStamp);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(pTimeStamp, pPayload, pPayload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9_HW::SetSkipMode(mfxSkipMode /*mode*/)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    return MFX_ERR_NONE;
}

void VideoDECODEVP9_HW::CalculateTimeSteps(mfxFrameSurface1 *p_surface)
{
    p_surface->Data.TimeStamp = GetMfxTimeStamp(m_num_output_frames * m_in_framerate);
    p_surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    p_surface->Info.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    p_surface->Info.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    p_surface->Info.CropX = m_vInitPar.mfx.FrameInfo.CropX;
    p_surface->Info.CropY = m_vInitPar.mfx.FrameInfo.CropY;
    p_surface->Info.PicStruct = m_vInitPar.mfx.FrameInfo.PicStruct;

    p_surface->Info.AspectRatioH = 1;
    p_surface->Info.AspectRatioW = 1;
}

enum
{
    NUMBER_OF_STATUS = 32,
};

mfxStatus MFX_CDECL VP9DECODERoutine(void *p_state, void * /* pp_param */, mfxU32 /* thread_number */, mfxU32)
{
    VideoDECODEVP9_HW::VP9DECODERoutineData& data = *(VideoDECODEVP9_HW::VP9DECODERoutineData*)p_state;
    VideoDECODEVP9_HW& decoder = *data.decoder;

    if (!data.surface_work)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if (data.copyFromFrame != UMC::FRAME_MID_INVALID)
    {
        UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

        mfxFrameSurface1 surfaceDst = *data.surface_work;
        surfaceDst.Info.Width = (surfaceDst.Info.CropW + 15) & ~0x0f;
        surfaceDst.Info.Height = (surfaceDst.Info.CropH + 15) & ~0x0f;

        mfxFrameSurface1 *surfaceSrc = decoder.m_FrameAllocator->GetSurfaceByIndex(data.copyFromFrame);
        if (!surfaceSrc)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        bool systemMemory = (decoder.m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) != 0;
        mfxU16 srcMemType = MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
        mfxU16 dstMemType = systemMemory ? MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_EXTERNAL_FRAME : srcMemType;

        mfxStatus sts = decoder.m_core->DoFastCopyWrapper(&surfaceDst, dstMemType, surfaceSrc, srcMemType);
        MFX_CHECK_STS(sts);

        decoder.m_FrameAllocator->SetDoNotNeedToCopyFlag(true);
        sts = decoder.m_FrameAllocator->PrepareToOutput(data.surface_work, data.copyFromFrame, 0, false);
        MFX_CHECK_STS(sts);
        decoder.m_FrameAllocator->SetDoNotNeedToCopyFlag(false);
        
        if (data.currFrameId != -1)
           decoder.m_FrameAllocator->DecreaseReference(data.currFrameId);

        decoder.m_FrameAllocator->DecreaseReference(data.copyFromFrame);

        return MFX_ERR_NONE;
    }

#ifdef MFX_VA_LINUX

    UMC::Status status = decoder.m_va->SyncTask(data.currFrameId);
    if (status != UMC::UMC_OK)
    {
        mfxStatus CriticalErrorStatus = (status == UMC::UMC_ERR_GPU_HANG) ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
        SetCriticalErrorOccured(CriticalErrorStatus);
        return CriticalErrorStatus;
    }

#endif

#if defined(VP9_STATUS_REPORTING_ENABLED) && defined(MFX_VA_WIN) && defined(NTDDI_WIN10_TH2)

    DXVA_Status_VPx pStatusReport[NUMBER_OF_STATUS];

    memset(pStatusReport, 0, sizeof(DXVA_Status_VPx) * NUMBER_OF_STATUS);

    for(int i = 0; i < NUMBER_OF_STATUS; i++)
        pStatusReport[i].bStatus = 3;

    decoder.m_Packer->GetStatusReport(&pStatusReport[0], sizeof(DXVA_Status_VPx) * NUMBER_OF_STATUS);

    {
        UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

        for (Ipp32u i = 0; i < NUMBER_OF_STATUS; i++)
        {
            if(pStatusReport[i].bStatus == 3)
                return MFX_ERR_DEVICE_FAILED;

            if (!pStatusReport[i].StatusReportFeedbackNumber || pStatusReport[i].CurrPic.Index7Bits == 127 ||
                (pStatusReport[i].StatusReportFeedbackNumber & 0x80000000))
                continue;

            mfxFrameSurface1 *surface_to_complete = decoder.m_FrameAllocator->GetSurfaceByIndex(pStatusReport[i].CurrPic.Index7Bits);
            if (!surface_to_complete)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            switch (pStatusReport[i].bStatus)
            {
            case 1:
                surface_to_complete->Data.Corrupted = MFX_CORRUPTION_MINOR;
                break;
            case 2:
                surface_to_complete->Data.Corrupted = MFX_CORRUPTION_MAJOR;
                break;
            case 3:
            case 4:
                return MFX_ERR_DEVICE_FAILED;
            }

            decoder.m_completedList.push_back(surface_to_complete);
        }

        VideoDECODEVP9_HW::StatuReportList::iterator it = std::find(decoder.m_completedList.begin(), decoder.m_completedList.end(), data.surface_work);
        if (it == decoder.m_completedList.end())
        {
            return MFX_TASK_WORKING;
        }

        decoder.m_completedList.remove(data.surface_work);
    }
#endif

    if (decoder.m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        mfxStatus sts = decoder.m_FrameAllocator->PrepareToOutput(data.surface_work, data.currFrameId, 0, false);
        MFX_CHECK_STS(sts);
    }

    UMC::AutomaticUMCMutex guard(decoder.m_mGuard);

    if (data.currFrameId != -1)
       decoder.m_FrameAllocator->DecreaseReference(data.currFrameId);

    return MFX_TASK_DONE;
}

mfxStatus VP9CompleteProc(void * /* p_state */, void * /* pp_param */, mfxStatus)
{
    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT * p_entry_point)
{
    UMC::AutomaticUMCMutex guard(m_mGuard);

    mfxStatus sts = MFX_ERR_NONE;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    if (NeedToReturnCriticalStatus(bs))
        return ReturningCriticalStatus();

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);


    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_VP9, true);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (!bs || !bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    if (surface_work->Data.Locked)
        return MFX_ERR_MORE_SURFACE;

    m_index++;
    m_frameInfo.showFrame = 0;
    *surface_out = 0;

    sts = DecodeFrameHeader(bs, m_frameInfo);
    MFX_CHECK_STS(sts);

    UpdateVideoParam(&m_vPar, m_frameInfo);

    // check resize
    if (m_vPar.mfx.FrameInfo.Width > surface_work->Info.Width || m_vPar.mfx.FrameInfo.Height > surface_work->Info.Height)
    {
        if (m_adaptiveMode)
            return (mfxStatus)MFX_ERR_REALLOC_SURFACE;

        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    // possible system memory case. when external surface is enough, but internal is not.
    if (!m_adaptiveMode)
    {
        if (m_vPar.mfx.FrameInfo.Width > m_vInitPar.mfx.FrameInfo.Width || m_vPar.mfx.FrameInfo.Height > m_vInitPar.mfx.FrameInfo.Height)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

        if (KEY_FRAME == m_frameInfo.frameType && (m_vPar.mfx.FrameInfo.Width != m_vInitPar.mfx.FrameInfo.Width || m_vPar.mfx.FrameInfo.Height != m_vInitPar.mfx.FrameInfo.Height))
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, false);
    MFX_CHECK_STS(sts);

    if (m_FrameAllocator->FindFreeSurface() == -1)
    {
        return MFX_WRN_DEVICE_BUSY;
    }

    UMC::FrameMemID currMid = 0;
    UMC::VideoDataInfo videoInfo;

    UMC::ColorFormat const cf = GetUMCColorFormat_VP9(&m_frameInfo);
    if (UMC::UMC_OK != videoInfo.Init(m_vPar.mfx.FrameInfo.Width, m_vPar.mfx.FrameInfo.Height, cf, m_frameInfo.bit_depth))
        return MFX_ERR_MEMORY_ALLOC;

    if (UMC::UMC_OK != m_FrameAllocator->Alloc(&currMid, &videoInfo, 0))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_frameInfo.currFrame = currMid;

    if (!m_frameInfo.frameCountInBS)
    {
        bs->DataOffset += bs->DataLength;
        bs->DataLength = 0;
    }

    if (UMC::UMC_OK != m_FrameAllocator->IncreaseReference(m_frameInfo.currFrame))
    {
        return MFX_ERR_UNKNOWN;
    }

    UMC::FrameMemID repeateFrame = UMC::FRAME_MID_INVALID;

    if (!m_frameInfo.show_existing_frame)
    {
        if (UMC::UMC_OK != m_va->BeginFrame(m_frameInfo.currFrame))
            return MFX_ERR_DEVICE_FAILED;

        sts = PackHeaders(&m_bs, m_frameInfo);
        MFX_CHECK_STS(sts);

        if (UMC::UMC_OK != m_va->Execute())
            return MFX_ERR_DEVICE_FAILED;

        if (UMC::UMC_OK != m_va->EndFrame())
            return MFX_ERR_DEVICE_FAILED;

        UpdateRefFrames(m_frameInfo.refreshFrameFlags, m_frameInfo); // move to async part
    }
    else
    {
        repeateFrame = m_frameInfo.ref_frame_map[m_frameInfo.frame_to_show];
        m_FrameAllocator->IncreaseReference(repeateFrame);
        ++m_statusReportFeedbackNumber;
    }

    p_entry_point->pRoutine = &VP9DECODERoutine;
    p_entry_point->pCompleteProc = &VP9CompleteProc;

    VP9DECODERoutineData* routineData = new VP9DECODERoutineData;
    routineData->decoder = this;
    routineData->currFrameId = m_frameInfo.currFrame;
    routineData->copyFromFrame = repeateFrame;
    routineData->surface_work = surface_work;
    routineData->index        = m_index;

    p_entry_point->pState = routineData;
    p_entry_point->requiredNumThreads = 1;

    if (m_frameInfo.showFrame)
    {
        sts = GetOutputSurface(surface_out, surface_work, m_frameInfo.currFrame);
        MFX_CHECK_STS(sts);

        (*surface_out)->Data.TimeStamp = bs->TimeStamp != MFX_TIMESTAMP_UNKNOWN? bs->TimeStamp : GetMfxTimeStamp(m_frameOrder * m_in_framerate);
        (*surface_out)->Data.Corrupted = 0;
        (*surface_out)->Data.FrameOrder = m_frameOrder;
        (*surface_out)->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        (*surface_out)->Info.CropW = (mfxU16)m_frameInfo.width;
        (*surface_out)->Info.CropH = (mfxU16)m_frameInfo.height;

        m_frameOrder++;
        return MFX_ERR_NONE;
    }
    else
    {
        sts = GetOutputSurface(surface_out, surface_work, m_frameInfo.currFrame);
        MFX_CHECK_STS(sts);
        surface_out = 0;
        return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK;
    }
}

mfxStatus VideoDECODEVP9_HW::DecodeSuperFrame(mfxBitstream *in, VP9DecoderFrame & info)
{
    mfxU32 frameSizes[8] = { 0 };
    mfxU32 frameCount = 0;

    m_bs = *in;

    ParseSuperFrameIndex(in->Data + in->DataOffset, in->DataLength, frameSizes, &frameCount);

    if (frameCount > 1)
    {
        if (info.frameCountInBS == 0) // first call we meet super frame
        {
            info.frameCountInBS = frameCount;
            info.currFrameInBS = 0;
        }

        if (info.frameCountInBS != frameCount) // it is not what we met before
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        m_bs.DataLength = frameSizes[info.currFrameInBS];
        m_bs.DataOffset = in->DataOffset;
        for (mfxU32 i = 0; i < info.currFrameInBS; i++)
            m_bs.DataOffset += frameSizes[i];

        info.currFrameInBS++;
        if (info.currFrameInBS < info.frameCountInBS)
            return MFX_ERR_NONE;
    }

    info.currFrameInBS = 0;
    info.frameCountInBS = 0;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::DecodeFrameHeader(mfxBitstream *in, VP9DecoderFrame & info)
{
    if (!in || !in->Data)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = DecodeSuperFrame(in, info);
    MFX_CHECK_STS(sts);
    in = &m_bs;

    VP9Bitstream bsReader(in->Data + in->DataOffset, in->DataOffset + in->DataLength);

    if (VP9_FRAME_MARKER != bsReader.GetBits(2))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    info.profile = bsReader.GetBit();
    info.profile |= bsReader.GetBit() << 1;
    if (info.profile > 2)
        info.profile += bsReader.GetBit();

    if (info.profile >= 4)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    info.show_existing_frame = bsReader.GetBit();
    if (info.show_existing_frame)
    {
        info.frame_to_show = bsReader.GetBits(3);
        info.width = info.sizesOfRefFrame[info.frame_to_show].width;
        info.height = info.sizesOfRefFrame[info.frame_to_show].height;
        info.showFrame = 1;
        return MFX_ERR_NONE;
    }

    info.frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
    info.showFrame = bsReader.GetBit();
    info.errorResilientMode = bsReader.GetBit();

    if (KEY_FRAME == info.frameType)
    {
        if (!CheckSyncCode(&bsReader))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        try
        { GetBitDepthAndColorSpace(&bsReader, &info); }
        catch (vp9_exception const& e)
        {
            UMC::Status status = e.GetStatus();
            if (status != UMC::UMC_OK)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        info.refreshFrameFlags = (1 << NUM_REF_FRAMES) - 1;

        for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
        {
            info.activeRefIdx[i] = 0;
        }

        GetFrameSize(&bsReader, &info);
    }
    else
    {
        info.intraOnly = info.showFrame ? 0 : bsReader.GetBit();
        info.resetFrameContext = info.errorResilientMode ? 0 : bsReader.GetBits(2);

        if (info.intraOnly)
        {
            if (!CheckSyncCode(&bsReader))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            try
            {
                GetBitDepthAndColorSpace(&bsReader, &info);
        
                info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);
                GetFrameSize(&bsReader, &info);
            }
            catch (vp9_exception const& e)
            {
                UMC::Status status = e.GetStatus();
                if (status != UMC::UMC_OK)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            
        }
        else
        {
            info.refreshFrameFlags = (mfxU8)bsReader.GetBits(NUM_REF_FRAMES);

            for (mfxU8 i = 0; i < REFS_PER_FRAME; ++i)
            {
                const mfxI32 ref = bsReader.GetBits(REF_FRAMES_LOG2);
                info.activeRefIdx[i] = ref;
                info.refFrameSignBias[LAST_FRAME + i] = bsReader.GetBit();
            }

            GetFrameSizeWithRefs(&bsReader, &info);

            info.allowHighPrecisionMv = bsReader.GetBit();

            static const INTERP_FILTER literal2Filter[] =
                { EIGHTTAP_SMOOTH, EIGHTTAP, EIGHTTAP_SHARP, BILINEAR };
            info.interpFilter = bsReader.GetBit() ? SWITCHABLE : literal2Filter[bsReader.GetBits(2)];
        }
    }

    if (!info.errorResilientMode)
    {
        info.refreshFrameContext = bsReader.GetBit();
        info.frameParallelDecodingMode = bsReader.GetBit();
    }
    else
    {
        info.refreshFrameContext = 0;
        info.frameParallelDecodingMode = 1;
    }

    // This flag will be overridden by the call to vp9_setup_past_independence
    // below, forcing the use of context 0 for those frame types.
    info.frameContextIdx = bsReader.GetBits(FRAME_CONTEXTS_LOG2);

    if (info.frameType == KEY_FRAME || info.intraOnly || info.errorResilientMode)
    {
       SetupPastIndependence(info);
    }

    SetupLoopFilter(&bsReader, &info.lf);

    //setup_quantization()
    {
        info.baseQIndex = bsReader.GetBits(QINDEX_BITS);
        mfxI32 old_y_dc_delta_q = m_y_dc_delta_q;
        mfxI32 old_uv_dc_delta_q = m_uv_dc_delta_q;
        mfxI32 old_uv_ac_delta_q = m_uv_ac_delta_q;

        if (bsReader.GetBit())
        {
            info.y_dc_delta_q = bsReader.GetBits(4);
            info.y_dc_delta_q = bsReader.GetBit() ? -info.y_dc_delta_q : info.y_dc_delta_q;
        }
        else
            info.y_dc_delta_q = 0;

        if (bsReader.GetBit())
        {
            info.uv_dc_delta_q = bsReader.GetBits(4);
            info.uv_dc_delta_q = bsReader.GetBit() ? -info.uv_dc_delta_q : info.uv_dc_delta_q;
        }
        else
            info.uv_dc_delta_q = 0;

        if (bsReader.GetBit())
        {
            info.uv_ac_delta_q = bsReader.GetBits(4);
            info.uv_ac_delta_q = bsReader.GetBit() ? -info.uv_ac_delta_q : info.uv_ac_delta_q;
        }
        else
            info.uv_ac_delta_q = 0;

        m_y_dc_delta_q = info.y_dc_delta_q;
        m_uv_dc_delta_q = info.uv_dc_delta_q;
        m_uv_ac_delta_q = info.uv_ac_delta_q;

        if (old_y_dc_delta_q  != info.y_dc_delta_q  ||
            old_uv_dc_delta_q != info.uv_dc_delta_q ||
            old_uv_ac_delta_q != info.uv_ac_delta_q ||
            0 == m_frameOrder)
        {
            InitDequantizer(&info);
        }

        info.lossless = (0 == info.baseQIndex &&
                         0 == info.y_dc_delta_q &&
                         0 == info.uv_dc_delta_q &&
                         0 == info.uv_ac_delta_q);
    }

    // setup_segmentation()
    {
        info.segmentation.updateMap = 0;
        info.segmentation.updateData = 0;

        info.segmentation.enabled = (mfxU8)bsReader.GetBit();
        if (info.segmentation.enabled)
        {
            // Segmentation map update
            info.segmentation.updateMap = (mfxU8)bsReader.GetBit();
            if (info.segmentation.updateMap)
            {
                for (mfxU8 i = 0; i < VP9_NUM_OF_SEGMENT_TREE_PROBS; ++i)
                    info.segmentation.treeProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);

                info.segmentation.temporalUpdate = (mfxU8)bsReader.GetBit();
                if (info.segmentation.temporalUpdate)
                {
                    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                        info.segmentation.predProbs[i] = (mfxU8) (bsReader.GetBit() ? bsReader.GetBits(8) : VP9_MAX_PROB);
                }
                else
                {
                    for (mfxU8 i = 0; i < VP9_NUM_OF_PREDICTION_PROBS; ++i)
                        info.segmentation.predProbs[i] = VP9_MAX_PROB;
                }
            }

            info.segmentation.updateData = (mfxU8)bsReader.GetBit();
            if (info.segmentation.updateData)
            {
                info.segmentation.absDelta = (mfxU8)bsReader.GetBit();

                ClearAllSegFeatures(info.segmentation);

                mfxI32 data = 0;
                mfxU32 nBits = 0;
                for (mfxU8 i = 0; i < VP9_MAX_NUM_OF_SEGMENTS; ++i)
                {
                    for (mfxU8 j = 0; j < SEG_LVL_MAX; ++j)
                    {
                        data = 0;
                        if (bsReader.GetBit()) // feature_enabled
                        {
                            EnableSegFeature(info.segmentation, i, (SEG_LVL_FEATURES) j);

                            nBits = GetUnsignedBits(SEG_FEATURE_DATA_MAX[j]);
                            data = bsReader.GetBits(nBits);
                            data = data > SEG_FEATURE_DATA_MAX[j] ? SEG_FEATURE_DATA_MAX[j] : data;

                            if (IsSegFeatureSigned( (SEG_LVL_FEATURES) j))
                                data = bsReader.GetBit() ? -data : data;
                        }

                        SetSegData(info.segmentation, i, (SEG_LVL_FEATURES) j, data);
                    }
                }
            }
        }
    }

    // setup_tile_info()
    {
        const mfxI32 alignedWidth = ALIGN_POWER_OF_TWO(info.width, MI_SIZE_LOG2);
        int minLog2TileColumns, maxLog2TileColumns, maxOnes;
        mfxU32 miCols = alignedWidth >> MI_SIZE_LOG2;
        GetTileNBits(miCols, minLog2TileColumns, maxLog2TileColumns);

        maxOnes = maxLog2TileColumns - minLog2TileColumns;
        info.log2TileColumns = minLog2TileColumns;
        while (maxOnes-- && bsReader.GetBit())
            info.log2TileColumns++;

        info.log2TileRows = bsReader.GetBit();
        if (info.log2TileRows)
            info.log2TileRows += bsReader.GetBit();
    }

    info.firstPartitionSize = bsReader.GetBits(16);
    if (0 == info.firstPartitionSize)
        return MFX_ERR_UNSUPPORTED;

    info.frameHeaderLength = Ipp32u(bsReader.BitsDecoded() / 8 + (bsReader.BitsDecoded() % 8 > 0));
    info.frameDataSize = in->DataLength;

    // vp9_loop_filter_frame_init()
    if (info.lf.filterLevel)
    {
        const mfxI32 scale = 1 << (info.lf.filterLevel >> 5);

        LoopFilterInfo & lf_info = info.lf_info;

        for (mfxU8 segmentId = 0; segmentId < VP9_MAX_NUM_OF_SEGMENTS; ++segmentId)
        {
            mfxI32 segmentFilterLevel = info.lf.filterLevel;
            if (IsSegFeatureActive(info.segmentation, segmentId, SEG_LVL_ALT_LF))
            {
                const mfxI32 data = GetSegData(info.segmentation, segmentId, SEG_LVL_ALT_LF);
                segmentFilterLevel = clamp(info.segmentation.absDelta == SEGMENT_ABSDATA ? data : info.lf.filterLevel + data,
                                           0,
                                           MAX_LOOP_FILTER);
            }

            if (!info.lf.modeRefDeltaEnabled)
            {
                memset(lf_info.level[segmentId], segmentFilterLevel, sizeof(lf_info.level[segmentId]) );
            }
            else
            {
                const mfxI32 intra_lvl = segmentFilterLevel + info.lf.refDeltas[INTRA_FRAME] * scale;
                lf_info.level[segmentId][INTRA_FRAME][0] = (mfxU8) clamp(intra_lvl, 0, MAX_LOOP_FILTER);

                for (mfxU8 ref = LAST_FRAME; ref < MAX_REF_FRAMES; ++ref)
                {
                    for (mfxU8 mode = 0; mode < MAX_MODE_LF_DELTAS; ++mode)
                    {
                        const mfxI32 inter_lvl = segmentFilterLevel + info.lf.refDeltas[ref] * scale
                                                        + info.lf.modeDeltas[mode] * scale;
                        lf_info.level[segmentId][ref][mode] = (mfxU8) clamp(inter_lvl, 0, MAX_LOOP_FILTER);
                    }
                }
            }
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::UpdateRefFrames(const mfxU8 refreshFrameFlags, VP9DecoderFrame & info)
{
    mfxI32 ref_index = 0;

    for (mfxU8 mask = refreshFrameFlags; mask; mask >>= 1)
    {
        if (mask & 1)
        {
            const UMC::FrameMemID oldMid = info.ref_frame_map[ref_index];
            if (oldMid >= 0)
            {
                if (m_FrameAllocator->DecreaseReference(oldMid) != UMC::UMC_OK)
                    return MFX_ERR_UNKNOWN;
            }

            info.ref_frame_map[ref_index] = info.currFrame;

            info.sizesOfRefFrame[ref_index].width = info.width;
            info.sizesOfRefFrame[ref_index].height = info.height;

            if (m_FrameAllocator->IncreaseReference(info.currFrame) != UMC::UMC_OK)
                return MFX_ERR_UNKNOWN;
        }
        ++ref_index;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9_HW::PackHeaders(mfxBitstream *bs, VP9DecoderFrame const & info)
{
    if ((NULL == bs) || (NULL == bs->Data))
        return MFX_ERR_NULL_PTR;

    if (!m_Packer.get())
    {
        m_Packer.reset(Packer::CreatePacker(m_va));
        if (!m_Packer.get())
            return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    VP9Bitstream vp9bs(bs->Data + bs->DataOffset, bs->DataLength);

    mfxStatus sts = MFX_ERR_NONE;
    try
    {
        m_Packer->BeginFrame();
        m_Packer->PackAU(&vp9bs, &info);
        m_Packer->EndFrame(); 
    }
    catch (vp9_exception const&)
    {
        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return sts;
}
#endif
