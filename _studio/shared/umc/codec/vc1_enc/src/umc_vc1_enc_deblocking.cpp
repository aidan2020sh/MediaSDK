// Copyright (c) 2008-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#include "umc_vc1_enc_deblocking.h"
namespace UMC_VC1_ENCODER
{
static uint8_t   curIndexesEx[2][2] = {
    {0,1}, //hor
    {0,2}  //ver
};

static uint8_t   prevIndexesEx[2][2] = {
    {2,3}, //hor
    {1,3}  //ver
};

static uint8_t   curIndexesIn[2][2] = {
    {2,3}, //hor
    {1,3}  //ver
};
static uint8_t   prevIndexesIn[2][2] = {
    {0,1}, //hor
    {0,2}  //ver
};
void GetExternalEdge4MV_VST(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     uint8_t nCurr;
     uint8_t nPrev;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;

     nCurr = curIndexesEx [bVer][0];
     nPrev = prevIndexesEx[bVer][0];

     //------------------ luma edge --------------------------------------------------------------------//
     if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
     {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV(&mvCurr,nCurr);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_2;
        }
     }
     nCurr = curIndexesEx [bVer][1];
     nPrev = prevIndexesEx[bVer][1];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV (&mvCurr,nCurr);
        if  (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_4;
        }
    }
    //------------------- chroma edge ---------------------------------------------------------------------//

    if(!pPred->isIntra() && !pCur->isIntra())
    {
        pPred->GetMV(&mvPrev,4);
        pCur ->GetMV(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,curIndexesEx [bVer][1]) && !pPred->isCoded(4,prevIndexesEx[bVer][1]))
                UFlag |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,curIndexesEx [bVer][1]) && !pPred->isCoded(5,prevIndexesEx[bVer][1]))
                VFlag |= IPPVC_EDGE_HALF_2;
        }
    }
    return;
}

void GetExternalEdge4MV_VST_Field(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0,0}, mvPrev = {0,0,0};
     bool bSecond = true;

     uint8_t nCurr;
     uint8_t nPrev;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;

     nCurr = curIndexesEx [bVer][0];
     nPrev = prevIndexesEx[bVer][0];

     //------------------ luma edge --------------------------------------------------------------------//
     if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
     {
        pPred->GetMV_F(&mvPrev,nPrev);
        pCur->GetMV_F(&mvCurr,nCurr);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_2;
        }
     }
     nCurr = curIndexesEx [bVer][1];
     nPrev = prevIndexesEx[bVer][1];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV_F(&mvPrev,nPrev);
        pCur->GetMV_F (&mvCurr,nCurr);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if  (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_4;
        }
    }
    //------------------- chroma edge ---------------------------------------------------------------------//

    if(!pPred->isIntra() && !pCur->isIntra())
    {
        pPred->GetMV_F(&mvPrev,4);
        pCur ->GetMV_F(&mvCurr,4);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,curIndexesEx [bVer][1]) && !pPred->isCoded(4,prevIndexesEx[bVer][1]))
                UFlag |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,curIndexesEx [bVer][1]) && !pPred->isCoded(5,prevIndexesEx[bVer][1]))
                VFlag |= IPPVC_EDGE_HALF_2;
        }
    }
    return;
}

void GetExternalEdge4MV_NOVST(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     uint8_t nCurr;
     uint8_t nPrev;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag =  0;

     nCurr = curIndexesEx [bVer][0];
     nPrev = prevIndexesEx[bVer][0];

     //------------------ luma edge --------------------------------------------------------------------//
     if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
     {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV(&mvCurr,nCurr);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_1;
        }
     }
     nCurr = curIndexesEx [bVer][1];
     nPrev = prevIndexesEx[bVer][1];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV (&mvCurr,nCurr);
        if  (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_HALF_2;
        }
    }
    //------------------- chroma edge ---------------------------------------------------------------------//

    if(!pPred->isIntra() && !pCur->isIntra())
    {
        pPred->GetMV(&mvPrev,4);
        pCur ->GetMV(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag = IPPVC_EDGE_ALL;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag = IPPVC_EDGE_ALL;
        }
    }
    return;
}
void GetExternalEdge4MV_NOVST_Field(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0,0}, mvPrev = {0,0,0};
     bool bSecond = true;

     uint8_t nCurr;
     uint8_t nPrev;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag =  0;

     nCurr = curIndexesEx [bVer][0];
     nPrev = prevIndexesEx[bVer][0];

     //------------------ luma edge --------------------------------------------------------------------//
     if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
     {
        pPred->GetMV_F(&mvPrev,nPrev);
        pCur->GetMV_F(&mvCurr,nCurr);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][0]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_1;
        }
     }
     nCurr = curIndexesEx [bVer][1];
     nPrev = prevIndexesEx[bVer][1];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV_F(&mvPrev,nPrev);
        pCur->GetMV_F (&mvCurr,nCurr);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if  (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(nCurr,curIndexesEx [bVer][1]) && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_HALF_2;
        }
    }
    //------------------- chroma edge ---------------------------------------------------------------------//

    if(!pPred->isIntra() && !pCur->isIntra())
    {
        pPred->GetMV_F(&mvPrev,4);
        pCur ->GetMV_F(&mvCurr,4);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag = IPPVC_EDGE_ALL;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag = IPPVC_EDGE_ALL;
        }
    }
    return;
}
void GetExternalEdge1MV_VST(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;

     if (!pPred->isIntra() && !pCur->isIntra())
     {
        pPred->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,0);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][1]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_2;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][1]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_4;
        }

        pPred->GetMV(&mvPrev,4);
        pCur ->GetMV(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,curIndexesEx [bVer][1]) && !pPred->isCoded(4,prevIndexesEx[bVer][1]))
                UFlag |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,curIndexesEx [bVer][1]) && !pPred->isCoded(5,prevIndexesEx[bVer][1]))
                VFlag |= IPPVC_EDGE_HALF_2;
        }
     }
    return;
}

