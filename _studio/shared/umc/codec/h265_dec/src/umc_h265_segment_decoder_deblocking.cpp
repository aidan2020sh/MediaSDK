/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder.h"
#include "umc_h265_frame_info.h"

#define VERT_FILT 0
#define HOR_FILT  1

namespace UMC_HEVC_DECODER
{

static Ipp32s tcTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  3,
    3,  3,  3,  4,  4,  4,  5,  5,  6,  6,  7,  8,  9, 10, 11, 13,
    14, 16, 18, 20, 22, 24
};

static Ipp32s betaTable[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24,
    26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56,
    58, 60, 62, 64
};

#define   QpUV(iQpY)  ( ((iQpY) < 0) ? (iQpY) : (((iQpY) > 57) ? ((iQpY)-6) : g_ChromaScale[(iQpY)]) )

void H265SegmentDecoder::DeblockSegment(H265Task & task)
{

    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(task.m_iFirstMB);
    H265Slice* pSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(curLCU->m_SliceIdx);
    Ipp32s i;

    if (NULL == pSlice)
        return;

    for (i = task.m_iFirstMB; i < task.m_iFirstMB + task.m_iMBToProcess; i++)
    {
        DeblockOneLCU(i);
    }
} // void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstMB, Ipp32s iNumMBs)

void H265SegmentDecoder::CleanLeftEdges(bool leftAvailable, H265EdgeData *ctb_start_edge, Ipp32s height)
{
    if (!leftAvailable)
    {
        for (Ipp32s j = 0; j < (height >> 3); j++)
        {
            VM_ASSERT(ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 0].strength == 3);
            VM_ASSERT(ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 1].strength == 3);

            ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 0].strength = 0;
            ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + 4 + 1].strength = 0;
        }
    }
}

void H265SegmentDecoder::CleanTopEdges(bool topAvailable, H265EdgeData *ctb_start_edge, Ipp32s width)
{
    if (!topAvailable)
    {
        for (Ipp32s i = 0; i < (width >> 3); i++)
        {
            VM_ASSERT(ctb_start_edge[i * 4 + 4 + 2].strength == 3);
            VM_ASSERT(ctb_start_edge[i * 4 + 4 + 3].strength == 3);

            ctb_start_edge[i * 4 + 4 + 2].strength = 0;
            ctb_start_edge[i * 4 + 4 + 3].strength = 0;
        }
    }
}

void H265SegmentDecoder::CleanRightHorEdges(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;

    for (Ipp32s j = 0; j < (m_pSeqParamSet->MaxCUSize >> 3); j++)
    {
        ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + (m_pSeqParamSet->MaxCUSize >> 1) + 2].strength = 0;
        ctb_start_edge[j * m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth + (m_pSeqParamSet->MaxCUSize >> 1) + 3].strength = 0;
    }
}

