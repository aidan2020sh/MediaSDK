// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_segment_decoder_mt.h"
#include "umc_h264_bitstream_inlines.h"

namespace UMC
{

void H264SegmentDecoderMultiThreaded::ResetMVs16x16(int32_t iListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMV, 16);
#endif // __ICL

    memset(pMV, 0, 16 * sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::ResetMVs16x16(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ResetMVs16x8(int32_t iListNum,
                                                   int32_t iVectorOffset)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors +
                                   iVectorOffset;

#ifdef __ICL
    __assume_aligned(pMV, 16);
#endif // __ICL

    memset(pMV, 0, 8 * sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::ResetMVs16x8(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ResetMVs8x16(int32_t iListNum,
                                                   int32_t iVectorOffset)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors +
                                   iVectorOffset;

#ifdef __ICL
    __assume_aligned(pMV, 16);
#endif // __ICL

    memset(pMV, 0, 2 * sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::ResetMVs8x16(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ResetMVs8x8(int32_t iListNum,
                                                  int32_t iVectorOffset)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors +
                                   iVectorOffset;

#ifdef __ICL
    __assume_aligned(pMV, 16);
#endif // __ICL

    memset(pMV, 0, 2 * sizeof(H264DecoderMotionVector));
    memset(pMV + 4, 0, 2 * sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::ResetMVs8x8(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::CopyMVs8x16(int32_t iListNum)
{
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;

#ifdef __ICL
    __assume_aligned(pMV, 16);
#endif // __ICL

    MFX_INTERNAL_CPY(pMV + 4, pMV + 0, 4 * sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(pMV + 8, pMV + 0, 4 * sizeof(H264DecoderMotionVector));
    MFX_INTERNAL_CPY(pMV + 12, pMV + 0, 4 * sizeof(H264DecoderMotionVector));

} // void H264SegmentDecoderMultiThreaded::CopyMVs8x16(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs16x16(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;

    // get the prediction
    ReconstructMVPredictor16x16(iListNum, &mvPred);

    // do the reconstruction
    {
        H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
        H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

#ifdef __ICL
        __assume_aligned(pMV, 16);
        __assume_aligned(pMVd, 16);
#endif // __ICL

        {
            int32_t i;
            H264DecoderMotionVector mv;

            // calculate new vector
            mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
            mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

            if (mv.mvy > m_MVDistortion[iListNum])
                m_MVDistortion[iListNum] = mv.mvy;

            for (i = 0; i < 16; i += 1)
                pMV[i] = mv;
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs16x16(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs16x8(int32_t iListNum,
                                                         int32_t iSubBlockNum)
{
    H264DecoderMotionVector mvPred;

    // get the prediction
    ReconstructMVPredictor16x8(iListNum, iSubBlockNum, &mvPred);

    // do the reconstruction
    {
        H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors +
                                       iSubBlockNum * 8;
        H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors +
                                        iSubBlockNum * 8;

#ifdef __ICL
        __assume_aligned(pMV, 16);
        __assume_aligned(pMVd, 16);
#endif // __ICL

        {
            int32_t i;
            H264DecoderMotionVector mv;

            // calculate new vector
            mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
            mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

            if (mv.mvy > m_MVDistortion[iListNum])
                m_MVDistortion[iListNum] = mv.mvy;

            for (i = 0; i < 8; i += 1)
                pMV[i] = mv;
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs16x8(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x16(int32_t iListNum,
                                                         int32_t iSubBlockNum)
{
    H264DecoderMotionVector mvPred;

    // get the prediction
    ReconstructMVPredictor8x16(iListNum, iSubBlockNum, &mvPred);

    // do the reconstruction
    {
        H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors +
                                       iSubBlockNum * 2;
        H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors +
                                        iSubBlockNum * 2;

#ifdef __ICL
        __assume_aligned(pMV, 16);
        __assume_aligned(pMVd, 16);
#endif // __ICL

        {
            H264DecoderMotionVector mv;

            // calculate new vector
            mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
            mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

            if (mv.mvy > m_MVDistortion[iListNum])
                m_MVDistortion[iListNum] = mv.mvy;

            pMV[0] = mv;
            pMV[1] = mv;
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x16(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVsExternal(int32_t iListNum)
{
    int8_t *pSBType = m_cur_mb.GlobalMacroblockInfo->sbtype;

    switch (pSBType[0])
    {
    case SBTYPE_8x8:
        ReconstructMVs8x8External(iListNum);
        break;
    case SBTYPE_8x4:
        // the first block
        ReconstructMVs8x4External(iListNum);
        // the second block
        ReconstructMVs8x4Left(iListNum, 1);
        break;
    case SBTYPE_4x8:
        // the first block
        ReconstructMVs4x8External(iListNum);
        // the second block
        ReconstructMVs4x8Top(iListNum, 1);
        break;
    default:
        // the first block
        ReconstructMVs4x4External(iListNum);
        // the second block
        ReconstructMVs4x4Top(iListNum, 1);
        // the third block
        ReconstructMVs4x4Left(iListNum, 1);
        // the fourth block
        ReconstructMVs4x4InternalNoCheckRef(iListNum, 5);
        break;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVsExternal(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVsTop(int32_t iListNum)
{
    int8_t *pSBType = m_cur_mb.GlobalMacroblockInfo->sbtype;

    switch (pSBType[1])
    {
    case SBTYPE_8x8:
        ReconstructMVs8x8Top(iListNum);
        break;
    case SBTYPE_8x4:
        // the first block
        ReconstructMVs8x4Top(iListNum);
        // the second block
        ReconstructMVs8x4Internal(iListNum, 6);
        break;
    case SBTYPE_4x8:
        // the first block
        ReconstructMVs4x8Top(iListNum, 2);
        // the second block
        ReconstructMVs4x8Top(iListNum, 3);
        break;
    default:
        // the first block
        ReconstructMVs4x4Top(iListNum, 2);
        // the second block
        ReconstructMVs4x4Top(iListNum, 3);
        // the third block
        ReconstructMVs4x4InternalFewCheckRef(iListNum, 6, 3);
        // the fourth block
        ReconstructMVs4x4InternalNoCheckRef(iListNum, 7);
        break;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVsTop(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVsLeft(int32_t iListNum)
{
    int8_t *pSBType = m_cur_mb.GlobalMacroblockInfo->sbtype;

    switch (pSBType[2])
    {
    case SBTYPE_8x8:
        ReconstructMVs8x8Left(iListNum);
        break;
    case SBTYPE_8x4:
        // the first block
        ReconstructMVs8x4Left(iListNum, 2);
        // the second block
        ReconstructMVs8x4Left(iListNum, 3);
        break;
    case SBTYPE_4x8:
        // the first block
        ReconstructMVs4x8Left(iListNum);
        // the second block
        ReconstructMVs4x8Internal(iListNum, 9, 6);
        break;
    default:
        // the first block
        ReconstructMVs4x4Left(iListNum, 2);
        // the second block
        ReconstructMVs4x4Internal(iListNum, 9, 6);
        // the third block
        ReconstructMVs4x4Left(iListNum, 3);
        // the fourth block
        ReconstructMVs4x4InternalNoCheckRef(iListNum, 13);
        break;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVsLeft(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVsInternal(int32_t iListNum)
{
    int8_t *pSBType = m_cur_mb.GlobalMacroblockInfo->sbtype;

    switch (pSBType[3])
    {
    case SBTYPE_8x8:
        ReconstructMVs8x8Internal(iListNum);
        break;
    case SBTYPE_8x4:
        // the first block
        ReconstructMVs8x4Internal(iListNum, 10);
        // the second block
        ReconstructMVs8x4Internal(iListNum, 14);
        break;
    case SBTYPE_4x8:
        // the first block
        ReconstructMVs4x8Internal(iListNum, 10, 7);
        // the second block
        ReconstructMVs4x8Internal(iListNum, 11, 6);
        break;
    default:
        // the first block
        ReconstructMVs4x4InternalFewCheckRef(iListNum, 10, 7);
        // the second block
        ReconstructMVs4x4InternalFewCheckRef(iListNum, 11, 6);
        // the third block
        ReconstructMVs4x4InternalFewCheckRef(iListNum, 14, 11);
        // the fourth block
        ReconstructMVs4x4InternalNoCheckRef(iListNum, 15);
        break;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVsInternal(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8External(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
        mbAddrC.block_num += 2;
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(iListNum, mbAddrC, &mvPred);
        else
            ReconstructMVPredictorExternalBlockMBAFF(iListNum, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[0] = mv;
        pMV[1] = mv;
        pMV[4] = mv;
        pMV[5] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8External(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Top(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_right;
        ReconstructMVPredictorTopBlock(iListNum, 2, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[2].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[2].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[2] = mv;
        pMV[3] = mv;
        pMV[6] = mv;
        pMV[7] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Top(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Left(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC.mb_num = m_CurMBAddr;
        mbAddrC.block_num = 6;
        ReconstructMVPredictorLeftBlock(iListNum, 2, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[8].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[8].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[8] = mv;
        pMV[9] = mv;
        pMV[12] = mv;
        pMV[13] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Left(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Internal(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlock(iListNum, 10, 5, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[10].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[10].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[10] = mv;
        pMV[11] = mv;
        pMV[14] = mv;
        pMV[15] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x8Internal(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4External(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderBlockLocation mbAddrC;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
    mbAddrC.block_num += 2;
    if (false == m_isMBAFF)
        ReconstructMVPredictorExternalBlock(iListNum, mbAddrC, &mvPred);
    else
        ReconstructMVPredictorExternalBlockMBAFF(iListNum, mbAddrC, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[0] = mv;
        pMV[1] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4External(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Top(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_right;
        ReconstructMVPredictorTopBlock(iListNum, 2, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[2].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[2].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[2] = mv;
        pMV[3] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Top(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Left(int32_t iListNum,
                                                            int32_t iRowNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        if (2 != iRowNum)
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mbs_left[iRowNum - 1];
        else
        {
            mbAddrC.mb_num = m_CurMBAddr;
            mbAddrC.block_num = 6;
        }
        ReconstructMVPredictorLeftBlock(iListNum, iRowNum, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iRowNum * 4].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iRowNum * 4].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iRowNum * 4 + 0] = mv;
        pMV[iRowNum * 4 + 1] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Left(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Internal(int32_t iListNum,
                                                                int32_t iBlockNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlock(iListNum, iBlockNum, iBlockNum - 5, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iBlockNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iBlockNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iBlockNum + 0] = mv;
        pMV[iBlockNum + 1] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs8x4Internal(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8External(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
        mbAddrC.block_num += 1;
        if (false == m_isMBAFF)
            ReconstructMVPredictorExternalBlock(iListNum, mbAddrC, &mvPred);
        else
            ReconstructMVPredictorExternalBlockMBAFF(iListNum, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[0] = mv;
        pMV[4] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8External(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8Top(int32_t iListNum,
                                                           int32_t iColumnNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        if (3 == iColumnNum)
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_right;
        else
        {
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
            mbAddrC.block_num += iColumnNum + 1;
        }
        ReconstructMVPredictorTopBlock(iListNum, iColumnNum, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iColumnNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iColumnNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iColumnNum + 0] = mv;
        pMV[iColumnNum + 4] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8Top(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8Left(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorLeftBlockFewCheckRef(iListNum, 2, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[8].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[8].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[8] = mv;
        pMV[12] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8LeftFewCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8Internal(int32_t iListNum,
                                                                int32_t iBlockNum,
                                                                int32_t iAddrC)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlock(iListNum, iBlockNum, iAddrC, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iBlockNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iBlockNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iBlockNum + 0] = mv;
        pMV[iBlockNum + 4] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x8Internal(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4External(int32_t iListNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderBlockLocation mbAddrC;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
    mbAddrC.block_num += 1;
    if (false == m_isMBAFF)
        ReconstructMVPredictorExternalBlock(iListNum, mbAddrC, &mvPred);
    else
        ReconstructMVPredictorExternalBlockMBAFF(iListNum, mbAddrC, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[0].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[0].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[0] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4External(int32_t iListNum)

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4Top(int32_t iListNum,
                                                           int32_t iColumnNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    {
        H264DecoderBlockLocation mbAddrC;

        if (3 == iColumnNum)
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_right;
        else
        {
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
            mbAddrC.block_num += iColumnNum + 1;
        }
        ReconstructMVPredictorTopBlock(iListNum, iColumnNum, mbAddrC, &mvPred);
    }

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iColumnNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iColumnNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iColumnNum] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4Top(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4Left(int32_t iListNum,
                                                            int32_t iRowNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorLeftBlockFewCheckRef(iListNum, iRowNum, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iRowNum * 4].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iRowNum * 4].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iRowNum * 4] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4LeftFewCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4Internal(int32_t iListNum,
                                                                int32_t iBlockNum,
                                                                int32_t iAddrC)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlock(iListNum, iBlockNum, iAddrC, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iBlockNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iBlockNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iBlockNum] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4Internal(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4InternalFewCheckRef(int32_t iListNum,
                                                                           int32_t iBlockNum,
                                                                           int32_t iAddrC)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlockFewCheckRef(iListNum, iBlockNum, iAddrC, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iBlockNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iBlockNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iBlockNum] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4InternalFewCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4InternalNoCheckRef(int32_t iListNum,
                                                                          int32_t iBlockNum)
{
    H264DecoderMotionVector mvPred;
    H264DecoderMotionVector *pMV = m_cur_mb.MVs[iListNum]->MotionVectors;
    H264DecoderMotionVector *pMVd = m_cur_mb.MVDelta[iListNum]->MotionVectors;

    // get the prediction
    ReconstructMVPredictorInternalBlockNoCheckRef(iListNum, iBlockNum, &mvPred);

#ifdef __ICL
    __assume_aligned(pMV, 16);
    __assume_aligned(pMVd, 16);
#endif // __ICL

    {
        H264DecoderMotionVector mv;

        // calculate new vector
        mv.mvx = (int16_t) (mvPred.mvx + pMVd[iBlockNum].mvx);
        mv.mvy = (int16_t) (mvPred.mvy + pMVd[iBlockNum].mvy);

        if (mv.mvy > m_MVDistortion[iListNum])
            m_MVDistortion[iListNum] = mv.mvy;

        pMV[iBlockNum] = mv;
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVs4x4InternalNoCheckRef(int32_t iListNum,

#define MEDIAN_OF_3(a, b, c) \
    (int16_t) ((MFX_MIN((a),(b))) ^ (MFX_MIN((b),(c))) ^ (MFX_MIN((c),(a))))

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorExternalBlock(int32_t iListNum,
                                                                          const H264DecoderBlockLocation &mbAddr,
                                                                          H264DecoderMotionVector *pPred)
{
    const H264DecoderBlockLocation * mbAddrC = &mbAddr;
    // get neighbouring addresses
    const H264DecoderBlockLocation & mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[0];
    const H264DecoderBlockLocation & mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;
    if (-1 == mbAddrC->mb_num)
        mbAddrC = &m_cur_mb.CurrentBlockNeighbours.mb_above_left;

    // when B & C partition is not available, use only A partition
    if ((-1 == (mbAddrB.mb_num & mbAddrC->mb_num)) &&
        (-1 != mbAddrA.mb_num))
    {
        *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
        return;
    }
    else
    {
        // if one and only on of the reference indixes is equal to
        // the current reference index, use owning it partition
        {
            int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 0);
            int32_t refIdxA, refIdxB, refIdxC;
            int32_t iEqual;

            refIdxA = (-1 != mbAddrA.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num)) : (-1);
            refIdxB = (-1 != mbAddrB.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num)) : (-1);
            refIdxC = (-1 != mbAddrC->mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrC->mb_num, mbAddrC->block_num)) : (-1);

            iEqual = 0;
            iEqual += (curRefIdx == refIdxA) ? (1) : (0);
            iEqual += (curRefIdx == refIdxB) ? (1) : (0);
            iEqual += (curRefIdx == refIdxC) ? (1) : (0);

            if (1 == iEqual)
            {
                if (curRefIdx == refIdxA)
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                else if (curRefIdx == refIdxB)
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num);
                else
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrC->mb_num, mbAddrC->block_num);

                return;
            }
        }

        // there is true median of A, B and C partition
        {
            H264DecoderMotionVector mvA, mvB, mvC;

            {
                mvA = (-1 != mbAddrA.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num) : zeroVector;
                mvB = (-1 != mbAddrB.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num) : zeroVector;
                mvC = (-1 != mbAddrC->mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrC->mb_num, mbAddrC->block_num) : zeroVector;
            }

            pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
            pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorExternalBlock(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorExternalBlockMBAFF(int32_t iListNum,
                                                                               H264DecoderBlockLocation mbAddrC,
                                                                               H264DecoderMotionVector *pPred)
{
    H264DecoderBlockLocation mbAddrA, mbAddrB;

    // get neighbouring addresses
    mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[0];
    mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;
    if (-1 == mbAddrC.mb_num)
        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_left;

    // when B & C partition is not available, use only A partition
    if ((-1 == (mbAddrB.mb_num & mbAddrC.mb_num)) &&
        (-1 != mbAddrA.mb_num))
    {
        *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);

        // adjust vertical vector
        pPred->mvy <<= GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrA.mb_num]);
        if (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
            pPred->mvy /= 2;

        return;
    }
    else
    {
        // if one and only on of the reference indixes is equal to
        // the current reference index, use owning it partition
        {
            int32_t curFldFlag = pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo);
            int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 0);
            int32_t refIdxA, refIdxB, refIdxC;
            int32_t iEqual;

            refIdxA = (-1 != mbAddrA.mb_num) ?
                      ((GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num) << curFldFlag) >> GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrA.mb_num])) :
                      (-1);
            refIdxB = (-1 != mbAddrB.mb_num) ?
                      ((GetReferenceIndex(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num) << curFldFlag) >> GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrB.mb_num])) :
                      (-1);
            refIdxC = (-1 != mbAddrC.mb_num) ?
                      ((GetReferenceIndex(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num) << curFldFlag) >> GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrC.mb_num])) :
                      (-1);

            iEqual = 0;
            iEqual += (curRefIdx == refIdxA) ? (1) : (0);
            iEqual += (curRefIdx == refIdxB) ? (1) : (0);
            iEqual += (curRefIdx == refIdxC) ? (1) : (0);

            if (1 == iEqual)
            {
                H264DecoderBlockLocation mbNeighbour;

                if (curRefIdx == refIdxA)
                    mbNeighbour = mbAddrA;
                else if (curRefIdx == refIdxB)
                    mbNeighbour = mbAddrB;
                else
                    mbNeighbour = mbAddrC;

                *pPred = GetMV(m_gmbinfo, iListNum, mbNeighbour.mb_num, mbNeighbour.block_num);

                // adjust vertical vector
                pPred->mvy <<= GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbNeighbour.mb_num]);
                if (curFldFlag)
                    pPred->mvy /= 2;

                return;
            }
        }

        // there is true median of A, B and C partition
        {
            H264DecoderMotionVector mvA, mvB, mvC;

            {
                if (0 <= mbAddrA.mb_num)
                {
                    mvA = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                    mvA.mvy <<= GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrA.mb_num]);
                }
                else
                    mvA = zeroVector;

                if (0 <= mbAddrB.mb_num)
                {
                    mvB = GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num);
                    mvB.mvy <<= GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrB.mb_num]);
                }
                else
                    mvB = zeroVector;

                if (0 <= mbAddrC.mb_num)
                {
                    mvC = GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num);
                    mvC.mvy <<= GetMBFieldDecodingFlag(m_gmbinfo->mbs[mbAddrC.mb_num]);
                }
                else
                    mvC = zeroVector;
            }

            pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
            pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);

            // adjust vertical vector
            if (pGetMBFieldDecodingFlag(m_cur_mb.GlobalMacroblockInfo))
                pPred->mvy /= 2;
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorExternalBlockMBAFF(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorTopBlock(int32_t iListNum,
                                                                     int32_t iColumnNum,
                                                                     H264DecoderBlockLocation mbAddrC,
                                                                     H264DecoderMotionVector *pPred)
{
    H264DecoderBlockLocation mbAddrB;

    // get neighbouring addresses
    mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;
    mbAddrB.block_num += iColumnNum;
    if (-1 == mbAddrC.mb_num)
    {
        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
        mbAddrC.block_num += iColumnNum - 1;
    }

    // when B & C partition is not available, use only A partition
    if (-1 == (mbAddrB.mb_num & mbAddrC.mb_num))
    {
        *pPred = m_cur_mb.GetMV(iListNum, iColumnNum - 1);
        return;
    }
    else
    {
        // if one and only on of the reference indixes is equal to
        // the current reference index, use owning it partition
        {
            int32_t curRefIdx = m_cur_mb.GetReferenceIndex(iListNum, iColumnNum);
            int32_t refIdxA, refIdxB, refIdxC;
            int32_t iEqual;

            refIdxA = m_cur_mb.GetReferenceIndex(iListNum, iColumnNum - 1);
            refIdxB = (-1 != mbAddrB.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num)) : (-1);
            refIdxC = (-1 != mbAddrC.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num)) : (-1);

            iEqual = 0;
            iEqual += (curRefIdx == refIdxA) ? (1) : (0);
            iEqual += (curRefIdx == refIdxB) ? (1) : (0);
            iEqual += (curRefIdx == refIdxC) ? (1) : (0);

            if (1 == iEqual)
            {
                if (curRefIdx == refIdxA)
                    *pPred = m_cur_mb.GetMV(iListNum, iColumnNum - 1);
                else if (curRefIdx == refIdxB)
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num);
                else
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num);

                return;
            }
        }

        // there is true median of A, B and C partition
        {
            H264DecoderMotionVector mvA, mvB, mvC;

            {
                mvA = m_cur_mb.GetMV(iListNum, iColumnNum - 1);
                mvB = (-1 != mbAddrB.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num) : (zeroVector);
                mvC = (-1 != mbAddrC.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num) : (zeroVector);
            }

            pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
            pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorTopBlock(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorLeftBlock(int32_t iListNum,
                                                                      int32_t iRowNum,
                                                                      H264DecoderBlockLocation mbAddrC,
                                                                      H264DecoderMotionVector *pPred)
{
    H264DecoderBlockLocation mbAddrA;

    // get neighbouring addresses
    mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[iRowNum];
    if (-1 == mbAddrC.mb_num)
        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mbs_left[iRowNum - 1];

    // if one and only on of the reference indixes is equal to
    // the current reference index, use owning it partition
    {
        int32_t curRefIdx = m_cur_mb.GetReferenceIndex(iListNum, iRowNum * 4);
        int32_t refIdxA, refIdxB, refIdxC;
        int32_t iEqual;

        refIdxA = (-1 != mbAddrA.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num)) : (-1);
        refIdxB = m_cur_mb.GetReferenceIndex(iListNum, iRowNum * 4 - 4);
        refIdxC = (-1 != mbAddrC.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num)) : (-1);

        iEqual = 0;
        iEqual += (curRefIdx == refIdxA) ? (1) : (0);
        iEqual += (curRefIdx == refIdxB) ? (1) : (0);
        iEqual += (curRefIdx == refIdxC) ? (1) : (0);

        if (1 == iEqual)
        {
            if (curRefIdx == refIdxA)
                *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
            else if (curRefIdx == refIdxB)
                *pPred = GetMV(m_gmbinfo, iListNum, m_CurMBAddr, iRowNum * 4 - 4);
            else
                *pPred = GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num);
            return;
        }
    }

    // there is true median of A, B and C partition
    {
        H264DecoderMotionVector mvA, mvB, mvC;

        {
            mvA = (-1 != mbAddrA.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num) : (zeroVector);
            mvB = m_cur_mb.GetMV(iListNum, iRowNum * 4 - 4);
            mvC = (-1 != mbAddrC.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num) : (zeroVector);
        }

        pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
        pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorLeftBlock(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorLeftBlockFewCheckRef(int32_t iListNum,
                                                                                 int32_t iRowNum,
                                                                                 H264DecoderMotionVector *pPred)
{
    H264DecoderBlockLocation mbAddrA;

    // get neighbouring addresses
    mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[iRowNum];

    // if one and only on of the reference indixes is equal to
    // the current reference index, use owning it partition
    {
        int32_t curRefIdx = m_cur_mb.GetReferenceIndex(iListNum, iRowNum * 4);
        int32_t refIdxA, refIdxB;

        refIdxA = (-1 != mbAddrA.mb_num) ? (GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num)) : (-1);
        refIdxB = m_cur_mb.GetReferenceIndex(iListNum, iRowNum * 4 - 4);

        // we check only A reference for equality, because of
        // both upper references have the same reference
        if ((refIdxA == curRefIdx) &&
            (refIdxB != curRefIdx))
        {
            *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
            return;
        }
    }

    // there is true median of A, B and C partition
    {
        H264DecoderMotionVector mvA, mvB, mvC;

        {
            mvA = (-1 != mbAddrA.mb_num) ? GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num) : zeroVector;
            mvB = m_cur_mb.GetMV(iListNum, iRowNum * 4 - 4);
            mvC = m_cur_mb.GetMV(iListNum, iRowNum * 4 - 3);
        }

        pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
        pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorLeftBlockFewCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlock(int32_t iListNum,
                                                                          int32_t iBlockNum,
                                                                          int32_t iAddrC,
                                                                          H264DecoderMotionVector *pPred)
{
    // if one and only on of the reference indixes is equal to
    // the current reference index, use owning it partition
    {
        int32_t curRefIdx = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum);
        int32_t refIdxA, refIdxB, refIdxC;
        int32_t iEqual;

        refIdxA = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum - 1);
        refIdxB = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum - 4);
        refIdxC = m_cur_mb.GetReferenceIndex(iListNum, iAddrC);

        iEqual = 0;
        iEqual += (curRefIdx == refIdxA) ? (1) : (0);
        iEqual += (curRefIdx == refIdxB) ? (1) : (0);
        iEqual += (curRefIdx == refIdxC) ? (1) : (0);

        if (1 == iEqual)
        {
            if (curRefIdx == refIdxA)
                *pPred = m_cur_mb.GetMV(iListNum, iBlockNum - 1);
            else if (curRefIdx == refIdxB)
                *pPred = m_cur_mb.GetMV(iListNum, iBlockNum - 4);
            else
                *pPred = m_cur_mb.GetMV(iListNum, iAddrC);

            return;
        }
    }

    // there is true median of A, B and C partition
    {
        H264DecoderMotionVector  &mvA = m_cur_mb.GetMV(iListNum, iBlockNum - 1);
        H264DecoderMotionVector  &mvB = m_cur_mb.GetMV(iListNum, iBlockNum - 4);
        H264DecoderMotionVector  &mvC = m_cur_mb.GetMV(iListNum, iAddrC);

        pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
        pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlock(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlockFewCheckRef(int32_t iListNum,
                                                                                     int32_t iBlockNum,
                                                                                     int32_t iAddrC,
                                                                                     H264DecoderMotionVector *pPred)
{
    // if one and only on of the reference indixes is equal to
    // the current reference index, use owning it partition
    {
        int32_t curRefIdx = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum);
        int32_t refIdxA, refIdxB;

        refIdxA = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum - 1);
        refIdxB = m_cur_mb.GetReferenceIndex(iListNum, iBlockNum - 4);

        // we check only A reference for equality, because of
        // both upper references have the same reference
        if ((refIdxA == curRefIdx) &&
            (refIdxB != curRefIdx))
        {
            *pPred = m_cur_mb.GetMV(iListNum, iBlockNum - 1);
            return;
        }
    }

    // there is true median of A, B and C partition
    {
        H264DecoderMotionVector &mvA = m_cur_mb.GetMV(iListNum, iBlockNum - 1);
        H264DecoderMotionVector &mvB = m_cur_mb.GetMV(iListNum, iBlockNum - 4);
        H264DecoderMotionVector &mvC = m_cur_mb.GetMV(iListNum, iAddrC);

        pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
        pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlockFewCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlockNoCheckRef(int32_t iListNum,
                                                                                    int32_t iBlockNum,
                                                                                    H264DecoderMotionVector *pPred)
{
    H264DecoderMotionVector &mvA = m_cur_mb.GetMV(iListNum, iBlockNum - 1);
    H264DecoderMotionVector &mvB = m_cur_mb.GetMV(iListNum, iBlockNum - 4);
    H264DecoderMotionVector &mvC = m_cur_mb.GetMV(iListNum, iBlockNum - 5);

    pPred->mvx = MEDIAN_OF_3(mvA.mvx, mvB.mvx, mvC.mvx);
    pPred->mvy = MEDIAN_OF_3(mvA.mvy, mvB.mvy, mvC.mvy);

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictorInternalBlockNoCheckRef(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor16x16(int32_t iListNum,
                                                                  H264DecoderMotionVector *pPred)
{
    ReconstructMVPredictorExternalBlock(iListNum, m_cur_mb.CurrentBlockNeighbours.mb_above_right, pPred);
} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor16x16(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor16x8(int32_t iListNum,
                                                                 int32_t iSubBlockNum,
                                                                 H264DecoderMotionVector *pPred)
{
    // check the fast condition
    if (0 == iSubBlockNum)
    {
        // do special case of prediction
        {
            H264DecoderBlockLocation mbAddrB;

            mbAddrB = m_cur_mb.CurrentBlockNeighbours.mb_above;

            if (-1 != mbAddrB.mb_num)
            {
                int32_t refIdxB = GetReferenceIndex(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num);
                int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 0);

                if (refIdxB == curRefIdx)
                {
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrB.mb_num, mbAddrB.block_num);
                    return;
                }
            }
        }

        // do common median prediction
        ReconstructMVPredictorExternalBlock(iListNum,
                                            m_cur_mb.CurrentBlockNeighbours.mb_above_right,
                                            pPred);
    }
    else
    {
        // do special case of prediction
        {
            H264DecoderBlockLocation mbAddrA;

            mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[2];

            if (-1 != mbAddrA.mb_num)
            {
                int32_t refIdxA = GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 2);

                if (refIdxA == curRefIdx)
                {
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                    return;
                }
            }
        }

        // do common median prediction
        {
            H264DecoderBlockLocation mbAddrC;

            mbAddrC.mb_num = -1;/*m_cur_mb.CurrentBlockNeighbours.mbs_left[1];*/

            ReconstructMVPredictorLeftBlock(iListNum,
                                            2,
                                            mbAddrC,
                                            pPred);
        }
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor16x8(int32_t iListNum,

void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor8x16(int32_t iListNum,
                                                                 int32_t iSubBlockNum,
                                                                 H264DecoderMotionVector *pPred)
{
    // check the fast condition
    if (0 == iSubBlockNum)
    {
        // do special case of prediction
        {
            H264DecoderBlockLocation mbAddrA;

            mbAddrA = m_cur_mb.CurrentBlockNeighbours.mbs_left[0];

            if (-1 != mbAddrA.mb_num)
            {
                int32_t refIdxA = GetReferenceIndex(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 0);

                if (refIdxA == curRefIdx)
                {
                    *pPred = GetMV(m_gmbinfo, iListNum, mbAddrA.mb_num, mbAddrA.block_num);
                    return;
                }
            }
        }

        // do common media prediction
        {

            H264DecoderBlockLocation mbAddrC;

            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
            mbAddrC.block_num += 2;
            ReconstructMVPredictorExternalBlock(iListNum,
                                                mbAddrC,
                                                pPred);
        }
    }
    else
    {
        H264DecoderBlockLocation mbAddrC;

        mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above_right;
        if (-1 == mbAddrC.mb_num)
        {
            mbAddrC = m_cur_mb.CurrentBlockNeighbours.mb_above;
            mbAddrC.block_num += 1;
        }

        if (-1 != mbAddrC.mb_num)
        {
            int32_t refIdxC = GetReferenceIndex(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num);
            int32_t curRefIdx = m_cur_mb.GetRefIdx(iListNum, 1);

            if (refIdxC == curRefIdx)
            {
                *pPred = GetMV(m_gmbinfo, iListNum, mbAddrC.mb_num, mbAddrC.block_num);
                return;
            }
        }

        ReconstructMVPredictorTopBlock(iListNum,
                                       2,
                                       mbAddrC,
                                       pPred);
    }

} // void H264SegmentDecoderMultiThreaded::ReconstructMVPredictor8x16(int32_t iListNum,

} // namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER