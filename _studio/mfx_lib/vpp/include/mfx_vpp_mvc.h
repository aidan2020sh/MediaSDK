/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 Intel Corporation. All Rights Reserved.
//
//
//          Video Pre Processing( MVC wrapper)
//
*/
/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_MVC_H
#define __MFX_VPP_MVC_H

#include <memory>
#include <map>

#include "mfxvideo++int.h"
#include "mfx_task.h"
#include "mfx_vpp_defs.h"
#include "mfx_vpp_base.h"
#include "mfx_vpp_service.h"
#include "mfx_vpp_sw.h"

using namespace std;

namespace MfxVideoProcessing
{
    class ImplementationMvc : public VideoVPP 
    {
    public:

        static mfxStatus Query(VideoCORE* core, mfxVideoParam *in, mfxVideoParam *out)
        {
            return VideoVPPSW::Query( core, in, out);
        }

        static mfxStatus QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request);

        ImplementationMvc(VideoCORE *core);
        virtual ~ImplementationMvc();

        // VideoVPP
        virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux);

        // VideoBase methods
        virtual mfxStatus Reset(mfxVideoParam *par);
        virtual mfxStatus Close(void);
        virtual mfxStatus Init(mfxVideoParam *par);

        virtual mfxStatus GetVideoParam(
            mfxVideoParam *par);

        virtual mfxStatus GetVPPStat(
            mfxVPPStat *stat);

        virtual mfxStatus VppFrameCheck(
            mfxFrameSurface1 *in, 
            mfxFrameSurface1 *out, 
            mfxExtVppAuxData *aux,
            MFX_ENTRY_POINT pEntryPoint[], 
            mfxU32 &numEntryPoints);

        virtual mfxStatus VppFrameCheck(
            mfxFrameSurface1 *, 
            mfxFrameSurface1 *)
        {
            return MFX_ERR_UNSUPPORTED;
        }

        virtual mfxTaskThreadingPolicy GetThreadingPolicy(void);

        // multi threading of SW_VPP functions
        mfxStatus RunVPPTask(
            mfxFrameSurface1 *in, 
            mfxFrameSurface1 *out, 
            FilterVPP::InternalParam *pParam );

        mfxStatus ResetTaskCounters();


    private:

        bool       m_bInit;
        bool       m_bMultiViewMode;
        VideoCORE* m_core;

        typedef map<mfxU16, VideoVPPSW*> mfxMultiViewVPP;
        typedef mfxMultiViewVPP::iterator mfxMultiViewVPP_Iterator;

        mfxMultiViewVPP_Iterator m_iteratorVPP;
        mfxMultiViewVPP m_VPP;
    };

}; // namespace MfxVideoProcessing

#endif // __MFX_VPP_MVC_H
#endif // MFX_ENABLE_VPP
/* EOF */
