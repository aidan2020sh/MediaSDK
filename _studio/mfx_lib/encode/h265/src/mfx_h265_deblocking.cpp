//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"
#include "mfx_h265_optimization.h"

#define VERT_FILT 0
#define HOR_FILT  1
#define Clip3 Saturate

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

static bool MVIsnotEq(H265MV mv0,
                      H265MV mv1)
{
    Ipp16s mvxd = (Ipp16s)ABS(mv0.mvx - mv1.mvx);
    Ipp16s mvyd = (Ipp16s)ABS(mv0.mvy - mv1.mvy);

    if ((mvxd >= 4) || (mvyd >= 4))
    {
        return true;
    }
    else
    {
        return false;
    }
}

void H265CU::DeblockOneCrossLuma(Ipp32s curPixelColumn,
                                 Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = par->Width;
    Ipp32s frameHeightInSamples = par->Height;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    H265EdgeData *edge;
    Ipp32s i, start, end;

    srcDstStride = pitch_rec_luma;
    baseSrcDst = y_rec + curPixelRow * srcDstStride + curPixelColumn;

    start = 0;
    end = 2;
    if ((Ipp32s)ctb_pely + curPixelRow >= frameHeightInSamples - 8)
    {
        end = 3;
    }

    for (i = start; i < end; i++)
    {
        edge = &m_edge[y + ((i + 1) >> 1)][x + 1][(i + 1) & 1];

        if (edge->strength > 0)
        {
            MFX_HEVC_COMMON::h265_FilterEdgeLuma_8u_I(edge, baseSrcDst + 4 * (i - 1) * srcDstStride,
                           srcDstStride, VERT_FILT);
        }
    }


    end = 2;
    if ((Ipp32s)ctb_pelx + curPixelColumn >= frameWidthInSamples - 8)
    {
        end = 3;
    }

    for (i = 0; i < end; i++)
    {
        edge = &m_edge[y + 1][x + ((i + 1) >> 1)][((i + 1) & 1) + 2];

        if (edge->strength > 0)
        {
            MFX_HEVC_COMMON::h265_FilterEdgeLuma_8u_I(edge, baseSrcDst + 4 * (i - 1), srcDstStride, HOR_FILT);
        }
    }
}

static Ipp8u GetChromaQP(
    Ipp8u in_QPy,
    Ipp32s in_QPOffset,
    Ipp8u in_BitDepthChroma)
{
    Ipp32s qPc;
    Ipp32s QpBdOffsetC = 6  *(in_BitDepthChroma - 8);

    qPc = Clip3(-QpBdOffsetC, 57, in_QPy + in_QPOffset);

    if (qPc >= 0)
    {
        qPc = h265_QPtoChromaQP[qPc];
    }

    return (Ipp8u)(qPc + QpBdOffsetC);
}


