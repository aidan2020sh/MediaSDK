//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#include "mfxvp9.h"
#include "mfx_enc_common.h"
#include "mfx_vp9_encode_hw_par.h"
#include "mfx_vp9_encode_hw_utils.h"
#include <math.h>
#include <memory.h>
#include "mfx_common_int.h"
#include "mfx_ext_buffers.h"

#if defined (PRE_SI_TARGET_PLATFORM_GEN10)

namespace MfxHwVP9Encode
{

bool IsExtBufferSupportedInInit(mfxU32 id)
{
    return id == MFX_EXTBUFF_CODING_OPTION_VP9
        || id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION
        || id == MFX_EXTBUFF_CODING_OPTION3;
}

bool IsExtBufferSupportedInRuntime(mfxU32 id)
{
    return id == MFX_EXTBUFF_CODING_OPTION_VP9;
}

mfxStatus CheckExtBufferHeaders(mfxU16 numExtParam, mfxExtBuffer** extParam, bool isRuntime)
{
    for (mfxU16 i = 0; i < numExtParam; i++)
    {
        MFX_CHECK_NULL_PTR1(extParam);

        mfxExtBuffer *pBuf = extParam[i];

        // check that NumExtParam complies with ExtParam
        MFX_CHECK_NULL_PTR1(extParam[i]);

        // check that there is no ext buffer duplication in ExtParam
        for (mfxU16 j = i + 1; j < numExtParam; j++)
        {
            if (extParam[j]->BufferId == pBuf->BufferId)
            {
                return MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }

        bool isSupported = isRuntime ?
            IsExtBufferSupportedInRuntime(pBuf->BufferId) : IsExtBufferSupportedInInit(pBuf->BufferId);

        if (!isSupported)
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

#define _CopyPar(pDst, pSrc, PAR) pDst->PAR = pSrc->PAR;
#define _SetOrCopyPar(PAR)\
if (pSrc)\
{\
    _CopyPar(pDst, pSrc, PAR); \
}\
else\
{\
    pDst->PAR = 1; \
}

inline mfxStatus SetOrCopySupportedParams(mfxInfoMFX *pDst, mfxInfoMFX *pSrc = 0)
{
    Zero(*pDst);

    MFX_CHECK_NULL_PTR1(pDst);

    _SetOrCopyPar(FrameInfo.Width);
    _SetOrCopyPar(FrameInfo.Height);
    _SetOrCopyPar(FrameInfo.CropW);
    _SetOrCopyPar(FrameInfo.CropH);
    _SetOrCopyPar(FrameInfo.CropX);
    _SetOrCopyPar(FrameInfo.CropY);
    _SetOrCopyPar(FrameInfo.FrameRateExtN);
    _SetOrCopyPar(FrameInfo.FrameRateExtD);
    _SetOrCopyPar(FrameInfo.AspectRatioW);
    _SetOrCopyPar(FrameInfo.AspectRatioH);
    _SetOrCopyPar(FrameInfo.ChromaFormat);
    _SetOrCopyPar(FrameInfo.BitDepthLuma);
    _SetOrCopyPar(FrameInfo.BitDepthChroma);
    _SetOrCopyPar(FrameInfo.Shift);
    _SetOrCopyPar(FrameInfo.PicStruct);
    _SetOrCopyPar(FrameInfo.FourCC);

    _SetOrCopyPar(LowPower);
    _SetOrCopyPar(CodecId);
    _SetOrCopyPar(CodecProfile);
    _SetOrCopyPar(TargetUsage);
    _SetOrCopyPar(GopPicSize);
    _SetOrCopyPar(GopRefDist);
    _SetOrCopyPar(RateControlMethod);
    _SetOrCopyPar(BufferSizeInKB);
    _SetOrCopyPar(InitialDelayInKB);
    _SetOrCopyPar(QPI);
    _SetOrCopyPar(TargetKbps);
    _SetOrCopyPar(MaxKbps);
    _SetOrCopyPar(BRCParamMultiplier);
    _SetOrCopyPar(NumRefFrame);

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtCodingOptionVP9 *pDst, mfxExtCodingOptionVP9 *pSrc = 0)
{
    MFX_CHECK_NULL_PTR1(pDst);

    ZeroExtBuffer(*pDst);

    _SetOrCopyPar(SharpnessLevel);
    _SetOrCopyPar(WriteIVFHeaders);
    _SetOrCopyPar(QIndexDeltaLumaDC);
    _SetOrCopyPar(QIndexDeltaChromaAC);
    _SetOrCopyPar(QIndexDeltaChromaDC);

    for (mfxU8 i = 0; i < MAX_REF_LF_DELTAS; i++)
    {
        _SetOrCopyPar(LoopFilterRefDelta[i]);
    }
    for (mfxU8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
    {
        _SetOrCopyPar(LoopFilterModeDelta[i]);
    }

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtCodingOption3 *pDst, mfxExtCodingOption3 *pSrc = 0)
{
    pSrc;
    MFX_CHECK_NULL_PTR1(pDst);

    ZeroExtBuffer(*pDst);

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    _SetOrCopyPar(TargetChromaFormatPlus1);
    _SetOrCopyPar(TargetBitDepthLuma);
    _SetOrCopyPar(TargetBitDepthChroma);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    return MFX_ERR_NONE;
}

mfxStatus SetSupportedParameters(mfxVideoParam & par)
{
    par.AsyncDepth = 1;
    par.IOPattern = 1;
    par.Protected = 0;
    memset(par.reserved, 0, sizeof(par.reserved));
    memset(&par.reserved2, 0, sizeof(par.reserved2));
    memset(&par.reserved3, 0, sizeof(par.reserved3));

    // mfxInfoMfx
    SetOrCopySupportedParams(&par.mfx);

    // extended buffers
    mfxStatus sts = CheckExtBufferHeaders(par.NumExtParam, par.ExtParam);
    MFX_CHECK_STS(sts);

    mfxExtCodingOptionVP9 *pOpt = GetExtBuffer(par);
    if (pOpt != 0)
    {
        SetOrCopySupportedParams(pOpt);
    }

    mfxExtCodingOption3 *pOpt3 = GetExtBuffer(par);
    if (pOpt3 != 0)
    {
        SetOrCopySupportedParams(pOpt3);
    }

    return MFX_ERR_NONE;
}

struct Bool
{
    Bool(bool initValue) : value(initValue) {};
    Bool & operator=(bool newValue)
    {
        value = newValue;
        return *this;
    };

    bool operator==(bool valueToCheck)
    {
        return (value == valueToCheck);
    };

    bool value;
};

bool CheckTriStateOption(mfxU16 & opt)
{
    if (opt != MFX_CODINGOPTION_UNKNOWN &&
        opt != MFX_CODINGOPTION_ON &&
        opt != MFX_CODINGOPTION_OFF)
    {
        opt = MFX_CODINGOPTION_UNKNOWN;
        return false;
    }

    return true;
}

template <class T, class U>
bool CheckRangeDflt(T & opt, U min, U max, U deflt)
{
    if (opt < static_cast<T>(min) || opt > static_cast<T>(max))
    {
        opt = static_cast<T>(deflt);
        return false;
    }

    return true;
}

mfxStatus CleanOutUnsupportedParameters(VP9MfxVideoParam &par)
{
    VP9MfxVideoParam tmp = par;
    mfxStatus sts = SetOrCopySupportedParams(&par.mfx, &tmp.mfx);
    MFX_CHECK_STS(sts);
    if (memcmp(&par.mfx, &tmp.mfx, sizeof(mfxInfoMFX)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    mfxExtCodingOptionVP9 &optTmp = GetExtBufferRef(tmp);
    mfxExtCodingOptionVP9 &optPar = GetExtBufferRef(par);
    SetOrCopySupportedParams(&optPar, &optTmp);
    if (memcmp(&optPar, &optTmp, sizeof(mfxExtCodingOptionVP9)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
bool IsChromaFormatSupported(mfxU16 profile, mfxU16 chromaFormat)
{
    switch (profile)
    {
    case MFX_PROFILE_VP9_0:
    case MFX_PROFILE_VP9_2:
        return chromaFormat == MFX_CHROMAFORMAT_YUV420;
    case MFX_PROFILE_VP9_1:
    case MFX_PROFILE_VP9_3:
        return chromaFormat == MFX_CHROMAFORMAT_YUV420 ||
            chromaFormat == MFX_CHROMAFORMAT_YUV444;
    }

    return false;
}

bool IsBitDepthSupported(mfxU16 profile, mfxU16 bitDepth)
{
    switch (profile)
    {
    case MFX_PROFILE_VP9_0:
    case MFX_PROFILE_VP9_1:
        return bitDepth == BITDEPTH_8;
    case MFX_PROFILE_VP9_2:
    case MFX_PROFILE_VP9_3:
        return bitDepth == 8 ||
            bitDepth == BITDEPTH_10;
    }

    return false;
}

mfxU16 GetChromaFormat(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_P010:
        return MFX_CHROMAFORMAT_YUV420;
    case MFX_FOURCC_AYUV:
    case MFX_FOURCC_Y410:
        return MFX_CHROMAFORMAT_YUV444;
    default:
        return 0;
    }
}

mfxU16 GetBitDepth(mfxU32 fourcc)
{
    switch (fourcc)
    {
    case MFX_FOURCC_NV12:
    case MFX_FOURCC_AYUV:
        return BITDEPTH_8;
    case MFX_FOURCC_Y410:
    case MFX_FOURCC_P010:
        return BITDEPTH_10;
    default:
        return 0;
    }
}

// check bit depth itself and it's compliance with fourcc
bool CheckBitDepth(mfxU16 depth, mfxU32 fourcc)
{
    if (depth != 0 &&
        (depth != BITDEPTH_8 && depth != BITDEPTH_10
        || fourcc != 0 && depth != GetBitDepth(fourcc)))
    {
        return false;
    }

    return true;
}

// check chroma format itself and it's compliance with fourcc
bool CheckChromaFormat(mfxU16 format, mfxU32 fourcc)
{
    if (format != MFX_CHROMAFORMAT_YUV420 && format != MFX_CHROMAFORMAT_YUV444
        || fourcc != 0 && format != GetChromaFormat(fourcc))
    {
        return false;
    }

    return true;
}
#endif // PRE_SI_TARGET_PLATFORM_GEN11

bool CheckFourcc(mfxU32 fourcc)
{
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    return fourcc == MFX_FOURCC_NV12 || fourcc == MFX_FOURCC_AYUV  // 8 bit
        || fourcc == MFX_FOURCC_P010 || fourcc == MFX_FOURCC_Y410; // 10 bit
#else //PRE_SI_TARGET_PLATFORM_GEN11
    return fourcc == MFX_FOURCC_NV12;
#endif //PRE_SI_TARGET_PLATFORM_GEN11
}

mfxStatus CheckParameters(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    caps;

    Bool changed = false;
    Bool unsupported = false;

    if (MFX_ERR_UNSUPPORTED == CleanOutUnsupportedParameters(par))
    {
        unsupported = true;
    }

    if (par.IOPattern &&
        par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY &&
        par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY &&
        par.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        par.IOPattern = 0;
        unsupported = true;
    }

    if (par.Protected != 0)
    {
        par.Protected = 0;
        unsupported = true;
    }

    // check mfxInfoMfx
    double      frameRate = 0.0;

    if ((par.mfx.FrameInfo.Width & 0x0f) != 0 || (par.mfx.FrameInfo.Height & 0x0f) != 0)
    {
        par.mfx.FrameInfo.Width = 0;
        par.mfx.FrameInfo.Height = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.Width &&
        (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width) ||
        par.mfx.FrameInfo.Height &&
        (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height))
    {
        par.mfx.FrameInfo.CropX = 0;
        par.mfx.FrameInfo.CropW = 0;
        par.mfx.FrameInfo.CropY = 0;
        par.mfx.FrameInfo.CropH = 0;
        unsupported = true;
    }

    if ((par.mfx.FrameInfo.FrameRateExtN == 0) != (par.mfx.FrameInfo.FrameRateExtD == 0))
    {
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        frameRate = (double)par.mfx.FrameInfo.FrameRateExtN / (double)par.mfx.FrameInfo.FrameRateExtD;
    }

    if (par.mfx.FrameInfo.FrameRateExtD != 0 &&
        (frameRate < 1.0 || frameRate > 180.0))
    {
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.AspectRatioH != 0 || par.mfx.FrameInfo.AspectRatioW != 0)
    {
        if (!(par.mfx.FrameInfo.AspectRatioH == 1 && par.mfx.FrameInfo.AspectRatioW == 1))
        {
            par.mfx.FrameInfo.AspectRatioH = 1;
            par.mfx.FrameInfo.AspectRatioW = 1;
            changed = true;
        }
    }

    if (par.mfx.FrameInfo.PicStruct > MFX_PICSTRUCT_PROGRESSIVE)
    {
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        unsupported = true;
    }

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    {
        mfxExtCodingOption3& opt3 = GetExtBufferRef(par);
        mfxU32& fourcc         = par.mfx.FrameInfo.FourCC;
        mfxU16& inFormat       = par.mfx.FrameInfo.ChromaFormat;
        mfxU16& inDepthLuma    = par.mfx.FrameInfo.BitDepthLuma;
        mfxU16& inDepthChroma  = par.mfx.FrameInfo.BitDepthLuma;
        mfxU16& profile        = par.mfx.CodecProfile;
        mfxU16& outFormatP1    = opt3.TargetChromaFormatPlus1;
        mfxU16& outDepthLuma   = opt3.TargetBitDepthLuma;
        mfxU16& outDepthChroma = opt3.TargetBitDepthLuma;

        if (fourcc != 0
            && false == CheckFourcc(fourcc))
        {
            fourcc = 0;
            unsupported = true;
        }

        if (false == CheckChromaFormat(inFormat, fourcc))
        {
            inFormat = 0;
            unsupported = true;
        }

        if (false == CheckBitDepth(inDepthLuma, fourcc))
        {
            inDepthLuma = 0;
            unsupported = true;
        }

        if (false == CheckBitDepth(inDepthChroma, fourcc))
        {
            inDepthChroma = 0;
            unsupported = true;
        }

        if (inDepthLuma != 0 && inDepthChroma != 0 &&
            inDepthLuma != inDepthChroma)
        {
            inDepthChroma = 0;
            unsupported = true;
        }

        if (par.mfx.FrameInfo.Shift != 0
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            && fourcc != MFX_FOURCC_P010
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            )
        {
            par.mfx.FrameInfo.Shift = 0;
            unsupported = true;
        }

        if (outFormatP1 != 0 && false == CheckChromaFormat(outFormatP1 - 1, fourcc))
        {
            outFormatP1 = 0;
            unsupported = true;
        }

        if (false == CheckBitDepth(outDepthLuma, fourcc))
        {
            outDepthLuma = 0;
            unsupported = true;
        }

        if (false == CheckBitDepth(outDepthChroma, fourcc))
        {
            outDepthChroma = 0;
            unsupported = true;
        }

        if (outDepthLuma != 0 && outDepthChroma != 0 &&
            outDepthLuma != outDepthChroma)
        {
            outDepthChroma = 0;
            unsupported = true;
        }

        // check compliance of profile and chroma format, bit depth
        if (profile != 0)
        {
            if (outFormatP1 != 0 && false == IsChromaFormatSupported(profile, outFormatP1 - 1))
            {
                outFormatP1 = 0;
                unsupported = true;
            }

            if (outDepthLuma != 0 && false == IsBitDepthSupported(profile, outDepthLuma))
            {
                outDepthLuma = 0;
                unsupported = true;
            }

            if (outDepthChroma != 0 && false == IsBitDepthSupported(profile, outDepthChroma))
            {
                outDepthChroma = 0;
                unsupported = true;
            }
        }
    }
#else //PRE_SI_TARGET_PLATFORM_GEN11
    if (par.mfx.CodecProfile > MFX_PROFILE_VP9_0)
    {
        par.mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.ChromaFormat > MFX_CHROMAFORMAT_YUV420)
    {
        par.mfx.FrameInfo.ChromaFormat = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.FourCC != 0 && par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12)
    {
        par.mfx.FrameInfo.FourCC = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.BitDepthLuma != 0 && par.mfx.FrameInfo.BitDepthLuma != 8)
    {
        par.mfx.FrameInfo.BitDepthLuma = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.BitDepthChroma != 0 && par.mfx.FrameInfo.BitDepthChroma != 8)
    {
        par.mfx.FrameInfo.BitDepthChroma = 0;
        unsupported = true;
    }

    if (par.mfx.FrameInfo.Shift != 0)
    {
        par.mfx.FrameInfo.Shift = 0;
        unsupported = true;
    }
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    if (par.mfx.NumThread > 1)
    {
        par.mfx.NumThread = 0;
        unsupported = true;
    }

    if (par.mfx.TargetUsage > MFX_TARGETUSAGE_BEST_SPEED)
    {
        par.mfx.TargetUsage = MFX_TARGETUSAGE_UNKNOWN;
        unsupported = true;
    }

    if (par.mfx.GopRefDist > 1)
    {
        par.mfx.GopRefDist = 1;
        changed = true;
    }

    if (par.mfx.RateControlMethod != 0)
    {
        if (par.mfx.RateControlMethod > MFX_RATECONTROL_CQP)
        {
            par.mfx.RateControlMethod = 0;
            par.mfx.InitialDelayInKB = 0;
            par.mfx.BufferSizeInKB = 0;
            par.mfx.TargetKbps = 0;
            par.mfx.MaxKbps = 0;
            par.mfx.BRCParamMultiplier = 0;
            unsupported = true;
        }
        else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR
                || par.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
        {
            if ((par.mfx.RateControlMethod == MFX_RATECONTROL_CBR  && par.mfx.MaxKbps != par.mfx.TargetKbps) ||
                (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR  && par.mfx.MaxKbps < par.mfx.TargetKbps))
            {
                par.mfx.MaxKbps = 0;
                changed = true;
            }

            if (par.mfx.InitialDelayInKB > par.mfx.BufferSizeInKB)
            {
                par.mfx.InitialDelayInKB = 0;
                unsupported = true;
            }
        }
        else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            if (par.mfx.QPI > MAX_Q_INDEX)
            {
                par.mfx.QPI = MAX_Q_INDEX;
                changed = true;
            }

            if (par.mfx.QPP > MAX_Q_INDEX)
            {
                par.mfx.QPP = MAX_Q_INDEX;
                changed = true;
            }

            if (par.mfx.QPB > 0)
            {
                par.mfx.QPB = 0;
                changed = true;
            }
        }
    }

    if (par.mfx.NumRefFrame > 3)
    {
        par.mfx.NumRefFrame = 3;
        changed = true;
    }

    // check extended buffers
    mfxExtCodingOptionVP9 &opt = GetExtBufferRef(par);
    if (false == CheckTriStateOption(opt.EnableMultipleSegments))
    {
        changed = true;
    }
    if (false == CheckTriStateOption(opt.WriteIVFHeaders))
    {
        changed = true;
    }

    if (false == CheckRangeDflt(opt.QIndexDeltaLumaDC, -MAX_ABS_Q_INDEX_DELTA, MAX_ABS_Q_INDEX_DELTA, 0))
    {
        changed = true;
    }

    if (false == CheckRangeDflt(opt.QIndexDeltaChromaAC, -MAX_ABS_Q_INDEX_DELTA, MAX_ABS_Q_INDEX_DELTA, 0))
    {
        changed = true;
    }

    if (false == CheckRangeDflt(opt.QIndexDeltaChromaDC, -MAX_ABS_Q_INDEX_DELTA, MAX_ABS_Q_INDEX_DELTA, 0))
    {
        changed = true;
    }

    for (mfxU8 i = 0; i < MAX_REF_LF_DELTAS; i++)
    {
        if (false == CheckRangeDflt(opt.LoopFilterRefDelta[i], -MAX_LF_LEVEL, MAX_LF_LEVEL, 0))
        {
            changed = true;
        }
    }

    for (mfxU8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
    {
        if (false == CheckRangeDflt(opt.LoopFilterModeDelta[i], -MAX_LF_LEVEL, MAX_LF_LEVEL, 0))
        {
            changed = true;
        }
    }

    if (unsupported == true)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return (changed == true) ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

template <typename T>
inline bool SetDefault(T &par, int defaultValue)
{
    bool defaultSet = false;

    if (par == 0)
    {
        par = (T)defaultValue;
        defaultSet = true;
    }

    return defaultSet;
}

template <typename T>
inline bool SetTwoDefaults(T &par1, T &par2, int defaultValue1, int defaultValue2)
{
    bool defaultsSet = false;

    if (par1 == 0 && par2 == 0)
    {
        par1 = (T)defaultValue1;
        par2 = (T)defaultValue2;
        defaultsSet = true;
    }

    return defaultsSet;
}

inline bool IsBufferBasedBRC(mfxU16 brcMethod)
{
    return brcMethod == MFX_RATECONTROL_CBR
        || brcMethod == MFX_RATECONTROL_VBR;
}

inline mfxU16 GetDefaultBufferSize(VP9MfxVideoParam const &par)
{
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        const mfxU8 defaultBufferInSec = 2;
        return defaultBufferInSec * ((par.mfx.TargetKbps + 7) / 8);
    }
    else
    {
        mfxFrameInfo const &fi = par.mfx.FrameInfo;

        const mfxU16 result_max_value = 0xffff;

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        if(par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || par.mfx.FrameInfo.FourCC == MFX_FOURCC_Y410) {
            mfxU32 result = (fi.Width * fi.Height * 3) / 1000; // size of two uncompressed 420 8bit frames in KB
            return (result < result_max_value ? (mfxU16)result : result_max_value);
        }
#endif //PRE_SI_TARGET_PLATFORM_GEN11
        mfxU32 result = (fi.Width * fi.Height * 3) / 2 / 1000;
        return (result < result_max_value ? (mfxU16)result : result_max_value); // uncompressed frame size for 420 8bit in KB
    }
}

inline mfxU16 GetDefaultAsyncDepth(VP9MfxVideoParam const &par)
{
    par;
    return 2;
}

inline mfxU16 GetMinProfile(mfxU16 depth, mfxU16 format)
{
    return MFX_PROFILE_VP9_0 +
        (depth > 8) * 2 +
        (format > MFX_CHROMAFORMAT_YUV420);
}

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
void SetDefailtsForProfileAndFrameInfo(VP9MfxVideoParam& par)
{
    mfxFrameInfo& fi = par.mfx.FrameInfo;

    SetDefault(fi.ChromaFormat, GetChromaFormat(fi.FourCC));
    SetDefault(fi.BitDepthLuma, GetBitDepth(fi.FourCC));
    SetDefault(fi.BitDepthChroma, GetBitDepth(fi.FourCC));

    mfxExtCodingOption3 &opt3 = GetExtBufferRef(par);
    SetDefault(opt3.TargetChromaFormatPlus1, fi.ChromaFormat + 1);
    SetDefault(opt3.TargetBitDepthLuma, fi.BitDepthLuma);
    SetDefault(opt3.TargetBitDepthChroma, fi.BitDepthChroma);

    SetDefault(par.mfx.CodecProfile, GetMinProfile(opt3.TargetBitDepthLuma, opt3.TargetChromaFormatPlus1 - 1));
}
#endif //PRE_SI_TARGET_PLATFORM_GEN11

#define DEFAULT_GOP_SIZE 0xffff
#define DEFAULT_FRAME_RATE 30

mfxStatus SetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    SetDefault(par.AsyncDepth, GetDefaultAsyncDepth(par));

    // mfxInfoMfx
    SetDefault(par.mfx.GopPicSize, DEFAULT_GOP_SIZE);
    SetDefault(par.mfx.GopRefDist, 1);
    SetDefault(par.mfx.NumRefFrame, 1);
    SetDefault(par.mfx.BRCParamMultiplier, 1);
    SetDefault(par.mfx.TargetUsage, MFX_TARGETUSAGE_BALANCED);
    SetDefault(par.mfx.RateControlMethod, MFX_RATECONTROL_CQP);
    SetDefault(par.mfx.BufferSizeInKB, GetDefaultBufferSize(par));
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        SetDefault(par.mfx.InitialDelayInKB, par.mfx.BufferSizeInKB / 2);
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        SetDefault(par.mfx.MaxKbps, par.mfx.TargetKbps);
    }

    // mfxInfomfx.FrameInfo
    mfxFrameInfo &fi = par.mfx.FrameInfo;

    if (false == SetTwoDefaults(fi.FrameRateExtN, fi.FrameRateExtD, 30, 1))
    {
        SetDefault(fi.FrameRateExtN, fi.FrameRateExtD * DEFAULT_FRAME_RATE);
        SetDefault(fi.FrameRateExtD,
            fi.FrameRateExtN % DEFAULT_FRAME_RATE == 0 ? fi.FrameRateExtN / DEFAULT_FRAME_RATE : 1);
    }
    SetTwoDefaults(fi.AspectRatioW, fi.AspectRatioH, 1, 1);
    SetDefault(fi.PicStruct, MFX_PICSTRUCT_PROGRESSIVE);

    // profile, chroma format, bit depth
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    SetDefailtsForProfileAndFrameInfo(par);
#else //PRE_SI_TARGET_PLATFORM_GEN11
    SetDefault(par.mfx.CodecProfile, MFX_PROFILE_VP9_0);
    SetDefault(fi.ChromaFormat, MFX_CHROMAFORMAT_YUV420);
    SetDefault(fi.BitDepthLuma, 8);
    SetDefault(fi.BitDepthChroma, 8);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    SetDefault(par.mfx.QPI, 128);
    SetDefault(par.mfx.QPP, par.mfx.QPI + 5);

    // ext buffers
    mfxExtCodingOptionVP9 &opt = GetExtBufferRef(par);
    SetDefault(opt.WriteIVFHeaders, MFX_CODINGOPTION_ON);

    // check final set of parameters
    mfxStatus sts = CheckParameters(par, caps);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CheckParametersAndSetDefaults(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
    // check mandatory parameters
    // (1) resolution
    mfxFrameInfo const &fi = par.mfx.FrameInfo;
    if (fi.Width == 0 || fi.Height == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // (2) fourcc
    if (fi.FourCC == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // (3) target and max bitrates
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR && par.mfx.TargetKbps == 0)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR &&
        (par.mfx.TargetKbps == 0 || par.mfx.MaxKbps == 0))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // (4) crops
    if (fi.CropW || fi.CropH || fi.CropX || fi.CropY)
    {
        if (fi.CropW == 0 || fi.CropH == 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    // (5) opaque memory allocation
    if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxExtOpaqueSurfaceAlloc &opaq = GetExtBufferRef(par);
        if (opaq.In.NumSurface == 0 ||
            opaq.In.Surfaces == 0 ||
            (opaq.In.Type & MFX_MEMTYPE_SYS_OR_D3D) == 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    mfxStatus sts = MFX_ERR_NONE;

    // check parameters defined by application
    mfxStatus checkSts = MFX_ERR_NONE;
    checkSts = CheckParameters(par, caps);
    if (checkSts == MFX_ERR_UNSUPPORTED)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // check non-mandatory parameters which require return of WARNING if not set
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        (par.mfx.QPI == 0 || par.mfx.QPP == 0))
    {
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // set defaults for parameters not defined by application
    sts = SetDefaults(par, caps);
    MFX_CHECK(sts >= 0, sts);

    return checkSts;
}

mfxStatus CheckEncodeFrameParam(
    VP9MfxVideoParam const & video,
    mfxEncodeCtrl       * ctrl,
    mfxFrameSurface1    * surface,
    mfxBitstream        * bs)
{
    mfxStatus checkSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    // check that surface contains valid data
    if (surface != 0)
    {
        MFX_CHECK(CheckFourcc(surface->Info.FourCC), MFX_ERR_INVALID_VIDEO_PARAM);

        if (video.m_inMemType == INPUT_SYSTEM_MEMORY)
        {
            MFX_CHECK(surface->Data.Y != 0, MFX_ERR_NULL_PTR);
            MFX_CHECK(surface->Data.U != 0, MFX_ERR_NULL_PTR);
            MFX_CHECK(surface->Data.V != 0, MFX_ERR_NULL_PTR);
        }
        else
        {
            MFX_CHECK(surface->Data.MemId != 0, MFX_ERR_INVALID_VIDEO_PARAM);
        }

        MFX_CHECK(surface->Info.Width <= video.mfx.FrameInfo.Width, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(surface->Info.Height <= video.mfx.FrameInfo.Height, MFX_ERR_INVALID_VIDEO_PARAM);

        mfxU32 pitch = (surface->Data.PitchHigh << 16) + surface->Data.PitchLow;
        MFX_CHECK(pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    // check bitstream buffer for enough space
    MFX_CHECK(bs->MaxLength > 0, MFX_ERR_NOT_ENOUGH_BUFFER);
    MFX_CHECK(bs->DataOffset < bs->MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(bs->MaxLength > bs->DataOffset + bs->DataLength, MFX_ERR_NOT_ENOUGH_BUFFER);
    MFX_CHECK(bs->DataOffset + bs->DataLength + video.mfx.BufferSizeInKB * 1000 <= bs->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);

    // check mfxEncodeCtrl for correct parameters
    if (ctrl)
    {
        MFX_CHECK (ctrl->QP <= 255, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        MFX_CHECK (ctrl->FrameType <= MFX_FRAMETYPE_P, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    }

    return checkSts;
}
} //namespace MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
