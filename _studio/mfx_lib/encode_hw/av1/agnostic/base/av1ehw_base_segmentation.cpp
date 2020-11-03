// Copyright (c) 2019-2020 Intel Corporation
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
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_base_general.h"
#include "av1ehw_base_segmentation.h"

using namespace AV1EHW;
using namespace AV1EHW::Base;

namespace AV1EHW
{

template<typename T>
inline bool CheckAndFixFeature(
    const ENCODE_CAPS_AV1& caps
    , mfxU16& featureEnabled
    , T& value
    , SEG_LVL_FEATURES feature)
{
    bool changed = false;

    if (IsFeatureEnabled(featureEnabled, feature) && !IsFeatureSupported(caps, feature))
    {
        DisableFeature(featureEnabled, feature);
        changed = true;
    }

    if (value && !IsFeatureEnabled(featureEnabled, feature))
    {
        value = 0;
        changed = true;
    }

    mfxI32 limit = SEGMENTATION_FEATURE_MAX[feature];
    if (SEGMENTATION_FEATURE_SIGNED[feature])
        changed |= CheckRangeOrClip(value, -limit, limit);
    else
        changed |= CheckRangeOrClip(value, 0, limit);

    return changed;
}

mfxU32 CheckAndFixSegmentParam(
    const ENCODE_CAPS_AV1& caps
    , mfxAV1SegmentParam& seg)
{
    mfxU32 changed = 0;

    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltQIndex, SEG_LVL_ALT_Q);
    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltLFLevelYVert, SEG_LVL_ALT_LF_Y_V);
    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltLFLevelYHorz, SEG_LVL_ALT_LF_Y_H);
    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltLFLevelU, SEG_LVL_ALT_LF_U);
    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.AltLFLevelV, SEG_LVL_ALT_LF_V);
    changed += CheckAndFixFeature(caps, seg.FeatureEnabled, seg.ReferenceFrame , SEG_LVL_REF_FRAME);

    return changed;
}

inline bool CheckAndZeroSegmentParam(
    mfxAV1SegmentParam& seg)
{
    bool changed = false;

    changed |= seg.FeatureEnabled != 0;
    changed |= seg.AltQIndex != 0;
    changed |= seg.AltLFLevelYVert != 0;
    changed |= seg.AltLFLevelYHorz != 0;
    changed |= seg.AltLFLevelU != 0;
    changed |= seg.AltLFLevelV != 0;
    changed |= seg.ReferenceFrame != 0;

    if (changed)
        seg = {};

    return changed;
}

static mfxU32 CheckSegmentMap(
    const mfxVideoParam& par
    , const mfxExtAV1Param& av1Par
    , mfxExtAV1Segmentation& seg)
{
    mfxU32 invalid = 0;

    mfxFrameInfo  fiTemp  = par.mfx.FrameInfo;
    mfxU16 frameWidthTemp = av1Par.FrameWidth;
    mfxU16 frameHeightTemp = av1Par.FrameHeight;
    SetDefaultFrameInfo(frameWidthTemp, frameHeightTemp, fiTemp);

    mfxU16 frameWidth = frameWidthTemp ? frameWidthTemp : fiTemp.Width;
    frameWidth        = GetActualEncodeWidth(frameWidth, IsOn(av1Par.EnableSuperres), av1Par.SuperresScaleDenominator);

    const mfxU16 frameHeight = frameHeightTemp ? frameHeightTemp : fiTemp.Height;

    const mfxU16 blockSize = seg.SegmentIdBlockSize > 0 ? seg.SegmentIdBlockSize : MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32;
    const mfxU16 widthInBlocks = CeilDiv(frameWidth, blockSize);
    const mfxU16 heightInBlocks = CeilDiv(frameHeight, blockSize);

    if (seg.NumSegmentIdAlloc && seg.NumSegmentIdAlloc < static_cast<mfxU32>(widthInBlocks) * heightInBlocks)
        invalid++;

    if (seg.SegmentId && seg.NumSegments)
    {
        for (mfxU32 i = 0; i < seg.NumSegmentIdAlloc; ++i)
        {
            if (seg.SegmentId[i] >= seg.NumSegments)
            {
                invalid++;
                break;
            }
        }
    }

    return invalid;
}

