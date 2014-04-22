/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <algorithm>
#include "umc_h264_mfx_supplier.h"

#ifndef UMC_RESTRICTED_CODE_MFX

#include "umc_h264_frame_list.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_bitstream.h"

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_dec_mfx.h"

#include "vm_sys_info.h"

#include "umc_h264_dec_debug.h"
#include "mfxpcp.h"

#if defined (MFX_VA)
#include "umc_va_dxva2.h"
#endif

#include "mfx_enc_common.h"

namespace UMC
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
RawHeader::RawHeader()
{
    Reset();
}

void RawHeader::Reset()
{
    m_id = -1;
    m_buffer.clear();
#ifdef __APPLE__    
    m_rbspSize = 0;
#endif 
}

Ipp32s RawHeader::GetID() const
{
    return m_id;
}

size_t RawHeader::GetSize() const
{
    return m_buffer.size();
}

Ipp8u * RawHeader::GetPointer()
{
    return &m_buffer[0];
}

void RawHeader::Resize(Ipp32s id, size_t newSize)
{
    m_id = id;
    m_buffer.resize(newSize);
}
    
#ifdef __APPLE__    
size_t RawHeader::GetRBSPSize()
{
    return m_rbspSize;
}
    
void RawHeader::SetRBSPSize(size_t rbspSize)
{
    m_rbspSize = rbspSize;
}    
#endif 

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RawHeaders::Reset()
{
    m_sps.Reset();
    m_pps.Reset();
}

RawHeader * RawHeaders::GetSPS()
{
    return &m_sps;
}

RawHeader * RawHeaders::GetPPS()
{
    return &m_pps;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////
MFXTaskSupplier::MFXTaskSupplier()
    : TaskSupplier()
{
    memset(&m_firstVideoParams, 0, sizeof(mfxVideoParam));
}

MFXTaskSupplier::~MFXTaskSupplier()
{
    Close();
}

Status MFXTaskSupplier::Init(BaseCodecParams *pInit)
{
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams> (pInit);

    if (NULL == init)
        return UMC_ERR_NULL_PTR;

    Close();

    m_initializationParams = *init;
    m_pMemoryAllocator = init->lpMemoryAllocator;
    m_DPBSizeEx = 0;

    m_TrickModeSpeed = 1;

    m_sei_messages = new SEI_Storer();
    m_sei_messages->Init();

    Ipp32s nAllowedThreadNumber = init->numThreads;
    if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;

    // calculate number of slice decoders.
    // It should be equal to CPU number
    m_iThreadNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);

    Status umcRes = SVC_Extension::Init();
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    switch (m_initializationParams.info.profile) // after MVC_Extension::Init()
    {
    case 0:
        m_decodingMode = UNKNOWN_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
    case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:
        m_decodingMode = MVC_DECODING_MODE;
        break;
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
    case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
        m_decodingMode = SVC_DECODING_MODE;
        AllocateView(0);
        break;
    default:
        m_decodingMode = AVC_DECODING_MODE;
        break;
    }

    AU_Splitter::Init(init);
    DPBOutput::Reset(m_iThreadNum != 1);

    switch(m_iThreadNum)
    {
    case 1:
        m_pTaskBroker = new TaskBrokerSingleThread(this);
        break;
    case 4:
    case 3:
    case 2:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    default:
        m_pTaskBroker = new TaskBrokerTwoThread(this);
        break;
    }

    m_pTaskBroker->Init(m_iThreadNum, false);

    // create slice decoder(s)
    m_pSegmentDecoder = new H264SegmentDecoderMultiThreaded *[m_iThreadNum];
    if (NULL == m_pSegmentDecoder)
        return UMC_ERR_ALLOC;
    memset(m_pSegmentDecoder, 0, sizeof(H264SegmentDecoderMultiThreaded *) * m_iThreadNum);

    Ipp32u i;
    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264SegmentDecoderMultiThreaded(m_pTaskBroker);

        if (NULL == m_pSegmentDecoder[i])
            return UMC_ERR_ALLOC;
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC_ERR_INIT;
    }

    m_local_delta_frame_time = 1.0/30;
    m_frameOrder             = 0;
    m_use_external_framerate = 0 < init->info.framerate;

    if (m_use_external_framerate)
    {
        m_local_delta_frame_time = 1 / init->info.framerate;
    }

    H264VideoDecoderParams *initH264 = DynamicCast<H264VideoDecoderParams> (pInit);
    m_DPBSizeEx = m_iThreadNum + (initH264 ? initH264->m_bufferedFrames : 0);
    return UMC_OK;
}

void MFXTaskSupplier::Reset()
{
    TaskSupplier::Reset();

    m_Headers.Reset();

    if (m_pTaskBroker)
        m_pTaskBroker->Init(m_iThreadNum, false);
}

H264DecoderFrame * MFXTaskSupplier::GetFreeFrame(const H264Slice *pSlice)
{
    AutomaticUMCMutex guard(m_mGuard);
    Ipp32u view_id = pSlice ? pSlice->GetSliceHeader()->nal_ext.mvc.view_id : 0;
    ViewItem &view = GetView(view_id);
    H264DecoderFrame *pFrame = 0;

    H264DBPList *pDPB = view.GetDPBList(0);

    // Traverse list for next disposable frame
    //if (m_pDecodedFramesList->countAllFrames() >= m_dpbSize + m_DPBSizeEx)
    pFrame = pDPB->GetDisposable();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (pDPB->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        pDPB->append(pFrame);
    }

    DecReferencePictureMarking::Remove(pFrame);
    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);
    pFrame->IncrementReference();

    if (GetAuxiliaryFrame(pFrame))
    {
        GetAuxiliaryFrame(pFrame)->Reset();
    }

    if (view.pCurFrame == pFrame)
        view.pCurFrame = 0;

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;

    return pFrame;
}

Status MFXTaskSupplier::RunDecoding(bool should_additional_check, H264DecoderFrame ** decoded)
{
    H264DecoderFrame * outputFrame = *decoded;
    ViewItem &view = GetView(outputFrame->m_viewId);

    Status umcRes = UMC_OK;

    if (!outputFrame->IsDecodingStarted())
        return UMC_ERR_FAILED;

    if (outputFrame->IsDecodingCompleted())
        return UMC_OK;

    for(;;)
    {
        m_pTaskBroker->WaitFrameCompletion();

        if (outputFrame->IsDecodingCompleted())
        {
            if (!should_additional_check)
                break;

            AutomaticUMCMutex guard(m_mGuard);

            Ipp32s count = 0;
            Ipp32s notDecoded = 0;
            for (H264DecoderFrame * pTmp = view.GetDPBList(0)->head(); pTmp; pTmp = pTmp->future())
            {
                if (!pTmp->m_isShortTermRef[0] &&
                    !pTmp->m_isShortTermRef[1] &&
                    !pTmp->m_isLongTermRef[0] &&
                    !pTmp->m_isLongTermRef[1] &&
                    ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)))
                {
                    count++;
                    break;
                }

                if (!pTmp->IsDecoded() && !pTmp->IsDecodingCompleted() && pTmp->IsFullFrame())
                    notDecoded++;
            }

            if (count || (view.GetDPBList(0)->countAllFrames() < view.dpbSize + m_DPBSizeEx))
                break;

            if (!notDecoded)
                break;
        }
    }

    VM_ASSERT(outputFrame->IsDecodingCompleted());

    return umcRes;
}

bool MFXTaskSupplier::CheckDecoding(bool should_additional_check, H264DecoderFrame * outputFrame)
{
    ViewItem &view = GetView(outputFrame->m_viewId);

    if (!outputFrame->IsDecodingStarted())
        return false;

    if (!outputFrame->IsDecodingCompleted())
        return false;

    if (!should_additional_check)
        return true;

    AutomaticUMCMutex guard(m_mGuard);

    Ipp32s count = 0;
    Ipp32s notDecoded = 0;
    for (H264DecoderFrame * pTmp = view.GetDPBList(0)->head(); pTmp; pTmp = pTmp->future())
    {
        if (!pTmp->m_isShortTermRef[0] &&
            !pTmp->m_isShortTermRef[1] &&
            !pTmp->m_isLongTermRef[0] &&
            !pTmp->m_isLongTermRef[1] &&
            ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)))
        {
            count++;
            break;
        }

        if (!pTmp->IsDecoded() && !pTmp->IsDecodingCompleted() && pTmp->IsFullFrame())
            notDecoded++;
    }

    if (count || (view.GetDPBList(0)->countAllFrames() < view.dpbSize + m_DPBSizeEx))
        return true;

    if (!notDecoded)
        return true;

    return false;
}

