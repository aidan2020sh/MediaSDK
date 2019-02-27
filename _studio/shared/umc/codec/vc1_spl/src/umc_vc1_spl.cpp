// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_SPLITTER)
#include "ipps.h"

#include "umc_vc1_spl.h"
#include "umc_vc1_spl_defs.h"
#include "umc_vc1_common.h"
#include "umc_vc1_common_defs.h"



namespace UMC
{
    using namespace UMC::VC1Common;

    Splitter *CreateVC1Splitter() { return (new VC1Splitter()); }

    VC1Splitter::VC1Splitter()
    {
        m_frame_constructor = NULL;
        m_pDataReader = NULL;

        m_seqHeaderFlag = 0;
        m_NextFrame = 0;

        m_frameBuf = NULL;
        m_frameBufSize = 1024;
        m_frameSize = 0;

        m_readBuf = NULL;
        m_readBufSize = 1024;
        m_readDataSize = 0;

        m_readData = NULL;
        m_stCodes = NULL;

        m_bNotDelete = false;
    }

    VC1Splitter::~VC1Splitter()
    {
        Close();
    }

    Status VC1Splitter::Init(SplitterParams& rInit)
    {
        UMC::Status umcSts = UMC_OK;
        MediaDataEx mediaData;
        Ipp32u splMode = 1;
        Ipp32u temp_val;

        Close();

        m_pDataReader = rInit.m_pDataReader;
        if(!m_pDataReader)
        {
            Close();
            return UMC_ERR_NULL_PTR;
        }

        m_info.m_splitter_flags = rInit.m_lFlags & (~AUDIO_SPLITTER);
        m_info.m_SystemType = VC1_PURE_VIDEO_STREAM;
        m_info.m_nOfTracks = 1;
        m_info.m_ppTrackInfo = new TrackInfo*[1];
        if(m_info.m_ppTrackInfo == NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }

        m_info.m_ppTrackInfo[0] = new TrackInfo;
        if(m_info.m_ppTrackInfo[0] == NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }

        m_info.m_ppTrackInfo[0]->m_Type = TRACK_VC1;
        m_info.m_ppTrackInfo[0]->m_isSelected = 1;
        m_info.m_ppTrackInfo[0]->m_pStreamInfo = new VideoStreamInfo;
        if(m_info.m_ppTrackInfo[0]->m_pStreamInfo == NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }

        m_readData = new MediaData();
        if(m_readData == NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }

        m_frameBufSize = 1024;
        m_frameBuf = (Ipp8u*)ippsMalloc_8u((Ipp32s)(m_frameBufSize*sizeof(Ipp8u)));
        if(m_frameBuf==NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }
        memset(m_frameBuf, 0,m_frameBufSize);

        m_readBufSize = 1024;
        m_readBuf = (Ipp8u*)ippsMalloc_8u((Ipp32s)(m_readBufSize*sizeof(Ipp8u)));
        if(m_readBuf==NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }
        memset(m_readBuf, 0, m_readBufSize);

        //for slice, field start code
        m_stCodes = (MediaDataEx::_MediaDataEx *)ippsMalloc_8u(START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx));
        if(m_stCodes == NULL)
        {
            Close();
            return UMC_ERR_ALLOC;
        }
        memset(m_stCodes, 0, (START_CODE_NUMBER*2*sizeof(Ipp32s)+sizeof(MediaDataEx::_MediaDataEx)));
        m_stCodes->count      = 0;
        m_stCodes->index      = 0;
        m_stCodes->bstrm_pos  = 0;
        m_stCodes->offsets    = (Ipp32u*)((Ipp8u*)m_stCodes +
            sizeof(MediaDataEx::_MediaDataEx));
        m_stCodes->values     = (Ipp32u*)((Ipp8u*)m_stCodes->offsets +
            START_CODE_NUMBER*sizeof( Ipp32u));

        m_readDataSize = m_readBufSize;
        umcSts = m_pDataReader->GetData(m_readBuf,&m_readDataSize);
        if(umcSts != UMC_OK || m_readDataSize == 0)
        {
            Close();
            return UMC_ERR_FAILED;
        }

        m_readData->SetBufferPointer(m_readBuf, m_readDataSize);

        temp_val = ((*(m_readBuf+3))<<24) + ((*(m_readBuf+2))<<16) + ((*(m_readBuf+1))<<8) + *(m_readBuf);
        if(temp_val == 0x0F010000)
        {
            m_frame_constructor = new vc1_frame_constructor_vc1();
            if(m_frame_constructor==NULL)
            {
                Close();
                return UMC_ERR_ALLOC;
            }
            ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype = VC1_VIDEO_VC1;
        }
        else
        {
            m_frame_constructor = new vc1_frame_constructor_rcv();
            if(m_frame_constructor==NULL)
            {
                Close();
                return UMC_ERR_ALLOC;
            }
            ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype = VC1_VIDEO_RCV;
        }

        umcSts = GetFirstSeqHeader(&mediaData, splMode);
        if(umcSts != UMC_OK)
        {
            Close();
            return UMC_ERR_INIT;
        }

        return UMC_OK;
    }

    Status VC1Splitter::Close()
    {
        DeleteReadBuf();
        DeleteFrameBuf();

        m_pDataReader = NULL;

        if(m_readData)
        {
            delete m_readData;
            m_readData = NULL;
        }

        if(m_stCodes)
        {
            ippsFree(m_stCodes);
            m_stCodes = NULL;
        }

        if(m_frame_constructor)
        {
            delete m_frame_constructor;
            m_frame_constructor = NULL;
        }

        if(m_info.m_ppTrackInfo)
        {
            if(m_info.m_ppTrackInfo[0])
            {
                if(m_info.m_ppTrackInfo[0]->m_pStreamInfo)
                {
                    delete m_info.m_ppTrackInfo[0]->m_pStreamInfo;
                    m_info.m_ppTrackInfo[0]->m_pStreamInfo = NULL;
                }

                delete m_info.m_ppTrackInfo[0];
                m_info.m_ppTrackInfo[0] = NULL;
            }

            delete []m_info.m_ppTrackInfo;
            m_info.m_ppTrackInfo = NULL;
        }
        m_readDataSize = 0;
        m_frameSize = 0;
        m_seqHeaderFlag = 0;
        m_NextFrame = 0;
        return UMC_OK;
    }


    void VC1Splitter::DeleteReadBuf()
    {
        if(m_readBuf)
        {
            ippsFree(m_readBuf);
            m_readBuf = NULL;
        }

        m_readBufSize = 0;
    }

    void VC1Splitter::DeleteFrameBuf()
    {
        if(m_frameBuf)
        {
            ippsFree(m_frameBuf);
            m_frameBuf = NULL;
        }

        m_frameBufSize = 0;
    }


    Status VC1Splitter::Run()
    {
        UMC::Status umcSts = UMC_OK;
        return umcSts;
    }

    Status VC1Splitter::Stop()
    {
        Status umcSts = UMC_OK;
        return umcSts;
    }

    Status VC1Splitter::GetNextData(MediaData*   data, Ipp32u /*nTrack*/)
    {
        Status umcSts = UMC_ERR_NOT_ENOUGH_DATA;
        Status umcStsReader = UMC_OK;
        Ipp32u splMode = 0;//1 - delete 000003, 0 - not;

        VC1FrameConstrInfo info;

        MediaDataEx *data_ex = DynamicCast<MediaDataEx,MediaData>(data);
        if((data_ex != NULL)&&(!m_bNotDelete))
        {
            data_ex->SetExData(m_stCodes);
            splMode = 1;
        }

        if(m_NextFrame == 1)
        {
            data->SetBufferPointer(m_frameBuf, m_frameBufSize);
            data->SetDataSize(m_frameSize);

            m_NextFrame = 0;
            return UMC_OK;
        }

        if(m_seqHeaderFlag)
        {
            if(ResizeFrameBuf(m_frameBufSize)!=UMC_OK)
                return UMC_ERR_ALLOC;

            m_seqHeaderFlag = 0;
        }

        memset(m_stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        memset(m_stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        //memset(m_frameBuf, 0,m_frameBufSize*sizeof(Ipp8s));
        m_stCodes->count = 0;
        m_NextFrame = 0;

        data->SetBufferPointer(m_frameBuf, m_frameBufSize);
        data->SetDataSize(0);

        info.in = m_readData;
        info.out = data;
        info.stCodes = m_stCodes;
        info.splMode = splMode;

        umcSts = m_frame_constructor->GetNextFrame(info);

        while(umcSts == UMC_ERR_NOT_ENOUGH_DATA)
        {
            m_readDataSize = m_readBufSize - (Ipp32u)m_readData->GetDataSize();

            umcStsReader = m_pDataReader->GetData(m_readBuf + (Ipp32u)m_readData->GetDataSize(),
                &m_readDataSize);
            m_readData->SetBufferPointer(m_readBuf,(Ipp32u)m_readData->GetDataSize() + m_readDataSize);
            umcSts = m_frame_constructor->GetNextFrame(info);
        }

        if(!(umcStsReader == UMC_OK || umcStsReader == UMC_ERR_END_OF_STREAM) )
        {
            data->SetDataSize(0);
            return umcStsReader;
        }

       m_frameSize = (Ipp32u)data->GetDataSize();

       if((((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype == VC1_VIDEO_VC1)
            && (umcSts == UMC_OK || umcSts == UMC_ERR_END_OF_STREAM))
        {
            switch((m_stCodes->values[0])>>24)
            {
            case VC1_SequenceHeader:
                m_seqHeaderFlag = 1;
                SwapData(m_frameBuf, m_frameSize);
                umcSts = m_frame_constructor->ParseVC1SeqHeader((Ipp8u*)data->GetDataPointer() + 4,
                                               &m_frameBufSize,
                                               (VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo);
                if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) != FLAG_VSPL_4BYTE_ACCESS)
                      SwapData(m_frameBuf, m_frameSize);
                break;
            case VC1_EndOfSequence:
                //data->SetDataSize(data->GetDataSize() - 1);
                if (umcStsReader == UMC_ERR_END_OF_STREAM)
                    return UMC_ERR_END_OF_STREAM;
                else
                    return UMC_ERR_NOT_ENOUGH_DATA;
            case VC1_EntryPointHeader:
            case VC1_FrameHeader:
            case VC1_Slice:
            case VC1_Field:
            case VC1_SliceLevelUserData:
            case VC1_FieldLevelUserData:
            case VC1_FrameLevelUserData:
            case VC1_EntryPointLevelUserData:
            case VC1_SequenceLevelUserData:
                if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
                    SwapData(m_frameBuf, m_frameSize);
                break;
            default:
                break;
            }

            return umcSts;
        }
        if((((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype == VC1_VIDEO_RCV)
            && (umcSts == UMC_OK || umcSts == UMC_ERR_END_OF_STREAM))
        {
            if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
                SwapData(m_frameBuf,(Ipp32u)data->GetDataSize());

            return umcSts;
        }
        else if(umcSts == UMC_ERR_NOT_ENOUGH_BUFFER)
        {
            m_frameBufSize = m_frameBufSize + 2048;

            if(ResizeFrameBuf(m_frameBufSize)!=UMC_OK)
                return UMC_ERR_ALLOC;

            return umcSts;
        }
        else
            return umcSts;
    }

    Status VC1Splitter::CheckNextData(MediaData* data, Ipp32u /*nTrack*/)
    {
        UMC::Status umcSts = UMC_ERR_NOT_ENOUGH_DATA;
        UMC::Status umcStsReader = UMC_OK;
        Ipp32u splMode = 0; //1 - delete 000103, 0 - not;
        VC1FrameConstrInfo info;

        MediaDataEx *data_ex = DynamicCast<MediaDataEx,MediaData>(data);
        if((data_ex != NULL)&&(!m_bNotDelete))
        {
            data_ex->SetExData(m_stCodes);
            splMode = 1;
        }


        if(m_NextFrame == 1)
        {
            data->SetBufferPointer(m_frameBuf, m_frameBufSize);
            data->SetDataSize(m_frameSize);

            return UMC_OK;
        }

        if(m_seqHeaderFlag)
        {
            if(ResizeFrameBuf(m_frameBufSize)!=UMC_OK)
                return UMC_ERR_ALLOC;

            m_seqHeaderFlag = 0;
        }
        memset(m_stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        memset(m_stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        //memset(m_frameBuf, 0,m_frameBufSize*sizeof(Ipp8s));
        m_stCodes->count = 0;
        m_NextFrame = 1;

        data->SetBufferPointer(m_frameBuf, m_frameBufSize);
        data->SetDataSize(0);

        info.in = m_readData;
        info.out = data;
        info.stCodes = m_stCodes;
        info.splMode = splMode;


        umcSts = m_frame_constructor->GetNextFrame(info);

        while( umcSts == UMC_ERR_NOT_ENOUGH_DATA)
        {
            m_readDataSize = m_readBufSize - (Ipp32u)m_readData->GetDataSize();

            umcStsReader = m_pDataReader->GetData(m_readBuf + (Ipp32u)m_readData->GetDataSize(),
                &m_readDataSize);
            m_readData->SetBufferPointer(m_readBuf,(Ipp32u)m_readData->GetDataSize() + m_readDataSize);
            umcSts = m_frame_constructor->GetNextFrame(info);
        }

        if(!(umcStsReader == UMC_OK || umcStsReader == UMC_ERR_END_OF_STREAM) )
        {
            data->SetDataSize(0);
            return umcStsReader;
        }

        m_frameSize = (Ipp32u)data->GetDataSize();

        if(umcSts == UMC_OK || (umcSts == UMC_ERR_END_OF_STREAM))
        {
            if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
                SwapData(m_frameBuf, (Ipp32u)data->GetDataSize());

            return umcSts;
        }
        else if(umcSts == UMC_ERR_NOT_ENOUGH_BUFFER)
        {
            return umcSts;
        }
        else
            return umcSts;
    }

    Status VC1Splitter::GetTimePosition(Ipp64f& /*timePos*/)
    {
        UMC::Status umcSts = UMC_OK;
        return umcSts;
    }

    Status VC1Splitter::SetRate(Ipp64f /*rate*/)
    {
        Status umcSts = UMC_OK;
        return umcSts;
    }


    Status VC1Splitter::ResizeFrameBuf(Ipp32u bufferSize)
    {
        DeleteFrameBuf();

        //buf size should be divisible by 4
        if( bufferSize&0x00000003)
            m_frameBufSize = (bufferSize&0xFFFFFFFC) + 4;
        else
            m_frameBufSize = bufferSize;

        m_frameBuf = (Ipp8u*)ippsMalloc_8u((Ipp32s)(m_frameBufSize*sizeof(Ipp8u)));
        if(m_frameBuf==NULL)
        {
            return UMC_ERR_ALLOC;
        }
        return UMC_OK;
    }

    Status VC1Splitter::SetTimePosition(Ipp64f /*start_time*/)
    {
        return UMC_OK;
    }

    Status VC1Splitter::GetInfo(SplitterInfo** ppInfo)
    {
        *ppInfo = &m_info;
        return UMC_OK;
    }

    Status VC1Splitter::GetFirstSeqHeader(MediaDataEx* mediaData,
                                          Ipp32u splMode)
    {
        Status umcSts = UMC_OK;
        Status umcStsReader = UMC_OK;
        Ipp32u size;
        Ipp8u* ptr;
        VC1FrameConstrInfo info;

        memset(m_stCodes->offsets, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        memset(m_stCodes->values, 0,START_CODE_NUMBER*sizeof(Ipp32s));
        //memset(m_frameBuf, 0,m_frameBufSize*sizeof(Ipp8s));
        m_stCodes->count = 0;

        mediaData->SetBufferPointer(m_frameBuf, m_frameBufSize);
        mediaData->SetExData(m_stCodes);

        m_NextFrame = 1;

        info.in = m_readData;
        info.out = mediaData;
        info.stCodes = m_stCodes;
        info.splMode = splMode;

        umcSts = m_frame_constructor->GetFirstSeqHeader(info);

        if((((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype & VC1_VIDEO_VC1)
            &&(m_stCodes->values[0])>>24 == VC1_SequenceHeader)
        {
            m_frameSize =(Ipp32u) mediaData->GetDataSize();
            m_seqHeaderFlag = 1;

            if( (m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
            {
                SwapData(m_frameBuf, m_frameSize);
                umcSts = m_frame_constructor->ParseVC1SeqHeader((Ipp8u*)mediaData->GetDataPointer() + 4,
                    &m_frameBufSize,
                    (VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo);
            }
            else
            {
                SwapData(m_frameBuf, m_frameSize);
                umcSts = m_frame_constructor->ParseVC1SeqHeader((Ipp8u*)mediaData->GetDataPointer() + 4,
                    &m_frameBufSize,
                    (VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo);
                SwapData(m_frameBuf, m_frameSize);
            }

            size = (Ipp32u)m_pDataReader->GetSize();

            if(size && ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->bitrate)
                ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->duration  =
                ((Ipp64f)(Ipp64s)(size * 8))/((Ipp64f)(Ipp64s)(((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->bitrate));
            else
                ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->duration = 60*60*3;

            //printf("bitrate = %d\n",((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->bitrate);
            //printf("framerate = %f\n",((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->framerate);
        }
        else if(((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->stream_subtype== VC1_VIDEO_RCV)
        {
            m_frameSize = (Ipp32u)mediaData->GetDataSize();

            ptr = (Ipp8u*)mediaData->GetDataPointer() + 4;
            //size = *(Ipp32u *)ptr;
            size = ((*(ptr+3))<<24) + ((*(ptr+2))<<16) + ((*(ptr+1))<<8) + *(ptr);
            ptr = ptr + 4 + size;

            ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->clip_info.height = ((*(ptr+3))<<24) + ((*(ptr+2))<<16) + ((*(ptr+1))<<8) + *(ptr);


            ptr = ptr + 4;
            ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->clip_info.width = ((*(ptr+3))<<24) + ((*(ptr+2))<<16) + ((*(ptr+1))<<8) + *(ptr);


            if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
                SwapData(m_frameBuf, m_frameSize);

            m_seqHeaderFlag = 1;
            if((m_info.m_splitter_flags & FLAG_VSPL_4BYTE_ACCESS) == FLAG_VSPL_4BYTE_ACCESS)
            {
                umcSts = m_frame_constructor->ParseVC1SeqHeader((Ipp8u*)mediaData->GetDataPointer() + 8,
                    &m_frameBufSize,
                    (VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo);
            }
            else
            {
                SwapData(m_frameBuf, m_frameSize);
                umcSts = m_frame_constructor->ParseVC1SeqHeader((Ipp8u*)mediaData->GetDataPointer() + 8,
                    &m_frameBufSize,
                    (VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo);
                SwapData(m_frameBuf, m_frameSize);
            }

            size = (Ipp32u)m_pDataReader->GetSize();

            if(size && (((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->bitrate))
                ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->duration  =
                ((Ipp64f)(Ipp64s)(size * 8))/((Ipp64f)(Ipp64s)(((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->bitrate));
            else
                ((VideoStreamInfo*)m_info.m_ppTrackInfo[0]->m_pStreamInfo)->duration = 60*60*3;
        }
        else
            return UMC_ERR_INIT;

        return umcStsReader;
    }
    Status VC1Splitter::EnableTrack(Ipp32u /*nTrack*/, Ipp32s /*iState*/)
    {
        return UMC_ERR_NOT_IMPLEMENTED;
    }
}


#endif //UMC_ENABLE_VC1_SPLITTER