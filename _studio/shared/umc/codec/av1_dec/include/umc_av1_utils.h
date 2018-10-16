//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2018 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_defs.h"
#ifdef UMC_ENABLE_AV1_VIDEO_DECODER

#ifndef __UMC_AV1_UTILS_H_
#define __UMC_AV1_UTILS_H_

#include "umc_av1_dec_defs.h"

namespace UMC_AV1_DECODER
{
    inline uint32_t CeilLog2(uint32_t x) { uint32_t l = 0; while (x > (1U << l)) l++; return l; }

    // we stop using UMC_VP9_DECODER namespace starting from Rev 0.25.2
    // because after switch to AV1-specific segmentation stuff there are only few definitions we need to re-use from VP9
    void SetSegData(SegmentationParams & seg, uint8_t segmentId, SEG_LVL_FEATURES featureId, int32_t seg_data);

    inline int32_t GetSegData(SegmentationParams const& seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.FeatureData[segmentId][featureId];
    }

    inline bool IsSegFeatureActive(SegmentationParams const& seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        return
            seg.segmentation_enabled && (seg.FeatureMask[segmentId] & (1 << featureId));
    }

    inline void EnableSegFeature(SegmentationParams & seg, uint8_t segmentId, SEG_LVL_FEATURES featureId)
    {
        seg.FeatureMask[segmentId] |= 1 << featureId;
    }

    inline uint8_t IsSegFeatureSigned(SEG_LVL_FEATURES featureId)
    {
        return
            SEG_FEATURE_DATA_SIGNED[featureId];
    }

    inline void ClearAllSegFeatures(SegmentationParams & seg)
    {
        memset(&seg.FeatureData, 0, sizeof(seg.FeatureData));
        memset(&seg.FeatureMask, 0, sizeof(seg.FeatureMask));
    }

    void SetupPastIndependence(FrameHeader & info);

    inline bool FrameIsIntraOnly(FrameHeader const& fh)
    {
        return fh.frame_type == INTRA_ONLY_FRAME;
    }

    inline bool FrameIsIntra(FrameHeader const& fh)
    {
        return (fh.frame_type == KEY_FRAME || FrameIsIntraOnly(fh));
    }

    inline bool FrameIsResilient(FrameHeader const& fh)
    {
        return FrameIsIntra(fh) || fh.error_resilient_mode;
    }

    inline uint32_t NumTiles(TileInfo const & info)
    {
        return info.TileCols * info.TileRows;
    }

    int IsCodedLossless(FrameHeader const&);

    void InheritFromPrevFrame(FrameHeader&, FrameHeader const&);

    inline bool FrameMightUsePrevFrameMVs(FrameHeader const& fh, SequenceHeader const& sh)
    {
#if UMC_AV1_DECODER_REV >= 8500
        return !FrameIsResilient(fh) && sh.enable_order_hint && sh.enable_ref_frame_mvs;
#else
        const bool large_scale_tile = 0; // this parameter isn't taken from the bitstream. Looks like decoder gets it from outside (e.g. container or some environment).
        return !FrameIsResilient(fh) && sh.enable_order_hint && !large_scale_tile;
#endif
    }

#if UMC_AV1_DECODER_REV == 5000
    inline bool FrameCanUsePrevFrameMVs(FrameHeader const& fh, SequenceHeader const& sh, FrameHeader const* prev_fh = 0)
    {
        return (FrameMightUsePrevFrameMVs(fh, sh) &&
            prev_fh &&
            !FrameIsIntraOnly(*prev_fh) &&
            fh.FrameWidth == prev_fh->FrameWidth &&
            fh.FrameHeight == prev_fh->FrameHeight);
    }
#endif

    inline void SetDefaultLFParams(LoopFilterParams& par)
    {
        par.loop_filter_delta_enabled = 1;
        par.loop_filter_delta_update = 1;

        par.loop_filter_ref_deltas[INTRA_FRAME] = 1;
        par.loop_filter_ref_deltas[LAST_FRAME] = 0;
        par.loop_filter_ref_deltas[LAST2_FRAME] = par.loop_filter_ref_deltas[LAST_FRAME];
        par.loop_filter_ref_deltas[LAST3_FRAME] = par.loop_filter_ref_deltas[LAST_FRAME];
        par.loop_filter_ref_deltas[BWDREF_FRAME] = par.loop_filter_ref_deltas[LAST_FRAME];
        par.loop_filter_ref_deltas[GOLDEN_FRAME] = -1;
        par.loop_filter_ref_deltas[ALTREF2_FRAME] = -1;
        par.loop_filter_ref_deltas[ALTREF_FRAME] = -1;

        par.loop_filter_mode_deltas[0] = 0;
        par.loop_filter_mode_deltas[1] = 0;
    }

    inline uint8_t GetNumPlanes(SequenceHeader const& sh)
    {
        return sh.color_config.mono_chrome ? 1 : MAX_MB_PLANE;
    }
}

#endif //__UMC_AV1_UTILS_H_
#endif //UMC_ENABLE_AV1_VIDEO_DECODER
