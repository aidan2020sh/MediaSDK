//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2016-2017 Intel Corporation. All Rights Reserved.
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
    return id == MFX_EXTBUFF_VP9_PARAM
        || id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION
        || id == MFX_EXTBUFF_CODING_OPTION2
        || id == MFX_EXTBUFF_CODING_OPTION3
        || id == MFX_EXTBUFF_DDI // TODO: remove when IVFHeader will be added to API or disabled by default
        || id == MFX_EXTBUFF_VP9_SEGMENTATION
        || id == MFX_EXTBUFF_VP9_TEMPORAL_LAYERS
        || id == MFX_EXTBUFF_ENCODER_RESET_OPTION;
}

bool IsExtBufferIgnoredInRuntime(mfxU32 id)
{
    // we may just ignore MFX_EXTBUFF_VPP_AUXDATA as it provides auxiliary information
    return id == MFX_EXTBUFF_VPP_AUXDATA;
}

bool IsExtBufferSupportedInRuntime(mfxU32 id)
{
    return id == MFX_EXTBUFF_VP9_PARAM
        || id == MFX_EXTBUFF_VP9_SEGMENTATION
        || IsExtBufferIgnoredInRuntime(id);
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

// below code allows to do 3 things:
// 1) Set 1 to parameters which are configurable (supported) by the encoder.
// 2) Copy only supported parameters:
//    a) During input paramerets validation to check if all parameters passed by app are supported.
//    b) In Reset() API function to inherrit all defaults from configuration that was actual prior to Reset() call.
#define COPY_PAR_IF_ZERO(pDst, pSrc, PAR)\
if (pDst->PAR == 0)\
{\
    pDst->PAR = pSrc->PAR;\
}

#define SET_OR_COPY_PAR(PAR)\
if (pSrc)\
{\
    COPY_PAR_IF_ZERO(pDst, pSrc, PAR); \
}\
else\
{\
    pDst->PAR = 1; \
}

#define SET_OR_COPY_PAR_DONT_INHERIT(PAR)\
if (pSrc)\
{\
    if (zeroDst)\
    {\
        COPY_PAR_IF_ZERO(pDst, pSrc, PAR); \
    }\
}\
else\
{\
    pDst->PAR = 1; \
}

#define COPY_PTR(PTR)\
if (pSrc)\
{\
    COPY_PAR_IF_ZERO(pDst, pSrc, PTR); \
}\

inline mfxStatus SetOrCopySupportedParams(mfxInfoMFX *pDst, mfxInfoMFX const *pSrc = 0, bool zeroDst = true)
{
    if (zeroDst)
    {
        Zero(*pDst);
    }

    MFX_CHECK_NULL_PTR1(pDst);

    SET_OR_COPY_PAR(FrameInfo.Width);
    SET_OR_COPY_PAR(FrameInfo.Height);
    SET_OR_COPY_PAR(FrameInfo.CropW);
    SET_OR_COPY_PAR(FrameInfo.CropH);
    SET_OR_COPY_PAR(FrameInfo.CropX);
    SET_OR_COPY_PAR(FrameInfo.CropY);
    SET_OR_COPY_PAR(FrameInfo.FrameRateExtN);
    SET_OR_COPY_PAR(FrameInfo.FrameRateExtD);
    SET_OR_COPY_PAR(FrameInfo.AspectRatioW);
    SET_OR_COPY_PAR(FrameInfo.AspectRatioH);
    SET_OR_COPY_PAR(FrameInfo.ChromaFormat);
    SET_OR_COPY_PAR(FrameInfo.BitDepthLuma);
    SET_OR_COPY_PAR(FrameInfo.BitDepthChroma);
    SET_OR_COPY_PAR(FrameInfo.Shift);
    SET_OR_COPY_PAR(FrameInfo.PicStruct);
    SET_OR_COPY_PAR(FrameInfo.FourCC);

    SET_OR_COPY_PAR(LowPower);
    SET_OR_COPY_PAR(CodecId);
    SET_OR_COPY_PAR(CodecProfile);
    SET_OR_COPY_PAR(TargetUsage);
    SET_OR_COPY_PAR(GopPicSize);
    SET_OR_COPY_PAR(GopRefDist);
    SET_OR_COPY_PAR(RateControlMethod);
    SET_OR_COPY_PAR(BufferSizeInKB);
    SET_OR_COPY_PAR(InitialDelayInKB);
    SET_OR_COPY_PAR(QPI);
    SET_OR_COPY_PAR(TargetKbps);
    SET_OR_COPY_PAR(MaxKbps);
    SET_OR_COPY_PAR(BRCParamMultiplier);
    SET_OR_COPY_PAR(NumRefFrame);

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtVP9Param *pDst, mfxExtVP9Param const *pSrc = 0, bool zeroDst = true)
{
    MFX_CHECK_NULL_PTR1(pDst);

    if (zeroDst)
    {
        ZeroExtBuffer(*pDst);
    }

    SET_OR_COPY_PAR_DONT_INHERIT(FrameWidth);
    SET_OR_COPY_PAR_DONT_INHERIT(FrameHeight);

    /*
    SET_OR_COPY_PAR(WriteIVFHeaders);

    for (mfxU8 i = 0; i < MAX_REF_LF_DELTAS; i++)
    {
        SET_OR_COPY_PAR(LoopFilterRefDelta[i]);
    }
    for (mfxU8 i = 0; i < MAX_MODE_LF_DELTAS; i++)
    {
        SET_OR_COPY_PAR(LoopFilterModeDelta[i]);
    }

    SET_OR_COPY_PAR(QIndexDeltaLumaDC);
    SET_OR_COPY_PAR(QIndexDeltaChromaAC);
    SET_OR_COPY_PAR(QIndexDeltaChromaDC);
    */

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtCodingOption2 *pDst, mfxExtCodingOption2 const *pSrc = 0, bool zeroDst = true)
{
    MFX_CHECK_NULL_PTR1(pDst);

    if (zeroDst)
    {
        ZeroExtBuffer(*pDst);
    }

    SET_OR_COPY_PAR(MBBRC);

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtCodingOption3 *pDst, mfxExtCodingOption3 const *pSrc = 0, bool zeroDst = true)
{
    pSrc;
    MFX_CHECK_NULL_PTR1(pDst);

    if (zeroDst)
    {
        ZeroExtBuffer(*pDst);
    }

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    SET_OR_COPY_PAR(TargetChromaFormatPlus1);
    SET_OR_COPY_PAR(TargetBitDepthLuma);
    SET_OR_COPY_PAR(TargetBitDepthChroma);
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtVP9Segmentation *pDst, mfxExtVP9Segmentation const *pSrc = 0, bool zeroDst = true)
{
    MFX_CHECK_NULL_PTR1(pDst);

    if (zeroDst)
    {
        ZeroExtBuffer(*pDst);
    }

    SET_OR_COPY_PAR_DONT_INHERIT(NumSegments);
    SET_OR_COPY_PAR(SegmentIdBlockSize);
    SET_OR_COPY_PAR(NumSegmentIdAlloc);

    for (mfxU8 i = 0; i < MAX_SEGMENTS; i++)
    {
        SET_OR_COPY_PAR_DONT_INHERIT(Segment[i].FeatureEnabled);
        SET_OR_COPY_PAR_DONT_INHERIT(Segment[i].ReferenceFrame);
        SET_OR_COPY_PAR_DONT_INHERIT(Segment[i].LoopFilterLevelDelta);
        SET_OR_COPY_PAR_DONT_INHERIT(Segment[i].QIndexDelta);
    }

    COPY_PTR(SegmentId);

    return MFX_ERR_NONE;
}

inline mfxStatus SetOrCopySupportedParams(mfxExtVP9TemporalLayers *pDst, mfxExtVP9TemporalLayers const *pSrc = 0, bool zeroDst = true)
{
    MFX_CHECK_NULL_PTR1(pDst);

    if (zeroDst)
    {
        ZeroExtBuffer(*pDst);
    }

    for (mfxU8 i = 0; i < MAX_NUM_TEMP_LAYERS_SUPPORTED; i++)
    {
        SET_OR_COPY_PAR_DONT_INHERIT(Layer[i].FrameRateScale);
        SET_OR_COPY_PAR_DONT_INHERIT(Layer[i].TargetKbps);
    }

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

    mfxExtVP9Param *pPar = GetExtBuffer(par);
    if (pPar != 0)
    {
        SetOrCopySupportedParams(pPar);
    }

    mfxExtCodingOption2 *pOpt2 = GetExtBuffer(par);
    if (pOpt2 != 0)
    {
        SetOrCopySupportedParams(pOpt2);
    }

    mfxExtCodingOption3 *pOpt3 = GetExtBuffer(par);
    if (pOpt3 != 0)
    {
        SetOrCopySupportedParams(pOpt3);
    }

    mfxExtVP9TemporalLayers *pTL = GetExtBuffer(par);
    if (pTL != 0)
    {
        SetOrCopySupportedParams(pTL);
    }

    return MFX_ERR_NONE;
}

inline bool IsOn(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_ON;
}

inline bool IsOff(mfxU32 opt)
{
    return opt == MFX_CODINGOPTION_OFF;
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

void InheritDefaults(VP9MfxVideoParam& defaultsDst, VP9MfxVideoParam const & defaultsSrc)
{
    // inherit default from mfxInfoMfx
    SetOrCopySupportedParams(&defaultsDst.mfx, &defaultsSrc.mfx, false);

    // inherit defaults from mfxExtVP9Param
    mfxExtVP9Param* pParDst = GetExtBuffer(defaultsDst);
    mfxExtVP9Param* pParSrc = GetExtBuffer(defaultsSrc);
    SetOrCopySupportedParams(pParDst, pParSrc, false);

    // inherit defaults from mfxExtCodingOption2
    mfxExtCodingOption2* pOpt2Dst = GetExtBuffer(defaultsDst);
    mfxExtCodingOption2* pOpt2Src = GetExtBuffer(defaultsSrc);
    SetOrCopySupportedParams(pOpt2Dst, pOpt2Src, false);

    // inherit defaults from mfxExtCodingOption3
    mfxExtCodingOption3* pOpt3Dst = GetExtBuffer(defaultsDst);
    mfxExtCodingOption3* pOpt3Src = GetExtBuffer(defaultsSrc);
    SetOrCopySupportedParams(pOpt3Dst, pOpt3Src, false);

    // inherit defaults from mfxExtVP9Segmentation
    mfxExtVP9Segmentation* pSegDst = GetExtBuffer(defaultsDst);
    mfxExtVP9Segmentation* pSegSrc = GetExtBuffer(defaultsSrc);
    if (defaultsDst.m_segBufPassed == true)
    {
        mfxU16 numSegmentsDst = pSegDst->NumSegments;
        if (numSegmentsDst != 0)
        {
            // NumSegments set to 0 means disabling of segmentation
            // so there is no need to inherit any segmentation paramaters
            SetOrCopySupportedParams(pSegDst, pSegSrc, false);
        }
    }
    else
    {
        SetOrCopySupportedParams(pSegDst, pSegSrc);
    }

    // inherit defaults from mfxExtVP9Segmentation
    mfxExtVP9TemporalLayers* pTLDst = GetExtBuffer(defaultsDst);
    mfxExtVP9TemporalLayers* pTLSrc = GetExtBuffer(defaultsSrc);
    SetOrCopySupportedParams(pTLDst, pTLSrc, !defaultsDst.m_tempLayersBufPassed);

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

    mfxExtVP9Param &parTmp = GetExtBufferRef(tmp);
    mfxExtVP9Param &parPar = GetExtBufferRef(par);
    SetOrCopySupportedParams(&parPar, &parTmp);
    if (memcmp(&parPar, &parTmp, sizeof(mfxExtVP9Param)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    mfxExtVP9Segmentation &segTmp = GetExtBufferRef(tmp);
    mfxExtVP9Segmentation &segPar = GetExtBufferRef(par);
    SetOrCopySupportedParams(&segPar, &segTmp);
    if (memcmp(&segPar, &segTmp, sizeof(mfxExtVP9Segmentation)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    mfxExtVP9TemporalLayers &tlTmp = GetExtBufferRef(tmp);
    mfxExtVP9TemporalLayers &tlPar = GetExtBufferRef(par);
    SetOrCopySupportedParams(&tlPar, &tlTmp);
    if (memcmp(&segPar, &segTmp, sizeof(mfxExtVP9TemporalLayers)))
    {
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

bool IsBrcModeSupported(mfxU16 brc)
{
    return brc == MFX_RATECONTROL_CBR
        || brc == MFX_RATECONTROL_VBR
        || brc == MFX_RATECONTROL_CQP
        || brc == MFX_RATECONTROL_ICQ;
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

mfxU16 MapTUToSupportedRange(mfxU16 tu)
{
    switch (tu)
    {
    case MFX_TARGETUSAGE_1:
    case MFX_TARGETUSAGE_2:
        return MFX_TARGETUSAGE_BEST_QUALITY;
    case MFX_TARGETUSAGE_3:
    case MFX_TARGETUSAGE_4:
    case MFX_TARGETUSAGE_5:
        return MFX_TARGETUSAGE_BALANCED;
    case MFX_TARGETUSAGE_6:
    case MFX_TARGETUSAGE_7:
        return MFX_TARGETUSAGE_BEST_SPEED;
    default:
        return MFX_TARGETUSAGE_UNKNOWN;
    }
}

inline bool IsFeatureSupported(ENCODE_CAPS_VP9 const & caps, mfxU8 feature)
{
    return (caps.SegmentFeatureSupport & (1 << feature)) != 0;
}

inline void Disable(mfxU16& features, mfxU8 feature)
{
    features &= ~(1 << feature);
}

inline bool CheckFeature(mfxU16& features, mfxI16* featureValue, mfxU8 feature, ENCODE_CAPS_VP9 const & caps)
{
    bool status = true;
    // check QIndex feature
    if (IsFeatureEnabled(features, feature) && !IsFeatureSupported(caps, feature))
    {
        Disable(features, feature);
        status = false;
    }

    if (featureValue && *featureValue && !IsFeatureEnabled(features, feature))
    {
        *featureValue = 0;
        status = false;
    }

    return status;
}

bool CheckAndFixQIndexDelta(mfxI16& qIndexDelta, mfxU16 qIndex)
{
    mfxI16 minQIdxDelta = qIndex ? 1 - qIndex : 1 - MAX_Q_INDEX;
    mfxI16 maxQIdxDelta = MAX_Q_INDEX - qIndex;

    // if Q index is OK, but Q index value + delta is out of valid range - clamp Q index delta
    return Clamp(qIndexDelta, minQIdxDelta, maxQIdxDelta);
}

mfxStatus CheckPerSegmentParams(mfxVP9SegmentParam& segPar, ENCODE_CAPS_VP9 const & caps, mfxU16 QP)
{
    Bool changed = false;
    mfxU16& features = segPar.FeatureEnabled;

    // check QIndex feature
    if (false == CheckFeature(features, &segPar.QIndexDelta, FEAT_QIDX, caps))
    {
        changed = true;
    }

    // if delta Q index value is out of valid range - just ignore it
    if (false == CheckRangeDflt(segPar.QIndexDelta, -MAX_Q_INDEX, MAX_Q_INDEX, 0))
    {
        changed = true;
    }
    else
    {
        // if delta Q index value is OK, but Q index value + delta is out of valid range - clamp Q index delta
        if (false == CheckAndFixQIndexDelta(segPar.QIndexDelta, QP))
        {
            changed = true;
        }
    }


    // check LF Level feature
    if (false == CheckFeature(features, &segPar.LoopFilterLevelDelta, FEAT_LF_LVL, caps))
    {
        changed = true;
    }

    if (false == CheckRangeDflt(segPar.LoopFilterLevelDelta, -MAX_LF_LEVEL, MAX_LF_LEVEL, 0))
    {
        changed = true;
    }

    // check reference feature
    if (false == CheckFeature(features, reinterpret_cast<mfxI16*>(&segPar.ReferenceFrame), FEAT_REF, caps))
    {
        changed = true;
    }

    if (segPar.ReferenceFrame &&
        segPar.ReferenceFrame != MFX_VP9_REF_INTRA &&
        segPar.ReferenceFrame != MFX_VP9_REF_LAST &&
        segPar.ReferenceFrame != MFX_VP9_REF_GOLDEN &&
        segPar.ReferenceFrame != MFX_VP9_REF_ALTREF)
    {
        segPar.ReferenceFrame = 0;
        changed = true;
    }

    // check skip feature
    if (false == CheckFeature(features, 0, FEAT_SKIP, caps))
    {
        changed = true;
    }

    return (changed == true) ?
        MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

mfxStatus CheckSegmentationMap(mfxU8 const * segMap, mfxU32 numSegmentIdAlloc, mfxU16 numSegments)
{
    for (mfxU32 i = 0; i < numSegmentIdAlloc; i++)
    {
        if (segMap[i] >= numSegments)
        {
            return MFX_ERR_UNSUPPORTED;
        }
    }

    return MFX_ERR_NONE;
}

inline mfxStatus GetCheckStatus(Bool& changed, Bool& unsupported)
{
    if (unsupported == true)
    {
        return MFX_ERR_UNSUPPORTED;
    }

    return (changed == true) ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

inline void ConvertStatusToBools(Bool& changed, Bool& unsupported, mfxStatus sts)
{
    if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
    {
        changed = true;
    }
    else if (sts == MFX_ERR_UNSUPPORTED)
    {
        unsupported = true;
    }
}

mfxStatus CheckSegmentationParam(mfxExtVP9Segmentation& seg, mfxU32 frameWidth, mfxU32 frameHeight, ENCODE_CAPS_VP9 const & caps, mfxU16 QP)
{
    Bool changed = false;
    Bool unsupported = false;

    if ((seg.NumSegments != 0 || true == AnyMandatorySegMapParam(seg)) && caps.ForcedSegmentationSupport == 0)
    {
        // driver/HW don't support segmentation
        // set all segmentation parameters to 0
        ZeroExtBuffer(seg);
        unsupported = true;
    }

    if (seg.NumSegments > MAX_SEGMENTS)
    {
        // further parameter check hardly rely on NumSegments. Cannot fix value of NumSegments and then use modified value for checks. Need to drop and return MFX_ERR_UNSUPPORTED
        seg.NumSegments = 0;
        unsupported = true;
    }

    // for Gen10 driver supports only 16x16 granularity for segmentation map
    // but every 32x32 block of seg map should contain equal segment ids because of HW limitation
    // so segment map is accepted from application in terms of 32x32 or 64x64 blocks
    if (seg.SegmentIdBlockSize && seg.SegmentIdBlockSize < MFX_VP9_SEGMENT_ID_BLOCK_SIZE_32x32)
    {
        seg.SegmentIdBlockSize = 0;
        unsupported = true;
    }

    // check that NumSegmentIdAlloc is enough for given frame resolution and block size
    mfxU16 blockSize = MapIdToBlockSize(seg.SegmentIdBlockSize);
    if (seg.NumSegmentIdAlloc && blockSize && frameWidth && frameHeight)
    {
        mfxU32 widthInBlocks = (frameWidth + blockSize - 1) / blockSize;
        mfxU32 heightInBlocks = (frameHeight + blockSize - 1) / blockSize;
        mfxU32 sizeInBlocks = widthInBlocks * heightInBlocks;

        if (seg.NumSegmentIdAlloc && seg.NumSegmentIdAlloc < sizeInBlocks)
        {
            seg.NumSegmentIdAlloc = 0;
            seg.SegmentIdBlockSize = 0;
            unsupported = true;
        }
    }

    for (mfxU16 i = 0; i < seg.NumSegments; i++)
    {
        mfxStatus sts = CheckPerSegmentParams(seg.Segment[i], caps, QP);
        if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        {
            changed = true;
        }
    }

    // clean out per-segment parameters for segments with numbers exceeding seg.NumSegments
    if (seg.NumSegments)
    {
        for (mfxU16 i = seg.NumSegments; i < MAX_SEGMENTS; i++)
        {
            if (seg.Segment[i].LoopFilterLevelDelta ||
                seg.Segment[i].QIndexDelta ||
                seg.Segment[i].ReferenceFrame)
            {
                Zero(seg.Segment[i]);
                changed = true;
            }
        }
    }

    // check that segmentation map contains only valid segment ids
    if (seg.NumSegments && seg.NumSegmentIdAlloc && seg.SegmentId)
    {
        mfxStatus sts = CheckSegmentationMap(seg.SegmentId, seg.NumSegmentIdAlloc, seg.NumSegments);
        if (sts == MFX_ERR_UNSUPPORTED)
        {
            seg.SegmentId = 0;
            unsupported = true;
        }
    }

    return GetCheckStatus(changed, unsupported);
}

mfxStatus CheckTemporalLayers(VP9MfxVideoParam& par, ENCODE_CAPS_VP9 const & caps)
{
    Bool changed = false;
    Bool unsupported = false;

    if (par.m_layerParam[0].Scale && par.m_layerParam[0].Scale != 1)
    {
        par.m_layerParam[0].Scale = 0;
        unsupported = true;
    }

    mfxU8 i = 0;
    mfxU16 prevScale = 0;
    mfxU32 prevBitrate = 0;
    par.m_numLayers = 0;
    bool gapsInScales = false;

    for (; i < MAX_NUM_TEMP_LAYERS_SUPPORTED; i++)
    {
        mfxU16& scale = par.m_layerParam[i].Scale;
        mfxU32& bitrate = par.m_layerParam[i].targetKbps;

        if (scale)
        {
            if (prevScale && (scale <= prevScale || scale % prevScale))
            {
                scale = 0;
                unsupported = true;
            }
            else
            {
                prevScale = scale;
            }
        }
        else
        {
            gapsInScales = true;
        }

        if (bitrate)
        {
            if (par.mfx.RateControlMethod &&
                false == IsBitrateBasedBRC(par.mfx.RateControlMethod))
            {
                bitrate = 0;
                changed = true;
            }
            else
            {
                if (caps.TemporalLayerRateCtrl == 0 || bitrate <= prevBitrate)
                {
                    bitrate = 0;
                    unsupported = true;
                }
                else
                {
                    prevBitrate = bitrate;
                }
            }
        }

        if (scale && !gapsInScales)
        {
            par.m_numLayers++;
        }
    }

    if (unsupported == true)
    {
        par.m_numLayers = 0;
    }

    if (par.m_numLayers)
    {
        mfxU16 temporalCycleLenght = par.m_layerParam[par.m_numLayers - 1].Scale;
        if (par.mfx.GopPicSize && par.mfx.GopPicSize < temporalCycleLenght)
        {
            par.m_numLayers = 0;
            i = 0;
            changed = true;
        }
    }

    for (; i < MAX_NUM_TEMP_LAYERS; i++)
    {
        if (par.m_layerParam[i].Scale != 0 ||
            par.m_layerParam[i].targetKbps != 0)
        {
            Zero(par.m_layerParam[i]);
            changed = true;
        }
    }

    return GetCheckStatus(changed, unsupported);
}

inline mfxU16 MinRefsForTemporalLayers(mfxU16 numTL)
{
    if (numTL < 3)
    {
        return 1;
    }
    else
    {
        return numTL - 1;
    }
}

mfxStatus CheckParameters(VP9MfxVideoParam &par, ENCODE_CAPS_VP9 const &caps)
{
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

    mfxFrameInfo& fi = par.mfx.FrameInfo;

    if ((fi.Width & 0x0f) != 0 || (fi.Height & 0x0f) != 0)
    {
        fi.Width = 0;
        fi.Height = 0;
        unsupported = true;
    }

    if (fi.Width &&
        (fi.CropX + fi.CropW > fi.Width) ||
        fi.Height &&
        (fi.CropY + fi.CropH > fi.Height))
    {
        fi.CropX = 0;
        fi.CropW = 0;
        fi.CropY = 0;
        fi.CropH = 0;
        unsupported = true;
    }

    if ((fi.FrameRateExtN == 0) != (fi.FrameRateExtD == 0))
    {
        fi.FrameRateExtN = 0;
        fi.FrameRateExtD = 0;
        unsupported = true;
    }

    if (fi.FrameRateExtD != 0)
    {
        frameRate = (double)fi.FrameRateExtN / (double)fi.FrameRateExtD;
    }

    if (fi.FrameRateExtD != 0 &&
        (frameRate < 1.0 || frameRate > 180.0))
    {
        fi.FrameRateExtN = 0;
        fi.FrameRateExtD = 0;
        unsupported = true;
    }

    if (fi.AspectRatioH != 0 || fi.AspectRatioW != 0)
    {
        if (!(fi.AspectRatioH == 1 && fi.AspectRatioW == 1))
        {
            fi.AspectRatioH = 1;
            fi.AspectRatioW = 1;
            changed = true;
        }
    }

    if (fi.PicStruct > MFX_PICSTRUCT_PROGRESSIVE)
    {
        fi.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        unsupported = true;
    }

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    {
        mfxExtCodingOption3& opt3 = GetExtBufferRef(par);
        mfxU32& fourcc         = fi.FourCC;
        mfxU16& inFormat       = fi.ChromaFormat;
        mfxU16& inDepthLuma    = fi.BitDepthLuma;
        mfxU16& inDepthChroma  = fi.BitDepthLuma;
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

        if (fi.Shift != 0
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
            && fourcc != MFX_FOURCC_P010
#endif //PRE_SI_TARGET_PLATFORM_GEN11
            )
        {
            fi.Shift = 0;
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

    if (fi.ChromaFormat > MFX_CHROMAFORMAT_YUV420)
    {
        fi.ChromaFormat = 0;
        unsupported = true;
    }

    if (fi.FourCC != 0 && fi.FourCC != MFX_FOURCC_NV12)
    {
        fi.FourCC = 0;
        unsupported = true;
    }

    if (fi.BitDepthLuma != 0 && fi.BitDepthLuma != 8)
    {
        fi.BitDepthLuma = 0;
        unsupported = true;
    }

    if (fi.BitDepthChroma != 0 && fi.BitDepthChroma != 8)
    {
        fi.BitDepthChroma = 0;
        unsupported = true;
    }

    if (fi.Shift != 0)
    {
        fi.Shift = 0;
        unsupported = true;
    }
#endif //PRE_SI_TARGET_PLATFORM_GEN11

    if (par.mfx.NumThread > 1)
    {
        par.mfx.NumThread = 0;
        unsupported = true;
    }

    if (par.mfx.TargetUsage)
    {
        par.mfx.TargetUsage = MapTUToSupportedRange(par.mfx.TargetUsage);
        if (par.mfx.TargetUsage == MFX_TARGETUSAGE_UNKNOWN)
        {
            unsupported = true;
        }
    }

    if (par.mfx.GopRefDist > 1)
    {
        par.mfx.GopRefDist = 1;
        changed = true;
    }

    mfxStatus tlSts = CheckTemporalLayers(par, caps);
    ConvertStatusToBools(changed, unsupported, tlSts);

    mfxU16& brcMode = par.mfx.RateControlMethod;

    mfxExtCodingOption2& opt2 = GetExtBufferRef(par);

    if (brcMode != 0)
    {
        if (false == IsBrcModeSupported(brcMode))
        {
            brcMode = 0;
            par.m_initialDelayInKb = 0;
            par.m_bufferSizeInKb = 0;
            par.m_targetKbps = 0;
            par.m_maxKbps = 0;
            par.mfx.BRCParamMultiplier = 0;
            unsupported = true;
        }
        else if (IsBitrateBasedBRC(brcMode))
        {
            if (par.m_numLayers)
            {
                const mfxU32 bitrateForTopTempLayer = par.m_layerParam[par.m_numLayers - 1].targetKbps;
                if (bitrateForTopTempLayer && par.m_targetKbps &&
                    bitrateForTopTempLayer != par.m_targetKbps)
                {
                    par.m_targetKbps = bitrateForTopTempLayer;
                    changed = true;
                }
            }

            if ((brcMode == MFX_RATECONTROL_CBR  &&
                par.m_maxKbps && par.m_maxKbps != par.m_targetKbps) ||
                (brcMode == MFX_RATECONTROL_VBR  &&
                    par.m_maxKbps && par.m_maxKbps < par.m_targetKbps))
            {
                par.m_maxKbps = par.m_targetKbps;
                changed = true;
            }

            if (IsBufferBasedBRC(brcMode) && par.m_bufferSizeInKb &&
                par.m_initialDelayInKb > par.m_bufferSizeInKb)
            {
                par.m_initialDelayInKb = 0;
                unsupported = true;
            }
        }
        else if (brcMode == MFX_RATECONTROL_CQP)
        {
            if (opt2.MBBRC != 0)
            {
                opt2.MBBRC = 0;
                changed = true;
            }

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

            if (par.mfx.BRCParamMultiplier > 1)
            {
                par.mfx.BRCParamMultiplier = 1;
                changed = true;
            }
        }
        else if (brcMode == MFX_RATECONTROL_ICQ)
        {
            if (par.mfx.ICQQuality > MAX_ICQ_QUALITY_INDEX)
            {
                par.mfx.ICQQuality = MAX_ICQ_QUALITY_INDEX;
                changed = true;
            }
        }
    }


    if (par.m_numLayers == 0)
    {
        // VP9 spec allows to store up to 8 reference frames.
        // this VP9 implementation use maximum 3 of 8 so far.
        // we don't need to allocate more if really only 3 are used.
        if (par.mfx.NumRefFrame > DPB_SIZE_REAL)
        {
            par.mfx.NumRefFrame = DPB_SIZE_REAL;
            changed = true;
        }

        // TargetUsage 4 and 7 don't support 3 reference frames
        if (par.mfx.TargetUsage)
        {
            if (par.mfx.TargetUsage != MFX_TARGETUSAGE_BEST_QUALITY &&
                par.mfx.NumRefFrame > 2)
            {
                par.mfx.NumRefFrame = 2;
                changed = true;
            }
        }
    }
    else
    {

        if (par.mfx.NumRefFrame &&
            par.mfx.NumRefFrame < MinRefsForTemporalLayers(par.m_numLayers))
        {
            par.mfx.NumRefFrame = MinRefsForTemporalLayers(par.m_numLayers);
            changed = true;
        }
    }


    mfxExtCodingOptionDDI &opt = GetExtBufferRef(par);
    if (false == CheckTriStateOption(opt.WriteIVFHeaders))
    {
        changed = true;
    }

     mfxExtVP9Param &extPar = GetExtBufferRef(par);

     if (extPar.FrameWidth % 2)
     {
         extPar.FrameWidth = 0;
         unsupported = true;
     }

     if (extPar.FrameHeight % 2)
     {
         extPar.FrameHeight = 0;
         unsupported = true;
     }

     if (extPar.FrameWidth && fi.Width &&
         extPar.FrameWidth > fi.Width)
     {
         extPar.FrameWidth = 0;
         unsupported = true;
     }

     if (extPar.FrameHeight && fi.Height &&
         extPar.FrameHeight > fi.Height)
     {
         extPar.FrameHeight = 0;
         unsupported = true;
     }

     /*if (extPar.FrameWidth &&
         (fi.CropX + fi.CropW > extPar.FrameWidth) ||
         extPar.FrameHeight &&
         (fi.CropY + fi.CropH > extPar.FrameHeight))
     {
         fi.CropX = 0;
         fi.CropW = 0;
         fi.CropY = 0;
         fi.CropH = 0;
         unsupported = true;
     }*/

    // TODO: uncomment when buffer mfxExtVP9CodingOption will be added to API
    /*if (false == CheckTriStateOption(opt.EnableMultipleSegments))
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
    }*/

    // check mfxExtCodingOption2
    if (false == CheckTriStateOption(opt2.MBBRC))
    {
        changed = true;
    }

    if (IsOn(opt2.MBBRC) && caps.AutoSegmentationSupport == 0)
    {
        opt2.MBBRC = MFX_CODINGOPTION_OFF;
        unsupported = true;
    }

    mfxExtOpaqueSurfaceAlloc& opaq = GetExtBufferRef(par);
    if (par.IOPattern &&
        par.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY
        && opaq.In.NumSurface)
    {
        opaq.In.NumSurface = 0;
        changed = true;
    }

    mfxExtVP9Segmentation& seg = GetExtBufferRef(par);

    mfxStatus segSts = CheckSegmentationParam(seg, extPar.FrameWidth, extPar.FrameHeight, caps, 0);
    ConvertStatusToBools(changed, unsupported, segSts);

    if (IsOn(opt2.MBBRC) && seg.NumSegments)
    {
        // explicit segmentation overwrites MBBRC
        opt2.MBBRC = MFX_CODINGOPTION_OFF;
        unsupported = true;
    }

    return GetCheckStatus(changed, unsupported);
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

inline mfxU32 GetDefaultBufferSize(VP9MfxVideoParam const &par)
{
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        const mfxU8 defaultBufferInSec = 2;
        return defaultBufferInSec * ((par.m_targetKbps + 7) / 8);
    }
    else
    {
        const mfxExtVP9Param& extPar = GetExtBufferRef(par);
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010 || par.mfx.FrameInfo.FourCC == MFX_FOURCC_Y410) {
            return (extPar.FrameWidth * extPar.FrameHeight * 3) / 1000; // size of two uncompressed 420 8bit frames in KB
        }
#endif //PRE_SI_TARGET_PLATFORM_GEN11
        return (extPar.FrameWidth * extPar.FrameHeight * 3) / 2 / 1000;  // size of uncompressed 420 8bit frame in KB
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

mfxStatus SetDefaults(
    VP9MfxVideoParam &par,
    ENCODE_CAPS_VP9 const &caps,
    mfxPlatform const & platform)
{
    SetDefault(par.AsyncDepth, GetDefaultAsyncDepth(par));

    // mfxInfoMfx
    SetDefault(par.mfx.TargetUsage, MFX_TARGETUSAGE_BALANCED);
    SetDefault(par.mfx.GopPicSize, DEFAULT_GOP_SIZE);
    SetDefault(par.mfx.GopRefDist, 1);
    SetDefault(par.mfx.NumRefFrame, 1);
    SetDefault(par.mfx.BRCParamMultiplier, 1);
    SetDefault(par.mfx.RateControlMethod, MFX_RATECONTROL_CBR);
    if (IsBufferBasedBRC(par.mfx.RateControlMethod))
    {
        SetDefault(par.m_initialDelayInKb, par.m_bufferSizeInKb / 2);
    }
    if (IsBitrateBasedBRC(par.mfx.RateControlMethod))
    {
        if (par.m_numLayers)
        {
            const mfxU32 bitrateForTopTempLayer = par.m_layerParam[par.m_numLayers - 1].targetKbps;
            SetDefault(par.m_targetKbps, bitrateForTopTempLayer);
        }
        SetDefault(par.m_maxKbps, par.m_targetKbps);
    }

    // mfxInfomfx.FrameInfo
    mfxFrameInfo &fi = par.mfx.FrameInfo;

    mfxExtVP9Param& extPar = GetExtBufferRef(par);
    SetDefault(extPar.FrameWidth, fi.Width);
    SetDefault(extPar.FrameHeight, fi.Height);

    SetDefault(par.m_bufferSizeInKb, GetDefaultBufferSize(par));

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        SetDefault(par.mfx.QPI, 128);
        SetDefault(par.mfx.QPP, par.mfx.QPI + 5);
    }

    mfxExtCodingOption2& opt2 = GetExtBufferRef(par);
    mfxExtVP9Segmentation& seg = GetExtBufferRef(par);
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        caps.AutoSegmentationSupport && !AllMandatorySegMapParams(seg))
    {
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
        if (platform.CodeName == MFX_PLATFORM_ICELAKE)
        {
            // workaround for issues with auto segmentation for ICL platform
            SetDefault(opt2.MBBRC, MFX_CODINGOPTION_OFF);
        }
        else
        {
            SetDefault(opt2.MBBRC, MFX_CODINGOPTION_ON);
        }
#else //PRE_SI_TARGET_PLATFORM_GEN11
        SetDefault(opt2.MBBRC, MFX_CODINGOPTION_ON);
        platform;
#endif //PRE_SI_TARGET_PLATFORM_GEN11
    }
    else
    {
        SetDefault(opt2.MBBRC, MFX_CODINGOPTION_OFF);
    }

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

    // ext buffers
    // TODO: uncomment when buffer mfxExtVP9CodingOption will be added to API
    // mfxExtVP9CodingOption &opt = GetExtBufferRef(par);
    mfxExtCodingOptionDDI &opt = GetExtBufferRef(par);
    SetDefault(opt.WriteIVFHeaders, MFX_CODINGOPTION_ON);

    // check final set of parameters
    mfxStatus sts = CheckParameters(par, caps);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus CheckParametersAndSetDefaults(
    VP9MfxVideoParam &par,
    ENCODE_CAPS_VP9 const &caps,
    mfxPlatform const & platform)
{
    mfxStatus sts = MFX_ERR_NONE;

    // check parameters defined by application
    mfxStatus checkSts = MFX_ERR_NONE;
    checkSts = CheckParameters(par, caps);
    if (checkSts == MFX_ERR_UNSUPPORTED)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // check presence of mandatory parameters
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

    // (3) target bitrate
    if ((IsBitrateBasedBRC(par.mfx.RateControlMethod)
        || par.mfx.RateControlMethod == 0)
        && par.m_numLayers == 0
        && par.m_targetKbps == 0)
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

    // (6) Mandatory segmentation parameters
    mfxExtVP9Segmentation const & seg = GetExtBufferRef(par);
    if (AnyMandatorySegMapParam(seg) && seg.NumSegments != 0 && !AllMandatorySegMapParams(seg))
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    // (7) At least one valid layer for temporal scalability
    if (par.m_tempLayersBufPassed && par.m_numLayers == 0)
    {
        mfxExtVP9TemporalLayers const & tl = GetExtBufferRef(par);
        for (mfxU8 i = 0; i < MAX_NUM_TEMP_LAYERS_SUPPORTED; i++)
        {
            if (tl.Layer[i].FrameRateScale != 0)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
    }

    // (8) Bitrates for all temporal layers in case of bitrate-based BRC
    if (par.m_numLayers &&
        (IsBitrateBasedBRC(par.mfx.RateControlMethod) || par.mfx.RateControlMethod == 0))
    {
        for (mfxU8 i = 0; i < par.m_numLayers; i++)
        {
            if (par.m_layerParam[i].targetKbps == 0)
            {
                return MFX_ERR_INVALID_VIDEO_PARAM;
            }
        }
    }


    // check non-mandatory parameters which require return of WARNING if not set
    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP &&
        (par.mfx.QPI == 0 || par.mfx.QPP == 0))
    {
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (IsBitrateBasedBRC(par.mfx.RateControlMethod) &&
        par.m_numLayers && par.m_targetKbps == 0)
    {
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    // set defaults for parameters not defined by application
    sts = SetDefaults(par, caps, platform);
    MFX_CHECK(sts >= 0, sts);

    // during parameters validation we worked with internal parameter versions
    // now need to update external (API) versions of these parameters
    par.SyncInternalParamToExternal();

    return checkSts;
}

mfxStatus CheckSurface(
    VP9MfxVideoParam const & video,
    mfxFrameSurface1 const & surface,
    mfxU32 initWidth,
    mfxU32 initHeight)
{
    mfxExtOpaqueSurfaceAlloc const & extOpaq = GetExtBufferRef(video);
    bool isOpaq = video.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY && extOpaq.In.NumSurface > 0;

    // check that surface contains valid data
    MFX_CHECK(CheckFourcc(surface.Info.FourCC), MFX_ERR_INVALID_VIDEO_PARAM);

    if (video.m_inMemType == INPUT_SYSTEM_MEMORY)
    {
        MFX_CHECK(surface.Data.Y != 0, MFX_ERR_NULL_PTR);
        MFX_CHECK(surface.Data.U != 0, MFX_ERR_NULL_PTR);
        MFX_CHECK(surface.Data.V != 0, MFX_ERR_NULL_PTR);
    }
    else if (isOpaq == false)
    {
        MFX_CHECK(surface.Data.MemId != 0, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    const mfxExtVP9Param& extPar = GetExtBufferRef(video);
    MFX_CHECK(surface.Info.Width >= extPar.FrameWidth, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(surface.Info.Height >= extPar.FrameHeight, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(surface.Info.Width <= initWidth, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(surface.Info.Height <= initHeight, MFX_ERR_INVALID_VIDEO_PARAM);

    mfxU32 pitch = (surface.Data.PitchHigh << 16) + surface.Data.PitchLow;
    MFX_CHECK(pitch < 0x8000, MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}

mfxStatus CheckAndFixCtrl(
    VP9MfxVideoParam const & video,
    mfxEncodeCtrl & ctrl,
    ENCODE_CAPS_VP9 const & caps)
{
    video;

    mfxStatus checkSts = MFX_ERR_NONE;

    // check mfxEncodeCtrl for correct parameters
    if (ctrl.QP > 255)
    {
        ctrl.QP = 255;
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    if (ctrl.FrameType > MFX_FRAMETYPE_P)
    {
        ctrl.FrameType = MFX_FRAMETYPE_P;
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    const mfxExtVP9Param* pExtParRuntime = GetExtBuffer(ctrl);
    const mfxExtVP9Param& extParInit = GetExtBufferRef(video);

    if (pExtParRuntime)
    {
        if (pExtParRuntime->FrameWidth != extParInit.FrameWidth ||
            pExtParRuntime->FrameHeight != extParInit.FrameHeight)
        {
            // runtime values of FrameWidth/FrameHeight should be same as static values
            // we cannot just remove whole mfxExtVP9Param since it has other parameters
            // so just return error to app to notify
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    mfxExtVP9Segmentation* seg = GetExtBuffer(ctrl);
    if (seg)
    {
        bool removeSegBuffer = false;
        mfxExtCodingOption2 const & opt2 = GetExtBufferRef(video);
        mfxStatus sts = MFX_ERR_NONE;
        if (IsOn(opt2.MBBRC))
        {
            // segmentation ext buffer conflicts with MBBRC. It will be ignored.
            removeSegBuffer = true;
        }
        else if (seg->NumSegments)
        {
            const mfxExtVP9Param& extPar = GetExtBufferRef(video);
            sts = CheckSegmentationParam(*seg, extPar.FrameWidth, extPar.FrameHeight, caps, ctrl.QP);
            if (sts == MFX_ERR_UNSUPPORTED ||
                true == AnyMandatorySegMapParam(*seg) && false == AllMandatorySegMapParams(*seg) ||
                IsOn(opt2.MBBRC))
            {
                // provided segmentation parameters are invalid or lack mandatory information.
                // Ext buffer will be ignored. Report to application about it with warning.
                removeSegBuffer = true;
            }
            else
            {
                if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
                {
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }

                mfxExtVP9Segmentation const & segInit = GetExtBufferRef(video);
                CombineInitAndRuntimeSegmentBuffers(*seg, segInit);
                if (false == AllMandatorySegMapParams(*seg))
                {
                    // neither runtime ext buffer not init don't contain valid segmentation map.
                    // Ext buffer will be ignored. Report to application about it with warning.
                    removeSegBuffer = true;
                }
            }
        }

        if (removeSegBuffer)
        {
            // remove ext buffer from mfxEncodeCtrl
            // and report to application about it with warning.
            checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            sts = RemoveExtBuffer(ctrl, MFX_EXTBUFF_VP9_SEGMENTATION);
            MFX_CHECK(sts >= MFX_ERR_NONE, checkSts);
        }
    }

    return checkSts;
}

mfxStatus CheckBitstream(
    VP9MfxVideoParam const & video,
    mfxBitstream        * bs)
{
    mfxStatus checkSts = MFX_ERR_NONE;
    MFX_CHECK_NULL_PTR1(bs);
    MFX_CHECK_NULL_PTR1(bs->Data);

    // check bitstream buffer for enough space
    MFX_CHECK(bs->MaxLength > 0, MFX_ERR_NOT_ENOUGH_BUFFER);
    MFX_CHECK(bs->DataOffset < bs->MaxLength, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(bs->MaxLength > bs->DataOffset + bs->DataLength, MFX_ERR_NOT_ENOUGH_BUFFER);
    MFX_CHECK(bs->DataOffset + bs->DataLength + video.m_bufferSizeInKb * 1000 <= bs->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);

    return checkSts;
}
} //namespace MfxHwVP9Encode

#endif // PRE_SI_TARGET_PLATFORM_GEN10
