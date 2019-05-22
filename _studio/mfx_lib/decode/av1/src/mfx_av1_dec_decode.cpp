// Copyright (c) 2017-2019 Intel Corporation
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

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)

#include "mfx_session.h"
#include "mfx_av1_dec_decode.h"

#include "mfx_task.h"

#include "mfx_common_int.h"
#include "mfx_common_decode_int.h"
#include "mfx_vpx_dec_common.h"

#include "mfx_umc_alloc_wrapper.h"

#include "umc_av1_dec_defs.h"
#include "umc_av1_frame.h"

#include "libmfx_core_hw.h"
#include "umc_va_dxva2.h"

#include "umc_av1_decoder_va.h"

#include <algorithm>

namespace MFX_VPX_Utility
{
    inline
    mfxU16 MatchProfile(mfxU32 fourcc)
    {
        fourcc;
        return MFX_PROFILE_AV1_MAIN;
    }

    const GUID DXVA_Intel_ModeAV1_VLD =
        { 0xca44afc5, 0xe1d0, 0x42e6, { 0x91, 0x54, 0xb1, 0x27, 0x18, 0x6d, 0x4d, 0x40 } };

    inline
    bool CheckGUID(VideoCORE* core, eMFXHWType type, mfxVideoParam const* par)
    {
        mfxVideoParam vp = *par;
        mfxU16 profile = vp.mfx.CodecProfile & 0xFF;
        if (profile == MFX_PROFILE_UNKNOWN)
        {
            profile = MatchProfile(vp.mfx.FrameInfo.FourCC);;
            vp.mfx.CodecProfile = profile;
        }

#if defined (MFX_VA_WIN)
        mfxU32 const va_profile =
            ChooseProfile(&vp, type) & (UMC::VA_CODEC | UMC::VA_ENTRY_POINT);

        GuidProfile const
            *f =     GuidProfile::GetGuidProfiles(),
            *l = f + GuidProfile::GetGuidProfileSize();
        GuidProfile const* p =
            std::find_if(f, l,
                [&](GuidProfile const& candidate)
                {
                    return
                        (static_cast<mfxU32>(candidate.profile) & (UMC::VA_CODEC | UMC::VA_ENTRY_POINT)) == va_profile &&
                        core->IsGuidSupported(candidate.guid, &vp) == MFX_ERR_NONE;
                }
            );

        return p != l;
#elif defined (MFX_VA_LINUX)
        if (core->IsGuidSupported(DXVA_Intel_ModeAV1_VLD, &vp) != MFX_ERR_NONE)
            return false;

        //Linux doesn't check GUID, just [mfxVideoParam]
        switch (profile)
        {
            case MFX_PROFILE_AV1_MAIN:
                return true;
            default: return false;
        }
#else
        core; type; par;
        return false;
#endif
    }

    eMFXPlatform GetPlatform(VideoCORE* core, mfxVideoParam const* par)
    {
        VM_ASSERT(core);
        VM_ASSERT(par);

        if (!par)
            return MFX_PLATFORM_SOFTWARE;

        eMFXPlatform platform = core->GetPlatformType();
        return
            platform != MFX_PLATFORM_SOFTWARE && !CheckGUID(core, core->GetHWType(), par) ?
            MFX_PLATFORM_SOFTWARE : platform;
    }
}

static void SetFrameType(const UMC_AV1_DECODER::AV1DecoderFrame& frame, mfxFrameSurface1 &surface_out)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface_out.Data.ExtParam, surface_out.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    const UMC_AV1_DECODER::FrameHeader& fh = frame.GetFrameHeader();
    switch (fh.frame_type)
    {
    case UMC_AV1_DECODER::KEY_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_I;
        break;
    case UMC_AV1_DECODER::INTER_FRAME:
    case UMC_AV1_DECODER::INTRA_ONLY_FRAME:
    case UMC_AV1_DECODER::SWITCH_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_P;
        break;
    default:
        extFrameInfo->FrameType = MFX_FRAMETYPE_UNKNOWN;
    }
}

VideoDECODEAV1::VideoDECODEAV1(VideoCORE* core, mfxStatus* sts)
    : m_core(core)
    , m_platform(MFX_PLATFORM_SOFTWARE)
    , m_opaque(false)
    , m_first_run(true)
    , m_response()
{
    if (sts)
    {
        *sts = MFX_ERR_NONE;
    }
}

VideoDECODEAV1::~VideoDECODEAV1()
{
    Close();
}

mfxStatus VideoDECODEAV1::Init(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(!m_decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_platform = MFX_VPX_Utility::GetPlatform(m_core, par);
    eMFXHWType type = MFX_HW_UNKNOWN;
    if (m_platform == MFX_PLATFORM_HARDWARE)
    {
        type = m_core->GetHWType();
    }

    MFX_CHECK(CheckVideoParamDecoders(par, m_core->IsExternalFrameAllocator(), type) >= MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(MFX_VPX_Utility::CheckVideoParam(par, MFX_CODEC_AV1, m_platform), MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(m_platform != MFX_PLATFORM_SOFTWARE, MFX_ERR_UNSUPPORTED);
#if !defined (MFX_VA)
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
#else
    m_decoder.reset(new UMC_AV1_DECODER::AV1DecoderVA());
    m_allocator.reset(new mfx_UMC_FrameAllocator_D3D());
#endif

    mfxFrameAllocRequest request{};
    mfxStatus sts = MFX_VPX_Utility::QueryIOSurfInternal(par, &request);
    MFX_CHECK_STS(sts);

    //mfxFrameAllocResponse response{};
    bool internal = ((m_platform == MFX_PLATFORM_SOFTWARE) ?
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) : (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY));

    if (!(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
    {
        if (!internal)
            request.AllocId = par->AllocId;

        sts = m_core->AllocFrames(&request, &m_response, internal);
    }
    else
    {
        auto opaq =
            reinterpret_cast<mfxExtOpaqueSurfaceAlloc*>(GetExtendedBuffer(par->ExtParam, par->NumExtParam,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION));

        MFX_CHECK(opaq && request.NumFrameMin <= opaq->Out.NumSurface, MFX_ERR_INVALID_VIDEO_PARAM);

        m_opaque = true;

        request.Type = MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_OPAQUE_FRAME;
        request.Type |= (opaq->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) ?
            MFX_MEMTYPE_SYSTEM_MEMORY : MFX_MEMTYPE_DXVA2_DECODER_TARGET;

        request.NumFrameMin       = opaq->Out.NumSurface;
        request.NumFrameSuggested = request.NumFrameMin;

        sts = m_core->AllocFrames(&request, &m_response,
            opaq->Out.Surfaces, opaq->Out.NumSurface);
    }

    MFX_CHECK_STS(sts);

    m_allocator->SetExternalFramesResponse(&m_response);

    UMC::Status umcSts = m_allocator->InitMfx(0, m_core, par, &request, &m_response, !internal, m_platform == MFX_PLATFORM_SOFTWARE);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    UMC_AV1_DECODER::AV1DecoderParams vp{};
    vp.allocator = m_allocator.get();
    vp.async_depth = par->AsyncDepth;
    vp.film_grain = par->mfx.FilmGrain ? 1 : 0; // 0 - film grain is forced off, 1 - film grain is controlled by apply_grain syntax parameter
    if (!vp.async_depth)
        vp.async_depth = MFX_AUTO_ASYNC_DEPTH_VALUE;

#if defined (MFX_VA)
    sts = m_core->CreateVA(par, &request, &m_response, m_allocator.get());
    MFX_CHECK_STS(sts);

    m_core->GetVA((mfxHDL*)&vp.pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);
#endif

    ConvertMFXParamsToUMC(par, &vp);

    umcSts = m_decoder->Init(&vp);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_NOT_INITIALIZED);

    m_first_run = true;
    m_init_video_par = *par;

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::Reset(mfxVideoParam* par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::Close()
{
    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    return MFX_ERR_NONE;
}

mfxTaskThreadingPolicy VideoDECODEAV1::GetThreadingPolicy()
{
    return MFX_TASK_THREADING_SHARED;
}

mfxStatus VideoDECODEAV1::Query(VideoCORE* core, mfxVideoParam* in, mfxVideoParam* out)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR1(out);

    mfxStatus sts = MFX_VPX_Utility::Query(core, in, out, MFX_CODEC_AV1, core->GetHWType());
    if (sts != MFX_ERR_NONE)
        return sts;

    eMFXPlatform platform = MFX_VPX_Utility::GetPlatform(core, out);
    if (platform != core->GetPlatformType())
    {
#ifdef MFX_VA
        sts = MFX_ERR_UNSUPPORTED;
#endif
    }

    return sts;
}

mfxStatus VideoDECODEAV1::QueryIOSurf(VideoCORE* core, mfxVideoParam* par, mfxFrameAllocRequest* request)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR)
    MFX_CHECK_NULL_PTR2(par, request);

    MFX_CHECK(
        par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY  ||
        par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY ||
        par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY &&
        par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY &&
        par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(!(
        par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY &&
        par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    mfxStatus sts = MFX_ERR_NONE;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        sts = MFX_VPX_Utility::QueryIOSurfInternal(par, request);
    else
    {
        request->Info = par->mfx.FrameInfo;
        request->NumFrameMin = 1;
        request->NumFrameSuggested = request->NumFrameMin + (par->AsyncDepth ? par->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
        request->Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }

    request->Type |= par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY ?
        MFX_MEMTYPE_OPAQUE_FRAME : MFX_MEMTYPE_EXTERNAL_FRAME;

    return sts;
}

mfxStatus VideoDECODEAV1::DecodeHeader(VideoCORE* core, mfxBitstream* bs, mfxVideoParam* par)
{
    MFX_CHECK(core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK_NULL_PTR3(bs, bs->Data, par);

    mfxStatus sts = MFX_ERR_NONE;
    try
    {
        MFXMediaDataAdapter in(bs);

        UMC_AV1_DECODER::AV1DecoderParams vp;
        UMC::Status res = UMC_AV1_DECODER::AV1Decoder::DecodeHeader(&in, vp);
        if (res != UMC::UMC_OK)
            return ConvertStatusUmc2Mfx(res);

        sts = FillVideoParam(core, &vp, par);
    }
    catch (UMC_AV1_DECODER::av1_exception & ex)
    {
        return ConvertStatusUmc2Mfx(ex.GetStatus());
    }

    MFX_CHECK_STS(sts);

    if (MFX_VPX_Utility::GetPlatform(core, par) != MFX_PLATFORM_SOFTWARE)
    {
        if (   par->mfx.FrameInfo.FourCC == MFX_FOURCC_P010
#if (MFX_VERSION >= 1027)
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y210
#endif
#ifdef PRE_SI_TARGET_PLATFORM_GEN12
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_P016
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y216
            || par->mfx.FrameInfo.FourCC == MFX_FOURCC_Y416
#endif
            )

            par->mfx.FrameInfo.Shift = 1;
    }

    return sts;
}

mfxStatus VideoDECODEAV1::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    UMC::AutomaticUMCMutex guard(m_guard);

    return MFX_ERR_NONE;
}


mfxStatus VideoDECODEAV1::GetDecodeStat(mfxDecodeStat* stat)
{
    MFX_CHECK_NULL_PTR1(stat);

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::DecodeFrameCheck(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT* entry_point)
{
    MFX_CHECK_NULL_PTR1(entry_point);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxThreadTask task;
    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out, &task);
    MFX_CHECK_STS(sts);

    entry_point->pRoutine = &DecodeRoutine;
    entry_point->pCompleteProc = &CompleteProc;
    entry_point->pState = this;
    entry_point->requiredNumThreads = 1;
    entry_point->pParam = task;

    return sts;
}

mfxStatus VideoDECODEAV1::SetSkipMode(mfxSkipMode /*mode*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEAV1::GetPayload(mfxU64* /*time_stamp*/, mfxPayload* /*pPayload*/)
{
    MFX_RETURN(MFX_ERR_UNSUPPORTED);
}

mfxStatus VideoDECODEAV1::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxThreadTask* task)
{
    MFX_CHECK_NULL_PTR1(task);

    UMC::AutomaticUMCMutex guard(m_guard);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = SubmitFrame(bs, surface_work, surface_out);
    MFX_CHECK_STS(sts);

    std::unique_ptr<TaskInfo> info(new TaskInfo);
    //auto info = std::make_unique<TaskInfo>();
    info->surface_work = GetOriginalSurface(surface_work);
    if (*surface_out)
        info->surface_out = GetOriginalSurface(*surface_out);

    *task =
        reinterpret_cast<mfxThreadTask>(info.release());

    return MFX_ERR_NONE;
}

mfxStatus VideoDECODEAV1::DecodeFrame(mfxFrameSurface1 *surface_out, AV1DecoderFrame* frame)
{
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);

    surface_out->Data.Corrupted = 0;
    int32_t const error = frame->GetError();

    if (error & UMC::ERROR_FRAME_DEVICE_FAILURE)
    {
        surface_out->Data.Corrupted |= MFX_CORRUPTION_MAJOR;
        MFX_CHECK(error != UMC::UMC_ERR_GPU_HANG, MFX_ERR_GPU_HANG);
        MFX_RETURN(MFX_ERR_DEVICE_FAILED);
    }
    else
    {
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
    }

    UMC::FrameMemID id = frame->GetFrameData()->GetFrameMID();
    mfxStatus sts = m_allocator->PrepareToOutput(surface_out, id, &m_video_par, m_opaque);
#ifdef MFX_VA
    frame->Displayed(true);
#else
    frame->Reset();
#endif

    return sts;
}

mfxStatus VideoDECODEAV1::QueryFrame(mfxThreadTask task)
{
    MFX_CHECK_NULL_PTR1(task);

    MFX_CHECK(m_core, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(m_decoder, MFX_ERR_NOT_INITIALIZED);

    auto info =
        reinterpret_cast<TaskInfo*>(task);

    mfxFrameSurface1* surface_out = info->surface_out;
    MFX_CHECK(surface_out, MFX_ERR_UNDEFINED_BEHAVIOR);

    UMC::FrameMemID id = m_allocator->FindSurface(surface_out, m_opaque);
    UMC_AV1_DECODER::AV1DecoderFrame* frame =
        m_decoder->FindFrameByMemID(id);

    MFX_CHECK(frame, MFX_ERR_UNDEFINED_BEHAVIOR);

    MFX_CHECK(frame->DecodingStarted(), MFX_ERR_UNDEFINED_BEHAVIOR);

    if (!frame->DecodingCompleted())
    {
        m_decoder->QueryFrames();
    }

    MFX_CHECK(frame->DecodingCompleted(), MFX_TASK_WORKING);

    mfxStatus sts = DecodeFrame(surface_out, frame);
    MFX_CHECK_STS(sts);

    return MFX_TASK_DONE;
}

mfxStatus VideoDECODEAV1::SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out)
{
    MFX_CHECK_NULL_PTR2(surface_work, surface_out);

    mfxStatus sts = CheckFrameInfoCodecs(&surface_work->Info, MFX_CODEC_AV1, m_platform != MFX_PLATFORM_SOFTWARE);
    MFX_CHECK_STS(sts);

    sts = CheckFrameData(surface_work);
    MFX_CHECK_STS(sts);

    MFX_CHECK(bs, MFX_ERR_MORE_DATA);

    sts = CheckBitstream(bs);
    MFX_CHECK_STS(sts);

    MFX_CHECK(!surface_work->Data.Locked, MFX_ERR_MORE_SURFACE);

    sts = m_allocator->SetCurrentMFXSurface(surface_work, m_opaque);
    MFX_CHECK_STS(sts);

    try
    {
        bool force = false;
        MFXMediaDataAdapter src(bs);

        for (;;)
        {
            UMC::Status umcRes = m_allocator->FindFreeSurface() != -1 ?
                m_decoder->GetFrame(bs ? &src : 0, nullptr) : UMC::UMC_ERR_NEED_FORCE_OUTPUT;

            UMC::Status umcFrameRes = umcRes;

             if (umcRes == UMC::UMC_NTF_NEW_RESOLUTION ||
                 umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS ||
                 umcRes == UMC::UMC_ERR_UNSUPPORTED)
            {
                 UMC_AV1_DECODER::AV1DecoderParams vp;
                 umcRes = m_decoder->GetInfo(&vp);
                 FillVideoParam(m_core, &vp, &m_video_par);
            }

            switch (umcRes)
            {
                case UMC::UMC_ERR_INVALID_STREAM:
                    umcRes = UMC::UMC_OK;
                    break;

                case UMC::UMC_NTF_NEW_RESOLUTION:
                    MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

#if defined(MFX_VA)
                case UMC::UMC_ERR_DEVICE_FAILED:
                    sts = MFX_ERR_DEVICE_FAILED;
                    break;

                case UMC::UMC_ERR_GPU_HANG:
                    sts = MFX_ERR_GPU_HANG;
                    break;
#endif
            }

            if (umcRes == UMC::UMC_WRN_REPOSITION_INPROGRESS)
            {
                if (!m_first_run)
                {
                    sts = MFX_WRN_VIDEO_PARAM_CHANGED;
                }
                else
                {
                    umcRes = UMC::UMC_OK;
                    m_first_run = false;
                }
            }

            if (umcRes == UMC::UMC_OK && m_allocator->FindFreeSurface() == -1)
            {
                sts = MFX_ERR_MORE_SURFACE;
                umcFrameRes = UMC::UMC_ERR_NOT_ENOUGH_BUFFER;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER || umcRes == UMC::UMC_WRN_INFO_NOT_READY || umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT)
            {
                force = (umcRes == UMC::UMC_ERR_NEED_FORCE_OUTPUT);
                sts = umcRes == UMC::UMC_ERR_NOT_ENOUGH_BUFFER ? (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK: MFX_WRN_DEVICE_BUSY;
            }

            if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA || umcRes == UMC::UMC_ERR_SYNC)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                sts = MFX_ERR_MORE_DATA;
            }


            if (umcRes != UMC::UMC_NTF_NEW_RESOLUTION)
            {
                src.Save(bs);
            }

            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
                return sts;

            //return these errors immediatelly unless we have [input == 0]
            if (sts == MFX_ERR_DEVICE_FAILED || sts == MFX_ERR_GPU_HANG)
            {
                if (!bs || bs->DataFlag == MFX_BITSTREAM_EOS)
                    force = true;
                else
                    return sts;
            }

            UMC_AV1_DECODER::AV1DecoderFrame* frame = GetFrameToDisplay();
            if (frame)
            {
                FillOutputSurface(surface_work, surface_out, frame);
                return MFX_ERR_NONE;
            }

            if (umcFrameRes != UMC::UMC_OK)
                break;

        } // for (;;)
    }
    catch(UMC_AV1_DECODER::av1_exception const&)
    {
    }
    catch (std::bad_alloc const&)
    {
        MFX_RETURN(MFX_ERR_MEMORY_ALLOC);
    }
    catch(...)
    {
        MFX_RETURN(MFX_ERR_UNKNOWN);
    }

    MFX_CHECK_STS(sts);
    return sts;
}

mfxStatus VideoDECODEAV1::DecodeRoutine(void* state, void* param, mfxU32, mfxU32)
{
    auto decoder = reinterpret_cast<VideoDECODEAV1*>(state);
    MFX_CHECK(decoder, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxThreadTask task =
        reinterpret_cast<TaskInfo*>(param);

    MFX_CHECK(task, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus sts = decoder->QueryFrame(task);
    MFX_CHECK_STS(sts);
    return sts;
}

mfxStatus VideoDECODEAV1::CompleteProc(void*, void* param, mfxStatus)
{
    auto info =
        reinterpret_cast<TaskInfo*>(param);
    delete info;

    return MFX_ERR_NONE;
}

inline
mfxU16 color_format2bit_depth(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::YUY2:
        case UMC::AYUV: return 8;

        case UMC::P010:
        case UMC::Y210:
        case UMC::Y410:  return 10;

        case UMC::P016:
        case UMC::Y216:
        case UMC::Y416:  return 12;

        default:         return 0;
    }
}

inline
mfxU16 color_format2chroma_format(UMC::ColorFormat format)
{
    switch (format)
    {
        case UMC::NV12:
        case UMC::P010:
        case UMC::P016: return MFX_CHROMAFORMAT_YUV420;

        case UMC::YUY2:
        case UMC::Y210:
        case UMC::Y216: return MFX_CHROMAFORMAT_YUV422;

        case UMC::AYUV:
        case UMC::Y410:
        case UMC::Y416: return MFX_CHROMAFORMAT_YUV444;

        default:        return MFX_CHROMAFORMAT_YUV420;
    }
}

inline
mfxU16 av1_native_profile_to_mfx_profile(mfxU16 native)
{
    switch (native)
    {
    case 0: return MFX_PROFILE_AV1_MAIN;
    case 1: return MFX_PROFILE_AV1_HIGH;
    case 2: return MFX_PROFILE_AV1_PRO;
    default: return 0;
    }
}

mfxStatus VideoDECODEAV1::FillVideoParam(VideoCORE* /*core*/, UMC_AV1_DECODER::AV1DecoderParams const* vp, mfxVideoParam* par)
{
    VM_ASSERT(vp);
    VM_ASSERT(par);

    mfxVideoParam p{};
    ConvertUMCParamsToMFX(vp, &p);

    p.mfx.FrameInfo.BitDepthLuma =
    p.mfx.FrameInfo.BitDepthChroma =
        color_format2bit_depth(vp->info.color_format);

    p.mfx.FrameInfo.ChromaFormat =
        color_format2chroma_format(vp->info.color_format);

    par->mfx.FrameInfo            = p.mfx.FrameInfo;
    par->mfx.CodecProfile         = av1_native_profile_to_mfx_profile(p.mfx.CodecProfile);
    par->mfx.CodecLevel           = p.mfx.CodecLevel;
    par->mfx.DecodedOrder         = p.mfx.DecodedOrder;
    par->mfx.MaxDecFrameBuffering = p.mfx.MaxDecFrameBuffering;

    par->mfx.FilmGrain = static_cast<mfxU16>(vp->film_grain);

    return MFX_ERR_NONE;
}

UMC_AV1_DECODER::AV1DecoderFrame* VideoDECODEAV1::GetFrameToDisplay()
{
    VM_ASSERT(m_decoder);

    UMC_AV1_DECODER::AV1DecoderFrame* frame
        = m_decoder->GetFrameToDisplay();

    if (!frame)
        return nullptr;

    frame->Outputted(true);
    frame->ShowAsExisting(false);

    return frame;
}

inline void CopyFilmGrainParam(mfxExtAV1FilmGrainParam &extBuf, UMC_AV1_DECODER::FilmGrainParams const& par)
{
    extBuf.Flags = 0;

    if (par.apply_grain)
        extBuf.Flags |= MFX_FILM_GRAIN_APPLY;

    extBuf.GrainSeed = (mfxU16)par.grain_seed;

    if (par.update_grain)
        extBuf.Flags |= MFX_FILM_GRAIN_UPDATE;

    extBuf.RefIdx = (mfxU8)par.film_grain_params_ref_idx;

    extBuf.NumYPoints = (mfxU8)par.num_y_points;
    for (int i = 0; i < UMC_AV1_DECODER::MAX_POINTS_IN_SCALING_FUNCTION_LUMA; i++)
    {
        extBuf.PointY[i].Value = (mfxU8)par.point_y_value[i];
        extBuf.PointY[i].Scaling = (mfxU8)par.point_y_scaling[i];
    }

    if (par.chroma_scaling_from_luma)
        extBuf.Flags |= MFX_FILM_GRAIN_CHROMA_SCALING_FROM_LUMA;

    extBuf.NumCbPoints = (mfxU8)par.num_cb_points;
    extBuf.NumCrPoints = (mfxU8)par.num_cr_points;
    for (int i = 0; i < UMC_AV1_DECODER::MAX_POINTS_IN_SCALING_FUNCTION_CHROMA; i++)
    {
        extBuf.PointCb[i].Value = (mfxU8)par.point_cb_value[i];
        extBuf.PointCb[i].Scaling = (mfxU8)par.point_cb_scaling[i];
        extBuf.PointCr[i].Value = (mfxU8)par.point_cr_value[i];
        extBuf.PointCr[i].Scaling = (mfxU8)par.point_cr_scaling[i];
    }

    extBuf.GrainScalingMinus8 = (mfxU8)par.grain_scaling - 8;
    extBuf.ArCoeffLag = (mfxU8)par.ar_coeff_lag;

    for (int i = 0; i < UMC_AV1_DECODER::MAX_AUTOREG_COEFFS_LUMA; i++)
        extBuf.ArCoeffsYPlus128[i] = (mfxU8)(par.ar_coeffs_y[i] + 128);

    for (int i = 0; i < UMC_AV1_DECODER::MAX_AUTOREG_COEFFS_CHROMA; i++)
    {
        extBuf.ArCoeffsCbPlus128[i] = (mfxU8)(par.ar_coeffs_cb[i] + 128);
        extBuf.ArCoeffsCrPlus128[i] = (mfxU8)(par.ar_coeffs_cr[i] + 128);
    }

    extBuf.ArCoeffShiftMinus6 = (mfxU8)par.ar_coeff_shift - 6;
    extBuf.GrainScaleShift = (mfxU8)par.grain_scale_shift;
    extBuf.CbMult = (mfxU8)par.cb_mult;
    extBuf.CbLumaMult = (mfxU8)par.cb_luma_mult;
    extBuf.CbOffset = (mfxU16)par.cb_offset;
    extBuf.CrMult = (mfxU8)par.cr_mult;
    extBuf.CrLumaMult = (mfxU8)par.cr_luma_mult;
    extBuf.CrOffset = (mfxU16)par.cr_offset;

    if (par.overlap_flag)
        extBuf.Flags |= MFX_FILM_GRAIN_OVERLAP;

    if (par.clip_to_restricted_range)
        extBuf.Flags |= MFX_FILM_GRAIN_CLIP_TO_RESTRICTED_RANGE;
}

void VideoDECODEAV1::FillOutputSurface(mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, UMC_AV1_DECODER::AV1DecoderFrame* frame)
{
    VM_ASSERT(surface_work);
    VM_ASSERT(frame);

    UMC::FrameData const* fd = frame->GetFrameData();
    VM_ASSERT(fd);

    mfxVideoParam vp;
    *surface_out = m_allocator->GetSurface(fd->GetFrameMID(), surface_work, &vp);
    if (m_opaque)
       *surface_out = m_core->GetOpaqSurface((*surface_out)->Data.MemId);

    mfxFrameSurface1* surface = *surface_out;
    VM_ASSERT(surface);

    SetFrameType(*frame, *surface);

    surface->Info.FrameId.TemporalId = 0;

    UMC::VideoDataInfo const* vi = fd->GetInfo();
    VM_ASSERT(vi);
    vi;

    surface->Info.CropW = static_cast<mfxU16>(frame->GetUpscaledWidth());
    surface->Info.CropH = static_cast<mfxU16>(frame->GetHeight());

    mfxExtAV1FilmGrainParam* extFilmGrain = (mfxExtAV1FilmGrainParam*)GetExtendedBuffer(surface->Data.ExtParam, surface->Data.NumExtParam, MFX_EXTBUFF_AV1_FILM_GRAIN_PARAM);
    if (extFilmGrain)
    {
        UMC_AV1_DECODER::FrameHeader const& fh = frame->GetFrameHeader();
        CopyFilmGrainParam(*extFilmGrain, fh.film_grain_params);
    }

}

#endif
