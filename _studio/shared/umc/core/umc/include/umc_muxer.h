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

#ifndef __UMC_MUXER_H__
#define __UMC_MUXER_H__

#include "umc_media_buffer.h"

namespace UMC
{

typedef enum
{
  UNDEF_TRACK = 0,
  AUDIO_TRACK = 1,
  VIDEO_TRACK = 2,
  VBI_TRACK   = 4
} MuxerTrackType;

class TrackParams
{
public:
  TrackParams(){
    type = UNDEF_TRACK;
    info.undef = NULL;
    info.audio = NULL;
    info.video = NULL;
  }
public:
  MuxerTrackType type;
  union {
    AudioStreamInfo *audio;
    VideoStreamInfo *video;
    void            *undef;
  } info;
  MediaBufferParams bufferParams;

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    TrackParams(const TrackParams &);
    TrackParams & operator = (const TrackParams &);
};

enum MuxerFlags
{
  FLAG_MUXER_ENABLE_THREADING     = 0x00000010,
  FLAG_DATA_FROM_SPLITTER         = 0x00000020,
  FLAG_FRAGMENTED_AT_I_PICTURES   = 0x00000040, // for mpeg4 muxer
  FLAG_FRAGMENTED_BY_HEADER_SIZE  = 0x00000080, // for mpeg4 muxer
  FLAG_START_WITH_HEADER          = 0x00000100  // for mpeg4 muxer
};

class DataWriter; // forward declaration

class MuxerParams
{
    DYNAMIC_CAST_DECL_BASE(MuxerParams)

public:
    // Default constructor
    MuxerParams();
    // Destructor
    virtual ~MuxerParams();
    // operator=
    MuxerParams & operator = (const MuxerParams & p);

    SystemStreamType m_SystemType;      // type of media stream
    int32_t      m_lFlags;               // muxer flag(s)
    int32_t      m_nNumberOfTracks;      // number of tracks
    TrackParams *pTrackParams;          // track parameters
    DataWriter  *m_lpDataWriter;        // pointer to data writer

protected:
    void Close();

private:
    // Declare private copy constructor to avoid accidental assignment
    // and klocwork complaining.
    MuxerParams(const MuxerParams &);
};

/******************************************************************************/

class Muxer
{
    DYNAMIC_CAST_DECL_BASE(Muxer)

public:
    // Default constructor
    Muxer();

    // Destructor
    virtual ~Muxer();

    // Flushes buffers and release all resources
    virtual Status Close(void);

    // Initialize muxer
    virtual Status Init(MuxerParams *lpInit) = 0;

    // Locks input buffer
    virtual Status LockBuffer(MediaData *lpData, int32_t iTrack);

    // Unlocks input buffer
    virtual Status UnlockBuffer(MediaData *lpData, int32_t iTrack);

    // Try to lock input buffer, copies user data into it and unlocks
    virtual Status PutData(MediaData *lpData, int32_t iTrack);

    // Deliver EOS
    virtual Status PutEndOfStream(int32_t iTrack);

    // Copy video sample to input buffer
    virtual Status PutVideoData(MediaData *lpData, int32_t iVideoIndex = 0);

    // Copy audio sample to input buffer
    virtual Status PutAudioData(MediaData *lpData, int32_t iAudioIndex = 0);

    // Flushes all data from buffers to output stream
    virtual Status Flush(void) = 0;

    // Get index of track of specified type
    virtual int32_t GetTrackIndex(MuxerTrackType type, int32_t index = 0);

protected:
    // Copy input parameters to m_pParams, m_uiTotalNumStreams, m_pTrackParams
    // and alloc m_ppBuffers array
    virtual Status CopyMuxerParams(MuxerParams *lpInit);

    // Provides time of first output sample
    // Returns UMC_ERR_NOT_ENOUGH_DATA when buffer is empty
    // Returns UMC_ERR_END_OF_STREAM when buffer is empty and EOS was received
    virtual Status GetOutputTime(int32_t iTrack, double &dTime) = 0;

    // Find stream with minimum time of first output sample
    // In flush mode it disregards empty buffers (if at least one buffer is not empty)
    // In non-flush mode it returns UMC_ERR_NOT_ENOUGH_DATA if one of buffer is empty
    Status GetStreamToWrite(int32_t &rStreamNumber, bool bFlushMode);

protected:
    MuxerParams  *m_pParams; // pointer to params
    uint32_t        m_uiTotalNumStreams; // number of tracks
    TrackParams  *m_pTrackParams; // pointer to track params [m_uiTotalNumStreams]
    MediaBuffer **m_ppBuffers; // array of pointers [m_uiTotalNumStreams] to media buffers
};

} // end namespace UMC

#endif // __UMC_MUXER_H__