mfxStatus MFXTaskSupplier::RunThread(mfxU32 threadNumber)
{
    Status sts = m_pSegmentDecoder[threadNumber]->ProcessSegment();
    if (sts == UMC_ERR_NOT_ENOUGH_DATA)
        return MFX_TASK_BUSY;
#if defined (MFX_VA)
    else if(sts == UMC_ERR_DEVICE_FAILED)
        return MFX_ERR_DEVICE_FAILED;
#endif

    if (sts != UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_TASK_WORKING;
}

bool MFXTaskSupplier::ProcessNonPairedField(H264DecoderFrame * pFrame)
{
    if (pFrame && pFrame->GetAU(1)->GetStatus() == H264DecoderFrameInfo::STATUS_NOT_FILLED)
    {
        pFrame->setPicOrderCnt(pFrame->PicOrderCnt(0, 1), 1);
        pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_NONE);
        pFrame->m_bottom_field_flag[1] = !pFrame->m_bottom_field_flag[0];

        H264Slice * pSlice = pFrame->GetAU(0)->GetSlice(0);
        pFrame->setPicNum(pSlice->GetSliceHeader()->frame_num*2 + 1, 1);

        Ipp32s isBottom = pSlice->IsBottomField() ? 0 : 1;
        pFrame->DefaultFill(isBottom, false);

        pFrame->SetErrorFlagged(isBottom ? ERROR_FRAME_BOTTOM_FIELD_ABSENT : ERROR_FRAME_TOP_FIELD_ABSENT);
        return true;
    }

    return false;
}

Status MFXTaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    if (!pFrame)
        return UMC_OK;

    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);
    if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return UMC_OK;

    return TaskSupplier::CompleteFrame(pFrame, field);
}

void MFXTaskSupplier::SetVideoParams(mfxVideoParam * par)
{
    m_firstVideoParams = *par;
}

Status MFXTaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    Status sts = TaskSupplier::DecodeHeaders(nalUnit);
    if (sts != UMC_OK)
        return sts;

    H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

    if (currSPS)
    {
        if (currSPS->bit_depth_luma > 8 || currSPS->bit_depth_chroma > 8 || currSPS->chroma_format_idc > 1)
            throw h264_exception(UMC_ERR_UNSUPPORTED);

        switch (currSPS->profile_idc)
        {
        case H264VideoDecoderParams::H264_PROFILE_UNKNOWN:
        case H264VideoDecoderParams::H264_PROFILE_BASELINE:
        case H264VideoDecoderParams::H264_PROFILE_MAIN:
        case H264VideoDecoderParams::H264_PROFILE_EXTENDED:
        case H264VideoDecoderParams::H264_PROFILE_HIGH:

        case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
        case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:

        case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
        case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
            break;
        default:
            throw h264_exception(UMC_ERR_UNSUPPORTED);
        }

        DPBOutput::OnNewSps(currSPS);
    }

    {
        // save sps/pps
        Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
        switch(nal_unit_type)
        {
            case NAL_UT_SPS:
            case NAL_UT_PPS:
                {
                    static Ipp8u start_code_prefix[] = {0, 0, 0, 1};

                    size_t size = nalUnit->GetDataSize();
                    bool isSPS = (nal_unit_type == NAL_UT_SPS);
                    RawHeader * hdr = isSPS ? GetSPS() : GetPPS();
                    Ipp32s id = isSPS ? m_Headers.m_SeqParams.GetCurrentID() : m_Headers.m_PicParams.GetCurrentID();
                    hdr->Resize(id, size + sizeof(start_code_prefix));
                    MFX_INTERNAL_CPY(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
                    MFX_INTERNAL_CPY(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), (Ipp32u)size);
#ifdef __APPLE__                    
                    hdr->SetRBSPSize(size);
#endif  
                 }
            break;
        }
    }

    MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    if ((NAL_Unit_Type)pMediaDataEx->values[0] == NAL_UT_SPS && m_firstVideoParams.mfx.FrameInfo.Width)
    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width < (currSPS->frame_width_in_mbs * 16) ||
                m_firstVideoParams.mfx.FrameInfo.Height < (currSPS->frame_height_in_mbs * 16) ||
                (currSPS->level_idc && m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->level_idc))
            {
                return UMC_NTF_NEW_RESOLUTION;
            }
        }

        return UMC_WRN_REPOSITION_INPROGRESS;
    }

    return UMC_OK;
}

