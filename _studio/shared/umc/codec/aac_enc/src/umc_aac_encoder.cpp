// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_AAC_AUDIO_ENCODER)

#include "umc_audio_encoder.h"
#include "aac_enc_fp.h"
#include "umc_aac_encoder.h"
#include "aaccmn_adts.h"
#include "aaccmn_adif.h"

#ifdef SBR_NEED_LOG
#include "sbr_enc_dbg.h"
#endif

#ifdef  __cplusplus
extern "C" {
#endif

void enc_adts_header(sAdts_fixed_header *pFixedHeader, sAdts_variable_header *pVarHeader, sBitsreamBuffer *pBS);
void enc_adif_header(sAdif_header* pAdifHeader, sBitsreamBuffer* pBS);

#ifdef  __cplusplus
}
#endif

using namespace UMC;


AACEncoder::AACEncoder()
{
    state      = NULL;
    stateMemId = 0;
    m_channel_number = 0;
    m_outputFormat = UMC_AAC_ADTS;
    m_pts_prev = 0.0;
    m_sampling_frequency = 0;
}

AACEncoder::~AACEncoder()
{
    Close();
}

bool AACEncoder::CheckBitRate(Ipp32s br, Ipp32s& /* ind */)
{
    if(br <= 0)
        return false;

    return true;
}

Status AACEncoder::Init(BaseCodecParams* init)
{
    AACEncoderParams* pAACEncoderParams;
    AudioEncoderParams* pAudioCodecInit = DynamicCast<AudioEncoderParams, BaseCodecParams>(init);
    Ipp32s m_ns_mode;
    Ipp32s m_stereo_mode;
    Ipp32s stereo_mode;
    Ipp32s size = 0;

    if(!pAudioCodecInit)
        return UMC_ERR_NULL_PTR;

    m_sampling_frequency = pAudioCodecInit->m_info.sample_frequency;
    m_channel_number     = pAudioCodecInit->m_info.channels;
    m_bitrate            = pAudioCodecInit->m_info.bitrate;
    m_pts_prev           = 0;

    pAACEncoderParams = DynamicCast<AACEncoderParams, BaseCodecParams>(init);
    if(pAACEncoderParams)
    {
        m_audioObjectType = pAACEncoderParams->audioObjectType;
        m_auxAudioObjectType = pAACEncoderParams->auxAudioObjectType;

        if(m_auxAudioObjectType == AOT_SBR) // m_audioObjectType may be LC (default) or LTP, usr should set by himself
            m_sampling_frequency >>= 1;
        else if(m_audioObjectType == AOT_SBR) // m_audioObjectType LC
        {
            m_auxAudioObjectType   = AOT_SBR;
            m_audioObjectType      = AOT_AAC_LC;
            m_sampling_frequency >>= 1;
        }

        m_stereo_mode  = pAACEncoderParams->stereo_mode;
        m_ns_mode      = pAACEncoderParams->ns_mode;
        m_outputFormat = pAACEncoderParams->outputFormat;
    }
    else
    {
        m_audioObjectType = AOT_AAC_LC;
        m_stereo_mode     = UMC_AAC_LR_STEREO;
        m_ns_mode         = 0;
        m_outputFormat    = UMC_AAC_ADTS;
    }

    if((m_outputFormat != UMC_AAC_ADTS) && (m_outputFormat != UMC_AAC_ADIF)
        && (m_outputFormat != UMC_AAC_RAW))
        return UMC_ERR_INIT;

    if((m_audioObjectType != AOT_AAC_LC) && (m_audioObjectType != AOT_AAC_LTP))
        return UMC_ERR_INIT;

    if(m_outputFormat == UMC_AAC_ADTS)
    {
        if((m_channel_number == 7) || (m_channel_number > 8))
            return UMC_ERR_INIT;
    }

    if     (m_audioObjectType == AOT_AAC_MAIN) m_adtsProfile = 0;
    else if(m_audioObjectType == AOT_AAC_LC)   m_adtsProfile = 1;
    else if(m_audioObjectType == AOT_AAC_SSR)  m_adtsProfile = 2;
    else if(m_audioObjectType == AOT_AAC_LTP)  m_adtsProfile = 3;
    else                                       m_adtsProfile = 4;

    m_adtsID = 1;
    if(m_adtsProfile >= 3)
        m_adtsID = 0;

    if(m_channel_number == 1)
        m_stereo_mode = UMC_AAC_MONO;

    switch (m_stereo_mode)
    {
    case UMC_AAC_MONO:
        stereo_mode = AAC_MONO;
        break;
    case UMC_AAC_LR_STEREO:
        stereo_mode = AAC_LR_STEREO;
        break;
    case UMC_AAC_MS_STEREO:
        stereo_mode = AAC_MS_STEREO;
        break;
    case UMC_AAC_JOINT_STEREO:
        stereo_mode = AAC_JOINT_STEREO;
        break;
    default:
        stereo_mode = AAC_LR_STEREO;
        break;
    }

    if(aacencInit(NULL, m_sampling_frequency, m_channel_number, m_bitrate,
        m_audioObjectType, m_auxAudioObjectType, stereo_mode, m_ns_mode, &size) != AAC_OK)
        return UMC_ERR_INIT;

    // checks or create memory allocator;
    if (BaseCodec::Init(init) != UMC_OK)
        return UMC_ERR_ALLOC;

    if (m_pMemoryAllocator->Alloc(&stateMemId, size, UMC_ALLOC_PERSISTENT) != UMC_OK)
        return UMC_ERR_ALLOC;

    state = (AACEnc*)m_pMemoryAllocator->Lock(stateMemId);
    if(!state)
        return UMC_ERR_ALLOC;

    if (aacencInit(state, m_sampling_frequency, m_channel_number, m_bitrate,
        m_audioObjectType, m_auxAudioObjectType, stereo_mode, m_ns_mode, &size) != AAC_OK)
        return UMC_ERR_INIT;

    MemUnlock();

    return UMC_OK;
}

