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

#include "h265_global_rom.h"
#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_dec_defs_yuv.h"
#include "umc_h265_frame.h"
#include "umc_h265_frame_info.h"
#include "umc_h265_slice_decoding.h"
#include "assert.h"
#include "math.h"

namespace UMC_HEVC_DECODER
{

enum
{
    SAO_PRED_SIZE = 66
};

/** get the sign of input variable
 * \param   x
 */
inline Ipp32s getSign(Ipp32s x)
{
  return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

static const Ipp8u EoTable[9] =
{
    1, //0
    2, //1
    0, //2
    3, //3
    4, //4
    0, //5
    0, //6
    0, //7
    0
};

H265SampleAdaptiveOffset::H265SampleAdaptiveOffset()
{
    m_ClipTable = 0;
    m_ClipTableBase = 0;
    m_OffsetBo = 0;
    m_OffsetBo2 = 0;
    m_OffsetBoChroma = 0;
    m_OffsetBo2Chroma = 0;
    m_lumaTableBo = 0;

    m_TmpU[0] = 0;
    m_TmpU[1] = 0;

    m_isInitialized = false;
}

void H265SampleAdaptiveOffset::destroy()
{
    delete [] m_ClipTableBase; m_ClipTableBase = 0;
    delete [] m_OffsetBo; m_OffsetBo = 0;
    delete [] m_OffsetBo2; m_OffsetBo2 = 0;
    delete [] m_OffsetBoChroma; m_OffsetBoChroma = 0;
    delete [] m_OffsetBo2Chroma; m_OffsetBo2Chroma = 0;
    delete[] m_lumaTableBo; m_lumaTableBo = 0;
    delete [] m_TmpU[0]; m_TmpU[0] = 0;
    delete [] m_TmpU[0]; m_TmpU[0] = 0;

    m_isInitialized = false;
}

void H265SampleAdaptiveOffset::init(H265SeqParamSet* pSPS)
{
    if (!pSPS->sample_adaptive_offset_enabled_flag)
        return;

    m_PicWidth  = pSPS->pic_width_in_luma_samples;
    m_PicHeight = pSPS->pic_height_in_luma_samples;

    m_MaxCUWidth  = pSPS->MaxCUWidth;
    m_MaxCUHeight = pSPS->MaxCUWidth;

    Ipp32u uiPixelRangeY = 1 << g_bitDepthY;
    Ipp32u uiBoRangeShiftY = g_bitDepthY - SAO_BO_BITS;

    m_lumaTableBo = new H265PlaneYCommon[uiPixelRangeY];
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++)
    {
        m_lumaTableBo[k2] = (H265PlaneYCommon)(1 + (k2>>uiBoRangeShiftY));
    }

    Ipp32u uiMaxY  = (1 << g_bitDepthY) - 1;
    Ipp32u uiMinY  = 0;

    Ipp32u iCRangeExt = uiMaxY>>1;

    m_ClipTableBase = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];
    m_OffsetBo      = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];
    m_OffsetBo2     = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];
    m_OffsetBoChroma   = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];
    m_OffsetBo2Chroma  = new H265PlaneYCommon[uiMaxY+2*iCRangeExt];

    for (Ipp32u i = 0; i < (uiMinY + iCRangeExt);i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)uiMinY;
    }

    for (Ipp32u i = uiMinY + iCRangeExt; i < (uiMaxY + iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)(i - iCRangeExt);
    }

    for (Ipp32u i = uiMaxY + iCRangeExt; i < (uiMaxY + 2 * iCRangeExt); i++)
    {
        m_ClipTableBase[i] = (H265PlaneYCommon)uiMaxY;
    }

    m_ClipTable = &(m_ClipTableBase[iCRangeExt]);

    m_TmpU[0] = new H265PlaneYCommon [2*m_PicWidth];
    m_TmpU[1] = new H265PlaneYCommon [2*m_PicWidth];

    m_isInitialized = true;
}

void H265PicParamSetBase::getColumnWidth(Ipp16u *columnWidth) const
{
    for (unsigned i = 0; i < num_tile_columns - 1; i++)
        columnWidth[i] = (Ipp16u)m_ColumnWidthArray[i];
}

void H265PicParamSetBase::getColumnWidthMinus1(Ipp16u *columnWidth) const
{
    for (unsigned i = 0; i < num_tile_columns - 1; i++)
        columnWidth[i] = (Ipp16u)(m_ColumnWidthArray[i] - 1);
}

void H265PicParamSetBase::setColumnWidth(Ipp32u* columnWidth)
{
    if (uniform_spacing_flag == 0)
    {
        m_ColumnWidthArray = new Ipp32u[num_tile_columns];
        if (NULL == m_ColumnWidthArray)
            return;

        for (Ipp32u i = 0; i < num_tile_columns - 1; i++)
        {
            m_ColumnWidthArray[i] = columnWidth[i];
        }
    }
}

void H265PicParamSetBase::getRowHeight(Ipp16u* rowHeight) const
{
    for (unsigned i = 0; i < num_tile_rows - 1; i++)
        rowHeight[i] = (Ipp16u)m_RowHeightArray[i];
}

void H265PicParamSetBase::getRowHeightMinus1(Ipp16u* rowHeight) const
{
    for (unsigned i = 0; i < num_tile_rows - 1; i++)
        rowHeight[i] = (Ipp16u)(m_RowHeightArray[i] - 1);
}

void H265PicParamSetBase::setRowHeight(Ipp32u* rowHeight)
{
    if (uniform_spacing_flag == 0)
    {
        m_RowHeightArray = new Ipp32u[num_tile_rows];

        for (Ipp32u i = 0; i < num_tile_rows - 1; i++)
        {
            m_RowHeightArray[i] = rowHeight[i];
        }
    }
}

ReferencePictureSet::ReferencePictureSet()
{
  ::memset(this, 0, sizeof(*this));
}

void ReferencePictureSet::sortDeltaPOC()
{
    Ipp32s temp;
    Ipp32s deltaPOC;
    bool Used;
    // sort in increasing order (smallest first)
    for (Ipp32s j = 1; j < m_NumberOfPictures; j++)
    {
        deltaPOC = m_DeltaPOC[j];
        Used = m_Used[j];
        for (Ipp32s k = j - 1; k >= 0; k--)
        {
            temp = m_DeltaPOC[k]; //modify sort
            if (deltaPOC < temp)
            {
                m_DeltaPOC[k + 1] = temp;
                m_Used[k + 1] = m_Used[k];
                m_DeltaPOC[k] = deltaPOC;
                m_Used[k] = Used;
            }
        }
    }
    // flip the negative values to largest first
    Ipp32s NumNegPics = (Ipp32s) m_NumberOfNegativePictures;
    for (Ipp32s j = 0, k = NumNegPics - 1; j < NumNegPics >> 1; j++, k--)
    {
        deltaPOC = m_DeltaPOC[j];
        Used = m_Used[j];
        m_DeltaPOC[j] = m_DeltaPOC[k];
        m_Used[j] = m_Used[k];
        m_DeltaPOC[k] = deltaPOC;
        m_Used[k] = Used;
    }
}

ReferencePictureSetList::ReferencePictureSetList()
    : m_NumberOfReferencePictureSets(0)
{
}

void ReferencePictureSetList::allocate(unsigned NumberOfReferencePictureSets)
{
    if (m_NumberOfReferencePictureSets == NumberOfReferencePictureSets)
        return;

    m_NumberOfReferencePictureSets = NumberOfReferencePictureSets;
    referencePictureSet.resize(NumberOfReferencePictureSets);
}

void H265SampleAdaptiveOffset::processSaoCuOrgLuma(Ipp32s addr, Ipp32s saoType, H265PlanePtrYCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    H265PlanePtrYCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrYCommon tmpU;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight;
    LPelX        = LPelX;
    TPelY        = TPelY;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetLumaAddr(addr);
    stride    = m_Frame->pitch_luma();

    tmpU = &(m_TmpU[0][LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth-1 : LCUWidth;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x] - pRec[x+1]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
                }

                pRec += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - tmpU[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown    = getSign(pRec[x] - pRec[x+stride]);
                    edgeType    = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x]= -signDown;

                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
                }
                pRec += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            startX = (LPelX == 0)           ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = startX; x < endX; x++)
            {
                pUpBuff[x] = getSign(pRec[x] - tmpU[x-1]);
            }

            for (y = startY; y < endY; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride+startX] - tmpL[y]);

                for (x = startX; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x] - pRec[x+stride+1]) ;
                    edgeType       =  signDown1 + pUpBuff[x] + 2;
                    pUpBufft[x+1]  = -signDown1;
                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            startX = (LPelX == 0) ? 1 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 1 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1)
            {
                pRec += stride;
            }

            for (x = startX - 1; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x] - tmpU[x+1]);
            }

            for (y = startY; y < endY; y++)
            {
                x = startX;
                signDown1      =  getSign(pRec[x] - tmpL[y+1]) ;
                edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                tmpUpBuff1[x] = -signDown1;
                pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];

                for (x = startX + 1; x < endX; x++)
                {
                    signDown1      =  getSign(pRec[x] - pRec[x+stride-1]) ;
                    edgeType       =  signDown1 + tmpUpBuff1[x+1] + 2;
                    tmpUpBuff1[x]  = -signDown1;
                    pRec[x] = m_ClipTable[pRec[x] + m_OffsetEo[edgeType]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX-1 + stride] - pRec[endX]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x] = m_OffsetBo[pRec[x]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuLuma(Ipp32s addr, Ipp32s saoType, H265PlanePtrYCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    bool* pbBorderAvail = pTmpCu->m_AvailBorder;
    H265PlanePtrYCommon pRec;
    Ipp32s tmpUpBuff1[65];
    Ipp32s tmpUpBuff2[65];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeft;
    Ipp32s signRight;
    Ipp32s signDown;
    Ipp32s signDown1;
    Ipp32u edgeType;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrYCommon startPtr;
    H265PlanePtrYCommon tmpU;
    H265PlanePtrYCommon pClipTbl = m_ClipTable;
    H265PlanePtrYCommon pOffsetBo = m_OffsetBo;
    Ipp32s startStride;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight;
    LPelX        = LPelX;
    TPelY        = TPelY;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetLumaAddr(addr);
    stride    = m_Frame->pitch_luma();

    tmpU = &(m_TmpU[0][LPelX]);

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth - 1);

            for (y = 0; y < LCUHeight; y++)
            {
                signLeft = getSign(pRec[startX] - startPtr[y*startStride]);

                for (x = startX; x < endX; x++)
                {
                    signRight =  getSign(pRec[x] - pRec[x+1]);
                    edgeType  =  signRight + signLeft + 2;
                    signLeft  = -signRight;

                    pRec[x]   = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            if (pbBorderAvail[SGU_T])
            {
                startY = 0;
                startPtr = tmpU;
            }
            else
            {
                startY = 1;
                startPtr = pRec;
                pRec += stride;
            }

            endY = (pbBorderAvail[SGU_B]) ? LCUHeight : LCUHeight - 1;

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - startPtr[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    signDown      = getSign(pRec[x] - pRec[x+stride]);
                    edgeType      = signDown + tmpUpBuff1[x] + 2;
                    tmpUpBuff1[x] = -signDown;

                    pRec[x]      = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth-1);

            //prepare 2nd line upper sign
            pUpBuff[startX] = getSign(pRec[startX+stride] - startPtr[0]);
            for (x = startX; x < endX; x++)
            {
                pUpBuff[x+1] = getSign(pRec[x+stride+1] - pRec[x]);
            }

            //1st line
            if (pbBorderAvail[SGU_TL])
            {
                edgeType = getSign(pRec[0] - tmpU[-1]) - pUpBuff[1] + 2;
                pRec[0]  = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];
            }

            if (pbBorderAvail[SGU_T])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x] - tmpU[x-1]) - pUpBuff[x+1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride+startX] - startPtr[y*startStride]);

                for (x = startX; x < endX; x++)
                {
                    signDown1 = getSign(pRec[x] - pRec[x+stride+1]);
                    edgeType  =  signDown1 + pUpBuff[x] + 2;
                    pRec[x]   = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];

                    pUpBufft[x+1] = -signDown1;
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }

            //last line
            if (pbBorderAvail[SGU_B])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_BR])
            {
                x = LCUWidth - 1;
                edgeType = getSign(pRec[x] - pRec[x+stride+1]) + pUpBuff[x] + 2;
                pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 1;
            }
            else
            {
                startX = 1;
                startPtr = pRec;
                startStride = stride;
            }

            endX   = (pbBorderAvail[SGU_R]) ? LCUWidth : (LCUWidth -1);

            //prepare 2nd line upper sign
            tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
            for (x = startX; x < endX; x++)
            {
                tmpUpBuff1[x+1] = getSign(pRec[x+stride] - pRec[x+1]);
            }

            //first line
            if (pbBorderAvail[SGU_T])
            {
                for (x = startX; x < LCUWidth - 1; x++)
                {
                    edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                    pRec[x] = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }

            if (pbBorderAvail[SGU_TR])
            {
                x= LCUWidth - 1;
                edgeType = getSign(pRec[x] - tmpU[x+1]) - tmpUpBuff1[x] + 2;
                pRec[x] = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                signDown1 = getSign(pRec[startX] - startPtr[(y+1)*startStride]) ;

                for (x = startX; x < endX; x++)
                {
                    edgeType       = signDown1 + tmpUpBuff1[x+1] + 2;
                    pRec[x]        = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                    tmpUpBuff1[x] = -signDown1;
                    signDown1      = getSign(pRec[x+1] - pRec[x+stride]) ;
                }

                tmpUpBuff1[endX] = -signDown1;

                pRec  += stride;
            }

            //last line
            if (pbBorderAvail[SGU_BL])
            {
                edgeType = getSign(pRec[0] - pRec[stride-1]) + tmpUpBuff1[1] + 2;
                pRec[0] = pClipTbl[pRec[0] + m_OffsetEo[edgeType]];

            }

            if (pbBorderAvail[SGU_B])
            {
                for (x = 1; x < endX; x++)
                {
                    edgeType = getSign(pRec[x] - pRec[x+stride-1]) + tmpUpBuff1[x+1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + m_OffsetEo[edgeType]];
                }
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x++)
                {
                    pRec[x] = pOffsetBo[pRec[x]];
                }

                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuOrgChroma(Ipp32s addr, Ipp32s saoType, H265PlanePtrUVCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    H265PlanePtrUVCommon pRec;
    Ipp32s tmpUpBuff1[66];
    Ipp32s tmpUpBuff2[66];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeftCb, signLeftCr;
    Ipp32s signRightCb, signRightCr;
    Ipp32s signDownCb, signDownCr;
    Ipp32u edgeTypeCb, edgeTypeCr;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrUVCommon tmpU;
    Ipp32s *pOffsetEoCb = m_OffsetEoChroma;
    Ipp32s *pOffsetEoCr = m_OffsetEo2Chroma;
    H265PlanePtrUVCommon pOffsetBoCb = m_OffsetBoChroma;
    H265PlanePtrUVCommon pOffsetBoCr = m_OffsetBo2Chroma;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight >> 1;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight   >> 1;
    LPelX        = LPelX;
    TPelY        = TPelY       >> 1;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetCbCrAddr(addr);
    stride    = m_Frame->pitch_chroma();

    tmpU = m_TmpU[0] + LPelX + picWidthTmp;

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeftCb = getSign(pRec[startX] - tmpL[y * 2]);
                signLeftCr = getSign(pRec[startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signRightCb = getSign(pRec[x] - pRec[x + 2]);
                    signRightCr = getSign(pRec[x + 1] - pRec[x + 3]);
                    edgeTypeCb = signRightCb + signLeftCb + 2;
                    edgeTypeCr = signRightCr + signLeftCr + 2;
                    signLeftCb = -signRightCb;
                    signLeftCr = -signRightCr;

                    pRec[x] = m_ClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                pRec += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - tmpU[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride]);
                    edgeTypeCb = signDownCb + tmpUpBuff1[x] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 1] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;

                    pRec[x] = m_ClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            startX = (LPelX == 0)           ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ?            1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (TPelY == 0)
            {
                pRec += stride;
            }

            for (x = startX; x < endX; x++)
            {
                pUpBuff[x] = getSign(pRec[x] - tmpU[x - 2]);
            }

            for (y = startY; y < endY; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX] - tmpL[y * 2]);
                pUpBufft[startX + 1] = getSign(pRec[stride + startX + 1] - tmpL[y * 2 + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride + 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride + 3]);
                    edgeTypeCb = signDownCb + pUpBuff[x] + 2;
                    edgeTypeCr = signDownCr + pUpBuff[x + 1] + 2;
                    pUpBufft[x + 2] = -signDownCb;
                    pUpBufft[x + 3] = -signDownCr;
                    pRec[x] = m_ClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                swapPtr  = pUpBuff;
                pUpBuff  = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            startX = (LPelX == 0) ? 2 : 0;
            endX   = (RPelX == picWidthTmp) ? LCUWidth - 2 : LCUWidth;

            startY = (TPelY == 0) ? 1 : 0;
            endY   = (BPelY == picHeightTmp) ? LCUHeight - 1 : LCUHeight;

            if (startY == 1)
            {
                pRec += stride;
            }

            for (x = startX - 2; x < endX; x++)
            {
                tmpUpBuff1[x + 2] = getSign(pRec[x] - tmpU[x + 2]);
            }

            for (y = startY; y < endY; y++)
            {
                x = startX;
                signDownCb = getSign(pRec[x] - tmpL[y * 2 + 2]);
                signDownCr = getSign(pRec[x + 1] - tmpL[y * 2 + 3]);
                edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                tmpUpBuff1[x] = -signDownCb;
                tmpUpBuff1[x + 1] = -signDownCr;
                pRec[x] = m_ClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1] = m_ClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                for (x = startX + 2; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride - 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + stride - 1]);
                    edgeTypeCb =  signDownCb + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr =  signDownCr + tmpUpBuff1[x + 3] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;
                    pRec[x] = m_ClipTable[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = m_ClipTable[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }

                tmpUpBuff1[endX] = getSign(pRec[endX - 2 + stride] - pRec[endX]);
                tmpUpBuff1[endX + 1] = getSign(pRec[endX - 1 + stride] - pRec[endX + 1]);

                pRec += stride;
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    pRec[x] = pOffsetBoCb[pRec[x]];
                    pRec[x + 1] = pOffsetBoCr[pRec[x + 1]];
                }
                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::processSaoCuChroma(Ipp32s addr, Ipp32s saoType, H265PlanePtrUVCommon tmpL)
{
    H265CodingUnit *pTmpCu = m_Frame->getCU(addr);
    bool* pbBorderAvail = pTmpCu->m_AvailBorder;
    H265PlanePtrUVCommon pRec;
    Ipp32s tmpUpBuff1[66];
    Ipp32s tmpUpBuff2[66];
    Ipp32s stride;
    Ipp32s LCUWidth  = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s LPelX     = (Ipp32s)pTmpCu->m_CUPelX;
    Ipp32s TPelY     = (Ipp32s)pTmpCu->m_CUPelY;
    Ipp32s RPelX;
    Ipp32s BPelY;
    Ipp32s signLeftCb, signLeftCr;
    Ipp32s signRightCb, signRightCr;
    Ipp32s signDownCb, signDownCr;
    Ipp32u edgeTypeCb, edgeTypeCr;
    Ipp32s picWidthTmp;
    Ipp32s picHeightTmp;
    Ipp32s startX;
    Ipp32s startY;
    Ipp32s endX;
    Ipp32s endY;
    Ipp32s x, y;
    H265PlanePtrUVCommon startPtr;
    H265PlanePtrUVCommon tmpU;
    H265PlanePtrUVCommon pClipTbl = m_ClipTable;
    Ipp32s *pOffsetEoCb = m_OffsetEoChroma;
    Ipp32s *pOffsetEoCr = m_OffsetEo2Chroma;
    H265PlanePtrUVCommon pOffsetBoCb = m_OffsetBoChroma;
    H265PlanePtrUVCommon pOffsetBoCr = m_OffsetBo2Chroma;
    Ipp32s startStride;

    picWidthTmp  = m_PicWidth;
    picHeightTmp = m_PicHeight >> 1;
    LCUWidth     = LCUWidth;
    LCUHeight    = LCUHeight   >> 1;
    LPelX        = LPelX;
    TPelY        = TPelY       >> 1;
    RPelX        = LPelX + LCUWidth;
    BPelY        = TPelY + LCUHeight;
    RPelX        = RPelX > picWidthTmp  ? picWidthTmp  : RPelX;
    BPelY        = BPelY > picHeightTmp ? picHeightTmp : BPelY;
    LCUWidth     = RPelX - LPelX;
    LCUHeight    = BPelY - TPelY;

    pRec      = m_Frame->GetCbCrAddr(addr);
    stride    = m_Frame->pitch_chroma();

    tmpU = m_TmpU[0] + LPelX + picWidthTmp;

    switch (saoType)
    {
    case SAO_EO_0: // dir: -
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail[SGU_R] ? LCUWidth : LCUWidth - 2;

            for (y = 0; y < LCUHeight; y++)
            {
                signLeftCb = getSign(pRec[startX] - startPtr[y * startStride]);
                signLeftCr = getSign(pRec[startX + 1] - startPtr[y * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signRightCb = getSign(pRec[x] - pRec[x + 2]);
                    signRightCr = getSign(pRec[x + 1] - pRec[x + 3]);
                    edgeTypeCb = signRightCb + signLeftCb + 2;
                    edgeTypeCr = signRightCr + signLeftCr + 2;
                    signLeftCb = -signRightCb;
                    signLeftCr = -signRightCr;

                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec  += stride;
            }
            break;
        }
    case SAO_EO_1: // dir: |
        {
            if (pbBorderAvail[SGU_T])
            {
                startY = 0;
                startPtr = tmpU;
            }
            else
            {
                startY = 1;
                startPtr = pRec;
                pRec += stride;
            }

            endY = pbBorderAvail[SGU_B] ? LCUHeight : LCUHeight - 1;

            for (x = 0; x < LCUWidth; x++)
            {
                tmpUpBuff1[x] = getSign(pRec[x] - startPtr[x]);
            }

            for (y = startY; y < endY; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride]);
                    edgeTypeCb = signDownCb + tmpUpBuff1[x] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 1] + 2;
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;

                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
                pRec += stride;
            }
            break;
        }
    case SAO_EO_2: // dir: 135
        {
            Ipp32s *pUpBuff = tmpUpBuff1;
            Ipp32s *pUpBufft = tmpUpBuff2;
            Ipp32s *swapPtr;

            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail[SGU_R] ? LCUWidth : LCUWidth - 2;

            //prepare 2nd line upper sign
            pUpBuff[startX] = getSign(pRec[startX + stride] - startPtr[0]);
            pUpBuff[startX + 1] = getSign(pRec[startX + 1 + stride] - startPtr[1]);
            for (x = startX; x < endX; x++)
            {
                pUpBuff[x + 2] = getSign(pRec[x + stride + 2] - pRec[x]);
            }

            //1st line
            if (pbBorderAvail[SGU_TL])
            {
                edgeTypeCb = getSign(pRec[0] - tmpU[-2]) - pUpBuff[2] + 2;
                edgeTypeCr = getSign(pRec[1] - tmpU[-1]) - pUpBuff[3] + 2;
                pRec[0] = pClipTbl[pRec[0] + pOffsetEoCb[edgeTypeCb]];
                pRec[1] = pClipTbl[pRec[1] + pOffsetEoCr[edgeTypeCr]];
            }

            if (pbBorderAvail[SGU_T])
            {
                for (x = 2; x < endX; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - tmpU[x - 2]) - pUpBuff[x + 2] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - tmpU[x - 1]) - pUpBuff[x + 3] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                pUpBufft[startX] = getSign(pRec[stride + startX] - startPtr[y * startStride]);
                pUpBufft[startX + 1] = getSign(pRec[stride + startX + 1] - startPtr[y * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    signDownCb = getSign(pRec[x] - pRec[x + stride + 2]);
                    signDownCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]);
                    edgeTypeCb = signDownCb + pUpBuff[x] + 2;
                    edgeTypeCr = signDownCr + pUpBuff[x + 1] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];

                    pUpBufft[x + 2] = -signDownCb;
                    pUpBufft[x + 3] = -signDownCr;
                }

                swapPtr = pUpBuff;
                pUpBuff = pUpBufft;
                pUpBufft = swapPtr;

                pRec += stride;
            }

            //last line
            if (pbBorderAvail[SGU_B])
            {
                for (x = startX; x < LCUWidth - 2; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - pRec[x + stride + 2]) + pUpBuff[x] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]) + pUpBuff[x + 1] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            if (pbBorderAvail[SGU_BR])
            {
                x = LCUWidth - 2;
                edgeTypeCb = getSign(pRec[x] - pRec[x + stride + 2]) + pUpBuff[x] + 2;
                edgeTypeCr = getSign(pRec[x + 1] - pRec[x + 1 + stride + 2]) + pUpBuff[x + 1] + 2;
                pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
            }
            break;
        }
    case SAO_EO_3: // dir: 45
        {
            if (pbBorderAvail[SGU_L])
            {
                startX = 0;
                startPtr = tmpL;
                startStride = 2;
            }
            else
            {
                startX = 2;
                startPtr = pRec;
                startStride = stride;
            }

            endX = pbBorderAvail[SGU_R] ? LCUWidth : LCUWidth - 2;

            //prepare 2nd line upper sign
            tmpUpBuff1[startX] = getSign(startPtr[startStride] - pRec[startX]);
            tmpUpBuff1[startX + 1] = getSign(startPtr[startStride + 1] - pRec[startX + 1]);
            for (x = startX; x < endX; x++)
            {
                tmpUpBuff1[x + 2] = getSign(pRec[x + stride] - pRec[x + 2]);
            }

            //first line
            if (pbBorderAvail[SGU_T])
            {
                for (x = startX; x < LCUWidth - 2; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - tmpU[x + 2]) - tmpUpBuff1[x] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - tmpU[x + 3]) - tmpUpBuff1[x + 1] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }

            if (pbBorderAvail[SGU_TR])
            {
                x = LCUWidth - 2;
                edgeTypeCb = getSign(pRec[x] - tmpU[x + 2]) - tmpUpBuff1[x] + 2;
                edgeTypeCr = getSign(pRec[x + 1] - tmpU[x + 3]) - tmpUpBuff1[x + 1] + 2;
                pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
            }

            pRec += stride;

            //middle lines
            for (y = 1; y < LCUHeight - 1; y++)
            {
                signDownCb = getSign(pRec[startX] - startPtr[(y + 1) * startStride]);
                signDownCr = getSign(pRec[startX + 1] - startPtr[(y + 1) * startStride + 1]);

                for (x = startX; x < endX; x += 2)
                {
                    edgeTypeCb = signDownCb + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr = signDownCr + tmpUpBuff1[x + 3] + 2;
                    pRec[x] = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1] = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                    tmpUpBuff1[x] = -signDownCb;
                    tmpUpBuff1[x + 1] = -signDownCr;
                    signDownCb = getSign(pRec[x + 2] - pRec[x + stride]);
                    signDownCr = getSign(pRec[x + 3] - pRec[x + 1 + stride]);
                }

                tmpUpBuff1[endX] = -signDownCb;
                tmpUpBuff1[endX + 1] = -signDownCr;

                pRec  += stride;
            }

            //last line
            if (pbBorderAvail[SGU_BL])
            {
                edgeTypeCb = getSign(pRec[0] - pRec[stride - 2]) + tmpUpBuff1[2] + 2;
                edgeTypeCr = getSign(pRec[1] - pRec[stride - 1]) + tmpUpBuff1[3] + 2;
                pRec[0] = pClipTbl[pRec[0] + pOffsetEoCb[edgeTypeCb]];
                pRec[1] = pClipTbl[pRec[1] + pOffsetEoCr[edgeTypeCr]];
            }

            if (pbBorderAvail[SGU_B])
            {
                for (x = 2; x < endX; x += 2)
                {
                    edgeTypeCb = getSign(pRec[x] - pRec[x + stride - 2]) + tmpUpBuff1[x + 2] + 2;
                    edgeTypeCr = getSign(pRec[x + 1] - pRec[x + stride - 1]) + tmpUpBuff1[x + 3] + 2;
                    pRec[x]  = pClipTbl[pRec[x] + pOffsetEoCb[edgeTypeCb]];
                    pRec[x + 1]  = pClipTbl[pRec[x + 1] + pOffsetEoCr[edgeTypeCr]];
                }
            }
            break;
        }
    case SAO_BO:
        {
            for (y = 0; y < LCUHeight; y++)
            {
                for (x = 0; x < LCUWidth; x += 2)
                {
                    pRec[x] = pOffsetBoCb[pRec[x]];
                    pRec[x + 1] = pOffsetBoCr[pRec[x + 1]];
                }

                pRec += stride;
            }
            break;
        }
    default: break;
    }
}

void H265SampleAdaptiveOffset::SAOProcess(H265DecoderFrame* pFrame, Ipp32s start, Ipp32s toProcess)
{
    m_Frame = pFrame;
    H265Slice * slice = pFrame->GetAU()->GetAnySlice();
    if (slice->GetSliceHeader()->slice_sao_luma_flag || slice->GetSliceHeader()->slice_sao_chroma_flag)
    {
        m_SaoBitIncreaseY = IPP_MAX(g_bitDepthY - 10, 0);
        m_SaoBitIncreaseC = IPP_MAX(g_bitDepthC - 10, 0);
    
        createNonDBFilterInfo();
        processSaoUnitAll();
    }
}

void H265SampleAdaptiveOffset::processSaoLine(SAOLCUParam* saoLCUParam, SAOLCUParam* saoLCUParamCb, SAOLCUParam* saoLCUParamCr, Ipp32s addr)
{
    H265PlaneYCommon tmpLeftBuff1[SAO_PRED_SIZE*2];
    H265PlaneYCommon tmpLeftBuff2[SAO_PRED_SIZE*2];
    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s LCUWidth = m_MaxCUWidth;
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s picHeightTmp = m_PicHeight;

    H265PlanePtrYCommon tmpL1 = tmpLeftBuff1;
    H265PlanePtrYCommon tmpL2 = tmpLeftBuff2;

    Ipp32s CUHeight = LCUHeight + 1;
    Ipp32s CUHeightChroma = (LCUHeight>>1) + 1;
    Ipp32s idxY = addr / frameWidthInCU;

    if ((idxY * LCUHeight + CUHeight) > picHeightTmp)
    {
        CUHeight = picHeightTmp - idxY * LCUHeight;
        CUHeightChroma = CUHeight >> 1;
    }
    
    H265PlanePtrYCommon rec = m_Frame->GetLumaAddr(addr);
    H265PlanePtrUVCommon recChroma = m_Frame->GetCbCrAddr(addr);
    Ipp32s stride = m_Frame->pitch_luma();

    for (Ipp32s i = 0; i < CUHeight; i++)
    {
        tmpL1[i] = rec[0];
        rec += stride;
    }

    for (Ipp32s i = 0; i < CUHeightChroma; i++)
    {
        tmpL1[SAO_PRED_SIZE + i*2] = recChroma[0];
        tmpL1[SAO_PRED_SIZE + i*2 + 1] = recChroma[1];
        recChroma += stride;
    }

    for (Ipp32s idxX = 0; idxX < frameWidthInCU; idxX++)
    {
        Ipp32s addr = idxY * frameWidthInCU + idxX;

        if (idxX != (frameWidthInCU - 1))
        {
            rec  = m_Frame->GetLumaAddr(addr);
            recChroma = m_Frame->GetCbCrAddr(addr);
            for (Ipp32s i = 0; i < CUHeight; i++)
            {
                tmpL2[i] = rec[i*stride+LCUWidth-1];
            }

            for (Ipp32s i = 0; i < CUHeightChroma; i++)
            {
                tmpL2[SAO_PRED_SIZE + i*2] = recChroma[i * stride + LCUWidth - 2];
                tmpL2[SAO_PRED_SIZE + i*2 + 1] = recChroma[i * stride + LCUWidth - 1];
            }
        }

        if (m_slice_sao_luma_flag && saoLCUParam[addr].m_typeIdx >= 0)
        {
            Ipp32s typeIdx = saoLCUParam[addr].m_typeIdx;
            if (!saoLCUParam[addr].m_mergeLeftFlag)
            {
                SetOffsetsLuma(saoLCUParam[addr], typeIdx);
            }

            if (!m_UseNIF)
                processSaoCuOrgLuma(addr, typeIdx, tmpL1);
            else
                processSaoCuLuma(addr, typeIdx, tmpL1);
        }

        if (m_slice_sao_chroma_flag && saoLCUParamCb[addr].m_typeIdx >= 0)
        {
            Ipp32s typeIdx = saoLCUParamCb[addr].m_typeIdx;
            VM_ASSERT(saoLCUParamCb[addr].m_typeIdx == saoLCUParamCr[addr].m_typeIdx);
            VM_ASSERT(saoLCUParamCb[addr].m_mergeLeftFlag == saoLCUParamCr[addr].m_mergeLeftFlag);

            if (!saoLCUParamCb[addr].m_mergeLeftFlag)
            {
                SetOffsetsChroma(saoLCUParamCb[addr], saoLCUParamCr[addr], typeIdx);
            }

            if (!m_UseNIF)
                processSaoCuOrgChroma(addr, typeIdx, tmpL1 + SAO_PRED_SIZE);
            else
                processSaoCuChroma(addr, typeIdx, tmpL1 + SAO_PRED_SIZE);
        }

        H265PlanePtrYCommon tmpUSwap = tmpL1;
        tmpL1  = tmpL2;
        tmpL2  = tmpUSwap;
    }
}

