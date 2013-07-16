/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_ENCODE_HW

#include <functional>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <math.h>
#include <limits.h> /* for INT_MIN, INT_MAX, etc. on Linux/Android */

#if defined(MFX_VA_WIN)
#include <atlbase.h>
#endif

#include "cmrt_cross_platform.h"

#include <assert.h>
#include "ippi.h"
#include "vm_time.h"
#include "umc_automatic_mutex.h"
#include "mfx_brc_common.h"
#include "mfx_h264_encode_hw_utils.h"
#include "libmfx_core.h"


using namespace MfxHwH264Encode;

static char chFrameType[] = "?IP?B???";

namespace MfxHwH264Encode
{
    const mfxU32 NUM_CLOCK_TS[9] = { 1, 1, 1, 2, 2, 3, 3, 2, 3 };

    inline bool Less(mfxU32 num1, mfxU32 num2)
    {
        return (num1 - num2) >= 0x80000000;
    }

    void SetSurfaceFree(Surface & surf)
    {
        surf.SetFree(true);
    }

    void SetReconstructFree(Reconstruct & rec)
    {
        rec.SetFree(true);
        rec.m_reference[TFIELD] = false;
        rec.m_reference[BFIELD] = false;
    }

    struct SetReconstructFreeAndDecRef
    {
        explicit SetReconstructFreeAndDecRef(VideoCORE& core) : m_core(core) {}

        void operator() (Reconstruct& rec)
        {
            SetReconstructFree(rec);
            if (rec.m_yuv && rec.m_yuv->Data.Locked > 0)
                m_core.DecreaseReference(&rec.m_yuv->Data);
            rec.m_yuv = 0;
        }

    private:
        void operator=(const SetReconstructFreeAndDecRef&);
        VideoCORE& m_core;
    };

    PairU16 GetPicStruct(
        MfxVideoParam const & video,
        DdiTask const &       task)
    {
        mfxU16 runtPs = task.m_yuv->Info.PicStruct;

        //if (mfxExtVppAuxData const * extAuxData = GetExtBuffer(task.m_ctrl))
        //    if (extAuxData->PicStruct != MFX_PICSTRUCT_UNKNOWN)
        //        runtPs = extAuxData->PicStruct;

        return GetPicStruct(video, runtPs);
    }

    PairU16 GetPicStruct(
        MfxVideoParam const & video,
        mfxU16                runtPs)
    {
        mfxExtCodingOption const * extOpt = GetExtBuffer(video);
        mfxU16 initPs   = video.mfx.FrameInfo.PicStruct;
        mfxU16 framePic = extOpt->FramePicture;
        mfxU16 fieldOut = extOpt->FieldOutput;

        static mfxU16 const PRG  = MFX_PICSTRUCT_PROGRESSIVE;
        static mfxU16 const TFF  = MFX_PICSTRUCT_FIELD_TFF;
        static mfxU16 const BFF  = MFX_PICSTRUCT_FIELD_BFF;
        static mfxU16 const UNK  = MFX_PICSTRUCT_UNKNOWN;
        static mfxU16 const DBL  = MFX_PICSTRUCT_FRAME_DOUBLING;
        static mfxU16 const TRPL = MFX_PICSTRUCT_FRAME_TRIPLING;
        static mfxU16 const REP  = MFX_PICSTRUCT_FIELD_REPEATED;

        PairU16 ps = MakePair(PRG, PRG);

        if (initPs == PRG) { assert(IsOff(fieldOut)); }
        if (initPs == UNK && runtPs == UNK) { assert(!"unsupported picstruct combination"); }

        if (initPs == PRG && runtPs == UNK)
            ps = MakePair(PRG, PRG);
        else if (initPs == PRG && runtPs == PRG)
            ps = MakePair(PRG, PRG);
        else if (initPs == PRG && runtPs == (PRG | DBL))
            ps = MakePair<mfxU16>(PRG, PRG | DBL);
        else if (initPs == PRG && runtPs == (PRG | TRPL))
            ps = MakePair<mfxU16>(PRG, PRG | TRPL);
        else if (initPs == BFF && runtPs == UNK)
            ps = MakePair(BFF, BFF);
        else if ((initPs == BFF || initPs == UNK) && runtPs == BFF)
            ps = MakePair(BFF, BFF);
        else if (initPs == TFF && runtPs == UNK)
            ps = MakePair(TFF, TFF);
        else if ((initPs == TFF || initPs == UNK) && runtPs == TFF)
            ps = MakePair(TFF, TFF);
        else if (initPs == UNK && runtPs == (PRG | BFF))
            ps = MakePair<mfxU16>(PRG, PRG | BFF);
        else if (initPs == UNK && runtPs == (PRG | TFF))
            ps = MakePair<mfxU16>(PRG, PRG | TFF);
        else if (initPs == UNK && runtPs == (PRG | BFF | REP))
            ps = MakePair<mfxU16>(PRG, PRG | BFF | REP);
        else if (initPs == UNK && runtPs == (PRG | TFF | REP))
            ps = MakePair<mfxU16>(PRG, PRG | TFF | REP);
        else if ((initPs == TFF || initPs == UNK) && runtPs == PRG)
            ps = MakePair<mfxU16>(PRG, PRG | TFF);
        else if (initPs == BFF && runtPs == PRG)
            ps = MakePair<mfxU16>(PRG, PRG | BFF);
        else if (initPs == PRG)
            ps = MakePair(PRG, PRG);
        else if (initPs == BFF)
            ps = MakePair(BFF, BFF);
        else if (initPs == TFF)
            ps = MakePair(TFF, TFF);
        else if (initPs == UNK && framePic == MFX_CODINGOPTION_OFF)
            ps = MakePair(TFF, TFF);
        else if (initPs == UNK && framePic != MFX_CODINGOPTION_OFF)
            ps = MakePair(PRG, PRG);

        if (IsOn(fieldOut) && ps[ENC] == PRG)
            ps[ENC] = ps[DISP] = (ps[1] & BFF) ? BFF : TFF;

        return ps;
    }


    PairU8 ExtendFrameType(mfxU32 type)
    {
        mfxU32 type1 = type & 0xff;
        mfxU32 type2 = type >> 8;

        if (type2 == 0)
        {
            type2 = type1 & ~MFX_FRAMETYPE_IDR; // second field can't be IDR

            if (type1 & MFX_FRAMETYPE_I)
            {
                type2 &= ~MFX_FRAMETYPE_I;
                type2 |=  MFX_FRAMETYPE_P;
            }
        }

        return PairU8(type1, type2);
    }

    bool CheckSubMbPartition(mfxExtCodingOptionDDI const * extDdi, mfxU8 frameType)
    {
        if (frameType & MFX_FRAMETYPE_P)
            return IsOff(extDdi->DisablePSubMBPartition);
        if (frameType & MFX_FRAMETYPE_B)
            return IsOff(extDdi->DisablePSubMBPartition);
        return true;
    }

    mfxU8 GetQpValue(
        MfxVideoParam const & par,
        mfxEncodeCtrl const & ctrl,
        mfxU32                frameType)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP /*||
            par.mfx.RateControlMethod == MFX_RATECONTROL_VCM && (frameType & MFX_FRAMETYPE_I)*/)
        {
            if (ctrl.QP > 0)
            {
                // get per frame qp
                return mfxU8(IPP_MIN(ctrl.QP, 51));
            }
            else
            {
                // get per stream qp
                switch (frameType & MFX_FRAMETYPE_IPB)
                {
                case MFX_FRAMETYPE_I: return mfxU8(par.mfx.QPI);
                case MFX_FRAMETYPE_P: return mfxU8(par.mfx.QPP);
                case MFX_FRAMETYPE_B: return mfxU8(par.mfx.QPB);
                default: assert(!"bad frame type (GetQpValue)"); return 0xff;
                }
            }
        }

        return 26;
    }

    bool IsPreferred(mfxExtAVCRefListCtrl const & refPicListCtrl, mfxU32 frameOrder, mfxU32 picStruct)
    {
        for (mfxU8 j = 0; j < 16; j ++)
            if (refPicListCtrl.PreferredRefList[j].FrameOrder == frameOrder &&
                refPicListCtrl.PreferredRefList[j].PicStruct == picStruct)
                return true;

        return false;
    }

    bool IsRejected(mfxExtAVCRefListCtrl const & refPicListCtrl, mfxU32 frameOrder, mfxU32 picStruct)
    {
        for (mfxU8 j = 0; j < 16; j ++)
            if (refPicListCtrl.RejectedRefList[j].FrameOrder == frameOrder &&
                refPicListCtrl.PreferredRefList[j].PicStruct == picStruct)
                return true;

        return false;
    }


    mfxI32 GetPicNum(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU8                            ref)
    {
        Reconstruct const & recFrame = recons[dpb[ref & 127].m_frameIdx];
        return recFrame.m_picNum[ref >> 7];
    }

    mfxI32 GetPicNumF(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU8                            ref)
    {
        Reconstruct const & recFrame = recons[dpb[ref & 127].m_frameIdx];
        return recFrame.m_reference[ref >> 7]
            ? recFrame.m_picNum[ref >> 7]
            : 0x20000;
    }

    mfxU8 GetLongTermPicNum(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU8                            ref)
    {
        Reconstruct const & recFrame = recons[dpb[ref & 127].m_frameIdx];
        return recFrame.m_longTermPicNum[ref >> 7];
    }

    mfxU32 GetLongTermPicNumF(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU8                            ref)
    {
        DpbFrame const    & dpbFrame = dpb[ref & 127];
        Reconstruct const & recFrame = recons[dpbFrame.m_frameIdx];

        return recFrame.m_reference[ref >> 7] && dpbFrame.m_longterm
            ? recFrame.m_longTermPicNum[ref >> 7]
            : 0x20;
    }

    struct BasePredicateForRefPic
    {
        typedef std::vector<Reconstruct> Recons;
        typedef ArrayDpbFrame            Dpb;
        typedef mfxU8                    Arg;
        typedef bool                     Res;

        BasePredicateForRefPic(Recons const & recons, Dpb const & dpb)
        : m_recons(recons)
        , m_dpb(dpb)
        {
        }

        Recons const & m_recons;
        Dpb const &    m_dpb;
    };

    struct RefPicNumIsGreater : public BasePredicateForRefPic
    {
        RefPicNumIsGreater(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 l, mfxU8 r) const
        {
            return Less(
                GetPicNum(m_recons, m_dpb, r),
                GetPicNum(m_recons, m_dpb, l));
        }
    };

    struct LongTermRefPicNumIsLess : public BasePredicateForRefPic
    {
        LongTermRefPicNumIsLess(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 l, mfxU8 r) const
        {
            return Less(
                GetLongTermPicNum(m_recons, m_dpb, l),
                GetLongTermPicNum(m_recons, m_dpb, r));
        }
    };

    struct RefPocIsLess : public BasePredicateForRefPic
    {
        RefPocIsLess(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 l, mfxU8 r) const
        {
            return Less(GetPoc(m_dpb, l), GetPoc(m_dpb, r));
        }
    };

    struct RefPocIsGreater : public BasePredicateForRefPic
    {
        RefPocIsGreater(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 l, mfxU8 r) const
        {
            return Less(GetPoc(m_dpb, r), GetPoc(m_dpb, l));
        }
    };

    struct RefPocIsLessThan : public BasePredicateForRefPic
    {
        RefPocIsLessThan(Recons const & recons, Dpb const & dpb, mfxU32 poc)
        : BasePredicateForRefPic(recons, dpb)
        , m_poc(poc)
        {
        }

        bool operator ()(mfxU8 r) const
        {
            return Less(GetPoc(m_dpb, r), m_poc);
        }

        mfxU32 m_poc;
    };

    struct RefPocIsGreaterThan : public BasePredicateForRefPic
    {
        RefPocIsGreaterThan(Recons const & recons, Dpb const & dpb, mfxU32 poc)
        : BasePredicateForRefPic(recons, dpb)
        , m_poc(poc)
        {
        }

        bool operator ()(mfxU8 r) const
        {
            return Less(m_poc, GetPoc(m_dpb, r));
        }

        mfxU32 m_poc;
    };

    struct RefIsShortTerm : public BasePredicateForRefPic
    {
        RefIsShortTerm(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 r) const
        {
            return m_recons[m_dpb[r & 127].m_frameIdx].m_reference[r >> 7] && !m_dpb[r & 127].m_longterm;
        }
    };

    struct RefIsLongTerm : public BasePredicateForRefPic
    {
        RefIsLongTerm(Recons const & recons, Dpb const & dpb)
        : BasePredicateForRefPic(recons, dpb)
        {
        }

        bool operator ()(mfxU8 r) const
        {
            return m_recons[m_dpb[r & 127].m_frameIdx].m_reference[r >> 7] && m_dpb[r & 127].m_longterm;
        }
    };

    struct RefIsFromHigherTemporalLayer : public BasePredicateForRefPic
    {
        RefIsFromHigherTemporalLayer(Recons const & recons, Dpb const & dpb, mfxU32 currTid)
        : BasePredicateForRefPic(recons, dpb)
        , m_currTid(currTid)
        {
        }

        bool operator ()(mfxU8 r) const
        {
            return m_currTid < m_recons[m_dpb[r & 127].m_frameIdx].m_tid;
        }

        mfxU32 m_currTid;
    };

    template <class T, class U>
    struct LogicalAndHelper
    {
        typedef typename T::Arg Arg;
        typedef typename T::Res Res;

        LogicalAndHelper(T pr1, U pr2)
        : m_pr1(pr1)
        , m_pr2(pr2)
        {
        }

        Res operator ()(Arg arg) const
        {
            return m_pr1(arg) && m_pr2(arg);
        }

        T m_pr1;
        U m_pr2;
    };

    template <class T, class U>
    LogicalAndHelper<T, U> LogicalAnd(T pr1, U pr2)
    {
        return LogicalAndHelper<T, U>(pr1, pr2);
    }

    template <class T>
    struct LogicalNotHelper
    {
        typedef typename T::argument_type Arg;
        typedef typename T::result_type   Res;

        LogicalNotHelper(T pr)
        : m_pr(pr)
        {
        }

        Res operator ()(Arg arg) const
        {
            return !m_pred(arg);
        }

        T m_pr;
    };

    template <class T>
    LogicalNotHelper<T> LogicalNot(T pr)
    {
        return LogicalNotHelper<T>(pr);
    }

    
    bool LongTermInList(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        ArrayU8x33 const &               list)
    {
        return list.End() == std::find_if(list.Begin(), list.End(), RefIsLongTerm(recons, dpb));
    }


    mfxU32 CalcTemporalLayerIndex(MfxVideoParam const & video, mfxI32 frameOrder)
    {
        mfxU32 i = 0;

        if (video.calcParam.numTemporalLayer > 0)
        {
            mfxU32 maxScale = video.calcParam.scale[video.calcParam.numTemporalLayer - 1];
            for (; i < video.calcParam.numTemporalLayer; i++)
                if (frameOrder % (maxScale / video.calcParam.scale[i]) == 0)
                    break;
        }

        return i;
    }
};

/////////////////////////////////////////////////////////////////////////////////
// FrameTypeGenerator

FrameTypeGenerator::FrameTypeGenerator()
    : m_frameOrder (0)    // in display order
    , m_gopOptFlag (0)
    , m_gopPicSize (0)
    , m_gopRefDist (0)
    , m_refBaseDist(0)   // key picture distance
    , m_biPyramid  (0)
    , m_idrDist    (0)
{
}

void FrameTypeGenerator::Init(MfxVideoParam const & video)
{
    m_gopOptFlag = video.mfx.GopOptFlag;
    m_gopPicSize = IPP_MAX(video.mfx.GopPicSize, 1);
    m_gopRefDist = IPP_MAX(video.mfx.GopRefDist, 1);
    m_idrDist    = m_gopPicSize * (video.mfx.IdrInterval + 1);

    mfxExtCodingOptionDDI * extDdi = GetExtBuffer(video);
    m_biPyramid = extDdi->BiPyramid == 3 ? 0 : extDdi->BiPyramid;

    m_frameOrder = 0;
}

namespace
{
    mfxU32 GetEncodingOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
    {
        assert(displayOrder >= begin);
        assert(displayOrder <  end);

        ref = (end - begin > 1);

        mfxU32 pivot = (begin + end) / 2;
        if (displayOrder == pivot)
            return counter;
        else if (displayOrder < pivot)
            return GetEncodingOrder(displayOrder, begin, pivot, counter + 1, ref);
        else
            return GetEncodingOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
    }
}

BiFrameLocation FrameTypeGenerator::GetBiFrameLocation() const
{
    BiFrameLocation loc;

    if (m_biPyramid != 0)
    {
        bool ref = false;
        mfxU32 orderInMiniGop = m_frameOrder % m_gopPicSize % m_gopRefDist - 1;

        loc.encodingOrder = GetEncodingOrder(orderInMiniGop, 0, m_gopRefDist - 1, 0, ref);
        loc.miniGopCount  = m_frameOrder % m_gopPicSize / m_gopRefDist;
        loc.refFrameFlag  = mfxU16(ref ? MFX_FRAMETYPE_REF : 0);
    }

    return loc;
}

PairU8 FrameTypeGenerator::Get() const
{
    mfxU16 keyPicture = (m_refBaseDist && m_frameOrder % m_refBaseDist == 0) ? MFX_FRAMETYPE_KEYPIC : 0;

    if (m_frameOrder == 0)
    {
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR | keyPicture);
    }

    if (m_frameOrder % m_gopPicSize == 0)
    {
        return ExtendFrameType(MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | keyPicture);
    }

    if (m_frameOrder % m_gopPicSize % m_gopRefDist == 0)
    {
        return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF | keyPicture);
    }

    if ((m_gopOptFlag & MFX_GOP_STRICT) == 0)
    {
        if ((m_frameOrder + 1) % m_gopPicSize == 0 && (m_gopOptFlag & MFX_GOP_CLOSED) ||
            (m_frameOrder + 1) % m_idrDist == 0)
        {
            // switch last B frame to P frame
            return ExtendFrameType(MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF | keyPicture);
        }
    }

    return ExtendFrameType(MFX_FRAMETYPE_B | keyPicture);
}

void FrameTypeGenerator::Next()
{
    m_frameOrder = (m_frameOrder + 1) % m_idrDist;
}

void TaskManager::UpdateRefFrames(
    ArrayDpbFrame const & dpb,
    DdiTask const &       task,
    mfxU32                field)
{
    mfxU32 ps = task.GetPicStructForEncode();

    for (mfxU32 i = 0; i < dpb.Size(); i++)
    {
        Reconstruct & ref = m_recons[dpb[i].m_frameIdx];

        if (dpb[i].m_longterm)
        {
            // update longTermPicNum
            if (ps == MFX_PICSTRUCT_PROGRESSIVE)
            {
                ref.m_longTermPicNum.top = ref.m_longTermFrameIdx;
                ref.m_longTermPicNum.bot = ref.m_longTermFrameIdx;
            }
            else
            {
                ref.m_longTermPicNum.top = 2 * ref.m_longTermFrameIdx + mfxU8( !field);
                ref.m_longTermPicNum.bot = 2 * ref.m_longTermFrameIdx + mfxU8(!!field);
            }
        }
        else
        {
            // update frameNumWrap
            if (ref.m_frameNum > task.m_frameNum)
            {
                ref.m_frameNumWrap = ref.m_frameNum - m_frameNumMax;
            }
            else
            {
                ref.m_frameNumWrap = mfxI32(ref.m_frameNum);
            }

            // update picNum
            if (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE)
            {
                ref.m_picNum.top = ref.m_frameNumWrap;
                ref.m_picNum.bot = ref.m_frameNumWrap;
            }
            else
            {
                ref.m_picNum.top = 2 * ref.m_frameNumWrap + ( !field);
                ref.m_picNum.bot = 2 * ref.m_frameNumWrap + (!!field);
            }
        }
    }
}

mfxU32 TaskManager::CountL1Refs(Reconstruct const & bframe) const
{
    mfxU32 l1RefNum = 0;
    for (mfxU32 i = 0; i < m_dpb.Size(); i++)
    {
        if (Less(bframe.m_frameOrder, m_recons[m_dpb[i].m_frameIdx].m_frameOrder))
        {
            l1RefNum++;
        }
    }

    return l1RefNum;
}


void TaskManager::BuildRefPicLists(
    DdiTask & task,
    mfxU32    field)
{
    ArrayU8x33 list0Frm(0xff); // list0 built like current picture is frame
    ArrayU8x33 list1Frm(0xff); // list1 built like current picture is frame

    ArrayU8x33    & list0 = task.m_list0[field];
    ArrayU8x33    & list1 = task.m_list1[field];
    ArrayDpbFrame & dpb   = task.m_dpb[field];

    mfxU32 useRefBasePicFlag = !!(task.m_type[field] & MFX_FRAMETYPE_KEYPIC);
    if (task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE)
        field = TFIELD;

    // update frameNumWrap and picNum of reference frames in dpb
    UpdateRefFrames(dpb, task, field);

    // build lists of reference frame
    if (task.m_type[field] & MFX_FRAMETYPE_IDR)
    {
        // in MVC P or B frame can be IDR
        // its DPB may be not empty
        // however it shouldn't have inter-frame references
    }
    else if (task.m_type[field] & MFX_FRAMETYPE_P)
    {
        // 8.2.4.2.1-2 "Initialisation process for
        // the reference picture list for P and SP slices in frames/fields"
        for (mfxU32 i = 0; i < dpb.Size(); i++)
            if (!dpb[i].m_longterm && (useRefBasePicFlag == dpb[i].m_refBase))
                list0Frm.PushBack(mfxU8(i));

        std::sort(
            list0Frm.Begin(),
            list0Frm.End(),
            RefPicNumIsGreater(m_recons, dpb));

        mfxU8 * firstLongTerm = list0Frm.End();

        for (mfxU32 i = 0; i < dpb.Size(); i++)
            if (dpb[i].m_longterm && (useRefBasePicFlag == dpb[i].m_refBase))
                list0Frm.PushBack(mfxU8(i));

        std::sort(
            firstLongTerm,
            list0Frm.End(),
            LongTermRefPicNumIsLess(m_recons, dpb));
    }
    else if (task.m_type[field] & MFX_FRAMETYPE_B)
    {
        // 8.2.4.2.3-4 "Initialisation process for
        // reference picture lists for B slices in frames/fields"
        for (mfxU32 i = 0; i < dpb.Size(); i++)
        {
            if (!dpb[i].m_longterm && (useRefBasePicFlag == dpb[i].m_refBase))
            {
                if (Less(dpb[i].m_poc[0], task.GetPoc(0)))
                    list0Frm.PushBack(mfxU8(i));
                else
                    list1Frm.PushBack(mfxU8(i));
            }
        }

        std::sort(
            list0Frm.Begin(),
            list0Frm.End(),
            RefPocIsGreater(m_recons, dpb));

        std::sort(
            list1Frm.Begin(),
            list1Frm.End(),
            RefPocIsLess(m_recons, dpb));

        // elements of list1 append list0
        // elements of list0 append list1
        mfxU32 list0Size = list0Frm.Size();
        mfxU32 list1Size = list1Frm.Size();

        for (mfxU32 ref = 0; ref < list1Size; ref++)
            list0Frm.PushBack(list1Frm[ref]);

        for (mfxU32 ref = 0; ref < list0Size; ref++)
            list1Frm.PushBack(list0Frm[ref]);

        mfxU8 * firstLongTermL0 = list0Frm.End();
        mfxU8 * firstLongTermL1 = list1Frm.End();

        for (mfxU32 i = 0; i < dpb.Size(); i++)
        {
            if (dpb[i].m_longterm && (useRefBasePicFlag == dpb[i].m_refBase))
            {
                list0Frm.PushBack(mfxU8(i));
                list1Frm.PushBack(mfxU8(i));
            }
        }

        std::sort(
            firstLongTermL0,
            list0Frm.End(),
            LongTermRefPicNumIsLess(m_recons, dpb));

        std::sort(
            firstLongTermL1,
            list1Frm.End(),
            LongTermRefPicNumIsLess(m_recons, dpb));
    }

    if (task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE)
    {
        // just copy lists
        list0 = list0Frm;
        list1 = list1Frm;
    }
    else
    {
        // for interlaced picture we need to perform
        // 8.2.4.2.5 "Initialisation process for reference picture lists in fields"

        list0.Resize(0);
        list1.Resize(0);

        ProcessFields(field, dpb, list0Frm, list0);
        ProcessFields(field, dpb, list1Frm, list1);
    }

    // "When the reference picture list RefPicList1 has more than one entry
    // and RefPicList1 is identical to the reference picture list RefPicList0,
    // the first two entries RefPicList1[0] and RefPicList1[1] are switched"
    if (list1.Size() > 1 && list0 == list1)
    {
        std::swap(list1[0], list1[1]);
    }

    task.m_initSizeList0[field] = list0.Size();
    task.m_initSizeList1[field] = list1.Size();
}

