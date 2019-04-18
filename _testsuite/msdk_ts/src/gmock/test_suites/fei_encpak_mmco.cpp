/******************************************************************************* *\

Copyright (C) 2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

#include "ts_encpak.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include "ts_fei_warning.h"
#include "avc2_struct.h"

namespace fei_encpak_mmco
{

// mmco types
enum
{
    MMCO_0  = 0,
    MMCO_1  = 1, // exclude ST
    MMCO_2  = 2, // exclude LT
    MMCO_3  = 3, // ST -> LT
    MMCO_4  = 4, // cut LTs
    MMCO_5  = 5, // remove all
    MMCO_6  = 6, // current -> LT
};

#define MAX_MMCO_IN_CASE 15

class TestSuite
{
public:
    tsVideoENCPAK encpak;

    TestSuite() : encpak(MFX_FEI_FUNCTION_ENC, MFX_FEI_FUNCTION_PAK, MFX_CODEC_AVC, true) {} // fix function id
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    struct mmco_par
    {
        mfxU16    frameNum; // count in pictures (2 per frame for field mode)
        mfxU16    mmco;
        mfxI16    idxST; // diff with current (without minus1)
        mfxU16    idxLT;
    };
    struct tc_struct
    {
        mfxStatus sts;
        mfxU16 mode; // MFX_PICSTRUCT_PROGRESSIVE, ...
        mfxU16 gopRefDist;
        struct mmco_par mmco_set[MAX_MMCO_IN_CASE];
    };

private:

    static const tc_struct test_case[];

};

// NOTE: it is only preliminary version with only few successive cases
// MMC0_5 (clear all) shouldn't be tested - it is not used in msdk

const TestSuite::tc_struct TestSuite::test_case[] =
{
 /*00*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{0}} }, // empty
 /*01*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{3, MMCO_1, 2, 0}, {6, MMCO_0, 0, 0}, {0}} }, // rm ST - ok
 /*02*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{3, MMCO_3, 2, 1}, {6, MMCO_2, 0, 1}, {0}} }, // rm LT - ok
 /*03*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{3, MMCO_3, 2, 0}, {0}} }, // ST->LT - ok
 /*04*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{2, MMCO_3, 1, 2}, {3, MMCO_3, 3, 1}, {4, MMCO_3, 1, 0}, {4, MMCO_1, 2, 0}, {5, MMCO_4, 0, 0}, {0}} }, // cut LT - no - (lib uses mmco2 instead)
 /*05*/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{1, MMCO_6, 0, 1}, {0}} }, // curr to LT - ok
//fields only for ST (mmco1)
 /*06*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{0}} }, // empty
 /*07*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{6, MMCO_1, 4, 0}, {6, MMCO_1, 5, 0}, {0}} }, // rm ST - ok
 /*08*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{6, MMCO_1, 4, 0}, {0}} }, // 1 - of 4 cases: remove only one field
 /*09*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{6, MMCO_1, 5, 0}, {0}} }, // 2
 /*10*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{7, MMCO_1, 4, 0}, {0}} }, // 3
 /*11*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{7, MMCO_1, 5, 0}, {0}} }, // 4
 /*12*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_TFF, 1, {{2, MMCO_1, 3, 0}, {4, MMCO_1, 4, 0}, {4, MMCO_1, 2, 0}, {5, MMCO_1, 2, 0}, {0}} }, // mix of mmco_1, rms 2 1st frames

 /*13*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{0}} }, // empty
 /*14*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{6, MMCO_1, 4, 0}, {6, MMCO_1, 5, 0}, {0}} }, // rm ST - ok
 /*15*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{6, MMCO_1, 4, 0}, {0}} }, // 1 - of 4 cases: remove only one field
 /*16*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{6, MMCO_1, 5, 0}, {0}} }, // 2
 /*17*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{7, MMCO_1, 4, 0}, {0}} }, // 3
 /*18*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{7, MMCO_1, 5, 0}, {0}} }, // 4
 /*19*/ {MFX_ERR_NONE, MFX_PICSTRUCT_FIELD_BFF, 1, {{2, MMCO_1, 3, 0}, {4, MMCO_1, 4, 0}, {4, MMCO_1, 2, 0}, {5, MMCO_1, 2, 0}, {0}} }, // mix of mmco_1, rms 2 1st frames