Status AACEncoder::Close()
{
    Status status;

    if (state == NULL)
        return UMC_OK;

    if (m_pMemoryAllocator == NULL)
        return UMC_OK;

    status = MemLock();
    if (status != UMC_OK)
        return status;

    aacencClose(state);
    MemUnlock();
    if (state)
    {
        m_pMemoryAllocator->Free(stateMemId);
        state = NULL;
    }
    BaseCodec::Close();
    return UMC_OK;
}

Status AACEncoder::GetInfo(BaseCodecParams* info)
{
    Ipp32s upsample = (m_auxAudioObjectType == AOT_SBR) ? 2 : 1;
    if (!info)
        return UMC_ERR_NULL_PTR;

    info->m_SuggestedInputSize  = m_channel_number*sizeof(Ipp16s)*1024*upsample;
    info->m_SuggestedOutputSize = ((768*m_channel_number+9/* ADTS_HEADER */)*sizeof(Ipp8u)+3)&(~3);

    AudioEncoderParams* pAudioCodecInfo = DynamicCast<AudioEncoderParams, BaseCodecParams>(info);

    if(!pAudioCodecInfo)
        return UMC_OK;

    pAudioCodecInfo->m_info.bitPerSample     = 16;
    pAudioCodecInfo->m_info.bitrate          = m_bitrate;
    pAudioCodecInfo->m_info.channels         = m_channel_number;
    pAudioCodecInfo->m_info.sample_frequency= m_sampling_frequency;
    pAudioCodecInfo->m_info.stream_type        = AAC_AUDIO;

    return UMC_OK;
}

/****************************************************************************/

