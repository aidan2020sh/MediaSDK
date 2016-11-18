//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016 Intel Corporation. All Rights Reserved.
//

#include "mfxvideo++int.h"
#include "mfx_plugin_module.h"
#include "mfx_vp9_encode_hw.h"
#include "mfx_vp9_encode_hw_par.h"
#include "mfx_vp9_encode_hw_ddi.h"
#include "mfx_vp9_encode_hw_utils.h"
#include "ippi.h"
#include "ipps.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)
namespace MfxHwVP9Encode
{

Plugin::Plugin(bool CreateByDispatcher)
    : m_bStartIVFSequence(false)
    , m_maxBsSize(0)
    , m_pmfxCore(NULL)
    , m_PluginParam()
    , m_createdByDispatcher(CreateByDispatcher)
    , m_adapter(this)
    , m_initialized(false)
    , m_frameArrivalOrder(0)
{
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_PARALLEL;
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.APIVersion.Major = MFX_VERSION_MAJOR;
    m_PluginParam.APIVersion.Minor = MFX_VERSION_MINOR;
    m_PluginParam.PluginUID = MFX_PLUGINID_VP9E_HW;
    m_PluginParam.Type = MFX_PLUGINTYPE_VIDEO_ENCODE;
    m_PluginParam.CodecId = MFX_CODEC_VP9;
    m_PluginParam.PluginVersion = 1;
}

mfxStatus Plugin::PluginInit(mfxCoreInterface * pCore)
{
    if (!pCore)
        return MFX_ERR_NULL_PTR;

    m_pmfxCore = pCore;
    m_initialized = false;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::PluginClose()
{
    m_initialized = false;

    if (m_createdByDispatcher) {
        delete this;
    }

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetPluginParam(mfxPluginParam *par)
{
    if (!par)
        return MFX_ERR_NULL_PTR;
    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

inline GUID GetGuid(VP9MfxVideoParam const &par)
{
    return (par.mfx.LowPower != MFX_CODINGOPTION_OFF) ?
    DXVA2_Intel_LowpowerEncode_VP9_Profile0 : DXVA2_Intel_Encode_VP9_Profile0;
}

mfxStatus Plugin::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    VP9_LOG("\n (VP9_LOG) Plugin::Query +");
    MFX_CHECK_NULL_PTR1(out);

    if (in == 0)
    {
        return SetSupportedParameters(*out);
    }
    else
    {
        // check that [in] and [out] have same number of ext buffers
        if (in->NumExtParam != out->NumExtParam || !in->ExtParam != !out->ExtParam)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        // check attached buffers (all are supported by encoder, no duplications, etc.)
        MFX_CHECK_STS(CheckExtBufferHeaders(*in));
        MFX_CHECK_STS(CheckExtBufferHeaders(*out));

        VP9MfxVideoParam toValidate = *in;

        // get HW caps from driver
        ENCODE_CAPS_VP9 caps;
        Zero(caps);
        MFX_CHECK(MFX_ERR_NONE == QueryHwCaps(m_pmfxCore, caps, GetGuid(toValidate)), MFX_ERR_UNSUPPORTED);

        // validate input parameters
        mfxStatus sts = MFX_ERR_NONE;
        sts = CheckParameters(toValidate, caps);

        // copy validated parameters to [out]
        out->mfx = toValidate.mfx;
        out->AsyncDepth = toValidate.AsyncDepth;
        out->IOPattern = toValidate.IOPattern;
        out->Protected = toValidate.Protected;

        // copy validated ext buffers
        if (in->ExtParam && out->ExtParam)
        {
            for (mfxU8 i = 0; i < in->NumExtParam; i++)
            {
                mfxExtBuffer *pInBuf = in->ExtParam[i];
                if (pInBuf)
                {
                    mfxExtBuffer *pOutBuf = GetExtendedBuffer(out->ExtParam, out->NumExtParam, pInBuf->BufferId);
                    if (pOutBuf && (pOutBuf->BufferSz == pInBuf->BufferSz))
                    {
                        mfxExtBuffer *pCorrectedBuf = GetExtendedBuffer(toValidate.ExtParam, toValidate.NumExtParam, pInBuf->BufferId);
                        MFX_CHECK_NULL_PTR1(pCorrectedBuf);
                        memcpy_s(pOutBuf, pOutBuf->BufferSz, pCorrectedBuf, pCorrectedBuf->BufferSz);
                    }
                    else
                    {
                        // the buffer is present in [in] and absent in [out]
                        // or buffer sizes are different in [in] and [out]
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    }
                }
            }
        }

        VP9_LOG("\n (VP9_LOG) Plugin::Query -");
        return sts;
    }
}

mfxStatus Plugin::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    out;
    MFX_CHECK_NULL_PTR2(par,in);
    MFX_CHECK_STS(CheckExtBufferHeaders(*par));

    mfxU32 inPattern = par->IOPattern & MFX_IOPATTERN_IN_MASK;
    MFX_CHECK(inPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
        inPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY, MFX_ERR_INVALID_VIDEO_PARAM);

    VP9MfxVideoParam toValidate = *par;

    // get HW caps from driver
    ENCODE_CAPS_VP9 caps;
    Zero(caps);
    MFX_CHECK(MFX_ERR_NONE == QueryHwCaps(m_pmfxCore, caps, GetGuid(toValidate)), MFX_ERR_UNSUPPORTED);

    // get validated and properly initialized set of parameters
    CheckParameters(toValidate, caps);
    SetDefaults(toValidate, caps);

    // fill mfxFrameAllocRequest
    switch (par->IOPattern)
    {
    case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
        in->Type = MFX_MEMTYPE_SYS_EXT;
        break;
    case MFX_IOPATTERN_IN_VIDEO_MEMORY:
        in->Type = MFX_MEMTYPE_D3D_EXT;
        break;
    default:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    in->NumFrameMin = (mfxU16)CalcNumSurfRaw(toValidate);
    in->NumFrameSuggested = in->NumFrameMin;

    in->Info = par->mfx.FrameInfo;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::Init(mfxVideoParam *par)
{
    VP9_LOG("\n (VP9_LOG) Plugin::Init +");

    if (m_initialized == true)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_STS(CheckExtBufferHeaders(*par));

    mfxStatus sts  = MFX_ERR_NONE;
    mfxStatus checkSts = MFX_ERR_NONE; // to save warnings ater parameters checking

    m_video = *par;

    m_ddi.reset(CreatePlatformVp9Encoder(m_pmfxCore));
    MFX_CHECK(m_ddi.get() != 0, MFX_ERR_UNSUPPORTED);

    sts = m_ddi->CreateAuxilliaryDevice(m_pmfxCore, GetGuid(m_video),
        m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    ENCODE_CAPS_VP9 caps = {};
    sts = m_ddi->QueryEncodeCaps(caps);
    if (sts != MFX_ERR_NONE)
        return MFX_ERR_UNSUPPORTED;

    // get validated and properly initialized set of parameters for encoding
    checkSts = CheckParametersAndSetDefaults(m_video, caps);
    MFX_CHECK(checkSts >= 0, checkSts);

    sts = m_ddi->CreateAccelerationService(m_video);
    MFX_CHECK_STS(sts);

    m_rawFrames.Init(CalcNumSurfRaw(m_video));

    mfxFrameAllocRequest request = {};
    request.Info = m_video.mfx.FrameInfo;
    request.Type = MFX_MEMTYPE_D3D_INT;

    // allocate internal surfaces for input raw frames in case of SYSTEM input memory
    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        request.NumFrameMin = request.NumFrameSuggested = (mfxU16)CalcNumSurfRaw(m_video);
        sts = m_rawLocalFrames.Init(m_pmfxCore, &request);
        MFX_CHECK_STS(sts);
    }

    // allocate and register surfaces for reconstructed frames
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)CalcNumSurfRecon(m_video);
    request.Info.FourCC = MFX_FOURCC_NV12;

    sts = m_reconFrames.Init(m_pmfxCore, &request);
    MFX_CHECK_STS(sts);
    sts = m_ddi->Register(m_reconFrames.GetFrameAllocReponse(), D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    // allocate and register surfaces for output bitstreams
    sts = m_ddi->QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, request, m_video.mfx.FrameInfo.Width, m_video.mfx.FrameInfo.Height);
    MFX_CHECK_STS(sts);
    request.NumFrameMin = request.NumFrameSuggested = (mfxU16)CalcNumTasks(m_video);

    sts = m_outBitstreams.Init(m_pmfxCore, &request);
    MFX_CHECK_STS(sts);
    sts = m_ddi->Register(m_outBitstreams.GetFrameAllocReponse(), D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    m_maxBsSize = request.Info.Width * request.Info.Height;

    // prepare enough space for tasks
    m_tasks.resize(CalcNumTasks(m_video));
    Zero(m_tasks);

    // prepare enough space for DPB management
    m_dpb.resize(request.NumFrameMin - (par->AsyncDepth - 1));

    m_bStartIVFSequence = true;

    m_initialized = true;

    m_frameArrivalOrder = 0;

    VP9_LOG("\n (VP9_LOG) Plugin::Init -");
    return checkSts;
}

mfxStatus Plugin::Reset(mfxVideoParam *par)
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }
    par;
#if 0 // commented out from initial commit
       mfxStatus sts  = MFX_ERR_NONE;
        mfxStatus sts1 = MFX_ERR_NONE;

        //printf("HybridPakDDIImpl::Reset\n");

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK(par->IOPattern == m_video.IOPattern, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        VP9MfxVideoParam parBeforeReset = m_video;
        VP9MfxVideoParam parAfterReset = *par;

        {
            mfxExtCodingOptionVP9*       pExtOpt = GetExtBuffer(parAfterReset);
            mfxExtOpaqueSurfaceAlloc*    pExtOpaque = GetExtBuffer(parAfterReset);

            sts1 = CheckParametersAndSetDefault(par,&parAfterReset, pExtOpt, pExtOpaque,true,true);
            MFX_CHECK(sts1>=0, sts1);
        }

        MFX_CHECK(parBeforeReset.AsyncDepth == parAfterReset.AsyncDepth
            && parBeforeReset.mfx.RateControlMethod == parAfterReset.mfx.RateControlMethod,
            MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        m_video = parAfterReset;

        sts = m_pTaskManager->Reset(&m_video);
        MFX_CHECK_STS(sts);

        sts = m_ddi->Reset(m_video);
        MFX_CHECK_STS(sts);

        if (IsResetOfPipelineRequired(parBeforeReset, parAfterReset))
        {
            sts = m_BSE->Reset(m_video);
        }
        MFX_CHECK_STS(sts);

        m_bStartIVFSequence = false;

        return sts1;
#endif // commented out from initial commit

        return MFX_ERR_UNSUPPORTED;
}

mfxStatus Plugin::EncodeFrameSubmit(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxThreadTask *task)
{
    VP9_LOG("\n (VP9_LOG) Frame ?? Plugin::EncodeFrameSubmit +");

    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    Task* pTask = 0;

    mfxStatus checkSts = CheckEncodeFrameParam(
        m_video,
        ctrl,
        surface,
        bs,
        true);

    MFX_CHECK(checkSts >= MFX_ERR_NONE, checkSts);

    {
        // take Task from the pool and initialize with arrived frame
        UMC::AutomaticUMCMutex guard(m_taskMutex);
        pTask = GetFreeTask(m_tasks);
        if (pTask == 0)
        {
            return MFX_WRN_DEVICE_BUSY;
        }

        MFX_CHECK_STS(m_rawFrames.GetFrame(surface, pTask->m_pRawFrame));
        MFX_CHECK_STS(LockSurface(pTask->m_pRawFrame, m_pmfxCore));

        if (ctrl)
        {
            pTask->m_ctrl = *ctrl;
        }

        pTask->m_timeStamp = surface->Data.TimeStamp;
        pTask->m_status = TASK_INITIALIZED;
        pTask->m_frameOrder = m_frameArrivalOrder;

        m_frameArrivalOrder++;

        // place mfxBitstream to the queue
        m_outs.push(bs);
    }

    *task = (mfxThreadTask*)pTask;

    VP9_LOG("\n (VP9_LOG) Frame %d Plugin::EncodeFrameSubmit -", pTask->m_frameOrder);

    return checkSts;
}

mfxStatus Plugin::ConfigTask(Task &task)
{
    MFX_CHECK(task.m_status == TASK_INITIALIZED, MFX_ERR_UNDEFINED_BEHAVIOR);

    VP9FrameLevelParam frameParam = { };
    mfxStatus sts = SetFramesParams(m_video, task.m_ctrl.FrameType, task.m_frameOrder, frameParam);

    task.m_pRecFrame = 0;
    task.m_pOutBs = 0;
    task.m_pRawLocalFrame = 0;

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        task.m_pRawLocalFrame = m_rawLocalFrames.GetFreeFrame();
        MFX_CHECK(task.m_pRawLocalFrame != 0, MFX_WRN_DEVICE_BUSY);
    }
    task.m_pRecFrame = m_reconFrames.GetFreeFrame();
    MFX_CHECK(task.m_pRecFrame != 0, MFX_WRN_DEVICE_BUSY);

    task.m_pOutBs = m_outBitstreams.GetFreeFrame();
    MFX_CHECK(task.m_pOutBs != 0, MFX_WRN_DEVICE_BUSY);

    sts = DecideOnRefListAndDPBRefresh(&m_video, &task, m_dpb, frameParam);

    task.m_frameParam = frameParam;

    task.m_pRecFrame->pSurface->Data.FrameOrder = task.m_frameOrder;

    if (task.m_frameParam.frameType == KEY_FRAME)
    {
        task.m_pRecRefFrames[REF_LAST] = task.m_pRecRefFrames[REF_GOLD] = task.m_pRecRefFrames[REF_ALT] = 0;
    }
    else
    {
        mfxU8 idxLast = task.m_frameParam.refList[REF_LAST];
        mfxU8 idxGold = task.m_frameParam.refList[REF_GOLD];
        mfxU8 idxAlt = task.m_frameParam.refList[REF_ALT];
        task.m_pRecRefFrames[REF_LAST] = m_dpb[idxLast];
        task.m_pRecRefFrames[REF_GOLD] = m_dpb[idxGold] != m_dpb[idxLast] ? m_dpb[idxGold] : 0;
        task.m_pRecRefFrames[REF_ALT] = m_dpb[idxAlt] != m_dpb[idxLast] && m_dpb[idxAlt] != m_dpb[idxGold] ? m_dpb[idxAlt] : 0;
    }

    MFX_CHECK_STS(LockSurface(task.m_pRecFrame, m_pmfxCore));
    MFX_CHECK_STS(LockSurface(task.m_pRawLocalFrame, m_pmfxCore));
    MFX_CHECK_STS(LockSurface(task.m_pOutBs, m_pmfxCore));

    task.m_status = TASK_SUBMITTED;

    UpdateDpb(frameParam, task.m_pRecFrame, m_dpb, m_pmfxCore);

    return sts;
}

mfxStatus Plugin::Execute(mfxThreadTask task, mfxU32 , mfxU32 )
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    Task* pTask = (Task*)task;
    MFX_CHECK(pTask->m_status == TASK_INITIALIZED || pTask->m_status == TASK_SUBMITTED, MFX_ERR_UNDEFINED_BEHAVIOR);

    if (pTask->m_status == TASK_INITIALIZED)
    {
        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::SubmitFrame +", pTask->m_frameOrder);
        mfxStatus sts = MFX_ERR_NONE;

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            MFX_CHECK_STS(ConfigTask(*pTask));
        }

        mfxFrameSurface1    *pSurface = 0;
        bool                bExternalSurface = true;

        mfxHDLPair surfaceHDL = {};

        // copy input frame from SYSTEM to VIDEO memory (if required)
        sts = CopyRawSurfaceToVideoMemory(m_pmfxCore, m_video, *pTask);
        MFX_CHECK_STS(sts);

        sts = pTask->GetInputSurface(pSurface, bExternalSurface);
        MFX_CHECK_STS(sts);

        // get handle to input frame in VIDEO memory (either external or local)
        sts = m_pmfxCore->FrameAllocator.GetHDL(m_pmfxCore->FrameAllocator.pthis, pSurface->Data.MemId, &surfaceHDL.first);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceHDL.first != 0, MFX_ERR_UNDEFINED_BEHAVIOR);

        // submit the frame to the driver
        sts = m_ddi->Execute(*pTask, surfaceHDL.first);
        MFX_CHECK_STS(sts);

        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::SubmitFrame -", pTask->m_frameOrder);
        return MFX_TASK_WORKING;
    }
    else
    {
        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::QueryFrame +", pTask->m_frameOrder);

        mfxStatus sts = MFX_ERR_NONE;

        sts = m_ddi->QueryStatus(*pTask);
        if (sts == MFX_WRN_DEVICE_BUSY)
        {
            return MFX_TASK_WORKING;
        }
        MFX_CHECK_STS(sts);

        pTask->m_pBitsteam = m_outs.front();
        sts = UpdateBitstream(*pTask);
        MFX_CHECK_STS(sts);

        {
            UMC::AutomaticUMCMutex guard(m_taskMutex);
            m_outs.pop();
            FreeTask(m_pmfxCore, *pTask);
        }

        VP9_LOG("\n (VP9_LOG) Frame %d Plugin::QueryFrame -", pTask->m_frameOrder);
        return MFX_TASK_DONE;
    }
}