void TaskManager::ProcessFields(
    mfxU32                field,
    ArrayDpbFrame const & dpb,
    ArrayU8x33 const &    picListFrm,
    ArrayU8x33 &          picListFld) const
{
    // 8.2.4.2.5 "Initialisation process for reference picture lists in fields"
    mfxU32 idxSameParity = 0; // index in frameList
    mfxU32 idxOppositeParity = 0; // index in frameList
    mfxU32 sameParity = field;
    mfxU32 oppositeParity = !field;

    picListFld.Resize(0);

    while (idxSameParity < picListFrm.Size() || idxOppositeParity < picListFrm.Size())
    {
        for (; idxSameParity < picListFrm.Size(); idxSameParity++)
        {
            if (m_recons[dpb[picListFrm[idxSameParity]].m_frameIdx].m_reference[sameParity])
            {
                picListFld.PushBack(picListFrm[idxSameParity]);
                if (field == BFIELD)
                    picListFld.Back() |= 0x80;

                idxSameParity++;
                break;
            }
        }
        for (; idxOppositeParity < picListFrm.Size(); idxOppositeParity++)
        {
            if (m_recons[dpb[picListFrm[idxOppositeParity]].m_frameIdx].m_reference[oppositeParity])
            {
                picListFld.PushBack(picListFrm[idxOppositeParity]);
                if (field == TFIELD)
                    picListFld.Back() |= 0x80;

                idxOppositeParity++;
                break;
            }
        }
    }
}

bool TaskManager::IsSubmitted(DdiTask const & task) const
{
    return task.m_idxBs.top != NO_INDEX && !m_bitstreams[task.m_idxBs.top % m_bitstreams.size()].IsFree();
}

TaskManager::TaskManager()
: m_frameNum(0)
, m_frameNumMax(0)
, m_frameOrderI(0)
, m_frameOrder(0)
, m_frameOrderIdr(0)
, m_cpbRemoval(0)
, m_cpbRemovalBufferingPeriod(0)
, m_numReorderFrames(0)
, m_viewIdx(0)
, m_pushed(0)
{
}

TaskManager::~TaskManager()
{
    Close();
}

void TaskManager::Init(
    VideoCORE *           core,
    MfxVideoParam const & video,
    mfxU32                viewIdx)
{
    m_core    = core;
    m_viewIdx = viewIdx;

    m_frameNum = 0;
    m_frameNumMax = 256;
    m_frameOrder = 0;
    m_frameOrderI = 0;
    m_frameOrderIdr = 0;
    m_idrPicId = 0;

    m_video = video;

    if (m_video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(m_video);
        m_video.IOPattern = (extOpaq->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            ? mfxU16(MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            : mfxU16(MFX_IOPATTERN_IN_VIDEO_MEMORY);
    }

    m_numReorderFrames = GetNumReorderFrames(video);

    m_dpb.Resize(0);

    m_frameTypeGen.Init(m_video);
    m_bitstreams.resize(CalcNumSurfBitstream(m_video));
    m_recons.resize(CalcNumSurfRecon(m_video));
    m_tasks.resize(CalcNumTasks(m_video));

    // need raw surfaces only when input surfaces are in system memory
    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        m_raws.resize(CalcNumSurfRaw(m_video));

    Zero(m_stat);
}

void TaskManager::Reset(MfxVideoParam const & video)
{
    mfxExtSpsHeader const * extSpsNew = GetExtBuffer(video);
    mfxExtSpsHeader const * extSpsOld = GetExtBuffer(m_video);

    if (!Equal(*extSpsNew, *extSpsOld))
    {
        m_dpb.Resize(0);
        m_idrPicId = 0;
        m_frameTypeGen.Init(video);
        Zero(m_stat);

        std::for_each(m_raws.begin(), m_raws.end(), SetSurfaceFree);
        std::for_each(m_bitstreams.begin(), m_bitstreams.end(), SetSurfaceFree);
        std::for_each(m_tasks.begin(), m_tasks.end(), SetReconstructFreeAndDecRef(*m_core));
        std::for_each(m_recons.begin(), m_recons.end(), SetReconstructFreeAndDecRef(*m_core));
    }

    m_video = video;
    m_numReorderFrames = GetNumReorderFrames(m_video);
}

void TaskManager::Close()
{
    UMC::AutomaticUMCMutex guard(m_mutex);
    Clear(m_tasks);
    Clear(m_raws);
    Clear(m_bitstreams);
    Clear(m_recons);
}

template<typename T> mfxU32 FindFreeSurface(std::vector<T> const & vec)
{
    for (size_t j = 0; j < vec.size(); j++)
    {
        if (vec[j].IsFree())
        {
            return (mfxU32)j;
        }
    }

    return NO_INDEX;
}

void TaskManager::SwitchLastB2P()
{
    DdiTask* latestFrame = 0;
    for (size_t i = 0; i < m_tasks.size(); i++)
    {
        DdiTask& task = m_tasks[i];

        if (task.IsFree() || task.m_bs != 0)
        {
            continue;
        }

        if (latestFrame == 0 || Less(latestFrame->m_frameOrder, task.m_frameOrder))
        {
            latestFrame = &task;
        }
    }

    if (latestFrame)
    {
        mfxU8 frameType = latestFrame->GetFrameType();

        if ((frameType & MFX_FRAMETYPE_B) &&
            (CountL1Refs(*latestFrame) == 0))
        {
            latestFrame->m_type.top = latestFrame->m_type.bot = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
        }
    }
}

DdiTask * TaskManager::SelectNextBFrameFromTail()
{
    DdiTask * toEncode = 0;

    for (size_t i = 0; i < m_tasks.size(); i++)
    {
        DdiTask & task = m_tasks[i];

        if (task.IsFree() || IsSubmitted(task))
        {
            continue;
        }

        if ((task.GetFrameType() & MFX_FRAMETYPE_B) == 0)
            return 0;

        if (toEncode == 0 || Less(task.m_frameOrder, toEncode->m_frameOrder))
        {
            toEncode = &task;
        }
    }

    return toEncode;
}

DdiTask * TaskManager::FindFrameToEncode()
{
    DdiTask * toEncode = 0;

    for (size_t i = 0; i < m_tasks.size(); i++)
    {
        DdiTask & task = m_tasks[i];

        if (task.IsFree() || IsSubmitted(task))
        {
            continue;
        }

        if (toEncode == 0 || Less(task.m_frameOrder, toEncode->m_frameOrder))
        {
            if ((task.GetFrameType() & MFX_FRAMETYPE_B) == 0 ||
                (CountL1Refs(task) > 0))
            {
                toEncode = &task;
            }
        }
    }

    if (toEncode)
    {
        mfxU32 idrPicFlag   = !!(toEncode->GetFrameType() & MFX_FRAMETYPE_IDR);
        mfxU32 intraPicFlag = !!(toEncode->GetFrameType() & MFX_FRAMETYPE_I);
        mfxU32 closedGop    = !!(m_video.mfx.GopOptFlag & MFX_GOP_CLOSED);
        mfxU32 strictGop    = !!(m_video.mfx.GopOptFlag & MFX_GOP_STRICT);

        if (!strictGop && (idrPicFlag || intraPicFlag && closedGop))
        {
            // find latest B frame prior to current I frame
            // since gop is closed such B frames will be encoded with only L1 ref
            // since gop is not strict it is possible (and better)
            // to switch latest B frame to P and encode other B frame before new gop begins
            DdiTask * latestBframe = 0;
            for (size_t i = 0; i < m_tasks.size(); i++)
            {
                if (m_tasks[i].IsFree() || IsSubmitted(m_tasks[i]))
                    continue;

                if ((m_tasks[i].GetFrameType() & MFX_FRAMETYPE_B) &&
                    Less(m_tasks[i].m_frameOrder, toEncode->m_frameOrder) &&
                    (latestBframe == 0 || Less(latestBframe->m_frameOrder, m_tasks[i].m_frameOrder)))
                {
                    latestBframe = &m_tasks[i];
                }
            }

            if (latestBframe)
            {
                latestBframe->m_type[0] = latestBframe->m_type[1] = MFX_FRAMETYPE_PREF;
                toEncode = latestBframe;
            }
        }
    }

    if (toEncode && toEncode->GetFrameType() & MFX_FRAMETYPE_B)
    {
        for (size_t i = 0; i < m_tasks.size(); i++)
        {
            if (m_tasks[i].IsFree() || IsSubmitted(m_tasks[i]))
                continue;

            if ((m_tasks[i].GetFrameType() & MFX_FRAMETYPE_B) &&
                m_tasks[i].m_loc.miniGopCount == toEncode->m_loc.miniGopCount &&
                m_tasks[i].m_loc.encodingOrder < toEncode->m_loc.encodingOrder)
            {
                toEncode = &m_tasks[i];
            }
        }
    }

    return toEncode;
}

namespace
{
    struct FindInDpbByExtFrameTag
    {
        FindInDpbByExtFrameTag(
            std::vector<Reconstruct> const & recons,
            mfxU32                           extFrameTag)
        : m_recons(recons)
        , m_extFrameTag(extFrameTag)
        {
        }

        bool operator ()(DpbFrame const & dpbFrame) const
        {
            return m_recons[dpbFrame.m_frameIdx].m_extFrameTag == m_extFrameTag;
        }

        std::vector<Reconstruct> const & m_recons;
        mfxU32                           m_extFrameTag;
    };

    struct OrderByFrameNumWrap
    {
        OrderByFrameNumWrap(
            std::vector<Reconstruct> const & recons)
        : m_recons(recons)
        {
        }

        bool operator ()(DpbFrame const & lhs, DpbFrame const & rhs) const
        {
            if (!lhs.m_longterm && !rhs.m_longterm)
                if (m_recons[lhs.m_frameIdx].m_frameNumWrap < m_recons[rhs.m_frameIdx].m_frameNumWrap)
                    return lhs.m_refBase > rhs.m_refBase;
                else
                    return m_recons[lhs.m_frameIdx].m_frameNumWrap < m_recons[rhs.m_frameIdx].m_frameNumWrap;
            else if (!lhs.m_longterm && rhs.m_longterm)
                return true;
            else if (lhs.m_longterm && !rhs.m_longterm)
                return false;
            else // both long term
                return m_recons[lhs.m_frameIdx].m_longTermPicNum[0] < m_recons[rhs.m_frameIdx].m_longTermPicNum[0];
        }

        std::vector<Reconstruct> const & m_recons;
    };

    struct OrderByDisplayOrder
    {
        OrderByDisplayOrder(
            std::vector<Reconstruct> const & recons)
        : m_recons(recons)
        {
        }

        bool operator ()(DpbFrame const & lhs, DpbFrame const & rhs) const
        {
            return m_recons[lhs.m_frameIdx].m_frameOrder < m_recons[rhs.m_frameIdx].m_frameOrder;
        }

        std::vector<Reconstruct> const & m_recons;
    };
};

void UpdateMaxLongTermFrameIdxPlus1(ArrayU8x8 & arr, mfxU32 curTidx, mfxU32 val)
{
    std::fill(arr.Begin() + curTidx, arr.End(), val);
}

void TaskManager::UpdateDpb(
    DdiTask       & task,
    mfxU32          fieldId,
    ArrayDpbFrame & dpbPostEncoding)
{
    // declare shorter names
    ArrayDpbFrame const &  initDpb  = task.m_dpb[fieldId];
    ArrayDpbFrame &        currDpb  = dpbPostEncoding;
    ArrayU8x8 &            maxLtIdx = currDpb.m_maxLongTermFrameIdxPlus1;
    mfxU32                 type     = task.m_type[fieldId];
    DecRefPicMarkingInfo & marking  = task.m_decRefPicMrk[fieldId];

    // marking commands will be applied to dpbPostEncoding
    // initial dpb stay unchanged
    currDpb = initDpb;

    if ((type & MFX_FRAMETYPE_REF) == 0)
        return; // non-reference frames doesn't change dpb

    mfxExtAVCRefListCtrl const * ctrl = task.m_internalListCtrlPresent
        ? &task.m_internalListCtrl
        : GetExtBuffer(task.m_ctrl);

    if (type & MFX_FRAMETYPE_IDR)
    {
        bool currFrameIsLongTerm = false;

        currDpb.Resize(0);
        UpdateMaxLongTermFrameIdxPlus1(maxLtIdx, 0, 0);

        marking.long_term_reference_flag = 0;

        if (ctrl)
        {
            for (mfxU32 i = 0; i < 16 && ctrl->LongTermRefList[i].FrameOrder != 0xffffffff; i++)
            {
                if (ctrl->LongTermRefList[i].FrameOrder == task.m_extFrameTag)
                {
                    marking.long_term_reference_flag = 1;
                    currFrameIsLongTerm = true;
                    task.m_longTermFrameIdx = 0;
                    break;
                }
            }
        }

        DpbFrame newDpbFrame;
        newDpbFrame.m_frameIdx = mfxU8(task.m_idxRecon);
        newDpbFrame.m_poc      = task.GetPoc();
        newDpbFrame.m_viewIdx  = mfxU16(task.m_viewIdx);
        newDpbFrame.m_longterm = currFrameIsLongTerm;
        currDpb.PushBack(newDpbFrame);
        if (task.m_storeRefBasePicFlag)
        {
            newDpbFrame.m_refBase = 1;
            currDpb.PushBack(newDpbFrame);
        }
        UpdateMaxLongTermFrameIdxPlus1(maxLtIdx, 0, marking.long_term_reference_flag);
    }
    else
    {
        mfxU32 ffid = task.GetFirstField();

        bool currFrameIsAddedToDpb = (fieldId != ffid) && (task.m_type[ffid] & MFX_FRAMETYPE_REF);

        // collect used long-term frame indices
        ArrayU8x16 usedLtIdx;
        usedLtIdx.Resize(16, 0);
        for (mfxU32 i = 0; i < initDpb.Size(); i++)
            if (initDpb[i].m_longterm)
                usedLtIdx[m_recons[initDpb[i].m_frameIdx].m_longTermFrameIdx] = 1;

        // check longterm list
        // when frameOrder is sent first time corresponsing 'short-term' reference is marked 'long-term'
        // when frameOrder is sent second time corresponsing 'long-term' reference is marked 'unused'
        if (ctrl)
        {
            // adaptive marking is supported only for progressive encoding
            assert(task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE);

            for (mfxU32 i = 0; i < 16 && ctrl->RejectedRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
            {
                DpbFrame * ref = std::find_if(
                    currDpb.Begin(),
                    currDpb.End(),
                    FindInDpbByExtFrameTag(m_recons, ctrl->RejectedRefList[i].FrameOrder));

                if (ref != currDpb.End())
                {
                    if (ref->m_longterm)
                    {
                        marking.PushBack(MMCO_LT_TO_UNUSED, m_recons[ref->m_frameIdx].m_longTermPicNum[0]);
                        usedLtIdx[m_recons[ref->m_frameIdx].m_longTermFrameIdx] = 0;
                    }
                    else
                    {
                        Reconstruct const & recon = m_recons[ref->m_frameIdx];
                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldId] - recon.m_picNum[0] - 1);
                    }

                    currDpb.Erase(ref);
                }
            }

            for (mfxU32 i = 0; i < 16 && ctrl->LongTermRefList[i].FrameOrder != MFX_FRAMEORDER_UNKNOWN; i++)
            {
                DpbFrame * dpbFrame = std::find_if(
                    currDpb.Begin(),
                    currDpb.End(),
                    FindInDpbByExtFrameTag(m_recons, ctrl->LongTermRefList[i].FrameOrder));

                if (dpbFrame != currDpb.End() && dpbFrame->m_longterm == 0)
                {
                    Reconstruct & ref = m_recons[dpbFrame->m_frameIdx];

                    // find free long-term frame index
                    mfxU8 longTermIdx = mfxU8(std::find(usedLtIdx.Begin(), usedLtIdx.End(), 0) - usedLtIdx.Begin());
                    assert(longTermIdx != usedLtIdx.Size());
                    if (longTermIdx == usedLtIdx.Size())
                        break;

                    if (longTermIdx >= maxLtIdx[task.m_tidx])
                    {
                        // need to update MaxLongTermFrameIdx
                        assert(longTermIdx < m_video.mfx.NumRefFrame);
                        marking.PushBack(MMCO_SET_MAX_LT_IDX, longTermIdx + 1);
                        UpdateMaxLongTermFrameIdxPlus1(maxLtIdx, task.m_tidx, longTermIdx + 1);
                    }

                    marking.PushBack(MMCO_ST_TO_LT, task.m_picNum[fieldId] - ref.m_picNum[0] - 1, longTermIdx);
                    usedLtIdx[longTermIdx] = 1;
                    ref.m_longTermFrameIdx = longTermIdx;

                    dpbFrame->m_longterm = 1;
                }
                else if (ctrl->LongTermRefList[i].FrameOrder == task.m_extFrameTag)
                {
                    // frame is not in dpb, but it is a current frame
                    // mark it as 'long-term'

                    // first make free space in dpb if it is full
                    if (currDpb.Size() == m_video.mfx.NumRefFrame)
                    {
                        DpbFrame * toRemove = std::min_element(currDpb.Begin(), currDpb.End(), OrderByFrameNumWrap(m_recons));

                        assert(toRemove != currDpb.End());
                        if (toRemove == currDpb.End())
                            break;

                        if (toRemove->m_longterm == 1)
                        {
                            // no short-term reference in dpb
                            // remove oldest long-term
                            toRemove = std::min_element(
                                currDpb.Begin(),
                                currDpb.End(),
                                OrderByDisplayOrder(m_recons));
                            assert(toRemove->m_longterm == 1); // must be longterm ref

                            Reconstruct const & ref = m_recons[toRemove->m_frameIdx];
                            marking.PushBack(MMCO_LT_TO_UNUSED, ref.m_longTermPicNum[0]);
                            usedLtIdx[ref.m_longTermFrameIdx] = 0;
                        }
                        else
                        {
                            Reconstruct const & ref = m_recons[toRemove->m_frameIdx];
                            marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldId] - ref.m_picNum[0] - 1);
                        }

                        currDpb.Erase(toRemove);
                    }

                    // find free long-term frame index
                    mfxU8 longTermIdx = mfxU8(std::find(usedLtIdx.Begin(), usedLtIdx.End(), 0) - usedLtIdx.Begin());
                    assert(longTermIdx != usedLtIdx.Size());
                    if (longTermIdx == usedLtIdx.Size())
                        break;

                    if (longTermIdx >= maxLtIdx[task.m_tidx])
                    {
                        // need to update MaxLongTermFrameIdx
                        assert(longTermIdx < m_video.mfx.NumRefFrame);
                        marking.PushBack(MMCO_SET_MAX_LT_IDX, longTermIdx + 1);
                        UpdateMaxLongTermFrameIdxPlus1(maxLtIdx, task.m_tidx, longTermIdx + 1);
                    }

                    marking.PushBack(MMCO_CURR_TO_LT, longTermIdx);
                    usedLtIdx[longTermIdx] = 1;
                    task.m_longTermFrameIdx = longTermIdx;

                    DpbFrame newDpbFrame;
                    newDpbFrame.m_frameIdx = mfxU8(task.m_idxRecon);
                    newDpbFrame.m_poc      = task.GetPoc();
                    newDpbFrame.m_viewIdx  = mfxU16(task.m_viewIdx);
                    newDpbFrame.m_longterm = true;
                    currDpb.PushBack(newDpbFrame);
                    assert(currDpb.Size() <= m_video.mfx.NumRefFrame);

                    currFrameIsAddedToDpb = true;
                }
            }
        }

        // if first field was a reference then entire frame is already in dpb
        if (!currFrameIsAddedToDpb)
        {
            for (mfxU32 refBase = 0; refBase <= task.m_storeRefBasePicFlag; refBase++)
            {
                if (currDpb.Size() == m_video.mfx.NumRefFrame)
                {
                    DpbFrame * toRemove = std::min_element(currDpb.Begin(), currDpb.End(), OrderByFrameNumWrap(m_recons));
                    assert(toRemove != currDpb.End());
                    if (toRemove == currDpb.End())
                        return;

                    if (toRemove->m_longterm == 1)
                    {
                        // no short-term reference in dpb
                        // remove oldest long-term
                        toRemove = std::min_element(
                            currDpb.Begin(),
                            currDpb.End(),
                            OrderByDisplayOrder(m_recons));
                        assert(toRemove->m_longterm == 1); // must be longterm ref

                        Reconstruct const & ref = m_recons[toRemove->m_frameIdx];
                        marking.PushBack(MMCO_LT_TO_UNUSED, ref.m_longTermPicNum[0]);
                        usedLtIdx[ref.m_longTermFrameIdx] = 0;
                    }
                    else if (marking.mmco.Size() > 0)
                    {
                        // already have mmco commands, sliding window will not be invoked
                        // remove oldest short-term manually
                        Reconstruct const & ref = m_recons[toRemove->m_frameIdx];
                        marking.PushBack(MMCO_ST_TO_UNUSED, task.m_picNum[fieldId] - ref.m_picNum[0] - 1);
                    }

                    currDpb.Erase(toRemove);
                }

                DpbFrame newDpbFrame;
                newDpbFrame.m_frameIdx = mfxU8(task.m_idxRecon);
                newDpbFrame.m_poc      = task.GetPoc();
                newDpbFrame.m_viewIdx  = mfxU16(task.m_viewIdx);
                newDpbFrame.m_longterm = 0;
                newDpbFrame.m_refBase  = (mfxU8)refBase;
                currDpb.PushBack(newDpbFrame);
                assert(currDpb.Size() <= m_video.mfx.NumRefFrame);
            }
        }
    }
}

namespace
{
    void DecideOnRefPicFlag(MfxVideoParam const & video, DdiTask & task)
    {
        mfxU32 numLayers = video.calcParam.numTemporalLayer;
        if (numLayers > 1)
        {
            Pair<mfxU8> & ft = task.m_type;

            mfxU32 lastLayerScale =
                video.calcParam.scale[numLayers - 1] /
                video.calcParam.scale[numLayers - 2];

            if (((ft[0] | ft[1]) & MFX_FRAMETYPE_REF) &&    // one of fields is ref pic
                numLayers > 1 &&                            // more than one temporal layer
                lastLayerScale == 2 &&                      // highest layer is dyadic
                task.m_tidx + 1 == numLayers)               // this is the highest layer
            {
                ft[0] &= ~MFX_FRAMETYPE_REF;
                ft[1] &= ~MFX_FRAMETYPE_REF;
            }
        }
    }

    Reconstruct const * FindOldestRef(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU32                           tidx)
    {
        Reconstruct const * oldest = 0;

        DpbFrame const * i = dpb.Begin();
        DpbFrame const * e = dpb.End();

        for (; i != e; ++i)
            if (recons[i->m_frameIdx].m_tidx == tidx)
                oldest = &recons[i->m_frameIdx];

        for (; i != e; ++i)
            if (recons[i->m_frameIdx].m_tidx == tidx &&
                Less(recons[i->m_frameIdx].m_frameOrder, oldest->m_frameOrder))
                oldest = &recons[i->m_frameIdx];

        return oldest;
    }

    mfxU32 CountRefs(
        std::vector<Reconstruct> const & recons,
        ArrayDpbFrame const &            dpb,
        mfxU32                           tidx)
    {
        mfxU32 counter = 0;

        DpbFrame const * i = dpb.Begin();
        DpbFrame const * e = dpb.End();
        for (; i != e; ++i)
            if (recons[i->m_frameIdx].m_tidx == tidx)
                counter++;
        return counter;
    }

    void CreateAdditionalDpbCommands(
        MfxVideoParam const &            video,
        std::vector<Reconstruct> const & recons,
        DdiTask &                        task)
    {
        task.m_internalListCtrlPresent = false;
        InitExtBufHeader(task.m_internalListCtrl);

        mfxU32 numLayers  = video.calcParam.numTemporalLayer;
        mfxU32 refPicFlag = !!((task.m_type[0] | task.m_type[1]) & MFX_FRAMETYPE_REF);

        if (refPicFlag &&                                   // only ref frames occupy slot in dpb
            video.calcParam.lyncMode == 0 &&                // no long term refs in lync-mode
            numLayers > 1 && task.m_tidx + 1 != numLayers)  // no dpb commands for last-not-based temporal laeyr
        {
            // find oldest ref frame from the same temporal layer
            Reconstruct const * toRemove = FindOldestRef(recons, task.m_dpb[0], task.m_tidx);

            if (toRemove == 0 && task.m_dpb[0].Size() == video.mfx.NumRefFrame)
            {
                // no ref frame from same layer but need to free dpb slot
                // look for oldest frame from the highest layer
                toRemove = FindOldestRef(recons, task.m_dpb[0], numLayers - 1);
                assert(toRemove != 0);
            }

            if (video.mfx.GopRefDist > 1 &&                     // B frames present
                task.m_tidx == 0 &&                             // base layer
                CountRefs(recons, task.m_dpb[0], 0) < 2 &&      // 0 or 1 refs from base layer
                task.m_dpb[0].Size() < video.mfx.NumRefFrame)   // dpb is not full yet
            {
                // this is to keep 2 references from base layer for B frames at next layer
                toRemove = 0;
            }

            if (toRemove)
            {
                task.m_internalListCtrl.RejectedRefList[0].FrameOrder = toRemove->m_frameOrder;
                task.m_internalListCtrl.RejectedRefList[0].PicStruct  = MFX_PICSTRUCT_PROGRESSIVE;
            }

            task.m_internalListCtrl.LongTermRefList[0].FrameOrder = task.m_frameOrder;
            task.m_internalListCtrl.LongTermRefList[0].PicStruct  = MFX_PICSTRUCT_PROGRESSIVE;
            task.m_internalListCtrlPresent = true;
        }
    }
};

mfxStatus TaskManager::AssignTask(
    mfxEncodeCtrl *    ctrl,
    mfxFrameSurface1 * surface,
    mfxBitstream *     bs,
    DdiTask *&         newTask,
    mfxU16             requiredFrameType)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    if (m_pushed != 0)
    {
        // unexpected error happened while encoding previous task
        // need to revert state of TaskManager
        if (m_pushed->m_idxRecon != NO_INDEX)
            SetReconstructFree(m_recons[m_pushed->m_idxRecon]);

        m_pushed->m_bs = 0;
        m_pushed->SetFree(true);
    }

    DdiTask * toEncode = 0;
    if (m_video.mfx.EncodedOrder == 0)
    {
        if (surface != 0)
        {
            PairU8 frameType(requiredFrameType & 0xff, requiredFrameType >> 8);
            if(requiredFrameType == MFX_FRAMETYPE_UNKNOWN)
            {
                frameType = m_frameTypeGen.Get();

                if (ctrl)
                {
                    if ((ctrl->FrameType & MFX_FRAMETYPE_IPB) == MFX_FRAMETYPE_I)
                    {
                        frameType = MakePair(MFX_FRAMETYPE_IREFIDR,
                            (ctrl->FrameType & MFX_FRAMETYPE_xI) ? MFX_FRAMETYPE_xIREF : MFX_FRAMETYPE_xPREF);
                    }
                    else if (mfxExtVppAuxData * extVpp = GetExtBuffer(*ctrl))
                    {
                        if (extVpp->SceneChangeRate > 90)
                            frameType = MakePair(MFX_FRAMETYPE_IREFIDR, MFX_FRAMETYPE_xPREF);
                    }
                }
            }

            if ((m_pushed = PushNewTask(surface, ctrl, frameType, m_frameOrder)) == 0)
            {
                return MFX_WRN_DEVICE_BUSY;
            }
        }

        // find oldest frame to encode
        toEncode = FindFrameToEncode();

        if (toEncode == 0 && surface == 0)
        {
            // it is possible that all buffered frames are B frames
            // so that none of them has L1 reference
            if (m_video.mfx.GopOptFlag & MFX_GOP_STRICT)
                toEncode = SelectNextBFrameFromTail(); // find first B frame from buffer and encode it w/o future refs
            else
            {
                SwitchLastB2P();
                toEncode = FindFrameToEncode();
            }
        }

        if (toEncode == 0)
        {
            if (surface != 0)
            {
                // change state here, but it is ok
                // because when there is no task to be confirmed
                m_frameTypeGen.Next();
                m_frameOrder++;
                m_core->IncreaseReference(&surface->Data);
                m_pushed = 0;
            }

            return MFX_ERR_MORE_DATA; // nothing to encode
        }
    }else
    {
        assert(surface);
        assert(ctrl);
        assert(ctrl->FrameType & MFX_FRAMETYPE_IPB);

        PairU8 frameType = MakePair(ctrl->FrameType & 0xff, ctrl->FrameType >> 8);
        if ((frameType[1] & MFX_FRAMETYPE_xIPB) == 0)
            frameType = ExtendFrameType(ctrl->FrameType);

        if ((m_pushed = PushNewTask(surface, ctrl, frameType, surface->Data.FrameOrder)) == 0)
            return MFX_WRN_DEVICE_BUSY;

        // no reordering in EncodedOrder
        toEncode = m_pushed;
    }

    mfxU32 ffid      = toEncode->GetFirstField();
    mfxU32 picStruct = toEncode->GetPicStructForEncode();

    toEncode->m_pushed        = m_pushed;
    toEncode->m_idrPicId      = m_idrPicId;
    toEncode->m_frameNum      = m_frameNum;
    toEncode->m_frameOrderIdr = (toEncode->m_type[ffid] & MFX_FRAMETYPE_IDR) ? toEncode->m_frameOrder : m_frameOrderIdr;
    toEncode->m_frameOrderI   = (toEncode->m_type[ffid] & MFX_FRAMETYPE_I)   ? toEncode->m_frameOrder : m_frameOrderI;

    toEncode->m_addRepackSize[ffid] = toEncode->m_addRepackSize[!ffid] = 0; // zero compensative padding size

    toEncode->m_picNum.top = toEncode->m_picNum.bot = (picStruct == MFX_PICSTRUCT_PROGRESSIVE)
        ? m_frameNum
        : m_frameNum * 2 + 1;

    mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(m_video);
    toEncode->m_storeRefBasePicFlag = (extSvc->RefBaseDist) ? 1 : 0; // store all base refs if key pictures enabled

    mfxU8 frameType = toEncode->GetFrameType();
    assert(frameType);

    if (frameType & MFX_FRAMETYPE_IDR)
        toEncode->m_frameNum = 0;

    toEncode->m_dpbOutputDelay    = 2 * (toEncode->m_frameOrder + m_numReorderFrames - m_cpbRemoval);
    toEncode->m_cpbRemoval[ ffid] = 2 * (m_cpbRemoval - m_cpbRemovalBufferingPeriod);
    toEncode->m_cpbRemoval[!ffid] = (toEncode->m_type[ffid] & MFX_FRAMETYPE_IDR)
        ? 1
        : toEncode->m_cpbRemoval[ffid] + 1;

    toEncode->m_bs = bs;

    if (m_bitstreams.size() > 0)
    {
        std::vector<Surface>::iterator it = std::find_if(
            m_bitstreams.begin(),
            m_bitstreams.end(),
            std::mem_fun_ref(&Surface::IsFree));
        if (it == m_bitstreams.end())
            return MFX_WRN_DEVICE_BUSY;

        toEncode->m_idxBs[0] = mfxU32(it - m_bitstreams.begin());

        if (picStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            std::vector<Surface>::iterator it2 = std::find_if(
                it + 1,
                m_bitstreams.end(),
                std::mem_fun_ref(&Surface::IsFree));
            if (it2 == m_bitstreams.end())
                return MFX_WRN_DEVICE_BUSY;

            toEncode->m_idxBs[1] = mfxU32(it2 - m_bitstreams.begin());
        }
    }

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
    {
        toEncode->m_idx = FindFreeSurface(m_raws);
        MFX_CHECK(toEncode->m_idx != NO_INDEX, MFX_WRN_DEVICE_BUSY);
    }

    toEncode->m_idxRecon = FindFreeSurface(m_recons);
    MFX_CHECK(toEncode->m_idxRecon != NO_INDEX, MFX_WRN_DEVICE_BUSY);

    toEncode->m_idxReconOffset = CalcNumSurfRecon(m_video) * m_viewIdx;
    toEncode->m_idxBsOffset    = CalcNumSurfBitstream(m_video) * m_viewIdx;

    mfxExtAvcTemporalLayers const * extTemp = GetExtBuffer(m_video);
    toEncode->m_tidx = CalcTemporalLayerIndex(m_video, toEncode->m_frameOrder - toEncode->m_frameOrderIdr);
    toEncode->m_tid  = m_video.calcParam.tid[toEncode->m_tidx];
    toEncode->m_pid  = toEncode->m_tidx + extTemp->BaseLayerPID;

    toEncode->m_decRefPicMrkRep[ffid] = m_decRefPicMrkRep;
    toEncode->m_dpb[ffid]             = m_dpb;
    BuildRefPicLists(*toEncode, ffid);
    ModifyRefPicLists(*toEncode, ffid);

    DecideOnRefPicFlag(m_video, *toEncode); // check for temporal layers
    CreateAdditionalDpbCommands(m_video, m_recons, *toEncode); // for svc temporal layers

    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);
    toEncode->m_subMbPartitionAllowed[0] = CheckSubMbPartition(extDdi, toEncode->m_type[0]);
    toEncode->m_subMbPartitionAllowed[1] = CheckSubMbPartition(extDdi, toEncode->m_type[1]);


    mfxExtCodingOption const * extOpt = GetExtBuffer(m_video);
    toEncode->m_insertSps[ ffid] = (toEncode->m_type[ffid] & MFX_FRAMETYPE_I) ? 1 : 0;
    toEncode->m_insertSps[!ffid] = 0;
    toEncode->m_insertAud[ ffid] = IsOn(extOpt->AUDelimiter);
    toEncode->m_insertAud[!ffid] = IsOn(extOpt->AUDelimiter);

    if (m_video.calcParam.lyncMode)
    {
        toEncode->m_insertPps[ ffid] = toEncode->m_insertSps[ ffid];
        toEncode->m_insertPps[!ffid] = toEncode->m_insertSps[!ffid];

        mfxU32 idrFlag = !!(toEncode->m_type[ ffid] & MFX_FRAMETYPE_IDR);
        mfxU32 refFlag = !!(toEncode->m_type[ ffid] & MFX_FRAMETYPE_REF);
        toEncode->m_nalRefIdc[ ffid] = idrFlag ? 3 : (refFlag ? 2 : 0);

        idrFlag = !!(toEncode->m_type[!ffid] & MFX_FRAMETYPE_IDR);
        refFlag = !!(toEncode->m_type[!ffid] & MFX_FRAMETYPE_REF);
        toEncode->m_nalRefIdc[!ffid] = idrFlag ? 3 : (refFlag ? 2 : 0);
    }
    else
    {
        toEncode->m_insertPps[ ffid] = toEncode->m_insertSps[ ffid] || IsOn(extDdi->RepeatPPS);
        toEncode->m_insertPps[!ffid] = toEncode->m_insertSps[!ffid] || IsOn(extDdi->RepeatPPS);
        toEncode->m_nalRefIdc[ ffid] = !!(toEncode->m_type[ ffid] & MFX_FRAMETYPE_REF);
        toEncode->m_nalRefIdc[!ffid] = !!(toEncode->m_type[!ffid] & MFX_FRAMETYPE_REF);
    }

    toEncode->m_cqpValue[0] = GetQpValue(m_video, toEncode->m_ctrl, toEncode->m_type[0]);
    toEncode->m_cqpValue[1] = GetQpValue(m_video, toEncode->m_ctrl, toEncode->m_type[1]);

    mfxExtMVCSeqDesc * extMvc = GetExtBuffer(m_video);
    toEncode->m_statusReportNumber[0] = 2 * (m_cpbRemoval * extMvc->NumView + m_viewIdx);
    toEncode->m_statusReportNumber[1] = 2 * (m_cpbRemoval * extMvc->NumView + m_viewIdx) + 1;

    toEncode->m_viewIdx = m_viewIdx;

    if (picStruct == MFX_PICSTRUCT_PROGRESSIVE)
    {
        UpdateDpb(*toEncode, ffid, toEncode->m_dpbPostEncoding);

        m_recons[toEncode->m_idxRecon] = *toEncode;
        m_recons[toEncode->m_idxRecon].m_reference[ffid] = (toEncode->m_type[ ffid] & MFX_FRAMETYPE_REF) != 0;
        m_recons[toEncode->m_idxRecon].SetFree(true); // recon surface will be marked used in ConfirmTask()
    }
    else
    {
        toEncode->m_decRefPicMrkRep[!ffid].presentFlag =
            (toEncode->m_type[ffid] & MFX_FRAMETYPE_IDR) ||
            (toEncode->m_decRefPicMrk[ffid].mmco.Size() > 0);

        toEncode->m_decRefPicMrkRep[!ffid].original_idr_flag          = (toEncode->m_type[ffid] & MFX_FRAMETYPE_IDR) ? 1 : 0;
        toEncode->m_decRefPicMrkRep[!ffid].original_frame_num         = (toEncode->m_frameNum);
        toEncode->m_decRefPicMrkRep[!ffid].original_field_pic_flag    = (picStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0 : 1;
        toEncode->m_decRefPicMrkRep[!ffid].original_bottom_field_flag = (ffid == BFIELD) ? 1 : 0;
        toEncode->m_decRefPicMrkRep[!ffid].dec_ref_pic_marking        = toEncode->m_decRefPicMrk[ffid];

        UpdateDpb(*toEncode, ffid, toEncode->m_dpb[!ffid]);
        m_recons[toEncode->m_idxRecon] = *toEncode;
        m_recons[toEncode->m_idxRecon].m_reference[ ffid] = (toEncode->m_type[ ffid] & MFX_FRAMETYPE_REF) != 0;

        BuildRefPicLists(*toEncode, !ffid);
        ModifyRefPicLists(*toEncode, !ffid);

        UpdateDpb(*toEncode, !ffid, toEncode->m_dpbPostEncoding);
        m_recons[toEncode->m_idxRecon].m_reference[!ffid] = (toEncode->m_type[!ffid] & MFX_FRAMETYPE_REF) != 0;
        m_recons[toEncode->m_idxRecon].SetFree(true); // recon surface will be marked used in ConfirmTask()
    }

    newTask = toEncode;
    return MFX_ERR_NONE;
}

DdiTask* TaskManager::PushNewTask(
    mfxFrameSurface1 * surface,
    mfxEncodeCtrl *    ctrl,
    PairU8             type,
    mfxU32             frameOrder)
{
    assert(surface);

    mfxU32 insertAt = FindFreeSurface(m_tasks);
    if (insertAt == NO_INDEX)
        return 0;

    DdiTask task;

    task.m_picStruct = GetPicStruct(m_video, surface->Info.PicStruct);

    mfxU32 ffid = task.GetFirstField();

    task.m_type[ ffid]   = type[0];
    task.m_type[!ffid]   = type[1];
    task.m_frameOrder    = frameOrder;
    task.m_yuv           = surface;
    task.m_extFrameTag   = surface->Data.FrameOrder; // mark task with external FrameOrder

    if (ctrl)
        task.m_ctrl = *ctrl;

    if (type[0] & MFX_FRAMETYPE_B)
    {
        task.m_loc = m_frameTypeGen.GetBiFrameLocation();

        task.m_type[ ffid] |= task.m_loc.refFrameFlag;
        task.m_type[!ffid] |= task.m_loc.refFrameFlag;
    }

    task.SetFree(false);
    m_tasks[insertAt] = task; // change state
                              // if task isn't confirmed by ConfirmTask
                              // it will be popped from list at next AssignTask

    return &m_tasks[insertAt];
}

namespace
{
    mfxU8 * FindByExtFrameTag(
        mfxU8 *                          begin,
        mfxU8 *                          end,
        ArrayDpbFrame const &            dpb,
        std::vector<Reconstruct> const & recons,
        mfxU32                           frameTag,
        mfxU32                           picStruct)
    {
        mfxU8 fieldId = picStruct == MFX_PICSTRUCT_FIELD_BFF ? 0x80 : 0;

        for (; begin != end; ++begin)
        {
            if (recons[dpb[*begin & 0x7f].m_frameIdx].m_extFrameTag == frameTag)
            {
                if (picStruct == MFX_PICSTRUCT_PROGRESSIVE || fieldId == (*begin & 0x80))
                    break;
            }
        }

        return begin;
    }

    void RotateRight(mfxU8 * begin, mfxU8 * end)
    {
        if (begin != end)
        {
            mfxU8 mostRight = *--end;

            for (; begin != end; --end)
                *end = *(end - 1);

            *begin = mostRight;
        }
    }

    void RotateLeft(mfxU8 * begin, mfxU8 * end)
    {
        if (begin != end)
        {
            --end;
            mfxU8 mostLeft = *begin;

            for (; begin != end; ++begin)
                *begin = *(begin + 1);

            *end = mostLeft;
        }
    }

    void ReorderRefPicListPreserveOrderInInitialRefList (
        ArrayU8x33 &                     refPicList,
        ArrayDpbFrame const &            dpb,
        std::vector<Reconstruct> const & recons,
        mfxExtAVCRefListCtrl const &     ctrl,
        mfxU32                           numActiveRef,
        mfxU32                           curPicStruct)
    {
        mfxU8 * begin = refPicList.Begin();
        mfxU8 * end   = refPicList.End();

        for (mfxU8 * ref = refPicList.Begin(); ref < end; ++ref)
        {
            mfxU32 extFrameTag = recons[dpb[(*ref) & 0x7f].m_frameIdx].m_extFrameTag;

            mfxU32 picStruct = curPicStruct;
            if (picStruct != MFX_PICSTRUCT_PROGRESSIVE)
                picStruct = ((*ref) & 0x80) ? MFX_PICSTRUCT_FIELD_BFF : MFX_PICSTRUCT_FIELD_TFF;

            if (IsPreferred(ctrl, extFrameTag, picStruct))
            {
                RotateRight(begin, ref + 1);
                begin++;
            }
            else if (IsRejected(ctrl, extFrameTag, picStruct))
            {
                RotateLeft(ref, end);
                --end;
            }
        }

        refPicList.Resize((mfxU32)(end - refPicList.Begin()));
        if (refPicList.Size() > numActiveRef)
            refPicList.Resize(numActiveRef);
    }

    void ReorderRefPicList( // PreserveOrderInPreferredRefList
        ArrayU8x33 &                     refPicList,
        ArrayDpbFrame const &            dpb,
        std::vector<Reconstruct> const & recons,
        mfxExtAVCRefListCtrl const &     ctrl,
        mfxU32                           numActiveRef,
        mfxU32                           curPicStruct)
    {
        curPicStruct;
        mfxU8 * begin = refPicList.Begin();
        mfxU8 * end   = refPicList.End();

        for (mfxU32 i = 0; i < 32 && ctrl.PreferredRefList[i].FrameOrder != 0xffffffff; i++)
        {
            mfxU8 * ref = FindByExtFrameTag(
                begin,
                end,
                dpb,
                recons,
                ctrl.PreferredRefList[i].FrameOrder,
                ctrl.PreferredRefList[i].PicStruct);

            if (ref != end)
            {
                RotateRight(begin, ref + 1);
                begin++;
            }
        }

        for (mfxU32 i = 0; i < 16 && ctrl.RejectedRefList[i].FrameOrder != 0xffffffff; i++)
        {
            mfxU8 * ref = FindByExtFrameTag(
                begin,
                end,
                dpb,
                recons,
                ctrl.RejectedRefList[i].FrameOrder,
                ctrl.RejectedRefList[i].PicStruct);

            if (ref != end)
            {
                RotateLeft(ref, end);
                --end;
            }
        }

        refPicList.Resize((mfxU32)(end - refPicList.Begin()));
        if (numActiveRef > 0 && refPicList.Size() > numActiveRef)
            refPicList.Resize(numActiveRef);
    }
};

void TaskManager::ModifyRefPicLists(
    DdiTask & task,
    mfxU32    fieldId) const
{
    ArrayDpbFrame const & dpb   = task.m_dpb[fieldId];
    ArrayU8x33 &          list0 = task.m_list0[fieldId];
    ArrayU8x33 &          list1 = task.m_list1[fieldId];
    mfxU32                ps    = task.GetPicStructForEncode();
    ArrayRefListMod &     mod0  = task.m_refPicList0Mod[fieldId];
    ArrayRefListMod &     mod1  = task.m_refPicList1Mod[fieldId];

    ArrayU8x33 initList0 = task.m_list0[fieldId];
    ArrayU8x33 initList1 = task.m_list1[fieldId];
    mfxI32     curPicNum = task.m_picNum[fieldId];

    if ((m_video.mfx.GopOptFlag & MFX_GOP_CLOSED) || Less(task.m_frameOrderI, task.m_frameOrder))
    {
        // remove references to pictures prior to first I frame in decoding order
        // if gop is closed do it for all frames in gop
        // if gop is open do it for pictures subsequent to first I frame in display order

        mfxU32 firstIntraFramePoc = 2 * (task.m_frameOrderI - task.m_frameOrderIdr);

        list0.Erase(
            std::remove_if(list0.Begin(), list0.End(), LogicalAnd(
                RefPocIsLessThan(m_recons, dpb, firstIntraFramePoc),
                RefIsShortTerm(m_recons, dpb))),
            list0.End());

        list1.Erase(
            std::remove_if(list1.Begin(), list1.End(), LogicalAnd(
                RefPocIsLessThan(m_recons, dpb, firstIntraFramePoc),
                RefIsShortTerm(m_recons, dpb))),
            list1.End());
    }

    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);

    if (mfxExtAVCRefListCtrl * ctrl = (m_video.calcParam.numTemporalLayer == 0) ? GetExtBuffer(task.m_ctrl) : (mfxExtAVCRefListCtrl *)0)
    {
        mfxU32 numActiveRefL0 = (task.m_type[fieldId] & MFX_FRAMETYPE_P)
            ? extDdi->NumActiveRefP
            : extDdi->NumActiveRefBL0;
        if (task.m_type[fieldId] & MFX_FRAMETYPE_PB)
        {
            numActiveRefL0 = ctrl->NumRefIdxL0Active ? IPP_MIN(ctrl->NumRefIdxL0Active,numActiveRefL0) : numActiveRefL0;
            ReorderRefPicList(list0, dpb, m_recons, *ctrl, numActiveRefL0, ps);
        }

        if (task.m_type[fieldId] & MFX_FRAMETYPE_B)
        {
            mfxU32 numActiveRefL1 = ctrl->NumRefIdxL1Active ? IPP_MIN(ctrl->NumRefIdxL0Active,extDdi->NumActiveRefBL1) : extDdi->NumActiveRefBL1;
            ReorderRefPicList(list1, dpb, m_recons, *ctrl, numActiveRefL1, ps);
        }
    }
    else
    {
        // prepare ref list for P-field of I/P field pair
        // swap 1st and 2nd entries of L0 ref pic list to use I-field of I/P pair as reference for P-field
        mfxU32 ffid = task.GetFirstField();
        if ((task.m_type[ ffid] & MFX_FRAMETYPE_I) &&
            (task.m_type[!ffid] & MFX_FRAMETYPE_P))
        {
            if (ps != MFX_PICSTRUCT_PROGRESSIVE && fieldId != ffid && list0.Size() > 1)
                std::swap(list0[0], list0[1]);
        }
        else if (task.m_type[fieldId] & MFX_FRAMETYPE_B)
        {
            mfxU8 save0 = list0[0];
            mfxU8 save1 = list1[0];

            list0.Erase(
                std::remove_if(list0.Begin(), list0.End(), 
                    RefPocIsGreaterThan(m_recons, dpb, task.GetPoc(fieldId))),
                list0.End());

            list1.Erase(
                std::remove_if(list1.Begin(), list1.End(),
                    RefPocIsLessThan(m_recons, dpb, task.GetPoc(fieldId))),
                list1.End());

            // keep at least one ref pic in lists
            if (list0.Size() == 0)
                list0.PushBack(save0);
            if (list1.Size() == 0)
                list1.PushBack(save1);
        }

        if (m_video.calcParam.numTemporalLayer > 0)
        {
            list0.Erase(
                std::remove_if(
                    list0.Begin(),
                    list0.End(),
                    RefIsFromHigherTemporalLayer(m_recons, dpb, task.m_tid)),
                list0.End());

            list1.Erase(
                std::remove_if(
                    list1.Begin(),
                    list1.End(),
                    RefIsFromHigherTemporalLayer(m_recons, dpb, task.m_tid)),
                list1.End());

            std::sort(list0.Begin(), list0.End(), RefPocIsGreater(m_recons, dpb));
            std::sort(list1.Begin(), list1.End(), RefPocIsLess(m_recons, dpb));

            if (m_video.calcParam.lyncMode)
            { // cut lists to 1 element for lync
                list0.Resize(IPP_MIN(list0.Size(), 1));
                list1.Resize(IPP_MIN(list1.Size(), 1));
            }
        }

        mfxU32 numActiveRefL1 = extDdi->NumActiveRefBL1;
        mfxU32 numActiveRefL0 = (task.m_type[fieldId] & MFX_FRAMETYPE_P)
            ? extDdi->NumActiveRefP
            : extDdi->NumActiveRefBL0;

        if (numActiveRefL0 > 0 && list0.Size() > numActiveRefL0)
            list0.Resize(numActiveRefL0);
        if (numActiveRefL1 > 0 && list1.Size() > numActiveRefL1)
            list1.Resize(numActiveRefL1);

        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(m_video);
        if (mfxU32 refBaseDist = extSvc->RefBaseDist)
        {
            mfxU32 prevKeyPicPoc = ((task.m_frameOrder - 1) / refBaseDist * refBaseDist - task.m_frameOrderIdr) * 2;

            list0.Erase(
                std::remove_if(list0.Begin(), list0.End(), LogicalAnd(
                    RefPocIsLessThan(m_recons, dpb, prevKeyPicPoc),
                    RefIsShortTerm(m_recons, dpb))),
                list0.End());

            list1.Erase(
                std::remove_if(list1.Begin(), list1.End(), LogicalAnd(
                    RefPocIsLessThan(m_recons, dpb, prevKeyPicPoc),
                    RefIsShortTerm(m_recons, dpb))),
                list1.End());
        }
    }

    initList0.Resize(list0.Size());
    initList1.Resize(list1.Size());

    bool noLongTermInList0 = LongTermInList(m_recons, dpb, initList0);
    bool noLongTermInList1 = LongTermInList(m_recons, dpb, initList1);

    mod0 = CreateRefListMod(dpb, m_recons, initList0, list0, task.m_viewIdx, curPicNum, noLongTermInList0);
    mod1 = CreateRefListMod(dpb, m_recons, initList1, list1, task.m_viewIdx, curPicNum, noLongTermInList1);
}

