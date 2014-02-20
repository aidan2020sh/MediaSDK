/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
//          VP8 encoder common
//
*/

#include "mfx_common.h"
#if defined(MFX_ENABLE_VP8_VIDEO_ENCODE_HW) 

#include "mfxvp8.h"
#include "mfx_enc_common.h"
#include "mfx_vp8_enc_common_hw.h"
#include "vm_sys_info.h"
#include <math.h>
#include <memory.h>
#include "mfx_common_int.h"
#include "ipps.h"
#include "umc_defs.h"

//#define NO_THREADING

namespace MFX_VP8ENC
{
mfxStatus CheckExtendedBuffers (mfxVideoParam* par)
{
#define NUM_SUPPORTED_BUFFERS 4
    mfxU32 supported_buffers[NUM_SUPPORTED_BUFFERS] = {
            //MFX_EXTBUFF_CODING_OPTION_SPSPPS,
            MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
            MFX_EXTBUFF_VP8_EX_CODING_OPT,
            MFX_EXTBUFF_VP8_PARAM,
            MFX_EXTBUFF_ENCODER_ROI
    };
    mfxU32 num_supported = 0;
    if (par->NumExtParam == 0 || par->ExtParam == 0)
    {
        return MFX_ERR_NONE;
    }
    for (mfxU32 n_buf=0; n_buf < NUM_SUPPORTED_BUFFERS; n_buf++)
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

#undef NUM_SUPPORTED_BUFFERS
}
/*temp function, while BRC is not ready*/
#define RANGE_FOR_MULTIPLIER 0x10000
static mfxU16 GetDefaultBitrate(mfxU16 w, mfxU16 h, double frame_rate,bool bMin, mfxU16& multiplier )
{
    const mfxU16 w_base = 1920;
    const mfxU16 h_base = 1088;
    const double fr_base = 30;
    const mfxU16  bitrate_base_avg = 6000;
    const mfxU16  bitrate_base_min = 1000;
    
    mfxU16  bitrate_base = bMin ? bitrate_base_min : bitrate_base_avg;

    multiplier = 1;

    if (w==0 || h==0)
        return 0;

    double k = (double)w_base*(double)h_base/((double)w*(double)h);
    k = sqrt(k);
    k = (double)bitrate_base/fr_base*frame_rate/k;
    
    multiplier = (mfxU16)((k + RANGE_FOR_MULTIPLIER)/RANGE_FOR_MULTIPLIER);

    return (mfxU16)(k/multiplier);
}
/*temp function, while BRC is not ready*/
static mfxU16 GetDefaultBufferSize(double frame_rate, mfxU16 bitrate, mfxU16& multiplier)
{
    static double numFrames = 10.0;

    multiplier = 1;

    if (frame_rate == 0.0)
        return 0;
    
    double bufSize = ((double)bitrate/frame_rate*numFrames/8.0);
    
    multiplier = (mfxU16)((bufSize + RANGE_FOR_MULTIPLIER)/RANGE_FOR_MULTIPLIER);

    return (mfxU16)(bufSize/multiplier);
}
/*function for const quantization*/
static mfxU16 GetDefaultBufferSize(mfxU16 w, mfxU16 h, mfxU16& multiplier)
{
    // size of uncompressed frame (YUV 4:2:0) in KB
    mfxU32 bufSize = (((w*h*3) >> 1) + 1000 - 1)/1000;

    multiplier = (mfxU16)((bufSize + RANGE_FOR_MULTIPLIER)/RANGE_FOR_MULTIPLIER);

    return (mfxU16)(bufSize/multiplier);
}
#undef RANGE_FOR_MULTIPLIER
static void SetSupportedMFXParameters(mfxInfoMFX*  par)
{
    memset(par,0,sizeof(mfxInfoMFX));

    par->BRCParamMultiplier = 1;
    par->FrameInfo.Width= 1;
    par->FrameInfo.Height= 1;
    par->FrameInfo.CropW= 1;
    par->FrameInfo.CropH= 1;
    par->FrameInfo.FrameRateExtN= 1;
    par->FrameInfo.FrameRateExtD= 1;
    par->FrameInfo.AspectRatioW= 1;
    par->FrameInfo.AspectRatioH= 1;
    par->FrameInfo.PicStruct= 1;
    par->FrameInfo.ChromaFormat= 1;
    par->CodecId =1;
    par->CodecProfile =1;
    par->NumThread =1;
    par->TargetUsage =1;
    par->GopPicSize =1;
    par->GopOptFlag =1;
    par->RateControlMethod =1;
    par->InitialDelayInKB =1;
    par->BufferSizeInKB =1;
    par->TargetKbps=1;
    par->MaxKbps=1;
}

static mfxStatus CheckMFXParameters(mfxInfoMFX*  par)
{
    bool        bChanged     = false;
    bool        bUnsupported = false;
    double      frameRate = 0.0;

    if ((par->FrameInfo.Width & 0x0f) != 0 || (par->FrameInfo.Height&0x0f) !=0)
    {
        par->FrameInfo.Width  = 0;
        par->FrameInfo.Height = 0;
        bUnsupported = true;
    }
    if (par->FrameInfo.CropW > par->FrameInfo.Width ||
        par->FrameInfo.CropH > par->FrameInfo.Height)
    {
        par->FrameInfo.CropW = 0;
        par->FrameInfo.CropH = 0; 
        bChanged = true;
    }
    if (par->FrameInfo.FrameRateExtD != 0)
    {
        frameRate = (double)par->FrameInfo.FrameRateExtN/(double)par->FrameInfo.FrameRateExtD;
    }
    if (frameRate < 0.1 || frameRate > 180.0)
    {
        par->FrameInfo.FrameRateExtN = 0;
        par->FrameInfo.FrameRateExtD = 0;
        bChanged = true;
    }
    if (par->FrameInfo.AspectRatioH!=0 || par->FrameInfo.AspectRatioW!=0)
    {
        if (!(par->FrameInfo.AspectRatioH==1 && par->FrameInfo.AspectRatioW==1))
        {
            par->FrameInfo.AspectRatioH = 0;
            par->FrameInfo.AspectRatioW = 0; 
            bChanged = true;
        }   
    }
    if (par->FrameInfo.PicStruct > MFX_PICSTRUCT_PROGRESSIVE)
    {
        par->FrameInfo.PicStruct = 0;
        bChanged = true;   
    }
    if (par->FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV420)
    {
        par->FrameInfo.ChromaFormat = 0;
        bChanged = true;
    }
    if (par->FrameInfo.CropX != 0 || par->FrameInfo.CropY != 0  )
    {
        par->FrameInfo.CropX = 0;
        par->FrameInfo.CropY = 0;
        bChanged = true;    
    }
    if (par->CodecProfile > MFX_PROFILE_VP8_3)
    {
        par->CodecProfile = MFX_PROFILE_UNKNOWN; 
        bChanged = true;
    }
    if (par->CodecLevel!=MFX_LEVEL_UNKNOWN)
    {
        par->CodecLevel = MFX_LEVEL_UNKNOWN;
        bChanged = true;    
    }
#ifdef NO_THREADING
    if (par->NumThread>1)  
    {
        par->NumThread = 0;
        bChanged = true; 
    }
#else
    if (par->NumThread>8)  
    {
        par->NumThread = 0;
        bChanged = true; 
    }
#endif
    if (par->TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)
    {
        par->TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        bChanged = true;    
    }
    if (par->GopRefDist)
    {
        par->GopRefDist = 0;
        bChanged = true;    
    }
    if (par->GopOptFlag > (MFX_GOP_STRICT|MFX_GOP_CLOSED))
    {
        par->GopOptFlag = 0;
        bChanged = true;
    }
    if (par->IdrInterval!=0)
    {
        par->IdrInterval = 0;
        bChanged = true;    
    }
    if (par->RateControlMethod > MFX_RATECONTROL_AVBR)
    {
        par->RateControlMethod = 0;
        par->InitialDelayInKB = 0;
        par->BufferSizeInKB = 0;
        par->TargetKbps = 0;
        par->MaxKbps = 0;
        par->BRCParamMultiplier = 0;
        bChanged = true;   
    }
    else if (par->RateControlMethod == MFX_RATECONTROL_CBR ||
             par->RateControlMethod == MFX_RATECONTROL_VBR ||
             par->RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        /*mfxU16 minBitrateMultiplier = 1;
        mfxU16 minBitrate  = GetDefaultBitrate(par->FrameInfo.Width,par->FrameInfo.Height,
                                              frameRate, true, minBitrateMultiplier);

        mfxU16 multiplier = par->BRCParamMultiplier == 0 ? 1 : par->BRCParamMultiplier;
        if ((mfxU32)par->TargetKbps*multiplier < (mfxU32)minBitrate*minBitrateMultiplier)
        {
            par->TargetKbps = 0;
            par->BRCParamMultiplier = 0;
            bUnsupported = true;
        }*/
        if (par->RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            if ((par->RateControlMethod == MFX_RATECONTROL_CBR  && par->MaxKbps != par->TargetKbps)||
                (par->RateControlMethod == MFX_RATECONTROL_VBR  && par->MaxKbps < par->TargetKbps))
            {
                par->MaxKbps = 0;
                bChanged = true;
            }            
            if (par->InitialDelayInKB > par->BufferSizeInKB)
            {
                par->InitialDelayInKB = 0;
                bChanged = true;        
            } 
        }
    }
    else if (par->RateControlMethod == MFX_RATECONTROL_CQP)
    {
        if (par->QPI > 127)
        {
            par->QPI = 127;
            bChanged = true;        
        }
        if (par->QPP > 127)
        {
            par->QPP = 127;
            bChanged = true;        
        }
        if (par->QPB > 0)
        {
            par->QPB = 0;
            bChanged = true;
        }        
    }
    else
    {
        bUnsupported = true;  
    }
    
    if (par->NumSlice != 0)
    {
        par->NumSlice = 0;
        bChanged = true;    
    }
    if (par->NumRefFrame != 0)
    {
        par->NumRefFrame = 0;
        bChanged = true;    
    }
    if (bUnsupported)
        return MFX_ERR_UNSUPPORTED;

    return (bChanged)? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: MFX_ERR_NONE;
}
static void SetSupportedExCodingParameters(mfxExtCodingOptionVP8*  par)
{
    par->EnableAutoAltRef   = 1;        
    par->TokenPartitions    = 1;        
    par->EnableMultipleSegments = 1; 
    memset(par->reserved,0,sizeof(par->reserved));
    //memset(par->reserved1,0,sizeof(par->reserved1));

}
static mfxStatus CheckExCodingParameters(mfxExtCodingOptionVP8*  par)
{
    mfxStatus   sts      = MFX_ERR_NONE;
    bool        bChanged = false;

    if (par->EnableAutoAltRef > MFX_CODINGOPTION_ADAPTIVE)
    {
        par->EnableAutoAltRef = 0;
        bChanged = true;    
    }
    /*if (par->EnableAutoGoldRef > MFX_CODINGOPTION_ADAPTIVE)
    {
        par->EnableAutoGoldRef = 0;
        bChanged = true;    
    }*/
    if (par->TokenPartitions > MFX_TOKENPART_VP8_8)
    {
        par->TokenPartitions = 0;
        bChanged = true;    
    }
    if (par->EnableMultipleSegments > MFX_CODINGOPTION_ADAPTIVE)
    {
        par->EnableMultipleSegments = MFX_CODINGOPTION_ADAPTIVE;
        bChanged = true;     
    }
    return (bChanged)? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: sts;
}

static void SetDefaultMFXParameters(mfxInfoMFX*  par)
{
    double frameRate = 0.0;

    if (par->BRCParamMultiplier == 0)
    {
        par->BRCParamMultiplier = 1;
    }
    if (par->FrameInfo.FrameRateExtN ==0 && par->FrameInfo.FrameRateExtD == 0)
    {
        par->FrameInfo.FrameRateExtN = 30;
        par->FrameInfo.FrameRateExtD = 1;
    }
    frameRate = par->FrameInfo.FrameRateExtD!=0 ?
        (double)par->FrameInfo.FrameRateExtN/(double)par->FrameInfo.FrameRateExtD:0;

    if (par->FrameInfo.AspectRatioH == 0 && par->FrameInfo.AspectRatioW == 0)
    {
        par->FrameInfo.AspectRatioH = 1;
        par->FrameInfo.AspectRatioW = 1; 
    }
    if (par->FrameInfo.PicStruct == 0)
    {
        par->FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }
    if (par->FrameInfo.ChromaFormat == 0)
    {
        par->FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }
    if (par->CodecProfile == 0)
    {
        par->CodecProfile = MFX_PROFILE_VP8_0; 
    }

#ifdef NO_THREADING
    if (par->NumThread == 0)
    {
        par->NumThread = 1; 
    }
#else
    if (par->NumThread == 0)
    {
        par->NumThread = (mfxU16)vm_sys_info_get_cpu_num(); 
    }
#endif

    if (par->TargetUsage == 0)
    {
        par->TargetUsage = MFX_TARGETUSAGE_BALANCED;
    }
    if (par->GopPicSize == 0)
    {
        par->GopPicSize = 0xFFFF; // max distance between Intra frames
    }
    /*if (par->GopRefDist == 0)
    {
        par->GopRefDist = par->GopPicSize; //max distance between Alt ref frames (if available)
    }*/
    if (par->GopOptFlag == 0)
    {
        par->GopOptFlag = MFX_GOP_CLOSED;
    }
    /*BRC parameters*/
    {
        if (par->RateControlMethod == 0)
        {
            par->RateControlMethod = MFX_RATECONTROL_VBR;
        }
        if (par->RateControlMethod == MFX_RATECONTROL_CBR ||
            par->RateControlMethod == MFX_RATECONTROL_VBR)
        {
            mfxU16 targetMultiplier     = par->BRCParamMultiplier;
            mfxU16 bufferMultiplier     = par->BRCParamMultiplier;

            if (par->TargetKbps == 0)
            {
                par->TargetKbps = GetDefaultBitrate(par->FrameInfo.Width,par->FrameInfo.Height,
                                                        frameRate, false, targetMultiplier); 
            }
            if (par->BufferSizeInKB == 0)
            {
                par->BufferSizeInKB = GetDefaultBufferSize(frameRate,par->TargetKbps,bufferMultiplier);
            }
            if (targetMultiplier != bufferMultiplier || targetMultiplier != par->BRCParamMultiplier)
            {
                mfxU16 multiplier       = IPP_MAX(targetMultiplier,IPP_MAX(par->BRCParamMultiplier, bufferMultiplier));
                par->TargetKbps     = (mfxU16)(((mfxU32)par->TargetKbps*targetMultiplier + multiplier - 1)/multiplier);
                par->BufferSizeInKB = (mfxU16)(((mfxU32)par->BufferSizeInKB*bufferMultiplier + multiplier - 1)/multiplier);
                par->InitialDelayInKB=(mfxU16)(((mfxU32)par->BufferSizeInKB*par->BRCParamMultiplier + multiplier - 1)/multiplier);
                par->MaxKbps = (mfxU16)(((mfxU32)par->MaxKbps*par->BRCParamMultiplier + multiplier - 1)/multiplier);
                par->BRCParamMultiplier = multiplier;
            }
            if (par->InitialDelayInKB == 0)
            {
                par->InitialDelayInKB = par->BufferSizeInKB>>1;        
            }
            if (par->MaxKbps == 0 && par->RateControlMethod == MFX_RATECONTROL_VBR)
            {
                par->MaxKbps =  (par->TargetKbps < 0x7fff) ? par->TargetKbps<<1 : 0xFFFF;
            }
        } 
        if (par->RateControlMethod == MFX_RATECONTROL_AVBR)
        {
            mfxU16 targetMultiplier     = par->BRCParamMultiplier;
            mfxU16 bufferMultiplier     = par->BRCParamMultiplier;

            if (par->TargetKbps == 0)
            {
                par->TargetKbps = GetDefaultBitrate(par->FrameInfo.Width,par->FrameInfo.Height,
                                                        frameRate, false, targetMultiplier); 
            }
            if (par->BufferSizeInKB == 0)
            {
                par->BufferSizeInKB = GetDefaultBufferSize(par->FrameInfo.Width,
                                                           par->FrameInfo.Height,
                                                           bufferMultiplier);
            }
            if (targetMultiplier != bufferMultiplier)
            {
                mfxU16 multiplier       = IPP_MAX(targetMultiplier,bufferMultiplier);
                par->TargetKbps     = (mfxU16)(((mfxU32)par->TargetKbps*targetMultiplier + multiplier - 1)/multiplier);
                par->BufferSizeInKB = (mfxU16)(((mfxU32)par->BufferSizeInKB*bufferMultiplier + multiplier - 1)/multiplier);
                par->BRCParamMultiplier = multiplier;
            }
            if (par->Accuracy == 0)
            {
                par->Accuracy = 0x00FF;
            }
            if (par->Quality == 0)
            {
                par->Quality  =  0x00FF;
            }
        }
        if (par->RateControlMethod == MFX_RATECONTROL_CQP)
        {
            if (par->QPI == 0)
            {
                par->QPI = 10;        
            }
            if (par->QPP == 0)
            {
                par->QPP = par->QPI;
            }
            if (par->BufferSizeInKB == 0)
            {
                par->BufferSizeInKB = GetDefaultBufferSize(par->FrameInfo.Width,
                                                           par->FrameInfo.Height,
                                                           par->BRCParamMultiplier);
            }
        }
    }
}
static void SetDefaultExCodingParameters(mfxExtCodingOptionVP8*  par, mfxExtCodingOptionVP8Param* parPlus, mfxExtEncoderROI * roi, mfxU16 tu, mfxU16 /*numTreads*/)
{
    bool            bChanged = false;
    static mfxU16   tokenPart[9] = {MFX_TOKENPART_VP8_1,MFX_TOKENPART_VP8_1,
                                    MFX_TOKENPART_VP8_2,MFX_TOKENPART_VP8_2,
                                    MFX_TOKENPART_VP8_4,MFX_TOKENPART_VP8_4,
                                    MFX_TOKENPART_VP8_4,MFX_TOKENPART_VP8_4,
                                    MFX_TOKENPART_VP8_8};
  
    if (par->EnableAutoAltRef == 0)
    {
        par->EnableAutoAltRef = (mfxU16)((tu < MFX_TARGETUSAGE_BALANCED)? MFX_CODINGOPTION_ON:MFX_CODINGOPTION_OFF);
    }
    /*if (par->EnableAutoGoldRef == 0)
    {
        par->EnableAutoGoldRef =(mfxU16)((tu < MFX_TARGETUSAGE_BALANCED)? MFX_CODINGOPTION_ON:MFX_CODINGOPTION_OFF);
    }*/
    if (par->TokenPartitions == 0)
    {
        par->TokenPartitions = 1; //tokenPart[numTreads > 8 ? 8 : numTreads]; 
    }
    if (par->EnableMultipleSegments == 0)
    {
        if (roi && roi->NumROI)
            par->EnableMultipleSegments = MFX_CODINGOPTION_ON;
        else
            par->EnableMultipleSegments = MFX_CODINGOPTION_OFF;
        bChanged = true;     
    }

    if (parPlus)
    {
        if (parPlus->NumPartitions == 0)
            parPlus->NumPartitions = 1;
    }
}

