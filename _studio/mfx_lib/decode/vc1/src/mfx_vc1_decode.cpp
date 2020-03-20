// Copyright (c) 2004-2020 Intel Corporation
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

#if defined (MFX_ENABLE_VC1_VIDEO_DECODE)
#include "mfx_vc1_decode.h"
#include "mfxvideo++int.h"
#include "mfx_vc1_dec_common.h"
#include "umc_vc1_dec_task_store.h"
#include "vm_sys_info.h"
#include "umc_vc1_dec_frame_descr_va.h"
// calculate framerate
#include "mfx_enc_common.h"
#include "mfx_thread_task.h"
#include "mfx_common_decode_int.h"

#if defined (MFX_VA)
#include "umc_va_dxva2.h"
#endif

using namespace MFXVC1DecCommon;
using namespace UMC;

class VC1TaskStore;

static void SetFrameType(const uint32_t type, mfxFrameSurface1 &surface)
{
    auto extFrameInfo = reinterpret_cast<mfxExtDecodedFrameInfo *>(GetExtendedBuffer(surface.Data.ExtParam, surface.Data.NumExtParam, MFX_EXTBUFF_DECODED_FRAME_INFO));
    if (extFrameInfo == nullptr)
        return;

    switch (type & VC1_BI_FRAME)
    {
    case VC1_I_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_I;
        break;
    case VC1_P_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_P;
        break;
    case VC1_B_FRAME:
    case VC1_BI_FRAME:
        extFrameInfo->FrameType = MFX_FRAMETYPE_B;
        break;
    default:// unexpected type
        extFrameInfo->FrameType = MFX_FRAMETYPE_UNKNOWN;
        assert(0);
    }
}

#ifdef MFX_VA
#define VC1_SKIPPED_DISABLE
#endif

void MFXVideoDECODEVC1::SetFrameOrder(mfx_UMC_FrameAllocator* pFrameAlloc, mfxVideoParam* par, bool isLast, VC1TSDescriptor tsd, bool isSamePolar)
{
    mfxFrameSurface1 surface = { };
    mfxFrameSurface1 *pSurface;

    uint32_t frameOrder = 0;
    UMC::FrameMemID idx = m_pVC1VideoDecoder->GetFrameOrder(isLast, isSamePolar, frameOrder);

    pSurface = pFrameAlloc->GetSurface(idx, &surface, par);
    if (frameOrder == 0xFFFFFFFF)
    {
        pSurface->Data.FrameOrder = 0xFFFFFFFF;
        return;
    }

    pSurface->Data.FrameOrder = frameOrder;
    pSurface->Data.TimeStamp = tsd.pts;
    pSurface->Data.DataFlag = (mfxU16)(tsd.isOriginal ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
}

void MFXVideoDECODEVC1::FillVideoSignalInfo(mfxExtVideoSignalInfo *pVideoSignal)
{
    pVideoSignal->ColourDescriptionPresent = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.ColourDescriptionPresent;
    if (pVideoSignal->ColourDescriptionPresent)
    {
        pVideoSignal->ColourPrimaries = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.ColourPrimaries;
        pVideoSignal->MatrixCoefficients = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.MatrixCoefficients;
        pVideoSignal->TransferCharacteristics = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.TransferCharacteristics;
    }
    else
    {
        pVideoSignal->ColourPrimaries = 1;
        pVideoSignal->MatrixCoefficients = 6;
        pVideoSignal->TransferCharacteristics = 1;
    }

}

MFXVideoDECODEVC1::MFXVideoDECODEVC1(VideoCORE *core, mfxStatus* mfxSts):VideoDECODE(),
m_VideoParams(NULL),
m_pInternMediaDataIn(NULL),
m_pVC1VideoDecoder(nullptr),
m_frame_constructor(NULL),
m_RBufID((UMC::MemID)-1),
m_BufSize(1024*1024),
m_pStCodes(0),
m_stCodesID((UMC::MemID)-1),
m_par(),
m_Initpar(),
m_FrameSize(0),
m_bIsInit(false),
m_FCInfo(),
m_pCore(core),
m_bIsNeedToProcFrame(true),
m_bIsDecInit(false),
m_bIsSWD3D(false),
m_bIsSamePolar(true),
m_bIsDecodeOrder(false),
m_SHSize(0),
m_SaveBytesSize(0),
m_sbs(),
m_CurrentBufFrame(0),
m_bIsBuffering(false),
m_isSWPlatform(false),
m_CurrentTask(0),
m_WaitedTask(0),
m_BufOffset(0),
m_ProcessedFrames(0),
m_SubmitFrame(0),
m_bIsFirstField(true),
m_IsOpaq(false),
m_pPrevOutSurface(NULL),
m_ext_dur(0),
m_bStsReport(true),
m_NumberOfQueries(0),
m_bPTSTaken(false)
{
    memset(&m_response, 0, sizeof(mfxFrameAllocResponse));
    memset(&m_response_op, 0, sizeof(m_response_op));

    m_MemoryAllocator.InitMem(NULL,core);
    m_VideoParams = new UMC::VideoDecoderParams;
    m_pInternMediaDataIn = new UMC::MediaData;
#ifdef MFX_VA_WIN
    memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1)*MAX_NUM_STATUS_REPORTS);
#endif
    *mfxSts = MFX_ERR_NONE;

    m_bTakeBufferedFrame = 0;
    m_pReadBuffer = NULL;
}

