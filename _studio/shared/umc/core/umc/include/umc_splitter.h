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

#ifndef __UMC_SPLITTER_H__
#define __UMC_SPLITTER_H__

#include "umc_structures.h"
#include "umc_data_reader.h"
#include "umc_dynamic_cast.h"
#include "umc_memory_allocator.h"
#include "umc_media_data.h"

namespace UMC
{

class SplitterParams
{
    DYNAMIC_CAST_DECL_BASE(SplitterParams)

public:
    // Default constructor
    SplitterParams();
    // Destructor
    virtual ~SplitterParams();

    uint32_t m_lFlags;                                       // (uint32_t) splitter's flags
    DataReader *m_pDataReader;                             // (DataReader *) pointer to data reader
    uint32_t m_uiSelectedVideoPID;                           // ID for video stream chosen by user
    uint32_t m_uiSelectedAudioPID;                           // ID for audio stream chosen by user
    MemoryAllocator *m_pMemoryAllocator;                   // (MemoryAllocator *) pointer to memory allocator object
};

enum TrackType
{
    /* video types 0x0000XXXX */
    TRACK_MPEG1V                = 0x00000001,
    TRACK_MPEG2V                = 0x00000002,
    TRACK_MPEG4V                = 0x00000004,
    TRACK_H261                  = 0x00000008,
    TRACK_H263                  = 0x00000010,
    TRACK_H264                  = 0x00000020,
    TRACK_DVSD                  = 0x00000040,
    TRACK_DV50                  = 0x00000080,
    TRACK_DVHD                  = 0x00000100,
    TRACK_DVSL                  = 0x00000200,
    TRACK_VC1                   = 0x00000400,
    TRACK_WMV                   = 0x00000800,
    TRACK_MJPEG                 = 0x00001000,
    TRACK_YUV                   = 0x00002000,
    TRACK_AVS                   = 0x00004000,
    TRACK_VP8                   = 0x00004000,
    TRACK_H265                  = 0x00008000,
    TRACK_ANY_VIDEO             = 0x0000FFFF,

    /* audio typTRACK 0x0XXX0000 */
    TRACK_PCM                   = 0x00010000,
    TRACK_LPCM                  = 0x00020000,
    TRACK_AC3                   = 0x00040000,
    TRACK_AAC                   = 0x00080000,
    TRACK_MPEGA                 = 0x00100000,
    TRACK_TWINVQ                = 0x00200000,
    TRACK_DTS                   = 0x00400000,
    TRACK_VORBIS                = 0x00800000,
    TRACK_AMR                   = 0x01000000,
    TRACK_WMA                   = 0x02000000,
    TRACK_ANY_AUDIO             = 0x0FFF0000,

    TRACK_SUB_PIC               = 0x10000000,
    TRACK_DVD_NAV               = 0x20000000,
    TRACK_ANY_DVD               = 0x30000000,

    TRACK_VBI_TXT               = 0x40000000,
    TRACK_VBI_SPEC              = 0x80000000,
    TRACK_ANY_VBI               = 0xC0000000,

    TRACK_ANY_SPECIAL           = 0xF0000000,

    TRACK_UNKNOWN               = 0x00000000
};

struct TrackInfo
{
    DYNAMIC_CAST_DECL_BASE(TrackInfo)

    TrackInfo()
    {
        m_Type = TRACK_UNKNOWN;
        m_PID = 0;
        m_isSelected = 0;
        m_pDecSpecInfo = NULL;
        m_pStreamInfo = NULL;
    }
    virtual ~TrackInfo() {}

    TrackType     m_Type;                 // common type (all audio/video/other in one enum)
    uint32_t        m_PID;                  //
    int32_t        m_isSelected;           // if Track is on or off
    MediaData    *m_pDecSpecInfo;         // Keeps DecSpecInfo and its length
    StreamInfo   *m_pStreamInfo;          // Base for AudioStreamInfo, VideoStreamInfo, etc
};

class SplitterInfo
{
    DYNAMIC_CAST_DECL_BASE(SplitterInfo)

public:
    // Default constructor
    SplitterInfo();
    // Destructor
    virtual ~SplitterInfo();

    // common fields
    uint32_t              m_splitter_flags;
    SystemStreamType    m_SystemType;       // system type (MPEG4, MPEG2, AVI, pure)
    uint32_t              m_nOfTracks;        // number of tracks detected
    double              m_dRate;            // current playback rate
    double              m_dDuration;        // duration of stream
    TrackInfo         **m_ppTrackInfo;      // array of pointers to TrackInfo(s)
    /******************* below is DEPRECATED fields ***********************/
    void DeprecatedSplitterInfo(); //DEPRECATED!!!