void H265SampleAdaptiveOffset::processSaoUnitAll()
{
    H265Slice * slice = m_Frame->GetAU()->GetAnySlice();
    SAOLCUParam* saoLCUParam = m_Frame->m_saoLcuParam[0];
    SAOLCUParam* saoLCUParamCb = m_Frame->m_saoLcuParam[1];
    SAOLCUParam* saoLCUParamCr = m_Frame->m_saoLcuParam[2];

    m_slice_sao_luma_flag = slice->GetSliceHeader()->slice_sao_luma_flag;
    m_slice_sao_chroma_flag = slice->GetSliceHeader()->slice_sao_chroma_flag;
    
    m_needPCMRestoration = (slice->GetSliceHeader()->m_SeqParamSet->getUsePCM() && slice->GetSliceHeader()->m_SeqParamSet->getPCMFilterDisableFlag()) ||
        slice->GetSliceHeader()->m_PicParamSet->getTransquantBypassEnableFlag();

    Ipp32s frameWidthInCU = m_Frame->getFrameWidthInCU();
    Ipp32s frameHeightInCU = m_Frame->getFrameHeightInCU();
    Ipp32s LCUHeight = m_MaxCUHeight;
    Ipp32s picWidthTmp = m_PicWidth;

    memcpy(m_TmpU[0], m_Frame->m_pYPlane, sizeof(H265PlaneYCommon) * picWidthTmp);
    memcpy(m_TmpU[0] + picWidthTmp, m_Frame->m_pUVPlane, sizeof(H265PlaneUVCommon) * picWidthTmp);

    for (Ipp32s idxY = 0; idxY < frameHeightInCU; idxY++)
    {
        Ipp32s addr = idxY * frameWidthInCU;

        if (idxY != (frameHeightInCU - 1))
        {
            H265PlanePtrYCommon pRec = m_Frame->GetLumaAddr(addr) + (LCUHeight - 1)*m_Frame->pitch_luma();
            memcpy(m_TmpU[1], pRec, sizeof(H265PlaneYCommon) * picWidthTmp);

            pRec = m_Frame->GetCbCrAddr(addr) + ((LCUHeight >> 1)- 1)*m_Frame->pitch_chroma();
            memcpy(m_TmpU[1] + picWidthTmp, pRec, sizeof(H265PlaneUVCommon) * picWidthTmp);
        }

        processSaoLine(saoLCUParam, saoLCUParamCb, saoLCUParamCr, addr);

        if (m_needPCMRestoration)
        {
            for(Ipp32s CUAddr = addr; CUAddr < addr + frameWidthInCU; CUAddr++)
            {
                H265CodingUnit* pcCU = m_Frame->getCU(CUAddr);
                PCMCURestoration(pcCU, 0, 0);
            }
        }
        
        H265PlanePtrYCommon tmpUSwap = m_TmpU[0];
        m_TmpU[0] = m_TmpU[1];
        m_TmpU[1] = tmpUSwap;
    }
}

void H265SampleAdaptiveOffset::SetOffsetsLuma(SAOLCUParam  &saoLCUParam, Ipp32s typeIdx)
{
    Ipp32s offset[LUMA_GROUP_NUM + 1];

    Ipp32u saoBitIncrease = m_SaoBitIncreaseY;

    if (typeIdx == SAO_BO)
    {
        for (Ipp32s i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
        {
            offset[i] = 0;
        }
        for (Ipp32s i = 0; i < saoLCUParam.m_length; i++)
        {
            offset[(saoLCUParam.m_subTypeIdx + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParam.m_offset[i] << saoBitIncrease;
        }

        H265PlaneYCommon *ppLumaTable = m_lumaTableBo;
        for (Ipp32s i = 0; i < (1 << g_bitDepthY); i++)
        {
            m_OffsetBo[i] = m_ClipTable[i + offset[ppLumaTable[i]]];
        }
    }
    else if (typeIdx == SAO_EO_0 || typeIdx == SAO_EO_1 || typeIdx == SAO_EO_2 || typeIdx == SAO_EO_3)
    {
        offset[0] = 0;
        for (Ipp32s i = 0; i < saoLCUParam.m_length; i++)
        {
            offset[i + 1] = saoLCUParam.m_offset[i] << saoBitIncrease;
        }
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++)
        {
            m_OffsetEo[edgeType] = offset[EoTable[edgeType]];
        }
    }
}

void H265SampleAdaptiveOffset::SetOffsetsChroma(SAOLCUParam &saoLCUParamCb, SAOLCUParam &saoLCUParamCr, Ipp32s typeIdx)
{
    Ipp32s offsetCb[LUMA_GROUP_NUM + 1];
    Ipp32s offsetCr[LUMA_GROUP_NUM + 1];

    Ipp32u saoBitIncrease = m_SaoBitIncreaseC;
    VM_ASSERT(saoLCUParamCb.m_length == saoLCUParamCr.m_length);

    if (typeIdx == SAO_BO)
    {
        for (Ipp32s i = 0; i < SAO_MAX_BO_CLASSES + 1; i++)
        {
            offsetCb[i] = 0;
            offsetCr[i] = 0;
        }
        for (Ipp32s i = 0; i < saoLCUParamCb.m_length; i++)
        {
            offsetCb[(saoLCUParamCb.m_subTypeIdx + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParamCb.m_offset[i] << saoBitIncrease;
            offsetCr[(saoLCUParamCr.m_subTypeIdx + i) % SAO_MAX_BO_CLASSES + 1] = saoLCUParamCr.m_offset[i] << saoBitIncrease;
        }

        H265PlaneYCommon *ppLumaTable = m_lumaTableBo;
        for (Ipp32s i = 0; i < (1 << g_bitDepthC); i++)
        {
            m_OffsetBoChroma[i] = m_ClipTable[i + offsetCb[ppLumaTable[i]]];
            m_OffsetBo2Chroma[i] = m_ClipTable[i + offsetCr[ppLumaTable[i]]];
        }
    }
    else if (typeIdx == SAO_EO_0 || typeIdx == SAO_EO_1 || typeIdx == SAO_EO_2 || typeIdx == SAO_EO_3)
    {
        offsetCb[0] = 0;
        offsetCr[0] = 0;
        for (Ipp32s i = 0; i < saoLCUParamCb.m_length; i++)
        {
            offsetCb[i + 1] = saoLCUParamCb.m_offset[i] << saoBitIncrease;
            offsetCr[i + 1] = saoLCUParamCr.m_offset[i] << saoBitIncrease;
        }
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++)
        {
            m_OffsetEoChroma[edgeType] = offsetCb[EoTable[edgeType]];
            m_OffsetEo2Chroma[edgeType] = offsetCr[EoTable[edgeType]];
        }
    }
}

void H265SampleAdaptiveOffset::createNonDBFilterInfo()
{
    bool independentSliceBoundaryForNDBFilter = false;
    bool independentTileBoundaryForNDBFilter = false;

    H265Slice* pSlice = m_Frame->GetAU()->GetSlice(0);

    if (pSlice->getPPS()->getNumTiles() > 1 && !pSlice->getPPS()->loop_filter_across_tiles_enabled_flag)
    {
        independentTileBoundaryForNDBFilter = true;
    }

    if (m_Frame->GetAU()->GetSliceCount() > 1)
    {
        for (Ipp32u i = 0; i < m_Frame->GetAU()->GetSliceCount(); i++)
        {
            H265Slice* slice = m_Frame->GetAU()->GetSlice(i);

            if (slice->GetSliceHeader()->slice_loop_filter_across_slices_enabled_flag == false)
            {
                independentSliceBoundaryForNDBFilter = true;
            }

            if (slice->getPPS()->getNumTiles() > 1 && !slice->getPPS()->loop_filter_across_tiles_enabled_flag)
            {
                independentTileBoundaryForNDBFilter = true;
            }
        }
    }

    m_UseNIF = independentSliceBoundaryForNDBFilter || independentTileBoundaryForNDBFilter;
    if (m_UseNIF)
    {
        for (Ipp32u i = 0; i < m_Frame->GetAU()->GetSliceCount(); i++)
        {
            H265Slice *slice = m_Frame->GetAU()->GetSlice(i);
            independentTileBoundaryForNDBFilter = false;

            if (slice->getPPS()->getNumTiles() > 1 && !slice->getPPS()->loop_filter_across_tiles_enabled_flag)
            {
                independentTileBoundaryForNDBFilter = true;
            }

            // Encode order CUs in slice
            for (Ipp32s j = pSlice->m_iFirstMB; j < slice->m_iMaxMB; j++)
            {
                Ipp32u idx = m_Frame->m_CodingData->getCUOrderMap(j);
                m_Frame->getCU(idx)->setNDBFilterBlockBorderAvailability(independentTileBoundaryForNDBFilter);
            }
        }
    }
}

/** PCM CU restoration.
 * \param pcCU pointer to current CU
 * \param uiAbsPartIdx part index
 * \param uiDepth CU depth
 * \returns Void
 */
void H265SampleAdaptiveOffset::PCMCURestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth)
{
    Ipp32u CurNumParts = m_Frame->getCD()->getNumPartInCU() >> (Depth << 1);
    Ipp32u QNumParts   = CurNumParts >> 2;

    // go to sub-CU
    if(pcCU->GetDepth(AbsZorderIdx) > Depth)
    {
        for (Ipp32u PartIdx = 0; PartIdx < 4; PartIdx++, AbsZorderIdx += QNumParts)
        {
            Ipp32u LPelX = pcCU->m_CUPelX + pcCU->m_rasterToPelX[AbsZorderIdx];
            Ipp32u TPelY = pcCU->m_CUPelY + pcCU->m_rasterToPelY[AbsZorderIdx];
            if((LPelX < pcCU->m_SliceHeader->m_SeqParamSet->getPicWidthInLumaSamples()) && (TPelY < pcCU->m_SliceHeader->m_SeqParamSet->getPicHeightInLumaSamples()))
                PCMCURestoration(pcCU, AbsZorderIdx, Depth + 1);
        }
        return;
    }

    // restore PCM samples
    if ((pcCU->GetIPCMFlag(AbsZorderIdx) && pcCU->m_SliceHeader->m_SeqParamSet->getPCMFilterDisableFlag()) || pcCU->isLosslessCoded(AbsZorderIdx))
    {
        PCMSampleRestoration(pcCU, AbsZorderIdx, Depth, TEXT_LUMA);
        PCMSampleRestoration(pcCU, AbsZorderIdx, Depth, TEXT_CHROMA);
    }
}

/** PCM sample restoration.
* \param pcCU pointer to current CU
* \param uiAbsPartIdx part index
* \param uiDepth CU depth
* \param ttText texture component type
* \returns Void
*/
void H265SampleAdaptiveOffset::PCMSampleRestoration(H265CodingUnit* pcCU, Ipp32u AbsZorderIdx, Ipp32u Depth, EnumTextType Text)
{
    H265PlaneYCommon* Src;
    Ipp32u Stride;
    Ipp32u Width;
    Ipp32u Height;
    Ipp32u PcmLeftShiftBit;
    Ipp32u X, Y;
    Ipp32u MinCoeffSize = m_Frame->getMinCUWidth() * m_Frame->getMinCUWidth();
    Ipp32u LumaOffset   = MinCoeffSize * AbsZorderIdx;
    Ipp32u ChromaOffset = LumaOffset >> 2;

    H265FrameCodingData * cd = pcCU->m_Frame->getCD();

    if(Text == TEXT_LUMA)
    {
        H265PlaneYCommon* Pcm;

        Src = m_Frame->GetLumaAddr(pcCU->CUAddr, AbsZorderIdx);
        Pcm = pcCU->m_IPCMSampleY + LumaOffset;
        Stride  = m_Frame->pitch_luma();
        Width  = (cd->m_MaxCUWidth >> Depth);
        Height = (cd->m_MaxCUWidth >> Depth);

        if (pcCU->isLosslessCoded(AbsZorderIdx) && !pcCU->GetIPCMFlag(AbsZorderIdx))
        {
            PcmLeftShiftBit = 0;
        }
        else
        {
            PcmLeftShiftBit = g_bitDepthY - pcCU->m_SliceHeader->m_SeqParamSet->pcm_bit_depth_luma;
        }

        for(Y = 0; Y < Height; Y++)
        {
            for(X = 0; X < Width; X++)
            {
                Src[X] = (Pcm[X] << PcmLeftShiftBit);
            }
            Pcm += Width;
            Src += Stride;
        }
    }
    else
    {
        H265PlaneUVCommon *PcmCb, *PcmCr;

        Src = m_Frame->GetCbCrAddr(pcCU->CUAddr, AbsZorderIdx);
        PcmCb = pcCU->m_IPCMSampleCb + ChromaOffset;
        PcmCr = pcCU->m_IPCMSampleCr + ChromaOffset;

        Stride = m_Frame->pitch_chroma();
        Width  = ((cd->m_MaxCUWidth >> Depth) / 2);
        Height = ((cd->m_MaxCUWidth >> Depth) / 2);
        if (pcCU->isLosslessCoded(AbsZorderIdx) && !pcCU->GetIPCMFlag(AbsZorderIdx))
        {
            PcmLeftShiftBit = 0;
        }
        else
        {
            PcmLeftShiftBit = g_bitDepthC - pcCU->m_SliceHeader->m_SeqParamSet->pcm_bit_depth_chroma;
        }

        for(Y = 0; Y < Height; Y++)
        {
            for(X = 0; X < Width; X++)
            {
                Src[X * 2] = (PcmCb[X] << PcmLeftShiftBit);
                Src[X * 2 + 1] = (PcmCr[X] << PcmLeftShiftBit);
            }
            PcmCb += Width;
            PcmCr += Width;
            Src += Stride;
        }
    }
}

void H265ScalingList::init()
{
    // TODO something w/ this.........
    //m_scalingListCoef[SCALING_LIST_32x32][3] = &m_scalingListCoef[SCALING_LIST_32x32][1][0]; // copy address for 32x32
}

void H265ScalingList::destroy()
{
}

} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
