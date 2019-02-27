// Copyright (c) 2005-2018 Intel Corporation
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
#include "umc_default_memory_allocator.h"
#include "umc_frame_constructor.h"

namespace UMC
{

const double Mpeg2FrameConstructor::FrameRate[9] = {
    0., 24000 / 1001., 24., 25., 30000 / 1001., 30., 50., 60000 / 1001., 60.
};
const ColorFormat Mpeg2FrameConstructor::ColorFormats[4] = {
    NONE, YUV420, YUV422, YUV444
};

const double Mpeg2FrameConstructor::AspectRatioMp1[15] = {
    0.0000, 1.0000, 0.6735, 0.7031, 0.7615,
    0.8055, 0.8437, 0.8437, 0.9375, 0.9815,
    1.0255, 1.0695, 1.1250, 1.1575, 1.2015
};

const int32_t Mpeg2FrameConstructor::AspectRatioMp2[5][2] = {
    {1, 1}, {1, 1}, {4, 3}, {16, 9}, {221, 100}
};

const int32_t Mpeg4FrameConstructor::AspectRatio[6][2] = {
    {0, 0}, {1, 1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}
};

const int16_t H263FrameConstructor::PictureSize[6][2] = {
    {0, 0}, {128, 96}, {176, 144}, {352, 288}, {704, 576}, {1408, 1152}
};

const AudioStreamType AudioFrameConstructor::MpegAStreamType[2][3] = {
    {MP2L1_AUDIO, MP2L2_AUDIO, MP2L3_AUDIO},
    {MP1L1_AUDIO, MP1L2_AUDIO, MP1L3_AUDIO}
};

const int32_t AudioFrameConstructor::MpegAFrequency[3][4] = {
    {22050, 24000, 16000, 0}, /* MPEG 1 */
    {44100, 48000, 32000, 0}, /* MPEG 2 */
    {11025, 12000,  8000, 0}  /* MPEG 2.5 */
};

const int32_t AudioFrameConstructor::MpegABitrate[2][3][15] = {
    { /* MPEG 2 */
        {0, 32, 48, 56,  64,  80,  96, 112, 128, 144, 160, 176, 192, 224, 256}, /* Layer 1 */
        {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}, /* Layer 2 */
        {0,  8, 16, 24,  32,  40,  48,  56,  64,  80,  96, 112, 128, 144, 160}  /* Layer 3 */
    },
    { /* MPEG 1 */
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448}, /* Layer 1 */
        {0, 32, 48, 56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320, 384}, /* Layer 2 */
        {0, 32, 40, 48,  56,  64,  80,  96, 112, 128, 160, 192, 224, 256, 320}  /* Layer 3 */
    }
};

const int32_t AudioFrameConstructor::MpegAChannels[4] = {
    2, 2, 2, 1
};

const float AudioFrameConstructor::MpegAFramesize[2][4] = {
    { 0.0f, 384.0f, 1152.0f,  576.0f }, /* MPEG 2 */
    { 0.0f, 384.0f, 1152.0f, 1152.0f }  /* MPEG 1 */
};

const int32_t AudioFrameConstructor::AACFrequency[16] = {
    96000, 88200, 64000, 48000, 44100, 32000, 24000,
    22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0
};

const int32_t AudioFrameConstructor::AACChannels[16] = {
    0, 1, 2, 3, 4, 5, 6, 8, 0, 0, 0, 0, 0, 0, 0, 0
};

const int32_t AudioFrameConstructor::AC3Frequency[3] = {
    48000, 44100, 32000
};

const int32_t AudioFrameConstructor::AC3FrequencyExt[8] = {
    48000, 44100, 32000, 0, 48000, 48000, 44100, 48000
};

const int32_t AudioFrameConstructor::AC3BitRate[19] = {
     32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 160000,
    192000, 224000, 256000, 320000, 384000, 448000, 512000, 576000, 640000
};

const int32_t AudioFrameConstructor::AC3NumChannels[] = {
    2, 1, 2, 3, 3, 4, 4, 5, 1, 2, 3, 4, 5, 6
};

const int32_t AudioFrameConstructor::DTSChannels[16] = {
    1, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6, 6, 6, 7, 8, 8
};

const int32_t AudioFrameConstructor::DTSFrequency[16] = {
    0, 8000, 16000, 32000, 0, 0, 11025, 22050, 44100, 0, 0, 12000, 24000, 48000, 0, 0
};

const int32_t AudioFrameConstructor::DTSBitrate[32] = {
      32000,   56000,   64000,   96000,  112000,  128000,  192000,  224000,
     256000,  320000,  384000,  448000,  512000,  576000,  640000,  768000,
     960000, 1024000, 1152000, 1280000, 1344000, 1408000, 1411200, 1472000,
    1536000, 1920000, 2048000, 3072000, 3840000,       0,       0,       0
};

const int32_t AudioFrameConstructor::LPCMChannels[16] = {
    1, 2, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0
};

Status Mpeg2FrameConstructor::ParseSequenceHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo)
{
    VideoStreamInfo vInfo;

    int32_t uiHeaderSizeEstimation = 17;
    if (iLen >= 0 && uiHeaderSizeEstimation > iLen)
        return UMC_ERR_NOT_ENOUGH_DATA;

    BitstreamReader bs;
    bs.Init(buf, iLen);
    bs.SkipBits(32); // skip sequence header

    vInfo.clip_info.width = (uint16_t)bs.GetBits(12);
    vInfo.clip_info.height = (uint16_t)bs.GetBits(12);

    // aspect_ratio_information will be checked separately for MPEG1 and MPEG2
    // because this field has different sematics
    uint32_t aspect_ratio_information = bs.GetBits(4) & 0x0f;
    uint8_t frame_rate_code = (uint8_t)bs.GetBits(4);
    if (0 == frame_rate_code || frame_rate_code > 8)
        return UMC_ERR_INVALID_STREAM;
    vInfo.framerate = FrameRate[frame_rate_code];

    vInfo.bitrate = 400 * bs.GetBits(18);

    bs.GetBits(1); // marker_bit
    bs.SkipBits(10); // vbv_buffer_size_value
    bs.SkipBits(1); // constrained_parameters_flag

    if (bs.GetBits(1)) //load_intra_quantiser_matrix
    {
        uiHeaderSizeEstimation += 64;
        if (iLen >= 0 && uiHeaderSizeEstimation > iLen)
            return UMC_ERR_NOT_ENOUGH_DATA;
        bs.SkipBits(64 * 8);
    }

    if (bs.GetBits(1)) // load_non_intra_quantiser_matrix
    {
        uiHeaderSizeEstimation += 64;
        if (iLen >= 0 && uiHeaderSizeEstimation > iLen)
            return UMC_ERR_NOT_ENOUGH_DATA;
        bs.SkipBits(64 * 8);
    }

    TrackType trackType;
    uint32_t code = bs.GetBits(32);
    if (0x1B5 == code && 0x01 == bs.GetBits(4))
    { // presence sequence_extension indicates MPEG2
        trackType = TRACK_MPEG2V;
        vInfo.stream_type = MPEG2_VIDEO;
        // check aspect_ratio_information for MPEG2.
        if (0 == aspect_ratio_information || aspect_ratio_information > 4)
            aspect_ratio_information = 1;//return UMC_ERR_FAILED;

        if (1 == aspect_ratio_information)
        { // pixel aspect ratio (square pixels)
            vInfo.aspect_ratio_width = vInfo.clip_info.width;
            vInfo.aspect_ratio_height = vInfo.clip_info.height;
        }
        else
        { // display aspect ratio
            vInfo.aspect_ratio_width = AspectRatioMp2[aspect_ratio_information][0];
            vInfo.aspect_ratio_height = AspectRatioMp2[aspect_ratio_information][1];
        }

        if(bs.PeekBits(1)) // check escape bit
        {
            vInfo.profile = bs.GetBits(8); // use profile for profile_and_level value
            vInfo.level   = 0;
        }
        else
        {
            vInfo.profile = bs.GetBits(4);
            vInfo.level   = bs.GetBits(4);
        }

        if (!bs.GetBits(1)) //progressive_sequence
        {
            // top-first is a temporary it will be defined exactly from first picture header
            vInfo.interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
        }

        vInfo.color_format = ColorFormats[bs.GetBits(2)];
        if(vInfo.color_format == NONE)
            return UMC_ERR_INVALID_STREAM;

        vInfo.clip_info.width  |= (bs.GetBits(2) << 12); // horizontal_size_extension
        vInfo.clip_info.height |= (bs.GetBits(2) << 12); // vertical_size_extension
        vInfo.bitrate |= (bs.GetBits(12) << 18); // bitrate_extension

        bs.GetBits(1);  // marker_bit
        bs.GetBits(12); // vbv_buffer_size_extension
        bs.GetBits(1);  // low_delay

        if(frame_rate_code > 8)
            vInfo.framerate = frame_rate_code*(double)(bs.GetBits(2) + 1)/(double)(bs.GetBits(5) + 1); // (frame_rate_extension_n+1)/(frame_rate_extension_d+1)
    }
    else
    { // absence of sequence_extension indicates MPEG1
        vInfo.profile = 0;
        vInfo.level = 0;
        trackType = TRACK_MPEG1V;
        vInfo.stream_type = MPEG1_VIDEO;
        // check aspect_ratio_information for MPEG1
        if (0 == aspect_ratio_information || aspect_ratio_information == 15)
            return UMC_ERR_FAILED;

        // MPEG1 specifies pixel aspect ratio
        vInfo.aspect_ratio_width = vInfo.clip_info.width;
        vInfo.aspect_ratio_height = (int32_t)(vInfo.clip_info.height * AspectRatioMp1[aspect_ratio_information]);
    }

    if (0 == vInfo.clip_info.width)
        return UMC_ERR_INVALID_STREAM;
    if (0 == vInfo.clip_info.height)
        return UMC_ERR_INVALID_STREAM;

    if (pInfo)
    { // if pointer to info passed, fill it
        pInfo->m_Type = trackType;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->profile = vInfo.profile;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->level = vInfo.level;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_height = vInfo.aspect_ratio_height;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_width = vInfo.aspect_ratio_width;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->bitrate = vInfo.bitrate;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->clip_info = vInfo.clip_info;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->framerate = vInfo.framerate;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->stream_type = vInfo.stream_type;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->interlace_type = vInfo.interlace_type;
    }

    return UMC_OK;
}

Status Mpeg2FrameConstructor::ParsePictureHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo)
{
    InterlaceType interlace_type = PROGRESSIVE;

    // Find first picture coding extension
    int32_t offset = 0;
    while ((iLen < 0 || offset < iLen - 8) && !(IS_CODE(&buf[offset], 0xb5) && (buf[offset + 4] & 0xf0) == 0x80))
        offset++;
    if (iLen >= 0 && offset >= iLen - 8)
        return UMC_ERR_NOT_ENOUGH_DATA;

    BitstreamReader bs;
    bs.Init(&buf[offset], 0xffffffff);

    // start_code, start_code_identifier, f_codes, intra_dc_precision
    bs.SkipBits(32 + 4 + 4 + 4 + 4 + 4 + 2);
    uint32_t picture_structure = bs.GetBits(2);
    uint32_t top_field_first = bs.GetBits(1);
    // frame_pred_frame_dct, concealment_motion_vectors, q_scale_type, intra_vlc_format
    // alternate_scan, repeat_first_field, chroma_420_type
    bs.SkipBits(1 + 1 + 1 + 1 + 1 + 1 + 1);

    uint32_t progressive_frame = bs.GetBits(1);
    if (1 == progressive_frame)
        interlace_type = PROGRESSIVE;
    else
    {
        if (1 == picture_structure) // Field
            interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
        else if (2 == picture_structure) // Bottom Field
            interlace_type = INTERLEAVED_BOTTOM_FIELD_FIRST;
        else if (3 == picture_structure) // Frame-picture
            interlace_type = (top_field_first) ? INTERLEAVED_TOP_FIELD_FIRST : INTERLEAVED_BOTTOM_FIELD_FIRST;
    }

    if (pInfo)
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->interlace_type = interlace_type;
    return UMC_OK;
}

Status Mpeg4FrameConstructor::ParseVideoObjectLayer(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo)
{
    VideoStreamInfo vInfo;

    int32_t pos = 0;
    while ((iLen < 0 || pos < iLen - 3) && !IS_CODE_INT(&buf[pos], 0x20, 0x2f))
        pos++;
    if (iLen > 0 && pos >= iLen - 3)
        return UMC_ERR_NOT_ENOUGH_DATA;

    // 28 bytes is enough for header
    int32_t iEstimatedHeaderSize = 28;
    if (iLen > 0 && pos + iEstimatedHeaderSize >= iLen)
        return UMC_ERR_NOT_ENOUGH_DATA;

    BitstreamReader bs;
    bs.Init(&buf[pos + 4], 0xffffffff);
    bs.GetBits(1); // random_accessible_vol
    uint32_t video_object_type_indication = bs.GetBits(8);
    if (0x12 == video_object_type_indication) // "Fine Granularity Scalable"
        return UMC_ERR_INVALID_STREAM;

    uint32_t video_object_layer_verid;
    uint32_t is_object_layer_identifier = bs.GetBits(1);
    if (is_object_layer_identifier)
    {
        video_object_layer_verid = bs.GetBits(4);
        bs.GetBits(3); // video_object_layer_priority
    }
    else
        video_object_layer_verid = 1;

    if (0x01 > video_object_layer_verid || 0x05 < video_object_layer_verid)
        return UMC_ERR_INVALID_STREAM;

    uint32_t aspect_ratio_info = (uint8_t)bs.GetBits(4);
    if (0x0F == aspect_ratio_info)
    { // "extended_PAR"
        vInfo.aspect_ratio_width = bs.GetBits(8);
        vInfo.aspect_ratio_height = bs.GetBits(8);
        if (0x00 == vInfo.aspect_ratio_width || 0x00 == vInfo.aspect_ratio_height)
            return UMC_ERR_INVALID_STREAM;
    }
    else
    {
        // treat forbidden or reserved values as a default value
        aspect_ratio_info = MFX_MAX(1, MFX_MIN(aspect_ratio_info, 5));
        vInfo.aspect_ratio_width = AspectRatio[aspect_ratio_info][0];
        vInfo.aspect_ratio_height = AspectRatio[aspect_ratio_info][1];
    }
//    else // forbidden or reserved values
//        return UMC_ERR_INVALID_STREAM;

    if (bs.GetBits(1)) // vol_control_parameters
    {
        bs.GetBits(3); // skip chroma_format, low_delay
        if (bs.GetBits(1)) // vbv_parameters
            bs.SkipBits(79); // skip vbv_parameters
    }

    uint32_t video_object_layer_shape = bs.GetBits(2);
    if (0x03 == video_object_layer_verid && 0x01 != video_object_layer_shape)
        bs.GetBits(4); // skip video_object_layer_shape_extension

    if (1 != bs.GetBits(1)) // marker_bit
        return UMC_ERR_INVALID_STREAM;

    uint32_t vop_time_increment_resolution = bs.GetBits(16);
    if (0x00 == vop_time_increment_resolution)
        return UMC_ERR_INVALID_STREAM;

    if (1 != bs.GetBits(1)) // marker_bit
        return UMC_ERR_INVALID_STREAM;

    uint8_t uiBitCount = 0;
    while (vop_time_increment_resolution >> uiBitCount)
        uiBitCount++;

    vInfo.framerate = 0.0;
    if (bs.GetBits(1)) // fixed_vop_rate
    {
        uint32_t fixed_vop_time_increment = bs.GetBits(uiBitCount);
        if (0x00 == fixed_vop_time_increment)
            return UMC_ERR_INVALID_STREAM;
        vInfo.framerate = vop_time_increment_resolution / fixed_vop_time_increment;
    }

    if (0x00 != video_object_layer_shape) // "rectangular"
        return UMC_ERR_INVALID_STREAM;

    if (1 != bs.GetBits(1)) // marker_bit
        return UMC_ERR_INVALID_STREAM;

    vInfo.clip_info.width = bs.GetBits(13);
    if (0 == vInfo.clip_info.width)
        return UMC_ERR_INVALID_STREAM;

    if (1 != bs.GetBits(1)) // marker_bit
        return UMC_ERR_INVALID_STREAM;

    vInfo.clip_info.height = bs.GetBits(13);
    if (0 == vInfo.clip_info.height)
        return UMC_ERR_INVALID_STREAM;

    if (1 != bs.GetBits(1)) // marker_bit
        return UMC_ERR_INVALID_STREAM;

    if (bs.GetBits(1)) // interlaced
        vInfo.interlace_type = INTERLEAVED_TOP_FIELD_FIRST;
    else
        vInfo.interlace_type = PROGRESSIVE;

    vInfo.stream_type = MPEG4_VIDEO;
    if (pInfo)
    { // if pointer to info passed, fill it
        pInfo->m_Type = TRACK_MPEG4V;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_width = vInfo.aspect_ratio_width;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_height = vInfo.aspect_ratio_height;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->framerate = vInfo.framerate;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->clip_info = vInfo.clip_info;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->interlace_type = vInfo.interlace_type;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->stream_type = vInfo.stream_type;
    }
    return UMC_OK;
}

Status H261FrameConstructor::ParseHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo)
{
    VideoStreamInfo vInfo;
    BitstreamReader bs;
    bs.Init(buf, iLen);

    // 4 bytes are enough for header
    if (iLen >= 0 && iLen < 4)
        return UMC_ERR_NOT_ENOUGH_DATA;

    bs.SkipBits(20); // skip short_video_start_marker
    bs.SkipBits(5); // skip temporal_reference
    bs.SkipBits(3); // skip flags

    if (bs.GetBits(1))
    { // CIF
        vInfo.clip_info.width = 352;
        vInfo.clip_info.height = 288;
    }
    else
    { // QCIF
        vInfo.clip_info.width = 176;
        vInfo.clip_info.height = 144;
    }

    vInfo.aspect_ratio_width = 12;
    vInfo.aspect_ratio_height = 11;
    vInfo.framerate = 30000 / 1001.;
    vInfo.stream_type = H261_VIDEO;
    if (pInfo)
    { // if pointer to info passed, fill it
        pInfo->m_Type = TRACK_H261;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_width = vInfo.aspect_ratio_width;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_height = vInfo.aspect_ratio_height;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->clip_info = vInfo.clip_info;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->framerate = vInfo.framerate;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->stream_type = vInfo.stream_type;
    }
    return UMC_OK;
}

Status H263FrameConstructor::ParseHeader(uint8_t *buf, int32_t iLen, Mpeg2TrackInfo *pInfo)
{
    VideoStreamInfo vInfo;
    BitstreamReader bs;
    bs.Init(buf, iLen);
    uint32_t pixel_aspect_ratio_code = 2;
    uint8_t source_format;

    // 14 bytes are enough for header
    if (iLen >= 0 && iLen < 14)
        return UMC_ERR_NOT_ENOUGH_DATA;

    bs.SkipBits(22); // skip short_video_start_marker
    bs.SkipBits(8); // skip temporal_reference

    uint32_t marker_bit = bs.GetBits(1);
    if (0 == marker_bit)
        return UMC_ERR_INVALID_STREAM;

    uint32_t zero_bit = bs.GetBits(1);
    if (1 == zero_bit) // zero_bit
        return UMC_ERR_INVALID_STREAM;

    bs.SkipBits(3); // skip bits
    source_format = (uint8_t)bs.GetBits(3);
    if (0 == source_format || 6 == source_format)
        return UMC_ERR_INVALID_STREAM;

    vInfo.framerate = 30000 / 1001.;
    vInfo.aspect_ratio_width = Mpeg4FrameConstructor::AspectRatio[2][0];
    vInfo.aspect_ratio_height = Mpeg4FrameConstructor::AspectRatio[2][1];

    // PLUSPTYPE
    if (7 == source_format)
    {
        uint8_t ufep = (uint8_t)bs.GetBits(3);
        uint8_t custom_PCF = 0;
        if (0x01 == ufep)
        { // OPPTYPE
            source_format = (uint8_t)bs.GetBits(3);
            if (0 == source_format || 7 == source_format)
                return UMC_ERR_FAILED;

            custom_PCF = (uint8_t)bs.GetBits(1);
            bs.SkipBits(10);
            if (0x08 != bs.GetBits(4))
                return UMC_ERR_FAILED;
        }

        // MPPTYPE
        if (bs.GetBits(3) > 5) // picture_coding_type
            return UMC_ERR_FAILED;

        bs.SkipBits(3); // flags (RPR, RRU, RTYPE)
        if (0x01 != bs.GetBits(3)) // should be '001'
            return UMC_ERR_FAILED;

        // If PLUSPTYPE is present, then CPM follows immediately after PLUSPTYPE in the picture header.
        if (bs.GetBits(1)) // CPM
            bs.SkipBits(2); // PSBI

        if (0x01 == ufep)
        {
            if (6 == source_format)
            { // custom picture format (CPFMT)
                pixel_aspect_ratio_code = bs.GetBits(4) & 0x0f;
                if (0 == pixel_aspect_ratio_code || (6 <= pixel_aspect_ratio_code && pixel_aspect_ratio_code <= 14))
                    return UMC_ERR_FAILED;

                vInfo.clip_info.width = (bs.GetBits(9) + 1) * 4;
                if (1 != bs.GetBits(1))
                    return UMC_ERR_FAILED;

                vInfo.clip_info.height = bs.GetBits(9) * 4;
                if (1 * 4 == vInfo.clip_info.height || vInfo.clip_info.height > 288 * 4)
                    return UMC_ERR_FAILED;

                if (0x0F == pixel_aspect_ratio_code)
                { // Extended Pixel Aspect Ratio (EPAR)
                    vInfo.aspect_ratio_width = bs.GetBits(8);
                    vInfo.aspect_ratio_height = bs.GetBits(8);
                    if (0 == vInfo.aspect_ratio_width || 0 == vInfo.aspect_ratio_height)
                        return UMC_ERR_FAILED;
                }
                else
                {
                    vInfo.aspect_ratio_width = Mpeg4FrameConstructor::AspectRatio[pixel_aspect_ratio_code][0];
                    vInfo.aspect_ratio_height = Mpeg4FrameConstructor::AspectRatio[pixel_aspect_ratio_code][1];
                }
            }

            if (1 == custom_PCF)
            { // Custom Picture Clock Frequency Code (CPCFC)
                uint32_t clock_conversion_factor = 1000 + (uint8_t)bs.GetBits(1);
                uint32_t clock_divisor = (uint8_t)bs.GetBits(7);
                if (0 == clock_divisor)
                    return UMC_ERR_FAILED;

                vInfo.framerate = 1800000. / (clock_divisor * clock_conversion_factor);
            }
        }
    }

    if (1 <= source_format && source_format <= 5)
    {
        vInfo.clip_info.width = PictureSize[source_format][0];
        vInfo.clip_info.height = PictureSize[source_format][1];
    }

    vInfo.stream_type = H263_VIDEO;
    if (pInfo)
    { // if pointer to info passed, fill it
        pInfo->m_Type = TRACK_H263;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_width = vInfo.aspect_ratio_width;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->aspect_ratio_height = vInfo.aspect_ratio_height;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->clip_info = vInfo.clip_info;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->framerate = vInfo.framerate;
        ((VideoStreamInfo *)pInfo->m_pStreamInfo)->stream_type = vInfo.stream_type;
    }
    return UMC_OK;
}

FrameConstructorParams::FrameConstructorParams()
{
    m_pInfo = NULL;
    m_lBufferSize = 0;
    m_nOfFrames = 0;
    m_bPureStream = false;
    m_bStopAtFrame = false;
    m_pMemoryAllocator = NULL;
}

FrameConstructor::FrameConstructor()
{
    // reset variables
    m_midAllocatedBuffer = MID_INVALID;
    m_pMemoryAllocator = NULL;
    m_pAllocated = NULL;
    m_pBuf = NULL;
    m_pInfo = NULL;

    m_bEndOfStream = false;
    m_bIsOutputBufferLocked = false;
    m_bInfoFilled = false;

    m_lBufferSize = 0;
    m_lLastBytePos = 0;
    m_lFirstBytePos = 0;
    m_lCurPos = 0;
    m_lCurPicStart = -1;
    m_iCurrentLevel = 0;
    m_uiCommitedFrames = 0;
    m_uiTotalFrames = 0;
    m_iFirstInDecOrderIdx = 0;
    m_dRate = 1.0;
    m_bStopAtFrame = false;
    m_bPureStream = false;

    m_LastFrame.SetBufferPointer(NULL, 0);
    m_LastFrame.SetAbsPos(0);
    m_LastFrame.SetTime(-1.0, -1.0);
}

FrameConstructor::~FrameConstructor()
{
    Close();
}

Status FrameConstructor::Stop()
{
    return UMC_OK;
}

Status FrameConstructor::Init(MediaReceiverParams *pInit)
{
    FrameConstructorParams *pParams = DynamicCast<FrameConstructorParams> (pInit);
    if (NULL == pParams)
        return UMC_ERR_NULL_PTR;

    if (0 == pParams->m_lBufferSize)
        return UMC_ERR_INIT;

    Close();

    m_bStopAtFrame = pParams->m_bStopAtFrame;
    m_bPureStream = pParams->m_bPureStream;

    // allocate buffer (one more)
    m_lBufferSize = (int32_t)pParams->m_lBufferSize;

    if (pParams->m_pMemoryAllocator)
    { // use the external memory allocator
        m_pMemoryAllocator = pParams->m_pMemoryAllocator;
    }
    else
    { // allocate default memory allocator
        m_pAllocated = new DefaultMemoryAllocator();
        if (NULL == m_pAllocated)
            return UMC_ERR_ALLOC;

        m_pMemoryAllocator = m_pAllocated;
    }

    Status umcRes = m_pMemoryAllocator->Alloc(&m_midAllocatedBuffer, m_lBufferSize + 4, UMC_ALLOC_PERSISTENT, 16);
    if (UMC_OK != umcRes)
        return UMC_ERR_ALLOC;

    m_pBuf = (uint8_t *)m_pMemoryAllocator->Lock(m_midAllocatedBuffer);
    if (NULL == m_pBuf)
        return UMC_ERR_ALLOC;

    m_iCurrentLevel = 0;

    m_pInfo = new Mpeg2TrackInfo;
    if (NULL == m_pInfo)
        return UMC_ERR_ALLOC;

    if (pParams->m_pInfo)
    {
        umcRes = m_pInfo->CopyFrom(pParams->m_pInfo);
    }
    else
    {
        if (DynamicCast<VideoFrameConstructor>(this))
            m_pInfo->m_Type = TRACK_ANY_VIDEO;
        else if (DynamicCast<AudioFrameConstructor>(this))
            m_pInfo->m_Type = TRACK_ANY_AUDIO;
        umcRes = m_pInfo->Alloc();
    }
    if (UMC_OK != umcRes)
        return umcRes;

    m_dRate = 1.0;
    return UMC_OK;
}

Status FrameConstructor::Close()
{
    Reset();

    // delete buffer
    if (m_pBuf && m_pMemoryAllocator)
    {
        m_pMemoryAllocator->Unlock(m_midAllocatedBuffer);
        m_pMemoryAllocator->Free(m_midAllocatedBuffer);
        m_pBuf = NULL;
    }

    // delete allocator if was allocated internally
    if (m_pAllocated)
    {
        delete m_pAllocated;
        m_pAllocated = NULL;
    }

    if (m_pInfo)
    {
        m_pInfo->ReleaseAll();
        delete m_pInfo;
        m_pInfo = NULL;
    }

    m_midAllocatedBuffer = MID_INVALID;
    m_pMemoryAllocator = NULL;
    m_lBufferSize = 0;
    m_iCurrentLevel = 0;
    m_uiCommitedFrames = 0;
    m_uiTotalFrames = 0;

    m_bInfoFilled = false;
    m_bIsOutputBufferLocked = false;
    return UMC_OK;
}

Status FrameConstructor::Reset()
{
    std::lock_guard<std::mutex> guard(m_synchro);

    while (UMC_OK == m_OutputQueue.Remove());

    m_bEndOfStream = false;
    m_bIsOutputBufferLocked = false;

    m_lLastBytePos = 0;
    m_lFirstBytePos = 0;
    m_lCurPos = 0;
    m_lCurPicStart = -1;
    m_iCurrentLevel = 0;
    m_uiCommitedFrames = 0;
    m_uiTotalFrames = 0;

    m_LastSample.Reset();
    m_PrevSample.Reset();
    m_CurFrame.Reset();
    m_CurFrame.iBufOffset = -1;
    m_LastFrame.SetBufferPointer(NULL, 0);
    m_LastFrame.SetAbsPos(0);
    m_LastFrame.SetTime(-1.0, -1.0);

    return UMC_OK;
}

Status FrameConstructor::SoftReset()
{
    std::lock_guard<std::mutex> guard(m_synchro);

    m_bEndOfStream = false;

    if (m_uiTotalFrames > 0)
    {
        const FCSample* lastSample = m_OutputQueue.LastBO();
        m_lCurPos = m_lLastBytePos = lastSample->iBufOffset + lastSample->uiSize;
        m_lCurPicStart = -1;
        m_CurFrame.iBufOffset = -1;
        m_CurFrame.dPTS = -1.0;
        m_CurFrame.dDTS = -1.0;
    }
    else
    {
        m_lCurPos = m_lLastBytePos = m_CurFrame.iBufOffset = m_lFirstBytePos = 0;
        m_lCurPicStart = -1;
        m_CurFrame.Reset();
        m_CurFrame.iBufOffset = -1;
    }

    m_PrevSample.uiSize = 0;
    m_LastSample.uiSize = 0;
    return UMC_OK;
}

void FrameConstructor::SetRate(double dRate)
{
    m_dRate = dRate;
}

Status FrameConstructor::LockInputBuffer(MediaData *in)
{
    std::lock_guard<std::mutex> guard(m_synchro);
    int32_t lChunkSize;

    // check error(s)
    if (NULL == in)
        return UMC_ERR_NULL_PTR;

    lChunkSize = (int32_t)in->GetDataSize();

    in->SetBufferPointer(m_pBuf + m_lLastBytePos, (size_t)((m_lFirstBytePos > m_lLastBytePos) ?
        MFX_MAX(0, m_lFirstBytePos - m_lLastBytePos - 4) : m_lBufferSize - m_lLastBytePos));

    if ((m_lFirstBytePos > m_lLastBytePos) && (m_lLastBytePos + lChunkSize + 4 >= m_lFirstBytePos))
        return UMC_ERR_NOT_ENOUGH_BUFFER; // at least 4 bytes gap between last and first bytes

    if ((m_lFirstBytePos <= m_lLastBytePos) && (m_lLastBytePos + lChunkSize > m_lBufferSize))
    {
        int32_t curFramePos = m_CurFrame.iBufOffset >= 0 ? m_CurFrame.iBufOffset : m_lLastBytePos;
        if (m_lLastBytePos + lChunkSize - curFramePos >= m_lFirstBytePos)
            return UMC_ERR_NOT_ENOUGH_BUFFER;

        MFX_INTERNAL_CPY(m_pBuf, m_pBuf + curFramePos, m_lLastBytePos - curFramePos);
        m_lLastBytePos -= curFramePos;
        m_lCurPos -= curFramePos;
        m_lCurPicStart -= MFX_MIN(m_lCurPicStart, curFramePos);
        m_PrevSample.iBufOffset -= MFX_MIN(m_PrevSample.iBufOffset, curFramePos);
        m_LastSample.iBufOffset -= MFX_MIN(m_LastSample.iBufOffset, curFramePos);
        m_CurFrame.iBufOffset = m_CurFrame.iBufOffset >= 0 ? 0 : m_CurFrame.iBufOffset;
        in->SetBufferPointer(m_pBuf + m_lLastBytePos,
            (size_t)(MFX_MAX(0, m_lFirstBytePos - m_lLastBytePos - 4)));
    }

    return UMC_OK;
}

