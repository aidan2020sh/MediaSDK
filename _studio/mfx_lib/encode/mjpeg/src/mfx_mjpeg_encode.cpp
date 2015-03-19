//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2008-2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)

#include "mfx_common.h"

#include "mfx_mjpeg_encode.h"
#include "mfx_tools.h"
#include "mfx_task.h"
#include "mfx_enc_common.h"
#include "vm_sys_info.h"
#include <umc_automatic_mutex.h>
#include "umc_video_processing.h"

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)


MJPEGEncodeTask::MJPEGEncodeTask(void)
{
    m_initialDataLength = 0;
    encodedPieces = 0;

} // MJPEGEncodeTask::MJPEGEncodeTask(void)

MJPEGEncodeTask::~MJPEGEncodeTask(void)
{
    Close();

} // MJPEGEncodeTask::~MJPEGEncodeTask(void)

void MJPEGEncodeTask::Close(void)
{
    m_initialDataLength = 0;
    encodedPieces = 0;

    if(m_pMJPEGVideoEncoder.get())
    {
        m_pMJPEGVideoEncoder.get()->Close();
    }

} // void MJPEGEncodeTask::Close(void)

mfxStatus MJPEGEncodeTask::Initialize(UMC::VideoEncoderParams &params)
{
    // close the object before initialization
    Close();

    {
        UMC::Status umcRes;

        m_initialDataLength = 0;
        encodedPieces = 0;

        m_pMJPEGVideoEncoder.reset(new UMC::MJPEGVideoEncoder);
        if(NULL == m_pMJPEGVideoEncoder.get())
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

        //m_pMJPEGVideoDecoder.get()->SetFrameAllocator(pFrameAllocator);
        umcRes = m_pMJPEGVideoEncoder.get()->Init(&params);
        if (umcRes != UMC::UMC_OK)
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    return MFX_ERR_NONE;

} // mfxStatus MJPEGEncodeTask::Initialize(UMC::VideoEncoderParams &params)

void MJPEGEncodeTask::Reset(void)
{
    m_initialDataLength = 0;
    encodedPieces = 0;

    if(m_pMJPEGVideoEncoder.get())
    {
        m_pMJPEGVideoEncoder->Reset();
    }

} // void MJPEGEncodeTask::Reset(void)

mfxU32 MJPEGEncodeTask::NumPicsCollected()
{
    return m_pMJPEGVideoEncoder.get()->NumPicsCollected();
}

mfxU32 MJPEGEncodeTask::NumPiecesCollected(void)
{
    return m_pMJPEGVideoEncoder.get()->NumPiecesCollected();
}

mfxStatus MJPEGEncodeTask::AddSource(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo, bool useAuxInput)
{
    Ipp32u  width       = frameInfo->CropW - frameInfo->CropX;
    Ipp32u  height      = frameInfo->CropH - frameInfo->CropY;
    Ipp32u  alignedWidth  = frameInfo->Width;
    Ipp32u  alignedHeight = frameInfo->Height;
    Ipp32u  numFields   = 0;
    Ipp32u  isBottom    = 0;
    Ipp32u  fieldOffset = 0;
    Ipp32u  numPieces   = 0;
    Ipp32u  mcuWidth, mcuHeight, numxMCU, numyMCU;
    UMC::Status sts = UMC::UMC_OK;

    UMC::MJPEGEncoderParams params;
    m_pMJPEGVideoEncoder->GetInfo(&params);

    switch (params.info.interlace_type)
    {
        case UMC::PROGRESSIVE:
        {
            numFields = 1;
            break;
        }
        case UMC::INTERLEAVED_TOP_FIELD_FIRST:
        {
            numFields = 2;
            height  >>= 1;
            alignedHeight >>= 1;
            break;
        }
        case UMC::INTERLEAVED_BOTTOM_FIELD_FIRST:
        {
            numFields = 2;
            height  >>= 1;
            alignedHeight >>= 1;
            isBottom  = 1;
            break;
        }
        default:
            return MFX_ERR_UNKNOWN;
    }

    // create an entry in the array
    while (NumPicsCollected() < numFields)
    {
        UMC::MJPEGEncoderPicture *p = new UMC::MJPEGEncoderPicture();

        if (NULL == p)
        {
            return MFX_ERR_MEMORY_ALLOC;
        }

        UMC::VideoData* pDataIn = p->m_sourceData.get();
        mfxU32 pitch = surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16);

        // color image
        if(MFX_CHROMAFORMAT_YUV400 != surface->Info.ChromaFormat)
        {
            if(surface->Info.FourCC == MFX_FOURCC_NV12)
            {
                fieldOffset = pitch * isBottom;
                pDataIn->Init(alignedWidth, alignedHeight, UMC::NV12, 8);
                pDataIn->SetImageSize(width, height);

                pDataIn->SetPlanePointer(surface->Data.Y + frameInfo->CropX + fieldOffset, 0);
                pDataIn->SetPlanePitch(pitch * numFields, 0);
                pDataIn->SetPlanePointer(surface->Data.UV + ((frameInfo->CropX >> 1) << 1) + fieldOffset, 1);
                pDataIn->SetPlanePitch(pitch * numFields, 1);
            }
            else if(surface->Info.FourCC == MFX_FOURCC_YV12)
            {
                fieldOffset = pitch * isBottom;
                pDataIn->Init(alignedWidth, alignedHeight, UMC::YV12, 8);
                pDataIn->SetImageSize(width, height);

                pDataIn->SetPlanePointer(surface->Data.Y + frameInfo->CropX + fieldOffset, 0);
                pDataIn->SetPlanePitch(pitch * numFields, 0);
                pDataIn->SetPlanePointer(surface->Data.V + (frameInfo->CropX >> 1) + (fieldOffset >> 1), 1);
                pDataIn->SetPlanePitch((pitch >> 1) * numFields, 1);
                pDataIn->SetPlanePointer(surface->Data.U + (frameInfo->CropX >> 1) + (fieldOffset >> 1), 2);
                pDataIn->SetPlanePitch((pitch >> 1) * numFields, 2);
            }
            else if(surface->Info.FourCC == MFX_FOURCC_YUY2)
            {
                UMC::VideoData* cvt = new UMC::VideoData();

                cvt->Init(alignedWidth, alignedHeight, UMC::YUV422);
                cvt->SetImageSize(width, height);
                sts = cvt->Alloc();
                if(sts != UMC::UMC_OK)
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_MEMORY_ALLOC;
                }

                if(MFX_CHROMAFORMAT_YUV422H == surface->Info.ChromaFormat)
                {
                    fieldOffset = pitch * isBottom;
                    pDataIn->Init(alignedWidth, alignedHeight, UMC::YUY2, 8);
                    pDataIn->SetImageSize(width, height);

                    pDataIn->SetPlanePointer(surface->Data.Y + ((frameInfo->CropX >> 1) << 2) + fieldOffset, 0);
                    pDataIn->SetPlanePitch(pitch * numFields, 0);
                    
                    UMC::VideoProcessing proc;
                    proc.GetFrame(pDataIn, cvt);
                }
                else if(MFX_CHROMAFORMAT_YUV422V == surface->Info.ChromaFormat)
                {
                    fieldOffset = pitch * isBottom;
                    Ipp8u* src = surface->Data.Y + ((frameInfo->CropX >> 1) << 2) + fieldOffset;
                    Ipp32u srcPitch = pitch * numFields;

                    Ipp8u* dst[3] = {(Ipp8u*)cvt->GetPlanePointer(0), 
                                     (Ipp8u*)cvt->GetPlanePointer(1), 
                                     (Ipp8u*)cvt->GetPlanePointer(2)};
                    Ipp32u dstPitch[3] = {(Ipp32u)cvt->GetPlanePitch(0), 
                                          (Ipp32u)cvt->GetPlanePitch(1), 
                                          (Ipp32u)cvt->GetPlanePitch(2)};
                    for(Ipp32u i=0; i<height; i++)
                        for(Ipp32u j=0; j<width/2; j++)
                        {
                            dst[0][i*dstPitch[0] + 2*j]                   = src[i*srcPitch + (j<<2)];
                            dst[1][((i/2)*2)*dstPitch[1] + 2*j + (i % 2)] = src[i*srcPitch + (j<<2)+1];
                            dst[0][i*dstPitch[0] + 2*j + 1]               = src[i*srcPitch + (j<<2)+2];
                            dst[2][((i/2)*2)*dstPitch[2] + 2*j + (i % 2)] = src[i*srcPitch + (j<<2)+3];
                        }
                }
                else
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                
                p->m_sourceData.reset(cvt);
                p->m_release_source_data = true;
            }
            else if(surface->Info.FourCC == MFX_FOURCC_RGB4)
            {
                fieldOffset = pitch * isBottom;
                pDataIn->Init(alignedWidth, alignedHeight, UMC::RGB32, 8);
                pDataIn->SetImageSize(width, height);

                pDataIn->SetPlanePointer(surface->Data.B + frameInfo->CropX * 4 + fieldOffset, 0);
                pDataIn->SetPlanePitch(pitch * numFields, 0);
            }
            else
            {
                delete p;
                return MFX_ERR_UNSUPPORTED;
            }

            if(pDataIn->GetColorFormat() == UMC::NV12 || pDataIn->GetColorFormat() == UMC::YV12)
            {            
                UMC::VideoData* cvt = new UMC::VideoData();

                cvt->Init(alignedWidth, alignedHeight, UMC::YUV420);
                cvt->SetImageSize(width, height);
                sts = cvt->Alloc();
                if(sts != UMC::UMC_OK)
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_MEMORY_ALLOC;
                }

                UMC::VideoProcessing proc;
                proc.GetFrame(pDataIn, cvt);
                
                p->m_sourceData.reset(cvt);
                p->m_release_source_data = true;
            }
            else if (pDataIn->GetColorFormat() == UMC::RGB32 && useAuxInput)
            {
                UMC::VideoData* cvt = new UMC::VideoData();

                cvt->Init(alignedWidth, alignedHeight, UMC::RGB32);
                cvt->SetImageSize(width, height);
                sts = cvt->Alloc();
                if(sts != UMC::UMC_OK)
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_MEMORY_ALLOC;
                }

                UMC::VideoProcessing proc;
                proc.GetFrame(pDataIn, cvt);
                
                p->m_sourceData.reset(cvt);
                p->m_release_source_data = true;
            }
        }
        // gray image
        else
        {
            if(surface->Info.FourCC == MFX_FOURCC_NV12)
            {
                fieldOffset = pitch * isBottom;
                pDataIn->Init(alignedWidth, alignedHeight, UMC::GRAY, 8);
                pDataIn->SetImageSize(width, height);

                pDataIn->SetPlanePointer(surface->Data.Y + frameInfo->CropX + fieldOffset, 0);
                pDataIn->SetPlanePitch(pitch * numFields, 0);
            }
            else if(surface->Info.FourCC == MFX_FOURCC_YV12)
            {
                fieldOffset = pitch * isBottom;
                pDataIn->Init(alignedWidth, alignedHeight, UMC::GRAY, 8);
                pDataIn->SetImageSize(width, height);

                pDataIn->SetPlanePointer(surface->Data.Y + frameInfo->CropX + fieldOffset, 0);
                pDataIn->SetPlanePitch(pitch * numFields, 0);
            }
            else if(surface->Info.FourCC == MFX_FOURCC_YUY2)
            {
                UMC::VideoData* cvt = new UMC::VideoData();

                cvt->Init(alignedWidth, alignedHeight, UMC::GRAY);
                cvt->SetImageSize(width, height);
                sts = cvt->Alloc();
                if(sts != UMC::UMC_OK)
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_MEMORY_ALLOC;
                }

                Ipp8u* src = surface->Data.Y;
                Ipp32u srcPitch = pitch;
                Ipp8u* dst = (Ipp8u*)cvt->GetPlanePointer(0);
                Ipp32u dstPitch = (Ipp32u)cvt->GetPlanePitch(0);

                for(Ipp32u i=0; i<height; i++)
                    for(Ipp32u j=0; j<width; j++)
                    {
                        dst[i*dstPitch + j] = src[i*srcPitch + (j<<1)];
                    }
                
                p->m_sourceData.reset(cvt);
                p->m_release_source_data = true;
            }
            else
            {
                delete p;
                return MFX_ERR_UNSUPPORTED;
            }

            if((surface->Info.FourCC == MFX_FOURCC_NV12 || surface->Info.FourCC == MFX_FOURCC_YV12) && useAuxInput)
            {            
                UMC::VideoData* cvt = new UMC::VideoData();

                cvt->Init(alignedWidth, alignedHeight, UMC::GRAY);
                cvt->SetImageSize(width, height);
                sts = cvt->Alloc();
                if(sts != UMC::UMC_OK)
                {
                    delete p;
                    delete cvt;
                    return MFX_ERR_MEMORY_ALLOC;
                }

                UMC::VideoProcessing proc;
                proc.GetFrame(pDataIn, cvt);
                
                p->m_sourceData.reset(cvt);
                p->m_release_source_data = true;
            }
        }

        p->m_sourceData.get()->SetTime((Ipp64f)surface->Data.TimeStamp);

        if(MFX_SCANTYPE_INTERLEAVED == params.interleaved || MFX_CHROMAFORMAT_YUV400 == surface->Info.ChromaFormat)
        {
            UMC::MJPEGEncoderScan *s = new UMC::MJPEGEncoderScan();

            if(params.restart_interval)
            {
                switch(surface->Info.ChromaFormat)
                {
                    case MFX_CHROMAFORMAT_YUV444:
                        mcuWidth = mcuHeight = 8;
                        break;
                    case MFX_CHROMAFORMAT_YUV422H:
                        mcuWidth  = 16;
                        mcuHeight = 8;
                        break;
                    case MFX_CHROMAFORMAT_YUV422V:
                        mcuWidth  = 8;
                        mcuHeight = 16;
                        break;
                    case MFX_CHROMAFORMAT_YUV420:
                        mcuWidth = mcuHeight = 16;
                        break;
                    case MFX_CHROMAFORMAT_YUV400:
                        mcuWidth = mcuHeight = 8;
                        break;
                    default:
                        delete p;
                        delete s;
                        return MFX_ERR_UNSUPPORTED;
                }

                numxMCU = (width  + (mcuWidth  - 1)) / mcuWidth;
                numyMCU = (height + (mcuHeight - 1)) / mcuHeight;
                
                numPieces = ((numxMCU * numyMCU + params.restart_interval - 1) / params.restart_interval) * numFields;
            }
            else
            {
                numPieces = 1;
            }

            s->Init(numPieces);

            p->m_scans.push_back(s);
        }
        else
        {
            for(Ipp32u i=0; i<3; i++)
            {
                UMC::MJPEGEncoderScan *s = new UMC::MJPEGEncoderScan();

                if(params.restart_interval)
                {
                    mcuWidth = 8;
                    mcuHeight = 8;

                    numxMCU = (width  + (mcuWidth  - 1)) / mcuWidth;
                    numyMCU = (height + (mcuHeight - 1)) / mcuHeight;

                    // U,V
                    if(i != 0)
                    {
                        switch(surface->Info.ChromaFormat)
                        {
                            case MFX_CHROMAFORMAT_YUV422H:
                            {
                                numxMCU = ((width >> 1) + (mcuWidth  - 1)) / mcuWidth;
                                break;
                            }
                            case MFX_CHROMAFORMAT_YUV422V:
                            {
                                numyMCU = ((height >> 1) + (mcuHeight - 1)) / mcuHeight;
                                break;
                            }
                            case MFX_CHROMAFORMAT_YUV420:
                            {
                                numxMCU = ((width >> 1) + (mcuWidth  - 1)) / mcuWidth;
                                numyMCU = ((height >> 1) + (mcuHeight - 1)) / mcuHeight;
                                break;
                            }
                        }
                    }
                    
                    numPieces = ((numxMCU * numyMCU + params.restart_interval - 1) / params.restart_interval) * numFields;
                }
                else
                {
                    numPieces = 1;
                }

                s->Init(numPieces);

                p->m_scans.push_back(s);
            }
        }

        m_pMJPEGVideoEncoder.get()->AddPicture(p);

        isBottom = 1 - isBottom;
    }

    return MFX_ERR_NONE;

} // mfxStatus MJPEGEncodeTask::AddSource(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo, bool useAuxInput)

