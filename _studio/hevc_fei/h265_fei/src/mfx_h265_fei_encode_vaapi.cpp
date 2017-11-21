//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)

#include "mfx_h265_fei_encode_vaapi.h"

using namespace MfxHwH265Encode;

namespace MfxHwH265FeiEncode
{
    VAAPIh265FeiEncoder::VAAPIh265FeiEncoder()
        : VAAPIEncoder()
    {}

    VAAPIh265FeiEncoder::~VAAPIh265FeiEncoder()
    {}

    mfxStatus VAAPIh265FeiEncoder::ConfigureExtraVAattribs(std::vector<VAConfigAttrib> & attrib)
    {
        attrib.reserve(attrib.size() + 1);

        VAConfigAttrib attr = {};

        attr.type = (VAConfigAttribType) VAConfigAttribFEIFunctionType;
        attrib.push_back(attr);

        return MFX_ERR_NONE;
    }

    mfxStatus VAAPIh265FeiEncoder::CheckExtraVAattribs(std::vector<VAConfigAttrib> & attrib)
    {
        mfxU32 i = 0;
        for (; i < attrib.size(); ++i)
        {
            if (attrib[i].type == (VAConfigAttribType) VAConfigAttribFEIFunctionType)
                break;
        }
        MFX_CHECK(i < attrib.size(), MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(attrib[i].value & VA_FEI_FUNCTION_ENC_PAK, MFX_ERR_DEVICE_FAILED);

        attrib[i].value = VA_FEI_FUNCTION_ENC_PAK;

        return MFX_ERR_NONE;
    }

    mfxStatus VAAPIh265FeiEncoder::PreSubmitExtraStage(Task const & task)
    {
        {
            VAStatus vaSts;
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "FrameCtrl");

            VABufferID &vaFeiFrameControlId = VABufferNew(VABID_FEI_FRM_CTRL, 0);
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaCreateBuffer");
                vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncMiscParameterBufferType,
                    sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlHEVC),
                    1,
                    NULL,
                    &vaFeiFrameControlId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            VAEncMiscParameterBuffer *miscParam;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaMapBuffer");
                vaSts = vaMapBuffer(m_vaDisplay,
                    vaFeiFrameControlId,
                    (void **)&miscParam);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControl;
            VAEncMiscParameterFEIFrameControlHEVC* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlHEVC*)miscParam->data;
            memset(vaFeiFrameControl, 0, sizeof(VAEncMiscParameterFEIFrameControlHEVC));

            mfxExtFeiHevcEncFrameCtrl* EncFrameCtrl = reinterpret_cast<mfxExtFeiHevcEncFrameCtrl*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTRL));
            MFX_CHECK(EncFrameCtrl, MFX_ERR_UNDEFINED_BEHAVIOR);

            vaFeiFrameControl->function                           = VA_FEI_FUNCTION_ENC_PAK;
            vaFeiFrameControl->search_path                        = EncFrameCtrl->SearchPath;
            vaFeiFrameControl->len_sp                             = EncFrameCtrl->LenSP;
            vaFeiFrameControl->ref_width                          = EncFrameCtrl->RefWidth;
            vaFeiFrameControl->ref_height                         = EncFrameCtrl->RefHeight;
            vaFeiFrameControl->search_window                      = EncFrameCtrl->SearchWindow;
            vaFeiFrameControl->num_mv_predictors_l0               = EncFrameCtrl->NumMvPredictors[0];
            vaFeiFrameControl->num_mv_predictors_l1               = EncFrameCtrl->NumMvPredictors[1];
            vaFeiFrameControl->multi_pred_l0                      = EncFrameCtrl->MultiPred[0];
            vaFeiFrameControl->multi_pred_l1                      = EncFrameCtrl->MultiPred[1];
            vaFeiFrameControl->sub_pel_mode                       = EncFrameCtrl->SubPelMode;
            vaFeiFrameControl->adaptive_search                    = EncFrameCtrl->AdaptiveSearch;
            vaFeiFrameControl->mv_predictor_input                 = EncFrameCtrl->MVPredictor;
            vaFeiFrameControl->per_block_qp                       = EncFrameCtrl->PerCuQp;
            vaFeiFrameControl->per_ctb_input                      = EncFrameCtrl->PerCtuInput;
            vaFeiFrameControl->force_lcu_split                    = EncFrameCtrl->ForceCtuSplit;
            vaFeiFrameControl->num_concurrent_enc_frame_partition = EncFrameCtrl->NumFramePartitions;

            // Input buffers
            mfxExtFeiHevcEncMVPredictors* mvp = reinterpret_cast<mfxExtFeiHevcEncMVPredictors*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED));
            vaFeiFrameControl->mv_predictor = mvp ? mvp->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcEncQP* qp = reinterpret_cast<mfxExtFeiHevcEncQP*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_QP));
            vaFeiFrameControl->qp = qp ? qp->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcEncCtuCtrl* ctuctrl = reinterpret_cast<mfxExtFeiHevcEncCtuCtrl*>(GetBufById(task.m_ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTU_CTRL));
            vaFeiFrameControl->ctb_ctrl = ctuctrl ? ctuctrl->VaBufferID : VA_INVALID_ID;

            // Output buffers
#if MFX_VERSION >= MFX_VERSION_NEXT
            mfxExtFeiHevcPakCtuRecordV0* ctucmd = reinterpret_cast<mfxExtFeiHevcPakCtuRecordV0*>(GetBufById(task.m_bs, MFX_EXTBUFF_HEVCFEI_PAK_CTU_REC));
            vaFeiFrameControl->ctb_cmd = ctucmd ? ctucmd->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcPakCuRecordV0* curec = reinterpret_cast<mfxExtFeiHevcPakCuRecordV0*>(GetBufById(task.m_bs, MFX_EXTBUFF_HEVCFEI_PAK_CU_REC));
            vaFeiFrameControl->cu_record = curec ? curec->VaBufferID : VA_INVALID_ID;

            mfxExtFeiHevcDistortion* distortion = reinterpret_cast<mfxExtFeiHevcDistortion*>(GetBufById(task.m_bs, MFX_EXTBUFF_HEVCFEI_ENC_DIST));
            vaFeiFrameControl->distortion = distortion ? distortion->VaBufferID : VA_INVALID_ID;
#else
            vaFeiFrameControl->ctb_cmd    = VA_INVALID_ID;
            vaFeiFrameControl->cu_record  = VA_INVALID_ID;
            vaFeiFrameControl->distortion = VA_INVALID_ID;
#endif

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "vaUnmapBuffer");
                vaSts = vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }
        }

        return MFX_ERR_NONE;
    }

    mfxStatus VAAPIh265FeiEncoder::PostQueryExtraStage()
    {
        return MFX_ERR_NONE;
    }
}

#endif
