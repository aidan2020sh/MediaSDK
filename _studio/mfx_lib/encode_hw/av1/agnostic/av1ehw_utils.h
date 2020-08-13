// Copyright (c) 2019-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfxvideo.h"
#include <memory>
#include <map>
#include <list>
#include <exception>
#include <functional>
#include <algorithm>
#include <assert.h>
#include "feature_blocks/mfx_feature_blocks_utils.h"

namespace AV1EHW
{
using namespace MfxFeatureBlocks;

template <class T>
class NotNull
{
public:
typedef typename std::remove_reference<decltype(*T(nullptr))>::type TBase;
    NotNull(T p = nullptr)
        : m_ptr(p)
    {
    }

    T get() const
    {
        if (!m_ptr)
            throw std::logic_error("nullptr deref");
        return m_ptr;
    }
    operator T() const
    {
        return get();
    }
    T operator->() const
    {
        return get();
    }

    TBase& operator*() const
    {
        return *get();
    }

private:
    T m_ptr = nullptr;
};

template <class T>
inline T& Deref(T* ptr)
{
    return *((NotNull<T*>(ptr)).get());
}

class OnExit
    : public std::function<void()>
{
public:
    OnExit(const OnExit&) = delete;

    template<class... TArg>
    OnExit(TArg&& ...arg)
        : std::function<void()>(std::forward<TArg>(arg)...)
    {}

    ~OnExit()
    {
        if (operator bool())
            operator()();
    }

    template<class... TArg>
    OnExit& operator=(TArg&& ...arg)
    {
        std::function<void()> tmp(std::forward<TArg>(arg)...);
        swap(tmp);
        return *this;
    }
};

inline void ThrowAssert(bool bThrow, const char* msg)
{
    if (bThrow)
        throw std::logic_error(msg);
}

template <class T>
constexpr std::size_t Size(const T& c) { return (std::size_t)c.size(); }

template <class T, std::size_t N>
constexpr std::size_t Size(const T(&)[N]) { return N; }

template<typename T, size_t N>
inline std::reverse_iterator<T*> RBegin(T (&arr)[N])
{
    return std::reverse_iterator<T*>(arr + N);
}

template<typename T>
inline typename std::reverse_iterator<
    typename std::conditional<
        std::is_const<T>::value
        , typename T::const_iterator
        , typename T::iterator
    >::type
> RBegin(T& container)
{
    return container.rbegin();
}

template<typename T, size_t N>
inline std::reverse_iterator<T*> REnd(T (&arr)[N])
{
    return std::reverse_iterator<T*>(arr);
}

template<typename T>
inline typename std::reverse_iterator<
    typename std::conditional<
        std::is_const<T>::value
        , typename T::const_iterator
        , typename T::iterator
    >::type
> REnd(T& container)
{
    return container.rend();
}

template<class T>
inline std::reverse_iterator<T> MakeRIter(T i)
{
    return std::reverse_iterator<T>(i);
}

template<class TIn, class TOut, class TPred>
void MoveIf(TIn& src, TOut& dst, TPred pred)
{
    TIn newSrc;
    std::partition_copy(
        std::make_move_iterator(std::begin(src))
        , std::make_move_iterator(std::end(src))
        , std::back_inserter(dst)
        , std::back_inserter(newSrc)
        , pred);
    src = std::move(newSrc);
}

template <class T>
inline bool Check(const T&)
{
    return true;
}

template <class T, T val, T... other>
inline bool Check(const T & opt)
{
    if (opt == val)
        return false;
    return Check<T, other...>(opt);
}

template <class T, T val>
inline bool CheckGE(T opt)
{
    return !(opt >= val);
}

template <class T, class... U>
bool Check(T & opt, T next, U... other)
{
    if (opt == next)
        return false;
    return Check(opt, other...);
}

template <class T>
inline bool CheckOrZero(T& opt)
{
    opt = 0;
    return true;
}

template <class T, T val, T... other>
inline bool CheckOrZero(T & opt)
{
    if (opt == val)
        return false;
    return CheckOrZero<T, other...>(opt);
}

template <class T, class... U>
bool CheckOrZero(T & opt, T next, U... other)
{
    if (opt == next)
        return false;
    return CheckOrZero(opt, (T)other...);
}

template <class T, class U>
bool CheckMaxOrZero(T & opt, U max)
{
    if (opt <= max)
        return false;
    opt = 0;
    return true;
}

template <class T, class U>
bool CheckMinOrZero(T & opt, U min)
{
    if (opt >= min)
        return false;
    opt = 0;
    return true;
}

template <class T, class U>
bool CheckMaxOrClip(T & opt, U max)
{
    if (opt <= max)
        return false;
    opt = T(max);
    return true;
}

template <class T, class U>
bool CheckMinOrClip(T & opt, U min)
{
    if (opt >= min)
        return false;
    opt = T(min);
    return true;
}

template<class T, class I>
inline bool CheckRangeOrClip(T & opt, I min, I max)
{
    if (opt < static_cast<T>(min))
    {
        opt = static_cast<T>(min);
        return true;
    }

    if (opt > static_cast<T>(max))
    {
        opt = static_cast<T>(max);
        return true;
    }

    return false;
}

template <class T>
bool CheckRangeOrSetDefault(T & opt, T min, T max, T dflt)
{
    if (opt >= min && opt <= max)
        return false;
    opt = dflt;
    return true;
}

inline bool CheckTriState(mfxU16 opt)
{
    return Check<mfxU16
        , MFX_CODINGOPTION_UNKNOWN
        , MFX_CODINGOPTION_ON
        , MFX_CODINGOPTION_OFF>(opt);
}

inline bool CheckTriStateOrZero(mfxU16& opt)
{
    return CheckOrZero<mfxU16
        , MFX_CODINGOPTION_UNKNOWN
        , MFX_CODINGOPTION_ON
        , MFX_CODINGOPTION_OFF>(opt);
}

template<class TVal, class TArg, typename std::enable_if<!std::is_constructible<TVal, TArg>::value, int>::type = 0>
inline TVal GetOrCall(TArg val) { return val(); }

template<class TVal, class TArg, typename = typename std::enable_if<std::is_constructible<TVal, TArg>::value>::type>
inline TVal GetOrCall(TArg val) { return TVal(val); }

template<typename T, typename TF>
inline bool SetDefault(T& opt, TF get_dflt)
{
    if (opt)
        return false;
    opt = GetOrCall<T>(get_dflt);
    return true;
}

template<typename T, typename TF>
inline bool SetIf(T& opt, bool bSet, TF get)
{
    if (!bSet)
        return false;
    opt = GetOrCall<T>(get);
    return true;
}
template<class T, class TF, class... TA>
inline bool SetIf(T& opt, bool bSet, TF&& get, TA&&... arg)
{
    if (!bSet)
        return false;
    opt = get(std::forward<TA>(arg)...);
    return true;
}

inline bool IsOn(mfxU16 opt)
{
    return opt == MFX_CODINGOPTION_ON;
}
inline bool IsOff(mfxU16 opt)
{
    return opt == MFX_CODINGOPTION_OFF;
}
inline bool IsIdr(mfxU32 type)
{
    return !!(type & MFX_FRAMETYPE_IDR);
}
inline bool IsI(mfxU32 type)
{
    return !!(type & MFX_FRAMETYPE_I);
}
inline bool IsB(mfxU32 type)
{
    return !!(type & MFX_FRAMETYPE_B);
}
inline bool IsP(mfxU32 type)
{
    return !!(type & MFX_FRAMETYPE_P);
}
inline bool IsRef(mfxU32 type)
{
    return !!(type & MFX_FRAMETYPE_REF);
}

template<class T>
bool AlignDown(T& value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    if (!(value & (alignment - 1))) return false;
    value = value & ~(alignment - 1);
    return true;
}

template<class T>
bool AlignUp(T& value, mfxU32 alignment)
{
    assert((alignment & (alignment - 1)) == 0); // should be 2^n
    if (!(value & (alignment - 1))) return false;
    value = (value + alignment - 1) & ~(alignment - 1);
    return true;
}

template<typename T>
inline mfxU32 CountTrailingZeroes(T x)
{
    mfxU32 l = 0;
    if (!x)
        return sizeof(x) * 8;
    while (!((x >> l) & 1))
        ++l;
    return l;
}
inline mfxU32 CeilLog2(mfxU32 x) { mfxU32 l = 0; while (x > (1U << l)) l++; return l; }
inline mfxU32 Ceil(mfxF64 x) { return (mfxU32)(.999 + x); }
template<class T> inline T CeilDiv(T x, T y) { return (x + y - 1) / y; }

template<class T, class Enable = void>
struct DefaultFiller
{
    static constexpr typename std::remove_reference<T>::type Get()
    {
        return typename std::remove_reference<T>::type();
    }
};

template<class T>
struct DefaultFiller<T, typename std::enable_if<
        std::is_arithmetic<
            typename std::remove_reference<T>::type
        >::value
    >::type>
{
    static constexpr typename std::remove_reference<T>::type Get()
    {
        return typename std::remove_reference<T>::type(-1);
    }
};

template<class A>
void Remove(
    A &_from
    , size_t _where
    , size_t _num = 1)
{
    if (std::end(_from) < std::begin(_from) + _where + _num)
        throw std::out_of_range("Remove() target is out of array range");

    auto it = std::copy(std::begin(_from) + _where + _num, std::end(_from), std::begin(_from) + _where);
    std::fill(it, std::end(_from), DefaultFiller<decltype(*it)>::Get());
}

template<class T, class Pr>
T RemoveIf(
    T _begin
    , T _end
    , Pr _pr)
{
    _begin = std::remove_if(_begin, _end, _pr);
    std::fill(_begin, _end, DefaultFiller<decltype(*_begin)>::Get());
    return _begin;
}

template<class T, class A>
void Insert(
    A &_to
    , size_t _where
    , T const & _what)
{
    if (std::begin(_to) + _where + 1 >= std::end(_to))
        throw std::out_of_range("Insert() target is out of array range");

    std::copy_backward(std::begin(_to) + _where, std::end(_to) - 1, std::end(_to));
    _to[_where] = _what;
}

template <class T>
bool InheritOption(T optInit, T & optReset)
{
    if (optReset == 0)
    {
        optReset = optInit;
        return true;
    }
    return false;
}
template<class TSrcIt, class TDstIt>
TDstIt InheritOptions(TSrcIt initFirst, TSrcIt initLast, TDstIt resetFirst)
{
    while (initFirst != initLast)
    {
        InheritOption(*initFirst++, *resetFirst++);
    }
    return resetFirst;
}

inline void UpdateMultiplier(mfxInfoMFX& mfx, mfxU16 MN)
{
    auto& MO = mfx.BRCParamMultiplier;

    SetDefault(MO, 1);

    if (MO != MN)
    {
        mfx.BufferSizeInKB = (mfxU16)CeilDiv<mfxU32>(mfx.BufferSizeInKB * MO, MN);

        if (   mfx.RateControlMethod == MFX_RATECONTROL_CBR
            || mfx.RateControlMethod == MFX_RATECONTROL_VBR
            || mfx.RateControlMethod == MFX_RATECONTROL_VCM
            || mfx.RateControlMethod == MFX_RATECONTROL_QVBR
            || mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
        {
            mfx.TargetKbps = (mfxU16)CeilDiv<mfxU32>(mfx.TargetKbps * MO, MN);
            mfx.InitialDelayInKB = (mfxU16)CeilDiv<mfxU32>(mfx.InitialDelayInKB * MO, MN);
            mfx.MaxKbps = (mfxU16)CeilDiv<mfxU32>(mfx.MaxKbps * MO, MN);
        }

        MO = MN;
    }
}

template<size_t offset>
class BRCMultiplied
{
public:
    BRCMultiplied(mfxInfoMFX& par)
        : pMfx(&par)
        , pcMfx(&par)
    {}

    BRCMultiplied(const mfxInfoMFX& par)
        : pMfx(nullptr)
        , pcMfx(&par)
    {}

    operator mfxU32 () const { return CV() * std::max<mfxU16>(1, pcMfx->BRCParamMultiplier); }

    mfxU32 operator= (mfxU32 x)
    {
        if (pMfx)
        {
            mfxU16 MN = std::max<mfxU16>(1, pcMfx->BRCParamMultiplier);

            while (CeilDiv<mfxU32>(x, MN) >= (1 << 16))
                MN++;

            UpdateMultiplier(*pMfx, MN);
            V() = (mfxU16)CeilDiv<mfxU32>(x, MN);
        }
        return *this;
    }

    BRCMultiplied& operator= (const BRCMultiplied& other)
    {
        *this = (mfxU32)other;
        return *this;
    }

private:
    mfxInfoMFX* pMfx;
    const mfxInfoMFX* pcMfx;

    inline mfxU16& V() { return *(mfxU16*)((mfxU8*)pMfx + offset); }
    inline mfxU16 CV() const { return *(const mfxU16*)((const mfxU8*)pcMfx + offset); }
};

typedef BRCMultiplied<offsetof(mfxInfoMFX, InitialDelayInKB)> InitialDelayInKB;
typedef BRCMultiplied<offsetof(mfxInfoMFX, BufferSizeInKB)> BufferSizeInKB;
typedef BRCMultiplied<offsetof(mfxInfoMFX, TargetKbps)> TargetKbps;
typedef BRCMultiplied<offsetof(mfxInfoMFX, MaxKbps)> MaxKbps;

template<class T>
class IterStepWrapper
    : public std::iterator_traits<T>
{
public:
    using iterator_category = std::forward_iterator_tag;
    using iterator_type     = IterStepWrapper;
    using reference         = typename std::iterator_traits<T>::reference;
    using pointer           = typename std::iterator_traits<T>::pointer;

    IterStepWrapper(T ptr, ptrdiff_t step = 1)
        : m_ptr(ptr)
        , m_step(step)
    { }

    iterator_type& operator++()
    {
        std::advance(m_ptr, m_step);
        return *this;
    }

    iterator_type operator++(int)
    {
        auto i = *this;
        ++(*this);
        return i;
    }

    reference operator*() { return *m_ptr; }
    pointer operator->() { return m_ptr; }
    bool operator==(const iterator_type& other)
    {
        return
            m_ptr == other.m_ptr
            || abs(std::distance(m_ptr, other.m_ptr)) < abs(m_step);
    }
    bool operator!=(const iterator_type& other)
    {
        return !((*this) == other);
    }
private:
    T m_ptr;
    ptrdiff_t m_step;
};

template <class T>
IterStepWrapper<T> MakeStepIter(T ptr, ptrdiff_t step = 1)
{
    return IterStepWrapper<T>(ptr, step);
}

template<class T>
inline bool Res2Bool(T& res2store, T res)
{
    res2store = res;
    return !!res;
}

inline mfxU16 Bool2CO(bool bOptON)
{
    return mfxU16(MFX_CODINGOPTION_OFF - !!bOptON * MFX_CODINGOPTION_ON);
}

inline mfxU8 CO2Flag(mfxU16 CO)
{
    assert(CO); // should be either ON or OFF
    return CO == MFX_CODINGOPTION_ON ? 1 : 0;
}

template<typename T>
inline T TileLog2(T blkSize, T target)
{
    mfxU16 k;
    for (k = 0; (blkSize << k) < target; k++) {
    }
    return k;
}

inline mfxU16 CountTL(const mfxExtAvcTemporalLayers& tl)
{
    typedef decltype(tl.Layer[0]) TRLayer;
    return std::max<mfxU16>(1, (mfxU16)std::count_if(
        tl.Layer, tl.Layer + Size(tl.Layer), [](TRLayer l) { return !!l.Scale; }));
}
} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)
