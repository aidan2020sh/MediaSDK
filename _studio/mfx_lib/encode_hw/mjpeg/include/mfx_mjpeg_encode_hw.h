/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
//          HW MJPEG  encoder
//
*/

#ifndef __MFX_MJPEG_ENCODE_HW_H__
#define __MFX_MJPEG_ENCODE_HW_H__

#include "mfx_common.h"
#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)

#include "mfxvideo++int.h"
#include "mfxvideopro++.h"
#include "mfx_mjpeg_encode_hw_utils.h"
#include "mfx_mjpeg_encode_interface.h"

using namespace MfxHwMJpegEncode;

class MFXVideoENCODEMJPEG_HW : public VideoENCODE {
public:
    static mfxStatus Query(VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
    static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
   
    MFXVideoENCODEMJPEG_HW(VideoCORE *core, mfxStatus *sts);
    virtual ~MFXVideoENCODEMJPEG_HW() {Close();}
    virtual mfxStatus Init(mfxVideoParam *par);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);
    virtual mfxStatus GetFrameParam(mfxFrameParam *par);
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat);

    virtual
    mfxStatus EncodeFrameCheck(mfxEncodeCtrl *ctrl,
                               mfxFrameSurface1 *surface,
                               mfxBitstream *bs,
                               mfxFrameSurface1 **reordered_surface,
                               mfxEncodeInternalParams *pInternalParams,
                               MFX_ENTRY_POINT pEntryPoints[],
                               mfxU32 &numEntryPoints);

   // previous scheduling model - functions are not need to be implemented, only to be compatible 
    virtual mfxStatus EncodeFrameCheck(mfxEncodeCtrl *, 
                                       mfxFrameSurface1 *, 
                                       mfxBitstream *, 
                                       mfxFrameSurface1 **, 
                                       mfxEncodeInternalParams *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual mfxStatus EncodeFrame(mfxEncodeCtrl *, 
                                  mfxEncodeInternalParams *, 
                                  mfxFrameSurface1 *, 
                                  mfxBitstream *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
    virtual mfxStatus CancelFrame(mfxEncodeCtrl *, 
                                  mfxEncodeInternalParams *, 
                                  mfxFrameSurface1 *, 
                                  mfxBitstream *)
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

protected:
    // callbacks to work with scheduler
    static mfxStatus MFXVideoENCODEMJPEG_HW::TaskRoutineSubmitFrame(void * state, 
                                                                    void * param, 
                                                                    mfxU32 threadNumber, 
                                                                    mfxU32 callNumber);

    static mfxStatus MFXVideoENCODEMJPEG_HW::TaskRoutineQueryFrame(void * state, 
                                                                   void * param, 
                                                                   mfxU32 threadNumber, 
                                                                   mfxU32 callNumber);
    
    mfxStatus UpdateDeviceStatus(mfxStatus sts);
    mfxStatus CheckDevice();

    mfxStatus CheckEncodeFrameParam(mfxVideoParam const & video,
                                    mfxEncodeCtrl       * ctrl,
                                    mfxFrameSurface1    * surface,
                                    mfxBitstream        * bs,
                                    bool                  isExternalFrameAllocator);

    // pointer to video core - class for memory/frames management and allocation
    VideoCORE*          m_pCore;
    mfxVideoParam       m_vFirstParam;
    mfxVideoParam       m_vParam;
    std::auto_ptr<DriverEncoder> m_ddi;

    bool                m_bInitialized;
    bool                m_deviceFailed;

    mfxFrameAllocResponse m_raw;        // raw surface, for input raw is in system memory case
    mfxFrameAllocResponse m_bitstream;  // bitstream surface
    mfxU32                m_counter;    // task number (StatusReportFeedbackNumber)

    TaskManager           m_TaskManager;
};

#endif // MFX_ENABLE_MJPEG_VIDEO_ENCODE
#endif
