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

#include "h265_frame_coding_data.h"

namespace UMC_HEVC_DECODER
{

PartitionInfo::PartitionInfo()
    : m_MaxCUDepth(0)
    , m_MaxCUSize(0)
{
}

void PartitionInfo::InitZscanToRaster(Ipp32s MaxDepth, Ipp32s Depth, Ipp32u StartVal, Ipp32u*& CurrIdx)
{
    Ipp32s Stride = 1 << (MaxDepth - 1);

    if (Depth == MaxDepth)
    {
        CurrIdx[0] = StartVal;
        CurrIdx++;
    }
    else
    {
        Ipp32s Step = Stride >> Depth;
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step * Stride, CurrIdx);
        InitZscanToRaster(MaxDepth, Depth + 1, StartVal + Step * Stride + Step, CurrIdx);
    }
}

void PartitionInfo::InitRasterToZscan(const H265SeqParamSet* sps)
{
    Ipp32u  NumPartInSize  = sps->MaxCUSize  / sps->MinCUSize;

    for (Ipp32u i = 0; i < NumPartInSize * NumPartInSize; i++)
    {
        m_rasterToZscan[m_zscanToRaster[i]] = i;
    }
}

void PartitionInfo::InitRasterToPelXY(const H265SeqParamSet* sps)
{
    Ipp32u* TempX = &m_rasterToPelX[0];
    Ipp32u* TempY = &m_rasterToPelY[0];

    Ipp32u NumPartInSize = sps->MaxCUSize / sps->MinCUSize;
    
    for (Ipp32u i = 0; i < NumPartInSize*NumPartInSize; i++)
    {
        TempX[i] = (m_zscanToRaster[i] % NumPartInSize) * sps->MinCUSize;
    }

    for (Ipp32u i = 1; i < NumPartInSize * NumPartInSize; i++)
    {
        TempY[i] = (m_zscanToRaster[i] / NumPartInSize) * sps->MinCUSize;
    }
};

void PartitionInfo::Init(const H265SeqParamSet* sps)
{
    if (m_MaxCUDepth == sps->MaxCUDepth && m_MaxCUSize == sps->MaxCUSize)
        return;

    m_MaxCUDepth = sps->MaxCUDepth;
    m_MaxCUSize = sps->MaxCUSize;

    memset(m_rasterToPelX, 0, sizeof(m_rasterToPelX));
    memset(m_rasterToPelY, 0, sizeof(m_rasterToPelY));
    memset(m_zscanToRaster, 0, sizeof(m_zscanToRaster));
    memset(m_rasterToZscan, 0, sizeof(m_rasterToZscan));

    // initialize partition order.
    Ipp32u* Tmp = &m_zscanToRaster[0];
    InitZscanToRaster(sps->MaxCUDepth + 1, 1, 0, Tmp);
    InitRasterToZscan(sps);

    // initialize conversion matrix from partition index to pel
    InitRasterToPelXY(sps);
}

void H265FrameCodingData::create(Ipp32s iPicWidth, Ipp32s iPicHeight, Ipp32u uiMaxWidth, Ipp32u uiMaxHeight, Ipp32u uiMaxDepth)
{
    m_MaxCUDepth = (Ipp8u) uiMaxDepth;
    m_NumPartitions  = 1 << (m_MaxCUDepth<<1);

    m_MaxCUWidth = uiMaxWidth;

    m_MinCUWidth = uiMaxWidth >> m_MaxCUDepth;

    m_NumPartInWidth = uiMaxWidth / m_MinCUWidth;

    m_WidthInCU = (iPicWidth % uiMaxWidth) ? iPicWidth / uiMaxWidth + 1 : iPicWidth / uiMaxWidth;
    m_HeightInCU = (iPicHeight % uiMaxHeight) ? iPicHeight / uiMaxHeight + 1 : iPicHeight / uiMaxHeight;

    m_NumCUsInFrame = m_WidthInCU * m_HeightInCU;
    m_CU = new H265CodingUnit*[m_NumCUsInFrame + 1];

    m_colocatedInfo = new H265MVInfo[m_NumCUsInFrame * m_NumPartitions];

    m_cuData.resize(m_NumCUsInFrame*m_NumPartitions);

    for (Ipp32s i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CU[i] = new H265CodingUnit;
        m_CU[i]->create (this, i);
    }

    m_CU[m_NumCUsInFrame] = 0;

    Ipp32s m_edgesInCTBSize = m_MaxCUWidth >> 3;
    m_edgesInFrameWidth = (m_edgesInCTBSize * m_WidthInCU + 2) * 4;
    Ipp32s edgesInFrameHeight = m_edgesInCTBSize * m_HeightInCU + 1;
    m_edge = new MFX_HEVC_PP::H265EdgeData[m_edgesInFrameWidth * edgesInFrameHeight];
}

void H265FrameCodingData::destroy()
{
    for (Ipp32s i = 0; i < m_NumCUsInFrame; i++)
    {
        m_CU[i]->destroy();
        delete m_CU[i];
        m_CU[i] = 0;
    }

    delete [] m_CU;
    m_CU = 0;

    delete[] m_colocatedInfo;
    m_colocatedInfo = 0;

    delete[] m_edge;
    m_edge = 0;

    m_SAO.destroy();
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