void TaskManager::ConfirmTask(DdiTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    mfxU32 ps = task.GetPicStructForEncode();

    if (task.GetFrameType() & MFX_FRAMETYPE_IDR)
    {
        m_frameNum = 0;
        m_idrPicId++;

        // sei message with buffering period will be sent with IDR picture
        // it will reset cpb/dpb delays
        m_cpbRemovalBufferingPeriod = m_cpbRemoval;
    }

    m_cpbRemoval++; // is incremented every frame (unlike frame_num)

    if (task.GetFrameType() & MFX_FRAMETYPE_REF)
    {
        m_frameNum = (m_frameNum + 1) % m_frameNumMax;
    }

    if (task.GetFrameType() & MFX_FRAMETYPE_IDR)
    {
        m_frameOrderIdr = task.m_frameOrder;
    }

    if (task.GetFrameType() & MFX_FRAMETYPE_I)
    {
        m_frameOrderI = task.m_frameOrder;
    }

    if (task.m_pushed != 0)
    {
        if (task.m_pushed->GetFrameType() == MFX_FRAMETYPE_IREFIDR)
            m_frameTypeGen.Init(m_video); // idr starts new gop

        m_frameTypeGen.Next();
        m_frameOrder++;
        m_core->IncreaseReference(&task.m_pushed->m_yuv->Data);
    }

    if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY && task.m_idx != NO_INDEX)
    {
        m_raws[task.m_idx].SetFree(false);
    }

    if (task.m_idxBs.top != NO_INDEX)
    {
        m_bitstreams[task.m_idxBs.top].SetFree(false);
    }

    if (task.m_idxBs.bot != NO_INDEX)
    {
        assert(ps != MFX_PICSTRUCT_PROGRESSIVE);
        m_bitstreams[task.m_idxBs.bot].SetFree(false);
    }

    if (task.m_idxRecon != NO_INDEX)
    {
        m_recons[task.m_idxRecon].SetFree(false);
        m_recons[task.m_idxRecon].m_reference[TFIELD] = (task.m_type[TFIELD] & MFX_FRAMETYPE_REF) != 0;
        m_recons[task.m_idxRecon].m_reference[BFIELD] = (task.m_type[BFIELD] & MFX_FRAMETYPE_REF) != 0;
    }

    // task already has a dpb with sliding window and adapative dec_ref_pic_marking commands applied
    // store it for next task
    m_dpb = task.m_dpbPostEncoding;

    // store dec_ref_pic_marking info of last picture for repetition sei
    mfxU32 lastField = (ps == MFX_PICSTRUCT_PROGRESSIVE) ? 0 : !task.GetFirstField();

    m_decRefPicMrkRep.presentFlag =
        (task.m_type[lastField] & MFX_FRAMETYPE_IDR) ||
        (task.m_decRefPicMrk[lastField].mmco.Size() > 0);

    m_decRefPicMrkRep.original_idr_flag          = (task.m_type[lastField] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    m_decRefPicMrkRep.original_frame_num         = (task.m_frameNum);
    m_decRefPicMrkRep.original_field_pic_flag    = (ps == MFX_PICSTRUCT_PROGRESSIVE) ? 0 : 1;
    m_decRefPicMrkRep.original_bottom_field_flag = (lastField == BFIELD) ? 1 : 0;
    m_decRefPicMrkRep.dec_ref_pic_marking        = task.m_decRefPicMrk[lastField];

    m_pushed = 0;
    m_stat.NumCachedFrame++;
}


void TaskManager::CompleteTask(DdiTask & task)
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    mfxExtCodingOptionDDI const * optDdi = GetExtBuffer(m_video);

    assert(std::find_if(m_tasks.begin(), m_tasks.end(), FindByFrameOrder(task.m_frameOrder)) != m_tasks.end());

    ArrayDpbFrame const & iniDpb = task.m_dpb[task.GetFirstField()];
    ArrayDpbFrame const & finDpb = task.m_dpbPostEncoding;
    for (mfxU32 i = 0; i < iniDpb.Size(); i++)
    {
        if (std::find(finDpb.Begin(), finDpb.End(), iniDpb[i]) == finDpb.End())
        {
            SetReconstructFree(m_recons[iniDpb[i].m_frameIdx]);

            if (IsOn(optDdi->RefRaw))
            {
                if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                    m_raws[iniDpb[i].m_frameIdx].SetFree(true);
                else
                    m_core->DecreaseReference(&m_recons[iniDpb[i].m_frameIdx].m_yuv->Data);
            }
        }
    }

    if (task.m_idxBs[0] != NO_INDEX)
    {
        m_bitstreams[task.m_idxBs[0]].SetFree(true);
    }

    if (task.m_idxBs[1] != NO_INDEX)
    {
        assert((task.GetPicStructForEncode() & MFX_PICSTRUCT_PROGRESSIVE) == 0);
        m_bitstreams[task.m_idxBs[1]].SetFree(true);
    }

    if (IsOff(optDdi->RefRaw))
    {
        m_core->DecreaseReference(&task.m_yuv->Data);

        if (task.m_idx != NO_INDEX && m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            m_raws[task.m_idx].SetFree(true);
    }
    else
    {
        if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            // when input surface is in system memory
            // we can release it right after task is completed
            // even if raw surfaces are used as reference
            m_core->DecreaseReference(&task.m_yuv->Data);

        if (!m_recons[task.m_idxRecon].m_reference[0] && !m_recons[task.m_idxRecon].m_reference[1])
        {
            m_core->DecreaseReference(&task.m_yuv->Data);
            if (m_video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                m_raws[task.m_idx].SetFree(true);
        }
    }

    if (task.m_idxRecon != NO_INDEX)
    {
        if (!m_recons[task.m_idxRecon].m_reference[0] && !m_recons[task.m_idxRecon].m_reference[1])
            SetReconstructFree(m_recons[task.m_idxRecon]);
    }

    m_stat.NumCachedFrame--;
    m_stat.NumFrame++;
    m_stat.NumBit += 8 * (task.m_bsDataLength[0] + task.m_bsDataLength[1]);

    task.m_bs = 0;
    task.SetFree(true);
}

mfxU32 TaskManager::CountRunningTasks()
{
    UMC::AutomaticUMCMutex guard(m_mutex);

    mfxU32 count = 0;
    for (size_t i = 0; i < m_bitstreams.size(); i++)
        if (!m_bitstreams[i].IsFree())
            ++count;

    return count;
}

NalUnit MfxHwH264Encode::GetNalUnit(mfxU8 * begin, mfxU8 * end)
{
    for (; begin < end - 5; ++begin)
    {
        if ((begin[0] == 0 && begin[1] == 0 && begin[2] == 1) ||
            (begin[0] == 0 && begin[1] == 0 && begin[2] == 0 && begin[3] == 1))
        {
            mfxU8 numZero = (begin[2] == 1 ? 2 : 3);
            mfxU8 type    = (begin[2] == 1 ? begin[3] : begin[4]) & 0x1f;

            for (mfxU8 * next = begin + 4; next < end - 4; ++next)
            {
                if (next[0] == 0 && next[1] == 0 && next[2] == 1)
                {
                    if (*(next - 1) == 0)
                        --next;

                    return NalUnit(begin, next, type, numZero);
                }
            }

            return NalUnit(begin, end, type, numZero);
        }
    }

    return NalUnit();
}

void MfxHwH264Encode::PrepareSeiMessage(
    DdiTask const &               task,
    mfxU32                        nalHrdBpPresentFlag,
    mfxU32                        vclHrdBpPresentFlag,
    mfxU32                        seqParameterSetId,
    mfxExtAvcSeiBufferingPeriod & msg)
{
    Zero(msg);

    assert(seqParameterSetId < 32);
    msg.seq_parameter_set_id                    = mfxU8(seqParameterSetId);
    msg.nal_cpb_cnt                             = !!nalHrdBpPresentFlag;
    msg.vcl_cpb_cnt                             = !!vclHrdBpPresentFlag;
    msg.initial_cpb_removal_delay_length        = 24;
    msg.nal_initial_cpb_removal_delay[0]        = task.m_initCpbRemoval;
    msg.nal_initial_cpb_removal_delay_offset[0] = task.m_initCpbRemovalOffset;
    msg.vcl_initial_cpb_removal_delay[0]        = task.m_initCpbRemoval;
    msg.vcl_initial_cpb_removal_delay_offset[0] = task.m_initCpbRemovalOffset;
}

void MfxHwH264Encode::PrepareSeiMessage(
    DdiTask const &                task,
    mfxU32                         fieldId,
    mfxU32                         cpbDpbDelaysPresentFlag,
    mfxExtAvcSeiPicTiming &        msg)
{
    Zero(msg);
    msg.cpb_dpb_delays_present_flag = mfxU8(cpbDpbDelaysPresentFlag);
    msg.cpb_removal_delay_length    = 24;
    msg.dpb_output_delay_length     = 24;
    msg.pic_struct_present_flag     = 1;
    msg.time_offset_length          = 24;
    msg.cpb_removal_delay           = task.m_cpbRemoval[fieldId];
    msg.dpb_output_delay            = task.m_dpbOutputDelay;

    switch (task.GetPicStructForDisplay())
    {
    case mfxU16(MFX_PICSTRUCT_FIELD_TFF):
    case mfxU16(MFX_PICSTRUCT_FIELD_BFF):
        msg.pic_struct = mfxU8(fieldId + 1);
        msg.ct_type    = 1;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF):
        msg.pic_struct = 3;
        msg.ct_type    = 1;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF):
        msg.pic_struct = 4;
        msg.ct_type    = 1;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_REPEATED):
        msg.pic_struct = 5;
        msg.ct_type    = 1;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FIELD_BFF | MFX_PICSTRUCT_FIELD_REPEATED):
        msg.pic_struct = 6;
        msg.ct_type    = 1;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_DOUBLING):
        msg.pic_struct = 7;
        msg.ct_type    = 0;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE | MFX_PICSTRUCT_FRAME_TRIPLING):
        msg.pic_struct = 8;
        msg.ct_type    = 0;
        break;
    case mfxU16(MFX_PICSTRUCT_PROGRESSIVE):
    default:
        msg.pic_struct = 0;
        msg.ct_type    = 0;
        break;
    }
}

void MfxHwH264Encode::PrepareSeiMessage(
    const DdiTask& task,
    mfxU32 fieldId,
    mfxU32 frame_mbs_only_flag,
    mfxExtAvcSeiDecRefPicMrkRep& extSeiDecRefPicMrkRep)
{
    extSeiDecRefPicMrkRep.original_idr_flag                = task.m_decRefPicMrkRep[fieldId].original_idr_flag;
    extSeiDecRefPicMrkRep.original_frame_num               = task.m_decRefPicMrkRep[fieldId].original_frame_num;
    extSeiDecRefPicMrkRep.original_field_info_present_flag = (frame_mbs_only_flag == 0);

    if (frame_mbs_only_flag == 0)
    {
        extSeiDecRefPicMrkRep.original_field_pic_flag    = task.m_decRefPicMrkRep[fieldId].original_field_pic_flag;
        extSeiDecRefPicMrkRep.original_bottom_field_flag = task.m_decRefPicMrkRep[fieldId].original_bottom_field_flag;
    }

    extSeiDecRefPicMrkRep.no_output_of_prior_pics_flag       = task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.no_output_of_prior_pics_flag;
    extSeiDecRefPicMrkRep.long_term_reference_flag           = task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.long_term_reference_flag;
    extSeiDecRefPicMrkRep.num_mmco_entries                   = task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.mmco.Size();
    extSeiDecRefPicMrkRep.adaptive_ref_pic_marking_mode_flag = task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.mmco.Size() > 0;

    for (mfxU8 i = 0; i < extSeiDecRefPicMrkRep.num_mmco_entries; i ++)
    {
        extSeiDecRefPicMrkRep.mmco[i]          =  task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.mmco[i];
        extSeiDecRefPicMrkRep.value[i * 2]     =  task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.value[i * 2];
        extSeiDecRefPicMrkRep.value[i * 2 + 1] =  task.m_decRefPicMrkRep[fieldId].dec_ref_pic_marking.value[i * 2 + 1];
    }

}

void MfxHwH264Encode::PrepareSeiMessage(
    MfxVideoParam const &   par,
    DdiTask const &         task,
    mfxExtAvcSeiRecPoint &  msg)
{
    if (task.m_IRState.refrType == NO_REFRESH || task.m_IRState.IntraSize == 0)
        return;
    mfxU16 refreshDimension = (task.m_IRState.refrType == VERT_REFRESH ? par.mfx.FrameInfo.Width : par.mfx.FrameInfo.Height) / 16;
    msg.recovery_frame_cnt = (refreshDimension + task.m_IRState.IntraSize - 1) / task.m_IRState.IntraSize - 1;
    msg.exact_match_flag = 1;
    msg.broken_link_flag = 0;
    msg.changing_slice_group_idc = 0;
}

// MVC BD {
mfxU32 MfxHwH264Encode::CalculateSeiSize( mfxExtAvcSeiBufferingPeriod const & msg)
{
    mfxU32 dataSizeInBits =
        2 * msg.initial_cpb_removal_delay_length * (msg.nal_cpb_cnt + msg.vcl_cpb_cnt);

    dataSizeInBits += CeilLog2(msg.seq_parameter_set_id) * 2 + 1;
    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    return dataSizeInBytes;
}

mfxU32 MfxHwH264Encode::CalculateSeiSize(
    mfxExtPictureTimingSEI const & extPt,
    mfxExtAvcSeiPicTiming const &  msg)
{
    mfxU32 dataSizeInBits = 0;

    if (msg.cpb_dpb_delays_present_flag)
    {
        dataSizeInBits += msg.cpb_removal_delay_length;
        dataSizeInBits += msg.dpb_output_delay_length;
    }

    if (msg.pic_struct_present_flag)
    {
        dataSizeInBits += 4; // msg.pic_struct;

        assert(msg.pic_struct <= 8);
        mfxU32 numClockTS = NUM_CLOCK_TS[IPP_MIN(msg.pic_struct, 8)];

        dataSizeInBits += numClockTS; // clock_timestamp_flag[i]
        for (mfxU32 i = 0; i < numClockTS; i++)
        {
            if (extPt.TimeStamp[i].ClockTimestampFlag)
            {
                mfxU32 tsSize = 19;

                if (extPt.TimeStamp[i].FullTimestampFlag)
                {
                    tsSize += 17;
                }
                else
                {
                    tsSize += ((
                        extPt.TimeStamp[i].HoursFlag * 5 + 7) *
                        extPt.TimeStamp[i].MinutesFlag + 7) *
                        extPt.TimeStamp[i].SecondsFlag + 1;
                }

                dataSizeInBits += tsSize + msg.time_offset_length;
            }
        }
    }

    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    return dataSizeInBytes;
}
// MVC BD }

AesCounter::AesCounter()
{
    Zero(*this);
}

void AesCounter::Init(const mfxExtPAVPOption & opt)
{
    m_opt = opt;
    Count = opt.CipherCounter.Count;
    IV    = opt.CipherCounter.IV;
}

namespace
{
    mfxU64 SwapEndian(mfxU64 val)
    {
        return
            ((val << 56) & 0xff00000000000000) | ((val << 40) & 0x00ff000000000000) |
            ((val << 24) & 0x0000ff0000000000) | ((val <<  8) & 0x000000ff00000000) |
            ((val >>  8) & 0x00000000ff000000) | ((val >> 24) & 0x0000000000ff0000) |
            ((val >> 40) & 0x000000000000ff00) | ((val >> 56) & 0x00000000000000ff);
    }
};

bool MfxHwH264Encode::Increment(
    mfxAES128CipherCounter & aesCounter,
    mfxExtPAVPOption const & extPavp)
{
    bool wrapped = false;

    if (extPavp.EncryptionType == MFX_PAVP_AES128_CTR)
    {
        mfxU64 tmp = SwapEndian(aesCounter.Count) + extPavp.CounterIncrement;

        if (extPavp.CounterType == MFX_PAVP_CTR_TYPE_A)
            tmp &= 0xffffffff;

        wrapped = (tmp < extPavp.CounterIncrement);
        aesCounter.Count = SwapEndian(tmp);
    }

    return wrapped;
}

void MfxHwH264Encode::Decrement(
    mfxAES128CipherCounter & aesCounter,
    mfxExtPAVPOption const & extPavp)
{
    if (extPavp.EncryptionType == MFX_PAVP_AES128_CTR)
    {
        mfxU64 tmp = SwapEndian(aesCounter.Count) - extPavp.CounterIncrement;

        if (extPavp.CounterType == MFX_PAVP_CTR_TYPE_A)
            tmp &= 0xffffffff;

        aesCounter.Count = SwapEndian(tmp);
    }
}

bool AesCounter::Increment()
{
    return (m_wrapped = MfxHwH264Encode::Increment(*this, m_opt));
}


namespace
{
    UMC::FrameType ConvertFrameTypeMfx2Umc(mfxU32 frameType)
    {
        switch (frameType & 0xf)
        {
        case MFX_FRAMETYPE_I: return UMC::I_PICTURE;
        case MFX_FRAMETYPE_P: return UMC::P_PICTURE;
        case MFX_FRAMETYPE_B: return UMC::B_PICTURE;
        default: assert(!"wrong coding type"); return UMC::NONE_PICTURE;
        }
    }

    mfxI32 ConvertPicStructMfx2Umc(mfxU32 picStruct)
    {
        switch (picStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE: return UMC::PS_FRAME;
        case MFX_PICSTRUCT_FIELD_TFF:   return UMC::PS_TOP_FIELD;
        case MFX_PICSTRUCT_FIELD_BFF:   return UMC::PS_BOTTOM_FIELD;
        default: assert(!"bad picStruct"); return UMC::PS_FRAME;
        }
    }
};

void UmcBrc::Init(MfxVideoParam const & video)
{
    assert(
        video.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        video.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        video.mfx.RateControlMethod == MFX_RATECONTROL_AVBR);

    mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(video);
    m_lookAhead = extOpt2->LookAheadDepth;

    mfxVideoParam tmpVideo = video;
    tmpVideo.mfx.GopRefDist = (extOpt2->LookAheadDepth >= 5) ? 1 : tmpVideo.mfx.GopRefDist;

    UMC::VideoBrcParams umcBrcParams;
    mfxStatus sts = ConvertVideoParam_Brc(&tmpVideo, &umcBrcParams);
    assert(sts == MFX_ERR_NONE); sts;

    umcBrcParams.GOPPicSize = tmpVideo.mfx.GopPicSize;
    umcBrcParams.GOPRefDist = tmpVideo.mfx.GopRefDist;
    umcBrcParams.profile    = tmpVideo.mfx.CodecProfile;
    umcBrcParams.level      = tmpVideo.mfx.CodecLevel;

    UMC::Status umcSts = m_impl.Init(&umcBrcParams, 0);
    assert(umcSts == UMC::UMC_OK); umcSts;
}

void UmcBrc::Close()
{
    m_impl.Close();
}

mfxU8 UmcBrc::GetQp(mfxU32 frameType, mfxU32 picStruct)
{
    if (m_lookAhead >= 5 && (frameType & MFX_FRAMETYPE_B))
        frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    UMC::FrameType umcFrameType = ConvertFrameTypeMfx2Umc(frameType);
    m_impl.SetPictureFlags(umcFrameType, ConvertPicStructMfx2Umc(picStruct));
    return mfxU8(m_impl.GetQP(umcFrameType));
}

mfxF32 UmcBrc::GetFractionalQp(mfxU32 frameType, mfxU32 picStruct)
{
    if (m_lookAhead >= 5 && (frameType & MFX_FRAMETYPE_B))
        frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    UMC::FrameType umcFrameType = ConvertFrameTypeMfx2Umc(frameType);
    m_impl.SetPictureFlags(umcFrameType, ConvertPicStructMfx2Umc(picStruct));
    return 0.f;//m_impl.GetFractionalQP(umcFrameType);
}

void UmcBrc::SetQp(mfxU32 qp, mfxU32 frameType)
{
    if (m_lookAhead >= 5 && (frameType & MFX_FRAMETYPE_B))
        frameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
    m_impl.SetQP(qp, ConvertFrameTypeMfx2Umc(frameType));
}

void UmcBrc::PreEnc(mfxU32 frameType, std::vector<VmeData *> const & vmeData, mfxU32 curEncOrder)
{
    for (size_t i = 0; i < vmeData.size(); i++)
    {
        if (vmeData[i]->encOrder == curEncOrder)
        {
            m_impl.PreEncFrame(ConvertFrameTypeMfx2Umc(frameType), vmeData[i]->intraCost, vmeData[i]->interCost);
            break;
        }
    }
}

mfxU32 UmcBrc::Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 userDataLength, mfxU32 repack, mfxU32 picOrder)
{
    return m_impl.PostPackFrame(ConvertFrameTypeMfx2Umc(frameType), 8 * dataLength, userDataLength * 8, repack, picOrder);
}

mfxU32 UmcBrc::GetMinFrameSize()
{
    mfxI32 minSize = 0;
    //m_impl.GetMinMaxFrameSize(&minSize, 0);
    assert(minSize >= 0);
    return mfxU32(minSize + 7) / 8;
}


namespace
{
    mfxF64 const QSTEP[52] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070
    };

    mfxU8 QStep2QpCeil(mfxF64 qstep) // QSTEP[qp] > qstep, may return qp=52
    {
        return mfxU8(std::lower_bound(QSTEP, QSTEP + 52, qstep) - QSTEP);
    }

    mfxU8 QStep2QpFloor(mfxF64 qstep) // QSTEP[qp] <= qstep, return 0<=qp<=51
    {
        mfxU8 qp = mfxU8(std::upper_bound(QSTEP, QSTEP + 52, qstep) - QSTEP);
        return qp > 0 ? qp - 1 : 0;
    }

    mfxU8 QStep2QpNearest(mfxF64 qstep) // return 0<=qp<=51
    {
        mfxU8 qp = QStep2QpFloor(qstep);
        return (qp == 51 || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
    }

    mfxF64 Qp2QStep(mfxU32 qp)
    {
        return QSTEP[IPP_MIN(51, qp)];
    }
}

#ifdef _DEBUG
//#define brcprintf printf
#else // _DEBUG
//#define brcprintf
#endif // _DEBUG

