/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2004-2016 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_MJPEG_DEC_DECODE_H_
#define _MFX_MJPEG_DEC_DECODE_H_

#define ALLOW_SW_FALLBACK

#include "mfx_common_int.h"
#include "umc_video_decoder.h"
#include "mfx_umc_alloc_wrapper.h"

#include "umc_mutex.h"

#ifdef ALLOW_SW_FALLBACK
#include <queue>
#endif

#include "mfx_task.h"


#if defined (MFX_VA)
#include "mfx_vpp_jpeg_d3d9.h"
#endif

namespace UMC
{
    class MJPEGVideoDecoderBaseMFX;
    class JpegFrameConstructor;
    class MediaDataEx;
    class FrameData;
};

class VideoDECODEMJPEGBase
{
public:
    std::auto_ptr<mfx_UMC_FrameAllocator>    m_FrameAllocator;

    UMC::VideoDecoderParams umcVideoParams;

    // Free tasks queue guard (if SW is used)
    UMC::Mutex m_guard;
    mfxDecodeStat m_stat;
    bool    m_isOpaq;
    mfxVideoParamWrapper m_vPar;

    VideoDECODEMJPEGBase();
    virtual ~VideoDECODEMJPEGBase(){};

    virtual mfxStatus Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal, bool isUseExternalFrames, VideoCORE *core) = 0;
    virtual mfxStatus ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf, bool isOpaq) = 0;
    virtual mfxStatus CheckTaskAvailability(mfxU32 maxTaskNumber) = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) = 0;
    virtual mfxStatus RunThread(void *pParam, mfxU32 threadNumber, mfxU32 callNumber) = 0;
    virtual mfxStatus CompleteTask(void *pParam, mfxStatus taskRes) = 0;
    virtual void ReleaseReservedTask() = 0;
    virtual mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic) = 0;
    virtual mfxStatus AllocateFrameData(UMC::FrameData *&data) = 0;
    virtual mfxStatus FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out) = 0;

    virtual mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual mfxStatus Close(void) = 0;

protected:

    mfxStatus GetVideoParam(mfxVideoParam *par, UMC::MJPEGVideoDecoderBaseMFX * mjpegDecoder);
};

#if defined (MFX_VA)
namespace UMC
{
    class MJPEGVideoDecoderMFX_HW;
};

class VideoDECODEMJPEGBase_HW : public VideoDECODEMJPEGBase
{
public:
    VideoDECODEMJPEGBase_HW();

    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal, bool isUseExternalFrames, VideoCORE *core);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus RunThread(void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    virtual mfxStatus CompleteTask(void *pParam, mfxStatus taskRes);
    virtual mfxStatus CheckTaskAvailability(mfxU32 maxTaskNumber);
    virtual mfxStatus ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf, bool isOpaq);
    virtual void ReleaseReservedTask();
    virtual mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic);
    virtual mfxStatus AllocateFrameData(UMC::FrameData *&data);
    virtual mfxStatus FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);

    mfxU32 AdjustFrameAllocRequest(mfxFrameAllocRequest *request, mfxInfoMFX *info, eMFXHWType hwType, eMFXVAType vaType);

    static void AdjustFourCC(mfxFrameInfo *requestFrameInfo, mfxInfoMFX *info, eMFXHWType hwType, eMFXVAType vaType, bool *needVpp);
    
    static mfxStatus CheckVPPCaps(VideoCORE * core, mfxVideoParam * par);
#if defined (MFX_VA_WIN)
    static mfxStatus CheckDecodeCaps(VideoCORE * core, mfxVideoParam * par);
#endif


protected:
    VideoVppJpegD3D9 *m_pCc;
    // True if we need special VPP color conversion after decoding
    bool   m_needVpp;
    // Decoder's array
    std::auto_ptr<UMC::MJPEGVideoDecoderMFX_HW> m_pMJPEGVideoDecoder;

    // Number of pictures collected
    mfxU32 m_numPic;
    // Output frame
    UMC::FrameData *m_dst;
    // Array of all currently using frames
    std::vector<UMC::FrameData*> m_dsts;

    UMC::VideoAccelerator * m_va;
};
#endif