void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr)
{
    H265CodingUnit* curLCU = m_pCurrentFrame->getCU(curLCUAddr);
    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s width, height;
    Ipp32s i, j;
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    VM_ASSERT (!curLCU->m_SliceHeader->slice_deblocking_filter_disabled_flag);

    width = frameWidthInSamples - curLCU->m_CUPelX;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    height = frameHeightInSamples - curLCU->m_CUPelY;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    bool picLBoundary = ((curLCU->m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((curLCU->m_CUPelY == 0) ? true : false);
    Ipp32s numLCUInPicWidth = m_pCurrentFrame->m_CodingData->m_WidthInCU;

    if (!curLCU->m_SliceHeader->m_PicParamSet->loop_filter_across_tiles_enabled_flag)
    {
        Ipp32u tileID = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr);

        if (picLBoundary)
            curLCU->m_AvailBorder[SGU_L] = false;
        else
        {
            curLCU->m_AvailBorder[SGU_L] = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - 1) == tileID ? true : false;
            CleanLeftEdges(curLCU->m_AvailBorder[SGU_L], ctb_start_edge, height);
        }

        if (picTBoundary)
            curLCU->m_AvailBorder[SGU_T] = false;
        else
        {
            curLCU->m_AvailBorder[SGU_T] = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - numLCUInPicWidth) == tileID ? true : false;
            CleanTopEdges(curLCU->m_AvailBorder[SGU_T], ctb_start_edge, width);
        }
    }
    else if (!curLCU->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag)
    {
        if (picLBoundary)
            curLCU->m_AvailBorder[SGU_L] = false;
        else
        {
            H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - 1);
            Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
            Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

            curLCU->m_AvailBorder[SGU_L] = refSA == SA ? true : false;
            CleanLeftEdges(curLCU->m_AvailBorder[SGU_L], ctb_start_edge, height);
        }

        if (picTBoundary)
            curLCU->m_AvailBorder[SGU_T] = false;
        else
        {
            H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - numLCUInPicWidth);
            Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
            Ipp32u          SA = curLCU->m_SliceHeader->SliceCurStartCUAddr;

            curLCU->m_AvailBorder[SGU_T] = refSA == SA ? true : false;
            CleanTopEdges(curLCU->m_AvailBorder[SGU_T], ctb_start_edge, width);
        }
    }
    else
    {
        curLCU->m_AvailBorder[SGU_L] = !picLBoundary;
        curLCU->m_AvailBorder[SGU_T] = !picTBoundary;
    }

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
            DeblockOneCrossLuma(curLCU, i, j);
        }
    }

    for (j = 0; j < height; j += 16)
    {
        for (i = 0; i < width; i += 16)
        {
            DeblockOneCrossChroma(curLCU, i, j);
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossLuma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    H265PlaneYCommon* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *ctb_start_edge, *edge;
    Ipp32s i, end;

    srcDstStride = m_pCurrentFrame->pitch_luma();
    baseSrcDst = m_pCurrentFrame->GetLumaAddr(curLCU->CUAddr) +
                 curPixelRow * srcDstStride + curPixelColumn;

    ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + 1) + 0;
    for (i = 0; i < 2; i++)
    {
        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<VERT_FILT>(curLCU->m_CUPelX + curPixelColumn, curLCU->m_CUPelY + curPixelRow + 4 * i, edge);

        if (edge->strength > 0)
        {
            MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_8u_I)(edge, baseSrcDst + 4 * i * srcDstStride, srcDstStride, VERT_FILT);
        }
        edge++;
    }

    end = 2;
    if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 8)
    {
        end = 3;
    }

    for (i = 0; i < end; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + ((i + 1) >> 1)) + (((i + 1) & 1) + 2);

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<HOR_FILT>(curLCU->m_CUPelX + curPixelColumn + 4 * (i - 1), curLCU->m_CUPelY + curPixelRow, edge);

        if (edge->strength > 0)
        {
            MFX_HEVC_PP::NAME(h265_FilterEdgeLuma_8u_I)(edge, baseSrcDst + 4 * (i - 1), srcDstStride, HOR_FILT);
        }
    }
}

