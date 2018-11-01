//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#include <limits>
#include "mfx_vp9_dec_decode.h"

#include "mfx_common.h"
#include "mfx_common_decode_int.h"
#include "mfx_vpx_dec_common.h"
#include "mfx_enc_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

#include "umc_vp9_utils.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"

using namespace UMC_VP9_DECODER;

#endif

#ifdef MFX_ENABLE_VP9_VIDEO_DECODE

#include "umc_data_pointers_copy.h"

#include "vm_sys_info.h"
#include "ippcore.h"

#include "ippcc.h"

#include "mfx_thread_task.h"

#ifdef _MSVC_LANG
#pragma warning(disable : 4505)
#endif

#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

static mfxStatus vpx_convert_status(mfxI32 status)
{
    switch (status)
    {
        case VPX_CODEC_OK:
            return MFX_ERR_NONE;
        case VPX_CODEC_ERROR:
            return MFX_ERR_UNKNOWN;
        case VPX_CODEC_MEM_ERROR:
            return MFX_ERR_MEMORY_ALLOC;
        case VPX_CODEC_INVALID_PARAM:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        case VPX_CODEC_ABI_MISMATCH:
        case VPX_CODEC_INCAPABLE:
        case VPX_CODEC_UNSUP_BITSTREAM:
        case VPX_CODEC_UNSUP_FEATURE:
        case VPX_CODEC_CORRUPT_FRAME:
        default:
            return MFX_ERR_UNSUPPORTED;
    }
}

#define CHECK_VPX_STATUS(status) \
    if (VPX_CODEC_OK != status) \
        return vpx_convert_status(status);

VideoDECODEVP9::VideoDECODEVP9(VideoCORE *core, mfxStatus *sts)
    :VideoDECODE()
    ,m_core(core)
    ,m_isInit(false)
    ,m_is_opaque_memory(false)
    ,m_num_output_frames(0)
    ,m_in_framerate(0)
    ,m_platform(MFX_PLATFORM_SOFTWARE)

