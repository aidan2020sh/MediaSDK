/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/


#include "mf_param_slice_control.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"


struct mfxFrameInfoHelper : public mfxFrameInfo
{
    mfxFrameInfoHelper(mfxU16 w, mfxU16 h, mfxU16 picStruct = MFX_PICSTRUCT_PROGRESSIVE)
    {
        Zero(*this);
        CropW = w;
        CropH = h;
        PicStruct = picStruct;
    }
};

//const mfxFrameInfo frameInfo720p = {
//    {0},    //mfxU32  reserved[6];
//    {0},    //mfxFrameId FrameId;
//    0,      //mfxU32  FourCC;
//    1280,   //mfxU16  Width;
//    720,    //mfxU16  Height;
//
//    0,      //mfxU16  CropX;
//    0,      //mfxU16  CropY;
//    1280,   //mfxU16  CropW;
//    720,    //mfxU16  CropH;
//
//    0,      //mfxU32  FrameRateExtN;
//    0,      //mfxU32  FrameRateExtD;
//    0,      //mfxU16  reserved3;
//
//    0,      //mfxU16  AspectRatioW;
//    0,      //mfxU16  AspectRatioH;
//
//    MFX_PICSTRUCT_PROGRESSIVE, //mfxU16  PicStruct;
//    0,      //mfxU16  ChromaFormat;
//    0,      //mfxU16  reserved2;
//};

TEST(Param_SliceControl, DefaultSettings)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(0, sc.GetNumSlice());
}

TEST(Param_SliceControl, SetModeInvalid)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(E_INVALIDARG, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MAX+1));
}

TEST(Param_SliceControl, SetModeUnsupported)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(E_INVALIDARG, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_BITS_PER_SLICE));
}

TEST(Param_SliceControl, SetSizeInvalid)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(E_INVALIDARG, sc.SetSize(0)); //TODO: consider possibility to set 0 (reset to default)
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(0, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_5kMBsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(5000));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(1, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_3600MBsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(1280*720/16/16));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(1, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_1800MBsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(1280*720/16/16/2));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(2, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_1kMBsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBS_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(1000));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(4, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_50MBrowsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(50));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(1, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_45MBrowsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(45));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(1, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_15MBrowsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(15));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(3, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetNumSlice_720p_10MBrowsPerSlice)
{
    MFParamSliceControl sc(NULL);
    EXPECT_EQ(S_OK, sc.SetMode(MF_PARAM_SLICECONTROL_MODE_MBROW_PER_SLICE));
    EXPECT_EQ(S_OK, sc.SetSize(10));
    EXPECT_EQ(S_OK, sc.SetFrameInfo(mfxFrameInfoHelper(1280,720)));
    EXPECT_EQ(5, sc.GetNumSlice());
}

TEST(Param_SliceControl, GetModeParameterRange_ULONG)
{
    MFParamSliceControl sc(NULL);
    ULONG ValueMin = 0, ValueMax = 0, SteppingDelta = 0;
    EXPECT_EQ(S_OK, sc.GetModeParameterRange(ValueMin, ValueMax, SteppingDelta));
    EXPECT_EQ(0, ValueMin);
    EXPECT_EQ(2, ValueMax);
    EXPECT_EQ(2, SteppingDelta);
}

TEST(Param_SliceControl, GetModeParameterRange_VARIANT_NULLPTR)
{
    MFParamSliceControl sc(NULL);
    VARIANT ValueMin, /*ValueMax, */SteppingDelta;
    EXPECT_EQ(E_POINTER, sc.GetModeParameterRange(&ValueMin, NULL, &SteppingDelta));
}

TEST(Param_SliceControl, GetModeParameterRange_VARIANT)
{
    MFParamSliceControl sc(NULL);
    VARIANT ValueMin, ValueMax, SteppingDelta;
    EXPECT_EQ(S_OK, sc.GetModeParameterRange(&ValueMin, &ValueMax, &SteppingDelta));
    EXPECT_EQ(VT_UI4, ValueMin.vt);
    EXPECT_EQ(0, ValueMin.ulVal);
    EXPECT_EQ(VT_UI4, ValueMin.vt);
    EXPECT_EQ(2, ValueMax.ulVal);
    EXPECT_EQ(VT_UI4, ValueMin.vt);
    EXPECT_EQ(2, SteppingDelta.ulVal);
}
