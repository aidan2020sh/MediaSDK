/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 Intel Corporation. All Rights Reserved.
//
//
//          VC-1 (VC1) encoder, macroblock functionality
//
*/
#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_ENCODER)

#ifndef _ENCODER_VC1_MB_H_
#define _ENCODER_VC1_MB_H_

#include "ippvc.h"
#include "umc_structures.h"
#include "umc_vc1_enc_def.h"
#include "umc_vc1_enc_bitstream.h"
#include "umc_vc1_enc_debug.h"
#include "umc_vc1_enc_common.h"
#include "umc_vc1_enc_block_template.h"
#include "umc_memory_allocator.h"
#include "umc_vc1_enc_statistic.h"
#include "umc_vc1_enc_tables.h"

#ifdef VC1_ME_MB_STATICTICS
#include "umc_me.h"
#endif
namespace UMC_VC1_ENCODER
{

class VC1EncoderCodedMB
{
private:

    eMBType         m_MBtype;

    // inter MB
    sCoordinate     m_dMV[2][4];      //  backward,forward. up to 4 coded MV for 1 MB.
                                      //  field bSecond contains flag Opposite.
    Ipp8u           m_pHybrid[4];

    eTransformType  m_tsType[6];
    bool            m_bMBTSType;      //transform type on MB level
    Ipp8u           m_uiFirstCodedBlock;

    // intra
    Ipp16s          m_iDC[6];

    //all
    Ipp8u           m_uRun   [6][65];
    Ipp16s          m_iLevel [6][64];
    Ipp8u           m_nPairs [6][4]; // 6 blocks, 4 subblocks
    Ipp8u           m_uiMBCBPCY;

    //for I MB
    Ipp8s           m_iACPrediction;

    // 4MV
    Ipp8u           m_uiIntraPattern;
    bool            m_bOverFlag;

    #ifdef VC1_ME_MB_STATICTICS
        UMC::MeMbStat* m_MECurMbStat;
    #endif
public:
    VC1EncoderCodedMB()
    {
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat = NULL;
    #endif
    }
    UMC::Status Init(eMBType type)
    {
        m_MBtype                = type;
        m_uiMBCBPCY             = 0;
        m_uiFirstCodedBlock     = 0;
        m_iACPrediction         = 0;
        m_pHybrid[0]            = 0;
        m_pHybrid[1]            = 0;
        m_pHybrid[2]            = 0;
        m_pHybrid[3]            = 0;
        m_bMBTSType             = true;
        m_uiIntraPattern        = 0;
        m_bOverFlag             = false;

        memset(m_iDC,0,6*sizeof(Ipp16s));

        memset(m_nPairs[0],0,4*sizeof(Ipp8u));
        memset(m_nPairs[1],0,4*sizeof(Ipp8u));
        memset(m_nPairs[2],0,4*sizeof(Ipp8u));
        memset(m_nPairs[3],0,4*sizeof(Ipp8u));
        memset(m_nPairs[4],0,4*sizeof(Ipp8u));
        memset(m_nPairs[5],0,4*sizeof(Ipp8u));

        m_tsType[0] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[1] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[2] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[3] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[4] = VC1_ENC_8x8_TRANSFORM;
        m_tsType[5] = VC1_ENC_8x8_TRANSFORM;

        return UMC::UMC_OK;
    }
    inline bool isIntra(Ipp32s blockNum)
    {
        return  ((m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blockNum)))!=0);
    }
    inline Ipp8u GetIntraPattern()
    {
        return m_uiIntraPattern;
    }

    inline void SetMBIntraPattern(Ipp8u uiIntraPattern)
    {
        m_uiIntraPattern = uiIntraPattern;
    }
    inline void SetIntraBlock(Ipp32u blockNum)
    {
        m_uiIntraPattern = m_uiIntraPattern | (1<<VC_ENC_PATTERN_POS(blockNum));
    }
    inline void ChangeType(eMBType type)
    {
        m_MBtype = type;
    }
    inline eMBType GetMBType()
    {
        return m_MBtype;
    }

    inline Ipp8u GetMBPattern()
    {
        return
           ((((m_nPairs[0][0] + m_nPairs[0][1] + m_nPairs[0][2] + m_nPairs[0][3])>0)<< (5 - 0))|
            (((m_nPairs[1][0] + m_nPairs[1][1] + m_nPairs[1][2] + m_nPairs[1][3])>0)<< (5 - 1))|
            (((m_nPairs[2][0] + m_nPairs[2][1] + m_nPairs[2][2] + m_nPairs[2][3])>0)<< (5 - 2))|
            (((m_nPairs[3][0] + m_nPairs[3][1] + m_nPairs[3][2] + m_nPairs[3][3])>0)<< (5 - 3))|
            (((m_nPairs[4][0] + m_nPairs[4][1] + m_nPairs[4][2] + m_nPairs[4][3])>0)<< (5 - 4))|
            (((m_nPairs[5][0] + m_nPairs[5][1] + m_nPairs[5][2] + m_nPairs[5][3])>0)<< (5 - 5)));

    }
     inline Ipp32u GetBlocksPattern()
    {
        Ipp32u  blocksPattern=0;
        Ipp32s  i;
        bool SubBlk[4];

        for(i=0;i<6;i++)
        {
            switch (m_tsType[i])
            {
            case  VC1_ENC_8x8_TRANSFORM:
                SubBlk[0]=SubBlk[1]=SubBlk[2]=SubBlk[3] = (m_nPairs[i][0]>0);
                break;
            case  VC1_ENC_8x4_TRANSFORM:
                SubBlk[0]=SubBlk[1]= (m_nPairs[i][0]>0);
                SubBlk[2]=SubBlk[3]= (m_nPairs[i][1]>0);
                break;
            case  VC1_ENC_4x8_TRANSFORM:
                SubBlk[0]=SubBlk[2]= (m_nPairs[i][0]>0);
                SubBlk[1]=SubBlk[3]= (m_nPairs[i][1]>0);
                break;
            case  VC1_ENC_4x4_TRANSFORM:
                SubBlk[0] =(m_nPairs[i][0]>0);
                SubBlk[1] =(m_nPairs[i][1]>0);
                SubBlk[2] =(m_nPairs[i][2]>0);
                SubBlk[3] =(m_nPairs[i][3]>0);
                break;
           }
           blocksPattern |= SubBlk[0]<<VC_ENC_PATTERN_POS1(i,0)
                         |  SubBlk[1]<<VC_ENC_PATTERN_POS1(i,1)
                         |  SubBlk[2]<<VC_ENC_PATTERN_POS1(i,2)
                         |  SubBlk[3]<<VC_ENC_PATTERN_POS1(i,3) ;

        }
        return blocksPattern;

    }
    inline bool isSkip()
    {
        switch (m_MBtype)
        {
            case VC1_ENC_I_MB:
            case VC1_ENC_P_MB_INTRA:
            case VC1_ENC_B_MB_INTRA:
                 return false;
            case VC1_ENC_B_MB_DIRECT:
                return (m_uiMBCBPCY == 0);
            case VC1_ENC_P_MB_1MV:
            case VC1_ENC_B_MB_F:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0));
            case VC1_ENC_B_MB_B:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[0][0].x==0 && m_dMV[0][0].y==0));
            case VC1_ENC_B_MB_FB:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0)&&(m_dMV[0][0].x==0 && m_dMV[0][0].y==0));
            case VC1_ENC_P_MB_4MV:
                return ((m_uiMBCBPCY == 0)&&(m_dMV[1][0].x==0 && m_dMV[1][0].y==0 &&
                                             m_dMV[1][1].x==0 && m_dMV[1][1].y==0 &&
                                             m_dMV[1][2].x==0 && m_dMV[1][2].y==0 &&
                                             m_dMV[1][3].x==0 && m_dMV[1][3].y==0 ));
            case VC1_ENC_P_MB_SKIP_1MV:
            case VC1_ENC_P_MB_SKIP_4MV:
            case VC1_ENC_B_MB_SKIP_F:
            case VC1_ENC_B_MB_SKIP_B:
            case VC1_ENC_B_MB_SKIP_F_4MV:
            case VC1_ENC_B_MB_SKIP_B_4MV:
            case VC1_ENC_B_MB_SKIP_FB:
            case VC1_ENC_B_MB_SKIP_DIRECT:
                return true;
            default:
                return false;
          }


    }
    inline void SetHybrid (Ipp8u Hybrid, Ipp32u nBlock=0)
    {
        m_pHybrid[nBlock] = Hybrid;
    }

    inline Ipp8u GetHybrid (Ipp32u nBlock=0)
    {
       return m_pHybrid[nBlock];
    }

    inline void SetMBCBPCY (Ipp8u  MBCBPCY)
    {
        m_uiMBCBPCY     =  MBCBPCY; //???
    }

    inline Ipp8u GetMBCBPCY()
    {
        return m_uiMBCBPCY;
    }
    inline void SetdMV(sCoordinate dmv, bool bForward=true)
    {
        m_dMV[bForward][0].x = m_dMV[bForward][1].x = m_dMV[bForward][2].x = m_dMV[bForward][3].x = dmv.x;
        m_dMV[bForward][0].y = m_dMV[bForward][1].y = m_dMV[bForward][2].y = m_dMV[bForward][3].y = dmv.y;
    }
    inline void SetdMV_F(sCoordinate dmv, bool bForward=true)
    {
        m_dMV[bForward][0].x = m_dMV[bForward][1].x = m_dMV[bForward][2].x = m_dMV[bForward][3].x = dmv.x;
        m_dMV[bForward][0].y = m_dMV[bForward][1].y = m_dMV[bForward][2].y = m_dMV[bForward][3].y = dmv.y;
        m_dMV[bForward][0].bSecond = m_dMV[bForward][1].bSecond =
        m_dMV[bForward][2].bSecond = m_dMV[bForward][3].bSecond = dmv.bSecond;
    }
    inline void SetBlockdMV(sCoordinate dmv,Ipp32s block ,bool bForward=true)
    {
        m_dMV[bForward][block].x = dmv.x;
        m_dMV[bForward][block].y = dmv.y;
    }
    inline void SetBlockdMV_F(sCoordinate dmv,Ipp32s block ,bool bForward=true)
    {
        m_dMV[bForward][block].x = dmv.x;
        m_dMV[bForward][block].y = dmv.y;
        m_dMV[bForward][block].bSecond =dmv.bSecond;

    }

    inline void SetACPrediction (Ipp8s ACPred)
    {
        m_iACPrediction   = ACPred;
    }
    inline Ipp8u GetACPrediction()
    {
        return m_iACPrediction;
    }

    inline void GetdMV(sCoordinate* dMV, bool forward = 1, Ipp8u blk = 0)
    {
        dMV->x = m_dMV[forward][blk].x;
        dMV->y = m_dMV[forward][blk].y;
        dMV->bSecond = m_dMV[forward][blk].bSecond;
    }

    inline void SetTSType(eTransformType* tsType)
    {
        m_tsType[0]= tsType[0];
        m_tsType[1]= tsType[1];
        m_tsType[2]= tsType[2];
        m_tsType[3]= tsType[3];
        m_tsType[4]= tsType[4];
        m_tsType[5]= tsType[5];

        m_bMBTSType = ( m_tsType[0] == m_tsType[1] && m_tsType[0] == m_tsType[2] &&
                        m_tsType[0] == m_tsType[3] && m_tsType[0] == m_tsType[4] &&
                        m_tsType[0] == m_tsType[5]);
    }
    inline eTransformType* GetTSType()
    {
        return m_tsType;
    }

    inline eTransformType GetTSTypeBlk(Ipp8u blk)
    {
        return m_tsType[blk];
    }

    inline void SetOverFlag(bool overflag)
    {
        m_bOverFlag = overflag;
    }

    inline bool GetOverFlag()
    {
        return m_bOverFlag;
    }

    void SaveResidual (Ipp16s* pBlk,Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk);

    void GetResiduals (Ipp16s* pBlk, Ipp32u  step, const Ipp8u* pScanMatrix, Ipp32s blk);

    inline bool isNulldMV(Ipp32s blk, bool bForward=true)
    {
        return (!((m_dMV[bForward][blk].x!=0)|| (m_dMV[bForward][blk].y!=0)));
    }
    // VLC coding

 #ifdef VC1_ME_MB_STATICTICS
    UMC::Status     SetMEFrStatPointer(UMC::MeMbStat* MEFrStat)
    {
        if(!MEFrStat)
        {
            assert(0);
            return UMC::UMC_ERR_NULL_PTR;
        }

        m_MECurMbStat = MEFrStat;

        return UMC::UMC_OK;
    }
 #endif

   UMC::Status WriteMBHeaderI_SM (VC1EncoderBitStreamSM* pCodedMB, bool bBitplanesRaw);
   UMC::Status WriteMBHeaderI_ADV(VC1EncoderBitStreamAdv* pCodedMB,bool bBitplanesRaw, bool bOverlapMB);
   UMC::Status WriteMBHeaderI_Field(VC1EncoderBitStreamAdv* pCodedMB,bool bBitplanesRaw, bool bOverlapMB);

template <class T>
UMC::Status WriteMBHeaderP_INTRA    ( T*              pCodedMB,
                                      bool            bBitplanesRaw,
                                      const Ipp16u*   pMVDiffTable,
                                      const Ipp16u*   pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;
    bool coded = (m_uiMBCBPCY!=0); //only 8x8 transform

    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
      Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        err = pCodedMB->PutBits(0,1); //if intra, then non-skip
        VC1_ENC_CHECK (err)
    }

    err = WriteMVDataPIntra(pCodedMB,pMVDiffTable);
    VC1_ENC_CHECK (err)


    if (coded ==0)
    {
        //err = WriteMBQuantParameter(pCodedMB, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        err = pCodedMB->PutBits(m_iACPrediction,1);
        VC1_ENC_CHECK (err)

    }
    else
    {
        err = pCodedMB->PutBits(m_iACPrediction,1);
        VC1_ENC_CHECK (err)
        err = pCodedMB->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)
    }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}
