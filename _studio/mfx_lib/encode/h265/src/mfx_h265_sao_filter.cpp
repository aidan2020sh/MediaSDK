//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_sao_filter.h"
#include "mfx_h265_ctb.h"
#include <math.h>

namespace H265Enc {

const int   g_skipLinesR[NUM_SAO_COMPONENTS] = {5, 3, 3};
const int   g_skipLinesB[NUM_SAO_COMPONENTS] = {4, 2, 2};


inline Ipp16s getSign(Ipp32s x)
{
    return ((x >> 31) | ((Ipp32s)( (((Ipp32u) -x)) >> 31)));
}

// see IEEE "Sample Adaptive Offset in the HEVC standard", p1760 Fast Distortion Estimation
inline Ipp64s EstimateDeltaDist(Ipp64s count, Ipp64s offset, Ipp64s diffSum, int shift)
{
    return (( count*offset*offset - diffSum*offset*2 ) >> shift);
}

template <typename T> inline T Clip3( T minVal, T maxVal, T a) { return std::min<T> (std::max<T> (minVal, a) , maxVal); }  ///< general min/max clip

Ipp64s GetDistortion( int typeIdx, int typeAuxInfo, int* quantOffset,  MFX_HEVC_PP::SaoCtuStatistics& statData, int shift)
{
    Ipp64s dist=0;

    int startFrom = 0;
    int end = NUM_SAO_EO_CLASSES;

    if (SAO_TYPE_BO == typeIdx) {
        startFrom = typeAuxInfo;
        end = startFrom + 4;
    }

    for (int offsetIdx=startFrom; offsetIdx < end; offsetIdx++) {
        dist += EstimateDeltaDist( statData.count[offsetIdx], quantOffset[offsetIdx], statData.diff[offsetIdx], shift);
    }

    return dist;

} // Ipp64s GetDistortion(...)


// try on each offset from [0, inputOffset]
inline int GetBestOffset( int typeIdx, int classIdx, Ipp64f lambda, int inputOffset, 
                          Ipp64s count, Ipp64s diffSum, int shift, int offsetTh,
                          Ipp64f* bestCost = NULL, Ipp64s* bestDist = NULL)// out
{
    int iterOffset = inputOffset;
    int outputOffset = 0;
    int testOffset;

    Ipp64s tempDist, tempRate;
    Ipp64f cost, minCost;
    
    minCost = lambda;

    const int deltaRate = (typeIdx == SAO_TYPE_BO) ? 2 : 1;
    while (iterOffset != 0) {
        tempRate = abs(iterOffset) + deltaRate;
        if (abs(iterOffset) == offsetTh) {
            tempRate --;
        }
     
        testOffset  = iterOffset;
        tempDist    = EstimateDeltaDist( count, testOffset, diffSum, shift);
        cost    = (Ipp64f)tempDist + lambda * (Ipp64f) tempRate;

        if (cost < minCost) {
            minCost = cost;
            outputOffset = iterOffset;
            if (bestDist) *bestDist = tempDist;
            if (bestCost) *bestCost = cost;
        }
        
        iterOffset = (iterOffset > 0) ? (iterOffset-1) : (iterOffset+1);
    }

    return outputOffset;

} // int GetBestOffset( ... )

static int tab_numSaoClass[] = {NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_EO_CLASSES, NUM_SAO_BO_CLASSES};

void GetQuantOffsets( int typeIdx,  MFX_HEVC_PP::SaoCtuStatistics& statData, int* quantOffsets, int& typeAuxInfo,
                      Ipp64f lambda, int bitDepth, int saoMaxOffsetQval, int shift)
{
    memset(quantOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    
    int numClasses = tab_numSaoClass[typeIdx];
    int classIdx;

    // calculate 'native' offset
    for (classIdx = 0; classIdx < numClasses; classIdx++) {
        if ( 0 == statData.count[classIdx] || (SAO_CLASS_EO_PLAIN == classIdx && SAO_TYPE_BO != typeIdx) ) {
            continue; //offset will be zero
        }

        Ipp64f meanDiff = (Ipp64f)(statData.diff[classIdx] << (bitDepth - 8)) / (Ipp64f)(statData.count[classIdx]);

        int offset;
        if (bitDepth > 8) {
            offset = (int)meanDiff;
            offset += offset >= 0 ? (1<<(bitDepth-8-1)) : -(1<<(bitDepth-8-1));
            offset >>= (bitDepth-8);
        } else {
            offset = (int)floor(meanDiff + 0.5);
        }

        quantOffsets[classIdx] = Clip3(-saoMaxOffsetQval, saoMaxOffsetQval, offset);
    }
    
    // try on to improve a 'native' offset
    if (SAO_TYPE_BO == typeIdx) {
        Ipp64f cost[NUM_SAO_BO_CLASSES];
        for (int classIdx = 0; classIdx < NUM_SAO_BO_CLASSES; classIdx++) {
            cost[classIdx] = lambda;
            if (quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx], 
                                                        statData.count[classIdx], statData.diff[classIdx],
                                                        shift, saoMaxOffsetQval, cost + classIdx);
            }
        }
        
        Ipp64f minCost = MAX_DOUBLE, bandCost;
        int band, startBand = 0;
        for (band = 0; band < (NUM_SAO_BO_CLASSES - 4 + 1); band++) {
            bandCost  = cost[band  ] + cost[band+1] + cost[band+2] + cost[band+3];

            if (bandCost < minCost) {
                minCost = bandCost;
                startBand = band;
            }
        }

        // clear unused bands
        for (band = 0; band < startBand; band++)
            quantOffsets[band] = 0;

        for (band = startBand + 4; band < NUM_SAO_BO_CLASSES; band++)
            quantOffsets[band] = 0;

        typeAuxInfo = startBand;

    } else { // SAO_TYPE_E[0/45/90/135]
        for (classIdx = 0; classIdx < NUM_SAO_EO_CLASSES; classIdx++) {
            if (SAO_CLASS_EO_FULL_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_HALF_PEAK == classIdx && quantOffsets[classIdx] > 0   ||
                SAO_CLASS_EO_FULL_VALLEY == classIdx && quantOffsets[classIdx] < 0 ||
                SAO_CLASS_EO_HALF_VALLEY == classIdx && quantOffsets[classIdx] < 0) {
                quantOffsets[classIdx] = 0;
            }

            if ( quantOffsets[classIdx] != 0 ) {
                quantOffsets[classIdx] = GetBestOffset( typeIdx, classIdx, lambda, quantOffsets[classIdx],
                                                        statData.count[classIdx], statData.diff[classIdx], shift, saoMaxOffsetQval);
            }
        }

        typeAuxInfo = 0;
    }

} // void GetQuantOffsets(...)


