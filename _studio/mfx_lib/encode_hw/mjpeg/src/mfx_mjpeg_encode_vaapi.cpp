/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2016 Intel Corporation. All Rights Reserved.
//
//
//          MJPEG encoder VAAPI
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"

#include "mfx_mjpeg_encode_vaapi.h"
#include "libmfx_core_interface.h"

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_vaapi.h"

using namespace MfxHwMJpegEncode;

VAAPIEncoder::VAAPIEncoder()
 : m_core(NULL)
 , m_width(-1)
 , m_height(-1)
 , m_vaDisplay(0)
 , m_vaContextEncode(VA_INVALID_ID)
 , m_vaConfig(VA_INVALID_ID)
 , m_qmBufferId(VA_INVALID_ID)
 , m_htBufferId(VA_INVALID_ID)
 , m_scanBufferId(VA_INVALID_ID)
 , m_ppsBufferId(VA_INVALID_ID)
{
}

VAAPIEncoder::~VAAPIEncoder()
{
    Destroy();
}

mfxStatus VAAPIEncoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;
    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);
    if(hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                VAProfileJPEGBaseline,
                &pEntrypoints[0],
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        if( VAEntrypointEncPicture == pEntrypoints[entrypointsIndx] )
        {
            bEncodeEnable = true;
            break;
        }
    }
    if( !bEncodeEnable )
    {
        return MFX_ERR_DEVICE_FAILED;
    }
    m_width  = width;
    m_height = height;

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::CreateAccelerationService(mfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    MFX_CHECK_WITH_ASSERT(par.mfx.CodecProfile == MFX_PROFILE_JPEG_BASELINE, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQueryConfigEntrypoints(
                m_vaDisplay,
                VAProfileJPEGBaseline,
                &pEntrypoints[0],
                &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    bool bEncodeEnable = false;
    for( entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++ )
    {
        if( VAEntrypointEncPicture == pEntrypoints[entrypointsIndx] )
        {
            bEncodeEnable = true;
            break;
        }
    }
    if( !bEncodeEnable )
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    // Configuration
    VAConfigAttrib attrib;

    attrib.type = VAConfigAttribRTFormat;

    vaSts = vaGetConfigAttributes(m_vaDisplay,
                          VAProfileJPEGBaseline,
                          VAEntrypointEncPicture,
                          &attrib, 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (!(attrib.value & VA_RT_FORMAT_YUV420) || !(attrib.value & VA_RT_FORMAT_YUV422) ) //to be do
        return MFX_ERR_DEVICE_FAILED;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        VAProfileJPEGBaseline,
        VAEntrypointEncPicture,
        NULL,
        0,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_width,
        m_height,
        VA_PROGRESSIVE,
        NULL,
        0,
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryBitstreamBufferInfo(mfxFrameAllocRequest& request)
{

    // request linear buffer
    request.Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    request.AllocId = m_vaContextEncode;
    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryEncodeCaps(JpegEncCaps & caps)
{
    MFX_CHECK_NULL_PTR1(m_core);
    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);

    if (hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    memset(&caps, 0, sizeof(caps));
    caps.Baseline = 1;
    caps.Sequential = 1;
    caps.Huffman = 1;

    caps.NonInterleaved = 0;
    caps.Interleaved = 1;

    caps.SampleBitDepth = 8;
    caps.MaxNumComponent = 3;
    caps.MaxNumScan = 1;
    caps.MaxNumHuffTable = 2;
    caps.MaxNumQuantTable = 2;

    VAStatus vaSts;
    vaExtQueryEncCapabilities pfnVaExtQueryCaps = NULL;
    pfnVaExtQueryCaps = (vaExtQueryEncCapabilities)vaGetLibFunc(m_vaDisplay,VPG_EXT_QUERY_ENC_CAPS);
    if (pfnVaExtQueryCaps)
    {
        VAEncQueryCapabilities VaEncCaps;
        memset(&VaEncCaps, 0, sizeof(VaEncCaps));
        VaEncCaps.size = sizeof(VAEncQueryCapabilities);
        vaSts = pfnVaExtQueryCaps(m_vaDisplay, VAProfileJPEGBaseline, &VaEncCaps);
        if (vaSts == VA_STATUS_ERROR_UNSUPPORTED_PROFILE)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        caps.MaxPicWidth  = VaEncCaps.MaxPicWidth;
        caps.MaxPicHeight = VaEncCaps.MaxPicHeight;
    }
    else
    {
        // Configuration
        VAConfigAttrib attrib;

        attrib.type = VAConfigAttribMaxPictureWidth;
        vaSts = vaGetConfigAttributes(m_vaDisplay,
                              VAProfileJPEGBaseline,
                              VAEntrypointEncPicture,
                              &attrib, 1);
        caps.MaxPicWidth      = attrib.value;

        attrib.type = VAConfigAttribMaxPictureHeight;
        vaSts = vaGetConfigAttributes(m_vaDisplay,
                              VAProfileJPEGBaseline,
                              VAEntrypointEncPicture,
                              &attrib, 1);
        caps.MaxPicHeight     = attrib.value;
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::RegisterBitstreamBuffer(mfxFrameAllocResponse& response)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;
    pQueue = &m_bsQueue;

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK( response.mids, MFX_ERR_NULL_PTR );

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++)
        {
            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number  = i;
            extSurf.surface = *pSurface;
            pQueue->push_back( extSurf );
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Execute(DdiTask &task, mfxHDL surface)
{
    VAStatus vaSts;
    ExecuteBuffers *pExecuteBuffers = task.m_pDdiData;
    ExtVASurface codedbuffer = m_bsQueue[task.m_idxBS];
    pExecuteBuffers->m_pps.coded_buf = (VABufferID)codedbuffer.surface;

    vaSts = vaBeginPicture(m_vaDisplay, m_vaContextEncode, *(VASurfaceID*)surface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    DestroyBuffers();
    vaSts = vaCreateBuffer(m_vaDisplay, m_vaContextEncode, VAEncPictureParameterBufferType, sizeof(VAEncPictureParameterBufferJPEG), 1, &pExecuteBuffers->m_pps, &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if(pExecuteBuffers->m_dqt_list.size())
    {
        // only the first dq table has been handled
        vaSts = vaCreateBuffer(m_vaDisplay, m_vaContextEncode, VAQMatrixBufferType, sizeof(VAQMatrixBufferJPEG), 1, &pExecuteBuffers->m_dqt_list[0], &m_qmBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    if(pExecuteBuffers->m_dht_list.size())
    {
        // only the first huffmn table has been handled
        vaSts = vaCreateBuffer(m_vaDisplay, m_vaContextEncode, VAHuffmanTableBufferType, sizeof(VAHuffmanTableBufferJPEGBaseline), 1, &pExecuteBuffers->m_dht_list[0], &m_htBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    if(pExecuteBuffers->m_payload_list.size())
    {
        m_appBufferIds.resize(pExecuteBuffers->m_payload_list.size());
        for( mfxU8 index = 0; index < pExecuteBuffers->m_payload_list.size(); index++)
        {
            vaSts = vaCreateBuffer(m_vaDisplay,
                                   m_vaContextEncode,
                                   VAEncPackedHeaderDataBufferType,
                                   pExecuteBuffers->m_payload_list[index].length,
                                   1,
                                   pExecuteBuffers->m_payload_list[index].data,
                                   &m_appBufferIds[index]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    if(pExecuteBuffers->m_scan_list.size() == 1)
    {
        vaSts = vaCreateBuffer(m_vaDisplay, m_vaContextEncode, VAEncSliceParameterBufferType, sizeof(VAEncSliceParameterBufferJPEG), 1, &pExecuteBuffers->m_scan_list[0], &m_scanBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    else
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    vaSts = vaRenderPicture(m_vaDisplay, m_vaContextEncode, (VABufferID *)&m_ppsBufferId, 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if(m_qmBufferId != VA_INVALID_ID)
    {
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextEncode, (VABufferID *)&m_qmBufferId, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    if( m_htBufferId != VA_INVALID_ID)
    {
        vaSts = vaRenderPicture(m_vaDisplay, m_vaContextEncode, (VABufferID *)&m_htBufferId, 1);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    if(m_appBufferIds.size())
    {
        for( mfxU8 index = 0; index < m_appBufferIds.size(); index++)
        {
            vaSts = vaRenderPicture(m_vaDisplay, m_vaContextEncode, (VABufferID *)&m_appBufferIds[index], 1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    vaSts = vaRenderPicture(m_vaDisplay, m_vaContextEncode, (VABufferID *)&m_scanBufferId, 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber;
        currentFeedback.surface = *(VASurfaceID*)surface;
        currentFeedback.idxBs   = task.m_idxBS;
        currentFeedback.size    = 0;

        m_feedbackCache.push_back( currentFeedback );
    }
    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::QueryStatus(DdiTask & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 waitSize;
    mfxU32 indxSurf;
    UMC::AutomaticUMCMutex guard(m_guard);

    for( indxSurf = 0; indxSurf < m_feedbackCache.size(); indxSurf++ )
    {
        ExtVASurface currentFeedback = m_feedbackCache[ indxSurf ];
        if( currentFeedback.number == task.m_statusReportNumber )
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            waitSize    = currentFeedback.size;
            isFound  = true;
            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    // find used bitstream
    VABufferID codedBuffer;
    if( waitIdxBs < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }
    if (waitSurface != VA_INVALID_SURFACE) // Not skipped frame
    {
        VASurfaceStatus surfSts = VASurfaceSkipped;

#if defined(SYNCHRONIZATION_BY_VA_SYNC_SURFACE)

        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
        guard.Unlock();

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaSyncSurface");
            vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
            // following code is workaround:
            // because of driver bug it could happen that decoding error will not be returned after decoder sync
            // and will be returned at subsequent encoder sync instead
            // just ignore VA_STATUS_ERROR_DECODING_ERROR in encoder
            if (vaSts == VA_STATUS_ERROR_DECODING_ERROR)
                vaSts = VA_STATUS_SUCCESS;
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        surfSts = VASurfaceReady;

#else

        vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        if (VASurfaceReady == surfSts)
        {
            m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
            guard.Unlock();
        }

#endif
        switch (surfSts)
        {
            case VASurfaceReady:
                VACodedBufferSegment *codedBufferSegment;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        codedBuffer,
                        (void **)(&codedBufferSegment));

                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }
//
// TODO: Commented check is not correct for 422 and 444 (RGB) encodings, need rework
//
//                MFX_CHECK_WITH_ASSERT(codedBufferSegment->size < (m_width * m_height * 3 / 2), MFX_ERR_DEVICE_FAILED);
                task.m_bsDataLength = codedBufferSegment->size;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                    vaUnmapBuffer( m_vaDisplay, codedBuffer );
                }

                return MFX_ERR_NONE;

            case VASurfaceRendering:
            case VASurfaceDisplaying:
                return MFX_WRN_DEVICE_BUSY;

            case VASurfaceSkipped:
            default:
                assert(!"bad feedback status");
                return MFX_ERR_DEVICE_FAILED;
        }
    }
    else
    {
        task.m_bsDataLength = waitSize;
        m_feedbackCache.erase(m_feedbackCache.begin() + indxSurf);
    }

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::UpdateBitstream(
    mfxMemId       MemId,
    DdiTask      & task)
{
    mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    IppiSize     roi       = {(int)task.m_bsDataLength, 1};
    mfxFrameData bitstream = { };

    m_core->LockFrame(MemId, &bitstream);
    MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    IppStatus sts = ippiCopyManaged_8u_C1R(
        (Ipp8u *)bitstream.Y, task.m_bsDataLength,
        bsData, task.m_bsDataLength,
        roi, IPP_NONTEMPORAL_LOAD);
    assert(sts == ippStsNoErr);

    task.bs->DataLength += task.m_bsDataLength;
    m_core->UnlockFrame(MemId, &bitstream);

    return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::DestroyBuffers() {
    MFX_DESTROY_VABUFFER(m_qmBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_htBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_scanBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    for (size_t index = 0; index < m_appBufferIds.size(); index++)
    {
        MFX_DESTROY_VABUFFER(m_appBufferIds[index], m_vaDisplay);
    }
    m_appBufferIds.clear();
    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Destroy()
{
    mfxStatus sts = MFX_ERR_NONE;
    if( m_vaContextEncode )
    {
        vaDestroyContext( m_vaDisplay, m_vaContextEncode );
        m_vaContextEncode = 0;
    }

    if( m_vaConfig )
    {
        vaDestroyConfig( m_vaDisplay, m_vaConfig );
        m_vaConfig = 0;
    }
    m_bsQueue.clear();
    m_feedbackCache.clear();
    DestroyBuffers();
    return sts;
}

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_LINUX)