void H265SegmentDecoder::DeblockOneCrossChroma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    H265PlaneUVCommon* baseSrcDst;
    Ipp32s srcDstStride;
    Ipp32s chromaCbQpOffset, chromaCrQpOffset;
    H265EdgeData *ctb_start_edge, *edge;
    Ipp32s i, end;

    srcDstStride = m_pCurrentFrame->pitch_chroma();
    baseSrcDst = m_pCurrentFrame->GetCbCrAddr(curLCU->CUAddr) +
                 (curPixelRow >> 1) * srcDstStride + (curPixelColumn >> 1) * 2;

    chromaCbQpOffset = m_pPicParamSet->pps_cb_qp_offset;
    chromaCrQpOffset = m_pPicParamSet->pps_cr_qp_offset;

    ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (curLCU->m_CUPelY >> 3) + (curLCU->m_CUPelX >> 3) * 4;

    for (i = 0; i < 2; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (y + i) + 4 * (x + 1) + 0;

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<VERT_FILT>(curLCU->m_CUPelX + curPixelColumn, curLCU->m_CUPelY + curPixelRow + 4 * i, edge);

        if (edge->strength > 1)
        {
            MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I(
                edge, 
                baseSrcDst + 4 * i * srcDstStride,
                srcDstStride,
                VERT_FILT,
                QpUV(edge->qp + chromaCbQpOffset),
                QpUV(edge->qp + chromaCrQpOffset));
        }
    }

    end = 2;
    if ((Ipp32s)curLCU->m_CUPelX + curPixelColumn >= frameWidthInSamples - 16)
    {
        end = 3;
    }

    for (i = 0; i < end; i++)
    {
        edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + 4 * (x + i) + 2;

        VM_ASSERT(edge->strength < 4);

        if (edge->strength == 3)
            GetEdgeStrengthDelayed<HOR_FILT>(curLCU->m_CUPelX + curPixelColumn + 4 * (i - 1), curLCU->m_CUPelY + curPixelRow, edge);

        if (edge->strength > 1)
        {
            MFX_HEVC_PP::h265_FilterEdgeChroma_Interleaved_8u_I(
                edge, 
                baseSrcDst + 4 * (i - 1) * 2,
                srcDstStride,
                HOR_FILT,
                QpUV(edge->qp + chromaCbQpOffset),
                QpUV(edge->qp + chromaCrQpOffset));
        }
    }
}

static bool H265_FORCEINLINE MVIsnotEq(H265MotionVector mv0,
                      H265MotionVector mv1)
{
    H265MotionVector mv = mv0 - mv1;

    if ((mv.getAbsHor() >= 4) || (mv.getAbsVer() >= 4))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void H265SegmentDecoder::GetCTBEdgeStrengths(void)
{
    if (m_pCurrentFrame->m_pSlicesInfo->GetSliceCount() == 1 && !m_pPicParamSet->tiles_enabled_flag)
    {
        if (m_pSeqParamSet->log2_min_transform_block_size == 2)
            GetCTBEdgeStrengthsSimple<2>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 3)
            GetCTBEdgeStrengthsSimple<3>();
        else
            VM_ASSERT(0);
    }
    else
    {
        if (m_pSeqParamSet->log2_min_transform_block_size == 2)
            GetCTBEdgeStrengths<2>();
        else if (m_pSeqParamSet->log2_min_transform_block_size == 3)
            GetCTBEdgeStrengths<3>();
        else
            VM_ASSERT(0);
    }
}