template <class T>
UMC::Status WriteMBHeaderPField_INTRA          (  T*              pCodedMB,
                                                const  Ipp8u*     pMBTypeFieldTable,
                                                const Ipp32u*     pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;
    bool coded = (m_uiMBCBPCY!=0);
    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif

   err = pCodedMB->PutBits(pMBTypeFieldTable[coded<<1],pMBTypeFieldTable[(coded<<1)+1]); //if intra, then non-skip
   VC1_ENC_CHECK (err)

   //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
   //VC1_ENC_CHECK (err)

   err = pCodedMB->PutBits(m_iACPrediction,1);
   VC1_ENC_CHECK (err)

   if (coded)
   {
       err = pCodedMB->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
       VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WriteMBHeaderB_INTRA    ( T*              pCodedMB,
                                      bool            bBitplanesRaw,
                                      const Ipp16u*   pMVDiffTable,
                                      const Ipp16u*   pCBPCYTable)
{
    UMC::Status     err     =   UMC::UMC_OK;

    VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        err = pCodedMB->PutBits(0,1); //non directMB
        VC1_ENC_CHECK (err)
    }

    err =  WriteMBHeaderP_INTRA (pCodedMB,bBitplanesRaw,pMVDiffTable,pCBPCYTable);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}


template <class T>  UMC::Status   WritePMB1MV (T*    pCodedBlock,
                                                     bool                   bRawBitplanes,
                                                     Ipp8u                  MVTab,
                                                     Ipp8u                  MVRangeIndex,
                                                     const Ipp16u*          pCBPCYTable,
                                                     bool                   bVTS,
                                                     bool                   bMVHalf,
                                                     const Ipp16s           (*pTTMBVLC)[4][6],
                                                     const Ipp8u            (*pTTBLKVLC)[6],
                                                     const Ipp8u*           pSBP4x4Table,
                                                     const sACTablesSet*    pACTablesSet,
                                                     sACEscInfo*            pACEscInfo
                                                     )

{
UMC::Status  err=UMC::UMC_OK;
Ipp32s       blk;

VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp32u MBStart = pCodedBlock->GetCurrBit();
#endif
if (bRawBitplanes)
{
    err = pCodedBlock->PutBits(0,1);//skip
    VC1_ENC_CHECK (err)
}
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
    err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex, m_uiMBCBPCY!=0, bMVHalf);
    VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif

    if (m_pHybrid[0])
    {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
    }

if (m_uiMBCBPCY!=0)
{

    err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
    VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

    if (bVTS)
    {
        err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
        VC1_ENC_CHECK (err)
    }

    for (blk = 0; blk<6; blk++)
    {
        if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
        {
#ifdef VC1_ME_MB_STATICTICS
            //Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
            err = WriteBlockAC(pCodedBlock,
                   pACTablesSet,
                   pACEscInfo,
                   blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
            //m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for

}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)(pCodedBlock->GetCurrBit()- MBStart);
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVField (T*                     pCodedBlock,
                                                    const Ipp8u*            pMBTypeFieldTable,
                                                    sMVFieldInfo*          pMVFieldInfo,
                                                    Ipp8u                  MVRangeIndex,
                                                    const Ipp32u*          pCBPCYTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const Ipp16s           (*pTTMBVLC)[4][6],
                                                    const Ipp8u            (*pTTBLKVLC)[6],
                                                    const Ipp8u*           pSBP4x4Table,
                                                    const sACTablesSet*    pACTablesSet,
                                                    sACEscInfo*            pACEscInfo
                                                    )

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    bool            bMVCoded =  (m_dMV[1][0].x || m_dMV[1][0].y);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)


#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField1Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }
   if (m_pHybrid[0])
   {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVFieldMixed (   T*                     pCodedBlock,
                                                            const Ipp8u*            pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            Ipp8u                  MVRangeIndex,
                                                            const Ipp32u*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const Ipp16s           (*pTTMBVLC)[4][6],
                                                            const Ipp8u            (*pTTBLKVLC)[6],
                                                            const Ipp8u*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            const Ipp8u*           pMV4BP,
                                                            sACEscInfo*            pACEscInfo)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    Ipp8u           MVPattern = 0;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType = (bCoded)? 7:6;


    VC1_NULL_PTR(pCodedBlock)


#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

    for (int i = 0; i<4; i++)
    {
        if (m_dMV[1][i].x || m_dMV[1][i].y)
        {
            MVPattern = (Ipp8u)(MVPattern | (1<<(3-i)));
        }    
    }
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
   err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
   VC1_ENC_CHECK (err)

   for (int i = 0; i<4; i++)
   {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField1Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i);
            VC1_ENC_CHECK (err)
        }  
        if (m_pHybrid[i])
        {
            err = pCodedBlock->PutBits(m_pHybrid[i]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
   } 


#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB2MVField (T*                     pCodedBlock,
                                                    const Ipp8u*           pMBTypeFieldTable,
                                                    sMVFieldInfo*          pMVFieldInfo,
                                                    Ipp8u                  MVRangeIndex,
                                                    const Ipp32u*          pCBPCYTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const Ipp16s           (*pTTMBVLC)[4][6],
                                                    const Ipp8u            (*pTTBLKVLC)[6],
                                                    const Ipp8u*           pSBP4x4Table,
                                                    const sACTablesSet*    pACTablesSet,
                                                    sACEscInfo*            pACEscInfo,
                                                    bool                   bForward=true)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    bool            bMVCoded =  (m_dMV[bForward][0].x || m_dMV[bForward][0].y || m_dMV[bForward][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }
   if (m_pHybrid[0])
   {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}

template <class T>  UMC::Status   WritePMB2MVFieldMixed (T*                     pCodedBlock,
                                                         const Ipp8u*           pMBTypeFieldTable,
                                                         sMVFieldInfo*          pMVFieldInfo,
                                                         Ipp8u                  MVRangeIndex,
                                                         const Ipp32u*          pCBPCYTable,
                                                         bool                   bVTS,
                                                         bool                   bMVHalf,
                                                         const Ipp16s           (*pTTMBVLC)[4][6],
                                                         const Ipp8u            (*pTTBLKVLC)[6],
                                                         const Ipp8u*           pSBP4x4Table,
                                                         const sACTablesSet*    pACTablesSet,
                                                         sACEscInfo*            pACEscInfo,
                                                         const Ipp8u*           pMV4BP,
                                                         bool                   bForward=true)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    Ipp8u           MVPattern = 0;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType = (bCoded)? 7:6;


    VC1_NULL_PTR(pCodedBlock)

    for (int i = 0; i<4; i++)
    {
        if (m_dMV[bForward][i].x || m_dMV[bForward][i].y || m_dMV[bForward][i].bSecond)
        {
            MVPattern = (Ipp8u)(MVPattern | (1<<(3-i)));
        }    
    }

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
   VC1_ENC_CHECK (err)



#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif 

   for (int i = 0; i<4; i++)
   {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i);
            VC1_ENC_CHECK (err)
        }  
        if (m_pHybrid[i])
        {
            err = pCodedBlock->PutBits(m_pHybrid[i]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
   }  

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldForward     (T*                     pCodedBlock,
                                                            const Ipp8u*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            Ipp8u                  MVRangeIndex,
                                                            const Ipp32u*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const Ipp16s           (*pTTMBVLC)[4][6],
                                                            const Ipp8u            (*pTTBLKVLC)[6],
                                                            const Ipp8u*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    bool            bMVCoded =  (m_dMV[1][0].x || m_dMV[1][0].y || m_dMV[1][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(1,1);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldForwardMixed     (T*                     pCodedBlock,
                                                            const Ipp8u*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            Ipp8u                  MVRangeIndex,
                                                            const Ipp32u*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const Ipp16s           (*pTTMBVLC)[4][6],
                                                            const Ipp8u            (*pTTBLKVLC)[6],
                                                            const Ipp8u*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            const Ipp8u*           pMV4BP,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    Ipp8u           MVPattern = 0;
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType = (bCoded)? 7:6;

    VC1_NULL_PTR(pCodedBlock)
    for (int i = 0; i<4; i++)
    {
        if (m_dMV[1][i].x || m_dMV[1][i].y || m_dMV[1][i].bSecond)
        {
            MVPattern = (Ipp8u)(MVPattern | (1<<(3-i)));
        }    
    }
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

    err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
    VC1_ENC_CHECK (err)
    if (bBitPlaneRaw)
    {
        err = pCodedBlock->PutBits(1,1);
        VC1_ENC_CHECK (err)
    }

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
    err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
    VC1_ENC_CHECK (err)
    for (int i = 0; i<4; i++)
    {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i, true);
            VC1_ENC_CHECK (err)
        }  
    }  

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
            //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
            //VC1_ENC_CHECK (err)

            if (bVTS)
            {
                err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
                VC1_ENC_CHECK (err)
            }

            for (blk = 0; blk<6; blk++)
            {
                if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
                {
#ifdef VC1_ME_MB_STATICTICS
                    Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                    err = WriteVSTSBlk ( pCodedBlock,
                        pTTBLKVLC,
                        pSBP4x4Table,
                        bVTS,
                        blk);

                    VC1_ENC_CHECK (err)

                        STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                    err = WriteBlockAC(pCodedBlock,
                        pACTablesSet,
                        pACEscInfo,
                        blk);
                    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                    VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                    m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
                }//if
            }//for
    }//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
   m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
   return err;
}
template <class T>  UMC::Status   WriteBMBFieldBackward    (T*                     pCodedBlock,
                                                            const Ipp8u*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            Ipp8u                  MVRangeIndex,
                                                            const Ipp32u*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const Ipp16s           (*pTTMBVLC)[4][6],
                                                            const Ipp8u            (*pTTBLKVLC)[6],
                                                            const Ipp8u*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    bool            bMVCoded =  (m_dMV[0][0].x || m_dMV[0][0].y || m_dMV[0][0].bSecond);
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + bMVCoded + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(0,1);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   if (bMVCoded)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0, false);
        VC1_ENC_CHECK (err)
   }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit() - MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldBackwardMixed    (T*                     pCodedBlock,
                                                            const Ipp8u*           pMBTypeFieldTable,
                                                            sMVFieldInfo*          pMVFieldInfo,
                                                            Ipp8u                  MVRangeIndex,
                                                            const Ipp32u*          pCBPCYTable,
                                                            bool                   bVTS,
                                                            bool                   bMVHalf,
                                                            const Ipp16s           (*pTTMBVLC)[4][6],
                                                            const Ipp8u            (*pTTBLKVLC)[6],
                                                            const Ipp8u*           pSBP4x4Table,
                                                            const sACTablesSet*    pACTablesSet,
                                                            sACEscInfo*            pACEscInfo,
                                                            const Ipp8u*           pMV4BP,
                                                            bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    Ipp8u           MVPattern = 0;
    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType = (bCoded)? 7:6;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
        Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
    for (int i = 0; i<4; i++)
    {
        if (m_dMV[0][i].x || m_dMV[0][i].y || m_dMV[0][i].bSecond)
        {
            MVPattern = (Ipp8u)(MVPattern | (1<<(3-i)));
        }    
    }
    err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
    VC1_ENC_CHECK (err)

    if (bBitPlaneRaw)
    {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
    }
    //err = pCodedBlock->PutBits(0,1);
    //VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
    err = pCodedBlock->PutBits(pMV4BP[MVPattern<<1],pMV4BP[(MVPattern<<1)+1]);
    VC1_ENC_CHECK (err)

    for (int i = 0; i<4; i++)
    {
        if (MVPattern & (1<<(3-i)))
        {
            err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, i, false);
            VC1_ENC_CHECK (err)
        }  
    }  
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
        if (bCoded)
        {
            err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
            VC1_ENC_CHECK (err)
                //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
                //VC1_ENC_CHECK (err)

                if (bVTS)
                {
                    err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
                    VC1_ENC_CHECK (err)
                }

                for (blk = 0; blk<6; blk++)
                {
                    if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
                    {
#ifdef VC1_ME_MB_STATICTICS
                        Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                        err = WriteVSTSBlk ( pCodedBlock,
                            pTTBLKVLC,
                            pSBP4x4Table,
                            bVTS,
                            blk);

                        VC1_ENC_CHECK (err)

                            STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                        err = WriteBlockAC(pCodedBlock,
                            pACTablesSet,
                            pACEscInfo,
                            blk);
                        STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                        VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                            m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
                    }//if
                }//for
        }//MBPattern!=0

#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit() - MBStart;
#endif
        return err;
}
template <class T>  UMC::Status   WriteBMBFieldInterpolated    (T*                     pCodedBlock,
                                                                const Ipp8u*           pMBTypeFieldTable,
                                                                sMVFieldInfo*          pMVFieldInfo,
                                                                Ipp8u                  MVRangeIndex,
                                                                const Ipp32u*          pCBPCYTable,
                                                                bool                   bVTS,
                                                                bool                   bMVHalf,
                                                                const Ipp16s           (*pTTMBVLC)[4][6],
                                                                const Ipp8u            (*pTTBLKVLC)[6],
                                                                const Ipp8u*           pSBP4x4Table,
                                                                const sACTablesSet*    pACTablesSet,
                                                                sACEscInfo*            pACEscInfo,
                                                                bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;
    bool            bMVCodedF =  (m_dMV[1][0].x || m_dMV[1][0].y || m_dMV[1][0].bSecond);
    bool            bMVCodedB =  (m_dMV[0][0].x || m_dMV[0][0].y || m_dMV[0][0].bSecond);

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + (bMVCodedF) + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(3,2);
   VC1_ENC_CHECK (err)

   err = pCodedBlock->PutBits(bMVCodedB,1);
   VC1_ENC_CHECK (err)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
   if (bMVCodedF)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0 ,true);
        VC1_ENC_CHECK (err)
   }
    if (bMVCodedB)
   {
        err = WriteMVDataInterField2Ref(pCodedBlock, MVRangeIndex,pMVFieldInfo, bMVHalf, 0,false);
        VC1_ENC_CHECK (err)
   }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