void GetExternalEdge1MV_VST_Field(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0,0}, mvPrev = {0,0,0};
     bool bSecond = false;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;


     if (!pPred->isIntra() && !pCur->isIntra())
     {
        pPred->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,0);
        
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][1]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_2;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][1]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][1]))
                YFlag |= IPPVC_EDGE_QUARTER_4;
        }

        pPred->GetMV(&mvPrev,4);
        pCur ->GetMV(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,curIndexesEx [bVer][1]) && !pPred->isCoded(4,prevIndexesEx[bVer][1]))
                UFlag |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,curIndexesEx [bVer][1]) && !pPred->isCoded(5,prevIndexesEx[bVer][1]))
                VFlag |= IPPVC_EDGE_HALF_2;
        }
     }
    return;
}

void GetExternalEdge1MV_VST_SM(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};
     bool coded  = true;
     bool VST4x4 = true;
     uint8_t nCurr;
     uint8_t nPrev;

     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }

     YFlag = UFlag = VFlag = 0;

     if(pPred->isIntra() || pCur->isIntra())
         return;

    pPred->GetMV(&mvPrev,0);
    pCur->GetMV(&mvCurr,0);

    if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
    {
        nCurr = curIndexesEx [bVer][0];
        nPrev = prevIndexesEx [bVer][0];
        VST4x4 = (pCur->GetBlkVSTType(nCurr) == VC1_ENC_4x4_TRANSFORM)
            || (pPred->GetBlkVSTType(nPrev) == VC1_ENC_4x4_TRANSFORM);

        coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][0])
            && !pPred->isCoded(nPrev,prevIndexesEx [bVer][0]);

        if (coded && !VST4x4)
            YFlag |= IPPVC_EDGE_QUARTER_1;

        coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][1])
            && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]);

        if (coded && !VST4x4)
            YFlag |= IPPVC_EDGE_QUARTER_2;

        nCurr = curIndexesEx [bVer][1];
        nPrev = prevIndexesEx [bVer][1];

        VST4x4 = (pCur->GetBlkVSTType(nCurr) == VC1_ENC_4x4_TRANSFORM)
            || (pPred->GetBlkVSTType(nPrev) == VC1_ENC_4x4_TRANSFORM);

        coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][0])
            && !pPred->isCoded(nPrev,prevIndexesEx[bVer][0]);

        if (coded && !VST4x4)
            YFlag |= IPPVC_EDGE_QUARTER_3;

        coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][1])
            && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]);
        if (coded && !VST4x4)
            YFlag |= IPPVC_EDGE_QUARTER_4;
    }

    pPred->GetMV(&mvPrev,4);
    pCur ->GetMV(&mvCurr,4);
    if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
    {
        VST4x4 = (pCur->GetBlkVSTType(4) == VC1_ENC_4x4_TRANSFORM)
            || (pPred->GetBlkVSTType(4) == VC1_ENC_4x4_TRANSFORM);

        coded = !pCur->isCoded(4,curIndexesEx [bVer][0])
            && !pPred->isCoded(4,prevIndexesEx[bVer][0]);

        if (coded && !VST4x4)
            UFlag |= IPPVC_EDGE_HALF_1;

        coded = !pCur->isCoded(4,curIndexesEx [bVer][1])
            && !pPred->isCoded(4,prevIndexesEx[bVer][1]);

        if (coded && !VST4x4)
            UFlag |= IPPVC_EDGE_HALF_2;

        VST4x4 = (pCur->GetBlkVSTType(5) == VC1_ENC_4x4_TRANSFORM)
            || (pPred->GetBlkVSTType(5) == VC1_ENC_4x4_TRANSFORM);

        coded = !pCur->isCoded(5,curIndexesEx [bVer][0])
            && !pPred->isCoded(5,prevIndexesEx[bVer][0]);

        if (coded && !VST4x4)
            VFlag |= IPPVC_EDGE_HALF_1;

        coded = !pCur->isCoded(5,curIndexesEx [bVer][1])
            && !pPred->isCoded(5,prevIndexesEx[bVer][1]);

        if (coded && !VST4x4)
            VFlag |= IPPVC_EDGE_HALF_2;
    }

    return;
}

void GetExternalEdge1MV_NOVST(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;

     if (!pPred->isIntra() && !pCur->isIntra())
     {
        pPred->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,0);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_2;
        }

        pPred->GetMV(&mvPrev,4);
        pCur->GetMV(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag     = IPPVC_EDGE_ALL;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag     = IPPVC_EDGE_ALL;
        }
     }
    return;
}

void GetExternalEdge1MV_NOVST_Field(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};
     bool bSecond = false;
     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;

     if (!pPred->isIntra() && !pCur->isIntra())
     {
        pPred->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,0);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(curIndexesEx [bVer][0],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][0],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(curIndexesEx [bVer][1],curIndexesEx [bVer][0]) && !pPred->isCoded(prevIndexesEx[bVer][1],prevIndexesEx[bVer][0]))
                YFlag |= IPPVC_EDGE_HALF_2;
        }

        pPred->GetMV_F(&mvPrev,4);
        pCur->GetMV_F(&mvCurr,4);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(4,curIndexesEx [bVer][0]) && !pPred->isCoded(4,prevIndexesEx[bVer][0]))
                UFlag     = IPPVC_EDGE_ALL;
            if (!pCur->isCoded(5,curIndexesEx [bVer][0]) && !pPred->isCoded(5,prevIndexesEx[bVer][0]))
                VFlag     = IPPVC_EDGE_ALL;
        }
     }
    return;
}

