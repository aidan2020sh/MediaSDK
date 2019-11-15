// Copyright (c) 2019 Intel Corporation
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

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE) && defined(MFX_ENABLE_HEVCE_ROI) && !defined(MFX_VA_LINUX)

#include "hevcehw_g11_roi_win.h"
#include "hevcehw_g11_ddi_packer_win.h"

using namespace HEVCEHW;
using namespace HEVCEHW::Gen11;

void Windows::Gen11::ROI::SubmitTask(const FeatureBlocks& /*blocks*/, TPushST Push)
{
    Push(BLK_PatchDDITask
        , [this](StorageW& global, StorageW& s_task) -> mfxStatus
    {
        auto& par = Glob::DDI_SubmitParam::Get(global);
        auto& core = Glob::VideoCore::Get(global);

        auto vaType = core.GetVAType();
        auto& task = Task::Common::Get(s_task);
        auto& pps = Deref(GetDDICB<ENCODE_SET_PICTURE_PARAMETERS_HEVC>(
            ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));
        const mfxExtEncoderROI* pROI = ExtBuffer::Get(task.ctrl);
        const mfxExtEncoderROI* pInitROI = ExtBuffer::Get(Glob::VideoParam::Get(global));
        mfxExtMBQP* pMBQP = ExtBuffer::Get(task.ctrl);

        mfxU32 qpBlk = 3 + Glob::EncodeCaps::Get(global).BlockSize;

        SetDefault(pROI, pInitROI);

        pps.NumROI = 0;

        bool bFillQpSurf = m_bViaCuQp && !pMBQP;

        if (bFillQpSurf)
        {
            auto& qpMapInfo = Glob::AllocMBQP::Get(global).Info();

            FrameLocker qpMap(core, task.CUQP.Mid);
            MFX_CHECK(qpMap.Y, MFX_ERR_LOCK_MEMORY);

            mfxI32 dQPMult = 1 - 2 * (pROI->ROIMode == MFX_ROI_MODE_PRIORITY);

            mfxU32 y = 0;
            std::for_each(
                MakeStepIter(qpMap.Y, qpMap.Pitch)
                , MakeStepIter(qpMap.Y + qpMapInfo.Height * qpMap.Pitch)
                , [&](mfxU8& qpMapRow)
            {
                mfxU32 x = 0;
                std::generate_n(&qpMapRow, qpMapInfo.Width
                    , [&]()
                {
                    auto pEnd = pROI->ROI + pROI->NumROI;
                    auto pRect = std::find_if(pROI->ROI, pEnd
                        , [x, y, qpBlk](const RectData& r)
                    {
                        return (x << qpBlk) >= r.Left
                            && (x << qpBlk) < r.Right
                            && (y << qpBlk) >= r.Top
                            && (y << qpBlk) < r.Bottom;
                    });

                    ++x;

                    if (pRect != pEnd)
                        return mfxU8(task.QpY + dQPMult * pRect->DeltaQP);

                    return mfxU8(task.QpY);
                });

                ++y;
            });
        }
        else
        {
            MFX_CHECK(pROI->NumROI, MFX_ERR_NONE);

            auto& sps = Deref(GetDDICB<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC>(
                ENCODE_ENC_PAK_ID, DDIPar_In, vaType, par));

            mfxI32 dQPMult = 1 - 2 * (pROI->ROIMode == MFX_ROI_MODE_PRIORITY && sps.RateControlMethod == MFX_RATECONTROL_CQP);

            std::transform(pROI->ROI, pROI->ROI + pROI->NumROI, pps.ROI
                , [qpBlk, dQPMult](RectData src)
            {
                ENCODE_ROI dst = {};

                // trimming should be done by driver
                dst.Roi.Left = (mfxU16)(src.Left >> qpBlk);
                dst.Roi.Top = (mfxU16)(src.Top >> qpBlk);
                // Driver expects a rect with the 'close' right bottom edge but
                // MSDK uses the 'open' edge rect, thus the right bottom edge which
                // is decreased by 1 below converts 'open' -> 'close' notation
                // We expect here boundaries are already aligned with the BlockSize
                // and Right > Left and Bottom > Top
                dst.Roi.Right = (mfxU16)(src.Right >> qpBlk) - 1;
                dst.Roi.Bottom = (mfxU16)(src.Bottom >> qpBlk) - 1;
                dst.PriorityLevelOrDQp = (mfxI8)(dQPMult * src.DeltaQP);

                return dst;
            });

            auto pEnd = std::remove_if(pps.ROI, pps.ROI + pps.NumROI
                , [](ENCODE_ROI roi)
            {
                return (roi.Roi.Left >= roi.Roi.Right || roi.Roi.Top >= roi.Roi.Bottom);
            });

            pps.NumROI = UCHAR(pEnd - pps.ROI);

            pps.MaxDeltaQp = 51; // used for BRC only
            pps.MinDeltaQp = -51;
        }

        return MFX_ERR_NONE;
    });
}

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
