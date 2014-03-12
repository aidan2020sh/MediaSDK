/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.
//
*/

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined (MFX_ENABLE_CM))

#pragma once

#ifndef __MFX_H265_ENC_CM_DEFS_H__
#define __MFX_H265_ENC_CM_DEFS_H__

#pragma warning(disable: 4505)
#pragma warning(push)
#pragma warning(disable: 4100)
#pragma warning(disable: 4201)
#include "cm_def.h"
#include "cm_vm.h"
#include "mfx_h265_set.h"

namespace H265Enc {

enum PuSize {
    PU32x32, PU32x16, PU16x32, PU32x8, PU32x24, PU8x32, PU24x32,
    PU16x16, PU16x8,  PU8x16,  PU16x4, PU16x12, PU4x16, PU12x16,
    PU8x8,   PU8x4,   PU4x8,
    PU_MAX
};

void AllocateCmResources(mfxU32 w, mfxU32 h, mfxU16 nRefs);

void RunVme(
    H265VideoParam const & param,
    H265Frame *            pFrameCur,
    H265Frame *            pFrameNext,
    H265Slice *            pSliceCur,
    H265Slice *            pSliceNext,
    H265Frame **           refsCur,
    H265Frame **           refsNext);

void FreeCmResources();
void PrintTimes();

struct CmMbIntraDist // sizeof=4
{
    mfxU16 intraDist;
    mfxU16 reserved;
};

struct CmMbIntraGrad // sizeof=80
{
    mfxU16 histogram[35];
    mfxU16 reserved[5];
};

struct CmMbDist32 // sizeof=64
{
    mfxU32      dist[16];  // only 9 are used
};

struct CmMbDist16 // sizeof=4
{
    mfxU32      dist;
};

struct H265EncCURBEData {
    //DW0
    union {
        mfxU32   DW0;
        struct {
            mfxU32  SkipModeEn:1;
            mfxU32  AdaptiveEn:1;
            mfxU32  BiMixDis:1;
            mfxU32  MBZ1:2;
            mfxU32  EarlyImeSuccessEn:1;
            mfxU32  MBZ2:1;
            mfxU32  T8x8FlagForInterEn:1;
            mfxU32  MBZ3:16;
            mfxU32  EarlyImeStop:8;
        };
    };

    //DW1
    union {
        mfxU32   DW1;
        struct {
            mfxU32  MaxNumMVs:6;
            mfxU32  MBZ4:10;
            mfxU32  BiWeight:6;
            mfxU32  MBZ5:6;
            mfxU32  UniMixDisable:1;
            mfxU32  MBZ6:3;
        };
    };

    //DW2
    union {
        mfxU32   DW2;
        struct {
            mfxU32  LenSP:8;
            mfxU32  MaxNumSU:8;
            mfxU32  MBZ7:16;
        };
    };

    //DW3
    union {
        mfxU32   DW3;
        struct {
            mfxU32  SrcSize:2;
            mfxU32  MBZ8:1;
            mfxU32  MBZ9:1;
            mfxU32  MbTypeRemap:2;
            mfxU32  SrcAccess:1;
            mfxU32  RefAccess:1;
            mfxU32  SearchCtrl:3;
            mfxU32  DualSearchPathOption:1;
            mfxU32  SubPelMode:2;
            mfxU32  SkipType:1;
            mfxU32  DisableFieldCacheAllocation:1;
            mfxU32  InterChromaMode:1;
            mfxU32  FTQSkipEnable:1;
            mfxU32  BMEDisableFBR:1;
            mfxU32  BlockBasedSkipEnabled:1;
            mfxU32  InterSAD:2;
            mfxU32  IntraSAD:2;
            mfxU32  SubMbPartMask:7;
            mfxU32  MBZ10:1;
        };
    };

    //DW4
    union {
        mfxU32   DW4;
        struct {
            mfxU32  SliceHeightMinusOne:8;
            mfxU32  PictureHeightMinusOne:8;
            mfxU32  PictureWidth:8;
            mfxU32  Log2MvScaleFactor:4;
            mfxU32  Log2MbScaleFactor:4;
        };
    };

    //DW5
    union {
        mfxU32   DW5;
        struct {
            mfxU32  MBZ12:16;
            mfxU32  RefWidth:8;
            mfxU32  RefHeight:8;
        };
    };

    //DW6
    union {
        mfxU32   DW6;
        struct {
            mfxU32  BatchBufferEndCommand:32;
        };
    };