template<Ipp32s tusize> void H265SegmentDecoder::GetCTBEdgeStrengths(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;
    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s xPel, yPel;

    Ipp32s height = m_pSeqParamSet->pic_height_in_luma_samples - m_curCU->m_CUPelY;
    if (height > maxCUSize)
        height = maxCUSize;

    Ipp32s width = m_pSeqParamSet->pic_width_in_luma_samples - m_curCU->m_CUPelX;
    if (width > maxCUSize)
        width = maxCUSize;

    Ipp32s curTULine = 0;
    H265EdgeData *edgeLine = ctb_start_edge;
    for (yPel = 0; yPel < height; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;
        curTULine = m_context->m_CurrCTBStride * (yPel >> tusize);

        if (m_curCU->m_CUPelX == 0)
        {
            edge[2].strength = edge[3].strength = 0;
        }

        for (xPel = 0; xPel < width; xPel += 8)
        {
            Ipp32s curTU = curTULine + (xPel >> tusize);
            Ipp32s leftTU = curTU - 1;
            Ipp32s upTU = curTU - m_context->m_CurrCTBStride;

            edge += 4;

            // Vertical edge
            GetEdgeStrength<tusize, VERT_FILT>(curTU, leftTU, edge, xPel, yPel);
            if (tusize < 3)
                GetEdgeStrength<tusize, VERT_FILT>(curTU + m_context->m_CurrCTBStride, leftTU + m_context->m_CurrCTBStride, edge + 1, xPel, yPel + 4);
            else if (tusize == 3)
                edge[1] = edge[0];

            // Horizontal edge
            GetEdgeStrength<tusize, HOR_FILT>(curTU, upTU, edge + 2, xPel, yPel);
            if (tusize < 3)
                GetEdgeStrength<tusize, HOR_FILT>(curTU + 1, upTU + 1, edge + 3, xPel + 4, yPel);
            else if (tusize == 3)
                edge[3] = edge[2];
        }

        for (; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    for (; yPel < maxCUSize; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;

        for (xPel = 0; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }
}

template<Ipp32s tusize, Ipp32s dir> void H265SegmentDecoder::GetEdgeStrength(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, Ipp32s xQ, Ipp32s yQ)
{
    H265FrameHLDNeighborsInfo infoQ = m_context->m_CurrCTBFlags[tuQ];
    H265FrameHLDNeighborsInfo infoP = m_context->m_CurrCTBFlags[tuP];

    edge->tcOffset = (Ipp8s)m_pSliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_pSliceHeader->slice_beta_offset;

    bool anotherCU;
    H265EdgeData *next_edge;
    if (dir == VERT_FILT)
    {
        anotherCU = (xQ == 0);
        next_edge = edge + 4;
    }
    else
    {
        anotherCU = (yQ == 0);
        next_edge = edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    edge->deblockQ = infoQ.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoQ.members.IsIPCM)
            edge->deblockQ = 0;
    }

    H265FrameHLDNeighborsInfo save;
    if (tusize < 3)
    {
        if (dir == VERT_FILT)
            save = m_context->m_CurrCTBFlags[tuQ + 1];
        else
            save = m_context->m_CurrCTBFlags[tuQ + m_context->m_CurrCTBStride];
    }
    else if (tusize == 3)
        save = m_context->m_CurrCTBFlags[tuQ];

    next_edge->isIntraP = save.members.IsIntra;
    next_edge->isTrCbfYP = save.members.IsTrCbfY;
    next_edge->deblockP = edge->deblockQ;
    next_edge->qpP = save.members.qp;

    if (!infoP.members.IsAvailable)
    {
        // Another slice or tile case. May need a delayed strength calculation
        VM_ASSERT(anotherCU);

        if (dir == VERT_FILT)
        {
            if (m_curCU->m_CUPelX + xQ == 0)
            {
                edge->strength = 0;
                return;
            }
        }
        else
        {
            if (m_curCU->m_CUPelY + yQ == 0)
            {
                edge->strength = 0;
                return;
            }
        }

        // delayed calculation
        edge->qp = infoQ.members.qp;
        edge->isIntraQ = infoQ.members.IsIntra;
        edge->isTrCbfYQ = infoQ.members.IsTrCbfY;
        edge->strength = 3;
        return;
    }

    edge->qp = (infoQ.members.qp + edge->qpP + 1) >> 1;

    // Intra
    if (anotherCU || infoQ.members.TrStart != infoP.members.TrStart)
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 2;
            return;
        }

        if (infoQ.members.IsTrCbfY || infoP.members.IsTrCbfY)
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 0;
            return;
        }
    }

    H265MVInfo *mvinfoQ = m_context->m_CurrCTB + tuQ;
    H265MVInfo *mvinfoP = m_context->m_CurrCTB + tuP;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

template<Ipp32s dir> void H265SegmentDecoder::GetEdgeStrengthDelayed(Ipp32s x, Ipp32s y, H265EdgeData *edge)
{
    if (m_pSeqParamSet->log2_min_transform_block_size == 2)
        GetEdgeStrengthDelayed<2, dir>(x, y, edge);
    else if (m_pSeqParamSet->log2_min_transform_block_size == 3)
        GetEdgeStrengthDelayed<3, dir>(x, y, edge);
    else
        VM_ASSERT(0);
}

