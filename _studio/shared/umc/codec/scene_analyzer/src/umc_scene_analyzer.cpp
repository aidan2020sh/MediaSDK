// Copyright (c) 2007-2018 Intel Corporation
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

#if defined (UMC_ENABLE_UMC_SCENE_ANALYZER)

#include "umc_scene_analyzer.h"
#include "umc_video_data_scene_info.h"

#include "ippi.h"

namespace UMC
{

SceneAnalyzer::SceneAnalyzer(void)
{
    m_pPrev = (SceneAnalyzerFrame *) 0;

} // SceneAnalyzer::SceneAnalyzer(void)

SceneAnalyzer::~SceneAnalyzer(void)
{
    // release the object
    Close();

} // SceneAnalyzer::~SceneAnalyzer(void)

Status SceneAnalyzer::Init(BaseCodecParams *pParams)
{
    const SceneAnalyzerParams *pSAParams = DynamicCast<SceneAnalyzerParams> (pParams);
    Status umcRes;

    // check error(s)
    if (NULL == pSAParams)
    {
        return UMC_ERR_NULL_PTR;
    }

    // release the object before initialization
    Close();

    // call the parent's method
    umcRes = SceneAnalyzerP::Init(pParams);
    if (UMC_OK != umcRes)
    {
        return umcRes;
    }

    // allocate being analyzed frames
    m_pPrev = new SceneAnalyzerFrame();
    if (NULL == m_pPrev)
    {
        return UMC_ERR_ALLOC;
    }

    return UMC_OK;

} // Status SceneAnalyzer::Init(BaseCodecParams *pParams)

Status SceneAnalyzer::Close(void)
{
    if (m_pPrev)
    {
        delete m_pPrev;
    }

    m_pPrev = (SceneAnalyzerFrame *) 0;

    // call the parent's method
    SceneAnalyzerP::Close();

    return UMC_OK;

} // Status SceneAnalyzer::Close(void)

Status SceneAnalyzer::GetFrame(MediaData *pSource, MediaData *pDestination)
{
    VideoData *pSrc = DynamicCast<VideoData> (pSource);
    VideoData *pDst = DynamicCast<VideoData> (pDestination);
    FrameType curFrameType = NONE_PICTURE;
    Status umcRes;

    // check error(s)
    if (NULL == pDst)
    {
        return UMC_ERR_NULL_PTR;
    }
    if (NULL == m_pPrev)
    {
        return UMC_ERR_NOT_INITIALIZED;
    }

    // do exchanging the buffers every time,
    // because the last analyzed frame is not a frame to show,
    // especially, when NULL source poiner is passed
    if (B_PICTURE != m_pPrev->m_frameType)
    {
        SceneAnalyzerFrame *pTemp;

        pTemp = m_pRef;
        m_pRef = m_pPrev;
        m_pPrev = m_pCur;
        m_pCur = pTemp;
    }
    else
    {
        SceneAnalyzerFrame *pTemp;

        pTemp = m_pPrev;
        m_pPrev = m_pCur;
        m_pCur = pTemp;
    }

    if (pSrc)
    {
        // if destination has incorrect type
        if (false == CheckSupportedColorFormat(pSrc))
        {
            return SceneAnalyzerBase::GetFrame(pSource, pDestination);
        }

        // initialize the current being analyzed frame
        umcRes = m_pCur->SetSource(pSrc, m_params.m_interlaceType);
        if (UMC_OK != umcRes)
            return umcRes;

        // we do I picture analysis only in the beggining.
        // In other cases we do P picture analysis to track
        // color and deviations. All statistic is reset,
        // when the real scene changing happens.

        // make 1 frame delay
        if (0 == m_frameCount)
        {
            curFrameType = I_PICTURE;

            // perform the analysis
            umcRes = AnalyzeFrame(m_pCur);
            if (UMC_OK != umcRes)
                return umcRes;
            UpdateHistory(m_pCur, I_PICTURE);

            // update the current frame
            m_pCur->m_frameType = I_PICTURE;

            // set the destination video data
            pDst = (VideoData *) 0;
        }
        else
        {
            // get planned frame type
            curFrameType = GetPlannedFrameType();

            // the previous frame is a reference frame.
            // So we should use it as a reference.
            if (B_PICTURE != m_pPrev->m_frameType)
            {
                // perform the analysis
                umcRes = AnalyzeFrame(m_pPrev, m_pCur);
                if (UMC_OK != umcRes)
                    return umcRes;
                UpdateHistory(m_pCur, P_PICTURE);

                // reset counter in case of scene changing
                if (m_pCur->m_bChangeDetected)
                {
                    // reset the history
                    m_framesInHistory = 0;

                    curFrameType = I_PICTURE;
                }

                // reset frame counter.
                // we do it in the separate IF clause,
                // because frame type could be set by the planning function.
                if (I_PICTURE == curFrameType)
                {
                    // reset frame counters
                    m_frameNum = 0;
                    m_bFrameNum = 0;
                }

                // update the current frame
                m_pCur->m_frameType = curFrameType;
            }
            else
            {
                bool bLongChange;
                bool bShortChange;
                InterlaceType longFrameStructure, shortFrameStructure;
                InterlaceType longFrameEstimation, shortFrameEstimation;
                uint32_t longDev, shortDev;

                // perform the analysis
                umcRes = AnalyzeFrame(m_pRef, m_pCur);
                if (UMC_OK != umcRes)
                    return umcRes;
                bLongChange = m_pCur->m_bChangeDetected;
                longFrameStructure = m_pCur->m_frameStructure;
                longFrameEstimation = m_pCur->m_frameEstimation;
                longDev = m_pCur->m_scaledPic.m_info.averageDev[SA_INTER_ESTIMATED];

                // perform the analysis
                umcRes = AnalyzeFrame(m_pPrev, m_pCur);
                if (UMC_OK != umcRes)
                    return umcRes;
                UpdateHistory(m_pCur, P_PICTURE);
                bShortChange = m_pCur->m_bChangeDetected;
                shortFrameStructure = m_pCur->m_frameStructure;
                shortFrameEstimation = m_pCur->m_frameEstimation;
                shortDev = m_pCur->m_scaledPic.m_info.averageDev[SA_INTER_ESTIMATED];

                if (bShortChange)
                {
                    // reset counter in case of scene changing
                    {
                        // reset the history
                        m_framesInHistory = 0;

                        curFrameType = I_PICTURE;
                    }

                    // reset frame counter.
                    // we do it in the separate IF clause,
                    // because frame type could be set by the planning function.
                    {
                        // reset frame counter
                        m_frameNum = 0;
                        m_bFrameNum = 0;
                    }

                    // close the previous GOP
                    m_pPrev->m_frameType = P_PICTURE;

                    // update the current frame
                    m_pCur->m_frameType = curFrameType;
                    m_pCur->m_frameStructure = shortFrameStructure;
                    m_pCur->m_frameEstimation = shortFrameEstimation;
                }
                // the reference and the current frames are
                // to different. It is the time to update
                // the reference
                else if ((bLongChange) || (longDev > shortDev))
                {
                    // reset frame counter
                    m_bFrameNum = 0;

                    // close the previous GOP
                    m_pPrev->m_frameType = P_PICTURE;

                    // update the current frame
                    curFrameType = GetPlannedFrameType();
                    m_pCur->m_frameType = curFrameType;
                    m_pCur->m_frameStructure = shortFrameStructure;
                    m_pCur->m_frameEstimation = shortFrameEstimation;
                }
                else
                {
                    // update the current frame
                    m_pCur->m_frameType = curFrameType;
                    m_pCur->m_frameStructure = longFrameStructure;
                    m_pCur->m_frameEstimation = longFrameEstimation;
                }
            }
        }
    }
    else
    {
        // we need to show the last frame,
        // then we shall return UMC_ERR_NOT_ENOUGH_DATA
        if (0 == m_frameCount)
        {
            pDst = (VideoData *) 0;
        }

        // set frame counter into an invalid value
        m_frameCount = -1;
    }

    if (pDst)
    {
        // set the destination video data
        InitializeVideoData(pDst, m_pPrev);

        // update destination video data
        pDst->SetFrameType(m_pPrev->m_frameType);
        switch (m_pPrev->m_frameEstimation)
        {
        case INTERLEAVED_TOP_FIELD_FIRST:
            pDst->SetPictureStructure(PS_TOP_FIELD_FIRST);
            break;

        case INTERLEAVED_BOTTOM_FIELD_FIRST:
            pDst->SetPictureStructure(PS_BOTTOM_FIELD_FIRST);
            break;

        default:
            pDst->SetPictureStructure(PS_FRAME);
            break;
        }
    }

#if 0
    static int prevFrameNum;
    static int prevBFrameNum;
    static int prevFramesInHistory;

    if (pDst)
    {
        char cStr[256];
        SceneAnalyzerPicture *pPic = &(m_pPrev->m_scaledPic);
        sprintf(cStr, "[% 5d % 5d] %s%s%s %s-%s: intra - % 3d, inter - % 3d, estim - % 3d, color - % 4d, history - % 3d % 5d %s\n",
                prevFrameNum,
                prevBFrameNum,
                (I_PICTURE == pDst->GetFrameType()) ? ("I") : (""),
                (P_PICTURE == pDst->GetFrameType()) ? (" ") : (""),
                (B_PICTURE == pDst->GetFrameType()) ? ("B") : (""),
                (PROGRESSIVE == m_pPrev->m_frameStructure) ? ("frame") : ("FIELD"),
                (PROGRESSIVE == m_pPrev->m_frameEstimation) ? ("frame") : ("FIELD"),
                pPic->m_info.averageDev[SA_INTRA],
                pPic->m_info.averageDev[SA_INTER],
                pPic->m_info.averageDev[SA_INTER_ESTIMATED],
                pPic->m_info.averageDev[SA_COLOR],
                prevFramesInHistory,
                pPic->m_info.bestMatches,
                (I_PICTURE != pDst->GetFrameType()) ? ("") : ("-------------------"));
        OutputDebugString(cStr);
    }

    prevFrameNum = m_frameNum;
    prevBFrameNum = m_bFrameNum;
    prevFramesInHistory = m_framesInHistory;
#endif

    // increment frame counter(s)
    m_frameCount += 1;
    m_frameNum += 1;
    if (B_PICTURE == curFrameType)
    {
        m_bFrameNum += 1;
    }
    else
    {
        m_bFrameNum = 0;
    }

    return (pDst) ? (UMC_OK) : (UMC_ERR_NOT_ENOUGH_DATA);

} // Status SceneAnalyzer::GetFrame(MediaData *pSource, MediaData *pDestination)

} // namespace UMC
#endif
