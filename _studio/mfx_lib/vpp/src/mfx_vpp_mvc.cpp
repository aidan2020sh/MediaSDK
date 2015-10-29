/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//
//
//         Video Pre Processing (MVC implementation)
//
*/

#include "math.h"
#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#include "mfx_enc_common.h"
#include "mfx_session.h"
#include "mfxmvc.h"
#include "mfxsvc.h"

#include "mfx_vpp_utils.h"
#include "mfx_vpp_mvc.h"

using namespace MfxVideoProcessing;

mfxStatus ImplementationMvc::QueryIOSurf(VideoCORE* core, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    mfxStatus sts = VideoVPPSW::QueryIOSurf(core, par, request);
        
    // in case of multiview we should correct result
    if( MFX_ERR_NONE == sts || MFX_WRN_PARTIAL_ACCELERATION == sts )
    {
        mfxExtBuffer* pHint = NULL;
        GetFilterParam(par, MFX_EXTBUFF_MVC_SEQ_DESC, &pHint);

        if( pHint )
        {
            mfxExtMVCSeqDesc* pHintMVC = (mfxExtMVCSeqDesc*)pHint;

            request[VPP_IN].NumFrameMin       = (mfxU16)(request[VPP_IN].NumFrameMin * pHintMVC->NumView);
            request[VPP_IN].NumFrameSuggested = (mfxU16)(request[VPP_IN].NumFrameSuggested * pHintMVC->NumView);

            request[VPP_OUT].NumFrameMin       = (mfxU16)(request[VPP_OUT].NumFrameMin * pHintMVC->NumView);
            request[VPP_OUT].NumFrameSuggested = (mfxU16)(request[VPP_OUT].NumFrameSuggested * pHintMVC->NumView);
        }
    }

    return sts;

} // mfxStatus ImplementationMvc::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request, const mfxU32 adapterNum)


ImplementationMvc::ImplementationMvc(VideoCORE *core)
: m_bInit( false )
, m_bMultiViewMode(false)
, m_core( core )
, m_iteratorVPP( )
{

} // ImplementationMvc::ImplementationMvc(VideoCORE *core)


ImplementationMvc::~ImplementationMvc()
{
    Close();

} // ImplementationMvc::~ImplementationMvc()


mfxStatus ImplementationMvc::Init(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    if( m_bInit )
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    // [1] multi-view Configuration
    mfxU32 viewCount = 1; // default
    mfxExtMVCSeqDesc* pHintMVC = NULL;

    mfxExtBuffer* pHint = NULL;
    GetFilterParam(par, MFX_EXTBUFF_MVC_SEQ_DESC, &pHint);

    m_bMultiViewMode = false; // really, constructor, destructor & close set one to false, but KW mark as issue

    if( pHint )
    {
        pHintMVC = (mfxExtMVCSeqDesc*)pHint;
        viewCount = pHintMVC->NumView;
        m_bMultiViewMode = true;
    }


    // [2] initialization
    mfxStatus mfxSts = MFX_ERR_NONE;
    mfxStatus intSts = MFX_ERR_NONE;

    for( mfxU32 viewIndx = 0; viewIndx < viewCount; viewIndx++ )
    {
        mfxU16 viewId = 0;

        if(m_bMultiViewMode)
        {
            viewId = (pHintMVC->View)? pHintMVC->View[viewIndx].ViewId : (mfxU16)viewIndx;
            if(viewId >= 1024) return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        VideoVPPSW* pNewVPP = new VideoVPPSW(m_core, &mfxSts);
        //MFX_CHECK_STS( mfxSts );
        if( mfxSts != MFX_ERR_NONE )
        {
            delete pNewVPP;
            pNewVPP = NULL;

            return mfxSts;
        }

        pair<mfxMultiViewVPP_Iterator, bool> insertSts = m_VPP.insert( make_pair(viewId, pNewVPP) );
        if( !insertSts.second )
        {
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        }

        mfxSts = m_VPP[viewId]->Init( par );
        if(MFX_WRN_PARTIAL_ACCELERATION == mfxSts || MFX_WRN_FILTER_SKIPPED == mfxSts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfxSts)
        {
            intSts = mfxSts;
            mfxSts = MFX_ERR_NONE;
        }

        MFX_CHECK_STS( mfxSts );
    }

    m_iteratorVPP = m_VPP.begin(); // save for EndOfStream processing

    m_bInit = true;

    return intSts;

} // mfxStatus ImplementationMvc::Init(mfxVideoParam *par)


mfxStatus ImplementationMvc::Reset(mfxVideoParam *par)
{
    MFX_CHECK_NULL_PTR1( par );

    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus sts = MFX_ERR_NONE;
    mfxStatus internalSts = MFX_ERR_NONE;

    sts = CheckExtParam(m_core, par->ExtParam,  par->NumExtParam);
    if( MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_WRN_FILTER_SKIPPED == sts )
    {
        internalSts = sts;
        sts = MFX_ERR_NONE;
    }
    MFX_CHECK_STS(sts);

    mfxMultiViewVPP_Iterator curVPP;
    for( curVPP = m_VPP.begin(); curVPP != m_VPP.end(); curVPP++)
    {
        sts = (curVPP->second)->Reset( par );
        if(MFX_WRN_PARTIAL_ACCELERATION == sts || MFX_WRN_FILTER_SKIPPED == sts || MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
        {
            internalSts = sts;
            sts = MFX_ERR_NONE;
        }
        MFX_CHECK_STS( sts );
    }

    m_iteratorVPP = m_VPP.begin(); // save for EndOfStream processing

    return (MFX_ERR_NONE == sts) ? internalSts : sts;

} // mfxStatus ImplementationMvc::Reset(mfxVideoParam *par)


mfxStatus ImplementationMvc::GetVideoParam(mfxVideoParam *par)
{
    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if(m_VPP.empty())
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus mfxSts = (m_VPP.begin()->second)->GetVideoParam( par );

    return mfxSts;

} // mfxStatus VideoVPPMain::GetVideoParam(mfxVideoParam *par)


mfxStatus ImplementationMvc::GetVPPStat(mfxVPPStat *stat)
{
    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    if(m_VPP.empty())
    {
        return MFX_ERR_NOT_INITIALIZED;
    }

    mfxStatus mfxSts = (m_VPP.begin()->second)->GetVPPStat( stat );

    return mfxSts;

} // mfxStatus ImplementationMvc::GetVPPStat(mfxVPPStat *stat)


mfxTaskThreadingPolicy ImplementationMvc::GetThreadingPolicy(void)
{
    return MFX_TASK_THREADING_INTRA;

} // mfxTaskThreadingPolicy ImplementationMvc::GetThreadingPolicy(void)


mfxStatus ImplementationMvc::VppFrameCheck(
    mfxFrameSurface1 *in,
    mfxFrameSurface1 *out,
    mfxExtVppAuxData *aux,
    MFX_ENTRY_POINT pEntryPoint[],
    mfxU32 &numEntryPoints)
{
    MFX_CHECK_NULL_PTR1( out );

    mfxStatus mfxSts;

    if( !m_bInit )
    {
        return MFX_ERR_NOT_INITIALIZED;
    }


    mfxU16 viewId = 0;

    if( m_bMultiViewMode )
    {
        // check in->info.viewId belong (set of viewId on init)
        viewId = (in) ? in->Info.FrameId.ViewId : m_iteratorVPP->first;
        if( m_VPP.find(viewId) == m_VPP.end() )
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    mfxSts = m_VPP[ viewId ]->VppFrameCheck( in, out, aux, pEntryPoint, numEntryPoints );

    if( m_bMultiViewMode )
    {
        // pass through viewID
        if( MFX_ERR_NONE == mfxSts || MFX_ERR_MORE_SURFACE == mfxSts )
        {
            out->Info.FrameId.ViewId = viewId;
        }

        // update iterator for EndOfStream processing
        m_iteratorVPP++;

        if( m_iteratorVPP == m_VPP.end() )
        {
            m_iteratorVPP = m_VPP.begin();
        }
    }

    return mfxSts;

} // mfxStatus ImplementationMvc::VppFrameCheck(...)

// depreciated
mfxStatus ImplementationMvc::RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux)
{
    mfxU16 viewId = 0;

    if( m_bMultiViewMode )
    {
        // check in->info.viewId belong (set of viewId on init)
        viewId = (in) ? in->Info.FrameId.ViewId : out->Info.FrameId.ViewId; // it is correct because outputViewID filled by Check()
        if( m_VPP.find(viewId) == m_VPP.end() )
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    mfxStatus mfxSts = m_VPP[ viewId ]->RunFrameVPP( in, out, aux);

    return mfxSts;

} // mfxStatus ImplementationMvc::RunFrameVPP(mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux)


mfxStatus ImplementationMvc::Close(void)
{
 //VPP_CHECK_NOT_INITIALIZED;
    if( !m_bInit )
    {
        return MFX_ERR_NONE;
    }

    mfxMultiViewVPP_Iterator curVPP;
    for( curVPP = m_VPP.begin(); curVPP != m_VPP.end(); curVPP++)
    {
        (curVPP->second)->Close( );
        delete (curVPP->second);
        //MFX_CHECK_STS( mfxSts );
    }

    m_VPP.erase( m_VPP.begin(), m_VPP.end() );

    m_bMultiViewMode = false;
    m_bInit = false;

    return MFX_ERR_NONE;

} // mfxStatus ImplementationMvc::Close( void )

#endif // MFX_ENABLE_VPP
/* EOF */
