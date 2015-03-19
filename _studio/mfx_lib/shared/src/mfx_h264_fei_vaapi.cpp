/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015 Intel Corporation. All Rights Reserved.
//
//
//          H264 FEI VAAPI encoder
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_h264.h>
#include "mfxfei.h"
#include "libmfx_core_vaapi.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
//#include <va/vendor/va_intel_statistics.h>
#include "mfx_h264_preenc.h"
#endif // #if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

#if defined(_DEBUG)
//#define mdprintf fprintf
#define mdprintf(...)
#else
#define mdprintf(...)
#endif

using namespace MfxHwH264Encode;

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

VAAPIFEIPREENCEncoder::VAAPIFEIPREENCEncoder()
: VAAPIEncoder()
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPREENCEncoder::~VAAPIFEIPREENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPREENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPREENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_ENCPAK)
        return CreateENCPAKAccelerationService(par);
    else if (m_codingFunction == MFX_FEI_FUNCTION_PREENC)
        return CreatePREENCAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPREENCEncoder::CreatePREENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    bool bEncodeEnable = false;
    //entry point for preENC
    vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            VAProfileNone, //specific for statistic
            Begin(pEntrypoints),
            &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    for (entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++) {
        if (VAEntrypointStatisticsIntel == pEntrypoints[entrypointsIndx]) {
            bEncodeEnable = true;
            break;
        }
    }

    if (!bEncodeEnable)
        return MFX_ERR_DEVICE_FAILED;

    //check attributes of the configuration
    VAConfigAttrib attrib[2];
    //attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].type = (VAConfigAttribType)VAConfigAttribStatisticsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0], 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    //if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
    //    return MFX_ERR_DEVICE_FAILED;

    //attrib[0].value = VA_RT_FORMAT_YUV420;

    //check statistic attributes
    VAConfigAttribValStatisticsIntel statVal;
    statVal.value = attrib[0].value;
    //temp to be removed
    // But lets leave for a while
    mdprintf(stderr,"Statistics attributes:\n");
    mdprintf(stderr,"max_past_refs: %d\n", statVal.bits.max_num_past_references);
    mdprintf(stderr,"max_future_refs: %d\n", statVal.bits.max_num_future_references);
    mdprintf(stderr,"num_outputs: %d\n", statVal.bits.num_outputs);
    mdprintf(stderr,"interlaced: %d\n\n", statVal.bits.interlaced);
    //attrib[0].value |= ((2 - 1/*MV*/ - 1/*mb*/) << 8);

    vaSts = vaCreateConfig(
            m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0],
            1,
            &m_vaConfig); //don't configure stat attribs
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    //FillConstPartOfPps(par, m_pps);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIFEIPREENCEncoder::CreateENCPAKAccelerationService(MfxVideoParam const & par)
{
    if (0 == m_reconQueue.size()) {
        /* We need to pass reconstructed surfaces when call vaCreateContext().
         * Here we don't have this info.
         */
        m_videoParam = par;
        return MFX_ERR_NONE;
    }

    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    bool bEncodeEnable = false;
    //ENC/PAK entry point
    vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            Begin(pEntrypoints),
            &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++)
    {
        if (VAEntrypointEncFEIIntel == pEntrypoints[entrypointsIndx]) {
            bEncodeEnable = true;
            break;
        }
    }

    if (!bEncodeEnable)
        return MFX_ERR_DEVICE_FAILED;

    // Configuration, check for BRC?
    VAConfigAttrib attrib[2];

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            (VAEntrypoint)VAEntrypointEncFEIIntel,
            &attrib[0], 2);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
            m_vaDisplay,
            ConvertProfileTypeMFX2VAAPI(par.mfx.CodecProfile),
            (VAEntrypoint)VAEntrypointEncFEIIntel,
            attrib,
            2,
            &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIFEIPREENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIPREENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    //VAStatsStatisticsParameterBufferTypeIntel,
    //VAStatsStatisticsBufferTypeIntel,
    //VAStatsMotionVectorBufferTypeIntel,
    //VAStatsMVPredictorBufferTypeIntel,

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc Execute");

    VAStatus vaSts;
    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    //Add preENC buffers
    //preENC control
    VABufferID statMVid = VA_INVALID_ID;
    VABufferID statOUTid = VA_INVALID_ID;
    VABufferID mvPredid = VA_INVALID_ID;
    VABufferID qpid = VA_INVALID_ID;
    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    //buffer for MV output
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    //find output surfaces
    mfxExtFeiPreEncMV* mvsOut = NULL;
    mfxExtFeiPreEncMBStat* mbstatOut = NULL;
    for (int i = 0; i < out->NumExtParam; i++) {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV)
        {
            mvsOut = &((mfxExtFeiPreEncMV*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MB)
        {
            mbstatOut = &((mfxExtFeiPreEncMBStat*) (out->ExtParam[i]))[fieldId];
        }
    }

    //find in buffers
    mfxExtFeiPreEncCtrl* feiCtrl = NULL;
    mfxExtFeiEncQP* feiQP = NULL;
    mfxExtFeiPreEncMVPredictors* feiMVPred = NULL;
    for (int i = 0; i < in->NumExtParam; i++) {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_CTRL)
        {
            feiCtrl = &((mfxExtFeiPreEncCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
        {
            feiQP = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV_PRED)
        {
            feiMVPred = &((mfxExtFeiPreEncMVPredictors*) (in->ExtParam[i]))[fieldId];
        }
    }

    //should be adjusted

    VAStatsStatisticsParameter16x16Intel m_statParams;
    memset(&m_statParams, 0, sizeof (VAStatsStatisticsParameter16x16Intel));
    VABufferID statParamsId = VA_INVALID_ID;

    //task.m_yuv->Data.DataFlag
    //m_core->GetExternalFrameHDL(); //video mem
    if (feiCtrl != NULL) // KW fix actually
    {
        m_statParams.adaptive_search = feiCtrl->AdaptiveSearch;
        m_statParams.disable_statistics_output = (mbstatOut == NULL) || feiCtrl->DisableStatisticsOutput;
        m_statParams.disable_mv_output = (mvsOut == NULL) || feiCtrl->DisableMVOutput;
        m_statParams.mb_qp = (feiQP == NULL) && feiCtrl->MBQp;
        m_statParams.mv_predictor_ctrl = (feiMVPred == NULL) ? feiCtrl->MVPredictor : 0;
    }
    else
    {
        m_statParams.adaptive_search = 0;
        m_statParams.disable_statistics_output = 1;
        m_statParams.disable_mv_output = 1;
        m_statParams.mb_qp = 0;
        m_statParams.mv_predictor_ctrl = 0;
    } // if (feiCtrl != NULL) // KW fix actually


    VABufferID outBuffers[2];
    int numOutBufs = 0;
    outBuffers[0] = VA_INVALID_ID;
    outBuffers[1] = VA_INVALID_ID;

    if ((m_statParams.mv_predictor_ctrl) && (feiMVPred != NULL) && (feiMVPred->MB != NULL)) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMVPredictorBufferTypeIntel,
                sizeof (VAMotionVectorIntel),
                numMB,
                feiMVPred->MB,
                &mvPredid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.mv_predictor = mvPredid;
        configBuffers[buffersCount++] = mvPredid;
        mdprintf(stderr, "MVPred bufId=%d\n", mvPredid);
    }

    if ((m_statParams.mb_qp) && (feiQP != NULL) && (feiQP->QP != NULL)) {

        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264),
                numMB,
                feiQP->QP,
                &qpid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.qp = qpid;
        configBuffers[buffersCount++] = qpid;
        mdprintf(stderr, "Qp bufId=%d\n", qpid);
    }

    if (!m_statParams.disable_mv_output) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMotionVectorBufferTypeIntel,
                sizeof (VAMotionVectorIntel)*numMB * 16, //16 MV per MB
                1,
                NULL, //should be mapped later
                &statMVid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        outBuffers[numOutBufs++] = statMVid;
        mdprintf(stderr, "MV bufId=%d\n", statMVid);
        configBuffers[buffersCount++] = statMVid;
    }

    if (!m_statParams.disable_statistics_output) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsStatisticsBufferTypeIntel,
                sizeof (VAStatsStatistics16x16Intel) * numMB,
                1,
                NULL,
                &statOUTid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        outBuffers[numOutBufs++] = statOUTid;
        mdprintf(stderr, "StatOut bufId=%d\n", statOUTid);
        configBuffers[buffersCount++] = statOUTid;
    }

    m_statParams.frame_qp = feiCtrl->Qp;
    m_statParams.ft_enable = feiCtrl->FTEnable;
    m_statParams.num_past_references = in->NumFrameL0;
    //currently only video mem is used, all input surfaces should be in video mem
    m_statParams.past_references = NULL;
    //fprintf(stderr,"in vaid: %x ", *inputSurface);
    if (in->NumFrameL0)
    {
        VASurfaceID* l0surfs = new VASurfaceID [in->NumFrameL0];
        m_statParams.past_references = &l0surfs[0]; //L0 refs

        for (int i = 0; i < in->NumFrameL0; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L0Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            l0surfs[i] = *s;
        }
      //  fprintf(stderr,"l0vaid: %x ", l0surfs[0]);
    }
    m_statParams.num_future_references = in->NumFrameL1;
    m_statParams.future_references = NULL;
    if (in->NumFrameL1)
    {
        VASurfaceID* l1surfs = new VASurfaceID [in->NumFrameL1];
        m_statParams.future_references = &l1surfs[0]; //L0 refs
        for (int i = 0; i < in->NumFrameL1; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L1Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle;
            l1surfs[i] = *s;
        }
        //fprintf(stderr,"l1vaid: %x", l1surfs[0]);
    }

    //fprintf(stderr,"\n");
    m_statParams.input = *inputSurface;
    m_statParams.interlaced = 0;
    m_statParams.inter_sad = feiCtrl->InterSAD;
    m_statParams.intra_sad = feiCtrl->IntraSAD;
    m_statParams.len_sp = feiCtrl->LenSP;
    m_statParams.max_len_sp = feiCtrl->MaxLenSP;
    m_statParams.outputs = &outBuffers[0]; //bufIDs for outputs
    m_statParams.sub_pel_mode = feiCtrl->SubPelMode;
    m_statParams.sub_mb_part_mask = feiCtrl->SubMBPartMask;
    m_statParams.ref_height = feiCtrl->RefHeight;
    m_statParams.ref_width = feiCtrl->RefWidth;
    m_statParams.search_window = feiCtrl->SearchWindow;

#if 0
    fprintf(stderr, "\n**** stat params:\n");
    fprintf(stderr, "input=%d\n", m_statParams.input);
    fprintf(stderr, "past_references=0x%x [", m_statParams.past_references);
    for (int i = 0; i < m_statParams.num_past_references; i++)
        fprintf(stderr, " %d", m_statParams.past_references[i]);
    fprintf(stderr, " ]\n");
    fprintf(stderr, "num_past_references=%d\n", m_statParams.num_past_references);
    fprintf(stderr, "future_references=0x%x [", m_statParams.future_references);
    for (int i = 0; i < m_statParams.num_future_references; i++)
        fprintf(stderr, " %d", m_statParams.future_references[i]);
    fprintf(stderr, " ]\n");
    fprintf(stderr, "num_future_references=%d\n", m_statParams.num_future_references);
    fprintf(stderr, "outputs=0x%x [", m_statParams.outputs);
    for (int i = 0; i < 2; i++)
        fprintf(stderr, " %d", m_statParams.outputs[i]);
    fprintf(stderr, " ]\n");

    fprintf(stderr, "mv_predictor=%d\n", m_statParams.mv_predictor);
    fprintf(stderr, "qp=%d\n", m_statParams.qp);
    fprintf(stderr, "frame_qp=%d\n", m_statParams.frame_qp);
    fprintf(stderr, "len_sp=%d\n", m_statParams.len_sp);
    fprintf(stderr, "max_len_sp=%d\n", m_statParams.max_len_sp);
    fprintf(stderr, "reserved0=%d\n", m_statParams.reserved0);
    fprintf(stderr, "sub_mb_part_mask=%d\n", m_statParams.sub_mb_part_mask);
    fprintf(stderr, "sub_pel_mode=%d\n", m_statParams.sub_pel_mode);
    fprintf(stderr, "inter_sad=%d\n", m_statParams.inter_sad);
    fprintf(stderr, "intra_sad=%d\n", m_statParams.intra_sad);
    fprintf(stderr, "adaptive_search=%d\n", m_statParams.adaptive_search);
    fprintf(stderr, "mv_predictor_ctrl=%d\n", m_statParams.mv_predictor_ctrl);
    fprintf(stderr, "mb_qp=%d\n", m_statParams.mb_qp);
    fprintf(stderr, "ft_enable=%d\n", m_statParams.ft_enable);
    fprintf(stderr, "reserved1=%d\n", m_statParams.reserved1);
    fprintf(stderr, "ref_width=%d\n", m_statParams.ref_width);
    fprintf(stderr, "ref_height=%d\n", m_statParams.ref_height);
    fprintf(stderr, "search_window=%d\n", m_statParams.search_window);
    fprintf(stderr, "reserved2=%d\n", m_statParams.reserved2);
    fprintf(stderr, "disable_mv_output=%d\n", m_statParams.disable_mv_output);
    fprintf(stderr, "disable_statistics_output=%d\n", m_statParams.disable_statistics_output);
    fprintf(stderr, "interlaced=%d\n", m_statParams.interlaced);
    fprintf(stderr, "reserved3=%d\n", m_statParams.reserved3);
    fprintf(stderr, "\n");
