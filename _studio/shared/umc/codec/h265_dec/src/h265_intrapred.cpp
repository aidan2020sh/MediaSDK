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

#include "umc_h265_frame_info.h"
#include "h265_tr_quant.h"

namespace UMC_HEVC_DECODER
{
    void H265SegmentDecoder::ReconIntraQT(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Depth)
    {
        Ipp32u InitTrDepth = (pCU->GetPartitionSize(AbsPartIdx) == PART_SIZE_2Nx2N ? 0 : 1);
        Ipp32u NumPart = pCU->getNumPartInter(AbsPartIdx);
        Ipp32u NumQParts = pCU->m_NumPartition >> (Depth << 1); // Number of partitions on this depth
        NumQParts >>= 2;

        if (pCU->GetIPCMFlag(AbsPartIdx))
        {
            ReconPCM(pCU, AbsPartIdx, Depth);

            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> Depth;
            Ipp32s XInc = pCU->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = pCU->m_rasterToPelY[AbsPartIdx] >> 2;
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
            return;
        }

        Ipp32u ChromaPredMode = pCU->GetChromaIntra(AbsPartIdx);
        if (ChromaPredMode == INTRA_DM_CHROMA_IDX)
        {
            ChromaPredMode = pCU->GetLumaIntra(AbsPartIdx);
        }

        for (Ipp32u PU = 0; PU < NumPart; PU++)
        {
            IntraRecQT(pCU, InitTrDepth, AbsPartIdx + PU * NumQParts, ChromaPredMode);
        }
    }

    void H265SegmentDecoder::IntraRecQT(H265CodingUnit* pCU,
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u ChromaPredMode)
    {
        Ipp32u FullDepth = pCU->GetDepth(AbsPartIdx) + TrDepth;
        Ipp32u TrMode = pCU->GetTrIndex(AbsPartIdx);
        if (TrMode == TrDepth)
        {
            Ipp32s Size = m_pSeqParamSet->MaxCUSize >> FullDepth;
            Ipp32s XInc = pCU->m_rasterToPelX[AbsPartIdx] >> 2;
            Ipp32s YInc = pCU->m_rasterToPelY[AbsPartIdx] >> 2;
            Ipp32s diagId = XInc + (m_pSeqParamSet->MaxCUSize >> 2) - YInc - 1;

            Ipp32u lfIf = m_context->m_RecLfIntraFlags[XInc] >> (YInc);
            Ipp32u tlIf = (*m_context->m_RecTLIntraFlags >> diagId) & 0x1;
            Ipp32u tpIf = m_context->m_RecTpIntraFlags[YInc] >> (XInc);

            IntraRecLumaBlk(pCU, TrDepth, AbsPartIdx, tpIf, lfIf, tlIf);
            if (Size == 4) {
                if ((AbsPartIdx & 0x03) == 0) {
                    IntraRecChromaBlk(pCU, TrDepth-1, AbsPartIdx, ChromaPredMode, tpIf, lfIf, tlIf);
                }
            } else {
                IntraRecChromaBlk(pCU, TrDepth, AbsPartIdx, ChromaPredMode, tpIf, lfIf, tlIf);
            }
            UpdateRecNeighboursBuffersN(XInc, YInc, Size, true);
        }
        else
        {
            Ipp32u NumQPart = pCU->m_Frame->getCD()->getNumPartInCU() >> ((FullDepth + 1) << 1);
            for (Ipp32u Part = 0; Part < 4; Part++)
            {
                IntraRecQT(pCU, TrDepth + 1, AbsPartIdx + Part * NumQPart, ChromaPredMode);
            }
        }
    }

    void H265SegmentDecoder::IntraRecLumaBlk(H265CodingUnit* pCU,
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        Ipp32u width = pCU->GetWidth(AbsPartIdx) >> TrDepth;

        //===== get Predicted Pels =====
        Ipp8u PredPel[(4*64+1) * 2];

        const Ipp32s FilteredModes[] = {10, 7, 1, 0, 10};
        const Ipp8u h265_log2table[] =
        {
            2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
            5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6
        };

        m_reconstructor->GetPredPelsLuma(pCU->m_Frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx),
            PredPel, width, pCU->m_Frame->pitch_luma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_luma);

        bool isFilterNeeded = true;
        Ipp32u LumaPredMode = pCU->GetLumaIntra(AbsPartIdx);