void GetExternalEdge4MV_VST_SM(VC1EncoderMBInfo *pPred, VC1EncoderMBInfo *pCur,bool bVer, uint8_t& YFlag, uint8_t& UFlag, uint8_t& VFlag)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     uint8_t nCurr;
     uint8_t nPrev;
     bool coded  = true;
     bool VST4x4 = true;

     if (!pPred )
     {
        YFlag  = UFlag  = VFlag  = IPPVC_EDGE_ALL;
        return;
     }
     YFlag = UFlag = VFlag = 0;


     //------------------ luma edge --------------------------------------------------------------------//
    nCurr = curIndexesEx [bVer][0];
    nPrev = prevIndexesEx[bVer][0];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV(&mvCurr,nCurr);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(nCurr) == VC1_ENC_4x4_TRANSFORM)
                || (pPred->GetBlkVSTType(nPrev) == VC1_ENC_4x4_TRANSFORM);


            coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][0])
                && !pPred->isCoded(nPrev,prevIndexesEx [bVer][0]);

            if (coded && !VST4x4)
                YFlag |= IPPVC_EDGE_QUARTER_1;

            coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][1])
                && !pPred->isCoded(nPrev,prevIndexesEx[bVer][1]);

            if (coded && !VST4x4)
                YFlag |= IPPVC_EDGE_QUARTER_2;
        }
    }

    nCurr = curIndexesEx [bVer][1];
    nPrev = prevIndexesEx[bVer][1];

    if (!pPred->isIntra(nPrev) && !pCur->isIntra(nCurr))
    {
        pPred->GetMV(&mvPrev,nPrev);
        pCur->GetMV (&mvCurr,nCurr);

        if  (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(nCurr) == VC1_ENC_4x4_TRANSFORM)
                || (pPred->GetBlkVSTType(nPrev) == VC1_ENC_4x4_TRANSFORM);

            coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][0])
                && !pPred->isCoded(nPrev,prevIndexesEx [bVer][0]);

            if (coded && !VST4x4)
                YFlag |= IPPVC_EDGE_QUARTER_3;

            coded = !pCur->isCoded(nCurr,curIndexesEx [bVer][1])
                && !pPred->isCoded(nPrev,prevIndexesEx [bVer][1]);

            if (coded && !VST4x4)
                YFlag |= IPPVC_EDGE_QUARTER_4;
        }
    }

    //------------------- chroma edge ---------------------------------------------------------------------//

    if(!pPred->isIntra() && !pCur->isIntra())
    {
        pPred->GetMV(&mvPrev,4);
        pCur ->GetMV(&mvCurr,4);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(4) == VC1_ENC_4x4_TRANSFORM)
                || (pPred->GetBlkVSTType(4) == VC1_ENC_4x4_TRANSFORM);

            coded = !pCur->isCoded(4,curIndexesEx [bVer][0])
                    && !pPred->isCoded(4,prevIndexesEx[bVer][0]);

            if (coded && !VST4x4)
                UFlag |= IPPVC_EDGE_HALF_1;

            coded = !pCur->isCoded(4,curIndexesEx [bVer][1])
                    && !pPred->isCoded(4,prevIndexesEx[bVer][1]);

            if (coded && !VST4x4)
                UFlag |= IPPVC_EDGE_HALF_2;

            coded = !pCur->isCoded(5,curIndexesEx [bVer][0])
                    && !pPred->isCoded(5,prevIndexesEx[bVer][0]);

            VST4x4 = (pCur->GetBlkVSTType(5) == VC1_ENC_4x4_TRANSFORM)
                || (pPred->GetBlkVSTType(5) == VC1_ENC_4x4_TRANSFORM);

            if (coded && !VST4x4)
                VFlag |= IPPVC_EDGE_HALF_1;

            coded = !pCur->isCoded(5,curIndexesEx [bVer][1])
                    && !pPred->isCoded(5,prevIndexesEx[bVer][1]);

            if (coded && !VST4x4)
                VFlag |= IPPVC_EDGE_HALF_2;
        }
     }
    return;
}
//======================================Internal=================================//
//====================================only for luma==============================//

void GetInternalEdge4MV_VST(VC1EncoderMBInfo *pCur, uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;

     if (!pCur->isIntra(0) && !pCur->isIntra(1))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,1);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
                YFlagV |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(0,3) && !pCur->isCoded(1,2))
                YFlagV |= IPPVC_EDGE_QUARTER_2;
        }
     }
     if (!pCur->isIntra(2) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,2);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
                YFlagV |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(2,3) && !pCur->isCoded(3,2))
                YFlagV |= IPPVC_EDGE_QUARTER_4;
        }
     }
     if (!pCur->isIntra(0) && !pCur->isIntra(2))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,2);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
                YFlagH |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(0,3) && !pCur->isCoded(2,1))
                YFlagH |= IPPVC_EDGE_QUARTER_2;
        }
     }
     if (!pCur->isIntra(1) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,1);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
                YFlagH |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(1,3) && !pCur->isCoded(3,1))
                YFlagH |= IPPVC_EDGE_QUARTER_4;
        }
     }
    return;
}

void GetInternalEdge4MV_VST_Field(VC1EncoderMBInfo *pCur, uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;

     bool bSecond = true;

     if (!pCur->isIntra(0) && !pCur->isIntra(1))
     {
        pCur->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,1);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
                YFlagV |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(0,3) && !pCur->isCoded(1,2))
                YFlagV |= IPPVC_EDGE_QUARTER_2;
        }
     }
     if (!pCur->isIntra(2) && !pCur->isIntra(3))
     {
        pCur->GetMV_F(&mvPrev,2);
        pCur->GetMV_F(&mvCurr,3);

        bSecond = (mvPrev.bSecond == mvCurr.bSecond);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
                YFlagV |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(2,3) && !pCur->isCoded(3,2))
                YFlagV |= IPPVC_EDGE_QUARTER_4;
        }
     }
     if (!pCur->isIntra(0) && !pCur->isIntra(2))
     {
        pCur->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,2);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
                YFlagH |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(0,3) && !pCur->isCoded(2,1))
                YFlagH |= IPPVC_EDGE_QUARTER_2;
        }
     }
     if (!pCur->isIntra(1) && !pCur->isIntra(3))
     {
        pCur->GetMV_F(&mvPrev,1);
        pCur->GetMV_F(&mvCurr,3);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
                YFlagH |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(1,3) && !pCur->isCoded(3,1))
                YFlagH |= IPPVC_EDGE_QUARTER_4;
        }
     }
    return;
}

void GetInternalEdge4MV_NOVST( VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;

     if (!pCur->isIntra(0) && !pCur->isIntra(1))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,1);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
                YFlagV |= IPPVC_EDGE_HALF_1;
            }
     }
     if (!pCur->isIntra(2) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,2);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
                YFlagV |= IPPVC_EDGE_HALF_2;
        }
     }
     if (!pCur->isIntra(0) && !pCur->isIntra(2))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,2);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
                YFlagH |= IPPVC_EDGE_HALF_1;

        }
     }
     if (!pCur->isIntra(1) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,1);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
                YFlagH |= IPPVC_EDGE_HALF_2;
        }
     }

    return;
}

