// Copyright (c) 2015-2018 Intel Corporation
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

enum
{
    TMP_MFXSC_DIRTY_RECT_DEFAULT     = 0,
    TMP_MFXSC_DIRTY_RECT_EXT_16x16   = 1,
    TMP_MFXSC_DIRTY_RECT_EXT_32x32   = 2,
    TMP_MFXSC_DIRTY_RECT_EXT_64x64   = 3,
    TMP_MFXSC_DIRTY_RECT_EXT_128x128 = 4,
    TMP_MFXSC_DIRTY_RECT_EXT_256x256 = 5
};

struct intRect
{
    mfxU32  Left;
    mfxU32  Top;
    mfxU32  Right;
    mfxU32  Bottom;
};

struct intDirtyRect
{
    mfxU16      NumRect;
    intRect*    Rects;
};

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
        mfxU32         width;
        mfxU32         height;
        Ipp8u*         roiMAP;
        Ipp16s*                         distUpper;
        Ipp16s*                         distLower;
        Ipp16s*                         distLeft;
        Ipp16s*                         distRight;
        std::vector <intRect>           dirtyRects;
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
    mfxU32 m_mode;

    //functions
    void          FreeSurfs();
    TaskContext*  GetFreeTask();
    void          ReleaseTask(TaskContext*& task);

    CmSurface2D*  GetCMSurface(const mfxFrameSurface1* mfxSurf);
    SurfaceIndex* GetCMSurfaceIndex(const mfxFrameSurface1* mfxSurf);
    mfxStatus     CMCopySysToGpu(CmSurface2D* cmDst, mfxFrameSurface1* mfxSrc);

    mfxStatus BuildRectangles(Ipp8u* roiMap, IppiSize roi, const mfxU32 maxRectsN, std::vector <intRect>& dstRects);
    mfxStatus BuildRectanglesExt(Ipp8u* roiMap, Ipp16s* du, Ipp16s* dd, Ipp16s* dl, Ipp16s* dr, IppiSize roi, std::vector <intRect>& dstRects);

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