{
    memset(&m_bs.Data, 0, sizeof(m_bs.Data));

    // allocate vpx decoder
    m_vpx_codec = malloc(sizeof(vpx_codec_ctx_t));

    if (!m_vpx_codec)
    {
        *sts = MFX_ERR_NOT_INITIALIZED;
    }

    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEVP9::~VideoDECODEVP9(void)
{
    Close();
}

mfxStatus VideoDECODEVP9::Init(mfxVideoParam *params)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_isInit)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    MFX_CHECK_NULL_PTR1(params);

    m_platform = MFX_PLATFORM_SOFTWARE;
    eMFXHWType type = MFX_HW_UNKNOWN;

    if (MFX_ERR_NONE > CheckVideoParamDecoders(params, m_core->IsExternalFrameAllocator(), type))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!MFX_VPX_Utility::CheckVideoParam(params, MFX_CODEC_VP9, m_platform))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_vInitPar = *params;

    if (0 == m_vInitPar.mfx.FrameInfo.FrameRateExtN || 0 == m_vInitPar.mfx.FrameInfo.FrameRateExtD)
    {
        m_vInitPar.mfx.FrameInfo.FrameRateExtD = 1000;
        m_vInitPar.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_vInitPar.mfx.FrameInfo.FrameRateExtD / m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vInitPar);

    if (0 == m_vInitPar.mfx.NumThread)
    {
        m_vInitPar.mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num();
    }

    m_FrameAllocator.reset(new mfx_UMC_FrameAllocator);

    int32_t useInternal = (m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || (m_vInitPar.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY);

    // allocate memory
    memset(&m_request, 0, sizeof(m_request));
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_frameInfo, 0, sizeof(m_frameInfo));

    sts = QueryIOSurfInternal(m_platform, &m_vInitPar, &m_request);
    MFX_CHECK_STS(sts);

    if (useInternal)
    {
        m_request.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;

        mfxExtOpaqueSurfaceAlloc *p_opq_ext = NULL;
        if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & params->IOPattern) // opaque memory case
        {
            p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

            if (NULL != p_opq_ext)
            {
                if (m_request.NumFrameMin > p_opq_ext->Out.NumSurface)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                m_is_opaque_memory = true;

                if (MFX_MEMTYPE_FROM_DECODE & p_opq_ext->Out.Type)
                {
                    if (MFX_MEMTYPE_SYSTEM_MEMORY & p_opq_ext->Out.Type)
                    {
                        // map surfaces with opaque
                        m_request.Type = (mfxU16)p_opq_ext->Out.Type;
                        m_request.NumFrameMin = m_request.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;

                    }
                    else
                    {
                        m_request.Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE;
                        m_request.Type |= MFX_MEMTYPE_DXVA2_DECODER_TARGET;
                        m_request.NumFrameMin = m_request.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;
                    }
                }
                else
                {
                    mfxFrameAllocRequest trequest = m_request;
                    trequest.Type =  (mfxU16)p_opq_ext->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;

                    trequest.AllocId = params->AllocId;

                    sts = m_core->AllocFrames(&trequest,
                                                &m_opaque_response,
                                                p_opq_ext->In.Surfaces,
                                                p_opq_ext->In.NumSurface);

                    if (MFX_ERR_NONE != sts && MFX_ERR_UNSUPPORTED != sts)
                    {
                        // unsupported means that current core couldn;t allocate the surfaces
                        return sts;
                    }
                }
            }
        }

        if (true == m_is_opaque_memory)
        {
            sts  = m_core->AllocFrames(&m_request,
                                         &m_response,
                                         p_opq_ext->Out.Surfaces,
                                         p_opq_ext->Out.NumSurface);
        }
        else
        {
            m_request.AllocId = params->AllocId;
            sts = m_core->AllocFrames(&m_request, &m_response);
            MFX_CHECK_STS(sts);
        }

        if (sts != MFX_ERR_NONE && sts != MFX_ERR_NOT_FOUND)
        {
            // second status means that external allocator was not found, it is ok
            return sts;
        }

        UMC::Status umcSts = UMC::UMC_OK;

        // no need to save surface descriptors if all memory is d3d
        if (params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        {
            umcSts = m_FrameAllocator->InitMfx(NULL, m_core, params, &m_request, &m_response, false, true);
        }
        else
        {
             // means that memory is d3d9 surfaces
            m_FrameAllocator->SetExternalFramesResponse(&m_response);
            umcSts = m_FrameAllocator->InitMfx(NULL, m_core, params, &m_request, &m_response, true, true);
        }

        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    else
    {
        m_request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;

        UMC::Status umcSts = m_FrameAllocator->InitMfx(0, m_core, params, &m_request, &m_response, !useInternal, MFX_PLATFORM_SOFTWARE == m_platform);
        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    vpx_codec_dec_cfg_t cfg;
    cfg.threads = m_vInitPar.mfx.NumThread;

    mfxI32 vpx_sts = vpx_codec_dec_init((vpx_codec_ctx_t *)m_vpx_codec, vpx_codec_vp9_dx(), &cfg, 0);
    CHECK_VPX_STATUS(vpx_sts);

    m_isInit = true;

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::Reset(mfxVideoParam *params)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(params);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    if (CheckVideoParamDecoders(params, m_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VPX_Utility::CheckVideoParam(params, MFX_CODEC_VP9, m_platform))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == IsSameVideoParam(params, &m_vInitPar))
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

     mfxExtOpaqueSurfaceAlloc *p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(params->ExtParam, params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

    if (NULL != p_opq_ext)
    {
        if (false == m_is_opaque_memory)
        {
            // decoder was not initialized with opaque extended buffer
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (m_request.NumFrameMin != p_opq_ext->Out.NumSurface)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    // need to sw acceleration
    if (m_platform != m_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (m_FrameAllocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    memset(&m_stat, 0, sizeof(m_stat));
    m_vInitPar = *params;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_vInitPar);
    m_vInitPar = m_vInitPar;

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::Close(void)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    m_FrameAllocator->Close();

    if (0 != m_response.NumFrameActual)
    {
        m_core->FreeFrames(&m_response);
    }

    m_isInit = false;
    m_is_opaque_memory = false;

    memset(&m_stat, 0, sizeof(m_stat));

    vpx_codec_destroy((vpx_codec_ctx_t *)m_vpx_codec);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::DecodeHeader(VideoCORE * core, mfxBitstream *bs, mfxVideoParam *params)
{
    return MFX_VP9_Utility::DecodeHeader(core, bs, params);
}

bool VideoDECODEVP9::IsSameVideoParam(mfxVideoParam *newPar, mfxVideoParam *oldPar)
{
    if (newPar->IOPattern != oldPar->IOPattern)
    {
        return false;
    }

    if (newPar->Protected != oldPar->Protected)
    {
        return false;
    }

    int32_t asyncDepth = MFX_MIN(newPar->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
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

    if (newPar->mfx.NumThread > oldPar->mfx.NumThread && oldPar->mfx.NumThread > 0)
    {
        return false;
    }

    return true;
}

mfxTaskThreadingPolicy VideoDECODEVP9::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;
}

mfxStatus VideoDECODEVP9::Query(VideoCORE *core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = core->GetHWType();
    return MFX_VPX_Utility::Query(core, p_in, p_out, MFX_CODEC_VP9, type);
}

mfxStatus VideoDECODEVP9::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *request)
{
    MFX_INTERNAL_CPY(&request->Info, &p_params->mfx.FrameInfo, sizeof(mfxFrameInfo));

    mfxU32 threads = p_params->mfx.NumThread;

    if (!threads)
    {
        threads = vm_sys_info_get_cpu_num();
    }

    if (platform != MFX_PLATFORM_SOFTWARE)
    {
        threads = 1;
    }

    if (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else // opaque memory case
    {
        if (MFX_PLATFORM_SOFTWARE == platform)
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
        else
        {
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    request->NumFrameMin = mfxU16 (4);

    request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    request->NumFrameSuggested = request->NumFrameMin;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::QueryIOSurf(VideoCORE *core, mfxVideoParam *p_params, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(p_params, request);

    if (!(p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        !(p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) &&
        !(p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((p_params->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) &&
        (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    eMFXPlatform platform = core->GetPlatformType();

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = core->GetHWType();
    }

    mfxVideoParam params;
    MFX_INTERNAL_CPY(&params, p_params, sizeof(mfxVideoParam));
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    mfxStatus sts = QueryIOSurfInternal(platform, &params, request);
    MFX_CHECK_STS(sts);

    int32_t isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        request->NumFrameSuggested = request->NumFrameMin = 4;
    }

    if (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    if (platform != core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetVideoParam(mfxVideoParam *par)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par);

    par->mfx = m_vInitPar.mfx;

    par->Protected = m_vInitPar.Protected;
    par->IOPattern = m_vInitPar.IOPattern;
    par->AsyncDepth = m_vInitPar.AsyncDepth;

    par->mfx.FrameInfo.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    par->mfx.FrameInfo.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    par->mfx.FrameInfo.AspectRatioH = m_vInitPar.mfx.FrameInfo.AspectRatioH;
    par->mfx.FrameInfo.AspectRatioW = m_vInitPar.mfx.FrameInfo.AspectRatioW;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetDecodeStat(mfxDecodeStat *stat)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat);

    m_stat.NumSkippedFrame = 0;
    m_stat.NumCachedFrame = 0;

    MFX_INTERNAL_CPY(stat, &m_stat, sizeof(m_stat));

    return MFX_ERR_NONE;
}

mfxFrameSurface1 * VideoDECODEVP9::GetOriginalSurface(mfxFrameSurface1 *p_surface)
{
    if (true == m_is_opaque_memory)
    {
        return m_core->GetNativeSurface(p_surface);
    }

    return p_surface;
}

mfxStatus VideoDECODEVP9::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *native_surface =  m_FrameAllocator->GetSurface(index, surface_work, &m_vInitPar);

    if (native_surface)
    {
        *surface_out = m_is_opaque_memory ? m_core->GetOpaqSurface(native_surface->Data.MemId) : native_surface;
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR3(ud, sz, ts);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9::GetPayload(mfxU64 *ts, mfxPayload *payload)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;


    MFX_CHECK_NULL_PTR3(ts, payload, payload->Data);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus VideoDECODEVP9::SetSkipMode(mfxSkipMode /*mode*/)
{
    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::ReadFrameInfo(mfxU8 *pData, mfxU32 size, VP9BaseFrameInfo *out)
{
    if (!pData || !size)
        return MFX_ERR_MORE_DATA;

    if (!out)
        return MFX_ERR_NULL_PTR;

    mfxU32 frameSizes[8] = { 0 };
    mfxU32 frameCount = 0;

    ParseSuperFrameIndex(pData, size, frameSizes, &frameCount);

    if (0 == frameCount)
    {
        frameCount = 1;
        frameSizes[0] = size;
    }

    if (frameCount > 0)
    {
        for (mfxU32 i = 0; i < frameCount; ++i)
        {
            const mfxU32 frameSize = frameSizes[i];
            VP9Bitstream bsReader(pData, frameSize);

            if (VP9_FRAME_MARKER != bsReader.GetBits(2))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            mfxU32 profile = 0;
            profile = bsReader.GetBit();
            profile |= bsReader.GetBit() << 1;
            if (profile > 2)
                profile += bsReader.GetBit();
            if (profile >= 4)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            bsReader.GetBit(); // show_existing_frame

            VP9_FRAME_TYPE frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
            out->ShowFrame = (mfxU16)bsReader.GetBit();
            mfxU32 errorResilientMode = bsReader.GetBit();

            mfxU32 bit_depth = 8;
            if (KEY_FRAME == frameType)
            {
                if (!CheckSyncCode(&bsReader))
                    return MFX_ERR_UNDEFINED_BEHAVIOR;

                if (profile >= 2)
                {
                    bit_depth = bsReader.GetBit() ? 12 : 10;
                }

                if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
                {
                    bsReader.GetBit();
                    if (1 == profile || 3 == profile)
                        bsReader.GetBits(3);
                }
                else
                {
                    if (1 == profile || 3 == profile)
                        bsReader.GetBit();
                    else
                    {
                        pData += frameSize;
                        continue;
                    }
                }

                out->Width = (mfxU16)bsReader.GetBits(16) + 1;
                out->Height = (mfxU16)bsReader.GetBits(16) + 1;
            }
            else
            {
                mfxU32 intraOnly = out->ShowFrame ? 0 : bsReader.GetBit();
                if (!intraOnly)
                {

                    pData += frameSize;
                    continue;
                }
                else
                {
                    // need to read info from refs frames
                }

                if (!errorResilientMode)
                    bsReader.GetBits(2);

                if (!CheckSyncCode(&bsReader))
                    return MFX_ERR_UNDEFINED_BEHAVIOR;

                if (profile >= 2)
                    bit_depth = bsReader.GetBit() ? 12 : 10;

                if (profile > 0)
                {
                    if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
                    {
                        bsReader.GetBit();
                        if (1 == profile || 3 == profile)
                            bsReader.GetBits(3);
                    }
                    else
                    {
                        if (1 == profile || 3 == profile)
                            bsReader.GetBit();
                        else
                        {
                            pData += frameSize;
                            continue;
                        }
                    }
                }

                bsReader.GetBits(NUM_REF_FRAMES);

                out->Width = (mfxU16)bsReader.GetBits(16) + 1;
                out->Height = (mfxU16)bsReader.GetBits(16) + 1;
            }
            pData += frameSize;
        }
    }

    return MFX_ERR_NONE;
}

static mfxStatus VP9CompleteProc(void *, void *p_param, mfxStatus )
{
    free(p_param);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::RunThread(void * /*p_params*/, mfxU32 /*thread_number*/)
{
    return MFX_TASK_DONE;
}

static mfxStatus __CDECL VP9DECODERoutine(void *state, void *param, mfxU32 /*thread_number*/, mfxU32)
{
    mfxStatus mfxSts = MFX_ERR_NONE;

    THREAD_TASK_INFO_VP9 *thread_info = (THREAD_TASK_INFO_VP9 *) param;

    vpx_codec_ctx_t *decoder = (vpx_codec_ctx_t *)state;

    mfxI32 vpx_sts = 0;
    vpx_sts = vpx_codec_decode(decoder, thread_info->m_p_bitstream, thread_info->m_frame.frame_size, NULL, 0);
    CHECK_VPX_STATUS(vpx_sts);

    vpx_codec_iter_t iter = NULL;
    vpx_image_t *img = vpx_codec_get_frame(decoder, &iter);

    if (img && thread_info->m_p_surface_out)
    {
        mfxFrameData inData = {};
        inData.Y = img->planes[VPX_PLANE_Y];
        inData.U = img->planes[VPX_PLANE_U];
        inData.V = img->planes[VPX_PLANE_V];
        inData.Pitch = (mfxU16)img->stride[VPX_PLANE_Y];

        mfxFrameInfo inInfo = {};
        inInfo.Width  = (mfxU16)img->w;
        inInfo.Height = (mfxU16)img->h;
        inInfo.CropW  = (mfxU16)img->d_w;
        inInfo.CropH  = (mfxU16)img->d_h;

        mfxFrameData outData = {};
        outData.Y  = (mfxU8*)thread_info->m_p_video_data->GetPlanePointer(0);
        outData.UV = (mfxU8*)thread_info->m_p_video_data->GetPlanePointer(1);
        outData.Pitch = (mfxU16)thread_info->m_p_video_data->GetPlanePitch(0);

        mfxSts = MFX_VPX_Utility::Convert_YV12_to_NV12(&inData, &inInfo, &outData, &thread_info->m_p_surface_out->Info);

        if (MFX_ERR_NONE == mfxSts)
        {
            thread_info->m_p_mfx_umc_frame_allocator->PrepareToOutput(thread_info->m_p_surface_out, thread_info->m_memId, &thread_info->m_video_params, false);
            thread_info->m_p_mfx_umc_frame_allocator->DecreaseReference(thread_info->m_memId);
            thread_info->m_p_mfx_umc_frame_allocator->Unlock(thread_info->m_memId);
        }
    }

    delete [] thread_info->m_p_bitstream;
    delete thread_info->m_p_video_data;

    return mfxSts;
}

mfxStatus VideoDECODEVP9::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *p_entry_point)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (!m_isInit)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    if (0 != surface_work->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    if (true == m_is_opaque_memory)
    {
        if (surface_work->Data.MemId || surface_work->Data.Y || surface_work->Data.R || surface_work->Data.A || surface_work->Data.UV) // opaq surface
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        surface_work = GetOriginalSurface(surface_work);
    }

    sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_VP9, false);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    sts = bs ? CheckBitstream(bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (NULL == bs)
    {
        return MFX_ERR_MORE_DATA;
    }

    if (0 == bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    sts = ReadFrameInfo(bs->Data + bs->DataOffset, bs->DataLength, &m_frameInfo);
    MFX_CHECK_STS(sts);

    if (bs)
    {
        mfxFrameSurface1 *p_surface = surface_work;
        if (true == m_is_opaque_memory)
        {
            p_surface = m_core->GetOpaqSurface(surface_work->Data.MemId, true);
        }

        p_surface->Info.CropW = m_frameInfo.Width;
        p_surface->Info.CropH = m_frameInfo.Height;

        mfxU16 Width = (m_frameInfo.Width + 15) & ~0x0f;
        mfxU16 Height = (m_frameInfo.Height + 15) & ~0x0f;

        if (   m_vInitPar.mfx.FrameInfo.Width < Width
            && m_vInitPar.mfx.FrameInfo.Height < Height)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    IVF_FRAME_VP9 frame;
    memset(&frame, 0, sizeof(IVF_FRAME_VP9));

    sts = ConstructFrame(bs, &m_bs, frame);
    MFX_CHECK_STS(sts);

    *surface_out = 0;

    sts = m_FrameAllocator->SetCurrentMFXSurface(surface_work, m_is_opaque_memory);
    MFX_CHECK_STS(sts);

    UMC::VideoDataInfo info;
    info.Init(surface_work->Info.Width, surface_work->Info.Height, UMC::YUV420);

    UMC::FrameMemID memId = 0;

    UMC::FrameData *p_frame_data = NULL;
    UMC::VideoData *video_data = new UMC::VideoData;

    if (m_frameInfo.ShowFrame)
    {
        m_FrameAllocator->Alloc(&memId, &info, 0);

        // get to decode frame data
        p_frame_data = (UMC::FrameData *) m_FrameAllocator->Lock(memId);

        if (NULL == p_frame_data)
        {
            delete video_data;
            return MFX_ERR_LOCK_MEMORY;
        }

        m_FrameAllocator->IncreaseReference(memId);

        UMC::Status umcSts = video_data->Init(surface_work->Info.Width, surface_work->Info.Height, UMC::YUV420);
        if (umcSts != UMC::UMC_OK)
            return ConvertStatusUmc2Mfx(umcSts);

        {
            const UMC::FrameData::PlaneMemoryInfo *p_info;

            p_info = p_frame_data->GetPlaneMemoryInfo(0);

            video_data->SetPlanePointer(p_info->m_planePtr, 0);
            uint32_t pitch = (uint32_t) p_info->m_pitch;

            p_info = p_frame_data->GetPlaneMemoryInfo(1);
            video_data->SetPlanePointer(p_info->m_planePtr, 1);

            video_data->SetPlanePitch(pitch, 0);
            video_data->SetPlanePitch(pitch, 1);
        }

        // get output surface
        sts = GetOutputSurface(surface_out, surface_work, memId);
        MFX_CHECK_STS(sts);
            //*surface_out = m_p_frame_allocator->GetSurface(memId, surface_work, &m_video_params);

        SetOutputParams(surface_work);

        (*surface_out)->Data.TimeStamp = bs->TimeStamp;
    }

    THREAD_TASK_INFO_VP9 *p_info = new THREAD_TASK_INFO_VP9;
    MFX_CHECK_NULL_PTR1(p_info);
    p_info->m_p_surface_work = surface_work;
    p_info->m_p_surface_out = GetOriginalSurface(*surface_out);
    p_info->m_p_mfx_umc_frame_allocator = m_FrameAllocator.get();
    p_info->m_memId = memId;
    p_info->m_frame = frame;
    p_info->m_video_params = m_vInitPar;

    p_info->m_p_video_data = video_data;

    p_info->m_p_bitstream = NULL;
    p_info->m_p_bitstream = new mfxU8[m_bs.DataLength];
    MFX_CHECK_NULL_PTR1(p_info->m_p_bitstream);
    MFX_INTERNAL_CPY(p_info->m_p_bitstream, m_bs.Data, m_bs.DataLength);

    p_entry_point->pRoutine = &VP9DECODERoutine;
    p_entry_point->pCompleteProc = &VP9CompleteProc;
    p_entry_point->pState = m_vpx_codec;
    p_entry_point->requiredNumThreads = 1;
    p_entry_point->pParam = p_info;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP9::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME_VP9& frame)
{
    MFX_CHECK_NULL_PTR1(p_out);

    if (0 == p_in->DataLength)
        return MFX_ERR_MORE_DATA;

    mfxU8 *bs_start = p_in->Data + p_in->DataOffset;

    if (p_out->Data)
    {
        delete[] p_out->Data;
        p_out->DataLength = 0;
    }

    p_out->Data = new uint8_t[p_in->DataLength];

    MFX_INTERNAL_CPY(p_out->Data, bs_start, p_in->DataLength);
    p_out->DataLength = p_in->DataLength;
    p_out->DataOffset = 0;

    frame.frame_size = p_in->DataLength;

    MoveBitstreamData(*p_in, p_in->DataLength);

    return MFX_ERR_NONE;
}

void VideoDECODEVP9::SetOutputParams(mfxFrameSurface1 *surface_work)
{
    mfxFrameSurface1 *surface = surface_work;

    if (true == m_is_opaque_memory)
    {
        surface = m_core->GetOpaqSurface(surface_work->Data.MemId, true);
    }

    surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    surface->Info.FrameRateExtD = m_vInitPar.mfx.FrameInfo.FrameRateExtD;
    surface->Info.FrameRateExtN = m_vInitPar.mfx.FrameInfo.FrameRateExtN;

    surface->Info.CropX = 0;
    surface->Info.CropY = 0;

    //surface->Info.CropW = m_frameInfo.Width;
    //surface->Info.CropH = m_frameInfo.Height;

    surface->Info.PicStruct = m_vInitPar.mfx.FrameInfo.PicStruct;

    surface->Info.AspectRatioH = 1;
    surface->Info.AspectRatioW = 1;
}

#endif // MFX_ENABLE_VP9_VIDEO_DECODE

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_VP9_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)

namespace MFX_VP9_Utility {

mfxStatus DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* params)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(bs, params);

    sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    if (bs->DataLength < 3)
    {
        MoveBitstreamData(*bs, bs->DataLength);
        return MFX_ERR_MORE_DATA;
    }

    bool bHeaderRead = false;

    VP9DecoderFrame frame{};
    frame.bit_depth = 8;

    for (;;)
    {
        VP9Bitstream bsReader(bs->Data + bs->DataOffset,  bs->DataLength - bs->DataOffset);

        if (VP9_FRAME_MARKER != bsReader.GetBits(2))
            break; // invalid

        frame.profile = bsReader.GetBit();
        frame.profile |= bsReader.GetBit() << 1;
        if (frame.profile > 2)
            frame.profile += bsReader.GetBit();

        if (frame.profile >= 4)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if (bsReader.GetBit()) // show_existing_frame
            break;

        VP9_FRAME_TYPE frameType = (VP9_FRAME_TYPE) bsReader.GetBit();
        mfxU32 showFrame = bsReader.GetBit();
        mfxU32 errorResilientMode = bsReader.GetBit();

        if (KEY_FRAME == frameType)
        {
            if (!CheckSyncCode(&bsReader))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if (frame.profile >= 2)
            {
                frame.bit_depth = bsReader.GetBit() ? 12 : 10;
            }

            if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
            {
                bsReader.GetBit(); // color_range
                if (1 == frame.profile || 3 == frame.profile)
                {
                    frame.subsamplingX = bsReader.GetBit();
                    frame.subsamplingY = bsReader.GetBit();
                    bsReader.GetBit(); // reserved_zero
                }
                else
                {
                    frame.subsamplingX = 1;
                    frame.subsamplingY = 1;
                }
            }
            else
            {
                if (1 == frame.profile || 3 == frame.profile)
                    bsReader.GetBit();
                else
                    break; // invalid
            }

            frame.width = (mfxU16)bsReader.GetBits(16) + 1;
            frame.height = (mfxU16)bsReader.GetBits(16) + 1;

            bHeaderRead = true;
        }
        else
        {
            mfxU32 intraOnly = showFrame ? 0 : bsReader.GetBit();
            if (!intraOnly)
                break;

            if (!errorResilientMode)
                bsReader.GetBits(2);

            if (!CheckSyncCode(&bsReader))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if (frame.profile >= 2)
                frame.bit_depth = bsReader.GetBit() ? 12 : 10;

            if (frame.profile == 0)
            {
                // There is no color format info in intra-only frame for frame.profile 0
                frame.subsamplingX = 1;
                frame.subsamplingY = 1;
            }
            else // frame.profile > 0
            {
                if (SRGB != (COLOR_SPACE)bsReader.GetBits(3)) // color_space
                {
                    bsReader.GetBit(); // color_range
                    if (1 == frame.profile || 3 == frame.profile)
                    {
                        frame.subsamplingX = bsReader.GetBit();
                        frame.subsamplingY = bsReader.GetBit();
                        bsReader.GetBit(); // reserved_zero
                    }
                    else
                    {
                        frame.subsamplingX = 1;
                        frame.subsamplingY = 1;
                    }
                }
                else
                {
                    if (1 == frame.profile || 3 == frame.profile)
                        bsReader.GetBit();
                    else
                        break; // invalid
                }
            }

            bsReader.GetBits(NUM_REF_FRAMES);

            frame.width = (mfxU16)bsReader.GetBits(16) + 1;
            frame.height = (mfxU16)bsReader.GetBits(16) + 1;

            bHeaderRead = true;
        }

        break;
    }

    if (!bHeaderRead)
    {
        MoveBitstreamData(*bs, bs->DataLength);
        return MFX_ERR_MORE_DATA;
    }

    FillVideoParam(core, frame, params);

    return MFX_ERR_NONE;
}

mfxStatus FillVideoParam(VideoCORE* core, UMC_VP9_DECODER::VP9DecoderFrame const& frame, mfxVideoParam* params)
{
    MFX_CHECK_NULL_PTR2(core, params);

    params->mfx.CodecProfile = mfxU16(frame.profile + 1);

    params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    params->mfx.FrameInfo.AspectRatioW = 1;
    params->mfx.FrameInfo.AspectRatioH = 1;

    params->mfx.FrameInfo.CropX = 0;
    params->mfx.FrameInfo.CropY = 0;
    params->mfx.FrameInfo.CropW = static_cast<mfxU16>(frame.width);
    params->mfx.FrameInfo.CropH = static_cast<mfxU16>(frame.height);

    params->mfx.FrameInfo.Width  = UMC::align_value<mfxU16>(params->mfx.FrameInfo.CropW, 16);
    params->mfx.FrameInfo.Height = UMC::align_value<mfxU16>(params->mfx.FrameInfo.CropH, 16);

    if (!frame.subsamplingX && !frame.subsamplingY)
        params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    //else if (!subsampling_x && subsampling_y)
    //    params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV440;
    else if (frame.subsamplingX && !frame.subsamplingY)
        params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    else if (frame.subsamplingX && frame.subsamplingY)
        params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    switch (frame.bit_depth)
    {
        case  8:
            params->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            if (MFX_CHROMAFORMAT_YUV444 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            else if (MFX_CHROMAFORMAT_YUV422 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            params->mfx.FrameInfo.BitDepthLuma   = 8;
            params->mfx.FrameInfo.BitDepthChroma = 8;
            params->mfx.FrameInfo.Shift = 0;
            break;

        case 10:
            params->mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
#if (MFX_VERSION >= 1027)
            if (MFX_CHROMAFORMAT_YUV444 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            else if (MFX_CHROMAFORMAT_YUV422 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
#endif
            params->mfx.FrameInfo.BitDepthLuma   = 10;
            params->mfx.FrameInfo.BitDepthChroma = 10;
            break;

        case 12:
            params->mfx.FrameInfo.FourCC = 0;
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
            if (MFX_CHROMAFORMAT_YUV420 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            else if(MFX_CHROMAFORMAT_YUV444 == params->mfx.FrameInfo.ChromaFormat)
                params->mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
#endif //PRE_SI_TARGET_PLATFORM_GEN12
            params->mfx.FrameInfo.BitDepthLuma   = 12;
            params->mfx.FrameInfo.BitDepthChroma = 12;
            break;
    }

    if (core->GetPlatformType() == MFX_PLATFORM_HARDWARE)
    {
        if (   params->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
            || params->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
            || params->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416
#endif //PRE_SI_TARGET_PLATFORM_GEN12
        )
            params->mfx.FrameInfo.Shift = 1;
    }

    return MFX_ERR_NONE;
}

} //MFX_VP9_Utility

#endif // #if defined(MFX_ENABLE_VP9_VIDEO_DECODE) || defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW)
