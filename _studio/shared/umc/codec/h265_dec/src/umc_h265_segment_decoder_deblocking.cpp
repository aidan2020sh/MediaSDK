/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_segment_decoder.h"
#include "umc_h265_frame_info.h"

enum
{
    VERT_FILT,
    HOR_FILT
};

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
    for (Ipp32s i = task.m_iFirstMB; i < task.m_iFirstMB + task.m_iMBToProcess; i++)
    {
        DeblockOneLCU(i);
    }
} // void H265SegmentDecoder::DeblockSegment(Ipp32s iFirstMB, Ipp32s iNumMBs)

static Ipp8u H265_FORCEINLINE MVIsnotEq(const H265MotionVector &mv0, const H265MotionVector &mv1)
{
    //return abs(mv0.Horizontal - mv1.Horizontal) >= 4 || abs(mv0.Vertical - mv1.Vertical) >= 4;
    return (7 <= (Ipp32u)(mv0.Horizontal - mv1.Horizontal + 3) || 7 <= (Ipp32u)(mv0.Vertical - mv1.Vertical + 3)) ? 1 : 0;
}

void H265SegmentDecoder::GetEdgeStrengthInter(H265MVInfo *mvinfoQ, H265MVInfo *mvinfoP, H265EdgeData *edge)
{
    Ipp32s numRefsQ = 0;
    for (Ipp32s i = 0; i < 2; i++)
        numRefsQ += (mvinfoQ->m_refIdx[i] >= 0) ? 1 : 0;

    Ipp32s numRefsP = 0;
    for (Ipp32s i = 0; i < 2; i++)
        numRefsP += (mvinfoP->m_refIdx[i] >= 0) ? 1 : 0;

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
                    edge->strength = MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1]);
                    return;
                }
                else
                {
                    edge->strength = MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0]);
                    return;
                }
            }
            else
            {
                edge->strength = (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_0]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_1])) &&
                        (MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_0], mvinfoP->m_mv[REF_PIC_LIST_1]) | MVIsnotEq(mvinfoQ->m_mv[REF_PIC_LIST_1], mvinfoP->m_mv[REF_PIC_LIST_0])) ? 1 : 0;
                return;
            }
        }

        edge->strength = 1;
        return;
    }
    else
    {
        Ipp32s POCDeltaQ, POCDeltaP;
        H265MotionVector *MVQ, *MVP;
        
        if (mvinfoQ->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_0];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaQ = mvinfoQ->m_pocDelta[REF_PIC_LIST_1];
            MVQ = &mvinfoQ->m_mv[REF_PIC_LIST_1];
        }

        if (mvinfoP->m_refIdx[REF_PIC_LIST_0] >= 0)
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_0];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_0];
        }
        else
        {
            POCDeltaP = mvinfoP->m_pocDelta[REF_PIC_LIST_1];
            MVP = &mvinfoP->m_mv[REF_PIC_LIST_1];
        }

        if (POCDeltaQ == POCDeltaP)
        {
            edge->strength = MVIsnotEq(*MVQ, *MVP);
            return;
        }

        edge->strength = 1;
        return;
    }
}

Ipp32s GetNumPartForHorFilt(H265CodingUnit* cu, Ipp32u absPartIdx)
{
    Ipp32s numParts;
    switch (cu->GetPartitionSize(absPartIdx))
    {
    case PART_SIZE_2NxN:
    case PART_SIZE_NxN:
    case PART_SIZE_2NxnU:
    case PART_SIZE_2NxnD:
        numParts = 2;
        break;
    default:
        numParts = 1;
        break;
    }

    return numParts;
}

Ipp32s GetNumPartForVertFilt(H265CodingUnit* cu, Ipp32u absPartIdx)
{
    Ipp32s numParts;
    switch (cu->GetPartitionSize(absPartIdx))
    {
    case PART_SIZE_Nx2N:
    case PART_SIZE_NxN:
    case PART_SIZE_nLx2N:
    case PART_SIZE_nRx2N:
        numParts = 2;
        break;
    default:
        numParts = 1;
        break;
    }

    return numParts;
}

