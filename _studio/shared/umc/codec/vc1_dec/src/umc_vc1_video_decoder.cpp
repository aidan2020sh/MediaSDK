//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_video_decoder.h"
#include "umc_video_data.h"
#include "vm_time.h"
#include "umc_media_data_ex.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_dec_seq.h"
#include "vm_sys_info.h"
#include "vm_thread.h"
#include "vm_types.h"
#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_task.h"
#include "mfx_trace.h"

#include "umc_memory_allocator.h"
#include "umc_vc1_dec_time_statistics.h"
#include "umc_vc1_common.h"
#include "umc_vc1_common_defs.h"
#include "assert.h"
#include "umc_vc1_dec_exception.h"

#include "umc_va_base.h"
#include "umc_vc1_dec_va.h"
#include "umc_vc1_dec_frame_descr_va.h"

#include "ipps.h"

using namespace UMC;
using namespace UMC::VC1Common;
using namespace UMC::VC1Exceptions;

namespace UMC
{
    VideoDecoder *CreateVC1Decoder() { return (new VC1VideoDecoder); }
}

VC1VideoDecoder::VC1VideoDecoder():m_pContext(NULL),
                                   m_pInitContext(),
                                   m_pdecoder(NULL),
                                   m_iThreadDecoderNum(0),
                                   m_dataBuffer(NULL),
                                   m_frameData(NULL),
                                   m_stCodes(NULL),
                                   m_decoderInitFlag(0),
                                   m_decoderFlags(0),
                                   m_iNewMemID((MemID)(-1)),
                                   m_iMemContextID((MemID)(-1)),
                                   m_iHeapID((MemID)(-1)),
                                   m_iFrameBufferID((MemID)(-1)),
                                   m_pts(0),
                                   m_pts_dif(0),
                                   m_bIsWMPSplitter(false),
                                   m_iMaxFramesInProcessing(0),
                                   m_bIsFrameToOut(false),
                                   m_iRefFramesDst(0),
                                   m_iBFramesDst(0),
                                   m_pPrevDescriptor(NULL),
                                   m_lFrameCount(0),
                                   m_bLastFrameNeedDisplay(false),
                                   m_bIsWarningStream(false),
                                   m_pSelfDecoder(NULL),
                                   m_pStore(NULL),
                                   m_va(NULL),
                                   m_CurrentMode(Routine),
                                   m_pCutBytes(NULL),
                                   m_pHeap(NULL),
                                   m_bIsReorder(true),
                                   m_pCurrentIn(NULL),
                                   m_pCurrentOut(NULL),
                                   m_bIsNeedAddFrameSC(false),
                                   m_bIsNeedToShiftIn(true),
                                   m_bIsNeedToFlush(false),
                                   m_AllocBuffer(0),
                                   m_pExtFrameAllocator(0),
                                   m_SurfaceNum(0),
                                   m_bIsExternalFR(false)


#ifdef CREATE_ES
                                   ,m_fPureVideo(NULL)
#endif
#ifdef  VC1_THREAD_STATISTIC
                                   ,m_parallelStat(NULL)
                                   ,m_eEntryArray(NULL)
#endif
{
#ifdef  VC1_THREAD_STATISTIC
    m_parallelStat = NULL;
#endif
}

VC1VideoDecoder::~VC1VideoDecoder()
{
#ifdef  VC1_THREAD_STATISTIC
    if (m_parallelStat)
        fclose(m_parallelStat);
#endif

#ifdef CREATE_ES
    if (m_fPureVideo)
        fclose(m_fPureVideo);
#endif
    Close();
}


Status VC1VideoDecoder::Init(BaseCodecParams *pInit)
{
    MediaData *data;
    Status umcRes = UMC_OK;
    Ipp32u i;
    Ipp32u readSize = 0;

    Close();

    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if(!init)
        return UMC_ERR_INIT;

    m_pSelfDecoder = this;

   // assert(init->info.stream_subtype != 0);

    m_decoderFlags = init->lFlags;
    data = init->m_pData;

    m_ClipInfo = init->info;
    if (m_ClipInfo.framerate != 0.0)
        m_bIsExternalFR = true;

    //almost all applications need reorder as default 
    if ((m_decoderFlags & FLAG_VDEC_REORDER) == FLAG_VDEC_REORDER)
        m_bIsReorder = true;
    else
        m_bIsReorder = false;

   
    Ipp32u mbWidth = init->info.clip_info.width/VC1_PIXEL_IN_LUMA;
    Ipp32u mbHeight= init->info.clip_info.height/VC1_PIXEL_IN_LUMA;

    m_AllocBuffer = 2*(mbHeight*VC1_PIXEL_IN_LUMA)*(mbWidth*VC1_PIXEL_IN_LUMA);

    m_SurfaceNum = init->m_SuggestedOutputSize;

    //VA initializtion
    if (init->pVideoAccelerator)
    {
        if ((init->pVideoAccelerator->m_Profile & VA_CODEC) == VA_VC1)
            m_va = init->pVideoAccelerator;
        else
            return UMC_ERR_UNSUPPORTED;
    }

    //MEMORY ALLOCATOR
    if (UMC_OK != BaseCodec::Init(pInit) )
    {
        Close();
        return UMC_ERR_INIT;
    }


    if (WMV_VIDEO == init->info.stream_type)
    {
        m_bIsWMPSplitter = true;
    }

    m_PostProcessing = init->pPostProcessing;


    // get allowed thread numbers
    Ipp32s nAllowedThreadNumber = init->numThreads;
    //printf("nAllowedThreadNumber = %d\n",nAllowedThreadNumber);

#if defined (_WIN32) && (_DEBUG)
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugAlloc);
#endif
#endif

#ifdef THREADS
    nAllowedThreadNumber = THREADS_NUM;
#endif

    //if(nAllowedThreadNumber < 0) nAllowedThreadNumber = 0;
    //else
    //    if(nAllowedThreadNumber > 8) nAllowedThreadNumber = 8;

    //nAllowedThreadNumber = 1;


    m_iThreadDecoderNum = (0 == nAllowedThreadNumber) ? (vm_sys_info_get_cpu_num()) : (nAllowedThreadNumber);
    //m_iThreadDecoderNum = (0 == nAllowedThreadNumber) ? 1 : nAllowedThreadNumber;

    m_iMaxFramesInProcessing = m_iThreadDecoderNum + NumBufferedFrames;

    if (UMC_OK != ContextAllocation(mbWidth, mbHeight))
        return UMC_ERR_INIT;


    //Heap allocation
    {
        Ipp32u heapSize = CalculateHeapSize();
        // Need to replace with MFX allocator
        if (m_pMemoryAllocator->Alloc(&m_iHeapID,
                                      heapSize,//100000,
                                      UMC_ALLOC_PERSISTENT,
                                      16) != UMC_OK)
                                      return UMC_ERR_ALLOC;

        new(m_pHeap) VC1TSHeap((Ipp8u*)(m_pMemoryAllocator->Lock(m_iHeapID)),heapSize);
    }
    if (UMC_OK != CreateFrameBuffer(m_AllocBuffer))
        return UMC_ERR_ALLOC;


    m_pContext->m_bIntensityCompensation = 0;

    // profile definition
    if ((VC1_VIDEO_RCV == init->info.stream_subtype)||
        (WMV3_VIDEO == init->info.stream_subtype))
        m_pContext->m_seqLayerHeader.PROFILE = VC1_PROFILE_MAIN;
    else if ((VC1_VIDEO_VC1 == init->info.stream_subtype)||
             (WVC1_VIDEO == init->info.stream_subtype))
             m_pContext->m_seqLayerHeader.PROFILE = VC1_PROFILE_ADVANCED;
    else
    {
        m_pContext->m_seqLayerHeader.PROFILE = VC1_PROFILE_UNKNOWN;
    }
    
    m_pContext->m_Offsets = m_frameData->GetExData()->offsets;
    m_pContext->m_values = m_frameData->GetExData()->values;

    if(data!=NULL && (Ipp32s*)data->GetDataPointer() != NULL) // seq header is presents
    {
        m_pCurrentIn = data;
        
        if(m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_UNKNOWN)
        {
            //assert(0);
            if((Ipp32u)((m_pCurrentIn)&&0xFF) == 0xC5)
                m_pContext->m_seqLayerHeader.PROFILE = VC1_PROFILE_MAIN;
            else
                m_pContext->m_seqLayerHeader.PROFILE = VC1_PROFILE_ADVANCED;
        }

        if(!((m_decoderFlags & FLAG_VDEC_4BYTE_ACCESS) == FLAG_VDEC_4BYTE_ACCESS))
        {
            //need to create buffer for swap data
            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
            {
                umcRes = GetStartCodes((Ipp8u*)data->GetDataPointer(),
                                       (Ipp32u)data->GetDataSize(),
                                        m_frameData,
                                        &readSize); // parse and copy to self buffer

                SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
                m_pContext->m_pBufferStart = (Ipp8u*)m_frameData->GetDataPointer();
                umcRes = StartCodesProcessing(m_pContext->m_pBufferStart,
                                              m_pContext->m_Offsets,
                                              m_pContext->m_values,
                                              true,
                                              false);
                if (UMC_ERR_SYNC != umcRes)
                    umcRes = UMC_OK;
                //DefineSize();
                if (m_bIsNeedToShiftIn)
                    m_pCurrentIn->MoveDataPointer(readSize);
            }
            else
            {
                // simple copy data
                ippsCopy_8u((Ipp8u*)data->GetDataPointer(),  m_dataBuffer, (Ipp32u)data->GetDataSize());
                SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)(Ipp32u)data->GetDataSize());
                m_pContext->m_FrameSize  = (Ipp32u)data->GetDataSize();
            }
        }
        else
        {
            m_pContext->m_pBufferStart  = (Ipp8u*)data->GetBufferPointer();
            m_pContext->m_FrameSize     = (Ipp32u)data->GetDataSize();
            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
            {
                VC1Status sts;
                m_pContext->m_bitstream.pBitstream = (Ipp32u*)(m_pContext->m_pBufferStart) + 1;//skip start code
                m_pContext->m_bitstream.bitOffset = 31;
                sts = SequenceLayer(m_pContext);
                VC1_TO_UMC_CHECK_STS(sts);
                //DefineSize();
                m_pCurrentIn->MoveDataPointer(m_pContext->m_FrameSize);
            }

        }
        m_pContext->m_bitstream.bitOffset    = 31;
        if (VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE)
        {
            umcRes = InitSMProfile(init);
            UMC_CHECK_STATUS(umcRes);
            readSize = (Ipp32u)m_pCurrentIn->GetDataSize();
            data->MoveDataPointer(readSize);
        }
        m_bIsNeedToShiftIn = true;


 // extract pure video from wmv