#ifdef ALLOW_SW_FALLBACK
// Forward declaration of used classes
class CJpegTask;

class VideoDECODEMJPEGBase_SW : public VideoDECODEMJPEGBase
{
public:
    VideoDECODEMJPEGBase_SW();

    virtual mfxStatus Init(mfxVideoParam *decPar, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response, mfxFrameAllocRequest *request_internal, bool isUseExternalFrames, VideoCORE *core);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus RunThread(void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    virtual mfxStatus CompleteTask(void *pParam, mfxStatus taskRes);
    virtual mfxStatus CheckTaskAvailability(mfxU32 maxTaskNumber);
    virtual mfxStatus ReserveUMCDecoder(UMC::MJPEGVideoDecoderBaseMFX* &pMJPEGVideoDecoder, mfxFrameSurface1 *surf, bool isOpaq);
    virtual void ReleaseReservedTask();
    virtual mfxStatus AddPicture(UMC::MediaDataEx *pSrcData, mfxU32 & numPic);
    virtual mfxStatus AllocateFrameData(UMC::FrameData *&data);
    virtual mfxStatus FillEntryPoint(MFX_ENTRY_POINT *pEntryPoint, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);

protected:
    CJpegTask *pLastTask;
    // Free tasks queue (if SW is used)
    std::queue<CJpegTask *> m_freeTasks;
    // Count of created tasks (if SW is used)
    mfxU16  m_tasksCount;
};
#endif

class VideoDECODEMJPEG : public VideoDECODE
{
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
    static mfxStatus DecodeHeader(VideoCORE *core, mfxBitstream *bs, mfxVideoParam *par);

    VideoDECODEMJPEG(VideoCORE *core, mfxStatus * sts);
    virtual ~VideoDECODEMJPEG(void);

    mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *stat);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 *surface_out);
    virtual mfxStatus GetUserData(mfxU8 *ud, mfxU32 *sz, mfxU64 *ts);
    virtual mfxStatus GetPayload(mfxU64 *ts, mfxPayload *payload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

protected:
    static mfxStatus QueryIOSurfInternal(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);

    bool IsSameVideoParam(mfxVideoParam * newPar, mfxVideoParam * oldPar);


    mfxStatus UpdateAllocRequest(mfxVideoParam *par, 
                                mfxFrameAllocRequest *request,
                                mfxExtOpaqueSurfaceAlloc * &pOpaqAlloc,
                                bool &mapping);

    mfxFrameSurface1 * GetOriginalSurface(mfxFrameSurface1 *surface);

    // Frames collecting unit
    std::auto_ptr<UMC::JpegFrameConstructor> m_frameConstructor;


    mfxVideoParamWrapper m_vFirstPar;
    mfxVideoParamWrapper m_vPar;

    VideoCORE * m_core;

    bool    m_isInit;
    bool    m_isOpaq;
    bool    m_isHeaderFound;
    bool    m_isHeaderParsed;

    mfxU32  m_frameOrder;

    std::auto_ptr<VideoDECODEMJPEGBase> decoder;

    mfxFrameAllocResponse m_response;
    mfxFrameAllocResponse m_response_alien;
    eMFXPlatform m_platform;

    UMC::Mutex m_mGuard;

    // Frame skipping rate
    mfxU32 m_skipRate;
    // Frame skipping count
    mfxU32 m_skipCount;

    //
    // Asynchronous processing functions
    //

    static
    mfxStatus MJPEGDECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static
    mfxStatus MJPEGCompleteProc(void *pState, void *pParam, mfxStatus taskRes);
};

#endif // _MFX_MJPEG_DEC_DECODE_H_
#endif // MFX_ENABLE_MJPEG_VIDEO_DECODE
