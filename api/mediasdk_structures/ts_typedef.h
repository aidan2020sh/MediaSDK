/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#define TYPEDEF_MEMBER(base, member, name) \
    struct name : std::decay<decltype(base::member)>::type {};

TYPEDEF_MEMBER(mfxExtOpaqueSurfaceAlloc,  In,                  mfxExtOpaqueSurfaceAlloc_InOut)
TYPEDEF_MEMBER(mfxExtAVCRefListCtrl,      PreferredRefList[0], mfxExtAVCRefListCtrl_Entry)
TYPEDEF_MEMBER(mfxExtPictureTimingSEI,    TimeStamp[0],        mfxExtPictureTimingSEI_TimeStamp)
TYPEDEF_MEMBER(mfxExtAvcTemporalLayers,   Layer[0],            mfxExtAvcTemporalLayers_Layer)
TYPEDEF_MEMBER(mfxExtAVCEncodedFrameInfo, UsedRefListL0[0],    mfxExtAVCEncodedFrameInfo_RefList)
#if _MSC_VER<1914
TYPEDEF_MEMBER(mfxExtVPPVideoSignalInfo,  In,                  mfxExtVPPVideoSignalInfo_InOut)
#else
typedef struct {
	mfxU16  TransferMatrix;
	mfxU16  NominalRange;
	mfxU16  reserved2[6];
} mfxExtVPPVideoSignalInfo_InOut;
#endif
TYPEDEF_MEMBER(mfxExtEncoderROI,          ROI[0],              mfxExtEncoderROI_Entry)
TYPEDEF_MEMBER(mfxExtDirtyRect,           Rect[0],             mfxExtDirtyRect_Entry)
TYPEDEF_MEMBER(mfxExtMoveRect,            Rect[0],             mfxExtMoveRect_Entry)
typedef union { mfxU32 n; char c[4]; } mfx4CC;
typedef mfxExtAVCRefLists::mfxRefPic mfxExtAVCRefLists_mfxRefPic;
typedef mfxExtFeiEncMV::mfxExtFeiEncMVMB mfxExtFeiEncMV_MB;
typedef mfxExtFeiEncMBCtrl::mfxExtFeiEncMBCtrlMB mfxExtFeiEncMBCtrl_MB;
typedef mfxExtFeiPreEncMVPredictors::mfxExtFeiPreEncMVPredictorsMB mfxExtFeiPreEncMVPredictors_MB;
typedef mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB mfxExtFeiPreEncMV_MB;
typedef mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB mfxExtFeiPreEncMBStat_MB;

#if MFX_VERSION >= 1023
typedef mfxExtFeiPPS::mfxExtFeiPpsDPB mfxExtFeiPPS_mfxExtFeiPpsDPB;
#endif // MFX_VERSION >= 1023
