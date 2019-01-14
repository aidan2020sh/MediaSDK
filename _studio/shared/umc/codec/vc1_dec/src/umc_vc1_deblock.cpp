// Copyright (c) 2004-2018 Intel Corporation
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

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_dec_seq.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_debug.h"

typedef enum
{
    LFilterSimplePFrame    = 0,
    LFilterMainPFrame      = 1,
    LFilterAdvancePFrame   = 3, /* 2 - VC1_PROFILE_RESERVED */
    LFilterMainPInterFrame = 4
} LFilterType;

static const int32_t deblock_table[16] = {IPPVC_EDGE_ALL ,
                                      IPPVC_EDGE_QUARTER_1 +  IPPVC_EDGE_HALF_2,
                                      IPPVC_EDGE_QUARTER_2 +  IPPVC_EDGE_HALF_2,
                                      IPPVC_EDGE_HALF_2,

                                      IPPVC_EDGE_HALF_1 + IPPVC_EDGE_QUARTER_3,
                                      IPPVC_EDGE_QUARTER_1 +  IPPVC_EDGE_QUARTER_3,
                                      IPPVC_EDGE_QUARTER_2 + IPPVC_EDGE_QUARTER_3,
                                      IPPVC_EDGE_QUARTER_3,

                                      IPPVC_EDGE_HALF_1+ IPPVC_EDGE_QUARTER_4,
                                      IPPVC_EDGE_QUARTER_1+ IPPVC_EDGE_QUARTER_4,
                                      IPPVC_EDGE_QUARTER_2+ IPPVC_EDGE_QUARTER_4,
                                      IPPVC_EDGE_QUARTER_4,

                                      IPPVC_EDGE_HALF_1,
                                      IPPVC_EDGE_QUARTER_1,
                                      IPPVC_EDGE_QUARTER_2,
                                      0};


//static const int32_t deblock_table_2[4] = {IPPVC_EDGE_HALF_1,
//                                       IPPVC_EDGE_QUARTER_1,
//                                       IPPVC_EDGE_QUARTER_2,
//                                       0};

static const int32_t deblock_table_2[4] = {IPPVC_EDGE_ALL,
                                          IPPVC_EDGE_HALF_1,
                                          IPPVC_EDGE_HALF_2,
                                          0};

static void HorizontalDeblockingLumaP(uint8_t* pUUpBlock,
                                      uint32_t Pquant,
                                      int32_t Pitch,
                                      const VC1MB* _pMB,
                                      const VC1MB* _pnMB,
                                      LFilterType _type)
{
    int32_t SBP = 0;

    int32_t count = 0;
    int32_t flag_ext = 0;
    int32_t flag_int = 0;

    int32_t Edge_ext = 3; /* Mark bottom left and right subblocks */
    int32_t Edge_int = 0;

    int32_t SBP_cur_count  = 0;
    int32_t SBP_next_count = 2;
    if (_pMB != _pnMB)
    {
        SBP_next_count = 0;
        SBP_cur_count  = 2;
    }
    for(count = 0; count<2;count++)
    {
        Edge_ext = 3;
        Edge_int = 0;
        SBP = _pMB->m_pBlocks[SBP_cur_count].SBlkPattern;
        if (_pnMB)
        {
            if ((LFilterMainPInterFrame != _type)
                &&(_pMB->m_pBlocks[SBP_cur_count].blkType < VC1_BLK_INTRA_TOP)
                &&(_pnMB->m_pBlocks[SBP_next_count].blkType < VC1_BLK_INTRA_TOP)
                &&(_pMB->m_pBlocks[SBP_cur_count].mv[0][0] == _pnMB->m_pBlocks[SBP_next_count].mv[0][0])
                &&(_pMB->m_pBlocks[SBP_cur_count].mv[0][1] == _pnMB->m_pBlocks[SBP_next_count].mv[0][1])
                )
            {
                int32_t DSBP = _pnMB->m_pBlocks[SBP_next_count].SBlkPattern;
                int32_t TSBP = SBP;
                Edge_ext = TSBP | (DSBP>>2);
                if (LFilterMainPFrame == _type)
                {
                    /* Historical fix for 4x4 blocks */
                    if (_pMB->m_pBlocks[SBP_cur_count].blkType==VC1_BLK_INTER4X4 && TSBP)
                        TSBP = 0xF;
                    if (_pnMB->m_pBlocks[SBP_next_count].blkType==VC1_BLK_INTER4X4 && DSBP)
                        DSBP = 0xF;
                }
                Edge_ext = TSBP | (DSBP>>2);
            }
            flag_ext += ((Edge_ext&3) << (count*2));
        }
        else
            flag_ext = 0;
        if ((_pMB->m_pBlocks[SBP_cur_count].blkType==VC1_BLK_INTER8X4) || (_pMB->m_pBlocks[SBP_cur_count].blkType==VC1_BLK_INTER4X4))
        {
            if ((SBP & 8) || (SBP & 2))
                Edge_int |= 2;
            if ((SBP & 4) || (SBP & 1))
                Edge_int |= 1;
        }
        flag_int += (Edge_int << (count*2));
        ++SBP_next_count;
        ++SBP_cur_count;
    }
    _own_FilterDeblockingLuma_HorEdge_VC1(pUUpBlock + 8*Pitch, Pquant,Pitch, deblock_table[flag_ext]);
    _own_FilterDeblockingLuma_HorEdge_VC1(pUUpBlock + 4*Pitch, Pquant,Pitch, deblock_table[flag_int]);
}