Status AACEncoder::GetFrame(MediaData* in, MediaData* out)
{
    Ipp8u*                outPointer = NULL;
    AACStatus             result = AAC_OK;
    Ipp32s                nSamples = 0;
    Ipp32s                nEncodedBytes = 0, headerBytes = 0;
    sBitsreamBuffer       BS;
    sBitsreamBuffer*      pBS = &BS;
    sAdts_fixed_header    adts_fixed_header;
    sAdts_variable_header adts_variable_header;
    Ipp32s                sampling_frequency_index = 0;
    //Ipp32s                rested_bytes;
    Ipp64f                pts_start, pts_end;
    Ipp32s upsample     = (m_auxAudioObjectType == AOT_SBR) ? 2 : 1;
    Status status;

    if (!in || !out)
        return UMC_ERR_NULL_PTR;

    nSamples = (Ipp32s)(in->GetDataSize()/sizeof(Ipp16s));
    if (nSamples < m_channel_number * (1024 * upsample) )
        return UMC_ERR_NOT_ENOUGH_DATA;

    nSamples   = m_channel_number * 1024 * upsample;
    outPointer = (Ipp8u *)out->GetDataPointer();

    status = MemLock();
    if (status != UMC_OK)
        return status;

    if (state == NULL)
        return UMC_ERR_NOT_INITIALIZED;

    result = aacencGetSampleFrequencyIndex(&sampling_frequency_index, state);

    INIT_BITSTREAM(pBS, outPointer)

    if (m_outputFormat == UMC_AAC_ADTS)
    {
        // Put to bistream ADTS header !
        // Fixed header.
        adts_fixed_header.ID                        = m_adtsID;
        adts_fixed_header.Layer                     = 0;
        adts_fixed_header.protection_absent         = 0;
        adts_fixed_header.Profile                   = m_adtsProfile;
        adts_fixed_header.sampling_frequency_index  = sampling_frequency_index;
        adts_fixed_header.private_bit               = 0;
        adts_fixed_header.channel_configuration     = m_channel_number;
        adts_fixed_header.original_copy             = 0;
        adts_fixed_header.Home                      = 0;
        adts_fixed_header.Emphasis                  = 0; 
        if (m_channel_number == 8)
            adts_fixed_header.channel_configuration = 7;

        // Variable header !
        adts_variable_header.copyright_identification_bit   = 0;
        adts_variable_header.copyright_identification_start = 0;
        adts_variable_header.aac_frame_length               = 0;
        adts_variable_header.adts_buffer_fullness           = 0x7FF;
        adts_variable_header.no_raw_data_blocks_in_frame    = 0;

        enc_adts_header(&adts_fixed_header, &adts_variable_header, pBS);
        if (adts_fixed_header.protection_absent == 0)
        {
            PUT_BITS(pBS,0,16); /* for CRC */
        }
    }
    else if (m_outputFormat == UMC_AAC_ADIF)
    {
        Ipp32s frame_number;
        result = aacencGetNumEncodedFrames(&frame_number, state);
        if (frame_number == 0)
        {
            sAdif_header adif_header;
            adif_header.adif_id                     = ADIF_SIGNATURE;
            adif_header.copyright_id_present        = 0;
            adif_header.original_copy               = 0;
            adif_header.home                        = 0;
            adif_header.bitstream_type              = 1;
            adif_header.bitrate                     = m_bitrate;
            adif_header.num_program_config_elements = 0;
            aacencFillProgramConfigElement(adif_header.pce, 0, state);
            enc_adif_header(&adif_header, pBS);
        }
    }

    SAVE_BITSTREAM(pBS)
    Byte_alignment(pBS);

    GET_BITS_COUNT(pBS, headerBytes)
    headerBytes >>= 3;

    result = aacencGetFrame((Ipp16s *)in->GetDataPointer(), &nEncodedBytes, outPointer + headerBytes, state);

    if (m_outputFormat == UMC_AAC_ADTS)
    {
        Ipp32u  *crc_ptr;
        Ipp32s  crc_offset;
        adts_variable_header.aac_frame_length = headerBytes + nEncodedBytes;

        INIT_BITSTREAM(pBS, outPointer)
        crc_ptr = BS.pCurrent_dword;
        crc_offset = BS.nBit_offset;
        enc_adts_header(&adts_fixed_header, &adts_variable_header, pBS);
        if (adts_fixed_header.protection_absent == 0)
        {
            sCrcSaveTable *crcSaveTable;
            Ipp32u crc;
            Ipp32s i;

            bs_CRC_reset(&crc);
            aacencGetCrcSaveTable(&crcSaveTable, state);

            bs_CRC_update(crc_ptr, crc_offset, (headerBytes - 2) * 8, &crc);
            for (i = 0; i < m_channel_number; i++)
            {
                bs_CRC_update(crcSaveTable[i].crc_ptr, crcSaveTable[i].crc_offset, crcSaveTable[i].crc_len, &crc);
                bs_CRC_update_zero(crcSaveTable[i].crc_zero_len, &crc);
            }
            PUT_BITS(pBS,crc,16);
            SAVE_BITSTREAM(pBS)
        }
    }

    in->GetTime(pts_start, pts_end);

    if(pts_start < 0)
        pts_start = m_pts_prev;

    //rested_bytes = in->GetDataSize()- nSamples*sizeof(Ipp16s);
    m_pts_prev = pts_end = pts_start+(Ipp32f)(nSamples/m_channel_number)/(Ipp32f)(m_sampling_frequency);

    out->SetTime(pts_start, pts_end);
    out->SetDataSize(headerBytes + nEncodedBytes);
    in->MoveDataPointer(nSamples*sizeof(Ipp16s));

    //if (rested_bytes) {
    in->SetTime(pts_end, pts_end);
    //}

    MemUnlock();
    return UMC_OK;
}

Status AACEncoder::GetDuration(Ipp32f* p_duration)
{
    Status status;

    if (!p_duration)
        return UMC_ERR_NULL_PTR;

    status = MemLock();
    if (status != UMC_OK)
        return status;

    if (state == NULL)
        return UMC_ERR_NOT_INITIALIZED;

    aacencGetDuration(p_duration, state);
    MemUnlock();
    return UMC_OK;
}

Status AACEncoder::MemLock()
{
    AACEnc *pOldState = state;
    state = (AACEnc*)m_pMemoryAllocator->Lock(stateMemId);
    if(!state)
        return UMC_ERR_ALLOC;

    if (state != pOldState)
        aacencUpdateMemMap(state, (Ipp32s)((Ipp8u *)state-(Ipp8u *)pOldState));

    return UMC_OK;
}

Status AACEncoder::MemUnlock()
{
    if (stateMemId)
    {
        if (m_pMemoryAllocator->Unlock(stateMemId) != UMC_OK)
            return UMC_ERR_ALLOC;
    }
    return UMC_OK;
}

#endif