#ifndef brcprintf
#define brcprintf
#endif // brcprintf

namespace
{
    mfxF64 const INTRA_QSTEP_COEFF  = 2.0;
    mfxF64 const INTRA_MODE_BITCOST = 0.0;
    mfxF64 const INTER_MODE_BITCOST = 0.0;
    mfxI32 const MAX_QP_CHANGE      = 2;
    mfxF64 const LOG2_64            = 3;
    mfxF64 const MIN_EST_RATE       = 0.3;
    mfxF64 const NORM_EST_RATE      = 100.0;

    mfxF64 const MIN_RATE_COEFF_CHANGE = 0.5;
    mfxF64 const MAX_RATE_COEFF_CHANGE = 2.0;
    mfxF64 const INIT_RATE_COEFF[] = {
        1.109, 1.196, 1.225, 1.309, 1.369, 1.428, 1.490, 1.588, 1.627, 1.723, 1.800, 1.851, 1.916,
        2.043, 2.052, 2.140, 2.097, 2.096, 2.134, 2.221, 2.084, 2.153, 2.117, 2.014, 1.984, 2.006,
        1.801, 1.796, 1.682, 1.549, 1.485, 1.439, 1.248, 1.221, 1.133, 1.045, 0.990, 0.987, 0.895,
        0.921, 0.891, 0.887, 0.896, 0.925, 0.917, 0.942, 0.964, 0.997, 1.035, 1.098, 1.170, 1.275
    };

    mfxF64 const DEP_STRENTH        = 2.0;

    mfxU8 GetSkippedQp(MbData const & mb)
    {
        if (mb.intraMbFlag)
            return 52; // never skipped
        if (abs(mb.mv[0].x - mb.costCenter0.x) >= 4 ||
            abs(mb.mv[0].y - mb.costCenter0.y) >= 4 ||
            abs(mb.mv[1].x - mb.costCenter1.x) >= 4 ||
            abs(mb.mv[1].y - mb.costCenter1.y) >= 4)
            return 52; // never skipped

        mfxU16 const * sumc = mb.lumaCoeffSum;
        mfxU8  const * nzc  = mb.lumaCoeffCnt;

        if (nzc[0] + nzc[1] + nzc[2] + nzc[3] == 0)
            return 0; // skipped at any qp

        mfxF64 qoff = 1.0 / 6;
        mfxF64 norm = 0.1666;

        mfxF64 qskip = IPP_MAX(IPP_MAX(IPP_MAX(
            nzc[0] ? (sumc[0] * norm / nzc[0]) / (1 - qoff) * LOG2_64 : 0,
            nzc[1] ? (sumc[1] * norm / nzc[1]) / (1 - qoff) * LOG2_64 : 0),
            nzc[2] ? (sumc[2] * norm / nzc[2]) / (1 - qoff) * LOG2_64 : 0),
            nzc[3] ? (sumc[3] * norm / nzc[3]) / (1 - qoff) * LOG2_64 : 0);

        return QStep2QpCeil(qskip);
    }
}

void LookAheadBrc2::Init(MfxVideoParam const & video)
{
    assert(video.mfx.RateControlMethod == MFX_RATECONTROL_LA);

    mfxExtCodingOptionDDI const * extDdi  = GetExtBuffer(video);
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(video);
    m_lookAhead     = extOpt2->LookAheadDepth - extDdi->LookAheadDep;
    m_lookAheadDep  = extDdi->LookAheadDep;
    m_qpUpdateRange = extDdi->QpUpdateRange;
    m_strength      = extDdi->StrengthN;

    mfxF64 fr = mfxF64(video.mfx.FrameInfo.FrameRateExtN) / video.mfx.FrameInfo.FrameRateExtD;
    m_totNumMb = video.mfx.FrameInfo.Width * video.mfx.FrameInfo.Height / 256;
    m_initTargetRate = 1000 * video.mfx.TargetKbps / fr / m_totNumMb;
    m_targetRateMin = m_initTargetRate;
    m_targetRateMax = m_initTargetRate;
    m_laData.reserve(m_lookAhead);

    assert(extDdi->RegressionWindow <= m_rateCoeffHistory[0].MAX_WINDOW);
    for (mfxU32 qp = 0; qp < 52; qp++)
        m_rateCoeffHistory[qp].Reset(extDdi->RegressionWindow, 100.0, 100.0 * INIT_RATE_COEFF[qp]);
    m_framesBehind = 0;
    m_bitsBehind = 0.0;
    m_curQp = -1;
    m_curBaseQp = -1;
}

void LookAheadBrc2::Close()
{
}

mfxU8 SelectQp(mfxF64 erate[52], mfxF64 budget)
{
    for (mfxU8 qp = 1; qp < 52; qp++)
        if (erate[qp] < budget)
            return (erate[qp - 1] + erate[qp] < 2 * budget) ? qp - 1 : qp;
    return 51;
}

mfxF64 GetTotalRate(std::vector<LookAheadBrc2::LaFrameData> const & laData, mfxI32 baseQp)
{
    mfxF64 totalRate = 0.0;
    for (size_t i = 0; i < laData.size(); i++)
        totalRate += laData[i].estRateTotal[CLIPVAL(0, 51, baseQp + laData[i].deltaQp)];
    return totalRate;
}

mfxU8 SelectQp(std::vector<LookAheadBrc2::LaFrameData> const & laData, mfxF64 budget)
{
    mfxF64 prevTotalRate = GetTotalRate(laData, 0);
    for (mfxU8 qp = 1; qp < 52; qp++)
    {
        mfxF64 totalRate = GetTotalRate(laData, qp);
        if (totalRate < budget)
            return (prevTotalRate + totalRate < 2 * budget) ? qp - 1 : qp;
        else
            prevTotalRate = totalRate;
    }
    return 51;
}

mfxU8 GetFrameTypeLetter(mfxU32 frameType)
{
    mfxU32 ref = (frameType & MFX_FRAMETYPE_REF) ? 0 : 'a' - 'A';
    if (frameType & MFX_FRAMETYPE_I)
        return mfxU8('I' + ref);
    if (frameType & MFX_FRAMETYPE_P)
        return mfxU8('P' + ref);
    if (frameType & MFX_FRAMETYPE_B)
        return mfxU8('B' + ref);
    return 'x';
}

mfxU8 LookAheadBrc2::GetQp(mfxU32 frameType, mfxU32 /*picStruct*/)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::GetQp");
    brcprintf("\r%4d: do=%4d type=%c Rt=%7.3f-%7.3f curc=%4d numc=%2d ", m_laData[0].encOrder, m_laData[0].poc/2,
        GetFrameTypeLetter(frameType), m_targetRateMin, m_targetRateMax, m_laData[0].interCost / m_totNumMb, mfxU32(m_laData.size()));

    mfxF64 totalEstRate[52] = { 0.0 };

    for (mfxU32 qp = 0; qp < 52; qp++)
    {
        mfxF64 rateCoeff = m_rateCoeffHistory[qp].GetCoeff();
        for (mfxU32 i = 0; i < m_laData.size(); i++)
        {
            m_laData[i].estRateTotal[qp] = IPP_MAX(MIN_EST_RATE, rateCoeff * m_laData[i].estRate[qp]);
            totalEstRate[qp] += m_laData[i].estRateTotal[qp];
        }
    }

    mfxI32 maxDeltaQp = INT_MIN;
    if (m_lookAheadDep > 0)
    {
        mfxI32 curQp = m_curBaseQp < 0 ? SelectQp(totalEstRate, m_targetRateMin * m_laData.size()) : m_curBaseQp;
        mfxF64 strength = 0.03 * curQp + .75;

        for (mfxU32 i = 0; i < m_laData.size(); i++)
        {
            mfxU32 intraCost    = m_laData[i].intraCost;
            mfxU32 interCost    = m_laData[i].interCost;
            mfxU32 propCost     = m_laData[i].propCost;
            mfxF64 ratio        = 1.0;//mfxF64(interCost) / intraCost;
            mfxF64 deltaQp      = log((intraCost + propCost * ratio) / intraCost) / log(2.0);
            m_laData[i].deltaQp = (interCost >= intraCost * 0.9)
                ? -mfxI32(deltaQp * 2 * strength + 0.5)
                : -mfxI32(deltaQp * 1 * strength + 0.5);
            maxDeltaQp = IPP_MAX(maxDeltaQp, m_laData[i].deltaQp);
        }
    }
    else
    {
        for (mfxU32 i = 0; i < m_laData.size(); i++)
        {
            mfxU32 intraCost    = m_laData[i].intraCost;
            mfxU32 interCost    = m_laData[i].interCost;
            m_laData[i].deltaQp = (interCost >= intraCost * 0.9) ? -5 : m_laData[i].bframe ? 0 : -2;
            maxDeltaQp = IPP_MAX(maxDeltaQp, m_laData[i].deltaQp);
        }
    }

    for (mfxU32 i = 0; i < m_laData.size(); i++)
        m_laData[i].deltaQp -= maxDeltaQp;

    mfxU8 minQp = SelectQp(m_laData, m_targetRateMax * m_laData.size());
    mfxU8 maxQp = SelectQp(m_laData, m_targetRateMin * m_laData.size());

    if (m_curBaseQp < 0)
        m_curBaseQp = minQp; // first frame
    else if (m_curBaseQp < minQp)
        m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, minQp);
    else if (m_curQp > maxQp)
        m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, maxQp);
    else
        ; // do not change qp if last qp guarantees target rate interval
    m_curQp = CLIPVAL(1, 51, m_curBaseQp + m_laData[0].deltaQp); // driver doesn't support qp=0

    brcprintf("bqp=%2d qp=%2d dqp=%2d erate=%7.3f ", m_curBaseQp, m_curQp, m_laData[0].deltaQp, m_laData[0].estRateTotal[m_curQp]); 

    return mfxU8(m_curQp);
}

void LookAheadBrc2::SetQp(mfxU32 /*qp*/, mfxU32 /*frameType*/)
{
    assert(0);
}

mfxF32 LookAheadBrc2::GetFractionalQp(mfxU32 /*frameType*/, mfxU32 /*picStruct*/)
{
    assert(0);
    return 26.0f;
}

void LookAheadBrc2::PreEnc(mfxU32 /*frameType*/, std::vector<VmeData *> const & vmeData, mfxU32 curEncOrder)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::PreEnc");

    size_t i = 0;
    for (; i < m_laData.size(); i++)
        if (m_laData[i].encOrder == curEncOrder)
            break;
    m_laData.erase(m_laData.begin(), m_laData.begin() + i);

    mfxU32 firstNewFrame = m_laData.empty() ? curEncOrder : m_laData.back().encOrder + 1;
    mfxU32 lastNewFrame  = curEncOrder + m_lookAhead;

    for (size_t i = 0; i < vmeData.size(); i++)
    {
        if (vmeData[i]->encOrder < firstNewFrame || vmeData[i]->encOrder >= lastNewFrame)
            continue;

        LaFrameData newData = { 0 };
        newData.encOrder  = vmeData[i]->encOrder;
        newData.poc       = vmeData[i]->poc;
        newData.interCost = vmeData[i]->interCost;
        newData.intraCost = vmeData[i]->intraCost;
        newData.propCost  = vmeData[i]->propCost;
        newData.bframe    = vmeData[i]->pocL1 != mfxU32(-1);
        for (size_t j = 0; j < vmeData[i]->mb.size(); j++)
        {
            MbData const & mb = vmeData[i]->mb[j];
            if (mb.intraMbFlag)
            {
                for (mfxU32 qp = 0; qp < 52; qp++)
                    newData.estRate[qp] += mb.dist / (QSTEP[qp] * INTRA_QSTEP_COEFF);
            }
            else
            {
                mfxU32 skipQp = GetSkippedQp(mb);
                for (mfxU32 qp = 0; qp < skipQp; qp++)
                    newData.estRate[qp] += mb.dist / (QSTEP[qp]);
            }
        }
        for (mfxU32 qp = 0; qp < 52; qp++)
            newData.estRate[qp] /= m_totNumMb;
        m_laData.push_back(newData);
    }
    assert(m_laData.size() <= m_lookAhead);
}

mfxU32 LookAheadBrc2::Report(mfxU32 /*frameType*/, mfxU32 dataLength, mfxU32 /*userDataLength*/, mfxU32 /*repack*/, mfxU32 /*picOrder*/)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::Report");
    mfxF64 realRatePerMb = 8 * dataLength / mfxF64(m_totNumMb);
    static int size = 0;
    static int nframe = 0;
    size += dataLength;
    nframe++;

    m_framesBehind++;
    m_bitsBehind += realRatePerMb;
    mfxF64 framesBeyond = (mfxF64)(IPP_MAX(2, m_laData.size()) - 1);

    m_targetRateMax = (m_initTargetRate * (m_framesBehind + (m_lookAhead - 1)) - m_bitsBehind) / framesBeyond;
    m_targetRateMin = (m_initTargetRate * (m_framesBehind + (framesBeyond   )) - m_bitsBehind) / framesBeyond;

    mfxF64 oldCoeff = m_rateCoeffHistory[m_curQp].GetCoeff();
    mfxF64 y = IPP_MAX(0.0, realRatePerMb);
    mfxF64 x = m_laData[0].estRate[m_curQp];
    mfxF64 minY = NORM_EST_RATE * INIT_RATE_COEFF[m_curQp] * MIN_RATE_COEFF_CHANGE;
    mfxF64 maxY = NORM_EST_RATE * INIT_RATE_COEFF[m_curQp] * MAX_RATE_COEFF_CHANGE;
    y = CLIPVAL(minY, maxY, y / x * NORM_EST_RATE); 
    m_rateCoeffHistory[m_curQp].Add(NORM_EST_RATE, y);
    mfxF64 ratio = m_rateCoeffHistory[m_curQp].GetCoeff() / oldCoeff;
    for (mfxI32 i = -m_qpUpdateRange; i <= m_qpUpdateRange; i++)
        if (i != 0 && m_curQp + i >= 0 && m_curQp + i < 52)
        {
            mfxF64 r = ((ratio - 1.0) * (1.0 - abs(i)/(m_qpUpdateRange + 1)) + 1.0);
            m_rateCoeffHistory[m_curQp + i].Add(NORM_EST_RATE,
                NORM_EST_RATE * m_rateCoeffHistory[m_curQp + i].GetCoeff() * r);
        }

    brcprintf("rrate=%6.3f newCoeff=%5.3f\n", realRatePerMb, m_rateCoeffHistory[m_curQp].GetCoeff());

    return 0;
}

mfxU32 LookAheadBrc2::GetMinFrameSize()
{
    return 0;
}


void Hrd::Setup(MfxVideoParam const & par)
{
    m_rcMethod = par.mfx.RateControlMethod;
    if (m_rcMethod != MFX_RATECONTROL_CBR &&
        m_rcMethod != MFX_RATECONTROL_VBR &&
        m_rcMethod != MFX_RATECONTROL_WIDI_VBR)
        m_rcMethod = MFX_RATECONTROL_CBR;

// MVC BD {
    // for ViewOutput mode HRD should be controlled for every view separately
    mfxExtCodingOption * opts = GetExtBuffer(par);
    if (IsMvcProfile(par.mfx.CodecProfile) && opts->ViewOutput == MFX_CODINGOPTION_ON)
    {
        m_bitrate  = GetMaxBitrateValue(par.calcParam.mvcPerViewPar.maxKbps) << 6;
        m_hrdIn90k = mfxU32(8000.0 * par.calcParam.mvcPerViewPar.bufferSizeInKB / m_bitrate * 90000.0);
    }
    else
    {
        m_bitrate  = GetMaxBitrateValue(par.calcParam.maxKbps) << 6;
        m_hrdIn90k = mfxU32(8000.0 * par.calcParam.bufferSizeInKB / m_bitrate * 90000.0);
    }
// MVC BD }
    m_tick     = 0.5 * par.mfx.FrameInfo.FrameRateExtD / par.mfx.FrameInfo.FrameRateExtN;

    m_taf_prv = 0.0;
// MVC BD {
    if (IsMvcProfile(par.mfx.CodecProfile) && opts->ViewOutput == MFX_CODINGOPTION_ON)
        m_trn_cur = double(8000) * par.calcParam.mvcPerViewPar.initialDelayInKB / m_bitrate;
    else
        m_trn_cur = double(8000) * par.calcParam.initialDelayInKB / m_bitrate;
// MVC BD }
    m_trn_cur = GetInitCpbRemovalDelay() / 90000.0;
}

void Hrd::Reset(MfxVideoParam const & par)
{
    m_bitrate  = GetMaxBitrateValue(par.calcParam.maxKbps) << 6;
    m_hrdIn90k = mfxU32(8000.0 * par.calcParam.bufferSizeInKB / m_bitrate * 90000.0);
}

void Hrd::RemoveAccessUnit(mfxU32 size, mfxU32 interlace, mfxU32 bufferingPeriod)
{
    mfxU32 initDelay = GetInitCpbRemovalDelay();

    double tai_earliest = bufferingPeriod
        ? m_trn_cur - (initDelay / 90000.0)
        : m_trn_cur - (m_hrdIn90k / 90000.0);

    double tai_cur = (m_rcMethod == MFX_RATECONTROL_VBR)
        ? IPP_MAX(m_taf_prv, tai_earliest)
        : m_taf_prv;

    m_taf_prv = tai_cur + double(8) * size / m_bitrate;
    m_trn_cur += m_tick * (interlace ? 1 : 2);
}

mfxU32 Hrd::GetInitCpbRemovalDelay() const
{
    double delay = IPP_MAX(0.0, m_trn_cur - m_taf_prv);
    mfxU32 initialCpbRemovalDelay = mfxU32(90000 * delay + 0.5);

    return initialCpbRemovalDelay == 0
        ? 1 // should not be equal to 0
        : initialCpbRemovalDelay > m_hrdIn90k && m_rcMethod == MFX_RATECONTROL_VBR
            ? m_hrdIn90k // should not exceed hrd buffer
            : initialCpbRemovalDelay;
}

mfxU32 Hrd::GetInitCpbRemovalDelayOffset() const
{
    // init_cpb_removal_delay + init_cpb_removal_delay_offset should be constant
    return m_hrdIn90k - GetInitCpbRemovalDelay();
}

InputBitstream::InputBitstream(
    mfxU8 const * buf,
    size_t        size,
    bool          hasStartCode,
    bool          doEmulationControl)
: m_buf(buf)
, m_ptr(buf)
, m_bufEnd(buf + size)
, m_bitOff(0)
, m_doEmulationControl(doEmulationControl)
{
    if (hasStartCode)
        m_ptr = m_buf = SkipStartCode(m_buf, m_bufEnd);
}

InputBitstream::InputBitstream(
    mfxU8 const * buf,
    mfxU8 const * bufEnd,
    bool          hasStartCode,
    bool          doEmulationControl)
: m_buf(buf)
, m_ptr(buf)
, m_bufEnd(bufEnd)
, m_bitOff(0)
, m_doEmulationControl(doEmulationControl)
{
    if (hasStartCode)
        m_ptr = m_buf = SkipStartCode(m_buf, m_bufEnd);
}

mfxU32 InputBitstream::NumBitsRead() const
{
    return mfxU32(8 * (m_ptr - m_buf) + m_bitOff);
}

mfxU32 InputBitstream::NumBitsLeft() const
{
    return mfxU32(8 * (m_bufEnd - m_ptr) - m_bitOff);
}

mfxU32 InputBitstream::GetBit()
{
    if (m_ptr >= m_bufEnd)
        throw EndOfBuffer();

    mfxU32 bit = (*m_ptr >> (7 - m_bitOff)) & 1;

    if (++m_bitOff == 8)
    {
        ++m_ptr;
        m_bitOff = 0;

        if (m_doEmulationControl &&
            m_ptr - m_buf >= 2 && (m_bufEnd - m_ptr) >= 1 &&
            *m_ptr == 0x3 && *(m_ptr - 1) == 0 && *(m_ptr - 2) == 0 && (*(m_ptr + 1) & 0xfc) == 0)
        {
            ++m_ptr; // skip start code emulation prevention byte (0x03)
        }
    }

    return bit;
}

mfxU32 InputBitstream::GetBits(mfxU32 nbits)
{
    mfxU32 bits = 0;
    for (; nbits > 0; --nbits)
    {
        bits <<= 1;
        bits |= GetBit();
    }

    return bits;
}

mfxU32 InputBitstream::GetUe()
{
    mfxU32 zeroes = 0;
    while (GetBit() == 0)
        ++zeroes;

    return zeroes == 0 ? 0 : ((1 << zeroes) | GetBits(zeroes)) - 1;
}

mfxI32 InputBitstream::GetSe()
{
    mfxU32 val = GetUe();
    mfxU32 sign = (val & 1);
    val = (val + 1) >> 1;
    return sign ? val : -mfxI32(val);
}

OutputBitstream::OutputBitstream(mfxU8 * buf, size_t size, bool emulationControl)
: m_buf(buf)
, m_ptr(buf)
, m_bufEnd(buf + size)
, m_bitOff(0)
, m_emulationControl(emulationControl)
{
    if (m_ptr < m_bufEnd)
        *m_ptr = 0; // clear next byte
}

OutputBitstream::OutputBitstream(mfxU8 * buf, mfxU8 * bufEnd, bool emulationControl)
: m_buf(buf)
, m_ptr(buf)
, m_bufEnd(bufEnd)
, m_bitOff(0)
, m_emulationControl(emulationControl)
{
    if (m_ptr < m_bufEnd)
        *m_ptr = 0; // clear next byte
}

mfxU32 OutputBitstream::GetNumBits() const
{
    return mfxU32(8 * (m_ptr - m_buf) + m_bitOff);
}

void OutputBitstream::PutBit(mfxU32 bit)
{
    if (m_ptr >= m_bufEnd)
        throw EndOfBuffer();

    mfxU8 mask = mfxU8(0xff << (8 - m_bitOff));
    mfxU8 newBit = mfxU8(bit << (7 - m_bitOff));
    *m_ptr = (*m_ptr & mask) | newBit;

    if (++m_bitOff == 8)
    {
        if (m_emulationControl && m_ptr - 2 >= m_buf &&
            (*m_ptr & 0xfc) == 0 && *(m_ptr - 1) == 0 && *(m_ptr - 2) == 0)
        {
            if (m_ptr + 1 >= m_bufEnd)
                throw EndOfBuffer();

            *(m_ptr + 1) = *(m_ptr + 0);
            *(m_ptr + 0) = 0x03;
            m_ptr++;
        }

        m_bitOff = 0;
        m_ptr++;
        if (m_ptr < m_bufEnd)
            *m_ptr = 0; // clear next byte
    }
}

void OutputBitstream::PutBits(mfxU32 val, mfxU32 nbits)
{
    assert(nbits <= 32);

    for (; nbits > 0; nbits--)
        PutBit((val >> (nbits - 1)) & 1);
}

void OutputBitstream::PutUe(mfxU32 val)
{
    if (val == 0)
    {
        PutBit(1);
    }
    else
    {
        val++;
        mfxU32 nbits = 1;
        while (val >> nbits)
            nbits++;

        PutBits(0, nbits - 1);
        PutBits(val, nbits);
    }
}

void OutputBitstream::PutSe(mfxI32 val)
{
    (val <= 0)
        ? PutUe(-2 * val)
        : PutUe( 2 * val - 1);
}

void OutputBitstream::PutTrailingBits()
{
    PutBit(1);
    while (m_bitOff != 0)
        PutBit(0);
}

void OutputBitstream::PutRawBytes(mfxU8 const * begin, mfxU8 const * end)
{
    assert(m_bitOff == 0);

    if (m_bufEnd - m_ptr < end - begin)
        throw EndOfBuffer();

    memcpy(m_ptr, begin, end - begin);
    m_bitOff = 0;
    m_ptr += end - begin;

    if (m_ptr < m_bufEnd)
        *m_ptr = 0;
}

void OutputBitstream::PutFillerBytes(mfxU8 filler, mfxU32 nbytes)
{
    assert(m_bitOff == 0);

    if (m_ptr + nbytes > m_bufEnd)
        throw EndOfBuffer();

    memset(m_ptr, filler, nbytes);
    m_ptr += nbytes;
}

void MfxHwH264Encode::PutSeiHeader(
    OutputBitstream & bs,
    mfxU32            payloadType,
    mfxU32            payloadSize)
{
    while (payloadType >= 255)
    {
        bs.PutBits(0xff, 8);
        payloadType -= 255;
    }

    bs.PutBits(payloadType, 8);

    while (payloadSize >= 255)
    {
        bs.PutBits(0xff, 8);
        payloadSize -= 255;
    }

    bs.PutBits(payloadSize, 8);
}

void MfxHwH264Encode::PutSeiMessage(
    OutputBitstream &                   bs,
    mfxExtAvcSeiBufferingPeriod const & msg)
{
    mfxU32 dataSizeInBits =
        2 * msg.initial_cpb_removal_delay_length * (msg.nal_cpb_cnt + msg.vcl_cpb_cnt);

    dataSizeInBits += CeilLog2(msg.seq_parameter_set_id) * 2 + 1;
    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    PutSeiHeader(bs, SEI_TYPE_BUFFERING_PERIOD, dataSizeInBytes);
    bs.PutUe(msg.seq_parameter_set_id);

    for (mfxU32 i = 0; i < msg.nal_cpb_cnt; i++)
    {
        bs.PutBits(msg.nal_initial_cpb_removal_delay[i], msg.initial_cpb_removal_delay_length);
        bs.PutBits(msg.nal_initial_cpb_removal_delay_offset[i], msg.initial_cpb_removal_delay_length);
    }

    for (mfxU32 i = 0; i < msg.vcl_cpb_cnt; i++)
    {
        bs.PutBits(msg.vcl_initial_cpb_removal_delay[i], msg.initial_cpb_removal_delay_length);
        bs.PutBits(msg.vcl_initial_cpb_removal_delay_offset[i], msg.initial_cpb_removal_delay_length);
    }

    if (bs.GetNumBits() & 7)
    {
        bs.PutBit(1);
        while (bs.GetNumBits() & 7)
            bs.PutBit(0);
    }
}

void MfxHwH264Encode::PutSeiMessage(
    OutputBitstream &              bs,
    mfxExtPictureTimingSEI const & extPt,
    mfxExtAvcSeiPicTiming const &  msg)
{
    mfxU32 dataSizeInBits = 0;

    if (msg.cpb_dpb_delays_present_flag)
    {
        dataSizeInBits += msg.cpb_removal_delay_length;
        dataSizeInBits += msg.dpb_output_delay_length;
    }

    if (msg.pic_struct_present_flag)
    {
        dataSizeInBits += 4; // msg.pic_struct;

        assert(msg.pic_struct <= 8);
        mfxU32 numClockTS = NUM_CLOCK_TS[IPP_MIN(msg.pic_struct, 8)];

        dataSizeInBits += numClockTS; // clock_timestamp_flag[i]
        for (mfxU32 i = 0; i < numClockTS; i++)
        {
            if (extPt.TimeStamp[i].ClockTimestampFlag)
            {
                mfxU32 tsSize = 19;

                if (extPt.TimeStamp[i].FullTimestampFlag)
                {
                    tsSize += 17;
                }
                else
                {
                    tsSize += ((
                        extPt.TimeStamp[i].HoursFlag * 5 + 7) *
                        extPt.TimeStamp[i].MinutesFlag + 7) *
                        extPt.TimeStamp[i].SecondsFlag + 1;
                }

                dataSizeInBits += tsSize + msg.time_offset_length;
            }
        }
    }

    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;
    PutSeiHeader(bs, SEI_TYPE_PIC_TIMING, dataSizeInBytes);

    if (msg.cpb_dpb_delays_present_flag)
    {
        bs.PutBits(msg.cpb_removal_delay, msg.cpb_removal_delay_length);
        bs.PutBits(msg.dpb_output_delay, msg.dpb_output_delay_length);
    }

    if (msg.pic_struct_present_flag)
    {
        assert(msg.pic_struct <= 8);
        mfxU32 numClockTS = NUM_CLOCK_TS[IPP_MIN(msg.pic_struct, 8)];

        bs.PutBits(msg.pic_struct, 4);
        for (mfxU32 i = 0; i < numClockTS; i ++)
        {
            bs.PutBit(extPt.TimeStamp[i].ClockTimestampFlag);
            if (extPt.TimeStamp[i].ClockTimestampFlag)
            {
                mfxU32 ctType = (extPt.TimeStamp[i].CtType == 0xffff)
                    ? msg.ct_type                // based on picstruct
                    : extPt.TimeStamp[i].CtType; // user-defined

                bs.PutBits(ctType, 2);
                bs.PutBit (extPt.TimeStamp[i].NuitFieldBasedFlag);
                bs.PutBits(extPt.TimeStamp[i].CountingType, 5);
                bs.PutBit (extPt.TimeStamp[i].FullTimestampFlag);
                bs.PutBit (extPt.TimeStamp[i].DiscontinuityFlag);
                bs.PutBit (extPt.TimeStamp[i].CntDroppedFlag);
                bs.PutBits(extPt.TimeStamp[i].NFrames, 8);

                if (extPt.TimeStamp[i].FullTimestampFlag)
                {
                    bs.PutBits(extPt.TimeStamp[i].SecondsValue, 6);
                    bs.PutBits(extPt.TimeStamp[i].MinutesValue, 6);
                    bs.PutBits(extPt.TimeStamp[i].HoursValue,   5);
                }
                else
                {
                    bs.PutBit(extPt.TimeStamp[i].SecondsFlag);
                    if (extPt.TimeStamp[i].SecondsFlag)
                    {
                        bs.PutBits(extPt.TimeStamp[i].SecondsValue, 6);
                        bs.PutBit(extPt.TimeStamp[i].MinutesFlag);
                        if (extPt.TimeStamp[i].MinutesFlag)
                        {
                            bs.PutBits(extPt.TimeStamp[i].MinutesValue, 6);
                            bs.PutBit(extPt.TimeStamp[i].HoursFlag);
                            if (extPt.TimeStamp[i].HoursFlag)
                                bs.PutBits(extPt.TimeStamp[i].HoursValue, 5);
                        }
                    }
                }

                bs.PutBits(extPt.TimeStamp[i].TimeOffset, msg.time_offset_length);
            }
        }
    }

    if (bs.GetNumBits() & 7)
    {
        bs.PutBit(1);
        while (bs.GetNumBits() & 7)
            bs.PutBit(0);
    }
}

void MfxHwH264Encode::PutSeiMessage(
    OutputBitstream &                   bs,
    mfxExtAvcSeiDecRefPicMrkRep const & msg)
{
    mfxU32 dataSizeInBits = 0, i;

    // calculate size of sei_payload
    dataSizeInBits += CeilLog2(msg.original_frame_num) * 2 + 2; // original_frame_num + original_idr_flag

    if (msg.original_field_info_present_flag)
        dataSizeInBits += msg.original_field_pic_flag == 0 ? 1 : 2; // original_field_info_present_flag + original_bottom_field_flag

    if (msg.original_idr_flag) {
        dataSizeInBits += 2; // no_output_of_prior_pics_flag + long_term_reference_flag
    }
    else {
        dataSizeInBits += 1; // adaptive_ref_pic_marking_mode_flag
        for (i = 0; i < msg.num_mmco_entries; i ++) {
            dataSizeInBits += CeilLog2(msg.mmco[i]) * 2 + 1; // memory_management_control_operation
            dataSizeInBits += CeilLog2(msg.value[2 * i]) * 2 + 1;
            if (msg.mmco[i] == 3)
                dataSizeInBits += CeilLog2(msg.value[2 * i + 1]) * 2 + 1;
        }
    }

    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    PutSeiHeader(bs, SEI_TYPE_DEC_REF_PIC_MARKING_REPETITION, dataSizeInBytes);

    bs.PutBit(msg.original_idr_flag);
    bs.PutUe(msg.original_frame_num);
    if (msg.original_field_info_present_flag)
    {
        bs.PutBit(msg.original_field_pic_flag);
        if (msg.original_field_pic_flag)
            bs.PutBit(msg.original_bottom_field_flag);
    }

    // put dec_ref_pic_marking() syntax
    if (msg.original_idr_flag) {
        bs.PutBit(msg.no_output_of_prior_pics_flag);
        bs.PutBit(msg.long_term_reference_flag);
    }
    else {
        bs.PutBit(msg.adaptive_ref_pic_marking_mode_flag);
        for (i = 0; i < msg.num_mmco_entries; i ++) {
            bs.PutUe(msg.mmco[i]);
            bs.PutUe(msg.value[2 * i]);
            if (msg.mmco[i] == 3)
                bs.PutUe(msg.value[2 * i + 1]);
        }
    }

    if (bs.GetNumBits() & 7)
    {
        bs.PutBit(1);
        while (bs.GetNumBits() & 7)
            bs.PutBit(0);
    }
}

void MfxHwH264Encode::PutSeiMessage(
        OutputBitstream &    bs,
        mfxExtAvcSeiRecPoint const & msg)
{
    mfxU32 dataSizeInBits = CeilLog2(msg.recovery_frame_cnt) * 2 + 1; // size of recovery_frame_cnt
    dataSizeInBits += 4; // exact_match_flag + broken_link_flag + changing_slice_group_idc
    mfxU32 dataSizeInBytes = (dataSizeInBits + 7) >> 3;

    PutSeiHeader(bs, SEI_TYPE_RECOVERY_POINT, dataSizeInBytes);

    bs.PutUe(msg.recovery_frame_cnt);
    bs.PutBit(msg.exact_match_flag);
    bs.PutBit(msg.broken_link_flag);
    bs.PutBits(msg.changing_slice_group_idc, 2);

    if (bs.GetNumBits() & 7)
    {
        bs.PutBit(1);
        while (bs.GetNumBits() & 7)
            bs.PutBit(0);
    }
}

namespace
{
    mfxU32 PutScalableInfoSeiPayload(
        OutputBitstream &     obs,
        MfxVideoParam const & par)
    {
        mfxU32 initialNumBits = obs.GetNumBits();

        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        mfxU32 numLayers = par.calcParam.numLayersTotal;

        mfxU32 temporal_id_nesting_flag = 0;
        mfxU32 priority_layer_info_present_flag = 0;
        mfxU32 priority_id_setting_flag = 0;

        obs.PutBit(temporal_id_nesting_flag);
        obs.PutBit(priority_layer_info_present_flag);
        obs.PutBit(priority_id_setting_flag);
        obs.PutUe(numLayers - 1);

        mfxU32 layer = 0;
        for (mfxU32 d = 0; d < par.calcParam.numDependencyLayer; d++)
        {
            mfxU32 did = par.calcParam.did[d];
            for (mfxU32 t = 0; t < extSvc->DependencyLayer[did].TemporalNum; t++)
            {
                mfxU32 tid = extSvc->DependencyLayer[did].TemporalId[t];
                for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++, layer++)
                {
                    mfxU8  priority_id                              = 0;
                    mfxU8  discardable_flag                         = 0;
                    mfxU8  sub_pic_layer_flag                       = 0;
                    mfxU8  sub_region_layer_flag                    = 0;
                    mfxU8  iroi_division_info_present_flag          = 0;
                    mfxU8  profile_level_info_present_flag          = 0;
                    mfxU8  bitrate_info_present_flag                = 0;
                    mfxU8  frm_rate_info_present_flag               = 0;
                    mfxU8  frm_size_info_present_flag               = 0;
                    mfxU8  layer_dependency_info_present_flag       = 0;
                    mfxU8  parameter_sets_info_present_flag         = 0;
                    mfxU8  bitstream_restriction_info_present_flag  = 0;
                    mfxU8  exact_inter_layer_pred_flag              = 0;
                    mfxU8  layer_conversion_flag                    = 0;
                    mfxU8  layer_output_flag                        = 0;
                    mfxU8  layer_dependency_info_src_layer_id_delta = 0;
                    mfxU8  parameter_sets_info_src_layer_id_delta   = 0;

                    obs.PutUe(layer);
                    obs.PutBits(priority_id, 6);
                    obs.PutBit(discardable_flag);
                    obs.PutBits(did, 3);
                    obs.PutBits(qid, 4);
                    obs.PutBits(tid, 3);
                    obs.PutBit(sub_pic_layer_flag);
                    obs.PutBit(sub_region_layer_flag);
                    obs.PutBit(iroi_division_info_present_flag);
                    obs.PutBit(profile_level_info_present_flag);
                    obs.PutBit(bitrate_info_present_flag);
                    obs.PutBit(frm_rate_info_present_flag);
                    obs.PutBit(frm_size_info_present_flag);
                    obs.PutBit(layer_dependency_info_present_flag);
                    obs.PutBit(parameter_sets_info_present_flag);
                    obs.PutBit(bitstream_restriction_info_present_flag);
                    obs.PutBit(exact_inter_layer_pred_flag);
                    obs.PutBit(layer_conversion_flag);
                    obs.PutBit(layer_output_flag);
                    if (layer_dependency_info_present_flag)
                        ;
                    else
                        obs.PutUe(layer_dependency_info_src_layer_id_delta);
                    if (parameter_sets_info_present_flag)
                        ;
                    else
                        obs.PutUe(parameter_sets_info_src_layer_id_delta);
                }
            }
        }

        if (obs.GetNumBits() & 7)
        {
            obs.PutBit(1);
            while (obs.GetNumBits() & 7)
                obs.PutBit(0);
        }

        return obs.GetNumBits() - initialNumBits;
    }
};

mfxU32 MfxHwH264Encode::PutScalableInfoSeiMessage(
    OutputBitstream &     obs,
    MfxVideoParam const & par)
{
    OutputBitstream tmp = obs;
    mfxU32 lengthInBytes = (PutScalableInfoSeiPayload(tmp, par) + 7) / 8;

    mfxU32 initialNumBits = obs.GetNumBits();
    mfxU8 const header[5] = { 0, 0, 0, 1, 6 };
    obs.PutRawBytes(header, header + sizeof(header));
    PutSeiHeader(obs, SEI_TYPE_SCALABILITY_INFO, lengthInBytes);
    PutScalableInfoSeiPayload(obs, par);
    obs.PutTrailingBits();

    return obs.GetNumBits() - initialNumBits;
}

// MVC BD {
void MfxHwH264Encode::PutSeiMessage(
    OutputBitstream &                   bs,
    mfxU32 needBufferingPeriod,
    mfxU32 needPicTimingSei,
    mfxU32 fillerSize,
    MfxVideoParam const & video,
    mfxExtAvcSeiBufferingPeriod const & msg_bp,
    mfxExtPictureTimingSEI const & extPt,
    mfxExtAvcSeiPicTiming const &  msg_pt)
{

    if (needBufferingPeriod == 0 && needPicTimingSei == 0 && fillerSize == 0)
        return;

    mfxExtMVCSeqDesc * extMvc = GetExtBuffer(video);

    mfxU32 dataSizeInBytes = 2; // hardcoded 2 bytes for MVC nested SEI syntax prior sei_messages (1 view in op)

    if (needBufferingPeriod)
    {
        dataSizeInBytes += 2; // hardcoded 2 bytes on BP sei_message() header. TODO: calculate real size of header
        dataSizeInBytes += CalculateSeiSize(msg_bp); // calculate size of BP SEI payload
    }

    if (needPicTimingSei)
    {
        dataSizeInBytes += 2; // hardcoded 2 bytes on PT sei_message() header. TODO: calculate real size of header
        dataSizeInBytes += CalculateSeiSize(extPt, msg_pt); // calculate size of PT SEI payload
    }

    if (fillerSize)
    {
        fillerSize -= fillerSize / 256; // compensate part of header
        dataSizeInBytes += 1; // last_payload_type_byte for filler payload
        dataSizeInBytes += (fillerSize + 254) / 255; // ff_bytes + last_payload_size_byte
        dataSizeInBytes += fillerSize; // filler payload
    }

    PutSeiHeader(bs, SEI_TYPE_MVC_SCALABLE_NESTING, dataSizeInBytes);

    bs.PutBit(1); // put operation_point_flag = 1
    bs.PutUe(0); // put num_view_components_op_minus1 = 0
    bs.PutBits(extMvc->View[1].ViewId, 10); // put sei_op_view_id[0]
    bs.PutBits(0, 3); // sei_op_temporal_id
    bs.PutBits(0, 1); // put sei_nesting_zero_bits

    if (needBufferingPeriod)
        PutSeiMessage(bs, msg_bp);
    if (needPicTimingSei)
        PutSeiMessage(bs, extPt, msg_pt);
    if (fillerSize)
    {
        // how many bytes takes to encode payloadSize depends on size of sei message
        // need to compensate it
        PutSeiHeader(bs, SEI_TYPE_FILLER_PAYLOAD, fillerSize);
        bs.PutFillerBytes(0xff, fillerSize);
    }

}
// MVC BD }

MfxFrameAllocResponse::MfxFrameAllocResponse()
    : m_core(0)
    , m_cmDevice(0)
    , m_numFrameActualReturnedByAllocFrames(0)
    , m_cmDestroy(0)
{
    Zero((mfxFrameAllocResponse &)*this);
}

MfxFrameAllocResponse::~MfxFrameAllocResponse()
{
    if (m_core)
    {
        if (MFX_HW_D3D11  == m_core->GetVAType())
        {
            for (size_t i = 0; i < m_responseQueue.size(); i++)
                m_core->FreeFrames(&m_responseQueue[i]);
        }
        else
        {
            if (mids)
            {
                NumFrameActual = m_numFrameActualReturnedByAllocFrames;
                m_core->FreeFrames(this);
            }
        }
    }

    if (m_cmDevice)
    {
        for (size_t i = 0; i < m_mids.size(); i++)
            if (m_mids[i])
                m_cmDestroy(m_cmDevice, m_mids[i]);

        for (size_t i = 0; i < m_sysmems.size(); i++)
        {
            if (m_sysmems[i])
            {
                CM_ALIGNED_FREE(m_sysmems[i]);
                m_sysmems[i] = 0;
            }
        }
    }
}

void MfxFrameAllocResponse::DestroyBuffer(CmDevice * device, void * p)
{
    device->DestroySurface((CmBuffer *&)p);
}

void MfxFrameAllocResponse::DestroySurface(CmDevice * device, void * p)
{
    device->DestroySurface((CmSurface2D *&)p);
}

void MfxFrameAllocResponse::DestroyBufferUp(CmDevice * device, void * p)
{
    device->DestroyBufferUP((CmBufferUP *&)p);
}

mfxStatus MfxFrameAllocResponse::Alloc(
    VideoCORE *            core,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return Error(MFX_ERR_MEMORY_ALLOC);

    req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

    if (MFX_HW_D3D11  == core->GetVAType())
    {
        mfxFrameAllocRequest tmp = req;
        tmp.NumFrameMin = tmp.NumFrameSuggested = 1;

        m_responseQueue.resize(req.NumFrameMin);
        m_mids.resize(req.NumFrameMin);

        for (int i = 0; i < req.NumFrameMin; i++)
        {
            mfxStatus sts = core->AllocFrames(&tmp, &m_responseQueue[i]);
            MFX_CHECK_STS(sts);
            m_mids[i] = m_responseQueue[i].mids[0];
        }

        mids = &m_mids[0];
        NumFrameActual = req.NumFrameMin;
    }
    else
    {
        mfxStatus sts = core->AllocFrames(&req, this);
        MFX_CHECK_STS(sts);
    }

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_locked.resize(req.NumFrameMin, 0);

    m_core = core;
    m_cmDevice = 0;
    m_cmDestroy = 0;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin; // no need in redundant frames
    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::Alloc(
    VideoCORE *            core,
    mfxFrameAllocRequest & req,
    mfxFrameSurface1 **    opaqSurf,
    mfxU32                 numOpaqSurf)
{
    if (m_core || m_cmDevice)
        return Error(MFX_ERR_MEMORY_ALLOC);

    req.NumFrameSuggested = req.NumFrameMin; // no need in 2 different NumFrames

    mfxStatus sts = core->AllocFrames(&req, this, opaqSurf, numOpaqSurf);
    MFX_CHECK_STS(sts);

    if (NumFrameActual < req.NumFrameMin)
        return MFX_ERR_MEMORY_ALLOC;

    m_core = core;
    m_cmDevice = 0;
    m_cmDestroy = 0;
    m_numFrameActualReturnedByAllocFrames = NumFrameActual;
    NumFrameActual = req.NumFrameMin; // no need in redundant frames
    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::AllocCmBuffers(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return Error(MFX_ERR_MEMORY_ALLOC);

    req.NumFrameSuggested = req.NumFrameMin;
    mfxU32 size = req.Info.Width * req.Info.Height;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    for (int i = 0; i < req.NumFrameMin; i++)
        m_mids[i] = CreateBuffer(device, size);

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroyBuffer;
    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::AllocCmSurfaces(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return Error(MFX_ERR_MEMORY_ALLOC);

    req.NumFrameSuggested = req.NumFrameMin;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    for (int i = 0; i < req.NumFrameMin; i++)
        m_mids[i] = CreateSurface(device, req.Info.Width, req.Info.Height, req.Info.FourCC);

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroySurface;
    return MFX_ERR_NONE;
}

mfxStatus MfxFrameAllocResponse::AllocCmBuffersUp(
    CmDevice *             device,
    mfxFrameAllocRequest & req)
{
    if (m_core || m_cmDevice)
        return Error(MFX_ERR_MEMORY_ALLOC);

    req.NumFrameSuggested = req.NumFrameMin;
    mfxU32 size = req.Info.Width * req.Info.Height;

    m_mids.resize(req.NumFrameMin, 0);
    m_locked.resize(req.NumFrameMin, 0);
    m_sysmems.resize(req.NumFrameMin, 0);   

    for (int i = 0; i < req.NumFrameMin; i++)
    {
        m_sysmems[i] = CM_ALIGNED_MALLOC(size, 0x1000);
        m_mids[i] = CreateBuffer(device, size, m_sysmems[i]);
    }

    NumFrameActual = req.NumFrameMin;
    mids = &m_mids[0];

    m_core     = 0;
    m_cmDevice = device;
    m_cmDestroy = &DestroyBufferUp;
    return MFX_ERR_NONE;
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

void * MfxFrameAllocResponse::GetSysmemBuffer(mfxU32 idx)
{
    return (idx < m_sysmems.size()) ? m_sysmems[idx] : 0;
}

mfxU32 MfxHwH264Encode::FindFreeResourceIndex(
    MfxFrameAllocResponse const & pool,
    mfxU32                        startingFrom)
{
    for (mfxU32 i = startingFrom; i < pool.NumFrameActual; i++)
        if (pool.Locked(i) == 0)
            return i;
    return NO_INDEX;
}

mfxMemId MfxHwH264Encode::AcquireResource(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    if (index > pool.NumFrameActual)
        return MID_INVALID;
    pool.Lock(index);
    return pool.mids[index];
}

mfxMemId MfxHwH264Encode::AcquireResource(
    MfxFrameAllocResponse & pool)
{
    return AcquireResource(pool, FindFreeResourceIndex(pool));
}

mfxHDLPair MfxHwH264Encode::AcquireResourceUp(
    MfxFrameAllocResponse & pool,
    mfxU32                  index)
{
    mfxHDLPair p = { 0, 0 };
    if (index > pool.NumFrameActual)
        return p;
    pool.Lock(index);
    p.first  = pool.mids[index];
    p.second = pool.GetSysmemBuffer(index);
    return p;
}

mfxHDLPair MfxHwH264Encode::AcquireResourceUp(
    MfxFrameAllocResponse & pool)
{
    return AcquireResourceUp(pool, FindFreeResourceIndex(pool));
}

void MfxHwH264Encode::ReleaseResource(
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


namespace
{
    mfxStatus CheckEncryptedData(
        mfxEncryptedData const & data,
        mfxU32                   maxBsSize)
    {
        MFX_CHECK(data.DataOffset <= data.MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK_NULL_PTR1(data.Data);
        MFX_CHECK(data.DataOffset + data.DataLength + maxBsSize <= data.MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        return MFX_ERR_NONE;
    }

    mfxStatus CheckEncryptedBitstream(
        mfxBitstream const & bs,
        mfxU32               picStruct,
        mfxU32               maxBsSize)
    {
        if (picStruct & MFX_PICSTRUCT_PROGRESSIVE)
        {
            MFX_CHECK_NULL_PTR1(bs.EncryptedData);
            mfxStatus sts = CheckEncryptedData(*bs.EncryptedData, maxBsSize);
            MFX_CHECK_STS(sts);
        }
        else
        {
            MFX_CHECK_NULL_PTR1(bs.EncryptedData);
            mfxStatus sts = CheckEncryptedData(*bs.EncryptedData, maxBsSize / 2);
            MFX_CHECK_STS(sts);

            MFX_CHECK_NULL_PTR1(bs.EncryptedData->Next);
            sts = CheckEncryptedData(*bs.EncryptedData->Next, maxBsSize / 2);
            MFX_CHECK_STS(sts);
        }

        return MFX_ERR_NONE;
    }
};


mfxStatus MfxHwH264Encode::CheckEncodeFrameParam(
    MfxVideoParam const & video,
    mfxEncodeCtrl *       ctrl,
    mfxFrameSurface1 *    surface,
    mfxBitstream *        bs,
    bool                  isExternalFrameAllocator)
{
    mfxStatus checkSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(bs);

    if (video.Protected == 0)
    {
        mfxU32 bufferSizeInKB = 0;
        if (IsMvcProfile(video.mfx.CodecProfile))
            bufferSizeInKB = video.calcParam.mvcPerViewPar.bufferSizeInKB;
        else
            bufferSizeInKB = video.calcParam.bufferSizeInKB;

        MFX_CHECK(bs->DataOffset <= bs->MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(bs->DataOffset + bs->DataLength + bufferSizeInKB * 1000u <= bs->MaxLength,
            MFX_ERR_NOT_ENOUGH_BUFFER);
        MFX_CHECK_NULL_PTR1(bs->Data);
    }
    else
    {
        mfxStatus sts = CheckEncryptedBitstream(
            *bs,
            video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1,
            1000u * video.calcParam.bufferSizeInKB);
        MFX_CHECK_STS(sts);
    }

    if (video.mfx.EncodedOrder == 1)
    {
        MFX_CHECK(surface != 0, MFX_ERR_MORE_DATA);
        MFX_CHECK_NULL_PTR1(ctrl);

        mfxU16 firstFieldType  = ctrl->FrameType & 0x07;
        mfxU16 secondFieldType = (ctrl->FrameType >> 8) & 0x07;

        // check frame type
        MFX_CHECK(
            firstFieldType == MFX_FRAMETYPE_I ||
            firstFieldType == MFX_FRAMETYPE_P ||
            firstFieldType == MFX_FRAMETYPE_B,
            MFX_ERR_INVALID_VIDEO_PARAM);

        if (secondFieldType)
        {
            MFX_CHECK(
                firstFieldType == MFX_FRAMETYPE_I ||
                firstFieldType == MFX_FRAMETYPE_P ||
                firstFieldType == MFX_FRAMETYPE_B,
                MFX_ERR_INVALID_VIDEO_PARAM);

            // check compatibility of fields types
            MFX_CHECK(
                firstFieldType == secondFieldType ||
                firstFieldType == MFX_FRAMETYPE_I && secondFieldType == MFX_FRAMETYPE_P,
                MFX_ERR_INVALID_VIDEO_PARAM);
        }
    }
    else
    {
        if (ctrl)
        {
            if (ctrl->FrameType)
            {
                // check FrameType for forced key-frame generation
                mfxU16 type = ctrl->FrameType & (MFX_FRAMETYPE_IPB | MFX_FRAMETYPE_xIPB);
                MFX_CHECK(
                    type == (MFX_FRAMETYPE_I)                    ||
                    type == (MFX_FRAMETYPE_I | MFX_FRAMETYPE_xI) ||
                    type == (MFX_FRAMETYPE_I | MFX_FRAMETYPE_xP),
                    MFX_ERR_INVALID_VIDEO_PARAM);
            }

            mfxExtAVCRefListCtrl * extRefListCtrl = GetExtBuffer(*ctrl);
            if (extRefListCtrl && video.calcParam.numTemporalLayer > 0 && video.calcParam.lyncMode == 0)
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    if (surface != 0)
    {
        mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(video);
        bool opaq = extOpaq->In.Surfaces != 0;

        MFX_CHECK((surface->Data.Y == 0) == (surface->Data.UV == 0), MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Data.Y != 0 || isExternalFrameAllocator || opaq, MFX_ERR_UNDEFINED_BEHAVIOR);
        MFX_CHECK(surface->Info.Width >= video.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height >= video.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);

        mfxStatus sts = CheckRunTimePicStruct(surface->Info.PicStruct, video.mfx.FrameInfo.PicStruct);
        if (sts < MFX_ERR_NONE)
            return sts;
        else if (sts > MFX_ERR_NONE)
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (ctrl != 0 && ctrl->NumPayload > 0)
    {
        MFX_CHECK_NULL_PTR1(ctrl->Payload);

        mfxStatus sts = CheckPayloads(ctrl->Payload, ctrl->NumPayload);
        MFX_CHECK_STS(sts);
    }

    return checkSts;
}

mfxStatus MfxHwH264Encode::CheckBeforeCopy(mfxExtMVCSeqDesc & dst, mfxExtMVCSeqDesc const & src)
{
    if (dst.NumViewAlloc   < src.NumView   ||
        dst.NumViewIdAlloc < src.NumViewId ||
        dst.NumOPAlloc     < src.NumOP)
    {
        dst.NumView   = src.NumView;
        dst.NumViewId = src.NumViewId;
        dst.NumOP     = src.NumOP;
        return MFX_ERR_NOT_ENOUGH_BUFFER;
    }

    if (dst.View == 0 || dst.ViewId == 0 || dst.OP == 0)
    {
        return MFX_ERR_NULL_PTR;
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::CheckBeforeCopyQueryLike(mfxExtMVCSeqDesc & dst, mfxExtMVCSeqDesc const & src)
{
    if (dst.View   && dst.NumViewAlloc   < src.NumView   ||
        dst.ViewId && dst.NumViewIdAlloc < src.NumViewId ||
        dst.OP     && dst.NumOPAlloc     < src.NumOP)
    {
        dst.NumView   = src.NumView;
        dst.NumViewId = src.NumViewId;
        dst.NumOP     = src.NumOP;
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

void MfxHwH264Encode::Copy(mfxExtMVCSeqDesc & dst, mfxExtMVCSeqDesc const & src)
{
    if (dst.View)
    {
        dst.NumView = src.NumView;
        std::copy(src.View,   src.View   + src.NumView,   dst.View);
    }

    if (dst.ViewId)
    {
        dst.NumViewId = src.NumViewId;
        std::copy(src.ViewId, src.ViewId + src.NumViewId, dst.ViewId);
    }

    if (dst.OP)
    {
        dst.NumOP = src.NumOP;
        for (mfxU32 i = 0; i < dst.NumOP; i++)
        {
            dst.OP[i].TemporalId     = src.OP[i].TemporalId;
            dst.OP[i].LevelIdc       = src.OP[i].LevelIdc;
            dst.OP[i].NumViews       = src.OP[i].NumViews;
            dst.OP[i].NumTargetViews = src.OP[i].NumTargetViews;
            dst.OP[i].TargetViewId   = &dst.ViewId[src.OP[i].TargetViewId - src.ViewId];
        }
    }

    dst.NumRefsTotal = src.NumRefsTotal;
}

void MfxHwH264Encode::FastCopyBufferVid2Sys(void * dstSys, void const * srcVid, mfxI32 bytes)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Surface copy (bitstream)");

    assert(dstSys != 0);
    assert(srcVid != 0);

    IppiSize roi = { bytes, 1 };
    IppStatus sts = ippiCopyManaged_8u_C1R((Ipp8u *)srcVid, bytes, (Ipp8u *)dstSys, bytes, roi, IPP_NONTEMPORAL_LOAD);
    assert(sts == ippStsNoErr); sts;
}

void CyclicTaskPool::Init(mfxU32 size)
{
    m_pool.resize(size);
    m_next = m_pool.begin();
}

DdiTask2ndField * CyclicTaskPool::GetFreeTask()
{
    if (m_pool.empty())
        return 0;
    if (m_next == m_pool.end())
        m_next = m_pool.begin();
    return &*(m_next++);
}

mfxU8 const * MfxHwH264Encode::SkipStartCode(mfxU8 const * begin, mfxU8 const * end)
{
    mfxU32 threeBytePrefix = (end - begin < 3)
        ? 0xffffffff
        : (begin[0] << 16) | (begin[1] << 8) | (begin[2]);

    if (threeBytePrefix == 1)
        return begin + 3;
    else if (threeBytePrefix == 0 && end - begin > 3 && begin[3] == 1)
        return begin + 4;
    else
        return begin;
}

mfxU8 * MfxHwH264Encode::SkipStartCode(mfxU8 * begin, mfxU8 * end)
{
    return const_cast<mfxU8 *>(SkipStartCode(const_cast<const mfxU8 *>(begin), const_cast<const mfxU8 *>(end)));
}

ArrayRefListMod MfxHwH264Encode::CreateRefListMod(
    ArrayDpbFrame const &            dpb,
    std::vector<Reconstruct> const & recons,
    ArrayU8x33                       initList,
    ArrayU8x33 const &               modList,
    mfxU32                           curViewIdx,
    mfxI32                           curPicNum,
    bool                             optimize)
{
    assert(initList.Size() == modList.Size());

    ArrayRefListMod refListMod;

    mfxI32 picNumPred     = curPicNum;
    mfxI32 picViewIdxPred = -1;

    for (mfxU32 refIdx = 0; refIdx < modList.Size(); refIdx++)
    {
        if (optimize && initList == modList)
            return refListMod;

        if (dpb[modList[refIdx] & 0x7f].m_viewIdx != curViewIdx)
        {
            // inter-view reference reordering
            mfxI32 viewIdx = dpb[modList[refIdx] & 0x7f].m_viewIdx;

            if (viewIdx > picViewIdxPred)
            {
                refListMod.PushBack(RefListMod(RPLM_INTERVIEW_ADD, mfxU16(viewIdx - picViewIdxPred - 1)));
            }
            else if (viewIdx < picViewIdxPred)
            {
                refListMod.PushBack(RefListMod(RPLM_INTERVIEW_SUB, mfxU16(picViewIdxPred - viewIdx - 1)));
            }
            else
            {
                assert(!"can't reorder ref list");
                break;
            }

            for (mfxU32 cIdx = initList.Size(); cIdx > refIdx; cIdx--)
                initList[cIdx] = initList[cIdx - 1];
            initList[refIdx] = modList[refIdx];
            mfxU32 nIdx = refIdx + 1;
            for (mfxU32 cIdx = refIdx + 1; cIdx <= initList.Size(); cIdx++)
                if (dpb[initList[cIdx] & 0x7f].m_viewIdx != mfxU32(viewIdx))
                    initList[nIdx++] = initList[cIdx];

            picViewIdxPred = viewIdx;
        }
        else if (dpb[modList[refIdx] & 0x7f].m_longterm)
        {
            // long term reference reordering
            mfxU8 longTermPicNum = GetLongTermPicNum(recons, dpb, modList[refIdx]);

            refListMod.PushBack(RefListMod(RPLM_LT_PICNUM, longTermPicNum));

            for (mfxU32 cIdx = initList.Size(); cIdx > refIdx; cIdx--)
                initList[cIdx] = initList[cIdx - 1];
            initList[refIdx] = modList[refIdx];
            mfxU32 nIdx = refIdx + 1;
            for (mfxU32 cIdx = refIdx + 1; cIdx <= initList.Size(); cIdx++)
                if (GetLongTermPicNumF(recons, dpb, initList[cIdx]) != longTermPicNum ||
                    dpb[initList[cIdx] & 0x7f].m_viewIdx != curViewIdx)
                    initList[nIdx++] = initList[cIdx];
        }
        else
        {
            // short term reference reordering
            mfxI32 picNum = GetPicNum(recons, dpb, modList[refIdx]);

            if (picNum > picNumPred)
            {
                mfxU16 absDiffPicNum = mfxU16(picNum - picNumPred);
                refListMod.PushBack(RefListMod(RPLM_ST_PICNUM_ADD, absDiffPicNum - 1));
            }
            else if (picNum < picNumPred)
            {
                mfxU16 absDiffPicNum = mfxU16(picNumPred - picNum);
                refListMod.PushBack(RefListMod(RPLM_ST_PICNUM_SUB, absDiffPicNum - 1));
            }
            else
            {
                assert(!"can't reorder ref list");
                break;
            }

            for (mfxU32 cIdx = initList.Size(); cIdx > refIdx; cIdx--)
                initList[cIdx] = initList[cIdx - 1];
            initList[refIdx] = modList[refIdx];
            mfxU32 nIdx = refIdx + 1;
            for (mfxU32 cIdx = refIdx + 1; cIdx <= initList.Size(); cIdx++)
                if (GetPicNumF(recons, dpb, initList[cIdx]) != picNum ||
                    dpb[initList[cIdx] & 0x7f].m_viewIdx != curViewIdx)
                    initList[nIdx++] = initList[cIdx];

            picNumPred = picNum;
        }
    }

    return refListMod;
}

mfxU8 * MfxHwH264Encode::CheckedMemcpy(
    mfxU8 *       dbegin,
    mfxU8 *       dend,
    mfxU8 const * sbegin,
    mfxU8 const * send)
{
    if (dend - dbegin < send - sbegin)
    {
        assert(0);
        throw EndOfBuffer();
    }

    memcpy(dbegin, sbegin, send - sbegin);
    return dbegin + (send - sbegin);
}


mfxU8 * MfxHwH264Encode::CheckedMemset(
    mfxU8 * dbegin,
    mfxU8 * dend,
    mfxU8   value,
    mfxU32  size)
{
    if (dbegin + size > dend)
    {
        assert(0);
        throw EndOfBuffer();
    }

    memset(dbegin, value, size);
    return dbegin + size;
}


void MfxHwH264Encode::ReadRefPicListModification(
    InputBitstream & reader)
{
    if (reader.GetBit())                    // ref_pic_list_modification_flag_l0
    {
        for (;;)
        {
            mfxU32 tmp = reader.GetUe();    // modification_of_pic_nums_idc

            if (tmp == RPLM_END)
                break;

            if (tmp > RPLM_INTERVIEW_ADD)
            {
                assert(!"bad bitstream");
                throw std::logic_error(": bad bitstream");
            }

            reader.GetUe();                 // abs_diff_pic_num_minus1 or
                                            // long_term_pic_num or
                                            // abs_diff_view_idx_minus1
        }
    }
}

void MfxHwH264Encode::ReadDecRefPicMarking(
    InputBitstream & reader,
    bool             idrPicFlag)
{
    if (idrPicFlag)
    {
        reader.GetBit();                    // no_output_of_prior_pics_flag
        reader.GetBit();                    // long_term_reference_flag
    }
    else
    {
        mfxU32 tmp = reader.GetBit();       // adaptive_ref_pic_marking_mode_flag
        assert(tmp == 0 && "adaptive_ref_pic_marking_mode_flag should be 0");
        tmp;
    }
}

void WriteRefPicListModification(
    OutputBitstream &       writer,
    ArrayRefListMod const & refListMod)
{
    writer.PutBit(refListMod.Size() > 0);       // ref_pic_list_modification_flag_l0
    if (refListMod.Size() > 0)
    {
        for (mfxU32 i = 0; i < refListMod.Size(); i++)
        {
            writer.PutUe(refListMod[i].m_idc);  // modification_of_pic_nums_idc
            writer.PutUe(refListMod[i].m_diff); // abs_diff_pic_num_minus1 or
                                                // long_term_pic_num or
                                                // abs_diff_view_idx_minus1
        }

        writer.PutUe(RPLM_END);
    }
}

void MfxHwH264Encode::WriteDecRefPicMarking(
    OutputBitstream &            writer,
    DecRefPicMarkingInfo const & marking,
    bool                         idrPicFlag)
{
    if (idrPicFlag)
    {
        writer.PutBit(marking.no_output_of_prior_pics_flag);    // no_output_of_prior_pics_flag
        writer.PutBit(marking.long_term_reference_flag);        // long_term_reference_flag
    }
    else
    {
        writer.PutBit(marking.mmco.Size() > 0);                 // adaptive_ref_pic_marking_mode_flag
        if (marking.mmco.Size())
        {
            for (mfxU32 i = 0; i < marking.mmco.Size(); i++)
            {
                writer.PutUe(marking.mmco[i]);                  // memory_management_control_operation
                writer.PutUe(marking.value[2 * i]);             // difference_of_pic_nums_minus1 or
                                                                // long_term_pic_num or
                                                                // long_term_frame_idx or
                                                                // max_long_term_frame_idx_plus1
                if (marking.mmco[i] == MMCO_ST_TO_LT)
                    writer.PutUe(marking.value[2 * i + 1]);     // long_term_frame_idx
            }

            writer.PutUe(MMCO_END);
        }
    }
}


mfxU8 * MfxHwH264Encode::RePackSlice(
    mfxU8 *               dbegin,
    mfxU8 *               dend,
    mfxU8 *               sbegin,
    mfxU8 *               send,
    MfxVideoParam const & par,
    DdiTask const &       task,
    mfxU32                fieldId)
{
    mfxExtSpsHeader * extSps = GetExtBuffer(par);
    mfxExtPpsHeader * extPps = GetExtBuffer(par);

    mfxU32 num_ref_idx_l0_active_minus1     = 0;
    mfxU32 num_ref_idx_l1_active_minus1     = 0;

    mfxU32 sliceType    = 0;
    mfxU32 fieldPicFlag = 0;

    mfxU32 tmp = 0;

    if (extPps->entropyCodingModeFlag == 0)
    {
        // remove start code emulation prevention bytes when doing full repack for CAVLC
        mfxU32 zeroCount = 0;
        mfxU8 * write = sbegin;
        for (mfxU8 * read = write; read != send; ++read)
        {
            if (*read == 0x03 && zeroCount >= 2 && read + 1 != send && (*(read + 1) & 0xfc) == 0)
            {
                // skip start code emulation prevention byte
                zeroCount = 0; // drop zero count
            }
            else
            {
                *(write++) = *read;
                zeroCount = (*read == 0) ? zeroCount + 1 : 0;
            }
        }
    }

    InputBitstream  reader(sbegin, send, true, extPps->entropyCodingModeFlag == 1);
    OutputBitstream writer(dbegin, dend);

    writer.PutUe(reader.GetUe());                               // first_mb_in_slice
    writer.PutUe(sliceType = reader.GetUe());                   // slice_type
    writer.PutUe(reader.GetUe());                               // pic_parameter_set_id

    mfxU32 log2MaxFrameNum = extSps->log2MaxFrameNumMinus4 + 4;
    writer.PutBits(reader.GetBits(log2MaxFrameNum), log2MaxFrameNum);

    if (!extSps->frameMbsOnlyFlag)
    {
        writer.PutBit(fieldPicFlag = reader.GetBit());          // field_pic_flag
        if (fieldPicFlag)
            writer.PutBit(reader.GetBit());                     // bottom_field_flag
    }

    if (task.m_type[fieldId] & MFX_FRAMETYPE_IDR)
        writer.PutUe(reader.GetUe());                           // idr_pic_id

    if (extSps->picOrderCntType == 0)
    {
        mfxU32 log2MaxPicOrderCntLsb = extSps->log2MaxPicOrderCntLsbMinus4 + 4;
        writer.PutBits(reader.GetBits(log2MaxPicOrderCntLsb), log2MaxPicOrderCntLsb);
    }

    if (sliceType % 5 == 1)
        writer.PutBit(reader.GetBit());                         // direct_spatial_mv_pred_flag

    if (sliceType % 5 == 0 || sliceType % 5 == 1)
    {
        writer.PutBit(tmp = reader.GetBit());                   // num_ref_idx_active_override_flag
        if (tmp)
        {
            num_ref_idx_l0_active_minus1 = reader.GetUe();
            writer.PutUe(num_ref_idx_l0_active_minus1);                       // num_ref_idx_l0_active_minus1
            if (sliceType % 5 == 1)
            {
                num_ref_idx_l1_active_minus1 = reader.GetUe();
                writer.PutUe(num_ref_idx_l1_active_minus1);                   // num_ref_idx_l1_active_minus1
            }
        }
        else
        {
            num_ref_idx_l0_active_minus1 = extPps->numRefIdxL0DefaultActiveMinus1;
            num_ref_idx_l1_active_minus1 = extPps->numRefIdxL1DefaultActiveMinus1;
        }
    }

    if (sliceType % 5 != 2 && sliceType % 5 != 4)
    {
        ReadRefPicListModification(reader);
        // align size of ref pic list modification with num_ref_idx_l0_active_minus1 which is written to bitstream
        if (task.m_refPicList0Mod[fieldId].Size() > num_ref_idx_l0_active_minus1 + 1)
        {
            ArrayRefListMod refPicListMod = task.m_refPicList0Mod[fieldId];
            refPicListMod.Resize(num_ref_idx_l0_active_minus1 + 1);
            WriteRefPicListModification(writer, refPicListMod);
        }
        else
            WriteRefPicListModification(writer, task.m_refPicList0Mod[fieldId]);
    }

    if (sliceType % 5 == 1)
    {
        ReadRefPicListModification(reader);
        if (task.m_refPicList1Mod[fieldId].Size() > num_ref_idx_l1_active_minus1 + 1)
        {
            ArrayRefListMod refPicListMod = task.m_refPicList1Mod[fieldId];
            refPicListMod.Resize(num_ref_idx_l1_active_minus1 + 1);
            WriteRefPicListModification(writer, refPicListMod);
        }
        else
            WriteRefPicListModification(writer, task.m_refPicList1Mod[fieldId]);
    }

    if (task.m_type[fieldId] & MFX_FRAMETYPE_REF)
    {
        bool idrPicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) != 0;
        ReadDecRefPicMarking(reader, idrPicFlag);
        WriteDecRefPicMarking(writer, task.m_decRefPicMrk[fieldId], idrPicFlag);
    }

    if (extPps->entropyCodingModeFlag && (sliceType % 5 != 2))
        writer.PutUe(reader.GetUe());                           // cabac_init_idc

    writer.PutSe(reader.GetSe());                               // slice_qp_delta

    if (1/*deblocking_filter_control_present_flag*/)
    {
        writer.PutUe(tmp = reader.GetUe());                     // disable_deblocking_filter_idc
        if (tmp != 1)
        {
            writer.PutSe(reader.GetSe());                       // slice_alpha_c0_offset_div2
            writer.PutSe(reader.GetSe());                       // slice_beta_offset_div2
        }
    }

    if (extPps->entropyCodingModeFlag)
    {
        while (reader.NumBitsRead() % 8)
            reader.GetBit();                                    // cabac_alignment_one_bit

        mfxU32 numAlignmentBits = (8 - (writer.GetNumBits() & 0x7)) & 0x7;
        writer.PutBits(0xff, numAlignmentBits);

        sbegin += reader.NumBitsRead() / 8;
        dbegin += writer.GetNumBits() / 8;

        memcpy(dbegin, sbegin, send - sbegin);
        dbegin += send - sbegin;
    }
    else
    {
        mfxU32 bitsLeft = reader.NumBitsLeft();

        for (; bitsLeft > 31; bitsLeft -= 32)
            writer.PutBits(reader.GetBits(32), 32);

        writer.PutBits(reader.GetBits(bitsLeft), bitsLeft);
        writer.PutBits(0, (8 - (writer.GetNumBits() & 7)) & 7); // trailing zeroes

        assert((reader.NumBitsRead() & 7) == 0);
        assert((writer.GetNumBits()  & 7) == 0);

        sbegin += (reader.NumBitsRead() + 7) / 8;
        dbegin += (writer.GetNumBits()  + 7) / 8;
    }

    return dbegin;
}

namespace
{
    mfxExtPictureTimingSEI const * SelectPicTimingSei(
        MfxVideoParam const & video,
        DdiTask const &       task)
    {
        if (mfxExtPictureTimingSEI const * extPt = GetExtBuffer(task.m_ctrl))
        {
            return extPt;
        }
        else
        {
            mfxExtPictureTimingSEI const * extPtGlobal = GetExtBuffer(video);
            return extPtGlobal;
        }
    }
};

void MfxHwH264Encode::PrepareSeiMessageBuffer(
    MfxVideoParam const & video,
    DdiTask const &       task,
    mfxU32                fieldId,
    PreAllocatedVector &  sei)
{
    mfxExtCodingOption const *     extOpt = GetExtBuffer(video);
    mfxExtSpsHeader const *        extSps = GetExtBuffer(video);
    mfxExtPictureTimingSEI const * extPt  = SelectPicTimingSei(video, task);

    mfxU32 fillerSize         = task.m_fillerSize[fieldId];
    mfxU32 fieldPicFlag       = (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE);
    mfxU32 secondFieldPicFlag = (task.GetFirstField() != fieldId);
    mfxU32 idrPicFlag         = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 recoveryPoint      = IsRecoveryPointSeiMessagePresent(
        task.m_ctrl.Payload,
        task.m_ctrl.NumPayload,
        GetPayloadLayout(fieldPicFlag, secondFieldPicFlag));

    mfxU32 needCpbRemovalDelay = idrPicFlag || recoveryPoint;

    mfxU32 needMarkingRepetitionSei =
        IsOn(extOpt->RefPicMarkRep) && task.m_decRefPicMrkRep[fieldId].presentFlag;

    mfxU32 needRecoveryPointSei = (task.m_IRState.refrType != NO_REFRESH && task.m_IRState.IntraLocation == 0 && extOpt->RecoveryPointSEI == MFX_CODINGOPTION_ON);

    mfxU32 needBufferingPeriod =
        IsOn(extOpt->VuiNalHrdParameters) && needCpbRemovalDelay ||
        IsOn(extOpt->VuiVclHrdParameters) && needCpbRemovalDelay ||
        IsOn(extOpt->PicTimingSEI)        && idrPicFlag          || // to activate sps
        needRecoveryPointSei;                                       // AU with Recovery point SEI should contain BP SEI

    mfxU32 needPicTimingSei =
        IsOn(extOpt->VuiNalHrdParameters) ||
        IsOn(extOpt->VuiVclHrdParameters) ||
        IsOn(extOpt->PicTimingSEI);

    mfxU32 needAtLeastOneSei =
        task.m_ctrl.Payload && task.m_ctrl.NumPayload > 0 ||
        fillerSize > 0      ||
        needBufferingPeriod ||
        needPicTimingSei    ||
        needMarkingRepetitionSei;

    OutputBitstream writer(sei.Buffer(), sei.Capacity());

    mfxU8 const SEI_STARTCODE[5] = { 0, 0, 0, 1, 6 };
    if (needAtLeastOneSei && IsOn(extOpt->SingleSeiNalUnit))
        writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));

    mfxExtAvcSeiBufferingPeriod msgBufferingPeriod = { 0 };
    mfxExtAvcSeiPicTiming msgPicTiming;

    mfxU32 sps_id = extSps->seqParameterSetId;
    sps_id = ((sps_id + !!task.m_viewIdx) & 0x1f);  // use appropriate sps id for dependent views

    if (needBufferingPeriod)
    {
        PrepareSeiMessage(
            task,
            IsOn(extOpt->VuiNalHrdParameters),
            IsOn(extOpt->VuiVclHrdParameters),
            sps_id,
            msgBufferingPeriod);

            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            PutSeiMessage(writer, msgBufferingPeriod);
            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutTrailingBits();
    }

    if (needPicTimingSei)
    {
        PrepareSeiMessage(
            task,
            fieldId,
            IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters),
            msgPicTiming);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writer, *extPt, msgPicTiming);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    // user-defined messages
    for (mfxU32 i = secondFieldPicFlag; i < task.m_ctrl.NumPayload; i = i + 1 + fieldPicFlag)
    {
        if (task.m_ctrl.Payload[i] != 0)
        {
            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            for (mfxU32 b = 0; b < task.m_ctrl.Payload[i]->NumBit / 8; b++)
                writer.PutBits(task.m_ctrl.Payload[i]->Data[b], 8);
            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutTrailingBits();
        }
    }

    if (needMarkingRepetitionSei) 
    {
        mfxU8 frameMbsOnlyFlag = (video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;

        mfxExtAvcSeiDecRefPicMrkRep decRefPicMrkRep;
        PrepareSeiMessage(task, fieldId, frameMbsOnlyFlag, decRefPicMrkRep);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writer, decRefPicMrkRep);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    if (needRecoveryPointSei)
    {
        mfxExtAvcSeiRecPoint msgPicTiming;
        PrepareSeiMessage(video, task, msgPicTiming);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writer, msgPicTiming);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    if (fillerSize > 0)
    {
        // how many bytes takes to encode payloadSize depends on size of sei message
        // need to compensate it
        fillerSize -= fillerSize / 256;
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiHeader(writer, SEI_TYPE_FILLER_PAYLOAD, fillerSize);
        writer.PutFillerBytes(0xff, fillerSize);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    if (needAtLeastOneSei && IsOn(extOpt->SingleSeiNalUnit))
        writer.PutTrailingBits();

    // add repack compensation to the end of last sei NALu.
    // It's padding done with trailing_zero_8bits. This padding could has greater size then real repack overhead.
    if (task.m_addRepackSize[fieldId] && needAtLeastOneSei)
        writer.PutFillerBytes(0xff, task.m_addRepackSize[fieldId]);

    sei.SetSize(writer.GetNumBits() / 8);
}

// MVC BD {
void MfxHwH264Encode::PrepareSeiMessageBufferDepView(
    MfxVideoParam const & video,
#ifdef AVC_BS
    DdiTask &       task,
#else // AVC_BS
    DdiTask const &       task,
#endif // AVC_BS
    mfxU32                fieldId,
    PreAllocatedVector &  sei)
{
    mfxExtCodingOption const *     extOpt = GetExtBuffer(video);
    mfxExtSpsHeader const *        extSps = GetExtBuffer(video);
    mfxExtPictureTimingSEI const * extPt  = SelectPicTimingSei(video, task);

    mfxU32 fillerSize         = task.m_fillerSize[fieldId];
    mfxU32 fieldPicFlag       = (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE);
    mfxU32 secondFieldPicFlag = (task.GetFirstField() != fieldId);
    mfxU32 idrPicFlag         = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 recoveryPoint      = IsRecoveryPointSeiMessagePresent(
        task.m_ctrl.Payload,
        task.m_ctrl.NumPayload,
        GetPayloadLayout(fieldPicFlag, secondFieldPicFlag));

    mfxU32 needCpbRemovalDelay = idrPicFlag || recoveryPoint;

    mfxU32 needMarkingRepetitionSei =
        IsOn(extOpt->RefPicMarkRep) && task.m_decRefPicMrkRep[fieldId].presentFlag;

    mfxU32 needBufferingPeriod =
        IsOn(extOpt->VuiNalHrdParameters) && needCpbRemovalDelay ||
        IsOn(extOpt->VuiVclHrdParameters) && needCpbRemovalDelay ||
        IsOn(extOpt->PicTimingSEI)        && idrPicFlag; // to activate sps

    mfxU32 needPicTimingSei =
        IsOn(extOpt->VuiNalHrdParameters) ||
        IsOn(extOpt->VuiVclHrdParameters) ||
        IsOn(extOpt->PicTimingSEI);

    mfxU32 needMvcNestingSei = needBufferingPeriod || needPicTimingSei;
    // for BD/AVCHD compatible encoding filler SEI should have MVC nesting wrapper
    if (IsOn(extOpt->ViewOutput))
        needMvcNestingSei |= (fillerSize != 0);

    mfxU32 needNotNestingSei =
        task.m_ctrl.Payload && task.m_ctrl.NumPayload > 0 ||
        fillerSize > 0 && IsOff(extOpt->ViewOutput) ||
        needMarkingRepetitionSei;

    OutputBitstream writer(sei.Buffer(), sei.Capacity());

    mfxU8 const SEI_STARTCODE[5] = { 0, 0, 0, 1, 6 };

// MVC BD {
    mfxExtAvcSeiBufferingPeriod msgBufferingPeriod = { 0 };
    mfxExtAvcSeiPicTiming msgPicTiming;
    mfxU32 sps_id = extSps->seqParameterSetId;
    sps_id = ((sps_id + !!task.m_viewIdx) & 0x1f);  // use appropriate sps id for dependent views
// MVC BD }

    if (needMvcNestingSei)
    {
        if (needBufferingPeriod)
        {
            PrepareSeiMessage(
                task,
                IsOn(extOpt->VuiNalHrdParameters),
                IsOn(extOpt->VuiVclHrdParameters),
                sps_id,
                msgBufferingPeriod);

            // write NAL unit with MVC nesting SEI for BP
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            PutSeiMessage(writer, needBufferingPeriod, 0, 0, video, msgBufferingPeriod, *extPt, msgPicTiming);
            writer.PutTrailingBits();
        }

        if (needPicTimingSei)
        {
            PrepareSeiMessage(
                task,
                fieldId,
                IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters),
                msgPicTiming);

            // write NAL unit with MVC nesting SEI for PT
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            PutSeiMessage(writer, 0, needPicTimingSei, 0, video, msgBufferingPeriod, *extPt, msgPicTiming);
            writer.PutTrailingBits();
        }

        if (fillerSize)
        {
            // write NAL unit with MVC nesting SEI for filler
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            PutSeiMessage(writer, 0, 0, fillerSize, video, msgBufferingPeriod, *extPt, msgPicTiming);
            writer.PutTrailingBits();
        }
    }

    if (needNotNestingSei && IsOn(extOpt->SingleSeiNalUnit))
        writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));

    // user-defined messages
    for (mfxU32 i = secondFieldPicFlag; i < task.m_ctrl.NumPayload; i = i + 1 + fieldPicFlag)
    {
        if (task.m_ctrl.Payload[i] != 0)
        {
            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            for (mfxU32 b = 0; b < task.m_ctrl.Payload[i]->NumBit / 8; b++)
                writer.PutBits(task.m_ctrl.Payload[i]->Data[b], 8);
            if (IsOff(extOpt->SingleSeiNalUnit))
                writer.PutTrailingBits();
        }
    }

    if (needMarkingRepetitionSei) 
    {
        mfxU8 frameMbsOnlyFlag = (video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;

        mfxExtAvcSeiDecRefPicMrkRep decRefPicMrkRep;
        PrepareSeiMessage(task, fieldId, frameMbsOnlyFlag, decRefPicMrkRep);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writer, decRefPicMrkRep);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    if (fillerSize > 0 && IsOff(extOpt->ViewOutput))
    {
        // how many bytes takes to encode payloadSize depends on size of sei message
        // need to compensate it
        fillerSize -= fillerSize / 256;
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiHeader(writer, SEI_TYPE_FILLER_PAYLOAD, fillerSize);
        writer.PutFillerBytes(0xff, fillerSize);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writer.PutTrailingBits();
    }

    if (needNotNestingSei && IsOn(extOpt->SingleSeiNalUnit))
        writer.PutTrailingBits();

    // w/a for SNB/IVB: padd sei buffer to compensate re-pack  of AVC headers to MVC
    // add repack compensation to the end of last sei NALu.
    if (needMvcNestingSei && task.m_addRepackSize[fieldId])
        writer.PutFillerBytes(0xff, task.m_addRepackSize[fieldId]);

    sei.SetSize(writer.GetNumBits() / 8);

#ifdef AVC_BS
    // encoding of AVC SEI with size equal to MVC SEI
    mfxU32 seiSizeMVC = writer.GetNumBits() / 8;
    mfxU32 seiSizeAVC = 0;
    mfxU32 needAtLeastOneSei =
        task.m_ctrl.Payload && task.m_ctrl.NumPayload > 0 ||
        fillerSize > 0      ||
        needBufferingPeriod ||
        needPicTimingSei    ||
        needMarkingRepetitionSei;

    OutputBitstream writerAVC(&sei[0], sei.Capacity());

    if (needAtLeastOneSei && IsOn(extOpt->SingleSeiNalUnit))
        writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));

    if (needBufferingPeriod)
    {
        PrepareSeiMessage(
            task,
            IsOn(extOpt->VuiNalHrdParameters),
            IsOn(extOpt->VuiVclHrdParameters),
            sps_id,
            msgBufferingPeriod);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writerAVC, msgBufferingPeriod);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutTrailingBits();
    }

    if (needPicTimingSei)
    {
        PrepareSeiMessage(
            task,
            fieldId,
            IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters),
            msgPicTiming);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writerAVC, *extPt, msgPicTiming);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutTrailingBits();
    }

    // user-defined messages
    for (mfxU32 i = secondFieldPicFlag; i < task.m_ctrl.NumPayload; i = i + 1 + fieldPicFlag)
    {
        if (task.m_ctrl.Payload[i] != 0)
        {
            if (IsOff(extOpt->SingleSeiNalUnit))
                writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
            for (mfxU32 b = 0; b < task.m_ctrl.Payload[i]->NumBit / 8; b++)
                writerAVC.PutBits(task.m_ctrl.Payload[i]->Data[b], 8);
            if (IsOff(extOpt->SingleSeiNalUnit))
                writerAVC.PutTrailingBits();
        }
    }

    if (needMarkingRepetitionSei) 
    {
        mfxU8 frameMbsOnlyFlag = (video.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;

        mfxExtAvcSeiDecRefPicMrkRep decRefPicMrkRep;
        PrepareSeiMessage(task, fieldId, frameMbsOnlyFlag, decRefPicMrkRep);

        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiMessage(writerAVC, decRefPicMrkRep);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutTrailingBits();
    }

    if (fillerSize > 0)
    {
        // how many bytes takes to encode payloadSize depends on size of sei message
        // need to compensate it
        fillerSize -= fillerSize / 256;
        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutRawBytes(SEI_STARTCODE, SEI_STARTCODE + sizeof(SEI_STARTCODE));
        PutSeiHeader(writerAVC, SEI_TYPE_FILLER_PAYLOAD, fillerSize);
        writerAVC.PutFillerBytes(0xff, fillerSize);
        if (IsOff(extOpt->SingleSeiNalUnit))
            writerAVC.PutTrailingBits();
    }

    if (needAtLeastOneSei && IsOn(extOpt->SingleSeiNalUnit))
        writerAVC.PutTrailingBits();

    // add repack compensation to the end of last sei NALu.
    if (task.m_addRepackSize[fieldId] && needAtLeastOneSei)
        writerAVC.PutFillerBytes(0xff, task.m_addRepackSize[fieldId]);

    seiSizeAVC = writerAVC.GetNumBits() / 8;
    mfxI32 seiSizeDiff = seiSizeMVC - seiSizeAVC;
    assert(seiSizeDiff >= 0);
    if (seiSizeDiff > 0)
    {
        writerAVC.PutFillerBytes(0xff, seiSizeDiff);
        task.m_addRepackSize[fieldId] += seiSizeDiff;
    }

    assert(writer.GetNumBits() == writerAVC.GetNumBits());
    sei.SetSize(writerAVC.GetNumBits() / 8);
#endif // AVC_BS
}
// MVC BD }
bool MfxHwH264Encode::IsInplacePatchNeeded(
    MfxVideoParam const & par,
    DdiTask const &       task,
    mfxU32                fieldId)
{
    mfxExtSpsHeader const * extSps = GetExtBuffer(par);
    mfxExtPpsHeader const * extPps = GetExtBuffer(par);

    mfxU8 constraintFlags =
        (extSps->constraints.set0 << 7) | (extSps->constraints.set1 << 6) |
        (extSps->constraints.set2 << 5) | (extSps->constraints.set3 << 4) |
        (extSps->constraints.set4 << 3) | (extSps->constraints.set5 << 2) |
        (extSps->constraints.set6 << 1) | (extSps->constraints.set7 << 0);

    if (task.m_nalRefIdc[fieldId] > 1)
        return true;

    return
        constraintFlags   != 0 ||
        extSps->nalRefIdc != 1 ||
        extPps->nalRefIdc != 1 ||
        extSps->gapsInFrameNumValueAllowedFlag == 1 ||
        (extSps->maxNumRefFrames & 1);
}

bool MfxHwH264Encode::IsSlicePatchNeeded(
    DdiTask const & task,
    mfxU32          fieldId)
{
    for (mfxU32 i = 0; i < task.m_refPicList0Mod[fieldId].Size(); i++)
        if (task.m_refPicList0Mod[fieldId][i].m_idc == RPLM_LT_PICNUM)
            return true;    // driver doesn't write reordering syntax for long term reference pictures

    for (mfxU32 i = 0; i < task.m_list0[fieldId].Size(); i++)
        if (task.m_dpb[fieldId][task.m_list0[fieldId][i] & 0x7f].m_longterm)
            return true; // driver insert incorrect reordering syntax when longterm ref present

    for (mfxU32 i = 0; i < task.m_list1[fieldId].Size(); i++)
        if (task.m_dpb[fieldId][task.m_list1[fieldId][i] & 0x7f].m_longterm)
            return true; // driver insert incorrect reordering syntax when longterm ref present

    bool list0ModifiedAndShortened =
        task.m_refPicList0Mod[fieldId].Size() > 0 &&
        task.m_initSizeList0[fieldId] != task.m_list0[fieldId].Size();

    return
        task.m_refPicList1Mod[fieldId].Size() > 0               || // driver doesn't write reordering syntax for List1
        list0ModifiedAndShortened                               || // driver doesn't write correct reordering syntax
                                                                   // when num_ref_idx_l0_active is different from inital
        task.m_decRefPicMrk[fieldId].mmco.Size() > 0            || // driver doesn't write dec_ref_pic_marking syntax
        task.m_decRefPicMrk[fieldId].long_term_reference_flag;     // even for idr frames
}

namespace
{
    void ReadUntilGapsInFrameNumValueAllowedFlag(
        InputBitstream & reader)
    {
        mfxU32 profileIdc = reader.GetBits(8);
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBit();
        reader.GetBits(8);
        reader.GetUe();
        if (profileIdc == 100 || profileIdc == 110 || profileIdc == 122 ||
            profileIdc == 244 || profileIdc ==  44 || profileIdc ==  83 ||
            profileIdc ==  86 || profileIdc == 118 || profileIdc == 128)
        {
            if (reader.GetUe() == 3)
                reader.GetBit();
            reader.GetUe();
            reader.GetUe();
            reader.GetBit();
            if (reader.GetBit())
                assert(0);
        }
        reader.GetUe();
        mfxU32 picOrderCntType = reader.GetUe();
        if (picOrderCntType == 0)
        {
            reader.GetUe();
        }
        else if (picOrderCntType == 1)
        {
            assert(0);
        }

        reader.GetUe();
    }
}

mfxU8 * MfxHwH264Encode::PatchBitstream(
    MfxVideoParam const & video,
    DdiTask const &       task,
    mfxU32                fieldId,
    mfxU8 *               sbegin, // contents of source buffer may be modified
    mfxU8 *               send,
    mfxU8 *               dbegin,
    mfxU8 *               dend)
{
    mfxExtSpsHeader const * extSps = GetExtBuffer(video);
    mfxExtPpsHeader const * extPps = GetExtBuffer(video);

    mfxU8 constraintFlags =
        (extSps->constraints.set0 << 7) | (extSps->constraints.set1 << 6) |
        (extSps->constraints.set2 << 5) | (extSps->constraints.set3 << 4) |
        (extSps->constraints.set4 << 3) | (extSps->constraints.set5 << 2) |
        (extSps->constraints.set6 << 1) | (extSps->constraints.set7 << 0);

    bool copy = (sbegin != dbegin);

    bool prefixNalUnitNeeded = video.calcParam.numTemporalLayer > 0;

    bool slicePatchNeeded = copy && IsSlicePatchNeeded(task, fieldId);
    assert(copy || !IsSlicePatchNeeded(task, fieldId) || !"slice patching requries intermediate bitstream buffer");

    bool spsPresent = false;

    for (NaluIterator nalu(sbegin, send); nalu != NaluIterator(); ++nalu)
    {
        if (nalu->type == 7)
        {
            mfxU8 * spsBegin = dbegin;
            if (extSps->gapsInFrameNumValueAllowedFlag || (extSps->maxNumRefFrames & 1))
            {
                assert(copy);
                InputBitstream reader(nalu->begin + nalu->numZero + 1, nalu->end);
                mfxExtSpsHeader spsHead = { 0 };
                ReadSpsHeader(reader, spsHead);

                spsHead.gapsInFrameNumValueAllowedFlag = extSps->gapsInFrameNumValueAllowedFlag;
                spsHead.maxNumRefFrames                = extSps->maxNumRefFrames;

                OutputBitstream writer(dbegin, dend);
                dbegin += WriteSpsHeader(writer, spsHead) / 8;
            }
            else
            {
                dbegin = copy
                    ? CheckedMemcpy(dbegin, dend, nalu->begin, nalu->end)
                    : nalu->end;
            }

            // snb and ivb driver doesn't provide controls for nal_ref_idc
            // if nal_ref_idc from mfxExtCodingOptionSPSPPS differs from value hardcoded in driver (1)
            // it needs to be patched
            spsBegin[nalu->numZero + 1] &= ~0x30;
            spsBegin[nalu->numZero + 1] |= extSps->nalRefIdc << 5;

            // snb and ivb driver doesn't provide controls for constraint flags in sps header
            // if any of them were set via mfxExtCodingOptionSPSPPS
            // sps header generated by driver needs to be patched
            // such patching doesn't change length of header
            spsBegin[nalu->numZero + 3] = constraintFlags;
            spsPresent = true;
        }
        else if (nalu->type == 8)
        {
            if (spsPresent ||               // pps always accompanies sps
                !video.calcParam.lyncMode)   // mfxExtAvcTemporalLayers buffer is not present, pps to every frame
            {
                mfxU8 * ppsBegin = dbegin;
                dbegin = copy
                    ? CheckedMemcpy(dbegin, dend, nalu->begin, nalu->end)
                    : nalu->end;

                // snb and ivb driver doesn't provide controls for nal_ref_idc
                // if nal_ref_idc from mfxExtCodingOptionSPSPPS differs from value hardcoded in driver (1)
                // it needs to be patched
                ppsBegin[nalu->numZero + 1] &= ~0x30;
                ppsBegin[nalu->numZero + 1] |= extPps->nalRefIdc << 5;
            }
        }
        else if (nalu->type == 9)
        {
            if (!video.calcParam.lyncMode) // mfxExtAvcTemporalLayers buffer is not present, aud to every frame
            {
                dbegin = copy
                    ? CheckedMemcpy(dbegin, dend, nalu->begin, nalu->end)
                    : nalu->end;
            }
        }
        else if (nalu->type == 1 || nalu->type == 5)
        {
            if (task.m_nalRefIdc[fieldId] > 1)
            {
                nalu->begin[nalu->numZero + 1] &= ~0x30;
                nalu->begin[nalu->numZero + 1] |= task.m_nalRefIdc[fieldId] << 5;
            }

            if (prefixNalUnitNeeded)
            {
                dbegin = PackPrefixNalUnitSvc(dbegin, dend, true, task, fieldId);
            }

            if (slicePatchNeeded)
            {
                assert(copy || !"slice patching requries intermediate bitstream buffer");
                dbegin = CheckedMemcpy(dbegin, dend, nalu->begin, nalu->begin + nalu->numZero + 2);
                dbegin = RePackSlice(dbegin, dend, nalu->begin + nalu->numZero + 2, nalu->end, video, task, fieldId);
            }
            else
            {
                dbegin = copy
                    ? CheckedMemcpy(dbegin, dend, nalu->begin, nalu->end)
                    : nalu->end;
            }
        }
        else
        {
            dbegin = copy
                ? CheckedMemcpy(dbegin, dend, nalu->begin, nalu->end)
                : nalu->end;
        }
    }

    return dbegin;
}

namespace
{
    mfxI32 CalcDistScaleFactor(
        mfxU32 pocCur,
        mfxU32 pocL0,
        mfxU32 pocL1)
    {
        mfxI32 tb = IPP_MIN(IPP_MAX(-128, mfxI32(pocCur - pocL0)), 127);
        mfxI32 td = IPP_MIN(IPP_MAX(-128, mfxI32(pocL1  - pocL0)), 127);
        mfxI32 tx = (16384 + abs(td/2)) / td;
        mfxI32 distScaleFactor = IPP_MIN(IPP_MAX(-1024, (tb * tx + 32) >> 6), 1023);
        return distScaleFactor;
    }

    mfxI32 CalcDistScaleFactor(
        DdiTask const & task,
        mfxU32 indexL0,
        mfxU32 indexL1)
    {
        if (indexL0 >= task.m_list0[0].Size() ||
            indexL1 >= task.m_list1[0].Size())
            return 128;
        mfxU32 pocL0 = task.m_dpb[0][task.m_list0[0][indexL0] & 127].m_poc[0];
        mfxU32 pocL1 = task.m_dpb[0][task.m_list1[0][indexL1] & 127].m_poc[0];
        if (pocL0 == pocL1)
            return 128;
        return CalcDistScaleFactor(task.GetPoc(0), pocL0, pocL1);
    }
};

mfxU32 MfxHwH264Encode::CalcBiWeight(
    DdiTask const & task,
    mfxU32 indexL0,
    mfxU32 indexL1)
{
    mfxI32 biWeight = CalcDistScaleFactor(task, indexL0, indexL1) >> 2;
    return biWeight < -64 || biWeight > 128
        ? 32
        : biWeight;
}




#endif // MFX_ENABLE_H264_VIDEO_ENCODE_HW
