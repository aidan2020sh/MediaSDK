/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_rnd_wrapper.h"
#include "vm_thread.h"
#include "mfx_itime.h"
#include "vm_event.h"

/*
    feeds actual render with frames at required FPS
    receives frames as burst chunks, and blocks untill chunks is emptied
    uses m_pRender * 2 internal buffer dueto maintaining output fps
*/
class BurstRender : public InterfaceProxy<IMFXVideoRender>
{
public:
    BurstRender(bool bVerbose, mfxU16 nBurstLen, ITime *pTime, IMFXVideoRender * pDecorated);
    virtual ~BurstRender();

    //init will be called in target thread for simplicity only this function for now
    virtual mfxStatus GetHandle(mfxHandleType type, mfxHDL *pHdl);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request);

protected:
    static mfxU32 VM_THREAD_CALLCONVENTION ThreadRoutine(void * pthis);

    virtual void      RenderThread();
    virtual mfxStatus CloseLocal();

    enum RenderState
    {
        STATE_IDLE, //at initial buffering we wont render frames until everything buffered
        STATE_RENDERING_ONLY,
        STATE_RENDERING_AND_DECODING,
        STATE_DISPATCH_GETHANDLE, //dispatches calls to gethandle from worker thread
    } m_state;

    struct GetHandleDispatchParams
    {
        mfxHandleType type;
        mfxHDL *pHdl;
    } m_forGetHandleDispatch;

    mfxU16 m_nBurstLen;
    mfxU16 m_nDecodeResume;//decoder resumes if buffer emptied to this value
    bool   m_bStop;
    bool   m_bVerbose;
    
    mfxStatus m_AsyncStatus;
    
    std::list<std::pair<mfxFrameSurface1*, mfxEncodeCtrl*> > m_bufferedFrames;
    vm_thread m_Thread;
    vm_mutex  m_FramesAcess;
    vm_event  m_shouldStartDecode;
    ITime   * m_pTime;
};