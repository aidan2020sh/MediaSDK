//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017 Intel Corporation. All Rights Reserved.
//
#ifndef _ASCSTRUCTURES_H_
#define _ASCSTRUCTURES_H_

#pragma once

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <intrin.h>
    #include <smmintrin.h>
#else
    #include <immintrin.h>
#endif

#include "mfxdefs.h"
#include "cmrt_cross_platform.h"

#if defined(_WIN32) || defined(_WIN64)
    #define ASC_API      __declspec(dllexport)
    #define ASC_API_FUNC __declspec(dllexport)
#else // linux
    #define ASC_API_FUNC //__attribute__((visibility("default")))
    #define ASC_API      //__attribute__((visibility("default")))
#endif //defined(_WIN32) || defined(_WIN64)

typedef mfxU8*             pmfxU8;
typedef mfxI8*             pmfxI8;
typedef mfxU16*            pmfxU16;
typedef mfxI16*            pmfxI16;
typedef mfxU32*            pmfxU32;
typedef mfxI32*            pmfxI32;
typedef mfxL32*            pmfxL32;
typedef mfxF32*            pmfxF32;
typedef mfxF64*            pmfxF64;
typedef mfxU64*            pmfxU64;
typedef mfxI64*            pmfxI64;

namespace ns_asc{

typedef enum ASC_LTR_DESICION {
    NO_LTR = false,
    YES_LTR = true,
    FORCE_LTR
} ASC_LTR_DEC;

typedef enum ASCSimilar {
    Not_same,
    Same
} ASCSimil;
typedef enum ASCLayers {
    ASCFull_Size,
    ASCSmall_Size
} ASClayer;
typedef enum ASCResizing_Target {
    ASCSmall_Target
} ASCRT;
typedef enum ASCData_Flow_Direction {
    ASCReference_Frame,
    ASCCurrent_Frame,
    ASCScene_Diff_Frame
}ASCDFD;
typedef enum ASCGoP_Sizes {
    Forbidden_GoP,
    Immediate_GoP,
    QuarterHEVC_GoP,
    Three_GoP,
    HalfHEVC_GoP,
    Five_GoP,
    Six_GoP,
    Seven_Gop,
    HEVC_Gop,
    Nine_Gop,
    Ten_Gop,
    Eleven_Gop,
    Twelve_Gop,
    Thirteen_Gop,
    Fourteen_Gop,
    Fifteen_Gop,
    Double_HEVC_Gop
} ASCGOP;
typedef enum ASCBufferPosition {
    ASCcurrent_frame_data,
    ASCprevious_frame_data,
    ASCprevious_previous_frame_data,
    ASCSceneVariation_frame_data
} ASCBP;
typedef enum ASCGPU_USAGE {
    ASCNo_GPU_Proc,
    ASCDo_GPU_Proc
}ASCGU;
typedef enum ASCFrameTypeScan {
    ASC_UNKNOWN,
    ASCprogressive_frame,
    ASCtopfieldfirst_frame,
    ASCbotfieldFirst_frame
}ASCFTS;
typedef enum ASCFrameFields {
    ASCTopField,
    ASCBottomField
}ASCFF;
typedef struct ASCtimming_measurement_var {
    LARGE_INTEGER
        tFrequency,
        tStart,
        tPause[10],
        tStop;
    mfxF64
        timeval,
        calctime;
}ASCTime;

typedef struct ASCcoordinates {
    mfxI16 
        x;
    mfxI16
        y;
}ASCMVector;

typedef struct ASCBaseline {
    mfxI32
        start;
    mfxI32
        end;
}ASCLine;

typedef struct ASCYUV_layer_store {
    mfxU8
        *data,
        *Y,
        *U,
        *V;
    mfxU32
        width,
        height,
        pitch,
        hBorder,
        wBorder,
        extWidth,
        extHeight;
}ASCYUV;

typedef struct ASCSAD_range {
    mfxU16 
        SAD,
        distance;
    ASCMVector
        BestMV;
}ASCRsad;

typedef struct ASCImage_details {
      mfxI32
        Original_Width,             //User input
        Original_Height,            //User input
        horizontal_pad,             //User input for original resolution in multiples of FRAMEMUL, derived for the other two layers
        vertical_pad,               //User input for original resolution in multiples of FRAMEMUL, derived for the other two layers
        _cwidth,                    //corrected size if width not multiple of FRAMEMUL
        _cheight,                   //corrected size if height not multiple of FRAMEMUL
        pitch,                      //horizontal_pad + _cwidth + horizontal_pad
        Extended_Width,             //horizontal_pad + _cwidth + horizontal_pad
        Extended_Height,            //vertical_pad + _cheight + vertical_pad
        Total_non_corrected_pixels,
        Pixels_in_Y_layer,          //_cwidth * _cheight
        Pixels_in_U_layer,          //Pixels_in_Y_layer / 4 (assuming 4:2:0)
        Pixels_in_V_layer,          //Pixels_in_Y_layer / 4 (assuming 4:2:0)
        Pixels_in_full_frame,       //Pixels_in_Y_layer * 3 / 2 (assuming 4:2:0)
        block_width,                //User input
        block_height,               //User input
        Pixels_in_block,            //block_width x block_height
        Width_in_blocks,            //_cwidth / block_width
        Height_in_blocks,           //_cheight / block_height
        Blocks_in_Frame,            //Pixels_in_Y_layer / Pixels_in_block
        initial_point,              //(Extended_Width * vertical_pad) + horizontal_pad
        sidesize,                   //_cheight + (1 * vertical_pad)
        endPoint,                   //(sidesize * Extended_Width) - horizontal_pad
        MVspaceSize;                //Pixels_in_Y_layer / block_width / block_height
}ASCImDetails;

typedef struct ASCVideo_characteristics {
    ASCImDetails
        *layer;
    mfxI32
        starting_frame,              //Frame number where the video is going to be accessed
        total_number_of_frames,      //Total number of frames in video file
        processed_frames,            //Number of frames that are going to be processed, taking into account the starting frame
        accuracy,
        key_frame_frequency,
        limitRange,
        maxXrange,
        maxYrange,
        interlaceMode,
        StartingField,
        currentField;
    ASCTime
        timer;
}ASCVidData;

typedef bool(*t_SCDetect)(mfxI32 diffMVdiffVal, mfxU32 RsCsDiff,   mfxU32 MVDiff,   mfxU32 Rs,       mfxU32 AFD,
                          mfxU32 CsDiff,        mfxI32 diffTSC,    mfxU32 TSC,      mfxU32 gchDC,    mfxI32 diffRsCsdiff,
                          mfxU32 posBalance,    mfxU32 SC,         mfxU32 TSCindex, mfxU32 Scindex,  mfxU32 Cs,
                          mfxI32 diffAFD,       mfxU32 negBalance, mfxU32 ssDCval,  mfxU32 refDCval, mfxU32 RsDiff);

typedef struct ASCstats_structure {
    mfxI32
        frameNum,
        scVal,
        tscVal,
        pdist,
        histogram[5],
        Schg,
        last_shot_distance;
    mfxU32
        SCindex,
        TSCindex,
        Rs,
        Cs,
        SC,
        TSC,
        RsDiff,
        CsDiff,
        RsCsDiff,
        MVdiffVal,
        AbsMVSize,
        AbsMVHSize,
        AbsMVVSize;
    mfxU16
        AFD;
    mfxI32
        ssDCval,
        refDCval,
        diffAFD,
        diffTSC,
        diffRsCsDiff,
        diffMVdiffVal;
    mfxU32
        gchDC,
        posBalance,
        negBalance,
        avgVal;
    mfxI64
        ssDCint,
        refDCint;
    bool
        Gchg;
    mfxU8
        picType,
        lastFrameInShot;
    bool
        repeatedFrame,
        firstFrame,
        copyFrameDelay,
        fadeIn,
        ltr_flag;
}ASCTSCstat;

} //namespace ns_asc
#endif //_STRUCTURES_H_