mfxStatus Plugin::FreeResources(mfxThreadTask task, mfxStatus )
{
    task; return MFX_ERR_NONE;
}

mfxStatus Plugin::Close()
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_STS(m_rawLocalFrames.Release());
    MFX_CHECK_STS(m_reconFrames.Release());
    MFX_CHECK_STS(m_outBitstreams.Release());

    m_initialized = false;

    return MFX_ERR_NONE;
}

mfxStatus Plugin::GetVideoParam(mfxVideoParam *par)
{
    if (m_initialized == false)
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    MFX_CHECK_NULL_PTR1(par);
    MFX_CHECK_STS(CheckExtBufferHeaders(*par));

    par->AsyncDepth = m_video.AsyncDepth;
    par->IOPattern = m_video.IOPattern;
    par->Protected = m_video.Protected;

    par->mfx = m_video.mfx;

    for (mfxU8 i = 0; i < par->NumExtParam; i++)
    {
        mfxExtBuffer *pOutBuf = par->ExtParam[i];
        if (pOutBuf == 0)
        {
            return MFX_ERR_NULL_PTR;
        }
        mfxExtBuffer *pLocalBuf = GetExtendedBuffer(m_video.ExtParam, m_video.NumExtParam, pOutBuf->BufferId);
        if (pLocalBuf && (pOutBuf->BufferSz == pLocalBuf->BufferSz))
        {
            memcpy_s(pOutBuf, pOutBuf->BufferSz, pLocalBuf, pLocalBuf->BufferSz);
        }
        else
        {
            assert(!"Encoder doesn't have requested buffer or bufefr sizes are different!");
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    return MFX_ERR_NONE;
}

inline void UpdatePictureHeader(unsigned int frameLen, unsigned int frameNum, unsigned char *pPictureHeader)
{
    mfxU32 ivf_frame_header[3] = {frameLen, frameNum << 1, 0x00000000};
    memcpy(pPictureHeader, ivf_frame_header, sizeof (ivf_frame_header));
};

mfxStatus Plugin::UpdateBitstream(
    Task & task)
{
    VP9_LOG("\n (VP9_LOG) Plugin::UpdateBitstream +");

    mfxFrameData bitstream = {};

    FrameLocker lock(m_pmfxCore, bitstream, task.m_pOutBs->pSurface->Data.MemId);
    if (bitstream.Y == 0)
    {
        return MFX_ERR_LOCK_MEMORY;
    }

    mfxU32   bsSizeToCopy  = task.m_bsDataLength;
    mfxU32   bsSizeAvail   = task.m_pBitsteam->MaxLength - task.m_pBitsteam->DataOffset - task.m_pBitsteam->DataLength;
    mfxU8 *  bsData        = task.m_pBitsteam->Data + task.m_pBitsteam->DataOffset + task.m_pBitsteam->DataLength;

    assert(bsSizeToCopy <= bsSizeAvail);

    if (bsSizeToCopy > bsSizeAvail)
    {
        bsSizeToCopy = bsSizeAvail;
    }

    // Avoid segfaults on very high bitrates
    if (bsSizeToCopy > m_maxBsSize)
    {
        lock.Unlock();
        return MFX_ERR_DEVICE_FAILED;
    }

    // Copy compressed picture from d3d surface to buffer in system memory
    if (bsSizeToCopy)
    {
        IppiSize roi = {(Ipp32s)bsSizeToCopy,1};
        ippiCopyManaged_8u_C1R(bitstream.Y, bitstream.Pitch, bsData, bsSizeToCopy, roi, IPP_NONTEMPORAL_LOAD);
    }

    mfxU8 * pIVFPicHeader = InsertSeqHeader(task) ? bsData + IVF_SEQ_HEADER_SIZE_BYTES : bsData;
    UpdatePictureHeader(bsSizeToCopy - IVF_PIC_HEADER_SIZE_BYTES - (InsertSeqHeader(task) ? IVF_SEQ_HEADER_SIZE_BYTES : 0), (mfxU32)task.m_frameOrder, pIVFPicHeader);

    task.m_pBitsteam->DataLength += bsSizeToCopy;

    // Update bitstream fields
    task.m_pBitsteam->TimeStamp = task.m_timeStamp;
    task.m_pBitsteam->FrameType = mfxU16(task.m_frameParam.frameType == KEY_FRAME ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P);
    task.m_pBitsteam->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    VP9_LOG("\n (VP9_LOG) Plugin::UpdateBitstream -");

    return MFX_ERR_NONE;
}


} // MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