Status FrameConstructor::UnLockInputBuffer(MediaData *in, Status streamStatus)
{
    Status umcRes = PreUnLockInputBuffer(in, streamStatus);
    std::lock_guard<std::mutex> guard(m_synchro);

    // automatically commit all uncommited samples
    if (UMC_OK == umcRes)
        m_uiCommitedFrames = m_uiTotalFrames;
    return umcRes;
}

Status FrameConstructor::PreUnLockInputBuffer(MediaData *in, Status streamStatus)
{
    std::lock_guard<std::mutex> guard(m_synchro);
    SplMediaData frame;

    if (NULL != in && UMC_OK == streamStatus)
    {
        if (DynamicCast<SplMediaData>(in) && ((SplMediaData *)in)->GetAbsPos() == m_LastSample.uiAbsPos)
        { // add on at the end of last input
            m_LastSample.uiSize += (uint32_t)in->GetDataSize();
        }
        else
        { // scroll input samples
            m_PrevSample = m_LastSample;
            m_LastSample.CopyFrom(in[0], m_lLastBytePos);
        }

        m_lLastBytePos += (int32_t)in->GetDataSize();
    }

    if (UMC_OK != streamStatus)
    {
        m_uiCommitedFrames = m_uiTotalFrames;
        m_bEndOfStream = true;
    }

    Status umcRes = UMC_OK;
    Status getFrameRes = UMC_ERR_INIT; // this value to indicate first iteration
    do
    {
        umcRes = GetFrame(&frame);
        if (UMC_ERR_INIT == getFrameRes)
            getFrameRes = umcRes;

        if (UMC_OK == umcRes)
        {
            // to avoid assigning of the same PTS to several frames
            if (ArePTSEqual(frame.GetTime(), m_LastFrame.GetTime()))
                frame.SetTime(-1.0, -1.0);

            FCSample sample;
            sample.CopyFrom(frame, (int32_t)((uint8_t *)frame.GetDataPointer() - m_pBuf));
            m_iCurrentLevel += (int32_t)frame.GetDataSize();

            if (sample.uiAbsPos < m_LastFrame.GetAbsPos())
                m_uiCommitedFrames = m_uiTotalFrames;
            m_OutputQueue.Add(sample, m_uiCommitedFrames);

            m_LastFrame = frame;
            m_uiTotalFrames++;
        }
        else if (UMC_ERR_SYNC == umcRes && 0 == m_uiTotalFrames)
        { // syncword was not found, so skip parsed bytes
            m_lFirstBytePos = m_lCurPos;
        }
    } while (UMC_OK == umcRes && !m_bStopAtFrame);

    return getFrameRes;
}

Status FrameConstructor::GetSampleFromQueue(FCSample *pSample)
{
    Status umcRes = m_OutputQueue.First(pSample[0]);
    return UMC_OK == umcRes ? UMC_OK : m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;
}

Status FrameConstructor::LockOutputBuffer(MediaData *out)
{
    std::lock_guard<std::mutex> guard(m_synchro);

    // check error(s)
    if (NULL == out)
        return UMC_ERR_NULL_PTR;

    // check if there is at least one sample
    if (0 == m_uiCommitedFrames)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    FCSample sample;
    Status umcRes = GetSampleFromQueue(&sample);
    if (UMC_OK != umcRes)
        return umcRes;

    // fill MediaData
    sample.CopyTo(out[0], m_pBuf);
    m_bIsOutputBufferLocked = true;
    return UMC_OK;
}

Status FrameConstructor::UnLockOutputBuffer(MediaData *out)
{
    std::lock_guard<std::mutex> guard(m_synchro);

    // check error(s)
    if (NULL == out)
        return UMC_ERR_NULL_PTR;

    FCSample sample;
    Status umcRes = GetSampleFromQueue(&sample);
    if (UMC_OK != umcRes || out->GetBufferPointer() != m_pBuf + sample.iBufOffset)
        return UMC_OK;

    m_OutputQueue.Remove(m_iFirstInDecOrderIdx);
    m_uiCommitedFrames--;
    m_uiTotalFrames--;
    if (m_dRate < 0.0)
        m_iFirstInDecOrderIdx--;

    // update bottom boundary
    if (m_OutputQueue.Size() > 0)
        m_lFirstBytePos = m_OutputQueue.FirstBO()->iBufOffset;
    else if (m_CurFrame.iBufOffset >= 0)
        m_lFirstBytePos = m_CurFrame.iBufOffset;
    else
        m_lFirstBytePos = m_lCurPos;

    m_iCurrentLevel = CalcCurrentLevel(m_lFirstBytePos, m_lLastBytePos, m_lBufferSize);

    m_bIsOutputBufferLocked = false;
    return UMC_OK;
}

Status FrameConstructor::GetLastFrame(MediaData *data)
{
    if (NULL == data)
        return UMC_ERR_NULL_PTR;

    std::lock_guard<std::mutex> guard(m_synchro);

    if (NULL == m_LastFrame.GetBufferPointer())
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (DynamicCast<SplMediaData>(data))
        *((SplMediaData *)data) = m_LastFrame;
    else
        *data = *((MediaData *)&m_LastFrame);

    return UMC_OK;
}

Status FrameConstructor::GetFrame(SplMediaData *frame)
{
    if ((m_lLastBytePos - m_lCurPos) == 0)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    m_CurFrame = m_LastSample;
    m_CurFrame.uiSize = m_lLastBytePos - m_lCurPos;
    m_CurFrame.iBufOffset = m_lCurPos;
    m_CurFrame.CopyTo(frame[0], m_pBuf);
    m_lCurPos = m_lLastBytePos;
    return UMC_OK;
}

Mpeg2TrackInfo *FrameConstructor::GetInfo(void)
{
    std::lock_guard<std::mutex> guard(m_synchro);
    m_pInfo->m_uiFramesReady = m_uiCommitedFrames - (m_bIsOutputBufferLocked ? 1 : 0);
    return m_pInfo;
}

void FrameConstructor::AssignAbsPos(int32_t iPos)
{
    if (m_PrevSample.IsHit(iPos))
    {
        m_CurFrame.uiAbsPos = m_PrevSample.uiAbsPos;
        if (m_bPureStream)
            m_CurFrame.uiAbsPos += iPos - m_PrevSample.iBufOffset;
    }
    else if (m_LastSample.IsHit(iPos))
    {
        m_CurFrame.uiAbsPos = m_LastSample.uiAbsPos;
        if (m_bPureStream)
            m_CurFrame.uiAbsPos += iPos - m_LastSample.iBufOffset;
    }
}

void FrameConstructor::AssignTimeStamps(int32_t iPos)
{
    if (m_PrevSample.IsHit(iPos) && !m_PrevSample.GetFlag(FCSample::STAMPS_APPLIED))
    {
        m_CurFrame.dPTS = m_PrevSample.dPTS;
        m_CurFrame.dDTS = m_PrevSample.dDTS;
        m_PrevSample.SetFlag(FCSample::STAMPS_APPLIED, true);
    }
    else if (m_LastSample.IsHit(iPos) && !m_LastSample.GetFlag(FCSample::STAMPS_APPLIED))
    {
        m_CurFrame.dPTS = m_LastSample.dPTS;
        m_CurFrame.dDTS = m_LastSample.dDTS;
        m_LastSample.SetFlag(FCSample::STAMPS_APPLIED, true);
    }
    else
    {
        m_CurFrame.dPTS = -1.0;
        m_CurFrame.dDTS = -1.0;
    }
}

VideoFrameConstructor::VideoFrameConstructor()
{
    m_bSeqSCFound = false;
    m_bPicSCFound = false;
    m_bFrameBegFound = false;
    m_bIsFinalizeSequenceSent = false;
    m_iFirstInDecOrderIdx = 0;
}

Status VideoFrameConstructor::Init(MediaReceiverParams *pInit)
{
    Status umcRes = FrameConstructor::Init(pInit);
    m_bSeqSCFound = false;
    m_bPicSCFound = false;
    m_bFrameBegFound = false;
    m_bIsFinalizeSequenceSent = false;
    m_iFirstInDecOrderIdx = 0;
    return umcRes;
}

Status VideoFrameConstructor::GetSampleFromQueue(FCSample *pSample)
{
    if (m_dRate >= 0.0)
    { // case of forward playback
        return FrameConstructor::GetSampleFromQueue(pSample);
    }

    if (0 == (int32_t)m_uiCommitedFrames)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (m_iFirstInDecOrderIdx >= 0)
        return m_OutputQueue.Get(pSample[0], m_iFirstInDecOrderIdx);

    int32_t iIdx;
    for (iIdx = 0; iIdx < (int32_t)m_uiCommitedFrames; iIdx++)
    {
        m_OutputQueue.Get(pSample[0], iIdx);
        if (I_PICTURE == pSample[0].GetFrameType())
            break;
    }

    // unable to find I_PICTURE
    if (iIdx >= (int32_t)m_uiCommitedFrames)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    m_iFirstInDecOrderIdx = iIdx;
    return UMC_OK;
}

Status VideoFrameConstructor::Reset()
{
    Status umcRes = FrameConstructor::Reset();
    std::lock_guard<std::mutex> guard(m_synchro);
    m_bSeqSCFound = false;
    m_bPicSCFound = false;
    m_bFrameBegFound = false;
    m_bIsFinalizeSequenceSent = false;
    m_iFirstInDecOrderIdx = m_dRate < 0.0 ? -1 : 0;
    return umcRes;
}

Status VideoFrameConstructor::SoftReset()
{
    FrameConstructor::SoftReset();
    std::lock_guard<std::mutex> guard(m_synchro);
    m_bPicSCFound = false;
    m_bFrameBegFound = false;
    return UMC_OK;
}

void VideoFrameConstructor::SetRate(double dRate)
{
    m_dRate = dRate;
    m_iFirstInDecOrderIdx = m_dRate < 0.0 ? -1 : 0;
}

bool VideoFrameConstructor::IsFrameStartFound(void)
{
    if (m_bFrameBegFound && m_bPicSCFound)
    {
        if (I_PICTURE == m_CurFrame.GetFrameType())
            return true;
        if (P_PICTURE == m_CurFrame.GetFrameType() && m_dRate < 4.0 && m_dRate > -4.0)
            return true;
        if (B_PICTURE == m_CurFrame.GetFrameType() && m_dRate < 3.0 && m_dRate > -3.0)
            return true;
    }
    return false;
}

bool VideoFrameConstructor::IsSampleComplyWithTmPolicy(FCSample &sample, double dRate)
{
    double absRate = MFX_ABS(dRate);
    return (absRate < 3 || B_PICTURE != sample.GetFrameType()) &&
           (absRate < 4 || P_PICTURE != sample.GetFrameType());
}

Status Mpeg2FrameConstructor::GetFrame(SplMediaData *frame)
{
    Status umcRes;
    uint8_t *buf = m_pBuf;
    bool bFound = false;

    while (!bFound)
    {
        // Find and parse sequence header
        if (!m_bSeqSCFound)
        {
            if (!m_bInfoFilled)
            { // we havn't yet parsed sequence header
                while (m_lCurPos < m_lLastBytePos - 3 && !IS_CODE(&m_pBuf[m_lCurPos], 0xb3))
                    m_lCurPos++;
                if (m_lCurPos >= m_lLastBytePos - 3)
                    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

                int32_t seqEnd = m_lCurPos + 4;
                while (seqEnd < m_lLastBytePos - 3 && !IS_CODE_4(&m_pBuf[seqEnd], 0x00, 0xb3, 0xb7, 0xb8))
                    seqEnd++;
                if (seqEnd >= m_lLastBytePos - 3)
                    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

                umcRes = ParseSequenceHeader(&m_pBuf[m_lCurPos], m_lLastBytePos - m_lCurPos, m_bInfoFilled ? NULL : m_pInfo);
                if (UMC_OK != umcRes)
                    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

                if (!m_pInfo->m_pDecSpecInfo)
                {
                    // populate decoder specific info with sequence header with all extensions
                    m_pInfo->m_pDecSpecInfo = new MediaData(seqEnd - m_lCurPos);
                    m_pInfo->m_pDecSpecInfo->SetDataSize(seqEnd - m_lCurPos);
                    MFX_INTERNAL_CPY(
                        m_pInfo->m_pDecSpecInfo->GetDataPointer(),
                        m_pBuf + m_lCurPos,
                        seqEnd - m_lCurPos);
                }

                m_bInfoFilled = true;
                m_CurFrame.iBufOffset = m_lCurPos;
                m_lCurPos += 4;
                m_bSeqSCFound = true;
                m_bFrameBegFound = true;

                AssignAbsPos(m_CurFrame.iBufOffset);
            }
            else
            { // we are after reposition, just wait for new sequence header or I-picture
                while (m_lCurPos < m_lLastBytePos - 5 &&
                    !(IS_CODE(&m_pBuf[m_lCurPos], 0xb3)) &&
                    !(IS_CODE(&m_pBuf[m_lCurPos], 0x00) && ((m_pBuf[m_lCurPos + 5] >> 3) & 0x07) == I_PICTURE))
                    m_lCurPos++;
                if (m_lCurPos >= m_lLastBytePos - 5)
                    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

                m_CurFrame.iBufOffset = m_lCurPos;
                m_bSeqSCFound = true;
                AssignAbsPos(m_CurFrame.iBufOffset);
            }
        }

        // Find begin of frame
        if (!m_bFrameBegFound)
        {
            int32_t savePos = m_lCurPos;
            while (m_lCurPos < m_lLastBytePos - 3 && !IS_CODE_3(&m_pBuf[m_lCurPos], 0xb3, 0xb8, 0x00))
                m_lCurPos++;
            if (m_lCurPos >= m_lLastBytePos - 3)
            {
                if (m_bEndOfStream)
                    return UMC_ERR_END_OF_STREAM;

                CutInterval(m_PrevSample, m_LastSample, m_pBuf, savePos, m_lCurPos, m_lLastBytePos, m_bPureStream);
                m_lLastBytePos -= m_lCurPos - savePos;
                m_lCurPos -= m_lCurPos - savePos;
                return UMC_ERR_NOT_ENOUGH_DATA;
            }

            m_CurFrame.iBufOffset = m_lCurPos;
            m_bFrameBegFound = true;

            AssignAbsPos(m_CurFrame.iBufOffset);
        }

        // Find picture start code
        if (!m_bPicSCFound)
        {
            while (m_lCurPos < m_lLastBytePos - 5 && !IS_CODE(&m_pBuf[m_lCurPos], 0x00))
                m_lCurPos++;
            if (m_lCurPos >= m_lLastBytePos - 5)
                return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

            VideoStreamInfo *pVSI = (VideoStreamInfo *)m_pInfo->m_pStreamInfo;
            if (!m_bInfoFilled && MPEG2_VIDEO == pVSI->stream_type && PROGRESSIVE != pVSI->interlace_type)
            {
                umcRes = ParsePictureHeader(&m_pBuf[m_lCurPos], m_lLastBytePos - m_lCurPos, m_bInfoFilled ? NULL : m_pInfo);
                if (UMC_OK != umcRes)
                    return umcRes;
            }
            m_bInfoFilled = true;

            // set time stamps
            AssignTimeStamps(m_lCurPos);

            // set frame type
            m_CurFrame.SetFrameType((buf[m_lCurPos + 5] >> 3) & 0x07);
            m_lCurPos += 6;
            m_bPicSCFound = true;
        }

        // Find begin of the next frame
        if (m_bFrameBegFound && m_bPicSCFound)
        {
            while (m_lCurPos < m_lLastBytePos - 3 && !IS_CODE_4(&m_pBuf[m_lCurPos], 0xb3, 0xb7, 0xb8, 0x00))
                m_lCurPos++;

            if (m_lCurPos >= m_lLastBytePos - 3)
            {
                if (!m_bEndOfStream)
                    return UMC_ERR_NOT_ENOUGH_DATA;

                MFX_INTERNAL_ZERO(&m_pBuf[m_lLastBytePos], 4);
                m_lLastBytePos += 4;
                m_lCurPos = m_lLastBytePos;
            }

            if (IS_CODE(&m_pBuf[m_lCurPos], 0xb7))
                m_lCurPos += 4; // append SEQ_ENC_CODE to current frame

            // Frame found, check type and playback rate
            m_CurFrame.uiSize = m_lCurPos - m_CurFrame.iBufOffset;
            if (IsSampleComplyWithTmPolicy(m_CurFrame, m_dRate))
            {
                m_CurFrame.CopyTo(frame[0], m_pBuf);
                // prepare for next frame
                m_CurFrame.dPTS = m_CurFrame.dDTS = -1.0;
                m_CurFrame.iBufOffset = m_lCurPos;
                bFound = true;
            }
            else
            {
                CutInterval(m_PrevSample, m_LastSample, m_pBuf, m_CurFrame.iBufOffset, m_lCurPos, m_lLastBytePos, m_bPureStream);
                m_lLastBytePos -= m_lCurPos - m_CurFrame.iBufOffset;
                m_lCurPos = m_CurFrame.iBufOffset;
                m_CurFrame.dPTS = m_CurFrame.dDTS = -1.0;
                m_CurFrame.iBufOffset = m_lCurPos;
            }

            // start finding next frame
            m_bFrameBegFound = false;
            m_bPicSCFound = false;
        }
    }

    return UMC_OK;
}

Status Mpeg4FrameConstructor::GetFrame(SplMediaData *frame)
{
    Status umcRes;
    uint8_t *buf = m_pBuf;
    bool bFrameFound = false;

    // initialization loop
    while (!m_bSeqSCFound)
    {
#if defined (__ICL)
        // pointless comparison of unsigned integer with zero
        // need to add one
#pragma warning(disable:186)
#endif
        while (m_lCurPos < m_lLastBytePos - 3 && !IS_CODE_INT(&buf[m_lCurPos], 0x00, 0x2F) && !IS_CODE(&buf[m_lCurPos], 0xb0))
            m_lCurPos++;
        if (m_lCurPos >= m_lLastBytePos - 3)
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

        m_CurFrame.iBufOffset = m_lCurPos;
        AssignAbsPos(m_CurFrame.iBufOffset);

        int32_t offset = 0;
        while (m_lCurPos + offset < m_lLastBytePos - 3 && !IS_CODE(&m_pBuf[m_lCurPos + offset], 0xb6))
            offset++;
        if (m_lCurPos + offset >= m_lLastBytePos - 5)
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

        umcRes = ParseVideoObjectLayer(&m_pBuf[m_lCurPos], -1, m_bInfoFilled ? NULL : m_pInfo);
        if (UMC_OK == umcRes)
        {
            m_bInfoFilled = true;
            m_bSeqSCFound = true;
            m_CurFrame.SetFrameType((buf[m_lCurPos + offset + 4] >> 6) + 1);
            AssignTimeStamps(m_lCurPos + offset);
            m_lCurPos += offset + 4;
        }
        else
        {
            m_lCurPos++;
        }
    }

    while (!bFrameFound)
    {
        while ((m_lCurPos < m_lLastBytePos - 3) && !IS_CODE(&buf[m_lCurPos + 0], 0xb6))
            m_lCurPos++;

        if (m_lCurPos >= m_lLastBytePos - 4)
        {
            if (!m_bEndOfStream)
                return UMC_ERR_NOT_ENOUGH_DATA;
            if (m_lCurPos >= m_lLastBytePos)
                return UMC_ERR_END_OF_STREAM;
            m_lCurPos = m_lLastBytePos;
        }

        //frame found, check type and playback rate
        if (IsSampleComplyWithTmPolicy(m_CurFrame, m_dRate))
        {
            m_CurFrame.uiSize = m_lCurPos - m_CurFrame.iBufOffset;
            m_CurFrame.CopyTo(frame[0], m_pBuf);
            bFrameFound = true;
        }

        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.SetFrameType((buf[m_lCurPos + 4] >> 6) + 1);
        AssignTimeStamps(m_CurFrame.iBufOffset);
        AssignAbsPos(m_CurFrame.iBufOffset);
        m_lCurPos += 4;
    }

    return UMC_OK;
}

Status H261FrameConstructor::GetFrame(SplMediaData *frame)
{
    Status umcRes;
    uint8_t *buf = m_pBuf;
    bool bFrameFound = false;

    while (!m_bSeqSCFound)
    {
        while ((m_lCurPos < m_lLastBytePos - 2) &&
                (buf[m_lCurPos + 0] != 0 ||
                 buf[m_lCurPos + 1] != 1 ||
                (buf[m_lCurPos + 2] & 0xf0) != 0))
            m_lCurPos++;

        umcRes = ParseHeader(&m_pBuf[m_lCurPos], m_lLastBytePos - m_lCurPos, m_bInfoFilled ? NULL : m_pInfo);
        if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

        m_bSeqSCFound = true;
        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.SetFrameType(NONE_PICTURE);
        AssignTimeStamps(m_CurFrame.iBufOffset);
        AssignAbsPos(m_CurFrame.iBufOffset);
        m_lCurPos += 3;
    }

    while (!bFrameFound)
    {
        while ((m_lCurPos < m_lLastBytePos - 2) &&
                (buf[m_lCurPos + 0] != 0 ||
                 buf[m_lCurPos + 1] != 1 ||
                (buf[m_lCurPos + 2] & 0xf0) != 0))
            m_lCurPos++;

        if (m_lCurPos + 3 >= m_lLastBytePos)
        {
            if (!m_bEndOfStream)
                return UMC_ERR_NOT_ENOUGH_DATA;
            if (m_lCurPos >= m_lLastBytePos)
                return UMC_ERR_END_OF_STREAM;
            m_lCurPos = m_lLastBytePos;
        }

        // found frame
        m_CurFrame.uiSize = m_lCurPos - m_CurFrame.iBufOffset;
        m_CurFrame.CopyTo(frame[0], m_pBuf);
        bFrameFound = true;

        // next frame
        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.SetFrameType(NONE_PICTURE);
        AssignTimeStamps(m_CurFrame.iBufOffset);
        AssignAbsPos(m_CurFrame.iBufOffset);
        m_lCurPos += 3;
    }

    return UMC_OK;
}

Status H263FrameConstructor::GetFrame(SplMediaData *frame)
{
    Status umcRes;
    uint8_t *buf = m_pBuf;
    bool bFrameFound = false;

    while (!m_bSeqSCFound)
    {
        while ((m_lCurPos < m_lLastBytePos - 2) &&
                (buf[m_lCurPos + 0] != 0 ||
                 buf[m_lCurPos + 1] != 0 ||
                (buf[m_lCurPos + 2] & 0xfc) != 0x80))
            m_lCurPos++;

        umcRes = ParseHeader(&m_pBuf[m_lCurPos], m_lLastBytePos - m_lCurPos, m_bInfoFilled ? NULL : m_pInfo);
        if (UMC_ERR_INVALID_STREAM == umcRes)
        {
            m_lCurPos += 1;
            continue;
        }
        else if (UMC_ERR_NOT_ENOUGH_DATA == umcRes)
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;

        m_bSeqSCFound = true;
        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.SetFrameType(NONE_PICTURE);
        AssignTimeStamps(m_CurFrame.iBufOffset);
        AssignAbsPos(m_CurFrame.iBufOffset);
        m_lCurPos += 3;
    }

    while (!bFrameFound)
    {
        while ((m_lCurPos < m_lLastBytePos - 2) &&
                (buf[m_lCurPos + 0] != 0 ||
                 buf[m_lCurPos + 1] != 0 ||
                (buf[m_lCurPos + 2] & 0xfc) != 0x80))
            m_lCurPos++;

        if (m_lCurPos + 3 >= m_lLastBytePos)
        {
            if (!m_bEndOfStream)
                return UMC_ERR_NOT_ENOUGH_DATA;
            if (m_lCurPos >= m_lLastBytePos)
                return UMC_ERR_END_OF_STREAM;
            m_lCurPos = m_lLastBytePos;
        }

        // found frame
        m_CurFrame.uiSize = m_lCurPos - m_CurFrame.iBufOffset;
        m_CurFrame.CopyTo(frame[0], m_pBuf);
        bFrameFound = true;

        // next frame
        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.SetFrameType(NONE_PICTURE);
        AssignTimeStamps(m_CurFrame.iBufOffset);
        AssignAbsPos(m_CurFrame.iBufOffset);
        m_lCurPos += 3;
    }

    return UMC_OK;
}

Status AudioFrameConstructor::Init(MediaReceiverParams *pInit)
{
    Status umcRes = FrameConstructor::Init(pInit);
    m_bHeaderParsed = false;
    return umcRes;
}

Status TimeStampedAudioFrameConstructor::GetFrame(SplMediaData *frame)
{
    if (m_lLastBytePos - m_lCurPos <= 0)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    if (!m_bHeaderParsed && (-1.0 != m_LastSample.dPTS || -1.0 != m_CurFrame.dPTS))
    {
        // first chunk with valid timestamp was received
        Status umcRes = UMC_OK;
        if (!m_bHeaderParsed)
            umcRes = ParseHeader();
        if (UMC_OK == umcRes)
        { // syncword found!
            m_CurFrame.uiAbsPos = m_LastSample.uiAbsPos;
            m_CurFrame.iBufOffset = m_lCurPos;
            m_lCurPos = m_lLastBytePos;
            if (m_CurFrame.iBufOffset < m_LastSample.iBufOffset && m_PrevSample.dPTS >= 0.0)
            {
                m_CurFrame.dPTS = m_PrevSample.dPTS;
                m_CurFrame.uiAbsPos = m_PrevSample.uiAbsPos;
            }
            if (m_CurFrame.dPTS < 0.0 && m_LastSample.dPTS >= 0.0)
                m_CurFrame.dPTS = m_LastSample.dPTS;

            m_bHeaderParsed = true;
        }
        else if (UMC_ERR_SYNC == umcRes)
        { // can't found syncword, skip data
            return UMC_ERR_SYNC;
        }

        if (!m_bEndOfStream)
        {
            if (m_CurFrame.dPTS < 0.0)
                m_CurFrame.dPTS = m_LastSample.dPTS;
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
    }
    else if (-1.0 != m_LastSample.dPTS && -1.0 == m_CurFrame.dPTS)
    { // header has already parsed, first chunk
        m_CurFrame.iBufOffset = m_lCurPos;
        m_CurFrame.dPTS = m_LastSample.dPTS;
        m_CurFrame.uiAbsPos = m_LastSample.uiAbsPos;
        m_lCurPos = m_lLastBytePos;
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;
    }
    else if (-1.0 == m_LastSample.dPTS && -1.0 != m_CurFrame.dPTS)
    {
        // data from current frame, move pointers, wait for next valid timestamp
        m_lCurPos = m_lLastBytePos;
        if (!m_bEndOfStream)
            return UMC_ERR_NOT_ENOUGH_DATA;
    }
    else if (-1.0 == m_LastSample.dPTS && -1.0 == m_CurFrame.dPTS)
    {
        // skip data, wait for first chunk with valid timestamp
        m_lCurPos = m_lLastBytePos;
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_SYNC;
    }

    if (m_CurFrame.iBufOffset == m_LastSample.iBufOffset)
    {
        if (!m_bEndOfStream)
            return UMC_ERR_NOT_ENOUGH_DATA;
        m_CurFrame = m_LastSample;
    }
    else
        m_CurFrame = m_PrevSample;

    m_CurFrame.CopyTo(frame[0], m_pBuf);
    m_lCurPos = m_CurFrame.iBufOffset + m_CurFrame.uiSize;
    m_CurFrame = m_LastSample;
    return UMC_OK;
}

BufferedAudioFrameConstructor::BufferedAudioFrameConstructor(double dToBuf)
: AudioFrameConstructor(), m_dToBuf(MFX_MAX(0.0, dToBuf))
{
}

Status BufferedAudioFrameConstructor::GetFrame(SplMediaData *frame)
{
    if (m_lLastBytePos == m_lCurPos)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    if (!m_bHeaderParsed)
    {
        // requires not empty m_PrevSample with valid PTS
        if (m_PrevSample.uiSize == 0 || m_PrevSample.dPTS < 0.0)
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

        Status umcRes = ParseHeader();
        if (umcRes == UMC_OK)
        {
            m_bHeaderParsed = true;
            if (m_LastSample.IsHit(m_lCurPos))
            {
                m_PrevSample.MovePointer(m_PrevSample.uiSize);
                m_CurFrame = m_LastSample;
                m_CurFrame.MovePointer(m_lCurPos - m_LastSample.iBufOffset);
                if (m_CurFrame.dPTS < 0.0)
                    m_CurFrame.dPTS = m_PrevSample.dPTS;
                return UMC_ERR_END_OF_STREAM;
            }
            else
            {
                m_CurFrame = m_PrevSample;
                m_CurFrame.MovePointer(m_lCurPos - m_PrevSample.iBufOffset);
            }
        }
        else
            return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : umcRes;
    }

    if (m_CurFrame.uiSize == 0)
        m_CurFrame = m_PrevSample;
    if (m_CurFrame.uiSize == 0 || m_CurFrame.dPTS < 0.0)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    if (m_LastSample.dPTS < 0.0 && !m_bEndOfStream)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (m_bEndOfStream || // case of eos
        (m_LastSample.dPTS - m_CurFrame.dPTS < 0) || // case of discontinuity
        (m_LastSample.dPTS - m_CurFrame.dPTS > m_dToBuf)) // enough data is already buffered
    {
        m_CurFrame.uiSize = m_LastSample.iBufOffset - m_CurFrame.iBufOffset;
        if (m_bEndOfStream)
        {
            m_CurFrame.uiSize = m_lLastBytePos - m_CurFrame.iBufOffset;
            m_LastSample.MovePointer(m_LastSample.uiSize);
        }
        m_CurFrame.CopyTo(*frame, m_pBuf);
        m_PrevSample.MovePointer(m_PrevSample.uiSize);
        m_CurFrame = m_LastSample;
        m_lCurPos = m_CurFrame.iBufOffset;
    }
    else
        return UMC_ERR_NOT_ENOUGH_DATA;

    return UMC_OK;
}

Status PureAudioFrameConstructor::GetFrame(SplMediaData *frame)
{
    if (m_lLastBytePos - m_lCurPos <= 0 && !m_bEndOfStream)
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (!m_bHeaderParsed)
    {
        Status umcRes = ParseHeader();
        if (UMC_OK == umcRes)
        { // syncword found!
            m_bHeaderParsed = true;
        }
        else if (UMC_ERR_SYNC == umcRes)
        { // can't found syncword, skip data
            return UMC_ERR_SYNC;
        }
    }

    if (m_lCurPos == m_lLastBytePos)
        return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;

    frame->SetBufferPointer(m_pBuf + m_lCurPos, m_lLastBytePos - m_lCurPos);
    frame->SetDataSize(m_lLastBytePos - m_lCurPos);
    frame->SetTime(m_LastSample.dPTS, m_LastSample.dDTS);
    frame->SetAbsPos(m_LastSample.uiAbsPos);
    m_lCurPos = m_lLastBytePos;
    m_CurFrame.iBufOffset = m_lCurPos;
    return UMC_OK;
}

Status AudioFrameConstructor::ParseHeader()
{
    AudioStreamInfo *pASI = (AudioStreamInfo *)m_pInfo->m_pStreamInfo;
    int32_t curPos = m_lCurPos;
    uint8_t *buf = m_pBuf;

    if (TRACK_MPEGA == m_pInfo->m_Type)
    {
        uint32_t header = 0, nextHeader;
        int32_t id = 0, layer = 0, bitrate = 0, freq = 0, mode = 0, padding = 0, protection = 0;
        int32_t mpg25 = 0;
        bool bFound = false;
        bool bSyncWordFound = false;

        for (; curPos < m_lLastBytePos - 3; curPos++)
        {
            // find syncword
            if (buf[curPos] != 0xff || (buf[curPos + 1] & 0xe0) != 0xe0)
                continue;

            header = (buf[curPos + 0] << 24) |
                     (buf[curPos + 1] << 16) |
                     (buf[curPos + 2] <<  8) |
                     (buf[curPos + 3]);

            id = MPEGA_HDR_VERSION(header);
            layer = MPEGA_HDR_LAYER(header);
            freq = MPEGA_HDR_SAMPLINGFREQ(header);
            bitrate = MPEGA_HDR_BITRADEINDEX(header);
            mode = MPEGA_HDR_MODE(header);
            padding = MPEGA_HDR_PADDING(header);
            protection = MPEGA_HDR_ERRPROTECTION(header);
            mpg25 = ((header >> 20) & 1) ? 0 : 2;

            // check forbidden values
            if ( 4 == layer   ||
                 3 == freq    ||
#ifdef FREE_FORMAT_PROHIBIT
                 0 == bitrate ||
#endif //FREE_FORMAT_PROHIBIT
                15 == bitrate ||
                (mpg25 > 0 && id == 1)) // for mpeg2.5 id should be 0
                continue;

            bSyncWordFound = true;

            int32_t size = 0;

            // evaluate frame size
            if (layer == 3)
                size = 72000 * (id + 1);
            else if (layer == 2)
                size = 72000 * 2;
            else if (layer == 1)
                size = 12000;

            size = size * MpegABitrate[id][layer - 1][bitrate] / MpegAFrequency[id + mpg25][freq] + padding;
            if (layer == 1)
                size *= 4;

            if (curPos + size >= m_lLastBytePos - 3)
                return UMC_ERR_NOT_ENOUGH_DATA;

            // check syncword of next frame
            if (buf[curPos + size] != 0xff || ((buf[curPos + size + 1] & 0xe0) != 0xe0)) // check syncword
            {
                bSyncWordFound = false;
                continue;
            }

            nextHeader = (m_pBuf[curPos + size + 0] << 16) |
                         (m_pBuf[curPos + size + 1] <<  8) |
                         (m_pBuf[curPos + size + 2]);

            // compare headers
            if ((nextHeader ^ (header >> 8)) & 0xfffe0c)
            {
                bSyncWordFound = false;
                continue;
            }

            // found header is valid
            bFound = true;
            break;
        }

        m_lCurPos = curPos;
        if (!bFound)
            return bSyncWordFound ? UMC_ERR_NOT_ENOUGH_DATA : UMC_ERR_SYNC;

        // fill info structure
        pASI->is_protected     = protection == 0;
        pASI->sample_frequency = MpegAFrequency[id + mpg25][freq];
        pASI->channels         = MpegAChannels[mode];
        pASI->bitPerSample     = 16;
        pASI->bitrate          = MpegABitrate[id][layer - 1][bitrate] * 1000;
        pASI->stream_type      = MpegAStreamType[id][layer - 1];
        pASI->header           = header;
        m_pInfo->m_Type        = ConvertAudioType(pASI->stream_type);
    }
    else if (TRACK_AC3 == m_pInfo->m_Type)
    {
        bool bFound = false;
        int32_t sample_rate_code = 0, frame_size_code = 0;
        for (; curPos < m_lLastBytePos - 4; curPos++)
        {
            if (buf[curPos + 0] != 0x0b || buf[curPos + 1] != 0x77)
                continue;

            // check forbidden values
            sample_rate_code = buf[curPos + 4] >> 6;
            frame_size_code  = buf[curPos + 4] & 0x3F;
            if (sample_rate_code == 0x03 || frame_size_code > 37)
                continue;

            bFound = true;
            break;
        }

        m_lCurPos = curPos;
        if (!bFound)
            return UMC_ERR_SYNC;

        // fill info structure
        pASI->sample_frequency = AC3Frequency[sample_rate_code];
        pASI->channels         = 2;
        pASI->bitPerSample     = 16;
        pASI->bitrate          = AC3BitRate[frame_size_code / 2];
    }
    else if (TRACK_LPCM == m_pInfo->m_Type)
    {
        pASI->bitrate = 48000 * 16 * pASI->channels;
        pASI->sample_frequency = 48000;
        pASI->bitPerSample = 16;
    }
    else if (TRACK_AAC == m_pInfo->m_Type && AAC_MPEG4_STREAM == pASI->stream_type)
    {
        uint32_t pos = 0;
        uint8_t *ptr = (uint8_t *)m_pInfo->m_pDecSpecInfo->GetDataPointer();
        size_t len = m_pInfo->m_pDecSpecInfo->GetDataSize();

        if (len < 2)
            return UMC_OK;
        //uint32_t audioObjectType = m_pBuf[0] >> 3;
        uint32_t freq_index = ((ptr[0] & 0x7) << 1) | (ptr[1] >> 7);
        if (0x0f == freq_index)
        {
            if (len < 5)
                return UMC_OK;
            pASI->sample_frequency = ((ptr[1] & 0x7f) << 17) | (ptr[2] << 9) |
                (ptr[3] << 1) | ((ptr[4] & 0x80) >> 7);
            pos += 3;
        }
        else
            pASI->sample_frequency = AACFrequency[freq_index];

        uint32_t chan = (ptr[pos + 1] >> 3) & 0x0f;
        pASI->channels = AACChannels[chan];
        pASI->bitPerSample = 16;
    }
    else if (TRACK_AAC == m_pInfo->m_Type && AAC_AUDIO == pASI->stream_type)
    {
        int32_t freq = 0, protection = 0, chan_conf = 0, length = 0;
        bool bFound = false;
        bool bSyncWordFound = false;

        for (; curPos + 5 < m_lLastBytePos; curPos++)
        {
            // find syncword
            if (buf[curPos] != 0xff || (buf[curPos + 1] & 0xf6) != 0xf0)
                continue;

            uint32_t uint = (buf[curPos + 0] << 24) | (buf[curPos + 1] << 16) |
                          (buf[curPos + 2] <<  8) | (buf[curPos + 3]);

            protection = (uint >> 16) & 0x01;
            freq = (uint >> 10) & 0x0f;
            chan_conf = (uint >> 6) & 0x07;
            length = ((buf[curPos + 3] & 0x03) << 11) + (buf[curPos + 4] << 3) + ((buf[curPos + 5] >> 5) & 0x07);
            int32_t max_length = 768 * AACChannels[chan_conf] + 7 + 2 * (protection == 0);

            // check forbidden values
            if (freq > 12 || chan_conf > 7 || 0 == length || length > max_length)
                continue;

            bSyncWordFound = true;

            if (curPos + length + 3 >= m_lLastBytePos)
                return UMC_ERR_NOT_ENOUGH_DATA;

            uint32_t next_uint =
                (buf[curPos + length + 0] << 24) | (buf[curPos + length + 1] << 16) |
                (buf[curPos + length + 2] <<  8) | (buf[curPos + length + 3]);

            // ID, layer, profile and frequency must be the same
            if ((uint ^ next_uint) & 0xfffefc00)
            {
                bSyncWordFound = false;
                continue;
            }

            // found valid header
            bFound = true;
            break;
        }

        m_lCurPos = curPos;
        if (!bFound)
            return bSyncWordFound ? UMC_ERR_NOT_ENOUGH_DATA : UMC_ERR_SYNC;

        // fill info structure
        pASI->is_protected = protection == 0;
        pASI->sample_frequency = AACFrequency[freq];
        pASI->channels = AACChannels[chan_conf];
        pASI->bitPerSample = 16;
        pASI->bitrate = 0;
        pASI->stream_type = AAC_AUDIO;
        m_pInfo->m_Type = TRACK_AAC;
    }
    else if (TRACK_DTS == m_pInfo->m_Type)
    {
        uint8_t byte0 = buf[curPos + 0], byte1 = buf[curPos + 1], byte2 = buf[curPos + 2];
        uint8_t byte3 = buf[curPos + 3], byte4 = buf[curPos + 4], byte5 = buf[curPos + 5];
        int32_t freq_code = 0, chan_code = 0, rate_code = 0;

        if (0x7f == byte0 && 0xfe == byte1 && 0x80 == byte2 && 0x01 == byte3)
        { // 16-bit word in BE
            if (curPos + 10 >= m_lLastBytePos)
                return UMC_ERR_NOT_ENOUGH_DATA;

            chan_code = ((buf[curPos + 7] & 0x0f) << 2) | ((buf[curPos + 8] & 0xc0) >> 6);
            freq_code = ((buf[curPos + 8] & 0x3c) >> 2);
            rate_code = ((buf[curPos + 8] & 0x03) << 3) | ((buf[curPos + 9] & 0xe0) >> 5);
        }
        else if (0x1f == byte0 && 0xff == byte1 && 0xe8 == byte2 && 0x00 == byte3 && 0x01 == (byte4 & 0xfc))
        { // 14-bit word in BE
            chan_code = 0;
            freq_code = 0;
            rate_code = 0;
        }
        else if (0xfe == byte0 && 0x7f == byte1 && 0x01 == byte2 && 0x80 == byte3)
        { // 16-bit word in LE
            if (curPos + 10 >= m_lLastBytePos)
                return UMC_ERR_NOT_ENOUGH_DATA;

            chan_code = 0;
            freq_code = 0;
            rate_code = 0;
        }
        else if (0xff == byte0 && 0x1f == byte1 && 0x00 == byte2 && 0xe8 == byte3 && 0x01 == (byte5 & 0xfc))
        { // 14-bit word in LE
            chan_code = 0;
            freq_code = 0;
            rate_code = 0;
        }

        pASI->channels = DTSChannels[chan_code > 15 ? 1 : chan_code];
        pASI->sample_frequency = DTSFrequency[freq_code];
        pASI->bitrate = DTSBitrate[rate_code];
        pASI->bitPerSample = 16;
        pASI->stream_type = DTS_AUDIO;
        m_pInfo->m_Type = TRACK_DTS;
    }

    return UMC_OK;
}

Mpeg2TrackInfo::Mpeg2TrackInfo(void)
{
    m_isSelected = 1;
    m_uiProgNum = 0;
    m_uiFramesReady = 0;
    m_iFirstFrameOrder = -1;
}

Status Mpeg2TrackInfo::CopyFrom(Mpeg2TrackInfo *pSrc)
{
    m_Type = pSrc->m_Type;
    m_PID = pSrc->m_PID;
    m_isSelected = pSrc->m_isSelected;
    m_uiProgNum = pSrc->m_uiProgNum;
    m_uiFramesReady = pSrc->m_uiFramesReady;
    m_iFirstFrameOrder = pSrc->m_iFirstFrameOrder;

    delete m_pDecSpecInfo;
    m_pDecSpecInfo = NULL;
    if (pSrc->m_pDecSpecInfo)
    {
        size_t size = pSrc->m_pDecSpecInfo->GetDataSize();
        UMC_NEW(m_pDecSpecInfo, MediaData);
        UMC_CALL(m_pDecSpecInfo->Alloc(size));
        MFX_INTERNAL_CPY((uint8_t *)m_pDecSpecInfo->GetDataPointer(), (uint8_t *)pSrc->m_pDecSpecInfo->GetDataPointer(), (int)size);
        UMC_CALL(m_pDecSpecInfo->SetDataSize(size));
    }

    ReleaseStreamInfo();
    if (pSrc->m_pStreamInfo)
    {
        Status umcRes = Alloc();
        if (UMC_OK != umcRes)
            return umcRes;

        if (m_pStreamInfo)
        {
            if (m_Type & TRACK_ANY_AUDIO)
                *((AudioStreamInfo *)m_pStreamInfo) = *((AudioStreamInfo *)pSrc->m_pStreamInfo);
            else if (m_Type & TRACK_ANY_VIDEO)
                *((VideoStreamInfo *)m_pStreamInfo) = *((VideoStreamInfo *)pSrc->m_pStreamInfo);
            else if (m_Type & TRACK_VBI_TXT)
                *((TeletextStreamInfo *)m_pStreamInfo) = *((TeletextStreamInfo *)pSrc->m_pStreamInfo);
        }
    }
    return UMC_OK;
}

void Mpeg2TrackInfo::ReleaseStreamInfo(void)
{
    delete m_pStreamInfo;
    m_pStreamInfo = NULL;
}

void Mpeg2TrackInfo::ReleaseDecSpecInfo(void)
{
    delete m_pDecSpecInfo;
    m_pDecSpecInfo = NULL;
}

void Mpeg2TrackInfo::ReleaseAll(void)
{
    m_iFirstFrameOrder = -1;
    m_isSelected = false;
    m_Type = TRACK_UNKNOWN;
    m_uiFramesReady = 0;
    m_uiProgNum = 0;
    m_PID = 0;

    ReleaseDecSpecInfo();
    ReleaseStreamInfo();
}

Status Mpeg2TrackInfo::Alloc(void)
{
    if (m_pStreamInfo)
        return UMC_OK;

    if (m_Type & TRACK_ANY_AUDIO)
    {
        m_pStreamInfo = new AudioStreamInfo;
        ((AudioStreamInfo *)m_pStreamInfo)->streamPID = m_PID;
        ((AudioStreamInfo *)m_pStreamInfo)->stream_type = (AudioStreamType)ConvertTrackType(m_Type);
    }
    else if (m_Type & TRACK_ANY_VIDEO)
    {
        m_pStreamInfo = new VideoStreamInfo;
        ((VideoStreamInfo *)m_pStreamInfo)->streamPID = m_PID;
        ((VideoStreamInfo *)m_pStreamInfo)->stream_type = (VideoStreamType)ConvertTrackType(m_Type);
    }
    else if (m_Type & TRACK_VBI_TXT)
    {
        m_pStreamInfo = new TeletextStreamInfo;
    }

    return UMC_OK;
}

void Mpeg2TrackInfo::SetDuration(double dDuration)
{
    if (m_pStreamInfo)
    {
        if (m_Type & TRACK_ANY_AUDIO)
            ((AudioStreamInfo *)m_pStreamInfo)->duration = dDuration;
        else if (m_Type & TRACK_ANY_VIDEO)
            ((VideoStreamInfo *)m_pStreamInfo)->duration = dDuration;
    }
}

SplMediaData::SplMediaData()
{
    m_uiAbsPos = 0;
    m_uiFlags = 0;
}

void SplMediaData::SetAbsPos(unsigned long long uiAbsPos)
{
    m_uiAbsPos = uiAbsPos;
}

unsigned long long SplMediaData::GetAbsPos(void) const
{
    return m_uiAbsPos;
}

bool SplMediaData::SetFlag(uint32_t mask, bool flag)
{
    bool previousFlag = (m_uiFlags & mask) ? true : false;
    m_uiFlags = flag ? (m_uiFlags | mask) : (m_uiFlags & ~mask);
    return previousFlag;
}

bool SplMediaData::GetFlag(uint32_t mask) const
{
    return (m_uiFlags & mask) ? true : false;
}

uint32_t SplMediaData::SetFlags(uint32_t flags)
{
    uint32_t oldFlags = m_uiFlags;
    m_uiFlags = flags;
    return oldFlags;
}

uint32_t SplMediaData::GetFlags() const
{
    return m_uiFlags;
}

FCSample::FCSample()
{
    Reset();
}

void FCSample::Reset(void)
{
    dPTS = -1.0;
    dDTS = -1.0;
    uiSize = 0;
    uiAbsPos = 0;
    uiFlags = 0;
    iBufOffset = 0;
}

void FCSample::CopyFrom(MediaData &data, int32_t iOffset)
{
    data.GetTime(dPTS, dDTS);
    uiSize = (uint32_t)data.GetDataSize();
    iBufOffset = iOffset;
    uiFlags = 0;
    SetFrameType(data.GetFrameType());
    if (DynamicCast<SplMediaData>(&data))
    {
        uiAbsPos = ((SplMediaData *)&data)->GetAbsPos();
        uiFlags = ((SplMediaData *)&data)->GetFlags();
    }
}

void FCSample::CopyTo(MediaData &data, uint8_t *pBufBase)
{
    data.SetBufferPointer(pBufBase + iBufOffset, uiSize);
    data.SetDataSize(uiSize);
    data.SetTime(dPTS, dDTS);
    data.SetFrameType((FrameType)GetFrameType());
    if (DynamicCast<SplMediaData>(&data))
    {
        ((SplMediaData *)&data)->SetAbsPos(uiAbsPos);
        ((SplMediaData *)&data)->SetFlags(uiFlags);
    }
}

bool FCSample::IsHit(int32_t iPos)
{
    return (iBufOffset <= iPos && iPos < iBufOffset + (int32_t)uiSize);
}

uint32_t FCSample::SetFrameType(uint32_t uiType)
{
    FrameType previousType = (FrameType)(uiFlags & 0x07);
    uiFlags = (uiFlags & ~0x07) | uiType;
    return previousType;
}

uint32_t FCSample::GetFrameType(void)
{
    return (FrameType)(uiFlags & 0x07);
}

bool FCSample::SetFlag(uint32_t uiFlagMask, bool bNewFlag)
{
    bool previousFlag = (uiFlags & uiFlagMask) ? true : false;
    uiFlags = bNewFlag ? (uiFlags | uiFlagMask) : (uiFlags & ~uiFlagMask);
    return previousFlag;
}

bool FCSample::GetFlag(uint32_t uiFlagMask)
{
    return (uiFlags & uiFlagMask) ? true : false;
}

void FCSample::MovePointer(uint32_t off)
{
    off = MFX_MIN(off, uiSize);
    uiSize -= off;
    iBufOffset += off;
}

InnerListElement::InnerListElement(void)
{
    pNext = NULL;
    pPrev = NULL;
}

InnerListElement::InnerListElement(FCSample &rData)
{
    m_data = rData;
    pNext = NULL;
    pPrev = NULL;
}

ReorderQueue::ReorderQueue(void)
{
    m_pSuperFirst = NULL;
    m_pSuperLast = NULL;
}

void ReorderQueue::AddToSuperList(InnerListElement *pAddedElem)
{
    if (!m_pSuperFirst)
        m_pSuperFirst = pAddedElem;
    if (m_pSuperLast)
        m_pSuperLast->pNext = pAddedElem;
    m_pSuperLast = pAddedElem;
}

void ReorderQueue::RemoveFromSuperList(void)
{
    InnerListElement *pAfter = m_pHeap->m_data.pNext;
    InnerListElement *pBefore = m_pHeap->m_data.pPrev;

    if (pBefore == NULL)
        m_pSuperFirst = pAfter;
    else
        pBefore->pNext = pAfter;

    if (pAfter == NULL)
        m_pSuperLast = pBefore;
    else
        pAfter->pPrev = pBefore;
}

Status ReorderQueue::Add(FCSample &rSample)
{
    InnerListElement innerElem(rSample);
    innerElem.pPrev = m_pSuperLast;
    Status umcRes = LinkedList<InnerListElement>::Add(innerElem);
    if (UMC_OK == umcRes)
        AddToSuperList(&m_pLast->m_data);
    return umcRes;
}

Status ReorderQueue::Add(FCSample &rSample, int32_t idx)
{
    Status umcRes;
    InnerListElement innerElem(rSample);
    innerElem.pPrev = m_pSuperLast;
    if (idx < m_iSize)
    {
        umcRes = LinkedList<InnerListElement>::Add(innerElem, idx);
        if (UMC_OK == umcRes)
            AddToSuperList(&LinkedList<InnerListElement>::GetElement(idx)->m_data);
    }
    else
    {
        umcRes = LinkedList<InnerListElement>::Add(innerElem);
        if (UMC_OK == umcRes)
            AddToSuperList(&m_pLast->m_data);
    }
    return umcRes;
}

Status ReorderQueue::Remove(void)
{
    Status umcRes = LinkedList<InnerListElement>::Remove();
    if (UMC_OK == umcRes)
        RemoveFromSuperList();
    return umcRes;
}

Status ReorderQueue::Remove(int32_t idx)
{
    Status umcRes = LinkedList<InnerListElement>::Remove(idx);
    if (UMC_OK == umcRes)
        RemoveFromSuperList();
    return umcRes;
}

Status ReorderQueue::First(FCSample &rSample)
{
    LinkedList<InnerListElement>::ListElement *pElem = LinkedList<InnerListElement>::GetElement(0);
    if (!pElem)
        return UMC_ERR_NOT_ENOUGH_DATA;

    rSample = pElem->m_data.m_data;
    return UMC_OK;
}

Status ReorderQueue::Last(FCSample &rSample)
{
    LinkedList<InnerListElement>::ListElement *pElem = LinkedList<InnerListElement>::GetElement(m_iSize - 1);
    if (!pElem)
        return UMC_ERR_NOT_ENOUGH_DATA;

    rSample = pElem->m_data.m_data;
    return UMC_OK;
}

Status ReorderQueue::Next(FCSample &rSample)
{
    if (NULL == m_pLastReturned)
        return UMC_ERR_FAILED;

    if (NULL == m_pLastReturned->pNext)
        return UMC_ERR_NOT_ENOUGH_DATA;

    m_pLastReturned = m_pLastReturned->pNext;
    rSample = m_pLastReturned->m_data.m_data;
    return UMC_OK;
}

Status ReorderQueue::Prev(FCSample &rSample)
{
    if (NULL == m_pLastReturned)
        return UMC_ERR_FAILED;

    if (NULL == m_pLastReturned->pPrev)
        return UMC_ERR_NOT_ENOUGH_DATA;

    m_pLastReturned = m_pLastReturned->pPrev;
    rSample = m_pLastReturned->m_data.m_data;
    return UMC_OK;
}

Status ReorderQueue::Get(FCSample &rSample, int32_t idx)
{
    LinkedList<InnerListElement>::ListElement *pElem = LinkedList<InnerListElement>::GetElement(idx);
    if (!pElem)
        return UMC_ERR_NOT_ENOUGH_DATA;

    rSample = pElem->m_data.m_data;
    return UMC_OK;
}

const FCSample *ReorderQueue::FirstBO(void) const
{
    return m_pSuperFirst ? &m_pSuperFirst->m_data : NULL;
}

const FCSample *ReorderQueue::LastBO(void) const
{
    return m_pSuperLast ? &m_pSuperLast->m_data : NULL;
}

Status PesFrameConstructor::GetFrame(SplMediaData *frame)
{
    if (m_PrevSample.uiSize > 0)
    {
        m_CurFrame.iBufOffset = m_PrevSample.iBufOffset + m_PrevSample.uiSize;
        m_PrevSample.CopyTo(frame[0], m_pBuf);
        m_PrevSample.MovePointer(m_PrevSample.uiSize);
        return UMC_OK;
    }
    else if (m_bEndOfStream)
    {
        if (m_LastSample.uiSize > 0)
        {
            m_CurFrame.iBufOffset = m_LastSample.iBufOffset + m_LastSample.uiSize;
            m_LastSample.CopyTo(frame[0], m_pBuf);
            m_LastSample.MovePointer(m_LastSample.uiSize);
            return UMC_OK;
        }
    }

    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM : UMC_ERR_NOT_ENOUGH_DATA;
}

Status PesFrameConstructor::Init(MediaReceiverParams *pInit)
{
    m_bIsFirst = true;
    return FrameConstructor::Init(pInit);
}

Status PesFrameConstructor::Reset(void)
{
    m_bIsFirst = true;
    return FrameConstructor::Reset();
}

Status PesFrameConstructor::SoftReset(void)
{
    m_bIsFirst = true;
    return FrameConstructor::SoftReset();
}

BitstreamReader::BitstreamReader(void)
{
    m_pSource = NULL;
    m_pEnd = NULL;
    m_nBits = 0;
    m_iReadyBits = 0;
}

BitstreamReader::BitstreamReader(uint8_t *pStream, uint32_t len)
{
    Init(pStream, len);
}

void BitstreamReader::Init(uint8_t *pStream, uint32_t len)
{
    m_pSource = pStream;
    m_pEnd = pStream + len;
    m_nBits = 0;
    m_iReadyBits = 0;
}

uint32_t BitstreamReader::CopyBit(void)
{
    if (0 == m_iReadyBits)
        Refresh();

    return ((m_nBits >> (m_iReadyBits - 1)) & 1);
}

uint32_t BitstreamReader::GetBit(void)
{
    if (0 == m_iReadyBits)
        Refresh();

    m_iReadyBits -= 1;
    return ((m_nBits >> m_iReadyBits) & 1);
}

uint32_t BitstreamReader::PeekBits(int32_t iNum)
{
    if (iNum <= 24)
    {
        if (iNum > m_iReadyBits)
            Refresh();

        return ((m_nBits >> m_iReadyBits) & ~(-1 << iNum));
    }
    else
    {
        return (GetBits(iNum - 16) << 16) + GetBits(16);
    }
}

uint32_t BitstreamReader::GetBits(int32_t iNum)
{
    if (iNum <= 24)
    {
        if (iNum > m_iReadyBits)
            Refresh();

        m_iReadyBits -= iNum;
        return ((m_nBits >> m_iReadyBits) & ~(-1 << iNum));
    }
    else
    {
        return (GetBits(iNum - 16) << 16) + GetBits(16);
    }
}

void BitstreamReader::SkipBits(int32_t iNum)
{
    if (iNum <= m_iReadyBits)
    {
        m_iReadyBits -= iNum;
    }
    else
    {
        iNum -= m_iReadyBits;
        m_pSource += iNum >> 3;
        m_iReadyBits = 0;
        Refresh();
        m_iReadyBits -= iNum & 0x7;
    }
}

uint8_t *BitstreamReader::Stream(void)
{
    return m_pSource - ((m_iReadyBits + 7) >> 3);
}

uint32_t BitstreamReader::GetUE(void)
{
    int32_t iZeros;

    // count leading zeros
    iZeros = 0;
    while (0 == CopyBit())
    {
        iZeros += 1;
        GetBit();
    }

    // get value
    return (GetBits(iZeros + 1) - 1);
}

int32_t BitstreamReader::GetSE(void)
{
    int32_t iZeros;
    int32_t iValue;

    // count leading zeros
    iZeros = 0;
    while (0 == CopyBit())
    {
        iZeros += 1;
        GetBit();
    }

    // get value
    iValue = GetBits(iZeros);
    return ((GetBit()) ? (-iValue) : (iValue));
}

void BitstreamReader::Refresh(void)
{
    while (24 > m_iReadyBits)
    {
        m_nBits = (m_nBits << 8) | m_pSource[0];
        m_iReadyBits += 8;
        m_pSource += 1;
    }
}

H264BitstreamReader::H264BitstreamReader(void)
{
    m_iZeroes = 0;
}

H264BitstreamReader::H264BitstreamReader(uint8_t *pStream, uint32_t len)
{
    Init(pStream, len);
}

void H264BitstreamReader::Init(uint8_t *pStream, uint32_t len)
{
    m_iZeroes = 0;
    BitstreamReader::Init(pStream, len);
}

void H264BitstreamReader::Refresh(void)
{
    while (24 > m_iReadyBits)
    {
        if (m_pSource + 1 >= m_pEnd)
            throw UMC_ERR_NOT_ENOUGH_DATA;

        // remove preventing byte
        if (m_iZeroes >= 2 && 0x03 == m_pSource[0] && 0x00 == (m_pSource[1] & 0xfc))
        {
            m_pSource += 1;
            m_iZeroes = 0;
        }

        // count sequential zeroes
        if (0 == m_pSource[0])
            m_iZeroes++;
        else
            m_iZeroes = 0;

        m_nBits = (m_nBits << 8) | m_pSource[0];
        m_iReadyBits += 8;
        m_pSource += 1;
    }
}

uint32_t ConvertTrackType(TrackType type)
{
    switch (type)
    {
    case TRACK_MPEG1V:  return MPEG1_VIDEO;
    case TRACK_MPEG2V:  return MPEG2_VIDEO;
    case TRACK_MPEG4V:  return MPEG4_VIDEO;
    case TRACK_H261:    return H261_VIDEO;
    case TRACK_H263:    return H263_VIDEO;
    case TRACK_H264:    return H264_VIDEO;
    case TRACK_DVSD:    return DIGITAL_VIDEO_SD;
    case TRACK_DVSL:    return DIGITAL_VIDEO_SL;
    case TRACK_DV50:    return DIGITAL_VIDEO_50;
    case TRACK_DVHD:    return DIGITAL_VIDEO_HD;
    case TRACK_WMV:     return WMV_VIDEO;
    case TRACK_MJPEG:   return MJPEG_VIDEO;
    case TRACK_PCM:     return PCM_AUDIO;
    case TRACK_LPCM:    return LPCM_AUDIO;
    case TRACK_AC3:     return AC3_AUDIO;
    case TRACK_AAC:     return AAC_AUDIO;
    case TRACK_MPEGA:   return MPEG1_AUDIO | MPEG2_AUDIO;
    case TRACK_TWINVQ:  return TWINVQ_AUDIO;
    case TRACK_DTS:     return DTS_AUDIO;
    case TRACK_VORBIS:  return VORBIS_AUDIO;
    default:            return UNDEF_STREAM;
    }
}

TrackType ConvertAudioType(uint32_t type)
{
    switch (type)
    {
    case PCM_AUDIO:    return TRACK_PCM;
    case LPCM_AUDIO:   return TRACK_LPCM;
    case AC3_AUDIO:    return TRACK_AC3;
    case AAC_AUDIO:    return TRACK_AAC;
    case MPEG1_AUDIO:  return TRACK_MPEGA;
    case MPEG2_AUDIO:  return TRACK_MPEGA;
    case MP1L1_AUDIO:  return TRACK_MPEGA;
    case MP1L2_AUDIO:  return TRACK_MPEGA;
    case MP1L3_AUDIO:  return TRACK_MPEGA;
    case MP2L1_AUDIO:  return TRACK_MPEGA;
    case MP2L2_AUDIO:  return TRACK_MPEGA;
    case MP2L3_AUDIO:  return TRACK_MPEGA;
    case TWINVQ_AUDIO: return TRACK_TWINVQ;
    case DTS_AUDIO:    return TRACK_DTS;
    case VORBIS_AUDIO: return TRACK_VORBIS;
    default:           return TRACK_UNKNOWN;
    }
}

TrackType ConvertVideoType(uint32_t type)
{
    switch (type)
    {
    case MPEG1_VIDEO:      return TRACK_MPEG1V;
    case MPEG2_VIDEO:      return TRACK_MPEG2V;
    case MPEG4_VIDEO:      return TRACK_MPEG4V;
    case H261_VIDEO:       return TRACK_H261;
    case H263_VIDEO:       return TRACK_H263;
    case H264_VIDEO:       return TRACK_H264;
    case DIGITAL_VIDEO_SD: return TRACK_DVSD;
    case DIGITAL_VIDEO_SL: return TRACK_DVSL;
    case DIGITAL_VIDEO_50: return TRACK_DV50;
    case DIGITAL_VIDEO_HD: return TRACK_DVHD;
    case WMV_VIDEO:        return TRACK_WMV;
    case MJPEG_VIDEO:      return TRACK_MJPEG;
    case VP8_VIDEO:        return TRACK_VP8;
    default:               return TRACK_UNKNOWN;
    }
}

const FrameType H264ParsingCore::SliceType[10] = {
    P_PICTURE, B_PICTURE, I_PICTURE, P_PICTURE, I_PICTURE,
    P_PICTURE, B_PICTURE, I_PICTURE, P_PICTURE, I_PICTURE
};

const uint16_t H264ParsingCore::AspectRatio[17][2] = {
    { 1,  1}, { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, { 24, 11},
    {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99},
    { 4,  3}, { 3,  2}, { 2,  1}
};

const ColorFormat H264ParsingCore::ColorFormats[4] = {
    GRAY, YUV420, YUV422, YUV444
};

H264ParsingCore::H264ParsingCore() :
    m_sps(),
    m_pps(),
    m_prev(),
    m_last(),
    m_synced(false), 
    m_skip(0),
    m_type(NONE_PICTURE),
    m_spsParsed(false)
{
}

void H264ParsingCore::Reset()
{
    uint32_t i;
    for (i = 0; i < MaxNumSps; i++)
        m_sps[i].Reset();
    for (i = 0; i < MaxNumPps; i++)
        m_pps[i].Reset();
    m_prev.Reset();
    m_last.Reset();
    m_synced = false;
    m_skip = 0;
    m_type = NONE_PICTURE;
    m_spsParsed = false;
}

void H264ParsingCore::GetInfo(VideoStreamInfo& info)
{
    const Slice& sh = m_last.IsReady() ? m_last : m_prev;
    if (m_prev.IsReady())
    {
        const Sps& sps = sh.GetPps().GetSps();
        info.stream_type = H264_VIDEO;
        info.profile = sps.profile_idc;
        info.level = sps.level_idc;
        info.clip_info.width = 16 * sps.frame_width_in_mbs;
        info.clip_info.width -= 2 * (sps.frame_cropping_rect_left_offset + sps.frame_cropping_rect_right_offset);
        info.clip_info.height = 16 * sps.frame_height_in_mbs;
        info.clip_info.height -= 2 * (sps.frame_cropping_rect_top_offset + sps.frame_cropping_rect_bottom_offset);
        info.clip_info.height *= (2 - sps.frame_mbs_only_flag);
        info.aspect_ratio_width = sps.sar_width * info.clip_info.width;
        info.aspect_ratio_height = sps.sar_height * info.clip_info.height;
        info.framerate = (0.5 * sps.time_scale) / sps.num_units_in_tick;
        info.interlace_type = sh.field_pic_flag ? sh.bottom_field_flag ?
            INTERLEAVED_BOTTOM_FIELD_FIRST : INTERLEAVED_TOP_FIELD_FIRST : PROGRESSIVE;
        info.color_format = ColorFormats[sps.chroma_format_idc];
        (void)info.bitrate;
    }
}

static
uint8_t FindNalu(MediaData& data)
{
    uint8_t res = 0xff;
    const uint8_t* beg = (uint8_t *)data.GetDataPointer();
    const uint8_t* end = beg + data.GetDataSize();
    const uint8_t* ptr;
    for (ptr = beg; ptr + 3 < end; ++ptr)
    {
        if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1)
        {
            res = (uint8_t) (ptr[3] & 0x1f);
            if (ptr > beg && *(ptr - 1) == 0)
                --ptr;
            break;
        }
    }

    data.MoveDataPointer((int32_t)(ptr - beg));
    return res;
}

static
uint8_t* GetDP(MediaData& data)
{
    return (uint8_t *)data.GetDataPointer();
}

static
uint32_t GetDPDist(MediaData& l, MediaData& r)
{
    uint8_t *pBegin = GetDP(r);
    return (uint32_t)(pBegin - GetDP(l));
}

H264ParsingCore::Result H264ParsingCore::Sync(MediaData& data, bool eos)
{
    m_synced = false;
    m_skip = 0;
    m_prev.Reset();
    m_type = NONE_PICTURE;
    data.SetFrameType(m_type);

    for (uint8_t naluType = FindNalu(data); naluType != 0xff; naluType = FindNalu(data))
    {
        switch (naluType)
        {
        case NALU_SPS:
        case NALU_PPS:
        case NALU_SPS_SUBSET:
            {
                MediaData next = data;
                next.MoveDataPointer(4);
                if (FindNalu(next) == 0xff)
                    return ErrNeedData;

                uint32_t naluSize = GetDPDist(data, next);
                Status umcRes = (naluType == NALU_PPS) ?
                    ParsePps(GetDP(data), naluSize + 4) : ParseSps(GetDP(data), naluSize + 4);

                if (umcRes == UMC_OK)
                {
                    m_spsParsed = (naluType == NALU_SPS);
                    m_skip = naluSize;
                    m_synced = true;
                    m_last.Reset();
                    return Ok;
                }

                data.MoveDataPointer(naluSize);
                break;
            }
        case NALU_SLICE:
        case NALU_IDR_SLICE:
            if (!m_last.IsReady())
            {
                size_t dataSize = data.GetDataSize();
                if (dataSize < BytesForSliceReq && !eos)
                    return ErrNeedData;
                Status umcRes = ParseSh(GetDP(data), (int32_t)dataSize);
                if (umcRes == UMC_ERR_NOT_ENOUGH_DATA)
                {
                    MediaData next = data;
                    next.MoveDataPointer(4);
                    if (FindNalu(next) == 0xff)
                        return ErrNeedData;
                    umcRes = UMC_ERR_INVALID_STREAM;
                }
                if (umcRes == UMC_ERR_INVALID_STREAM)
                {
                    data.MoveDataPointer(4);
                    continue;
                }
            }

            m_type = SliceType[m_last.slice_type];
            m_prev = m_last;
            m_last.Reset();
            m_skip = 4;
            m_synced = true;
            return OkPic;
        case NALU_SEI:
        case NALU_DELIMITER:
            if (m_spsParsed)
            {
                m_skip = 4;
                m_synced = true;
                m_last.Reset();
                return Ok;
            }
        default:
            data.MoveDataPointer(4);
            break;
        }
    }

    return ErrNeedData;
}

bool H264ParsingCore::IsFieldOfOneFrame(const H264ParsingCore::Slice& prev, const H264ParsingCore::Slice& last)
{
#if 0
    if (!pFrame)
        return false;

    if (pFrame && pFrame->GetAU(0)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED
        && pFrame->GetAU(1)->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return false;
#endif

    if ((prev.nal_ref_idc && !last.nal_ref_idc) ||
        (!prev.nal_ref_idc && last.nal_ref_idc))
        return false;

    if (prev.field_pic_flag != last.field_pic_flag)
        return false;

//    if (pSlice1->GetSliceHeader()->frame_num != pSlice2->GetSliceHeader()->frame_num)
  //      return false;

    if (prev.bottom_field_flag == last.bottom_field_flag)
        return false;

    return true;
}

bool H264ParsingCore::IsNewPicture(const H264ParsingCore::Slice& prev, const H264ParsingCore::Slice& last)
{
    // frame_num differs in value
    if (last.frame_num != prev.frame_num)
        return true;

    // pic_parameter_set_id differs in value
    if (last.pic_parameter_set_id != prev.pic_parameter_set_id)
        return true;

    // field_pic_flag differs in value
    if (last.field_pic_flag != prev.field_pic_flag)
        return true;

    // bottom_pic_flag is present in both and differs in value
    if (last.field_pic_flag && prev.field_pic_flag)
        if (last.bottom_field_flag != prev.bottom_field_flag)
            return true;

    // nal_ref_idc differs in value with one of the nal_ref_idc values being equal to 0
    if (last.nal_ref_idc != 0 && prev.nal_ref_idc == 0)
        return true;

    // pic_order_cnt_type is equal to 0 for both and
    // either pic_order_cnt_lsb differs in value, or delta_pic_order_cnt_bottom differs in value
    if (last.GetPps().GetSps().pic_order_cnt_type == 0 && prev.GetPps().GetSps().pic_order_cnt_type == 0)
        if (last.pic_order_cnt_lsb != prev.pic_order_cnt_lsb ||
            last.delta_pic_order_cnt_bottom != prev.delta_pic_order_cnt_bottom)
            return true;

    // pic_order_cnt_type is equal to 1 for both and
    // either delta_pic_order_cnt[0] differs in value, or delta_pic_order_cnt[1] differs in value
    if (last.GetPps().GetSps().pic_order_cnt_type == 1 && prev.GetPps().GetSps().pic_order_cnt_type == 1)
        if (last.delta_pic_order_cnt[0] != prev.delta_pic_order_cnt[0] ||
            last.delta_pic_order_cnt[1] != prev.delta_pic_order_cnt[1])
            return true;

    // nal_unit_type differs in value with one of the nal_unit_type values being equal to 5
    if (!last.idr_flag && prev.idr_flag)
        return true;

    // nal_unit_type is equal to 5 for both and idr_pic_id differs in value
    if (last.idr_flag && prev.idr_flag)
        if (last.idr_pic_id != prev.idr_pic_id)
            return true;

    return false;
}

H264ParsingCore::Result H264ParsingCore::Construct(MediaData& data, bool eos)
{
    if (!m_synced)
        return ErrNeedSync;

    data.SetFrameType(m_type);
    data.MoveDataPointer(m_skip);
    m_skip = 0;

    for (uint8_t naluType = FindNalu(data); naluType != 0xff; naluType = FindNalu(data))
    {
        switch (naluType)
        {
        case NALU_SPS:
        case NALU_PPS:
        case NALU_SPS_SUBSET:
            {
                MediaData next = data;
                next.MoveDataPointer(4);
                if (FindNalu(next) == 0xff)
                    return ErrNeedData;

                uint32_t naluSize = GetDPDist(data, next);
                Status umcRes = (naluType == NALU_PPS) ?
                    ParsePps(GetDP(data), naluSize + 4) : ParseSps(GetDP(data), naluSize + 4);
                if (umcRes != UMC_OK)
                {
                    m_synced = false;
                    return ErrInvalid;
                }

                data.MoveDataPointer(naluSize);
            }

            break;
        case NALU_SEI:
        case NALU_DELIMITER:
            {
                MediaData next = data;
                next.MoveDataPointer(4);
                if (FindNalu(next) == 0xff)
                    return ErrNeedData;

                uint32_t naluSize = GetDPDist(data, next);
                data.MoveDataPointer(naluSize);
            }
            break;
        case NALU_SLICE:
        case NALU_IDR_SLICE:
            {
                size_t dataSize = data.GetDataSize();
                if (dataSize < BytesForSliceReq && !eos)
                    return ErrNeedData;

                Status umcRes = ParseSh(GetDP(data), (int32_t)dataSize);
                if (umcRes == UMC_ERR_NOT_ENOUGH_DATA)
                {
                    MediaData next = data;
                    next.MoveDataPointer(4);
                    if (FindNalu(next) == 0xff)
                        return ErrNeedData;
                    umcRes = UMC_ERR_INVALID_STREAM;
                }

                if (umcRes == UMC_ERR_INVALID_STREAM)
                {
                    m_synced = false;
                    return ErrInvalid;
                }

                if (!m_prev.IsReady())
                {
                    m_type = SliceType[m_last.slice_type];
                    m_prev = m_last;
                    m_last.Reset();
                    m_skip = 4;
                    return OkPic;
                }
                else
                {
                    bool isNewFrame = IsNewPicture(m_prev, m_last);
                    bool isFieldsOfOneFrame = IsFieldOfOneFrame(m_prev, m_last);
                    if (isNewFrame && !isFieldsOfOneFrame)
                    {
                        m_synced = false;
                        data.SetFrameType(m_type);
                        return Ok;
                    }
                    else
                    {
                        m_type = MFX_MAX(m_type, SliceType[m_last.slice_type]);
                        if (m_last.nal_ref_idc == 0)
                            m_prev.nal_ref_idc = 0;
                        if (m_last.idr_flag == 1)
                            m_prev.idr_flag = 1;
                        m_last.Reset();
                        data.MoveDataPointer(4);
                    }
                }
            }
            break;
        default:
            data.MoveDataPointer(4);
            break;
        }
    }

    return ErrNeedData;
}

Status H264ParsingCore::ParseSps(uint8_t *buf, int32_t len)
{
    Sps sps;
    int32_t i, j;
    H264BitstreamReader bs(buf, len);

    try
    {
        while (0 == bs.GetBits(8)); // skip all zeroes and one
        bs.GetBits(8); // skip nal_ref_idc and nal_unit_type

        sps.profile_idc = (uint8_t)bs.GetBits(8);
        bs.GetBits(8); // skip flags
        sps.level_idc = (uint8_t)bs.GetBits(8);

        uint8_t idSps = (uint8_t)bs.GetUE();
        if (idSps >= MaxNumSps)
            return UMC_ERR_INVALID_STREAM;

        if (100 == sps.profile_idc || 110 == sps.profile_idc || 122 == sps.profile_idc || 144 == sps.profile_idc ||
            83 == sps.profile_idc || 86 == sps.profile_idc ||
            128 == sps.profile_idc || 118 == sps.profile_idc)
        {
            sps.chroma_format_idc = (uint8_t)bs.GetUE();
            if (sps.chroma_format_idc > 3)
                return UMC_ERR_INVALID_STREAM;
            if (sps.chroma_format_idc == 3)
                bs.GetBit(); // residue_transform_flag

            uint8_t value = (uint8_t)bs.GetUE(); // bit_depth_luma_minus8
            if (value > 4)
                return UMC_ERR_INVALID_STREAM;

            value = (uint8_t)bs.GetUE(); // bit_depth_chroma_minus8
            if (value > 4)
                return UMC_ERR_INVALID_STREAM;

            bs.GetBit(); // qpprime_y_zero_transform_bypass_flag
            if (bs.GetBit()) // seq_scaling_matrix_present_flag
            {
                for (i = 0; i < 8; i++)
                {
                    if (bs.GetBit()) // presented_flag[i]
                    {
                        // pass scaling_lists
                        uint32_t lastScale = 8;
                        uint32_t nextScale = 8;
                        int32_t maxnum = i < 6 ? 16 : 64;
                        for (j = 0; j < maxnum; j++)
                        {
                            if (0 != nextScale)
                            {
                                int32_t delta_scale = bs.GetSE();
                                if (delta_scale < -128 || delta_scale > 127)
                                    return UMC_ERR_INVALID_STREAM;

                                nextScale = (lastScale + delta_scale + 256) & 0xff;
                            }

                            lastScale = (nextScale == 0) ? lastScale : nextScale;;
                        }
                    }
                }
            }
        }

        sps.log2_max_frame_num = (uint8_t) (bs.GetUE() + 4);
        sps.pic_order_cnt_type = (uint8_t) bs.GetUE();

        if (sps.pic_order_cnt_type == 0)
        {
            sps.log2_max_pic_order_cnt_lsb = (uint8_t) (bs.GetUE() + 4);
            if (sps.log2_max_pic_order_cnt_lsb < 4 || sps.log2_max_pic_order_cnt_lsb > 16)
                return UMC_ERR_INVALID_STREAM;
        }
        else if (sps.pic_order_cnt_type == 1)
        {
            sps.delta_pic_order_always_zero_flag = (uint8_t)bs.GetBit();
            bs.GetSE(); // offset_for_non_ref_pic
            bs.GetSE(); // offset_for_top_to_bottom_field
            uint32_t num_ref_frames_in_pic_order_cnt_cycle = bs.GetUE();

            for (i = 0; i < (int32_t)num_ref_frames_in_pic_order_cnt_cycle; i++)
                bs.GetSE(); // offset_for_ref_frame[i]
        }
        else if (sps.pic_order_cnt_type > 2)
            return UMC_ERR_INVALID_STREAM;

        bs.GetUE(); // num_ref_frames
        bs.GetBit(); // gaps_in_frame_num_value_allowed_flag
        sps.frame_width_in_mbs = (uint8_t) (bs.GetUE() + 1);
        sps.frame_height_in_mbs = (uint8_t) (bs.GetUE() + 1);
        sps.frame_mbs_only_flag = (uint8_t)bs.GetBit();
        if (sps.frame_mbs_only_flag == 0)
            bs.GetBit(); // mb_adaptive_frame_field_flag

        bs.GetBit(); // direct_8x8_inference_flag
        if (bs.GetBit()) // frame_cropping_flag
        {
            sps.frame_cropping_rect_left_offset = (uint8_t)bs.GetUE();
            sps.frame_cropping_rect_right_offset = (uint8_t)bs.GetUE();
            sps.frame_cropping_rect_top_offset = (uint8_t)bs.GetUE();
            sps.frame_cropping_rect_bottom_offset = (uint8_t)bs.GetUE();
        }

        if (bs.GetBit()) // vui_parameters_present_flag
        {
            if (bs.GetBit()) // aspect_ratio_present_flag
            {
                uint8_t aspect_ratio_idc = (uint8_t)bs.GetBits(8);
                if (aspect_ratio_idc == 255)
                { // EXTENDED_SAR
                    sps.sar_width = (uint16_t)bs.GetBits(16);
                    sps.sar_height = (uint16_t)bs.GetBits(16);
                }
                else if (aspect_ratio_idc < 17)
                {
                    sps.sar_width  = AspectRatio[aspect_ratio_idc][0];
                    sps.sar_height = AspectRatio[aspect_ratio_idc][1];
                }
                else
                    return UMC_ERR_INVALID_STREAM;
            }

            if (bs.GetBit()) // overscan_info_present_flag
                bs.GetBit(); // overscan_appropriate_flag

            if (bs.GetBit()) // video_signal_type_present_flag
            {
                bs.GetBits(3); // video_format
                bs.GetBit(); // video_full_range_flag
                if (bs.GetBit()) // colour_description_present_flag
                {
                    bs.GetBits(8); // colour_primaries
                    bs.GetBits(8); // transfer_characteristics
                    bs.GetBits(8); // matrix_coefficients
                }
            }

            if (bs.GetBit()) // chroma_loc_info_present_flag
            {
                bs.GetUE(); // chroma_sample_loc_type_top_field
                bs.GetUE(); // chroma_sample_loc_type_bottom_field
            }

            if (bs.GetBit()) // timing_info_present_flag
            {
                sps.num_units_in_tick = (bs.GetBits(16)) << 16;
                sps.num_units_in_tick += bs.GetBits(16);
                if (sps.num_units_in_tick == 0)
                    return UMC_ERR_INVALID_STREAM;

                sps.time_scale = (bs.GetBits(16)) << 16;
                sps.time_scale += bs.GetBits(16);
                if (sps.time_scale == 0)
                    return UMC_ERR_INVALID_STREAM;
            }
        }

        // if no error validate SPS
        sps.SetReady(true);
        m_sps[idSps] = sps;
    }
    catch (...)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC_OK;
}

Status H264ParsingCore::ParsePps(uint8_t *buf, int32_t len)
{
    Pps pps;
    H264BitstreamReader bs(buf, len);

    try
    {
        while (0 == bs.GetBits(8)); // skip all zeroes and one
        bs.GetBits(8); // skip nal_ref_idc and nal_unit_type

        uint32_t idPps = bs.GetUE();
        if (idPps >= MaxNumPps)
            return UMC_ERR_INVALID_STREAM;

        uint32_t idSps = bs.GetUE();
        if (idSps >= MaxNumSps || !m_sps[idSps].IsReady())
            return UMC_ERR_INVALID_STREAM;
        pps.SetSps(m_sps[idSps]);

        bs.GetBit(); // entropy_coding_mode
        pps.pic_order_present_flag = (uint8_t)bs.GetBit();

        // if no error validate PPS
        pps.SetReady(true);
        m_pps[idPps] = pps;
    }
    catch (...)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC_OK;
}

Status H264ParsingCore::ParseSh(uint8_t *buf, int32_t len)
{
    Slice sh;
    H264BitstreamReader bs(buf, len);

    try
    {
        while (0 == bs.GetBits(8)); // skip all zeros and one
        bs.GetBit(); // skip forbidden zero bit
        sh.nal_ref_idc = (uint8_t)bs.GetBits(2);
        sh.idr_flag = (5 == bs.GetBits(5));

        bs.GetUE(); // first_mb_in_slice
        sh.slice_type = (uint8_t)bs.GetUE();
        if (sh.slice_type > 9)
            return UMC_ERR_INVALID_STREAM;

        uint32_t idPps = bs.GetUE();
        if (idPps >= MaxNumPps || !m_pps[idPps].IsReady())
            return UMC_ERR_INVALID_STREAM;
        sh.SetPps(m_pps[idPps]);

        sh.frame_num = bs.GetBits(sh.GetPps().GetSps().log2_max_frame_num);
        if (sh.GetPps().GetSps().frame_mbs_only_flag == 0)
        {
            sh.field_pic_flag = (uint8_t)bs.GetBit();
            if (sh.field_pic_flag)
                sh.bottom_field_flag = (uint8_t)bs.GetBit();
        }

        if (sh.idr_flag)
            sh.idr_pic_id = bs.GetUE();

        if (sh.GetPps().GetSps().pic_order_cnt_type == 0)
        {
            sh.pic_order_cnt_lsb = bs.GetBits(sh.GetPps().GetSps().log2_max_pic_order_cnt_lsb);
            if (sh.GetPps().pic_order_present_flag && !sh.field_pic_flag)
                sh.delta_pic_order_cnt_bottom = bs.GetSE();
        }

        if (sh.GetPps().GetSps().pic_order_cnt_type == 1 && sh.GetPps().GetSps().delta_pic_order_always_zero_flag == 0)
        {
            sh.delta_pic_order_cnt[0] = bs.GetSE();
            if (sh.GetPps().pic_order_present_flag && !sh.field_pic_flag)
                sh.delta_pic_order_cnt[1] = bs.GetSE();
        }

        // if no error validate slice
        sh.SetReady(true);
        m_last = sh;
    }
    catch (...)
    {
        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    return UMC_OK;
}

void H264ParsingCore::Sps::Reset()
{
    SetReady(false);
    time_scale = 30;
    num_units_in_tick = 1;
    sar_width = 1;
    sar_height = 1;
    profile_idc = 0;
    level_idc = 0;
    chroma_format_idc = 1;
    log2_max_pic_order_cnt_lsb = 0;
    log2_max_frame_num = 0;
    frame_mbs_only_flag = 0;
    pic_order_cnt_type = 0;
    delta_pic_order_always_zero_flag = 0;
    frame_width_in_mbs = 0;
    frame_height_in_mbs = 0;
    frame_cropping_rect_left_offset = 0;
    frame_cropping_rect_right_offset = 0;
    frame_cropping_rect_top_offset = 0;
    frame_cropping_rect_bottom_offset = 0;
}

void H264ParsingCore::Pps::Reset()
{
    SetReady(false);
    pic_order_present_flag = 0;
    m_sps = 0;
}

void H264ParsingCore::Slice::Reset()
{
    SetReady(false);
    pic_parameter_set_id = 0;
    field_pic_flag = 0;
    bottom_field_flag = 0;
    frame_num = 0;
    slice_type = 0;
    pic_order_cnt_lsb = 0;
    delta_pic_order_cnt_bottom = 0;
    delta_pic_order_cnt[0] = 0;
    delta_pic_order_cnt[1] = 0;
    nal_ref_idc = 0;
    idr_flag = 0;
    idr_pic_id = 0;
    m_pps = 0;
}

H264FrameConstructor::H264FrameConstructor()
: VideoFrameConstructor(), m_core()
{
}

Status H264FrameConstructor::Init(MediaReceiverParams *pInit)
{
    m_core.Reset();
    return VideoFrameConstructor::Init(pInit);
}

Status H264FrameConstructor::Reset()
{
    m_core.Reset();
    return VideoFrameConstructor::Reset();
}

Status H264FrameConstructor::SoftReset()
{
    MediaData data;
    m_core.Sync(data); // re-sync
    return VideoFrameConstructor::SoftReset();
}

static
uint8_t* Find001(uint8_t* beg, uint8_t* end)
{
    for (uint8_t* ptr = beg; ptr + 3 < end; ++ptr)
        if (ptr[0] == 0 && ptr[1] == 0 && ptr[2] == 1)
            return ptr;
    return end;
}

static
Status PopulateDecSpecInfoH264(Mpeg2TrackInfo& info, uint8_t* beg, uint8_t* end)
{
    // info already exists
    if (info.m_pDecSpecInfo)
        return UMC_ERR_FAILED;

    uint8_t* pStart = 0;
    uint8_t* pEnd = 0;

    uint8_t* naluBeg = Find001(beg, end);
    if (naluBeg == end)
        return UMC_ERR_NOT_ENOUGH_DATA;

    struct HeadersFoundFlag
    {
        bool isSPS;
        bool isPPS;
        bool isSubSPS;

        HeadersFoundFlag()
            : isSPS(false)
            , isPPS(false)
            , isSubSPS(false)
        {
        }
    };

    HeadersFoundFlag flags;

    uint8_t* naluEnd;
    for (naluEnd = Find001(naluBeg + 1, end); naluEnd != end; naluEnd = Find001(naluBeg + 1, end))
    {
        uint32_t startCode = (naluBeg[3] & 0x1f);

        bool quite = false;

        switch (startCode)
        {
        case NALU_SEI:
        case NALU_SPS:
            if (!pStart)
                pStart = naluBeg;
            flags.isSPS = flags.isSPS ? true : (startCode == NALU_SPS);
            break;

        case NALU_PPS:
            flags.isPPS = true;
            break;

        case NALU_SPS_SUBSET:
            flags.isSubSPS = true;
            break;

        default:
        case NALU_SLICE_EXTENSION:
        case NALU_IDR_SLICE:
        case NALU_SLICE:
            if (pStart)
                quite = true;
            break;
        }

        if (quite)
            break;

        pEnd = naluEnd;
        naluBeg = naluEnd;
    }

    if (naluEnd == end && (!flags.isPPS || !flags.isSPS))
        return UMC_ERR_NOT_ENOUGH_DATA;

    if (!pStart || !flags.isPPS) // for stupid klockwork
        return UMC_ERR_NOT_ENOUGH_DATA;

    size_t size = pEnd - pStart;
    info.m_pDecSpecInfo = new MediaData(size);
    info.m_pDecSpecInfo->SetDataSize(size);
    uint8_t* pDsi = (uint8_t *)info.m_pDecSpecInfo->GetDataPointer();
    MFX_INTERNAL_CPY(pDsi, pStart, (uint32_t)size);
    return UMC_OK;
}

static
bool IsSampleComplyWithTmPolicyH264(FrameType type, double dRate)
{
    return ((I_PICTURE == type) || (0.0 <= dRate && dRate <= 1.0));
}

void H264FrameConstructor::InternBuf2MediaData(MediaData& data)
{
    data.SetBufferPointer(m_pBuf + m_lCurPos, m_lLastBytePos - m_lCurPos);
    data.SetDataSize(m_lLastBytePos - m_lCurPos);
}

void H264FrameConstructor::MediaData2InternBuf(MediaData& data)
{
    m_lCurPos = (int32_t)((uint8_t *)data.GetDataPointer() - m_pBuf);
}

Status H264FrameConstructor::GetFrame(SplMediaData *frame)
{
    MediaData data;
    InternBuf2MediaData(data);

    for (;;)
    {
        H264ParsingCore::Result res = m_core.Construct(data, m_bEndOfStream);
        MediaData2InternBuf(data);

        switch (res)
        {
        case H264ParsingCore::ErrNeedSync:
            {
                H264ParsingCore::Result syncRes = m_core.Sync(data, m_bEndOfStream);
                MediaData2InternBuf(data);
                if (syncRes == H264ParsingCore::ErrNeedData)
                    return m_bEndOfStream ? UMC_ERR_END_OF_STREAM :
                        m_CurFrame.iBufOffset < 0 ? UMC_ERR_SYNC : UMC_ERR_NOT_ENOUGH_DATA;

                if (m_CurFrame.iBufOffset < 0)
                {
                    m_CurFrame.iBufOffset = m_lCurPos;
                    AssignAbsPos(m_lCurPos);
                }
                if (syncRes == H264ParsingCore::OkPic)
                {
                    m_lCurPicStart = m_lCurPos;
                    AssignTimeStamps(m_lCurPos);
                }
            }
            break;

        case H264ParsingCore::OkPic:
            m_lCurPicStart = m_lCurPos;
            AssignTimeStamps(m_lCurPos);
            if (!m_bInfoFilled)
            {
                m_core.GetInfo(*((VideoStreamInfo *)m_pInfo->m_pStreamInfo));
                PopulateDecSpecInfoH264(
                    *m_pInfo,
                    m_pBuf + m_CurFrame.iBufOffset,
                    m_pBuf + m_lCurPos + 4);
                m_bInfoFilled = true;
            }
            break;

        case H264ParsingCore::ErrNeedData:
            if (m_bEndOfStream)
            {
                if (m_lCurPicStart >= 0)
                {
                    memset(m_pBuf + m_lLastBytePos, 0, 4); // four zeros for robustness
                    m_lLastBytePos += 4;
                    m_lCurPos = m_lLastBytePos;
                    res = H264ParsingCore::Ok; // output last frame
                }
                else
                    return UMC_ERR_END_OF_STREAM;
            }
            else
                return UMC_ERR_NOT_ENOUGH_DATA;

        case H264ParsingCore::ErrInvalid:
        case H264ParsingCore::Ok:
            if (!IsSampleComplyWithTmPolicyH264(data.GetFrameType(), m_dRate))
            { // if playback rate isn't normal, only I_PICTUREs are allowed
                if (m_lCurPicStart >= 0)
                {
                    CutInterval(m_PrevSample, m_LastSample, m_pBuf, m_lCurPicStart, m_lCurPos, m_lLastBytePos);
                    m_lLastBytePos -= m_lCurPos - m_lCurPicStart;
                    m_lCurPos = m_lCurPicStart;
                    InternBuf2MediaData(data);
                    m_lCurPicStart = -1;
                    m_CurFrame.dPTS = m_CurFrame.dDTS = -1.0;
                }
            }
            else
            {
                m_CurFrame.uiSize = m_lCurPos - m_CurFrame.iBufOffset;
                m_CurFrame.SetFrameType(data.GetFrameType());

                m_CurFrame.CopyTo(frame[0], m_pBuf);
                m_CurFrame.Reset();
                m_CurFrame.iBufOffset = -1;
                m_lCurPicStart = -1;

                if (m_dRate < 0.0)
                {
                    m_core.Reset();
                    m_PrevSample.Reset();
                    m_LastSample.Reset();
                    m_lLastBytePos = m_lCurPos;
                }

                return UMC_OK;
            }
            break;
        }
    }
}

Status MJPEGFrameConstructor::GetFrame(SplMediaData *frame)
{
    //TODO: unsupported mode
    if (m_dRate < 0.0)
    {
        return UMC_ERR_UNSUPPORTED;
    }
    uint8_t * pDataStart = m_pBuf + m_lCurPos;
    uint32_t  nData = m_lLastBytePos - m_lCurPos;
    uint8_t * pDataEnd = pDataStart + nData;
    uint8_t * pData;

    for (pData = pDataStart + m_lParserStartPos; pData + 2 <= pDataEnd; pData++)
    {
        uint16_t shortCode = *(uint16_t*)pData ;

        if(m_lNumBytesToSkip)
        {
            m_lNumBytesToSkip--;
            continue;
        }

        if((JPEG_APP1 == shortCode || JPEG_APP13 == shortCode) && m_state == StateWaitEoi)
        {
            m_state = StateWaitMarkerLength;
            m_lNumBytesToSkip = 1;
            continue;
        }

        if(m_state == StateWaitMarkerLength)
        {
            m_lNumBytesToSkip = SWAP_SHORT(shortCode) - 1;
            m_state = StateWaitEoi;
            continue;
        }

        if (JPEG_SOI == shortCode)
        {
            m_lCurPos = (int32_t)(pData - m_pBuf);
            m_state = StateWaitEoi;
            m_CurFrame.iBufOffset = m_lCurPos;
            m_lParserStartPos = 0;
            pDataStart = pData;
            continue;
        }
        if (shortCode != JPEG_EOI)
            continue;
        
        switch(m_state)
        {
            case StateWaitSoi : break;
            case StateWaitEoi :
            {
                m_CurFrame.uiSize = (uint32_t)(pData - pDataStart) + 2;
                m_CurFrame.SetFrameType(I_PICTURE);
                m_CurFrame.CopyTo(frame[0], m_pBuf);

                m_lCurPos += m_CurFrame.uiSize;
                m_CurFrame.Reset();
                m_CurFrame.iBufOffset = m_lCurPos;
                m_state = StateWaitSoi;
                m_lCurPicStart = -1;
                m_lParserStartPos = 0;

                return UMC_OK;
            }
            case StateWaitMarkerLength : break;
        }
    }
    switch(m_state)
    {
        case StateWaitSoi : break;
        case StateWaitMarkerLength:
        case StateWaitEoi : {
            m_lParserStartPos = (int32_t)(pData - pDataStart);
            break;
        }
    }
    
    return UMC_ERR_NOT_ENOUGH_DATA;
    
}
MJPEGFrameConstructor :: MJPEGFrameConstructor()
    : m_state (StateWaitSoi)
    , m_lParserStartPos()
    , m_lNumBytesToSkip() {
}
Status MJPEGFrameConstructor::Init(MediaReceiverParams *pInit) 
{
    return VideoFrameConstructor::Init(pInit);
}
Status MJPEGFrameConstructor::Reset() 
{
    m_state = StateWaitSoi;
    return VideoFrameConstructor::Reset();
}
Status MJPEGFrameConstructor::SoftReset() 
{
    m_state = StateWaitSoi;
    return VideoFrameConstructor::SoftReset();
}


} // namespace UMC