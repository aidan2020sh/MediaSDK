/* ///////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MJPEG_VIDEO_DECODER)
#include <string.h>
#include "vm_debug.h"
#include "umc_video_data.h"
#include "umc_mjpeg_mfx_decode_hw.h"
#include "membuffin.h"

#include "umc_va_base.h"

#ifdef UMC_VA_DXVA
#include "umc_va_dxva2.h"

namespace UMC
{

MJPEGVideoDecoderMFX_HW::MJPEGVideoDecoderMFX_HW(void)
{
} // ctor

MJPEGVideoDecoderMFX_HW::~MJPEGVideoDecoderMFX_HW(void)
{
    Close();
} // dtor

Status MJPEGVideoDecoderMFX_HW::Init(BaseCodecParams* lpInit)
{
    UMC::Status status;
    Ipp32u i, numThreads;

    VideoDecoderParams* pDecoderParams;

    pDecoderParams = DynamicCast<VideoDecoderParams>(lpInit);

    if(0 == pDecoderParams)
        return UMC_ERR_NULL_PTR;

    status = Close();
    if(UMC_OK != status)
        return UMC_ERR_INIT;

    m_DecoderParams  = *pDecoderParams;

    m_IsInit       = true;
    //m_firstFrame   = true;
    m_interleaved  = false;
    m_frameNo      = 0;
    m_frame        = 0;
    m_frameSampling = 0;
    m_statusReportFeedbackCounter = 1;
    m_fourCC       = 0;

    m_pMemoryAllocator = pDecoderParams->lpMemoryAllocator;
    m_va = pDecoderParams->pVideoAccelerator;

    // allocate the JPEG decoders
    numThreads = JPEG_MAX_THREADS_HW;
    if ((m_DecoderParams.numThreads) &&
        (numThreads > (Ipp32u) m_DecoderParams.numThreads))
    {
        numThreads = m_DecoderParams.numThreads;
    }
    for (i = 0; i < numThreads; i += 1)
    {
        m_dec[i].reset(new CJPEGDecoder());
        if (NULL == m_dec[i].get())
        {
            return UMC_ERR_ALLOC;
        }
    }
    m_numDec = numThreads;

    VM_ASSERT(m_pMemoryAllocator);

    m_local_delta_frame_time = 1.0/30;
    m_local_frame_time       = 0;

    if (pDecoderParams->info.framerate)
    {
        m_local_delta_frame_time = 1 / pDecoderParams->info.framerate;
    }

    return UMC_OK;
} // MJPEGVideoDecoderMFX_HW::Init()

Status MJPEGVideoDecoderMFX_HW::Reset(void)
{
    m_statusReportFeedbackCounter = 1;
    {
        AutomaticUMCMutex guard(m_guard);

        m_cachedReadyTaskIndex.clear();
        m_cachedCorruptedTaskIndex.clear();
    }

    return MJPEGVideoDecoderMFX::Reset();
} // MJPEGVideoDecoderMFX_HW::Reset()

Status MJPEGVideoDecoderMFX_HW::Close(void)
{
    {
        AutomaticUMCMutex guard(m_guard);

        m_cachedReadyTaskIndex.clear();
        m_cachedCorruptedTaskIndex.clear();
    }

    return MJPEGVideoDecoderMFX::Close();
} // MJPEGVideoDecoderMFX_HW::Close()

Status MJPEGVideoDecoderMFX_HW::AllocateFrame()
{
    IppiSize size = m_frameDims;
    size.height = m_DecoderParams.info.clip_info.height;
    size.width = m_DecoderParams.info.clip_info.width;

    VideoDataInfo info;
    info.Init(size.width, size.height, NV12, 8);
    info.SetPictureStructure(m_interleaved ? VideoDataInfo::PS_FRAME : VideoDataInfo::PS_TOP_FIELD_FIRST);

    FrameMemID frmMID;
    Status sts = m_frameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC_OK)
    {
        return UMC_ERR_ALLOC;
    }

    m_frameData.Init(&info, frmMID, m_frameAllocator);

    return UMC_OK;
}

Status MJPEGVideoDecoderMFX_HW::CloseFrame(UMC::FrameData** in, const mfxU32 fieldPos)
{
    if((*in) != NULL)
    {
        (*in)[fieldPos].Close();
    }

    return UMC_OK;

} // Status MJPEGVideoDecoderMFX::CloseFrame(void)

mfxStatus MJPEGVideoDecoderMFX_HW::CheckStatusReportNumber(Ipp32u statusReportFeedbackNumber, mfxU16* corrupted)
{
    UMC::Status sts = UMC_OK;

    mfxU32 numStructures = 32;
    JPEG_DECODE_QUERY_STATUS queryStatus[32];

    std::set<mfxU32>::iterator iteratorReady;
    std::set<mfxU32>::iterator iteratorCorrupted;
    //in Case TDR occur status won't be updated, otherwise driver will change status to correct.
    for (mfxU32 i = 0; i < numStructures; i += 1){
        queryStatus[i].bStatus = 3;
    }
    // execute call
    sts = m_va->ExecuteStatusReportBuffer((void*)queryStatus, sizeof(JPEG_DECODE_QUERY_STATUS) * numStructures);

    // houston, we have a problem :)
    if(sts != UMC_OK)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    for (mfxU32 i = 0; i < numStructures; i += 1)
    {
        if (0 == queryStatus[i].bStatus || 1 == queryStatus[i].bStatus)
        {
            AutomaticUMCMutex guard(m_guard);
            m_cachedReadyTaskIndex.insert(queryStatus[i].StatusReportFeedbackNumber);
        }
        else if (2 == queryStatus[i].bStatus)
        {
            AutomaticUMCMutex guard(m_guard);
            m_cachedCorruptedTaskIndex.insert(queryStatus[i].StatusReportFeedbackNumber);
        }
        else if (3 == queryStatus[i].bStatus)
        {
            return MFX_ERR_DEVICE_FAILED;
        }
        else if (5 == queryStatus[i].bStatus)
        {
            // Operation is not completed; do nothing
            continue;
        }
    }

    {
        AutomaticUMCMutex guard(m_guard);
        iteratorReady = find(m_cachedReadyTaskIndex.begin(), m_cachedReadyTaskIndex.end(), statusReportFeedbackNumber);
    
        if (m_cachedReadyTaskIndex.end() == iteratorReady)
        {
            iteratorCorrupted = find(m_cachedCorruptedTaskIndex.begin(), m_cachedCorruptedTaskIndex.end(), statusReportFeedbackNumber);
            
            if(m_cachedCorruptedTaskIndex.end() == iteratorCorrupted)
            {
                return MFX_TASK_BUSY;
            }

            m_cachedCorruptedTaskIndex.erase(iteratorCorrupted);

            *corrupted = 1;

            return MFX_TASK_DONE;
        }

        m_cachedReadyTaskIndex.erase(iteratorReady);
    }

    return MFX_TASK_DONE;
}

Status MJPEGVideoDecoderMFX_HW::_DecodeHeader(CBaseStreamInput* in, Ipp32s* cnt)
{
    JSS      sampling;
    JERRCODE jerr;

    if (!m_IsInit)
        return UMC_ERR_NOT_INITIALIZED;

    jerr = m_dec[0]->SetSource(in);
    if(JPEG_OK != jerr)
        return UMC_ERR_FAILED;

    IppiSize size = {0};

    jerr = m_dec[0]->ReadHeader(
        &size.width, &size.height, &m_frameChannels, &m_color, &sampling, &m_framePrecision);

    if(JPEG_ERR_BUFF == jerr)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if(JPEG_OK != jerr)
        return UMC_ERR_FAILED;

    bool sizeHaveChanged = (m_frameDims.width != size.width || (m_frameDims.height != size.height && m_frameDims.height != (size.height << 1)));

    if ((m_frameSampling != (int)sampling) || (m_frameDims.width && sizeHaveChanged))
    {
        in->Seek(-m_dec[0]->GetNumDecodedBytes(),UIC_SEEK_CUR);
        *cnt = 0;
        return UMC_ERR_NOT_ENOUGH_DATA;//UMC_WRN_INVALID_STREAM;
    }

    *cnt = m_dec[0]->GetNumDecodedBytes();
    return UMC_OK;
}

Status MJPEGVideoDecoderMFX_HW::_DecodeField(MediaDataEx* in)
{
    Ipp32s     nUsedBytes = 0;

    CMemBuffInput stream;
    stream.Open((Ipp8u*)in->GetDataPointer(), (Ipp32s)in->GetDataSize());

    Status status = _DecodeHeader(&stream, &nUsedBytes);
    if (status > 0)
    {
        in->MoveDataPointer(nUsedBytes);
        return UMC_OK;
    }

    if(UMC_OK != status)
    {
        in->MoveDataPointer(nUsedBytes);
        return status;
    }

    status = GetFrameHW(in);
    if (status != UMC_OK)
    {
        return status;
    }

    in->MoveDataPointer(m_dec[0]->GetNumDecodedBytes());

    return status;
} // MJPEGVideoDecoderMFX_HW::_DecodeField(MediaDataEx* in)

Status MJPEGVideoDecoderMFX_HW::DefaultInitializationHuffmantables()
{    // VD [23.08.2010] - use default huffman tables if not presented in stream
    JERRCODE jerr;
    if(m_dec[0]->m_dctbl[0].IsEmpty())
    {
        jerr = m_dec[0]->m_dctbl[0].Create();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_dec[0]->m_dctbl[0].Init(0,0,(Ipp8u*)&DefaultLuminanceDCBits[0],(Ipp8u*)&DefaultLuminanceDCValues[0]);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    if(m_dec[0]->m_dctbl[1].IsEmpty())
    {
        jerr = m_dec[0]->m_dctbl[1].Create();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_dec[0]->m_dctbl[1].Init(1,0,(Ipp8u*)&DefaultChrominanceDCBits[0],(Ipp8u*)&DefaultChrominanceDCValues[0]);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    if(m_dec[0]->m_actbl[0].IsEmpty())
    {
        jerr = m_dec[0]->m_actbl[0].Create();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_dec[0]->m_actbl[0].Init(0,1,(Ipp8u*)&DefaultLuminanceACBits[0],(Ipp8u*)&DefaultLuminanceACValues[0]);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    if(m_dec[0]->m_actbl[1].IsEmpty())
    {
        jerr = m_dec[0]->m_actbl[1].Create();
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;

        jerr = m_dec[0]->m_actbl[1].Init(1,1,(Ipp8u*)&DefaultChrominanceACBits[0],(Ipp8u*)&DefaultChrominanceACValues[0]);
        if(JPEG_OK != jerr)
            return UMC_ERR_FAILED;
    }

    return UMC_OK;
}

Status MJPEGVideoDecoderMFX_HW::GetFrameHW(MediaDataEx* in)
{
    JERRCODE jerr;
    Ipp8u    buffersForUpdate = 0; // Bits: 0 - picParams, 1 - quantTable, 2 - huffmanTables, 3 - bistreamData, 4 - scanParams
    bool     foundSOS = false;
    Ipp32u   marker;
    JPEG_DECODE_SCAN_PARAMETER scanParams;

    const size_t  srcSize = in->GetDataSize();
    const MediaDataEx::_MediaDataEx *pAuxData = in->GetExData();

    // we strongly need auxilary data
    if (NULL == pAuxData)
        return UMC_ERR_NULL_PTR;

    if (!m_va)
        return UMC_ERR_NOT_INITIALIZED;

    m_statusReportFeedbackCounter++;

    Status sts = m_va->BeginFrame(m_frameData.GetFrameMID());
    if (sts != UMC_OK)
        return sts;


    /////////////////////////////////////////////////////////////////////////////////////////

    buffersForUpdate = (1 << 5) - 1;
    m_dec[0]->m_num_scans = GetNumScans(in);

    for (Ipp32u i = 0; i < pAuxData->count; i += 1)
    {
        // get chunk size
        size_t chunkSize = (i + 1 < pAuxData->count) ?
                    (pAuxData->offsets[i + 1] - pAuxData->offsets[i]) :
                    (srcSize - pAuxData->offsets[i]);

        marker = pAuxData->values[i] & 0xFF;

        if(!foundSOS && (JM_SOS != marker))
        {
            continue;
        }
        else
        {
            foundSOS = true;
        }

        // some data
        if (JM_SOS == marker)
        {
            Ipp32u nextNotRSTMarkerPos = i+1;

            while((pAuxData->values[nextNotRSTMarkerPos] & 0xFF) >= JM_RST0 &&
                  (pAuxData->values[nextNotRSTMarkerPos] & 0xFF) <= JM_RST7)
            {
                  nextNotRSTMarkerPos++;
            }

            // update chunk size
            chunkSize = (nextNotRSTMarkerPos < pAuxData->count) ?
                        (pAuxData->offsets[nextNotRSTMarkerPos] - pAuxData->offsets[i]) :
                        (srcSize - pAuxData->offsets[i]);

            CMemBuffInput stream;
            stream.Open((Ipp8u*)in->GetDataPointer() + pAuxData->offsets[i] + 2, (Ipp32s)in->GetDataSize() - pAuxData->offsets[i] - 2);

            jerr = m_dec[0]->SetSource(&stream);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;

            jerr = m_dec[0]->ParseSOS(JO_READ_HEADER);
            if(JPEG_OK != jerr)
            {
                if (JPEG_ERR_BUFF == jerr)
                    return UMC_ERR_NOT_ENOUGH_DATA;
                else
                    return UMC_ERR_FAILED;
            }

            buffersForUpdate |= 1 << 4;

            scanParams.NumComponents        = (USHORT)m_dec[0]->m_curr_scan->ncomps;
            scanParams.RestartInterval      = (USHORT)m_dec[0]->m_curr_scan->jpeg_restart_interval;
            scanParams.MCUCount             = (UINT)(m_dec[0]->m_curr_scan->numxMCU * m_dec[0]->m_curr_scan->numyMCU);
            scanParams.ScanHoriPosition     = 0;
            scanParams.ScanVertPosition     = 0;
            scanParams.DataOffset           = (UINT)(pAuxData->offsets[i] + m_dec[0]->GetSOSLen() + 2); // 2 is marker length
            scanParams.DataLength           = (UINT)(chunkSize - m_dec[0]->GetSOSLen() - 2);

            for(int j = 0; j < m_dec[0]->m_curr_scan->ncomps; j += 1)
            {
                scanParams.ComponentSelector[j] = (UCHAR)m_dec[0]->m_ccomp[m_dec[0]->m_curr_comp_no + 1 - m_dec[0]->m_curr_scan->ncomps + j].m_id;
                scanParams.DcHuffTblSelector[j] = (UCHAR)m_dec[0]->m_ccomp[m_dec[0]->m_curr_comp_no + 1 - m_dec[0]->m_curr_scan->ncomps + j].m_dc_selector;
                scanParams.AcHuffTblSelector[j] = (UCHAR)m_dec[0]->m_ccomp[m_dec[0]->m_curr_comp_no + 1 - m_dec[0]->m_curr_scan->ncomps + j].m_ac_selector;
            }

            sts = PackHeaders(in, &scanParams, &buffersForUpdate);
            if (sts != UMC_OK)
                return sts;
        }
        else if (JM_DRI == marker)
        {
            CMemBuffInput stream;
            stream.Open((Ipp8u*)in->GetDataPointer() + pAuxData->offsets[i] + 2, (Ipp32s)in->GetDataSize() - pAuxData->offsets[i] - 2);

            jerr = m_dec[0]->SetSource(&stream);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;

            jerr = m_dec[0]->ParseDRI();
            if(JPEG_OK != jerr)
            {
                if (JPEG_ERR_BUFF == jerr)
                    return UMC_ERR_NOT_ENOUGH_DATA;
                else
                    return UMC_ERR_FAILED;
            }
        }
        else if (JM_DQT == marker)
        {
            CMemBuffInput stream;
            stream.Open((Ipp8u*)in->GetDataPointer() + pAuxData->offsets[i] + 2, (Ipp32s)in->GetDataSize() - pAuxData->offsets[i] - 2);

            jerr = m_dec[0]->SetSource(&stream);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;

            jerr = m_dec[0]->ParseDQT();
            if(JPEG_OK != jerr)
            {
                if (JPEG_ERR_BUFF == jerr)
                    return UMC_ERR_NOT_ENOUGH_DATA;
                else
                    return UMC_ERR_FAILED;
            }

            buffersForUpdate |= 1 << 1;
        }
        else if (JM_DHT == marker)
        {
            CMemBuffInput stream;
            stream.Open((Ipp8u*)in->GetDataPointer() + pAuxData->offsets[i] + 2, (Ipp32s)in->GetDataSize() - pAuxData->offsets[i] - 2);

            jerr = m_dec[0]->SetSource(&stream);
            if(JPEG_OK != jerr)
                return UMC_ERR_FAILED;

            jerr = m_dec[0]->ParseDHT();
            if(JPEG_OK != jerr)
            {
                if (JPEG_ERR_BUFF == jerr)
                    return UMC_ERR_NOT_ENOUGH_DATA;
                else
                    return UMC_ERR_FAILED;
            }

            buffersForUpdate |= 1 << 2;
        }
        else if ((JM_RST0 <= marker) && (JM_RST7 >= marker))
        {
            continue;
        }
        else if (JM_EOI == marker)
        {
            continue;
        }
        else
        {
            return UMC_ERR_NOT_IMPLEMENTED;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    JPEG_DECODE_IMAGE_LAYOUT imageLayout;
    sts = m_va->EndFrame(&imageLayout);
    if (sts != UMC_OK)
        return sts;

    m_convertInfo.colorFormat = GetChromaType();
    m_convertInfo.UOffset = imageLayout.ComponentDataOffset[1];
    m_convertInfo.VOffset = imageLayout.ComponentDataOffset[2];
    return UMC_OK;
}

Status MJPEGVideoDecoderMFX_HW::PackHeaders(MediaData* src, JPEG_DECODE_SCAN_PARAMETER* obtainedScanParams, Ipp8u* buffersForUpdate)
{
    UMCVACompBuffer* compBuf = 0;
    Ipp32u bitstreamTile = 0;
    bool shiftDataOffset = false;

    Status sts = UMC_OK;

    /////////////////////////////////////////////////////////////////////////////////////////
    if((*buffersForUpdate & 1) != 0)
    {
        *buffersForUpdate -= 1;

        JPEG_DECODE_PICTURE_PARAMETERS *picParams = (JPEG_DECODE_PICTURE_PARAMETERS*)m_va->GetCompBuffer(D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA, &compBuf);
        if(!picParams)
            return UMC_ERR_DEVICE_FAILED;

        picParams->FrameWidth                = (USHORT)m_dec[0]->m_jpeg_width;
        picParams->FrameHeight               = (USHORT)m_dec[0]->m_jpeg_height;
        picParams->NumCompInFrame            = (USHORT)m_dec[0]->m_jpeg_ncomp; // TODO: change for multi-scan images
        picParams->ChromaType                = (UCHAR)GetChromaType();
        picParams->TotalScans                = (USHORT)m_dec[0]->m_num_scans;
                
        switch(m_rotation)
        {
        case 0:
            picParams->Rotation = 0;
            break;
        case 90:
            picParams->Rotation = 1;
            break;
        case 180:
            picParams->Rotation = 3;
            break;
        case 270:
            picParams->Rotation = 2;
            break;
        }

        for (Ipp32s i = 0; i < m_dec[0]->m_jpeg_ncomp; i++)
        {
            picParams->ComponentIdentifier[i] = (UCHAR)m_dec[0]->m_ccomp[i].m_id;
            picParams->QuantTableSelector[i]  = (UCHAR)m_dec[0]->m_ccomp[i].m_q_selector;
        }

        picParams->InterleavedData = (picParams->TotalScans == 1) ? 1 : 0;
        picParams->StatusReportFeedbackNumber = m_statusReportFeedbackCounter;
        picParams->RenderTargetFormat = m_fourCC;

        compBuf->SetDataSize(sizeof(JPEG_DECODE_PICTURE_PARAMETERS));

        sts = m_va->Execute();
        if (sts != UMC_OK)
            return sts;
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    if((*buffersForUpdate & (1 << 1)) != 0)
    {
        *buffersForUpdate -= 1 << 1;

        for (Ipp32s i = 0; i < MAX_QUANT_TABLES; i++)
        {
            if (!m_dec[0]->m_qntbl[i].m_initialized)
                continue;

            JPEG_DECODE_QM_TABLE *quantTable = (JPEG_DECODE_QM_TABLE*)m_va->GetCompBuffer(D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA, &compBuf);
            if(!quantTable)
                return UMC_ERR_DEVICE_FAILED;

            quantTable->TableIndex = (UCHAR)m_dec[0]->m_qntbl[i].m_id;
            quantTable->Precision  = (UCHAR)m_dec[0]->m_qntbl[i].m_precision;

            if (m_dec[0]->m_qntbl[i].m_precision) // should be always zero for 8-bit quantization tables
            {
                return UMC_ERR_FAILED;
            }

            ippsCopy_8u(m_dec[0]->m_qntbl[i].m_raw8u, quantTable->Qm, DCTSIZE2);

            /*Ipp16u* invQuantTable = m_dec->m_qntbl[i];
            for (Ipp32s k = 0; k < DCTSIZE2; k++)
            {
                quantTable->Qm[k] = (UCHAR)invQuantTable[k];
            }*/

            compBuf->SetDataSize(sizeof(JPEG_DECODE_QM_TABLE));

            sts = m_va->Execute();
            if (sts != UMC_OK)
                return sts;
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    if((*buffersForUpdate & (1 << 2)) != 0)
    {
        *buffersForUpdate -= 1 << 2;

        if (DefaultInitializationHuffmantables() != UMC_OK)
            return UMC_ERR_FAILED;

        for (Ipp32s i = 0; i < MAX_HUFF_TABLES; i++)
        {
            if(m_dec[0]->m_dctbl[i].IsValid())
            {
                JPEG_DECODE_HUFFMAN_TABLE *huffmanTables = (JPEG_DECODE_HUFFMAN_TABLE*)m_va->GetCompBuffer(D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA, &compBuf);
                if(!huffmanTables)
                    return UMC_ERR_DEVICE_FAILED;

                huffmanTables->TableClass = (UCHAR)m_dec[0]->m_dctbl[i].m_hclass;
                huffmanTables->TableIndex = (UCHAR)m_dec[0]->m_dctbl[i].m_id;

                ippsCopy_8u(m_dec[0]->m_dctbl[i].GetBits(),   huffmanTables->BITS,    sizeof(huffmanTables->BITS));
                ippsCopy_8u(m_dec[0]->m_dctbl[i].GetValues(), huffmanTables->HUFFVAL, sizeof(huffmanTables->HUFFVAL));
                compBuf->SetDataSize(sizeof(JPEG_DECODE_HUFFMAN_TABLE));

                sts = m_va->Execute();
                if (sts != UMC_OK)
                    return sts;
            }

            if(m_dec[0]->m_actbl[i].IsValid())
            {
                JPEG_DECODE_HUFFMAN_TABLE *huffmanTables = (JPEG_DECODE_HUFFMAN_TABLE*)m_va->GetCompBuffer(D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA, &compBuf);
                if(!huffmanTables)
                    return UMC_ERR_DEVICE_FAILED;

                huffmanTables->TableClass = (UCHAR)m_dec[0]->m_actbl[i].m_hclass;
                huffmanTables->TableIndex = (UCHAR)m_dec[0]->m_actbl[i].m_id;

                ippsCopy_8u(m_dec[0]->m_actbl[i].GetBits(),   huffmanTables->BITS,    sizeof(huffmanTables->BITS));
                ippsCopy_8u(m_dec[0]->m_actbl[i].GetValues(), huffmanTables->HUFFVAL, sizeof(huffmanTables->HUFFVAL));
                compBuf->SetDataSize(sizeof(JPEG_DECODE_HUFFMAN_TABLE));

                sts = m_va->Execute();
                if (sts != UMC_OK)
                    return sts;
            }
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    if((*buffersForUpdate & (1 << 3)) != 0)
    {
        *buffersForUpdate -= 1 << 3;

        Ipp8u *bistreamData = (Ipp8u*)m_va->GetCompBuffer(D3DDDIFMT_BITSTREAMDATA, &compBuf);
        if(!bistreamData)
            return UMC_ERR_DEVICE_FAILED;

        if(m_dec[0]->m_num_scans == 1)
        {
            // buffer size is enough
            if (obtainedScanParams->DataLength <= (Ipp32u)compBuf->GetBufferSize())
            {
                memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset, obtainedScanParams->DataLength);
                compBuf->SetDataSize(obtainedScanParams->DataLength);
                shiftDataOffset = true;
            }
            // buffer size is not enough
            else
            {
                memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset, (Ipp32u)compBuf->GetBufferSize());
                compBuf->SetDataSize((Ipp32u)compBuf->GetBufferSize());
                bitstreamTile = obtainedScanParams->DataLength - (Ipp32u)compBuf->GetBufferSize();
                shiftDataOffset = true;
            }
        }
        else
        {
            // buffer size is enough to keep all data (headers + 3 scan data)
            if ((Ipp32u)src->GetDataSize() <= (Ipp32u)compBuf->GetBufferSize())
            {
                memcpy(bistreamData, src->GetDataPointer(), src->GetDataSize());
                compBuf->SetDataSize((Ipp32s)src->GetDataSize());
            }
            // buffer size is enough to keep pixel data for one scan
            else if (obtainedScanParams->DataLength <= (Ipp32u)compBuf->GetBufferSize())
            {
                memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset, obtainedScanParams->DataLength);
                compBuf->SetDataSize(obtainedScanParams->DataLength);
                shiftDataOffset = true;
                
                *buffersForUpdate |= 1 << 3;
            }
            // buffer size is not enough
            else
            {
                memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset, (Ipp32u)compBuf->GetBufferSize());
                compBuf->SetDataSize((Ipp32u)compBuf->GetBufferSize());
                bitstreamTile = obtainedScanParams->DataLength - (Ipp32u)compBuf->GetBufferSize();
                shiftDataOffset = true;
                
                *buffersForUpdate |= 1 << 3;
            }
        }

        sts = m_va->Execute();
        if (sts != UMC_OK)
            return sts;
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    if((*buffersForUpdate & (1 << 4)) != 0)
    {
        *buffersForUpdate -= 1 << 4;

        JPEG_DECODE_SCAN_PARAMETER *scanParams = (JPEG_DECODE_SCAN_PARAMETER*)m_va->GetCompBuffer(D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA, &compBuf);
        if(!scanParams)
            return UMC_ERR_DEVICE_FAILED;

        memcpy(scanParams, obtainedScanParams, sizeof(JPEG_DECODE_SCAN_PARAMETER));
        if(shiftDataOffset)
        {
            scanParams->DataOffset = 0;
        }
        compBuf->SetDataSize(sizeof(JPEG_DECODE_SCAN_PARAMETER));

        sts = m_va->Execute();
        if (sts != UMC_OK)
            return sts;
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    while(bitstreamTile != 0)
    {
        Ipp8u *bistreamData = (Ipp8u*)m_va->GetCompBuffer(D3DDDIFMT_BITSTREAMDATA, &compBuf);
        if(!bistreamData)
            return UMC_ERR_DEVICE_FAILED;

        if (bitstreamTile <= (Ipp32u)compBuf->GetBufferSize())
        {
            memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset + obtainedScanParams->DataLength - bitstreamTile, bitstreamTile);
            compBuf->SetDataSize(bitstreamTile);
            bitstreamTile = 0;
        }
        else
        {
            memcpy(bistreamData, (Ipp8u*)src->GetDataPointer() + obtainedScanParams->DataOffset + obtainedScanParams->DataLength - bitstreamTile, compBuf->GetBufferSize());
            compBuf->SetDataSize(compBuf->GetBufferSize());
            bitstreamTile = bitstreamTile - compBuf->GetBufferSize();
        }

        sts = m_va->Execute();
        if (sts != UMC_OK)
            return sts;
    }

    return UMC_OK;
}