template <typename PixType>
void GetCtuStatistics_RBoundary_Internal( int compIdx, const PixType* recBlk, int recStride, const PixType* orgBlk,
                                          int orgStride, int width,  int height, int shift,// to switch btw NV12/YV12 Chroma
                                          const MFX_HEVC_PP::CTBBorders& borders, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes)
{
    int x, y, startX, startY, endX, endY;
    int edgeType;
    Ipp16s signLeft, signRight;//, signDown;
    Ipp64s *diff, *count;

    int skipLinesR = g_skipLinesR[compIdx];
    int skipLinesB = g_skipLinesB[compIdx];

    const PixType *recLine, *orgLine;
    const PixType* recLineAbove;
    const PixType* recLineBelow;

    for (int typeIdx = 0; typeIdx < NUM_SAO_BASE_TYPES; typeIdx++) {
        MFX_HEVC_PP::SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
        statsData.Reset();

        recLine = recBlk;
        orgLine = orgBlk;
        diff    = statsData.diff;
        count   = statsData.count;
        switch(typeIdx) {
            case SAO_TYPE_EO_0: {
                diff +=2;
                count+=2;
                endY   = height - skipLinesB;

                startX = width - skipLinesR + 1;
                endX   = width -1;

                for (y = 0; y < endY; y++) {
                    signLeft = getSign(recLine[startX] - recLine[startX-1]);
                    for (x = startX; x < endX; x++) {
                        signRight =  getSign(recLine[x] - recLine[x+1]);
                        edgeType  =  signRight + signLeft;
                        signLeft  = -signRight;

                        diff [edgeType] += (orgLine[x << shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_90: {
                diff +=2;
                count+=2;

                startX = width - skipLinesR + 1;
                endX   = width;

                startY = borders.m_top ? 0 : 1;
                endY   = height - skipLinesB;
                if ( 0 == borders.m_top ) {
                    recLine += recStride;
                    orgLine += orgStride;
                }

                for (y = startY; y < endY; y++) {
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType = getSign(recLine[x] - recLineBelow[x]) + getSign(recLine[x] - recLineAbove[x]);

                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_135: {
                diff +=2;
                count+=2;

                startX = width - skipLinesR+1;
                endX   = width - 1;

                startY = borders.m_top ? 0 : 1;
                endY   = height - skipLinesB;

                if ( 0 == borders.m_top ) {
                    recLine += recStride;
                    orgLine += orgStride;
                }

                //middle lines only, eclude 2 corner points
                for (y = startY; y < endY; y++) {
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType = getSign(recLine[x] - recLineBelow[x+1]) + getSign(recLine[x] - recLineAbove[x-1]);
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_45: {
                diff +=2;
                count+=2;

                startX = width - skipLinesR+1;
                endX   = width - 1;

                startY = borders.m_top ? 0 : 1;
                endY   = height - skipLinesB;

                if ( 0 == borders.m_top ) {
                    recLine += recStride;
                    orgLine += orgStride;
                }

                //middle lines only, eclude 2 corner points
                for (y = startY; y < endY; y++) {
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType = getSign(recLine[x] - recLineBelow[x-1]) + getSign(recLine[x] - recLineAbove[x+1]);
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_BO: {
                startX = width- skipLinesR + 1;
                endX = width;

                endY = height- skipLinesB;
                const int shiftBits = 3;
                for (y = 0; y < endY; y++) {
                    for (x = startX; x < endX; x++) {
                        int bandIdx= recLine[x] >> shiftBits;
                        diff [bandIdx] += (orgLine[x<<shift] - recLine[x]);
                        count[bandIdx]++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            default: {
                VM_ASSERT(!"Non supported SAO type\n");
            }
        }
    }

} // void GetCtuStatistics_RBoundary_Internal(...)


template <typename PixType>
void GetCtuStatistics_BottomBoundary_Internal( int compIdx, const PixType* recBlk, int recStride, const PixType* orgBlk, 
                                               int orgStride, int width,  int height, int shift,// to switch btw NV12/YV12 Chroma
                                               const MFX_HEVC_PP::CTBBorders& borders, MFX_HEVC_PP::SaoCtuStatistics* statsDataTypes)
{
    int x, y, startX, startY, endX, endY;
    int edgeType;
    Ipp16s signLeft;
    Ipp16s signRight;//, signDown;
    Ipp64s *diff, *count;
    int skipLinesB = g_skipLinesB[compIdx];
    const PixType *recLine, *orgLine;
    const PixType* recLineAbove;
    const PixType* recLineBelow;

    int offset = (height - skipLinesB);
    recBlk = recBlk + offset*recStride;
    orgBlk = orgBlk + offset*orgStride;

    for (int typeIdx = 0; typeIdx < NUM_SAO_BASE_TYPES; typeIdx++) {
        MFX_HEVC_PP::SaoCtuStatistics& statsData= statsDataTypes[typeIdx];
        statsData.Reset();

        recLine = recBlk;
        orgLine = orgBlk;
        diff    = statsData.diff;
        count   = statsData.count;
        switch (typeIdx) {
            case SAO_TYPE_EO_0: {
                diff +=2;
                count+=2;
                endY   = height - skipLinesB;

                startX = borders.m_left ? 0 : 1;
                endX   = width -1;

                for (y = 0; y < skipLinesB; y++) {
                    signLeft = getSign(recLine[startX] - recLine[startX-1]);
                    for (x = startX; x < endX; x++) {
                        signRight =  getSign(recLine[x] - recLine[x+1]);
                        edgeType  =  signRight + signLeft;
                        signLeft  = -signRight;

                        diff [edgeType] += (orgLine[x << shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine  += recStride;
                    orgLine  += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_90: {
                diff  +=2;
                count +=2;
                startX = 0;
                endX   = width;

                startY = 0;
                endY   = skipLinesB;

                for (y = startY; y < endY-1; y++) {// aya :: -1 due to real boundaries
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType  = getSign(recLine[x] - recLineBelow[x]) + getSign(recLine[x] - recLineAbove[x]);

                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_135: {
                diff +=2;
                count+=2;

                startX = borders.m_left ? 0 : 1;
                endX   = width - 1;

                for (y = 0; y < skipLinesB-1; y++) {
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType = getSign(recLine[x] - recLineBelow[x+1]) + getSign(recLine[x] - recLineAbove[x-1]);
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_EO_45: {
                diff +=2;
                count+=2;

                startX = borders.m_left ? 0 : 1;
                endX   = width - 1;
                for (y = 0; y < skipLinesB-1; y++) {
                    recLineBelow = recLine + recStride;
                    recLineAbove = recLine - recStride;

                    for (x = startX; x < endX; x++) {
                        edgeType = getSign(recLine[x] - recLineBelow[x-1]) + getSign(recLine[x] - recLineAbove[x+1]);
                        diff [edgeType] += (orgLine[x<<shift] - recLine[x]);
                        count[edgeType] ++;
                    }

                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            case SAO_TYPE_BO: {
                startX = 0;
                endX = width;

                endY = height- skipLinesB;
                const int shiftBits = 3;

                for (y = 0; y < skipLinesB; y++) {
                    for (x = startX; x < endX; x++) {
                        int bandIdx = recLine[x] >> shiftBits;
                        diff [bandIdx] += (orgLine[x<<shift] - recLine[x]);
                        count[bandIdx]++;
                    }
                    recLine += recStride;
                    orgLine += orgStride;
                }
            }
            break;
            default: {
                VM_ASSERT(!"Non supported SAO type\n");
            }
        }
    }

} // void GetCtuStatistics_BottomBoundary_Internal(...)


void InvertQuantOffsets(int type_idx, int typeAuxInfo, int* dstOffsets, int* srcOffsets)
{
    int codedOffset[MAX_NUM_SAO_CLASSES];

    small_memcpy(codedOffset, srcOffsets, sizeof(int)*MAX_NUM_SAO_CLASSES);
    memset(dstOffsets, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

    if (type_idx == SAO_TYPE_BO) {
        for (int i = 0; i < 4; i++) {
            int pos = (typeAuxInfo+ i)%NUM_SAO_BO_CLASSES;
            dstOffsets[pos] = codedOffset[pos];// not full range is copied
        }
    } else {//EO    
        for (int i = 0; i < NUM_SAO_EO_CLASSES; i++) {
            dstOffsets[i] = codedOffset[i];
        }
        VM_ASSERT(dstOffsets[SAO_CLASS_EO_PLAIN] == 0); //keep EO plain offset as zero
    }

} // void InvertQuantOffsets( ... )


void SaoEncodeFilter::ReconstructCtuSaoParam(SaoCtuParam& recParam)
{
    SaoCtuParam* mergeList[2] = {NULL, NULL};
    GetMergeList(m_ctb_addr, mergeList);

    for (int compIdx = 0; compIdx < NUM_USED_SAO_COMPONENTS; compIdx++) {
        SaoOffsetParam& offsetParam = recParam[compIdx];

        if ( SAO_MODE_ON == offsetParam.mode_idx ) {
            InvertQuantOffsets(offsetParam.type_idx, offsetParam.typeAuxInfo, offsetParam.offset, offsetParam.offset);
        }
        else if ( SAO_MODE_MERGE == offsetParam.mode_idx ) { // were reconstructed early
            SaoCtuParam* mergeTarget = mergeList[offsetParam.type_idx];
            VM_ASSERT(mergeTarget != NULL);

            offsetParam = (*mergeTarget)[compIdx];
        }
    }

} // void SaoEncodeFilter::ReconstructCtuSaoParam(...)


void SliceDecisionAlgorithm(bool* sliceEnabled, int picTempLayer)
{
    //(here could be implemented adaptive SAO)
    for (int compIdx = 0; compIdx < NUM_USED_SAO_COMPONENTS; compIdx++) {
        sliceEnabled[compIdx] = true;
    }

} // void SliceDecisionAlgorithm(bool* sliceEnabled, int picTempLayer)

// SAO Offset
SaoOffsetParam::SaoOffsetParam()
{
    Reset();
}

SaoOffsetParam::~SaoOffsetParam()
{
}

void SaoOffsetParam::Reset()
{
    mode_idx = SAO_MODE_OFF;
    type_idx = -1;
    typeAuxInfo = -1;
    memset(offset, 0, sizeof(int)* MAX_NUM_SAO_CLASSES);
}


SaoOffsetParam& SaoOffsetParam::operator= (const SaoOffsetParam& src)
{
    mode_idx = src.mode_idx;
    type_idx = src.type_idx;
    typeAuxInfo = src.typeAuxInfo;
    saoMaxOffsetQVal = src.saoMaxOffsetQVal;
    small_memcpy(offset, src.offset, sizeof(int)* MAX_NUM_SAO_CLASSES);

    return *this;
}

// SaoCtuParam
SaoCtuParam::SaoCtuParam()
{
    Reset();
}

SaoCtuParam::~SaoCtuParam()
{
}

void SaoCtuParam::Reset()
{
    for (int compIdx = 0; compIdx < 3; compIdx++){
        m_offsetParam[compIdx].Reset();
    }
}


SaoCtuParam& SaoCtuParam::operator= (const SaoCtuParam& src)
{
    for (int compIdx = 0; compIdx < 3; compIdx++){
        m_offsetParam[compIdx] = src.m_offsetParam[compIdx];
    }
    return *this;

}

// SAO ENCODE FILTER
SaoEncodeFilter::SaoEncodeFilter()
{
} // SaoEncodeFilter::SaoEncodeFilter()


void SaoEncodeFilter::Close()
{
} // void SaoEncodeFilter::Close()


SaoEncodeFilter::~SaoEncodeFilter()
{
    Close();
} // SaoEncodeFilter::~SaoEncodeFilter()


void SaoEncodeFilter::Init(int width, int height, int maxCUWidth, int maxDepth, int bitDepth, int saoOpt)
{
    m_frameSize.width = width;
    m_frameSize.height= height;

    m_maxCuSize = maxCUWidth;

    m_bitDepth = bitDepth;
    m_saoMaxOffsetQVal = (1<<(MIN(m_bitDepth,10)-5))-1;

    m_numCTU_inWidth = (m_frameSize.width  + m_maxCuSize - 1) / m_maxCuSize;
    m_numCTU_inHeight= (m_frameSize.height + m_maxCuSize - 1) / m_maxCuSize;
#ifndef AMT_SAO_MIN
    m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : NUM_SAO_BASE_TYPES - 1;
#else
    m_numSaoModes = (saoOpt == SAO_OPT_ALL_MODES) ? NUM_SAO_BASE_TYPES : ((saoOpt == SAO_OPT_FAST_MODES_ONLY)? NUM_SAO_BASE_TYPES - 1: 1);
#endif
} // void SaoEncodeFilter::Init(...)


template <typename PixType>
void SaoEncodeFilter::EstimateCtuSao(  mfxFrameData* orgYuv, mfxFrameData* recYuv, bool* sliceEnabled, SaoCtuParam* saoParam)
{
    GetCtuSaoStatistics<PixType>(orgYuv, recYuv);

#if defined(MFX_HEVC_SAO_PREDEBLOCKED_ENABLED)
    for(int i = 0; i < NUM_SAO_BASE_TYPES; i++) {
        m_statData[SAO_Y][i] += m_statData_predeblocked[SAO_Y][i];
    }
#endif
    
    sliceEnabled[SAO_Y] = sliceEnabled[SAO_Cb] = sliceEnabled[SAO_Cr] = false;
    
    int tmpDepth = 0;
    SliceDecisionAlgorithm(sliceEnabled, tmpDepth); //adaptive SAO disabled now. 

    GetBestCtuSaoParam(sliceEnabled, recYuv, saoParam);

} // void SaoEncodeFilter::EstimateCtuSao()


template <typename PixType>
void SaoEncodeFilter::GetCtuSaoStatistics(mfxFrameData* orgYuv, mfxFrameData* recYuv)
{

    {
        int xPos   = m_ctb_pelx;
        int yPos   = m_ctb_pely;
        int height = (yPos + m_maxCuSize > m_frameSize.height)?(m_frameSize.height- yPos):m_maxCuSize;
        int width  = (xPos + m_maxCuSize  > m_frameSize.width )?(m_frameSize.width - xPos):m_maxCuSize;
        
        int compIdx = SAO_Y;
        int shift   = 0;// YV12/NV12 Chroma dispatcher
        int  recStride = recYuv->Pitch;
        PixType* recBlk=  (PixType*)recYuv->Y;

        int  orgStride  = orgYuv->Pitch;
        PixType* orgBlk = (PixType*)orgYuv->Y;

        h265_GetCtuStatistics(compIdx, recBlk, recStride, orgBlk, orgStride,
                              width, height, shift, m_borders, m_numSaoModes, m_statData[compIdx]);

        for (compIdx = 1; compIdx < NUM_USED_SAO_COMPONENTS; compIdx++ ) {
            int  recStride = recYuv->PitchHigh;//yv12
            PixType* recBlk=  (compIdx == SAO_Cb) ? (PixType*)recYuv->Cb : (PixType*)recYuv->Cr;

            int  orgStride  = orgYuv->Pitch; //nv12
            PixType* orgBlk = (compIdx == SAO_Cb) ? (PixType*)orgYuv->CbCr : (PixType*)orgYuv->CbCr + 1;

            shift = 1;

            h265_GetCtuStatistics(compIdx, recBlk, recStride, orgBlk, orgStride,
                                  width >> 1, height >> 1, shift, m_borders, m_numSaoModes, m_statData[compIdx]);
        }
    }

} // void SaoEncodeFilter::GetCtuSaoStatistics(...)


void SaoEncodeFilter::GetBestCtuSaoParam( bool* sliceEnabled, mfxFrameData* srcYuv, SaoCtuParam* codedParam)
{
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    SaoCtuParam* mergeList[2] = {NULL, NULL};
    GetMergeList(m_ctb_addr, mergeList);

    SaoCtuParam modeParam;
    Ipp64f minCost = MAX_DOUBLE, modeCost = MAX_DOUBLE;

    ModeDecision_Base( mergeList, sliceEnabled, modeParam, modeCost, SAO_CABACSTATE_BLK_CUR);

    if (modeCost < minCost) {
        minCost = modeCost;
        *codedParam = modeParam;
        m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    }
            
#if defined(SAO_MODE_MERGE_ENABLED)
    ModeDecision_Merge( mergeList, sliceEnabled, modeParam, modeCost, SAO_CABACSTATE_BLK_CUR);

    if(modeCost < minCost) {
        minCost = modeCost;
        *codedParam = modeParam;
        m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_NEXT], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    }
#endif
} // void SaoEncodeFilter::GetBestCtuSaoParam(...)


void SaoEncodeFilter::GetMergeList(int ctu, SaoCtuParam* mergeList[2])
{
    int ctuX = ctu % m_numCTU_inWidth;
    int ctuY = ctu / m_numCTU_inWidth;
    int mergedCTUPos;

    if (ctuY > 0) {
        mergedCTUPos = ctu- m_numCTU_inWidth;        
        if ( m_slice_ids[ctu] == m_slice_ids[mergedCTUPos] ) {
            mergeList[SAO_MERGE_ABOVE] = &(m_codedParams_TotalFrame[mergedCTUPos]);
        }
    }

    if (ctuX > 0) {
        mergedCTUPos = ctu- 1;
        if ( m_slice_ids[ctu] == m_slice_ids[mergedCTUPos] ) {
            mergeList[SAO_MERGE_LEFT] = &(m_codedParams_TotalFrame[mergedCTUPos]);
        }
    }

} // void SaoEncodeFilter::GetMergeList(int ctu, SaoCtuParam* blkParams, SaoCtuParam* mergeList[2])

void SaoEncodeFilter::ModeDecision_Merge( SaoCtuParam* mergeList[2], bool* sliceEnabled,
                                          SaoCtuParam& bestParam, Ipp64f& bestCost, int cabac)
{
    int shift = 2 * (m_bitDepth - 8);
    bestCost = MAX_DOUBLE;
    Ipp64f cost;
    SaoCtuParam testParam;

    for (int mode=0; mode< NUM_SAO_MERGE_TYPES; mode++) {
        if (NULL == mergeList[mode]) {
            continue;
        }

        testParam = *(mergeList[mode]);
        Ipp64f distortion=0;
        for (int compIdx=0; compIdx< 1; compIdx++) {
            testParam[compIdx].mode_idx = SAO_MODE_MERGE;
            testParam[compIdx].type_idx = mode;

            SaoOffsetParam& mergedOffset = (*(mergeList[mode]))[compIdx];

            if ( SAO_MODE_OFF != mergedOffset.mode_idx ) {
                distortion += (Ipp64f)GetDistortion( mergedOffset.type_idx, mergedOffset.typeAuxInfo,
                                                     mergedOffset.offset, m_statData[compIdx][mergedOffset.type_idx], shift);
            }
        }

        m_bsf->CtxRestore(m_ctxSAO[cabac], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        m_bsf->Reset();

        CodeSaoCtbParam(m_bsf, testParam, sliceEnabled, (NULL != mergeList[SAO_MERGE_LEFT]), (NULL != mergeList[SAO_MERGE_ABOVE]), false);

        int rate = GetNumWrittenBits();

        cost = distortion / m_labmda[0] + (Ipp64f)rate;

        if (cost < bestCost) {
            bestCost = cost;
            bestParam= testParam;
            m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

} // void SaoEncodeFilter::ModeDecision_Merge(...)


// etimate compression gain SAO_ON vs SAO_OFF via RDO
void SaoEncodeFilter::ModeDecision_Base( SaoCtuParam* mergeList[2],  bool *sliceEnabled, SaoCtuParam &bestParam, 
                                         Ipp64f &bestCost, int cabac)
{
    int shift = 2 * (m_bitDepth - 8);
    Ipp64f minCost = 0.0, cost = 0.0;
    int rate = 0, minRate = 0;
    Ipp64s dist[NUM_SAO_COMPONENTS], modeDist[NUM_SAO_COMPONENTS];
    SaoOffsetParam testOffset[NUM_SAO_COMPONENTS];
    int compIdx;

    modeDist[SAO_Y]= modeDist[SAO_Cb] = modeDist[SAO_Cr] = 0;
        
    bestParam[SAO_Y].mode_idx = SAO_MODE_OFF;
    bestParam[SAO_Y].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, mergeList[SAO_MERGE_LEFT] != NULL, mergeList[SAO_MERGE_ABOVE] != NULL, true);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    compIdx = SAO_Y;
    bestParam[compIdx].mode_idx = SAO_MODE_OFF;

    m_bsf->Reset();
    CodeSaoCtbOffsetParam(m_bsf, compIdx, bestParam[compIdx], sliceEnabled[compIdx]);
    minRate = GetNumWrittenBits();

    modeDist[compIdx] = 0;
    minCost = m_labmda[compIdx] * (Ipp64f)minRate;

    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);

    if (sliceEnabled[compIdx]) {
        for (Ipp32s type_idx = 0; type_idx < m_numSaoModes; type_idx++) {
            testOffset[compIdx].mode_idx = SAO_MODE_ON;
            testOffset[compIdx].type_idx = type_idx;
            testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

            GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                            testOffset[compIdx].typeAuxInfo, m_labmda[compIdx], m_bitDepth, m_saoMaxOffsetQVal, shift);

            dist[compIdx] = GetDistortion(testOffset[compIdx].type_idx, testOffset[compIdx].typeAuxInfo,
                                          testOffset[compIdx].offset, m_statData[compIdx][type_idx], shift);

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, compIdx, testOffset[compIdx], sliceEnabled[compIdx]);
            rate = GetNumWrittenBits();

            cost = (Ipp64f)dist[compIdx] + m_labmda[compIdx]*((Ipp64f)rate);
            if (cost < minCost) {
                minCost = cost;
                minRate = rate;
                modeDist[compIdx] = dist[compIdx];
                bestParam[compIdx]= testOffset[compIdx];

                m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            }
        }
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_TEMP], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->CtxSave(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    
    bool isChromaEnabled = (NUM_USED_SAO_COMPONENTS > 1); // ? true : false;
    Ipp64f chromaLambda = m_labmda[SAO_Cb];
    
    if ( isChromaEnabled ) {
        VM_ASSERT(m_labmda[SAO_Cb] == m_labmda[SAO_Cr]);
        Ipp64f chromaLambda = m_labmda[SAO_Cb];

        m_bsf->Reset();

        bestParam[SAO_Cb].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cb].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, bestParam[SAO_Cb], sliceEnabled[SAO_Cb]);

        bestParam[SAO_Cr].mode_idx = SAO_MODE_OFF;
        bestParam[SAO_Cr].saoMaxOffsetQVal = m_saoMaxOffsetQVal;
        CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, bestParam[SAO_Cr], sliceEnabled[SAO_Cr]);
        
        minRate = GetNumWrittenBits();

        modeDist[SAO_Cb] = modeDist[SAO_Cr]= 0;
        minCost= chromaLambda*((Ipp64f)minRate);

        for (Ipp32s type_idx = 0; type_idx < NUM_SAO_BASE_TYPES; type_idx++) {
            for (compIdx = SAO_Cb; compIdx < NUM_SAO_COMPONENTS; compIdx++) {
                if (!sliceEnabled[compIdx]) {
                    testOffset[compIdx].mode_idx = SAO_MODE_OFF;
                    dist[compIdx]= 0;
                    continue;
                }
                testOffset[compIdx].mode_idx = SAO_MODE_ON;
                testOffset[compIdx].type_idx = type_idx;
                testOffset[compIdx].saoMaxOffsetQVal = m_saoMaxOffsetQVal;

                GetQuantOffsets(type_idx, m_statData[compIdx][type_idx], testOffset[compIdx].offset,
                                testOffset[compIdx].typeAuxInfo, m_labmda[compIdx],
                                m_bitDepth, m_saoMaxOffsetQVal, shift);                

                dist[compIdx]= GetDistortion(type_idx, testOffset[compIdx].typeAuxInfo,
                                             testOffset[compIdx].offset,
                                             m_statData[compIdx][type_idx], shift);
            }

            m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_MID], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
            m_bsf->Reset();
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cb, testOffset[SAO_Cb], sliceEnabled[SAO_Cb]);
            CodeSaoCtbOffsetParam(m_bsf, SAO_Cr, testOffset[SAO_Cr], sliceEnabled[SAO_Cr]);
            rate = GetNumWrittenBits();

            cost = (Ipp64f)(dist[SAO_Cb]+ dist[SAO_Cr]) + chromaLambda*((Ipp64f)rate);
            if (cost < minCost) {
                minCost = cost;
                minRate = rate;
                modeDist[SAO_Cb] = dist[SAO_Cb];
                modeDist[SAO_Cr] = dist[SAO_Cr];
                bestParam[SAO_Cb]= testOffset[SAO_Cb];
                bestParam[SAO_Cr]= testOffset[SAO_Cr];
            }
        }
    }
        
    bestCost = (Ipp64f)modeDist[SAO_Y]/m_labmda[SAO_Y];

    if (isChromaEnabled) {
        bestCost += (Ipp64f)(modeDist[SAO_Cb]+ modeDist[SAO_Cr])/chromaLambda;
    }

    m_bsf->CtxRestore(m_ctxSAO[SAO_CABACSTATE_BLK_CUR], tab_ctxIdxOffset[SAO_MERGE_FLAG_HEVC], 2);
    m_bsf->Reset();
    CodeSaoCtbParam(m_bsf, bestParam, sliceEnabled, (mergeList[SAO_MERGE_LEFT]!= NULL), (mergeList[SAO_MERGE_ABOVE]!= NULL), false);
    bestCost += (Ipp64f)GetNumWrittenBits();

} // void SaoEncodeFilter::ModeDecision_Base(...)

// SAO DECODE FILTER
template <typename PixType>
SaoDecodeFilter<PixType>::SaoDecodeFilter()
{
    m_ClipTable     = NULL;
    m_ClipTableBase = NULL;
    m_lumaTableBo   = NULL;

    m_TmpU = NULL;
    m_TmpL = NULL;

} // SaoDecodeFilter::SaoDecodeFilter()


template <typename PixType>
void SaoDecodeFilter<PixType>::Close()
{    
    m_ClipTable = NULL;

    if (m_ClipTableBase) {
        delete [] m_ClipTableBase;
        m_ClipTableBase = NULL;
    }

    if (m_lumaTableBo) {
        delete [] m_lumaTableBo;
        m_lumaTableBo = NULL;
    }

    if (m_TmpU) {
        delete[] m_TmpU;
        m_TmpU = NULL;
    }

    if (m_TmpL) {
        delete[] m_TmpL;
        m_TmpL = NULL;
    }
} // void SaoDecodeFilter::Close()


template <typename PixType>
SaoDecodeFilter<PixType>::~SaoDecodeFilter()
{
    Close();
} // SaoDecodeFilter::~SaoDecodeFilter()


template <typename PixType>
void SaoDecodeFilter<PixType>::Init(int maxCUWidth, int maxDepth, int bitDepth, int num)
{
    m_bitDepth = bitDepth;

    m_maxCuSize  = maxCUWidth;

    Ipp32u uiPixelRangeY = 1 << m_bitDepth;
    Ipp32u uiBoRangeShiftY = m_bitDepth - SAO_BO_BITS;
    m_lumaTableBo = new PixType[uiPixelRangeY];
    for (Ipp32u k2 = 0; k2 < uiPixelRangeY; k2++) {
        m_lumaTableBo[k2] = (PixType)(1 + (k2>>uiBoRangeShiftY));
    }

    Ipp32u uiMaxY  = (1 << m_bitDepth) - 1;
    Ipp32u uiMinY  = 0;

    Ipp32u iCRangeExt = uiMaxY>>1;

    m_ClipTableBase = new PixType[uiMaxY+2*iCRangeExt];

    for (Ipp32u i = 0; i < (uiMinY + iCRangeExt);i++) {
        m_ClipTableBase[i] = (PixType)uiMinY;
    }

    for (Ipp32u i = uiMinY + iCRangeExt; i < (uiMaxY + iCRangeExt); i++) {
        m_ClipTableBase[i] = (PixType)(i - iCRangeExt);
    }

    for (Ipp32u i = uiMaxY + iCRangeExt; i < (uiMaxY + 2 * iCRangeExt); i++) {
        m_ClipTableBase[i] = (PixType)uiMaxY;
    }

    m_ClipTable = &(m_ClipTableBase[iCRangeExt]);

    m_TmpU = new PixType [num * maxCUWidth];
    m_TmpL = new PixType [num * maxCUWidth];
}
  // void SaoDecodeFilter::Init(...)


template <typename PixType>
void SaoDecodeFilter<PixType>::SetOffsetsLuma(SaoOffsetParam  &saoLCUParam, Ipp32s typeIdx, Ipp32s *offsetEo, PixType *offsetBo)
{
    Ipp32s offset[LUMA_GROUP_NUM + 1] = {0};
    static const Ipp8u EoTable[9] = { 1,  2,   0,  3,  4,  0,  0,  0,  0 };

    if (typeIdx == SAO_TYPE_BO) {
        for (Ipp32s i = 0; i < NUM_SAO_BO_CLASSES + 1; i++) {
            offset[i] = 0;
        }
        for (Ipp32s i = 0; i < 4; i++) {
            offset[(saoLCUParam.typeAuxInfo + i) % NUM_SAO_BO_CLASSES + 1] = saoLCUParam.offset[(saoLCUParam.typeAuxInfo + i) % NUM_SAO_BO_CLASSES];//[i];
        }

        PixType *ppLumaTable = m_lumaTableBo;
        for (Ipp32s i = 0; i < (1 << m_bitDepth); i++) {
            offsetBo[i] = m_ClipTable[i + offset[ppLumaTable[i]]];
        }
    }
    else if (typeIdx == SAO_TYPE_EO_0 || typeIdx == SAO_TYPE_EO_90 || typeIdx == SAO_TYPE_EO_135 || typeIdx == SAO_TYPE_EO_45) {
        for (Ipp32s edgeType = 0; edgeType < 6; edgeType++) {
            offsetEo[edgeType] = saoLCUParam.offset[edgeType];
        }
    }
}

//  CTU Caller
template <typename PixType>
void H265CU<PixType>::EstimateCtuSao( H265BsFake *bs, SaoCtuParam* saoParam, SaoCtuParam* saoParam_TotalFrame,
                                      const MFX_HEVC_PP::CTBBorders & borders, const Ipp8u* slice_ids)
{
    m_saoEncodeFilter.m_ctb_addr = this->m_ctbAddr;
    m_saoEncodeFilter.m_ctb_pelx = this->m_ctbPelX;
    m_saoEncodeFilter.m_ctb_pely = this->m_ctbPelY;

    m_saoEncodeFilter.m_codedParams_TotalFrame = saoParam_TotalFrame;
    m_saoEncodeFilter.m_bsf = bs;
    m_saoEncodeFilter.m_labmda[0] = this->m_rdLambda*256;
    m_saoEncodeFilter.m_borders = borders;

    m_saoEncodeFilter.m_slice_ids = (Ipp8u*)slice_ids;
    //chroma issues
    m_saoEncodeFilter.m_labmda[1] = m_saoEncodeFilter.m_labmda[2] = m_saoEncodeFilter.m_labmda[0];

    mfxFrameData orgYuv;
    mfxFrameData recYuv;

    orgYuv.Y = (mfxU8*)this->m_ySrc;
    orgYuv.UV = (mfxU8*)this->m_uvSrc;
    orgYuv.Pitch = (Ipp16s)this->m_pitchSrcLuma;

    recYuv.Y = (mfxU8*)this->m_yRec;
    recYuv.UV = (mfxU8*)this->m_uvRec;
    recYuv.Pitch = (Ipp16s)this->m_pitchRecLuma;
    recYuv.PitchHigh = (Ipp16s)this->m_pitchRecLuma;

    bool    sliceEnabled[NUM_SAO_COMPONENTS] = {false, false, false};
    m_saoEncodeFilter.EstimateCtuSao<PixType>( &orgYuv, &recYuv, sliceEnabled, saoParam);
    
    // set slice param
    if ( !this->m_cslice->slice_sao_luma_flag ) {
        this->m_cslice->slice_sao_luma_flag = (Ipp8u)sliceEnabled[SAO_Y];
    }
    if ( !this->m_cslice->slice_sao_chroma_flag ) {
        this->m_cslice->slice_sao_chroma_flag = (sliceEnabled[SAO_Cb] || sliceEnabled[SAO_Cr] ) ? 1 : 0;
    }

} // void H265CU::EstimateCtuSao( void )


template <typename PixType>
void H265CU<PixType>::GetStatisticsCtuSaoPredeblocked( const MFX_HEVC_PP::CTBBorders & borders )
{
    int maxCUSixe = m_saoEncodeFilter.m_maxCuSize;
    IppiSize frameSize = m_saoEncodeFilter.m_frameSize;

    int height = ((int)this->m_ctbPelY + maxCUSixe > frameSize.height)?(frameSize.height- this->m_ctbPelY):maxCUSixe;
    int width  = ((int)this->m_ctbPelX + maxCUSixe  > frameSize.width )?(frameSize.width - this->m_ctbPelX):maxCUSixe;

    mfxFrameData orgYuv;
    mfxFrameData recYuv;

    orgYuv.Y = (mfxU8*)this->m_ySrc;
    orgYuv.UV = (mfxU8*)this->m_uvSrc;
    orgYuv.Pitch = (Ipp16s)this->m_pitchSrcLuma;

    recYuv.Y = (mfxU8*)this->m_yRec;
    recYuv.UV = (mfxU8*)this->m_uvRec;
    recYuv.Pitch = (Ipp16s)this->m_pitchRecLuma;
    recYuv.PitchHigh = (Ipp16s)this->m_pitchRecLuma;

    int compIdx = SAO_Y;

    MFX_HEVC_PP::SaoCtuStatistics    statData_predeblocked_R[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];
    MFX_HEVC_PP::SaoCtuStatistics    statData_predeblocked_B[NUM_SAO_COMPONENTS][NUM_SAO_BASE_TYPES];

    GetCtuStatistics_RBoundary_Internal( compIdx, recYuv.Y, recYuv.Pitch, orgYuv.Y, orgYuv.Pitch, 
                                         width, height, 0, borders, statData_predeblocked_R[compIdx]);

    GetCtuStatistics_BottomBoundary_Internal( compIdx, recYuv.Y, recYuv.Pitch, orgYuv.Y, orgYuv.Pitch,
                                              width, height, 0, borders, statData_predeblocked_B[compIdx]);

    for (int i = 0; i < NUM_SAO_BASE_TYPES; i++) {
        m_saoEncodeFilter.m_statData_predeblocked[SAO_Y][i] = statData_predeblocked_R[compIdx][i];
        m_saoEncodeFilter.m_statData_predeblocked[SAO_Y][i] += statData_predeblocked_B[compIdx][i];
    }

} // void H265CU::GetStatisticsCtuSaoPredeblocked( void )

template class H265CU<Ipp8u>;
template class H265CU<Ipp16u>;
template class SaoDecodeFilter<Ipp8u>;
template class SaoDecodeFilter<Ipp16u>;

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