void H265SegmentDecoder::DeblockTU(Ipp32u absPartIdx, Ipp32u trDepth, Ipp32s edgeType)
{
    Ipp32u stopDepth = m_cu->GetDepth(absPartIdx) + m_cu->GetTrIndex(absPartIdx);
    if (trDepth < stopDepth)
    {
        trDepth++;
        Ipp32u depth = trDepth;
        Ipp32u partOffset = m_cu->m_NumPartition >> (depth << 1);
        
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += partOffset)
        {
            DeblockTU(absPartIdx, trDepth, edgeType);
        }

        return;
    }

    Ipp32s size = m_cu->GetWidth(absPartIdx) >> m_cu->GetTrIndex(absPartIdx);

    if (m_context->m_sps->MinCUSize == 4 && ((edgeType == VERT_FILT) ? ((absPartIdx % 2) != 0) : ((absPartIdx-((absPartIdx>>2)<<2))/2 != 0)))
    {
        return;
    }

    Ipp32u curPixelColumn = m_cu->m_rasterToPelX[absPartIdx];
    Ipp32u curPixelRow = m_cu->m_rasterToPelY[absPartIdx];

    H265EdgeData edge1;
    H265EdgeData *edge = &edge1;
    edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

    if (edgeType == VERT_FILT)
    {
        Ipp32s srcDstStride = m_pCurrentFrame->pitch_luma();
        H265PlaneYCommon* baseSrcDst = m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr);

        Ipp32s srcDstStrideChroma = m_pCurrentFrame->pitch_chroma();
        H265PlaneYCommon* baseSrcDstChroma = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr);

        for (Ipp32s i = 0; i < size / 4; i++)
        {
            CalculateEdge<VERT_FILT>(edge, curPixelColumn, curPixelRow + 4 * i);

            if (edge->strength > 0)
            {
                m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
            }

            if (edge->strength > 1 && (i % 2) == 0 && !(curPixelColumn % 16) && !(curPixelRow % 8))
            {
                m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * (i>>1), VERT_FILT, m_pPicParamSet->pps_cb_qp_offset, m_pPicParamSet->pps_cr_qp_offset, m_pSeqParamSet->bit_depth_chroma);
            }
        }

        if (predictionExist && (curPixelRow >= saveRow && curPixelRow <= saveRow + saveHeight)&& (saveColumn > curPixelColumn && saveColumn < curPixelColumn + size))
        {
            curPixelColumn = saveColumn;

            for (Ipp32s i = 0; i < size / 4; i++)
            {
                CalculateEdge<VERT_FILT>(edge, curPixelColumn, curPixelRow + 4 * i);

                if (edge->strength > 0)
                {
                    m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);
                }

                if (edge->strength > 1 && (i % 2) == 0 && !(curPixelColumn % 16) && !(curPixelRow % 8))
                {
                    m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * (i>>1), VERT_FILT, m_pPicParamSet->pps_cb_qp_offset, m_pPicParamSet->pps_cr_qp_offset, m_pSeqParamSet->bit_depth_chroma);
                }
            }
        }
    }
    else
    {
        Ipp32s minSize = (isMin4x4Block ? 4 : 8);

        Ipp32s x = curPixelColumn >> (2 + !isMin4x4Block);
        Ipp32s y = curPixelRow >> 3;

        H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
        H265EdgeData *edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x;

        for (Ipp32s i = 0; i < size / minSize; i++)
        {
            edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
            edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;
            
            CalculateEdge<HOR_FILT>(edge, curPixelColumn + minSize * i, curPixelRow);

            edge++;
        }

        if (predictionExist && (curPixelColumn >= saveColumn && curPixelColumn <= saveColumn + saveHeight)&& (saveRow > curPixelRow && saveRow < curPixelRow + size))
        {
            curPixelRow = saveRow;
            y = curPixelRow >> 3;

            edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x;

            for (Ipp32s i = 0; i < size / minSize; i++)
            {
                edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
                edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

                CalculateEdge<HOR_FILT>(edge, curPixelColumn + minSize * i, curPixelRow);

                edge++;
            }
        }
    }
}

