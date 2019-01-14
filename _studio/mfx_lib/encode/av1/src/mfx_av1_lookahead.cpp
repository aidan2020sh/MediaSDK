// Copyright (c) 2014-2019 Intel Corporation
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

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "math.h"
#include "numeric"
#include "algorithm"

#include "ippi.h"

#include "mfx_av1_encode.h"
#include "mfx_av1_frame.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_lookahead.h"
#include "mfx_av1_defs.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_quant.h"
#include "mfx_av1_dispatcher_wrappers.h"
#include <tmmintrin.h>

using namespace AV1Enc;

//#define PAQ_LOGGING
template <typename PixType>
static
    void GetPredPelsLuma(PixType* src, int32_t srcPitch, int32_t x, int32_t y, IppiSize roi, PixType* PredPel, int32_t width, uint8_t bitDepthLuma)
{
    PixType* tmpSrcPtr;
    PixType dcval;
    int32_t i;

    dcval = (1 << (bitDepthLuma - 1));

    if ((x | y) == 0) {
        // Fill border with DC value
        for (i = 0; i <= 4*width; i++)
            PredPel[i] = dcval;
    } else if (x == 0) {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        int32_t hlim = std::min(roi.width - x, 2*width);
        for (i = 0; i < hlim; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[i];
        }
        for (i = hlim; i < 2*width; i++) {
            PredPel[2*width + 1 + i] = dcval;
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];
        }
    } else if (y == 0) {
        // Fill top and top right border with rec. samples
        tmpSrcPtr = src - 1;
        dcval = tmpSrcPtr[0];
        PredPel[0] = dcval;
        int32_t vlim = std::min(roi.height - y, 2*width);
        for (i = 0; i < vlim; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++) {
            PredPel[1 + i] = dcval;
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
        }
    } else {
        // Fill top-left border with rec. samples
        tmpSrcPtr = src - srcPitch - 1;
        PredPel[0] = tmpSrcPtr[0];

        // Fill top and top right border with rec. samples
        int32_t hlim = std::min(roi.width - x, 2*width);
        tmpSrcPtr = src - srcPitch;
        for (i = 0; i < hlim; i++)
            PredPel[1 + i] = tmpSrcPtr[i];
        for (i = hlim; i < 2*width; i++)
            PredPel[1 + i] = tmpSrcPtr[hlim - 1];

        // Fill left and below left border with rec. samples
        int32_t vlim = std::min(roi.height - y, 2*width);
        tmpSrcPtr = src - 1;
        for (i = 0; i < vlim; i++) {
            PredPel[2*width + 1 + i] = tmpSrcPtr[0];
            tmpSrcPtr += srcPitch;
        }
        for (i = vlim; i < 2*width; i++)
            PredPel[2*width + 1 + i] = tmpSrcPtr[-srcPitch];
    }
}

static inline int32_t H265_Calc_SATD(uint8_t* pSrc, int32_t srcStep, uint8_t* pRef, int32_t width)
{
    int32_t numblks = width >> 3;
    int32_t satd = 0;
    for (int32_t i = 0; i < numblks; i++) {
        for (int32_t j = 0; j < numblks; j++) {
            uint8_t *pS = pSrc + i*8*srcStep + j*8;
            uint8_t *pR = pRef + i*8*width + j*8;
            satd += AV1PP::satd_8x8(pS, srcStep, pR, width);
        }
    }
    satd = (satd + 2) >> 2; // to allign with hevce satd
    return satd;
}

static inline int32_t H265_Calc_SATD(uint16_t* pSrc, int32_t srcStep, uint16_t* pRef, int32_t width)
{
    int32_t numblks = width >> 3;
    int32_t satd = 0;
    for (int32_t i = 0; i < numblks; i++) {
        for (int32_t j = 0; j < numblks; j++) {
            uint16_t *pS = pSrc + i*8*srcStep + j*8;
            uint16_t *pR = pRef + i*8*width + j*8;
            satd +=  AV1PP::satd_8x8(pS, srcStep, pR, width);
        }
    }
    satd = (satd + 2) >> 2;// to allign with hevce satd
    return satd;
}


template <typename PixType>
void MeInterpolate(const AV1MEInfo* me_info, const AV1MV *MV, PixType *src,
                   int32_t srcPitch, PixType *dst, int32_t dstPitch, int32_t pelX, int32_t pelY)
{
    alignas(64) int16_t preAvgTmpBuf[72*64];
    int32_t w = me_info->width;
    int32_t h = me_info->height;
    int32_t dx = MV->mvx & 3;
    int32_t dy = MV->mvy & 3;
    int32_t bitDepth = sizeof(PixType) == 1 ? 8 : 10;

    int32_t m_ctbPelX = pelX;
    int32_t m_ctbPelY = pelY;

    int32_t refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 2) + (m_ctbPelY + me_info->posy + (MV->mvy >> 2)) * srcPitch;
    src += refOffset;

    assert (!(dx == 0 && dy == 0));
    if (dy == 0) {
        //InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, PixType>( INTERP_HOR, src, srcPitch, dst, dstPitch, dx, w, h, 6, 32, bitDepth, preAvgTmpBuf);
    } else if (dx == 0) {
        //InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, PixType>( INTERP_VER, src, srcPitch, dst, dstPitch, dy, w, h, 6, 32, bitDepth, preAvgTmpBuf);
    } else {
        int16_t tmpBuf[80 * 80];
        int16_t *tmp = tmpBuf + 80 * 8 + 8;
        int32_t tmpPitch = 80;

        //InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, PixType, int16_t>( INTERP_HOR, src - 1 * srcPitch, srcPitch, tmp, tmpPitch, dx, w, h + 3, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);

        int32_t shift  = 20 - bitDepth;
        int16_t offset = 1 << (shift - 1);

        //InterpolateEncFast<UMC_HEVC_DECODER::TEXT_LUMA, int16_t, PixType>( INTERP_VER,  tmp + 1 * tmpPitch, tmpPitch, dst, dstPitch, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf);
    }

    return;
} //


template <typename PixType>
int32_t MatchingMetricPu(const PixType *src, int32_t srcPitch, FrameData *refPic, const AV1MEInfo *meInfo, const AV1MV *mv, int32_t useHadamard, int32_t pelX, int32_t pelY)
{
    int32_t refOffset = pelX + meInfo->posx + (mv->mvx >> 2) + (pelY + meInfo->posy + (mv->mvy >> 2)) * refPic->pitch_luma_pix;
    const PixType *rec = (PixType*)refPic->y + refOffset;
    int32_t pitchRec = refPic->pitch_luma_pix;
    alignas(32) PixType predBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    if ((mv->mvx | mv->mvy) & 3) {
        MeInterpolate(meInfo, mv, (PixType*)refPic->y, refPic->pitch_luma_pix, predBuf, MAX_CU_SIZE, pelX, pelY);
        rec = predBuf;
        pitchRec = MAX_CU_SIZE;

        if(useHadamard)
            return (AV1PP::satd(src, srcPitch, rec, pitchRec, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA));
        else
            return AV1PP::sad_general(src, srcPitch, rec, pitchRec, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA);

    } else {
        if(useHadamard)
            return (AV1PP::satd(src, srcPitch, rec, pitchRec, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA));
        else
            return AV1PP::sad_general(src, srcPitch, rec, pitchRec, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA);
    }
}

template
    int32_t MatchingMetricPu<uint8_t>(const uint8_t *src, int32_t srcPitch, FrameData *refPic, const AV1MEInfo *meInfo, const AV1MV *mv, int32_t useHadamard, int32_t pelX, int32_t pelY);

template <typename PixType>
int32_t IntraPredSATD(PixType *pSrc, int32_t srcPitch, int32_t x, int32_t y, IppiSize roi, int32_t width, uint8_t bitDepthLuma, int32_t modes)
{
    typedef typename GetHistogramType<PixType>::type HistType;
    PixType PredPel[4*2*64+1];
    alignas(32) PixType recPel[MAX_CU_SIZE * MAX_CU_SIZE];
    alignas(32) HistType hist8[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];

    pSrc += y*srcPitch + x;
    int32_t numblks = width >> 3;

    GetPredPelsLuma(pSrc, srcPitch, x, y, roi, PredPel, width, bitDepthLuma);

    //h265_PredictIntra_DC(PredPel, recPel, width, width, 1);
    int32_t bestSATD = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if(!modes) return bestSATD;

    //h265_PredictIntra_Ver(PredPel, recPel, width, width, bitDepthLuma, 1);
    int32_t satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    //h265_PredictIntra_Hor(PredPel, recPel, width, width, bitDepthLuma, 1);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    //h265_AnalyzeGradient(pSrc, srcPitch, hist8, hist8, width, width);
    if (numblks > 1) {
        int32_t pitch = 40 * numblks;
        const HistType *histBlock = hist8;
        for (int32_t py = 0; py < numblks; py++, histBlock += pitch)
            for (int32_t px = 0; px < numblks; px++)
                if (px | py) {
                    for (int32_t i = 0; i < 35; i++)
                        hist8[i] += histBlock[40 * px + i];
                }
    }
    int32_t angmode = (int32_t)(std::max_element(hist8 + 2, hist8 + 35) - hist8);

    //h265_PredictIntra_Planar(PredPel, recPel, width, width);
    satd = H265_Calc_SATD(pSrc, srcPitch, recPel, width);
    if (satd < bestSATD)
        bestSATD = satd;

    return bestSATD;
}

template int32_t IntraPredSATD<uint8_t>(uint8_t *pSrc, int32_t srcPitch, int32_t x, int32_t y, IppiSize roi, int32_t width, uint8_t bitDepthLuma, int32_t modes);

template <typename PixType>
void DoIntraAnalysis(FrameData* frame, int32_t* intraCost)
{
    int32_t width  = frame->width;
    int32_t height = frame->height;
    PixType* luma   = (PixType*)frame->y;
    uint8_t bitDepth = sizeof(PixType) == 1 ? 8 : 10;;
    int32_t pitch_luma_pix = frame->pitch_luma_pix;
    uint8_t  bitDepthLuma = bitDepth;

    // analysis for blk 8x8
    const int32_t sizeBlk = SIZE_BLK_LA;
    const int32_t picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const int32_t picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {width, height};

    for (int32_t blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (int32_t blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            int32_t x = blk_col*sizeBlk;
            int32_t y = blk_row*sizeBlk;
            int32_t fPos = blk_row * picWidthInBlks + blk_col;
            intraCost[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, bitDepthLuma);
        }
    }
}

template <typename PixType>
void DoIntraAnalysis_OneRow(FrameData* frame, int32_t* intraCost, int32_t row, int32_t sizeBlk, int32_t modes)
{
    int32_t width  = frame->width;
    int32_t height = frame->height;
    PixType* luma   = (PixType*)frame->y;
    uint8_t bitDepth = sizeof(PixType) == 1 ? 8 : 10;;
    int32_t pitch_luma_pix = frame->pitch_luma_pix;
    uint8_t  bitDepthLuma = bitDepth;

    // analysis for blk 8x8
    //const int32_t sizeBlk = SIZE_BLK_LA;
    const int32_t picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const int32_t picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;
    const IppiSize frmSize = {width, height};

    //for (int32_t blk_row = 0; blk_row < picHeightInBlks; blk_row++)
    int32_t blk_row = row;
    {
        for (int32_t blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            int32_t x = blk_col*sizeBlk;
            int32_t y = blk_row*sizeBlk;
            int32_t fPos = blk_row * picWidthInBlks + blk_col;
            intraCost[fPos] = IntraPredSATD(luma, pitch_luma_pix, x, y, frmSize, sizeBlk, bitDepthLuma, modes);
        }
    }
}

int32_t ClipMV(AV1MV& rcMv, int32_t HorMin, int32_t HorMax, int32_t VerMin, int32_t VerMax)
{
    int32_t change = 0;
    if (rcMv.mvx < HorMin) {
        change = 1;
        rcMv.mvx = (int16_t)HorMin;
    } else if (rcMv.mvx > HorMax) {
        change = 1;
        rcMv.mvx = (int16_t)HorMax;
    }
    if (rcMv.mvy < VerMin) {
        change = 1;
        rcMv.mvy = (int16_t)VerMin;
    } else if (rcMv.mvy > VerMax) {
        change = 1;
        rcMv.mvy = (int16_t)VerMax;
    }

    return change;
}

const int16_t tab_mePatternSelector[2][12][2] = {
    {{0,0}, {0,-1},  {1,0},  {0,1},  {-1,0}, {0,0}, {0,0}, {0,0},  {0,0},  {0,0},   {0,0},  {0,0}}, //diamond
    {{0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0},  {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}} //box
};

// Cyclic pattern to avoid trying already checked positions
const int16_t tab_mePattern[1 + 8 + 3][2] = {
    {0,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1}, {0,-1}, {1,-1}
};

const struct TransitionTable {
    uint8_t start;
    uint8_t len;
} tab_meTransition[1 + 8 + 3] = {
    {1,8}, {7,5}, {1,3}, {1,5}, {3,3}, {3,5}, {5,3}, {5,5}, {7,3}, {7,5}, {1,3}, {1,5}
};

template <typename PixType, typename Frame>
void MeIntPelLog(Frame *curr, Frame *ref, int32_t pelX, int32_t pelY, int32_t posx, int32_t posy, AV1MV *mv, int32_t *cost, int32_t *cost_satd, int32_t *mvCost)
{
    int16_t meStepBest = 2;
    int16_t meStepMax = MAX(MIN(8, 8), 16) * 4;
    // regulation from outside
    AV1MV mvBest = *mv;
    int32_t costBest = *cost;
    int32_t mvCostBest = *mvCost;

    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    PixType *src = m_ySrc + posx + posy * curr->pitch_luma_pix;

    int32_t useHadamard = 0;
    int16_t mePosBest = 0;

    // limits to clip MV allover the CU
    const int32_t MvShift = 2;
    const int32_t offset = 8;
    const int32_t blkSize = SIZE_BLK_LA;
    int32_t HorMax = (curr->width + offset - pelX - 1) << MvShift;
    int32_t HorMin = ( - (int32_t)blkSize - offset - (int32_t) pelX + 1) << MvShift;
    int32_t VerMax = (curr->height + offset - pelY - 1) << MvShift;
    int32_t VerMin = ( - (int32_t) blkSize - offset - (int32_t) pelY + 1) << MvShift;

    // expanding search
    AV1MV mvCenter = mvBest;
    for (int16_t meStep = meStepBest; (1<<meStep) <= meStepMax; meStep ++) {
        for (int16_t mePos = 0; mePos < 9; mePos++) {
            AV1MV mv = {
                int16_t(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                int16_t(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };

            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            //int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
            AV1MEInfo meInfo;
            meInfo.posx = (uint8_t)posx;
            meInfo.posy = (uint8_t)posy;
            int32_t cost = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, &mv, useHadamard, pelX, pelY);

            if (costBest > cost) {
                int32_t mvCost = 0;//MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    meStepBest = meStep;
                    mePosBest = mePos;
                }
            }
        }
    }

    // then logarithm from best
    // for Cpattern
    int32_t transPos = mePosBest;
    uint8_t start = 0, len = 1;
    int16_t meStep = meStepBest;

    mvCenter = mvBest;
    int32_t iterbest = costBest;
    while (meStep >= 2) {
        int32_t refine = 1;
        int32_t bestPos = 0;
        for (int16_t mePos = start; mePos < start + len; mePos++) {
            AV1MV mv = {
                int16_t(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                int16_t(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };
            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            //int32_t cost = MatchingMetricPu(src, meInfo, &mv, ref, useHadamard);
            AV1MEInfo meInfo;
            meInfo.posx = (uint8_t)posx;
            meInfo.posy = (uint8_t)posy;
            int32_t cost = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, &mv, useHadamard, pelX, pelY);

            if (costBest > cost) {
                int32_t mvCost = 0;//MvCost1RefLog(mv, predInfo);
                cost += mvCost;
                if (costBest > cost) {
                    costBest = cost;
                    mvCostBest = mvCost;
                    mvBest = mv;
                    refine = 0;
                    bestPos = mePos;
                }
            }
        }

        if (refine) {
            meStep --;
        } else {
            mvCenter = mvBest;
        }
        start = tab_meTransition[bestPos].start;
        len   = tab_meTransition[bestPos].len;
        iterbest = costBest;
        transPos = bestPos;
    }

    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;

    if(cost_satd) {
        // recalc hadamar
        AV1MEInfo meInfo;
        meInfo.posx = (uint8_t)posx;
        meInfo.posy = (uint8_t)posy;
        useHadamard = 1;
        *cost_satd = MatchingMetricPu(src, curr->pitch_luma_pix, ref, &meInfo, mv, useHadamard, pelX, pelY);
    }
} //


template <typename PixType>
void MeIntPelLog_Refine(FrameData *curr, FrameData *ref, int32_t pelX, int32_t pelY, AV1MV *mv, int32_t *cost, int32_t *cost_satd, int32_t *mvCost, int32_t blkSize)
{
    int16_t meStepBest = 2;
    int16_t meStepMax = 8;

    int32_t log2w = BSR(blkSize >> 2);

    // regulation from outside
    AV1MV mvBest = *mv;
    int32_t costBest = *cost;
    int32_t mvCostBest = *mvCost;

    PixType *src = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;
    int32_t srcPitch = curr->pitch_luma_pix;
    int32_t useHadamard = 0;
    int16_t mePosBest = 0;

    // limits to clip MV allover the CU
    const int32_t MvShift = 2;
    const int32_t offset = 8;

    int32_t HorMax = (curr->width + offset - pelX - 1) << MvShift;
    int32_t HorMin = ( - (int32_t)blkSize - offset - (int32_t) pelX + 1) << MvShift;
    int32_t VerMax = (curr->height + offset - pelY - 1) << MvShift;
    int32_t VerMin = ( - (int32_t) blkSize - offset - (int32_t) pelY + 1) << MvShift;

    // expanding search
    AV1MV mvCenter = mvBest;
    for (int16_t meStep = meStepBest; (1<<meStep) <= meStepMax; meStep ++) {
        for (int16_t mePos = 0; mePos < 9; mePos++) {
            AV1MV mv = {
                int16_t(mvCenter.mvx + (tab_mePattern[mePos][0] << meStep)),
                int16_t(mvCenter.mvy + (tab_mePattern[mePos][1] << meStep))
            };

            //if (ClipMV(mv))
            if ( ClipMV(mv, HorMin, HorMax, VerMin, VerMax) )
                continue;

            int32_t refOffset = pelX + (mv.mvx >> 2) + (pelY + (mv.mvy >> 2)) * ref->pitch_luma_pix;
            const PixType *rec = (PixType*)ref->y + refOffset;
            int32_t pitchRec = ref->pitch_luma_pix;
            int32_t cost = AV1PP::sad_general(src, srcPitch, rec, pitchRec, log2w, log2w);

            if (costBest > cost) {
                costBest = cost;
                mvCostBest = 0;
                mvBest = mv;
                meStepBest = meStep;
                mePosBest = mePos;
            }
        }
    }
    *mv = mvBest;
    *cost = costBest;
    *mvCost = mvCostBest;
    if (cost_satd) {
        // recalc hadamar
        int32_t refOffset = pelX + (mv->mvx >> 2) + (pelY + (mv->mvy >> 2)) * ref->pitch_luma_pix;
        const PixType *rec = (PixType*)ref->y + refOffset;
        *cost_satd = AV1PP::satd(src, srcPitch, rec, ref->pitch_luma_pix, log2w, log2w);
    }
    return;
} //


void AddMvCost(int32_t log2Step, const int32_t *dists,
               AV1MV *mv, int32_t *costBest, int32_t *mvCostBest, int32_t patternSubPel)
{
    const int16_t (*pattern)[2] = tab_mePatternSelector[int32_t(patternSubPel == SUBPEL_BOX)] + 1;
    int32_t numMvs = (patternSubPel == SUBPEL_BOX) ? 8 : 4; // BOX or DIA

    AV1MV centerMv = *mv;
    for (int32_t i = 0; i < numMvs; i++) {
        int32_t cost = dists[i];
        if (*costBest > cost) {
            int16_t mvx = centerMv.mvx + (pattern[i][0] << log2Step);
            int16_t mvy = centerMv.mvy + (pattern[i][1] << log2Step);
            int32_t mvCost = 0;//MvCost1RefLog(mvx, mvy, predInfo);
            cost += mvCost;
            if (*costBest > cost) {
                *costBest = cost;
                *mvCostBest = mvCost;
                mv->mvx = mvx;
                mv->mvy = mvy;
            }
        }
    }
}

template <typename TSrc, typename TDst>
void InterpHor(const TSrc *src, int32_t pitchSrc, TDst *dst, int32_t pitchDst, int32_t dx,
               int32_t width, int32_t height, uint32_t shift, int16_t offset, uint32_t bitDepth, int16_t *tmpBuf)
{
    //InterpolateEncFast<LUMA, TSrc, TDst>(INTERP_HOR, src, pitchSrc, dst, pitchDst, dx, width, height, shift, offset, bitDepth, tmpBuf);//isFast
}

template <typename TSrc, typename TDst>
void InterpVer(const TSrc *src, int32_t pitchSrc, TDst *dst, int32_t pitchDst, int32_t dy,
               int32_t width, int32_t height, uint32_t shift, int16_t offset, uint32_t bitDepth, int16_t *tmpBuf)
{
    //InterpolateEncFast<LUMA, TSrc, TDst>(INTERP_VER, src, pitchSrc, dst, pitchDst, dy, width, height, shift, offset, bitDepth, tmpBuf);//isFast
}

template <typename PixType>
void MeSubPelBatchedBox(const AV1MEInfo *meInfo,
                        FrameData *curr, FrameData *ref, int32_t pelX, int32_t pelY, AV1MV *mv, int32_t *cost, int32_t *mvCost, int32_t *cost_satd, bool bQPel)
{
    int32_t m_ctbPelX = pelX;
    int32_t m_ctbPelY = pelY;
    int32_t m_pitchSrcLuma = curr->pitch_luma_pix;
    PixType *m_ySrc = (PixType*)curr->y + pelX + pelY * curr->pitch_luma_pix;

    int32_t patternSubPel = SUBPEL_BOX;
    int32_t bitDepthLuma = sizeof(PixType) == 1 ? 8 : 10;
    int32_t bitDepthLumaShift = bitDepthLuma - 8;
    int32_t hadamardMe = 0;

    assert(patternSubPel == SUBPEL_BOX);

    int32_t w = meInfo->width;
    int32_t h = meInfo->height;
    int32_t log2w = BSR(w>>2);
    int32_t log2h = BSR(h>>2);
    int32_t bitDepth = bitDepthLuma;
    int32_t costShift = bitDepthLumaShift;
    int32_t shift = 20 - bitDepth;
    int32_t offset = 1 << (19 - bitDepth);
    int32_t useHadamard = (hadamardMe >= 2);

    int32_t pitchSrc = m_pitchSrcLuma;
    PixType *src = m_ySrc + meInfo->posx + meInfo->posy * pitchSrc;

    int32_t pitchRef = ref->pitch_luma_pix;
    const PixType *refY = (const PixType *)ref->y;
    refY += (int32_t)(m_ctbPelX + meInfo->posx + (mv->mvx >> 2) + (m_ctbPelY + meInfo->posy + (mv->mvy >> 2)) * pitchRef);

    int32_t (*costFunc)(const PixType*, int32_t, const PixType*, int32_t, int32_t, int32_t);
    costFunc = AV1PP::sad_general;

    /*int32_t costs[8];

    int32_t pitchTmp2 = (w + 1 + 15) & ~15;
    int32_t pitchHpel = (w + 1 + 15) & ~15;
    alignas(32) int16_t tmpPels[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 8)];
    alignas(32) PixType subpel[(MAX_CU_SIZE + 16) * (MAX_CU_SIZE + 2)];
    alignas(64) int16_t preAvgTmpBuf[72*64];

    // intermediate halfpels
    InterpHor(refY - 1 - 4 * pitchRef, pitchRef, tmpPels, pitchTmp2, 2, (w + 8) & ~7, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf); // (intermediate)
    int16_t *tmpHor2 = tmpPels + 3 * pitchTmp2;

    // interpolate horizontal halfpels
    //h265_InterpLumaPack(tmpHor2 + pitchTmp2, pitchTmp2, subpel, pitchHpel, w + 1, h, bitDepth);
    costs[3] = costFunc(src, pitchSrc, subpel + 1, pitchHpel, log2w, log2h) >> costShift;
    costs[7] = costFunc(src, pitchSrc, subpel,     pitchHpel, log2w, log2h) >> costShift;

    // interpolate diagonal halfpels
    InterpVer(tmpHor2, pitchTmp2, subpel, pitchHpel, 2, (w + 8) & ~7, h + 2, shift, offset, bitDepth, preAvgTmpBuf);
    costs[0] = costFunc(src, pitchSrc, subpel,                 pitchHpel, log2w, log2h) >> costShift;
    costs[2] = costFunc(src, pitchSrc, subpel + 1,             pitchHpel, log2w, log2h) >> costShift;
    costs[4] = costFunc(src, pitchSrc, subpel + 1 + pitchHpel, pitchHpel, log2w, log2h) >> costShift;
    costs[6] = costFunc(src, pitchSrc, subpel     + pitchHpel, pitchHpel, log2w, log2h) >> costShift;

    // interpolate vertical halfpels
    pitchHpel = (w + 15) & ~15;
    InterpVer(refY - pitchRef, pitchRef, subpel, pitchHpel, 2, w, h + 2, 6, 32, bitDepth, preAvgTmpBuf);
    costs[1] = costFunc(src, pitchSrc, subpel,             pitchHpel, log2w, log2h) >> costShift;
    costs[5] = costFunc(src, pitchSrc, subpel + pitchHpel, pitchHpel, log2w, log2h) >> costShift;
    */
    AV1MV mvBest = *mv;
    int32_t costBest = *cost;
    int32_t mvCostBest = *mvCost;
    /*
    if (hadamardMe == 2) {
        // when satd is for subpel only need to recalculate cost for intpel motion vector
        costBest = AV1PP::satd(src, pitchSrc, refY, pitchRef, log2w, log2h) >> costShift;
        costBest += mvCostBest;
    }

    AddMvCost(1, costs, &mvBest, &costBest, &mvCostBest, patternSubPel);

    if (bQPel) {
        int32_t hpelx = mvBest.mvx - mv->mvx + 4; // can be 2, 4 or 6
        int32_t hpely = mvBest.mvy - mv->mvy + 4; // can be 2, 4 or 6
        int32_t dx = hpelx & 3; // can be 0 or 2
        int32_t dy = hpely & 3; // can be 0 or 2

        int32_t pitchQpel = (w + 15) & ~15;

        // interpolate vertical quater-pels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 5 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, 3 - dy, w, h, 6, 32, bitDepth, preAvgTmpBuf);                             // hpx+0 hpy-1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely - 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+0 hpy-1/4
        costs[1] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        // interpolate vertical qpels
        if (dx == 0) // best halfpel is intpel or ver-halfpel
            InterpVer(refY + (hpely - 3 >> 2) * pitchRef, pitchRef, subpel, pitchQpel, dy + 1, w, h, 6, 32, bitDepth, preAvgTmpBuf);                             // hpx+0 hpy+1/4
        else // best halfpel is hor-halfpel or diag-halfpel
            InterpVer(tmpHor2 + (hpelx >> 2) + (hpely + 1 >> 2) * pitchTmp2, pitchTmp2, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+0 hpy+1/4
        costs[5] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        // intermediate horizontal quater-pels (left of best half-pel)
        int32_t pitchTmp1 = (w + 15) & ~15;
        int16_t *tmpHor1 = tmpPels + 3 * pitchTmp1;
        InterpHor(refY - 4 * pitchRef + (hpelx - 5 >> 2), pitchRef, tmpPels, pitchTmp1, 3 - dx, w, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);  // hpx-1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (left of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
        {}//h265_InterpLumaPack(tmpHor1 + pitchTmp1, pitchTmp1, subpel, pitchHpel, w, h, bitDepth); // hpx-1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor1 + (hpely >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy+0
        costs[7] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        // interpolate 2 diagonal quater-pels (left-top and left-bottom of best half-pel)
        InterpVer(tmpHor1 + (hpely - 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy-1/4
        costs[0] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;
        InterpVer(tmpHor1 + (hpely + 1 >> 2) * pitchTmp1, pitchTmp1, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx-1/4 hpy+1/4
        costs[6] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        // intermediate horizontal quater-pels (right of best half-pel)
        int32_t pitchTmp3 = (w + 15) & ~15;
        int16_t *tmpHor3 = tmpPels + 3 * pitchTmp3;
        InterpHor(refY - 4 * pitchRef + (hpelx - 3 >> 2), pitchRef, tmpPels, pitchTmp3, dx + 1, w, h + 8, bitDepth - 8, 0, bitDepth, preAvgTmpBuf);  // hpx+1/4 hpy+0 (intermediate)

        // interpolate horizontal quater-pels (right of best half-pel)
        if (dy == 0) // best halfpel is intpel or hor-halfpel
        {}//h265_InterpLumaPack(tmpHor3 + pitchTmp3, pitchTmp3, subpel, pitchHpel, w, h, bitDepth); // hpx+1/4 hpy+0
        else // best halfpel is vert-halfpel or diag-halfpel
            InterpVer(tmpHor3 + (hpely >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy+0
        costs[3] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        // interpolate 2 diagonal quater-pels (right-top and right-bottom of best half-pel)
        InterpVer(tmpHor3 + (hpely - 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, 3 - dy, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy-1/4
        costs[2] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;
        InterpVer(tmpHor3 + (hpely + 1 >> 2) * pitchTmp3, pitchTmp3, subpel, pitchQpel, dy + 1, w, h, shift, offset, bitDepth, preAvgTmpBuf); // hpx+1/4 hpy+1/4
        costs[4] = costFunc(src, pitchSrc, subpel, pitchQpel, log2w, log2h) >> costShift;

        AddMvCost(0, costs, &mvBest, &costBest, &mvCostBest, patternSubPel);
    }
    */
    *cost = costBest;
    *mvCost = mvCostBest;
    *mv = mvBest;

    // second metric
    if (cost_satd) {
        int32_t useHadamard = 1;
        *cost_satd = MatchingMetricPu(src, pitchSrc, ref, meInfo, &mvBest, useHadamard, pelX, pelY);
    }
}


template
    void MeIntPelLog<uint8_t>(FrameData *curr, FrameData *ref, int32_t pelX, int32_t pelY, int32_t posx, int32_t posy, AV1MV *mv, int32_t *cost, int32_t *cost_satd, int32_t *mvCost);
template
    void MeIntPelLog_Refine<uint8_t>(FrameData *curr, FrameData *ref, int32_t pelX, int32_t pelY, AV1MV *mv, int32_t *cost, int32_t *cost_satd, int32_t *mvCost, int32_t blkSize);

/*
// mode: 0 - means regular ME{ F(i), F(i+1) }, 1 - means pdist Future ME { P, Pnext }, 2 - means pdist Past ME { P, Pprev }
void DoInterAnalysis(FrameData* curr, Statistics* currStat, FrameData* ref, int32_t* sad, int32_t* satd, AV1MV* out_mv, uint8_t isRefine, uint8_t mode, int32_t lowresFactor, uint8_t bitDepthLuma)
{
    // analysis for blk 8x8.
    int32_t width  = curr->width;
    int32_t height = curr->height;
    const int32_t sizeBlk = SIZE_BLK_LA;
    const int32_t picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const int32_t picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    int32_t bitDepth8 = (bitDepthLuma == 8) ? 1 : 0;

    for (int32_t blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
        for (int32_t blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
            int32_t x = blk_col*sizeBlk;
            int32_t y = blk_row*sizeBlk;
            int32_t fPos = blk_row * picWidthInBlks + blk_col;

            AV1MV mv = {0, 0};
            int32_t cost = INT_MAX;
            int32_t cost_satd = 0;
            int32_t mvCost = 0;//INT_MAX;

            if (isRefine) {
                int32_t widthLowres  = width  >> lowresFactor;
                int32_t heightLowres = height >> lowresFactor;
                const int32_t sizeBlkLowres = SIZE_BLK_LA;
                const int32_t picWidthInBlksLowres  = (widthLowres  + sizeBlkLowres - 1) / sizeBlkLowres;
                const int32_t picHeightInBlksLowres = (heightLowres + sizeBlkLowres - 1) / sizeBlkLowres;
                int32_t fPosLowres = (blk_row >> lowresFactor) * picWidthInBlksLowres + (blk_col >> lowresFactor);

                if (mode == 0) {
                    mv   = currStat->m_mv[fPosLowres];
                    cost = currStat->m_interSad[fPosLowres];
                } else if (mode == 1) {
                    mv   = currStat->m_mv_pdist_future[fPosLowres];
                    cost = currStat->m_interSad_pdist_future[fPosLowres];
                } else {
                    mv   = currStat->m_mv_pdist_past[fPosLowres];
                    cost = currStat->m_interSad_pdist_past[fPosLowres];
                }

                mv.mvx <<= lowresFactor;
                mv.mvy <<= lowresFactor;

                // recalculate to nearest int-pel
                mv.mvx = (mv.mvx + 1) & ~3;
                mv.mvy = (mv.mvy + 1) & ~3;
                {
                    // limits to clip MV allover the CU
                    int32_t pelX = x;
                    int32_t pelY = y;
                    const int32_t MvShift = 2;
                    const int32_t offset = 8;
                    const int32_t blkSize = SIZE_BLK_LA;
                    int32_t HorMax = (width + offset - pelX - 1) << MvShift;
                    int32_t HorMin = ( - (int32_t)blkSize - offset - (int32_t) pelX + 1) << MvShift;
                    int32_t VerMax = (curr->height + offset - pelY - 1) << MvShift;
                    int32_t VerMin = ( - (int32_t) blkSize - offset - (int32_t) pelY + 1) << MvShift;

                    ClipMV(mv, HorMin, HorMax, VerMin, VerMax);
                }


                MeIntPelLog_Refine<uint8_t>(curr, ref, x, y, &mv, &cost, &cost_satd, &mvCost, SIZE_BLK_LA);

            } else {

                if (bitDepth8)
                    MeIntPelLog<uint8_t>(curr, ref, x, y, 0, 0, &mv, &cost, &cost_satd, &mvCost);
                else
                    MeIntPelLog<uint16_t>(curr, ref, x, y, 0, 0, &mv, &cost, &cost_satd, &mvCost);
            }

            {
                AV1MV mv_sad = mv;
                AV1MEInfo meInfo;
                meInfo.posx = 0;
                meInfo.posy = 0;
                meInfo.width  = SIZE_BLK_LA;
                meInfo.height = SIZE_BLK_LA;

                if (bitDepth8)
                    MeSubPelBatchedBox<uint8_t>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, satd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);
                else
                    MeSubPelBatchedBox<uint16_t>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, satd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);

                sad[fPos] = cost;
                out_mv[fPos] = mv_sad;
            }
        }
    }
}
*/

void DoInterAnalysis_OneRow(FrameData* curr, Statistics* currStat, FrameData* ref, int32_t* sad, int32_t* satd, AV1MV* out_mv, uint8_t isRefine, uint8_t mode, int32_t lowresFactor, uint8_t bitDepthLuma, int32_t row, FrameData* frame=NULL, FrameData* framePrev=NULL)
{
    // analysis for blk 8x8.
    int32_t width  = curr->width;
    int32_t height = curr->height;
    const int32_t sizeBlk = SIZE_BLK_LA;
    const int32_t picWidthInBlks  = (width  + sizeBlk - 1) / sizeBlk;
    const int32_t picHeightInBlks = (height + sizeBlk - 1) / sizeBlk;

    int32_t bitDepth8 = (bitDepthLuma == 8) ? 1 : 0;
    const IppiSize frmSize = {width, height};
    //for (int32_t blk_row = 0; blk_row < picHeightInBlks; blk_row++) {
    int32_t blk_row = row;
    for (int32_t blk_col = 0; blk_col < picWidthInBlks; blk_col++) {
        int32_t x = blk_col*sizeBlk;
        int32_t y = blk_row*sizeBlk;
        int32_t fPos = blk_row * picWidthInBlks + blk_col;

        AV1MV mv = {0, 0};
        int32_t cost = INT_MAX;
        int32_t cost_satd = 0;
        int32_t mvCost = 0;//INT_MAX;

        if (isRefine) {
            int32_t widthLowres  = width  >> lowresFactor;
            int32_t heightLowres = height >> lowresFactor;
            const int32_t sizeBlkLowres = SIZE_BLK_LA;
            const int32_t picWidthInBlksLowres  = (widthLowres  + sizeBlkLowres - 1) / sizeBlkLowres;
            const int32_t picHeightInBlksLowres = (heightLowres + sizeBlkLowres - 1) / sizeBlkLowres;
            int32_t fPosLowres = (blk_row >> lowresFactor) * picWidthInBlksLowres + (blk_col >> lowresFactor);

            if (mode == 0) {
                mv   = currStat->m_mv[fPosLowres];
                cost = currStat->m_interSad[fPosLowres];
            } else if (mode == 1) {
                mv   = currStat->m_mv_pdist_future[fPosLowres];
                cost = currStat->m_interSad_pdist_future[fPosLowres];
            } else {
                mv   = currStat->m_mv_pdist_past[fPosLowres];
                cost = currStat->m_interSad_pdist_past[fPosLowres];
            }

            mv.mvx <<= lowresFactor;
            mv.mvy <<= lowresFactor;
            // recalculate to nearest int-pel
            mv.mvx = (mv.mvx + 1) & ~3;
            mv.mvy = (mv.mvy + 1) & ~3;
            //---------
            {
                // limits to clip MV allover the CU
                int32_t pelX = x;
                int32_t pelY = y;
                const int32_t MvShift = 2;
                const int32_t offset = 8;
                const int32_t blkSize = SIZE_BLK_LA;
                int32_t HorMax = (width + offset - pelX - 1) << MvShift;
                int32_t HorMin = ( - (int32_t)blkSize - offset - (int32_t) pelX + 1) << MvShift;
                int32_t VerMax = (curr->height + offset - pelY - 1) << MvShift;
                int32_t VerMin = ( - (int32_t) blkSize - offset - (int32_t) pelY + 1) << MvShift;

                ClipMV(mv, HorMin, HorMax, VerMin, VerMax);
            }
            //---------
            if (bitDepth8) {
                MeIntPelLog_Refine<uint8_t>(curr, ref, x, y, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost, sizeBlk);
                /*uint8_t *src = curr->y + x + y * curr->pitch_luma_pix;
                int32_t refOffset = x + (mv.mvx >> 2) + (y + (mv.mvy >> 2)) * ref->pitch_luma_pix;
                const uint8_t *rec = ref->y + refOffset;
                int32_t pitchRec = ref->pitch_luma_pix;
                cost = AV1PP::sad_general(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA);
                mvCost = 0;*/
            }
            else
            {
                MeIntPelLog_Refine<uint16_t>(curr, ref, x, y, &mv, &cost, satd ? (&cost_satd) : NULL, &mvCost, sizeBlk);
                /*uint16_t *src = (uint16_t *)curr->y + x + y * curr->pitch_luma_pix;
                int32_t refOffset = x + (mv.mvx >> 2) + (y + (mv.mvy >> 2)) * ref->pitch_luma_pix;
                const uint16_t *rec = (uint16_t *)ref->y + refOffset;
                int32_t pitchRec = ref->pitch_luma_pix;
                cost = AV1PP::sad_general(src, curr->pitch_luma_pix, rec, ref->pitch_luma_pix, LOG2_SIZE_BLK_LA, LOG2_SIZE_BLK_LA);
                mvCost = 0;*/
            }

        } else {

            if (bitDepth8)
                MeIntPelLog<uint8_t>(curr, ref, x, y, 0, 0, &mv, &cost, /*satd ? (&cost_satd) :*/ NULL, &mvCost);
            else
                MeIntPelLog<uint16_t>(curr, ref, x, y, 0, 0, &mv, &cost, /*satd ? (&cost_satd) :*/ NULL, &mvCost);

            sad[fPos] = cost;
            out_mv[fPos] = mv;

        }

        {
            AV1MV mv_sad = mv;
            AV1MEInfo meInfo;
            meInfo.posx = 0;
            meInfo.posy = 0;
            meInfo.width  = SIZE_BLK_LA;
            meInfo.height = SIZE_BLK_LA;
            int32_t subSatd = (satd) ? 1: 0;
            if(lowresFactor && !isRefine && frame && framePrev) subSatd = 0;

            if (bitDepth8)
                MeSubPelBatchedBox<uint8_t>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */subSatd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);
            else
                MeSubPelBatchedBox<uint16_t>(&meInfo, curr, ref, x, y, &mv_sad, &cost, &mvCost, /*m_par, */subSatd ? (satd + fPos) : NULL, (lowresFactor && !isRefine) ? true: false);

            sad[fPos] = cost;
            out_mv[fPos] = mv_sad;

            if (lowresFactor && !isRefine && frame && framePrev) {
                // LowresFactor FullresMetrics for GopCqBrc
                int32_t widthFullres  = width  << lowresFactor;
                int32_t heightFullres = height << lowresFactor;
                const int32_t sizeBlkFullres = SIZE_BLK_LA << lowresFactor;
                int32_t log2wFullRes = BSR(sizeBlkFullres >> 2);
                int32_t xFullres = blk_col * sizeBlkFullres;
                int32_t yFullres = blk_row * sizeBlkFullres;
                const IppiSize frmSizeFullres = {widthFullres, heightFullres};
                AV1MV mvFullres = {0, 0};
                mvFullres.mvx = mv_sad.mvx << lowresFactor;
                mvFullres.mvy = mv_sad.mvy << lowresFactor;
                if (bitDepth8) {
                    uint8_t *src = frame->y + xFullres + yFullres * frame->pitch_luma_pix;
                    int32_t refOffset = xFullres + (mvFullres.mvx >> 2) + (yFullres + (mvFullres.mvy >> 2)) * framePrev->pitch_luma_pix;
                    const uint8_t *rec = framePrev->y + refOffset;
                    cost = AV1PP::sad_general(src, frame->pitch_luma_pix, rec, ref->pitch_luma_pix, log2wFullRes, log2wFullRes);
                    MeIntPelLog_Refine<uint8_t>(frame, framePrev, xFullres, yFullres, &mvFullres, &cost, satd ? (satd + fPos) : NULL, &mvCost, sizeBlkFullres);
                } else {
                    uint16_t *src = (uint16_t *)frame->y + xFullres + yFullres * frame->pitch_luma_pix;
                    int32_t refOffset = xFullres + (mvFullres.mvx >> 2) + (yFullres + (mvFullres.mvy >> 2)) * framePrev->pitch_luma_pix;
                    const uint16_t *rec = (uint16_t *)framePrev->y + refOffset;
                    cost = AV1PP::sad_general(src, frame->pitch_luma_pix, rec, ref->pitch_luma_pix, log2wFullRes, log2wFullRes);
                    MeIntPelLog_Refine<uint16_t>(frame, framePrev, xFullres, yFullres, &mvFullres, &cost, satd ? (satd + fPos) : NULL, &mvCost, sizeBlkFullres);
                }

                mv_sad.mvx = mvFullres.mvx >> lowresFactor;
                mv_sad.mvy = mvFullres.mvy >> lowresFactor;
                out_mv[fPos] = mv_sad;
                sad[fPos] = cost;
            }
        }
    }
    //}
}


