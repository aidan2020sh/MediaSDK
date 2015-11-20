//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_ddi.h"
#include <assert.h>
#include <math.h>

namespace MfxHwH265Encode
{

const mfxU32 TableA1[][6] =
{
//  Level   |MaxLumaPs |    MaxCPB    | MaxSlice  | MaxTileRows | MaxTileCols
//                                      Segments  |
//                                      PerPicture|
/*  1   */ {   36864,      350,    350,        16,            1,           1},
/*  2   */ {  122880,     1500,   1500,        16,            1,           1},
/*  2.1 */ {  245760,     3000,   3000,        20,            1,           1},
/*  3   */ {  552960,     6000,   6000,        30,            2,           2},
/*  3.1 */ {  983040,    10000,  10000,        40,            3,           3},
/*  4   */ { 2228224,    12000,  30000,        75,            5,           5},
/*  4.1 */ { 2228224,    20000,  50000,        75,            5,           5},
/*  5   */ { 8912896,    25000, 100000,       200,           11,          10},
/*  5.1 */ { 8912896,    40000, 160000,       200,           11,          10},
/*  5.2 */ { 8912896,    60000, 240000,       200,           11,          10},
/*  6   */ {35651584,    60000, 240000,       600,           22,          20},
/*  6.1 */ {35651584,   120000, 480000,       600,           22,          20},
/*  6.2 */ {35651584,   240000, 800000,       600,           22,          20},
};

const mfxU32 TableA2[][4] =
{
//  Level   | MaxLumaSr |      MaxBR   | MinCr
/*  1    */ {    552960,    128,    128,    2},
/*  2    */ {   3686400,   1500,   1500,    2},
/*  2.1  */ {   7372800,   3000,   3000,    2},
/*  3    */ {  16588800,   6000,   6000,    2},
/*  3.1  */ {  33177600,  10000,  10000,    2},
/*  4    */ {  66846720,  12000,  30000,    4},
/*  4.1  */ { 133693440,  20000,  50000,    4},
/*  5    */ { 267386880,  25000, 100000,    6},
/*  5.1  */ { 534773760,  40000, 160000,    8},
/*  5.2  */ {1069547520,  60000, 240000,    8},
/*  6    */ {1069547520,  60000, 240000,    8},
/*  6.1  */ {2139095040, 120000, 480000,    8},
/*  6.2  */ {4278190080, 240000, 800000,    6},
};

const mfxU16 MaxLidx = (sizeof(TableA1) / sizeof(TableA1[0]));

const mfxU16 LevelIdxToMfx[] =
{
    MFX_LEVEL_HEVC_1 ,
    MFX_LEVEL_HEVC_2 ,
    MFX_LEVEL_HEVC_21,
    MFX_LEVEL_HEVC_3 ,
    MFX_LEVEL_HEVC_31,
    MFX_LEVEL_HEVC_4 ,
    MFX_LEVEL_HEVC_41,
    MFX_LEVEL_HEVC_5 ,
    MFX_LEVEL_HEVC_51,
    MFX_LEVEL_HEVC_52,
    MFX_LEVEL_HEVC_6 ,
    MFX_LEVEL_HEVC_61,
    MFX_LEVEL_HEVC_62,
};

inline mfxU16 MaxTidx(mfxU16 l) { return (LevelIdxToMfx[Min(l, MaxLidx)] >= MFX_LEVEL_HEVC_4); }

mfxU16 LevelIdx(mfxU16 mfx_level)
{
    switch (mfx_level & 0xFF)
    {
    case  MFX_LEVEL_HEVC_1  : return  0;
    case  MFX_LEVEL_HEVC_2  : return  1;
    case  MFX_LEVEL_HEVC_21 : return  2;
    case  MFX_LEVEL_HEVC_3  : return  3;
    case  MFX_LEVEL_HEVC_31 : return  4;
    case  MFX_LEVEL_HEVC_4  : return  5;
    case  MFX_LEVEL_HEVC_41 : return  6;
    case  MFX_LEVEL_HEVC_5  : return  7;
    case  MFX_LEVEL_HEVC_51 : return  8;
    case  MFX_LEVEL_HEVC_52 : return  9;
    case  MFX_LEVEL_HEVC_6  : return 10;
    case  MFX_LEVEL_HEVC_61 : return 11;
    case  MFX_LEVEL_HEVC_62 : return 12;
    default: break;
    }
    assert("unknown level");
    return 0;
}

mfxU16 TierIdx(mfxU16 mfx_level)
{
    return !!(mfx_level & MFX_TIER_HEVC_HIGH);
}

mfxU16 MfxLevel(mfxU16 l, mfxU16 t){ return LevelIdxToMfx[Min(l, MaxLidx)] | (MFX_TIER_HEVC_HIGH * !!t); }

mfxU32 GetMaxDpbSize(mfxU32 PicSizeInSamplesY, mfxU32 MaxLumaPs, mfxU32 maxDpbPicBuf)
{
    if (PicSizeInSamplesY <= (MaxLumaPs >> 2))
        return Min(4 * maxDpbPicBuf, 16U);
    else if (PicSizeInSamplesY <= (MaxLumaPs >> 1))
        return Min( 2 * maxDpbPicBuf, 16U);
    else if (PicSizeInSamplesY <= ((3 * MaxLumaPs) >> 2))
        return Min( (4 * maxDpbPicBuf ) / 3, 16U);

    return maxDpbPicBuf;
}

mfxStatus CheckProfile(mfxVideoParam& par)
{
    mfxStatus sts = MFX_ERR_NONE;

    switch (par.mfx.CodecProfile)
    {
    case 0:
        break;

    case MFX_PROFILE_HEVC_MAIN10:
        break;

    case MFX_PROFILE_HEVC_MAINSP:
        if (par.mfx.GopPicSize > 1)
        {
            par.mfx.GopPicSize = 1;
            sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }

    case MFX_PROFILE_HEVC_MAIN:
        if (par.mfx.FrameInfo.BitDepthLuma > 8)
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        break;

    default:
        return MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

mfxU32 GetMaxDpbSizeByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    mfxU32 MaxLumaPs         = TableA1[LevelIdx(par.mfx.CodecLevel)][0];
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;

    return GetMaxDpbSize(PicSizeInSamplesY, MaxLumaPs, 6);
}

mfxU32 GetMaxKbpsByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    return TableA2[LevelIdx(par.mfx.CodecLevel)][1+TierIdx(par.mfx.CodecLevel)] * 1100 / 1000;
}

mfxF64 GetMaxFrByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    mfxU32 MaxLumaSr   = TableA2[LevelIdx(par.mfx.CodecLevel)][0];
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;

