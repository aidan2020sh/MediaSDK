//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2013 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_ENC_H__
#define __MFX_H265_ENC_H__


struct H265VideoParam {
// preset
    Ipp32u SourceWidth;
    Ipp32u SourceHeight;
    Ipp32u Log2MaxCUSize;
    Ipp32s MaxCUDepth;
    Ipp32u QuadtreeTULog2MaxSize;
    Ipp32u QuadtreeTULog2MinSize;
    Ipp32u QuadtreeTUMaxDepthIntra;
    Ipp32u QuadtreeTUMaxDepthInter;
    Ipp8s  QPI;
    Ipp8s  QPP;
    Ipp8s  QPB;
    Ipp8u  AMPFlag;
    Ipp8u  TMVPFlag;

    Ipp32s NumSlices;
    Ipp8u  AnalyseFlags;
    Ipp32s GopPicSize;
    Ipp32s GopRefDist;
    Ipp32s IdrInterval;

    Ipp32u NumRefFrames;
    Ipp32u NumRefToStartCodeBSlice;
    Ipp8u  TreatBAsReference;
    Ipp8u MaxRefIdxL0;
    Ipp8u MaxRefIdxL1;
    Ipp8u MaxBRefIdxL0;

    Ipp8u SplitThresholdStrengthCU;
    Ipp8u SplitThresholdStrengthTU;
    Ipp8u num_cand_1[8];
    Ipp8u num_cand_2[8];

    Ipp8u SBHFlag;  // Sign Bit Hiding
    Ipp8u RDOQFlag; // RDO Quantization
    Ipp8u WPPFlag; // Wavefront
    Ipp32u num_threads;

// derived
    Ipp32u Width;
    Ipp32u Height;
    Ipp32u CropRight;
    Ipp32u CropBottom;

    Ipp32s AddCUDepth;
    Ipp32u MaxCUSize;
    Ipp32u MinCUSize;
    Ipp32u MinTUSize;
    Ipp32u MaxTrSize;
    Ipp32u Log2NumPartInCU;
    Ipp32u NumPartInCU;
    Ipp32u NumPartInCUSize;
    Ipp32u NumMinTUInMaxCU;
    Ipp32u PicWidthInCtbs;
    Ipp32u PicHeightInCtbs;
    Ipp32u PicWidthInMinCbs;
    Ipp32u PicHeightInMinCbs;
    Ipp32u Log2MinTUSize;
    Ipp8u  AMPAcc[MAX_CU_DEPTH];
    Ipp64f cu_split_threshold_cu[MAX_TOTAL_DEPTH];
    Ipp64f cu_split_threshold_tu[MAX_TOTAL_DEPTH];
    Ipp32s MaxTotalDepth;

    Ipp8u UseDQP;
    Ipp32u MaxCuDQPDepth;
    Ipp32u MinCuDQPSize;
    Ipp8s QPIChroma;
    Ipp8s QPPChroma;
    Ipp8s QPBChroma;
    Ipp8s QP;
    Ipp8s QPChroma;

    H265SeqParameterSet *csps;
    H265PicParameterSet *cpps;
    EncoderRefPicList *m_pRefPicList;
    Ipp8u *m_slice_ids;
};

struct H265ShortTermRefPicSet
{
    Ipp8u  inter_ref_pic_set_prediction_flag;
    Ipp32s delta_idx;
    Ipp8u  delta_rps_sign;
    Ipp32s abs_delta_rps;
    Ipp8u  use_delta_flag[MAX_NUM_REF_FRAMES];
    Ipp8u  num_negative_pics;
    Ipp8u  num_positive_pics;
    Ipp32s delta_poc[MAX_NUM_REF_FRAMES]; /* negative+positive */
    Ipp8u  used_by_curr_pic_flag[MAX_NUM_REF_FRAMES]; /* negative+positive */
};

class H265Encoder {
public:
    MFXVideoENCODEH265 *mfx_video_encode_h265_ptr;

    H265BsReal *bs;
    H265BsFake *bsf;
    Ipp8u *memBuf;
    Ipp32s m_frameCountEncoded;
    H265CU *cu;

    H265ProfileLevelSet m_profile_level;
    H265VidParameterSet m_vps;
    H265SeqParameterSet m_sps;
    H265PicParameterSet m_pps;
    H265VideoParam m_videoParam;
    H265Slice *m_slices;
    H265CUData *data_temp;
    Ipp32u data_temp_size;
    Ipp8u *m_slice_ids;
    H265EncoderRowInfo *m_row_info;

    EncoderRefPicList *m_pRefPicList;
    Ipp32u m_numShortTermRefPicSets;
    Ipp32s m_PicOrderCnt_Accu; // Accumulator to compensate POC resets on IDR frames.
    Ipp32u m_PicOrderCnt;

    H265FrameList m_cpb;
    H265FrameList m_dpb;
    H265Frame *m_pCurrentFrame;
    H265Frame *m_pLastFrame;     // ptr to last frame
    H265Frame *m_pReconstructFrame;

    Ipp32s profile_frequency;
    Ipp32s m_iProfileIndex;
    Ipp32u *eFrameType;

    Ipp8u m_bMakeNextFrameKey;
    Ipp8u m_bMakeNextFrameIDR;
    Ipp32s m_uIntraFrameInterval;
    Ipp32s m_uIDRFrameInterval;
    Ipp32s m_l1_cnt_to_start_B;

    Ipp8u m_isBpyramid;
    H265ShortTermRefPicSet m_ShortRefPicSet[66];

    volatile Ipp32u m_incRow;
    CABAC_CONTEXT_H265 *m_context_array_wpp;

    H265BRC *m_brc;

    H265Encoder() {
        memBuf = NULL; bs = NULL; bsf = NULL;
        data_temp = NULL;
        eFrameType = NULL;
        m_slices = NULL;
        m_slice_ids = NULL;
        m_row_info = NULL;
        m_pCurrentFrame = m_pLastFrame = m_pReconstructFrame = NULL;
        m_pRefPicList = NULL;
        m_brc = NULL;
        m_context_array_wpp = NULL;
    }
    ~H265Encoder() {};
///////////////
    // ------ bitstream
    mfxStatus WriteEndOfSequence(mfxBitstream *bs);
    mfxStatus WriteEndOfStream(mfxBitstream *bs);
    mfxStatus WriteFillerData(mfxBitstream *bs, mfxI32 num);
    void PutProfileLevel(H265BsReal *bs_, Ipp8u profile_present_flag, Ipp32s max_sub_layers);
    mfxStatus PutVPS(H265BsReal *bs_);
    mfxStatus PutSPS(H265BsReal *bs_);
    mfxStatus PutPPS(H265BsReal *bs_);
    mfxStatus PutShortTermRefPicSet(H265BsReal *bs_, Ipp32s idx);
    mfxStatus PutSliceHeader(H265BsReal *bs_, H265Slice *slice);

//  SPS, PPS
    mfxStatus SetProfileLevel();
    mfxStatus SetVPS();
    mfxStatus SetSPS();
    mfxStatus SetPPS();
    mfxStatus SetSlice(H265Slice *slice, Ipp32u curr_slice);

    void InitShortTermRefPicSet();
    mfxStatus Init(mfxVideoH265InternalParam *param, mfxExtCodingOption *opts, mfxExtCodingOptionHEVC *opts_hevc);

    void Close();
    Ipp32u DetermineFrameType();
    mfxStatus EncodeFrame(mfxFrameSurface1 *surface, mfxBitstream *bs);
    mfxStatus DeblockThread(Ipp32s ithread);
    mfxStatus EncodeThread(Ipp32s ithread);
    mfxStatus MoveFromCPBToDPB();
    mfxStatus CleanDPB();

    void CreateRefPicSet(H265Slice *curr_slice);
    mfxStatus CheckCurRefPicSet(H265Slice *curr_slice);
    mfxStatus UpdateRefPicList(H265Slice *curr_slice);

    inline EncoderRefPicList* GetRefPicLists(Ipp32s SliceNum)
    {
        return &m_pRefPicList[SliceNum];
    }    // RefPicLists

    //////////////////

    mfxStatus InitH265VideoParam(mfxVideoH265InternalParam *param, mfxExtCodingOption *opts, mfxExtCodingOptionHEVC *opts_hevc);
};

#endif // __MFX_H265_ENC_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