static mfxU8 GetLastActiveSegId(const mfxAV1SegmentParam (&segParams)[AV1_MAX_NUM_OF_SEGMENTS])
{
    // the logic is from AV1 spec 5.9.14

    mfxU8 LastActiveSegId = 0;
    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; i++)
        if (segParams[i].FeatureEnabled)
            LastActiveSegId = i;

    return LastActiveSegId;
}

static mfxU32 CheckNumSegments(mfxExtAV1Segmentation& seg)
{
    mfxU32 invalid = 0;

    invalid += CheckMaxOrZero(seg.NumSegments, AV1_MAX_NUM_OF_SEGMENTS);
    // By AV1 spec 6.10.8 segment_id must be in the range 0 to LastActiveSegId. Following check helps to ensure it.
    invalid += CheckMaxOrZero(seg.NumSegments, GetLastActiveSegId(seg.Segment) + 1);

    return invalid;
}

//this function not needed for Silicon
mfxU32 CheckHWLimitations(mfxExtAV1Segmentation& seg)
{
    mfxU32 invalid = 0;

    if (seg.NumSegments)
    {
        const mfxAV1SegmentParam lastSeg = seg.Segment[seg.NumSegments - 1];

        if (IsFeatureEnabled(lastSeg.FeatureEnabled, SEG_LVL_ALT_Q) && !lastSeg.AltQIndex)
        {
            // last segment with zero delta QP isn't supported by HW
            invalid++;
        }
    }

    return invalid;
}

mfxStatus CheckAndFixSegmentBuffers(
    const mfxVideoParam& par
    , const ENCODE_CAPS_AV1& caps
    , const mfxExtAV1Param* pAV1
    , mfxExtAV1Segmentation* pSeg)
{
    // If Segmentation is not explicitly enabled through SegmentationMode, it will assumed disabled
    MFX_CHECK(IsSegmentationEnabled(pAV1), MFX_ERR_NONE);

    // Only support Force Segmentation currently
    MFX_CHECK(!IsAutoSegmentationEnabled(pAV1), MFX_ERR_UNSUPPORTED);
    MFX_CHECK(IsForceSegmentationEnabled(pAV1), MFX_ERR_UNSUPPORTED);

    MFX_CHECK(pSeg,  MFX_ERR_NONE);

    mfxU32 invalid = 0, changed = 0;

    invalid += !caps.ForcedSegmentationSupport;

    // Further parameter check hardly rely on NumSegments. Cannot fix value of NumSegments and then use
    // modified value for checks. Need to drop and return MFX_ERR_UNSUPPORTED
    invalid += CheckNumSegments(*pSeg);

    invalid += CheckSegmentMap(par, *pAV1, *pSeg);

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);

    for (mfxU16 i = 0; i < pSeg->NumSegments; i++)
    {
        changed += CheckAndFixSegmentParam(caps, pSeg->Segment[i]);
    }

    // clean out per-segment parameters for segments with numbers exceeding seg.NumSegments
    for (mfxU16 i = pSeg->NumSegments; i < AV1_MAX_NUM_OF_SEGMENTS; ++i)
    {
        changed += CheckAndZeroSegmentParam(pSeg->Segment[i]);
    }

    MFX_CHECK(!invalid, MFX_ERR_UNSUPPORTED);
    MFX_CHECK(!changed, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

    return MFX_ERR_NONE;
}

void Segmentation::SetSupported(ParamSupport& blocks)
{
    blocks.m_ebCopySupported[MFX_EXTBUFF_AV1_SEGMENTATION].emplace_back(
        [](const mfxExtBuffer* pSrc, mfxExtBuffer* pDst) -> void
    {
        const auto& buf_src = *(const mfxExtAV1Segmentation*)pSrc;
        auto& buf_dst = *(mfxExtAV1Segmentation*)pDst;

        MFX_COPY_FIELD(NumSegments);

        for (mfxU32 i = 0; i < buf_src.NumSegments; i++)
        {
            MFX_COPY_FIELD(Segment[i]);
        }

        MFX_COPY_FIELD(SegmentIdBlockSize);
        MFX_COPY_FIELD(NumSegmentIdAlloc);
        MFX_COPY_FIELD(SegmentId);
        MFX_COPY_FIELD(TemporalUpdate);
    });
}