void H265CU::DeblockOneCrossChroma(Ipp32s curPixelColumn,
                                   Ipp32s curPixelRow)
{
    Ipp32s frameWidthInSamples = par->Width;
    Ipp32s frameHeightInSamples = par->Height;
    Ipp32s x = curPixelColumn >> 3;
    Ipp32s y = curPixelRow >> 3;
    PixType* baseSrcDst;
    Ipp32s srcDstStride;
    Ipp32s chromaQpOffset;
    H265EdgeData *edge;
    Ipp32s i, c, start, end;

    srcDstStride = pitch_rec_chroma;
    baseSrcDst = u_rec + (curPixelRow >> 1) * srcDstStride + (curPixelColumn >> 1);
    chromaQpOffset = cslice->slice_cb_qp_offset;

    for (c = 0; c < 2; c++)
    {
        start = 0;
        end = 2;
        if ((Ipp32s)ctb_pely + curPixelRow >= frameHeightInSamples - 16)
        {
            end = 3;
        }

        for (i = start; i < end; i++)
        {
            edge = &m_edge[y + i][x + 1][0];

            if (edge->strength > 1)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeChroma_Plane_8u_I(
                    edge, 
                    baseSrcDst + 4 * (i - 1) * srcDstStride,
                    srcDstStride, 
                    VERT_FILT, 
                    GetChromaQP(edge->qp, 0, 8));
            }
        }

        end = 2;
        if ((Ipp32s)ctb_pelx + curPixelColumn >= frameWidthInSamples - 16)
        {
            end = 3;
        }

        for (i = 0; i < end; i++)
        {
            edge = &m_edge[y + 1][x + i][2];

            if (edge->strength > 1)
            {
                MFX_HEVC_COMMON::h265_FilterEdgeChroma_Plane_8u_I(
                    edge, 
                    baseSrcDst + 4 * (i - 1),
                    srcDstStride, 
                    HOR_FILT,
                    GetChromaQP(edge->qp, 0, 8));
            }
        }

        baseSrcDst = v_rec + (curPixelRow >> 1) * srcDstStride + (curPixelColumn >> 1);
        chromaQpOffset = cslice->slice_cr_qp_offset;
    }
}