Status MFXTaskSupplier::DecodeSEI(MediaDataEx *nalUnit)
{
    H264Bitstream bitStream;

    try
    {
        H264MemoryPiece mem;
        mem.SetData(nalUnit);

        H264MemoryPiece swappedMem;
        swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        memset(swappedMem.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

        SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        swapper->SwapMemory(&swappedMem, &mem);

        bitStream.Reset((Ipp8u*)swappedMem.GetPointer(), (Ipp32u)swappedMem.GetDataSize());

        NAL_Unit_Type nal_unit_type;
        Ipp32u nal_ref_idc;

        bitStream.GetNALUnitType(nal_unit_type, nal_ref_idc);

        do
        {
            H264SEIPayLoad    m_SEIPayLoads;

            size_t decoded1 = bitStream.BytesDecoded();

            bitStream.ParseSEI(m_Headers, &m_SEIPayLoads);
            
            if (m_SEIPayLoads.payLoadType == SEI_PIC_TIMING_TYPE)
            {
                DEBUG_PRINT((VM_STRING("debug headers SEI - %d, picstruct - %d\n"), m_SEIPayLoads.payLoadType, m_SEIPayLoads.SEI_messages.pic_timing.pic_struct));
            }
            else
            {
                DEBUG_PRINT((VM_STRING("debug headers SEI - %d\n"), m_SEIPayLoads.payLoadType));
            }

            if (m_SEIPayLoads.payLoadType == SEI_USER_DATA_REGISTERED_TYPE)
            {
                m_UserData = m_SEIPayLoads;
            }
            else
            {
                if (m_SEIPayLoads.payLoadType == SEI_RESERVED)
                    continue;

                H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }

            size_t decoded2 = bitStream.BytesDecoded();

            // calculate payload size
            size_t size = decoded2 - decoded1;

            VM_ASSERT(size == m_SEIPayLoads.payLoadSize + 2 + (m_SEIPayLoads.payLoadSize / 255) + (m_SEIPayLoads.payLoadType / 255));

            MediaDataEx nalUnit1;

            nalUnit1.SetBufferPointer((Ipp8u*)nalUnit->GetDataPointer(), decoded1 + size);
            nalUnit1.SetDataSize(decoded1 + size);
            nalUnit1.MoveDataPointer((Ipp32s)decoded1);

            if (m_sei_messages)
                m_sei_messages->AddMessage(&nalUnit1, m_SEIPayLoads.payLoadType, -1);

        } while (bitStream.More_RBSP_Data());

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC_OK;
}

void MFXTaskSupplier::AddFakeReferenceFrame(H264Slice * )
{
}

} // namespace UMC

bool IsBaselineConstraints(mfxExtSVCSeqDesc * svcDesc)
{
    bool isBaselineConstraints = true;
    for (Ipp32u i = 0; i < sizeof(svcDesc->DependencyLayer)/sizeof(svcDesc->DependencyLayer[0]); i++)
    {
        if (svcDesc->DependencyLayer[i].Active)
        {
            if ((svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[0] & 0xf) || (svcDesc->DependencyLayer[i].ScaledRefLayerOffsets[1] & 0xf))
            {
                isBaselineConstraints = false;
                break;
            }

            Ipp32u depD = svcDesc->DependencyLayer[i].RefLayerDid;
            if (depD >= 8)
                continue;

            if ( ! ((svcDesc->DependencyLayer[i].Width == svcDesc->DependencyLayer[depD].Width &&
                svcDesc->DependencyLayer[i].Height == svcDesc->DependencyLayer[depD].Height) ||

                (svcDesc->DependencyLayer[i].Width == 2*svcDesc->DependencyLayer[depD].Width &&
                svcDesc->DependencyLayer[i].Height == 2*svcDesc->DependencyLayer[depD].Height) ||

                (svcDesc->DependencyLayer[i].Width == (2*svcDesc->DependencyLayer[depD].Width)/3 &&
                svcDesc->DependencyLayer[i].Height == (2*svcDesc->DependencyLayer[depD].Height)/3) ))
            {
                isBaselineConstraints = false;
                break;
            }
        }
    }

    return isBaselineConstraints;
}

bool MFX_Utility::IsNeedPartialAcceleration(mfxVideoParam * par, eMFXHWType )
{
    if (!par)
        return false;

    if (par->mfx.SliceGroupsPresent) // Is FMO
        return true;

    /*mfxExtSVCSeqDesc * svcDesc = (mfxExtSVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
    if (svcDesc)
    {
        if (!IsBaselineConstraints(svcDesc))
            return true;
    }*/

    mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
    if (points && points->NumRefsTotal > 16)
        return true;

    return false;
}

eMFXPlatform MFX_Utility::GetPlatform(VideoCORE * core, mfxVideoParam * par)
{
    eMFXPlatform platform = core->GetPlatformType();

    if (!par)
        return platform;

    eMFXHWType typeHW = MFX_HW_UNKNOWN;
#if defined (MFX_VA)
    typeHW = core->GetHWType();
#endif

    if (par && IsNeedPartialAcceleration(par, typeHW) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

#if defined (MFX_VA)
    GUID name;

    switch (typeHW)
    {
    case MFX_HW_LAKE:
        name = sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT;
        break;
    case MFX_HW_HSW_ULT:
    case MFX_HW_HSW:
    case MFX_HW_BDW:
    case MFX_HW_SCL:
        if (isMVCProfile(par->mfx.CodecProfile))
        {
            mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
            if (par->mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH && points && points->NumView > 2)
                name = sDXVA_ModeH264_VLD_Multiview_NoFGT;
            else
                name = par->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? sDXVA_ModeH264_VLD_Stereo_Progressive_NoFGT : sDXVA_ModeH264_VLD_Stereo_NoFGT;
        }
        else if (isSVCProfile(par->mfx.CodecProfile))
        {
            if (par->mfx.CodecProfile == MFX_PROFILE_AVC_SCALABLE_HIGH)
                name = sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_High;
            else
                name = sDXVA_ModeH264_VLD_SVC_Scalable_Constrained_Baseline;
        }
        else
            name = sDXVA2_ModeH264_VLD_NoFGT;

        break;
    default:
        name = sDXVA2_ModeH264_VLD_NoFGT;
        break;
    }

    if (MFX_ERR_NONE != core->IsGuidSupported(name, par) &&
        platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }
#endif

    return platform;
}

UMC::Status MFX_Utility::FillVideoParam(UMC::TaskSupplier * supplier, mfxVideoParam *par, bool full)
{
    const UMC_H264_DECODER::H264SeqParamSet * seq = supplier->GetHeaders()->m_SeqParams.GetCurrentHeader();
    if (!seq)
        return UMC::UMC_ERR_FAILED;

    if (UMC::FillVideoParam(seq, par, full) != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    const UMC_H264_DECODER::H264PicParamSet * pps = supplier->GetHeaders()->m_PicParams.GetCurrentHeader();
    if (pps)
        par->mfx.SliceGroupsPresent = pps->num_slice_groups > 1;

    return UMC::UMC_OK;
}

static inline bool IsFieldOfOneFrame(UMC::H264Slice * pSlice1, UMC::H264Slice *pSlice2)
{
    if (!pSlice2 || !pSlice1)
        return true;

    if ((pSlice1->GetSliceHeader()->nal_ref_idc && !pSlice2->GetSliceHeader()->nal_ref_idc) ||
        (!pSlice1->GetSliceHeader()->nal_ref_idc && pSlice2->GetSliceHeader()->nal_ref_idc))
        return false;

    if (pSlice1->GetSliceHeader()->field_pic_flag != pSlice2->GetSliceHeader()->field_pic_flag)
        return false;

    if (pSlice1->GetSliceHeader()->bottom_field_flag == pSlice2->GetSliceHeader()->bottom_field_flag)
        return false;

    return true;
}

UMC::Status FillVideoParamSVCBySliceInfo(mfxExtSVCSeqDesc * seqDesc, const UMC::H264Slice * slice)
{
    if (!slice)
        return UMC::UMC_OK;

    const UMC_H264_DECODER::H264SliceHeader * sliceHeader = slice->GetSliceHeader();
    if (!sliceHeader->nal_ext.extension_present || !sliceHeader->nal_ext.svc_extension_flag)
        return UMC::UMC_OK;

    const UMC_H264_DECODER::H264SeqParamSet * seq = slice->GetSeqParam();
    Ipp32u currentDLayer = sliceHeader->nal_ext.svc.dependency_id;
    if (!seqDesc->DependencyLayer[currentDLayer].Active)
    {
        seqDesc->DependencyLayer[currentDLayer].Width = (mfxU16) (seq->frame_width_in_mbs * 16);
        seqDesc->DependencyLayer[currentDLayer].Height = (mfxU16) (seq->frame_height_in_mbs * 16);

        seqDesc->DependencyLayer[currentDLayer].CropX = (mfxU16) (0);
        seqDesc->DependencyLayer[currentDLayer].CropY = (mfxU16) (0);

        seqDesc->DependencyLayer[currentDLayer].CropW = (mfxU16) (seqDesc->DependencyLayer[currentDLayer].Width);
        seqDesc->DependencyLayer[currentDLayer].CropH = (mfxU16) (seqDesc->DependencyLayer[currentDLayer].Height);

        seqDesc->DependencyLayer[currentDLayer].Active = 1;
    }

    Ipp32u currentQLayer = sliceHeader->nal_ext.svc.quality_id;
    if ((currentQLayer + 1) > seqDesc->DependencyLayer[currentDLayer].QualityNum)
    {
        seqDesc->DependencyLayer[currentDLayer].QualityNum = (mfxU16)(currentQLayer + 1);
    }

    Ipp32u currentTLayer = sliceHeader->nal_ext.svc.temporal_id;
    bool isFound = false;
    for (Ipp32u k = 0; k < seqDesc->DependencyLayer[currentDLayer].TemporalNum; k++)
    {
        if (seqDesc->DependencyLayer[currentDLayer].TemporalId[k] == currentTLayer)
        {
            isFound = true;
            break;
        }
    }

    if (!isFound && seqDesc->DependencyLayer[currentDLayer].TemporalNum <= sizeof(seqDesc->DependencyLayer[currentDLayer].TemporalId)/sizeof(seqDesc->DependencyLayer[currentDLayer].TemporalId[0]))
    {
        seqDesc->DependencyLayer[currentDLayer].TemporalId[seqDesc->DependencyLayer[currentDLayer].TemporalNum++] = (mfxU16)currentTLayer;
    }

    if (!seqDesc->DependencyLayer[currentDLayer].RefLayerDid && slice->GetSliceHeader()->ref_layer_dq_id != -1)
    {
        seqDesc->DependencyLayer[currentDLayer].RefLayerDid = (mfxU16)(slice->GetSliceHeader()->ref_layer_dq_id >> 4);
        seqDesc->DependencyLayer[currentDLayer].RefLayerQid = (mfxU16)(slice->GetSliceHeader()->ref_layer_dq_id & 0x0f);

        const H264SeqParamSetSVCExtension *curSps = slice->GetSeqSVCParam();

        Ipp32s leftOffset = 0;
        Ipp32s topOffset = 0;
        Ipp32s rightOffset = 0;
        Ipp32s bottomOffset = 0;

        if (sliceHeader->nal_ext.svc.quality_id || sliceHeader->nal_ext.svc.no_inter_layer_pred_flag)
        {
        }
        else
        {
            if (curSps->extended_spatial_scalability == 1) {
                leftOffset = 2 * curSps->seq_scaled_ref_layer_left_offset;
                topOffset = 2 * curSps->seq_scaled_ref_layer_top_offset *
                    (2 - curSps->frame_mbs_only_flag);

                rightOffset = 2 * curSps->seq_scaled_ref_layer_right_offset;
                bottomOffset = 2 * curSps->seq_scaled_ref_layer_bottom_offset *
                    (2 - curSps->frame_mbs_only_flag);

            } else if (curSps->extended_spatial_scalability == 2) {
                leftOffset = 2 * sliceHeader->scaled_ref_layer_left_offset;
                topOffset = 2 * sliceHeader->scaled_ref_layer_top_offset *
                    (2 - curSps->frame_mbs_only_flag);

                rightOffset = 2 * sliceHeader->scaled_ref_layer_right_offset;
                bottomOffset = 2 * sliceHeader->scaled_ref_layer_bottom_offset *
                    (2 - curSps->frame_mbs_only_flag);
            }
        }

        seqDesc->DependencyLayer[currentDLayer].ScaledRefLayerOffsets[0] = (mfxI16)leftOffset;
        seqDesc->DependencyLayer[currentDLayer].ScaledRefLayerOffsets[1] = (mfxI16)topOffset;
        seqDesc->DependencyLayer[currentDLayer].ScaledRefLayerOffsets[2] = (mfxI16)rightOffset;
        seqDesc->DependencyLayer[currentDLayer].ScaledRefLayerOffsets[3] = (mfxI16)bottomOffset;
    }

    return UMC::UMC_OK;
}

class PosibleMVC
{
public:
    PosibleMVC(UMC::TaskSupplier * supplier);
    virtual ~PosibleMVC();

    virtual UMC::Status DecodeHeader(UMC::MediaData* params, mfxBitstream *bs, mfxVideoParam *out);
    virtual UMC::Status ProcessNalUnit(UMC::MediaData * data);
    virtual bool IsEnough() const;

    mfxExtSVCSeqDesc  * GetSVCSeqDesc();

protected:
    bool m_isSPSFound;
    bool m_isPPSFound;
    bool m_isSubSPSFound;

    bool m_isFrameLooked;

    bool m_isSVC_SEIFound;

    bool m_isMVCBuffer;
    bool m_isSVCBuffer;

    bool IsAVCEnough() const;
    bool isMVCRequered() const;

    mfxExtSVCSeqDesc m_svcSeqDesc;
    UMC::TaskSupplier * m_supplier;
    UMC::H264Slice * m_lastSlice;
};

PosibleMVC::PosibleMVC(UMC::TaskSupplier * supplier)
    : m_isSPSFound(false)
    , m_isPPSFound(false)
    , m_isSubSPSFound(false)
    , m_isFrameLooked(false)
    , m_isSVC_SEIFound(false)
    , m_supplier(supplier)
    , m_lastSlice(0)
{
}

PosibleMVC::~PosibleMVC()
{
    if (m_lastSlice)
        m_lastSlice->DecrementReference();
}

mfxExtSVCSeqDesc* PosibleMVC::GetSVCSeqDesc()
{
    return &m_svcSeqDesc;
}

bool PosibleMVC::IsEnough() const
{
    return m_isSPSFound && m_isPPSFound && m_isSubSPSFound && m_isFrameLooked;
}

bool PosibleMVC::IsAVCEnough() const
{
    return m_isSPSFound && m_isPPSFound;
}

bool PosibleMVC::isMVCRequered() const
{
    return m_isMVCBuffer || m_isSVCBuffer;
}

UMC::Status PosibleMVC::DecodeHeader(UMC::MediaData * data, mfxBitstream *bs, mfxVideoParam *out)
{
    if (!data)
        return UMC::UMC_ERR_NULL_PTR;

    m_isMVCBuffer = GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC) != 0;
    m_isSVCBuffer = GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC) != 0;

    memset(&m_svcSeqDesc, 0, sizeof(m_svcSeqDesc));

    if (isMVCRequered())
    {
        m_isFrameLooked = !m_isSVCBuffer;
    }
    else
    {
        m_isSubSPSFound = true; // do not need to find it
    }

    m_lastSlice = 0;

    UMC::Status umcRes = UMC::UMC_ERR_NOT_ENOUGH_DATA;
    for ( ; data->GetDataSize() > 3; )
    {
        m_supplier->GetNalUnitSplitter()->MoveToStartCode(data); // move data pointer to start code
        if (!m_isSPSFound && !m_isSVC_SEIFound) // move point to first start code
        {
            bs->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
            bs->DataLength = (mfxU32)data->GetDataSize();
        }

        umcRes = ProcessNalUnit(data);

        if (umcRes == UMC::UMC_ERR_UNSUPPORTED)
            umcRes = UMC::UMC_OK;

        if (umcRes != UMC::UMC_OK)
            break;

        if (IsEnough())
            return UMC::UMC_OK;
    }

    if (umcRes == UMC::UMC_ERR_SYNC) // move pointer
    {
        bs->DataOffset = (mfxU32)((mfxU8*)data->GetDataPointer() - (mfxU8*)data->GetBufferPointer());
        bs->DataLength = (mfxU32)data->GetDataSize();
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;
    }

    if (!m_isFrameLooked && ((data->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME) == 0))
        m_isFrameLooked = true;

    if (umcRes == UMC::UMC_ERR_NOT_ENOUGH_DATA)
    {
        bool isEOS = ((data->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_END_OF_STREAM) != 0) ||
            ((data->GetFlags() & UMC::MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME) == 0);
        if (isEOS)
        {
            return UMC::UMC_OK;
        }
    }

    if (!isMVCRequered())
    {
        m_isSubSPSFound = true;
        m_isFrameLooked = true;
    }

    if (IsEnough())
        return UMC::UMC_OK;

    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
}

UMC::Status PosibleMVC::ProcessNalUnit(UMC::MediaData * data)
{
    try
    {
        Ipp32s startCode = m_supplier->GetNalUnitSplitter()->CheckNalUnitType(data);

        bool needProcess = false;

        UMC::MediaDataEx *nalUnit = m_supplier->GetNalUnit(data);

        switch ((UMC::NAL_Unit_Type)startCode)
        {
        case UMC::NAL_UT_IDR_SLICE:
        case UMC::NAL_UT_SLICE:
        case UMC::NAL_UT_AUXILIARY:
        case UMC::NAL_UT_CODED_SLICE_EXTENSION:
            {
                if (IsAVCEnough())
                {
                    if (!isMVCRequered())
                    {
                        m_isFrameLooked = true;
                        return UMC::UMC_OK;
                    }
                }
                else
                    break; // skip nal unit

                if (!nalUnit)
                {
                    return UMC::UMC_ERR_NOT_ENOUGH_DATA;
                }

                Ipp32s sps_id = m_supplier->GetHeaders()->m_SeqParams.GetCurrentID();
                Ipp32s sps_mvc_id = m_supplier->GetHeaders()->m_SeqParamsMvcExt.GetCurrentID();
                Ipp32s sps_svc_id = m_supplier->GetHeaders()->m_SeqParamsSvcExt.GetCurrentID();
                Ipp32s sps_pps_id = m_supplier->GetHeaders()->m_PicParams.GetCurrentID();

                UMC::H264Slice * pSlice = m_supplier->DecodeSliceHeader(nalUnit);
                if (pSlice)
                {
                    if (!m_lastSlice)
                    {
                        FillVideoParamSVCBySliceInfo(&m_svcSeqDesc, pSlice);
                        m_lastSlice = pSlice;
                        m_lastSlice->Release();
                        return UMC::UMC_OK;
                    }

                    if ((false == IsPictureTheSame(m_lastSlice, pSlice)))
                    {
                        if (!IsFieldOfOneFrame(pSlice, m_lastSlice))
                        {
                            m_supplier->GetHeaders()->m_SeqParams.SetCurrentID(sps_id);
                            m_supplier->GetHeaders()->m_SeqParamsMvcExt.SetCurrentID(sps_mvc_id);
                            m_supplier->GetHeaders()->m_SeqParamsSvcExt.SetCurrentID(sps_svc_id);
                            m_supplier->GetHeaders()->m_PicParams.SetCurrentID(sps_pps_id);

                            pSlice->Release();
                            pSlice->DecrementReference();
                            m_isFrameLooked = true;
                            return UMC::UMC_OK;
                        }
                    }

                    FillVideoParamSVCBySliceInfo(&m_svcSeqDesc, pSlice);

                    pSlice->Release();
                    m_lastSlice->DecrementReference();
                    m_lastSlice = pSlice;
                    return UMC::UMC_OK;
                }
            }
            break;

        case UMC::NAL_UT_SPS:
            needProcess = true;
            break;

        case UMC::NAL_UT_PPS:
            needProcess = true;
            break;

        case UMC::NAL_UT_SUBSET_SPS:
            needProcess = true;
            break;

        case UMC::NAL_UT_AUD:
            //if (header_was_started)
                //quite = true;
            break;

        case UMC::NAL_UT_SEI:  // SVC scalable SEI can be before SPS/PPS
        case UMC::NAL_UT_PREFIX:
            needProcess = true;
            break;

        case UMC::NAL_UT_UNSPECIFIED:
        case UMC::NAL_UT_DPA:
        case UMC::NAL_UT_DPB:
        case UMC::NAL_UT_DPC:
        case UMC::NAL_UT_END_OF_SEQ:
        case UMC::NAL_UT_END_OF_STREAM:
        case UMC::NAL_UT_FD:
        case UMC::NAL_UT_SPS_EX:
        default:
            break;
        };

        if (!nalUnit)
        {
            return UMC::UMC_ERR_NOT_ENOUGH_DATA;
        }

        if (needProcess)
        {
            try
            {
                UMC::Status umcRes = m_supplier->ProcessNalUnit(nalUnit);
                if (umcRes < UMC::UMC_OK)
                {
                    return UMC::UMC_OK;
                }
            }
            catch(UMC::h264_exception ex)
            {
                if (ex.GetStatus() != UMC::UMC_ERR_UNSUPPORTED)
                {
                    throw;
                }
            }

            switch ((UMC::NAL_Unit_Type)startCode)
            {
            case UMC::NAL_UT_SPS:
                m_isSPSFound = true;
                break;

            case UMC::NAL_UT_PPS:
                m_isPPSFound = true;
                break;

            case UMC::NAL_UT_SUBSET_SPS:
                m_isSubSPSFound = true;
                break;

            case UMC::NAL_UT_SEI:  // SVC scalable SEI can be before SPS/PPS
            case UMC::NAL_UT_PREFIX:
                if (!m_isSVC_SEIFound)
                {
                    const UMC_H264_DECODER::H264SEIPayLoad * svcPayload = m_supplier->GetHeaders()->m_SEIParams.GetHeader(UMC::SEI_SCALABILITY_INFO);
                    m_isSVC_SEIFound = svcPayload != 0;
                }
                break;

            case UMC::NAL_UT_UNSPECIFIED:
            case UMC::NAL_UT_SLICE:
            case UMC::NAL_UT_IDR_SLICE:
            case UMC::NAL_UT_AUD:
            case UMC::NAL_UT_DPA:
            case UMC::NAL_UT_DPB:
            case UMC::NAL_UT_DPC:
            case UMC::NAL_UT_END_OF_SEQ:
            case UMC::NAL_UT_END_OF_STREAM:
            case UMC::NAL_UT_FD:
            case UMC::NAL_UT_SPS_EX:
            case UMC::NAL_UT_AUXILIARY:
            case UMC::NAL_UT_CODED_SLICE_EXTENSION:
            default:
                break;
            };

            return UMC::UMC_OK;
        }
    }
    catch(const UMC::h264_exception & ex)
    {
        return ex.GetStatus();
    }

    return UMC::UMC_OK;
}

UMC::Status MFX_Utility::DecodeHeader(UMC::TaskSupplier * supplier, UMC::H264VideoDecoderParams* lpInfo, mfxBitstream *bs, mfxVideoParam *out)
{
    UMC::Status umcRes = UMC::UMC_OK;

    if (!lpInfo->m_pData)
        return UMC::UMC_ERR_NULL_PTR;

    if (!lpInfo->m_pData->GetDataSize())
        return UMC::UMC_ERR_NOT_ENOUGH_DATA;

    umcRes = supplier->PreInit(lpInfo);
    if (umcRes != UMC::UMC_OK)
        return UMC::UMC_ERR_FAILED;

    PosibleMVC headersDecoder(supplier);
    umcRes = headersDecoder.DecodeHeader(lpInfo->m_pData, bs, out);

    if (umcRes != UMC::UMC_OK)
        return umcRes;

    umcRes = supplier->GetInfo(lpInfo);
    if (umcRes == UMC::UMC_OK)
    {
        mfxExtSVCSeqDesc *svcSeqDescInput = (mfxExtSVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
        if (svcSeqDescInput)
        {
            MFX_INTERNAL_CPY(svcSeqDescInput->DependencyLayer, &headersDecoder.GetSVCSeqDesc()->DependencyLayer, sizeof(headersDecoder.GetSVCSeqDesc()->DependencyLayer));
            MFX_INTERNAL_CPY(svcSeqDescInput->TemporalScale, &headersDecoder.GetSVCSeqDesc()->TemporalScale, sizeof(headersDecoder.GetSVCSeqDesc()->TemporalScale));
        }

        FillVideoParam(supplier, out, false);
    }
    else
    {
    }

    return umcRes;
}

UMC::Status FillVideoParamSVC(mfxExtSVCSeqDesc *seqDesc, const UMC_H264_DECODER::H264SEIPayLoad * svcPayload)
{
    if (!seqDesc || !svcPayload || svcPayload->payLoadType != UMC::SEI_SCALABILITY_INFO || !svcPayload->user_data.size())
        return UMC::UMC_OK;

    Ipp32u currentLayer = 0;

    UMC_H264_DECODER::scalability_layer_info* layers = (UMC_H264_DECODER::scalability_layer_info*)&svcPayload->user_data[0];
    if (!layers)
        return UMC::UMC_OK;

    for (Ipp32u i = 0; i < svcPayload->SEI_messages.scalability_info.num_layers; i++)
    {
        currentLayer = layers[i].dependency_id;

        seqDesc->DependencyLayer[currentLayer].Active = 1;

        if (!seqDesc->DependencyLayer[currentLayer].Width && layers[i].frm_width_in_mbs)
        {
            seqDesc->DependencyLayer[currentLayer].Width    = (mfxU16)(layers[i].frm_width_in_mbs * 16);
            seqDesc->DependencyLayer[currentLayer].Height   = (mfxU16)(layers[i].frm_height_in_mbs * 16);
        }

        if (layers[i].quality_id + 1 > seqDesc->DependencyLayer[currentLayer].QualityNum)
        {
            seqDesc->DependencyLayer[currentLayer].QualityNum = layers[i].quality_id + 1;
        }

        bool isTempFound = false;
        for (Ipp32u temp = 0; temp < seqDesc->DependencyLayer[currentLayer].TemporalNum; temp++)
        {
            if (layers[i].temporal_id == seqDesc->DependencyLayer[currentLayer].TemporalId[temp])
            {
                isTempFound = true;
                break;
            }
        }

        if (!isTempFound)
        {
            Ipp32u tempNum = seqDesc->DependencyLayer[currentLayer].TemporalNum;
            seqDesc->DependencyLayer[currentLayer].TemporalId[tempNum] = layers[i].temporal_id;
            seqDesc->DependencyLayer[currentLayer].TemporalNum++;
        }
    }

    return UMC::UMC_OK;
}

UMC::Status MFX_Utility::FillVideoParamMVCEx(UMC::TaskSupplier * supplier, ::mfxVideoParam *par)
{
    const UMC_H264_DECODER::H264SeqParamSetSVCExtension * seqSVC = supplier->GetHeaders()->m_SeqParamsSvcExt.GetCurrentHeader();

    if (seqSVC)
    {
        const UMC_H264_DECODER::H264SEIPayLoad * svcPayload = supplier->GetHeaders()->m_SEIParams.GetHeader(UMC::SEI_SCALABILITY_INFO);
        mfxExtSVCSeqDesc *svcSeqDesc = (mfxExtSVCSeqDesc *)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);

        UMC::Status sts = FillVideoParamSVC(svcSeqDesc, svcPayload);
        if (sts != UMC::UMC_OK)
            return sts;

        UMC::FillVideoParam(seqSVC, par, true);
        return UMC::UMC_OK;
    }

    const UMC_H264_DECODER::H264SeqParamSetMVCExtension * seqMVC = supplier->GetHeaders()->m_SeqParamsMvcExt.GetCurrentHeader();
    mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

    if (!seqMVC)
        return UMC::UMC_OK;
    par->mfx.CodecProfile = seqMVC->profile_idc;
    par->mfx.CodecLevel = (mfxU16)supplier->GetLevelIDC();

    if (!points)
        return UMC::UMC_OK;

    Ipp32u numRefFrames = (seqMVC->num_ref_frames + 1) * (seqMVC->num_views_minus1 + 1);
    points->NumRefsTotal = (mfxU16)numRefFrames;


    UMC::Status sts = FillVideoParamExtension(seqMVC, par);
    par->mfx.CodecProfile = seqMVC->profile_idc;
    par->mfx.CodecLevel = (mfxU16)supplier->GetLevelIDC();
    return sts;
}

template <typename T>
void CheckDimensions(T &info_in, T &info_out, mfxStatus & sts)
{
    if (info_in.Width % 16 == 0 && info_in.Width <= 16384)
        info_out.Width = info_in.Width;
    else
    {
        info_out.Width = 0;
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (info_in.Height % 16 == 0 && info_in.Height <= 16384)
        info_out.Height = info_in.Height;
    else
    {
        info_out.Height = 0;
        sts = MFX_ERR_UNSUPPORTED;
    }

    if ((info_in.Width || info_in.Height) && !(info_in.Width && info_in.Height))
    {
        info_out.Width = 0;
        info_out.Height = 0;
        sts = MFX_ERR_UNSUPPORTED;
   }
}

mfxStatus MFX_Utility::Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (in == out)
    {
        mfxVideoParam in1;
        MFX_INTERNAL_CPY(&in1, in, sizeof(mfxVideoParam));
        return MFX_Utility::Query(core, &in1, out, type);
    }

    memset(&out->mfx, 0, sizeof(mfxInfoMFX));

    if (in)
    {
        if (in->mfx.CodecId == MFX_CODEC_AVC)
            out->mfx.CodecId = in->mfx.CodecId;

        if ((MFX_PROFILE_UNKNOWN == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_BASELINE == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MAIN == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_HIGH == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_EXTENDED == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MULTIVIEW_HIGH == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_STEREO_HIGH == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_SCALABLE_BASELINE == in->mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_SCALABLE_HIGH == in->mfx.CodecProfile))
            out->mfx.CodecProfile = in->mfx.CodecProfile;
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (core->GetVAType() == MFX_HW_VAAPI)
        {
            int codecProfile = in->mfx.CodecProfile & 0xFF;
            if (    (codecProfile == MFX_PROFILE_AVC_STEREO_HIGH) ||
                    (codecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH) ||
                    (codecProfile == MFX_PROFILE_AVC_SCALABLE_BASELINE) ||
                    (codecProfile == MFX_PROFILE_AVC_SCALABLE_HIGH) )
                return MFX_ERR_UNSUPPORTED;
        }

        switch (in->mfx.CodecLevel)
        {
        case MFX_LEVEL_UNKNOWN:
        case MFX_LEVEL_AVC_1:
        case MFX_LEVEL_AVC_1b:
        case MFX_LEVEL_AVC_11:
        case MFX_LEVEL_AVC_12:
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2:
        case MFX_LEVEL_AVC_21:
        case MFX_LEVEL_AVC_22:
        case MFX_LEVEL_AVC_3:
        case MFX_LEVEL_AVC_31:
        case MFX_LEVEL_AVC_32:
        case MFX_LEVEL_AVC_4:
        case MFX_LEVEL_AVC_41:
        case MFX_LEVEL_AVC_42:
        case MFX_LEVEL_AVC_5:
        case MFX_LEVEL_AVC_51:
        case MFX_LEVEL_AVC_52:
            out->mfx.CodecLevel = in->mfx.CodecLevel;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        if (in->mfx.NumThread < 128)
        {
            out->mfx.NumThread = in->mfx.NumThread;
        }
        else
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE) // Actually AsyncDepth > 5-7 is for debugging only.
            out->AsyncDepth = in->AsyncDepth;

        if (in->mfx.DecodedOrder)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.SliceGroupsPresent)
        {
            if (in->mfx.SliceGroupsPresent == 1)
                out->mfx.SliceGroupsPresent = in->mfx.SliceGroupsPresent;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.TimeStampCalc)
        {
            if (in->mfx.TimeStampCalc == 1)
                in->mfx.TimeStampCalc = out->mfx.TimeStampCalc;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.ExtendedPicStruct)
        {
            if (in->mfx.ExtendedPicStruct == 1)
                in->mfx.ExtendedPicStruct = out->mfx.ExtendedPicStruct;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if ((in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) ||
            (in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        {
            Ipp32u mask = in->IOPattern & 0xf0;
            if (mask == MFX_IOPATTERN_OUT_VIDEO_MEMORY || mask == MFX_IOPATTERN_OUT_SYSTEM_MEMORY || mask == MFX_IOPATTERN_OUT_OPAQUE_MEMORY)
                out->IOPattern = in->IOPattern;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.FourCC)
        {
            if (in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
                out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        if (in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV420 || in->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV400)
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        else
            sts = MFX_ERR_UNSUPPORTED;

        CheckDimensions(in->mfx.FrameInfo, out->mfx.FrameInfo, sts);

        /*if (in->mfx.FrameInfo.CropX <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;

        if (in->mfx.FrameInfo.CropY <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        if (out->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW <= out->mfx.FrameInfo.Width)
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;

        if (out->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH <= out->mfx.FrameInfo.Height)
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;*/

        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        if ((in->mfx.FrameInfo.FrameRateExtN || in->mfx.FrameInfo.FrameRateExtD) && !(in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD))
        {
            out->mfx.FrameInfo.FrameRateExtN = 0;
            out->mfx.FrameInfo.FrameRateExtD = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        {
            out->mfx.FrameInfo.AspectRatioW = 0;
            out->mfx.FrameInfo.AspectRatioH = 0;
            sts = MFX_ERR_UNSUPPORTED;
        }

        switch (in->mfx.FrameInfo.PicStruct)
        {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_FIELD_BFF:
        case MFX_PICSTRUCT_FIELD_REPEATED:
        case MFX_PICSTRUCT_FRAME_DOUBLING:
        case MFX_PICSTRUCT_FRAME_TRIPLING:
            out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
            break;
        default:
            sts = MFX_ERR_UNSUPPORTED;
            break;
        }

        mfxStatus stsExt = CheckDecodersExtendedBuffers(in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (in->Protected)
        {
            out->Protected = in->Protected;

            if (type == MFX_HW_UNKNOWN)
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->Protected = 0;
            }

            if (!IS_PROTECTION_ANY(in->Protected))
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->Protected = 0;
            }

            if (in->Protected == MFX_PROTECTION_GPUCP_AES128_CTR && core->GetVAType() != MFX_HW_D3D11)
            {
                sts = MFX_ERR_UNSUPPORTED;
                out->Protected = 0;
            }

            if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
            {
                out->IOPattern = 0;
                sts = MFX_ERR_UNSUPPORTED;
            }

            mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (IS_PROTECTION_PAVP_ANY(in->Protected))
            {
                if (pavpOptIn && pavpOptOut)
                {
                    pavpOptOut->Header.BufferId = pavpOptIn->Header.BufferId;
                    pavpOptOut->Header.BufferSz = pavpOptIn->Header.BufferSz;

                    switch(pavpOptIn->CounterType)
                    {
                    case MFX_PAVP_CTR_TYPE_A:
                        pavpOptOut->CounterType = pavpOptIn->CounterType;
                        break;
                    case MFX_PAVP_CTR_TYPE_B:
                        if (in->Protected == MFX_PROTECTION_PAVP)
                        {
                            pavpOptOut->CounterType = pavpOptIn->CounterType;
                            break;
                        }
                        pavpOptOut->CounterType = 0;
                        sts = MFX_ERR_UNSUPPORTED;
                        break;
                    case MFX_PAVP_CTR_TYPE_C:
                        pavpOptOut->CounterType = pavpOptIn->CounterType;
                        break;
                    default:
                        pavpOptOut->CounterType = 0;
                        sts = MFX_ERR_UNSUPPORTED;
                        break;
                    }

                    if (pavpOptIn->EncryptionType == MFX_PAVP_AES128_CBC || pavpOptIn->EncryptionType == MFX_PAVP_AES128_CTR)
                    {
                        pavpOptOut->EncryptionType = pavpOptIn->EncryptionType;
                    }
                    else
                    {
                        pavpOptOut->EncryptionType = 0;
                        sts = MFX_ERR_UNSUPPORTED;
                    }

                    pavpOptOut->CounterIncrement = 0;
                    memset(&pavpOptOut->CipherCounter, 0, sizeof(mfxAES128CipherCounter));
                    memset(pavpOptOut->reserved, 0, sizeof(pavpOptOut->reserved));
                }
                else
                {
                    if (pavpOptOut || pavpOptIn)
                    {
                        sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    }
                }
            }
            else
            {
                if (pavpOptOut || pavpOptIn)
                {
                    sts = MFX_ERR_UNSUPPORTED;
                }
            }
        }
        else
        {
            mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (pavpOptIn)
                sts = MFX_ERR_UNSUPPORTED;
        }

        mfxExtMVCSeqDesc * mvcPointsIn = (mfxExtMVCSeqDesc *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
        mfxExtMVCSeqDesc * mvcPointsOut = (mfxExtMVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

        if (mvcPointsIn && mvcPointsOut)
        {
            if (mvcPointsIn->NumView <= UMC::H264_MAX_NUM_VIEW && mvcPointsIn->NumView <= mvcPointsIn->NumViewAlloc && mvcPointsOut->NumViewAlloc >= mvcPointsIn->NumView)
            {
                mvcPointsOut->NumView = mvcPointsIn->NumView;

                for (Ipp32u i = 0; i < mvcPointsIn->NumView; i++)
                {
                    if (mvcPointsIn->View[i].ViewId >= UMC::H264_MAX_NUM_VIEW)
                        continue;

                    mvcPointsOut->View[i].ViewId = mvcPointsIn->View[i].ViewId;

                    if (mvcPointsIn->View[i].NumAnchorRefsL0 <= UMC::H264_MAX_NUM_VIEW_REF)
                    {
                        mvcPointsOut->View[i].NumAnchorRefsL0 = mvcPointsIn->View[i].NumAnchorRefsL0;
                        MFX_INTERNAL_CPY(mvcPointsOut->View[i].AnchorRefL0, mvcPointsIn->View[i].AnchorRefL0, sizeof(mvcPointsIn->View[i].AnchorRefL0));
                    }
                    else
                        sts = MFX_ERR_UNSUPPORTED;

                    if (mvcPointsIn->View[i].NumAnchorRefsL1 <= UMC::H264_MAX_NUM_VIEW_REF)
                    {
                        mvcPointsOut->View[i].NumAnchorRefsL1 = mvcPointsIn->View[i].NumAnchorRefsL1;
                        MFX_INTERNAL_CPY(mvcPointsOut->View[i].AnchorRefL1, mvcPointsIn->View[i].AnchorRefL1, sizeof(mvcPointsIn->View[i].AnchorRefL1));
                    }
                    else
                        sts = MFX_ERR_UNSUPPORTED;

                    if (mvcPointsIn->View[i].NumNonAnchorRefsL0 <= UMC::H264_MAX_NUM_VIEW_REF)
                    {
                        mvcPointsOut->View[i].NumNonAnchorRefsL0 = mvcPointsIn->View[i].NumNonAnchorRefsL0;
                        MFX_INTERNAL_CPY(mvcPointsOut->View[i].NonAnchorRefL0, mvcPointsIn->View[i].NonAnchorRefL0, sizeof(mvcPointsIn->View[i].NonAnchorRefL0));
                    }
                    else
                        sts = MFX_ERR_UNSUPPORTED;

                    if (mvcPointsIn->View[i].NumNonAnchorRefsL1 <= UMC::H264_MAX_NUM_VIEW_REF)
                    {
                        mvcPointsOut->View[i].NumNonAnchorRefsL1 = mvcPointsIn->View[i].NumNonAnchorRefsL1;
                        MFX_INTERNAL_CPY(mvcPointsOut->View[i].NonAnchorRefL1, mvcPointsIn->View[i].NonAnchorRefL1, sizeof(mvcPointsIn->View[i].NonAnchorRefL1));
                    }
                    else
                        sts = MFX_ERR_UNSUPPORTED;
                }
            }

            if (mvcPointsIn->NumOP <= mvcPointsIn->NumOPAlloc && mvcPointsOut->NumOPAlloc >= mvcPointsIn->NumOP &&
                mvcPointsIn->NumViewId <= mvcPointsIn->NumViewIdAlloc && mvcPointsOut->NumViewIdAlloc >= mvcPointsIn->NumViewId)
            {
                mfxU16 * targetViews = mvcPointsOut->ViewId;

                mvcPointsOut->NumOP = mvcPointsIn->NumOP;
                for (Ipp32u i = 0; i < mvcPointsIn->NumOP; i++)
                {
                    if (mvcPointsIn->OP[i].TemporalId <= 7)
                        mvcPointsOut->OP[i].TemporalId = mvcPointsIn->OP[i].TemporalId;
                    else
                        sts = MFX_ERR_UNSUPPORTED;

                    switch (mvcPointsIn->OP[i].LevelIdc)
                    {
                    case MFX_LEVEL_UNKNOWN:
                    case MFX_LEVEL_AVC_1:
                    case MFX_LEVEL_AVC_1b:
                    case MFX_LEVEL_AVC_11:
                    case MFX_LEVEL_AVC_12:
                    case MFX_LEVEL_AVC_13:
                    case MFX_LEVEL_AVC_2:
                    case MFX_LEVEL_AVC_21:
                    case MFX_LEVEL_AVC_22:
                    case MFX_LEVEL_AVC_3:
                    case MFX_LEVEL_AVC_31:
                    case MFX_LEVEL_AVC_32:
                    case MFX_LEVEL_AVC_4:
                    case MFX_LEVEL_AVC_41:
                    case MFX_LEVEL_AVC_42:
                    case MFX_LEVEL_AVC_5:
                    case MFX_LEVEL_AVC_51:
                    case MFX_LEVEL_AVC_52:
                        mvcPointsOut->OP[i].LevelIdc = mvcPointsIn->OP[i].LevelIdc;
                        break;
                    default:
                        sts = MFX_ERR_UNSUPPORTED;
                    }

                    if (mvcPointsIn->OP[i].NumViews <= UMC::H264_MAX_NUM_VIEW)
                        mvcPointsOut->OP[i].NumViews = mvcPointsIn->OP[i].NumViews;
                    else
                        sts = MFX_ERR_UNSUPPORTED;

                    if (mvcPointsIn->OP[i].NumTargetViews <= mvcPointsIn->OP[i].NumViews && mvcPointsIn->OP[i].NumTargetViews <= UMC::H264_MAX_NUM_VIEW)
                    {
                        mvcPointsOut->OP[i].TargetViewId = targetViews;
                        mvcPointsOut->OP[i].NumTargetViews = mvcPointsIn->OP[i].NumTargetViews;
                        for (Ipp32u j = 0; j < mvcPointsIn->OP[i].NumTargetViews; j++)
                        {

                            if (mvcPointsIn->OP[i].TargetViewId[j] <= UMC::H264_MAX_NUM_VIEW)
                                mvcPointsOut->OP[i].TargetViewId[j] = mvcPointsIn->OP[i].TargetViewId[j];
                            else
                                sts = MFX_ERR_UNSUPPORTED;
                        }

                        targetViews += mvcPointsIn->OP[i].NumTargetViews;
                    }
                }
            }

            //if (mvcPointsIn->CompatibilityMode > 0 && mvcPointsIn->CompatibilityMode < 2)
            //    mvcPointsOut->CompatibilityMode = mvcPointsIn->CompatibilityMode;
        }
        else
        {
            if (mvcPointsIn || mvcPointsOut)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        mfxExtMVCTargetViews * targetViewsIn = (mfxExtMVCTargetViews *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
        mfxExtMVCTargetViews * targetViewsOut = (mfxExtMVCTargetViews *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);

        if (targetViewsIn && targetViewsOut)
        {
            if (targetViewsIn->TemporalId <= 7)
                targetViewsOut->TemporalId = targetViewsIn->TemporalId;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (targetViewsIn->NumView <= 1024)
                targetViewsOut->NumView = targetViewsIn->NumView;
            else
                sts = MFX_ERR_UNSUPPORTED;

            MFX_INTERNAL_CPY(targetViewsOut->ViewId, targetViewsIn->ViewId, sizeof(targetViewsIn->ViewId));
        }
        else
        {
            if (targetViewsIn || targetViewsOut)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        mfxExtSVCSeqDesc * svcDescIn = (mfxExtSVCSeqDesc *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
        mfxExtSVCSeqDesc * svcDescOut = (mfxExtSVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);

        if (svcDescIn && svcDescOut)
        {
            MFX_INTERNAL_CPY(svcDescOut, svcDescIn, sizeof(mfxExtSVCSeqDesc));

            for (Ipp32u layer = 0; layer < sizeof(svcDescIn->DependencyLayer)/sizeof(svcDescIn->DependencyLayer[0]); layer++)
            {
                CheckDimensions(svcDescIn->DependencyLayer[layer], svcDescOut->DependencyLayer[layer], sts);

                if (svcDescIn->DependencyLayer[layer].TemporalNum > 7)
                {
                    svcDescOut->DependencyLayer[layer].TemporalNum = 0;
                    memset(svcDescOut->DependencyLayer[layer].TemporalId, 0, sizeof(svcDescOut->DependencyLayer[layer].TemporalId));
                    sts = MFX_ERR_UNSUPPORTED;
                }

                if (svcDescIn->DependencyLayer[layer].QualityNum > 15)
                {
                    svcDescOut->DependencyLayer[layer].QualityNum = 0;
                    sts = MFX_ERR_UNSUPPORTED;
                }

                if (svcDescOut->DependencyLayer[layer].TemporalNum)
                {
                    typedef std::vector<Ipp16u> TemporalIDList;
                    TemporalIDList vec(svcDescIn->DependencyLayer[layer].TemporalId, svcDescIn->DependencyLayer[layer].TemporalId + svcDescOut->DependencyLayer[layer].TemporalNum);

                    std::sort(vec.begin(), vec.end());
                    TemporalIDList::iterator end_iter = std::adjacent_find(vec.begin(), vec.end());

                    if (end_iter != vec.end())
                    {
                        svcDescOut->DependencyLayer[layer].TemporalNum = 0;
                        memset(svcDescOut->DependencyLayer[layer].TemporalId, 0, sizeof(svcDescOut->DependencyLayer[layer].TemporalId));
                        sts = MFX_ERR_UNSUPPORTED;
                    }
                }
            }
        }
        else
        {
            if (svcDescIn || svcDescOut)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        mfxExtSvcTargetLayer * svcTargetIn = (mfxExtSvcTargetLayer *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        mfxExtSvcTargetLayer * svcTargetOut = (mfxExtSvcTargetLayer *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);

        if (svcTargetIn && svcTargetOut)
        {
            if (svcTargetIn->TargetDependencyID <= 7)
            {
                svcTargetOut->TargetDependencyID = svcTargetIn->TargetDependencyID;
                if (svcDescOut && !svcDescOut->DependencyLayer[svcTargetOut->TargetDependencyID].Active)
                {
                    svcTargetOut->TargetDependencyID = 0;
                    sts = MFX_ERR_INVALID_VIDEO_PARAM;
                }
            }
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (svcTargetIn->TargetQualityID <= 15)
                svcTargetOut->TargetQualityID = svcTargetIn->TargetQualityID;
            else
                sts = MFX_ERR_UNSUPPORTED;

            if (svcTargetIn->TargetTemporalID <= 7)
                svcTargetOut->TargetTemporalID = svcTargetIn->TargetTemporalID;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }
        else
        {
            if (svcTargetIn || svcTargetOut)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (GetPlatform(core, out) != core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform(core, out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        out->mfx.CodecId = MFX_CODEC_AVC;
        out->mfx.CodecProfile = 1;
        out->mfx.CodecLevel = 1;

        out->mfx.NumThread = 1;

        out->mfx.DecodedOrder = 0;

        out->mfx.TimeStampCalc = 1;
        out->mfx.SliceGroupsPresent = 1;
        out->mfx.ExtendedPicStruct = 1;
        out->AsyncDepth = 1;

        // mfxFrameInfo
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width = 16;
        out->mfx.FrameInfo.Height = 16;

        //out->mfx.FrameInfo.CropX = 1;
        //out->mfx.FrameInfo.CropY = 1;
        //out->mfx.FrameInfo.CropW = 1;
        //out->mfx.FrameInfo.CropH = 1;

        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;

        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;

        out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        if (type >= MFX_HW_SNB)
        {
            out->Protected = MFX_PROTECTION_GPUCP_PAVP;

            mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
            if (pavpOptOut)
            {
                mfxExtBuffer header = pavpOptOut->Header;
                memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                pavpOptOut->Header = header;
                pavpOptOut->CounterType = (mfxU16)((type == MFX_HW_IVB)||(type == MFX_HW_VLV) ? MFX_PAVP_CTR_TYPE_C : MFX_PAVP_CTR_TYPE_A);
                pavpOptOut->EncryptionType = MFX_PAVP_AES128_CTR;
                pavpOptOut->CounterIncrement = 0;
                pavpOptOut->CipherCounter.Count = 0;
                pavpOptOut->CipherCounter.IV = 0;
            }
        }

        mfxExtMVCTargetViews * targetViewsOut = (mfxExtMVCTargetViews *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
        if (targetViewsOut)
        {
            targetViewsOut->TemporalId = 1;
            targetViewsOut->NumView = 1;
        }

        mfxExtMVCSeqDesc * mvcPointsOut = (mfxExtMVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);
        if (mvcPointsOut)
        {
        }

        mfxExtSvcTargetLayer * targetLayerOut = (mfxExtSvcTargetLayer *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
        if (targetLayerOut)
        {
            targetLayerOut->TargetDependencyID = 1;
            targetLayerOut->TargetQualityID = 1;
            targetLayerOut->TargetTemporalID = 1;
        }

        mfxExtSVCSeqDesc * svcDescOut = (mfxExtSVCSeqDesc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_SVC_SEQ_DESC);
        if (svcDescOut)
        {
            for (size_t i = 0; i < sizeof(svcDescOut->DependencyLayer)/sizeof(svcDescOut->DependencyLayer[0]); i++)
            {
                svcDescOut->DependencyLayer[i].Active = 1;
                svcDescOut->DependencyLayer[i].Width = 1;
                svcDescOut->DependencyLayer[i].Height = 1;
                svcDescOut->DependencyLayer[i].QualityNum = 1;
                svcDescOut->DependencyLayer[i].TemporalNum = 1;
            }
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }

        out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }
    }

    return sts;
}

bool MFX_Utility::CheckVideoParam(mfxVideoParam *in, eMFXHWType type)
{
    if (!in)
        return false;

    if (in->Protected)
    {
        if (type == MFX_HW_UNKNOWN || !IS_PROTECTION_ANY(in->Protected))
            return false;
    }

    if (MFX_CODEC_AVC != in->mfx.CodecId)
        return false;

    Ipp32u profile_idc = ExtractProfile(in->mfx.CodecProfile);
    switch (profile_idc)
    {
    case MFX_PROFILE_UNKNOWN:
    case MFX_PROFILE_AVC_BASELINE:
    case MFX_PROFILE_AVC_MAIN:
    case MFX_PROFILE_AVC_EXTENDED:
    case MFX_PROFILE_AVC_HIGH:
    case MFX_PROFILE_AVC_STEREO_HIGH:
    case MFX_PROFILE_AVC_MULTIVIEW_HIGH:
    case MFX_PROFILE_AVC_SCALABLE_BASELINE:
    case MFX_PROFILE_AVC_SCALABLE_HIGH:
        break;
    default:
        return false;
    }

    mfxExtSvcTargetLayer * svcTarget = (mfxExtSvcTargetLayer*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_SVC_TARGET_LAYER);
    if (svcTarget)
    {
        if (svcTarget->TargetDependencyID > 7)
            return false;

        if (svcTarget->TargetQualityID > 15)
            return false;

        if (svcTarget->TargetTemporalID > 7)
            return false;
    }

    mfxExtMVCTargetViews * targetViews = (mfxExtMVCTargetViews *)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_MVC_TARGET_VIEWS);
    if (targetViews)
    {
        if (targetViews->TemporalId > 7)
            return false;

        if (targetViews->NumView > 1024)
            return false;
    }

    if (in->mfx.FrameInfo.Width > 16384 || (in->mfx.FrameInfo.Width % 16))
        return false;

    if (in->mfx.FrameInfo.Height > 16384 || (in->mfx.FrameInfo.Height % 16))
        return false;

#if 0
    // ignore Crop parameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    if (in->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
        return false;

    // both zero or not zero
    if ((in->mfx.FrameInfo.AspectRatioW || in->mfx.FrameInfo.AspectRatioH) && !(in->mfx.FrameInfo.AspectRatioW && in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (in->mfx.FrameInfo.PicStruct)
    {
    case MFX_PICSTRUCT_UNKNOWN:
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
    case MFX_PICSTRUCT_FIELD_REPEATED:
    case MFX_PICSTRUCT_FRAME_DOUBLING:
    case MFX_PICSTRUCT_FRAME_TRIPLING:
        break;
    default:
        return false;
    }

    if (in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 && in->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV400)
        return false;

    if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return false;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return false;

    return true;
}

#endif // UMC_RESTRICTED_CODE_MFX
#endif // UMC_ENABLE_H264_VIDEO_DECODER