    AudioStreamInfo m_audio_info;       // DEPRECATED!!! (AudioStreamInfo) audio track 0 info
    VideoStreamInfo m_video_info;       // DEPRECATED!!! (VideoStreamInfo) video track 0 info
    SystemStreamInfo m_system_info;     // DEPRECATED!!! (SystemStreamInfo) media stream info

    // memory for auxilary tracks will be allocated inside
    // GetInfo method user should free it then
    AudioStreamInfo *m_audio_info_aux;  // DEPRECATED!!! auxilary audio tracks 1..
    VideoStreamInfo *m_video_info_aux;  // DEPRECATED!!! auxilary video tracks 1..

    int32_t number_audio_tracks;         // DEPRECATED!!!  (int32_t) number of available audio tracks
    int32_t number_video_tracks;         // DEPRECATED!!!  (int32_t) number of available video tracks
};

/*
//  Class:       Splitter
//
//  Notes:       Base abstract class of splitter. Class describes
//               the high level interface of abstract splitter of media stream.
//               All specific ( avi, mpeg2, mpeg4 etc ) must be implemented in
//               derevied classes.
//               Splitter uses this class to obtain data
//
*/
class Splitter
{
    DYNAMIC_CAST_DECL_BASE(Splitter)

public:
    // constructor
    Splitter();

    // decstructor
    virtual ~Splitter() {}

    // Get media data type
    static SystemStreamType GetStreamType(DataReader *dr);

    // Initialize splitter
    virtual Status Init(SplitterParams& rInit) = 0;

    // Close splitter and free all resources
    virtual Status Close() = 0;

    // Get next data, unlocks previously returned
    virtual Status GetNextData(MediaData* /*data*/, uint32_t /*nTrack*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Get next data without moving DataReader
    virtual Status CheckNextData(MediaData* /*data*/, uint32_t /*nTrack*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Set time position
    virtual Status SetTimePosition(double /*timePos*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Get time position
    virtual Status GetTimePosition(double& timePos)
        {timePos = 0; return UMC_ERR_NOT_IMPLEMENTED;}

    // Get splitter info
    virtual Status GetInfo(SplitterInfo** /*ppInfo*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Set playback rate
    virtual Status SetRate(double /*rate*/)
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // changes state of track
    // iState = 0 means disable, iState = 1 means enable
    virtual Status EnableTrack(uint32_t /*nTrack*/, int32_t /*iState*/)
    {
        return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Runs reading threads
    virtual Status Run()
    {
      return UMC_ERR_NOT_IMPLEMENTED;
    }

    // Stops reading threads
    virtual Status Stop() = 0;

protected:

    DataReader  *m_pDataReader;  // (DataReader *) pointer to data reader
    SplitterInfo m_info;      // (SplitterInfo *) splitter info

    /******************* below is DEPRECATED methods ***********************/
public:
    void DeprecatedSplitter();
    //  DEPRECATED!!! Get next video data from track
    virtual Status GetNextVideoData(MediaData* data, uint32_t track_idx);
    //  DEPRECATED!!! Get next audio data from track
    virtual Status GetNextAudioData(MediaData* data, uint32_t track_idx);
    //  DEPRECATED!!! Get next video data
    virtual Status GetNextVideoData(MediaData* data);
    //  DEPRECATED!!! Get next audio data
    virtual Status GetNextAudioData(MediaData* data);
    //  DEPRECATED!!! Get next video data
    virtual Status CheckNextVideoData(MediaData* data,uint32_t track_idx=0);
    //  DEPRECATED!!! Get next audio data
    virtual Status CheckNextAudioData(MediaData* data,uint32_t track_idx=0);
    //  DEPRECATED!!! Set position
    virtual Status SetPosition(double) { return UMC_OK; }
    //  DEPRECATED!!! Get position
    virtual Status GetPosition(double) { return UMC_OK; }
    //  DEPRECATED!!! Get splitter info
    virtual Status GetInfo(SplitterInfo* pInfo);
    //  DEPRECATED!!!
    virtual Status PrepareForRePosition() { return UMC_OK; }

protected:
    Status GetBaseNextData(MediaData* data, uint32_t nTrack, bool bCheck);  // !!! TEMPORAL
    uint8_t pFirstFrame[32];  // !!! TEMPORAL
    uint8_t pAudioTrTbl[32];
    uint8_t pVideoTrTbl[32];
    SplitterInfo *pNewSplInfo;
};

} // namespace UMC

#endif /* __UMC_SPLITTER_H__ */