Ipp16u MJPEGVideoDecoderMFX_HW::GetNumScans(MediaDataEx* in)
{
    MediaDataEx::_MediaDataEx *pAuxData = in->GetExData();
    Ipp16u numScans = 0;
    for(Ipp32u i=0; i<pAuxData->count; i++)
    {
        if((pAuxData->values[i] & 0xFF) == JM_SOS)
        {
            numScans++;
        }
    }

    return numScans;
}

Status MJPEGVideoDecoderMFX_HW::GetFrame(UMC::MediaDataEx *pSrcData,
                                         UMC::FrameData** out,
                                         const mfxU32  fieldPos)
{
    Status status = UMC_OK;

    if(0 == out)
    {
        return UMC_ERR_NULL_PTR;
    }

    vm_debug_trace1(VM_DEBUG_NONE, __VM_STRING("MJPEG, frame: %d\n"), m_frameNo);

    // allocate frame
    Status sts = AllocateFrame();
    if (sts != UMC_OK)
    {
        return sts;
    }

    if(m_interleaved)
    {
        // interleaved frame
        status = _DecodeField(pSrcData);
        if (status > 0)
        {
            return UMC_OK;
        }

        if (UMC_OK != status)
        {
            return status;
        }

        (*out)[fieldPos].Init(m_frameData.GetInfo(), m_frameData.GetFrameMID(), m_frameAllocator);
        (*out)[fieldPos].SetTime(pSrcData->GetTime());
        m_frameData.Close();
    }
    else
    {
        // progressive frame
        status = _DecodeField(pSrcData);
        if (status > 0)
        {
            return UMC_OK;
        }

        if (UMC_OK != status)
        {
            return status;
        }

        (*out)->Init(m_frameData.GetInfo(), m_frameData.GetFrameMID(), m_frameAllocator);
        (*out)->SetTime(pSrcData->GetTime());
        m_frameData.Close();
    }

    return status;

} // MJPEGVideoDecoderMFX_HW::GetFrame()

ConvertInfo * MJPEGVideoDecoderMFX_HW::GetConvertInfo()
{
    return &m_convertInfo;
}

} // end namespace UMC

#endif // #ifdef UMC_VA_DXVA

#endif // UMC_ENABLE_MJPEG_VIDEO_DECODER