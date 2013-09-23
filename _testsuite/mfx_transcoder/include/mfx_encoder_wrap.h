/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_ENCODE_WRAPPER_H
#define __MFX_ENCODE_WRAPPER_H

#include "mfx_renders.h"
#include "mfx_component_params.h"
#include "test_statistics.h"
#include "mfx_ivideo_encode.h"

class MFXEncodeWRAPPER : public MFXFileWriteRender
{
public:
    MFXEncodeWRAPPER( ComponentParams &refParams
                    , mfxStatus *status
                    , std::auto_ptr<IVideoEncode> &pTargetEncode);
    ~MFXEncodeWRAPPER();

    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);
    virtual mfxStatus GetVideoParam(mfxVideoParam *par); 
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl=NULL);
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out);
    virtual mfxStatus Reset(mfxVideoParam *pParam);
    virtual mfxStatus Close();
    virtual mfxStatus WaitTasks(mfxU32 nMilisecconds);
    virtual mfxStatus SetExtraParams(EncodeExtraParams *pExtraParams);
    
protected:
    //forces creation particular number of output bitstreams
    virtual mfxStatus SetOutBitstreamsNum(mfxU16 nBitstreamsOut);
    virtual mfxStatus ZeroBottomStripe(mfxFrameSurface1 * psrf);
    virtual mfxStatus InitBitstream(mfxBitstream *bs);

    //link to out params
    ComponentParams&    m_refParams;

    std::auto_ptr<IVideoEncode> m_encoder;
    mfxSession          m_InSession;
    mfxU64              m_EncodedSize;
    Timer               m_encTime;

    // ref file
    vm_file *           m_pRefFile;
    mfxU8*              m_pCacheBuffer;
    mfxU32              m_nCacheBufferSize;

    EncodeExtraParams   m_ExtraParams;

    struct
    {
        mfxSyncPoint    m_SyncPoint;
        mfxBitstream  * m_pBitstream;
        mfxEncodeCtrl * m_pCtrl;
        bool            m_bCtrlAllocated;
    }*                  m_pSyncPoints;
    mfxU16              m_nSyncPoints;

    //overwrite for comparison
    virtual mfxStatus PutBsData(mfxBitstream* pData);

    //asignments are not used
    void operator = (const MFXEncodeWRAPPER & /*other*/){}
};

#ifdef PAVP_BUILD
#include "pavp_video_lib.h"
class MFXProtectedEncodeWRAPPER: public MFXEncodeWRAPPER
{
public:
    MFXProtectedEncodeWRAPPER(
                    CPAVPVideo  *pavpVideo
                    , ComponentParams &refParams
                    , mfxStatus *status
                    , std::auto_ptr<IVideoEncode> &pTargetEncode)
        :MFXEncodeWRAPPER(refParams, status, pTargetEncode)
        ,m_pavpVideo(pavpVideo)
    {}
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out)
    {
        if (NULL != in && NULL != out)
            memcpy(out, in, sizeof(*out)); // workaround for MPEG2 encoder. Queery always fail for protected due to counter type checking switch-case logic
        else
            return MFXEncodeWRAPPER::Query(in, out);
        return MFX_ERR_NONE;
    }
protected:
    virtual mfxStatus InitBitstream(mfxBitstream *bs);
    virtual mfxStatus PutBsData(mfxBitstream* pData);
    
    CPAVPVideo  *m_pavpVideo;
};
#endif /PAVP_BUILD

#endif//__MFX_ENCODE_WRAPPER_H