#endif

    //MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            (VABufferType)VAStatsStatisticsParameterBufferTypeIntel,
            sizeof (m_statParams)*numMB,
            1,
            &m_statParams,
            &statParamsId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    configBuffers[buffersCount++] = statParamsId;

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv = statMVid;
        currentFeedback.mbstat = statOUTid;
        m_statFeedbackCache.push_back(currentFeedback);
    }

    //should we release here or not
    if (m_statParams.past_references)
    {
        delete [] m_statParams.past_references;
    }
    if (m_statParams.future_references)
    {
        delete [] m_statParams.future_references;
    }

    //destroy buffers - this should be removed
    MFX_DESTROY_VABUFFER(statParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(qpid, m_vaDisplay);
    /* these allocated buffers will be destroyed in QueryStatus()
     * when driver returned result of preenc processing  */
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    //MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIPREENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID statMVid = VA_INVALID_ID;
    VABufferID statOUTid = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            statMVid = currentFeedback.mv;
            //statMVid = m_statMVId.pop_front()
            statOUTid = currentFeedback.mbstat;

            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            VAMotionVectorIntel* mvs;
            VAStatsStatistics16x16Intel* mbstat;

            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

            //find output surfaces
            mfxExtFeiPreEncMV* mvsOut = NULL;
            for (int i = 0; i < out->NumExtParam; i++) {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MV)
                {
                    mvsOut = &((mfxExtFeiPreEncMV*)(out->ExtParam[i]))[fieldId];
                    break;
                }
            }

            mfxExtFeiPreEncMBStat* mbstatOut = NULL;
            for (int i = 0; i < out->NumExtParam; i++) {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_MB)
                {
                    mbstatOut = &((mfxExtFeiPreEncMBStat*)(out->ExtParam[i]))[fieldId];
                    break;
                }
            }

            //find control buffer
            mfxExtFeiPreEncCtrl* feiCtrl = NULL;
            for (int i = 0; i < in->NumExtParam; i++) {
                if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_CTRL)
                {
                    feiCtrl = &((mfxExtFeiPreEncCtrl*)(in->ExtParam[i]))[fieldId];
                    break;
                }
            }

            if (feiCtrl && !feiCtrl->DisableMVOutput && mvsOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statMVid,
                            (void **) (&mvs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvsOut->MB, 16 * sizeof (VAMotionVectorIntel) * mvsOut->NumMBAlloc,
                                mvs, 16 * sizeof (VAMotionVectorIntel) * mvsOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statMVid);
                }

                MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
            }

            if (feiCtrl && !feiCtrl->DisableStatisticsOutput && mbstatOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statOUTid,
                            (void **) (&mbstat));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbstatOut->MB, sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc,
                                mbstat, sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statOUTid);
                }

                MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            return MFX_ERR_NONE;
#if 0
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
#endif
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)

VAAPIFEIENCEncoder::VAAPIFEIENCEncoder()
: VAAPIEncoder()
, m_codingFunction(MFX_FEI_FUNCTION_ENC)
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
, m_codedBufferId(VA_INVALID_ID){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIENCEncoder::~VAAPIFEIENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_ENC)
        return CreateENCAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIENCEncoder::CreateENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_ENC_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