void GetInternalEdge4MV_NOVST_Field( VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;
     bool bSecond = true;

     if (!pCur->isIntra(0) && !pCur->isIntra(1))
     {
        pCur->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,1);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
                YFlagV |= IPPVC_EDGE_HALF_1;
            }
     }
     if (!pCur->isIntra(2) && !pCur->isIntra(3))
     {
        pCur->GetMV_F(&mvPrev,2);
        pCur->GetMV_F(&mvCurr,3);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);

        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
                YFlagV |= IPPVC_EDGE_HALF_2;
        }
     }
     if (!pCur->isIntra(0) && !pCur->isIntra(2))
     {
        pCur->GetMV_F(&mvPrev,0);
        pCur->GetMV_F(&mvCurr,2);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);
        
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
                YFlagH |= IPPVC_EDGE_HALF_1;

        }
     }
     if (!pCur->isIntra(1) && !pCur->isIntra(3))
     {
        pCur->GetMV_F(&mvPrev,1);
        pCur->GetMV_F(&mvCurr,3);
        bSecond = (mvPrev.bSecond == mvCurr.bSecond);
        
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y && bSecond)
        {
            if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
                YFlagH |= IPPVC_EDGE_HALF_2;
        }
     }

    return;
}
void GetInternalEdge1MV_VST(VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{

     YFlagV= 0;
     YFlagH= 0;

     if (!pCur->isIntra())
     {
        if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
            YFlagV |= IPPVC_EDGE_QUARTER_1;
        if (!pCur->isCoded(0,3) && !pCur->isCoded(1,2))
            YFlagV |= IPPVC_EDGE_QUARTER_2;
        if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
            YFlagV |= IPPVC_EDGE_QUARTER_3;
        if (!pCur->isCoded(2,3) && !pCur->isCoded(3,2))
            YFlagV |= IPPVC_EDGE_QUARTER_4;
        if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
            YFlagH |= IPPVC_EDGE_QUARTER_1;
        if (!pCur->isCoded(0,3) && !pCur->isCoded(2,1))
            YFlagH |= IPPVC_EDGE_QUARTER_2;
        if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
            YFlagH |= IPPVC_EDGE_QUARTER_3;
        if (!pCur->isCoded(1,3) && !pCur->isCoded(3,1))
            YFlagH |= IPPVC_EDGE_QUARTER_4;
    }
    return;
}
void GetInternalEdge1MV_NOVST(VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     YFlagV= 0;
     YFlagH= 0;

     if (!pCur->isIntra())
     {
        if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
            YFlagV |= IPPVC_EDGE_HALF_1;
        if (!pCur->isCoded(2,1) && !pCur->isCoded(3,0))
            YFlagV |= IPPVC_EDGE_HALF_2;
        if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
            YFlagH |= IPPVC_EDGE_HALF_1;
        if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
            YFlagH |= IPPVC_EDGE_HALF_2;
    }
}


void GetInternalEdge4MV_VST_SM(VC1EncoderMBInfo *pCur, uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;

     bool coded  = true;
     bool VST4x4 = true;

    if (!pCur->isIntra(0) && !pCur->isIntra(1))
    {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,1);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(1) == VC1_ENC_4x4_TRANSFORM)
                    || (pCur->GetBlkVSTType(0)== VC1_ENC_4x4_TRANSFORM);

            coded  = !pCur->isCoded(0,1) && !pCur->isCoded(1,0);

            if (coded && !VST4x4)
                YFlagV |= IPPVC_EDGE_QUARTER_1;

            coded  = !pCur->isCoded(0,3) && !pCur->isCoded(1,2);

            if (coded && !VST4x4)
                YFlagV |= IPPVC_EDGE_QUARTER_2;
        }
    }


    if (!pCur->isIntra(2) && !pCur->isIntra(3))
    {
        //(simple profile exception)
        //instead block 2 used block 1
        pCur->GetMV(&mvPrev,2);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(1) == VC1_ENC_4x4_TRANSFORM)
                    || (pCur->GetBlkVSTType(3)== VC1_ENC_4x4_TRANSFORM);

            coded  = !pCur->isCoded(1,0) && !pCur->isCoded(3,0);

            if (coded && !VST4x4)
                YFlagV |= IPPVC_EDGE_QUARTER_3;

            coded  = !pCur->isCoded(1,2) && !pCur->isCoded(3,2);

            if (coded && !VST4x4)
                YFlagV |= IPPVC_EDGE_QUARTER_4;
        }
    }

    if (!pCur->isIntra(0) && !pCur->isIntra(2))
    {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,2);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(0) == VC1_ENC_4x4_TRANSFORM)
                    || (pCur->GetBlkVSTType(2)== VC1_ENC_4x4_TRANSFORM);

            coded  = !pCur->isCoded(0,2) && !pCur->isCoded(2,0);

            if (coded && !VST4x4)
                YFlagH |= IPPVC_EDGE_QUARTER_1;

            coded  = !pCur->isCoded(0,3) && !pCur->isCoded(2,1);

            if (coded && !VST4x4)
                YFlagH |= IPPVC_EDGE_QUARTER_2;
        }
    }

    if (!pCur->isIntra(1) && !pCur->isIntra(3))
    {
        pCur->GetMV(&mvPrev,1);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            VST4x4 = (pCur->GetBlkVSTType(1) == VC1_ENC_4x4_TRANSFORM)
                    || (pCur->GetBlkVSTType(3)== VC1_ENC_4x4_TRANSFORM);

            coded  = !pCur->isCoded(1,2) && !pCur->isCoded(3,0);

            if (coded && !VST4x4)
                YFlagH |= IPPVC_EDGE_QUARTER_3;

            coded  = !pCur->isCoded(1,3) && !pCur->isCoded(3,1);

            if (coded && !VST4x4)
                YFlagH |= IPPVC_EDGE_QUARTER_4;
        }
    }

    return;
}
void GetInternalEdge4MV_NOVST_SM( VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     sCoordinate    mvCurr    = {0,0}, mvPrev = {0,0};

     YFlagV = 0;
     YFlagH = 0;

     if (!pCur->isIntra(0) && !pCur->isIntra(1))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,1);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
                YFlagV |= IPPVC_EDGE_HALF_1;
            }
     }

     //(simple profile exception)
     //instead block 2 used block 1
     if (!pCur->isIntra(2) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,2);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(1,0) && !pCur->isCoded(3,0))
                YFlagV |= IPPVC_EDGE_HALF_2;
        }
     }
     if (!pCur->isIntra(0) && !pCur->isIntra(2))
     {
        pCur->GetMV(&mvPrev,0);
        pCur->GetMV(&mvCurr,2);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
                YFlagH |= IPPVC_EDGE_HALF_1;

        }
     }
     if (!pCur->isIntra(1) && !pCur->isIntra(3))
     {
        pCur->GetMV(&mvPrev,1);
        pCur->GetMV(&mvCurr,3);
        if (mvPrev.x == mvCurr.x && mvPrev.y == mvCurr.y)
        {
            if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
                YFlagH |= IPPVC_EDGE_HALF_2;
        }
     }

    return;
}