    //DW7
    union {
        mfxU32   DW7;
        struct {
            mfxU32  IntraPartMask:5;
            mfxU32  NonSkipZMvAdded:1;
            mfxU32  NonSkipModeAdded:1;
            mfxU32  IntraCornerSwap:1;
            mfxU32  MBZ13:8;
            mfxU32  MVCostScaleFactor:2;
            mfxU32  BilinearEnable:1;
            mfxU32  SrcFieldPolarity:1;
            mfxU32  WeightedSADHAAR:1;
            mfxU32  AConlyHAAR:1;
            mfxU32  RefIDCostMode:1;
            mfxU32  MBZ00:1;
            mfxU32  SkipCenterMask:8;
        };
    };

    //DW8
    union {
        mfxU32   DW8;
        struct {
            mfxU32  ModeCost_0:8;
            mfxU32  ModeCost_1:8;
            mfxU32  ModeCost_2:8;
            mfxU32  ModeCost_3:8;
        };
    };

    //DW9
    union {
        mfxU32   DW9;
        struct {
            mfxU32  ModeCost_4:8;
            mfxU32  ModeCost_5:8;
            mfxU32  ModeCost_6:8;
            mfxU32  ModeCost_7:8;
        };
    };

    //DW10
    union {
        mfxU32   DW10;
        struct {
            mfxU32  ModeCost_8:8;
            mfxU32  ModeCost_9:8;
            mfxU32  RefIDCost:8;
            mfxU32  ChromaIntraModeCost:8;
        };
    };

    //DW11
    union {
        mfxU32   DW11;
        struct {
            mfxU32  MvCost_0:8;
            mfxU32  MvCost_1:8;
            mfxU32  MvCost_2:8;
            mfxU32  MvCost_3:8;
        };
    };

    //DW12
    union {
        mfxU32   DW12;
        struct {
            mfxU32  MvCost_4:8;
            mfxU32  MvCost_5:8;
            mfxU32  MvCost_6:8;
            mfxU32  MvCost_7:8;
        };
    };

    //DW13
    union {
        mfxU32   DW13;
        struct {
            mfxU32  QpPrimeY:8;
            mfxU32  QpPrimeCb:8;
            mfxU32  QpPrimeCr:8;
            mfxU32  TargetSizeInWord:8;
        };
    };

    //DW14
    union {
        mfxU32   DW14;
        struct {
            mfxU32  FTXCoeffThresh_DC:16;
            mfxU32  FTXCoeffThresh_1:8;
            mfxU32  FTXCoeffThresh_2:8;
        };
    };

    //DW15
    union {
        mfxU32   DW15;
        struct {
            mfxU32  FTXCoeffThresh_3:8;
            mfxU32  FTXCoeffThresh_4:8;
            mfxU32  FTXCoeffThresh_5:8;
            mfxU32  FTXCoeffThresh_6:8;
        };
    };


    //DW16
    union {
        mfxU32   DW16;
        struct {
            mfxU32  IMESearchPath0:8;
            mfxU32  IMESearchPath1:8;
            mfxU32  IMESearchPath2:8;
            mfxU32  IMESearchPath3:8;
        };
    };

    //DW17
    union {
        mfxU32   DW17;
        struct {
            mfxU32  IMESearchPath4:8;
            mfxU32  IMESearchPath5:8;
            mfxU32  IMESearchPath6:8;
            mfxU32  IMESearchPath7:8;
        };
    };

    //DW18
    union {
        mfxU32   DW18;
        struct {
            mfxU32  IMESearchPath8:8;
            mfxU32  IMESearchPath9:8;
            mfxU32  IMESearchPath10:8;
            mfxU32  IMESearchPath11:8;
        };
    };

    //DW19
    union {
        mfxU32   DW19;
        struct {
            mfxU32  IMESearchPath12:8;
            mfxU32  IMESearchPath13:8;
            mfxU32  IMESearchPath14:8;
            mfxU32  IMESearchPath15:8;
        };
    };

    //DW20
    union {
        mfxU32   DW20;
        struct {
            mfxU32  IMESearchPath16:8;
            mfxU32  IMESearchPath17:8;
            mfxU32  IMESearchPath18:8;
            mfxU32  IMESearchPath19:8;
        };
    };

    //DW21
    union {
        mfxU32   DW21;
        struct {
            mfxU32  IMESearchPath20:8;
            mfxU32  IMESearchPath21:8;
            mfxU32  IMESearchPath22:8;
            mfxU32  IMESearchPath23:8;
        };
    };

    //DW22
    union {
        mfxU32   DW22;
        struct {
            mfxU32  IMESearchPath24:8;
            mfxU32  IMESearchPath25:8;
            mfxU32  IMESearchPath26:8;
            mfxU32  IMESearchPath27:8;
        };
    };