#ifdef CREATE_ES
        if (!m_fPureVideo)
        {
            m_fPureVideo = fopen("output_es.es","wb");
            if (!m_fPureVideo)
                return UMC_ERR_ALLOC;
        }
        if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
        {
            fwrite(data->GetBufferPointer(), 1, data->GetBufferSize(), m_fPureVideo);
        }
        else
        {
            Ipp32u begin = 0x0000000F;
            fwrite(&begin, 1, 3, m_fPureVideo);
            begin = 0xC5;
            fwrite(&begin, 1, 1, m_fPureVideo);
            begin = 0x0000004;
            fwrite(&begin, 1, 4, m_fPureVideo);
            Ipp32u seqHead = *m_pContext->m_pBufferStart;
            SwapData((Ipp8u*)(m_pContext->m_pBufferStart),4);
            fwrite(m_pContext->m_pBufferStart, 1, 4, m_fPureVideo);
            Ipp32u picSize[2] = {(m_pContext->m_seqLayerHeader.MAX_CODED_HEIGHT+1)*2, (m_pContext->m_seqLayerHeader.MAX_CODED_WIDTH+1)*2};
            fwrite(picSize, 1, 8, m_fPureVideo);
            begin = 0x0000000C;
            fwrite(&begin, 1, 4, m_fPureVideo);
            fwrite(&begin, 1, 12, m_fPureVideo);
        }
#endif

        if(m_AllocBuffer == 0)
        {
            Close();
            return UMC_ERR_SYNC;
        }

        GetFPS(m_pContext);
        GetPTS(data->GetTime());
        m_decoderInitFlag = 1;
    }
    else
    {
        //NULL input data, no sequence header
        
        // aligned width and height 
        m_pContext->m_seqLayerHeader.MAX_CODED_HEIGHT = init->info.clip_info.height/2 - 1;
        m_pContext->m_seqLayerHeader.MAX_CODED_WIDTH = init->info.clip_info.width/2 - 1;

        m_pContext->m_seqLayerHeader.widthMB = (Ipp16u)(init->info.clip_info.width/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader.heightMB  = (Ipp16u)(init->info.clip_info.height/VC1_PIXEL_IN_LUMA);

        m_pContext->m_seqLayerHeader.MaxWidthMB = m_pContext->m_seqLayerHeader.widthMB;
        m_pContext->m_seqLayerHeader.MaxHeightMB = m_pContext->m_seqLayerHeader.heightMB;

        if(init->info.interlace_type != UMC::PROGRESSIVE)
            m_pContext->m_seqLayerHeader.INTERLACE  = 1;

        if(m_AllocBuffer == 0)
        {
            Close();
            return UMC_ERR_SYNC;
        }
    }

    if (!InitVAEnvironment(m_AllocBuffer))
        return UMC_ERR_ALLOC;

    if (!m_pSelfDecoder->InitAlloc(m_pContext,2*m_iMaxFramesInProcessing))
        return UMC_ERR_ALLOC;

    m_pInitContext = *m_pContext;
    try
    // memory allocation and Init all env for frames/tasks store - VC1TaskStore object
    {
        m_pStore = (VC1TaskStore*)m_pHeap->s_alloc<VC1TaskStore>();
        m_pStore = new(m_pStore) VC1TaskStore(m_pMemoryAllocator);

        if (!m_pStore->Init(m_iThreadDecoderNum,
                            m_iMaxFramesInProcessing,
                            this) )
            return UMC_ERR_ALLOC;


#if defined UMC_VA_DXVA || defined UMC_VA_LINUX
        m_pStore->CreateDSQueue(m_pContext,m_va);
#else
        m_pStore->CreateDSQueue(m_pContext,m_bIsReorder,0);
#endif

        m_pStore->CreateOutBuffersQueue();
        m_pStore->SetDefinition(&m_pContext->m_seqLayerHeader);
        m_pStore->FillIdxVector(2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));



        // create thread decoders for multithreading support
        {
            m_pHeap->s_new(&m_pdecoder,m_iThreadDecoderNum);
            memset(m_pdecoder, 0, sizeof(VC1ThreadDecoder*) * m_iThreadDecoderNum);


            for (i = 0; i < m_iThreadDecoderNum; i += 1)
            {
                m_pHeap->s_new(&m_pdecoder[i]);
            }

            for (i = 0;i < m_iThreadDecoderNum;i += 1)
            {
                if (UMC_OK != m_pdecoder[i]->Init(m_pContext,i,m_pStore,m_va,m_pMemoryAllocator,NULL))
                    return UMC_ERR_INIT;
            }
        }
    }
    catch(...)
    {
        // only allocation errors here
        Close();
        return UMC_ERR_ALLOC;
    }

#ifdef  VC1_THREAD_STATISTIC
    {
        Ipp32u NumEntries = m_pContext->m_seqLayerHeader.widthMB*9*4*2; // 9 - types of works,
                                                                         // 4 - number of calling (getNext, start proc, end proc, addPerfomed )
                                                                         // 2 - for possible sleep and wake states

        m_eEntryArray = (VC1ThreadEntry**)ippsMalloc_8u(sizeof(VC1ThreadEntry*)*m_iThreadDecoderNum);
        for (i = 0;i < m_iThreadDecoderNum;i += 1)
        {
            m_eEntryArray[i] = (VC1ThreadEntry*)ippsMalloc_8u(sizeof(VC1ThreadEntry)*NumEntries);
            if (!m_eEntryArray[i])
                return UMC_ERR_INIT;
            m_pdecoder[i]->m_pJobSlice->m_Statistic = new VC1ThreadStatistic(i,m_eEntryArray[i],NumEntries);
        }
    }
#endif

#ifdef _PROJECT_STATISTICS_
    TimeStatisticsStructureInitialization();
    VC1VideoDecoderParams *VC1Init = DynamicCast<VC1VideoDecoderParams, BaseCodecParams>(pInit);

    if(VC1Init)
        m_timeStatistics->streamName = VC1Init->streamName;
#endif
    //internal decoding flags
    m_lFrameCount = 0;
    m_iRefFramesDst = 0;
    m_iBFramesDst = 0;
    //internal exception initialization
    vc1_except_profiler::GetEnvDescript(smart_recon, mbGroupLevel);


    return umcRes;
}


bool VC1VideoDecoder::InitVAEnvironment(Ipp32u frameSize)
{
    // Think about HW
    if (!m_va)
    {
        if (!m_pContext->m_frmBuff.m_pFrames)
        {
            m_pHeap->s_new_one(&m_pContext->m_frmBuff.m_pFrames, 2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));
            if (!m_pContext->m_frmBuff.m_pFrames)
                return false;
            // Allocation only if absence external allocator
            if (!m_pExtFrameAllocator)
            {
                if (!m_pContext->m_frmBuff.m_pFrames[0].m_pAllocatedMemory )
                {
                    if(m_pMemoryAllocator->Alloc(&m_iNewMemID,
                        (m_iMaxFramesInProcessing + VC1NUMREFFRAMES)*frameSize, // 2 - references frames !!!!!
                        UMC_ALLOC_PERSISTENT,16) != UMC_OK )
                    {
                        Close();
                        return false;
                    }
                    m_pContext->m_frmBuff.m_pFrames[0].m_pAllocatedMemory = (Ipp8u*)m_pMemoryAllocator->Lock(m_iNewMemID);
                    if(m_pContext->m_frmBuff.m_pFrames[0].m_pAllocatedMemory == NULL)
                        return false;
                }
            }
        }
        return true;
    }

    if (!m_pContext->m_frmBuff.m_pFrames)
    {
        m_pHeap->s_new_one(&m_pContext->m_frmBuff.m_pFrames, m_SurfaceNum);
        if (!m_pContext->m_frmBuff.m_pFrames)
            return false;
    }

#ifdef UMC_VA_DXVA
    if (m_va->IsIntelCustomGUID())
    {
#ifndef MFX_PROTECTED_FEATURE_DISABLE
        if(m_va->GetProtectedVA())
            m_pSelfDecoder = new VC1VideoDecoderVA<VC1PackerDXVA_Protected>;
        else
#endif
            m_pSelfDecoder = new VC1VideoDecoderVA<VC1PackerDXVA_EagleLake>;
    }
    else
        m_pSelfDecoder = new VC1VideoDecoderVA<VC1PackerDXVA>;
#endif

#ifdef UMC_VA_LINUX
    m_pSelfDecoder = new VC1VideoDecoderVA<VC1PackerLVA>;
#endif

    m_pSelfDecoder->SetVideoHardwareAccelerator(m_va);

    return true;
}
Status VC1VideoDecoder::InitSMProfile(VideoDecoderParams *init)
{
    Ipp32s seq_size = 0;
    Ipp8u* seqStart = NULL;
    Ipp32u height;
    Ipp32u width;
    VC1Status sts = VC1_OK;

    if (!m_bIsWMPSplitter)
    {
        SwapData(m_pContext->m_pBufferStart, m_pContext->m_FrameSize);

        seqStart = m_pContext->m_pBufferStart + 4;
        seq_size  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);

        assert(seq_size > 0);
        assert(seq_size < 100);

        seqStart = m_pContext->m_pBufferStart + 8 + seq_size;
        height  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);
        seqStart+=4;
        width  = ((*(seqStart+3))<<24) + ((*(seqStart+2))<<16) + ((*(seqStart+1))<<8) + *(seqStart);

        m_pContext->m_seqLayerHeader.widthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader.heightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader.CODED_HEIGHT  = m_pContext->m_seqLayerHeader.MAX_CODED_HEIGHT = height/2 - 1;
        m_pContext->m_seqLayerHeader.CODED_WIDTH = m_pContext->m_seqLayerHeader.MAX_CODED_WIDTH = width/2 - 1;

        m_pContext->m_seqLayerHeader.MaxWidthMB  = (Ipp16u)((width+15)/VC1_PIXEL_IN_LUMA);
        m_pContext->m_seqLayerHeader.MaxHeightMB = (Ipp16u)((height+15)/VC1_PIXEL_IN_LUMA);

        SwapData(m_pContext->m_pBufferStart, m_pContext->m_FrameSize);

        m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart + 2; // skip header
        m_pContext->m_bitstream.bitOffset = 31;
    }
    else //Get info from parameters
    {
        m_pContext->m_seqLayerHeader.MAX_CODED_HEIGHT = init->info.clip_info.height/2 - 1;
        m_pContext->m_seqLayerHeader.MAX_CODED_WIDTH = init->info.clip_info.width/2 - 1;

        m_pContext->m_seqLayerHeader.widthMB
            = (Ipp16u)((init->info.clip_info.width+15)/VC1_PIXEL_IN_LUMA);

        m_pContext->m_seqLayerHeader.heightMB
            = (Ipp16u)((init->info.clip_info.height+15)/VC1_PIXEL_IN_LUMA);

        m_pContext->m_seqLayerHeader.MaxWidthMB = m_pContext->m_seqLayerHeader.widthMB;
        m_pContext->m_seqLayerHeader.MaxHeightMB = m_pContext->m_seqLayerHeader.heightMB;
    }
    sts = SequenceLayer(m_pContext);
    VC1_TO_UMC_CHECK_STS(sts);
    return UMC_OK;
}