static void HorizontalDeblockingChromaP(uint8_t* pUUpBlock,
                                      uint32_t Pquant,
                                      int32_t Pitch,
                                      VC1Block* _pBlk,
                                      VC1Block* _pnBlk,
                                      LFilterType _type)
{

    int32_t SBP = _pBlk->SBlkPattern;
    int32_t Edge_int = 0;
    int32_t Edge_ext = 3;
    if (_pnBlk)
    {
        if ((LFilterMainPInterFrame != _type)
            &&(_pBlk->blkType < VC1_BLK_INTRA_TOP)
            &&(_pnBlk->blkType < VC1_BLK_INTRA_TOP)
            &&(_pBlk->mv[0][0] == _pnBlk->mv[0][0])
            &&(_pBlk->mv[0][1] == _pnBlk->mv[0][1])
            )
        {
            int32_t DSBP = _pnBlk->SBlkPattern;
            int32_t TSBP = SBP;
            if (LFilterMainPFrame == _type)
            {
                /* Historical fix for 4x4 blocks */
                if (_pBlk->blkType==VC1_BLK_INTER4X4 && TSBP)
                    TSBP = 0xF;
                if (_pnBlk->blkType==VC1_BLK_INTER4X4 && DSBP)
                    DSBP = 0xF;
            }
            Edge_ext = TSBP | (DSBP>>2);
        }
    }
    else
        Edge_ext  = 0;
    if ((_pBlk->blkType==VC1_BLK_INTER8X4) || (_pBlk->blkType==VC1_BLK_INTER4X4))
    {
        if ((SBP & 8) || (SBP & 2))
            Edge_int |= 2;
        if ((SBP & 4) || (SBP & 1))
            Edge_int |= 1;
    }

    _own_FilterDeblockingChroma_HorEdge_VC1(pUUpBlock + 8*Pitch, Pquant,Pitch, deblock_table_2[Edge_ext&3]);
    _own_FilterDeblockingChroma_HorEdge_VC1(pUUpBlock + 4*Pitch, Pquant,Pitch, deblock_table_2[Edge_int]);
}