    return (mfxF64)MaxLumaSr / PicSizeInSamplesY;
}

mfxU32 GetMaxCpbInKBByLevel(MfxVideoParam const & par)
{
    assert(par.mfx.CodecLevel != 0);

    return TableA1[LevelIdx(par.mfx.CodecLevel)][1+TierIdx(par.mfx.CodecLevel)] * 1100 / 8000;
}

mfxStatus CorrectLevel(MfxVideoParam& par, bool bCheckOnly)
{
    mfxStatus sts = MFX_ERR_NONE;
    //mfxU32 CpbBrVclFactor    = 1000;
    mfxU32 CpbBrNalFactor    = 1100;
    mfxU32 PicSizeInSamplesY = (mfxU32)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples;
    mfxU32 LumaSr            = Ceil(mfxF64(PicSizeInSamplesY) * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
    mfxU16 NewLevel          = par.mfx.CodecLevel;

    mfxU16 lidx = LevelIdx(par.mfx.CodecLevel);
    mfxU16 tidx = TierIdx(par.mfx.CodecLevel);

    if (tidx > MaxTidx(lidx))
        tidx = 0;

    while (lidx < MaxLidx)
    {
        mfxU32 MaxLumaPs   = TableA1[lidx][0];
        mfxU32 MaxCPB      = TableA1[lidx][1+tidx];
        mfxU32 MaxSSPP     = TableA1[lidx][3];
        mfxU32 MaxTileRows = TableA1[lidx][4];
        mfxU32 MaxTileCols = TableA1[lidx][5];
        mfxU32 MaxLumaSr   = TableA2[lidx][0];
        mfxU32 MaxBR       = TableA2[lidx][1+tidx];
        //mfxU32 MinCR       = TableA2[lidx][3];
        mfxU32 MaxDpbSize  = GetMaxDpbSize(PicSizeInSamplesY, MaxLumaPs, 6);

        if (   PicSizeInSamplesY > MaxLumaPs
            || par.m_ext.HEVCParam.PicWidthInLumaSamples > sqrt((mfxF64)MaxLumaPs * 8)
            || par.m_ext.HEVCParam.PicHeightInLumaSamples > sqrt((mfxF64)MaxLumaPs * 8)
            || (mfxU32)par.mfx.NumRefFrame + 1 > MaxDpbSize
            || par.m_ext.HEVCTiles.NumTileColumns > MaxTileCols
            || par.m_ext.HEVCTiles.NumTileRows > MaxTileRows
            || (mfxU32)par.mfx.NumSlice > MaxSSPP
            || (par.isBPyramid() && MaxDpbSize < mfxU32((par.mfx.GopRefDist - 1) / 2 + 1)))
        {
            lidx ++;
            continue;
        }

        //if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            if (   par.BufferSizeInKB * 8000 > CpbBrNalFactor * MaxCPB
                || LumaSr > MaxLumaSr
                || par.MaxKbps * 1000 > CpbBrNalFactor * MaxBR
                || par.TargetKbps * 1000 > CpbBrNalFactor * MaxBR)
            {
                if (tidx >= MaxTidx(lidx))
                {
                    lidx ++;
                    tidx = 0;
                }
                else
                    tidx ++;

                continue;
            }
        }

        break;
    }

    NewLevel = MfxLevel(lidx, tidx);

    if (par.mfx.CodecLevel != NewLevel)
    {
        par.mfx.CodecLevel = bCheckOnly ? 0 : NewLevel;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }
    return sts;
}

mfxU16 AddTileSlices(
    MfxVideoParam& par,
    mfxU32 SliceStructure,
    mfxU32 nCol,
    mfxU32 nRow,
    mfxU32 nSlice)
{
    mfxU32 nLCU = nCol * nRow;
    mfxU32 nLcuPerSlice = nLCU / nSlice;
    mfxU32 nSlicePrev = (mfxU32)par.m_slice.size();

    if (par.m_ext.CO2.NumMbPerSlice != 0)
        nLcuPerSlice = par.m_ext.CO2.NumMbPerSlice;

    if (SliceStructure == 0)
    {
        nSlice = 1;
        nLcuPerSlice = nLCU;
    }
    else if (SliceStructure < 4)
    {
        nSlice = Min(nSlice, nRow);
        mfxU32 nRowsPerSlice = CeilDiv(nRow, nSlice);

        if (SliceStructure == 1)
        {
            mfxU32 r0 = nRowsPerSlice;
            mfxU32 r1 = nRowsPerSlice;

            while (((r0 & (r0 - 1)) || (nRow % r0)) && r0 < nRow)
                r0 ++;

            while (((r1 & (r1 - 1)) || (nRow % r1)) && r1)
                r1 --;

            nRowsPerSlice = (r1 > 0 && (nRowsPerSlice - r1 < r0 - nRowsPerSlice)) ? r1 : r0;

            nSlice = CeilDiv(nRow, nRowsPerSlice);
        }
        else
        {
            while (nRowsPerSlice * (nSlice - 1) >= nRow)
            {
                nSlice ++;
                nRowsPerSlice = CeilDiv(nRow, nSlice);
            }
        }

        nLcuPerSlice = nRowsPerSlice * nCol;
    }

    par.m_slice.resize(nSlicePrev + nSlice);

    /*
    mfxU32 f = (nSlice < 2 || (nLCU % nSlice == 0)) ? 0 : (nLCU % nSlice);
    bool   s = false;

    if (f == 0)
    {
        f;
    }
    else if (f > nSlice / 2)
    {
        s = false;
        nLcuPerSlice += 1;
        f = (nSlice / (nSlice - f));

        while ((nLcuPerSlice * nSlice - (nLCU / f) > nLCU) && f)
            f --;
    }
    else
    {
        s = true;
        f = nSlice / f;
    }

    */
    mfxF64 k = (mfxF64)nLCU / (mfxF64) nSlice;

    mfxU32 i = nSlicePrev;
    for ( ; i < par.m_slice.size(); i ++)
    {
        //par.m_slice[i].NumLCU = (i == nSlicePrev + nSlice-1) ? nLCU : nLcuPerSlice + s * (f && i % f == 0) - !s * (f && i % f == 0);
        //par.m_slice[i].SegmentAddress = (i > 0) ? (par.m_slice[i-1].SegmentAddress + par.m_slice[i-1].NumLCU) : 0;

        par.m_slice[i].SegmentAddress = (mfxU32)(k*(mfxF64)i);
        if (i != 0)
            par.m_slice[i - 1].NumLCU = par.m_slice[i].SegmentAddress - par.m_slice[i - 1].SegmentAddress;
    }
    if ((i > 0) && ((i - 1) < par.m_slice.size()))
    {
        par.m_slice[i - 1].NumLCU = par.m_slice[nSlicePrev].SegmentAddress + nLCU - par.m_slice[i - 1].SegmentAddress ;
    }

    return (mfxU16)nSlice;
}