// /**/ {MFX_ERR_NONE, MFX_PICSTRUCT_PROGRESSIVE, 1, {{0, MMCO_6, 0, 2}, {3, MMCO_6, 0, 4}, {3, MMCO_2, 2, 0}, {4, MMCO_3, 0, 1}, {0}} }, // mix - no
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);
mfxStatus LoadTC(std::vector<mfxExtBuffer*>& encbuf, std::vector<mfxExtBuffer*>& pakbuf, const TestSuite::tc_struct& tc, mfxU32 frame, mfxU32 field, mfxU32 idxToPickBuffers)
{
    mfxU32 fieldMode = (tc.mode != MFX_PICSTRUCT_PROGRESSIVE);
    mfxU32 picture = fieldMode ? frame * 2 + field : frame;

    mfxU32 picType = !fieldMode ? MFX_PICTYPE_FRAME :
            ((tc.mode == MFX_PICSTRUCT_FIELD_TFF) == (field == 0)) ? MFX_PICTYPE_TOPFIELD : MFX_PICTYPE_BOTTOMFIELD;

    mfxU16 processed[PpsDPBSize] = {0}; // for fields were affected (MFX_PICTYPE_*)

    // field is set index but not true field - only 0 for single_field mode
    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encbuf, MFX_EXTBUFF_FEI_PPS, idxToPickBuffers);
    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(pakbuf, MFX_EXTBUFF_FEI_PPS, idxToPickBuffers);
    if (!ppsE || !ppsP)
        return MFX_ERR_NOT_FOUND;

    mfxExtFeiPPS::mfxExtFeiPpsDPB newDpb[PpsDPBSize];
    bool havemmco = false;

    for (int op = 0; tc.mmco_set[op].mmco != 0; op++) {
        if (tc.mmco_set[op].frameNum != picture)
            continue;
        const mfxExtFeiPPS::mfxExtFeiPpsDPB *current = 0; // current frame in newDpb

        // for first mmco occurrence
        if (!havemmco && tc.mmco_set[op].mmco != MMCO_0) {
            havemmco = true;
            CopyPpsDPB(ppsE->DpbBefore, newDpb); // will rebuild without sliding window
            // first add current frame, if it was in DpbAfter and not in DpbBefore
            const mfxExtFeiPPS::mfxExtFeiPpsDPB *origin = 0; // current frame if it were no mmco
            for (int d = 0; d < PpsDPBSize && !origin && ppsE->DpbAfter[d].Index != 0xffff; d++)
                if (ppsE->DpbAfter[d].FrameNumWrap == (mfxI32)frame)
                    origin = &ppsE->DpbAfter[d];

            if (origin)
            for (int d = 0; d < PpsDPBSize; d++) {// while not same nor empty
                if (newDpb[d].Index == 0xffff || newDpb[d].FrameNumWrap == (mfxI32)frame) {
                    newDpb[d] = *origin;
                    current = &newDpb[d];
                    break;
                }
            }
            // if (!current) - possible if not reference frame
        }

        switch (tc.mmco_set[op].mmco) {
        case MMCO_1:
            {
                mfxI32 target = (picture | fieldMode) - tc.mmco_set[op].idxST; // "CurrPicNum is set equal to 2 * frame_num + 1"
                bool err = true;
                for (int d = 0; d < PpsDPBSize && newDpb[d].Index != 0xffff; d++) {
                    if (newDpb[d].LongTermFrameIdx == 0xffff && newDpb[d].FrameNumWrap == (target >> fieldMode)) {
                        mfxU32 refType = !fieldMode ? (MFX_PICTYPE_FRAME | MFX_PICTYPE_TOPFIELD | MFX_PICTYPE_BOTTOMFIELD) :
                                ((target & 1) ? picType : (picType ^ (MFX_PICTYPE_TOPFIELD | MFX_PICTYPE_BOTTOMFIELD))); // same parity for bigger picnum
                        if (processed[d] & refType) // same picture again
                            return MFX_ERR_ABORTED; // or what?
                        err = false;
                        processed[d] |= refType;
                        newDpb[d].PicType &= ~refType;
                        if (newDpb[d].PicType) // one field left - keep it
                            break;
                        // remove with shift left
                        for ( ; d+1 < PpsDPBSize && newDpb[d+1].Index != 0xffff; d++) {
                            newDpb[d] = newDpb[d+1];
                            processed[d] = processed[d+1];
                        }
                        newDpb[d].Index = 0xffff;
                        break;
                    }
                }
                if (err)
                    return MFX_ERR_ABORTED; // or what?
            }
            break;
        case MMCO_2:
            {
                mfxI32 target = tc.mmco_set[op].idxLT;
                bool err = true;
                for (int d = 0; d < PpsDPBSize && newDpb[d].Index != 0xffff; d++) {
                    if (newDpb[d].LongTermFrameIdx == target) {
                        mfxU32 refType = 0xff; // any. TODO check if it can work by fields
                        if (processed[d] & refType) // same picture again
                            return MFX_ERR_ABORTED; // or what?
                        processed[d] |= refType;
                        // remove with shift left
                        for ( ; d+1 < PpsDPBSize && newDpb[d+1].Index != 0xffff; d++) {
                            newDpb[d] = newDpb[d+1];
                            processed[d] = processed[d+1];
                        }
                        newDpb[d].Index = 0xffff;
                        err = false;
                        break;
                    }
                }
                if (err)
                    return MFX_ERR_ABORTED; // or what?
            }
            break;
        case MMCO_6: // assumes tc.mmco_set[op].idxST is 0
        case MMCO_3:
            {
                mfxI32 target = (picture | fieldMode) - tc.mmco_set[op].idxST;
                bool err = true;
                for (int d = 0; d < PpsDPBSize && newDpb[d].Index != 0xffff; d++) {
                    if (newDpb[d].LongTermFrameIdx == 0xffff && newDpb[d].FrameNumWrap == (target >> fieldMode)) {
                        mfxU32 refType = !fieldMode ? (MFX_PICTYPE_FRAME | MFX_PICTYPE_TOPFIELD | MFX_PICTYPE_BOTTOMFIELD) :
                                ((target & 1) ? picType : (picType ^ (MFX_PICTYPE_TOPFIELD | MFX_PICTYPE_BOTTOMFIELD))); // same parity for bigger picnum
                        if (processed[d] & refType) // same picture again
                            return MFX_ERR_ABORTED; // or what?
                        processed[d] |= refType;
                        // change ST to LT
                        newDpb[d].LongTermFrameIdx = tc.mmco_set[op].idxLT;
                        err = false;
                        break;
                    }
                }
                if (err)
                    return MFX_ERR_ABORTED; // or what?
            }
            break;
        case MMCO_4: // now msdk removes one by one, never go here
            {
                mfxI32 target = tc.mmco_set[op].idxLT; // max allowed value
                bool err = true;
                for (int d = 0; d < PpsDPBSize && newDpb[d].Index != 0xffff; d++) {
                    if (newDpb[d].LongTermFrameIdx != 0xffff && newDpb[d].LongTermFrameIdx > target) {
                        mfxU32 refType = 0xff; // any. TODO check if it can work by fields
                        if (processed[d] & refType) // same picture again
                            return MFX_ERR_ABORTED; // or what?
                        processed[d] |= refType;
                        // remove with shift left
                        for ( ; d+1 < PpsDPBSize && newDpb[d+1].Index != 0xffff; d++) {
                            newDpb[d] = newDpb[d+1];
                            processed[d] = processed[d+1];
                        }
                        newDpb[d].Index = 0xffff;
                        err = false; // at least one is required for testing
                    }
                }
                if (err)
                    return MFX_ERR_ABORTED; // or what?
            }
            break;
        case MMCO_5: // now msdk removes one by one, never go here
            if (current) // put current first, if exists
                newDpb[0] = *current;
            else
                newDpb[0].Index = 0xffff;
            // remove all ST and LT
            for (int d = 1; d < PpsDPBSize; d++)
                newDpb[d].Index = 0xffff;
            break;
        default:
            return MFX_ERR_ABORTED; // or what?
        }
    }

    if (havemmco) {
        CopyPpsDPB(newDpb, ppsE->DpbAfter); // update with based on mmco
        CopyPpsDPB(newDpb, ppsP->DpbAfter); // and for pak
    }

    return MFX_ERR_NONE;
}