void Segmentation::Query1WithCaps(const FeatureBlocks& /*blocks*/, TPushQ1 Push)
{
    Push(BLK_CheckAndFix
        , [this](const mfxVideoParam& /*in*/, mfxVideoParam& par, StorageW& global) -> mfxStatus
    {
        const auto& caps = Glob::EncodeCaps::Get(global);
        const mfxExtAV1Param* pAV1 = ExtBuffer::Get(par);
        mfxExtAV1Segmentation* pSeg = ExtBuffer::Get(par);

        auto sts = CheckAndFixSegmentBuffers(par, caps, pAV1, pSeg);
        MFX_CHECK_STS(sts);

        return MFX_ERR_NONE;
    });
}

void Segmentation::SetDefaults(const FeatureBlocks& /*blocks*/, TPushSD Push)
{
    Push(BLK_SetDefaults
        , [this](mfxVideoParam& par, StorageW&, StorageRW&)
    {
        mfxExtAV1Segmentation* pSeg = ExtBuffer::Get(par);
        if (pSeg)
            SetDefault(pSeg->SegmentIdBlockSize, MFX_AV1_SEGMENT_ID_BLOCK_SIZE_32x32);

        return MFX_ERR_NONE;
    });
}

void Segmentation::AllocTask(const FeatureBlocks& blocks, TPushAT Push)
{
    Push(BLK_AllocTask
        , [this, &blocks](
            StorageR& /*global*/
            , StorageRW& task) -> mfxStatus
        {
            task.Insert(Task::Segment::Key, new MakeStorable<Task::Segment::TRef>);
            return MFX_ERR_NONE;
        });
}

static void MergeSegParam(
    const mfxExtAV1Segmentation& initSeg
    , const mfxExtAV1Segmentation& frameSeg
    , mfxExtAV1Segmentation& mergedSeg)
{
    assert(frameSeg.NumSegments); // we assume that MergeSegParam is called for active per-frame segment configuration

    // (1) take Init ext buffer
    mergedSeg = initSeg;

    // (2) switch to Frame per-segment param
    mergedSeg.NumSegments = frameSeg.NumSegments;
    const auto emptyPar = mfxAV1SegmentParam{};
    std::fill_n(mergedSeg.Segment, AV1_MAX_NUM_OF_SEGMENTS, emptyPar);
    std::copy_n(frameSeg.Segment, frameSeg.NumSegments, mergedSeg.Segment);

    // (3) switch to Frame segment map (if provided)
    if (frameSeg.SegmentId)
    {
        mergedSeg.NumSegmentIdAlloc = frameSeg.NumSegmentIdAlloc;
        mergedSeg.SegmentId = frameSeg.SegmentId;
        mergedSeg.SegmentIdBlockSize = frameSeg.SegmentIdBlockSize;
    }
}

inline bool CheckForCompleteness(const mfxExtAV1Segmentation& seg)
{
    return seg.NumSegments && seg.SegmentId && seg.NumSegmentIdAlloc && seg.SegmentIdBlockSize;
}

