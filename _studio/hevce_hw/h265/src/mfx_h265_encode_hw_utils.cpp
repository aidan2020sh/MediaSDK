//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_bs.h"
#include "vm_time.h"
#include <algorithm>
#include <functional>
#include <list>
#include <assert.h>

namespace MfxHwH265Encode
{

template<class T, class A> void Insert(A& _to, mfxU32 _where, T const & _what)
{
    memmove(&_to[_where + 1], &_to[_where], sizeof(_to)-(_where + 1) * sizeof(_to[0]));
    _to[_where] = _what;
}

template<class A> void Remove(A& _from, mfxU32 _where, mfxU32 _num = 1)
{
    memmove(&_from[_where], &_from[_where + _num], sizeof(_from)-(_where + _num) * sizeof(_from[0]));
    memset(&_from[sizeof(_from) / sizeof(_from[0]) - _num], IDX_INVALID, sizeof(_from[0]) * _num);
}

mfxU32 CountL1(DpbArray const & dpb, mfxI32 poc)
{
    mfxU32 c = 0;
    for (mfxU32 i = 0; !isDpbEnd(dpb, i); i ++)
        c += dpb[i].m_poc > poc;
    return c;
}

template<class T> T Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    T begin,
    T end,
    bool flush)
{
    T top  = begin;
    T b0 = end; // 1st non-ref B with L1 > 0
    std::vector<T> brefs;

    while ( top != end && (top->m_frameType & MFX_FRAMETYPE_B))
    {
        if (CountL1(dpb, top->m_poc))
        {
            if (top->m_frameType & MFX_FRAMETYPE_REF)
            {
                if (par.isBPyramid())
                    brefs.push_back(top);
                else if (b0 == end || (top->m_poc - b0->m_poc < 2))
                    return top;
            }
            else if (b0 == end)
                b0 = top;
        }

        top ++;
    }

    if (!brefs.empty())
        return brefs[brefs.size() / 2];

    if (b0 != end)
        return b0;

    if (flush && top == end && begin != end)
    {
        top --;
        top->m_frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    }

    return top;
}

MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_core(0)
    , m_isExternal(true)
{
    Zero(*(mfxFrameAllocResponse*)this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    Free();
} 

void MfxFrameAllocResponse::Free()
{
    if (m_core == 0)
        return;

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    mfxCoreParam par = {};

    m_core->GetCoreParam(&par);

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        for (size_t i = 0; i < m_responseQueue.size(); i++)
        {
            fa.Free(fa.pthis, &m_responseQueue[i]);
        }
        m_responseQueue.resize(0);
    }
    else
    {
        if (mids)
        {
            NumFrameActual = m_numFrameActualReturnedByAllocFrames;
            fa.Free(fa.pthis, this);
            mids = 0;
        }
    }
}

mfxStatus MfxFrameAllocResponse::Alloc(
    MFXCoreInterface *     core,
    mfxFrameAllocRequest & req,
    bool                   /*isCopyRequired*/)
{
    mfxFrameAllocator & fa = core->FrameAllocator();
    mfxCoreParam par = {};
    mfxFrameAllocResponse & response = *(mfxFrameAllocResponse*)this;

    core->GetCoreParam(&par);

    req.NumFrameSuggested = req.NumFrameMin;

    if ((par.Impl & 0xF00) == MFX_IMPL_VIA_D3D11)
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = fa.Alloc(fa.pthis, &tmp, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);
            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = fa.Alloc(fa.pthis, &req, &response);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;
    
    m_locked.resize(req.NumFrameMin, 0);
    if (m_locked.size() > 0) memset(&m_locked[0], 0, sizeof(m_locked[0]) * m_locked.size());

    m_core = core;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin;
    m_info = req.Info;
    m_isExternal = false;

    return MFX_ERR_NONE;
} 

mfxU32 MfxFrameAllocResponse::FindFreeResourceIndex(mfxFrameSurface1* external_surf)
{
    if (m_isExternal && external_surf)
    {
        mfxU32 i = 0;

        if (0 == m_mids.size())
            m_info = external_surf->Info;

        for (i = 0; i < m_mids.size(); i ++)
        {
            if (m_mids[i] == external_surf->Data.MemId)
            {
                m_locked[i] = 0;
                return i;
            }
        }

        m_mids.push_back(external_surf->Data.MemId);
        m_locked.push_back(0);

        mids = &m_mids[0];

        m_numFrameActualReturnedByAllocFrames = NumFrameActual = (mfxU16)m_mids.size();

        return i;
    }

    return MfxHwH265Encode::FindFreeResourceIndex(*this);
}

mfxU32 MfxFrameAllocResponse::Lock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return 0;
    assert(m_locked[idx] < 0xffffffff);
    return ++m_locked[idx];
}

void MfxFrameAllocResponse::Unlock()
{
    std::fill(m_locked.begin(), m_locked.end(), 0);
}

mfxU32 MfxFrameAllocResponse::Unlock(mfxU32 idx)
{
    if (idx >= m_locked.size())
        return mfxU32(-1);
    assert(m_locked[idx] > 0);
    return --m_locked[idx];
}

mfxU32 MfxFrameAllocResponse::Locked(mfxU32 idx) const
{
    return (idx < m_locked.size()) ? m_locked[idx] : 1;
}

mfxU32 FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom)
{
    for (mfxU32 i = startingFrom; i < pool.NumFrameActual; i++)
        if (pool.Locked(i) == 0)
            return i;
    return 0xFFFFFFFF;
}

mfxMemId AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    if (index > pool.NumFrameActual)
        return 0;
    pool.Lock(index);
    return pool.mids[index];
}

void ReleaseResource(
    MfxFrameAllocResponse & pool,
    mfxMemId                mid)
{
    for (mfxU32 i = 0; i < pool.NumFrameActual; i++)
    {
        if (pool.mids[i] == mid)
        {
            pool.Unlock(i);
            break;
        }
    }
}

mfxStatus GetNativeHandleToRawSurface(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task,
    mfxHDL &              nativeHandle)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocator & fa = core.FrameAllocator();
    mfxExtOpaqueSurfaceAlloc const & opaq = video.m_ext.Opaque;
    mfxFrameSurface1 * surface = task.m_surf_real;

    nativeHandle = 0;

    if (   video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
        sts = fa.GetHDL(fa.pthis, task.m_midRaw, &nativeHandle);
    else if (   video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY
             || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        sts = fa.GetHDL(fa.pthis, surface->Data.MemId, &nativeHandle);
    else
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    if (nativeHandle == 0)
        return (MFX_ERR_UNDEFINED_BEHAVIOR);

    return sts;
}

mfxStatus CopyRawSurfaceToVideoMemory(
    MFXCoreInterface &    core,
    MfxVideoParam const & video,
    Task const &          task)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxExtOpaqueSurfaceAlloc const & opaq = video.m_ext.Opaque;
    mfxFrameSurface1 * surface = task.m_surf_real;

    if (   video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY 
        || video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && (opaq.In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
    {
        mfxFrameAllocator & fa = core.FrameAllocator();
        mfxFrameData d3dSurf = {};
        mfxFrameData sysSurf = surface->Data;
        d3dSurf.MemId = task.m_midRaw;
        bool needUnlock = false;
        
        mfxFrameSurface1 surfSrc = { {}, video.mfx.FrameInfo, sysSurf };
        mfxFrameSurface1 surfDst = { {}, video.mfx.FrameInfo, d3dSurf };

        if (!sysSurf.Y)
        {
            sts = fa.Lock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
            needUnlock = true;
        }
        
        sts = fa.Lock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);

        sts = core.CopyFrame(&surfDst, &surfSrc);

        if (needUnlock)
        {
            sts = fa.Unlock(fa.pthis, sysSurf.MemId, &surfSrc.Data);
            MFX_CHECK_STS(sts);
        }
        
        sts = fa.Unlock(fa.pthis, d3dSurf.MemId, &surfDst.Data);
        MFX_CHECK_STS(sts);
    }

    return sts;
}

namespace ExtBuffer
{
    bool Construct(mfxVideoParam const & par, mfxExtHEVCParam& buf)
    {
        if (!Construct<mfxVideoParam, mfxExtHEVCParam>(par, buf))
        {
            buf.PicWidthInLumaSamples  = Align(par.mfx.FrameInfo.CropW > 0 ? par.mfx.FrameInfo.CropW : par.mfx.FrameInfo.Width, CODED_PIC_ALIGN_W);
            buf.PicHeightInLumaSamples = Align(par.mfx.FrameInfo.CropH > 0 ? par.mfx.FrameInfo.CropH: par.mfx.FrameInfo.Height, CODED_PIC_ALIGN_H);

            return false;
        }

        return true;
    }
};

MfxVideoParam::MfxVideoParam()
    : BufferSizeInKB  (0)
    , InitialDelayInKB(0)
    , TargetKbps      (0)
    , MaxKbps         (0)
    , LTRInterval     (0)
    , LCUSize         (DEFAULT_LCU_SIZE)
    , InsertHRDInfo   (false)
    , RawRef          (false)
{
    Zero(*(mfxVideoParam*)this);
    Zero(NumRefLX);
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);

    //Copy(m_vps, par.m_vps);
    //Copy(m_sps, par.m_sps);
    //Copy(m_pps, par.m_pps);

    BufferSizeInKB   = par.BufferSizeInKB;
    InitialDelayInKB = par.InitialDelayInKB;
    TargetKbps       = par.TargetKbps;
    MaxKbps          = par.MaxKbps;
    NumRefLX[0]      = par.NumRefLX[0];
    NumRefLX[1]      = par.NumRefLX[1];
    LTRInterval      = par.LTRInterval;
    LCUSize          = par.LCUSize;
    InsertHRDInfo    = par.InsertHRDInfo;
    RawRef           = par.RawRef;
}

MfxVideoParam::MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
}

void MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;

    ExtBuffer::Construct(par, m_ext.HEVCParam);
    ExtBuffer::Construct(par, m_ext.HEVCTiles);
    ExtBuffer::Construct(par, m_ext.Opaque);
    ExtBuffer::Construct(par, m_ext.CO2);
    ExtBuffer::Construct(par, m_ext.CO3);
    ExtBuffer::Construct(par, m_ext.DDI);
    ExtBuffer::Construct(par, m_ext.AVCTL);
}

mfxStatus MfxVideoParam::FillPar(mfxVideoParam& par, bool query)
{
        SyncCalculableToVideoParam();

        par.mfx        = mfx;
        par.AsyncDepth = AsyncDepth;
        par.IOPattern  = IOPattern;
        par.Protected  = Protected;

        return GetExtBuffers(par, query);
}

mfxStatus MfxVideoParam::GetExtBuffers(mfxVideoParam& par, bool query)
{
    mfxStatus sts = MFX_ERR_NONE;

    ExtBuffer::Set(par, m_ext.HEVCParam);
    ExtBuffer::Set(par, m_ext.HEVCTiles);
    ExtBuffer::Set(par, m_ext.Opaque);
    ExtBuffer::Set(par, m_ext.CO2);
    ExtBuffer::Set(par, m_ext.CO3);
    ExtBuffer::Set(par, m_ext.DDI);
    ExtBuffer::Set(par, m_ext.AVCTL);

    mfxExtCodingOptionSPSPPS* pSPSPPS = ExtBuffer::Get(par);
    if (pSPSPPS && !query)
    {
        MFX_CHECK(pSPSPPS->SPSBuffer, MFX_ERR_NULL_PTR);
        MFX_CHECK(pSPSPPS->PPSBuffer, MFX_ERR_NULL_PTR);

        HeaderPacker packer;
        mfxU8* buf = 0;
        mfxU32 len = 0;

        sts = packer.Reset(*this);
        MFX_CHECK_STS(sts);

        packer.GetSPS(buf, len);
        MFX_CHECK(pSPSPPS->SPSBufSize >= len, MFX_ERR_NOT_ENOUGH_BUFFER);

        memcpy_s(pSPSPPS->SPSBuffer, len, buf, len);
        pSPSPPS->SPSBufSize = (mfxU16)len;

        packer.GetPPS(buf, len);
        MFX_CHECK(pSPSPPS->PPSBufSize >= len, MFX_ERR_NOT_ENOUGH_BUFFER);

        memcpy_s(pSPSPPS->PPSBuffer, len, buf, len);
        pSPSPPS->PPSBufSize = (mfxU16)len;
    }

    return sts;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = Max<mfxU32>(mfx.BRCParamMultiplier, 1);

    BufferSizeInKB = mfx.BufferSizeInKB * multiplier;
    LTRInterval    = 0;
    LCUSize        = DEFAULT_LCU_SIZE;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        InitialDelayInKB = mfx.InitialDelayInKB * multiplier;
        TargetKbps       = mfx.TargetKbps       * multiplier;
        MaxKbps          = mfx.MaxKbps          * multiplier;
    } else
    {
        InitialDelayInKB = 0;
        TargetKbps       = 0;
        MaxKbps          = 0;
    }

    InsertHRDInfo = false;
    RawRef        = false;

    m_slice.resize(0);

    SetTL(m_ext.AVCTL);
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = BufferSizeInKB;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = Max(maxVal32, TargetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
            maxVal32 = Max(maxVal32, Max(MaxKbps, InitialDelayInKB));
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    mfx.BufferSizeInKB     = (mfxU16)CeilDiv(BufferSizeInKB, mfx.BRCParamMultiplier);

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        mfx.TargetKbps = (mfxU16)CeilDiv(TargetKbps, mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = (mfxU16)CeilDiv(InitialDelayInKB, mfx.BRCParamMultiplier);
            mfx.MaxKbps          = (mfxU16)CeilDiv(MaxKbps, mfx.BRCParamMultiplier);
        }
    }
    mfx.NumSlice = (mfxU16)m_slice.size();
}

mfxU8 GetAspectRatioIdc(mfxU16 w, mfxU16 h)
{
    if (w ==   0 || h ==  0) return  0;
    if (w ==   1 && h ==  1) return  1;
    if (w ==  12 && h == 11) return  2;
    if (w ==  10 && h == 11) return  3;
    if (w ==  16 && h == 11) return  4;
    if (w ==  40 && h == 33) return  5;
    if (w ==  24 && h == 11) return  6;
    if (w ==  20 && h == 11) return  7;
    if (w ==  32 && h == 11) return  8;
    if (w ==  80 && h == 33) return  9;
    if (w ==  18 && h == 11) return 10;
    if (w ==  15 && h == 11) return 11;
    if (w ==  64 && h == 33) return 12;
    if (w == 160 && h == 99) return 13;
    if (w ==   4 && h ==  3) return 14;
    if (w ==   3 && h ==  2) return 15;
    if (w ==   2 && h ==  1) return 16;
    return 255;
}

const mfxU8 AspectRatioByIdc[][2] =
{
    {  0,  0},
    {  1,  1},
    { 12, 11},
    { 10, 11},
    { 16, 11},
    { 40, 33},
    { 24, 11},
    { 20, 11},
    { 32, 11},
    { 80, 33},
    { 18, 11},
    { 15, 11},
    { 64, 33},
    {160, 99},
    {  4,  3},
    {  3,  2},
    {  2,  1},
};

struct FakeTask
{
    mfxI32 m_poc;
    mfxU16 m_frameType;
    mfxU8  m_tid;
};

struct STRPSFreq : STRPS
{
    STRPSFreq(STRPS const & rps, mfxU32 n)
    {
        *(STRPS*)this = rps;
        N = n;
    }
    mfxU32 N;
};

class EqSTRPS
{
private:
    STRPS const & m_ref;
public:
    EqSTRPS(STRPS const & ref) : m_ref(ref) {};
    bool operator () (STRPSFreq const & cur) { return Equal<STRPS>(m_ref, cur); };
    EqSTRPS & operator = (EqSTRPS const &) { assert(0); return *this; }
};

bool Greater(STRPSFreq const & l, STRPSFreq const r)
{
    return (l.N > r.N);
}

mfxU32 NBitsUE(mfxU32 b)
{
    mfxU32 n = 1;

     if (!b)
         return n;

    b ++;

    while (b >> n)
        n ++;

    return n * 2 - 1;
};

template<class T> mfxU32 NBits(T const & list, mfxU8 nSet, STRPS const & rps, mfxU8 idx)
{
    mfxU32 n = (idx != 0);
    mfxU32 nPic = mfxU32(rps.num_negative_pics + rps.num_positive_pics);

    if (rps.inter_ref_pic_set_prediction_flag)
    {
        assert(idx > rps.delta_idx_minus1);
        STRPS const & ref = list[idx - rps.delta_idx_minus1 - 1];
        nPic = mfxU32(ref.num_negative_pics + ref.num_positive_pics);

        if (idx == nSet)
            n += NBitsUE(rps.delta_idx_minus1);

        n += 1;
        n += NBitsUE(rps.abs_delta_rps_minus1);
        n += nPic;

        for (mfxU32 i = 0; i <= nPic; i ++)
            if (!rps.pic[i].used_by_curr_pic_flag)
                n ++;

        return n;
    }

    n += NBitsUE(rps.num_negative_pics);
    n += NBitsUE(rps.num_positive_pics);

    for (mfxU32 i = 0; i < nPic; i ++)
        n += NBitsUE(rps.pic[i].delta_poc_sx_minus1) + 1;

    return n;
}

template<class T> void OptimizeSTRPS(T const & list, mfxU8 n, STRPS& oldRPS, mfxU8 idx)
{
    if (idx == 0)
        return;

    STRPS newRPS;
    mfxI8 k = 0, i = 0, j = 0;

    for (k = idx - 1; k >= 0; k --)
    {
        STRPS const & refRPS = list[k];

        if ((refRPS.num_negative_pics + refRPS.num_positive_pics + 1)
             < (oldRPS.num_negative_pics + oldRPS.num_positive_pics))
            continue;

        newRPS = oldRPS;
        newRPS.inter_ref_pic_set_prediction_flag = 1;
        newRPS.delta_idx_minus1 = (idx - k - 1);

        if (newRPS.delta_idx_minus1 > 0 && idx < n)
            break;

        std::list<mfxI16> dPocs[2];
        mfxI16 dPoc = 0;
        bool found = false;

        for (i = 0; i < oldRPS.num_negative_pics + oldRPS.num_positive_pics; i ++)
        {
            dPoc = oldRPS.pic[i].DeltaPocSX;
            if (dPoc)
                dPocs[dPoc > 0].push_back(dPoc);

            for (j = 0; j < refRPS.num_negative_pics + refRPS.num_positive_pics; j ++)
            {
                dPoc = oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX;
                if (dPoc)
                    dPocs[dPoc > 0].push_back(dPoc);
            }
        }

        dPocs[0].sort(std::greater<mfxI16>());
        dPocs[1].sort(std::less<mfxI16>());
        dPocs[0].unique();
        dPocs[1].unique();

        dPoc = 0;

        while ((!dPocs[0].empty() || !dPocs[1].empty()) && !found)
        {
            dPoc *= -1;
            bool positive = (dPoc > 0 && !dPocs[1].empty()) || dPocs[0].empty();
            dPoc = dPocs[positive].front();
            dPocs[positive].pop_front();

            for (i = 0; i <= refRPS.num_negative_pics + refRPS.num_positive_pics; i ++)
                newRPS.pic[i].used_by_curr_pic_flag = newRPS.pic[i].use_delta_flag = 0;

            i = 0;
            for (j = refRPS.num_negative_pics + refRPS.num_positive_pics - 1; 
                 j >= refRPS.num_negative_pics; j --)
            {
                if (   oldRPS.pic[i].DeltaPocSX < 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (dPoc < 0 && oldRPS.pic[i].DeltaPocSX == dPoc)
            {
                j = refRPS.num_negative_pics + refRPS.num_positive_pics;
                newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                newRPS.pic[j].use_delta_flag = 1;
                i ++;
            }

            for (j = 0; j < refRPS.num_negative_pics; j ++)
            {
                if (   oldRPS.pic[i].DeltaPocSX < 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (i != oldRPS.num_negative_pics)
                continue;

            for (j = refRPS.num_negative_pics - 1; j >= 0; j --)
            {
                if (   oldRPS.pic[i].DeltaPocSX > 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            if (dPoc > 0 && oldRPS.pic[i].DeltaPocSX == dPoc)
            {
                j = refRPS.num_negative_pics + refRPS.num_positive_pics;
                newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                newRPS.pic[j].use_delta_flag = 1;
                i ++;
            }

            for (j = refRPS.num_negative_pics;
                 j < refRPS.num_negative_pics + refRPS.num_positive_pics; j ++)
            {
                if (   oldRPS.pic[i].DeltaPocSX > 0
                    && oldRPS.pic[i].DeltaPocSX - refRPS.pic[j].DeltaPocSX == dPoc)
                {
                    newRPS.pic[j].used_by_curr_pic_flag = oldRPS.pic[i].used_by_curr_pic_sx_flag;
                    newRPS.pic[j].use_delta_flag = 1;
                    i ++;
                }
            }

            found = (i == oldRPS.num_negative_pics + oldRPS.num_positive_pics);
        }

        if (found)
        {
            newRPS.delta_rps_sign       = (dPoc < 0);
            newRPS.abs_delta_rps_minus1 = Abs(dPoc) - 1;

            if (NBits(list, n, newRPS, idx) < NBits(list, n, oldRPS, idx))
                oldRPS = newRPS;
        }

        if (idx < n)
            break;
    }
}

void ReduceSTRPS(std::vector<STRPSFreq> & sets, mfxU32 NumSlice)
{
    if (sets.empty())
        return;

    STRPS  rps = sets.back();
    mfxU32 n = sets.back().N; //current RPS used for N frames
    mfxU8  nSet = mfxU8(sets.size());
    mfxU32 bits0 = //bits for RPS in SPS and SSHs
        NBits(sets, nSet, rps, nSet - 1) //bits for RPS in SPS
        + (CeilLog2(nSet) - CeilLog2(nSet - 1)) //diff of bits for STRPS num in SPS
        + (nSet > 1) * (NumSlice * CeilLog2((mfxU32)sets.size()) * n); //bits for RPS idx in SSHs
    
    //emulate removal of current RPS from SPS
    nSet --; 
    rps.inter_ref_pic_set_prediction_flag = 0;
    OptimizeSTRPS(sets, nSet, rps, nSet);

    mfxU32 bits1 = NBits(sets, nSet, rps, nSet) * NumSlice * n;//bits for RPS in SSHs (no RPS in SPS)

    if (bits1 < bits0)
    {
        sets.pop_back();
        ReduceSTRPS(sets, NumSlice);
    }
}

bool isCurrLt(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc)
{
    for (mfxU32 i = 0; i < 2; i ++)
        for (mfxU32 j = 0; j < numRefActive[i]; j ++)
            if (poc == DPB[RPL[i][j]].m_poc)
                return DPB[RPL[i][j]].m_ltr;
    return false;
}

inline bool isCurrLt(Task const & task, mfxI32 poc)
{
    return isCurrLt(task.m_dpb[0], task.m_refPicList, task.m_numRefActive, poc);
}


void MfxVideoParam::SyncHeadersToMfxParam()
{
    mfxFrameInfo& fi = mfx.FrameInfo;
    mfxU16 SubWidthC[4] = { 1, 2, 2, 1 };
    mfxU16 SubHeightC[4] = { 1, 2, 1, 1 };
    mfxU16 cropUnitX = SubWidthC[m_sps.chroma_format_idc];
    mfxU16 cropUnitY = SubHeightC[m_sps.chroma_format_idc];

    mfx.CodecProfile = m_sps.general.profile_idc;
    mfx.CodecLevel = m_sps.general.level_idc / 3;

    if (m_sps.general.tier_flag)
        mfx.CodecLevel |= MFX_TIER_HEVC_HIGH;

    mfx.NumRefFrame = m_sps.sub_layer[0].max_dec_pic_buffering_minus1;
    mfx.GopRefDist  = m_sps.sub_layer[0].max_num_reorder_pics + 1;

    mfx.FrameInfo.ChromaFormat = m_sps.chroma_format_idc;

    m_ext.HEVCParam.PicWidthInLumaSamples  = (mfxU16)m_sps.pic_width_in_luma_samples;
    m_ext.HEVCParam.PicHeightInLumaSamples = (mfxU16)m_sps.pic_height_in_luma_samples;

    fi.Width = Align(m_ext.HEVCParam.PicWidthInLumaSamples, LCUSize);
    fi.Height = Align(m_ext.HEVCParam.PicHeightInLumaSamples, LCUSize);

    fi.CropX = 0;
    fi.CropY = 0;
    fi.CropW = fi.Width;
    fi.CropH = fi.Height;

    if (m_sps.conformance_window_flag)
    {
        fi.CropX += cropUnitX * (mfxU16)m_sps.conf_win_left_offset;
        fi.CropW -= cropUnitX * (mfxU16)m_sps.conf_win_left_offset;
        fi.CropW -= cropUnitX * (mfxU16)m_sps.conf_win_right_offset;
        fi.CropY += cropUnitY * (mfxU16)m_sps.conf_win_top_offset;
        fi.CropH -= cropUnitY * (mfxU16)m_sps.conf_win_top_offset;
        fi.CropH -= cropUnitY * (mfxU16)m_sps.conf_win_bottom_offset;
    }

    fi.BitDepthLuma = m_sps.bit_depth_luma_minus8 + 8;
    fi.BitDepthChroma = m_sps.bit_depth_chroma_minus8 + 8;

    if (m_sps.vui_parameters_present_flag)
    {
        VUI& vui = m_sps.vui;

        if (vui.aspect_ratio_info_present_flag)
        {
            if (vui.aspect_ratio_idc == 255)
            {
                fi.AspectRatioW = vui.sar_width;
                fi.AspectRatioH = vui.sar_height;
            }
            else if (vui.aspect_ratio_idc < sizeof(AspectRatioByIdc) / sizeof(AspectRatioByIdc[0]))
            {
                fi.AspectRatioW = AspectRatioByIdc[vui.aspect_ratio_idc][0];
                fi.AspectRatioH = AspectRatioByIdc[vui.aspect_ratio_idc][1];
            }
        }

        if (vui.timing_info_present_flag)
        {
            fi.FrameRateExtN = vui.time_scale;
            fi.FrameRateExtD = vui.num_units_in_tick;
        }

        if (vui.default_display_window_flag)
        {
            fi.CropX += cropUnitX * (mfxU16)vui.def_disp_win_left_offset;
            fi.CropW -= cropUnitX * (mfxU16)vui.def_disp_win_left_offset;
            fi.CropW -= cropUnitX * (mfxU16)vui.def_disp_win_right_offset;
            fi.CropY += cropUnitY * (mfxU16)vui.def_disp_win_top_offset;
            fi.CropH -= cropUnitY * (mfxU16)vui.def_disp_win_top_offset;
            fi.CropH -= cropUnitY * (mfxU16)vui.def_disp_win_bottom_offset;
        }

        if (m_sps.vui.hrd_parameters_present_flag)
        {
            HRDInfo& hrd = m_sps.vui.hrd;
            HRDInfo::SubLayer& sl0 = hrd.sl[0];
            HRDInfo::SubLayer::CPB& cpb0 = sl0.cpb[0];

            MaxKbps = ((cpb0.bit_rate_value_minus1 + 1) << (6 + hrd.bit_rate_scale)) / 1000;
            BufferSizeInKB = ((cpb0.cpb_size_value_minus1 + 1) << (4 + hrd.cpb_size_scale)) / 8000;

            if (cpb0.cbr_flag)
                mfx.RateControlMethod = MFX_RATECONTROL_CBR;
            else
                mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        }
    }

    NumRefLX[0] = m_pps.num_ref_idx_l0_default_active_minus1 + 1;
    NumRefLX[1] = m_pps.num_ref_idx_l1_default_active_minus1 + 1;

    if (m_pps.tiles_enabled_flag)
    {
        m_ext.HEVCTiles.NumTileColumns = m_pps.num_tile_columns_minus1 + 1;
        m_ext.HEVCTiles.NumTileRows = m_pps.num_tile_rows_minus1 + 1;
    }
}

void MfxVideoParam::SyncMfxToHeadersParam()
{
    PTL& general = m_vps.general;
    SubLayerOrdering& slo = m_vps.sub_layer[0];

    Zero(m_vps);
    m_vps.video_parameter_set_id    = 0;
    m_vps.reserved_three_2bits      = 3;
    m_vps.max_layers_minus1         = 0;
    m_vps.max_sub_layers_minus1     = mfxU16(NumTL() - 1);
    m_vps.temporal_id_nesting_flag  = 1;
    m_vps.reserved_0xffff_16bits    = 0xFFFF;
    m_vps.sub_layer_ordering_info_present_flag = 0;

    m_vps.timing_info_present_flag          = 1;
    m_vps.num_units_in_tick                 = mfx.FrameInfo.FrameRateExtD;
    m_vps.time_scale                        = mfx.FrameInfo.FrameRateExtN;
    m_vps.poc_proportional_to_timing_flag   = 0;
    m_vps.num_hrd_parameters                = 0;

    general.profile_space               = 0;
    general.tier_flag                   = !!(mfx.CodecLevel & MFX_TIER_HEVC_HIGH);
    general.profile_idc                 = (mfxU8)mfx.CodecProfile;
    general.profile_compatibility_flags = 0;
    general.progressive_source_flag     = !!(mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE);
    general.interlaced_source_flag      = !!(mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF));
    general.non_packed_constraint_flag  = 0;
    general.frame_only_constraint_flag  = 0;
    general.level_idc                   = (mfxU8)(mfx.CodecLevel & 0xFF) * 3;

    slo.max_dec_pic_buffering_minus1    = mfx.NumRefFrame;
    slo.max_num_reorder_pics            = mfx.GopRefDist - 1;
    slo.max_latency_increase_plus1      = 0;

    Zero(m_sps);
    ((LayersInfo&)m_sps) = ((LayersInfo&)m_vps);
    m_sps.video_parameter_set_id   = m_vps.video_parameter_set_id;
    m_sps.max_sub_layers_minus1    = m_vps.max_sub_layers_minus1;
    m_sps.temporal_id_nesting_flag = m_vps.temporal_id_nesting_flag;

    m_sps.seq_parameter_set_id              = 0;
    m_sps.chroma_format_idc                 = mfx.FrameInfo.ChromaFormat;
    m_sps.separate_colour_plane_flag        = 0;
    m_sps.pic_width_in_luma_samples         = m_ext.HEVCParam.PicWidthInLumaSamples;
    m_sps.pic_height_in_luma_samples        = m_ext.HEVCParam.PicHeightInLumaSamples;
    m_sps.conformance_window_flag           = 0;
    m_sps.bit_depth_luma_minus8             = Max(0, (mfxI32)mfx.FrameInfo.BitDepthLuma - 8);
    m_sps.bit_depth_chroma_minus8           = Max(0, (mfxI32)mfx.FrameInfo.BitDepthChroma - 8);
    m_sps.log2_max_pic_order_cnt_lsb_minus4 = (mfxU8)Clip3(0, 12, (mfxI32)CeilLog2(slo.max_num_reorder_pics + slo.max_dec_pic_buffering_minus1 + 1) - 3);

    m_sps.log2_min_luma_coding_block_size_minus3   = 0; // SKL
    m_sps.log2_diff_max_min_luma_coding_block_size = CeilLog2(LCUSize) - 3 - m_sps.log2_min_luma_coding_block_size_minus3;
    m_sps.log2_min_transform_block_size_minus2     = 0;
    m_sps.log2_diff_max_min_transform_block_size   = 3;
    m_sps.max_transform_hierarchy_depth_inter      = 2;
    m_sps.max_transform_hierarchy_depth_intra      = 2;
    m_sps.scaling_list_enabled_flag                = 0;
    m_sps.amp_enabled_flag                         = 1;
    m_sps.sample_adaptive_offset_enabled_flag      = 0; // SKL
    m_sps.pcm_enabled_flag                         = 0; // SKL

    assert(0 == m_sps.pcm_enabled_flag);

    {
        mfxU32 MaxPocLsb = (1<<(m_sps.log2_max_pic_order_cnt_lsb_minus4+4));
        std::list<FakeTask> frames;
        std::list<FakeTask>::iterator cur;
        std::vector<STRPSFreq> sets;
        std::vector<STRPSFreq>::iterator it;
        DpbArray dpb = {};
        DpbFrame tmp = {};
        mfxU8 rpl[2][MAX_DPB_SIZE] = {};
        mfxU8 nRef[2] = {};
        STRPS rps;
        mfxI32 STDist = Min<mfxI32>(mfx.GopPicSize, 128);
        bool moreLTR = !!LTRInterval;

        Fill(dpb, IDX_INVALID);

        for (mfxU32 i = 0; (moreLTR || sets.size() != 64); i ++)
        {
            FakeTask new_frame = {i, GetFrameType(*this, i), 0};

            frames.push_back(new_frame);

            cur = Reorder(*this, dpb, frames.begin(), frames.end(), false);

            if (cur == frames.end())
                continue;

            if (isTL())
            {
                cur->m_tid = GetTId(cur->m_poc);

                if (HighestTId() == cur->m_tid)
                    cur->m_frameType &= ~MFX_FRAMETYPE_REF;

                for (mfxI16 j = 0; !isDpbEnd(dpb, j); j++)
                    if (dpb[j].m_tid > 0 && dpb[j].m_tid >= cur->m_tid)
                        Remove(dpb, j--);
            }

            if (   (i > 0 && (cur->m_frameType & MFX_FRAMETYPE_I))
                || (!moreLTR && cur->m_poc >= STDist))
                break;

            if (!(cur->m_frameType & MFX_FRAMETYPE_I) && cur->m_poc < STDist)
            {
                ConstructRPL(*this, dpb, !!(cur->m_frameType & MFX_FRAMETYPE_B), cur->m_poc, cur->m_tid, rpl, nRef);

                Zero(rps);
                ConstructSTRPS(dpb, rpl, nRef, cur->m_poc, rps);

                it = std::find_if(sets.begin(), sets.end(), EqSTRPS(rps));

                if (it == sets.end())
                    sets.push_back(STRPSFreq(rps, 1));
                else
                    it->N ++;
            }

            if (cur->m_frameType & MFX_FRAMETYPE_REF)
            {
                if (moreLTR)
                {
                    for (mfxU16 j = 0; !isDpbEnd(dpb,j); j ++)
                    {
                        if (dpb[j].m_ltr)
                        {
                            mfxU32 dPocCycleMSB = (cur->m_poc / MaxPocLsb - dpb[j].m_poc / MaxPocLsb);
                            mfxU32 dPocLSB      = dpb[j].m_poc - (cur->m_poc - dPocCycleMSB * MaxPocLsb - Lsb(cur->m_poc, MaxPocLsb));
                            bool skip           = false;

                            if (mfxU32(m_sps.log2_max_pic_order_cnt_lsb_minus4+5) <= CeilLog2(m_sps.num_long_term_ref_pics_sps))
                            {
                                moreLTR = false;
                            }
                            else
                            {
                                for (mfxU32 k = 0; k < m_sps.num_long_term_ref_pics_sps; k ++)
                                {
                                    if (m_sps.lt_ref_pic_poc_lsb_sps[k] == dPocLSB)
                                    {
                                        moreLTR = !(cur->m_poc >= (mfxI32)MaxPocLsb);
                                        skip    = true;
                                        break;
                                    }
                                }
                            }

                            if (!moreLTR || skip)
                                break;

                            m_sps.lt_ref_pic_poc_lsb_sps[m_sps.num_long_term_ref_pics_sps] = (mfxU16)dPocLSB;
                            m_sps.used_by_curr_pic_lt_sps_flag[m_sps.num_long_term_ref_pics_sps] = isCurrLt(dpb, rpl, nRef, dpb[j].m_poc);
                            m_sps.num_long_term_ref_pics_sps ++;

                            if (m_sps.num_long_term_ref_pics_sps == 32)
                                moreLTR = false;
                        }
                    }
                }

                tmp.m_poc = cur->m_poc;
                tmp.m_tid = cur->m_tid;
                UpdateDPB(*this, tmp, dpb);
            }

            frames.erase(cur);
        }

        std::sort(sets.begin(), sets.end(), Greater);
        assert(sets.size() <= 64);

        for (mfxU8 i = 0; i < sets.size(); i ++)
            OptimizeSTRPS(sets, (mfxU8)sets.size(), sets[i], i);

        ReduceSTRPS(sets, mfx.NumSlice);

        for (it = sets.begin(); it != sets.end(); it ++)
            m_sps.strps[m_sps.num_short_term_ref_pic_sets++] = *it;
        
        m_sps.long_term_ref_pics_present_flag = 1; // !!LTRInterval;
    }

    m_sps.temporal_mvp_enabled_flag             = 1; // SKL ?
    m_sps.strong_intra_smoothing_enabled_flag   = 0; // SKL

    m_sps.vui_parameters_present_flag = 1;

    m_sps.vui.aspect_ratio_info_present_flag = 1;
    if (m_sps.vui.aspect_ratio_info_present_flag)
    {
        m_sps.vui.aspect_ratio_idc = GetAspectRatioIdc(mfx.FrameInfo.AspectRatioW, mfx.FrameInfo.AspectRatioH);
        if (m_sps.vui.aspect_ratio_idc == 255)
        {
            m_sps.vui.sar_width  = mfx.FrameInfo.AspectRatioW;
            m_sps.vui.sar_height = mfx.FrameInfo.AspectRatioH;
        }
    }

    {
        mfxFrameInfo& fi = mfx.FrameInfo;
        mfxU16 SubWidthC[4] = {1,2,2,1};
        mfxU16 SubHeightC[4] = {1,2,1,1};
        mfxU16 cropUnitX = SubWidthC[m_sps.chroma_format_idc];
        mfxU16 cropUnitY = SubHeightC[m_sps.chroma_format_idc];

        m_sps.conf_win_left_offset      = (fi.CropX / cropUnitX);
        m_sps.conf_win_right_offset     = (m_sps.pic_width_in_luma_samples - fi.CropW - fi.CropX) / cropUnitX;
        m_sps.conf_win_top_offset       = (fi.CropY / cropUnitY);
        m_sps.conf_win_bottom_offset    = (m_sps.pic_height_in_luma_samples - fi.CropH - fi.CropY) / cropUnitY;
        m_sps.conformance_window_flag   =    m_sps.conf_win_left_offset
                                          || m_sps.conf_win_right_offset
                                          || m_sps.conf_win_top_offset
                                          || m_sps.conf_win_bottom_offset;
    }

    m_sps.vui.timing_info_present_flag = !!m_vps.timing_info_present_flag;
    if (m_sps.vui.timing_info_present_flag)
    {
        m_sps.vui.num_units_in_tick = m_vps.num_units_in_tick;
        m_sps.vui.time_scale        = m_vps.time_scale;
    }

    //m_sps.vui.frame_field_info_present_flag = 1;

    if (InsertHRDInfo && mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        HRDInfo& hrd = m_sps.vui.hrd;
        HRDInfo::SubLayer& sl0 = hrd.sl[0];
        HRDInfo::SubLayer::CPB& cpb0 = sl0.cpb[0];

        m_sps.vui.hrd_parameters_present_flag = 1;

        hrd.nal_hrd_parameters_present_flag = 1;
        hrd.sub_pic_hrd_params_present_flag = 0;

        hrd.bit_rate_scale = 0;
        hrd.cpb_size_scale = 2;

        hrd.initial_cpb_removal_delay_length_minus1 = 23;
        hrd.au_cpb_removal_delay_length_minus1      = 23;
        hrd.dpb_output_delay_length_minus1          = 23;

        sl0.fixed_pic_rate_general_flag = 1;
        sl0.low_delay_hrd_flag          = 0;
        sl0.cpb_cnt_minus1              = 0;

        cpb0.bit_rate_value_minus1 = ((mfx.MaxKbps * 1000) >> (6 + hrd.bit_rate_scale)) - 1;
        cpb0.cpb_size_value_minus1 = ((mfx.BufferSizeInKB * 8000) >> (4 + hrd.cpb_size_scale)) - 1;
        cpb0.cbr_flag              = (mfx.RateControlMethod == MFX_RATECONTROL_CBR);
    }

    Zero(m_pps);
    m_pps.seq_parameter_set_id = m_sps.seq_parameter_set_id;

    m_pps.pic_parameter_set_id                  = 0;
    m_pps.dependent_slice_segments_enabled_flag = 0;
    m_pps.output_flag_present_flag              = 0;
    m_pps.num_extra_slice_header_bits           = 0;
    m_pps.sign_data_hiding_enabled_flag         = 0;
    m_pps.cabac_init_present_flag               = 0;
    m_pps.num_ref_idx_l0_default_active_minus1  = NumRefLX[0] - 1;
    m_pps.num_ref_idx_l1_default_active_minus1  = 0;
    m_pps.init_qp_minus26                       = 0;
    m_pps.constrained_intra_pred_flag           = 0;
    m_pps.transform_skip_enabled_flag           = 0;
    m_pps.cu_qp_delta_enabled_flag              = (mfx.RateControlMethod == MFX_RATECONTROL_CQP) ? 0 : 1;

    m_pps.cb_qp_offset                          = 0;
    m_pps.cr_qp_offset                          = 0;
    m_pps.slice_chroma_qp_offsets_present_flag  = 0;

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        m_pps.init_qp_minus26 = (mfx.GopRefDist == 1 ? mfx.QPP : mfx.QPB) - 26;
        // m_pps.cb_qp_offset = -1;
        // m_pps.cr_qp_offset = -1;
    }

    m_pps.slice_chroma_qp_offsets_present_flag  = 0;
    m_pps.weighted_pred_flag                    = 0;
    m_pps.weighted_bipred_flag                  = 0;
    m_pps.transquant_bypass_enabled_flag        = 0;
    m_pps.tiles_enabled_flag                    = 0;
    m_pps.entropy_coding_sync_enabled_flag      = 0;

    if (m_ext.HEVCTiles.NumTileColumns * m_ext.HEVCTiles.NumTileRows > 1)
    {
        mfxU16 nCol   = (mfxU16)CeilDiv(m_ext.HEVCParam.PicWidthInLumaSamples,  LCUSize);
        mfxU16 nRow   = (mfxU16)CeilDiv(m_ext.HEVCParam.PicHeightInLumaSamples, LCUSize);
        mfxU16 nTCol  = Max<mfxU16>(m_ext.HEVCTiles.NumTileColumns, 1);
        mfxU16 nTRow  = Max<mfxU16>(m_ext.HEVCTiles.NumTileRows, 1);

        m_pps.tiles_enabled_flag        = 1;
        m_pps.uniform_spacing_flag      = 1;
        m_pps.num_tile_columns_minus1   = nTCol - 1;
        m_pps.num_tile_rows_minus1      = nTRow - 1;
        
        for (mfxU16 i = 0; i < nTCol - 1; i ++)
            m_pps.column_width[i] = ((i + 1) * nCol) / nTCol - (i * nCol) / nTCol;

        for (mfxU16 j = 0; j < nTRow - 1; j ++)
            m_pps.row_height[j] = ((j + 1) * nRow) / nTRow - (j * nRow) / nTRow;
    }

    m_pps.loop_filter_across_slices_enabled_flag      = 0;
    m_pps.deblocking_filter_control_present_flag      = 0;

    if (m_ext.CO2.DisableDeblockingIdc)
    {
        m_pps.deblocking_filter_control_present_flag  = 1;
        m_pps.deblocking_filter_disabled_flag = 1;
        m_pps.deblocking_filter_override_enabled_flag = 0;
    }
    
    m_pps.scaling_list_data_present_flag              = 0;
    m_pps.lists_modification_present_flag             = 1;
    m_pps.log2_parallel_merge_level_minus2            = 0;
    m_pps.slice_segment_header_extension_present_flag = 0;

}

mfxU16 FrameType2SliceType(mfxU32 ft)
{
    if (ft & MFX_FRAMETYPE_B)
        return 0;
    if (ft & MFX_FRAMETYPE_P)
        return 1;
    return 2;
}

bool isCurrRef(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc)
{
    for (mfxU32 i = 0; i < 2; i ++)
        for (mfxU32 j = 0; j < numRefActive[i]; j ++)
            if (poc == DPB[RPL[i][j]].m_poc)
                return true;
    return false;
}

inline bool isCurrRef(Task const & task, mfxI32 poc)
{
    return isCurrRef(task.m_dpb[0], task.m_refPicList, task.m_numRefActive, poc);
}

void ConstructSTRPS(
    DpbArray const & DPB,
    mfxU8 const (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 const (&numRefActive)[2],
    mfxI32 poc,
    STRPS& rps)
{
    mfxU32 i, nRef;

    for (i = 0, nRef = 0; !isDpbEnd(DPB, i); i ++)
    {
        if (DPB[i].m_ltr)
            continue;

        rps.pic[nRef].DeltaPocSX = (mfxI16)(DPB[i].m_poc - poc);
        rps.pic[nRef].used_by_curr_pic_sx_flag = isCurrRef(DPB, RPL, numRefActive, DPB[i].m_poc);

        rps.num_negative_pics += rps.pic[nRef].DeltaPocSX < 0;
        rps.num_positive_pics += rps.pic[nRef].DeltaPocSX > 0;
        nRef ++;
    }

    MFX_SORT_STRUCT(rps.pic, nRef, DeltaPocSX, >);
    MFX_SORT_STRUCT(rps.pic, rps.num_negative_pics, DeltaPocSX, <);

    for (i = 0; i < nRef; i ++)
    {
        mfxI16 prev  = (!i || i == rps.num_negative_pics) ? 0 : rps.pic[i-1].DeltaPocSX;
        rps.pic[i].delta_poc_sx_minus1 = Abs(rps.pic[i].DeltaPocSX - prev) - 1;
    }
}

bool Equal(STRPS const & l, STRPS const & r)
{
    //ignore inter_ref_pic_set_prediction_flag, check only DeltaPocSX

    if (   l.num_negative_pics != r.num_negative_pics
        || l.num_positive_pics != r.num_positive_pics)
        return false;

    for(mfxU8 i = 0; i < l.num_negative_pics + l.num_positive_pics; i ++)
        if (   l.pic[i].DeltaPocSX != r.pic[i].DeltaPocSX
            || l.pic[i].used_by_curr_pic_sx_flag != r.pic[i].used_by_curr_pic_sx_flag)
            return false;

    return true;
}

bool isForcedDeltaPocMsbPresent(
    Task const & prevTask,
    mfxI32 poc,
    mfxU32 MaxPocLsb)
{
    DpbArray const & DPB = prevTask.m_dpb[0];

    if (Lsb(prevTask.m_poc, MaxPocLsb) == Lsb(poc, MaxPocLsb))
        return true;

    for (mfxU16 i = 0; !isDpbEnd(DPB, i); i ++)
        if (DPB[i].m_poc != poc && Lsb(DPB[i].m_poc, MaxPocLsb) == Lsb(poc, MaxPocLsb))
            return true;

    return false;
}

void MfxVideoParam::GetSliceHeader(Task const & task, Task const & prevTask, Slice & s) const
{
    bool  isP   = !!(task.m_frameType & MFX_FRAMETYPE_P);
    bool  isB   = !!(task.m_frameType & MFX_FRAMETYPE_B);

    Zero(s);
    
    s.first_slice_segment_in_pic_flag = 1;
    s.no_output_of_prior_pics_flag    = 0;
    s.pic_parameter_set_id            = m_pps.pic_parameter_set_id;

    if (!s.first_slice_segment_in_pic_flag)
    {
        if (m_pps.dependent_slice_segments_enabled_flag)
        {
            s.dependent_slice_segment_flag = 0;
        }

        s.segment_address = 0;
    }

    s.type = FrameType2SliceType(task.m_frameType);

    if (m_pps.output_flag_present_flag)
        s.pic_output_flag = 1;

    assert(0 == m_sps.separate_colour_plane_flag);

    if (task.m_shNUT != IDR_W_RADL && task.m_shNUT != IDR_N_LP)
    {
        mfxU32 i, j;
        mfxU16 nLTR = 0, nSTR[2] = {}, nDPBST = 0, nDPBLT = 0;
        mfxI32 STR[2][MAX_DPB_SIZE] = {};                           // used short-term references
        mfxI32 LTR[MAX_NUM_LONG_TERM_PICS] = {};                    // used long-term references
        DpbArray const & DPB = task.m_dpb[0];                       // DPB before encoding
        mfxU8 const (&RPL)[2][MAX_DPB_SIZE] = task.m_refPicList;    // Ref. Pic. List

        ConstructSTRPS(DPB, RPL, task.m_numRefActive, task.m_poc, s.strps);

        s.pic_order_cnt_lsb = (task.m_poc & ~(0xFFFFFFFF << (m_sps.log2_max_pic_order_cnt_lsb_minus4 + 4)));

        // count STRs and LTRs in DPB (including unused)
        for (i = 0; !isDpbEnd(DPB, i); i ++)
        {
            nDPBST += !DPB[i].m_ltr;
            nDPBLT += DPB[i].m_ltr;
        }

        //check for suitable ST RPS in SPS
        for (i = 0; i < m_sps.num_short_term_ref_pic_sets; i ++)
        {
            if (Equal(m_sps.strps[i], s.strps))
            {
                //use ST RPS from SPS
                s.short_term_ref_pic_set_sps_flag = 1;
                s.short_term_ref_pic_set_idx      = (mfxU8)i;
                break;
            }
        }

        if (!s.short_term_ref_pic_set_sps_flag)
            OptimizeSTRPS(m_sps.strps, m_sps.num_short_term_ref_pic_sets, s.strps, m_sps.num_short_term_ref_pic_sets);

        for (i = 0; i < mfxU32(s.strps.num_negative_pics + s.strps.num_positive_pics); i ++)
        {
            bool isAfter = (s.strps.pic[i].DeltaPocSX > 0);
            if (s.strps.pic[i].used_by_curr_pic_sx_flag)
                STR[isAfter][nSTR[isAfter]++] = task.m_poc + s.strps.pic[i].DeltaPocSX;
        }

        if (nDPBLT)
        {
            assert(m_sps.long_term_ref_pics_present_flag);

            mfxU32 MaxPocLsb  = (1<<(m_sps.log2_max_pic_order_cnt_lsb_minus4+4));
            mfxU32 dPocCycleMSBprev = 0;
            mfxI32 DPBLT[MAX_DPB_SIZE] = {};
            const mfxI32 InvalidPOC = -9000;

            for (i = 0, nDPBLT = 0; !isDpbEnd(DPB, i); i ++)
                if (DPB[i].m_ltr)
                    DPBLT[nDPBLT++] = DPB[i].m_poc;

            MFX_SORT(DPBLT, nDPBLT, <); // sort for DeltaPocMsbCycleLt (may only increase)

            for (nLTR = 0, j = 0; j < nDPBLT; j ++)
            {
                // insert LTR using lt_ref_pic_poc_lsb_sps 
               for (i = 0; i < m_sps.num_long_term_ref_pics_sps; i ++)
               {
                   mfxU32 dPocCycleMSB = (task.m_poc / MaxPocLsb - DPBLT[j] / MaxPocLsb);
                   mfxU32 dPocLSB      = DPBLT[j] - (task.m_poc - dPocCycleMSB * MaxPocLsb - s.pic_order_cnt_lsb);

                   if (   dPocLSB == m_sps.lt_ref_pic_poc_lsb_sps[i]
                       && isCurrLt(task, DPBLT[j]) == !!m_sps.used_by_curr_pic_lt_sps_flag[i]
                       && dPocCycleMSB >= dPocCycleMSBprev)
                   {
                       Slice::LongTerm & curlt = s.lt[s.num_long_term_sps];

                       curlt.lt_idx_sps                    = i;
                       curlt.used_by_curr_pic_lt_flag      = !!m_sps.used_by_curr_pic_lt_sps_flag[i];
                       curlt.poc_lsb_lt                    = m_sps.lt_ref_pic_poc_lsb_sps[i];
                       curlt.delta_poc_msb_cycle_lt        = dPocCycleMSB - dPocCycleMSBprev;
                       curlt.delta_poc_msb_present_flag    = !!curlt.delta_poc_msb_cycle_lt || isForcedDeltaPocMsbPresent(prevTask, DPBLT[j], MaxPocLsb);
                       dPocCycleMSBprev                    = dPocCycleMSB;

                       s.num_long_term_sps ++;
                       
                       if (curlt.used_by_curr_pic_lt_flag)
                       {
                           assert(nLTR < MAX_NUM_LONG_TERM_PICS); //KW
                           LTR[nLTR++] = DPBLT[j];
                       }

                       DPBLT[j] = InvalidPOC;
                       break;
                   }
               }
            }

            for (j = 0, dPocCycleMSBprev = 0; j < nDPBLT; j ++)
            {
                Slice::LongTerm & curlt = s.lt[s.num_long_term_sps + s.num_long_term_pics];

                if (DPBLT[j] == InvalidPOC)
                    continue; //already inserted using lt_ref_pic_poc_lsb_sps

                mfxU32 dPocCycleMSB = (task.m_poc / MaxPocLsb - DPBLT[j] / MaxPocLsb);
                mfxU32 dPocLSB      = DPBLT[j] - (task.m_poc - dPocCycleMSB * MaxPocLsb - s.pic_order_cnt_lsb);

                assert(dPocCycleMSB >= dPocCycleMSBprev);

                curlt.used_by_curr_pic_lt_flag      = isCurrLt(task, DPBLT[j]);
                curlt.poc_lsb_lt                    = dPocLSB;
                curlt.delta_poc_msb_cycle_lt        = dPocCycleMSB - dPocCycleMSBprev;
                curlt.delta_poc_msb_present_flag    = !!curlt.delta_poc_msb_cycle_lt || isForcedDeltaPocMsbPresent(prevTask, DPBLT[j], MaxPocLsb);
                dPocCycleMSBprev                    = dPocCycleMSB;

                s.num_long_term_pics ++;

                if (curlt.used_by_curr_pic_lt_flag)
                {
                    assert(nLTR < MAX_NUM_LONG_TERM_PICS); //KW
                    LTR[nLTR++] = DPBLT[j];
                }
            }
        }

        if (m_sps.temporal_mvp_enabled_flag)
            s.temporal_mvp_enabled_flag = 1;

        if (m_pps.lists_modification_present_flag)
        {
            mfxU16 rIdx = 0;
            mfxI32 RPLTempX[2][16] = {}; // default ref. list without modifications
            mfxU16 NumRpsCurrTempListX[2] = 
            {
                Max<mfxU16>(nSTR[0] + nSTR[1] + nLTR, task.m_numRefActive[0]),
                Max<mfxU16>(nSTR[0] + nSTR[1] + nLTR, task.m_numRefActive[1])
            };

            for (j = 0; j < 2; j ++)
            {
                rIdx = 0;
                while (rIdx < NumRpsCurrTempListX[j])
                {
                    for (i = 0; i < nSTR[j] && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = STR[j][i];
                    for (i = 0; i < nSTR[!j] && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = STR[!j][i];
                    for (i = 0; i < nLTR && rIdx < NumRpsCurrTempListX[j]; i ++)
                        RPLTempX[j][rIdx++] = LTR[i];
                }

                for (rIdx = 0; rIdx < task.m_numRefActive[j]; rIdx ++)
                {
                    for (i = 0; i < NumRpsCurrTempListX[j] && DPB[RPL[j][rIdx]].m_poc != RPLTempX[j][i]; i ++);
                    s.list_entry_lx[j][rIdx] = (mfxU8)i;
                    s.ref_pic_list_modification_flag_lx[j] |= (i != rIdx);
                }
            }
        }
    }

    if (m_sps.sample_adaptive_offset_enabled_flag)
    {
        s.sao_luma_flag   = 0;
        s.sao_chroma_flag = 0;
    }

    if (isP || isB)
    {
        s.num_ref_idx_active_override_flag = 
           (          m_pps.num_ref_idx_l0_default_active_minus1 + 1 != task.m_numRefActive[0]
            || isB && m_pps.num_ref_idx_l1_default_active_minus1 + 1 != task.m_numRefActive[1]);

        s.num_ref_idx_l0_active_minus1 = task.m_numRefActive[0] - 1;

        if (isB)
            s.num_ref_idx_l1_active_minus1 = task.m_numRefActive[1] - 1;

        if (isB)
            s.mvd_l1_zero_flag = 0;

        if (m_pps.cabac_init_present_flag)
            s.cabac_init_flag = 0;

        if (s.temporal_mvp_enabled_flag)
            s.collocated_from_l0_flag = 1;

        assert(0 == m_pps.weighted_pred_flag);
        assert(0 == m_pps.weighted_bipred_flag);

        s.five_minus_max_num_merge_cand = 0;
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        s.slice_qp_delta = mfxI8(task.m_qpY - (m_pps.init_qp_minus26 + 26));

    if (m_pps.slice_chroma_qp_offsets_present_flag)
    {
        s.slice_cb_qp_offset = 0;
        s.slice_cr_qp_offset = 0;
    }

     s.deblocking_filter_disabled_flag = m_pps.deblocking_filter_disabled_flag; //  needed for DDI
     s.beta_offset_div2     = m_pps.beta_offset_div2; //  needed for DDI
     s.tc_offset_div2       = m_pps.tc_offset_div2;   //  needed for DDI

    if (m_pps.deblocking_filter_override_enabled_flag)
        s.deblocking_filter_override_flag = 0;

    assert(0 == s.deblocking_filter_override_flag);

    s.loop_filter_across_slices_enabled_flag = 0;

    if (m_pps.tiles_enabled_flag || m_pps.entropy_coding_sync_enabled_flag)
    {
        s.num_entry_point_offsets = 0;

        assert(0 == s.num_entry_point_offsets);
    }
}

MfxVideoParam& MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
    return *this;
}

void HRD::Setup(SPS const & sps, mfxU32 InitialDelayInKB)
{
    VUI const & vui = sps.vui;
    HRDInfo const & hrd = sps.vui.hrd;
    HRDInfo::SubLayer::CPB const & cpb0 = hrd.sl[0].cpb[0];

    if (   !sps.vui_parameters_present_flag
        || !sps.vui.hrd_parameters_present_flag
        || !(hrd.nal_hrd_parameters_present_flag || hrd.vcl_hrd_parameters_present_flag))
    {
        m_bIsHrdRequired = false;
        return;
    }

    m_bIsHrdRequired = true;

    m_rcMethod = MFX_RATECONTROL_VBR;
    if (cpb0.cbr_flag)
        m_rcMethod = MFX_RATECONTROL_CBR;

    m_bitrate  = (cpb0.bit_rate_value_minus1 + 1) << (6 + hrd.bit_rate_scale);
    m_hrdIn90k = mfxU32(mfxF64((cpb0.cpb_size_value_minus1 + 1) << (4 + hrd.cpb_size_scale)) / m_bitrate * 90000.);
    m_tick     = mfxF64(vui.time_scale) / vui.num_units_in_tick;
    m_taf_prv  = 0.;
    m_trn_cur  = 8000. * InitialDelayInKB / m_bitrate;
    m_trn_cur = GetInitCpbRemovalDelay() / 90000.;
}

void HRD::Reset(SPS const & sps)
{
    HRDInfo const & hrd = sps.vui.hrd;
    HRDInfo::SubLayer::CPB const & cpb0 = hrd.sl[0].cpb[0];

    if (m_bIsHrdRequired == false)
        return;

    m_bitrate  = (cpb0.bit_rate_value_minus1 + 1) << (6 + hrd.bit_rate_scale);
    m_hrdIn90k = mfxU32(mfxF64((cpb0.cpb_size_value_minus1 + 1) << (4 + hrd.cpb_size_scale)) / m_bitrate * 90000.);
}

void HRD::RemoveAccessUnit(mfxU32 size, mfxU32 bufferingPeriod)
{
    if (m_bIsHrdRequired == false)
        return;

    mfxU32 initDelay = GetInitCpbRemovalDelay();

    mfxF64 tai_earliest = bufferingPeriod
        ? m_trn_cur - (initDelay / 90000.)
        : m_trn_cur - (m_hrdIn90k / 90000.);

    mfxF64 tai_cur = (m_rcMethod == MFX_RATECONTROL_VBR)
        ? Max(m_taf_prv, tai_earliest)
        : m_taf_prv;

    m_taf_prv = tai_cur + 8. * size / m_bitrate;
    m_trn_cur += m_tick;
}

mfxU32 HRD::GetInitCpbRemovalDelay() const
{
    if (m_bIsHrdRequired == false)
        return 0;

    mfxF64 delay = Max(0., m_trn_cur - m_taf_prv);
    mfxU32 initialCpbRemovalDelay = mfxU32(90000 * delay + 0.5);

    return initialCpbRemovalDelay == 0
        ? 1 // should not be equal to 0
        : initialCpbRemovalDelay > m_hrdIn90k && m_rcMethod == MFX_RATECONTROL_VBR
            ? m_hrdIn90k // should not exceed hrd buffer
            : initialCpbRemovalDelay;
}

mfxU32 HRD::GetInitCpbRemovalDelayOffset() const
{
    if (m_bIsHrdRequired == false)
        return 0;

    return m_hrdIn90k - GetInitCpbRemovalDelay();
}

mfxU32 HRD::GetMaxFrameSize(mfxU32 bufferingPeriod) const
{
    if (m_bIsHrdRequired == false)
        return 0;

    mfxU32 initDelay = GetInitCpbRemovalDelay();

    mfxF64 tai_earliest = (bufferingPeriod)
        ? m_trn_cur - (initDelay / 90000.)
        : m_trn_cur - (m_hrdIn90k / 90000.);

    mfxF64 tai_cur = (m_rcMethod == MFX_RATECONTROL_VBR)
        ? Max(m_taf_prv, tai_earliest)
        : m_taf_prv;

    return (mfxU32)((m_trn_cur - tai_cur) * m_bitrate);
}

void TaskManager::Reset(mfxU32 numTask)
{
    if (numTask)
    {
        m_free.resize(numTask);
        m_reordering.resize(0);
        m_encoding.resize(0);
        m_querying.resize(0);
    }
    else
    {
        while (!m_querying.empty())
            vm_time_sleep(1);
    }
}

Task* TaskManager::New()
{
    UMC::AutomaticUMCMutex guard(m_listMutex);
    Task* pTask = 0;

    if (!m_free.empty())
    {
        pTask = &m_free.front();
        m_reordering.splice(m_reordering.end(), m_free, m_free.begin());
    }

    return pTask;
}

Task* TaskManager::Reorder(
    MfxVideoParam const & par,
    DpbArray const & dpb,
    bool flush)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    TaskList::iterator begin = m_reordering.begin();
    TaskList::iterator end   = m_reordering.end();
    TaskList::iterator top   = MfxHwH265Encode::Reorder(par, dpb, begin, end, flush);

    if (top == end)
        return 0;

    top->m_dpb_output_delay = (mfxU32)std::distance(begin, top);

    return &*top;
}

void TaskManager::Submit(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_reordering.begin(); it != m_reordering.end(); it ++)
    {
        if (pTask == &*it)
        {
            m_encoding.splice(m_encoding.end(), m_reordering, it);
            break;
        }
    }
}
Task* TaskManager::GetSubmittedTask()
{
    UMC::AutomaticUMCMutex guard(m_listMutex);
    if (m_encoding.size() > 0)
    {
        return &(*m_encoding.begin());
    }
    return 0;
}
void TaskManager::SubmitForQuery(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_encoding.begin(); it != m_encoding.end(); it ++)
    {
        if (pTask == &*it)
        {
            m_querying.splice(m_querying.end(), m_encoding, it);
            break;
        }
    }
}
bool TaskManager::isSubmittedForQuery(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_querying.begin(); it != m_querying.end(); it ++)
    {
        if (pTask == &*it)
        {
            return true;
        }
    }
    return false;
}

void TaskManager::Ready(Task* pTask)
{
    UMC::AutomaticUMCMutex guard(m_listMutex);

    for (TaskList::iterator it = m_querying.begin(); it != m_querying.end(); it ++)
    {
        if (pTask == &*it)
        {
            m_free.splice(m_free.end(), m_querying, it);
            break;
        }
    }
}

mfxU8 GetFrameType(
    MfxVideoParam const & video,
    mfxU32                frameOrder)
{
    mfxU32 gopOptFlag = video.mfx.GopOptFlag;
    mfxU32 gopPicSize = video.mfx.GopPicSize;
    mfxU32 gopRefDist = video.mfx.GopRefDist;
    mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval);

    if (gopPicSize == 0xffff) //infinite GOP
        idrPicDist = gopPicSize = 0xffffffff;

    if (idrPicDist && frameOrder % idrPicDist == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (!idrPicDist && frameOrder == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

    if (frameOrder % gopPicSize == 0)
        return (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF);

    if (frameOrder % gopPicSize % gopRefDist == 0)
        return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

    //if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        if ((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED) ||
            (idrPicDist && (frameOrder + 1) % idrPicDist == 0))
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF); // switch last B frame to P frame

    if (video.isBPyramid() && (frameOrder % gopPicSize % gopRefDist - 1) % 2 == 1)
        return (MFX_FRAMETYPE_B | MFX_FRAMETYPE_REF);

    return (MFX_FRAMETYPE_B);
}

mfxU8 GetDPBIdxByFO(DpbArray const & DPB, mfxU32 fo)
{
    for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
    if (DPB[i].m_fo == fo)
        return i;
    return mfxU8(MAX_DPB_SIZE);
}

mfxU8 GetDPBIdxByPoc(DpbArray const & DPB, mfxI32 poc)
{
    for (mfxU8 i = 0; !isDpbEnd(DPB, i); i++)
    if (DPB[i].m_poc == poc)
        return i;
    return mfxU8(MAX_DPB_SIZE);
}

void InitDPB(
    Task &        task,
    Task const &  prevTask,
    mfxExtAVCRefListCtrl * pLCtrl)
{
    if (   task.m_poc > task.m_lastIPoc
        && prevTask.m_poc <= prevTask.m_lastIPoc) // 1st TRAIL
    {
        Fill(task.m_dpb[TASK_DPB_ACTIVE], IDX_INVALID);

        mfxU8 idx = GetDPBIdxByPoc(prevTask.m_dpb[TASK_DPB_AFTER], task.m_lastIPoc);

        if (idx < MAX_DPB_SIZE)
            task.m_dpb[TASK_DPB_ACTIVE][0] = prevTask.m_dpb[TASK_DPB_AFTER][idx];
    }
    else
    {
        Copy(task.m_dpb[TASK_DPB_ACTIVE], prevTask.m_dpb[TASK_DPB_AFTER]);
    }

    Copy(task.m_dpb[TASK_DPB_BEFORE], prevTask.m_dpb[TASK_DPB_AFTER]);

    {
        DpbArray& dpb = task.m_dpb[TASK_DPB_ACTIVE];

        for (mfxI16 i = 0; !isDpbEnd(dpb, i); i++)
            if (dpb[i].m_tid > 0 && dpb[i].m_tid >= task.m_tid)
                Remove(dpb, i--);

        if (pLCtrl)
        {
            for (mfxU16 i = 0; i < 16 && pLCtrl->RejectedRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
            {
                mfxU16 idx = GetDPBIdxByFO(dpb, pLCtrl->RejectedRefList[i].FrameOrder);

                if (idx < MAX_DPB_SIZE && dpb[idx].m_ltr)
                    Remove(dpb, idx);
            }
        }
    }
}

void UpdateDPB(
    MfxVideoParam const & par,
    DpbFrame const & task,
    DpbArray & dpb,
    mfxExtAVCRefListCtrl * pLCtrl)
{
    mfxU16 end = 0; // DPB end
    mfxU16 st0 = 0; // first ST ref in DPB

    while (!isDpbEnd(dpb, end)) end ++;
    for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0++);

    // frames stored in DPB in POC ascending order,
    // LTRs before STRs (use LTR-candidate as STR as long as it possible)

    MFX_SORT_STRUCT(dpb, st0, m_poc, > );
    MFX_SORT_STRUCT((dpb + st0), mfxU16(end - st0), m_poc, >);

    // sliding window over STRs
    if (end && end == par.mfx.NumRefFrame)
    {
        if (par.isLowDelay() && st0 == 0)
        {
            for (st0 = 1; ((dpb[st0].m_poc - dpb[0].m_poc) % par.NumRefLX[0]) == 0 && st0 < end; st0++);
        }
        else
        {
            for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0 ++);

            if (par.LTRInterval)
            {
                // mark/replace LTR in DPB
                if (!st0)
                {
                    dpb[st0].m_ltr = true;
                    st0 ++;
                }
                else if (dpb[st0].m_poc - dpb[0].m_poc >= (mfxI32)par.LTRInterval)
                {
                    dpb[st0].m_ltr = true;
                    st0 = 0;
                }
            }
        }

        Remove(dpb, st0 == end ? 0 : st0);
        end --;
    }

    assert(end < MAX_DPB_SIZE); //just for KW
    dpb[end++] = task;

    if (pLCtrl)
    {
        bool sort = false;

        for (st0 = 0; dpb[st0].m_ltr && st0 < end; st0++);

        for (mfxU16 i = 0; i < 16 && pLCtrl->LongTermRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
        {
            mfxU16 idx = GetDPBIdxByFO(dpb, pLCtrl->LongTermRefList[i].FrameOrder);

            if (idx < MAX_DPB_SIZE && !dpb[idx].m_ltr)
            {
                DpbFrame ltr = dpb[idx];
                ltr.m_ltr = true;
                Remove(dpb, idx);
                Insert(dpb, st0, ltr);
                st0++;
                sort = true;
            }
        }

        if (sort)
        {
            MFX_SORT_STRUCT(dpb, st0, m_poc, >);
        }
    }

    if (par.isBPyramid() && (task.m_ldb || task.m_codingType < CODING_TYPE_B))
        for (mfxU16 i = 0; i < end - 1; i ++)
            dpb[i].m_codingType = 0; //don't keep coding types for prev. mini-GOP
}

void ReportDPB(DpbArray const & DPB, mfxExtDPB& report)
{
    report.DPBSize = 0;

    while (!isDpbEnd(DPB, report.DPBSize++));

    for (mfxU32 i = 0; i < report.DPBSize; i++)
    {

        report.DPB[i].FrameOrder = DPB[i].m_fo;
        report.DPB[i].LongTermIdx = DPB[i].m_ltr ? 0 : MFX_LONGTERM_IDX_NO_IDX;
        report.DPB[i].PicType = MFX_PICTYPE_FRAME;
    }
}

bool isLTR(
    DpbArray const & dpb,
    mfxU32 LTRInterval,
    mfxI32 poc)
{
    mfxI32 LTRCandidate = dpb[0].m_poc;

    for (mfxU16 i = 1; !isDpbEnd(dpb, i); i ++)
    {
        if (   dpb[i].m_poc > LTRCandidate
            && dpb[i].m_poc - LTRCandidate >= (mfxI32)LTRInterval)
        {
            LTRCandidate = dpb[i].m_poc;
            break;
        }
    }

    return (poc == LTRCandidate) || (LTRCandidate == 0 && poc >= (mfxI32)LTRInterval);
}

void ConstructRPL(
    MfxVideoParam const & par,
    DpbArray const & DPB,
    bool isB,
    mfxI32 poc,
    mfxU8  tid,
    mfxU8 (&RPL)[2][MAX_DPB_SIZE],
    mfxU8 (&numRefActive)[2],
    mfxExtAVCRefLists * pExtLists,
    mfxExtAVCRefListCtrl * pLCtrl)
{
    mfxU8& l0 = numRefActive[0];
    mfxU8& l1 = numRefActive[1];
    mfxU8 LTR[MAX_DPB_SIZE] = {};
    mfxU8 nLTR = 0;
    mfxU8 NumStRefL0 = (mfxU8)(par.NumRefLX[0]);

    l0 = l1 = 0;

    if (pExtLists)
    {
        for (mfxU32 i = 0; i < pExtLists->NumRefIdxL0Active; i ++)
        {
            mfxU8 idx = GetDPBIdxByFO(DPB, (mfxI32)pExtLists->RefPicList0[i].FrameOrder);

            if (idx < MAX_DPB_SIZE)
            {
                RPL[0][l0 ++] = idx;

                if (l0 == par.NumRefLX[0])
                    break;
            }
        }

        for (mfxU32 i = 0; i < pExtLists->NumRefIdxL1Active; i ++)
        {
            mfxU8 idx = GetDPBIdxByFO(DPB, (mfxI32)pExtLists->RefPicList1[i].FrameOrder);

            if (idx < MAX_DPB_SIZE)
                RPL[0][l1 ++] = idx;

            if (l1 == par.NumRefLX[1])
                break;
        }
    }

    if (l0 == 0)
    {
        l1 = 0;

        for (mfxU8 i = 0; !isDpbEnd(DPB, i); i ++)
        {
            if (DPB[i].m_tid > tid)
                continue;

            if (poc > DPB[i].m_poc)
            {
                if (DPB[i].m_ltr || (par.LTRInterval && isLTR(DPB, par.LTRInterval, DPB[i].m_poc)))
                    LTR[nLTR++] = i;
                else
                    RPL[0][l0++] = i;
            }
            else if (isB)
                RPL[1][l1++] = i;
        }

        if (pLCtrl)
        {
            // reorder STRs to POC descending order
            for (mfxU8 lx = 0; lx < 2; lx++)
                MFX_SORT_COMMON(RPL[lx], numRefActive[lx],
                    DPB[RPL[lx][_i]].m_poc < DPB[RPL[lx][_j]].m_poc);

            while (nLTR)
                RPL[0][l0++] = LTR[--nLTR];
        }
        else
        {
            NumStRefL0 -= !!nLTR;

            if (l0 > NumStRefL0)
            {
                MFX_SORT_COMMON(RPL[0], numRefActive[0], Abs(DPB[RPL[0][_i]].m_poc - poc) < Abs(DPB[RPL[0][_j]].m_poc - poc));
                if (par.isLowDelay())
                {
                    while (l0 > NumStRefL0)
                    {
                        mfxI32 i;

                        for (i = 0; (i < l0) && (((DPB[RPL[0][0]].m_poc - DPB[RPL[0][i]].m_poc) % par.NumRefLX[0]) == 0) ; i++);
                        
                        Remove(RPL[0], (i >= l0 - 1) ? 0 : i);
                        l0--;
                    }
                }
                else
                {
                    Remove(RPL[0], (par.LTRInterval && !nLTR && l0 > 1), l0 - NumStRefL0);
                    l0 = NumStRefL0;
                }
            }
            if (l1 > par.NumRefLX[1])
            {
                MFX_SORT_COMMON(RPL[1], numRefActive[1], Abs(DPB[RPL[1][_i]].m_poc - poc) > Abs(DPB[RPL[1][_j]].m_poc - poc));
                Remove(RPL[1], par.NumRefLX[1], l1 - par.NumRefLX[1]);
                l1 = (mfxU8)par.NumRefLX[1];
            }

            // reorder STRs to POC descending order
            for (mfxU8 lx = 0; lx < 2; lx++)
                MFX_SORT_COMMON(RPL[lx], numRefActive[lx],
                    DPB[RPL[lx][_i]].m_poc < DPB[RPL[lx][_j]].m_poc);

            if (nLTR)
            {
                MFX_SORT(LTR, nLTR, < );
                // use LTR as 2nd reference
                Insert(RPL[0], !!l0, LTR[0]);
                l0++;

                for (mfxU16 i = 1; i < nLTR && l0 < par.NumRefLX[0]; i++, l0++)
                    Insert(RPL[0], l0, LTR[i]);
            }
        }
    }

    assert(l0 > 0);

    if (pLCtrl)
    {
        mfxU16 MaxRef[2] = { par.NumRefLX[0], par.NumRefLX[1] };
        mfxU16 pref[2] = {};

        if (pLCtrl->NumRefIdxL0Active)
            MaxRef[0] = Min(pLCtrl->NumRefIdxL0Active, MaxRef[0]);

        if (pLCtrl->NumRefIdxL1Active)
            MaxRef[1] = Min(pLCtrl->NumRefIdxL1Active, MaxRef[1]);

        for (mfxU16 i = 0; i < 16 && pLCtrl->RejectedRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
        {
            mfxU8 idx = GetDPBIdxByFO(DPB, pLCtrl->RejectedRefList[i].FrameOrder);

            if (idx < MAX_DPB_SIZE)
            {
                for (mfxU16 lx = 0; lx < 2; lx++)
                {
                    for (mfxU16 j = 0; j < numRefActive[lx]; j++)
                    {
                        if (RPL[lx][j] == idx)
                        {
                            Remove(RPL[lx], j);
                            numRefActive[lx]--;
                            break;
                        }
                    }
                }
            }
        }

        for (mfxU16 i = 0; i < 16 && pLCtrl->PreferredRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
        {
            mfxU8 idx = GetDPBIdxByFO(DPB, pLCtrl->PreferredRefList[i].FrameOrder);

            if (idx < MAX_DPB_SIZE)
            {
                for (mfxU16 lx = 0; lx < 2; lx++)
                {
                    for (mfxU16 j = 0; j < numRefActive[lx]; j++)
                    {
                        if (RPL[lx][j] == idx)
                        {
                            Remove(RPL[lx], j);
                            Insert(RPL[lx], pref[lx]++, idx);
                            break;
                        }
                    }
                }
            }
        }

        for (mfxU16 lx = 0; lx < 2; lx++)
        {
            if (numRefActive[lx] > MaxRef[lx])
            {
                Remove(RPL[lx], MaxRef[lx], (numRefActive[lx] - MaxRef[lx]));
                numRefActive[lx] = (mfxU8)MaxRef[lx];
            }
        }

        if (l0 == 0)
            RPL[0][l0++] = 0;
    }

    assert(l0 > 0);

    if (isB && !l1 && l0)
        RPL[1][l1++] = RPL[0][l0-1];

    if (!isB)
    {
        for (mfxU16 i = 0; i < Min<mfxU16>(l0, par.NumRefLX[1]); i ++)
            RPL[1][l1++] = RPL[0][i];
        //for (mfxI16 i = l0 - 1; i >= 0 && l1 < par.NumRefLX[1]; i --)
        //    RPL[1][l1++] = RPL[0][i];
    }
}

mfxU8 GetCodingType(Task const & task)
{
    mfxU8 t = CODING_TYPE_B;

    assert(task.m_frameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));

    if (task.m_frameType & MFX_FRAMETYPE_I)
        return CODING_TYPE_I;

    if (task.m_frameType & MFX_FRAMETYPE_P)
        return CODING_TYPE_P;

    if (task.m_ldb)
        return CODING_TYPE_B;
    
    for (mfxU8 i = 0; i < 2; i ++)
    {
        for (mfxU32 j = 0; j < task.m_numRefActive[i]; j ++)
        {
            if (task.m_dpb[0][task.m_refPicList[i][j]].m_ldb)
                continue; // don't count LDB as B here

            if (task.m_dpb[0][task.m_refPicList[i][j]].m_codingType > CODING_TYPE_B)
                return CODING_TYPE_B2;

            if (task.m_dpb[0][task.m_refPicList[i][j]].m_codingType == CODING_TYPE_B)
                t = CODING_TYPE_B1;
        }
    }

    return t;
}

mfxU8 GetSHNUT(Task const & task)
{
    const bool isI   = !!(task.m_frameType & MFX_FRAMETYPE_I);
    const bool isRef = !!(task.m_frameType & MFX_FRAMETYPE_REF);
    const bool isIDR = !!(task.m_frameType & MFX_FRAMETYPE_IDR);

    if (isIDR)
        return IDR_W_RADL;
    if (isI)
        return CRA_NUT;

    if (task.m_tid > 0)
    {
        if (isRef)
            return TSA_R;
        return TSA_N;
    }

    if (task.m_poc > task.m_lastIPoc)
    {
        if (isRef)
            return TRAIL_R;
        return TRAIL_N;
    }

    if (isRef)
        return RASL_R;
    return RASL_N;
}

void ConfigureTask(
    Task &                task,
    Task const &          prevTask,
    MfxVideoParam const & par)
{
    assert(task.m_bs != 0);
    const bool isI    = !!(task.m_frameType & MFX_FRAMETYPE_I);
    const bool isP    = !!(task.m_frameType & MFX_FRAMETYPE_P);
    const bool isB    = !!(task.m_frameType & MFX_FRAMETYPE_B);
    const bool isIDR  = !!(task.m_frameType & MFX_FRAMETYPE_IDR);

    mfxExtDPB*              pDPBReport = ExtBuffer::Get(*task.m_bs);
    mfxExtAVCRefLists*      pExtLists = ExtBuffer::Get(task.m_ctrl);
    mfxExtAVCRefListCtrl*   pExtListCtrl = ExtBuffer::Get(task.m_ctrl);

    if (par.isTL())
    {
        task.m_tid = isI ? 0 : par.GetTId(task.m_poc - prevTask.m_lastIPoc);

        if (par.HighestTId() == task.m_tid)
            task.m_frameType &= ~MFX_FRAMETYPE_REF;
    }

    const bool isRef = !!(task.m_frameType & MFX_FRAMETYPE_REF);

    // set coding type and QP
    if (isB)
    {
        task.m_qpY = (mfxU8)par.mfx.QPB;
        // if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP && par.isBPyramid() && (isRef) )
        //    task.m_qpY = (mfxU8)((par.mfx.QPB + par.mfx.QPP + 1)/2);
    }
    else if (isP)
    {
        // encode P as GPB
        task.m_qpY = (mfxU8)par.mfx.QPP;
        // if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP && par.isLowDelay() && ((task.m_poc - prevTask.m_lastIPoc) % par.NumRefLX[0]) == 0)
        //    task.m_qpY = (mfxU8)((par.mfx.QPP + par.mfx.QPI + 1)/2);
        task.m_frameType &= ~MFX_FRAMETYPE_P;
        task.m_frameType |= MFX_FRAMETYPE_B;
        task.m_ldb = true;
    }
    else
    {
        assert(task.m_frameType & MFX_FRAMETYPE_I);
        task.m_qpY = (mfxU8)par.mfx.QPI;
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        task.m_qpY = 0;
    else if (task.m_ctrl.QP)
        task.m_qpY = (mfxU8)task.m_ctrl.QP;

    task.m_lastIPoc = prevTask.m_lastIPoc;

    InitDPB(task, prevTask, pExtListCtrl);

    //construct ref lists
    Zero(task.m_numRefActive);
    Fill(task.m_refPicList, IDX_INVALID);

    if (!isI)
    {
        ConstructRPL(par, task.m_dpb[TASK_DPB_ACTIVE], isB, task.m_poc, task.m_tid,
            task.m_refPicList, task.m_numRefActive, pExtLists, pExtListCtrl);
    }

    task.m_codingType = GetCodingType(task);

    if (isIDR)
        task.m_insertHeaders = (INSERT_VPS | INSERT_SPS | INSERT_PPS);
    else
        task.m_insertHeaders = 0;

    if (isIDR && par.m_sps.vui.hrd_parameters_present_flag)
        task.m_insertHeaders |= INSERT_BPSEI;

    if (   par.m_sps.vui.frame_field_info_present_flag
        || par.m_sps.vui.hrd.nal_hrd_parameters_present_flag
        || par.m_sps.vui.hrd.vcl_hrd_parameters_present_flag)
        task.m_insertHeaders |= INSERT_PTSEI;

    //if (RepeatePPS)  task.m_insertHeaders |= INSERT_PPS;
    //if (AUDelimiter) task.m_insertHeaders |= INSERT_AUD

    task.m_cpb_removal_delay = (isIDR ? 0 : prevTask.m_cpb_removal_delay + 1);

    // update dpb
    if (isIDR)
        Fill(task.m_dpb[TASK_DPB_AFTER], IDX_INVALID);
    else
        Copy(task.m_dpb[TASK_DPB_AFTER], task.m_dpb[TASK_DPB_ACTIVE]);

    if (isRef)
    {
        if (isI)
            task.m_lastIPoc = task.m_poc;

        UpdateDPB(par, task, task.m_dpb[TASK_DPB_AFTER], pExtListCtrl);

        // is current frame will be used as LTR
        if (par.LTRInterval)
            task.m_ltr |= isLTR(task.m_dpb[TASK_DPB_AFTER], par.LTRInterval, task.m_poc);
    }

    if (pDPBReport)
        ReportDPB(task.m_dpb[TASK_DPB_AFTER], *pDPBReport);

    task.m_shNUT = GetSHNUT(task);

    par.GetSliceHeader(task, prevTask, task.m_sh);
    task.m_statusReportNumber = prevTask.m_statusReportNumber + 1;
    task.m_statusReportNumber = (task.m_statusReportNumber == 0) ? 1 : task.m_statusReportNumber;

}

}; //namespace MfxHwH265Encode