#define VERIFY_FIELD(var, expected, name) { \
    g_tsLog << name << " = " << var << " (expected = " << expected << ")\n"; \
    if ( var != expected) { \
        g_tsLog << "ERROR: " << name <<  " in stream != expected\n"; \
        err++; \
    } \
}

class ProcessSetMMCO : public tsSurfaceProcessor, public tsBitstreamWriter, public tsParserH264AU
{
    mfxU32 m_nFields;
    mfxU32 m_nSurf;
    mfxU32 m_nPic;
    mfxVideoParam& m_par;

public:
    TestSuite::tc_struct m_tc; // some fields differs with original tc (numRef*)

    ProcessSetMMCO(mfxVideoParam& par, const TestSuite::tc_struct& tc, const char* fname)
        : tsBitstreamWriter(fname), tsParserH264AU(), m_nSurf(0), m_nPic(0), m_par(par), m_tc(tc)
    {
        m_nFields = m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
        set_trace_level(BS_H264_TRACE_LEVEL_REF_LIST);
    }

    ~ProcessSetMMCO()
    {
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        s.Data.FrameOrder = m_nSurf;
        m_nSurf++;

        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        using namespace BS_AVC2; // for NALU type
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        mfxU32 nFrame = (m_nFields == 2) ? m_nPic >> 1 : m_nPic;
        mfxU16 err = 0;

        while (nFrames--) {
            for (mfxU16 field = 0; field < 1/*m_nFields*/; field++) { // now one field per call
                UnitType& au = ParseOrDie();
                for (Bs32u i = 0; i < au.NumUnits; i++)
                {
                    if (au.NALU[i].nal_unit_type != SLICE_IDR && au.NALU[i].nal_unit_type != SLICE_NONIDR)
                        continue; // todo: check if missed pps succeeds
                    dec_ref_pic_marking_params* marking = au.NALU[i].slice_hdr->dec_ref_pic_marking;
                    int numExpected = 0, numFound = 0;
                    for (int d = 0; d<MAX_MMCO_IN_CASE; numExpected += (m_tc.mmco_set[d].mmco != 0 && m_tc.mmco_set[d].frameNum == m_nPic), d++);

                    while (marking) {
                        numFound += (marking->memory_management_control_operation != 0);
                        bool matched = false;

                        switch (marking->memory_management_control_operation) {
                        case MMCO_0:
                            matched = true;
                            break;
                        case MMCO_1:
                            {
                                mfxI32 target = marking->difference_of_pic_nums_minus1+1;
                                for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic ||
                                            m_tc.mmco_set[d].mmco != marking->memory_management_control_operation ||
                                            m_tc.mmco_set[d].idxST != target)
                                        continue;
                                    matched = true;
                                }
                            }
                            break;
                        case MMCO_2:
                            {
                                mfxU32 target = marking->long_term_pic_num;
                                for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic ||
                                            m_tc.mmco_set[d].mmco != marking->memory_management_control_operation ||
                                            m_tc.mmco_set[d].idxLT != target)
                                        continue;
                                    matched = true;
                                }
                            }
                        break;
                        case MMCO_3:
                            {
                                mfxI32 target = marking->difference_of_pic_nums_minus1+1;
                                for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic ||
                                            m_tc.mmco_set[d].mmco != marking->memory_management_control_operation ||
                                            m_tc.mmco_set[d].idxST != target)
                                        continue;
                                    matched = (marking->long_term_frame_idx == m_tc.mmco_set[d].idxLT);
                                    break;
                                }
                            }
                            break;
                        case MMCO_4: // now msdk removes one by one, never go here
                            {
                                mfxI32 target = marking->max_long_term_frame_idx_plus1 - 1;
                                for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic ||
                                            m_tc.mmco_set[d].mmco != marking->memory_management_control_operation)
                                        continue;
                                    matched = (m_tc.mmco_set[d].idxLT == target);
                                    break;
                                }
                            }
                            break;
                        case MMCO_5: // now msdk removes one by one, never go here
                            {
                                for (int d = 0; d<MAX_MMCO_IN_CASE; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic)
                                        continue;
                                    matched = (m_tc.mmco_set[d].mmco == MMCO_5);
                                    break;
                                }
                            }
                            break;
                        case MMCO_6:
                            {
                                mfxU32 target = marking->long_term_frame_idx;
                                for (int d = 0; d<MAX_MMCO_IN_CASE && !matched; d++) {
                                    if (m_tc.mmco_set[d].frameNum != m_nPic ||
                                            m_tc.mmco_set[d].mmco != marking->memory_management_control_operation)
                                        continue;
                                    matched = (m_tc.mmco_set[d].idxLT == target);
                                    break;
                                }
                            }
                            break;
                        default:
                            ;
                        }
                        VERIFY_FIELD(matched, true, "mmco_" << marking->memory_management_control_operation << " matched ")
                        err += !matched;
                        marking = marking->next;
                    }
                    VERIFY_FIELD(numFound, numExpected, "Number of nz mmco in " << nFrame << " frame (" << m_nPic << ")")
                    err += (numFound != numExpected);
                    g_tsLog << "\n";
                }
            }
        }

        m_nPic++;

        bs.DataOffset = 0;
        bs.DataLength = 0;

        if (err)
            return MFX_ERR_ABORTED;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    CHECK_FEI_SUPPORT();

    const tc_struct& tc = test_case[id];
    //srand(id);

    const mfxU16 n_frames = 10;
    mfxU16 picStruct = tc.mode;
    mfxStatus sts;

    encpak.enc.m_par.mfx.FrameInfo.PicStruct = picStruct;
    encpak.enc.m_pPar->mfx.GopPicSize = 30;
    encpak.enc.m_pPar->mfx.GopRefDist  = tc.gopRefDist;
    encpak.enc.m_pPar->mfx.NumRefFrame = 4;
    encpak.enc.m_pPar->mfx.TargetUsage = 4;
    encpak.enc.m_pPar->mfx.EncodedOrder = 1;
    encpak.enc.m_pPar->AsyncDepth = 1;
    encpak.enc.m_pPar->IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    encpak.pak.m_par.mfx = encpak.enc.m_par.mfx;
    encpak.pak.m_par.AsyncDepth = encpak.enc.m_par.AsyncDepth;
    encpak.pak.m_par.IOPattern = encpak.enc.m_par.IOPattern;

    mfxU16 num_fields = encpak.enc.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    bool bTwoBuffers = (encpak.enc.m_par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) && !encpak.m_bSingleField;

    ProcessSetMMCO pd(encpak.enc.m_par, tc, "bs.out");

    encpak.m_filler = &pd;
    encpak.m_bs_processor = &pd;

    encpak.PrepareInitBuffers();

////// mods for Init to be here
//    mfxExtFeiPPS* ppsE = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.enc.initbuf, MFX_EXTBUFF_FEI_PPS);
//    mfxExtFeiPPS* ppsP = (mfxExtFeiPPS*)GetExtFeiBuffer(encpak.pak.initbuf, MFX_EXTBUFF_FEI_PPS);

    g_tsStatus.disable(); // it is checked many times inside, resetting expected to err_none

    sts = encpak.Init();

    if (sts != MFX_ERR_NONE) {
        g_tsStatus.expect(tc.sts); // if init fails check if it is expected
    }

    if (sts == MFX_ERR_NONE)
        sts = encpak.AllocBitstream((encpak.enc.m_par.mfx.FrameInfo.Width * encpak.enc.m_par.mfx.FrameInfo.Height) * 16 * n_frames + 1024*1024);


    mfxU32 count;
    mfxU32 async = TS_MAX(1, encpak.enc.m_par.AsyncDepth);
    async = TS_MIN(n_frames, async - 1);

    for ( count = 0; count < n_frames && sts == MFX_ERR_NONE; count++) {
        for (mfxU32 field = 0; field < num_fields && sts == MFX_ERR_NONE; field++) {
            // In single-field mode only one set is used
            mfxU32 idxToPickBuffers = encpak.m_bSingleField ? 0 : field;

            sts = encpak.PrepareFrameBuffers(field);
            if (sts != MFX_ERR_NONE) {
                g_tsStatus.expect(tc.sts); // if fails check if it is expected
                g_tsStatus.check(sts);
                break;
            }

            sts = LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, count, field, idxToPickBuffers);
            if (sts != MFX_ERR_NONE) {
                g_tsStatus.expect(tc.sts); // if fails check if it is expected
                g_tsStatus.check(sts);
                break;
            }

            if (bTwoBuffers) {
                LoadTC(encpak.enc.inbuf, encpak.pak.inbuf, tc, count, field, 1 ^ idxToPickBuffers);
            }
            // to modify pps etc
            mfxExtFeiPPS * fppsE = (mfxExtFeiPPS *)GetExtFeiBuffer(encpak.enc.inbuf, MFX_EXTBUFF_FEI_PPS, idxToPickBuffers);
            mfxExtFeiSliceHeader * fsliceE = (mfxExtFeiSliceHeader *)GetExtFeiBuffer(encpak.enc.inbuf, MFX_EXTBUFF_FEI_SLICE, idxToPickBuffers);
            for (mfxI32 s=0; s<fsliceE->NumSlice; s++) {
                fsliceE->Slice[s].PPSId = fppsE->PPSId;
            }

            mfxExtFeiPPS * fppsP = (mfxExtFeiPPS *)GetExtFeiBuffer(encpak.pak.inbuf, MFX_EXTBUFF_FEI_PPS, idxToPickBuffers);
            mfxExtFeiSliceHeader * fsliceP = (mfxExtFeiSliceHeader *)GetExtFeiBuffer(encpak.pak.inbuf, MFX_EXTBUFF_FEI_SLICE, idxToPickBuffers);
            for (mfxI32 s=0; s<fsliceP->NumSlice; s++) {
                fsliceP->Slice[s].PPSId = fppsP->PPSId;
            }

            sts = encpak.EncodeFrame(field);

            g_tsStatus.expect(tc.sts); // if fails check if it is expected
        }

    }

    g_tsLog << count << " FRAMES Encoded\n";

    g_tsStatus.enable();
    int ret = g_tsStatus.check(/*sts*/);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encpak_mmco);
};