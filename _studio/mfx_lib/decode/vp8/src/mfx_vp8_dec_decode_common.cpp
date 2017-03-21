//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_vp8_dec_decode_common.h"

#include "umc_structures.h"
#include "umc_defs.h"
#include "ippcore.h"
#include "ipps.h"
#include "ippcc.h"
#include "mfx_common.h"
#include "mfx_common_decode_int.h"
#include "mfx_common_int.h"
#include "mfx_enc_common.h"

using namespace UMC;

#define VP8_START_CODE_FOUND(ptr) ((ptr)[0] == 0x9d && (ptr)[1] == 0x01 && (ptr)[2] == 0x2a)

namespace VP8DecodeCommon
{

    mfxStatus DecodeHeader(VideoCORE *, mfxBitstream *p_bs, mfxVideoParam *p_params)
    {
        mfxStatus sts = MFX_ERR_NONE;

        MFX_CHECK_NULL_PTR2(p_bs, p_params);

        sts = CheckBitstream(p_bs);
        MFX_CHECK_STS(sts);
    
        if (p_bs->DataLength < 3)
        {
            return MFX_ERR_MORE_DATA;
        }

        mfxU8 *p_bitstream = p_bs->Data + p_bs->DataOffset;
        mfxU8 *p_bitstream_end = p_bs->Data + p_bs->DataOffset + p_bs->DataLength;

        mfxU32 n_bytes_offset = 0;
        bool start_code_found = false;

        while (p_bitstream < p_bitstream_end)
        {
            if (VP8_START_CODE_FOUND(p_bitstream)) // (0x9D && 0x01 && 0x2A)
            {
                start_code_found = true;
                // move to frame tag
                p_bitstream -= 3;
                n_bytes_offset -= 3;
                break;
            }
    
            p_bitstream += 1;
            n_bytes_offset += 1;
        }

        if (!start_code_found)
        {
            // set offset, but leave last six bytes
            // since they can be start bytes of start code and frame tag
    
            MoveBitstreamData(*p_bs, n_bytes_offset - 6);

            return MFX_ERR_MORE_DATA;
        }

        FrameType frame_type;
        mfxU16 width, height;
    
        frame_type = (p_bitstream[0] & 1) ? P_PICTURE : I_PICTURE; // 1 bits

        p_params->mfx.CodecProfile = ((p_bitstream[0] >> 1) & 0x7) + 1;

        if (frame_type != I_PICTURE)
        {
            return MFX_ERR_MORE_DATA;
        }

        mfxU32 first_partion_size = (p_bitstream[0] >> 5) | // 19 bit
                                    (p_bitstream[1] << 3) |
                                    (p_bitstream[2] << 11);
    

        if (p_bitstream + first_partion_size > p_bitstream_end)
        {
            return MFX_ERR_MORE_DATA;
        }

        // move to start code
        p_bitstream += 3;
    
        width = ((p_bitstream[4] << 8) | p_bitstream[3]) & 0x3fff;
        height = ((p_bitstream[6] << 8) | p_bitstream[5]) & 0x3fff;
    
        p_params->mfx.FrameInfo.AspectRatioW = 1;
        p_params->mfx.FrameInfo.AspectRatioH = 1;
    
        p_params->mfx.FrameInfo.CropW = width;
        p_params->mfx.FrameInfo.CropH = height;

        p_params->mfx.FrameInfo.Width = (p_params->mfx.FrameInfo.CropW + 15) & ~0x0f;
        p_params->mfx.FrameInfo.Height = (p_params->mfx.FrameInfo.CropH + 15) & ~0x0f;

        p_params->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        p_params->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        p_params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        MoveBitstreamData(*p_bs, n_bytes_offset);

        return MFX_ERR_NONE;

    }

    void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset)
    {
        VM_ASSERT(offset <= bs.DataLength);
        bs.DataOffset += offset;
        bs.DataLength -= offset;
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MFX_VP8_Utility implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool MFX_VP8_Utility::IsNeedPartialAcceleration(mfxVideoParam *p_params)
{
    if (!p_params)
        return false;

    if (p_params->mfx.FrameInfo.Width > 4096 || p_params->mfx.FrameInfo.Height > 4096)
        return true;

    return false;

} // bool MFX_VP8_Utility::IsNeedPartialAcceleration(mfxVideoParam *p_params)

eMFXPlatform MFX_VP8_Utility::GetPlatform(VideoCORE *p_core, mfxVideoParam *p_params)
{
    eMFXPlatform platform = p_core->GetPlatformType();

    if (p_params && IsNeedPartialAcceleration(p_params) && platform != MFX_PLATFORM_SOFTWARE)
    {
        return MFX_PLATFORM_SOFTWARE;
    }

    return platform;

} // eMFXPlatform MFX_VP8_Utility::GetPlatform(VideoCORE *p_core, mfxVideoParam *p_params)

mfxStatus MFX_VP8_Utility::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out, eMFXHWType type)
{
    MFX_CHECK_NULL_PTR1(p_out);
    mfxStatus  sts = MFX_ERR_NONE;

    if (p_in == p_out)
    {
        mfxVideoParam in1;
        MFX_INTERNAL_CPY(&in1, p_in, sizeof(mfxVideoParam));
        return Query(p_core, &in1, p_out, type);
    }

    memset(&p_out->mfx, 0, sizeof(mfxInfoMFX));

    if (p_in)
    {
        if (p_in->mfx.CodecId == MFX_CODEC_VP8)
            p_out->mfx.CodecId = p_in->mfx.CodecId;

        switch (p_in->mfx.CodecLevel)
        {
            case MFX_LEVEL_UNKNOWN:
                p_out->mfx.CodecLevel = p_in->mfx.CodecLevel;
                break;
        }

        if (p_in->mfx.NumThread < 128)
            p_out->mfx.NumThread = p_in->mfx.NumThread;

        if (p_in->AsyncDepth < MFX_MAX_ASYNC_DEPTH_VALUE)
            p_out->AsyncDepth = p_in->AsyncDepth;

        if ((p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) || (p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        {
            if ( !((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)) )
                p_out->IOPattern = p_in->IOPattern;
        }

        if (p_in->mfx.FrameInfo.FourCC)
        {
            // mfxFrameInfo
            if (p_in->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12)
                p_out->mfx.FrameInfo.FourCC = p_in->mfx.FrameInfo.FourCC;
            else
                sts = MFX_ERR_UNSUPPORTED;
        }

        switch(p_in->mfx.FrameInfo.ChromaFormat)
        {
            case MFX_CHROMAFORMAT_YUV420:
                p_out->mfx.FrameInfo.ChromaFormat = p_in->mfx.FrameInfo.ChromaFormat;
                break;
            default:
                if (p_in->mfx.FrameInfo.FourCC)
                    sts = MFX_ERR_UNSUPPORTED;

                break;
        }

        if (!p_in->mfx.FrameInfo.ChromaFormat && !(!p_in->mfx.FrameInfo.FourCC && !p_in->mfx.FrameInfo.ChromaFormat))
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Width % 16 == 0 && p_in->mfx.FrameInfo.Width <= 4096)
            p_out->mfx.FrameInfo.Width = p_in->mfx.FrameInfo.Width;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.Height % 16 == 0 && p_in->mfx.FrameInfo.Height <= 2304)
            p_out->mfx.FrameInfo.Height = p_in->mfx.FrameInfo.Height;
        else
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->mfx.FrameInfo.CropX <= p_out->mfx.FrameInfo.Width)
            p_out->mfx.FrameInfo.CropX = p_in->mfx.FrameInfo.CropX;

        if (p_in->mfx.FrameInfo.CropY <= p_out->mfx.FrameInfo.Height)
            p_out->mfx.FrameInfo.CropY = p_in->mfx.FrameInfo.CropY;

        if (p_out->mfx.FrameInfo.CropX + p_in->mfx.FrameInfo.CropW <= p_out->mfx.FrameInfo.Width)
            p_out->mfx.FrameInfo.CropW = p_in->mfx.FrameInfo.CropW;

        if (p_out->mfx.FrameInfo.CropY + p_in->mfx.FrameInfo.CropH <= p_out->mfx.FrameInfo.Height)
            p_out->mfx.FrameInfo.CropH = p_in->mfx.FrameInfo.CropH;

        p_out->mfx.FrameInfo.FrameRateExtN = p_in->mfx.FrameInfo.FrameRateExtN;
        p_out->mfx.FrameInfo.FrameRateExtD = p_in->mfx.FrameInfo.FrameRateExtD;

        p_out->mfx.FrameInfo.AspectRatioW = p_in->mfx.FrameInfo.AspectRatioW;
        p_out->mfx.FrameInfo.AspectRatioH = p_in->mfx.FrameInfo.AspectRatioH;

        mfxStatus stsExt = CheckDecodersExtendedBuffers(p_in);
        if (stsExt < MFX_ERR_NONE)
            sts = MFX_ERR_UNSUPPORTED;

        if (p_in->Protected)
        {
            sts = MFX_ERR_UNSUPPORTED;
        }

        mfxExtOpaqueSurfaceAlloc *opaque_in = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_in->ExtParam, p_in->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        mfxExtOpaqueSurfaceAlloc *opaque_out = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);

        if (opaque_in && opaque_out)
        {
            opaque_out->In.Type = opaque_in->In.Type;
            opaque_out->In.NumSurface = opaque_in->In.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->In.Surfaces, opaque_in->In.Surfaces, opaque_in->In.NumSurface);

            opaque_out->Out.Type = opaque_in->Out.Type;
            opaque_out->Out.NumSurface = opaque_in->Out.NumSurface;
            MFX_INTERNAL_CPY(opaque_out->Out.Surfaces, opaque_in->Out.Surfaces, opaque_in->Out.NumSurface);
        }
        else
        {
            if (opaque_out || opaque_in)
            {
                sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        if (GetPlatform(p_core, p_out) != p_core->GetPlatformType() && sts == MFX_ERR_NONE)
        {
            VM_ASSERT(GetPlatform(p_core, p_out) == MFX_PLATFORM_SOFTWARE);
            sts = MFX_WRN_PARTIAL_ACCELERATION;
        }
    }
    else
    {
        p_out->mfx.CodecId = MFX_CODEC_VP8;
        p_out->mfx.CodecProfile = 1;
        p_out->mfx.CodecLevel = 1;

        p_out->mfx.NumThread = 1;

        p_out->AsyncDepth = 1;

        // mfxFrameInfo
        p_out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        p_out->mfx.FrameInfo.Width = 16;
        p_out->mfx.FrameInfo.Height = 16;

        p_out->mfx.FrameInfo.FrameRateExtN = 1;
        p_out->mfx.FrameInfo.FrameRateExtD = 1;

        p_out->mfx.FrameInfo.AspectRatioW = 1;
        p_out->mfx.FrameInfo.AspectRatioH = 1;

        p_out->mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (type == MFX_HW_UNKNOWN)
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
        }
        else
        {
            p_out->IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        }

        mfxExtOpaqueSurfaceAlloc * opaqueOut = (mfxExtOpaqueSurfaceAlloc *)GetExtBuffer(p_out->ExtParam, p_out->NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        if (opaqueOut)
        {
        }
    }

    return sts;

} // mfxStatus MFX_VP8_Utility::Query(VideoCORE *p_core, mfxVideoParam *p_in, mfxVideoParam *p_out, eMFXHWType type)

bool MFX_VP8_Utility::CheckVideoParam(mfxVideoParam *p_in, eMFXHWType )
{
    if (!p_in)
        return false;

    if (p_in->Protected)
       return false;

    if (MFX_CODEC_VP8 != p_in->mfx.CodecId)
        return false;

    if (p_in->mfx.FrameInfo.Width > 4096 || (p_in->mfx.FrameInfo.Width % 16))
        return false;

    if (p_in->mfx.FrameInfo.Height > 4096 || (p_in->mfx.FrameInfo.Height % 16))
        return false;

#if 0
    // ignore Crop paramsameters at Init/Reset stage
    if (in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        return false;

    if (in->mfx.FrameInfo.CropX + in->mfx.FrameInfo.CropW > in->mfx.FrameInfo.Width)
        return false;

    if (in->mfx.FrameInfo.CropY + in->mfx.FrameInfo.CropH > in->mfx.FrameInfo.Height)
        return false;
#endif

    if (MFX_FOURCC_NV12 != p_in->mfx.FrameInfo.FourCC)
        return false;

    // both zero or not zero
    if ((p_in->mfx.FrameInfo.AspectRatioW || p_in->mfx.FrameInfo.AspectRatioH) && !(p_in->mfx.FrameInfo.AspectRatioW && p_in->mfx.FrameInfo.AspectRatioH))
        return false;

    switch (p_in->mfx.FrameInfo.PicStruct)
    {
        case MFX_PICSTRUCT_UNKNOWN:
        case MFX_PICSTRUCT_PROGRESSIVE:
            break;

        default:
            return false;
    }

    if (MFX_CHROMAFORMAT_YUV420 != p_in->mfx.FrameInfo.ChromaFormat)
    {
        return false;
    }

    if (!(p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if ((p_in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && (p_in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return false;

    if (p_in->mfx.CodecProfile > MFX_PROFILE_VP8_3)
        return false;

    if (p_in->mfx.CodecLevel != MFX_LEVEL_UNKNOWN)
        return false;

    return true;

} // bool MFX_VP8_Utility::CheckVideoParam(mfxVideoParam *p_in, eMFXHWType)
