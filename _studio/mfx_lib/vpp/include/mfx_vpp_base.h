/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2010 - 2011 Intel Corporation. All Rights Reserved.
//
//
//                     basic class for (SW)VPP filter
//
*/
/* ****************************************************************************** */

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)

#ifndef __MFX_VPP_BASE_H
#define __MFX_VPP_BASE_H

#include <limits.h>

#include "mfxvideo++int.h"
#include "mfx_vpp_defs.h"
#include "vm_interlocked.h"

class FilterVPP {
public:

    typedef struct {
        mfxU16  inPicStruct;
        mfxU16  outPicStruct;
        mfxExtVppAuxData *aux;
        mfxU64 inTimeStamp; // advFRC mode only
        mfxU64 outTimeStamp; // advFRC mode only

    } InternalParam;

    virtual ~FilterVPP(void) {}

    virtual mfxStatus Init(mfxFrameInfo* In, mfxFrameInfo* Out)=0;

    virtual mfxStatus Close(void)=0;

    virtual mfxStatus SetParam( mfxExtBuffer* pHint ) = 0;

    virtual mfxStatus RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam)=0;

    virtual mfxStatus RunFrameVPPTask(mfxFrameSurface1 *in, mfxFrameSurface1 *out, InternalParam* pParam)
    {
        mfxU32 numStarted_cur = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_numStarted));
        if (numStarted_cur > m_maxNumRegions)
            return MFX_TASK_BUSY;

        mfxStatus mfxRes = RunFrameVPP(in, out, pParam);
        // MFX_ERR_MORE_SURFACE may come from FRC only
        VPP_IGNORE_MFX_STS(mfxRes, MFX_ERR_MORE_SURFACE);

        if ((mfxRes != MFX_ERR_MORE_DATA) &&
            (mfxRes != MFX_ERR_NONE))
        {   // there's a bug
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        mfxU32 numFinished_cur = vm_interlocked_inc32(reinterpret_cast<volatile Ipp32u *>(&m_numFinished));
        if (numFinished_cur != m_maxNumRegions)
        {   // a region was successfully performed
            // mfxRes must be always ERR_NONE here
            return MFX_TASK_WORKING;
        }

        return mfxRes;  // ERR_NONE or MORE_SURFACE
    }

    virtual mfxStatus Reset(mfxVideoParam *par) = 0;

    virtual mfxU8 GetInFramesCount( void )=0;
    virtual mfxU8 GetOutFramesCount( void )=0;

    // work buffer management Functions should be called AFTER initialization
    virtual mfxStatus GetBufferSize( mfxU32* pBufferSize ) = 0;
    virtual mfxStatus SetBuffer( mfxU8* pBuffer ) = 0;


    // simulation of async VPP processing in sync part.
    // this function changes!!! filter state
    virtual mfxStatus CheckProduceOutput(mfxFrameSurface1 *in, mfxFrameSurface1 *out ) = 0;

    // this function doesn't change filter state
    virtual bool      IsReadyOutput( mfxRequestType requestType ) = 0;

    mfxStatus GetFrameInfo(mfxFrameInfo* In, mfxFrameInfo* Out)
    {
        VPP_CHECK_NOT_INITIALIZED;

        if( NULL != In )
        {
            *In = m_errPrtctState.In;
        }

        if( NULL != Out )
        {
            *Out = m_errPrtctState.Out;
        }

        return MFX_ERR_NONE;
    }

    void ResetFilterTaskCounters()
    {
        m_numStarted = 0;
        m_numFinished = 0;
    }

protected:

    typedef struct
    {
        mfxFrameInfo    In;
        mfxFrameInfo    Out;
        bool            isInited;
        bool            isFirstFrameProcessed;

    } sErrPrtctState;

    // pass through for simple (no delay) filter
    virtual mfxStatus PassThrough(mfxFrameSurface1 *in, mfxFrameSurface1 *out)
    {
        if( in && out )
        {
            // pass through in according with new spec requirement
            out->Data.FrameOrder   = in->Data.FrameOrder;
            out->Data.TimeStamp    = in->Data.TimeStamp;
            out->Info.AspectRatioH = in->Info.AspectRatioH;
            out->Info.AspectRatioW = in->Info.AspectRatioW;
            out->Info.PicStruct    = in->Info.PicStruct;
        }

        return MFX_ERR_NONE;
    }

    FilterVPP(void)
    {
        m_numStarted = 0;
        m_numFinished = 0;
        m_maxNumRegions = 1;
        m_hRegionIn = 0;
        m_hRegionOut = 0;
    }

    VideoCORE*      m_core;
    sErrPrtctState  m_errPrtctState;
    mfxU32          m_numStarted;     // number of started regions
    mfxU32          m_numFinished;    // number of finished regions
    mfxU32          m_maxNumRegions;  // 1 region - 1 thread
    mfxU32          m_hRegionIn;      // region height of input surface
    mfxU32          m_hRegionOut;     // region height of output surface
};

#endif // __MFX_VPP_BASE_H
#endif // MFX_ENABLE_VPP
/*EOF*/