    //DW23
    union {
        mfxU32   DW23;
        struct {
            mfxU32  IMESearchPath28:8;
            mfxU32  IMESearchPath29:8;
            mfxU32  IMESearchPath30:8;
            mfxU32  IMESearchPath31:8;
        };
    };

    //DW24
    union {
        mfxU32   DW24;
        struct {
            mfxU32  IMESearchPath32:8;
            mfxU32  IMESearchPath33:8;
            mfxU32  IMESearchPath34:8;
            mfxU32  IMESearchPath35:8;
        };
    };

    //DW25
    union {
        mfxU32   DW25;
        struct {
            mfxU32  IMESearchPath36:8;
            mfxU32  IMESearchPath37:8;
            mfxU32  IMESearchPath38:8;
            mfxU32  IMESearchPath39:8;
        };
    };

    //DW26
    union {
        mfxU32   DW26;
        struct {
            mfxU32  IMESearchPath40:8;
            mfxU32  IMESearchPath41:8;
            mfxU32  IMESearchPath42:8;
            mfxU32  IMESearchPath43:8;
        };
    };

    //DW27
    union {
        mfxU32   DW27;
        struct {
            mfxU32  IMESearchPath44:8;
            mfxU32  IMESearchPath45:8;
            mfxU32  IMESearchPath46:8;
            mfxU32  IMESearchPath47:8;
        };
    };

    //DW28
    union {
        mfxU32   DW28;
        struct {
            mfxU32  IMESearchPath48:8;
            mfxU32  IMESearchPath49:8;
            mfxU32  IMESearchPath50:8;
            mfxU32  IMESearchPath51:8;
        };
    };

    //DW29
    union {
        mfxU32   DW29;
        struct {
            mfxU32  IMESearchPath52:8;
            mfxU32  IMESearchPath53:8;
            mfxU32  IMESearchPath54:8;
            mfxU32  IMESearchPath55:8;
        };
    };

    //DW30
    union {
        mfxU32   DW30;
        struct {
            mfxU32  Intra4x4ModeMask:9;
            mfxU32  MBZ14:7;
            mfxU32  Intra8x8ModeMask:9;
            mfxU32  MBZ15:7;
        };
    };

    //DW31
    union {
        mfxU32   DW31;
        struct {
            mfxU32  Intra16x16ModeMask:4;
            mfxU32  IntraChromaModeMask:4;
            mfxU32  IntraComputeType:2;
            mfxU32  MBZ16:22;
        };
    };

    //DW32
    union {
        mfxU32   DW32;
        struct {
            mfxU32  SkipVal:16;
            mfxU32  MultipredictorL0EnableBit:8;
            mfxU32  MultipredictorL1EnableBit:8;
        };
    };

    //DW33
    union {
        mfxU32   DW33;
        struct {
            mfxU32  IntraNonDCPenalty16x16:8;
            mfxU32  IntraNonDCPenalty8x8:8;
            mfxU32  IntraNonDCPenalty4x4:8;
            mfxU32  MBZ18:8;
        };
    };

    //DW34
    union {
        mfxU32   DW34;
        struct {
            mfxU32  MaxVmvR:16;
            mfxU32  MBZ19:16;
        };
    };


    //DW35
    union {
        mfxU32   DW35;
        struct {
            mfxU32  PanicModeMBThreshold:16;
            mfxU32  SmallMbSizeInWord:8;
            mfxU32  LargeMbSizeInWord:8;
        };
    };

    //DW36
    union {
        mfxU32   DW36;
        struct {
            mfxU32  HMERefWindowsCombiningThreshold:8;
            mfxU32  HMECombineOverlap:2;
            mfxU32  CheckAllFractionalEnable:1;
            mfxU32  MBZ20:21;
        };
    };

    //DW37
    union {
        mfxU32   DW37;
        struct {
            mfxU32  CurLayerDQId:8;
            mfxU32  TemporalId:4;
            mfxU32  NoInterLayerPredictionFlag:1;
            mfxU32  AdaptivePredictionFlag:1;
            mfxU32  DefaultBaseModeFlag:1;
            mfxU32  AdaptiveResidualPredictionFlag:1;
            mfxU32  DefaultResidualPredictionFlag:1;
            mfxU32  AdaptiveMotionPredictionFlag:1;
            mfxU32  DefaultMotionPredictionFlag:1;
            mfxU32  TcoeffLevelPredictionFlag:1;
            mfxU32  UseRawMePredictor:1;
            mfxU32  SpatialResChangeFlag:1;
            mfxU32  isFwdFrameShortTermRef:1;
            mfxU32  MBZ21:9;
        };
    };