#endif
    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WriteBMBFieldDirect           (T*                     pCodedBlock,
                                                                const Ipp8u*           pMBTypeFieldTable,
                                                                sMVFieldInfo*          /*pMVFieldInfo*/,
                                                                Ipp8u                  /*MVRangeIndex*/,
                                                                const Ipp32u*          pCBPCYTable,
                                                                bool                   bVTS,
                                                                const Ipp16s           (*pTTMBVLC)[4][6],
                                                                const Ipp8u            (*pTTBLKVLC)[6],
                                                                const Ipp8u*           pSBP4x4Table,
                                                                const sACTablesSet*    pACTablesSet,
                                                                sACEscInfo*            pACEscInfo,
                                                                bool                   bBitPlaneRaw)

{
    UMC::Status     err=UMC::UMC_OK;
    Ipp32s          blk;

    bool            bCoded   =  (m_uiMBCBPCY!=0);
    Ipp8u           nMBType =   (((Ipp8u)bCoded)<<1) + 0 + 2;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

   err = pCodedBlock->PutBits(pMBTypeFieldTable[nMBType<<1],pMBTypeFieldTable[(nMBType<<1)+1]);
   VC1_ENC_CHECK (err)
   if (bBitPlaneRaw)
   {
        err = pCodedBlock->PutBits(0,1);
        VC1_ENC_CHECK (err)
   }
   err = pCodedBlock->PutBits(2,2);
   VC1_ENC_CHECK (err)

    if (bCoded)
    {
        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //VC1_ENC_CHECK (err)

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }

        for (blk = 0; blk<6; blk++)
        {
            if (m_uiMBCBPCY & ((1<<VC_ENC_PATTERN_POS(blk))))
            {
#ifdef VC1_ME_MB_STATICTICS
                Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
                err = WriteVSTSBlk ( pCodedBlock,
                    pTTBLKVLC,
                    pSBP4x4Table,
                    bVTS,
                    blk);

                VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                    pACTablesSet,
                    pACEscInfo,
                    blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

                VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
                m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
#endif
        }//if
    }//for
}//MBPattern!=0
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
return err;
}
template <class T>  UMC::Status   WritePMB1MVMixed  (T*                     pCodedBlock,
                                                     bool                   bRawBitplanes,
                                                     Ipp8u                  MVTab,
                                                     Ipp8u                  MVRangeIndex,
                                                     const Ipp16u*          pCBPCYTable,
                                                     bool                   bVTS,
                                                     bool                   bMVHalf,
                                                     const Ipp16s           (*pTTMBVLC)[4][6],
                                                     const Ipp8u            (*pTTBLKVLC)[6],
                                                     const Ipp8u*           pSBP4x4Table,
                                                     const sACTablesSet*    pACTablesSet,
                                                     sACEscInfo*            pACEscInfo)

{
UMC::Status  err        =   UMC::UMC_OK;
VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
if (bRawBitplanes)
{
    UMC::Status  err=UMC::UMC_OK;
    err = pCodedBlock->PutBits(0,1);//mixed 1 MV
    VC1_ENC_CHECK (err)
}

err =  WritePMB1MV  (pCodedBlock,  bRawBitplanes, MVTab,
                     MVRangeIndex, pCBPCYTable,   bVTS, bMVHalf,
                     pTTMBVLC,     pTTBLKVLC,     pSBP4x4Table,
                     pACTablesSet, pACEscInfo);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif

return err;

}
template <class T>  UMC::Status   WritePMB4MVMixed (T*                     pCodedBlock,
                                                    bool                   bRawBitplanes,
                                                    Ipp32u                 quant,
                                                    Ipp8u                  MVTab,
                                                    Ipp8u                  MVRangeIndex,
                                                    const Ipp16u*          pCBPCYTable,
                                                    const Ipp16u*          pMVDiffTable,
                                                    bool                   bVTS,
                                                    bool                   bMVHalf,
                                                    const Ipp16s           (*pTTMBVLC)[4][6],
                                                    const Ipp8u            (*pTTBLKVLC)[6],
                                                    const Ipp8u*           pSBP4x4Table,
                                                    const Ipp32u**         pDCEncTable,
                                                    const sACTablesSet**   pACTablesSetIntra,
                                                    const sACTablesSet*    pACTablesSetInter,
                                                    sACEscInfo*            pACEscInfo
                                                    )

{
    UMC::Status  err        =   UMC::UMC_OK;
    Ipp32s       blk        =   0;
    Ipp32u       pattern    =   m_uiMBCBPCY;
    bool         bInterCoded=   (m_uiMBCBPCY & (~m_uiIntraPattern))? true:false;

    #ifdef VC1_ME_MB_STATICTICS
        Ipp32u MBStart = pCodedBlock->GetCurrBit();
    #endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(0,1);//skip
        VC1_ENC_CHECK (err)
    }
    for (blk=0; blk<4;blk++)
    {
        pattern |=
            ( !isNulldMV(blk)|| (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk))))?
            (1<<VC_ENC_PATTERN_POS(blk)):0;

    }
    err = pCodedBlock->PutBits(pCBPCYTable[2*(pattern&0x3F)], pCBPCYTable[2*(pattern&0x3F)+1]);
    VC1_ENC_CHECK (err)

    for (blk=0; blk<4;blk++)
    {
    #ifdef VC1_ME_MB_STATICTICS
        Ipp32u BlkStart = pCodedBlock->GetCurrBit();
        Ipp32u MVStart = pCodedBlock->GetCurrBit();
    #endif
        if(pattern & (1<<VC_ENC_PATTERN_POS(blk)))
        {
            if (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))
            {
                err = WriteMVDataPIntra(pCodedBlock,pMVDiffTable,blk);
                VC1_ENC_CHECK (err)
            }
            else
            {
                err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex,(m_uiMBCBPCY&(1<<VC_ENC_PATTERN_POS(blk)))!=0, bMVHalf, blk);
                VC1_ENC_CHECK (err)
            }
        }
        if (m_pHybrid[blk] && (!(m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))))
        {
            err = pCodedBlock->PutBits(m_pHybrid[blk]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVF[blk]   =  m_MECurMbStat->MVF[blk]  + (Ipp16u)(pCodedBlock->GetCurrBit()- MVStart);
        m_MECurMbStat->coeff[blk] =  m_MECurMbStat->coeff[blk]+ (Ipp16u)(pCodedBlock->GetCurrBit()- BlkStart);
    #endif
    }
//err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
//VC1_ENC_CHECK (err)

if (m_uiIntraPattern && m_iACPrediction>=0)
{
    err = pCodedBlock->PutBits(m_iACPrediction,1);
    VC1_ENC_CHECK (err)
}
if (bVTS && bInterCoded)
{
    err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
    VC1_ENC_CHECK (err)
}

for (blk = 0; blk<6; blk++)
{
    const sACTablesSet*   pACTablesSet = 0;

#ifdef VC1_ME_MB_STATICTICS
    Ipp32u BlkStart = pCodedBlock->GetCurrBit();
#endif

    if (m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(blk)))
    {
        err = WriteBlockDC(pCodedBlock,pDCEncTable[blk],quant,blk);
        VC1_ENC_CHECK (err)
        pACTablesSet =  pACTablesSetIntra[blk];
    }
    else
    {
         if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
         {
            err = WriteVSTSBlk ( pCodedBlock,
            pTTBLKVLC,
            pSBP4x4Table,
            bVTS,
            blk);

            VC1_ENC_CHECK (err)
         }
         pACTablesSet =  pACTablesSetInter;
    }

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
    if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
    {
        err = WriteBlockAC(pCodedBlock,
            pACTablesSet,
            pACEscInfo,
            blk);

        VC1_ENC_CHECK (err)
    }
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk]+(Ipp16u)(pCodedBlock->GetCurrBit()- BlkStart);
#endif
}//for

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)(pCodedBlock->GetCurrBit()- MBStart);
#endif
return err;
}

template <class T>  UMC::Status   WritePMB4MVSkipMixed    (T*                   pCodedBlock,
                                                           bool                 bRawBitplanes)

{
    UMC::Status  err        =   UMC::UMC_OK;
    Ipp32s       blk        =   0;

    VC1_NULL_PTR(pCodedBlock)

    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    for (blk=0; blk<4;blk++)
    {
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif
        if (m_pHybrid[blk])
        {
            err = pCodedBlock->PutBits(m_pHybrid[blk]-1,1); // Hybrid
            VC1_ENC_CHECK (err)
        }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
    }
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}
template <class T>  UMC::Status   WritePMB1MVSkipMixed    (T*                   pCodedBlock,
                                                           bool                 bRawBitplanes)

{
    UMC::Status  err        =   UMC::UMC_OK;
    //Ipp32s       blk        =   0;

    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1);//mixed 1 MV
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    if (m_pHybrid[0])
    {
        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)
    }

    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}

template <class T>  UMC::Status   WriteBMB_DIRECT      (T*                     pCodedBlock,
                                                        bool                   bRawBitplanes,
                                                        const Ipp16u*          pCBPCYTable,
                                                        bool                   bVTS,
                                                        const Ipp16s           (*pTTMBVLC)[4][6],
                                                        const Ipp8u            (*pTTBLKVLC)[6],
                                                        const Ipp8u*           pSBP4x4Table,
                                                        const sACTablesSet*    pACTablesSet,
                                                        sACEscInfo*            pACEscInfo)
{
    UMC::Status  err=UMC::UMC_OK;
    Ipp32s       blk;

    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(m_uiMBCBPCY==0,1); //skip
        VC1_ENC_CHECK (err)
    }
    if (m_uiMBCBPCY!=0)
    {

        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //if (err != UMC::UMC_OK) return err;

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }
        for (blk = 0; blk<6; blk++)
        {
           if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
           {
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif
            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

    STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                pACTablesSet,
                pACEscInfo,
                blk);
    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
           }
        }//for
    }//MBPattern!=0

    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}
template <class T>  UMC::Status  WriteBMB      (T*                     pCodedBlock,
                                                bool                   bRawBitplanes,
                                                Ipp8u                  MVTab,
                                                Ipp8u                  MVRangeIndex,
                                                const Ipp16u*          pCBPCYTable,
                                                bool                   bVTS,
                                                bool                   bMVHalf,
                                                const Ipp16s           (*pTTMBVLC)[4][6],
                                                const Ipp8u            (*pTTBLKVLC)[6],
                                                const Ipp8u*           pSBP4x4Table,
                                                const sACTablesSet*    pACTablesSet,
                                                sACEscInfo*            pACEscInfo,
                                                bool                   bBFraction)
{
    UMC::Status  err=UMC::UMC_OK;
    Ipp32s       blk;
    Ipp8u        mvType = (m_MBtype == VC1_ENC_B_MB_F)? 0:((m_MBtype == VC1_ENC_B_MB_B)?1:2);
    bool         NotLastInter = false;


    VC1_NULL_PTR(pCodedBlock)
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif

    if (m_MBtype == VC1_ENC_B_MB_FB)
    {
        NotLastInter = (m_dMV[1][0].x!=0 || m_dMV[1][0].y!=0 || m_uiMBCBPCY!=0);

    }
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(0,1); //skip
        VC1_ENC_CHECK (err)
    }
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif
    err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex, m_uiMBCBPCY!=0 || NotLastInter,  bMVHalf, 0, m_MBtype == VC1_ENC_B_MB_F);
    VC1_ENC_CHECK (err)

    #ifdef VC1_ME_MB_STATICTICS
        if(m_MBtype == VC1_ENC_B_MB_F)
            m_MECurMbStat->MVF[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
        else
            m_MECurMbStat->MVB[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
    #endif

    err = pCodedBlock->PutBits(BMVTypeVLC[bBFraction][mvType*2],BMVTypeVLC[bBFraction][mvType*2+1]);
    VC1_ENC_CHECK (err)

    if (NotLastInter)
    {
    #ifdef VC1_ME_MB_STATICTICS
        MVStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif
            err = WriteMVDataInter(pCodedBlock, MVDiffTablesVLC[MVTab],MVRangeIndex,   m_uiMBCBPCY!=0, bMVHalf, 0, true);
            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->MVB[0] += (Ipp16u)pCodedBlock->GetCurrBit()- MVStart;
    #endif
    }

    if (m_uiMBCBPCY!=0)
    {

        err = pCodedBlock->PutBits(pCBPCYTable[2*(m_uiMBCBPCY&0x3F)], pCBPCYTable[2*(m_uiMBCBPCY&0x3F)+1]);
        VC1_ENC_CHECK (err)
        //err = WriteMBQuantParameter(pCodedPicture, doubleQuant>>1);
        //if (err != UMC::UMC_OK) return err;

        if (bVTS)
        {
            err = WriteVSTSMB (pCodedBlock, pTTMBVLC);
            VC1_ENC_CHECK (err)
        }
        for (blk = 0; blk<6; blk++)
        {
            if ((m_uiMBCBPCY & (1<<VC_ENC_PATTERN_POS(blk))))
            {
    #ifdef VC1_ME_MB_STATICTICS
        Ipp16u BlkStart = (Ipp16u)pCodedBlock->GetCurrBit();
    #endif

            err = WriteVSTSBlk ( pCodedBlock,
                pTTBLKVLC,
                pSBP4x4Table,
                bVTS,
                blk);

            VC1_ENC_CHECK (err)

    STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
                err = WriteBlockAC(pCodedBlock,
                pACTablesSet,
                pACEscInfo,
                blk);
    STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);

            VC1_ENC_CHECK (err)
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedBlock->GetCurrBit()- BlkStart;
    #endif
            }
        }//for
    }//MBPattern!=0
    #ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
    #endif
    return err;
}


template <class T>
UMC::Status   WritePMB_SKIP(T*        pCodedBlock,
                            bool      bRawBitplanes)
{
    UMC::Status  err=UMC::UMC_OK;

    VC1_NULL_PTR(pCodedBlock)
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1);//skip
        VC1_ENC_CHECK (err)
    }
    if (m_pHybrid[0])
    {

        err = pCodedBlock->PutBits(m_pHybrid[0]-1,1); // Hybrid
        VC1_ENC_CHECK (err)

    }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status   WriteBMB_SKIP_NONDIRECT(T*      pCodedBlock,
                                      bool                   bRawBitplanes,
                                      bool                   bBFraction)
{
    UMC::Status     err     = UMC::UMC_OK;
    Ipp8u           mvType  = 0;

    VC1_NULL_PTR(pCodedBlock)
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif

    switch(m_MBtype)
    {
    case VC1_ENC_B_MB_SKIP_F :
    case VC1_ENC_B_MB_F:
            mvType = 0;
            break;
    case VC1_ENC_B_MB_SKIP_B :
    case VC1_ENC_B_MB_B:
            mvType = 1;
            break;
    default:
            mvType = 2;
            break;
    }
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(0,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1); //skip
        VC1_ENC_CHECK (err)
    }
    err = pCodedBlock->PutBits(BMVTypeVLC[bBFraction][mvType*2],BMVTypeVLC[bBFraction][mvType*2+1]);
    VC1_ENC_CHECK (err);
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status  WriteBMB_SKIP_DIRECT(T*                     pCodedBlock,
                                  bool                   bRawBitplanes)
{
    UMC::Status  err = UMC::UMC_OK;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedBlock->GetCurrBit();
#endif
    if (bRawBitplanes)
    {
        err = pCodedBlock->PutBits(1,1); //direct MB
        VC1_ENC_CHECK (err)
        err = pCodedBlock->PutBits(1,1); //skip
        VC1_ENC_CHECK (err)
    }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedBlock->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WritePMB_INTRA (T*                     pCodedMB,
                            bool                   bBitplanesRaw,
                            Ipp32u                 quant,
                            const Ipp16u*          pMVDiffTable,
                            const Ipp16u*          pCBPCYTable,
                            const Ipp32u**         pDCEncTable,
                            const sACTablesSet**   pACTablesSet,
                            sACEscInfo*            pACEscInfo)
{
    UMC::Status     err     =   UMC::UMC_OK;
    Ipp32u          blk;

        VC1_NULL_PTR(pCodedMB)

#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif
    err     = WriteMBHeaderP_INTRA    ( pCodedMB,bBitplanesRaw, pMVDiffTable,pCBPCYTable);
    VC1_ENC_CHECK (err)

    for (blk=0; blk<6; blk++)
    {
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u BlkStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif
        err = WriteBlockDC(pCodedMB,pDCEncTable[blk],quant,blk);
        VC1_ENC_CHECK (err)

STATISTICS_START_TIME(m_TStat->AC_Coefs_StartTime);
        err = WriteBlockAC(pCodedMB,pACTablesSet[blk], pACEscInfo,blk);
STATISTICS_END_TIME(m_TStat->AC_Coefs_StartTime, m_TStat->AC_Coefs_EndTime, m_TStat->AC_Coefs_TotalTime);
        VC1_ENC_CHECK (err)
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] += (Ipp16u)pCodedMB->GetCurrBit()- BlkStart;
#endif
    }

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

template <class T>
UMC::Status WritePMBMixed_INTRA (T*                     pCodedMB,
                                 bool                   bBitplanesRaw,
                                 Ipp32u                 quant,
                                 const Ipp16u*          pMVDiffTable,
                                 const Ipp16u*          pCBPCYTable,
                                 const Ipp32u**         pDCEncTable,
                                 const sACTablesSet**   pACTablesSet,
                                 sACEscInfo*            pACEscInfo)
{
     UMC::Status     err     =   UMC::UMC_OK;
#ifdef VC1_ME_MB_STATICTICS
    Ipp16u MBStart = (Ipp16u)pCodedMB->GetCurrBit();
#endif
    if (bBitplanesRaw)
    {
        UMC::Status     err     =   UMC::UMC_OK;
        err = pCodedMB->PutBits(0,1);
        VC1_ENC_CHECK (err)
    }

    err = WritePMB_INTRA(pCodedMB, bBitplanesRaw,quant,pMVDiffTable,pCBPCYTable,pDCEncTable, pACTablesSet, pACEscInfo);

#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->whole = (Ipp16u)pCodedMB->GetCurrBit()- MBStart;
#endif
    return err;
}

/*-------------------------------------------------------------------*/

template <class T>
UMC::Status   WriteBlockDC  (    T*                pCodedBlock,
                                    const Ipp32u*     pEncTable,
                                    Ipp32u            quant,
                                    Ipp32s             blk)
{
    UMC::Status UMCSts = UMC::UMC_OK;
#ifdef VC1_ME_MB_STATICTICS
    Ipp32u BlkStart = pCodedBlock->GetCurrBit();;
#endif
    switch (quant)
    {
    case 1:
            UMCSts = WriteDCQuant1    (m_iDC[blk],pEncTable, pCodedBlock);
        break;
    case 2:
            UMCSts = WriteDCQuant2    (m_iDC[blk],pEncTable, pCodedBlock);
        break;
    default:
            UMCSts = WriteDCQuantOther (m_iDC[blk],pEncTable, pCodedBlock);
    }
#ifdef VC1_ME_MB_STATICTICS
    m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk] +(Ipp16u)(pCodedBlock->GetCurrBit() - BlkStart);
#endif

    return UMCSts;
}
template <class T>  UMC::Status   WriteBlockAC( T*                      pCodedBlock,
                                                const sACTablesSet*     pACTablesSet,
                                                sACEscInfo*             pACEscInfo,
                                                Ipp32u                  blk)
{
    UMC::Status     err                    = UMC::UMC_OK;
    Ipp32s          i                      = 0;
    static const Ipp32s    nSubblk [4]     = {1,2,2,4};

    const Ipp32u    *pEncTable   = pACTablesSet->pEncTable;
    Ipp8u           nPairsBlock  = 0;

    VC1_NULL_PTR(pCodedBlock)

#ifdef VC1_ME_MB_STATICTICS
      Ipp32u BlkStart = pCodedBlock->GetCurrBit();
#endif
    for (Ipp32s nSubblock=0; nSubblock<nSubblk[m_tsType[blk]]; nSubblock++)
    {
        const Ipp8u     *pTableDR    = pACTablesSet->pTableDR;
        const Ipp8u     *pTableDL    = pACTablesSet->pTableDL;
        const Ipp8u     *pTableInd   = pACTablesSet->pTableInd;
        Ipp8u           nPairs       = m_nPairs[blk][nSubblock];

        if (nPairs == 0)
            continue;

        // put codes into bitstream
        i = 0;
        for (Ipp32s not_last = 1; not_last>=0; not_last--)
        {
            for (; i < nPairs - not_last; i++)
            {
                bool    sign = false;
                Ipp8u   run  = m_uRun [blk] [i+nPairsBlock];
                Ipp16s  lev  = m_iLevel[blk][i+nPairsBlock];

                Ipp8u mode = GetMode( run, lev, pTableDR, pTableDL, sign);

#ifdef VC1_ENC_DEBUG_ON
                pDebug->SetRLMode(mode, blk, i+nPairsBlock);
#endif

                switch (mode)
                {
                    case 3:
                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(0,2);                       //mode
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(!not_last,1);               //last
                        VC1_ENC_CHECK (err)
                        if ((!pACEscInfo->levelSize) && (!pACEscInfo->runSize))
                        {
                            pACEscInfo->runSize = 6;
                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*11 +1]==0)? 8:11;
                            pACEscInfo->levelSize = (pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize + 1]<=0) ?
                                                            2 : pACEscInfo->levelSize;
                            err = pCodedBlock->PutBits(pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize],
                                                    pACEscInfo->pLSizeTable[2*pACEscInfo->levelSize +1]);
                            VC1_ENC_CHECK (err)
                            err = pCodedBlock->PutBits(pACEscInfo->runSize - 3, 2);
                            VC1_ENC_CHECK (err)
                        }
                        err = pCodedBlock->PutBits(run,pACEscInfo->runSize);
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(sign, 1);
                        VC1_ENC_CHECK (err)
                        if (lev>((1<<pACEscInfo->levelSize)-1))
                        {
                            lev =(1<<pACEscInfo->levelSize)-1;
                            m_iLevel[blk][i+nPairsBlock] = lev;
                        }
                        err = pCodedBlock->PutBits(lev,pACEscInfo->levelSize);
                        VC1_ENC_CHECK (err)
                        break;
                    case 2:
                    case 1:
                        err = pCodedBlock->PutBits(pEncTable[0], pEncTable[1]); //ESCAPE
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(1,mode);                     //mode
                        VC1_ENC_CHECK (err)
                     case 0:
                        Ipp16s index = pTableInd[run] + lev;
                        err = pCodedBlock->PutBits(pEncTable[2*index], pEncTable[2*index + 1]);
                        VC1_ENC_CHECK (err)
                        err = pCodedBlock->PutBits(sign, 1);
                        VC1_ENC_CHECK (err)
                        break;

                }
            }

            pTableDR    = pACTablesSet->pTableDRLast;
            pTableDL    = pACTablesSet->pTableDLLast;
            pTableInd   = pACTablesSet->pTableIndLast;

        }
        nPairsBlock = nPairsBlock + m_nPairs[blk][nSubblock];
    }
#ifdef VC1_ME_MB_STATICTICS
        m_MECurMbStat->coeff[blk] = m_MECurMbStat->coeff[blk] + (Ipp16u)(pCodedBlock->GetCurrBit() - BlkStart);
#endif
    return UMC::UMC_OK;
}


protected:
   template <class T>
   UMC::Status   WriteVSTSBlk (    T*                      pCodedBlock,
                                   const Ipp8u            (*pTTBLKVLC)[6],
                                   const Ipp8u*            pSBP4x4Table,
                                   bool                    bVTS,
                                   Ipp32u                   blk)
   {
       UMC::Status     err                    = UMC::UMC_OK;
       eTransformType  type                   = m_tsType[blk];

       VC1_NULL_PTR(pCodedBlock)

       if ( m_uiMBCBPCY == 0)
            return err;

        if (bVTS && !m_bMBTSType && blk!= m_uiFirstCodedBlock)
        {
            Ipp8u          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0);

            subPat = (subPat>0)?subPat-1:subPat;

            assert(subPat < 4);

            //block level transform type
            err = pCodedBlock->PutBits(pTTBLKVLC[type][2*subPat],pTTBLKVLC[type][2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        if (type == VC1_ENC_4x4_TRANSFORM)
        {
            Ipp8u          subPat  = ((m_nPairs[blk][0]>0)<<3) + ((m_nPairs[blk][1]>0)<<2)+
                                     ((m_nPairs[blk][2]>0)<<1) +  (m_nPairs[blk][3]>0);
             //VC1_ENC_4x4_TRANSFORM
            err = pCodedBlock->PutBits(pSBP4x4Table[2*subPat],pSBP4x4Table[2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        else if (type != VC1_ENC_8x8_TRANSFORM &&
                (m_bMBTSType && blk!= m_uiFirstCodedBlock || !bVTS))
        {
            Ipp8u          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0) - 1;
            //if MB level or frame level
            err = pCodedBlock->PutBits(SubPattern8x4_4x8VLC[2*subPat],SubPattern8x4_4x8VLC[2*subPat+1] );
            VC1_ENC_CHECK (err)
        }
        return err;

   }

template <class T>  inline
UMC::Status WriteMVDataPIntra(T* pCodedMB, const Ipp16u* table)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    bool            NotSkip =   (m_uiMBCBPCY != 0);
    Ipp8u           index   =   37*NotSkip + 36 - 1;

    VC1_NULL_PTR  (pCodedMB)

    ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
    VC1_ENC_CHECK (ret)

    //printf("MVDiff index = %d\n", index);
    //printf("DMV_X  = %d, DMV_Y  = %d\n", 0, 0);

    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataPIntra(T* pCodedMB, const Ipp16u* table, Ipp32u blockNum)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    bool            NotSkip =   ((m_uiMBCBPCY &(1<<VC_ENC_PATTERN_POS(blockNum))) != 0);
    Ipp8u           index   =   37*NotSkip + 36 - 1;

    VC1_NULL_PTR  (pCodedMB)

    ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
    VC1_ENC_CHECK (ret)

    //printf("MVDiff index = %d\n", index);
    //printf("DMV_X  = %d, DMV_Y  = %d\n", 0, 0);

    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInter(T* pCodedMB, const Ipp16u* table, Ipp8u rangeIndex, bool   NotSkip, bool bMVHalf, Ipp32u blockNum = 0, bool bForward = true)
{
    UMC::Status     ret     =   UMC::UMC_OK;
    Ipp16s          index   =   0;
    Ipp16s          dx      =   m_dMV[bForward][blockNum].x;
    Ipp16s          dy      =   m_dMV[bForward][blockNum].y;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);
    Ipp8u           limit   =   (bMVHalf)? VC1_ENC_HALF_MV_LIMIT : VC1_ENC_MV_LIMIT;


    VC1_NULL_PTR  (pCodedMB)

    dx = dx*(1 -2*signX);
    dy = dy*(1 -2*signY);

    if (bMVHalf)
    {
        dx = dx >> 1;
        dy = dy >> 1;
    }

    index = (dx < limit && dy < limit)? 6*MVSizeOffset[3*dy]+ MVSizeOffset[3*dx] - 1:35;

    if (index < 34)
    {
        Ipp32s diffX = dx - MVSizeOffset[3*dx+1];
        Ipp32s diffY = dy - MVSizeOffset[3*dy+1];
        Ipp8u sizeX = MVSizeOffset[3*dx+2];
        Ipp8u sizeY = MVSizeOffset[3*dy+2];

        diffX =  (diffX<<1)+signX;
        diffY =  (diffY<<1)+signY;

        if (bMVHalf)
        {
            sizeX -= sizeX>>3;
            sizeY -= sizeY>>3;
        }
        index+= 37*NotSkip;

        ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(diffX, sizeX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(diffY, sizeY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
         // escape mode
        Ipp8u sizeX = longMVLength[2*rangeIndex];
        Ipp8u sizeY = longMVLength[2*rangeIndex+1];


        dx = ((1<<sizeX)-1)& (m_dMV[bForward][blockNum].x);
        dy = ((1<<sizeY)-1)& (m_dMV[bForward][blockNum].y);

        if (bMVHalf)
        {
            dx= dx>>1;
            dy= dy>>1;
        }

        index= 35 + 37*NotSkip - 1;

        ret = pCodedMB->PutBits(table[2*index], table[2*index+1]);
        VC1_ENC_CHECK (ret);

        if (bMVHalf)
        {
            sizeX -= sizeX>>3;
            sizeY -= sizeY>>3;
        }
        ret = pCodedMB->PutBits(dx, sizeX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, sizeY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif
    }
    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInterField1Ref(T* pCodedMB, Ipp8u rangeIndex,
                                      sMVFieldInfo* pMVFieldInfo,
                                      bool bMVHalf,
                                      Ipp32u blockNum=0,
                                      bool bForward=true )
{
    UMC::Status     ret     =   UMC::UMC_OK;
    Ipp16s          indexX  =   0;
    Ipp16s          indexY  =   0;
    Ipp16s          index   =   0;

    Ipp16s          dx      =   m_dMV[bForward][blockNum].x;
    Ipp16s          dy      =   m_dMV[bForward][blockNum].y;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);

    VC1_NULL_PTR  (pCodedMB)

    dx = (signX)? -dx:dx;
    dy = (signY)? -dy:dy;

    if (bMVHalf)
    {
        dx = dx>>1;
        dy = dy>>1;
    }

    index = (dx < pMVFieldInfo->limitX && dy < pMVFieldInfo->limitY)?
                9*(indexY = pMVFieldInfo->pMVSizeOffsetFieldIndexY[dy])+
                  (indexX = pMVFieldInfo->pMVSizeOffsetFieldIndexX[dx]) - 1:71;

    if (index < 71)
    {
        dx = dx - pMVFieldInfo->pMVSizeOffsetFieldX[indexX];
        dy = dy - pMVFieldInfo->pMVSizeOffsetFieldY[indexY];

        dx =  (dx<<1)+signX;
        dy =  (dy<<1)+signY;

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX + pMVFieldInfo->bExtendedX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY + pMVFieldInfo->bExtendedY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
         // escape mode
        index       = 71;

        indexX = longMVLength[2*rangeIndex];
        indexY = longMVLength[2*rangeIndex+1];

        dx = ((1<<indexX)-1)& ((signX)? -dx:dx);
        dy = ((1<<indexY)-1)& ((signY)? -dy:dy);

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX );
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY );
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif
    }
    return ret;
}
template <class T>  inline
UMC::Status WriteMVDataInterField2Ref(T* pCodedMB, Ipp8u rangeIndex,
                                      sMVFieldInfo* pMVFieldInfo,
                                      bool bMVHalf,
                                      Ipp32u blockNum=0,
                                      bool bForward=true )
{
    UMC::Status     ret     =   UMC::UMC_OK;
    Ipp16s          indexX  =   0;
    Ipp16s          indexY  =   0;
    Ipp16s          index   =   0;

    Ipp16s          dx      =   m_dMV[bForward][blockNum].x;
    Ipp16s          dy      =   m_dMV[bForward][blockNum].y;
    bool            bNonDominant =   m_dMV[bForward][blockNum].bSecond;
    bool            signX   =   (dx<0);
    bool            signY   =   (dy<0);

    VC1_NULL_PTR  (pCodedMB)

    dx = (signX)? -dx:dx;
    dy = (signY)? -dy:dy;

    if (bMVHalf)
    {
        dx = dx>>1;
        dy = dy>>1;
    }
    index = (dx < pMVFieldInfo->limitX && dy < pMVFieldInfo->limitY)?
                9*(((indexY = pMVFieldInfo->pMVSizeOffsetFieldIndexY[dy])<<1) + bNonDominant)+
                    (indexX = pMVFieldInfo->pMVSizeOffsetFieldIndexX[dx]) - 1:125;

    if (index < 125)
    {
        dx = dx - pMVFieldInfo->pMVSizeOffsetFieldX[indexX];
        dy = dy - pMVFieldInfo->pMVSizeOffsetFieldY[indexY];

        dx =  (dx <<1)+signX;
        dy =  (dy <<1)+signY;

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX + pMVFieldInfo->bExtendedX);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY + pMVFieldInfo->bExtendedY);
        VC1_ENC_CHECK (ret);

#ifdef VC1_ENC_DEBUG_ON
        if(bMVHalf)
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x/2, m_dMV[bForward][blockNum].y/2,(!bForward),blockNum);
        else
            pDebug->SetMVDiff(m_dMV[bForward][blockNum].x, m_dMV[bForward][blockNum].y,(!bForward),blockNum);
#endif
    }
    else
    {
        // escape mode

        Ipp32s     y = ((((signY)? -dy:dy) - bNonDominant)<<1) + bNonDominant;

        index       = 125;


        indexX = longMVLength[2*rangeIndex];
        indexY = longMVLength[2*rangeIndex+1];

        dx = (Ipp16s)(((1<<indexX)-1)&  ((signX)? -dx: dx));
        dy = (Ipp16s)(((1<<indexY)-1)&  (y));

#ifdef VC1_ENC_DEBUG_ON
    if(bMVHalf)
        pDebug->SetMVDiff(dx, dy/2,(!bForward),blockNum);
    else
        pDebug->SetMVDiff(dx, dy,(!bForward),blockNum);
#endif

        ret = pCodedMB->PutBits(pMVFieldInfo->pMVModeField1RefTable_VLC[2*index],
                                pMVFieldInfo->pMVModeField1RefTable_VLC[2*index+1]);
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dx, indexX );
        VC1_ENC_CHECK (ret);

        ret = pCodedMB->PutBits(dy, indexY );
        VC1_ENC_CHECK (ret);

    }
    return ret;
}
template <class T>
UMC::Status   WriteVSTSMB (   T*           pCodedBlock,
                             const Ipp16s  (*pTTMBVLC)[4][6])
{
    UMC::Status err           = UMC::UMC_OK;
    Ipp32u interPattern  = m_uiMBCBPCY & (~m_uiIntraPattern);
    Ipp32s blk;

    VC1_NULL_PTR(pCodedBlock)

    for(blk=0;blk<6;blk++)
    {
        if (interPattern & (1<<VC_ENC_PATTERN_POS(blk))) break;
    }

    m_uiFirstCodedBlock    = (Ipp8u)blk;
    eTransformType MBtype  = m_tsType[blk];

#ifdef VC1_ENC_DEBUG_ON
    pDebug->SetVTSType(m_tsType);
#endif

    Ipp8u          subPat  = ((m_nPairs[blk][0]>0)<<1) + (m_nPairs[blk][1]>0);
    subPat = (subPat>0)?subPat-1:subPat;

    err = pCodedBlock->PutBits(pTTMBVLC[m_bMBTSType][MBtype][2*subPat],pTTMBVLC[m_bMBTSType][MBtype][2*subPat+1] );
    return err;
}
};
 
/*------------------------------------------------------------------------------------------*/
inline UMC::Status  copyChromaBlockYV12 (Ipp8u*  pURow,  Ipp8u* pVRow,   Ipp32u UVRowStep, 
                                         Ipp16s* pUBlock,Ipp16s* pVBlock,Ipp32u UVBlockStep,
                                         Ipp32u  nPos)
{
    IppiSize roiSize = {8, 8};
    _own_Copy8x8_16x16_8u16s(pURow + (nPos <<3), UVRowStep, pUBlock, UVBlockStep, roiSize);
    _own_Copy8x8_16x16_8u16s(pVRow + (nPos <<3), UVRowStep, pVBlock, UVBlockStep, roiSize);

    return UMC::UMC_OK;
}

inline UMC::Status  copyChromaBlockNV12 (Ipp8u*  pUVRow,  Ipp8u* /*pVRow*/,  Ipp32u UVRowStep, 
                                        Ipp16s* pUBlock, Ipp16s* pVBlock,    Ipp32u UVBlockStep,
                                        Ipp32u  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlane ++);
            *(pV ++) = *(pPlane ++);
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  copyDiffChromaBlockYV12 (Ipp8u*  pURowS,  Ipp8u* pVRowS,   Ipp32u UVRowStepS, 
                                            Ipp8u*   pURowR,  Ipp8u* pVRowR,   Ipp32u UVRowStepR, 
                                            Ipp16s*  pUBlock,Ipp16s* pVBlock,  Ipp32u UVBlockStep,
                                            Ipp32u  nPosS, Ipp32u nPosR)
{
    
    _own_ippiGetDiff8x8_8u16s_C1  ( pURowS + (nPosS <<3), UVRowStepS, pURowR + (nPosR<<3), UVRowStepR,
                                    pUBlock, UVBlockStep,0,0,0,0);
    _own_ippiGetDiff8x8_8u16s_C1  ( pVRowS + (nPosS <<3), UVRowStepS, pVRowR + (nPosR<<3), UVRowStepR,
                                    pVBlock, UVBlockStep,0,0,0,0);

    return UMC::UMC_OK;
}
inline UMC::Status  copyDiffChromaBlockNV12 (Ipp8u*   pUVRowS,   Ipp8u* /*pVRowS*/,    Ipp32u UVRowStepS, 
                                             Ipp8u*   pUVRowR,   Ipp8u* /*pVRowR*/,    Ipp32u UVRowStepR, 
                                             Ipp16s*  pUBlock,   Ipp16s* pVBlock,      Ipp32u UVBlockStep,
                                             Ipp32u  nPosS, Ipp32u nPosR)
{
    pUVRowS += (nPosS << 4);
    pUVRowR += (nPosR << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU     = pUBlock;
        Ipp16s* pV     = pVBlock;
        Ipp8u* pPlaneS = pUVRowS;
        Ipp8u* pPlaneR = pUVRowR;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlaneS ++) - *(pPlaneR ++);
            *(pV ++) = *(pPlaneS ++) - *(pPlaneR ++);
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowS +=  UVRowStepS;
        pUVRowR +=  UVRowStepR;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  copyBDiffChromaBlockYV12 (Ipp8u*   pURowS,  Ipp8u* pVRowS,   Ipp32u UVRowStepS, 
                                              Ipp8u*   pURowF,  Ipp8u* pVRowF,   Ipp32u UVRowStepF, 
                                              Ipp8u*   pURowB,  Ipp8u* pVRowB,   Ipp32u UVRowStepB,  
                                              Ipp16s*  pUBlock,Ipp16s* pVBlock,  Ipp32u UVBlockStep,
                                              Ipp32u  nPosS, Ipp32u nPosFB)
{

    ippiGetDiff8x8B_8u16s_C1  (pURowS  + (nPosS <<3), UVRowStepS,   pURowF  + (nPosFB <<3),   UVRowStepF, 0,
                                                                    pURowB  + (nPosFB <<3),   UVRowStepB, 0,
                                                                    pUBlock, UVBlockStep,0);

    ippiGetDiff8x8B_8u16s_C1  (pVRowS  + (nPosS <<3), UVRowStepS,   pVRowF  + (nPosFB <<3),   UVRowStepF, 0,
                                                                    pVRowB  + (nPosFB <<3),   UVRowStepB, 0,
                                                                    pVBlock, UVBlockStep,0);

    return UMC::UMC_OK;
}
inline UMC::Status  copyBDiffChromaBlockNV12 (Ipp8u*   pUVRowS,   Ipp8u* /*pVRowS*/,    Ipp32u UVRowStepS, 
                                              Ipp8u*   pUVRowF,   Ipp8u* /*pVRowF*/,    Ipp32u UVRowStepF,
                                              Ipp8u*   pUVRowB,   Ipp8u* /*pVRowB*/,    Ipp32u UVRowStepB ,
                                              Ipp16s*  pUBlock,   Ipp16s* pVBlock,      Ipp32u UVBlockStep,
                                              Ipp32u  nPosS, Ipp32u nPosFB)
{
    pUVRowS += (nPosS  << 4);
    pUVRowF += (nPosFB << 4);
    pUVRowB += (nPosFB << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU     = pUBlock;
        Ipp16s* pV     = pVBlock;
        Ipp8u* pPlaneS = pUVRowS;
        Ipp8u* pPlaneF = pUVRowF;
        Ipp8u* pPlaneB = pUVRowB;

        for (int j = 0; j <8; j++)
        {
            *(pU ++) = *(pPlaneS ++) - ((*(pPlaneF ++) + *(pPlaneB ++) + 1)>>1);
            *(pV ++) = *(pPlaneS ++) - ((*(pPlaneF ++) + *(pPlaneB ++) + 1)>>1);
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowS +=  UVRowStepS;
        pUVRowF +=  UVRowStepF;
        pUVRowB +=  UVRowStepB;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteChromaBlockYV12 (Ipp8u*  pURow,  Ipp8u* pVRow,   Ipp32u UVRowStep, 
                                         Ipp16s* pUBlock,Ipp16s* pVBlock,Ipp32u UVBlockStep,
                                         Ipp32u  nPos)
{
    IppiSize roiSize = {8, 8};
    _own_ippiConvert_16s8u_C1R(pUBlock, UVBlockStep,pURow + (nPos <<3), UVRowStep,roiSize);
    _own_ippiConvert_16s8u_C1R(pVBlock, UVBlockStep,pVRow + (nPos <<3), UVRowStep,roiSize);
    return UMC::UMC_OK;
}
inline UMC::Status  pasteChromaBlockNV12 (Ipp8u*  pUVRow,  Ipp8u* /*pVRow*/,   Ipp32u UVRowStep, 
                                          Ipp16s* pUBlock, Ipp16s* pVBlock,    Ipp32u UVBlockStep,
                                          Ipp32u  nPos)
{
    pUVRow += (nPos << 4);
    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane = pUVRow;

        for (int j = 0; j <8; j++)
        {
            Ipp16s U = *(pU ++);
            Ipp16s V = *(pV ++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (Ipp8u)U;
            *(pPlane ++) = (Ipp8u)V;
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRow +=  UVRowStep;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumChromaBlockYV12 (Ipp8u*  pURowRef,  Ipp8u* pVRowRef,   Ipp32u UVRowStepRef, 
                                             Ipp8u*  pURowDst,  Ipp8u* pVRowDst,   Ipp32u UVRowStepDst,
                                             Ipp16s* pUBlock,   Ipp16s* pVBlock,   Ipp32u UVBlockStep,
                                             Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    _own_Add8x8_8u16s(pURowRef + (nPosRef <<3), UVRowStepRef,pUBlock,UVBlockStep, pURowDst + (nPosDst <<3), UVRowStepDst, 0, 0);
    _own_Add8x8_8u16s(pVRowRef + (nPosRef <<3), UVRowStepRef,pVBlock,UVBlockStep, pVRowDst + (nPosDst <<3), UVRowStepDst, 0, 0);

   return UMC::UMC_OK;
}
inline UMC::Status  pasteSumChromaBlockNV12 (Ipp8u*  pUVRowRef,  Ipp8u* /*pVRowRef*/,   Ipp32u UVRowStepRef, 
                                             Ipp8u*  pUVRowDst,  Ipp8u* /*pVRowDst*/,   Ipp32u UVRowStepDst,
                                             Ipp16s* pUBlock,   Ipp16s* pVBlock,   Ipp32u UVBlockStep,
                                             Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    pUVRowDst += (nPosDst << 4);
    pUVRowRef += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane    = pUVRowDst;
        Ipp8u* pPlaneRef = pUVRowRef;


        for (int j = 0; j <8; j++)
        {
            Ipp16s U = *(pU ++) + *(pPlaneRef++);
            Ipp16s V = *(pV ++) + *(pPlaneRef++);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (Ipp8u)U;
            *(pPlane ++) = (Ipp8u)V;
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowDst +=  UVRowStepDst;
        pUVRowRef +=  UVRowStepRef;
    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumChromaBlockYV12 (Ipp8u*  pURowRefF,  Ipp8u* pVRowRefF, Ipp32u UVRowStepRefF,
                                               Ipp8u*  pURowRefB,  Ipp8u* pVRowRefB, Ipp32u UVRowStepRefB, 
                                               Ipp8u*  pURowDst,   Ipp8u* pVRowDst,   Ipp32u UVRowStepDst,
                                               Ipp16s* pUBlock,    Ipp16s* pVBlock,   Ipp32u UVBlockStep,
                                               Ipp32u  nPosRef,    Ipp32u  nPosDst)
{
    ippiMC8x8B_8u_C1     (  pURowRefF + (nPosRef <<3),UVRowStepRefF, 0,
                            pURowRefB + (nPosRef <<3),UVRowStepRefB, 0,
                            pUBlock, UVBlockStep,
                            pURowDst+(nPosDst <<3), UVRowStepDst,  0);

    ippiMC8x8B_8u_C1     (  pVRowRefF + (nPosRef <<3),UVRowStepRefF, 0,
                            pVRowRefB + (nPosRef <<3),UVRowStepRefB, 0,
                            pVBlock, UVBlockStep,
                            pVRowDst+(nPosDst <<3), UVRowStepDst,  0);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumChromaBlockNV12 (Ipp8u*  pUVRowRefF, Ipp8u* /*pVRowRefF*/,   Ipp32u UVRowStepRefF, 
                                              Ipp8u*  pUVRowRefB, Ipp8u* /*pVRowRefF*/,   Ipp32u UVRowStepRefB, 
                                              Ipp8u*  pUVRowDst,  Ipp8u* /*pVRowDst*/,   Ipp32u UVRowStepDst,
                                              Ipp16s* pUBlock,    Ipp16s* pVBlock,   Ipp32u UVBlockStep,
                                              Ipp32u  nPosRef,    Ipp32u  nPosDst)
{
    pUVRowDst += (nPosDst << 4);
    pUVRowRefF += (nPosRef << 4);
    pUVRowRefB += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        Ipp16s* pU = pUBlock;
        Ipp16s* pV = pVBlock;
        Ipp8u* pPlane    = pUVRowDst;
        Ipp8u* pPlaneRefF = pUVRowRefF;
        Ipp8u* pPlaneRefB = pUVRowRefB;


        for (int j = 0; j <8; j++)
        {
            Ipp16s U = *(pU ++) + ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);
            Ipp16s V = *(pV ++) + ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);

            U = (U < 0)  ? 0   : U;
            U = (U > 255)? 255 : U;
            V = (V < 0)  ? 0   : V;
            V = (V >255) ? 255 : V;

            *(pPlane ++) = (Ipp8u)U;
            *(pPlane ++) = (Ipp8u)V;
        }
        pUBlock = (Ipp16s*)((Ipp8u*)pUBlock + UVBlockStep);
        pVBlock = (Ipp16s*)((Ipp8u*)pVBlock + UVBlockStep);
        pUVRowDst +=  UVRowStepDst;
        pUVRowRefF +=  UVRowStepRefF;
        pUVRowRefB +=  UVRowStepRefB;

    }
    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumSkipChromaBlockYV12 (   Ipp8u*  pURowRef,  Ipp8u* pVRowRef,   Ipp32u UVRowStepRef, 
                                                    Ipp8u*  pURowDst,  Ipp8u* pVRowDst,   Ipp32u UVRowStepDst,                                             
                                                    Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    IppiSize roiSize = {8, 8};
    ippiCopy_8u_C1R(pURowRef + (nPosRef <<3), UVRowStepRef, pURowDst + (nPosDst <<3), UVRowStepDst,roiSize);
    ippiCopy_8u_C1R(pVRowRef + (nPosRef <<3), UVRowStepRef, pVRowDst + (nPosDst <<3), UVRowStepDst,roiSize);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteSumSkipChromaBlockNV12 (   Ipp8u*  pUVRowRef,  Ipp8u* /*pVRowRef*/,   Ipp32u UVRowStepRef, 
                                                    Ipp8u*  pUVRowDst,  Ipp8u* /*pVRowDst*/,   Ipp32u UVRowStepDst,
                                                    Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    IppiSize roiSize = {16, 8};

    pUVRowDst += (nPosDst << 4);
    pUVRowRef += (nPosRef << 4);

    ippiCopy_8u_C1R(pUVRowRef, UVRowStepRef, pUVRowDst, UVRowStepDst,roiSize);

    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumSkipChromaBlockYV12 (  Ipp8u*  pURowRefF,  Ipp8u* pVRowRefF,   Ipp32u UVRowStepRefF, 
                                                    Ipp8u*  pURowRefB,  Ipp8u* pVRowRefB,   Ipp32u UVRowStepRefB,
                                                    Ipp8u*  pURowDst,   Ipp8u* pVRowDst,    Ipp32u UVRowStepDst,                                             
                                                    Ipp32u  nPosRef,   Ipp32u  nPosDst)
{
    IppiSize roiSize = {8, 8};
    ippiAverage8x8_8u_C1R(  pURowRefF + (nPosRef <<3), UVRowStepRefF,
                            pURowRefB + (nPosRef <<3), UVRowStepRefB,
                            pURowDst  + (nPosDst <<3), UVRowStepDst);
    ippiAverage8x8_8u_C1R(  pVRowRefF + (nPosRef <<3), UVRowStepRefF,
                            pVRowRefB + (nPosRef <<3), UVRowStepRefB,
                            pVRowDst  + (nPosDst <<3), UVRowStepDst);
 
    return UMC::UMC_OK;
}
inline UMC::Status  pasteBSumSkipChromaBlockNV12 (   Ipp8u*  pUVRowRefF,  Ipp8u* /*pVRowRefF*/,   Ipp32u UVRowStepRefF, 
                                                     Ipp8u*  pUVRowRefB,  Ipp8u* /*pVRowRefB*/,   Ipp32u UVRowStepRefB, 
                                                     Ipp8u*  pUVRowDst,   Ipp8u* /*pVRowDst*/,   Ipp32u UVRowStepDst,
                                                     Ipp32u  nPosRef,     Ipp32u  nPosDst)
{
     //ippiAverage16x8_8u_C1R

    pUVRowDst += (nPosDst << 4);
    pUVRowRefF += (nPosRef << 4);
    pUVRowRefB += (nPosRef << 4);

    for (int i=0; i<8; i++)
    {
        Ipp8u* pPlane    = pUVRowDst;
        Ipp8u* pPlaneRefF = pUVRowRefF;
        Ipp8u* pPlaneRefB = pUVRowRefB;

        for (int j = 0; j < 16; j++)
        {
            *(pPlane ++) = ((*(pPlaneRefF++) + *(pPlaneRefB++) + 1)>>1);
        }
        pUVRowDst +=  UVRowStepDst;
        pUVRowRefF +=  UVRowStepRefF;
        pUVRowRefB +=  UVRowStepRefB;

    }

    return UMC::UMC_OK;
}

class VC1EncoderMBData
{
public:

    Ipp16s*                     m_pBlock[VC1_ENC_NUMBER_OF_BLOCKS];
    Ipp32u                      m_uiBlockStep[VC1_ENC_NUMBER_OF_BLOCKS];

public:

   UMC::Status InitBlocks(Ipp16s* pBuffer);
   void Reset()
   {
       assert(m_pBlock[0]!=0);
       memset(m_pBlock[0],0,VC1_ENC_BLOCK_SIZE*VC1_ENC_NUMBER_OF_BLOCKS*sizeof(Ipp16s));
   }


   VC1EncoderMBData()
   {
        memset(m_pBlock,        0,sizeof(Ipp16s*)*VC1_ENC_NUMBER_OF_BLOCKS);
        memset(m_uiBlockStep,   0,sizeof(Ipp32u) *VC1_ENC_NUMBER_OF_BLOCKS);

   }
   
   virtual UMC::Status CopyMBProg(Ipp8u* pY, Ipp32u stepY, Ipp8u* pU, Ipp8u* pV, Ipp32u stepUV, Ipp32u nPos)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;


        /* luma block */
        IppiSize roiSize = {16, 16};
       _own_Copy8x8_16x16_8u16s(pY + (nPos <<4), stepY, m_pBlock[0], m_uiBlockStep[0], roiSize); 
        /* chroma block*/
       copyChromaBlockYV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

       return UMC::UMC_OK;
   }
   virtual UMC::Status CopyDiffMBProg(  Ipp8u*   pYS,  Ipp32u stepYS, 
                                        Ipp8u*   pUS,  Ipp8u* pVS,   Ipp32u stepUVS, 
                                        Ipp8u*   pYR,  Ipp32u stepYR,
                                        Ipp8u*   pUR,  Ipp8u* pVR,   Ipp32u stepUVR,                                 
                                        Ipp32u  nPosS, Ipp32u nPosR)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       
       _own_ippiGetDiff16x16_8u16s_C1(  pYS + (nPosS <<4), stepYS,
                                        pYR + (nPosR <<4), stepYR,
                                        m_pBlock[0], m_uiBlockStep[0], 0,0,0,0);

       copyDiffChromaBlockYV12(pUS,pVS,stepUVS,pUR,pVR,stepUVR,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosR);      
       

       return UMC::UMC_OK;
   }
   virtual UMC::Status CopyBDiffMBProg( Ipp8u*   pYS,  Ipp32u stepYS, 
                                        Ipp8u*   pUS,  Ipp8u* pVS,   Ipp32u stepUVS, 
                                        Ipp8u*   pYF,  Ipp32u stepYF,
                                        Ipp8u*   pUF,  Ipp8u* pVF,   Ipp32u stepUVF,
                                        Ipp8u*   pYB,  Ipp32u stepYB,
                                        Ipp8u*   pUB,  Ipp8u* pVB,   Ipp32u stepUVB,
                                        Ipp32u  nPosS, Ipp32u nPosFB)

   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       ippiGetDiff16x16B_8u16s_C1(pYS + (nPosS <<4),stepYS, pYF + (nPosFB <<4), stepYF, 0, pYB + (nPosFB <<4), stepYB, 0, m_pBlock[0], m_uiBlockStep[0],0);                                 

       copyBDiffChromaBlockYV12(pUS,pVS,stepUVS,pUF,pVF,stepUVF,pUB,pVB,stepUVB,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosFB);   

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteMBProg(Ipp8u* pY, Ipp32u stepY, Ipp8u* pU, Ipp8u* pV, Ipp32u stepUV, Ipp32u nPos)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;
 
       /* luma block */
       IppiSize roiSize = {16, 16};
       _own_ippiConvert_16s8u_C1R(m_pBlock[0], m_uiBlockStep[0],pY + (nPos <<4), stepY,roiSize);
       /* chroma block */
       pasteChromaBlockYV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

       return UMC::UMC_OK;
   }

   virtual UMC::Status PasteSumMBProg(  Ipp8u* pYRef, Ipp32u stepYRef, 
                                        Ipp8u* pURef, Ipp8u* pVRef, Ipp32u stepUVRef, 
                                        Ipp8u* pYDst, Ipp32u stepYDst, 
                                        Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                        Ipp32u nPosRef, Ipp32u  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       _own_Add16x16_8u16s(pYRef + (nPosRef <<4),stepYRef,m_pBlock[0],m_uiBlockStep[0],
                           pYDst + (nPosDst <<4) ,stepYDst, 0, 0);
      
       /* chroma block */
       pasteSumChromaBlockYV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteBSumMBProg( Ipp8u* pYRefF, Ipp32u stepYRefF, 
                                        Ipp8u* pURefF, Ipp8u* pVRefF, Ipp32u stepUVRefF, 
                                        Ipp8u* pYRefB, Ipp32u stepYRefB, 
                                        Ipp8u* pURefB, Ipp8u* pVRefB, Ipp32u stepUVRefB, 
                                        Ipp8u* pYDst, Ipp32u stepYDst, 
                                        Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                        Ipp32u nPosRef, Ipp32u  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       ippiMC16x16B_8u_C1   (pYRefF + (nPosRef <<4),stepYRefF, 0,
                             pYRefB + (nPosRef <<4),stepYRefB, 0,
                             m_pBlock[0],m_uiBlockStep[0],
                             pYDst + (nPosDst <<4) ,stepYDst, 0);

       /* chroma block */
       pasteBSumChromaBlockYV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB, pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

       return UMC::UMC_OK;
   }

   virtual UMC::Status PasteSumSkipMBProg(  Ipp8u* pYRef, Ipp32u stepYRef, 
                                            Ipp8u* pURef, Ipp8u* pVRef, Ipp32u stepUVRef, 
                                            Ipp8u* pYDst, Ipp32u stepYDst, 
                                            Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                            Ipp32u nPosRef, Ipp32u  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */
       IppiSize roiSize = {16, 16};
       ippiCopy_8u_C1R(pYRef + (nPosRef <<4), stepYRef, pYDst + (nPosDst <<4) ,stepYDst,roiSize);
       
       /* chroma block */
       pasteSumSkipChromaBlockYV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

       return UMC::UMC_OK;
   }
   virtual UMC::Status PasteBSumSkipMBProg( Ipp8u* pYRefF, Ipp32u stepYRefF, 
                                            Ipp8u* pURefF, Ipp8u* pVRefF, Ipp32u stepUVRefF,
                                            Ipp8u* pYRefB, Ipp32u stepYRefB, 
                                            Ipp8u* pURefB, Ipp8u* pVRefB, Ipp32u stepUVRefB,
                                            Ipp8u* pYDst, Ipp32u stepYDst, 
                                            Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                            Ipp32u nPosRef, Ipp32u  nPosDst)
   {
       if (!m_pBlock[0])
           return UMC::UMC_ERR_NOT_INITIALIZED;

       /* luma block */

       ippiAverage16x16_8u_C1R(pYRefF + (nPosRef <<4), stepYRefF, pYRefB + (nPosRef <<4),stepYRefB, pYDst + (nPosDst <<4),  stepYDst);
 
       /* chroma block */
       pasteBSumSkipChromaBlockYV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

       return UMC::UMC_OK;
   }

   inline static bool IsCodedBlock(Ipp8u MBPattern, Ipp32s blk)
   {
        return ((MBPattern &(1 << (5 - blk)))!=0);
   }
};

class VC1EncoderMBDataNV12: public VC1EncoderMBData
{
public:

    virtual UMC::Status CopyMBProg(Ipp8u* pY, Ipp32u stepY, Ipp8u* pU, Ipp8u* pV, Ipp32u stepUV, Ipp32u nPos)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;


        /* luma block */
        IppiSize roiSize = {16, 16};
        _own_Copy8x8_16x16_8u16s(pY + (nPos <<4), stepY, m_pBlock[0], m_uiBlockStep[0], roiSize); 
        /* chroma block*/
        copyChromaBlockNV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

        return UMC::UMC_OK;
    }
    virtual UMC::Status CopyDiffMBProg( Ipp8u*   pYS,  Ipp32u stepYS, 
                                        Ipp8u*   pUS,  Ipp8u* pVS,   Ipp32u stepUVS, 
                                        Ipp8u*   pYR,  Ipp32u stepYR,
                                        Ipp8u*   pUR,  Ipp8u* pVR,   Ipp32u stepUVR,                                 
                                        Ipp32u  nPosS, Ipp32u nPosR)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;


        _own_ippiGetDiff16x16_8u16s_C1( pYS + (nPosS <<4), stepYS,
                                        pYR + (nPosR <<4), stepYR,
                                        m_pBlock[0], m_uiBlockStep[0], 0,0,0,0);

        copyDiffChromaBlockNV12(pUS,pVS,stepUVS,pUR,pVR,stepUVR,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosR);      


        return UMC::UMC_OK;
    }
    virtual UMC::Status CopyBDiffMBProg(Ipp8u*   pYS,  Ipp32u stepYS, 
                                        Ipp8u*   pUS,  Ipp8u* pVS,   Ipp32u stepUVS, 
                                        Ipp8u*   pYF,  Ipp32u stepYF,
                                        Ipp8u*   pUF,  Ipp8u* pVF,   Ipp32u stepUVF,
                                        Ipp8u*   pYB,  Ipp32u stepYB,
                                        Ipp8u*   pUB,  Ipp8u* pVB,   Ipp32u stepUVB,
                                        Ipp32u  nPosS, Ipp32u nPosFB)

    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        ippiGetDiff16x16B_8u16s_C1(pYS + (nPosS <<4),stepYS, pYF + (nPosFB <<4), stepYF, 0, pYB + (nPosFB <<4), stepYB, 0, m_pBlock[0], m_uiBlockStep[0],0);                                 

        copyBDiffChromaBlockNV12(pUS,pVS,stepUVS,pUF,pVF,stepUVF,pUB,pVB,stepUVB,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosS,nPosFB);   

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteMBProg(Ipp8u* pY, Ipp32u stepY, Ipp8u* pU, Ipp8u* pV, Ipp32u stepUV, Ipp32u nPos)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        IppiSize roiSize = {16, 16};
        _own_ippiConvert_16s8u_C1R(m_pBlock[0], m_uiBlockStep[0],pY + (nPos <<4), stepY,roiSize);
        /* chroma block */
        pasteChromaBlockNV12(pU,pV,stepUV,m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPos);       

        return UMC::UMC_OK;
    }

    virtual UMC::Status PasteSumMBProg( Ipp8u* pYRef, Ipp32u stepYRef, 
                                        Ipp8u* pURef, Ipp8u* pVRef, Ipp32u stepUVRef, 
                                        Ipp8u* pYDst, Ipp32u stepYDst, 
                                        Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                        Ipp32u nPosRef, Ipp32u  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        _own_Add16x16_8u16s(pYRef + (nPosRef <<4),stepYRef,m_pBlock[0],m_uiBlockStep[0], pYDst + (nPosDst <<4) ,stepYDst, 0, 0);

        /* chroma block */
        pasteSumChromaBlockNV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteBSumMBProg(Ipp8u* pYRefF, Ipp32u stepYRefF, 
                                        Ipp8u* pURefF, Ipp8u* pVRefF, Ipp32u stepUVRefF, 
                                        Ipp8u* pYRefB, Ipp32u stepYRefB, 
                                        Ipp8u* pURefB, Ipp8u* pVRefB, Ipp32u stepUVRefB, 
                                        Ipp8u* pYDst, Ipp32u stepYDst, 
                                        Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                        Ipp32u nPosRef, Ipp32u  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        ippiMC16x16B_8u_C1   (  pYRefF + (nPosRef <<4),stepYRefF, 0,
                                pYRefB + (nPosRef <<4),stepYRefB, 0,
                                m_pBlock[0],m_uiBlockStep[0],
                                pYDst + (nPosDst <<4) ,stepYDst, 0);

        /* chroma block */
        pasteBSumChromaBlockNV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB, pUDst,pVDst,stepUVDst, m_pBlock[4],m_pBlock[5],m_uiBlockStep[4],nPosRef, nPosDst);       

        return UMC::UMC_OK;
    }

    virtual UMC::Status PasteSumSkipMBProg( Ipp8u* pYRef, Ipp32u stepYRef, 
                                            Ipp8u* pURef, Ipp8u* pVRef, Ipp32u stepUVRef, 
                                            Ipp8u* pYDst, Ipp32u stepYDst, 
                                            Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                            Ipp32u nPosRef, Ipp32u  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */
        IppiSize roiSize = {16, 16};
        ippiCopy_8u_C1R(pYRef + (nPosRef <<4), stepYRef, pYDst + (nPosDst <<4) ,stepYDst,roiSize);

        /* chroma block */
        pasteSumSkipChromaBlockNV12(pURef,pVRef,stepUVRef,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

        return UMC::UMC_OK;
    }
    virtual UMC::Status PasteBSumSkipMBProg(    Ipp8u* pYRefF, Ipp32u stepYRefF, 
                                                Ipp8u* pURefF, Ipp8u* pVRefF, Ipp32u stepUVRefF,
                                                Ipp8u* pYRefB, Ipp32u stepYRefB, 
                                                Ipp8u* pURefB, Ipp8u* pVRefB, Ipp32u stepUVRefB,
                                                Ipp8u* pYDst, Ipp32u stepYDst, 
                                                Ipp8u* pUDst, Ipp8u* pVDst, Ipp32u stepUVDst,                             
                                                Ipp32u nPosRef, Ipp32u  nPosDst)
    {
        if (!m_pBlock[0])
            return UMC::UMC_ERR_NOT_INITIALIZED;

        /* luma block */

        ippiAverage16x16_8u_C1R(pYRefF + (nPosRef <<4), stepYRefF, pYRefB + (nPosRef <<4),stepYRefB, pYDst + (nPosDst <<4),  stepYDst);

        /* chroma block */
        pasteBSumSkipChromaBlockNV12(pURefF,pVRefF,stepUVRefF,pURefB,pVRefB,stepUVRefB,pUDst,pVDst,stepUVDst,nPosRef, nPosDst);      

        return UMC::UMC_OK;
    }
};
struct MBEdges
{
    unsigned YExHor:4;
    unsigned YExVer:4;
    unsigned YInHor:4;
    unsigned YInVer:4;

    unsigned YAdUpp:4;
    unsigned YAdBot:4;
    unsigned YAdLef:4;
    unsigned YAdRig:4;

    unsigned UExHor:4;
    unsigned UExVer:4;
    unsigned UAdHor:4;
    unsigned UAdVer:4;

    unsigned VExHor:4;
    unsigned VExVer:4;
    unsigned VAdHor:4;
    unsigned VAdVer:4;
};
class VC1EncoderMBInfo
{

private:

    bool            m_bIntra;
    Ipp8u           m_uiMBPattern;

    //for P frames
    sCoordinate     m_MV [2][6];
    Ipp8u           m_uiIntraPattern;
    Ipp32u          m_uiBlocksPattern;
    MBEdges         m_sMBEdges;
    Ipp32u          m_uiVSTPattern;

    //for I frames, smoothing
    bool            m_uiOverlap;


public:
    VC1EncoderMBInfo():
        m_bIntra (true),
        m_uiMBPattern(0),
        m_uiIntraPattern(0),
        m_uiBlocksPattern(0),
        m_uiVSTPattern(0),
        m_uiOverlap(false)
    {

        memset(m_MV[0],       0, sizeof(sCoordinate)*6);
        memset(m_MV[1],       0, sizeof(sCoordinate)*6);

    }
    inline void SetEdgesIntra(bool top, bool left)
    {
        m_sMBEdges.YExHor = m_sMBEdges.UExHor = m_sMBEdges.VExHor = 0;
        m_sMBEdges.YExVer = m_sMBEdges.UExVer = m_sMBEdges.VExVer = 0;
        m_sMBEdges.YInHor = m_sMBEdges.YInVer =  0;

        m_sMBEdges.YAdUpp=m_sMBEdges.YAdBot=m_sMBEdges.YAdLef=m_sMBEdges.YAdRig=
        m_sMBEdges.UAdHor=m_sMBEdges.UAdVer=m_sMBEdges.VAdHor=m_sMBEdges.VAdVer = 0xff;

        if (top)
        {
            m_sMBEdges.YExHor = m_sMBEdges.UExHor = m_sMBEdges.VExHor = 0xff;
        }
        if (left)
        {
            m_sMBEdges.YExVer = m_sMBEdges.UExVer = m_sMBEdges.VExVer =  0xff;
        }
    }
    inline void SetInternalBlockEdge(Ipp8u YFlagUp, Ipp8u YFlagBot, Ipp8u UFlagH, Ipp8u VFlagH,
                                     Ipp8u YFlagL,  Ipp8u YFlagR,   Ipp8u UFlagV, Ipp8u VFlagV)
    {
        m_sMBEdges.YAdUpp = YFlagUp  & 0x0F;
        m_sMBEdges.YAdBot = YFlagBot & 0x0F;
        m_sMBEdges.UAdHor = UFlagH   & 0x0F;
        m_sMBEdges.VAdHor = VFlagH   & 0x0F;

        m_sMBEdges.YAdLef = YFlagL   & 0x0F;
        m_sMBEdges.YAdRig = YFlagR   & 0x0F;
        m_sMBEdges.UAdVer = UFlagV   & 0x0F;
        m_sMBEdges.VAdVer = VFlagV   & 0x0F;

    }
    inline void SetExternalEdgeVer(Ipp8u YFlag, Ipp8u UFlag, Ipp8u VFlag)
    {
        m_sMBEdges.YExVer = YFlag & 0x0F;
        m_sMBEdges.UExVer = UFlag & 0x0F;
        m_sMBEdges.VExVer = VFlag & 0x0F;
    }
    inline void SetExternalEdgeHor(Ipp8u YFlag, Ipp8u UFlag, Ipp8u VFlag)
    {
        m_sMBEdges.YExHor = YFlag & 0x0F;
        m_sMBEdges.UExHor = UFlag & 0x0F;
        m_sMBEdges.VExHor = VFlag & 0x0F;
    }
    inline void SetInternalEdge(Ipp8u YFlagV, Ipp8u YFlagH)
    {
        m_sMBEdges.YInVer = YFlagV & 0x0F;
        m_sMBEdges.YInHor = YFlagH & 0x0F;
    }
    inline Ipp32u GetLumaExHorEdge()
    {
        return m_sMBEdges.YExHor;
    }
    inline Ipp32u GetLumaExVerEdge()
    {
        return m_sMBEdges.YExVer;
    }
    inline Ipp32u GetLumaInHorEdge()
    {
        return m_sMBEdges.YInHor;
    }
    inline Ipp32u GetLumaInVerEdge()
    {
        return m_sMBEdges.YInVer;
    }
    inline Ipp32u GetLumaAdUppEdge()
    {
        return m_sMBEdges.YAdUpp;
    }
    inline Ipp32u GetLumaAdBotEdge()
    {
        return m_sMBEdges.YAdBot;
    }
    inline Ipp32u GetLumaAdLefEdge()
    {
        return m_sMBEdges.YAdLef;
    }
    inline Ipp32u GetLumaAdRigEdge()
    {
        return m_sMBEdges.YAdRig;
    }
    inline Ipp32u GetUExHorEdge()
    {
        return m_sMBEdges.UExHor;
    }
    inline Ipp32u GetVExHorEdge()
    {
        return m_sMBEdges.VExHor;
    }
    inline Ipp32u GetUExVerEdge()
    {
        return m_sMBEdges.UExVer;
    }
    inline Ipp32u GetVExVerEdge()
    {
        return m_sMBEdges.VExVer;
    }
    inline Ipp32u GetVAdVerEdge()
    {
        return m_sMBEdges.VAdVer;
    }
   inline Ipp32u GetUAdHorEdge()
    {
        return m_sMBEdges.UAdHor;
    }
    inline Ipp32u GetVAdHorEdge()
    {
        return m_sMBEdges.VAdHor;
    }
    inline Ipp32u GetUAdVerEdge()
    {
        return m_sMBEdges.UAdVer;
    }

    inline  void SetBlocksPattern(Ipp32u blocksPattern)
    {
        m_uiBlocksPattern = blocksPattern;
    }
    inline bool isCoded(Ipp32s blk, Ipp32s subblk)
    {
        return ((m_uiBlocksPattern & (1<<VC_ENC_PATTERN_POS1(blk, subblk)))!=0);
    }

    inline void Init( bool intra)
    {
         m_bIntra =  intra;

         SetMVNULL();

         if (intra)
         {
            m_uiIntraPattern = VC1_ENC_PAT_MASK_MB;
         }
         else
         {
            m_uiIntraPattern = 0;
         }
    }
    inline bool isIntra()
    {
        return  (m_bIntra);
    }
    inline bool isIntra(Ipp32s i)
    {
        return  ((m_uiIntraPattern & (1<<VC_ENC_PATTERN_POS(i)))!=0);
    }

    inline bool GetLumaMV(sCoordinate *mv, bool bForward=true)
    {

        switch(m_uiIntraPattern >> 2)
        {
        case 0x0:
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x );
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y );
            return true;
        case 0x8: //1000
            mv->x = median3(m_MV[bForward][1].x , m_MV[bForward][2].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][1].y , m_MV[bForward][2].y , m_MV[bForward][3].y );
            return true;
        case 0x4: //100
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][2].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][2].y , m_MV[bForward][3].y );
            return true;
        case 0x2://10
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][1].x , m_MV[bForward][3].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][1].y , m_MV[bForward][3].y );
            return true;
        case 0x1:
            mv->x = median3(m_MV[bForward][0].x , m_MV[bForward][1].x , m_MV[bForward][2].x );
            mv->y = median3(m_MV[bForward][0].y , m_MV[bForward][1].y , m_MV[bForward][2].y );
            return true;
        case 0xC:
        case 0xA:
        case 0x9:
        case 0x6:
        case 0x5:
        case 0x3:
            mv->x = (m_MV[bForward][0].x + m_MV[bForward][1].x + m_MV[bForward][2].x + m_MV[bForward][3].x)/2;
            mv->y = (m_MV[bForward][0].y + m_MV[bForward][1].y + m_MV[bForward][2].y + m_MV[bForward][3].y)/2;
            return true;
        default:
            mv->x = 0;
            mv->y = 0;
            return false;
        }
    }
    inline bool  GetLumaVectorFields (sCoordinate *mv, bool bSecond,bool bForward=true)
    {        
        Ipp32u n =  m_MV[bForward][0].bSecond + m_MV[bForward][1].bSecond + m_MV[bForward][2].bSecond + m_MV[bForward][3].bSecond;
        
        switch (n)
        {
        case 4:
            mv->bSecond = 1;
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x);
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);
            return true;
        case 3:
            mv->bSecond = 1;
            if (m_MV[bForward][0].bSecond == 0)
            {
                mv->x = median3( m_MV[bForward][1].x, m_MV[bForward][2].x, m_MV[bForward][3].x);
                mv->y = median3( m_MV[bForward][1].y, m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][1].bSecond == 0)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][2].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][2].bSecond == 0)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y,m_MV[bForward][3].y);            
            }
            else
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y);            
            }
            return true;
        case 2:
            Ipp32u mask[4];
            mask[0] = (m_MV[bForward][0].bSecond == bSecond)? 0xffffffff:0;
            mask[1] = (m_MV[bForward][1].bSecond == bSecond)? 0xffffffff:0;
            mask[2] = (m_MV[bForward][2].bSecond == bSecond)? 0xffffffff:0;
            mask[3] = (m_MV[bForward][3].bSecond == bSecond)? 0xffffffff:0;
            mv->x = ((Ipp16s)(mask[0]&m_MV[bForward][0].x)+ (Ipp16s)(mask[1]&m_MV[bForward][1].x) + (Ipp16s)(mask[2]&m_MV[bForward][2].x) + (Ipp16s)(mask[3]&m_MV[bForward][3].x))/2;
            mv->y = ((Ipp16s)(mask[0]&m_MV[bForward][0].y)+ (Ipp16s)(mask[1]&m_MV[bForward][1].y) + (Ipp16s)(mask[2]&m_MV[bForward][2].y) + (Ipp16s)(mask[3]&m_MV[bForward][3].y))/2;
            mv->bSecond = bSecond;
            
            return true;   
        case 1:
            if (m_MV[bForward][0].bSecond == 1)
            {
                mv->x = median3( m_MV[bForward][1].x, m_MV[bForward][2].x, m_MV[bForward][3].x);
                mv->y = median3( m_MV[bForward][1].y, m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][1].bSecond == 1)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][2].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);            
            }
            else if (m_MV[bForward][2].bSecond == 1)
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][3].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y,m_MV[bForward][3].y);            
            }
            else
            {
                mv->x = median3(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x);
                mv->y = median3(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y);            
            }
            mv->bSecond = 0;
            return true;          
         case 0:
            mv->x = median4(m_MV[bForward][0].x, m_MV[bForward][1].x,m_MV[bForward][2].x,m_MV[bForward][3].x);
            mv->y = median4(m_MV[bForward][0].y ,m_MV[bForward][1].y ,m_MV[bForward][2].y ,m_MV[bForward][3].y);
            mv->bSecond = 0;
            return true;          
         default:
            assert (0);
            return false;     
        
        }
    
    }
    inline void SetMVNULL()
    {
        m_MV[0][0].x = m_MV[0][1].x = m_MV[0][2].x = m_MV[0][3].x = 0;
        m_MV[0][0].y = m_MV[0][1].y = m_MV[0][2].y = m_MV[0][3].y = 0;
        m_MV[1][0].x = m_MV[1][1].x = m_MV[1][2].x = m_MV[1][3].x = 0;
        m_MV[1][0].y = m_MV[1][1].y = m_MV[1][2].y = m_MV[1][3].y = 0;
    }
    inline void SetMV(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][0].x = m_MV[bForward][1].x = m_MV[bForward][2].x = m_MV[bForward][3].x = mv.x;
        m_MV[bForward][0].y = m_MV[bForward][1].y = m_MV[bForward][2].y = m_MV[bForward][3].y = mv.y;
    }
    inline void SetMV_F(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][0].x = m_MV[bForward][1].x = m_MV[bForward][2].x = m_MV[bForward][3].x = mv.x;
        m_MV[bForward][0].y = m_MV[bForward][1].y = m_MV[bForward][2].y = m_MV[bForward][3].y = mv.y;
        m_MV[bForward][0].bSecond = m_MV[bForward][1].bSecond =
        m_MV[bForward][2].bSecond = m_MV[bForward][3].bSecond = mv.bSecond;
    }
    inline void SetMVChroma(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][4].x = m_MV[bForward][5].x = mv.x;
        m_MV[bForward][4].y = m_MV[bForward][5].y = mv.y;
    }
    inline void SetMVChroma_F(sCoordinate mv, bool bForward=true)
    {
        m_MV[bForward][4].x = m_MV[bForward][5].x = mv.x;
        m_MV[bForward][4].y = m_MV[bForward][5].y = mv.y;
        m_MV[bForward][4].bSecond = m_MV[bForward][5].bSecond = mv.bSecond;
    }

    inline UMC::Status SetMV(sCoordinate mv, Ipp32s blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return UMC::UMC_ERR_FAILED;
        m_MV[bForward][blockNum].x =  mv.x;
        m_MV[bForward][blockNum].y =  mv.y;
        return UMC::UMC_OK;
    }
    inline UMC::Status SetMV_F(sCoordinate mv, Ipp32s blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return UMC::UMC_ERR_FAILED;
        m_MV[bForward][blockNum].x =  mv.x;
        m_MV[bForward][blockNum].y =  mv.y;
        m_MV[bForward][blockNum].bSecond = mv.bSecond;
        return UMC::UMC_OK;
    }
    inline UMC::Status GetMV(sCoordinate* mv, Ipp32s blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return 0;
        mv->x =  m_MV[bForward][blockNum].x ;
        mv->y =  m_MV[bForward][blockNum].y;
        return UMC::UMC_OK;
    }

    inline UMC::Status GetMV_F(sCoordinate* mv, Ipp32s blockNum, bool bForward=true)
    {
        if (blockNum>=6)
            return 0;
        mv->x =  m_MV[bForward][blockNum].x ;
        mv->y =  m_MV[bForward][blockNum].y;
        mv->bSecond = m_MV[bForward][blockNum].bSecond;
        return UMC::UMC_OK;
    }

    inline void        SetMBPattern(Ipp8u pattern)
    {
        m_uiMBPattern = pattern;
    }
    inline Ipp8u       GetPattern()
    {
        return m_uiMBPattern;
    }
    inline Ipp8u       GetIntraPattern()
    {
        return m_uiIntraPattern;
    }
    inline void SetIntraBlock(Ipp32s blockNum)
    {
        m_MV[0][blockNum].x = m_MV[0][blockNum].y =
        m_MV[1][blockNum].x = m_MV[1][blockNum].y = 0;
        m_uiIntraPattern = m_uiIntraPattern | (1<<VC_ENC_PATTERN_POS(blockNum));
    }

    inline Ipp32u GetBlkPattern(Ipp32s blk)
    {
        return (m_uiBlocksPattern >> (5 - blk)*4) & 0xf;
    }
    inline eTransformType GetBlkVSTType(Ipp32s blk)
    {
        Ipp32u num = blk;

        Ipp32u pattern = (m_uiVSTPattern >> (num*2));

        return BlkTransformTypeTabl[pattern & 0x03];
    }

    inline void SetVSTPattern(eTransformType* pBlkVTSType)
    {
        Ipp32u blk = 0;
        eTransformType VTSType = VC1_ENC_8x8_TRANSFORM;
        m_uiVSTPattern = 0;

        for(blk = 0; blk < 6; blk ++)
        {
            VTSType = pBlkVTSType[blk];
            if(!GetBlkPattern(blk))
                VTSType = VC1_ENC_8x8_TRANSFORM;

            switch(VTSType)
            {
                case VC1_ENC_8x8_TRANSFORM:
                    break;
                case VC1_ENC_8x4_TRANSFORM:
                    m_uiVSTPattern |= (0x1<<(blk*2));
                    break;
                case VC1_ENC_4x8_TRANSFORM:
                    m_uiVSTPattern |= (0x2<<(blk*2));
                    break;
                case VC1_ENC_4x4_TRANSFORM:
                    m_uiVSTPattern |= (0x3<<(blk*2));
                    break;
                default:
                    assert(0);
                    break;
            }
        }
    }

    inline Ipp32u GetVSTPattern()
    {
        return m_uiVSTPattern;
    }
    inline void SetOverlap(bool overlap)
    {
        m_uiOverlap = overlap;
    }
    inline bool GetOverlap()
    {
        return m_uiOverlap;
    }
};