static void VerticalDeblockingLumaP(uint8_t* pUUpLBlock,
                                    uint32_t Pquant,
                                    int32_t Pitch,
                                    VC1MB* _pMB,
                                    const VC1MB* _pnMB,
                                    LFilterType _type)
{
    int32_t SBP = 0;
    int32_t LSBP = 0;
    VC1Block* pL   = 0;


    int32_t count = 0;
    int32_t flag_ext = 0;
    int32_t flag_int = 0;

    int32_t Edge_ext = 3; /* Mark bottom left and right subblocks */
    int32_t Edge_int = 0;

    int32_t SBP_cur_count  = 0;
    int32_t SBP_next_count = 1;
    if (_pMB != _pnMB)
    {
        SBP_next_count = 0;
        SBP_cur_count  = 1;
    }

    for(count = 0; count<2;count++)
    {
        pL   = &_pMB->m_pBlocks[SBP_cur_count];
        Edge_ext = 3;
        Edge_int = 0;
        SBP = _pMB->m_pBlocks[SBP_cur_count].SBlkPattern;
        LSBP = SBP;
        if (_pnMB)
        {
            if (2 == SBP_cur_count)
            {
                pL   = &_pMB->m_pBlocks[SBP_cur_count-1];
                LSBP = pL->SBlkPattern;
            }
            if ((LFilterMainPInterFrame != _type)
                &&(_pMB->m_pBlocks[SBP_cur_count].blkType  < VC1_BLK_INTRA_TOP)
                &&(_pnMB->m_pBlocks[SBP_next_count].blkType < VC1_BLK_INTRA_TOP)
                &&(_pMB->m_pBlocks[SBP_cur_count].mv[0][0] == _pnMB->m_pBlocks[SBP_next_count].mv[0][0])
                &&(_pMB->m_pBlocks[SBP_cur_count].mv[0][1] == _pnMB->m_pBlocks[SBP_next_count].mv[0][1])
                )
            {
                int32_t RSBP = _pnMB->m_pBlocks[SBP_next_count].SBlkPattern;
                if (LFilterMainPFrame == _type)
                {
                    /* Historical reasons of VC-1 implementation */
                    /* Historical fix for 4x4 blocks */
                    if (pL->blkType==VC1_BLK_INTER4X4 && LSBP)
                        LSBP = 0xF;
                    if (_pnMB->m_pBlocks[SBP_next_count].blkType==VC1_BLK_INTER4X4 && RSBP)
                        RSBP = 0xF;
                }
                Edge_ext = LSBP | (RSBP>>1);
                Edge_ext = ( ((Edge_ext&4)>>1) + (Edge_ext&1));
            }
            flag_ext += ((Edge_ext&3) << (count*2));
        }
        else
            flag_ext = 0;
        if ((_pMB->m_pBlocks[SBP_cur_count].blkType==VC1_BLK_INTER4X8) || (_pMB->m_pBlocks[SBP_cur_count].blkType==VC1_BLK_INTER4X4))
        {
            if ((SBP & 8) || (SBP & 4))
                Edge_int |= 2;
            if ((SBP & 2) || (SBP & 1))
                Edge_int |= 1;
        }
        flag_int += (Edge_int << (count*2));
        SBP_next_count +=2;
        SBP_cur_count +=2;
    }
    _own_FilterDeblockingLuma_VerEdge_VC1(pUUpLBlock + 8, Pquant,Pitch, deblock_table[flag_ext]);
    _own_FilterDeblockingLuma_VerEdge_VC1(pUUpLBlock + 4, Pquant,Pitch, deblock_table[flag_int]);
}

