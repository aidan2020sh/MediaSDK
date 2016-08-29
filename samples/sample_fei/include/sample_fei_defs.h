/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#ifndef __SAMPLE_FEI_DEFS_H__
#define __SAMPLE_FEI_DEFS_H__

#include <mfxfei.h>
#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include <algorithm>
#include <memory>
#include <cstring>
#include <list>
#include <vector>

const mfxU16 MaxFeiEncMVPNum = 4;

#define MSDK_ZERO_ARRAY(VAR, NUM) {memset(VAR, 0, sizeof(VAR[0])*NUM);}
#define SAFE_RELEASE_EXT_BUFSET(SET) {if (SET){ SET->vacant = true; SET = NULL;}}
#define SAFE_DEC_LOCKER(SURFACE){if (SURFACE && SURFACE->Data.Locked) {SURFACE->Data.Locked--;}}
#define SAFE_UNLOCK(SURFACE){if (SURFACE) {SURFACE->Data.Locked = 0;}}
#define SAFE_FCLOSE(FPTR, ERR){ if (FPTR && fclose(FPTR)) { return ERR; }}
#define SAFE_FREAD(PTR, SZ, COUNT, FPTR, ERR) { if (FPTR && (fread(PTR, SZ, COUNT, FPTR) != COUNT)){ return ERR; }}
#define SAFE_FWRITE(PTR, SZ, COUNT, FPTR, ERR) { if (FPTR && (fwrite(PTR, SZ, COUNT, FPTR) != COUNT)){ return ERR; }}
#define SAFE_FSEEK(FPTR, OFFSET, ORIGIN, ERR) { if (FPTR && fseek(FPTR, OFFSET, ORIGIN)){ return ERR; }}

#define MFX_FRAMETYPE_IPB (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)
#define MFX_FRAMETYPE_IP  (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P                  )
#define MFX_FRAMETYPE_PB  (                  MFX_FRAMETYPE_P | MFX_FRAMETYPE_B)

enum
{
    FEI_SLICETYPE_I = 2,
    FEI_SLICETYPE_P = 0,
    FEI_SLICETYPE_B = 1,
};

enum
{
    RPLM_ST_PICNUM_SUB = 0,
    RPLM_ST_PICNUM_ADD = 1,
    RPLM_LT_PICNUM     = 2,
    RPLM_END           = 3,
    RPLM_INTERVIEW_SUB = 4,
    RPLM_INTERVIEW_ADD = 5,
};

enum
{
    MMCO_END            = 0,
    MMCO_ST_TO_UNUSED   = 1,
    MMCO_LT_TO_UNUSED   = 2,
    MMCO_ST_TO_LT       = 3,
    MMCO_SET_MAX_LT_IDX = 4,
    MMCO_ALL_TO_UNUSED  = 5,
    MMCO_CURR_TO_LT     = 6,
};

// B frame location struct for reordering

template<class T> inline void Zero(T & obj) { memset(&obj, 0, sizeof(obj)); }
struct BiFrameLocation
{
    BiFrameLocation() { Zero(*this); }

    mfxU32 miniGopCount;    // sequence of B frames between I/P frames
    mfxU32 encodingOrder;   // number within mini-GOP (in encoding order)
    mfxU16 refFrameFlag;    // MFX_FRAMETYPE_REF if B frame is reference
};

template<class T, mfxU32 N>
struct FixedArray
{
    FixedArray()
        : m_numElem(0)
    {
    }

    explicit FixedArray(T fillVal)
        : m_numElem(0)
    {
        Fill(fillVal);
    }

    void PushBack(T const & val)
    {
        //assert(m_numElem < N);
        m_arr[m_numElem] = val;
        m_numElem++;
    }

    void PushFront(T const val)
    {
        //assert(m_numElem < N);
        std::copy(m_arr, m_arr + m_numElem, m_arr + 1);
        m_arr[0] = val;
        m_numElem++;
    }

    void Erase(T * p)
    {
        //assert(p >= m_arr && p <= m_arr + m_numElem);

        m_numElem = mfxU32(
            std::copy(p + 1, m_arr + m_numElem, p) - m_arr);
    }

