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

#ifndef __H265_TR_QUANT_H
#define __H265_TR_QUANT_H

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_bitstream.h"
#include "h265_coding_unit.h"
#include "h265_global_rom.h"
#include "assert.h"

#define QP_BITS 15

namespace UMC_HEVC_DECODER
{

// transform and quantization class
class H265TrQuant
{
public:
    H265TrQuant();
    ~H265TrQuant();

    // transform & inverse transform functions
    template <typename DstCoeffsType>
    void InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual, Ipp32u Stride,
        H265CoeffsPtrCommon pCoeff,Ipp32u Width, Ipp32u Height, bool transformSkip);

    void InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode);

    // ML: OPT: allows to propogate convert const shift
    template <int bitDepth, typename DstCoeffsType>
    void InvTransformSkip(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height);

private:
    H265CoeffsPtrCommon m_residualsBuffer;
    H265CoeffsPtrCommon m_residualsBuffer1;
    H265CoeffsPtrCommon m_tempTransformBuffer;

    // forward Transform
    void Transform(Ipp32u Mode, Ipp8u* pResidual, Ipp32u Stride, Ipp32s* pCoeff, Ipp32s Width, Ipp32s Height);
};// END CLASS DEFINITION H265TrQuant

} // end namespace UMC_HEVC_DECODER

#endif //__H265_TR_QUANT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
