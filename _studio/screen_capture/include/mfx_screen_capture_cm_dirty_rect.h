/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015 Intel Corporation. All Rights Reserved.

File Name: mfx_screen_capture_dirty_rect.h

\* ****************************************************************************** */

#if !defined(__MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__)
#define __MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__

#include <list>
#include <vector>
#include <map>
#include "mfxstructures.h"
#include "mfxplugin.h"
#include "mfxsc.h"
#include "umc_mutex.h"

#include "mfx_screen_capture_ddi.h"

#include "cmrt_cross_platform.h"

namespace MfxCapture
{

struct TaskContext
{
    mfxU8 busy;
    CmQueue *      pQueue;
    CmThreadSpace* pThreadSpace;
    CmTask*        pTask;
    CmKernel*      pKernel;
    struct {
        CmSurface2DUP* cmSurf;
        mfxU8*         physBuffer;
        mfxU32         physBufferSz;
        mfxU32         physBufferPitch;
    } roiMap;
};

class CmDirtyRectFilter : public DirtyRectFilter
{
public:
    CmDirtyRectFilter(const mfxCoreInterface* _core);
    virtual ~CmDirtyRectFilter();

    mfxStatus Init(const mfxVideoParam* par, bool isSysMem, bool isOpaque = false);
    mfxStatus Init(const mfxFrameInfo& in, const mfxFrameInfo& out);
    mfxStatus GetParam(mfxFrameInfo& in, mfxFrameInfo& out);
    mfxStatus RunFrameVPP(mfxFrameSurface1& in, mfxFrameSurface1& out);

    mfxStatus Close();
protected:
    UMC::Mutex m_guard;

    //library stuff
    const mfxCoreInterface* m_pmfxCore;
    mfxHandleType           m_mfxDeviceType;
    mfxHDL                  m_mfxDeviceHdl;

    //CM stuff
    CmDevice*                m_pCMDevice;
    std::vector<CmProgram *> m_vPrograms;
    CmQueue*                 m_pQueue;

    //library-CM
    std::map<const mfxFrameSurface1*,CmSurface2D*> m_CMSurfaces;

    //internal resources and configs
    mfxU16 m_width ;
    mfxU16 m_height;
    bool   m_bSysMem;
    bool   m_bOpaqMem;
    std::vector<TaskContext*> m_vpTasks;

    //functions
    void          FreeSurfs();
    TaskContext*  GetFreeTask();
    void          ReleaseTask(TaskContext*& task);

    CmSurface2D*  GetCMSurface(const mfxFrameSurface1* mfxSurf);
    SurfaceIndex* GetCMSurfaceIndex(const mfxFrameSurface1* mfxSurf);
    mfxStatus     CMCopySysToGpu(CmSurface2D* cmDst, mfxFrameSurface1* mfxSrc);

private:
    //prohobit copy constructor
    CmDirtyRectFilter(const CmDirtyRectFilter& that);
    //prohibit assignment operator
    CmDirtyRectFilter& operator=(const CmDirtyRectFilter&);
};

template<typename T>
T* GetExtendedBuffer(const mfxU32& BufferId, const mfxFrameSurface1* surf);

} //namespace MfxCapture
#endif //#if !defined(__MFX_SCREEN_CAPTURE_CM_DIRTY_RECT_H__)