    //the following offsets are in the unit of 2 pixels.
    //DW38
    union {
        mfxU32   DW38;
        struct {
            mfxU32  ScaledRefLayerLeftOffset:16;
            mfxU32  ScaledRefLayerRightOffset:16;
        };
    };

    //DW39
    union {
        mfxU32   DW39;
        struct {
            mfxU32  ScaledRefLayerTopOffset:16;
            mfxU32  ScaledRefLayerBottomOffset:16;
        };
    };
};


struct mfxVMEIMEIn
{
    union{
        mfxU8 Multicall;
        struct{
            mfxU8 StreamOut            :1;
            mfxU8 StreamIn            :1;
        };
    };
    mfxU8        IMESearchPath0to31[32];
    mfxU32        mbz0[2];
    mfxU8        IMESearchPath32to55[24];
    mfxU32        mbz1;
    mfxU8        RecRefID[2][4];
    mfxI16Pair    RecordMvs16[2];
    mfxU16        RecordDst16[2];
    mfxU16        RecRefID16[2];
    mfxU16        RecordDst[2][8];
    mfxI16Pair    RecordMvs[2][8];
};


const mfxU8 SingleSU[56] =
{
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};


#define L1  0x0F
#define R1  0x01
#define U1  0xF0
#define D1  0x10


const mfxU8 RasterScan_48x40[56] =
{// Starting location does not matter so long as wrapping enabled.
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1,R1|D1,
    R1,R1,R1,R1,R1,R1,R1
};


//      37 1E 1F 20 21 22 23 24
//      36 1D 0C 0D 0E 0F 10 25
//      35 1C 0B 02 03 04 11 26
//      34 1B 0A 01>00<05 12 27
//      33 1A 09 08 07 06 13 28
//      32 19 18 17 16 15 14 29
//      31 30 2F 2E 2D 2C 2B 2A
const mfxU8 FullSpiral_48x40[56] =
{ // L -> U -> R -> D
    L1,
    U1,
    R1,R1,
    D1,D1,
    L1,L1,L1,
    U1,U1,U1,
    R1,R1,R1,R1,
    D1,D1,D1,D1,
    L1,L1,L1,L1,L1,
    U1,U1,U1,U1,U1,
    R1,R1,R1,R1,R1,R1,
    D1,D1,D1,D1,D1,D1,      // The last D1 steps outside the search window.
    L1,L1,L1,L1,L1,L1,L1,   // These are outside the search window.
    U1,U1,U1,U1,U1,U1,U1
};


const mfxU8 Diamond[56] =
{
    0x0F,0xF1,0x0F,0x12,//5
    0x0D,0xE2,0x22,0x1E,//9
    0x10,0xFF,0xE2,0x20,//13
    0xFC,0x06,0xDD,//16
    0x2E,0xF1,0x3F,0xD3,0x11,0x3D,0xF3,0x1F,//24
    0xEB,0xF1,0xF1,0xF1,//28
    0x4E,0x11,0x12,0xF2,0xF1,//33
    0xE0,0xFF,0xFF,0x0D,0x1F,0x1F,//39
    0x20,0x11,0xCF,0xF1,0x05,0x11,//45
    0x00,0x00,0x00,0x00,0x00,0x00,//51
};


#define LUTMODE_INTRA_NONPRED    0x00  // extra penalty for non-predicted modes
#define LUTMODE_INTRA            0x01
#define LUTMODE_INTRA_16x16      0x01
#define LUTMODE_INTRA_8x8        0x02
#define LUTMODE_INTRA_4x4        0x03
#define LUTMODE_INTER_BWD        0x09
#define LUTMODE_REF_ID           0x0A
#define LUTMODE_INTRA_CHROMA     0x0B
#define LUTMODE_INTER            0x08
#define LUTMODE_INTER_16x16      0x08
#define LUTMODE_INTER_16x8       0x04
#define LUTMODE_INTER_8x16       0x04
#define LUTMODE_INTER_8x8q       0x05
#define LUTMODE_INTER_8x4q       0x06
#define LUTMODE_INTER_4x8q       0x06
#define LUTMODE_INTER_4x4q       0x07
#define LUTMODE_INTER_16x8_FIELD 0x06
#define LUTMODE_INTER_8x8_FIELD  0x07

#pragma warning(pop)

} // namespace


#endif // __MFX_H265_ENC_CM_DEFS_H__

#endif // MFX_ENABLE_CM

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