//#endif

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    mfxStatus mfxSts = MFX_ERR_NONE;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    // SPS
    if (task.m_insertSps[fieldId])
    {
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
        ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

        packed_header_param_buffer.type = VAEncPackedHeaderSequence;
        packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
        packed_header_param_buffer.bit_length = packedSps.DataLength*8;

        MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedSps.DataLength, 1, packedSps.pData,
                            &m_packedSpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSpsBufferId;
        /* sequence parameter set */
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),1,
                                   &m_sps,
                                   &m_spsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
        configBuffers[buffersCount++] = m_spsBufferId;
    }

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    VABufferID vaFeiMCODEOutId = VA_INVALID_ID;

    //find ext buffers
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiEncMBCtrl* mbctrl = NULL;
    mfxExtFeiEncMVPredictors* mvpred = NULL;
    mfxExtFeiEncFrameCtrl* frameCtrl = NULL;
    mfxExtFeiEncQP* mbqp = NULL;
    mfxExtFeiEncMBStat* mbstat = NULL;
    mfxExtFeiEncMV* mvout = NULL;
    mfxExtFeiPakMBCtrl* mbcodeout = NULL;

    /* in buffers */
    for (int i = 0; i < in->NumExtParam; i++)
    {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL)
        {
            frameCtrl = &((mfxExtFeiEncFrameCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED)
        {
            mvpred = &((mfxExtFeiEncMVPredictors*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB)
        {
            mbctrl = &((mfxExtFeiEncMBCtrl*) (in->ExtParam[i]))[fieldId];
        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
        {
            mbqp = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
        }
    }

    for (int i = 0; i < out->NumExtParam; i++)
    {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
        {
            mvout = &((mfxExtFeiEncMV*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
        {
            mbstat = &((mfxExtFeiEncMBStat*) (out->ExtParam[i]))[fieldId];
        }
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
        {
            mbcodeout = &((mfxExtFeiPakMBCtrl*) (out->ExtParam[i]))[fieldId];
        }
    }

    if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
                sizeof(VAEncMVPredictorH264Intel)*numMB,  //limitation from driver, num elements should be 1
                1,
                mvpred->MB,
                &vaFeiMVPredId );
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMVPredId;
        mdprintf(stderr, "vaFeiMVPredId=%d\n", vaFeiMVPredId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIMBControlBufferTypeIntel,
                sizeof (VAEncFEIMBControlH264Intel)*numMB,  //limitation from driver, num elements should be 1
                1,
                mbctrl->MB,
                &vaFeiMBControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBControlId;
        mdprintf(stderr, "vaFeiMBControlId=%d\n", vaFeiMBControlId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264)*numMB, //limitation from driver, num elements should be 1
                1,
                mbqp->QP,
                &vaFeiMBQPId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBQPId;
        mdprintf(stderr, "vaFeiMBQPId=%d\n", vaFeiMBQPId);
    }

    //output buffer for MB distortions
    if (mbstat != NULL)
    {
        if (m_vaFeiMBStatId.size() == 0 )
        {
            //m_vaFeiMBStatId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                        sizeof (VAEncFEIDistortionBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        &vaFeiMBStatId);
                        //&m_vaFeiMBStatId[ii]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMBStatId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMBStatId;
        //outBuffers[numOutBufs++] = m_vaFeiMBStatId[idxRecon];
        mdprintf(stderr, "MB Stat bufId=%d\n", vaFeiMBStatId);
        configBuffers[buffersCount++] = vaFeiMBStatId;
        //configBuffers[buffersCount++] = m_vaFeiMBStatId[idxRecon];
    }

    //output buffer for MV
    if (mvout != NULL)
    {
        //if (m_vaFeiMVOutId.size() == 0 )
        {
            //m_vaFeiMVOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIMVBufferTypeIntel,
                        sizeof (VAMotionVectorIntel)*16*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        //&m_vaFeiMVOutId[ii]);
                        &vaFeiMVOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMVOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMVOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMVOutId[idxRecon];
        mdprintf(stderr, "MV Out bufId=%d\n", vaFeiMVOutId);
        configBuffers[buffersCount++] = vaFeiMVOutId;
        //configBuffers[buffersCount++] = m_vaFeiMVOutId[idxRecon];
    }

    //output buffer for MBCODE (Pak object cmds)
    if (mbcodeout != NULL)
    {
        //if (m_vaFeiMCODEOutId.size() == 0 )
        {
            //m_vaFeiMCODEOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIModeBufferTypeIntel,
                        sizeof (VAEncFEIModeBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        //&m_vaFeiMCODEOutId[ii]);
                        &vaFeiMCODEOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } //if (m_vaFeiMCODEOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMCODEOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMCODEOutId[idxRecon];
        mdprintf(stderr, "MCODE Out bufId=%d\n", vaFeiMCODEOutId);
        configBuffers[buffersCount++] = vaFeiMCODEOutId;
        //configBuffers[buffersCount++] = m_vaFeiMCODEOutId[idxRecon];
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->max_len_sp = frameCtrl->MaxLenSP;

        vaFeiFrameControl->distortion = vaFeiMBStatId;
        vaFeiFrameControl->mv_data = vaFeiMVOutId;
        vaFeiFrameControl->mb_code_data = vaFeiMCODEOutId;
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
        vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
        vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
        vaFeiFrameControl->num_mv_predictors = frameCtrl->NumMVPredictors;
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);
    }
//    mfxU32 idxRecon = task.m_idxBs[fieldId];

    int ref_counter_l0 = 0;
    int ref_counter_l1 = 0;
    /* to fill L0 List*/
    if (in->NumFrameL0)
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            m_pps.ReferenceFrames[ref_counter_l0].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n ",
                    ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
        }
    }
    /* to fill L1 List*/
    if (in->NumFrameL1)
    {
        /* continue to fill ref list */
        for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle;
            m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n",ref_counter_l0 + ref_counter_l1,
                                    m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id);
        }
    }


    int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;
    mdprintf(stderr,"slice_type=%d\n",slice_type);
    /* slice parameters */
    {
        int i = 0, j = 0;

        mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        if (task.m_numSlice[fieldId])
            m_slice.resize(task.m_numSlice[fieldId]);
        mfxU32 numSlice = m_slice.size();
        mfxU32 idx = 0, ref = 0;

        SliceDivider divider = MakeSliceDivider(
            m_caps.SliceStructure,
            task.m_numMbPerSlice,
            numSlice,
            m_sps.picture_width_in_mbs,
            m_sps.picture_height_in_mbs / numPics);

        for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )
        {
            m_slice[i].macroblock_address = divider.GetFirstMbInSlice();
            m_slice[i].num_macroblocks = divider.GetNumMbInSlice();
            m_slice[i].macroblock_info = VA_INVALID_ID;
            m_slice[i].slice_type = slice_type;
            m_slice[i].pic_parameter_set_id = m_pps.pic_parameter_set_id;
            m_slice[i].idr_pic_id = task.m_idrPicId;
            m_slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));
            m_slice[i].direct_spatial_mv_pred_flag = 1;

            if ( slice_type == SLICE_TYPE_I ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = -1;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_P ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_B ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = in->NumFrameL1;
            }

            m_slice[i].cabac_init_idc = 0;
            m_slice[i].slice_qp_delta = 0;
            m_slice[i].disable_deblocking_filter_idc = 0;
            m_slice[i].slice_alpha_c0_offset_div2 = 2;
            m_slice[i].slice_beta_offset_div2 = 2;
            /*    */
            for (j=0; j<32; j++)
            {
                m_slice[i].RefPicList0[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[j].flags = VA_PICTURE_H264_INVALID;
                m_slice[i].RefPicList1[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[j].flags = VA_PICTURE_H264_INVALID;
            }

            if ((in->NumFrameL0) && ((slice_type == SLICE_TYPE_P) || (slice_type == SLICE_TYPE_P)) )
            {
                for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList0[ref_counter_l0].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
            if ( (in->NumFrameL1) && (slice_type == SLICE_TYPE_B) )
            {
                for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
        } // for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )

        for( size_t i = 0; i < m_slice.size(); i++ )
        {
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            //configBuffers[buffersCount++] = m_sliceBufferId[i];
            mdprintf(stderr, "m_sliceBufferId[%d]=%d\n", i, m_sliceBufferId[i]);
        }
    } /* slice parameters */

    /* UpdatePPS */
    m_pps.frame_num = task.m_frameNum;
    m_pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    m_pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;
    m_pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
    m_pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);

    /* hard coded, ENC does not generate real reconstruct surface,
     * and this surface should be unchanged
     * BUT (!) this surface should be from reconstruct surface pool which was passed to
     * component when vaCreateContext was called */
    m_pps.CurrPic.picture_id = m_reconQueue[0].surface;
    m_pps.CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    mdprintf(stderr,"m_pps.CurrPic.picture_id = %d\n",m_pps.CurrPic.picture_id);

    /* Need to allocated coded buffer
     */
    if (VA_INVALID_ID == m_codedBufferId)
    {
        int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
        int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
        int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncCodedBufferType,
                                codedbuf_size,
                                1,
                                NULL,
                                &m_codedBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_codedBufferId;
        mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId);
    }
    m_pps.coded_buf = m_codedBufferId;

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPictureParameterBufferType,
                            sizeof(m_pps),
                            1,
                            &m_pps,
                            &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_ppsBufferId;
    mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

    if (task.m_insertPps[fieldId])
    {
        // PPS
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
        ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

        packed_header_param_buffer.type = VAEncPackedHeaderPicture;
        packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
        packed_header_param_buffer.bit_length = packedPps.DataLength*8;

        MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedPpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedPps.DataLength, 1, packedPps.pData,
                            &m_packedPpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedPpsBufferId;
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
        {
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &m_sliceBufferId[i],
                1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }


    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = vaFeiMVOutId;
        currentFeedback.mbstat    = vaFeiMBStatId;
        currentFeedback.mbcode    = vaFeiMCODEOutId;
        m_statFeedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            mfxExtFeiEncMBStat* mbstat = NULL;
            mfxExtFeiEncMV* mvout = NULL;
            mfxExtFeiPakMBCtrl* mbcodeout = NULL;
            int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

            /* NO Bitstream in ENC */
            task.m_bsDataLength[fieldId] = 0;

            for (int i = 0; i < out->NumExtParam; i++)
            {
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
                {
                    mvout = &((mfxExtFeiEncMV*) (out->ExtParam[i]))[fieldId];
                }
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
                {
                    mbstat = &((mfxExtFeiEncMBStat*) (out->ExtParam[i]))[fieldId];
                }
                if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
                {
                    mbcodeout = &((mfxExtFeiPakMBCtrl*) (out->ExtParam[i]))[fieldId];
                }
            }

            if (mbstat != NULL && vaFeiMBStatId != VA_INVALID_ID)
            {
                VAEncFEIDistortionBufferH264Intel* mbs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMBStatId,
                            (void **) (&mbs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbstat->MB, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB,
                               mbs, sizeof (VAEncFEIDistortionBufferH264Intel) * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBStatId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMBStatId, m_vaDisplay);
                vaFeiMBStatId = VA_INVALID_ID;
            }

            if (mvout != NULL && vaFeiMVOutId != VA_INVALID_ID)
            {
                VAMotionVectorIntel* mvs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMVOutId,
                            (void **) (&mvs));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvout->MB, sizeof (VAMotionVectorIntel) * 16 * numMB,
                               mvs, sizeof (VAMotionVectorIntel) * 16 * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMVOutId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMVOutId, m_vaDisplay);
                vaFeiMVOutId = VA_INVALID_ID;
            }

            if (mbcodeout != NULL && vaFeiMBCODEOutId != VA_INVALID_ID)
            {
                VAEncFEIModeBufferH264Intel* mbcs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                        m_vaDisplay,
                        vaFeiMBCODEOutId,
                        (void **) (&mbcs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbcodeout->MB, sizeof (VAEncFEIModeBufferH264Intel) * numMB,
                                  mbcs, sizeof (VAEncFEIModeBufferH264Intel) * numMB);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBCODEOutId);
                }

                MFX_DESTROY_VABUFFER(vaFeiMBCODEOutId, m_vaDisplay);
                vaFeiMBCODEOutId = VA_INVALID_ID;
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            return MFX_ERR_NONE;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif //#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)


#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

VAAPIFEIPAKEncoder::VAAPIFEIPAKEncoder()
: VAAPIEncoder()
, m_codingFunction(MFX_FEI_FUNCTION_PAK)
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
, m_codedBufferId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPAKEncoder::~VAAPIFEIPAKEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPAKEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPAKEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_PAK)
        return CreatePAKAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPAKEncoder::CreatePAKAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_PAK_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIPAKEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIFEIPAKEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;

    mfxStatus mfxSts = MFX_ERR_NONE;

    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    //VASurfaceID *inputSurface = (VASurfaceID*) surface;
    VASurfaceID *inputSurface = NULL;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    // SPS
    if (task.m_insertSps[fieldId])
    {
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
        ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

        packed_header_param_buffer.type = VAEncPackedHeaderSequence;
        packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
        packed_header_param_buffer.bit_length = packedSps.DataLength*8;

        MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedSps.DataLength, 1, packedSps.pData,
                            &m_packedSpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSpsBufferId;

        /* sequence parameter set */
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),1,
                                   &m_sps,
                                   &m_spsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
        configBuffers[buffersCount++] = m_spsBufferId;
    } //   if (task.m_insertSps[fieldId])

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;

    //buffer for MV output
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    mfxPAKInput* in = (mfxPAKInput*)task.m_userData[0];
    mfxPAKOutput* out = (mfxPAKOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    VABufferID vaFeiMCODEOutId = VA_INVALID_ID;

    //find ext buffers
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiEncMBCtrl* mbctrl = NULL;
    mfxExtFeiEncMVPredictors* mvpred = NULL;
    mfxExtFeiEncFrameCtrl* frameCtrl = NULL;
    mfxExtFeiEncQP* mbqp = NULL;
    mfxExtFeiEncMBStat* mbstat = NULL;
    mfxExtFeiEncMV* mvout = NULL;
    mfxExtFeiPakMBCtrl* mbcodeout = NULL;


//    /* in buffers */
    for (int i = 0; i < out->NumExtParam; i++)
    {
        if (out->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_CTRL)
        {
            frameCtrl = &((mfxExtFeiEncFrameCtrl*) (out->ExtParam[i]))[fieldId];
        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV_PRED)
//        {
//            mvpred = &((mfxExtFeiEncMVPredictors*) (in->ExtParam[i]))[fieldId];
//        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB)
//        {
//            mbctrl = &((mfxExtFeiEncMBCtrl*) (in->ExtParam[i]))[fieldId];
//        }
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PREENC_QP)
//        {
//            mbqp = &((mfxExtFeiEncQP*) (in->ExtParam[i]))[fieldId];
//        }
    }

    for (int i = 0; i < in->NumExtParam; i++)
    {
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MV)
        {
            mvout = &((mfxExtFeiEncMV*) (in->ExtParam[i]))[fieldId];
        }
        /* Does not need statistics for PAK */
