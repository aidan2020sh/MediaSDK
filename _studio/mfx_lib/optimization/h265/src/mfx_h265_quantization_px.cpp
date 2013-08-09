/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE) || defined(MFX_ENABLE_H265_VIDEO_DECODE)

#include "mfx_h265_optimization.h"

#if defined(MFX_TARGET_OPTIMIZATION_PX) || defined(MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2) || defined(MFX_TARGET_OPTIMIZATION_ATOM)

#define Saturate(min_val, max_val, val) IPP_MAX((min_val), IPP_MIN((max_val), (val)))

namespace MFX_HEVC_ENCODER
{
    void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        Ipp32s sign;
        Ipp32s aval;
        Ipp32s qval;

#pragma ivdep
#pragma vector always

        for (Ipp32s i = 0; i < len; i++)
        {
            sign = pSrc[i] >> 15;

            aval = abs((Ipp32s)pSrc[i]);        // remove sign
            qval = (aval * scale + offset) >> shift;
            qval = (qval ^ sign) - sign;        // restore sign

            pDst[i] = (Ipp16s)Saturate(-32768, 32767, qval);
        }        

    } // void h265_QuantFwd_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scaleLevel, int scaleOffset, int scale)


    Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scale, int offset, int shift)
    {
        Ipp32s sign;
        Ipp32s aval;
        Ipp32s qval;
        Ipp32s abs_sum = 0;

        for (Ipp32s i = 0; i < len; i++)
        {
            sign = pSrc[i] >> 15;

            aval = abs((Ipp32s)pSrc[i]);        // remove sign
            qval = (aval * scale + offset) >> shift;
            qval = (qval ^ sign) - sign;        // restore sign

            pDst[i] = (Ipp16s)Saturate(-32768, 32767, qval);

            pDelta[i] = (Ipp32s)( ((Ipp64s)abs(pSrc[i]) * scale - (qval<<shift) )>> (shift-8) );

            abs_sum += qval;
        }

        return abs_sum;

    } // Ipp32s h265_QuantFwd_SBH_16s(const Ipp16s* pSrc, Ipp16s* pDst, Ipp32s*  pDelta, int len, int scaleLevel, int scaleOffset, int scale)

} // end namespace MFX_HEVC_ENCODER


namespace MFX_HEVC_COMMON
{
    void h265_QuantInv_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)
    {
        // ML: OPT: verify vectorization
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s n = 0; n < len; n++)
        {
            // clipped when decoded
            Ipp32s coeffQ = (pSrc[n] * scale + offset) >> shift;
            pDst[n] = (Ipp16s)Saturate(-32768, 32767, coeffQ);
        }

    } // void h265_QuantInv_16s(const Ipp16s* pSrc, Ipp16s* pDst, int len, int scale, int offset, int shift)

    
    void h265_QuantInv_ScaleList_LShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int shift)
    {
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s n = 0; n < len; n++)
        {
            // clipped when decoded
            Ipp32s CoeffQ   = Saturate(-32768, 32767, pSrc[n] * pScaleList[n]); 
            pDst[n] = (Ipp16s)Saturate(-32768, 32767, CoeffQ << shift );
        }

    } // void h265_QuantInv_ScaleList_LShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int shift)


    void h265_QuantInv_ScaleList_RShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int offset, int shift)
    {
#ifdef __INTEL_COMPILER
#pragma ivdep
#pragma vector always
#endif
        for (Ipp32s n = 0; n < len; n++)
        {
            // clipped when decoded
            Ipp32s coeffQ = ((pSrc[n] * pScaleList[n]) + offset) >> shift;
            pDst[n] = (Ipp16s)Saturate(-32768, 32767, coeffQ);
        }

    } // void h265_QuantInv_ScaleList_RShift_16s(const Ipp16s* pSrc, const Ipp16s* pScaleList, Ipp16s* pDst, int len, int offset, int shift)

} // namespace MFX_HEVC_COMMON

#endif // #if defined (MFX_TARGET_OPTIMIZATION_PX) || (MFX_TARGET_OPTIMIZATION_SSE4) || (MFX_TARGET_OPTIMIZATION_AVX2)
#endif // #if defined (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */
