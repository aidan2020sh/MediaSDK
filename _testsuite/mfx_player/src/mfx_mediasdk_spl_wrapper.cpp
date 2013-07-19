/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */
#include "mfx_pipeline_defs.h"

#include "mfx_mediasdk_spl_wrapper.h"
#include "mfx_frame_constructor.h"

MediaSDKSplWrapper::MediaSDKSplWrapper()
    : m_pConstructor()
    , m_videoTrackIndex(0)
{
}

MediaSDKSplWrapper::~MediaSDKSplWrapper()
{
    Close();
}

void MediaSDKSplWrapper::Close()
{
    for (mfxU32 i=0; i < m_streamParams.NumTrackAllocated; i++)
    {
        delete m_streamParams.TrackInfo[i];
        m_streamParams.TrackInfo[i] = NULL;
    }
    delete m_streamParams.TrackInfo;
    m_streamParams.TrackInfo = NULL;

    //MFXSplitter_Close(m_mfxSplitter);

    MFX_DELETE(m_pConstructor);
}

mfxStatus MediaSDKSplWrapper::Init(const vm_char *strFileName)
{
    mfxStatus sts = MFX_ERR_NONE;
    FILE* pSrcFile = NULL;
    pSrcFile = _tfopen(strFileName, VM_STRING("rb"));

    MFX_ZERO_MEM(m_ffmpegSplReader);
    m_ffmpegSplReader.m_bInited = true;
    m_ffmpegSplReader.m_fSource = pSrcFile;

    m_dataIO.pthis = &m_ffmpegSplReader;
    m_dataIO.Read = RdRead;
    m_dataIO.Seek = RdSeek;

   // sts = MFXSplitter_Init(&m_dataIO, &m_mfxSplitter);
    MFX_CHECK_STS(sts);

    MFX_ZERO_MEM(m_streamParams);
  //  sts = MFXSplitter_GetInfo(m_mfxSplitter, &m_streamParams);
    MFX_CHECK_STS(sts);

    m_streamParams.TrackInfo = new mfxTrackInfo*[m_streamParams.NumTrack];
    for (mfxU32 i=0; i < m_streamParams.NumTrack; i++)
        m_streamParams.TrackInfo[i] = new mfxTrackInfo;
    m_streamParams.NumTrackAllocated = m_streamParams.NumTrack;

  //  sts = MFXSplitter_GetInfo(m_mfxSplitter, &m_streamParams);
    MFX_CHECK_STS(sts);

    for (mfxU32 i=0; i < m_streamParams.NumTrack; i++)
    {
        if (m_streamParams.TrackInfo[i]->Type & MFX_TRACK_ANY_VIDEO)
        {
            m_videoTrackIndex = i;
            break;
        }
    }

    MFX_CHECK_WITH_ERR(m_pConstructor = new MFXFrameConstructor(), MFX_ERR_MEMORY_ALLOC);

    //initializing of frame constructor
    mfxVideoParam vParam;
    vParam.mfx.FrameInfo.Width  = m_streamParams.TrackInfo[m_videoTrackIndex]->VideoParam.FrameInfo.Width;
    vParam.mfx.FrameInfo.Height = m_streamParams.TrackInfo[m_videoTrackIndex]->VideoParam.FrameInfo.Height;
    vParam.mfx.CodecProfile     = m_streamParams.TrackInfo[m_videoTrackIndex]->VideoParam.CodecProfile;

    MFX_CHECK_STS(m_pConstructor->Init(&vParam));

    return sts;
}

mfxStatus MediaSDKSplWrapper::ReadNextFrame(mfxBitstream2 &bs2)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 iOutputTrack = 0;
    mfxBitstream bs;

    MFX_CHECK_POINTER(m_mfxSplitter);
    while (iOutputTrack != m_videoTrackIndex && sts == MFX_ERR_NONE)
    {
        if (sts == MFX_ERR_NONE)
        {
   //         sts = MFXSplitter_GetBitstream(m_mfxSplitter, &iOutputTrack, &bs);
        }
        if (sts == MFX_ERR_NONE && iOutputTrack == m_videoTrackIndex)
        {
            sts = m_pConstructor->ConstructFrame(&bs, &bs2);
        }
        if (sts == MFX_ERR_NONE)
        {
  //          sts = MFXSplitter_ReleaseBitstream(m_mfxSplitter, iOutputTrack, &bs);
        }
    }

    return sts;
}

mfxStatus MediaSDKSplWrapper::GetStreamInfo(sStreamInfo * pParams)
{
    MFX_CHECK_POINTER(pParams);

    pParams->nWidth  = m_streamParams.TrackInfo[0]->VideoParam.FrameInfo.Width;
    pParams->nHeight = m_streamParams.TrackInfo[0]->VideoParam.FrameInfo.Height;

    switch (m_streamParams.TrackInfo[0]->Type)
    {
        case MFX_TRACK_MPEG2V: pParams->videoType = MFX_CODEC_MPEG2; break;
        case MFX_TRACK_H264  : pParams->videoType = MFX_CODEC_AVC;   break;
        default : 
            return MFX_ERR_UNKNOWN;
    }

    pParams->CodecProfile = m_streamParams.TrackInfo[0]->VideoParam.CodecProfile;

    if (m_streamParams.SystemType == MFX_MP4_ATOM_STREAM && pParams->videoType == MFX_CODEC_AVC)
    {
        pParams->isDefaultFC = false;
    }else
    {
        pParams->isDefaultFC = true;
    }

    return MFX_ERR_NONE;
}

mfxStatus MediaSDKSplWrapper::SeekTime(mfxF64 /*fSeekTo*/)
{
    m_pConstructor->Reset();

    return MFX_ERR_NONE;
}

mfxStatus MediaSDKSplWrapper::SeekPercent(mfxF64 /*fSeekTo*/)
{

    return MFX_ERR_NONE;
}

mfxStatus MediaSDKSplWrapper::SeekFrameOffset(mfxU32 /*nFrameOffset*/, mfxFrameInfo & /*in_info*/)
{
    return MFX_ERR_NONE;
}

mfxI32 MediaSDKSplWrapper::RdRead(void* in_DataReader, mfxBitstream *BS)
{
    DataReader *pDr;
    size_t ReadSize;

    if (NULL == in_DataReader)
        return MFX_ERR_NULL_PTR;

    pDr = (DataReader*)(in_DataReader);
    if (NULL == pDr)
        return MFX_ERR_UNKNOWN;

    size_t freeSpace = BS->MaxLength - BS->DataOffset - BS->DataLength;
    void *freeSpacePtr = (void*)(BS->Data + BS->DataOffset + BS->DataLength);
    ReadSize = fread_s(freeSpacePtr, freeSpace, 1, freeSpace, pDr->m_fSource);
    BS->DataLength +=  ReadSize;

    return ReadSize;
}

mfxI64 MediaSDKSplWrapper::RdSeek(void* in_DataReader, mfxI64 offset, mfxSeekOrigin origin)
{
    DataReader *pRd;
    int oldOffset, fileSize;
    int whence;

    if (NULL == in_DataReader)
        return MFX_ERR_NULL_PTR;
    else
        pRd = (DataReader*)in_DataReader;

    switch(origin)
    {
    case MFX_SEEK_ORIGIN_BEGIN:
        whence = SEEK_SET;
        break;
    case MFX_SEEK_ORIGIN_CURRENT:
        whence = SEEK_CUR;
        break;
    default:
        whence = SEEK_END;
        break;
    }

    if (whence == VM_FILE_SEEK_END && offset == 0)
    {
        oldOffset = ftell(pRd->m_fSource);
        fseek(pRd->m_fSource, 0, SEEK_END);
        fileSize = ftell(pRd->m_fSource);
        fseek(pRd->m_fSource, oldOffset, SEEK_SET);
        return fileSize;
    }
    fseek(pRd->m_fSource, (long) offset, whence);

    return MFX_ERR_NONE;
}
