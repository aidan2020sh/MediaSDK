/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_ENCODE_HW
#include <algorithm>
#include <numeric>

#include "cmrt_cross_platform.h"

#include "mfx_session.h"
#include "mfx_task.h"
#include "libmfx_core.h"
#include "libmfx_core_hw.h"
#include "libmfx_core_interface.h"
#include "mfx_ext_buffers.h"

#include "mfx_h264_encode_hw.h"
#include "mfx_h264_enc_common_hw.h"
#include "mfx_h264_encode_hw_utils.h"
#include "umc_va_dxva2_protected.h"

#include "mfx_h264_encode_cm.h"
#include "mfx_h264_encode_cm_defs.h"

#define DEBUG_ADAPT 0

using namespace MfxHwH264Encode;


namespace
{
    mfxU32 PaddingBytesToWorkAroundHrdIssue(
        MfxVideoParam const &      video,
        Hrd                        hrd,
        std::list<DdiTask> const & submittedTasks,
        mfxU32                     fieldPicFlag,
        mfxU32                     secondPicFlag)
    {
        mfxExtCodingOption const * extOpt = GetExtBuffer(video);

        if (video.mfx.RateControlMethod != MFX_RATECONTROL_CBR || IsOff(extOpt->NalHrdConformance))
            return 0;

        mfxF64 frameRate = mfxF64(video.mfx.FrameInfo.FrameRateExtN) / video.mfx.FrameInfo.FrameRateExtD;
        mfxU32 avgFrameSize = mfxU32(1000 * video.calcParam.targetKbps / frameRate);
        if (avgFrameSize <= 128 * 1024 * 8)
            return 0;

        for (DdiTaskCiter i = submittedTasks.begin(); i != submittedTasks.end(); ++i)
            hrd.RemoveAccessUnit(0, i->m_fieldPicFlag, false);
        if (secondPicFlag)
            hrd.RemoveAccessUnit(0, fieldPicFlag, false);
        hrd.RemoveAccessUnit(0, fieldPicFlag, false);

        mfxU32 bufsize  = 8000 * video.calcParam.bufferSizeInKB;
        mfxU32 bitrate  = GetMaxBitrateValue(video.calcParam.maxKbps) << 6;
        mfxU32 delay    = hrd.GetInitCpbRemovalDelay();
        mfxU32 fullness = mfxU32(mfxU64(delay) * bitrate / 90000.0);

        const mfxU32 maxFrameSize = video.mfx.FrameInfo.Width * video.mfx.FrameInfo.Height;
        if (fullness > bufsize)
            return IPP_MIN((fullness - bufsize + 7) / 8, maxFrameSize);

        return 0;
    }

    mfxStatus ReadSpsPpsHeaders(MfxVideoParam & par)
    {
        mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);

        try
        {
            if (extBits->SPSBuffer)
            {
                InputBitstream reader(extBits->SPSBuffer, extBits->SPSBufSize);
                mfxExtSpsHeader * extSps = GetExtBuffer(par);
                ReadSpsHeader(reader, *extSps);

                if (extBits->PPSBuffer)
                {
                    InputBitstream reader(extBits->PPSBuffer, extBits->PPSBufSize);
                    mfxExtPpsHeader * extPps = GetExtBuffer(par);
                    ReadPpsHeader(reader, *extSps, *extPps);
                }
            }
        }
        catch (std::exception &)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        return MFX_ERR_NONE;
    }

    mfxU16 GetFrameWidth(MfxVideoParam & par)
    {
        mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
        if (extBits->SPSBuffer)
        {
            mfxExtSpsHeader * extSps = GetExtBuffer(par);
            return mfxU16(16 * (extSps->picWidthInMbsMinus1 + 1));
        }
        else
        {
            return par.mfx.FrameInfo.Width;
        }
    }

    mfxU16 GetFrameHeight(MfxVideoParam & par)
    {
        mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
        if (extBits->SPSBuffer)
        {
            mfxExtSpsHeader * extSps = GetExtBuffer(par);
            return mfxU16(16 * (extSps->picHeightInMapUnitsMinus1 + 1) * (2 - extSps->frameMbsOnlyFlag));
        }
        else
        {
            return par.mfx.FrameInfo.Height;
        }
    }

    IDirect3DDeviceManager9 * GetDeviceManager(VideoCORE * core)
    {
#if defined(_WIN32) || defined(_WIN64)
      D3D9Interface * d3dIface = QueryCoreInterface<D3D9Interface>(core, MFXICORED3D_GUID);
        if (d3dIface == 0)
            return 0;
        return d3dIface->GetD3D9DeviceManager();
#else
    throw std::logic_error("GetDeviceManager not implemented on Linux for Look Ahead");
#endif
    }

    VmeData * FindUnusedVmeData(std::vector<VmeData> & vmeData)
    {
        VmeData * oldest = 0;
        for (size_t i = 0; i < vmeData.size(); i++)
        {
            if (!vmeData[i].used)
                return &vmeData[i];
            if (oldest == 0 || oldest->encOrder > vmeData[i].encOrder)
                oldest = &vmeData[i];
        }
        return oldest;
    }
}

VideoENCODE * CreateMFXHWVideoENCODEH264(
    VideoCORE * core,
    mfxStatus * res)
{
    return new MFXHWVideoENCODEH264(core, res);
}

mfxStatus MFXHWVideoENCODEH264::Init(mfxVideoParam * par)
{
    if (m_impl.get() != 0)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC))
        if (par->mfx.CodecProfile == 0)
            par->mfx.CodecProfile = MFX_PROFILE_AVC_SCALABLE_BASELINE;

    std::auto_ptr<VideoENCODE> impl(
        IsMvcProfile(par->mfx.CodecProfile)
            ? (VideoENCODE *) new ImplementationMvc(m_core)
            : IsSvcProfile(par->mfx.CodecProfile)
                ? (VideoENCODE *) new ImplementationSvc(m_core)
                : (VideoENCODE *) new ImplementationAvc(m_core));

    mfxStatus sts = impl->Init(par);
    MFX_CHECK(
        sts >= MFX_ERR_NONE &&
        sts != MFX_WRN_PARTIAL_ACCELERATION, sts);

    m_impl = impl;
    return sts;
}

mfxStatus MFXHWVideoENCODEH264::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out,
    void *          state)
{

    // FIXME: remove when mfx_transcoder start sending correct Profile
    if (in && in->mfx.CodecProfile == 0)
        if (GetExtBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC))
            in->mfx.CodecProfile = MFX_PROFILE_AVC_SCALABLE_BASELINE;

    if (in && IsMvcProfile(in->mfx.CodecProfile) && !IsHwMvcEncSupported())
        return MFX_WRN_PARTIAL_ACCELERATION;

    if (in == 0)
        return ImplementationAvc::Query(core, in, out);

    if (IsMvcProfile(in->mfx.CodecProfile))
        return ImplementationMvc::Query(core, in, out);

    if (IsSvcProfile(in->mfx.CodecProfile))
        return ImplementationSvc::Query(core, in, out);

    if (state)
    {
        MFXHWVideoENCODEH264 * AVCEncoder = (MFXHWVideoENCODEH264*)state;

        if (!AVCEncoder->m_impl.get())
        {
            assert(!"Encoder implementation isn't initialized");
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        return ImplementationAvc::Query(core, in, out, AVCEncoder->m_impl.get());
    }

    return ImplementationAvc::Query(core, in, out);
}

mfxStatus MFXHWVideoENCODEH264::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    if (GetExtBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC))
        if (par->mfx.CodecProfile == 0)
            par->mfx.CodecProfile = MFX_PROFILE_AVC_SCALABLE_BASELINE;

    if (IsMvcProfile(par->mfx.CodecProfile) && !IsHwMvcEncSupported())
        return MFX_WRN_PARTIAL_ACCELERATION;

    if (IsMvcProfile(par->mfx.CodecProfile))
        return ImplementationMvc::QueryIOSurf(core, par, request);

    if (IsSvcProfile(par->mfx.CodecProfile))
        return ImplementationSvc::QueryIOSurf(core, par, request);

    return ImplementationAvc::QueryIOSurf(core, par, request);
}

