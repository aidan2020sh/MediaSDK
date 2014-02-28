/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mf_venc_obuf.h

Purpose: define code for support of output buffering in Intel MediaFoundation
encoder plug-ins.

Defined Classes, Structures & Enumerations:
  * MFEncBitstream - enables synchronization between IMFSample and mfxBitstream

*********************************************************************************/

#ifndef __MF_VENC_OBUF_H__
#define __MF_VENC_OBUF_H__

#include "mf_utils.h"
#include "mf_samples_extradata.h"
#include <limits>

#define LTR_DEFAULT_FRAMEINFO 0xFFFF // [31..16] = 0, [15..0] = 0xFFFF 
/*------------------------------------------------------------------------------*/

class MFEncBitstream : public MFDebugDump
{
public:
    MFEncBitstream (void);
    ~MFEncBitstream(void);

    mfxStatus Init   (mfxU32 bufSize,
                      MFSamplesPool* pSamplesPool);
    void      Close  (void);
    mfxStatus Alloc  (void);
    HRESULT   Sync   (void);
    HRESULT   Release(void);

    static mfxU32 CalcBufSize(const mfxVideoParam &params);

    mfxBitstream* GetMfxBitstream(void){ return &m_Bst; }
    void IsGapBitstream(bool bGap){ m_bGap = bGap; }

    IMFSample* GetSample(void)
    { if (m_pSample) m_pSample->AddRef(); return m_pSample; }

    mfxU32 GetFrameOrder() const
    {
        mfxU32 nResult = _UI32_MAX;
        if (NULL != m_pEncodedFrameInfo)
        {
            nResult = m_pEncodedFrameInfo->FrameOrder;
        }
        return nResult;
    }

protected:
    bool            m_bInitialized; // flag to indicate initialization
    bool            m_bLocked;      // flag to indicate lock of MF buffer
    bool            m_bGap;         // flag to indicate discontinuity frame
    mfxU32          m_BufSize;
    IMFSample*      m_pSample;
    IMFMediaBuffer* m_pMediaBuffer;
    mfxBitstream    m_Bst;
    MFSamplesPool*  m_pSamplesPool;
    mfxExtAVCEncodedFrameInfo*  m_pEncodedFrameInfo;

private:
    // avoiding possible problems by defining operator= and copy constructor
    MFEncBitstream(const MFEncBitstream&);
    MFEncBitstream& operator=(const MFEncBitstream&);
};

/*------------------------------------------------------------------------------*/

struct MFEncOutData
{
    bool             bFreeData;         // Empty entry
    bool             bSynchronized;     // SyncOperation is completed
    mfxSyncPoint     syncPoint;

    bool             bSyncPointUsed;    // SyncOperation is started
    mfxStatus        iSyncPointSts;
    MFEncBitstream*  pMFBitstream;

    void Use()
    {
        bFreeData = false;
    }

    mfxStatus SyncOperation(MFXVideoSession* pMfxVideoSession)
    {
        ATLASSERT(!(bFreeData));
        bSyncPointUsed = true;
        ATLASSERT(!bSynchronized);
        iSyncPointSts = pMfxVideoSession->SyncOperation(syncPoint, INFINITE);
        bSynchronized = true;
        return iSyncPointSts;
    }

    void Release()
    {
        pMFBitstream->Release();
        syncPoint      = NULL;
        bSyncPointUsed = false;
        bFreeData      = true;
        bSynchronized  = false;
    }
};

/*------------------------------------------------------------------------------*/

// initial number of output bitstreams (samples)
#define MF_BITSTREAMS_NUM 1
// number of buffered output events to start receiving input data again after maximum is reached
#define MF_OUTPUT_EVENTS_COUNT_LOW_LIMIT 5
// maximum number of buffered output events (bitstreams)
#define MF_OUTPUT_EVENTS_COUNT_HIGH_LIMIT 15

/*------------------------------------------------------------------------------*/

class MFOutputBitstream
{
public:
    MFOutputBitstream::MFOutputBitstream();
    virtual void            Free(); //free m_pFreeSamplesPool, OutBitstreams, Header
    void                    DiscardCachedSamples(MFXVideoSession* pMfxVideoSession);
    void                    ReleaseSample();
    bool                    GetSample(IMFSample** ppResult, HRESULT &hr);
    HRESULT                 FindHeader();
    mfxStatus               UseBitstream(mfxU32 bufSize);
    mfxStatus               InitFreeSamplesPool(); // Free samples pool allocation
    bool                    HandleDevBusy(mfxStatus& sts, MFXVideoSession* pMfxVideoSession);
    mfxStatus               SetFreeBitstream(mfxU32 bufSize);
    void                    CheckBitstreamsNumLimit(void);
    bool                    HaveBitstreamToDisplay() const; //m_pDispBitstream != m_pOutBitstream
    void                    SetFramerate(mfxU32 fr_n, mfxU32 fr_d) { m_Framerate = mf_get_framerate(fr_n, fr_d); }
    //used only in ProcessFrameEnc as arguments for EncodeFrameAsync
    MFEncOutData*           GetOutBitstream() const;
    //used in 1) AsyncThread to SyncOperation or get saved sts. 2) ProcessOutput (check against NULL)
    MFEncOutData*           GetDispBitstream() const;

    mfxU32                  m_uiHeaderSize;
    mfxU8*                  m_pHeader;
    mfxU32                  m_uiHasOutputEventExists; //TODO: try to hide
    
    //used widely (4 places)
    bool                    m_bBitstreamsLimit;
    //fields below are used about 1-2 times
    bool                    m_bSetDiscontinuityAttribute;
    //TODO: move to MFSampleExtradataTransport
    std::queue<LONGLONG>    m_arrDTS;        // Input timestamps used for output DTS
    bool                    m_bSetDTS;       // if not set previous DTSes are ignored.
    MFSampleExtradataTransport m_ExtradataTransport;

    FILE*                   m_dbg_encout;
    //debug tracing functions:
    void                    TraceBitstream(size_t i) const;
    void                    TraceBitstreams() const;
protected:
    HRESULT                 FindHeaderInternal(mfxBitstream* pBst);
    virtual void            FreeHeader();
    virtual void            FreeOutBitstreams();
    //moves to next bitstream on circular buffer (may return to m_pOutBitstreams)
    void                    MoveToNextBitstream(size_t &nBitstreamIndex) const;
    mfxStatus               AllocBitstream(size_t nIndex, mfxU32 bufSize);

    mfxF64                  m_Framerate;
    MFSamplesPool*          m_pFreeSamplesPool;

    // pointers to encoded bitstreams
    // Stored in "circular buffer" with "one slot always open". 
    // m_nOutBitstreamsNum      - number of allocated elements. m_pOutBitstreams - allocated buffer.
    // m_nDispBitstreamIndex    - index of "start"
    // m_nOutBitstreamIndex     - index of next element after the "end", This is "open slot"
    mfxU32                  m_nOutBitstreamsNum; 
    MFEncOutData**          m_pOutBitstreams;
    size_t                  m_nOutBitstreamIndex;
    size_t                  m_nDispBitstreamIndex;
};

#endif // #ifndef __MF_VENC_OBUF_H__
