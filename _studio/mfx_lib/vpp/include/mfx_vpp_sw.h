// Copyright (c) 2010-2018 Intel Corporation
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

/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_SW_H
#define __MFX_VPP_SW_H

#include <memory>

#include "mfxvideo++int.h"

#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "mfx_task.h"

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
    #include "mfx_vpp_service.h"
#endif

/* ******************************************************************** */
/*                 Core Class of MSDK VPP                               */
/* ******************************************************************** */

#include "mfx_vpp_hw.h"

class VideoVPPBase
{
public:

  static mfxStatus Query( VideoCORE *core, mfxVideoParam *in, mfxVideoParam *out);
  static mfxStatus QueryIOSurf(VideoCORE *core, mfxVideoParam *par, mfxFrameAllocRequest *request);
  
  VideoVPPBase(VideoCORE *core, mfxStatus* sts);
  virtual ~VideoVPPBase();

  // VideoBase methods
  virtual mfxStatus Reset(mfxVideoParam *par);
  virtual mfxStatus Close(void);
  virtual mfxStatus Init(mfxVideoParam *par);

  virtual mfxStatus GetVideoParam(mfxVideoParam *par);
  virtual mfxStatus GetVPPStat(mfxVPPStat *stat);
  virtual mfxStatus VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux,
                                  MFX_ENTRY_POINT pEntryPoint[], mfxU32 &numEntryPoints);

  virtual mfxStatus VppFrameCheck(mfxFrameSurface1 *, mfxFrameSurface1 *)
  {
      return MFX_ERR_UNSUPPORTED;
  }

  virtual mfxStatus RunFrameVPP(mfxFrameSurface1* in, mfxFrameSurface1* out, mfxExtVppAuxData *aux) = 0;

  virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

protected:

  typedef struct
  {
    mfxFrameInfo    In;
    mfxFrameInfo    Out;
    mfxFrameInfo    Deffered;

    mfxU16          IOPattern;
    mfxU16          AsyncDepth;

    bool            isInited;
    bool            isFirstFrameProcessed;
    bool            isCompositionModeEnabled;

  } sErrPrtctState;

  virtual mfxStatus InternalInit(mfxVideoParam *par) = 0;

  // methods of sync part of VPP for check info/params
  mfxStatus CheckIOPattern( mfxVideoParam* par );

  static mfxStatus QueryCaps(VideoCORE * core, MfxHwVideoProcessing::mfxVppCaps& caps);
  static mfxStatus CheckPlatformLimitations( VideoCORE* core, mfxVideoParam & param, bool bCorrectionEnable );
  mfxU32    GetNumUsedFilters();

  std::vector<mfxU32> m_pipelineList; // list like DO_USE but contains some internal filter names (ex RESIZE, DEINTERLACE etc) 

  //
  bool               m_bDynamicDeinterlace;

  VideoCORE*  m_core;
  mfxVPPStat  m_stat;

  // protection state. Keeps Init/Reset params that allows checking that RunFrame params
  // do not violate Init/Reset params
  sErrPrtctState m_errPrtctState;

  // State that keeps Init params. They are changed on Init only
  sErrPrtctState m_InitState;

#if defined (MFX_ENABLE_OPAQUE_MEMORY)
  // opaque processing
  bool                  m_bOpaqMode[2];
  mfxFrameAllocRequest  m_requestOpaq[2];
#endif

  //
  // HW VPP Support
  //
  std::unique_ptr<MfxHwVideoProcessing::VideoVPPHW>     m_pHWVPP;
};

class VideoVPP_HW : public VideoVPPBase
{
public:
    VideoVPP_HW(VideoCORE *core, mfxStatus* sts);

    virtual mfxStatus InternalInit(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus GetVideoParam(mfxVideoParam *par);

    virtual mfxStatus VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux,
                                    MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints);

    virtual mfxStatus RunFrameVPP(mfxFrameSurface1* in, mfxFrameSurface1* out, mfxExtVppAuxData *aux);

    mfxStatus PassThrough(mfxFrameInfo* In, mfxFrameInfo* Out, mfxU32 taskIndex);
};

#if !defined (MFX_ENABLE_HW_ONLY_VPP)
class VideoVPP_SW : public VideoVPPBase
{
public:
    VideoVPP_SW(VideoCORE *core, mfxStatus* sts);

    static
    mfxStatus Query(VideoCORE* core,mfxVideoParam *par);

    static
    mfxStatus QueryCaps(VideoCORE* core, MfxHwVideoProcessing::mfxVppCaps& caps);

    virtual mfxStatus InternalInit(mfxVideoParam *par);
    virtual mfxStatus Close(void);
    virtual mfxStatus Reset(mfxVideoParam *par);

    virtual mfxStatus VppFrameCheck(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux,
                                    MFX_ENTRY_POINT pEntryPoints[], mfxU32 &numEntryPoints);

  // request about readiness output any filter wo processing of input
  bool      IsReadyOutput( mfxRequestType requestType );
  mfxU32    GetFilterIndexStart( mfxRequestType requestType );

  mfxStatus PassThrough(mfxFrameInfo* In, mfxFrameInfo* Out, mfxU16 realOutPicStruct);

  // methods for crop processing
  mfxStatus SetCrop(mfxFrameSurface1 *in, mfxFrameSurface1 *out);

  mfxU32    GetNumConnections();

  // method of sync part of VPP. before processing we must know about output
  mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out );

  // creation/destroy of pipeline
  mfxStatus CreatePipeline(mfxFrameInfo* In, mfxFrameInfo* Out);
  mfxStatus DestroyPipeline( void );

  // creation/destroy frames pool for work with video memory [FastCopy]
  mfxStatus CreateInternalSystemFramesPool( mfxVideoParam *par );
  mfxStatus DestroyInternalSystemFramesPool( void );

  // creation/destroy frames pool for filter-to-filter connection
  mfxStatus CreateConnectionFramesPool( void );
  mfxStatus DestroyConnectionFramesPool( void );

  // creation/destroy work buffers required by filters
  mfxStatus CreateWorkBuffer( void );
  mfxStatus DestroyWorkBuffer( void );

  // methods for pre/post processing of input surface (copy d3d to sys, lock etc)
  mfxStatus PreProcessOfInputSurface(mfxFrameSurface1 *in, mfxFrameSurface1** ppOut);
  mfxStatus PostProcessOfInputSurface();

  // methods for pre/post processing of output surface (copy sys to d3d, lock etc)
  mfxStatus PreProcessOfOutputSurface(mfxFrameSurface1 *out, mfxFrameSurface1** ppOut);
  mfxStatus PostProcessOfOutputSurface(mfxFrameSurface1 *out, mfxFrameSurface1 *pOutSurf, mfxStatus sts);

  // should be called once for RunFrameVPP
  mfxStatus PreWork(mfxFrameSurface1* in, mfxFrameSurface1* out,
                    mfxFrameSurface1** pInSurf, mfxFrameSurface1** pOutSurf);
  mfxStatus PostWork(mfxFrameSurface1* in, mfxFrameSurface1* out,
                     mfxFrameSurface1* pInSurf, mfxFrameSurface1* pOutSurf,
                     mfxStatus processingSts);

  mfxStatus RunVPPTask(mfxFrameSurface1 *in, mfxFrameSurface1 *out, FilterVPP::InternalParam *pParam );

  mfxStatus ResetTaskCounters();

  // VideoVPP
  virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux);

    // frames pool for filter-to-filter connection
    DynamicFramesPool m_connectionFramesPool[MAX_NUM_VPP_FILTERS-1];

    FilterVPP*          m_ppFilters[MAX_NUM_VPP_FILTERS];

    struct MultiThreadModeVPP {
      // used in Pre and Post work
      bool              bUseInput;
      mfxI32            preWorkIsStarted;
      mfxI32            preWorkIsDone;
      mfxI32            postWorkIsStarted;
      mfxI32            postWorkIsDone;
      mfxU32            maxNumThreads;
      mfxU32            fIndx;
      FilterVPP*        pCurFilter; // current filter for multithreading

      mfxFrameSurface1* pInSurf;        // global pointers for all threads
      mfxFrameSurface1* pOutSurf;
      mfxFrameSurface1* pipelineInSurf;
      mfxFrameSurface1* pipelineOutSurf;

      MultiThreadModeVPP( void )
          : bUseInput(false)
          , preWorkIsStarted(0)
          , preWorkIsDone(0)
          , postWorkIsStarted(0)
          , postWorkIsDone(0)
          , maxNumThreads(0)
          , fIndx(0)
          , pCurFilter(NULL)
          , pInSurf(NULL)
          , pOutSurf(NULL)
          , pipelineInSurf(NULL)
          , pipelineOutSurf(NULL){ };

  } m_threadModeVPP;

  // frames pool for optimization work with video memory
  DynamicFramesPool m_internalSystemFramesPool[2];
  bool              m_bDoFastCopyFlag[2];

  DynamicFramesPool m_externalFramesPool[2];

  // work buffer is required by filters
  mfxMemId      m_memIdWorkBuf;

  struct AsyncParams
  {
      mfxFrameSurface1 *surf_in;
      mfxFrameSurface1 *surf_out;
      mfxExtVppAuxData *aux;
      mfxU16  inputPicStruct;
      mfxU64  inputTimeStamp;
  };

  // internal VPP parameters
  FilterVPP::InternalParam m_internalParam;
protected:
    mfxStatus ConfigureExecuteParams(mfxVideoParam *par);
};

#endif

mfxStatus RunFrameVPPRoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
mfxStatus CompleteFrameVPPRoutine(void *pState, void *pParam, mfxStatus taskRes);

VideoVPPBase* CreateAndInitVPPImpl(mfxVideoParam *par, VideoCORE *core, mfxStatus *mfxSts);

#endif // __MFX_VPP_SW_H

#endif // MFX_ENABLE_VPP
/* EOF */
