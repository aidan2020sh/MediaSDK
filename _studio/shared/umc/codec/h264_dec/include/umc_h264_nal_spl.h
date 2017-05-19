//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_NAL_SPL_H
#define __UMC_H264_NAL_SPL_H

#include <vector>
#include "umc_h264_dec_defs_dec.h"
#include "umc_media_data_ex.h"
#include "umc_h264_heap.h"

namespace UMC
{

inline
bool IsItAllowedCode(Ipp32s iCode)
{
    return ((NAL_UT_SLICE <= (iCode & NAL_UNITTYPE_BITS)) &&
        (NAL_UT_PPS >= (iCode & NAL_UNITTYPE_BITS)) &&
        (NAL_UT_SEI != (iCode & NAL_UNITTYPE_BITS))) ||
        (NAL_UT_SPS_EX == (iCode & NAL_UNITTYPE_BITS)) ||
        (NAL_UT_AUXILIARY == (iCode & NAL_UNITTYPE_BITS));
} // bool IsItAllowedCode(Ipp32s iCode)

inline
bool IsHeaderCode(Ipp32s iCode)
{
    return (NAL_UT_SPS == (iCode & NAL_UNITTYPE_BITS)) ||
           (NAL_UT_SPS_EX == (iCode & NAL_UNITTYPE_BITS)) ||
           (NAL_UT_PPS == (iCode & NAL_UNITTYPE_BITS)) ||
           (NAL_UT_SUBSET_SPS == (iCode & NAL_UNITTYPE_BITS));
}

inline
bool IsVLCCode(Ipp32s iCode)
{
    return ((NAL_UT_SLICE <= (iCode & NAL_UNITTYPE_BITS)) &&
           (NAL_UT_IDR_SLICE >= (iCode & NAL_UNITTYPE_BITS))) ||
           (NAL_UT_AUXILIARY == (iCode & NAL_UNITTYPE_BITS)) ||
           (NAL_UT_CODED_SLICE_EXTENSION == (iCode & NAL_UNITTYPE_BITS));
}

class MediaData;
class NALUnitSplitter;

class NalUnit : public MediaData
{
public:
    NalUnit()
        : MediaData()
        , m_nal_unit_type(NAL_UT_UNSPECIFIED)
        , m_use_external_memory(false)
    {
    }

    NAL_Unit_Type GetNalUnitType() const
    {
        return (NAL_Unit_Type)m_nal_unit_type;
    }

    bool IsUsedExternalMem() const
    {
        return m_use_external_memory;
    }

    int m_nal_unit_type;
    bool m_use_external_memory;
};

class SwapperBase
{
public:
    virtual ~SwapperBase() {}

    virtual void SwapMemory(Ipp8u *pDestination, size_t &nDstSize, Ipp8u *pSource, size_t nSrcSize) = 0;
    virtual void SwapMemory(H264MemoryPiece * pMemDst, H264MemoryPiece * pMemSrc, Ipp8u defaultValue = DEFAULT_NU_TAIL_VALUE) = 0;

    virtual void CopyBitStream(Ipp8u *pDestination, Ipp8u *pSource, size_t &nSrcSize) = 0;
};

class StartCodeIteratorBase
{
public:

    StartCodeIteratorBase()
        : m_pSource(0)
        , m_nSourceSize(0)
        , m_pSourceBase(0)
        , m_nSourceBaseSize(0)
        , m_suggestedSize(10 * 1024)
    {
    }

    virtual ~StartCodeIteratorBase()
    {
    }

    virtual Ipp32s Init(MediaData * pSource)
    {
        m_pSourceBase = m_pSource = (Ipp8u *) pSource->GetDataPointer();
        m_nSourceBaseSize = m_nSourceSize = pSource->GetDataSize();
        return -1;
    }

    Ipp32s GetCurrentOffset()
    {
        return (Ipp32s)(m_pSource - m_pSourceBase);
    }

    virtual void SetSuggestedSize(size_t size)
    {
        if (size > m_suggestedSize)
            m_suggestedSize = size;
    }

    virtual Ipp32s GetNext() = 0;

    virtual Ipp32s CheckNalUnitType(MediaData * pSource) = 0;
    virtual Ipp32s MoveToStartCode(MediaData * pSource) = 0;
    virtual Ipp32s GetNALUnit(MediaData * pSource, NalUnit * pDst) = 0;

    virtual void Reset() = 0;

protected:
    Ipp8u * m_pSource;
    size_t  m_nSourceSize;

    Ipp8u * m_pSourceBase;
    size_t  m_nSourceBaseSize;

    size_t  m_suggestedSize;
};

class NALUnitSplitter
{
public:

    NALUnitSplitter();

    virtual ~NALUnitSplitter();

    virtual void Init();
    virtual void Release();

    virtual Ipp32s CheckNalUnitType(MediaData * pSource);
    virtual Ipp32s MoveToStartCode(MediaData * pSource);
    virtual NalUnit * GetNalUnits(MediaData * in);

    virtual void Reset();

    virtual void SetSuggestedSize(size_t size)
    {
        if (!m_pStartCodeIter)
            return;

        m_pStartCodeIter->SetSuggestedSize(size);
    }

    SwapperBase * GetSwapper()
    {
        return m_pSwapper;
    }

protected:

    SwapperBase *   m_pSwapper;
    StartCodeIteratorBase * m_pStartCodeIter;

    NalUnit     m_nalUnit;
};

} // namespace UMC

#endif // __UMC_H264_NAL_SPL_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