 mfxStatus SetSupportedParameters (mfxVideoParam* par)
 {
     mfxExtCodingOptionVP8*     pExtOpt = 0;
     mfxExtOpaqueSurfaceAlloc * pOpaqAlloc = 0;

     MFX_CHECK(CheckExtendedBuffers(par) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);  

     SetSupportedMFXParameters(&par->mfx);

     par->AsyncDepth = 1;
     par->IOPattern  = 1;
     par->Protected  = 0;

     memset(par->reserved, 0, sizeof(par->reserved));
     memset(&par->reserved2,0, sizeof(par->reserved2));
     memset(&par->reserved3,0, sizeof(par->reserved3));

     pOpaqAlloc =(mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(  par->ExtParam, 
                                                                 par->NumExtParam, 
                                                                 MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
     if (pOpaqAlloc)
     {
         pOpaqAlloc->In.Type = 1;    
     }

     pExtOpt =(mfxExtCodingOptionVP8*)GetExtBuffer(  par->ExtParam, 
                                                     par->NumExtParam,
                                                     MFX_EXTBUFF_VP8_EX_CODING_OPT);
     if (pExtOpt)
     {
         SetSupportedExCodingParameters(pExtOpt);    
     }
     return MFX_ERR_NONE;
 }
 mfxStatus CheckParameters(mfxVideoParam*   parSrc,
                           mfxVideoParam*   parDst)

 {         
     mfxStatus sts = MFX_ERR_NONE;

     mfxExtCodingOptionVP8      *pExDst     = 0, *pExSrc     = 0;
     mfxExtOpaqueSurfaceAlloc   *pOpaqDst   = 0, *pOpaqSrc   = 0;
     mfxExtEncoderROI           *pExtRoiDst = 0, *pExtRoiSrc = 0;

     MFX_CHECK_NULL_PTR2(parSrc,parDst);
     MFX_CHECK(CheckExtendedBuffers(parSrc) == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);
     MFX_CHECK(CheckExtendedBuffers(parDst) == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);  

     MFX_CHECK(parSrc->Protected == 0,MFX_ERR_UNSUPPORTED);

    parDst->AsyncDepth = parSrc->AsyncDepth;
    parDst->IOPattern      = parSrc->IOPattern;
    parDst->Protected      = parSrc->Protected;

    /*      Check IO pattern        */
    {
        mfxU32 ioPattern = parDst->IOPattern & ( MFX_IOPATTERN_IN_VIDEO_MEMORY|
                                                 MFX_IOPATTERN_IN_SYSTEM_MEMORY|
                                                 MFX_IOPATTERN_IN_OPAQUE_MEMORY);
        if (ioPattern & (ioPattern - 1))
            return MFX_ERR_UNSUPPORTED;

        pOpaqSrc =(mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(
                                              parSrc->ExtParam, 
                                              parSrc->NumExtParam, 
                                              MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        pOpaqDst =(mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(
                                              parDst->ExtParam, 
                                              parDst->NumExtParam, 
                                              MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (ioPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {

            if (!pOpaqSrc || !pOpaqDst)
                return MFX_ERR_INVALID_VIDEO_PARAM;
           
            if (!(pOpaqSrc->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && !(pOpaqSrc->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            if ((pOpaqSrc->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (pOpaqSrc->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
                return MFX_ERR_INVALID_VIDEO_PARAM;

            MFX_INTERNAL_CPY(pOpaqDst,pOpaqSrc,sizeof(mfxExtOpaqueSurfaceAlloc));           
        }
        else if (pOpaqDst)
        {
            mfxExtBuffer header = pOpaqDst->Header;
            memset(pOpaqDst,0,sizeof(mfxExtOpaqueSurfaceAlloc));
            pOpaqDst->Header = header;        
        }
    }
    /*Check MFX Parameters*/
    {
        mfxStatus temp_sts = MFX_ERR_NONE;
        MFX_INTERNAL_CPY(&parDst->mfx,&parSrc->mfx, sizeof(mfxInfoMFX));
        temp_sts = CheckMFXParameters(&parDst->mfx);
        if (temp_sts < 0)
            return temp_sts;
        else if (temp_sts > 0)
            sts = temp_sts;
    }
    /*Check ext params for VP8 */
    {
        mfxStatus temp_sts = MFX_ERR_NONE;
        pExSrc = (mfxExtCodingOptionVP8*)GetExtBuffer(parSrc->ExtParam, 
                                                      parSrc->NumExtParam,
                                                      MFX_EXTBUFF_VP8_EX_CODING_OPT);
        pExDst = (mfxExtCodingOptionVP8*)GetExtBuffer(parDst->ExtParam, 
                                                      parDst->NumExtParam,
                                                      MFX_EXTBUFF_VP8_EX_CODING_OPT);

        if (pExSrc && pExDst)
        {
            MFX_INTERNAL_CPY(pExDst,pExSrc,sizeof(mfxExtCodingOptionVP8));

            temp_sts = CheckExCodingParameters(pExDst);
            if (temp_sts < 0)
                return temp_sts;
            else if (temp_sts > 0)
                sts = temp_sts;
        }
        else if(pExDst)
        {
            mfxExtBuffer header = pExDst->Header;
            memset(pExDst,0,sizeof(mfxExtOpaqueSurfaceAlloc));
            pExDst->Header = header;        
        }
    }

    // Check ROI for VP8
    {
        pExtRoiSrc = (mfxExtEncoderROI*)GetExtBuffer(parSrc->ExtParam, 
                                                      parSrc->NumExtParam,
                                                      MFX_EXTBUFF_ENCODER_ROI);
        pExtRoiDst = (mfxExtEncoderROI*)GetExtBuffer(parDst->ExtParam, 
                                                      parDst->NumExtParam,
                                                      MFX_EXTBUFF_ENCODER_ROI);

        if (pExtRoiSrc && pExtRoiDst)
        {
            if (pExtRoiSrc->NumROI > 4)
            {
                pExtRoiDst->NumROI = 4;
                MFX_INTERNAL_CPY(pExtRoiDst->ROI, pExtRoiSrc->ROI, &(pExtRoiSrc->ROI[4]) - pExtRoiSrc->ROI);
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            }
            else
                MFX_INTERNAL_CPY(pExtRoiDst, pExtRoiSrc, sizeof(mfxExtEncoderROI));
        }
    }
    return sts > 0 ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM: sts;
 
 
 }
mfxU16 GetDefaultAsyncDepth()
{
    return 1;
}

mfxStatus CheckParametersAndSetDefault(mfxVideoParam*              pParamSrc,
                                       mfxVideoParam*              pParamDst,
                                       mfxExtCodingOptionVP8*      pExCodingVP8Dst,
                                       mfxExtCodingOptionVP8Param* pExCodingVP8ParamDst,
                                       mfxExtOpaqueSurfaceAlloc*   pOpaqAllocDst,
                                       bool                        bExternalFrameAllocator,
                                       bool                        bReset)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxExtCodingOptionVP8*  pExCodingVP8Src = 0;

    MFX_CHECK_NULL_PTR1(pParamSrc);
    MFX_CHECK(CheckExtendedBuffers(pParamSrc) == MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM);  
    MFX_CHECK(pParamSrc->Protected == 0,MFX_ERR_INVALID_VIDEO_PARAM);

    sts = CheckVideoParamEncoders(pParamSrc,bExternalFrameAllocator, MFX_HW_UNKNOWN);
    MFX_CHECK_STS(sts);

    if (pParamSrc->AsyncDepth == 0)
        pParamDst->AsyncDepth = GetDefaultAsyncDepth();
    else
        pParamDst->AsyncDepth = pParamSrc->AsyncDepth;
    pParamDst->IOPattern  = pParamSrc->IOPattern;
    pParamDst->Protected  = pParamSrc->Protected;

    /*      Check IO pattern        */
    {
        mfxU32 ioPattern = pParamDst->IOPattern & ( MFX_IOPATTERN_IN_VIDEO_MEMORY|
                                                    MFX_IOPATTERN_IN_SYSTEM_MEMORY|
                                                    MFX_IOPATTERN_IN_OPAQUE_MEMORY);
        if (ioPattern & (ioPattern - 1))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (ioPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            mfxExtOpaqueSurfaceAlloc * pOpaqAlloc =(mfxExtOpaqueSurfaceAlloc *)GetExtendedBuffer(
                                                                pParamSrc->ExtParam, 
                                                                pParamSrc->NumExtParam, 
                                                                MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
            if (!pOpaqAlloc && !bReset)
                return MFX_ERR_INVALID_VIDEO_PARAM;

            if (!bReset)
            {
                if (!(pOpaqAlloc->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET) && !(pOpaqAlloc->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
                    return MFX_ERR_INVALID_VIDEO_PARAM;

                if ((pOpaqAlloc->In.Type & MFX_MEMTYPE_SYSTEM_MEMORY) && (pOpaqAlloc->In.Type & MFX_MEMTYPE_DXVA2_DECODER_TARGET))
                    return MFX_ERR_INVALID_VIDEO_PARAM;

                MFX_INTERNAL_CPY(pOpaqAllocDst,pOpaqAlloc,sizeof(mfxExtOpaqueSurfaceAlloc));  
            }
            else if (pOpaqAlloc)
            {
                MFX_CHECK(pOpaqAlloc->In.Type == pOpaqAllocDst->In.Type,MFX_ERR_INCOMPATIBLE_VIDEO_PARAM); 
            }
        }
    }
    /*Check MFX Parameters*/
    {
        mfxStatus temp_sts = MFX_ERR_NONE;
        MFX_INTERNAL_CPY(&pParamDst->mfx,&pParamSrc->mfx, sizeof(mfxInfoMFX));
        temp_sts = CheckMFXParameters(&pParamDst->mfx);
        if (temp_sts < 0)
            return temp_sts;
        else if (temp_sts > 0)
            sts = temp_sts;

        SetDefaultMFXParameters(&pParamDst->mfx);
    }
    /*Check ext params for VP8 */
    {
        mfxStatus temp_sts = MFX_ERR_NONE;
        pExCodingVP8Src =(mfxExtCodingOptionVP8*)GetExtBuffer(pParamSrc->ExtParam, 
            pParamSrc->NumExtParam,
            MFX_EXTBUFF_VP8_EX_CODING_OPT);
        if (pExCodingVP8Src)
        {
            MFX_INTERNAL_CPY(pExCodingVP8Dst,pExCodingVP8Src,sizeof(mfxExtCodingOptionVP8));
        }
        temp_sts = CheckExCodingParameters(pExCodingVP8Dst);
        if (temp_sts < 0)
            return temp_sts;
        else if (temp_sts > 0)
            sts = temp_sts;

        mfxExtEncoderROI * pExtRoi = (mfxExtEncoderROI*)GetExtBuffer(pParamSrc->ExtParam,
            pParamSrc->NumExtParam,
            MFX_EXTBUFF_ENCODER_ROI);

        SetDefaultExCodingParameters(pExCodingVP8Dst,pExCodingVP8ParamDst,pExtRoi,pParamDst->mfx.TargetUsage,pParamDst->mfx.NumThread);
    }
    return sts;
}



}
#endif //namespace MFX_VP8ENC