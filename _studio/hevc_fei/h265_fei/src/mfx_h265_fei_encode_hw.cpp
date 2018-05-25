//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined (MFX_ENABLE_HEVC_VIDEO_FEI_ENCODE)
#include "mfx_h265_fei_encode_hw.h"

using namespace MfxHwH265Encode;

namespace MfxHwH265FeiEncode
{

mfxStatus H265FeiEncode_HW::ExtraParametersCheck(mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs)
{
    if (!surface) return MFX_ERR_NONE; // In case of frames draining in display order

    MFX_CHECK(ctrl, MFX_ERR_INVALID_VIDEO_PARAM);

    eMFXHWType platform = m_core->GetHWType();

#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(OPEN_SOURCE)
    bool isSKL = platform == MFX_HW_SCL, isICLplus = platform >= MFX_HW_ICL;
#else
    bool isSKL = platform == MFX_HW_SCL, isICLplus = false;
#endif


    // mfxExtFeiHevcEncFrameCtrl is a mandatory buffer
    mfxExtFeiHevcEncFrameCtrl* EncFrameCtrl = reinterpret_cast<mfxExtFeiHevcEncFrameCtrl*>(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTRL));
    MFX_CHECK(EncFrameCtrl, MFX_ERR_INVALID_VIDEO_PARAM);

    // Check HW limitations for mfxExtFeiHevcEncFrameCtrl parameters
    MFX_CHECK(EncFrameCtrl->NumMvPredictors[0] <= 4, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(EncFrameCtrl->NumMvPredictors[1] <= 4, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(EncFrameCtrl->MultiPred[0]       <= (isICLplus ? 2 : 1),
                                                     MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(EncFrameCtrl->MultiPred[1]       <= (isICLplus ? 2 : 1),
                                                     MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(EncFrameCtrl->SubPelMode         <= 3 &&
              EncFrameCtrl->SubPelMode         != 2, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(EncFrameCtrl->MVPredictor == 0
          ||  EncFrameCtrl->MVPredictor == 1
          ||  EncFrameCtrl->MVPredictor == 2
          || (EncFrameCtrl->MVPredictor == 3 && isICLplus)
          ||  EncFrameCtrl->MVPredictor == 7,        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(EncFrameCtrl->ForceCtuSplit <= (isSKL ? 1: 0), MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(EncFrameCtrl->FastIntraMode <= (isSKL ? 1: 0), MFX_ERR_INVALID_VIDEO_PARAM);

    switch (EncFrameCtrl->NumFramePartitions)
    {
    case 1:
    case 2:
    case 4:
    case 8:
    case 16:
        break;
    default:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    MFX_CHECK(EncFrameCtrl->AdaptiveSearch <= 1,  MFX_ERR_INVALID_VIDEO_PARAM);
    if (EncFrameCtrl->SearchWindow)
    {
        MFX_CHECK(EncFrameCtrl->LenSP          == 0
               && EncFrameCtrl->SearchPath     == 0
               && EncFrameCtrl->RefWidth       == 0
               && EncFrameCtrl->RefHeight      == 0, MFX_ERR_INVALID_VIDEO_PARAM);

        MFX_CHECK(EncFrameCtrl->SearchWindow <= 5,   MFX_ERR_INVALID_VIDEO_PARAM);
    }
    else
    {
        MFX_CHECK(EncFrameCtrl->SearchPath     <= 2,  MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(EncFrameCtrl->LenSP          >= 1 &&
                  EncFrameCtrl->LenSP          <= 63, MFX_ERR_INVALID_VIDEO_PARAM);

        if (EncFrameCtrl->AdaptiveSearch)
        {
            MFX_CHECK(EncFrameCtrl->LenSP      >= 2,  MFX_ERR_INVALID_VIDEO_PARAM);
        }

        if (isSKL)
        {
            mfxU16 frameType = GetFrameType(m_vpar, m_frameOrder - m_lastIDR);

            MFX_CHECK(EncFrameCtrl->RefWidth  % 4 == 0
                   && EncFrameCtrl->RefHeight % 4 == 0
                   && EncFrameCtrl->RefWidth  <= ((frameType & MFX_FRAMETYPE_B) ? 32 : 64)
                   && EncFrameCtrl->RefHeight <= ((frameType & MFX_FRAMETYPE_B) ? 32 : 64)
                   && EncFrameCtrl->RefWidth * EncFrameCtrl->RefHeight <= 2048,
                      // For B frames actual limit is RefWidth*RefHeight <= 1024.
                      // Is is already guranteed by limit of 32 pxls for RefWidth and RefHeight in case of B frame
                   MFX_ERR_INVALID_VIDEO_PARAM);
        }
        else
        {
            // ICL+
            MFX_CHECK(  (EncFrameCtrl->RefWidth == 64 && EncFrameCtrl->RefHeight == 64)
                     || (EncFrameCtrl->RefWidth == 48 && EncFrameCtrl->RefHeight == 40),
                     MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }

    // Check if requested buffers are provided

    if (EncFrameCtrl->MVPredictor)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_MV_PRED), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if (EncFrameCtrl->PerCuQp)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_QP), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    if (EncFrameCtrl->PerCtuInput)
    {
        MFX_CHECK(GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_ENC_CTU_CTRL), MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // Check for mfxExtFeiHevcRepackCtrl

    mfxExtFeiHevcRepackCtrl* repackctrl = reinterpret_cast<mfxExtFeiHevcRepackCtrl*>(
        GetBufById(ctrl, MFX_EXTBUFF_HEVCFEI_REPACK_CTRL));

    if (repackctrl)
    {
        MFX_CHECK(!EncFrameCtrl->PerCuQp, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(repackctrl->NumPasses <= 8, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return MFX_ERR_NONE;
}

};
#endif
