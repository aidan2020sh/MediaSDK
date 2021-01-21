// Copyright (c) 2013-2020 Intel Corporation
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

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <math.h>
#include <algorithm>
#include "mfx_h265_brc.h"
#include "mfx_h265_tables.h"

#ifdef _MSVC_LANG
#pragma warning(disable : 4244)
#pragma warning(disable : 4267)
#pragma warning(disable : 4305)
#endif

namespace {

    Ipp64f const QSTEP[88] = {
         0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
         2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
        12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
        57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070,
        256.000, 287.350, 322.540, 362.039, 406.375, 456.140, 512.000, 574.701, 645.080, 724.077, 812.749, 912.280,
        1024.000, 1149.401, 1290.159, 1448.155, 1625.499, 1824.561, 2048.000, 2298.802, 2580.318, 2896.309, 3250.997, 3649.121,
        4096.000, 4597.605, 5160.637, 5792.619, 6501.995, 7298.242, 8192.000, 9195.209, 10321.273, 11585.238, 13003.989, 14596.485
    };

    Ipp8u QStep2QpFloor(Ipp64f qstep, Ipp8u qpoffset = 0) // QSTEP[qp] <= qstep, return 0<=qp<=51+mQuantOffset
    {
        Ipp8u qp = Ipp8u(std::upper_bound(QSTEP, QSTEP + std::min(52 + qpoffset, 88), qstep) - QSTEP);
        return qp > 0 ? qp - 1 : 0;
    }

    Ipp8u Qstep2QP(Ipp64f qstep, Ipp8u qpoffset = 0) // return 0<=qp<=51+mQuantOffset
    {
        Ipp8u qp = QStep2QpFloor(qstep, qpoffset);
        return (qp == 51 + qpoffset || qstep < (QSTEP[qp] + QSTEP[qp + 1]) / 2) ? qp : qp + 1;
    }

    Ipp64f QP2Qstep(Ipp32u qp, Ipp32u qpoffset = 0)
    {
        return QSTEP[IPP_MIN(51 + qpoffset, qp)];
    }
}

namespace H265Enc {

//#define PRINT_BRC_STATS

#ifdef PRINT_BRC_STATS

static int brc_fprintf(char *fmt, ...) 
{
   va_list args;
   va_start(args, fmt);
   int r = vfprintf(stderr, fmt, args);
   va_end(args);
   return r;
}

#else
#define brc_fprintf
#endif

#define BRC_MAX_LOAN_LENGTH 75
#define BRC_LOAN_RATIO 0.075

#define BRC_QSTEP_SCALE_EXPONENT 0.7

#define BRC_RATE_CMPLX_EXPONENT 0.5

mfxStatus H265BRC::Close()
{
    mfxStatus status = MFX_ERR_NONE;
    if (!mIsInit)
        return MFX_ERR_NOT_INITIALIZED;
    mIsInit = false;
    return status;
}

mfxStatus H265BRC::InitHRD()
{
    mfxU64 bufSizeBits = mParams.HRDBufferSizeBytes << 3;
    mfxU64 maxBitrate = mParams.maxBitrate;

    if (MFX_RATECONTROL_VBR != mRCMode)
        maxBitrate = mBitrate;

    if (maxBitrate < (mfxU64)mBitrate)
        maxBitrate = mBitrate;

    mParams.maxBitrate = (Ipp32s)((maxBitrate >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE));
    mHRD.maxBitrate = mParams.maxBitrate;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    if (mHRD.maxBitrate < mBitrate)
        mBitrate = mHRD.maxBitrate;

    Ipp32s bitsPerFrame = (Ipp32s)(mBitrate / mFramerate);

    if (bufSizeBits > 0 && bufSizeBits < (mfxU64)(bitsPerFrame << 1))
        bufSizeBits = (bitsPerFrame << 1);

    mHRD.bufSize = (Ipp32u)((bufSizeBits >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE));
    mParams.HRDBufferSizeBytes = (Ipp32s)(mHRD.bufSize >> 3);

    if (mParams.HRDInitialDelayBytes <= 0)
        mParams.HRDInitialDelayBytes = (MFX_RATECONTROL_CBR == mRCMode ? mParams.HRDBufferSizeBytes/2 : mParams.HRDBufferSizeBytes);
    else if (mParams.HRDInitialDelayBytes < bitsPerFrame >> 3)
        mParams.HRDInitialDelayBytes = bitsPerFrame >> 3;
    if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
        mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;
    mHRD.bufFullness = mParams.HRDInitialDelayBytes << 3;

    mHRD.underflowQuant = 0;
    mHRD.overflowQuant = 999;
    mHRD.frameNum = 0;

    return MFX_ERR_NONE;
}

Ipp32s H265BRC::GetInitQP()
{
  Ipp32s fs, fsLuma;

  fsLuma = mParams.width * mParams.height;
  fs = fsLuma;
  if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
    fs += fsLuma / 2;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
    fs += fsLuma;
  else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
    fs += fsLuma * 2;
  fs = fs * mParams.bitDepthLuma / 8;
  
  Ipp64f qstep = pow(1.5 * fs * mFramerate / mBitrate, 0.8);
  Ipp32s q = Qstep2QP(qstep, mQuantOffset) + mQuantOffset;

  BRC_CLIP(q, 1, mQuantMax);

  return q;
}

mfxStatus H265BRC::SetParams(const mfxVideoParam *params, H265VideoParam &video)
{
    if (!params)
        return MFX_ERR_NULL_PTR;


    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mParams.BRCMode = params->mfx.RateControlMethod;
    mParams.targetBitrate =  params->mfx.TargetKbps * brcParamMultiplier * 1000;
    if (!mParams.targetBitrate)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mParams.BRCMode != MFX_RATECONTROL_AVBR) {
        mParams.HRDBufferSizeBytes = params->mfx.BufferSizeInKB * brcParamMultiplier * 1000;
        mParams.HRDInitialDelayBytes = params->mfx.InitialDelayInKB * brcParamMultiplier * 1000;
        if (mParams.HRDInitialDelayBytes > mParams.HRDBufferSizeBytes)
            mParams.HRDInitialDelayBytes = mParams.HRDBufferSizeBytes;
    } else {
        mParams.HRDBufferSizeBytes = mParams.HRDInitialDelayBytes = 0;
    }

    mParams.maxBitrate = params->mfx.MaxKbps * brcParamMultiplier * 1000;

    mParams.frameRateExtN = video.FrameRateExtN;
    mParams.frameRateExtD = video.FrameRateExtD;
    if (!mParams.frameRateExtN || !mParams.frameRateExtD)
        return MFX_ERR_INVALID_VIDEO_PARAM;


    mParams.width = params->mfx.FrameInfo.Width;
    mParams.height = params->mfx.FrameInfo.Height;
    if (params->mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
        mParams.height >>= 1;
    mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;
    mParams.bitDepthLuma =video.bitDepthLuma;

    return MFX_ERR_NONE;
}

mfxStatus H265BRC::Init(const mfxVideoParam *params,  H265VideoParam &video, Ipp32s enableRecode)
{
    mfxStatus status = MFX_ERR_NONE;

    if (!params)
        return MFX_ERR_NULL_PTR;

    status = SetParams(params, video);
    if (status != MFX_ERR_NONE)
        return status;

    mLowres = video.LowresFactor ? 1 : 0;
    mRecode = (enableRecode && (mParams.BRCMode != MFX_RATECONTROL_AVBR)) ? 1 : 0;

    mFramerate = (Ipp64f)mParams.frameRateExtN / mParams.frameRateExtD;
    mBitrate = mParams.targetBitrate;
    mRCMode = (mfxU16)mParams.BRCMode;

    if (mBitrate <= 0 || mFramerate <= 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        status = InitHRD();
        if (status != MFX_ERR_NONE)
            return status;
        mMaxBitrate = mParams.maxBitrate >> 3;
        mBF = (mfxI64)mParams.HRDInitialDelayBytes * mParams.frameRateExtN;
        mBFsaved = mBF;

        mPredBufFulness = mHRD.bufFullness;
        mRealPredFullness = mHRD.bufFullness;
    }

    mBitsDesiredFrame = (Ipp32s)(mBitrate / mFramerate);
    if (mBitsDesiredFrame < 10)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    mQuantOffset = 6 * (mParams.bitDepthLuma - 8);

    mQuantMax = 51 + mQuantOffset;
    mQuantMin = 1;
    mMinQp = mQuantMin;


    mBitsDesiredTotal = 0;
    mBitsEncodedTotal = 0;

    mQuantUpdated = 1;
    mRecodedFrame_encOrder = -1;

    if (video.AnalyzeCmplx) {
        if (video.MaxFrameSizeInBits) {
            if (params->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) {
                mMaxFrameSizeInBitsI = mMaxFrameSizeInBits = video.MaxFrameSizeInBits;
            }
            else {
                mMaxFrameSizeInBitsI = mMaxFrameSizeInBits = video.MaxFrameSizeInBits/2;
            }
        } 
#ifdef LOW_COMPLX_PAQ
        else if ((video.DeltaQpMode&AMT_DQP_PAQ) && video.FullresMetrics == 1 
            //&& mParams.bitDepthLuma == 8 // now 10bit
            && params->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE && mRCMode != MFX_RATECONTROL_AVBR) {
            // Dynamic Safe Init/Scene change
            mMaxFrameSizeInBitsI = (BRC_MAX_LOAN_LENGTH * BRC_LOAN_RATIO * 2.0f + 1.5f) * mBitsDesiredFrame;
        }
#endif
        InitMinQForMaxFrameSize(&video);

        mTotalDeviation = 0;

        mNumLayers = video.PGopPicSize > 1 ?  2 : video.BiPyramidLayers + 1;
        if (video.GopRefDist > 1 && video.BiPyramidLayers == 1)
            mNumLayers = 3; // in case of BiPyramid is off there are 3 layers for BRC: I, P, and other Bs

        if (mRCMode == MFX_RATECONTROL_AVBR) {
            Zero(mEstCoeff);
            Zero(mEstCnt);
        }
        mTotAvComplx = 0;
        mTotComplxCnt = 0;
        mTotPrevCmplx = 0;
        mComplxSum = 0;
        mTotMeanComplx = 0;
        mTotTarget = mBitsDesiredFrame;
        mLoanLength = 0;
        mLoanRatio = 0;
        mLoanBitsPerFrame = 0;

        //mLoanLengthP = 0;
        //mLoanBitsPerFrameP = 0;

        mQstepBase = -1;
        
        if (mRCMode != MFX_RATECONTROL_AVBR) {
            Zero(mPrevCmplxLayer);
            Zero(mPrevQstepLayer);
            Zero(mPrevBitsLayer);
        }
        Zero(mPrevQpLayer);
        Zero(mPrevQpLayerSet);

        mPrevBitsIP = -1;
        mPrevCmplxIP = -1;
        mPrevQstepIP = -1;
        mPrevQpIP = -1;

        mEncOrderCoded = -1;

        Ipp32s fs;
        fs = mParams.width * mParams.height;
        if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
            fs += fs / 2;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
            fs += fs;
        else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
            fs += fs * 2;
        mfs = fs;

        mCmplxRate = 20.*pow((Ipp64f)fs, 0.8); // adjustment for subpel
        if (mQuantOffset)
            mCmplxRate *= (1 << mQuantOffset / 6) * 0.9;

        mQstepScale0 = mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);// * (1 << mQuantOffset / 6);

        Ipp64f sum = 1;
        Ipp64f layerRatio = 0.3; // tmp ???
        Ipp64f layerShare = layerRatio;
        for (Ipp32s i = 2; i < mNumLayers; i++) {
            sum += layerShare;
            layerShare *= 2*layerRatio;
        }
        Zero(mLayerTarget);
        Ipp64f targetMiniGop = mBitsDesiredFrame * video.GopRefDist;
        mLayerTarget[1] = targetMiniGop / sum;
        mLayerTarget[0] = mLayerTarget[1] * 3;

        brc_fprintf("layer Targets: %d %d ", (int)mLayerTarget[0], (int)mLayerTarget[1]);
        for (Ipp32s i = 2; i < mNumLayers; i++) {
            mLayerTarget[i] = mLayerTarget[i-1]*layerRatio;
            brc_fprintf(" %d", (int)mLayerTarget[i]);
        }
        brc_fprintf("\n");

        if (mRCMode == MFX_RATECONTROL_AVBR) {
            for (Ipp32s i = 0; i < mNumLayers; i++) {
                mEstCoeff[i] = mLayerTarget[i] * 10.;
                mEstCnt[i] = 1.;
            }
        }

    } else {

        Ipp32s q = GetInitQP();

        if (!mRecode) {
            if (q - 6 > 10)
                mQuantMin = IPP_MAX(10, q - 10);
            else
                mQuantMin = IPP_MAX(q - 6, 2);

            if (q < mQuantMin)
                q = mQuantMin;
        }

        mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = q;

        mRCbap = 100;
        mRCqap = 100;
        mRCfap = 100;

        mRCq = q;
        mRCqa = mRCqa0 = 1. / (Ipp64f)mRCq;
        mRCfa = mBitsDesiredFrame;
        mRCfa_short = mBitsDesiredFrame;

        mSChPoc = 0;
        mSceneChange = 0;
        mBitsEncodedPrev = mBitsDesiredFrame;

        mMaxQp = 999;
        mMinQp = -1;
    }

    mDynamicInit = 0;  // AMT_LTR

    mPicType = MFX_FRAMETYPE_I;
    mSceneNum = 0;
    mMiniGopFrames.clear();
    mIsInit = true;

    return status;
}


#define RESET_BRC_CORE \
{ \
    if (sizeNotChanged) { \
        mRCq = (Ipp32s)(1./mRCqa * pow(mRCfa/mBitsDesiredFrame, 0.32) + 0.5); \
        BRC_CLIP(mRCq, 1, mQuantMax); \
    } else \
        mRCq = GetInitQP(); \
    mQuantPrev = mQuantI = mQuantP = mQuantB = mQPprev = mRCq; \
    mRCqa = mRCqa0 = 1./mRCq; \
    mRCfa = mBitsDesiredFrame; \
    mRCfa_short = mBitsDesiredFrame; \
}


mfxStatus H265BRC::Reset(mfxVideoParam *params, H265VideoParam &video, Ipp32s enableRecode)
{
    mfxStatus status;

    mfxU16 brcParamMultiplier = params->mfx.BRCParamMultiplier ? params->mfx.BRCParamMultiplier : 1;
    mfxU16 rcmode_new = params->mfx.RateControlMethod;
    Ipp32s bufSize_new = rcmode_new == MFX_RATECONTROL_AVBR ? 0 : (params->mfx.BufferSizeInKB * brcParamMultiplier * 8000 >> (4 + MFX_H265_CPBSIZE_SCALE)) << (4 + MFX_H265_CPBSIZE_SCALE);
    bool sizeNotChanged = (mParams.width == params->mfx.FrameInfo.Width &&
                           mParams.height == params->mfx.FrameInfo.Height &&
                           mParams.chromaFormat == params->mfx.FrameInfo.ChromaFormat);

    Ipp32s bufSize, maxBitrate;
    Ipp64f bufFullness;
    mfxU16 rcmode = mRCMode;
    Ipp32s targetBitrate = mBitrate;

    if (rcmode_new != rcmode) {
        if (MFX_RATECONTROL_CBR != rcmode_new && MFX_RATECONTROL_AVBR != rcmode)
            rcmode = rcmode_new;
        else
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    mRecode = enableRecode ? 1 : 0;

    if (!(mParams.HRDBufferSizeBytes | bufSize_new)) { // no HRD
        status = SetParams(params, video);
        if (status != MFX_ERR_NONE)
            return status;
        mBitrate = mParams.targetBitrate;
        mFramerate = (Ipp64f)mParams.frameRateExtN / mParams.frameRateExtD;
        if (mBitrate <= 0 || mFramerate <= 0)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        mBitsDesiredFrame = (Ipp32s)(mBitrate / mFramerate);
        RESET_BRC_CORE
        return MFX_ERR_NONE;
    } else if (mParams.HRDBufferSizeBytes == 0 && bufSize_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    Ipp32s maxBitrate_new = (params->mfx.MaxKbps * brcParamMultiplier * 1000 >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);
    Ipp32s targetBitrate_new = params->mfx.TargetKbps * brcParamMultiplier * 1000;
    Ipp32s targetBitrate_new_r = (targetBitrate_new >> (6 + MFX_H265_BITRATE_SCALE)) << (6 + MFX_H265_BITRATE_SCALE);

    // framerate change not allowed in case of HRD
    if (mParams.frameRateExtN * video.FrameRateExtD != mParams.frameRateExtD * video.FrameRateExtN)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    bufSize = mHRD.bufSize;
    maxBitrate = (Ipp32s)mHRD.maxBitrate;
    bufFullness = mHRD.bufFullness;

    if (targetBitrate_new_r > maxBitrate_new && maxBitrate_new > 0)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else if (MFX_RATECONTROL_CBR == rcmode && targetBitrate_new > 0 && maxBitrate_new > 0 && maxBitrate_new != targetBitrate_new_r)
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (MFX_RATECONTROL_CBR == rcmode) {
        if (targetBitrate_new > 0 && maxBitrate_new <= 0)
            maxBitrate_new = targetBitrate_new_r;
        else if (targetBitrate_new <= 0 && maxBitrate_new > 0)
            targetBitrate_new = maxBitrate_new;
    }

    if (targetBitrate_new > 0)
        targetBitrate = targetBitrate_new;

    if (bufSize_new > bufSize)
        bufSize = bufSize_new;

    if (maxBitrate_new > 0 && maxBitrate_new < maxBitrate) {
        bufFullness = mHRD.bufFullness * maxBitrate_new / maxBitrate;
        mBF = (mfxI64)((mfxU64)mBF * (maxBitrate_new >> 6) / (mMaxBitrate >> 3));
        maxBitrate = maxBitrate_new;
    } else if (maxBitrate_new > maxBitrate) {
        if (MFX_RATECONTROL_VBR == rcmode) {
            Ipp64f bf_delta = (maxBitrate_new - maxBitrate) / mFramerate;
          // lower estimate for the fullness with the bitrate updated at tai;
          // for VBR the fullness encoded in buffering period SEI can be below the real buffer fullness
            bufFullness += bf_delta;
            if (bufFullness > (Ipp64f)(bufSize - 1))
                bufFullness = (Ipp64f)(bufSize - 1);

            mBF += (mfxI64)((maxBitrate_new >> 3) - mMaxBitrate) * (mfxI64)mParams.frameRateExtD;
            if (mBF > (mfxI64)(bufSize >> 3) * mParams.frameRateExtN)
                mBF = (mfxI64)(bufSize >> 3) * mParams.frameRateExtN;

            maxBitrate = maxBitrate_new;
        } else // HRD overflow is possible for CBR
            return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    if (bufSize_new > 0 && bufSize_new < bufSize) {
        bufSize = bufSize_new;
        if (bufFullness > (Ipp64f)(bufSize - 1)) {
            if (MFX_RATECONTROL_VBR == rcmode)
                bufFullness = (Ipp64f)(bufSize - 1);
            else
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
    }

    if (targetBitrate > maxBitrate)
        targetBitrate = maxBitrate;

    mHRD.bufSize = bufSize;
    mHRD.maxBitrate = (Ipp64f)maxBitrate;
    mHRD.bufFullness = bufFullness;
    mHRD.inputBitsPerFrame = mHRD.maxBitrate / mFramerate;

    mParams.BRCMode = rcmode;
    mParams.HRDBufferSizeBytes = bufSize >> 3;
    mParams.maxBitrate = maxBitrate;
    mParams.targetBitrate = targetBitrate;
    if (!sizeNotChanged) {
        mParams.width = params->mfx.FrameInfo.Width;
        mParams.height = params->mfx.FrameInfo.Height;
        mParams.chromaFormat = params->mfx.FrameInfo.ChromaFormat;
    }
    mMaxBitrate = (Ipp32u)(maxBitrate >> 3);

    RESET_BRC_CORE

    mSceneChange = 0;
    mBitsEncodedPrev = mBitsDesiredFrame;

    mMaxQp = 999;
    mMinQp = -1;
    if (video.MaxFrameSizeInBits) {
        if (params->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE) {
            mMaxFrameSizeInBitsI = mMaxFrameSizeInBits = video.MaxFrameSizeInBits;
        }
        else {
            mMaxFrameSizeInBitsI = mMaxFrameSizeInBits = video.MaxFrameSizeInBits / 2;
        }
    }
#ifdef LOW_COMPLX_PAQ
    else if ((video.DeltaQpMode&AMT_DQP_PAQ) && video.FullresMetrics == 1
        //&& mParams.bitDepthLuma == 8 // now 10bit
        && params->mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE && mRCMode != MFX_RATECONTROL_AVBR) {
        // Dynamic Safe Init/Scene change
        mMaxFrameSizeInBitsI = (BRC_MAX_LOAN_LENGTH * BRC_LOAN_RATIO * 2.0f + 1.5f) * mBitsDesiredFrame;
    }
#endif
    InitMinQForMaxFrameSize(&video);
    return MFX_ERR_NONE;
}

#define BRC_QSTEP_COMPL_EXPONENT 0.4
#define BRC_QSTEP_COMPL_EXPONENT_AVBR 0.2
#define BRC_CMPLX_DECAY 0.1
#define BRC_CMPLX_DECAY_AVBR 0.2
#define BRC_MIN_CMPLX 0.01
#define BRC_MIN_CMPLX_LAYER 0.05
#define BRC_MIN_CMPLX_LAYER_I 0.01

static const Ipp64f brc_qstep_factor[8] = {pow(2., -1./6.), 1., pow(2., 1./6.),   pow(2., 2./6.), pow(2., 3./6.), pow(2., 4./6.), pow(2., 5./6.), 2.};
static const Ipp64f minQstep = QSTEP[1];
static const Ipp64f maxQstepChange = pow(2, 0.5);
static const Ipp64f predCoef = 1000000. / (1920 * 1080);

Ipp32s H265BRC::PredictFrameSize(H265VideoParam *video, Frame *frames[], Ipp32s qp, mfxBRC_FrameData *refFrData)
{
    Ipp32s i;
    Ipp64f cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;
    Ipp32s layer = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[0]->m_pyramidLayer)));
    Ipp32s predBitsLayer = -1, predBits = -1;
    Ipp64f q = QP2Qstep(qp, mQuantOffset);
    Ipp32s refQp = -1, refEncOrder = -1, refDispOrder = -1;
    Ipp64f refQstep = 0;

    if (frames[0]->m_picCodeType == MFX_FRAMETYPE_I) 
    {
        Ipp64f w;
        Ipp64f predBits0 = cmplx * predCoef * mParams.width * mParams.height / q;

        if (mQuantOffset)
            predBits0 *= (1 << mQuantOffset / 6) * 0.9;

        if (mPrevCmplxLayer[0] > 0) {
            w = fabs(cmplx / mPrevCmplxLayer[0] - 1) * 0.1;
            if (w > 1)
                w = 1;
            if (mPrevCmplxLayer[0] > BRC_MIN_CMPLX_LAYER_I)
                predBits = cmplx / mPrevCmplxLayer[0] * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];
            else
                predBits = cmplx / BRC_MIN_CMPLX_LAYER_I * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];

            Ipp64f predBits1 = predBits;
            predBits = (Ipp32s)(w*predBits0  + (1 - w)*predBits);
            if (mPrevCmplxLayer[0] > 0.1 && cmplx > mPrevCmplxLayer[0]) {
                predBits  = IPP_MAX(predBits1, predBits);
            }

        } else // can never happen
            predBits = predBits0;

        brc_fprintf("%f %f %f %f %d | %d \n", cmplx , mPrevCmplxLayer[0] , mPrevQstepLayer[0] , q , mPrevBitsLayer[0], predBits);

        refFrData->encOrder = -1;
        refFrData->qp = -1; // ???
        refFrData->cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;

    }
    else if (frames[0]->m_picCodeType == MFX_FRAMETYPE_P) 
    {
        Frame *refFrame = frames[0]->m_refPicList[0].m_refFrames[0];
        refEncOrder = refFrame->m_encOrder;
        refDispOrder = refFrame->m_frameOrder;

        Ipp32s size = mMiniGopFrames.size();
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer (rare case)
            for (i = 0; i < size; i++) {
                if (mMiniGopFrames[i].layer <= 1 && mMiniGopFrames[i].encOrder < frames[0]->m_encOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder > refDispOrder) {
                if (mMiniGopFrames[i].cmplx > cmplx)
                    cmplx = mMiniGopFrames[i].cmplx;
            }
        }

        refFrData->encOrder = refEncOrder;
        refFrData->qp = refQp;
        refFrData->cmplx = cmplx;

        if (mPrevCmplxLayer[layer] > BRC_MIN_CMPLX_LAYER)
            predBitsLayer = cmplx / mPrevCmplxLayer[layer] * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];

        if (mPrevCmplxLayer[layer - 1] > BRC_MIN_CMPLX)
            predBits = cmplx / mPrevCmplxLayer[layer-1] * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];

        brc_fprintf(" -- %d %d -- ", qp, refQp);

        if (qp < refQp) {
            Ipp64f intraCmplx = frames[0]->m_stats[mLowres]->m_avgIntraSatd;

            if (video->GopRefDist <= 2) {
                Ipp64f w = ((Ipp64f)refQp - qp) / 12;
                if (w > 0.5)
                    w = 0.5;
                intraCmplx = w*intraCmplx + (1 - w)*cmplx;
            }

            if (mPrevCmplxIP >= 0 && frames[0]->m_sceneOrder == mSceneNum) {
                if (mPrevCmplxIP > BRC_MIN_CMPLX) {
                    predBits = intraCmplx / mPrevCmplxIP * mPrevQstepIP / q * mPrevBitsIP;
                    if (predBits > predBitsLayer)
                        predBitsLayer = predBits;
                }
            } else if (mPrevCmplxLayer[0] > BRC_MIN_CMPLX) {
                predBits = intraCmplx / mPrevCmplxLayer[0] * mPrevQstepLayer[0] / q * mPrevBitsLayer[0];
                if (predBits > predBitsLayer)
                    predBitsLayer = predBits;
            }
        }
    }
    else
    {

        Frame *refFrame0 = frames[0]->m_refPicList[0].m_refFrames[0];
        Frame *refFrame1 = frames[0]->m_refPicList[1].m_refFrames[0];

        refEncOrder = refFrame0->m_encOrder;
        refDispOrder = refFrame0->m_frameOrder;


        Ipp32s size = mMiniGopFrames.size();
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer
            for (i = 0; i < size; i++) {
                if (mMiniGopFrames[i].layer < layer && mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder < frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder > refDispOrder) {
                if (mMiniGopFrames[i].cmplx > cmplx)
                    cmplx = mMiniGopFrames[i].cmplx;
            }
        }

        Ipp64f pastCmpx = cmplx;
        Ipp64f pastQstep = refQstep;
        Ipp32s pastQp = refQp;
        Ipp32s pastEncOrder = refEncOrder;
        
        refQp = -1;
        refEncOrder = refFrame1->m_encOrder;
        refDispOrder = refFrame1->m_frameOrder;


        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].encOrder == refEncOrder) {
                refQstep = mMiniGopFrames[i].qstep;
                refQp = mMiniGopFrames[i].qp;
                break;
            }
        }

        if (refQp < 0) { // ref frame is not in the buffer
            for (i = size - 1; i >= 0; i--) {
                if (mMiniGopFrames[i].layer < layer && mMiniGopFrames[i].dispOrder > frames[0]->m_frameOrder) {
                    refDispOrder = mMiniGopFrames[i].dispOrder;
                    refEncOrder = mMiniGopFrames[i].encOrder;
                    refQstep = mMiniGopFrames[i].qstep;
                    refQp = mMiniGopFrames[i].qp;
                }
            }
        }

        Ipp64f futCmpx = -1;
        for (i = 0; i < size; i++) {
            if (mMiniGopFrames[i].dispOrder > frames[0]->m_frameOrder && mMiniGopFrames[i].dispOrder <= refDispOrder) {
                if (mMiniGopFrames[i].cmplxInter > futCmpx)
                    futCmpx = mMiniGopFrames[i].cmplxInter;
            }
        }

        if (futCmpx < 0)
            futCmpx = 1e5;
        Ipp64f futQstep = refQstep;
        Ipp32s futQp = refQp;
        Ipp32s futEncOrder = refEncOrder;

        // need ot take into account intraSatd or at least intraShare (?)
        if (qp >= pastQp && qp >= futQp) {
            if (futCmpx < pastCmpx) {
                cmplx = futCmpx;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;

            } else {
                cmplx = pastCmpx;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            }
        } else if (qp >= pastQp && qp < futQp) {
            if (pastCmpx < futCmpx * futQstep / q)  {
                cmplx = pastCmpx;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            } else {
                cmplx = futCmpx * futQstep / q;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            }
        } else if (qp < pastQp && qp >= futQp) {
            if (futCmpx < pastCmpx * pastQstep / q)  {
                cmplx = futCmpx;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            } else {
                cmplx = pastCmpx * pastQstep / q;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            }
        } else { // qp < pastQp && qp < futQp
            if (pastCmpx <= futCmpx * sqrt(futQstep / pastQstep)) {
                cmplx = pastCmpx * pastQstep / q;
                refFrData->qp = pastQp;
                refFrData->encOrder = pastEncOrder;
            } else {
                cmplx = futCmpx * futQstep / q;
                refFrData->qp = futQp;
                refFrData->encOrder = futEncOrder;
            }
        }

        refFrData->cmplx = cmplx;

        if (mPrevCmplxLayer[layer] > BRC_MIN_CMPLX_LAYER)
            predBitsLayer = cmplx / mPrevCmplxLayer[layer] * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];

        if (mPrevCmplxLayer[layer - 1] > BRC_MIN_CMPLX)
            predBits = cmplx / mPrevCmplxLayer[layer-1] * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];

    }

    if (predBits < 0 && predBitsLayer < 0) {
        predBitsLayer = cmplx / BRC_MIN_CMPLX_LAYER * mPrevQstepLayer[layer] / q * mPrevBitsLayer[layer];
        if (layer > 0)
            predBits = cmplx / BRC_MIN_CMPLX * mPrevQstepLayer[layer-1] / q * mPrevBitsLayer[layer-1];
    }

    if (predBitsLayer >= 0 && predBits >= 0) {

        Ipp64f ratL, rat;
        if (mPrevCmplxLayer[layer] > 0) {
            ratL = cmplx / mPrevCmplxLayer[layer];
            if (ratL > 1)
                ratL = 1/ratL;
        } else
            ratL = 0;
        if (mPrevCmplxLayer[layer-1] > 0) {
            rat = cmplx / mPrevCmplxLayer[layer-1];
            if (rat > 1)
                rat = 1/rat;
        } else
            rat = 0;

        if (ratL > 0.8 * rat)
            predBits = predBitsLayer;

    } else if (predBitsLayer >= 0)
        predBits = predBitsLayer;

    return predBits;
}

void H265BRC::SetMiniGopData(Frame *frames[], Ipp32s numFrames)
{
    Ipp32s frcnt = 0, i;
    Ipp32s size;
    Ipp32s start = 0;
    Ipp32s toerase = 0;

    if (numFrames <= 0)
        return;

    if (mMiniGopFrames.size() > 0) {
        if (mMiniGopFrames.back().encOrder <= mEncOrderCoded)
            mMiniGopFrames.clear();
        else {
            for (i = (Ipp32s)mMiniGopFrames.size() - 1; i > 0; i--) {
                if (mMiniGopFrames[i].layer <= 1) {
                    if (mMiniGopFrames[i - 1].encOrder <= mEncOrderCoded) {
                        toerase = i;
                        break;
                    }
                }
            }
            if (toerase > 0) {
                for (i = toerase - 1; i >= 0; i--) {
                    if (mMiniGopFrames[i].layer <= 1) { // need to keep prev P for not yet coded B's
                        if (i > 0)
                            mMiniGopFrames.erase(mMiniGopFrames.begin(), mMiniGopFrames.begin() + i);
                        break;
                    } else
                        mMiniGopFrames.erase(mMiniGopFrames.begin() + i);
                }
            }
        }

        for (i = (Ipp32s)mMiniGopFrames.size() - 1; i >= 0; i--) {
            if (frames[0]->m_encOrder >  mMiniGopFrames[i].encOrder) {
                start = i + 1;
                break;
            }
        }
    }

    size = numFrames + start;
    mMiniGopFrames.resize(size);
   
    for (i = 0; i < numFrames; i++) {
        mMiniGopFrames[i + start].cmplx = frames[i]->m_stats[mLowres]->m_avgBestSatd;
        mMiniGopFrames[i + start].cmplxInter = frames[i]->m_stats[mLowres]->m_avgInterSatd;
        mMiniGopFrames[i + start].encOrder = frames[i]->m_encOrder;
        mMiniGopFrames[i + start].dispOrder = frames[i]->m_frameOrder;
        mMiniGopFrames[i + start].layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[i]->m_pyramidLayer)));
    }
}

void H265BRC::UpdateMiniGopData(Frame *frame, Ipp8s qp)
{
    Ipp32u i;
    for (i = 0; i < mMiniGopFrames.size(); i++) {
        if (mMiniGopFrames[i].encOrder == frame->m_encOrder) {
            mMiniGopFrames[i].qp = qp;
            mMiniGopFrames[i].qstep = QP2Qstep(qp, mQuantOffset);
            break;
        }
    }
}


#define BRC_MIN_CMPLX_FACTOR 0.1

#define BRC_CONST_MUL_B1 1.836646650
#define BRC_CONST_MUL_B2 2.151466181
#define BRC_CONST_MUL_B3 1.882509199

#define BRC_CONST_EXP_R_B1 0.464067453
#define BRC_CONST_EXP_R_B2 0.42717007
#define BRC_CONST_EXP_R_B3 0.403810703

#define BRC_CONST_EXP_CP_B1 0.435985838
#define BRC_CONST_EXP_CI_B1 0.293863142
#define BRC_CONST_EXP_CP_B2 0.349251646
#define BRC_CONST_EXP_CI_B2 0.270572445
#define BRC_CONST_EXP_CP_B3 0.260676727
#define BRC_CONST_EXP_CI_B3 0.262438217

#define BRC_CONST_MUL_I8 1.207685293
#define BRC_CONST_EXP_R_I8 0.743703955
#define BRC_CONST_EXP_C_I8 0.867665105

#define BRC_CONST_MUL_P1 2.014391968
#define BRC_CONST_EXP_R_P1 0.474328692
#define BRC_CONST_EXP_CI_P1 0.306698776
#define BRC_CONST_EXP_CP_P1 0.398015286

#define BRC_CONST_MUL_P2 1.799534763
#define BRC_CONST_EXP_R_P2 0.4912499995
#define BRC_CONST_EXP_CI_P2 0.3652363425
#define BRC_CONST_EXP_CP_P2 0.445514609

#define BRC_CONST_MUL_P3 1.584677558
#define BRC_CONST_EXP_R_P3 0.508171307
#define BRC_CONST_EXP_CI_P3 0.423773909
#define BRC_CONST_EXP_CP_P3 0.493013932

#define BRC_CONST_MUL_P8 1.321864816
#define BRC_CONST_EXP_R_P8 0.604323959
#define BRC_CONST_EXP_CI_P8 0.617129289
#define BRC_CONST_EXP_CP_P8 0.300691


void H265BRC::InitMinQForMaxFrameSize(H265VideoParam *video)
{
#ifndef AMT_MAX_FRAME_SIZE
    return;
#else
    //Init
    mMinQstepCmplxKInitEncOrder = 0;
    // Vars for Update
    for (Ipp32s l = 0; l < 5; l++) {
        mMinQstepCmplxKUpdtC[l] = 0;
        mMinQstepCmplxKUpdt[l] = 0;
        mMinQstepCmplxKUpdtErr[l] = 0.16;
    }

    mMinQstepCmplxK[0] = BRC_CONST_MUL_I8;
    mMinQstepCmplxK[2] = BRC_CONST_MUL_B1;
    mMinQstepCmplxK[3] = BRC_CONST_MUL_B2;
    mMinQstepCmplxK[4] = BRC_CONST_MUL_B3;

    // P is based on GopRefDist
    if(video->GopRefDist>4) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P8;
        mMinQstepRateEP = BRC_CONST_EXP_R_P8;
        mMinQstepPCmplxEP = BRC_CONST_EXP_CP_P8;
        mMinQstepICmplxEP = BRC_CONST_EXP_CI_P8;
    } else if(video->GopRefDist>1) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P3;
        mMinQstepRateEP = BRC_CONST_EXP_R_P3;
        mMinQstepPCmplxEP = BRC_CONST_EXP_CP_P3;
        mMinQstepICmplxEP = BRC_CONST_EXP_CI_P3;
    } else if(video->picStruct == MFX_PICSTRUCT_PROGRESSIVE) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P1;
        mMinQstepRateEP = BRC_CONST_EXP_R_P1;
        mMinQstepPCmplxEP = BRC_CONST_EXP_CP_P1;
        mMinQstepICmplxEP = BRC_CONST_EXP_CI_P1;
    } else {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P2;
        mMinQstepRateEP = BRC_CONST_EXP_R_P2;
        mMinQstepPCmplxEP = BRC_CONST_EXP_CP_P2;
        mMinQstepICmplxEP = BRC_CONST_EXP_CI_P2;
    }
#endif
}

#ifdef AMT_MAX_FRAME_SIZE
void H265BRC::ResetMinQForMaxFrameSize(H265VideoParam *video, Frame *f)
{
    //Reset
    mMinQstepCmplxKInitEncOrder = f->m_encOrder;
    
    for (Ipp32s l = 1; l < 5; l++) {
        mMinQstepCmplxKUpdtC[l] = 0;
        mMinQstepCmplxKUpdt[l] = 0;
        mMinQstepCmplxKUpdtErr[l] = 0.16;
    }

    mMinQstepCmplxK[2] = BRC_CONST_MUL_B1;
    mMinQstepCmplxK[3] = BRC_CONST_MUL_B2;
    mMinQstepCmplxK[4] = BRC_CONST_MUL_B3;

    // P is based on GopRefDist
    if(video->GopRefDist>4) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P8;
        mMinQstepRateEP = BRC_CONST_EXP_R_P8;
    } else if(video->GopRefDist>1) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P3;
        mMinQstepRateEP = BRC_CONST_EXP_R_P3;
    } else if (video->picStruct == MFX_PICSTRUCT_PROGRESSIVE) {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P1;
        mMinQstepRateEP = BRC_CONST_EXP_R_P1;
    } else {
        mMinQstepCmplxK[1] = BRC_CONST_MUL_P2;
        mMinQstepRateEP = BRC_CONST_EXP_R_P2;
    }
}

void H265BRC::UpdateMinQForMaxFrameSize(Ipp32u maxFrameSizeInBits, H265VideoParam *video, Frame *f, Ipp32s bits, Ipp32s layer, mfxBRCStatus Sts)
{
    if(!maxFrameSizeInBits) return;
    if(!bits) bits = 8;
    if(layer == 0) {

        Ipp64f fzCmplx = f->m_fzCmplx;
        Ipp64f MinQstepCmplxK = f->m_fzCmplxK;

        Ipp64f R = mfs / (Ipp64f) bits;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f rateQstep = pow(R, BRC_CONST_EXP_R_I8);
        Ipp64f cmplxExp = pow(C,  BRC_CONST_EXP_C_I8);
        Ipp64f cmplxQstep = MinQstepCmplxK * cmplxExp;
        Ipp64f QstepScaleComputed = rateQstep * cmplxQstep;

        Ipp64f QstepScaleReal = QP2Qstep(f->m_sliceQpY, mQuantOffset);
        Ipp64f dS = log(QstepScaleReal) - log(QstepScaleComputed);
        Ipp64f upDlt = IPP_MIN(0.5, IPP_MAX(0.0125, 1.3042 * pow(R,-0.922)));
        if(Sts & MFX_BRC_ERR_MAX_FRAME) upDlt  = 0.9;

        // abs envelope + iirmavg4
        mMinQstepCmplxKUpdtErr[0] = IPP_MAX((mMinQstepCmplxKUpdtErr[0] + abs(dS)/MinQstepCmplxK)/2, abs(dS)/MinQstepCmplxK);
        dS = BRC_CLIP(dS, -0.5, 1.0);

        mMinQstepCmplxK[0] = MinQstepCmplxK * (1.0 + upDlt*dS);
        mMinQstepCmplxKUpdt[0]++;
        mMinQstepCmplxKUpdtC[0] = C;
        
        // Sanity Check 
        if(Sts & MFX_BRC_ERR_MAX_FRAME) {
            Ipp32s qp_prev = f->m_sliceQpY; //Qstep2QP(QstepScaleComputed, mQuantOffset);
            if(qp_prev < 51 + mQuantOffset) {
                Ipp64f rateQstepNew = pow( (Ipp64f) mfs / (Ipp64f) maxFrameSizeInBits, BRC_CONST_EXP_R_I8);
                Ipp64f QstepScaleUpdtComputed = rateQstepNew * mMinQstepCmplxK[0] * cmplxExp;
                Ipp32s qp_now = Qstep2QP(QstepScaleUpdtComputed, mQuantOffset);
                Ipp32s off = IPP_MAX(1, ((Ipp32f)(bits - maxFrameSizeInBits)/((Ipp32f)bits*0.16) + 0.5));
                if(qp_prev + off > qp_now) {
                    qp_now = IPP_MIN(51, qp_prev + off);
                    QstepScaleUpdtComputed = QP2Qstep(qp_now);
                    mMinQstepCmplxK[0] = QstepScaleUpdtComputed / (rateQstepNew * cmplxExp);
                    mMinQstepCmplxKUpdtErr[0] = 0.16;
                }
            }
        }

    } else if(layer == 1) {

        if(f->m_encOrder < mMinQstepCmplxKInitEncOrder && bits < (Ipp32s) maxFrameSizeInBits) return;
        
        Ipp64f iCmplx = IPP_MAX(BRC_MIN_CMPLX_FACTOR, f->m_stats[mLowres]->m_avgIntraSatd);
        Ipp64f fzCmplx = f->m_fzCmplx;
        Ipp64f MinQstepCmplxK = f->m_fzCmplxK;
        Ipp64f MinQstepRateEP = f->m_fzRateE;
        
        Ipp64f R = (Ipp64f) mfs / (Ipp64f) bits;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f rateQstep = pow(R, MinQstepRateEP);
        Ipp64f cmplxExp = pow(C,  mMinQstepPCmplxEP) * pow(iCmplx, mMinQstepICmplxEP);
        Ipp64f cmplxQstep = MinQstepCmplxK * cmplxExp;
        Ipp64f QstepScaleComputed = rateQstep * cmplxQstep;
        Ipp64f QstepScaleReal = QP2Qstep(f->m_sliceQpY, mQuantOffset);
        Ipp64f dS = log(QstepScaleReal) - log(QstepScaleComputed);
        Ipp64f upDlt = IPP_MIN(0.5, IPP_MAX(0.0125, 1.3042 * pow(R,-0.922)));
        if(Sts & MFX_BRC_ERR_MAX_FRAME) upDlt = 0.5;

        // abs envelope + iirmavg4
        mMinQstepCmplxKUpdtErr[1] = IPP_MAX((mMinQstepCmplxKUpdtErr[1] + abs(dS)/MinQstepCmplxK)/2, abs(dS)/MinQstepCmplxK);
        dS = BRC_CLIP(dS, -0.5, 1.0);

        mMinQstepCmplxK[1] = MinQstepCmplxK*(1.0 + upDlt*dS);
        mMinQstepCmplxKUpdt[1]++;
        mMinQstepCmplxKUpdtC[1] = iCmplx;

        mMinQstepRateEP = IPP_MIN(1.0, IPP_MAX(0.125, MinQstepRateEP + IPP_MAX(-0.1, IPP_MIN(0.2, 0.01 * (log(QstepScaleReal) - log(QstepScaleComputed))*log(R)))));

        // Sanity Check 
        if(Sts & MFX_BRC_ERR_MAX_FRAME) {
            Ipp32s qp_prev = f->m_sliceQpY; // Qstep2QP(QstepScaleComputed, mQuantOffset));

            if(qp_prev < 51 + mQuantOffset) {
                Ipp64f rateQstepNew = pow( (Ipp64f) mfs / (Ipp64f) maxFrameSizeInBits, mMinQstepRateEP);
                Ipp64f QstepScaleUpdtComputed = rateQstepNew * mMinQstepCmplxK[1] * cmplxExp;
                Ipp32s qp_now = Qstep2QP(QstepScaleUpdtComputed, mQuantOffset);
                Ipp32s off = IPP_MAX(1, ((Ipp32f)(bits - maxFrameSizeInBits)/((Ipp32f)bits*0.16) + 0.5));
                if(qp_prev + off > qp_now) {
                    qp_now = IPP_MIN(51, qp_prev + off);
                    QstepScaleUpdtComputed = QP2Qstep(qp_now);
                    mMinQstepCmplxK[1] = QstepScaleUpdtComputed / (rateQstepNew * cmplxExp);
                    mMinQstepCmplxKUpdtErr[1] = 0.16;
                }
            }
        }

    } else if(layer == 2) {

        // update only if enough bits (coeffs coded most probably not just overhead)
        if(bits < (Ipp32f)mBitsDesiredFrame*0.66f) return;
        if(f->m_encOrder < mMinQstepCmplxKInitEncOrder && bits < (Ipp32s) maxFrameSizeInBits) return;

        Ipp64f iCmplx = IPP_MAX(BRC_MIN_CMPLX_FACTOR, f->m_stats[mLowres]->m_avgIntraSatd);
        Ipp64f fzCmplx = f->m_fzCmplx;
        Ipp64f MinQstepCmplxK = f->m_fzCmplxK;

        Ipp64f R = mfs / (Ipp64f) bits;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f rateQstep = pow(R, BRC_CONST_EXP_R_B1);
        Ipp64f cmplxExp = pow(C,  BRC_CONST_EXP_CP_B1) * pow(iCmplx,  BRC_CONST_EXP_CI_B1);
        Ipp64f cmplxQstep = MinQstepCmplxK * cmplxExp;
        Ipp64f QstepScaleComputed = rateQstep * cmplxQstep;
        Ipp64f QstepScaleReal = QP2Qstep(f->m_sliceQpY, mQuantOffset);
        Ipp64f dS = log(QstepScaleReal) - log(QstepScaleComputed);
        Ipp64f upDlt = 0.5;
        if(Sts & MFX_BRC_ERR_MAX_FRAME) upDlt = 1.0;
        // abs envelope + iirmavg4
        mMinQstepCmplxKUpdtErr[layer] = IPP_MAX((mMinQstepCmplxKUpdtErr[layer] + abs(dS)/MinQstepCmplxK)/2, abs(dS)/MinQstepCmplxK);
        dS = BRC_CLIP(dS, -0.5, 1.0);

        mMinQstepCmplxK[layer] = MinQstepCmplxK * (1.0 + upDlt*dS);
        mMinQstepCmplxKUpdt[layer]++;
        mMinQstepCmplxKUpdtC[2] = iCmplx;

        // Sanity Check 
        if(Sts & MFX_BRC_ERR_MAX_FRAME) {
            Ipp32s qp_prev = f->m_sliceQpY; // Qstep2QP(QstepScaleComputed, mQuantOffset);
            if(qp_prev < 51 + mQuantOffset) {
                Ipp64f rateQstepNew = pow( (Ipp64f) mfs / (Ipp64f) maxFrameSizeInBits, BRC_CONST_EXP_R_B1);
                Ipp64f QstepScaleUpdtComputed = rateQstepNew * mMinQstepCmplxK[layer] * cmplxExp;
                Ipp32s qp_now = Qstep2QP(QstepScaleUpdtComputed, mQuantOffset);
                Ipp32s off = IPP_MAX(1, ((Ipp32f)(bits - maxFrameSizeInBits)/((Ipp32f)bits*0.16) + 0.5));
                if(qp_prev + off > qp_now) {
                    qp_now = IPP_MIN(51, qp_prev + off);
                    QstepScaleUpdtComputed = QP2Qstep(qp_now);
                    mMinQstepCmplxK[layer] = QstepScaleUpdtComputed / (rateQstepNew * cmplxExp);
                    mMinQstepCmplxKUpdtErr[layer] = 0.16;
                }
            }
        }
    } else if(layer == 3) {

        // update only if enough bits (coeffs coded most probably not just overhead)
        if(bits < (Ipp32f)mBitsDesiredFrame*0.66f) return;
        if(f->m_encOrder < mMinQstepCmplxKInitEncOrder && bits < (Ipp32s) maxFrameSizeInBits) return;

        Ipp64f iCmplx = IPP_MAX(BRC_MIN_CMPLX_FACTOR, f->m_stats[mLowres]->m_avgIntraSatd);
        Ipp64f fzCmplx = f->m_fzCmplx;
        Ipp64f MinQstepCmplxK = f->m_fzCmplxK;

        Ipp64f R = mfs / (Ipp64f) bits;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f rateQstep = pow(R, BRC_CONST_EXP_R_B2);
        Ipp64f cmplxExp = pow(C,  BRC_CONST_EXP_CP_B2) * pow(iCmplx,  BRC_CONST_EXP_CI_B2);
        Ipp64f cmplxQstep = MinQstepCmplxK * cmplxExp;
        Ipp64f QstepScaleComputed = rateQstep * cmplxQstep;
        Ipp64f QstepScaleReal = QP2Qstep(f->m_sliceQpY, mQuantOffset);
        Ipp64f dS = log(QstepScaleReal) - log(QstepScaleComputed);
        Ipp64f upDlt = 0.5;
        if(Sts & MFX_BRC_ERR_MAX_FRAME) upDlt = 1.0;
        // abs envelope + iirmavg4
        mMinQstepCmplxKUpdtErr[layer] = IPP_MAX((mMinQstepCmplxKUpdtErr[layer] + abs(dS)/MinQstepCmplxK)/2, abs(dS)/MinQstepCmplxK);
        dS = BRC_CLIP(dS, -0.5, 1.0);

        mMinQstepCmplxK[layer] = MinQstepCmplxK * (1.0 + upDlt*dS);
        mMinQstepCmplxKUpdt[layer]++;
        mMinQstepCmplxKUpdtC[3] = iCmplx;

        // Sanity Check 
        if(Sts & MFX_BRC_ERR_MAX_FRAME) {
            Ipp32s qp_prev = f->m_sliceQpY; // Qstep2QP(QstepScaleComputed, mQuantOffset);
            if(qp_prev < 51 + mQuantOffset) {
                Ipp64f rateQstepNew = pow( (Ipp64f) mfs / (Ipp64f) maxFrameSizeInBits, BRC_CONST_EXP_R_B2);
                Ipp64f QstepScaleUpdtComputed = rateQstepNew * mMinQstepCmplxK[layer] * cmplxExp;
                Ipp32s qp_now = Qstep2QP(QstepScaleUpdtComputed, mQuantOffset);
                Ipp32s off = IPP_MAX(1, ((Ipp32f)(bits - maxFrameSizeInBits)/((Ipp32f)bits*0.16) + 0.5));
                if(qp_prev + off > qp_now) {
                    qp_now = IPP_MIN(51, qp_prev + off);
                    QstepScaleUpdtComputed = QP2Qstep(qp_now);
                    mMinQstepCmplxK[layer] = QstepScaleUpdtComputed / (rateQstepNew * cmplxExp);
                    mMinQstepCmplxKUpdtErr[layer] = 0.16;
                }
            }
        }
    } else if(layer == 4) {

        // update only if enough bits (coeffs coded most probably not just overhead)
        if(bits < (Ipp32f)mBitsDesiredFrame*0.4f) return;
        if(f->m_encOrder < mMinQstepCmplxKInitEncOrder && bits < (Ipp32s) maxFrameSizeInBits) return;

        Ipp64f iCmplx = IPP_MAX(BRC_MIN_CMPLX_FACTOR, f->m_stats[mLowres]->m_avgIntraSatd);
        Ipp64f fzCmplx = f->m_fzCmplx;
        Ipp64f MinQstepCmplxK = f->m_fzCmplxK;

        Ipp64f R = mfs / (Ipp64f) bits;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f rateQstep = pow(R, BRC_CONST_EXP_R_B3);
        Ipp64f cmplxExp = pow(C,  BRC_CONST_EXP_CP_B3) * pow(iCmplx,  BRC_CONST_EXP_CI_B3);
        Ipp64f cmplxQstep = MinQstepCmplxK * cmplxExp;
        Ipp64f QstepScaleComputed = rateQstep * cmplxQstep;
        Ipp64f QstepScaleReal = QP2Qstep(f->m_sliceQpY, mQuantOffset);
        Ipp64f dS = log(QstepScaleReal) - log(QstepScaleComputed);
        Ipp64f upDlt = 0.5;
        if(Sts & MFX_BRC_ERR_MAX_FRAME) upDlt = 1.0;

        // abs envelope + iirmavg4
        mMinQstepCmplxKUpdtErr[layer] = IPP_MAX((mMinQstepCmplxKUpdtErr[layer] + abs(dS)/MinQstepCmplxK)/2, abs(dS)/MinQstepCmplxK);
        dS = BRC_CLIP(dS, -0.5, 1.0);

        mMinQstepCmplxK[layer] = MinQstepCmplxK * (1.0 + upDlt*dS);
        mMinQstepCmplxKUpdt[layer]++;
        mMinQstepCmplxKUpdtC[4] = iCmplx;

        // Sanity Check 
        if(Sts & MFX_BRC_ERR_MAX_FRAME) {
            Ipp32s qp_prev = f->m_sliceQpY; // Qstep2QP(QstepScaleComputed, mQuantOffset);
            if(qp_prev < 51 + mQuantOffset) {
                Ipp64f rateQstepNew = pow( (Ipp64f) mfs / (Ipp64f) maxFrameSizeInBits, BRC_CONST_EXP_R_B3);
                Ipp64f QstepScaleUpdtComputed = rateQstepNew * mMinQstepCmplxK[layer] * cmplxExp;
                Ipp32s qp_now = Qstep2QP(QstepScaleUpdtComputed, mQuantOffset);
                Ipp32s off = IPP_MAX(1, ((Ipp32f)(bits - maxFrameSizeInBits)/((Ipp32f)bits*0.16) + 0.5));
                if(qp_prev + off > qp_now) {
                    qp_now = IPP_MIN(51, qp_prev + off);
                    QstepScaleUpdtComputed = QP2Qstep(qp_now);
                    mMinQstepCmplxK[layer] = QstepScaleUpdtComputed / (rateQstepNew * cmplxExp);
                    mMinQstepCmplxKUpdtErr[layer] = 0.16;
                }
            }
        }
    }
}
#endif

Ipp64f H265BRC::GetMinQForMaxFrameSize(Ipp32u maxFrameSizeInBits, H265VideoParam *video, Frame *f, Ipp32s layer, Frame *maxpoc, Ipp32s myPos)
{
    f->m_maxFrameSizeInBitsSet = maxFrameSizeInBits;
#ifndef AMT_MAX_FRAME_SIZE
    return 0;
#else
    if (!maxFrameSizeInBits) {
        return 0;
    }

    if (f->m_picCodeType == MFX_FRAMETYPE_I) {
        Ipp64f fzCmplx = f->m_stats[mLowres]->m_avgIntraSatd;
        f->m_fzCmplx = fzCmplx;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        
        // complexity based conditional use of intra update
        Ipp64f irat = mMinQstepCmplxKUpdtC[0]/C;
        //if(mMinQstepCmplxKUpdt[0] && (irat > 1.16 || irat < 0.84)) {
        if(mMinQstepCmplxKUpdt[0] && (irat > 1.4 || irat < 0.7)) {
            // reset intra
            mMinQstepCmplxK[0] = BRC_CONST_MUL_I8;
            mMinQstepCmplxKUpdt[0] = 0;
            mMinQstepCmplxKUpdtC[0] = C;
            mMinQstepCmplxKUpdtErr[0] = 0.16;
        }
        f->m_fzCmplxK = mMinQstepCmplxK[0];

        Ipp64f BitsDesiredFrame = (Ipp64f) maxFrameSizeInBits;
        if (mMinQstepCmplxKUpdt[0]) {
            BitsDesiredFrame *= (1.0 - 0.165 - IPP_MIN(0.115, mMinQstepCmplxKUpdtErr[0]));
        }

        Ipp64f R = mfs / (Ipp64f) BitsDesiredFrame;
        
        Ipp64f QstepScale = pow(R, BRC_CONST_EXP_R_I8);
        Ipp64f cmplxQstep = mMinQstepCmplxK[0] * pow(C, BRC_CONST_EXP_C_I8);
        QstepScale *= cmplxQstep;
        
        if (!mMinQstepCmplxKUpdt[0]) {
            QstepScale = IPP_MIN(128, QstepScale);
        }
        // reset SOP
        ResetMinQForMaxFrameSize(video, f);

        return QstepScale;
    } else  if (f->m_picCodeType == MFX_FRAMETYPE_P) {
        Frame *rf = f->m_refPicList[0].m_refFrames[0];
        Ipp64f minQI = 0.0;
        // For All
        Ipp64f BitsDesiredI = (Ipp64f) maxFrameSizeInBits;

        if (mMinQstepCmplxKUpdt[0]) {
            BitsDesiredI *= (1.0 - 0.165 - IPP_MIN(0.115, mMinQstepCmplxKUpdtErr[0]));
        }

        Ipp64f iCmplx = IPP_MAX(BRC_MIN_CMPLX_FACTOR, f->m_stats[mLowres]->m_avgIntraSatd);

        Ipp64f RI = mfs / (Ipp64f) BitsDesiredI;
        minQI = pow(RI, BRC_CONST_EXP_R_I8);
        Ipp64f cmplxQstepI = mMinQstepCmplxK[0] * pow(iCmplx, BRC_CONST_EXP_C_I8);
        minQI *= cmplxQstepI;
        if (!mMinQstepCmplxKUpdt[0]) {
            minQI = IPP_MIN(128, minQI);
        }
        
        // BRC seems to be unware of GOPIndex Layer! -N
        //Ipp64f fzCmplx = f->m_avCmplx;
        Ipp64f avc = 0;
        Ipp64f cp[8] = {0};
        Ipp32s rdist = IPP_MIN(myPos, IPP_MAX(0, IPP_MIN(7, f->m_poc - rf->m_poc - 1)));
        switch(rdist) {
        default:
        case 7: cp[7] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-7]; avc += cp[7];
        case 6: cp[6] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-6]; avc += cp[6];
        case 5: cp[5] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-5]; avc += cp[5];
        case 4: cp[4] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-4]; avc += cp[4];
        case 3: cp[3] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-3]; avc += cp[3];
        case 2: cp[2] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-2]; avc += cp[2];
        case 1: cp[1] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-1]; avc += cp[1];
        case 0: cp[0] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos];   avc += cp[0];
        };
        avc /= (rdist+1);
        Ipp64f fzCmplx = avc;
        Ipp64f cpm = IPP_MAX(IPP_MAX(IPP_MAX(cp[3],cp[2]),IPP_MAX(cp[0],cp[1])), IPP_MAX(IPP_MAX(cp[6],cp[7]),IPP_MAX(cp[4],cp[5])));
        Ipp32s cpmc = 0;
        for (Ipp32s k = 0; k <= rdist; k++) {
            if (cp[k] > fzCmplx) cpmc++;
        }
        // single peak
        if(rdist > 1 && cpmc == 1) fzCmplx = cpm;
        
        f->m_fzCmplx = fzCmplx;

        // complexity based conditional use of update
        Ipp64f irat = mMinQstepCmplxKUpdtC[1]/iCmplx;
        //if(mMinQstepCmplxKUpdt[0] && (irat > 1.16 || irat < 0.84)) {
        if(mMinQstepCmplxKUpdt[1] && (irat > 1.4 || irat < 0.7)) {
            // reset 
            if(video->GopRefDist>4)      {
                mMinQstepCmplxK[1] = BRC_CONST_MUL_P8;
                mMinQstepRateEP = BRC_CONST_EXP_R_P8;
            } else if(video->GopRefDist>1) {
                mMinQstepCmplxK[1] = BRC_CONST_MUL_P3;
                mMinQstepRateEP = BRC_CONST_EXP_R_P3;
            } else if (video->picStruct == MFX_PICSTRUCT_PROGRESSIVE) {
                mMinQstepCmplxK[1] = BRC_CONST_MUL_P1;
                mMinQstepRateEP = BRC_CONST_EXP_R_P1;
            } else {
                mMinQstepCmplxK[1] = BRC_CONST_MUL_P2;
                mMinQstepRateEP = BRC_CONST_EXP_R_P2;
            }

            mMinQstepCmplxKUpdt[1] = 0;
            mMinQstepCmplxKUpdtC[1] = iCmplx;
            mMinQstepCmplxKUpdtErr[1] = 0.16;
        }

        f->m_fzCmplxK = mMinQstepCmplxK[1];
        f->m_fzRateE = mMinQstepRateEP;
        Ipp64f BitsDesiredFrame = (Ipp64f) maxFrameSizeInBits;
        if (mMinQstepCmplxKUpdt[1]) {
            BitsDesiredFrame *= (1.0 - 0.165 - IPP_MIN(0.115, mMinQstepCmplxKUpdtErr[1]));
        }

        Ipp64f R = mfs / (Ipp64f) BitsDesiredFrame;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f QstepScale = pow(R, mMinQstepRateEP);
        Ipp64f cmplxQstep = mMinQstepCmplxK[1] * pow(C,  mMinQstepPCmplxEP) * pow(iCmplx,  mMinQstepICmplxEP);

        QstepScale *= cmplxQstep;
        if (!mMinQstepCmplxKUpdt[1]) {
            QstepScale = IPP_MIN(128, QstepScale);
        }
        
        if (QstepScale < minQI) {
            Ipp32f qp_cur = Qstep2QP(QstepScale, mQuantOffset);
            Ipp32f qp0 =  qp_cur;
            Ipp32f qp_prev = rf->m_sliceQpY;
            if (f->m_picStruct != MFX_PICSTRUCT_PROGRESSIVE && video->GopRefDist == 1 && video->MaxRefIdxP[0] > 1 && f->m_refPicList[0].m_refFrames[1]) { 
                // Assuming IPP coding (not interlace Low delay Pyramid)
                // Pick qp from same field parity
                qp_prev = f->m_refPicList[0].m_refFrames[1]->m_sliceQpY;
            }
            
            Ipp32f besta = -1;
            Ipp64f bestQ = QstepScale;
            Ipp64f bestQp = qp_cur;
            // ref quality determines intra
            Ipp32f a = IPP_MIN(5, IPP_MAX(qp_prev - qp_cur -1, 0));
            Ipp32f b = 6 - a;
            Ipp64f minQN = (b*QstepScale + a*minQI)/6.0;
            if (a > 0) {
                besta = a;
                Ipp32f qp_i = Qstep2QP(minQI, mQuantOffset);
                if( qp_prev - qp_i - 1 > 5) {
                    // even min Intra q is lower than prev_qp - 6
                    minQN = minQI; bestQ = minQI; bestQp = qp_i;
                } else {
                    for(Ipp64f rq = QstepScale; rq < minQI; ) {
                        qp_cur = Qstep2QP(rq, mQuantOffset);
                        a = IPP_MIN(5, IPP_MAX(qp_prev - qp_cur -1, 0));
                        b = 6 - a;
                        Ipp64f tQN = (b*rq + a*minQI)/6.0;
                        if(tQN < minQN || a==5) { minQN = tQN; besta= a; bestQ = rq; bestQp = qp_cur;}
                        if(a <= 0) break;
                        rq += (QP2Qstep(qp_cur+1, mQuantOffset) - QP2Qstep(qp_cur, mQuantOffset));
                    }
                }
            }
            QstepScale = bestQ;
        }
        return QstepScale;

    } else if(layer == 2) {

        Ipp64f BitsDesiredFrame = (Ipp64f) maxFrameSizeInBits;
        if (mMinQstepCmplxKUpdt[layer]) {
            BitsDesiredFrame *= (1.0 - 0.16 - IPP_MIN(0.10, mMinQstepCmplxKUpdtErr[layer]));
        }

        Ipp64f iCmplx = f->m_stats[mLowres]->m_avgIntraSatd;
        Frame *rff = f->m_refPicList[1].m_refFrames[0];
        Ipp32s rdistf = IPP_MAX(0, IPP_MIN(4, rff->m_poc - f->m_poc - 1));
        Ipp64f cn[4]={-1,-1,-1,-1};
        switch(rdistf) {
        default:
        case 3: cn[3] = (myPos+4>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+4];
        case 2: cn[2] = (myPos+3>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+3];
        case 1: cn[1] = (myPos+2>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+2];
        case 0:
            {
                cn[0] = (myPos+1>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+1];
                if(cn[0]>-1 && maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1] < iCmplx)  cn[0] *= (Ipp64f)iCmplx/(Ipp64f)maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1];
            }
        };
        Frame *rfp = f->m_refPicList[0].m_refFrames[0];
        Ipp32s rdistp = IPP_MIN(myPos, IPP_MAX(0, IPP_MIN(4, f->m_poc - rfp->m_poc - 1)));
        Ipp64f cp[4]={0};
        switch(rdistp) {
        default:
        case 3: cp[3] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-3];
        case 2: cp[2] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-2];
        case 1: cp[1] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-1];
        case 0: cp[0] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos];
        };
        Ipp64f mcp = IPP_MAX(IPP_MAX(cp[3],cp[2]),IPP_MAX(cp[1],cp[0]));
        Ipp64f mcn = IPP_MAX(IPP_MAX(cn[3],cn[2]),IPP_MAX(cn[1],cn[0]));

        Ipp64f fzCmplx = (mcn>-1)?IPP_MIN(mcp, mcn):mcp;
        f->m_fzCmplx = fzCmplx;
        f->m_fzCmplxK = mMinQstepCmplxK[layer];

        Ipp64f R = mfs / (Ipp64f) BitsDesiredFrame;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f QstepScale = pow(R, BRC_CONST_EXP_R_B1);
        Ipp64f cmplxQstep = mMinQstepCmplxK[layer] * pow(C,  BRC_CONST_EXP_CP_B1) * pow(iCmplx,  BRC_CONST_EXP_CI_B1);
        QstepScale *= cmplxQstep;

        if (!mMinQstepCmplxKUpdt[layer]) {
            QstepScale = IPP_MIN(128, QstepScale);
        }
        return QstepScale;

    } else if(layer == 3) {

        Ipp64f BitsDesiredFrame = (Ipp64f) maxFrameSizeInBits;
        if (mMinQstepCmplxKUpdt[layer]) {
            BitsDesiredFrame *= (1.0 - 0.16 - IPP_MIN(0.10, mMinQstepCmplxKUpdtErr[layer]));
        }
        Ipp64f iCmplx = f->m_stats[mLowres]->m_avgIntraSatd;
        Frame *rff = f->m_refPicList[1].m_refFrames[0];
        Ipp32s rdistf = IPP_MAX(0, IPP_MIN(2, rff->m_poc - f->m_poc - 1));
        Ipp64f cn[2]={-1, -1};
        switch(rdistf) {
        default:
        case 1: cn[1] = (myPos+2>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+2];
        case 0: 
            {
                cn[0] = (myPos+1>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+1];
                if(cn[0]>-1 && maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1] < iCmplx)  cn[0] *= (Ipp64f)iCmplx/(Ipp64f)maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1];
            }
        };
        Frame *rfp = f->m_refPicList[0].m_refFrames[0];
        Ipp32s rdistp = IPP_MIN(myPos, IPP_MAX(0, IPP_MIN(2, f->m_poc - rfp->m_poc - 1)));
        Ipp64f cp[2]={0};
        switch(rdistp) {
        default:
        case 1: cp[1] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos-1];
        case 0: cp[0] = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos];
        }
        Ipp64f mcp = IPP_MAX(cp[0],cp[1]);
        Ipp64f mcn = IPP_MAX(cn[0],cn[1]);

        Ipp64f fzCmplx = (mcn>-1)?IPP_MIN(mcp, mcn):mcp;
        f->m_fzCmplx = fzCmplx;
        f->m_fzCmplxK = mMinQstepCmplxK[layer];

        Ipp64f R = mfs / (Ipp64f) BitsDesiredFrame;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f QstepScale = pow(R, BRC_CONST_EXP_R_B2);
        Ipp64f cmplxQstep = mMinQstepCmplxK[layer] * pow(C,  BRC_CONST_EXP_CP_B2) * pow(iCmplx,  BRC_CONST_EXP_CI_B2);
        QstepScale *= cmplxQstep;
        
        if (!mMinQstepCmplxKUpdt[layer]) {
            QstepScale = IPP_MIN(128, QstepScale);
        }
        return QstepScale;

    } else if(layer == 4) {

        Ipp64f BitsDesiredFrame = (Ipp64f) maxFrameSizeInBits;
        if (mMinQstepCmplxKUpdt[layer]) {
            BitsDesiredFrame *= (1.0 - 0.16 - IPP_MIN(0.10, mMinQstepCmplxKUpdtErr[layer]));
        }
        Ipp64f iCmplx = f->m_stats[mLowres]->m_avgIntraSatd;
        Ipp64f cn = (myPos+1>32)?-1:maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos+1];
        // Custom Adaptation for next frame
        if(cn>-1 && maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1] < iCmplx)  cn *= (Ipp64f)iCmplx/(Ipp64f)maxpoc->m_stats[mLowres]->m_intraSatdHist[myPos+1];

        Ipp64f cp = maxpoc->m_stats[mLowres]->m_bestSatdHist[myPos];
        
        Ipp64f fzCmplx = (cn>-1)?IPP_MIN(cp, cn):cp;
        f->m_fzCmplx = fzCmplx;
        f->m_fzCmplxK = mMinQstepCmplxK[layer];

        Ipp64f R = mfs / (Ipp64f) BitsDesiredFrame;
        Ipp64f C = IPP_MAX(BRC_MIN_CMPLX_FACTOR, fzCmplx);
        Ipp64f QstepScale = pow(R, BRC_CONST_EXP_R_B3);
        Ipp64f cmplxQstep = mMinQstepCmplxK[layer] * pow(C,  BRC_CONST_EXP_CP_B3) * pow(iCmplx,  BRC_CONST_EXP_CI_B3);
        QstepScale *= cmplxQstep;

        if (!mMinQstepCmplxKUpdt[layer]) {
            QstepScale = IPP_MIN(128, QstepScale);
        }
        return QstepScale;

    } else {
        return 0;
    }
#endif
}


// AMT_LTR

const mfxF64 COEFF_INTRA[2] = { -0.114758, -0.411153 };
void get_coeff_intra(mfxF64 *pCoeff) {
    pCoeff[0] = COEFF_INTRA[0];
    pCoeff[1] = COEFF_INTRA[1];
}

#define PWR_SC 0.61
#define MIN_SC 1.0
#define MAX_SC 64.0

mfxF64 getScaledIntraBits(mfxF64 targetBits, mfxF64 rawSize, mfxF64 SC0) {
    mfxF64 SC = SC0;
    if (SC < MIN_SC)  SC = MIN_SC;
    if (SC > MAX_SC)  SC = MAX_SC;
    SC = pow((mfxF64)SC, PWR_SC);
    mfxF64 dBits = log((targetBits / rawSize) / SC);

    return dBits;
}

mfxI32 estimate_qp_intra(mfxI32 targetBits, mfxI32 rawSize, mfxF64 sc)
{
    mfxF64 dBits = getScaledIntraBits(targetBits, rawSize, sc);
    mfxF64 coeffIntra[2];
    get_coeff_intra(coeffIntra);

    mfxF64 qpNew = (dBits - coeffIntra[1]) / coeffIntra[0];
    mfxI32 qp = (mfxI32)(qpNew + 0.5);

    return qp;
}

mfxI32 compute_new_qp_intra(mfxI32 targetBits, mfxI32 rawSize, mfxF64 sc, mfxI32 iBits, mfxF64 icmplx, mfxI32 iqp) {
    mfxF64 qp_hat = estimate_qp_intra(iBits, rawSize, icmplx);
    mfxF64 dQp = iqp - qp_hat;
    if (dQp < (-6.0))  dQp = -6.0;
    if (dQp > 6.0)     dQp = 6.0;

    mfxF64 qp_pred = estimate_qp_intra(targetBits, rawSize, sc);
    mfxF64 qpNew = qp_pred + dQp;
    mfxI32 qp = (mfxI32)(qpNew + 0.5);
    if (qp < 4)  qp = 4;
    if (qp > 50) qp = 50;

    return qp;
}

Ipp32u getRawFrameSizeInPel(Ipp32s width, Ipp32s height, Ipp16u chromaFormat) {
    Ipp32u rawFrameSizeInPel = width * height;

    if(chromaFormat == MFX_CHROMAFORMAT_YUV420)
        rawFrameSizeInPel += rawFrameSizeInPel >> 1;
    else if (chromaFormat == MFX_CHROMAFORMAT_YUV422)
        rawFrameSizeInPel += rawFrameSizeInPel;
    else if (chromaFormat == MFX_CHROMAFORMAT_YUV444)
        rawFrameSizeInPel += rawFrameSizeInPel << 1;

    return rawFrameSizeInPel;
}

Ipp32s estimateNewIntraQP(Ipp32s GopRefDist, Ipp32s ltrConfidenceLevel, Ipp32s minDQP, Ipp32s QP0)
{
    if (minDQP >= 0)
        return QP0;

    Ipp32s qp = QP0;
    Ipp32s dqp = 0;

    if (ltrConfidenceLevel > 0)
    {
        if (GopRefDist == 1) {
            if (minDQP < (-6))
                minDQP = -6;
            if (minDQP < 0) {
                if (ltrConfidenceLevel < 2) {
                    dqp -= 2;
                }
                else if (ltrConfidenceLevel < 3) {
                    dqp -= 5;
                }
                else {
                    dqp -= 6;
                }
                if (dqp < minDQP)
                    dqp = minDQP;
            }
        }
        else if (GopRefDist == 8) {
            if (minDQP < (-4))
                minDQP = -4;
            if (minDQP < 0) {
                if (ltrConfidenceLevel < 2) {
                    dqp -= 2;
                }
                else {
                    dqp -= 4;
                }
                if (dqp < minDQP)
                    dqp = minDQP;
            }
        }
    }

    qp = QP0 + dqp;
    return qp;
}

Ipp32s H265BRC::GetQP(H265VideoParam *video, Frame *frames[], Ipp32s numFrames)
{
    Frame *frame = nullptr;
    Ipp32s i = 0, j = 0;
    Ipp32s qp = 0;

    if (video->AnalyzeCmplx) {
        if (!frames || numFrames <= 0) {
            if (mQstepBase > 0)
                return Qstep2QP(mQstepBase, mQuantOffset) - mQuantOffset;
            else
                return GetInitQP() - mQuantOffset;
        }

        // limit ourselves to miniGOP+1 for the time being
        Ipp32s lalenlim = (video->picStruct == MFX_PICSTRUCT_PROGRESSIVE ? video->GopRefDist + 1 : video->GopRefDist*2 /* + 2*/);
        if (numFrames > lalenlim)
            numFrames = lalenlim;

        Ipp32s len = numFrames;
        //for (i = 1; i < numFrames; i++) {
        //    if (!frames[i] || (frames[i]->m_stats[mLowres]->m_sceneCut)) {
        //        len = i;
        //        break;
        //    }
        //}
#ifdef PRINT_BRC_STATS
        if (frames[0]->m_stats[mLowres]->m_sceneCut > 0)
            brc_fprintf("SCENE CUT %d \n", frames[0]->m_encOrder);
#endif

        Ipp64f scCmplx = -1;
        Ipp32s rfo = -1, reo = -1, ri = -1, rso = 0;
        Ipp32s eso = -1, ei = -1;
        for (i = 0; i < len; i++) {
            if (frames[i] && (frames[i]->m_stats[mLowres]->m_sceneCut > 0)) { // enc order scene cut frame
                eso = frames[i]->m_sceneOrder;
                ei = i;
            }
            if (frames[i] && (frames[i]->m_stats[mLowres]->m_sceneCut < 0)) { // real (frame order) scene cut frame
                if (frames[i]->m_sceneOrder == eso) { // enc order scene cut must have been met, we need the sc frame nearest in frame order to the I (in case we have several scene cuts within frames[])
                    rfo = frames[i]->m_frameOrder;
                    reo = frames[i]->m_encOrder;
                    ri = i;
                }
            }
        }

        Ipp32s mindist = 999;
        if (ri >= 0) {
            for (i = 0; i < len; i++) {
                if (frames[i] && (frames[i]->m_sceneOrder == eso) && (i != ri)) { // real (frame order) scene cut frame
                    Ipp32s dist = frames[i]->m_frameOrder - rfo; // can not be <= 0
                    if (dist < mindist) {
                        scCmplx = frames[i]->m_stats[mLowres]->m_avgInterSatd;
                        mindist = dist;
                    }
                }
            }

            if (scCmplx < frames[ri]->m_stats[mLowres]->m_avgBestSatd)
                frames[ri]->m_stats[mLowres]->m_avgBestSatd = scCmplx;
            
            frames[ri]->m_stats[mLowres]->m_sceneCut = 0; // we estimate the cmplx the first time we see the scene cut frame, and don't update it later - shuold be good enough for our needs
        }

        Ipp64f avCmplx = 0;

        if (frames[0]->m_stats[mLowres]->m_sceneCut > 0) {
            Ipp32s cnt = 1;
            avCmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;
            for (i = 0; i < len; i++) {
                if (frames[i]->m_sceneOrder == eso) {
                    avCmplx += frames[i]->m_stats[mLowres]->m_avgBestSatd;
                    cnt++;
                }
            }
            avCmplx /= cnt;
        } else {
            for (i = 0; i < len; i++)
                avCmplx += frames[i]->m_stats[mLowres]->m_avgBestSatd;
            avCmplx /= len;
        }
        frames[0]->m_avCmplx = avCmplx;

        frames[0]->m_totAvComplx = mTotAvComplx;
        frames[0]->m_totComplxCnt = mTotComplxCnt;
        frames[0]->m_complxSum = mComplxSum;

        Ipp64f w = 0, qScale, r, prCmplx = mTotPrevCmplx;
        Ipp64f decay = mRCMode != MFX_RATECONTROL_AVBR ? BRC_CMPLX_DECAY : BRC_CMPLX_DECAY_AVBR;

        mTotAvComplx *= decay;
        mTotComplxCnt *= decay;

        if (avCmplx < BRC_MIN_CMPLX)
            avCmplx = BRC_MIN_CMPLX;

        mTotAvComplx += avCmplx;
        mTotComplxCnt += 1;
        Ipp64f complx = mTotAvComplx / mTotComplxCnt;

        mComplxSum += avCmplx;
        mTotMeanComplx = mComplxSum / (frames[0]->m_encOrder + 1);

        Ipp64f expnt = mRCMode != MFX_RATECONTROL_AVBR ? BRC_QSTEP_COMPL_EXPONENT : BRC_QSTEP_COMPL_EXPONENT_AVBR;
        frames[0]->m_CmplxQstep = pow(complx, expnt);

        Ipp32s layer = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[0]->m_pyramidLayer)));

        //commented out since it affected certain clips (eg. bbc)
        //if (frames[0]->m_encOrder == 0) {
        //    if (avCmplx > 6)
        //        mQstepScale0 = mQstepScale = mQstepScale0 * avCmplx / 6;
        //}

        if (prCmplx) {
            r = fabs(avCmplx / prCmplx - 1);
            {
                w = r * 0.1;
                if (w > 1) w = 1;

                qScale = w * mQstepScale0 + (1 - w) * mQstepScale;

                if (mRCMode != MFX_RATECONTROL_AVBR || ((qScale - mQstepScale) * (avCmplx - prCmplx) > 0))
                {
                    mCmplxRate *= qScale / mQstepScale;
                    brc_fprintf("prCmplx %d %.3f %.3f %.1f %.3f %.3f \n", frames[0]->m_encOrder, avCmplx , prCmplx,  mCmplxRate , mQstepScale, qScale);
                    mQstepScale = qScale;
                }
            }
        }

        Ipp32s maxpoc = frames[0]->m_poc;
        Ipp32s maxpocpos = 0;
        for (Ipp32s ki = 0; ki < len; ki++) {
            if (frames[ki]->m_poc >= maxpoc) {
                maxpoc = frames[ki]->m_poc;
                maxpocpos = ki;
            }
        }
        Ipp32s histsize = frames[maxpocpos]->m_stats[mLowres]->m_bestSatdHist.size()-1;
        Ipp32s mypos = histsize - IPP_MIN(histsize, IPP_MAX(0, (maxpoc - frames[0]->m_poc)));

        // Get Min mQStepScale based on Cmplx Rate Model based Prediction
        Ipp64f minQ = GetMinQForMaxFrameSize(frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? mMaxFrameSizeInBitsI : mMaxFrameSizeInBits, video, frames[0], layer, frames[maxpocpos], mypos);

        Ipp64f q = frames[0]->m_CmplxQstep * mQstepScale;

        brc_fprintf("%d %.3f %.3f %.4f %.3f %.3f %.3lf \n", frames[0]->m_encOrder, q, complx, frames[0]->m_CmplxQstep , mQstepScale, w, minQ);

        Ipp64f targetBits = 0;
        for (i = 0; i < len; i++) {
            Ipp32s lr = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[i]->m_pyramidLayer)));
            targetBits += mLayerTarget[lr];
        }

        if (mRCMode != MFX_RATECONTROL_AVBR) // cbr/vbr
        {
            Ipp32s predbits;
            mfxBRC_FrameData refFrameData;
            Ipp32s qpbase = Qstep2QP(q, mQuantOffset);
            Ipp32s qpmin = 0, qp_, qp0;

            if (frames[0]->m_picCodeType == MFX_FRAMETYPE_I || frames[0]->m_picCodeType == MFX_FRAMETYPE_P)
                SetMiniGopData(frames, len);

            qp0 = qp = IPP_MAX(qpbase + layer - 1, 1);
            qp_ = IPP_MAX(qp - 3, 1);

            frames[0]->m_qpBase = qp;

            brc_fprintf("%d: l-%d %d qp %d %.3f %.3f %.3f \n", frames[0]->m_encOrder, layer, len, qp, complx,  frames[0]->m_stats[mLowres]->m_avgBestSatd, frames[0]->m_stats[mLowres]->m_avgIntraSatd);

            Ipp64f maxbits;

            for (;;) {

                //if (mPrevQstepLayer[layer] <= 0  && frames[0]->m_encOrder == 0) {
                if (mPrevQstepLayer[layer] <= 0) {
                    Ipp64f qi = QP2Qstep(qp, mQuantOffset);
                    predbits = frames[0]->m_stats[mLowres]->m_avgBestSatd * predCoef * mParams.width * mParams.height / qi;
                    if (mQuantOffset)
                        predbits =  (Ipp32s)((predbits << mQuantOffset/6) * 0.9);
                    refFrameData.qp = qp;
                    refFrameData.encOrder = -1;
                    refFrameData.cmplx = frames[0]->m_stats[mLowres]->m_avgBestSatd;

                } else {
                    predbits = PredictFrameSize(video, frames, qp, &refFrameData);
                }

                if (mPrevQpLayer[layer] <= 0) {
                    mPrevCmplxLayer[layer] = refFrameData.cmplx;
                    mPrevQpLayer[layer] = qp;
                    mPrevQstepLayer[layer] = QP2Qstep(qp, mQuantOffset);
                    mPrevBitsLayer[layer] = predbits;
                }
                brc_fprintf(" %d %d \n", qp, predbits);

                maxbits = mPredBufFulness >> 1;

                if ((layer > 1) || qp >= refFrameData.qp)
                    if (maxbits > mHRD.bufSize >> 2)
                        maxbits = mHRD.bufSize >> 2;

                if (predbits > maxbits && qp < mQuantMax) { // frame is predicted to exceed half buf fullness
                    qp++;
                    qpmin = qp;
                } else if ((Ipp32s)(mPredBufFulness + mBitsDesiredFrame) - predbits > (Ipp32s)mHRD.bufSize) {
                    if (qp > qpmin + 1 && qp > qp_)
                        qp--;
                    else
                        break;
                } else
                    break;
            }

            if (video->m_framesInParallel <= video->GopRefDist / 2) {
                if (refFrameData.encOrder > mEncOrderCoded) {
                    if (qp < refFrameData.qp) {
                        qp = refFrameData.qp;
                    }
                }
            }

            UpdateMiniGopData(frames[0], qp);

            Ipp32s predbufFul;
            qpbase = IPP_MAX(qp + 1 - layer, 1);
            Ipp32s refqp = -1;

            for (;;) {
                Ipp32s qpp = qpbase;
                q = QP2Qstep(qpbase, mQuantOffset);

                // trying to forecast the buffer state for the LA frames
                Ipp32s forecastBits = predbits;
                //Ipp32s maxRefQp = -1, minRefQp = 99;
                for (i = 1; i < len; i++) {
                    Ipp32s lri = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[i]->m_pyramidLayer)));
                    Ipp32s qpi = qpbase + lri - 1;
                    UpdateMiniGopData(frames[i], qpi);
                    Ipp32s bits = PredictFrameSize(video, &frames[i], qpi, &refFrameData);
                    if (frames[i]->m_picCodeType == MFX_FRAMETYPE_P) {
                        refqp = refFrameData.qp;  // currently there can be only one p-frame (2 p-fields with the same refqp) in frames[]
                        frames[i]->m_refQp = refqp;
                    }

                    brc_fprintf("( %d %d ) ", frames[i]->m_encOrder, bits);
                    if (mPrevQpLayer[lri] <= 0) {
                        //mPrevCmplxLayer[lri] = frames[i]->m_stats[mLowres]->m_avgBestSatd;
                        mPrevCmplxLayer[lri] = refFrameData.cmplx; // ???
                        mPrevQpLayer[lri] = qpi;
                        mPrevQstepLayer[lri] = QP2Qstep(qpi, mQuantOffset);
                        mPrevBitsLayer[lri] = bits;
                    }

                    forecastBits += bits;
                }
                brc_fprintf("\n");

                predbufFul = mPredBufFulness;
                if (mRCMode == MFX_RATECONTROL_VBR && mParams.maxBitrate > (Ipp32s)mBitrate && mPredBufFulness > mRealPredFullness)
                    mPredBufFulness = mRealPredFullness;

                Ipp32s targetfullness = IPP_MAX(mParams.HRDInitialDelayBytes << 3, (Ipp32s)mHRD.bufSize >> 1);
                Ipp32s curdev = targetfullness - mPredBufFulness;
                Ipp64s estdev = curdev + forecastBits - targetBits;

    //            Ipp32s maxdevSoft = IPP_MIN((Ipp32s)(mHRD.bufSize >> 3), mPredBufFulness >> 1);
                Ipp32s maxdevSoft = IPP_MAX((Ipp32s)(curdev >> 1), mPredBufFulness >> 1);
                maxdevSoft = IPP_MIN((Ipp32s)(mHRD.bufSize >> 3), maxdevSoft);
                Ipp32s mindevSoft = IPP_MIN((Ipp32s)(mHRD.bufSize >> 3), ((Ipp32s)mHRD.bufSize - mPredBufFulness) >> 1);

                Ipp64f qf = 1;
                Ipp32s targ;

                brc_fprintf("dev %d %d %d %d \n", (int)estdev, forecastBits, (int)targetBits, len*(int)mHRD.inputBitsPerFrame);
                Ipp64f qf1 = 0;
                Ipp32s laLim = IPP_MIN(mPredBufFulness * 3 >> 2, (Ipp32s)mHRD.bufSize * 3 >> 3);
                if (forecastBits - (len - 1)*(Ipp32s)mHRD.inputBitsPerFrame > laLim) {
                    targ = (len - 1)*(Ipp32s)mHRD.inputBitsPerFrame + laLim;
                    if (targ < forecastBits >> 1)
                        targ = forecastBits >> 1;
                    qf1 = (Ipp64f)forecastBits / targ;
                }
                if (estdev > maxdevSoft) {
                    targ = targetBits + maxdevSoft - curdev;
                    if (targ < (forecastBits >> 1) + 1)
                        targ = (forecastBits >> 1) + 1;
                    if (targ < forecastBits)
                        qf = sqrt((Ipp64f)forecastBits / targ);
                } else if (estdev < -mindevSoft) { // && mRCMode != MFX_RATECONTROL_VBR) {
                    targ = targetBits - mindevSoft - curdev;
                    brc_fprintf("%d %d %d \n", mindevSoft, curdev, targ);
                    if (targ > (forecastBits * 3 >> 1) + 1)
                        targ = (forecastBits * 3 >> 1) + 1;
                    if (targ > forecastBits)
                        qf = sqrt((Ipp64f)forecastBits / targ);
                }

                qf = IPP_MAX(qf, qf1);

                q *= qf;
                mQstepBase = q;
                qpbase = Qstep2QP(q, mQuantOffset);

                if (qpbase > qpp) {
                    if (qpbase > refqp && qpp < refqp) {
                        qpbase = refqp;
                        continue;
                    }
                } 

                Ipp32s qpl = qpbase + layer - 1;
                Ipp32s qpn = qp;

                if (qpl > qp) // 07/04/16
                    qpn = qpl;
                else if (qf < 1 && qp <= qp0 && qpl < qp) {
                    qpn = IPP_MAX(qpl, qpmin + 1);
                }
                BRC_CLIP(qpn, mQuantMin, mQuantMax); // 08/04
                //BRC_CLIP(qp, qp0 - 1, qp0 + 3);

                if (qpn < qp && qpmin == 0) {
                    qp = qpn;
                
                    for (;;) { // check framesinParallel frames as above ???

                        predbits = PredictFrameSize(video, frames, qp, &refFrameData);
                        brc_fprintf(" %d %d \n", qp, predbits);

                        maxbits = mPredBufFulness >> 1;

                        if ((layer > 1) || qp >= refFrameData.qp)
                            if (maxbits > mHRD.bufSize >> 2)
                                maxbits = mHRD.bufSize >> 2;

                        if (predbits > maxbits && qp < mQuantMax) { // frame is predicted to exceed half buf fullness
                            qp++;
                            qpmin = qp;
                        } else if ((Ipp32s)(mPredBufFulness + mBitsDesiredFrame) - predbits > (Ipp32s)mHRD.bufSize) {
                            if (qp > qpmin + 1 && qp > qp_)
                                qp--;
                            else
                                break;
                        } else
                            break;
                    }
                } else
                    qp = qpn;

                brc_fprintf("\n %d %d %.3f %.3f ", forecastBits, qp, mQstepBase, qf);

                break;
            }


            if (layer > 1) {
#ifdef AMT_MAX_FRAME_SIZE
                Ipp32s qpLayer_1 = mPrevQpLayerSet[layer - 1];
#else
                Ipp32s qpLayer_1 = mPrevQpLayer[layer - 1];
#endif
                if (qp < qpLayer_1)
                    qp  = qpLayer_1;
            } else
                BRC_CLIP(qp, qp0 - 2, qp);

            if (video->enableLTR ) {
                Ipp32s QP0 = qp;

                mfxI32 qpMin = 1;

                Ipp32s mRawFrameSizeInPixs = getRawFrameSizeInPel(mParams.width, mParams.height, mParams.chromaFormat);
                mfxU16 type = (frames[0]->m_picCodeType & MFX_FRAMETYPE_IDR) ? MFX_FRAMETYPE_IDR : ((frames[0]->m_picCodeType & MFX_FRAMETYPE_I) ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P);

                if (!mDynamicInit) {
                    mDynamicInit = 1;
                }
                else if (type == MFX_FRAMETYPE_I && frames[0]->m_ltrConfidenceLevel > 0) {
                    qpMin = qp;
                    if (frames[0]->m_RsCs > 10.0 && LastICmplx > 1.0 && LastIQpAct && LastIFrameSize) {
                        mfxF64 sc = frames[0]->m_SC;
                        mfxF64 targetFrameSize = 0;
                        mfxF64 frac = (video->GopRefDist == 1) ? 0.25 : 0.30;
                        mfxI64 bufLow = mHRD.bufSize * frac;
                        targetFrameSize = mHRD.bufFullness - bufLow;
                        if (targetFrameSize > mMaxFrameSizeInBitsI)
                            targetFrameSize = mMaxFrameSizeInBitsI;
                        qpMin = compute_new_qp_intra(targetFrameSize, mRawFrameSizeInPixs, sc, LastIFrameSize, LastICmplx, LastIQpAct);
                    }

                    minQ = QP2Qstep(qpMin, 0);

                    Ipp32s minDQP = qpMin - qp;
                    qp = estimateNewIntraQP(video->GopRefDist, frames[0]->m_ltrConfidenceLevel, minDQP, QP0);

                    QP0 = (Ipp32s)Qstep2QP(minQ, mQuantOffset);
                }
            }  //if (video->enableLTR)

            qp = IPP_MAX(qp, Qstep2QP(minQ, mQuantOffset));

            BRC_CLIP(qp, mMinQp, mQuantMax);
            mMinQp = mQuantMin;
            UpdateMiniGopData(frames[0], qp);

            Ipp32s predbits_new = PredictFrameSize(video, frames, qp, &refFrameData);

            frames[0]->m_cmplx = refFrameData.cmplx;

            mPredBufFulness = predbufFul;

            mRealPredFullness -= predbits_new - mHRD.inputBitsPerFrame;
            mPredBufFulness -= predbits_new - mBitsDesiredFrame;
            frames[0]->m_predBits = predbits_new;
#ifdef AMT_MAX_FRAME_SIZE
            if (mPrevQpLayer[layer] <= 0)
#endif
                mPrevQpLayer[layer] = qp;

            mPrevQpLayerSet[layer] = qp;

            brc_fprintf("\n # %d %d %d %d %f %d f %d\n", frames[0]->m_encOrder, predbits, predbits_new, mPredBufFulness, frames[0]->m_cmplx, qp, frames[0]->m_secondFieldFlag ? 1 : 0);

        }
        else // AVBR
        {

            Ipp32s bufsize = mParams.width * mParams.height * mParams.bitDepthLuma;
            //Ipp64f maxQstep = QP2Qstep(51 + mQuantOffset, mQuantOffset);
            Ipp64f maxQstep = QSTEP[51 + mQuantOffset];
            if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV420)
                bufsize += bufsize >> 1;
            else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV422)
                bufsize += bufsize;
            else if (mParams.chromaFormat == MFX_CHROMAFORMAT_YUV444)
                bufsize += bufsize << 1;

            Ipp64s totdev;
            Ipp64f estBits = 0;
            Ipp64f cmpl = pow(avCmplx, BRC_RATE_CMPLX_EXPONENT); // use frames[0]->m_CmplxQstep (^0.4) ???

            for (i = 0; i < len; i++) {
                Ipp32s layer = (frames[i]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[i]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[i]->m_pyramidLayer)));
                Ipp64f qstep = q * brc_qstep_factor[layer];
                BRC_CLIP(qstep, minQstep, maxQstep);
                if (layer == 0) 
                    layer = 1;
                estBits += mEstCoeff[layer] * cmpl / (mEstCnt[layer] * qstep);
            }

            brc_fprintf("%d: l-%d %d q %.3f %.3f %.3f %.3f \n", frames[0]->m_encOrder, layer, len, q, cmpl, frames[0]->m_stats[mLowres]->m_avgBestSatd, frames[0]->m_stats[mLowres]->m_avgIntraSatd);

            if (frames[0]->m_encOrder <= video->GopRefDist)
                totdev = mTotalDeviation + 0.1*(estBits - targetBits);
            else
                totdev = mTotalDeviation + 0.5*(estBits - targetBits);

            //Ipp64f devtopLim = bufsize*0.1;
            //if (devtopLim < mTotalDeviation - targetBits*0.5)
            //    devtopLim = mTotalDeviation - targetBits*0.5;
            //Ipp64f devbotLim = -bufsize*0.1;
            //if (devbotLim > mTotalDeviation + targetBits*0.5)
            //    devbotLim = mTotalDeviation + targetBits*0.5;

            //brc_fprintf("dev %d %d %Id %Id %d %d \n", frames[0]->m_encOrder, bufsize, mTotalDeviation, totdev, (int)devtopLim, (int)devbotLim);

            Ipp64f qf = 1;
            //if (totdev > devtopLim)
            //    qf = totdev / devtopLim;
            //else if (totdev < devbotLim)
            //    qf = devbotLim / totdev;

            //if (frames[0]->m_encOrder > video->GopRefDist) {
            //    if (mTotalDeviation > bufsize) {
            //        qf1 = mTotalDeviation / (Ipp64f)bufsize;
            //        if (qf > 1) {
            //            if (qf1 > qf)
            //                qf = qf1;
            //        } else {
            //            qf *= qf1;
            //            if (qf < 1)
            //                qf = 1;
            //        }
            //    } else if (mTotalDeviation < -bufsize) {
            //        qf1 = (Ipp64f)(-bufsize) / mTotalDeviation;
            //        if (qf < 1) {
            //            if (qf1 < qf)
            //                qf = qf1;
            //        } else {
            //            qf *= qf1;
            //            if (qf > 1)
            //                qf = 1;
            //        }
            //    }
            //}

            //brc_fprintf("%d %.3f %.3f %.3f %.3f \n", frames[0]->m_encOrder, qf, avCmplx, mTotMeanComplx, q);

            //if (qf < 1) {
            //    Ipp64f w = 0.5 * avCmplx / mTotMeanComplx;
            //    if (w > 1.)
            //        w = 1.;
            //    qf = w*qf + (1 - w);
            //}

            //q *= qf;


            //qp = Qstep2QP(q, mQuantOffset);

            if (layer <= 1 && frames[0]->m_stats[mLowres]->m_sceneCut <= 0) {
                if (totdev > 10*mBitsDesiredFrame)
                    qf *= 1 + ((Ipp64f)totdev - 10*mBitsDesiredFrame) / (20*mBitsDesiredFrame);
                else if (totdev < -10*mBitsDesiredFrame)
                    qf *= 1 + ((Ipp64f)totdev - 10*mBitsDesiredFrame) / (20*mBitsDesiredFrame);
                BRC_CLIP(qf, 1/maxQstepChange, maxQstepChange);


                Ipp64f qc = mQstepScale0 * frames[0]->m_CmplxQstep;

                brc_fprintf("%d %.3f ", frames[0]->m_encOrder, qf);

                if (qf > 1 && q > qc) {
                    Ipp64f w = (4 * qc - q) / (4 * qc);
                    if (w > 0) 
                        qf = qf * w + 1 - w;
                } else if (qf < 1 && q < qc) {
                    Ipp64f w = (q - qc * 0.25) / q;
                    if (w > 0) 
                        qf = qf * w + 1 - w;
                }
                q *= qf;
                brc_fprintf("%.3f %.3f %.3f %.3f \n", q, qc, qf, mQstepBase);
            }


            if (layer <= 1) {
                if (mQstepBase > 0)
                    BRC_CLIP(q, mQstepBase/maxQstepChange, q);
                    //BRC_CLIP(q, mQstepBase/maxQstepChange, mQstepBase*maxQstepChange);
                mQstepBase = q;
            } 
            //else
            //    q = mQstepBase;

            Ipp32s l = (frames[0]->m_picCodeType == MFX_FRAMETYPE_I ? 0 : (frames[0]->m_picCodeType == MFX_FRAMETYPE_P ? 1 : 1 + IPP_MAX(1, frames[0]->m_pyramidLayer)));
            q *= brc_qstep_factor[l];

            q = IPP_MAX(minQ, q);

            qp = Qstep2QP(q, mQuantOffset);

            if (l > 1) {
#ifdef AMT_MAX_FRAME_SIZE
                Ipp32s qpLayer_1 = mPrevQpLayerSet[l - 1];
#else
                Ipp32s qpLayer_1 = mPrevQpLayer[l - 1];
#endif
                if (qp < qpLayer_1) {
                    qp  = qpLayer_1;
                    //mQstepBase = QP2Qstep(qp, mQuantOffset);
                }
            }
            brc_fprintf("%d %.3f %.3f %d \n", frames[0]->m_encOrder, q, qf, qp);

            if (video->enableLTR) {
                Ipp32s QP0 = qp;

                mfxI32 qpMin = 1;

                Ipp32s mRawFrameSizeInPixs = getRawFrameSizeInPel(mParams.width, mParams.height, mParams.chromaFormat);
                mfxU16 type = (frames[0]->m_picCodeType & MFX_FRAMETYPE_IDR) ? MFX_FRAMETYPE_IDR : ((frames[0]->m_picCodeType & MFX_FRAMETYPE_I) ? MFX_FRAMETYPE_I : MFX_FRAMETYPE_P);

                if (!mDynamicInit) {
                    mDynamicInit = 1;
                }
                else if (type == MFX_FRAMETYPE_I && frames[0]->m_ltrConfidenceLevel > 0) {
                    qpMin = qp;
                    if (frames[0]->m_RsCs > 10.0 && LastICmplx > 1.0 && LastIQpAct && LastIFrameSize) {
                        mfxF64 sc = frames[0]->m_SC;
                        mfxF64 targetFrameSize = 0;
                        mfxF64 frac = (video->GopRefDist == 1) ? 0.25 : 0.30;
                        if (mRCMode == MFX_RATECONTROL_CBR && video->hrdPresentFlag) {
                            mfxI64 bufLow = mHRD.bufSize * frac;
                            targetFrameSize = mHRD.bufFullness - bufLow;
                            if (targetFrameSize > mMaxFrameSizeInBitsI)
                                targetFrameSize = mMaxFrameSizeInBitsI;
                            qpMin = compute_new_qp_intra(targetFrameSize, mRawFrameSizeInPixs, sc, LastIFrameSize, LastICmplx, LastIQpAct);
                        }
                   }

                    Ipp32s minDQP = -1;
                    qpMin = qp + minDQP;
                    minQ = QP2Qstep(qpMin, 0);

                    minDQP = qpMin - qp;
                    qp = estimateNewIntraQP(video->GopRefDist, frames[0]->m_ltrConfidenceLevel, minDQP, QP0);
                    if (qp > (QP0 - 1))
                        qp = QP0 - 1;

                    if(qp < qpMin)
                        minQ = QP2Qstep(qp, 0);

                    QP0 = (Ipp32s)Qstep2QP(minQ, mQuantOffset);
                }
            }  //if (video->enableLTR)

            BRC_CLIP(qp, mQuantMin, mQuantMax);

            if (mPrevQpLayer[l] <= 0)
                mPrevQpLayer[l] = qp;

            mPrevQpLayerSet[l] = qp;
        }

        return qp - mQuantOffset;

    } else {

        Ipp32s qp = mQuantB;
        Frame* pFrame = NULL;

        if (frames && numFrames > 0)
            pFrame = frames[0];

        if (pFrame) {

            if (pFrame->m_encOrder == mRecodedFrame_encOrder)
                qp = mQuantRecoded;
            else {

                if (pFrame->m_picCodeType == MFX_FRAMETYPE_I) 
                    qp = mQuantI;
                else if (pFrame->m_picCodeType == MFX_FRAMETYPE_P) 
                    qp = mQuantP;
                else { 
                    if (video->BiPyramidLayers > 1) {
                        qp = mQuantB + pFrame->m_pyramidLayer - 1;
                        BRC_CLIP(qp, 1, mQuantMax);
                    } else
                        qp = mQuantB;
                }
            }
        }

        return qp - mQuantOffset;
    }        

}

mfxStatus H265BRC::SetQP(Ipp32s qp, mfxU16 frameType)
{
    if (MFX_FRAMETYPE_B == frameType) {
        mQuantB = qp + mQuantOffset;
        BRC_CLIP(mQuantB, 1, mQuantMax);
    } else {
        mRCq = qp + mQuantOffset;
        BRC_CLIP(mRCq, 1, mQuantMax);
        mQuantI = mQuantP = mRCq;
    }
    return MFX_ERR_NONE;
}


mfxBRCStatus H265BRC::UpdateAndCheckHRD(Ipp32s frameBits, Ipp64f inputBitsPerFrame, Ipp32s recode)
{
    mfxBRCStatus ret = MFX_ERR_NONE;
    Ipp64f bufFullness;

    if (!(recode & (MFX_BRC_EXT_FRAMESKIP - 1))) { // BRC_EXT_FRAMESKIP == 16
        mHRD.prevBufFullness = mHRD.bufFullness;
        mHRD.underflowQuant = 0;
        mHRD.overflowQuant = 999;
    } else { // frame is being recoded - restore buffer state
        mHRD.bufFullness = mHRD.prevBufFullness;
    }

    mHRD.maxFrameSize = (Ipp32s)(mHRD.bufFullness - 1);
    mHRD.minFrameSize = (mRCMode != MFX_RATECONTROL_CBR ? 0 : (Ipp32s)(mHRD.bufFullness + 1 + 1 + inputBitsPerFrame - mHRD.bufSize));
    if (mHRD.minFrameSize < 0)
        mHRD.minFrameSize = 0;

    bufFullness = mHRD.bufFullness - frameBits;

    if (bufFullness < 2) {
        bufFullness = inputBitsPerFrame;
        ret = MFX_BRC_ERR_BIG_FRAME;
        if (bufFullness > (Ipp64f)mHRD.bufSize)
            bufFullness = (Ipp64f)mHRD.bufSize;
    } else {
        bufFullness += inputBitsPerFrame;
        if (bufFullness > (Ipp64f)mHRD.bufSize - 1) {
            bufFullness = (Ipp64f)mHRD.bufSize - 1;
            if (mRCMode == MFX_RATECONTROL_CBR)
                ret = MFX_BRC_ERR_SMALL_FRAME;
        }
    }
    if (MFX_ERR_NONE == ret)
        mHRD.frameNum++;
    else if ((recode & MFX_BRC_EXT_FRAMESKIP) || MFX_BRC_RECODE_EXT_PANIC == recode || MFX_BRC_RECODE_PANIC == recode) // no use in changing QP
        ret |= MFX_BRC_NOT_ENOUGH_BUFFER;

    mHRD.bufFullness = bufFullness;

    return ret;
}

#define BRC_SCENE_CHANGE_RATIO1 20.0
#define BRC_SCENE_CHANGE_RATIO2 10.0
#define BRC_RCFAP_SHORT 5

#define I_WEIGHT 1.2
#define P_WEIGHT 0.25
#define B_WEIGHT 0.2

#ifdef LOW_COMPLX_PAQ
#define BRC_BIT_LOAN \
{ \
    if (picType == MFX_FRAMETYPE_I) { \
        if (video->DeltaQpMode & AMT_DQP_PAQ) { \
            mLoanRatio = BRC_LOAN_RATIO * pow(2.0f, -1.0f * pFrame->m_stats[0]->m_avgDPAQ / 6.0f); \
        } else { \
            mLoanRatio = BRC_LOAN_RATIO; \
        } \
        if (mLoanLength) \
            bitsEncoded += mLoanLength * mLoanBitsPerFrame; \
        mLoanLength = video->GopPicSize; \
        if (mLoanLength > BRC_MAX_LOAN_LENGTH || mLoanLength == 0) \
            mLoanLength = BRC_MAX_LOAN_LENGTH; \
        Ipp32s bitsEncodedI = (Ipp32s)((Ipp64f)bitsEncoded  / (mLoanLength * mLoanRatio + 1)); \
        mLoanBitsPerFrame = (bitsEncoded - bitsEncodedI) / mLoanLength; \
        bitsEncoded = bitsEncodedI; \
    } else if (mLoanLength) { \
        bitsEncoded += mLoanBitsPerFrame; \
        mLoanLength--; \
    } \
}
#else
#define BRC_BIT_LOAN \
{ \
    if (picType == MFX_FRAMETYPE_I) { \
        if (mLoanLength) \
            bitsEncoded += mLoanLength * mLoanBitsPerFrame; \
        mLoanLength = video->GopPicSize; \
        if (mLoanLength > BRC_MAX_LOAN_LENGTH || mLoanLength == 0) \
            mLoanLength = BRC_MAX_LOAN_LENGTH; \
        Ipp32s bitsEncodedI = (Ipp32s)((Ipp64f)bitsEncoded  / (mLoanLength * BRC_LOAN_RATIO + 1)); \
        mLoanBitsPerFrame = (bitsEncoded - bitsEncodedI) / mLoanLength; \
        bitsEncoded = bitsEncodedI; \
    } else if (mLoanLength) { \
        bitsEncoded += mLoanBitsPerFrame; \
        mLoanLength--; \
    } \
}
#endif


//#define BRC_BIT_LOAN_P \
//{ \
//    if (picType == MFX_FRAMETYPE_P) { \
//        if (mLoanLengthP) \
//            bitsEncoded += mLoanLengthP * mLoanBitsPerFrameP; \
//        mLoanLengthP = video->GopRefDist; \
//        if (mLoanLength > 16 || mLoanLength == 0) \
//            mLoanLength = 16; \
//        Ipp32s bitsEncodedP = (Ipp32s)((Ipp64f)bitsEncoded  / (mLoanLengthP * 0.25 + 1)); \
//        mLoanBitsPerFrameP = (bitsEncoded - bitsEncodedP) / mLoanLengthP; \
//        bitsEncoded = bitsEncodedP; \
//    } else if (mLoanLengthP && picType != MFX_FRAMETYPE_I) { \
//        bitsEncoded += mLoanBitsPerFrameP; \
//        mLoanLengthP--; \
//    } \
//}



mfxBRCStatus H265BRC::PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, Frame *pFrame, Ipp32s totalFrameBits, Ipp32s overheadBits, Ipp32s repack)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    if (mBitrate == 0)
        return Sts;
    if (!pFrame)
        return MFX_ERR_NULL_PTR;

    Ipp32s bitsEncoded = totalFrameBits - overheadBits;
    Ipp64f e2pe;
    Ipp32s qp, qpprev;
    Ipp32u prevFrameType = mPicType;
    Ipp32u picType = pFrame->m_picCodeType;

    mPoc = pFrame->m_poc;

    sliceQpY += mQuantOffset;

    Ipp32s layer = (picType == MFX_FRAMETYPE_I ? 0 : (picType == MFX_FRAMETYPE_P ?  1 : 1 + IPP_MAX(1, pFrame->m_pyramidLayer))); // should be 0 for I, 1 for P, etc. !!!
    Ipp64f qstep = QP2Qstep(sliceQpY, mQuantOffset);

#ifdef AMT_MAX_FRAME_SIZE_DEBUG
    if(video && video->MaxFrameSizeInBits && repack) {
        if(totalFrameBits < (int) ((mMaxFrameSizeInBits)? mMaxFrameSizeInBits : video->MaxFrameSizeInBits)) {
            printf("\n RPCK FIX  SIZE: %d layer %d Act %d Max %d Q %lf\n", pFrame->m_encOrder, layer, totalFrameBits, (int)((mMaxFrameSizeInBits) ? mMaxFrameSizeInBits : video->MaxFrameSizeInBits), qstep);
        } else {
            printf("\n ---- MIS SIZE: %d layer %d Act %d Max %d Q %lf\n", pFrame->m_encOrder, layer, totalFrameBits, (int)((mMaxFrameSizeInBits) ? mMaxFrameSizeInBits : video->MaxFrameSizeInBits), qstep);
        }
    } else if(video->MaxFrameSizeInBits && totalFrameBits > (int)((mMaxFrameSizeInBits) ? mMaxFrameSizeInBits : video->MaxFrameSizeInBits)) {
        printf("\n OVER MAX  SIZE: %d layer %d Act %d Max %d Q %lf\n", pFrame->m_encOrder, layer, totalFrameBits, (int)((mMaxFrameSizeInBits) ? mMaxFrameSizeInBits : video->MaxFrameSizeInBits), qstep);
    } else {
        brc_fprintf("\n ---- ---- SIZE: %d layer %d Act %d Max %d Q %lf\n", pFrame->m_encOrder, layer, totalFrameBits, (int)((mMaxFrameSizeInBits) ? mMaxFrameSizeInBits : video->MaxFrameSizeInBits), qstep);
    }
#endif

    // AMT_LTR
    if (picType == MFX_FRAMETYPE_I) {
        LastIFrameSize = totalFrameBits;
        LastICmplx = pFrame->m_SC;
        LastIQpAct = pFrame->m_sliceQpY;
    }

    if (video && video->AnalyzeCmplx) {

        if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {

            Sts = UpdateAndCheckHRD(totalFrameBits,  mHRD.inputBitsPerFrame, repack);

            // stuff bits right away in case of overflow
            // PostPackFrame is supposed to be called again after bit stuffing, hence don't update any stats here
            if (Sts & MFX_BRC_ERR_SMALL_FRAME) {

                brc_fprintf("OVER %d %d %d \n", pFrame->m_encOrder, totalFrameBits, (int)mHRD.prevBufFullness);

                Sts |= MFX_BRC_NOT_ENOUGH_BUFFER;
                return Sts;
            }

            mTotPrevCmplx = mTotComplxCnt > 0 ? mTotAvComplx  / mTotComplxCnt : 0;


            if (layer == 1) { // 08/04/16
                Ipp32s refQp = -1;
                if (pFrame->m_refPicList[0].m_refFramesCount > 0)
                    refQp = pFrame->m_refPicList[0].m_refFrames[0]->m_sliceQpY;
                if (sliceQpY < refQp) {
                    mPrevCmplxIP = pFrame->m_stats[mLowres]->m_avgIntraSatd;
                    mPrevQstepIP = qstep;
                    mPrevBitsIP = bitsEncoded;
                    mPrevQpIP = sliceQpY;
                    mSceneNum = pFrame->m_sceneOrder;
                }
            }

            mPrevCmplxLayer[layer] = pFrame->m_cmplx;
            mPrevQstepLayer[layer] = qstep;
            mPrevBitsLayer[layer] = bitsEncoded;
            mPrevQpLayer[layer] = sliceQpY;

            if (Sts & MFX_BRC_ERR_BIG_FRAME) { // assume the recode request will not be ignored
                mHRD.bufFullness = mHRD.prevBufFullness;

                mTotAvComplx  = pFrame->m_totAvComplx;
                mTotComplxCnt = pFrame->m_totComplxCnt;
                mComplxSum  = pFrame->m_complxSum;

                brc_fprintf("UNDR %d %d %.1f %.1f %.3f -> ", sliceQpY , pFrame->m_qpBase, mCmplxRate, mTotTarget, mQstepScale);

                if (sliceQpY != pFrame->m_qpBase) {
                    Ipp64f qbase = QP2Qstep(pFrame->m_qpBase, mQuantOffset);
                    Ipp64f w = sliceQpY > pFrame->m_qpBase ? qstep / qbase : (qbase / qstep - 1) * 0.3 + 1;
                    mCmplxRate += w * bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                    mTotTarget += w * mBitsDesiredFrame;
                } else {
                    mCmplxRate += bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                    mTotTarget += mBitsDesiredFrame;
                }
                mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);
                mQstepScale *= bitsEncoded / (mHRD.bufFullness / 2); // ???
                brc_fprintf(" %.1f %.1f %.3f \n", mCmplxRate, mTotTarget, mQstepScale);

                mPredBufFulness = (int)mHRD.prevBufFullness;
                mRealPredFullness = (int)mHRD.prevBufFullness;

                Ipp64f minqstep = pFrame->m_CmplxQstep * mQstepScale;
                mMinQp = Qstep2QP(minqstep, mQuantOffset);

                if (mMinQp < sliceQpY + 1)
                    mMinQp = sliceQpY + 1;

                if (mMinQp > mQuantMax)
                    mMinQp = mQuantMax;

                brc_fprintf("UNDER %d %d %d %d %d %.3f %d \n", pFrame->m_encOrder, totalFrameBits, pFrame->m_predBits, (int)mHRD.prevBufFullness, mPredBufFulness, mQstepScale, mMinQp);

                return Sts;
            }
#ifdef AMT_MAX_FRAME_SIZE
            if(video->RepackForMaxFrameSize && video->MaxFrameSizeInBits && mMaxFrameSizeInBits && totalFrameBits > (Ipp32s) pFrame->m_maxFrameSizeInBitsSet && pFrame->m_sliceQpY < 51) Sts = MFX_BRC_ERR_MAX_FRAME;
            UpdateMinQForMaxFrameSize(pFrame->m_maxFrameSizeInBitsSet, video, pFrame, totalFrameBits, layer, Sts);
            if (Sts & MFX_BRC_ERR_MAX_FRAME) return Sts;
#endif

            mPredBufFulness += pFrame->m_predBits - totalFrameBits;
            mRealPredFullness += pFrame->m_predBits - totalFrameBits;
            BRC_CLIP(mRealPredFullness, 0, (Ipp32s)mHRD.bufSize);

            brc_fprintf("L%d %d %d %d %d %d %d %d %f \n", layer, pFrame->m_encOrder, sliceQpY, totalFrameBits, bitsEncoded, pFrame->m_predBits, (int)mHRD.bufFullness, mPredBufFulness, mPrevCmplxLayer[layer]);

            BRC_BIT_LOAN;

//            BRC_BIT_LOAN_P;

            if (mRCMode == MFX_RATECONTROL_CBR) {
                Ipp64f fmax = (Ipp64f)mHRD.bufSize * 0.9;
                if (mHRD.bufFullness > fmax) {
                    Ipp64f w = (mHRD.bufFullness / fmax - 1) / 8.; // 16.
                    mCmplxRate *= 1. - w;
                }
            }

            Ipp64f decay = 1 - 0.1*(Ipp64f)mBitsDesiredFrame / mHRD.bufSize;
            mCmplxRate *= decay;
            mTotTarget *= decay;

            if (sliceQpY != pFrame->m_qpBase) {
                Ipp64f qbase = QP2Qstep(pFrame->m_qpBase, mQuantOffset);
                Ipp64f w = sliceQpY > pFrame->m_qpBase ? qstep / qbase : (qbase / qstep - 1) * 0.3 + 1;
                mCmplxRate += w * bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                mTotTarget += w * mBitsDesiredFrame;
            } else {
                mCmplxRate += bitsEncoded * pow(qstep / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
                mTotTarget += mBitsDesiredFrame;
            }


            {
                Ipp64f w = pFrame->m_encOrder / (Ipp64f)100;
                if (w > 1)
                    w = 1;
                mQstepScale = (1-w)*mQstepScale + w*pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);
            }


            mBitsEncodedTotal += totalFrameBits;
            mBitsDesiredTotal += mBitsDesiredFrame;

            mTotalDeviation += bitsEncoded - mBitsDesiredFrame;

            brc_fprintf("pp %d %d %d %f %f %f %f \n", pFrame->m_encOrder, bitsEncoded, mBitsDesiredFrame, mCmplxRate, mTotTarget, mQstepScale, pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT));

            mEncOrderCoded = pFrame->m_encOrder;
            UpdateMiniGopData(pFrame, sliceQpY);

        } else { // AVBR
#ifdef AMT_MAX_FRAME_SIZE
            if(video->RepackForMaxFrameSize && video->MaxFrameSizeInBits && mMaxFrameSizeInBits && totalFrameBits > (Ipp32s)pFrame->m_maxFrameSizeInBitsSet && pFrame->m_sliceQpY < 51) Sts = MFX_BRC_ERR_MAX_FRAME;
            UpdateMinQForMaxFrameSize(pFrame->m_maxFrameSizeInBitsSet, video, pFrame, totalFrameBits, layer, Sts);
            if (Sts & MFX_BRC_ERR_MAX_FRAME) return Sts;
#endif

            mPrevQpLayer[layer] = sliceQpY;

            Ipp64f decay = layer ? 0.5 : 0.25;

            mTotPrevCmplx = mTotComplxCnt > 0 ? mTotAvComplx  / mTotComplxCnt : 0;
            //mTotPrevCmplx = pFrame->m_avCmplx;
            //if (mTotPrevCmplx < BRC_MIN_CMPLX)
            //    mTotPrevCmplx = BRC_MIN_CMPLX;


            Ipp64f cmpl = pow(pFrame->m_avCmplx, BRC_RATE_CMPLX_EXPONENT);

            if (cmpl > pow(BRC_MIN_CMPLX, BRC_RATE_CMPLX_EXPONENT)) {
                Ipp64f coefn = bitsEncoded * qstep / cmpl;
                mEstCoeff[layer] *= decay;
                mEstCoeff[layer] += coefn;
                mEstCnt[layer] *= decay;
                mEstCnt[layer] += 1.;
            }

            BRC_BIT_LOAN

            Ipp32s qp = sliceQpY - layer;
            BRC_CLIP(qp, 1, mQuantMax);
            Ipp64f qs = QP2Qstep(qp, mQuantOffset);

            mCmplxRate += bitsEncoded * pow(qs / pFrame->m_CmplxQstep, 1./BRC_QSTEP_SCALE_EXPONENT);
            mTotTarget += mBitsDesiredFrame;

            mBitsEncodedTotal += totalFrameBits;
            mBitsDesiredTotal += mBitsDesiredFrame;
            mTotalDeviation += bitsEncoded - mBitsDesiredFrame;

            if (mTotalDeviation > 30*mBitsDesiredFrame)
                mCmplxRate *= 1.001;
            else if (mTotalDeviation < -30*mBitsDesiredFrame)
                mCmplxRate *= 0.999;

            mEncOrderCoded = pFrame->m_encOrder;

            mQstepScale = pow(mCmplxRate / mTotTarget, BRC_QSTEP_SCALE_EXPONENT);

            brc_fprintf("L%d %d %d %d %d %.1f %d %.3f %.3f %Id \n\n", layer, pFrame->m_encOrder, qp, bitsEncoded, mBitsDesiredFrame, mCmplxRate, (int)mTotTarget, mQstepScale, mBitsEncodedTotal/mTotTarget, mTotalDeviation);

        }

        return Sts;
    }

    if (!repack && mQuantUpdated <= 0) { // BRC reported buffer over/underflow but the application ignored it
        mQuantI = mQuantIprev;
        mQuantP = mQuantPprev;
        mQuantB = mQuantBprev;
        mRecode |= 2;
        mQp = mRCq;
        UpdateQuant(mBitsEncoded, totalFrameBits, layer);
    }

    mQuantIprev = mQuantI;
    mQuantPprev = mQuantP;
    mQuantBprev = mQuantB;

    mBitsEncoded = bitsEncoded;

    if (mSceneChange)
        if (mQuantUpdated == 1 && mPoc > mSChPoc + 1)
            mSceneChange &= ~16;

    Ipp64f buffullness = 1.e12; // a big number
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        buffullness = repack ? mHRD.prevBufFullness : mHRD.bufFullness;
        Sts = UpdateAndCheckHRD(totalFrameBits, mHRD.inputBitsPerFrame, repack);
    }

    qpprev = qp = mQp = sliceQpY;

    Ipp64f fa_short0 = mRCfa_short;
    mRCfa_short += (bitsEncoded - mRCfa_short) / BRC_RCFAP_SHORT;

    {
        qstep = QP2Qstep(qp, mQuantOffset);
        Ipp64f qstep_prev = QP2Qstep(mQPprev, mQuantOffset);
        Ipp64f frameFactor = 1.0;
        Ipp64f targetFrameSize = IPP_MAX((Ipp64f)mBitsDesiredFrame, mRCfa);
        if (picType & MFX_FRAMETYPE_I)
            frameFactor = 1.5;

        e2pe = bitsEncoded * sqrt(qstep) / (mBitsEncodedPrev * sqrt(qstep_prev));

        Ipp64f maxFrameSize;
        maxFrameSize = 2.5/9. * buffullness + 5./9. * targetFrameSize;
        BRC_CLIP(maxFrameSize, targetFrameSize, BRC_SCENE_CHANGE_RATIO2 * targetFrameSize * frameFactor);

        Ipp64f famax = 1./9. * buffullness + 8./9. * mRCfa;

        Ipp32s maxqp = mQuantMax;
        if (mRCMode == MFX_RATECONTROL_CBR) {
            maxqp = IPP_MIN(maxqp, mHRD.overflowQuant - 1);
        }

        if (bitsEncoded >  maxFrameSize && qp < maxqp) {
            Ipp64f targetSizeScaled = maxFrameSize * 0.8;
            Ipp64f qstepnew = qstep * pow(bitsEncoded / targetSizeScaled, 0.9);
            Ipp32s qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
              qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            if (qpnew > qp) {
                mQp = mRCq = mQuantI = mQuantP = qpnew;
                if (picType & MFX_FRAMETYPE_B)
                    mQuantB = qpnew;
                else {
                    SetQuantB();
                }

                mRCfa_short = fa_short0;

                if (e2pe > BRC_SCENE_CHANGE_RATIO1) { // scene change, resetting BRC statistics
                  mRCfa = mBitsDesiredFrame;
                  mRCqa = 1./qpnew;
                  mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;
                  mSceneChange |= 1;
                  if (picType != MFX_FRAMETYPE_B) {
                      mSceneChange |= 16;
                      mSChPoc = mPoc;
                  }
                  mRCfa_short = mBitsDesiredFrame;
                }
                if (mRecode) {
                    mQuantUpdated = 0;
                    mHRD.frameNum--;
                    mMinQp = qp;
                    mRecodedFrame_encOrder = pFrame->m_encOrder;
                    mQuantRecoded = qpnew;
                    brc_fprintf("recode1 %d %d %d %d \n", pFrame->m_encOrder, qpnew, bitsEncoded ,  (int)maxFrameSize);
                    return MFX_BRC_ERR_BIG_FRAME;
                }
            }
        }

        if (mRCfa_short > famax && (!repack) && qp < maxqp) {

            Ipp64f qstepnew = qstep * mRCfa_short / (famax * 0.8);
            Ipp32s qpnew = Qstep2QP(qstepnew, mQuantOffset);
            if (qpnew == qp)
                qpnew++;
            BRC_CLIP(qpnew, 1, maxqp);

            mRCfa = mBitsDesiredFrame;
            mRCqa = 1./qpnew;
            mQp = mRCq = mQuantI = mQuantP = mQuantB = mQuantPrev = qpnew;

            mRCfa_short = mBitsDesiredFrame;

            if (mRecode) {
                mQuantUpdated = 0;
                mHRD.frameNum--;
                mMinQp = qp;
                mRecodedFrame_encOrder = pFrame->m_encOrder;
                mQuantRecoded = qpnew;
                brc_fprintf("recode2 %d %d \n", pFrame->m_encOrder, qpnew);
                return MFX_BRC_ERR_BIG_FRAME;
            }
        }
    }

    mPicType = picType;

    Ipp64f fa = mRCfa;
    bool oldScene = false;
    if ((mSceneChange & 16) && (mPoc < mSChPoc) && (mBitsEncoded * (0.9 * BRC_SCENE_CHANGE_RATIO1) < (Ipp64f)mBitsEncodedP) && (Ipp64f)mBitsEncoded < 1.5*fa)
        oldScene = true;

    if (Sts != MFX_BRC_OK && mRecode) {
        Sts = UpdateQuantHRD(totalFrameBits, Sts, overheadBits, layer, pFrame->m_encOrder == mRecodedFrame_encOrder);
        mRecodedFrame_encOrder = pFrame->m_encOrder;
        mQuantUpdated = 0;
        mPicType = prevFrameType;
        mRCfa_short = fa_short0;
    } else {
        // check minCR ??? { ... }

        if (mQuantUpdated == 0 && 1./qp < mRCqa)
            mRCqa += (1. / qp - mRCqa) / 16;
        else if (mQuantUpdated == 0)
            mRCqa += (1. / qp - mRCqa) / (mRCqap > 25 ? 25 : mRCqap);
        else
            mRCqa += (1. / qp - mRCqa) / mRCqap;

        BRC_CLIP(mRCqa, 1./mQuantMax , 1./1.);

        if (repack != MFX_BRC_RECODE_PANIC && repack != MFX_BRC_RECODE_EXT_PANIC && !oldScene) {
            mQPprev = qp;
            mBitsEncodedPrev = mBitsEncoded;

            Sts = UpdateQuant(bitsEncoded, totalFrameBits,  layer, pFrame->m_encOrder == mRecodedFrame_encOrder);

            if (mPicType != MFX_FRAMETYPE_B) {
                mQuantPrev = mQuantP;
                mBitsEncodedP = mBitsEncoded;
            }

            mQuantP = mQuantI = mRCq;
        }
        mQuantUpdated = 1;
        //    mMaxBitsPerPic = mMaxBitsPerPicNot0;

        if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
            mHRD.underflowQuant = 0;
            mHRD.overflowQuant = 999;
            mBF += (mfxI64)mMaxBitrate * (mfxI64)mParams.frameRateExtD;
            mBF -= ((mfxI64)totalFrameBits >> 3) * mParams.frameRateExtN;
            if ((MFX_RATECONTROL_VBR == mRCMode) && (mBF > (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN))
                mBF = (mfxI64)mParams.HRDBufferSizeBytes * mParams.frameRateExtN;
        }
        mMinQp = -1;
        mMaxQp = 999;
    }

#ifdef PRINT_BRC_STATS
    if (Sts & 1)
        brc_fprintf("underflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
    else if (Sts & 4)
        brc_fprintf("overflow %d %d %d \n", pFrame->m_encOrder, mQuantRecoded, Sts);
#endif

    return Sts;
};

mfxBRCStatus H265BRC::UpdateQuant(Ipp32s bEncoded, Ipp32s totalPicBits, Ipp32s layer, Ipp32s recode)
{
    mfxBRCStatus Sts = MFX_ERR_NONE;
    Ipp64f  bo, qs, dq;
    Ipp32s  quant;
    Ipp32u bitsPerPic = (Ipp32u)mBitsDesiredFrame;
    mfxI64 totalBitsDeviation;

    if (recode)
        quant = mQuantRecoded;
    else
        //quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : ((mPicType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));
        quant = mQp;

    if (mRecode & 2) {
        mRCfa = bitsPerPic;
        mRCqa = mRCqa0;
        mRecode &= ~2;
    }

    mBitsEncodedTotal += totalPicBits;
    mBitsDesiredTotal += bitsPerPic;
    totalBitsDeviation = mBitsEncodedTotal - mBitsDesiredTotal;

    //if (mParams.HRDBufferSizeBytes > 0) {
    if (mRCMode == MFX_RATECONTROL_VBR) {
        mfxI64 targetFullness = IPP_MIN(mParams.HRDInitialDelayBytes << 3, (Ipp32s)mHRD.bufSize / 2);
        mfxI64 minTargetFullness = IPP_MIN(mHRD.bufSize / 2, mBitrate * 2); // half bufsize or 2 sec
        if (targetFullness < minTargetFullness)
            targetFullness = minTargetFullness;
        mfxI64 bufferDeviation = targetFullness - (Ipp64s)mHRD.bufFullness;
        if (bufferDeviation > totalBitsDeviation)
            totalBitsDeviation = bufferDeviation;
    }

    if (mPicType != MFX_FRAMETYPE_I || mRCMode == MFX_RATECONTROL_CBR || mQuantUpdated == 0)
        mRCfa += (bEncoded - mRCfa) / mRCfap;
    SetQuantB();
    if (mQuantUpdated == 0)
        if (mQuantB < quant)
            mQuantB = quant;
    qs = pow(bitsPerPic / mRCfa, 2.0);
    dq = mRCqa * qs;

    Ipp32s bap = mRCbap;
    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        Ipp64f bfRatio = mHRD.bufFullness / mBitsDesiredFrame;
        if (totalBitsDeviation > 0) {
            bap = (Ipp32s)bfRatio*3;
            bap = IPP_MAX(bap, 10);
            BRC_CLIP(bap, mRCbap/10, mRCbap);
        }
    }
    bo = (Ipp64f)totalBitsDeviation / bap / mBitsDesiredFrame;
    BRC_CLIP(bo, -1.0, 1.0);

    dq = dq + (1./mQuantMax - dq) * bo;
    BRC_CLIP(dq, 1./mQuantMax, 1./mQuantMin);
    quant = (Ipp32s) (1. / dq + 0.5);

    if (quant >= mRCq + 5)
        quant = mRCq + 3;
    else if (quant >= mRCq + 3)
        quant = mRCq + 2;
    else if (quant > mRCq + 1)
        quant = mRCq + 1;
    else if (quant <= mRCq - 5)
        quant = mRCq - 3;
    else if (quant <= mRCq - 3)
        quant = mRCq - 2;
    else if (quant < mRCq - 1)
        quant = mRCq - 1;

    mRCq = quant;

    if (mRCMode == MFX_RATECONTROL_CBR || mRCMode == MFX_RATECONTROL_VBR) {
        Ipp64f qstep = QP2Qstep(quant, mQuantOffset);
        Ipp64f fullnessThreshold = MIN(bitsPerPic * 12, mHRD.bufSize*3/16);
        qs = 1.0;
        if (bEncoded > mHRD.bufFullness && mPicType != MFX_FRAMETYPE_I)
            qs = (Ipp64f)bEncoded / (mHRD.bufFullness);

        if (mHRD.bufFullness < fullnessThreshold && (Ipp32u)totalPicBits > bitsPerPic)
            qs *= sqrt((Ipp64f)fullnessThreshold * 1.3 / mHRD.bufFullness); // ??? is often useless (quant == quant_old)

        if (qs > 1.0) {
            qstep *= qs;
            quant = Qstep2QP(qstep, mQuantOffset);
            if (mRCq == quant)
                quant++;

            BRC_CLIP(quant, 1, mQuantMax);

            mQuantB = ((quant + quant) * 563 >> 10) + 1;
            BRC_CLIP(mQuantB, 1, mQuantMax);
            mRCq = quant;
        }
    }

    return Sts;
}

mfxBRCStatus H265BRC::UpdateQuantHRD(Ipp32s totalFrameBits, mfxBRCStatus sts, Ipp32s overheadBits, Ipp32s layer, Ipp32s recode)
{
    Ipp32s quant, quant_prev;
    Ipp32s wantedBits = (sts == MFX_BRC_ERR_BIG_FRAME ? mHRD.maxFrameSize * 3 / 4 : mHRD.minFrameSize * 5 / 4);
    Ipp32s bEncoded = totalFrameBits - overheadBits;
    Ipp64f qstep, qstep_new;

    wantedBits -= overheadBits;
    if (wantedBits <= 0) // possible only if BRC_ERR_BIG_FRAME
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    if (recode)
        quant_prev = quant = mQuantRecoded;
    else
        //quant_prev = quant = (mPicType == MFX_FRAMETYPE_I) ? mQuantI : ((mPicType == MFX_FRAMETYPE_P) ? mQuantP : (layer > 0 ? mQuantB + layer - 1 : mQuantB));
        quant_prev = quant = mQp;

    if (sts & MFX_BRC_ERR_BIG_FRAME)
        mHRD.underflowQuant = quant;
    else if (sts & MFX_BRC_ERR_SMALL_FRAME)
        mHRD.overflowQuant = quant;

    qstep = QP2Qstep(quant, mQuantOffset);
    qstep_new = qstep * sqrt((Ipp64f)bEncoded / wantedBits);
//    qstep_new = qstep * sqrt(sqrt((Ipp64f)bEncoded / wantedBits));
    quant = Qstep2QP(qstep_new, mQuantOffset);
    BRC_CLIP(quant, 1, mQuantMax);

    if (sts & MFX_BRC_ERR_SMALL_FRAME) // overflow
    {
        Ipp32s qpMin = IPP_MAX(mHRD.underflowQuant, mMinQp);
        if (qpMin > 0) {
            if (quant < (qpMin + quant_prev + 1) >> 1)
                quant = (qpMin + quant_prev + 1) >> 1;
        }
        if (quant > quant_prev - 1)
            quant = quant_prev - 1;
        if (quant < mHRD.underflowQuant + 1)
            quant = mHRD.underflowQuant + 1;
        if (quant < mMinQp + 1 && quant_prev > mMinQp + 1)
            quant = mMinQp + 1;
    } 
    else // underflow
    {
        if (mHRD.overflowQuant <= mQuantMax) {
            if (quant > (quant_prev + mHRD.overflowQuant + 1) >> 1)
                quant = (quant_prev + mHRD.overflowQuant + 1) >> 1;
        }
        if (quant < quant_prev + 1)
            quant = quant_prev + 1;
        if (quant > mHRD.overflowQuant - 1)
            quant = mHRD.overflowQuant - 1;
    }

   if (quant == quant_prev)
        return (sts | MFX_BRC_NOT_ENOUGH_BUFFER);

    mQuantRecoded = quant;

    return sts;
}

mfxStatus H265BRC::GetInitialCPBRemovalDelay(Ipp32u *initial_cpb_removal_delay, Ipp32s recode)
{
    Ipp32u cpb_rem_del_u32;
    mfxU64 cpb_rem_del_u64, temp1_u64, temp2_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        if (recode)
            mBF = mBFsaved;
        else
            mBFsaved = mBF;
    }

    temp1_u64 = (mfxU64)mBF * 90000;
    temp2_u64 = (mfxU64)mMaxBitrate * mParams.frameRateExtN;
    cpb_rem_del_u64 = temp1_u64 / temp2_u64;
    cpb_rem_del_u32 = (Ipp32u)cpb_rem_del_u64;

    if (MFX_RATECONTROL_VBR == mRCMode) {
        mBF = temp2_u64 * cpb_rem_del_u32 / 90000;
        temp1_u64 = (mfxU64)cpb_rem_del_u32 * mMaxBitrate;
        Ipp32u dec_buf_ful = (Ipp32u)(temp1_u64 / (90000/8));
        if (recode)
            mHRD.prevBufFullness = (Ipp64f)dec_buf_ful;
        else
            mHRD.bufFullness = (Ipp64f)dec_buf_ful;
    }

    *initial_cpb_removal_delay = cpb_rem_del_u32;
    return MFX_ERR_NONE;
}

void  H265BRC::GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits)
{
    if (minFrameSizeInBits)
      *minFrameSizeInBits = mHRD.minFrameSize;
    if (maxFrameSizeInBits)
      *maxFrameSizeInBits = mHRD.maxFrameSize;
}

#define CLIPVAL(MINVAL, MAXVAL, VAL) (IPP_MAX(MINVAL, IPP_MIN(MAXVAL, VAL)))
mfxF64 const INIT_RATE_COEFF[] = {
        1.109, 1.196, 1.225, 1.309, 1.369, 1.428, 1.490, 1.588, 1.627, 1.723, 1.800, 1.851, 1.916,
        2.043, 2.052, 2.140, 2.097, 2.096, 2.134, 2.221, 2.084, 2.153, 2.117, 2.014, 1.984, 2.006,
        1.801, 1.796, 1.682, 1.549, 1.485, 1.439, 1.248, 1.221, 1.133, 1.045, 0.990, 0.987, 0.895,
        0.921, 0.891, 0.887, 0.896, 0.925, 0.917, 0.942, 0.964, 0.997, 1.035, 1.098, 1.170, 1.275
    };
/*mfxF64 const QSTEP[52] = {
        0.630,  0.707,  0.794,  0.891,  1.000,   1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
        2.828,  3.175,  3.564,  4.000,  4.490,   5.040,   5.657,   6.350,   7.127,   8.000,   8.980,  10.079,  11.314,
    12.699, 14.254, 16.000, 17.959, 20.159,  22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
    57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070
};*/
mfxF64 const QSTEP[52] = {
    0.82,    0.93,    1.11,    1.24,     1.47,    1.65,    1.93,    2.12,    2.49,    2.80,
    3.15,    3.53,    4.10,    4.55,     5.24,    5.88,    6.44,    7.20,    8.36,    8.92,
    10.26,  11.55,    12.93,  14.81,    17.65,   19.30,   22.30,   25.46,   28.97,   33.22,
    38.50,  43.07,    50.52,  55.70,    64.34,   72.55,   82.25,   93.12,   108.95, 122.40,
    130.39,148.69,   185.05, 194.89,   243.18,  281.61,  290.58,   372.38,  378.27, 406.62,
    468.34,525.69 
};


mfxF64 const INTRA_QSTEP_COEFF  = 2.0;
mfxF64 const INTRA_MODE_BITCOST = 0.0;
mfxF64 const INTER_MODE_BITCOST = 0.0;
mfxI32 const MAX_QP_CHANGE      = 1;
mfxF64 const LOG2_64            = 3;
mfxF64 const MIN_EST_RATE       = 0.1;
mfxF64 const NORM_EST_RATE      = 100.0;

mfxF64 const MIN_RATE_COEFF_CHANGE = 0.05;
mfxF64 const MAX_RATE_COEFF_CHANGE = 20.0;



mfxStatus VMEBrc::Init(const mfxVideoParam *init, H265VideoParam &, mfxI32 )
{
    m_qpUpdateRange = 20;
    mfxU32 RegressionWindow = 20;
    static mfxF64 coeff[8]   = {1.3, 0.80, 0.7, 0.3, 0.25, 0.2, 0.15, 0.1};
    static mfxU32 RegressionWindows[8] = {2,3,6,10,20,20,20,20};

    mfxF64 fr = mfxF64(init->mfx.FrameInfo.FrameRateExtN) / init->mfx.FrameInfo.FrameRateExtD;
    m_totNumMb = init->mfx.FrameInfo.Width * init->mfx.FrameInfo.Height / 256;
    m_initTargetRate = 1000 * init->mfx.TargetKbps / fr / m_totNumMb;
    m_targetRateMin = m_initTargetRate;
    m_targetRateMax = m_initTargetRate;
    m_laData.resize(0);

    m_bPyr = true; //to do

   for (mfxU32 level = 0; level < 8; level++)
   {
   for (mfxU32 qp = 0; qp < 52; qp++)
        m_rateCoeffHistory[level][qp].Reset(m_bPyr ? RegressionWindows[level] : RegressionWindow, 100.0, 100.0 * (m_bPyr ?INIT_RATE_COEFF[qp]*coeff[level]:1));
   }

    m_framesBehind = 0;
    m_bitsBehind = 0.0;
    m_curQp = -1;
    m_curBaseQp = -1;
    m_lookAheadDep = 0;
    

    return MFX_ERR_NONE;
}

mfxStatus VMEBrc::SetFrameVMEData(const mfxExtLAFrameStatistics *pLaOut, mfxU32 width, mfxU32 height)
{
    mfxU32 resNum = 0;
    mfxU32 numLaFrames = pLaOut->NumFrame;
    mfxU32 k = height*width >> 7;

    UMC::AutomaticUMCMutex guard(m_mutex);

    while(resNum < pLaOut->NumStream) 
    {
        if (pLaOut->FrameStat[resNum*numLaFrames].Height == height &&
            pLaOut->FrameStat[resNum*numLaFrames].Width  == width) 
            break;
        resNum ++;
    }
    MFX_CHECK(resNum <  pLaOut->NumStream, MFX_ERR_UNDEFINED_BEHAVIOR);
    MFX_CHECK(pLaOut->NumFrame > 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxLAFrameInfo * pFrameData = pLaOut->FrameStat + numLaFrames*resNum;
    std::list<LaFrameData>::iterator it = m_laData.begin();
    while (m_laData.size()>0)
    {
        it = m_laData.begin();
        if (!((*it).bNotUsed))
            break;
        m_laData.pop_front();
    }

    // some frames can be stored already
    // start of stored sequence
    it = m_laData.begin();
     while (it != m_laData.end())
    {
        if ((*it).encOrder == pFrameData[0].FrameEncodeOrder)
            break;
        ++it;
    }
    mfxU32 ind  = 0;
    
    // check stored sequence
    while ((it != m_laData.end()) && (ind < numLaFrames))
    {
        MFX_CHECK((*it).encOrder == pFrameData[ind].FrameEncodeOrder, MFX_ERR_UNDEFINED_BEHAVIOR);
        ++ind;
        ++it;
    }
    MFX_CHECK(it == m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);

    // store a new data
    for (; ind < numLaFrames; ind++)
    {
        LaFrameData data = {};

        data.encOrder  = pFrameData[ind].FrameEncodeOrder;
        data.dispOrder = pFrameData[ind].FrameDisplayOrder;
        data.interCost = pFrameData[ind].InterCost;
        data.intraCost = pFrameData[ind].IntraCost;
        data.propCost  = pFrameData[ind].DependencyCost;
        data.bframe    = (pFrameData[ind].FrameType & MFX_FRAMETYPE_B) != 0;
        data.layer     = pFrameData[ind].Layer;

        MFX_CHECK(data.intraCost, MFX_ERR_UNDEFINED_BEHAVIOR);

        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            data.estRate[qp] = ((mfxF64)pFrameData[ind].EstimatedRate[qp])/(QSTEP[qp]*k);
            if ( m_bPyr)
            {
                if (data.estRate[qp] < 0.2)
                    data.estRate[qp] = 0.2;
            }
        }
        //printf("EncOrder %d, dispOrder %d, interCost %d,  intraCost %d, data.propCost %d, frametype %d\n", 
        //    data.encOrder,data.dispOrder, data.interCost, data.intraCost, data.propCost, pFrameData[ind].FrameType );
        m_laData.push_back(data);
    }
    if (m_lookAheadDep == 0)
        m_lookAheadDep = numLaFrames;
      
    //printf("VMEBrc::SetFrameVMEData m_laData[0].encOrder %d\n", pFrameData[0].FrameEncodeOrder);

    return MFX_ERR_NONE;
}

mfxU8 SelectQp(mfxF64 erate[52], mfxF64 budget, mfxU8 minQP)
{
    minQP = (minQP != 0) ? minQP : 1; //KW
    for (mfxU8 qp = minQP; qp < 52; qp++)
        if (erate[qp] < budget)
            return (erate[qp - 1] + erate[qp] < 2 * budget) ? qp - 1 : qp;
    return 51;
}


mfxF64 GetTotalRate(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxI32 baseQp)
{
    mfxF64 totalRate = 0.0;
    std::list<VMEBrc::LaFrameData>::iterator it = start;
    for (; it!=end; ++it)
        totalRate += (*it).estRateTotal[CLIPVAL(0, 51, baseQp + (*it).deltaQp)];
    return totalRate;
}



 mfxI32 SelectQp(std::list<VMEBrc::LaFrameData>::iterator start, std::list<VMEBrc::LaFrameData>::iterator end, mfxF64 budget, mfxU8 minQP)
{
    mfxF64 prevTotalRate = GetTotalRate(start,end, minQP);
    for (mfxU8 qp = minQP+1; qp < 52; qp++)
    {
        mfxF64 totalRate = GetTotalRate(start,end, qp);
        if (totalRate < budget)
            return (prevTotalRate + totalRate < 2 * budget) ? qp - 1 : qp;
        else
            prevTotalRate = totalRate;
    }
    return 51;
}

mfxU8 GetFrameTypeLetter(mfxU32 frameType)
{
    mfxU32 ref = (frameType & MFX_FRAMETYPE_REF) ? 0 : 'a' - 'A';
    if (frameType & MFX_FRAMETYPE_I)
        return mfxU8('I' + ref);
    if (frameType & MFX_FRAMETYPE_P)
        return mfxU8('P' + ref);
    if (frameType & MFX_FRAMETYPE_B)
        return mfxU8('B' + ref);
    return 'x';
}




void VMEBrc::PreEnc(mfxU32 /*frameType*/, std::vector<VmeData *> const & /*vmeData*/, mfxU32 /*curEncOrder*/)
{
}


mfxU32 VMEBrc::Report(mfxU32 frameType, mfxU32 dataLength, mfxU32 /*userDataLength*/, mfxU32 /*repack*/, mfxU32  picOrder, mfxU32 /* maxFrameSize */, mfxU32 /* qp */)
{
    frameType; // unused

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "LookAheadBrc2::Report");
    //printf("VMEBrc::Report order %d\n", picOrder);
    mfxF64 realRatePerMb = 8 * dataLength / mfxF64(m_totNumMb);

    m_framesBehind++;
    m_bitsBehind += realRatePerMb;
   
    // start of sequence
    mfxU32 numFrames = 0;
    std::list<LaFrameData>::iterator start = m_laData.begin();
    for(;start != m_laData.end(); ++start)
    {
        if ((*start).encOrder == picOrder)
            break;
    }
    for (std::list<LaFrameData>::iterator it = start; it != m_laData.end(); ++it) numFrames++;
    numFrames = IPP_MIN(numFrames, m_lookAheadDep);

     mfxF64 framesBeyond = (mfxF64)(IPP_MAX(2, numFrames) - 1); 
     m_targetRateMax = (m_initTargetRate * (m_framesBehind + m_lookAheadDep - 1) - m_bitsBehind) / framesBeyond;
     m_targetRateMin = (m_initTargetRate * (m_framesBehind + framesBeyond  ) - m_bitsBehind) / framesBeyond;


    if (start != m_laData.end())
    {
        mfxU32 level = (*start).layer < 8 ? (*start).layer : 7;
        mfxI32 curQp = (*start).qp;
        mfxF64 oldCoeff = m_rateCoeffHistory[level][curQp].GetCoeff();
        mfxF64 y = IPP_MAX(0.0, realRatePerMb);
        mfxF64 x = (*start).estRate[curQp];
        //printf("%4d)\tlevel\t%4d\tintra\t%8d\tinter\t%8d\tprop\t %8d\tqp\t%2d\tbase\t%2d\t%3d\test rate\t%5f\test rate total\t%5f\treal\t%5f\tk\t%5f\toldCoeff\t%5f \n",(*start).encOrder,level, (*start).intraCost, (*start).interCost,(*start).propCost, curQp, m_curBaseQp, (*start).deltaQp, (*start).estRate[curQp], (*start).estRateTotal[curQp],realRatePerMb, y/x*NORM_EST_RATE, oldCoeff*NORM_EST_RATE);
        mfxF64 minY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MIN_RATE_COEFF_CHANGE;
        mfxF64 maxY = NORM_EST_RATE * INIT_RATE_COEFF[curQp] * MAX_RATE_COEFF_CHANGE;
        y = CLIPVAL(minY, maxY, y / x * NORM_EST_RATE); 
        m_rateCoeffHistory[level][curQp].Add(NORM_EST_RATE, y);
        //mfxF64 ratio = m_rateCoeffHistory[curQp].GetCoeff() / oldCoeff;
        mfxF64 ratio = y / (oldCoeff*NORM_EST_RATE);
        for (mfxI32 i = -m_qpUpdateRange; i <= m_qpUpdateRange; i++)
            if (i != 0 && curQp + i >= 0 && curQp + i < 52)
            {
                mfxF64 r = ((ratio - 1.0) * (1.0 - ((mfxF64)abs(i))/((mfxF64)m_qpUpdateRange + 1.0)) + 1.0);
                m_rateCoeffHistory[level][curQp + i].Add(NORM_EST_RATE,
                    NORM_EST_RATE * m_rateCoeffHistory[level][curQp + i].GetCoeff() * r);
            }
        (*start).bNotUsed = 1;
    }
    return 0;
}
Ipp32s VMEBrc::GetQP(H265VideoParam *video, Frame *pFrame[], Ipp32s numFrames)
{
     if ((!pFrame)||(numFrames == 0))
         return 26;
     return GetQP(*video, pFrame[0], 0 ); 
 }

mfxI32 VMEBrc::GetQP(H265VideoParam &video, Frame *pFrame, mfxI32 *chromaQP )
{

    UMC::AutomaticUMCMutex guard(m_mutex);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "VMEBrc::GetQp");

    if (!m_laData.size())
        return 26;

    mfxF64 totalEstRate[52] = {}; 
    mfxU16 maxLayer = 0;

     // start of sequence
    std::list<LaFrameData>::iterator start = m_laData.begin();
    while (start != m_laData.end())
    {
        if ((*start).encOrder == pFrame->m_encOrder)
            break;
        ++start;
    }
    MFX_CHECK(start != m_laData.end(), MFX_ERR_UNDEFINED_BEHAVIOR);
    
    std::list<LaFrameData>::iterator it = start;
    mfxU32 numberOfFrames = 0;
    for(it = start;it != m_laData.end(); ++it) 
        numberOfFrames++;

    numberOfFrames = IPP_MIN(numberOfFrames, m_lookAheadDep);

   // fill totalEstRate
   it = start;
   for(mfxU32 i=0; i < numberOfFrames ; i++)
   {
        for (mfxU32 qp = 0; qp < 52; qp++)
        {
            
            (*it).estRateTotal[qp] = IPP_MAX(MIN_EST_RATE, m_rateCoeffHistory[(*it).layer < 8 ? (*it).layer : 7][qp].GetCoeff() * (*it).estRate[qp]);
            totalEstRate[qp] += (*it).estRateTotal[qp];        
        }
        ++it;
   }
   


   // calculate Qp
    mfxI32 maxDeltaQp = -100000;
    mfxI32 curQp = m_curBaseQp < 0 ? SelectQp(totalEstRate, m_targetRateMin * numberOfFrames, 8) : m_curBaseQp;
    mfxF64 strength = 0.01 * curQp + 1.2;

    // fill deltaQp
    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        mfxU32 intraCost    = (*it).intraCost;
        mfxU32 interCost    = (*it).interCost;
        mfxU32 propCost     = (*it).propCost;
        mfxF64 ratio        = 1.0;//mfxF64(interCost) / intraCost;
        mfxF64 deltaQp      = log((intraCost + propCost * ratio) / intraCost) / log(2.0);
        (*it).deltaQp = (interCost >= intraCost * 0.9)
            ? -mfxI32(deltaQp * 1.5 * strength + 0.5)
            : -mfxI32(deltaQp * 1.0 * strength + 0.5);
        maxDeltaQp = IPP_MAX(maxDeltaQp, (*it).deltaQp);
        maxLayer = ((*it).layer > maxLayer) ? (*it).layer : maxLayer;
        ++it;
    }
    
 
    it = start;
    for (mfxU32 i=0; i < numberOfFrames ; i++)
    {
        (*it).deltaQp -= maxDeltaQp;
        ++it;
    }

    
    {
        mfxU8 minQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMax * numberOfFrames, 8);
        mfxU8 maxQp = (mfxU8) SelectQp(start,m_laData.end(), m_targetRateMin * numberOfFrames, 8);

        if (m_curBaseQp < 0)
            m_curBaseQp = minQp; // first frame
        else if (m_curBaseQp < minQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, minQp);
        else if (m_curQp > maxQp)
            m_curBaseQp = CLIPVAL(m_curBaseQp - MAX_QP_CHANGE, m_curBaseQp + MAX_QP_CHANGE, maxQp);
        else
            {}; // do not change qp if last qp guarantees target rate interval
    }
    if ((*start).deltaQp == 0 && (*start).layer == 0 && maxLayer > 1)
    {
        (*start).deltaQp = -1;     
    }
    m_curQp = CLIPVAL(1, 51, m_curBaseQp + (*start).deltaQp); // driver doesn't support qp=0
    //printf("%d) intra %d, inter %d, prop %d, delta %d, maxdelta %d, baseQP %d, qp %d \n",(*start).encOrder,(*start).intraCost, (*start).interCost, (*start).propCost, (*start).deltaQp, maxDeltaQp, m_curBaseQp,m_curQp );

    //printf("%d\t base QP %d\tdelta QP %d\tcurQp %d, rate (%f, %f), total rate %f (%f, %f), number of frames %d\n", 
    //    (*start).dispOrder, m_curBaseQp, (*start).deltaQp, m_curQp, (mfxF32)m_targetRateMax*numberOfFrames,(mfxF32)m_targetRateMin*numberOfFrames,  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp),  (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp + 1), (mfxF32)GetTotalRate(start,m_laData.end(), m_curQp -1),numberOfFrames);
    //if ((*start).dispOrder > 1700 && (*start).dispOrder < 1800 )
    {
        //for (mfxU32 qp = 0; qp < 52; qp++)
        //{
        //    printf("....qp %d coeff hist %f\n", qp, (mfxF32)m_rateCoeffHistory[qp].GetCoeff());
       // }        
    }
    (*start).qp = m_curQp;

    return mfxU8(m_curQp);
}

const size_t PreAnalysisExtBuffers::NUM_EXT_PARAM = sizeof(PreAnalysisExtBuffers().extParamAll) / sizeof(PreAnalysisExtBuffers().extParamAll[0]);

PreAnalysisExtBuffers::PreAnalysisExtBuffers()
{
    Zero(extParamAll);
    CleanUp();
    size_t count = 0;
    extParamAll[count++] = &extLAStats.Header;
    assert(NUM_EXT_PARAM == count);
}

void PreAnalysisExtBuffers::CleanUp()
{
    mfxExtBuffer header = { Type2Id<mfxExtLAFrameStatistics>::id, sizeof(extLAStats) };
    memset(&extLAStats, 0, sizeof(mfxExtLAFrameStatistics));
    extLAStats.Header = header;

}

// Initialize with specified parameter(s)
mfxStatus WrapExtBRC::Init(const mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode) {
    mfxExtBRC *extBRC = GetExtBuffer(*init);
    if (extBRC && extBRC->pthis) {
        mfxVideoParam video = *init;
        m_pBRC = extBRC;
        mIsInit = 1;
        return m_pBRC->Init(m_pBRC->pthis, &video);
    }
    return MFX_ERR_NOT_INITIALIZED;
}

mfxStatus WrapExtBRC::Close() {
    if (m_pBRC) {
        mIsInit = 0;
        return m_pBRC->Close(m_pBRC->pthis);
    }
    return MFX_ERR_NOT_INITIALIZED;
}

mfxStatus WrapExtBRC::Reset(mfxVideoParam *init, H265VideoParam &video, Ipp32s enableRecode) {
    if (m_pBRC) {
        return m_pBRC->Reset(m_pBRC->pthis, init);
    }
    return MFX_ERR_NOT_INITIALIZED;
}

mfxBRCStatus WrapExtBRC::PostPackFrame(H265VideoParam *video, Ipp8s sliceQpY, Frame *pFrame, Ipp32s bitsEncodedFrame, Ipp32s overheadBits, Ipp32s recode) {
    if (m_pBRC) {
        mfxBRCFrameParam frame_par = {};
        mfxBRCFrameCtrl  frame_ctrl = {};
        mfxBRCFrameStatus frame_sts = {};

        frame_ctrl.QpY = sliceQpY;
        frame_par.CodedFrameSize = bitsEncodedFrame >> 3;  // Size of frame in bytes after encoding
        frame_par.EncodedOrder = pFrame->m_encOrder;
        frame_par.FrameType = pFrame->m_picCodeType;
        frame_par.PyramidLayer = pFrame->m_pyramidLayer;
        frame_par.NumRecode = recode;
        frame_par.DisplayOrder = pFrame->m_poc;

        if (video->AnalyzeCmplx) {
            m_ppExtBuffers.extLAStats.NumAlloc = 1;
            m_ppExtBuffers.extLAStats.FrameStat = &ppframe;
            m_ppExtBuffers.extLAStats.NumStream = 1;
            m_ppExtBuffers.extLAStats.NumFrame = 1;
            mfxLAFrameInfo* pFrameData = &m_ppExtBuffers.extLAStats.FrameStat[0];
            pFrameData->Width = video->LowresFactor ? pFrame->m_lowres->width : pFrame->m_origin->width;
            pFrameData->Height = video->LowresFactor ? pFrame->m_lowres->height : pFrame->m_origin->height;
            pFrameData->FrameType = pFrame->m_picCodeType;
            pFrameData->FrameDisplayOrder = pFrame->m_poc;
            pFrameData->FrameEncodeOrder = pFrame->m_encOrder;
            pFrameData->Layer = pFrame->m_pyramidLayer;
            pFrameData->IntraCost = pFrame->m_stats[video->LowresFactor ? 1 : 0]->m_avgIntraSatd * (pFrameData->Width*pFrameData->Height);
            pFrameData->InterCost = pFrame->m_stats[video->LowresFactor ? 1 : 0]->m_avgInterSatd * (pFrameData->Width*pFrameData->Height);
            //pFrameData->BestCost = pFrame->m_stats[video->LowresFactor ? 1 : 0]->m_avgBestSatd * (pFrameData->Width*pFrameData->Height);
            frame_par.NumExtParam = 1;
            frame_par.ExtParam = m_ppExtBuffers.extParamAll;
        }

        mfxStatus sts = m_pBRC->Update(m_pBRC->pthis, &frame_par, &frame_ctrl, &frame_sts);
        MFX_CHECK(sts == MFX_ERR_NONE, (mfxU32)H265Enc::MFX_BRC_ERROR); // BRC_ERROR
        
        m_minSize = frame_sts.MinFrameSize;

        switch (frame_sts.BRCStatus)
        {
        case ::MFX_BRC_OK:
            return  H265Enc::MFX_BRC_OK;
        case ::MFX_BRC_BIG_FRAME:
            return H265Enc::MFX_BRC_BIG_FRAME;
        case ::MFX_BRC_SMALL_FRAME:
            return H265Enc::MFX_BRC_SMALL_FRAME;
        case ::MFX_BRC_PANIC_BIG_FRAME:
            return H265Enc::MFX_BRC_ERR_BIG_FRAME;
        case ::MFX_BRC_PANIC_SMALL_FRAME:
            return H265Enc::MFX_BRC_ERR_SMALL_FRAME;
        }
        return H265Enc::MFX_BRC_OK;
    }
    return H265Enc::MFX_BRC_ERROR;
}

Ipp32s WrapExtBRC::GetQP(H265VideoParam *video, Frame *pFrames[], Ipp32s numFrames) {
    if (m_pBRC) {
        mfxBRCFrameParam frame_par = { 0 };
        mfxBRCFrameCtrl  frame_ctrl = { 0 };

        if (pFrames && numFrames > 0) {
            frame_par.EncodedOrder = pFrames[0]->m_encOrder;
            frame_par.DisplayOrder = pFrames[0]->m_poc;
            frame_par.FrameType = pFrames[0]->m_picCodeType;
            frame_par.PyramidLayer = pFrames[0]->m_pyramidLayer;

            if (video->AnalyzeCmplx) {
                m_paExtBuffers.extLAStats.NumAlloc = MAX_LAFRAME_ALLOC;
                m_paExtBuffers.extLAStats.FrameStat = laframes;
                m_paExtBuffers.extLAStats.NumStream = 1;
                mfxU32 lookahead = IPP_MIN(numFrames, m_paExtBuffers.extLAStats.NumAlloc);
                // summary LA data and copy into output structure
                m_paExtBuffers.extLAStats.NumFrame = lookahead;
                m_paExtBuffers.extLAStats.NumStream = 1;
                mfxLAFrameInfo* pFrameData = &m_paExtBuffers.extLAStats.FrameStat[0];
                for (mfxU32 i = 0; i < lookahead; i++)
                {
                    pFrameData[i].Width = video->LowresFactor ? pFrames[i]->m_lowres->width : pFrames[i]->m_origin->width;
                    pFrameData[i].Height = video->LowresFactor ? pFrames[i]->m_lowres->height : pFrames[i]->m_origin->height;
                    pFrameData[i].FrameType = pFrames[i]->m_picCodeType;
                    pFrameData[i].FrameDisplayOrder = pFrames[i]->m_poc;
                    pFrameData[i].FrameEncodeOrder = pFrames[i]->m_encOrder;
                    pFrameData[i].Layer = pFrames[i]->m_pyramidLayer;
                    pFrameData[i].IntraCost = pFrames[i]->m_stats[video->LowresFactor ? 1 : 0]->m_avgIntraSatd * (pFrameData[i].Width*pFrameData[i].Height);
                    pFrameData[i].InterCost = pFrames[i]->m_stats[video->LowresFactor ? 1 : 0]->m_avgInterSatd * (pFrameData[i].Width*pFrameData[i].Height);
                    //pFrameData[i].BestCost = pFrames[i]->m_stats[video->LowresFactor ? 1 : 0]->m_avgBestSatd * (pFrameData[i].Width*pFrameData[i].Height);
                }
                frame_par.NumExtParam = 1;
                frame_par.ExtParam = m_paExtBuffers.extParamAll;
            }
        }
        m_pBRC->GetFrameCtrl(m_pBRC->pthis, &frame_par, &frame_ctrl);
        return (mfxU8)frame_ctrl.QpY;
    }
    else {
        return -1;
    }
}

void WrapExtBRC::GetMinMaxFrameSize(Ipp32s *minFrameSizeInBits, Ipp32s *maxFrameSizeInBits) {
    *minFrameSizeInBits = m_minSize;
    *maxFrameSizeInBits = 0;
}

BrcIface * CreateBrc(mfxVideoParam const * video)
{
    mfxExtCodingOption2 *ext = GetExtBuffer(*video);
    mfxExtBRC *extBRC = GetExtBuffer(*video);
    if (video->mfx.RateControlMethod == MFX_RATECONTROL_LA_EXT)
        return new VMEBrc;
    else if (ext && extBRC && ext->ExtBRC == MFX_CODINGOPTION_ON && extBRC->pthis)
        return new WrapExtBRC();
    else
        return new H265BRC;
}
} // namespace

#endif