    void Erase(T * b, T * e)
    {
        //assert(b <= e);
        //assert(b >= m_arr && b <= m_arr + m_numElem);
        //assert(e >= m_arr && e <= m_arr + m_numElem);

        m_numElem = mfxU32(
            std::copy(e, m_arr + m_numElem, b) - m_arr);
    }

    void Resize(mfxU32 size, T fillVal = T())
    {
        //assert(size <= N);
        for (mfxU32 i = m_numElem; i < size; ++i)
            m_arr[i] = fillVal;
        m_numElem = size;
    }

    T * Begin()
    {
        return m_arr;
    }

    T const * Begin() const
    {
        return m_arr;
    }

    T * End()
    {
        return m_arr + m_numElem;
    }

    T const * End() const
    {
        return m_arr + m_numElem;
    }

    T & Back()
    {
        //assert(m_numElem > 0);
        return m_arr[m_numElem - 1];
    }

    T const & Back() const
    {
        //assert(m_numElem > 0);
        return m_arr[m_numElem - 1];
    }

    mfxU32 Size() const
    {
        return m_numElem;
    }

    mfxU32 Capacity() const
    {
        return N;
    }

    T & operator[](mfxU32 idx)
    {
        //assert(idx < N);
        return m_arr[idx];
    }

    T const & operator[](mfxU32 idx) const
    {
        //assert(idx < N);
        return m_arr[idx];
    }

    void Fill(T val)
    {
        for (mfxU32 i = 0; i < N; i++)
        {
            m_arr[i] = val;
        }
    }

    template<mfxU32 M>
    bool operator==(const FixedArray<T, M>& r) const
    {
        //assert(Size() <= N);
        //assert(r.Size() <= M);

        if (Size() != r.Size())
        {
            return false;
        }

        for (mfxU32 i = 0; i < Size(); i++)
        {
            if (m_arr[i] != r[i])
            {
                return false;
            }
        }

        return true;
    }

private:
    T      m_arr[N];
    mfxU32 m_numElem;
};

template <typename T> struct Pair
{
    T top;
    T bot;

    Pair()
    {
    }

    template<typename U> Pair(Pair<U> const & pair)
        : top(static_cast<T>(pair.top))
        , bot(static_cast<T>(pair.bot))
    {
    }

    template<typename U> explicit Pair(U const & value)
        : top(static_cast<T>(value))
        , bot(static_cast<T>(value))
    {
    }

    template<typename U> Pair(U const & t, U const & b)
        : top(static_cast<T>(t))
        , bot(static_cast<T>(b))
    {
    }

    template<typename U>
    Pair<T> & operator =(Pair<U> const & pair)
    {
        Pair<T> tmp(pair);
        std::swap(*this, tmp);
        return *this;
    }

    T & operator[] (mfxU32 parity)
    {
        //assert(parity < 2);
        return (&top)[parity & 1];
    }

    T const & operator[] (mfxU32 parity) const
    {
        //assert(parity < 2);
        return (&top)[parity & 1];
    }
};

struct RefListMod
{
    RefListMod() : m_idc(3), m_diff(0) {}
    RefListMod(mfxU16 idc, mfxU16 diff) : m_idc(idc), m_diff(diff) { /*assert(idc < 6);*/ }
    mfxU16 m_idc;
    mfxU16 m_diff;
};

typedef FixedArray<RefListMod, 32> ArrayRefListMod;
typedef FixedArray<mfxU8, 8>       ArrayU8x8;
typedef FixedArray<mfxU8, 16>      ArrayU8x16;
typedef FixedArray<mfxU8, 32>      ArrayU8x32;
typedef FixedArray<mfxU8, 33>      ArrayU8x33;
typedef FixedArray<mfxU32, 64>     ArrayU32x64;

typedef Pair<mfxU8> PairU8;
typedef Pair<mfxI32> PairI32;

struct DpbFrame
{
    DpbFrame() { Zero(*this); }