MFXVideoDECODEVC1::~MFXVideoDECODEVC1(void)
{
    Close();
    if (m_VideoParams)
    {
        delete m_VideoParams;
        m_VideoParams = NULL;
    }
    if (m_pInternMediaDataIn)
    {
        delete m_pInternMediaDataIn;
        m_pInternMediaDataIn = NULL;
    }
}
mfxStatus MFXVideoDECODEVC1::Init(mfxVideoParam *par)
{
    // already init
    MFX_CHECK(!m_bIsDecInit, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxStatus       MFXSts = MFX_ERR_NONE;
    UMC::Status     IntUMCStatus = UMC::UMC_OK;
    UMC::Status     umcSts;
    bool            isPartMode = false;
    bool            MapOpaq = true;
    bool            Polar   = false;

    m_isSWPlatform = true;
    m_CurrentTask = 0;
    m_WaitedTask = 0;

    MFXSts  = CheckVideoParamDecoders(par, m_pCore->IsExternalFrameAllocator(), m_pCore->GetHWType());
    MFX_CHECK_STS(MFXSts);
    MFXSts = CheckInput(par);
    MFX_CHECK_STS(MFXSts);

    MFX_CHECK(MFX_PLATFORM_SOFTWARE != m_pCore->GetPlatformType() || !par->Protected, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(IsHWSupported(m_pCore, par) || par->Protected <= 0, MFX_ERR_INVALID_VIDEO_PARAM);

    mfxU16  IOPattern = par->IOPattern;

    m_bIsDecodeOrder = (1 == par->mfx.DecodedOrder)? true : false;

    // Setting buffering options
    m_CurrentBufFrame = 0;
    m_bIsBuffering = (disp_queue_size > 0)?IsBufferMode(m_pCore, par):false;
    m_par = *par;
    m_Initpar = *par;

    m_par.mfx.NumThread = 0;

    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_par);
    m_par.mfx.NumThread = (mfxU16)(m_par.AsyncDepth ? m_par.AsyncDepth : m_pCore->GetAutoAsyncDepth());

    if (MFX_PLATFORM_SOFTWARE == m_pCore->GetPlatformType())
    {
#ifdef ALLOW_SW_VC1_FALLBACK
        m_pFrameAlloc.reset(new mfx_UMC_FrameAllocator_NV12);
        m_pVC1VideoDecoder.reset(new UMC::VC1VideoDecoderSW);
#endif
    }
    else
    {
        if (IsHWSupported(m_pCore, par))
        {
#if defined (MFX_VA)
            m_pFrameAlloc.reset(new mfx_UMC_FrameAllocator_D3D);
            m_pVC1VideoDecoder.reset(new UMC::VC1VideoDecoderHW);
#endif
            m_isSWPlatform = false;
        }
        else
        {
#ifdef ALLOW_SW_VC1_FALLBACK
            m_pFrameAlloc.reset(new mfx_UMC_FrameAllocator_NV12);
            m_pVC1VideoDecoder.reset(new UMC::VC1VideoDecoderSW);
            isPartMode = true;
#else
            MFX_RETURN(MFX_ERR_UNSUPPORTED);
#endif
        }
    }

    // frames allocation

    // SW platform and system external
    if (m_isSWPlatform && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
    {
        // no need to alloc surfaces - direct working with external
        memset(&m_response, 0, sizeof(mfxFrameAllocResponse));
        umcSts = m_pFrameAlloc->InitMfx(0, m_pCore, &m_par,  0, &m_response, true, m_isSWPlatform);
        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);
        m_bIsSWD3D = false;
    }
    else
    {
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        SetAllocRequestInternal(m_pCore, &m_par, &request);

        m_response.NumFrameActual = 32;

         // to process Opaque surfaces
        mfxExtOpaqueSurfaceAlloc *pOpqAlloc = 0;
        MFXSts = UpdateAllocRequest(&m_par, &request, &pOpqAlloc, MapOpaq, Polar);
        MFX_CHECK_STS(MFXSts);

        if (MapOpaq && !Polar)
        {
            MFXSts = m_pCore->AllocFrames(&request,
                                          &m_response,
                                          pOpqAlloc->Out.Surfaces,
                                          pOpqAlloc->Out.NumSurface);
        }
        else
        {
            bool isNeedCopy = ((MFX_IOPATTERN_OUT_SYSTEM_MEMORY & IOPattern) && (request.Type & MFX_MEMTYPE_INTERNAL_FRAME)) || ((MFX_IOPATTERN_OUT_VIDEO_MEMORY & IOPattern) && (m_isSWPlatform));

#ifdef MFX_VA_WIN
            request.Type|=MFX_MEMTYPE_SHARED_RESOURCE;
#endif
            MFXSts = m_pCore->AllocFrames(&request, &m_response,isNeedCopy);
            MFX_CHECK_STS(MFXSts);
        }

        MFX_CHECK(MFXSts == MFX_ERR_NONE || MFXSts == MFX_ERR_NOT_FOUND, MFXSts);

#if defined (MFX_VA)
        if (!m_isSWPlatform)
        {
            MFXSts = m_pCore->CreateVA(&m_par, &request, &m_response, m_pFrameAlloc.get());
            MFX_CHECK(MFXSts >= MFX_ERR_NONE, MFXSts);
        }
#endif

        // External allocator already was set
        MFXSts = MFX_ERR_NONE; // there are no errors

        if ((IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY && !m_isSWPlatform)||
            (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY && m_isSWPlatform)||
            (MapOpaq && Polar))  // use internal surfaces
        {
            umcSts = m_pFrameAlloc->InitMfx(0, m_pCore, &m_par, &request, &m_response, false, m_isSWPlatform);
        }
        else
        {
            m_pFrameAlloc->SetExternalFramesResponse(&m_response);
            umcSts = m_pFrameAlloc->InitMfx(0, m_pCore, &m_par, &request, &m_response, true, m_isSWPlatform);
        }

        MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

        m_bIsSWD3D = true;
    }

    MFX_CHECK(!m_IsOpaq || m_pCore->IsCompatibleForOpaq(), MFX_ERR_UNDEFINED_BEHAVIOR);

    m_pVC1VideoDecoder->SetExtFrameAllocator(m_pFrameAlloc.get());

    ConvertMfxToCodecParams(&m_par);
    m_VideoParams->lpMemoryAllocator = &m_MemoryAllocator;
    if (par->mfx.FrameInfo.Width && par->mfx.FrameInfo.Height)
    {
        m_BufSize = par->mfx.FrameInfo.Width*par->mfx.FrameInfo.Height*2;
        // Set surface number for decoding. HW requires
        m_VideoParams->m_SuggestedOutputSize = m_response.NumFrameActual;
        IntUMCStatus = m_pVC1VideoDecoder->Init(m_VideoParams);
        MFX_CHECK_UMC_STS(IntUMCStatus);
        m_InternMediaDataOut.Init(par->mfx.FrameInfo.Width,par->mfx.FrameInfo.Height,UMC::YV12);
    }

    umcSts = m_MemoryAllocator.Alloc(&m_RBufID, m_BufSize, UMC::UMC_ALLOC_PERSISTENT, 16);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_pReadBuffer = (uint8_t*)(m_MemoryAllocator.Lock(m_RBufID));

    m_FrameConstrData.SetBufferPointer(m_pReadBuffer, m_BufSize);
    m_FrameConstrData.SetDataSize(0);

    umcSts = m_MemoryAllocator.Alloc(&m_stCodesID, START_CODE_NUMBER*2*sizeof(int32_t)+sizeof(UMC::MediaDataEx::_MediaDataEx), UMC::UMC_ALLOC_PERSISTENT, 16);
    MFX_CHECK(umcSts == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_pStCodes = (UMC::MediaDataEx::_MediaDataEx*)(m_MemoryAllocator.Lock(m_stCodesID));
    memset(reinterpret_cast<void*>(m_pStCodes), 0, (START_CODE_NUMBER*2*sizeof(int32_t)+sizeof(UMC::MediaDataEx::_MediaDataEx)));
    m_pStCodes->count      = 0;
    m_pStCodes->index      = 0;
    m_pStCodes->bstrm_pos  = 0;
    m_pStCodes->offsets    = (uint32_t*)((uint8_t*)m_pStCodes + sizeof(UMC::MediaDataEx::_MediaDataEx));
    m_pStCodes->values     = (uint32_t*)((uint8_t*)m_pStCodes->offsets + START_CODE_NUMBER*sizeof( uint32_t));

    // Should add type of statuses to MFX
    MFX_CHECK(IntUMCStatus == UMC::UMC_OK, MFX_ERR_MEMORY_ALLOC);

    m_FCInfo.in = m_pInternMediaDataIn;
    m_FCInfo.out = &m_FrameConstrData;
    m_FCInfo.stCodes = m_pStCodes;
    m_FCInfo.splMode = 2;

    m_bIsNeedToProcFrame = true;

    memset(&m_sbs, 0, sizeof(mfxBitstream));
    m_sbs.Data = m_pSaveBytes;

    // Check. May be we in part mode
    if (MFX_ERR_NONE == MFXSts && isPartMode)
        MFXSts = MFX_WRN_PARTIAL_ACCELERATION;

    if ((m_isSWPlatform && (IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) ||
        (!m_isSWPlatform && (IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)))
    {
        m_bIsSamePolar = true;
    }
    else
    {
        m_bIsSamePolar = false;
    }

    m_bIsDecInit = true;

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::Reset(mfxVideoParam *par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    Status          umcSts = UMC_OK;

    // Common parameters
    MFXSts  = CheckVideoParamDecoders(par, m_pCore->IsExternalFrameAllocator(), m_pCore->GetHWType()/*MFX_HW_UNKNOWN*/);
    MFX_CHECK_STS(MFXSts);

    MFXSts = CheckInput(par);
    MFX_CHECK_STS(MFXSts);

    MFXSts = CheckForCriticalChanges(par);
    MFX_CHECK_STS(MFXSts);

    m_bIsDecodeOrder = (1 == par->mfx.DecodedOrder)? true : false;
    // buffers setting
    m_FrameConstrData.SetBufferPointer(m_pReadBuffer, m_BufSize);
    m_FrameConstrData.SetDataSize(0);
    memset(m_pStCodes, 0, (START_CODE_NUMBER*2*sizeof(int32_t)+sizeof(UMC::MediaDataEx::_MediaDataEx)));
    m_pStCodes->count      = 0;
    m_pStCodes->index      = 0;
    m_pStCodes->bstrm_pos  = 0;
    m_pStCodes->offsets    = (uint32_t*)((uint8_t*)m_pStCodes + sizeof(UMC::MediaDataEx::_MediaDataEx));
    m_pStCodes->values     = (uint32_t*)((uint8_t*)m_pStCodes->offsets + START_CODE_NUMBER*sizeof( uint32_t));
    // UMC decoder reset

    m_par = *par;
    bool isNeedChangeVideoParamWarning = IsNeedChangeVideoParam(&m_par);

    if (m_isSWPlatform)
        m_par.mfx.NumThread = (mfxU16)(m_par.AsyncDepth ? m_par.AsyncDepth : (mfxU16)vm_sys_info_get_cpu_num());
    else
        m_par.mfx.NumThread = m_pCore->GetAutoAsyncDepth();

    ConvertMfxToCodecParams(par);
    m_VideoParams->lpMemoryAllocator = &m_MemoryAllocator;

    umcSts = m_pVC1VideoDecoder->Reset();
    MFX_CHECK_UMC_STS(umcSts);
    // Clear output display index queue
    m_qMemID.clear();
    m_qSyncMemID.clear();
    m_qTS.clear();
    m_qBSTS.clear();
    for (mfxU32 i = 0; i < m_DisplayList.size();i++)
        m_pCore->DecreaseReference(&m_DisplayList[i]->Data);
    m_DisplayList.clear();
    m_DisplayListAsync.clear();
    // other vars
    m_bIsNeedToProcFrame = true;
    m_bIsDecInit = true;

    m_CurrentTask = 0;
    m_WaitedTask = 0;
    m_SubmitFrame = 0;
    m_ProcessedFrames = 0;

    memset(&m_sbs, 0, sizeof(mfxBitstream));
    m_sbs.Data = m_pSaveBytes;

    umcSts = m_pFrameAlloc->Reset();
    MFX_CHECK_UMC_STS(umcSts);
    m_bIsInit = false;
    // setting buffering options
    m_CurrentBufFrame = 0;
    m_bIsBuffering = (disp_queue_size > 0)?IsBufferMode(m_pCore, par):false;
    m_BufOffset = 0;
    m_bIsFirstField = true;
    m_pPrevOutSurface = NULL;
    m_ext_dur = 0;
    m_SHSize = 0;
    m_SaveBytesSize = 0;

    if (false == IsHWSupported(m_pCore, par))
    {
        MFXSts = MFX_WRN_PARTIAL_ACCELERATION;
    }

    MFX_CHECK(!isNeedChangeVideoParamWarning, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    m_NumberOfQueries = 0;
    m_bPTSTaken = false;

    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::Close(void)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    m_ext_dur = 0;

    if (m_pVC1VideoDecoder.get())
    {
        m_pVC1VideoDecoder->Close();
        // Free decoder
        m_pVC1VideoDecoder.reset(0);
    }
    if (m_frame_constructor)
    {
        delete m_frame_constructor;
        m_frame_constructor = 0;
    }
    if ((int)m_RBufID != -1)
    {
        m_MemoryAllocator.Unlock(m_RBufID);
        m_MemoryAllocator.Free(m_RBufID);
        m_RBufID = (UMC::MemID)-1;
    }
    if ((int)m_stCodesID != -1)
    {
        m_MemoryAllocator.Unlock(m_stCodesID);
        m_MemoryAllocator.Free(m_stCodesID);
        m_stCodesID = (UMC::MemID)-1;
    }
    // Free frame allocator
    if (m_pFrameAlloc.get())
    {
        m_pFrameAlloc->Close();
        m_pFrameAlloc.reset(0);
    }
    if (m_pCore)
    {
        if (m_response.mids)
            m_pCore->FreeFrames(&m_response);

        if (m_response_op.mids)
            m_pCore->FreeFrames(&m_response_op);
    }
    memset(&m_response, 0, sizeof(m_response));
    memset(&m_response_op, 0, sizeof(m_response_op));

    m_qMemID.clear();
    m_qSyncMemID.clear();
    m_qTS.clear();
    m_qBSTS.clear();

    if (!m_bIsDecInit)
        MFXSts = MFX_ERR_NOT_INITIALIZED;

    m_bIsDecInit = false;
    m_pPrevOutSurface = NULL;

    memset(&m_Initpar,0, sizeof(mfxVideoParam));
    return MFXSts;
}

mfxTaskThreadingPolicy MFXVideoDECODEVC1::GetThreadingPolicy(void)
{
    if (m_isSWPlatform)
    {
        return MFX_TASK_THREADING_SHARED;
    }
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    else if (MFX_HW_VAAPI == m_pCore->GetVAType())
    {
        return MFX_TASK_THREADING_INTRA;
    }
#endif
    else
    {
        return MFX_TASK_THREADING_DEFAULT;
    }
}

mfxStatus MFXVideoDECODEVC1::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxVideoParam   tpar;
    MFX_CHECK_NULL_PTR1(out);
    if (in)
    {
        tpar = *in;
        in = &tpar;

        if (!IsHWSupported(core, in))
            MFXSts = MFX_WRN_PARTIAL_ACCELERATION;
    }

    if (MFX_ERR_NONE == MFXSts)
        MFXSts = MFXVC1DecCommon::Query(core, in, out);
    else // MFX_WRN_PARTIAL_ACCELERATION;
    {
        MFXSts = MFXVC1DecCommon::Query(core, in, out);

        if (0 < out->Protected)
        {
            // unsupported since there is no protected decoding
            out->Protected = 0;
            MFXSts = MFX_ERR_UNSUPPORTED;
        }

        if (MFX_ERR_NONE == MFXSts)
            MFXSts = MFX_WRN_PARTIAL_ACCELERATION;
    }
    MFX_CHECK(MFXSts < MFX_ERR_NONE, MFXSts);
    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::GetVideoParam(mfxVideoParam *par)
{
    MFX_CHECK(m_bIsDecInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(par);

    mfxStatus       MFXSts = MFX_ERR_NONE;

    par->mfx = m_par.mfx;
    par->Protected = m_par.Protected;
    par->IOPattern = m_par.IOPattern;
    par->AsyncDepth = m_par.AsyncDepth;


    mfxExtVideoSignalInfo * videoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    if (videoSignal)
    {
        FillVideoSignalInfo(videoSignal);
    }
    mfxExtCodingOptionSPSPPS *pSPS = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);
    if (pSPS)
    {
        if (pSPS->SPSBufSize < m_RawSeq.size())
            return MFX_ERR_NOT_ENOUGH_BUFFER;

        std::copy(std::begin(m_RawSeq), std::end(m_RawSeq), pSPS->SPSBuffer);
        pSPS->SPSBufSize = (mfxU16)m_RawSeq.size();
    }
    MFX_CHECK(MFXSts < MFX_ERR_NONE, MFXSts);
    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoDECODEVC1::DecodeFrameCheck");
    mfxStatus MFXSts;

    MFX_CHECK(m_bIsDecInit, MFX_ERR_NOT_INITIALIZED);

    if (NeedToReturnCriticalStatus(bs))
        return ReturningCriticalStatus();

    if(m_par.mfx.CodecProfile == 0)
    {
        if(bs)
        {
            Close();
            MFXSts  = CheckBitstream(bs);
            MFX_CHECK_STS(MFXSts);

            if((*bs->Data)&&0xFF == 0xC5)
                m_par.mfx.CodecProfile = MFX_PROFILE_VC1_SIMPLE;
            else
            {
                //start code search
                uint8_t* ptr = bs->Data+ bs->DataOffset;
                uint32_t i = 0;
                while(((i < bs->DataLength) && (*(uint32_t*)ptr!= 0x0F010000)))
                {
                    ptr++;
                    i++;
                }

                if(*((uint32_t*)ptr)!= 0x0F010000)
                {
                    bs->DataOffset = bs->DataLength;
                    return MFX_ERR_MORE_DATA;
                }
                else
                {
                    m_par.mfx.CodecProfile = MFX_PROFILE_VC1_ADVANCED;
                }
            }

            Init(&m_par);
        }
        else
        {
            return MFX_ERR_MORE_DATA;
        }
    }

    MFX_CHECK_NULL_PTR2(surface_work, surface_disp);
    // for EOS support
    if (bs)
    {

        MFXSts  = CheckBitstream(bs);
        MFX_CHECK_STS(MFXSts);

#if defined(MFX_VA_WIN) && !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (m_par.Protected && bs)
    {
        MFX_CHECK(m_pVC1VideoDecoder->m_va->GetProtectedVA(), MFX_ERR_UNDEFINED_BEHAVIOR);

        MFXSts  = CheckEncryptedBitstream(bs);
        MFX_CHECK_STS(MFXSts);

        m_pVC1VideoDecoder->m_va->GetProtectedVA()->SetModes(&m_par);
        m_pVC1VideoDecoder->m_va->GetProtectedVA()->SetBitstream(bs);
    }
#endif
    }

    // not enough descriptors in queue for the next frame
    if (!m_pVC1VideoDecoder->m_pStore->IsReadyDS() && m_bIsNeedToProcFrame)
        return MFX_WRN_DEVICE_BUSY;

    // Check input surface for data correctness
    MFXSts  = CheckFrameInfo(&surface_work->Info);

    if (!m_IsOpaq)
    {
        MFXSts  = CheckFrameData(surface_work);
        MFX_CHECK_STS(MFXSts);
    }
    else
    {
        bool opaqSfsIsClean = !(surface_work->Data.Y || surface_work->Data.U || surface_work->Data.V || surface_work->Data.MemId);
        MFX_CHECK(opaqSfsIsClean, MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    MFXSts = MFX_ERR_MORE_DATA;

    //out last frame
    if (!bs)
    {

        // remain bytes processing if advanced profile
        if (m_SaveBytesSize)
        {
            m_sbs.DataLength = m_SaveBytesSize;
            bs = &m_sbs;
            m_SaveBytesSize = 0;
        }
        else if (!m_bIsNeedToProcFrame)
        {
            // Range map processing
            return  SelfDecodeFrame(surface_work, surface_disp, bs);
        }
        else
        {
            m_bTakeBufferedFrame = false;
            // Lets return buffered frames
            // Or set frame order in case of decoding order
            return ReturnLastFrame(surface_work, surface_disp);
        }
    }

    // construct frame functionality
    if (m_bIsNeedToProcFrame)
    {
        while((bs->DataLength > 4  ||
              (bs == &m_sbs && bs->DataLength) ||
              (bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME && bs->DataLength)||
              (bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME && m_FrameConstrData.GetDataSize() && (bs->DataLength==0)))  &&
               MFXSts == MFX_ERR_MORE_DATA )
        {
            if (m_bIsNeedToProcFrame)
            {
                if(bs->DataLength)
                {
                    MFXSts = SelfConstructFrame(bs);
                    if (MFX_WRN_IN_EXECUTION == MFXSts)
                    {
                        MFXSts = MFX_ERR_MORE_DATA;
                        continue;
                    }
                    // we sure that bitstream contains full frame
                    if ((bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME)&&
                        (MFX_ERR_MORE_DATA == MFXSts))
                        continue;
                    MFX_CHECK_STS(MFXSts);
                }

                MFXSts = SelfDecodeFrame(surface_work, surface_disp, bs);
            }
        }
        if (bs == &m_sbs)
        {
            UMC::AutomaticUMCMutex guard(m_guard);
            bs = NULL;
            if (MFX_ERR_MORE_DATA == MFXSts)
                return ReturnLastFrame(surface_work, surface_disp);
        }
    }
    else    // Range map processing
        MFXSts = SelfDecodeFrame(surface_work, surface_disp, bs);

    return MFXSts;
}
mfxStatus MFXVideoDECODEVC1::SelfConstructFrame(mfxBitstream *bs)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXVideoDECODEVC1::SelfConstructFrame");
    mfxStatus MFXSts = MFX_ERR_NONE;
    Status IntUMCStatus = UMC_OK;

    if (0 == (m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height))
    {
        MFXSts = MFXVC1DecCommon::ParseSeqHeader(bs, &m_par, 0, 0);
        MFX_CHECK_STS(MFXSts);
        MFXSts = Reset(&m_par);
        MFX_CHECK_STS(MFXSts);
    }
    mfxU32 ReadDataSize = (mfxU32)m_FrameConstrData.GetDataSize();
    mfxU32 Offset = 0;

    if (MFX_PROFILE_VC1_ADVANCED != m_par.mfx.CodecProfile)
    {
        ConvertMfxBSToMediaDataForFconstr(bs);
        if ((*(bs->Data+3) == 0xC5)&& (!m_bIsInit)) // sequence header of simple/main profile
        {
            m_FrameSize  = 4;
            uint8_t* ptemp = bs->Data + bs->DataOffset + 4;
            uint32_t temp_size = ((*(ptemp+3))<<24) + ((*(ptemp+2))<<16) + ((*(ptemp+1))<<8) + *(ptemp);


            m_FrameSize += temp_size;
            m_FrameSize +=12;
            ptemp = (uint8_t*)bs->Data + bs->DataOffset + m_FrameSize;
            temp_size = ((*(ptemp+3))<<24) + ((*(ptemp+2))<<16) + ((*(ptemp+1))<<8) + *(ptemp);
            m_FrameSize += temp_size;
            m_FrameSize +=4;
            m_bIsInit = true;

            m_RawSeq.resize(m_FrameSize);
            std::copy(bs->Data, bs->Data + m_FrameSize, std::begin(m_RawSeq));
        }
        else
        {
            if (!m_FrameConstrData.GetDataSize()) // begin of the frame
            {
                uint8_t* pCur = bs->Data + bs->DataOffset;
                m_FrameSize  = ((*(pCur+3))<<24) + ((*(pCur+2))<<16) + ((*(pCur+1))<<8) + *(pCur);
                m_FrameSize &= 0x0fffffff;
                m_FrameSize += 8;
                m_qBSTS.push_back(bs->TimeStamp);
            }
        }

        uint32_t dataSize = ((bs->DataLength) > (m_FrameSize - ReadDataSize))?(m_FrameSize - ReadDataSize):(bs->DataLength);
        uint32_t remainedBytes = (uint32_t)(m_FrameConstrData.GetBufferSize() - m_FrameConstrData.GetDataSize());
        dataSize = (dataSize > remainedBytes) ? remainedBytes : dataSize;
        MFX_INTERNAL_CPY((uint8_t*)m_FrameConstrData.GetBufferPointer() + m_FrameConstrData.GetDataSize(),
                    bs->Data + bs->DataOffset,
                    dataSize);
        m_FrameConstrData.SetDataSize(m_FrameConstrData.GetDataSize() + dataSize);
        if (dataSize < (m_FrameSize - ReadDataSize))
        {
            bs->DataLength -= dataSize;
            bs->DataOffset += dataSize;
            return MFX_ERR_MORE_DATA;
        }
        else
        {
            bs->DataLength -= (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize);
            bs->DataOffset += (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize);
        }
        return MFX_ERR_NONE;
    }
    else
    {
        ConvertMfxBSToMediaDataForFconstr(bs);

        IntUMCStatus = m_frame_constructor->GetNextFrame(m_FCInfo);
        if (FrameStartCodePresence())
            m_qBSTS.push_back(bs->TimeStamp);

        m_SHSize = 0;
        if (IntUMCStatus == UMC::UMC_OK)
        {
            // Let continue if zero frame size
            // Possible when application gives zero data size in bs
            if (m_FrameConstrData.GetDataSize() == 0)
            {
                PrepareMediaIn();
                return MFX_WRN_IN_EXECUTION;
            }
            // Checking for SH presence
            // If presence doesn't move bs pointers
            // Decision about pointers shifting after SH parsing
            {
                mfxU32 start_code = *((mfxU32*)(m_FrameConstrData.GetBufferPointer()));
                if (0x0F010000 == start_code)
                {
                    m_SHSize = (mfxU32)m_FrameConstrData.GetDataSize() - ReadDataSize + Offset;
                    if (m_SHSize)
                    {
                        m_RawSeq.resize(m_SHSize);
                        std::copy(reinterpret_cast<uint8_t*>(m_FrameConstrData.GetBufferPointer()),
                                  reinterpret_cast<uint8_t*>(m_FrameConstrData.GetBufferPointer()) + m_SHSize,
                                  std::begin(m_RawSeq));
                    }
                    return MFX_ERR_NONE;
                }
            }
            bs->DataLength -= (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize + Offset);
            bs->DataOffset += (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize + Offset);

            m_SaveBytesSize = 0;
            return MFX_ERR_NONE;
        }
        else
        {
            // Checking for SH presence
            // SH should be whole in bs
            {
                mfxU32 start_code = *((mfxU32*)(m_FrameConstrData.GetBufferPointer()));
                if ((0x0F010000 == start_code)&&(m_FrameConstrData.GetDataSize() > 3) && (bs->DataFlag != MFX_BITSTREAM_COMPLETE_FRAME))
                {
                     return (IntUMCStatus == UMC::UMC_ERR_NOT_ENOUGH_DATA)?MFX_ERR_MORE_DATA:MFX_ERR_NOT_ENOUGH_BUFFER;
                }
            }

            bs->DataLength -= (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize + Offset);
            bs->DataOffset += (mfxU32)(m_FrameConstrData.GetDataSize() - ReadDataSize + Offset);

            if (IntUMCStatus == UMC::UMC_ERR_NOT_ENOUGH_DATA)
            {

                if (bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME)
                {
                    if (bs->DataLength <= 4)
                    {
                        std::copy(bs->Data + bs->DataOffset, bs->Data + bs->DataOffset + bs->DataLength,
                            reinterpret_cast<uint8_t*>(m_FrameConstrData.GetBufferPointer()) + m_FrameConstrData.GetDataSize());
                        m_sbs.TimeStamp = bs->TimeStamp;
                    }
                    m_SaveBytesSize = 0;
                    m_FrameConstrData.SetDataSize(m_FrameConstrData.GetDataSize() + bs->DataLength);
                    bs->DataOffset = bs->DataOffset + bs->DataLength;
                    ReadDataSize = bs->DataOffset;
                    bs->DataLength = 0;
                }
                else
                {
                    m_SaveBytesSize = bs->DataLength;
                    MFX_CHECK(m_SaveBytesSize <= 4, MFX_ERR_UNDEFINED_BEHAVIOR);
                    std::copy(bs->Data + bs->DataOffset, bs->Data + bs->DataOffset + m_SaveBytesSize, m_pSaveBytes);
                    m_sbs.TimeStamp = bs->TimeStamp;
                }
                return MFX_ERR_MORE_DATA;
            }
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
    }
}

mfxStatus MFXVideoDECODEVC1::SelfDecodeFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp, mfxBitstream *bs)
{
    mfxStatus  MFXSts;
    Status     IntUMCStatus;

    m_bTakeBufferedFrame = true;

    MFXSts = ConvertMfxPlaneToMediaData(surface_work);
    MFX_CHECK_STS(MFXSts);

    if (m_bIsNeedToProcFrame)
    {
        //time stamps processing
        if (m_qBSTS.size())
        {
            m_FrameConstrData.SetTime(GetUmcTimeStamp(m_qBSTS.front()));
        }
        else
        {
            m_FrameConstrData.SetTime(-1.0);
        }

        IntUMCStatus = m_pVC1VideoDecoder->GetFrame(&m_FrameConstrData, &m_InternMediaDataOut);
    }
    else  // mean that we need one more surface for range map
    {
        m_bIsNeedToProcFrame  = true;
        // only set one more surface
        MFX_CHECK_UMC_STS(m_pVC1VideoDecoder->SetRMSurface());
        PrepareMediaIn();

        m_qMemID.push_back(m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder, m_bIsSamePolar));
        if (m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder, m_bIsSamePolar) > -1)
            m_qSyncMemID.push_back(m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder, m_bIsSamePolar));
        UMC::FrameMemID memID = GetDispMemID();
        m_SubmitFrame++;

        MFXSts = FillOutputSurface(surface_work, memID);
        MFX_CHECK_STS(MFXSts);

        if (m_qSyncMemID.size())
        {
            memID = m_qSyncMemID.front();
            m_qSyncMemID.pop_front();
        }

        if (memID > -1)
        {
            MFXSts = GetOutputSurface(surface_disp, surface_work, memID);
            MFX_CHECK_STS(MFXSts);
            m_CurrentBufFrame++;
        }
        else
        {
            *surface_disp = 0;
            if (bs && bs->DataLength > 4)
                return MFX_ERR_MORE_SURFACE;
            else
                return MFX_ERR_MORE_DATA;
        }


        return IsDisplayFrameReady(surface_disp);
    }

    if (bs &&
        (IntUMCStatus == UMC::UMC_ERR_NOT_ENOUGH_DATA)&&
        (m_InternMediaDataOut.GetFrameType() == NONE_PICTURE)) // SH, EH processing
    {
        PrepareMediaIn();

        // moving pointers if SH
        // otherwise m_SHSize is 0
        bs->DataLength -= m_SHSize;
        bs->DataOffset += m_SHSize;

        return MFX_ERR_MORE_DATA;
    }

    else if ((IntUMCStatus == UMC::UMC_OK)||
             (IntUMCStatus == UMC::UMC_ERR_NOT_ENOUGH_DATA))
    {
        // Updated parameters with weq header data
        if (0 == (m_par.mfx.FrameInfo.FrameRateExtD * m_par.mfx.FrameInfo.FrameRateExtN))
        {
            // new frame rate parameters
            mfxU32 frCode;
            m_par.mfx.FrameInfo.FrameRateExtD = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRAMERATEDR;
            m_par.mfx.FrameInfo.FrameRateExtN = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRAMERATENR;
            frCode = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRMRTQ_POSTPROC;

            // special case
            if (7 == m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRMRTQ_POSTPROC &&
                (m_par.mfx.FrameInfo.FrameRateExtN > 7 || m_par.mfx.FrameInfo.FrameRateExtD > 2))
            {
                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                m_par.mfx.FrameInfo.FrameRateExtN = 30;
            }
            else
            {
                MapFrameRateIntoMfx(m_par.mfx.FrameInfo.FrameRateExtN,
                                    m_par.mfx.FrameInfo.FrameRateExtD,
                                    (mfxU16)frCode);

            }
        }
        if (!m_par.mfx.FrameInfo.AspectRatioH)
        {
            // new aspect ratio parameters
            if (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioH)
                m_par.mfx.FrameInfo.AspectRatioH = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioH;
        }

        if (!m_par.mfx.FrameInfo.AspectRatioW)
        {
            // new aspect ratio parameters
            if (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioW)
                m_par.mfx.FrameInfo.AspectRatioW = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioW;
        }

        if ((m_InternMediaDataOut.GetFrameType() != NONE_PICTURE)) // non SH
        {
            VC1TSDescriptor td;
            MFX_CHECK(!m_qBSTS.empty(), MFX_ERR_UNDEFINED_BEHAVIOR);

            if (m_qBSTS.front() == MFX_TIME_STAMP_INVALID)
            {
                td.pts = GetMfxTimeStamp(m_InternMediaDataOut.GetTime());
                td.isOriginal = false;
            }
            else
            {
                td.pts = m_qBSTS.front();
                td.isOriginal = true;
            }
            m_qBSTS.pop_front();
            m_qTS.push_back(td);

#ifndef VC1_SKIPPED_DISABLE
            // if we faced with skipped frame - let coping it
            if (!m_isSWPlatform &&
                m_pVC1VideoDecoder->IsLastFrameSkipped() &&
                m_bIsSamePolar)
            {
                MFXSts = ProcessSkippedFrame();
                MFX_CHECK_STS(MFXSts);
            }
#endif
        }

        if (((m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG)||
            (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG)||
            (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.RANGERED))&&
            (!m_pVC1VideoDecoder->IsLastFrameSkipped())) // skipped picture
        {
            m_bIsNeedToProcFrame = false;
            return MFX_ERR_MORE_SURFACE;
        }


        m_SubmitFrame++;
        PrepareMediaIn();
        // working with external frames
        m_qMemID.push_back(m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder, m_bIsSamePolar));
        if (m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder,  m_bIsSamePolar) > -1)
            m_qSyncMemID.push_back(m_pVC1VideoDecoder->GetDisplayIndex(m_bIsDecodeOrder, m_bIsSamePolar));

        UMC::FrameMemID memID = GetDispMemID();

        // if decode order return surface immediately
        if (m_bIsDecodeOrder)
            IntUMCStatus = UMC::UMC_OK;

        if ((IntUMCStatus == UMC::UMC_OK) && m_qSyncMemID.size())
        {
            memID = m_qSyncMemID.front();
            m_qSyncMemID.pop_front();
        }

        MFXSts = FillOutputSurface(surface_work, memID);
        MFX_CHECK_STS(MFXSts);


        if ((memID > -1) &&(IntUMCStatus == UMC::UMC_OK))
        {
            MFXSts = GetOutputSurface(surface_disp, surface_work, memID);
            MFX_CHECK_STS(MFXSts);

            m_CurrentBufFrame++;
        }
        else
        {
            *surface_disp = 0;
        }

        if (IntUMCStatus == UMC::UMC_OK && *surface_disp)
        {
            return IsDisplayFrameReady(surface_disp);
        }
        else
        {
            if (bs && bs->DataLength > 4)
                return MFX_ERR_MORE_SURFACE;
            else
                return MFX_ERR_MORE_DATA;
        }

    }
    else if (bs && IntUMCStatus == UMC::UMC_NTF_NEW_RESOLUTION) // SH with valid params
    {
        // new frame rate parameters
        mfxU32 frCode;
        m_par.mfx.FrameInfo.FrameRateExtD = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRAMERATEDR;
        m_par.mfx.FrameInfo.FrameRateExtN = m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRAMERATENR;
        frCode = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRMRTQ_POSTPROC;

        // special case
        if (7 == m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.FRMRTQ_POSTPROC
            && (m_par.mfx.FrameInfo.FrameRateExtN > 7 || m_par.mfx.FrameInfo.FrameRateExtD > 2))
        {
            m_par.mfx.FrameInfo.FrameRateExtD = 1;
            m_par.mfx.FrameInfo.FrameRateExtN = 30;
        }
        else
        {
            MapFrameRateIntoMfx(m_par.mfx.FrameInfo.FrameRateExtN,
                                m_par.mfx.FrameInfo.FrameRateExtD,
                                (mfxU16)frCode);

        }
        // new aspect ratio parameters
        if (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioH)
            m_par.mfx.FrameInfo.AspectRatioH = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioH;
        if (m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioW)
            m_par.mfx.FrameInfo.AspectRatioW = (mfxU16)m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.AspectRatioW;

        bs->DataLength -= m_SHSize;
        bs->DataOffset += m_SHSize;

       //new crop parameters
        m_par.mfx.FrameInfo.CropW = (mfxU16)(2*(m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.CODED_WIDTH+1));
        m_par.mfx.FrameInfo.CropH = (mfxU16)(2*(m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.CODED_HEIGHT+1));

        return MFX_WRN_VIDEO_PARAM_CHANGED;
    }

    MFX_CHECK(IntUMCStatus != UMC::UMC_ERR_INVALID_PARAMS, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(IntUMCStatus != UMC::UMC_ERR_LOCK, MFX_ERR_LOCK_MEMORY);
    MFX_CHECK(IntUMCStatus != UMC::UMC_ERR_UNSUPPORTED, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(IntUMCStatus != UMC::UMC_OK, MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECODEVC1::ReturnLastFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_disp)
{
    m_bTakeBufferedFrame = false;

    // Need to think about HW

    if (m_DisplayList.size() &&  m_bIsBuffering)
    {
        std::vector<mfxFrameSurface1*>::iterator it = m_DisplayList.begin();
        *surface_disp = *it;
        mfxStatus MFXSts = m_pCore->DecreaseReference(&(*surface_disp)->Data);
        MFX_CHECK_STS(MFXSts);

        m_DisplayList.erase(it);
        return MFX_ERR_NONE;
    }

    if (m_bIsDecodeOrder)
    {
        SetFrameOrder(m_pFrameAlloc.get(), &m_par, true, m_qTS.front(), m_bIsSamePolar);
        return MFX_ERR_MORE_DATA;
    }

    m_pVC1VideoDecoder->m_pStore->SeLastFramesMode();

    FrameMemID memID = m_pVC1VideoDecoder->GetLastDisplayIndex();

    m_qMemID.push_back(memID);
    m_qSyncMemID.push_back(memID);

    memID = m_qMemID.front();
    if (memID > -1)
    {
        memID = m_qSyncMemID.front();
        m_qSyncMemID.pop_front();
    }

    if (memID > -1)
    {
        mfxU16 Corrupted = 0;
        mfxStatus MFXSts = GetOutputSurface(surface_disp, surface_work, memID);
        MFX_CHECK_STS(MFXSts);

        m_pVC1VideoDecoder->SetCorrupted(NULL, Corrupted);
        (*surface_disp)->Data.Corrupted = Corrupted;

        return MFX_ERR_NONE;
    }
    else
    {
        *surface_disp = 0;
        memID = m_qMemID.back();

        switch (memID) {
        case -1:
        case -2:
            return MFX_ERR_MORE_DATA;
        default:
            // only one frame in stream | reset + decodeframeasync + drain with zero bitstream
            // memID may be arbitrary in case of external allocator
            mfxU16 Corrupted = 0;
            mfxStatus MFXSts;

            m_qSyncMemID.pop_back();
            MFXSts = GetOutputSurface(surface_disp, surface_work, memID);
            MFX_CHECK_STS(MFXSts);

            m_pVC1VideoDecoder->SetCorrupted(NULL, Corrupted);
            (*surface_disp)->Data.Corrupted = Corrupted;

            return MFX_ERR_NONE;
        }
    }

}
void      MFXVideoDECODEVC1::PrepareMediaIn(void)
{
    m_FrameConstrData.SetBufferPointer(m_pReadBuffer, m_BufSize);
    m_FrameConstrData.SetDataSize(0);
    m_pStCodes->count = 0;
    m_bPTSTaken = false;

}

mfxStatus MFXVideoDECODEVC1::PostProcessFrameHW(mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp)
{
    mfxStatus  sts = MFX_ERR_NONE;
    FrameMemID memID;
    FrameMemID memIDdisp = -1;
    bool isSkip = false;
    mfxFrameSurface1 *surface_out;
    mfxU16 Corrupted = 0;

    if (m_DisplayListAsync.size() && m_bIsBuffering)
    {
        surface_out = m_DisplayListAsync.front();
        surface_out = GetOriginalSurface(surface_out);
    }
    else
        surface_out = surface_disp;

    memIDdisp = m_qMemID.front();
    m_qMemID.pop_front();
    memID = m_pVC1VideoDecoder->ProcessQueuesForNextFrame(isSkip, Corrupted);
    if (isSkip) // calculate buffered skip frames
    {
        m_ProcessedFrames++;
    }
    if ((memID > -1) && !m_bIsSWD3D)
    {
        sts = m_pFrameAlloc->PrepareToOutput(surface_work, memID, &m_par, m_IsOpaq);
        MFX_CHECK_STS(sts);
    }

    if (-1 == memIDdisp)
        return MFX_ERR_MORE_DATA;

    if (m_bIsSWD3D && memIDdisp > -1)
    {
        if (!(m_bIsDecodeOrder && isSkip))
            sts = m_pFrameAlloc->PrepareToOutput(surface_out, memIDdisp, &m_par, m_IsOpaq);
        MFX_CHECK_STS(sts);
    }
    m_pVC1VideoDecoder->UnlockSurfaces();

    if (m_bTakeBufferedFrame && memID > -1)
        surface_out->Data.Corrupted = Corrupted;

    if (m_DisplayListAsync.size())
    {
        std::vector<mfxFrameSurface1*>::iterator it = m_DisplayListAsync.begin();
        m_DisplayListAsync.erase(it);
        if (m_BufOffset > 0)
            m_BufOffset--;
    }

    return MFX_ERR_NONE;
}

#ifdef ALLOW_SW_VC1_FALLBACK
mfxStatus MFXVideoDECODEVC1::PostProcessFrame(mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_disp)
{
    mfxStatus  sts = MFX_ERR_NONE;
    FrameMemID memID;
    FrameMemID memIDdisp = -1;
    bool isSkip = false;
    mfxFrameSurface1 *surface_out;
    mfxU16 Corrupted = 0;

    // m_DisplayList - nned for buffering in HW mode only
    surface_out = surface_disp;
    {
        memIDdisp = m_qMemID.front();
        m_qMemID.pop_front();
        if (m_bIsSamePolar)
        {
            sts = ProcessSkippedFrame();
            MFX_CHECK_STS(sts);
        }

        memID = m_pVC1VideoDecoder->ProcessQueuesForNextFrame(isSkip, Corrupted);
        if ((memID > -1) && !m_bIsSWD3D)
        {
            sts = m_pFrameAlloc->PrepareToOutput(surface_work, memID, &m_par, m_IsOpaq);
            MFX_CHECK_STS(sts);
        }
    }

    if (m_bIsSWD3D && memIDdisp > -1)
    {
        if (!(m_bIsDecodeOrder && isSkip))
            sts = m_pFrameAlloc->PrepareToOutput(surface_out, memIDdisp, &m_par, m_IsOpaq);
        MFX_CHECK_STS(sts);
    }
    m_pVC1VideoDecoder->UnlockSurfaces();

    if (memID > -1)
        surface_disp->Data.Corrupted = Corrupted;

    return (-1 == memIDdisp)?MFX_ERR_MORE_DATA:MFX_ERR_NONE;
}
#endif
mfxStatus MFXVideoDECODEVC1::GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR3(ud, sz, ts);
    return MFXSts;
}
mfxStatus MFXVideoDECODEVC1::DecodeHeader(VideoCORE *, mfxBitstream *bs, mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR2(bs, par);

    mfxStatus MFXSts  = CheckBitstream(bs);
    MFX_CHECK_STS(MFXSts);

    mfxExtVideoSignalInfo *pVideoSignal = (mfxExtVideoSignalInfo *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_VIDEO_SIGNAL_INFO);
    mfxExtCodingOptionSPSPPS *pSPS = (mfxExtCodingOptionSPSPPS *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_CODING_OPTION_SPSPPS);

    mfxVideoParam temp;
    MFXSts = MFXVC1DecCommon::ParseSeqHeader(bs, &temp, pVideoSignal, pSPS);
    if(MFXSts == MFX_ERR_NOT_INITIALIZED) return MFX_ERR_MORE_DATA;
    if((MFXSts == MFX_ERR_MORE_DATA) && (bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME || bs->DataFlag & MFX_BITSTREAM_EOS))
        MFXSts = MFX_ERR_NONE;

    MFX_CHECK_STS(MFXSts);

    par->mfx.FrameInfo = temp.mfx.FrameInfo;

    par->mfx.CodecProfile = temp.mfx.CodecProfile;
    par->mfx.CodecLevel = temp.mfx.CodecLevel;
    return MFXSts;
}
mfxStatus MFXVideoDECODEVC1::SetSkipMode(mfxSkipMode mode)
{
    MFX_CHECK(m_bIsDecInit, MFX_ERR_NOT_INITIALIZED);

    Status umc_sts = UMC_OK;
    int32_t speed_shift;
    switch(mode)
    {
    case MFX_SKIPMODE_NOSKIP:
        speed_shift = -10;
        break;
    case MFX_SKIPMODE_MORE:
        speed_shift = 1;
        break;
    case MFX_SKIPMODE_LESS:
        speed_shift = -1;
        break;
    default:
        {
#if !defined OPEN_SOURCE
           // disable deblocking
            speed_shift = (int32_t)mode;
            if (0x22 == speed_shift)
            {
                break;
            }
            else if (0x23 == speed_shift)
            {
                m_bStsReport = false;
                return MFX_ERR_NONE;
            }
#endif
            MFX_RETURN(MFX_ERR_INVALID_HANDLE);
        }
    }

    umc_sts = m_pVC1VideoDecoder->ChangeVideoDecodingSpeed(speed_shift);
    if (UMC_OK != umc_sts)
    {
        return MFX_WRN_VALUE_NOT_CHANGED;
    }
    else
    {
        return MFX_ERR_NONE;
    }
}
mfxStatus MFXVideoDECODEVC1::GetPayload( mfxU64 *ts, mfxPayload *payload )
{
    MFX_CHECK_NULL_PTR2(ts, payload);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECODEVC1::ConvertMfxToCodecParams(mfxVideoParam *par)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    UMC::VideoDecoderParams *init = DynamicCast<UMC::VideoDecoderParams, UMC::BaseCodecParams>(m_VideoParams);

    //Should be always
    if (init)
    {
        m_pCore->GetVA((mfxHDL*)&init->pVideoAccelerator, MFX_MEMTYPE_FROM_DECODE);
        init->info.clip_info.height = par->mfx.FrameInfo.Height;
        init->info.clip_info.width = par->mfx.FrameInfo.Width;

        init->numThreads = (0 != par->mfx.NumThread)?par->mfx.NumThread:m_pCore->GetNumWorkingThreads();
        init->numThreads += disp_queue_size;


        if (!par->mfx.DecodedOrder)
            init->lFlags |= UMC::FLAG_VDEC_REORDER;

        if (MFX_TIMESTAMPCALC_TELECINE == par->mfx.TimeStampCalc)
            init->lFlags |= UMC::FLAG_VDEC_TELECINE_PTS;

        init->info.framerate = CalculateUMCFramerate( par->mfx.FrameInfo.FrameRateExtN, par->mfx.FrameInfo.FrameRateExtD);
        init->info.aspect_ratio_width = par->mfx.FrameInfo.AspectRatioW;
        init->info.aspect_ratio_height = par->mfx.FrameInfo.AspectRatioH;

        // new constructor possible
        if (m_frame_constructor)
        {
            delete m_frame_constructor;
            m_frame_constructor = 0;
        }

        if (MFX_PROFILE_VC1_ADVANCED == par->mfx.CodecProfile)
        {
            init->info.stream_subtype = UMC::VC1_VIDEO_VC1;
            init->info.stream_type = UMC::VC1_VIDEO;

            m_frame_constructor = new UMC::vc1_frame_constructor_vc1;
        }
        else if ((MFX_PROFILE_VC1_MAIN == par->mfx.CodecProfile)||
                 (MFX_PROFILE_VC1_SIMPLE == par->mfx.CodecProfile))

        {
            init->info.stream_subtype = UMC::VC1_VIDEO_RCV;
            m_frame_constructor = new UMC::vc1_frame_constructor_rcv;
        }

        if (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
            init->info.interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;
        else
            init->info.interlace_type = UMC::PROGRESSIVE;
    }
    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::ConvertMfxBSToMediaDataForFconstr(mfxBitstream    *pBitstream)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    m_pInternMediaDataIn->SetBufferPointer(pBitstream->Data + pBitstream->DataOffset,pBitstream->DataLength); // splitter works
    m_pInternMediaDataIn->SetDataSize(0);
    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::ConvertMfxPlaneToMediaData(mfxFrameSurface1 *surface)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;
    mfxFrameSurface1 *pNativeSurface = surface;

    if (MFX_IOPATTERN_OUT_OPAQUE_MEMORY & m_par.IOPattern)
    pNativeSurface = GetOriginalSurface(surface);

    mfxFrameData* pData = &pNativeSurface->Data;


    MFXSts = m_pFrameAlloc->SetCurrentMFXSurface(pNativeSurface, m_IsOpaq);
    MFX_CHECK_STS(MFXSts);

    m_InternMediaDataOut.Init(m_par.mfx.FrameInfo.Width, m_par.mfx.FrameInfo.Height, 3);

    m_InternMediaDataOut.SetPlanePointer((void*)pData->Y, 0);
    m_InternMediaDataOut.SetPlanePointer((void*)pData->V, 2);
    m_InternMediaDataOut.SetPlanePointer((void*)pData->U, 1);

    m_InternMediaDataOut.SetPlanePitch(pData->Pitch, 0);
    m_InternMediaDataOut.SetPlanePitch(pData->Pitch/2, 1);
    m_InternMediaDataOut.SetPlanePitch(pData->Pitch/2, 2);

    return MFXSts;
}

mfxStatus MFXVideoDECODEVC1::QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = SetAllocRequestExternal(core, par, request);
    MFX_CHECK_STS(sts);

    MFX_CHECK(
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)  ||
        (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) ||
        (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(
        !(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
        !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(
        !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
        !(par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(
        !(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ||
        !(par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY),
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (!IsHWSupported(core, par))
    {
        sts = MFX_WRN_PARTIAL_ACCELERATION;
    }

    return sts;
}
mfxStatus MFXVideoDECODEVC1::SetAllocRequestInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);
    MFX_CHECK(par->IOPattern, MFX_ERR_INVALID_VIDEO_PARAM);
    par->mfx.FrameInfo.CropX = 0;
    par->mfx.FrameInfo.CropY = 0;

    request->Info = par->mfx.FrameInfo;
    request->Info.FourCC = MFX_FOURCC_NV12;

    bool isSWplatform = true;

    if (MFX_PLATFORM_HARDWARE == core->GetPlatformType() && IsHWSupported(core, par))
        isSWplatform = false;

    if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY && isSWplatform) // all surfaces are external no need to define allocatione request
        return MFX_ERR_NONE;

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY && isSWplatform) // external D3D surfaces, sw platform
    {
        CalculateFramesNumber(request, par, IsBufferMode(core, par));
        request->Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        return MFX_ERR_NONE;
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY && !isSWplatform) // external Sys memory, hw platform
    {
        CalculateFramesNumber(request, par, IsBufferMode(core, par));
        request->Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        return MFX_ERR_NONE;
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY && !isSWplatform) // no need to real alloc but have to request surface for video acceler
    {
        CalculateFramesNumber(request, par, IsBufferMode(core, par));
        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
        return MFX_ERR_NONE;
    }

    if (par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        CalculateFramesNumber(request, par, IsBufferMode(core, par));
    }

    return MFX_ERR_NONE;
}
mfxStatus MFXVideoDECODEVC1::SetAllocRequestExternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR2(par, request);
    MFX_CHECK(par->IOPattern, MFX_ERR_INVALID_VIDEO_PARAM);
    par->mfx.FrameInfo.CropX = 0;
    par->mfx.FrameInfo.CropY = 0;

    request->Info = par->mfx.FrameInfo;
    request->Info.FourCC = MFX_FOURCC_NV12;

    bool isSWplatform = true;

    if (MFX_PLATFORM_HARDWARE == core->GetPlatformType() && IsHWSupported(core, par))
        isSWplatform = false;

    if (par->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
    {
        if (isSWplatform)
            CalculateFramesNumber(request, par, IsBufferMode(core, par));
        else
            request->NumFrameSuggested = request->NumFrameMin = (par->AsyncDepth > 0)?2*par->AsyncDepth:2*MFX_AUTO_ASYNC_DEPTH_VALUE;

        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
    }
    else if (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)
    {
        if (!isSWplatform)
            CalculateFramesNumber(request, par, IsBufferMode(core, par));
        else
            request->NumFrameSuggested = request->NumFrameMin = (par->AsyncDepth > 0)?2*par->AsyncDepth:2*MFX_AUTO_ASYNC_DEPTH_VALUE;

        request->Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }
    else // opaque memory
    {
        CalculateFramesNumber(request, par, IsBufferMode(core, par));
        if (isSWplatform)
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
        else
            request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
    }

    return MFX_ERR_NONE;
}

void MFXVideoDECODEVC1::CalculateFramesNumber(mfxFrameAllocRequest *request, mfxVideoParam *par, bool isBufMode)
{
    mfxU16 NumAddSurfaces = isBufMode?disp_queue_size:0;

    request->NumFrameMin = 2*(VC1VideoDecoder::NumBufferedFrames + VC1VideoDecoder::NumReferenceFrames) + NumAddSurfaces; // 2 - from range mapping needs
    request->NumFrameMin += 2*(par->AsyncDepth ?par->AsyncDepth : MFX_AUTO_ASYNC_DEPTH_VALUE);
    request->NumFrameSuggested = request->NumFrameMin;
}

mfxStatus MFXVideoDECODEVC1::CheckFrameInfo(mfxFrameInfo *info)
{
    MFX_CHECK(info->Width <= m_Initpar.mfx.FrameInfo.Width, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(info->Height <= m_Initpar.mfx.FrameInfo.Height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECODEVC1::CheckInput(mfxVideoParam *in)
{
    MFX_CHECK(in->mfx.FrameInfo.Width % 16 == 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->mfx.FrameInfo.Height % 16 == 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->IOPattern, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420, MFX_ERR_INVALID_VIDEO_PARAM);
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECODEVC1::CheckForCriticalChanges(mfxVideoParam *in)
{
    mfxStatus sts = CheckFrameInfo(&in->mfx.FrameInfo);
    MFX_CHECK_STS(sts);

    if ((in->IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)) !=
        (m_Initpar.IOPattern & (MFX_IOPATTERN_OUT_OPAQUE_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY)))
    {
        MFX_RETURN(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    MFX_CHECK(in->mfx.CodecProfile == m_Initpar.mfx.CodecProfile, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(in->AsyncDepth == m_Initpar.AsyncDepth, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    MFX_CHECK(in->Protected == m_Initpar.Protected, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

    if (m_par.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc * opaqueNew = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        MFX_CHECK(opaqueNew, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(opaqueNew->In.Type == m_AlloExtBuffer.In.Type, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(opaqueNew->In.NumSurface == m_AlloExtBuffer.In.NumSurface, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        for (uint32_t i = 0; i < opaqueNew->In.NumSurface; i++)
        {
            MFX_CHECK(opaqueNew->In.Surfaces[i] == m_AlloExtBuffer.In.Surfaces[i], MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }

        MFX_CHECK(opaqueNew->Out.Type == m_AlloExtBuffer.Out.Type, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK(opaqueNew->Out.NumSurface == m_AlloExtBuffer.Out.NumSurface, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        for (uint32_t i = 0; i < opaqueNew->Out.NumSurface; i++)
        {
            MFX_CHECK(opaqueNew->Out.Surfaces[i] == m_AlloExtBuffer.Out.Surfaces[i], MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
    }

    return MFX_ERR_NONE;

}
mfxStatus MFXVideoDECODEVC1::GetDecodeStat(mfxDecodeStat *stat)
{
    MFX_CHECK(m_bIsDecInit, MFX_ERR_NOT_INITIALIZED);
    MFX_CHECK_NULL_PTR1(stat);

    stat->NumCachedFrame = m_pVC1VideoDecoder->m_iMaxFramesInProcessing;
    stat->NumError = 0;
    stat->NumSkippedFrame = 0;
    mfxU32 NumShiftFrames = (1 == m_pVC1VideoDecoder->m_iThreadDecoderNum) ? 1 : m_pVC1VideoDecoder->m_iThreadDecoderNum - 1;
    stat->NumFrame = (mfxU32)((m_pVC1VideoDecoder->m_lFrameCount > m_pVC1VideoDecoder->m_iMaxFramesInProcessing) ? (m_pVC1VideoDecoder->m_lFrameCount - NumShiftFrames) : m_pVC1VideoDecoder->m_lFrameCount);
    return MFX_ERR_NONE;
}
FrameMemID MFXVideoDECODEVC1::GetDispMemID()
{
    return m_qMemID.front();
}

bool MFXVideoDECODEVC1::IsHWSupported(VideoCORE *pCore, mfxVideoParam *par)
{
    if (MFX_PLATFORM_SOFTWARE != pCore->GetPlatformType())
    {
#if defined (MFX_VA)
        if (MFX_ERR_NONE != pCore->IsGuidSupported(sDXVA2_Intel_ModeVC1_D_Super, par))
            return false;
#endif
    }
    return true;
}

mfxStatus MFXVideoDECODEVC1::UpdateAllocRequest(mfxVideoParam *par,
                                                mfxFrameAllocRequest *request,
                                                mfxExtOpaqueSurfaceAlloc **pOpaqAlloc,
                                                bool &Mapping,
                                                bool &Polar)
{
    Mapping = false;
    if (!(par->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_NONE;

    m_IsOpaq = true;
    Polar = false;

    *pOpaqAlloc = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
    if (*pOpaqAlloc)
    {
        m_AlloExtBuffer = **pOpaqAlloc;
        MFX_CHECK(request->NumFrameMin <= (*pOpaqAlloc)->Out.NumSurface, MFX_ERR_INVALID_VIDEO_PARAM);

        {
            Mapping = true;

            MFX_CHECK(
                (*pOpaqAlloc)->Out.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET ||
                (*pOpaqAlloc)->Out.Type  & MFX_MEMTYPE_SYSTEM_MEMORY,
                MFX_ERR_INVALID_VIDEO_PARAM);

            MFX_CHECK(
                !((*pOpaqAlloc)->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY) ||
                !((*pOpaqAlloc)->Out.Type  & MFX_MEMTYPE_DXVA2_DECODER_TARGET),
                MFX_ERR_INVALID_VIDEO_PARAM);


            if ((*pOpaqAlloc)->Out.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            {
                if (m_isSWPlatform) // SW platform
                {
                    // need to map surfaces with opaque
                    request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_SYSTEM_MEMORY;
                    request->NumFrameMin = request->NumFrameSuggested = (mfxU16)(*pOpaqAlloc)->Out.NumSurface;
                }
                else
                {
                    mfxStatus sts;
                    Polar = true;
                    request->Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET | MFX_MEMTYPE_FROM_DECODE;
                    mfxFrameAllocRequest trequest = *request;
                    trequest.Type =  (mfxU16)(*pOpaqAlloc)->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)(*pOpaqAlloc)->Out.NumSurface;
                    sts = m_pCore->AllocFrames(&trequest,
                                               &m_response_op,
                                               (*pOpaqAlloc)->Out.Surfaces,
                                               (*pOpaqAlloc)->Out.NumSurface);

                    if (MFX_ERR_NONE != sts &&
                        MFX_ERR_UNSUPPORTED != sts) // unsupported means that current Core couldn;t allocate the surfaces
                        return sts;
                }
            }
            else
            {
                if (!m_isSWPlatform) // HW platform
                {
                    // need to map surfaces with opaque
                    request->Type = MFX_MEMTYPE_OPAQUE_FRAME | MFX_MEMTYPE_FROM_DECODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET;
                    request->NumFrameMin = request->NumFrameSuggested = (mfxU16)(*pOpaqAlloc)->Out.NumSurface;
                }
                else
                {
                    mfxStatus sts;
                    Polar = true;
                    request->Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_FROM_DECODE;
                    mfxFrameAllocRequest trequest = *request;
                    trequest.Type =  (mfxU16)(*pOpaqAlloc)->Out.Type;
                    trequest.NumFrameMin = trequest.NumFrameSuggested = (mfxU16)(*pOpaqAlloc)->Out.NumSurface;
                    sts = m_pCore->AllocFrames(&trequest,
                                               &m_response_op,
                                               (*pOpaqAlloc)->Out.Surfaces,
                                               (*pOpaqAlloc)->Out.NumSurface);

                    if (MFX_ERR_NONE != sts &&
                        MFX_ERR_UNSUPPORTED != sts) // unsupported means that current Core couldn;t allocate the surfaces
                        return sts;
                }

            }
        }
        return MFX_ERR_NONE;
    }
    else
        return MFX_ERR_NONE;
}
void   MFXVideoDECODEVC1::FillMFXDataOutputSurface(mfxFrameSurface1 *surface)
{
    if (!m_qTS.front().isOriginal)
    {
         m_qTS.front().pts += m_ext_dur;
    }

    if (m_bIsDecodeOrder)
    {
        surface->Data.FrameOrder = 0xFFFFFFFF;
        SetFrameOrder(m_pFrameAlloc.get(), &m_par, false, m_qTS.front(), m_bIsSamePolar);
        if (m_pVC1VideoDecoder->m_frameOrder > 0)
            m_qTS.pop_front();
    }
    else
    {
        surface->Data.FrameOrder = m_pVC1VideoDecoder->m_frameOrder++;
        surface->Data.TimeStamp = m_qTS.front().pts;
        surface->Data.DataFlag = (mfxU16)(m_qTS.front().isOriginal ? MFX_FRAMEDATA_ORIGINAL_TIMESTAMP : 0);
        m_qTS.pop_front();
    }

    if ((surface->Info.PicStruct & MFX_PICSTRUCT_FIELD_REPEATED) &&
        m_par.mfx.TimeStampCalc)
    {
        m_ext_dur += GetMfxTimeStamp((mfxF64)0.5*surface->Info.FrameRateExtD/surface->Info.FrameRateExtN);
    }
}

mfxStatus   MFXVideoDECODEVC1::FillOutputSurface(mfxFrameSurface1 *surface, UMC::FrameMemID memID)
{
    mfxStatus sts = MFX_ERR_NONE;
    // filling data of internal surface
    mfxFrameSurface1 *int_surface = m_pFrameAlloc->GetInternalSurface(m_pVC1VideoDecoder->GetDisplayIndex(true, false));
    if (int_surface)
        sts = FillOutputSurface(int_surface);
    else
        return FillOutputSurface(surface);

    if (memID >= 0)
    {
        int_surface = m_pFrameAlloc->GetInternalSurface(memID);
        if (int_surface)
            surface->Info = int_surface->Info;
    }

    MFX_CHECK_STS(sts);
    return sts;
}
mfxStatus   MFXVideoDECODEVC1::FillOutputSurface(mfxFrameSurface1 *surface)
{
    surface->Info.CropX = 0;
    surface->Info.CropY = 0;
    surface->Info.CropW = (mfxU16)(2*(m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.CODED_WIDTH+1));
    surface->Info.CropH = (mfxU16)(2*(m_pVC1VideoDecoder->m_pContext->m_seqLayerHeader.CODED_HEIGHT+1));

    surface->Info.AspectRatioH = m_par.mfx.FrameInfo.AspectRatioH;
    surface->Info.AspectRatioW = m_par.mfx.FrameInfo.AspectRatioW;

    if (0 == surface->Info.AspectRatioH)
    {
        surface->Info.AspectRatioH = 1;
    }

    if (0 == surface->Info.AspectRatioW)
    {
        surface->Info.AspectRatioW = 1;
    }

    surface->Info.FrameRateExtD = m_par.mfx.FrameInfo.FrameRateExtD;
    surface->Info.FrameRateExtN = m_par.mfx.FrameInfo.FrameRateExtN;

    if (m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->FCM == VC1_Progressive)
    {
        surface->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (m_par.mfx.ExtendedPicStruct)
        {
            // Define repeat frames according to spec Annex I
            if (0x1 == m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->RPTFRM)
                surface->Info.PicStruct |= MFX_PICSTRUCT_FRAME_DOUBLING;
            else if (0x2 == m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->RPTFRM)
                surface->Info.PicStruct |= MFX_PICSTRUCT_FRAME_TRIPLING;
        }
    }
    else
    {
        // Define display fields ordering according to spec Annex I
        if (m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_seqLayerHeader.PULLDOWN)
        {
            if (m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->TFF)
                surface->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;
            else
                surface->Info.PicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else
            surface->Info.PicStruct = MFX_PICSTRUCT_FIELD_TFF;

        if (m_par.mfx.ExtendedPicStruct)
        {
            if (m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->FCM == VC1_FrameInterlace)
                surface->Info.PicStruct |= MFX_PICSTRUCT_PROGRESSIVE;

            if (m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->RFF)
                surface->Info.PicStruct |= MFX_PICSTRUCT_FIELD_REPEATED;
        }
    }

    SetFrameType(m_pVC1VideoDecoder->m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->PTYPE, *surface);

    return MFX_ERR_NONE;
}

mfxStatus   MFXVideoDECODEVC1::GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index)
{
    mfxFrameSurface1 *pNativeSurface =  m_pFrameAlloc->GetSurface(index, surface_work, &m_par);
    MFX_CHECK(pNativeSurface, MFX_ERR_UNDEFINED_BEHAVIOR);
    *surface_out = (m_IsOpaq)?m_pCore->GetOpaqSurface(pNativeSurface->Data.MemId):pNativeSurface;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECODEVC1::IsDisplayFrameReady(mfxFrameSurface1 **surface_disp)
{
    mfxStatus MFXSts;
    if ((m_CurrentBufFrame <= disp_queue_size) && m_bIsBuffering)
    {
        m_bTakeBufferedFrame = false;
        if (m_bIsDecodeOrder)
        {
            SetFrameOrder(m_pFrameAlloc.get(), &m_par, false, m_qTS.front(), m_bIsSamePolar);
            if (m_pVC1VideoDecoder->m_frameOrder > 0)
                m_qTS.pop_front();
        }
        m_DisplayList.push_back(*surface_disp);
        m_DisplayListAsync.push_back(*surface_disp);
        MFXSts = m_pCore->IncreaseReference(&(*surface_disp)->Data);
        MFX_CHECK_STS(MFXSts);

        return MFX_ERR_MORE_SURFACE;
    }
    else
    {

        MFXSts = m_pCore->IncreaseReference(&(GetOriginalSurface(*surface_disp)->Data));
        MFX_CHECK_STS(MFXSts);

        if (m_bTakeBufferedFrame)
        {
            m_DisplayList.push_back(*surface_disp);
            m_DisplayListAsync.push_back(*surface_disp);
        }

        if (m_bIsBuffering)
        {
            *surface_disp = m_DisplayList[m_BufOffset++];
        }
        else
            *surface_disp = m_DisplayList.back();

        {
            std::vector<mfxFrameSurface1*>::iterator it = m_DisplayList.begin();
            m_DisplayList.erase(it);
            if (m_BufOffset > 0)
                m_BufOffset--;
        }

        MFXSts = m_pCore->DecreaseReference(&(GetOriginalSurface(*surface_disp)->Data));
        MFX_CHECK_STS(MFXSts);

        return MFX_ERR_NONE;
    }
}
bool MFXVideoDECODEVC1::IsBufferMode(VideoCORE *pCore, mfxVideoParam *par)
{
    (void)pCore;
    (void)par;

#if defined(MFX_VA_LINUX)
    if ((IsHWSupported(pCore, par) &&
        (MFX_PLATFORM_HARDWARE == pCore->GetPlatformType())&&
        (par->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)&&
        !par->mfx.DecodedOrder &&
        1 != par->AsyncDepth))
        return true;
    else
#endif
        return false;
}
mfxStatus MFXVideoDECODEVC1::RunThread(mfxFrameSurface1 *surface_work,
                                       mfxFrameSurface1 *surface_disp,
                                       mfxU32 threadNumber,
                                       mfxU32 taskID)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "MFXVideoDECODEVC1::RunThread");

    (void)threadNumber;
    mfxStatus sts;

#ifdef ALLOW_SW_VC1_FALLBACK
    // for SW we need to process task
    // for HW just process ready frame
    if (m_isSWPlatform)
    {
        Status umcRes = m_pVC1VideoDecoder->RunThread(threadNumber);  // threadNumber can be more than real number of m_pdecoder!!!!

        if (UMC_ERR_NOT_ENOUGH_DATA == umcRes || UMC_WRN_INFO_NOT_READY == umcRes)
            return MFX_TASK_NEED_CONTINUE;
        else if (UMC_OK == umcRes || UMC_LAST_FRAME == umcRes) // surface is prepare to output
        {
            UMC::AutomaticUMCMutex guard(m_guard);

            // if not waiting task - free desriptor and wait needed task
            if (taskID != m_WaitedTask)
            {
                ((VC1TaskStoreSW*)(m_pVC1VideoDecoder->m_pStore))->FreeBusyDescriptor();
                if (UMC_LAST_FRAME == umcRes)
                    m_pVC1VideoDecoder->m_pStore->SeLastFramesMode();

                return MFX_TASK_NEED_CONTINUE;
            }
            sts =  PostProcessFrame(surface_work, surface_disp);
            if (MFX_ERR_NONE ==  sts)
            {
                m_WaitedTask++;
                return MFX_TASK_DONE;
            }
            else if (MFX_ERR_MORE_DATA ==  sts)
                return MFX_TASK_NEED_CONTINUE;
            else
                return sts;
        }
    }
    else // HW implementation
#endif // #ifdef ALLOW_SW_VC1_FALLBACK
    {
#ifdef MFX_VA
        if (IsStatusReportEnable())
        {
            sts = MFX_ERR_NONE;
            // we dont need to wait status for skipped frames
            // we just output such frames twice
            if (!m_pVC1VideoDecoder->IsFrameSkipped() && m_SubmitFrame > m_ProcessedFrames) // processed and submit surface managment
            {
                sts = GetStatusReport();
            }
#ifdef MFX_VA_WIN
            else if (m_SubmitFrame == m_ProcessedFrames && m_pStatusQueue.size())
            {
                sts = GetStatusReport();
            }
#endif

            MFX_CHECK_STS(sts);
            if (MFX_TASK_BUSY == sts)
                return sts;
        }

        UMC::AutomaticUMCMutex guard(m_guard);

        // for HW only sequence processing
        if (m_WaitedTask != taskID)
            return MFX_TASK_NEED_CONTINUE;
        sts = PostProcessFrameHW(surface_work, surface_disp);
        if (MFX_ERR_NONE ==  sts)
        {
            m_WaitedTask++;
            return MFX_TASK_DONE;
        }
        else if (MFX_ERR_MORE_DATA ==  sts)
            return MFX_TASK_NEED_CONTINUE;
        else
        {

            return sts;
        }
#endif // #ifdef MFX_VA
    }

    return MFX_ERR_ABORTED;
}

static mfxStatus VC1CompleteProc(void *, void *pParam, mfxStatus )
{
    delete (MFXVideoDECODEVC1::AsyncSurface *)pParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoDECODEVC1::DecodeFrameCheck(mfxBitstream *bs,
                                              mfxFrameSurface1 *surface_work,
                                              mfxFrameSurface1 **surface_out,
                                              MFX_ENTRY_POINT *pEntryPoint)
{
    UMC::AutomaticUMCMutex guard(m_guard);
    // To be checked

    mfxStatus mfxSts = DecodeFrameCheck(bs, surface_work, surface_out);
    if (MFX_ERR_NONE == mfxSts ||
        (mfxStatus)MFX_ERR_MORE_DATA_SUBMIT_TASK == mfxSts) // It can be useful to run threads right after first frame receive
    {
        AsyncSurface *pAsyncSurface = new AsyncSurface;
        pAsyncSurface->surface_work = GetOriginalSurface(surface_work);
        pAsyncSurface->surface_out = GetOriginalSurface(*surface_out);
        pAsyncSurface->taskID = m_CurrentTask++;
        pAsyncSurface->isFrameSkipped = false;

        pEntryPoint->pRoutine = &VC1DECODERoutine;
        pEntryPoint->pCompleteProc = &VC1CompleteProc;
        pEntryPoint->pState = this;
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
        if (MFX_HW_VAAPI == m_pCore->GetVAType() && !m_isSWPlatform)
            pEntryPoint->requiredNumThreads = 1;
        else
#endif
            pEntryPoint->requiredNumThreads = m_par.mfx.NumThread;
        pEntryPoint->pParam = pAsyncSurface;
        pEntryPoint->pRoutineName = (char *)"DecodeVC1";

        if (MFX_ERR_NONE == mfxSts)
        {
            FillMFXDataOutputSurface(*surface_out);
        }
    }
    return mfxSts;
}
mfxStatus  MFXVideoDECODEVC1::ProcessSkippedFrame()
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU16 MemType;

    mfxFrameSurface1 *origSurface;
    mfxFrameSurface1 *surface_out;
    mfxFrameSurface1 tSurface;

    if (m_isSWPlatform)
    {
        MemType = MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_EXTERNAL_FRAME;
    }
    else
    {
        MemType = MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_EXTERNAL_FRAME;
    }

    FrameMemID origMemID = m_pVC1VideoDecoder->GetSkippedIndex();
    FrameMemID displayMemID = m_pVC1VideoDecoder->GetSkippedIndex(false);
    if (origMemID > -1)
    {
        origSurface = m_pFrameAlloc->GetSurface(origMemID, &tSurface, &m_par);
    }
    else
        return sts;

    if (displayMemID > -1)
    {
        surface_out = m_pFrameAlloc->GetSurface(displayMemID, &tSurface, &m_par);
    }
    else
        return sts;

    sts =  m_pCore->DoFastCopyWrapper(surface_out, MemType, origSurface, MemType);
    return sts;
}

#ifdef MFX_VA_WIN
bool MFXVideoDECODEVC1::NeedToGetStatus(UMC::VC1FrameDescriptor *pCurrDescriptor)
{
    if (!m_pStatusQueue.size())
        return true;
    else if (1 == m_pStatusQueue.size() && pCurrDescriptor->m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace) //one field
    {
        //both fields were submitted but only one presents
        {
            if (!pCurrDescriptor->m_bIsFieldAbsent)
                return true;
        }
    }
    return false;
}
#endif

mfxStatus MFXVideoDECODEVC1::GetStatusReport()
{
#ifdef MFX_VA_WIN
    Status umcRes = UMC_OK;
    mfxI32 i = 0;

    UMC::VC1FrameDescriptor *pCurrDescriptor = m_pVC1VideoDecoder->m_pStore->GetFirstDS();

    if (!pCurrDescriptor)
        return MFX_ERR_NONE;

    // get statuses from HW for several frames only if there are no ready
    while (NeedToGetStatus(pCurrDescriptor))
    {
        //TDR handling.
        for(i=0; i< MAX_NUM_STATUS_REPORTS;i++){
            m_pStatusReport[i].bStatus = 4;
        }
        umcRes = static_cast<UMC::VC1VideoDecoderHW*>(m_pVC1VideoDecoder.get())->GetStatusReport(m_pStatusReport);
        MFX_CHECK(UMC_ERR_FAILED != umcRes, MFX_ERR_DEVICE_FAILED);
        if (UMC_WRN_INFO_NOT_READY == umcRes)
        {
            if (m_NumberOfQueries < NumOfSeqQuery)
            {
                m_NumberOfQueries++;
                return MFX_TASK_BUSY;
            }
            else //if no response from HW - lets say that this frame is broken
            {
                m_ProcessedFrames++;
                pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted = UMC::ERROR_FRAME_MAJOR;
                m_pStatusQueue.clear();
                return MFX_ERR_NONE;

            }
        }

        m_NumberOfQueries = 0;

        for (i = VC1_MAX_REPORTS - 1; i >= 0; i--)
        {
            if (m_pStatusReport[i].StatusReportFeedbackNumber == (1<<8))
            {
                // to check two fields as one frame
                if (m_pStatusReport[i].bPicStructure != 3)
                {
                    m_pStatusQueue.push_back(m_pStatusReport[i]);
                    if (!m_bIsFirstField)
                    {
                        m_ProcessedFrames++;
                        m_bIsFirstField = true;
                    }
                    else // think about statuses
                    {
                        m_bIsFirstField = false;
                        if (pCurrDescriptor->m_bIsFieldAbsent)
                            m_ProcessedFrames++;
                    }
                }
                else
                {
                    m_pStatusQueue.push_back(m_pStatusReport[i]);
                    m_ProcessedFrames++;
                }

            }
        }
        memset(m_pStatusReport, 0, sizeof(DXVA_Status_VC1)*VC1_MAX_REPORTS);
    }

    DXVA_Status_VC1 *pCurrFrameStatus = &m_pStatusQueue.front();
    {
        switch(pCurrFrameStatus->bStatus)
        {
        case 1:
            pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted = UMC::ERROR_FRAME_MINOR;
            break;
        case 2:
            pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted = UMC::ERROR_FRAME_MAJOR;
            break;
        case 3:
            pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted = UMC::ERROR_FRAME_MAJOR;
            break;
        case 4:
            pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted = UMC::ERROR_FRAME_MAJOR;
            MFX_RETURN(MFX_ERR_DEVICE_FAILED);
        default:
            break;
        }
        if (VC1_FieldInterlace == pCurrDescriptor->m_pContext->m_picLayerHeader->FCM)
        {
            if (!pCurrDescriptor->m_bIsFieldAbsent)
                m_pStatusQueue.pop_front();
            else
            {
                //no need to pop status for second field
                if (pCurrDescriptor->m_pContext->m_picLayerHeader->TFF)
                    pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted |= UMC::ERROR_FRAME_BOTTOM_FIELD_ABSENT;
                else
                    pCurrDescriptor->m_pContext->m_frmBuff.m_pFrames[pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex].corrupted |= UMC::ERROR_FRAME_TOP_FIELD_ABSENT;
            }
        }
        m_pStatusQueue.pop_front();
    }

#elif defined(MFX_VA_LINUX)
#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
    VideoAccelerator *va;
    m_pCore->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);

    UMC::VC1FrameDescriptor *pCurrDescriptor = m_pVC1VideoDecoder->m_pStore->GetFirstDS();

    if (pCurrDescriptor)
    {
        Status sts = va->SyncTask(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex);
        if (sts != UMC::UMC_OK)
        {
            mfxStatus CriticalErrorStatus = (sts == UMC::UMC_ERR_GPU_HANG) ? MFX_ERR_GPU_HANG : MFX_ERR_DEVICE_FAILED;
            SetCriticalErrorOccured(CriticalErrorStatus);
            return CriticalErrorStatus;
        }
    }
#else
    VideoAccelerator *va;
    m_pCore->GetVA((mfxHDL*)&va, MFX_MEMTYPE_FROM_DECODE);

    Status sts = UMC_OK;
    VASurfaceStatus surfSts = VASurfaceSkipped;

    UMC::VC1FrameDescriptor *pCurrDescriptor = m_pVC1VideoDecoder->m_pStore->GetFirstDS();

    if (pCurrDescriptor)
    {
        VAStatus surfErr = VA_STATUS_SUCCESS;
        sts = va->QueryTaskStatus(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex, &surfSts, &surfErr);
        MFX_CHECK(sts != UMC_ERR_GPU_HANG, MFX_ERR_GPU_HANG);
        MFX_CHECK(sts == UMC_OK, MFX_ERR_DEVICE_FAILED);
    }
#endif //#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)
#endif

    return MFX_ERR_NONE;
}

mfxFrameSurface1 *MFXVideoDECODEVC1::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_IsOpaq)
        return m_pCore->GetNativeSurface(surface);
    return surface;
}

bool MFXVideoDECODEVC1::IsStatusReportEnable()
{
    return m_bStsReport;
}

bool MFXVideoDECODEVC1::FrameStartCodePresence()
{
    if (m_FCInfo.stCodes->values[0] == 0x0D010000 && !m_bPTSTaken)
    {
        m_bPTSTaken = true;
        return true;
    }

    return false;
}

mfxStatus __CDECL VC1DECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 )
{
    volatile mfxStatus TaskSts;
    MFXVideoDECODEVC1 *pMFXVC1Decoder = (MFXVideoDECODEVC1 *)pState;
    MFXVideoDECODEVC1::AsyncSurface *pAsyncSurface = (MFXVideoDECODEVC1::AsyncSurface *)pParam;

    TaskSts = pMFXVC1Decoder->RunThread(pAsyncSurface->surface_work,
                                        pAsyncSurface->surface_out,
                                        threadNumber,
                                        pAsyncSurface->taskID);
    return TaskSts;
}
#endif
