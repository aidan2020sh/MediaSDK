/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2014 Intel Corporation. All Rights Reserved.
//
//
//          H264 encoder common
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW)

#include <limits>
#include <limits.h>

#if defined (MFX_VA_LINUX)
#include <va/va_enc_h264.h>
#endif

#include "ippi.h"
#include "libmfx_core_interface.h"
#include "libmfx_core_factory.h"

#include "mfx_h264_encode_hw_utils.h"

#include "mfx_brc_common.h"
#include "mfx_h264_enc_common.h"
#include "umc_h264_brc.h"
#include "umc_h264_core_enc.h"
#ifdef MFX_ENABLE_H264_VIDEO_FEI_ENCPAK
#include "mfxfei.h"
#endif


using namespace MfxHwH264Encode;

const mfxU32 DEFAULT_CPB_IN_SECONDS = 2000;          //  BufferSizeInBits = DEFAULT_CPB_IN_SECONDS * MaxKbps
const mfxU32 MAX_BITRATE_RATIO = mfxU32(1.5 * 1000); //  MaxBps = MAX_BITRATE_RATIO * TargetKbps;

const mfxU32 MIN_LOOKAHEAD_DEPTH = 10;
const mfxU32 MAX_LOOKAHEAD_DEPTH = 100;

namespace
{
    static const mfxU8 EXTENDED_SAR = 0xff;

    static const mfxU32 MAX_H_MV = 2048;

    static const struct { mfxU16 w, h; } TABLE_E1[] =
    {
        {   0,   0 }, {   1,   1 }, {  12,  11 }, {  10,  11 }, {  16,  11 },
        {  40,  33 }, {  24,  11 }, {  20,  11 }, {  32,  11 }, {  80,  33 },
        {  18,  11 }, {  15,  11 }, {  64,  33 }, { 160,  99 }, {   4,   3 },
        {   3,   2 }, {   2,   1 }
    };

    const mfxU16 AVBR_ACCURACY_MIN = 1;
    const mfxU16 AVBR_ACCURACY_MAX = 65535;

    const mfxU16 AVBR_CONVERGENCE_MIN = 1;
    const mfxU16 AVBR_CONVERGENCE_MAX = 65535;

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

    bool CheckTriStateOptionForOff(mfxU16 & opt)
    {
        if (opt !=  MFX_CODINGOPTION_OFF)
        {
            opt = MFX_CODINGOPTION_OFF;
            return false;
        }

        return true;
    }

    inline void SetDefaultOn(mfxU16 & opt)
    {
        if (opt ==  MFX_CODINGOPTION_UNKNOWN)
        {
            opt = MFX_CODINGOPTION_ON;
        }
    }

    inline void SetDefaultOff(mfxU16 & opt)
    {
        if (opt ==  MFX_CODINGOPTION_UNKNOWN)
        {
            opt = MFX_CODINGOPTION_OFF;
        }
    }

    template <class T, class U>
    bool CheckFlag(T & opt, U deflt)
    {
        if (opt > 1)
        {
            opt = static_cast<T>(deflt);
            return false;
        }

        return true;
    }

    template <class T, class U>
    bool CheckRange(T & opt, U min, U max)
    {
        if (opt < min)
        {
            opt = static_cast<T>(min);
            return false;
        }

        if (opt > max)
        {
            opt = static_cast<T>(max);
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

    struct FunctionQuery {};
    struct FunctionInit {};

    template <class T, class U>
    mfxU32 CheckAgreement(FunctionQuery, T & lowerPriorityValue, U higherPriorityValue)
    {
        if (lowerPriorityValue != 0 &&
            lowerPriorityValue != higherPriorityValue)
            return lowerPriorityValue = higherPriorityValue, 1;
        else
            return 0;
    }

    template <class T, class U>
    mfxU32 CheckAgreement(FunctionInit, T & lowerPriorityValue, U higherPriorityValue)
    {
        if (lowerPriorityValue == 0)
            return lowerPriorityValue = higherPriorityValue, 0; // assignment, not a correction
        else if (lowerPriorityValue != higherPriorityValue)
            return lowerPriorityValue = higherPriorityValue, 1; // correction
        else
            return 0; // already equal
    }

    bool CheckMbAlignment(mfxU32 & opt)
    {
        if (opt & 0x0f)
        {
            opt = opt & 0xfffffff0;
            return false;
        }

        return true;
    }

    bool IsValidCodingLevel(mfxU16 level)
    {
        return
            level == MFX_LEVEL_AVC_1  ||
            level == MFX_LEVEL_AVC_11 ||
            level == MFX_LEVEL_AVC_12 ||
            level == MFX_LEVEL_AVC_13 ||
            level == MFX_LEVEL_AVC_1b ||
            level == MFX_LEVEL_AVC_2  ||
            level == MFX_LEVEL_AVC_21 ||
            level == MFX_LEVEL_AVC_22 ||
            level == MFX_LEVEL_AVC_3  ||
            level == MFX_LEVEL_AVC_31 ||
            level == MFX_LEVEL_AVC_32 ||
            level == MFX_LEVEL_AVC_4  ||
            level == MFX_LEVEL_AVC_41 ||
            level == MFX_LEVEL_AVC_42 ||
            level == MFX_LEVEL_AVC_5  ||
            level == MFX_LEVEL_AVC_51 ||
            level == MFX_LEVEL_AVC_52;
    }

    bool IsValidCodingProfile(mfxU16 profile)
    {
        return
            IsAvcBaseProfile(profile)       ||
            IsAvcHighProfile(profile)       ||
            IsMvcProfile(profile) && (profile != MFX_PROFILE_AVC_MULTIVIEW_HIGH) || // Multiview high isn't supported by MSDK
            profile == MFX_PROFILE_AVC_MAIN ||
            IsSvcProfile(profile);
    }

    inline mfxU16 GetMaxSupportedLevel()
    {
        return MFX_LEVEL_AVC_52;
    }

    mfxU16 GetNextProfile(mfxU16 profile)
    {
        switch (profile)
        {
        case MFX_PROFILE_AVC_BASELINE:    return MFX_PROFILE_AVC_MAIN;
        case MFX_PROFILE_AVC_MAIN:        return MFX_PROFILE_AVC_HIGH;
        case MFX_PROFILE_AVC_HIGH:        return MFX_PROFILE_UNKNOWN;
        case MFX_PROFILE_AVC_STEREO_HIGH: return MFX_PROFILE_UNKNOWN;
        default: assert(!"bad profile");
            return MFX_PROFILE_UNKNOWN;
        }
    }

    mfxU16 GetLevelLimitByDpbSize(mfxVideoParam const & par)
    {
        mfxU32 dpbSize = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 3 / 2;

        dpbSize *= par.mfx.NumRefFrame;
        assert(dpbSize > 0);

        if (dpbSize <=   152064) return MFX_LEVEL_AVC_1;
        if (dpbSize <=   345600) return MFX_LEVEL_AVC_11;
        if (dpbSize <=   912384) return MFX_LEVEL_AVC_12;
        if (dpbSize <=  1824768) return MFX_LEVEL_AVC_21;
        if (dpbSize <=  3110400) return MFX_LEVEL_AVC_22;
        if (dpbSize <=  6912000) return MFX_LEVEL_AVC_31;
        if (dpbSize <=  7864320) return MFX_LEVEL_AVC_32;
        if (dpbSize <= 12582912) return MFX_LEVEL_AVC_4;
        if (dpbSize <= 13369344) return MFX_LEVEL_AVC_42;
        if (dpbSize <= 42393600) return MFX_LEVEL_AVC_5;
        if (dpbSize <= 70778880) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByFrameSize(mfxVideoParam const & par)
    {
        mfxU32 numMb = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;

        if (numMb <=    99) return MFX_LEVEL_AVC_1;
        if (numMb <=   396) return MFX_LEVEL_AVC_11;
        if (numMb <=   792) return MFX_LEVEL_AVC_21;
        if (numMb <=  1620) return MFX_LEVEL_AVC_22;
        if (numMb <=  3600) return MFX_LEVEL_AVC_31;
        if (numMb <=  5120) return MFX_LEVEL_AVC_32;
        if (numMb <=  8192) return MFX_LEVEL_AVC_4;
        if (numMb <=  8704) return MFX_LEVEL_AVC_42;
        if (numMb <= 22080) return MFX_LEVEL_AVC_5;
        if (numMb <= 36864) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByMbps(mfxVideoParam const & par)
    {
        mfxU32 numMb = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;
        mfxF64 fR = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
        mfxF64 mbps = numMb * fR;

        if (mbps <=   1485)  return MFX_LEVEL_AVC_1;
        if (mbps <=   3000)  return MFX_LEVEL_AVC_11;
        if (mbps <=   6000)  return MFX_LEVEL_AVC_12;
        if (mbps <=  11880)  return MFX_LEVEL_AVC_13;
        if (mbps <=  19800)  return MFX_LEVEL_AVC_21;
        if (mbps <=  20250)  return MFX_LEVEL_AVC_22;
        if (mbps <=  40500)  return MFX_LEVEL_AVC_3;
        if (mbps <= 108000)  return MFX_LEVEL_AVC_31;
        if (mbps <= 216000)  return MFX_LEVEL_AVC_32;
        if (mbps <= 245760)  return MFX_LEVEL_AVC_4;
        if (mbps <= 522240)  return MFX_LEVEL_AVC_42;
        if (mbps <= 589824)  return MFX_LEVEL_AVC_5;
        if (mbps <= 983040)  return MFX_LEVEL_AVC_51;
        if (mbps <= 2073600) return MFX_LEVEL_AVC_52;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByMaxBitrate(mfxU16 profile, mfxU32 kbps)
    {
        mfxU32 brFactor = IsAvcHighProfile(profile) ? 1500 : 1200;
        mfxU32 br = 1000 * kbps;

        if (br <=     64 * brFactor) return MFX_LEVEL_AVC_1;
        if (br <=    128 * brFactor) return MFX_LEVEL_AVC_1b;
        if (br <=    192 * brFactor) return MFX_LEVEL_AVC_11;
        if (br <=    384 * brFactor) return MFX_LEVEL_AVC_12;
        if (br <=    768 * brFactor) return MFX_LEVEL_AVC_13;
        if (br <=   2000 * brFactor) return MFX_LEVEL_AVC_2;
        if (br <=   4000 * brFactor) return MFX_LEVEL_AVC_21;
        if (br <=  10000 * brFactor) return MFX_LEVEL_AVC_3;
        if (br <=  14000 * brFactor) return MFX_LEVEL_AVC_31;
        if (br <=  20000 * brFactor) return MFX_LEVEL_AVC_32;
        if (br <=  50000 * brFactor) return MFX_LEVEL_AVC_41;
        if (br <= 135000 * brFactor) return MFX_LEVEL_AVC_5;
        if (br <= 240000 * brFactor) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    mfxU16 GetLevelLimitByBufferSize(mfxU16 profile, mfxU32 bufferSizeInKb)
    {
        mfxU32 brFactor = IsAvcHighProfile(profile) ? 1500 : 1200;
        mfxU32 bufSize = 8000 * bufferSizeInKb;

        if (bufSize <=    175 * brFactor) return MFX_LEVEL_AVC_1;
        if (bufSize <=    350 * brFactor) return MFX_LEVEL_AVC_1b;
        if (bufSize <=    500 * brFactor) return MFX_LEVEL_AVC_11;
        if (bufSize <=   1000 * brFactor) return MFX_LEVEL_AVC_12;
        if (bufSize <=   2000 * brFactor) return MFX_LEVEL_AVC_13;
        if (bufSize <=   4000 * brFactor) return MFX_LEVEL_AVC_21;
        if (bufSize <=  10000 * brFactor) return MFX_LEVEL_AVC_3;
        if (bufSize <=  14000 * brFactor) return MFX_LEVEL_AVC_31;
        if (bufSize <=  20000 * brFactor) return MFX_LEVEL_AVC_32;
        if (bufSize <=  25000 * brFactor) return MFX_LEVEL_AVC_4;
        if (bufSize <=  62500 * brFactor) return MFX_LEVEL_AVC_41;
        if (bufSize <= 135000 * brFactor) return MFX_LEVEL_AVC_5;
        if (bufSize <= 240000 * brFactor) return MFX_LEVEL_AVC_51;

        return MFX_LEVEL_UNKNOWN;
    }

    // calculate possible minimum level for encoding of input stream
    mfxU16 GetMinLevelForAllParameters(MfxVideoParam const & par)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(par);
        assert(extSps != 0);

        if (par.mfx.FrameInfo.Width         == 0 ||
            par.mfx.FrameInfo.Height        == 0)
        {
            // input information isn't enough to determine required level
            return 0;
        }

        mfxU16 maxSupportedLevel = GetMaxSupportedLevel();
        mfxU16 level = GetLevelLimitByFrameSize(par);

        if (level == 0 || level == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (extSps->vui.flags.timingInfoPresent == 0 ||
            par.mfx.FrameInfo.FrameRateExtN == 0 ||
            par.mfx.FrameInfo.FrameRateExtD == 0)
        {
            // no information about frame rate
            return level;
        }

        mfxU16 levelMbps = GetLevelLimitByMbps(par);

        if (levelMbps == 0 || levelMbps == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelMbps > level)
            level = levelMbps;

        if (par.mfx.NumRefFrame      != 0 &&
            par.mfx.FrameInfo.Width  != 0 &&
            par.mfx.FrameInfo.Height != 0)
        {
            mfxU16 levelDpbs = GetLevelLimitByDpbSize(par);

            if (levelDpbs == 0 || levelDpbs == maxSupportedLevel)
            {
                // level is already maximum possible, return it
                return maxSupportedLevel;
            }


            if (levelDpbs > level)
                level = levelDpbs;
        }

        mfxU16 profile = par.mfx.CodecProfile;
        mfxU32 kbps = par.calcParam.targetKbps;
        mfxU16 levelBr = GetLevelLimitByMaxBitrate(profile, kbps);

        if (levelBr == 0 || levelBr == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelBr > level)
            level = levelBr;

        profile = par.mfx.CodecProfile;
        mfxU32 cpb = par.calcParam.bufferSizeInKB;
        mfxU16 levelCPM = GetLevelLimitByBufferSize(profile, cpb);
        if (levelCPM == 0 || levelCPM == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelCPM > level)
            level = levelCPM;

        return level;
    }
    // calculate minimum level required for encoding of input stream (with given resolution and frame rate)
    // input stream can't be encoded with lower level regardless of any encoding parameters
    mfxU16 GetMinLevelForResolutionAndFramerate(MfxVideoParam const & par)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(par);
        assert(extSps != 0);

        if (par.mfx.FrameInfo.Width         == 0 ||
            par.mfx.FrameInfo.Height        == 0)
        {
            // input information isn't enough to determine required level
            return 0;
        }

        mfxU16 maxSupportedLevel = GetMaxSupportedLevel();
        mfxU16 level = GetLevelLimitByFrameSize(par);

        if (level == 0 || level == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (extSps->vui.flags.timingInfoPresent == 0 ||
            par.mfx.FrameInfo.FrameRateExtN == 0 ||
            par.mfx.FrameInfo.FrameRateExtD == 0)
        {
            // no information about frame rate
            return level;
        }

        mfxU16 levelMbps = GetLevelLimitByMbps(par);

        if (levelMbps == 0 || levelMbps == maxSupportedLevel)
        {
            // level is already maximum possible, return it
            return maxSupportedLevel;
        }

        if (levelMbps > level)
            level = levelMbps;

        return level;
    }

    mfxU16 GetMaxNumRefFrame(mfxU16 level, mfxU16 width, mfxU16 height)
    {
        mfxU32 maxDpbSize = 0;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : maxDpbSize =   152064; break;
        case MFX_LEVEL_AVC_1b: maxDpbSize =   152064; break;
        case MFX_LEVEL_AVC_11: maxDpbSize =   345600; break;
        case MFX_LEVEL_AVC_12: maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_13: maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_2 : maxDpbSize =   912384; break;
        case MFX_LEVEL_AVC_21: maxDpbSize =  1824768; break;
        case MFX_LEVEL_AVC_22: maxDpbSize =  3110400; break;
        case MFX_LEVEL_AVC_3 : maxDpbSize =  3110400; break;
        case MFX_LEVEL_AVC_31: maxDpbSize =  6912000; break;
        case MFX_LEVEL_AVC_32: maxDpbSize =  7864320; break;
        case MFX_LEVEL_AVC_4 : maxDpbSize = 12582912; break;
        case MFX_LEVEL_AVC_41: maxDpbSize = 12582912; break;
        case MFX_LEVEL_AVC_42: maxDpbSize = 13369344; break;
        case MFX_LEVEL_AVC_5 : maxDpbSize = 42393600; break;
        case MFX_LEVEL_AVC_51: maxDpbSize = 70778880; break;
        case MFX_LEVEL_AVC_52: maxDpbSize = 70778880; break;
        default: assert(!"bad CodecLevel");
        }

        mfxU32 frameSize = width * height * 3 / 2;
        return mfxU16(IPP_MAX(1, IPP_MIN(maxDpbSize / frameSize, 16)));
    }

    mfxU16 GetMaxNumRefFrame(mfxVideoParam const & par)
    {
        return GetMaxNumRefFrame(par.mfx.CodecLevel, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height);
    }

    mfxU32 GetMaxBitrate(mfxVideoParam const & par)
    {
        mfxU32 brFactor = IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return     64 * brFactor;
        case MFX_LEVEL_AVC_1b: return    128 * brFactor;
        case MFX_LEVEL_AVC_11: return    192 * brFactor;
        case MFX_LEVEL_AVC_12: return    384 * brFactor;
        case MFX_LEVEL_AVC_13: return    768 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  20000 * brFactor;
        case MFX_LEVEL_AVC_41: return  50000 * brFactor;
        case MFX_LEVEL_AVC_42: return  50000 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxPerViewBitrate(MfxVideoParam const & par)
    {
        mfxU32 brFactor = IsMvcProfile(par.mfx.CodecProfile) ? 1500 :
            IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = IsMvcProfile(par.mfx.CodecProfile) ? par.calcParam.mvcPerViewPar.codecLevel : par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return     64 * brFactor;
        case MFX_LEVEL_AVC_1b: return    128 * brFactor;
        case MFX_LEVEL_AVC_11: return    192 * brFactor;
        case MFX_LEVEL_AVC_12: return    384 * brFactor;
        case MFX_LEVEL_AVC_13: return    768 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  20000 * brFactor;
        case MFX_LEVEL_AVC_41: return  50000 * brFactor;
        case MFX_LEVEL_AVC_42: return  50000 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxBufferSize(mfxVideoParam const & par)
    {
        mfxU32 brFactor = IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return    175 * brFactor;
        case MFX_LEVEL_AVC_1b: return    350 * brFactor;
        case MFX_LEVEL_AVC_11: return    500 * brFactor;
        case MFX_LEVEL_AVC_12: return   1000 * brFactor;
        case MFX_LEVEL_AVC_13: return   2000 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  25000 * brFactor;
        case MFX_LEVEL_AVC_41: return  62500 * brFactor;
        case MFX_LEVEL_AVC_42: return  62500 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMaxPerViewBufferSize(MfxVideoParam const & par)
    {
        mfxU32 brFactor = IsMvcProfile(par.mfx.CodecProfile) ? 1500 :
            IsAvcHighProfile(par.mfx.CodecProfile) ? 1500 : 1200;

        mfxU16 level = IsMvcProfile(par.mfx.CodecProfile) ? par.calcParam.mvcPerViewPar.codecLevel : par.mfx.CodecLevel;
        if (level == MFX_LEVEL_UNKNOWN)
            level = MFX_LEVEL_AVC_52;

        switch (level)
        {
        case MFX_LEVEL_AVC_1 : return    175 * brFactor;
        case MFX_LEVEL_AVC_1b: return    350 * brFactor;
        case MFX_LEVEL_AVC_11: return    500 * brFactor;
        case MFX_LEVEL_AVC_12: return   1000 * brFactor;
        case MFX_LEVEL_AVC_13: return   2000 * brFactor;
        case MFX_LEVEL_AVC_2 : return   2000 * brFactor;
        case MFX_LEVEL_AVC_21: return   4000 * brFactor;
        case MFX_LEVEL_AVC_22: return   4000 * brFactor;
        case MFX_LEVEL_AVC_3 : return  10000 * brFactor;
        case MFX_LEVEL_AVC_31: return  14000 * brFactor;
        case MFX_LEVEL_AVC_32: return  20000 * brFactor;
        case MFX_LEVEL_AVC_4 : return  25000 * brFactor;
        case MFX_LEVEL_AVC_41: return  62500 * brFactor;
        case MFX_LEVEL_AVC_42: return  62500 * brFactor;
        case MFX_LEVEL_AVC_5 : return 135000 * brFactor;
        case MFX_LEVEL_AVC_51: return 240000 * brFactor;
        case MFX_LEVEL_AVC_52: return 240000 * brFactor;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }


    mfxU32 GetMaxMbps(mfxVideoParam const & par)
    {
        switch (par.mfx.CodecLevel)
        {
        case MFX_LEVEL_AVC_1 :
        case MFX_LEVEL_AVC_1b: return   1485;
        case MFX_LEVEL_AVC_11: return   3000;
        case MFX_LEVEL_AVC_12: return   6000;
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2 : return  11800;
        case MFX_LEVEL_AVC_21: return  19800;
        case MFX_LEVEL_AVC_22: return  20250;
        case MFX_LEVEL_AVC_3 : return  40500;
        case MFX_LEVEL_AVC_31: return 108000;
        case MFX_LEVEL_AVC_32: return 216000;
        case MFX_LEVEL_AVC_4 :
        case MFX_LEVEL_AVC_41: return 245760;
        case MFX_LEVEL_AVC_42: return 522240;
        case MFX_LEVEL_AVC_5 : return 589824;
        case MFX_LEVEL_AVC_51: return 983040;
        case MFX_LEVEL_AVC_52: return 2073600;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU32 GetMinCr(mfxU32 level)
    {
        return level >= MFX_LEVEL_AVC_31 && level <= MFX_LEVEL_AVC_42 ? 4 : 2; // AVCHD spec requires MinCR = 4 for levels  4.1, 4.2
    }

    mfxU32 GetFirstMaxFrameSize(mfxVideoParam const & par)
    {
        mfxU32 picSizeInMbs = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256;
        return 384 * IPP_MAX(picSizeInMbs, GetMaxMbps(par) / 172) / GetMinCr(par.mfx.CodecLevel);
    }

    mfxU32 GetMaxFrameSize(mfxVideoParam const & par)
    {
        mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
        return mfxU32(384 * GetMaxMbps(par) / frameRate / GetMinCr(par.mfx.CodecLevel));
    }

    mfxU32 GetMaxVmv(mfxU32 level)
    {
        switch (level)
        {
        case MFX_LEVEL_AVC_1 :
        case MFX_LEVEL_AVC_1b: return  64;
        case MFX_LEVEL_AVC_11:
        case MFX_LEVEL_AVC_12:
        case MFX_LEVEL_AVC_13:
        case MFX_LEVEL_AVC_2 : return 128;
        case MFX_LEVEL_AVC_21:
        case MFX_LEVEL_AVC_22:
        case MFX_LEVEL_AVC_3 : return 256;
        case MFX_LEVEL_AVC_31:
        case MFX_LEVEL_AVC_32:
        case MFX_LEVEL_AVC_4 :
        case MFX_LEVEL_AVC_41:
        case MFX_LEVEL_AVC_42:
        case MFX_LEVEL_AVC_5 :
        case MFX_LEVEL_AVC_51:
        case MFX_LEVEL_AVC_52: return 512;
        default: assert(!"bad CodecLevel"); return 0;
        }
    }

    mfxU16 GetDefaultAsyncDepth(MfxVideoParam const & par)
    {
//        mfxExtCodingOption2 const * extOpt2 = GetExtBuffer(par);

        if (par.mfx.EncodedOrder)
            return 1;

        //if (IsOn(extOpt2->ExtBRC))
        //    return 1;

        mfxU32 picSize = par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height;
        if (picSize < 200000)
            return 6; // CIF
        else if (picSize < 500000)
            return 5; // SD
        else if (picSize < 900000)
            return 4; // between SD and HD
        else
            return 3; // HD
    }

    mfxU8 GetDefaultPicOrderCount(MfxVideoParam const & par)
    {
        if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
            return 0;
        if (par.mfx.GopRefDist > 1)
            return 0;

        if (par.calcParam.numTemporalLayer > 1)
        {
            if (par.mfx.NumRefFrame < par.calcParam.scale[par.calcParam.numTemporalLayer - 2])
                return 0; // more than 1 consecutive non-reference frames

            if (par.calcParam.scale[par.calcParam.numTemporalLayer - 1] /
                par.calcParam.scale[par.calcParam.numTemporalLayer - 2] > 2)
                return 0; // more than 1 consecutive non-reference frames
        }

        return 2;
    }

    mfxU8 GetDefaultLog2MaxPicOrdCntMinus4(MfxVideoParam const & par)
    {
        mfxU32 numReorderFrames = GetNumReorderFrames(par);
        mfxU32 maxPocDiff = (numReorderFrames * par.mfx.GopRefDist + 1) * 2;

        if (par.calcParam.numTemporalLayer > 0 || par.calcParam.lyncMode)
        {
            mfxU32 maxScale = par.calcParam.scale[par.calcParam.numTemporalLayer - 1];
            // for Lync number of temporal layers should be changed dynamically w/o IDR insertion
            // to assure this first SPS in bitstream should contain maximum possible log2_max_frame_num_minus4
            if (par.calcParam.lyncMode)
                maxScale = 8; // 8 is maximum scale for 4 temporal layers

            maxPocDiff = IPP_MAX(maxPocDiff, 2 * maxScale);
        }

        mfxU32 log2MaxPoc = CeilLog2(2 * maxPocDiff - 1);
        return mfxU8(IPP_MAX(log2MaxPoc, 4) - 4);
    }

    mfxU16 GetDefaultGopRefDist(mfxU32 targetUsage, eMFXHWType platform)
    {
        if(platform == MFX_HW_VLV){
            const mfxU16 DEFAUILT_GOP_REF_DIST[8] = { 3, 3, 2, 2, 1, 1, 1 };
            assert(targetUsage > 0 && targetUsage < 8);
            return DEFAUILT_GOP_REF_DIST[targetUsage - 1];
        }else{
            return 3;
        }
        /*

        */
    }

    mfxU16 GetDefaultNumRefFrames(mfxU32 targetUsage)
    {
        mfxU16 const DEFAULT_BY_TU[] = { 0, 3, 3, 3, 2, 1, 1, 1 };
        return DEFAULT_BY_TU[targetUsage];
    }

    mfxU16 GetDefaultMaxNumRefActivePL0(mfxU32 targetUsage, eMFXHWType platform, bool isLowPower)
    {
        if (platform == MFX_HW_IVB || platform == MFX_HW_VLV)
            return 1;
        else if (platform == MFX_HW_SOFIA)
        {
            mfxU16 const DEFAULT_BY_TU[] = { 0, 2, 2, 2, 2, 2, 1, 1 };
            return DEFAULT_BY_TU[targetUsage];
        }
        else if (!isLowPower)
        {
            mfxU16 const DEFAULT_BY_TU[] = { 0, 8, 6, 4, 3, 2, 1, 1 };
            return DEFAULT_BY_TU[targetUsage];
        }
        else
        {
            mfxU16 const DEFAULT_BY_TU[] = { 0, 3, 3, 2, 2, 2, 1, 1 };
            return DEFAULT_BY_TU[targetUsage];
        }
    }

    mfxU16 GetDefaultMaxNumRefActiveBL0(mfxU32 targetUsage, eMFXHWType platform)
    {
        if (platform == MFX_HW_IVB || platform == MFX_HW_VLV)
            return 1;
        else
        {
            mfxU16 const DEFAULT_BY_TU[] = { 0, 4, 4, 3, 2, 2, 1, 1 };
            return DEFAULT_BY_TU[targetUsage];
        }
    }

    mfxU16 GetDefaultMaxNumRefActiveBL1(mfxU32 targetUsage, eMFXHWType platform)
    {
        platform; targetUsage;
        mfxU16 const DEFAULT_BY_TU[] = { 2, 2, 1, 1, 1, 1, 1, 1 };
        return DEFAULT_BY_TU[targetUsage];
    }

    mfxU16 GetDefaultIntraPredBlockSize(
        MfxVideoParam const & par,
        eMFXHWType            platform)
    {
        mfxU8 minTUForTransform8x8 = (platform <= MFX_HW_IVB || platform == MFX_HW_VLV) ? 4 : 7;
        return (IsAvcBaseProfile(par.mfx.CodecProfile)       ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_MAIN ||
            par.mfx.TargetUsage > minTUForTransform8x8)
                ? mfxU16(MFX_BLOCKSIZE_MIN_16X16)
                : mfxU16(MFX_BLOCKSIZE_MIN_4X4);
    }

    mfxU32 GetCpbSizeValue(mfxU32 kbyte, mfxU32 scale)
    {
        return (8000 * kbyte) >> (4 + scale);
    }

    mfxU32 GetUncompressedSizeInKb(mfxVideoParam const & par)
    {
        mfxU64 mvcMultiplier = 1;
        if (IsMvcProfile(par.mfx.CodecProfile))
        {
            mfxExtMVCSeqDesc * extMvc = GetExtBuffer(par);
            mfxExtCodingOption * extOpt = GetExtBuffer(par);
            if (extOpt->ViewOutput != MFX_CODINGOPTION_ON) // in case of ViewOutput bitstream should contain one view (not all views)
                mvcMultiplier = extMvc->NumView ? extMvc->NumView : 1;
        }

        return mfxU32(IPP_MIN(UINT_MAX, par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * mvcMultiplier * 3u / 2000u));
    }


    mfxU8 GetDefaultLog2MaxFrameNumMinux4(mfxVideoParam const & video)
    {
        // frame_num should be able to distinguish NumRefFrame reference frames + current frame
        return mfxU8(IPP_MAX(4, CeilLog2(video.mfx.NumRefFrame + 1)) - 4);
    }

    mfxU32 CheckAgreementOfFrameRate(
        FunctionQuery,
        mfxU32 & frameRateExtN,
        mfxU32 & frameRateExtD,
        mfxU32   timeScale,
        mfxU32   numUnitsInTick)
    {
        if (frameRateExtN != 0 &&
            frameRateExtD != 0 &&
            mfxU64(frameRateExtN) * numUnitsInTick * 2 != mfxU64(frameRateExtD) * timeScale)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 1;
        }

        return 0;
    }

    mfxU32 CheckAgreementOfFrameRate(
        FunctionInit,
        mfxU32 & frameRateExtN,
        mfxU32 & frameRateExtD,
        mfxU32   timeScale,
        mfxU32   numUnitsInTick)
    {
        if (frameRateExtN == 0 || frameRateExtD == 0)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 0; // initialized, no error
        }
        else if (mfxU64(frameRateExtN) * numUnitsInTick * 2 != mfxU64(frameRateExtD) * timeScale)
        {
            frameRateExtN = timeScale;
            frameRateExtD = numUnitsInTick * 2;
            return 1; // modified
        }
        else
        {
            return 0; // equal, no error
        }
    }

    inline mfxU16 FlagToTriState(mfxU16 flag)
    {
        return flag ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
    }

    template <class TFunc>
    bool CheckAgreementOfSequenceLevelParameters(MfxVideoParam & par, mfxExtSpsHeader const & sps)
    {
        mfxU32 changed = 0;

        mfxFrameInfo & fi = par.mfx.FrameInfo;
        mfxExtCodingOption * extOpt   = GetExtBuffer(par);
        mfxExtCodingOption2 * extOpt2 = GetExtBuffer(par);
        mfxExtCodingOption3 * extOpt3 = GetExtBuffer(par);


        TFunc f;

        changed |= CheckAgreement(f, par.mfx.CodecProfile, sps.profileIdc);
        changed |= CheckAgreement(f, par.mfx.CodecLevel,   sps.levelIdc);
        changed |= CheckAgreement(f, par.mfx.NumRefFrame,  sps.maxNumRefFrames);
        changed |= CheckAgreement(f, fi.ChromaFormat,      sps.chromaFormatIdc);

        mfxU16 const cropUnitX = CROP_UNIT_X[fi.ChromaFormat];
        mfxU16 const cropUnitY = CROP_UNIT_Y[fi.ChromaFormat] * (2 - sps.frameMbsOnlyFlag);

        changed |= CheckAgreement(f, fi.Width,     mfxU16(16 * (sps.picWidthInMbsMinus1 + 1)));
        changed |= CheckAgreement(f, fi.Height,    mfxU16(16 * (sps.picHeightInMapUnitsMinus1 + 1) * (2 - sps.frameMbsOnlyFlag)));
        changed |= CheckAgreement(f, fi.PicStruct, mfxU16(sps.frameMbsOnlyFlag ? mfxU16(MFX_PICSTRUCT_PROGRESSIVE) : fi.PicStruct));
        changed |= CheckAgreement(f, fi.CropX,     mfxU16(sps.frameCropLeftOffset * cropUnitX));
        changed |= CheckAgreement(f, fi.CropY,     mfxU16(sps.frameCropTopOffset * cropUnitY));
        changed |= CheckAgreement(f, fi.CropW,     mfxU16(fi.Width  - (sps.frameCropLeftOffset + sps.frameCropRightOffset) * cropUnitX));
        changed |= CheckAgreement(f, fi.CropH,     mfxU16(fi.Height - (sps.frameCropTopOffset  + sps.frameCropBottomOffset) * cropUnitY));

        mfxU16 disableVui = FlagToTriState(!sps.vuiParametersPresentFlag);
        changed |= CheckAgreement(f, extOpt2->DisableVUI, disableVui);

        mfxU16 aspectRatioPresent   = FlagToTriState(sps.vui.flags.aspectRatioInfoPresent);
        mfxU16 timingInfoPresent    = FlagToTriState(sps.vui.flags.timingInfoPresent);
        mfxU16 overscanInfoPresent  = FlagToTriState(sps.vui.flags.overscanInfoPresent);
        mfxU16 bitstreamRestriction = FlagToTriState(sps.vui.flags.bitstreamRestriction);

        changed |= CheckAgreement(f, extOpt3->AspectRatioInfoPresent, aspectRatioPresent);
        changed |= CheckAgreement(f, extOpt3->TimingInfoPresent,      timingInfoPresent);
        changed |= CheckAgreement(f, extOpt3->OverscanInfoPresent,    overscanInfoPresent);
        changed |= CheckAgreement(f, extOpt3->BitstreamRestriction,   bitstreamRestriction);

        if (sps.vuiParametersPresentFlag)
        {
            if (sps.vui.flags.timingInfoPresent)
            {
                mfxU16 fixedFrameRate = FlagToTriState(sps.vui.flags.fixedFrameRate);
                changed |= CheckAgreement(f, extOpt2->FixedFrameRate, fixedFrameRate);
                changed |= CheckAgreementOfFrameRate(f, fi.FrameRateExtN, fi.FrameRateExtD, sps.vui.timeScale, sps.vui.numUnitsInTick);
            }

            if (sps.vui.flags.aspectRatioInfoPresent)
            {
                AspectRatioConverter arConv(sps.vui.aspectRatioIdc, sps.vui.sarWidth, sps.vui.sarHeight);
                changed |= CheckAgreement(f, fi.AspectRatioW, arConv.GetSarWidth());
                changed |= CheckAgreement(f, fi.AspectRatioH, arConv.GetSarHeight());
            }

            if (sps.vui.flags.nalHrdParametersPresent)
            {
                mfxU16 rcmethod   = sps.vui.nalHrdParameters.cbrFlag[0] ? mfxU16(MFX_RATECONTROL_CBR) : mfxU16(MFX_RATECONTROL_VBR);
                mfxU16 maxkbps    = mfxU16(((
                    (sps.vui.nalHrdParameters.bitRateValueMinus1[0] + 1) <<
                    (6 + sps.vui.nalHrdParameters.bitRateScale)) + 999) / 1000);
                mfxU16 buffersize = mfxU16((((sps.vui.nalHrdParameters.cpbSizeValueMinus1[0] + 1) <<
                    (4 + sps.vui.nalHrdParameters.cpbSizeScale)) + 7999) / 8000);
                mfxU16 lowDelayHrd = FlagToTriState(sps.vui.flags.lowDelayHrd);

                changed |= CheckAgreement(f, par.mfx.RateControlMethod,    rcmethod);
                changed |= CheckAgreement(f, par.calcParam.maxKbps,        maxkbps);
                changed |= CheckAgreement(f, par.calcParam.bufferSizeInKB, buffersize);
                changed |= CheckAgreement(f, extOpt3->LowDelayHrd,         lowDelayHrd);

            }
        }

        mfxU16 picTimingSei = sps.vui.flags.picStructPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
        mfxU16 vuiNalHrdParameters = sps.vui.flags.nalHrdParametersPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);
        mfxU16 vuiVclHrdParameters = sps.vui.flags.vclHrdParametersPresent
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

        if (sps.vui.flags.bitstreamRestriction)
            changed |= CheckAgreement(f, extOpt->MaxDecFrameBuffering, sps.vui.maxDecFrameBuffering);

        changed |= CheckAgreement(f, extOpt->PicTimingSEI,         picTimingSei);
        changed |= CheckAgreement(f, extOpt->VuiNalHrdParameters,  vuiNalHrdParameters);
        changed |= CheckAgreement(f, extOpt->VuiVclHrdParameters,  vuiVclHrdParameters);

        return changed == 0;
    }

    template <class TFunc>
    bool CheckAgreementOfPictureLevelParameters(mfxVideoParam & par, mfxExtPpsHeader const & pps)
    {
        mfxU32 changed = 0;

        mfxExtCodingOption * extOpt = GetExtBuffer(par);

        mfxU16 intraPredBlockSize = pps.transform8x8ModeFlag
            ? mfxU16(MFX_BLOCKSIZE_MIN_8X8)
            : mfxU16(MFX_BLOCKSIZE_MIN_16X16);
        mfxU16 cavlc = pps.entropyCodingModeFlag
            ? mfxU16(MFX_CODINGOPTION_OFF)
            : mfxU16(MFX_CODINGOPTION_ON);

        TFunc f;

        changed |= CheckAgreement(f, extOpt->IntraPredBlockSize, intraPredBlockSize);
        changed |= CheckAgreement(f, extOpt->CAVLC,              cavlc);

        return changed == 0;
    }

    template <class T>
    void InheritOption(T optInit, T & optReset)
    {
        if (optReset == 0)
            optReset = optInit;
    }
}

mfxU32 MfxHwH264Encode::CalcNumSurfRaw(MfxVideoParam const & video)
{
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(video);
    mfxExtCodingOption3 const *   extOpt3 = GetExtBuffer(video);
    
    if (video.IOPattern == MFX_IOPATTERN_IN_SYSTEM_MEMORY)
        return video.AsyncDepth + video.mfx.GopRefDist - 1 +
            IPP_MAX(1, extOpt2->LookAheadDepth) + (video.AsyncDepth - 1) +
            (IsOn(extOpt2->UseRawRef) ? video.mfx.NumRefFrame : 0) + ((extOpt2->MaxSliceSize!=0 || IsOn(extOpt3->FadeDetection)) ? 1:0);
    else
        return 0;
}

mfxU32 MfxHwH264Encode::CalcNumSurfRecon(MfxVideoParam const & video)
{
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(video);

    if (IsOn(extOpt2->UseRawRef))
        return video.mfx.NumRefFrame + (video.AsyncDepth - 1) +
            (video.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY ? video.mfx.GopRefDist : 1);
    else
        return video.mfx.NumRefFrame + video.AsyncDepth;
}

mfxU32 MfxHwH264Encode::CalcNumSurfBitstream(MfxVideoParam const & video)
{
    return (IsFieldCodingPossible(video) ? video.AsyncDepth * 2 : video.AsyncDepth);
}

mfxU16 MfxHwH264Encode::GetMaxNumSlices(MfxVideoParam const & par)
{
    mfxExtCodingOption3 * extOpt3 = GetExtBuffer(par);
    return IPP_MAX(extOpt3->NumSliceI, IPP_MAX(extOpt3->NumSliceP, extOpt3->NumSliceB));
}

mfxU8 MfxHwH264Encode::GetNumReorderFrames(MfxVideoParam const & video)
{
    mfxExtCodingOption2 * extOpt2 = GetExtBuffer(video);
    mfxU8 numReorderFrames = video.mfx.GopRefDist > 1 ? 1 : 0;

    if (video.mfx.GopRefDist > 2 && extOpt2->BRefType == MFX_B_REF_PYRAMID)
    {
        numReorderFrames = (mfxU8)IPP_MAX(CeilLog2(video.mfx.GopRefDist - 1), 1);
    }

    return numReorderFrames;
}

mfxU32 MfxHwH264Encode::CalcNumTasks(MfxVideoParam const & video)
{
    assert(video.mfx.GopRefDist > 0);
    assert(video.AsyncDepth > 0);
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(video);

    return video.mfx.GopRefDist + (video.AsyncDepth - 1) + IPP_MAX(1, extOpt2->LookAheadDepth) +
        (IsOn(extOpt2->UseRawRef) ? video.mfx.NumRefFrame : 0);
}

mfxI64 MfxHwH264Encode::CalcDTSFromPTS(
    mfxFrameInfo const & info,
    mfxU16               dpbOutputDelay,
    mfxU64               timeStamp)
{
    if (timeStamp != MFX_TIMESTAMP_UNKNOWN)
    {
        mfxF64 tcDuration90KHz = (mfxF64)info.FrameRateExtD / (info.FrameRateExtN * 2) * 90000; // calculate tick duration
        return mfxI64(timeStamp - tcDuration90KHz * dpbOutputDelay); // calculate DTS from PTS
    }

    return MFX_TIMESTAMP_UNKNOWN;
}

mfxU32 MfxHwH264Encode::GetMaxBitrateValue(mfxU32 kbps, mfxU32 scale)
{
    return (1000 * kbps) >> (6 + scale);
}

mfxU8 MfxHwH264Encode::GetCabacInitIdc(mfxU32 targetUsage)
{
    targetUsage;
    assert(targetUsage >= 1);
    assert(targetUsage <= 7);
    //const mfxU8 CABAC_INIT_IDC_TABLE[] = { 0, 2, 2, 1, 0, 0, 0, 0 };
    return 0;
    //return CABAC_INIT_IDC_TABLE[targetUsage];
}

bool MfxHwH264Encode::IsLookAheadSupported(
    MfxVideoParam const & /*video*/,
    eMFXHWType            platform)
{
    return ((platform >= MFX_HW_HSW) && (platform != MFX_HW_VLV));
}

// determine and return mode of Query operation (valid modes are 1, 2, 3, 4 - see MSDK spec for details)
mfxU8 MfxHwH264Encode::DetermineQueryMode(mfxVideoParam * in)
{
    if (in == 0)
        return 1;
    else
    {
        mfxExtEncoderCapability * caps = GetExtBuffer(*in);
        mfxExtEncoderResetOption * resetOpt = GetExtBuffer(*in);
        if (caps)
        {
            if (resetOpt)
            {
                // attached mfxExtEncoderCapability indicates mode 4. In this mode mfxExtEncoderResetOption shouldn't be attached
                return 0;
            }
            // specific mode to notify encoder to check guid only
            if (0x667 == caps->reserved[0])
            {
                return 5;
            }

            return 4;
        }else if (resetOpt)
            return 3;
        else
            return 2;
    }
}

mfxStatus MfxHwH264Encode::QueryHwCaps(VideoCORE* core, ENCODE_CAPS & hwCaps, GUID guid, bool isWiDi, mfxU32 width,  mfxU32 height)
{
    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(core);
    if (!pEncodeCaps)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    else
    {
        if (pEncodeCaps->GetHWCaps<ENCODE_CAPS>(guid, &hwCaps) == MFX_ERR_NONE)
            return MFX_ERR_NONE;
    }
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        return Error(MFX_ERR_DEVICE_FAILED);

    if (isWiDi)
        ddi->ForceCodingFunction(ENCODE_ENC_PAK | ENCODE_WIDI);

    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, width, height, true);
    MFX_CHECK_STS(sts);

    sts = ddi->QueryEncodeCaps(hwCaps);
    MFX_CHECK_STS(sts);

    return pEncodeCaps->SetHWCaps<ENCODE_CAPS>(guid, &hwCaps);


}

mfxStatus MfxHwH264Encode::QueryMbProcRate(VideoCORE* core, mfxVideoParam const & par, mfxU32 (&mbPerSec)[16], GUID guid, bool isWiDi, mfxU32 width,  mfxU32 height)
{
    EncodeHWCaps* pEncodeCaps = QueryCoreInterface<EncodeHWCaps>(core, MFXIHWMBPROCRATE_GUID);
    if (!pEncodeCaps)
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    else
    {
        if (pEncodeCaps->GetHWCaps<mfxU32>(guid, mbPerSec, 16) == MFX_ERR_NONE &&
            mbPerSec[(par.mfx.TargetUsage?par.mfx.TargetUsage:4) - 1] != 0) //check if MbPerSec for particular TU was already queried or need to save
            return MFX_ERR_NONE;
    }

    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        return Error(MFX_ERR_DEVICE_FAILED);

    if (isWiDi)
        ddi->ForceCodingFunction(ENCODE_ENC_PAK | ENCODE_WIDI);

    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, width, height, true);
    MFX_CHECK_STS(sts);

    mfxU32 tempMbPerSec[16] = {0, };
    sts = ddi->QueryMbPerSec(par, tempMbPerSec);
    
    MFX_CHECK_STS(sts);
    mbPerSec[par.mfx.TargetUsage - 1] = tempMbPerSec[0];

    return pEncodeCaps->SetHWCaps<mfxU32>(guid, mbPerSec, 16);
}

mfxStatus MfxHwH264Encode::QueryInputTilingSupport(VideoCORE* core, mfxVideoParam const & par, mfxU32 &inputTiling, GUID guid, mfxU32 width,  mfxU32 height)
{

    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        return Error(MFX_ERR_DEVICE_FAILED);

    mfxStatus sts = ddi->CreateAuxilliaryDevice(core, guid, width, height, true);
    MFX_CHECK_STS(sts);

    sts = ddi->QueryInputTilingSupport(par, inputTiling);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::QueryGuid(VideoCORE* core, GUID guid)
{
    std::auto_ptr<DriverEncoder> ddi;

    ddi.reset(CreatePlatformH264Encoder(core));
    if (ddi.get() == 0)
        return Error(MFX_ERR_DEVICE_FAILED);

    return ddi->QueryHWGUID(core, guid, true);
}

mfxStatus MfxHwH264Encode::ReadSpsPpsHeaders(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);

    try
    {
        if (extBits->SPSBuffer)
        {
            InputBitstream reader(extBits->SPSBuffer, extBits->SPSBufSize);
            mfxExtSpsHeader * extSps = GetExtBuffer(par);
            ReadSpsHeader(reader, *extSps);

            if (extBits->PPSBuffer)
            {
                InputBitstream reader(extBits->PPSBuffer, extBits->PPSBufSize);
                mfxExtPpsHeader * extPps = GetExtBuffer(par);
                ReadPpsHeader(reader, *extSps, *extPps);
            }
        }
    }
    catch (std::exception &)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxU16 MfxHwH264Encode::GetFrameWidth(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    if (extBits->SPSBuffer)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(par);
        return mfxU16(16 * (extSps->picWidthInMbsMinus1 + 1));
    }
    else
    {
        return par.mfx.FrameInfo.Width;
    }
}

mfxU16 MfxHwH264Encode::GetFrameHeight(MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    if (extBits->SPSBuffer)
    {
        mfxExtSpsHeader * extSps = GetExtBuffer(par);
        return mfxU16(16 * (extSps->picHeightInMapUnitsMinus1 + 1) * (2 - extSps->frameMbsOnlyFlag));
    }
    else
    {
        return par.mfx.FrameInfo.Height;
    }
}


mfxStatus MfxHwH264Encode::CorrectCropping(MfxVideoParam& par)
{
    mfxStatus sts = MFX_ERR_NONE;

    mfxU16 horzCropUnit = CROP_UNIT_X[par.mfx.FrameInfo.ChromaFormat];
    mfxU16 vertCropUnit = CROP_UNIT_Y[par.mfx.FrameInfo.ChromaFormat];
    vertCropUnit *= IsFieldCodingPossible(par) ? 2 : 1;

    mfxU16 misalignment = par.mfx.FrameInfo.CropX & (horzCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropX += horzCropUnit - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (par.mfx.FrameInfo.CropW >= horzCropUnit - misalignment)
            par.mfx.FrameInfo.CropW -= horzCropUnit - misalignment;
        else
            par.mfx.FrameInfo.CropW = 0;
    }

    misalignment = par.mfx.FrameInfo.CropW & (horzCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.CropW - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    misalignment = par.mfx.FrameInfo.CropY & (vertCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropY += vertCropUnit - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

        if (par.mfx.FrameInfo.CropH >= vertCropUnit - misalignment)
            par.mfx.FrameInfo.CropH -= vertCropUnit - misalignment;
        else
            par.mfx.FrameInfo.CropH = 0;
    }

    misalignment = par.mfx.FrameInfo.CropH & (vertCropUnit - 1);
    if (misalignment > 0)
    {
        par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.CropH - misalignment;
        sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return sts;
}

bool MfxHwH264Encode::IsRunTimeOnlyExtBuffer(mfxU32 id)
{
    return
           id == MFX_EXTBUFF_AVC_REFLIST_CTRL
        || id == MFX_EXTBUFF_ENCODED_FRAME_INFO
        || id == MFX_EXTBUFF_MBQP
        || id == MFX_EXTBUFF_MB_DISABLE_SKIP_MAP
        || id == MFX_EXTBUFF_AVC_ENCODE_CTRL;
}

bool MfxHwH264Encode::IsRunTimeExtBufferIdSupported(mfxU32 id)
{
    return
          (id == MFX_EXTBUFF_AVC_REFLIST_CTRL
#if defined (ADVANCED_REF)
        || id == MFX_EXTBUFF_AVC_REFLISTS
#endif
        || id == MFX_EXTBUFF_ENCODED_FRAME_INFO
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
        || id == MFX_EXTBUFF_CODING_OPTION2
        || id == MFX_EXTBUFF_ENCODER_ROI
        || id == MFX_EXTBUFF_MBQP
        || id == MFX_EXTBUFF_MB_DISABLE_SKIP_MAP
        || id == MFX_EXTBUFF_ENCODER_WIDI_USAGE
        || id == MFX_EXTBUFF_AVC_ENCODE_CTRL
        || id == MFX_EXTBUFF_PRED_WEIGHT_TABLE
#if defined (MFX_ENABLE_H264_VIDEO_FEI_ENCPAK)
        || id == MFX_EXTBUFF_FEI_ENC_CTRL
        || id == MFX_EXTBUFF_FEI_ENC_MB
        || id == MFX_EXTBUFF_FEI_ENC_MV_PRED
#endif
        );
}

bool MfxHwH264Encode::IsVideoParamExtBufferIdSupported(mfxU32 id)
{
    return
           (id == MFX_EXTBUFF_CODING_OPTION
        || id == MFX_EXTBUFF_CODING_OPTION_SPSPPS
        || id == MFX_EXTBUFF_DDI
        || id == MFX_EXTBUFF_DUMP
        || id == MFX_EXTBUFF_PAVP_OPTION
        || id == MFX_EXTBUFF_MVC_SEQ_DESC
        || id == MFX_EXTBUFF_VIDEO_SIGNAL_INFO
        || id == MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION
        || id == MFX_EXTBUFF_PICTURE_TIMING_SEI
        || id == MFX_EXTBUFF_AVC_TEMPORAL_LAYERS
        || id == MFX_EXTBUFF_CODING_OPTION2
        || id == MFX_EXTBUFF_SVC_SEQ_DESC
        || id == MFX_EXTBUFF_SVC_RATE_CONTROL
        || id == MFX_EXTBUFF_ENCODER_RESET_OPTION
        || id == MFX_EXTBUFF_ENCODER_CAPABILITY
        || id == MFX_EXTBUFF_ENCODER_WIDI_USAGE
        || id == MFX_EXTBUFF_ENCODER_ROI
        || id == MFX_EXTBUFF_CODING_OPTION3
        || id == MFX_EXTBUFF_CHROMA_LOC_INFO
        || id == MFX_EXTBUFF_PRED_WEIGHT_TABLE
        || id == MFX_EXTBUFF_DIRTY_RECTANGLES
        || id == MFX_EXTBUFF_MOVING_RECTANGLES
        || id == MFX_EXTBUFF_FEI_CODING_OPTION
#if defined (MFX_ENABLE_H264_VIDEO_FEI_ENCPAK)
        || id == MFX_EXTBUFF_FEI_PARAM
#endif
        );
}

mfxStatus MfxHwH264Encode::CheckExtBufferId(mfxVideoParam const & par)
{
    for (mfxU32 i = 0; i < par.NumExtParam; i++)
    {
        if (par.ExtParam[i] == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;

        if (!IsVideoParamExtBufferIdSupported(par.ExtParam[i]->BufferId))
            return MFX_ERR_INVALID_VIDEO_PARAM;

        // check if buffer presents twice in video param
        if (MfxHwH264Encode::GetExtBuffer(
            par.ExtParam + i + 1,
            par.NumExtParam - i - 1,
            par.ExtParam[i]->BufferId) != 0)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    return MFX_ERR_NONE;
}

namespace
{
    mfxStatus CheckWidthAndHeight(mfxU32 width, mfxU32 height, mfxU32 picStruct)
    {
        MFX_CHECK(width  > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(height > 0, MFX_ERR_INVALID_VIDEO_PARAM);

        MFX_CHECK((width  & 15) == 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK((height & 15) == 0, MFX_ERR_INVALID_VIDEO_PARAM);

        // repeat flags are for EncodeFrameAsync
        if (picStruct & MFX_PICSTRUCT_PART2)
            picStruct = MFX_PICSTRUCT_UNKNOWN;

        // more then one flag was set
        if (mfxU32 picStructPart1 = picStruct & MFX_PICSTRUCT_PART1)
            if (picStructPart1 & (picStructPart1 - 1))
                picStruct = MFX_PICSTRUCT_UNKNOWN;

        if ((picStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            MFX_CHECK((height & 31) == 0, MFX_ERR_INVALID_VIDEO_PARAM);

        return MFX_ERR_NONE;
    }
};

mfxU32 MfxHwH264Encode::GetLastDid(mfxExtSVCSeqDesc const & extSvc)
{
    mfxI32 i = 7;
    for (; i >= 0; i--)
        if (extSvc.DependencyLayer[i].Active)
            break;
    return mfxU32(i);
}

mfxStatus MfxHwH264Encode::CheckWidthAndHeight(MfxVideoParam const & par)
{
    mfxExtCodingOptionSPSPPS const * extBits = GetExtBuffer(par);

    if (extBits->SPSBuffer)
        // width/height/picStruct are always valid
        // when they come from SPSPPS buffer
        // no need to check them
        return MFX_ERR_NONE;

    mfxU16 width     = par.mfx.FrameInfo.Width;
    mfxU16 height    = par.mfx.FrameInfo.Height;
    mfxU16 picStruct = par.mfx.FrameInfo.PicStruct;

    if (IsSvcProfile(par.mfx.CodecProfile))
    {
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);
        mfxU32 lastDid = GetLastDid(*extSvc);
        if (lastDid > 7)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        width  = extSvc->DependencyLayer[lastDid].Width;
        height = extSvc->DependencyLayer[lastDid].Height;
        if (width == 0 || height == 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return ::CheckWidthAndHeight(width, height, picStruct);
}

mfxStatus MfxHwH264Encode::CopySpsPpsToVideoParam(
    MfxVideoParam & par)
{
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);

    bool changed = false;

    if (extBits->SPSBuffer)
    {
        mfxExtSpsHeader const * extSps = GetExtBuffer(par);
        if (!CheckAgreementOfSequenceLevelParameters<FunctionInit>(par, *extSps))
            changed = true;
    }


    if (extBits->PPSBuffer)
    {
        mfxExtPpsHeader const * extPps = GetExtBuffer(par);
        if (!CheckAgreementOfPictureLevelParameters<FunctionInit>(par, *extPps))
            changed = true;
    }

    return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

// check encoding configuration for number of H264 spec violations allowed by MSDK
// it's required to report correct status to application
mfxStatus MfxHwH264Encode::CheckForAllowedH264SpecViolations(
    MfxVideoParam const & par)
{
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE
        && par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN
        && par.mfx.CodecLevel > MFX_LEVEL_AVC_41)
    {
        return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::CheckVideoParam(
    MfxVideoParam &     par,
    ENCODE_CAPS const & hwCaps,
    bool                setExtAlloc,
    eMFXHWType          platform,
    eMFXVAType          vaType)
{
    mfxStatus checkSts = MFX_ERR_NONE;

    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps  = GetExtBuffer(par);
    mfxExtCodingOption3 *      extOpt3 = GetExtBuffer(par);

    // first check mandatory parameters
    MFX_CHECK(
        extBits->SPSBuffer != 0 ||
        par.mfx.FrameInfo.Width > 0 && par.mfx.FrameInfo.Height > 0, MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(
        extSps->vui.timeScale           > 0 && extSps->vui.numUnitsInTick      > 0 ||
        par.mfx.FrameInfo.FrameRateExtN > 0 && par.mfx.FrameInfo.FrameRateExtD > 0,
        MFX_ERR_INVALID_VIDEO_PARAM);

    MFX_CHECK(par.mfx.TargetUsage            <= 7, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par.mfx.FrameInfo.ChromaFormat != 0, MFX_ERR_INVALID_VIDEO_PARAM);
    MFX_CHECK(par.IOPattern                  != 0, MFX_ERR_INVALID_VIDEO_PARAM);

    mfxStatus sts = MFX_ERR_NONE;

    if (IsMvcProfile(par.mfx.CodecProfile))
    {
        mfxExtCodingOption * extOpt = GetExtBuffer(par);
        mfxExtMVCSeqDesc * extMvc   = GetExtBuffer(par);
        sts = CheckAndFixMVCSeqDesc(extMvc, extOpt->ViewOutput == MFX_CODINGOPTION_ON);
        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == sts)
        {
            checkSts = sts;
        }
        else if (sts == MFX_ERR_UNSUPPORTED)
        {
            return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        sts = CheckVideoParamMvcQueryLike(par); // check and correct mvc per-view bitstream, buffer, level
        switch (sts)
        {
        case MFX_ERR_UNSUPPORTED:
            return MFX_ERR_INVALID_VIDEO_PARAM;
        case MFX_ERR_INVALID_VIDEO_PARAM:
        case MFX_WRN_PARTIAL_ACCELERATION:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
            return sts;
        case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
            checkSts = sts;
            break;
        }
    }

    sts = CheckVideoParamQueryLike(par, hwCaps, platform, vaType);
    switch (sts)
    {
    case MFX_ERR_UNSUPPORTED:
        return MFX_ERR_INVALID_VIDEO_PARAM;
    case MFX_ERR_INVALID_VIDEO_PARAM:
    case MFX_WRN_PARTIAL_ACCELERATION:
    case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        return sts;
    case MFX_WRN_INCOMPATIBLE_VIDEO_PARAM:
        checkSts = sts;
        break;
    }

    if (par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY)
    {
        MFX_CHECK(setExtAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    MFX_CHECK(
        ((par.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY) || (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY) ||(par.Protected == 0)),
        MFX_ERR_INVALID_VIDEO_PARAM);

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ &&
        !IsSvcProfile(par.mfx.CodecProfile))
        MFX_CHECK(par.calcParam.targetKbps > 0, MFX_ERR_INVALID_VIDEO_PARAM);

    if (extOpt3->NumSliceI || extOpt3->NumSliceP || extOpt3->NumSliceB)
    {
        // application should set number of slices for all 3 slice types at once
        MFX_CHECK(extOpt3->NumSliceI > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(extOpt3->NumSliceP > 0, MFX_ERR_INVALID_VIDEO_PARAM);
        MFX_CHECK(extOpt3->NumSliceB > 0, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    SetDefaults(par, hwCaps, setExtAlloc, platform, vaType);

    sts = CheckForAllowedH264SpecViolations(par);
    if (sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM)
        checkSts = sts;

    if (par.IOPattern == MFX_IOPATTERN_IN_OPAQUE_MEMORY)
    {
        mfxExtCodingOption2 *      extOpt2 = GetExtBuffer(par);
        //mfxExtOpaqueSurfaceAlloc * extOpaq = GetExtBuffer(par);

        mfxU32 numFrameMin = par.mfx.GopRefDist + (IsOn(extOpt2->UseRawRef) ? par.mfx.NumRefFrame : 0) + par.AsyncDepth - 1;

        if (IsMvcProfile(par.mfx.CodecProfile))
        {
            mfxExtMVCSeqDesc * extMvc  = GetExtBuffer(par);
            numFrameMin *= extMvc->NumView;
        }

        //MFX_CHECK(extOpaq->In.NumSurface >= numFrameMin, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    return checkSts;
}

/*
 * utils for debug purposes
 */
struct Bool
{
    explicit Bool(bool val) : m_val(val) {}
    Bool(Bool const & rhs) : m_val(rhs.m_val) {}
    Bool & operator =(Bool const & rhs) { m_val = rhs.m_val; return *this; }
    operator bool() { return m_val; }

    Bool & operator =(bool val)
    {
        //__asm { int 3 }
        m_val = val;
        return *this;
    }

private:
    bool m_val;
};

mfxStatus MfxHwH264Encode::CheckAndFixRectQueryLike(
    MfxVideoParam const & par,
    mfxRectDesc *         rect)
{
    mfxStatus checkSts = MFX_ERR_NONE;

    // check that rectangle is aligned to MB, correct it if not
    if (!CheckMbAlignment(rect->Left))   checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignment(rect->Right))  checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignment(rect->Top))    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
    if (!CheckMbAlignment(rect->Bottom)) checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    // check that rectangle dimensions don't conflict with each other and don't exceed frame size
    if (par.mfx.FrameInfo.Width)
    {
        if(!CheckRangeDflt(rect->Left, mfxU32(0), mfxU32(par.mfx.FrameInfo.Width -16), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;
        if(!CheckRangeDflt(rect->Right, mfxU32(rect->Left + 16), mfxU32(par.mfx.FrameInfo.Width), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;
    }
    else if(rect->Right && rect->Right < (rect->Left + 16))
        {
            checkSts = MFX_ERR_UNSUPPORTED;
            rect->Right = 0;
        }

    if (par.mfx.FrameInfo.Height)
    {
        if(!CheckRangeDflt(rect->Top, mfxU32(0), mfxU32(par.mfx.FrameInfo.Height -16), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;
        if(!CheckRangeDflt(rect->Bottom, mfxU32(rect->Top + 16), mfxU32(par.mfx.FrameInfo.Height), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;
    }
    else if(rect->Bottom && rect->Bottom < (rect->Top + 16))
        {
            checkSts = MFX_ERR_UNSUPPORTED;
            rect->Bottom = 0;
        }

    return checkSts;
}

mfxStatus MfxHwH264Encode::CheckAndFixRoiQueryLike(
    MfxVideoParam const & par,
    mfxRoiDesc *          roi)
{
    mfxStatus checkSts = CheckAndFixRectQueryLike(par, (mfxRectDesc*)roi);

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        if (!CheckRangeDflt(roi->Priority, -51, 51, 0))
            checkSts = MFX_ERR_UNSUPPORTED;
    } else if (par.mfx.RateControlMethod)
    {
        if (!CheckRangeDflt(roi->Priority, -3, 3, 0))
            checkSts = MFX_ERR_UNSUPPORTED;
    }

    return checkSts;
}

mfxStatus MfxHwH264Encode::CheckAndFixMovingRectQueryLike(
    MfxVideoParam const & par,
    mfxMovingRectDesc *   rect)
{
    mfxStatus checkSts = CheckAndFixRectQueryLike(par, (mfxRectDesc*)rect);

    if (par.mfx.FrameInfo.Width)
        if(!CheckRangeDflt(rect->SourceLeft, mfxU32(0), mfxU32(par.mfx.FrameInfo.Width - 1), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;

    if (par.mfx.FrameInfo.Height)
        if(!CheckRangeDflt(rect->SourceTop, mfxU32(0), mfxU32(par.mfx.FrameInfo.Height - 1), mfxU32(0))) checkSts = MFX_ERR_UNSUPPORTED;

    return checkSts;
}

//typedef bool Bool;

mfxStatus MfxHwH264Encode::CheckVideoParamQueryLike(
    MfxVideoParam &     par,
    ENCODE_CAPS const & hwCaps,
    eMFXHWType          platform,
    eMFXVAType          vaType)
{
    Bool unsupported(false);
    Bool changed(false);
    Bool warning(false);

    mfxExtCodingOption *       extOpt       = GetExtBuffer(par);
    mfxExtCodingOptionDDI *    extDdi       = GetExtBuffer(par);
    mfxExtPAVPOption *         extPavp      = GetExtBuffer(par);
    mfxExtVideoSignalInfo *    extVsi       = GetExtBuffer(par);
    mfxExtCodingOptionSPSPPS * extBits      = GetExtBuffer(par);
    mfxExtPictureTimingSEI *   extPt        = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps       = GetExtBuffer(par);
    mfxExtPpsHeader *          extPps       = GetExtBuffer(par);
    mfxExtMVCSeqDesc *         extMvc       = GetExtBuffer(par);
    mfxExtAvcTemporalLayers *  extTemp      = GetExtBuffer(par);
    mfxExtSVCSeqDesc *         extSvc       = GetExtBuffer(par);
    mfxExtCodingOption2 *      extOpt2      = GetExtBuffer(par);
    mfxExtEncoderROI *         extRoi       = GetExtBuffer(par);
    mfxExtCodingOption3 *      extOpt3      = GetExtBuffer(par);
    mfxExtChromaLocInfo *      extCli       = GetExtBuffer(par);
    mfxExtDirtyRect  *         extDirtyRect = GetExtBuffer(par);
    mfxExtMoveRect *           extMoveRect  = GetExtBuffer(par);
    mfxExtPredWeightTable *    extPwt       = GetExtBuffer(par);

    // check hw capabilities
    if (par.mfx.FrameInfo.Width  > hwCaps.MaxPicWidth ||
        par.mfx.FrameInfo.Height > hwCaps.MaxPicHeight)
        return Error(MFX_WRN_PARTIAL_ACCELERATION);

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1 )!= MFX_PICSTRUCT_PROGRESSIVE && hwCaps.NoInterlacedField){
        if(par.mfx.LowPower == MFX_CODINGOPTION_ON)
        {
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            changed = true;
        }
        else
            return Error(MFX_WRN_PARTIAL_ACCELERATION);
    }

    if (hwCaps.MaxNum_TemporalLayer != 0 &&
        hwCaps.MaxNum_TemporalLayer < par.calcParam.numTemporalLayer)
        return Error(MFX_WRN_PARTIAL_ACCELERATION);

#if defined(LOWPOWERENCODE_AVC)
    if (!CheckTriStateOption(par.mfx.LowPower)) changed = true;

    if (IsOn(par.mfx.LowPower))
    {
        if (par.mfx.GopRefDist > 1)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }

        if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        {
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }


        if (par.mfx.RateControlMethod != 0 &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
            par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
        {
            changed = true;
            par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        }

        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
            && par.calcParam.cqpHrdMode == 0)
        {
            if (!CheckRange(par.mfx.QPI, 10, 51)) changed = true;
            if (!CheckRange(par.mfx.QPP, 10, 51)) changed = true;
            if (!CheckRange(par.mfx.QPB, 10, 51)) changed = true;
        }
    }
#endif

    if (par.mfx.GopRefDist > 1 && hwCaps.SliceIPOnly)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    // sps/pps parameters if present overrides corresponding fields of VideoParam
    if (extBits->SPSBuffer)
    {
        if (!CheckAgreementOfSequenceLevelParameters<FunctionQuery>(par, *extSps))
            changed = true;

        if (extBits->PPSBuffer)
            if (!CheckAgreementOfPictureLevelParameters<FunctionQuery>(par, *extPps))
                changed = true;
    }

    if (par.Protected != 0)
    {
        if (!IsProtectionPavp(par.Protected) && !IsProtectionHdcp(par.Protected))
        {
            unsupported = true;
            par.Protected = 0;
        }

        if (extPavp->EncryptionType != 0 &&
            extPavp->EncryptionType != MFX_PAVP_AES128_CTR &&
            extPavp->EncryptionType != MFX_PAVP_AES128_CBC)
        {
            unsupported = true;
            extPavp->EncryptionType = 0;
        }

        if (extPavp->CounterType != 0 &&
            extPavp->CounterType != MFX_PAVP_CTR_TYPE_A &&
            extPavp->CounterType != MFX_PAVP_CTR_TYPE_B &&
            extPavp->CounterType != MFX_PAVP_CTR_TYPE_C)
        {
            unsupported = true;
            extPavp->CounterType = 0;
        }

        if ((extPavp->CounterType == MFX_PAVP_CTR_TYPE_A) &&
            (extPavp->CipherCounter.Count & 0xffffffff) != 0)
        {
            changed = true;
            extPavp->CipherCounter.Count = 0xffffffff00000000;
        }

        if (extPavp->CounterIncrement != 0)
        {
            if (!CheckRange(extPavp->CounterIncrement, 0xC000u, 0xC0000u))
            {
                changed = true;
                extPavp->CounterIncrement = 0xC000;
            }
        }
    }

    if (par.IOPattern != 0)
    {
        if ((par.IOPattern & MFX_IOPATTERN_IN_MASK) != par.IOPattern)
        {
            changed = true;
            par.IOPattern &= MFX_IOPATTERN_IN_MASK;
        }

        if (par.IOPattern != MFX_IOPATTERN_IN_VIDEO_MEMORY  &&
            par.IOPattern != MFX_IOPATTERN_IN_SYSTEM_MEMORY &&
            par.IOPattern != MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            changed = true;
            par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        }
    }

    //disable SW brc
    /*if (IsOn(extOpt2->ExtBRC) && par.mfx.RateControlMethod == MFX_RATECONTROL_LA && IsLookAheadSupported(par, platform))
    {
        // turn on ExtBRC if LA will be used
        changed = true;
        extOpt2->ExtBRC = MFX_CODINGOPTION_OFF;
    }*/

    /*if (par.AsyncDepth > 1 && IsOn(extOpt2->ExtBRC))
    {
        changed = true;
        par.AsyncDepth = 1;
    }*/

    if (par.mfx.TargetUsage > 7)
    {
        changed = true;
        par.mfx.TargetUsage = 7;
    }

    if (par.mfx.GopPicSize > 0 &&
        par.mfx.GopRefDist > 0 &&
        par.mfx.GopRefDist > par.mfx.GopPicSize)
    {
        changed = true;
        par.mfx.GopRefDist = par.mfx.GopPicSize - 1;
    }

    if (par.mfx.GopRefDist > 1)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
    }

    if (par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_WIDI_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VCM &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VME  &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
        !bRateControlLA(par.mfx.RateControlMethod))
    {
        changed = true;
        par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    }
    if (extOpt2->MaxSliceSize)
    {
        if (par.mfx.GopRefDist > 1)
        {
            changed = true;
            par.mfx.GopRefDist = 1;
        }
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
        {
            par.mfx.RateControlMethod = MFX_RATECONTROL_LA;
            changed = true;
        }
        if (extOpt2->LookAheadDepth > 1)
        {
            extOpt2->LookAheadDepth = 1;
            changed = true;
        }
        if (IsOn(extOpt->FieldOutput))
        {
            unsupported = true;
            extOpt->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
        }
        if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
        {
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            changed = true;
        }
        if (extDdi->LookAheadDependency != 0)
        {
            extDdi->LookAheadDependency = 0;
            changed = true;
        }
        if (par.AsyncDepth > 1)
        {
            par.AsyncDepth  = 1;
            changed = true;
        }
        if (extOpt2->NumMbPerSlice)
        {
            extOpt2->NumMbPerSlice = 0;
            unsupported = true;      
        }


    }
    else if (extOpt2->LookAheadDepth > 0)
    {
        if (extOpt2->LookAheadDepth < MIN_LOOKAHEAD_DEPTH)
        {
            changed = true;
            extOpt2->LookAheadDepth = MIN_LOOKAHEAD_DEPTH;
        }

        if (extOpt2->LookAheadDepth > MAX_LOOKAHEAD_DEPTH)
        {
            changed = true;
            extOpt2->LookAheadDepth = MAX_LOOKAHEAD_DEPTH;
        }

        if (!bRateControlLA(par.mfx.RateControlMethod))
        {
            changed = true;
            extOpt2->LookAheadDepth = 0;
        }
        else if (par.mfx.GopRefDist > 0 && extOpt2->LookAheadDepth < 2 * par.mfx.GopRefDist)
        {
            changed = true;
            extOpt2->LookAheadDepth = 2 * par.mfx.GopRefDist;
        }
    }


    if (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART2)
    { // repeat/double/triple flags are for EncodeFrameAsync
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
    }

    if (mfxU16 picStructPart1 = par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PART1)
    {
        if (picStructPart1 & (picStructPart1 - 1))
        { // more then one flag set
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
    }

    if (par.mfx.FrameInfo.Width & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Width = 0;
    }

    if (par.mfx.FrameInfo.Height & 15)
    {
        unsupported = true;
        par.mfx.FrameInfo.Height = 0;
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 &&
        (par.mfx.FrameInfo.Height & 31) != 0)
    {
        unsupported = true;
        par.mfx.FrameInfo.PicStruct = 0;
        par.mfx.FrameInfo.Height = 0;
    }

    if (par.mfx.FrameInfo.Width > 0)
    {
        if (par.mfx.FrameInfo.CropX > par.mfx.FrameInfo.Width)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            unsupported = true;
            par.mfx.FrameInfo.CropX = 0;
        }

        if (par.mfx.FrameInfo.CropX + par.mfx.FrameInfo.CropW > par.mfx.FrameInfo.Width)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            unsupported = true;
            par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;
        }
    }

    if (par.mfx.FrameInfo.Height > 0)
    {
        if (par.mfx.FrameInfo.CropY > par.mfx.FrameInfo.Height)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            unsupported = true;
            par.mfx.FrameInfo.CropY = 0;
        }

        if (par.mfx.FrameInfo.CropY + par.mfx.FrameInfo.CropH > par.mfx.FrameInfo.Height)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            unsupported = true;
            par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;
        }
    }

    if (CorrectCropping(par) != MFX_ERR_NONE)
    { // cropping read from sps header always has correct alignment
        changed = true;
    }


    if (extOpt2->NumMbPerSlice != 0)
    {
        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;
        mfxU16 widthInMbs  = par.mfx.FrameInfo.Width / 16;
        mfxU16 heightInMbs = par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1);

        if (   (hwCaps.SliceStructure == 0)
            || (hwCaps.SliceStructure  < 4 && (extOpt2->NumMbPerSlice % (par.mfx.FrameInfo.Width >> 4)))
            || (hwCaps.SliceStructure == 1 && ((extOpt2->NumMbPerSlice / widthInMbs) & ((extOpt2->NumMbPerSlice / widthInMbs) - 1)))
            || (widthInMbs * heightInMbs) < extOpt2->NumMbPerSlice)
        {
            extOpt2->NumMbPerSlice = 0;
            unsupported = true;
        }
    }

    if (par.mfx.NumSlice !=0 &&
      (extOpt3->NumSliceI != 0 || extOpt3->NumSliceP != 0 || extOpt3->NumSliceB != 0))
    {
        if(par.mfx.NumSlice != extOpt3->NumSliceI &&
           par.mfx.NumSlice != extOpt3->NumSliceP &&
           par.mfx.NumSlice != extOpt3->NumSliceB)
        {
            changed = true;
            par.mfx.NumSlice = 0;
        }
    }

    if (par.mfx.NumSlice         != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (par.mfx.NumSlice > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            par.mfx.NumSlice = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            hwCaps.SliceStructure,
            extOpt2->NumMbPerSlice,
            par.mfx.NumSlice,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (par.mfx.NumSlice != divider.GetNumSlice())
        {
            changed = true;
            par.mfx.NumSlice = (mfxU16)divider.GetNumSlice();
        }
    }

    if (extOpt3->NumSliceI       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceI > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceI = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            hwCaps.SliceStructure,
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceI,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceI != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceI = (mfxU16)divider.GetNumSlice();
        }
    }

    if (extOpt3->NumSliceP       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceP > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceP = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            hwCaps.SliceStructure,
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceP,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceP != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceP = (mfxU16)divider.GetNumSlice();
        }
    }

    if (extOpt3->NumSliceB       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        if (extOpt3->NumSliceB > par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height / 256)
        {
            unsupported = true;
            extOpt3->NumSliceB = 0;
        }

        bool fieldCoding = (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0;

        SliceDivider divider = MakeSliceDivider(
            hwCaps.SliceStructure,
            extOpt2->NumMbPerSlice,
            extOpt3->NumSliceB,
            par.mfx.FrameInfo.Width / 16,
            par.mfx.FrameInfo.Height / 16 / (fieldCoding ? 2 : 1));

        if (extOpt3->NumSliceB != divider.GetNumSlice())
        {
            changed = true;
            extOpt3->NumSliceB = (mfxU16)divider.GetNumSlice();
        }
    }

    if (par.mfx.FrameInfo.ChromaFormat != 0 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC != 0 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_RGB4 &&
        par.mfx.FrameInfo.FourCC != MFX_FOURCC_YUY2)
    {
        unsupported = true;
        par.mfx.FrameInfo.FourCC = 0;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV420 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_MONOCHROME)
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_YUY2 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV422)
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
    }

    if (par.mfx.FrameInfo.FourCC == MFX_FOURCC_RGB4 &&
        par.mfx.FrameInfo.ChromaFormat != MFX_CHROMAFORMAT_YUV444)
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
    }

    if (hwCaps.Color420Only &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        unsupported = true;
        par.mfx.FrameInfo.ChromaFormat = 0;
    }
    
    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE &&
        (par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV422 ||
         par.mfx.FrameInfo.ChromaFormat == MFX_CHROMAFORMAT_YUV444))
    {
        changed = true;
        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if (!CheckTriStateOption(extOpt2->DisableVUI))         changed = true;
    if (!CheckTriStateOption(extOpt->VuiNalHrdParameters)) changed = true;
    if (!CheckTriStateOption(extOpt->VuiVclHrdParameters)) changed = true;
    if (!CheckTriStateOption(extOpt2->FixedFrameRate))     changed = true;
    if (!CheckTriStateOption(extOpt3->LowDelayHrd))        changed = true;

    if (IsOn(extOpt2->DisableVUI))
    {
        bool contradictoryVuiParam = false;
        if (!CheckTriStateOptionForOff(extOpt3->TimingInfoPresent))      contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->OverscanInfoPresent))    contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->AspectRatioInfoPresent)) contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt3->BitstreamRestriction))   contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->VuiNalHrdParameters))     contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->VuiVclHrdParameters))     contradictoryVuiParam = true;
        if (!CheckTriStateOptionForOff(extOpt->PicTimingSEI))            contradictoryVuiParam = true;

        if (contradictoryVuiParam)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
        }
    }

    if ((par.mfx.FrameInfo.FrameRateExtN == 0) !=
        (par.mfx.FrameInfo.FrameRateExtD == 0))
    {
        if (extBits->SPSBuffer && !IsOff(extOpt3->TimingInfoPresent))
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        unsupported = true;
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
    }

    if (!IsOff(extOpt3->TimingInfoPresent) &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0 &&
        par.mfx.FrameInfo.FrameRateExtN > mfxU64(172) * par.mfx.FrameInfo.FrameRateExtD)
    {
        if (extBits->SPSBuffer) // frame_rate read from sps
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        unsupported = true;
        par.mfx.FrameInfo.FrameRateExtN = 0;
        par.mfx.FrameInfo.FrameRateExtD = 0;
    }

    if (IsOff(extOpt3->TimingInfoPresent) &&
        IsOn(extOpt2->FixedFrameRate))
    {
        // if timing_info_present_flag is 0, fixed_frame_rate_flag is inferred to be 0 (Annex E.2.1)
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        extOpt2->FixedFrameRate = MFX_CODINGOPTION_OFF;
    }

    if (!IsOff(extOpt3->TimingInfoPresent))
    {
        if (IsOn(extOpt2->FixedFrameRate) &&
            IsOn(extOpt3->LowDelayHrd))
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            extOpt3->LowDelayHrd = MFX_CODINGOPTION_OFF;
        }
    }

    if (par.mfx.CodecProfile != MFX_PROFILE_UNKNOWN && !IsValidCodingProfile(par.mfx.CodecProfile))
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        if (par.mfx.CodecProfile == MFX_PROFILE_AVC_MULTIVIEW_HIGH)
            return Error(MFX_ERR_UNSUPPORTED);

        changed = true;
        par.mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
    }

    if (par.mfx.CodecLevel != MFX_LEVEL_UNKNOWN && !IsValidCodingLevel(par.mfx.CodecLevel))
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        par.mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
    }

    if (par.mfx.NumRefFrame != 0)
    {
        if ((par.mfx.NumRefFrame & 1) &&
            (hwCaps.HeaderInsertion) &&
            (par.Protected || !IsOff(extOpt->NalHrdConformance)))
        {
            // when driver writes headers it can write only even values of num_ref_frames
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            par.mfx.NumRefFrame++;
        }
    }

    if (par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByFrameSize(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            else if (par.mfx.CodecLevel != 0)
            {
                changed = true; // warning should be returned if frame size exceeds maximum value supported by AVC standard
                if (par.mfx.CodecLevel < GetMaxSupportedLevel())
                    par.mfx.CodecLevel = GetMaxSupportedLevel();
            }
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (!IsOff(extOpt3->TimingInfoPresent)  &&
        par.mfx.FrameInfo.Width         != 0 &&
        par.mfx.FrameInfo.Height        != 0 &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        mfxU16 minLevel = GetLevelLimitByMbps(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
            {
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
            }
            else if (par.mfx.CodecLevel != 0)
            {
                changed = true; // warning should be returned if MB processing rate exceeds maximum value supported by AVC standard
                if (par.mfx.CodecLevel < GetMaxSupportedLevel())
                    par.mfx.CodecLevel = GetMaxSupportedLevel();
            }
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (par.mfx.NumRefFrame      != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByDpbSize(par);
        if (minLevel == 0)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = false;
            par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            par.mfx.NumRefFrame = GetMaxNumRefFrame(par);
        }
        else if (par.mfx.CodecLevel != 0 && par.mfx.CodecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = false;
            par.mfx.CodecLevel = minLevel;
        }
    }

    if (IsAvcHighProfile(par.mfx.CodecProfile) && (par.mfx.CodecProfile & MASK_CONSTRAINT_SET0123_FLAG))
    {
        changed = true;
        par.mfx.CodecProfile = par.mfx.CodecProfile & ~MASK_CONSTRAINT_SET0123_FLAG;
    }

    if (!CheckTriStateOption(extOpt->RateDistortionOpt))        changed = true;
    if (!CheckTriStateOption(extOpt->EndOfSequence))            changed = true;
    if (!CheckTriStateOption(extOpt->FramePicture))             changed = true;
    if (!CheckTriStateOption(extOpt->CAVLC))                    changed = true;
    if (!CheckTriStateOption(extOpt->RefPicListReordering))     changed = true;
    if (!CheckTriStateOption(extOpt->ResetRefList))             changed = true;
    if (!CheckTriStateOption(extOpt->RefPicMarkRep))            changed = true;
    if (!CheckTriStateOption(extOpt->FieldOutput))              changed = true;
    if (!CheckTriStateOption(extOpt->AUDelimiter))              changed = true;
    if (!CheckTriStateOption(extOpt->EndOfStream))              changed = true;
    if (!CheckTriStateOption(extOpt->PicTimingSEI))             changed = true;
    if (!CheckTriStateOption(extOpt->SingleSeiNalUnit))         changed = true;
    if (!CheckTriStateOption(extOpt->NalHrdConformance))        changed = true;
    if (!CheckTriStateOption(extDdi->RefRaw))                   changed = true;
    if (!CheckTriStateOption(extDdi->DirectSpatialMvPredFlag))  changed = true;
    if (!CheckTriStateOption(extDdi->Hme))                      changed = true;
    if (!CheckTriStateOption(extOpt2->BitrateLimit))            changed = true;
    if (!CheckTriStateOption(extOpt2->MBBRC))                   changed = true;
    //if (!CheckTriStateOption(extOpt2->ExtBRC))                  changed = true;
    if (!CheckTriStateOption(extOpt2->RepeatPPS))               changed = true;
    if (!CheckTriStateOption(extOpt2->UseRawRef))               changed = true;

    if (!CheckTriStateOption(extOpt3->DirectBiasAdjustment))        changed = true;
    if (!CheckTriStateOption(extOpt3->GlobalMotionBiasAdjustment))  changed = true;

    if (    vaType != MFX_HW_VAAPI
        && (IsOn(extOpt3->DirectBiasAdjustment) || IsOn(extOpt3->GlobalMotionBiasAdjustment)))
    {
        changed = true;
        extOpt3->DirectBiasAdjustment       = MFX_CODINGOPTION_OFF;
        extOpt3->GlobalMotionBiasAdjustment = MFX_CODINGOPTION_OFF;
    }

    if (!CheckRangeDflt(extOpt3->MVCostScalingFactor, 0, 3, 0)) changed = true;

    if (extOpt3->MVCostScalingFactor && !IsOn(extOpt3->GlobalMotionBiasAdjustment))
    {
        changed = true;
        extOpt3->MVCostScalingFactor = 0;
    }

    if (extOpt2->BufferingPeriodSEI > MFX_BPSEI_IFRAME)
    {
        changed = true;
        extOpt2->BufferingPeriodSEI = 0;
    }

    if (IsMvcProfile(par.mfx.CodecProfile) && IsOn(extOpt->FieldOutput))
    {
        unsupported = true;
        extOpt->FieldOutput = MFX_CODINGOPTION_UNKNOWN;
    }

    if ((IsMvcProfile(par.mfx.CodecProfile) || IsSvcProfile(par.mfx.CodecProfile)) && par.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        unsupported = true;
        par.mfx.RateControlMethod = 0;
    }

    if (hwCaps.MBBRCSupport == 0 && hwCaps.ICQBRCSupport == 0 && IsOn(extOpt2->MBBRC))
    {
        changed = true;
        extOpt2->MBBRC = MFX_CODINGOPTION_OFF;
    }

    if (extOpt2->BRefType > 2)
    {
        changed = true;
        extOpt2->BRefType = MFX_B_REF_UNKNOWN;
    }

    if (extOpt2->BRefType && extOpt2->BRefType != MFX_B_REF_OFF &&
        par.mfx.GopRefDist != 0 && par.mfx.GopRefDist < 3)
    {
        changed = true;
        extOpt2->BRefType = MFX_B_REF_OFF;
    }

    if (extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_UNKNOWN &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_16X16 &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_8X8 &&
        extOpt->IntraPredBlockSize != MFX_BLOCKSIZE_MIN_4X4)
    {
        changed = true;
        extOpt->IntraPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
    }

    if (extOpt->InterPredBlockSize != MFX_BLOCKSIZE_UNKNOWN &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_16X16 &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_8X8 &&
        extOpt->InterPredBlockSize != MFX_BLOCKSIZE_MIN_4X4)
    {
        changed = true;
        extOpt->InterPredBlockSize = MFX_BLOCKSIZE_UNKNOWN;
    }

    if (extOpt->MVPrecision != MFX_MVPRECISION_UNKNOWN &&
        extOpt->MVPrecision != MFX_MVPRECISION_QUARTERPEL &&
        extOpt->MVPrecision != MFX_MVPRECISION_HALFPEL &&
        extOpt->MVPrecision != MFX_MVPRECISION_INTEGER)
    {
        changed = true;
        extOpt->MVPrecision = MFX_MVPRECISION_UNKNOWN;
    }

    if (extOpt->MaxDecFrameBuffering != 0)
    {
        if (par.mfx.CodecLevel       != 0 &&
            par.mfx.FrameInfo.Width  != 0 &&
            par.mfx.FrameInfo.Height != 0)
        {
            mfxU16 maxDpbSize = GetMaxNumRefFrame(par);
            if (extOpt->MaxDecFrameBuffering > maxDpbSize)
            {
                if (extBits->SPSBuffer)
                    return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                changed = true;
                extOpt->MaxDecFrameBuffering = maxDpbSize;
            }
        }

        if (par.mfx.NumRefFrame != 0)
        {
            if (extOpt->MaxDecFrameBuffering < par.mfx.NumRefFrame)
            {
                if (extBits->SPSBuffer)
                    return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                changed = true;
                extOpt->MaxDecFrameBuffering = par.mfx.NumRefFrame;
            }
        }
    }

    if ((IsOff(extOpt->CAVLC)) &&
        (IsAvcBaseProfile(par.mfx.CodecProfile) || hwCaps.NoCabacSupport))
    {
        if (extBits->SPSBuffer)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        changed = true;
        extOpt->CAVLC = MFX_CODINGOPTION_ON;
    }

    if (extOpt->IntraPredBlockSize >= MFX_BLOCKSIZE_MIN_8X8)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) || par.mfx.CodecProfile == MFX_PROFILE_AVC_MAIN)
        {
            if (extBits->PPSBuffer && extPps->transform8x8ModeFlag)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            extOpt->IntraPredBlockSize = MFX_BLOCKSIZE_MIN_16X16;
        }
    }

    if (par.calcParam.cqpHrdMode)
    {
        if (IsOn(extOpt->RecoveryPointSEI))
        {
            changed = true;
            extOpt->RecoveryPointSEI = MFX_CODINGOPTION_OFF;
        }
        if (IsOn(extOpt->PicTimingSEI))
        {
            changed = true;
            extOpt->PicTimingSEI = MFX_CODINGOPTION_OFF;
        }
        if (extOpt2->BufferingPeriodSEI != MFX_BPSEI_DEFAULT)
        {
            changed = true;
            extOpt2->BufferingPeriodSEI = MFX_BPSEI_DEFAULT;
        }
    }

    if ((IsOn(extOpt->VuiNalHrdParameters) ||IsOn(extOpt->NalHrdConformance)) &&
        par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_CBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD &&
        par.calcParam.cqpHrdMode == 0)
    {
        changed = true;
        extOpt->NalHrdConformance = MFX_CODINGOPTION_OFF; 
        extOpt->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiNalHrdParameters)
        && IsOff(extOpt->NalHrdConformance)
        && par.calcParam.cqpHrdMode == 0)
    {
        changed = true;
        extOpt->VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiVclHrdParameters) &&
        IsOff(extOpt->NalHrdConformance))
    {
        changed = true;
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (IsOn(extOpt->VuiVclHrdParameters) &&
        par.mfx.RateControlMethod != 0 &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_VBR &&
        par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR)
    {
        changed = true;
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;
    }

    if (!IsOn(extOpt2->DisableVUI) &&
        IsOff(extOpt->VuiNalHrdParameters) &&
        IsOff(extOpt->VuiVclHrdParameters) &&
        extOpt2->FixedFrameRate != MFX_CODINGOPTION_UNKNOWN &&
        extOpt3->LowDelayHrd != MFX_CODINGOPTION_UNKNOWN &&
        IsOn(extOpt2->FixedFrameRate) != IsOff(extOpt3->LowDelayHrd))
    {
        // when low_delay_hrd_flag isn't present in bitstream, it's value should be inferred to be equal to 1 - fixed_frame_rate_flag (Annex E.2.1)
        changed = true;
        if (IsOn(extOpt2->FixedFrameRate))
            extOpt3->LowDelayHrd = MFX_CODINGOPTION_OFF;
        else
            extOpt3->LowDelayHrd = MFX_CODINGOPTION_ON;
    }

    if (extDdi->WeightedBiPredIdc == 1 || extDdi->WeightedBiPredIdc > 2)
    {
        changed = true;
        extDdi->WeightedBiPredIdc = 0;
    }

    if (extDdi->CabacInitIdcPlus1 > 3)
    {
        changed = true;
        extDdi->CabacInitIdcPlus1 = 0;
    }

    if (bIntRateControlLA(par.mfx.RateControlMethod) &&
        !IsLookAheadSupported(par, platform))
    {
        unsupported = true;
        par.mfx.RateControlMethod = 0;
    }

    if (extDdi->LaScaleFactor < 2 && par.mfx.FrameInfo.Width > 4000)
    {
        extDdi->LaScaleFactor = 2;
        changed = true;
    }

    if (extDdi->LaScaleFactor > 1)
    {
        par.calcParam.widthLa = AlignValue<mfxU16>((par.mfx.FrameInfo.Width / extDdi->LaScaleFactor), 16);
        par.calcParam.heightLa = AlignValue<mfxU16>((par.mfx.FrameInfo.Height / extDdi->LaScaleFactor), 16);
    } else
    {
        par.calcParam.widthLa = par.mfx.FrameInfo.Width;
        par.calcParam.heightLa = par.mfx.FrameInfo.Height;
    }

    if (!CheckRangeDflt(extVsi->VideoFormat,             0,   8, 5)) changed = true;
    if (!CheckRangeDflt(extVsi->ColourPrimaries,         0, 255, 2)) changed = true;
    if (!CheckRangeDflt(extVsi->TransferCharacteristics, 0, 255, 2)) changed = true;
    if (!CheckRangeDflt(extVsi->MatrixCoefficients,      0, 255, 2)) changed = true;
    if (!CheckFlag(extVsi->VideoFullRange, 0))                       changed = true;
    if (!CheckFlag(extVsi->ColourDescriptionPresent, 0))             changed = true;

    if (!CheckRangeDflt(extCli->ChromaLocInfoPresentFlag,       0, 1, 0)) changed = true;
    if (!CheckRangeDflt(extCli->ChromaSampleLocTypeTopField,    0, 5, 0)) changed = true;
    if (!CheckRangeDflt(extCli->ChromaSampleLocTypeBottomField, 0, 5, 0)) changed = true;

    if (   IsOn(extOpt2->DisableVUI) && extCli->ChromaLocInfoPresentFlag
        || !extCli->ChromaLocInfoPresentFlag && (extCli->ChromaSampleLocTypeTopField || extCli->ChromaSampleLocTypeBottomField))
    {
        extCli->ChromaLocInfoPresentFlag       = 0;
        extCli->ChromaSampleLocTypeTopField    = 0;
        extCli->ChromaSampleLocTypeBottomField = 0;
        changed = true;
    }

    if (par.calcParam.cqpHrdMode == 0)
    {
        // regular check for compatibility of profile/level and BRC parameters
        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.targetKbps != 0)
        {
            if (!IsOff(extOpt2->BitrateLimit)        &&
                par.mfx.FrameInfo.Width         != 0 &&
                par.mfx.FrameInfo.Height        != 0 &&
                par.mfx.FrameInfo.FrameRateExtN != 0 &&
                par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                mfxF64 rawDataBitrate = 12.0 * par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height *
                    par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD;
                mfxU32 minTargetKbps = mfxU32(IPP_MIN(0xffffffff, rawDataBitrate / 1000 / 700));

                if (par.calcParam.targetKbps < minTargetKbps)
                {
                    changed = true;
                    par.calcParam.targetKbps = minTargetKbps;
                }
            }

            if (!IsOff(extOpt->NalHrdConformance) && extBits->SPSBuffer == 0 || IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiVclHrdParameters))
            {
                mfxU16 profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
                for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
                {
                    if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.targetKbps))
                    {
                        if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                        {
                            if (extBits->SPSBuffer)
                                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                            changed = true;
                            par.mfx.CodecLevel   = minLevel;
                            par.mfx.CodecProfile = profile;
                        }
                        break;
                    }
                }

                if (profile == MFX_PROFILE_UNKNOWN)
                {
                    if (extBits->SPSBuffer)
                        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                    {
                        par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                        par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                    }

                    changed = true;
                    par.calcParam.targetKbps = mfxU32(IPP_MIN(GetMaxBitrate(par) / 1000, UINT_MAX));
                }
            }

            if (extOpt2->MaxFrameSize != 0 &&
                par.mfx.FrameInfo.FrameRateExtN != 0 && par.mfx.FrameInfo.FrameRateExtD != 0)
            {
                mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                mfxU32 avgFrameSizeInBytes = mfxU32(par.calcParam.targetKbps * 1000 / frameRate / 8);
                if (extOpt2->MaxFrameSize < avgFrameSizeInBytes)
                {
                    changed = true;
                    extOpt2->MaxFrameSize = avgFrameSizeInBytes;
                }
            }
        }

        if (par.calcParam.targetKbps != 0 && par.calcParam.maxKbps != 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            {
                if (par.calcParam.maxKbps != par.calcParam.targetKbps)
                {
                    changed = true;
                    if (extBits->SPSBuffer && (
                        extSps->vui.flags.nalHrdParametersPresent ||
                        extSps->vui.flags.vclHrdParametersPresent))
                        par.calcParam.targetKbps = par.calcParam.maxKbps;
                    else
                        par.calcParam.maxKbps = par.calcParam.targetKbps;
                }
            }
            else if (
                par.mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                if (par.calcParam.maxKbps < par.calcParam.targetKbps)
                {
                    if (extBits->SPSBuffer && (
                        extSps->vui.flags.nalHrdParametersPresent ||
                        extSps->vui.flags.vclHrdParametersPresent))
                        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                    changed = true;
                    par.calcParam.maxKbps = par.calcParam.targetKbps;
                }
            }
        }

        if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.maxKbps != 0)
        {
            mfxU16 profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.maxKbps))
                {
                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                    {
                        if (extBits->SPSBuffer)
                            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                        changed = true;
                        par.mfx.CodecLevel = minLevel;
                        par.mfx.CodecProfile = profile;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
            {
                if (extBits->SPSBuffer)
                    return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                {
                    par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                    par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                }

                changed = true;
                par.calcParam.maxKbps = mfxU32(IPP_MIN(GetMaxBitrate(par) / 1000, UINT_MAX));
            }
        }

        if (par.calcParam.bufferSizeInKB != 0 && bRateControlLA(par.mfx.RateControlMethod) &&(par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD))
        {
            mfxU32 uncompressedSizeInKb = GetUncompressedSizeInKb(par);
            if (par.calcParam.bufferSizeInKB < uncompressedSizeInKb)
            {
                changed = true;
                par.calcParam.bufferSizeInKB = uncompressedSizeInKb;
            }
        }

        if (par.calcParam.bufferSizeInKB != 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                mfxU32 uncompressedSizeInKb = GetUncompressedSizeInKb(par);
                if (par.calcParam.bufferSizeInKB < uncompressedSizeInKb)
                {
                    changed = true;
                    par.calcParam.bufferSizeInKB = uncompressedSizeInKb;
                }
            }
            else
            {
                mfxF64 avgFrameSizeInKB = 0;
                if (par.mfx.RateControlMethod       != MFX_RATECONTROL_AVBR &&
                    par.mfx.FrameInfo.FrameRateExtN != 0 &&
                    par.mfx.FrameInfo.FrameRateExtD != 0 &&
                    par.calcParam.targetKbps        != 0)
                {
                    mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                    avgFrameSizeInKB = par.calcParam.targetKbps / frameRate / 8;

                    if (par.calcParam.bufferSizeInKB < 2 * avgFrameSizeInKB)
                    {
                        if (extBits->SPSBuffer && (
                            extSps->vui.flags.nalHrdParametersPresent ||
                            extSps->vui.flags.vclHrdParametersPresent))
                            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                        changed = true;
                        par.calcParam.bufferSizeInKB = mfxU32(2 * avgFrameSizeInKB + 1);
                    }
                }

                mfxU16 profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
                for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
                {
                    if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.bufferSizeInKB))
                    {
                        if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                        {
                            if (extBits->SPSBuffer)
                                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                            changed = true;
                            par.mfx.CodecLevel = minLevel;
                        }
                        break;
                    }
                }

                if (profile == MFX_PROFILE_UNKNOWN)
                {
                    if (extBits->SPSBuffer)
                        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                    if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                    {
                        par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                        par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                    }

                    changed = true;
                    par.calcParam.bufferSizeInKB = mfxU16(IPP_MIN(GetMaxBufferSize(par) / 8000, USHRT_MAX));
                }

                if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                    par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                    par.calcParam.initialDelayInKB != 0)
                {
                    if (par.calcParam.initialDelayInKB > par.calcParam.bufferSizeInKB)
                    {
                        changed = true;
                        par.calcParam.initialDelayInKB = par.calcParam.bufferSizeInKB / 2;
                    }

                    if (avgFrameSizeInKB != 0 && par.calcParam.initialDelayInKB < avgFrameSizeInKB)
                    {
                        changed = true;
                        par.calcParam.initialDelayInKB = mfxU16(IPP_MIN(par.calcParam.bufferSizeInKB, avgFrameSizeInKB));
                    }
                }
            }
        }
    }
    else
    {
        // special check for compatibility of profile/level and BRC parameters for cqpHrdMode
        mfxU16 profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.decorativeHrdParam.targetKbps))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel   = minLevel;
                    par.mfx.CodecProfile = profile;
                }
                break;
            }

            if (profile == MFX_PROFILE_UNKNOWN)
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
                {
                    par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                    par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
                }
            }
        }

        if (par.calcParam.decorativeHrdParam.maxKbps < par.calcParam.decorativeHrdParam.targetKbps)
        {
            changed = true;
            par.calcParam.decorativeHrdParam.maxKbps = par.calcParam.decorativeHrdParam.targetKbps;
        }

        profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.decorativeHrdParam.maxKbps))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel = minLevel;
                    par.mfx.CodecProfile = profile;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
        {
            if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
            {
                par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            }
        }

        profile = mfxU16(IPP_MAX(MFX_PROFILE_AVC_BASELINE, par.mfx.CodecProfile & MASK_PROFILE_IDC));
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.decorativeHrdParam.bufferSizeInKB))
            {
                if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0 && par.mfx.CodecLevel < minLevel)
                {
                    changed = true;
                    par.mfx.CodecLevel = minLevel;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
        {
            if (par.mfx.CodecLevel != 0 && par.mfx.CodecProfile != 0)
            {
                par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
                par.mfx.CodecLevel = MFX_LEVEL_AVC_52;
            }
            changed = true;

        }

        if (par.calcParam.decorativeHrdParam.initialDelayInKB > par.calcParam.decorativeHrdParam.bufferSizeInKB)
        {
            changed = true;
            par.calcParam.decorativeHrdParam.initialDelayInKB = par.calcParam.decorativeHrdParam.bufferSizeInKB / 2;
        }
    }

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE  && par.mfx.FrameInfo.PicStruct != 0)
    {
        if (par.mfx.CodecLevel != 0)
        {
            if (par.mfx.CodecLevel < MFX_LEVEL_AVC_21)
            {
                if (extBits->SPSBuffer) // level came from sps header, override picstruct
                    par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                else // level came from videoparam, override level
                    par.mfx.CodecLevel = MFX_LEVEL_AVC_21;
                changed = true;
            }
            else if (par.mfx.CodecLevel > MFX_LEVEL_AVC_41)
            {
                if (GetMinLevelForResolutionAndFramerate(par) <= MFX_LEVEL_AVC_41)
                {
                    // it's possible to encode stream with level lower than 4.2
                    // correct encoding parameters to satisfy H264 spec (table A-4, frame_mbs_only_flag)
                    changed = true;
                    par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                }
                else
                {
                    // it's impossible to encode stream with level lower than 4.2
                    // allow H264 spec violation ((table A-4, frame_mbs_only_flag)) and return warning
                    warning = true;
                }
            }
        }
        else
        {
            //check correct param for default codeclevel
            mfxU16  minlevel = GetMinLevelForAllParameters(par);
            if(minlevel > MFX_LEVEL_AVC_41)
            {
                if (minlevel == MFX_LEVEL_AVC_52)
                {
                    warning = true;
                }
                else
                {
                    if(par.mfx.CodecLevel != MFX_LEVEL_AVC_52)
                    {
                        changed = true;
                        par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
                    }
                }
            }
        }
    }

    if (par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH)
        {
            changed = true;
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        }
    }

    if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0 && par.mfx.NumRefFrame == 1)
    {
        par.mfx.NumRefFrame ++; // HSW and IVB don't support 1 reference frame for interlaced encoding
        changed = true;
    }

    if (IsOn(extOpt->FieldOutput))
    {
        if (par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE || IsOn(extOpt->FramePicture))
        {
            changed = true;
            extOpt->FieldOutput = MFX_CODINGOPTION_OFF;
        }
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && par.calcParam.cqpHrdMode == 0)
    {
        if (!CheckRange(par.mfx.QPI, 0, 51)) changed = true;
        if (!CheckRange(par.mfx.QPP, 0, 51)) changed = true;
        if (!CheckRange(par.mfx.QPB, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ &&
        hwCaps.ICQBRCSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VCM &&
        hwCaps.VCMBitrateControl == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR &&
        hwCaps.QVBRBRCSupport == 0)
    {
        par.mfx.RateControlMethod = 0;
        unsupported = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ || par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR &&
        extOpt2->MBBRC == MFX_CODINGOPTION_OFF)
    {
        // for ICQ or QVBR BRC mode MBBRC is ignored by driver and always treated as ON
        // need to change extOpt2->MBBRC respectively to notify application about it
        extOpt2->MBBRC = MFX_CODINGOPTION_ON;
        changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ ||
        par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        if (!CheckRange(par.mfx.ICQQuality, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        if (!CheckRange(extOpt3->QVBRQuality, 0, 51)) changed = true;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (!CheckRange(par.mfx.Accuracy,    AVBR_ACCURACY_MIN,    AVBR_ACCURACY_MAX))    changed = true;
        if (!CheckRange(par.mfx.Convergence, AVBR_CONVERGENCE_MIN, AVBR_CONVERGENCE_MAX)) changed = true;
    }

    for (mfxU32 i = 0; i < 3; i++)
    {
        mfxU32 maxTimeOffset = (1 << 24) - 1;

        mfxU32 maxNFrames = (par.mfx.FrameInfo.FrameRateExtN > 0 && par.mfx.FrameInfo.FrameRateExtD > 0)
            ? (par.mfx.FrameInfo.FrameRateExtN - 1) / par.mfx.FrameInfo.FrameRateExtD
            : 255;

        if (extPt->TimeStamp[i].CtType != 0xffff)
        {
            if (!CheckRangeDflt(extPt->TimeStamp[i].CtType, 0, 3, 2))
                changed = true;
        }

        if (!CheckRangeDflt(extPt->TimeStamp[i].CountingType, 0, 31, 0))    changed = true;
        if (!CheckRange(extPt->TimeStamp[i].NFrames, 0u, maxNFrames))       changed = true;
        if (!CheckRange(extPt->TimeStamp[i].SecondsValue, 0, 59))           changed = true;
        if (!CheckRange(extPt->TimeStamp[i].MinutesValue, 0, 59))           changed = true;
        if (!CheckRange(extPt->TimeStamp[i].HoursValue, 0, 23))             changed = true;
        if (!CheckRange(extPt->TimeStamp[i].TimeOffset, 0u, maxTimeOffset)) changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].ClockTimestampFlag, 0))         changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].NuitFieldBasedFlag, 1))         changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].FullTimestampFlag, 1))          changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].DiscontinuityFlag, 0))          changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].CntDroppedFlag, 0))             changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].SecondsFlag, 0))                changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].MinutesFlag, 0))                changed = true;
        if (!CheckFlag (extPt->TimeStamp[i].HoursFlag, 0))                  changed = true;
    }

    if (!CheckRangeDflt(extBits->SPSId, 0,  31, 0)) changed = true;
    if (!CheckRangeDflt(extBits->PPSId, 0, 255, 0)) changed = true;

    if (extBits->SPSBuffer)
    {
        if (extSps->seqParameterSetId > 31                                  ||
            !IsValidCodingProfile(extSps->profileIdc)                       ||
            !IsValidCodingLevel(extSps->levelIdc)                           ||
            extSps->chromaFormatIdc != 1                                    ||
            extSps->bitDepthLumaMinus8 != 0                                 ||
            extSps->bitDepthChromaMinus8 != 0                               ||
            extSps->qpprimeYZeroTransformBypassFlag != 0                    ||
            extSps->seqScalingMatrixPresentFlag != 0                        ||
            extSps->picOrderCntType == 1                                    ||
            /*extSps->gapsInFrameNumValueAllowedFlag != 0                     ||*/
            extSps->mbAdaptiveFrameFieldFlag != 0                           ||
            extSps->vui.flags.vclHrdParametersPresent != 0                  ||
            extSps->vui.nalHrdParameters.cpbCntMinus1 > 0                   ||
            extSps->vui.numReorderFrames > extSps->vui.maxDecFrameBuffering)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        // the following fields aren't supported by snb/ivb_win7 drivers directly and requires sps patching
        // patching is not possible whe protection is on
        if (par.Protected != 0)
            if (extSps->nalRefIdc        != 1 ||
                extSps->constraints.set0 != 0 ||
                extSps->constraints.set1 != 0 ||
                extSps->constraints.set2 != 0 ||
                extSps->constraints.set3 != 0 ||
                extSps->constraints.set4 != 0 ||
                extSps->constraints.set5 != 0 ||
                extSps->constraints.set6 != 0 ||
                extSps->constraints.set7 != 0)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

        if (extBits->PPSBuffer)
        {
            if (extPps->seqParameterSetId != extSps->seqParameterSetId ||
                extPps->numSliceGroupsMinus1 > 0                       ||
                extPps->numRefIdxL0DefaultActiveMinus1 > 31            ||
                extPps->numRefIdxL1DefaultActiveMinus1 > 31            ||
                extPps->weightedPredFlag != 0                          ||
                extPps->weightedBipredIdc == 1                         ||
                extPps->picInitQpMinus26 < -26                         ||
                extPps->picInitQpMinus26 > 25                          ||
                extPps->picInitQsMinus26 != 0                          ||
                extPps->chromaQpIndexOffset < -12                      ||
                extPps->chromaQpIndexOffset > 12                       ||
                extPps->deblockingFilterControlPresentFlag == 0        ||
                extPps->redundantPicCntPresentFlag != 0                ||
                extPps->picScalingMatrixPresentFlag != 0               ||
                extPps->secondChromaQpIndexOffset < -12                ||
                extPps->secondChromaQpIndexOffset > 12)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            // the following fields aren't supported by snb/ivb_win7 drivers directlyand requires pps patching
            // patching is not possible whe protection is on
            if (par.Protected != 0)
                if (extSps->nalRefIdc != 1)
                    return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
    }

    if (hwCaps.UserMaxFrameSizeSupport == 0 && extOpt2->MaxFrameSize)
    {
        extOpt2->MaxFrameSize = 0;
        changed = true;
    }

    if (IsMvcProfile(par.mfx.CodecProfile) && MFX_ERR_UNSUPPORTED == CheckMVCSeqDescQueryLike(extMvc))
    {
        unsupported = true;
    }

    par.SyncCalculableToVideoParam();

    if (par.calcParam.numTemporalLayer > 0 && par.mfx.EncodedOrder != 0)
    {
        changed = true;
        memset(extTemp->Layer, 0, sizeof(extTemp->Layer)); // unnamed structures can't be used in templates
        Zero(par.calcParam.scale);
        Zero(par.calcParam.tid);
        par.calcParam.numTemporalLayer = 0;
        par.calcParam.lyncMode = 0;
    }

    if (par.calcParam.lyncMode && par.mfx.GopRefDist > 1)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_VCM && par.mfx.GopRefDist > 1)
    {
        changed = true;
        par.mfx.GopRefDist = 1;
    }

    if (extTemp->BaseLayerPID + par.calcParam.numTemporalLayer > 64)
    {
        unsupported = true;
        extTemp->BaseLayerPID = 0;
    }

    if (par.calcParam.numTemporalLayer > 0 &&
        IsAvcProfile(par.mfx.CodecProfile) &&
        extTemp->Layer[0].Scale != 1)
    {
        unsupported = true;
        extTemp->Layer[0].Scale = 0;
    }

    for (mfxU32 i = 1; i < par.calcParam.numTemporalLayer; i++)
    {
        if (par.calcParam.scale[i] <= par.calcParam.scale[i - 1] || // increasing
            par.calcParam.scale[i] %  par.calcParam.scale[i - 1])   // divisible
        {
            unsupported = true;
            extTemp->Layer[par.calcParam.tid[i]].Scale = 0;
        }

        if (par.calcParam.lyncMode &&
            par.calcParam.scale[i] != par.calcParam.scale[i - 1] * 2)
        {
            unsupported = true;
            extTemp->Layer[par.calcParam.tid[i]].Scale = 0;
        }
    }

    if (hwCaps.MaxNum_QualityLayer == 0 && hwCaps.MaxNum_DependencyLayer == 0)
    {
        for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
        {
            mfxU32 did = par.calcParam.did[i];

            if (extSvc->DependencyLayer[did].BasemodePred != 0 &&
                extSvc->DependencyLayer[did].BasemodePred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].BasemodePred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
            if (extSvc->DependencyLayer[did].MotionPred != 0 &&
                extSvc->DependencyLayer[did].MotionPred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].MotionPred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
            if (extSvc->DependencyLayer[did].ResidualPred != 0 &&
                extSvc->DependencyLayer[did].ResidualPred != MFX_CODINGOPTION_OFF)
            {
                extSvc->DependencyLayer[did].ResidualPred = MFX_CODINGOPTION_OFF;
                changed = true;
            }
        }
    }

    if (extOpt2->IntRefType > 2 || (extOpt2->IntRefType && hwCaps.RollingIntraRefresh == 0))
    {
        extOpt2->IntRefType = 0;
        unsupported = true;
    }
    if((extOpt2->IntRefType == 2) &&
        (platform <= MFX_HW_BDW))
    {
        extOpt2->IntRefType = 0;
        unsupported = true;
    }

    if (extOpt2->IntRefType && par.mfx.GopRefDist > 1)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefType && par.mfx.NumRefFrame > 1 && par.calcParam.lyncMode == 0)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefType && par.calcParam.numTemporalLayer && par.calcParam.lyncMode == 0)
    {
        extOpt2->IntRefType = 0;
        changed = true;
    }

    if (extOpt2->IntRefQPDelta < -51 || extOpt2->IntRefQPDelta > 51)
    {
        extOpt2->IntRefQPDelta = 0;
        changed = true;
    }

    if (extOpt2->IntRefCycleSize != 0 &&
        par.mfx.GopPicSize != 0 &&
        extOpt2->IntRefCycleSize >= par.mfx.GopPicSize)
    {
        // refresh cycle length shouldn't be greater or equal to GOP size
        extOpt2->IntRefType = 0;
        extOpt2->IntRefCycleSize = 0;
        changed = true;
    }

    if (extOpt3->IntRefCycleDist != 0 &&
        par.mfx.GopPicSize != 0 &&
        extOpt3->IntRefCycleDist >= par.mfx.GopPicSize)
    {
        // refresh period length shouldn't be greater or equal to GOP size
        extOpt2->IntRefType = 0;
        extOpt3->IntRefCycleDist = 0;
        changed = true;
    }

    if (extOpt3->IntRefCycleDist != 0 &&
        extOpt2->IntRefCycleSize != 0 &&
        extOpt2->IntRefCycleSize > extOpt3->IntRefCycleDist)
    {
        // refresh period shouldn't be greater than refresh cycle size
        extOpt3->IntRefCycleDist = 0;
        changed = true;
    }

    if (extOpt2->Trellis & ~(MFX_TRELLIS_OFF | MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B))
    {
        extOpt2->Trellis &= (MFX_TRELLIS_OFF | MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B);
        changed = true;
    }

    if ((extOpt2->Trellis & MFX_TRELLIS_OFF) && (extOpt2->Trellis & ~MFX_TRELLIS_OFF))
    {
        extOpt2->Trellis = MFX_TRELLIS_OFF;
        changed = true;
    }

    if (extOpt2->Trellis && IsSvcProfile(par.mfx.CodecProfile))
    {
        extOpt2->Trellis = 0;
        changed = true;
    }

    /*if (extOpt2->Trellis && hwCaps.EnhancedEncInput == 0)
    {
        extOpt2->Trellis = 0;
        unsupported = true;
    }*/

    if (extOpt2->BRefType        != 0 &&
        extOpt2->BRefType        != MFX_B_REF_OFF &&
        par.mfx.CodecLevel       != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        const mfxU16 nfxMaxByLevel    = GetMaxNumRefFrame(par);
        const mfxU16 nfxMinForPyramid = (par.mfx.GopRefDist - 1) / 2 + 1;

        if (nfxMinForPyramid > nfxMaxByLevel)
        {
            // max dpb size is not enougn for pyramid
            changed = true;
            extOpt2->BRefType = MFX_B_REF_OFF;
        }
        else
        {
            if (par.mfx.NumRefFrame != 0 &&
                par.mfx.NumRefFrame < nfxMinForPyramid)
            {
                changed = true;
                par.mfx.NumRefFrame = nfxMinForPyramid;
            }
        }
    }

    if (par.calcParam.numTemporalLayer >  1 &&
        par.mfx.CodecLevel             != 0 &&
        par.mfx.FrameInfo.Width        != 0 &&
        par.mfx.FrameInfo.Height       != 0)
    {
        mfxU16 const nrfMaxByLevel     = GetMaxNumRefFrame(par);
        mfxU16 const nrfMinForTemporal = mfxU16(1 << (par.calcParam.numTemporalLayer - 2));

        if (nrfMinForTemporal > nrfMaxByLevel)
        {
            // max dpb size is not enougn for requested number of temporal layers
            changed = true;
            par.calcParam.numTemporalLayer = 1;
        }
        else
        {
            if (par.mfx.NumRefFrame != 0 &&
                par.mfx.NumRefFrame < nrfMinForTemporal)
            {
                changed = true;
                par.mfx.NumRefFrame = nrfMinForTemporal;
            }
        }
    }

    if (extRoi->NumROI && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
        hwCaps.ROIBRCPriorityLevelSupport == 0)
    {
        unsupported = true;
        extRoi->NumROI = 0;
    }

    if (extRoi->NumROI)
    {
        if (extRoi->NumROI > hwCaps.MaxNumOfROI)
        {
            if (hwCaps.MaxNumOfROI == 0)
                unsupported = true;
            else
                changed = true;

            extRoi->NumROI = hwCaps.MaxNumOfROI;
        }
    }

    for (mfxU16 i = 0; i < extRoi->NumROI; i++)
    {
        mfxStatus sts = CheckAndFixRoiQueryLike(par, (mfxRoiDesc*)(&(extRoi->ROI[i])));
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extDirtyRect->NumRect && hwCaps.DirtyRectSupport == 0)
    {
        unsupported = true;
        extDirtyRect->NumRect = 0;
    }

    for (mfxU16 i = 0; i < extDirtyRect->NumRect; i++)
    {
        mfxStatus sts = CheckAndFixRectQueryLike(par, (mfxRectDesc*)(&(extDirtyRect->Rect[i])));
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extMoveRect->NumRect && hwCaps.MoveRectSupport == 0)
    {
        unsupported = true;
        extMoveRect->NumRect = 0;
    }

    for (mfxU16 i = 0; i < extMoveRect->NumRect; i++)
    {
        mfxStatus sts = CheckAndFixMovingRectQueryLike(par, (mfxMovingRectDesc*)(&(extMoveRect->Rect[i])));
        if (sts < MFX_ERR_NONE)
            unsupported = true;
        else if (sts != MFX_ERR_NONE)
            changed = true;
    }

    if (extPwt)
    {
        mfxU16 maxLuma[2] = {};
        mfxU16 maxChroma[2] = {};
        bool out_of_caps = false;
        if (0 == hwCaps.NoWeightedPred)
        {
            if (hwCaps.LumaWeightedPred)
            {
                maxLuma[0] = std::min<mfxU16>(hwCaps.MaxNum_WeightedPredL0, 32);
                maxLuma[1] = std::min<mfxU16>(hwCaps.MaxNum_WeightedPredL1, 32);
            }
            if (hwCaps.ChromaWeightedPred)
            {
                maxChroma[0] = std::min<mfxU16>(hwCaps.MaxNum_WeightedPredL0, 32);
                maxChroma[1] = std::min<mfxU16>(hwCaps.MaxNum_WeightedPredL1, 32);
            }
        }

        for (mfxU16 lx = 0; lx < 2; lx++)
        {
            for (mfxU16 i = 0; i < 32 && !out_of_caps && extPwt->LumaWeightFlag[lx][i]; i++)
                out_of_caps = (i >= maxLuma[lx]);
            for (mfxU16 i = 0; i < 32 && !out_of_caps && extPwt->ChromaWeightFlag[lx][i]; i++)
                out_of_caps = (i >= maxChroma[lx]);
        }
        
        if (out_of_caps)
        {
            Zero(extPwt->LumaWeightFlag);
            Zero(extPwt->ChromaWeightFlag);

            for (mfxU16 lx = 0; lx < 2; lx++)
            {
                for (mfxU16 i = 0; i < maxLuma[lx]; i++)
                    extPwt->LumaWeightFlag[lx][i] = 1;
                for (mfxU16 i = 0; i < maxChroma[lx]; i++)
                    extPwt->ChromaWeightFlag[lx][i] = 1;
            }

            changed = true;
        }
    }

    if (!CheckRangeDflt(extOpt2->SkipFrame, 0, 3, 0)) changed = true;

    if ( extOpt2->SkipFrame && hwCaps.SkipFrame == 0 && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD)
    {
        extOpt2->SkipFrame = 0;
        changed = true;
    }

     if (extOpt3->WinBRCMaxAvgKbps || extOpt3->WinBRCSize)
     {
         if ((par.mfx.RateControlMethod != MFX_RATECONTROL_LA  && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_EXT))
         {
             extOpt3->WinBRCMaxAvgKbps = 0;
             extOpt3->WinBRCSize = 0;
             par.calcParam.WinBRCMaxAvgKbps = 0;
             changed = true;
         }
         else if (extOpt3->WinBRCSize==0) 
         {
             warning = true;
         }
         else if (par.calcParam.targetKbps && par.calcParam.WinBRCMaxAvgKbps < par.calcParam.targetKbps)
         {
             extOpt3->WinBRCMaxAvgKbps = 0;
             par.calcParam.WinBRCMaxAvgKbps = 0;
             extOpt3->WinBRCSize = 0;
             unsupported = true;
         }
     }

    if (   extOpt2->MinQPI || extOpt2->MaxQPI
        || extOpt2->MinQPP || extOpt2->MaxQPP
        || extOpt2->MinQPB || extOpt2->MaxQPB)
    {
        if (!CheckRangeDflt(extOpt2->MaxQPI, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MaxQPP, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MaxQPB, 0, 51, 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPI, 0, (extOpt2->MaxQPI ? extOpt2->MaxQPI : 51), 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPP, 0, (extOpt2->MaxQPP ? extOpt2->MaxQPP : 51), 0)) changed = true;
        if (!CheckRangeDflt(extOpt2->MinQPB, 0, (extOpt2->MaxQPB ? extOpt2->MaxQPB : 51), 0)) changed = true;
    }
    if (((extOpt3->WinBRCSize > 0) || (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)) && extOpt2->SkipFrame!=0 && extOpt2->SkipFrame != MFX_SKIPFRAME_INSERT_DUMMY )
    {
        extOpt2->SkipFrame = MFX_SKIPFRAME_INSERT_DUMMY;
        changed = true;
    }
    if (((extOpt3->WinBRCSize > 0) || (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)) && extOpt2->BRefType != MFX_B_REF_OFF && extOpt2->BRefType != 0 )
    {
        extOpt2->BRefType  = MFX_B_REF_OFF;
        changed = true;
    }


    if (!CheckTriStateOption(extOpt3->EnableMBQP)) changed = true;

    if (IsOn(extOpt3->EnableMBQP) && !(hwCaps.MbQpDataSupport && par.mfx.RateControlMethod == MFX_RATECONTROL_CQP))
    {
        extOpt3->EnableMBQP = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (!CheckTriStateOption(extOpt3->MBDisableSkipMap)) changed = true;

    if (IsOn(extOpt3->MBDisableSkipMap) && vaType != MFX_HW_VAAPI)
    {
        extOpt3->MBDisableSkipMap = MFX_CODINGOPTION_OFF;
        changed = true;
    }

    if (!CheckRangeDflt(extOpt2->DisableDeblockingIdc, 0, 2, 0)) changed = true;
    if (!CheckTriStateOption(extOpt2->EnableMAD)) changed = true;
    if(!((extOpt2->AdaptiveI == 0)&&(extOpt2->AdaptiveB == 0)))
    {
        if(!((extOpt2->AdaptiveI == MFX_CODINGOPTION_OFF)&&(extOpt2->AdaptiveB == MFX_CODINGOPTION_OFF)))
        {
               extOpt2->AdaptiveI = 0;
               extOpt2->AdaptiveB = 0;
               unsupported = true;
        }
    }

    if (!CheckRangeDflt(extOpt3->WeightedPred, 
            (mfxU16)MFX_WEIGHTED_PRED_UNKNOWN, 
            (mfxU16)MFX_WEIGHTED_PRED_EXPLICIT, 
            (mfxU16)MFX_WEIGHTED_PRED_DEFAULT))
            changed = true;

    if (!CheckRangeDflt(extOpt3->WeightedBiPred,
            (mfxU16)MFX_WEIGHTED_PRED_UNKNOWN,
            (mfxU16)MFX_WEIGHTED_PRED_IMPLICIT,
            (mfxU16)MFX_WEIGHTED_PRED_DEFAULT))
            changed = true;

    if (    hwCaps.NoWeightedPred
        && (extOpt3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT
        || extOpt3->WeightedBiPred == MFX_WEIGHTED_PRED_EXPLICIT))
    {
        extOpt3->WeightedPred = MFX_WEIGHTED_PRED_DEFAULT;
        extOpt3->WeightedBiPred = MFX_WEIGHTED_PRED_DEFAULT;
        unsupported = true;
    }

    if (!CheckTriStateOption(extOpt3->FadeDetection)) changed = true;

    return unsupported
        ? MFX_ERR_UNSUPPORTED
        : (changed || warning)
            ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM
            : MFX_ERR_NONE;
}

// checks MVC per-view parameters (bitrates, buffer size, initial delay, level)
mfxStatus MfxHwH264Encode::CheckVideoParamMvcQueryLike(MfxVideoParam & par)
{
    bool changed     = false;

    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps  = GetExtBuffer(par);

// first of all allign CodecLevel with general (not per-view) parameters: resolution, framerate, DPB size
    if (par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByFrameSize(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
    }

    if (extSps->vui.flags.timingInfoPresent  &&
        par.mfx.FrameInfo.Width         != 0 &&
        par.mfx.FrameInfo.Height        != 0 &&
        par.mfx.FrameInfo.FrameRateExtN != 0 &&
        par.mfx.FrameInfo.FrameRateExtD != 0)
    {
        mfxU16 minLevel = GetLevelLimitByMbps(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = true;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
    }

    if (par.mfx.NumRefFrame      != 0 &&
        par.mfx.FrameInfo.Width  != 0 &&
        par.mfx.FrameInfo.Height != 0)
    {
        mfxU16 minLevel = GetLevelLimitByDpbSize(par);
        if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
        {
            if (extBits->SPSBuffer)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            changed = false;
            par.calcParam.mvcPerViewPar.codecLevel = minLevel;
        }
    }

    // check MVC per-view parameters (bitrates, buffer size, initial delay, level)
    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.targetKbps != 0)
    {
        if (par.mfx.FrameInfo.Width         != 0 &&
            par.mfx.FrameInfo.Height        != 0 &&
            par.mfx.FrameInfo.FrameRateExtN != 0 &&
            par.mfx.FrameInfo.FrameRateExtD != 0)
        {
            mfxF64 rawDataBitrate = 12.0 * par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height *
                par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD;
            mfxU32 minTargetKbps = mfxU32(IPP_MIN(0xffffffff, rawDataBitrate / 1000 / 500));

            if (par.calcParam.mvcPerViewPar.targetKbps < minTargetKbps)
            {
                changed = true;
                par.calcParam.mvcPerViewPar.targetKbps = minTargetKbps;
            }
        }

        if (extSps->vui.flags.nalHrdParametersPresent || extSps->vui.flags.vclHrdParametersPresent)
        {
            mfxU16 profile = MFX_PROFILE_AVC_HIGH;
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.mvcPerViewPar.targetKbps))
                {
                    if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                    {
                        if (extBits->SPSBuffer)
                            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                        changed = true;
                        par.calcParam.mvcPerViewPar.codecLevel   = minLevel;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
        }
    }

    if (par.calcParam.mvcPerViewPar.targetKbps != 0 && par.calcParam.mvcPerViewPar.maxKbps != 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            if (par.calcParam.maxKbps != par.calcParam.targetKbps)
            {
                changed = true;
                if (extSps->vui.flags.nalHrdParametersPresent || extSps->vui.flags.vclHrdParametersPresent)
                    par.calcParam.mvcPerViewPar.targetKbps = par.calcParam.mvcPerViewPar.maxKbps;
                else
                    par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
            }
        }
        else if (
            par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR)
        {
            if (par.calcParam.mvcPerViewPar.maxKbps < par.calcParam.mvcPerViewPar.targetKbps)
            {
                if (extBits->SPSBuffer && (
                    extSps->vui.flags.nalHrdParametersPresent ||
                    extSps->vui.flags.vclHrdParametersPresent))
                    return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                changed = true;
                par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
            }
        }
    }

    if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP && par.calcParam.mvcPerViewPar.maxKbps != 0)
    {
        mfxU16 profile = MFX_PROFILE_AVC_HIGH;
        for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
        {
            if (mfxU16 minLevel = GetLevelLimitByMaxBitrate(profile, par.calcParam.mvcPerViewPar.maxKbps))
            {
                if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                {
                    if (extBits->SPSBuffer)
                        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                    changed = true;
                    par.calcParam.mvcPerViewPar.codecLevel = minLevel;
                }
                break;
            }
        }

        if (profile == MFX_PROFILE_UNKNOWN)
            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);
    }

    if (par.calcParam.bufferSizeInKB != 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            mfxU32 uncompressedSizeInKb = GetUncompressedSizeInKb(par);
            if (par.calcParam.bufferSizeInKB < uncompressedSizeInKb)
            {
                changed = true;
                par.calcParam.bufferSizeInKB = uncompressedSizeInKb;
            }
        }
        else
        {
            mfxF64 avgFrameSizeInKB = 0;
            if (par.mfx.RateControlMethod       != MFX_RATECONTROL_AVBR &&
                par.mfx.FrameInfo.FrameRateExtN != 0 &&
                par.mfx.FrameInfo.FrameRateExtD != 0 &&
                par.calcParam.targetKbps        != 0)
            {
                mfxF64 frameRate = mfxF64(par.mfx.FrameInfo.FrameRateExtN) / par.mfx.FrameInfo.FrameRateExtD;
                avgFrameSizeInKB = par.calcParam.mvcPerViewPar.targetKbps / frameRate / 8;

                if (par.calcParam.mvcPerViewPar.bufferSizeInKB < 2 * avgFrameSizeInKB)
                {
                    if (extBits->SPSBuffer && (
                        extSps->vui.flags.nalHrdParametersPresent ||
                        extSps->vui.flags.vclHrdParametersPresent))
                        return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                    changed = true;
                    par.calcParam.mvcPerViewPar.bufferSizeInKB = mfxU16(2 * avgFrameSizeInKB + 1);
                }
            }

            mfxU16 profile = MFX_PROFILE_AVC_HIGH;
            for (; profile != MFX_PROFILE_UNKNOWN; profile = GetNextProfile(profile))
            {
                if (mfxU16 minLevel = GetLevelLimitByBufferSize(profile, par.calcParam.mvcPerViewPar.bufferSizeInKB))
                {
                    if (par.calcParam.mvcPerViewPar.codecLevel != 0 && par.calcParam.mvcPerViewPar.codecLevel < minLevel)
                    {
                        if (extBits->SPSBuffer)
                            return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

                        changed = true;
                        par.calcParam.mvcPerViewPar.codecLevel = minLevel;
                    }
                    break;
                }
            }

            if (profile == MFX_PROFILE_UNKNOWN)
                return Error(MFX_ERR_INCOMPATIBLE_VIDEO_PARAM);

            if (par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                par.calcParam.initialDelayInKB != 0)
            {
                if (par.calcParam.mvcPerViewPar.initialDelayInKB > par.calcParam.mvcPerViewPar.bufferSizeInKB)
                {
                    changed = true;
                    par.calcParam.mvcPerViewPar.initialDelayInKB = par.calcParam.mvcPerViewPar.bufferSizeInKB / 2;
                }

                if (avgFrameSizeInKB != 0 && par.calcParam.mvcPerViewPar.initialDelayInKB < avgFrameSizeInKB)
                {
                    changed = true;
                    par.calcParam.mvcPerViewPar.initialDelayInKB = mfxU16(IPP_MIN(par.calcParam.mvcPerViewPar.bufferSizeInKB, avgFrameSizeInKB));
                }
            }
        }
    }

    return changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

// check mfxExtMVCSeqDesc as in Query
mfxStatus MfxHwH264Encode::CheckMVCSeqDescQueryLike(mfxExtMVCSeqDesc * mvcSeqDesc)
{
    bool unsupported = false;
    if (mvcSeqDesc->NumView > 0 && (mvcSeqDesc->NumView > 2 || mvcSeqDesc->NumView < 2))
    {
        unsupported = true;
        mvcSeqDesc->NumView = 0;
    }

    if (mvcSeqDesc->NumOP > 0 && mvcSeqDesc->NumOP > 1024)
    {
        unsupported = true;
        mvcSeqDesc->NumOP = 0;
    }

    if (mvcSeqDesc->NumOP > 0 && mvcSeqDesc->NumViewId > 1024 * mvcSeqDesc->NumOP)
    {
        unsupported = true;
        mvcSeqDesc->NumViewId = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0 && (mvcSeqDesc->NumViewAlloc < mvcSeqDesc->NumView))
    {
        unsupported = true;
        mvcSeqDesc->NumViewAlloc = 0;
    }

    return unsupported ? MFX_ERR_UNSUPPORTED : MFX_ERR_NONE;
}

// check mfxExtMVCSeqDesc before encoding
mfxStatus MfxHwH264Encode::CheckAndFixMVCSeqDesc(mfxExtMVCSeqDesc * mvcSeqDesc, bool isViewOutput)
{
    bool unsupported = false;
    bool changed = false;
    if (mvcSeqDesc->NumView > 2 || mvcSeqDesc->NumView < 2)
    {
        unsupported = true;
        mvcSeqDesc->NumView = 0;
    }

    if (mvcSeqDesc->NumOP > 1024)
    {
        unsupported = true;
        mvcSeqDesc->NumOP = 0;
    }

    if (mvcSeqDesc->NumOP > 0 && mvcSeqDesc->NumViewId > 1024 * mvcSeqDesc->NumOP)
    {
        unsupported = true;
        mvcSeqDesc->NumViewId = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0 && mvcSeqDesc->NumViewAlloc < mvcSeqDesc->NumView)
    {
        changed = true;
        mvcSeqDesc->NumViewAlloc = 0;
        mvcSeqDesc->View = 0;
    }

    if (mvcSeqDesc->NumViewAlloc > 0)
    {
        if (mvcSeqDesc->View == 0)
        {
            unsupported = true;
        }
        else
        {
            if (mvcSeqDesc->View[0].ViewId != 0 && isViewOutput)
            {
                 changed = true;
                mvcSeqDesc->View[0].ViewId = 0;
            }
        }
    }

    if (mvcSeqDesc->NumViewIdAlloc > 0 && mvcSeqDesc->NumViewIdAlloc < mvcSeqDesc->NumViewId)
    {
        changed = true;
        mvcSeqDesc->NumViewId = 0;
        mvcSeqDesc->NumViewIdAlloc = 0;
        mvcSeqDesc->ViewId = 0;
    }

    if (mvcSeqDesc->NumViewIdAlloc > 0)
    {
        if (mvcSeqDesc->ViewId == 0)
        {
            unsupported = true;
        }
        else
        {
            if (mvcSeqDesc->ViewId[0] != 0 && isViewOutput)
            {
                changed = true;
                mvcSeqDesc->ViewId[0] = 0;
            }
        }
    }

    if (mvcSeqDesc->NumOPAlloc > 0 &&  mvcSeqDesc->NumOPAlloc < mvcSeqDesc->NumOP)
    {
        changed = true;
        mvcSeqDesc->NumOP = 0;
        mvcSeqDesc->NumOPAlloc = 0;
        mvcSeqDesc->OP = 0;
    }

    if (mvcSeqDesc->NumOPAlloc > 0 && mvcSeqDesc->OP == 0)
    {
        unsupported = true;
    }

    return unsupported ? MFX_ERR_UNSUPPORTED :
        changed ? MFX_WRN_INCOMPATIBLE_VIDEO_PARAM : MFX_ERR_NONE;
}

void MfxHwH264Encode::InheritDefaultValues(
    MfxVideoParam const & parInit,
    MfxVideoParam &       parReset)
{
    InheritOption(parInit.AsyncDepth,             parReset.AsyncDepth);
    InheritOption(parInit.mfx.BRCParamMultiplier, parReset.mfx.BRCParamMultiplier);
    InheritOption(parInit.mfx.CodecId,            parReset.mfx.CodecId);
    InheritOption(parInit.mfx.CodecProfile,       parReset.mfx.CodecProfile);
    InheritOption(parInit.mfx.CodecLevel,         parReset.mfx.CodecLevel);
    InheritOption(parInit.mfx.NumThread,          parReset.mfx.NumThread);
    InheritOption(parInit.mfx.TargetUsage,        parReset.mfx.TargetUsage);
    InheritOption(parInit.mfx.GopPicSize,         parReset.mfx.GopPicSize);
    InheritOption(parInit.mfx.GopRefDist,         parReset.mfx.GopRefDist);
    InheritOption(parInit.mfx.GopOptFlag,         parReset.mfx.GopOptFlag);
    InheritOption(parInit.mfx.IdrInterval,        parReset.mfx.IdrInterval);
    InheritOption(parInit.mfx.RateControlMethod,  parReset.mfx.RateControlMethod);
    InheritOption(parInit.mfx.BufferSizeInKB,     parReset.mfx.BufferSizeInKB);
    InheritOption(parInit.mfx.NumSlice,           parReset.mfx.NumSlice);
    InheritOption(parInit.mfx.NumRefFrame,        parReset.mfx.NumRefFrame);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_CQP && parReset.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
    {
        InheritOption(parInit.mfx.QPI, parReset.mfx.QPI);
        InheritOption(parInit.mfx.QPP, parReset.mfx.QPP);
        InheritOption(parInit.mfx.QPB, parReset.mfx.QPB);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_AVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        InheritOption(parInit.mfx.Accuracy,    parReset.mfx.Accuracy);
        InheritOption(parInit.mfx.Convergence, parReset.mfx.Convergence);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_ICQ && parReset.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        InheritOption(parInit.mfx.ICQQuality, parReset.mfx.ICQQuality);
    }

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_VCM && parReset.mfx.RateControlMethod == MFX_RATECONTROL_VCM)
    {
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
    }


    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.FourCC,         parReset.mfx.FrameInfo.FourCC);
    InheritOption(parInit.mfx.FrameInfo.Width,          parReset.mfx.FrameInfo.Width);
    InheritOption(parInit.mfx.FrameInfo.Height,         parReset.mfx.FrameInfo.Height);
    InheritOption(parInit.mfx.FrameInfo.CropX,          parReset.mfx.FrameInfo.CropX);
    InheritOption(parInit.mfx.FrameInfo.CropY,          parReset.mfx.FrameInfo.CropY);
    InheritOption(parInit.mfx.FrameInfo.CropW,          parReset.mfx.FrameInfo.CropW);
    InheritOption(parInit.mfx.FrameInfo.CropH,          parReset.mfx.FrameInfo.CropH);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtN,  parReset.mfx.FrameInfo.FrameRateExtN);
    InheritOption(parInit.mfx.FrameInfo.FrameRateExtD,  parReset.mfx.FrameInfo.FrameRateExtD);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioW,   parReset.mfx.FrameInfo.AspectRatioW);
    InheritOption(parInit.mfx.FrameInfo.AspectRatioH,   parReset.mfx.FrameInfo.AspectRatioH);

    mfxExtCodingOption const * extOptInit   = GetExtBuffer(parInit);
    mfxExtCodingOption *       extOptReset  = GetExtBuffer(parReset);

    InheritOption(extOptInit->RateDistortionOpt,     extOptReset->RateDistortionOpt);
    InheritOption(extOptInit->MECostType,            extOptReset->MECostType);
    InheritOption(extOptInit->MESearchType,          extOptReset->MESearchType);
    InheritOption(extOptInit->MVSearchWindow.x,      extOptReset->MVSearchWindow.x);
    InheritOption(extOptInit->MVSearchWindow.y,      extOptReset->MVSearchWindow.y);
    InheritOption(extOptInit->EndOfSequence,         extOptReset->EndOfSequence);
    InheritOption(extOptInit->FramePicture,          extOptReset->FramePicture);
    InheritOption(extOptInit->CAVLC,                 extOptReset->CAVLC);
    InheritOption(extOptInit->NalHrdConformance,     extOptReset->NalHrdConformance);
    InheritOption(extOptInit->SingleSeiNalUnit,      extOptReset->SingleSeiNalUnit);
    InheritOption(extOptInit->VuiVclHrdParameters,   extOptReset->VuiVclHrdParameters);
    InheritOption(extOptInit->RefPicListReordering,  extOptReset->RefPicListReordering);
    InheritOption(extOptInit->ResetRefList,          extOptReset->ResetRefList);
    InheritOption(extOptInit->RefPicMarkRep,         extOptReset->RefPicMarkRep);
    InheritOption(extOptInit->FieldOutput,           extOptReset->FieldOutput);
    InheritOption(extOptInit->IntraPredBlockSize,    extOptReset->IntraPredBlockSize);
    InheritOption(extOptInit->InterPredBlockSize,    extOptReset->InterPredBlockSize);
    InheritOption(extOptInit->MVPrecision,           extOptReset->MVPrecision);
    InheritOption(extOptInit->MaxDecFrameBuffering,  extOptReset->MaxDecFrameBuffering);
    InheritOption(extOptInit->AUDelimiter,           extOptReset->AUDelimiter);
    InheritOption(extOptInit->EndOfStream,           extOptReset->EndOfStream);
    InheritOption(extOptInit->PicTimingSEI,          extOptReset->PicTimingSEI);
    InheritOption(extOptInit->VuiNalHrdParameters,   extOptReset->VuiNalHrdParameters);

    mfxExtCodingOption2 const * extOpt2Init  = GetExtBuffer(parInit);
    mfxExtCodingOption2 *       extOpt2Reset = GetExtBuffer(parReset);

    InheritOption(extOpt2Init->DisableVUI,      extOpt2Reset->DisableVUI);
    InheritOption(extOpt2Init->IntRefType,      extOpt2Reset->IntRefType);
    InheritOption(extOpt2Init->IntRefCycleSize, extOpt2Reset->IntRefCycleSize);
    InheritOption(extOpt2Init->SkipFrame,       extOpt2Reset->SkipFrame);

    mfxExtCodingOption3 const * extOpt3Init  = GetExtBuffer(parInit);
    mfxExtCodingOption3 *       extOpt3Reset = GetExtBuffer(parReset);

    InheritOption(extOpt3Init->NumSliceI, extOpt3Reset->NumSliceI);
    InheritOption(extOpt3Init->NumSliceP, extOpt3Reset->NumSliceP);
    InheritOption(extOpt3Init->NumSliceB, extOpt3Reset->NumSliceB);
    InheritOption(extOpt3Init->IntRefCycleDist, extOpt3Reset->IntRefCycleDist);

    if (parInit.mfx.RateControlMethod == MFX_RATECONTROL_QVBR && parReset.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        InheritOption(parInit.mfx.InitialDelayInKB, parReset.mfx.InitialDelayInKB);
        InheritOption(parInit.mfx.TargetKbps,       parReset.mfx.TargetKbps);
        InheritOption(parInit.mfx.MaxKbps,          parReset.mfx.MaxKbps);
        InheritOption(extOpt3Init->QVBRQuality,     extOpt3Reset->QVBRQuality);
    }

    // special encoding modes should be continued after Reset (if new parameters allow this)
    mfxExtSpecialEncodingModes const * extSpecModesInit  = GetExtBuffer(parInit);
    mfxExtSpecialEncodingModes       * extSpecModesReset = GetExtBuffer(parReset);
    // NumRefFrame can't be increased by Reset
    // so WiDi WA for dummy frames is continued after Reset() w/o additional checks
    InheritOption(extSpecModesInit->refDummyFramesForWiDi, extSpecModesReset->refDummyFramesForWiDi);

    parReset.SyncVideoToCalculableParam();

    // not inherited:
    // InheritOption(parInit.mfx.FrameInfo.PicStruct,      parReset.mfx.FrameInfo.PicStruct);
    // InheritOption(parInit.IOPattern,                    parReset.IOPattern);
    // InheritOption(parInit.mfx.FrameInfo.ChromaFormat,   parReset.mfx.FrameInfo.ChromaFormat);
}


namespace
{
    bool IsDyadic(mfxU32 tempScales[8], mfxU32 numTempLayers)
    {
        if (numTempLayers > 0)
            for (mfxU32 i = 1; i < numTempLayers; ++i)
                if (tempScales[i] != 2 * tempScales[i - 1])
                    return false;
        return true;
    }

    bool IsPowerOf2(mfxU32 n)
    {
        return (n & (n - 1)) == 0;
    }
};

bool IsHRDBasedBRCMethod(mfxU16  RateControlMethod)
{
    return  RateControlMethod != MFX_RATECONTROL_CQP && RateControlMethod != MFX_RATECONTROL_AVBR &&
            RateControlMethod != MFX_RATECONTROL_ICQ &&
            RateControlMethod != MFX_RATECONTROL_LA && RateControlMethod != MFX_RATECONTROL_LA_ICQ;
}


void MfxHwH264Encode::SetDefaults(
    MfxVideoParam &     par,
    ENCODE_CAPS const & hwCaps,
    bool                setExtAlloc,
    eMFXHWType          platform,
    eMFXVAType          vaType)
{
    mfxExtCodingOption *       extOpt  = GetExtBuffer(par);
    mfxExtCodingOption2 *      extOpt2 = GetExtBuffer(par);
    mfxExtCodingOption3 *      extOpt3 = GetExtBuffer(par);
    mfxExtCodingOptionDDI *    extDdi  = GetExtBuffer(par);
    mfxExtPAVPOption *         extPavp = GetExtBuffer(par);
    mfxExtVideoSignalInfo *    extVsi  = GetExtBuffer(par);
    mfxExtCodingOptionSPSPPS * extBits = GetExtBuffer(par);
    mfxExtSpsHeader *          extSps  = GetExtBuffer(par);
    mfxExtPpsHeader *          extPps  = GetExtBuffer(par);
    mfxExtSVCSeqDesc *         extSvc  = GetExtBuffer(par);
    mfxExtSVCRateControl *     extRc   = GetExtBuffer(par);
    mfxExtChromaLocInfo*       extCli  = GetExtBuffer(par);

    if (extOpt2->UseRawRef)
        extDdi->RefRaw = extOpt2->UseRawRef;
    else if (!extOpt2->UseRawRef)
        extOpt2->UseRawRef = extDdi->RefRaw;

    if (extOpt2->UseRawRef == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->UseRawRef = MFX_CODINGOPTION_OFF;

#if defined(LOWPOWERENCODE_AVC)
    if (IsOn(par.mfx.LowPower))
    {
        if (par.mfx.GopRefDist == 0)
            par.mfx.GopRefDist = 1;
        if (par.mfx.FrameInfo.PicStruct == 0)
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }
#endif

    if (extOpt2->MaxSliceSize)
    {
        if (par.mfx.GopRefDist == 0)
            par.mfx.GopRefDist = 1;
        if (par.mfx.RateControlMethod == 0)
            par.mfx.RateControlMethod = MFX_RATECONTROL_LA;
        if (extOpt2->LookAheadDepth == 0)
            extOpt2->LookAheadDepth = 1;
        if (par.mfx.FrameInfo.PicStruct  == 0)
            par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        if (extDdi->LaScaleFactor == 0)
            extDdi->LaScaleFactor = 2;
        if (par.AsyncDepth == 0)
            par.AsyncDepth = 1;
    }

    if (IsProtectionPavp(par.Protected))
    {
        if (extPavp->EncryptionType == 0)
            extPavp->EncryptionType = MFX_PAVP_AES128_CTR;

        if (extPavp->CounterType == 0)
            extPavp->CounterType = MFX_PAVP_CTR_TYPE_A;

        if (extPavp->CounterIncrement == 0)
            extPavp->CounterIncrement = 0xC000;
    }

    mfxU8 fieldCodingPossible = IsFieldCodingPossible(par);

    if (par.IOPattern == 0)
        par.IOPattern = setExtAlloc
            ? mfxU16(MFX_IOPATTERN_IN_VIDEO_MEMORY)
            : mfxU16(MFX_IOPATTERN_IN_SYSTEM_MEMORY);

    if (par.AsyncDepth == 0)
        par.AsyncDepth = GetDefaultAsyncDepth(par);

    if (par.mfx.TargetUsage == 0)
        par.mfx.TargetUsage = 4;

    if (par.mfx.NumSlice == 0)
    {
        if (extOpt2->NumMbPerSlice != 0){
            par.mfx.NumSlice = (
                (par.mfx.FrameInfo.Width / 16) *
                (par.mfx.FrameInfo.Height / 16 / (fieldCodingPossible ? 2 : 1)) +
                extOpt2->NumMbPerSlice - 1) / extOpt2->NumMbPerSlice;
        }
        else
            par.mfx.NumSlice = 1;
    }

    if (par.mfx.RateControlMethod == 0)
        par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR)
    {
        if (par.mfx.Accuracy == 0)
            par.mfx.Accuracy = AVBR_ACCURACY_MAX;

        if (par.mfx.Convergence == 0)
            par.mfx.Convergence = AVBR_CONVERGENCE_MAX;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ || par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
    {
        if (par.mfx.ICQQuality == 0)
            par.mfx.ICQQuality = 26;
    }

    if (par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR)
    {
        if (extOpt3->QVBRQuality == 0)
            extOpt3->QVBRQuality = 26;
    }

    if (par.mfx.GopRefDist == 0)
    {
        if (IsAvcBaseProfile(par.mfx.CodecProfile) ||
            par.mfx.CodecProfile == MFX_PROFILE_AVC_CONSTRAINED_HIGH ||
            par.calcParam.numTemporalLayer > 0 && par.calcParam.lyncMode ||
            hwCaps.SliceIPOnly)
        {
            par.mfx.GopRefDist = 1;
        }
        else if (par.calcParam.numTemporalLayer > 1)
        { // svc temporal layers
            par.mfx.GopRefDist = 1;
            extOpt2->BRefType = MFX_B_REF_OFF;

            if (IsDyadic(par.calcParam.scale, par.calcParam.numTemporalLayer) &&
                par.mfx.GopPicSize % par.calcParam.scale[par.calcParam.numTemporalLayer - 1] == 0)
            {
                if (par.calcParam.numTemporalLayer == 2)
                {
                    par.mfx.GopRefDist = 2;
                    extOpt2->BRefType = MFX_B_REF_OFF;
                }
                else
                {
                    par.mfx.GopRefDist = 4;
                    extOpt2->BRefType = MFX_B_REF_PYRAMID;
                }
            }
        }
        else
        {
            par.mfx.GopRefDist = GetDefaultGopRefDist(par.mfx.TargetUsage, platform);
            if (par.mfx.GopPicSize > 0 && par.mfx.GopPicSize <= par.mfx.GopRefDist)
                par.mfx.GopRefDist = par.mfx.GopPicSize;
        }

        if (par.mfx.RateControlMethod & MFX_RATECONTROL_LA_EXT)
        {
            par.mfx.GopRefDist = 3;
        }
    }
    if ((par.mfx.RateControlMethod & ( MFX_RATECONTROL_LA |MFX_RATECONTROL_LA_HRD|MFX_RATECONTROL_LA_EXT)) && (extOpt3->WinBRCMaxAvgKbps || extOpt3->WinBRCSize))
    {
      if (!extOpt3->WinBRCMaxAvgKbps)
      {
          extOpt3->WinBRCMaxAvgKbps = par.mfx.TargetKbps*2;
          par.calcParam.WinBRCMaxAvgKbps = par.calcParam.targetKbps*2;
      }
      if (!extOpt3->WinBRCSize)
      {
          extOpt3->WinBRCSize = (mfxU16)(par.mfx.FrameInfo.FrameRateExtN / par.mfx.FrameInfo.FrameRateExtD);
          extOpt3->WinBRCSize = (!extOpt3->WinBRCSize) ? 30 : extOpt3->WinBRCSize;
      }
    }
    
    if ( (!extOpt2->SkipFrame) && ((extOpt3->WinBRCSize >= 0) || par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD))
    {
        extOpt2->SkipFrame = MFX_SKIPFRAME_INSERT_DUMMY;
    }

    //FIXME: WA for MVC quality problem on progressive content.
    if(IsMvcProfile(par.mfx.CodecProfile)){
        extDdi->NumActiveRefP = extDdi->NumActiveRefBL0 = extDdi->NumActiveRefBL1 = 1;
    }
    if (extDdi->NumActiveRefP == 0)
        extDdi->NumActiveRefP = GetDefaultMaxNumRefActivePL0(par.mfx.TargetUsage, platform,IsOn(par.mfx.LowPower));

    if (par.mfx.GopRefDist > 1)
    {
        if (extDdi->NumActiveRefBL0 == 0)
            extDdi->NumActiveRefBL0 = GetDefaultMaxNumRefActiveBL0(par.mfx.TargetUsage, platform);
        if (extDdi->NumActiveRefBL1 == 0)
            extDdi->NumActiveRefBL1 = GetDefaultMaxNumRefActiveBL1(par.mfx.TargetUsage, platform);
    }

    if (par.mfx.GopPicSize == 0)
    {
        mfxU32 maxScale = (par.calcParam.numTemporalLayer) > 0
            ? par.calcParam.scale[par.calcParam.numTemporalLayer - 1]
            : 1;
        par.mfx.GopPicSize = mfxU16((256 + maxScale - 1) / maxScale * maxScale);
    }

    if (extOpt2->BRefType == MFX_B_REF_UNKNOWN)
    {
        assert(par.mfx.GopRefDist > 0);
        assert(par.mfx.GopPicSize > 0);

        if (platform >= MFX_HW_HSW && platform != MFX_HW_VLV &&
            IsDyadic(par.calcParam.scale, par.calcParam.numTemporalLayer) &&
            par.mfx.GopRefDist >= 4 &&
            IsPowerOf2(par.mfx.GopRefDist) &&
            par.mfx.GopPicSize % par.mfx.GopRefDist == 0 &&
            !bRateControlLA(par.mfx.RateControlMethod))
        {
            extOpt2->BRefType = mfxU16(par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ? MFX_B_REF_PYRAMID : MFX_B_REF_OFF);
        }
        else
        {
            extOpt2->BRefType = MFX_B_REF_OFF;
        }
    }

    if (par.mfx.FrameInfo.FourCC == 0)
        par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

    if (par.mfx.FrameInfo.ChromaFormat == 0)
        par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;

    if (par.mfx.FrameInfo.CropW == 0)
        par.mfx.FrameInfo.CropW = par.mfx.FrameInfo.Width - par.mfx.FrameInfo.CropX;

    if (par.mfx.FrameInfo.CropH == 0)
        par.mfx.FrameInfo.CropH = par.mfx.FrameInfo.Height - par.mfx.FrameInfo.CropY;

    if (par.mfx.FrameInfo.AspectRatioW == 0)
        par.mfx.FrameInfo.AspectRatioW = 1;

    if (par.mfx.FrameInfo.AspectRatioH == 0)
        par.mfx.FrameInfo.AspectRatioH = 1;

    if (extOpt->InterPredBlockSize == MFX_BLOCKSIZE_UNKNOWN)
        extOpt->InterPredBlockSize = MFX_BLOCKSIZE_MIN_4X4;

    if (extOpt->MVPrecision == MFX_MVPRECISION_UNKNOWN)
        extOpt->MVPrecision = MFX_MVPRECISION_QUARTERPEL;

    if (extOpt->RateDistortionOpt == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RateDistortionOpt = MFX_CODINGOPTION_OFF;

    if (extOpt->RateDistortionOpt == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RateDistortionOpt = MFX_CODINGOPTION_OFF;

    if (extOpt->EndOfSequence == MFX_CODINGOPTION_UNKNOWN)
        extOpt->EndOfSequence = MFX_CODINGOPTION_OFF;

    if (extOpt->RefPicListReordering == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RefPicListReordering = MFX_CODINGOPTION_OFF;

    if (extOpt->ResetRefList == MFX_CODINGOPTION_UNKNOWN)
        extOpt->ResetRefList = MFX_CODINGOPTION_OFF;

    if (extOpt->RefPicMarkRep == MFX_CODINGOPTION_UNKNOWN)
        extOpt->RefPicMarkRep = MFX_CODINGOPTION_OFF;

    if (extOpt->FieldOutput == MFX_CODINGOPTION_UNKNOWN)
        extOpt->FieldOutput = MFX_CODINGOPTION_OFF;

    if (extOpt->AUDelimiter == MFX_CODINGOPTION_UNKNOWN)
        extOpt->AUDelimiter = (par.calcParam.lyncMode)
            ? mfxU16(MFX_CODINGOPTION_OFF)
            : mfxU16(MFX_CODINGOPTION_ON);

    if (extOpt->EndOfStream == MFX_CODINGOPTION_UNKNOWN)
        extOpt->EndOfStream = MFX_CODINGOPTION_OFF;

    if (extOpt->PicTimingSEI == MFX_CODINGOPTION_UNKNOWN)
        extOpt->PicTimingSEI = MFX_CODINGOPTION_ON;

    if (extOpt->ViewOutput == MFX_CODINGOPTION_UNKNOWN)
        extOpt->ViewOutput = MFX_CODINGOPTION_OFF;

    if (extOpt->VuiNalHrdParameters == MFX_CODINGOPTION_UNKNOWN)
        extOpt->VuiNalHrdParameters =
            par.mfx.RateControlMethod == MFX_RATECONTROL_CQP ||
            par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR ||
            (bRateControlLA(par.mfx.RateControlMethod) && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD)||
            IsOn(extOpt2->DisableVUI)
                ? mfxU16(MFX_CODINGOPTION_OFF)
                : mfxU16(MFX_CODINGOPTION_ON);

    if (extOpt->VuiVclHrdParameters == MFX_CODINGOPTION_UNKNOWN)
        extOpt->VuiVclHrdParameters = MFX_CODINGOPTION_OFF;

    if (extOpt->NalHrdConformance == MFX_CODINGOPTION_UNKNOWN)
        extOpt->NalHrdConformance = IsOn(extOpt->VuiNalHrdParameters) || IsOn(extOpt->VuiNalHrdParameters)
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

    if (extOpt->CAVLC == MFX_CODINGOPTION_UNKNOWN)
        extOpt->CAVLC = (IsAvcBaseProfile(par.mfx.CodecProfile) || hwCaps.NoCabacSupport)
            ? mfxU16(MFX_CODINGOPTION_ON)
            : mfxU16(MFX_CODINGOPTION_OFF);

    if (extOpt->SingleSeiNalUnit == MFX_CODINGOPTION_UNKNOWN)
        extOpt->SingleSeiNalUnit = MFX_CODINGOPTION_ON;

    if (extOpt->NalHrdConformance == MFX_CODINGOPTION_UNKNOWN)
        extOpt->NalHrdConformance = MFX_CODINGOPTION_ON;

    if (extDdi->MEInterpolationMethod == ENC_INTERPOLATION_TYPE_NONE)
        extDdi->MEInterpolationMethod = ENC_INTERPOLATION_TYPE_AVC6TAP;

    if (extDdi->RefRaw == MFX_CODINGOPTION_UNKNOWN)
        extDdi->RefRaw = MFX_CODINGOPTION_OFF;

    if (extDdi->DirectSpatialMvPredFlag == MFX_CODINGOPTION_UNKNOWN)
        extDdi->DirectSpatialMvPredFlag = MFX_CODINGOPTION_ON;

    if (extDdi->Hme == MFX_CODINGOPTION_UNKNOWN)
        extDdi->Hme = MFX_CODINGOPTION_ON;

    //if (extOpt2->ExtBRC == MFX_CODINGOPTION_UNKNOWN)
    //    extOpt2->ExtBRC = MFX_CODINGOPTION_OFF;

    if (extOpt2->LookAheadDepth == 0)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_LA_ICQ)
            extOpt2->LookAheadDepth = IPP_MAX(10, 2 * par.mfx.GopRefDist);
        else if (bRateControlLA(par.mfx.RateControlMethod)||par.mfx.RateControlMethod == MFX_RATECONTROL_VME)
            extOpt2->LookAheadDepth = IPP_MAX(40, 2 * par.mfx.GopRefDist);

    }
    if (extDdi->LookAheadDependency == 0)
        extDdi->LookAheadDependency = (bIntRateControlLA(par.mfx.RateControlMethod) && par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
            ? IPP_MIN(10, extOpt2->LookAheadDepth / 2)
            : extOpt2->LookAheadDepth;

    if (extDdi->LaScaleFactor == 0) // default: use LA 2X for TU3-7 and LA 1X for TU1-2
        if (par.mfx.TargetUsage > 2 || par.mfx.FrameInfo.Width >= 4000)
            extDdi->LaScaleFactor = 2;
        else
            extDdi->LaScaleFactor = 1;

    if ((extDdi->LaScaleFactor != 1) &&
        (extDdi->LaScaleFactor != 2) &&
        (extDdi->LaScaleFactor != 4))
        extDdi->LaScaleFactor = 1;

    if (extDdi->QpUpdateRange == 0)
        extDdi->QpUpdateRange = 10;

    if (extDdi->RegressionWindow == 0)
        extDdi->RegressionWindow = 20;

    if (extOpt2->BitrateLimit == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->BitrateLimit = MFX_CODINGOPTION_ON;

    if (extOpt2->RepeatPPS == MFX_CODINGOPTION_UNKNOWN)
        extOpt2->RepeatPPS = MFX_CODINGOPTION_ON;

    extDdi->MaxMVs = 32;
    extDdi->SkipCheck = 1;
    extDdi->DirectCheck = 1;
    extDdi->BiDirSearch = 1;
    extDdi->FieldPrediction = fieldCodingPossible;
    extDdi->MVPrediction = 1;

    if (extDdi->StrengthN == 0)
        extDdi->StrengthN = 220;

    if (par.mfx.NumRefFrame == 0)
    {
        mfxU16 const nrfMin             = (par.mfx.GopRefDist > 1 ? 2 : 1);
        mfxU16 const nrfDefault         = IPP_MAX(nrfMin, GetDefaultNumRefFrames(par.mfx.TargetUsage));
        mfxU16 const nrfMaxByCaps       = IPP_MIN(IPP_MAX(1, hwCaps.MaxNum_Reference), 8) * 2;
        mfxU16 const nrfMaxByLevel      = GetMaxNumRefFrame(par);
        mfxU16 const nrfMinForPyramid   = nrfMin + (par.mfx.GopRefDist - 1) / 2;
        mfxU16 const nrfMinForTemporal  = mfxU16(nrfMin + par.calcParam.numTemporalLayer - 1);
        mfxU16 const nrfMinForInterlace = 2; // HSW and IVB don't support 1 reference frame for interlace

        if (par.calcParam.numTemporalLayer > 1)
            par.mfx.NumRefFrame = nrfMinForTemporal;
        else if (extOpt2->BRefType != MFX_B_REF_OFF)
            par.mfx.NumRefFrame = nrfMinForPyramid;
        else if (extOpt2->IntRefType)
            par.mfx.NumRefFrame = 1;
        else if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
            par.mfx.NumRefFrame = IPP_MIN(IPP_MIN(IPP_MAX(nrfDefault, nrfMinForInterlace), nrfMaxByLevel), nrfMaxByCaps);
        else
            par.mfx.NumRefFrame = IPP_MIN(IPP_MIN(nrfDefault, nrfMaxByLevel), nrfMaxByCaps);

#if defined(LOWPOWERENCODE_AVC)
        if (IsOn(par.mfx.LowPower))
        {
            par.mfx.NumRefFrame = IPP_MIN(hwCaps.MaxNum_Reference, par.mfx.NumRefFrame);
        }
#endif
    }

    if (extOpt->IntraPredBlockSize == MFX_BLOCKSIZE_UNKNOWN)
        extOpt->IntraPredBlockSize = GetDefaultIntraPredBlockSize(par, platform);

    if (par.mfx.CodecProfile == MFX_PROFILE_UNKNOWN)
    {
        par.mfx.CodecProfile = MFX_PROFILE_AVC_BASELINE;

        if ((par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_TFF) ||
            (par.mfx.FrameInfo.PicStruct & MFX_PICSTRUCT_FIELD_BFF))
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (par.mfx.GopRefDist > 1)
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (IsOff(extOpt->CAVLC))
            par.mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;

        if (extOpt->IntraPredBlockSize >= MFX_BLOCKSIZE_MIN_8X8)
            par.mfx.CodecProfile = MFX_PROFILE_AVC_HIGH;
    }

    if (par.mfx.CodecLevel == MFX_LEVEL_UNKNOWN)
        par.mfx.CodecLevel = MFX_LEVEL_AVC_1;

    if (extOpt2->BufferingPeriodSEI == MFX_BPSEI_DEFAULT &&
        extOpt->RecoveryPointSEI == MFX_CODINGOPTION_ON &&
        extOpt2->IntRefType == 0)
    {
        extOpt2->BufferingPeriodSEI = MFX_BPSEI_IFRAME;
    }

    SetDefaultOff(extOpt2->DisableVUI);
    SetDefaultOn(extOpt3->AspectRatioInfoPresent);
    SetDefaultOff(extOpt3->OverscanInfoPresent);
    SetDefaultOn(extOpt3->TimingInfoPresent);
    SetDefaultOn(extOpt2->FixedFrameRate);
    SetDefaultOff(extOpt3->LowDelayHrd);
    SetDefaultOn(extOpt3->BitstreamRestriction);
    SetDefaultOff(extOpt->RecoveryPointSEI);

    CheckVideoParamQueryLike(par, hwCaps, platform, vaType);

    if (extOpt3->NumSliceI == 0 && extOpt3->NumSliceP == 0 && extOpt3->NumSliceB == 0)
        extOpt3->NumSliceI = extOpt3->NumSliceP = extOpt3->NumSliceB = par.mfx.NumSlice;

    if (extOpt2->MBBRC == MFX_CODINGOPTION_UNKNOWN)
    {
        if (par.mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        {
            // for ICQ BRC mode MBBRC is ignored by driver and always treated as ON
            // need to change extOpt2->MBBRC respectively to notify application about it
            extOpt2->MBBRC = MFX_CODINGOPTION_ON;
        }
        else
        {
            // turn off MBBRC by default
            extOpt2->MBBRC = MFX_CODINGOPTION_OFF;
        }
    }

    if (extOpt2->IntRefType && extOpt2->IntRefCycleSize == 0)
    {
        // set intra refresh cycle to 1 sec by default
        extOpt2->IntRefCycleSize =
            (mfxU16)((par.mfx.FrameInfo.FrameRateExtN + par.mfx.FrameInfo.FrameRateExtD - 1) / par.mfx.FrameInfo.FrameRateExtD);
    }

    if (par.calcParam.mvcPerViewPar.codecLevel == MFX_LEVEL_UNKNOWN)
        par.calcParam.mvcPerViewPar.codecLevel = MFX_LEVEL_AVC_1;

    if (extOpt3->EnableMBQP == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->EnableMBQP = MFX_CODINGOPTION_OFF;

    if (extOpt3->MBDisableSkipMap == MFX_CODINGOPTION_UNKNOWN)
        extOpt3->MBDisableSkipMap = MFX_CODINGOPTION_OFF;

    CheckVideoParamMvcQueryLike(par);

    if (par.calcParam.cqpHrdMode == 0)
    {
        if (par.calcParam.maxKbps == 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                mfxU32 maxBps = par.calcParam.targetKbps * MAX_BITRATE_RATIO;
                if (IsOn(extOpt->NalHrdConformance) ||
                    IsOn(extOpt->VuiVclHrdParameters))
                    maxBps = IPP_MIN(maxBps, GetMaxBitrate(par));

                par.calcParam.maxKbps = mfxU32(IPP_MIN(maxBps / 1000, UINT_MAX));
                assert(par.calcParam.maxKbps >= par.calcParam.targetKbps);
            }
            else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            {
                par.calcParam.maxKbps = par.calcParam.targetKbps;
            }
        }

        if (par.calcParam.mvcPerViewPar.maxKbps == 0)
        {
            if (par.mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_WIDI_VBR ||
                par.mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD)
            {
                mfxU32 maxBps = par.calcParam.mvcPerViewPar.targetKbps * MAX_BITRATE_RATIO;
                if (IsOn(extOpt->NalHrdConformance) ||
                    IsOn(extOpt->VuiVclHrdParameters))
                    maxBps = IPP_MIN(maxBps, GetMaxPerViewBitrate(par));

                par.calcParam.mvcPerViewPar.maxKbps = mfxU32(IPP_MIN(maxBps / 1000, UINT_MAX));
                assert(par.calcParam.mvcPerViewPar.maxKbps >= par.calcParam.mvcPerViewPar.targetKbps);
            }
            else if (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
            {
                par.calcParam.mvcPerViewPar.maxKbps = par.calcParam.mvcPerViewPar.targetKbps;
            }
        }

        if (par.calcParam.bufferSizeInKB == 0)
        {
            if ((bRateControlLA(par.mfx.RateControlMethod) && (par.mfx.RateControlMethod != MFX_RATECONTROL_LA_HRD)) || par.mfx.RateControlMethod == MFX_RATECONTROL_VME)
            {
                par.calcParam.bufferSizeInKB = (par.mfx.FrameInfo.Width * par.mfx.FrameInfo.Height * 3 / 2 / 1000);
            }
            else
            {
                mfxU32 bufferSizeInBits = IPP_MIN(
                    GetMaxBufferSize(par),                           // limit by spec
                    par.calcParam.maxKbps * DEFAULT_CPB_IN_SECONDS); // limit by common sense

                par.calcParam.bufferSizeInKB = !IsHRDBasedBRCMethod(par.mfx.RateControlMethod)
                        ? GetUncompressedSizeInKb(par)
                        : bufferSizeInBits / 8000;
            }
        }

        if (par.calcParam.mvcPerViewPar.bufferSizeInKB == 0)
        {
            mfxU32 bufferSizeInBits = IPP_MIN(
                GetMaxPerViewBufferSize(par),                           // limit by spec
                par.calcParam.mvcPerViewPar.maxKbps * DEFAULT_CPB_IN_SECONDS); // limit by common sense

            par.calcParam.mvcPerViewPar.bufferSizeInKB = !IsHRDBasedBRCMethod(par.mfx.RateControlMethod)
                    ? GetUncompressedSizeInKb(par)
                    : bufferSizeInBits / 8000;
        }

        if (par.calcParam.initialDelayInKB == 0 && IsHRDBasedBRCMethod(par.mfx.RateControlMethod))
        {
            par.calcParam.initialDelayInKB = par.calcParam.bufferSizeInKB / 2;
            par.calcParam.mvcPerViewPar.initialDelayInKB = par.calcParam.mvcPerViewPar.bufferSizeInKB / 2;
        }
    }

    if (extOpt->MaxDecFrameBuffering == 0)
    {
        extOpt->MaxDecFrameBuffering = par.mfx.NumRefFrame;
    }

    if (extDdi->DisablePSubMBPartition == MFX_CODINGOPTION_UNKNOWN)
    {
        extDdi->DisablePSubMBPartition = MFX_CODINGOPTION_OFF;

        // check restriction A.3.3 (f)
        if (IsAvcBaseProfile(par.mfx.CodecProfile) && par.mfx.CodecLevel <= MFX_LEVEL_AVC_3)
            extDdi->DisablePSubMBPartition = MFX_CODINGOPTION_ON;
    }

    if (extDdi->DisableBSubMBPartition == MFX_CODINGOPTION_UNKNOWN)
    {
        extDdi->DisableBSubMBPartition = MFX_CODINGOPTION_OFF;

        // check restriction A.3.3 (f)
        if (IsAvcBaseProfile(par.mfx.CodecProfile) && par.mfx.CodecLevel <= MFX_LEVEL_AVC_3)
            extDdi->DisableBSubMBPartition = MFX_CODINGOPTION_ON;
    }

    if (extDdi->WeightedBiPredIdc == 0)
    {
        extDdi->WeightedBiPredIdc = (par.mfx.GopRefDist == 3 && !IsMvcProfile(par.mfx.CodecProfile) && extOpt2->BRefType == MFX_B_REF_OFF)
            ? 2  // explicit weighted biprediction (when 2 B frames in a row)
            : 0; // no weighted biprediction
    }

    if (extDdi->CabacInitIdcPlus1 == 0)
    {
        extDdi->CabacInitIdcPlus1 = GetCabacInitIdc(par.mfx.TargetUsage) + 1;
    }

    if (hwCaps.UserMaxFrameSizeSupport && extOpt2->MaxFrameSize == 0)
        extOpt2->MaxFrameSize = IPP_MIN(GetMaxFrameSize(par), GetFirstMaxFrameSize(par));

    par.ApplyDefaultsToMvcSeqDesc();

    bool svcSupportedByHw = (hwCaps.MaxNum_QualityLayer || hwCaps.MaxNum_DependencyLayer);
    for (mfxU32 i = 0; i < par.calcParam.numDependencyLayer; i++)
    {
        mfxU32 did = par.calcParam.did[i];

        if (extSvc->DependencyLayer[did].CropH == 0)
            extSvc->DependencyLayer[did].CropH = extSvc->DependencyLayer[did].Height - extSvc->DependencyLayer[did].CropY;
        if (extSvc->DependencyLayer[did].CropW == 0)
            extSvc->DependencyLayer[did].CropW = extSvc->DependencyLayer[did].Width  - extSvc->DependencyLayer[did].CropX;

        if (extSvc->DependencyLayer[did].ScanIdxPresent == 0)
            extSvc->DependencyLayer[did].ScanIdxPresent = MFX_CODINGOPTION_OFF;

        if (IsOff(extSvc->DependencyLayer[did].ScanIdxPresent))
        {
            for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++)
            {
                extSvc->DependencyLayer[did].QualityLayer[qid].ScanIdxStart = 0;
                extSvc->DependencyLayer[did].QualityLayer[qid].ScanIdxEnd   = 15;
            }
        }

        if (extSvc->DependencyLayer[did].BasemodePred == 0)
            extSvc->DependencyLayer[did].BasemodePred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
        if (extSvc->DependencyLayer[did].MotionPred == 0)
            extSvc->DependencyLayer[did].MotionPred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
        if (extSvc->DependencyLayer[did].ResidualPred == 0)
            extSvc->DependencyLayer[did].ResidualPred = svcSupportedByHw
                ? mfxU16(MFX_CODINGOPTION_ADAPTIVE)
                : mfxU16(MFX_CODINGOPTION_OFF);
    }

    if (!IsSvcProfile(par.mfx.CodecProfile))
    {
        extSvc->TemporalScale[0]                                = 1;
        extSvc->DependencyLayer[0].Active                       = 1;
        extSvc->DependencyLayer[0].Width                        = par.mfx.FrameInfo.Width;
        extSvc->DependencyLayer[0].Height                       = par.mfx.FrameInfo.Height;
        extSvc->DependencyLayer[0].CropX                        = par.mfx.FrameInfo.CropX;
        extSvc->DependencyLayer[0].CropY                        = par.mfx.FrameInfo.CropY;
        extSvc->DependencyLayer[0].CropW                        = par.mfx.FrameInfo.CropW;
        extSvc->DependencyLayer[0].CropH                        = par.mfx.FrameInfo.CropH;
        extSvc->DependencyLayer[0].GopPicSize                   = par.mfx.GopPicSize;
        extSvc->DependencyLayer[0].GopRefDist                   = par.mfx.GopRefDist;
        extSvc->DependencyLayer[0].GopOptFlag                   = par.mfx.GopOptFlag;
        extSvc->DependencyLayer[0].IdrInterval                  = par.mfx.IdrInterval;
        extSvc->DependencyLayer[0].BasemodePred                 = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].MotionPred                   = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].ResidualPred                 = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].ScanIdxPresent               = MFX_CODINGOPTION_OFF;
        extSvc->DependencyLayer[0].TemporalNum                  = 1;
        extSvc->DependencyLayer[0].TemporalId[0]                = 0;
        extSvc->DependencyLayer[0].QualityNum                   = 1;

        extRc->RateControlMethod = par.mfx.RateControlMethod;
        extRc->NumLayers         = 1;
        switch (extRc->RateControlMethod)
        {
        case MFX_RATECONTROL_CBR:
        case MFX_RATECONTROL_VBR:
        case MFX_RATECONTROL_LA:
        case MFX_RATECONTROL_VCM:
        case MFX_RATECONTROL_QVBR:
        case MFX_RATECONTROL_LA_HRD:

            extRc->Layer[0].CbrVbr.TargetKbps       = par.mfx.TargetKbps;
            extRc->Layer[0].CbrVbr.InitialDelayInKB = par.mfx.InitialDelayInKB;
            extRc->Layer[0].CbrVbr.BufferSizeInKB   = par.mfx.BufferSizeInKB;
            extRc->Layer[0].CbrVbr.MaxKbps          = par.mfx.MaxKbps;
            break;
        case MFX_RATECONTROL_CQP:
            extRc->Layer[0].Cqp.QPI = par.mfx.QPI;
            extRc->Layer[0].Cqp.QPP = par.mfx.QPP;
            extRc->Layer[0].Cqp.QPB = par.mfx.QPB;
            break;
        case MFX_RATECONTROL_AVBR:
            extRc->Layer[0].Avbr.TargetKbps  = par.mfx.TargetKbps;
            extRc->Layer[0].Avbr.Convergence = par.mfx.Convergence;
            extRc->Layer[0].Avbr.Accuracy    = par.mfx.Accuracy;
            break;
        case MFX_RATECONTROL_ICQ:
        case MFX_RATECONTROL_LA_ICQ:
            break;
        case MFX_RATECONTROL_LA_EXT:
            break;
        case MFX_RATECONTROL_VME:
            break;
        default:
            assert(0);
            break;
        }
    }

    if (extBits->SPSBuffer == 0)
    {
        mfxFrameInfo const & fi = par.mfx.FrameInfo;


        extSps->nalRefIdc                       = par.calcParam.lyncMode ? 3 : 1;
        extSps->nalUnitType                     = 7;
        extSps->profileIdc                      = mfxU8(par.mfx.CodecProfile & MASK_PROFILE_IDC);
        if (IsSvcProfile(extSps->profileIdc))
            extSps->profileIdc                  = MFX_PROFILE_AVC_HIGH;
        extSps->constraints.set0                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET0_FLAG));
        extSps->constraints.set1                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET1_FLAG));
        if (par.calcParam.numTemporalLayer > 0 && IsAvcBaseProfile(par.mfx.CodecProfile))
            extSps->constraints.set1            = 1; // Lync requires constraint base profile
        extSps->constraints.set2                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET2_FLAG));
        extSps->constraints.set3                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET3_FLAG));
        extSps->constraints.set4                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET4_FLAG));
        extSps->constraints.set5                = mfxU8(!!(par.mfx.CodecProfile & MASK_CONSTRAINT_SET5_FLAG));
        extSps->constraints.set6                = 0;
        extSps->constraints.set7                = 0;
        extSps->levelIdc                        = IsMvcProfile(par.mfx.CodecProfile) ? mfxU8(par.calcParam.mvcPerViewPar.codecLevel) : mfxU8(par.mfx.CodecLevel);
        extSps->seqParameterSetId               = mfxU8(extBits->SPSId);
        extSps->chromaFormatIdc                 = mfxU8(MFX_CHROMAFORMAT_YUV420);//in case if RGB or YUY2 passed we still need encode in 420
        extSps->separateColourPlaneFlag         = 0;
        extSps->bitDepthLumaMinus8              = 0;
        extSps->bitDepthChromaMinus8            = 0;
        extSps->qpprimeYZeroTransformBypassFlag = 0;
        extSps->seqScalingMatrixPresentFlag     = 0;
        extSps->log2MaxFrameNumMinus4           = 4;//GetDefaultLog2MaxFrameNumMinux4(par);
        extSps->picOrderCntType                 = extOpt2->SkipFrame ? 0  : GetDefaultPicOrderCount(par);
        extSps->log2MaxPicOrderCntLsbMinus4     = extOpt2->SkipFrame ? ((mfxU8)CeilLog2(2 * par.mfx.GopPicSize) + 1 - 4) : GetDefaultLog2MaxPicOrdCntMinus4(par);
        extSps->log2MaxPicOrderCntLsbMinus4     = IPP_MIN(12, extSps->log2MaxPicOrderCntLsbMinus4);
        extSps->deltaPicOrderAlwaysZeroFlag     = 1;
        extSps->maxNumRefFrames                 = mfxU8(par.mfx.NumRefFrame);
        extSps->gapsInFrameNumValueAllowedFlag  = par.calcParam.numTemporalLayer > 1
            || par.calcParam.lyncMode; // for Lync change of temporal structure shouldn't change SPS.
        extSps->frameMbsOnlyFlag                = (fi.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? 1 : 0;
        extSps->picWidthInMbsMinus1             = (fi.Width / 16) - 1;
        extSps->picHeightInMapUnitsMinus1       = (fi.Height / 16 / (2 - extSps->frameMbsOnlyFlag)) - 1;
        extSps->direct8x8InferenceFlag          = 1;

        mfxU16 cropUnitX = CROP_UNIT_X[fi.ChromaFormat];
        //Hack!!!! Fix cropping for ARGB and YUY2 formats, need to redesign in case real 444 or 422 support in AVC.
        mfxU16 croma_format = fi.ChromaFormat;
        if(croma_format == MFX_CHROMAFORMAT_YUV444 || croma_format == MFX_CHROMAFORMAT_YUV422)
            croma_format = MFX_CHROMAFORMAT_YUV420;
        mfxU16 cropUnitY = CROP_UNIT_Y[croma_format] * (2 - extSps->frameMbsOnlyFlag);

        extSps->frameCropLeftOffset   = (fi.CropX / cropUnitX);
        extSps->frameCropRightOffset  = (fi.Width - fi.CropW - fi.CropX) / cropUnitX;
        extSps->frameCropTopOffset    = (fi.CropY / cropUnitY);
        extSps->frameCropBottomOffset = (fi.Height - fi.CropH - fi.CropY) / cropUnitY;
        extSps->frameCroppingFlag     =
            extSps->frameCropLeftOffset || extSps->frameCropRightOffset ||
            extSps->frameCropTopOffset  || extSps->frameCropBottomOffset;

        extSps->vuiParametersPresentFlag = !IsOn(extOpt2->DisableVUI);

        if (extSps->vuiParametersPresentFlag)
        {
            AspectRatioConverter arConv(fi.AspectRatioW, fi.AspectRatioH);
            extSps->vui.flags.aspectRatioInfoPresent = (fi.AspectRatioH && fi.AspectRatioW) && !IsOff(extOpt3->AspectRatioInfoPresent);
            extSps->vui.aspectRatioIdc               = arConv.GetSarIdc();
            extSps->vui.sarWidth                     = arConv.GetSarWidth();
            extSps->vui.sarHeight                    = arConv.GetSarHeight();

            extSps->vui.flags.overscanInfoPresent = !IsOff(extOpt3->OverscanInfoPresent);
            extSps->vui.flags.overscanAppropriate = 0;

            extSps->vui.videoFormat                    = mfxU8(extVsi->VideoFormat);
            extSps->vui.flags.videoFullRange           = extVsi->VideoFullRange;
            extSps->vui.flags.colourDescriptionPresent = extVsi->ColourDescriptionPresent;
            extSps->vui.colourPrimaries                = mfxU8(extVsi->ColourPrimaries);
            extSps->vui.transferCharacteristics        = mfxU8(extVsi->TransferCharacteristics);
            extSps->vui.matrixCoefficients             = mfxU8(extVsi->MatrixCoefficients);
            extSps->vui.flags.videoSignalTypePresent   =
                extSps->vui.videoFormat                    != 5 ||
                extSps->vui.flags.videoFullRange           != 0 ||
                extSps->vui.flags.colourDescriptionPresent != 0;

            extSps->vui.flags.chromaLocInfoPresent     = !!extCli->ChromaLocInfoPresentFlag;
            extSps->vui.chromaSampleLocTypeTopField    = mfxU8(extCli->ChromaSampleLocTypeTopField);
            extSps->vui.chromaSampleLocTypeBottomField = mfxU8(extCli->ChromaSampleLocTypeBottomField);

            extSps->vui.flags.timingInfoPresent = !IsOff(extOpt3->TimingInfoPresent);
            extSps->vui.numUnitsInTick          = fi.FrameRateExtD;
            extSps->vui.timeScale               = fi.FrameRateExtN * 2;
            extSps->vui.flags.fixedFrameRate    = !IsOff(extOpt2->FixedFrameRate);


            extSps->vui.flags.nalHrdParametersPresent = IsOn(extOpt->VuiNalHrdParameters);
            extSps->vui.flags.vclHrdParametersPresent = IsOn(extOpt->VuiVclHrdParameters);

            extSps->vui.nalHrdParameters.cpbCntMinus1 = 0;
            extSps->vui.nalHrdParameters.bitRateScale = 0;
            extSps->vui.nalHrdParameters.cpbSizeScale = 2;

            if (par.calcParam.cqpHrdMode)
            {
                extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.decorativeHrdParam.maxKbps) - 1;
                extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.decorativeHrdParam.bufferSizeInKB, 2) - 1;
                extSps->vui.nalHrdParameters.cbrFlag[0] = par.calcParam.cqpHrdMode == 1 ? 1 : 0;
            }
            else
            {
                //FIXME: extSps isn't syncronized with m_video after next assignment for ViewOutput mode. Need to Init ExtSps HRD for MVC keeping them syncronized
                if (IsMvcProfile(par.mfx.CodecProfile))
                {
                    extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.mvcPerViewPar.maxKbps) - 1;
                    extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.mvcPerViewPar.bufferSizeInKB, 2) - 1;
                }
                else
                {
                    extSps->vui.nalHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.maxKbps) - 1;
                    extSps->vui.nalHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.bufferSizeInKB, 2) - 1;
                }
            extSps->vui.nalHrdParameters.cbrFlag[0]                         = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR);
            }
            extSps->vui.nalHrdParameters.initialCpbRemovalDelayLengthMinus1 = 23;
            extSps->vui.nalHrdParameters.cpbRemovalDelayLengthMinus1        = 23;
            extSps->vui.nalHrdParameters.dpbOutputDelayLengthMinus1         = 23;
            extSps->vui.nalHrdParameters.timeOffsetLength                   = 24;

            extSps->vui.vclHrdParameters.cpbCntMinus1                       = 0;
            extSps->vui.vclHrdParameters.bitRateScale                       = 0;
            extSps->vui.vclHrdParameters.cpbSizeScale                       = 2;
            if (IsMvcProfile(par.mfx.CodecProfile))
            {
                extSps->vui.vclHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.mvcPerViewPar.maxKbps) - 1;
                extSps->vui.vclHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.mvcPerViewPar.bufferSizeInKB, 2) - 1;
            }
            else
            {
                extSps->vui.vclHrdParameters.bitRateValueMinus1[0] = GetMaxBitrateValue(par.calcParam.maxKbps) - 1;
                extSps->vui.vclHrdParameters.cpbSizeValueMinus1[0] = GetCpbSizeValue(par.calcParam.bufferSizeInKB, 2) - 1;
            }
            extSps->vui.vclHrdParameters.cbrFlag[0]                         = (par.mfx.RateControlMethod == MFX_RATECONTROL_CBR);
            extSps->vui.vclHrdParameters.initialCpbRemovalDelayLengthMinus1 = 23;
            extSps->vui.vclHrdParameters.cpbRemovalDelayLengthMinus1        = 23;
            extSps->vui.vclHrdParameters.dpbOutputDelayLengthMinus1         = 23;
            extSps->vui.vclHrdParameters.timeOffsetLength                   = 24;

            extSps->vui.flags.lowDelayHrd                                   = IsOn(extOpt3->LowDelayHrd);;
            extSps->vui.flags.picStructPresent                              = IsOn(extOpt->PicTimingSEI);

            extSps->vui.flags.bitstreamRestriction           = !IsOff(extOpt3->BitstreamRestriction);
            extSps->vui.flags.motionVectorsOverPicBoundaries = 1;
            extSps->vui.maxBytesPerPicDenom                  = 2;
            extSps->vui.maxBitsPerMbDenom                    = 1;
            extSps->vui.log2MaxMvLengthHorizontal            = mfxU8(CeilLog2(4 * MAX_H_MV - 1));
            extSps->vui.log2MaxMvLengthVertical              = mfxU8(CeilLog2(4 * GetMaxVmv(par.mfx.CodecLevel) - 1));
            extSps->vui.numReorderFrames                     = GetNumReorderFrames(par);
            extSps->vui.maxDecFrameBuffering                 = mfxU8(extOpt->MaxDecFrameBuffering);

            extSps->vuiParametersPresentFlag =
                extSps->vui.flags.aspectRatioInfoPresent  ||
                extSps->vui.flags.overscanInfoPresent     ||
                extSps->vui.flags.videoSignalTypePresent  ||
                extSps->vui.flags.chromaLocInfoPresent    ||
                extSps->vui.flags.timingInfoPresent       ||
                extSps->vui.flags.nalHrdParametersPresent ||
                extSps->vui.flags.vclHrdParametersPresent ||
                extSps->vui.flags.picStructPresent        ||
                extSps->vui.flags.bitstreamRestriction;
        }
    }

    if (extBits->PPSBufSize == 0)
    {
        extPps->nalRefIdc                             = par.calcParam.lyncMode ? 3 : 1;
        extPps->picParameterSetId                     = mfxU8(extBits->PPSId);
        extPps->seqParameterSetId                     = mfxU8(extBits->SPSId);
        extPps->entropyCodingModeFlag                 = IsOff(extOpt->CAVLC);
        extPps->bottomFieldPicOrderInframePresentFlag = 0;
        extPps->numSliceGroupsMinus1                  = 0;
        extPps->sliceGroupMapType                     = 0;
        extPps->numRefIdxL0DefaultActiveMinus1        = 0;
        extPps->numRefIdxL1DefaultActiveMinus1        = 0;
        extPps->weightedPredFlag                      = mfxU8(extOpt3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT);
        extPps->weightedBipredIdc                     = extOpt3->WeightedBiPred ? mfxU8(extOpt3->WeightedBiPred - 1) : mfxU8(extDdi->WeightedBiPredIdc);
        extPps->picInitQpMinus26                      = 0;
        extPps->picInitQsMinus26                      = 0;
        extPps->chromaQpIndexOffset                   = 0;
        extPps->deblockingFilterControlPresentFlag    = 1;
        extPps->constrainedIntraPredFlag              = 0;
        extPps->redundantPicCntPresentFlag            = 0;
        extPps->moreRbspData                          =
            !IsAvcBaseProfile(par.mfx.CodecProfile) &&
            par.mfx.CodecProfile != MFX_PROFILE_AVC_MAIN;
        extPps->transform8x8ModeFlag                  = extOpt->IntraPredBlockSize > MFX_BLOCKSIZE_MIN_16X16;
        extPps->picScalingMatrixPresentFlag           = 0;
        extPps->secondChromaQpIndexOffset             = 0;
    }

    par.SyncCalculableToVideoParam();
}

mfxStatus MfxHwH264Encode::CheckPayloads(
    const mfxPayload* const* payload,
    mfxU16 numPayload)
{
    const mfxU8 SUPPORTED_SEI[] = {
        0, // 00 buffering_period
        0, // 01 pic_timing
        1, // 02 pan_scan_rect
        1, // 03 filler_payload
        1, // 04 user_data_registered_itu_t_t35
        1, // 05 user_data_unregistered
        1, // 06 recovery_point
        0, // 07 dec_ref_pic_marking_repetition
        0, // 08 spare_pic
        1, // 09 scene_info
        0, // 10 sub_seq_info
        0, // 11 sub_seq_layer_characteristics
        0, // 12 sub_seq_characteristics
        1, // 13 full_frame_freeze
        1, // 14 full_frame_freeze_release
        1, // 15 full_frame_snapshot
        1, // 16 progressive_refinement_segment_start
        1, // 17 progressive_refinement_segment_end
        0, // 18 motion_constrained_slice_group_set
        1, // 19 film_grain_characteristics
        1, // 20 deblocking_filter_display_preference
        1, // 21 stereo_video_info
        0, // 22 post_filter_hint
        0, // 23 tone_mapping_info
        0, // 24 scalability_info
        0, // 25 sub_pic_scalable_layer
        0, // 26 non_required_layer_rep
        0, // 27 priority_layer_info
        0, // 28 layers_not_present
        0, // 29 layer_dependency_change
        0, // 30 scalable_nesting
        0, // 31 base_layer_temporal_hrd
        0, // 32 quality_layer_integrity_check
        0, // 33 redundant_pic_property
        0, // 34 tl0_dep_rep_index
        0, // 35 tl_switching_point
        0, // 36 parallel_decoding_info
        0, // 37 mvc_scalable_nesting
        0, // 38 view_scalability_info
        0, // 39 multiview_scene_info
        0, // 40 multiview_acquisition_info
        0, // 41 non_required_view_component
        0, // 42 view_dependency_change
        0, // 43 operation_points_not_present
        0, // 44 base_view_temporal_hrd
        1, // 45 frame_packing_arrangement
    };

    for (mfxU16 i = 0; i < numPayload; ++i)
    {
        // Particular payload can absent
        // Check only present payloads
        if (payload[i] && payload[i]->NumBit > 0)
        {
            MFX_CHECK_NULL_PTR1(payload[i]->Data);

            // Check buffer size
            MFX_CHECK(
                payload[i]->NumBit <= 8u * payload[i]->BufSize,
                MFX_ERR_UNDEFINED_BEHAVIOR);

            // Sei messages type constraints
            MFX_CHECK(
                payload[i]->Type < (sizeof SUPPORTED_SEI / sizeof SUPPORTED_SEI[0]) &&
                SUPPORTED_SEI[payload[i]->Type] == 1,
                MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxHwH264Encode::CheckRunTimeExtBuffers(
    MfxVideoParam const & video,
    mfxEncodeCtrl *       ctrl)
{
    mfxStatus checkSts = MFX_ERR_NONE;

    for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
        MFX_CHECK_NULL_PTR1(ctrl->ExtParam[i]);


    for (mfxU32 i = 0; i < ctrl->NumExtParam; i++)
    {
        if (ctrl->ExtParam[i])
        {
            if (false == IsRunTimeExtBufferIdSupported(ctrl->ExtParam[i]->BufferId))
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // don't return error in runtime, just ignore unsupported ext buffer and return warning

            if (MfxHwH264Encode::GetExtBuffer(
                ctrl->ExtParam + i + 1,
                ctrl->NumExtParam - i - 1,
                ctrl->ExtParam[i]->BufferId) != 0)
            {
                if (!(ctrl->ExtParam[i]->BufferId == MFX_EXTBUFF_AVC_REFLISTS && video.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE))
                {
                    // if buffer is attached twice, ignore second one and return warning
                    // the only exception is MFX_EXTBUFF_AVC_REFLISTS (it can be attached twice for interlace case)
                    checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                }
            }
        }
    }

    mfxExtAVCRefListCtrl const * extRefListCtrl = GetExtBuffer(*ctrl);
    if (extRefListCtrl && video.calcParam.numTemporalLayer > 0 && video.calcParam.lyncMode == 0)
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

    mfxExtSpecialEncodingModes const * extSpecModes = GetExtBuffer(video);
    if (extRefListCtrl && extSpecModes->refDummyFramesForWiDi)
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // no DPB and ref list manipulations allowed for WA WiDi mode

#if defined (ADVANCED_REF)
    mfxExtAVCRefLists const * extRefLists = GetExtBuffer(*ctrl);
    if (extRefLists && video.calcParam.numTemporalLayer > 0 && video.calcParam.lyncMode == 0)
        checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
#endif

    // check timestamp from mfxExtPictureTimingSEI
    mfxExtPictureTimingSEI const * extPt   = GetExtBuffer(*ctrl);

    if (extPt)
    {
        for (mfxU32 i = 0; i < 3; i++)
        {
            // return warning if there is any 0xffff template except for CtType
            if (extPt->TimeStamp[i].CountingType       == 0xffff ||
                extPt->TimeStamp[i].NFrames            == 0xffff ||
                extPt->TimeStamp[i].SecondsValue       == 0xffff ||
                extPt->TimeStamp[i].MinutesValue       == 0xffff ||
                extPt->TimeStamp[i].HoursValue         == 0xffff ||
                extPt->TimeStamp[i].TimeOffset         == 0xffff ||
                extPt->TimeStamp[i].ClockTimestampFlag == 0xffff ||
                extPt->TimeStamp[i].NuitFieldBasedFlag == 0xffff ||
                extPt->TimeStamp[i].FullTimestampFlag  == 0xffff ||
                extPt->TimeStamp[i].DiscontinuityFlag  == 0xffff ||
                extPt->TimeStamp[i].CntDroppedFlag     == 0xffff ||
                extPt->TimeStamp[i].SecondsFlag        == 0xffff ||
                extPt->TimeStamp[i].MinutesFlag        == 0xffff ||
                extPt->TimeStamp[i].HoursFlag          == 0xffff)
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    // check ROI
    mfxExtEncoderROI const * extRoi = GetExtBuffer(*ctrl);

    if (extRoi) {
        mfxU16 const MaxNumOfROI = 0; // TODO: change to caps->MaxNumOfROI after it will be added to DDI
        mfxU16 actualNumRoi = extRoi->NumROI;
        if (extRoi->NumROI)
        {
            if (extRoi->NumROI > MaxNumOfROI)
            {
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
                actualNumRoi = MaxNumOfROI;
            }
        }

        for (mfxU16 i = 0; i < actualNumRoi; i++)
        {
            // check that ROI is aligned to MB
            if ((extRoi->ROI[i].Left & 0x0f) || (extRoi->ROI[i].Right & 0x0f) ||
                (extRoi->ROI[i].Top & 0x0f) || (extRoi->ROI[i].Bottom & 0x0f))
                checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;

            // check that ROI dimensions don't conflict with each other and don't exceed frame size
            if(extRoi->ROI[i].Left > mfxU32(video.mfx.FrameInfo.Width -16) ||
               extRoi->ROI[i].Right < mfxU32(extRoi->ROI[i].Left + 16) ||
               extRoi->ROI[i].Right >  video.mfx.FrameInfo.Width)
               checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            if(extRoi->ROI[i].Top > mfxU32(video.mfx.FrameInfo.Height -16) ||
               extRoi->ROI[i].Bottom < mfxU32(extRoi->ROI[i].Top + 16) ||
               extRoi->ROI[i].Bottom >  video.mfx.FrameInfo.Height)
               checkSts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
        }
    }

    return checkSts;
}

mfxStatus MfxHwH264Encode::CheckRunTimePicStruct(
    mfxU16 runtPs,
    mfxU16 initPs)
{
    static mfxU16 const PRG  = MFX_PICSTRUCT_PROGRESSIVE;
    static mfxU16 const TFF  = MFX_PICSTRUCT_FIELD_TFF;
    static mfxU16 const BFF  = MFX_PICSTRUCT_FIELD_BFF;
    static mfxU16 const UNK  = MFX_PICSTRUCT_UNKNOWN;
    static mfxU16 const DBL  = MFX_PICSTRUCT_FRAME_DOUBLING;
    static mfxU16 const TRPL = MFX_PICSTRUCT_FRAME_TRIPLING;
    static mfxU16 const REP  = MFX_PICSTRUCT_FIELD_REPEATED;

    if (initPs == PRG && runtPs == UNK         ||
        initPs == PRG && runtPs == PRG         ||
        initPs == PRG && runtPs == (PRG | DBL) ||
        initPs == PRG && runtPs == (PRG | TRPL))
        return MFX_ERR_NONE;

    if (initPs == BFF && runtPs == UNK ||
        initPs == BFF && runtPs == BFF ||
        initPs == UNK && runtPs == BFF ||
        initPs == TFF && runtPs == UNK ||
        initPs == TFF && runtPs == TFF ||
        initPs == UNK && runtPs == TFF)
        return MFX_ERR_NONE;

    if (initPs == UNK && runtPs == (PRG | BFF)       ||
        initPs == UNK && runtPs == (PRG | TFF)       ||
        initPs == UNK && runtPs == (PRG | BFF | REP) ||
        initPs == UNK && runtPs == (PRG | TFF | REP) ||
        initPs == UNK && runtPs == PRG               ||
        initPs == BFF && runtPs == PRG               ||
        initPs == TFF && runtPs == PRG)
        return MFX_ERR_NONE;

    if (initPs == UNK && runtPs == UNK)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    return MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
}

bool MfxHwH264Encode::IsRecoveryPointSeiMessagePresent(
    const mfxPayload* const* payload,
    mfxU16 numPayload,
    mfxU32 payloadLayout)
{
    mfxU32 step = payloadLayout == 0 /*MFX_PAYLOAD_LAYOUT_ALL*/ ? 1 : 2;
    mfxU32 i    = payloadLayout == 2 /*MFX_PAYLOAD_LAYOUT_ODD*/ ? 1 : 0;

    for (; i < numPayload; i += step)
        if (payload[i] && payload[i]->NumBit > 0 && payload[i]->Type == 6)
            break;

    return i < numPayload;
}

mfxStatus MfxHwH264Encode::CopyFrameDataBothFields(
    VideoCORE *          core,
    mfxFrameData const & dst,
    mfxFrameData const & src,
    mfxFrameInfo const & info)
{
    mfxFrameSurface1 surfSrc = { {0,}, info, src };
    mfxFrameSurface1 surfDst = { {0,}, info, dst };
    return core->DoFastCopyWrapper(&surfDst,MFX_MEMTYPE_INTERNAL_FRAME|MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_FROM_ENCODE, &surfSrc, MFX_MEMTYPE_EXTERNAL_FRAME|MFX_MEMTYPE_SYSTEM_MEMORY);
}

void MfxHwH264Encode::CopyFrameData(
    const mfxFrameData& dst,
    const mfxFrameData& src,
    const mfxFrameInfo& info,
    mfxU32 fieldId)
{
    mfxU32 height = info.Height;
    mfxU32 srcOffset = 0;
    mfxU32 dstOffset = 0;
    mfxU32 srcPitch = src.Pitch;
    mfxU32 dstPitch = dst.Pitch;

    if ((info.PicStruct & MFX_PICSTRUCT_PROGRESSIVE) == 0)
    {
        height /= 2;
        srcOffset += fieldId * srcPitch;
        dstOffset += fieldId * dstPitch;
        srcPitch *= 2;
        dstPitch *= 2;
    }

    IppiSize roiLuma = { info.Width, height };
    ippiCopy_8u_C1R(src.Y + srcOffset, srcPitch, dst.Y + dstOffset, dstPitch, roiLuma);

    IppiSize roiChroma = { info.Width, height / 2 };
    ippiCopy_8u_C1R(src.UV + srcOffset, srcPitch, dst.UV + dstOffset, dstPitch, roiChroma);
}

#if 0 // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing
void MfxHwH264Encode::WriteFrameData(
    vm_file *            file,
    VideoCORE *          core,
    mfxFrameData const & fdata,
    mfxFrameInfo const & info)
{
    mfxFrameData data = fdata;
    FrameLocker lock(core, data, false);

    if (file != 0 && data.Y != 0 && data.UV != 0)
    {
        for (mfxU32 i = 0; i < info.Height; i++)
            vm_file_fwrite(data.Y + i * data.Pitch, 1, info.Width, file);

        for (mfxI32 y = 0; y < info.Height / 2; y++)
            for (mfxI32 x = 0; x < info.Width; x += 2)
                vm_file_fwrite(data.UV + y * data.Pitch + x, 1, 1, file);

        for (mfxI32 y = 0; y < info.Height / 2; y++)
            for (mfxI32 x = 1; x < info.Width; x += 2)
                vm_file_fwrite(data.UV + y * data.Pitch + x, 1, 1, file);

        vm_file_fflush(file);
    }
}
#endif // removed dependency from fwrite(). Custom writing to file shouldn't be present in MSDK releases w/o documentation and testing

mfxStatus MfxHwH264Encode::ReadFrameData(
    vm_file *            file,
    mfxU32               frameNum,
    VideoCORE *          core,
    mfxFrameData const & fdata,
    mfxFrameInfo const & info)
{
    mfxFrameData data = fdata;
    FrameLocker lock(core, data, false);

    if (file != 0 && data.Y != 0 && data.UV != 0)
    {
        mfxU32 frameSize = (info.Height * info.Width * 5) / 4;
        if (vm_file_fseek(file, frameSize * frameNum, VM_FILE_SEEK_SET) != 0)
        {
            MFX_RETURN(MFX_ERR_NOT_FOUND);
        }

        for (mfxU32 i = 0; i < info.Height; i++)
            (void)vm_file_fread(data.Y + i * data.Pitch, 1, info.Width, file);

        for (mfxI32 y = 0; y < info.Height / 2; y++)
            for (mfxI32 x = 0; x < info.Width; x += 2)
                (void)vm_file_fread(data.UV + y * data.Pitch + x, 1, 1, file);

        for (mfxI32 y = 0; y < info.Height / 2; y++)
            for (mfxI32 x = 1; x < info.Width; x += 2)
                (void)vm_file_fread(data.UV + y * data.Pitch + x, 1, 1, file);
    }

    return MFX_ERR_NONE;
}

mfxExtBuffer* MfxHwH264Encode::GetExtBuffer(mfxExtBuffer** extBuf, mfxU32 numExtBuf, mfxU32 id)
{
    if (extBuf != 0)
    {
        for (mfxU16 i = 0; i < numExtBuf; i++)
            if (extBuf[i] != 0 && extBuf[i]->BufferId == id) // assuming aligned buffers
                return (extBuf[i]);
    }

    return 0;
}

mfxEncryptedData* MfxHwH264Encode::GetEncryptedData(mfxBitstream& bs, mfxU32 fieldId)
{
    return (fieldId == 0)
        ? bs.EncryptedData
        : (bs.EncryptedData)
            ? bs.EncryptedData->Next
            : 0;
}

mfxU8 MfxHwH264Encode::ConvertFrameTypeMfx2Ddi(mfxU32 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I: return CODING_TYPE_I;
    case MFX_FRAMETYPE_P: return CODING_TYPE_P;
    case MFX_FRAMETYPE_B: return CODING_TYPE_B;
    default: assert(!"Unsupported frame type"); return 0;
    }
}


mfxU8 MfxHwH264Encode::ConvertMfxFrameType2SliceType(mfxU8 type)
{
    switch (type & MFX_FRAMETYPE_IPB)
    {
    case MFX_FRAMETYPE_I: return SLICE_TYPE_I + 5;
    case MFX_FRAMETYPE_P: return SLICE_TYPE_P + 5;
    case MFX_FRAMETYPE_B: return SLICE_TYPE_B + 5;
    default: assert("bad codingType"); return 0xff;
    }
}


mfxU32 SliceDivider::GetFirstMbInSlice() const
{
    assert(m_currSliceFirstMbRow * m_numMbInRow < 0x100000000);
    return m_currSliceFirstMbRow * m_numMbInRow;
}

mfxU32 SliceDivider::GetNumMbInSlice() const
{
    assert(m_currSliceNumMbRow * m_numMbInRow < 0x100000000);
    return m_currSliceNumMbRow * m_numMbInRow;
}

mfxU32 SliceDivider::GetNumSlice() const
{
    assert(0 < m_numSlice && m_numSlice < 0x100000000);
    return m_numSlice;
}


SliceDividerBluRay::SliceDividerBluRay(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerBluRay::Next;
    m_numSlice            = IPP_MAX(numSlice, 1);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = m_numSlice;
    m_leftMbRow           = m_numMbRow;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = m_leftMbRow / m_leftSlice;
}

bool SliceDividerBluRay::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        state.m_currSliceNumMbRow = state.m_leftMbRow / state.m_leftSlice;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}


SliceDividerOneSlice::SliceDividerOneSlice(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    numSlice;
    m_pfNext              = &SliceDividerOneSlice::Next;
    m_numSlice            = 1;
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = 1;
    m_leftMbRow           = heightInMbs;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = heightInMbs;
}

bool SliceDividerOneSlice::Next(SliceDividerState & state)
{
    state.m_leftMbRow = 0;
    state.m_leftSlice = 0;
    return false;
}


namespace
{
    // nearest but less than n
    mfxU32 GetNearestPowerOf2(mfxU32 n)
    {
        mfxU32 mask = 0x80000000;
        for (; mask > 0; mask >>= 1)
            if (n & mask)
                break;
        return mask;
    }
};

SliceDividerSnb::SliceDividerSnb(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerSnb::Next;
    m_numSlice            = IPP_MAX(numSlice, 1);
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_leftSlice           = m_numSlice;
    m_leftMbRow           = m_numMbRow;
    m_currSliceFirstMbRow = 0;
    m_currSliceNumMbRow   = m_leftMbRow / m_leftSlice;

    // check if frame is divisible to requested number of slices
    mfxU32 numMbRowInSlice = IPP_MAX(1, m_numMbRow / m_numSlice);
    numMbRowInSlice = GetNearestPowerOf2(numMbRowInSlice) << 1;
    if ((m_numMbRow + numMbRowInSlice - 1) / numMbRowInSlice < m_numSlice)
        numMbRowInSlice >>= 1; // As number of slices can only be increased, try smaller slices

    m_numSlice = (m_numMbRow + numMbRowInSlice - 1) / numMbRowInSlice;
    m_leftSlice = m_numSlice;
    m_currSliceNumMbRow = IPP_MIN(numMbRowInSlice, m_leftMbRow);
}

bool SliceDividerSnb::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}


SliceDividerHsw::SliceDividerHsw(
    mfxU32 numSlice,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerHsw::Next;
    m_numSlice            = IPP_MAX(1, IPP_MIN(heightInMbs, numSlice));
    m_numMbInRow          = widthInMbs;
    m_numMbRow            = heightInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = heightInMbs;

    mfxU32 H  = heightInMbs;      // frame height
    mfxU32 n  = m_numSlice;       // num slices
    mfxU32 sh = (H + n - 1) / n;  // slice height

    // check if frame is divisible to requested number of slices
    // so that size of last slice is no bigger than other slices
    while (sh * (n - 1) >= H)
    {
        n++;
        sh = (H + n - 1) / n;
    }

    m_currSliceNumMbRow   = sh;
    m_numSlice            = n;
    m_leftSlice           = n;
}

bool SliceDividerHsw::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDividerLync::SliceDividerLync(
    mfxU32 sliceSizeInMbs,
    mfxU32 widthInMbs,
    mfxU32 heightInMbs)
{
    m_pfNext              = &SliceDividerLync::Next;
    m_numMbInRow          = 1;
    m_numMbRow            = heightInMbs*widthInMbs;
    m_currSliceFirstMbRow = 0;
    m_leftMbRow           = m_numMbRow;
    m_numSlice            = (m_numMbRow + sliceSizeInMbs - 1) / sliceSizeInMbs;
    m_currSliceNumMbRow   = sliceSizeInMbs;
    m_leftSlice           = m_numSlice;
}

bool SliceDividerLync::Next(SliceDividerState & state)
{
    state.m_leftMbRow -= state.m_currSliceNumMbRow;
    state.m_leftSlice -= 1;

    if (state.m_leftSlice == 0)
    {
        assert(state.m_leftMbRow == 0);
        return false;
    }
    else
    {
        state.m_currSliceFirstMbRow += state.m_currSliceNumMbRow;
        if (state.m_currSliceNumMbRow > state.m_leftMbRow)
            state.m_currSliceNumMbRow = state.m_leftMbRow;
        assert(state.m_currSliceNumMbRow != 0);
        return true;
    }
}

SliceDivider MfxHwH264Encode::MakeSliceDivider(
    mfxU32  sliceHwCaps,
    mfxU32  sliceSizeInMbs,
    mfxU32  numSlice,
    mfxU32  widthInMbs,
    mfxU32  heightInMbs)
{
    if(sliceHwCaps > 0 && sliceSizeInMbs > 0)
        return SliceDividerLync(sliceSizeInMbs, widthInMbs, heightInMbs);

    switch (sliceHwCaps)
    {
    case 1:  return SliceDividerSnb(numSlice, widthInMbs, heightInMbs);
    case 2:  return SliceDividerHsw(numSlice, widthInMbs, heightInMbs);
    case 3:  return SliceDividerBluRay(numSlice, widthInMbs, heightInMbs);
    case 4:  return SliceDividerHsw(numSlice, widthInMbs, heightInMbs);//If arbitrary slice size supported by HW make legasy HSW division, need to implement arbitrary case
    default: return SliceDividerOneSlice(numSlice, widthInMbs, heightInMbs);
    }
}


namespace
{
    bool TestSliceDivider(
        mfxU32 requestedNumSlice,
        mfxU32 width,
        mfxU32 height,
        mfxU32 refNumSlice,
        mfxU32 refNumMbRowsInSlice,
        mfxU32 refNumMbRowsInLastSlice)
    {
        mfxU32 numMbsInRow = (width + 15) / 16;
        SliceDivider sd = SliceDividerSnb(requestedNumSlice, numMbsInRow, (height + 15) / 16);

        mfxU32 numSlice = sd.GetNumSlice();
        if (numSlice != refNumSlice)
            return false;

        mfxU32 predictedPirstMbInSlice = 0;
        for (mfxU32 i = 0; i < numSlice; i++)
        {
            mfxU32 firstMbInSlice = sd.GetFirstMbInSlice();
            mfxU32 numMbInSlice = sd.GetNumMbInSlice();

            if (firstMbInSlice != predictedPirstMbInSlice)
                return false;

            bool hasNext = sd.Next();
            if (hasNext != (i + 1 < numSlice))
                return false;

            if (hasNext)
            {
                if (numMbInSlice != refNumMbRowsInSlice * numMbsInRow)
                    return false;
            }
            else
            {
                if (numMbInSlice != refNumMbRowsInLastSlice * numMbsInRow)
                    return false;
            }

            predictedPirstMbInSlice += numMbInSlice;
        }

        return true;
    }

    bool TestSliceDividerWithReport(
        mfxU32 requestedNumSlice,
        mfxU32 width,
        mfxU32 height,
        mfxU32 refNumSlice,
        mfxU32 refNumMbRowsInSlice,
        mfxU32 refNumMbRowsInLastSlice)
    {
        if (!TestSliceDivider(requestedNumSlice, width, height, refNumSlice, refNumMbRowsInSlice, refNumMbRowsInLastSlice))
        {
            //std::cout << "Failed with requestedNumSlice=" << requestedNumSlice << " width=" << width << " height=" << height << std::endl;
            return false;
        }

        //std::cout << "Passed with requestedNumSlice=" << requestedNumSlice << " width=" << width << " height=" << height << std::endl;
        return true;
    }
}

void SliceDividerSnb::Test()
{
    TestSliceDividerWithReport( 2,  720,  480,  2, 16, 14);
    TestSliceDividerWithReport( 4,  720,  480,  4,  8,  6);
    TestSliceDividerWithReport( 8,  720,  480,  8,  4,  2);
    TestSliceDividerWithReport(16,  720,  480, 30,  1,  1);
    TestSliceDividerWithReport( 2, 1920, 1080,  2, 64,  4);
    TestSliceDividerWithReport( 4, 1920, 1080,  5, 16,  4);
    TestSliceDividerWithReport( 8, 1920, 1080,  9,  8,  4);
    TestSliceDividerWithReport(16, 1920, 1080, 17,  4,  4);
    TestSliceDividerWithReport( 8, 1280,  720, 12,  4,  1);
    TestSliceDividerWithReport( 8,  480,  320, 10,  2,  2);

    TestSliceDividerWithReport( 2,  720,  480 / 2,  2,  8,  7);
    TestSliceDividerWithReport( 4,  720,  480 / 2,  4,  4,  3);
    TestSliceDividerWithReport( 8,  720,  480 / 2,  8,  2,  1);
    TestSliceDividerWithReport(16,  720,  480 / 2, 15,  1,  1);
    TestSliceDividerWithReport( 2, 1920, 1080 / 2,  2, 32,  2);
    TestSliceDividerWithReport( 4, 1920, 1080 / 2,  5,  8,  2);
    TestSliceDividerWithReport( 8, 1920, 1080 / 2,  9,  4,  2);
    TestSliceDividerWithReport(16, 1920, 1080 / 2, 17,  2,  2);
    TestSliceDividerWithReport( 8, 1280,  720 / 2, 12,  2,  1);
    TestSliceDividerWithReport( 8,  480,  320 / 2, 10,  1,  1);
}

// D3D9Encoder

MfxVideoParam::MfxVideoParam()
{
    // mfxVideoParam
    AllocId = 0;
    AsyncDepth = 0;
    memset(&mfx, 0, sizeof(mfxInfoMFX));
    memset(&vpp, 0, sizeof(mfxInfoVPP));
    Protected = 0;
    IOPattern = 0;
    NumExtParam = 0;

    memset(m_extParam, 0, sizeof(m_extParam));
    // external, documented
    memset(&m_extOpt, 0, sizeof(m_extOpt));
    memset(&m_extOpt2, 0, sizeof(m_extOpt2));
    memset(&m_extOptSpsPps, 0, sizeof(m_extOptSpsPps));
    memset(&m_extOptPavp, 0, sizeof(m_extOptPavp));
    memset(&m_extVideoSignal, 0, sizeof(m_extVideoSignal));
    memset(&m_extOpaque, 0, sizeof(m_extOpaque));
    memset(&m_extMvcSeqDescr, 0, sizeof(m_extMvcSeqDescr));
    memset(&m_extPicTiming, 0, sizeof(m_extPicTiming));
    memset(&m_extTempLayers, 0, sizeof(m_extTempLayers));
    memset(&m_extSvcSeqDescr, 0, sizeof(m_extSvcSeqDescr));
    memset(&m_extSvcRateCtrl, 0, sizeof(m_extSvcRateCtrl));
    memset(&m_extEncResetOpt, 0, sizeof(m_extEncResetOpt));

    // internal, not documented
    memset(&m_extOptDdi, 0, sizeof(m_extOptDdi));
    memset(&m_extDumpFiles, 0, sizeof(m_extDumpFiles));
    memset(&m_extSps, 0, sizeof(m_extSps));
    memset(&m_extPps, 0, sizeof(m_extPps));

    memset(&calcParam, 0, sizeof(calcParam));
}

MfxVideoParam::MfxVideoParam(MfxVideoParam const & par)
{
    Construct(par);
    calcParam = par.calcParam;
}

MfxVideoParam::MfxVideoParam(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
}

MfxVideoParam& MfxVideoParam::operator=(MfxVideoParam const & par)
{
    Construct(par);
    calcParam = par.calcParam;
    return *this;
}

MfxVideoParam& MfxVideoParam::operator=(mfxVideoParam const & par)
{
    Construct(par);
    SyncVideoToCalculableParam();
    return *this;
}

void MfxVideoParam::SyncVideoToCalculableParam()
{
    mfxU32 multiplier = IPP_MAX(mfx.BRCParamMultiplier, 1);

    calcParam.bufferSizeInKB   = mfx.BufferSizeInKB   * multiplier;
    if (IsOn(m_extOpt.VuiNalHrdParameters)
        && !IsOn(m_extOpt.VuiVclHrdParameters)
        && IsOff(m_extOpt.NalHrdConformance)
        && mfx.RateControlMethod == MFX_RATECONTROL_CQP
        && mfx.FrameInfo.FrameRateExtN != 0
        && mfx.FrameInfo.FrameRateExtD != 0
        && mfx.BufferSizeInKB != 0
        && mfx.InitialDelayInKB != 0
        && mfx.TargetKbps != 0)
    {
        calcParam.cqpHrdMode = mfx.MaxKbps > 0 ? 2 : 1;
    }

    if (calcParam.cqpHrdMode)
    {
        calcParam.decorativeHrdParam.bufferSizeInKB  = calcParam.bufferSizeInKB;
        calcParam.decorativeHrdParam.initialDelayInKB = mfx.InitialDelayInKB * multiplier;
        calcParam.decorativeHrdParam.targetKbps       = mfx.TargetKbps       * multiplier;
        calcParam.decorativeHrdParam.maxKbps          = mfx.MaxKbps > 0 ? mfx.MaxKbps       * multiplier : calcParam.decorativeHrdParam.targetKbps;
    }

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP
        && mfx.RateControlMethod != MFX_RATECONTROL_ICQ
        && mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
    {
        calcParam.initialDelayInKB = mfx.InitialDelayInKB * multiplier;
        calcParam.targetKbps       = mfx.TargetKbps       * multiplier;
        calcParam.maxKbps          = mfx.MaxKbps          * multiplier;
    }
    else
    {
        calcParam.bufferSizeInKB = calcParam.initialDelayInKB = calcParam.maxKbps = 0;
    }
    if (mfx.RateControlMethod == MFX_RATECONTROL_LA ||
        mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD ||
        mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
        calcParam.WinBRCMaxAvgKbps = m_extOpt3.WinBRCMaxAvgKbps * multiplier;

    if (IsSvcProfile(mfx.CodecProfile))
    {
        calcParam.numTemporalLayer = 0;
        calcParam.tid[0]   = 0;
        calcParam.scale[0] = 1;

        for (mfxU32 did = 8; did > 0; did--)
        {
            if (m_extSvcSeqDescr.DependencyLayer[did - 1].Active)
            {
                // 'numTemporalLayers' is used as temporal fix for array bound violation (calcParam.tid[], calcParam.scale[])
                // FIXME: need to implement correct checking for 'TemporalNum' (this param is used before calling CheckVideoParamQueryLike())
                mfxU16 numTemporalLayers = IPP_MIN(m_extSvcSeqDescr.DependencyLayer[did - 1].TemporalNum, 8);
                for (mfxU32 tidx = 0; tidx < numTemporalLayers; tidx++)
                {
                    mfxU32 tid = m_extSvcSeqDescr.DependencyLayer[did - 1].TemporalId[tidx];
                    calcParam.tid[calcParam.numTemporalLayer]   = tid;
                    calcParam.scale[calcParam.numTemporalLayer] = m_extSvcSeqDescr.TemporalScale[tid];
                    calcParam.numTemporalLayer++;
                }
                break;
            }
        }
    }
    else
    {
        calcParam.numTemporalLayer = 0;
        calcParam.tid[0]   = 0;
        calcParam.scale[0] = 1;
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (m_extTempLayers.Layer[i].Scale != 0)
            {
                calcParam.tid[calcParam.numTemporalLayer]   = i;
                calcParam.scale[calcParam.numTemporalLayer] = m_extTempLayers.Layer[i].Scale;
                calcParam.numTemporalLayer++;
            }
        }

        if (calcParam.numTemporalLayer)
            calcParam.lyncMode = 1;

        calcParam.numDependencyLayer = 1;
        calcParam.numLayersTotal     = 1;
    }

    if (IsMvcProfile(mfx.CodecProfile))
    {
        mfxExtMVCSeqDesc * extMvc = GetExtBuffer(*this);
        if (extMvc->NumView)
        {
            calcParam.mvcPerViewPar.bufferSizeInKB   = calcParam.bufferSizeInKB / extMvc->NumView;
            if (mfx.RateControlMethod != MFX_RATECONTROL_CQP
                && mfx.RateControlMethod != MFX_RATECONTROL_ICQ
                && mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ)
            {
                calcParam.mvcPerViewPar.initialDelayInKB = calcParam.initialDelayInKB / extMvc->NumView;
                calcParam.mvcPerViewPar.targetKbps       = calcParam.targetKbps / extMvc->NumView;
                calcParam.mvcPerViewPar.maxKbps          = calcParam.maxKbps / extMvc->NumView;
            }
            else
            {
                calcParam.mvcPerViewPar.initialDelayInKB = calcParam.mvcPerViewPar.targetKbps = calcParam.mvcPerViewPar.maxKbps = 0;
            }
        }
        calcParam.mvcPerViewPar.codecLevel       = mfx.CodecLevel;
    }

    if (IsSvcProfile(mfx.CodecProfile))
    {
        calcParam.numLayersTotal     = 0;
        calcParam.numDependencyLayer = 0;
        for (mfxU32 i = 0; i < 8; i++)
        {
            if (m_extSvcSeqDescr.DependencyLayer[i].Active)
            {
                calcParam.did[calcParam.numDependencyLayer] = i;
                calcParam.numLayerOffset[calcParam.numDependencyLayer] = calcParam.numLayersTotal;
                calcParam.numLayersTotal +=
                    m_extSvcSeqDescr.DependencyLayer[i].QualityNum *
                    m_extSvcSeqDescr.DependencyLayer[i].TemporalNum;
                calcParam.numDependencyLayer++;
            }
        }

        calcParam.dqId1Exists = m_extSvcSeqDescr.DependencyLayer[calcParam.did[0]].QualityNum > 1 ? 1 : 0;
    }
}

void MfxVideoParam::SyncCalculableToVideoParam()
{
    mfxU32 maxVal32 = calcParam.bufferSizeInKB;

    if (mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        maxVal32 = IPP_MAX(maxVal32, calcParam.targetKbps);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
            maxVal32 = IPP_MAX(IPP_MAX(maxVal32, IPP_MAX(calcParam.maxKbps, calcParam.initialDelayInKB)),calcParam.WinBRCMaxAvgKbps);
    }

    mfx.BRCParamMultiplier = mfxU16((maxVal32 + 0x10000) / 0x10000);
    if (calcParam.cqpHrdMode == 0 || calcParam.bufferSizeInKB)
    {
        mfx.BufferSizeInKB     = mfxU16((calcParam.bufferSizeInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);
    }

    if (mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VBR ||
        mfx.RateControlMethod == MFX_RATECONTROL_VCM ||
        mfx.RateControlMethod == MFX_RATECONTROL_AVBR||
        mfx.RateControlMethod == MFX_RATECONTROL_QVBR ||
        bRateControlLA(mfx.RateControlMethod))
    {
        mfx.TargetKbps = mfxU16((calcParam.targetKbps + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);

        if (mfx.RateControlMethod != MFX_RATECONTROL_AVBR)
        {
            mfx.InitialDelayInKB = mfxU16((calcParam.initialDelayInKB + mfx.BRCParamMultiplier - 1) / mfx.BRCParamMultiplier);
            mfx.MaxKbps          = mfxU16((calcParam.maxKbps + mfx.BRCParamMultiplier - 1)         / mfx.BRCParamMultiplier);
        }
    }
    if (mfx.RateControlMethod == MFX_RATECONTROL_LA ||
        mfx.RateControlMethod == MFX_RATECONTROL_LA_HRD ||
        mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
        m_extOpt3.WinBRCMaxAvgKbps = mfxU16((calcParam.WinBRCMaxAvgKbps + mfx.BRCParamMultiplier - 1)         / mfx.BRCParamMultiplier);
}



void MfxVideoParam::Construct(mfxVideoParam const & par)
{
    mfxVideoParam & base = *this;
    base = par;

    Zero(m_extParam);
    Zero(calcParam);

    NumExtParam = 0;

#define CONSTRUCT_EXT_BUFFER(type, name)        \
    InitExtBufHeader(name);                     \
    if (type * opts = GetExtBuffer(par))        \
        name = *opts;                           \
    m_extParam[NumExtParam++] = &name.Header;

    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption,         m_extOpt);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOptionSPSPPS,   m_extOptSpsPps);
    CONSTRUCT_EXT_BUFFER(mfxExtPAVPOption,           m_extOptPavp);
    CONSTRUCT_EXT_BUFFER(mfxExtVideoSignalInfo,      m_extVideoSignal);
    CONSTRUCT_EXT_BUFFER(mfxExtOpaqueSurfaceAlloc,   m_extOpaque);
    CONSTRUCT_EXT_BUFFER(mfxExtMVCSeqDesc,           m_extMvcSeqDescr);
    CONSTRUCT_EXT_BUFFER(mfxExtPictureTimingSEI,     m_extPicTiming);
    CONSTRUCT_EXT_BUFFER(mfxExtAvcTemporalLayers,    m_extTempLayers);
    CONSTRUCT_EXT_BUFFER(mfxExtSVCSeqDesc,           m_extSvcSeqDescr);
    CONSTRUCT_EXT_BUFFER(mfxExtSVCRateControl,       m_extSvcRateCtrl);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOptionDDI,      m_extOptDdi);
    CONSTRUCT_EXT_BUFFER(mfxExtDumpFiles,            m_extDumpFiles);
    CONSTRUCT_EXT_BUFFER(mfxExtSpsHeader,            m_extSps);
    CONSTRUCT_EXT_BUFFER(mfxExtPpsHeader,            m_extPps);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption2,        m_extOpt2);
    CONSTRUCT_EXT_BUFFER(mfxExtEncoderResetOption,   m_extEncResetOpt);
    CONSTRUCT_EXT_BUFFER(mfxExtEncoderROI,           m_extEncRoi);
    CONSTRUCT_EXT_BUFFER(mfxExtCodingOption3,        m_extOpt3);
    CONSTRUCT_EXT_BUFFER(mfxExtChromaLocInfo,        m_extChromaLoc);
    CONSTRUCT_EXT_BUFFER(mfxExtFeiParam,             m_extFeiParam);
    CONSTRUCT_EXT_BUFFER(mfxExtSpecialEncodingModes, m_extSpecModes);
    CONSTRUCT_EXT_BUFFER(mfxExtPredWeightTable,      m_extPwt);
    CONSTRUCT_EXT_BUFFER(mfxExtDirtyRect,            m_extDirtyRect);
    CONSTRUCT_EXT_BUFFER(mfxExtMoveRect,             m_extMoveRect);
    CONSTRUCT_EXT_BUFFER(mfxExtFeiCodingOption,      m_extFeiOpt);
#undef CONSTRUCT_EXT_BUFFER

    ExtParam = m_extParam;
    assert(NumExtParam == mfxU16(sizeof m_extParam / sizeof m_extParam[0]));
}

void MfxVideoParam::ConstructMvcSeqDesc(
    mfxExtMVCSeqDesc const & desc)
{
    m_extMvcSeqDescr.NumView        = desc.NumView;
    m_extMvcSeqDescr.NumViewAlloc   = desc.NumViewAlloc;
    m_extMvcSeqDescr.View           = 0;
    m_extMvcSeqDescr.NumViewId      = desc.NumViewId;
    m_extMvcSeqDescr.NumViewIdAlloc = desc.NumViewIdAlloc;
    m_extMvcSeqDescr.ViewId         = 0;
    m_extMvcSeqDescr.NumOP          = desc.NumOP;
    m_extMvcSeqDescr.NumOPAlloc     = desc.NumOPAlloc;
    m_extMvcSeqDescr.OP             = 0;
    m_extMvcSeqDescr.NumRefsTotal   = desc.NumRefsTotal;

    if (desc.View)
    {
        m_storageView.resize(desc.NumView);
        std::copy(desc.View,   desc.View   + desc.NumView,   m_storageView.begin());
        m_extMvcSeqDescr.View           = &m_storageView[0];

        if (desc.ViewId && desc.OP)
        {
            m_storageOp.resize(desc.NumOP);
            m_storageViewId.resize(desc.NumViewId);

            std::copy(desc.OP,     desc.OP     + desc.NumOP,     m_storageOp.begin());
            std::copy(desc.ViewId, desc.ViewId + desc.NumViewId, m_storageViewId.begin());

            for (size_t i = 0; i < m_storageOp.size(); ++i)
            {
                ptrdiff_t offset = desc.OP[i].TargetViewId - desc.ViewId;
                m_storageOp[i].TargetViewId = &m_storageViewId[0] + offset;
            }

            m_extMvcSeqDescr.ViewId         = &m_storageViewId[0];
            m_extMvcSeqDescr.OP             = &m_storageOp[0];
        }
    }
}

void MfxVideoParam::ApplyDefaultsToMvcSeqDesc()
{
    if (false == IsMvcProfile(mfx.CodecProfile))
    {
        m_extMvcSeqDescr.NumView = 1;
        return;
    }

    if (m_extMvcSeqDescr.NumView == 0)
        m_extMvcSeqDescr.NumView = 2;

    if (m_extMvcSeqDescr.View == 0)
    {
        m_extMvcSeqDescr.NumViewAlloc = m_extMvcSeqDescr.NumView;
        m_storageView.resize(m_extMvcSeqDescr.NumView);
        Zero(m_storageView);
        for (size_t i = 0; i < m_storageView.size(); i++)
            m_storageView[i].ViewId = mfxU16(i);
        m_extMvcSeqDescr.View           = &m_storageView[0];
    }

    if (m_extMvcSeqDescr.ViewId == 0)
    {
        m_extMvcSeqDescr.NumViewId = m_extMvcSeqDescr.NumViewIdAlloc = m_extMvcSeqDescr.NumView;
        m_storageViewId.resize(m_extMvcSeqDescr.NumViewId);
        Zero(m_storageViewId);
        for (size_t i = 0; i < m_extMvcSeqDescr.NumViewIdAlloc; i++)
            m_storageViewId[i] = mfxU16(i);
        m_extMvcSeqDescr.ViewId         = &m_storageViewId[0];
    }

    if (m_extMvcSeqDescr.OP == 0)
    {
        m_extMvcSeqDescr.NumOP = 1;
        m_extMvcSeqDescr.NumOPAlloc = 1;
        m_storageOp.resize(m_extMvcSeqDescr.NumOP);
        Zero(m_storageOp);

        m_storageOp[0].TemporalId     = 0;
        m_storageOp[0].LevelIdc       = mfx.CodecLevel;
        m_storageOp[0].NumViews       = mfxU16(m_storageViewId.size());
        m_storageOp[0].NumTargetViews = mfxU16(m_storageViewId.size());
        m_storageOp[0].TargetViewId   = &m_storageViewId[0];

        m_extMvcSeqDescr.OP             = m_storageOp.size() ? &m_storageOp[0] : 0;
    }
    else
    {
        for (mfxU32 i = 0; i < m_extMvcSeqDescr.NumOP; i++)
        {
            if (m_extMvcSeqDescr.OP[i].LevelIdc == 0)
                m_extMvcSeqDescr.OP[i].LevelIdc = mfx.CodecLevel;
        }
    }
}

AspectRatioConverter::AspectRatioConverter(mfxU16 sarw, mfxU16 sarh) :
    m_sarIdc(0),
    m_sarWidth(0),
    m_sarHeight(0)
{
    if (sarw != 0 && sarh != 0)
    {
        for (mfxU8 i = 1; i < sizeof(TABLE_E1) / sizeof(TABLE_E1[0]); i++)
        {
            if ((sarw % TABLE_E1[i].w) == 0 &&
                (sarh % TABLE_E1[i].h) == 0 &&
                (sarw / TABLE_E1[i].w) == (sarh / TABLE_E1[i].h))
            {
                m_sarIdc    = i;
                m_sarWidth  = TABLE_E1[i].w;
                m_sarHeight = TABLE_E1[i].h;
                return;
            }
        }

        m_sarIdc    = EXTENDED_SAR;
        m_sarWidth  = sarw;
        m_sarHeight = sarh;
    }
}

AspectRatioConverter::AspectRatioConverter(mfxU8 sarIdc, mfxU16 sarw, mfxU16 sarh)
{
    if (sarIdc < sizeof(TABLE_E1) / sizeof(TABLE_E1[0]))
    {
        m_sarIdc    = sarIdc;
        m_sarWidth  = TABLE_E1[sarIdc].w;
        m_sarHeight = TABLE_E1[sarIdc].h;
    }
    else
    {
        m_sarIdc    = EXTENDED_SAR;
        m_sarWidth  = sarw;
        m_sarHeight = sarh;
    }
}

namespace
{
#   ifdef min
#       undef min
#   endif
#   ifdef max
#       undef max
#   endif

    template <class Tfrom>
    class RangeChecker
    {
    public:
        RangeChecker(Tfrom from) : m_from(from) {}

        template <class Tto> operator Tto() const
        {
            if (std::numeric_limits<Tto>::min() <= m_from && m_from <= std::numeric_limits<Tto>::max())
                return static_cast<Tto>(m_from);
            throw InvalidBitstream();
        }

    private:
        Tfrom m_from;
    };

    class InputBitstreamCheckedRange
    {
    public:
        InputBitstreamCheckedRange(InputBitstream & impl) : m_impl(impl) {}

        mfxU32 NumBitsRead() const { return m_impl.NumBitsRead(); }
        mfxU32 NumBitsLeft() const { return m_impl.NumBitsLeft(); }

        mfxU8 GetBit() { return static_cast<mfxU8>(m_impl.GetBit()); }

        RangeChecker<mfxU32> GetBits(mfxU32 nbits) { return m_impl.GetBits(nbits); }
        RangeChecker<mfxU32> GetUe()               { return m_impl.GetUe(); }
        RangeChecker<mfxI32> GetSe()               { return m_impl.GetSe(); }

    private:
        void operator =(InputBitstreamCheckedRange const &);
        InputBitstream & m_impl;
    };

    void ReadScalingList(
        InputBitstreamCheckedRange & reader,
        mfxU8 *                      scalingList,
        mfxU32                       size)
    {
        mfxU8 lastScale = 8;
        mfxU8 nextScale = 8;

        for (mfxU32 i = 0; i < size; i++)
        {
            if (nextScale != 0)
            {
                mfxI32 deltaScale = reader.GetSe();
                if (deltaScale < -128 || deltaScale > 127)
                    throw InvalidBitstream();

                nextScale = mfxU8((lastScale + deltaScale + 256) & 0xff);
            }

            scalingList[i] = (nextScale == 0) ? lastScale : nextScale;
            lastScale = scalingList[i];
        }
    }

    void ReadHrdParameters(
        InputBitstreamCheckedRange & reader,
        HrdParameters &              hrd)
    {
        hrd.cpbCntMinus1                        = reader.GetUe();

        if (hrd.cpbCntMinus1 > 31)
            throw InvalidBitstream();

        hrd.bitRateScale                        = reader.GetBits(4);
        hrd.cpbSizeScale                        = reader.GetBits(4);

        for (mfxU32 i = 0; i <= hrd.cpbCntMinus1; i++)
        {
            hrd.bitRateValueMinus1[i]           = reader.GetUe();
            hrd.cpbSizeValueMinus1[i]           = reader.GetUe();
            hrd.cbrFlag[i]                      = reader.GetBit();
        }

        hrd.initialCpbRemovalDelayLengthMinus1  = reader.GetBits(5);
        hrd.cpbRemovalDelayLengthMinus1         = reader.GetBits(5);
        hrd.dpbOutputDelayLengthMinus1          = reader.GetBits(5);
        hrd.timeOffsetLength                    = reader.GetBits(5);
    }

    void WriteHrdParameters(
        OutputBitstream &     writer,
        HrdParameters const & hrd)
    {
        writer.PutUe(hrd.cpbCntMinus1);
        writer.PutBits(hrd.bitRateScale, 4);
        writer.PutBits(hrd.cpbSizeScale, 4);

        for (mfxU32 i = 0; i <= hrd.cpbCntMinus1; i++)
        {
            writer.PutUe(hrd.bitRateValueMinus1[i]);
            writer.PutUe(hrd.cpbSizeValueMinus1[i]);
            writer.PutBit(hrd.cbrFlag[i]);
        }

        writer.PutBits(hrd.initialCpbRemovalDelayLengthMinus1, 5);
        writer.PutBits(hrd.cpbRemovalDelayLengthMinus1, 5);
        writer.PutBits(hrd.dpbOutputDelayLengthMinus1, 5);
        writer.PutBits(hrd.timeOffsetLength, 5);
    }

    bool MoreRbspData(InputBitstream reader)
    {
        mfxU32 bitsLeft = reader.NumBitsLeft();

        if (bitsLeft == 0)
            return false;

        if (reader.GetBit() == 0)
            return true;

        --bitsLeft;
        for (; bitsLeft > 0; --bitsLeft)
            if (reader.GetBit() == 1)
                return true;

        return false;
    }
};

void MfxHwH264Encode::ReadSpsHeader(
    InputBitstream &  is,
    mfxExtSpsHeader & sps)
{
    // set defaults for optional fields
    Zero(sps.vui);
    sps.chromaFormatIdc               = 1;
    sps.vui.videoFormat               = 5;
    sps.vui.colourPrimaries           = 2;
    sps.vui.transferCharacteristics   = 2;
    sps.vui.matrixCoefficients        = 2;
    sps.vui.flags.fixedFrameRate      = 1;

    InputBitstreamCheckedRange reader(is);

    mfxU32 unused                                           = reader.GetBit(); // forbiddenZeroBit

    sps.nalRefIdc                                           = reader.GetBits(2);
    if (sps.nalRefIdc == 0)
        throw InvalidBitstream();

    sps.nalUnitType                                         = reader.GetBits(5);
    if (sps.nalUnitType != 7)
        throw InvalidBitstream();

    sps.profileIdc                                          = reader.GetBits(8);
    sps.constraints.set0                                    = reader.GetBit();
    sps.constraints.set1                                    = reader.GetBit();
    sps.constraints.set2                                    = reader.GetBit();
    sps.constraints.set3                                    = reader.GetBit();
    sps.constraints.set4                                    = reader.GetBit();
    sps.constraints.set5                                    = reader.GetBit();
    sps.constraints.set6                                    = reader.GetBit();
    sps.constraints.set7                                    = reader.GetBit();
    sps.levelIdc                                            = reader.GetBits(8);
    sps.seqParameterSetId                                   = reader.GetUe();

    if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
        sps.profileIdc == 244 || sps.profileIdc ==  44 || sps.profileIdc ==  83 ||
        sps.profileIdc ==  86 || sps.profileIdc == 118 || sps.profileIdc == 128)
    {
        sps.chromaFormatIdc                                 = reader.GetUe();
        if (sps.chromaFormatIdc == 3)
            unused                                          = reader.GetBit(); // separateColourPlaneFlag

        sps.bitDepthLumaMinus8                              = reader.GetUe();
        sps.bitDepthChromaMinus8                            = reader.GetUe();

        sps.qpprimeYZeroTransformBypassFlag                 = reader.GetBit();
        sps.seqScalingMatrixPresentFlag                     = reader.GetBit();

        if (sps.seqScalingMatrixPresentFlag)
        {
            for (mfxU32 i = 0; i < ((sps.chromaFormatIdc != 3) ? 8u : 12u); i++)
            {
                sps.seqScalingListPresentFlag[i]            = reader.GetBit();
                if (sps.seqScalingListPresentFlag[i])
                {
                    (i < 6)
                        ? ReadScalingList(reader, sps.scalingList4x4[i],     16)
                        : ReadScalingList(reader, sps.scalingList8x8[i - 6], 64);
                }
            }
        }
    }

    sps.log2MaxFrameNumMinus4                               = reader.GetUe();
    sps.picOrderCntType                                     = reader.GetUe();

    if (sps.picOrderCntType == 0)
    {
        sps.log2MaxPicOrderCntLsbMinus4                     = reader.GetUe();
    }
    else if (sps.picOrderCntType == 1)
    {
        sps.deltaPicOrderAlwaysZeroFlag                     = reader.GetBit();
        sps.offsetForNonRefPic                              = reader.GetSe();
        sps.offsetForTopToBottomField                       = reader.GetSe();
        sps.numRefFramesInPicOrderCntCycle                  = reader.GetUe();

        for (mfxU32 i = 0; i < sps.numRefFramesInPicOrderCntCycle; i++)
            sps.offsetForRefFrame[i]                        = reader.GetSe();
    }

    sps.maxNumRefFrames                                     = reader.GetUe();
    sps.gapsInFrameNumValueAllowedFlag                      = reader.GetBit();
    sps.picWidthInMbsMinus1                                 = reader.GetUe();
    sps.picHeightInMapUnitsMinus1                           = reader.GetUe();
    sps.frameMbsOnlyFlag                                    = reader.GetBit();

    if (!sps.frameMbsOnlyFlag)
        sps.mbAdaptiveFrameFieldFlag                        = reader.GetBit();

    sps.direct8x8InferenceFlag                              = reader.GetBit();
    sps.frameCroppingFlag                                   = reader.GetBit();

    if (sps.frameCroppingFlag)
    {
        sps.frameCropLeftOffset                             = reader.GetUe();
        sps.frameCropRightOffset                            = reader.GetUe();
        sps.frameCropTopOffset                              = reader.GetUe();
        sps.frameCropBottomOffset                           = reader.GetUe();
    }

    sps.vuiParametersPresentFlag                            = reader.GetBit();
    if (sps.vuiParametersPresentFlag)
    {
        sps.vui.flags.aspectRatioInfoPresent                = reader.GetBit();
        if (sps.vui.flags.aspectRatioInfoPresent)
        {
            sps.vui.aspectRatioIdc                          = reader.GetBits(8);
            if (sps.vui.aspectRatioIdc == 255)
            {
                sps.vui.sarWidth                            = reader.GetBits(16);
                sps.vui.sarHeight                           = reader.GetBits(16);
            }
        }

        sps.vui.flags.overscanInfoPresent                   = reader.GetBit();
        if (sps.vui.flags.overscanInfoPresent)
            sps.vui.flags.overscanAppropriate               = reader.GetBit();

        sps.vui.flags.videoSignalTypePresent                = reader.GetBit();
        if (sps.vui.flags.videoSignalTypePresent)
        {
            sps.vui.videoFormat                             = reader.GetBits(3);
            sps.vui.flags.videoFullRange                    = reader.GetBit();
            sps.vui.flags.colourDescriptionPresent          = reader.GetBit();

            if (sps.vui.flags.colourDescriptionPresent)
            {
                sps.vui.colourPrimaries                     = reader.GetBits(8);
                sps.vui.transferCharacteristics             = reader.GetBits(8);
                sps.vui.matrixCoefficients                  = reader.GetBits(8);
            }
        }

        sps.vui.flags.chromaLocInfoPresent                  = reader.GetBit();
        if (sps.vui.flags.chromaLocInfoPresent)
        {
            sps.vui.chromaSampleLocTypeTopField             = reader.GetUe();
            sps.vui.chromaSampleLocTypeBottomField          = reader.GetUe();
        }

        sps.vui.flags.timingInfoPresent                     = reader.GetBit();
        if (sps.vui.flags.timingInfoPresent)
        {
            sps.vui.numUnitsInTick                          = reader.GetBits(32);
            sps.vui.timeScale                               = reader.GetBits(32);
            sps.vui.flags.fixedFrameRate                    = reader.GetBit();
        }

        sps.vui.flags.nalHrdParametersPresent               = reader.GetBit();
        if (sps.vui.flags.nalHrdParametersPresent)
            ReadHrdParameters(reader, sps.vui.nalHrdParameters);

        sps.vui.flags.vclHrdParametersPresent               = reader.GetBit();
        if (sps.vui.flags.vclHrdParametersPresent)
            ReadHrdParameters(reader, sps.vui.vclHrdParameters);

        if (sps.vui.flags.nalHrdParametersPresent || sps.vui.flags.vclHrdParametersPresent)
            sps.vui.flags.lowDelayHrd                       = reader.GetBit();

        sps.vui.flags.picStructPresent                      = reader.GetBit();
        sps.vui.flags.bitstreamRestriction                  = reader.GetBit();

        if (sps.vui.flags.bitstreamRestriction)
        {
            sps.vui.flags.motionVectorsOverPicBoundaries    = reader.GetBit();
            sps.vui.maxBytesPerPicDenom                     = reader.GetUe();
            sps.vui.maxBitsPerMbDenom                       = reader.GetUe();
            sps.vui.log2MaxMvLengthHorizontal               = reader.GetUe();
            sps.vui.log2MaxMvLengthVertical                 = reader.GetUe();
            sps.vui.numReorderFrames                        = reader.GetUe();
            sps.vui.maxDecFrameBuffering                    = reader.GetUe();
        }
    }
}

mfxU8 MfxHwH264Encode::ReadSpsIdOfPpsHeader(
    InputBitstream is)
{
    InputBitstreamCheckedRange reader(is);
    mfxU8 ppsId = reader.GetUe();
    ppsId;

    mfxU8 spsId = reader.GetUe();
    if (spsId > 31)
        throw InvalidBitstream();

    return spsId;
}

void MfxHwH264Encode::ReadPpsHeader(
    InputBitstream &        is,
    mfxExtSpsHeader const & sps,
    mfxExtPpsHeader &       pps)
{
    InputBitstreamCheckedRange reader(is);

    mfxU32 unused                                               = reader.GetBit(); // forbiddenZeroBit

    pps.nalRefIdc                                               = reader.GetBits(2);
    if (pps.nalRefIdc == 0)
        throw InvalidBitstream();

    mfxU8 nalUnitType                                           = reader.GetBits(5);
    if (nalUnitType != 8)
        throw InvalidBitstream();

    pps.picParameterSetId                                       = reader.GetUe();
    pps.seqParameterSetId                                       = reader.GetUe();

    if (pps.seqParameterSetId != sps.seqParameterSetId)
        throw InvalidBitstream();

    pps.entropyCodingModeFlag                                   = reader.GetBit();

    pps.bottomFieldPicOrderInframePresentFlag                   = reader.GetBit();;

    pps.numSliceGroupsMinus1                                    = reader.GetUe();
    if (pps.numSliceGroupsMinus1 > 0)
    {
        if (pps.numSliceGroupsMinus1 > 7)
            throw InvalidBitstream();

        pps.sliceGroupMapType                                   = reader.GetUe();
        if (pps.sliceGroupMapType == 0)
        {
            for (mfxU32 i = 0; i <= pps.numSliceGroupsMinus1; i++)
                pps.sliceGroupInfo.t0.runLengthMinus1[i]        = reader.GetUe();
        }
        else if (pps.sliceGroupMapType == 2)
        {
            for (mfxU32 i = 0; i < pps.numSliceGroupsMinus1; i++)
            {
                pps.sliceGroupInfo.t2.topLeft[i]                = reader.GetUe();
                pps.sliceGroupInfo.t2.bottomRight[i]            = reader.GetUe();
            }
        }
        else if (
            pps.sliceGroupMapType == 3 ||
            pps.sliceGroupMapType == 4 ||
            pps.sliceGroupMapType == 5)
        {
            pps.sliceGroupInfo.t3.sliceGroupChangeDirectionFlag = reader.GetBit();
            pps.sliceGroupInfo.t3.sliceGroupChangeRate          = reader.GetUe();
        }
        else if (pps.sliceGroupMapType == 6)
        {
            pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1       = reader.GetUe();
            for (mfxU32 i = 0; i <= pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1; i++)
                unused                                          = reader.GetBits(CeilLog2(pps.numSliceGroupsMinus1 + 1)); // sliceGroupId
        }
    }

    pps.numRefIdxL0DefaultActiveMinus1                          = reader.GetUe();
    pps.numRefIdxL1DefaultActiveMinus1                          = reader.GetUe();

    pps.weightedPredFlag                                        = reader.GetBit();
    pps.weightedBipredIdc                                       = reader.GetBits(2);
    pps.picInitQpMinus26                                        = reader.GetSe();
    pps.picInitQsMinus26                                        = reader.GetSe();
    pps.chromaQpIndexOffset                                     = reader.GetSe();;

    pps.deblockingFilterControlPresentFlag                      = reader.GetBit();
    pps.constrainedIntraPredFlag                                = reader.GetBit();
    pps.redundantPicCntPresentFlag                              = reader.GetBit();

    pps.moreRbspData = MoreRbspData(is);
    if (pps.moreRbspData)
    {
        pps.transform8x8ModeFlag                                = reader.GetBit();
        pps.picScalingMatrixPresentFlag                         = reader.GetBit();
        if (pps.picScalingMatrixPresentFlag)
        {
            for (mfxU32 i = 0; i < 6 + ((sps.chromaFormatIdc != 3) ? 2u : 6u) * pps.transform8x8ModeFlag; i++)
            {
                mfxU32 picScalingListPresentFlag                = reader.GetBit();
                if (picScalingListPresentFlag)
                {
                    (i < 6)
                        ? ReadScalingList(reader, pps.scalingList4x4[i],     16)
                        : ReadScalingList(reader, pps.scalingList8x8[i - 6], 64);
                }
            }
        }

        pps.secondChromaQpIndexOffset                           = reader.GetSe();
    }
}

namespace
{
    void WriteSpsData(
        OutputBitstream &       writer,
        mfxExtSpsHeader const & sps)
    {
        writer.PutBits(sps.profileIdc, 8);
        writer.PutBit(sps.constraints.set0);
        writer.PutBit(sps.constraints.set1);
        writer.PutBit(sps.constraints.set2);
        writer.PutBit(sps.constraints.set3);
        writer.PutBit(sps.constraints.set4);
        writer.PutBit(sps.constraints.set5);
        writer.PutBit(sps.constraints.set6);
        writer.PutBit(sps.constraints.set7);
        writer.PutBits(sps.levelIdc, 8);
        writer.PutUe(sps.seqParameterSetId);
        if (sps.profileIdc == 100 || sps.profileIdc == 110 || sps.profileIdc == 122 ||
            sps.profileIdc == 244 || sps.profileIdc ==  44 || sps.profileIdc ==  83 ||
            sps.profileIdc ==  86 || sps.profileIdc == 118 || sps.profileIdc == 128)
        {
            writer.PutUe(sps.chromaFormatIdc);
            if (sps.chromaFormatIdc == 3)
                writer.PutBit(sps.separateColourPlaneFlag);
            writer.PutUe(sps.bitDepthLumaMinus8);
            writer.PutUe(sps.bitDepthChromaMinus8);
            writer.PutBit(sps.qpprimeYZeroTransformBypassFlag);
            writer.PutBit(sps.seqScalingMatrixPresentFlag);
            if (sps.seqScalingMatrixPresentFlag)
            {
                assert("seq_scaling_matrix is unsupported");
            }
        }
        writer.PutUe(sps.log2MaxFrameNumMinus4);
        writer.PutUe(sps.picOrderCntType);
        if (sps.picOrderCntType == 0)
        {
            writer.PutUe(sps.log2MaxPicOrderCntLsbMinus4);
        }
        else if (sps.picOrderCntType == 1)
        {
            writer.PutBit(sps.deltaPicOrderAlwaysZeroFlag);
            writer.PutSe(sps.offsetForNonRefPic);
            writer.PutSe(sps.offsetForTopToBottomField);
            writer.PutUe(sps.numRefFramesInPicOrderCntCycle);
            for (mfxU32 i = 0; i < sps.numRefFramesInPicOrderCntCycle; i++)
                writer.PutSe(sps.offsetForRefFrame[i]);
        }
        writer.PutUe(sps.maxNumRefFrames);
        writer.PutBit(sps.gapsInFrameNumValueAllowedFlag);
        writer.PutUe(sps.picWidthInMbsMinus1);
        writer.PutUe(sps.picHeightInMapUnitsMinus1);
        writer.PutBit(sps.frameMbsOnlyFlag);
        if (!sps.frameMbsOnlyFlag)
            writer.PutBit(sps.mbAdaptiveFrameFieldFlag);
        writer.PutBit(sps.direct8x8InferenceFlag);
        writer.PutBit(sps.frameCroppingFlag);
        if (sps.frameCroppingFlag)
        {
            writer.PutUe(sps.frameCropLeftOffset);
            writer.PutUe(sps.frameCropRightOffset);
            writer.PutUe(sps.frameCropTopOffset);
            writer.PutUe(sps.frameCropBottomOffset);
        }
        writer.PutBit(sps.vuiParametersPresentFlag);
        if (sps.vuiParametersPresentFlag)
        {
            writer.PutBit(sps.vui.flags.aspectRatioInfoPresent);
            if (sps.vui.flags.aspectRatioInfoPresent)
            {
                writer.PutBits(sps.vui.aspectRatioIdc, 8);
                if (sps.vui.aspectRatioIdc == 255)
                {
                    writer.PutBits(sps.vui.sarWidth, 16);
                    writer.PutBits(sps.vui.sarHeight, 16);
                }
            }
            writer.PutBit(sps.vui.flags.overscanInfoPresent);
            if (sps.vui.flags.overscanInfoPresent)
                writer.PutBit(sps.vui.flags.overscanAppropriate);
            writer.PutBit(sps.vui.flags.videoSignalTypePresent);
            if (sps.vui.flags.videoSignalTypePresent)
            {
                writer.PutBits(sps.vui.videoFormat, 3);
                writer.PutBit(sps.vui.flags.videoFullRange);
                writer.PutBit(sps.vui.flags.colourDescriptionPresent);
                if (sps.vui.flags.colourDescriptionPresent)
                {
                    writer.PutBits(sps.vui.colourPrimaries, 8);
                    writer.PutBits(sps.vui.transferCharacteristics, 8);
                    writer.PutBits(sps.vui.matrixCoefficients, 8);
                }
            }
            writer.PutBit(sps.vui.flags.chromaLocInfoPresent);
            if (sps.vui.flags.chromaLocInfoPresent)
            {
                writer.PutUe(sps.vui.chromaSampleLocTypeTopField);
                writer.PutUe(sps.vui.chromaSampleLocTypeBottomField);
            }
            writer.PutBit(sps.vui.flags.timingInfoPresent);
            if (sps.vui.flags.timingInfoPresent)
            {
                writer.PutBits(sps.vui.numUnitsInTick, 32);
                writer.PutBits(sps.vui.timeScale, 32);
                writer.PutBit(sps.vui.flags.fixedFrameRate);
            }
            writer.PutBit(sps.vui.flags.nalHrdParametersPresent);
            if (sps.vui.flags.nalHrdParametersPresent)
                WriteHrdParameters(writer, sps.vui.nalHrdParameters);
            writer.PutBit(sps.vui.flags.vclHrdParametersPresent);
            if (sps.vui.flags.vclHrdParametersPresent)
                WriteHrdParameters(writer, sps.vui.vclHrdParameters);
            if (sps.vui.flags.nalHrdParametersPresent || sps.vui.flags.vclHrdParametersPresent)
                writer.PutBit(sps.vui.flags.lowDelayHrd);
            writer.PutBit(sps.vui.flags.picStructPresent);
            writer.PutBit(sps.vui.flags.bitstreamRestriction);
            if (sps.vui.flags.bitstreamRestriction)
            {
                writer.PutBit(sps.vui.flags.motionVectorsOverPicBoundaries);
                writer.PutUe(sps.vui.maxBytesPerPicDenom);
                writer.PutUe(sps.vui.maxBitsPerMbDenom);
                writer.PutUe(sps.vui.log2MaxMvLengthHorizontal);
                writer.PutUe(sps.vui.log2MaxMvLengthVertical);
                writer.PutUe(sps.vui.numReorderFrames);
                writer.PutUe(sps.vui.maxDecFrameBuffering);
            }
        }
    }

    void WriteSpsMvcExtension(
        OutputBitstream &        writer,
        mfxExtMVCSeqDesc const & extMvc)
    {
        writer.PutUe(extMvc.NumView - 1);                       // num_views_minus1
        for (mfxU32 i = 0; i < extMvc.NumView; i++)
            writer.PutUe(extMvc.View[i].ViewId);                // view_id[i]
        for (mfxU32 i = 1; i < extMvc.NumView; i++)
        {
            writer.PutUe(extMvc.View[i].NumAnchorRefsL0);       // num_anchor_refs_l0[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumAnchorRefsL0; j++)
                writer.PutUe(extMvc.View[i].AnchorRefL0[j]);    // anchor_ref_l0[i][j]
            writer.PutUe(extMvc.View[i].NumAnchorRefsL1);       // num_anchor_refs_l1[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumAnchorRefsL1; j++)
                writer.PutUe(extMvc.View[i].AnchorRefL1[j]);    // anchor_ref_l1[i][j]
        }
        for (mfxU32 i = 1; i < extMvc.NumView; i++)
        {
            writer.PutUe(extMvc.View[i].NumNonAnchorRefsL0);    // num_non_anchor_refs_l0[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumNonAnchorRefsL0; j++)
                writer.PutUe(extMvc.View[i].NonAnchorRefL0[j]); // non_anchor_ref_l0[i][j]
            writer.PutUe(extMvc.View[i].NumNonAnchorRefsL1);    // num_non_anchor_refs_l1[i]
            for (mfxU32 j = 0; j < extMvc.View[i].NumNonAnchorRefsL1; j++)
                writer.PutUe(extMvc.View[i].NonAnchorRefL1[j]); // non_anchor_ref_l1[i][j]
        }
        writer.PutUe(extMvc.NumOP - 1);                         // num_level_values_signalled_minus1
        for (mfxU32 i = 0; i < extMvc.NumOP; i++)
        {
            writer.PutBits(extMvc.OP[i].LevelIdc, 8);           // level_idc[i]
            writer.PutUe(0);                                    // num_applicable_ops_minus1[i]
            for (mfxU32 j = 0; j < 1; j++)
            {
                writer.PutBits(extMvc.OP[i].TemporalId, 3);     // applicable_op_temporal_id[i][j]
                writer.PutUe(extMvc.OP[i].NumTargetViews - 1);  // applicable_op_num_target_views_minus1[i][j]
                for (mfxU32 k = 0; k < extMvc.OP[i].NumTargetViews; k++)
                    writer.PutUe(extMvc.OP[i].TargetViewId[k]); // applicable_op_target_view_id[i][j][k]
                writer.PutUe(extMvc.OP[i].NumViews - 1);        // applicable_op_num_views_minus1[i][j]
            }
        }
    }

    void WriteSpsSvcExtension(
        OutputBitstream &          writer,
        mfxExtSpsHeader const &    sps,
        mfxExtSpsSvcHeader const & extSvc)
    {
        mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;

        writer.PutBit(extSvc.interLayerDeblockingFilterControlPresentFlag); // inter_layer_deblocking_filter_control_present_flag
        writer.PutBits(extSvc.extendedSpatialScalabilityIdc, 2);            // extended_spatial_scalability_idc
        if (chromaArrayType == 1 || chromaArrayType == 2)
            writer.PutBit(extSvc.chromaPhaseXPlus1Flag);                    // chroma_phase_x_plus1_flag
        if (chromaArrayType == 1)
            writer.PutBits(extSvc.chromaPhaseYPlus1, 2);                    // chroma_phase_y_plus1
        if (extSvc.extendedSpatialScalabilityIdc == 1)
        {
            if (chromaArrayType > 0)
            {
                writer.PutBit(extSvc.seqRefLayerChromaPhaseXPlus1Flag);     // seq_ref_layer_chroma_phase_x_plus1_flag
                writer.PutBits(extSvc.seqRefLayerChromaPhaseYPlus1, 2);     // seq_ref_layer_chroma_phase_y_plus1
            }
            writer.PutSe(extSvc.seqScaledRefLayerLeftOffset);               // seq_scaled_ref_layer_left_offset
            writer.PutSe(extSvc.seqScaledRefLayerTopOffset);                // seq_scaled_ref_layer_top_offset
            writer.PutSe(extSvc.seqScaledRefLayerRightOffset);              // seq_scaled_ref_layer_right_offset
            writer.PutSe(extSvc.seqScaledRefLayerBottomOffset);             // seq_scaled_ref_layer_bottom_offset
        }
        writer.PutBit(extSvc.seqTcoeffLevelPredictionFlag);                 // seq_tcoeff_level_prediction_flag
        if (extSvc.seqTcoeffLevelPredictionFlag)
            writer.PutBit(extSvc.adaptiveTcoeffLevelPredictionFlag);        // adaptive_tcoeff_level_prediction_flag
        writer.PutBit(extSvc.sliceHeaderRestrictionFlag);                   // slice_header_restriction_flag
    }
}

mfxU32 MfxHwH264Encode::WriteSpsHeader(
    OutputBitstream &       writer,
    mfxExtSpsHeader const & sps)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0); // forbiddenZeroBit
    writer.PutBits(sps.nalRefIdc, 2);
    writer.PutBits(7, 5); // nalUnitType
    WriteSpsData(writer, sps);
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

mfxU32 MfxHwH264Encode::WriteSpsHeader(
    OutputBitstream &        writer,
    mfxExtSpsHeader const &  sps,
    mfxExtBuffer const &     spsExt)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0);                   // forbidden_zero_bit
    writer.PutBits(sps.nalRefIdc, 2);   // nal_ref_idc
    writer.PutBits(sps.nalUnitType, 5); // nal_unit_type

    WriteSpsData(writer, sps);

    if (IsSvcProfile(sps.profileIdc))
    {
        assert(spsExt.BufferId == MFX_EXTBUFF_SPS_SVC_HEADER);
        mfxExtSpsSvcHeader const & extSvc = (mfxExtSpsSvcHeader const &)spsExt;

        WriteSpsSvcExtension(writer, sps, extSvc);
        writer.PutBit(0);               // svc_vui_parameters_present_flag
    }
    else if (IsMvcProfile(sps.profileIdc))
    {
        assert(spsExt.BufferId == MFX_EXTBUFF_MVC_SEQ_DESC);
        mfxExtMVCSeqDesc const & extMvc = (mfxExtMVCSeqDesc const &)spsExt;

        writer.PutBit(1);               // bit_equal_to_one
        WriteSpsMvcExtension(writer, extMvc);
        writer.PutBit(0);               // mvc_vui_parameters_present_flag
    }
    writer.PutBit(0);                   // additional_extension2_flag
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

mfxU32 MfxHwH264Encode::WritePpsHeader(
    OutputBitstream &       writer,
    mfxExtPpsHeader const & pps)
{
    mfxU32 initNumBits = writer.GetNumBits();

    const mfxU8 header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);

    writer.PutBit(0); // forbiddenZeroBit
    writer.PutBits(pps.nalRefIdc, 2);
    writer.PutBits(8, 5); // nalUnitType
    writer.PutUe(pps.picParameterSetId);
    writer.PutUe(pps.seqParameterSetId);
    writer.PutBit(pps.entropyCodingModeFlag);
    writer.PutBit(pps.bottomFieldPicOrderInframePresentFlag);
    writer.PutUe(pps.numSliceGroupsMinus1);
    if (pps.numSliceGroupsMinus1 > 0)
    {
        writer.PutUe(pps.sliceGroupMapType);
        if (pps.sliceGroupMapType == 0)
        {
            for (mfxU32 i = 0; i <= pps.numSliceGroupsMinus1; i++)
                writer.PutUe(pps.sliceGroupInfo.t0.runLengthMinus1[i]);
        }
        else if (pps.sliceGroupMapType == 2)
        {
            for (mfxU32 i = 0; i < pps.numSliceGroupsMinus1; i++)
            {
                writer.PutUe(pps.sliceGroupInfo.t2.topLeft[i]);
                writer.PutUe(pps.sliceGroupInfo.t2.bottomRight[i]);
            }
        }
        else if (
            pps.sliceGroupMapType == 3 ||
            pps.sliceGroupMapType == 4 ||
            pps.sliceGroupMapType == 5)
        {
            writer.PutBit(pps.sliceGroupInfo.t3.sliceGroupChangeDirectionFlag);
            writer.PutUe(pps.sliceGroupInfo.t3.sliceGroupChangeRate);
        }
        else if (pps.sliceGroupMapType == 6)
        {
            writer.PutUe(pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1);
            assert("unsupprted slice_group_map_type = 6");
            for (mfxU32 i = 0; i <= pps.sliceGroupInfo.t6.picSizeInMapUnitsMinus1; i++)
                writer.PutBits(1, CeilLog2(pps.numSliceGroupsMinus1 + 1));
        }
    }
    writer.PutUe(pps.numRefIdxL0DefaultActiveMinus1);
    writer.PutUe(pps.numRefIdxL1DefaultActiveMinus1);
    writer.PutBit(pps.weightedPredFlag);
    writer.PutBits(pps.weightedBipredIdc, 2);
    writer.PutSe(pps.picInitQpMinus26);
    writer.PutSe(pps.picInitQsMinus26);
    writer.PutSe(pps.chromaQpIndexOffset);
    writer.PutBit(pps.deblockingFilterControlPresentFlag);
    writer.PutBit(pps.constrainedIntraPredFlag);
    writer.PutBit(pps.redundantPicCntPresentFlag);
    if (pps.moreRbspData)
    {
        writer.PutBit(pps.transform8x8ModeFlag);
        writer.PutBit(pps.picScalingMatrixPresentFlag);
        if (pps.picScalingMatrixPresentFlag)
        {
            //for(int i=0; i < 6 + ((sps.chroma_format_idc != 3) ? 2:6)*pps.transform8x8ModeFlag; i++){
            for(int i=0; i < 6+2*(!!pps.transform8x8ModeFlag); i++){
                //Put scaling list present flag
                writer.PutBit(pps.picScalingListPresentFlag[i]);
                if( pps.picScalingListPresentFlag[i] ){
                   if( i<6 )
                       WriteScalingList(writer, &pps.scalingList4x4[i][0], 16);
                   else
                       WriteScalingList(writer, &pps.scalingList8x8[i-6][0], 64);
                }
            }
        }
        writer.PutSe(pps.secondChromaQpIndexOffset);
    }
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

void MfxHwH264Encode::WriteScalingList(
    OutputBitstream &       writer,
    const mfxU8* scalingList,
    mfxI32 sizeOfScalingList)
{
    Ipp16s lastScale, nextScale;
    Ipp32s j;

    Ipp16s delta_scale;
    Ipp8s delta_code;
    const Ipp32s* scan;

    lastScale=nextScale=8;

    if( sizeOfScalingList == 16 )
        scan = UMC_H264_ENCODER::dec_single_scan[0];
    else
        scan = UMC_H264_ENCODER::dec_single_scan_8x8[0];

    for( j = 0; j<sizeOfScalingList; j++ ){
         if( nextScale != 0 ){
            delta_scale = (Ipp16s)(scalingList[scan[j]]-lastScale);
            delta_code = (Ipp8s)(delta_scale);
            writer.PutSe(delta_scale);
            nextScale = scalingList[scan[j]];
         }
         lastScale = (nextScale==0) ? lastScale:nextScale;
    }
}

void MfxHwH264Encode::WriteRefPicListModification(
    OutputBitstream &       writer,
    ArrayRefListMod const & refListMod)
{
    writer.PutBit(refListMod.Size() > 0);       // ref_pic_list_modification_flag_l0
    if (refListMod.Size() > 0)
    {
        for (mfxU32 i = 0; i < refListMod.Size(); i++)
        {
            writer.PutUe(refListMod[i].m_idc);  // modification_of_pic_nums_idc
            writer.PutUe(refListMod[i].m_diff); // abs_diff_pic_num_minus1 or
                                                // long_term_pic_num or
                                                // abs_diff_view_idx_minus1
        }

        writer.PutUe(RPLM_END);
    }
}

void MfxHwH264Encode::WriteDecRefPicMarking(
    OutputBitstream &            writer,
    DecRefPicMarkingInfo const & marking,
    mfxU32                       idrPicFlag)
{
    if (idrPicFlag)
    {
        writer.PutBit(marking.no_output_of_prior_pics_flag);    // no_output_of_prior_pics_flag
        writer.PutBit(marking.long_term_reference_flag);        // long_term_reference_flag
    }
    else
    {
        writer.PutBit(marking.mmco.Size() > 0);                 // adaptive_ref_pic_marking_mode_flag
        if (marking.mmco.Size())
        {
            for (mfxU32 i = 0; i < marking.mmco.Size(); i++)
            {
                writer.PutUe(marking.mmco[i]);                  // memory_management_control_operation
                writer.PutUe(marking.value[2 * i]);             // difference_of_pic_nums_minus1 or
                                                                // long_term_pic_num or
                                                                // long_term_frame_idx or
                                                                // max_long_term_frame_idx_plus1
                if (marking.mmco[i] == MMCO_ST_TO_LT)
                    writer.PutUe(marking.value[2 * i + 1]);     // long_term_frame_idx
            }

            writer.PutUe(MMCO_END);
        }
    }
}

mfxU32 MfxHwH264Encode::WriteAud(
    OutputBitstream & writer,
    mfxU32            frameType)
{
    mfxU32 initNumBits = writer.GetNumBits();

    mfxU8 const header[4] = { 0, 0, 0, 1 };
    writer.PutRawBytes(header, header + sizeof header / sizeof header[0]);
    writer.PutBit(0);
    writer.PutBits(0, 2);
    writer.PutBits(9, 5);
    writer.PutBits(ConvertFrameTypeMfx2Ddi(frameType) - 1, 3);
    writer.PutTrailingBits();

    return writer.GetNumBits() - initNumBits;
}

bool MfxHwH264Encode::IsAvcProfile(mfxU32 profile)
{
    return
        IsAvcBaseProfile(profile)           ||
        profile == MFX_PROFILE_AVC_MAIN     ||
        profile == MFX_PROFILE_AVC_EXTENDED ||
        IsAvcHighProfile(profile);
}

bool MfxHwH264Encode::IsAvcBaseProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_BASELINE ||
        profile == MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
}

bool MfxHwH264Encode::IsAvcHighProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_HIGH ||
        profile == MFX_PROFILE_AVC_CONSTRAINED_HIGH ||
        profile == MFX_PROFILE_AVC_PROGRESSIVE_HIGH;
}

bool MfxHwH264Encode::IsMvcProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_STEREO_HIGH ||
        profile == MFX_PROFILE_AVC_MULTIVIEW_HIGH;
}

bool MfxHwH264Encode::IsSvcProfile(mfxU32 profile)
{
    return
        profile == MFX_PROFILE_AVC_SCALABLE_BASELINE ||
        profile == MFX_PROFILE_AVC_SCALABLE_HIGH;
}

bool MfxHwH264Encode::operator ==(
    mfxExtSpsHeader const & lhs,
    mfxExtSpsHeader const & rhs)
{
    // part from the beginning of sps to nalHrdParameters
    mfxU8 const * lhsBegin1 = (mfxU8 const *)&lhs;
    mfxU8 const * rhsBegin1 = (mfxU8 const *)&rhs;
    mfxU8 const * lhsEnd1   = (mfxU8 const *)&lhs.vui.nalHrdParameters;

    // part from the end of vclHrdParameters to the end of sps
    mfxU8 const * lhsBegin2 = (mfxU8 const *)&lhs.vui.maxBytesPerPicDenom;
    mfxU8 const * rhsBegin2 = (mfxU8 const *)&rhs.vui.maxBytesPerPicDenom;
    mfxU8 const * lhsEnd2   = (mfxU8 const *)&lhs + sizeof(lhs);

    if (memcmp(lhsBegin1, rhsBegin1, lhsEnd1 - lhsBegin1) != 0 ||
        memcmp(lhsBegin2, rhsBegin2, lhsEnd2 - lhsBegin2) != 0)
        return false;

    if (lhs.vui.flags.nalHrdParametersPresent)
        if (!Equal(lhs.vui.nalHrdParameters, rhs.vui.nalHrdParameters))
            return false;

    if (lhs.vui.flags.vclHrdParametersPresent)
        if (!Equal(lhs.vui.vclHrdParameters, rhs.vui.vclHrdParameters))
            return false;

    return true;
}

mfxU8 * MfxHwH264Encode::PackPrefixNalUnitSvc(
    mfxU8 *         begin,
    mfxU8 *         end,
    bool            emulationControl,
    DdiTask const & task,
    mfxU32          fieldId,
    mfxU32          nalUnitType)
{
    OutputBitstream obs(begin, end, false);

    mfxU32 idrFlag   = (task.m_type[fieldId] & MFX_FRAMETYPE_IDR) ? 1 : 0;
    mfxU32 nalRefIdc = task.m_nalRefIdc[fieldId];

    mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;

    obs.PutBits(1, 24);                     // 001
    obs.PutBits(0, 1);                      // forbidden_zero_flag
    obs.PutBits(nalRefIdc, 2);              // nal_ref_idc
    obs.PutBits(nalUnitType, 5);            // nal_unit_type
    obs.PutBits(1, 1);                      // svc_extension_flag
    obs.PutBits(idrFlag, 1);                // idr_flag
    obs.PutBits(task.m_pid, 6);             // priority_id
    obs.PutBits(1, 1);                      // no_inter_layer_pred_flag
    obs.PutBits(task.m_did, 3);             // dependency_id
    obs.PutBits(task.m_qid, 4);             // quality_id
    obs.PutBits(task.m_tid, 3);             // temporal_id
    obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
    obs.PutBits(1, 1);                      // discardable_flag
    obs.PutBits(1, 1);                      // output_flag
    obs.PutBits(0x3, 2);                    // reserved_three_2bits

    OutputBitstream obs1(begin + obs.GetNumBits() / 8, end, emulationControl);
    if (nalRefIdc && nalUnitType == 14)
    {
        mfxU32 additional_prefix_nal_unit_extension_flag = 0;

        obs1.PutBit(task.m_storeRefBasePicFlag);
        if ((useRefBasePicFlag || task.m_storeRefBasePicFlag) && !idrFlag)
            WriteDecRefPicMarking(obs1, task.m_decRefPicMrk[fieldId], idrFlag);

        obs1.PutBit(additional_prefix_nal_unit_extension_flag);
        assert(additional_prefix_nal_unit_extension_flag == 0);
        obs1.PutTrailingBits();
    }

    return begin + obs.GetNumBits() / 8 + obs1.GetNumBits() / 8;
}

namespace
{
    ENCODE_PACKEDHEADER_DATA MakePackedByteBuffer(mfxU8 * data, mfxU32 size, mfxU32 skipEmulCount)
    {
        ENCODE_PACKEDHEADER_DATA desc = { 0 };
        desc.pData                  = data;
        desc.BufferSize             = size;
        desc.DataLength             = size;
        desc.SkipEmulationByteCount = skipEmulCount;
        return desc;
    }

    void PrepareSpsPpsHeaders(
        MfxVideoParam const &               par,
        std::vector<mfxExtSpsHeader> &      sps,
        std::vector<mfxExtSpsSvcHeader> &   subset,
        std::vector<mfxExtPpsHeader> &      pps)
    {
        mfxExtSpsHeader const *  extSps = GetExtBuffer(par);
        mfxExtPpsHeader const *  extPps = GetExtBuffer(par);
        mfxExtSVCSeqDesc const * extSvc = GetExtBuffer(par);

        mfxU16 numViews             = extSps->profileIdc == MFX_PROFILE_AVC_STEREO_HIGH ? 2 : 1;
        mfxU32 numDep               = par.calcParam.numDependencyLayer;
        mfxU32 firstDid             = par.calcParam.did[0];
        mfxU32 lastDid              = par.calcParam.did[numDep - 1];
        mfxU32 numQualityAtLastDep  = extSvc->DependencyLayer[lastDid].QualityNum;

        mfxU16 heightMul = 2 - extSps->frameMbsOnlyFlag;

        // prepare sps for base layer
        sps[0] = *extSps;
        if (IsSvcProfile(par.mfx.CodecProfile)) // force SPS id to 0 for SVC profile only. For other profiles should be able to encode custom SPS id
            sps[0].seqParameterSetId         = 0;
        sps[0].picWidthInMbsMinus1       = extSvc->DependencyLayer[firstDid].Width / 16 - 1;
        sps[0].picHeightInMapUnitsMinus1 = extSvc->DependencyLayer[firstDid].Height / 16 / heightMul - 1;

        if (numViews > 1)
        {
            // MVC requires SPS and number of Subset SPS.
            // HeaderPacker prepares 2 SPS NAL units:
            // one for base view (sps_id = 0, profile_idc = 100)
            // and another for all other views (sps_id = 1, profile_idc = 128).
            // Second SPS will be re-packed to SubsetSPS after return from driver.
            for (mfxU16 view = 0; view < numViews; view++)
            {
                sps[view] = *extSps;
                pps[view] = *extPps;

                if (numViews > 1 && view == 0) // MVC base view
                    sps[view].profileIdc = MFX_PROFILE_AVC_HIGH;

                sps[view].seqParameterSetId = mfxU8(!!view) & 0x1f;
                pps[view].picParameterSetId = mfxU8(!!view);
                pps[view].seqParameterSetId = sps[view].seqParameterSetId;
            }
            return;
        }

        // prepare sps for enhanced spatial layers
        for (mfxU32 i = 0, spsidx = 1; i < numDep; i++)
        {
            mfxU32 did = par.calcParam.did[i];
            if (i == 0 && extSvc->DependencyLayer[did].QualityNum == 1)
                continue; // don't need a subset sps for did = 0

            sps[spsidx] = *extSps;
            sps[spsidx].nalUnitType               = 15;
            sps[spsidx].profileIdc                = mfxU8(par.mfx.CodecProfile & MASK_PROFILE_IDC);
            sps[spsidx].seqParameterSetId         = mfxU8(i);
            sps[spsidx].picWidthInMbsMinus1       = extSvc->DependencyLayer[did].Width / 16 - 1;
            sps[spsidx].picHeightInMapUnitsMinus1 = extSvc->DependencyLayer[did].Height / 16 / heightMul - 1;

            InitExtBufHeader(subset[spsidx - 1]);
            subset[spsidx - 1].interLayerDeblockingFilterControlPresentFlag = 1;
            subset[spsidx - 1].extendedSpatialScalabilityIdc                = 1;
            subset[spsidx - 1].chromaPhaseXPlus1Flag                        = 0;
            subset[spsidx - 1].chromaPhaseYPlus1                            = 0;
            subset[spsidx - 1].seqRefLayerChromaPhaseXPlus1Flag             = 0;
            subset[spsidx - 1].seqRefLayerChromaPhaseYPlus1                 = 0;
            subset[spsidx - 1].seqScaledRefLayerLeftOffset                  = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[0];
            subset[spsidx - 1].seqScaledRefLayerTopOffset                   = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[1];
            subset[spsidx - 1].seqScaledRefLayerRightOffset                 = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[2];
            subset[spsidx - 1].seqScaledRefLayerBottomOffset                = extSvc->DependencyLayer[did].ScaledRefLayerOffsets[3];
            subset[spsidx - 1].sliceHeaderRestrictionFlag                   = 0;
            subset[spsidx - 1].seqTcoeffLevelPredictionFlag                 = mfxU8(extSvc->DependencyLayer[did].QualityLayer[0].TcoeffPredictionFlag);
            subset[spsidx - 1].adaptiveTcoeffLevelPredictionFlag            = 0;
            spsidx++;
        }

        // prepare pps for base and enhanced spatial layers
        for (mfxU32 i = 0; i < numDep; i++)
        {
            mfxU32 did = par.calcParam.did[i];
            mfxU32 simulcast =
                IsOff(extSvc->DependencyLayer[did].BasemodePred) &&
                IsOff(extSvc->DependencyLayer[did].MotionPred) &&
                IsOff(extSvc->DependencyLayer[did].ResidualPred) ? 1 : 0;

            pps[i] = *extPps;
            if (IsSvcProfile(par.mfx.CodecProfile)) // force SPS/PPS id to 0 for SVC profile only. For other profiles should be able to encode custom SPS/PPS
            {
                pps[i].seqParameterSetId = mfxU8(i);
                pps[i].picParameterSetId = mfxU8(i);
            }
            pps[i].constrainedIntraPredFlag = (i == numDep - 1 && numQualityAtLastDep == 1 || simulcast) ? 0 : 1;
        }

        // pack pps for enhanced quality layer of highest spatial layer if exists
        if (numQualityAtLastDep > 1)
        {
            pps.back() = *extPps;
            pps.back().seqParameterSetId        = mfxU8(numDep - 1);
            pps.back().picParameterSetId        = mfxU8(numDep);
            pps.back().constrainedIntraPredFlag = 0;
        }
    }
};

void HeaderPacker::Init(
    MfxVideoParam const & par,
    ENCODE_CAPS const &   hwCaps,
    bool                  emulPrev)
{
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(par);
    mfxExtSVCSeqDesc const *      extSvc = GetExtBuffer(par);
    mfxExtSpsHeader const *       extSps = GetExtBuffer(par);
    mfxExtCodingOption2 const *   extOpt2 = GetExtBuffer(par);

    mfxU16 numViews = extSps->profileIdc == MFX_PROFILE_AVC_STEREO_HIGH ? 2 : 1;
    mfxU32 numDep   = par.calcParam.numDependencyLayer;
    mfxU32 firstDid = par.calcParam.did[0];
    mfxU32 lastDid  = par.calcParam.did[numDep - 1];

    mfxU32 numQualityAtFirstDep = extSvc->DependencyLayer[firstDid].QualityNum;
    mfxU32 numQualityAtLastDep  = extSvc->DependencyLayer[lastDid].QualityNum;

    mfxU32 additionalSpsForFirstSpatialLayer = (numQualityAtFirstDep > 1);
    mfxU32 additionalPpsForLastSpatialLayer  = (numQualityAtLastDep > 1);

    mfxU32 numSpsHeaders = IPP_MAX(numDep + additionalSpsForFirstSpatialLayer, numViews);
    mfxU32 numPpsHeaders = IPP_MAX(numDep + additionalPpsForLastSpatialLayer,  numViews);

    mfxU32 maxNumSlices = GetMaxNumSlices(par);

    m_sps.resize(numSpsHeaders);
    m_pps.resize(numPpsHeaders);
    m_subset.resize(numSpsHeaders / numViews - 1);
    m_packedSps.resize(numSpsHeaders);
    m_packedPps.resize(numPpsHeaders);
    m_packedSlices.resize(maxNumSlices);
    m_headerBuffer.resize(SPSPPS_BUFFER_SIZE);
    m_sliceBuffer.resize(SLICE_BUFFER_SIZE);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_subset);
    Zero(m_packedAud);
    Zero(m_packedSps);
    Zero(m_packedPps);
    Zero(m_packedSlices);
    Zero(m_spsIdx);
    Zero(m_ppsIdx);
    Zero(m_refDqId);
    Zero(m_simulcast);

    m_emulPrev = emulPrev;
    m_isMVC = numViews > 1;

    m_numMbPerSlice = extOpt2->NumMbPerSlice;

    PrepareSpsPpsHeaders(par, m_sps, m_subset, m_pps);

    // prepare data for slice level
    m_needPrefixNalUnit       = IsSvcProfile(par.mfx.CodecProfile) || (par.calcParam.numTemporalLayer > 0);
    m_cabacInitIdc            = extDdi->CabacInitIdcPlus1 - 1;
    m_directSpatialMvPredFlag = extDdi->DirectSpatialMvPredFlag;
    for (mfxU32 i = 0; i < numDep; i++)
    {
        mfxU32 did     = par.calcParam.did[i];
        mfxU32 prevDid = i > 0 ? par.calcParam.did[i - 1] : 0;

        m_simulcast[did] =
            IsOff(extSvc->DependencyLayer[did].BasemodePred) &&
            IsOff(extSvc->DependencyLayer[did].MotionPred) &&
            IsOff(extSvc->DependencyLayer[did].ResidualPred) ? 1 : 0;
        m_refDqId[did] = did > 0
            ? mfxU8(16 * prevDid + extSvc->DependencyLayer[prevDid].QualityNum - 1)
            : 0;

        for (mfxU32 qid = 0; qid < extSvc->DependencyLayer[did].QualityNum; qid++)
        {
            m_spsIdx[did][qid] = mfxU8(did == 0 ? (!!qid) : (did + additionalSpsForFirstSpatialLayer));
            m_ppsIdx[did][qid] = mfxU8(did);
        }
    }
    if (additionalPpsForLastSpatialLayer)
        m_ppsIdx[lastDid][numQualityAtLastDep - 1]++;


    // pack headers
    OutputBitstream obs(Begin(m_headerBuffer), End(m_headerBuffer), m_emulPrev);

    ENCODE_PACKEDHEADER_DATA * bufDesc  = Begin(m_packedSps);
    mfxU8 *                    bufBegin = Begin(m_headerBuffer);
    mfxU32                     numBits  = 0;

    // pack scalability sei
    Zero(m_packedScalabilitySei);
    if (IsSvcProfile(par.mfx.CodecProfile))
    {
        mfxU32 numBits = PutScalableInfoSeiMessage(obs, par);
        assert(numBits % 8 == 0);
        m_packedScalabilitySei = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }

    // pack sps for base and enhanced spatial layers with did > 0
    for (size_t i = 0; i < m_sps.size(); i++)
    {
        numBits = (i == 0 || (numViews > 1))
            ? WriteSpsHeader(obs, m_sps[i])
            : WriteSpsHeader(obs, m_sps[i], m_subset[i - 1].Header);
        *bufDesc++ = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }

    // pack pps for base and enhanced spatial layers
    bufDesc = Begin(m_packedPps);
    for (size_t i = 0; i < m_pps.size(); i++)
    {
        numBits = WritePpsHeader(obs, m_pps[i]);
        *bufDesc++ = MakePackedByteBuffer(bufBegin, numBits / 8, m_emulPrev ? 0 : 4);
        bufBegin += numBits / 8;
    }

    m_hwCaps = hwCaps;
}

void HeaderPacker::ResizeSlices(mfxU32 num)
{
    m_packedSlices.resize(num);
    Zero(m_packedSlices);
}

ENCODE_PACKEDHEADER_DATA const & HeaderPacker::PackAud(
    DdiTask const & task,
    mfxU32          fieldId)
{
    mfxU8 * audBegin = m_packedPps.back().pData + m_packedPps.back().DataLength;

    OutputBitstream obs(audBegin, End(m_headerBuffer), m_emulPrev);
    mfxU32 numBits = WriteAud(obs, task.m_type[fieldId]);
    m_packedAud = MakePackedByteBuffer(audBegin, numBits / 8, m_emulPrev ? 0 : 4);

    return m_packedAud;
}

std::vector<ENCODE_PACKEDHEADER_DATA> const & HeaderPacker::PackSlices(
    DdiTask const & task,
    mfxU32          fieldId)
{
    if (task.m_SliceInfo.size())
    {
        mfxU32 numSlices = (mfxU32)task.m_SliceInfo.size();
        m_numMbPerSlice = 0;
        m_packedSlices.resize(numSlices);
        if (m_sliceBuffer.size() < (size_t) (numSlices*50))
        {
            m_sliceBuffer.resize(numSlices*50);
        }
        Zero(m_sliceBuffer);
    }
    else if (task.m_numSlice[fieldId])
    {
        m_packedSlices.resize(task.m_numSlice[fieldId]);
    }

    Zero(m_packedSlices);

    mfxU8 * sliceBufferBegin = Begin(m_sliceBuffer);
    mfxU8 * sliceBufferEnd   = End(m_sliceBuffer);

    for (mfxU32 i = 0; i < m_packedSlices.size(); i++)
    {
        mfxU8 * endOfPrefix = m_needPrefixNalUnit && task.m_did == 0 && task.m_qid == 0
            ? PackPrefixNalUnitSvc(sliceBufferBegin, sliceBufferEnd, true, task, fieldId)
            : sliceBufferBegin;

        OutputBitstream obs(endOfPrefix, sliceBufferEnd, false); // pack without emulation control

        if (task.m_SliceInfo.size())
            WriteSlice(obs, task, fieldId, task.m_SliceInfo[i].startMB, task.m_SliceInfo[i].numMB);
        else
            WriteSlice(obs, task, fieldId, i);


        m_packedSlices[i].pData                  = sliceBufferBegin;
        m_packedSlices[i].DataLength             = mfxU32((endOfPrefix - sliceBufferBegin) * 8 + obs.GetNumBits()); // for slices length is in bits
        m_packedSlices[i].BufferSize             = (m_packedSlices[i].DataLength + 7) / 8;
        m_packedSlices[i].SkipEmulationByteCount = mfxU32(endOfPrefix - sliceBufferBegin + 3);

        sliceBufferBegin += m_packedSlices[i].BufferSize;
    }

    if (task.m_AUStartsFromSlice[fieldId])
        m_packedSlices[0].SkipEmulationByteCount = 4;

    return m_packedSlices;
}

void WritePredWeightTable(
    OutputBitstream &   obs,
    ENCODE_CAPS const & hwCaps,
    DdiTask const &     task,
    mfxU32              fieldId,
    mfxU32              chromaArrayType)
{
    const mfxExtPredWeightTable* pPWT = GetExtBuffer(task.m_ctrl, fieldId);
    mfxU32 nRef[2] = {
        IPP_MAX(1, task.m_list0[fieldId].Size()),
        IPP_MAX(1, task.m_list1[fieldId].Size())
    };
    mfxU32 maxWeights[2] = { hwCaps.MaxNum_WeightedPredL0, hwCaps.MaxNum_WeightedPredL1 };
    bool present;

    if (!pPWT)
        pPWT = &task.m_pwt[fieldId];

    obs.PutUe(pPWT->LumaLog2WeightDenom);

    if (chromaArrayType != 0)
        obs.PutUe(pPWT->ChromaLog2WeightDenom);

    for (mfxU32 lx = 0; lx <= (mfxU32)!!(task.m_type[fieldId] & MFX_FRAMETYPE_B); lx++)
    {
        for (mfxU32 i = 0; i < nRef[lx]; i++)
        {
            present = !!pPWT->LumaWeightFlag[lx][i] && hwCaps.LumaWeightedPred;

            if (i < maxWeights[lx])
            {
                obs.PutBit(present);

                if (present)
                {
                    obs.PutSe(pPWT->Weights[lx][i][0][0]);
                    obs.PutSe(pPWT->Weights[lx][i][0][1]);
                }
            }
            else
            {
                obs.PutBit(0);
            }

            if (chromaArrayType != 0)
            {
                present = !!pPWT->ChromaWeightFlag[lx][i] && hwCaps.ChromaWeightedPred;

                if (i < maxWeights[lx])
                {
                    obs.PutBit(present);

                    if (present)
                    {
                        for (mfxU32 j = 1; j < 3; j++)
                        {
                            obs.PutSe(pPWT->Weights[lx][i][j][0]);
                            obs.PutSe(pPWT->Weights[lx][i][j][1]);
                        }
                    }
                }
                else
                {
                    obs.PutBit(0);
                }
            }
        }
    }
}

mfxU32 HeaderPacker::WriteSlice(
    OutputBitstream & obs,
    DdiTask const &   task,
    mfxU32            fieldId,
    mfxU32            sliceId)
{
    mfxU32 sliceType    = ConvertMfxFrameType2SliceType(task.m_type[fieldId]) % 5;
    mfxU32 refPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_REF);
    mfxU32 idrPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 nalRefIdc    = task.m_nalRefIdc[fieldId];
    mfxU32 nalUnitType  = (task.m_did == 0 && task.m_qid == 0) ? (idrPicFlag ? 5 : 1) : 20;
    mfxU32 fieldPicFlag = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps = task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    // if frame_mbs_only_flag = 0 and current task implies encoding of progressive frame
    // then picture height in MBs isn't equal to PicHeightInMapUnits. Multiplier required
    mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (fieldPicFlag == 0) ? 2 : 1;
    mfxU32 picHeightInMBs      = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;

    SliceDivider divider = MakeSliceDivider(
        m_hwCaps.SliceStructure,
        m_numMbPerSlice,
        (mfxU32)m_packedSlices.size(),
        sps.picWidthInMbsMinus1 + 1,
        picHeightInMBs);

    mfxU32 firstMbInSlice = 0;
    for (mfxU32 i = 0; i <= sliceId; i++, divider.Next())
        firstMbInSlice = divider.GetFirstMbInSlice();

    mfxU32 sliceHeaderRestrictionFlag = 0;
    mfxU32 sliceSkipFlag = 0;

    mfxU8 startcode[4] = { 0, 0, 0, 1};
    mfxU8 * pStartCode = startcode;
#if !defined(ANDROID)
    if (task.m_AUStartsFromSlice[fieldId] == false || sliceId > 0)
        pStartCode ++;
#endif
    obs.PutRawBytes(pStartCode, startcode + sizeof startcode);
    obs.PutBit(0);
    obs.PutBits(nalRefIdc, 2);
    obs.PutBits(nalUnitType, 5);

    mfxU32 noInterLayerPredFlag = (task.m_qid == 0) ? m_simulcast[task.m_did] : 0;
    if (nalUnitType == 20)
    {
        mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;
        obs.PutBits(1, 1);          // svc_extension_flag
        obs.PutBits(idrPicFlag, 1);
        obs.PutBits(task.m_pid, 6);
        obs.PutBits(noInterLayerPredFlag, 1);
        obs.PutBits(task.m_did, 3);
        obs.PutBits(task.m_qid, 4);
        obs.PutBits(task.m_tid, 3);
        obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
        obs.PutBits(1, 1);          // discardable_flag
        obs.PutBits(1, 1);          // output_flag
        obs.PutBits(0x3, 2);        // reserved_three_2bits
    }

    obs.PutUe(firstMbInSlice);
    obs.PutUe(sliceType + 5);
    obs.PutUe(pps.picParameterSetId);
    obs.PutBits(task.m_frameNum, sps.log2MaxFrameNumMinus4 + 4);
    if (!sps.frameMbsOnlyFlag)
    {
        obs.PutBit(fieldPicFlag);
        if (fieldPicFlag)
            obs.PutBit(fieldId);
    }
    if (idrPicFlag)
        obs.PutUe(task.m_idrPicId);
    if (sps.picOrderCntType == 0)
    {
        obs.PutBits(task.GetPoc(fieldId), sps.log2MaxPicOrderCntLsbMinus4 + 4);
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt_bottom
    }
    if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag)
    {
        obs.PutSe(0); // delta_pic_order_cnt[0]
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt[1]
    }
    if (task.m_qid == 0)
    {
        if (sliceType == SLICE_TYPE_B)
            obs.PutBit(IsOn(m_directSpatialMvPredFlag));
        if (sliceType != SLICE_TYPE_I)
        {
            mfxU32 numRefIdxL0ActiveMinus1 = IPP_MAX(1, task.m_list0[fieldId].Size()) - 1;
            mfxU32 numRefIdxL1ActiveMinus1 = IPP_MAX(1, task.m_list1[fieldId].Size()) - 1;
            mfxU32 numRefIdxActiveOverrideFlag =
                (numRefIdxL0ActiveMinus1 != pps.numRefIdxL0DefaultActiveMinus1) ||
                (numRefIdxL1ActiveMinus1 != pps.numRefIdxL1DefaultActiveMinus1 && sliceType == SLICE_TYPE_B);

            obs.PutBit(numRefIdxActiveOverrideFlag);
            if (numRefIdxActiveOverrideFlag)
            {
                obs.PutUe(numRefIdxL0ActiveMinus1);
                if (sliceType == SLICE_TYPE_B)
                    obs.PutUe(numRefIdxL1ActiveMinus1);
            }
        }
        if (sliceType != SLICE_TYPE_I)
            WriteRefPicListModification(obs, task.m_refPicList0Mod[fieldId]);
        if (sliceType == SLICE_TYPE_B)
            WriteRefPicListModification(obs, task.m_refPicList1Mod[fieldId]);
        if (pps.weightedPredFlag  == 1 && sliceType == SLICE_TYPE_P ||
            pps.weightedBipredIdc == 1 && sliceType == SLICE_TYPE_B)
        {
            mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;
            WritePredWeightTable(obs, m_hwCaps, task, fieldId, chromaArrayType);
        }
        if (refPicFlag || task.m_nalRefIdc[fieldId])
        {
            WriteDecRefPicMarking(obs, task.m_decRefPicMrk[fieldId], idrPicFlag);
            mfxU32 storeRefBasePicFlag = 0;
            if (nalUnitType == 20 && !sliceHeaderRestrictionFlag)
                obs.PutBit(storeRefBasePicFlag);
        }
    }
    if (pps.entropyCodingModeFlag && sliceType != SLICE_TYPE_I)
        obs.PutUe(m_cabacInitIdc);
    obs.PutSe(task.m_cqpValue[fieldId] - (pps.picInitQpMinus26 + 26));
    if (pps.deblockingFilterControlPresentFlag)
    {
        mfxU32 disableDeblockingFilterIdc = task.m_disableDeblockingIdc;
        mfxI32 sliceAlphaC0OffsetDiv2     = 0;
        mfxI32 sliceBetaOffsetDiv2        = 0;

        obs.PutUe(disableDeblockingFilterIdc);
        if (disableDeblockingFilterIdc != 1)
        {
            obs.PutSe(sliceAlphaC0OffsetDiv2);
            obs.PutSe(sliceBetaOffsetDiv2);
        }
    }
    if (nalUnitType == 20)
    {
        mfxExtSpsSvcHeader const & subset = m_subset[m_spsIdx[task.m_did][task.m_qid] - 1];

        if (!noInterLayerPredFlag && task.m_qid == 0)
        {
            mfxU32 interLayerDeblockingFilterIdc    = (task.m_did || task.m_qid) ? 0 : 1;
            mfxU32 interLayerSliceAlphaC0OffsetDiv2 = 0;
            mfxU32 interLayerSliceBetaOffsetDiv2    = 0;
            mfxU32 constrainedIntraResamplingFlag   = 0;

            obs.PutUe(m_refDqId[task.m_did]);

            if (subset.interLayerDeblockingFilterControlPresentFlag)
            {
                obs.PutUe(interLayerDeblockingFilterIdc);
                if (interLayerDeblockingFilterIdc != 1)
                {
                    obs.PutSe(interLayerSliceAlphaC0OffsetDiv2);
                    obs.PutSe(interLayerSliceBetaOffsetDiv2);
                }
            }
            obs.PutBit(constrainedIntraResamplingFlag);
            if (subset.extendedSpatialScalabilityIdc == 2)
            {
                if (sps.chromaFormatIdc > 0)
                {
                    obs.PutBit(subset.seqRefLayerChromaPhaseXPlus1Flag);
                    obs.PutBits(subset.seqRefLayerChromaPhaseYPlus1, 2);
                }
                obs.PutSe(subset.seqScaledRefLayerLeftOffset);
                obs.PutSe(subset.seqScaledRefLayerTopOffset);
                obs.PutSe(subset.seqScaledRefLayerRightOffset);
                obs.PutSe(subset.seqScaledRefLayerBottomOffset);
            }
        }

        if (!noInterLayerPredFlag)
        {
            mfxU32 sliceSkipFlag                     = 0;
            mfxU32 adaptiveBaseModeFlag              = !m_simulcast[task.m_did];
            mfxU32 defaultBaseModeFlag               = 0;
            mfxU32 adaptiveMotionPredictionFlag      = !m_simulcast[task.m_did];
            mfxU32 defaultMotionPredictionFlag       = 0;
            mfxU32 adaptiveResidualPredictionFlag    = !m_simulcast[task.m_did];
            mfxU32 defaultResidualPredictionFlag     = 0;
            mfxU32 adaptiveTcoeffLevelPredictionFlag = 0;

            obs.PutBit(sliceSkipFlag);
            if (sliceSkipFlag)
                obs.PutUe(divider.GetNumMbInSlice() - 1);
            else
            {
                obs.PutBit(adaptiveBaseModeFlag);
                if (!adaptiveBaseModeFlag)
                    obs.PutBit(defaultBaseModeFlag);
                if (!defaultBaseModeFlag)
                {
                    obs.PutBit(adaptiveMotionPredictionFlag);
                    if (!adaptiveMotionPredictionFlag)
                        obs.PutBit(defaultMotionPredictionFlag);
                }
                obs.PutBit(adaptiveResidualPredictionFlag);
                if (!adaptiveResidualPredictionFlag)
                    obs.PutBit(defaultResidualPredictionFlag);
            }
            if (adaptiveTcoeffLevelPredictionFlag)
                obs.PutBit(subset.seqTcoeffLevelPredictionFlag);
        }

        if (!sliceHeaderRestrictionFlag && !sliceSkipFlag)
        {
            mfxU32 scanIdxStart = 0;
            mfxU32 scanIdxEnd = 15;
            obs.PutBits(scanIdxStart, 4);
            obs.PutBits(scanIdxEnd, 4);
        }
    }

    return obs.GetNumBits();
}// FIXME: add svc extension here

mfxU32 HeaderPacker::WriteSlice(
    OutputBitstream & obs,
    DdiTask const &   task,
    mfxU32            fieldId,
    mfxU32            firstMbInSlice,
    mfxU32            numMbInSlice)
{
    mfxU32 sliceType    = ConvertMfxFrameType2SliceType(task.m_type[fieldId]) % 5;
    mfxU32 refPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_REF);
    mfxU32 idrPicFlag   = !!(task.m_type[fieldId] & MFX_FRAMETYPE_IDR);
    mfxU32 nalRefIdc    = task.m_nalRefIdc[fieldId];
    mfxU32 nalUnitType  = (task.m_did == 0 && task.m_qid == 0) ? (idrPicFlag ? 5 : 1) : 20;
    mfxU32 fieldPicFlag = task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE;

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps = task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    // if frame_mbs_only_flag = 0 and current task implies encoding of progressive frame
    // then picture height in MBs isn't equal to PicHeightInMapUnits. Multiplier required
    //mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (fieldPicFlag == 0) ? 2 : 1;
    //mfxU32 picHeightInMBs      = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;

    mfxU32 sliceHeaderRestrictionFlag = 0;
    mfxU32 sliceSkipFlag = 0;
#if defined(ANDROID)
    mfxU8 startcode[4] = { 0, 0, 0, 1 };
#else
    mfxU8 startcode[3] = { 0, 0, 1 };
#endif
    obs.PutRawBytes(startcode, startcode + sizeof startcode);
    obs.PutBit(0);
    obs.PutBits(nalRefIdc, 2);
    obs.PutBits(nalUnitType, 5);

    mfxU32 noInterLayerPredFlag = (task.m_qid == 0) ? m_simulcast[task.m_did] : 0;
    if (nalUnitType == 20)
    {
        mfxU32 useRefBasePicFlag = (task.m_type[fieldId] & MFX_FRAMETYPE_KEYPIC) ? 1 : 0;
        obs.PutBits(1, 1);          // svc_extension_flag
        obs.PutBits(idrPicFlag, 1);
        obs.PutBits(task.m_pid, 6);
        obs.PutBits(noInterLayerPredFlag, 1);
        obs.PutBits(task.m_did, 3);
        obs.PutBits(task.m_qid, 4);
        obs.PutBits(task.m_tid, 3);
        obs.PutBits(useRefBasePicFlag, 1);      // use_ref_base_pic_flag
        obs.PutBits(1, 1);          // discardable_flag
        obs.PutBits(1, 1);          // output_flag
        obs.PutBits(0x3, 2);        // reserved_three_2bits
    }

    obs.PutUe(firstMbInSlice);
    obs.PutUe(sliceType + 5);
    obs.PutUe(pps.picParameterSetId);
    obs.PutBits(task.m_frameNum, sps.log2MaxFrameNumMinus4 + 4);
    if (!sps.frameMbsOnlyFlag)
    {
        obs.PutBit(fieldPicFlag);
        if (fieldPicFlag)
            obs.PutBit(fieldId);
    }
    if (idrPicFlag)
        obs.PutUe(task.m_idrPicId);
    if (sps.picOrderCntType == 0)
    {
        obs.PutBits(task.GetPoc(fieldId), sps.log2MaxPicOrderCntLsbMinus4 + 4);
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt_bottom
    }
    if (sps.picOrderCntType == 1 && !sps.deltaPicOrderAlwaysZeroFlag)
    {
        obs.PutSe(0); // delta_pic_order_cnt[0]
        if (pps.bottomFieldPicOrderInframePresentFlag && !fieldPicFlag)
            obs.PutSe(0); // delta_pic_order_cnt[1]
    }
    if (task.m_qid == 0)
    {
        if (sliceType == SLICE_TYPE_B)
            obs.PutBit(IsOn(m_directSpatialMvPredFlag));
        if (sliceType != SLICE_TYPE_I)
        {
            mfxU32 numRefIdxL0ActiveMinus1 = IPP_MAX(1, task.m_list0[fieldId].Size()) - 1;
            mfxU32 numRefIdxL1ActiveMinus1 = IPP_MAX(1, task.m_list1[fieldId].Size()) - 1;
            mfxU32 numRefIdxActiveOverrideFlag =
                (numRefIdxL0ActiveMinus1 != pps.numRefIdxL0DefaultActiveMinus1) ||
                (numRefIdxL1ActiveMinus1 != pps.numRefIdxL1DefaultActiveMinus1 && sliceType == SLICE_TYPE_B);

            obs.PutBit(numRefIdxActiveOverrideFlag);
            if (numRefIdxActiveOverrideFlag)
            {
                obs.PutUe(numRefIdxL0ActiveMinus1);
                if (sliceType == SLICE_TYPE_B)
                    obs.PutUe(numRefIdxL1ActiveMinus1);
            }
        }
        if (sliceType != SLICE_TYPE_I)
            WriteRefPicListModification(obs, task.m_refPicList0Mod[fieldId]);
        if (sliceType == SLICE_TYPE_B)
            WriteRefPicListModification(obs, task.m_refPicList1Mod[fieldId]);
        if (pps.weightedPredFlag  == 1 && sliceType == SLICE_TYPE_P ||
            pps.weightedBipredIdc == 1 && sliceType == SLICE_TYPE_B)
        {
            mfxU32 chromaArrayType = sps.separateColourPlaneFlag ? 0 : sps.chromaFormatIdc;
            WritePredWeightTable(obs, m_hwCaps, task, fieldId, chromaArrayType);
        }
        if (refPicFlag)
        {
            WriteDecRefPicMarking(obs, task.m_decRefPicMrk[fieldId], idrPicFlag);
            mfxU32 storeRefBasePicFlag = 0;
            if (nalUnitType == 20 && !sliceHeaderRestrictionFlag)
                obs.PutBit(storeRefBasePicFlag);
        }
    }
    if (pps.entropyCodingModeFlag && sliceType != SLICE_TYPE_I)
        obs.PutUe(m_cabacInitIdc);
    obs.PutSe(task.m_cqpValue[fieldId] - (pps.picInitQpMinus26 + 26));
    if (pps.deblockingFilterControlPresentFlag)
    {
        mfxU32 disableDeblockingFilterIdc = task.m_disableDeblockingIdc;
        mfxI32 sliceAlphaC0OffsetDiv2     = 0;
        mfxI32 sliceBetaOffsetDiv2        = 0;

        obs.PutUe(disableDeblockingFilterIdc);
        if (disableDeblockingFilterIdc != 1)
        {
            obs.PutSe(sliceAlphaC0OffsetDiv2);
            obs.PutSe(sliceBetaOffsetDiv2);
        }
    }
    if (nalUnitType == 20)
    {
        mfxExtSpsSvcHeader const & subset = m_subset[m_spsIdx[task.m_did][task.m_qid] - 1];

        if (!noInterLayerPredFlag && task.m_qid == 0)
        {
            mfxU32 interLayerDeblockingFilterIdc    = (task.m_did || task.m_qid) ? 0 : 1;
            mfxU32 interLayerSliceAlphaC0OffsetDiv2 = 0;
            mfxU32 interLayerSliceBetaOffsetDiv2    = 0;
            mfxU32 constrainedIntraResamplingFlag   = 0;

            obs.PutUe(m_refDqId[task.m_did]);

            if (subset.interLayerDeblockingFilterControlPresentFlag)
            {
                obs.PutUe(interLayerDeblockingFilterIdc);
                if (interLayerDeblockingFilterIdc != 1)
                {
                    obs.PutSe(interLayerSliceAlphaC0OffsetDiv2);
                    obs.PutSe(interLayerSliceBetaOffsetDiv2);
                }
            }
            obs.PutBit(constrainedIntraResamplingFlag);
            if (subset.extendedSpatialScalabilityIdc == 2)
            {
                if (sps.chromaFormatIdc > 0)
                {
                    obs.PutBit(subset.seqRefLayerChromaPhaseXPlus1Flag);
                    obs.PutBits(subset.seqRefLayerChromaPhaseYPlus1, 2);
                }
                obs.PutSe(subset.seqScaledRefLayerLeftOffset);
                obs.PutSe(subset.seqScaledRefLayerTopOffset);
                obs.PutSe(subset.seqScaledRefLayerRightOffset);
                obs.PutSe(subset.seqScaledRefLayerBottomOffset);
            }
        }

        if (!noInterLayerPredFlag)
        {
            mfxU32 sliceSkipFlag                     = 0;
            mfxU32 adaptiveBaseModeFlag              = !m_simulcast[task.m_did];
            mfxU32 defaultBaseModeFlag               = 0;
            mfxU32 adaptiveMotionPredictionFlag      = !m_simulcast[task.m_did];
            mfxU32 defaultMotionPredictionFlag       = 0;
            mfxU32 adaptiveResidualPredictionFlag    = !m_simulcast[task.m_did];
            mfxU32 defaultResidualPredictionFlag     = 0;
            mfxU32 adaptiveTcoeffLevelPredictionFlag = 0;

            obs.PutBit(sliceSkipFlag);
            if (sliceSkipFlag)
                obs.PutUe(numMbInSlice - 1);
            else
            {
                obs.PutBit(adaptiveBaseModeFlag);
                if (!adaptiveBaseModeFlag)
                    obs.PutBit(defaultBaseModeFlag);
                if (!defaultBaseModeFlag)
                {
                    obs.PutBit(adaptiveMotionPredictionFlag);
                    if (!adaptiveMotionPredictionFlag)
                        obs.PutBit(defaultMotionPredictionFlag);
                }
                obs.PutBit(adaptiveResidualPredictionFlag);
                if (!adaptiveResidualPredictionFlag)
                    obs.PutBit(defaultResidualPredictionFlag);
            }
            if (adaptiveTcoeffLevelPredictionFlag)
                obs.PutBit(subset.seqTcoeffLevelPredictionFlag);
        }

        if (!sliceHeaderRestrictionFlag && !sliceSkipFlag)
        {
            mfxU32 scanIdxStart = 0;
            mfxU32 scanIdxEnd = 15;
            obs.PutBits(scanIdxStart, 4);
            obs.PutBits(scanIdxEnd, 4);
        }
    }

    return obs.GetNumBits();
}


const mfxU8 M_N[2][3][3][2] = { // [isB][ctxIdx][cabac_init_idc][m, n]
    {// m and n values for 3 CABAC contexts 11-13 (mb_skip_flag for P frames)
        {{23, 33}, {22, 25}, {29, 16}},
        {{23,  2}, {34,  0}, {25,  0}},
        {{21,  0}, {16,  0}, {14,  0}}
    },
    {// m and n values for 3 CABAC contexts 24-26 (mb_skip_flag for B frames)
        {{18, 64}, {26, 34}, {20, 40}},
        {{ 9, 43}, {19, 22}, {20, 10}},
        {{29,  0}, {40,  0}, {29,  0}}
    },
};

const mfxI8 M_N_mbt_b[9][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 27-35 (mb_type for B frames)
    {{ 26,  67}, { 57,   2}, { 54,   0}},
    {{ 16,  90}, { 41,  36}, { 37,  42}},
    {{  9, 104}, { 26,  69}, { 12,  97}},
    {{-46, 127}, {-45, 127}, {-32, 127}},
    {{-20, 104}, {-15, 101}, {-22, 117}},
    {{  1,  67}, { -4,  76}, { -2,  74}},
    {{-13,  78}, { -6,  71}, { -4,  85}},
    {{-11,  65}, {-13,  79}, {-24, 102}},
    {{  1,  62}, {  5,  52}, {  5,  57}}
};


const mfxI8 M_N_mvd_b[14][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 40-53 (mvd_lx for B frames)
    {{ -3,  69}, { -2,  69}, {-11,  89}},
    {{ -6,  81}, { -5,  82}, {-15, 103}},
    {{-11,  96}, {-10,  96}, {-21, 116}},
    {{  6,  55}, {  2,  59}, { 19,  57}},
    {{  7,  67}, {  2,  75}, { 20,  58}},
    {{ -5,  86}, { -3,  87}, {  4,  84}},
    {{  2,  88}, { -3, 100}, {  6,  96}},
    {{  0,  58}, {  1,  56}, {  1,  63}},
    {{ -3,  76}, { -3,  74}, { -5,  85}},
    {{-10,  94}, { -6,  85}, {-13, 106}},
    {{  5,  54}, {  0,  59}, {  5,  63}},
    {{  4,  69}, { -3,  81}, {  6,  75}},
    {{ -3,  81}, { -7,  86}, { -3,  90}},
    {{  0,  88}, { -5,  95}, { -1, 101}}
};

const mfxI8 M_N_ref_b[6][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 54-59 (ref_idx_lx for B frames)
    {{ -7,  67}, { -1,  66}, {  3,  55}},
    {{ -5,  74}, { -1,  77}, { -4,  79}},
    {{ -4,  74}, {  1,  70}, { -2,  75}},
    {{ -5,  80}, { -2,  86}, {-12,  97}},
    {{ -7,  72}, { -5,  72}, { -7,  50}},
    {{  1,  58}, {  0,  61}, {  1,  60}},
};

const mfxI8 M_N_cbp_b[12][3][2] = { // [ctxIdx][cabac_init_idc][m, n]
    // m and n values for CABAC contexts 73-84 (coded_block_pattern for B frames)
    {{-27, 126}, {-39, 127}, {-36, 127}},
    {{-28,  98}, {-18,  91}, {-17,  91}},
    {{-25, 101}, {-17,  96}, {-14,  95}},
    {{-23,  67}, {-26,  81}, {-25,  84}},
    {{-28,  82}, {-35,  98}, {-25,  86}},
    {{-20,  94}, {-24, 102}, {-12,  89}},
    {{-16,  83}, {-23,  97}, {-17,  91}},
    {{-22, 110}, {-27, 119}, {-31, 127}},
    {{-21,  91}, {-24,  99}, {-14,  76}},
    {{-18, 102}, {-21, 110}, {-18, 103}},
    {{-13,  93}, {-18, 102}, {-13,  90}},
    {{-29, 127}, {-36, 127}, {-37, 127}}
};

#define Clip3(min, max, val) val > max ? max : val < min ? min : val

inline void InitCabacContext(mfxI16 m, mfxI16 n, mfxU16 sliceQP, mfxU8& ctx)
{
    mfxI16 preCtxState = Clip3( 1, 126, ( ( m * sliceQP ) >> 4 ) + n);
    if (preCtxState <= 63)
        ctx = mfxU8(63 - preCtxState); // MPS = 0
    else
        ctx = mfxU8((preCtxState - 64) | (1 << 6)); // MPS = 1
}

#define INIT_CABAC_CONTEXTS(idc, QP, MN, ctx) \
    for (mfxU32 i = 0; i < sizeof(ctx); i++) \
        InitCabacContext(MN[i][idc][0], MN[i][idc][1], mfxU16(QP), ctx[i]);


ENCODE_PACKEDHEADER_DATA const & HeaderPacker::PackSkippedSlice(
            DdiTask const & task,
            mfxU32          fieldId)
{
    Zero(m_packedSlices);

    mfxU8 * sliceBufferBegin = Begin(m_sliceBuffer);
    mfxU8 * sliceBufferEnd   = End(m_sliceBuffer);

    mfxU8 * endOfPrefix = m_needPrefixNalUnit && task.m_did == 0 && task.m_qid == 0
            ? PackPrefixNalUnitSvc(sliceBufferBegin, sliceBufferEnd, true, task, fieldId)
            : sliceBufferBegin;

    CabacPackerSimple packer(endOfPrefix, sliceBufferEnd, m_emulPrev);
    WriteSlice(packer, task, fieldId, 0);

    mfxExtSpsHeader const & sps = task.m_viewIdx ? m_sps[task.m_viewIdx] : m_sps[m_spsIdx[task.m_did][task.m_qid]];
    mfxExtPpsHeader const & pps = task.m_viewIdx ? m_pps[task.m_viewIdx] : m_pps[m_ppsIdx[task.m_did][task.m_qid]];

    mfxU32 picHeightMultiplier = (sps.frameMbsOnlyFlag == 0) && (task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE) ? 2 : 1;
    mfxU32 picHeightInMB       = (sps.picHeightInMapUnitsMinus1 + 1) * picHeightMultiplier;
    mfxU32 picWidthInMB        = (sps.picWidthInMbsMinus1 + 1);
    mfxU32 picSizeInMB         = picWidthInMB * picHeightInMB;

    if (pps.entropyCodingModeFlag)
    {
        mfxU32 numAlignmentBits = (8 - (packer.GetNumBits() & 0x7)) & 0x7;
        packer.PutBits(0xff, numAlignmentBits);

        // encode dummy MB data

        mfxU8 cabacContexts[3]; // 3 CABAC contexts for mb_skip_flag
        mfxU32 sliceQP = task.m_cqpValue[fieldId];

        INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N[!!(task.m_type[fieldId] & MFX_FRAMETYPE_B)], cabacContexts);

        mfxU8 ctx276 = 63; // ctx for end_of_slice_flag: MPS = 0, pStateIdx = 63 (non-adapting prob state)

        if ((task.m_type[fieldId] & MFX_FRAMETYPE_B) && (task.m_type[fieldId] & MFX_FRAMETYPE_REF))
        {
            mfxU8 ctxMBT[9];
            mfxU8 ctxREF[6];
            mfxU8 ctxMVD[14];
            mfxU8 ctxCBP[12];

            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_mbt_b, ctxMBT);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_ref_b, ctxREF);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_mvd_b, ctxMVD);
            INIT_CABAC_CONTEXTS(m_cabacInitIdc, sliceQP, M_N_cbp_b, ctxCBP);

            for (mfxU32 uMB = 0; uMB < (picSizeInMB-1); uMB ++)
            {
                bool mbA = !!(uMB % picWidthInMB); //is left MB available
                bool mbB = (uMB >= picWidthInMB);   //is above MB available

                packer.EncodeBin(&cabacContexts[mbA + mbB], 0); // encode mb_skip_flag = 0

                // mb_type = 1 (B_L0_16x16) 1 0 0
                packer.EncodeBin(&ctxMBT[mbA + mbB], 1);
                packer.EncodeBin(&ctxMBT[3], 0);
                packer.EncodeBin(&ctxMBT[5], 0);

                if (task.m_list0[fieldId].Size() > 1)
                    packer.EncodeBin(&ctxREF[0], 0); // ref_idx_l0 = 0

                packer.EncodeBin(&ctxMVD[0], 0); // mvd_l0[][][0] = 0
                packer.EncodeBin(&ctxMVD[7], 0); // mvd_l0[][][1] = 0

                // coded_block_pattern = 0 (0 0 0 0, 0)
                packer.EncodeBin(&ctxCBP[mbA + 2 * mbB], 0);
                packer.EncodeBin(&ctxCBP[  1 + 2 * mbB], 0);
                packer.EncodeBin(&ctxCBP[mbA + 2 * 1  ], 0);
                packer.EncodeBin(&ctxCBP[  1 + 2 * 1  ], 0);
                packer.EncodeBin(&ctxCBP[4], 0);

                packer.EncodeBin(&ctx276, 0); // end_of_slice_flag = 0
            }

            packer.EncodeBin(&cabacContexts[2], 0); // encode mb_skip_flag = 0

            // mb_type = 1 (B_L0_16x16) 1 0 0
            packer.EncodeBin(&ctxMBT[2], 1);
            packer.EncodeBin(&ctxMBT[3], 0);
            packer.EncodeBin(&ctxMBT[5], 0);

            if (task.m_list0[fieldId].Size() > 1)
                packer.EncodeBin(&ctxREF[0], 0); // ref_idx_l0 = 0

            packer.EncodeBin(&ctxMVD[0], 0); // mvd_l0[][][0] = 0
            packer.EncodeBin(&ctxMVD[7], 0); // mvd_l0[][][1] = 0

            // coded_block_pattern = 0 (0 0 0 0, 0)
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[3], 0);
            packer.EncodeBin(&ctxCBP[4], 0);

        }
        else
        {
            for (mfxU32 uMB = 0; uMB < (picSizeInMB-1); uMB ++)
            {
                packer.EncodeBin(&cabacContexts[0], 1); // encode mb_skip_flag = 1 for every MB.
                packer.EncodeBin(&ctx276, 0); // encode end_of_slice_flag = 0 for every MB
            }

            packer.EncodeBin(&cabacContexts[0], 1); // encode mb_skip_flag = 1 for every MB.
        }

        packer.TerminateEncode();
    }
    else
    {
        if ((task.m_type[fieldId] & MFX_FRAMETYPE_B) && (task.m_type[fieldId] & MFX_FRAMETYPE_REF))
        {
            for (mfxU32 uMB = 0; uMB < picSizeInMB; uMB ++)
            {
                packer.PutUe(0); // mb_skip_run = 0
                packer.PutUe(1); // mb_type = 1 (B_L0_16x16)

                if (task.m_list0[fieldId].Size() > 1)
                    packer.PutBit(1);// ref_idx_l0 = 0

                packer.PutSe(0); // mvd_l0[][][0] = 0
                packer.PutSe(0); // mvd_l0[][][1] = 0

                packer.PutUe(0); // coded_block_pattern = 0
            }
        }
        else
        {
            packer.PutUe(picSizeInMB); // write mb_skip_run = picSizeInMBs
        }

        packer.PutTrailingBits();

        VM_ASSERT(packer.GetNumBits() % 8 == 0);
    }

    m_packedSlices[0].pData                  = sliceBufferBegin;
    m_packedSlices[0].DataLength             = mfxU32((endOfPrefix - sliceBufferBegin) + (packer.GetNumBits() / 8));
    m_packedSlices[0].BufferSize             = m_packedSlices[0].DataLength;
    m_packedSlices[0].SkipEmulationByteCount = m_emulPrev ? 0 : (mfxU32(endOfPrefix - sliceBufferBegin) + 3);

    return m_packedSlices[0];
}

#endif //MFX_ENABLE_H264_VIDEO_..._HW

