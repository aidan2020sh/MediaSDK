/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_DEBLOCKING_H
#define __UMC_H264_DEC_DEBLOCKING_H

#include "umc_h264_dec_defs_dec.h"

namespace UMC
{

#if defined(_MSC_VER) && !defined(_WIN32_WCE)
#define __align(value) __declspec(align(value))
#else // !defined(_MSC_VER)
#define __align(value)
#endif // defined(_MSC_VER)

#define IClip(Min, Max, Val) (((Val) < (Min)) ? (Min) : (((Val) > (Max)) ? (Max) : (Val)))
#define SetEdgeStrength(edge, strength) {                                               \
    Ipp32u val = (((((strength) * 256) + strength) * 256 + strength) * 256 + strength); \
    memcpy_s(edge, sizeof(Ipp32u), &val, sizeof(Ipp32u));                               \
}
#define CopyEdgeStrength(dst_edge, src_edge)  \
    memcpy_s(dst_edge, sizeof(Ipp32u), src_edge, sizeof(Ipp32u))
#define CompareEdgeStrength(strength, edge) \
    ((((((strength) * 256) + strength) * 256 + strength) * 256 + strength) == *((Ipp32u *) (edge)))

// declare used types and constants

enum
{
    VERTICAL_DEBLOCKING     = 0,
    HORIZONTAL_DEBLOCKING   = 1,
    NUMBER_OF_DIRECTION     = 2
};

enum
{
    CURRENT_BLOCK           = 0,
    NEIGHBOUR_BLOCK         = 1
};

extern
Ipp8u VERTICAL_DEBLOCKING_BLOCKS_MAP[16];

// alpha table
extern
Ipp8u ALPHA_TABLE[52];

// beta table
extern
Ipp8u BETA_TABLE[52];

// clipping table
extern
Ipp8u CLIP_TAB[52][5];

// chroma scaling QP table
extern
Ipp8u QP_SCALE_CR[52];

// masks for external blocks pair "coded bits"
extern
Ipp32u EXTERNAL_BLOCK_MASK[NUMBER_OF_DIRECTION][2][4];

// masks for internal blocks pair "coded bits"
extern
Ipp32u INTERNAL_BLOCKS_MASK[NUMBER_OF_DIRECTION][12];

#pragma pack(16)

// turn off "non-virtual destructor" remark
#pragma warning(disable : 444)

typedef struct DeblockingParameters
{
    Ipp8u  Strength[NUMBER_OF_DIRECTION][16];                   // (Ipp8u [][]) arrays of deblocking sthrengths
    Ipp32s DeblockingFlag[NUMBER_OF_DIRECTION];                 // (Ipp32u []) flags to do deblocking
    Ipp32s ExternalEdgeFlag[NUMBER_OF_DIRECTION];               // (Ipp32u []) flags to do deblocking on external edges
    Ipp32s nMaxMVector;                                         // (Ipp32u) maximum vertical motion vector
    Ipp32s nNeighbour[NUMBER_OF_DIRECTION];                     // (Ipp32u) neighbour macroblock addres
    size_t iReferences[4][2];                                   // (Ipp32s [][]) exact references for deblocking
    Ipp32s iReferenceNum[4];                                    // (Ipp32s []) number of valid exact references
    Ipp32s MBFieldCoded;                                        // (Ipp32u) flag means macroblock is field coded (picture may not)
    Ipp32s nAlphaC0Offset;                                      // (Ipp32s) alpha c0 offset
    Ipp32s nBetaOffset;                                         // (Ipp32s) beta offset
    PlanePtrYCommon pLuma;                                      // (Ipp8u *) pointer to luminance data
    PlanePtrUVCommon pChroma;                                   // (Ipp8u *) pointer to chrominance data

    Ipp32s m_isSameSlice;

    Ipp32s pitch_luma;
    Ipp32s pitch_chroma;
} DeblockingParameters;

typedef struct DeblockingParametersMBAFF : public DeblockingParameters
{
    Ipp8u  StrengthComplex[16];                                 // (Ipp8u) arrays of deblocking sthrengths
    Ipp8u  StrengthExtra[16];                                   // (Ipp8u) arrays of deblocking sthrengths
    Ipp32s UseComplexVerticalDeblocking;                        // (Ipp32u) flag to do complex deblocking on external vertical edge
    Ipp32s ExtraHorizontalEdge;                                 // (Ipp32u) flag to do deblocking on extra horizontal edge
    Ipp32s nLeft[2];                                            // (Ipp32u []) left couple macroblock numbers

} DeblockingParametersMBAFF;

// restore "non-virtual destructor" remark
#pragma warning(default : 444)

#pragma pack()

} // namespace UMC

#endif // __UMC_H264_DEC_DEBLOCKING_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