void H265CU::GetEdgeStrength(H265CUPtr *pcCUQptr,
                             H265EdgeData *edge,
                             Ipp32s curPixelColumn,
                             Ipp32s curPixelRow,
                             Ipp32s crossSliceBoundaryFlag,
                             Ipp32s crossTileBoundaryFlag,
                             Ipp32s tcOffset,
                             Ipp32s betaOffset,
                             Ipp32s dir)
{
    Ipp32s log2LCUSize = par->Log2MaxCUSize;
    Ipp32s log2MinTUSize = par->Log2MinTUSize;
    Ipp32u uiPartQ, uiPartP;
    Ipp32s maxDepth = log2LCUSize - log2MinTUSize;
    H265CUPtr CUPPtr;
    H265CUData *pcCUP, *pcCUQ;
    pcCUQ = pcCUQptr->ctb_data_ptr;

    edge->strength = 0;

    edge->uiPartQ = uiPartQ = h265_scan_r2z[maxDepth][((curPixelRow >> log2MinTUSize) << (log2LCUSize - log2MinTUSize)) + (curPixelColumn >> log2MinTUSize)];

    if (dir == HOR_FILT)
    {
        if ((curPixelRow >> log2MinTUSize) == ((curPixelRow - 1) >> log2MinTUSize))
        {
            return;
        }
        getPUAbove(&CUPPtr, uiPartQ, !crossSliceBoundaryFlag, false, false, false, !crossTileBoundaryFlag);
        if (pcCUQ != data && CUPPtr.ctb_data_ptr) {
            // pcCUQptr is left of cur CTB, adjust CUPPtr
            CUPPtr.ctb_data_ptr -= ((size_t)1 << (size_t)(par->Log2NumPartInCU));
            CUPPtr.ctb_addr --;
        }
    }
    else
    {
        if ((curPixelColumn >> log2MinTUSize) == ((curPixelColumn - 1) >> log2MinTUSize))
        {
            return;
        }
        getPULeft(&CUPPtr, uiPartQ, !crossSliceBoundaryFlag, false, !crossTileBoundaryFlag);
        if (pcCUQ != data && CUPPtr.ctb_data_ptr) {
            // pcCUQptr is above of cur CTB, adjust CUPPtr
            CUPPtr.ctb_data_ptr -= par->PicWidthInCtbs << par->Log2NumPartInCU;
            CUPPtr.ctb_addr -= par->PicWidthInCtbs;
        }
    }

    if (CUPPtr.ctb_data_ptr == NULL)
    {
        return;
    }

    uiPartP = CUPPtr.abs_part_idx;
    pcCUP = CUPPtr.ctb_data_ptr;

    edge->deblockP = 1;
    edge->deblockQ = 1;

    edge->tcOffset = (Ipp8s)tcOffset;
    edge->betaOffset = (Ipp8s)betaOffset;
/*
    if ((pcCUP->m_IPCMFlag[uiPartP] != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
    {
        edge->deblockP = 0;
    }

    if ((pcCUQ->m_IPCMFlag[uiPartQ] != 0) && (m_pSeqParamSet->getPCMFilterDisableFlag() != 0))
    {
        edge->deblockQ = 0;
    }

    if ((pcCUP->m_CUTransquantBypass[uiPartP] != 0))
    {
        edge->deblockP = 0;
    }

    if ((pcCUQ->m_CUTransquantBypass[uiPartQ] != 0))
    {
        edge->deblockQ = 0;
    }
*/
    edge->qp = (pcCUQ[uiPartQ].qp + pcCUP[uiPartP].qp + 1) >> 1;
    Ipp32s depthp = pcCUP[uiPartP].depth + pcCUP[uiPartP].tr_idx;
    Ipp32s depthq = pcCUQ[uiPartQ].depth + pcCUQ[uiPartQ].tr_idx;

    if ((pcCUQ != pcCUP) ||
        (pcCUP[uiPartP].depth != pcCUQ[uiPartQ].depth) ||
        (depthp != depthq) ||
        ((uiPartP ^ uiPartQ) >> ((par->MaxCUDepth - depthp) << 1)))
    {
        if ((pcCUQ[uiPartQ].pred_mode == MODE_INTRA) ||
            (pcCUP[uiPartP].pred_mode == MODE_INTRA))
        {
            edge->strength = 2;
            return;
        }

        if (((pcCUQ[uiPartQ].cbf[0] >> pcCUQ[uiPartQ].tr_idx) != 0) ||
            ((pcCUP[uiPartP].cbf[0] >> pcCUP[uiPartP].tr_idx) != 0))
        {
            edge->strength = 1;
            return;
        }
    }
    else
    {
        if ((pcCUQ[uiPartQ].pred_mode == MODE_INTRA) ||
            (pcCUP[uiPartP].pred_mode == MODE_INTRA))
        {
            edge->strength = 0;
            return;
        }
    }

    /* Check MV */
    {
        Ipp32s RefPOCQ0, RefPOCQ1, RefPOCP0, RefPOCP1;
        Ipp32s numRefsP, numRefsQ;
        H265MV MVQ0, MVQ1, MVP0, MVP1;
        Ipp32s i;

        numRefsP = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUP[uiPartP].ref_idx[RefPicList] >= 0)
            {
                numRefsP++;
            }
        }

        numRefsQ = 0;

        for (i = 0; i < 2; i++)
        {
            EnumRefPicList RefPicList = (i ? REF_PIC_LIST_1 : REF_PIC_LIST_0);
            if (pcCUQ[uiPartQ].ref_idx[RefPicList] >= 0)
            {
                numRefsQ++;
            }
        }

        if (numRefsP != numRefsQ)
        {
            edge->strength = 1;
            return;
        }

        if (numRefsQ == 2)
        {
            RefPOCQ0 = par->m_pRefPicList[par->m_slice_ids[ctb_addr]].
                m_RefPicListL0.m_RefPicList[pcCUQ[uiPartQ].ref_idx[REF_PIC_LIST_0]]->m_PicOrderCnt;
            RefPOCQ1 = par->m_pRefPicList[par->m_slice_ids[ctb_addr]].
                m_RefPicListL1.m_RefPicList[pcCUQ[uiPartQ].ref_idx[REF_PIC_LIST_1]]->m_PicOrderCnt;
            MVQ0 = pcCUQ[uiPartQ].mv[REF_PIC_LIST_0];
            MVQ1 = pcCUQ[uiPartQ].mv[REF_PIC_LIST_1];
            RefPOCP0 = par->m_pRefPicList[par->m_slice_ids[CUPPtr.ctb_addr]].
                m_RefPicListL0.m_RefPicList[pcCUP[uiPartP].ref_idx[REF_PIC_LIST_0]]->m_PicOrderCnt;
            RefPOCP1 = par->m_pRefPicList[par->m_slice_ids[CUPPtr.ctb_addr]].
                m_RefPicListL1.m_RefPicList[pcCUP[uiPartP].ref_idx[REF_PIC_LIST_1]]->m_PicOrderCnt;
            MVP0 = pcCUP[uiPartP].mv[REF_PIC_LIST_0];
            MVP1 = pcCUP[uiPartP].mv[REF_PIC_LIST_1];

            if (((RefPOCQ0 == RefPOCP0) && (RefPOCQ1 == RefPOCP1)) ||
                ((RefPOCQ0 == RefPOCP1) && (RefPOCQ1 == RefPOCP0)))

            {
                if (RefPOCP0 != RefPOCP1)
                {
                    if (RefPOCP0 == RefPOCQ0)
                    {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    }
                    else
                    {
                        Ipp32s bs = ((MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0)) ? 1 : 0);
                        edge->strength = (Ipp8u)bs;
                        return;
                    }
                }
                else
                {
                    Ipp32s bs = (((MVIsnotEq(MVP0, MVQ0) | MVIsnotEq(MVP1, MVQ1)) &&
                                  (MVIsnotEq(MVP0, MVQ1) | MVIsnotEq(MVP1, MVQ0))) ? 1 : 0);
                    edge->strength = (Ipp8u)bs;
                    return;
                }
            }

            edge->strength = 1;
            return;
        }
        else
        {
            if (pcCUQ[uiPartQ].ref_idx[REF_PIC_LIST_0] >= 0) {
                RefPOCQ0 = par->m_pRefPicList[par->m_slice_ids[ctb_addr]].
                    m_RefPicListL0.m_RefPicList[pcCUQ[uiPartQ].ref_idx[REF_PIC_LIST_0]]->m_PicOrderCnt;
                MVQ0 = pcCUQ[uiPartQ].mv[REF_PIC_LIST_0];
            } else {
                RefPOCQ0 = par->m_pRefPicList[par->m_slice_ids[ctb_addr]].
                    m_RefPicListL1.m_RefPicList[pcCUQ[uiPartQ].ref_idx[REF_PIC_LIST_1]]->m_PicOrderCnt;
                MVQ0 = pcCUQ[uiPartQ].mv[REF_PIC_LIST_1];
            }

            if (pcCUP[uiPartP].ref_idx[REF_PIC_LIST_0] >= 0) {
                RefPOCP0 = par->m_pRefPicList[par->m_slice_ids[CUPPtr.ctb_addr]].
                    m_RefPicListL0.m_RefPicList[pcCUP[uiPartP].ref_idx[REF_PIC_LIST_0]]->m_PicOrderCnt;
                MVP0 = pcCUP[uiPartP].mv[REF_PIC_LIST_0];
            } else {
                RefPOCP0 = par->m_pRefPicList[par->m_slice_ids[CUPPtr.ctb_addr]].
                    m_RefPicListL1.m_RefPicList[pcCUP[uiPartP].ref_idx[REF_PIC_LIST_1]]->m_PicOrderCnt;
                MVP0 = pcCUP[uiPartP].mv[REF_PIC_LIST_1];
            }

            if (RefPOCQ0 == RefPOCP0)
            {
                Ipp32s bs = (MVIsnotEq(MVP0, MVQ0) ? 1 : 0);
                edge->strength = (Ipp8u)bs;
                return;
            }

            edge->strength = 1;
            return;
        }
    }
}


//Edges

//  0
//2_|_3
//  |
//  1

void H265CU::SetEdges(Ipp32s width,
                      Ipp32s height)
{
    Ipp32s maxCUSize = par->MaxCUSize;
    Ipp32s curPixelColumn, curPixelRow;
    Ipp32s crossSliceBoundaryFlag, crossTileBoundaryFlag;
    Ipp32s tcOffset, betaOffset;
    H265EdgeData edge;
    Ipp32s dir;
    Ipp32s i, j, e;

    crossSliceBoundaryFlag = cslice->slice_loop_filter_across_slices_enabled_flag;
    crossTileBoundaryFlag = 1;
    tcOffset = cslice->slice_tc_offset_div2 << 1;
    betaOffset = cslice->slice_beta_offset_div2 << 1;

    H265CUPtr aboveLCU;
    getPUAbove(&aboveLCU, 0, 0, false, false, false, 0);

// uncomment to hide false uninitialized memory read warning
//    memset(&edge, 0, sizeof(edge));

    if (aboveLCU.ctb_data_ptr)
    {
        for (i = 0; i < width; i += 8)
        {
            for (e = 0; e < 2; e++)
            {
                curPixelColumn = i;
                curPixelRow = (maxCUSize - 8 + 4 * e);

                GetEdgeStrength(&aboveLCU, &edge, curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset, VERT_FILT);

                m_edge[0][(i >> 3)+1][e] = edge;
            }
        }
    }
    else
    {
        for (i = 0; i < width; i += 8)
        {
            for (e = 0; e < 2; e++)
            {
                m_edge[0][(i >> 3)+1][e].strength = 0;
            }
        }
    }

    H265CUPtr leftLCU;
    getPULeft(&leftLCU, 0, 0, false, 0);

    if (leftLCU.ctb_data_ptr)
    {
        for (j = 0; j < height; j += 8)
        {
            for (e = 2; e < 4; e++)
            {
                curPixelColumn = (maxCUSize - 8 + 4 * (e - 2));
                curPixelRow = j;

                GetEdgeStrength(&leftLCU, &edge, curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset, HOR_FILT);

                m_edge[(j >> 3)+1][0][e] = edge;
            }
        }
    }
    else
    {
        for (j = 0; j < (height >> 3); j++)
        {
            m_edge[j+1][0][2].strength = 0;
            m_edge[j+1][0][3].strength = 0;
        }
    }
    H265CUPtr curLCU;
    curLCU.ctb_data_ptr = data;
    curLCU.ctb_addr = ctb_addr;
    curLCU.abs_part_idx = 0;

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
            for (e = 0; e < 4; e++)
            {
                if (e < 2)
                {
                    curPixelColumn = i;
                    curPixelRow = (j + 4 * e);
                    dir = VERT_FILT;
                }
                else
                {
                    curPixelColumn = (i + 4 * (e - 2));
                    curPixelRow = j;
                    dir = HOR_FILT;
                }

                GetEdgeStrength(&curLCU, &edge, curPixelColumn, curPixelRow,
                                crossSliceBoundaryFlag, crossTileBoundaryFlag,
                                tcOffset, betaOffset, dir);

                m_edge[(j >> 3)+1][(i >> 3)+1][e] = edge;
            }
        }
    }
}

void H265CU::Deblock()
{
    Ipp32s maxCUSize = par->MaxCUSize;
    Ipp32s frameHeightInSamples = par->Height;
    Ipp32s frameWidthInSamples = par->Width;
    Ipp32s width, height;
    Ipp32s i, j;

    width = frameWidthInSamples - ctb_pelx;

    if (width > maxCUSize)
    {
        width = maxCUSize;
    }

    height = frameHeightInSamples - ctb_pely;

    if (height > maxCUSize)
    {
        height = maxCUSize;
    }

    SetEdges(width, height);

    for (j = 0; j < height; j += 8)
    {
        for (i = 0; i < width; i += 8)
        {
            DeblockOneCrossLuma(i, j);
        }
    }

    for (j = 0; j < height; j += 16)
    {
        for (i = 0; i < width; i += 16)
        {
            DeblockOneCrossChroma(i, j);
        }
    }
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