void H265SegmentDecoder::DeblockOneLCU(Ipp32u absPartIdx, Ipp32u depth, Ipp32s edgeType)
{
    if (m_cu->GetDepth(absPartIdx) > depth)
    {
        depth++;
        Ipp32u partOffset = m_cu->m_NumPartition >> (depth << 1);
        for (Ipp32s i = 0; i < 4; i++, absPartIdx += partOffset)
        {
            DeblockOneLCU(absPartIdx, depth, edgeType);
        }
        return;
    }

    Ipp32s countPart = (edgeType == VERT_FILT) ? GetNumPartForVertFilt(m_cu, absPartIdx) : GetNumPartForHorFilt(m_cu, absPartIdx);
    predictionExist = false;

    if (countPart == 2)
    {
        Ipp32u partAddr;
        Ipp32u width;
        Ipp32u height;

        Ipp32s isFourParts = m_cu->getNumPartInter(absPartIdx) == 4;

        Ipp32u PUOffset = (g_PUOffset[m_cu->GetPartitionSize(absPartIdx)] << ((m_context->m_sps->MaxCUDepth - depth) << 1)) >> 4;
        Ipp32s subPartIdx = absPartIdx + PUOffset;
        if (isFourParts && edgeType == HOR_FILT)
            subPartIdx += PUOffset;

        m_cu->getPartIndexAndSize(absPartIdx, depth, 1, partAddr, width, height);

        Ipp32u curPixelColumn = m_cu->m_rasterToPelX[subPartIdx];
        Ipp32u curPixelRow = m_cu->m_rasterToPelY[subPartIdx];

        if (m_context->m_sps->MinCUSize != 4 || (edgeType == VERT_FILT ? ((subPartIdx % 2) == 0) : ((subPartIdx-((subPartIdx>>2)<<2))/2 == 0)))
        {
            predictionExist = true;
            saveColumn = curPixelColumn;
            saveRow = curPixelRow;
            saveHeight = (edgeType == VERT_FILT) ? height : width;
            saveHeight <<= isFourParts;
        }
    }

    DeblockTU(absPartIdx, depth, edgeType);
}