static mfxStatus SetFinalSegParam(
    const mfxVideoParam& par
    , const ENCODE_CAPS_AV1& caps
    , const mfxExtAV1Segmentation& initSeg
    , const mfxExtAV1Segmentation* pFrameSeg
    , mfxExtAV1Segmentation& finalSeg)
{
    const auto emptySeg = mfxExtAV1Segmentation{};

    if (IsSegmentationSwitchedOff(pFrameSeg))
    {
        AV1E_LOG("  *DEBUG* STATUS: Segmentation was disabled for current frame!\n");
        finalSeg = emptySeg;
        return MFX_ERR_NONE;
    }

    if (pFrameSeg)
    {
        MergeSegParam(initSeg, *pFrameSeg, finalSeg);

        const mfxExtAV1Param& av1Par = ExtBuffer::Get(par);
        const mfxStatus checkSts = CheckAndFixSegmentBuffers(par, caps, &av1Par, &finalSeg);

        if (checkSts < MFX_ERR_NONE)
        {
            AV1E_LOG("  *DEBUG* STATUS: Merge of Init and Frame mfxExtAV1Segmentation resulted in erroneous configuration!\n");
            finalSeg = emptySeg;
            return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }
    else
        finalSeg = initSeg;

    if (!CheckForCompleteness(finalSeg))
    {
        AV1E_LOG("  *DEBUG* STATUS: Merge of Init and Frame mfxExtAV1Segmentation didn't give complete segment params!\n");
        finalSeg = emptySeg;
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

void Segmentation::InitTask(const FeatureBlocks& blocks, TPushIT Push)
{
    Push(BLK_InitTask
        , [this, &blocks](
            mfxEncodeCtrl* /*pCtrl*/
            , mfxFrameSurface1* /*pSurf*/
            , mfxBitstream* /*pBs*/
            , StorageW& global
            , StorageW& task) -> mfxStatus
        {
            const auto& par = Glob::VideoParam::Get(global);
            const mfxExtAV1Param& av1Par = ExtBuffer::Get(par);

            if (!IsForceSegmentationEnabled(&av1Par))
                return MFX_ERR_NONE;

            mfxExtAV1Segmentation *pFrameSeg = ExtBuffer::Get(Task::Common::Get(task).ctrl);

            mfxStatus checkSts = MFX_ERR_NONE;
            const auto& caps = Glob::EncodeCaps::Get(global);

            if (pFrameSeg && !IsSegmentationSwitchedOff(pFrameSeg))
            {
                checkSts = CheckAndFixSegmentBuffers(par, caps, &av1Par, pFrameSeg);

                if (checkSts < MFX_ERR_NONE)
                {
                    // ignore Frame segment param and return warning if there are issues MSDK can't fix
                    pFrameSeg = nullptr;
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                    AV1E_LOG("  *DEBUG* STATUS: Critical issue in Frame mfxExtAV1Segmentation - it's ignored!\n");
                }
            }

            const mfxExtAV1Segmentation& initSeg = ExtBuffer::Get(par);
            mfxExtAV1Segmentation& finalSeg = Task::Segment::Get(task);
            const mfxStatus mergeSts = SetFinalSegParam(par, caps, initSeg, pFrameSeg, finalSeg);

            return checkSts == MFX_ERR_NONE ? mergeSts : checkSts;
        });
}

inline std::tuple<bool, bool> NeedMapUpdate(const mfxExtAV1Segmentation& currPar, const mfxExtAV1Segmentation& refPar)
{
    bool updateMap = true;
    bool temporalUpdate = false;

    if (!IsSegmentationSwitchedOff(&refPar))
    {
        if (currPar.SegmentIdBlockSize == refPar.SegmentIdBlockSize
            && currPar.NumSegments == refPar.NumSegments)
        {
            assert(currPar.SegmentId);
            assert(refPar.SegmentId);

            const mfxU32 mapSize = std::min(currPar.NumSegmentIdAlloc, refPar.NumSegmentIdAlloc);

            if (std::equal(currPar.SegmentId, currPar.SegmentId + mapSize, refPar.SegmentId))
                updateMap = false;
        }

        // default value of TemporalUpdate (MFX_CODINGOPTION_UNKNOWN) is treated as ON
        // (we assume that in average enabled segmentation_temporal_update saves bits in comparison with disabled)
        // TODO: implement setting of default value for TemporalUpdate so it's always ON or OFF
        temporalUpdate = updateMap && !IsOff(currPar.TemporalUpdate);
    }

    return std::make_tuple(updateMap, temporalUpdate);
}

inline bool NeedParUpdate(const mfxExtAV1Segmentation& currPar, const mfxExtAV1Segmentation& refPar)
{
    if (currPar.NumSegments != refPar.NumSegments)
        return true;

    auto EqualParam = [](const mfxAV1SegmentParam& left, const mfxAV1SegmentParam& right)
    {
        return left.FeatureEnabled  == right.FeatureEnabled
            && left.AltQIndex       == right.AltQIndex
            && left.AltLFLevelYHorz == right.AltLFLevelYVert
            && left.AltLFLevelYVert == right.AltLFLevelYHorz
            && left.AltLFLevelU     == right.AltLFLevelU
            && left.AltLFLevelV     == right.AltLFLevelV
            && left.ReferenceFrame  == right.ReferenceFrame;
    };

    if (std::equal(currPar.Segment, currPar.Segment + currPar.NumSegments, refPar.Segment, EqualParam))
        return false;
    else
        return true;
}

mfxStatus Segmentation::UpdateFrameHeader(
    const mfxExtAV1Segmentation& currPar
    , const FH& fh
    , SegmentationParams& seg) const
{
    seg = SegmentationParams{};

    MFX_CHECK(currPar.NumSegments, MFX_ERR_NONE);

    seg.segmentation_enabled = 1;

    if (fh.primary_ref_frame == PRIMARY_REF_NONE) // AV1 spec 5.9.14
    {
        seg.segmentation_update_map = 1;
        seg.segmentation_temporal_update = 0;
        seg.segmentation_update_data = 1;
    }
    else
    {
        const mfxU8 primaryRefFrameDpbIdx = static_cast<mfxU8>(fh.ref_frame_idx[fh.primary_ref_frame]);
        const mfxExtAV1Segmentation* pRefPar = dpb.at(primaryRefFrameDpbIdx).get();
        assert(pRefPar);
        std::tie(seg.segmentation_update_map, seg.segmentation_temporal_update) =
            NeedMapUpdate(currPar, *pRefPar);
        seg.segmentation_update_data = NeedParUpdate(currPar, *pRefPar);
    }

    for (mfxU8 i = 0; i < AV1_MAX_NUM_OF_SEGMENTS; ++i)
    {
        seg.FeatureMask[i] = static_cast<uint32_t>(currPar.Segment[i].FeatureEnabled);
        seg.FeatureData[i][SEG_LVL_ALT_Q]      = currPar.Segment[i].AltQIndex;
        seg.FeatureData[i][SEG_LVL_ALT_LF_Y_V] = currPar.Segment[i].AltLFLevelYVert;
        seg.FeatureData[i][SEG_LVL_ALT_LF_Y_H] = currPar.Segment[i].AltLFLevelYHorz;
        seg.FeatureData[i][SEG_LVL_ALT_LF_U]   = currPar.Segment[i].AltLFLevelU;
        seg.FeatureData[i][SEG_LVL_ALT_LF_V]   = currPar.Segment[i].AltLFLevelV;
        seg.FeatureData[i][SEG_LVL_REF_FRAME]  = currPar.Segment[i].ReferenceFrame;
        if (IsFeatureEnabled(currPar.Segment[i].FeatureEnabled, SEG_LVL_SKIP))
            seg.FeatureData[i][SEG_LVL_SKIP] = 1;
        if (IsFeatureEnabled(currPar.Segment[i].FeatureEnabled, SEG_LVL_GLOBALMV))
            seg.FeatureData[i][SEG_LVL_GLOBALMV] = 1;
    }

    return MFX_ERR_NONE;
}

static void RetainSegMap(mfxExtAV1Segmentation& par)
{
    const mfxU8* pMap = par.SegmentId;
    par.SegmentId = new mfxU8[par.NumSegmentIdAlloc];
    std::copy_n(pMap, par.NumSegmentIdAlloc, par.SegmentId);
}

static void PutSegParamToDPB(const mfxExtAV1Segmentation& par
    , mfxU16 numRefFrame
    , const DpbRefreshType& refreshFrameFlags
    , Segmentation::SegDpbType& dpb)
{
    if (dpb.empty())
        dpb.resize(numRefFrame);

    auto SegParamReleaser = [](mfxExtAV1Segmentation* pSeg)
    {
        delete []pSeg->SegmentId;
        delete pSeg;
    };

    UpdateDPB(dpb, par, refreshFrameFlags, SegParamReleaser);
}

void Segmentation::PostReorderTask(const FeatureBlocks& /*blocks*/, TPushPostRT Push)
{
    Push(BLK_ConfigureTask
        , [this](
            StorageW& global
            , StorageW& s_task) -> mfxStatus
    {
        mfxExtAV1Segmentation& seg = Task::Segment::Get(s_task);;
        FH& fh = Task::FH::Get(s_task);

        auto sts = UpdateFrameHeader(seg, fh, fh.segmentation_params);
        MFX_CHECK_STS(sts);

        if (fh.refresh_frame_flags)
        {
            RetainSegMap(seg);

            const auto& numRefFrame = Glob::VideoParam::Get(global).mfx.NumRefFrame;
            const auto& refreshFrameFlags = Task::Common::Get(s_task).RefreshFrameFlags;

            PutSegParamToDPB(seg, numRefFrame, refreshFrameFlags, dpb);
        }

        if (!fh.segmentation_params.segmentation_update_map)
        {
            seg.SegmentId = nullptr;
            seg.NumSegmentIdAlloc = 0;
        }


        return MFX_ERR_NONE;
    });
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
