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

#include "h265_pattern.h"
#include "umc_h265_frame.h"

namespace UMC_HEVC_DECODER
{

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tables
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Ipp8u H265Pattern::m_IntraFilter[5] =
{
    10, //4x4
    7, //8x8
    1, //16x16
    0, //32x32
    10, //64x64
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public member functions (TComPatternParam)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/** \param  piTexture     pixel data
 \param  iRoiWidth     pattern width
 \param  iRoiHeight    pattern height
 \param  iStride       buffer stride
 \param  iOffsetLeft   neighbour offset (left)
 \param  iOffsetRight  neighbour offset (right)
 \param  iOffsetAbove  neighbour offset (above)
 \param  iOffsetBottom neighbour offset (bottom)
 */
void H265PatternParam::SetPatternParamPel(Ipp32s RoiWidth,
                                          Ipp32s RoiHeight,
                                          Ipp32s OffsetLeft,
                                          Ipp32s OffsetAbove)
{
    m_ROIWidth       = RoiWidth;
    m_ROIHeight      = RoiHeight;
    m_OffsetLeft     = OffsetLeft;
    m_OffsetAbove    = OffsetAbove;
}

/**
 \param  pcCU          CU data structure
 \param  iComp         component index (0=Y, 1=Cb, 2=Cr)
 \param  iRoiWidth     pattern width
 \param  iRoiHeight    pattern height
 \param  iStride       buffer stride
 \param  iOffsetLeft   neighbour offset (left)
 \param  iOffsetRight  neighbour offset (right)
 \param  iOffsetAbove  neighbour offset (above)
 \param  iOffsetBottom neighbour offset (bottom)
 \param  uiPartDepth   CU depth
 \param  uiAbsPartIdx  part index
 */
void H265PatternParam::SetPatternParamCU(Ipp8u RoiWidth,
                                         Ipp8u RoiHeight,
                                         Ipp32s OffsetLeft,
                                         Ipp32s OffsetAbove)
{
    m_OffsetLeft = OffsetLeft;
    m_OffsetAbove = OffsetAbove;

    m_ROIWidth = RoiWidth;
    m_ROIHeight = RoiHeight;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Public member functions (H265Pattern)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void H265Pattern::InitPattern(Ipp32s RoiWidth,
                              Ipp32s RoiHeight,
                              Ipp32s OffsetLeft,
                              Ipp32s OffsetAbove)
{
    m_PatternY. SetPatternParamPel(RoiWidth, RoiHeight, OffsetLeft, OffsetAbove);
    m_PatternCb.SetPatternParamPel(RoiWidth >> 1, RoiHeight >> 1, OffsetLeft >> 1, OffsetAbove >> 1);
    m_PatternCr.SetPatternParamPel(RoiWidth >> 1, RoiHeight >> 1, OffsetLeft >> 1, OffsetAbove >> 1);

    return;
}

void H265Pattern::InitPattern(H265CodingUnit* pCU, Ipp32u PartDepth, Ipp32u AbsPartIdx)
{
    VM_ASSERT(pCU->m_AbsIdxInLCU == 0);

    Ipp32u OffsetLeft  = 0;
    Ipp32u OffsetAbove = 0;

    Ipp8u Width = (Ipp8u) (pCU->m_WidthArray[AbsPartIdx] >> PartDepth);
    Ipp8u Height = (Ipp8u) (pCU->m_HeightArray[AbsPartIdx] >> PartDepth);

    Ipp32u CurrPicPelX = pCU->m_CUPelX + g_RasterToPelX[g_ZscanToRaster[AbsPartIdx]];
    Ipp32u CurrPicPelY = pCU->m_CUPelY + g_RasterToPelY[g_ZscanToRaster[AbsPartIdx]];

    if (CurrPicPelX != 0)
    {
        OffsetLeft = 1;
    }
    if (CurrPicPelY != 0)
    {
        OffsetAbove = 1;
    }

    m_PatternY.SetPatternParamCU(Width, Height, OffsetLeft, OffsetAbove);
    m_PatternCb.SetPatternParamCU(Width >> 1, Height >> 1, OffsetLeft, OffsetAbove);
    m_PatternCr.SetPatternParamCU(Width >> 1, Height >> 1, OffsetLeft, OffsetAbove);
}

void H265Pattern::InitAdiPatternLuma(H265CodingUnit* pCU, Ipp32u ZorderIdxInPart, Ipp32u PartDepth, H265PlanePtrYCommon pAdiBuf,
                                     Ipp32u OrgBufStride, Ipp32u OrgBufHeight, bool *NeighborFlags, Ipp32s NumIntraNeighbor)
{
    H265PlanePtrYCommon pRoiOrigin;
    H265PlanePtrYCommon pAdiTemp;
    Ipp32s CUSize = pCU->m_WidthArray[ZorderIdxInPart] >> PartDepth;
    Ipp32s CUSize2 = CUSize << 1;
    Ipp32s Size;
    Ipp32s PicStride = pCU->m_Frame->pitch_luma();

    Ipp32s UnitSize = g_MaxCUWidth >> g_MaxCUDepth;
    Ipp32s NumUnitsInCU = CUSize / UnitSize;
    Ipp32s TotalUnits = (NumUnitsInCU << 2) + 1;

    Size = (CUSize << 1) + 1;

    if (((Size << 2) > OrgBufStride) || ((Size << 2) > OrgBufHeight))
    {
        return;
    }

    pRoiOrigin = pCU->m_Frame->GetLumaAddr(pCU->CUAddr, pCU->m_AbsIdxInLCU + ZorderIdxInPart);
    pAdiTemp = pAdiBuf;

    FillReferenceSamplesLuma(g_bitDepthY, pRoiOrigin, pAdiTemp, NeighborFlags, NumIntraNeighbor, UnitSize, NumUnitsInCU, TotalUnits, CUSize, Size, PicStride);

    Ipp32u i;
    // generate filtered intra prediction samples
    Ipp32u BufSize = CUSize2 + CUSize2 + 1;  // left and left above border + above and above right border + top left corner = length of 3. filter buffer
    Ipp32u WH = Size * Size;               // number of elements in one buffer

    H265PlanePtrYCommon pFilteredBuf1 = pAdiBuf + WH;        // 1. filter buffer
    H265PlanePtrYCommon pFilteredBuf2 = pFilteredBuf1 + WH;  // 2. filter buffer
    H265PlanePtrYCommon pFilterBuf = pFilteredBuf2 + WH;     // buffer for 2. filtering (sequential)
    H265PlanePtrYCommon pFilterBufN = pFilterBuf + BufSize;   // buffer for 1. filtering (sequential)

    Ipp32s l = 0;
    // left border from bottom to top
    for (i = 0; i < CUSize2; i++)
    {
        pFilterBuf[l++] = pAdiTemp[Size * (CUSize2 - i)];
    }
    // top left corner
    pFilterBuf[l++] = pAdiTemp[0];
    // above border from left to right
    for (i=0; i < CUSize2; i++)
    {
        pFilterBuf[l++] = pAdiTemp[1 + i];
    }

    if (pCU->m_SliceHeader->m_SeqParamSet->getUseStrongIntraSmoothing())
    {
        unsigned blkSize = 32;
        int bottomLeft = pFilterBuf[0];
        int topLeft = pFilterBuf[CUSize2];
        int topRight = pFilterBuf[BufSize - 1];
        int threshold = 1 << (g_bitDepthY - 5);
        bool bilinearLeft = abs(bottomLeft + topLeft - 2 * pFilterBuf[CUSize]) < threshold;
        bool bilinearAbove  = abs(topLeft + topRight - 2 * pFilterBuf[CUSize2 + CUSize]) < threshold;

        if (CUSize >= blkSize && (bilinearLeft && bilinearAbove))
        {
            int shift = g_ConvertToBit[CUSize] + 3;  // log2(uiCuSize2)
            pFilterBufN[0] = pFilterBuf[0];
            pFilterBufN[CUSize2] = pFilterBuf[CUSize2];
            pFilterBufN[BufSize - 1] = pFilterBuf[BufSize - 1];

            for (i = 1; i < CUSize2; i++)
            {
                pFilterBufN[i] = ((CUSize2 - i) * bottomLeft + i * topLeft + CUSize) >> shift;
            }

            for (i = 1; i < CUSize2; i++)
            {
                pFilterBufN[CUSize2 + i] = ((CUSize2 - i) * topLeft + i * topRight + CUSize) >> shift;
            }
        }
        else
        {
            // 1. filtering with [1 2 1]
            pFilterBufN[0] = pFilterBuf[0];
            pFilterBufN[BufSize - 1] = pFilterBuf[BufSize - 1];
            for (i = 1; i < BufSize - 1; i++)
            {
                pFilterBufN[i] = (pFilterBuf[i - 1] + 2 * pFilterBuf[i] + pFilterBuf[i + 1] + 2) >> 2;
            }
        }
    }
    else
    {
        // 1. filtering with [1 2 1]
        pFilterBufN[0] = pFilterBuf[0];
        pFilterBufN[BufSize - 1] = pFilterBuf[BufSize - 1];
        for (i = 1; i < BufSize - 1; i++)
        {
            pFilterBufN[i] = (pFilterBuf[i - 1] + 2 * pFilterBuf[i] + pFilterBuf[i + 1] + 2) >> 2;
        }
    }

    // fill 1. filter buffer with filtered values
    l = 0;
    for (i = 0; i < CUSize2; i++)
    {
        pFilteredBuf1[Size * (CUSize2 - i)] = pFilterBufN[l++];
    }
    pFilteredBuf1[0] = pFilterBufN[l++];
    for (i = 0; i < CUSize2; i++)
    {
        pFilteredBuf1[1 + i] = pFilterBufN[l++];
    }
}

void H265Pattern::FillReferenceSamplesChroma(Ipp32s bitDepth,
                                             H265PlanePtrUVCommon pRoiOrigin,
                                             H265PlanePtrUVCommon pAdiTemp,
                                             bool* NeighborFlags,
                                             Ipp32s NumIntraNeighbor,
                                             Ipp32u UnitSize,
                                             Ipp32s NumUnitsInCU,
                                             Ipp32s TotalUnits,
                                             Ipp32u CUSize,
                                             Ipp32u Size,
                                             Ipp32s PicStride)
{
    H265PlanePtrUVCommon pRoiTemp;
    Ipp32u i, j;
    Ipp32s DCValue = 1 << (bitDepth - 1);
    Ipp32s Width = Size << 1;
    Ipp32s Height = Size;
    Ipp32s CUWidth = CUSize << 1;
    Ipp32s CUHeight = CUSize;

    if (NumIntraNeighbor == 0)
    {
        // Fill border with DC value
        for (i = 0; i < Width; i++)
        {
            pAdiTemp[i] = DCValue;
        }
        for (i = 1; i < Height; i++)
        {
            pAdiTemp[i * Width] = DCValue;
            pAdiTemp[i * Width + 1] = DCValue;
        }
    }
    else if (NumIntraNeighbor == TotalUnits)
    {
        // Fill top-left border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride - 2;
        pAdiTemp[0] = pRoiTemp[0];
        pAdiTemp[1] = pRoiTemp[1];

        // Fill left border with rec. samples
        pRoiTemp = pRoiOrigin - 2;

        for (i = 0; i < CUHeight; i++)
        {
            pAdiTemp[(1 + i) * Width] = pRoiTemp[0];
            pAdiTemp[(1 + i) * Width + 1] = pRoiTemp[1];
            pRoiTemp += PicStride;
        }

        // Fill below left border with rec. samples
        for (i = 0; i < CUHeight; i++)
        {
            pAdiTemp[(1 + CUHeight + i) * Width] = pRoiTemp[0];
            pAdiTemp[(1 + CUHeight + i) * Width + 1] = pRoiTemp[1];
            pRoiTemp += PicStride;
        }

        // Fill top border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride;
        for (i = 0; i < CUWidth; i++)
        {
            pAdiTemp[2 + i] = pRoiTemp[i];
        }

        // Fill top right border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride + CUWidth;
        for (i = 0; i < CUWidth; i++)
        {
            pAdiTemp[2 + CUWidth + i] = pRoiTemp[i];
        }
    }
    else // reference samples are partially available
    {
        Ipp32u NumUnits2 = NumUnitsInCU << 1;
        Ipp32u TotalSamples = TotalUnits * UnitSize;
        H265PlaneUVCommon pAdiLine[5 * MAX_CU_SIZE];
        H265PlanePtrUVCommon pAdiLineTemp;
        bool *pNeighborFlags;
        Ipp32s Prev;
        Ipp32s Next;
        Ipp32s Curr;
        H265PlaneUVCommon pRef1 = 0, pRef2 = 0;

        // Initialize
        for (i = 0; i < TotalSamples * 2; i++)
        {
            pAdiLine[i] = DCValue;
        }

        // Fill top-left sample
        pRoiTemp = pRoiOrigin - PicStride - 2;
        pAdiLineTemp = pAdiLine + (NumUnits2 * UnitSize) * 2;
        pNeighborFlags = NeighborFlags + NumUnits2;
        if (*pNeighborFlags)
        {
            pAdiLineTemp[0] = pRoiTemp[0];
            pAdiLineTemp[1] = pRoiTemp[1];
            for (i = 2; i < UnitSize * 2; i += 2)
            {
                pAdiLineTemp[i] = pAdiLineTemp[0];
                pAdiLineTemp[i + 1] = pAdiLineTemp[1];
            }
        }

        // Fill left & below-left samples
        pRoiTemp += PicStride;

        pAdiLineTemp -= 2;
        pNeighborFlags--;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize; i++)
                {
                    pAdiLineTemp[-(Ipp32s)i * 2] = pRoiTemp[i * PicStride];
                    pAdiLineTemp[-(Ipp32s)i * 2 + 1] = pRoiTemp[i * PicStride + 1];
                }
            }
            pRoiTemp += UnitSize * PicStride;
            pAdiLineTemp -= UnitSize * 2;
            pNeighborFlags--;
        }

        // Fill above & above-right samples
        pRoiTemp = pRoiOrigin - PicStride;
        pAdiLineTemp = pAdiLine + ((NumUnits2 + 1) * UnitSize) * 2;
        pNeighborFlags = NeighborFlags + NumUnits2 + 1;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize * 2; i++)
                {
                    pAdiLineTemp[i] = pRoiTemp[i];
                }
            }
            pRoiTemp += UnitSize * 2;
            pAdiLineTemp += UnitSize * 2;
            pNeighborFlags++;
        }
        // Pad reference samples when necessary
        Prev = -1;
        Curr = 0;
        Next = 1;
        pAdiLineTemp = pAdiLine;
        while (Curr < TotalUnits)
        {
            if (!NeighborFlags[Curr])
            {
                if (Curr == 0)
                {
                    while (Next < TotalUnits && !NeighborFlags[Next])
                    {
                        Next++;
                    }
                    pRef1 = pAdiLine[Next * UnitSize * 2];
                    pRef2 = pAdiLine[Next * UnitSize * 2 + 1];
                    // Pad unavailable samples with new value
                    while (Curr < Next)
                    {
                        for (i = 0; i < UnitSize; i++)
                        {
                            pAdiLineTemp[i * 2] = pRef1;
                            pAdiLineTemp[i * 2 + 1] = pRef2;
                        }
                        pAdiLineTemp += UnitSize * 2;
                        Curr++;
                    }
                }
                else
                {
                    pRef1 = pAdiLine[(Curr * UnitSize - 1) * 2];
                    pRef2 = pAdiLine[(Curr * UnitSize - 1) * 2 + 1];
                    for (i = 0; i < UnitSize; i++)
                    {
                        pAdiLineTemp[i * 2] = pRef1;
                        pAdiLineTemp[i * 2 + 1] = pRef2;
                    }
                    pAdiLineTemp += UnitSize * 2;
                    Curr++;
                }
            }
            else
            {
                pAdiLineTemp += UnitSize * 2;
                Curr++;
            }
        }

        // Copy processed samples
        pAdiLineTemp = pAdiLine + (Height + UnitSize - 2) * 2;
        for (i = 0; i < Width; i++)
        {
            pAdiTemp[i] = pAdiLineTemp[i];
        }
        pAdiLineTemp = pAdiLine + (Height - 1) * 2;
        for (i = 1; i < Height; i++)
        {
            pAdiTemp[i * Width] = pAdiLineTemp[-(Ipp32s)i * 2];
            pAdiTemp[i * Width + 1] = pAdiLineTemp[-(Ipp32s)i * 2 + 1];
        }
    }
}

