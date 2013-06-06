/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//    Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_SEGMENT_DECODER_H
#define __UMC_H265_SEGMENT_DECODER_H

#include "umc_h265_dec.h"
#include "umc_h265_dec_tables.h"

#include "umc_h265_thread.h"
#include "umc_h265_frame.h"

#include "h265_prediction.h"

namespace UMC_HEVC_DECODER
{

struct H265SliceHeader;
struct H265FrameHLDNeighborsInfo;
struct H265TUData;
struct H265MotionVector;
struct MVBuffer;
struct SAOParams;
struct AMVPInfo;
class H265Prediction;
class H265TrQuant;
class TaskBroker_H265;

struct H265EdgeData
{
    Ipp8u strength;
    Ipp8u qp;
    Ipp8u deblockP;
    Ipp8u deblockQ;
    Ipp8s tcOffset;
    Ipp8s betaOffset;
};

//
// Class to incapsulate functions, implementing common decoding functional.
//

struct Context
{
    H265DecoderRefPicList::ReferenceInformation *m_pRefPicList[2];
    Ipp32s m_CurMBAddr;

    H265Bitstream *m_pBitStream;                                // (H265Bitstream *) pointer to bit stream object

    Ipp32s mb_width;                                            // (Ipp32s) width in MB
    Ipp32s mb_height;                                           // (Ipp32s) height in MB

    Ipp32s m_MBSkipCount;
    Ipp32s m_QuantPrev;

    H265CoeffsPtrCommon m_pCoeffBlocksWrite;                        // (Ipp16s *) pointer to write buffer
    H265CoeffsPtrCommon m_pCoeffBlocksRead;                         // (Ipp16s *) pointer to read buffer

    Ipp32s bit_depth_luma;
    Ipp32s bit_depth_chroma;
    Ipp32s m_prev_dquant;

    // external data
    const H265PicParamSet *m_pPicParamSet;                      // (const H265PicParamSet *) pointer to current picture parameters sets
    const H265SeqParamSet *m_pSeqParamSet;                      // (const H265SeqParamSet *) pointer to current sequence parameters sets
    H265DecoderFrame *m_pCurrentFrame;                          // (H265DecoderFrame *) pointer to frame being decoded