//        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_ENC_MB_STAT)
//        {
//            mbstat = &((mfxExtFeiEncMBStat*) (in->ExtParam[i]))[fieldId];
//        }
        if (in->ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PAK_CTRL)
        {
            mbcodeout = &((mfxExtFeiPakMBCtrl*) (in->ExtParam[i]))[fieldId];
        }
    }

//    if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
//                sizeof(VAEncMVPredictorH264Intel)*numMB,  //limitation from driver, num elements should be 1
//                1,
//                mvpred->MB,
//                &vaFeiMVPredId );
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMVPredId;
//        mdprintf(stderr, "vaFeiMVPredId=%d\n", vaFeiMVPredId);
//    }
//
//    if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType)VAEncFEIMBControlBufferTypeIntel,
//                sizeof (VAEncFEIMBControlH264Intel)*numMB,  //limitation from driver, num elements should be 1
//                1,
//                mbctrl->MB,
//                &vaFeiMBControlId);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMBControlId;
//        mdprintf(stderr, "vaFeiMBControlId=%d\n", vaFeiMBControlId);
//    }
//
//    if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
//    {
//        vaSts = vaCreateBuffer(m_vaDisplay,
//                m_vaContextEncode,
//                (VABufferType)VAEncQpBufferType,
//                sizeof (VAEncQpBufferH264)*numMB, //limitation from driver, num elements should be 1
//                1,
//                mbqp->QP,
//                &vaFeiMBQPId);
//        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
//        configBuffers[buffersCount++] = vaFeiMBQPId;
//        mdprintf(stderr, "vaFeiMBQPId=%d\n", vaFeiMBQPId);
//    }

    //output buffer for MB distortions
    if (mbstat != NULL)
    {
        if (m_vaFeiMBStatId.size() == 0 )
        {
            //m_vaFeiMBStatId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                        sizeof (VAEncFEIDistortionBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        NULL, //should be mapped later
                        &vaFeiMBStatId);
                        //&m_vaFeiMBStatId[ii]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMBStatId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMBStatId;
        //outBuffers[numOutBufs++] = m_vaFeiMBStatId[idxRecon];
        mdprintf(stderr, "MB Stat bufId=%d\n", vaFeiMBStatId);
        configBuffers[buffersCount++] = vaFeiMBStatId;
        //configBuffers[buffersCount++] = m_vaFeiMBStatId[idxRecon];
    }

    //output buffer for MV
    if (mvout != NULL)
    {
        //if (m_vaFeiMVOutId.size() == 0 )
        {
            //m_vaFeiMVOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIMVBufferTypeIntel,
                        sizeof (VAMotionVectorIntel)*16*numMB, //limitation from driver, num elements should be 1
                        1,
                        mvout->MB, //should be mapped later
                        //&m_vaFeiMVOutId[ii]);
                        &vaFeiMVOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } // if (m_vaFeiMVOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMVOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMVOutId[idxRecon];
        mdprintf(stderr, "MV Out bufId=%d\n", vaFeiMVOutId);
        configBuffers[buffersCount++] = vaFeiMVOutId;
        //configBuffers[buffersCount++] = m_vaFeiMVOutId[idxRecon];
    }

    //output buffer for MBCODE (Pak object cmds)
    if (mbcodeout != NULL)
    {
        //if (m_vaFeiMCODEOutId.size() == 0 )
        {
            //m_vaFeiMCODEOutId.resize(16);
            //for( mfxU32 ii = 0; ii < 16; ii++ )
            {
                vaSts = vaCreateBuffer(m_vaDisplay,
                        m_vaContextEncode,
                        (VABufferType)VAEncFEIModeBufferTypeIntel,
                        sizeof (VAEncFEIModeBufferH264Intel)*numMB, //limitation from driver, num elements should be 1
                        1,
                        mbcodeout->MB, //should be mapped later
                        //&m_vaFeiMCODEOutId[ii]);
                        &vaFeiMCODEOutId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        } //if (m_vaFeiMCODEOutId.size() == 0 )
        //outBuffers[numOutBufs++] = vaFeiMCODEOutId;
        //outBuffers[numOutBufs++] = m_vaFeiMCODEOutId[idxRecon];
        mdprintf(stderr, "MCODE Out bufId=%d\n", vaFeiMCODEOutId);
        configBuffers[buffersCount++] = vaFeiMCODEOutId;
        //configBuffers[buffersCount++] = m_vaFeiMCODEOutId[idxRecon];
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        //vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_INTEL;
        vaFeiFrameControl->function = VA_ENC_FUNCTION_PAK_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->max_len_sp = frameCtrl->MaxLenSP;

        vaFeiFrameControl->distortion = VA_INVALID_ID;//vaFeiMBStatId;
        vaFeiFrameControl->mv_data = vaFeiMVOutId;
        vaFeiFrameControl->mb_code_data = vaFeiMCODEOutId;
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
        vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
        vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
        vaFeiFrameControl->num_mv_predictors = frameCtrl->NumMVPredictors;
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);
    }
//    mfxU32 idxRecon = task.m_idxBs[fieldId];

    int ref_counter_l0 = 0;
    int ref_counter_l1 = 0;
    /* to fill L0 List*/
    if (in->NumFrameL0)
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            m_pps.ReferenceFrames[ref_counter_l0].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n ",
                    ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
        }
    }
    /* to fill L1 List*/
    if (in->NumFrameL1)
    {
        /* continue to fill ref list */
        for (ref_counter_l1 = 0 ; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
        {
            mfxHDL handle;
            mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
            MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
            VASurfaceID* s = (VASurfaceID*) handle;
            m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id = *s;
            mdprintf(stderr,"m_pps.ReferenceFrames[%d].picture_id: %x\n", ref_counter_l0 + ref_counter_l1,
                    m_pps.ReferenceFrames[ref_counter_l0 + ref_counter_l1].picture_id);
        }
    }

    int slice_type = ConvertMfxFrameType2SliceType( task.m_type[fieldId]) - 5;
    mdprintf(stderr,"slice_type=%d\n",slice_type);
    /* slice parameters */
    {
        int i = 0, j = 0;

        mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        if (task.m_numSlice[fieldId])
            m_slice.resize(task.m_numSlice[fieldId]);
        mfxU32 numSlice = m_slice.size();
        mfxU32 idx = 0, ref = 0;

        SliceDivider divider = MakeSliceDivider(
            m_caps.SliceStructure,
            task.m_numMbPerSlice,
            numSlice,
            m_sps.picture_width_in_mbs,
            m_sps.picture_height_in_mbs / numPics);

        for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )
        {
            m_slice[i].macroblock_address = divider.GetFirstMbInSlice();
            m_slice[i].num_macroblocks = divider.GetNumMbInSlice();
            m_slice[i].macroblock_info = VA_INVALID_ID;
            m_slice[i].slice_type = slice_type;
            m_slice[i].pic_parameter_set_id = m_pps.pic_parameter_set_id;
            m_slice[i].idr_pic_id = task.m_idrPicId;
            m_slice[i].pic_order_cnt_lsb = mfxU16(task.GetPoc(fieldId));
            m_slice[i].direct_spatial_mv_pred_flag = 1;

            if ( slice_type == SLICE_TYPE_I ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = -1;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_P ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = -1;
            } else if ( slice_type == SLICE_TYPE_B ) {
                m_slice[i].num_ref_idx_l0_active_minus1 = in->NumFrameL0;
                m_slice[i].num_ref_idx_l1_active_minus1 = in->NumFrameL1;
            }

            m_slice[i].cabac_init_idc = 0;
            m_slice[i].slice_qp_delta = 0;
            m_slice[i].disable_deblocking_filter_idc = 0;
            m_slice[i].slice_alpha_c0_offset_div2 = 2;
            m_slice[i].slice_beta_offset_div2 = 2;
            /*    */
            for (j=0; j<32; j++)
            {
                m_slice[i].RefPicList0[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[j].flags = VA_PICTURE_H264_INVALID;
                m_slice[i].RefPicList1[j].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[j].flags = VA_PICTURE_H264_INVALID;
            }

            if ((in->NumFrameL0) && ((slice_type == SLICE_TYPE_P) || (slice_type == SLICE_TYPE_P)) )
            {
                for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[ref_counter_l0]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList0[ref_counter_l0].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
            if ( (in->NumFrameL1) && (slice_type == SLICE_TYPE_B) )
            {
                for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++)
                {
                    mfxHDL handle;
                    mfxSts = m_core->GetExternalFrameHDL(in->L1Surface[ref_counter_l1]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
                }
            }
        } // for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )

        for( size_t i = 0; i < m_slice.size(); i++ )
        {
            //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                                    m_vaContextEncode,
                                    VAEncSliceParameterBufferType,
                                    sizeof(m_slice[i]),
                                    1,
                                    &m_slice[i],
                                    &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            //configBuffers[buffersCount++] = m_sliceBufferId[i];
            mdprintf(stderr, "m_sliceBufferId[%d]=%d\n", i, m_sliceBufferId[i]);
        }
    } /* slice parameters */

    /* UpdatePPS */
    m_pps.frame_num = task.m_frameNum;
    m_pps.pic_fields.bits.idr_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    m_pps.pic_fields.bits.reference_pic_flag = (task.m_type[fieldId] & MFX_FRAMETYPE_REF) ? 1 : 0;
    m_pps.CurrPic.TopFieldOrderCnt = task.GetPoc(TFIELD);
    m_pps.CurrPic.BottomFieldOrderCnt = task.GetPoc(BFIELD);

    mfxHDL recon_handle;
    mfxSts = m_core->GetExternalFrameHDL(out->OutSurface->Data.MemId, &recon_handle);
    m_pps.CurrPic.picture_id = *(VASurfaceID*) recon_handle; //id in the memory by ptr
    m_pps.CurrPic.flags = VA_PICTURE_H264_SHORT_TERM_REFERENCE;
    mdprintf(stderr,"m_pps.CurrPic.picture_id = %d\n",m_pps.CurrPic.picture_id);

    /* Need to allocated coded buffer
     */
    if (VA_INVALID_ID == m_codedBufferId)
    {
        int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
        int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
        int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncCodedBufferType,
                                codedbuf_size,
                                1,
                                NULL,
                                &m_codedBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_codedBufferId;
        mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId);
    }
    m_pps.coded_buf = m_codedBufferId;

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPictureParameterBufferType,
                            sizeof(m_pps),
                            1,
                            &m_pps,
                            &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_ppsBufferId;
    mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

    if (task.m_insertPps[fieldId])
    {
        // PPS
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
        ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

        packed_header_param_buffer.type = VAEncPackedHeaderPicture;
        packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
        packed_header_param_buffer.bit_length = packedPps.DataLength*8;

        MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedPpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedPps.DataLength, 1, packedPps.pData,
                            &m_packedPpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedPpsBufferId;
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------

    mfxHDL handle_1;
    mfxSts = m_core->GetExternalFrameHDL(in->InSurface->Data.MemId, &handle_1);
    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
    inputSurface = (VASurfaceID*) handle_1; //id in the memory by ptr

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[0]=%d\n", m_pps.ReferenceFrames[0].picture_id);
    mdprintf(stderr, "m_pps.ReferenceFrames[1]=%d\n", m_pps.ReferenceFrames[1].picture_id);

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
        {
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &m_sliceBufferId[i],
                1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }


    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = vaFeiMVOutId;
        //currentFeedback.mbstat    = vaFeiMBStatId;
        currentFeedback.mbcode    = vaFeiMCODEOutId;
        m_statFeedbackCache.push_back(currentFeedback);
        //m_feedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIFEIPAKEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[fieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            /* first to destroy used MBCode and MV buffers */
            MFX_DESTROY_VABUFFER(vaFeiMBCODEOutId, m_vaDisplay);
            MFX_DESTROY_VABUFFER(vaFeiMVOutId, m_vaDisplay);

            VACodedBufferSegment *codedBufferSegment;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                vaSts = vaMapBuffer(
                    m_vaDisplay,
                    m_codedBufferId,
                    (void **)(&codedBufferSegment));
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            task.m_bsDataLength[fieldId] = codedBufferSegment->size;
            memcpy_s(task.m_bs->Data + task.m_bs->DataOffset, codedBufferSegment->size,
                                     codedBufferSegment->buf, codedBufferSegment->size);
            task.m_bs->DataLength = codedBufferSegment->size;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                vaUnmapBuffer( m_vaDisplay, m_codedBufferId );
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            return MFX_ERR_NONE;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            return MFX_WRN_DEVICE_BUSY;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            return MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
    }

    mdprintf(stderr, "query_vaapi done\n");

} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)

#endif //defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
/* EOF */