void H265SegmentDecoder::DeblockOneCross(Ipp32s curPixelColumn, Ipp32s curPixelRow, bool isNeedAddHorDeblock)
{
    Ipp32s srcDstStride = m_pCurrentFrame->pitch_luma();
    H265PlaneYCommon* baseSrcDst = m_pCurrentFrame->GetLumaAddr(m_cu->CUAddr);

    Ipp32s srcDstStrideChroma = m_pCurrentFrame->pitch_chroma();
    H265PlaneUVCommon*baseSrcDstChroma = m_pCurrentFrame->GetCbCrAddr(m_cu->CUAddr);

    Ipp32s chromaCbQpOffset = m_pPicParamSet->pps_cb_qp_offset;
    Ipp32s chromaCrQpOffset = m_pPicParamSet->pps_cr_qp_offset;

    H265EdgeData edgeForCheck;
    H265EdgeData *edge = &edgeForCheck;
    edge->tcOffset = (Ipp8s)m_cu->m_SliceHeader->slice_tc_offset;
    edge->betaOffset = (Ipp8s)m_cu->m_SliceHeader->slice_beta_offset;

    /*for (Ipp32s i = 0; i < 2; i++)
    {
        CalculateEdge<VERT_FILT>(curLCU, edge, curPixelColumn, curPixelRow + 4 * i);

        if (edge->strength == 0)
            continue;

        m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn, curPixelRow + 4 * i, VERT_FILT, m_pSeqParamSet->bit_depth_luma);

        if (edge->strength > 1 && i == 0 && !(curPixelColumn % 16))
        {
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, (curPixelColumn >> 1), (curPixelRow>>1) + 4 * i, VERT_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }
    }*/

    Ipp32s x = curPixelColumn >> (2 + !isMin4x4Block);
    Ipp32s y = curPixelRow >> 3;

    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
    H265EdgeData *hor_edge = ctb_start_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * y + x - 1;

    Ipp32s end = isNeedAddHorDeblock ? 3 : 2;
    for (Ipp32s i = 0; i < end; i++)
    {
        Ipp32s currPixelColumnTemp = curPixelColumn + 4 * (i - 1);
        H265CodingUnit* curLCUTemp = m_cu;

        if (currPixelColumnTemp < 0)
        {
            currPixelColumnTemp += m_pSeqParamSet->MaxCUSize;

            if (m_cu->m_CUPelX)
            {
                curLCUTemp = m_pCurrentFrame->getCU(m_cu->CUAddr - 1);
                if (curLCUTemp->m_SliceHeader->slice_deblocking_filter_disabled_flag)
                {
                    hor_edge++;
                    continue;
                }

                //edge->tcOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_tc_offset;
                //edge->betaOffset = (Ipp8s)curLCUTemp->m_SliceHeader->slice_beta_offset;
            }
            else
            {
                hor_edge++;
                continue;
            }
        }

        //CalculateEdge<HOR_FILT>(curLCUTemp, edge, currPixelColumnTemp, curPixelRow);
        edge = hor_edge;

        if (edge->strength > 0)
        {
            m_reconstructor->FilterEdgeLuma(edge, baseSrcDst, srcDstStride, curPixelColumn + 4 * (i - 1), curPixelRow, HOR_FILT, m_pSeqParamSet->bit_depth_luma);
        }

        if (edge->strength > 1 && i != 1 && !(curPixelRow % 16))
        {
            Ipp32s column = (curPixelColumn>>1) + 4 * (i - 1);
            if (i == 2)
                column = (curPixelColumn>>1) + 4 * (i - 2);
            m_reconstructor->FilterEdgeChroma(edge, baseSrcDstChroma, srcDstStrideChroma, column, (curPixelRow >> 1), HOR_FILT, chromaCbQpOffset, chromaCrQpOffset, m_pSeqParamSet->bit_depth_chroma);
        }

        /*if (curPixelColumn + 4 * (i - 1) < 0)
        {
            // restore offsets
            edge->tcOffset = (Ipp8s)curLCU->m_SliceHeader->slice_tc_offset;
            edge->betaOffset = (Ipp8s)curLCU->m_SliceHeader->slice_beta_offset;
        }

        if (hor_edge->strength != edge->strength || (hor_edge->qp != edge->qp && edge->strength))
        {
            __asm int 3;
        }*/

        if (isMin4x4Block || i == 0)
        {
            hor_edge++;
        }
    }
}

void H265SegmentDecoder::DeblockOneLCU(Ipp32s curLCUAddr)
{
    m_cu = m_pCurrentFrame->getCU(curLCUAddr);
    m_pSliceHeader = m_cu->m_SliceHeader;

    if (m_cu->m_SliceHeader->slice_deblocking_filter_disabled_flag)
        return;

    Ipp32s maxCUSize = m_pSeqParamSet->MaxCUSize;
    Ipp32s frameHeightInSamples = m_pSeqParamSet->pic_height_in_luma_samples;
    Ipp32s frameWidthInSamples = m_pSeqParamSet->pic_width_in_luma_samples;

    VM_ASSERT (!m_cu->m_SliceHeader->slice_deblocking_filter_disabled_flag || m_bIsNeedWADeblocking);

    Ipp32s width = frameWidthInSamples - m_cu->m_CUPelX;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    Ipp32s height = frameHeightInSamples - m_cu->m_CUPelY;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    isMin4x4Block = m_pSeqParamSet->MinCUSize == 4 ? 1 : 0;

    bool picLBoundary = ((m_cu->m_CUPelX == 0) ? true : false);
    bool picTBoundary = ((m_cu->m_CUPelY == 0) ? true : false);
    Ipp32s numLCUInPicWidth = m_pCurrentFrame->m_CodingData->m_WidthInCU;

    m_cu->m_AvailBorder.m_left = !picLBoundary;
    m_cu->m_AvailBorder.m_top = !picTBoundary;

    if (!m_cu->m_SliceHeader->m_PicParamSet->loop_filter_across_tiles_enabled_flag)
    {
        Ipp32u tileID = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr);

        if (picLBoundary)
            m_cu->m_AvailBorder.m_left = false;
        else
        {
            m_cu->m_AvailBorder.m_left = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - 1) == tileID ? true : false;
        }

        if (picTBoundary)
            m_cu->m_AvailBorder.m_top = false;
        else
        {
            m_cu->m_AvailBorder.m_top = m_pCurrentFrame->m_CodingData->getTileIdxMap(curLCUAddr - numLCUInPicWidth) == tileID ? true : false;
        }
    }

    if (!m_cu->m_SliceHeader->slice_loop_filter_across_slices_enabled_flag)
    {
        if (picLBoundary)
            m_cu->m_AvailBorder.m_left = false;
        else
        {
            if (m_cu->m_AvailBorder.m_left)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - 1);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = m_cu->m_SliceHeader->SliceCurStartCUAddr;

                m_cu->m_AvailBorder.m_left = refSA == SA ? true : false;
            }
        }

        if (picTBoundary)
            m_cu->m_AvailBorder.m_top = false;
        else
        {
            if (m_cu->m_AvailBorder.m_top)
            {
                H265CodingUnit* refCU = m_pCurrentFrame->getCU(curLCUAddr - numLCUInPicWidth);
                Ipp32u          refSA = refCU->m_SliceHeader->SliceCurStartCUAddr;
                Ipp32u          SA = m_cu->m_SliceHeader->SliceCurStartCUAddr;

                m_cu->m_AvailBorder.m_top = refSA == SA ? true : false;
            }
        }
    }