mfxU32 MJPEGEncodeTask::CalculateNumPieces(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo)
{
    Ipp32u  width       = frameInfo->CropW - frameInfo->CropX;
    Ipp32u  height      = frameInfo->CropH - frameInfo->CropY;
    Ipp32u  numFields   = 0;
    Ipp32u  numPieces   = 0;
    Ipp32u  mcuWidth, mcuHeight, numxMCU, numyMCU;

    UMC::MJPEGEncoderParams params;
    m_pMJPEGVideoEncoder->GetInfo(&params);

    switch (params.info.interlace_type)
    {
        case UMC::PROGRESSIVE:
        {
            numFields = 1;
            break;
        }
        case UMC::INTERLEAVED_TOP_FIELD_FIRST:
        {
            numFields = 2;
            height  >>= 1;
            break;
        }
        case UMC::INTERLEAVED_BOTTOM_FIELD_FIRST:
        {
            numFields = 2;
            height  >>= 1;
            break;
        }
        default:
            return 0;
    }

    // create an entry in the array
    for(Ipp32u i=0; i<numFields; i++)
    {
        if(MFX_SCANTYPE_INTERLEAVED == params.interleaved || MFX_CHROMAFORMAT_YUV400 == surface->Info.ChromaFormat)
        {
            if(params.restart_interval)
            {
                switch(surface->Info.ChromaFormat)
                {
                    case MFX_CHROMAFORMAT_YUV444:
                        mcuWidth = mcuHeight = 8;
                        break;
                    case MFX_CHROMAFORMAT_YUV422H:
                        mcuWidth  = 16;
                        mcuHeight = 8;
                        break;
                    case MFX_CHROMAFORMAT_YUV422V:
                        mcuWidth  = 8;
                        mcuHeight = 16;
                        break;
                    case MFX_CHROMAFORMAT_YUV420:
                        mcuWidth = mcuHeight = 16;
                        break;
                    case MFX_CHROMAFORMAT_YUV400:
                        mcuWidth = mcuHeight = 8;
                        break;
                    default:
                        return 0;
                }

                numxMCU = (width  + (mcuWidth  - 1)) / mcuWidth;
                numyMCU = (height + (mcuHeight - 1)) / mcuHeight;
                
                numPieces += ((numxMCU * numyMCU + params.restart_interval - 1) / params.restart_interval) * numFields;
            }
            else
            {
                numPieces += 1;
            }
        }
        else
        {
            for(Ipp32u j=0; j<3; j++)
            {
                if(params.restart_interval)
                {
                    mcuWidth = 8;
                    mcuHeight = 8;

                    numxMCU = (width  + (mcuWidth  - 1)) / mcuWidth;
                    numyMCU = (height + (mcuHeight - 1)) / mcuHeight;

                    // U,V
                    if(j != 0)
                    {
                        switch(surface->Info.ChromaFormat)
                        {
                            case MFX_CHROMAFORMAT_YUV422H:
                            {
                                numxMCU = ((width >> 1) + (mcuWidth  - 1)) / mcuWidth;
                                break;
                            }
                            case MFX_CHROMAFORMAT_YUV422V:
                            {
                                numyMCU = ((height >> 1) + (mcuHeight - 1)) / mcuHeight;
                                break;
                            }
                            case MFX_CHROMAFORMAT_YUV420:
                            {
                                numxMCU = ((width >> 1) + (mcuWidth  - 1)) / mcuWidth;
                                numyMCU = ((height >> 1) + (mcuHeight - 1)) / mcuHeight;
                                break;
                            }
                        }
                    }
                    
                    numPieces += ((numxMCU * numyMCU + params.restart_interval - 1) / params.restart_interval) * numFields;
                }
                else
                {
                    numPieces += 1;
                }
            }
        }
    }

    return numPieces;

} // mfxStatus MJPEGEncodeTask::CalculateNumPieces(mfxFrameSurface1* surface, mfxFrameInfo* frameInfo)

mfxStatus MJPEGEncodeTask::EncodePiece(const mfxU32 threadNumber)
{
    mfxU32 pieceNum = 0;

    {
        UMC::AutomaticUMCMutex guard(m_guard);
        pieceNum = encodedPieces;

        if (pieceNum >= NumPiecesCollected())
        {
            return MFX_TASK_DONE;
        }

        encodedPieces++;
    }

    UMC::Status umc_sts = m_pMJPEGVideoEncoder.get()->EncodePiece(threadNumber, pieceNum);
    if(UMC::UMC_ERR_INVALID_PARAMS == umc_sts)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    if(UMC::UMC_OK != umc_sts)
    {
        return MFX_ERR_UNKNOWN;
    }
    
    return (pieceNum == NumPiecesCollected()) ? (MFX_TASK_DONE) : (MFX_TASK_WORKING);
}

mfxStatus MFXVideoENCODEMJPEG::MJPEGENCODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber)
{
    mfxStatus mfxRes = MFX_ERR_NONE;
    MFXVideoENCODEMJPEG &obj = *((MFXVideoENCODEMJPEG *) pState);
    MJPEGEncodeTask *pTask = (MJPEGEncodeTask*)pParam;

    mfxRes = obj.RunThread(*pTask, threadNumber, callNumber);
    MFX_CHECK_STS(mfxRes);

    return mfxRes;
}

mfxStatus MFXVideoENCODEMJPEG::MJPEGENCODECompleteProc(void *pState, void *pParam, mfxStatus /*taskRes*/)
{
    UMC::Status umc_sts = UMC::UMC_OK;
    MFXVideoENCODEMJPEG &obj = *((MFXVideoENCODEMJPEG *) pState);
    MJPEGEncodeTask *pTask = (MJPEGEncodeTask*)pParam;

    UMC::MediaData pDataOut;
    pDataOut.SetBufferPointer(pTask->bs->Data + pTask->bs->DataOffset + pTask->bs->DataLength, pTask->bs->MaxLength - pTask->bs->DataOffset - pTask->bs->DataLength);

    umc_sts = pTask->m_pMJPEGVideoEncoder.get()->PostProcessing(&pDataOut);
    if(UMC::UMC_OK != umc_sts)
    {
        if(UMC::UMC_ERR_NOT_ENOUGH_BUFFER == umc_sts)
        {
            return MFX_ERR_NOT_ENOUGH_BUFFER;
        }
        else
        {
            return MFX_ERR_UNKNOWN;
        }
    }

    pTask->bs->DataLength += (mfxU32)(pDataOut.GetDataSize());
    
    obj.m_frameCount++;

    if(pTask->bs->DataLength - pTask->m_initialDataLength) 
    {
        obj.m_encodedFrames++;
        obj.m_totalBits += (pTask->bs->DataLength - pTask->m_initialDataLength) * 8;
    }

    mfxFrameSurface1* surf = 0;
    surf = pTask->surface;
    if(surf)
    {
        pTask->bs->TimeStamp = surf->Data.TimeStamp;
        pTask->bs->DecodeTimeStamp = surf->Data.TimeStamp;
        obj.m_core->DecreaseReference(&(surf->Data));
    }

    pTask->Reset();

    {
        UMC::AutomaticUMCMutex guard(obj.m_guard);
        obj.m_freeTasks.push(pTask);
    }

    return MFX_ERR_NONE;
}

// check for known ExtBuffers, returns error code. or -1 if found unknown
// zero mfxExtBuffer* are OK
static mfxStatus CheckExtBuffers(mfxExtBuffer** ebuffers, mfxU32 nbuffers)
{
    mfxU32 ID_list[] = {MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
                        MFX_EXTBUFF_JPEG_HUFFMAN,
                        MFX_EXTBUFF_JPEG_QT};

    mfxU32 ID_found[sizeof(ID_list)/sizeof(ID_list[0])] = {0,};

    if (!ebuffers) return MFX_ERR_NONE;

    for(mfxU32 i=0; i<nbuffers; i++) 
    {
        bool is_known = false;
        if (!ebuffers[i]) 
        {
            return MFX_ERR_NULL_PTR; //continue;
        }
        for (mfxU32 j=0; j<sizeof(ID_list)/sizeof(ID_list[0]); j++)
        {
            if (ebuffers[i]->BufferId == ID_list[j]) 
            {
                if (ID_found[j])
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                is_known = true;
                ID_found[j] = 1; // to avoid duplicated
                break;
            }
        }
        if (!is_known)
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;

} // static mfxStatus CheckExtBuffers(mfxExtBuffer** ebuffers, mfxU32 nbuffers)

MFXVideoENCODEMJPEG::MFXVideoENCODEMJPEG(VideoCORE *core, mfxStatus *status) : VideoENCODE()
{
    m_core          = core;

    m_tasksCount = 0;
    pLastTask = NULL;

    m_totalBits      = 0;
    m_frameCountSync = 0;
    m_frameCount     = 0;
    m_encodedFrames  = 0;
    m_isInitialized  = false;
    m_useAuxInput    = false;
    //m_useSysOpaq     = false;
    //m_useVideoOpaq   = false;
    m_isOpaque       = false;

    *status = MFX_ERR_NONE;

    if(!m_core)
        *status =  MFX_ERR_MEMORY_ALLOC;

    memset(&m_vFirstParam, 0, sizeof(mfxVideoParam));
    memset(&m_vParam, 0, sizeof(mfxVideoParam));
}
MFXVideoENCODEMJPEG::~MFXVideoENCODEMJPEG(void)
{
    Close();
}

mfxStatus MFXVideoENCODEMJPEG::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    mfxU32 isCorrected = 0;
    mfxU32 isInvalid = 0;
    mfxStatus st;
    MFX_CHECK_NULL_PTR1(out)
    CHECK_VERSION(in->Version);
    CHECK_VERSION(out->Version);

    if(!in)
    {
        memset(&out->mfx, 0, sizeof(mfxInfoMFX));
        out->mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        out->mfx.FrameInfo.Width         = 1;
        out->mfx.FrameInfo.Height        = 1;
        out->mfx.FrameInfo.CropX         = 0;
        out->mfx.FrameInfo.CropY         = 0;
        out->mfx.FrameInfo.CropW         = 1;
        out->mfx.FrameInfo.CropH         = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.AspectRatioW  = 1;
        out->mfx.FrameInfo.AspectRatioH  = 1;
        out->mfx.FrameInfo.PicStruct     = 1;
        out->mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        out->mfx.CodecId                 = MFX_CODEC_JPEG;
        out->mfx.CodecLevel              = 0;
        out->mfx.CodecProfile            = MFX_PROFILE_JPEG_BASELINE;
        out->mfx.NumThread               = 1;
        out->mfx.Interleaved             = MFX_SCANTYPE_INTERLEAVED;
        out->mfx.Quality                 = 1;
        out->mfx.RestartInterval         = 0;
        out->AsyncDepth                  = 1;
        out->IOPattern                   = 1;
        out->Protected                   = 0;

        //Extended coding options
        st = CheckExtBuffers(out->ExtParam, out->NumExtParam);
        MFX_CHECK_STS(st);

        // Scene analysis info from VPP is not used in Query and Init
        mfxExtVppAuxData* optsSA = (mfxExtVppAuxData*)GetExtBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_VPP_AUXDATA);
        if(optsSA)
            return MFX_ERR_UNSUPPORTED;
    }
    else
    {
        // Check options for correctness
        if(in->mfx.CodecId != MFX_CODEC_JPEG)
            return MFX_ERR_UNSUPPORTED;

        // Check compatibility of input and output Ext buffers
        if ((in->NumExtParam != out->NumExtParam) || (in->NumExtParam &&((in->ExtParam == 0) != (out->ExtParam == 0))))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        st = CheckExtBuffers(in->ExtParam, in->NumExtParam);
        MFX_CHECK_STS(st);
        st = CheckExtBuffers(out->ExtParam, out->NumExtParam);
        MFX_CHECK_STS(st);

        // Check external buffers
        mfxExtJPEGQuantTables* qt_in  = (mfxExtJPEGQuantTables*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_JPEG_QT );
        mfxExtJPEGQuantTables* qt_out = (mfxExtJPEGQuantTables*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_JPEG_QT );
        mfxExtJPEGHuffmanTables* ht_in  = (mfxExtJPEGHuffmanTables*)GetExtBuffer( in->ExtParam, in->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
        mfxExtJPEGHuffmanTables* ht_out = (mfxExtJPEGHuffmanTables*)GetExtBuffer( out->ExtParam, out->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

        if ((qt_in == 0) != (qt_out == 0) ||
            (ht_in == 0) != (ht_out == 0))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if (qt_in && qt_out)
        {
            if(qt_in->NumTable <= 4)
            {
                qt_out->NumTable = qt_in->NumTable;
                for(mfxU16 i=0; i<qt_out->NumTable; i++)
                    for(mfxU16 j=0; j<64; j++)
                        qt_out->Qm[i][j] = qt_in->Qm[i][j];
            }
            else
            {
                qt_out->NumTable = 0;
                for(mfxU16 i=0; i<4; i++)
                    for(mfxU16 j=0; j<64; j++)
                        qt_out->Qm[i][j] = 0;
                isInvalid++;
            }
        }

        if (ht_in && ht_out)
        {
            if(ht_in->NumDCTable <= 4)
            {
                ht_out->NumDCTable = ht_in->NumDCTable;
                for(mfxU16 i=0; i<ht_out->NumDCTable; i++)
                {
                    for(mfxU16 j=0; j<16; j++)
                        ht_out->DCTables[i].Bits[j] = ht_in->DCTables[i].Bits[j];
                    for(mfxU16 j=0; j<12; j++)
                        ht_out->DCTables[i].Values[j] = ht_in->DCTables[i].Values[j];
                }
            }
            else
            {
                ht_out->NumDCTable = 0;
                for(mfxU16 i=0; i<4; i++)
                {
                    for(mfxU16 j=0; j<16; j++)
                        ht_out->DCTables[i].Bits[j] = 0;
                    for(mfxU16 j=0; j<12; j++)
                        ht_out->DCTables[i].Values[j] = 0;
                }
                isInvalid++;
            }

            if(ht_in->NumACTable <= 4)
            {
                ht_out->NumACTable = ht_in->NumACTable;
                for(mfxU16 i=0; i<ht_out->NumACTable; i++)
                {
                    for(mfxU16 j=0; j<16; j++)
                        ht_out->ACTables[i].Bits[j] = ht_in->ACTables[i].Bits[j];
                    for(mfxU16 j=0; j<162; j++)
                        ht_out->ACTables[i].Values[j] = ht_in->ACTables[i].Values[j];
                }
            }
            else
            {
                ht_out->NumACTable = 0;
                for(mfxU16 i=0; i<4; i++)
                {
                    for(mfxU16 j=0; j<16; j++)
                        ht_out->ACTables[i].Bits[j] = 0;
                    for(mfxU16 j=0; j<162; j++)
                        ht_out->ACTables[i].Values[j] = 0;
                }
                isInvalid++;
            }
        }

        mfxU32 fourCC = in->mfx.FrameInfo.FourCC;
        mfxU16 chromaFormat = in->mfx.FrameInfo.ChromaFormat;

        if ((fourCC == 0 && chromaFormat == 0) ||
            (fourCC == MFX_FOURCC_NV12 && (chromaFormat == MFX_CHROMAFORMAT_YUV420 || chromaFormat == MFX_CHROMAFORMAT_YUV400)) ||
            (fourCC == MFX_FOURCC_YV12 && (chromaFormat == MFX_CHROMAFORMAT_YUV420 || chromaFormat == MFX_CHROMAFORMAT_YUV400)) ||
            (fourCC == MFX_FOURCC_YUY2 && (chromaFormat == MFX_CHROMAFORMAT_YUV422H || chromaFormat == MFX_CHROMAFORMAT_YUV422V || chromaFormat == MFX_CHROMAFORMAT_YUV400)) ||
            (fourCC == MFX_FOURCC_RGB4 && chromaFormat == MFX_CHROMAFORMAT_YUV444))
        {
            out->mfx.FrameInfo.FourCC = in->mfx.FrameInfo.FourCC;
            out->mfx.FrameInfo.ChromaFormat = in->mfx.FrameInfo.ChromaFormat;
        }
        else
        {
            out->mfx.FrameInfo.FourCC = 0;
            out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            isInvalid++;
        }

        if (in->Protected != 0)
            return MFX_ERR_UNSUPPORTED;

        out->Protected = 0;
        out->AsyncDepth = in->AsyncDepth;

        //Check for valid height and width (no need to check max size because we support all 2-bytes values)
        if (in->mfx.FrameInfo.Width & 15)
        {
            out->mfx.FrameInfo.Width = 0;
            isInvalid ++;
        }
        else
            out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;

        if (in->mfx.FrameInfo.Height & ((in->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 15 : 31) )
        {
            out->mfx.FrameInfo.Height = 0;
            isInvalid ++;
        }
        else
            out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;

        //Check for valid framerate
        if((!in->mfx.FrameInfo.FrameRateExtN && in->mfx.FrameInfo.FrameRateExtD) ||
            (in->mfx.FrameInfo.FrameRateExtN && !in->mfx.FrameInfo.FrameRateExtD) ||
            (in->mfx.FrameInfo.FrameRateExtD && ((mfxF64)in->mfx.FrameInfo.FrameRateExtN / in->mfx.FrameInfo.FrameRateExtD) > 172)) 
        {
            isInvalid++;
            out->mfx.FrameInfo.FrameRateExtN = out->mfx.FrameInfo.FrameRateExtD = 0;
        }
        else
        {
            out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
            out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;
        }

        switch(in->IOPattern)
        {
            case 0:
            case MFX_IOPATTERN_IN_SYSTEM_MEMORY:
            case MFX_IOPATTERN_IN_VIDEO_MEMORY:
            case MFX_IOPATTERN_IN_OPAQUE_MEMORY:
                out->IOPattern = in->IOPattern;
                break;
            default:
                isCorrected++;
                if(in->IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                    out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
                else if(in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
                    out->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
                else
                    out->IOPattern = 0;
        }

        out->mfx.NumThread = in->mfx.NumThread;
        if(out->mfx.NumThread < 1)
            out->mfx.NumThread = (mfxU16)vm_sys_info_get_cpu_num();

#if defined (__ICL)
        //error #186: pointless comparison of unsigned integer with zero
#pragma warning(disable:186)
#endif

        // profile and level can be corrected
        switch(in->mfx.CodecProfile)
        {
            case MFX_PROFILE_JPEG_BASELINE:
            //case MFX_PROFILE_JPEG_EXTENDED:
            //case MFX_PROFILE_JPEG_PROGRESSIVE:
            //case MFX_PROFILE_JPEG_LOSSLESS:
            case MFX_PROFILE_UNKNOWN:
                out->mfx.CodecProfile = MFX_PROFILE_JPEG_BASELINE;
                break;
            default:
                isInvalid++;
                out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
                break;
        }

        // Check crops
        if (in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height && in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropH = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
        if (in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width && in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropW = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
        if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width) {
            out->mfx.FrameInfo.CropX = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
        if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height) {
            out->mfx.FrameInfo.CropY = 0;
            isInvalid ++;
        } else
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;

        out->mfx.FrameInfo.AspectRatioW = in->mfx.FrameInfo.AspectRatioW;
        out->mfx.FrameInfo.AspectRatioH = in->mfx.FrameInfo.AspectRatioH;

        if (in->mfx.Quality > 100)
        {
            out->mfx.Quality = 100;
            isCorrected++;
        }
        else
        {
            out->mfx.Quality = in->mfx.Quality;
        }

        switch (in->mfx.FrameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_UNKNOWN:
            case MFX_PICSTRUCT_PROGRESSIVE:
            case MFX_PICSTRUCT_FIELD_TFF:
            case MFX_PICSTRUCT_FIELD_BFF:
            //case MFX_PICSTRUCT_FIELD_REPEATED:
            //case MFX_PICSTRUCT_FRAME_DOUBLING:
            //case MFX_PICSTRUCT_FRAME_TRIPLING:
                out->mfx.FrameInfo.PicStruct = in->mfx.FrameInfo.PicStruct;
                break;
            default:
                isInvalid++;
                out->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
                break;
        }
    }

    if(isInvalid)
        return MFX_ERR_UNSUPPORTED;

    if(isCorrected)
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_NULL_PTR1(request)

    // check for valid IOPattern
    mfxU16 IOPatternIn = par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_IN_OPAQUE_MEMORY);
    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0) ||
        ((IOPatternIn != MFX_IOPATTERN_IN_VIDEO_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_SYSTEM_MEMORY) && (IOPatternIn != MFX_IOPATTERN_IN_OPAQUE_MEMORY)))
    {
       return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (par->Protected != 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    request->NumFrameSuggested = request->NumFrameMin = par->AsyncDepth ? par->AsyncDepth : 1;
    request->Info              = par->mfx.FrameInfo;

    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
    }
    else if(par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) 
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    } 
    else 
    {
        request->Type = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
    }

    return (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::Init(mfxVideoParam *par_in)
{
    mfxStatus st = MFX_ERR_NONE, QueryStatus = MFX_ERR_NONE;
    mfxVideoParam* par = par_in;

    if(m_isInitialized)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(par == NULL)
        return MFX_ERR_NULL_PTR;

    MFX_CHECK(CheckExtBuffers(par->ExtParam, par->NumExtParam)== MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    mfxExtJPEGQuantTables*    jpegQT       = (mfxExtJPEGQuantTables*)   GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT       = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    mfxVideoParam checked;
    mfxU16 ext_counter = 0;
    checked = *par;

    if (jpegQT)
    {
        m_checkedJpegQT = *jpegQT;
        m_pCheckedExt[ext_counter++] = &m_checkedJpegQT.Header;
    } 
    else 
    {
        memset(&m_checkedJpegQT, 0, sizeof(m_checkedJpegQT));
        m_checkedJpegQT.Header.BufferId = MFX_EXTBUFF_JPEG_QT;
        m_checkedJpegQT.Header.BufferSz = sizeof(m_checkedJpegQT);
    }
    if (jpegHT)
    {
        m_checkedJpegHT = *jpegHT;
        m_pCheckedExt[ext_counter++] = &m_checkedJpegHT.Header;
    } 
    else 
    {
        memset(&m_checkedJpegHT, 0, sizeof(m_checkedJpegHT));
        m_checkedJpegHT.Header.BufferId = MFX_EXTBUFF_JPEG_HUFFMAN;
        m_checkedJpegHT.Header.BufferSz = sizeof(m_checkedJpegHT);
    }
    if (opaqAllocReq) 
    {
        m_checkedOpaqAllocReq = *opaqAllocReq;
        m_pCheckedExt[ext_counter++] = &m_checkedOpaqAllocReq.Header;
    }
    checked.ExtParam = m_pCheckedExt;
    checked.NumExtParam = ext_counter;

    QueryStatus = Query(par, &checked);

    if (QueryStatus != MFX_ERR_NONE && QueryStatus != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && QueryStatus != MFX_WRN_PARTIAL_ACCELERATION)
    {
        if (QueryStatus == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return QueryStatus;
    }

    par = &checked; // from now work with fixed copy of input!

    if (opaqAllocReq)
        opaqAllocReq = &m_checkedOpaqAllocReq;

    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    m_isOpaque = false;
    if (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        if (opaqAllocReq == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
        {
            // check memory type in opaque allocation request
            switch (opaqAllocReq->In.Type & (MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET))
            {
            case MFX_MEMTYPE_SYSTEM_MEMORY:
            case MFX_MEMTYPE_DXVA2_DECODER_TARGET:
            case MFX_MEMTYPE_DXVA2_PROCESSOR_TARGET:
                break;
            default:
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }

            // use opaque surfaces. Need to allocate
            m_isOpaque = true;
        }
    }

    // return an error if requested opaque memory type isn't equal to native
/* outdated, SW encoder can receive opaque with video mem, typically from HW VPP.
    if (m_isOpaque)
        if ((opaqAllocReq->In.Type & MFX_MEMTYPE_FROM_ENCODE) && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            return MFX_ERR_INVALID_VIDEO_PARAM;
*/
    // Allocate Opaque frames and frame for copy from video memory (if any)
    //memset(&m_auxInput, 0, sizeof(m_auxInput));
    m_useAuxInput = false;
    //m_useSysOpaq = false;
    //m_useVideoOpaq = false;
    if (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY || m_isOpaque) 
    {
        bool bOpaqVideoMem = m_isOpaque && !(opaqAllocReq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
        bool bNeedAuxInput = (par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY) || bOpaqVideoMem;
        
        mfxFrameAllocRequest request;
        memset(&request, 0, sizeof(request));
        request.Info = par->mfx.FrameInfo;

        //// try to allocate opaque surfaces in video memory for another component in transcoding chain
        //if (bOpaqVideoMem) 
        //{
        //    memset(&m_response_alien, 0, sizeof(m_response_alien));
        //    request.Type =  (mfxU16)opaqAllocReq->In.Type;
        //    request.NumFrameMin =request.NumFrameSuggested = (mfxU16)opaqAllocReq->In.NumSurface;

        //    st = m_core->AllocFrames(&request, &m_response_alien, opaqAllocReq->In.Surfaces, opaqAllocReq->In.NumSurface);

        //    if (MFX_ERR_NONE != st && MFX_ERR_UNSUPPORTED != st) // unsupported means that current Core couldn;t allocate the surfaces
        //        return st;

        //    if (m_response_alien.NumFrameActual < request.NumFrameMin)
        //        return MFX_ERR_MEMORY_ALLOC;

        //    if (st != MFX_ERR_UNSUPPORTED)
        //        m_useVideoOpaq = true;
        //}

        // allocate all we need in system memory
        memset(&m_response, 0, sizeof(m_response));
        if (bNeedAuxInput) 
        {
            // allocate additional surface in system memory for FastCopy from video memory
            request.Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
            request.NumFrameSuggested = request.NumFrameMin = par->AsyncDepth ? par->AsyncDepth : m_core->GetAutoAsyncDepth();
            st = m_core->AllocFrames(&request, &m_response);
            MFX_CHECK_STS(st);

            if (m_response.NumFrameActual < request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;
        } 
        //else 
        //{
        //    // allocate opaque surfaces in system memory
        //    request.Type =  (mfxU16)opaqAllocReq->In.Type;
        //    request.NumFrameMin       = opaqAllocReq->In.NumSurface;
        //    request.NumFrameSuggested = opaqAllocReq->In.NumSurface;
        //    st = m_core->AllocFrames(&request, &m_response, opaqAllocReq->In.Surfaces, opaqAllocReq->In.NumSurface);
        //    MFX_CHECK_STS(st);
        //}

        if (bNeedAuxInput) 
        {
            m_useAuxInput = true;
            //m_auxInput.Data.MemId = m_response.mids[0];
            //m_auxInput.Info = request.Info;
        }
        //else
        //    m_useSysOpaq = true;

        //st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
        //MFX_CHECK_STS(st);
    }

    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0 ||
        par->mfx.FrameInfo.FrameRateExtN == 0 ||
        par->mfx.FrameInfo.FrameRateExtD == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if(par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    //Reset frame count
    m_frameCountSync = 0;
    m_frameCount = 0;

    m_tasksCount = 0;
    pLastTask = NULL;

    m_vFirstParam = *par;

    m_vParam = m_vFirstParam;

    if (!par->mfx.FrameInfo.FrameRateExtD || !par->mfx.FrameInfo.FrameRateExtN) 
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU32 DoubleBytesPerPx = 0;
    switch(m_vParam.mfx.FrameInfo.FourCC)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
            DoubleBytesPerPx = 3;
            break;
        case MFX_FOURCC_YUY2:
            DoubleBytesPerPx = 4;
            break;
        case MFX_FOURCC_RGB4:
        default:
            DoubleBytesPerPx = 8;
            break;
    }

    memset(&m_umcVideoParams, 0, sizeof(UMC::MJPEGEncoderParams));

    m_umcVideoParams.profile               = m_vParam.mfx.CodecProfile;
    m_umcVideoParams.quality               = m_vParam.mfx.Quality;
    m_umcVideoParams.numThreads            = UMC::JPEG_ENC_MAX_THREADS;
    m_umcVideoParams.chroma_format         = m_vParam.mfx.FrameInfo.ChromaFormat;
    m_umcVideoParams.info.clip_info.width  = m_vParam.mfx.FrameInfo.Width;
    m_umcVideoParams.info.clip_info.height = m_vParam.mfx.FrameInfo.Height;
    m_umcVideoParams.buf_size              = 16384 + m_vParam.mfx.FrameInfo.Width * m_vParam.mfx.FrameInfo.Height * DoubleBytesPerPx / 2;
    m_umcVideoParams.restart_interval      = m_vParam.mfx.RestartInterval;
    m_umcVideoParams.interleaved           = (m_vParam.mfx.Interleaved == MFX_SCANTYPE_INTERLEAVED) ? 1 : 0;

    switch(m_vParam.mfx.FrameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
            m_umcVideoParams.info.interlace_type = UMC::PROGRESSIVE;
            break;
        case MFX_PICSTRUCT_FIELD_TFF:
            m_umcVideoParams.info.interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;
            break;
        case MFX_PICSTRUCT_FIELD_BFF:
            m_umcVideoParams.info.interlace_type = UMC::INTERLEAVED_BOTTOM_FIELD_FIRST;
            break;
    }

    if (st == MFX_ERR_NONE)
    {
        m_isInitialized = true;
        return (QueryStatus != MFX_ERR_NONE) ? QueryStatus : (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
        return st;
}

mfxStatus MFXVideoENCODEMJPEG::Reset(mfxVideoParam *par)
{
    mfxStatus st, QueryStatus;

    if(!m_isInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    if(par == NULL)
        return MFX_ERR_NULL_PTR;

    MFX_CHECK(CheckExtBuffers(par->ExtParam, par->NumExtParam)== MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);

    mfxExtJPEGQuantTables*    jpegQT       = (mfxExtJPEGQuantTables*)   GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT       = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    mfxExtOpaqueSurfaceAlloc* opaqAllocReq = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION );

    mfxVideoParam checked;
    mfxU16 ext_counter = 0;
    checked = *par;

    if (jpegQT)
    {
        m_checkedJpegQT = *jpegQT;
        m_pCheckedExt[ext_counter++] = &m_checkedJpegQT.Header;
    } 
    else 
    {
        memset(&m_checkedJpegQT, 0, sizeof(m_checkedJpegQT));
        m_checkedJpegQT.Header.BufferId = MFX_EXTBUFF_JPEG_QT;
        m_checkedJpegQT.Header.BufferSz = sizeof(m_checkedJpegQT);
    }
    if (jpegHT)
    {
        m_checkedJpegHT = *jpegHT;
        m_pCheckedExt[ext_counter++] = &m_checkedJpegHT.Header;
    } 
    else 
    {
        memset(&m_checkedJpegHT, 0, sizeof(m_checkedJpegHT));
        m_checkedJpegHT.Header.BufferId = MFX_EXTBUFF_JPEG_HUFFMAN;
        m_checkedJpegHT.Header.BufferSz = sizeof(m_checkedJpegHT);
    }
    if (opaqAllocReq) 
    {
        m_checkedOpaqAllocReq = *opaqAllocReq;
        m_pCheckedExt[ext_counter++] = &m_checkedOpaqAllocReq.Header;
    }
    checked.ExtParam = m_pCheckedExt;
    checked.NumExtParam = ext_counter;

    st = Query(par, &checked);

    if (st != MFX_ERR_NONE && st != MFX_WRN_INCOMPATIBLE_VIDEO_PARAM && st != MFX_WRN_PARTIAL_ACCELERATION)
    {
        if (st == MFX_ERR_UNSUPPORTED)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        else
            return st;
    }

    QueryStatus = st;

    par = &checked; // from now work with fixed copy of input!
    
    if (par->mfx.FrameInfo.Width == 0 || par->mfx.FrameInfo.Height == 0 ||
        par->mfx.FrameInfo.FrameRateExtN == 0 ||
        par->mfx.FrameInfo.FrameRateExtD == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if ((par->IOPattern & 0xffc8) || (par->IOPattern == 0)) // 0 is possible after Query
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!m_core->IsExternalFrameAllocator() && (par->IOPattern & (MFX_IOPATTERN_OUT_VIDEO_MEMORY | MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // checks for opaque memory
    if (!(m_vFirstParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) && (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY))
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    if(par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE && 
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_TFF &&
       par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_FIELD_BFF)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // check that new params don't require allocation of additional memory
    if (par->mfx.FrameInfo.Width > m_vFirstParam.mfx.FrameInfo.Width ||
        par->mfx.FrameInfo.Height > m_vFirstParam.mfx.FrameInfo.Height ||
        m_vFirstParam.mfx.FrameInfo.FourCC != par->mfx.FrameInfo.FourCC ||
        m_vFirstParam.mfx.FrameInfo.ChromaFormat != par->mfx.FrameInfo.ChromaFormat)
        return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;


    //Reset frame count
    m_frameCountSync = 0;
    m_frameCount = 0;

    m_tasksCount = 0;
    pLastTask = NULL;
    while(!m_freeTasks.empty())
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        delete m_freeTasks.front();
        m_freeTasks.pop();
    }

    m_vParam.mfx = par->mfx;

    m_vParam.IOPattern = par->IOPattern;
    m_vParam.Protected = 0;
    
    if(par->AsyncDepth != m_vFirstParam.AsyncDepth)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (!par->mfx.FrameInfo.FrameRateExtD || !par->mfx.FrameInfo.FrameRateExtN) 
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mfxU32 DoubleBytesPerPx = 0;
    switch(m_vParam.mfx.FrameInfo.FourCC)
    {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_YV12:
            DoubleBytesPerPx = 3;
            break;
        case MFX_FOURCC_YUY2:
            DoubleBytesPerPx = 4;
            break;
        case MFX_FOURCC_RGB4:
        default:
            DoubleBytesPerPx = 8;
            break;
    }

    memset(&m_umcVideoParams, 0, sizeof(UMC::MJPEGEncoderParams));

    m_umcVideoParams.profile               = m_vParam.mfx.CodecProfile;
    m_umcVideoParams.quality               = m_vParam.mfx.Quality;
    m_umcVideoParams.numThreads            = UMC::JPEG_ENC_MAX_THREADS;
    m_umcVideoParams.chroma_format         = m_vParam.mfx.FrameInfo.ChromaFormat;
    m_umcVideoParams.info.clip_info.width  = m_vParam.mfx.FrameInfo.Width;
    m_umcVideoParams.info.clip_info.height = m_vParam.mfx.FrameInfo.Height;
    m_umcVideoParams.buf_size              = 16384 + m_vParam.mfx.FrameInfo.Width * m_vParam.mfx.FrameInfo.Height * DoubleBytesPerPx / 2;
    m_umcVideoParams.restart_interval      = m_vParam.mfx.RestartInterval;
    m_umcVideoParams.interleaved           = (m_vParam.mfx.Interleaved == MFX_SCANTYPE_INTERLEAVED) ? 1 : 0;

    switch(m_vParam.mfx.FrameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
            m_umcVideoParams.info.interlace_type = UMC::PROGRESSIVE;
            break;
        case MFX_PICSTRUCT_FIELD_TFF:
            m_umcVideoParams.info.interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;
            break;
        case MFX_PICSTRUCT_FIELD_BFF:
            m_umcVideoParams.info.interlace_type = UMC::INTERLEAVED_BOTTOM_FIELD_FIRST;
            break;
    }

    if (st == MFX_ERR_NONE)
    {
        m_isInitialized = true;
        return (QueryStatus != MFX_ERR_NONE) ? QueryStatus : (IsHWLib())? MFX_WRN_PARTIAL_ACCELERATION : MFX_ERR_NONE;
    }
    else
        return st;
}

mfxStatus MFXVideoENCODEMJPEG::Close(void)
{
    mfxStatus       MFXSts = MFX_ERR_NONE;

    m_tasksCount = 0;
    pLastTask = NULL;

    // delete free tasks queue
    while (false == m_freeTasks.empty())
    {
        UMC::AutomaticUMCMutex guard(m_guard);
        delete m_freeTasks.front();
        m_freeTasks.pop();
    }

    if(m_useAuxInput && m_response.NumFrameActual)
    {
        m_core->FreeFrames(&m_response);
        m_response.NumFrameActual = 0;
    }

    return MFXSts;
}

mfxStatus MFXVideoENCODEMJPEG::GetVideoParam(mfxVideoParam *par)
{
    UMC::Status umcRes = UMC::UMC_OK;
    UMC::MJPEGVideoEncoder* pMJPEGVideoEncoder;

    if(!m_isInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par)
    CHECK_VERSION(par->Version);

    if (0 > CheckExtBuffers( par->ExtParam, par->NumExtParam))
        return MFX_ERR_UNSUPPORTED;

    mfxExtJPEGQuantTables* jpegQT = (mfxExtJPEGQuantTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_QT );
    mfxExtJPEGHuffmanTables*  jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( par->ExtParam, par->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );

    // copy structures and extCodingOption if exist in dst
    par->mfx = m_vParam.mfx;
    par->Protected = 0;
    par->AsyncDepth = m_vParam.AsyncDepth;
    par->IOPattern = m_vParam.IOPattern;

    if(jpegQT || jpegHT)
    {
        if(!pLastTask)
            return MFX_ERR_UNSUPPORTED;

        pMJPEGVideoEncoder = pLastTask->m_pMJPEGVideoEncoder.get();
        
        if(jpegQT)
        {
            umcRes = pMJPEGVideoEncoder->FillQuantTableExtBuf(jpegQT);
            if (umcRes != UMC::UMC_OK)
                return MFX_ERR_NOT_INITIALIZED;
        }

        if(jpegHT)
        {
            umcRes = pMJPEGVideoEncoder->FillHuffmanTableExtBuf(jpegHT);
            if (umcRes != UMC::UMC_OK)
                return MFX_ERR_NOT_INITIALIZED;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::GetFrameParam(mfxFrameParam *par)
{
    if(!m_isInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(par)
    *par = m_mfxFrameParam;

    return MFX_ERR_NONE;
}

mfxFrameSurface1* MFXVideoENCODEMJPEG::GetOriginalSurface(mfxFrameSurface1 *surface)
{
    if (m_isOpaque)
        return m_core->GetNativeSurface(surface);
    else
        return surface;
}

mfxStatus MFXVideoENCODEMJPEG::RunThread(MJPEGEncodeTask &task, mfxU32 threadNumber, mfxU32 callNumber)
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    UMC::MJPEGEncoderParams info;
    task.m_pMJPEGVideoEncoder.get()->GetInfo(&info);
    mfxU32 numFields = ((info.info.interlace_type == UMC::PROGRESSIVE) ? 1 : 2);

    if(callNumber == 0)
    {
        mfxFrameSurface1 *surface = task.surface;

        if (m_useAuxInput)
        {
            mfxRes = m_core->LockFrame(task.auxInput.Data.MemId, &task.auxInput.Data);
            MFX_CHECK_STS(mfxRes);

            mfxRes = m_core->DoFastCopyWrapper(&task.auxInput,
                                               MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                                               surface,
                                               MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);

            MFX_CHECK_STS(mfxRes);

            if (task.auxInput.Data.Y)
            {
                if (((task.auxInput.Info.FourCC == MFX_FOURCC_YV12) && (!task.auxInput.Data.U || !task.auxInput.Data.V)) ||
                    ((task.auxInput.Info.FourCC == MFX_FOURCC_NV12) && !task.auxInput.Data.UV))
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                
                mfxU32 pitch = task.auxInput.Data.PitchLow + ((mfxU32)task.auxInput.Data.PitchHigh << 16);
                if (!pitch)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            else
            {
                if ((task.auxInput.Info.FourCC == MFX_FOURCC_YV12 && (task.auxInput.Data.U || task.auxInput.Data.V)) ||
                    (task.auxInput.Info.FourCC == MFX_FOURCC_NV12 && task.auxInput.Data.UV))
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }

            mfxRes = task.AddSource(&task.auxInput, &(m_vParam.mfx.FrameInfo), false);
            MFX_CHECK_STS(mfxRes);

            mfxRes = m_core->UnlockFrame(task.auxInput.Data.MemId, &task.auxInput.Data);
            MFX_CHECK_STS(mfxRes);
        }
        else if (surface->Data.Y == 0 && surface->Data.U == 0 && surface->Data.V == 0 && surface->Data.A == 0)
        {
            mfxRes = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
            MFX_CHECK_STS(mfxRes);

            if (surface->Data.Y)
            {
                if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V)) ||
                    (surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV))
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
                
                mfxU32 pitch = surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16);
                if (!pitch)
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            else
            {
                if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (surface->Data.U || surface->Data.V)) ||
                    (surface->Info.FourCC == MFX_FOURCC_NV12 && surface->Data.UV))
                {
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            
            mfxRes = task.AddSource(task.surface, &(m_vParam.mfx.FrameInfo), true);
            MFX_CHECK_STS(mfxRes);
            
            mfxRes = m_core->UnlockExternalFrame(task.surface->Data.MemId, &task.surface->Data);
            MFX_CHECK_STS(mfxRes);
        }
        else
        {
            mfxRes = task.AddSource(surface, &(m_vParam.mfx.FrameInfo), false);
            MFX_CHECK_STS(mfxRes);
        }
    }
    else if(task.m_pMJPEGVideoEncoder.get()->NumPicsCollected() != numFields)
    {
        return MFX_TASK_WORKING;
    }
    
    return task.EncodePiece(threadNumber);
}

mfxStatus MFXVideoENCODEMJPEG::EncodeFrame(mfxEncodeCtrl *, mfxEncodeInternalParams *, mfxFrameSurface1* /*inputSurface*/, mfxBitstream* /*bs*/)
{
#if 0    
    mfxFrameSurface1 *surface = inputSurface;
    mfxStatus st;
    mfxU32 minBufferSize;
    mfxU32 initialDataLength = 0;
    UMC::Status     Sts = UMC::UMC_OK;
    bool locked = false;

    if(!m_isInitialized || !m_pEncoder)
        return MFX_ERR_NOT_INITIALIZED;

    if(bs)
    {
        initialDataLength = bs->DataLength;

        if (m_vParam.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            minBufferSize = 0;
        else
            minBufferSize = 0;

        if(bs->MaxLength - (bs->DataOffset + bs->DataLength) < minBufferSize && m_vParam.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
            return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if (m_isOpaque)
        m_core->GetNativeSurface(surface);

    if (inputSurface != surface)
    {
        // input surface is opaque surface
        surface->Data.FrameOrder = inputSurface->Data.FrameOrder;
        surface->Data.TimeStamp  = inputSurface->Data.TimeStamp;
        surface->Data.Corrupted  = inputSurface->Data.Corrupted;
        surface->Data.DataFlag   = inputSurface->Data.DataFlag;
        surface->Info            = inputSurface->Info;
    }

    if(surface)
    {
        if(m_useAuxInput)
        {
            // copy from d3d to internal frame in system memory
            // Lock internal. FastCopy to use locked, to avoid additional lock/unlock
            st = m_core->LockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
            MFX_CHECK_STS(st);

            st = m_core->DoFastCopyWrapper(&m_auxInput,
                MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_SYSTEM_MEMORY,
                surface,
                MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_DXVA2_DECODER_TARGET);
            MFX_CHECK_STS(st);

            m_auxInput.Data.FrameOrder = surface->Data.FrameOrder;
            m_auxInput.Data.TimeStamp = surface->Data.TimeStamp;
            surface = &m_auxInput; // replace input pointer
        }
        else
        {
            if (surface->Data.Y == 0 && surface->Data.U == 0 && surface->Data.V == 0 && surface->Data.A == 0)
            {
                st = m_core->LockExternalFrame(surface->Data.MemId, &(surface->Data));
                if (st == MFX_ERR_NONE)
                    locked = true;
                else
                    return st;
            }
        }

        if (!surface->Data.Y || surface->Data.Pitch > 0x7fff || surface->Data.Pitch < m_vParam.mfx.FrameInfo.Width || !surface->Data.Pitch ||
            surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV ||
            surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V) )
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    if(surface)
    {
        mfxU32  Width       = m_vParam.mfx.FrameInfo.Width;
        mfxU32  Height      = m_vParam.mfx.FrameInfo.Height;
        mfxU32  ChromaPitch = 0;

        if(surface->Info.FourCC == MFX_FOURCC_NV12)
        {
            ChromaPitch = surface->Data.Pitch;
            pDataIn->Init(Width, Height, UMC::NV12, 8);

            pDataIn->SetPlanePointer(surface->Data.Y, 0);
            pDataIn->SetPlanePitch(surface->Data.Pitch, 0);
            pDataIn->SetPlanePointer(surface->Data.UV, 1);
            pDataIn->SetPlanePitch(ChromaPitch, 1);
        }
        else if(surface->Info.FourCC == MFX_FOURCC_RGB3)
        {
            ChromaPitch = surface->Data.Pitch;
            pDataIn->Init(Width, Height, UMC::RGB24, 8);

            pDataIn->SetPlanePointer(surface->Data.R, 0);
            pDataIn->SetPlanePitch(surface->Data.Pitch, 0);
        }
        else if(surface->Info.FourCC == MFX_FOURCC_RGB4)
        {
            ChromaPitch = surface->Data.Pitch;
            pDataIn->Init(Width, Height, UMC::RGB32, 8);

            pDataIn->SetPlanePointer(surface->Data.R, 0);
            pDataIn->SetPlanePitch(surface->Data.Pitch, 0);
        }
        else
        {
            ChromaPitch = surface->Data.Pitch >> 1;
            pDataIn->Init(Width, Height, UMC::YUV420, 8);

            pDataIn->SetPlanePointer(surface->Data.Y, 0);
            pDataIn->SetPlanePitch(surface->Data.Pitch, 0);
            pDataIn->SetPlanePointer(surface->Data.U, 1);
            pDataIn->SetPlanePitch(ChromaPitch, 1);
            pDataIn->SetPlanePointer(surface->Data.V, 2);
            pDataIn->SetPlanePitch(ChromaPitch, 2);
        }

        pDataIn->SetTime((Ipp64f)surface->Data.TimeStamp);
    }
    else
        return MFX_ERR_NONE;

    pDataOut->SetBufferPointer(bs->Data + bs->DataOffset, bs->MaxLength - bs->DataOffset);

    Sts = m_pEncoder->GetFrame(pDataIn, pDataOut);

    bs->DataLength += (mfxU32)pDataOut->GetDataSize();
    m_frameCount++;
    if(bs->DataLength - initialDataLength) 
    {
        m_encodedFrames++;
        m_totalBits += (bs->DataLength - initialDataLength) * 8;
    }

    if (m_useAuxInput)
        m_core->UnlockFrame(m_auxInput.Data.MemId, &m_auxInput.Data);
    else
    {
        if(locked)
            m_core->UnlockExternalFrame(surface->Data.MemId, &(surface->Data));
    }

    mfxFrameSurface1* surf = 0;
    surf = inputSurface;
    if(surf)
    {
        bs->TimeStamp = surf->Data.TimeStamp;
        m_core->DecreaseReference(&(surf->Data));
    }
#endif
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::GetEncodeStat(mfxEncodeStat *stat)
{
    if(!m_isInitialized)
        return MFX_ERR_NOT_INITIALIZED;

    MFX_CHECK_NULL_PTR1(stat)
    memset(stat, 0, sizeof(mfxEncodeStat));
    stat->NumCachedFrame = 0;
    stat->NumBit = m_totalBits;
    stat->NumFrame = m_encodedFrames;

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams)
{
    pInternalParams;
    mfxU32    minBufferSize = 0;
    mfxStatus sts = MFX_ERR_NONE;
    UMC::Status umc_sts = UMC::UMC_OK;

    mfxExtJPEGQuantTables*   jpegQT = NULL;
    mfxExtJPEGHuffmanTables* jpegHT = NULL;
    UMC::MJPEGVideoEncoder*  pMJPEGVideoEncoder = NULL;

    if(!m_isInitialized) return MFX_ERR_NOT_INITIALIZED;
    if (bs == 0) return MFX_ERR_NULL_PTR;

    {
        // make sure that there is a free task
        if (m_freeTasks.empty())
        {
            if(m_tasksCount >= (m_vParam.AsyncDepth ? m_vParam.AsyncDepth : m_core->GetAutoAsyncDepth()))
            {
                return MFX_WRN_DEVICE_BUSY;
            }

            std::auto_ptr<MJPEGEncodeTask> pTask(new MJPEGEncodeTask());

            if (NULL == pTask.get())
            {
                return MFX_ERR_MEMORY_ALLOC;
            }

            // initialize the task
            sts = pTask.get()->Initialize(m_umcVideoParams);
            if (MFX_ERR_NONE != sts)
            {
                return sts;
            }

            if(m_useAuxInput)
            {
                memset(&pTask->auxInput, 0, sizeof(pTask->auxInput));
                pTask->auxInput.Data.MemId = m_response.mids[m_tasksCount];
                pTask->auxInput.Info = m_vParam.mfx.FrameInfo;
            }

            // Set Quant/Huffman tables if they were passed by ext buffer in init/reset
            {
                if (m_vParam.ExtParam && m_vParam.NumExtParam > 0)
                {
                    jpegQT = (mfxExtJPEGQuantTables*)   GetExtBuffer( m_vParam.ExtParam, m_vParam.NumExtParam, MFX_EXTBUFF_JPEG_QT );
                    jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( m_vParam.ExtParam, m_vParam.NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
                }

                pMJPEGVideoEncoder = pTask.get()->m_pMJPEGVideoEncoder.get();

                if(jpegQT)
                {
                    umc_sts = pMJPEGVideoEncoder->SetQuantTableExtBuf(jpegQT);
                }

                if(jpegHT)
                {
                    umc_sts = pMJPEGVideoEncoder->SetHuffmanTableExtBuf(jpegHT);
                }

                if(umc_sts != UMC::UMC_OK)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;
            }

            m_tasksCount++;

            // save the task object into the queue
            {
                UMC::AutomaticUMCMutex guard(m_guard);
                m_freeTasks.push(pTask.release());
            }
        }
    }


    //Check for enough bitstream buffer size
    if( bs->MaxLength < (bs->DataOffset + bs->DataLength))
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if( bs->MaxLength - (bs->DataOffset + bs->DataLength) < minBufferSize || bs->MaxLength - bs->DataOffset == 0)
    {
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if(bs->Data == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    if(bs)
    {
        m_freeTasks.front()->m_initialDataLength = bs->DataLength;
    }

    if (surface) 
    { // check frame parameters

        if (//surface->Info.Width < enc->m_info.info.clip_info.width ||
            //surface->Info.Height < enc->m_info.info.clip_info.height ||
            //surface->Info.CropW > 0 && surface->Info.CropW != enc->m_info.info.clip_info.width ||
            //surface->Info.CropH > 0 && surface->Info.CropH != enc->m_info.info.clip_info.height ||
            surface->Info.ChromaFormat != m_vParam.mfx.FrameInfo.ChromaFormat)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (surface->Info.Width != m_vParam.mfx.FrameInfo.Width ||
            surface->Info.Height != m_vParam.mfx.FrameInfo.Height)
        {
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (surface->Data.Y)
        {
            if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (!surface->Data.U || !surface->Data.V)) ||
                (surface->Info.FourCC == MFX_FOURCC_NV12 && !surface->Data.UV))
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            
            mfxU32 pitch = surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16);
            if (!pitch)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else
        {
            if ((surface->Info.FourCC == MFX_FOURCC_YV12 && (surface->Data.U || surface->Data.V)) ||
                (surface->Info.FourCC == MFX_FOURCC_NV12 && surface->Data.UV))
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
    }

    *reordered_surface = surface;

    if (ctrl && (ctrl->FrameType != MFX_FRAMETYPE_UNKNOWN) && ((ctrl->FrameType & 0xff) != (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR)))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (ctrl && ctrl->ExtParam && ctrl->NumExtParam > 0)
    {
        jpegQT = (mfxExtJPEGQuantTables*)   GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_QT );
        jpegHT = (mfxExtJPEGHuffmanTables*) GetExtBuffer( ctrl->ExtParam, ctrl->NumExtParam, MFX_EXTBUFF_JPEG_HUFFMAN );
    }

    pMJPEGVideoEncoder = m_freeTasks.front()->m_pMJPEGVideoEncoder.get();

    if(jpegQT)
    {
        umc_sts = pMJPEGVideoEncoder->SetQuantTableExtBuf(jpegQT);
    }
    else if(!pMJPEGVideoEncoder->IsQuantTableInited())
    {
        umc_sts = pMJPEGVideoEncoder->SetDefaultQuantTable(m_vParam.mfx.Quality);
    }

    if(umc_sts != UMC::UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    if(jpegHT)
    {
        umc_sts = pMJPEGVideoEncoder->SetHuffmanTableExtBuf(jpegHT);
    }
    else if(!pMJPEGVideoEncoder->IsHuffmanTableInited())
    {
        umc_sts = pMJPEGVideoEncoder->SetDefaultHuffmanTable();
    }

    if(umc_sts != UMC::UMC_OK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    m_frameCountSync++;
    if (!surface) 
    {
        return MFX_ERR_MORE_DATA;
    }

    return MFX_ERR_NONE;
}

mfxStatus MFXVideoENCODEMJPEG::EncodeFrameCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxFrameSurface1 **reordered_surface, mfxEncodeInternalParams *pInternalParams, MFX_ENTRY_POINT *pEntryPoint)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1 *pOriginalSurface;

    MFX::AutoTimer timer("MJPEGEnc_check");

    sts = EncodeFrameCheck(ctrl, surface, bs, reordered_surface, pInternalParams);

    pEntryPoint->pState = this;
    pEntryPoint->pRoutine = MJPEGENCODERoutine;
    pEntryPoint->pCompleteProc = MJPEGENCODECompleteProc;//TaskCompleteProc;
    pEntryPoint->pRoutineName = (char *)"EncodeMJPEG";

    pOriginalSurface = GetOriginalSurface(surface);

    // input surface is opaque surface
    if (pOriginalSurface != surface)
    {
        if (pOriginalSurface == 0)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        pOriginalSurface->Info = surface->Info;
        pOriginalSurface->Data.Corrupted = surface->Data.Corrupted;
        pOriginalSurface->Data.DataFlag = surface->Data.DataFlag;
        pOriginalSurface->Data.TimeStamp = surface->Data.TimeStamp;
        pOriginalSurface->Data.FrameOrder = surface->Data.FrameOrder;
    }

    if (surface && !pOriginalSurface)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (MFX_ERR_NONE == sts || (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_MORE_BITSTREAM == sts)
    {
        // lock surface. If input surface is opaque core will lock both opaque and associated realSurface
        if (pOriginalSurface) 
        {
            m_core->IncreaseReference(&(pOriginalSurface->Data));
        }

        MJPEGEncodeTask *pTask = NULL;
        {
            UMC::AutomaticUMCMutex guard(m_guard);
            pTask = m_freeTasks.front();
        }

        pEntryPoint->requiredNumThreads = IPP_MIN(pTask->m_pMJPEGVideoEncoder->NumEncodersAllocated(),
                                                  IPP_MIN(m_vParam.mfx.NumThread,
                                                          pTask->CalculateNumPieces(pOriginalSurface, &(m_vParam.mfx.FrameInfo))));

        pTask->bs           = (sts == (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK) ? 0 : bs;
        pTask->ctrl         = ctrl;
        pTask->surface      = pOriginalSurface;
        pEntryPoint->pParam = pTask;

        pLastTask = m_freeTasks.front();

        // remove the ready task from the queue
        {
            UMC::AutomaticUMCMutex guard(m_guard);
            m_freeTasks.pop();
        }
    }

    return sts;
}

#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE
