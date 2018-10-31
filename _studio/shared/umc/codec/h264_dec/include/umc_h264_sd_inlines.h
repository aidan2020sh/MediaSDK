//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_SEGMENT_DECODER_INLINES_H
#define __UMC_H264_SEGMENT_DECODER_INLINES_H

using namespace UMC_H264_DECODER;

namespace UMC
{
//
// Class to incapsulate functions, implementing common decoding functional.
//

inline
uint32_t BIT_CHECK(const uint32_t value, int32_t n)
{
    return (uint32_t)((value & mask_bit[n]) >> n);
}

inline
uint32_t IsLeftBlockExist(int32_t iCBP, int32_t iBlock)
{
    return (((uint32_t) -(iCBP & iLeftBlockMask[iBlock])) >> 31);

} // uint32_t IsLeftBlockExist(int32_t iCBP, int32_t iBlock)

inline
uint32_t IsTopBlockExist(int32_t iCBP, int32_t iBlock)
{
    return (((uint32_t) -(iCBP & iTopBlockMask[iBlock])) >> 31);
} // uint32_t IsTopBlockExist(int32_t iCBP, int32_t iBlock)


// Get context functions
inline
uint32_t H264SegmentDecoder::GetDCBlocksLumaContext()
{
    bool use_above = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0;
    bool use_left = m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num>=0;
    uint32_t above_coeffs=use_above?
        GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above.block_num) : 0;
    uint32_t left_coeffs=use_left?
        GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].block_num) : 0;
    if(use_above && use_left) return (above_coeffs+left_coeffs+1)/2;
    else if (use_above ) return above_coeffs;
    else if (use_left) return left_coeffs;
    else return 0;
}

// an universal function for an every case of the live
inline
uint32_t H264SegmentDecoder::GetBlocksLumaContext(int32_t x,int32_t y)
{
    bool use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num>=0;
    bool use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left[y].mb_num>=0;
    uint8_t above_coeffs=0;
    if (use_above) above_coeffs= y==0?
        GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above.block_num+x):
        m_cur_mb.GetNumCoeff(y*4+x-4);
    uint8_t left_coeffs=0;
    if (use_left)left_coeffs = x==0?
        GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left[y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left[y].block_num):
        m_cur_mb.GetNumCoeff(y*4+x-1);

    if(use_above && use_left) return (above_coeffs+left_coeffs+1)/2;
    else if (use_above ) return above_coeffs;
    else if (use_left) return left_coeffs;
    else return 0;
}

// a function for the first luma block in a macroblock
inline
int32_t H264SegmentDecoder::GetBlocksLumaContextExternal(void)
{
    bool use_above = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num >= 0;
    bool use_left = m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num >= 0;

    if (use_above && use_left)
    {
        int32_t above_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above.block_num);
        int32_t left_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].block_num);

        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else if (use_above)
    {
        return GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above.block_num);
    }
    else if (use_left)
    {
        return GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left[0].block_num);
    }
    else
        return 0;

} // int32_t GetBlocksLumaContextExternal(void)

// a function for a block on the upper edge of a macroblock,
// but not for the first block
inline
int32_t H264SegmentDecoder::GetBlocksLumaContextTop(int32_t x, int32_t left_coeffs)
{
    bool use_above = m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num >= 0;
    int32_t above_coeffs;

    if (use_above)
    {
        above_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above.mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above.block_num + x);
        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else
        return left_coeffs;

} // int32_t GetBlocksLumaContextTop(int32_t x, int32_t left_coeffs)

// a function for a block on the left edge of a macroblock,
// but not for the first block
inline
uint32_t H264SegmentDecoder::GetBlocksLumaContextLeft(int32_t y, int32_t above_coeffs)
{
    bool use_left = m_cur_mb.CurrentBlockNeighbours.mbs_left[y].mb_num >= 0;
    int32_t left_coeffs;

    if (use_left)
    {
        left_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left[y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left[y].block_num);
        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else
        return above_coeffs;

} // uint32_t GetBlocksLumaContextLeft(int32_t y, int32_t above_coeffs)

// a function for any internal block of a macroblock
inline
uint32_t H264SegmentDecoder::GetBlocksLumaContextInternal(int32_t raster_block_num, uint8_t *pNumCoeffsArray)
{
    return (pNumCoeffsArray[raster_block_num - 1] +
            pNumCoeffsArray[raster_block_num - 4] + 1) / 2;

} // uint32_t GetBlocksLumaContextInternal(int32_t x, int32_t y, uint8_t *pNumCoeffsArray)

// an universal function for an every case of the live
inline
uint32_t H264SegmentDecoder::GetBlocksChromaContextBMEH(int32_t x,int32_t y,int32_t component)
{
    uint8_t above_coeffs=0;
    uint8_t left_coeffs=0;
    bool use_above;
    bool use_left;
    if (component)
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num>=0;
        if (use_above)
            above_coeffs=y==0?
                GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].block_num+x):
                m_cur_mb.GetNumCoeff(y*2+x-2+20);
        if (use_left)
            left_coeffs=x==0?
                GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].block_num):
                m_cur_mb.GetNumCoeff(y*2+x-1+20);
    }
    else
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num>=0;
        if (use_above)
            above_coeffs=y==0?
                GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].block_num+x):
                m_cur_mb.GetNumCoeff(y*2+x-2+16);
        if (use_left)
            left_coeffs=x==0?
                GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].block_num):
                m_cur_mb.GetNumCoeff(y*2+x-1+16);
    }

    if(use_above && use_left) return (above_coeffs+left_coeffs+1)/2;
    else if (use_above ) return above_coeffs;
    else if (use_left) return left_coeffs;
    else return 0;
}

// a function for the first block in a macroblock
inline
int32_t H264SegmentDecoder::GetBlocksChromaContextBMEHExternal(int32_t iComponent)
{
    bool use_above = m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].mb_num >= 0;
    bool use_left = m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].mb_num >= 0;
    int32_t above_coeffs, left_coeffs;

    if (use_above && use_left)
    {
        above_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].block_num);
        left_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].block_num);
        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else if (use_above)
    {
        above_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].block_num);
        return above_coeffs;
    }
    else if (use_left)
    {
        left_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].block_num);
        return left_coeffs;
    }
    else
        return 0;

} // int32_t GetBlocksChromaContextBMEHExternal(int32_t iComponent)

// a function for a block on the upper edge of a macroblock,
// but not for the first block
inline
int32_t H264SegmentDecoder::GetBlocksChromaContextBMEHTop(int32_t x, int32_t left_coeffs, int32_t iComponent)
{
    bool use_above = m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].mb_num >= 0;
    int32_t above_coeffs;

    if (use_above)
    {
        above_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[iComponent].block_num + x);
        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else
        return left_coeffs;

} // int32_t GetBlocksChromaContextBMEHTop(int32_t x, int32_t left_coeffs, int32_t iComponent)

// a function for a block on the left edge of a macroblock,
// but not for the first block
inline
int32_t H264SegmentDecoder::GetBlocksChromaContextBMEHLeft(int32_t y, int32_t above_coeffs, int32_t iComponent)
{
    bool use_left = m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][0].mb_num >= 0;
    int32_t left_coeffs;

    if (use_left)
    {
        left_coeffs = GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[iComponent][y].block_num);
        return (above_coeffs + left_coeffs + 1) / 2;
    }
    else
        return above_coeffs;

} // int32_t GetBlocksChromaContextBMEHLeft(int32_t y, int32_t above_coeffs, int32_t iComponent)

// a function for any internal block of a macroblock
inline
int32_t H264SegmentDecoder::GetBlocksChromaContextBMEHInternal(int32_t raster_block_num, uint8_t *pNumCoeffsArray)
{
    return (pNumCoeffsArray[raster_block_num- 1] +
            pNumCoeffsArray[raster_block_num - 2] + 1) / 2;

} // int32_t GetBlocksChromaContextBMEHInternal(int32_t x, int32_t y, uint8_t *pNumCoeffsArray)

inline
uint32_t H264SegmentDecoder::GetBlocksChromaContextH2(int32_t x,int32_t y,int32_t component)
{
    uint8_t above_coeffs=0;
    uint8_t left_coeffs=0;
    bool use_above;
    bool use_left;
    if (component)
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num>=0;
        if (use_above)
            above_coeffs=y==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].block_num+x) :
            m_cur_mb.GetNumCoeff(y*2+x-2+24);

        if (use_left)
            left_coeffs=x==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].block_num) :
            m_cur_mb.GetNumCoeff(y*2+x-1+24);
    }
    else
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num>=0;
        if (use_above)
            above_coeffs = y==0 ?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].block_num+x) :
            m_cur_mb.GetNumCoeff(y*2+x-2+16);
        if (use_left)
            left_coeffs = x==0 ?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].block_num) :
            m_cur_mb.GetNumCoeff(y*2+x-1+16);
    }

    if(use_above && use_left) return (above_coeffs+left_coeffs+1)/2;
    else if (use_above ) return above_coeffs;
    else if (use_left) return left_coeffs;
    else return 0;
}

inline
uint32_t H264SegmentDecoder::GetBlocksChromaContextH4(int32_t x,int32_t y,int32_t component)
{
    uint8_t above_coeffs=0;
    uint8_t left_coeffs=0;
    bool use_above;
bool use_left;
    if (component)
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num>=0;
        if (use_above)
            above_coeffs=y==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[1].block_num+x):
            m_cur_mb.GetNumCoeff(y*4+x-4+32);
        if (use_left)
            left_coeffs=x==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[1][y].block_num):
            m_cur_mb.GetNumCoeff(y*4+x-1+32);
    }
    else
    {
        use_above = y || m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num>=0;
        use_left = x || m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num>=0;
        if (use_above)
            above_coeffs=y==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].mb_num, m_cur_mb.CurrentBlockNeighbours.mb_above_chroma[0].block_num+x):
            m_cur_mb.GetNumCoeff(y*4+x-4+16);
        if (use_left)
            left_coeffs=x==0?
            GetNumCoeff(&m_mbinfo, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].mb_num, m_cur_mb.CurrentBlockNeighbours.mbs_left_chroma[0][y].block_num) :
            m_cur_mb.GetNumCoeff(y*4+x-1+16);
    }

    if(use_above && use_left) return (above_coeffs+left_coeffs+1)/2;
    else if (use_above ) return above_coeffs;
    else if (use_left) return left_coeffs;
    else return 0;
}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)
{
    //luma
    if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num))
    {
        Block->block_num+=3;
        Block->mb_num=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    }
    else
    {
        Block->block_num--;
        Block->mb_num=m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)
{
    //luma
    if (BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num+=12;
        Block->mb_num  = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else
    {
        Block->block_num-=4;
        Block->mb_num = m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetTopLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetTopLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)
{
    //luma
    if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num) && BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num+=15;
        Block->mb_num = m_cur_mb.CurrentMacroblockNeighbours.mb_D;
    }
    else if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num))
    {
        Block->block_num--;
        Block->mb_num = m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    }
    else if ( BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num+=11;
        Block->mb_num = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else
    {
        Block->block_num-=5;
        Block->mb_num = m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetTopLeftLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetTopRightLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)
{
    //luma
    if (Block->block_num==3)
    {
        Block->block_num+=9;
        Block->mb_num = m_cur_mb.CurrentMacroblockNeighbours.mb_C;
    }
    else if ( BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num+=13;
        Block->mb_num = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else if (!above_right_avail_4x4[Block->block_num])
    {
        Block->mb_num = -1;
    }
    else
    {
        Block->block_num-=3;
        Block->mb_num = m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetTopRightLocationForCurrentMBLumaNonMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block,int32_t AdditionalDecrement)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);

    int32_t MB_X=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    int32_t MB_N;
        //luma
    if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num))
    {
        if (MB_X>=0)
        {
            uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
            if (curmbfff)
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        // 1 1 1
                        MB_N=MB_X;
                    }
                    else
                    {
                        // 1 1 0
                        uint32_t yN = Block->block_num/4;
                        yN*=4;
                        yN-=AdditionalDecrement;
                        yN/=2;
                        Block->block_num=(yN/4)*4;
                        if (AdditionalDecrement)
                            MB_N=MB_X  + 1;
                        else
                            MB_N=MB_X;
                        AdditionalDecrement=0;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 1 0 1
                        MB_N=MB_X + 1;
                    }
                    else
                    {
                        // 1 0 0
                        uint32_t yN = Block->block_num/4;
                        yN*=4;
                        yN-=AdditionalDecrement;
                        yN+=16;
                        yN/=2;
                        Block->block_num=(yN/4)*4;
                        if (AdditionalDecrement)
                            MB_N=MB_X + 1;
                        else
                            MB_N=MB_X;
                        AdditionalDecrement=0;
                    }
                }
            }
            else
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        //0 1 1
                        uint32_t yN = Block->block_num/4;
                        yN*=4;
                        yN-=AdditionalDecrement;
                        yN*=2;
                        if (yN<16)
                        {
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN-=16;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*4;
                        AdditionalDecrement=0;
                    }
                    else
                    {
                        // 0 1 0
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 0 0 1
                        uint32_t yN = Block->block_num/4;
                        yN*=4;
                        yN-=AdditionalDecrement;
                        yN*=2;
                        if (yN<15)
                        {
                            yN++;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN-=15;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*4;
                        AdditionalDecrement=0;
                    }
                    else
                    {
                        // 0 0 0
                        MB_N=MB_X + 1;
                    }
                }
            }
        }
        else
        {
            Block->mb_num = -1;//no left neighbours
            return;
        }
        Block->block_num+=3-4*AdditionalDecrement;
        Block->mb_num = MB_N;
    }
    else
    {
        Block->block_num--;
        Block->mb_num = m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block,int32_t AdditionalDecrement)

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block, int32_t DeblockCalls)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t pair_offset = (curmbtf) ? (1) : (-1);
    int32_t MB_X;
    int32_t MB_N;
    //luma
    if (BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        if (curmbfff && !curmbtf )
        {
            MB_N = m_CurMBAddr + pair_offset;
            Block->block_num+=12;
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
            if (MB_X>=0)
            {
                uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
                MB_N=MB_X;
                Block->block_num+=12;
                if (curmbfff || !curmbtf || xfff)
                {
                    if (!(curmbfff && curmbtf && !xfff && DeblockCalls))
                        MB_N+= 1;
                }
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }
        Block->mb_num = MB_N;
        return ;
    }
    else
    {
        Block->block_num-=4;
        Block->mb_num = m_CurMBAddr;
        return;
    }

} // void H264SegmentDecoder::GetTopLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block,uint8_t DeblockCalls)


inline
void H264SegmentDecoder::GetTopLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t MB_X;
    int32_t MB_N;
    //luma
    if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num) && BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        if (curmbfff && !curmbtf )
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_A;
            if (MB_X<0)
            {
                Block->mb_num = -1;
                return;
            }
            if (!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]))
            {
                MB_N = MB_X;
                Block->block_num+=15;
            }
            else
            {
                MB_N = MB_X + 1;
                Block->block_num+=7;
            }
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_D;
            if (MB_X>=0)
            {
                if (curmbfff==curmbtf)
                {
                    MB_N=MB_X + 1;
                }
                else
                {
                    if (!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]))
                    {
                        MB_N=MB_X + 1;
                    }
                    else
                    {
                        MB_N=MB_X;
                    }
                }
                Block->block_num+=15;
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }

        Block->mb_num = MB_N;
        return;
    }
    else if (BLOCK_IS_ON_LEFT_EDGE(Block->block_num))
    {
        //Block->block_num-=4;
        GetLeftLocationForCurrentMBLumaMBAFF(Block,1);
        return;
    }
    else if (BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num--;
        GetTopLocationForCurrentMBLumaMBAFF(Block,0);
        return;
    }
    else
    {
        Block->block_num-=5;
        Block->mb_num = m_CurMBAddr;
        return;
    }

} // void H264SegmentDecoder::GetTopLeftLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetTopRightLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t MB_X;
    int32_t MB_N;
    //luma
    if (Block->block_num==3)
    {
        if (curmbfff && !curmbtf )
        {
            Block->mb_num = -1;
            return;
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_C;
            if (MB_X>=0)
            {
                if (curmbfff==curmbtf)
                {
                    MB_N=MB_X + 1;
                }
                else
                {
                    if (!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]))
                    {
                        MB_N=MB_X + 1;
                    }
                    else
                    {
                        MB_N=MB_X;
                    }
                }
                Block->block_num+=9;
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }
        Block->mb_num = MB_N;
        return;
    }
    else if ( BLOCK_IS_ON_TOP_EDGE(Block->block_num))
    {
        Block->block_num++;
        GetTopLocationForCurrentMBLumaMBAFF(Block,0);
        return;
    }
    else if (!above_right_avail_4x4_lin[Block->block_num])
    {
        Block->mb_num = -1;
        return;
    }
    else
    {
        Block->block_num-=3;
        Block->mb_num = m_CurMBAddr;
    }

    return;

} // void H264SegmentDecoder::GetTopRightLocationForCurrentMBLumaMBAFF(H264DecoderBlockLocation *Block)

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaNonMBAFFBMEH(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num-16,1))
    {
        Block->block_num+=1;
        Block->mb_num=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    }
    else
    {
        Block->block_num--;
        Block->mb_num=m_CurMBAddr;
    }

    return;

}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaNonMBAFFH2(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num-16,2))
    {
        Block->block_num+=1;
        Block->mb_num=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    }
    else
    {
        Block->block_num--;
        Block->mb_num=m_CurMBAddr;
    }

    return;

}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaNonMBAFFH4(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num-16,3))
    {
        Block->block_num+=3;
        Block->mb_num=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    }
    else
    {
        Block->block_num--;
        Block->mb_num=m_CurMBAddr;
    }

    return;

}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaNonMBAFFBMEH(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,1))
    {
        Block->block_num+=2;
        Block->mb_num  = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else
    {
        Block->block_num-=2;
        Block->mb_num = m_CurMBAddr;
    }
    return;
}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaNonMBAFFH2(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,2))
    {
        Block->block_num+=6;
        Block->mb_num  = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else
    {
        Block->block_num-=2;
        Block->mb_num = m_CurMBAddr;
    }
    return;
}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaNonMBAFFH4(H264DecoderBlockLocation *Block)
{
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,3))
    {
        Block->block_num+=12;
        Block->mb_num  = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
    }
    else
    {
        Block->block_num-=4;
        Block->mb_num = m_CurMBAddr;
    }
    return;
}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaMBAFFBMEH(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t MB_X=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    int32_t MB_N;
    Block->block_num-=16;
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num,1))
    {
        if (MB_X>=0) //left mb addr vaild?
        {
            uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
            if (curmbfff)
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        // 1 1 1
                        MB_N=MB_X;
                    }
                    else
                    {
                        // 1 1 0
                        uint32_t yN = Block->block_num/2, xN=Block->block_num%2;
                        yN/=2;
                        Block->block_num=yN*2+xN;
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 1 0 1
                        MB_N=MB_X+1;
                    }
                    else
                    {
                        // 1 0 0
                        uint32_t yN = Block->block_num/2, xN=Block->block_num%2;
                        yN+=2;
                        yN/=2;
                        Block->block_num=yN*2+xN;
                        MB_N=MB_X;
                    }
                }
            }
            else
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        //0 1 1
                        uint32_t yN = Block->block_num/2, xN=Block->block_num%2;
                        if (yN<1)
                        {
                            yN*=2;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN*=2;
                            yN-=2;
                            MB_N=MB_X+1;
                        }
                        Block->block_num=yN*2+xN;
                    }
                    else
                    {
                        // 0 1 0
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 0 0 1
                        uint32_t yN = Block->block_num/2, xN=Block->block_num%2;
                        if (yN<1)
                        {
                            yN*=8;
                            yN++;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN*=8;
                            yN-=7;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*2+xN;
                    }
                    else
                    {
                        // 0 0 0
                        MB_N=MB_X + 1;
                    }
                }
            }
        }
        else
        {
            Block->mb_num = -1;//no left neighbours
            return;
        }
        Block->block_num+=16;
        Block->block_num+=1;
        Block->mb_num = MB_N;
    }
    else
    {
        Block->block_num+=16;
        Block->block_num--;
        Block->mb_num = m_CurMBAddr;
    }
    return;
}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaMBAFFH2(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t MB_X=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    int32_t MB_N;
    Block->block_num-=16;
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num,2))
    {
        if (MB_X>=0)
        {
            uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
            if (curmbfff)
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        // 1 1 1
                        MB_N=MB_X;
                    }
                    else
                    {
                        // 1 1 0
                        uint32_t yN = Block->block_num/2;
                        yN/=2;
                        Block->block_num = yN*2;
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 1 0 1
                        MB_N=MB_X + 1;
                    }
                    else
                    {
                        // 1 0 0
                        uint32_t yN = Block->block_num/2;
                        yN*=4;
                        yN+=16;
                        yN/=2;
                        Block->block_num=(yN/4)*2;
                        MB_N=MB_X;
                    }
                }
            }
            else
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        //0 1 1
                        uint32_t yN = Block->block_num/2;
                        yN*=4;
                        yN*=2;
                        if (yN<16)
                        {
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN-=16;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*2;
                    }
                    else
                    {
                        // 0 1 0
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 0 0 1
                        uint32_t yN = Block->block_num/2;
                        yN*=4;
                        yN*=2;
                        if (yN<16)
                        {
                            yN++;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN-=15;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*2;
                    }
                    else
                    {
                        // 0 0 0
                        MB_N=MB_X + 1;
                    }
                }
            }
        }
        else
        {
            Block->mb_num = -1;//no left neighbours
            return;
        }

        Block->block_num+=16;
        Block->block_num++;
        Block->mb_num = MB_N;
    }
    else
    {
        Block->block_num+=16;
        Block->block_num++;
        Block->mb_num = m_CurMBAddr;
    }
}

inline
void H264SegmentDecoder::GetLeftLocationForCurrentMBChromaMBAFFH4(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t MB_X=m_cur_mb.CurrentMacroblockNeighbours.mb_A;
    int32_t MB_N;
    Block->block_num-=16;
    //chroma
    if (CHROMA_BLOCK_IS_ON_LEFT_EDGE(Block->block_num,3))
    {
        if (MB_X>=0) //left mb addr vaild?
        {
            uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
            if (curmbfff)
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        // 1 1 1
                        MB_N=MB_X;
                    }
                    else
                    {
                        // 1 1 0
                        uint32_t yN = Block->block_num/4;
                        yN/=2;
                        Block->block_num=yN*4;
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 1 0 1
                        MB_N=MB_X+1;
                    }
                    else
                    {
                        // 1 0 0
                        uint32_t yN = Block->block_num/4;
                        yN+=16;
                        yN/=2;
                        Block->block_num=yN*4;
                        MB_N=MB_X;
                    }
                }
            }
            else
            {
                if (curmbtf)
                {
                    if (xfff)
                    {
                        //0 1 1
                        uint32_t yN = Block->block_num/4;
                        if (yN<1)
                        {
                            yN*=2;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN*=2;
                            yN-=4;
                            MB_N=MB_X+1;
                        }
                        Block->block_num=yN*4;
                    }
                    else
                    {
                        // 0 1 0
                        MB_N=MB_X;
                    }
                }
                else
                {
                    if (xfff)
                    {
                        // 0 0 1
                        uint32_t yN = Block->block_num/2;
                        if (yN<1)
                        {
                            yN*=8;
                            yN++;
                            MB_N=MB_X;
                        }
                        else
                        {
                            yN*=8;
                            yN-=15;
                            MB_N=MB_X + 1;
                        }
                        Block->block_num=(yN/4)*4;
                    }
                    else
                    {
                        // 0 0 0
                        MB_N=MB_X + 1;
                    }
                }
            }
        }
        else
        {
            Block->mb_num = -1;//no left neighbours
            return;
        }
        Block->block_num+=16;
        Block->mb_num = MB_N;
    }
    else
    {
        Block->block_num+=16;
        Block->block_num--;
        Block->mb_num = m_CurMBAddr;
    }
    return;
}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaMBAFFBMEH(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t pair_offset = curmbtf? 1:-1;
    int32_t MB_X;
    int32_t MB_N;
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,1))
    {
        if (curmbfff && !curmbtf )
        {
            MB_N = m_CurMBAddr + pair_offset;
            Block->block_num+=2;
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
            if (MB_X>=0)
            {
                uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
                if (!curmbfff && curmbtf && !xfff)
                {
                    MB_N=MB_X;
                    Block->block_num+=2;
                }
                else
                {
                    //if (!curmbff && curmbtf && xfff)
                    //    Block->block_num+=0;
                    //else
                    Block->block_num+=2;
                    MB_N=MB_X + 1;
                }
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }
        Block->mb_num = MB_N;
        return;
    }
    else
    {
        Block->block_num-=2;
        Block->mb_num = m_CurMBAddr;
        return;
    }

}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaMBAFFH2(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t pair_offset = curmbtf? 1:-1;
    int32_t MB_X;
    int32_t MB_N;
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,2))
    {
        if (curmbfff && !curmbtf )
        {
            MB_N = m_CurMBAddr + pair_offset;
            Block->block_num+=6;
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
            if (MB_X>=0)
            {
                uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
                if (!curmbfff && curmbtf && !xfff)
                {
                    MB_N=MB_X;
                    Block->block_num+=6;
                }
                else
                {
                    //if (!curmbff && curmbtf && xfff)
                    //    Block->block_num+=0;
                    //else
                    Block->block_num+=6;
                    MB_N=MB_X + 1;
                }
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }
        Block->mb_num = MB_N;
        return;
    }
    else
    {
        Block->block_num-=2;
        Block->mb_num = m_CurMBAddr;
        return;
    }

}

inline
void H264SegmentDecoder::GetTopLocationForCurrentMBChromaMBAFFH4(H264DecoderBlockLocation *Block)
{
    bool curmbfff = !pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
    bool curmbtf  = !(m_CurMBAddr & 1);
    int32_t pair_offset = curmbtf? 1:-1;
    int32_t MB_X;
    int32_t MB_N;
    //chroma
    if (CHROMA_BLOCK_IS_ON_TOP_EDGE(Block->block_num-16,3))
    {
        if (curmbfff && !curmbtf )
        {
            MB_N = m_CurMBAddr + pair_offset;
            Block->block_num+=12;
        }
        else
        {
            MB_X = m_cur_mb.CurrentMacroblockNeighbours.mb_B;
            if (MB_X>=0)
            {
                uint8_t xfff=!GetMBFieldDecodingFlag(m_gmbinfo->mbs[MB_X]);
                if (!curmbfff && curmbtf && !xfff)
                {
                    MB_N=MB_X;
                    Block->block_num+=12;
                }
                else
                {
                    Block->block_num+=12;
                    MB_N=MB_X + 1;
                }
            }
            else
            {
                Block->mb_num = -1;
                return;
            }
        }
        Block->mb_num = MB_N;
        return;
    }
    else
    {
        Block->block_num-=4;
        Block->mb_num = m_CurMBAddr;
        return;
    }

}

void H264SegmentDecoder::UpdateNeighbouringBlocksBMEH(int32_t DeblockCalls)
{
    H264DecoderBlockNeighboursInfo * pN = &m_cur_mb.CurrentBlockNeighbours;

    pN->mb_above.block_num=0;
    pN->mb_above_chroma[0].block_num=16;
    pN->mb_above_left.block_num=0;
    pN->mb_above_right.block_num=3;
    pN->mbs_left[0].block_num=0;
    pN->mbs_left[1].block_num=4;
    pN->mbs_left[2].block_num=8;
    pN->mbs_left[3].block_num=12;
    pN->mbs_left_chroma[0][0].block_num=16;
    pN->mbs_left_chroma[0][1].block_num=16+2;
    if (m_isMBAFF)
    {
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaMBAFF(&pN->mb_above,DeblockCalls);
        GetTopRightLocationForCurrentMBLumaMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaMBAFFBMEH(&pN->mb_above_chroma[0]);
        GetLeftLocationForCurrentMBChromaMBAFFBMEH(&pN->mbs_left_chroma[0][0]);
        GetLeftLocationForCurrentMBChromaMBAFFBMEH(&pN->mbs_left_chroma[0][1]);
    }
    else
    {
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaNonMBAFF(&pN->mb_above);
        GetTopRightLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaNonMBAFFBMEH(&pN->mb_above_chroma[0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFBMEH(&pN->mbs_left_chroma[0][0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFBMEH(&pN->mbs_left_chroma[0][1]);
    }
    pN->mbs_left_chroma[1][0]=pN->mbs_left_chroma[0][0];
    pN->mbs_left_chroma[1][1]=pN->mbs_left_chroma[0][1];
    pN->mb_above_chroma[1] = pN->mb_above_chroma[0];
    pN->mbs_left_chroma[1][0].block_num+=4;
    pN->mbs_left_chroma[1][1].block_num+=4;
    pN->mb_above_chroma[1].block_num+=4;
}

void H264SegmentDecoder::UpdateNeighbouringBlocksH2(int32_t DeblockCalls)
{
    H264DecoderBlockNeighboursInfo * pN = &m_cur_mb.CurrentBlockNeighbours;

    pN->mb_above.block_num=0;
    pN->mb_above_chroma[0].block_num=16;
    pN->mb_above_left.block_num=0;
    pN->mb_above_right.block_num=3;
    pN->mbs_left[0].block_num=0;
    pN->mbs_left[1].block_num=4;
    pN->mbs_left[2].block_num=8;
    pN->mbs_left[3].block_num=12;
    pN->mbs_left_chroma[0][0].block_num=16;
    pN->mbs_left_chroma[0][1].block_num=16+2;
    pN->mbs_left_chroma[0][2].block_num=16+4;
    pN->mbs_left_chroma[0][3].block_num=16+6;
    if (m_isMBAFF)
    {
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaMBAFF(&pN->mb_above,DeblockCalls);
        GetTopRightLocationForCurrentMBLumaMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaMBAFFH2(&pN->mb_above_chroma[0]);
        GetLeftLocationForCurrentMBChromaMBAFFH2(&pN->mbs_left_chroma[0][0]);
        GetLeftLocationForCurrentMBChromaMBAFFH2(&pN->mbs_left_chroma[0][1]);
        GetLeftLocationForCurrentMBChromaMBAFFH2(&pN->mbs_left_chroma[0][2]);
        GetLeftLocationForCurrentMBChromaMBAFFH2(&pN->mbs_left_chroma[0][3]);
    }
    else
    {
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaNonMBAFF(&pN->mb_above);
        GetTopRightLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaNonMBAFFH2(&pN->mb_above_chroma[0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH2(&pN->mbs_left_chroma[0][0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH2(&pN->mbs_left_chroma[0][1]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH2(&pN->mbs_left_chroma[0][2]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH2(&pN->mbs_left_chroma[0][3]);
    }
    pN->mbs_left_chroma[1][0]=pN->mbs_left_chroma[0][0];
    pN->mbs_left_chroma[1][1]=pN->mbs_left_chroma[0][1];
    pN->mbs_left_chroma[1][2]=pN->mbs_left_chroma[0][2];
    pN->mbs_left_chroma[1][3]=pN->mbs_left_chroma[0][3];
    pN->mb_above_chroma[1] = pN->mb_above_chroma[0];
    pN->mbs_left_chroma[1][0].block_num+=8;
    pN->mbs_left_chroma[1][1].block_num+=8;
    pN->mbs_left_chroma[1][2].block_num+=8;
    pN->mbs_left_chroma[1][3].block_num+=8;
    pN->mb_above_chroma[1].block_num+=8;
}

void H264SegmentDecoder::UpdateNeighbouringBlocksH4(int32_t DeblockCalls)
{
    H264DecoderBlockNeighboursInfo * pN = &m_cur_mb.CurrentBlockNeighbours;

    pN->mb_above.block_num=0;
    pN->mb_above_chroma[0].block_num=16;
    pN->mb_above_left.block_num=0;
    pN->mb_above_right.block_num=3;
    pN->mbs_left[0].block_num=0;
    pN->mbs_left[1].block_num=4;
    pN->mbs_left[2].block_num=8;
    pN->mbs_left[3].block_num=12;
    pN->mbs_left_chroma[0][0].block_num=16;
    pN->mbs_left_chroma[0][1].block_num=16+4;
    pN->mbs_left_chroma[0][2].block_num=16+8;
    pN->mbs_left_chroma[0][3].block_num=16+12;
    if (m_isMBAFF)
    {
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaMBAFF(&pN->mb_above,DeblockCalls);
        GetTopRightLocationForCurrentMBLumaMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaMBAFFH4(&pN->mb_above_chroma[0]);

        pN->mbs_left_chroma[0][0] = pN->mbs_left[0];
        pN->mbs_left_chroma[0][1] = pN->mbs_left[1];
        pN->mbs_left_chroma[0][2] = pN->mbs_left[2];
        pN->mbs_left_chroma[0][3] = pN->mbs_left[3];
        pN->mbs_left_chroma[0][0].block_num +=16;
        pN->mbs_left_chroma[0][1].block_num +=16;
        pN->mbs_left_chroma[0][2].block_num +=16;
        pN->mbs_left_chroma[0][3].block_num +=16;
    }
    else
    {
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[0]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[1]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[2]);
        GetLeftLocationForCurrentMBLumaNonMBAFF(&pN->mbs_left[3]);
        GetTopLocationForCurrentMBLumaNonMBAFF(&pN->mb_above);
        GetTopRightLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_right);
        GetTopLeftLocationForCurrentMBLumaNonMBAFF(&pN->mb_above_left);
        GetTopLocationForCurrentMBChromaNonMBAFFH4(&pN->mb_above_chroma[0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH4(&pN->mbs_left_chroma[0][0]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH4(&pN->mbs_left_chroma[0][1]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH4(&pN->mbs_left_chroma[0][2]);
        GetLeftLocationForCurrentMBChromaNonMBAFFH4(&pN->mbs_left_chroma[0][3]);
    }
    pN->mbs_left_chroma[1][0]=pN->mbs_left_chroma[0][0];
    pN->mbs_left_chroma[1][1]=pN->mbs_left_chroma[0][1];
    pN->mbs_left_chroma[1][2]=pN->mbs_left_chroma[0][2];
    pN->mbs_left_chroma[1][3]=pN->mbs_left_chroma[0][3];
    pN->mb_above_chroma[1] = pN->mb_above_chroma[0];
    pN->mbs_left_chroma[1][0].block_num+=16;
    pN->mbs_left_chroma[1][1].block_num+=16;
    pN->mbs_left_chroma[1][2].block_num+=16;
    pN->mbs_left_chroma[1][3].block_num+=16;
    pN->mb_above_chroma[1].block_num+=16;
}

inline
void H264SegmentDecoder::UpdateCurrentMBInfo()
{
/*    if (0 == (((int32_t) m_isMBAFF) |
              ((int32_t) m_isSliceGroups)) && m_cur_mb.isInited)
    {
        m_cur_mb.GlobalMacroblockInfo++;
        m_cur_mb.LocalMacroblockInfo++;
        m_cur_mb.MacroblockCoeffsInfo++;
        m_cur_mb.MVs[0]++;
        m_cur_mb.MVs[1]++;
        m_cur_mb.MVDelta[0]++;
        m_cur_mb.MVDelta[1]++;
        m_cur_mb.RefIdxs[0] = &m_cur_mb.GlobalMacroblockInfo->refIdxs[0];
        m_cur_mb.RefIdxs[1] = &m_cur_mb.GlobalMacroblockInfo->refIdxs[1];
        return;
    }*/

    m_cur_mb.GlobalMacroblockInfo = &m_gmbinfo->mbs[m_CurMBAddr];
    m_cur_mb.LocalMacroblockInfo = &m_mbinfo.mbs[m_CurMBAddr];
    m_cur_mb.MacroblockCoeffsInfo = &m_mbinfo.MacroblockCoeffsInfo[m_CurMBAddr];
    m_cur_mb.MVs[0] = &m_gmbinfo->MV[0][m_CurMBAddr];
    m_cur_mb.MVs[1] = &m_gmbinfo->MV[1][m_CurMBAddr];
    m_cur_mb.MVDelta[0] = &m_mbinfo.MVDeltas[0][m_CurMBAddr];
    m_cur_mb.MVDelta[1] = &m_mbinfo.MVDeltas[1][m_CurMBAddr];
    m_cur_mb.RefIdxs[0] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[0];
    m_cur_mb.RefIdxs[1] = &m_gmbinfo->mbs[m_CurMBAddr].refIdxs[1];

    m_cur_mb.isInited = true;

    if (m_isMBAFF)
    {
        m_PairMBAddr = (m_CurMBAddr & 1) ? m_CurMBAddr-1 : m_CurMBAddr+1;
        m_cur_mb.GlobalMacroblockPairInfo = &m_gmbinfo->mbs[m_PairMBAddr];
        m_cur_mb.LocalMacroblockPairInfo = &m_mbinfo.mbs[m_PairMBAddr];
    }

} // void H264SegmentDecoder::UpdateCurrentMBInfo()

inline
void H264SegmentDecoder::UpdateNeighbouringAddresses()
{
    if (0 == (((int32_t) m_isMBAFF) |
              ((int32_t) m_isSliceGroups)))
    {
        UpdateNeighbouringAddressesField();
    }
    else
    {
        H264DecoderMacroblockNeighboursInfo * pN = &m_cur_mb.CurrentMacroblockNeighbours;

        if (m_isSliceGroups)
        {
            if (!((int32_t)m_isMBAFF && (m_CurMBAddr & 1))) //update only if top mb recieved
            {
                int32_t real_mb_width = mb_width*((int32_t)m_isMBAFF + 1);
                int32_t mb_left_offset = (int32_t)m_isMBAFF + 1;
                pN->mb_A = m_CurMB_X>0? m_CurMBAddr - mb_left_offset : -1;
                pN->mb_B = m_CurMB_Y>0? m_CurMBAddr - real_mb_width :-1;
                pN->mb_C = (m_CurMB_Y > 0 && m_CurMB_X < ((signed)mb_width) - 1) ? (m_CurMBAddr - real_mb_width + mb_left_offset) : (-1);
                pN->mb_D = m_CurMB_Y>0&&m_CurMB_X>0?m_CurMBAddr-real_mb_width-mb_left_offset:-1;
                if (m_bNeedToCheckMBSliceEdges)
                {
                    if (pN->mb_A>=0 &&
                        (m_cur_mb.GlobalMacroblockInfo->slice_id != m_gmbinfo->mbs[pN->mb_A].slice_id))
                        pN->mb_A = -1;
                    if (pN->mb_B>=0 &&
                        (m_cur_mb.GlobalMacroblockInfo->slice_id != m_gmbinfo->mbs[pN->mb_B].slice_id))
                        pN->mb_B = -1;
                    if (pN->mb_C>=0 &&
                        (m_cur_mb.GlobalMacroblockInfo->slice_id != m_gmbinfo->mbs[pN->mb_C].slice_id))
                        pN->mb_C = -1;
                    if (pN->mb_D>=0 && (m_cur_mb.GlobalMacroblockInfo->slice_id != m_gmbinfo->mbs[pN->mb_D].slice_id))
                        pN->mb_D = -1;
                }
            }
        }
        else
        {
            if (!((int32_t)m_isMBAFF && (m_CurMBAddr & 1))) //update only if top mb recieved
            {
                int32_t real_mb_width = mb_width*((int32_t)m_isMBAFF + 1);
                int32_t mb_left_offset = (int32_t)m_isMBAFF + 1;
                pN->mb_A = m_CurMB_X>0? m_CurMBAddr - mb_left_offset : -1;
                pN->mb_B = m_CurMB_Y>0? m_CurMBAddr - real_mb_width :-1;
                pN->mb_C = (m_CurMB_Y > 0 && m_CurMB_X < ((signed)mb_width) - 1) ? (m_CurMBAddr - real_mb_width + mb_left_offset) : (-1);
                pN->mb_D = m_CurMB_Y>0&&m_CurMB_X>0?m_CurMBAddr-real_mb_width-mb_left_offset:-1;
                if (m_bNeedToCheckMBSliceEdges)
                {
                    if (pN->mb_A>=0 && (pN->mb_A < m_iFirstSliceMb))
                        pN->mb_A = -1;
                    if (pN->mb_B>=0 && (pN->mb_B < m_iFirstSliceMb))
                        pN->mb_B = -1;
                    if (pN->mb_C>=0 && (pN->mb_C < m_iFirstSliceMb))
                        pN->mb_C = -1;
                    if (pN->mb_D>=0 && (pN->mb_D < m_iFirstSliceMb))
                        pN->mb_D = -1;
                }
            }
        }

        m_cur_mb.GlobalMacroblockPairInfo = &m_gmbinfo->mbs[m_PairMBAddr];
        m_cur_mb.LocalMacroblockPairInfo = &m_mbinfo.mbs[m_PairMBAddr];
    }

} // void H264SegmentDecoder::UpdateNeighbouringAddresses(uint8_t IgnoreSliceEdges)

inline
void H264SegmentDecoder::UpdateNeighbouringAddressesField(void)
{
    H264DecoderMacroblockNeighboursInfo *pNInfo = &(m_cur_mb.CurrentMacroblockNeighbours);
    int32_t iFirst = m_iFirstSliceMb;
    int32_t iMBWidth = mb_width;

    if (m_CurMB_X && m_CurMB_Y)
    {
        if (iFirst <= m_CurMBAddr - iMBWidth - 1)
        {
            pNInfo->mb_A = m_CurMBAddr - 1;
            pNInfo->mb_B = m_CurMBAddr - iMBWidth;
            pNInfo->mb_C = (m_CurMB_X + 1 < iMBWidth) ? (m_CurMBAddr - iMBWidth + 1) : (-1);
            pNInfo->mb_D = m_CurMBAddr - iMBWidth - 1;
        }
        else
        {
            pNInfo->mb_A = (iFirst <= m_CurMBAddr - 1) ? (m_CurMBAddr - 1) : (-1);
            pNInfo->mb_B = (iFirst <= m_CurMBAddr - iMBWidth) ? (m_CurMBAddr - iMBWidth) : (-1);
            pNInfo->mb_C = ((m_CurMB_X + 1 < iMBWidth) && (iFirst <= m_CurMBAddr - iMBWidth + 1)) ? (m_CurMBAddr - iMBWidth + 1) : (-1);
            pNInfo->mb_D = (iFirst <= m_CurMBAddr - iMBWidth - 1) ? (m_CurMBAddr - iMBWidth - 1) : (-1);
        }
    }
    else if (m_CurMB_X)
    {
        pNInfo->mb_A = (iFirst <= m_CurMBAddr - 1) ? (m_CurMBAddr - 1) : (-1);
        pNInfo->mb_B = -1;
        pNInfo->mb_C = -1;
        pNInfo->mb_D = -1;
    }
    else if (m_CurMB_Y)
    {
        pNInfo->mb_A = -1;
        pNInfo->mb_B = (iFirst <= m_CurMBAddr - iMBWidth) ? (m_CurMBAddr - iMBWidth) : (-1);
        pNInfo->mb_C = (m_CurMB_X + 1 < iMBWidth) && (iFirst <= m_CurMBAddr - iMBWidth + 1) ? (m_CurMBAddr - iMBWidth + 1) : (-1);
        pNInfo->mb_D = -1;
    }
    else
    {
        pNInfo->mb_A = -1;
        pNInfo->mb_B = -1;
        pNInfo->mb_C = -1;
        pNInfo->mb_D = -1;
    }

} // void H264SegmentDecoder::UpdateNeighbouringAddressesField(void)


} // namespace UMC

#endif /* __UMC_H264_SEGMENT_DECODER_INLINES_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
