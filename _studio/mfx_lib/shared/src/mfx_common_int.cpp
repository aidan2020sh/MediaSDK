//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2009-2017 Intel Corporation. All Rights Reserved.
//

#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"
#include "mfxpcp.h"
#include "mfxfei.h"
#include <stdexcept>
#include <string>

#if defined(_WIN32) || defined(_WIN64)
#include <DXGI.h>
#endif

mfxStatus CheckVideoParamCommon(mfxVideoParam *in, eMFXHWType type);

mfxExtBuffer* GetExtendedBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
        {
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
        }
    }

    return 0;
}

mfxExtBuffer* GetExtendedBufferInternal(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    mfxExtBuffer* result = GetExtendedBuffer(extBuf, numExtBuf, id);

    if (!result) throw std::logic_error(": no external buffer found");
    return result;
}

mfxStatus CheckFrameInfoCommon(mfxFrameInfo  *info, mfxU32 /* codecId */)
{
    if (!info)
        return MFX_ERR_NULL_PTR;

    if ((info->Width % 16) || !info->Width)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((info->Height % 16) || !info->Height)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    switch (info->FourCC)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_YV12:
    case MFX_FOURCC_YUY2:
    case MFX_FOURCC_RGB3:
    case MFX_FOURCC_RGB4:
    case MFX_FOURCC_P010:
    case MFX_FOURCC_NV16:
    case MFX_FOURCC_P210:
    case MFX_FOURCC_AYUV:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    case MFX_FOURCC_Y210:
    case MFX_FOURCC_Y216:
    case MFX_FOURCC_Y410:
#endif //PRE_SI_TARGET_PLATFORM_GEN11
#if defined(_WIN32) || defined(_WIN64)
    case DXGI_FORMAT_AYUV:
#endif
        break;
    default:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->BitDepthLuma > 8 || info->BitDepthChroma > 8)
    {
        switch (info->FourCC)
        {
        case MFX_FOURCC_P010:
        case MFX_FOURCC_P210:
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        case MFX_FOURCC_Y210:
        case MFX_FOURCC_Y216:
        case MFX_FOURCC_Y410:
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            break;
        default:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (info->Shift)
    {
        if (info->FourCC != MFX_FOURCC_P010 && info->FourCC != MFX_FOURCC_P210)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (info->ChromaFormat > MFX_CHROMAFORMAT_YUV444)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (info->FrameRateExtN != 0 && info->FrameRateExtD == 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((info->AspectRatioW || info->AspectRatioH) &&
        (!info->AspectRatioW || !info->AspectRatioH))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus CheckFrameInfoEncoders(mfxFrameInfo  *info)
{
    if (info->CropX > info->Width)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (info->CropY > info->Height)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (info->CropX + info->CropW > info->Width)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (info->CropY + info->CropH > info->Height)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    switch (info->PicStruct)
    {
    case MFX_PICSTRUCT_UNKNOWN:
    case MFX_PICSTRUCT_PROGRESSIVE:
    case MFX_PICSTRUCT_FIELD_TFF:
    case MFX_PICSTRUCT_FIELD_BFF:
        break;

    default:
    case MFX_PICSTRUCT_FIELD_REPEATED:
    case MFX_PICSTRUCT_FRAME_DOUBLING:
    case MFX_PICSTRUCT_FRAME_TRIPLING:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // both zero or not zero
    if ((info->AspectRatioW || info->AspectRatioH) && !(info->AspectRatioW && info->AspectRatioH))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((info->FrameRateExtN || info->FrameRateExtD) && !(info->FrameRateExtN && info->FrameRateExtD))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus CheckFrameInfoCodecs(mfxFrameInfo  *info, mfxU32 codecId, bool isHW)
{
    mfxStatus sts = CheckFrameInfoCommon(info, codecId);
    if (sts != MFX_ERR_NONE)
        return sts;

    switch (codecId)
    {
    case MFX_CODEC_JPEG:
#if defined(_WIN32) || defined(_WIN64)
        if (info->FourCC != MFX_FOURCC_NV12 && info->FourCC != DXGI_FORMAT_AYUV && info->FourCC != MFX_FOURCC_RGB4 && info->FourCC != MFX_FOURCC_YUY2)
            return MFX_ERR_INVALID_VIDEO_PARAM;
#else
        if (info->FourCC != MFX_FOURCC_NV12 && info->FourCC != MFX_FOURCC_RGB4 && info->FourCC != MFX_FOURCC_YUY2)
            return MFX_ERR_INVALID_VIDEO_PARAM;
#endif
        break;
    case MFX_CODEC_VP8:
        if (info->FourCC != MFX_FOURCC_NV12 && info->FourCC != MFX_FOURCC_YV12)
            return MFX_ERR_INVALID_VIDEO_PARAM;
            break;
    case MFX_CODEC_VP9:
        if (info->FourCC != MFX_FOURCC_NV12
            && info->FourCC != MFX_FOURCC_P010
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            && info->FourCC != MFX_FOURCC_AYUV
            && info->FourCC != MFX_FOURCC_Y410
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            )
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    case MFX_CODEC_AVC:
        if (info->FourCC != MFX_FOURCC_NV12 &&
            info->FourCC != MFX_FOURCC_P010 &&
            info->FourCC != MFX_FOURCC_NV16 &&
            info->FourCC != MFX_FOURCC_P210)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    case MFX_CODEC_HEVC:
        if (info->FourCC != MFX_FOURCC_NV12 &&
            info->FourCC != MFX_FOURCC_YUY2 &&
            info->FourCC != MFX_FOURCC_P010 &&
            info->FourCC != MFX_FOURCC_NV16
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            && info->FourCC != MFX_FOURCC_AYUV
            && info->FourCC != MFX_FOURCC_Y216
            && info->FourCC != MFX_FOURCC_Y210
            && info->FourCC != MFX_FOURCC_Y410
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            )
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    default:
        if (info->FourCC != MFX_FOURCC_NV12)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    }

    switch (codecId)
    {
    case MFX_CODEC_JPEG:
        if (info->ChromaFormat != MFX_CHROMAFORMAT_YUV420 && info->ChromaFormat != MFX_CHROMAFORMAT_YUV444 &&
            info->ChromaFormat != MFX_CHROMAFORMAT_YUV400 && info->ChromaFormat != MFX_CHROMAFORMAT_YUV422H)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    case MFX_CODEC_AVC:
        if (info->ChromaFormat != MFX_CHROMAFORMAT_YUV420 && info->ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
            info->ChromaFormat != MFX_CHROMAFORMAT_YUV400)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    case MFX_CODEC_HEVC:
    case MFX_CODEC_VP9:
        if (info->ChromaFormat != MFX_CHROMAFORMAT_YUV420
            && info->ChromaFormat != MFX_CHROMAFORMAT_YUV400
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            && info->ChromaFormat != MFX_CHROMAFORMAT_YUV422
            && info->ChromaFormat != MFX_CHROMAFORMAT_YUV444
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            )
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    default:
        if (info->ChromaFormat != MFX_CHROMAFORMAT_YUV420 && info->ChromaFormat != MFX_CHROMAFORMAT_YUV400)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        break;
    }

    if (codecId != MFX_CODEC_HEVC && 
       (info->FourCC == MFX_FOURCC_P010 ||
        info->FourCC == MFX_FOURCC_P210))
    {
        if (info->Shift != (isHW ? 1 : 0))
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus CheckAudioParamCommon(mfxAudioParam *in)
{
//    mfxStatus sts;

    switch (in->mfx.CodecId)
    {
        case MFX_CODEC_AAC:
        case MFX_CODEC_MP3:
            break;
        default:
            return MFX_ERR_INVALID_AUDIO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus CheckAudioParamDecoders(mfxAudioParam *in)
{
    mfxStatus sts = CheckAudioParamCommon(in);
    if (sts < MFX_ERR_NONE)
        return sts;

    return MFX_ERR_NONE;
}

mfxStatus CheckAudioParamEncoders(mfxAudioParam *in)
{
    mfxStatus sts = CheckAudioParamCommon(in);
    if (sts < MFX_ERR_NONE)
        return sts;

    return MFX_ERR_NONE;
}


mfxStatus CheckVideoParamCommon(mfxVideoParam *in, eMFXHWType type)
{
    if (!in)
        return MFX_ERR_NULL_PTR;

    mfxStatus sts = CheckFrameInfoCodecs(&in->mfx.FrameInfo, in->mfx.CodecId, type != MFX_HW_UNKNOWN);
    if (sts != MFX_ERR_NONE)
        return sts;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (in->Protected)
    {
        if (type == MFX_HW_UNKNOWN || !IS_PROTECTION_ANY(in->Protected))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        mfxExtPAVPOption * pavpOpt = (mfxExtPAVPOption*)GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION);
        if (pavpOpt && pavpOpt->Header.BufferSz != sizeof(mfxExtPAVPOption))
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (IS_PROTECTION_PAVP_ANY(in->Protected) && !pavpOpt)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if (!IS_PROTECTION_PAVP_ANY(in->Protected) && pavpOpt)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }
#endif

    switch (in->mfx.CodecId)
    {
        case MFX_CODEC_AVC:
        case MFX_CODEC_HEVC:
        case MFX_CODEC_MPEG2:
        case MFX_CODEC_VC1:
        case MFX_CODEC_JPEG:
        case MFX_CODEC_VP8:
        case MFX_CODEC_VP9:
            break;
        default:
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (!in->IOPattern)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (in->mfx.CodecId == MFX_CODEC_HEVC &&
       (in->mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || in->mfx.FrameInfo.FourCC == MFX_FOURCC_P210))
    {
        if (type == MFX_HW_UNKNOWN)
        {
            if (in->mfx.FrameInfo.Shift != 0)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        else
        {
            if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && in->mfx.FrameInfo.Shift != 1)
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus CheckVideoParamDecoders(mfxVideoParam *in, bool IsExternalFrameAllocator, eMFXHWType type)
{
    mfxStatus sts = CheckVideoParamCommon(in, type);
    if (sts < MFX_ERR_NONE)
        return sts;

    if (!(in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY) && !(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if ((in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (in->mfx.DecodedOrder && in->mfx.CodecId != MFX_CODEC_JPEG && in->mfx.CodecId != MFX_CODEC_AVC && in->mfx.CodecId != MFX_CODEC_HEVC)
        return MFX_ERR_UNSUPPORTED;

    if (!IsExternalFrameAllocator && (in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (in->Protected)
    {
        if (in->mfx.CodecId == MFX_CODEC_AVC || in->mfx.CodecId == MFX_CODEC_JPEG)
        {
            sts = CheckDecodersExtendedBuffers(in);
            if (sts < MFX_ERR_NONE)
                return sts;
        }
        else
        {
            if (IS_PROTECTION_PAVP_ANY(in->Protected) &&
               (in->NumExtParam != 1 || !GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION)))
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (!((in->IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY)||(in->IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY)))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        return MFX_ERR_NONE;
    }
#endif

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (GetExtendedBuffer(in->ExtParam, in->NumExtParam, MFX_EXTBUFF_PAVP_OPTION))
        return MFX_ERR_INVALID_VIDEO_PARAM;
#endif

    sts = CheckDecodersExtendedBuffers(in);
    if (sts < MFX_ERR_NONE)
        return sts;

    return MFX_ERR_NONE;
}

mfxStatus CheckVideoParamEncoders(mfxVideoParam *in, bool IsExternalFrameAllocator, eMFXHWType type)
{
    mfxStatus sts = CheckFrameInfoEncoders(&in->mfx.FrameInfo);
    if (sts < MFX_ERR_NONE)
        return sts;

    sts = CheckVideoParamCommon(in, type);
    if (sts < MFX_ERR_NONE)
        return sts;

    if (!IsExternalFrameAllocator && (in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (in->Protected && !(in->IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    return MFX_ERR_NONE;
}

mfxStatus CheckAudioFrame(const mfxAudioFrame *aFrame)
{
    if (!aFrame || !aFrame->Data)
        return MFX_ERR_NULL_PTR;

    if (aFrame->DataLength > aFrame->MaxLength)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_ERR_NONE;
}


mfxStatus CheckBitstream(const mfxBitstream *bs)
{
    if (!bs || !bs->Data)
        return MFX_ERR_NULL_PTR;

    if (bs->DataOffset + bs->DataLength > bs->MaxLength)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (bs->EncryptedData)
    {
        mfxEncryptedData * encryptedData = bs->EncryptedData;
        while (encryptedData)
        {
            if (!encryptedData->Data)
                return MFX_ERR_NULL_PTR;

            if (!encryptedData->DataLength || encryptedData->DataOffset + encryptedData->DataLength > encryptedData->MaxLength)
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            encryptedData = encryptedData->Next;
        }
    }
#endif
    return MFX_ERR_NONE;
}

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
mfxStatus CheckEncryptedBitstream(const mfxBitstream *bs)
{
       mfxEncryptedData *ptr = 0;
        if  (!(bs->DataFlag & MFX_BITSTREAM_COMPLETE_FRAME))
            return MFX_ERR_UNDEFINED_BEHAVIOR;

        if(bs->EncryptedData)
        {
            if(!bs->EncryptedData->Data)
            return MFX_ERR_NULL_PTR;

            if((bs->EncryptedData->DataOffset > bs->EncryptedData->MaxLength)
                || (bs->EncryptedData->DataLength > bs->EncryptedData->MaxLength))
                return MFX_ERR_UNDEFINED_BEHAVIOR;

            if((bs->DataLength > 4) && (bs->EncryptedData->DataLength == 0))
                 return MFX_ERR_MORE_DATA;

            ptr = bs->EncryptedData->Next;

            while(ptr)
            {
                if(ptr->DataOffset > ptr->MaxLength || ptr->DataLength > ptr->MaxLength)
                    return MFX_ERR_UNDEFINED_BEHAVIOR;

                if(ptr->DataLength == 0)
                    return MFX_ERR_MORE_DATA;

                if(!ptr->Data)
                    return MFX_ERR_NULL_PTR;

                ptr = ptr->Next;
            }
        }
    return MFX_ERR_NONE;
}
#endif // #if !defined(MFX_PROTECTED_FEATURE_DISABLE)

mfxStatus CheckFrameData(const mfxFrameSurface1 *surface)
{
    if (!surface)
        return MFX_ERR_NULL_PTR;

    if (!surface->Data.MemId)
    {
        mfxU32 pitch = surface->Data.PitchLow + ((mfxU32)surface->Data.PitchHigh << 16);

        switch (surface->Info.FourCC)
        {
        case MFX_FOURCC_NV12:
        case MFX_FOURCC_P010:
        case MFX_FOURCC_P210:
        case MFX_FOURCC_NV16:
            if (!surface->Data.Y || !surface->Data.UV)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0xFFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        case MFX_FOURCC_YV12:
            if (!surface->Data.Y || !surface->Data.U || !surface->Data.V)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0xFFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        case MFX_FOURCC_YUY2:
            if (!surface->Data.Y || !surface->Data.U || !surface->Data.V)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0x1FFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        case MFX_FOURCC_RGB3:
        case MFX_FOURCC_A2RGB10:
            if (!surface->Data.R || !surface->Data.G || !surface->Data.B)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0x2FFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        case MFX_FOURCC_RGB4:
        case MFX_FOURCC_AYUV:
            if (!surface->Data.A || !surface->Data.R || !surface->Data.G || !surface->Data.B)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0x3FFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        case MFX_FOURCC_Y410:
            if (!surface->Data.Y || !surface->Data.Y410)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
#endif //PRE_SI_TARGET_PLATFORM_GEN11
        case MFX_FOURCC_P8:
            if (!surface->Data.Y)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
#if defined(_WIN32) || defined(_WIN64)
        case DXGI_FORMAT_AYUV:
            if (!surface->Data.A || !surface->Data.R || !surface->Data.G || !surface->Data.B)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            if (pitch > 0x3FFFF)
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
#endif
        default:
            break;
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus CheckDecodersExtendedBuffers(mfxVideoParam* par)
{
    static const mfxU32 g_commonSupportedExtBuffers[]       = {
                                                               MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
#ifndef MFX_ADAPTIVE_PLAYBACK_DISABLE
                                                               MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK,
#endif
    };

    static const mfxU32 g_decoderSupportedExtBuffersAVC[]   = {
                                                               MFX_EXTBUFF_MVC_SEQ_DESC,
                                                               MFX_EXTBUFF_MVC_TARGET_VIEWS,
#if !defined (MFX_PROTECTED_FEATURE_DISABLE)
                                                               MFX_EXTBUFF_PAVP_OPTION,
#endif
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
                                                               MFX_EXTBUFF_DEC_VIDEO_PROCESSING,
#endif
                                                               MFX_EXTBUFF_FEI_PARAM};

    static const mfxU32 g_decoderSupportedExtBuffersHEVC[]  = {
                                                               MFX_EXTBUFF_HEVC_PARAM
#ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE
                                                               , MFX_EXTBUFF_FORCE_PRIVATE_DDI
#endif
                                                               };

    static const mfxU32 g_decoderSupportedExtBuffersVC1[]   = {MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION,
#if !defined (MFX_PROTECTED_FEATURE_DISABLE)
                                                               MFX_EXTBUFF_PAVP_OPTION,
#endif
                                                               };


    static const mfxU32 g_decoderSupportedExtBuffersMJPEG[] = {MFX_EXTBUFF_JPEG_HUFFMAN,
                                                               MFX_EXTBUFF_JPEG_QT};

    const mfxU32 *supported_buffers = 0;
    mfxU32 numberOfSupported = 0;

    if (par->mfx.CodecId == MFX_CODEC_AVC)
    {
        supported_buffers = g_decoderSupportedExtBuffersAVC;
        numberOfSupported = sizeof(g_decoderSupportedExtBuffersAVC) / sizeof(g_decoderSupportedExtBuffersAVC[0]);
    }
    else if (par->mfx.CodecId == MFX_CODEC_VC1 || par->mfx.CodecId == MFX_CODEC_MPEG2)
    {
        supported_buffers = g_decoderSupportedExtBuffersVC1;
        numberOfSupported = sizeof(g_decoderSupportedExtBuffersVC1) / sizeof(g_decoderSupportedExtBuffersVC1[0]);
    }
    else if (par->mfx.CodecId == MFX_CODEC_HEVC)
    {
        supported_buffers = g_decoderSupportedExtBuffersHEVC;
        numberOfSupported = sizeof(g_decoderSupportedExtBuffersHEVC) / sizeof(g_decoderSupportedExtBuffersHEVC[0]);
    }
    else if (par->mfx.CodecId == MFX_CODEC_JPEG)
    {
        supported_buffers = g_decoderSupportedExtBuffersMJPEG;
        numberOfSupported = sizeof(g_decoderSupportedExtBuffersMJPEG) / sizeof(g_decoderSupportedExtBuffersMJPEG[0]);
    }
    else
    {
        supported_buffers = g_commonSupportedExtBuffers;
        numberOfSupported = sizeof(g_commonSupportedExtBuffers) / sizeof(g_commonSupportedExtBuffers[0]);
    }

    if (!supported_buffers)
        return MFX_ERR_NONE;

    const mfxU32 *common_supported_buffers = g_commonSupportedExtBuffers;
    mfxU32 common_numberOfSupported = sizeof(g_commonSupportedExtBuffers) / sizeof(g_commonSupportedExtBuffers[0]);

    for (mfxU32 i = 0; i < par->NumExtParam; i++)
    {
        if (par->ExtParam[i] == NULL)
        {
           return MFX_ERR_NULL_PTR;
        }

        bool is_known = false;
        for (mfxU32 j = 0; j < numberOfSupported; ++j)
        {
            if (par->ExtParam[i]->BufferId == supported_buffers[j])
            {
                is_known = true;
                break;
            }
        }

        for (mfxU32 j = 0; j < common_numberOfSupported; ++j)
        {
            if (par->ExtParam[i]->BufferId == common_supported_buffers[j])
            {
                is_known = true;
                break;
            }
        }

        if (!is_known)
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extended Buffer class
//////////////////////////////////////////////////////////////////////////////////////////////////////////
ExtendedBuffer::ExtendedBuffer()
{
}

ExtendedBuffer::~ExtendedBuffer()
{
    Release();
}

void ExtendedBuffer::Release()
{
    if(m_buffers.empty())
        return;

    BuffersList::iterator iter = m_buffers.begin();
    BuffersList::iterator iter_end = m_buffers.end();

    for( ; iter != iter_end; ++iter)
    {
        mfxU8 * buffer = (mfxU8 *)*iter;
        delete[] buffer;
    }

    m_buffers.clear();
}

size_t ExtendedBuffer::GetCount() const
{
    return m_buffers.size();
}

void ExtendedBuffer::AddBuffer(mfxExtBuffer * in)
{
    if (GetBufferByIdInternal(in->BufferId))
        return;

    mfxExtBuffer * buffer = (mfxExtBuffer *)(new mfxU8[in->BufferSz]);
    memset(buffer, 0, in->BufferSz);
    buffer->BufferSz = in->BufferSz;
    buffer->BufferId = in->BufferId;
    AddBufferInternal(buffer);
}

void ExtendedBuffer::AddBufferInternal(mfxExtBuffer * buffer)
{
    m_buffers.push_back(buffer);
}

mfxExtBuffer ** ExtendedBuffer::GetBuffers()
{
    return &m_buffers[0];
}

mfxExtBuffer * ExtendedBuffer::GetBufferByIdInternal(mfxU32 id)
{
    BuffersList::iterator iter = m_buffers.begin();
    BuffersList::iterator iter_end = m_buffers.end();

    for( ; iter != iter_end; ++iter)
    {
        mfxExtBuffer * buffer = *iter;
        if (buffer->BufferId == id)
            return buffer;
    }

    return 0;
}

mfxExtBuffer * ExtendedBuffer::GetBufferByPositionInternal(mfxU32 pos)
{
    if (pos >= m_buffers.size())
        return 0;

    return m_buffers[pos];
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  mfxVideoParamWrapper implementation
////////////////////////////////////////////////////////////////////////////////////////////////////////////
mfxVideoParamWrapper::mfxVideoParamWrapper()
    : m_mvcSequenceBuffer(0)
{
}

mfxVideoParamWrapper::mfxVideoParamWrapper(const mfxVideoParam & par)
    : m_mvcSequenceBuffer(0)
{
    CopyVideoParam(par);
}

mfxVideoParamWrapper::~mfxVideoParamWrapper()
{
    delete[] m_mvcSequenceBuffer;
}

mfxVideoParamWrapper & mfxVideoParamWrapper::operator = (const mfxVideoParam & par)
{
    CopyVideoParam(par);
    return *this;
}

mfxVideoParamWrapper & mfxVideoParamWrapper::operator = (const mfxVideoParamWrapper & par)
{
    return mfxVideoParamWrapper::operator = ( *((const mfxVideoParam *)&par));
}

bool mfxVideoParamWrapper::CreateExtendedBuffer(mfxU32 bufferId)
{
    if (m_buffers.GetBufferById<void *>(bufferId))
        return true;

    switch(bufferId)
    {
    case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
        m_buffers.AddTypedBuffer<mfxExtVideoSignalInfo>(bufferId);
        break;

    case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
        m_buffers.AddTypedBuffer<mfxExtCodingOptionSPSPPS>(bufferId);
        break;

    case MFX_EXTBUFF_HEVC_PARAM:
        m_buffers.AddTypedBuffer<mfxExtHEVCParam>(bufferId);
        break;

    default:
        VM_ASSERT(false);
        return false;
    }

    NumExtParam = (mfxU16)m_buffers.GetCount();
    ExtParam = NumExtParam ? m_buffers.GetBuffers() : 0;

    return true;
}

void mfxVideoParamWrapper::CopyVideoParam(const mfxVideoParam & par)
{
    mfxVideoParam * temp = this;
    *temp = par;

    this->NumExtParam = 0;
    this->ExtParam = 0;

    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        switch(par.ExtParam[i]->BufferId)
        {
#ifdef MFX_EXTBUFF_FORCE_PRIVATE_DDI_ENABLE
        case MFX_EXTBUFF_FORCE_PRIVATE_DDI:
#endif
#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
        case MFX_EXTBUFF_DEC_VIDEO_PROCESSING:
#endif
        case MFX_EXTBUFF_MVC_TARGET_VIEWS:
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        case MFX_EXTBUFF_PAVP_OPTION:
#endif
        case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
        case MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION:
#if defined(MFX_ENABLE_SVC_VIDEO_DECODE)
        case MFX_EXTBUFF_SVC_SEQ_DESC:
        case MFX_EXTBUFF_SVC_TARGET_LAYER:
#endif
#ifndef MFX_ADAPTIVE_PLAYBACK_DISABLE
        case MFX_EXTBUFF_DEC_ADAPTIVE_PLAYBACK:
#endif
        case MFX_EXTBUFF_JPEG_QT:
        case MFX_EXTBUFF_JPEG_HUFFMAN:
        case MFX_EXTBUFF_HEVC_PARAM:
        case MFX_EXTBUFF_FEI_PARAM:
            {
                void * in = GetExtendedBufferInternal(par.ExtParam, par.NumExtParam, par.ExtParam[i]->BufferId);
                m_buffers.AddBuffer(par.ExtParam[i]);
                mfxExtBuffer * out = m_buffers.GetBufferById<mfxExtBuffer >(par.ExtParam[i]->BufferId);
                if (NULL == out)
                {
                    VM_ASSERT(false);
                    throw UMC::UMC_ERR_FAILED;
                }
                memcpy_s((void*)out, out->BufferSz, in, par.ExtParam[i]->BufferSz);
            }
            break;
        case MFX_EXTBUFF_MVC_SEQ_DESC:
            {
                mfxExtMVCSeqDesc * mvcPoints = (mfxExtMVCSeqDesc *)GetExtendedBufferInternal(par.ExtParam, par.NumExtParam, MFX_EXTBUFF_MVC_SEQ_DESC);

                m_buffers.AddTypedBuffer<mfxExtMVCSeqDesc>(MFX_EXTBUFF_MVC_SEQ_DESC);
                mfxExtMVCSeqDesc * points = m_buffers.GetBufferById<mfxExtMVCSeqDesc>(MFX_EXTBUFF_MVC_SEQ_DESC);

                size_t size = mvcPoints->NumView * sizeof(mfxMVCViewDependency) + mvcPoints->NumOP * sizeof(mfxMVCOperationPoint) + mvcPoints->NumViewId * sizeof(mfxU16);
                if (!m_mvcSequenceBuffer)
                    m_mvcSequenceBuffer = new mfxU8[size];
                else
                {
                    delete[] m_mvcSequenceBuffer;
                    m_mvcSequenceBuffer = new mfxU8[size];
                }

                mfxU8 * ptr = m_mvcSequenceBuffer;

                if (points)
                {
                    points->NumView = points->NumViewAlloc = mvcPoints->NumView;
                    points->View = (mfxMVCViewDependency * )ptr;
                    memcpy_s(points->View, mvcPoints->NumView * sizeof(mfxMVCViewDependency), mvcPoints->View, mvcPoints->NumView * sizeof(mfxMVCViewDependency));
                    ptr += mvcPoints->NumView * sizeof(mfxMVCViewDependency);

                    points->NumView = points->NumViewAlloc = mvcPoints->NumView;
                    points->ViewId = (mfxU16 *)ptr;
                    memcpy_s(points->ViewId, mvcPoints->NumViewId * sizeof(mfxU16), mvcPoints->ViewId, mvcPoints->NumViewId * sizeof(mfxU16));
                    ptr += mvcPoints->NumViewId * sizeof(mfxU16);

                    points->NumOP = points->NumOPAlloc = mvcPoints->NumOP;
                    points->OP = (mfxMVCOperationPoint *)ptr;
                    memcpy_s(points->OP, mvcPoints->NumOP * sizeof(mfxMVCOperationPoint), mvcPoints->OP, mvcPoints->NumOP * sizeof(mfxMVCOperationPoint));

                    mfxU16 * targetView = points->ViewId;
                    for (mfxU32 j = 0; j < points->NumOP; j++)
                    {
                        points->OP[j].TargetViewId = targetView;
                        targetView += points->OP[j].NumTargetViews;
                    }
                }
            }
            break;

        case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
            break;

        default:
            {
                VM_ASSERT(false);
                throw UMC::UMC_ERR_FAILED;
            }
            break;
        };
    }

    NumExtParam = (mfxU16)m_buffers.GetCount();
    ExtParam = NumExtParam ? m_buffers.GetBuffers() : 0;
}