        if (LumaPredMode == INTRA_LUMA_DC_IDX)
        {
            isFilterNeeded = false;
        }
        else
        {
            Ipp32s diff = IPP_MIN(abs((int)LumaPredMode - (int)INTRA_LUMA_HOR_IDX), abs((int)LumaPredMode - (int)INTRA_LUMA_VER_IDX));

            if (diff <= FilteredModes[h265_log2table[width - 4] - 2])
            {
                isFilterNeeded = false;
            }
        }   

        if (isFilterNeeded)
        {
            m_reconstructor->FilterPredictPels(m_context, pCU, PredPel, width, TrDepth, AbsPartIdx);
        }

        //===== get prediction signal =====    
        H265PlanePtrYCommon pRec = m_context->m_frame->GetLumaAddr(pCU->CUAddr, AbsPartIdx);
        Ipp32u pitch = m_context->m_frame->pitch_luma();

        m_reconstructor->PredictIntra(LumaPredMode, PredPel, pRec, pitch, width, m_pSeqParamSet->bit_depth_luma);

        //===== inverse transform =====
        if (!pCU->GetCbf(COMPONENT_LUMA, AbsPartIdx, TrDepth))
            return;

        Ipp32u NumCoeffInc = m_pSeqParamSet->MinCUSize * m_pSeqParamSet->MinCUSize;
        H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffY + (NumCoeffInc * AbsPartIdx);
        bool useTransformSkip = pCU->GetTransformSkip(COMPONENT_LUMA, AbsPartIdx) != 0;

        m_TrQuant->InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_LUMA, pCU->GetLumaIntra(AbsPartIdx),
            pRec, pitch, pCoeff, width, useTransformSkip);

    } // void H265SegmentDecoder::IntraRecLumaBlk(...)

    template <typename PixType>
    void SumOfResidAndPred(H265CoeffsPtrCommon p_ResiU, H265CoeffsPtrCommon p_ResiV, Ipp32u residualPitch, PixType *pRecIPred, Ipp32u RecIPredStride, Ipp32u Size,
        bool chromaUPresent, bool chromaVPresent, Ipp32u bit_depth)
    {
        if (sizeof(PixType) == 1)
            bit_depth = 8;

        for (Ipp32u y = 0; y < Size; y++)
        {
            for (Ipp32u x = 0; x < Size; x++)
            {
                if (chromaUPresent)
                    pRecIPred[2*x] = (PixType)ClipC(pRecIPred[2*x] + p_ResiU[x], bit_depth);
                if (chromaVPresent)
                    pRecIPred[2*x+1] = (PixType)ClipC(pRecIPred[2*x + 1] + p_ResiV[x], bit_depth);

            }
            p_ResiU     += residualPitch;
            p_ResiV     += residualPitch;
            pRecIPred += RecIPredStride;
        }
    }

    void H265SegmentDecoder::IntraRecChromaBlk(H265CodingUnit* pCU,
        Ipp32u TrDepth,
        Ipp32u AbsPartIdx,
        Ipp32u ChromaPredMode,
        Ipp32u tpIf, Ipp32u lfIf, Ipp32u tlIf)
    {
        Ipp8u PredPel[(4*64+2)*2];

        m_reconstructor->GetPredPelsChromaNV12(pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx),
            PredPel, pCU->GetWidth(AbsPartIdx) >> TrDepth, pCU->m_Frame->pitch_chroma(), tpIf, lfIf, tlIf, m_pSeqParamSet->bit_depth_chroma);

        //===== get prediction signal =====
        Ipp32u Size = pCU->GetWidth(AbsPartIdx) >> (TrDepth + 1);
        H265PlanePtrUVCommon pRecIPred = pCU->m_Frame->GetCbCrAddr(pCU->CUAddr, AbsPartIdx);
        Ipp32u RecIPredStride = pCU->m_Frame->pitch_chroma();

        m_reconstructor->PredictIntraChroma(ChromaPredMode, PredPel, pRecIPred, RecIPredStride, Size);

        bool chromaUPresent = pCU->GetCbf(COMPONENT_CHROMA_U, AbsPartIdx, TrDepth) != 0;
        bool chromaVPresent = pCU->GetCbf(COMPONENT_CHROMA_V, AbsPartIdx, TrDepth) != 0;

        if (!chromaUPresent && !chromaVPresent)
            return;

        //===== inverse transform =====
        Ipp32u NumCoeffInc = (pCU->m_SliceHeader->m_SeqParamSet->MinCUSize * pCU->m_SliceHeader->m_SeqParamSet->MinCUSize) >> 2;
        Ipp32u residualPitch = m_ppcYUVResi->pitch_chroma() >> 1;

        // Cb
        if (chromaUPresent)
        {
            H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCb + (NumCoeffInc * AbsPartIdx);
            H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
            bool useTransformSkip = pCU->GetTransformSkip(COMPONENT_CHROMA_U, AbsPartIdx) != 0;
            m_TrQuant->InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_U, REG_DCT,
                pResi, residualPitch, pCoeff, Size, useTransformSkip);
        }

        // Cr
        if (chromaVPresent)
        {
            H265CoeffsPtrCommon pCoeff = pCU->m_TrCoeffCr + (NumCoeffInc * AbsPartIdx);
            H265CoeffsPtrCommon pResi = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;
            bool useTransformSkip = pCU->GetTransformSkip(COMPONENT_CHROMA_V, AbsPartIdx) != 0;
            m_TrQuant->InvTransformNxN(pCU->GetCUTransquantBypass(AbsPartIdx), TEXT_CHROMA_V, REG_DCT,
                pResi, residualPitch, pCoeff, Size, useTransformSkip);
        }
/*
        //===== reconstruction =====
        {
            H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
            H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;

            switch ((chromaVPresent << 1) | chromaUPresent) {

            case ((0 << 1) | 1):    // U only

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_loadl_epi64((__m128i *)&p_ResiU[x]);
                        __m128i ResiV = _mm_setzero_si128();
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);        
                
                        IPred = _mm_cvtepu8_epi16(IPred);
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);
                        IPred = _mm_add_epi16(IPred, ResiU);
                        IPred = _mm_packus_epi16(IPred, IPred);

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;

            case ((1 << 1) | 0):    // V only

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_setzero_si128();
                        __m128i ResiV = _mm_loadl_epi64((__m128i *)&p_ResiV[x]);
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);
                
                        IPred = _mm_cvtepu8_epi16(IPred);
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);
                        IPred = _mm_add_epi16(IPred, ResiU);
                        IPred = _mm_packus_epi16(IPred, IPred);

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;

            case ((1 << 1) | 1):    // both U and V

                for (Ipp32u y = 0; y < Size; y++)
                {
                    for (Ipp32u x = 0; x < Size; x += 4)
                    {
                        __m128i ResiU = _mm_loadl_epi64((__m128i *)&p_ResiU[x]);        // load 4x16
                        __m128i ResiV = _mm_loadl_epi64((__m128i *)&p_ResiV[x]);        // load 4x16
                        __m128i IPred = _mm_loadl_epi64((__m128i *)&pRecIPred[2*x]);    // load 8x8           
                
                        IPred = _mm_cvtepu8_epi16(IPred);           // zero-extend
                        ResiU = _mm_unpacklo_epi16(ResiU, ResiV);   // interleave residuals
                        IPred = _mm_add_epi16(IPred, ResiU);        // add residuals
                        IPred = _mm_packus_epi16(IPred, IPred);     // ClipC()

                        _mm_storel_epi64((__m128i *)&pRecIPred[2*x], IPred);
                    }
                    p_ResiU += residualPitch;
                    p_ResiV += residualPitch;
                    pRecIPred += RecIPredStride;
                }
                break;
            }
        }
*/
        //===== reconstruction =====
          {
            H265CoeffsPtrCommon p_ResiU = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pUPlane;
            H265CoeffsPtrCommon p_ResiV = (H265CoeffsPtrCommon)m_ppcYUVResi->m_pVPlane;

            if (m_pSeqParamSet->bit_depth_chroma > 8 || m_pSeqParamSet->bit_depth_luma > 8)
                SumOfResidAndPred<Ipp16u>(p_ResiU, p_ResiV, residualPitch, (Ipp16u*)pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
            else
                SumOfResidAndPred<Ipp8u>(p_ResiU, p_ResiV, residualPitch, pRecIPred, RecIPredStride, Size, chromaUPresent, chromaVPresent, m_pSeqParamSet->bit_depth_chroma);
                
        }
    }
} // end namespace UMC_HEVC_DECODER

#endif // UMC_ENABLE_H264_VIDEO_DECODER
