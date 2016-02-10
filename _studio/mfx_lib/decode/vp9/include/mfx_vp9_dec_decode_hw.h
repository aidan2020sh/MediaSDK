/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#ifndef _MFX_VP9_DECODE_HW_H_
#define _MFX_VP9_DECODE_HW_H_

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP9_VIDEO_DECODE_HW) && defined(MFX_VA)

#include "mfx_umc_alloc_wrapper.h"
#include "umc_mutex.h"

#include "mfx_task.h"
#include "mfx_vp9_dec_decode_utils.h"
#include <list>

#include "umc_vp9_dec_defs.h"
#include "umc_vp9_frame.h"

using namespace MfxVP9Decode;

namespace UMC_VP9_DECODER { class Packer; }

class VideoDECODEVP9_HW : public VideoDECODE
{
public:

    VideoDECODEVP9_HW(VideoCORE *pCore, mfxStatus *sts);
    virtual ~VideoDECODEVP9_HW();

    static mfxStatus Query(VideoCORE *pCore, mfxVideoParam *pIn, mfxVideoParam *pOut);
    static mfxStatus QueryIOSurf(VideoCORE *pCore, mfxVideoParam *pPar, mfxFrameAllocRequest *pRequest);

    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *pPar);
    virtual mfxStatus Close();

    virtual mfxTaskThreadingPolicy GetThreadingPolicy();
    virtual mfxStatus GetVideoParam(mfxVideoParam *pPar);
    virtual mfxStatus GetDecodeStat(mfxDecodeStat *pStat);

    virtual mfxStatus DecodeHeader(VideoCORE * core, mfxBitstream *bs, mfxVideoParam *params);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxFrameSurface1 *pSurfaceWork, mfxFrameSurface1 **ppSurfaceOut, MFX_ENTRY_POINT *pEntryPoint);

    virtual mfxStatus GetUserData(mfxU8 *pUserData, mfxU32 *pSize, mfxU64 *pTimeStamp);
    virtual mfxStatus GetPayload(mfxU64 *pTimeStamp, mfxPayload *pPayload);
    virtual mfxStatus SetSkipMode(mfxSkipMode mode);

protected:
    void CalculateTimeSteps(mfxFrameSurface1 *);
    static mfxStatus QueryIOSurfInternal(eMFXPlatform, mfxVideoParam *, mfxFrameAllocRequest *);

    mfxStatus UpdateRefFrames(const mfxU8 refreshFrameFlags, UMC_VP9_DECODER::VP9DecoderFrame & info);

    mfxStatus DecodeSuperFrame(mfxBitstream *in, UMC_VP9_DECODER::VP9DecoderFrame & info);
    mfxStatus DecodeFrameHeader(mfxBitstream *in, UMC_VP9_DECODER::VP9DecoderFrame & info);
    mfxStatus PackHeaders(mfxBitstream *bs, UMC_VP9_DECODER::VP9DecoderFrame const & info);

    void UpdateVideoParam(mfxVideoParam *par, UMC_VP9_DECODER::VP9DecoderFrame const & frameInfo);

    mfxStatus GetOutputSurface(mfxFrameSurface1 **surface_out, mfxFrameSurface1 *surface_work, UMC::FrameMemID index);

private:
    bool                    m_isInit;
    VideoCORE*              m_core;
    eMFXPlatform            m_platform;

    mfxVideoParamWrapper    m_vInitPar;
    mfxVideoParamWrapper    m_vPar;

    mfxU32                  m_num_output_frames;
    mfxF64                  m_in_framerate;
    mfxU16                  m_frameOrder;
    mfxU32                  m_statusReportFeedbackNumber;

    UMC::Mutex              m_mGuard;

    bool                    m_adaptiveMode;
    mfxU32                  m_index;
    std::auto_ptr<mfx_UMC_FrameAllocator> m_FrameAllocator;

    std::auto_ptr<UMC_VP9_DECODER::Packer>  m_Packer;

    mfxFrameAllocResponse   m_response;
    mfxDecodeStat           m_stat;

    friend mfxStatus MFX_CDECL VP9DECODERoutine(void *p_state, void *pp_param, mfxU32 thread_number, mfxU32);
    friend mfxStatus VP9CompleteProc(void *p_state, void *pp_param, mfxStatus);

    void ResetFrameInfo();

    struct VP9DECODERoutineData
    {
        VideoDECODEVP9_HW* decoder;
        mfxFrameSurface1* surface_work;
        UMC::FrameMemID currFrameId;
        UMC::FrameMemID copyFromFrame;
        mfxU32     index;
    };

    UMC::VideoAccelerator * m_va;

    typedef std::list<mfxFrameSurface1 *> StatuReportList;
    StatuReportList m_completedList;

    UMC_VP9_DECODER::VP9DecoderFrame m_frameInfo;

    typedef struct {
        mfxU32 width;
        mfxU32 height;
    } SizeOfFrame;

    SizeOfFrame m_firstSizes;

    SizeOfFrame m_sizesOfRefFrame[UMC_VP9_DECODER::NUM_REF_FRAMES];
    
    mfxBitstream m_bs;

    mfxI32 m_baseQIndex;
    mfxI32 m_y_dc_delta_q;
    mfxI32 m_uv_dc_delta_q;
    mfxI32 m_uv_ac_delta_q;
};

#endif // _MFX_VP9_DECODE_HW_H_
#endif // MFX_ENABLE_VP9_VIDEO_DECODE && MFX_VA