template<Ipp32s tusize, Ipp32s dir> void H265SegmentDecoder::GetEdgeStrengthDelayed(Ipp32s x, Ipp32s y, H265EdgeData *edge)
{
    edge->qp = (edge->qp + edge->qpP + 1) >> 1;

    if (edge->isIntraQ || edge->isIntraP)
    {
        edge->strength = 2;
        return;
    }
    else if (edge->isTrCbfYQ || edge->isTrCbfYP)
    {
        edge->strength = 1;
        return;
    }

    x >>= tusize;
    y >>= tusize;

    H265MVInfo *mvinfoQ = m_pCurrentFrame->m_CodingData->m_colocatedInfo + y * m_pSeqParamSet->NumPartitionsInFrameWidth + x;

    if (dir == VERT_FILT)
    {
        x -= 1;
        VM_ASSERT(x >= 0);
    }
    else
    {
        y -= 1;
        VM_ASSERT(y >= 0);
    }

    H265MVInfo *mvinfoP = m_pCurrentFrame->m_CodingData->m_colocatedInfo + y * m_pSeqParamSet->NumPartitionsInFrameWidth + x;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

void H265SegmentDecoder::GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265EdgeData *edge)
{
    Ipp32s numRefsQ = 0;
    for (Ipp32s i = 0; i < 2; i++)
        if (mvinfoQ->m_refIdx[i] >= 0)
            numRefsQ++;

    Ipp32s numRefsP = 0;
    for (Ipp32s i = 0; i < 2; i++)
        if (mvinfoP->m_refIdx[i] >= 0)
            numRefsP++;

    if (numRefsP != numRefsQ)
    {
        edge->strength = 1;
        return;
    }

    if (numRefsQ == 2)
    {
        if (((mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_0]) &&
            (mvinfoQ->m_pocDelta[REF_PIC_LIST_1] == mvinfoP->m_pocDelta[REF_PIC_LIST_1])) ||
            ((mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_1]) &&
            (mvinfoQ->m_pocDelta[REF_PIC_LIST_1] == mvinfoP->m_pocDelta[REF_PIC_LIST_0])))
        {
            if (mvinfoP->m_pocDelta[REF_PIC_LIST_0] != mvinfoP->m_pocDelta[REF_PIC_LIST_1])
            {
                if (mvinfoQ->m_pocDelta[REF_PIC_LIST_0] == mvinfoP->m_pocDelta[REF_PIC_LIST_0])
                {
                    edge->strength = (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) |
                        MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1])) ? 1 : 0;
                    return;
                }
                else
                {
                    edge->strength = (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) |
                        MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0])) ? 1 : 0;
                    return;
                }
            }
            else
            {
                edge->strength = ((MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) |
                        MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1])) &&
                        (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) |
                        MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0])) ? 1 : 0);
                return;
            }
        }

        edge->strength = 1;
        return;
    }
    else
    {
        Ipp16s POCDeltaQ, POCDeltaP;
        H265MotionVector MVQ, MVP;
        
        if (mvinfoQ->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_0];
            MVQ = mvinfoQ->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_1];
            MVQ = mvinfoQ->m_mv[REF_PIC_LIST_1];
        }

        if (mvinfoP->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_0];
            MVP = mvinfoP->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_1];
            MVP = mvinfoP->m_mv[REF_PIC_LIST_1];
        }

        if (POCDeltaQ == POCDeltaP)
        {
            edge->strength = MVIsnotEq(MVP, MVQ) ? 1 : 0;
            return;
        }

        edge->strength = 1;
        return;
    }
}

template<Ipp32s tusize> void H265SegmentDecoder::GetCTBEdgeStrengthsSimple(void)
{
    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge +
        m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_curCU->m_CUPelY >> 3) + (m_curCU->m_CUPelX >> 3) * 4;
    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s xPel, yPel;

    Ipp32s height = m_pSeqParamSet->pic_height_in_luma_samples - m_curCU->m_CUPelY;
    if (height > maxCUSize)
        height = maxCUSize;

    Ipp32s width = m_pSeqParamSet->pic_width_in_luma_samples - m_curCU->m_CUPelX;
    if (width > maxCUSize)
        width = maxCUSize;

    Ipp32s curTULine = 0;
    H265EdgeData *edgeLine = ctb_start_edge;
    for (yPel = 0; yPel < height; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;
        curTULine = m_context->m_CurrCTBStride * (yPel >> tusize);

        if (m_curCU->m_CUPelX == 0)
        {
            edge[2].strength = edge[3].strength = 0;
        }

        for (xPel = 0; xPel < width; xPel += 8)
        {
            Ipp32s curTU = curTULine + (xPel >> tusize);
            Ipp32s leftTU = curTU - 1;
            Ipp32s upTU = curTU - m_context->m_CurrCTBStride;

            edge += 4;

            // Vertical edge
            bool anotherCU = (xPel == 0);
            GetEdgeStrengthSimple(curTU, leftTU, edge, anotherCU);
            if (tusize < 3)
                GetEdgeStrengthSimple(curTU + m_context->m_CurrCTBStride, leftTU + m_context->m_CurrCTBStride, edge + 1, anotherCU);
            else if (tusize == 3)
                edge[1] = edge[0];

            // Horizontal edge
            anotherCU = (yPel == 0);
            GetEdgeStrengthSimple(curTU, upTU, edge + 2, anotherCU);
            if (tusize < 3)
                GetEdgeStrengthSimple(curTU + 1, upTU + 1, edge + 3, anotherCU);
            else if (tusize == 3)
                edge[3] = edge[2];
        }

        for (; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    for (; yPel < maxCUSize; yPel += 8)
    {
        H265EdgeData *edge = edgeLine;

        for (xPel = 0; xPel < maxCUSize; xPel += 8)
        {
            edge += 4;
            edge[0].strength = edge[1].strength = edge[2].strength = edge[3].strength = 0;
        }

        edgeLine += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }
}

void H265SegmentDecoder::GetEdgeStrengthSimple(Ipp32s tuQ, Ipp32s tuP, H265EdgeData *edge, bool anotherCU)
{
    H265FrameHLDNeighborsInfo infoQ = m_context->m_CurrCTBFlags[tuQ];
    H265FrameHLDNeighborsInfo infoP = m_context->m_CurrCTBFlags[tuP];

    edge->tcOffset = (Ipp8s)m_pSliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_pSliceHeader->slice_beta_offset;

    edge->deblockQ = infoQ.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoQ.members.IsIPCM)
            edge->deblockQ = 0;
    }
    edge->deblockP = infoP.members.IsTransquantBypass ? false : true;
    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (infoP.members.IsIPCM)
            edge->deblockP = 0;
    }

    if (!infoP.members.IsAvailable)
    {
        // Another slice or tile case. May need a delayed strength calculation
        VM_ASSERT(anotherCU);
        edge->strength = 0;
        return;
    }

    edge->qp = (infoQ.members.qp + infoP.members.qp + 1) >> 1;

    // Intra
    if (anotherCU || infoQ.members.TrStart != infoP.members.TrStart)
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 2;
            return;
        }

        if (infoQ.members.IsTrCbfY || infoP.members.IsTrCbfY)
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if (infoQ.members.IsIntra || infoP.members.IsIntra)
        {
            edge->strength = 0;
            return;
        }
    }

    H265MVInfo *mvinfoQ = m_context->m_CurrCTB + tuQ;
    H265MVInfo *mvinfoP = m_context->m_CurrCTB + tuP;
    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