template <class PixType>
PixType AvgPixBlk(PixType* in, int32_t pitch, int32_t posx, int32_t posy, int32_t step)
{
    int32_t posx_orig = posx * step;
    int32_t posy_orig = posy * step;

    PixType* src = in + posy_orig*pitch + posx_orig;
    int64_t sum = 0;

    for (int32_t y = 0; y < step; y++) {
        for (int32_t x = 0; x < step; x++) {
            sum += src[ y*pitch + x ];
        }
    }

    return (PixType) ((sum + (step * step / 2)) / (step * step));
}

template <class PixType>
PixType NearestPixBlk(PixType* in, int32_t pitch, int32_t posx, int32_t posy, int32_t step)
{
    int32_t posx_orig = posx * step;
    int32_t posy_orig = posy * step;

    PixType* src = in + posy_orig*pitch + posx_orig;
    //return src[0];

    // the same as IppNearest
    int32_t pos_x = (step >> 1) - 1;
    int32_t pos_y = (step >> 1) - 1;
    return src[pos_y*pitch + pos_x];
}

template <class PixType>
int64_t Scale(FrameData* in, FrameData* out)
{
    const PixType *pixIn = (const PixType *)in->y;
    PixType *pixOut = (PixType *)out->y;
    int32_t widthOrig  = in->width;
    int32_t heightOrig = in->height;

    int32_t widthOut  = out->width;
    int32_t heightOut = out->height;

    int32_t pitchIn = in->pitch_luma_pix;
    int32_t pitchOut = out->pitch_luma_pix;
    int32_t step = widthOrig / widthOut;

    for ( int32_t y = 0; y < heightOut; y++ ) {
        for (int32_t x = 0; x < widthOut; x++) {
            //pixOut[ y*pitchOut + x] = AvgPixBlk( pixIn, pitchIn, x, y, step);
            pixOut[ y*pitchOut + x] = NearestPixBlk( pixIn, pitchIn, x, y, step);
        }
    }
    return 0;
}


template <class PixType, typename Frame>
int64_t CalcPixDiff(Frame *prev, Frame *curr, int32_t planeIdx)
{
    const PixType *pixPrev = (const PixType *)prev->y;
    const PixType *pixCurr = (const PixType *)curr->y;
    int32_t width = prev->width;
    int32_t height = prev->height;
    int32_t pitchPrev = prev->pitch_luma_pix;
    int32_t pitchCurr = curr->pitch_luma_pix;

    if (planeIdx > 0) {
        pixPrev = (const PixType *)prev->uv;
        pixCurr = (const PixType *)curr->uv;
        pitchPrev = prev->pitch_chroma_pix;
        pitchCurr = curr->pitch_chroma_pix;
        height /= 2; // TODO: YUV420
    }

    int64_t diff = 0;
    for (int32_t y = 0; y < height; y++, pixPrev += pitchPrev, pixCurr += pitchCurr) {
        for (int32_t x = 0; x < width; x++) {
            diff += abs(pixPrev[x] - pixCurr[x]);
        }
    }

    return diff;
}

template <class PixType>
void BuildHistLuma(const PixType *pix, int32_t pitch, int32_t width, int32_t height, int32_t bitDepth, int64_t *hist)
{
    const int32_t binMask = (1 << bitDepth) - 1;
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (int32_t y = 0; y < height; y++, pix += pitch)
        for (int32_t x = 0; x < width; x++)
            hist[pix[x] & binMask]++;
}

template <class PixType>
void BuildHistChroma(const PixType *pix, int32_t pitch, int32_t width, int32_t height, int32_t bitDepth, int64_t *hist)
{
    const int32_t binMask = (1 << bitDepth) - 1;
    memset(hist, 0, (1 << bitDepth) * sizeof(hist[0]));
    for (int32_t y = 0; y < height; y++, pix += pitch)
        for (int32_t x = 0; x < width * 2; x += 2)
            hist[pix[x] & binMask]++;
}

template <class PixType, class Frame>
int64_t CalcHistDiff(Frame *prev, Frame *curr, int32_t planeIdx)
{
    const int32_t maxNumBins = 1 << (sizeof(PixType) == 1 ? 8 : 10);
    int64_t histPrev[maxNumBins];
    int64_t histCurr[maxNumBins];
    int32_t bitDepth = sizeof(PixType) == 1 ? 8 : 10;

    if (planeIdx == 0) {
        const PixType *pixPrev = (const PixType *)prev->y;
        const PixType *pixCurr = (const PixType *)curr->y;
        int32_t width = prev->width;
        int32_t height = prev->height;
        int32_t pitchPrev = prev->pitch_luma_pix;
        int32_t pitchCurr = curr->pitch_luma_pix;

        BuildHistLuma<PixType>(pixPrev, pitchPrev, width, height, bitDepth, histPrev);
        BuildHistLuma<PixType>(pixCurr, pitchCurr, width, height, bitDepth, histCurr);
    }
    //else {
    //    const PixType *pixPrev = (const PixType *)prev->uv + (planeIdx - 1);
    //    const PixType *pixCurr = (const PixType *)curr->uv + (planeIdx - 1);
    //    int32_t width = prev->width / 2; // TODO: YUV422, YUV444
    //    int32_t height = prev->height / 2; // TODO: YUV422, YUV444
    //    int32_t pitchPrev = prev->pitch_chroma_pix;
    //    int32_t pitchCurr = curr->pitch_chroma_pix;
    //    bitDepth = prev->m_bitDepthChroma;

    //    BuildHistChroma<PixType>(pixPrev, pitchPrev, width, height, bitDepth, histPrev);
    //    BuildHistChroma<PixType>(pixCurr, pitchCurr, width, height, bitDepth, histCurr);
    //}

    int32_t numBins = 1 << bitDepth;
    int64_t diff = 0;
    for (int32_t i = 0; i < numBins; i++)
        diff += abs(histPrev[i] - histCurr[i]);

    return diff;
}


enum {
    BLOCK_SIZE = 4
};

// CalcRsCs_OneRow()  works with blk_8x8. (SIZE_BLK_LA)
template <class PixType>
void CalcRsCs_OneRow(FrameData* data, Statistics* stat, AV1VideoParam& par, int32_t row)
{
    int32_t locx, locy;

    const int32_t RsCsSIZE = BLOCK_SIZE*BLOCK_SIZE;
    int32_t hblocks = (int32_t)data->height / BLOCK_SIZE;
    int32_t wblocks = (int32_t)data->width / BLOCK_SIZE;

    stat->m_frameCs = 0.0;
    stat->m_frameRs = 0.0;

    int32_t pitch = data->pitch_luma_pix;
    int32_t pitchRsCs4 = stat->m_pitchRsCs4;

    int32_t iStart = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * row;
    int32_t iEnd  = (/*par.MaxCUSize*/ SIZE_BLK_LA / BLOCK_SIZE) * (row + 1);
    iEnd = std::min(iEnd, hblocks);

    for (int32_t j = 0; j < wblocks; j += 16) {
        PixType *ySrc = (PixType *)data->y + (iStart*BLOCK_SIZE)*pitch + BLOCK_SIZE*j;
        int32_t *rs = stat->m_rs[0] + iStart * pitchRsCs4 + j;
        int32_t *cs = stat->m_cs[0] + iStart * pitchRsCs4 + j;
        AV1PP::compute_rscs(ySrc, pitch, rs, cs, pitchRsCs4, BLOCK_SIZE*16, BLOCK_SIZE*(iEnd-iStart));
    }
}


// 8x8 blk
int32_t GetSpatioTemporalComplexity_own(int32_t sad, float scpp)
{
    const int32_t width = SIZE_BLK_LA;
    const int32_t height = SIZE_BLK_LA;
    float sadpp=(float)sad/(float)(width*height);
    sadpp=(sadpp*sadpp);
    if     (sadpp < 0.09*scpp )  return 0; // Very Low
    else if(sadpp < 0.36*scpp )  return 1; // Low
    else if(sadpp < 1.44*scpp )  return 2; // Medium
    else if(sadpp < 3.24*scpp )  return 3; // High
    else                         return 4; // VeryHigh
}

#define MAXPDIST 5
#define REFMOD_MAX      15


static void AddCandidate(AV1MV *pMV, int *uMVnum, AV1MV *MVCandidate)
{
    int i;
    MVCandidate[*uMVnum] = *pMV;
    for (i = 0; i < *uMVnum; i ++)
    {
        if (MVCandidate[*uMVnum].mvx == MVCandidate[i].mvx &&
            MVCandidate[*uMVnum].mvy == MVCandidate[i].mvy)
            break;
    }
    /* Test if Last Predictor vector is valid */
    if (i == *uMVnum)
    {
        (*uMVnum)++;
    }
}

struct isEqual
{
    isEqual(int32_t frameOrder): m_frameOrder(frameOrder)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_frameOrder == m_frameOrder;
    }

    int32_t m_frameOrder;
};


struct isEqualFrameType
{
    isEqualFrameType(uint32_t frameType): m_frameType(frameType)
    {}

    template<typename T>
    bool operator()(const T& l)
    {
        return (*l).m_origin->m_picCodeType == m_frameType;
    }

    uint32_t m_frameType;
};

struct isEqualRefFrame
{
    isEqualRefFrame(){}

    template<typename T>
    bool operator()(const T& l)
    {
        return ((*l).m_picCodeType == MFX_FRAMETYPE_I) || ((*l).m_picCodeType == MFX_FRAMETYPE_P);
    }
};