static void VerticalDeblockingChromaP(uint8_t* pUUpLBlock,
                                      uint32_t Pquant,
                                      int32_t Pitch,
                                      VC1Block* _pBlk,
                                      VC1Block* _pnBlk,
                                      LFilterType _type)
{
    int32_t SBP =  _pBlk->SBlkPattern;
    int32_t LSBP = SBP;
    VC1Block* pL   = _pBlk;

    int32_t Edge_ext = 3; /* Mark bottom left and right subblocks */
    int32_t Edge_int = 0;


        if (_pnBlk)
        {
            if ((LFilterMainPInterFrame != _type)
                &&(_pBlk->blkType < VC1_BLK_INTRA_TOP)
                &&(_pnBlk->blkType < VC1_BLK_INTRA_TOP)
                &&(_pBlk->mv[0][0] == _pnBlk->mv[0][0])
                &&(_pBlk->mv[0][1] == _pnBlk->mv[0][1])
            )
            {
                int32_t RSBP = _pnBlk->SBlkPattern;
                if (LFilterMainPFrame == _type)
                {
                    /* Historical reasons of VC-1 implementation */
                    /* Historical fix for 4x4 blocks */
                    if (pL->blkType==VC1_BLK_INTER4X4 && LSBP)
                        LSBP = 0xF;
                    if (_pnBlk->blkType==VC1_BLK_INTER4X4 && RSBP)
                        RSBP = 0xF;
                }
                Edge_ext = LSBP | (RSBP>>1);
                Edge_ext = ( ((Edge_ext&4)>>1) + (Edge_ext&1));
            }
        }
        else
            Edge_ext  = 0;

        if ((_pBlk->blkType==VC1_BLK_INTER4X8) || (_pBlk->blkType==VC1_BLK_INTER4X4))
        {
            if ((SBP & 8) || (SBP & 4))
                Edge_int |= 2;
            if ((SBP & 2) || (SBP & 1))
                Edge_int |= 1;
        }
    _own_FilterDeblockingChroma_VerEdge_VC1(pUUpLBlock + 8, Pquant,Pitch, deblock_table_2[Edge_ext]);
    _own_FilterDeblockingChroma_VerEdge_VC1(pUUpLBlock + 4, Pquant,Pitch, deblock_table_2[Edge_int]);
}

void Deblocking_ProgressiveIpicture(VC1Context* pContext)
{
    int32_t WidthMB =  pContext->m_seqLayerHeader.widthMB;
    int32_t MaxWidthMB =  pContext->m_seqLayerHeader.MaxWidthMB;
    int32_t curX, curY;
    uint32_t PQuant = pContext->m_picLayerHeader->PQUANT;

    int32_t HeightMB   = pContext->DeblockInfo.HeightMB;
    VC1MB* m_CurrMB = pContext->m_pCurrMB;

    int32_t YPitch = m_CurrMB->currYPitch;
    int32_t UPitch = m_CurrMB->currUPitch;
    int32_t VPitch = m_CurrMB->currVPitch;



    int32_t flag_ver = 0;

    /* Deblock horizontal edges */
    for (curY=0; curY<HeightMB-1; curY++)
    {
        for (curX=0; curX<WidthMB; curX++)
        {
            flag_ver = 0;
            /* Top luma blocks */
            _own_FilterDeblockingLuma_HorEdge_VC1((m_CurrMB->currYPlane+8*YPitch), PQuant, YPitch,flag_ver);

            /* Bottom Luma blocks */
            _own_FilterDeblockingLuma_HorEdge_VC1((m_CurrMB->currYPlane+YPitch*16),
                                              PQuant,YPitch,flag_ver);

            /* Horizontal deblock Cb */
            _own_FilterDeblockingChroma_HorEdge_VC1((m_CurrMB->currUPlane+8*UPitch),
                                                PQuant,UPitch,flag_ver);
            /* Horizontal deblock Cr */
            _own_FilterDeblockingChroma_HorEdge_VC1((m_CurrMB->currVPlane+8*VPitch),
                                                PQuant,VPitch,flag_ver);

            ++m_CurrMB;
        }
        
        m_CurrMB += (MaxWidthMB - WidthMB);
    }
    if (pContext->DeblockInfo.is_last_deblock)
    {
    for (curX=0; curX<WidthMB; curX++)
    {
        flag_ver =0;
        /* Top luma blocks */
        _own_FilterDeblockingLuma_HorEdge_VC1((m_CurrMB->currYPlane+8*YPitch), PQuant, YPitch,flag_ver);
        ++m_CurrMB;
    }
    m_CurrMB += (MaxWidthMB - WidthMB);

    } else
        HeightMB -= 1;

    m_CurrMB   -= MaxWidthMB*HeightMB;

    /* Deblock vertical edges */
    for (curY=0; curY<HeightMB; curY++)
    {
        for (curX=0; curX<WidthMB-1; curX++)
        {
            flag_ver =0;
            _own_FilterDeblockingLuma_VerEdge_VC1((m_CurrMB->currYPlane+8), PQuant,YPitch,flag_ver);

            /* Bottom Luma blocks */
            _own_FilterDeblockingLuma_VerEdge_VC1((m_CurrMB->currYPlane+16),PQuant,YPitch,flag_ver);
            //flag_ver = IPPVC_EDGE_HALF_2;
            /* Vertical deblock Cb */
            _own_FilterDeblockingChroma_VerEdge_VC1(m_CurrMB->currUPlane+8,PQuant,UPitch,flag_ver);
            /* Vertical deblock Cr */
            _own_FilterDeblockingChroma_VerEdge_VC1(m_CurrMB->currVPlane+8,PQuant,VPitch,flag_ver);
            ++m_CurrMB;
         }
        flag_ver =0;
        _own_FilterDeblockingLuma_VerEdge_VC1((m_CurrMB->currYPlane+8), PQuant,YPitch,flag_ver);
        ++m_CurrMB;
        m_CurrMB += (MaxWidthMB - WidthMB);
    }


}

void Deblocking_ProgressivePpicture(VC1Context* pContext)
{
    int32_t WidthMB =  pContext->m_seqLayerHeader.widthMB;
    int32_t MaxWidthMB =  pContext->m_seqLayerHeader.MaxWidthMB;
    int32_t HeightMB   = pContext->DeblockInfo.HeightMB;
    int32_t curX, curY;
    int32_t PQuant = pContext->m_picLayerHeader->PQUANT;
    VC1MB* m_CurrMB = pContext->m_pCurrMB;

    int32_t YPitch = m_CurrMB->currYPitch;
    int32_t UPitch = m_CurrMB->currUPitch;
    int32_t VPitch = m_CurrMB->currVPitch;

    LFilterType type = static_cast<LFilterType>(pContext->m_seqLayerHeader.PROFILE);
    LFilterType type_current = type;
    LFilterType type_current_next = type;

    if (VC1_PROFILE_MAIN == static_cast<int>(type))
    {
        if ((pContext->m_MBs[0].mbType == VC1_MB_INTRA) || (pContext->m_MBs[0].m_pBlocks[0].blkType > VC1_BLK_INTER))
        {
            type              = LFilterMainPInterFrame;
            type_current      = LFilterMainPInterFrame;
            type_current_next = LFilterMainPInterFrame;
        }
    }

    for (curY=0; curY<HeightMB-1; curY++)
    {
        for (curX=0; curX<WidthMB; curX++)
        {
            if ((m_CurrMB->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
                type_current =  LFilterMainPFrame;

            HorizontalDeblockingLumaP(m_CurrMB->currYPlane,PQuant,YPitch,m_CurrMB,m_CurrMB,type_current);

            if (((m_CurrMB+MaxWidthMB)->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
                type_current_next =  LFilterMainPFrame;

            HorizontalDeblockingLumaP(m_CurrMB->currYPlane+YPitch*8,PQuant,YPitch,m_CurrMB,m_CurrMB+WidthMB,type_current_next);


            HorizontalDeblockingChromaP(m_CurrMB->currUPlane,
                                        PQuant,
                                        UPitch,
                                        &m_CurrMB->m_pBlocks[4],
                                        &(m_CurrMB+MaxWidthMB)->m_pBlocks[4],
                                        type_current_next);

            HorizontalDeblockingChromaP(m_CurrMB->currVPlane,
                                         PQuant,
                                         VPitch,
                                         &m_CurrMB->m_pBlocks[5],
                                         &(m_CurrMB+MaxWidthMB)->m_pBlocks[5],
                                         type_current_next);

            type_current      = type;
            type_current_next = type;
            m_CurrMB++;
        }
        m_CurrMB += (MaxWidthMB - WidthMB);
    }
    if (pContext->DeblockInfo.is_last_deblock)
    {
        for (curX=0; curX<WidthMB; curX++)
    {
        if ((m_CurrMB->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
            type_current =  LFilterMainPFrame;

        HorizontalDeblockingLumaP(m_CurrMB->currYPlane,PQuant,YPitch,m_CurrMB,m_CurrMB,type_current);
        HorizontalDeblockingLumaP(m_CurrMB->currYPlane+YPitch*8,PQuant,YPitch,m_CurrMB,NULL,type_current_next);

        HorizontalDeblockingChromaP(m_CurrMB->currUPlane,
            PQuant,
            UPitch,
            &m_CurrMB->m_pBlocks[4],
            NULL,
            type_current_next);

        HorizontalDeblockingChromaP(m_CurrMB->currVPlane,
            PQuant,
            VPitch,
            &m_CurrMB->m_pBlocks[5],
            NULL,
            type_current_next);
        type_current      = type;
        type_current_next = type;
        m_CurrMB++;
    }
        m_CurrMB += (MaxWidthMB - WidthMB);

    } else
        HeightMB -=1;

    m_CurrMB -= MaxWidthMB*HeightMB;


    for (curY=0; curY<HeightMB; curY++)
    {
        for (curX=0; curX<WidthMB-1; curX++)
        {
            if ((m_CurrMB->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
                type_current =  LFilterMainPFrame;

            VerticalDeblockingLumaP(m_CurrMB->currYPlane,
                PQuant,
                YPitch,
                m_CurrMB,
                m_CurrMB,
                type_current);


            if (((m_CurrMB+1)->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
                type_current_next =  LFilterMainPFrame;

            VerticalDeblockingLumaP((m_CurrMB->currYPlane+8),
                PQuant,
                YPitch,
                m_CurrMB,
                m_CurrMB+1,
                type_current_next);

            VerticalDeblockingChromaP(m_CurrMB->currUPlane,
                PQuant,
                UPitch,
                &m_CurrMB->m_pBlocks[4],
                &(m_CurrMB+1)->m_pBlocks[4],
                type_current_next);

            VerticalDeblockingChromaP(m_CurrMB->currVPlane,
                PQuant,
                VPitch,
                &m_CurrMB->m_pBlocks[5],
                &(m_CurrMB+1)->m_pBlocks[5],
                type_current_next);

            type_current      = type;
            type_current_next = type;
            ++m_CurrMB;

        }

        if ((m_CurrMB->mbType&VC1_MB_4MV_INTER) == VC1_MB_4MV_INTER)
            type_current =  LFilterMainPFrame;

        VerticalDeblockingLumaP(m_CurrMB->currYPlane,
            PQuant,
            YPitch,
            m_CurrMB,
            m_CurrMB,
            type_current);
         VerticalDeblockingLumaP((m_CurrMB->currYPlane+8),
            PQuant,
            YPitch,
            m_CurrMB,
            NULL,
            type_current_next);
        VerticalDeblockingChromaP((m_CurrMB->currUPlane),
            PQuant,
            UPitch,
            &m_CurrMB->m_pBlocks[4],
            NULL,
            type_current_next);
        VerticalDeblockingChromaP((m_CurrMB->currVPlane),
            PQuant,VPitch,
            &m_CurrMB->m_pBlocks[5],
            NULL,
            type_current_next);
        type_current      = type;
        type_current_next = type;
        ++m_CurrMB;
        m_CurrMB += (MaxWidthMB - WidthMB);
    }

}
#endif //UMC_ENABLE_VC1_VIDEO_DECODER