void H265Pattern::InitAdiPatternChroma(H265CodingUnit* pCU, Ipp32u ZorderIdxInPart, Ipp32u PartDepth, H265PlanePtrUVCommon pAdiBuf,
                                       Ipp32u OrgBufStride, Ipp32u OrgBufHeight, bool *NeighborFlags, Ipp32s NumIntraNeighbor)
{
    H265PlanePtrUVCommon pRoiOrigin;
    H265PlanePtrUVCommon pAdiTemp;
    Ipp32u CUSize = pCU->m_WidthArray[ZorderIdxInPart] >> PartDepth;
    Ipp32u Size;

    Ipp32s UnitSize = (g_MaxCUWidth >> g_MaxCUDepth) >> 1; // for chroma
    Ipp32s NumUnitsInCU = (CUSize / UnitSize) >> 1;            // for chroma
    Ipp32s TotalUnits = (NumUnitsInCU << 2) + 1;

    CUSize >>= 1;  // for chroma
    Size = CUSize * 2 + 1;

    if ((4 * Size > OrgBufStride) || (4 * Size > OrgBufHeight))
    {
        return;
    }

    Ipp32s PicStride = pCU->m_Frame->pitch_chroma();

    // get CbCb pattern
    pRoiOrigin = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, ZorderIdxInPart);
    pAdiTemp = pAdiBuf;

    FillReferenceSamplesChroma(g_bitDepthC, pRoiOrigin, pAdiTemp, NeighborFlags, NumIntraNeighbor, UnitSize, NumUnitsInCU, TotalUnits, CUSize, Size, PicStride);
}

void H265Pattern::FillReferenceSamplesLuma(Ipp32s bitDepth,
                                           H265PlanePtrYCommon pRoiOrigin,
                                           H265PlanePtrYCommon pAdiTemp,
                                           bool* NeighborFlags,
                                           Ipp32s NumIntraNeighbor,
                                           Ipp32u UnitSize,
                                           Ipp32s NumUnitsInCU,
                                           Ipp32s TotalUnits,
                                           Ipp32u CUSize,
                                           Ipp32u Size,
                                           Ipp32s PicStride)
{
    H265PlanePtrYCommon pRoiTemp;
    Ipp32u i, j;
    Ipp32s DCValue = 1 << (bitDepth - 1);

    if (NumIntraNeighbor == 0)
    {
        // Fill border with DC value
        for (i = 0; i < Size; i++)
        {
            pAdiTemp[i] = DCValue;
        }
        for (i = 1; i < Size; i++)
        {
            pAdiTemp[i * Size] = DCValue;
        }
    }
    else if (NumIntraNeighbor == TotalUnits)
    {
        // Fill top-left border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride - 1;
        pAdiTemp[0] = pRoiTemp[0];

        // Fill left border with rec. samples
        pRoiTemp = pRoiOrigin - 1;

        for (i = 0; i < CUSize; i++)
        {
            pAdiTemp[(1 + i) * Size] = pRoiTemp[0];
            pRoiTemp += PicStride;
        }

        // Fill below left border with rec. samples
        for (i = 0; i < CUSize; i++)
        {
            pAdiTemp[(1 + CUSize + i) * Size] = pRoiTemp[0];
            pRoiTemp += PicStride;
        }

        // Fill top border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride;
        for (i = 0; i < CUSize; i++)
        {
            pAdiTemp[1 + i] = pRoiTemp[i];
        }

        // Fill top right border with rec. samples
        pRoiTemp = pRoiOrigin - PicStride + CUSize;
        for (i = 0; i < CUSize; i++)
        {
            pAdiTemp[1 + CUSize + i] = pRoiTemp[i];
        }
    }
    else // reference samples are partially available
    {
        Ipp32u NumUnits2 = NumUnitsInCU << 1;
        Ipp32u TotalSamples = TotalUnits * UnitSize;
        H265PlaneYCommon pAdiLine[5 * MAX_CU_SIZE];
        H265PlanePtrYCommon pAdiLineTemp;
        bool *pNeighborFlags;
        Ipp32s Prev;
        Ipp32s Next;
        Ipp32s Curr;
        Ipp16s pRef = 0;

        // Initialize
        for (i = 0; i < TotalSamples; i++)
        {
            pAdiLine[i] = DCValue;
        }

        // Fill top-left sample
        pRoiTemp = pRoiOrigin - PicStride - 1;
        pAdiLineTemp = pAdiLine + (NumUnits2 * UnitSize);
        pNeighborFlags = NeighborFlags + NumUnits2;
        if (*pNeighborFlags)
        {
            pAdiLineTemp[0] = pRoiTemp[0];
            for (i = 1; i < UnitSize; i++)
            {
                pAdiLineTemp[i] = pAdiLineTemp[0];
            }
        }

        // Fill left & below-left samples
        pRoiTemp += PicStride;

        pAdiLineTemp--;
        pNeighborFlags--;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize; i++)
                {
                    pAdiLineTemp[-(Ipp32s)i] = pRoiTemp[i * PicStride];
                }
            }
            pRoiTemp += UnitSize * PicStride;
            pAdiLineTemp -= UnitSize;
            pNeighborFlags--;
        }

        // Fill above & above-right samples
        pRoiTemp = pRoiOrigin - PicStride;
        pAdiLineTemp = pAdiLine + ((NumUnits2 + 1) * UnitSize);
        pNeighborFlags = NeighborFlags + NumUnits2 + 1;
        for (j = 0; j < NumUnits2; j++)
        {
            if (*pNeighborFlags)
            {
                for (i = 0; i < UnitSize; i++)
                {
                    pAdiLineTemp[i] = pRoiTemp[i];
                }
            }
            pRoiTemp += UnitSize;
            pAdiLineTemp += UnitSize;
            pNeighborFlags++;
        }
        // Pad reference samples when necessary
        Prev = -1;
        Curr = 0;
        Next = 1;
        pAdiLineTemp = pAdiLine;
        while (Curr < TotalUnits)
        {
            if (!NeighborFlags[Curr])
            {
                if (Curr == 0)
                {
                    while (Next < TotalUnits && !NeighborFlags[Next])
                    {
                        Next++;
                    }
                    pRef = pAdiLine[Next * UnitSize];
                    // Pad unavailable samples with new value
                    while (Curr < Next)
                    {
                        for (i = 0; i < UnitSize; i++)
                        {
                            pAdiLineTemp[i] = pRef;
                        }
                        pAdiLineTemp += UnitSize;
                        Curr++;
                    }
                }
                else
                {
                    pRef = pAdiLine[Curr * UnitSize - 1];
                    for (i = 0; i < UnitSize; i++)
                    {
                        pAdiLineTemp[i] = pRef;
                    }
                    pAdiLineTemp += UnitSize;
                    Curr++;
                }
            }
            else
            {
                pAdiLineTemp += UnitSize;
                Curr++;
            }
        }

        // Copy processed samples
        pAdiLineTemp = pAdiLine + Size + UnitSize - 2;
        for (i = 0; i < Size; i++)
        {
            pAdiTemp[i] = pAdiLineTemp[i];
        }
        pAdiLineTemp = pAdiLine + Size - 1;
        for (i = 1; i < Size; i++)
        {
            pAdiTemp[i * Size] = pAdiLineTemp[-(Ipp32s)i];
        }
    }
}