mfxStatus ImplementationAvc::Query(
    VideoCORE *     core,
    mfxVideoParam * in,
    mfxVideoParam * out,
    void *          state)
{
    MFX_CHECK_NULL_PTR2(core, out);

    mfxStatus sts;


    // "in" parameters should uniquely identify one of 4 Query operation modes (see MSDK spec for details)
    mfxU8 queryMode = DetermineQueryMode(in);
    if (queryMode == 0)
        return MFX_ERR_UNDEFINED_BEHAVIOR; // input parameters are contradictory and don't allow to choose Query mode

    if (queryMode == 1) // see MSDK spec for details related to Query mode 1
    {
        Zero(out->mfx);

        out->IOPattern             = 1;
        out->Protected             = 1;
        out->AsyncDepth            = 1;
        out->mfx.CodecId           = 1;
        out->mfx.CodecLevel        = 1;
        out->mfx.CodecProfile      = 1;
        out->mfx.NumThread         = 0;
        out->mfx.TargetUsage       = 1;
        out->mfx.GopPicSize        = 1;
        out->mfx.GopRefDist        = 1;
        out->mfx.GopOptFlag        = 1;
        out->mfx.IdrInterval       = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.InitialDelayInKB  = 1;
        out->mfx.BufferSizeInKB    = 1;
        out->mfx.TargetKbps        = 1;
        out->mfx.MaxKbps           = 1;
        out->mfx.NumSlice          = 1;
        out->mfx.NumRefFrame       = 1;
        out->mfx.NumThread         = 1;
        out->mfx.EncodedOrder      = 1;

        out->mfx.FrameInfo.FourCC        = 1;
        out->mfx.FrameInfo.Width         = 1;
        out->mfx.FrameInfo.Height        = 1;
        out->mfx.FrameInfo.CropX         = 1;
        out->mfx.FrameInfo.CropY         = 1;
        out->mfx.FrameInfo.CropW         = 1;
        out->mfx.FrameInfo.CropH         = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW  = 1;
        out->mfx.FrameInfo.AspectRatioH  = 1;
        out->mfx.FrameInfo.ChromaFormat  = 1;
        out->mfx.FrameInfo.PicStruct     = 1;

        if (mfxExtMVCSeqDesc * opt = GetExtBuffer(*out))
        {
            opt->NumOP     = 1;
            opt->NumView   = 1;
            opt->NumViewId = 1;
        }

        if (mfxExtAVCRefListCtrl * ctrl = GetExtBuffer(*out))
        {
            mfxExtBuffer tmp = ctrl->Header;
            Zero(*ctrl);
            ctrl->Header = tmp;

            ctrl->NumRefIdxL0Active = 1;
            ctrl->NumRefIdxL1Active = 1;
            ctrl->ApplyLongTermIdx  = 1;

            ctrl->LongTermRefList[0].FrameOrder   = 1;
            ctrl->LongTermRefList[0].LongTermIdx  = 1;
            ctrl->PreferredRefList[0].FrameOrder  = 1;
            ctrl->RejectedRefList[0].FrameOrder   = 1;
        }

        if (mfxExtEncoderResetOption * resetOpt = GetExtBuffer(*out))
        {
            resetOpt->StartNewSequence = 1;
        }
    }
    else if (queryMode == 2)  // see MSDK spec for details related to Query mode 2
    {
        ENCODE_CAPS hwCaps = { 0, };
        mfxExtAVCEncoderWiDiUsage * isWiDi = GetExtBuffer(*in);
        
        // let use dedault values if input resolution is 0x0
        mfxU32 Width  = in->mfx.FrameInfo.Width == 0 ? 1920: in->mfx.FrameInfo.Width;
        mfxU32 Height =  in->mfx.FrameInfo.Height == 0 ? 1088: in->mfx.FrameInfo.Height;

        sts = QueryHwCaps(core, hwCaps, MSDK_Private_Guid_Encode_AVC_Query, isWiDi != 0, Width, Height);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        MfxVideoParam tmp = *in; // deep copy, create all supported extended buffers

        sts = ReadSpsPpsHeaders(tmp);
        MFX_CHECK_STS(sts);

        mfxStatus checkSts = CheckVideoParamQueryLike(tmp, hwCaps, core->GetHWType(), core->GetVAType());
        if (checkSts == MFX_WRN_PARTIAL_ACCELERATION)
            return MFX_WRN_PARTIAL_ACCELERATION;
        else if (checkSts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
            checkSts = MFX_ERR_UNSUPPORTED;

        out->IOPattern  = tmp.IOPattern;
        out->Protected  = tmp.Protected;
        out->AsyncDepth = tmp.AsyncDepth;
        out->mfx = tmp.mfx;

        // should have same number of buffers
        if (in->NumExtParam != out->NumExtParam || !in->ExtParam != !out->ExtParam)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        // should have same buffers
        if (in->ExtParam && out->ExtParam)
        {
            for (mfxU32 i = 0; i < in->NumExtParam; i++)
                MFX_CHECK_NULL_PTR1(in->ExtParam[i]);

            for (mfxU32 i = 0; i < out->NumExtParam; i++)
                MFX_CHECK_NULL_PTR1(out->ExtParam[i]);

            for (mfxU32 i = 0; i < in->NumExtParam; i++)
            {
                if (in->ExtParam[i])
                {
                    if (IsRunTimeExtBufferIdSupported(in->ExtParam[i]->BufferId))
                        continue; // it's runtime only ext buffer. Nothing to check or correct, just move on.

                    if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_ENCODER_WIDI_USAGE)
                        continue; // this buffer notify that WiDi is caller for Query. Nothing to check or correct, just move on.

                    if (!IsVideoParamExtBufferIdSupported(in->ExtParam[i]->BufferId))
                        return MFX_ERR_UNSUPPORTED;

                    if (MfxHwH264Encode::GetExtBuffer(
                        in->ExtParam + i + 1,
                        in->NumExtParam - i - 1,
                        in->ExtParam[i]->BufferId) != 0)
                    {
                        // buffer present twice in 'in'
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    }

                    if (mfxExtBuffer * buf = GetExtBuffer(out->ExtParam, out->NumExtParam, in->ExtParam[i]->BufferId))
                    {
                        mfxExtBuffer * corrected = GetExtBuffer(tmp.ExtParam, tmp.NumExtParam, in->ExtParam[i]->BufferId);
                        MFX_CHECK_NULL_PTR1(corrected);

                        if (buf->BufferId == MFX_EXTBUFF_MVC_SEQ_DESC)
                        {
                            // mfxExtMVCSeqDesc is complex structure
                            // deep-copy is required if mvc description is fully initialized
                            mfxExtMVCSeqDesc & src = *(mfxExtMVCSeqDesc *)corrected;
                            mfxExtMVCSeqDesc & dst = *(mfxExtMVCSeqDesc *)buf;

                            mfxStatus sts = CheckBeforeCopyQueryLike(dst, src);
                            if (sts != MFX_ERR_NONE)
                                return MFX_ERR_UNSUPPORTED;

                            Copy(dst, src);
                        }
                        else
                        {
                            // shallow-copy
                            memcpy(buf, corrected, corrected->BufferSz);
                        }
                    }
                    else
                    {
                        // buffer doesn't present in 'out'
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    }
                }
            }

            for (mfxU32 i = 0; i < out->NumExtParam; i++)
            {
                if (out->ExtParam[i] && GetExtBuffer(in->ExtParam, in->NumExtParam, out->ExtParam[i]->BufferId) == 0)
                    return MFX_ERR_UNSUPPORTED;
            }
        }

        return checkSts;
    }
    else if (queryMode == 3)  // see MSDK spec for details related to Query mode 3
    {
        mfxStatus checkSts = MFX_ERR_NONE;
        if (state == 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR; // encoder isn't initialized. Query() can't operate in mode 3

        checkSts = CheckExtBufferId(*in);
        MFX_CHECK_STS(checkSts);

        MfxVideoParam newPar = *in;
        bool isIdrRequired = false;

        ImplementationAvc * AVCEncoder = (ImplementationAvc*)state;

        checkSts = AVCEncoder->ProcessAndCheckNewParameters(newPar, isIdrRequired);
        if (checkSts < MFX_ERR_NONE)
            return checkSts;

        mfxExtEncoderResetOption * extResetOptIn = GetExtBuffer(*in);
        mfxExtEncoderResetOption * extResetOptOut = GetExtBuffer(*out);

        if (extResetOptOut != 0)
        {
            extResetOptOut->StartNewSequence = extResetOptIn->StartNewSequence;
            if (extResetOptIn->StartNewSequence == MFX_CODINGOPTION_UNKNOWN)
            {
                extResetOptOut->StartNewSequence = mfxU16(isIdrRequired ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF);
            }
        }

        return checkSts;
    }
    else if (4 == queryMode)// Query mode 4: Query should do a single thing - report MB processing rate
    {
        mfxU32 mbPerSec[16] = {0, };
       
        // let use dedault values if input resolution is 0x0, 1920x1088 - should cover almost all cases
        mfxU32 Width  = in->mfx.FrameInfo.Width == 0 ? 1920: in->mfx.FrameInfo.Width;
        mfxU32 Height =  in->mfx.FrameInfo.Height == 0 ? 1088: in->mfx.FrameInfo.Height;

        mfxExtEncoderCapability * extCaps = GetExtBuffer(*out);
        if (extCaps == 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR; // can't return MB processing rate since mfxExtEncoderCapability isn't attached to "out"

        mfxExtAVCEncoderWiDiUsage * isWiDi = GetExtBuffer(*in);
        // query MB processing rate from driver
        sts = QueryMbProcRate(core, *in, mbPerSec, MSDK_Private_Guid_Encode_AVC_Query, isWiDi != 0, Width, Height);
        if (sts != MFX_ERR_NONE)
        {
            extCaps->MBPerSec = 0;
            if (sts == MFX_ERR_UNSUPPORTED)
                return sts; // driver don't support reporting of MB processing rate

            return MFX_WRN_PARTIAL_ACCELERATION; // any other HW problem
        }

        extCaps->MBPerSec = mbPerSec[0];

        if (extCaps->MBPerSec == 0)
        {
            // driver returned status OK and MAX_MB_PER_SEC = 0. Treat this as driver doesn't support reporting of MAX_MB_PER_SEC for requested encoding configuration
            return MFX_ERR_UNSUPPORTED;
        }

        return MFX_ERR_NONE;
    }
    else if (5 == queryMode)
    {
        return QueryGuid(core, MSDK_Private_Guid_Encode_AVC_Query);
    }

    return MFX_ERR_NONE;
}

mfxStatus ImplementationAvc::QueryIOSurf(
    VideoCORE *            core,
    mfxVideoParam *        par,
    mfxFrameAllocRequest * request)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY,
        MFX_ERR_INVALID_VIDEO_PARAM);

    ENCODE_CAPS hwCaps = { { 0 } };
    mfxExtAVCEncoderWiDiUsage * isWiDi = GetExtBuffer(*par);
    
    // let use dedault values if input resolution is 0x0, 1920x1088 - should cover almost all cases
    mfxU32 Width  = par->mfx.FrameInfo.Width == 0 ? 1920 : par->mfx.FrameInfo.Width;
    mfxU32 Height =  par->mfx.FrameInfo.Height == 0 ? 1088: par->mfx.FrameInfo.Height;

    sts = QueryHwCaps(core, hwCaps, MSDK_Private_Guid_Encode_AVC_Query, isWiDi != 0, Width, Height);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    MfxVideoParam tmp(*par);

    sts = ReadSpsPpsHeaders(tmp);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(tmp);
    MFX_CHECK_STS(sts);

    sts = CopySpsPpsToVideoParam(tmp);
    if (sts < MFX_ERR_NONE)
        return sts;

    mfxStatus checkSts = CheckVideoParamQueryLike(tmp, hwCaps, core->GetHWType(), core->GetVAType());
    if (checkSts == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION; // return immediately

    SetDefaults(tmp, hwCaps, true, core->GetHWType(), core->GetVAType());

    if (tmp.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request->Type =
            MFX_MEMTYPE_EXTERNAL_FRAME |
            MFX_MEMTYPE_FROM_ENCODE |
            MFX_MEMTYPE_SYSTEM_MEMORY;
        request->NumFrameMin = tmp.mfx.GopRefDist + tmp.AsyncDepth - 1;
        request->NumFrameSuggested = request->NumFrameMin;
    }
    else // MFX_IOPATTERN_IN_VIDEO_MEMORY || MFX_IOPATTERN_IN_OPAQUE_MEMORY
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        request->Type |= (inPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
            ? MFX_MEMTYPE_OPAQUE_FRAME
            : MFX_MEMTYPE_EXTERNAL_FRAME;
        request->NumFrameMin = GetBufferingDepth(tmp);
        request->NumFrameSuggested = request->NumFrameMin;
    }

    // get FrameInfo from original VideoParam
    request->Info = tmp.mfx.FrameInfo;

    return MFX_ERR_NONE;
}

ImplementationAvc::ImplementationAvc(VideoCORE * core)
: m_core(core)
, m_video()
, m_maxBsSize(0)
, m_enabledSwBrc(false)
{
/*
    FEncLog = fopen("EncLog.txt", "wb");
*/
}

ImplementationAvc::~ImplementationAvc()
{
}

mfxStatus ImplementationAvc::Init(mfxVideoParam * par)
{
    mfxStatus sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    m_video = *par;
    sts = ReadSpsPpsHeaders(m_video);
    MFX_CHECK_STS(sts);

    sts = CheckWidthAndHeight(m_video);
    MFX_CHECK_STS(sts);

    m_ddi.reset( CreatePlatformH264Encoder( m_core ) );
    if (m_ddi.get() == 0)
        return MFX_WRN_PARTIAL_ACCELERATION;

    mfxExtAVCEncoderWiDiUsage * isWiDi = GetExtBuffer(*par);
    if (isWiDi)
        m_ddi->ForceCodingFunction(ENCODE_ENC_PAK | ENCODE_WIDI);

    sts = m_ddi->CreateAuxilliaryDevice(
        m_core,
        DXVA2_Intel_Encode_AVC,
        GetFrameWidth(m_video),
        GetFrameHeight(m_video));
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    sts = m_ddi->QueryEncodeCaps(m_caps);
    if (sts != MFX_ERR_NONE)
        return MFX_WRN_PARTIAL_ACCELERATION;

    m_currentPlatform = m_core->GetHWType();
    m_currentVaType   = m_core->GetVAType();

    mfxStatus spsppsSts = CopySpsPpsToVideoParam(m_video);

    mfxStatus checkStatus = CheckVideoParam(m_video, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_WRN_PARTIAL_ACCELERATION;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;
    else if (checkStatus == MFX_ERR_NONE)
        checkStatus = spsppsSts;

    // CQP enabled
    mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(m_video);
    m_enabledSwBrc = IsOn(extOpt2->ExtBRC) || m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA;

    // need it for both ENCODE and ENC
    m_hrd.Setup(m_video);

    mfxExtPAVPOption * extOptPavp = GetExtBuffer(m_video);
    m_aesCounter.Init(*extOptPavp);

    m_brc.SetImpl(m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA ? (Brc *)new LookAheadBrc2 : (Brc *)new UmcBrc);

    if (m_enabledSwBrc)
    {
        m_brc.Init(m_video);
        mfxU16 storedRateControlMethod = m_video.mfx.RateControlMethod;
        m_video.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        sts = m_ddi->CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);
        m_video.mfx.RateControlMethod = storedRateControlMethod;
    }
    else
    {
        sts = m_ddi->CreateAccelerationService(m_video);
        MFX_CHECK_STS(sts);
    }

    mfxFrameAllocRequest request = { { 0 } };
    request.Info = m_video.mfx.FrameInfo;

    mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(m_video);

    // Allocate raw surfaces.
    // This is required only in case of system memory at input

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = mfxU16(CalcNumSurfRaw(m_video));

        sts = m_raw.Alloc(m_core, request);
        MFX_CHECK_STS(sts);
    }
    else if (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        request.Type        = extOpaq->In.Type;
        request.NumFrameMin = extOpaq->In.NumSurface;

        sts = m_opaqHren.Alloc(m_core, request, extOpaq->In.Surfaces, extOpaq->In.NumSurface);
        MFX_CHECK_STS(sts);

        if (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
        {
            request.Type        = MFX_MEMTYPE_D3D_INT;
            request.NumFrameMin = extOpaq->In.NumSurface;
            sts = m_raw.Alloc(m_core, request);
        }
    }

    m_inputFrameType =
        m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY ||
        m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            ? MFX_IOPATTERN_IN_SYSTEM_MEMORY
            : MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // ENC+PAK always needs separate chain for reconstructions produced by PAK.
    request.Type        = m_video.Protected ? MFX_MEMTYPE_D3D_SERPENT_INT : MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = mfxU16(CalcNumSurfRecon(m_video));
    sts = m_rec.Alloc(m_core, request);
    MFX_CHECK_STS(sts);

    sts = m_ddi->Register(m_rec.NumFrameActual ? m_rec : m_raw, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    m_recFrameOrder.resize(request.NumFrameMin, 0xffffffff);

    // Allocate surfaces for bitstreams.
    // Need at least one such surface and more for async-mode.
    request.Type = MFX_MEMTYPE_D3D_INT;
    request.NumFrameMin = mfxU16(CalcNumSurfBitstream(m_video));

    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request);
    MFX_CHECK_STS(sts);
    // driver may suggest too small buffer for bitstream
    request.Info.Width  = IPP_MAX(request.Info.Width,  m_video.mfx.FrameInfo.Width);
    request.Info.Height = IPP_MAX(request.Info.Height, m_video.mfx.FrameInfo.Height * 3 / 2);
    m_maxBsSize = request.Info.Width * request.Info.Height;

    sts = m_bit.Alloc(m_core, request);
    MFX_CHECK_STS(sts);

    if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA)
    {
        m_cmDevice.Reset(CreateCmDevicePtr(m_core));
        m_cmCtx.reset(new CmContext(m_video, m_cmDevice));

        request.Info.Width  = m_video.calcParam.widthLa;
        request.Info.Height = m_video.calcParam.heightLa;

        mfxU32 numMb = request.Info.Width * request.Info.Height / 256;
        m_vmeDataStorage.resize(extOpt2->LookAheadDepth + m_video.AsyncDepth);
        for (size_t i = 0; i < m_vmeDataStorage.size(); i++)
            m_vmeDataStorage[i].mb.resize(numMb);
        m_tmpVmeData.reserve(extOpt2->LookAheadDepth);

        mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);
        if (extDdi->LaScaleFactor > 1)
        {
            request.Info.FourCC = MFX_FOURCC_NV12;
            request.Type        = MFX_MEMTYPE_D3D_INT;
            request.NumFrameMin = mfxU16(m_video.mfx.NumRefFrame + m_video.AsyncDepth);

            sts = m_rawLa.AllocCmSurfaces(m_cmDevice, request);
            MFX_CHECK_STS(sts);
        }

        request.Info.Width  = m_video.calcParam.widthLa  / 16 * sizeof(SVCPAKObject);
        request.Info.Height = m_video.calcParam.heightLa / 16;
        request.Info.FourCC = MFX_FOURCC_P8;
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = mfxU16(m_video.mfx.NumRefFrame + m_video.AsyncDepth);

        sts = m_mb.AllocCmBuffersUp(m_cmDevice, request);
        MFX_CHECK_STS(sts);

        request.Info.Width  = sizeof(SVCEncCURBEData);
        request.Info.Height = 1;
        request.Info.FourCC = MFX_FOURCC_P8;
        request.Type        = MFX_MEMTYPE_D3D_INT;
        request.NumFrameMin = 1 + !!(m_video.AsyncDepth > 1);

        sts = m_curbe.AllocCmBuffers(m_cmDevice, request);
        MFX_CHECK_STS(sts);
    }

    sts = m_ddi->Register(m_bit, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    m_emulatorForSyncPart.Init(m_video);
    m_emulatorForAsyncPart = m_emulatorForSyncPart;

    m_free.resize(GetBufferingDepth(m_video) + m_video.AsyncDepth - 1);
    m_incoming.clear();
    m_reordering.clear();
    m_lookaheadStarted.clear();
    m_lookaheadFinished.clear();
    m_encoding.clear();

    m_fieldCounter   = 0;
    m_1stFieldStatus = MFX_ERR_NONE;
    m_frameOrder     = 0;
    m_stagesToGo     = AsyncRoutineEmulator::STAGE_CALL_EMULATOR;
    m_failedStatus   = MFX_ERR_NONE;
    m_frameOrderIdrInDisplayOrder = 0;
    m_frameOrderStartLyncStructure = 0;

    m_lastTask = DdiTask();
    m_lastTask.m_aesCounter[0] = m_aesCounter;
    Decrement(m_lastTask.m_aesCounter[0], *extOptPavp);

    // initialization of parameters for Intra refresh
    if (extOpt2->IntRefType)
    {
        mfxU16 refreshDimension = extOpt2->IntRefType == HORIZ_REFRESH ? m_video.mfx.FrameInfo.Height >> 4 : m_video.mfx.FrameInfo.Width >> 4;
        m_intraStripeWidthInMBs = (refreshDimension + extOpt2->IntRefCycleSize - 1) / extOpt2->IntRefCycleSize;
        m_frameOrderIFrameInDisplayOrder = 0;
    }
    Zero(m_stat);

    // FIXME: w/a for SNB issue with HRD at high bitrates
    // FIXME: check what to do with WA on Linux (MFX_HW_VAAPI) - currently it is switched off
    m_useWAForHighBitrates = (MFX_HW_VAAPI != m_core->GetVAType()) && !m_enabledSwBrc &&
        m_video.mfx.RateControlMethod == MFX_RATECONTROL_CBR &&
        (m_currentPlatform < MFX_HW_HSW || m_currentPlatform == MFX_HW_VLV); // HRD WA for high bitrates isn't required for HSW and beyond

    // required for slice header patching
    if ((m_caps.HeaderInsertion == 1 || m_currentPlatform == MFX_HW_IVB && m_core->GetVAType() == MFX_HW_VAAPI) && m_video.Protected == 0)
        m_tmpBsBuf.resize(m_maxBsSize);

    const size_t MAX_SEI_SIZE    = 10 * 1024;
    const size_t MAX_FILLER_SIZE = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height;
    m_sei.Alloc(mfxU32(MAX_SEI_SIZE + MAX_FILLER_SIZE));

    m_bufferedInFrames.clear();
    m_bufferedInFrames.reserve(m_video.mfx.GopRefDist+1); //check mem alloc for surfaces
    m_readyInFrames.clear();
    m_readyInFrames.reserve(m_video.mfx.GopRefDist+1); //check mem alloc for surfaces
    //Init frame type generator for adaptive GOP
    m_frameTypeGenAdaptiveGOP.Init(m_video);

    m_videoInit = m_video;

    return checkStatus;
}

mfxStatus ImplementationAvc::ProcessAndCheckNewParameters(
    MfxVideoParam & newPar,
    bool & isIdrRequired)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxExtPAVPOption * extPavpNew = GetExtBuffer(newPar);
    mfxExtPAVPOption * extPavpOld = GetExtBuffer(m_video);
    *extPavpNew = *extPavpOld; // ignore any change in mfxExtPAVPOption

    mfxExtEncoderResetOption * extResetOpt = GetExtBuffer(newPar);

    sts = ReadSpsPpsHeaders(newPar);
    MFX_CHECK_STS(sts);

    mfxExtOpaqueSurfaceAlloc * extOpaqNew = GetExtBuffer(newPar);
    mfxExtOpaqueSurfaceAlloc * extOpaqOld = GetExtBuffer(m_video);
    MFX_CHECK(
        extOpaqOld->In.Type       == extOpaqNew->In.Type       &&
        extOpaqOld->In.NumSurface == extOpaqNew->In.NumSurface,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    mfxStatus spsppsSts = CopySpsPpsToVideoParam(newPar);

    InheritDefaultValues(m_video, newPar);

    mfxStatus checkStatus = CheckVideoParam(newPar, m_caps, m_core->IsExternalFrameAllocator(), m_currentPlatform, m_currentVaType);
    if (checkStatus == MFX_WRN_PARTIAL_ACCELERATION)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if (checkStatus < MFX_ERR_NONE)
        return checkStatus;
    else if (checkStatus == MFX_ERR_NONE)
        checkStatus = spsppsSts;

    // check if change of Lync temporal scalability required by new parameters
    mfxU32 tempLayerIdx = 0;
    bool changeLyncLayers = false;

    if (m_video.calcParam.lyncMode && newPar.calcParam.lyncMode)
    {
        // calculate Lync temporal layer for next frame
        tempLayerIdx     = CalcTemporalLayerIndex(m_video, m_frameOrder - m_frameOrderStartLyncStructure);
        changeLyncLayers = m_video.calcParam.numTemporalLayer != newPar.calcParam.numTemporalLayer;
    }

    mfxExtSpsHeader const * extSpsNew = GetExtBuffer(newPar);
    mfxExtSpsHeader const * extSpsOld = GetExtBuffer(m_video);

    // check if IDR required after change of encoding parameters
    isIdrRequired = !Equal(*extSpsNew, *extSpsOld)
        || tempLayerIdx != 0 && changeLyncLayers
        || newPar.mfx.GopPicSize != m_video.mfx.GopPicSize;

    if (isIdrRequired && IsOff(extResetOpt->StartNewSequence))
        return MFX_ERR_INVALID_VIDEO_PARAM; // Reset can't change parameters w/o IDR. Report an error

    mfxExtCodingOption * extOptNew = GetExtBuffer(newPar);
    mfxExtCodingOption * extOptOld = GetExtBuffer(m_video);

    bool brcReset =
        m_video.calcParam.targetKbps != newPar.calcParam.targetKbps ||
        m_video.calcParam.maxKbps    != newPar.calcParam.maxKbps;

    if (brcReset && IsOn(extOptNew->NalHrdConformance) &&
        m_video.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    MFX_CHECK(
        IsAvcProfile(newPar.mfx.CodecProfile)                                   &&
        m_video.AsyncDepth                 == newPar.AsyncDepth                 &&
        m_videoInit.mfx.GopRefDist         >= newPar.mfx.GopRefDist             &&
        m_videoInit.mfx.NumSlice           >= newPar.mfx.NumSlice               &&
        m_video.mfx.NumRefFrame            >= newPar.mfx.NumRefFrame            &&
        m_video.mfx.RateControlMethod      == newPar.mfx.RateControlMethod      &&
        m_video.calcParam.initialDelayInKB == newPar.calcParam.initialDelayInKB &&
        m_video.calcParam.bufferSizeInKB   == newPar.calcParam.bufferSizeInKB   &&
        m_videoInit.mfx.FrameInfo.Width    >= newPar.mfx.FrameInfo.Width        &&
        m_videoInit.mfx.FrameInfo.Height   >= newPar.mfx.FrameInfo.Height       &&
        m_video.mfx.FrameInfo.ChromaFormat == newPar.mfx.FrameInfo.ChromaFormat,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    MFX_CHECK(
        IsOn(extOptOld->FieldOutput) || extOptOld->FieldOutput == extOptNew->FieldOutput,
        MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    return checkStatus;
} // ProcessAndCheckNewParameters

mfxStatus ImplementationAvc::Reset(mfxVideoParam *par)
{
    mfxStatus sts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(par);

    sts = CheckExtBufferId(*par);
    MFX_CHECK_STS(sts);

    MfxVideoParam newPar = *par;
    bool isIdrRequired = false;

    mfxStatus checkStatus = ProcessAndCheckNewParameters(newPar, isIdrRequired);
    if (checkStatus < MFX_ERR_NONE)
        return checkStatus;

    // m_encoding contains few submitted and not queried tasks, wait for their completion
    for (DdiTaskIter i = m_encoding.begin(); i != m_encoding.end(); ++i)
        for (mfxU32 f = 0; f <= i->m_fieldPicFlag; f++)
            while ((sts = QueryStatus(*i, i->m_fid[f])) == MFX_TASK_BUSY)
                vm_time_sleep(1);
    for (size_t size = m_encoding.size(); size-->0;)
        OnEncodingQueried();

    m_emulatorForSyncPart.Init(newPar);
    m_emulatorForAsyncPart = m_emulatorForSyncPart;

    m_hrd.Reset(newPar);
    m_ddi->Reset(newPar);

    m_1stFieldStatus = MFX_ERR_NONE;
    m_fieldCounter   = 0;
    m_stagesToGo     = AsyncRoutineEmulator::STAGE_CALL_EMULATOR;

    mfxExtEncoderResetOption const * extResetOpt = GetExtBuffer(newPar);

    // perform reset of encoder and start new sequence with IDR in following cases:
    // 1) change of encoding parameters require insertion of IDR
    // 2) application explicitly asked about starting new sequence
    if (isIdrRequired || IsOn(extResetOpt->StartNewSequence))
    {
        m_free.splice(m_free.end(), m_incoming);
        m_free.splice(m_free.end(), m_reordering);
        m_free.splice(m_free.end(), m_lookaheadStarted);
        m_free.splice(m_free.end(), m_lookaheadFinished);
        m_free.splice(m_free.end(), m_encoding);

        for (DdiTaskIter i = m_free.begin(); i != m_free.end(); ++i)
        {
            if (i->m_yuv)
                m_core->DecreaseReference(&i->m_yuv->Data);
            *i = DdiTask();
        }

        Zero(m_stat);
        m_lastTask = DdiTask();

        m_frameOrder                  = 0;
        m_frameOrderIdrInDisplayOrder = 0;
        m_frameOrderStartLyncStructure = 0;

        m_1stFieldStatus = MFX_ERR_NONE;
        m_fieldCounter   = 0;

        m_raw.Unlock();
        m_rawLa.Unlock();
        m_mb.Unlock();
        m_rawSys.Unlock();
        m_rec.Unlock();
        m_bit.Unlock();

        // reset of Intra refresh
        mfxExtCodingOption2 * extOpt2New = GetExtBuffer(newPar);
        if (extOpt2New->IntRefType)
        {
            mfxU16 refreshDimension = extOpt2New->IntRefType == HORIZ_REFRESH ? m_video.mfx.FrameInfo.Height >> 4 : m_video.mfx.FrameInfo.Width >> 4;
            m_intraStripeWidthInMBs = (refreshDimension + extOpt2New->IntRefCycleSize - 1) / extOpt2New->IntRefCycleSize;
            m_frameOrderIFrameInDisplayOrder = 0;
        }
    } else if (m_video.calcParam.lyncMode && newPar.calcParam.lyncMode &&
        m_video.calcParam.numTemporalLayer != newPar.calcParam.numTemporalLayer)
    {
        // reset starting point of Lync temporal scalability calculation if number of temporal layers was changed w/o IDR
        m_frameOrderStartLyncStructure = m_frameOrder;
    }

    m_video = newPar;
    return checkStatus;
}

mfxStatus ImplementationAvc::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        if (mfxExtBuffer * buf = GetExtBuffer(m_video.ExtParam, m_video.NumExtParam, par->ExtParam[i]->BufferId))
        {
            if (par->ExtParam[i]->BufferId == MFX_EXTBUFF_CODING_OPTION_SPSPPS)
            {
                // need to generate sps/pps nal units
                mfxExtCodingOptionSPSPPS * dst = (mfxExtCodingOptionSPSPPS *)par->ExtParam[i];

                mfxExtSpsHeader * sps = GetExtBuffer(m_video);
                mfxExtPpsHeader * pps = GetExtBuffer(m_video);

                try
                {
                    if (dst->SPSBuffer)
                    {
                        MFX_CHECK(dst->SPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);
                        OutputBitstream writerSps(dst->SPSBuffer, dst->SPSBufSize);
                        WriteSpsHeader(writerSps, *sps);
                        dst->SPSBufSize = mfxU16((writerSps.GetNumBits() + 7) / 8);
                    }
                    if (dst->PPSBuffer)
                    {
                        MFX_CHECK(dst->PPSBufSize, MFX_ERR_INVALID_VIDEO_PARAM);
                        OutputBitstream writerPps(dst->PPSBuffer, dst->PPSBufSize);
                        WritePpsHeader(writerPps, *pps);
                        dst->PPSBufSize = mfxU16((writerPps.GetNumBits() + 7) / 8);
                    }
                }
                catch (std::exception &)
                {
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                dst->SPSId = sps->seqParameterSetId;
                dst->PPSId = pps->picParameterSetId;
            }
            else
            {
                memcpy(par->ExtParam[i], buf, par->ExtParam[i]->BufferSz);
            }
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    par->mfx = m_video.mfx;
    par->Protected = m_video.Protected;
    par->IOPattern = m_video.IOPattern;
    par->AsyncDepth = m_video.AsyncDepth;

    return MFX_ERR_NONE;
}

mfxStatus ImplementationAvc::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par);

    return MFX_ERR_UNSUPPORTED;
}

mfxStatus ImplementationAvc::GetEncodeStat(mfxEncodeStat *stat)
{
    MFX_CHECK_NULL_PTR1(stat);
    UMC::AutomaticUMCMutex guard(m_listMutex);
    *stat = m_stat;
    return MFX_ERR_NONE;
}

struct CompareByMidRec
{
    mfxMemId m_mid;
    CompareByMidRec(mfxMemId mid) : m_mid(mid) {}
    bool operator ()(DpbFrame const & frame) const { return frame.m_midRec == m_mid; }
};

struct CompareByMidRaw
{
    mfxMemId m_cmSurf;
    CompareByMidRaw(mfxMemId cmSurf) : m_cmSurf(cmSurf) {}
    bool operator ()(DpbFrame const & frame) const { return frame.m_cmRaw == m_cmSurf; }
};

void ImplementationAvc::OnNewFrame()
{
    m_stagesToGo &= ~AsyncRoutineEmulator::STAGE_ACCEPT_FRAME;

    UMC::AutomaticUMCMutex guard(m_listMutex);
    m_reordering.splice(m_reordering.end(), m_incoming, m_incoming.begin());
}


void ImplementationAvc::OnLookaheadSubmitted(DdiTaskIter task)
{
    m_stagesToGo &= ~AsyncRoutineEmulator::STAGE_START_LA;

    if (m_inputFrameType == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        m_core->DecreaseReference(&task->m_yuv->Data);
    m_lookaheadStarted.splice(m_lookaheadStarted.end(), m_reordering, task);
}


void ImplementationAvc::OnLookaheadQueried()
{
    m_stagesToGo &= ~AsyncRoutineEmulator::STAGE_WAIT_LA;

    DdiTask & task = m_lookaheadStarted.front();

    ArrayDpbFrame & iniDpb = task.m_dpb[0];
    ArrayDpbFrame & finDpb = task.m_dpbPostEncoding;
    for (mfxU32 i = 0; i < iniDpb.Size(); i++)
    {
        // m_cmRaw is always filled
        if (std::find_if(finDpb.Begin(), finDpb.End(), CompareByMidRaw(iniDpb[i].m_cmRaw)) == finDpb.End())
        {
            ReleaseResource(m_rawLa, iniDpb[i].m_cmRawLa);
            ReleaseResource(m_mb,    iniDpb[i].m_cmMb);
            if (m_cmDevice)
                m_cmDevice->DestroySurface(iniDpb[i].m_cmRaw);
        }
    }

    ReleaseResource(m_curbe, task.m_cmCurbe);

    if (m_cmDevice)
    {
        m_cmDevice->DestroyVmeSurfaceG7_5(task.m_cmRefs);
        m_cmDevice->DestroyVmeSurfaceG7_5(task.m_cmRefsLa);
    }

    if ((task.GetFrameType() & MFX_FRAMETYPE_REF) == 0)
    {
        ReleaseResource(m_rawLa, task.m_cmRawLa);
        ReleaseResource(m_mb,    task.m_cmMb);
        if (m_cmDevice)
            m_cmDevice->DestroySurface(task.m_cmRaw);
    }

    m_lookaheadFinished.splice(m_lookaheadFinished.end(), m_lookaheadStarted, m_lookaheadStarted.begin());
}


void ImplementationAvc::OnEncodingSubmitted(DdiTaskIter task)
{
    m_stagesToGo &= ~AsyncRoutineEmulator::STAGE_START_ENCODE;

    m_encoding.splice(m_encoding.end(), m_lookaheadFinished, task);
}


void ImplementationAvc::OnEncodingQueried()
{
    m_stagesToGo &= ~AsyncRoutineEmulator::STAGE_WAIT_ENCODE;

    DdiTask & task = m_encoding.front();

    if (m_inputFrameType == MFX_IOPATTERN_IN_VIDEO_MEMORY)
        m_core->DecreaseReference(&task.m_yuv->Data);

    ArrayDpbFrame const & iniDpb = task.m_dpb[task.m_fid[0]];
    ArrayDpbFrame const & finDpb = task.m_dpbPostEncoding;
    for (mfxU32 i = 0; i < iniDpb.Size(); i++)
        if (std::find_if(finDpb.Begin(), finDpb.End(), CompareByMidRec(iniDpb[i].m_midRec)) == finDpb.End())
            ReleaseResource(m_rec, iniDpb[i].m_midRec);

    ReleaseResource(m_raw, task.m_midRaw);
    ReleaseResource(m_bit, task.m_midBit[0]);
    ReleaseResource(m_bit, task.m_midBit[1]);
    if ((task.m_reference[0] + task.m_reference[1]) == 0)
        ReleaseResource(m_rec, task.m_midRec);

    mfxU32 numBits = 8 * (task.m_bsDataLength[0] + task.m_bsDataLength[1]);
    task = DdiTask();

    UMC::AutomaticUMCMutex guard(m_listMutex);

    m_stat.NumBit += numBits;
    m_stat.NumCachedFrame--;
    m_stat.NumFrame++;

    m_free.splice(m_free.end(), m_encoding, m_encoding.begin());
}


namespace
{
    void TEMPORAL_HACK_WITH_DPB(
        ArrayDpbFrame &             dpb,
        mfxMemId const *            mids,
        std::vector<mfxU32> const & fo)
    {
        for (mfxU32 i = 0; i < dpb.Size(); i++)
        {
            mfxU32 idxRec = mfxU32(std::find(fo.begin(), fo.end(), dpb[i].m_frameOrder) - fo.begin());
            dpb[i].m_frameIdx = idxRec;
            dpb[i].m_midRec   = mids[idxRec];
        }
    }

    mfxStatus CountLeadingFF(
        VideoCORE & core,
        DdiTask &   task,
        mfxU32      fid)
    {
        mfxFrameData bitstream = { 0 };

        FrameLocker lock(&core, bitstream, task.m_midBit[fid]);
        if (bitstream.Y == 0)
            return Error(MFX_ERR_LOCK_MEMORY);

        mfxU32 skippedMax = IPP_MIN(15, task.m_bsDataLength[fid]);
        while (*bitstream.Y == 0xff && task.m_numLeadingFF[fid] < skippedMax)
        {
            ++bitstream.Y;
            ++task.m_numLeadingFF[fid];
        }

        return MFX_ERR_NONE;
    }
}


void ImplementationAvc::BrcPreEnc(
    DdiTask const & task)
{
    m_tmpVmeData.resize(m_lookaheadFinished.size());
    DdiTaskIter j = m_lookaheadFinished.begin();
    for (size_t i = 0; i < m_tmpVmeData.size(); ++i, ++j)
        m_tmpVmeData[i] = j->m_vmeData;

    m_brc.PreEnc(task.GetFrameType(), m_tmpVmeData, task.m_encOrder);
}


mfxStatus ImplementationAvc::AsyncRoutine(mfxBitstream * bs)
{
    mfxExtCodingOption    const * extOpt = GetExtBuffer(m_video);
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);

    if (m_stagesToGo == 0)
    {
        UMC::AutomaticUMCMutex guard(m_listMutex);
        m_stagesToGo = m_emulatorForAsyncPart.Go(!m_incoming.empty());
    }

    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_ACCEPT_FRAME)
    {
        DdiTask & newTask = m_incoming.front();

        if (!m_video.mfx.EncodedOrder)
        {
            if (newTask.m_type[0] == 0)
                newTask.m_type     = GetFrameType(m_video, m_frameOrder - m_frameOrderIdrInDisplayOrder);
            newTask.m_frameOrder   = m_frameOrder;
            newTask.m_picStruct    = GetPicStruct(m_video, newTask);
            newTask.m_fieldPicFlag = newTask.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
            newTask.m_fid[0]       = newTask.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
            newTask.m_fid[1]       = newTask.m_fieldPicFlag - newTask.m_fid[0];

            if (newTask.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF)
                std::swap(newTask.m_type.top, newTask.m_type.bot);

            newTask.m_frameOrderIdrInDisplayOrder = m_frameOrderIdrInDisplayOrder;
            newTask.m_frameOrderStartLyncStructure = m_frameOrderStartLyncStructure;

            if (newTask.GetFrameType() & MFX_FRAMETYPE_B)
            {
                newTask.m_loc = GetBiFrameLocation(m_video, m_frameOrder - m_frameOrderIdrInDisplayOrder);
                newTask.m_type[0] |= newTask.m_loc.refFrameFlag;
                newTask.m_type[1] |= newTask.m_loc.refFrameFlag;
            }

            if (newTask.GetFrameType() & MFX_FRAMETYPE_IDR)
            {
                m_frameOrderIdrInDisplayOrder = newTask.m_frameOrder;
                m_frameOrderStartLyncStructure = m_frameOrderIdrInDisplayOrder; // IDR always starts new Lync temporal scalabilty structure
                newTask.m_frameOrderStartLyncStructure = m_frameOrderStartLyncStructure;
            }

            if (newTask.GetFrameType() & MFX_FRAMETYPE_I)
            {
                m_frameOrderIFrameInDisplayOrder = newTask.m_frameOrder;
            }

            newTask.m_IRState = GetIntraRefreshState(
                m_video,
                newTask.m_frameOrder - m_frameOrderIFrameInDisplayOrder,
                newTask.m_ctrl,
                m_intraStripeWidthInMBs);

            m_frameOrder++;
        }
        else
        {
            newTask.m_picStruct    = GetPicStruct(m_video, newTask);
            newTask.m_fieldPicFlag = newTask.m_picStruct[ENC] != MFX_PICSTRUCT_PROGRESSIVE;
            newTask.m_fid[0]       = newTask.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF;
            newTask.m_fid[1]       = newTask.m_fieldPicFlag - newTask.m_fid[0];

            if (newTask.m_picStruct[ENC] == MFX_PICSTRUCT_FIELD_BFF)
                std::swap(newTask.m_type.top, newTask.m_type.bot);
        }

        // move task to reordering queue
        OnNewFrame();
    }

    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_START_LA)
    {
        bool gopStrict = !!(m_video.mfx.GopOptFlag & MFX_GOP_STRICT);

        DdiTaskIter task = (m_video.mfx.EncodedOrder)
            ? m_reordering.begin()
            : FindFrameToEncode(m_lastTask.m_dpbPostEncoding,
                m_reordering.begin(), m_reordering.end(),
                gopStrict, m_reordering.size() < m_video.mfx.GopRefDist);
        if (task == m_reordering.end())
            return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

        task->m_idx    = FindFreeResourceIndex(m_raw);
        task->m_midRaw = AcquireResource(m_raw, task->m_idx);

        mfxStatus sts = GetNativeHandleToRawSurface(*m_core, m_video, *task, task->m_handleRaw);
        if (sts != MFX_ERR_NONE)
            return Error(sts);

        sts = CopyRawSurfaceToVideoMemory(*m_core, m_video, *task);
        if (sts != MFX_ERR_NONE)
            return Error(sts);

        if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA)
        {
            mfxHDLPair cmMb = AcquireResourceUp(m_mb);
            task->m_cmMb    = (CmBufferUP *)cmMb.first;
            task->m_cmMbSys = (void *)cmMb.second;
            task->m_cmRawLa = (CmSurface2D *)AcquireResource(m_rawLa);
            task->m_cmCurbe = (CmBuffer *)AcquireResource(m_curbe);
            task->m_vmeData = FindUnusedVmeData(m_vmeDataStorage);
            if ((!task->m_cmRawLa && extDdi->LaScaleFactor > 1) || !task->m_cmMb || !task->m_cmCurbe || !task->m_vmeData)
                return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

            task->m_cmRaw = CreateSurface(m_cmDevice, task->m_handleRaw.first, m_currentVaType);
        }

        ConfigureTask(*task, m_lastTask, m_video);
        m_lastTask = *task;

        if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA)
        {
            ArrayDpbFrame const & dpb = task->m_dpb[0];
            ArrayU8x33 const &    l0  = task->m_list0[0];
            ArrayU8x33 const &    l1  = task->m_list1[0];

            DdiTask * fwd = 0;
            if (l0.Size() > 0)
                fwd = find_if_ptr2(m_lookaheadFinished, m_lookaheadStarted,
                    FindByFrameOrder(dpb[l0[0] & 127].m_frameOrder));

            DdiTask * bwd = 0;
            if (l1.Size() > 0)
                bwd = find_if_ptr2(m_lookaheadFinished, m_lookaheadStarted,
                    FindByFrameOrder(dpb[l1[0] & 127].m_frameOrder));

            task->m_cmRefs = CreateVmeSurfaceG75(m_cmDevice, task->m_cmRaw,
                fwd ? &fwd->m_cmRaw : 0, bwd ? &bwd->m_cmRaw : 0, !!fwd, !!bwd);

            if (extDdi->LaScaleFactor > 1)
                task->m_cmRefsLa = CreateVmeSurfaceG75(m_cmDevice, task->m_cmRawLa,
                    fwd ? &fwd->m_cmRawLa : 0, bwd ? &bwd->m_cmRawLa : 0, !!fwd, !!bwd);

            task->m_cmRefMb = bwd ? bwd->m_cmMb : 0;
            task->m_fwdRef  = fwd;
            task->m_bwdRef  = bwd;

            SubmitLookahead(*task);
        }

        OnLookaheadSubmitted(task);
    }


    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_WAIT_LA)
    {
        if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA)
            if (!QueryLookahead(m_lookaheadStarted.front()))
                return MFX_TASK_BUSY;

        OnLookaheadQueried();

        if (extDdi->LookAheadDep > 0 && m_lookaheadFinished.size() >= extDdi->LookAheadDep)
        {
            DdiTaskIter end = m_lookaheadFinished.end();
            DdiTaskIter beg = end;
            std::advance(beg, -extDdi->LookAheadDep);

            AnalyzeVmeData(beg, end, m_video.calcParam.widthLa, m_video.calcParam.heightLa);
        }
    }


    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_START_ENCODE)
    {
        DdiTaskIter task = m_lookaheadFinished.begin();

        Hrd hrd = m_hrd; // tmp copy

        if ((task->GetFrameType() & MFX_FRAMETYPE_IDR) &&
            (IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters)))
        {
            if (!m_encoding.empty())
            {
                // wait until all previously submitted encoding tasks are finished
                // start from the last one
                // when the last one is finished all others are finished as well
                mfxStatus sts = MFX_ERR_NONE;
                DdiTask & last = m_encoding.back();
                if ((sts = QueryStatus(last, last.m_fid[1])) != MFX_ERR_NONE)
                    return sts;

                // track hrd buffer
                for (DdiTaskIter i = m_encoding.begin(); i != m_encoding.end(); ++i)
                {
                    for (mfxU32 f = 0; f <= i->m_fieldPicFlag; f++)
                    {
                        if ((sts = QueryStatus(*i, i->m_fid[f])) != MFX_ERR_NONE)
                            return Error(sts);

                        hrd.RemoveAccessUnit(
                            i->m_bsDataLength[i->m_fid[f]] - i->m_numLeadingFF[i->m_fid[f]],
                            i->m_fieldPicFlag,
                            !!(i->m_type[i->m_fid[f]] & MFX_FRAMETYPE_IDR));
                    }
                }
            }
        }

        task->m_initCpbRemoval       = hrd.GetInitCpbRemovalDelay();
        task->m_initCpbRemovalOffset = hrd.GetInitCpbRemovalDelayOffset();

        task->m_idxRecon  = FindFreeResourceIndex(m_rec);
        task->m_idxBs[0]  = FindFreeResourceIndex(m_bit);
        task->m_midRec    = AcquireResource(m_rec, task->m_idxRecon);
        task->m_midBit[0] = AcquireResource(m_bit, task->m_idxBs[0]);
        if (!task->m_midRec || !task->m_midBit[0])
            return Error(MFX_ERR_UNDEFINED_BEHAVIOR);

        if (task->m_fieldPicFlag)
        {
            task->m_idxBs[1]  = FindFreeResourceIndex(m_bit);
            task->m_midBit[1] = AcquireResource(m_bit, task->m_idxBs[1]);
            if (!task->m_midBit[1])
                return Error(MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        // !!! HACK !!!
        m_recFrameOrder[task->m_idxRecon] = task->m_frameOrder;
        TEMPORAL_HACK_WITH_DPB(task->m_dpb[0],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task->m_dpb[1],          m_rec.mids, m_recFrameOrder);
        TEMPORAL_HACK_WITH_DPB(task->m_dpbPostEncoding, m_rec.mids, m_recFrameOrder);


        if (m_enabledSwBrc)
        {
            if (m_video.mfx.RateControlMethod == MFX_RATECONTROL_LA)
                BrcPreEnc(*task);
            task->m_cqpValue[0] = task->m_cqpValue[1] = m_brc.GetQp(task->m_type[task->m_fid[0]], task->m_picStruct[ENC]);
        }
        
        for (mfxU32 f = 0; f <= task->m_fieldPicFlag; f++)
        {
            if (m_useWAForHighBitrates)
                task->m_fillerSize[task->m_fid[f]] = PaddingBytesToWorkAroundHrdIssue(
                    m_video, m_hrd, m_encoding, task->m_fieldPicFlag, f);

            PrepareSeiMessageBuffer(m_video, *task, task->m_fid[f], m_sei);

            mfxStatus sts = m_ddi->Execute(task->m_handleRaw.first, *task, task->m_fid[f], m_sei);
            if (sts != MFX_ERR_NONE)
                return Error(sts);
        }

        OnEncodingSubmitted(task);
    }


    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_WAIT_ENCODE)
    {
        mfxExtCodingOption const * extOpt = GetExtBuffer(m_video);

        DdiTask & task = m_encoding.front();

        mfxStatus sts = MFX_ERR_NONE;
        if (IsOff(extOpt->FieldOutput))
        {
            mfxU32 ffid = task.m_fid[0];
            mfxU32 sfid = task.m_fid[1];

            for (;; ++task.m_repack)
            {
                if ((sts = QueryStatus(task, sfid)) != MFX_ERR_NONE)
                    return sts;

                if (task.m_fieldPicFlag)
                    if ((sts = QueryStatus(task, ffid)) != MFX_ERR_NONE)
                        return Error(sts);

                if (m_enabledSwBrc)
                {
                    mfxU32 res = m_brc.Report(task.m_type[0], task.m_bsDataLength[0], 0, !!task.m_repack, task.m_frameOrder);
                    if (res == 1) // ERR_BIG_FRAME
                    {
                        task.m_bsDataLength[0] = task.m_bsDataLength[1] = 0;
                        task.m_cqpValue[0] = task.m_cqpValue[1] = m_brc.GetQp(task.m_type[task.m_fid[0]], task.m_picStruct[ENC]);
                        sts = m_ddi->Execute(task.m_handleRaw.first, task, task.m_fid[0], m_sei);
                        if (sts != MFX_ERR_NONE)
                            return Error(sts);
                        continue;
                    }
                }

                break;
            }
            task.m_bs = bs;
            for (mfxU32 f = 0; f <= task.m_fieldPicFlag; f++)
                if ((sts = UpdateBitstream(task, task.m_fid[f])) != MFX_ERR_NONE)
                    return Error(sts);

            OnEncodingQueried();
        }
        else
        {
            std::pair<mfxBitstream *, mfxU32> * stupidPair = reinterpret_cast<std::pair<mfxBitstream *, mfxU32> *>(bs);
            assert(stupidPair->second < 2);
            task.m_bs = stupidPair->first;
            mfxU32 fid = task.m_fid[stupidPair->second & 1];

            if ((sts = QueryStatus(task, fid)) != MFX_ERR_NONE)
                return sts;
            if ((sts = UpdateBitstream(task, fid)) != MFX_ERR_NONE)
                return Error(sts);

            if (task.m_fieldCounter == 2)
            {
                OnEncodingQueried();
                UMC::AutomaticUMCMutex guard(m_listMutex);
                m_listOfPairsForStupidFieldOutputMode.pop_front();
                m_listOfPairsForStupidFieldOutputMode.pop_front();
            }
        }
    }

    if (m_stagesToGo & AsyncRoutineEmulator::STAGE_RESTART)
    {
        m_stagesToGo = AsyncRoutineEmulator::STAGE_CALL_EMULATOR;
        return MFX_TASK_BUSY;
    }

    return MFX_TASK_DONE;
}


mfxStatus ImplementationAvc::AsyncRoutineHelper(void * state, void * param, mfxU32, mfxU32)
{
    ImplementationAvc & impl = *(ImplementationAvc *)state;

    if (impl.m_failedStatus != MFX_ERR_NONE)
        return impl.m_failedStatus;
    
    mfxStatus sts = MFX_ERR_NONE;
    try
    {
        sts = impl.AsyncRoutine((mfxBitstream *)param);
        if (sts != MFX_TASK_BUSY && sts != MFX_TASK_DONE)
            impl.m_failedStatus = sts;
    }
    catch (...)
    {
        impl.m_failedStatus = MFX_ERR_DEVICE_FAILED;
        sts = MFX_ERR_DEVICE_FAILED;
    }

    return sts;
}


mfxStatus ImplementationAvc::EncodeFrameCheck(
    mfxEncodeCtrl *           ctrl,
    mfxFrameSurface1 *        surface,
    mfxBitstream *            bs,
    mfxFrameSurface1 **       reordered_surface,
    mfxEncodeInternalParams * internalParams,
    MFX_ENTRY_POINT           entryPoints[],
    mfxU32 &                  numEntryPoints)
{
    mfxExtCodingOption const * extOpt = GetExtBuffer(m_video);
    if (IsOff(extOpt->FieldOutput))
    {
        return EncodeFrameCheckNormalWay(ctrl, surface, bs,
            reordered_surface, internalParams, entryPoints, numEntryPoints);
    }
    else
    {
        if (m_fieldCounter == 0)
        {
            mfxStatus sts = EncodeFrameCheckNormalWay(ctrl, surface, bs,
                reordered_surface, internalParams, entryPoints, numEntryPoints);
            if (sts == MFX_WRN_DEVICE_BUSY || sts < MFX_ERR_NONE)
                return sts;

            UMC::AutomaticUMCMutex guard(m_listMutex);
            m_listOfPairsForStupidFieldOutputMode.push_back(std::make_pair(bs, 0));
            entryPoints->pParam = &m_listOfPairsForStupidFieldOutputMode.back();

            m_fieldCounter = 1;
            m_1stFieldStatus = sts;
            return MFX_ERR_MORE_BITSTREAM;
        }
        else
        {
            m_fieldCounter = 0;

            *reordered_surface = surface;

            UMC::AutomaticUMCMutex guard(m_listMutex);
            m_listOfPairsForStupidFieldOutputMode.push_back(std::make_pair(bs, 1));
            entryPoints[0].pState               = this;
            entryPoints[0].pParam               = &m_listOfPairsForStupidFieldOutputMode.back();
            entryPoints[0].pCompleteProc        = 0;
            entryPoints[0].pGetSubTaskProc      = 0;
            entryPoints[0].pCompleteSubTaskProc = 0;
            entryPoints[0].requiredNumThreads   = 1;
            entryPoints[0].pRoutineName         = "AsyncRoutine";
            entryPoints[0].pRoutine             = AsyncRoutineHelper;
            numEntryPoints = 1;

            return m_1stFieldStatus;
        }
    }
}


namespace
{
    bool IncrementAesCounterAndCheckIfWrapped(
        mfxU32       picStruct,
        AesCounter & aesCounter)
    {
        bool wrapped = aesCounter.Increment();
        if ((picStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            wrapped = aesCounter.Increment();

        return wrapped;
    }
};


mfxStatus ImplementationAvc::EncodeFrameCheckNormalWay(
    mfxEncodeCtrl *           ctrl,
    mfxFrameSurface1 *        surface,
    mfxBitstream *            bs,
    mfxFrameSurface1 **       reordered_surface,
    mfxEncodeInternalParams * ,//internalParams,
    MFX_ENTRY_POINT           entryPoints[],
    mfxU32 &                  numEntryPoints)
{
    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video, ctrl, surface, bs,
        m_core->IsExternalFrameAllocator());
    if (checkSts < MFX_ERR_NONE)
        return checkSts;

    mfxStatus status = checkSts;

    {
        UMC::AutomaticUMCMutex guard(m_listMutex);
        if (m_free.empty())
            return MFX_WRN_DEVICE_BUSY;
    }

    mfxU32 stagesToGo = m_emulatorForSyncPart.Go(!!surface);

    while (stagesToGo & AsyncRoutineEmulator::STAGE_RESTART)
        stagesToGo = m_emulatorForSyncPart.Go(!!surface);

    if (stagesToGo == AsyncRoutineEmulator::STAGE_CALL_EMULATOR)
        return MFX_ERR_MORE_DATA; // end of encoding session

    if ((stagesToGo & AsyncRoutineEmulator::STAGE_WAIT_ENCODE) == 0)
    {
        status = mfxStatus(MFX_ERR_MORE_DATA_RUN_TASK);
        bs = 0; // no output will be generated
    }

    if (surface)
    {
        mfxEncodeCtrl defaultCtrl = {};
        if (ctrl == 0) 
            ctrl = &defaultCtrl;

        UMC::AutomaticUMCMutex guard(m_listMutex);

        m_free.front().m_yuv  = surface;
        m_free.front().m_ctrl = *ctrl;
        m_free.front().m_type = ExtendFrameType(ctrl->FrameType);

        m_free.front().m_extFrameTag = surface->Data.FrameOrder;
        m_free.front().m_frameOrder  = surface->Data.FrameOrder;
        m_free.front().m_timeStamp   = surface->Data.TimeStamp;
        m_core->IncreaseReference(&surface->Data);

        m_stat.NumCachedFrame++;
        m_incoming.splice(m_incoming.end(), m_free, m_free.begin());
    }

    if (bs && IsProtectionPavp(m_video.Protected)) // AES counter is incremented only if output is expected
    {
        mfxU32 picStruct = m_video.mfx.FrameInfo.PicStruct;
        if (IncrementAesCounterAndCheckIfWrapped(picStruct, m_aesCounter))
            status = MFX_WRN_OUT_OF_RANGE;
    }

    *reordered_surface = surface;

    entryPoints[0].pState               = this;
    entryPoints[0].pParam               = bs;
    entryPoints[0].pCompleteProc        = 0;
    entryPoints[0].pGetSubTaskProc      = 0;
    entryPoints[0].pCompleteSubTaskProc = 0;
    entryPoints[0].requiredNumThreads   = 1;
    entryPoints[0].pRoutineName         = "AsyncRoutine";
    entryPoints[0].pRoutine             = AsyncRoutineHelper;
    numEntryPoints = 1;

    return status;
}


void ImplementationAvc::SubmitLookahead(
    DdiTask & task)
{
    task.m_vmeData->poc      = task.GetPoc(0);
    task.m_vmeData->pocL0    = task.m_fwdRef ? task.m_fwdRef->GetPoc(0) : 0xffffffff;
    task.m_vmeData->pocL1    = task.m_bwdRef ? task.m_bwdRef->GetPoc(0) : 0xffffffff;
    task.m_vmeData->encOrder = task.m_encOrder;
    task.m_vmeData->used     = true;

    task.m_event = m_cmCtx->RunVme(task, 26);
}


bool ImplementationAvc::QueryLookahead(
    DdiTask & task)
{
    return m_cmCtx->QueryVme(task, task.m_event);
}


mfxStatus ImplementationAvc::QueryStatus(
    DdiTask & task,
    mfxU32    fid)
{
    if (task.m_bsDataLength[fid] == 0)
    {
        mfxStatus sts = m_ddi->QueryStatus(task, fid);
        if (sts == MFX_WRN_DEVICE_BUSY)
            return MFX_TASK_BUSY;
        if (sts != MFX_ERR_NONE)
            return Error(sts);

        if (m_video.Protected == 0)
            if ((sts = CountLeadingFF(*m_core, task, fid)) != MFX_ERR_NONE)
                return Error(sts);
    }

    return MFX_ERR_NONE;
}


//Estimate cost of input frames p0 b p1, if b==p1 => only p frame
mfxU32 ImplementationAvc::RunPreMeCost(
    FrameTypeAdapt* p0,
    FrameTypeAdapt* b,
    FrameTypeAdapt* p1
    )
{
    mfxU16 ftype = MFX_FRAMETYPE_B;
    int q=26;
#if 1
    if(b == p1)
    {
        ftype = MFX_FRAMETYPE_P;
        q=26;
        if(b == p0){
            q=26;
            ftype = MFX_FRAMETYPE_I;
        }
    }
#endif
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RunPreMeCost");

    //calc biweight
    mfxU32 biWeight = 0;
    if(ftype == MFX_FRAMETYPE_B)
    {
        mfxU32 td = (p1->m_frameNum - p0->m_frameNum);
        mfxU32 tx = (16384+td)/td;
        mfxU32 tb = (b->m_frameNum - p0->m_frameNum);
        biWeight = (tb*tx+128)>>8;
        //fprintf(stderr,"biW: %d  %d\n", tb*tx+32, biWeight);
    }

/*
    m_cmCtx->RunVme(q,
        (IDirect3DSurface9 *)ConvertMidToNativeHandle(*m_core, b->m_surface->Data.MemId),
        b == p0 ? 0 : (IDirect3DSurface9 *)ConvertMidToNativeHandle(*m_core, p0->m_surface->Data.MemId),
        b == p1 ? 0 : (IDirect3DSurface9 *)ConvertMidToNativeHandle(*m_core, p1->m_surface->Data.MemId),
        Begin(m_mbData),
        biWeight,
        b->m_surface4X,
        p0->m_surface4X,
        p1->m_surface4X
*/

    b->intraCost = 0;
    b->interCost = 0;
    b->totalDist = 0;
    b->intraDist = 0;
    b->interDist = 0;
    b->numIntraMb = 0;
    b->mb.resize(m_mbData.size());

    mfxU32 fullCost = 0;
    mfxU32 fullSkip = 0;
    mfxU32 fullDist = 0;
    //mfxU32 intraCost = 0;
    for (size_t i = 0; i < m_mbData.size(); i++)
    {
        if( m_mbData[i].IntraMbFlag )
        {
            //if(ftype != MFX_FRAMETYPE_B)
            fullCost += m_mbData[i].intraCost;
        }else if(!m_mbData[i].SkipMbFlag)
        {
            fullCost += m_mbData[i].interCost;
        }
        fullDist += m_mbData[i].dist;
//        fullCost += m_mb[i].dist;

        if(m_mbData[i].SkipMbFlag) fullSkip++;

        b->mb[i].intraCost     = m_mbData[i].intraCost;
        b->mb[i].interCost     = IPP_MIN(m_mbData[i].intraCost, m_mbData[i].interCost);  //?
        b->mb[i].intraMbFlag   = m_mbData[i].IntraMbFlag;
        b->mb[i].skipMbFlag    = m_mbData[i].SkipMbFlag;
        b->mb[i].mbType        = m_mbData[i].MbType5Bits;
        b->mb[i].subMbShape    = m_mbData[i].SubMbShape;
        b->mb[i].subMbPredMode = m_mbData[i].SubMbPredMode;
        b->mb[i].w1            = 32;//mfxU8(extPps->weightedBipredIdc == 2 ? CalcBiWeight(task, 0, 0) : 32);
        b->mb[i].w0            = 32;//mfxU8(64 - cur->mb[i].w1);
        b->mb[i].dist          = m_mbData[i].dist;
        b->mb[i].costCenter0.x = m_mbData[i].costCenter0X;
        b->mb[i].costCenter0.y = m_mbData[i].costCenter0Y;
        b->mb[i].costCenter1.x = m_mbData[i].costCenter1X;
        b->mb[i].costCenter1.y = m_mbData[i].costCenter1Y;

        Copy(b->mb[i].lumaCoeffSum, m_mbData[i].lumaCoeffSum);
        Copy(b->mb[i].lumaCoeffCnt, m_mbData[i].lumaCoeffCnt);
        Copy(b->mb[i].mv, m_mbData[i].mv);

        b->intraCost += b->mb[i].intraCost;
        b->interCost += b->mb[i].interCost;
        b->numIntraMb += b->mb[i].intraMbFlag;
    }

    //b->totalDist = b->interDist + b->intraDist;
    //fprintf(stderr,"inter: %d  intra:%d\n", b->interCost, b->intraCost);
#if DEBUG_ADAPT
    char* ft="B";
    if(b == p1)
    {
        ft="P";
        if(b == p0) ft="I";
    }
    fprintf(stderr,"%s:%d fullCost=%d fullDist=%d intraMb:%d  w:%d skip:%d   %d:%d  %d:%d  %d:%d\n",
        ft, b->m_frameNum, fullCost, fullDist, b->numIntraMb,
        biWeight, fullSkip,
        p0->m_frameNum, p0->m_frameType, b->m_frameNum, b->m_frameType, p1->m_frameNum, p1->m_frameType);
#endif

    return fullCost;
//    return fullDist;
}


mfxStatus ImplementationAvc::UpdateBitstream(
    DdiTask & task,
    mfxU32    fid)
{
    mfxFrameData bitstream = { 0 };

    mfxU32 fieldNumInStreamOrder = (task.GetFirstField() != fid);

    bool needIntermediateBitstreamBuffer =
        IsSlicePatchNeeded(task, fid) ||
        m_video.calcParam.numTemporalLayer > 0 ||
        (m_video.mfx.NumRefFrame & 1);

    bool doPatch =
        needIntermediateBitstreamBuffer ||
        IsInplacePatchNeeded(m_video, task, fid);

    if (m_caps.HeaderInsertion == 0 && (m_currentPlatform != MFX_HW_IVB || m_core->GetVAType() != MFX_HW_VAAPI) || m_video.Protected != 0)
        doPatch = needIntermediateBitstreamBuffer = false;

    // Lock d3d surface with compressed picture.
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface lock (bitstream)");
    MFX_LTRACE_S(MFX_TRACE_LEVEL_INTERNAL, task.m_FrameName);

    FrameLocker lock(m_core, bitstream, task.m_midBit[fid]);
    if (bitstream.Y == 0)
        return Error(MFX_ERR_LOCK_MEMORY);

    MFX_AUTO_TRACE_STOP();

    mfxU32 skippedff = task.m_numLeadingFF[fid];
    task.m_bsDataLength[fid] -= skippedff;
    bitstream.Y += skippedff;

    // In case of encrypted bitsteam aligned number of bytes should be copied.
    // For AES it should be a multiple of 16.
    // While DataLength in mfxBitstream remains is what returned from Query (may be not aligned).
    mfxU32   bsSizeActual  = task.m_bsDataLength[fid];
    mfxU32   bsSizeToCopy  = task.m_bsDataLength[fid];
    mfxU32   bsSizeAvail   = task.m_bs->MaxLength - task.m_bs->DataOffset - task.m_bs->DataLength;
    mfxU8 *  bsData        = task.m_bs->Data + task.m_bs->DataOffset + task.m_bs->DataLength;
    mfxU32 * dataLength    = &task.m_bs->DataLength;

    if (m_video.Protected == 0)
    {
        if (needIntermediateBitstreamBuffer)
        {
            bsData      = &m_tmpBsBuf[0];
            bsSizeAvail = mfxU32(m_tmpBsBuf.size());
        }
    }
    else
    {
        mfxEncryptedData * edata = GetEncryptedData(*task.m_bs, fieldNumInStreamOrder);

        bsData        = edata->Data + edata->DataOffset + edata->DataLength;
        dataLength    = &edata->DataLength;
        bsSizeToCopy  = AlignValue(bsSizeActual, 16);
        bsSizeAvail   = edata->MaxLength - edata->DataOffset - edata->DataLength;
    }

    mfxU32 initialDataLength = *dataLength;

    assert(bsSizeToCopy <= bsSizeAvail);

    if (bsSizeToCopy > bsSizeAvail)
    {
        bsSizeToCopy = bsSizeAvail;
        bsSizeActual = bsSizeAvail;
        if (m_video.Protected)
        {
            bsSizeToCopy = AlignValue(bsSizeToCopy - 15, 16);
            bsSizeActual = IPP_MIN(bsSizeActual, bsSizeToCopy);
        }
    }

    // Copy compressed picture from d3d surface to buffer in system memory
    FastCopyBufferVid2Sys(bsData, bitstream.Y, bsSizeToCopy);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface unlock (bitstream)");
        MFX_LTRACE_S(MFX_TRACE_LEVEL_INTERNAL, task.m_FrameName);
        mfxStatus sts = lock.Unlock();
        MFX_CHECK_STS(sts);
    }

    if (doPatch)
    {
        mfxU8 * dbegin = bsData;
        mfxU8 * dend   = bsData + bsSizeActual;

        if (needIntermediateBitstreamBuffer)
        {
            dbegin = task.m_bs->Data + task.m_bs->DataOffset + task.m_bs->DataLength;
            dend   = task.m_bs->Data + task.m_bs->MaxLength;
        }

        mfxU8 * endOfPatchedBitstream =
            PatchBitstream(m_video, task, fid, bsData, bsData + bsSizeActual, dbegin, dend);

        *dataLength += (mfxU32)(endOfPatchedBitstream - dbegin);
    }
    else
    {
        *dataLength += bsSizeActual;
    }

    if (m_enabledSwBrc)
    {
        mfxU32 minFrameSize = m_brc.GetMinFrameSize();
        mfxU32 frameSize = *dataLength - initialDataLength;
        bsData      += frameSize;
        bsSizeAvail -= frameSize;
        if (frameSize < minFrameSize)
        {
            CheckedMemset(bsData, bsData + bsSizeAvail, 0, minFrameSize - frameSize);
            *dataLength += minFrameSize - frameSize;
            mfxU32 brcStatus = m_brc.Report(task.m_type[fid], minFrameSize, minFrameSize - frameSize, 1, task.m_frameOrder * 2 + 0);
            brcStatus;
            assert(brcStatus == 0);
        }
        else
        {
            CheckedMemset(bsData, bsData + bsSizeAvail, 0, skippedff);
            *dataLength += skippedff;
        }
    }

    // Update bitstream fields
    task.m_bs->TimeStamp = task.m_timeStamp;
    task.m_bs->DecodeTimeStamp = CalcDTSFromPTS(m_video.mfx.FrameInfo, mfxU16(task.m_dpbOutputDelay), task.m_timeStamp);
    task.m_bs->PicStruct = task.GetPicStructForDisplay();
    task.m_bs->FrameType = task.m_type[task.GetFirstField()] & ~MFX_FRAMETYPE_KEYPIC;
    if (task.m_fieldPicFlag)
        task.m_bs->FrameType = mfxU16(task.m_bs->FrameType | ((task.m_type[!task.GetFirstField()]& ~MFX_FRAMETYPE_KEYPIC) << 8));

    // to support WinBlue HMFT API related to LTR control Intel HMFT should return actual reference lists to application
    // MSDK release for WinBlie officially uses API 1.6 which is insufficient to provide HMFT with required data
    // encoder unofficially uses new MSDK API 1.7 to return ref lists to HMFT
    if (task.m_fieldPicFlag == 0)
    {
        // use updated structure mfxBitstream from API 1.7 to attach ext buffer
        mfxBitstream * bs = (mfxBitstream*)task.m_bs;
        if (bs->NumExtParam == 1)  //olny 1 ext buffer is supported for mfxBitstream at the moment. Treat other number as incorrect and ignore ext buffers
        {
            mfxExtAVCEncodedFrameInfo * encFrameInfo = (mfxExtAVCEncodedFrameInfo*)GetExtBuffer(bs->ExtParam, bs->NumExtParam, MFX_EXTBUFF_ENCODED_FRAME_INFO);
            if (encFrameInfo)
            {
                encFrameInfo->FrameOrder = task.m_extFrameTag;
                encFrameInfo->LongTermIdx = task.m_longTermFrameIdx == NO_INDEX_U8 ? NO_INDEX_U16 : task.m_longTermFrameIdx;

                // only return of ref list L0 is supported at the moment
                mfxU8 i = 0;
                for (i = 0; i < task.m_list0[0].Size(); i ++)
                {
                    DpbFrame& refFrame = task.m_dpb[0][task.m_list0[0][i] & 127]; // retrieve corresponding ref frame from DPB
                    encFrameInfo->UsedRefListL0[i].FrameOrder = refFrame.m_extFrameTag;
                    if (refFrame.m_longterm && refFrame.m_longTermIdxPlus1) // reference frame is LTR with valid LTR idx
                        encFrameInfo->UsedRefListL0[i].LongTermIdx = refFrame.m_longTermIdxPlus1 - 1;
                    else
                        encFrameInfo->UsedRefListL0[i].LongTermIdx = NO_INDEX_U16;
                    encFrameInfo->UsedRefListL0[i].PicStruct = (mfxU16)MFX_PICSTRUCT_PROGRESSIVE;
                }

                for (; i < 32; i ++)
                {
                    encFrameInfo->UsedRefListL0[i].FrameOrder  = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
                    encFrameInfo->UsedRefListL0[i].LongTermIdx = NO_INDEX_U16;
                    encFrameInfo->UsedRefListL0[i].PicStruct   = (mfxU16)MFX_PICSTRUCT_UNKNOWN;
                }
            }
        }
    }

    if (m_video.Protected != 0)
    {
        // Return aes counter compressed picture encrypted with
        mfxEncryptedData * edata = GetEncryptedData(*task.m_bs, fieldNumInStreamOrder);
        edata->CipherCounter = task.m_aesCounter[fid];
    }

    task.m_fieldCounter++;

    // Update hrd buffer
    m_hrd.RemoveAccessUnit(
        task.m_bsDataLength[fid],
        task.m_fieldPicFlag,
        (task.m_type[fid] & MFX_FRAMETYPE_IDR) != 0);

    return MFX_ERR_NONE;
}

#if 0
mfxStatus ImplementationAvc::AdaptiveGOP(
    mfxEncodeCtrl**           ctrl,
    mfxFrameSurface1**        surface,
    mfxU16* requiredFrameType)
{
    static int fnum = 0;
    //Create new frame, consider only display order
    FrameTypeAdapt* new_frame = NULL;
    //mfxU32 numMB = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height / 256;
    if(*surface)
    {
        new_frame = new FrameTypeAdapt(m_cmDevice,m_video.mfx.FrameInfo.Width,m_video.mfx.FrameInfo.Height);
#define NORMAL_TYPE 1
#if NORMAL_TYPE
        int posInGOP = fnum % m_video.mfx.GopPicSize;
        if( posInGOP == 0)
        {
            new_frame->m_frameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
            new_frame->m_isAdapted = 0;
        }else
        {
            int posInMiniGOP = (posInGOP-1) % m_video.mfx.GopRefDist;
            if(posInMiniGOP == (m_video.mfx.GopRefDist - 1))
            {
                new_frame->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                new_frame->m_isAdapted = 1;
            }else
            {
                new_frame->m_frameType = MFX_FRAMETYPE_B;
                new_frame->m_isAdapted = 1;
#if 0
                if(posInMiniGOP == (m_video.mfx.GopRefDist - 2))
                {
                    new_frame->m_frameType |= MFX_FRAMETYPE_REF;
                }
#endif
            }
        }
#endif
        new_frame->m_frameNum = fnum++;
        new_frame->m_ctrl = *ctrl;
        new_frame->m_surface = *surface;
        //downsample //FIXME
        //m_cmCtx->DownSample(GetNativeSurfaceHandle(m_core, new_frame->m_surface->Data.MemId),
        //    &(new_frame->m_surface4X.GetDXSurface()));
        //drop surface to file
#if 0
        {
            /* dump DownSampled image */
            FILE* FdsSurf = fopen("dsSurf.yuv", "a+b");

            mfxU32 numMbCols = m_video.mfx.FrameInfo.Width / 16;
            mfxU32 numMbRows = m_video.mfx.FrameInfo.Height / 16;
            mfxU8 *dsSurf = new mfxU8[((numMbCols * 4) * (numMbRows * 4) * 3) / 2];
            new_frame->m_surface4X.Read(dsSurf);
            fwrite(dsSurf,1,(numMbCols * 4) * (numMbRows * 4), FdsSurf);
            
            mfxU8 *pUV = dsSurf + ((numMbCols * 4) * (numMbRows * 4));
            memset(pUV,128,((numMbCols * 4) * (numMbRows * 4))/2);
            for (int i = 0; i < ((numMbCols * 4) * (numMbRows * 4)) >> 2; i++)
            {
                fwrite(pUV+2*i,1,1, FdsSurf);
            }
            for (int i = 0; i < ((numMbCols * 4) * (numMbRows * 4)) >> 2; i++)
            {
                fwrite(pUV+2*i+1,1,1, FdsSurf);
            }
            fflush(FdsSurf);
            delete[] dsSurf;
            fclose(FdsSurf);
        }
#endif

        //fprintf(stderr,"frame type:%d  surface:%x\n", new_frame->m_frameType & MFX_FRAMETYPE_IPB, surface);
        m_bufferedInFrames.push_back(new_frame);
        //Lock surface
        m_core->IncreaseReference(&(*surface)->Data);
        //m_readyInFrames.push_back(new_frame);
    }

#if 0
    //check if have enough frames to analyse
    if(m_bufferedInFrames.size() == m_video.mfx.GopRefDist + 1 ||
        (m_bufferedInFrames.size()>0 && *surface == 0))
    {
        //Detect best frame type
        mfxU32 numMB = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height / 256;
        if(m_bufferedInFrames.size()>0 && *surface == 0)
        {
            int last = m_bufferedInFrames.size()-1;
            m_bufferedInFrames[last]->m_isAdapted = 0;
            m_bufferedInFrames[last]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }

        int i=1;

        for(i=1; i<m_bufferedInFrames.size()-1; i++ )
        {
            //fprintf(stderr,"analyse: %d sz:%d\n",m_bufferedInFrames[i]->m_frameNum, m_bufferedInFrames.size());
            if(!m_bufferedInFrames[i]->m_isAdapted) break;
#if 0
            mfxU32 costPn = RunPreMeCost(m_bufferedInFrames[i-1],
                m_bufferedInFrames[i],
                m_bufferedInFrames[i]);
#endif

            mfxU32 costBP = RunPreMeCost(m_bufferedInFrames[i-1],
                m_bufferedInFrames[i],
                m_bufferedInFrames[i+1]);


            mfxU32 costP1 = RunPreMeCost(m_bufferedInFrames[i-1],
                m_bufferedInFrames[i],
                m_bufferedInFrames[i]);

            mfxU32 costP2 = RunPreMeCost(m_bufferedInFrames[i],
                m_bufferedInFrames[i+1],
                m_bufferedInFrames[i+1]);

            mfxU32 costP0 = RunPreMeCost(m_bufferedInFrames[i-1],
                m_bufferedInFrames[i+1],
                m_bufferedInFrames[i+1]);
#if 0
            if( i+2 < m_bufferedInFrames.size())
            {
                mfxU32 costP3 = RunPreMeCost(m_bufferedInFrames[i-1],
                    m_bufferedInFrames[i+2],
                    m_bufferedInFrames[i+2]);
                fprintf(stderr,"\n");

                mfxU32 costB1P = RunPreMeCost(m_bufferedInFrames[i-1],
                    m_bufferedInFrames[i],
                    m_bufferedInFrames[i+2]);
                mfxU32 costB2P = RunPreMeCost(m_bufferedInFrames[i-1],
                    m_bufferedInFrames[i+1],
                    m_bufferedInFrames[i+2]);
            }

#endif
            //                    mfxU32 qpP = m_brc.GetQp(MFX_FRAMETYPE_P, MFX_PICSTRUCT_PROGRESSIVE);
            //                    mfxU32 qpB = m_brc.GetQp(MFX_FRAMETYPE_B, MFX_PICSTRUCT_PROGRESSIVE);

            //fprintf(stderr,"costs  P1:%u P2:%u  BP:%u  P0=%u\n", costP1, costP2, costBP, costP0 );
            //fprintf(stderr,"costs  PP:%u  BP:%f\n", costP1 + costP2, 0.8*costBP + costP0);
            if( m_bufferedInFrames[i+1]->m_frameType & MFX_FRAMETYPE_IDR )
            {
                m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                break;
            }
#if 1
            if( m_bufferedInFrames[i+1]->numIntraMb > numMB/2 ) //too much intra MBs for B frame
            {
                m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                m_bufferedInFrames[i+1]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                i++;
                break;
            }
#endif
            //convert to P if two P better
            if( costP1 + costP2 < 0.8*costBP + costP0 )
            {
                m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                break;
            }

            //if B
            m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_B;

            //calc costP_rP0, cost_B_rP0_rP1, cost_P_rP0, cost_P_rP1 and compare them
            //check number of intra blocks (if > 1/3 => should be P)
            //find farest P with the cost <= 1.3*costP0?
#define MMIN(a,b) ((a) < (b)? (a): (b))
            for(int k=i+1; k<MMIN(m_bufferedInFrames.size(),m_video.mfx.GopRefDist + 1); k++)
            {
                if(!m_bufferedInFrames[k]->m_isAdapted) break;

                mfxU32 costP = RunPreMeCost(m_bufferedInFrames[i-1],
                    m_bufferedInFrames[k],
                    m_bufferedInFrames[k]);
                //fprintf(stderr,"costP: %u intra:%d\n", costP, m_bufferedInFrames[k]->numIntraMb);
                if( costP > 1.3*costP0 ||
                    m_bufferedInFrames[k]->numIntraMb > numMB/3)
                {
                    i = k;
                    m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                    break;
                }else if(k-i >= m_video.mfx.GopRefDist - 1) //restriction in number of B frames
                {
                    i = k;
                    m_bufferedInFrames[i]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                    break;
                }else
                {
                    m_bufferedInFrames[k]->m_frameType = MFX_FRAMETYPE_B;
                }
            }
            //fprintf(stderr,"endi: %d\n",i);
            break;
        }

        //move to ready queue
        for(int k=0; k<i; k++)
        {
            m_readyInFrames.push_back(m_bufferedInFrames[k]);
        }
        m_bufferedInFrames.erase(m_bufferedInFrames.begin(), m_bufferedInFrames.begin()+i);
    }
#endif
    //check if we have surface for new task
    if(m_readyInFrames.size() > 0)
    {
        FrameTypeAdapt* frame = m_readyInFrames.front();
        *ctrl =  frame->m_ctrl;
        *surface = frame->m_surface;
        m_core->DecreaseReference(&frame->m_surface->Data);
        m_readyInFrames.erase(m_readyInFrames.begin());
        *requiredFrameType =  frame->m_frameType;
        if((*requiredFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_P )
            *requiredFrameType |= MFX_FRAMETYPE_REF;

        //printf("submit: %d t: %d\n", frame->m_frameNum, frame->m_frameType & MFX_FRAMETYPE_IPB);
        delete frame;
        //goto assign task
    }else
    {
        if(*surface != 0)
        {
            //fprintf(stderr,"err more data\n");
            return MFX_ERR_MORE_DATA;
        }
        //fprintf(stderr,"no frames in pool\n");
        //otherwise goto assign task
    }
    return MFX_ERR_NONE;
}

#define OPT_GOP1 0
#if OPT_GOP1 == 0
mfxStatus ImplementationAvc::AdaptiveGOP1(
    mfxEncodeCtrl**           ctrl,
    mfxFrameSurface1**        surface,
    mfxU16* requiredFrameType)
{
   //return AdaptiveGOP(ctrl, surface, requiredFrameType);
   
   //mfxU32 numMB = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height / 256;

#define MAX_B_FRAMES 255
#define MAX_COST 0x07FFFFFF

    //mfxU16  bestForLen[MAX_B_FRAMES][MAX_B_FRAMES];
    //mfxU32  bestCost[MAX_B_FRAMES];

    static int fnum = 0;
    //Create new frame, consider only display order
    FrameTypeAdapt* new_frame = NULL;
    if(*surface)
    {
        new_frame = new FrameTypeAdapt(m_cmDevice,m_video.mfx.FrameInfo.Width,m_video.mfx.FrameInfo.Height);

#define NORMAL_TYPE 1 
#if NORMAL_TYPE
        int posInGOP = fnum % m_video.mfx.GopPicSize;
        if( posInGOP == 0)
        {
            new_frame->m_frameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
            new_frame->m_isAdapted = 0;
        }else
        {
            int posInMiniGOP = (posInGOP-1) % m_video.mfx.GopRefDist;
            if(posInMiniGOP == (m_video.mfx.GopRefDist - 1))
            {
                new_frame->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                new_frame->m_isAdapted = 1;
            }else
            {
                new_frame->m_frameType = MFX_FRAMETYPE_B;
                new_frame->m_isAdapted = 1;
#if 0
                if(posInMiniGOP == (m_video.mfx.GopRefDist - 2))
                {
                    new_frame->m_frameType |= MFX_FRAMETYPE_REF;
                }
#endif
            }
        }
#endif
        new_frame->m_frameNum = fnum++;
        new_frame->m_ctrl = *ctrl;
        new_frame->m_surface = *surface;
        //fprintf(stderr,"frame type:%d  surface:%x\n", new_frame->m_frameType & MFX_FRAMETYPE_IPB, surface);
        //downsample
        CmSurface orig_surf(m_cmDevice, (IDirect3DSurface9 *)ConvertMidToNativeHandle(*m_core, new_frame->m_surface->Data.MemId));
        m_cmCtx->DownSample(orig_surf.GetIndex(),
            new_frame->m_surface4X.GetIndex());
        //drop surface to file
#if 0
        {
            /* dump DownSampled image */
            FILE* FdsSurf = fopen("dsSurf.yuv", "a+b");

            mfxU32 numMbCols = m_video.mfx.FrameInfo.Width / 16;
            mfxU32 numMbRows = m_video.mfx.FrameInfo.Height / 16;
            mfxU8 *dsSurf = new mfxU8[((numMbCols * 4) * (numMbRows * 4) * 3) / 2];
            new_frame->m_surface4X.Read(dsSurf);
            fwrite(dsSurf,1,(numMbCols * 4) * (numMbRows * 4), FdsSurf);
            
            mfxU8 *pUV = dsSurf + ((numMbCols * 4) * (numMbRows * 4));
            memset(pUV,128,((numMbCols * 4) * (numMbRows * 4))/2);
            for (int i = 0; i < ((numMbCols * 4) * (numMbRows * 4)) >> 2; i++)
            {
                fwrite(pUV+2*i,1,1, FdsSurf);
            }
            for (int i = 0; i < ((numMbCols * 4) * (numMbRows * 4)) >> 2; i++)
            {
                fwrite(pUV+2*i+1,1,1, FdsSurf);
            }
            fflush(FdsSurf);
            delete[] dsSurf;
            fclose(FdsSurf);
        }
#endif

        m_bufferedInFrames.push_back(new_frame);
        //Lock surface
        m_core->IncreaseReference(&(*surface)->Data);
        //m_readyInFrames.push_back(new_frame);
        //fnum++;
    }

#if 0
    //check if have enough frames to analyse
    if(m_bufferedInFrames.size() == m_video.mfx.GopRefDist + 1 ||
        (m_bufferedInFrames.size()>0 && *surface == 0))
    {
        //Detect best frame type
        if(m_bufferedInFrames.size()>0 && *surface == 0)
        {
            int last = m_bufferedInFrames.size()-1;
            m_bufferedInFrames[last]->m_isAdapted = 0;
            m_bufferedInFrames[last]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }

        //find in a sequence optimal solutions for len 1,2,3,4...GopRefDist
        //optimum for 1 is P
        //optimum for 2 is PP or BP
        //optimum for 3 is PPP, BBP, PBP, BPP (get optimum for len=2 and add one more)
        //optimum for 4 is PPPP, BBBP, PBBP, PBPP, PPBP, BPBP, BPPP

        int maxLen = MMIN(m_video.mfx.GopRefDist,m_bufferedInFrames.size()-1);
        //check IDR/I frame in the next sequence
        int shiftOffset=1; //how many frames shift to ready query
        int i;
        for(i=1; i<maxLen; i++)
            if(m_bufferedInFrames[i]->m_frameType&MFX_FRAMETYPE_I) break;
        maxLen = i;

        if(maxLen > 1) //no need to check for only P frames
        {
            bestForLen[0][0] = MFX_FRAMETYPE_P;
            bestForLen[1][0] = MFX_FRAMETYPE_P; //idx==len
            bestCost[0] = MAX_COST;
            for(int i=1; i<maxLen; i++) //check all possible lengths 
            {
                int idx=0;
                bestCost[i] = MAX_COST;
                mfxU16 bestSequence[2][MAX_B_FRAMES];
                for(int k=0; k<=i; k++) //try different len of B frames
                {
                    //all previous best cases + P frame
                    //and all B  BB..BP pattern                    
                    memcpy(&bestSequence[idx][0], &bestForLen[i-k][0], (i-k)*sizeof(mfxU16));
                    //Set B
                    for(int l=i-k; l<i; l++) bestSequence[idx][l] = MFX_FRAMETYPE_B;
                    bestSequence[idx][i] = MFX_FRAMETYPE_P;
                    //get cost for this path
            
                    //calc pash cost
                    int prevP = 0;
                    int nextP = 0;
                    mfxU32 cost = 0;
                    //find nextP
                    if(nextP != i )
                    {
                        while(bestSequence[idx][nextP] != (mfxU16)MFX_FRAMETYPE_P) nextP++;
                        nextP++;
                    }
                    for(int m=0; m<i+1; m++)
                    {
                        if(bestSequence[idx][m] == MFX_FRAMETYPE_B)
                        {
                            cost += 0.84*RunPreMeCost(m_bufferedInFrames[prevP],
                                m_bufferedInFrames[m+1],
                                m_bufferedInFrames[nextP]);
                        }else if(bestSequence[idx][m] == MFX_FRAMETYPE_P)
                        {
                            cost += RunPreMeCost(m_bufferedInFrames[prevP],
                                m_bufferedInFrames[m+1],
                                m_bufferedInFrames[m+1]);
                            //find nextP
                            if( m != i) //if not last
                            {
                                prevP = m+1;
                                nextP = prevP;
                                while(bestSequence[idx][nextP] != (mfxU16)MFX_FRAMETYPE_P) nextP++;
                                nextP++;
                            }
                        }
                    }

                    if( cost < bestCost[i] )
                    {
                        bestCost[i] = cost;
                        idx = (++idx)&1;
                    }
                }
                //copy best path
                idx = (++idx)&1;
                for(int l=0; l<=i; l++) bestForLen[i+1][l] = bestSequence[idx][l];
#if DEBUG_ADAPT
                    fprintf(stderr,"best path: %d %d ", i, bestCost[i]);
                    for(int m=0; m<i+1; m++)
                    {
                        switch(bestSequence[idx][m])
                        {
                        case MFX_FRAMETYPE_I:
                            fprintf(stderr,"I");
                            break;
                        case MFX_FRAMETYPE_B:
                            fprintf(stderr,"B");
                            break;
                        case MFX_FRAMETYPE_P:
                            fprintf(stderr,"P");
                            break;
                        }
                    }
                    fprintf(stderr,"\n\n");
#endif

            }
            //find first P 
            for(int l=0; l<maxLen; l++)
            {
                m_bufferedInFrames[l+1]->m_frameType = bestForLen[maxLen][l];
                if(bestForLen[maxLen][l] == MFX_FRAMETYPE_P){ shiftOffset = l+1; break; }  
            }
        }

        //move to ready queue
        for(int k=0; k<shiftOffset; k++)
        {
            m_readyInFrames.push_back(m_bufferedInFrames[k]);
        }
        m_bufferedInFrames.erase(m_bufferedInFrames.begin(), m_bufferedInFrames.begin()+shiftOffset);
    }
#endif

    //check if we have surface for new task
    if(m_readyInFrames.size() > 0)
    {
        FrameTypeAdapt* frame = m_readyInFrames.front();
        *ctrl =  frame->m_ctrl;
        *surface = frame->m_surface;
        *requiredFrameType =  frame->m_frameType;
        if((*requiredFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_P )
            *requiredFrameType |= MFX_FRAMETYPE_REF;
        //fprintf(stderr,"submit: %d t: %d\n", frame->m_frameNum, frame->m_frameType & MFX_FRAMETYPE_IPB);
        m_core->DecreaseReference(&frame->m_surface->Data);
        m_readyInFrames.erase(m_readyInFrames.begin());
        delete frame;
        //goto assign task
    }else
    {
        if(*surface != 0)
        {
            //fprintf(stderr,"err more data\n");
            return MFX_ERR_MORE_DATA;
        }
        //fprintf(stderr,"no frames in pool\n");
        //otherwise goto assign task
    }
    return MFX_ERR_NONE;
}

#else

mfxStatus ImplementationAvc::AdaptiveGOP1(
    mfxEncodeCtrl**           ctrl,
    mfxFrameSurface1**        surface,
    mfxU16* requiredFrameType)
{
    if (m_video.mfx.GopOptFlag&8)
        return AdaptiveGOP(ctrl, surface, requiredFrameType);
   mfxU32 numMB = m_video.mfx.FrameInfo.Width * m_video.mfx.FrameInfo.Height / 256;

#define MAX_B_FRAMES 255
#define MAX_COST 0x07FFFFFF

    mfxU16  bestForLen[MAX_B_FRAMES][MAX_B_FRAMES];
    mfxU32  bestCost[MAX_B_FRAMES];

    static int fnum = 0;
    //Create new frame, consider only display order
    FrameTypeAdapt* new_frame = NULL;
    if(*surface)
    {
        new_frame = new FrameTypeAdapt(numMB);

#define NORMAL_TYPE 1
#if NORMAL_TYPE
        int posInGOP = fnum % m_video.mfx.GopPicSize;
        if( posInGOP == 0)
        {
            new_frame->m_frameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR;
            new_frame->m_isAdapted = 0;
        }else
        {
            int posInMiniGOP = (posInGOP-1) % m_video.mfx.GopRefDist;
            if(posInMiniGOP == (m_video.mfx.GopRefDist - 1))
            {
                new_frame->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                new_frame->m_isAdapted = 1;
            }else
            {
                new_frame->m_frameType = MFX_FRAMETYPE_B;
                new_frame->m_isAdapted = 1;
#if 0
                if(posInMiniGOP == (m_video.mfx.GopRefDist - 2))
                {
                    new_frame->m_frameType |= MFX_FRAMETYPE_REF;
                }
#endif
            }
        }
#endif
        new_frame->m_frameNum = fnum++;
        new_frame->m_ctrl = *ctrl;
        new_frame->m_surface = *surface;
        //fprintf(stderr,"frame type:%d  surface:%x\n", new_frame->m_frameType & MFX_FRAMETYPE_IPB, surface);
        m_bufferedInFrames.push_back(new_frame);
        //Lock surface
        m_core->IncreaseReference(&(*surface)->Data);
        //m_readyInFrames.push_back(new_frame);
    }

#if 1
    //check if have enough frames to analyse
    if(m_bufferedInFrames.size() == m_video.mfx.GopRefDist + 1 ||
        (m_bufferedInFrames.size()>0 && *surface == 0))
    {
        //Detect best frame type
        if(m_bufferedInFrames.size()>0 && *surface == 0)
        {
            int last = m_bufferedInFrames.size()-1;
            m_bufferedInFrames[last]->m_isAdapted = 0;
            m_bufferedInFrames[last]->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }

        //find in a sequence optimal solutions for len 1,2,3,4...GopRefDist
        //optimum for 1 is P
        //optimum for 2 is one of PP or BP
        //optimum for 3 is one of PPP, BBP, PBP, BPP (get optimum for len=2 and add one more)
        //optimum for 4 is one of PPPP, BBBP, PBBP, PBPP, PPBP, BPBP, BPPP

        int maxLen = MMIN(m_video.mfx.GopRefDist,m_bufferedInFrames.size()-1);
        //check IDR/I frame in the next sequence
        int shiftOffset=1; //how many frames shift to ready query
        int i;
        for(i=1; i<=maxLen; i++)
            if(m_bufferedInFrames[i]->m_frameType&MFX_FRAMETYPE_I) break;

        maxLen = i-1;

        if(maxLen > 1) //no need to check for only P frames
        {
            bestForLen[0][0] = MFX_FRAMETYPE_P;
            bestForLen[1][0] = MFX_FRAMETYPE_P; //idx==len
            //first P , len B = 0
            bestCost[0] = RunPreMeCost(m_bufferedInFrames[0],
                                m_bufferedInFrames[1],
                                m_bufferedInFrames[1]);

            for(int i=1; i<maxLen; i++) //check all possible lengths
            {
                int idx=0;
                bestCost[i] = MAX_COST;
                mfxU16 bestSequence[2][MAX_B_FRAMES];
                for(int k=0; k<=i; k++) //try different len of B frames
                {
                    //all previous best cases + P frame
                    //and all B  BB..BP pattern
                    memcpy(&bestSequence[idx][0], &bestForLen[i-k][0], (i-k)*sizeof(mfxU16));
                    //Set B
                    for(int l=i-k; l<i; l++) bestSequence[idx][l] = MFX_FRAMETYPE_B;
                    bestSequence[idx][i] = MFX_FRAMETYPE_P;
                    //get cost for this path

                    //calc path cost
                    int prevP = 0;
                    mfxU32 cost = 0;
                    if( k < i){
                        prevP = i-k;
                        cost = bestCost[i-k-1];
                    }
                    int nextP = prevP;
                    //find nextP
                    if(nextP != i )
                    {
                        while(bestSequence[idx][nextP] != (mfxU16)MFX_FRAMETYPE_P) nextP++;
                        nextP++;
                    }
#if DEBUG_ADAPT
                    fprintf(stderr,"path: %d pP:%d %d ", cost, prevP, nextP);
                    for(int m=0; m<i+1; m++)
                    {
                        switch(bestSequence[idx][m])
                        {
                        case MFX_FRAMETYPE_I:
                            fprintf(stderr,"I");
                            break;
                        case MFX_FRAMETYPE_B:
                            fprintf(stderr,"B");
                            break;
                        case MFX_FRAMETYPE_P:
                            fprintf(stderr,"P");
                            break;
                        }
                    }
                    fprintf(stderr,"\n");
#endif
                    //best path from previous len + current path
                    for(int m=i-k; m<i+1; m++)
                    {
                        if(bestSequence[idx][m] == MFX_FRAMETYPE_B)
                        {
                            static const float kf[3] = {1, 0.84, 0.84 };
                            float kff = 1;//kf[k];
                            //if( m_bufferedInFrames[m+1]->m_frameNum < 120 ) kff=0.94;
                            if(m_bufferedInFrames[nextP]->m_frameType & MFX_FRAMETYPE_IDR)
                            {
                                cost = MAX_COST>>1;
                            }else{
                                cost += 0.84*RunPreMeCost(m_bufferedInFrames[prevP],
                                    m_bufferedInFrames[m+1],
                                    m_bufferedInFrames[nextP]);
                            }
                            //fprintf(stderr,"corr=%f\n");
                            //if( m_bufferedInFrames[m+1]->numIntraMb > numMB/3 ) cost=MAX_COST;
                        }else if(bestSequence[idx][m] == MFX_FRAMETYPE_P)
                        {
                            mfxU32 costP = RunPreMeCost(m_bufferedInFrames[prevP],
                                m_bufferedInFrames[m+1],
                                m_bufferedInFrames[m+1]);
                            if( m+1-prevP  > 1) cost += costP*(1+(float)m_bufferedInFrames[m+1]->numIntraMb/numMB);
                            else cost += costP;
                            //find nextP
                            if( m != i) //if not last
                            {
                                prevP = m+1;
                                nextP = prevP;
                                while(bestSequence[idx][nextP] != (mfxU16)MFX_FRAMETYPE_P) nextP++;
                                nextP++;
                            }
                        }
                    }
#if DEBUG_ADAPT
                    fprintf(stderr,"path cost: %d\n", cost);
#endif
                    if( cost < bestCost[i] )
                    {
                        bestCost[i] = cost;
                        idx = (++idx)&1;
                    }
                }
                //copy best path
                idx = (++idx)&1;
                for(int l=0; l<=i; l++) bestForLen[i+1][l] = bestSequence[idx][l];
#if DEBUG_ADAPT
                    fprintf(stderr,"best path: %d %d ", i, bestCost[i]);
                    for(int m=0; m<i+1; m++)
                    {
                        switch(bestSequence[idx][m])
                        {
                        case MFX_FRAMETYPE_I:
                            fprintf(stderr,"I");
                            break;
                        case MFX_FRAMETYPE_B:
                            fprintf(stderr,"B");
                            break;
                        case MFX_FRAMETYPE_P:
                            fprintf(stderr,"P");
                            break;
                        }
                    }
                    fprintf(stderr,"\n\n");
#endif
            }
            //find first P
            for(int l=0; l<maxLen; l++)
            {
                m_bufferedInFrames[l+1]->m_frameType = bestForLen[maxLen][l];
                if(bestForLen[maxLen][l] == MFX_FRAMETYPE_P)
                {
                    m_bufferedInFrames[l+1]->m_frameType |= MFX_FRAMETYPE_REF;
                    shiftOffset = l+1; break;
                }
            }
        }else if(maxLen == 1)
        {
                        RunPreMeCost(m_bufferedInFrames[0],
                                m_bufferedInFrames[1],
                                m_bufferedInFrames[1]);

        }

        //move to ready queue
        //fprintf(stderr,"move to ready: ");
        for(int k=0; k<shiftOffset; k++)
        {
            //fprintf(stderr,"%d,",m_bufferedInFrames[k]->m_frameType);
            m_readyInFrames.push_back(m_bufferedInFrames[k]);
        }
        //fprintf(stderr,"\n");
        m_bufferedInFrames.erase(m_bufferedInFrames.begin(), m_bufferedInFrames.begin()+shiftOffset);
    }
#endif

    //check if we have surface for new task
    //fprintf(stderr,"s:%d b:%d r:%d\n", *surface, m_bufferedInFrames.size(), m_readyInFrames.size());
    if(m_readyInFrames.size() > 0)
    {
        FrameTypeAdapt* frame = m_readyInFrames.front();
        *ctrl =  frame->m_ctrl;
        *surface = frame->m_surface;
        m_core->DecreaseReference(&frame->m_surface->Data);
        m_readyInFrames.erase(m_readyInFrames.begin());
        *requiredFrameType =  frame->m_frameType;
        if((*requiredFrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_P )
            *requiredFrameType |= MFX_FRAMETYPE_REF;
        //fprintf(stderr,"submit: %d t:%d  r:%d\n", frame->m_frameNum, frame->m_frameType & MFX_FRAMETYPE_IPB, frame->m_frameType & MFX_FRAMETYPE_REF);
        delete frame;
        //goto assign task
    }else
    {
        if(*surface != 0)
        {
            //fprintf(stderr,"err more data\n");
            return MFX_ERR_MORE_DATA;
        }
        //fprintf(stderr,"no frames in pool\n");
        //otherwise goto assign task
    }
    return MFX_ERR_NONE;
}
#endif

#endif

#endif // MFX_ENABLE_H264_VIDEO_ENCODE_HW