void AV1Enc::DetermineQpMap_IFrame(FrameIter curr, FrameIter end, AV1VideoParam& videoParam)
{
    static const float lmt_sc2[10] = { // lower limit of SFM(Rs,Cs) range for spatial classification
        4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, static_cast<float>(INT_MAX) };
    const int32_t futr_qp = MAX_DQP;
    assert(videoParam.Log2MaxCUSize == 6);
    Frame* inFrame = *curr;
    AV1MV candMVs[(64 / 8)*(64 / 8)];
    int32_t LowresFactor = 0;
#ifdef LOW_COMPLX_PAQ
    LowresFactor = videoParam.LowresFactor;
#endif
    int32_t lowRes = LowresFactor ? 1 : 0;
    int32_t log2SubBlk = 3 + LowresFactor;
    int32_t widthInSubBlks = videoParam.Width >> 3;
    if (lowRes) {
        widthInSubBlks = (inFrame->m_lowres->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
    }
    uint8_t bitDepthLuma = inFrame->m_bitDepthLuma;

    int32_t heightInCtbs = (int32_t)videoParam.PicHeightInCtbs;
    int32_t widthInCtbs = (int32_t)videoParam.PicWidthInCtbs;

    bool futr_key = false;
    int32_t REF_DIST = 8;

    FrameIter it1 = ++FrameIter(curr);
    Frame* futr_frame = inFrame;
    for (FrameIter it = it1; it != end; it++) {
        if (*it) {
            futr_frame = *it;
            if (futr_frame->m_picCodeType == MFX_FRAMETYPE_I) {
                futr_key = true;
            }
        }
    }
    REF_DIST = std::min(8, (inFrame->m_frameOrder - futr_frame->m_frameOrder));

    for (int32_t row = 0; row<heightInCtbs; row++) {
        for (int32_t col = 0; col<widthInCtbs; col++) {
            int32_t pelX = col * videoParam.MaxCUSize;
            int32_t pelY = row * videoParam.MaxCUSize;
            //int32_t MaxDepth = 0; // std::min((int32_t)videoParam.Log2MaxCUSize - log2SubBlk, videoParam.MaxCuDQPDepth);

            //for (int32_t depth = 0; depth <= MaxDepth; depth++)
            { // 64x64 -> 8x8
                int32_t log2BlkSize = videoParam.Log2MaxCUSize;// -depth;
                int32_t pitchRsCs = inFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize - 2);
                int32_t blkSize = 1 << log2BlkSize;
                int32_t width = std::min(64, videoParam.Width - pelX);
                int32_t height = std::min(64, videoParam.Height - pelY);
                for (int32_t y = 0; y < height; y += blkSize) {
                    for (int32_t x = 0; x < width; x += blkSize) {
                        int32_t idx = ((pelY + y) >> log2BlkSize)*pitchRsCs + ((pelX + x) >> log2BlkSize);
                        float Rs2 = inFrame->m_stats[0]->m_rs[log2BlkSize - 2][idx];
                        float Cs2 = inFrame->m_stats[0]->m_cs[log2BlkSize - 2][idx];
                        int32_t blkW4 = std::min(blkSize, width - x) >> 2;
                        int32_t blkH4 = std::min(blkSize, height - y) >> 2;
                        int32_t numSubBlkw = blkW4 >> (log2SubBlk - 2);
                        int32_t numSubBlkh = blkH4 >> (log2SubBlk - 2);
#ifdef LOW_COMPLX_PAQ
                        if (numSubBlkh*(1 << log2SubBlk) < height || numSubBlkw*(1 << log2SubBlk) < width) {
                            int32_t widthInTiles = widthInCtbs; // << depth;
                            int32_t off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                            //inFrame->m_stats[0]->coloc_futr[depth][off] = 0;
                            //inFrame->m_stats[0]->qp_mask[depth][off] = 0;
                            inFrame->m_stats[0]->coloc_futr[off] = 0;
                            inFrame->m_stats[0]->qp_mask[off] = 0;
                            continue;
                        }
#endif
                        Rs2 /= (blkW4*blkH4);
                        Cs2 /= (blkW4*blkH4);
                        float SC = sqrt(Rs2 + Cs2);
                        //float tsc_RTML = 0.6f*sqrt(SC / (1 << (bitDepthLuma - 8)))*(1 << (bitDepthLuma - 8));
//#ifdef LOW_COMPLX_PAQ
                        //if (lowRes) tsc_RTML *= 1.1f;
//#endif
                        //float tsc_RTMG = std::min(2 * tsc_RTML, SC / 1.414f);
                        //float tsc_RTS = std::min(3 * tsc_RTML, SC / 1.414f);
                        int scVal = 0;
                        for (int32_t i = 0; i < 10; i++) {
                            if (SC < lmt_sc2[i] * (float)(1 << (bitDepthLuma - 8))) {
                                scVal = i;
                                break;
                            }
                        }

                        // Modification for integer ME
                        SC /= 1.414f;
                        float tsc_RTML = 0.6f*SC;
                        float tsc_RTMG = SC;
                        float tsc_RTS = 1.4 * SC;

                        FrameIter itStart = ++FrameIter(curr);
                        int coloc = 0;
                        float pdsad_futr = 0.0f;
                        for (FrameIter it = itStart; it != end; it++) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            int uMVnum = 0;
                            float psad_futr = 0.0f;
                            for (int32_t i = 0; i<numSubBlkh; i++) {
                                for (int32_t j = 0; j<numSubBlkw; j++) {
                                    int32_t off = (((pelY + y) >> (log2SubBlk)) + i) * widthInSubBlks + ((pelX + x) >> (log2SubBlk)) + j;
                                    psad_futr += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_futr /= (blkW4*blkH4) << 4;

                            if (psad_futr < tsc_RTML && uMVnum < 1 + std::max(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc++;
                            else
                                break;
                        }
                        pdsad_futr = 0.0f;
#ifdef LOW_COMPLX_PAQ
                        if (itStart != end) {
                            float sadFrm[17] = { 0 };
                            float sadSum = 0;
                            float sadMax = 0;
                            int32_t sadCnt = 0;
                            for (FrameIter it = itStart; it != end; it++) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                float psad_futr = 0.0f;
                                for (int32_t i = 0; i < numSubBlkh; i++) {
                                    for (int32_t j = 0; j < numSubBlkw; j++) {
                                        int32_t off = (((pelY + y) >> (log2SubBlk)) + i) * widthInSubBlks + ((pelX + x) >> (log2SubBlk)) + j;
                                        psad_futr += stat->m_interSad[off];
                                    }
                                }
                                psad_futr /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_futr;
                                sadCnt++;
                                sadSum += psad_futr;
                            }
                            float sadAvg = sadSum / sadCnt;
                            int32_t numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_futr = sadMax;
                            }
                            else {
                                pdsad_futr = sadAvg * 2;
                            }
                        }
#else
                        for (int32_t i = 0; i<numSubBlkh; i++) {
                            for (int32_t j = 0; j<numSubBlkw; j++) {
                                int32_t off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                pdsad_futr += inFrame->m_stats[lowRes]->m_interSad_pdist_future[off];
                            }
                        }
                        pdsad_futr /= (blkW4*blkH4) << 4;
#endif

                        if (itStart == end) {
                            pdsad_futr = tsc_RTS;
                            coloc = 0;
                        }

                        int32_t widthInTiles = videoParam.PicWidthInCtbs; // << depth;
                        int32_t off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                        //inFrame->m_stats[0]->coloc_futr[depth][off] = coloc;
                        inFrame->m_stats[0]->coloc_futr[off] = coloc;

                        //int32_t &qp_mask = inFrame->m_stats[0]->qp_mask[depth][off];
                        int32_t &qp_mask = inFrame->m_stats[0]->qp_mask[off];
                        if (scVal<1 && pdsad_futr<tsc_RTML) {
                            // Visual Quality (Flat area first, because coloc count doesn't work in flat areas)
                            coloc = 1;
                            qp_mask = -1 * coloc;
                        }
                        else {
                            if (futr_key) {
                                coloc = std::min(REF_DIST / 2, coloc);
                            }
                            if (coloc >= 8 && pdsad_futr<tsc_RTML) {
                                // Stable Motion, Propagation & Motion reuse (long term stable hypothesis, 8+=>inf)
                                qp_mask = -1 * std::min(futr_qp, (int32_t)(((float)coloc / 8.f)*futr_qp));
                            }
                            else if (coloc >= 8 && pdsad_futr<tsc_RTMG) {
                                // Stable Motion, Propagation possible & Motion reuse
                                qp_mask = -1 * std::min(futr_qp, (int32_t)(((float)coloc / 8.f)*4.f));
                            }
                            else if (coloc>1 && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion reuse
                                qp_mask = -1 * std::min(4, (int32_t)(((float)coloc / 8.f)*4.f));
                            }
                            else if (scVal >= 6 && pdsad_futr>tsc_RTS) {
                                // Reduce disproportional cost on high texture and bad motion
                                qp_mask = 1;
                            }
                            else {
                                // Default
                                qp_mask = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}
/*
void AV1Enc::DetermineQpMap_IFrame(FrameIter curr, FrameIter end, AV1VideoParam& videoParam)
{
    Frame* inFrame    = *curr;

    int wBlock = inFrame->m_origin->width/8;
    int w4 = inFrame->m_origin->width/4;

    const int CU_SIZE = videoParam.MaxCUSize;
    int heightInTiles = videoParam.PicHeightInCtbs;
    int widthInTiles  = videoParam.PicWidthInCtbs;

    const int MAX_TILE_SIZE = CU_SIZE;//64;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;

    float tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    static const float lmt_sc2[10] = {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, (float)INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    AV1MV candMVs[(64/8)*(64/8)];

    float futr_qp = MAX_DQP;

    int32_t c8_height = (inFrame->m_origin->height);
    int32_t c8_width  = (inFrame->m_origin->width);

    const int32_t statIdx = 0;// we use original stats here

    for (int32_t row=0;row<heightInTiles;row++) {
        for (int32_t col=0;col<widthInTiles;col++) {
            int i,j;

            N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;

            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;

            float Rs=0.0, Cs=0.0;
            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += inFrame->m_stats[statIdx]->m_rs[0][(row*r4h+i)*w4+(col*r4w+j)];
                    Cs += inFrame->m_stats[statIdx]->m_cs[0][(row*r4h+i)*w4+(col*r4w+j)];
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);

            float SC = sqrt(Rs + Cs);

            int scVal = 0;
            inFrame->m_stats[statIdx]->rscs_ctb[row*widthInTiles+col] = SC;
            for (i = 0; i < 10; i++) {
                if (SC < lmt_sc2[i]*(float)(1<<(inFrame->m_bitDepthLuma-8))) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->m_stats[statIdx]->sc_mask[row*widthInTiles+col] = scVal;

            SC /= 1.414f;
            // Modified (Pre analysis now Integer ME only)
            float tsc_RTML = 0.6f*SC;
            float tsc_RTMG = SC; // std::min(2 * tsc_RTML, 2 * SC);
            float tsc_RTS = 1.4 * SC; // std::min(3 * tsc_RTML, 3 * SC);
            FrameIter itStart = ++FrameIter(curr);
            int coloc=0;
            float pdsad_futr=0.0;
            for (FrameIter it = itStart; it != end; it++) {
                Statistics* stat = (*it)->m_stats[statIdx];

                int uMVnum = 0;
                float psad_futr = 0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_futr += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_futr/=tdiv;

                if (psad_futr < tsc_RTML && uMVnum<(mbT*nbT)/2)
                    coloc++;
                else
                    break;
            }

            pdsad_futr=0.0;
            for(i=0;i<mbT;i++)
                for(j=0;j<nbT;j++)
                    pdsad_futr += inFrame->m_stats[statIdx]->m_interSad_pdist_future[(row*rbh+i)*(wBlock)+(col*rbw+j)];
            pdsad_futr/=tdiv;

            if (itStart == end) {
                pdsad_futr = tsc_RTS;
                coloc = 0;
            }

            inFrame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] = coloc;

            if(scVal<1 && pdsad_futr<tsc_RTML) {
                // Visual Quality (Flat area first, because coloc count doesn't work in flat areas)
                coloc = 1;
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*coloc;
            } else if(coloc>=8 && pdsad_futr<tsc_RTML) {
                // Stable Motion, Propagation & Motion reuse (long term stable hypothesis, 8+=>inf)
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min((int32_t)futr_qp, (int32_t)(((float)coloc/8.f)*futr_qp));
            } else if(coloc>=8 && pdsad_futr<tsc_RTMG) {
                // Stable Motion, Propagation possible & Motion reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min((int32_t)futr_qp, (int32_t)(((float)coloc/8.f)*4.f));
            } else if(coloc>1 && pdsad_futr<tsc_RTMG) {
                // Stable Motion & Motion reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min(4, (int32_t)(((float)coloc/8.f)*4.f));
            } else if(scVal>=6 && pdsad_futr>tsc_RTS) {
                // Reduce disproportional cost on high texture and bad motion
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 1;
            } else {
                // Default
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0;
            }
        }
    }
}*/

void AV1Enc::DetermineQpMap_PFrame(FrameIter begin, FrameIter curr, FrameIter end, AV1VideoParam & videoParam)
{
    static const float lmt_sc2[10] = { // lower limit of SFM(Rs,Cs) range for spatial classification
        4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, static_cast<float>(INT_MAX) };
    FrameIter itCurr = curr;
    Frame* inFrame = *curr;
    Frame* past_frame = *begin;
    const float futr_qp = MAX_DQP;
    const int32_t REF_DIST = std::min(8, (inFrame->m_frameOrder - past_frame->m_frameOrder));
    AV1MV candMVs[(64 / 8)*(64 / 8)];
    int32_t LowresFactor = 0;
#ifdef LOW_COMPLX_PAQ
    LowresFactor = videoParam.LowresFactor;
#endif
    int32_t lowRes = LowresFactor ? 1 : 0;
    int32_t log2SubBlk = 3 + LowresFactor;
    int32_t widthInSubBlks = videoParam.Width >> 3;
    if (lowRes) {
        widthInSubBlks = (inFrame->m_lowres->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
    }
    uint8_t bitDepthLuma = inFrame->m_bitDepthLuma;
    int32_t heightInCtbs = (int32_t)videoParam.PicHeightInCtbs;
    int32_t widthInCtbs = (int32_t)videoParam.PicWidthInCtbs;

    bool futr_key = false;
    FrameIter it1 = ++FrameIter(curr);
    for (FrameIter it = it1; it != end; it++) {
        if (*it && (*it)->m_picCodeType == MFX_FRAMETYPE_I) {
            futr_key = true;
            break;
        }
    }

    for (int32_t row = 0; row<heightInCtbs; row++) {
        for (int32_t col = 0; col<widthInCtbs; col++) {
            int32_t pelX = col * videoParam.MaxCUSize;
            int32_t pelY = row * videoParam.MaxCUSize;
            //int32_t MaxDepth = std::min((int32_t)videoParam.Log2MaxCUSize - log2SubBlk, videoParam.MaxCuDQPDepth);
            //for (int32_t depth = 0; depth <= MaxDepth; depth++)
            { // 64x64
                int32_t log2BlkSize = videoParam.Log2MaxCUSize; // -depth;
                int32_t pitchRsCs = inFrame->m_stats[0]->m_pitchRsCs4 >> (log2BlkSize - 2);
                int32_t blkSize = 1 << log2BlkSize;
                int32_t width = std::min(64, videoParam.Width - pelX);
                int32_t height = std::min(64, videoParam.Height - pelY);
                for (int32_t y = 0; y < height; y += blkSize) {
                    for (int32_t x = 0; x < width; x += blkSize) {
                        int32_t idx = ((pelY + y) >> log2BlkSize)*pitchRsCs + ((pelX + x) >> log2BlkSize);
                        float Rs2 = inFrame->m_stats[0]->m_rs[log2BlkSize - 2][idx];
                        float Cs2 = inFrame->m_stats[0]->m_cs[log2BlkSize - 2][idx];
                        int32_t blkW4 = std::min(blkSize, width - x) >> 2;
                        int32_t blkH4 = std::min(blkSize, height - y) >> 2;

                        int32_t numSubBlkw = blkW4 >> (log2SubBlk - 2);
                        int32_t numSubBlkh = blkH4 >> (log2SubBlk - 2);
#ifdef LOW_COMPLX_PAQ
                        if (numSubBlkh*(1 << log2SubBlk) < height || numSubBlkw*(1 << log2SubBlk) < width) {
                            int32_t widthInTiles = widthInCtbs; // << depth;
                            int32_t off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                            //inFrame->m_stats[0]->coloc_futr[depth][off] = 0;
                            //inFrame->m_stats[0]->qp_mask[depth][off] = 0;
                            inFrame->m_stats[0]->coloc_futr[off] = 0;
                            inFrame->m_stats[0]->qp_mask[off] = 0;
                            continue;
                        }
#endif

                        Rs2 /= (blkW4*blkH4);
                        Cs2 /= (blkW4*blkH4);
                        float SC = sqrt(Rs2 + Cs2);
                        //float tsc_RTML = 0.6f*sqrt(SC / (1 << (bitDepthLuma - 8)))*(1 << (bitDepthLuma - 8));
//#ifdef LOW_COMPLX_PAQ
                        //if (lowRes) tsc_RTML *= 1.1f;
//#endif
                        //float tsc_RTMG = std::min(2 * tsc_RTML, SC / 1.414f);
                        //float tsc_RTS = std::min(3 * tsc_RTML, SC / 1.414f);
                        int scVal = 0;
                        for (int32_t i = 0; i < 10; i++) {
                            if (SC < lmt_sc2[i] * (float)(1 << (bitDepthLuma - 8))) {
                                scVal = i;
                                break;
                            }
                        }

                        SC /= 1.414;

                        float tsc_RTML = 0.6f*SC;
                        float tsc_RTF = tsc_RTML / 2;
                        float tsc_RTMG = SC;
                        float tsc_RTS = 1.4f * SC;

                        // RTF RTS logic for P Frame is based on MC Error

                        float pdsad_past = 0.0f;
                        float pdsad_futr = 0.0f;
                        FrameIter itStart = ++FrameIter(itCurr);
#ifdef LOW_COMPLX_PAQ
                        if (itStart != end) {
                            float sadFrm[17] = { 0 };
                            float sadSum = 0;
                            float sadMax = 0;
                            int32_t sadCnt = 0;
                            for (FrameIter it = itStart; it != end; it++) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                float psad_futr = 0.0f;
                                for (int32_t i = 0; i < numSubBlkh; i++) {
                                    for (int32_t j = 0; j < numSubBlkw; j++) {
                                        int32_t off = (((pelY + y) >> (log2SubBlk)) + i) * widthInSubBlks + ((pelX + x) >> (log2SubBlk)) + j;
                                        psad_futr += stat->m_interSad[off];
                                    }
                                }
                                psad_futr /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_futr;
                                sadCnt++;
                                sadSum += psad_futr;
                            }
                            float sadAvg = sadSum / sadCnt;
                            int32_t numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_futr = sadMax;
                            }
                            else {
                                pdsad_futr = sadAvg * 2;
                            }
                        }
                        if (itCurr != begin) {
                            float sadFrm[17] = { 0 };
                            float sadSum = 0;
                            float sadMax = 0;
                            int32_t sadCnt = 0;
                            for (FrameIter it = itCurr; it != begin; it--) {
                                Statistics* stat = (*it)->m_stats[lowRes];
                                int uMVnum = 0;
                                float psad_past = 0.0;
                                for (int32_t i = 0; i < numSubBlkh; i++) {
                                    for (int32_t j = 0; j < numSubBlkw; j++) {
                                        int32_t off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                        psad_past += stat->m_interSad[off];
                                    }
                                }
                                psad_past /= (blkW4*blkH4) << 4;
                                sadFrm[sadCnt] = psad_past;
                                sadCnt++;
                                sadSum += psad_past;
                            }
                            float sadAvg = sadSum / sadCnt;
                            int32_t numSadHigh = 0;
                            for (int fri = 0; fri < sadCnt; fri++) {
                                if (sadFrm[fri] > sadAvg) numSadHigh++;
                            }
                            if (numSadHigh == 1) {
                                pdsad_past = sadMax;
                            }
                            else {
                                pdsad_past = sadAvg * 2;
                            }
                        }
#else
                        for (int32_t i = 0; i<numSubBlkh; i++) {
                            for (int32_t j = 0; j<numSubBlkw; j++) {
                                int32_t off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                pdsad_past += inFrame->m_stats[lowRes]->m_interSad_pdist_past[off];
                                pdsad_futr += inFrame->m_stats[lowRes]->m_interSad_pdist_future[off];
                            }
                        }
                        pdsad_past /= (blkW4*blkH4) << 4;
                        pdsad_futr /= (blkW4*blkH4) << 4;
#endif
                        // future (forward propagate)
                        int32_t coloc_futr = 0;
                        for (FrameIter it = itStart; it != end; it++) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            int32_t uMVnum = 0;
                            float psad_futr = 0.0f;
                            for (int32_t i = 0; i<numSubBlkh; i++) {
                                for (int32_t j = 0; j<numSubBlkw; j++) {
                                    int32_t off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                    psad_futr += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_futr /= (blkW4*blkH4) << 4;
                            if (psad_futr < tsc_RTML && uMVnum < 1 + std::max(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc_futr++;
                            else break;
                        }


                        // past (backward propagate)
                        int32_t coloc_past = 0;
                        for (FrameIter it = itCurr; it != begin; it--) {
                            Statistics* stat = (*it)->m_stats[lowRes];
                            int uMVnum = 0;
                            float psad_past = 0.0;
                            for (int32_t i = 0; i<numSubBlkh; i++) {
                                for (int32_t j = 0; j<numSubBlkw; j++) {
                                    int32_t off = (((pelY + y) >> log2SubBlk) + i) * widthInSubBlks + ((pelX + x) >> log2SubBlk) + j;
                                    psad_past += stat->m_interSad[off];
                                    AddCandidate(&stat->m_mv[off], &uMVnum, candMVs);
                                }
                            }
                            psad_past /= (blkW4*blkH4) << 4;
                            if (psad_past < tsc_RTML && uMVnum < 1 + std::max(1, (numSubBlkh*numSubBlkw) / 2))
                                coloc_past++;
                            else
                                break;
                        }

                        if (itStart == end) {
                            pdsad_futr = tsc_RTS;
                            coloc_futr = 0;
                        }
                        if (itCurr == begin) {
                            pdsad_past = tsc_RTS;
                            coloc_past = 0;
                        }

                        int32_t widthInTiles = widthInCtbs; // << depth;
                        int32_t off = ((pelY + y)*widthInTiles + (pelX + x)) >> log2BlkSize;
                        //inFrame->m_stats[0]->coloc_futr[depth][off] = coloc_futr;
                        inFrame->m_stats[0]->coloc_futr[off] = coloc_futr;
                        int32_t coloc = std::max(coloc_past, coloc_futr);
                        if (futr_key) coloc = coloc_past;

                        //int32_t &qp_mask = inFrame->m_stats[0]->qp_mask[depth][off];
                        int32_t &qp_mask = inFrame->m_stats[0]->qp_mask[off];
                        //if (coloc_past >= REF_DIST && past_frame->m_stats[0]->coloc_futr[depth][off] >= REF_DIST) {
                        if (coloc_past >= REF_DIST && past_frame->m_stats[0]->coloc_futr[off] >= REF_DIST) {
                            // Stable Motion & P Skip (when GOP is small repeated P QP lowering can change RD Point)
                            // Avoid quantization noise recoding
                            //inFrame->qp_mask[off] = std::min(0, past_frame->qp_mask[off]+1);
                            qp_mask = 0; // Propagate
                        }
                        else {
                            if (futr_key) {
                                coloc = std::min(REF_DIST / 2, coloc);
                            }
                            if (coloc >= 8 && coloc == coloc_futr && pdsad_futr<tsc_RTML) {
                                // Stable Motion & Motion reuse
                                qp_mask = -1 * std::min((int)futr_qp, (int)(((float)coloc / 8.0)*futr_qp));
                            }
                            else if (coloc >= 8 && coloc == coloc_futr && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion reuse
                                qp_mask = -1 * std::min((int)futr_qp, (int)(((float)coloc / 8.0)*4.0));
                            }
                            else if (coloc>1 && coloc == coloc_futr && pdsad_futr<tsc_RTMG) {
                                // Stable Motion & Motion Reuse
                                qp_mask = -1 * std::min(4, (int)(((float)coloc / 8.0)*4.0));
                                // Possibly propagation
                            }
                            else if (coloc>1 && coloc == coloc_past && pdsad_past<tsc_RTMG) {
                                // Stable Motion & Motion Reuse
                                qp_mask = -1 * std::min(4, (int)(((float)coloc / 8.0)*4.0));
                                // Past Boost probably no propagation since coloc_futr is less than coloc_past
                            }
                            else if (scVal >= 6 && pdsad_past>tsc_RTS && coloc == 0) {
                                // reduce disproportional cost on high texture and bad motion
                                // use pdsad_past since its coded a in order p frame
                                qp_mask = 1;
                            }
                            else {
                                // Default
                                qp_mask = 0;
                            }
                        }
                    }
                }
            }
        }
    }
}
/*
void AV1Enc::DetermineQpMap_PFrame(FrameIter begin, FrameIter curr, FrameIter end, AV1VideoParam & videoParam)
{
    FrameIter itCurr = curr;
    Frame* inFrame    = *curr;
    Frame* past_frame = *begin;

    int row, col;
    int c8_width, c8_height;
    //int widthInRegionGrid = inFrame->m_origin->width / 16;
    int wBlock = inFrame->m_origin->width/8;
    int w4 = inFrame->m_origin->width/4;

    const int CU_SIZE = videoParam.MaxCUSize;
    int heightInTiles = videoParam.PicHeightInCtbs;
    int widthInTiles  = videoParam.PicWidthInCtbs;

    const int MAX_TILE_SIZE = CU_SIZE;//64;
    int rbw = MAX_TILE_SIZE/8;
    int rbh = MAX_TILE_SIZE/8;
    int r4w = MAX_TILE_SIZE/4;
    int r4h = MAX_TILE_SIZE/4;

    float tdiv;
    int M,N, mbT,nbT, m4T, n4T;
    static float lmt_sc2[10]   =   {4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78, (float)INT_MAX}; // lower limit of SFM(Rs,Cs) range for spatial classification
    AV1MV candMVs[(64/8)*(64/8)];

    float tsc_RTF, tsc_RTML, tsc_RTMG, tsc_RTS;
    const float futr_qp = MAX_DQP;
    const int32_t REF_DIST = std::min(8, (inFrame->m_frameOrder - past_frame->m_frameOrder));

    const int32_t statIdx = 0;// we use original stats here

    c8_height = (inFrame->m_origin->height);
    c8_width  = (inFrame->m_origin->width);

    bool futr_key = false;
    for(row=0;row<heightInTiles;row++) {
        for(col=0;col<widthInTiles;col++) {
            int i, j;

            N = (col==widthInTiles-1)?(c8_width-(widthInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;
            M = (row==heightInTiles-1)?(c8_height-(heightInTiles-1)*MAX_TILE_SIZE):MAX_TILE_SIZE;

            mbT = M/8;
            nbT = N/8;
            m4T = M/4;
            n4T = N/4;
            tdiv = M*N;

            float Rs=0.0, Cs=0.0;
            for(i=0;i<m4T;i++) {
                for(j=0;j<n4T;j++) {
                    Rs += inFrame->m_stats[statIdx]->m_rs[0][(row*r4h+i)*w4+(col*r4w+j)];
                    Cs += inFrame->m_stats[statIdx]->m_cs[0][(row*r4h+i)*w4+(col*r4w+j)];
                }
            }
            Rs/=(m4T*n4T);
            Cs/=(m4T*n4T);
            float SC = sqrt(Rs + Cs);
            inFrame->m_stats[statIdx]->rscs_ctb[row*widthInTiles+col] = SC;

            int scVal = 0;
            for(i = 0;i<10;i++) {
                if(SC < lmt_sc2[i]*(float)(1<<(inFrame->m_bitDepthLuma-8))) {
                    scVal   =   i;
                    break;
                }
            }
            inFrame->m_stats[statIdx]->sc_mask[row*widthInTiles+col] = scVal;
            // Modified (Pre analysis now Integer ME only)
            SC /= 1.414;

            tsc_RTML = 0.6f*SC;
            tsc_RTF = tsc_RTML/2;
            tsc_RTMG = SC; // std::min(2 * tsc_RTML, SC / 1.414);
            tsc_RTS = 1.4f * SC; // std::min(3 * tsc_RTML, SC / 1.414);


            // RTF RTS logic for P Frame is based on MC Error
            float pdsad_past=0.0;
            float pdsad_futr=0.0;
            for(i=0;i<mbT;i++) {
                for(j=0;j<nbT;j++) {
                    pdsad_past += inFrame->m_stats[statIdx]->m_interSad_pdist_past[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                    pdsad_futr += inFrame->m_stats[statIdx]->m_interSad_pdist_future[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                }
            }
            pdsad_past/=tdiv;
            pdsad_futr/=tdiv;


            // future (forward propagate)
            int coloc_futr=0;
            FrameIter itStart = ++FrameIter(itCurr);
            for (FrameIter it = itStart; it != end; it++) {
                int uMVnum = 0;
                Statistics* stat = (*it)->m_stats[statIdx];
                float psad_futr=0.0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_futr += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_futr/=tdiv;
                if (psad_futr < tsc_RTML && uMVnum < (mbT*nbT)/2)
                    coloc_futr++;
                else break;
            }


            // past (backward propagate)
            int32_t coloc_past=0;
            for (FrameIter it = itCurr; it != begin; it--) {
                Statistics* stat = (*it)->m_stats[statIdx];
                int uMVnum = 0;
                float psad_past=0.0;
                for(i=0;i<mbT;i++) {
                    for(j=0;j<nbT;j++) {
                        psad_past += stat->m_interSad[(row*rbh+i)*(wBlock)+(col*rbw+j)];
                        AddCandidate(&stat->m_mv[(row*rbh+i)*(wBlock)+(col*rbw+j)], &uMVnum, candMVs);
                    }
                }
                psad_past/=tdiv;
                if(psad_past<tsc_RTML && uMVnum<(mbT*nbT)/2)
                    coloc_past++;
                else break;
            };


            inFrame->m_stats[statIdx]->coloc_past[row*widthInTiles+col] = coloc_past;
            inFrame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] = coloc_futr;
            int32_t coloc = std::max(coloc_past, coloc_futr);

            if(futr_key) coloc = coloc_past;

            if (coloc_past >= REF_DIST && past_frame->m_stats[statIdx]->coloc_futr[row*widthInTiles+col] >= REF_DIST) {
                // Stable Motion & P Skip (when GOP is small repeated P QP lowering can change RD Point)
                // Avoid quantization noise recoding
                //inFrame->qp_mask[row*widthInTiles+col] = std::min(0, past_frame->qp_mask[row*widthInTiles+col]+1);
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0; // Propagate
            } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTML && !futr_key) {
                // Stable Motion & Motion reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min((int)futr_qp, (int)(((float)coloc/8.0)*futr_qp));
            } else if(coloc>=8 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                // Stable Motion & Motion reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min((int)futr_qp, (int)(((float)coloc/8.0)*4.0));
            } else if(coloc>1 && coloc==coloc_futr && pdsad_futr<tsc_RTMG && !futr_key) {
                // Stable Motion & Motion Reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min(4, (int)(((float)coloc/8.0)*4.0));
                // Possibly propagation
            } else if(coloc>1 && coloc==coloc_past && pdsad_past<tsc_RTMG) {
                // Stable Motion & Motion Reuse
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = -1*std::min(4, (int)(((float)coloc/8.0)*4.0));
                // Past Boost probably no propagation since coloc_futr is less than coloc_past
            }  else if(scVal>=6 && pdsad_past>tsc_RTS && coloc==0) {
                // reduce disproportional cost on high texture and bad motion
                // use pdsad_past since its coded a in order p frame
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 1;
            } else {
                // Default
                inFrame->m_stats[statIdx]->qp_mask[row*widthInTiles+col] = 0;
            }
        }
    }
}
*/
void WriteQpMap(Frame *inFrame, const AV1VideoParam& param) {

#ifdef PAQ_LOGGING
    int heightInTiles = param.PicHeightInCtbs;
    int widthInTiles = param.PicWidthInCtbs;
    int row, col;
    FILE *fp = fopen("qpmap.txt", "a+");
    fprintf(fp, "%d ", inFrame->m_frameOrder);
    fprintf(fp, "%d %d \n", 1, 1);
    for(row = 0; row<heightInTiles; row++) {
        for(col=0; col<widthInTiles; col++) {
            int val = inFrame->m_stats[0]->qp_mask[row*widthInTiles+col];
            fprintf(fp, "%+d ", val);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
#else
    inFrame, param;
    return;
#endif
}

void WriteDQpMap(Frame *inFrame, const AV1VideoParam& param) {

#ifdef PAQ_LOGGING

    int heightInTiles = param.PicHeightInCtbs;
    int widthInTiles = param.PicWidthInCtbs;
    int row, col;
    FILE *fp = fopen("qpmap.txt", "a+");
    fprintf(fp, "%d ", inFrame->m_frameOrder);
    fprintf(fp, "%d %d \n", 1, inFrame->m_sliceQpY);
    for(row = 0; row<heightInTiles; row++) {
        for(col=0; col<widthInTiles; col++) {
            int val = inFrame->m_lcuQps[row*widthInTiles+col];
            fprintf(fp, "%+d ", val);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
#else
    inFrame, param;
    return;
#endif
}

// based on blk_8x8. picHeight maybe original or lowres
int32_t GetNumActiveRows(int32_t region_row, int32_t rowsInRegion, int32_t picHeight)
{
    int32_t numRows = rowsInRegion;
    if(((region_row * rowsInRegion) + numRows) * SIZE_BLK_LA > picHeight)
        numRows = (picHeight - (region_row * rowsInRegion)*SIZE_BLK_LA) / SIZE_BLK_LA;

    return numRows;
}


Frame* AV1Enc::GetPrevAnchor(FrameIter begin, FrameIter end, const Frame* curr)
{
    FrameIter itCurr = std::find_if(begin, end, isEqual(curr->m_frameOrder));

    Frame* prevP = NULL;
    for (FrameIter it = begin; it != itCurr; it++) {
        if ((*it)->m_picCodeType == MFX_FRAMETYPE_P ||
            (*it)->m_picCodeType == MFX_FRAMETYPE_I) {
                prevP = (*it);
        }
    }

    return prevP;
}

Frame* AV1Enc::GetNextAnchor(FrameIter curr, FrameIter end)
{
    Frame *nextP = NULL;
    FrameIter it = std::find_if(++curr, end, isEqualRefFrame());
    if (it != end)
        nextP = (*it);

    return nextP;
}


void AV1Enc::DoPDistInterAnalysis_OneRow(Frame* curr, Frame* prevP, Frame* nextP, int32_t region_row, int32_t lowresRowsInRegion, int32_t originRowsInRegion, uint8_t LowresFactor, uint8_t FullresMetrics)
{
    FrameData* currFrame  = LowresFactor ? curr->m_lowres : curr->m_origin;
    Statistics* stat = curr->m_stats[LowresFactor ? 1 : 0];

    // compute (PDist = miniGopSize) Past ME
    if (prevP) {
        FrameData * prevFrame = LowresFactor ? prevP->m_lowres : prevP->m_origin;
        int32_t rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
        for (int32_t i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, prevFrame, &stat->m_interSad_pdist_past[0], NULL, &stat->m_mv_pdist_past[0], 0, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i,
                FullresMetrics ? curr->m_origin : NULL,
                FullresMetrics ? prevP->m_origin : NULL);
        }
#ifndef LOW_COMPLX_PAQ
        if (LowresFactor) {
            FrameData* currFrame  = curr->m_origin;
            FrameData * prevFrame = prevP->m_origin;

            int32_t rowsInRegion = originRowsInRegion;
            int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, prevFrame->height);
            for(int32_t i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, prevFrame, &curr->m_stats[0]->m_interSad_pdist_past[0], NULL, &curr->m_stats[0]->m_mv_pdist_past[0], 1, 2, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion)+i); // refine
            }

        }
#endif
    }

    // compute (PDist = miniGopSize) Future ME
    if (nextP) {
        FrameData* nextFrame = LowresFactor ? nextP->m_lowres : nextP->m_origin;
        int32_t rowsInRegion = LowresFactor ? lowresRowsInRegion : originRowsInRegion;
        int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
        for (int32_t i = 0; i < numActiveRows; i++) {
            DoInterAnalysis_OneRow(currFrame, NULL, nextFrame, &stat->m_interSad_pdist_future[0], NULL, &stat->m_mv_pdist_future[0], 0, 1, LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i,
                FullresMetrics ? curr->m_origin : NULL,
                FullresMetrics ? nextP->m_origin : NULL);
        }
#ifndef LOW_COMPLX_PAQ
        if (LowresFactor) {
            FrameData* currFrame = curr->m_origin;
            FrameData* nextFrame = nextP->m_origin;

            int32_t rowsInRegion = originRowsInRegion;
            int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, nextFrame->height);
            for(int32_t i=0;i<numActiveRows;i++) {
                DoInterAnalysis_OneRow(currFrame, stat, nextFrame, &curr->m_stats[0]->m_interSad_pdist_future[0], NULL, &curr->m_stats[0]->m_mv_pdist_future[0], 1, 1,LowresFactor, curr->m_bitDepthLuma, (region_row * rowsInRegion) + i);//refine
            }
        }
#endif
    }
}


void AV1Enc::BackPropagateAvgTsc(FrameIter prevRef, FrameIter currRef)
{
    Frame* input = *currRef;

    //uint32_t frameType = input->m_picCodeType;
    FrameData* origFrame  =  input->m_origin;
    Statistics* origStats = input->m_stats[0];

    origStats->avgTSC = 0.0;
    origStats->avgsqrSCpp = 0.0;
    int32_t gop_size = 1;
    float avgTSC     = 0.0;
    float avgsqrSCpp = 0.0;

    for (FrameIter it = currRef; it != prevRef; it--) {
        Statistics* stat = (*it)->m_stats[0];
        avgTSC     += stat->TSC;
        avgsqrSCpp += stat->SC;
        gop_size++;
    }

    int32_t frameSize = origFrame->width * origFrame->height;

    avgTSC /= gop_size;
    avgTSC /= frameSize;

    avgsqrSCpp /= gop_size;
    avgsqrSCpp/=sqrt((float)(frameSize));
    avgsqrSCpp = sqrt(avgsqrSCpp);

    for (FrameIter it = currRef; it != prevRef; it--) {
        Statistics* stats = (*it)->m_stats[0];
        stats->avgTSC = avgTSC;
        stats->avgsqrSCpp = avgsqrSCpp;
    }
}

void WritePSAD(Frame *inFrame)
{
#ifdef PAQ_LOGGING

    int i,j,M,N,P;
    int *p;

    M = inFrame->height / 8;
    N = inFrame->width / 8;
    {
        FILE *fp = fopen("psad_map.txt", "a+");
        fprintf(fp, "PSAD for Frame %d\n", inFrame->m_frameOrder);
        for(i=0;i<M;i++)
        {
            for(j=0;j<N;j++)
            {
                //if(fp)
                fprintf(fp, "%4d ", inFrame->m_interSad[i*N+j]);
            }
            //if(fp)
            fprintf(fp, "\n");
        }
        fclose(fp);
    }
#else
    inFrame;
    return;
#endif
}

void WriteMV(Frame *inFrame)
{
#ifdef PAQ_LOGGING

    FILE *fp=NULL;
    int i,j;
    char fname[64];
    int *p;

    int height = inFrame->height / 8;
    int width  = inFrame->width / 8;
    {
        sprintf_s(fname, sizeof(fname), "%s_%d_%dx%d.txt", "psmv_me", inFrame->m_frameOrder, inFrame->width, inFrame->height);

        fp = fopen(fname, "a+");
        if(!fp) return;
        for(j=0;j<height;j++) {
            for(i=0;i<width;i++) {
                fprintf(fp, "(%7d,%7d) ", inFrame->m_mv[i+j*width].mvx, inFrame->m_mv[i+j*width].mvy);
            }
            fprintf(fp, "\n");
        }
        fprintf(fp, "\n");
        fclose(fp);
    }
#else
    inFrame;
    return;
#endif
}


int Lookahead::ConfigureLookaheadFrame(Frame* in, int32_t fieldNum)
{
    if (in == NULL && m_inputQueue.empty()) {
        return 0;
    }

    if (fieldNum == 0 && in) {
        std::vector<ThreadingTask> &tt = in->m_ttLookahead;
        int32_t poc = in->m_frameOrder;

        size_t startIdx = 0;
        tt[startIdx].Init(TT_PREENC_START, 0, 0, poc, 1, 0);
        tt[startIdx].la = this;

        size_t endIdx   = tt.size() - 1;
        tt[endIdx].Init(TT_PREENC_END, 0, 0, poc, 0, 0);
        tt[endIdx].la = this;


        for (size_t idx = 1; idx < endIdx; idx++) {
            int32_t row = (int32_t)idx - 1;
            tt[idx].Init(TT_PREENC_ROUTINE, row, 0, poc, 0, 0);
            tt[idx].la = this;

            // dependency LA_ROUTINE <- LA_START
            AddTaskDependency(&tt[idx], &tt[startIdx], &m_ttHubPool);

            // dependency LA_END     <- LA_ROUTINE
            AddTaskDependency(&tt[endIdx], &tt[idx], &m_ttHubPool);
        }
    }

     return 1;
}


void AV1Encoder::OnLookaheadCompletion()
{
    if (m_la.get() == 0)
        return;

    // update
    FrameIter itEnd = m_inputQueue.begin();
    for (itEnd; itEnd != m_inputQueue.end(); itEnd++) {
        if ((*itEnd)->m_lookaheadRefCounter > 0)
            break;
    }

    if (m_videoParam.AnalyzeCmplx) {
        for (FrameIter it = m_inputQueue.begin(); it != itEnd; it++) {
            AverageComplexity(*it, m_videoParam);
        }
    }

    m_lookaheadQueue.splice(m_lookaheadQueue.end(), m_inputQueue, m_inputQueue.begin(), itEnd);
}

bool MetricIsGreater(const StatItem &l, const StatItem &r) { return (l.met > r.met); }

//void WriteCmplx(Frame *frames[], int32_t numFrames)
//{
//    {
//        FILE *fp = fopen("BRC_complexity.txt", "a+");
//        fprintf(fp, "complexity for Frame %d type %d\n", frames[0]->m_frameOrder, frames[0]->m_picCodeType);
//        for(int i=0;i<numFrames;i++) {
//            fprintf(fp, "%11.3f \n", frames[i]->m_avgBestSatd);
//
//        }
//        //fprintf(fp, "\n");
//        fclose(fp);
//    }
//}
// ========================================================
void Lookahead::AnalyzeSceneCut_AndUpdateState_Atul(Frame* in)
{
    if (in == NULL)
        return;

    FrameIter curr = std::find_if(m_inputQueue.begin(), m_inputQueue.end(), isEqual(in->m_frameOrder));
    Frame* prev = curr == m_inputQueue.begin() ? NULL : *(--FrameIter(curr));
    int32_t sceneCut = DetectSceneCut_AMT(in, prev);
    if (sceneCut && in->m_frameOrder < 2) sceneCut = 0;

#if 0
    if (sceneCut) {
        FILE *fp = fopen("sceneReport.txt", "a+");
        fprintf(fp, "%i\t", in->m_frameOrder);
        fclose(fp);
    }
#endif

    if (sceneCut) {
        m_enc.m_sceneOrder++;
        if (in->m_stats[1])
            in->m_stats[1]->m_sceneCut = -1;
        if (in->m_stats[0])
            in->m_stats[0]->m_sceneCut = -1;
    }

    in->m_sceneOrder = m_enc.m_sceneOrder;
    if (!m_videoParam.AdaptiveI) sceneCut = 0;
    //special case for interlace mode to keep (P | I) pair instead of (I | P)
    int32_t rightAnchor = (m_videoParam.picStruct == MFX_PICSTRUCT_PROGRESSIVE || (m_videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE && in->m_secondFieldFlag));

    uint8_t insertKey = 0;
    if (sceneCut) {
        if (in->m_picCodeType == MFX_FRAMETYPE_B) {
            m_pendingSceneCut = 1;
        } else {
            if  (rightAnchor){
                m_pendingSceneCut = 0;
                insertKey = 1;
            }
        }
    } else if (m_pendingSceneCut && (in->m_picCodeType != MFX_FRAMETYPE_B) && rightAnchor) {
        m_pendingSceneCut = 0;
        insertKey = 1;
    }

    // special marker for accurate INTRA encode for RASL
    if (m_pendingSceneCut) {
        in->m_forceTryIntra = 1;
    }

    int32_t isInsertI = insertKey && (in->m_picCodeType == MFX_FRAMETYPE_P);
    if (isInsertI) {
        Frame* frame = in;

        // restore global state
        m_enc.RestoreGopCountersFromFrame(frame, !!m_videoParam.encodedOrder);

        // light configure
        frame->m_isIdrPic = false;
        frame->m_picCodeType = MFX_FRAMETYPE_I;
        //frame->m_poc = 0;

        if (m_videoParam.PGopPicSize > 1) {
            //const uint8_t PGOP_LAYERS[PGOP_PIC_SIZE] = { 0, 2, 1, 2 };
            frame->m_RPSIndex = 0;
            frame->m_pyramidLayer = 0;//PGOP_LAYERS[frame->m_RPSIndex];
        }

        m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);

        // we need update all frames in inputQueue after replacement to propogate SceneCut (frame type change) effect
        FrameIter it = curr;
        it++;
        for (it; it != m_inputQueue.end(); it++) {
            frame = (*it);
            m_enc.ConfigureInputFrame(frame, !!m_videoParam.encodedOrder);
            m_enc.UpdateGopCounters(frame, !!m_videoParam.encodedOrder);
        }
    }

#if 0
    {
        FILE *fp = fopen("scene.txt", "a+");
        fprintf(fp, "FrameOrder %i SceneOrder %i SceneCutDetected %i InsertI %i\n", in->m_frameOrder, in->m_sceneOrder, sceneCut, isInsertI);
        fclose(fp);
    }
#endif

    if (insertKey && in->m_picCodeType == MFX_FRAMETYPE_I) {
        if (in->m_stats[1])
            in->m_stats[1]->m_sceneCut = 1;
        if (in->m_stats[0])
            in->m_stats[0]->m_sceneCut = 1;
    }

    if (prev) {
        prev->m_lookaheadRefCounter--;
    }

} //
// ========================================================
void Lookahead::ResetState()
{
    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;
}

void AV1Enc::AverageComplexity(Frame *in, AV1VideoParam& videoParam)
{
    if (in == NULL)
        return;

    uint8_t useLowres = videoParam.LowresFactor;
    FrameData* frame = useLowres ? in->m_lowres : in->m_origin;
    Statistics* stat = in->m_stats[useLowres ? 1 : 0];

    int64_t sumSatd = 0;
    int64_t sumSatdIntra = 0;
    int64_t sumSatdInter = 0;
    int32_t intraBlkCount = 0;

    if (in->m_frameOrder > 0 && (in->m_picCodeType != MFX_FRAMETYPE_I || stat->m_sceneCut > 0)) {
    //if (in->m_frameOrder > 0 && (in->m_picCodeType != MFX_FRAMETYPE_I)) {
        int32_t blkCount = (int32_t)stat->m_intraSatd.size();

        for (int32_t blk = 0; blk < blkCount; blk++) {
            if (in->m_picCodeType != MFX_FRAMETYPE_I)
                sumSatd += std::min(stat->m_intraSatd[blk], stat->m_interSatd[blk]);
            else
                sumSatd += stat->m_intraSatd[blk];

            if( stat->m_intraSatd[blk] <= stat->m_interSatd[blk] ) {
                intraBlkCount++;
            }

            sumSatdIntra += stat->m_intraSatd[blk];
            sumSatdInter += stat->m_interSatd[blk];
        }
        stat->m_intraRatio = (float)intraBlkCount / blkCount;

    } else {
        sumSatd = std::accumulate(stat->m_intraSatd.begin(), stat->m_intraSatd.end(), 0);
        stat->m_intraRatio = 1.f;
        sumSatdIntra = sumSatd;
    }

    // store metric in frame
    int32_t resolution = frame->width * frame->height;
    if(useLowres && videoParam.FullresMetrics) {
        resolution = in->m_origin->width * in->m_origin->height;
    }

    stat->m_avgBestSatd = (float)sumSatd / (resolution);
    stat->m_avgIntraSatd = (float)sumSatdIntra / (resolution);
    stat->m_avgInterSatd = (float)sumSatdInter / (resolution);

    // hack for BRC
    if (in->m_stats[0]) {
        in->m_stats[0]->m_avgBestSatd = stat->m_avgBestSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[0]->m_avgIntraSatd = stat->m_avgIntraSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[0]->m_avgInterSatd = stat->m_avgInterSatd / (1 << videoParam.bitDepthLumaShift);
    }
    if (in->m_stats[1]) {
        in->m_stats[1]->m_avgBestSatd = stat->m_avgBestSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[1]->m_avgIntraSatd = stat->m_avgIntraSatd / (1 << videoParam.bitDepthLumaShift);
        in->m_stats[1]->m_avgInterSatd = stat->m_avgInterSatd / (1 << videoParam.bitDepthLumaShift);
    }

    if (useLowres) {
        uint8_t res = Saturate(0, 1, useLowres-1);
        float scaleFactor = 1.29;
        if (!videoParam.FullresMetrics) {
            float tabCorrFactor[] = {1.5f, 2.f};
            scaleFactor = tabCorrFactor[res];
        }
        if(videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE) scaleFactor *= 1.29;
        if (in->m_stats[0]) {
            in->m_stats[0]->m_avgBestSatd /= scaleFactor;
            in->m_stats[0]->m_avgIntraSatd /= scaleFactor;
            in->m_stats[0]->m_avgInterSatd /= scaleFactor;
        }
        if (in->m_stats[1]) {
            in->m_stats[1]->m_avgBestSatd /= scaleFactor;
            in->m_stats[1]->m_avgIntraSatd /= scaleFactor;
            in->m_stats[1]->m_avgInterSatd /= scaleFactor;
        }
    }
}


void AV1Enc::AverageRsCs(Frame *in)
{
    if (in == NULL)
        return;

    Statistics* stat = in->m_stats[0];
    FrameData* data = in->m_origin;
    stat->m_frameCs = std::accumulate(stat->m_cs[4], stat->m_cs[4] + stat->m_rcscSize[4], 0);
    stat->m_frameRs = std::accumulate(stat->m_rs[4], stat->m_rs[4] + stat->m_rcscSize[4], 0);

    int32_t hblocks  = (int32_t)data->height / BLOCK_SIZE;
    int32_t wblocks  = (int32_t)data->width / BLOCK_SIZE;
    stat->m_frameCs /= hblocks * wblocks;
    stat->m_frameRs /= hblocks * wblocks;
    stat->m_frameCs  = sqrt(stat->m_frameCs);
    stat->m_frameRs  = sqrt(stat->m_frameRs);

    float RsGlobal = in->m_stats[0]->m_frameRs;
    float CsGlobal = in->m_stats[0]->m_frameCs;
    int32_t frameSize = in->m_origin->width * in->m_origin->height;

    in->m_stats[0]->SC = sqrt(((RsGlobal*RsGlobal) + (CsGlobal*CsGlobal))*frameSize);
    in->m_stats[0]->TSC = std::accumulate(in->m_stats[0]->m_interSad.begin(), in->m_stats[0]->m_interSad.end(), 0);
}


template<class InputIterator, class UnaryPredicate>
InputIterator h265_findPastRef_OrSetBegin(InputIterator curr, InputIterator begin, UnaryPredicate pred)
{
    while (curr!=begin) {
        if (pred(*curr)) return curr;
        curr--;
    }
    return begin;
}

int32_t AV1Enc::BuildQpMap(FrameIter begin, FrameIter end, int32_t frameOrderCentral, AV1VideoParam& videoParam, int32_t doUpdateState)
{
    //FrameIter begin = m_inputQueue.begin();
    //FrameIter end   = m_inputQueue.end();

    //int32_t frameOrderCentral = in->m_frameOrder - m_bufferingPaq;
    uint8_t isBufferingEnough = (frameOrderCentral >= 0);
    FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));

    // make decision
    if (isBufferingEnough && curr != end) {

        uint32_t frameType = (*curr)->m_picCodeType;
        bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);

        if (isRef) {
            Frame* prevP = GetPrevAnchor(begin, end, *curr);
            Frame* nextP = GetNextAnchor(curr, end);
            FrameIter itNextRefPlus1 = nextP ? ++(std::find_if(begin, end, isEqual(nextP->m_frameOrder))) : end;

            uint32_t frameType = (*curr)->m_picCodeType;
            if (videoParam.DeltaQpMode & AMT_DQP_PAQ) {

                if (nextP == NULL) {
                    std::fill((*curr)->m_stats[0]->m_interSad_pdist_future.begin(), (*curr)->m_stats[0]->m_interSad_pdist_future.end(), IPP_MAX_32S);
                    std::fill((*curr)->m_stats[1]->m_interSad_pdist_future.begin(), (*curr)->m_stats[1]->m_interSad_pdist_future.end(), IPP_MAX_32S);
                }
                if (frameType == MFX_FRAMETYPE_I)
                    DetermineQpMap_IFrame(curr, itNextRefPlus1, videoParam);
                else if (frameType == MFX_FRAMETYPE_P) {
                    FrameIter itPrevRef  = std::find_if(begin, end, isEqual(prevP->m_frameOrder));
                    DetermineQpMap_PFrame(itPrevRef, curr, itNextRefPlus1, videoParam);
                }
            }
            if ((videoParam.DeltaQpMode & AMT_DQP_CAL) && (frameType == MFX_FRAMETYPE_P)) {
                FrameIter itPrevRef = std::find_if(begin, end, isEqual(prevP->m_frameOrder));
                BackPropagateAvgTsc(itPrevRef, curr);
            }
        }
        //WriteQpMap(*curr, m_videoParam);
    }


    // update state
    if (doUpdateState) {
        //int32_t frameOrderCentral = in->m_frameOrder - m_bufferingPaq;
        uint8_t isBufferingEnough = (frameOrderCentral >= 0);
        FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));

        // Release resource counter
        if (isBufferingEnough && curr != end) {
            uint32_t frameType = (*curr)->m_picCodeType;
            bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);
            //bool isEos = (m_slideWindowPaq[centralPos + 1] == -1);

            if (/*isEos || */isRef) {
                FrameIter itEnd = /*isEos ? end : */curr; // isEos has more priority
                FrameIter itStart = begin;
                if (curr != begin) {
                    curr--;
                    itStart = h265_findPastRef_OrSetBegin(curr, begin, isEqualRefFrame());
                }
                for (FrameIter it = itStart; it != itEnd; it++) {
                    (*it)->m_lookaheadRefCounter--;
                }
            }
        }
    }

    return 0;
} //

void AV1Enc::GetLookaheadGranularity(const AV1VideoParam& videoParam, int32_t& regionCount, int32_t& lowRowsInRegion, int32_t& originRowsInRegion, int32_t& numTasks)
{
    int32_t lowresHeightInBlk = ((videoParam.Height >> videoParam.LowresFactor) + (SIZE_BLK_LA-1)) >> 3;
    //int32_t originHeightInBlk = ((videoParam.Height) + (SIZE_BLK_LA-1)) >> 3;

    regionCount = videoParam.num_threads;// should be based on blk_8x8
    regionCount = std::min(lowresHeightInBlk, regionCount);
    // adjust
    {
        lowRowsInRegion  = (lowresHeightInBlk + (regionCount - 1)) / regionCount;
        regionCount      = (lowresHeightInBlk + (lowRowsInRegion - 1)) / lowRowsInRegion;
    }

    lowRowsInRegion    = (lowresHeightInBlk + (regionCount - 1)) / regionCount;
    originRowsInRegion = lowRowsInRegion << videoParam.LowresFactor;

    /*int32_t*/ numTasks = 1 /*LA_START*/  + regionCount + 1 /*LA_END*/;

}

Lookahead::Lookahead(AV1Encoder & enc)
    : m_inputQueue(enc.m_inputQueue)
    , m_videoParam(enc.m_videoParam)
    , m_enc(enc)
    , m_pendingSceneCut(0)
{
    m_bufferingPaq = 0;

    if (m_videoParam.DeltaQpMode & (AMT_DQP_CAL|AMT_DQP_PAQ))
        m_bufferingPaq = m_videoParam.GopRefDist;

    // to prevent multiple PREENC_START per frame
    m_lastAcceptedFrame[0] = m_lastAcceptedFrame[1] = NULL;

    int32_t numTasks;
    GetLookaheadGranularity(m_videoParam, m_regionCount, m_lowresRowsInRegion, m_originRowsInRegion, numTasks);
}

Lookahead::~Lookahead()
{
}


const float CU_RSCS_TH[6][4][8] = {
    {{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  8.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0},{  4.0,  6.0,  9.0, 11.0, 14.0, 18.0, 26.0,65025.0}},
    {{  5.0,  6.0,  8.0, 11.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  7.0,  9.0, 11.0, 14.0, 18.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  7.0,  9.0, 12.0, 14.0, 18.0, 25.0,65025.0}},
    {{  5.0,  7.0, 10.0, 12.0, 15.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 20.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 14.0, 18.0, 24.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0}},
    {{  6.0,  8.0, 10.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  5.0,  8.0, 10.0, 13.0, 15.0, 19.0, 26.0,65025.0},{  5.0,  8.0, 10.0, 12.0, 15.0, 18.0, 24.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 15.0, 19.0, 24.0,65025.0}},
    {{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}},
    {{  6.0,  9.0, 11.0, 14.0, 17.0, 21.0, 27.0,65025.0},{  6.0,  9.0, 11.0, 13.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 12.0, 14.0, 16.0, 19.0, 25.0,65025.0},{  7.0,  9.0, 11.0, 13.0, 16.0, 19.0, 24.0,65025.0}}
};


const float LQ_M[6][8]   = {
    {4.2415, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.0684},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.2005, 4.2005, 4.2005},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // B3
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B4
};
const float LQ_K[6][8] = {
    {12.8114, 13.8536, 13.8536, 13.8536, 13.8395, 13.8395, 13.8395, 13.8395},   // I
    {12.3857, 12.3857, 12.3857, 12.3857, 12.3857, 13.7122, 13.7122, 13.7122},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // B3
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B4
};
const float LQ_M16[6][8]   = {
    {4.3281, 3.9818, 3.9818, 3.9818, 4.0684, 4.0684, 4.0684, 4.3281},   // I
    {4.5878, 4.5878, 4.5878, 4.5878, 4.5878, 4.3281, 4.3281, 4.3281},   // P
    {4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255, 4.3255},   // B1
    {4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052, 4.4052},   // B2
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005},   // B3
    {4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005, 4.2005}    // B4
};
const float LQ_K16[6][8] = {
    {14.4329, 14.8983, 14.8983, 14.8983, 14.9069, 14.9069, 14.9069, 14.4329},   // I
    {12.4456, 12.4456, 12.4456, 12.4456, 12.4456, 13.5336, 13.5336, 13.5336},   // P
    {13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286, 13.7286},   // B1
    {13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463, 13.1463},   // B2
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122},   // B3
    {13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122, 13.7122}    // B4
};


int GetCalqDeltaQp(Frame* frame, const AV1VideoParam & par, int32_t ctb_addr, float sliceLambda, float sliceQpY)
{
    int32_t picClass = 0;
    if(frame->m_picCodeType == MFX_FRAMETYPE_I) {
        picClass = 0;   // I
    } else {
        picClass = frame->m_pyramidLayer+1;
    }
    if (picClass > 4)
        picClass = 4;

    static int pQPi[5][4] = {{22,27,32,37}, {23,28,33,38}, {24,29,34,39}, {25,30,35,40}, {26,31,36,41}};
    int32_t qpClass = 0;
    if(sliceQpY < pQPi[picClass][0])
        qpClass = 0;
    else if(sliceQpY > pQPi[picClass][3])
        qpClass = 3;
    else
        qpClass = (int32_t)((sliceQpY - 22 - picClass) / 5);

    int32_t col =  (ctb_addr % par.PicWidthInCtbs);
    int32_t row =  (ctb_addr / par.PicWidthInCtbs);
    int32_t pelX = col * par.MaxCUSize;
    int32_t pelY = row * par.MaxCUSize;

    //TODO: replace by template call here
    // complex ops in Enqueue frame can cause severe threading eff loss -NG
    float rscs = 0;
    if (picClass < 2) {
        // calulate from 4x4 Rs/Cs
        int32_t N = (col==par.PicWidthInCtbs-1)?(frame->m_origin->width-(par.PicWidthInCtbs-1)*par.MaxCUSize):par.MaxCUSize;
        int32_t M = (row==par.PicHeightInCtbs-1)?(frame->m_origin->height-(par.PicHeightInCtbs-1)*par.MaxCUSize):par.MaxCUSize;
        int32_t m4T = M/4;
        int32_t n4T = N/4;
        int32_t X4 = pelX/4;
        int32_t Y4 = pelY/4;
        int32_t w4 = frame->m_origin->width/4;
        int32_t Rs=0,Cs=0;
        for(int32_t i=0;i<m4T;i++) {
            for(int32_t j=0;j<n4T;j++) {
                Rs += frame->m_stats[0]->m_rs[0][(Y4+i)*w4+(X4+j)];
                Cs += frame->m_stats[0]->m_cs[0][(Y4+i)*w4+(X4+j)];
            }
        }
        Rs/=(m4T*n4T);
        Cs/=(m4T*n4T);
        rscs = sqrt(Rs + Cs);
    }

    int32_t rscsClass = 0;
    {
        int32_t k = 7;
        for (int32_t i = 0; i < 8; i++) {
            if (rscs < CU_RSCS_TH[picClass][qpClass][i]*(float)(1<<(par.bitDepthLumaShift))) {
                k = i;
                break;
            }
        }
        rscsClass = k;
    }

    float dLambda = sliceLambda * 512.f;
    int32_t gopSize = frame->m_biFramesInMiniGop + 1;
    float QP = 0.f;
    if(16 == gopSize) {
        QP = LQ_M16[picClass][rscsClass]*log( dLambda ) + LQ_K16[picClass][rscsClass];
    } else if(8 == gopSize){
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    } else { // default case could be modified !!!
        QP = LQ_M[picClass][rscsClass]*log( dLambda ) + LQ_K[picClass][rscsClass];
    }
    QP -= sliceQpY;
    return (QP>=0.0)?int(QP+0.5):int(QP-0.5);
} //


void UpdateAllLambda(Frame* frame, const AV1VideoParam& param)
{
    Statistics* stats = frame->m_stats[0];

    bool IsHiCplxGOP = false;
    bool IsMedCplxGOP = false;
    if (param.DeltaQpMode&AMT_DQP_CAL) {
        float SADpp = stats->avgTSC;
        float SCpp  = stats->avgsqrSCpp;
        if (SCpp > 2.0) {
            float minSADpp = 0;
            if (param.GopRefDist > 8) {
                minSADpp = 1.3f*SCpp - 2.6f;
                if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                if (!IsHiCplxGOP) {
                    float minSADpp = 1.1f*SCpp - 2.2f;
                    if(minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                }
            }
            else {
                minSADpp = 1.1f*SCpp - 1.5f;
                if (minSADpp>0 && minSADpp<SADpp) IsHiCplxGOP = true;
                if (!IsHiCplxGOP) {
                    float minSADpp = 1.0f*SCpp - 2.0f;
                    if (minSADpp>0 && minSADpp<SADpp) IsMedCplxGOP = true;
                }
            }
        }
    }

}

void AV1Enc::ApplyDeltaQp(Frame* frame, const AV1VideoParam & par, uint8_t useBrc)
{
    int32_t numCtb = par.PicHeightInCtbs * par.PicWidthInCtbs;

    // assign PAQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_PAQ) {// no pure CALQ
        for (int32_t ctb = 0; ctb < numCtb; ctb++) {
            int32_t deltaQp = frame->m_stats[0]->qp_mask[ctb];
            //deltaQp = Saturate(-MAX_DQP, MAX_DQP, deltaQp);

            int32_t lcuQp = frame->m_lcuQps[ctb] + deltaQp;
            lcuQp = Saturate(0, 51, lcuQp);
            frame->m_lcuQps[ctb] = (int8_t)lcuQp;
        }
    }
    // recalc (align) CTB lambdas with CTB Qp
    for (uint8_t i = 0; i < par.NumSlices; i++)
        UpdateAllLambda(frame, par);

    // assign CALQ deltaQp
    if (par.DeltaQpMode&AMT_DQP_CAQ) {
        float baseQP = frame->m_sliceQpY;
        float sliceLambda =  frame->m_lambda;
        for (int32_t ctb_addr = 0; ctb_addr < numCtb; ctb_addr++) {
            int calq = GetCalqDeltaQp(frame, par, ctb_addr, sliceLambda, baseQP);

            int32_t totalDQP = calq;
            /*
            if(useBrc) {
            if(par.cbrFlag) totalDQP = Saturate(-1, 1, totalDQP);  // CBR
            else            totalDQP = Saturate(-2, 2, totalDQP);  // VBR
            }
            */
            if (par.DeltaQpMode&AMT_DQP_PAQ) {
                totalDQP += frame->m_stats[0]->qp_mask[ctb_addr];
            }
            //totalDQP = Saturate(-MAX_DQP, MAX_DQP, totalDQP);
            int32_t lcuQp = frame->m_sliceQpY + totalDQP;
            lcuQp = Saturate(0, 51, lcuQp);
            frame->m_lcuQps[ctb_addr] = (int8_t)lcuQp;
        }
    }

    // if (BRC) => need correct deltaQp (via QStep) to hack BRC logic
    /*
    if (useBrc) {
    int32_t totalDeltaQp = 0;
    int32_t corr = 0;
    float corr0 = pow(2.0, (frame->m_sliceQpY-4)/6.0);
    float denum = 0.0;
    for (int32_t ctb = 0; ctb < numCtb; ctb++) {
    denum += pow(2.0, (frame->m_lcuQps[ctb] -4) / 6.0);
    }
    corr = 6.0 * log10( (numCtb * corr0) / denum) / log10(2.0);
    // final correction
    for (int32_t ctb = 0; ctb < numCtb; ctb++) {
    int32_t lcuQp = frame->m_lcuQps[ctb] + corr;
    lcuQp = Saturate(0, 51, lcuQp);
    frame->m_lcuQps[ctb] = lcuQp;
    }
    }
    */
    //WriteDQpMap(frame, par);
}

#if 0
int32_t rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    int32_t numRows = rowsInRegion;
                    if(((int32_t)(region_row * rowsInRegion) + numRows)*SIZE_BLK_LA > frame->height)
                            numRows = (frame->height - (region_row * rowsInRegion)*SIZE_BLK_LA) / SIZE_BLK_LA;
#endif

namespace {
    __m128i LoadAndSum4(int32_t *p, int32_t pitch, int32_t x, int32_t y) {
        return _mm_hadd_epi32(_mm_add_epi32(_mm_loadu_si128((__m128i *)(p+(x+0)+(y+0)*pitch)),
                                            _mm_loadu_si128((__m128i *)(p+(x+0)+(y+1)*pitch))),
                              _mm_add_epi32(_mm_loadu_si128((__m128i *)(p+(x+4)+(y+0)*pitch)),
                                            _mm_loadu_si128((__m128i *)(p+(x+4)+(y+1)*pitch))));
    }

    void SumUp(int32_t *out, int32_t pitchOut, const int32_t *in, int32_t pitchIn, uint32_t Width, uint32_t Height, int32_t shift) {
        for (uint32_t y = 0; y < ((Height+(1<<shift)-1)>>shift); y++) {
            for (uint32_t x = 0; x < ((Width+(1<<shift)-1)>>shift); x++)
                out[y*pitchOut+x] = in[(2*y)*pitchIn+(2*x)]+in[(2*y)*pitchIn+(2*x+1)]+in[(2*y+1)*pitchIn+(2*x)]+in[(2*y+1)*pitchIn+(2*x+1)];
        }
    }
};

mfxStatus Lookahead::Execute(ThreadingTask& task)
{
    int32_t frameOrder = task.poc;
    if (frameOrder < 0)
        return MFX_ERR_NONE;

    ThreadingTaskSpecifier action = task.action;
    uint32_t region_row = task.row;
    //uint32_t region_col = task.col;
    FrameIter begin = m_inputQueue.begin();
    FrameIter end   = m_inputQueue.end();

    // fix for interlace processing to keep old-style (pair)
    Frame* in[2] = {NULL, NULL};
    {
        FrameIter it = std::find_if(begin, end, isEqual(frameOrder));
        in[0] = (it == end) ? NULL : *it;

        if ((m_videoParam.picStruct != MFX_PICSTRUCT_PROGRESSIVE) && in[0]) {
            it++;
            in[1] = (it == end) ? NULL : *it;
        }
    }

    int32_t fieldCount = (m_videoParam.picStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 2;
    uint8_t useLowres = m_videoParam.LowresFactor;

    switch (action) {
    case TT_PREENC_START:
        {
            // do this stage only once per frame
            if ((m_lastAcceptedFrame[0] && in[0]) && (m_lastAcceptedFrame[0]->m_frameOrder == in[0]->m_frameOrder)) {
                break;
            }

            for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {

                if (in[fieldNum] && (useLowres || m_videoParam.SceneCut)) {
                    if (in[fieldNum]->m_bitDepthLuma == 8)
                        Scale<uint8_t>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres);
                    else
                        Scale<uint16_t>(in[fieldNum]->m_origin, in[fieldNum]->m_lowres);

                    if (m_videoParam.DeltaQpMode || m_videoParam.AnalyzeCmplx)
                        PadRectLuma(*in[fieldNum]->m_lowres, m_videoParam.fourcc, 0, 0, in[fieldNum]->m_lowres->width, in[fieldNum]->m_lowres->height);
                }
                if (m_videoParam.SceneCut) {
                    //int32_t updateGop = 1;
                    //int32_t updateState = 1;
                    //DetectSceneCut(begin, end, in[fieldNum], updateGop, updateState);
                    AnalyzeSceneCut_AndUpdateState_Atul(in[fieldNum]);
                }
                m_lastAcceptedFrame[fieldNum] = in[fieldNum];
            }

            break;
        }


    case TT_PREENC_ROUTINE:
        {

            if (in[0] && in[0]->m_frameOrder > 0 &&
                (m_videoParam.AnalyzeCmplx || (m_videoParam.DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL)) )) // PAQ/CAL disable in encOrder
            {
                for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame* curr = in[fieldNum];
                    FrameIter it = std::find_if(begin, end, isEqual(curr->m_frameOrder));
                    if (it == begin || it == end) {
                        it = end;
                    } else {
                        it--;
                    }

                    // ME (prev, curr)
                    if (it != end) {
                        Frame* prev = (*it);
                        FrameData* frame = useLowres ? curr->m_lowres : curr->m_origin;
                        Statistics* stat = curr->m_stats[ useLowres ? 1 : 0 ];
                        FrameData* framePrev = useLowres ? prev->m_lowres : prev->m_origin;

                        int32_t rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                        int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                        for (int32_t i=0; i<numActiveRows; i++) {
                            DoInterAnalysis_OneRow(frame, NULL, framePrev, &stat->m_interSad[0], &stat->m_interSatd[0],
                                &stat->m_mv[0], 0, 0, m_videoParam.LowresFactor, curr->m_bitDepthLuma, region_row * rowsInRegion + i,
                                m_videoParam.FullresMetrics ? curr->m_origin : NULL,
                                m_videoParam.FullresMetrics ? prev->m_origin : NULL);
                        }
#ifndef LOW_COMPLX_PAQ
                        if ((m_videoParam.DeltaQpMode & (AMT_DQP_PAQ|AMT_DQP_CAL)) && useLowres) { // need refine for _Original_ resolution
                            FrameData* frame = curr->m_origin;
                            Statistics* originStat = curr->m_stats[0];
                            Statistics* lowresStat = curr->m_stats[1];
                            FrameData* framePrev = prev->m_origin;

                            int32_t rowsInRegion = m_originRowsInRegion;
                            int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);

                            for (int32_t i=0; i<numActiveRows; i++) {
                                DoInterAnalysis_OneRow(frame, lowresStat, framePrev, &originStat->m_interSad[0], NULL, &originStat->m_mv[0], 1, 0, m_videoParam.LowresFactor, curr->m_bitDepthLuma, region_row * rowsInRegion + i);//fixed-NG
                            }
                        }
#endif
                    }
#ifndef LOW_COMPLX_PAQ
                    // ME (prevRef <-  currRef ->  nextRef)
                    if (m_videoParam.DeltaQpMode & AMT_DQP_PAQ) {
                        int32_t frameOrderCentral = curr->m_frameOrder - m_bufferingPaq;
                        bool doAnalysis = (frameOrderCentral >= 0);

                        if (doAnalysis) {
                            FrameIter curr = std::find_if(begin, end, isEqual(frameOrderCentral));
                            uint32_t frameType = (*curr)->m_picCodeType;
                            bool isRef = (frameType == MFX_FRAMETYPE_P || frameType == MFX_FRAMETYPE_I);
                            if (isRef) {
                                Frame* prevP = GetPrevAnchor(begin, end, *curr);
                                Frame* nextP = GetNextAnchor(curr, end);
                                DoPDistInterAnalysis_OneRow(*curr, prevP, nextP, region_row, m_lowresRowsInRegion, m_originRowsInRegion, m_videoParam.LowresFactor, m_videoParam.FullresMetrics);
                            }
                        }
                    }
#endif
                } // foreach fieldNum
            } else {
                for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame* curr = in[fieldNum];
                    Statistics* stat = curr->m_stats[ useLowres ? 1 : 0 ];
                    FrameData* frame = useLowres ? curr->m_lowres : curr->m_origin;
                    int32_t rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);
                    int32_t picWidthInBlks = (frame->width + SIZE_BLK_LA - 1) / SIZE_BLK_LA;
                    int32_t fPos = (region_row * rowsInRegion) * picWidthInBlks; // [fPos, lPos)
                    int32_t lPos = fPos + numActiveRows*picWidthInBlks;
                    std::fill(stat->m_interSad.begin()+fPos, stat->m_interSad.begin()+lPos, std::numeric_limits<int32_t>::max());
                    std::fill(stat->m_interSatd.begin()+fPos, stat->m_interSatd.begin()+lPos, std::numeric_limits<int32_t>::max());
                }
            }

            // INTRA:  optional ORIGINAL / LOWRES
            if (in && m_videoParam.AnalyzeCmplx) {
                for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    FrameData* frame = useLowres ? in[fieldNum]->m_lowres : in[fieldNum]->m_origin;
                    Statistics* stat = in[fieldNum]->m_stats[useLowres ? 1 : 0];
                    int32_t rowsInRegion = useLowres ? m_lowresRowsInRegion : m_originRowsInRegion;
                    int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, frame->height);
                    int32_t size = SIZE_BLK_LA;
                    int32_t modes = 1;
                    if(useLowres && m_videoParam.FullresMetrics) {
                        frame = in[fieldNum]->m_origin;
                        size = (SIZE_BLK_LA)<<m_videoParam.LowresFactor;
                        modes = 0;
                    }
                    if (in[fieldNum]->m_bitDepthLuma == 8) {
                        for (int32_t i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<uint8_t>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i, size, modes);
                    } else {
                        for (int32_t i=0; i<numActiveRows; i++) DoIntraAnalysis_OneRow<uint16_t>(frame, &stat->m_intraSatd[0], region_row * rowsInRegion + i, size, modes);
                    }
                }
            }


            // RsCs: only ORIGINAL
            if (in) {
                int32_t rowsInRegion = m_originRowsInRegion;
                int32_t numActiveRows = GetNumActiveRows(region_row, rowsInRegion, in[0]->m_origin->height);

                for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame* curr = in[fieldNum];
                    for(int32_t i=0;i<numActiveRows;i++) {
                        (curr->m_bitDepthLuma == 8)
                            ? CalcRsCs_OneRow<uint8_t>(curr->m_origin, curr->m_stats[0], m_videoParam, region_row * rowsInRegion + i)
                            : CalcRsCs_OneRow<uint16_t>(curr->m_origin, curr->m_stats[0], m_videoParam, region_row * rowsInRegion + i);
                    }
                }
            }
            break;
        }


    case TT_PREENC_END:
        {
            if (in) {
                for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                    Frame *curr = in[fieldNum];
                    int32_t width4  = (m_videoParam.Width+63)&~63;
                    int32_t height4 = (m_videoParam.Height+63)&~63;
                    int32_t pitchRsCs4  = curr->m_stats[0]->m_pitchRsCs4;
                    int32_t pitchRsCs8  = curr->m_stats[0]->m_pitchRsCs4>>1;
                    int32_t pitchRsCs16 = curr->m_stats[0]->m_pitchRsCs4>>2;
                    int32_t pitchRsCs32 = curr->m_stats[0]->m_pitchRsCs4>>3;
                    int32_t pitchRsCs64 = curr->m_stats[0]->m_pitchRsCs4>>4;
                    int32_t *rs4  = curr->m_stats[0]->m_rs[0];
                    int32_t *rs8  = curr->m_stats[0]->m_rs[1];
                    int32_t *rs16 = curr->m_stats[0]->m_rs[2];
                    int32_t *rs32 = curr->m_stats[0]->m_rs[3];
                    int32_t *rs64 = curr->m_stats[0]->m_rs[4];
                    int32_t *cs4  = curr->m_stats[0]->m_cs[0];
                    int32_t *cs8  = curr->m_stats[0]->m_cs[1];
                    int32_t *cs16 = curr->m_stats[0]->m_cs[2];
                    int32_t *cs32 = curr->m_stats[0]->m_cs[3];
                    int32_t *cs64 = curr->m_stats[0]->m_cs[4];

                    SumUp(rs8,  pitchRsCs8,  rs4,  pitchRsCs4,  m_videoParam.Width, m_videoParam.Height, 3);
                    SumUp(rs16, pitchRsCs16, rs8,  pitchRsCs8,  m_videoParam.Width, m_videoParam.Height, 4);
                    SumUp(rs32, pitchRsCs32, rs16, pitchRsCs16, m_videoParam.Width, m_videoParam.Height, 5);
                    SumUp(rs64, pitchRsCs64, rs32, pitchRsCs32, m_videoParam.Width, m_videoParam.Height, 6);
                    SumUp(cs8,  pitchRsCs8,  cs4,  pitchRsCs4,  m_videoParam.Width, m_videoParam.Height, 3);
                    SumUp(cs16, pitchRsCs16, cs8,  pitchRsCs8,  m_videoParam.Width, m_videoParam.Height, 4);
                    SumUp(cs32, pitchRsCs32, cs16, pitchRsCs16, m_videoParam.Width, m_videoParam.Height, 5);
                    SumUp(cs64, pitchRsCs64, cs32, pitchRsCs32, m_videoParam.Width, m_videoParam.Height, 6);
                }
            }
            // syncpoint only (like accumulation & refCounter--), no real hard working here!!!
            for (int32_t fieldNum = 0; fieldNum < fieldCount; fieldNum++) {
                if (m_videoParam.AnalyzeCmplx) {
                    FrameIter it = std::find_if(begin, end, isEqual(in[fieldNum]->m_frameOrder));
                    if (it == begin || it == end) {
                        it = end;
                    } else {
                        it--;
                    }
                    if (it != end) (*it)->m_lookaheadRefCounter--;

                }
                if (m_videoParam.DeltaQpMode) {
                    AverageRsCs(in[fieldNum]);
                    if (m_videoParam.DeltaQpMode == AMT_DQP_CAQ) {
                        if (in[fieldNum]) in[fieldNum]->m_lookaheadRefCounter--;
                    } else { // & (PAQ | CAL)
                        int32_t frameOrderCentral = in[fieldNum]->m_frameOrder - m_bufferingPaq;
                        int32_t updateState = 1;
                        BuildQpMap(begin, end, frameOrderCentral, m_videoParam, updateState);
                    }
                }
            }
            break;
        }
    case TT_HUB:
        {
            break;
        }
    default:
        {
            assert(!"invalid lookahead action");
        }
    };

    //vm_interlocked_inc32(&m_task->m_numFinishedThreadingTasks);
    return MFX_ERR_NONE;

} //

//
// AMT SceneCut algorithm
//
enum {
        REF = 0,
        CUR = 1,

        PAD_V = 2,
        PAD_H = 2,
        LOWRES_W  = 112,
        LOWRES_H =  64,
        LOWRES_SIZE = LOWRES_W * LOWRES_H,
        LOWRES_PITCH = LOWRES_W + 2 *PAD_H,

        BLOCK_W = 8,
        BLOCK_H = 8,
        WIDTH_IN_BLOCKS  = LOWRES_W / BLOCK_W,
        HEIGHT_IN_BLOCKS = LOWRES_H / BLOCK_H,
        NUM_BLOCKS = (LOWRES_H / BLOCK_H) * (LOWRES_W / BLOCK_W),

        NUM_TSC = 10,
        NUM_SC  = 10
    };

