// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder.h"
#include "umc_h264_dec_deblocking.h"

namespace UMC
{

#define IsVectorDifferenceBig(one, two, iMaxDiff) \
    /* the original code is \
    ((4 <= abs(one.mvx - two.mvx)) || \
     (iMaxDiff <= abs(one.mvy - two.mvy))); \
    but the current code is equal and uses less comparisons */ \
    ((7 <= (uint32_t)((one).mvx - (two).mvx + 3)) || \
     ((uint32_t)(iMaxDiff * 2 - 1) <= (uint32_t)((one).mvy - (two).mvy + iMaxDiff - 1)))

void H264SegmentDecoder::PrepareDeblockingParametersISlice()
{
    // set deblocking flag(s)
    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = 1;
    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = 1;

    // calculate strengths
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        // deblocking with strong deblocking of external edge
        SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 0, 4);
    }

    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(m_deblockingParams.Strength[VERTICAL_DEBLOCKING] + 12, 3);

    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        if (m_deblockingParams.MBFieldCoded)
        {
            // deblocking field macroblock with external edge
            SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 3);
        }
        else
        {
            // deblocking with strong deblocking of external edge
            SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 0, 4);
        }
    }

    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 4, 3);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 8, 3);
    SetEdgeStrength(m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING] + 12, 3);

} // void H264SegmentDecoder::PrepareDeblockingParametersISlice()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice()
{
    int32_t mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        PrepareDeblockingParametersISlice();
        return;
    }

    if (!m_deblockingParams.m_isSameSlice)
    {
        PrepareDeblockingParametersBSlice();
        return;
    }

    // try simplest function to prepare deblocking parameters
    switch (mbtype)
    {
        // when macroblock has type inter 16 on 16
    case MBTYPE_INTER:
    case MBTYPE_FORWARD:
    case MBTYPE_BACKWARD:
    case MBTYPE_BIDIR:
        PrepareDeblockingParametersPSlice16x16Vert();
        PrepareDeblockingParametersPSlice16x16Horz();
        break;
/*
        // when macroblock has type inter 16 on 8
    case MBTYPE_INTER_16x8:
        PrepareDeblockingParametersPSlice8x16(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersPSlice16x8(HORIZONTAL_DEBLOCKING);
        return;

        // when macroblock has type inter 8 on 16
    case MBTYPE_INTER_8x16:
        PrepareDeblockingParametersPSlice16x8(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersPSlice8x16(HORIZONTAL_DEBLOCKING);
        return;
*/
    default:
        PrepareDeblockingParametersPSlice4(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersPSlice4(HORIZONTAL_DEBLOCKING);
        break;
    }

    union
    {
        uint8_t *Strength;
        uint32_t *edge;
    } u;

    u.Strength = m_deblockingParams.Strength[VERTICAL_DEBLOCKING];
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] && !(*u.edge))
    {
        m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
    }

    u.Strength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];
    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] && !(*u.edge))
    {
        m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice()

void H264SegmentDecoder::PrepareDeblockingParametersBSlice()
{
    int32_t mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;
    int32_t af = 0;

    // when this macroblock is intra coded
    if (IS_INTRA_MBTYPE(mbtype))
    {
        PrepareDeblockingParametersISlice();
        return;
    }

    // try simplest function to prepare deblocking parameters
    switch (mbtype)
    {
        // when macroblock has type inter 16 on 16
    case MBTYPE_INTER:
    case MBTYPE_FORWARD:
    case MBTYPE_BACKWARD:
    case MBTYPE_BIDIR:
        PrepareDeblockingParametersBSlice16x16Vert();
        PrepareDeblockingParametersBSlice16x16Horz();
        af = 1;
        break;

        // when macroblock has type inter 16 on 8
    case MBTYPE_INTER_16x8:
        PrepareDeblockingParametersBSlice8x16(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersBSlice16x8(HORIZONTAL_DEBLOCKING);
        break;

        // when macroblock has type inter 8 on 16
    case MBTYPE_INTER_8x16:
        PrepareDeblockingParametersBSlice16x8(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersBSlice8x16(HORIZONTAL_DEBLOCKING);
        break;

    default:
        PrepareDeblockingParametersBSlice4(VERTICAL_DEBLOCKING);
        PrepareDeblockingParametersBSlice4(HORIZONTAL_DEBLOCKING);
        af = 1;
        break;
    }

    if (af) {
        union
        {
            uint8_t *Strength;
            uint32_t *edge;
        } u;
        u.Strength = m_deblockingParams.Strength[VERTICAL_DEBLOCKING];
        if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] && !(*u.edge))
        {
            m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING] = 0;
        }

        u.Strength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];
        if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] && !(*u.edge))
        {
            m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING] = 0;
        }
    }
} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice()

enum
{
    VERTICAL_OUTER_EDGE_MASK    = 0x00a0a,

    VERTICAL_OUTER_EDGE_BLOCK_0 = 0x00002,
    VERTICAL_OUTER_EDGE_BLOCK_1 = 0x00008,
    VERTICAL_OUTER_EDGE_BLOCK_2 = 0x00200,
    VERTICAL_OUTER_EDGE_BLOCK_3 = 0x00800,

    HORIZONTAL_OUTER_EDGE_MASK  = 0x00066,

    HORIZONTAL_OUTER_EDGE_BLOCK_0 = 0x00002,
    HORIZONTAL_OUTER_EDGE_BLOCK_1 = 0x00004,
    HORIZONTAL_OUTER_EDGE_BLOCK_2 = 0x00020,
    HORIZONTAL_OUTER_EDGE_BLOCK_3 = 0x00040
};

static
uint8_t InternalBlockDeblockingTable[2][16][4] =
{
    // strength arrays for vertical deblocking
    {
        {0, 0, 0, 0},
        {2, 0, 0, 0},
        {0, 0, 2, 0},
        {2, 0, 2, 0},
        {0, 2, 0, 0},
        {2, 2, 0, 0},
        {0, 2, 2, 0},
        {2, 2, 2, 0},
        {0, 0, 0, 2},
        {2, 0, 0, 2},
        {0, 0, 2, 2},
        {2, 0, 2, 2},
        {0, 2, 0, 2},
        {2, 2, 0, 2},
        {0, 2, 2, 2},
        {2, 2, 2, 2}
    },

    // strength arrays for horizontal deblocking
    {
        {0, 0, 0, 0},
        {2, 0, 0, 0},
        {0, 2, 0, 0},
        {2, 2, 0, 0},
        {0, 0, 2, 0},
        {2, 0, 2, 0},
        {0, 2, 2, 0},
        {2, 2, 2, 0},
        {0, 0, 0, 2},
        {2, 0, 0, 2},
        {0, 2, 0, 2},
        {2, 2, 0, 2},
        {0, 0, 2, 2},
        {2, 0, 2, 2},
        {0, 2, 2, 2},
        {2, 2, 2, 2}
    }
};

void H264SegmentDecoder::PrepareDeblockingParametersIntern16x16Vert()
{
    //foo_internal(pParams, VERTICAL_DEBLOCKING);
    //return;

    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[VERTICAL_DEBLOCKING];

    //
    // internal edge(s)
    //
    if (cbp4x4_luma & 0x1fffe)
    {
        int32_t a, b, res;

        // set deblocking flag
        m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = 1;

        // deblocking strength depends on CBP only
        // we use fast CBP comparison
        a = cbp4x4_luma >> 1;
        b = cbp4x4_luma >> 2;
        res = (a | b) & 0x505;
        res = (res & 0x05) | (res >> 7);
        CopyEdgeStrength(pStrength + 4, InternalBlockDeblockingTable[VERTICAL_DEBLOCKING][res]);

        a = cbp4x4_luma >> 5;
        res = (a | b) & 0x505;
        res = (res & 0x05) | (res >> 7);
        CopyEdgeStrength(pStrength + 8, InternalBlockDeblockingTable[VERTICAL_DEBLOCKING][res]);

        b = cbp4x4_luma >> 6;
        res = (a | b) & 0x505;
        res = (res & 0x05) | (res >> 7);
        CopyEdgeStrength(pStrength + 12, InternalBlockDeblockingTable[VERTICAL_DEBLOCKING][res]);
    }
    else
    {
        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersIntern16x16Vert()

void H264SegmentDecoder::PrepareDeblockingParametersIntern16x16Horz()
{
    //foo_internal(pParams, HORIZONTAL_DEBLOCKING);
    //return;
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];

    //
    // internal edge(s)
    //
    if (cbp4x4_luma & 0x1fffe)
    {
        int32_t a, b, res;

        // set deblocking flag
        m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] |= 1;

        // deblocking strength depends on CBP only
        // we use fast CBP comparison
        a = cbp4x4_luma >> 1;
        b = cbp4x4_luma >> 3;
        res = (a | b) & 0x33;
        res = (res & 0x03) | (res >> 2);
        CopyEdgeStrength(pStrength + 4, InternalBlockDeblockingTable[HORIZONTAL_DEBLOCKING][res]);

        a = cbp4x4_luma >> 9;
        res = (a | b) & 0x33;
        res = (res & 0x03) | (res >> 2);
        CopyEdgeStrength(pStrength + 8, InternalBlockDeblockingTable[HORIZONTAL_DEBLOCKING][res]);

        b = cbp4x4_luma >> 11;
        res = (a | b) & 0x33;
        res = (res & 0x03) | (res >> 2);
        CopyEdgeStrength(pStrength + 12, InternalBlockDeblockingTable[HORIZONTAL_DEBLOCKING][res]);
    }
    else
    {
        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersIntern16x16Horz()

inline void H264SegmentDecoder::foo_external_p(int32_t cbp4x4_luma, int32_t nNeighbour, int32_t dir,
    int32_t idx,
    H264DecoderFrame **pNRefPicList0,
    ReferenceFlags *pNFields0)
{
    int32_t blkQ, blkP;
    int32_t blkQ2, blkP2;

    H264DecoderMacroblockLocalInfo *pNeighbour = &m_mbinfo.mbs[nNeighbour];
    int32_t nBlock, nNeighbourBlock;

    blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
    blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

    blkQ2 = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx + 1];
    blkP2 = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx + 1];

    bool is_first = (cbp4x4_luma & blkQ) ||
        (pNeighbour->cbp4x4_luma & blkP);

    bool is_second = (cbp4x4_luma & blkQ2) ||
        (pNeighbour->cbp4x4_luma & blkP2);

    if (is_first && is_second)
    {
        m_deblockingParams.Strength[dir][idx] = 2;
        m_deblockingParams.Strength[dir][idx + 1] = 2;
        m_deblockingParams.DeblockingFlag[dir] = 1;
        return;
    }

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = idx * 4;
        nNeighbourBlock = nBlock + 3;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx + 12;
    }

    size_t iRefQFrw, iRefPFrw;

    // field coded image
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        int32_t index;

        // select reference index for current block
        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;

        // select reference index for previous block
        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
        iRefPFrw = pNRefPicList0[index] ? pNRefPicList0[index]->DeblockPicID(GetReferenceField(pNFields0, index)) : (size_t)-1;
    }
    // frame coded image
    else
    {
        int32_t index;

        // select reference index for current block
        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;

        // select reference index for previous block
        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
        iRefPFrw = pNRefPicList0[index] ? pNRefPicList0[index]->DeblockPicID(0) : (size_t)-1;
    }

    // when reference indexes are equal
    if (iRefQFrw == iRefPFrw)
    {
        if (!is_first)
        {
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);

            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector))
            {
                m_deblockingParams.Strength[dir][idx] = 1;
                m_deblockingParams.DeblockingFlag[dir] = 1;
            }
            else
            {
                m_deblockingParams.Strength[dir][idx] = 0;
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }

        idx++;

        // when one of couple of blocks has coeffs
        if (!is_second)
        {
            // calc block and neighbour block number
            if (VERTICAL_DEBLOCKING == dir)
            {
                nBlock = idx * 4;
                nNeighbourBlock = nBlock + 3;
            }
            else
            {
                nBlock = idx;
                nNeighbourBlock = idx + 12;
            }

            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);

            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector))
            {
                m_deblockingParams.Strength[dir][idx] = 1;
                m_deblockingParams.DeblockingFlag[dir] = 1;
            }
            // when forward and backward reference pictures of previous block are equal
            else
            {
                m_deblockingParams.Strength[dir][idx] = 0;
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }
    }
    // when reference indexes are different
    else
    {
        m_deblockingParams.Strength[dir][idx] = (uint8_t) (1 + (is_first ? 1 : 0));
        m_deblockingParams.Strength[dir][idx + 1] = (uint8_t) (1 + (is_second ? 1 : 0));
        m_deblockingParams.DeblockingFlag[dir] = 1;
    }
}

inline void H264SegmentDecoder::foo_internal_p(int32_t dir,
    int32_t idx, uint32_t cbp4x4_luma)
{
    size_t iRefQFrw, iRefPFrw;

    int32_t nBlock, nNeighbourBlock;

    int32_t blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];
    int32_t blkQ2 = INTERNAL_BLOCKS_MASK[dir][idx - 3];

    if ((cbp4x4_luma & blkQ) && (cbp4x4_luma & blkQ2))
    {
        m_deblockingParams.Strength[dir][idx] = 2;
        m_deblockingParams.Strength[dir][idx + 1] = 2;
        m_deblockingParams.DeblockingFlag[dir] = 1;
        return;
    }

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = (idx & 3) * 4 + (idx >> 2);
        nNeighbourBlock = nBlock - 1;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx - 4;
    }

    // field coded image
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        int32_t index;

        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
        iRefPFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;
    }
    else  // frame coded image
    {
        int32_t index;

        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
        iRefPFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;
    }

    // when reference indexes are equal
    if (iRefQFrw == iRefPFrw)
    {
        if (!(cbp4x4_luma & blkQ))
        {
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);

            // compare motion vectors
            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector))
            {
                m_deblockingParams.Strength[dir][idx] = 1;
                m_deblockingParams.DeblockingFlag[dir] = 1;
            }
            else
            {
                m_deblockingParams.Strength[dir][idx] = 0;
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }

        if (!(cbp4x4_luma & blkQ2))
        {
            idx++;
            // calc block and neighbour block number
            if (VERTICAL_DEBLOCKING == dir)
            {
                nBlock = (idx & 3) * 4 + (idx >> 2);
                nNeighbourBlock = nBlock - 1;
            }
            else
            {
                nBlock = idx;
                nNeighbourBlock = idx - 4;
            }

            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);

            // compare motion vectors
            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector))
            {
                m_deblockingParams.Strength[dir][idx] = 1;
                m_deblockingParams.DeblockingFlag[dir] = 1;
            }
            else
            {
                m_deblockingParams.Strength[dir][idx] = 0;
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx + 1] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }
    }
    // when reference indexes are different
    else
    {
        m_deblockingParams.Strength[dir][idx] = (uint8_t) (1 + ((cbp4x4_luma & blkQ) ? 1 : 0));
        m_deblockingParams.Strength[dir][idx + 1] = (uint8_t) (1 + ((cbp4x4_luma & blkQ2) ? 1 : 0));
        m_deblockingParams.DeblockingFlag[dir] = 1;
    }
}

void H264SegmentDecoder::PrepareDeblockingParametersPSlice4(uint32_t dir)
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    int32_t *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);

    //
    // external edge
    //

    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        // select neighbour addres
        int32_t nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nNeighbour].mbtype))
        {
            H264DecoderFrame **pNRefPicList0 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 0)->m_RefPicList;
            ReferenceFlags *pNFields0 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 0)->m_Flags;

            // cicle on blocks
            for (int32_t idx = 0; idx < 4; idx += 2)
            {
                foo_external_p(cbp4x4_luma, nNeighbour, dir,
                    idx,
                    pNRefPicList0, pNFields0);
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (m_deblockingParams.MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //
    // internal edge(s)
    //
    {
        // cicle of edge(s)
        // we do all edges in one cicle
        for (int32_t idx = 4; idx < 16; idx += 2)
        {
            foo_internal_p(dir,
                idx, cbp4x4_luma);
        }
    }
} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice4(uint32_t dir)

#define SET_DEBLOCKING_STRENGTH_P_SLICE_VERTICAL(block_num0, block_num1) \
{ \
    size_t refPrev = GetReferencePSlice(nNeighbour, block_num0 * 4 + 3); \
    if (refCur == refPrev) \
    { \
        if (0 == (VERTICAL_OUTER_EDGE_BLOCK_##block_num0 & uMask)) \
        { \
            H264DecoderMotionVector &pMVPrev = GetMV(m_gmbinfo, 0, nNeighbour, block_num0 * 4 + 3); \
            if (IsVectorDifferenceBig(mvCur, pMVPrev, m_deblockingParams.nMaxMVector)) \
            { \
                pStrength[block_num0] = 1; \
                iDeblockingFlag = 1; \
            } \
            else \
                pStrength[block_num0] = 0; \
        } \
        else \
        { \
            pStrength[block_num0] = 2; \
            iDeblockingFlag = 1; \
        } \
        if (0 == (VERTICAL_OUTER_EDGE_BLOCK_##block_num1 & uMask)) \
        { \
            H264DecoderMotionVector &pMVPrev = GetMV(m_gmbinfo, 0, nNeighbour, block_num1 * 4 + 3); \
            if (IsVectorDifferenceBig(mvCur, pMVPrev, m_deblockingParams.nMaxMVector)) \
            { \
                pStrength[block_num1] = 1; \
                iDeblockingFlag = 1; \
            } \
            else \
                pStrength[block_num1] = 0; \
        } \
        else \
        { \
            pStrength[block_num1] = 2; \
            iDeblockingFlag = 1; \
        } \
    } \
    else \
    { \
        pStrength[block_num0] = (uint8_t) (1 + ((VERTICAL_OUTER_EDGE_BLOCK_##block_num0 & uMask) ? (1) : (0))); \
        pStrength[block_num1] = (uint8_t) (1 + ((VERTICAL_OUTER_EDGE_BLOCK_##block_num1 & uMask) ? (1) : (0))); \
        iDeblockingFlag = 1; \
    } \
}

#define SET_DEBLOCKING_STRENGTH_P_SLICE_HORIZONTAL(block_num0, block_num1) \
{ \
    size_t refPrev = GetReferencePSlice(nNeighbour, block_num0 + 12); \
    if (refCur == refPrev) \
    { \
        if (0 == (HORIZONTAL_OUTER_EDGE_BLOCK_##block_num0 & uMask)) \
        { \
            H264DecoderMotionVector &pMVPrev = GetMV(m_gmbinfo, 0, nNeighbour, block_num0 + 12); \
            if (IsVectorDifferenceBig(mvCur, pMVPrev, m_deblockingParams.nMaxMVector)) \
            { \
                pStrength[block_num0] = 1; \
                iDeblockingFlag = 1; \
            } \
            else \
                pStrength[block_num0] = 0; \
        } \
        else \
        { \
            pStrength[block_num0] = 2; \
            iDeblockingFlag = 1; \
        } \
        if (0 == (HORIZONTAL_OUTER_EDGE_BLOCK_##block_num1 & uMask)) \
        { \
            H264DecoderMotionVector &pMVPrev = GetMV(m_gmbinfo, 0, nNeighbour, block_num1 + 12); \
            if (IsVectorDifferenceBig(mvCur, pMVPrev, m_deblockingParams.nMaxMVector)) \
            { \
                pStrength[block_num1] = 1; \
                iDeblockingFlag = 1; \
            } \
            else \
                pStrength[block_num1] = 0; \
        } \
        else \
        { \
            pStrength[block_num1] = 2; \
            iDeblockingFlag = 1; \
        } \
    } \
    else \
    { \
        pStrength[block_num0] = (uint8_t) (1 + ((HORIZONTAL_OUTER_EDGE_BLOCK_##block_num0 & uMask) ? (1) : (0))); \
        pStrength[block_num1] = (uint8_t) (1 + ((HORIZONTAL_OUTER_EDGE_BLOCK_##block_num1 & uMask) ? (1) : (0))); \
        iDeblockingFlag = 1; \
    } \
}

inline
size_t H264SegmentDecoder::GetReferencePSlice(int32_t iMBNum, int32_t iBlockNum)
{
    int32_t index;
    size_t ref;

    // select reference index for current block
    index = GetReferenceIndex(m_gmbinfo, 0, iMBNum, iBlockNum);
    if (0 <= index)
    {
        H264DecoderFrame **pRefPicList;
        H264DecoderMacroblockGlobalInfo *pMBInfo;
        int32_t iNum = 0;

        // obtain reference list
        pMBInfo = m_gmbinfo->mbs + iMBNum;
        pRefPicList = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 0)->m_RefPicList;
        if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
        {
            ReferenceFlags *pFields;

            pFields = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 0)->m_Flags;
            iNum = GetReferenceField(pFields, index);
        }
        ref = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(iNum) : (size_t)-1;
    }
    else
        ref = (size_t)-1;

    return ref;

} // int32_t H264SegmentDecoder::GetReferencePSlice(int32_t iMBNum, int32_t iBlockNum)

inline void H264SegmentDecoder::GetReferencesBCurMB(int32_t iBlockNum, size_t *pReferences)
{
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        int32_t index = m_cur_mb.GetReferenceIndex(0, iBlockNum);
        pReferences[0] = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(1, iBlockNum);
        pReferences[1] = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(GetReferenceField(m_pFields[1], index)) : (size_t)-1;
    }
    else
    {
        int32_t index = m_cur_mb.GetReferenceIndex(0, iBlockNum);
        pReferences[0] = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(1, iBlockNum);
        pReferences[1] = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(0) : (size_t)-1;
    }
}

inline void H264SegmentDecoder::GetReferencesBSlice(int32_t iMBNum, int32_t iBlockNum, size_t *pReferences)
{
    H264DecoderMacroblockGlobalInfo *pMBInfo = m_gmbinfo->mbs + iMBNum;

    H264DecoderFrame **pRefPicList0 = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 0)->m_RefPicList;
    H264DecoderFrame **pRefPicList1 = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 1)->m_RefPicList;

    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        ReferenceFlags *pFields = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 0)->m_Flags;
        int32_t index = GetReferenceIndex(m_gmbinfo, 0, iMBNum, iBlockNum);
        pReferences[0] = pRefPicList0[index] ? pRefPicList0[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;

        pFields = m_pCurrentFrame->GetRefPicList(pMBInfo->slice_id, 1)->m_Flags;
        index = GetReferenceIndex(m_gmbinfo, 1, iMBNum, iBlockNum);
        pReferences[1] = pRefPicList1[index] ? pRefPicList1[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
    }
    else
    {
        int32_t index = GetReferenceIndex(m_gmbinfo, 0, iMBNum, iBlockNum);
        pReferences[0] = pRefPicList0[index] ? pRefPicList0[index]->DeblockPicID(0) : (size_t)-1;

        index = GetReferenceIndex(m_gmbinfo, 1, iMBNum, iBlockNum);
        pReferences[1] = pRefPicList1[index] ? pRefPicList1[index]->DeblockPicID(0) : (size_t)-1;
    }
} // int32_t H264SegmentDecoder::GetReferencesBSlice(int32_t iMBNum, int32_t iBlockNum, int32_t *pReferences)

void H264SegmentDecoder::PrepareDeblockingParametersPSlice16x16Vert()
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[VERTICAL_DEBLOCKING];
    int32_t iDeblockingFlag = 0;

    //
    // precalculating of reference numbers of current macroblock.
    // it is more likely we have to compare reference numbers.
    // we will use it also in horizontal deblocking
    //
    m_deblockingParams.iReferences[0][0] = GetReferencePSlice(m_CurMBAddr, 0);

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[VERTICAL_DEBLOCKING];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            uint32_t uMask;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // create special mask
            uMask = cbp4x4_luma | (pNeighbour->cbp4x4_luma >> 5);

            // when at least one from a couple of blocks has coeffs
            if (VERTICAL_OUTER_EDGE_MASK != (uMask & VERTICAL_OUTER_EDGE_MASK))
            {
                // obtain current block parameters
                size_t refCur = m_deblockingParams.iReferences[0][0];
                H264DecoderMotionVector  &mvCur = m_cur_mb.GetMV(0, 0);

                SET_DEBLOCKING_STRENGTH_P_SLICE_VERTICAL(0, 1)

                SET_DEBLOCKING_STRENGTH_P_SLICE_VERTICAL(2, 3)
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 2);
                iDeblockingFlag = 1;
            }
        }
        else
        {
            SetEdgeStrength(pStrength + 0, 4);
            iDeblockingFlag = 1;
        }
    }

    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = iDeblockingFlag;

    //
    // internal edge(s)
    //
    PrepareDeblockingParametersIntern16x16Vert();

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice16x16Vert()

void H264SegmentDecoder::PrepareDeblockingParametersPSlice16x16Horz()
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];
    int32_t iDeblockingFlag = 0;

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            uint32_t uMask;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // create special mask
            uMask = cbp4x4_luma | (pNeighbour->cbp4x4_luma >> 10);

            // when at least one from a couple of blocks has coeffs
            if (HORIZONTAL_OUTER_EDGE_MASK != (uMask & HORIZONTAL_OUTER_EDGE_MASK))
            {
                // obtain current block parameters
                size_t refCur = m_deblockingParams.iReferences[0][0];
                H264DecoderMotionVector &mvCur = m_cur_mb.GetMV(0, 0);

                SET_DEBLOCKING_STRENGTH_P_SLICE_HORIZONTAL(0, 1)

                SET_DEBLOCKING_STRENGTH_P_SLICE_HORIZONTAL(2, 3)
            }
            // when at least one from a couple of blocks has coeffs
            else
            {
                SetEdgeStrength(pStrength + 0, 2);
                iDeblockingFlag = 1;
            }
        }
        else
        {
            if (m_deblockingParams.MBFieldCoded)
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            iDeblockingFlag = 1;
        }
    }

    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = iDeblockingFlag;

    //
    // internal edge(s)
    //
    PrepareDeblockingParametersIntern16x16Horz();

} // void H264SegmentDecoder::PrepareDeblockingParametersPSlice16x16Horz()

inline void H264SegmentDecoder::EvaluateStrengthExternal(int32_t cbp4x4_luma, int32_t nNeighbour, int32_t dir,
    int32_t idx,
    H264DecoderFrame **pNRefPicList0,
    ReferenceFlags *pNFields0,
    H264DecoderFrame **pNRefPicList1,
    ReferenceFlags *pNFields1)
{
    int32_t blkQ, blkP;
    int32_t blkQ2, blkP2;

    H264DecoderMacroblockLocalInfo *pNeighbour = &m_mbinfo.mbs[nNeighbour];
    int32_t nBlock, nNeighbourBlock;

    blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
    blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

    blkQ2 = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx + 1];
    blkP2 = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx + 1];

    bool is_first = (cbp4x4_luma & blkQ) ||
        (pNeighbour->cbp4x4_luma & blkP);

    bool is_second = (cbp4x4_luma & blkQ2) ||
        (pNeighbour->cbp4x4_luma & blkP2);

    if (is_first && is_second)
    {
        m_deblockingParams.Strength[dir][idx] = 2;
        m_deblockingParams.Strength[dir][idx + 1] = 2;
        m_deblockingParams.DeblockingFlag[dir] = 1;
        return;
    }

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = idx * 4;
        nNeighbourBlock = nBlock + 3;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx + 12;
    }

    size_t iRefQFrw, iRefPFrw, iRefQBck, iRefPBck;

    iRefQFrw = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][0];
    iRefQBck = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][1];

    // field coded image
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        int32_t index;

        // select reference index for previous block
        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
        iRefPFrw = pNRefPicList0[index] ? pNRefPicList0[index]->DeblockPicID(GetReferenceField(pNFields0, index)) : (size_t)-1;
        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
        iRefPBck = pNRefPicList1[index] ? pNRefPicList1[index]->DeblockPicID(GetReferenceField(pNFields1, index)) : (size_t)-1;
    }
    // frame coded image
    else
    {
        int32_t index;
        // select reference index for previous block
        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
        iRefPFrw = pNRefPicList0[index] ? pNRefPicList0[index]->DeblockPicID(0) : (size_t)-1;

        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
        iRefPBck = pNRefPicList1[index] ? pNRefPicList1[index]->DeblockPicID(0) : (size_t)-1;
    }

    // when reference indexes are equal
    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
    {
        if (!is_first)
        {
            m_deblockingParams.Strength[dir][idx] = 0;

            // select current block motion vectors
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

            // select previous block motion vectors
            H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
            H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

            // when forward and backward reference pictures of previous block are different
            if (iRefPFrw != iRefPBck)
            {
                if (iRefQFrw != iRefPFrw)
                {
                    swapValues(pVectorPFrw, pVectorPBck);
                }

                if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
            // when forward and backward reference pictures of previous block are equal
            else
            {
                if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
                    (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }

        idx++;

        // when one of couple of blocks has coeffs
        if (!is_second)
        {
            // calc block and neighbour block number
            if (VERTICAL_DEBLOCKING == dir)
            {
                nBlock = idx * 4;
                nNeighbourBlock = nBlock + 3;
            }
            else
            {
                nBlock = idx;
                nNeighbourBlock = idx + 12;
            }

            // select current block motion vectors
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

            // select previous block motion vectors
            H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
            H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

            m_deblockingParams.Strength[dir][idx] = 0;

            // when forward and backward reference pictures of previous block are different
            if (iRefPFrw != iRefPBck)
            {
                if (iRefQFrw != iRefPFrw)
                {
                    swapValues(pVectorPFrw, pVectorPBck);
                }

                if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
            // when forward and backward reference pictures of previous block are equal
            else
            {
                if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
                    (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }
    }
    // when reference indexes are different
    else
    {
        m_deblockingParams.Strength[dir][idx] = (uint8_t) (1 + (is_first ? 1 : 0));
        m_deblockingParams.Strength[dir][idx + 1] = (uint8_t) (1 + (is_second ? 1 : 0));
        m_deblockingParams.DeblockingFlag[dir] = 1;
    }
}

inline void H264SegmentDecoder::foo_internal(int32_t dir,
    int32_t idx, uint32_t cbp4x4_luma)
{
    size_t iRefQFrw, iRefQBck, iRefPFrw, iRefPBck;

    int32_t nBlock, nNeighbourBlock;

    int32_t blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];
    int32_t blkQ2 = INTERNAL_BLOCKS_MASK[dir][idx - 3];

    if ((cbp4x4_luma & blkQ) && (cbp4x4_luma & blkQ2))
    {
        m_deblockingParams.Strength[dir][idx] = 2;
        m_deblockingParams.Strength[dir][idx + 1] = 2;
        m_deblockingParams.DeblockingFlag[dir] = 1;
        return;
    }

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = (idx & 3) * 4 + (idx >> 2);
        nNeighbourBlock = nBlock - 1;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx - 4;
    }

    // field coded image
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        int32_t index;

        // select forward reference index for blocks
        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
        iRefPFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;

        // select backward reference index for blocks
        index = m_cur_mb.GetReferenceIndex(1, nBlock);
        iRefQBck = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(GetReferenceField(m_pFields[1], index)) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(1, nNeighbourBlock);
        iRefPBck = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(GetReferenceField(m_pFields[1], index)) : (size_t)-1;
    }
    else  // frame coded image
    {
        int32_t index;

        index = m_cur_mb.GetReferenceIndex(0, nBlock);
        iRefQFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(0, nNeighbourBlock);
        iRefPFrw = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;

        index = m_cur_mb.GetReferenceIndex(1, nBlock);
        iRefQBck = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(0) : (size_t)-1;
        index = m_cur_mb.GetReferenceIndex(1, nNeighbourBlock);
        iRefPBck = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(0) : (size_t)-1;
    }

    // when reference indexes are equal
    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
    {
        if (!(cbp4x4_luma & blkQ))
        {
            // select current block motion vectors
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

            // select previous block motion vectors
            H264DecoderMotionVector pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);
            H264DecoderMotionVector pVectorPBck = m_cur_mb.GetMV(1, nNeighbourBlock);

            // set initial value of strength
            m_deblockingParams.Strength[dir][idx] = 0;

            // when forward and backward reference pictures of previous block are different
            if (iRefPFrw != iRefPBck)
            {
                // select previous block motion vectors
                if (iRefQFrw != iRefPFrw)
                {
                    swapValues(pVectorPFrw, pVectorPBck);
                }

                // compare motion vectors
                if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }

            }
            // when forward and backward reference pictures of previous block are equal
            else
            {
                // compare motion vectors
                if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
                    (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }

        if (!(cbp4x4_luma & blkQ2))
        {
            idx++;
            // calc block and neighbour block number
            if (VERTICAL_DEBLOCKING == dir)
            {
                nBlock = (idx & 3) * 4 + (idx >> 2);
                nNeighbourBlock = nBlock - 1;
            }
            else
            {
                nBlock = idx;
                nNeighbourBlock = idx - 4;
            }

            // set initial value of strength
            m_deblockingParams.Strength[dir][idx] = 0;

            // select current block motion vectors
            H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
            H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

            // select previous block motion vectors
            H264DecoderMotionVector pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);
            H264DecoderMotionVector pVectorPBck = m_cur_mb.GetMV(1, nNeighbourBlock);

            // when forward and backward reference pictures of previous block are different
            if (iRefPFrw != iRefPBck)
            {
                // select previous block motion vectors
                if ((iRefQFrw != iRefPFrw) && (cbp4x4_luma & blkQ))
                {
                    swapValues(pVectorPFrw, pVectorPBck);
                }

                // compare motion vectors
                if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
            // when forward and backward reference pictures of previous block are equal
            else
            {
                // compare motion vectors
                if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
                    (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                    IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
                {
                    m_deblockingParams.Strength[dir][idx] = 1;
                    m_deblockingParams.DeblockingFlag[dir] = 1;
                }
            }
        }
        else
        {
            m_deblockingParams.Strength[dir][idx + 1] = 2;
            m_deblockingParams.DeblockingFlag[dir] = 1;
        }
    }
    // when reference indexes are different
    else
    {
        m_deblockingParams.Strength[dir][idx] = (uint8_t) (1 + ((cbp4x4_luma & blkQ) ? 1 : 0));
        m_deblockingParams.Strength[dir][idx + 1] = (uint8_t) (1 + ((cbp4x4_luma & blkQ2) ? 1 : 0));
        m_deblockingParams.DeblockingFlag[dir] = 1;
    }
}

inline uint8_t H264SegmentDecoder::EvaluateStrengthInternal(int32_t dir, int32_t idx)
{
    int32_t nBlock, nNeighbourBlock;

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = VERTICAL_DEBLOCKING_BLOCKS_MAP[idx];
        nNeighbourBlock = nBlock - 1;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx - 4;
    }

    size_t iRefQFrw = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][0];
    size_t iRefQBck = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][1];
    size_t iRefPFrw = m_deblockingParams.iReferences[subblock_block_membership[nNeighbourBlock]][0];
    size_t iRefPBck = m_deblockingParams.iReferences[subblock_block_membership[nNeighbourBlock]][1];

    // when reference indexes are equal
    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
    {
        // select current block motion vectors
        H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
        H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

        // select previous block motion vectors
        H264DecoderMotionVector pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);
        H264DecoderMotionVector pVectorPBck = m_cur_mb.GetMV(1, nNeighbourBlock);

        // when forward and backward reference pictures of previous block are different
        if (iRefPFrw != iRefPBck)
        {
            // select previous block motion vectors
            if (iRefQFrw != iRefPFrw)
            {
                swapValues(pVectorPFrw, pVectorPBck);
            }

            // compare motion vectors
            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
            {
                m_deblockingParams.DeblockingFlag[dir] = 1;
                return 1;
            }

        }
        // when forward and backward reference pictures of previous block are equal
        else
        {
            // compare motion vectors
            if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
                (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
            {
                m_deblockingParams.DeblockingFlag[dir] = 1;
                return 1;
            }
        }

        return 0;
    }
    // when reference indexes are different
    else
    {
        m_deblockingParams.DeblockingFlag[dir] = 1;
        return 1;
    }
}

inline uint8_t H264SegmentDecoder::EvaluateStrengthInternal8x8(int32_t dir, int32_t idx)
{
    int32_t nBlock, nNeighbourBlock;

    // calc block and neighbour block number
    if (VERTICAL_DEBLOCKING == dir)
    {
        nBlock = VERTICAL_DEBLOCKING_BLOCKS_MAP[idx];
        nNeighbourBlock = nBlock - 1;
    }
    else
    {
        nBlock = idx;
        nNeighbourBlock = idx - 4;
    }

    // select current block motion vectors
    H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, nBlock);
    H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, nBlock);

    // select previous block motion vectors
    H264DecoderMotionVector &pVectorPFrw = m_cur_mb.GetMV(0, nNeighbourBlock);
    H264DecoderMotionVector &pVectorPBck = m_cur_mb.GetMV(1, nNeighbourBlock);

    size_t iRefQFrw = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][0];
    size_t iRefQBck = m_deblockingParams.iReferences[subblock_block_membership[nBlock]][1];

    // when forward and backward reference pictures of previous block are different
    if (iRefQFrw != iRefQBck)
    {
        // compare motion vectors
        if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
            IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
        {
            m_deblockingParams.DeblockingFlag[dir] = 1;
            return 1;
        }
    }
    // when forward and backward reference pictures of previous block are equal
    else
    {
        // compare motion vectors
        if ((IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
            IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector)) &&
            (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
            IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector)))
        {
            m_deblockingParams.DeblockingFlag[dir] = 1;
            return 1;
        }
    }

    return 0;
}

inline void H264SegmentDecoder::GetReferencesB8x8()
{
    // field coded image
    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
    {
        for (int32_t i = 0; i < 4; i++)
        {
            int32_t index = m_cur_mb.GetRefIdx(0, i);
            m_deblockingParams.iReferences[i][0] = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(GetReferenceField(m_pFields[0], index)) : (size_t)-1;

            // select backward reference index for blocks
            index = m_cur_mb.GetRefIdx(1, i);
            m_deblockingParams.iReferences[i][1] = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(GetReferenceField(m_pFields[1], index)) : (size_t)-1;
        }
    }
    else  // frame coded image
    {
        for (int32_t i = 0; i < 4; i++)
        {
            int32_t index;

            index = m_cur_mb.GetRefIdx(0, i);
            m_deblockingParams.iReferences[i][0] = m_pRefPicList[0][index] ? m_pRefPicList[0][index]->DeblockPicID(0) : (size_t)-1;

            index = m_cur_mb.GetRefIdx(1, i);
            m_deblockingParams.iReferences[i][1] = m_pRefPicList[1][index] ? m_pRefPicList[1][index]->DeblockPicID(0) : (size_t)-1;
        }
    }
}

inline void H264SegmentDecoder::SetStrength(int32_t dir, int32_t block_num, uint32_t cbp4x4_luma, uint8_t strength)
{
    int32_t blkQ = INTERNAL_BLOCKS_MASK[dir][block_num - 4];
    if (cbp4x4_luma & blkQ)
    {
        m_deblockingParams.Strength[dir][block_num] = 2;
        m_deblockingParams.DeblockingFlag[dir] = 1;
    }
    else
    {
        m_deblockingParams.Strength[dir][block_num] = strength;
    }
}

void H264SegmentDecoder::PrepareStrengthsInternal()
{
    int32_t mbtype = m_cur_mb.GlobalMacroblockInfo->mbtype;

    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;

    switch (mbtype)
    {
    case MBTYPE_INTER:
    case MBTYPE_FORWARD:
    case MBTYPE_BACKWARD:
    case MBTYPE_BIDIR:
        {
            for (int32_t i = 4; i < 16; i++)
            {
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, 0);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, 0);
            }
        }
        break;

    case MBTYPE_INTER_8x16:
    case MBTYPE_INTER_16x8:
        {
            for (int32_t i = 4; i < 8; i++)
            {
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, 0);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, 0);
            }

            uint8_t strength_horz = 0, strength_vert = 0;

            if (mbtype == MBTYPE_INTER_8x16)
            {
                strength_vert = EvaluateStrengthInternal(VERTICAL_DEBLOCKING, 8);
            }
            else
            {
                strength_horz = EvaluateStrengthInternal(HORIZONTAL_DEBLOCKING, 8);
            }

            for (int32_t i = 8; i < 12; i++)
            {
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, strength_vert);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, strength_horz);
            }

            for (int32_t i = 12; i < 16; i++)
            {
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, 0);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, 0);
            }
        }
        break;

    case MBTYPE_INTER_8x8:
    case MBTYPE_INTER_8x8_REF0:
    default:
        {
            uint8_t strength;

            for (int32_t i = 8; i < 10; i++)
            {
                strength = EvaluateStrengthInternal(VERTICAL_DEBLOCKING, i);
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, strength);
                strength = EvaluateStrengthInternal(HORIZONTAL_DEBLOCKING, i);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, strength);
            }

            for (int32_t i = 10; i < 12; i++)
            {
                strength = EvaluateStrengthInternal(VERTICAL_DEBLOCKING, i);
                SetStrength(VERTICAL_DEBLOCKING, i, cbp4x4_luma, strength);
                strength = EvaluateStrengthInternal(HORIZONTAL_DEBLOCKING, i);
                SetStrength(HORIZONTAL_DEBLOCKING, i, cbp4x4_luma, strength);
            }

            for (int32_t i = 0; i < 4; i++)
            {
                int32_t start_8x8 = subblock_block_mapping[i];
                int32_t start_block = start_8x8 + 4;
                int32_t start_block_vertical = VERTICAL_DEBLOCKING_BLOCKS_MAP[start_8x8 + 1];
                switch (m_cur_mb.GlobalMacroblockInfo->sbtype[i])
                {
                case SBTYPE_8x8:
                    {
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical, cbp4x4_luma, 0);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical + 1, cbp4x4_luma, 0);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block, cbp4x4_luma, 0);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block + 1, cbp4x4_luma, 0);
                    }
                    break;

                case SBTYPE_8x4:
                    {
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical, cbp4x4_luma, 0);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical + 1, cbp4x4_luma, 0);

                        strength = EvaluateStrengthInternal8x8(HORIZONTAL_DEBLOCKING, start_block);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block, cbp4x4_luma, strength);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block + 1, cbp4x4_luma, strength);
                    }
                    break;

                case SBTYPE_4x8:
                    {
                        strength = EvaluateStrengthInternal8x8(VERTICAL_DEBLOCKING, start_block_vertical);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical, cbp4x4_luma, strength);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical + 1, cbp4x4_luma, strength);

                        SetStrength(HORIZONTAL_DEBLOCKING, start_block, cbp4x4_luma, 0);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block + 1, cbp4x4_luma, 0);
                    }
                    break;

                default: // 4x4 sub division
                    {
                        strength = EvaluateStrengthInternal8x8(VERTICAL_DEBLOCKING, start_block_vertical);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical, cbp4x4_luma, strength);
                        strength = EvaluateStrengthInternal8x8(VERTICAL_DEBLOCKING, start_block_vertical + 1);
                        SetStrength(VERTICAL_DEBLOCKING, start_block_vertical + 1, cbp4x4_luma, strength);

                        strength = EvaluateStrengthInternal8x8(HORIZONTAL_DEBLOCKING, start_block);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block, cbp4x4_luma, strength);
                        strength = EvaluateStrengthInternal8x8(HORIZONTAL_DEBLOCKING, start_block + 1);
                        SetStrength(HORIZONTAL_DEBLOCKING, start_block + 1, cbp4x4_luma, strength);
                    }
                    break;
                }
            }
        }
        break;
    }
}

void H264SegmentDecoder::PrepareDeblockingParametersBSlice4(uint32_t dir)
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    int32_t *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);

    GetReferencesB8x8();

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nNeighbour].mbtype))
        {
            int32_t idx;

            H264DecoderFrame **pNRefPicList0 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 0)->m_RefPicList;
            ReferenceFlags *pNFields0 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 0)->m_Flags;
            H264DecoderFrame **pNRefPicList1 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 1)->m_RefPicList;
            ReferenceFlags *pNFields1 = m_pCurrentFrame->GetRefPicList(m_gmbinfo->mbs[nNeighbour].slice_id, 1)->m_Flags;

            // cicle on blocks
            for (idx = 0; idx < 4; idx += 2)
            {
                EvaluateStrengthExternal(cbp4x4_luma, nNeighbour, dir,
                    idx,
                    pNRefPicList0, pNFields0, pNRefPicList1, pNFields1);
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (m_deblockingParams.MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

#if 1
    // static uint8_t temp[2][160];

    if (dir == VERTICAL_DEBLOCKING)
    {
        PrepareStrengthsInternal();

        /*MFX_INTERNAL_CPY(temp[0], m_deblockingParams.Strength[0], 16);
        MFX_INTERNAL_CPY(temp[1], m_deblockingParams.Strength[1], 16);
            SetEdgeStrength(m_deblockingParams.Strength[0] + 4, 0);
            SetEdgeStrength(m_deblockingParams.Strength[0] + 8, 0);
            SetEdgeStrength(m_deblockingParams.Strength[0] + 12, 0);
            SetEdgeStrength(m_deblockingParams.Strength[1] + 4, 0);
            SetEdgeStrength(m_deblockingParams.Strength[1] + 8, 0);
            SetEdgeStrength(m_deblockingParams.Strength[1] + 12, 0);*/
    }

#else
    //
    // internal edge(s)
    //
    {
        int32_t idx;

        /*for (idx = 4; idx < 16; idx += 1)
        {
            int32_t blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];
            if (cbp4x4_luma & blkQ)
            {
                m_deblockingParams.Strength[dir][idx] = 2;
                *pDeblockingFlag = 1;
            }
            else
            {
                m_deblockingParams.Strength[dir][idx] = foo_internal(pParams, dir, idx);
            }
        }*/

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4; idx < 16; idx += 2)
        {
            foo_internal(pParams, dir,
                idx, cbp4x4_luma);
        }
    }

    /*for (int32_t i = 4; i < 16; i++)
    {
        if (temp[dir][i] != m_deblockingParams.Strength[dir][i])
        {
            foo_internal(pParams, dir);
            foo_internal(pParams, dir,
                i, cbp4x4_luma);
        }
    }*/
#endif

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice4(uint32_t dir)

#define SET_DEBLOCKING_STRENGTH_B_SLICE_VERTICAL(block_num0, block_num1) \
{ \
    size_t refPrev[2]; \
    GetReferencesBSlice(nNeighbour, block_num0 * 4 + 3, refPrev); \
    if ((0 == (refPrev[0] ^ refPrev[1] ^ refCurFwd ^ refCurBwd)) && \
        ((refPrev[0] == refCurFwd) || (refPrev[0] == refCurBwd))) \
    { \
        if (0 == (VERTICAL_OUTER_EDGE_BLOCK_##block_num0 & uMask)) \
        { \
            H264DecoderMotionVector pMVPrevFwd = GetMV(m_gmbinfo, 0, nNeighbour, block_num0 * 4 + 3); \
            H264DecoderMotionVector pMVPrevBwd = GetMV(m_gmbinfo, 1, nNeighbour, block_num0 * 4 + 3); \
            if (refCurFwd != refCurBwd) \
            { \
                /* exchange reference for direct equality */ \
                if (refCurFwd != refPrev[0]) \
                { \
                    swapValues(mvCurFwd, mvCurBwd); \
                    swapValues(refCurFwd, refCurBwd); \
                } \
                /* compare motion vectors */ \
                if (IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                    IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) \
                { \
                    pStrength[block_num0] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num0] = 0; \
            } \
            else \
            { \
                /* compare motion vectors */ \
                if ((IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) && \
                    (IsVectorDifferenceBig(mvCurFwd, pMVPrevBwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevFwd, m_deblockingParams.nMaxMVector))) \
                { \
                    pStrength[block_num0] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num0] = 0; \
            } \
        } \
        else \
        { \
            pStrength[block_num0] = 2; \
            iDeblockingFlag = 1; \
        } \
        if (0 == (VERTICAL_OUTER_EDGE_BLOCK_##block_num1 & uMask)) \
        { \
            H264DecoderMotionVector pMVPrevFwd = GetMV(m_gmbinfo, 0, nNeighbour, block_num1 * 4 + 3); \
            H264DecoderMotionVector pMVPrevBwd = GetMV(m_gmbinfo, 1, nNeighbour, block_num1 * 4 + 3); \
            if (refCurFwd != refCurBwd) \
            { \
                /* exchange reference for direct equality */ \
                if (refCurFwd != refPrev[0]) \
                { \
                    swapValues(mvCurFwd, mvCurBwd); \
                    swapValues(refCurFwd, refCurBwd); \
                } \
                /* compare motion vectors */ \
                if (IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                    IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) \
                { \
                    pStrength[block_num1] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num1] = 0; \
            } \
            else \
            { \
                /* compare motion vectors */ \
                if ((IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) && \
                    (IsVectorDifferenceBig(mvCurFwd, pMVPrevBwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevFwd, m_deblockingParams.nMaxMVector))) \
                { \
                    pStrength[block_num1] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num1] = 0; \
            } \
        } \
        else \
        { \
            pStrength[block_num1] = 2; \
            iDeblockingFlag = 1; \
        } \
    } \
    else \
    { \
        pStrength[block_num0] = (uint8_t) (1 + ((VERTICAL_OUTER_EDGE_BLOCK_##block_num0 & uMask) ? (1) : (0))); \
        pStrength[block_num1] = (uint8_t) (1 + ((VERTICAL_OUTER_EDGE_BLOCK_##block_num1 & uMask) ? (1) : (0))); \
        iDeblockingFlag = 1; \
    } \
}

#define SET_DEBLOCKING_STRENGTH_B_SLICE_HORIZONTAL(block_num0, block_num1) \
{ \
    size_t refPrev[2]; \
    GetReferencesBSlice(nNeighbour, block_num0 + 12, refPrev); \
    if ((0 == (refPrev[0] ^ refPrev[1] ^ refCurFwd ^ refCurBwd)) && \
        ((refPrev[0] == refCurFwd) || (refPrev[0] == refCurBwd))) \
    { \
        bool is_first = (0 == (HORIZONTAL_OUTER_EDGE_BLOCK_##block_num0 & uMask)); \
        if (is_first) \
        { \
            H264DecoderMotionVector pMVPrevFwd = GetMV(m_gmbinfo, 0, nNeighbour, block_num0 + 12); \
            H264DecoderMotionVector pMVPrevBwd = GetMV(m_gmbinfo, 1, nNeighbour, block_num0 + 12); \
            if (refCurFwd != refCurBwd) \
            { \
                /* exchange reference for direct equality */ \
                if (refCurFwd != refPrev[0]) \
                { \
                    swapValues(mvCurFwd, mvCurBwd); \
                    swapValues(refCurFwd, refCurBwd); \
                } \
                /* compare motion vectors */ \
                if (IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                    IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) \
                { \
                    pStrength[block_num0] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num0] = 0; \
            } \
            else \
            { \
                /* compare motion vectors */ \
                if ((IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) && \
                    (IsVectorDifferenceBig(mvCurFwd, pMVPrevBwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevFwd, m_deblockingParams.nMaxMVector))) \
                { \
                    pStrength[block_num0] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num0] = 0; \
            } \
        } \
        else \
        { \
            pStrength[block_num0] = 2; \
            iDeblockingFlag = 1; \
        } \
        if (0 == (HORIZONTAL_OUTER_EDGE_BLOCK_##block_num1 & uMask)) \
        { \
            H264DecoderMotionVector pMVPrevFwd = GetMV(m_gmbinfo, 0, nNeighbour, block_num1 + 12); \
            H264DecoderMotionVector pMVPrevBwd = GetMV(m_gmbinfo, 1, nNeighbour, block_num1 + 12); \
            if (refCurFwd != refCurBwd) \
            { \
                /* exchange reference for direct equality */ \
                if (refCurFwd != refPrev[0]) \
                { \
                    swapValues(mvCurFwd, mvCurBwd); \
                    swapValues(refCurFwd, refCurBwd); \
                } \
                /* compare motion vectors */ \
                if (IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                    IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) \
                { \
                    pStrength[block_num1] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num1] = 0; \
            } \
            else \
            { \
                /* compare motion vectors */ \
                if ((IsVectorDifferenceBig(mvCurFwd, pMVPrevFwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevBwd, m_deblockingParams.nMaxMVector)) && \
                    (IsVectorDifferenceBig(mvCurFwd, pMVPrevBwd, m_deblockingParams.nMaxMVector) || \
                     IsVectorDifferenceBig(mvCurBwd, pMVPrevFwd, m_deblockingParams.nMaxMVector))) \
                { \
                    pStrength[block_num1] = 1; \
                    iDeblockingFlag = 1; \
                } \
                else \
                    pStrength[block_num1] = 0; \
            } \
        } \
        else \
        { \
            pStrength[block_num1] = 2; \
            iDeblockingFlag = 1; \
        } \
    } \
    else \
    { \
        pStrength[block_num0] = (uint8_t) (1 + ((HORIZONTAL_OUTER_EDGE_BLOCK_##block_num0 & uMask) ? (1) : (0))); \
        pStrength[block_num1] = (uint8_t) (1 + ((HORIZONTAL_OUTER_EDGE_BLOCK_##block_num1 & uMask) ? (1) : (0))); \
        iDeblockingFlag = 1; \
    } \
}

void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x16Vert()
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[VERTICAL_DEBLOCKING];
    int32_t iDeblockingFlag = 0;

    //
    // precalculating of reference numbers of current macroblock.
    // it is more likely we have to compare reference numbers.
    // we will use it also in horizontal deblocking
    //
    GetReferencesBCurMB(0, m_deblockingParams.iReferences[0]);

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[VERTICAL_DEBLOCKING])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[VERTICAL_DEBLOCKING];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nNeighbour].mbtype))
        {
            // select neighbour
            H264DecoderMacroblockLocalInfo *pNeighbour = &m_mbinfo.mbs[nNeighbour];

            // create special mask
            uint32_t uMask = cbp4x4_luma | (pNeighbour->cbp4x4_luma >> 5);

            // when at least one from a couple of blocks has coeffs
            if (VERTICAL_OUTER_EDGE_MASK != (uMask & VERTICAL_OUTER_EDGE_MASK))
            {
                // obtain current block parameters
                size_t refCurFwd = m_deblockingParams.iReferences[0][0];
                size_t refCurBwd = m_deblockingParams.iReferences[0][1];
                H264DecoderMotionVector mvCurFwd = m_cur_mb.GetMV(0, 0);
                H264DecoderMotionVector mvCurBwd = m_cur_mb.GetMV(1, 0);

                SET_DEBLOCKING_STRENGTH_B_SLICE_VERTICAL(0, 1)

                SET_DEBLOCKING_STRENGTH_B_SLICE_VERTICAL(2, 3)
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 2);
                iDeblockingFlag = 1;
            }
        }
        else
        {
            SetEdgeStrength(pStrength + 0, 4);
            iDeblockingFlag = 1;
        }
    }

    m_deblockingParams.DeblockingFlag[VERTICAL_DEBLOCKING] = iDeblockingFlag;

    //
    // internal edge(s)
    //
    PrepareDeblockingParametersIntern16x16Vert();

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x16Vert()

void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x16Horz()
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[HORIZONTAL_DEBLOCKING];
    int32_t iDeblockingFlag = 0;

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[HORIZONTAL_DEBLOCKING])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[HORIZONTAL_DEBLOCKING];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE(m_gmbinfo->mbs[nNeighbour].mbtype))
        {
            // select neighbour
            H264DecoderMacroblockLocalInfo *pNeighbour = &m_mbinfo.mbs[nNeighbour];

            // create special mask
            uint32_t uMask = cbp4x4_luma | (pNeighbour->cbp4x4_luma >> 10);

            // when at least one from a couple of blocks has coeffs
            if (HORIZONTAL_OUTER_EDGE_MASK != (uMask & HORIZONTAL_OUTER_EDGE_MASK))
            {
                // obtain current block parameters
                size_t refCurFwd = m_deblockingParams.iReferences[0][0];
                size_t refCurBwd = m_deblockingParams.iReferences[0][1];
                H264DecoderMotionVector mvCurFwd = m_cur_mb.GetMV(0, 0);
                H264DecoderMotionVector mvCurBwd = m_cur_mb.GetMV(1, 0);

                SET_DEBLOCKING_STRENGTH_B_SLICE_HORIZONTAL(0, 1)

                SET_DEBLOCKING_STRENGTH_B_SLICE_HORIZONTAL(2, 3)
            }
            // when at least one from a couple of blocks has coeffs
            else
            {
                SetEdgeStrength(pStrength + 0, 2);
                iDeblockingFlag = 1;
            }
        }
        else
        {
            if (m_deblockingParams.MBFieldCoded)
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            iDeblockingFlag = 1;
        }
    }

    m_deblockingParams.DeblockingFlag[HORIZONTAL_DEBLOCKING] = iDeblockingFlag;

    //
    // internal edge(s)
    //
    PrepareDeblockingParametersIntern16x16Horz();

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x16Horz()

void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x8(uint32_t dir)
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    int32_t *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);
    size_t iRefQFrw, iRefQBck;

    //
    // external edge
    //

    // load reference indexes & motion vector for first half of current block
    {
        // field coded image
        if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
        {
            H264DecoderFrame **pRefPicList;
            int32_t index;
            ReferenceFlags *pFields;

            // select reference index for current block
            index = m_cur_mb.GetRefIdx(0, 0);
            if (0 <= index)
            {
                pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                // select reference fields number array
                pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_Flags;
                iRefQFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
            }
            else
                iRefQFrw = (size_t)-1;
            index = m_cur_mb.GetRefIdx(1, 0);
            if (0 <= index)
            {
                pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                // select reference fields number array
                pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_Flags;
                iRefQBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
            }
            else
                iRefQBck = (size_t)-1;
        }
        // frame coded image
        else
        {
            H264DecoderFrame **pRefPicList;
            int32_t index;

            // select reference index for current block
            index = m_cur_mb.GetRefIdx(0, 0);
            if (0 <= index)
            {
                pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                iRefQFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
            }
            else
                iRefQFrw = (size_t)-1;
            index = m_cur_mb.GetRefIdx(1, 0);
            if (0 <= index)
            {
                pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                iRefQBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
            }
            else
                iRefQBck = (size_t)-1;
        }
    }

    H264DecoderMotionVector &pVectorQFrw = m_cur_mb.GetMV(0, 0);
    H264DecoderMotionVector &pVectorQBck = m_cur_mb.GetMV(1, 0);

    // prepare deblocking parameter for external edge
    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            int32_t idx;

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                int32_t blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma & blkQ) ||
                    (pNeighbour->cbp4x4_luma & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    int32_t nNeighbourBlock;
                    size_t iRefPFrw, iRefPBck;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264DecoderFrame **pRefPicList;
                        int32_t index;
                        ReferenceFlags *pFields;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            // select reference fields number array
                            pFields = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_Flags;
                            iRefPFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                        }
                        else
                            iRefPFrw = (size_t)-1;
                        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_RefPicList;
                            // select reference fields number array
                            pFields = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_Flags;
                            iRefPBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                        }
                        else
                            iRefPBck = (size_t)-1;
                    }
                    // frame coded image
                    else
                    {
                        H264DecoderFrame **pRefPicList;
                        int32_t index;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            iRefPFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                        }
                        else
                            iRefPFrw = (size_t)-1;
                        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_RefPicList;
                            iRefPBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                        }
                        else
                            iRefPBck = (size_t)-1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw == iRefPFrw) && (iRefQBck == iRefPBck)) ||
                        ((iRefQFrw == iRefPBck) && (iRefQBck == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            // select previous block motion vectors
                            if (iRefQFrw != iRefPFrw)
                            {
                                swapValues(pVectorPFrw, pVectorPBck);
                            }

                            // compare motion vectors
                            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                                IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            // compare motion vectors
                            if (IsVectorDifferenceBig(pVectorQFrw, pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                                IsVectorDifferenceBig(pVectorQBck, pVectorPBck, m_deblockingParams.nMaxMVector))
                            {
                                if (IsVectorDifferenceBig(pVectorQFrw, pVectorPBck, m_deblockingParams.nMaxMVector) ||
                                    IsVectorDifferenceBig(pVectorQBck, pVectorPFrw, m_deblockingParams.nMaxMVector))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (m_deblockingParams.MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //if (dir == VERTICAL_DEBLOCKING)
      //  foo_internal(pParams, dir);
    //return;
    //
    // internal edge(s)
    //
    {
        int32_t idx;

        // cicle of edge(s)
        for (idx = 4;idx < 8;idx += 1)
        {
            int32_t blkQ;

            blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp4x4_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // we haven't to compare motion vectors  - they are equal
            else
                pStrength[idx] = 0;
        }

        // load reference indexes & motion vector for second half of current block
        {
            size_t iRefQFrw2, iRefQBck2;
            uint32_t nStrength;

            // load reference indexes for current block

            // field coded image
            if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
            {
                H264DecoderFrame **pRefPicList;
                int32_t index;
                ReferenceFlags *pFields;

                // select reference index for current block
                index = m_cur_mb.GetRefIdx(0, 3);
                if (0 <= index)
                {
                    pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                    // select reference fields number array
                    pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_Flags;
                    iRefQFrw2 = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                }
                else
                    iRefQFrw2 = (size_t)-1;
                index = m_cur_mb.GetRefIdx(1, 3);
                if (0 <= index)
                {
                    pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                    // select reference fields number array
                    pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_Flags;
                    iRefQBck2 = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                }
                else
                    iRefQBck2 = (size_t)-1;
            }
            // frame coded image
            else
            {
                H264DecoderFrame **pRefPicList;
                int32_t index;

                // select reference index for current block
                index = m_cur_mb.GetRefIdx(0, 3);
                if (0 <= index)
                {
                    pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                    iRefQFrw2 = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                }
                else
                    iRefQFrw2 = (size_t)-1;
                index = m_cur_mb.GetRefIdx(1, 3);
                if (0 <= index)
                {
                    pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                    iRefQBck2 = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                }
                else
                    iRefQBck2 = (size_t)-1;
            }

            // when reference indexes are equal
            if (((iRefQFrw == iRefQFrw2) && (iRefQBck == iRefQBck2)) ||
                ((iRefQFrw == iRefQBck2) && (iRefQBck == iRefQFrw2)))
            {
                // set initial value of strength
                nStrength = 0;

                // when forward and backward reference pictures of previous block are different
                if (iRefQFrw2 != iRefQBck2)
                {
                    H264DecoderMotionVector pVectorQFrw2 = m_cur_mb.GetMV(0, 15);
                    H264DecoderMotionVector pVectorQBck2 = m_cur_mb.GetMV(1, 15);

                    // select previous block motion vectors
                    if (iRefQFrw != iRefQFrw2)
                    {
                        swapValues(pVectorQFrw2, pVectorQBck2);
                    }

                    // compare motion vectors
                    if (IsVectorDifferenceBig(pVectorQFrw, pVectorQFrw2, m_deblockingParams.nMaxMVector) ||
                        IsVectorDifferenceBig(pVectorQBck, pVectorQBck2, m_deblockingParams.nMaxMVector))
                    {
                        nStrength = 1;
                        *pDeblockingFlag = 1;
                    }
                }
                // when forward and backward reference pictures of previous block are equal
                else
                {
                    // select block second motion vectors
                    H264DecoderMotionVector & pVectorQFrw2 = m_cur_mb.GetMV(0, 15);
                    H264DecoderMotionVector & pVectorQBck2 = m_cur_mb.GetMV(1, 15);

                    // compare motion vectors
                    if (IsVectorDifferenceBig(pVectorQFrw, pVectorQFrw2, m_deblockingParams.nMaxMVector) ||
                        IsVectorDifferenceBig(pVectorQBck, pVectorQBck2, m_deblockingParams.nMaxMVector))
                    {
                        if (IsVectorDifferenceBig(pVectorQFrw, pVectorQBck2, m_deblockingParams.nMaxMVector) ||
                            IsVectorDifferenceBig(pVectorQBck, pVectorQFrw2, m_deblockingParams.nMaxMVector))
                        {
                            nStrength = 1;
                            *pDeblockingFlag = 1;
                        }
                    }
                }
            }
            // when reference indexes are different
            else
            {
                nStrength = 1;
                *pDeblockingFlag = 1;
            }

            // cicle of edge(s)
            for (idx = 8;idx < 12;idx += 1)
            {
                int32_t blkQ;

                blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

                if (cbp4x4_luma & blkQ)
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // we have compared motion vectors
                else
                    pStrength[idx] = (uint8_t) nStrength;
            }
        }

        // cicle of edge(s)
        for (idx = 12;idx < 16;idx += 1)
        {
            int32_t blkQ;

            blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp4x4_luma & blkQ)
            {
                pStrength[idx] = 2;
                *pDeblockingFlag = 1;
            }
            // we haven't to compare motion vectors  - they are equal
            else
                pStrength[idx] = 0;
        }
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice16x8(uint32_t dir)

void H264SegmentDecoder::PrepareDeblockingParametersBSlice8x16(uint32_t dir)
{
    uint32_t cbp4x4_luma = m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
    uint8_t *pStrength = m_deblockingParams.Strength[dir];
    int32_t *pDeblockingFlag = &(m_deblockingParams.DeblockingFlag[dir]);

    //
    // external edge
    //
    if (m_deblockingParams.ExternalEdgeFlag[dir])
    {
        int32_t nNeighbour;

        // select neighbour addres
        nNeighbour = m_deblockingParams.nNeighbour[dir];

        // when neighbour macroblock isn't intra
        if (!IS_INTRA_MBTYPE((m_gmbinfo->mbs + nNeighbour)->mbtype))
        {
            H264DecoderMacroblockLocalInfo *pNeighbour;
            int32_t idx;
            size_t iRefQFrw[2], iRefQBck[2];
            H264DecoderMotionVector *(pVectorQFrw[2]), *(pVectorQBck[2]);

            // in following calculations we avoided multiplication on 15
            // by using formulae a * 15 = a * 16 - a

            // load reference indexes for current block
            for (idx = 0;idx < 2;idx += 1)
            {
                // field coded image
                if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
                {
                    H264DecoderFrame **pRefPicList;
                    int32_t index;
                    ReferenceFlags *pFields;

                    // select reference index for current block
                    index = m_cur_mb.GetReferenceIndex(0, idx * 16 - idx);
                    if (0 <= index)
                    {
                        pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                        // select reference fields number array
                        pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_Flags;
                        iRefQFrw[idx] = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                    }
                    else
                        iRefQFrw[idx] = (size_t)-1;
                    index = m_cur_mb.GetReferenceIndex(1, idx * 16 - idx);
                    if (0 <= index)
                    {
                        pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                        // select reference fields number array
                        pFields = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_Flags;
                        iRefQBck[idx] = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                    }
                    else
                        iRefQBck[idx] = (size_t)-1;
                }
                // frame coded image
                else
                {
                    H264DecoderFrame **pRefPicList;
                    int32_t index;

                    // select reference index for current block
                    index = m_cur_mb.GetReferenceIndex(0, idx * 16 - idx);
                    if (0 <= index)
                    {
                        pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 0)->m_RefPicList;
                        iRefQFrw[idx] = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                    }
                    else
                        iRefQFrw[idx] = (size_t)-1;
                    index = m_cur_mb.GetReferenceIndex(1, idx * 16 - idx);
                    if (0 <= index)
                    {
                        pRefPicList = m_pCurrentFrame->GetRefPicList(m_cur_mb.GlobalMacroblockInfo->slice_id, 1)->m_RefPicList;
                        iRefQBck[idx] = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                    }
                    else
                        iRefQBck[idx] = (size_t)-1;
                }

                // select current block motion vectors
                pVectorQFrw[idx] = m_cur_mb.GetMVPtr(0, idx * 16 - idx);
                pVectorQBck[idx] = m_cur_mb.GetMVPtr(1, idx * 16 - idx);
            }

            // select neighbour
            pNeighbour = m_mbinfo.mbs + nNeighbour;

            // cicle on blocks
            for (idx = 0;idx < 4;idx += 1)
            {
                int32_t blkQ, blkP;

                blkQ = EXTERNAL_BLOCK_MASK[dir][CURRENT_BLOCK][idx];
                blkP = EXTERNAL_BLOCK_MASK[dir][NEIGHBOUR_BLOCK][idx];

                // when one of couple of blocks has coeffs
                if ((cbp4x4_luma & blkQ) ||
                    (pNeighbour->cbp4x4_luma & blkP))
                {
                    pStrength[idx] = 2;
                    *pDeblockingFlag = 1;
                }
                // compare motion vectors & reference indexes
                else
                {
                    int32_t nNeighbourBlock;
                    size_t iRefPFrw, iRefPBck;

                    // calc block and neighbour block number
                    if (VERTICAL_DEBLOCKING == dir)
                        nNeighbourBlock = idx * 4 + 3;
                    else
                        nNeighbourBlock = idx + 12;

                    // field coded image
                    if (FRM_STRUCTURE > m_pCurrentFrame->m_PictureStructureForDec)
                    {
                        H264DecoderFrame **pRefPicList;
                        int32_t index;
                        ReferenceFlags *pFields;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            // select reference fields number array
                            pFields = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_Flags;
                            iRefPFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                        }
                        else
                            iRefPFrw = (size_t)-1;
                        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_RefPicList;
                            // select reference fields number array
                            pFields = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_Flags;
                            iRefPBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(GetReferenceField(pFields, index)) : (size_t)-1;
                        }
                        else
                            iRefPBck = (size_t)-1;
                    }
                    // frame coded image
                    else
                    {
                        H264DecoderFrame **pRefPicList;
                        int32_t index;

                        // select reference index for previous block
                        index = GetReferenceIndex(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 0)->m_RefPicList;
                            iRefPFrw = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                        }
                        else
                            iRefPFrw = (size_t)-1;
                        index = GetReferenceIndex(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);
                        if (0 <= index)
                        {
                            pRefPicList = m_pCurrentFrame->GetRefPicList((m_gmbinfo->mbs + nNeighbour)->slice_id, 1)->m_RefPicList;
                            iRefPBck = pRefPicList[index] ? pRefPicList[index]->DeblockPicID(0) : (size_t)-1;
                        }
                        else
                            iRefPBck = (size_t)-1;
                    }

                    // when reference indexes are equal
                    if (((iRefQFrw[idx / 2] == iRefPFrw) && (iRefQBck[idx / 2] == iRefPBck)) ||
                        ((iRefQFrw[idx / 2] == iRefPBck) && (iRefQBck[idx / 2] == iRefPFrw)))
                    {
                        // set initial value of strength
                        pStrength[idx] = 0;

                        // when forward and backward reference pictures of previous block are different
                        if (iRefPFrw != iRefPBck)
                        {
                            H264DecoderMotionVector pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                            H264DecoderMotionVector pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

                            // select previous block motion vectors
                            if (iRefQFrw[idx / 2] != iRefPFrw)
                            {
                                swapValues(pVectorPFrw, pVectorPBck);
                            }

                            // compare motion vectors
                            if (IsVectorDifferenceBig(*pVectorQFrw[idx / 2], pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                                IsVectorDifferenceBig(*pVectorQBck[idx / 2], pVectorPBck, m_deblockingParams.nMaxMVector))
                            {
                                pStrength[idx] = 1;
                                *pDeblockingFlag = 1;
                            }
                        }
                        // when forward and backward reference pictures of previous block are equal
                        else
                        {
                            // select previous block motion vectors
                            H264DecoderMotionVector &pVectorPFrw = GetMV(m_gmbinfo, 0, nNeighbour, nNeighbourBlock);
                            H264DecoderMotionVector &pVectorPBck = GetMV(m_gmbinfo, 1, nNeighbour, nNeighbourBlock);

                            // compare motion vectors
                            if (IsVectorDifferenceBig(*pVectorQFrw[idx / 2], pVectorPFrw, m_deblockingParams.nMaxMVector) ||
                                IsVectorDifferenceBig(*pVectorQBck[idx / 2], pVectorPBck, m_deblockingParams.nMaxMVector))
                            {
                                if (IsVectorDifferenceBig(*pVectorQFrw[idx / 2], pVectorPBck, m_deblockingParams.nMaxMVector) ||
                                    IsVectorDifferenceBig(*pVectorQBck[idx / 2], pVectorPFrw, m_deblockingParams.nMaxMVector))
                                {
                                    pStrength[idx] = 1;
                                    *pDeblockingFlag = 1;
                                }
                            }
                        }
                    }
                    // when reference indexes are different
                    else
                    {
                        pStrength[idx] = 1;
                        *pDeblockingFlag = 1;
                    }
                }
            }
        }
        // external edge required in strong filtering
        else
        {
            if ((HORIZONTAL_DEBLOCKING == dir) &&
                (m_deblockingParams.MBFieldCoded))
            {
                SetEdgeStrength(pStrength + 0, 3);
            }
            else
            {
                SetEdgeStrength(pStrength + 0, 4);
            }
            *pDeblockingFlag = 1;
        }
    }

    //if (dir == VERTICAL_DEBLOCKING)
     //   foo_internal(pParams, dir);
    //return;

    //
    // internal edge(s)
    //
    {
        int32_t idx;

        // reset all strengths
        SetEdgeStrength(pStrength + 4, 0);
        SetEdgeStrength(pStrength + 8, 0);
        SetEdgeStrength(pStrength + 12, 0);

        // set deblocking flag
        if (cbp4x4_luma & 0x1fffe)
            *pDeblockingFlag = 1;

        // cicle of edge(s)
        // we do all edges in one cicle
        for (idx = 4;idx < 16;idx += 1)
        {
            int32_t blkQ;

            blkQ = INTERNAL_BLOCKS_MASK[dir][idx - 4];

            if (cbp4x4_luma & blkQ)
                pStrength[idx] = 2;
        }
    }

} // void H264SegmentDecoder::PrepareDeblockingParametersBSlice8x16(uint32_t dir)

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER