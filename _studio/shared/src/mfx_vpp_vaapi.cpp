/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_VPP)
#if defined (MFX_VA_LINUX)

#include "mfx_vpp_defs.h"
#include "mfx_vpp_vaapi.h"
#include "mfx_utils.h"
#include "libmfx_core_vaapi.h"
#include "ippcore.h"
#include "ippi.h"

enum QueryStatus
{
    VPREP_GPU_READY         =   0,
    VPREP_GPU_BUSY          =   1,
    VPREP_GPU_NOT_REACHED   =   2,
    VPREP_GPU_FAILED        =   3
};

//////////////////////////////////////////////////////////////////////////
using namespace MfxHwVideoProcessing;

static float convertValue(const float OldMin,const float OldMax,const float NewMin,const float NewMax,const float OldValue)
{
    if((0 == NewMin) && (0 == NewMax)) return OldValue; //workaround
    float oldRange = OldMax - OldMin;
    float newRange = NewMax - NewMin;
    return (((OldValue - OldMin) * newRange) / oldRange) + NewMin;
}

#define DEFAULT_HUE 0
#define DEFAULT_SATURATION 1
#define DEFAULT_CONTRAST 1
#define DEFAULT_BRIGHTNESS 0

VAAPIVideoProcessing::VAAPIVideoProcessing():
  m_bRunning(false)
, m_core(NULL)
, m_vaDisplay(0)
, m_vaConfig(0)
, m_vaContextVPP(0)
, m_denoiseFilterID(VA_INVALID_ID)
, m_deintFilterID(VA_INVALID_ID)
, m_numFilterBufs(0)
, m_primarySurface4Composition(NULL)
{

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));

    m_cachedReadyTaskIndex.clear();
    m_feedbackCache.clear();

} // VAAPIVideoProcessing::VAAPIVideoProcessing()


VAAPIVideoProcessing::~VAAPIVideoProcessing()
{
    Close();

} // VAAPIVideoProcessing::~VAAPIVideoProcessing()

mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxVideoParam* pParams, bool /*isTemporal*/)
{
    MFX_CHECK_NULL_PTR1( core );

    VAAPIVideoCORE* hwCore = dynamic_cast<VAAPIVideoCORE*>(core);

    MFX_CHECK_NULL_PTR1( hwCore );

    mfxStatus sts = hwCore->GetVAService( &m_vaDisplay);
    MFX_CHECK_STS( sts );

    sts = Init( &m_vaDisplay, pParams);

    MFX_CHECK_STS(sts);

    m_cachedReadyTaskIndex.clear();

    m_core = core;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::CreateDevice(VideoCORE * core, mfxInitParams* pParams)


mfxStatus VAAPIVideoProcessing::DestroyDevice(void)
{
    mfxStatus sts = Close();

    return sts;

} // mfxStatus VAAPIVideoProcessing::DestroyDevice(void)


mfxStatus VAAPIVideoProcessing::Close(void)
{
    if (m_primarySurface4Composition != NULL)
    {
        vaDestroySurfaces(m_vaDisplay,m_primarySurface4Composition,1);
        free(m_primarySurface4Composition);
        m_primarySurface4Composition = NULL;
    }
    if( m_vaContextVPP )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextVPP );
        m_vaContextVPP = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }

    if( VA_INVALID_ID != m_denoiseFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_denoiseFilterID);
    }

    if( VA_INVALID_ID != m_deintFilterID )
    {
        vaDestroyBuffer(m_vaDisplay, m_deintFilterID);
    }

    for(int i = 0; i < VAProcFilterCount; i++)
        m_filterBufs[i] = VA_INVALID_ID;

    m_denoiseFilterID   = VA_INVALID_ID;
    m_deintFilterID     = VA_INVALID_ID;

    memset( (void*)&m_denoiseCaps, 0, sizeof(m_denoiseCaps));
    memset( (void*)&m_deinterlacingCaps, 0, sizeof(m_deinterlacingCaps));

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Close(void)

mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)
{
    if(false == m_bRunning)
    {
        MFX_CHECK_NULL_PTR1( pVADisplay );
        MFX_CHECK_NULL_PTR1( pParams );

        m_cachedReadyTaskIndex.clear();

        VAEntrypoint* va_entrypoints = NULL;
        VAStatus vaSts;
        int va_max_num_entrypoints   = vaMaxNumEntrypoints(m_vaDisplay);
        if(va_max_num_entrypoints)
            va_entrypoints = (VAEntrypoint*)ippsMalloc_8u(va_max_num_entrypoints*sizeof(VAEntrypoint));
        else
            return MFX_ERR_DEVICE_FAILED;

        mfxI32 entrypointsCount = 0, entrypointsIndx = 0;

        vaSts = vaQueryConfigEntrypoints(m_vaDisplay,
                                            VAProfileNone,
                                            va_entrypoints,
                                            &entrypointsCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for( entrypointsIndx = 0; entrypointsIndx < entrypointsCount; entrypointsIndx++ )
        {
            if( VAEntrypointVideoProc == va_entrypoints[entrypointsIndx] )
            {
                m_bRunning = true;
                break;
            }
        }
        ippsFree(va_entrypoints);

        if( !m_bRunning )
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Configuration
        VAConfigAttrib va_attributes;
        vaSts = vaGetConfigAttributes(m_vaDisplay, VAProfileNone, VAEntrypointVideoProc, &va_attributes, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaCreateConfig( m_vaDisplay,
                                VAProfileNone,
                                VAEntrypointVideoProc,
                                &va_attributes,
                                1,
                                &m_vaConfig);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        // Context
        int width = pParams->vpp.Out.Width;
        int height = pParams->vpp.Out.Height;

        vaSts = vaCreateContext(m_vaDisplay,
                                m_vaConfig,
                                width,
                                height,
                                VA_PROGRESSIVE,
                                0, 0,
                                &m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::Init(_mfxPlatformAccelerationService* pVADisplay, mfxVideoParam* pParams)


mfxStatus VAAPIVideoProcessing::Register(
    mfxHDLPair* /*pSurfaces*/,
    mfxU32 /*num*/,
    BOOL /*bRegister*/)
{
    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Register(_mfxPlatformVideoSurface *pSurfaces, ...)


mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)
{
    VAStatus vaSts;
    memset(&caps,  0, sizeof(mfxVppCaps));

    VAProcFilterType filters[VAProcFilterCount];
    mfxU32 num_filters = VAProcFilterCount;

    vaSts = vaQueryVideoProcFilters(m_vaDisplay, m_vaContextVPP, filters, &num_filters);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_denoise_caps = 1;
    vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterNoiseReduction,
                               &m_denoiseCaps, &num_denoise_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mfxU32 num_deinterlacing_caps = VAProcDeinterlacingCount;
    vaQueryVideoProcFilterCaps(m_vaDisplay,
                               m_vaContextVPP,
                               VAProcFilterDeinterlacing,
                               &m_deinterlacingCaps, &num_deinterlacing_caps);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for( mfxU32 filtersIndx = 0; filtersIndx < num_filters; filtersIndx++ )
    {
        if (filters[filtersIndx])
        {
            switch (filters[filtersIndx])
            {
                case VAProcFilterNoiseReduction:
                    caps.uDenoiseFilter = 1;
                    break;
                case VAProcFilterDeinterlacing:
                    for (mfxU32 i = 0; i < num_deinterlacing_caps; i++)
                    {
                        if (VAProcDeinterlacingBob == m_deinterlacingCaps[i].type)
                            caps.uSimpleDI = 1;
                        if (VAProcDeinterlacingWeave == m_deinterlacingCaps[i].type           ||
                            VAProcDeinterlacingMotionAdaptive == m_deinterlacingCaps[i].type  ||
                            VAProcDeinterlacingMotionCompensated == m_deinterlacingCaps[i].type)
                            caps.uAdvancedDI = 1;
                    }
                    break;
                case VAProcFilterColorBalance:
                    caps.uProcampFilter = 1;
                    break;
               default:
                    break;
            }
        }
    }

    caps.uMaxWidth  = 4096;
    caps.uMaxHeight = 4096;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryCapabilities(mfxVppCaps& caps)

mfxStatus VAAPIVideoProcessing::Execute(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    /* There is a special case for composition */
    mfxStatus mfxSts = MFX_ERR_NONE;
    if (pParams->bComposite)
    {
        mfxSts = Execute_Composition(pParams);
        return mfxSts;
    }

    if (VA_INVALID_ID == m_deintFilterID)
    {
        if (pParams->iDeinterlacingAlgorithm)
        {
            VAProcFilterParameterBufferDeinterlacing deint;
            deint.type  = VAProcFilterDeinterlacing;
            deint.flags = 0;
            //WA for VPG driver. Need to rewrite it with caps usage when driver begins to return a correct list of supported DI algorithms
#ifndef MFX_VA_ANDROID
            if (MFX_DEINTERLACING_BOB == pParams->iDeinterlacingAlgorithm)
            {
                deint.algorithm = VAProcDeinterlacingBob;
            }
            else
            {
                deint.algorithm = VAProcDeinterlacingMotionAdaptive;
            }

            mfxDrvSurface* pRefSurf_frameInfo = &(pParams->pRefSurfaces[0]);
            switch (pRefSurf_frameInfo->frameInfo.PicStruct)
            {
                case MFX_PICSTRUCT_PROGRESSIVE:
                    deint.flags = VA_DEINTERLACING_ONE_FIELD;
                    break;

                case MFX_PICSTRUCT_FIELD_TFF:
                    break; // don't set anything, see comment in va_vpp.h

                case MFX_PICSTRUCT_FIELD_BFF:
                    deint.flags = VA_DEINTERLACING_BOTTOM_FIELD;
                    break;
            }
#endif
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextVPP,
                                   VAProcFilterParameterBufferType,
                                   sizeof(deint), 1,
                                   &deint, &m_deintFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_deintFilterID;
        }
    }

    if (VA_INVALID_ID == m_denoiseFilterID)
    {
        if (pParams->denoiseFactor || true == pParams->bDenoiseAutoAdjust)
        {
            VAProcFilterParameterBuffer denoise;
            denoise.type  = VAProcFilterNoiseReduction;
            if(pParams->bDenoiseAutoAdjust)
                denoise.value = m_denoiseCaps.range.default_value;
            else
                denoise.value = convertValue(0,
                                            100,
                                              m_denoiseCaps.range.min_value,
                                              m_denoiseCaps.range.max_value,
                                              pParams->denoiseFactor);
            vaSts = vaCreateBuffer((void*)m_vaDisplay,
                          m_vaContextVPP,
                          VAProcFilterParameterBufferType,
                          sizeof(denoise), 1,
                          &denoise, &m_denoiseFilterID);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            m_filterBufs[m_numFilterBufs++] = m_denoiseFilterID;
        }
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    // mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;
    mfxU32 refCount = (mfxU32) pParams->fwdRefCount;

    //m_pipelineParam.resize(SampleCount);
    //m_pipelineParam.clear();
    m_pipelineParam.resize(pParams->refCount);
    //m_pipelineParamID.resize(SampleCount);
    //m_pipelineParamID.clear();
    m_pipelineParamID.resize(pParams->refCount, VA_INVALID_ID);

    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        VASurfaceID* srf = (VASurfaceID*)(pRefSurf->hdl.first);
        m_pipelineParam[refIdx].surface = *srf;

        // source cropping
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        input_region[refIdx].y   = inInfo->CropY;
        input_region[refIdx].x   = inInfo->CropX;
        input_region[refIdx].height = inInfo->CropH;
        input_region[refIdx].width  = inInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        // destination cropping
        mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        output_region[refIdx].y  = outInfo->CropY;
        output_region[refIdx].x   = outInfo->CropX;
        output_region[refIdx].height= outInfo->CropH;
        output_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        m_pipelineParam[refIdx].output_background_color = 0xff000000; // black for ARGB

        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:  //VA_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardBT601;
            break;
        }

        mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
        switch (targetFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardBT601;
            break;
        }

        //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES

        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_PROGRESSIVE:
                m_pipelineParam[refIdx].filter_flags = VA_FRAME_PICTURE;
                break;
            case MFX_PICSTRUCT_FIELD_TFF:
                m_pipelineParam[refIdx].filter_flags = VA_TOP_FIELD;
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                m_pipelineParam[refIdx].filter_flags = VA_BOTTOM_FIELD;
                break;
        }

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = m_numFilterBufs;
        /* Special case for composition:
         * as primary surface processed as sub-stream
         * pipeline and filter properties should be *_FAST */
        if (pParams->bComposite)
        {
            m_pipelineParam[refIdx].num_filters  = 0;
            m_pipelineParam[refIdx].pipeline_flags |= VA_PROC_PIPELINE_FAST;
            m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        for( refIdx = 0; refIdx < SampleCount; refIdx++ )
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    /* Final End Picture call... if there is no composition case ! */
    if (!pParams->bComposite)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    else /* Special case for Composition */
    {
        unsigned int uBeginPictureCounter = 0;
        std::vector<VAProcPipelineParameterBuffer> m_pipelineParamComp;
        std::vector<VABufferID> m_pipelineParamCompID;
        /* for new buffers for Begin Picture*/
        m_pipelineParamComp.resize(pParams->fwdRefCount/7);
        m_pipelineParamCompID.resize(pParams->fwdRefCount/7);

        /* pParams->fwdRefCount actually is number of sub stream*/
        for( refIdx = 1; refIdx <= refCount; refIdx++ )
        {
            /*for frames 8, 15, 22, 29,... */
            if ((refIdx != 1) && ((refIdx %7) == 1) )
            {
                m_pipelineParam[refIdx].output_background_color = 0xff000000;
                vaSts = vaBeginPicture(m_vaDisplay,
                                    m_vaContextVPP,
                                    *outputSurface);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                /*to copy initial properties of primary surface... */
                m_pipelineParamComp[uBeginPictureCounter] = m_pipelineParam[0];
                /* ... and to In-place output*/
                m_pipelineParamComp[uBeginPictureCounter].surface = *outputSurface;
                //m_pipelineParam[0].surface = *outputSurface;

                vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextVPP,
                                    VAProcPipelineParameterBufferType,
                                    sizeof(VAProcPipelineParameterBuffer),
                                    1,
                                    &m_pipelineParamComp[uBeginPictureCounter],
                                    &m_pipelineParamCompID[uBeginPictureCounter]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
                vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamCompID[uBeginPictureCounter], 1);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            m_pipelineParam[refIdx] = m_pipelineParam[0];
            mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx]);

            VASurfaceID* srf = (VASurfaceID*)(pRefSurf->hdl.first);
            m_pipelineParam[refIdx].surface = *srf;

            /* to process input parameters of sub stream:
             * crop info and original size*/
            mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
            input_region[refIdx].y   = inInfo->CropY;
            input_region[refIdx].x   = inInfo->CropX;
            input_region[refIdx].height = inInfo->CropH;
            input_region[refIdx].width  = inInfo->CropW;
            m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

            /* to process output parameters of sub stream:
             *  position and destination size */
            output_region[refIdx].y  = pParams->dstRects[refIdx].DstY;
            output_region[refIdx].x   = pParams->dstRects[refIdx].DstX;
            output_region[refIdx].height= pParams->dstRects[refIdx].DstH;
            output_region[refIdx].width  = pParams->dstRects[refIdx].DstW;
            m_pipelineParam[refIdx].output_region = &output_region[refIdx];

            //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES
            m_pipelineParam[refIdx].pipeline_flags  |= VA_PROC_PIPELINE_FAST;
            m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;

            m_pipelineParam[refIdx].filters  = m_filterBufs;
            m_pipelineParam[refIdx].num_filters  = 0;

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcPipelineParameterBufferType,
                                sizeof(VAProcPipelineParameterBuffer),
                                1,
                                &m_pipelineParam[refIdx],
                                &m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            /*for frames 7, 14, 21, ...
             * or for the last frame*/
            if ( ((refIdx % 7) ==0) || (refCount == refIdx) )
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
                vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

        } /* for( refIdx = 1; refIdx <= (pParams->fwdRefCount); refIdx++ )*/

        for( refIdx = 0; refIdx < uBeginPictureCounter; refIdx++ )
        {
            if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
            {
                vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamCompID[refIdx]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                m_pipelineParamCompID[refIdx] = VA_INVALID_ID;
            }
        }
    } /* if (!pParams->bComposite) */

    for( refIdx = 0; refIdx < m_pipelineParamID.size(); refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_pipelineParamID[refIdx] = VA_INVALID_ID;
        }
    }

    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute(FASTCOMP_BLT_PARAMS *pVideoCompositingBlt)

mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "VPP Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    VASurfaceAttrib attrib;
    std::vector<VABlendState> blend_state;

    MFX_CHECK_NULL_PTR1( pParams );
    MFX_CHECK_NULL_PTR1( pParams->targetSurface.hdl.first );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces );
    MFX_CHECK_NULL_PTR1( pParams->pRefSurfaces[0].hdl.first );

    if (m_primarySurface4Composition == NULL)
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[0]);
        mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        VAImage imagePrimarySurface;
        mfxU8* pPrimarySurfaceBuffer;

        m_primarySurface4Composition = (VASurfaceID*)calloc(1,sizeof(VASurfaceID));
        /* KW fix, but it is true, required to check, is memory allocated o not  */
        if (m_primarySurface4Composition == NULL)
            return MFX_ERR_MEMORY_ALLOC;

        attrib.type = VASurfaceAttribPixelFormat;
        attrib.value.type = VAGenericValueTypeInteger;
        if (inInfo->FourCC == MFX_FOURCC_NV12)
        {
            attrib.value.value.i = VA_FOURCC_NV12;
        }
        else if (inInfo->FourCC == MFX_FOURCC_RGB4)
        {
            attrib.value.value.i = VA_FOURCC_ARGB;
        }
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

        /* NB! Second parameter is always VA_RT_FORMAT_YUV420 even for RGB surfaces!!! */
        vaSts = vaCreateSurfaces(m_vaDisplay, VA_RT_FORMAT_YUV420, inInfo->Width, inInfo->Height,
                m_primarySurface4Composition, 1, &attrib, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaDeriveImage(m_vaDisplay, *m_primarySurface4Composition, &imagePrimarySurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaMapBuffer(m_vaDisplay, imagePrimarySurface.buf, (void **) &pPrimarySurfaceBuffer);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        IppStatus ippSts = ippStsNoErr;
        IppiSize roiSize;
        roiSize.width = imagePrimarySurface.width;
        roiSize.height = imagePrimarySurface.height;

        if (imagePrimarySurface.format.fourcc == VA_FOURCC_ARGB)
        {
            const Ipp8u backgroundValues[4] = { (Ipp8u)((pParams->iBackgroundColor &0xff000000) >>24),
                                                (Ipp8u)((pParams->iBackgroundColor &0x00ff0000) >>16),
                                                (Ipp8u)((pParams->iBackgroundColor &0x0000ff00) >> 8),
                                                (Ipp8u) (pParams->iBackgroundColor &0x000000ff)    };
            ippSts = ippiSet_8u_C4R(backgroundValues, pPrimarySurfaceBuffer,
                                imagePrimarySurface.pitches[0], roiSize);
            MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
        }
        if (imagePrimarySurface.format.fourcc == VA_FOURCC_NV12)
        {
            Ipp8u valueY =  (Ipp8u)((pParams->iBackgroundColor &0x00ff0000) >>16);
            Ipp16s valueUV = (Ipp16s)(pParams->iBackgroundColor &0x0000ffff);
            ippSts = ippiSet_8u_C1R(valueY, pPrimarySurfaceBuffer, imagePrimarySurface.pitches[0], roiSize);
            MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
            //
            roiSize.height = roiSize.height/2;
            ippSts = ippiSet_16s_C1R(valueUV, (Ipp16s *)(pPrimarySurfaceBuffer + imagePrimarySurface.offsets[1]),
                                            imagePrimarySurface.pitches[1], roiSize);
            MFX_CHECK(ippStsNoErr == ippSts, MFX_ERR_DEVICE_FAILED);
        }

        vaSts = vaUnmapBuffer(m_vaDisplay, imagePrimarySurface.buf);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        vaSts = vaDestroyImage(m_vaDisplay, imagePrimarySurface.image_id);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    /* pParams->refCount is total number of processing surfaces:
     * in case of composition this is primary + sub streams*/

    // mfxU32 SampleCount = pParams->refCount;
    // WA:
    mfxU32 SampleCount = 1;
    mfxU32 refIdx = 0;
    mfxU32 refCount = (mfxU32) pParams->fwdRefCount;

    //m_pipelineParam.resize(SampleCount);
    //m_pipelineParam.clear();
    m_pipelineParam.resize(pParams->refCount + 1);
    //m_pipelineParamID.resize(SampleCount);
    //m_pipelineParamID.clear();
    m_pipelineParamID.resize(pParams->refCount + 1, VA_INVALID_ID);
    blend_state.resize(pParams->refCount + 1);

    std::vector<VARectangle> input_region;
    input_region.resize(pParams->refCount + 1);
    std::vector<VARectangle> output_region;
    output_region.resize(pParams->refCount + 1);
    VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);

    for( refIdx = 0; refIdx < SampleCount; refIdx++ )
    {
        mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx]);

        //VASurfaceID* srf = (VASurfaceID*)(pRefSurf->hdl.first);
        //m_pipelineParam[refIdx].surface = *srf;
        m_pipelineParam[refIdx].surface = *m_primarySurface4Composition;
        //VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
        //m_pipelineParam[refIdx].surface = *outputSurface;

        // source cropping
        //mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
        mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        input_region[refIdx].y   = 0;
        input_region[refIdx].x   = 0;
        input_region[refIdx].height = outInfo->CropH;
        input_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

        // destination cropping
        //mfxFrameInfo *outInfo = &(pParams->targetSurface.frameInfo);
        output_region[refIdx].y  = 0; //outInfo->CropY;
        output_region[refIdx].x   = 0; //outInfo->CropX;
        output_region[refIdx].height= outInfo->CropH;
        output_region[refIdx].width  = outInfo->CropW;
        m_pipelineParam[refIdx].output_region = &output_region[refIdx];

        m_pipelineParam[refIdx].output_background_color = pParams->iBackgroundColor;

        mfxU32  refFourcc = pRefSurf->frameInfo.FourCC;
        switch (refFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:  //VA_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].surface_color_standard = VAProcColorStandardBT601;
            break;
        }

        mfxU32  targetFourcc = pParams->targetSurface.frameInfo.FourCC;
        switch (targetFourcc)
        {
        case MFX_FOURCC_RGB4:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardNone;
            break;
        case MFX_FOURCC_NV12:
        default:
            m_pipelineParam[refIdx].output_color_standard = VAProcColorStandardBT601;
            break;
        }

        //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES

        switch (pRefSurf->frameInfo.PicStruct)
        {
            case MFX_PICSTRUCT_PROGRESSIVE:
                m_pipelineParam[refIdx].filter_flags = VA_FRAME_PICTURE;
                break;
            case MFX_PICSTRUCT_FIELD_TFF:
                m_pipelineParam[refIdx].filter_flags = VA_TOP_FIELD;
                break;
            case MFX_PICSTRUCT_FIELD_BFF:
                m_pipelineParam[refIdx].filter_flags = VA_BOTTOM_FIELD;
                break;
        }

        /* Global alpha and luma key can not be enabled together*/
        if (pParams->dstRects[refIdx].GlobalAlphaEnable !=0)
        {
            blend_state[refIdx].flags = VA_BLEND_GLOBAL_ALPHA;
            blend_state[refIdx].global_alpha = ((float)pParams->dstRects[refIdx].GlobalAlpha) /255;
        }
        /* Luma color key  for YUV surfaces only.
         * And Premultiplied alpha blending for RGBA surfaces only.
         * So, these two flags can't combine together  */
        if ((pParams->dstRects[refIdx].LumaKeyEnable != 0) &&
            (pParams->dstRects[refIdx].PixelAlphaEnable == 0) )
        {
            blend_state[refIdx].flags |= VA_BLEND_LUMA_KEY;
            blend_state[refIdx].min_luma = pParams->dstRects[refIdx].LumaKeyMin;
            blend_state[refIdx].max_luma = pParams->dstRects[refIdx].LumaKeyMax;
        }
        if ((pParams->dstRects[refIdx].LumaKeyEnable == 0 ) &&
            (pParams->dstRects[refIdx].PixelAlphaEnable != 0 ) )
        {
            blend_state[refIdx].flags |= VA_BLEND_PREMULTIPLIED_ALPHA;
        }

        m_pipelineParam[refIdx].filters  = m_filterBufs;
        m_pipelineParam[refIdx].num_filters  = m_numFilterBufs;
        /* Special case for composition:
         * as primary surface processed as sub-stream
         * pipeline and filter properties should be *_FAST */
        if (pParams->bComposite)
        {
            m_pipelineParam[refIdx].num_filters  = 0;
            m_pipelineParam[refIdx].pipeline_flags |= VA_PROC_PIPELINE_FAST;
            m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;

            if ((pParams->dstRects[refIdx].GlobalAlphaEnable != 0) ||
                    (pParams->dstRects[refIdx].LumaKeyEnable != 0) ||
                    (pParams->dstRects[refIdx].PixelAlphaEnable != 0))
            {
                m_pipelineParam[refIdx].blend_state = &blend_state[refIdx];
            }
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextVPP,
                            VAProcPipelineParameterBufferType,
                            sizeof(VAProcPipelineParameterBuffer),
                            1,
                            &m_pipelineParam[refIdx],
                            &m_pipelineParamID[refIdx]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    //VASurfaceID *outputSurface = (VASurfaceID*)(pParams->targetSurface.hdl.first);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
        vaSts = vaBeginPicture(m_vaDisplay,
                            m_vaContextVPP,
                            *outputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
        for( refIdx = 0; refIdx < SampleCount; refIdx++ )
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    /* Final End Picture call... if there is no composition case ! */
    if (!pParams->bComposite)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    else /* Special case for Composition */
    {
        unsigned int uBeginPictureCounter = 0;
        std::vector<VAProcPipelineParameterBuffer> m_pipelineParamComp;
        std::vector<VABufferID> m_pipelineParamCompID;
        /* for new buffers for Begin Picture*/
        m_pipelineParamComp.resize(pParams->fwdRefCount/7);
        m_pipelineParamCompID.resize(pParams->fwdRefCount/7);

        /* pParams->fwdRefCount actually is number of sub stream*/
        for( refIdx = 1; refIdx <= (refCount + 1); refIdx++ )
        {
            /*for frames 8, 15, 22, 29,... */
            if ((refIdx != 1) && ((refIdx %7) == 1) )
            {
                m_pipelineParam[refIdx].output_background_color = pParams->iBackgroundColor;
                vaSts = vaBeginPicture(m_vaDisplay,
                                    m_vaContextVPP,
                                    *outputSurface);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                /*to copy initial properties of primary surface... */
                m_pipelineParamComp[uBeginPictureCounter] = m_pipelineParam[0];
                /* ... and to In-place output*/
                m_pipelineParamComp[uBeginPictureCounter].surface = *outputSurface;
                //m_pipelineParam[0].surface = *outputSurface;

                vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextVPP,
                                    VAProcPipelineParameterBufferType,
                                    sizeof(VAProcPipelineParameterBuffer),
                                    1,
                                    &m_pipelineParamComp[uBeginPictureCounter],
                                    &m_pipelineParamCompID[uBeginPictureCounter]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaBeginPicture");
                vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamCompID[uBeginPictureCounter], 1);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            m_pipelineParam[refIdx] = m_pipelineParam[0];
            mfxDrvSurface* pRefSurf = &(pParams->pRefSurfaces[refIdx-1]);

            VASurfaceID* srf = (VASurfaceID*)(pRefSurf->hdl.first);
            m_pipelineParam[refIdx].surface = *srf;

            /* to process input parameters of sub stream:
             * crop info and original size*/
            mfxFrameInfo *inInfo = &(pRefSurf->frameInfo);
            input_region[refIdx].y   = inInfo->CropY;
            input_region[refIdx].x   = inInfo->CropX;
            input_region[refIdx].height = inInfo->CropH;
            input_region[refIdx].width  = inInfo->CropW;
            m_pipelineParam[refIdx].surface_region = &input_region[refIdx];

            /* to process output parameters of sub stream:
             *  position and destination size */
            output_region[refIdx].y  = pParams->dstRects[refIdx-1].DstY;
            output_region[refIdx].x   = pParams->dstRects[refIdx-1].DstX;
            output_region[refIdx].height= pParams->dstRects[refIdx-1].DstH;
            output_region[refIdx].width  = pParams->dstRects[refIdx-1].DstW;
            m_pipelineParam[refIdx].output_region = &output_region[refIdx];

            /* Global alpha and luma key can not be enabled together*/
            /* Global alpha and luma key can not be enabled together*/
            if (pParams->dstRects[refIdx-1].GlobalAlphaEnable !=0)
            {
                blend_state[refIdx].flags = VA_BLEND_GLOBAL_ALPHA;
                blend_state[refIdx].global_alpha = ((float)pParams->dstRects[refIdx-1].GlobalAlpha) /255;
            }
            /* Luma color key  for YUV surfaces only.
             * And Premultiplied alpha blending for RGBA surfaces only.
             * So, these two flags can't combine together  */
            if ((pParams->dstRects[refIdx-1].LumaKeyEnable != 0) &&
                (pParams->dstRects[refIdx-1].PixelAlphaEnable == 0) )
            {
                blend_state[refIdx].flags |= VA_BLEND_LUMA_KEY;
                blend_state[refIdx].min_luma = pParams->dstRects[refIdx-1].LumaKeyMin;
                blend_state[refIdx].max_luma = pParams->dstRects[refIdx-1].LumaKeyMax;
            }
            if ((pParams->dstRects[refIdx-1].LumaKeyEnable == 0 ) &&
                (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0 ) )
            {
                blend_state[refIdx].flags |= VA_BLEND_PREMULTIPLIED_ALPHA;
            }
            if ((pParams->dstRects[refIdx-1].GlobalAlphaEnable != 0) ||
                    (pParams->dstRects[refIdx-1].LumaKeyEnable != 0) ||
                    (pParams->dstRects[refIdx-1].PixelAlphaEnable != 0))
            {
                m_pipelineParam[refIdx].blend_state = &blend_state[refIdx];
            }




            //m_pipelineParam[refIdx].pipeline_flags = ?? //VA_PROC_PIPELINE_FAST or VA_PROC_PIPELINE_SUBPICTURES
            m_pipelineParam[refIdx].pipeline_flags  |= VA_PROC_PIPELINE_FAST;
            m_pipelineParam[refIdx].filter_flags    |= VA_FILTER_SCALING_FAST;

            m_pipelineParam[refIdx].filters  = m_filterBufs;
            m_pipelineParam[refIdx].num_filters  = 0;

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextVPP,
                                VAProcPipelineParameterBufferType,
                                sizeof(VAProcPipelineParameterBuffer),
                                1,
                                &m_pipelineParam[refIdx],
                                &m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaRenderPicture");
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextVPP, &m_pipelineParamID[refIdx], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            /*for frames 7, 14, 21, ...
             * or for the last frame*/
            if ( ((refIdx % 7) ==0) || ((refCount + 1) == refIdx) )
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "vaEndPicture");
                vaSts = vaEndPicture(m_vaDisplay, m_vaContextVPP);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

        } /* for( refIdx = 1; refIdx <= (pParams->fwdRefCount); refIdx++ )*/

        for( refIdx = 0; refIdx < uBeginPictureCounter; refIdx++ )
        {
            if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
            {
                vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamCompID[refIdx]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                m_pipelineParamCompID[refIdx] = VA_INVALID_ID;
            }
        }
    } /* if (!pParams->bComposite) */

    for( refIdx = 0; refIdx < m_pipelineParamID.size(); refIdx++ )
    {
        if ( m_pipelineParamID[refIdx] != VA_INVALID_ID)
        {
            vaSts = vaDestroyBuffer(m_vaDisplay, m_pipelineParamID[refIdx]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            m_pipelineParamID[refIdx] = VA_INVALID_ID;
        }
    }

    // (3) info needed for sync operation
    //-------------------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback; // {surface & number_of_task}
        currentFeedback.surface = *outputSurface;
        currentFeedback.number = pParams->statusReportID;
        m_feedbackCache.push_back(currentFeedback);
    }

    return MFX_ERR_NONE;
} // mfxStatus VAAPIVideoProcessing::Execute_Composition(mfxExecuteParams *pParams)



mfxStatus VAAPIVideoProcessing::QueryTaskStatus(mfxU32 taskIndex)
{
    VAStatus vaSts;
    FASTCOMP_QUERY_STATUS queryStatus;

    // (1) find params (sutface & number) are required by feedbackNumber
    //-----------------------------------------------
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        std::vector<ExtVASurface>::iterator iter = m_feedbackCache.begin();
        while(iter != m_feedbackCache.end())
        {
            ExtVASurface currentFeedback = *iter;

            VASurfaceStatus surfSts = VASurfaceSkipped;

            vaSts = vaQuerySurfaceStatus(m_vaDisplay,  currentFeedback.surface, &surfSts);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            switch (surfSts)
            {
                case VASurfaceReady:
                    queryStatus.QueryStatusID = currentFeedback.number;
                    queryStatus.Status = VPREP_GPU_READY;
                    m_cachedReadyTaskIndex.insert(queryStatus.QueryStatusID);
                    iter = m_feedbackCache.erase(iter);
                    break;
                case VASurfaceRendering:
                case VASurfaceDisplaying:
                    ++iter;
                    break;
                case VASurfaceSkipped:
                default:
                    assert(!"bad feedback status");
                    return MFX_ERR_DEVICE_FAILED;
            }
        }

        std::set<mfxU32>::iterator iterator = m_cachedReadyTaskIndex.find(taskIndex);

        if (m_cachedReadyTaskIndex.end() == iterator)
        {
            return MFX_TASK_BUSY;
        }

        m_cachedReadyTaskIndex.erase(iterator);
    }

    return MFX_TASK_DONE;

} // mfxStatus VAAPIVideoProcessing::QueryTaskStatus(mfxU32 taskIndex)

mfxStatus VAAPIVideoProcessing::QueryTaskStatus(FASTCOMP_QUERY_STATUS *pQueryStatus, mfxU32 numStructures)
{
//     VASurfaceID waitSurface;
//
//     // (1) find params (sutface & number) are required by feedbackNumber
//     //-----------------------------------------------
//     {
//         UMC::AutomaticUMCMutex guard(m_guard);
//
//         bool isFound  = false;
//         int num_element = m_feedbackCache.size();
//         for( mfxU32 indxSurf = 0; indxSurf < num_element; indxSurf++ )
//         {
//             //ExtVASurface currentFeedback = m_feedbackCache.pop();
//             ExtVASurface currentFeedback = m_feedbackCache[indxSurf];
//
//             // (2) Syncronization by output (target surface)
//             //-----------------------------------------------
//             VAStatus vaSts = vaSyncSurface(m_vaDisplay, currentFeedback.surface);
//             MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//
//             VASurfaceStatus surfSts = VASurfaceReady;
//
//             vaSts = vaQuerySurfaceStatus(m_vaDisplay,  currentFeedback.surface, &surfSts);
//             MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//
//             // update for comp vpp_hw
//
//             pQueryStatus[indxSurf].StatusReportID = currentFeedback.number;
//             pQueryStatus[indxSurf].Status = VPREP_GPU_READY;
//
//             m_feedbackCache.erase( m_feedbackCache.begin() );
//         }
//
//     }
    return MFX_ERR_NONE;

} // mfxStatus VAAPIVideoProcessing::QueryTaskStatus(PREPROC_QUERY_STATUS *pQueryStatus, mfxU32 numStructures)

#endif // #if defined (MFX_VA_LINUX)
#endif // #if defined (MFX_VPP_ENABLE)
/* EOF */
