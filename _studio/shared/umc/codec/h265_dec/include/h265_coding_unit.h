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

#ifndef __H265_CODING_UNIT_H
#define __H265_CODING_UNIT_H

#include <vector>


#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_heap.h"
#include "umc_h265_headers.h"
#include "umc_h265_dec.h"
#include "umc_h265_frame.h"
#include "h265_motion_info.h"
#include "h265_global_rom.h"

namespace UMC_HEVC_DECODER
{

class H265Pattern;
class H265SegmentDecoderMultiThreaded;

class H265CodingUnit
{
public:
    //pointers --------------------------------------------------------------------------------------
    H265DecoderFrame*        m_Frame;          // frame pointer
    H265SliceHeader *        m_SliceHeader;    // slice header pointer

    // conversion of partition index to picture pel position
    Ipp32u *m_rasterToPelX;
    Ipp32u *m_rasterToPelY;

    Ipp32u *m_zscanToRaster;

    Ipp32s                   m_SliceIdx;
    bool                     m_AvailBorder[8];

    //CU description --------------------------------------------------------------------------------
    Ipp32u                    CUAddr;         // CU address in a slice
    Ipp32u                    m_AbsIdxInLCU;    // absolute address in a CU. It's Z scan order
    Ipp32u                    m_CUPelX;         // CU position in a pixel (X)
    Ipp32u                    m_CUPelY;         // CU position in a pixel (Y)
    Ipp32u                    m_NumPartition;   // total number of minimum partitions in a CU

    //CU data ----------------------------------------------------------------------------------------
    bool*                     m_CUTransquantBypass; // array of cu_transquant_bypass flags
    Ipp8u*                    m_TrIdxArray;     // array of transform indices
    H265CoeffsPtrCommon        m_TrCoeffY;       // transformed coefficient buffer (Y)
    H265CoeffsPtrCommon        m_TrCoeffCb;      // transformed coefficient buffer (Cb)
    H265CoeffsPtrCommon        m_TrCoeffCr;      // transformed coefficient buffer (Cr)

protected:
    Ipp8u*                    m_widthArray;     // array of widths
    Ipp8u*                    m_depthArray;     // array of depths

    Ipp8u*                    m_partSizeArray;  // array of partition sizes
    Ipp8u*                    m_predModeArray;  // array of prediction modes
    Ipp8u*                    m_qpArray;        // array of QP values

    Ipp8u*                    m_transformSkip[3];  // array of transform skipping flags
    Ipp8u*                    m_cbf[3];         // array of coded block flags (CBF)

    Ipp8u*                    m_lumaIntraDir;    // array of intra directions (luma)
    Ipp8u*                    m_chromaIntraDir;  // array of intra directions (chroma)

    bool*                    m_IPCMFlag;        // array of intra_pcm flags

public:

    inline Ipp8u GetLumaIntra(Ipp32s partAddr) const
    {
        return m_lumaIntraDir[partAddr];
    }

    inline Ipp8u GetChromaIntra(Ipp32s partAddr) const
    {
        return m_chromaIntraDir[partAddr];
    }

    inline bool GetIPCMFlag(Ipp32s partAddr) const
    {
        return m_IPCMFlag[partAddr];
    }

    inline Ipp8u GetTransformSkip(Ipp32s plane, Ipp32s partAddr) const
    {
        return m_transformSkip[plane][partAddr];
    }

    inline Ipp8u GetCbf(Ipp32s plane, Ipp32s partAddr) const
    {
        return m_cbf[plane][partAddr];
    }

    inline Ipp8u GetQP(Ipp32s partAddr) const
    {
        return m_qpArray[partAddr];
    }

    inline Ipp8u GetWidth(Ipp32s partAddr) const
    {
        return m_widthArray[partAddr];
    }

    inline Ipp8u GetDepth(Ipp32s partAddr) const
    {
        return m_depthArray[partAddr];
    }

    H265PlanePtrYCommon        m_IPCMSampleY;     // PCM sample buffer (Y)
    H265PlanePtrUVCommon       m_IPCMSampleCb;    // PCM sample buffer (Cb)
    H265PlanePtrUVCommon       m_IPCMSampleCr;    // PCM sample buffer (Cr)

    // misc. variables -------------------------------------------------------------------------------------
    Ipp8s                   m_CodedQP;

    bool isLosslessCoded(Ipp32u absPartIdx);

protected:

    Ipp8u* m_cumulativeMemoryPtr;

    // compute scaling factor from POC difference
    Ipp32s GetDistScaleFactor(Ipp32s DiffPocB, Ipp32s DiffPocD);

public:

    H265CodingUnit();
    virtual ~H265CodingUnit();

    // create / destroy / init  / copy -----------------------------------------------------------------------------

    void create (H265FrameCodingData * frameCD);
    void destroy ();

    void initCU (H265SegmentDecoderMultiThreaded* sd, Ipp32u CUAddr);
    void setOutsideCUPart (Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for CU description ------- (only functions with declaration here. simple get/set are removed)
    Ipp32u getSCUAddr();
    void setDepthSubParts (Ipp32u Depth, Ipp32u AbsPartIdx);

    // member functions for CU data ------------- (only functions with declaration here. simple get/set are removed)
    EnumPartSize GetPartitionSize (Ipp32u Idx) const
    {
        return static_cast<EnumPartSize>(m_partSizeArray[Idx]);
    }

    EnumPredMode GetPredictionMode (Ipp32u Idx) const
    {
        return static_cast<EnumPredMode>(m_predModeArray[Idx]);
    }

    void setPartSizeSubParts (EnumPartSize Mode, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCUTransquantBypassSubParts(bool flag, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setPredModeSubParts (EnumPredMode Mode, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setSizeSubParts (Ipp32u Width, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setQPSubParts (Ipp32u QP, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setTrIdxSubParts (Ipp32u TrIdx, Ipp32u AbsPartIdx, Ipp32u Depth);

    Ipp32u getQuadtreeTULog2MinSizeInCU (Ipp32u Idx);

    H265_FORCEINLINE Ipp8u* GetCbf (EnumTextType Type) const
    {
        return m_cbf[g_ConvertTxtTypeToIdx[Type]];
    }

    H265_FORCEINLINE Ipp8u GetCbf(Ipp32u Idx, EnumTextType Type) const
    {
        return m_cbf[g_ConvertTxtTypeToIdx[Type]][Idx];
    }

    H265_FORCEINLINE Ipp8u GetCbf(Ipp32u Idx, EnumTextType Type, Ipp32u TrDepth) const
    {
        return (Ipp8u)((GetCbf(Idx, Type) >> TrDepth ) & 0x1);
    }

    void setCbfSubParts (Ipp32u CbfY, Ipp32u CbfU, Ipp32u CbfV, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setCbfSubParts (Ipp32u m_Cbf, EnumTextType TType, Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for coding tool information (only functions with declaration here. simple get/set are removed)
    template <typename T>
    void setSubPart (T Parameter, T* pBaseLCU, Ipp32u CUAddr, Ipp32u CUDepth, Ipp32u PUIdx);
    void setLumaIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);
    void setChromIntraDirSubParts (Ipp32u Dir, Ipp32u AbsPartIdx, Ipp32u Depth);

    void setTransformSkipSubParts(Ipp32u useTransformSkip, EnumTextType Type, Ipp32u AbsPartIdx, Ipp32u Depth);

    void setIPCMFlagSubParts (bool IpcmFlag, Ipp32u AbsPartIdx, Ipp32u Depth);

    // member functions for accessing partition information -----------------------------------------------------------
    void getPartIndexAndSize (Ipp32u AbsPartIdx, Ipp32u Depth, Ipp32u PartIdx, Ipp32u &PartAddr, Ipp32u &Width, Ipp32u &Height);
    void getPartSize(Ipp32u AbsPartIdx, Ipp32u partIdx, Ipp32s &nPSW, Ipp32s &nPSH);
    Ipp8u getNumPartInter(Ipp32u AbsPartIdx);

    void fillMVPCand (Ipp32u PartIdx, Ipp32u PartAddr, EnumRefPicList RefPicList, Ipp32s RefIdx, AMVPInfo* pInfo);
    bool isDiffMER(Ipp32s xN, Ipp32s yN, Ipp32s xP, Ipp32s yP);

    // member functions for modes ---------------------- (only functions with declaration here. simple get/set are removed)
    bool isBipredRestriction(Ipp32u AbsPartIdx, Ipp32u PartIdx);

    void getAllowedChromaDir (Ipp32u AbsPartIdx, Ipp32u* ModeList);

    // member functions for SBAC context ----------------------------------------------------------------------------------
    Ipp32u getCtxQtCbf (EnumTextType Type, Ipp32u TrDepth);

    // member functions for RD cost storage  ------------(only functions with declaration here. simple get/set are removed)
    Ipp32u getCoefScanIdx(Ipp32u AbsPartIdx, Ipp32u L2Width, bool IsLuma, bool IsIntra);

    void setNDBFilterBlockBorderAvailability(bool independentTileBoundaryEnabled);
    Ipp32s getLastValidPartIdx(Ipp32s AbsPartIdx);
};

struct H265FrameHLDNeighborsInfo
{
    union {
        struct {
            Ipp16u IsAvailable        : 1;  //  1
            Ipp16u IsIntra            : 1;  //  2
            Ipp16u SkipFlag           : 1;  //  3
            Ipp16u Depth              : 3;  //  6
            Ipp16u IntraDir           : 6;  // 12
            Ipp16u IsIPCM             : 1;  // 13
            Ipp16u IsTransquantBypass : 1;  // 14
            Ipp16u IsTrCbfY           : 1;  // 15
            Ipp8u qp;
            Ipp8u TrStart;
        } members;
        Ipp32u data;
    };
};

struct H265PUInfo
{
    H265MVInfo interinfo;
    H265DecoderFrame *refFrame[2];
    Ipp32u PartAddr;
    Ipp32u Width, Height;
};

} // end namespace UMC_HEVC_DECODER

#endif //__H265_CODING_UNIT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
