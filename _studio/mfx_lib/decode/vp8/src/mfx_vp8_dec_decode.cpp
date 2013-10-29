/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.
//
*/

#include <limits>
#include "mfx_vp8_dec_decode.h"

#ifdef MFX_ENABLE_VP8_VIDEO_DECODE

#include "mfx_common.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"

#include "umc_data_pointers_copy.h"

#include "vm_sys_info.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippcc.h"

#include "mfx_thread_task.h"

#pragma warning(disable : 4505)

#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
#include "vpx_config.h"

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

void MoveBitstreamData2(mfxBitstream& bs, mfxU32 offset)
{
    VM_ASSERT(offset <= bs.DataLength);
    bs.DataOffset += offset;
    bs.DataLength -= offset;

} // void MoveBitstreamData2(mfxBitstream& bs, mfxU32 offset)

VideoDECODEVP8::VideoDECODEVP8(VideoCORE *p_core, mfxStatus *p_sts)
    :VideoDECODE()
    ,m_p_core(p_core)
    ,m_is_initialized(false)
    ,m_is_image_available(false)
    ,m_is_opaque_memory(false)
    ,m_platform(MFX_PLATFORM_SOFTWARE)
    ,m_p_video_accelerator(0)
    ,m_curr_decode_index(0)
    ,m_curr_display_index(0)
    ,m_num_output_frames(0)
    ,m_in_framerate(0)
{
    memset(&m_bs.Data, 0, sizeof(m_bs.Data));

    vm_mutex_set_invalid(&m_mutex);

    // allocate vpx decoder
    m_vpx_codec = ippMalloc(sizeof(vpx_codec_ctx_t));

    if (!m_vpx_codec) 
    {
        *p_sts = MFX_ERR_NOT_INITIALIZED;
    }

    if (p_sts)
    {
        *p_sts = MFX_ERR_NONE;
    }

} // VideoDECODEVP8::VideoDECODEVP8(VideoCORE *p_core, mfxStatus *p_sts)

VideoDECODEVP8::~VideoDECODEVP8(void)
{
    Close();

} // VideoDECODEVP8::~VideoDECODEVP8(void)

void VideoDECODEVP8::SetOutputParams(mfxFrameSurface1 *p_surface_work)
{
    mfxFrameSurface1 *p_surface = p_surface_work;

    if (true == m_is_opaque_memory)
    {
        p_surface = m_p_core->GetOpaqSurface(p_surface_work->Data.MemId, true);
    }

    p_surface->Data.TimeStamp = GetMfxTimeStamp(m_num_output_frames * m_in_framerate);
    p_surface->Data.FrameOrder = m_num_output_frames;
    m_num_output_frames += 1;

    p_surface->Info.FrameRateExtD = m_on_init_video_params.mfx.FrameInfo.FrameRateExtD;
    p_surface->Info.FrameRateExtN = m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;

    p_surface->Info.CropX = m_on_init_video_params.mfx.FrameInfo.CropX;
    p_surface->Info.CropY = m_on_init_video_params.mfx.FrameInfo.CropY;
    p_surface->Info.PicStruct = m_on_init_video_params.mfx.FrameInfo.PicStruct;

    p_surface->Info.AspectRatioH = 1;
    p_surface->Info.AspectRatioW = 1;

} // void VideoDECODEVP8::SetOutputParams(mfxFrameSurface1 *p_surface_work)

mfxStatus VideoDECODEVP8::Init(mfxVideoParam *p_params)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (true == m_is_initialized)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    MFX_CHECK_NULL_PTR1(p_params);

    // initialize the sync object
    if (VM_OK != vm_mutex_init(&m_mutex))
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    m_platform = MFX_PLATFORM_SOFTWARE;
    eMFXHWType type = MFX_HW_UNKNOWN;

    if (MFX_ERR_NONE > CheckVideoParamDecoders(p_params, m_p_core->IsExternalFrameAllocator(), type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VP8_Utility::CheckVideoParam(p_params, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    m_on_init_video_params = *p_params;
    m_init_w = p_params->mfx.FrameInfo.Width;
    m_init_h = p_params->mfx.FrameInfo.Height;

    if (0 == m_on_init_video_params.mfx.FrameInfo.FrameRateExtN || 0 == m_on_init_video_params.mfx.FrameInfo.FrameRateExtD)
    {
        m_on_init_video_params.mfx.FrameInfo.FrameRateExtD = 1000;
        m_on_init_video_params.mfx.FrameInfo.FrameRateExtN = 30000;
    }

    m_in_framerate = (mfxF64) m_on_init_video_params.mfx.FrameInfo.FrameRateExtD / m_on_init_video_params.mfx.FrameInfo.FrameRateExtN;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_on_init_video_params);
    m_video_params = m_on_init_video_params;

    if (0 == m_video_params.mfx.NumThread)
    {
        m_video_params.mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num();
    }

    m_p_frame_allocator_nv12.reset(new mfx_UMC_FrameAllocator_NV12);
    m_p_frame_allocator = m_p_frame_allocator_nv12;

    Ipp32s useInternal = (m_on_init_video_params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) || (m_on_init_video_params.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY);

    // allocate memory
    memset(&m_request, 0, sizeof(m_request));
    memset(&m_response, 0, sizeof(m_response));

    sts = QueryIOSurfInternal(m_platform, &m_video_params, &m_request);
    MFX_CHECK_STS(sts);

    if (useInternal)
    {
        m_request.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;
        
        mfxExtOpaqueSurfaceAlloc *p_opq_ext = NULL;
        if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & p_params->IOPattern) // opaque memory case
        {
            p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(p_params->ExtParam, p_params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

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
                    mfxStatus sts;
                    mfxFrameAllocRequest trequest = m_request;
                    trequest.Type =  (mfxU16)p_opq_ext->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)p_opq_ext->Out.NumSurface;

                    sts = m_p_core->AllocFrames(&trequest, 
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
            sts  = m_p_core->AllocFrames(&m_request,
                                         &m_response,
                                         p_opq_ext->Out.Surfaces,
                                         p_opq_ext->Out.NumSurface);
        }
        else
        {
            sts = m_p_core->AllocFrames(&m_request, &m_response);
            MFX_CHECK_STS(sts);
        }

        if (sts != MFX_ERR_NONE && sts != MFX_ERR_NOT_FOUND)
        {
            // second status means that external allocator was not found, it is ok
            return sts;
        }

        UMC::Status umcSts = UMC::UMC_OK;

        // no need to save surface descriptors if all memory is d3d
        if (p_params->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        {
            umcSts = m_p_frame_allocator->InitMfx(NULL, m_p_core, p_params, &m_request, &m_response, false, true);
        }
        else
        {
             // means that memory is d3d9 surfaces
            m_p_frame_allocator->SetExternalFramesResponse(&m_response);
            umcSts = m_p_frame_allocator->InitMfx(NULL, m_p_core, p_params, &m_request, &m_response, true, true);
        }

        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }
    else
    {
        m_request.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;

        UMC::Status umcSts = m_p_frame_allocator->InitMfx(0, m_p_core, p_params, &m_request, &m_response, !useInternal, MFX_PLATFORM_SOFTWARE == m_platform);
        if (UMC::UMC_OK != umcSts)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }
    }

    // init webm software decoder
    mfxI32 vpx_sts = 0;
    vpx_codec_dec_cfg_t cfg;
    cfg.threads = 8;
    
    vpx_sts = vpx_codec_dec_init((vpx_codec_ctx_t *)m_vpx_codec, vpx_codec_vp8_dx(), &cfg, 0); 
    CHECK_VPX_STATUS(vpx_sts);

    m_is_initialized = true;

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::Init(mfxVideoParam *p_params)

mfxStatus VideoDECODEVP8::Reset(mfxVideoParam *p_params)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(p_params);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_p_core->GetHWType();
    }

    if (CheckVideoParamDecoders(p_params, m_p_core->IsExternalFrameAllocator(), type) < MFX_ERR_NONE)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == MFX_VP8_Utility::CheckVideoParam(p_params, type))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (false == IsSameVideoParam(p_params, &m_on_init_video_params))
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

     mfxExtOpaqueSurfaceAlloc *p_opq_ext = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(p_params->ExtParam, p_params->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

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
    if (m_platform != m_p_core->GetPlatformType())
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (m_p_frame_allocator->Reset() != UMC::UMC_OK)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }

    memset(&m_decode_stat, 0, sizeof(m_decode_stat));
    m_on_init_video_params = *p_params;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_on_init_video_params);
    m_video_params = m_on_init_video_params;

    if (isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::Reset(mfxVideoParam *p_params)

mfxStatus VideoDECODEVP8::Close(void)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    m_p_frame_allocator->Close();

    if (0 != m_response.NumFrameActual)
    {
        m_p_core->FreeFrames(&m_response);
    }

    m_is_initialized = false;
    m_is_opaque_memory = false;

    m_p_video_accelerator = 0;
    m_curr_decode_index = 0;
    m_curr_display_index = 0;

    memset(&m_decode_stat, 0, sizeof(m_decode_stat));

    vpx_codec_destroy((vpx_codec_ctx_t *)m_vpx_codec);
    
    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::Close(void)

mfxTaskThreadingPolicy VideoDECODEVP8::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;

} // mfxTaskThreadingPolicy VideoDECODEVP8::GetThreadingPolicy(void)

mfxStatus VideoDECODEVP8::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)
{
    MFX_CHECK_NULL_PTR1(p_out);

    eMFXHWType type = p_core->GetHWType();
    return MFX_VP8_Utility::Query(p_core, p_in, p_out, type);

} // mfxStatus VideoDECODEVP8::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out)

mfxStatus VideoDECODEVP8::GetVideoParam(mfxVideoParam *p_params)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(p_params);

    MFX_INTERNAL_CPY(&p_params->mfx, &m_on_init_video_params.mfx, sizeof(mfxInfoMFX));

    p_params->Protected = m_on_init_video_params.Protected;
    p_params->IOPattern = m_on_init_video_params.IOPattern;
    p_params->AsyncDepth = m_on_init_video_params.AsyncDepth;

    if (!p_params->mfx.FrameInfo.FrameRateExtD && !p_params->mfx.FrameInfo.FrameRateExtN)
    {
        p_params->mfx.FrameInfo.FrameRateExtD = m_video_params.mfx.FrameInfo.FrameRateExtD;
        p_params->mfx.FrameInfo.FrameRateExtN = m_video_params.mfx.FrameInfo.FrameRateExtN;

        if (!p_params->mfx.FrameInfo.FrameRateExtD && !p_params->mfx.FrameInfo.FrameRateExtN)
        {
            p_params->mfx.FrameInfo.FrameRateExtN = 30;
            p_params->mfx.FrameInfo.FrameRateExtD = 1;
        }
    }

    if (!p_params->mfx.FrameInfo.AspectRatioH && !p_params->mfx.FrameInfo.AspectRatioW)
    {
        p_params->mfx.FrameInfo.AspectRatioH = m_video_params.mfx.FrameInfo.AspectRatioH;
        p_params->mfx.FrameInfo.AspectRatioW = m_video_params.mfx.FrameInfo.AspectRatioW;

        if (!p_params->mfx.FrameInfo.AspectRatioH && !p_params->mfx.FrameInfo.AspectRatioW)
        {
            p_params->mfx.FrameInfo.AspectRatioH = 1;
            p_params->mfx.FrameInfo.AspectRatioW = 1;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::GetVideoParam(mfxVideoParam *p_params)

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

mfxStatus VideoDECODEVP8::DecodeHeader(VideoCORE *p_core, mfxBitstream *p_bs, mfxVideoParam *p_params)
{
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK_NULL_PTR2(p_bs, p_params);

    sts = CheckBitstream(p_bs);
    MFX_CHECK_STS(sts);

    if (p_bs->DataLength < 3)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU8 *p_bitstream = p_bs->Data + p_bs->DataOffset;
    mfxU8 *p_bitstream_end = p_bs->Data + p_bs->DataOffset + p_bs->DataLength;

    mfxU32 n_bytes_offset = 0;
    bool start_code_found = false;

    while (p_bitstream < p_bitstream_end)
    {
        if (VP8_START_CODE_FOUND(p_bitstream)) // (0x9D && 0x01 && 0x2A)
        {
            start_code_found = true;
            // move to frame tag
            p_bitstream -= 3;
            n_bytes_offset -= 3;
            break;
        }

        p_bitstream += 1;
        n_bytes_offset += 1;
    }

    if (false == start_code_found)
    {
        // set offset, but leave last six bytes 
        // since they can be start bytes of start code and frame tag

        MoveBitstreamData2(*p_bs, n_bytes_offset - 6);

        return MFX_ERR_MORE_DATA;
    }

    FrameType frame_type;
    mfxU16 width, height;

    frame_type = (p_bitstream[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits

    p_params->mfx.CodecProfile = ((p_bitstream[0] >> 1) & 0x7) + 1;

    if (I_PICTURE != frame_type)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU32 first_partion_size = (p_bitstream[0] >> 5) | // 19 bit
                                (p_bitstream[1] << 3) |
                                (p_bitstream[2] << 11);

    
    if (p_bitstream + first_partion_size > p_bitstream_end)
    {
        return MFX_ERR_MORE_DATA; 
    }

    // move to start code
    p_bitstream += 3;

    width = ((p_bitstream[4] << 8) | p_bitstream[3]) & 0x3fff;
    height = ((p_bitstream[6] << 8) | p_bitstream[5]) & 0x3fff;

    p_params->mfx.FrameInfo.AspectRatioW = 1; // pBitstream[4] >> 6;
    p_params->mfx.FrameInfo.AspectRatioH = 1; // pBitstream[6] >> 6;

    p_params->mfx.FrameInfo.CropW = width;
    p_params->mfx.FrameInfo.CropH = height;

    p_params->mfx.FrameInfo.Width = (p_params->mfx.FrameInfo.CropW + 15) & ~0x0f;
    p_params->mfx.FrameInfo.Height = (p_params->mfx.FrameInfo.CropH + 15) & ~0x0f;

    p_params->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    p_params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    MoveBitstreamData2(*p_bs, n_bytes_offset);

    eMFXPlatform platform = MFX_VP8_Utility::GetPlatform(p_core, p_params);

    if (platform != p_core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);

        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::DecodeHeader(VideoCORE *p_core, mfxBitstream *p_bs, mfxVideoParam *p_params)

mfxStatus VideoDECODEVP8::PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work)
{
    mfxU8 *p_bitstream = p_bs->Data + p_bs->DataOffset;
    mfxU8 *p_bitstream_end = p_bs->Data + p_bs->DataOffset + p_bs->DataLength;

    while (p_bitstream < p_bitstream_end)
    {
        if (VP8_START_CODE_FOUND(p_bitstream)) // (0x9D && 0x01 && 0x2A)
        {
            break;
        }

        p_bitstream += 1;
    }

    mfxU32 width, height;

    width = ((p_bitstream[4] << 8) | p_bitstream[3]) & 0x3fff;
    height = ((p_bitstream[6] << 8) | p_bitstream[5]) & 0x3fff;

    width = (width + 15) & ~0x0f;
    height = (height + 15) & ~0x0f;

    mfxFrameSurface1 *p_surface = p_surface_work;

    if (true == m_is_opaque_memory)
    {
        p_surface = m_p_core->GetOpaqSurface(p_surface_work->Data.MemId, true);
    }

    if (0 == p_surface->Info.CropW)
    {
        p_surface->Info.CropW = m_on_init_video_params.mfx.FrameInfo.Width;
    }

    if (0 == p_surface->Info.CropH)
    {
        p_surface->Info.CropH = m_on_init_video_params.mfx.FrameInfo.Height;
    }

    if (m_init_w != width && m_init_h != height)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (p_surface->Info.Width < width || p_surface->Info.Height < height)
    {
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::PreDecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface)

mfxStatus VideoDECODEVP8::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)
{
    MFX_CHECK_NULL_PTR2(p_params, p_request);

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

    eMFXPlatform platform = MFX_VP8_Utility::GetPlatform(p_core, p_params);

    eMFXHWType type = MFX_HW_UNKNOWN;
    if (platform == MFX_PLATFORM_HARDWARE)
    {
        type = p_core->GetHWType();
    }

    mfxVideoParam params;
    MFX_INTERNAL_CPY(&params, p_params, sizeof(mfxVideoParam));
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&params);

    mfxStatus sts = QueryIOSurfInternal(platform, &params, p_request);
    MFX_CHECK_STS(sts);

    Ipp32s isInternalManaging = (MFX_PLATFORM_SOFTWARE == platform) ?
        (params.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    if (isInternalManaging)
    {
        p_request->NumFrameSuggested = p_request->NumFrameMin = 4;
    }

    if (params.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    if (platform != p_core->GetPlatformType())
    {
        VM_ASSERT(platform == MFX_PLATFORM_SOFTWARE);
        return MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (true == isNeedChangeVideoParamWarning)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::QueryIOSurf(VideoCORE *p_core, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)

mfxStatus VideoDECODEVP8::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)
{
    MFX_INTERNAL_CPY(&p_request->Info, &p_params->mfx.FrameInfo, sizeof(mfxFrameInfo));

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
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (p_params->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else // opaque memory case
    {
        if (MFX_PLATFORM_SOFTWARE == platform)
        {
            p_request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        }
        else
        {
            p_request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        }
    }

    p_request->NumFrameMin = mfxU16 (4);

    p_request->NumFrameMin += p_params->AsyncDepth ? p_params->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE;
    p_request->NumFrameSuggested = p_request->NumFrameMin;

    if (MFX_PLATFORM_SOFTWARE == platform)
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else
    {
        p_request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::QueryIOSurfInternal(eMFXPlatform platform, mfxVideoParam *p_params, mfxFrameAllocRequest *p_request)

mfxStatus VideoDECODEVP8::GetDecodeStat(mfxDecodeStat *p_stat)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(p_stat);

    m_decode_stat.NumSkippedFrame = 0;
    m_decode_stat.NumCachedFrame = 0;

    MFX_INTERNAL_CPY(p_stat, &m_decode_stat, sizeof(m_decode_stat));

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::GetDecodeStat(mfxDecodeStat *p_stat)

static mfxStatus VP8CompleteProc(void *, void *p_param, mfxStatus )
{
    delete p_param;
    return MFX_ERR_NONE;

} // static mfxStatus VP8CompleteProc(void *, void *p_param, mfxStatus )

mfxStatus VideoDECODEVP8::RunThread(void *p_params, mfxU32 thread_number)
{
    p_params; thread_number;

    return MFX_TASK_DONE;

} // mfxStatus VideoDECODEVP8::RunThread(void *p_params, mfxU32 thread_number)

static mfxStatus __CDECL VP8DECODERoutine(void *p_state, void *p_param, mfxU32 /*thread_number*/, mfxU32)
{
    THREAD_TASK_INFO *p_thread_info = (THREAD_TASK_INFO *) p_param;

    p_state;

    vpx_codec_ctx_t *p_decoder = (vpx_codec_ctx_t *)p_state;

    mfxI32 vpx_sts = 0;
    vpx_sts = vpx_codec_decode(p_decoder, p_thread_info->m_p_bitstream, p_thread_info->m_frame.frame_size, NULL, 0);
    CHECK_VPX_STATUS(vpx_sts);

    vpx_codec_iter_t iter = NULL;
    vpx_image_t      *img = vpx_codec_get_frame(p_decoder, &iter);

    if (img)
    {
        Ipp8u *p_data = (Ipp8u *) p_thread_info->m_p_video_data->GetPlanePointer(0);
        Ipp32u pitch = (Ipp32u) p_thread_info->m_p_video_data->GetPlanePitch(0);

        unsigned char *buf = img->planes[0];

        for (unsigned int j = 0; j < img->d_h; j += 1)
        {
            MFX_INTERNAL_CPY(p_data, buf, img->d_w);
            p_data += pitch;
            buf += img->stride[0];
        }

        p_data = (Ipp8u *)p_thread_info->m_p_video_data->GetPlanePointer(1);
        pitch = (Ipp32u)p_thread_info->m_p_video_data->GetPlanePitch(1);

        buf = img->planes[1];

        for (unsigned int j = 0; j < (img->d_h + 1) >> 1; j += 1)
        {
            MFX_INTERNAL_CPY(p_data, buf, (img->d_w + 1) >> 1);
            p_data += pitch;
            buf += img->stride[1];
        }

        p_data = (Ipp8u *)p_thread_info->m_p_video_data->GetPlanePointer(2);
        pitch = (Ipp32u) p_thread_info->m_p_video_data->GetPlanePitch(2);

        buf = img->planes[2];
        
        for (unsigned int j = 0; j < (img->d_h + 1) >> 1; j += 1)
        {
            MFX_INTERNAL_CPY(p_data, buf, (img->d_w + 1) >> 1);
            p_data += pitch;
            buf += img->stride[2];
        }

        p_thread_info->m_p_mfx_umc_frame_allocator->PrepareToOutput(p_thread_info->m_p_surface_out, p_thread_info->m_memId, &p_thread_info->m_video_params, false);
        p_thread_info->m_p_mfx_umc_frame_allocator->DecreaseReference(p_thread_info->m_memId);
        p_thread_info->m_p_mfx_umc_frame_allocator->Unlock(p_thread_info->m_memId);
    }

    delete [] p_thread_info->m_p_bitstream;
    delete p_thread_info->m_p_video_data;

    return MFX_ERR_NONE;

} // static mfxStatus __CDECL VP8DECODERoutine(void *p_state, void *p_param, mfxU32 thread_number, mfxU32)

mfxStatus VideoDECODEVP8::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT *p_entry_point)
{
    mfxStatus sts = MFX_ERR_NONE;

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR2(p_surface_work, pp_surface_out);

    if (0 != p_surface_work->Data.Locked)
    {
        return MFX_ERR_MORE_SURFACE;
    }
   
    if (true == m_is_opaque_memory)
    {
        if (p_surface_work->Data.MemId || p_surface_work->Data.Y || p_surface_work->Data.R || p_surface_work->Data.A || p_surface_work->Data.UV) // opaq surface
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        p_surface_work = GetOriginalSurface(p_surface_work);
    }

    sts = CheckFrameInfoCodecs(&p_surface_work->Info, MFX_CODEC_VP8);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(p_surface_work);
    MFX_CHECK_STS(sts);

    sts = p_bs ? CheckBitstream(p_bs) : MFX_ERR_NONE;
    MFX_CHECK_STS(sts);

    if (NULL == p_bs)
    {
        return MFX_ERR_MORE_DATA;
    }

    bool show_frame;
    FrameType frame_type;

    if (0 == p_bs->DataLength)
    {
        return MFX_ERR_MORE_DATA;
    }

    mfxU8 *pTemp = p_bs->Data + p_bs->DataOffset;
    frame_type = (pTemp[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits
    show_frame = (pTemp[0] >> 4) & 0x1;

    if (I_PICTURE == frame_type)
    {
        sts = PreDecodeFrame(p_bs, p_surface_work);
        MFX_CHECK_STS(sts);
    }

    IVF_FRAME frame;
    memset(&frame, 0, sizeof(IVF_FRAME));

    sts = ConstructFrame(p_bs, &m_bs, frame);
    MFX_CHECK_STS(sts);

    *pp_surface_out = 0;

    sts = m_p_frame_allocator->SetCurrentMFXSurface(p_surface_work, m_is_opaque_memory);
    MFX_CHECK_STS(sts);

    UMC::VideoDataInfo info;
    info.Init(p_surface_work->Info.Width, (p_surface_work->Info.CropH + 15) & ~0x0f, YUV420);
    
    UMC::FrameMemID memId = 0;

    UMC::FrameData *p_frame_data = NULL;
    UMC::VideoData *video_data = new UMC::VideoData;

    if (true == show_frame)
    {
        m_p_frame_allocator->Alloc(&memId, &info, 0);

        // get to decode frame data
        p_frame_data = (UMC::FrameData *) m_p_frame_allocator->Lock(memId);

        if (NULL == p_frame_data)
        {
            delete video_data;
            return MFX_ERR_LOCK_MEMORY;
        }

        m_p_frame_allocator->IncreaseReference(memId);
        
        UMC::Status umcSts = video_data->Init(p_surface_work->Info.Width, p_surface_work->Info.Height, UMC::YUV420);
        umcSts;

        const UMC::FrameData::PlaneMemoryInfo *p_info;

        p_info = p_frame_data->GetPlaneMemoryInfo(0);

        video_data->SetPlanePointer(p_info->m_planePtr, 0);
        Ipp32u pitch = (Ipp32u) p_info->m_pitch;
        
        p_info = p_frame_data->GetPlaneMemoryInfo(1);
        video_data->SetPlanePointer(p_info->m_planePtr, 1);
        
        p_info = p_frame_data->GetPlaneMemoryInfo(2);
        
        video_data->SetPlanePointer(p_info->m_planePtr, 2);

        video_data->SetPlanePitch(pitch, 0);
        video_data->SetPlanePitch(pitch / 2, 1);
        video_data->SetPlanePitch(pitch / 2, 2);

        // get output surface
        sts = GetOutputSurface(pp_surface_out, p_surface_work, memId);
        MFX_CHECK_STS(sts);
            //*pp_surface_out = m_p_frame_allocator->GetSurface(memId, p_surface_work, &m_video_params);

        SetOutputParams(p_surface_work);
    }

    THREAD_TASK_INFO *p_info = new THREAD_TASK_INFO;
    p_info->m_p_surface_work = p_surface_work;
    p_info->m_p_surface_out = GetOriginalSurface(*pp_surface_out);
    p_info->m_p_mfx_umc_frame_allocator = m_p_frame_allocator.get();
    p_info->m_memId = memId;
    p_info->m_frame = frame;
    p_info->m_p_mutex = &m_mutex;
    p_info->m_video_params = m_video_params;

    p_info->m_p_video_data = video_data;

    p_info->m_p_bitstream = NULL;
    p_info->m_p_bitstream = new mfxU8[m_bs.DataLength];
    MFX_INTERNAL_CPY(p_info->m_p_bitstream, m_bs.Data, m_bs.DataLength);

    p_entry_point->pRoutine = &VP8DECODERoutine;
    p_entry_point->pCompleteProc = &VP8CompleteProc;
    p_entry_point->pState = m_vpx_codec;
    p_entry_point->requiredNumThreads = 1;
    p_entry_point->pParam = p_info;

    if (false == show_frame)
    {
        return MFX_ERR_MORE_SURFACE;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::DecodeFrameCheck(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 **pp_surface_out, MFX_ENTRY_POINT *p_entry_point)

mfxStatus VideoDECODEVP8::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)
{
    bs; surface_work; surface_out;

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out)

mfxFrameSurface1 * VideoDECODEVP8::GetOriginalSurface(mfxFrameSurface1 *p_surface)
{
    if (true == m_is_opaque_memory)
    {
        return m_p_core->GetNativeSurface(p_surface);
    }

    return p_surface;

} // mfxFrameSurface1 * VideoDECODEVP8::GetOriginalSurface(mfxFrameSurface1 *p_surface)


mfxStatus VideoDECODEVP8::GetOutputSurface(mfxFrameSurface1 **pp_surface_out, mfxFrameSurface1 *p_surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *p_native_surface =  m_p_frame_allocator->GetSurface(index, p_surface_work, &m_on_init_video_params);
    
    if (p_native_surface)
    {
        *pp_surface_out = m_p_core->GetOpaqSurface(p_native_surface->Data.MemId) ? m_p_core->GetOpaqSurface(p_native_surface->Data.MemId) : p_native_surface;
    }
    else
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEVP8::GetUserData(mfxU8 *p_ud, mfxU32 *p_sz, mfxU64 *p_ts)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR3(p_ud, p_sz, p_ts);

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VideoDECODEVP8::GetUserData(mfxU8 *p_ud, mfxU32 *p_sz, mfxU64 *p_ts)

mfxStatus VideoDECODEVP8::GetPayload(mfxSession , mfxU64 *p_ts, mfxPayload *p_payload)
{
    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR3(p_ts, p_payload, p_payload->Data);

    return MFX_ERR_UNSUPPORTED;

} // mfxStatus VideoDECODEVP8::GetPayload(mfxSession , mfxU64 *p_ts, mfxPayload *p_payload)

mfxStatus VideoDECODEVP8::SetSkipMode(mfxSkipMode mode)
{
    mode;

    if (false == m_is_initialized)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::SetSkipMode(mfxSkipMode mode)

bool VideoDECODEVP8::IsSameVideoParam(mfxVideoParam *p_new_par, mfxVideoParam *p_old_par)
{
    if (p_new_par->IOPattern != p_old_par->IOPattern)
    {
        return false;
    }

    if (p_new_par->Protected != p_old_par->Protected)
    {
        return false;
    }

    Ipp32s asyncDepth = IPP_MIN(p_new_par->AsyncDepth, MFX_MAX_ASYNC_DEPTH_VALUE);
    if (asyncDepth != p_old_par->AsyncDepth)
    {
        return false;
    }

    if (p_new_par->mfx.FrameInfo.Height != p_old_par->mfx.FrameInfo.Height)
    {
        return false;
    }

    if (p_new_par->mfx.FrameInfo.Width != p_old_par->mfx.FrameInfo.Width)
    {
        return false;
    }

    if (p_new_par->mfx.FrameInfo.ChromaFormat != p_old_par->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (p_new_par->mfx.NumThread > p_old_par->mfx.NumThread && p_old_par->mfx.NumThread > 0)
    {
        return false;
    }

    return true;

} // bool VideoDECODEVP8::IsSameVideoParam(mfxVideoParam *p_new_par, mfxVideoParam *p_old_par)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_VP8_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool MFX_VP8_Utility::IsNeedPartialAcceleration(mfxVideoParam *p_params)
{
    if (!p_params)
        return false;

    if (p_params->mfx.FrameInfo.Width > 4096 || p_params->mfx.FrameInfo.Height > 4096)
        return true;

    return false;

} // bool MFX_VP8_Utility::IsNeedPartialAcceleration(mfxVideoParam *p_params)

eMFXPlatform MFX_VP8_Utility::GetPlatform(VideoCORE *p_core, mfxVideoParam *p_params)
{
    eMFXPlatform platform = p_core->GetPlatformType();

    if (p_params && IsNeedPartialAcceleration(p_params) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

    return platform;

} // eMFXPlatform MFX_VP8_Utility::GetPlatform(VideoCORE *p_core, mfxVideoParam *p_params)

mfxStatus MFX_VP8_Utility::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(p_out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (p_in == p_out)
    {
        mfxVideoParam in1;
        MFX_INTERNAL_CPY(&in1, p_in, sizeof(mfxVideoParam));
        return Query(p_core, &in1, p_out, type);
    }

    memset(&p_out->mfx, 0, sizeof(mfxInfoMFX));

    if (p_in)
    {
        if (p_in->mfx.CodecId == MFX_CODEC_VP8)
            p_out->mfx.CodecId = p_in->mfx.CodecId;

        switch (p_in->mfx.CodecLevel)
        {
            case MFX_LEVEL_UNKNOWN:
                p_out->mfx.CodecLevel = p_in->mfx.CodecLevel;
                break;
        }

        if (p_in->mfx.NumThread < 128)
            p_out->mfx.NumThread = p_in->mfx.NumThread;

        if (p_in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            p_out->AsyncDepth = p_in->AsyncDepth;

        if ((p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            if ( !((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) )
                p_out->IOPattern = p_in->IOPattern;
        }

        if (p_in->mfx.FrameInfo.FourCC)
        {
            // mfxFrameInfo
            if (p_in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
                p_out->mfx.FrameInfo.FourCC = p_in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        switch(p_in->mfx.FrameInfo.ChromaFormat)
        {
            case MFX_CHROMAFORMAT_YUV420:
                p_out->mfx.FrameInfo.ChromaFormat = p_in->mfx.FrameInfo.ChromaFormat;
                break;
            default:
                if (p_in->mfx.FrameInfo.FourCC)
                    sts = MFX_ERR_UNSUPPORTED;

                break;
        }

        if (!p_in->mfx.FrameInfo.ChromaFormat && !(!p_in->mfx.FrameInfo.FourCC && !p_in->mfx.FrameInfo.ChromaFormat))
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Width % 16 == 0 && p_in->mfx.FrameInfo.Width <= 4096)
            p_out->mfx.FrameInfo.Width = p_in->mfx.FrameInfo.Width;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Height % 16 == 0 && p_in->mfx.FrameInfo.Height <= 2304)
            p_out->mfx.FrameInfo.Height = p_in->mfx.FrameInfo.Height;
        else
            sts = MFX_ERR_UNSUPPORTED;

        /*if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;

        if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

        if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;*/

        p_out->mfx.FrameInfo.FrameRateExtN = p_in->mfx.FrameInfo.FrameRateExtN;
        p_out->mfx.FrameInfo.FrameRateExtD = p_in->mfx.FrameInfo.FrameRateExtD;

        p_out->mfx.FrameInfo.AspectRatioW = p_in->mfx.FrameInfo.AspectRatioW;
        p_out->mfx.FrameInfo.AspectRatioH = p_in->mfx.FrameInfo.AspectRatioH;

        mfxStatus stsExt = CheckDecodersExtendedBuffers(p_in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->Protected)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        mfxExtOpaqueSurfaceAlloc *opaque_in = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_in->ExtParam, p_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc *opaque_out = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (opaque_in && opaque_out)
        {
            opaque_out->In.Type = opaque_in->In.Type;
            opaque_out->In.NumSurface = opaque_in->In.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->In.Surfaces, opaque_in->In.Surfaces, opaque_in->In.NumSurface);

            opaque_out->Out.Type = opaque_in->Out.Type;
            opaque_out->Out.NumSurface = opaque_in->Out.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->Out.Surfaces, opaque_in->Out.Surfaces, opaque_in->Out.NumSurface);
        }
        else
        {
            if (opaque_out || opaque_in)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (GetPlatform(p_core, p_out) != p_core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform(p_core, p_out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        p_out->mfx.CodecId = MFX_CODEC_VP8;
        p_out->mfx.CodecProfile = 1;
        p_out->mfx.CodecLevel = 1;

        p_out->mfx.NumThread = 1;

        p_out->AsyncDepth = 1;

        // mfxFrameInfo
        p_out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        p_out->mfx.FrameInfo.Width = 16;
        p_out->mfx.FrameInfo.Height = 16;

        p_out->mfx.FrameInfo.FrameRateExtN = 1;
        p_out->mfx.FrameInfo.FrameRateExtD = 1;

        p_out->mfx.FrameInfo.AspectRatioW = 1;
        p_out->mfx.FrameInfo.AspectRatioH = 1;

        p_out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }
    }

    return sts;

} // mfxStatus MFX_VP8_Utility::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out, eMFXHWType type)

bool MFX_VP8_Utility::CheckVideoParam(mfxVideoParam *p_in, eMFXHWType )
{
    if (!p_in)
        return false;

    if (p_in->Protected)
       return false;

    if (MFX_CODEC_VP8 != p_in->mfx.CodecId)
        return false;

    if (p_in->mfx.FrameInfo.Width > 4096 || (p_in->mfx.FrameInfo.Width % 16))
        return false;

    if (p_in->mfx.FrameInfo.Height > 4096 || (p_in->mfx.FrameInfo.Height % 16))
        return false;

#if 0
    // ignore Crop paramsameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    if (MFX_FOURCC_NV12 != p_in->mfx.FrameInfo.FourCC)
        return false;

    // both zero or not zero
    if ((p_in->mfx.FrameInfo.AspectRatioW || p_in->mfx.FrameInfo.AspectRatioH) && !(p_in->mfx.FrameInfo.AspectRatioW && p_in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (p_in->mfx.FrameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
            break;

        default:
            return false;
    }

    if (MFX_CHROMAFORMAT_YUV420 != p_in->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (!(p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    return true;

} // bool MFX_VP8_Utility::CheckVideoParam(mfxVideoParam *p_in, eMFXHWType)

mfxStatus VideoDECODEVP8::DecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 *p_surface_out)
{
    p_bs = p_bs;
    p_surface_work = p_surface_work;
    p_surface_out = p_surface_out;

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::DecodeFrame(mfxBitstream *p_bs, mfxFrameSurface1 *p_surface_work, mfxFrameSurface1 *p_surface_out)

mfxStatus VideoDECODEVP8::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME& frame)
{
    MFX_CHECK_NULL_PTR1(p_out);

    if (0 == p_in->DataLength)
        return MFX_ERR_MORE_DATA;

    mfxU8 *p_bs_start = p_in->Data + p_in->DataOffset;

    if (p_out->Data)
    {
        ippFree(p_out->Data);
        p_out->DataLength = 0;
    }

    p_out->Data = (Ipp8u*)ippMalloc(p_in->DataLength);

    ippsCopy_8u(p_bs_start, p_out->Data, p_in->DataLength);
    p_out->DataLength = p_in->DataLength;
    p_out->DataOffset = 0;

    frame.frame_size = p_in->DataLength;

    MoveBitstreamData2(*p_in, p_in->DataLength);

    return MFX_ERR_NONE;

} // mfxStatus VideoDECODEVP8::ConstructFrame(mfxBitstream *p_in, mfxBitstream *p_out, IVF_FRAME& frame)

#endif // MFX_ENABLE_VP8_VIDEO_DECODE