#if 1
    DeblockOneLCU(0, 0, VERT_FILT);

    H265EdgeData *ctb_start_edge = m_pCurrentFrame->m_CodingData->m_edge + m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth * (m_cu->m_CUPelY >> 3) + (m_cu->m_CUPelX >> (2 + !isMin4x4Block));
    Ipp32s minSize = isMin4x4Block ? 4 : 8;
    
    for (int i = 0; i < (height >> 3); i++)
    {
        memset(ctb_start_edge, 0, sizeof(H265EdgeData)*width / minSize);
        ctb_start_edge += m_pCurrentFrame->m_CodingData->m_edgesInFrameWidth;
    }

    DeblockOneLCU(0, 0, HOR_FILT);
#endif

    bool isNeddAddHorDeblock = false;

    if ((Ipp32s)m_cu->m_CUPelX + width > frameWidthInSamples - 8)
    {
        isNeddAddHorDeblock = true;
    }

    if (m_bIsNeedWADeblocking && !isNeddAddHorDeblock)
    {
        Ipp32u width = frameWidthInSamples - m_cu->m_CUPelX;

        if (width > m_pSeqParamSet->MaxCUSize)
        {
            width = m_pSeqParamSet->MaxCUSize;
        }

        if (m_hasTiles)
        {
            H265CodingUnit* nextAddr = m_pCurrentFrame->m_CodingData->getCU(m_cu->CUAddr + 1);
            if (nextAddr && nextAddr->m_SliceHeader->slice_deblocking_filter_disabled_flag)
            {
                isNeddAddHorDeblock = true;
            }
        }
        else
        {
            if (m_pCurrentFrame->m_CodingData->GetInverseCUOrderMap(m_cu->CUAddr) == (Ipp32s)m_cu->m_SliceHeader->m_sliceSegmentCurEndCUAddr / m_pCurrentFrame->m_CodingData->m_NumPartitions)
            {
                H265Slice * nextSlice = m_pCurrentFrame->GetAU()->GetSliceByNumber(m_cu->m_SliceIdx + 1);
                if (!nextSlice || nextSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
                {
                    isNeddAddHorDeblock = true;
                }
            }
        }
    }

    for (Ipp32s j = 0; j < height; j += 8)
    {
        for (Ipp32s i = 0; i < width; i += 8)
        {
            DeblockOneCross(i, j, i == width - 8 ? isNeddAddHorDeblock : false);
        }
    }
}

