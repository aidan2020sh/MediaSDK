/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <vector>
#include <algorithm>

// #define DUMP_BS

namespace hevce_default_ref_lists
{
    using namespace BS_HEVC2;

    struct Frame
    {
        mfxU32 type;
        mfxU8 nalType;
        mfxI32 poc;
        mfxU32 bpo;
        mfxU16 PLayer;
        bool   bSecondField;
        bool   bBottomField;
        mfxI32 lastRAP;
        mfxI32 IPoc;
        mfxI32 prevIPoc;
        mfxI32 nextIPoc;
    };

    struct ExternalFrame
    {
        mfxU32 type;
        mfxU8  nalType;
        mfxI32 poc;
        mfxU16 PLayer;
        bool   bSecondField;
        bool   bBottomField;
        std::vector<Frame> ListX[2];
    };

    enum TestType
    {
        LOWPOWER     = 0x01,
        NOT_LOWPOWER = 0x02,

        HUGE_SIZE_4K = 0x04,
        HUGE_SIZE_8K = 0x08
    };

    char FrameTypeToChar(mfxU32 type)
    {
        char c = (type & MFX_FRAMETYPE_I) ? 'i' : (type & MFX_FRAMETYPE_P) ? 'p' : 'b';
        return (type & MFX_FRAMETYPE_REF) ? char(toupper(c)) : c;
    }

    bool isBFF(mfxVideoParam const & video)
    {
        return ((video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BOTTOM) == MFX_PICSTRUCT_FIELD_BOTTOM);
    }

    mfxI32 GetFrameNum(bool bField, mfxI32 Poc, bool bSecondField)
    {
        return bField ? (Poc + (!bSecondField)) / 2 : Poc;
    }

    mfxU8 GetFrameType(
        mfxVideoParam const & video,
        mfxU32                pictureOrder,
        bool                  isPictureOfLastFrame)
    {
        mfxU32 gopOptFlag = video.mfx.GopOptFlag;
        mfxU32 gopPicSize = video.mfx.GopPicSize;
        mfxU32 gopRefDist = video.mfx.GopRefDist;
        mfxU32 idrPicDist = gopPicSize * (video.mfx.IdrInterval);

        if (gopPicSize == 0xffff)
            idrPicDist = gopPicSize = 0xffffffff;

        bool bFields = !!(video.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);

        mfxU32 frameOrder = bFields ? pictureOrder / 2 : pictureOrder;

        bool   bSecondField = bFields && (pictureOrder & 1);
        bool   bIdr = (idrPicDist ? frameOrder % idrPicDist : frameOrder) == 0;

        if (bIdr)
            return bSecondField ? (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF) : (MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_IDR);

        if (frameOrder % gopPicSize == 0)
            return (mfxU8)(bSecondField ? MFX_FRAMETYPE_P : MFX_FRAMETYPE_I) | MFX_FRAMETYPE_REF;

        if (frameOrder % gopPicSize % gopRefDist == 0)
            return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);

        if ((gopOptFlag & MFX_GOP_STRICT) == 0)
        {
            if (((frameOrder + 1) % gopPicSize == 0 && (gopOptFlag & MFX_GOP_CLOSED)) ||
                (idrPicDist && (frameOrder + 1) % idrPicDist == 0) ||
                isPictureOfLastFrame)
            {
                return (MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF);
            }
        }

        return MFX_FRAMETYPE_B;
    }

    class EncodeEmulator
    {
    protected:

        class LastReorderedFieldInfo
        {
        public:
            mfxI32     m_poc;
            bool       m_bReference;
            bool       m_bFirstField;

            LastReorderedFieldInfo() :
                m_poc(-1),
                m_bReference(false),
                m_bFirstField(false){}

            void Reset()
            {
                m_poc = -1;
                m_bReference = false;
                m_bFirstField = false;
            }
            void SaveInfo(Frame const & frame)
            {
                m_poc = frame.poc;
                m_bReference = ((frame.type & MFX_FRAMETYPE_REF) != 0);
                m_bFirstField = !frame.bSecondField;
            }
            void CorrectFrameInfo(Frame & frame)
            {
                if ( !isCorrespondSecondField(frame) )
                    return;
                // copy params to the 2nd field
                if (m_bReference)
                    frame.type |= MFX_FRAMETYPE_REF;
            }
            bool isCorrespondSecondField(Frame const & frame)
            {
                if (m_poc + 1 != frame.poc || !frame.bSecondField || !m_bFirstField)
                    return false;
                return true;
            }
            bool bFirstField() { return m_bFirstField; }
        };

        typedef std::vector<Frame>           FrameArray;
        typedef std::vector<Frame>::iterator FrameIterator;

        FrameArray                  m_queue;
        LastReorderedFieldInfo      m_lastFieldInfo;

        FrameArray                  m_dpb;

        tsExtBufType<mfxVideoParam> m_emuPar;
        mfxU32                      m_nMaxFrames;

        mfxI32                      m_lastIdr;   // for POC calculation
        mfxI32                      m_anchorPOC; // for P-Pyramid
        Frame                       m_lastFrame;

    private:

        mfxU8 GetNALUType(Frame const & frame, bool isRAPIntra)
        {
            const bool isI   = !!(frame.type & MFX_FRAMETYPE_I);
            const bool isRef = !!(frame.type & MFX_FRAMETYPE_REF);
            const bool isIDR = !!(frame.type & MFX_FRAMETYPE_IDR);

            if (isIDR)
                return IDR_W_RADL;

            if (isI && isRAPIntra)
            {
                return CRA_NUT;
            }

            if (frame.poc > frame.lastRAP)
            {
                return isRef ? TRAIL_R : TRAIL_N;
            }

            if (isRef)
                return RASL_R;
            return RASL_N;
        }

        bool HasL1(mfxI32 poc)
        {
            for (auto it = m_dpb.begin(); it < m_dpb.end(); it++)
                if (it->poc > poc)
                    return true;
            return false;
        }

        mfxU32 BRefOrder(mfxU32 displayOrder, mfxU32 begin, mfxU32 end, mfxU32 counter, bool & ref)
        {
            ref = (end - begin > 1);

            mfxU32 pivot = (begin + end) / 2;
            if (displayOrder == pivot)
                return counter;
            else if (displayOrder < pivot)
                return BRefOrder(displayOrder, begin, pivot, counter + 1, ref);
            else
                return BRefOrder(displayOrder, pivot + 1, end, counter + 1 + pivot - begin, ref);
        }

        mfxU32 GetBiFrameLocation(mfxU32 displayOrder, mfxU32 num, bool &ref)
        {
            ref = false;
            return BRefOrder(displayOrder, 0, num, 0, ref);
        }

        mfxU32 BPyrReorder(const std::vector<FrameIterator> & bframes)
        {
            const mfxU32 num = (mfxU32)bframes.size();
            if (bframes[0]->bpo == (mfxU32)MFX_FRAMEORDER_UNKNOWN)
            {
                bool bRef = false;

                for(mfxU32 i = 0; i < num; i++)
                {
                    bframes[i]->bpo = GetBiFrameLocation(i,num, bRef);
                    if (bRef)
                        bframes[i]->type |= MFX_FRAMETYPE_REF;
                }
            }
            mfxU32 minBPO = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
            mfxU32 ind = 0;
            for(mfxU32 i = 0; i < num; i++)
            {
                if (bframes[i]->bpo < minBPO)
                {
                    ind = i;
                    minBPO = bframes[i]->bpo;
                }
            }
            return ind;
        }

        FrameIterator Reorder(bool flush, bool bFields)
        {
            FrameIterator begin = m_queue.begin();
            FrameIterator end = m_queue.begin();

            while (end != m_queue.end())
            {
                if ((end != begin) && (end->type & MFX_FRAMETYPE_IDR))
                {
                    flush = true;
                    break;
                }
                end++;
            }

            if (bFields && m_lastFieldInfo.bFirstField())
            {
                while (begin != end && !m_lastFieldInfo.isCorrespondSecondField(*begin))
                    begin++;

                if (begin != end)
                {
                    m_lastFieldInfo.CorrectFrameInfo(*begin);
                    return begin;
                }
                else
                    begin = m_queue.begin();
            }

            FrameIterator top = Reorder(begin, end, flush, bFields);

            if (top == end)
            {
                return top;
            }

            if (bFields)
            {
                m_lastFieldInfo.SaveInfo(*top);
                // According to current lib implementation first field in pyramid is ref.
                // Now can change 1st B in bpyr to ref after it was saved as not ref.
                // If change before saving both fields will become ref.
                if ((top->type & MFX_FRAMETYPE_B) && !top->bSecondField &&
                    (((mfxExtCodingOption2&)m_emuPar).BRefType == MFX_B_REF_PYRAMID))
                    top->type |= MFX_FRAMETYPE_REF;
            }

            return top;
        }

        FrameIterator Reorder(FrameIterator begin, FrameIterator end, bool flush, bool bFields)
        {
            FrameIterator top = begin;
            FrameIterator b0 = end; // 1st non-ref B with L1 > 0
            std::vector<FrameIterator> bframes;

            bool isBPyramid = !!(((mfxExtCodingOption2&)m_emuPar).BRefType == MFX_B_REF_PYRAMID);

            while ( top != end && (top->type & MFX_FRAMETYPE_B))
            {
                if (HasL1(top->poc) && (!top->bSecondField))
                {
                    if (isBPyramid)
                        bframes.push_back(top);
                    else if (top->type & MFX_FRAMETYPE_REF)
                    {
                        if (b0 == end || (top->poc - b0->poc < bFields + 2))
                            return top;
                    }
                    else if (b0 == end)
                        b0 = top;
                }
                top ++;
            }

            if (!bframes.empty())
            {
                // note, bframes here contain only first fields
                return bframes[BPyrReorder(bframes)];
            }

            if (b0 != end)
                return b0;

            bool strict = !!(m_emuPar.mfx.GopOptFlag & MFX_GOP_STRICT);
            if (flush && top == end && begin != end)
            {
                top --;
                if (strict)
                    top->type |= MFX_FRAMETYPE_REF;
                else
                    top->type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;

                if (top->bSecondField && top != begin)
                {
                    top--;
                    if (strict)
                        top->type |= MFX_FRAMETYPE_REF;
                    else
                        top->type = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF;
                }
            }

            return top;
        }

    public:
        EncodeEmulator() :
            m_lastIdr(0)
            , m_anchorPOC(-1)
            {
                m_lastFrame.IPoc = -1;
                m_lastFrame.prevIPoc = -1;
                m_lastFrame.nextIPoc = -1;
            };

        ~EncodeEmulator() {};

        void Init(tsExtBufType<mfxVideoParam> & par, mfxU32 nMaxFrames)
        {
            m_emuPar = par;
            m_nMaxFrames = nMaxFrames;
        }

        ExternalFrame ProcessFrame(const mfxFrameSurface1 & s, bool flash)
        {
            ExternalFrame out = { 0, 0, -1, 0 };
            FrameIterator itOut = m_queue.end();

            bool bIsFieldCoding       = !!(m_emuPar.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_SINGLE);
            mfxExtCodingOption3 & CO3 = m_emuPar;
            bool isPPyramid           = !!(MFX_P_REF_PYRAMID == CO3.PRefType);

            //=================1. Get type, poc, put frame in the queue=====================
            if (!flash)
            {
                bool isPictureOfLastFrameInGOP = false;

                if ((s.Data.FrameOrder >> bIsFieldCoding) == ((m_nMaxFrames - 1) >> bIsFieldCoding)) // EOS
                    isPictureOfLastFrameInGOP = true;

                Frame frame;

                frame.type = GetFrameType(m_emuPar, (bIsFieldCoding ? (m_lastIdr %2) : 0) + s.Data.FrameOrder - m_lastIdr, isPictureOfLastFrameInGOP);

                if (frame.type & MFX_FRAMETYPE_IDR)
                    m_lastIdr = s.Data.FrameOrder;

                frame.poc          = s.Data.FrameOrder - m_lastIdr;
                frame.bSecondField = bIsFieldCoding && (s.Data.FrameOrder & 1);
                frame.bBottomField = false;
                if (bIsFieldCoding)
                {
                    frame.bBottomField = (s.Info.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF)) == 0 ?
                        (isBFF(m_emuPar) != frame.bSecondField) :
                        (s.Info.PicStruct & MFX_PICSTRUCT_FIELD_BFF) != 0;
                }

                if (frame.type & MFX_FRAMETYPE_I)
                {
                    m_anchorPOC = frame.poc;
                }

                if (isPPyramid)
                {
                    constexpr mfxU16 DEFAULT_PPYR_INTERVAL = 3; // same as in hevce_hw\h265\include\mfx_h265_encode_hw_utils.h
                    mfxI32 PPyrInterval = (m_emuPar.mfx.NumRefFrame > 0) ?
                        std::min(DEFAULT_PPYR_INTERVAL, m_emuPar.mfx.NumRefFrame) :
                        DEFAULT_PPYR_INTERVAL;

                    mfxI32 idx = std::abs(( frame.poc >> bIsFieldCoding) - (m_anchorPOC >> bIsFieldCoding) ) % PPyrInterval;
                    frame.PLayer = (idx != 0); // strong STR to PLayer==0
                }

                frame.nalType  = (mfxU8)-1;
                frame.bpo      = (mfxU32)MFX_FRAMEORDER_UNKNOWN;
                frame.lastRAP  = -1;
                frame.IPoc     = -1;
                frame.prevIPoc = -1;
                frame.nextIPoc = -1;

                m_queue.push_back(frame);
            }

            //=================2. Reorder frames, fill output frame poc, type, etc=====================
            itOut = Reorder(flash, bIsFieldCoding);
            if (itOut == m_queue.end())
                return out;

            bool isIdr   = !!(itOut->type & MFX_FRAMETYPE_IDR);
            bool isRef   = !!(itOut->type & MFX_FRAMETYPE_REF);
            bool isI     = !!(itOut->type & MFX_FRAMETYPE_I);
            bool isB     = !!(itOut->type & MFX_FRAMETYPE_B);

            itOut->lastRAP = m_lastFrame.lastRAP;

            if (isI)
            {
                itOut->IPoc = itOut->poc;
                itOut->prevIPoc = m_lastFrame.IPoc;
                itOut->nextIPoc = -1;
            }
            else
            {
                if (itOut->poc >= m_lastFrame.IPoc)
                {
                    itOut->IPoc = m_lastFrame.IPoc;
                    itOut->prevIPoc = m_lastFrame.prevIPoc;
                    itOut->nextIPoc = m_lastFrame.nextIPoc;
                }
                else
                {
                    itOut->IPoc = m_lastFrame.prevIPoc;
                    itOut->prevIPoc = -1;
                    itOut->nextIPoc = m_lastFrame.IPoc;
                }
            }

            out.poc          = itOut->poc;
            out.type         = itOut->type;
            out.bSecondField = itOut->bSecondField;
            out.bBottomField = itOut->bBottomField;

            //=================3. Update DPB=====================
            if (isIdr)
                m_dpb.clear();

            if (itOut->poc > itOut->lastRAP &&
                m_lastFrame.poc <= m_lastFrame.lastRAP)
            {
                const mfxI32 & lastRAP = itOut->lastRAP;
                // On the 1st TRAIL remove all except IRAP
                m_dpb.erase(std::remove_if(m_dpb.begin(), m_dpb.end(),
                                                [&lastRAP] (Frame const & entry) { return entry.poc != lastRAP; } ),
                                        m_dpb.end());
            }

            //=================4. Construct RPL=====================
            std::sort(m_dpb.begin(), m_dpb.end(),   [](const Frame & lhs_frame, const Frame & rhs_frame)
                                                    {
                                                        return lhs_frame.poc < rhs_frame.poc;
                                                    });

            std::vector<Frame> & L0 = out.ListX[0];
            std::vector<Frame> & L1 = out.ListX[1];

            L0.clear();
            L1.clear();

            if (!isI)
            {
                // Fill L0/L1
                for (auto it = m_dpb.begin(); it != m_dpb.end(); it++)
                {
                    bool list = it->poc > out.poc;
                    out.ListX[list].push_back(*it);
                }

                auto preferSamePolarity = [&](const Frame & lhs_frame, const Frame & rhs_frame)
                {
                   mfxI32 currFrameNum = GetFrameNum(true, out.poc, out.bSecondField);

                   mfxU32 lhs_distance = std::abs(GetFrameNum(true, lhs_frame.poc, lhs_frame.bSecondField) - currFrameNum) * 2 + ((lhs_frame.bBottomField == out.bBottomField) ? 0 : 1);

                   mfxU32 rhs_distance = std::abs(GetFrameNum(true, rhs_frame.poc, rhs_frame.bSecondField) - currFrameNum) * 2 + ((rhs_frame.bBottomField == out.bBottomField) ? 0 : 1);

                   return lhs_distance <= rhs_distance;
                };

                auto distance = [&](const Frame & lhs_frame, const Frame & rhs_frame)
                {
                   mfxU32 lhs_distance = std::abs(lhs_frame.poc - out.poc);
                   mfxU32 rhs_distance = std::abs(rhs_frame.poc - out.poc);

                   return lhs_distance < rhs_distance;
                };

                // If lists are bigger than max supported, sort them and remove extra elements
                if (L0.size() > (isB ? CO3.NumRefActiveBL0[0] : CO3.NumRefActiveP[0]))
                {
                    if (isPPyramid)
                    {
                        // For P-pyramid we remove oldest references
                        // with the highest layer except the closest frame or field pair.

                        if (bIsFieldCoding)
                        {
                            std::sort(L0.begin(), L0.end(), [&](const Frame & lhs_frame, const Frame & rhs_frame)
                                                            {
                                                                return !preferSamePolarity(lhs_frame, rhs_frame);
                                                            });
                        }
                        else
                        {
                            std::sort(L0.begin(), L0.end(), [&](const Frame & lhs_frame, const Frame & rhs_frame)
                                                            {
                                                                return !distance(lhs_frame, rhs_frame);
                                                            });
                        }

                        while (L0.size() > CO3.NumRefActiveP[out.PLayer])
                        {
                            auto weak = L0.begin();
                            for (auto it = L0.begin(); it != L0.end(); it ++)
                            {
                                if (weak->PLayer < it->PLayer &&
                                    (bIsFieldCoding ?
                                        it->poc != L0.rbegin()[0].poc && it->poc != L0.rbegin()[1].poc :
                                        it->poc != L0.rbegin()[0].poc))
                                {
                                    weak = it;
                                }
                            }

                            L0.erase(weak);
                        }
                    } // if PPyramid
                }

                if (bIsFieldCoding)
                {
                    std::sort(L0.begin(), L0.end(), preferSamePolarity);
                    std::sort(L1.begin(), L1.end(), preferSamePolarity);
                }
                else
                {
                    std::sort(L0.begin(), L0.end(), distance);
                    std::sort(L1.begin(), L1.end(), distance);
                }

                if ((m_emuPar.mfx.GopOptFlag & MFX_GOP_CLOSED))
                {
                    const mfxI32 & IPoc = itOut->IPoc;
                    {
                        // Remove L0 refs beyond GOP
                        L0.erase(std::remove_if(L0.begin(), L0.end(),
                                                        [&IPoc] (const Frame & frame) { return frame.poc < IPoc; } ),
                                                L0.end());
                    }

                    const mfxI32 & nextIPoc = itOut->nextIPoc;
                    if (nextIPoc != -1)
                    {
                        // Remove L1 refs beyond GOP
                        L1.erase(std::remove_if(L1.begin(), L1.end(),
                                                        [&nextIPoc] (const Frame & frame) { return frame.poc >= nextIPoc; } ),
                                                L1.end());
                    }
                }

                // if B's L1 is zero (e.g. in case of closed gop), take closest
                if (isB && !L1.size() && L0.size())
                    L1.push_back( *std::min_element(L0.cbegin(), L0.cend(), distance) );

                if (!isB)
                {
                    L1 = L0;
                    std::sort(L1.begin(), L1.end(), distance);
                }

                // Remove extra entries
                if (isB && L0.size() > CO3.NumRefActiveBL0[0])
                    L0.resize(CO3.NumRefActiveBL0[0]);
                if (!isB && L0.size() > CO3.NumRefActiveP[0])
                    L0.resize(CO3.NumRefActiveP[0]);

                if (L1.size() > CO3.NumRefActiveBL1[0])
                    L1.resize(CO3.NumRefActiveBL1[0]);

                std::sort(L0.begin(), L0.end(), distance);
                std::sort(L1.begin(), L1.end(), distance);
            }

            //=================5. Save current frame in DPB=====================
            if (isRef)
            {
                if (m_dpb.size() == m_emuPar.mfx.NumRefFrame)
                {
                    auto toRemove = m_dpb.begin();
                    if (isPPyramid)
                    {
                        auto weak = m_dpb.begin();

                        // remove a picture with the highest layer
                        for (auto it = m_dpb.begin(); it != m_dpb.end(); it ++)
                        {
                            if (bIsFieldCoding && it->poc / 2 == itOut->poc / 2)
                                continue;
                            if (weak->PLayer < it->PLayer)
                                weak = it;
                        }
                        toRemove = weak;
                    }

                    m_dpb.erase(toRemove);
                }
                m_dpb.push_back(*itOut);
            }

            itOut->nalType = GetNALUType(*itOut, !bIsFieldCoding);

            if (itOut->nalType == CRA_NUT || itOut->nalType == IDR_W_RADL)
                itOut->lastRAP = itOut->poc;

            m_lastFrame = *itOut;

            m_queue.erase(itOut);

            return out;
        }
    };

    class TestSuite
        : public tsBitstreamProcessor
        , public tsSurfaceProcessor
        , public tsParserHEVC2
        , public EncodeEmulator
        , public tsVideoEncoder
    {
    private:
        std::vector<ExternalFrame> m_emu;
    #ifdef DUMP_BS
        tsBitstreamWriter m_writer;
    #endif

    public:
        static const unsigned int n_cases;

        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3
        };

        typedef struct
        {
            mfxU32 type;
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
            mfxU32 nFrames;
        } tc_struct;

        static const tc_struct test_case[];

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
    #ifdef DUMP_BS
            , m_writer("/tmp/debug.265")
    #endif
        {
            m_bs_processor = this;
            m_filler = this;
        }

        ~TestSuite(){}

        int RunTest(unsigned int id);

        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
        {
            if (ps)
            {
                ps->Data.FrameOrder = m_cur;
                ps->Info.PicStruct = 0;
                ExternalFrame f = ProcessFrame(*ps, m_cur < m_max ? false : true);

                if (f.poc >= 0)
                    m_emu.push_back(f);
            }

            if (m_cur >= m_max)
                return 0;

            m_cur++;

            return ps;
        }

        void Init()
        {
            GetVideoParam();
            EncodeEmulator::Init(m_par, m_max);
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

    #ifdef DUMP_BS
        m_writer.ProcessBitstream(bs, nFrames);
    #endif

            while (nFrames--)
            {
                auto& hdr = ParseOrDie();

                for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
                {
                    if (!IsHEVCSlice(pNALU->nal_unit_type))
                        continue;

                    const auto & sh = *pNALU->slice;

                    g_tsLog << "POC " << sh.POC << "\n";

                    auto frame = std::find_if(m_emu.begin(), m_emu.end(), [&sh] (const ExternalFrame & frame) { return frame.poc == sh.POC; });

                    if (frame == m_emu.end())
                    {
                        auto reorderedFrame = std::find_if(m_queue.begin(), m_queue.end(), [&sh] (const Frame & frame) { return frame.poc == sh.POC; });
                        if (reorderedFrame == m_queue.end())
                        {
                            g_tsLog << "ERROR: Test Problem: encoded frame emulation not found, check test logic\n";
                            return MFX_ERR_ABORTED;
                        }

                        static std::string slices[] = {"B", "P", "I" };

                        g_tsLog << "ERROR: Frame is in the reordering list and not encoded yet\n";
                        g_tsLog << " Slice type:\n";
                        g_tsLog << " - Actual: " << slices[sh.type]
                            << " - Expected: " << FrameTypeToChar(reorderedFrame->type) << "\n";
                        return MFX_ERR_ABORTED;
                    }

                    ExternalFrame & emulated = *frame;

                    mfxU32 lx[2] = { sh.num_ref_idx_l0_active, sh.num_ref_idx_l1_active };

                    for (mfxU32 list = 0; list < 2; list++)
                    {
                        RefPic * actualList = list == 0 ? sh.L0 : sh.L1;
                        g_tsLog << " List " << list << ":\n";

                        for (mfxU32 idx = 0; idx < lx[list]; idx++)
                        {
                            mfxI32 expectedPOC = (idx < emulated.ListX[list].size()) ? emulated.ListX[list][idx].poc : -1;
                            g_tsLog << " - Actual: " << actualList[idx].POC
                                << " - Expected: " << expectedPOC << "\n";
                            EXPECT_EQ(expectedPOC, actualList[idx].POC)
                                << " list = " << list
                                << " idx = " << idx << "\n";
                        }
                    }

                    m_emu.erase(frame);
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }
    };

#define mfx_PicStruct   tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize  tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist  tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_GopOptFlag  tsStruct::mfxVideoParam.mfx.GopOptFlag
#define mfx_IdrInterval tsStruct::mfxVideoParam.mfx.IdrInterval
#define mfx_PRefType    tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType    tsStruct::mfxExtCodingOption2.BRefType
#define mfx_GPB         tsStruct::mfxExtCodingOption3.GPB

    const TestSuite::tc_struct TestSuite::test_case[] =
    {

        {/*00*/ LOWPOWER | NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                300},
        {/*01*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                600},
        {/*02*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                600},
        {/*03*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                80},
        {/*04*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                163},
        {/*05*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  3 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                163},
        {/*06*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*07*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*08*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  4 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*09*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*10*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*11*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*12*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*13*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*14*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*15*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                80},
        {/*16*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*17*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_GopOptFlag,  MFX_GOP_CLOSED | MFX_GOP_STRICT },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                163},
        {/*18*/ LOWPOWER | NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                80},
        {/*19*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
        {/*20*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
        {/*21*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { EXT_COD3, &mfx_GPB,         MFX_CODINGOPTION_OFF }},
                80},
        {/*22*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_TOP },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  8 },
                 { MFX_PAR,  &mfx_IdrInterval, 1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { EXT_COD3, &mfx_GPB,         MFX_CODINGOPTION_OFF }},
                163},
        {/*23*/ NOT_LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_FIELD_BOTTOM },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_PYRAMID }},
                163},
        {/*24*/ LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                 50 },
        {/*25*/ LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID }},
                 50 },
        {/*26*/ LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  50 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD3, &mfx_PRefType,    MFX_P_REF_SIMPLE }},
                 200 },
        {/*27*/ LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  30 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                 200 },
        {/*28*/ LOWPOWER,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  256 },
                 { MFX_PAR,  &mfx_GopRefDist,  1 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF }},
                 600 },
        {/*29*/ LOWPOWER | NOT_LOWPOWER | HUGE_SIZE_4K,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2160 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4096 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  2160 }},
                 1 },
        {/*30*/ LOWPOWER | NOT_LOWPOWER | HUGE_SIZE_8K,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_OFF },
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  8192 },
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 },
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  8192 },
                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4096 }},
                 1 },
        {/*31*/ LOWPOWER | NOT_LOWPOWER | HUGE_SIZE_4K,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  4096 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2160 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  4096 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  2160 }},
                 1 },
        {/*32*/ LOWPOWER | NOT_LOWPOWER | HUGE_SIZE_8K,
                {{ MFX_PAR,  &mfx_PicStruct,   MFX_PICSTRUCT_PROGRESSIVE },
                 { MFX_PAR,  &mfx_GopPicSize,  0 },
                 { MFX_PAR,  &mfx_GopRefDist,  0 },
                 { EXT_COD2, &mfx_BRefType,    MFX_B_REF_PYRAMID },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  8192 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4096 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,  8192 },
                 { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,  4096 }},
                 1 }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        const tc_struct& tc = test_case[id];

        mfxU32 allowedType = 0;

        TS_START;

        if (m_par.mfx.LowPower == MFX_CODINGOPTION_ON && g_tsHWtype >= MFX_HW_CNL)
        {
            allowedType |= LOWPOWER;
        }
        else
        {
            allowedType |= NOT_LOWPOWER;
        }

        if (!(tc.type & allowedType) || (g_tsHWtype < MFX_HW_CNL && g_tsConfig.lowpower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "[WARNING] SKIPPING TEST-CASE #" << id << ": NOT ALLOWED TEST TYPE\n";
            return 0;
        }

        SETPARS(m_pPar, MFX_PAR);

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2);

        mfxExtCodingOption3& co3 = m_par;
        SETPARS(&co3, EXT_COD3);

        m_max = tc.nFrames;
        m_par.AsyncDepth = 1;

        if (g_tsHWtype <= MFX_HW_CNL && g_tsConfig.lowpower != MFX_CODINGOPTION_ON && (tc.type & HUGE_SIZE_8K))
        {
            MFXInit();
            Load();
            g_tsLog << "\n\nWARNING: 8k resolution is not supported on platform less ICL without VDENC!\n\n\n";
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            Query();
            throw tsSKIP;
        }

        Init();
        GetVideoParam();

        if ( !(tc.type & (HUGE_SIZE_4K | HUGE_SIZE_8K) && (g_tsConfig.sim)) )
            EncodeFrames(tc.nFrames);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(hevce_default_ref_lists);
}
