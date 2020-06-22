// Copyright (c) 2020 Intel Corporation
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

#include <cstdint>
#include <vector>
#include <array>
#include <deque>

#include "av1_scd.h"
#include "aenc.h"

#if defined(ENABLE_ADAPTIVE_ENCODE)

namespace aenc {

    enum class FrameType { UNDEF, IDR, I, P, B, DUMMY };

    class Error
    {
    public:
        Error(const char* msg) : Message(msg) {};
        const char* Message;
    };


    class ExternalFrame
    {
    public:
        uint32_t POC = 0;
        bool SceneChanged = false;
        bool RepeatedFrame = false;
        uint32_t TemporalComplexity = 0;
        bool LTR = false;
        uint32_t MiniGopSize = 0;
        uint32_t PyramidLayer = 0;
        FrameType Type = FrameType::UNDEF;
        int32_t DeltaQP = 0;
        uint32_t ClassAPQ = 0;
        uint32_t ClassCmplx = 0;
        bool KeepInDPB = 0;
        std::vector<uint32_t> RemoveFromDPB;
        std::vector<uint32_t> RefList;

        void print();
        operator AEncFrame();
    };


    class InternalFrame
    {
    public:
        uint32_t POC = 0;
        bool SceneChanged = false;
        bool RepeatedFrame = false;
        bool SceneTransition = false;
        uint32_t TemporalComplexity = 0;
        bool LTR = false;
        bool UseLtrAsReference = false;
        uint32_t MiniGopSize = 0;  //actual number of frames in MiniGOP, may be odd
        uint32_t MiniGopType = 0;  //strictly 1, 2, 4, or 8 frames
        uint32_t MiniGopIdx = 0;
        uint32_t PyramidLayer = 0;
        FrameType Type = FrameType::UNDEF;
        int32_t DeltaQP = 0;
        bool KeepInDPB = false;
        std::vector<uint32_t> RemoveFromDPB;
        std::vector<uint32_t> RefList;

        //AGOP
        uint32_t PPyramidIdx = 0;
        uint32_t PPyramidLayer = 0;
        FrameType PrevType = FrameType::UNDEF;

        //stat
        //AREF / ALTR
        int32_t highMvCount = 0;
        int32_t MV = 0;
        int32_t CORR = 0;

        //APQ
        int32_t SC = 0;         // Scene complexity index
        int32_t TSC = 0;        // TemporalSpatial complexity index
        int32_t MVSize = 0;     // Sum Abs Mv
        int32_t Contrast = 0;   // Constrast Quarter Sub Pic
        int32_t ClassAPQ = 0;   // APQ Classes 0,1,2,3
        int32_t ClassSCTSC = 0; // Combo index

        //SCD image
        ASCVidSample ScdImage;
        ASCTSCstat ScdStat;

        operator ExternalFrame();
    };



    class AEnc
    {
    public:
        void Init(AEncParam param);
        void Close();

        mfxStatus ProcessFrame(uint32_t POC, const uint8_t* InFrame, int32_t pitch, AEncFrame* OutFrame);
        void   UpdatePFrameBits(uint32_t displayOrder, uint32_t bits, uint32_t QpY, uint32_t ClassCmplx);

    protected:
        //stat computation
        void RunScd(const uint8_t* InFrame, int32_t pitch, InternalFrame& f);
        void SaveStat(InternalFrame& f);
        void SaveAltrStat(InternalFrame& f);
        void SaveArefStat(InternalFrame& f); //only saves stat
        void SaveApqStat(InternalFrame& f); //only saves stat

        //first stage, GOP/miniGOP decision
        bool MakeMiniGopDecision(uint32_t& GOPDecision);
        uint32_t GetMiniGopSizeCommon();
        uint32_t GetMiniGopSizeAGOP();
        void MakeIFrameDecision(InternalFrame& f);

        //second stage, reflist and dQP decision
        void SaveFrameTypeInfo(InternalFrame& f);
        void ComputeStat(InternalFrame& f);
        void MakeDbpDecision(InternalFrame& f);
        void BuildRefList(InternalFrame& f);
        void AdjustQp(InternalFrame& f);

        //tool specific
        void ComputeStatAltr(InternalFrame& f);
        void ComputeStatAref(InternalFrame& f);
        void ComputeStatApq(InternalFrame& f);
        void MakeAltrArefDecision(InternalFrame& f);
        void MakeArefDecision(InternalFrame& f);
        void MakeAltrDecision(InternalFrame& f);
        void MakeDbpDecisionLtr(InternalFrame& f);
        void MakeDbpDecisionAref(InternalFrame& f);
        void BuildRefListLtr(InternalFrame& f);
        void BuildRefListAref(InternalFrame& f);
        void AdjustQpLtr(InternalFrame& f);
        void AdjustQpAref(InternalFrame& f);
        void AdjustQpApq(InternalFrame& f);
        void AdjustQpAgop(InternalFrame& f);
        bool isArefKeyFrame(InternalFrame f, uint32_t dist);


        //out
        mfxStatus OutputDecision(AEncFrame* OutFrame);

        //misc
        void MarkFrameAsI(InternalFrame& f);
        void MarkFrameAsIDR(InternalFrame& f);
        void MarkFrameAsLTR(InternalFrame& f);
        void MarkFrameInMiniGOP(InternalFrame& f, uint32_t MiniGopSize, uint32_t MiniGopIdx);
        template <typename Condition>
        void RemoveFrameFromDPB(InternalFrame& f, Condition C); //removes frame that satisfies condition, updates DPB in "f"
        template <typename Condition>
        void AddFrameToRefList(InternalFrame& f, Condition C); //add frame to ref list in "f" from DPB, that satisfies condition
        template <size_t size>
        void SetFrameQP(InternalFrame& f, std::array<int32_t, size> QpTable);


        //general
        ASC Scd;
        AEncParam InitParam{};

        std::deque<InternalFrame> FrameBuffer;
        std::deque<ExternalFrame> OutputBuffer;
        std::vector<InternalFrame> DpbBuffer;
        std::vector<uint32_t> RemoveFromDPBDelayed;

        //ALTR specific
        ASC LtrScd;
        uint32_t LtrPoc = 0; //TODO: remove global LtrPoc
        bool m_isLtrOn = false;

        //GOP specific
        uint32_t PocOfLastIFrame = 0;
        uint32_t PocOfLastIdrFrame = 0;

        // APQ Feedback
        uint32_t LastPFrameNoisy = 0;
        uint32_t LastPFrameQp = 0;

        //ALTR specific
        int32_t m_avgMV0 = 0;
        uint32_t m_sceneTranBuffer[8]{};  // Buffer holds scene transition data for recent 8  frames
        int32_t DeltaQpForLtrFrame = 0; // set default as -4;

        //AREF
        mfxU8  m_prevTemporalActs[8]{};
        mfxU8  m_hasLowActivity = 0;
        uint32_t PocOfLastArefKeyFrame = 0;
        int32_t DeltaQpOffsetForAref = 0;
    };

} //namespace aenc

#endif // ENABLE_ADAPTIVE_ENCODE