struct tmpTileInfo
{
    mfxU32 id;
    mfxU32 nCol;
    mfxU32 nRow;
    mfxU32 nLCU;
    mfxU32 nSlice;
};

inline mfxF64 nSliceCoeff(tmpTileInfo const & tile)
{
    assert(tile.nSlice > 0);
    return (mfxF64(tile.nLCU) / tile.nSlice);
}

mfxU16 MakeSlices(MfxVideoParam& par, mfxU32 SliceStructure)
{
    mfxU32 nCol   = CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);
    mfxU32 nRow   = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize);
    mfxU32 nTCol  = Max<mfxU32>(par.m_ext.HEVCTiles.NumTileColumns, 1);
    mfxU32 nTRow  = Max<mfxU32>(par.m_ext.HEVCTiles.NumTileRows, 1);
    mfxU32 nTile  = nTCol * nTRow;
    mfxU32 nLCU   = nCol * nRow;
    mfxU32 nSlice = Min(Min<mfxU32>(nLCU, MAX_SLICES), Max<mfxU32>(par.mfx.NumSlice, 1));

    par.m_slice.resize(0);
    if (par.m_ext.CO2.NumMbPerSlice != 0)
        nSlice = CeilDiv(nLCU / nTile, par.m_ext.CO2.NumMbPerSlice) * nTile;

    if (SliceStructure == 0)
        nSlice = 1;

    if (nSlice > 1)
        nSlice = Max(nSlice, nTile);

    if (nTile == 1) //TileScan = RasterScan, no SegmentAddress conversion required
        return (mfxU16)AddTileSlices(par, SliceStructure, nCol, nRow, nSlice);

    std::vector<mfxU32> colWidth(nTCol, 0);
    std::vector<mfxU32> rowHeight(nTRow, 0);
    std::vector<mfxU32> colBd(nTCol+1, 0);
    std::vector<mfxU32> rowBd(nTRow+1, 0);
    std::vector<mfxU32> TsToRs(nLCU);

    //assume uniform spacing
    for (mfxU32 i = 0; i < nTCol; i ++)
        colWidth[i] = ((i + 1) * nCol) / nTCol - (i * nCol) / nTCol;

    for (mfxU32 j = 0; j < nTRow; j ++)
        rowHeight[j] = ((j + 1) * nRow) / nTRow - (j * nRow) / nTRow;

    for (mfxU32 i = 0; i < nTCol; i ++)
        colBd[i + 1] = colBd[i] + colWidth[i];

    for (mfxU32 j = 0; j < nTRow; j ++)
        rowBd[j + 1] = rowBd[j] + rowHeight[j];

    for (mfxU32 rso = 0; rso < nLCU; rso ++)
    {
        mfxU32 tbX = rso % nCol;
        mfxU32 tbY = rso / nCol;
        mfxU32 tileX = 0;
        mfxU32 tileY = 0;
        mfxU32 tso   = 0;

        for (mfxU32 i = 0; i < nTCol; i ++)
            if (tbX >= colBd[i])
                tileX = i;

        for (mfxU32 j = 0; j < nTRow; j ++)
            if (tbY >= rowBd[j])
                tileY = j;

        for (mfxU32 i = 0; i < tileX; i ++)
            tso += rowHeight[tileY] * colWidth[i];

        for (mfxU32 j = 0; j < tileY; j ++)
            tso += nCol * rowHeight[j];

        tso += (tbY - rowBd[tileY]) * colWidth[tileX] + tbX - colBd[tileX];

        assert(tso < nLCU);

        TsToRs[tso] = rso;
    }

    if (nSlice == 1)
    {
        AddTileSlices(par, SliceStructure, nCol, nRow, nSlice);
    }
    else
    {
        std::vector<tmpTileInfo> tile(nTile);
        mfxU32 id = 0;
        mfxU32 nLcuPerSlice = CeilDiv(nLCU, nSlice);
        mfxU32 nSliceRest   = nSlice;

        for (mfxU32 j = 0; j < nTRow; j ++)
        {
            for (mfxU32 i = 0; i < nTCol; i ++)
            {
                tile[id].id = id;
                tile[id].nCol = colWidth[i];
                tile[id].nRow = rowHeight[j];
                tile[id].nLCU = tile[id].nCol * tile[id].nRow;
                tile[id].nSlice = Max(1U, tile[id].nLCU / nLcuPerSlice);
                nSliceRest -= tile[id].nSlice;
                id ++;
            }
        }

        if (nSliceRest)
        {
            while (nSliceRest)
            {
                MFX_SORT_COMMON(tile, tile.size(), nSliceCoeff(tile[_i]) < nSliceCoeff(tile[_j]));

                assert(tile[0].nLCU > tile[0].nSlice);

                tile[0].nSlice ++;
                nSliceRest --;
            }

            MFX_SORT_STRUCT(tile, tile.size(), id, >);
        }

        for (mfxU32 i = 0; i < tile.size(); i ++)
            AddTileSlices(par, SliceStructure, tile[i].nCol, tile[i].nRow, tile[i].nSlice);
    }

    for (mfxU32 i = 0; i < par.m_slice.size(); i ++)
    {
        assert(par.m_slice[i].SegmentAddress < nLCU);

        par.m_slice[i].SegmentAddress = TsToRs[par.m_slice[i].SegmentAddress];
    }

    return (mfxU16)par.m_slice.size();
}

bool CheckTriStateOption(mfxU16 & opt)
{
    if (opt != MFX_CODINGOPTION_UNKNOWN &&
        opt != MFX_CODINGOPTION_ON &&
        opt != MFX_CODINGOPTION_OFF)
    {
        opt = MFX_CODINGOPTION_UNKNOWN;
        return true;
    }

    return false;
}

template <class T, class U0>
bool CheckMin(T & opt, U0 min)
{
    if (opt < min)
    {
        opt = T(min);
        return true;
    }

    return false;
}

template <class T, class U0>
bool CheckMax(T & opt, U0 max)
{
    if (opt > max)
    {
        opt = T(max);
        return true;
    }

    return false;
}

template <class T, class U0, class U1>
bool CheckRange(T & opt, U0 min, U1 max)
{
    if (opt < min)
    {
        opt = T(min);
        return true;
    }

    if (opt > max)
    {
        opt = T(max);
        return true;
    }

    return false;
}