#define NABS(a)           (((a)<0)?(-(a)):(a))

    int32_t SCDetectUF1(
        float diffMVdiffVal, float RsCsDiff,   float MVDiff,   float Rs,       float AFD,
        float CsDiff,        float diffTSC,    float TSC,      float gchDC,    float diffRsCsdiff,
        float posBalance,    float SC,         float TSCindex, float Scindex,  float Cs,
        float diffAFD,       float negBalance, float ssDCval,  float refDCval, float RsDiff);

    const AV1MV ZERO_MV = {0,0};

    void MotionRangeDeliveryF(int32_t xLoc, int32_t yLoc, int32_t *limitXleft, int32_t *limitXright, int32_t *limitYup, int32_t *limitYdown)
    {
        int32_t Extended_Height = LOWRES_H + 2 *PAD_V;
        int32_t locY = yLoc / ((3 * (16 / BLOCK_H)) / 2);
        int32_t locX = xLoc / ((3 * (16 / BLOCK_W)) / 2);
        *limitXleft = std::max(-16, -(xLoc * BLOCK_W) - PAD_H);
        *limitXright = std::min(15, LOWRES_PITCH - (((xLoc + 1) * BLOCK_W) + PAD_H));
        *limitYup = std::max(-12, -(yLoc * BLOCK_H) - PAD_V);
        *limitYdown = std::min(11, Extended_Height - (((yLoc + 1) * BLOCK_W) + PAD_V));
    }

    int32_t MVcalcSAD8x8(AV1MV MV, uint8_t* curY, uint8_t* refY, uint32_t *bestSAD, int32_t *distance)
    {
        int32_t preDist = (MV.mvx * MV.mvx) + (MV.mvy * MV.mvy);
        uint8_t* fRef = refY + MV.mvx + (MV.mvy * LOWRES_PITCH);
        uint32_t SAD = AV1PP::sad_general(curY, LOWRES_PITCH, fRef, LOWRES_PITCH, 1, 1);
        if ((SAD < *bestSAD) || ((SAD == *(bestSAD)) && *distance > preDist)) {
            *distance = preDist;
            *(bestSAD) = SAD;
            return 1;
        }
        return 0;
    }

    void SearchLimitsCalc(int32_t xLoc, int32_t yLoc, int32_t *limitXleft, int32_t *limitXright, int32_t *limitYup, int32_t *limitYdown, int32_t range, AV1MV mv)
    {
        int32_t Extended_Height = LOWRES_H + 2 *PAD_V;
        int32_t locX  = (xLoc * BLOCK_W) + PAD_H + mv.mvx;
        int32_t locY  = (yLoc * BLOCK_H) + PAD_V + mv.mvy;
        *limitXleft  = std::max(-locX,-range);
        *limitXright = std::min(LOWRES_PITCH - ((xLoc + 1) * BLOCK_W) - PAD_H - mv.mvx, range);
        *limitYup    = std::max(-locY,-range);
        *limitYdown  = std::min(Extended_Height - ((yLoc + 1) * BLOCK_H) - PAD_V - mv.mvy, range);
    }

    int32_t DistInt(AV1MV vector)
    {
        return (vector.mvx*vector.mvx) + (vector.mvy*vector.mvy);
    }

    void MVpropagationCheck(int32_t xLoc, int32_t yLoc, AV1MV *propagatedMV)
    {
        int32_t Extended_Height = LOWRES_H + 2 *PAD_V;
        int32_t
            left = (xLoc * BLOCK_W) + PAD_H,
            right = (LOWRES_PITCH - left - BLOCK_W),
            up = (yLoc * BLOCK_H) + PAD_V,
            down = (Extended_Height - up - BLOCK_H);

        if(propagatedMV->mvx < 0) {
            if(left + propagatedMV->mvx < 0)
                propagatedMV->mvx = -left;
        } else {
            if(right - propagatedMV->mvx < 0)
                propagatedMV->mvx = right;
        }

        if(propagatedMV->mvy < 0) {
            if(up + propagatedMV->mvy < 0)
                propagatedMV->mvy = -up;
        } else {
            if(down - propagatedMV->mvy < 0)
                propagatedMV->mvy = down;
        }
    }

    struct Rsad
    {
        uint32_t SAD;
        uint32_t distance;
        AV1MV BestMV;
    };

    #define EXTRANEIGHBORS

    int32_t HME_Low8x8fast(uint8_t *src, uint8_t *ref, AV1MV *srcMv, int32_t fPos, int32_t first, uint32_t accuracy, uint32_t* SAD)
    {
        AV1MV
            lineMV[2],
            tMV,
            ttMV,
            preMV = ZERO_MV,
            Nmv;
        int32_t
            limitXleft = 0,
            limitXright = 0,
            limitYup = 0,
            limitYdown = 0,
            direction[2],
            counter,
            lineSAD[2],
            distance = INT_MAX,
            plane = 0,
            bestPlane = 0,
            bestPlaneN = 0,
            zeroSAD = INT_MAX;

        uint32_t
            tl,
            *outSAD,
            bestSAD = INT_MAX;
        Rsad
            range;
        int32_t
            foundBetter = 0,//1,
            truneighbor = 0;//1;

        int32_t xLoc = (fPos % WIDTH_IN_BLOCKS);
        int32_t yLoc = (fPos / WIDTH_IN_BLOCKS);
        int32_t offset = (yLoc * LOWRES_PITCH * BLOCK_H) + (xLoc * BLOCK_W);
        uint8_t *objFrame = src + offset;
        uint8_t *refFrame = ref + offset;
        AV1MV *current = srcMv;
        outSAD = SAD;
        int32_t acc = (accuracy == 1) ? 1 : 4;

        if (first) {
            MVcalcSAD8x8(ZERO_MV, objFrame, refFrame, &bestSAD, &distance);
            zeroSAD = bestSAD;
            current[fPos] = ZERO_MV;
            outSAD[fPos] = bestSAD;
            if (zeroSAD == 0)
                return zeroSAD;
            MotionRangeDeliveryF(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown);
        }

        range.SAD = outSAD[fPos];
        range.BestMV = current[fPos];
        range.distance = INT_MAX;

        direction[0] = 0;
        lineMV[0].mvx = 0;
        lineMV[0].mvy = 0;
        {
            uint8_t *ps = objFrame;
            uint8_t *pr = refFrame + (limitYup * LOWRES_PITCH) + limitXleft;
            int32_t xrange = limitXright - limitXleft + 1,
                yrange = limitYdown - limitYup + 1,
                bX = 0,
                bY = 0;
            uint32_t bSAD = INT_MAX;
            AV1PP::search_best_block8x8(ps, pr, LOWRES_PITCH, xrange, yrange, &bSAD, &bX, &bY);
            if (bSAD < range.SAD) {
                range.SAD = bSAD;
                range.BestMV.mvx = bX + limitXleft;
                range.BestMV.mvy = bY + limitYup;
                range.distance = DistInt(range.BestMV);
            }
        }
        outSAD[fPos] = range.SAD;
        current[fPos] = range.BestMV;

        /*ExtraNeighbors*/
#ifdef EXTRANEIGHBORS
        if (fPos > (WIDTH_IN_BLOCKS * 3)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 3)].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 3)].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos > (WIDTH_IN_BLOCKS * 2)) && (xLoc > 0)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2) - 1].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2) - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (fPos > (WIDTH_IN_BLOCKS * 2)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2)].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2)].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos > (WIDTH_IN_BLOCKS * 2)) && (xLoc < WIDTH_IN_BLOCKS - 1)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - (WIDTH_IN_BLOCKS * 2) + 1].mvx;
            Nmv.mvy = current[fPos - (WIDTH_IN_BLOCKS * 2) + 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
#endif
        if ((fPos > WIDTH_IN_BLOCKS) && (xLoc > 0)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS - 1].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (fPos > WIDTH_IN_BLOCKS) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if ((fPos> WIDTH_IN_BLOCKS) && (xLoc < WIDTH_IN_BLOCKS - 1)) {
            tl = range.SAD;
            Nmv.mvx = current[fPos - WIDTH_IN_BLOCKS + 1].mvx;
            Nmv.mvy = current[fPos - WIDTH_IN_BLOCKS + 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (xLoc > 0) {
            tl = range.SAD;
            distance = INT_MAX;
            Nmv.mvx = current[fPos - 1].mvx;
            Nmv.mvy = current[fPos - 1].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (xLoc > 1) {
            tl = range.SAD;
            distance = INT_MAX;
            Nmv.mvx = current[fPos - 2].mvx;
            Nmv.mvy = current[fPos - 2].mvy;
            distance = DistInt(Nmv);
            MVpropagationCheck(xLoc, yLoc, &Nmv);
            if (MVcalcSAD8x8(Nmv, objFrame, refFrame, &tl, &distance)) {
                range.BestMV = Nmv;
                range.SAD = tl;
                foundBetter = 0;
                truneighbor = 1;
                bestPlaneN = plane;
            }
        }
        if (truneighbor) {
            ttMV = range.BestMV;
            bestSAD = range.SAD;
            SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 1, range.BestMV);
            for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy++) {
                for (tMV.mvx = limitXleft; tMV.mvx <= limitXright; tMV.mvx++) {
                    preMV.mvx = tMV.mvx + ttMV.mvx;
                    preMV.mvy = tMV.mvy + ttMV.mvy;
                    foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame, &bestSAD, &distance);
                    if (foundBetter) {
                        range.BestMV = preMV;
                        range.SAD = bestSAD;
                        bestPlaneN = plane;
                        foundBetter = 0;
                    }
                }
            }

            if (truneighbor) {
                outSAD[fPos] = range.SAD;
                current[fPos] = range.BestMV;
                truneighbor = 0;
                bestPlane = bestPlaneN;
            }
        }
        range.SAD = outSAD[fPos];
        /*Zero search*/
        if (!first) {
            SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 16/*32*/, ZERO_MV);
            counter = 0;
            for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy += 2) {
                lineSAD[0] = INT_MAX;
                for (tMV.mvx = limitXleft + ((counter % 2) * 2); tMV.mvx <= limitXright; tMV.mvx += 4) {
                    ttMV.mvx = tMV.mvx + ZERO_MV.mvx;
                    ttMV.mvy = tMV.mvy + ZERO_MV.mvy;
                    bestSAD = range.SAD;
                    distance = range.distance;
                    foundBetter = MVcalcSAD8x8(ttMV, objFrame, refFrame, &bestSAD, &distance);
                    if (foundBetter) {
                        range.BestMV = ttMV;
                        range.SAD = bestSAD;
                        range.distance = distance;
                        foundBetter = 0;
                        bestPlane = 0;
                    }
                }
                counter++;
            }
        }
        ttMV = range.BestMV;
        SearchLimitsCalc(xLoc, yLoc, &limitXleft, &limitXright, &limitYup, &limitYdown, 1, range.BestMV);
        for (tMV.mvy = limitYup; tMV.mvy <= limitYdown; tMV.mvy++) {
            for (tMV.mvx = limitXleft; tMV.mvx <= limitXright; tMV.mvx++) {
                preMV.mvx = tMV.mvx + ttMV.mvx;
                preMV.mvy = tMV.mvy + ttMV.mvy;
                foundBetter = MVcalcSAD8x8(preMV, objFrame, refFrame, &bestSAD, &distance);
                if (foundBetter) {
                    range.BestMV = preMV;
                    range.SAD = bestSAD;
                    range.distance = distance;
                    bestPlane = 0;
                    foundBetter = 0;
                }
            }
        }
        outSAD[fPos] = range.SAD;
        current[fPos] = range.BestMV;

        return(zeroSAD);
    }

    static int32_t PDISTTbl2[/*NumTSC*NumSC*/] =
    {
        2, 3, 3, 4, 4, 5, 5, 5, 5, 5,
        2, 2, 3, 3, 4, 4, 5, 5, 5, 5,
        1, 2, 2, 3, 3, 3, 4, 4, 5, 5,
        1, 1, 2, 2, 3, 3, 3, 4, 4, 5,
        1, 1, 2, 2, 3, 3, 3, 3, 3, 4,
        1, 1, 1, 2, 2, 3, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 3, 3, 3, 3,
        1, 1, 1, 1, 2, 2, 2, 3, 3, 3,
        1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };


    const float FLOAT_MAX  =       2241178.0;

    static float
        lmt_sc2[/*NumSC*/] = { 4.0, 9.0, 15.0, 23.0, 32.0, 42.0, 53.0, 65.0, 78.0, FLOAT_MAX },             // lower limit of SFM(Rs,Cs) range for spatial classification
        // 9 ranges of SC are: 0 0-4, 1 4-9, 2 9-15, 3 15-23, 4 23-32, 5 32-44, 6 42-53, 7 53-65, 8 65-78, 9 78->??
        lmt_tsc2[/*NumTSC*/] = { 0.75, 1.5, 2.25, 3.0, 4.0, 5.0, 6.0, 7.5, 9.25, FLOAT_MAX };               // lower limit of AFD
        // 8 ranges of TSC (based on FD) are:0 0-0.75 1 0.75-1.5, 2 1.5-2.25. 3 2.25-3, 4 3-4, 5 4-5, 6 5-6, 7 6-7.5, 8 7.5-9.25, 9 9.25->??

    void SceneStats::Create(const AllocInfo &allocInfo)
    {
        for (int32_t idx = 0; idx < 2; idx++) {
            memset(data, 0, sizeof(data)/sizeof(data[0]));
            int32_t initialPoint = (LOWRES_PITCH * PAD_V) + PAD_H;
            Y = data + initialPoint;
        }
    }

    template <class T> void SubSampleImage(const T *pSrc, int32_t srcWidth, int32_t srcHeight, int32_t srcPitch, uint8_t *pDst, int32_t dstWidth, int32_t dstHeight, int32_t dstPitch)
    {
        int32_t shift = 2*(sizeof(T) - 1);

        int32_t dstSSHstep = srcWidth / dstWidth;
        int32_t dstSSVstep = srcHeight / dstHeight;

        //No need for exact subsampling, approximation is good enough for analysis.
        for(int32_t i = 0, j = 0 ; i < dstHeight; i++, j += dstSSVstep) {
            for(int32_t k = 0, h = 0 ; k < dstWidth; k++, h += dstSSHstep) {
                pDst[(i * dstPitch) + k] = (uint8_t)(pSrc[(j * srcPitch) + h] >> shift);
            }
        }
    }

    int32_t TableLookUp(int32_t limit, float *table, float comparisonValue)
    {
        for (int32_t pos = 0; pos < limit; pos++) {
            if (comparisonValue < table[pos])
                return pos;
        }
        return limit;
    }

    int32_t ShotDetect(SceneStats& input, SceneStats& ref, int32_t* histogram, int64_t ssDCint, int64_t refDCint)
    {
        float RsDiff, CsDiff;
        {
            float* objRs = input.Rs;
            float* objCs = input.Cs;
            float* refRs = ref.Rs;
            float* refCs = ref.Cs;
            int32_t len = (2*WIDTH_IN_BLOCKS) * (2*HEIGHT_IN_BLOCKS); // blocks in 8x8 but RsCs in 4x4
            AV1PP::compute_rscs_diff(objRs, objCs, refRs, refCs, len, &RsDiff, &CsDiff);
            RsDiff /= NUM_BLOCKS * (2 / (8 / BLOCK_H)) * (2 / (8 / BLOCK_W));
            CsDiff /= NUM_BLOCKS * (2 / (8 / BLOCK_H)) * (2 / (8 / BLOCK_W));
            input.RsCsDiff = (float)sqrt((RsDiff*RsDiff) + (CsDiff*CsDiff));
        }

        float ssDCval  = ssDCint * (1.0 / LOWRES_SIZE);
        float refDCval = refDCint * (1.0 / LOWRES_SIZE);
        float gchDC = NABS(ssDCval - refDCval);
        float posBalance = (float)(histogram[3] + histogram[4]);
        float negBalance = (float)(histogram[0] + histogram[1]);
        posBalance -= negBalance;
        posBalance = NABS(posBalance);
        float histTOT = (float)(histogram[0] + histogram[1] + histogram[2] + histogram[3] + histogram[4]);
        posBalance /= histTOT;
        negBalance /= histTOT;

        float diffAFD = input.AFD - ref.AFD;
        float diffTSC = input.TSC - ref.TSC;
        float diffRsCsDiff = input.RsCsDiff - ref.RsCsDiff;
        float diffMVdiffVal = input.MVdiffVal - ref.MVdiffVal;
        int32_t TSCindex = TableLookUp(NUM_TSC, lmt_tsc2, input.TSC);

        float SC = (float)sqrt(( input.avgRs * input.avgRs) + (input.avgCs * input.avgCs));
        int32_t SCindex  = TableLookUp(NUM_SC, lmt_sc2,  SC);

        int32_t Schg = SCDetectUF1(diffMVdiffVal, input.RsCsDiff, input.MVdiffVal, input.avgRs, input.AFD, CsDiff, diffTSC, input.TSC, gchDC, diffRsCsDiff, posBalance, SC, TSCindex, SCindex, input.avgCs, diffAFD, negBalance, ssDCval, refDCval, RsDiff);

        return Schg;
    }

    void MotionAnalysis(uint8_t *src, AV1MV *srcMv, uint8_t *ref, AV1MV *refMv, float *TSC, float *AFD, float *MVdiffVal)
    {
        uint32_t valNoM = 0, valb = 0;
        uint32_t SAD[NUM_BLOCKS];

        *MVdiffVal = 0;
        for (int32_t fPos = 0; fPos < NUM_BLOCKS; fPos++) {
            valNoM += HME_Low8x8fast(src, ref, srcMv, fPos, 1, 1, SAD);
            valb += SAD[fPos];
            *MVdiffVal += (srcMv[fPos].mvx - refMv[fPos].mvx) * (srcMv[fPos].mvx - refMv[fPos].mvx);
            *MVdiffVal += (srcMv[fPos].mvy - refMv[fPos].mvy) * (srcMv[fPos].mvy - refMv[fPos].mvy);
        }

        *TSC = (float)valb / (float)LOWRES_SIZE;
        *AFD = (float)valNoM / (float)LOWRES_SIZE;
        *MVdiffVal = (float)((float)*MVdiffVal / (float)NUM_BLOCKS);
    }

    int32_t AV1Enc::DetectSceneCut_AMT(Frame* input, Frame* prev)
    {
        {
            int32_t srcWidth  = input->m_origin->width;
            int32_t srcHeight = input->m_origin->height;
            int32_t srcPitch  = input->m_origin->pitch_luma_pix;

            uint8_t* lowres = input->m_sceneStats->Y;
            int32_t dstWidth  = LOWRES_W;
            int32_t dstHeight = LOWRES_H;
            int32_t dstPitch  = LOWRES_PITCH;

            if (input->m_bitDepthLuma == 8) {
                uint8_t *origin = input->m_origin->y;
                SubSampleImage(origin, srcWidth, srcHeight, srcPitch, lowres, dstWidth, dstHeight, dstPitch);
            } else {
                uint16_t *origin = (uint16_t*)input->m_origin->y;
                SubSampleImage(origin, srcWidth, srcHeight, srcPitch, lowres, dstWidth, dstHeight, dstPitch);
            }
        }

        SceneStats* stats[2] = {prev ? prev->m_sceneStats : input->m_sceneStats, input->m_sceneStats};
        uint8_t  *src  = stats[CUR]->Y;
        AV1MV *srcMv =  stats[CUR]->mv;
        uint8_t  *ref  = stats[REF]->Y;
        AV1MV *refMv =  stats[REF]->mv;
        {
            const int32_t BLOCK_SIZE = 4;
            const int32_t hblocks = LOWRES_H / BLOCK_SIZE;
            const int32_t wblocks = LOWRES_W / BLOCK_SIZE;
            const int32_t len = hblocks * wblocks;
            SceneStats* data = stats[CUR];

            AV1PP::compute_rscs_4x4(src, LOWRES_PITCH, wblocks, hblocks, data->Rs, data->Cs);

            ippsSum_32f(data->Rs, len, &data->avgRs, ippAlgHintFast);
            data->avgRs = (float)sqrt(data->avgRs / len);
            ippsSqrt_32f_I(data->Rs, len);
            ippsSum_32f(data->Cs, len, &data->avgCs, ippAlgHintFast);
            data->avgCs = (float)sqrt(data->avgCs / len);
            ippsSqrt_32f_I(data->Cs, len);
        }

        int32_t Schg = 0;
        if (prev == NULL) {
            stats[CUR]->TSC = 0;
            stats[CUR]->AFD = 0;
            stats[CUR]->MVdiffVal = 0;
            stats[CUR]->RsCsDiff = 0;
        } else {
            MotionAnalysis(src, srcMv, ref, refMv, &stats[CUR]->TSC, &stats[CUR]->AFD, &stats[CUR]->MVdiffVal);

            long long ssDCint, refDCint;
            int32_t histogram[5];
            AV1PP::image_diff_histogram(src, ref, LOWRES_PITCH, LOWRES_W, LOWRES_H, histogram, &ssDCint, &refDCint);
            Schg = ShotDetect(*stats[CUR], *stats[REF], histogram, ssDCint, refDCint);
        }

        return Schg;
    }

    int32_t SCDetectUF1(
        float diffMVdiffVal, float RsCsDiff,   float MVDiff,   float Rs,       float AFD,
        float CsDiff,        float diffTSC,    float TSC,      float gchDC,    float diffRsCsdiff,
        float posBalance,    float SC,         float TSCindex, float Scindex,  float Cs,
        float diffAFD,       float negBalance, float ssDCval,  float refDCval, float RsDiff)
    {
        if (diffMVdiffVal <= 100.429) {
            if (diffAFD <= 12.187) {
                if (diffRsCsdiff <= 160.697) {
                    if (diffTSC <= 2.631) {
                        if (MVDiff <= 262.598) {
                            return 0;
                        } else if (MVDiff > 262.598) {
                            if (Scindex <= 2) {
                                if (RsCsDiff <= 258.302) {
                                    return 0;
                                } else if (RsCsDiff > 258.302) {
                                    if (AFD <= 29.182) {
                                        return 1;
                                    } else if (AFD > 29.182) {
                                        return 0;
                                    }
                                }
                            } else if (Scindex > 2) {
                                return 0;
                            }
                        }
                    } else if (diffTSC > 2.631) {
                        if (negBalance <= 0.317) {
                            return 0;
                        } else if (negBalance > 0.317) {
                            if (Cs <= 18.718) {
                                if (AFD <= 10.086) {
                                    return 1;
                                } else if (AFD > 10.086) {
                                    if (Scindex <= 3) {
                                        if (SC <= 20.257) {
                                            return 0;
                                        } else if (SC > 20.257) {
                                            if (AFD <= 23.616) {
                                                return 0;
                                            } else if (AFD > 23.616) {
                                                return 1;
                                            }
                                        }
                                    } else if (Scindex > 3) {
                                        return 0;
                                    }
                                }
                            }
                            else if (Cs > 18.718)
                            {
                                return 0;
                            }
                        }
                    }
                }
                else if (diffRsCsdiff > 160.697) {
                    if (Scindex <= 3) {
                        if (Rs <= 11.603) {
                            return 1;
                        } else if (Rs > 11.603) {
                            if (MVDiff <= 239.661) {
                                return 0;
                            } else if (MVDiff > 239.661) {
                                if (Rs <= 16.197) {
                                    return 1;
                                } else if (Rs > 16.197) {
                                    return 0;
                                }
                            }
                        }
                    } else if (Scindex > 3) {
                        return 0;
                    }
                }
            } else if (diffAFD > 12.187) {
                if (RsCsDiff <= 574.733) {
                    if (negBalance <= 0.304) {
                        if (MVDiff <= 226.839) {
                            return 0;
                        } else if (MVDiff > 226.839) {
                            if (diffRsCsdiff <= 179.802) {
                                return 0;
                            } else if (diffRsCsdiff > 179.802) {
                                if (RsDiff <= 187.951) {
                                    return 1;
                                } else if (RsDiff > 187.951) {
                                    return 0;
                                }
                            }
                        }
                    } else if (negBalance > 0.304) {
                        if (refDCval <= 51.521) {
                            return 1;
                        } else if (refDCval > 51.521) {
                            if (diffTSC <= 9.661) {
                                if (MVDiff <= 207.214) {
                                    return 0;
                                } else if (MVDiff > 207.214) {
                                    if (ssDCval <= 41.36) {
                                        return 1;
                                    } else if (ssDCval > 41.36) {
                                        if (diffRsCsdiff <= 330.407) {
                                            return 0;
                                        } else if (diffRsCsdiff > 330.407) {
                                            return 1;
                                        }
                                    }
                                }
                            } else if (diffTSC > 9.661) {
                                if (Scindex <= 5) {
                                    if (posBalance <= 0.661) {
                                        if (diffRsCsdiff <= 45.975) {
                                            return 0;
                                        } else if (diffRsCsdiff > 45.975) {
                                            if (Scindex <= 4) {
                                                return 1;
                                            } else if (Scindex > 4) {
                                                if (ssDCval <= 125.414) {
                                                    return 1;
                                                } else if (ssDCval > 125.414) {
                                                    return 0;
                                                }
                                            }
                                        }
                                    } else if (posBalance > 0.661) {
                                        return 0;
                                    }
                                } else if (Scindex > 5) {
                                    return 0;
                                }
                            }
                        }
                    }
                } else if (RsCsDiff > 574.733) {
                    if (diffRsCsdiff <= 299.149) {
                        return 0;
                    } else if (diffRsCsdiff > 299.149) {
                        if (SC <= 39.857) {
                            if (MVDiff <= 325.304) {
                                if (Cs <= 11.102) {
                                    return 0;
                                } else if (Cs > 11.102) {
                                    return 1;
                                }
                            } else if (MVDiff > 325.304) {
                                return 0;
                            }
                        } else if (SC > 39.857) {
                            if (TSC <= 24.941) {
                                return 0;
                            } else if (TSC > 24.941) {
                                if (diffRsCsdiff <= 635.748) {
                                    return 0;
                                } else if (diffRsCsdiff > 635.748) {
                                    return 1;
                                }
                            }
                        }
                    }
                }
            }
        } else if (diffMVdiffVal > 100.429) {
            if (diffAFD <= 9.929) {
                if (diffAFD <= 7.126) {
                    if (diffRsCsdiff <= 96.456) {
                        return 0;
                    } else if (diffRsCsdiff > 96.456) {
                        if (Rs <= 24.984) {
                            if (diffTSC <= -0.375) {
                                if (ssDCval <= 41.813) {
                                    return 1;
                                } else if (ssDCval > 41.813) {
                                    return 0;
                                }
                            } else if (diffTSC > -0.375) {
                                return 1;
                            }
                        } else if (Rs > 24.984) {
                            return 0;
                        }
                    }
                } else if (diffAFD > 7.126) {
                    if (refDCval <= 62.036) {
                        if (posBalance <= 0.545) {
                            return 1;
                        } else if (posBalance > 0.545) {
                            return 0;
                        }
                    } else if (refDCval > 62.036) {
                        return 0;
                    }
                }
            } else if (diffAFD > 9.929) {
                if (negBalance <= 0.114) {
                    if (diffAFD <= 60.112) {
                        if (SC <= 21.607) {
                            if (diffMVdiffVal <= 170.955) {
                                return 0;
                            } else if (diffMVdiffVal > 170.955) {
                                return 1;
                            }
                        } else if (SC > 21.607) {
                            if (CsDiff <= 394.574) {
                                return 0;
                            } else if (CsDiff > 394.574) {
                                if (MVDiff <= 311.679) {
                                    return 0;
                                } else if (MVDiff > 311.679) {
                                    return 1;
                                }
                            }
                        }
                    } else if (diffAFD > 60.112) {
                        if (negBalance <= 0.021) {
                            if (Cs <= 25.091) {
                                if (ssDCval <= 161.319) {
                                    return 1;
                                } else if (ssDCval > 161.319) {
                                    return 0;
                                }
                            } else if (Cs > 25.091) {
                                return 0;
                            }
                        } else if (negBalance > 0.021) {
                            if (CsDiff <= 118.864) {
                                return 0;
                            } else if (CsDiff > 118.864) {
                                return 1;
                            }
                        }
                    }
                } else if (negBalance > 0.114) {
                    if (diffRsCsdiff <= 73.957) {
                        if (refDCval <= 52.698) {
                            return 1;
                        } else if (refDCval > 52.698) {
                            if (CsDiff <= 292.448) {
                                if (gchDC <= 17.615) {
                                    if (diffRsCsdiff <= 31.82) {
                                        return 0;
                                    } else if (diffRsCsdiff > 31.82) {
                                        if (TSCindex <= 8) {
                                            return 1;
                                        } else if (TSCindex > 8) {
                                            if (diffTSC <= 9.277) {
                                                return 0;
                                            } else if (diffTSC > 9.277) {
                                                return 1;
                                            }
                                        }
                                    }
                                } else if (gchDC > 17.615) {
                                    return 0;
                                }
                            } else if (CsDiff > 292.448) {
                                return 1;
                            }
                        }
                    } else if (diffRsCsdiff > 73.957) {
                        if (MVDiff <= 327.545) {
                            if (TSCindex <= 8) {
                                if (Scindex <= 2) {
                                    return 1;
                                } else if (Scindex > 2) {
                                    if (Cs <= 15.328) {
                                        if (gchDC <= 11.785) {
                                            return 1;
                                        } else if (gchDC > 11.785) {
                                            return 0;
                                        }
                                    } else if (Cs > 15.328) {
                                        return 0;
                                    }
                                }
                            } else if (TSCindex > 8) {
                                if (Scindex <= 3) {
                                    if (posBalance <= 0.355) {
                                        return 1;
                                    } else if (posBalance > 0.355) {
                                        if (Scindex <= 2) {
                                            return 1;
                                        } else if (Scindex > 2) {
                                            if (CsDiff <= 190.806) {
                                                if (AFD <= 30.279) {
                                                    return 1;
                                                } else if (AFD > 30.279) {
                                                    if (Rs <= 11.233) {
                                                        return 0;
                                                    } else if (Rs > 11.233) {
                                                        if (RsDiff <= 135.624) {
                                                            if (Rs <= 13.737) {
                                                                return 1;
                                                            } else if (Rs > 13.737) {
                                                                return 0;
                                                            }
                                                        } else if (RsDiff > 135.624) {
                                                            return 1;
                                                        }
                                                    }
                                                }
                                            } else if (CsDiff > 190.806) {
                                                return 1;
                                            }
                                        }
                                    }
                                } else if (Scindex > 3) {
                                    if (diffMVdiffVal <= 132.973) {
                                        if (Scindex <= 4) {
                                            return 1;
                                        } else if (Scindex > 4) {
                                            if (TSC <= 18.753) {
                                                return 0;
                                            } else if (TSC > 18.753) {
                                                return 1;
                                            }
                                        }
                                    } else if (diffMVdiffVal > 132.973) {
                                        return 1;
                                    }
                                }
                            }
                        } else if (MVDiff > 327.545) {
                            if (CsDiff <= 369.087) {
                                if (Rs <= 18.379) {
                                    if (diffMVdiffVal <= 267.732) {
                                        if (Cs <= 10.464) {
                                            return 1;
                                        } else if (Cs > 10.464) {
                                            return 0;
                                        }
                                    } else if (diffMVdiffVal > 267.732) {
                                        return 1;
                                    }
                                } else if (Rs > 18.379) {
                                    if (diffAFD <= 35.587) {
                                        return 0;
                                    } else if (diffAFD > 35.587) {
                                        if (CsDiff <= 190.153) {
                                            return 0;
                                        } else if (CsDiff > 190.153) {
                                            return 1;
                                        }
                                    }
                                }
                            } else if (CsDiff > 369.087) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
/* EOF */