    H265CoeffsPtrCommon m_pCoefficientsBuffer;
    Ipp32u m_nAllocatedCoefficientsBuffer;
};

STRUCT_DECLSPEC_ALIGN class H265SegmentDecoder : public Context
{
public:
    //h265 functions
    //create internal buffers
    void create(H265SeqParamSet* pSPS);
    //destroy internal buffers
    void destroy();
    void DecodeSAOOneLCU(H265CodingUnit* pCU);
    void parseSaoOneLcuInterleaving(Ipp32s rx,
                                    Ipp32s ry,
                                    SAOParams* pSaoParam,
                                    H265CodingUnit* pcCU,
                                    Ipp32s iCUAddrInSlice,
                                    Ipp32s iCUAddrUpInSlice,
                                    Ipp32s allowMergeLeft,
                                    Ipp32s allowMergeUp);

    void parseSaoMaxUvlc(Ipp32u& val, Ipp32u maxSymbol);
    void parseSaoUflc(Ipp32s length, Ipp32u&  riVal);
    void parseSaoMerge(Ipp32u&  ruiVal);
    void parseSaoTypeIdx(Ipp32u&  ruiVal);
    void parseSaoOffset(SAOLCUParam* psSaoLcuParam, Ipp32u compIdx);

    void ReadUnaryMaxSymbolCABAC(Ipp32u& uVal, Ipp32u CtxIdx, Ipp32s Offset, Ipp32u MaxSymbol);
    void DecodeSplitFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeMergeIndexCABAC(H265CodingUnit* pCU, Ipp32u PartIdx, Ipp32u AbsPartIdx, EnumPartSize CUMode, Ipp8u* InterDirNeighbours, MVBuffer* MVBufferdNeighbours, Ipp32u Depth);
    void DecodeSkipFlagCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeCUTransquantBypassFlag(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ParseMergeIndexCABAC(H265CodingUnit* pCU, Ipp32u& MergeIndex, Ipp32u AbsPartIdx, Ipp32u Depth);
    Ipp32s DecodeMVPIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartAddr, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList, H265MotionVector &MV);
    void ParseMVPIdxCABAC(Ipp32s& MVPIdx);
    void getMVPredAMVP(H265CodingUnit* pCU, Ipp32u PartIdx, Ipp32u PartAddr, EnumRefPicList RefPicList, H265MotionVector& m_MVPred);
    Ipp32s DecodePredModeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodePartSizeCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeIPCMInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodePCMAlignBits();
    void DecodePredInfoCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeIntraDirLumaAngCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeIntraDirChromaCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodePUWiseCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeMergeFlagCABAC(H265CodingUnit* SubCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PUIdx);
    void DecodeInterDirPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx);
    void ParseInterDirCABAC(H265CodingUnit* pCU, Ipp32u &InterDir, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeRefFrmIdxPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList);
    void ParseRefFrmIdxCABAC(H265CodingUnit* pCU, Ipp32s& RefFrmIdx, EnumRefPicList RefList);
    void DecodeMVdPUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, EnumRefPicList RefList);
    void ParseTransformSubdivFlagCABAC(Ipp32u& SubdivFlag, Ipp32u Log2TransformBlockSize);
    void ReadEpExGolombCABAC(Ipp32u& Value, Ipp32u Count);
    void DecodeCoeff(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u Width, Ipp32u Height, bool& CodeDQP);
    void DecodeTransform(H265CodingUnit* pCU, Ipp32u offsetLuma, Ipp32u offsetChroma,
                                             Ipp32u AbsPartIdx, Ipp32u nsAbsPartIdx, Ipp32u Depth, Ipp32u  width, Ipp32u height,
                                             Ipp32u TrIdx, Ipp32u InnerQuadIdx, bool& CodeDQP);
    void ParseQtCbfCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, EnumTextType Type, Ipp32u TrDepth, Ipp32u Depth);
    void ParseQtRootCbfCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& QtRootCbf);
    void DecodeQP(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    void ParseDeltaQPCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReadUnarySymbolCABAC(Ipp32u& Value, Ipp32s ctxIdx, Ipp32s Offset);
    void FinishDecodeCU(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    void DecodeCUCABAC(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u& IsLast);
    bool DecodeSliceEnd(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);

    void ParseLastSignificantXYCABAC(Ipp32u& PosLastX, Ipp32u& PosLastY, Ipp32u width, Ipp32u height, EnumTextType Type, Ipp32u ScanIdx);

    void ParseCoeffNxNCABAC(H265CodingUnit* pCU, H265CoeffsPtrCommon pCoef, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type);

    void ParseTransformSkipFlags(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Width, Ipp32u Height, Ipp32u Depth, EnumTextType Type);

    void ReadCoefRemainExGolombCABAC(Ipp32u &Symbol, Ipp32u &GoRiceParam);

    void ReconstructCU(H265CodingUnit* pCU, H265CodingUnit* pCUCur, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconInter(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void ReconPCM(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodeInterTexture(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void DecodePCMTextureLuma(H265CodingUnit* pCU, Ipp16s PartIdx, H265PlanePtrYCommon pPCM, H265PlanePtrYCommon pReco, Ipp32u Stride, Ipp32u Width, Ipp32u Height);
    void DecodePCMTextureChroma(H265CodingUnit* pCU, Ipp16s PartIdx, H265PlanePtrUVCommon pPCMCb, H265PlanePtrUVCommon pPCMCr, H265PlanePtrUVCommon pReco, Ipp32u Stride, Ipp32u Width, Ipp32u Height);
    void FillPCMBuffer(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    void IntraLumaRecQT(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         H265DecYUVBufferPadded* pRecoYUV,
                         H265DecYUVBufferPadded* pPredYUV,
                         H265DecYUVBufferPadded* pResiYUV);
    void IntraChromaRecQT(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         H265DecYUVBufferPadded* pRecoYUV,
                         H265DecYUVBufferPadded* pPredYUV,
                         H265DecYUVBufferPadded* pResiYUV);
    void IntraRecLumaBlk(H265CodingUnit* pCU,
                         Ipp32u TrDepth,
                         Ipp32u AbsPartIdx,
                         H265DecYUVBufferPadded* pRecoYUV,
                         H265DecYUVBufferPadded* pPredYUV,
                         H265DecYUVBufferPadded* pResiYUV);
    void IntraRecChromaBlkNV12(H265CodingUnit* pCU,
                           Ipp32u TrDepth,
                           Ipp32u AbsPartIdx,
                           H265DecYUVBufferPadded* pRecoYUV,
                           H265DecYUVBufferPadded* pPredYUV,
                           H265DecYUVBufferPadded* pResiYUV);
    void IntraRecChromaBlk(H265CodingUnit* pCU,
                           Ipp32u TrDepth,
                           Ipp32u AbsPartIdx,
                           H265DecYUVBufferPadded* pRecoYUV,
                           H265DecYUVBufferPadded* pPredYUV,
                           H265DecYUVBufferPadded* pResiYUV);
    void IntraRecQT(H265CodingUnit* pCU,
                    Ipp32u TrDepth,
                    Ipp32u AbsPartIdx,
                    H265DecYUVBufferPadded* pRecoYUV,
                    H265DecYUVBufferPadded* pPredYUV,
                    H265DecYUVBufferPadded* pResiYUV);

    void DeblockOneLCU(Ipp32s curLCUAddr, Ipp32s cross);
    void DeblockOneCrossLuma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow,
                             Ipp32s onlyOneUp, Ipp32s onlyOneLeft, Ipp32s cross);
    void DeblockOneCrossChroma(H265CodingUnit* curLCU, Ipp32s curPixelColumn, Ipp32s curPixelRow,
                               Ipp32s onlyOneUp, Ipp32s onlyOneLeft, Ipp32s cross);
    void FilterEdgeLuma(H265EdgeData *edge, H265PlaneYCommon *srcDst, Ipp32s srcDstStride, Ipp32s dir);
    void FilterEdgeChroma(H265EdgeData *edge, H265PlaneUVCommon *srcDst, Ipp32s srcDstStride,
        Ipp32s chromaCbQpOffset, Ipp32s chromaCrQpOffset, Ipp32s dir);
#if (HEVC_OPT_CHANGES & 32)
// ML: OPT: Parameterized 'dir' to make it constant
    template< Ipp32s dir>
    void GetEdgeStrength(H265CodingUnit* pcCUQ, H265EdgeData *edge, Ipp32s curColumn, Ipp32s curRow,
                         Ipp32s crossSliceBoundaryFlag, Ipp32s crossTileBoundaryFlag, Ipp32s tcOffset,
                         Ipp32s betaOffset);
#else
    void GetEdgeStrength(H265CodingUnit* pcCUQ, H265EdgeData *edge, Ipp32s curColumn, Ipp32s curRow,
                         Ipp32s crossSliceBoundaryFlag, Ipp32s crossTileBoundaryFlag, Ipp32s tcOffset,
                         Ipp32s betaOffset, Ipp32s dir);
#endif // HEVC_OPT_CHANGES
    void SetEdges(H265CodingUnit* curLCU, Ipp32s width, Ipp32s height, Ipp32s cross, Ipp32s calculateCurLCU);

    //end of h265 functions

    //h265 members
    H265SampleAdaptiveOffset m_SAO;

    bool m_AlfCtrl;
    Ipp32u m_MaxAlfCtrlDepth;
    H265CodingUnit* m_curCU;
    bool m_DecodeDQPFlag;
    Ipp32u m_MaxDepth; //max number of depth
    H265CodingUnit** m_ppcCU; // CU data array
    H265DecYUVBufferPadded* m_ppcYUVResi; //array of residual buffer
    H265DecYUVBufferPadded* m_ppcYUVReco; //array of prediction & reconstruction buffer

    std::auto_ptr<H265Prediction> m_Prediction;

    H265TrQuant* m_TrQuant;
    Ipp32u m_BakAbsPartIdx;
    Ipp32u m_BakChromaOffset;
    Ipp32u m_bakAbsPartIdxCU;

    H265EdgeData m_edge[9][9][4];

    // Updated after whole CTB is done for initializing external neighbour data in m_CurCTB
    H265FrameHLDNeighborsInfo *m_TopNgbrs;
    H265TUData *m_TopMVInfo;
    // Updated after every PU is processed and MV info is available
    H265FrameHLDNeighborsInfo *m_CurrCTBFlags;
    H265TUData *m_CurrCTB;
    Ipp32s m_CurrCTBStride;

    std::vector<H265FrameHLDNeighborsInfo> m_TopNgbrsHolder;
    std::vector<H265TUData> m_TopMVInfoHolder;
    std::vector<H265TUData> m_CurrCTBHolder;
    std::vector<H265FrameHLDNeighborsInfo> m_CurrCTBFlagsHolder;

    void UpdateNeighborBuffers(H265CodingUnit* pCU, Ipp32u AbsPartIdx);
    void ResetRowBuffer(void);
    void UpdatePUInfo(H265CodingUnit *pCU, Ipp32u PartX, Ipp32u PartY, Ipp32u PartWidth, Ipp32u PartHeight, Ipp8s RefIdx[2], H265MotionVector MV[2]);
    void UpdateCurrCUContext(Ipp32u lastCUAddr, Ipp32u newCUAddr);

    //end of h265 members

    Ipp32s m_iNumber;                                           // (Ipp32s) ordinal number of decoder
    H265Slice *m_pSlice;                                        // (H265Slice *) current slice pointer
    H265SliceHeader *m_pSliceHeader;                      // (H265SliceHeader *) current slice header
    TaskBroker_H265 * m_pTaskBroker;

    // Default constructor
    H265SegmentDecoder(TaskBroker_H265 * pTaskBroker);
    // Destructor
    virtual ~H265SegmentDecoder(void);

    // Initialize object
    virtual UMC::Status Init(Ipp32s iNumber);

    // Decode slice's segment
    virtual UMC::Status ProcessSegment(void) = 0;

    virtual UMC::Status ProcessSlice(Ipp32s nCurMBNumber, Ipp32s &nMBToProcess) = 0;

//protected:
    // Release object
    void Release(void);

    void DeblockFrame(Ipp32s nCurMBNumber, Ipp32s nMacroBlocksToDeblock);

    // Function to de-block partition of macro block row
    virtual void DeblockSegment(Ipp32s iCurMBNumber, Ipp32s iMBToProcess);

    H265CoeffsPtrCommon GetCoefficientsBuffer(Ipp32u nNum = 0);

private:
    Ipp16u *g_SigLastScan_inv[3][4];
    Ipp32u getCtxSplitFlag(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u Depth);
    Ipp32u getCtxSkipFlag(H265CodingUnit *pCU, Ipp32u AbsPartIdx);
    void getIntraDirLumaPredictor(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32s IntraDirPred[]);
    void getInterMergeCandidates(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PUIdx, MVBuffer* MVBufferNeighbours,
        Ipp8u* InterDirNeighbours, Ipp32s &numValidMergeCand, Ipp32s mrgCandIdx);
    void fillMVPCand(H265CodingUnit *pCU, Ipp32u AbsPartIdx, Ipp32u PartIdx, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo);
    // add possible motion vector predictor candidates
    bool AddMVPCand(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr);
    bool AddMVPCandOrder(AMVPInfo* pInfo, EnumRefPicList RefPicList, Ipp32s RefIdx, Ipp32s NeibAddr);
    bool GetColMVP(EnumRefPicList refPicListIdx, Ipp32u PartX, Ipp32u PartY, H265MotionVector& rcMV, Ipp32s RefIdx);
    bool hasEqualMotion(Ipp32s dir1, Ipp32s dir2);

    // we lock the assignment operator to avoid any
    // accasional assignments
    H265SegmentDecoder & operator = (H265SegmentDecoder &)
    {
        return *this;

    } // H265SegmentDecoder & operator = (H265SegmentDecoder &)
};

extern void InitializeSDCreator_ManyBits_H265();

} // namespace UMC_HEVC_DECODER

#endif /* __UMC_H264_SEGMENT_DECODER_H */
#endif // UMC_ENABLE_H264_VIDEO_DECODER
