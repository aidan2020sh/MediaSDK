// Copyright (c) 2005-2018 Intel Corporation
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

#include "umc_demuxer_defs.h"

#include <ipps.h>

namespace UMC
{

Mpeg2Es::Mpeg2Es(void)
{
    pESDSs = NULL;
    pEsInfo = NULL;
    uiType = 0;
    uiPid = 0;
    Release();
}

Mpeg2Es::~Mpeg2Es(void)
{
    Release();
}

void Mpeg2Es::Release(void)
{
    uiEsInfoLen = 0;
    if (pESDSs)
    {
        delete pESDSs;
        pESDSs = NULL;
    }
    if (pEsInfo)
    {
        ippsFree(pEsInfo);
        pEsInfo = NULL;
    }
}

Ipp8u *Mpeg2Es::SetInfo(Ipp8u *pPtr, Ipp16u uiLen)
{
    if (pEsInfo)
    {
        ippsFree(pEsInfo);
        pEsInfo = NULL;
        uiEsInfoLen = 0;
    }
    if (pPtr && uiLen)
    {
        pEsInfo = ippsMalloc_8u(uiLen);
        if (pEsInfo)
        {
            uiEsInfoLen = uiLen;
            ippsCopy_8u(pPtr, pEsInfo, uiLen);
        }
        else
            Release();
    }
    return pEsInfo;
}

Mpeg2TsPmt::Mpeg2TsPmt(void)
{
    pProgInfo = NULL;
    pESs = NULL;
    uiProgInd = 0;
    uiProgNum = 0;
    uiProgPid = 0;
    Release();
}

Mpeg2TsPmt::~Mpeg2TsPmt(void)
{
    Release();
}

void Mpeg2TsPmt::Release(void)
{
    uiVer = 0;
    uiSecLen = 0;
    uiPcrPid = 0;
    uiProgInfoLen = 0;
    if (pESs)
    {
        for (; uiESs > 0; uiESs--)
            pESs[uiESs - 1].Release();
        delete[] pESs;
        pESs = NULL;
    }
    uiESs = 0;
    if (pProgInfo)
    {
        ippsFree(pProgInfo);
        pProgInfo = NULL;
    }
}

Ipp8u *Mpeg2TsPmt::SetInfo(Ipp8u *pPtr, Ipp16u uiLen)
{
    if (pProgInfo)
    {
        ippsFree(pProgInfo);
        pProgInfo = NULL;
        uiProgInfoLen = 0;
    }
    if (pPtr && uiLen)
    {
        pProgInfo = ippsMalloc_8u(uiLen);
        if (pProgInfo)
        {
            uiProgInfoLen = uiLen;
            ippsCopy_8u(pPtr, pProgInfo, uiLen);
        }
        else
            Release();
    }
    return pProgInfo;
}

Mpeg2TsPat::~Mpeg2TsPat(void)
{
    Release();
}

Mpeg2TsPat::Mpeg2TsPat(void)
{
    pProgs = NULL;
    Release();
}

void Mpeg2TsPat::Release(void)
{
    uiVer = 0;
    uiSecLen = 0;
    uiTsId = 0;
    if (pProgs)
    {
        for (; uiProgs > 0; uiProgs--)
            pProgs[uiProgs - 1].Release();
        delete[] pProgs;
        pProgs = NULL;
    }
    uiProgs = 0;
}

Ipp32s Mpeg2TsPat::GetProgIdx(Ipp32u uiPid)
{
    Ipp32u i;
    for (i = 0; i < uiProgs; i++)
        if (pProgs[i].uiProgPid == uiPid)
            return (Ipp32s)i;
    return -1;
}

ESDescriptor::ESDescriptor(void)
{
    pDecSpecInfo = NULL;
    Release();
}

void ESDescriptor::Release(void)
{
    uiEsId = 0;
    avgBitrate = 0;
    uiPredefinedSLConfig = 0;
    uiDecSpecInfoLen = 0;
    pDecSpecInfo = NULL;
}

TrackRule::TrackRule(void)
{
    m_TypeMask = TRACK_UNKNOWN;
    m_iPID = -1;
    m_iLimit = 0;
    m_bValidAfterInit = false;
}

TrackRule::TrackRule(TrackType mask, Ipp16s iPid, Ipp32s iLimit, bool bValid)
{
    m_TypeMask = mask;
    m_iPID = iPid;
    m_iLimit = iLimit;
    m_bValidAfterInit = bValid;
}

CheckPoint::CheckPoint(void)
{
    dTime = 0.0;
    uiPos = 0;
}

CheckPoint::CheckPoint(Ipp64f time, Ipp64u pos)
{
    dTime = time;
    uiPos = pos;
}

Ipp64f CheckPoint::GetBPS(CheckPoint &rTarget)
{
    return (Ipp64s)(rTarget.uiPos - uiPos) / (rTarget.dTime - dTime);
}

RulesMatchingState::RulesMatchingState(void)
{
    m_uiRules = 0 ;
    m_pRules = NULL;
    m_iMaxTracks = 0;
    m_iMatchedTracks = 0;
    m_pRuleState = NULL;
    m_bAnyRuleValidAfterInit = false;
    ippsZero_8u(m_bIsTriedByRules, MAX_TRACK);
}

void RulesMatchingState::Reset(void)
{
    m_iMatchedTracks = 0;
    ippsZero_32s(m_pRuleState, m_uiRules);
    ippsZero_8u(m_bIsTriedByRules, MAX_TRACK);
}

} // namespace UMC