template <Ipp32s direction>
void H265SegmentDecoder::CalculateEdge(H265EdgeData * edge, Ipp32s xPel, Ipp32s yPel)
{
    Ipp32s tusize = m_pSeqParamSet->log2_min_transform_block_size;
    Ipp32s y = yPel >> tusize;
    Ipp32s x = xPel >> tusize;

    Ipp32s tuPAddr = y*m_pCurrentFrame->m_CodingData->m_NumPartInWidth + x;
    Ipp32s tuQAddr = y*m_pCurrentFrame->m_CodingData->m_NumPartInWidth + x;

    bool anotherCU;
    H265CodingUnit* cuP = m_cu;

    if (direction == VERT_FILT)
    {
        if (tusize <= 3)
        {
            tuPAddr -= 1;
        }
        else if (tusize == 4)
        {
            tuPAddr -= (xPel & 8) ? 0 : 1;
        }
        else
        {
            tuPAddr -= (xPel & 24) ? 0 : 1;
        }
        
        anotherCU = xPel == 0;
        if (anotherCU)
        {
            if (!m_cu->m_AvailBorder.m_left)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(m_cu->CUAddr - 1);
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
    }
    else
    {
        if (tusize <= 3)
        {
            tuPAddr -= m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
        else if (tusize == 4)
        {
            tuPAddr -= (yPel & 8) ? 0 : m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }
        else
        {
            tuPAddr -= (yPel & 24) ? 0 : m_pCurrentFrame->m_CodingData->m_NumPartInWidth;
        }

        anotherCU = yPel == 0;
        if (anotherCU)
        {
            if (!m_cu->m_AvailBorder.m_top)
            {
                edge->strength = 0;
                return;
            }

            cuP = m_pCurrentFrame->getCU(m_cu->CUAddr - m_pCurrentFrame->getFrameWidthInCU());
            tuPAddr += m_pCurrentFrame->m_CodingData->m_NumPartitions;
        }
    }

    if (tuPAddr == tuQAddr)
    {
        edge->strength = 0;
        return;
    }

    tuPAddr = m_cu->m_rasterToZscan[tuPAddr];
    tuQAddr = m_cu->m_rasterToZscan[tuQAddr];

    H265CodingUnitData * tuP = cuP->GetCUData(tuPAddr);
    H265CodingUnitData * tuQ = m_cu->GetCUData(tuQAddr);

    edge->deblockQ = tuQ->cu_transform_bypass ? 0 : 1;
    edge->deblockP = tuP->cu_transform_bypass ? 0 : 1;

    if (m_pSeqParamSet->pcm_loop_filter_disabled_flag)
    {
        if (tuQ->pcm_flag)
            edge->deblockQ = 0;

        if (tuP->pcm_flag)
            edge->deblockP = 0;
    }

    edge->qp = (tuQ->qp + tuP->qp + 1) >> 1;

    if (tuQ->predMode || tuP->predMode) // predMode > 0 - intra or out of frame bounds
    {
        edge->strength = (tuQ->predMode == 2 || tuP->predMode == 2) ? 0 : 2;  // predMode equals 2 means out of frame bounds !!
        return;
    }

    // Intra
    if (anotherCU || tuQ->trStart != tuP->trStart || tuQ->trIndex != tuP->trIndex)
    {
        if (m_cu->GetCbf(COMPONENT_LUMA, tuQ->trStart, tuQ->trIndex) != 0 || cuP->GetCbf(COMPONENT_LUMA, tuP->trStart, tuP->trIndex) != 0)
        {
            edge->strength = 1;
            return;
        }
    }

    Ipp32s PartX = (m_cu->m_CUPelX >> m_pSeqParamSet->log2_min_transform_block_size) + x;
    Ipp32s PartY = (m_cu->m_CUPelY >> m_pSeqParamSet->log2_min_transform_block_size) + y;

    H265MVInfo *mvinfoQ = &m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX);
    H265MVInfo *mvinfoP;

    if (direction == VERT_FILT)
    {
        mvinfoP = &(m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * PartY + PartX - 1));
    }
    else
    {
        mvinfoP = &(m_context->m_frame->getCD()->GetTUInfo(m_context->m_frame->getNumPartInCUSize() * m_context->m_frame->getFrameWidthInCU() * (PartY - 1) + PartX));
    }

    GetEdgeStrengthInter(mvinfoQ, mvinfoP, edge);
}


} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