struct VC1EncoderMB
{
    VC1EncoderMBData    * pData;
    VC1EncoderMBData    * pRecData;
    VC1EncoderMBInfo    * pInfo;
};


class VC1EncoderMBs
{
private:
    VC1EncoderMBInfo ** m_MBInfo;       // rows of macroblocks info
    VC1EncoderMBData ** m_MBData;       // rows of macroblocks data
    VC1EncoderMBData ** m_MBRecData;    // rows of macroblocks data

    Ipp32s              m_iCurrRowIndex;
    Ipp32s              m_iPrevRowIndex;
    Ipp32u              m_iCurrMBIndex;
    Ipp32u              m_uiMBsInRow;
    Ipp32u              m_uiMBsInCol;

public:
    VC1EncoderMBs():
        m_iCurrRowIndex(0),
        m_iPrevRowIndex(-1),
        m_iCurrMBIndex(0),
        m_uiMBsInRow(0),
        m_uiMBsInCol(0)
    {
        m_MBInfo = 0;
        m_MBData = 0;
    }
    ~VC1EncoderMBs()
    {
        Close();
    }

    VC1EncoderMBInfo*       GetCurrMBInfo();
    VC1EncoderMBInfo*       GetTopMBInfo();
    VC1EncoderMBInfo*       GetLeftMBInfo();
    VC1EncoderMBInfo*       GetTopLeftMBInfo();
    VC1EncoderMBInfo*       GetTopRightMBInfo();
    VC1EncoderMBInfo*       GetPevMBInfo(Ipp32s x, Ipp32s y);

    VC1EncoderMBData*       GetCurrMBData();
    VC1EncoderMBData*       GetTopMBData();
    VC1EncoderMBData*       GetLeftMBData();
    VC1EncoderMBData*       GetTopLeftMBData();

    VC1EncoderMBData*       GetRecCurrMBData();
    VC1EncoderMBData*       GetRecTopMBData();
    VC1EncoderMBData*       GetRecLeftMBData();
    VC1EncoderMBData*       GetRecTopLeftMBData();


    VC1EncoderMB*           GetCurrMB();
    VC1EncoderMB*           GetTopMB();
    VC1EncoderMB*           GetLeftMB();
    VC1EncoderMB*           GetTopLeft();

    static Ipp32u           CalcAllocMemorySize(Ipp32u MBsInRow, Ipp32u MBsInCol, bool bNV12);
    UMC::Status             Init(Ipp8u* pPicBufer, Ipp32u AllocatedMemSize, Ipp32u MBsInRow, Ipp32u MBsInCol, bool bNV12);
    UMC::Status             Close();
    UMC::Status             NextMB();
    UMC::Status             NextRow();
    UMC::Status             StartRow();
    UMC::Status             Reset();
};

typedef struct
{
    VC1EncoderMBData* LeftMB;
    VC1EncoderMBData* TopMB;
    VC1EncoderMBData* TopLeftMB;
} NeighbouringMBsData;

inline void GetTSType (Ipp32u pattern, eTransformType* pBlockTT)
{
    Ipp8u BlockTrans = (Ipp8u)(pattern & 0x03);
    for(Ipp32u blk_num = 0; blk_num < 6; blk_num++)
    {
        pBlockTT[blk_num] = BlkTransformTypeTabl[BlockTrans];
        pattern>>=2;
        BlockTrans = (Ipp8u)(pattern & 0x03);
    }
}
}
#endif
#endif //defined (UMC_ENABLE_VC1_VIDEO_ENCODER)
