// Copyright (c) 2011-2018 Intel Corporation
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

#ifndef _MFX_MFE_ADAPTER_DXVA_
#define _MFX_MFE_ADAPTER_DXVA_

#if defined(MFX_VA_WIN) && defined(MFX_ENABLE_MFE)
#include <vector>
#include <list>
#include <condition_variable>
#include <mfxstructures.h>
#include "encoding_ddi.h"
#include "hevce_ddi_main.h"
class MFEDXVAEncoder
{
    typedef void* CAPS;
    struct m_stream_ids_t
    {
        ENCODE_MULTISTREAM_INFO info;
        mfxStatus sts;
        bool      interlace;
        mfxU8     fieldNum;
        bool      isSubmitted;
        mfxU32    feedbackSize;
        m_stream_ids_t( ENCODE_MULTISTREAM_INFO _info,
                        mfxStatus _sts,
                        bool fields):
        info(_info),
        sts(_sts),
        interlace(fields),
        fieldNum(0),
        isSubmitted(false),
        feedbackSize(0)
        {
        };
        inline void reset()
        {
            sts = MFX_ERR_NONE;
            fieldNum = 0;
            isSubmitted = false;
        };
        inline void resetField()
        {
            isSubmitted = false;
        };
        inline void fieldSubmitted()
        {
            fieldNum++;
            isSubmitted = true;
        };
        inline bool isFieldSubmitted()
        {
            return (fieldNum!=0 && isSubmitted);
        };
        inline bool isFrameSubmitted()
        {
            return isSubmitted && ((interlace && fieldNum == 2) || (fieldNum==1 && !interlace));
        };
    };

public:
    MFEDXVAEncoder();

    virtual
        ~MFEDXVAEncoder();
    mfxStatus Create(ID3D11VideoDevice *pVideoDevice,
                     ID3D11VideoContext *pVideoContext);

    mfxStatus Join(mfxExtMultiFrameParam const & par,
                   ENCODE_MULTISTREAM_INFO &info,
                   bool doubleField);
    mfxStatus Disjoin(ENCODE_MULTISTREAM_INFO info);
    mfxStatus Destroy();
    //MSFT runtime restrict multiple contexts per device
    //so for DXVA MFE implementation the same context being used for encoder and MFE submission
    ID3D11VideoDecoder* GetVideoDecoder();
    mfxStatus Submit(ENCODE_MULTISTREAM_INFO info, long long timeToWait, bool skipFrame);//time passed in microseconds
    //returns pointer to particular caps with only read access, NULL if caps not set.
    CAPS GetCaps(MFE_CODEC codecId);
//placeholder
#ifdef MFX_ENABLE_AV1_VIDEO_ENCODE
    ENCODE_CAPS_AV1 GetCaps() { return m_pAv1CAPS; };
#endif
    virtual void AddRef();
    virtual void Release();

private:
    mfxU32      m_refCounter;

    std::condition_variable     m_mfe_wait;
    std::mutex                  m_mfe_guard;

    ID3D11VideoDevice*  m_pVideoDevice;
    ID3D11VideoContext* m_pVideoContext;
    ID3D11VideoDecoder* m_pMfeContext;

    // a pool (heap) of objects
    std::list<m_stream_ids_t> m_streams_pool;

    // a pool (heap) of objects
    std::list<m_stream_ids_t> m_submitted_pool;

    // a list of objects filled with context info ready to submit
    std::list<m_stream_ids_t> m_toSubmit;

    typedef std::list<m_stream_ids_t>::iterator StreamsIter_t;

    //A number of frames which will be submitted together (Combined)
    mfxU32 m_framesToCombine;

    //A desired number of frames which might be submitted. if
    // actual number of sessions less then it,  m_framesToCombine
    // will be adjusted
    mfxU32 m_maxFramesToCombine;

    // A counter frames collected for the next submission. These
    // frames will be submitted together either when get equal to
    // m_pipelineStreams or when collection timeout elapses.
    mfxU32 m_framesCollected;

    //To save caps for different codecs and reuse - no need in cycling query each time
    ENCODE_CAPS *m_pAvcCAPS;
    ENCODE_CAPS_HEVC *m_pHevcCAPS;
#ifdef MFX_ENABLE_AV1_VIDEO_ENCODE
    ENCODE_CAPS_AV1 *m_pAv1CAPS;
#endif
    // We need contexts extracted from m_toSubmit to
    // place to a linear vector to pass them to vaMFSubmit
    std::vector<ENCODE_COMPBUFFERDESC> m_contexts;
    // store iterators to particular items
    std::vector<StreamsIter_t> m_streams;
    // store iterators to particular items
    std::map<mfxU32, StreamsIter_t> m_streamsMap;
    //time frequency for conversion to us/ms
    long long m_time_frequency;

    // currently up-to-to 3 frames worth combining
    static const mfxU32 MAX_FRAMES_TO_COMBINE = 3;
};
#endif // MFX_VA_WIN && MFX_ENABLE_MFE

#endif /* _MFX_MFE_ADAPTER_DXVA */