template <class T, class U0, class U1, class U2>
bool CheckRangeDflt(T & opt, U0 min, U1 max, U2 deflt)
{
    if ((opt < T(min) || opt > T(max)) && opt != T(deflt))
    {
        opt = T(deflt);
        return true;
    }

    return false;
}

template <class T, class U0>
bool CheckOption(T & opt, U0 deflt)
{
    if (opt == T(deflt))
        return false;

    opt = T(deflt);
    return true;
}

#if 0 //(_MSC_VER >= 1800) // VS 2013+
template <class T, class U0, class... U1>
bool CheckOption(T & opt, U0 deflt, U1... supprtd)
{
    if (opt == T(deflt))
        return false;

    if (CheckOption(opt, supprtd...))
    {
        opt = T(deflt);
        return true;
    }

    return false;
}
#else
template <class T, class U0, class U1>
bool CheckOption(T & opt, U0 deflt, U1 s0)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
template <class T, class U0, class U1, class U2>
bool CheckOption(T & opt, U0 deflt, U1 s0, U2 s1)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0, (T)s1))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
template <class T, class U0, class U1, class U2, class U3>
bool CheckOption(T & opt, U0 deflt, U1 s0, U2 s1, U3 s2)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0, (T)s1, (T)s2))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
template <class T, class U0, class U1, class U2, class U3, class U4>
bool CheckOption(T & opt, U0 deflt, U1 s0, U2 s1, U3 s2, U4 s3)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0, (T)s1, (T)s2, (T)s3))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
template <class T, class U0, class U1, class U2, class U3, class U4, class U5>
bool CheckOption(T & opt, U0 deflt, U1 s0, U2 s1, U3 s2, U4 s3, U5 s4)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0, (T)s1, (T)s2, (T)s3, (T)s4))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
template <class T, class U0, class U1, class U2, class U3, class U4, class U5, class U6>
bool CheckOption(T & opt, U0 deflt, U1 s0, U2 s1, U3 s2, U4 s3, U5 s4, U6 s5)
{
    if (opt == T(deflt)) return false;
    if (CheckOption(opt, (T)s0, (T)s1, (T)s2, (T)s3, (T)s4, (T)s5))
    {
        opt = T(deflt);
        return true;
    }
    return false;
}
#endif

bool CheckTU(mfxU8 support, mfxU16& tu)
{
    assert(tu < 8);

    mfxI16 abs_diff = 0;
    bool   sign = 0;
    mfxI16 newtu = tu;
    bool   changed = false;

    do
    {
        newtu = tu + (1 - 2 * sign) * abs_diff;
        abs_diff += !sign;
        sign = !sign;
    } while (!(support & (1 << (newtu - 1))) && newtu > 0);

    changed = (tu != newtu);
    tu      = newtu;

    return changed;
}

mfxU16 minRefForPyramid(mfxU16 GopRefDist)
{
    assert(GopRefDist > 0);
    return (GopRefDist - 1) / 2 + 3;
}
 const mfxU16 AVBR_ACCURACY_MIN = 1;
 const mfxU16 AVBR_ACCURACY_MAX = 65535;

 const mfxU16 AVBR_CONVERGENCE_MIN = 1;
 const mfxU16 AVBR_CONVERGENCE_MAX = 65535;

template <class T>
void InheritOption(T optInit, T & optReset)
{
    if (optReset == 0)
        optReset = optInit;
}

void InheritDefaultValues(
    MfxVideoParam const & parInit,
    MfxVideoParam &       parReset)
{
    InheritOption(parInit.AsyncDepth,             parReset.AsyncDepth);
    InheritOption(parInit.mfx.BRCParamMultiplier, parReset.mfx.BRCParamMultiplier);
    InheritOption(parInit.mfx.CodecId,            parReset.mfx.CodecId);
    InheritOption(parInit.mfx.CodecProfile,       parReset.mfx.CodecProfile);
    InheritOption(parInit.mfx.CodecLevel,         parReset.mfx.CodecLevel);
    InheritOption(parInit.mfx.NumThread,          parReset.mfx.NumThread);
    InheritOption(parInit.mfx.TargetUsage,        parReset.mfx.TargetUsage);
    InheritOption(parInit.mfx.GopPicSize,         parReset.mfx.GopPicSize);
    InheritOption(parInit.mfx.GopRefDist,         parReset.mfx.GopRefDist);
    InheritOption(parInit.mfx.GopOptFlag,         parReset.mfx.GopOptFlag);
    InheritOption(parInit.mfx.IdrInterval,        parReset.mfx.IdrInterval);
    InheritOption(parInit.mfx.RateControlMethod,  parReset.mfx.RateControlMethod);
    InheritOption(parInit.mfx.BufferSizeInKB,     parReset.mfx.BufferSizeInKB);
    InheritOption(parInit.mfx.NumSlice,           parReset.mfx.NumSlice);
    InheritOption(parInit.mfx.NumRefFrame,        parReset.mfx.NumRefFrame);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {

        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        printf("parInit.mfx.RateControlMethod CBR1 : %d, %d\n", parInit.mfx.MaxKbps, parReset.mfx.MaxKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CQP && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        InheritOption(parInit.mfx.QPI, parReset.mfx.QPI);
        InheritOption(parInit.mfx.QPP, parReset.mfx.QPP);
        InheritOption(parInit.mfx.QPB, parReset.mfx.QPB);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_AVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        InheritOption(parInit.mfx.Accuracy,    parReset.mfx.Accuracy);
        InheritOption(parInit.mfx.Convergence, parReset.mfx.Convergence);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_ICQ && parReset.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        InheritOption(parInit.mfx.ICQQuality, parReset.mfx.ICQQuality);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VCM && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }


    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.Width,          parReset.mfx.FrameInfo.Width);
    InheritOption(parInit.mfx.FrameInfo.Height,         parReset.mfx.FrameInfo.Height);
    InheritOption(parInit.mfx.FrameInfo.CropX,          parReset.mfx.FrameInfo.CropX);
    InheritOption(parInit.mfx.FrameInfo.CropY,          parReset.mfx.FrameInfo.CropY);
    InheritOption(parInit.mfx.FrameInfo.CropW,          parReset.mfx.FrameInfo.CropW);
    InheritOption(parInit.mfx.FrameInfo.CropH,          parReset.mfx.FrameInfo.CropH);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtN,  parReset.mfx.FrameInfo.FrameRateExtN);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtD,  parReset.mfx.FrameInfo.FrameRateExtD);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioW,   parReset.mfx.FrameInfo.AspectRatioW);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioH,   parReset.mfx.FrameInfo.AspectRatioH);

    mfxExtHEVCParam const * extHEVCInit  = &parInit.m_ext.HEVCParam;
    mfxExtHEVCParam *       extHEVCReset = &parReset.m_ext.HEVCParam;

    InheritOption(extHEVCInit->GeneralConstraintFlags,  extHEVCReset->GeneralConstraintFlags);
    //InheritOption(extHEVCInit->PicHeightInLumaSamples,  extHEVCReset->PicHeightInLumaSamples);
    //InheritOption(extHEVCInit->PicWidthInLumaSamples,   extHEVCReset->PicWidthInLumaSamples);

    mfxExtHEVCTiles const * extHEVCTilInit  = &parInit.m_ext.HEVCTiles;
    mfxExtHEVCTiles *       extHEVCTilReset = &parReset.m_ext.HEVCTiles;

    InheritOption(extHEVCTilInit->NumTileColumns,  extHEVCTilReset->NumTileColumns);
    InheritOption(extHEVCTilInit->NumTileRows,     extHEVCTilReset->NumTileRows);

    mfxExtCodingOption2 const * extOpt2Init  = &parInit.m_ext.CO2;
    mfxExtCodingOption2 *       extOpt2Reset = &parReset.m_ext.CO2;

    InheritOption(extOpt2Init->IntRefType,      extOpt2Reset->IntRefType);
    InheritOption(extOpt2Init->IntRefCycleSize, extOpt2Reset->IntRefCycleSize);
    InheritOption(extOpt2Init->IntRefQPDelta,   extOpt2Reset->IntRefQPDelta);
    InheritOption(extOpt2Init->MBBRC,      extOpt2Reset->MBBRC);
    InheritOption(extOpt2Init->BRefType,   extOpt2Reset->BRefType);
    InheritOption(extOpt2Init->NumMbPerSlice,   extOpt2Reset->NumMbPerSlice);
    InheritOption(extOpt2Init->DisableDeblockingIdc,  extOpt2Reset->DisableDeblockingIdc);

    mfxExtCodingOption3 const * extOpt3Init  = &parInit.m_ext.CO3;
    mfxExtCodingOption3 *       extOpt3Reset = &parReset.m_ext.CO3;

    InheritOption(extOpt3Init->IntRefCycleDist, extOpt3Reset->IntRefCycleDist);
    InheritOption(extOpt3Init->PRefType, extOpt3Reset->PRefType);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_QVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
        InheritOption(extOpt3Init->QVBRQuality,     extOpt3Reset->QVBRQuality);
    }


    // not inherited:
    // InheritOption(parInit.mfx.FrameInfo.PicStruct,      parReset.mfx.FrameInfo.PicStruct);
    // InheritOption(parInit.IOPattern,                    parReset.IOPattern);
    // InheritOption(parInit.mfx.FrameInfo.ChromaFormat,   parReset.mfx.FrameInfo.ChromaFormat);
}

mfxStatus CheckVideoParam(MfxVideoParam& par, ENCODE_CAPS_HEVC const & caps, bool bInit = false)
{
    mfxU32 unsupported = 0, changed = 0, incompatible = 0;
    mfxStatus sts = MFX_ERR_NONE;

    mfxF64 maxFR   = 300.;
    mfxU32 maxBR   = 0xFFFFFFFF;
    mfxU32 maxBuf  = 0xFFFFFFFF;
    mfxU16 maxDPB  = 16;

    if (par.mfx.CodecLevel)
    {
        maxFR = GetMaxFrByLevel(par);
        maxBR = GetMaxKbpsByLevel(par);
        maxBuf = GetMaxCpbInKBByLevel(par);
        maxDPB = (mfxU16)GetMaxDpbSizeByLevel(par);
    }
    if ((!par.mfx.FrameInfo.Width) ||
        (!par.mfx.FrameInfo.Height))
    {
        return MFX_ERR_UNSUPPORTED;
    }
    if (bInit)
    {
        unsupported     += CheckMin(par.mfx.FrameInfo.Width,  Align(par.mfx.FrameInfo.Width, par.LCUSize));
        unsupported     += CheckMin(par.mfx.FrameInfo.Height, Align(par.mfx.FrameInfo.Height, par.LCUSize));
    }
    else
    {
        changed     += CheckMin(par.mfx.FrameInfo.Width, Align(par.mfx.FrameInfo.Width, par.LCUSize));
        changed     += CheckMin(par.mfx.FrameInfo.Height, Align(par.mfx.FrameInfo.Height, par.LCUSize));
    }

    unsupported += CheckMax(par.mfx.FrameInfo.Width, caps.MaxPicWidth);
    unsupported += CheckMax(par.mfx.FrameInfo.Height, caps.MaxPicHeight);

    incompatible += CheckMax(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.mfx.FrameInfo.Width);
    incompatible += CheckMax(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.mfx.FrameInfo.Height);
    changed      += CheckMin(par.m_ext.HEVCParam.PicWidthInLumaSamples, Align(par.m_ext.HEVCParam.PicWidthInLumaSamples, CODED_PIC_ALIGN_W));
    changed      += CheckMin(par.m_ext.HEVCParam.PicHeightInLumaSamples, Align(par.m_ext.HEVCParam.PicHeightInLumaSamples, CODED_PIC_ALIGN_H));

    unsupported += CheckOption(par.mfx.FrameInfo.BitDepthLuma, 8, 0);
    unsupported += CheckOption(par.mfx.FrameInfo.BitDepthChroma, 8, 0);

    if (   caps.TileSupport == 0
        && (par.m_ext.HEVCTiles.NumTileColumns > 1 || par.m_ext.HEVCTiles.NumTileRows > 1))
    {
        par.m_ext.HEVCTiles.NumTileColumns = 1;
        par.m_ext.HEVCTiles.NumTileRows    = 1;
        unsupported ++;
    }

    unsupported += CheckMax(par.m_ext.HEVCTiles.NumTileColumns, (mfxU32)MAX_NUM_TILE_COLUMNS);
    unsupported += CheckMax(par.m_ext.HEVCTiles.NumTileRows, (mfxU32)MAX_NUM_TILE_ROWS);

    changed += CheckMax(par.mfx.TargetUsage, (mfxU32)MFX_TARGETUSAGE_BEST_SPEED);

    if (par.mfx.TargetUsage && caps.TUSupport)
        changed += CheckTU(caps.TUSupport, par.mfx.TargetUsage);

    changed += CheckMax(par.mfx.GopRefDist, caps.SliceIPOnly ? 1 : (par.mfx.GopPicSize ? par.mfx.GopPicSize - 1 : 0xFFFF));

    unsupported += CheckOption(par.Protected, 0);

    changed += CheckOption(par.IOPattern
        , (mfxU32)MFX_IOPATTERN_IN_VIDEO_MEMORY
        , (mfxU32)MFX_IOPATTERN_IN_SYSTEM_MEMORY
        , (mfxU32)MFX_IOPATTERN_IN_OPAQUE_MEMORY
        , 0);

    if (par.mfx.RateControlMethod == (mfxU32)MFX_RATECONTROL_AVBR)
    {
        par.mfx.RateControlMethod = (mfxU32)MFX_RATECONTROL_CBR;
        par.mfx.Accuracy = 0;
        par.mfx.Convergence = 0;
        changed ++;
    }


    unsupported += CheckOption(par.mfx.RateControlMethod
        , 0
        , (mfxU32)MFX_RATECONTROL_CBR
        , (mfxU32)MFX_RATECONTROL_VBR
        , (mfxU32)MFX_RATECONTROL_AVBR
        , (mfxU32)MFX_RATECONTROL_CQP
        , (mfxU32)MFX_RATECONTROL_ICQ
        , caps.VCMBitRateControl ? MFX_RATECONTROL_VCM : 0);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        unsupported += CheckMax(par.mfx.ICQQuality, 51);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (par.mfx.Accuracy)
            changed += CheckRange(par.mfx.Accuracy, AVBR_ACCURACY_MIN, AVBR_ACCURACY_MAX);
        if (par.mfx.Convergence)
            changed += CheckRange(par.mfx.Convergence, AVBR_CONVERGENCE_MIN, AVBR_CONVERGENCE_MAX);
    }


    changed += CheckTriStateOption(par.m_ext.CO2.MBBRC);

    if (caps.MBBRCSupport == 0 || par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        changed += CheckOption(par.m_ext.CO2.MBBRC, (mfxU32)MFX_CODINGOPTION_OFF, 0);

    changed += CheckOption(par.mfx.FrameInfo.PicStruct, (mfxU16)MFX_PICSTRUCT_PROGRESSIVE, 0);

    if (par.m_ext.HEVCParam.PicWidthInLumaSamples > 0)
    {
        changed += CheckRange(par.mfx.FrameInfo.CropX, 0, par.m_ext.HEVCParam.PicWidthInLumaSamples);
        changed += CheckRange(par.mfx.FrameInfo.CropW, 0, par.m_ext.HEVCParam.PicWidthInLumaSamples - par.mfx.FrameInfo.CropX);
    }

    if (par.m_ext.HEVCParam.PicHeightInLumaSamples > 0)
    {
        changed += CheckRange(par.mfx.FrameInfo.CropY, 0, par.m_ext.HEVCParam.PicHeightInLumaSamples);
        changed += CheckRange(par.mfx.FrameInfo.CropH, 0, par.m_ext.HEVCParam.PicHeightInLumaSamples - par.mfx.FrameInfo.CropY);
    }

    unsupported += CheckOption(par.mfx.FrameInfo.ChromaFormat, (mfxU16)MFX_CHROMAFORMAT_YUV420, 0);
    unsupported += CheckOption(par.mfx.FrameInfo.FourCC, (mfxU32)MFX_FOURCC_NV12, 0);

    if (par.mfx.FrameInfo.FrameRateExtN && par.mfx.FrameInfo.FrameRateExtD) // FR <= 300
    {
        if (par.mfx.FrameInfo.FrameRateExtN > (mfxU32)300 * par.mfx.FrameInfo.FrameRateExtD)
        {
            par.mfx.FrameInfo.FrameRateExtN = par.mfx.FrameInfo.FrameRateExtD = 0;
            unsupported++;
        }
    }

    if ((par.mfx.FrameInfo.FrameRateExtN == 0) !=
        (par.mfx.FrameInfo.FrameRateExtD == 0))
    {
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
        incompatible ++;
    }

    changed += CheckMax(par.mfx.NumRefFrame, maxDPB);

    changed += CheckMax(par.m_ext.DDI.NumActiveRefBL0, caps.MaxNum_Reference0);
    changed += CheckMax(par.m_ext.DDI.NumActiveRefBL1, caps.MaxNum_Reference1);

    if (   (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
        && par.MaxKbps != 0
        && par.TargetKbps != 0
        && par.MaxKbps < par.TargetKbps)
    {
        par.MaxKbps = par.TargetKbps;
        changed ++;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
        && par.MaxKbps != par.TargetKbps
        && par.MaxKbps!= 0
        && par.TargetKbps!= 0)
    {
        par.MaxKbps = par.TargetKbps;
        changed ++;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        changed += CheckRangeDflt(par.mfx.QPI, 1, 51, 0);
        changed += CheckRangeDflt(par.mfx.QPP, 1, 51, 0);
        changed += CheckRangeDflt(par.mfx.QPB, 1, 51, 0);
    }

    if (par.BufferSizeInKB != 0)
    {
        mfxU32 rawBytes = par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples * 3 / 2 / 1000;

        if (  (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            || par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
            && par.BufferSizeInKB < rawBytes)
        {
            par.BufferSizeInKB = rawBytes;
            changed ++;
        }
        else if (   par.mfx.RateControlMethod != MFX_RATECONTROL_CQP
                 || par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ)
        {
            mfxF64 fr = par.mfx.FrameInfo.FrameRateExtD ? (mfxF64)par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD : 30.;
            mfxU32 avgFS = Ceil((mfxF64)par.TargetKbps / fr / 8);

            changed += CheckRange(par.BufferSizeInKB, avgFS * 2 + 1, maxBuf);
        }
    }

    if (par.m_ext.CO2.NumMbPerSlice != 0)
    {
        mfxU32 nLCU  = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize) * CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);
        mfxU32 nTile = Max<mfxU32>(par.m_ext.HEVCTiles.NumTileColumns, 1) * Max<mfxU32>(par.m_ext.HEVCTiles.NumTileRows, 1);

        changed += CheckRange(par.m_ext.CO2.NumMbPerSlice, 0, nLCU / nTile);
    }

    changed += CheckOption(par.mfx.NumSlice, MakeSlices(par, caps.SliceStructure), 0);

    if (   par.m_ext.CO2.BRefType == MFX_B_REF_PYRAMID
           && par.mfx.GopRefDist > 0
           && ( par.mfx.GopRefDist < 2
            || minRefForPyramid(par.mfx.GopRefDist) > 16
            || (par.mfx.NumRefFrame && minRefForPyramid(par.mfx.GopRefDist) > par.mfx.NumRefFrame)))
    {
        par.m_ext.CO2.BRefType = MFX_B_REF_OFF;
        changed ++;
    }
    if (par.mfx.GopRefDist > 1 && par.mfx.NumRefFrame == 1)
    {
        par.mfx.NumRefFrame = 2;
        changed ++;
    }

    if (par.m_ext.CO3.PRefType == MFX_P_REF_PYRAMID &&  par.mfx.GopRefDist > 1)
    {
        par.m_ext.CO3.PRefType = MFX_P_REF_DEFAULT;
        changed ++;
    }

    {
        mfxU16 prev = 0;

        unsupported += CheckOption(par.m_ext.AVCTL.Layer[0].Scale, 0, 1);
        unsupported += CheckOption(par.m_ext.AVCTL.Layer[7].Scale, 0);

        for (mfxU16 i = 1; i < 7; i++)
        {
            if (!par.m_ext.AVCTL.Layer[i].Scale)
                continue;

            if (par.m_ext.AVCTL.Layer[i].Scale <= par.m_ext.AVCTL.Layer[prev].Scale
                || par.m_ext.AVCTL.Layer[i].Scale %  par.m_ext.AVCTL.Layer[prev].Scale)
            {
                par.m_ext.AVCTL.Layer[i].Scale = 0;
                unsupported++;
                break;
            }
            prev = i;
        }

        if (par.isTL())
            changed += CheckOption(par.mfx.GopRefDist, 1, 0);
    }

    if (par.m_ext.CO2.IntRefCycleSize != 0 &&
        par.mfx.GopPicSize != 0 &&
        par.m_ext.CO2.IntRefCycleSize >= par.mfx.GopPicSize)
    {
        // refresh cycle length shouldn't be greater or equal to GOP size
        par.m_ext.CO2.IntRefType = 0;
        par.m_ext.CO2.IntRefCycleSize = 0;
        changed = true;
    }

    if (par.m_ext.CO3.IntRefCycleDist != 0 &&
        par.mfx.GopPicSize != 0 &&
        par.m_ext.CO3.IntRefCycleDist >= par.mfx.GopPicSize)
    {
        // refresh period length shouldn't be greater or equal to GOP size
        par.m_ext.CO2.IntRefType = 0;
        par.m_ext.CO3.IntRefCycleDist = 0;
        changed = true;
    }

    if (par.m_ext.CO3.IntRefCycleDist != 0 &&
        par.m_ext.CO2.IntRefCycleSize != 0 &&
        par.m_ext.CO2.IntRefCycleSize > par.m_ext.CO3.IntRefCycleDist)
    {
        // refresh period shouldn't be greater than refresh cycle size
        par.m_ext.CO3.IntRefCycleDist = 0;
        changed = true;
    }

    sts = CheckProfile(par);

    if (sts >= MFX_ERR_NONE && par.mfx.CodecLevel > 0)
    {
        if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) changed +=1;
        sts = CorrectLevel(par, false);

    }

    if (sts == MFX_ERR_NONE && changed)
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    if (sts >= MFX_ERR_NONE && unsupported)
        sts = MFX_ERR_UNSUPPORTED;

    if (sts >= MFX_ERR_NONE && incompatible)
        sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

    return sts;
}

void SetDefaults(
    MfxVideoParam &          par,
    ENCODE_CAPS_HEVC const & hwCaps)
{
    mfxU64 rawBits = (mfxU64)par.m_ext.HEVCParam.PicWidthInLumaSamples * par.m_ext.HEVCParam.PicHeightInLumaSamples * 3 / 2 * 8;
    mfxF64 maxFR   = 300.;
    mfxU32 maxBR   = 0xFFFFFFFF;
    mfxU32 maxBuf  = 0xFFFFFFFF;
    mfxU16 maxDPB  = 16;

    if (par.mfx.CodecLevel)
    {
        maxFR  = GetMaxFrByLevel(par);
        maxBR  = GetMaxKbpsByLevel(par);
        maxBuf = GetMaxCpbInKBByLevel(par);
        maxDPB = (mfxU16)GetMaxDpbSizeByLevel(par);
    }

    if (!par.AsyncDepth)
        par.AsyncDepth = 5;

    if (!par.mfx.CodecProfile)
        par.mfx.CodecProfile = mfxU16(par.mfx.FrameInfo.BitDepthLuma > 8 ? MFX_PROFILE_HEVC_MAIN10 : MFX_PROFILE_HEVC_MAIN);

    if (!par.mfx.TargetUsage)
        par.mfx.TargetUsage = 4;

    if (hwCaps.TUSupport)
        CheckTU(hwCaps.TUSupport, par.mfx.TargetUsage);

    if (!par.m_ext.HEVCTiles.NumTileColumns)
        par.m_ext.HEVCTiles.NumTileColumns = 1;

    if (!par.m_ext.HEVCTiles.NumTileRows)
        par.m_ext.HEVCTiles.NumTileRows = 1;

    if (!par.mfx.NumSlice || !par.m_slice.size())
    {
        MakeSlices(par, hwCaps.SliceStructure);
        par.mfx.NumSlice = (mfxU16)par.m_slice.size();
    }

    if (!par.mfx.FrameInfo.FourCC)
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    if (!par.mfx.FrameInfo.CropW)
        par.mfx.FrameInfo.CropW = par.m_ext.HEVCParam.PicWidthInLumaSamples - par.mfx.FrameInfo.CropX;

    if (!par.mfx.FrameInfo.CropH)
        par.mfx.FrameInfo.CropH = par.m_ext.HEVCParam.PicHeightInLumaSamples - par.mfx.FrameInfo.CropY;

    if (!par.mfx.FrameInfo.FrameRateExtN || !par.mfx.FrameInfo.FrameRateExtD)
    {
        par.mfx.FrameInfo.FrameRateExtD = 1001;
        par.mfx.FrameInfo.FrameRateExtN = mfxU32(Min(30000./par.mfx.FrameInfo.FrameRateExtD, maxFR) * par.mfx.FrameInfo.FrameRateExtD);
    }

    if (!par.mfx.FrameInfo.AspectRatioW)
        par.mfx.FrameInfo.AspectRatioW = 1;

    if (!par.mfx.FrameInfo.AspectRatioH)
        par.mfx.FrameInfo.AspectRatioH = 1;

    if (!par.mfx.FrameInfo.PicStruct)
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    if (!par.mfx.FrameInfo.ChromaFormat)
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (!par.mfx.FrameInfo.BitDepthLuma)
        par.mfx.FrameInfo.BitDepthLuma = 8;

    if (!par.mfx.FrameInfo.BitDepthChroma)
        par.mfx.FrameInfo.BitDepthChroma = par.mfx.FrameInfo.BitDepthLuma;

    if (!par.mfx.RateControlMethod)
        par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        if (!par.mfx.QPI)
            par.mfx.QPI = 26;
        if (!par.mfx.QPP)
            par.mfx.QPP = (mfxU16) Min (par.mfx.QPI + 2, 51);
        if (!par.mfx.QPB)
            par.mfx.QPB = (mfxU16) Min (par.mfx.QPP + 2, 51);

        if (!par.BufferSizeInKB)
            par.BufferSizeInKB = Min(maxBuf, mfxU32(rawBits / 8000));

        if (par.m_ext.CO2.MBBRC == MFX_CODINGOPTION_UNKNOWN)
            par.m_ext.CO2.MBBRC = MFX_CODINGOPTION_OFF;

    }
    else if (   par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
    {
        if (!par.BufferSizeInKB)
            par.BufferSizeInKB = Min(maxBuf, mfxU32(rawBits / 8000));
    }
    else if (   par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
             || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR
             || par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        if (!par.TargetKbps)
            par.TargetKbps = Min(maxBR, mfxU32(rawBits * par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD / 150000));
        if (!par.MaxKbps)
            par.MaxKbps = par.TargetKbps;
        if (!par.BufferSizeInKB)
            par.BufferSizeInKB = Min(maxBuf, par.MaxKbps / 16);
        if (!par.InitialDelayInKB)
            par.InitialDelayInKB = par.BufferSizeInKB / 2;
        if (par.m_ext.CO2.MBBRC == MFX_CODINGOPTION_UNKNOWN)
            par.m_ext.CO2.MBBRC = MFX_CODINGOPTION_ON;
    }
    else if(par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (!par.mfx.Accuracy)
            par.mfx.Accuracy =  AVBR_ACCURACY_MAX;
        if (!par.mfx.Convergence)
            par.mfx.Convergence =  AVBR_CONVERGENCE_MAX;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ && !par.mfx.ICQQuality)
        par.mfx.ICQQuality = 26;

    /*if (!par.mfx.GopOptFlag)
        par.mfx.GopOptFlag = MFX_GOP_CLOSED;*/

    if (!par.mfx.GopPicSize)
        par.mfx.GopPicSize = (par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAINSP ? 1 : 0xFFFF);

    if (!par.NumRefLX[0] && par.m_ext.DDI.NumActiveRefBL0)
        par.NumRefLX[0] = par.m_ext.DDI.NumActiveRefBL0;

    if (!par.NumRefLX[1] && par.m_ext.DDI.NumActiveRefBL1)
        par.NumRefLX[1] = par.m_ext.DDI.NumActiveRefBL1;

    if (!par.NumRefLX[0])
        par.NumRefLX[0] = hwCaps.MaxNum_Reference0;

    if (!par.NumRefLX[1])
        par.NumRefLX[1] = hwCaps.MaxNum_Reference1;

    if (!par.mfx.GopRefDist)
    {
        if (par.isTL() || hwCaps.SliceIPOnly || !par.NumRefLX[1] || par.mfx.GopPicSize < 3 || par.mfx.NumRefFrame == 1)
            par.mfx.GopRefDist = 1;
        else
            par.mfx.GopRefDist = Min<mfxU16>(par.mfx.GopPicSize - 1, 4);
    }

    if (par.m_ext.CO2.BRefType == MFX_B_REF_UNKNOWN)
    {
        if (par.mfx.GopRefDist > 3 && ((minRefForPyramid(par.mfx.GopRefDist) <= par.mfx.NumRefFrame) || par.mfx.NumRefFrame ==0))
            par.m_ext.CO2.BRefType = MFX_B_REF_PYRAMID;
        else
            par.m_ext.CO2.BRefType = MFX_B_REF_OFF;
    }

    if (par.m_ext.CO3.PRefType == MFX_P_REF_DEFAULT)
    {
        if (par.mfx.GopRefDist == 1 && par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            par.m_ext.CO3.PRefType = MFX_P_REF_PYRAMID;
        else if (par.mfx.GopRefDist == 1)
            par.m_ext.CO3.PRefType = MFX_P_REF_SIMPLE;
    }

    if (!par.mfx.NumRefFrame)
    {
        par.mfx.NumRefFrame = par.isBPyramid() ? mfxU16(minRefForPyramid(par.mfx.GopRefDist)) : (par.NumRefLX[0] + (par.mfx.GopRefDist > 1) * par.NumRefLX[1]);
        par.mfx.NumRefFrame = Max(mfxU16(par.NumTL() - 1), par.mfx.NumRefFrame);
        par.mfx.NumRefFrame = Min(maxDPB, par.mfx.NumRefFrame);
    }
    else
    {
        while (par.NumRefLX[0] + par    .NumRefLX[1] > par.mfx.NumRefFrame)
        {
            if (   par.mfx.GopRefDist == 1 && par.NumRefLX[1] == 1
                && par.NumRefLX[0] + par.NumRefLX[1] == par.mfx.NumRefFrame + 1)
                break;

            if (    par.NumRefLX[1] >= par.NumRefLX[0]
                && !(par.mfx.GopRefDist == 1 && par.NumRefLX[1] == 1))
                par.NumRefLX[1] --;
            else
                par.NumRefLX[0] --;
        }

        par.NumRefLX[0] = Max<mfxU16>(1, par.NumRefLX[0]);
        par.NumRefLX[1] = Max<mfxU16>(1, par.NumRefLX[1]);
    }

    /*if (   DEFAULT_LTR_INTERVAL > 0
        && !par.LTRInterval
        && par.NumRefLX[0] > 1
        && par.mfx.GopPicSize > (DEFAULT_LTR_INTERVAL * 2)
        && (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR))
        par.LTRInterval = DEFAULT_LTR_INTERVAL;*/

    if (!par.m_ext.AVCTL.Layer[0].Scale)
        par.m_ext.AVCTL.Layer[0].Scale = 1;

    if (!par.mfx.CodecLevel)
        CorrectLevel(par, false);

    if (par.m_ext.CO2.IntRefType && par.m_ext.CO2.IntRefCycleSize == 0)
    {
        // set intra refresh cycle to 1 sec by default
        par.m_ext.CO2.IntRefCycleSize =
            (mfxU16)((par.mfx.FrameInfo.FrameRateExtN + par.mfx.FrameInfo.FrameRateExtD - 1) / par.mfx.FrameInfo.FrameRateExtD);
    }



}

} //namespace MfxHwH265Encode