/** Get pointer to reference samples for intra prediction
 * \param uiDirMode   prediction mode index
 * \param log2BlkSize size of block (2 = 4x4, 3 = 8x8, 4 = 16x16, 5 = 32x32, 6 = 64x64)
 * \param piAdiBuf    pointer to unfiltered reference samples
 * \return            pointer to (possibly filtered) reference samples
 *
 * The prediction mode index is used to determine whether a smoothed reference sample buffer is returned.
 */
H265PlanePtrYCommon H265Pattern::GetPredictorPtr(Ipp32u DirMode, Ipp32u log2BlkSize, H265PlanePtrYCommon pAdiBuf)
{
    H265PlanePtrYCommon pSrc;
    VM_ASSERT(log2BlkSize >= 2 && log2BlkSize < 7);
    Ipp32s diff = IPP_MIN((abs((Ipp32s) DirMode - HOR_IDX)), (abs((Ipp32s)DirMode - VER_IDX)));
    Ipp8u FiltIdx = diff > (Ipp32s) m_IntraFilter[log2BlkSize - 2] ? 1 : 0;
    if (DirMode == DC_IDX)
    {
        FiltIdx = 0; //no smoothing for DC or LM chroma
    }
    VM_ASSERT(FiltIdx <= 1);

    pSrc = pAdiBuf;

    if (FiltIdx)
    {
        Ipp32s size = (2 << log2BlkSize) + 1;
        pSrc += size * size;
    }

    return pSrc;
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