void GetInternalEdge1MV_VST_SM(VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     YFlagV = 0;
     YFlagH = 0;
     bool coded  = true;
     bool VST4x4 = true;

     if (pCur->isIntra())
         return;

    VST4x4 = (pCur->GetBlkVSTType(1) == VC1_ENC_4x4_TRANSFORM)
            || (pCur->GetBlkVSTType(0)== VC1_ENC_4x4_TRANSFORM);

    coded  = !pCur->isCoded(0,1) && !pCur->isCoded(1,0);

    if (coded && !VST4x4)
        YFlagV |= IPPVC_EDGE_QUARTER_1;

    coded  = !pCur->isCoded(0,3) && !pCur->isCoded(1,2);

    if (coded && !VST4x4)
        YFlagV |= IPPVC_EDGE_QUARTER_2;

    //(simple profile exception)
    //instead block 2 used block 1
    VST4x4 = (pCur->GetBlkVSTType(3) == VC1_ENC_4x4_TRANSFORM)
            || (pCur->GetBlkVSTType(1)== VC1_ENC_4x4_TRANSFORM);

    coded = !pCur->isCoded(1,0) && !pCur->isCoded(3,0);

    if (coded && !VST4x4)
        YFlagV |= IPPVC_EDGE_QUARTER_3;

    coded = !pCur->isCoded(1,2) && !pCur->isCoded(3,2);
    if (coded && !VST4x4)
        YFlagV |= IPPVC_EDGE_QUARTER_4;

    VST4x4 = (pCur->GetBlkVSTType(2) == VC1_ENC_4x4_TRANSFORM)
        || (pCur->GetBlkVSTType(0)== VC1_ENC_4x4_TRANSFORM);

        coded = !pCur->isCoded(0,2) && !pCur->isCoded(2,0);

    if (coded && !VST4x4)
        YFlagH |= IPPVC_EDGE_QUARTER_1;

    coded = !pCur->isCoded(0,3) && !pCur->isCoded(2,1);

    if (coded && !VST4x4)
        YFlagH |= IPPVC_EDGE_QUARTER_2;

    VST4x4 = (pCur->GetBlkVSTType(1) == VC1_ENC_4x4_TRANSFORM
        || (pCur->GetBlkVSTType(3)== VC1_ENC_4x4_TRANSFORM));

    coded = !pCur->isCoded(1,2) && !pCur->isCoded(3,0);

    if (coded && !VST4x4)
        YFlagH |= IPPVC_EDGE_QUARTER_3;

    coded = !pCur->isCoded(1,3) && !pCur->isCoded(3,1);

    if (coded && !VST4x4)
        YFlagH |= IPPVC_EDGE_QUARTER_4;

    return;
}
void GetInternalEdge1MV_NOVST_SM(VC1EncoderMBInfo *pCur,uint8_t& YFlagV, uint8_t& YFlagH)
{
     YFlagV= 0;
     YFlagH= 0;

     if (!pCur->isIntra())
     {
        if (!pCur->isCoded(0,1) && !pCur->isCoded(1,0))
            YFlagV |= IPPVC_EDGE_HALF_1;

       //(simple profile exception)
       //instead block 2 used block 1
        if (!pCur->isCoded(1,0) && !pCur->isCoded(3,0))
            YFlagV |= IPPVC_EDGE_HALF_2;

        if (!pCur->isCoded(0,2) && !pCur->isCoded(2,0))
            YFlagH |= IPPVC_EDGE_HALF_1;

        if (!pCur->isCoded(1,2) && !pCur->isCoded(3,0))
            YFlagH |= IPPVC_EDGE_HALF_2;
    }
}


void GetInternalBlockEdge(VC1EncoderMBInfo *pCur,
                          uint8_t& YFlagUp, uint8_t& YFlagBot, uint8_t& UFlagH, uint8_t& VFlagH,
                          uint8_t& YFlagL,  uint8_t& YFlagR,   uint8_t& UFlagV, uint8_t& VFlagV)
{

    YFlagUp = YFlagBot = UFlagH = VFlagH = 0;
    YFlagL  = YFlagR   = UFlagV = VFlagV = 0;

    switch (pCur->GetBlkVSTType(0))
    {
        case VC1_ENC_8x8_TRANSFORM:
            YFlagUp|= IPPVC_EDGE_HALF_1;
            YFlagL |= IPPVC_EDGE_HALF_1;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(0,0) && !pCur->isCoded(0,2))
                YFlagUp |= IPPVC_EDGE_HALF_1;
            YFlagL |= IPPVC_EDGE_HALF_1;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(0,0) && !pCur->isCoded(0,1))
                YFlagL |= IPPVC_EDGE_HALF_1;
            YFlagUp |= IPPVC_EDGE_HALF_1;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(0,0) && !pCur->isCoded(0,2))
                YFlagUp |= IPPVC_EDGE_QUARTER_1;
           if (!pCur->isCoded(0,1) && !pCur->isCoded(0,3))
                YFlagUp |= IPPVC_EDGE_QUARTER_2;
            if (!pCur->isCoded(0,0) && !pCur->isCoded(0,1))
                YFlagL |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(0,2) && !pCur->isCoded(0,3))
                YFlagL |= IPPVC_EDGE_QUARTER_2;
            break;
      default:
          break;
    }

    switch (pCur->GetBlkVSTType(1))
    {
        case VC1_ENC_8x8_TRANSFORM:
            YFlagUp |= IPPVC_EDGE_HALF_2;
            YFlagR |= IPPVC_EDGE_HALF_1;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(1,0) && !pCur->isCoded(1,2))
                YFlagUp |= IPPVC_EDGE_HALF_2;
            YFlagR |= IPPVC_EDGE_HALF_1;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(1,0) && !pCur->isCoded(1,1))
                YFlagR |= IPPVC_EDGE_HALF_1;
            YFlagUp |= IPPVC_EDGE_HALF_2;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(1,0) && !pCur->isCoded(1,2))
                YFlagUp |= IPPVC_EDGE_QUARTER_3;
           if  (!pCur->isCoded(1,1) && !pCur->isCoded(1,3))
                YFlagUp |= IPPVC_EDGE_QUARTER_4;
            if (!pCur->isCoded(1,0) && !pCur->isCoded(1,1))
                YFlagR |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(1,2) && !pCur->isCoded(1,3))
                YFlagR |= IPPVC_EDGE_QUARTER_2;
            break;
      default:
          break;
    }

    switch (pCur->GetBlkVSTType(2))
    {
        case VC1_ENC_8x8_TRANSFORM:
            YFlagBot |= IPPVC_EDGE_HALF_1;
            YFlagL |= IPPVC_EDGE_HALF_2;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(2,0) && !pCur->isCoded(2,2))
                YFlagBot |= IPPVC_EDGE_HALF_1;
            YFlagL |= IPPVC_EDGE_HALF_2;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(2,0) && !pCur->isCoded(2,1))
                YFlagL |= IPPVC_EDGE_HALF_2;
            YFlagBot |= IPPVC_EDGE_HALF_1;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(2,0) && !pCur->isCoded(2,2))
                YFlagBot |= IPPVC_EDGE_QUARTER_1;
            if (!pCur->isCoded(2,1) && !pCur->isCoded(2,3))
                YFlagBot |= IPPVC_EDGE_QUARTER_2;
            if (!pCur->isCoded(2,0) && !pCur->isCoded(2,1))
                YFlagL |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(2,2) && !pCur->isCoded(2,3))
                YFlagL |= IPPVC_EDGE_QUARTER_4;
            break;
      default:
          break;
    }

    switch (pCur->GetBlkVSTType(3))
    {
        case VC1_ENC_8x8_TRANSFORM:
            YFlagBot |= IPPVC_EDGE_HALF_2;
            YFlagR |= IPPVC_EDGE_HALF_2;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(3,0) && !pCur->isCoded(3,2))
                YFlagBot |= IPPVC_EDGE_HALF_2;
            YFlagR |= IPPVC_EDGE_HALF_2;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(3,0) && !pCur->isCoded(3,1))
                YFlagR |= IPPVC_EDGE_HALF_2;
            YFlagBot |= IPPVC_EDGE_HALF_2;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(3,0) && !pCur->isCoded(3,2))
                YFlagBot |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(3,1) && !pCur->isCoded(3,3))
                YFlagBot |= IPPVC_EDGE_QUARTER_4;
            if (!pCur->isCoded(3,0) && !pCur->isCoded(3,1))
                YFlagR |= IPPVC_EDGE_QUARTER_3;
            if (!pCur->isCoded(3,2) && !pCur->isCoded(3,3))
                YFlagR |= IPPVC_EDGE_QUARTER_4;
            break;
      default:
          break;
    }

    switch (pCur->GetBlkVSTType(4))
    {
        case VC1_ENC_8x8_TRANSFORM:
            UFlagH = IPPVC_EDGE_ALL;
            UFlagV = IPPVC_EDGE_ALL;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(4,0) && !pCur->isCoded(4,2))
                UFlagH = IPPVC_EDGE_ALL;
            UFlagV = IPPVC_EDGE_ALL;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(4,0) && !pCur->isCoded(4,1))
                UFlagV = IPPVC_EDGE_ALL;
            UFlagH = IPPVC_EDGE_ALL;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(4,0) && !pCur->isCoded(4,2))
                UFlagH |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,1) && !pCur->isCoded(4,3))
                UFlagH |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(4,0) && !pCur->isCoded(4,1))
                UFlagV |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(4,2) && !pCur->isCoded(4,3))
                UFlagV |= IPPVC_EDGE_HALF_2;
             break;
      default:
          break;
    }

    switch (pCur->GetBlkVSTType(5))
    {
        case VC1_ENC_8x8_TRANSFORM:
            VFlagH = IPPVC_EDGE_ALL;
            VFlagV = IPPVC_EDGE_ALL;
            break;
        case VC1_ENC_8x4_TRANSFORM:
            if (!pCur->isCoded(5,0) && !pCur->isCoded(5,2))
                VFlagH = IPPVC_EDGE_ALL;
            VFlagV = IPPVC_EDGE_ALL;
            break;
       case VC1_ENC_4x8_TRANSFORM:
            if (!pCur->isCoded(5,0) && !pCur->isCoded(5,1))
                VFlagV = IPPVC_EDGE_ALL;
            VFlagH = IPPVC_EDGE_ALL;
            break;
      case VC1_ENC_4x4_TRANSFORM:
            if (!pCur->isCoded(5,0) && !pCur->isCoded(5,2))
                VFlagH |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,1) && !pCur->isCoded(5,3))
                VFlagH |= IPPVC_EDGE_HALF_2;
            if (!pCur->isCoded(5,0) && !pCur->isCoded(5,1))
                VFlagV |= IPPVC_EDGE_HALF_1;
            if (!pCur->isCoded(5,2) && !pCur->isCoded(5,3))
                VFlagV |= IPPVC_EDGE_HALF_2;
            break;
      default:
          break;
    }

    return;
}

//Advance profile progressive mode external edge
fGetExternalEdge GetExternalEdge[2][2] = {//4 MV, VTS
    {GetExternalEdge1MV_NOVST,GetExternalEdge1MV_VST},
    {GetExternalEdge4MV_NOVST,GetExternalEdge4MV_VST}
};

//Advance profile field mode external edge
fGetExternalEdge GetFieldExternalEdge[2][2] = {//4 MV, VTS
    {GetExternalEdge1MV_NOVST_Field,GetExternalEdge1MV_VST_Field},
    {GetExternalEdge4MV_NOVST_Field,GetExternalEdge4MV_VST_Field}
};

//Advance profile internal edge
fGetInternalEdge GetInternalEdge[2][2] = {//4 MV, VTS
    {GetInternalEdge1MV_NOVST,GetInternalEdge1MV_VST},
    {GetInternalEdge4MV_NOVST,GetInternalEdge4MV_VST}
};

//Advance profile internal edge
fGetInternalEdge GetFieldInternalEdge[2][2] = {//4 MV, VTS
    {GetInternalEdge1MV_NOVST,GetInternalEdge1MV_VST},
    {GetInternalEdge4MV_NOVST_Field, GetInternalEdge4MV_VST_Field}
};

//Simple/Main profile external edge
fGetExternalEdge GetExternalEdge_SM[2][2] = {//4 MV, VTS
    {GetExternalEdge1MV_NOVST,GetExternalEdge1MV_VST_SM},
    {GetExternalEdge4MV_NOVST,GetExternalEdge4MV_VST_SM}
};

//Simple/Main profile internal edge
fGetInternalEdge GetInternalEdge_SM[2][2] = {//4 MV, VTS
    {GetInternalEdge1MV_NOVST_SM,GetInternalEdge1MV_VST_SM},
    {GetInternalEdge4MV_NOVST_SM,GetInternalEdge4MV_VST_SM}
};

fDeblock_I_MB Deblk_I_MBFunction_YV12[8] =
    {no_Deblocking_I_MB, no_Deblocking_I_MB,
     Deblock_I_LeftMB_YV12,   Deblock_I_MB_YV12,
     Deblock_I_LeftTopBottomMB_YV12, Deblock_I_TopBottomMB_YV12, Deblock_I_LeftBottomMB_YV12, Deblock_I_BottomMB_YV12
    };

fDeblock_I_MB Deblk_I_MBFunction_NV12[8] =
    {no_Deblocking_I_MB, no_Deblocking_I_MB,
     Deblock_I_LeftMB_NV12,   Deblock_I_MB_NV12,
     Deblock_I_LeftTopBottomMB_NV12, Deblock_I_TopBottomMB_NV12, Deblock_I_LeftBottomMB_NV12, Deblock_I_BottomMB_NV12
    };

fDeblock_P_MB Deblk_P_MBFunction_NoVTS_YV12[16] =
{
    //no variable transform
    //top row
     no_Deblocking_P_MB,           no_Deblocking_P_MB,
     no_Deblocking_P_MB,           no_Deblocking_P_MB,

     //middle row
     Deblock_P_LeftMB_NoVT_YV12,  Deblock_P_MB_NoVT_YV12,
     no_Deblocking_P_MB,          Deblock_P_RightMB_NoVT_YV12,

     //top-bottom case
     Deblock_P_LeftTopBottomMB_NoVT_YV12,  Deblock_P_TopBottomMB_NoVT_YV12,
     no_Deblocking_P_MB,                   Deblock_P_RightTopBottomMB_NoVT_YV12,

     //bottom row
     Deblock_P_LeftBottomMB_NoVT_YV12,  Deblock_P_BottomMB_NoVT_YV12,
     no_Deblocking_P_MB,                Deblock_P_RightBottomMB_NoVT_YV12
};

fDeblock_P_MB Deblk_P_MBFunction_VTS_YV12[16] =
{

     //variable trasform
     //top row
     no_Deblocking_P_MB,           no_Deblocking_P_MB,
     no_Deblocking_P_MB,           no_Deblocking_P_MB,

     //middle row
      Deblock_P_LeftMB_VT_YV12,    Deblock_P_MB_VT_YV12,
      no_Deblocking_P_MB,          Deblock_P_RightMB_VT_YV12,

     //top-bottom case
     Deblock_P_LeftTopBottomMB_VT_YV12,    Deblock_P_TopBottomMB_VT_YV12,
     no_Deblocking_P_MB,                   Deblock_P_RightTopBottomMB_VT_YV12,

     //bottom row
     Deblock_P_LeftBottomMB_VT_YV12,    Deblock_P_BottomMB_VT_YV12,
     no_Deblocking_P_MB,                Deblock_P_RightBottomMB_VT_YV12
};

fDeblock_P_MB Deblk_P_MBFunction_NoVTS_NV12[16] =
{
    //no variable transform
    //top row
     no_Deblocking_P_MB,           no_Deblocking_P_MB,
     no_Deblocking_P_MB,           no_Deblocking_P_MB,

     //middle row
     Deblock_P_LeftMB_NoVT_NV12,   Deblock_P_MB_NoVT_NV12,
     no_Deblocking_P_MB,           Deblock_P_RightMB_NoVT_NV12,

     //top-bottom case
     Deblock_P_LeftTopBottomMB_NoVT_NV12,  Deblock_P_TopBottomMB_NoVT_NV12,
     no_Deblocking_P_MB,                   Deblock_P_RightTopBottomMB_NoVT_NV12,

     //bottom row
     Deblock_P_LeftBottomMB_NoVT_NV12,  Deblock_P_BottomMB_NoVT_NV12,
     no_Deblocking_P_MB,                Deblock_P_RightBottomMB_NoVT_NV12
};

fDeblock_P_MB Deblk_P_MBFunction_VTS_NV12[16] =
{
    //variable trasform
    //top row
     no_Deblocking_P_MB,           no_Deblocking_P_MB,
     no_Deblocking_P_MB,           no_Deblocking_P_MB,

     //middle row
      Deblock_P_LeftMB_VT_NV12,    Deblock_P_MB_VT_NV12,
      no_Deblocking_P_MB,          Deblock_P_RightMB_VT_NV12,

     //top-bottom case
     Deblock_P_LeftTopBottomMB_VT_NV12,    Deblock_P_TopBottomMB_VT_NV12,
     no_Deblocking_P_MB,                   Deblock_P_RightTopBottomMB_VT_NV12,

     //bottom row
     Deblock_P_LeftBottomMB_VT_NV12,    Deblock_P_BottomMB_VT_NV12,
     no_Deblocking_P_MB,                Deblock_P_RightBottomMB_VT_NV12
};


fDeblock_P_MB* Deblk_P_MBFunction_YV12[2] =
{   
    Deblk_P_MBFunction_NoVTS_YV12,
    Deblk_P_MBFunction_VTS_YV12
};

fDeblock_P_MB* Deblk_P_MBFunction_NV12[2] =
{
    Deblk_P_MBFunction_NoVTS_NV12,
    Deblk_P_MBFunction_VTS_NV12
};

}


#endif