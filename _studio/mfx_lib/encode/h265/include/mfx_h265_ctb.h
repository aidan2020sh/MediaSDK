//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_CTB_H__
#define __MFX_H265_CTB_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_set.h"
#include "mfx_h265_bitstream.h"
#include "mfx_h265_optimization.h"
#include "mfx_h265_sao_filter.h"
#include "mfx_h265_tables.h"
#include "mfx_h265_paq.h"

using namespace MFX_HEVC_PP;

namespace H265Enc {

//Optimization with Intelligent Content and RD Analysis (ICRA)
#define AMT_ICRA_OPT 
#ifdef AMT_ICRA_OPT
#define AMT_BIREFINE_CONVERGE
#endif

struct H265MV
{
    Ipp16s  mvx;
    Ipp16s  mvy;
};

template <int MAX_NUM_CAND>
struct MvPredInfo
{
    H265MV mvCand[2 * MAX_NUM_CAND];
    Ipp8s  refIdx[2 * MAX_NUM_CAND];
    Ipp32s numCand;
};

struct IntraLumaMode
{
    Ipp8u    mode;      // 0..34
    Ipp32s   numBits;   // estimated number of bits * 256
    Ipp32s   satd;
    CostType cost;      // satd + bitCost or full RD cost
    CostType bitCost;   // numBits * lambda
};

const H265MV MV_ZERO = {};

Ipp32s operator == (const H265MV &mv1, const H265MV &mv2);
Ipp32s operator != (const H265MV &mv1, const H265MV &mv2);
Ipp32s qdiff(const H265MV *mv1, const H265MV *mv2);
Ipp32s qcost(const H265MV *mv);
Ipp32s qdist(const H265MV *mv1, const H265MV *mv2);

struct H265CUData
{
public:
    Ipp8u depth;
    Ipp8u size;
    Ipp8u partSize;
    Ipp8u predMode;
    Ipp8u trIdx;
    Ipp8s qp;
    Ipp8u cbf[3];
    Ipp8u intraLumaDir;
    Ipp8u intraChromaDir;
    Ipp8u interDir;
    Ipp8u mergeIdx;
    Ipp8s mvpIdx[2];
    H265MV mv[2];
    H265MV mvd[2];
    Ipp8s refIdx[2];

    Ipp8u transformSkipFlag[3];
    union {
        struct {
            Ipp8u mergeFlag : 1;
            Ipp8u ipcmFlag : 1;
            Ipp8u transquantBypassFlag : 1;
            Ipp8u skippedFlag : 1;
        } flags;
        Ipp8u _flags;
    };
};

typedef struct {
    Ipp64u cost[4];
    Ipp32u num[4];
} costStat;

struct H265CUPtr {
    H265CUData *ctbData;
    Ipp32s ctbAddr;
    Ipp32s absPartIdx;
};
#define MAX_PU_IN_CTB (64/8*2 * 64/8*2)
#define MAX_PU_CASES_IN_CTB (MAX_PU_IN_CTB*4/3+2)
struct H265MEInfo
{
    Ipp8u posx, posy; // in pix inside LCU
    Ipp8u width, height;
    H265MV MV[2]; // [fwd/bwd]
    Ipp8s refIdx[2];
    Ipp32s absPartIdx;
    Ipp8u interDir;   // INTER_DIR_PRED_LX
    Ipp8u splitMode;
    Ipp8u depth;
};

#define IS_INTRA(data, partIdx) ((data)[partIdx].predMode == MODE_INTRA)

static inline Ipp8u isSkipped (H265CUData *data, Ipp32u partIdx)
{
    return data[partIdx].flags.skippedFlag;
}

#define MAX_TOTAL_DEPTH (MAX_CU_DEPTH+4)

#define CHROMA_SIZE_DIV 2 // 4 - 4:2:0 ; 2 - 4:2:2,4:2:0 ; 1 - 4:4:4,4:2:2,4:2:0

template <class PixType> struct GetHistogramType;
template <> struct GetHistogramType<Ipp8u> { typedef Ipp16u type; };
template <> struct GetHistogramType<Ipp16u> { typedef Ipp32u type; };

template <typename PixType>
class H265CU
{
public:
    typedef typename GetHistogramType<PixType>::type HistType;

    H265VideoParam *m_par;
    H265Frame      *m_currFrame;
    Ipp32u          m_ctbAddr;           ///< CU address in a slice
    Ipp32u          m_absIdxInLcu;      ///< absolute address in a CU. It's Z scan order
    Ipp32u          m_ctbPelX;           ///< CU position in a pixel (X)
    Ipp32u          m_ctbPelY;           ///< CU position in a pixel (Y)
    Ipp32u          m_numPartition;     ///< total number of minimum partitions in a CU
    __ALIGN32 CoeffsType    m_residualsY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_residualsU[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_residualsV[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 PixType       m_predIntraAll[35*32*32];
    __ALIGN32 PixType       m_srcTr[32*32]; // transposed src block

    // new
    // working/final coeffs
    __ALIGN32 CoeffsType    m_coeffWorkY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_coeffWorkU[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_coeffWorkV[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    // temporarily best coeffs for lower depths
    __ALIGN32 CoeffsType    m_coeffStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];     // (+1 for Intra_NxN)
    __ALIGN32 CoeffsType    m_coeffStoredU[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV]; // (+1 for Intra Chroma, temp until code is cleaned up)
    __ALIGN32 CoeffsType    m_coeffStoredV[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV]; // (+1 for Intra Chroma, temp until code is cleaned up)
    // inter reconstruct pixels
    __ALIGN32 PixType       m_recStoredY[5+1][MAX_CU_SIZE * MAX_CU_SIZE];       // (+1 for Intra_NxN)
    __ALIGN32 PixType       m_recStoredC[5+1][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];   // (+1 for Intra Chroma, temp until code is cleaned up)
    __ALIGN32 PixType       m_interRecWorkY[MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interRecWorkC[MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    // inter prediction pixels
    __ALIGN32 PixType       m_interPredBufsY[2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 PixType       m_interPredBufsC[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV * 2];
    // inter residual
    __ALIGN32 CoeffsType    m_interResidBufsY[2][MAX_CU_SIZE * MAX_CU_SIZE];
    __ALIGN32 CoeffsType    m_interResidBufsU[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];
    __ALIGN32 CoeffsType    m_interResidBufsV[2][MAX_CU_SIZE * MAX_CU_SIZE / CHROMA_SIZE_DIV];


    // buffer contains the result of 1-ref interpolation between 2 PredInterUni calls
    __ALIGN32 Ipp16s    m_interpBuf[MAX_CU_SIZE*MAX_CU_SIZE];

    PixType    *m_interPredY;
    PixType    *m_interPredC;
    CoeffsType *m_interResidY;
    CoeffsType *m_interResidU;
    CoeffsType *m_interResidV;
    PixType    *m_interPredBestY;
    PixType    *m_interPredBestC;
    CoeffsType *m_interResidBestY;
    CoeffsType *m_interResidBestU;
    CoeffsType *m_interResidBestV;

    // stored CABAC contexts for each CU level (temp best)
    CABAC_CONTEXT_H265  m_ctxStored[5+1][NUM_CABAC_CONTEXT]; // (+1 for Intra_NxN)

    CostType m_costStored[5+1]; // stored RD costs (+1 for Intra_NxN)
    CostType m_costCurr;        // current RD cost

    Ipp8u         m_inNeighborFlags[4*MAX_CU_SIZE+1];
    Ipp8u         m_outNeighborFlags[4*MAX_CU_SIZE+1];
    H265CUData*   m_above;          ///< pointer of above CU
    H265CUData*   m_left;           ///< pointer of left CU
    H265CUData*   m_aboveLeft;      ///< pointer of above-left CU
    H265CUData*   m_aboveRight;     ///< pointer of above-right CU

    // merge and AMVP candidates shared between function calls for one CU depth, one part mode and all PUs (1, 2, or 4)
    MvPredInfo<5> m_mergeCand[4];
    MvPredInfo<2> m_amvpCand[4][2 * MAX_NUM_REF_IDX];

    Ipp32s m_aboveAddr;
    Ipp32s m_leftAddr;
    Ipp32s m_aboveLeftAddr;
    Ipp32s m_aboveRightAddr;
    Ipp8s *m_colFrmRefFramesTbList[2];
    bool  *m_colFrmRefIsLongTerm[2];
    H265EdgeData m_edge[9][9][4];

    Ipp32u m_bakAbsPartIdxCu;

    EnumIntraAngMode m_cuIntraAngMode;
    Ipp8u            m_mpm[3];          // list of most probable intra modes, shared within CheckIntraLuma
    Ipp8u            m_mpmSorted[3];    // mpm sorted as in 8.4.2.4

    // aya - may be used late to speed up SAD calculation
    //__ALIGN32 Ipp8u m_src_aligned_block[MAX_CU_SIZE*MAX_CU_SIZE];

    PixType *m_ySrc;
    PixType *m_uvSrc;
    Ipp32s m_pitchSrcLuma;
    Ipp32s m_pitchSrcChroma;
    PixType *m_yRec;
    PixType *m_uvRec;
    Ipp32s m_pitchRecLuma;
    Ipp32s m_pitchRecChroma;

    static const Ipp32s NUM_DATA_STORED_LAYERS = 5 + 1;
    H265CUData *m_data; // 1 layer,
    H265CUData *m_dataStored; // depth array, current best for CU

    H265CUData* m_ctbData;//all data

    __ALIGN32 HistType m_hist4[40 * MAX_CU_SIZE / 4 * MAX_CU_SIZE / 4];
    __ALIGN32 HistType m_hist8[40 * MAX_CU_SIZE / 8 * MAX_CU_SIZE / 8];
    bool m_isHistBuilt;

    H265BsFake *m_bsf;

    Ipp8u m_rdOptFlag;
    Ipp64f m_rdLambda;
    Ipp64f m_rdLambdaInter;
    Ipp64f m_rdLambdaInterMv;
    Ipp64f m_rdLambdaSqrt;
    Ipp64f m_ChromaDistWeight;

    H265Slice *m_cslice;
    Ipp8u m_adaptMinDepth;      // min CU depth from collocated CUs
    Ipp8u m_adaptMaxDepth;      // max CU depth from Projected CUs
    Ipp32s HorMax;              // MV common limits in CU
    Ipp32s HorMin;
    Ipp32s VerMax;
    Ipp32s VerMin;
    const Ipp8u *m_logMvCostTable;
    costStat *m_costStat;

    SaoEncodeFilter m_saoEncodeFilter;

#if defined(AMT_ICRA_OPT)
    Ipp32s  m_lcuCs[(MAX_CU_SIZE/4)*(MAX_CU_SIZE/4)];
    Ipp32s  m_lcuRs[(MAX_CU_SIZE/4)*(MAX_CU_SIZE/4)];
    Ipp32s  m_SCid;
    Ipp32f  m_SCpp;
    Ipp32s  m_STC;
    Ipp32s  m_mvdMax;
    Ipp32s  m_mvdCost;
#endif

    inline bool  IsIntra(Ipp32u partIdx)
    { return m_data[partIdx].predMode == MODE_INTRA; }

    inline Ipp8u GetTransformSkip(Ipp32u idx,EnumTextType type)
    { return m_data[idx].transformSkipFlag[h265_type2idx[type]];}

    inline Ipp8u GetCbf(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    { return (Ipp8u)( ( m_data[idx].cbf[h265_type2idx[type]] >> trDepth ) & 0x1 ); }

    inline void SetCbfZero(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    {  m_data[idx].cbf[h265_type2idx[type]] &= ~(1 << trDepth); }

    inline void SetCbfOne(Ipp32u idx, EnumTextType type, Ipp32u trDepth )
    {  m_data[idx].cbf[h265_type2idx[type]] |= (1 << trDepth); }

    Ipp8u GetQtRootCbf(Ipp32u idx);

    void GetPuLeft(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true
                   //,Ipp32s enforceDependentSliceRestriction = true
                   //,Ipp32s enforceTileRestriction = true
                   );

    void GetPuAbove(H265CUPtr *cu, Ipp32u currPartUnitIdx, Ipp32s enforceSliceRestriction = true,
//                    Ipp32s enforceDependentSliceRestriction = true, Ipp32s motionDataCompresssion = false,
                    Ipp32s planarAtLcuBoundary = false
//                    , Ipp32s enforceTileRestriction = true
                    );

    bool GetTempMvPred(const H265CUData *currPb, Ipp32s xPb, Ipp32s yPb, Ipp32s nPbW, Ipp32s nPbH,
                       Ipp32s listIdx, Ipp32s refIdx, H265MV *mvLxCol);

    bool GetColMv(const H265CUData *currPb, Ipp32s listIdxCurr, Ipp32s refIdxCurr,
                  const H265Frame *colPic, const H265CUData *colPb, H265MV *mvLxCol);

    H265CUData *GetNeighbour(Ipp32s &neighbourBlockZScanIdx, Ipp32s neighbourBlockColumn,
                             Ipp32s neighbourBlockRow, Ipp32s curBlockZScanIdx,
                             bool isNeedTocheckCurLCU);

    bool AddMvpCand(MvPredInfo<2> *info, H265CUData *data, Ipp32s blockZScanIdx, Ipp32s refPicListIdx,
                    Ipp32s refIdx, bool order);

    void GetMvpCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s refPicListIdx, Ipp32s refIdx,
                    Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize, MvPredInfo<2> *info);

    void GetMergeCand(Ipp32s topLeftCUBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx, Ipp32s cuSize,
                      MvPredInfo<5> *mergeInfo);

    void GetAmvpCand(Ipp32s topLeftBlockZScanIdx, Ipp32s partMode, Ipp32s partIdx,
                     MvPredInfo<2> amvpInfo[2 * MAX_NUM_REF_IDX]);

    void SetupMvPredictionInfo(Ipp32s blockZScanIdx, Ipp32s partMode, Ipp32s curPUidx);

    void GetPartOffsetAndSize(Ipp32s idx, Ipp32s partMode, Ipp32s cuSize, Ipp32s &partX,
                              Ipp32s &partY, Ipp32s &partWidth, Ipp32s &partHeight);

    bool CheckIdenticalMotion(const Ipp8s refIdx[2], const H265MV mvs[2]) const;

    Ipp32s ClipMV(H265MV &rcMv) const; // returns 1 if changed, otherwise 0

    H265CUData *GetQpMinCuLeft(Ipp32u &uiLPartUnitIdx, Ipp32u uiCurrAbsIdxInLCU,
                               bool bEnforceSliceRestriction = true,
                               bool bEnforceDependentSliceRestriction = true);

    H265CUData *GetQpMinCuAbove(Ipp32u &aPartUnitIdx, Ipp32u currAbsIdxInLCU,
                                bool enforceSliceRestriction = true,
                                bool enforceDependentSliceRestriction = true);

    Ipp8s GetRefQp(Ipp32u uiCurrAbsIdxInLCU);
    void SetQpSubParts(int qp, int absPartIdx, int depth);
    void SetQpSubCUs(int qp, int absPartIdx, int depth, bool &foundNonZeroCbf);
    void CheckDeltaQp(void);

    void ConvertTransIdx(Ipp32u trIdx, Ipp32u &lumaTrMode, Ipp32u &chromaTrMode)  {
        lumaTrMode   = trIdx;
        chromaTrMode = trIdx;
        return;
    }

    void GetAllowedChromaDir(Ipp32s absPartIdx, Ipp8u *modeList);

    Ipp32s GetIntradirLumaPred(Ipp32s absPartIdx, Ipp8u *intraDirPred);

    Ipp32u GetCtxSplitFlag(Ipp32s absPartIdx, Ipp32u depth);

    Ipp32u GetCtxQtCbf(EnumTextType type, Ipp32u trDepth) const;

    Ipp32u GetTransformIdx(Ipp32s absPartIdx) { return (Ipp32u)m_data[absPartIdx].trIdx; }

    Ipp32u GetCtxSkipFlag(Ipp32s absPartIdx);

    Ipp32u GetCtxInterDir(Ipp32s absPartIdx);

    Ipp32u GetCoefScanIdx(Ipp32s absPartIdx, Ipp32u log2Width, Ipp32s bIsLuma, Ipp32s bIsIntra) const;

    template <class H265Bs>
    void CodeCoeffNxN(H265Bs *bs, H265CU *pCU, CoeffsType *coeffs, Ipp32s absPartIdx, Ipp32u width, EnumTextType type);

    template <class H265Bs>
    void PutTransform(H265Bs *bs, Ipp32u offsetLuma, Ipp32u offsetChroma, Ipp32s absPartIdx,
                      Ipp32u absTuPartIdx, Ipp32u depth, Ipp32u width, Ipp32u trIdx,
                      Ipp8u& codeDqp, Ipp8u splitFlagOnly = 0);

    template <class H265Bs>
    void EncodeSao(H265Bs *bs, Ipp32s absPartIdx, Ipp32s depth, Ipp8u rdMode,
                   SaoCtuParam &saoBlkParam, bool leftMergeAvail, bool aboveMergeAvail);

    template <class H265Bs>
    void EncodeCU(H265Bs *bs, Ipp32s absPartIdx, Ipp32s depth, Ipp8u rdMode = 0);

    void UpdateCuQp(void);

    template <class H265Bs>
    void EncodeCoeff(H265Bs *bs, Ipp32s absPartIdx, Ipp32u depth, Ipp32u width,
                     Ipp8u &codeDqp);

    template <class H265Bs>
    void CodeIntradirLumaAng(H265Bs *bs, Ipp32s absPartIdx, Ipp8u multiple);

    Ipp32s GetIntraLumaModeCost(Ipp8u mode, const CABAC_CONTEXT_H265 ctx); // don't update m_bsf's contexts

    Ipp32s GetIntraLumaModeCost(Ipp8u mode); // update cabac m_bsf's contexts

    CostType GetIntraChromaModeCost(Ipp32s absPartIdx);

    CostType GetTransformSubdivFlagCost(Ipp32s depth, Ipp32s trDepth);
    
    CostType GetCuSplitFlagCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);

    CostType GetCuModesCost(H265CUData *data, Ipp32s absPartIdx, Ipp32s depth);

    template <EnumTextType PLANE_TYPE>
    void PredInterUni(Ipp32s puX, Ipp32s puY, Ipp32s puW, Ipp32s puH, Ipp32s listIdx,
                      const Ipp8s refIdx[2], const H265MV mvs[2], PixType *dst, Ipp32s dstPitch,
                      Ipp32s isBiPred, MFX_HEVC_PP::EnumAddAverageType eAddAverage, Ipp32s isFast);

    template <EnumTextType PLANE_TYPE>
    void InterPredCu(Ipp32s absPartIdx, Ipp8u depth, PixType *dst, Ipp32s pitchDst);

    void IntraPred(Ipp32s absPartIdx, Ipp8u depth);

    void IntraPredTu(Ipp32s absPartIdx, Ipp32u idx422, Ipp32s width, Ipp32s predMode, Ipp8u isLuma);

    void GetAngModesFromHistogram(Ipp32s xPu, Ipp32s yPu, Ipp32s puSize, Ipp8s *modes, Ipp32s numModes);

    Ipp8u GetTrSplitMode(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, Ipp8u partSize, Ipp8u strict = 1);

    void TransformInv(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra, Ipp8u bitDepth);

    void TransformInv2(void *dst, Ipp32s pitch_dst, Ipp32s offset, Ipp32s width, Ipp8u isLuma,
                       Ipp8u isIntra, Ipp8u bitDepth);

    void TransformFwd(Ipp32s offset, Ipp32s width, Ipp8u isLuma, Ipp8u isIntra);

    void GetInitAvailablity();

    void EncAndRecLuma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost);

    void EncAndRecChroma(Ipp32s absPartIdx, Ipp32s offset, Ipp8u depth, CostType *cost);

    void EncAndRecLumaTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, CostType *cost,
                         Ipp8u costPredFlag, IntraPredOpt predOpt);

    void EncAndRecChromaTu(Ipp32s absPartIdx, Ipp32s idx422, Ipp32s offset, Ipp32s width, CostType *cost,
                           IntraPredOpt pred_opt);

    void QuantInvTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s isLuma);

    void QuantFwdTu(Ipp32s absPartIdx, Ipp32s offset, Ipp32s width, Ipp32s qp, Ipp32s isLuma);

    void Deblock();

    void DeblockOneCrossLuma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void DeblockOneCrossChroma(Ipp32s curPixelColumn, Ipp32s curPixelRow);

    void SetEdges(Ipp32s width, Ipp32s height);

    void GetEdgeStrength(H265CUPtr *pcCUQ, H265EdgeData *edge, Ipp32s curPixelColumn,
                         Ipp32s curPixelRow, Ipp32s crossSliceBoundaryFlag,
                         Ipp32s crossTileBoundaryFlag, Ipp32s tcOffset, Ipp32s betaOffset,
                         Ipp32s dir);

    void EstimateCtuSao(H265BsFake *bs, SaoCtuParam *saoParam, SaoCtuParam *saoParam_TotalFrame,
                        const MFX_HEVC_PP::CTBBorders &borders, const Ipp8u *slice_ids);

    void GetStatisticsCtuSaoPredeblocked(const MFX_HEVC_PP::CTBBorders &borders);

    void FillSubPart(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trIdx, Ipp8u partSize, Ipp8u lumaDir,
                     Ipp8u qp);

    void FillSubPartIntraLumaDir(Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth, Ipp8u lumaDir);

    void CopySubPartTo(H265CUData *dataCopy, Ipp32s absPartIdx, Ipp8u depthCu, Ipp8u trDepth);

    void CalcCostLuma(Ipp32s absPartIdx, Ipp8u depth, Ipp8u trDepth, CostOpt costOpt);

    //CostType CalcCostSkipExperimental(Ipp32s absPartIdx, Ipp8u depth);

    Ipp32s CheckMergeCand(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand,
                          Ipp32s useHadamard, Ipp32s *mergeCandIdx);

    void RefineBiPred(const H265MEInfo *meInfo, const Ipp8s refIdxs[2], Ipp32s curPUidx,
                      H265MV mvs[2], Ipp32s *cost, Ipp32s *mvCost);

    void MeCu(Ipp32s absPartIdx, Ipp8u depth);

    void CheckMerge2Nx2N(Ipp32s absPartIdx, Ipp8u depth);

    void CheckInter(Ipp32s absPartIdx, Ipp8u depth, Ipp32s partMode, const H265MEInfo *meInfo2Nx2N);

    void CheckIntra(Ipp32s absPartIdx, Ipp32s depth);

    void CheckIntraLuma(Ipp32s absPartIdx, Ipp32s depth);

    void CheckIntraChroma(Ipp32s absPartIdx, Ipp32s depth);

    void FilterIntraPredPels(const PixType *predPel, PixType *predPelFilt, Ipp32s width);

    Ipp32s InitIntraLumaModes(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                              IntraLumaMode *modes);

    Ipp32s FilterIntraLumaModesBySatd(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                      IntraLumaMode *modes, Ipp32s numModes);

    Ipp32s FilterIntraLumaModesByRdoTr0(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                        IntraLumaMode *modes, Ipp32s numModes);

    Ipp32s FilterIntraLumaModesByRdoTrAll(Ipp32s absPartIdx, Ipp32s depth, Ipp32s trDepth,
                                          IntraLumaMode *modes, Ipp32s numModes);
#ifdef AMT_ICRA_OPT
    Ipp32s MePu(H265MEInfo *meInfos, Ipp32s partIdx, bool doME=true);
    Ipp32s MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx);
#else
    void MePu(H265MEInfo *meInfos, Ipp32s partIdx);
    void MePuGacc(H265MEInfo *meInfos, Ipp32s partIdx);
#endif

    Ipp32s PuCost(H265MEInfo *meInfo);

    bool CheckGpuIntraCost(Ipp32s absPartIdx, Ipp32s depth) const;

    void MeIntPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const H265Frame *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeIntPelFullSearch(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo,
                            const H265Frame *ref, H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeIntPelLog(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const H265Frame *ref,
                     H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeSubPel(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const H265Frame *ref,
                  H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void AddMvCost(const MvPredInfo<2> *predInfo, Ipp32s log2Step, const Ipp32s *dists, H265MV *mv,
                   Ipp32s *costBest, Ipp32s *mvCostBest) const;

    void MeSubPelBatchedBox(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const H265Frame *ref,
                            H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void MeSubPelBatchedDia(const H265MEInfo *meInfo, const MvPredInfo<2> *predInfo, const H265Frame *ref,
                            H265MV *mv, Ipp32s *cost, Ipp32s *mvCost) const;

    void CuCost(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo* bestInfo);

    void TuGetSplitInter(Ipp32s absPartIdx, Ipp8u trDepth, Ipp8u trDepthMax, CostType *cost);

    void TuMaxSplitInter(Ipp32s absPartIdx, Ipp8u trIdxMax);

    void DetailsXY(H265MEInfo *meInfo) const;

    void MeInterpolate(const H265MEInfo* meInfo, const H265MV *MV, PixType *in_pSrc,
                       Ipp32s in_SrcPitch, PixType *buf, Ipp32s buf_pitch, Ipp32s isFast) const;

    Ipp32s MatchingMetricPu(const PixType *src, const H265MEInfo* meInfo, const H265MV* mv,
                            const H265Frame *refPic, Ipp32s useHadamard) const;

    Ipp32s MatchingMetricBipredPu(const PixType *src, const H265MEInfo *meInfo, const Ipp8s refIdx[2],
                                  const H265MV mvs[2], Ipp32s useHadamard);

    Ipp32s MvCost1RefLog(H265MV mv, const MvPredInfo<2> *predInfo) const;

    Ipp32s MvCost1RefLog(Ipp16s mvx, Ipp16s mvy, const MvPredInfo<2> *predInfo) const;

    void InitCu(H265VideoParam *_par, H265CUData *_data, H265CUData *_dataTemp, Ipp32s cuAddr,
                PixType *_y, PixType *_uv, Ipp32s _pitch_luma, Ipp32s _pitch_chroma, H265Frame *currFrame, H265BsFake *_bsf,
                H265Slice *cslice, Ipp32s initializeDataFlag, const Ipp8u *logMvCostTable);

#if defined(AMT_ICRA_OPT)
    //void CalcRsCs(void);
    void GetSpatialComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth);
    Ipp32s GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth);
    Ipp32s GetSpatioTemporalComplexity(Ipp32s absPartIdx, Ipp32s depth, Ipp32s partAddr, Ipp32s partDepth, Ipp32s& scVal);
    Ipp8u EncInterLumaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width);
    void EncInterChromaTuGetBaseCBF(Ipp32u absPartIdx, Ipp32s offset, Ipp32s width, Ipp8u *nz);
    bool TuMaxSplitInterHasNonZeroCoeff(Ipp32u absPartIdx, Ipp8u trIdxMax);
    bool tryIntraICRA(Ipp32s absPartIdx, Ipp32s depth);
    bool tryIntraRD(Ipp32s absPartIdx, Ipp32s depth, IntraLumaMode *modes);
    H265CUData* GetCuDataXY(Ipp32s x, Ipp32s y, H265Frame *ref);
    void GetProjectedDepth(Ipp32s absPartIdx, Ipp32s depth, Ipp8u splitMode);
    void FastCheckAMP(Ipp32s absPartIdx, Ipp8u depth, const H265MEInfo *meInfo2Nx2N);
    void CheckSkipCandFullRD(const H265MEInfo *meInfo, const MvPredInfo<5> *mergeCand, Ipp32s *mergeCandIdx);
#endif
    CostType ModeDecision(Ipp32s absPartIdx, Ipp8u depth);

    bool getdQPFlag           ()                        { return m_bEncodeDQP;        }
    void setdQPFlag           ( bool b )                { m_bEncodeDQP = b;           }
    int GetCalqDeltaQp(TAdapQP* sliceAQP, Ipp64f sliceLambda);

    bool HaveChromaRec() const;

    H265CUData *StoredCuData(Ipp32s depth) const;

    void SaveFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadFinalCuDecision(Ipp32s absPartIdx, Ipp32s depth);
    const H265CUData *GetBestCuDecision(Ipp32s absPartIdx, Ipp32s depth) const;

    void SaveInterTuDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadInterTuDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadIntraLumaDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth);
    void LoadIntraChromaDecision(Ipp32s absPartIdx, Ipp32s depth);

    void SaveBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth);
    void LoadBestInterPredAndResid(Ipp32s absPartIdx, Ipp32s depth);

    Ipp8u GetAdaptiveMinDepth() const;

private:
    Ipp32s m_isRdoq;
    bool m_bEncodeDQP;

    // random generation code, for development purposes
    //void FillRandom(Ipp32s absPartIdx, Ipp8u depth);
    //void FillZero(Ipp32s absPartIdx, Ipp8u depth);
};

template <class H265Bs>
void CodeSaoCtbOffsetParam(H265Bs *bs, int compIdx, SaoOffsetParam& ctbParam, bool sliceEnabled);

template <class H265Bs>
void CodeSaoCtbParam(H265Bs *bs, SaoCtuParam &saoBlkParam, bool *sliceEnabled, bool leftMergeAvail,
                     bool aboveMergeAvail, bool onlyEstMergeInfo);

Ipp32s tuHad(const Ipp8u *src, Ipp32s pitch_src, const Ipp8u *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);
Ipp32s tuHad(const Ipp16u *src, Ipp32s pitch_src, const Ipp16u *rec, Ipp32s pitch_rec,
             Ipp32s width, Ipp32s height);

Ipp32u GetQuadtreeTuLog2MinSizeInCu(const H265VideoParam *par, Ipp32u log2CbSize,
                                    Ipp8u partSize, Ipp8u predMode);

void GetPartAddr(Ipp32s uPartIdx, Ipp32s partMode, Ipp32s m_NumPartition, Ipp32s &partAddr);


template <class H265CuBase>
Ipp32s GetLastValidPartIdx(H265CuBase* cu, Ipp32s absPartIdx);

template <class H265CuBase>
Ipp8s GetLastCodedQP(H265CuBase* cu, Ipp32s absPartIdx);

Ipp32s GetLumaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch);

Ipp32s GetChromaOffset(const H265VideoParam *par, Ipp32s absPartIdx, Ipp32s pitch);

void CopySubPartTo_(H265CUData *dst, const H265CUData *src, Ipp32s absPartIdx, Ipp32u numParts);

void FillSubPartIntraCuModes_(H265CUData *data, Ipp32s numParts, Ipp8u cuWidth, Ipp8u cuDepth, Ipp8u partMode);
void FillSubPartIntraPartMode_(H265CUData *data, Ipp32s numParts, Ipp8u partMode);
void FillSubPartCuQp_(H265CUData *data, Ipp32s numParts, Ipp8u qp);
void FillSubPartIntraPredModeY_(H265CUData *data, Ipp32s numParts, Ipp8u mode);
void FillSubPartIntraPredModeC_(H265CUData *data, Ipp32s numParts, Ipp8u mode);
void FillSubPartTrDepth_(H265CUData *data, Ipp32s numParts, Ipp8u trDepth);
void FillSubPartSkipFlag_(H265CUData *data, Ipp32s numParts, Ipp8u skipFlag);

template <size_t COMP_IDX> void SetCbfBit(H265CUData *data, Ipp32u trDepth)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] |= (1 << trDepth);
}
template <size_t COMP_IDX> void SetCbf(H265CUData *data, Ipp32u trDepth)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] = (1 << trDepth);
}
template <size_t COMP_IDX> void ResetCbf(H265CUData *data)
{
    VM_ASSERT(COMP_IDX <= 2);
    data->cbf[COMP_IDX] = 0;
}

template<class T>
Ipp32u CheckSum(const T *buf, Ipp32s size, Ipp32u initialSum = 0)
{ return CheckSum((const Ipp8u *)buf, sizeof(T) * size, initialSum); }

template<class T>
Ipp32u CheckSum(const T *buf, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32u initialSum = 0)
{ return CheckSum((const Ipp8u *)buf, sizeof(T) * pitch, sizeof(T) * width, height, initialSum); }

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s size, Ipp32u initialSum);

template<>
Ipp32u CheckSum<Ipp8u>(const Ipp8u *buf, Ipp32s pitch, Ipp32s width, Ipp32s height, Ipp32u initialSum);

Ipp32u CheckSumCabacCtx(const Ipp8u buf[188]);

Ipp32u CheckSumCabacCtxChroma(const Ipp8u buf[188]);

} // namespace

#endif // __MFX_H265_CTB_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
