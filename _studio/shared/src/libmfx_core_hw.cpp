/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2013 Intel Corporation. All Rights Reserved.

File Name: libmf_core_hw.cpp

\* ****************************************************************************** */

#include "mfx_common.h"
#if defined (MFX_VA_WIN)

#include "libmfx_core_hw.h"
#include "umc_va_dxva2.h"
#include "mfx_common_decode_int.h"
#include "mfx_enc_common.h"

using namespace UMC;

VideoAccelerationHW ConvertMFXToUMCType(eMFXHWType type)
{
    VideoAccelerationHW umcType = VA_HW_UNKNOWN;

    switch(type)
    {
    case MFX_HW_LAKE:
        umcType = VA_HW_LAKE;
        break;

    case MFX_HW_LRB:
        umcType = VA_HW_LRB;
        break;
    case MFX_HW_SNB:
        umcType = VA_HW_SNB;
        break;
    case MFX_HW_IVB:
        umcType = VA_HW_IVB;
        break;

    case MFX_HW_HSW:
        umcType = VA_HW_HSW;
        break;
    case MFX_HW_HSW_ULT:
        umcType = VA_HW_HSW_ULT;
        break;

    case MFX_HW_BDW:
        umcType = VA_HW_BDW;
        break;

    case MFX_HW_SCL:
        umcType = VA_HW_SCL;
        break;

    case MFX_HW_VLV:
        umcType = VA_HW_VLV;
        break;
    }

    return umcType;
}

mfxU32 ChooseProfile(mfxVideoParam * param, eMFXHWType hwType)
{
    mfxU32 profile = UMC::VA_VLD;

    // video accelerator is needed for decoders only 
    switch (param->mfx.CodecId)
    {
    case MFX_CODEC_VC1:
        profile |= VA_VC1;
        break;
    case MFX_CODEC_MPEG2:
        profile |= VA_MPEG2;
        break;
    case MFX_CODEC_AVC:
        {
        profile |= VA_H264;

        mfxU32 profile_idc = ExtractProfile(param->mfx.CodecProfile);

        if (IsMVCProfile(profile_idc))
        {
            if (hwType == MFX_HW_HSW || hwType == MFX_HW_HSW_ULT || hwType == MFX_HW_BDW || hwType == MFX_HW_SCL)
            {
#if 0 // Intel  MVC GUID
                profile |= H264_VLD_MVC;
                if (param->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
                    profile |= VA_SHORT_SLICE_MODE;
#else // or MS MVC GUID
                mfxExtMVCSeqDesc * points = (mfxExtMVCSeqDesc *)GetExtendedBuffer(param->ExtParam, param->NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

                if (profile_idc == MFX_PROFILE_AVC_MULTIVIEW_HIGH && points && points->NumView > 2)
                    profile |= VA_PROFILE_MVC_MV;
                else
                    profile |= param->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? VA_PROFILE_MVC_STEREO_PROG : VA_PROFILE_MVC_STEREO;

                //if (param->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE)
                //    profile |= VA_SHORT_SLICE_MODE;
#endif
            }
            else
            {
                profile |= VA_LONG_SLICE_MODE;
            }
        }

        if (IsSVCProfile(profile_idc))
        {
            profile |= (profile_idc == MFX_PROFILE_AVC_SCALABLE_HIGH) ? VA_PROFILE_SVC_HIGH : VA_PROFILE_SVC_BASELINE;
        }
        }
        break;
    case MFX_CODEC_JPEG:
        profile |= VA_JPEG;
        break;
    case MFX_CODEC_VP8:
        profile |= VA_VP8;
        break;
    case MFX_CODEC_HEVC:
        profile |= VA_H265;
        break;


    default:
        return 0;
    }

    return profile;
}

bool IsHwMvcEncSupported()
{
    return true;
}

#endif 
/* EOF */
