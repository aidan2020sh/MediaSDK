 /* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008-2014 Intel Corporation. All Rights Reserved.
//
//
//          MPEG2 encoder
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_ENCODE)

#include "mfx_enc_common.h"
#include "mfx_mpeg2_encode_utils_hw.h"
#include "umc_mpeg2_video_encoder.h"
#include "umc_mpeg2_enc.h"
#include "umc_mpeg2_enc_defs.h"
#include "mfx_brc_common.h"
#include "mfx_mpeg2_enc_common.h"
#include "umc_video_brc.h"
#include "umc_mpeg2_brc.h"
#include "mfx_task.h"
#include "libmfx_core.h"

// temporary defines
#define CHECK_VERSION(ver) /*ver.Major==0;*/
#define CHECK_CODEC_ID(id, myid) /*id==myid;*/

#define RANGE_TO_F_CODE(range, fcode) { \
    Ipp32s fc = 1;                        \
    while((4<<fc) < range && fc <= 15)    \
    fc++;                               \
    fcode = fc;                           \
}
namespace MPEG2EncoderHW
{
    mfxStatus CheckHwCaps(VideoCORE* core, mfxVideoParam const * par, mfxExtCodingOption const * ext, ENCODE_CAPS*  pCaps)
    {
        ENCODE_CAPS hwCaps = { 0 };

        mfxStatus sts = MfxHwMpeg2Encode::QueryHwCaps(core, hwCaps);
        if (sts != MFX_ERR_NONE)
            return MFX_WRN_PARTIAL_ACCELERATION;

        if (par->mfx.FrameInfo.Width  > hwCaps.MaxPicWidth ||
            par->mfx.FrameInfo.Height > hwCaps.MaxPicHeight)
            return MFX_WRN_PARTIAL_ACCELERATION;

        if (par->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            if (ext == 0)
                ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);

            if (ext != 0 && ext->FramePicture == MFX_CODINGOPTION_OFF)
                return MFX_WRN_PARTIAL_ACCELERATION;
        }
        if (!hwCaps.EncodeFunc && !hwCaps.EncFunc)
        {
            return MFX_WRN_PARTIAL_ACCELERATION;
        }
        if( pCaps )
        {
            *pCaps = hwCaps;
        }

        return MFX_ERR_NONE;
    } 
    HW_MODE GetHwEncodeMode(ENCODE_CAPS &caps)
    {
        if( caps.EncodeFunc )
        {
            return FULL_ENCODE;
        }
        else if(caps.EncFunc)
        {
            return HYBRID_ENCODE;
        }
        else
        {
            return UNSUPPORTED;
        }
    }
    mfxStatus ApplyTargetUsage (mfxVideoParamEx_MPEG2* par)
    {
        static mfxU16 ranges [8] = {64,64,64,64,64,64,64,64};

        if (par->mfxVideoParams.mfx.CodecProfile == MFX_PROFILE_MPEG2_SIMPLE)
        {
            par->mfxVideoParams.mfx.GopRefDist = 1;
        }

        if (par->mfxVideoParams.mfx.GopRefDist == 0)
        {
            par->mfxVideoParams.mfx.GopRefDist = 3;
        }
        if (par->mfxVideoParams.mfx.GopPicSize == 0)
        {
            par->mfxVideoParams.mfx.GopPicSize = 24;
        }
        if (par->mfxVideoParams.mfx.GopRefDist > par->mfxVideoParams.mfx.GopPicSize)
        {
            par->mfxVideoParams.mfx.GopRefDist = par->mfxVideoParams.mfx.GopPicSize;
        }
        if (par->MVRangeP[0] == 0 ||  par->MVRangeP[1] == 0 )
        {
            mfxU32 FrameW = par->mfxVideoParams.mfx.FrameInfo.Width;

            par->MVRangeP[0] = FrameW < 200 ? 32 /*3*/ : FrameW < 500 ? 64 /*4*/  : FrameW < 1400 ? 128 /*5*/ : 256/*6*/;             // 200, 500, 1400 are horizontal resolution
            par->MVRangeP[1] = par->MVRangeP[0] > 128 /*5*/ ? 128 /*5*/ : par->MVRangeP[0];
        }
        par->MVRangeB[0][0] =  par->MVRangeP[0];
        par->MVRangeB[0][1] =  par->MVRangeP[1];
        par->MVRangeB[1][0] =  par->MVRangeP[0];
        par->MVRangeB[1][1] =  par->MVRangeP[1];

        par->bAllowFieldPrediction = 1;
        par->bAllowFieldDCT        = 1;

        return MFX_ERR_NONE;
    }

    mfxExtCodingOptionSPSPPS* GetExtCodingOptionsSPSPPS(mfxExtBuffer** ebuffers, 
        mfxU32 nbuffers) 
    {
        for(mfxU32 i=0; i<nbuffers; i++) 
        {
            if((0 != (*ebuffers+i)) && ((*ebuffers+i)->BufferId == MFX_EXTBUFF_CODING_OPTION_SPSPPS)) 
            {
                return (mfxExtCodingOptionSPSPPS*)(*ebuffers+i);
            }
        }
        return 0;
    }
    mfxStatus CheckExtendedBuffers (mfxVideoParam* par)
    {
        mfxU32 supported_buffers[] = {
            MFX_EXTBUFF_CODING_OPTION
            ,MFX_EXTBUFF_CODING_OPTION_SPSPPS
            ,MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION
            ,MFX_EXTBUFF_VIDEO_SIGNAL_INFO
#ifdef PAVP_SUPPORT
            ,MFX_EXTBUFF_PAVP_OPTION
#endif   
            ,MFX_EXTBUFF_QM
        };
        mfxU32 num_supported = 0;

        if (par->NumExtParam == 0 || par->ExtParam == 0)
        {
            return MFX_ERR_NONE;
        }
        for (mfxU32 n_buf=0; n_buf < sizeof(supported_buffers)/sizeof(mfxU32); n_buf++)
        {
            mfxU32 num = 0;
            for (mfxU32 i=0; i < par->NumExtParam; i++)
            {
                if (par->ExtParam[i] == NULL)
                {
                    return MFX_ERR_NULL_PTR;
                }
                if (par->ExtParam[i]->BufferId == supported_buffers[n_buf])
                {
                    num ++;
                }        
            }
            if (num > 1) 
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            num_supported += num;

        }
        return (num_supported == par->NumExtParam) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED;
    }

#define isNONLocked(pSurface) (pSurface->Data.Y == 0)

    mfxStatus CopyFrame(mfxFrameSurface1 *pDst, mfxU16 dstMemType, 
        mfxFrameSurface1 *pSrc, mfxU16 srcMemType, 
        VideoCORE* pCore)
    {
        mfxStatus sts = MFX_ERR_NONE;

        mfxFrameSurface1 srcTempSurface = {}; 
        mfxFrameSurface1 dstTempSurface = {};

        srcTempSurface.Info = pSrc->Info;
        dstTempSurface.Info = pDst->Info;

        bool isSrcLocked = false;
        bool isDstLocked = false;


        if (isNONLocked(pSrc))
        {    
            if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
            {
                sts = pCore->LockExternalFrame(pSrc->Data.MemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            else
            {
                sts = pCore->LockFrame(pSrc->Data.MemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            isSrcLocked = true;
        }
        else
        {
            srcTempSurface.Data = pSrc->Data;
        }
        if (isNONLocked(pDst))
        {    
            if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
            {
                sts = pCore->LockExternalFrame(pDst->Data.MemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            else
            {
                sts = pCore->LockFrame(pDst->Data.MemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            isDstLocked = true;
        }
        else
        {
            dstTempSurface.Data = pDst->Data;
        }
        sts = pCore->DoFastCopy(&dstTempSurface, &srcTempSurface);
        MFX_CHECK_STS(sts);

        if (true == isSrcLocked)
        {
            if (srcMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
            {
                sts = pCore->UnlockExternalFrame(pSrc->Data.MemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            else
            {
                sts = pCore->UnlockFrame(pSrc->Data.MemId, &srcTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
        }

        if (true == isDstLocked)
        {
            if (dstMemType & MFX_MEMTYPE_EXTERNAL_FRAME)
            {
                sts = pCore->UnlockExternalFrame(pDst->Data.MemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
            else 
            {
                sts = pCore->UnlockFrame(pDst->Data.MemId, &dstTempSurface.Data);
                MFX_CHECK_STS(sts);
            }
        }
        return MFX_ERR_NONE;
    }
#undef isNONLocked

    /*static mfxU16 GetBufferSizeInKB (mfxU16 TargetKbps, double frame_rate, bool bMin = false)
    {
    mfxU32 numFrames = (bMin)? 2:10;
    if (TargetKbps==0 || frame_rate == 0.0)
    {
    return 0;
    }
    return (mfxU16)((numFrames*TargetKbps)/(frame_rate*8));
    }
    static mfxU16 GetBitRate(mfxU16 w, mfxU16 h, double frame_rate, bool bIntraFrames ,bool bMin = false)
    {
    double coeff = 0;
    if (bIntraFrames)
    {
    coeff = (bMin)? 30.0:15.0;
    }
    else
    {
    coeff = (bMin)? 250.0:25.0;   
    }
    return (mfxU16)(frame_rate*(double)(w*h*3/2)*8.0/1000.0/coeff);
    }*/
    mfxStatus FillMFXFrameParams(mfxFrameParamMPEG2*     pFrameParams,
                                mfxU8 frameType, 
                                mfxVideoParamEx_MPEG2 *pExParams, 
                                mfxU16 inputPictStruct, 
                                bool bBackwOnly)
    {
        mfxU16                  seqPicStruct = pExParams->mfxVideoParams.mfx.FrameInfo.PicStruct & 0x0F; 
        bool                    bField = false;

        pFrameParams->FrameType    = frameType;  

        mfxU16 curPicStruct = 0;
        switch (seqPicStruct)
        {
        case MFX_PICSTRUCT_PROGRESSIVE: 
            curPicStruct = mfxU16(MFX_PICSTRUCT_PROGRESSIVE | (inputPictStruct &0xF0)); break;
        case MFX_PICSTRUCT_FIELD_TFF:
            curPicStruct = mfxU16(MFX_PICSTRUCT_FIELD_TFF | (inputPictStruct & (~MFX_PICSTRUCT_FIELD_BFF) & 0x0F)); break;
        case MFX_PICSTRUCT_FIELD_BFF:
            curPicStruct = mfxU16(MFX_PICSTRUCT_FIELD_BFF | (inputPictStruct & (~MFX_PICSTRUCT_FIELD_TFF) & 0x0F)); break;
        case MFX_PICSTRUCT_UNKNOWN:    
        default:
            curPicStruct = mfxU16((inputPictStruct == MFX_PICSTRUCT_UNKNOWN) ? MFX_PICSTRUCT_PROGRESSIVE : inputPictStruct); break;    
        }

        pFrameParams->CodecFlags  = 0;
        pFrameParams->BitStreamPCEelement = 0;

        if(curPicStruct & MFX_PICSTRUCT_PROGRESSIVE)  {

            if (!(seqPicStruct & MFX_PICSTRUCT_PROGRESSIVE)) /*progressive frame in not progressive sequence*/
            {
                pFrameParams->TopFieldFirst       = (curPicStruct & MFX_PICSTRUCT_FIELD_BFF)? 0:1;
                pFrameParams->RepeatFirstField    = (curPicStruct & MFX_PICSTRUCT_FIELD_REPEATED)? 1:0; 
            }
            else /*progressive frame in progressive sequence*/
            {
                if (curPicStruct & MFX_PICSTRUCT_FRAME_DOUBLING)
                {
                    pFrameParams->RepeatFirstField = 1;
                }
                else if (curPicStruct & MFX_PICSTRUCT_FRAME_TRIPLING)
                {
                    pFrameParams->RepeatFirstField  = 1;
                    pFrameParams->TopFieldFirst     = 1;
                }    
            }
            pFrameParams->PicStructure        = MFX_MPEG2_FRAME_PICTURE;
            pFrameParams->ProgressiveFrame    = 1;
            pFrameParams->FrameMbsOnlyFlag    = 1;
            pFrameParams->FrameDCTprediction  = 1;
        }
        else if(curPicStruct & MFX_PICSTRUCT_FIELD_TFF)
        {
            pFrameParams->TopFieldFirst     = 1;        

            if (pExParams->bFieldCoding)
            {
                pFrameParams->PicStructure        = MFX_MPEG2_TOP_FIELD;
                pFrameParams->FieldPicFlag        = 1;
                bField  = true;
            }
            else
            {
                pFrameParams->PicStructure        = MFX_MPEG2_FRAME_PICTURE;
                pFrameParams->InterlacedFrameFlag = 1;
                pFrameParams->FrameDCTprediction  = 0;
                pFrameParams->FrameMbsOnlyFlag    = 0;
            }
        }
        else if(curPicStruct & MFX_PICSTRUCT_FIELD_BFF)
        {

            if (pExParams->bFieldCoding)
            {
                pFrameParams->PicStructure       = MFX_MPEG2_BOTTOM_FIELD;
                pFrameParams->BottomFieldFlag    = 1;
                pFrameParams->FieldPicFlag       = 1;
                bField  = true;
            }
            else
            {
                pFrameParams->PicStructure        = MFX_MPEG2_FRAME_PICTURE;
                pFrameParams->InterlacedFrameFlag = 1;
                pFrameParams->FrameDCTprediction  = 0;
                pFrameParams->FrameMbsOnlyFlag    = 0;

            }    
        }
        else
        {
            pFrameParams->PicStructure      = MFX_MPEG2_FRAME_PICTURE;
            pFrameParams->TopFieldFirst     = 1;
            pFrameParams->ProgressiveFrame  = 1;
            pFrameParams->FrameMbsOnlyFlag  = 1;
            pFrameParams->FrameDCTprediction= 1;
        }

        pFrameParams->Chroma420type       = 1;
        pFrameParams->ChromaFormatIdc     = MFX_CHROMAFORMAT_YUV420;
        pFrameParams->FrameHinMbMinus1    = (pExParams->mfxVideoParams.mfx.FrameInfo.Height)>>4;
        pFrameParams->FrameWinMbMinus1    = (pExParams->mfxVideoParams.mfx.FrameInfo.Width )>>4;
        if (bField)
        {
            pFrameParams->FrameHinMbMinus1 = (pFrameParams->FrameHinMbMinus1>>1);
        }

        MFX_CHECK(pFrameParams->FrameHinMbMinus1>=1 &&pFrameParams->FrameWinMbMinus1>=1, MFX_ERR_UNSUPPORTED);

        pFrameParams->NumMb            = pFrameParams->FrameHinMbMinus1*pFrameParams->FrameWinMbMinus1;
        pFrameParams->FrameHinMbMinus1 = pFrameParams->FrameHinMbMinus1 - 1;
        pFrameParams->FrameWinMbMinus1 = pFrameParams->FrameWinMbMinus1 - 1;
        pFrameParams->CloseEntryFlag   = !!(pExParams->mfxVideoParams.mfx.GopOptFlag & MFX_GOP_CLOSED);

        if (pFrameParams->FrameType & MFX_FRAMETYPE_I)
        {
            pFrameParams->RefPicFlag        = 1;
            pFrameParams->BackwardPredFlag  = 0;
            pFrameParams->IntraPicFlag      = 1;
            pFrameParams->ForwardPredFlag   = 0;
            pFrameParams->NumRefFrame       = 0;
        }
        else if (pFrameParams->FrameType & MFX_FRAMETYPE_P)
        {
            pFrameParams->RefPicFlag        = 1;
            pFrameParams->BackwardPredFlag  = 0;
            pFrameParams->IntraPicFlag      = 0;
            pFrameParams->ForwardPredFlag   = 1; 
            pFrameParams->NumRefFrame       = 1;
        }
        else if (pFrameParams->FrameType & MFX_FRAMETYPE_B)
        {
            pFrameParams->RefPicFlag       = 0;
            pFrameParams->BackwardPredFlag = 1;
            pFrameParams->ForwardPredFlag  = (bBackwOnly)? 0 : 1;
            pFrameParams->IntraPicFlag     = 0;
            pFrameParams->NumRefFrame      = (bBackwOnly)? 1 : 2;
        }
        else
        {
            return MFX_ERR_UNSUPPORTED; 
        }
        pFrameParams->BSConcealmentNeed     = 0;
        pFrameParams->BSConcealmentMethod   = 0;
        pFrameParams->MvGridAndChroma       = 0;
        pFrameParams->BitStreamFcodes       = 0;
        pFrameParams->IntraDCprecision      = 1;

        if (pFrameParams->IntraPicFlag)
        {  
            pFrameParams->BitStreamFcodes = 0xffff;
        }  
        else if (pFrameParams->RefPicFlag)
        {
            pFrameParams->BitStreamFcodes = 0xff;

            mfxU32 fcode=0;
            RANGE_TO_F_CODE (((mfxI32)pExParams->MVRangeP[0]),fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<12;

            fcode=0;
            RANGE_TO_F_CODE (((mfxI32)pExParams->MVRangeP[1]), fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<8;

        }
        else
        {      
            mfxU32 fcode=0;
            RANGE_TO_F_CODE ((mfxI32)pExParams->MVRangeB[0][0],fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<12;

            fcode=0;
            RANGE_TO_F_CODE ((mfxI32)pExParams->MVRangeB[0][1], fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<8;

            fcode=0;
            RANGE_TO_F_CODE ((mfxI32)pExParams->MVRangeB[1][0],fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<4;

            fcode=0;
            RANGE_TO_F_CODE ((mfxI32)pExParams->MVRangeB[1][1], fcode);
            pFrameParams->BitStreamFcodes |= (fcode & 0x0f)<<0;
        }
        return MFX_ERR_NONE; 
    
    }
    bool AVBR_via_CBR (mfxVideoParam *par)
    {
       if (par->mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
           return false;
       par->mfx.RateControlMethod = MFX_RATECONTROL_CBR;
       par->mfx.MaxKbps = par->mfx.TargetKbps;
       par->mfx.InitialDelayInKB = par->mfx.BufferSizeInKB = 0;
       return true;
    }

    FramesSet::FramesSet()
    {
        Reset();
    }
    void FramesSet::Reset()
    {
        m_pInputFrame = 0;
        m_pRefFrame[0] = m_pRefFrame[1] = 0;
        m_pRawFrame[0] = m_pRawFrame[1] = 0;
        m_pRecFrame = 0;
        m_nFrame = 0;
        m_nRefFrame[0] = m_nRefFrame[1] = 0;
        m_nLastRefBeforeIntra = -1;
        m_nLastRef = -1;    
    }
    mfxStatus FramesSet::ReleaseFrames(VideoCORE* pCore)
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (m_pInputFrame)
        {
            sts = pCore->DecreaseReference(&m_pInputFrame->Data);
            MFX_CHECK_STS(sts);
            m_pInputFrame = 0;
        }
        if (m_pRecFrame)
        {
            sts = pCore->DecreaseReference(&m_pRecFrame->Data);
            MFX_CHECK_STS(sts);
            m_pRecFrame = 0;
        }
        if (m_pRefFrame[0])
        {
            sts = pCore->DecreaseReference(&m_pRefFrame[0]->Data);
            MFX_CHECK_STS(sts);
            m_pRefFrame[0] = 0;
        }
        if (m_pRefFrame[1])
        {
            sts = pCore->DecreaseReference(&m_pRefFrame[1]->Data);
            MFX_CHECK_STS(sts);
            m_pRefFrame[1] = 0;
        }
        if (m_pRawFrame[0])
        {
            sts = pCore->DecreaseReference(&m_pRawFrame[0]->Data);
            MFX_CHECK_STS(sts);
            m_pRawFrame[0] = 0;
        }
        if (m_pRawFrame[1])
        {
            sts = pCore->DecreaseReference(&m_pRawFrame[1]->Data);
            MFX_CHECK_STS(sts);
            m_pRawFrame[1] = 0;
        }
        return sts;
    }
    mfxStatus FramesSet::LockRefFrames(VideoCORE* pCore)
    {
        mfxStatus sts = MFX_ERR_NONE;
        
        if (m_pRefFrame[0])
        {
            sts = pCore->IncreaseReference(&m_pRefFrame[0]->Data);
            MFX_CHECK_STS(sts);
        }
        if (m_pRefFrame[1])
        {
            sts = pCore->IncreaseReference(&m_pRefFrame[1]->Data);
            MFX_CHECK_STS(sts);
        }
        if (m_pRawFrame[0])
        {
            sts = pCore->IncreaseReference(&m_pRawFrame[0]->Data);
            MFX_CHECK_STS(sts);
        }
        if (m_pRawFrame[1])
        {
            sts = pCore->IncreaseReference(&m_pRawFrame[1]->Data);
            MFX_CHECK_STS(sts);
        }
        return sts;   
    }

    mfxStatus ControllerBase::Query(VideoCORE * core, mfxVideoParam *in, mfxVideoParam *out, bool bAVBR_WA)
    {
#ifdef PAVP_SUPPORT
        eMFXHWType type = core->GetHWType();
#endif
        MFX_CHECK_NULL_PTR1(out);
        if(in==0) 
        {
            memset(&out->mfx, 0, sizeof(mfxInfoMFX));
            out->mfx.FrameInfo.FourCC = 1;
            out->mfx.FrameInfo.Width = 1;
            out->mfx.FrameInfo.Height = 1;
            out->mfx.FrameInfo.CropX = 0;
            out->mfx.FrameInfo.CropY = 0;
            out->mfx.FrameInfo.CropW = 1;
            out->mfx.FrameInfo.CropH = 1;
            out->mfx.FrameInfo.ChromaFormat = 1;
            out->mfx.FrameInfo.FrameRateExtN = 1;
            out->mfx.FrameInfo.FrameRateExtD = 1;
            out->mfx.FrameInfo.AspectRatioW = 1;
            out->mfx.FrameInfo.AspectRatioH = 1;
            out->mfx.FrameInfo.PicStruct = 1;
            out->mfx.CodecProfile = 1;
            out->mfx.CodecLevel = 1;
            out->mfx.GopPicSize = 1;
            out->mfx.GopRefDist = 1;
            out->mfx.GopOptFlag = 1;
            out->mfx.RateControlMethod = 1; // not sure, it is BRC
            out->mfx.InitialDelayInKB = 1; // not sure, it is BRC
            out->mfx.BufferSizeInKB = 1; // not sure, it is BRC
            out->mfx.TargetKbps = 1;
            out->mfx.MaxKbps = 1; // not sure, it is BRC
            out->mfx.NumSlice = 1;
            out->mfx.NumThread = 1;
            out->mfx.TargetUsage = 1;
            out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            out->AsyncDepth = 0;
            out->Protected = 0;

            MFX_CHECK_STS (CheckExtendedBuffers(out));
            mfxExtCodingOption* ext_out = GetExtCodingOptions(out->ExtParam,out->NumExtParam);
            if (ext_out)
            {
                mfxU32 bufOffset = sizeof(mfxExtBuffer);
                mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;

                memset ((mfxU8*)(ext_out) + bufOffset,0, bufSize);

                ext_out->EndOfSequence     = 1;
                ext_out->FramePicture      = 1;         
            }
            mfxExtCodingOptionSPSPPS* pSPSPPS_out = GetExtCodingOptionsSPSPPS (out->ExtParam, out->NumExtParam);
            if (pSPSPPS_out)
            {
                pSPSPPS_out->PPSBuffer  = NULL;
                pSPSPPS_out->PPSBufSize = 0;
                pSPSPPS_out->SPSBuffer  = NULL;
                pSPSPPS_out->SPSBufSize = 0;  
            }
            mfxExtVideoSignalInfo* pVideoSignal_out = GetExtVideoSignalInfo(out->ExtParam, out->NumExtParam);
            if (pVideoSignal_out)
            {
                pVideoSignal_out->VideoFormat              = 1;
                pVideoSignal_out->ColourDescriptionPresent = 1;
                pVideoSignal_out->ColourPrimaries          = 1;
                pVideoSignal_out->TransferCharacteristics  = 1;
                pVideoSignal_out->MatrixCoefficients       = 1;
                pVideoSignal_out->VideoFullRange           = 0;
            }
#ifdef PAVP_SUPPORT
            if (type >=MFX_HW_HSW)
            {
                out->Protected = 1;
                mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
                if (pavpOptOut)
                {
                    mfxExtBuffer header = pavpOptOut->Header;
                    memset(pavpOptOut, 0, sizeof(mfxExtPAVPOption));
                    pavpOptOut->Header = header;
                    pavpOptOut->CounterType = 1;
                    pavpOptOut->EncryptionType = 1;
                    pavpOptOut->CounterIncrement = 0;
                    pavpOptOut->CipherCounter.Count = 0;
                    pavpOptOut->CipherCounter.IV = 0;
                }
            }
#endif
        } 
        else 
        {
            bool bUnsupported = false;
            bool bWarning = false;
            //bool bInvalid = false;

            mfxExtCodingOptionSPSPPS* pSPSPPS_out = GetExtCodingOptionsSPSPPS (out->ExtParam, out->NumExtParam);
            mfxExtCodingOptionSPSPPS* pSPSPPS_in  = GetExtCodingOptionsSPSPPS (in->ExtParam,  in->NumExtParam);

            if (pSPSPPS_out && pSPSPPS_in)
            {
                if (pSPSPPS_in->SPSBuffer && pSPSPPS_out->SPSBuffer && pSPSPPS_out->SPSBufSize && pSPSPPS_in->SPSBufSize)
                {
                    mfxU32 real_size = 0;
                    if (SHParametersEx::CheckSHParameters (pSPSPPS_in->SPSBuffer, pSPSPPS_in->SPSBufSize, real_size, out, 0))
                    {
                        if (real_size <= pSPSPPS_out->SPSBufSize)
                        {
                            memcpy_s(pSPSPPS_out->SPSBuffer, real_size * sizeof(mfxU8), pSPSPPS_in->SPSBuffer, real_size * sizeof(mfxU8));  
                            memset(pSPSPPS_out->SPSBuffer + real_size, 0, pSPSPPS_out->SPSBufSize - real_size);
                        }
                        else
                        {
                            memset(pSPSPPS_out->SPSBuffer, 0, pSPSPPS_out->SPSBufSize);
                            bUnsupported   = true;
                        }

                    }
                    else
                    {
                        memset(pSPSPPS_out->SPSBuffer, 0, pSPSPPS_out->SPSBufSize);
                        bUnsupported   = true;
                    }        
                }
                else if (pSPSPPS_in->SPSBuffer || pSPSPPS_out->SPSBuffer || pSPSPPS_out->SPSBufSize || pSPSPPS_in->SPSBufSize)
                {
                    bUnsupported   = true;                
                }
            }
            else if (!(pSPSPPS_in == 0 && pSPSPPS_out ==0))
            {
                bUnsupported   = true;  
            }

            out->mfx        = in->mfx;
            out->IOPattern  = in->IOPattern;
            out->Protected  = in->Protected;
            out->AsyncDepth = in->AsyncDepth;

            mfxStatus sts = CheckHwCaps(core, out);
            MFX_CHECK_STS(sts);

            MFX_CHECK_STS (CheckExtendedBuffers(in));  
            MFX_CHECK_STS (CheckExtendedBuffers(out)); 

#ifndef PAVP_SUPPORT
            if (out->Protected)
            {
                out->Protected = 0;
                bUnsupported   = true;
            }
#else
            if (in->Protected)
            {
                out->Protected = in->Protected;

                if (type < MFX_HW_HSW)
                {
                    bUnsupported   = true;
                    out->Protected = 0;
                }

                if (!IS_PROTECTION_PAVP_ANY(in->Protected))
                {
                    bUnsupported   = true;
                    out->Protected = 0;
                }

                if (in->IOPattern && ((in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)==0))
                {
                    out->IOPattern = 0;
                    bUnsupported   = true;
                }

                mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
                mfxExtPAVPOption * pavpOptOut = (mfxExtPAVPOption*)GetExtendedBuffer(out->ExtParam, out->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);


                if (pavpOptIn && pavpOptOut)
                {
                    pavpOptOut->Header.BufferId = pavpOptIn->Header.BufferId;
                    pavpOptOut->Header.BufferSz = pavpOptIn->Header.BufferSz;

                    switch(pavpOptIn->CounterType)
                    {
                    case MFX_PAVP_CTR_TYPE_A:
                    case MFX_PAVP_CTR_TYPE_B:
                    case MFX_PAVP_CTR_TYPE_C:
                        pavpOptOut->CounterType = pavpOptIn->CounterType;
                        break;
                    default:
                        pavpOptOut->CounterType = 0;
                        bUnsupported   = true;
                        break;
                    }
                    if (pavpOptIn->EncryptionType == MFX_PAVP_AES128_CTR)
                    {
                        pavpOptOut->EncryptionType = pavpOptIn->EncryptionType;
                    }
                    else
                    {
                        pavpOptOut->EncryptionType = 0;
                        bUnsupported   = true;
                    }

                    pavpOptOut->CounterIncrement = 0;
                    memset(&pavpOptOut->CipherCounter, 0, sizeof(mfxAES128CipherCounter));
                    memset(pavpOptOut->reserved, 0, sizeof(pavpOptOut->reserved));
                }
                else
                {
                    if (pavpOptOut || pavpOptIn)
                    {
                        return MFX_ERR_UNDEFINED_BEHAVIOR;
                    }
                }
            }
            else
            {
                mfxExtPAVPOption * pavpOptIn = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
                if (pavpOptIn)
                    bUnsupported   = true;
            }
#endif

            if (out->mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 && out->mfx.FrameInfo.FourCC !=0)
            {
                out->mfx.FrameInfo.FourCC = 0;
                bUnsupported   = true;
            }

            mfxU16 ps = out->mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_PROGRESSIVE|MFX_PICSTRUCT_FIELD_TFF|MFX_PICSTRUCT_FIELD_BFF); 

            if (ps != MFX_PICSTRUCT_PROGRESSIVE &&
                ps != MFX_PICSTRUCT_FIELD_TFF &&
                ps != MFX_PICSTRUCT_FIELD_BFF &&
                ps != MFX_PICSTRUCT_UNKNOWN)
            {
                ps = MFX_PICSTRUCT_UNKNOWN;
                bWarning = true;
            }
            if (out->mfx.FrameInfo.PicStruct != ps)
            {
                out->mfx.FrameInfo.PicStruct = ps;
                bWarning = true;
            }

            mfxU16 t = (out->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 0x0f:0x1f;

            if ((out->mfx.FrameInfo.Width !=0 && out->mfx.FrameInfo.Height==0) ||
                (out->mfx.FrameInfo.Width ==0 && out->mfx.FrameInfo.Height!=0))
            {
                out->mfx.FrameInfo.Width = 0;
                out->mfx.FrameInfo.Height= 0;
                bUnsupported = true;
            }
            if (out->mfx.FrameInfo.Width > 0x1fff ||(out->mfx.FrameInfo.Width & 0x0f))
            {
                out->mfx.FrameInfo.Width = 0;
                bUnsupported = true;
            }
            if (out->mfx.FrameInfo.Height > 0x1fff ||(out->mfx.FrameInfo.Height & t))
            {
                out->mfx.FrameInfo.Height = 0;
                bUnsupported = true;
            }

            if (out->mfx.FrameInfo.CropX != 0)
            {
                out->mfx.FrameInfo.CropX = 0;
                bUnsupported = true;
            }
            if (out->mfx.FrameInfo.CropY != 0)
            {
                out->mfx.FrameInfo.CropY = 0;
                bUnsupported = true;
            }

            /*
            if (out->mfx.FrameInfo.CropW > out->mfx.FrameInfo.Width)
            {
            out->mfx.FrameInfo.CropW = 0;
            bUnsupported = true;
            }
            if (out->mfx.FrameInfo.CropH > out->mfx.FrameInfo.Height)
            {
            out->mfx.FrameInfo.CropH = 0;
            bUnsupported = true;
            }*/


            mfxU32 temp = out->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY);

            if ((temp==0 && out->IOPattern)|| temp==(MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))
            {
                out->IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
                bWarning = true;
            }
            mfxExtCodingOption* ext_in  = GetExtCodingOptions(in->ExtParam, in->NumExtParam);
            mfxExtCodingOption* ext_out = GetExtCodingOptions(out->ExtParam,out->NumExtParam);

            MFX_CHECK ((ext_in !=0 && ext_out != 0) || (ext_in == 0 && ext_out == 0),  MFX_ERR_UNDEFINED_BEHAVIOR );

            if (ext_in && ext_out)
            {
                mfxExtCodingOption temp = {0};

                mfxU32 bufOffset = sizeof(mfxExtBuffer);
                mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;

                memcpy_s(&temp, sizeof(mfxExtCodingOption), ext_in, sizeof(mfxExtCodingOption));

                memset ((mfxU8*)(ext_out) + bufOffset,0, bufSize);

                ext_out->EndOfSequence     = temp.EndOfSequence;
                ext_out->FramePicture      = temp.FramePicture;  

                bWarning = bWarning || (memcmp((mfxU8*)(ext_out) + bufOffset,(mfxU8*)(&temp) + bufOffset, bufSize)!= 0);
                bUnsupported = bUnsupported || (temp.FieldOutput == MFX_CODINGOPTION_ON);
            }

            mfxExtVideoSignalInfo* pVideoSignal_out = GetExtVideoSignalInfo(out->ExtParam, out->NumExtParam);
            mfxExtVideoSignalInfo* pVideoSignal_in  = GetExtVideoSignalInfo(in->ExtParam,  in->NumExtParam);

            MFX_CHECK ((pVideoSignal_in == 0) == (pVideoSignal_out == 0),  MFX_ERR_UNDEFINED_BEHAVIOR );

            if (pVideoSignal_in && pVideoSignal_out)
            {
                *pVideoSignal_out = *pVideoSignal_in;

                if (CheckExtVideoSignalInfo(pVideoSignal_out) == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    bWarning = true;
                }
            }

            if ((out->mfx.FrameInfo.Width!= 0 && out->mfx.FrameInfo.CropW > out->mfx.FrameInfo.Width) ||
                (out->mfx.FrameInfo.CropW == 0 && out->mfx.FrameInfo.CropH != 0))
            {
                bWarning = true;
                out->mfx.FrameInfo.CropW = out->mfx.FrameInfo.Width;
            }

            if ((out->mfx.FrameInfo.Height != 0 && out->mfx.FrameInfo.CropH > out->mfx.FrameInfo.Height)||
                (out->mfx.FrameInfo.CropW != 0 && out->mfx.FrameInfo.CropH == 0))
            {
                bWarning = true;
                out->mfx.FrameInfo.CropH = out->mfx.FrameInfo.Height;
            }

            if (out->mfx.FrameInfo.CropX || out->mfx.FrameInfo.CropY)
            {    
                out->mfx.FrameInfo.CropX = 0;
                out->mfx.FrameInfo.CropY = 0;
                bWarning = true;
            }

            if (out->mfx.FrameInfo.FrameRateExtN !=0 && out->mfx.FrameInfo.FrameRateExtD != 0)
            {
                sts = CheckFrameRateMPEG2(out->mfx.FrameInfo.FrameRateExtD, out->mfx.FrameInfo.FrameRateExtN);
                if (sts != MFX_ERR_NONE)
                {
                    bWarning = true;            
                }        
            }
            else
            {
                if (out->mfx.FrameInfo.FrameRateExtN !=0 || out->mfx.FrameInfo.FrameRateExtD != 0)
                {
                    out->mfx.FrameInfo.FrameRateExtN = 0;
                    out->mfx.FrameInfo.FrameRateExtD = 0;
                    bUnsupported = true;
                }
            }


            if ((out->mfx.TargetUsage < MFX_TARGETUSAGE_BEST_QUALITY || out->mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)&& 
                out->mfx.TargetUsage !=0)
            {
                out->mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
                bWarning = true;
            }    


            if (out->mfx.FrameInfo.ChromaFormat != 0 &&
                out->mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420)
            {
                out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                bWarning = true;
            }

            sts = CheckAspectRatioMPEG2(
                out->mfx.FrameInfo.AspectRatioW,
                out->mfx.FrameInfo.AspectRatioH, 
                out->mfx.FrameInfo.Width, 
                out->mfx.FrameInfo.Height,
                out->mfx.FrameInfo.CropW,
                out->mfx.FrameInfo.CropH);

            if (sts != MFX_ERR_NONE)
            {
                bWarning = true;
                if (sts < MFX_ERR_NONE)
                {
                    out->mfx.FrameInfo.AspectRatioW = 1;
                    out->mfx.FrameInfo.AspectRatioH = 1; 
                }
            }

            if (out->mfx.CodecProfile != MFX_PROFILE_MPEG2_SIMPLE &&
                out->mfx.CodecProfile != MFX_PROFILE_MPEG2_MAIN   &&
                out->mfx.CodecProfile != MFX_PROFILE_MPEG2_HIGH &&
                out->mfx.CodecProfile != MFX_PROFILE_UNKNOWN)
            {
                out->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
                bWarning = true;
            }
            if (out->mfx.CodecLevel != MFX_LEVEL_MPEG2_LOW &&
                out->mfx.CodecLevel != MFX_LEVEL_MPEG2_MAIN &&
                out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH1440 &&
                out->mfx.CodecLevel != MFX_LEVEL_MPEG2_HIGH &&
                out->mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
            {
                out->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
                bWarning = true;
            }
            if (bAVBR_WA)
            {
                bWarning = AVBR_via_CBR(out) ? true : bWarning;
            }
            if (out->mfx.RateControlMethod != MFX_RATECONTROL_CBR  &&
                out->mfx.RateControlMethod != MFX_RATECONTROL_VBR  &&
                out->mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                out->mfx.RateControlMethod != MFX_RATECONTROL_CQP  &&
                out->mfx.RateControlMethod != 0)
            {
                out->mfx.RateControlMethod = MFX_RATECONTROL_VBR;
                bWarning = true;
            }

            mfxU16 gof = out->mfx.GopOptFlag & (MFX_GOP_CLOSED | MFX_GOP_STRICT);
            if (out->mfx.GopOptFlag != gof)
            {    
                out->mfx.GopOptFlag = gof;
                bWarning = true;
            }

            if (bUnsupported)
            {
                return MFX_ERR_UNSUPPORTED;
            }
            /* Unreachable code is commented out
            if(bInvalid)
                return MFX_ERR_INVALID_VIDEO_PARAM;*/
            if (bWarning)
            {
                return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }

        }
        return MFX_ERR_NONE;    
    }

    mfxStatus ControllerBase::QueryIOSurf(VideoCORE * core, mfxVideoParam *par, mfxFrameAllocRequest *request)
    {
        mfxVideoParamEx_MPEG2 videoParamEx;

        MFX_CHECK_NULL_PTR1(par);
        MFX_CHECK_NULL_PTR1(request);
        CHECK_VERSION(par->Version);
        CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);
        MFX_CHECK (CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM); 

#ifdef PAVP_SUPPORT
        eMFXHWType type = core->GetHWType();
        MFX_CHECK(par->Protected == 0 || type >= MFX_HW_HSW, MFX_ERR_INVALID_VIDEO_PARAM);
#endif

        mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);
        mfxExtCodingOptionSPSPPS* pSPSPPS = GetExtCodingOptionsSPSPPS (par->ExtParam, par->NumExtParam);

        mfxVideoParam parFromSpsPps = {};

        mfxExtCodingOption extFromSpsPps = {};

        if (pSPSPPS)
        {
            parFromSpsPps = *par;
            if (ext)
                extFromSpsPps = *ext;

            //mfxStatus sts = MFX_ERR_NONE;
            mfxU32 real_len = 0;
            MFX_CHECK(pSPSPPS->PPSBufSize == 0, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(!pSPSPPS->PPSBuffer, MFX_ERR_INVALID_VIDEO_PARAM);

            if (!SHParametersEx::CheckSHParameters(pSPSPPS->SPSBuffer, pSPSPPS->SPSBufSize, real_len, &parFromSpsPps, &extFromSpsPps))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            par = &parFromSpsPps;
            ext = &extFromSpsPps;
        }

        ENCODE_CAPS EncCaps = {};

        mfxStatus sts = CheckHwCaps(core, par, ext, &EncCaps);
        MFX_CHECK_STS(sts);

        if (par->mfx.FrameInfo.Width > 0x1fff || (par->mfx.FrameInfo.Width & 0x0f) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        mfxU32 mask = (par->mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE)? 0x0f:0x1f;

        if (par->mfx.FrameInfo.Height > 0x1fff || (par->mfx.FrameInfo.Height & mask) != 0 )
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        memcpy_s (&videoParamEx.mfxVideoParams, sizeof (mfxVideoParam), par, sizeof (mfxVideoParam));
        ApplyTargetUsage (&videoParamEx);

        if ((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY)) == MFX_IOPATTERN_IN_VIDEO_MEMORY ||
            ((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY|MFX_IOPATTERN_IN_OPAQUE_MEMORY))== MFX_IOPATTERN_IN_OPAQUE_MEMORY))
        {
            request->Info              = videoParamEx.mfxVideoParams.mfx.FrameInfo ;
            request->NumFrameMin       = videoParamEx.mfxVideoParams.mfx.GopRefDist + 3;
            request->NumFrameSuggested = request->NumFrameMin;
            request->Type              = (par->IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
                ? MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_OPAQUE_FRAME  |MFX_MEMTYPE_DXVA2_DECODER_TARGET
                : MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET;
        }
        else if ((par->IOPattern & (MFX_IOPATTERN_IN_VIDEO_MEMORY|MFX_IOPATTERN_IN_SYSTEM_MEMORY))==MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        {
            request->Info              =  videoParamEx.mfxVideoParams.mfx.FrameInfo;
            request->NumFrameMin       =  videoParamEx.mfxVideoParams.mfx.GopRefDist + 3;
            request->NumFrameSuggested = request->NumFrameMin;
            request->Type              = MFX_MEMTYPE_FROM_ENCODE|MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY;
        }
        else
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        if (ext && (ext->EndOfSequence==MFX_CODINGOPTION_ON))
        {
            request->NumFrameMin       = request->NumFrameMin*2+1;
            request->NumFrameSuggested = request->NumFrameSuggested*2+1;
        }
        if (EncCaps.EncodeFunc)
        {
            request->NumFrameMin = request->NumFrameMin + videoParamEx.mfxVideoParams.AsyncDepth;
            request->NumFrameSuggested = request->NumFrameSuggested + videoParamEx.mfxVideoParams.AsyncDepth;        
        }

        return MFX_ERR_NONE;
    }

    mfxStatus  UnlockFrames (MFXGOP* pGOP, MFXWaitingList* pWaitingList, VideoCORE* pcore)
    {
        mfxStatus  sts = MFX_ERR_NONE;
        if (pGOP !=0)
        {
            pGOP->CloseGop();
            for (;;)
            {
                sFrameEx  fr = {0};
                if (!pGOP->GetFrameExForDecoding(&fr,0,0,0))
                {
                    break;
                }
                sts = pcore->DecreaseReference(&fr.m_pFrame->Data);
                MFX_CHECK_STS(sts);
                pGOP->ReleaseCurrentFrame();
            }
        }
        if (pWaitingList)
        {
            for (;;)
            {
                sFrameEx  fr = {0};
                if (!pWaitingList->GetFrameEx(&fr))
                {
                    break;
                }
                sts = pcore->DecreaseReference(&fr.m_pFrame->Data);
                MFX_CHECK_STS(sts);
                pWaitingList->MoveOnNextFrame();
            }
        }
        return sts;
    }
    ControllerBase::ControllerBase(VideoCORE *core, bool bAVBR_WA ): 
        m_InputSurfaces(core),
        m_bAVBR_WA (bAVBR_WA),
        m_pCore (core),
        m_pGOP (0),
        m_pWaitingList (0),
        m_bInitialized (false),
        m_InputFrameOrder (-1),
        m_OutputFrameOrder(-1),
        m_BitstreamLen (0)
    {

        memset (&m_VideoParamsEx, 0, sizeof(mfxVideoParamEx_MPEG2)); 
    }
    mfxExtCodingOptionQuantMatrix* GetExtCodingOptionsQuantMaxtrix(mfxExtBuffer** ebuffers,  mfxU32 nbuffers) 
    {
        for(mfxU32 i=0; i<nbuffers; i++) 
        {
            if((*ebuffers+i)->BufferId == MFX_EXTBUFF_QM) 
            {
                return (mfxExtCodingOptionQuantMatrix*)(*ebuffers+i);
            }
        }
        return 0;
    }
    mfxStatus ControllerBase::Reset(mfxVideoParam *par, bool bAllowRawFrames)
    {
        mfxStatus sts = MFX_ERR_NONE;

        bool bProgressiveSequence = false;
        mfxFrameInfo *pFrameInfo = 0;
        bool bCorrected = false;
        bool bUnsupported = false;
        bool bInvalid = false;
        eMFXHWType type = m_pCore->GetHWType();


        MFX_CHECK_NULL_PTR1(par);
        CHECK_VERSION(par->Version);
        CHECK_CODEC_ID(par->mfx.CodecId, MFX_CODEC_MPEG2);
        MFX_CHECK(CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);  
        
        memset(&m_VideoParamsEx,0,sizeof(mfxVideoParamEx_MPEG2));
        memcpy_s(&m_VideoParamsEx.mfxVideoParams.mfx, sizeof(mfxInfoMFX), &par->mfx, sizeof(mfxInfoMFX));
        m_VideoParamsEx.mfxVideoParams.IOPattern = par->IOPattern;
        m_VideoParamsEx.mfxVideoParams.Protected = par->Protected;
        m_VideoParamsEx.mfxVideoParams.AsyncDepth = par->AsyncDepth == 0 ? 2: par->AsyncDepth;


        if (m_VideoParamsEx.mfxVideoParams.mfx.BRCParamMultiplier == 0)
            m_VideoParamsEx.mfxVideoParams.mfx.BRCParamMultiplier = 1;

        /*-------------------Check and correct parameters:---------------------*/
        sts = CheckVideoParamEncoders(par, m_pCore->IsExternalFrameAllocator(), type);
        MFX_CHECK_STS(sts);

#ifndef PAVP_SUPPORT
        MFX_CHECK(par->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);
#else
        if (par->Protected)
        {
            MFX_CHECK(type >= MFX_HW_HSW, MFX_ERR_INVALID_VIDEO_PARAM);

            mfxExtPAVPOption *pavpOpt = (mfxExtPAVPOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);           
            
            MFX_CHECK(IS_PROTECTION_PAVP_ANY(par->Protected), MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK((par->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY), MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(pavpOpt, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(pavpOpt->EncryptionType == MFX_PAVP_AES128_CTR, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(pavpOpt->CounterType == MFX_PAVP_CTR_TYPE_A || pavpOpt->CounterType == MFX_PAVP_CTR_TYPE_B || pavpOpt->CounterType == MFX_PAVP_CTR_TYPE_C, MFX_ERR_INVALID_VIDEO_PARAM);
            m_VideoParamsEx.sExtPAVPOption = *pavpOpt;
            if(m_VideoParamsEx.sExtPAVPOption.CounterIncrement == 0)
                m_VideoParamsEx.sExtPAVPOption.CounterIncrement = 0xc000;
            memset(m_VideoParamsEx.sExtPAVPOption.reserved, 0, sizeof(m_VideoParamsEx.sExtPAVPOption.reserved));
        }
        else
        {
             mfxExtPAVPOption *pavpOpt = (mfxExtPAVPOption*)GetExtendedBuffer(par->ExtParam, par->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
             MFX_CHECK(!pavpOpt, MFX_ERR_INVALID_VIDEO_PARAM);
        }   
#endif

        mfxExtCodingOption extFromSpsPps = {};
        mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);


        if (mfxExtCodingOptionSPSPPS* pSPSPPS = GetExtCodingOptionsSPSPPS (par->ExtParam, par->NumExtParam))
        {
            MFX_CHECK(pSPSPPS->PPSBufSize == 0, MFX_ERR_INVALID_VIDEO_PARAM);
            MFX_CHECK(!pSPSPPS->PPSBuffer, MFX_ERR_INVALID_VIDEO_PARAM);    

            mfxU32 real_len = 0;
            if (ext != 0)
            {
                extFromSpsPps = *ext;
            }
            if (!SHParametersEx::CheckSHParameters(pSPSPPS->SPSBuffer, pSPSPPS->SPSBufSize, real_len, &m_VideoParamsEx.mfxVideoParams, &extFromSpsPps))
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

            ext = &extFromSpsPps;
        }
        mfxExtCodingOptionQuantMatrix* pMatrix = GetExtCodingOptionsQuantMaxtrix(par->ExtParam, par->NumExtParam);
        if (pMatrix)
        {
            memcpy_s(&m_VideoParamsEx.sQuantMatrix, sizeof(mfxExtCodingOptionQuantMatrix), pMatrix, sizeof(mfxExtCodingOptionQuantMatrix));
        }

        sts = CheckHwCaps(m_pCore, &m_VideoParamsEx.mfxVideoParams, ext);
        if (sts != MFX_ERR_NONE)
        {
            return is_initialized()
                ? MFX_ERR_INVALID_VIDEO_PARAM // reset can't return partial acceleration
                : MFX_WRN_PARTIAL_ACCELERATION;
        }
        m_nEncodeCalls = 0;
        m_nFrameInGOP  = 0;

        pFrameInfo = &m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo; 

        switch (pFrameInfo->PicStruct)
        { 
        case MFX_PICSTRUCT_PROGRESSIVE:
            bProgressiveSequence = true;
            break;
        case MFX_PICSTRUCT_FIELD_TFF:
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_FIELD_BFF:
            break;  
        default:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        if (pFrameInfo->CropX!=0      || pFrameInfo->CropY!=0 ||
            pFrameInfo->Width > 0x1fff || pFrameInfo->Height > 0x1fff ||
            pFrameInfo->CropW > pFrameInfo->Width ||
            pFrameInfo->CropH > pFrameInfo->Height)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        sts = CheckFrameRateMPEG2(pFrameInfo->FrameRateExtD,pFrameInfo->FrameRateExtN);
        if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        {
            bCorrected = true;        
        }
        else if (sts == MFX_ERR_INVALID_VIDEO_PARAM)
        {
            bInvalid = true;
        }
        if ((pFrameInfo->Width  & 15) !=0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        pFrameInfo->Width = pFrameInfo->CropW ? ((pFrameInfo->CropW + 15)/16)*16 : pFrameInfo->Width;

        if (bProgressiveSequence)
        {
            if ((pFrameInfo->Height  & 15) !=0)
                return MFX_ERR_INVALID_VIDEO_PARAM;
            pFrameInfo->Height = pFrameInfo->CropH ? ((pFrameInfo->CropH + 15)/16)*16 : pFrameInfo->Height;

        }
        else
        {
            if ((pFrameInfo->Height  & 31) !=0)
                return MFX_ERR_INVALID_VIDEO_PARAM; 
            pFrameInfo->Height = pFrameInfo->CropH ? ((pFrameInfo->CropH + 31)/32)*32 : pFrameInfo->Height;
        }
        if (m_bInitialized == false)
        {
            m_InitWidth  = pFrameInfo->Width;
            m_InitHeight = pFrameInfo->Height;
        }
        else if (m_InitWidth < pFrameInfo->Width || m_InitHeight < pFrameInfo->Height)
        {           
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
        }

        if (ext)
        {
            mfxExtCodingOption temp = {};

            mfxU32 bufOffset = sizeof(mfxExtBuffer);
            mfxU32 bufSize   = sizeof(mfxExtCodingOption) - bufOffset;
            temp.EndOfSequence = ext->EndOfSequence;
            temp.FramePicture  = ext->FramePicture;  

            bCorrected   = bCorrected || (memcmp((mfxU8*)(ext) + bufOffset,(mfxU8*)(&temp) + bufOffset, bufSize)!= 0);
            bUnsupported = bUnsupported || (ext->FieldOutput == MFX_CODINGOPTION_ON);   
        
        }
        m_VideoParamsEx.bFieldCoding = false;
        if (!bProgressiveSequence)
        {
            m_VideoParamsEx.bFieldCoding = (ext && ext->FramePicture == MFX_CODINGOPTION_OFF)? true:false;
        }

        sts = CheckAspectRatioMPEG2(
            pFrameInfo->AspectRatioW,
            pFrameInfo->AspectRatioH, 
            pFrameInfo->Width, 
            pFrameInfo->Height,
            pFrameInfo->CropW,
            pFrameInfo->CropH);

        if (sts < 0)
        {
            if (!m_bInitialized)
            {
                pFrameInfo->AspectRatioW = 0;
                pFrameInfo->AspectRatioH = 0; 
                bInvalid = true;
            }
            else
            {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;            
            }
        }
        if (m_bAVBR_WA)
        {
            bCorrected = AVBR_via_CBR(&m_VideoParamsEx.mfxVideoParams) ? true : bCorrected;
        }

        if (m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod != MFX_RATECONTROL_CBR  &&
            m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod != MFX_RATECONTROL_VBR  &&
            m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            /*if RateControlMethod was undefined MSDK have to use default one */
            m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
        }

        Ipp64f fr = CalculateUMCFramerate(m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.FrameRateExtN,
            m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.FrameRateExtD);

        CorrectProfileLevelMpeg2(m_VideoParamsEx.mfxVideoParams.mfx.CodecProfile, 
            m_VideoParamsEx.mfxVideoParams.mfx.CodecLevel, 
            m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.Width,
            m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.Height,
            fr, 
            m_VideoParamsEx.mfxVideoParams.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            ? 0
            : (mfxU32)(m_VideoParamsEx.mfxVideoParams.mfx.TargetKbps * m_VideoParamsEx.mfxVideoParams.mfx.BRCParamMultiplier * BRC_BITS_IN_KBIT));

        ApplyTargetUsage(&m_VideoParamsEx);

        m_InputFrameOrder = -1;
        m_OutputFrameOrder= -1;
        m_BitstreamLen    = 0;

        m_VideoParamsEx.bAddEOS = (ext && (ext->EndOfSequence == MFX_CODINGOPTION_ON));

        sts = UnlockFrames (m_pGOP, m_pWaitingList,m_pCore);
        MFX_CHECK_STS(sts);

        mfxU16 GopRefDist  = m_VideoParamsEx.mfxVideoParams.mfx.GopRefDist;

        mfxI32 maxFramesInWaitingList  = 0;
        mfxI32 minFramesInWaitingList = 0;
        mfxI32 delayInWaitingList = 0;


        if (par->mfx.EncodedOrder)
        {
            maxFramesInWaitingList   = m_VideoParamsEx.mfxVideoParams.AsyncDepth + ((m_VideoParamsEx.bAddEOS)? 1 : 0);
            minFramesInWaitingList   = (m_VideoParamsEx.bAddEOS)? 1 : 0;
            delayInWaitingList       = minFramesInWaitingList + m_VideoParamsEx.mfxVideoParams.AsyncDepth - 1;
        }
        else
        {
            maxFramesInWaitingList = (GopRefDist + 1)*3;
            minFramesInWaitingList = ((m_VideoParamsEx.bAddEOS)? 1 : 0);
            delayInWaitingList     =  GopRefDist + minFramesInWaitingList; 
        }

        if (m_pGOP)
        {
            if (m_pGOP->GetMaxBFrames() < GopRefDist - 1)
            {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }
            m_pGOP->Reset(m_VideoParamsEx.mfxVideoParams.mfx.GopOptFlag&MFX_GOP_CLOSED, m_VideoParamsEx.mfxVideoParams.mfx.IdrInterval,ext && ext->EndOfSequence !=0,m_VideoParamsEx.mfxVideoParams.mfx.EncodedOrder!=0);
        }
        else
        {
            m_pGOP = new MFXGOP;
            if (!m_pGOP)
            {
                return MFX_ERR_NULL_PTR;
            }
            sts = m_pGOP->Init(GopRefDist - 1, m_VideoParamsEx.mfxVideoParams.mfx.GopOptFlag&MFX_GOP_CLOSED, m_VideoParamsEx.mfxVideoParams.mfx.IdrInterval,ext && ext->EndOfSequence==MFX_CODINGOPTION_ON,m_VideoParamsEx.mfxVideoParams.mfx.EncodedOrder!=0);
            MFX_CHECK_STS(sts);  
        }

        if (m_pWaitingList)
        {
            if (m_pWaitingList->GetMaxFrames() < maxFramesInWaitingList)
            {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }  
            m_pWaitingList->Reset(minFramesInWaitingList, delayInWaitingList);
        }
        else
        {
            m_pWaitingList = new MFXWaitingList;
            if (!m_pWaitingList)
            {
                return MFX_ERR_NULL_PTR;
            }
            sts = m_pWaitingList->Init(maxFramesInWaitingList, minFramesInWaitingList,delayInWaitingList);
            MFX_CHECK_STS(sts);
        }
        m_VideoParamsEx.bRawFrames = bAllowRawFrames ? (m_VideoParamsEx.mfxVideoParams.mfx.TargetUsage > 5):false;

        m_VideoParamsEx.mfxVideoParams.mfx.NumSlice = (mfxU16)((m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.Height)>>4);

        {
            mfxFrameAllocRequest request = {};
            sts = QueryIOSurf(m_pCore, par, &request);
            MFX_CHECK(sts>=0,sts);
            sts = m_InputSurfaces.Reset (par, request.NumFrameMin);
            MFX_CHECK(sts != MFX_ERR_INVALID_VIDEO_PARAM, m_bInitialized ? MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: sts);
            MFX_CHECK_STS(sts);
        } 

        if (bUnsupported)
            return MFX_ERR_UNSUPPORTED;
        if(bInvalid)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        m_bInitialized = true;

        return bCorrected ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
    }

    //virtual mfxStatus Close(void); // same name
    mfxStatus ControllerBase::Close(void)
    {
        mfxStatus sts = MFX_ERR_NONE;

        sts = UnlockFrames (m_pGOP, m_pWaitingList,m_pCore);
        MFX_CHECK_STS(sts);

        if (m_pGOP)
        {
            m_pGOP->Close();
            delete m_pGOP;
            m_pGOP = 0;  
        }
        if (m_pWaitingList)
        {
            m_pWaitingList->Close();
            delete m_pWaitingList;
            m_pWaitingList = 0;
        }
        m_InputSurfaces.Close();

        m_bInitialized = false;


        return MFX_ERR_NONE;
    }

    mfxStatus ControllerBase::GetVideoParam(mfxVideoParam *par)
    {
        MFX_CHECK_NULL_PTR1(par);
        CHECK_VERSION(par->Version);

        memcpy_s(&par->mfx, sizeof(mfxInfoMFX), &m_VideoParamsEx.mfxVideoParams.mfx, sizeof(mfxInfoMFX));
        par->IOPattern = m_VideoParamsEx.mfxVideoParams.IOPattern;
        par->mfx.CodecId = MFX_CODEC_MPEG2;
        mfxExtCodingOption* ext = GetExtCodingOptions(par->ExtParam, par->NumExtParam);
        if (ext)
            ext->FramePicture= (mfxU16)((m_VideoParamsEx.bFieldCoding) ? MFX_CODINGOPTION_OFF : MFX_CODINGOPTION_ON);

        return MFX_ERR_NONE;
    }
    mfxStatus ControllerBase::GetEncodeStat(mfxEncodeStat *stat)
    {
        MFX_CHECK_NULL_PTR1(stat)
            if(!is_initialized())
                return MFX_ERR_NOT_INITIALIZED;

        stat->NumBit   = (mfxU64)(m_BitstreamLen*8);
        stat->NumFrame = m_OutputFrameOrder + 1;

        stat->NumCachedFrame = m_InputFrameOrder - m_OutputFrameOrder;
        return MFX_ERR_NONE;
    }
    mfxStatus ControllerBase::ReorderFrame(mfxEncodeInternalParams *pInInternalParams, mfxFrameSurface1 *in,
        mfxEncodeInternalParams *pOutInternalParams, mfxFrameSurface1 **out)
    {
        if (in)
        {
            if (!m_pWaitingList->AddFrame( in, pInInternalParams))
            {
                return MFX_ERR_NOT_ENOUGH_BUFFER;
            }
        }
        // Fill GOP structure using waiting list
        for(;;)
        {
            sFrameEx         CurFrame = {0};

            if (!m_pWaitingList->GetFrameEx(&CurFrame, in == NULL))
            {
                break;
            }
            if (!m_pGOP->AddFrame(&CurFrame))
            { 
                break;
            }
            m_pWaitingList->MoveOnNextFrame();
        }
        if (!in)
        {
            m_pGOP->CloseGop();
        }

        sFrameEx  CurFrame = {0};

        // Extract next frame from GOP structure 
        if (!m_pGOP->GetFrameExForDecoding(&CurFrame,m_pWaitingList->isNextReferenceIntra(), m_pWaitingList->isNextBFrame(),m_pWaitingList->isLastFrame()))
        {
            return MFX_ERR_MORE_DATA;
        }
        mfxU16 frameType = CurFrame.m_sEncodeInternalParams.FrameType;

        //Correct InternalFlags
        CurFrame.m_sEncodeInternalParams.InternalFlags = (CurFrame.m_bAddHeader)? MFX_IFLAG_ADD_HEADER:0;
        if (CurFrame.m_bAddEOS)
        {
            CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_ADD_EOS;
        } 
        if (CurFrame.m_bOnlyBwdPrediction && isBPredictedFrame(frameType))
        {
            CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_BWD_ONLY;
        }
        // Check frame order parameters
        if (isPredictedFrame(frameType))
        {
            sFrameEx refFrame = {0};
            if (m_pGOP->GetFrameExReference(&refFrame))
            {

                MFX_CHECK((CurFrame.m_FrameOrder > refFrame.m_FrameOrder) && ((Ipp32s)CurFrame.m_FrameOrder - (Ipp32s)refFrame.m_FrameOrder <= m_VideoParamsEx.mfxVideoParams.mfx.GopRefDist) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }
        }
        else if (isBPredictedFrame(frameType))
        {
            if (CurFrame.m_bOnlyBwdPrediction)
            {
                sFrameEx refFrame = {0};
                if (m_pGOP->GetFrameExReference(&refFrame,true))
                {
                    MFX_CHECK((CurFrame.m_FrameOrder < refFrame.m_FrameOrder) && ((Ipp32s)refFrame.m_FrameOrder - (Ipp32s)CurFrame.m_FrameOrder < m_VideoParamsEx.mfxVideoParams.mfx.GopRefDist) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                }      
            }
            else
            {
                sFrameEx refFrameF = {0};
                sFrameEx refFrameB = {0};
                if (m_pGOP->GetFrameExReference(&refFrameF,false) && m_pGOP->GetFrameExReference(&refFrameB,true))
                {
                    MFX_CHECK((CurFrame.m_FrameOrder < refFrameB.m_FrameOrder) && (CurFrame.m_FrameOrder > refFrameF.m_FrameOrder) ,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
                }      
            }  
        }
        *out = CurFrame.m_pFrame; 
        *pOutInternalParams = CurFrame.m_sEncodeInternalParams;

        m_pGOP->ReleaseCurrentFrame();

        return MFX_ERR_NONE;
    }
    mfxStatus ControllerBase::CheckNextFrame(mfxEncodeInternalParams *pOutInternalParams, mfxFrameSurface1 **out)
    {

        // Fill GOP structure using waiting list
        for(;;)
        {
            sFrameEx         CurFrame = {0};

            if (!m_pWaitingList->GetFrameEx(&CurFrame, false))
            {
                break;
            }
            if (!m_pGOP->AddFrame(&CurFrame))
            { 
                break;
            }
            m_pWaitingList->MoveOnNextFrame();
        }

        sFrameEx  CurFrame = {0};

        // Extract next frame from GOP structure 
        if (!m_pGOP->GetFrameExForDecoding(&CurFrame,m_pWaitingList->isNextReferenceIntra(), m_pWaitingList->isNextBFrame(),m_pWaitingList->isLastFrame()))
        {
            return MFX_ERR_MORE_DATA;
        }
        mfxU16 frameType = CurFrame.m_sEncodeInternalParams.FrameType;

        //Correct InternalFlags
        CurFrame.m_sEncodeInternalParams.InternalFlags = (CurFrame.m_bAddHeader)? MFX_IFLAG_ADD_HEADER:0;
        if (CurFrame.m_bAddEOS)
        {
            CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_ADD_EOS;
        } 
        if (CurFrame.m_bOnlyBwdPrediction && isBPredictedFrame(frameType))
        {
            CurFrame.m_sEncodeInternalParams.InternalFlags |= MFX_IFLAG_BWD_ONLY;
        }


        *out = CurFrame.m_pFrame; 
        *pOutInternalParams = CurFrame.m_sEncodeInternalParams;

        m_pGOP->ReleaseCurrentFrame();

        return MFX_ERR_NONE;
    }

    mfxStatus ControllerBase::EncodeFrameCheck(
        mfxEncodeCtrl *ctrl,
        mfxFrameSurface1 *surface,
        mfxBitstream *bs,
        mfxFrameSurface1 **reordered_surface,
        mfxEncodeInternalParams *pInternalParams)
    {
        mfxStatus sts = MFX_ERR_NONE;
        bool bWarning = false;

        MFX_CHECK(is_initialized(),MFX_ERR_NOT_INITIALIZED);
        CHECK_VERSION(bs->Version);
        MFX_CHECK_NULL_PTR2(bs, pInternalParams);
        MFX_CHECK(bs->DataOffset <= 32 , MFX_ERR_UNDEFINED_BEHAVIOR);

        mfxU32 startbspos = bs->DataOffset + bs->DataLength;
        mfxI32 output_buffer_size = (mfxI32)(bs->MaxLength - startbspos);

        MFX_CHECK(output_buffer_size >= (mfxI32)m_VideoParamsEx.mfxVideoParams.mfx.BufferSizeInKB * m_VideoParamsEx.mfxVideoParams.mfx.BRCParamMultiplier * 1000,MFX_ERR_NOT_ENOUGH_BUFFER);
        MFX_CHECK_NULL_PTR1(bs->Data);

        if (surface)
        {
            if ((m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.PicStruct&0x0f) != MFX_PICSTRUCT_UNKNOWN)
            {
                if ((surface->Info.PicStruct&0x0f) != (m_VideoParamsEx.mfxVideoParams.mfx.FrameInfo.PicStruct&0x0f) &&
                    (surface->Info.PicStruct&0x0f) != MFX_PICSTRUCT_UNKNOWN &&
                    (surface->Info.PicStruct&0x0f) != MFX_PICSTRUCT_PROGRESSIVE)
                {
                    bWarning=true;
                }
            }
            else if ((surface->Info.PicStruct&0x0f) == MFX_PICSTRUCT_UNKNOWN)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
            bWarning = bWarning || (!m_InputSurfaces.CheckInputFrame(&surface->Info));
        
            MFX_CHECK(surface->Info.FourCC == MFX_FOURCC_NV12, MFX_ERR_UNDEFINED_BEHAVIOR);

            if (surface->Data.Y)
            {
                MFX_CHECK(surface->Data.Pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
                CHECK_VERSION(surface->Version);
            }
            sts = m_pCore->IncreaseReference(&surface->Data);
            MFX_CHECK_STS(sts);
            m_InputFrameOrder++;

            mfxU16 frameType = (ctrl)? ctrl->FrameType : 0;
            if (m_VideoParamsEx.mfxVideoParams.mfx.EncodedOrder)
            {
                mfxU16 type = frameType & (MFX_FRAMETYPE_I|MFX_FRAMETYPE_P|MFX_FRAMETYPE_B);
                MFX_CHECK ((type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_P || type == MFX_FRAMETYPE_B), MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }

            pInternalParams->FrameType    = frameType;
            pInternalParams->FrameOrder   = (!m_VideoParamsEx.mfxVideoParams.mfx.EncodedOrder)? m_InputFrameOrder:surface->Data.FrameOrder;
            if (ctrl)
            {
                pInternalParams->ExtParam    = ctrl->ExtParam; 
                pInternalParams->NumExtParam = ctrl->NumExtParam;
                pInternalParams->NumPayload  = ctrl->NumPayload;
                pInternalParams->Payload     = ctrl->Payload;
                pInternalParams->QP          = ctrl->QP;
            }
            else
            {
                pInternalParams->ExtParam    = 0; 
                pInternalParams->NumExtParam = 0;
                pInternalParams->NumPayload  = 0;
                pInternalParams->Payload     = 0;
                pInternalParams->QP          = 0;     
            }

            *reordered_surface = GetOpaqSurface(surface);

            if (m_InputFrameOrder < m_pWaitingList->GetDelay())
            {          
                return (mfxStatus)MFX_ERR_MORE_DATA_RUN_TASK; 
            }
            else
            {
                m_nEncodeCalls ++;
                return bWarning? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: MFX_ERR_NONE;
            }
        }
        else
        {
            m_nEncodeCalls ++;
            *reordered_surface = 0;
            return (m_nEncodeCalls <= (mfxU32)m_InputFrameOrder+1)?  MFX_ERR_NONE : MFX_ERR_MORE_DATA;   
        }
    }

    mfxStatus ControllerBase::CheckFrameType(mfxEncodeInternalParams *pInternalParams)
    { 
        mfxU16 type = pInternalParams->FrameType & (MFX_FRAMETYPE_I|MFX_FRAMETYPE_P|MFX_FRAMETYPE_B);
        if (!m_VideoParamsEx.mfxVideoParams.mfx.EncodedOrder)
        { 
            if (type != MFX_FRAMETYPE_I)
            {
                GetFrameTypeMpeg2 (m_nFrameInGOP,
                    m_VideoParamsEx.mfxVideoParams.mfx.GopPicSize, 
                    m_VideoParamsEx.mfxVideoParams.mfx.GopRefDist,
                    m_VideoParamsEx.mfxVideoParams.mfx.GopOptFlag&MFX_GOP_CLOSED,
                    &pInternalParams->FrameType);
            } 
            m_nFrameInGOP = (pInternalParams->FrameType & MFX_FRAMETYPE_I) ? 1 : m_nFrameInGOP + 1;
        }
        else
        {
            MFX_CHECK((type == MFX_FRAMETYPE_I || type == MFX_FRAMETYPE_P || type == MFX_FRAMETYPE_B), MFX_ERR_UNDEFINED_BEHAVIOR);
        }
        return MFX_ERR_NONE;
    }

#define RET_UMC_TO_MFX(umc_ret) ConvertStatusUmc2Mfx(umc_ret)
    static void ConvertFrameParamsToUMC(const mfxFrameParamMPEG2* pFrameParams, UMC::FrameType &frType,
        Ipp32u &picture_structure, 
        Ipp32u &repeat_first_field, 
        Ipp32u &top_field_first, 
        Ipp32u &second_field)
    {
        frType             = ((pFrameParams->FrameType &  MFX_FRAMETYPE_I) ? UMC::I_PICTURE : (pFrameParams->FrameType &  MFX_FRAMETYPE_P ? UMC::P_PICTURE : UMC::B_PICTURE));
        picture_structure  =  pFrameParams->PicStructure;
        repeat_first_field = (pFrameParams->RepeatFirstField) ? 1:0;
        top_field_first    = (pFrameParams->TopFieldFirst) ? 1:0;
        second_field       = (pFrameParams->SecondFieldFlag) ? 1:0;
    }

    void MPEG2BRC_HW::QuantIntoScaleTypeAndCode (Ipp32s quant_value, Ipp32s &q_scale_type, Ipp32s &quantiser_scale_code)
    {
        if(quant_value > 7 && quant_value <= 62) 
        {
            q_scale_type = 0;
            quantiser_scale_code = (quant_value + 1) >> 1;
        } 
        else 
        { // non-linear quantizer
            q_scale_type = 1;
            if(quant_value <= 8) 
            {
                quantiser_scale_code = quant_value;
            }
            else if (quant_value > 62) 
            {
                quantiser_scale_code = 25+((quant_value-64+4)>>3);
            }
        }
        if(quantiser_scale_code < 1) 
        {
            quantiser_scale_code = 1;
        }
        if(quantiser_scale_code > 31) 
        {
            quantiser_scale_code = 31;
        } 
    }
    Ipp32s MPEG2BRC_HW::ChangeQuant(Ipp32s quant_value_old, Ipp32s quant_value_new)
    {
        Ipp32s q_scale_type         = 0;
        Ipp32s quantiser_scale_code = 0;
        Ipp32s quant_value = quant_value_new;


        if(quant_value_old == quant_value) 
        {
            return quant_value;
        }
        QuantIntoScaleTypeAndCode (quant_value_new, q_scale_type, quantiser_scale_code);
        quant_value = ScaleTypeAndCodeIntoQuant (q_scale_type, quantiser_scale_code);

        if (quant_value == quant_value_old)
        {
            if (quant_value_new > quant_value_old)
            {
                if (quantiser_scale_code == 31)
                {
                    return quant_value;                
                }
                else
                {
                    quantiser_scale_code ++;                
                }
            }
            else
            {
                if (quantiser_scale_code == 1)
                {
                    return quant_value;                
                }
                else
                {
                    quantiser_scale_code --;                
                }             
            }
            quant_value = ScaleTypeAndCodeIntoQuant (q_scale_type, quantiser_scale_code);
        }

        return quant_value;
    }
    mfxStatus MPEG2BRC_HW::Init(mfxVideoParam* par)
    {
        mfxStatus   sts = MFX_ERR_NONE;

        m_bConstantQuant = (par->mfx.RateControlMethod == MFX_RATECONTROL_CQP)? 1:0;
        m_bLimitedMode   = 0;

        if (m_bConstantQuant)
        {
            UMC::Mpeg2_BrcParams brcParams;
            UMC::Status ret = UMC::UMC_OK;
            brcParams.frameWidth  = par->mfx.FrameInfo.Width;
            brcParams.frameHeight = par->mfx.FrameInfo.Height;
            brcParams.quant[0] = par->mfx.QPI;
            brcParams.quant[1] = par->mfx.QPP;
            brcParams.quant[2] = par->mfx.QPB;


            if (m_pBRC == NULL)
            {
                m_pBRC = new UMC::MPEG2BRC_CONST_QUNT;
            }
            ret = m_pBRC->Init(&brcParams);
            MFX_CHECK_UMC_STS (ret);

            ret = m_pBRC->GetParams(&brcParams);
            MFX_CHECK_UMC_STS (ret);

            mfxU32 bufferSizeInKB = ((brcParams.maxFrameSize + 999)/1000);
            par->mfx.BRCParamMultiplier = (mfxU16)((bufferSizeInKB + 0x10000) / 0x10000);
            par->mfx.BufferSizeInKB = (mfxU16)(bufferSizeInKB / par->mfx.BRCParamMultiplier);
        }
        else
        {
            UMC::VideoBrcParams brcParams;
            UMC::Status ret = UMC::UMC_OK;

            sts = ConvertVideoParam_Brc(par, &brcParams);
            MFX_CHECK_STS(sts);
            if (brcParams.HRDBufferSizeBytes == 0)
                brcParams.HRDBufferSizeBytes = IPP_MIN(65535000, brcParams.targetBitrate / 4); // limit buffer size with 2 seconds
            if (brcParams.maxBitrate == 0)
                brcParams.maxBitrate = brcParams.targetBitrate;

            switch (par->mfx.FrameInfo.PicStruct)
            { 
            case MFX_PICSTRUCT_PROGRESSIVE:
                brcParams.info.interlace_type = UMC::PROGRESSIVE;
                break;
            case MFX_PICSTRUCT_FIELD_TFF: 
            case MFX_PICSTRUCT_UNKNOWN:
                brcParams.info.interlace_type = UMC::INTERLEAVED_TOP_FIELD_FIRST;
                break;     
            case MFX_PICSTRUCT_FIELD_BFF:
                brcParams.info.interlace_type = UMC::INTERLEAVED_BOTTOM_FIELD_FIRST;
                break;     
            default:
                return MFX_ERR_INVALID_VIDEO_PARAM;
            } 

            if (m_pBRC == NULL)
            {
                m_pBRC = UMC::CreateBRC(UMC::BRC_MPEG2);
                ret = m_pBRC->Init(&brcParams,m_pCore->GetHWType() < MFX_HW_HSW || m_pCore->GetHWType()==MFX_HW_VLV);
                MFX_CHECK_UMC_STS (ret);
            }
            else
            {
                m_pBRC->Close();
                ret = m_pBRC->Init(&brcParams, m_pCore->GetHWType() < MFX_HW_HSW || m_pCore->GetHWType()==MFX_HW_VLV);
                MFX_CHECK_UMC_STS (ret);
            }

            ret = m_pBRC->GetParams(&brcParams);
            MFX_CHECK_UMC_STS (ret);

            m_bufferSizeInKB = (mfxU32)(brcParams.HRDBufferSizeBytes / 1000);
            m_InputBitsPerFrame = (mfxI32)(brcParams.targetBitrate / brcParams.info.framerate);

            mfxU32 maxVal32 = IPP_MAX(
                IPP_MAX(
                (mfxU32)brcParams.HRDInitialDelayBytes / 1000,
                (mfxU32)brcParams.HRDBufferSizeBytes / 1000),
                IPP_MAX(
                (mfxU32)brcParams.targetBitrate / 1000,
                (mfxU32)brcParams.maxBitrate / 1000));

            par->mfx.BRCParamMultiplier = (mfxU16)((maxVal32 + 0x10000) / 0x10000);
            par->mfx.BufferSizeInKB     = (mfxU16)(m_bufferSizeInKB                      / par->mfx.BRCParamMultiplier);
            par->mfx.InitialDelayInKB   = (mfxU16)(brcParams.HRDInitialDelayBytes / 1000 / par->mfx.BRCParamMultiplier);
            par->mfx.TargetKbps         = (mfxU16)(brcParams.targetBitrate        / 1000 / par->mfx.BRCParamMultiplier);
            par->mfx.MaxKbps            = (mfxU16)(brcParams.maxBitrate           / 1000 / par->mfx.BRCParamMultiplier);


            mfxU32 MBcount = (par->mfx.FrameInfo.Width/16)*(par->mfx.FrameInfo.Height/16);

            m_MinFrameSizeBits[0] = 16 * 6 * MBcount + 140 + (par->mfx.FrameInfo.Width/16)*32;
            m_MinFrameSizeBits[1] = 1*MBcount + 140 + (par->mfx.FrameInfo.Width/16)*32; 
            m_MinFrameSizeBits[2] = 1*MBcount + 140 + (par->mfx.FrameInfo.Width/16)*32; 

            m_MinFieldSizeBits[0] = 12 * 6 * (MBcount/2) + 140 + (par->mfx.FrameInfo.Width/16)*32;
            m_MinFieldSizeBits[1] = 1*MBcount/2 + 140 + (par->mfx.FrameInfo.Width/16)*32; 
            m_MinFieldSizeBits[2] = 1*MBcount/2 + 140 + (par->mfx.FrameInfo.Width/16)*32; 


            mfxU32 GOPLengthBits     = ((mfxU32)(brcParams.maxBitrate/brcParams.info.framerate))*brcParams.GOPPicSize;
            mfxI32 numPFrames        = (brcParams.GOPPicSize/brcParams.GOPRefDist > 1) ? brcParams.GOPPicSize/brcParams.GOPRefDist - 1:0;
            mfxI32 numBFrames        = (brcParams.GOPPicSize > numPFrames + 1) ? brcParams.GOPPicSize - numPFrames - 1:0;

            mfxU32 minGOPLengthBits  = m_MinFrameSizeBits[0] + numPFrames * m_MinFrameSizeBits[1] + numBFrames *m_MinFrameSizeBits[2];

            m_GopSize       = par->mfx.GopPicSize;
            m_FirstGopSize  = (m_GopSize - 1)/par->mfx.GopRefDist * par->mfx.GopRefDist + 1;


            if (GOPLengthBits < minGOPLengthBits)
            {
                return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
            }  

        }
        return sts;      
    }
    void MPEG2BRC_HW::Close ()
    {
        if (m_pBRC)
        {
            UMC::DeleteBRC(&m_pBRC);
        }
        m_pBRC = 0;

        m_bConstantQuant = 0;
        m_MinFrameSizeBits [0] = 0;
        m_MinFrameSizeBits [1] = 0;
        m_MinFrameSizeBits [2] = 0;

        m_MinFieldSizeBits [0] = 0;
        m_MinFieldSizeBits [1] = 0;
        m_MinFieldSizeBits [2] = 0;

        m_FirstGopSize = 0;
        m_GopSize = 0;
        m_bufferSizeInKB = 0;
        m_InputBitsPerFrame = 0;
        m_bLimitedMode = 0;
    }
    mfxStatus MPEG2BRC_HW::StartNewFrame(const mfxFrameParamMPEG2 *pFrameParams, mfxI32 recode)
    {
        UMC::Status ret = UMC::UMC_OK;
        Ipp32u        picture_structure  = 0;
        Ipp32u        repeat_first_field = 0;
        Ipp32u        top_field_first    = 0;
        Ipp32u        second_field       = 0;
        UMC::FrameType frType   = ((pFrameParams->FrameType &  MFX_FRAMETYPE_I) ? 
            UMC::I_PICTURE : (pFrameParams->FrameType &  MFX_FRAMETYPE_P ? UMC::P_PICTURE : UMC::B_PICTURE));

        if (UMC::BRC_RECODE_NONE == recode )
        {
            if (pFrameParams->FrameType &  (MFX_FRAMETYPE_P|MFX_FRAMETYPE_I))
            {
                m_bLimitedMode = 0;
            }
        }
        if (UMC::BRC_RECODE_NONE == recode && m_bLimitedMode)
        {
            recode = UMC::BRC_EXT_FRAMESKIP;
        }

        ConvertFrameParamsToUMC(pFrameParams, frType,picture_structure,repeat_first_field, top_field_first,second_field);
        ret = m_pBRC->SetPictureFlags(frType, picture_structure, repeat_first_field, top_field_first, second_field);
        MFX_CHECK_UMC_STS (ret);
        ret = m_pBRC->PreEncFrame(frType,recode);
        MFX_CHECK_UMC_STS (ret);

        return MFX_ERR_NONE;
    }

    mfxStatus MPEG2BRC_HW::SetQuantDCPredAndDelay(mfxFrameCUC *pCUC, mfxI32 recode)
    {
        UMC::FrameType  frType   = ((pCUC->FrameParam->MPEG2.FrameType &  MFX_FRAMETYPE_I) ? 
            UMC::I_PICTURE : (pCUC->FrameParam->MPEG2.FrameType &  MFX_FRAMETYPE_P ? UMC::P_PICTURE : UMC::B_PICTURE));

        Ipp32s        quant_value             = m_pBRC->GetQP(frType);
        Ipp32s        q_scale_type            = 0;
        Ipp32s        quantiser_scale_code    = 0;
        UMC::Status   ret                     = UMC::UMC_OK;
        Ipp64f        hrdBufFullness          = 0;
        UMC::VideoBrcParams brcParams;

        QuantIntoScaleTypeAndCode (quant_value, q_scale_type, quantiser_scale_code);

        if (pCUC->FrameParam->MPEG2.FrameType &  MFX_FRAMETYPE_I)
        {
            if(quant_value < 15)
                pCUC->FrameParam->MPEG2.IntraVLCformat = 1;
            else
                pCUC->FrameParam->MPEG2.IntraVLCformat = 0;
        }

        pCUC->FrameParam->MPEG2.QuantScaleType = q_scale_type;
        for (int i = 0; i<(int)pCUC->NumMb;i++)
        {
            pCUC->MbParam->Mb[i].MPEG2.QpScaleCode = (mfxU8)quantiser_scale_code;
            pCUC->MbParam->Mb[i].MPEG2.QpScaleType = (mfxU8)q_scale_type;  
        }     

        ret = m_pBRC->GetHRDBufferFullness(&hrdBufFullness, recode);
        MFX_CHECK_UMC_STS (ret);
        ret = m_pBRC->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (ret);

        if (UMC::BRC_VBR == brcParams.BRCMode || m_bConstantQuant)
        {
            pCUC->FrameParam->MPEG2.VBVDelay = 0xffff;
        }
        if (pCUC->FrameParam->MPEG2.VBVDelay != 0xffff)
        {
            pCUC->FrameParam->MPEG2.VBVDelay = (Ipp32s)((hrdBufFullness  - 32) * 90000.0 / brcParams.maxBitrate);
        }

        if(quant_value  >= 8)
        {
            pCUC->FrameParam->MPEG2.IntraDCprecision = 0;
        }
        else if(quant_value  >= 4)
        {
            pCUC->FrameParam->MPEG2.IntraDCprecision = 1;
        }
        else
        {
            pCUC->FrameParam->MPEG2.IntraDCprecision = 2;
        }

        return MFX_ERR_NONE;
    }
    mfxStatus MPEG2BRC_HW::SetQuantDCPredAndDelay(mfxFrameParamMPEG2 *pFrameParams, mfxU8 *pQuant)
    {
        UMC::FrameType  frType   = ((pFrameParams->FrameType &  MFX_FRAMETYPE_I) ? 
                                    UMC::I_PICTURE : (pFrameParams->FrameType &  MFX_FRAMETYPE_P ? UMC::P_PICTURE : UMC::B_PICTURE));

        Ipp32s        quant_value             = m_pBRC->GetQP(frType);
        Ipp32s        q_scale_type            = 0;
        Ipp32s        quantiser_scale_code    = 0;
        UMC::Status   ret                     = UMC::UMC_OK;
        Ipp64f        hrdBufFullness          = 0;
        UMC::VideoBrcParams brcParams;

        QuantIntoScaleTypeAndCode (quant_value, q_scale_type, quantiser_scale_code);

        if (pFrameParams->FrameType &  MFX_FRAMETYPE_I)
        {
            if(quant_value < 15)
                pFrameParams->IntraVLCformat = 1;
            else
                pFrameParams->IntraVLCformat = 0;
        }

        pFrameParams->QuantScaleType = q_scale_type;
        
        ret = m_pBRC->GetHRDBufferFullness(&hrdBufFullness, 0);
        MFX_CHECK_UMC_STS (ret);
        ret = m_pBRC->GetParams(&brcParams);
        MFX_CHECK_UMC_STS (ret);

        pFrameParams->VBVDelay = 0xffff;
        
        if(quant_value  >= 8)
        {
            pFrameParams->IntraDCprecision = 0;
        }
        else if(quant_value  >= 4)
        {
            pFrameParams->IntraDCprecision = 1;
        }
        else
        {
            pFrameParams->IntraDCprecision = 2;
        }
        *pQuant = (mfxU8)quantiser_scale_code;

        return MFX_ERR_NONE;
    }
    mfxStatus MPEG2BRC_HW::UpdateBRC(const mfxFrameParamMPEG2 *pParams, mfxBitstream* pBitsream, mfxU32 bitsize, mfxU32 numEncodedFrame, bool bNotEnoughBuffer ,mfxI32 &recode)
    {
        if (m_bConstantQuant)
        {
            return bNotEnoughBuffer ? MFX_ERR_NOT_ENOUGH_BUFFER: MFX_ERR_NONE; 
        }

        UMC::BRCStatus  hrdSts      = UMC::BRC_OK;
        mfxStatus       sts         = MFX_ERR_NONE;
        Ipp32s          framestoI   = 0;
        Ipp32s          gopSize     = 0;
        Ipp32s          maxSize = 0, minSize = 0;
        Ipp64f          buffullness = 0;
        Ipp32s          buffullnessbyI = 0;

        UMC::VideoBrcParams brcParams;
        UMC::FrameType      frType   = ((pParams->FrameType &  MFX_FRAMETYPE_I) ? 
            UMC::I_PICTURE : (pParams->FrameType &  MFX_FRAMETYPE_P ? UMC::P_PICTURE : UMC::B_PICTURE));


        if (numEncodedFrame >= m_FirstGopSize) 
        {
            gopSize = m_GopSize;
            framestoI = gopSize - ((numEncodedFrame - m_FirstGopSize) % gopSize) - 1;
        } 
        else 
        {
            gopSize = m_FirstGopSize;
            framestoI = gopSize - numEncodedFrame - 1;
        }
        if (bNotEnoughBuffer)
        {
            bitsize = (m_bufferSizeInKB+1)*1000*8;            
        }

        hrdSts = m_pBRC->PostPackFrame(frType, bitsize, 0, recode);

        m_pBRC->GetHRDBufferFullness(&buffullness, 0);
        m_pBRC->GetMinMaxFrameSize(&minSize, &maxSize);
        m_pBRC->GetParams(&brcParams);

        if (hrdSts == UMC::BRC_OK) 
        {
            Ipp32s inputbitsPerPic = m_InputBitsPerFrame;
            Ipp32s minbitsPerPredPic = 0, minbitsPerIPic = 0;
            recode = 0;

            if (pParams->FieldPicFlag)
            {
                minbitsPerPredPic   = m_MinFieldSizeBits[1];
                minbitsPerIPic      = m_MinFieldSizeBits[0];
                inputbitsPerPic     >>= 1;
                framestoI           *= 2;
                if (!pParams->SecondFieldFlag)
                {
                    framestoI--;
                }
            } 
            else 
            {
                minbitsPerPredPic   = m_MinFrameSizeBits[1];
                minbitsPerIPic      = m_MinFrameSizeBits[0];
            }
            buffullnessbyI = (Ipp32s)buffullness + framestoI * (inputbitsPerPic - minbitsPerPredPic);

            if (buffullnessbyI < minbitsPerIPic ||
                (pParams->FieldPicFlag && !pParams->SecondFieldFlag && 
                bitsize*2 > (mfxU32)(inputbitsPerPic + maxSize) )) 
            {
                if (!m_bLimitedMode) 
                {
                    Ipp32s quant     = m_pBRC->GetQP(frType);
                    Ipp32s new_quant = ChangeQuant(quant, quant+2);

                    recode = UMC::BRC_RECODE_EXT_QP;

                    if (new_quant == quant ) 
                    {
                        m_bLimitedMode = true;
                        recode = UMC::BRC_RECODE_EXT_PANIC;
                    }
                    m_pBRC->SetQP(new_quant, frType);                           
                } 
                else 
                {
                    if (!bNotEnoughBuffer)
                    {
                        sts = MFX_ERR_NONE;
                    }
                    else
                    {
                        sts = MFX_ERR_NOT_ENOUGH_BUFFER;
                    }
                }
            }
        }
        else
        {
            if (!(hrdSts & UMC::BRC_NOT_ENOUGH_BUFFER)) 
            {
                recode = UMC::BRC_RECODE_QP;
            } 
            else 
            {
                if (hrdSts & UMC::BRC_ERR_SMALL_FRAME) 
                {
                    mfxI32 maxSize = 0, minSize = 0;
                    Ipp8u *p = pBitsream->Data + pBitsream->DataOffset + pBitsream->DataLength;
                    m_pBRC->GetMinMaxFrameSize(&minSize, &maxSize);
                    if (bitsize < (mfxU32)minSize && pBitsream->DataOffset + 1 + pBitsream->DataLength < pBitsream->MaxLength)
                    {
                        mfxU32 nBytes = ((mfxU32)minSize - bitsize + 7)/8;
                        nBytes = (pBitsream->DataOffset  + pBitsream->DataLength + nBytes < pBitsream->MaxLength) ? nBytes : pBitsream->MaxLength - (pBitsream->DataOffset + pBitsream->DataLength);
                        memset (p,0,nBytes);
                        pBitsream->DataLength += nBytes;
                        bitsize += (nBytes*8);
                        // p += nBytes;
                    }
                    m_pBRC->PostPackFrame(frType, bitsize, 0, recode);
                    recode = UMC::BRC_RECODE_NONE;

                } 
                else 
                {
                    if (!m_bLimitedMode)
                    {
                        m_bLimitedMode = true;
                        recode = UMC::BRC_RECODE_EXT_PANIC;                        
                    }
                    else
                    {
                        sts = MFX_ERR_NOT_ENOUGH_BUFFER;                        
                    }
                }
            }
        }
        return sts;

    }

#define isNONLocked(pSurface) (pSurface->Data.Y == 0)
    
    mfxStatus FrameStore::GetInternalFrame(mfxFrameSurface1** ppFrame)
    {
        mfxFrameSurface1*   pFrames     = m_pFramesStore;
        mfxU32              numFrames   = m_nFrames;   

        for (mfxU32 i = 0; i < numFrames; i++)
        {
            if (!pFrames[i].Data.Locked)
            {
                *ppFrame = &pFrames[i];
                return MFX_ERR_NONE;
            }        
        }
        return MFX_ERR_NOT_FOUND;    
    }
    mfxStatus FrameStore::ReleaseFrames ()
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (m_pRefFrame[0])
        {
            sts = m_pCore->DecreaseReference(&m_pRefFrame[0]->Data);
            MFX_CHECK_STS (sts);      
        }
        if (m_pRefFrame[1])
        {
            sts = m_pCore->DecreaseReference(&m_pRefFrame[1]->Data);
            MFX_CHECK_STS (sts);      
        }
        if (m_pRawFrame[0])
        {
            sts = m_pCore->DecreaseReference(&m_pRawFrame[0]->Data);
            MFX_CHECK_STS (sts);      
        }
        if (m_pRawFrame[1])
        {
            sts = m_pCore->DecreaseReference(&m_pRawFrame[1]->Data);
            MFX_CHECK_STS (sts);      
        }

        m_pRefFrame[0]   = 0;
        m_pRefFrame[1]   = 0;

        m_pRawFrame[0]   = 0;
        m_pRawFrame[1]   = 0;

        m_nFrame = 0;
        m_nRefFrame[0] = 0;
        m_nRefFrame[1] = 0;

        m_nLastRefBeforeIntra = -1;
        m_nLastRef = -1;

        return sts;
    
    }
    mfxStatus FrameStore::Init(bool bRawFrame, mfxU16 InputFrameType, bool bHW, mfxU32 mTasks, mfxFrameInfo* pFrameInfo, bool bProtected)
    {
        mfxStatus sts = MFX_ERR_NONE;
        mfxU32 num = 0;
        mfxU16 type=0;
        bool bHWInput = ((InputFrameType & MFX_MEMTYPE_DXVA2_DECODER_TARGET) != 0);

        m_InputType = InputFrameType;
        m_bHWFrames = bHW;        
        m_bRawFrame  = bRawFrame;

        ReleaseFrames();

        if (bHWInput != bHW)
        {
            num += mTasks;
            if (m_bRawFrame)
            {
                num += mTasks*2;            
            }
        }
        num += mTasks*3;  // reconstructed frames     

        m_request.NumFrameSuggested = m_request.NumFrameMin = (mfxU16)num;
        type = bHW ?
            (mfxU16)(MFX_MEMTYPE_INTERNAL_FRAME |MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_FROM_ENCODE):
            (mfxU16)(MFX_MEMTYPE_INTERNAL_FRAME |MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE); 

#ifdef PAVP_SUPPORT
        MFX_CHECK(bHW||(!bProtected),MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        type = (mfxU16)((bProtected) ? type | MFX_MEMTYPE_PROTECTED:type);
#else
        MFX_CHECK(!bProtected,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
#endif


        if (m_response.NumFrameActual>0)
        {
            MFX_CHECK(m_request.Type == type, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);  
            MFX_CHECK(m_request.Info.Width >= pFrameInfo->Width && m_request.Info.Height >= pFrameInfo->Height, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            MFX_CHECK(m_response.NumFrameActual >= m_request.NumFrameMin, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            m_nFrames = m_response.NumFrameActual;
            for (mfxU32 i=0; i<m_nFrames; i++)
            {
                m_pFramesStore[i].Data.Locked = 0;
            }
        } 
        else if (m_request.NumFrameSuggested)
        {
            m_request.Type = type;
            memcpy_s (&m_request.Info, sizeof(mfxFrameInfo), pFrameInfo, sizeof(mfxFrameInfo)); 

            sts = m_pCore->AllocFrames(&m_request, &m_response);
            MFX_CHECK_STS(sts);

            if (m_response.NumFrameActual < m_request.NumFrameMin)
                return MFX_ERR_MEMORY_ALLOC;

            m_nFrames = m_response.NumFrameActual;

            if (m_pFramesStore)
                return MFX_ERR_MEMORY_ALLOC;

            m_pFramesStore = new mfxFrameSurface1 [m_nFrames];
            MFX_CHECK_NULL_PTR1(m_pFramesStore);

            memset (m_pFramesStore,0,sizeof(mfxFrameSurface1)*m_nFrames);

            for (mfxU32 i=0; i<m_nFrames; i++)
            {
                m_pFramesStore[i].Data.MemId = m_response.mids[i];
                m_pFramesStore[i].Info = m_request.Info;
                m_pFramesStore[i].Data.reserved[0] = 0x01;
            }
        }
        return MFX_ERR_NONE;    
    }
    mfxStatus FrameStore::NextFrame(mfxFrameSurface1 *pInputFrame, mfxU32 nFrame, bool bReference, bool bIntra, FramesSet *pFrames)
    {
        mfxStatus sts           = MFX_ERR_NONE;
        bool      bHWInput      = (m_InputType & MFX_MEMTYPE_DXVA2_DECODER_TARGET)? true: false ;
        mfxU16    localFrameType= m_bHWFrames?
            (mfxU16)(MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_FROM_ENCODE):
        (mfxU16)(MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE);

        m_nFrame = nFrame;

        if (bIntra)
        {
            m_nLastRefBeforeIntra = m_nLastRef;         
        }
        if (bReference)
        {
            m_nLastRef = nFrame;
        }
        if (bHWInput == m_bHWFrames)
        {
            pFrames->m_pInputFrame = pInputFrame;
        }
        else
        {
            mfxFrameSurface1* pTmpFrame = 0;
            sts = GetInternalFrame(&pTmpFrame);
            MFX_CHECK_STS(sts);
            sts = m_pCore->DoFastCopyWrapper(pTmpFrame, localFrameType, pInputFrame, m_InputType);
            MFX_CHECK_STS(sts);
            pTmpFrame->Data.TimeStamp = pInputFrame->Data.TimeStamp;
            pFrames->m_pInputFrame = pTmpFrame;
        } 
        sts = m_pCore->IncreaseReference(&pFrames->m_pInputFrame->Data);
        MFX_CHECK_STS(sts);

        // prepare reconstructed 
        {
            mfxFrameSurface1* pTmpFrame = 0;
            sts = GetInternalFrame(&pTmpFrame);
            MFX_CHECK_STS(sts);
            pFrames->m_pRecFrame = pTmpFrame;  
            sts = m_pCore->IncreaseReference(&pFrames->m_pRecFrame->Data);
            MFX_CHECK_STS(sts);
        }

        if (bReference)
        {
            m_nRefFrame[0] = m_nRefFrame[1];
            m_nRefFrame[1] = nFrame;

            // prepare reference 
            if (m_pRefFrame[0])
            {
                sts = m_pCore->DecreaseReference(&m_pRefFrame[0]->Data);
                MFX_CHECK_STS(sts);
            }
            m_pRefFrame[0] = m_pRefFrame[1];
            m_pRefFrame[1] = pFrames->m_pRecFrame;
            sts = m_pCore->IncreaseReference(&m_pRefFrame[1]->Data);
            MFX_CHECK_STS(sts); 

            if (m_bRawFrame)
            {
                if (m_pRawFrame[0])
                {
                    sts = m_pCore->DecreaseReference(&m_pRawFrame[0]->Data);
                    MFX_CHECK_STS(sts);
                }
                m_pRawFrame[0] = m_pRawFrame[1];
                m_pRawFrame[1] = pFrames->m_pInputFrame;
                sts = m_pCore->IncreaseReference(&m_pRawFrame[1]->Data);
                MFX_CHECK_STS(sts);             
            }
        }
        // Set Reference frames
        pFrames->m_pRefFrame[0] = m_pRefFrame[0];
        pFrames->m_pRefFrame[1] = m_pRefFrame[1];
        pFrames->m_pRawFrame[0] = m_pRawFrame[0];
        pFrames->m_pRawFrame[1] = m_pRawFrame[1];

        pFrames->m_nFrame       = m_nFrame;
        pFrames->m_nLastRefBeforeIntra   = m_nLastRefBeforeIntra;
        pFrames->m_nLastRef = m_nLastRef;
        pFrames->m_nRefFrame[0] = m_nRefFrame [0];
        pFrames->m_nRefFrame[1] = m_nRefFrame [1];

        sts = pFrames->LockRefFrames(m_pCore);
        MFX_CHECK_STS(sts); 

        return sts;  
    }
    mfxStatus FrameStore::Close()
    {
        mfxStatus sts = MFX_ERR_NONE;

        sts = ReleaseFrames();
        MFX_CHECK_STS(sts);

        if (m_response.NumFrameActual)
        {
            sts = m_pCore->FreeFrames(&m_response);
            m_response.NumFrameActual = 0;        
        }

        if (m_nFrames)
        {
            delete [] m_pFramesStore;
            m_nFrames = 0;
            m_pFramesStore = 0;
        }

        m_InputType = 0;

        memset(&m_request,0,  sizeof(mfxFrameAllocRequest));
        memset(&m_response,0, sizeof(mfxFrameAllocResponse));

        return sts;   
    }    
    
    
    mfxStatus EncodeFrameTaskHybrid::Close()
    {
        mfxStatus sts = MFX_ERR_NONE;
        sts = ReleaseFrames ();
        MFX_CHECK_STS (sts);

        if (m_pFrameCUC)
        {
            m_pCore->FreeBuffer(m_FrameCUCId);
            m_FrameCUCId = 0;
        }
        m_pFrameCUC = 0; 
        m_taskStatus = NOT_STARTED;
        memset (&m_FrameEnc, 0, sizeof(FramesSet));
        memset (&m_FramePak, 0, sizeof(FramesSet));

        return sts;    
    }
    mfxStatus EncodeFrameTaskHybrid::Init(mfxVideoParam* pVideoParams, VideoCORE* pCore, mfxU16 refEncType, mfxU16 refPakType)
    {
        mfxStatus sts   = MFX_ERR_NONE;
        m_pCore         = pCore;
        m_refEncType    = refEncType;
        m_refPakType    = refPakType;

        m_bCopyReference = (refEncType == refPakType || (refEncType & MFX_MEMTYPE_EXTERNAL_FRAME))? false: true ;
        memset(&m_sEncodeInternalParams,0,sizeof(mfxEncodeInternalParams));

        if (!m_pFrameCUC)
        {
            mfxU32 numSlices = pVideoParams->mfx.NumSlice;
            mfxU32 numMBs    = numSlices*((pVideoParams->mfx.FrameInfo.Width)>>4);
            mfxU8* pBuffer   = 0;
            mfxU8* p         = 0;
            mfxU32 numBlocks = (pVideoParams->mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422) ? 8 : 6;
            mfxU32 numCoeffs  = numMBs*numBlocks*64;

            mfxU32 bufferSize = sizeof(mfxFrameCUC) + sizeof(mfxFrameParam)+ sizeof(mfxSliceParam)*numSlices + 
                sizeof(mfxMbParam) + sizeof(mfxMbCode)*numMBs + sizeof(mfxExtMpeg2Coeffs) + 
                sizeof(mfxI16)*numCoeffs + sizeof(mfxExtBuffer**)+ 64 + sizeof(mfxFrameSurface) + sizeof(mfxFrameData*)*4;

            sts = m_pCore->AllocBuffer(bufferSize,MFX_MEMTYPE_SYSTEM_MEMORY,&m_FrameCUCId);
            MFX_CHECK_STS(sts);

            sts = m_pCore->LockBuffer(m_FrameCUCId,&pBuffer);
            MFX_CHECK_STS(sts);

            memset(pBuffer,0,bufferSize);

            p = pBuffer;
            m_pFrameCUC = (mfxFrameCUC*)p;
            p += sizeof(mfxFrameCUC);

            m_pFrameCUC->FourCC     = MFX_CUC_MPEG2_MV;
            m_pFrameCUC->FrameCucSz = bufferSize;

            m_pFrameCUC->FrameParam = (mfxFrameParam*)p;
            p += sizeof(mfxFrameParam);
            m_pFrameCUC->FrameParam->MPEG2.CucId = MFX_CUC_MPEG2_FRAMEPARAM;
            m_pFrameCUC->FrameParam->MPEG2.CucSz = sizeof(mfxFrameParam);

            m_pFrameCUC->SliceParam = (mfxSliceParam*)p;
            m_pFrameCUC->NumSlice   =  (mfxU16)numSlices;

            for (mfxU32 i=0; i<numSlices; i++)
            {
                m_pFrameCUC->SliceParam[i].MPEG2.CucId = MFX_CUC_MPEG2_SLICEPARAM;
                m_pFrameCUC->SliceParam[i].MPEG2.CucSz = sizeof(mfxSliceParam);
                p += sizeof(mfxSliceParam);
            }

            m_pFrameCUC->MbParam = (mfxMbParam*)p;
            m_pFrameCUC->NumMb   = (mfxU16) numMBs;        
            m_pFrameCUC->MbParam->CucId = MFX_CUC_MPEG2_MBPARAM;
            m_pFrameCUC->MbParam->CucSz = sizeof(mfxMbParam);
            p += sizeof(mfxMbParam);
            m_pFrameCUC->MbParam->Mb = (mfxMbCode*)p;
            p += sizeof(mfxMbCode)*numMBs;

            m_pFrameCUC->FrameSurface = (mfxFrameSurface*) p;
            p+= sizeof(mfxFrameSurface);

            memcpy_s(&m_pFrameCUC->FrameSurface->Info, sizeof(mfxFrameInfo), &pVideoParams->mfx.FrameInfo, sizeof(mfxFrameInfo));
            m_pFrameCUC->FrameSurface->NumFrameData = 6;
            m_pFrameCUC->FrameSurface->Data = (mfxFrameData **)p;
            p += sizeof(mfxFrameData *)*6;

            m_pFrameCUC->FrameSurface->Data[0] = 0;
            m_pFrameCUC->FrameSurface->Data[1] = 0;
            m_pFrameCUC->FrameSurface->Data[2] = 0;
            m_pFrameCUC->FrameSurface->Data[3] = 0;
            m_pFrameCUC->FrameSurface->Data[4] = 0;
            m_pFrameCUC->FrameSurface->Data[5] = 0;

            m_pFrameCUC->ExtBuffer = (mfxExtBuffer**)p;  
            p+= sizeof(mfxExtBuffer*);

            m_pFrameCUC->NumExtBuffer = 1;

            m_pFrameCUC->ExtBuffer[0]  = (mfxExtBuffer*)p;
            mfxExtMpeg2Coeffs* sCoeffs = (mfxExtMpeg2Coeffs*)(m_pFrameCUC->ExtBuffer[0]);
            p += sizeof(mfxExtMpeg2Coeffs);

            sCoeffs->Header.BufferId = MFX_CUC_MPEG2_RESCOEFF;
            sCoeffs->Header.BufferSz = sizeof(mfxExtMpeg2Coeffs) + sizeof(mfxI16)*numCoeffs;
            sCoeffs->Coeffs = UMC::align_pointer<Ipp16s*>(p);

            m_initNumSlice = numSlices;
            m_initNumMb    = numMBs;
        }
        else
        {
            mfxU32 numSlices = pVideoParams->mfx.NumSlice;
            mfxU32 numMBs    = numSlices*((pVideoParams->mfx.FrameInfo.Width)>>4);
            MFX_CHECK(numSlices >= numSlices && numMBs >= numMBs, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
        m_taskStatus = NOT_STARTED;

        sts = ReleaseFrames ();
        MFX_CHECK_STS (sts);

        m_FrameEnc.Reset();
        m_FramePak.Reset();

        return sts;
    }

    mfxStatus EncodeFrameTaskHybrid::ReleaseFrames ()
    {
        mfxStatus sts = MFX_ERR_NONE;
        sts = m_FrameEnc.ReleaseFrames(m_pCore);
        MFX_CHECK_STS(sts);
        sts = m_FramePak.ReleaseFrames(m_pCore);
        MFX_CHECK_STS(sts);  
        return sts;    
    }
    mfxStatus EncodeFrameTaskHybrid::FillFrameParams (mfxU8 frameType, 
                                                      mfxVideoParamEx_MPEG2 *pExParams, 
                                                      mfxU16 surface_pict_struct, 
                                                      bool bBackwOnly)
    {
        mfxStatus  sts = MFX_ERR_NONE;
        mfxFrameParamMPEG2*     pFrameParams = &m_pFrameCUC->FrameParam->MPEG2;

        sts = FillMFXFrameParams(pFrameParams,frameType,pExParams,surface_pict_struct,bBackwOnly);
        MFX_CHECK_STS(sts);

        if (pFrameParams->RefPicFlag) 
        {
            MFX_CHECK(m_FrameEnc.m_nRefFrame[0] <= m_FrameEnc.m_nRefFrame[1] && (m_FrameEnc.m_nRefFrame[1] - m_FrameEnc.m_nRefFrame[0])<= pExParams->mfxVideoParams.mfx.GopRefDist, MFX_ERR_UNSUPPORTED);
            MFX_CHECK((mfxI32)(m_FrameEnc.m_nFrame) >= m_FrameEnc.m_nLastRefBeforeIntra, MFX_ERR_UNSUPPORTED);
        } 
        else 
        {
            MFX_CHECK(m_FrameEnc.m_nFrame <= m_FrameEnc.m_nRefFrame[1] && m_FrameEnc.m_nFrame >= m_FrameEnc.m_nRefFrame[0],MFX_ERR_UNSUPPORTED);
        }
        pFrameParams->TemporalReference = (mfxU16)(m_FrameEnc.m_nFrame - m_FrameEnc.m_nLastRefBeforeIntra - 1);
        m_pFrameCUC->MbParam->NumMb    = pFrameParams->NumMb;
        m_pFrameCUC->NumMb             = pFrameParams->NumMb;
        m_pFrameCUC->NumSlice          = pFrameParams->FrameHinMbMinus1+1;

        return sts;
    }
    void EncodeFrameTaskHybrid::FillFrameDataParameters (bool bEnc)
    {
        FramesSet *p = (bEnc)? &m_FrameEnc: &m_FramePak;
        if (p->m_pRecFrame)
        {
            p->m_pRecFrame->Data.FrameOrder = p->m_nFrame;
        }
#ifdef __SW_ENC
        p->m_pInputFrame->Data.FrameOrder = p->m_nFrame;
        if (p->m_pRecFrame)
        {
            p->m_pRecFrame->Data.FrameOrder = p->m_nFrame;
        }
        if (p->m_pRefFrame[0])
        {
            p->m_pRefFrame[0]->Data.FrameOrder = p->m_nRefFrame[0];
        }
        if (p->m_pRefFrame[1])
        {
            p->m_pRefFrame[1]->Data.FrameOrder = p->m_nRefFrame[1];
        }
        if (p->m_pRawFrame[0])
        {
            p->m_pRawFrame[0]->Data.FrameOrder = p->m_nRefFrame[0];
        }
        if (p->m_pRawFrame[1])
        {
            p->m_pRawFrame[1]->Data.FrameOrder = p->m_nRefFrame[1];
        }
#endif
        m_pFrameCUC->FrameSurface->Data[0] = &p->m_pInputFrame->Data;
        m_pFrameCUC->FrameSurface->Data[1] = &p->m_pRecFrame->Data;
        m_pFrameCUC->FrameSurface->Data[2] = p->m_pRefFrame[0]? &p->m_pRefFrame[0]->Data : 0;
        m_pFrameCUC->FrameSurface->Data[3] = p->m_pRefFrame[1]? &p->m_pRefFrame[1]->Data : 0;
        m_pFrameCUC->FrameSurface->Data[4] = p->m_pRawFrame[0]? &p->m_pRawFrame[0]->Data : 0;
        m_pFrameCUC->FrameSurface->Data[5] = p->m_pRawFrame[1]? &p->m_pRawFrame[1]->Data : 0;

        m_pFrameCUC->FrameParam->MPEG2.CurrFrameLabel = 0;
        m_pFrameCUC->FrameParam->MPEG2.RecFrameLabel = 1;
        m_pFrameCUC->FrameParam->MPEG2.RefFrameListB [0][0] = (p->m_pRawFrame[0])? 4:2;
        m_pFrameCUC->FrameParam->MPEG2.RefFrameListB [1][0] = (p->m_pRawFrame[1])? 5:3;
    }
    mfxStatus EncodeFrameTaskHybrid::FillSliceMBParams()
    {
        mfxSliceParam*  pSlice          = m_pFrameCUC->SliceParam;
        mfxU32          numSlices       = m_pFrameCUC->NumSlice;
        mfxMbCode*      pMB             = m_pFrameCUC->MbParam->Mb;
        mfxU32          numMBs          = m_pFrameCUC->NumMb;
        mfxU32          numMBsInSlice   = m_pFrameCUC->FrameParam->MPEG2.FrameWinMbMinus1+1;
        mfxU32          num         = 0;

        for (mfxU32 i = 0; i < numSlices;i++)
        {
            mfxSliceParamMPEG2 *pSliceParam = &pSlice[i].MPEG2;
            pSliceParam->FirstMbX  = 0;
            pSliceParam->FirstMbY  = (mfxU8)i;
            pSliceParam->NumMb     = (mfxU16)numMBsInSlice;
            pSliceParam->SliceId   = (mfxU16)i;
            pSliceParam->SliceHeaderSize = 0;
            pSliceParam->SliceDataSize = 0;
            pSliceParam->SliceDataOffset = 0;
            pSliceParam->SliceType = MFX_SLICECHOP_NONE;
            pSliceParam->QScaleCode = 0;

            num += pSliceParam->NumMb;     
            MFX_CHECK((mfxU32)num <= numMBs,MFX_ERR_UNDEFINED_BEHAVIOR);

            for (mfxU32 j=0; j<pSliceParam->NumMb; j++)
            {
                pMB->MPEG2.MbXcnt = (mfxU8)j;
                pMB->MPEG2.MbYcnt = (mfxU8)i;
                pMB ++;
            }
        }
        return MFX_ERR_NONE;
    }



#define MB_MV(pmb,dir,num) (pmb)->MV[(dir)*4+(num)]
#define MB_FSEL(pmb,dir,i) ((mfxMbCodeAVC*)pmb)->RefPicSelect[dir][i] // tmp cast

    mfxStatus EncodeFrameTaskHybrid::SetSkipMBParams()
    {
        mfxSliceParam*  pSlice          = m_pFrameCUC->SliceParam;
        mfxU32          numSlices       = m_pFrameCUC->NumSlice;
        mfxMbCode*      pMB             = m_pFrameCUC->MbParam->Mb;
        mfxU32          numMBs          = m_pFrameCUC->NumMb;
        mfxU32          num             = 0;
        mfxU32          type            = m_pFrameCUC->FrameParam->MPEG2.BackwardPredFlag ? 0x02: 0x01;

        if (m_pFrameCUC->FrameParam->MPEG2.IntraPicFlag)
        {
            return MFX_ERR_UNSUPPORTED;    
        }

        for (mfxU32 i = 0; i < numSlices;i++)
        {
            mfxSliceParamMPEG2 *pSliceParam = &pSlice[i].MPEG2;
            num += pSliceParam->NumMb;     
            MFX_CHECK((mfxU32)num <= numMBs,MFX_ERR_UNDEFINED_BEHAVIOR);

            for (mfxU32 j=0; j<pSliceParam->NumMb; j++)
            {
                mfxMbCodeMPEG2 *pMFXMB = &pMB->MPEG2;
                pMFXMB->CodedPattern4x4Y = 0;
                pMFXMB->CodedPattern4x4U = 0;
                pMFXMB->CodedPattern4x4V = 0;
                pMFXMB->IntraMbFlag = 0;
                pMFXMB->FirstMbFlag = (j == 0)? 1:0;
                pMFXMB->LastMbFlag  = (j + 1 == pSliceParam->NumMb)? 1:0;

                (MB_MV(pMFXMB,0,0))[0] = (MB_MV(pMFXMB,0,0))[1] =
                    (MB_MV(pMFXMB,0,1))[0] = (MB_MV(pMFXMB,0,1))[1] =
                    (MB_MV(pMFXMB,1,0))[0] = (MB_MV(pMFXMB,1,0))[1] = 
                    (MB_MV(pMFXMB,1,1))[0] = (MB_MV(pMFXMB,1,1))[1] = 0;

                MB_FSEL(pMFXMB,0,0) = 0;
                MB_FSEL(pMFXMB,0,1) = 0;
                MB_FSEL(pMFXMB,1,0) = 0;
                MB_FSEL(pMFXMB,1,1) = 0; 

                pMFXMB->MbType5Bits   = type;
                pMFXMB->NumPackedMv   = 1;
                pMFXMB->Skip8x8Flag = 1;
                pMFXMB->FieldMbFlag = 0;

                pMB ++;

            }
        }
        return MFX_ERR_NONE;

    }
    bool EncodeFrameTaskHybrid::isNextField ()
    {
        if (m_pFrameCUC->FrameParam->MPEG2.FieldPicFlag)
        {
            m_pFrameCUC->FrameParam->MPEG2.BottomFieldFlag = !m_pFrameCUC->FrameParam->MPEG2.BottomFieldFlag;
            m_pFrameCUC->FrameParam->MPEG2.SecondFieldFlag = 1;
            m_pFrameCUC->FrameParam->MPEG2.PicStructure = (m_pFrameCUC->FrameParam->MPEG2.BottomFieldFlag)?MFX_MPEG2_BOTTOM_FIELD : MFX_MPEG2_TOP_FIELD;

            /*  second field of intra frames is coded as P */
            /* Unreachable code is commented out
            if (m_pFrameCUC->FrameParam->MPEG2.IntraPicFlag && 0)
            {
                m_pFrameCUC->FrameParam->MPEG2.FrameType &= ~MFX_FRAMETYPE_I;
                m_pFrameCUC->FrameParam->MPEG2.FrameType |= MFX_FRAMETYPE_P;
                m_pFrameCUC->FrameParam->MPEG2.IntraPicFlag = 0;   
            }*/
            return true;

        }
        return false;    
    }
    mfxStatus EncodeFrameTaskHybrid::CopyReference()
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (m_bCopyReference && m_pFrameCUC->FrameParam->MPEG2.RefPicFlag && m_FrameEnc.m_pRefFrame[1] && m_FramePak.m_pRefFrame[1] )
        {
            sts = m_pCore->DoFastCopyWrapper(m_FrameEnc.m_pRefFrame[1], m_refEncType, m_FramePak.m_pRefFrame[1], m_refPakType);
            MFX_CHECK_STS(sts);        
        }

        return sts;
    }


    mfxStatus FrameStoreHybrid::Init(bool bHWEnc, 
        bool    bHWPak, 
        bool    bRawFrame, 
        mfxU16    InputFrameType, 
        mfxU32    nTasks, 
        mfxFrameInfo* pFrameInfo)
    {
        mfxStatus sts = MFX_ERR_NONE;

        m_bEncHW = bHWEnc;
        m_bPakHW = bHWPak;
        

        if (m_pFrameStoreEnc)
        {
            sts = m_pFrameStoreEnc->Reset(bRawFrame,InputFrameType,bHWEnc,nTasks,pFrameInfo);
            MFX_CHECK_STS (sts);        
        }
        else
        {
            m_pFrameStoreEnc = new FrameStore(m_pCore);
            sts = m_pFrameStoreEnc->Init(bRawFrame,InputFrameType,bHWEnc,nTasks,pFrameInfo);
            MFX_CHECK_STS (sts);        
        }
        if (bHWEnc != bHWPak)
        {
            if (m_pFrameStorePak)
            {
                sts = m_pFrameStorePak->Reset(false,InputFrameType,bHWPak,nTasks,pFrameInfo);
                MFX_CHECK_STS (sts);        
            }
            else
            {
                m_pFrameStorePak = new FrameStore(m_pCore);
                sts = m_pFrameStorePak->Init(false,InputFrameType,bHWPak,nTasks,pFrameInfo);
                MFX_CHECK_STS (sts);        
            }        
        }
        return sts;
    }
    mfxStatus FrameStoreHybrid::NextFrame(mfxFrameSurface1 *pInputFrame, 
        mfxU32 nFrame, 
        bool bReference, 
        bool bIntra, 
        FramesSet *pEncFrames,
        FramesSet *pPakFrames)
    {
        mfxStatus sts = MFX_ERR_NONE;
        if (!m_pFrameStoreEnc)
            return MFX_ERR_NOT_INITIALIZED;

        sts = m_pFrameStoreEnc->NextFrame(pInputFrame,nFrame,bReference,bIntra,pEncFrames);
        MFX_CHECK_STS (sts);

        if (m_pFrameStorePak)
        {
            sts = m_pFrameStorePak->NextFrame(pInputFrame,nFrame,bReference,bIntra,pPakFrames);
            MFX_CHECK_STS (sts);
        }
        else
        {
            *pPakFrames = *pEncFrames;
            sts = pPakFrames->LockFrames(m_pCore);
            MFX_CHECK_STS (sts);
        } 
        return sts;
    }
    mfxStatus FrameStoreHybrid::Close()
    {
        mfxStatus sts = MFX_ERR_NONE;
        m_bEncHW = 0;
        m_bPakHW = 0;
        if (m_pFrameStoreEnc)
        {
            sts = m_pFrameStoreEnc->Close();
            MFX_CHECK_STS (sts);
            delete m_pFrameStoreEnc;
            m_pFrameStoreEnc = 0;        
        }
        if (m_pFrameStorePak)
        {
            sts = m_pFrameStorePak->Close();
            MFX_CHECK_STS (sts);
            delete m_pFrameStorePak;
            m_pFrameStorePak = 0;        
        }
        return sts;         
    }
    mfxFrameAllocResponse* FrameStoreHybrid::GetFrameAllocResponse(bool bHW)
    {
        if (m_bEncHW == bHW)
        {
            if (!m_pFrameStoreEnc)
                return 0;
            return m_pFrameStoreEnc->GetFrameAllocResponse();
        }
        else if (m_bPakHW == bHW)
        {
            if (!m_pFrameStorePak)
                return 0;
            return m_pFrameStorePak->GetFrameAllocResponse();            
        }
        else
        {
            return 0;            
        }
    }


}





#endif // MFX_ENABLE_MPEG2_VIDEO_ENCODE