    PairI32 m_poc;
    mfxU32  m_frameOrder;
    mfxU32  m_frameNum;
    mfxI32  m_frameNumWrap;
    PairI32 m_picNum;
    mfxU32  m_frameIdx;
    PairU8  m_longTermPicNum;
    PairU8  m_refPicFlag;
    mfxU8   m_longterm; // at least one field is a long term reference
    mfxU8   m_refBase;
};

struct ArrayDpbFrame : public FixedArray<DpbFrame, 16>
{
    ArrayDpbFrame()
        : FixedArray<DpbFrame, 16>()
    {
        m_maxLongTermFrameIdxPlus1.Resize(8, 0);
    }

    ArrayU8x8 m_maxLongTermFrameIdxPlus1; // for each temporal layer
};

inline mfxI32 GetPicNum(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_picNum[ref >> 7];
}

inline mfxI32 GetPicNumF(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    DpbFrame const & dpbFrame = dpb[ref & 127];
    return dpbFrame.m_refPicFlag[ref >> 7] ? dpbFrame.m_picNum[ref >> 7] : 0x20000;
}

inline mfxU8 GetLongTermPicNum(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_longTermPicNum[ref >> 7];
}

inline mfxU32 GetLongTermPicNumF(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    DpbFrame const & dpbFrame = dpb[ref & 127];

    return dpbFrame.m_refPicFlag[ref >> 7] && dpbFrame.m_longterm
        ? dpbFrame.m_longTermPicNum[ref >> 7]
        : 0x20;
}

inline mfxI32 GetPoc(ArrayDpbFrame const & dpb, mfxU8 ref)
{
    return dpb[ref & 127].m_poc[ref >> 7];
}

struct DecRefPicMarkingInfo
{
    DecRefPicMarkingInfo() { Zero(*this); }

    void PushBack(mfxU8 op, mfxU32 param0, mfxU32 param1 = 0)
    {
        mmco.PushBack(op);
        value.PushBack(param0);
        value.PushBack(param1);
    }

    mfxU8       no_output_of_prior_pics_flag;
    mfxU8       long_term_reference_flag;
    ArrayU8x32  mmco;       // memory management control operation id
    ArrayU32x64 value;      // operation-dependent data, max 2 per operation
};

struct BasePredicateForRefPic
{
    typedef ArrayDpbFrame Dpb;
    typedef mfxU8         Arg;
    typedef bool          Res;

    BasePredicateForRefPic(Dpb const & dpb) : m_dpb(dpb) {}

    void operator =(BasePredicateForRefPic const &);

    Dpb const & m_dpb;
};

struct RefPicNumIsGreater : public BasePredicateForRefPic
{
    RefPicNumIsGreater(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPicNum(m_dpb, l) > GetPicNum(m_dpb, r);
    }
};

struct LongTermRefPicNumIsLess : public BasePredicateForRefPic
{
    LongTermRefPicNumIsLess(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetLongTermPicNum(m_dpb, l) < GetLongTermPicNum(m_dpb, r);
    }
};

struct RefPocIsLess : public BasePredicateForRefPic
{
    RefPocIsLess(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPoc(m_dpb, l) < GetPoc(m_dpb, r);
    }
};

struct RefPocIsGreater : public BasePredicateForRefPic
{
    RefPocIsGreater(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 l, mfxU8 r) const
    {
        return GetPoc(m_dpb, l) > GetPoc(m_dpb, r);
    }
};

struct RefPocIsLessThan : public BasePredicateForRefPic
{
    RefPocIsLessThan(Dpb const & dpb, mfxI32 poc) : BasePredicateForRefPic(dpb), m_poc(poc) {}

    bool operator ()(mfxU8 r) const
    {
        return GetPoc(m_dpb, r) < m_poc;
    }

    mfxI32 m_poc;
};

struct RefPocIsGreaterThan : public BasePredicateForRefPic
{
    RefPocIsGreaterThan(Dpb const & dpb, mfxI32 poc) : BasePredicateForRefPic(dpb), m_poc(poc) {}

    bool operator ()(mfxU8 r) const
    {
        return GetPoc(m_dpb, r) > m_poc;
    }

    mfxI32 m_poc;
};

struct RefIsShortTerm : public BasePredicateForRefPic
{
    RefIsShortTerm(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 r) const
    {
        return m_dpb[r & 127].m_refPicFlag[r >> 7] && !m_dpb[r & 127].m_longterm;
    }
};

struct RefIsLongTerm : public BasePredicateForRefPic
{
    RefIsLongTerm(Dpb const & dpb) : BasePredicateForRefPic(dpb) {}

    bool operator ()(mfxU8 r) const
    {
        return m_dpb[r & 127].m_refPicFlag[r >> 7] && m_dpb[r & 127].m_longterm;
    }
};

inline bool RefListHasLongTerm(
    ArrayDpbFrame const & dpb,
    ArrayU8x33 const &    list)
{
    return std::find_if(list.Begin(), list.End(), RefIsLongTerm(dpb)) != list.End();
}

template <class T, class U> struct LogicalAndHelper
{
    typedef typename T::Arg Arg;
    typedef typename T::Res Res;
    T m_pr1;
    U m_pr2;

    LogicalAndHelper(T pr1, U pr2) : m_pr1(pr1), m_pr2(pr2) {}

    Res operator ()(Arg arg) const { return m_pr1(arg) && m_pr2(arg); }
};

template <class T, class U> LogicalAndHelper<T, U> LogicalAnd(T pr1, U pr2)
{
    return LogicalAndHelper<T, U>(pr1, pr2);
}

template <class T> struct LogicalNotHelper
{
    typedef typename T::argument_type Arg;
    typedef typename T::result_type   Res;
    T m_pr;

    LogicalNotHelper(T pr) : m_pr(pr) {}

    Res operator ()(Arg arg) const { return !m_pred(arg); }
};

template <class T> LogicalNotHelper<T> LogicalNot(T pr)
{
    return LogicalNotHelper<T>(pr);
}

inline mfxI8 GetIdxOfFirstSameParity(ArrayU8x33 const & refList, mfxU32 fieldId)
{
    for (mfxU8 i = 0; i < refList.Size(); i++)
    {
        mfxU8 refFieldId = (refList[i] & 128) >> 7;
        if (fieldId == refFieldId)
        {
            return (mfxI8)i;
        }
    }

    return -1;
}

inline void UpdateMaxLongTermFrameIdxPlus1(ArrayU8x8 & arr, mfxU32 curTidx, mfxU32 val)
{
    std::fill(arr.Begin() + curTidx, arr.End(), val);
}

enum
{
    TFIELD = 0,
    BFIELD = 1
};

inline bool OrderByFrameNumWrap(DpbFrame const & lhs, DpbFrame const & rhs)
{
    if (!lhs.m_longterm && !rhs.m_longterm)
        if (lhs.m_frameNumWrap < rhs.m_frameNumWrap)
            return lhs.m_refBase > rhs.m_refBase;
        else
            return lhs.m_frameNumWrap < rhs.m_frameNumWrap;
    else if (!lhs.m_longterm && rhs.m_longterm)
        return true;
    else if (lhs.m_longterm && !rhs.m_longterm)
        return false;
    else // both long term
        return lhs.m_longTermPicNum[0] < rhs.m_longTermPicNum[0];
}


inline bool OrderByDisplayOrder(DpbFrame const & lhs, DpbFrame const & rhs)
{
    return lhs.m_frameOrder < rhs.m_frameOrder;
}

struct OrderByNearestPrev
{
    mfxU32 m_fo;

    OrderByNearestPrev(mfxU32 displayOrder) : m_fo(displayOrder) {}
    bool operator() (DpbFrame const & l, DpbFrame const & r)
    {
        return (l.m_frameOrder < m_fo) && ((r.m_frameOrder > m_fo) || ((m_fo - l.m_frameOrder) < (m_fo - r.m_frameOrder)));
    }
};

#endif // #define __SAMPLE_FEI_DEFS_H__