Ipp32u VC1VideoDecoder::CalculateHeapSize()
{
    Ipp32u Size = 0;
    Ipp32u counter = 0;


    Size += align_value<Ipp32u>(sizeof(VC1TaskStore));
    Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder**)*m_iThreadDecoderNum);
    if (!m_va)
        Size += align_value<Ipp32u>(sizeof(Frame)*(2*m_iMaxFramesInProcessing + 2*VC1NUMREFFRAMES));
    else
        Size += align_value<Ipp32u>(sizeof(Frame)*(m_SurfaceNum));


    for (counter = 0; counter < m_iThreadDecoderNum; counter += 1)
    {
        Size += align_value<Ipp32u>(sizeof(VC1ThreadDecoder));
    }

    //if(!((m_decoderFlags & FLAG_VDEC_4BYTE_ACCESS) == FLAG_VDEC_4BYTE_ACCESS))
        Size += align_value<Ipp32u>(sizeof(MediaDataEx));



    return Size;
}
Status VC1VideoDecoder::ProcessSeqHeader()
{
    Status umcRes = UMC_OK;
    VideoDecoderParams params;
    //need to change display index
    m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetNextIndex();
    m_pStore->FreeIndexQueue();
    m_pStore->ResetDSQueue();
    m_lFrameCount = 0;
    m_pContext->m_frmBuff.m_iPrevIndex  = -1;
    m_pContext->m_frmBuff.m_iNextIndex= -1;
    m_pContext->m_frmBuff.m_iCurrIndex = -1;
    m_pContext->m_bIntensityCompensation = 0;

    if (/*out_data!=NULL && */
        m_pContext->m_frmBuff.m_iDisplayIndex>-1 && (umcRes == UMC_OK))
    {
        umcRes = WriteFrame(m_pCurrentIn,m_pContext,m_pCurrentOut);
        umcRes =  UMC_OK;
    }
    else
        umcRes =  UMC_ERR_NOT_ENOUGH_DATA;

    params.m_pData = m_pCurrentIn;
    params.lFlags = m_decoderFlags;
    params.info.stream_type = m_ClipInfo.stream_type;
    params.info.stream_subtype = m_ClipInfo.stream_subtype;
    params.numThreads = m_iThreadDecoderNum;
    params.info.clip_info.width = m_ClipInfo.clip_info.width;
    params.info.clip_info.height = m_ClipInfo.clip_info.height;
    params.pVideoAccelerator = m_va;

    if(m_allocatedPostProcessing == NULL)
        params.pPostProcessing = m_PostProcessing;
    else
        m_PostProcessing = NULL;

    //Close();
    params.lpMemoryAllocator = 0;

    umcRes = Init(&params);

    if (m_pCurrentOut)
        m_pCurrentOut->SetTime(m_pts);

    return umcRes;

}
Status VC1VideoDecoder::ContextAllocation(Ipp32u mbWidth,Ipp32u mbHeight)
{
    // need to extend for threading case
    if (!m_pContext)
    {

        Ipp8u* ptr = NULL;
        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        ptr += align_value<Ipp32u>(sizeof(VC1VLCTables));
        ptr += align_value<Ipp32u>(sizeof(Ipp16s)*mbHeight*mbWidth*2*2);
        ptr += align_value<Ipp32u>(mbHeight*mbWidth);
        ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));
        if(m_stCodes == NULL)
        {
            ptr += align_value<Ipp32u>(START_CODE_NUMBER*2*sizeof(Ipp32u)+sizeof(MediaDataEx::_MediaDataEx));
        }

        // Need to replace with MFX allocator
        if (m_pMemoryAllocator->Alloc(&m_iMemContextID,
                                      (size_t)ptr,
                                      UMC_ALLOC_PERSISTENT,
                                      16) != UMC_OK)
                                      return UMC_ERR_ALLOC;

        m_pContext = (VC1Context*)(m_pMemoryAllocator->Lock(m_iMemContextID));
        memset(m_pContext,0,(size_t)ptr);
        ptr = (Ipp8u*)m_pContext;

        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        m_pContext->m_vlcTbl = (VC1VLCTables*)ptr;

        ptr += align_value<Ipp32u>(sizeof(VC1VLCTables));
        m_pContext->savedMV_Curr = (Ipp16s*)ptr;

        ptr += align_value<Ipp32u>(sizeof(Ipp16s)*mbHeight*mbWidth*2*2);
        m_pContext->savedMVSamePolarity_Curr = ptr;

        ptr +=  align_value<Ipp32u>(mbHeight*mbWidth);
        m_pHeap = (VC1TSHeap*)ptr;

        if(m_stCodes == NULL)
        {
            ptr += align_value<Ipp32u>(sizeof(VC1TSHeap));
            m_stCodes = (MediaDataEx::_MediaDataEx *)(ptr);


            memset(m_stCodes, 0, (START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx)));
            m_stCodes->count      = 0;
            m_stCodes->index      = 0;
            m_stCodes->bstrm_pos  = 0;
            m_stCodes->offsets    = (Ipp32u*)((Ipp8u*)m_stCodes +
            sizeof(MediaDataEx::_MediaDataEx));
            m_stCodes->values     = (Ipp32u*)((Ipp8u*)m_stCodes->offsets +
            START_CODE_NUMBER*sizeof( Ipp32u));
        }

    }
    return UMC_OK;
}
Status VC1VideoDecoder::StartCodesProcessing(Ipp8u*   pBStream,
                                             Ipp32u*  pOffsets,
                                             Ipp32u*  pValues,
                                             bool     IsDataPrepare,
                                             bool     IsNeedToInitCall)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VC1VideoDecoder::StartCodesProcessing");
    Status umcRes = UMC_OK;
    Ipp32u readSize = 0;
    Ipp32u UnitSize;
    VC1Status sts = VC1_OK;
    while ((*pValues != 0x0D010000)&&
           (*pValues))
    {
        m_pContext->m_bitstream.pBitstream = (Ipp32u*)((Ipp8u*)m_frameData->GetDataPointer() + *pOffsets) + 1;//skip start code
        if(*(pOffsets + 1))
            UnitSize = *(pOffsets + 1) - *pOffsets;
        else
            UnitSize = (Ipp32u)(m_pCurrentIn->GetBufferSize() - *pOffsets);

        if (!IsDataPrepare)
        {
            // copy data to self buffer
            ippsCopy_8u(pBStream + *pOffsets,
                        m_dataBuffer,
                        UnitSize);
            SwapData(m_dataBuffer, align_value<Ipp32u>(UnitSize));
            m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_dataBuffer + 1; //skip start code
        }
        readSize += UnitSize;
        m_pContext->m_bitstream.bitOffset  = 31;
        switch (*pValues)
        {
        case 0x0F010000:
            bool isFPSChange;
            VC1Context context;
            size_t alignment;
           umcRes =  UMC_ERR_NOT_ENOUGH_DATA;
            context = *m_pContext;
            sts = SequenceLayer(&context);
            VC1_TO_UMC_CHECK_STS(sts);
            isFPSChange = GetFPS(&context);
            alignment = (context.m_seqLayerHeader.INTERLACE)?32:16;


            if (align_value<Ipp32u>(2*(context.m_seqLayerHeader.MAX_CODED_WIDTH+1)) > align_value<Ipp32u>(2*(m_pInitContext.m_seqLayerHeader.MAX_CODED_WIDTH+1)))
                return UMC_ERR_INVALID_PARAMS;
            if (context.m_seqLayerHeader.INTERLACE != m_pInitContext.m_seqLayerHeader.INTERLACE )
                return UMC_ERR_INVALID_PARAMS;
            if (align_value<Ipp32u>(2*(context.m_seqLayerHeader.MAX_CODED_HEIGHT+1),alignment) > align_value<Ipp32u>(2*(m_pInitContext.m_seqLayerHeader.MAX_CODED_HEIGHT+1),alignment))
                return UMC_ERR_INVALID_PARAMS;
            // start codes are applicable for advanced profile only
            if (context.m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
                return UMC_ERR_INVALID_PARAMS;



            else if (!m_decoderInitFlag)
            {
                *m_pContext = context;
                m_decoderInitFlag = true;
                //if(!InitInterlacedTables(m_pContext))
                //    return UMC_ERR_FAILED;
                //m_pCurrentIn->MoveDataPointer(m_pContext->m_FrameSize);

            }
            else
            {
                *m_pContext = context;

                if (align_value<Ipp32u>(2*(context.m_seqLayerHeader.MAX_CODED_WIDTH+1)) != align_value<Ipp32u>(2*(m_pInitContext.m_seqLayerHeader.MAX_CODED_WIDTH+1))
                   || align_value<Ipp32u>(2*(context.m_seqLayerHeader.MAX_CODED_HEIGHT+1),alignment) != align_value<Ipp32u>(2*(m_pInitContext.m_seqLayerHeader.MAX_CODED_HEIGHT+1),alignment))
                {
                    m_pStore->SetNewSHParams(m_pContext);
                    umcRes = UMC_NTF_NEW_RESOLUTION;
                }
             }
            VC1_TO_UMC_CHECK_STS(sts);
            break;
        case 0x0A010000:
            umcRes =  UMC_ERR_NOT_ENOUGH_DATA;
            break;
        case 0x0E010000:
            umcRes = EntryPointLayer(m_pContext);

            if (UMC_OK != umcRes)
                return UMC_ERR_INVALID_PARAMS;
            if (!m_bIsWMPSplitter)
                umcRes =  UMC_ERR_NOT_ENOUGH_DATA;
            else
                umcRes = UMC_OK;
            break;
        case 0x0C010000:
        case 0x1B010000:
        case 0x1C010000:
        case 0x1D010000:
        case 0x1E010000:
        case 0x1F010000:
            umcRes = UMC_ERR_NOT_ENOUGH_DATA;
            break;
        default:
            //printf("incorrect start code suffix \n");
            umcRes = UMC_ERR_SYNC;
            break;

        }
        pValues++;
        pOffsets++;
    }
    if (((0x0D010000) == *pValues) && m_decoderInitFlag)// we have frame to decode
    {
        UnitSize = (Ipp32u)(m_pCurrentIn->GetBufferSize() - *pOffsets);
        readSize += UnitSize;
        m_pContext->m_pBufferStart = ((Ipp8u*)m_frameData->GetDataPointer() + *pOffsets); //skip start code
        if (!IsDataPrepare)
        {
            // copy frame data to self buffer
            ippsCopy_8u((Ipp8u*)m_frameData->GetDataPointer() + *pOffsets,
                m_dataBuffer,
                UnitSize);
            //use own buffer
            SwapData(m_dataBuffer, align_value<Ipp32u>(UnitSize));
            m_pContext->m_pBufferStart = m_dataBuffer; //skip start code
        }
        else
            ++m_lFrameCount;


        try //work with queue of frame descriptors and process every frame
        {
            umcRes = m_pSelfDecoder->VC1DecodeFrame(this,m_pCurrentIn,m_pCurrentOut);
        }
        catch (vc1_exception ex)
        {
            exception_type e_type = ex.get_exception_type();
            if ((e_type == internal_pipeline_error)||
                (e_type == mem_allocation_er))
                return UMC_ERR_FAILED;
        }
    }
    else if (m_decoderInitFlag && (!m_pSelfDecoder->m_bIsNeedToFlush))
    {
        m_pCurrentIn->MoveDataPointer(m_pContext->m_FrameSize);
    }
    else
        umcRes = UMC_ERR_NOT_ENOUGH_DATA;


    return umcRes;
}

Status VC1VideoDecoder::SMProfilesProcessing(Ipp8u* pBitstream)
{
    Status umcRes = UMC_OK;
    m_pContext->m_bitstream.pBitstream = (Ipp32u*)pBitstream;
    m_pContext->m_bitstream.bitOffset  = 31;
    if (((*m_pContext->m_bitstream.pBitstream&0xFF) == 0xC5)&&
        (!m_bIsWMPSplitter))// sequence header, exit after process. in WMV container - no seq header in WMV3
    {
        VideoDecoderParams params;
        params.m_pData = m_pCurrentIn;
        params.lFlags = m_decoderFlags;
        params.info.stream_type = m_ClipInfo.stream_type;
        params.info.stream_subtype = m_ClipInfo.stream_subtype;
        params.numThreads = m_iThreadDecoderNum;
        params.info.clip_info.width = m_ClipInfo.clip_info.width;
        params.info.clip_info.height = m_ClipInfo.clip_info.height;
        params.m_SuggestedOutputSize = m_SurfaceNum;
    
        if(m_allocatedPostProcessing == NULL)
            params.pPostProcessing = m_PostProcessing;
        else
            m_PostProcessing = NULL;

        bool deblocking = (Ipp32s)m_pStore->IsDeblockingOn();

        Close();
        Init(&params);
        
        //need to add code for restoring skip mode!
        if(!deblocking)
        {
            Ipp32s speed_shift = 0x22;
            m_pStore->ChangeVideoDecodingSpeed(speed_shift);
        }

        return UMC_ERR_NOT_ENOUGH_DATA;
    }
    if (m_bIsWMPSplitter)
    {
#ifdef CREATE_ES
            {
                Ipp32u tdataSize = (m_pCurrentIn->GetDataSize())&0x00FFFFFF;
                fwrite(&tdataSize, 1, 4, m_fPureVideo);
                tdataSize = 0xFFFFFFFF; // timestamp
                fwrite(&tdataSize, 1, 4, m_fPureVideo);
                fwrite((Ipp8u*)m_pCurrentIn->GetBufferPointer(), 1, m_pCurrentIn->GetDataSize(), m_fPureVideo);
            }
#endif
    }

    m_pContext->m_pBufferStart  = (Ipp8u*)pBitstream;
    ++m_lFrameCount;
    try //work with queue of frame descriptors and process every frame
    {
        umcRes = m_pSelfDecoder->VC1DecodeFrame(this,m_pCurrentIn,m_pCurrentOut);
    }
    catch (vc1_exception ex)
    {
        exception_type e_type = ex.get_exception_type();
        if (e_type == internal_pipeline_error)
            return UMC_ERR_FAILED;
    }
    return umcRes;

}
Status VC1VideoDecoder::ParseStreamFromMediaData()
{
    Status umcRes = UMC_OK;

    // we have no start codes. Let find them
    if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
    {
        Ipp32u readSize;
        m_frameData->GetExData()->count = 0;
        memset(m_frameData->GetExData()->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        memset(m_frameData->GetExData()->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        if (m_bIsWMPSplitter)
            MSWMVPreprocessing();
        umcRes = GetStartCodes((Ipp8u*)m_pCurrentIn->GetDataPointer(),
                               (Ipp32u)m_pCurrentIn->GetDataSize(),
                               m_frameData,
                               &readSize); // parse and copy to self buffer

        SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
        m_pContext->m_FrameSize = readSize;
        umcRes = StartCodesProcessing((Ipp8u*)m_frameData->GetDataPointer(),
                                       m_frameData->GetExData()->offsets,
                                       m_frameData->GetExData()->values,
                                       true,
                                       true);

    }
    else //Simple/Main profiles pack without Start Codes
    {
        // copy data to self buffer
        ippsCopy_8u((Ipp8u*)m_pCurrentIn->GetDataPointer(),
                     m_dataBuffer,
                    (Ipp32u)m_pCurrentIn->GetDataSize());

        m_pContext->m_FrameSize = (Ipp32u)m_pCurrentIn->GetDataSize();
        m_frameData->SetDataSize(m_pContext->m_FrameSize);
        SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
        umcRes = SMProfilesProcessing(m_dataBuffer);
    }
    return umcRes;
}

Status VC1VideoDecoder::ParseStreamFromMediaDataEx(MediaDataEx *in_ex)
{
    Status umcRes = UMC_OK;
    if(!((m_decoderFlags & FLAG_VDEC_4BYTE_ACCESS) == FLAG_VDEC_4BYTE_ACCESS))
    {
        if ((in_ex->GetExData())&&
            (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE))// we have already start codes from splitter. Advance profile only
        {
            m_pContext->m_Offsets = in_ex->GetExData()->offsets;
            m_pContext->m_values = in_ex->GetExData()->values;
            ippsCopy_8u((Ipp8u*)m_pCurrentIn->GetDataPointer(),
                        m_dataBuffer,
                       (Ipp32u)m_pCurrentIn->GetDataSize());

            //SwapData((Ipp8u*)m_frameData->GetBufferPointer(), (Ipp32u)m_frameData->GetDataSize());
            m_pContext->m_FrameSize = (Ipp32u)m_pCurrentIn->GetDataSize();
            m_frameData->SetDataSize(m_pContext->m_FrameSize);
            SwapData((Ipp8u*)m_frameData->GetDataPointer(), m_pContext->m_FrameSize);
            umcRes = StartCodesProcessing((Ipp8u*)m_frameData->GetDataPointer(),
                                           m_pContext->m_Offsets,
                                           m_pContext->m_values,
                                           true,
                                           true);

        }
        else
        {
            // we have no start codes. Let find them
            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
            {
                Ipp32u readSize;
                m_frameData->GetExData()->count = 0;
                memset(m_frameData->GetExData()->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
                memset(m_frameData->GetExData()->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
                if (m_bIsWMPSplitter)
                    MSWMVPreprocessing();
                umcRes = GetStartCodes((Ipp8u*)m_pCurrentIn->GetDataPointer(),
                                       (Ipp32u)m_pCurrentIn->GetDataSize(),
                                        m_frameData,
                                        &readSize); // parse and copy to self buffer

                m_pContext->m_FrameSize = readSize;
                //m_frameData->SetDataSize(m_pContext->m_FrameSize);
                SwapData((Ipp8u*)m_frameData->GetDataPointer(), (Ipp32u)m_frameData->GetDataSize());
                umcRes = StartCodesProcessing((Ipp8u*)m_frameData->GetDataPointer(),
                                               m_pContext->m_Offsets,
                                               m_pContext->m_values,
                                               true,
                                               true);

            }
            else //Simple/Main profiles pack without Start Codes
            {
                // copy data to self buffer
                ippsCopy_8u((Ipp8u*)m_pCurrentIn->GetDataPointer(),
                            m_dataBuffer,
                            (Ipp32u)m_pCurrentIn->GetDataSize());
                m_pContext->m_FrameSize = (Ipp32u)m_pCurrentIn->GetDataSize();
                m_frameData->SetDataSize(m_pContext->m_FrameSize);
                SwapData((Ipp8u*)m_frameData->GetDataPointer(), m_pContext->m_FrameSize);
                umcRes = SMProfilesProcessing(m_dataBuffer);
            }
        }
    }
    else // Data already swapped. We sure about align
    {
        if ((!in_ex->GetExData()->offsets)&&
            (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE))
        {
            //printf("Using MediaDataEx without StartCodes and swapping input data into splitter is impossible!\n");
            return UMC_ERR_FAILED;
        }
        else
        {
            if (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE)
            {
                m_pContext->m_Offsets = in_ex->GetExData()->offsets;
                m_pContext->m_values = in_ex->GetExData()->values;
                m_pContext->m_FrameSize = (Ipp32u)m_pCurrentIn->GetDataSize();
                m_frameData->SetBufferPointer((Ipp8u*)m_pCurrentIn->GetDataPointer(),m_pContext->m_FrameSize);
                m_frameData->SetDataSize(m_pContext->m_FrameSize);
                umcRes = StartCodesProcessing((Ipp8u*)in_ex->GetDataPointer(),
                    m_pContext->m_Offsets,
                    m_pContext->m_values,
                    true,
                    true);
            }
            else
            {
                m_pContext->m_FrameSize = (Ipp32u)m_pCurrentIn->GetDataSize();
                m_frameData->SetDataSize(m_pContext->m_FrameSize);
                umcRes = SMProfilesProcessing((Ipp8u*)m_pCurrentIn->GetDataPointer());
            }

        }
    }
    return umcRes;
}

Status VC1VideoDecoder::ParseInputBitstream()
{
    Status umcRes = UMC_OK;
    MediaDataEx *in_ex = DynamicCast<MediaDataEx,MediaData>(m_pCurrentIn);
    if (in_ex)
        umcRes = ParseStreamFromMediaDataEx(in_ex);
    else
    {
        if((m_decoderFlags & FLAG_VDEC_4BYTE_ACCESS) == FLAG_VDEC_4BYTE_ACCESS)
        {
            //printf("Using MediaData with swapping input data into splitter is impossible!\n");
            return UMC_ERR_FAILED;
        }
        else
            umcRes = ParseStreamFromMediaData();
    }
    return umcRes;
}
void VC1VideoDecoder::MSWMVPreprocessing()
{
    Ipp32u start_code = *((Ipp32u*)(m_pCurrentIn->GetBufferPointer()));
    switch (start_code)
    {
    case 0x0D010000:
    case 0x0F010000:
    case 0x0A010000:
    case 0x0E010000:
    case 0x0C010000:
    case 0x1B010000:
    case 0x1C010000:
    case 0x1D010000:
    case 0x1E010000:
    case 0x1F010000:
        // nothing to do
        m_bIsNeedAddFrameSC = false;
#ifdef CREATE_ES
        if (start_code != 0x0F010000) // not start code
            fwrite(m_pCurrentIn->GetDataPointer(), 1, m_pCurrentIn->GetDataSize(), m_fPureVideo);
#endif
        return ;
    default:
#ifdef CREATE_ES
            Ipp32u begin = 0x0D010000;
            fwrite(&begin, 1, 4, m_fPureVideo);
            fwrite(m_pCurrentIn->GetDataPointer(), 1, m_pCurrentIn->GetDataSize(), m_fPureVideo);
#endif
        m_bIsNeedAddFrameSC = true;
        return;
    }
}

Status VC1VideoDecoder::GetFrame(MediaData* in, MediaData* out)
{
    Status umcRes = UMC_OK;
    VideoDecoderParams params;


    VideoData *out_data = DynamicCast<VideoData, MediaData>(out);

    if (NULL == out_data)
    {
        return UMC_ERR_NULL_PTR;
    }

    if(in!=NULL && (Ipp32u)in->GetDataSize() == 0)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if(!m_pContext)
        return UMC_ERR_NOT_INITIALIZED;

   STATISTICS_START_TIME(m_timeStatistics->startTime);

   m_pCurrentIn = in;
    if (out_data)
    {
        m_pCurrentOut = out_data;
        m_pCurrentOut->SetFrameType(NONE_PICTURE);
    }

    if (in == NULL)
    {
        // in should be always present
        return UMC_ERR_FAILED;
    }
    else
    {
        if (m_AllocBuffer < in->GetDataSize())
            return UMC_ERR_NOT_ENOUGH_BUFFER;

        if (!m_PostProcessing)
        {
            if (!m_va)
                m_PostProcessing = m_allocatedPostProcessing = UMC::createVideoProcessing();
        }
        umcRes = ParseInputBitstream();

        if (UMC_OK == umcRes ||
            (UMC_ERR_NOT_ENOUGH_DATA == umcRes && m_pCurrentOut->GetFrameType() != NONE_PICTURE))
        {
            if (out_data)
            {
                if(-1.0 == in->GetTime())
                {
                    out_data->SetTime(m_pts);
                    GetPTS(in->GetTime());
                }
                else
                {
                    out_data->SetTime(in->GetTime());
                    m_pts = in->GetTime();
                }

            }
        }
   STATISTICS_END_TIME  (m_timeStatistics->startTime,
                         m_timeStatistics->endTime,
                         m_timeStatistics->totalTime);


    }

    return umcRes;
}

Status VC1VideoDecoder::Close(void)
{
    Status umcRes = UMC_OK;

    m_AllocBuffer = 0;

    if (m_pStore)
        m_pStore->StopDecoding();

    // reset all values 
    umcRes = Reset();

    if(m_pdecoder)
    {
        for(Ipp32u i = 0; i < m_iThreadDecoderNum; i += 1)
        {
#ifdef  VC1_THREAD_STATISTIC
            if (m_eEntryArray[i])
            {
                ippsFree(m_eEntryArray[i]);
                m_eEntryArray[i] = NULL;
            }
            if (m_pdecoder[i]->m_pJobSlice->m_Statistic)
            {
                delete m_pdecoder[i]->m_pJobSlice->m_Statistic;
                m_pdecoder[i] = NULL;
            }
#endif
            if(m_pdecoder[i])
            {
                //if (i > 0)
                //    m_pdecoder[i]->WaitAndStop();

                delete m_pdecoder[i];
                m_pdecoder[i] = 0;
            }
        }
#ifdef  VC1_THREAD_STATISTIC
        ippsFree(m_eEntryArray);
        m_eEntryArray = NULL;

#endif
        m_pdecoder = NULL;
    }

    if (m_pStore)
    {
        delete m_pStore;
        m_pStore = 0;
    }
    
    FreeAlloc(m_pContext);

    if(m_pMemoryAllocator)
    {
        if (static_cast<int>(m_iMemContextID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iMemContextID);
            m_pMemoryAllocator->Free(m_iMemContextID);
            m_iMemContextID = (MemID)-1;
        }

        if (static_cast<int>(m_iHeapID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iHeapID);
            m_pMemoryAllocator->Free(m_iHeapID);
            m_iHeapID = (MemID)-1;
        }

        if (static_cast<int>(m_iFrameBufferID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iFrameBufferID);
            m_pMemoryAllocator->Free(m_iFrameBufferID);
            m_iFrameBufferID = (MemID)-1;
        }

        if (static_cast<int>(m_iNewMemID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iNewMemID);
            m_pMemoryAllocator->Free(m_iNewMemID);
            m_iNewMemID = (MemID)-1;
        }
    }

    m_pContext = NULL;
    m_dataBuffer = NULL;
    m_stCodes = NULL;
    m_frameData = NULL;
    m_pHeap = NULL;

    memset(&m_pInitContext,0,sizeof(VC1Context));

    if (m_allocatedPostProcessing)
    {
        delete m_allocatedPostProcessing;
        m_allocatedPostProcessing = 0;
    }
    m_pMemoryAllocator = 0;

#ifdef CREATE_ES
    if (m_fPureVideo)
        fclose(m_fPureVideo);
#endif
#ifdef _PROJECT_STATISTICS_
    if(m_timeStatistics)
        WriteStatisticResults();
    DeleteStatistics();
#endif


    if (m_va)
        {
            if ((m_pSelfDecoder)&&
                (m_pSelfDecoder != this))
            {
                delete m_pSelfDecoder;
                m_pSelfDecoder = NULL;
            }
        }

#if defined (_WIN32) && (_DEBUG)
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).Release();
#endif
#endif

    m_iThreadDecoderNum = 0;
    m_decoderInitFlag = 0;
    m_decoderFlags = 0;
    m_bIsNeedAddFrameSC = false;
    return umcRes;
}


Status VC1VideoDecoder::Reset(void)
{
    Status umcRes = UMC_OK;

    if (m_pStore)
        m_pStore->StopDecoding();


    if(m_pContext==NULL)
        return UMC_ERR_NOT_INITIALIZED;

    m_bIsWarningStream = false;
    m_bLastFrameNeedDisplay = false;
   if (m_pStore)
   {
 
        if (!m_pStore->Reset())
            return UMC_ERR_NOT_INITIALIZED;
    
    #if defined UMC_VA_DXVA || defined UMC_VA_LINUX
            m_pStore->CreateDSQueue(&m_pInitContext,m_va);
    #else
            m_pStore->CreateDSQueue(&m_pInitContext,m_bIsReorder,0);
    #endif
            m_pStore->CreateOutBuffersQueue();
            m_pStore->SetDefinition(&(m_pInitContext.m_seqLayerHeader)); 
            m_pStore->FillIdxVector(2 * (m_iMaxFramesInProcessing + VC1NUMREFFRAMES));
    }
    m_lFrameCount = 0;
    m_iRefFramesDst = 0;
    m_iBFramesDst = 0;

    m_pContext->m_frmBuff.m_iDisplayIndex =  -1;
    m_pContext->m_frmBuff.m_iCurrIndex    =  -1;
    m_pContext->m_frmBuff.m_iPrevIndex    =  -1;
    m_pContext->m_frmBuff.m_iNextIndex    =  -1;

    if (m_pContext->m_pSingleMB)
    {
        m_pContext->m_pSingleMB->m_currMBXpos = -1;
        m_pContext->m_pSingleMB->m_currMBYpos = 0;
    }

    m_pContext->m_pCurrMB = &m_pContext->m_MBs[0];
    m_pContext->CurrDC = m_pContext->DCACParams;
    m_pContext->DeblockInfo.HeightMB = 0;
    m_pContext->DeblockInfo.start_pos = 0;
    m_pContext->DeblockInfo.is_last_deblock = 1;
    m_pts = 0;
    m_bIsExternalFR = false;
    m_bIsFrameToOut = false;

    return umcRes;
}


Status VC1VideoDecoder::GetInfo(BaseCodecParams* pbInfo)
{
    Status umcRes = UMC_OK;

    VideoDecoderParams *pInfo = DynamicCast<VideoDecoderParams> (pbInfo);

    //check initialization
    if(m_pContext==NULL)
        return UMC_ERR_NOT_INITIALIZED;

    // check error(s)
    if (NULL == pInfo)
        return UMC_ERR_NULL_PTR;


    pInfo->info = m_ClipInfo;

    pInfo->profile = m_pContext->m_seqLayerHeader.PROFILE;
    pInfo->level = m_pContext->m_seqLayerHeader.LEVEL;
    pInfo->numThreads = m_iThreadDecoderNum;


    return umcRes;
}


Status VC1VideoDecoder::GetPerformance(Ipp64f /**perf*/)
{
    Status umcRes = UMC_OK;
    return umcRes;
}


Status VC1VideoDecoder::ResetSkipCount()
{
    Status umcRes = UMC_OK;
    return umcRes;
}


Status VC1VideoDecoder::SkipVideoFrame(Ipp32s)
{
    Status umcRes = UMC_OK;
    return umcRes;
}


Ipp32u VC1VideoDecoder::GetNumOfSkippedFrames()
{
    return 0;
}


void VC1VideoDecoder::GetPTS(Ipp64f in_pts)
{
   if(in_pts == -1.0 && m_pts == -1.0)
       m_pts = 0;
   else if (in_pts == -1.0)
   {
//       Ipp64f diff = m_pts;
       if(m_ClipInfo.framerate != 0.0)
       {
           m_pts = m_pts + 1.0/m_ClipInfo.framerate - m_pts_dif;
       }
       else
       {
           m_pts = m_pts + 1.0/24.0 - m_pts_dif;
       }
       //if (m_pStore)
       //{
       //    diff = m_pts - diff;

       //    if ((m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->RFF) &&
       //        (m_decoderFlags & UMC::FLAG_VDEC_TELECINE_PTS))
       //    {
       //        m_pts += diff/2.0;
       //    }
       //}
   }
   else
   {
       if(m_pts_dif == 0)
       {
           if(m_ClipInfo.framerate)
                m_pts = m_pts + 1.0/m_ClipInfo.framerate - m_pts_dif;
           else
                m_pts = m_pts + 1.0/24.0 - m_pts_dif;

           m_pts_dif = in_pts - m_pts;
       }
       else
       {
            if(m_ClipInfo.framerate)
                m_pts = m_pts + 1.0/m_ClipInfo.framerate - m_pts_dif;
            else
                m_pts = m_pts + 1.0/24.0 - m_pts_dif;
       }
       //if (m_pStore)
       //{
       //    if ( m_pStore->GetLastDS()->m_pContext->m_picLayerHeader->RFF &&
       //        (m_decoderFlags & UMC::FLAG_VDEC_TELECINE_PTS))
       //    {
       //        m_pts -= m_pts_dif/2.0;
       //    }
       //}
   }
}

bool VC1VideoDecoder::GetFPS(VC1Context* pContext)
{
    // no need to modify frame rate if it set by application
    if (m_bIsExternalFR)
        return false;

    Ipp64f prevFPS = m_ClipInfo.framerate;
    if((m_ClipInfo.stream_subtype == VC1_VIDEO_VC1)||(m_ClipInfo.stream_type == static_cast<int>(WVC1_VIDEO)))
    {
        m_ClipInfo.bitrate = pContext->m_seqLayerHeader.BITRTQ_POSTPROC;
        m_ClipInfo.framerate = pContext->m_seqLayerHeader.FRMRTQ_POSTPROC;

        //for advanced profile
        if ((m_ClipInfo.framerate == 0.0) && (m_ClipInfo.bitrate == 31))
            {
                //Post processing indicators for Frame Rate and Bit Rate are undefined
                m_ClipInfo.bitrate = 0;
            }
            else if ((m_ClipInfo.framerate == 0.0) && (m_ClipInfo.bitrate == 30))
            {
                m_ClipInfo.framerate =  2.0;
                m_ClipInfo.bitrate = 1952;
            }
            else if ((m_ClipInfo.framerate == 1.0) && (m_ClipInfo.bitrate == 31))
            {
                m_ClipInfo.framerate =  6.0;
                m_ClipInfo.bitrate = 2016;
            }
            else
            {
                if (m_ClipInfo.framerate == 7.0)
                    m_ClipInfo.framerate = 30.0;
                else
                    m_ClipInfo.framerate = (2.0 + m_ClipInfo.framerate*4);

                if (m_ClipInfo.bitrate == 31)
                    m_ClipInfo.bitrate = 2016;
                else
                    m_ClipInfo.bitrate = (32 + m_ClipInfo.bitrate * 64);
            }
    }
    else 
    {
        m_ClipInfo.bitrate = pContext->m_seqLayerHeader.BITRTQ_POSTPROC;
        m_ClipInfo.framerate = pContext->m_seqLayerHeader.FRMRTQ_POSTPROC;

        if (7.0 == m_ClipInfo.framerate)
            m_ClipInfo.framerate = 30.0;
        else if (m_ClipInfo.framerate != 0.0)
            m_ClipInfo.framerate = (2.0 + m_ClipInfo.framerate*4);
        else 
            m_ClipInfo.framerate = 24.0;
    }
    

    if(m_ClipInfo.framerate < 0.0)
        m_ClipInfo.framerate = 24.0;

    m_ClipInfo.framerate = MapFrameRateIntoUMC(pContext->m_seqLayerHeader.FRAMERATENR, 
                                               pContext->m_seqLayerHeader.FRAMERATEDR, 
                                               pContext->m_seqLayerHeader.FRMRTQ_POSTPROC); 


    return (prevFPS != m_ClipInfo.framerate);

}


Status VC1VideoDecoder::WriteFrame(MediaData* in_UpLeveldata, VC1Context* pContext, UMC::VideoData* out_data)
{
STATISTICS_START_TIME(m_timeStatistics->ColorConv_StartTime);
    IppiSize roiSize;
    Status vc1_sts;

    roiSize.height = 2*(pContext->m_seqLayerHeader.MAX_CODED_HEIGHT+1);
    roiSize.width = 2*(pContext->m_seqLayerHeader.MAX_CODED_WIDTH+1);

    Ipp32s Index = pContext->m_frmBuff.m_iDisplayIndex;
    UMC::VideoData in_data;
    Ipp64f start_pts =  -1;
    Ipp64f end_pts =  -1;

    in_data.Init(roiSize.width, roiSize.height, YUV_VC1);

    if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
    {
        if (((pContext->m_picLayerHeader->RANGEREDFRM-1) == *pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iDisplayIndex].pRANGE_MAPY)||
            (pContext->m_picLayerHeader->PTYPE != VC1_B_FRAME)||
            !m_bLastFrameNeedDisplay)
        {
            in_data.SetPlaneBitDepth(*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iDisplayIndex].pRANGE_MAPY, 0);
            in_data.SetPlaneBitDepth(*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iDisplayIndex].pRANGE_MAPY, 1);
            in_data.SetPlaneBitDepth(*pContext->m_frmBuff.m_pFrames[pContext->m_frmBuff.m_iDisplayIndex].pRANGE_MAPY, 2);
        }
        else if ((pContext->m_picLayerHeader->RANGEREDFRM == 8)&&
                 (pContext->m_picLayerHeader->PTYPE == VC1_B_FRAME))
        {
            in_data.SetPlaneBitDepth(7, 0);
            in_data.SetPlaneBitDepth(7, 1);
            in_data.SetPlaneBitDepth(7, 2);
        }
        //warning - B Range Reduction does not match anchor frame
        else
        {
            in_data.SetPlaneBitDepth(-1, 0);
            in_data.SetPlaneBitDepth(-1, 1);
            in_data.SetPlaneBitDepth(-1, 2);
        }

    }
    else
    {
        in_data.SetPlaneBitDepth(pContext->m_seqLayerHeader.RANGE_MAPY, 0);
        in_data.SetPlaneBitDepth(pContext->m_seqLayerHeader.RANGE_MAPUV, 1);
        in_data.SetPlaneBitDepth(pContext->m_seqLayerHeader.RANGE_MAPUV, 2);

    }

    in_data.SetPlanePointer((void*)pContext->m_frmBuff.m_pFrames[Index].m_pY, 0);
    in_data.SetPlanePointer((void*)pContext->m_frmBuff.m_pFrames[Index].m_pU, 1);
    in_data.SetPlanePointer((void*)pContext->m_frmBuff.m_pFrames[Index].m_pV, 2);
    in_data.SetPlanePitch(pContext->m_frmBuff.m_pFrames[Index].m_iYPitch, 0);
    in_data.SetPlanePitch(pContext->m_frmBuff.m_pFrames[Index].m_iUPitch, 1);
    in_data.SetPlanePitch(pContext->m_frmBuff.m_pFrames[Index].m_iVPitch, 2);

    if (in_UpLeveldata)
        in_UpLeveldata->GetTime(start_pts,end_pts);
    if (!m_bIsWMPSplitter)
        GetPTS(start_pts);
    //else
    //    m_pts = end_pts;

    in_data.SetTime(start_pts,m_pts);

    vc1_sts = m_PostProcessing->GetFrame(&in_data, out_data);
STATISTICS_END_TIME(m_timeStatistics->ColorConv_StartTime,
                    m_timeStatistics->ColorConv_EndTime,
                    m_timeStatistics->ColorConv_TotalTime);

    return vc1_sts;
}

Status VC1VideoDecoder::CreateFrameBuffer(Ipp32u bufferSize)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VC1VideoDecoder::CreateFrameBuffer");
    if(m_dataBuffer == NULL)
    {
       if (m_pMemoryAllocator->Alloc(&m_iFrameBufferID,
                                     bufferSize,
                                     UMC_ALLOC_PERSISTENT,
                                     16) != UMC_OK)
                                     return UMC_ERR_ALLOC;
        m_dataBuffer = (Ipp8u*)(m_pMemoryAllocator->Lock(m_iFrameBufferID));
        if(m_dataBuffer==NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }
    }

    memset(m_dataBuffer,0,bufferSize);
    m_pContext->m_pBufferStart  = (Ipp8u*)m_dataBuffer;
    m_pContext->m_bitstream.pBitstream       = (Ipp32u*)(m_pContext->m_pBufferStart);

    if(m_frameData == NULL)
    {
        m_pHeap->s_new(&m_frameData);
    }

    m_frameData->SetBufferPointer(m_dataBuffer, bufferSize);
    m_frameData->SetDataSize(bufferSize);
    m_frameData->SetExData(m_stCodes);

    return UMC_OK;
}

Status VC1VideoDecoder::GetStartCodes (Ipp8u* pDataPointer,
                                       Ipp32u DataSize,
                                       MediaDataEx* out,
                                       Ipp32u* readSize)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VC1VideoDecoder::GetStartCodes");
    Ipp8u* readPos = pDataPointer;
    Ipp32u readBufSize =  DataSize;
    Ipp8u* readBuf = pDataPointer;

    Ipp8u* currFramePos = (Ipp8u*)out->GetBufferPointer();
    Ipp32u frameSize = 0;
    Ipp32u frameBufSize = (Ipp32u)out->GetBufferSize();
    MediaDataEx::_MediaDataEx *stCodes = out->GetExData();

    Ipp32u size = 0;
    Ipp8u* ptr = NULL;
    Ipp32u readDataSize = 0;
    Ipp32u zeroNum = 0;
    Ipp32u a = 0x0000FF00 | (*readPos);
    Ipp32u b = 0xFFFFFFFF;
    Ipp32u FrameNum = 0;
    
    Ipp32u shift = 0;

    bool isWriteSlice = false;

    memset(stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
    memset(stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));

    if (m_bIsNeedAddFrameSC)
    {
        *(Ipp32u*)currFramePos = 0x0D010000;
        currFramePos += 4;
        stCodes->offsets[stCodes->count] = 0;
        stCodes->values[stCodes->count] = 0x0D010000;
        stCodes->count++;
        shift += 4;
    }

    while(readPos < (readBuf + readBufSize))
    {
        if (stCodes->count > 512)
            return UMC_ERR_INVALID_STREAM;
        //find sequence of 0x000001 or 0x000003
        while(!( b == 0x00000001 || b == 0x00000003 )
            &&(++readPos < (readBuf + readBufSize)))
        {
            a = (a<<8)| (Ipp32s)(*readPos);
            b = a & 0x00FFFFFF;
        }

        //check end of read buffer
        if(readPos < (readBuf + readBufSize - 1))
        {
            if(*readPos == 0x01)
            {
                if ((IS_VC1_DATA_SC(*(readPos + 1)) || IS_VC1_USER_DATA(*(readPos + 1))) &&
                    ( ((stCodes->count > 0 &&
                      IS_VC1_DATA_SC(stCodes->values[/*stCodes->count - 1*/0] >> 24)) ||
                      isWriteSlice)))
                {
                    isWriteSlice = false;
                    readPos+=2;
                    ptr = readPos - 5;
                    if (stCodes->count) // if not first start code
                    {
                        //trim zero bytes
                        while ( (*ptr==0) && (ptr > readBuf) )
                            ptr--;
                    }

                    //slice or field size
                    size = (Ipp32u)(ptr - readBuf - readDataSize+1);

                    if(frameSize + size > frameBufSize)
                        return UMC_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, currFramePos, size);

                    currFramePos = currFramePos + size;
                    frameSize = frameSize + size;

                    zeroNum = frameSize - 4*((frameSize)/4);
                    if(zeroNum!=0)
                        zeroNum = 4 - zeroNum;

                    memset(currFramePos, 0, zeroNum);

                    //set write parameters
                    currFramePos = currFramePos + zeroNum;
                    frameSize = frameSize + zeroNum;

                    stCodes->offsets[stCodes->count] = frameSize;
                    if (m_bIsNeedAddFrameSC)
                        stCodes->offsets[stCodes->count] += 4;
                    
                    stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                    readDataSize = (Ipp32u)(readPos - readBuf - 4);

                    a = 0x00010b00 |(Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;

                    zeroNum = 0;
                    stCodes->count++;
                }
                else
                {
                    if (stCodes->count && IS_VC1_USER_DATA(stCodes->values[stCodes->count - 1] >> 24))
                    {
                        if (IS_VC1_DATA_SC(*(readPos + 1)))
                            isWriteSlice = true;
                        else
                            isWriteSlice = false;

                        readPos+=2;
                        a = (a<<8)| (Ipp32s)(*readPos);
                        b = a & 0x00FFFFFF;
                        readDataSize = (Ipp32u)(readPos - readBuf - 4);
                        continue;
                    }
                    else if(FrameNum)
                    {
                        readPos+=2;
                        ptr = readPos - 5;
                        //trim zero bytes
                        if (stCodes->count) // if not first start code
                        {
                            while ( (*ptr==0) && (ptr > readBuf) )
                                ptr--;
                        }

                        //slice or field size
                        size = (Ipp32u)(readPos - readBuf - readDataSize - 4);

                        if(frameSize + size > frameBufSize)
                            return UMC_ERR_NOT_ENOUGH_BUFFER;

                        ippsCopy_8u(readBuf + readDataSize, currFramePos, size);

                        //currFramePos = currFramePos + size;
                        frameSize = frameSize + size;

                        stCodes->offsets[stCodes->count] = frameSize;
                        if (m_bIsNeedAddFrameSC)
                            stCodes->offsets[stCodes->count] += 4;
                        stCodes->values[stCodes->count]  = ((*(readPos-1))<<24) + ((*(readPos-2))<<16) + ((*(readPos-3))<<8) + (*(readPos-4));

                        stCodes->count++;

                        out->SetDataSize(frameSize + shift);
                        readDataSize = readDataSize + size;
                        //*readSize = readDataSize + shift;
                        *readSize = readDataSize;
                        return UMC_OK;
                    }
                    else 
                    {
                        //beginning of frame
                        readPos++;
                        a = 0x00000100 |(Ipp32s)(*readPos);
                        b = a & 0x00FFFFFF;

                        //end of seguence
                        if((((*(readPos))<<24) + ((*(readPos-1))<<16) + ((*(readPos-2))<<8) + (*(readPos-3))) == 0x0A010000)
                        {
                            *readSize = 4;
                            stCodes->offsets[stCodes->count] = (Ipp32u)(0);
                            stCodes->values[stCodes->count]  = ((*(readPos))<<24) + ((*(readPos-1))<<16) + ((*(readPos-2))<<8) + (*(readPos-3));
                            stCodes->count++;
                            out->SetDataSize(4);      
                            ippsCopy_8u(readBuf + readDataSize, currFramePos, 4);
                            return UMC_OK;
                        }

                        stCodes->offsets[stCodes->count] = (Ipp32u)(0);
                        stCodes->values[stCodes->count]  = ((*(readPos))<<24) + ((*(readPos-1))<<16) + ((*(readPos-2))<<8) + (*(readPos-3));

                        stCodes->count++;
                        zeroNum = 0;
                        FrameNum++;
                    }
                }
            }
            else //if(*readPos == 0x03)
            {
                //000003
                if((*(readPos + 1) <  0x04))
                {
                    size = (Ipp32u)(readPos - readBuf - readDataSize);
                    if(frameSize + size > frameBufSize)
                        return UMC_ERR_NOT_ENOUGH_BUFFER;

                    ippsCopy_8u(readBuf + readDataSize, currFramePos, size);
                    frameSize = frameSize + size;
                    currFramePos = currFramePos + size;
                    zeroNum = 0;

                    readPos++;
                    a = (a<<8)| (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;

                    readDataSize = readDataSize + size + 1;
                }
                else
                {
                    readPos++;
                    a = (a<<8)| (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;
                }
            }

        }
        else
        {
            // last portion pf user data
            if (!IS_VC1_USER_DATA(stCodes->values[stCodes->count - 1] >> 24) || isWriteSlice)
            {
                //end of stream
                size = (Ipp32u)(readPos- readBuf - readDataSize);

                if(frameSize + size > frameBufSize)
                {
                    return UMC_ERR_NOT_ENOUGH_BUFFER;
                }

                ippsCopy_8u(readBuf + readDataSize, currFramePos, size);
            }
            else
            {
                stCodes->values[stCodes->count - 1] = 0;
                stCodes->offsets[stCodes->count - 1] = 0;
            }


            out->SetDataSize(frameSize + size + shift);
            readDataSize = readDataSize + size;
            //*readSize = readDataSize + shift;
            *readSize = readDataSize;
            return UMC_OK;
        }
    }
    return UMC_OK;
}


void VC1VideoDecoder::GetFrameSize(MediaData* in)
{
    Ipp32s frameSize = 0;
    Ipp8u* ptr = m_pContext->m_pBufferStart;
    Ipp32u i = 0;
    Ipp32u* offset = m_pContext->m_Offsets;
    Ipp32u* value = m_pContext->m_values;

    m_pContext->m_FrameSize = (Ipp32u)in->GetDataSize();

    if(m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {
        value++;
        i++;
        while((*value == 0x0B010000)||(*value == 0x0C010000))
        {
            value ++;
            i++;
        }
        if(*value != 0x00000000)
        {
            m_pContext->m_FrameSize = offset[i];
        }
    }
    else
    {
        if ((*m_pContext->m_bitstream.pBitstream&0xFF) == 0xC5) // sequence header
            frameSize = 36; // size of seq header for .rcv format
        else
        {
            frameSize  = ((*(ptr))<<24) + ((*(ptr + 1))<<16) + ((*(ptr + 2))<<8) + *(ptr + 3);
            frameSize &= 0x0fffffff;
            frameSize += 8;
        }
    }
}
Status VC1VideoDecoder::VC1DecodeFrame(VC1VideoDecoder* pDec,MediaData* in, VideoData* out_data)
{
    if (pDec != this)
        return UMC_ERR_INIT;

    Status umcRes = UMC_OK;
    bool skip = false;

    VC1FrameDescriptor *pCurrDescriptor = NULL;

    m_pStore->GetReadyDS(&pCurrDescriptor);

    if (NULL == pCurrDescriptor)
        throw vc1_exception(internal_pipeline_error);


    try // check input frame for valid
    {
        umcRes = pCurrDescriptor->preProcData(m_pContext,
                                              GetCurrentFrameSize(),
                                              m_lFrameCount,
                                              m_bIsWMPSplitter,
                                              skip);
        if (UMC_OK != umcRes)
        {

            if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
                throw vc1_exception(invalid_stream);
            else
                throw vc1_exception(internal_pipeline_error);
        }

        if (VC1_SKIPPED_FRAME == pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE)
            out_data->SetFrameType(D_PICTURE); // means skipped 
        else
            out_data->SetFrameType(I_PICTURE);

        pCurrDescriptor->m_pContext->typeOfPreviousFrame =  m_pContext->typeOfPreviousFrame;

        if (!pCurrDescriptor->isEmpty())//check descriptor correctness
            pCurrDescriptor->VC1FrameDescriptor::processFrame(m_pContext->m_Offsets,m_pContext->m_values);

    }
    catch (vc1_exception ex)
    {
        if (invalid_stream == ex.get_exception_type())
        {
            if (fast_err_detect == vc1_except_profiler::GetEnvDescript().m_Profile)
                m_pStore->AddInvalidPerformedDS(pCurrDescriptor);
            else if (fast_decoding == vc1_except_profiler::GetEnvDescript().m_Profile)
                m_pStore->ResetPerformedDS(pCurrDescriptor);
            else
                // Error - let speak about it 
                m_pStore->ResetPerformedDS(pCurrDescriptor);
            // for smart decoding we should decode and reconstruct frame according to standard pipeline

            return  UMC_ERR_NOT_ENOUGH_DATA;
        }
        else if (mem_allocation_er == ex.get_exception_type())
            return UMC_ERR_LOCK;
        else
            return UMC_ERR_FAILED;
    }


    if (umcRes == UMC_WRN_INVALID_STREAM)
        m_bIsWarningStream = true;

    // Update round ctrl field of seq header.
    m_pContext->m_seqLayerHeader.RNDCTRL = pCurrDescriptor->m_pContext->m_seqLayerHeader.RNDCTRL;


    // for Intensity compensation om multi-threading model
    if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
    {
        m_pContext->m_bIntensityCompensation = pCurrDescriptor->m_pContext->m_bIntensityCompensation;
    }


    if (!pCurrDescriptor->isEmpty())//check descriptor correctness
    {
        if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
            m_pStore->SetDstForFrameAdv(pCurrDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
        else
            m_pStore->SetDstForFrame(pCurrDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
    }



    // Wake Upppp
    //m_pStore->WakeUP();

    if (m_lFrameCount == 1)
    {
        //m_pStore->StartDecoding();
        if (m_bIsReorder)
            m_bLastFrameNeedDisplay = true;

        // we should beffer reference frames 
        //if ((2 == m_pStore->GetProcFramesNumber())&& 
        //    (umcRes != UMC_ERR_INVALID_STREAM))
        //{
        //    m_bIsFrameToOut = true;
        //}
        //else
            m_bIsFrameToOut = false;

    }
    else
        m_bIsFrameToOut = true;


STATISTICS_END_TIME(m_timeStatistics->ThreadPrep_StartTime,
                    m_timeStatistics->ThreadPrep_EndTime,
                    m_timeStatistics->ThreadPrep_TotalTime);

    
    //if ((m_pStore->GetProcFramesNumber() >= (m_iMaxFramesInProcessing - NumBufferedFrames - 1))&&
    //    (umcRes != UMC_ERR_INVALID_STREAM))
    //{
    //    m_bIsFrameToOut = true;
    //}
    //else
    //    m_bIsFrameToOut = false;



    if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
        m_pContext->typeOfPreviousFrame = pCurrDescriptor->m_pContext->m_picLayerHeader->FCM;

    if (!m_pExtFrameAllocator)  // MFX special mode
        umcRes = ProcessPerformedDS(in, out_data);

    if (!m_bIsFrameToOut && (UMC_OK == umcRes))
        umcRes = UMC_ERR_NOT_ENOUGH_DATA;

    m_pCurrentIn->MoveDataPointer(m_pContext->m_FrameSize);
    return umcRes;
}

Status VC1VideoDecoder::ProcessPrevFrame(VC1FrameDescriptor *pCurrDescriptor, MediaData* in, VideoData* out_data)
{
    VC1FrameDescriptor *pFirstDescriptor = NULL;
    Status umcRes = UMC_OK;
    pFirstDescriptor = m_pStore->GetFirstDS();

   if (pFirstDescriptor &&  m_bIsReorder)
    {
        if ((out_data!=NULL && pFirstDescriptor->m_iFrameCounter > 1)&&
            (VC1_IS_REFERENCE(pFirstDescriptor->m_pContext->m_picLayerHeader->PTYPE)))
            umcRes = WriteFrame(in,pFirstDescriptor->m_pContext,out_data);
    }
    if (m_pPrevDescriptor)
    {
        if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
        {
            RangeRefFrame(m_pPrevDescriptor->m_pContext);
        }
        else
            m_pStore->CompensateDSInQueue(m_pPrevDescriptor);

        m_pStore->WakeTasksInAlienQueue(m_pPrevDescriptor);
    }
    return umcRes;
}

void   VC1VideoDecoder::SetVideoHardwareAccelerator            (VideoAccelerator* va)
{
    if (va)
        m_va = (VideoAccelerator*)va;
}

bool VC1VideoDecoder::InitAlloc(VC1Context* pContext, Ipp32u MaxFrameNum)
{
    if(!InitTables(pContext))
        return false;

    //only if absence external frame allocator
        //frames allocation
        Ipp32s i;
        Ipp32u h = pContext->m_seqLayerHeader.MaxHeightMB * VC1_PIXEL_IN_LUMA;
        Ipp32u w = (pContext->m_seqLayerHeader.MaxWidthMB * VC1_PIXEL_IN_LUMA); // align on 128
        Ipp32s frame_size = h*w + (h / 2 * w / 2)*2;
        Ipp32s n_references = MaxFrameNum + VC1NUMREFFRAMES; //


        for(i = 0; i < n_references; i++)
        {
            if (!m_pExtFrameAllocator)
            { 
                pContext->m_frmBuff.m_pFrames[i].m_pAllocatedMemory =
                    pContext->m_frmBuff.m_pFrames[0].m_pAllocatedMemory + frame_size*i;

                pContext->m_frmBuff.m_pFrames[i].m_AllocatedMemorySize = frame_size;

                pContext->m_frmBuff.m_pFrames[i].m_pY = pContext->m_frmBuff.m_pFrames[i].m_pAllocatedMemory;
                pContext->m_frmBuff.m_pFrames[i].m_pU = pContext->m_frmBuff.m_pFrames[i].m_pAllocatedMemory
                    + (h * w);

                pContext->m_frmBuff.m_pFrames[i].m_pV = pContext->m_frmBuff.m_pFrames[i].m_pAllocatedMemory +
                    (h * w)+ (h / 2 )*(w / 2 );

                pContext->m_frmBuff.m_pFrames[i].m_iYPitch = (w);
                pContext->m_frmBuff.m_pFrames[i].m_iUPitch = (w / 2);
                pContext->m_frmBuff.m_pFrames[i].m_iVPitch = (w / 2);
            }

            pContext->m_frmBuff.m_pFrames[i].RANGE_MAPY  = -1;
            pContext->m_frmBuff.m_pFrames[i].RANGE_MAPUV = -1;
            pContext->m_frmBuff.m_pFrames[i].pRANGE_MAPY = &pContext->m_frmBuff.m_pFrames[i].RANGE_MAPY;
            pContext->m_frmBuff.m_pFrames[i].LumaTablePrev[0] = 0;
            pContext->m_frmBuff.m_pFrames[i].LumaTablePrev[1] = 0;
            pContext->m_frmBuff.m_pFrames[i].LumaTableCurr[0] = 0;
            pContext->m_frmBuff.m_pFrames[i].LumaTableCurr[1] = 0;
            pContext->m_frmBuff.m_pFrames[i].ChromaTablePrev[0] = 0;
            pContext->m_frmBuff.m_pFrames[i].ChromaTablePrev[1] = 0;
            pContext->m_frmBuff.m_pFrames[i].ChromaTableCurr[0] = 0;
            pContext->m_frmBuff.m_pFrames[i].ChromaTableCurr[1] = 0;
        }

    pContext->m_frmBuff.m_iDisplayIndex = -1;
    pContext->m_frmBuff.m_iCurrIndex    =  0;
    pContext->m_frmBuff.m_iPrevIndex    = -1;
    pContext->m_frmBuff.m_iNextIndex    = -1;
    pContext->m_frmBuff.m_iRangeMapIndex   =  MaxFrameNum + VC1NUMREFFRAMES - 1;
    pContext->m_frmBuff.m_iToFreeIndex = -1;
    return true;
}


void   VC1VideoDecoder::FreeAlloc(VC1Context* pContext)
{
    if(pContext)
    {
        FreeTables(pContext);
    }
}


void VC1VideoDecoder::SetVideoDecodingSpeed(VC1Skipping::SkippingMode  SkipMode)
{
    m_pStore->SetVideoDecodingSpeed(SkipMode);
}
Status VC1VideoDecoder::ChangeVideoDecodingSpeed(Ipp32s& speed_shift)
{
    if (m_pStore->ChangeVideoDecodingSpeed(speed_shift))
        return UMC_OK;
    else
        return UMC_ERR_FAILED;
}
Status VC1VideoDecoder::ProcessPerformedDS(MediaData* in, VideoData* out_data)
{
    VC1FrameDescriptor *pCurrDescriptor = NULL;
    
    Status umcRes = UMC_OK;
    Status umcWRes = UMC_OK;

    if (m_bIsFrameToOut) 
    {
        if (m_pStore->GetPerformedDS(&pCurrDescriptor))
        {

            if (!pCurrDescriptor->isDescriptorValid())
            {
                umcRes = UMC_ERR_FAILED;
            }
            else
            {

                if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_InitPicLayer->PTYPE))
                {
                   Ipp16u heightMB =  m_pContext->m_seqLayerHeader.heightMB;
                    if (pCurrDescriptor->m_pContext->m_InitPicLayer->FCM == VC1_FieldInterlace)
                    {
                        heightMB =  m_pContext->m_seqLayerHeader.heightMB +  (m_pContext->m_seqLayerHeader.heightMB & 1);
                        ippsCopy_8u(pCurrDescriptor->m_pContext->savedMVSamePolarity,
                            m_pContext->savedMVSamePolarity_Curr,
                            heightMB*m_pContext->m_seqLayerHeader.MaxWidthMB);
                    }

                    ippsCopy_16s(pCurrDescriptor->m_pContext->savedMV,
                        m_pContext->savedMV_Curr,
                        heightMB*m_pContext->m_seqLayerHeader.MaxWidthMB*2*2);
                }

                if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                    PrepareForNextFrame(pCurrDescriptor->m_pContext);

                if (!m_bIsReorder)
                {
                    if ((pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE < VC1_B_FRAME)&&
                        (pCurrDescriptor->m_pContext->m_bNeedToUseCompBuffer))
                    {
                        pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetRangeMapIndex();
                    }
                    else
                        pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex = pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex;


                    umcWRes = WriteFrame(in,pCurrDescriptor->m_pContext,out_data);
                    m_pStore->OpenNextFrames(pCurrDescriptor,&m_pPrevDescriptor,&m_iRefFramesDst,&m_iBFramesDst);

                    out_data->SetFrameType(m_pStore->GetOutputFrameType());

                    if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
                        out_data->SetFrameType(I_PICTURE);
                    else
                        out_data->SetFrameType(B_PICTURE);
                }
                else if (pCurrDescriptor->m_iFrameCounter > 1)
                {
                    if (!VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
                        umcWRes = WriteFrame(in,pCurrDescriptor->m_pContext,out_data);
                    m_pStore->OpenNextFrames(pCurrDescriptor,&m_pPrevDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
                }
                else
                {
                    m_pStore->OpenNextFrames(pCurrDescriptor,&m_pPrevDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
                    if (umcRes == UMC_OK)
                        umcRes = UMC_ERR_NOT_ENOUGH_DATA;
                }
                m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;
            }
        }
        else  if (m_pStore->GetReadySkippedDS(&pCurrDescriptor,false))
        {
            if (!pCurrDescriptor->isDescriptorValid())
            {
                umcRes = UMC_ERR_FAILED;
            }
            else
            {
                if (pCurrDescriptor->isSpecialBSkipFrame())
                {
                    pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex = pCurrDescriptor->m_pContext->m_frmBuff.m_iPrevIndex;
                    umcWRes = WriteFrame(in,pCurrDescriptor->m_pContext,out_data);
                    pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE = VC1_B_FRAME;
                }
                else
                {
                    DecodeSkippicture(pCurrDescriptor->m_pContext);
                    if (!m_bIsReorder)
                    {
                        pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex = pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex;
                        umcWRes = WriteFrame(in,pCurrDescriptor->m_pContext,out_data);
                        if (VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
                            out_data->SetFrameType(I_PICTURE);
                        else
                            out_data->SetFrameType(B_PICTURE);
                    }
                    if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
                        PrepareForNextFrame(pCurrDescriptor->m_pContext);
                }
                m_pStore->OpenNextFrames(pCurrDescriptor,&m_pPrevDescriptor,&m_iRefFramesDst,&m_iBFramesDst);
                umcRes = UMC_OK;
            }
        }
        else
            throw vc1_exception(internal_pipeline_error);


#ifdef  VC1_THREAD_STATISTIC
        PrintParallelStatistic(m_lFrameCount,pCurrDescriptor->m_pContext);
        for (Ipp32u i=0;i<m_iThreadDecoderNum;i++)
            m_pdecoder[i]->m_pJobSlice->m_Statistic->reset();
#endif
    }
    else if (umcRes == UMC_OK)
        umcRes = UMC_ERR_NOT_ENOUGH_DATA;

    return umcRes;

}

Status VC1VideoDecoder::CheckLevelProfileOnly(VideoDecoderParams *pParam)
{
    Ipp32u Profile;
    // we can init without data. Hence we cannot check profile
    UMC_CHECK_PTR(pParam->m_pData);
    Ipp32u* pData = (Ipp32u*)pParam->m_pData->GetDataPointer();
    // we can init without data. Hence we cannot check profile
    UMC_CHECK_PTR(pData);
    if ((WVC1_VIDEO == pParam->info.stream_subtype)||
        ((VC1_VIDEO_VC1) == pParam->info.stream_subtype))
    {
        //First double word can be start code
        if  (0x0F010000 == *pData)
            pData += 1;
    }
    // May be need to add pParam->profile too. Chech first two bits
    Profile = ((*pData)&0xC0) >> 6;
    if (VC1_IS_VALID_PROFILE(Profile))
        return UMC_OK;
    else
        return UMC_ERR_UNSUPPORTED;
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER

