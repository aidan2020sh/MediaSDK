/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_DEC_INIT_TABLES_CABAC_H__
#define __UMC_H265_DEC_INIT_TABLES_CABAC_H__

#include "umc_h265_dec_defs_dec.h"

namespace UMC_HEVC_DECODER
{
// LPS precalculated probability ranges. HEVC spec 9.3.4.3.1
const
Ipp8u rangeTabLPSH265[128][4]=
{
    { 128, 176, 208, 240},
    { 128, 176, 208, 240},
    { 128, 167, 197, 227},
    { 128, 167, 197, 227},
    { 128, 158, 187, 216},
    { 128, 158, 187, 216},
    { 123, 150, 178, 205},
    { 123, 150, 178, 205},
    { 116, 142, 169, 195},
    { 116, 142, 169, 195},
    { 111, 135, 160, 185},
    { 111, 135, 160, 185},
    { 105, 128, 152, 175},
    { 105, 128, 152, 175},
    { 100, 122, 144, 166},
    { 100, 122, 144, 166},
    {  95, 116, 137, 158},
    {  95, 116, 137, 158},
    {  90, 110, 130, 150},
    {  90, 110, 130, 150},
    {  85, 104, 123, 142},
    {  85, 104, 123, 142},
    {  81,  99, 117, 135},
    {  81,  99, 117, 135},
    {  77,  94, 111, 128},
    {  77,  94, 111, 128},
    {  73,  89, 105, 122},
    {  73,  89, 105, 122},
    {  69,  85, 100, 116},
    {  69,  85, 100, 116},
    {  66,  80,  95, 110},
    {  66,  80,  95, 110},
    {  62,  76,  90, 104},
    {  62,  76,  90, 104},
    {  59,  72,  86,  99},
    {  59,  72,  86,  99},
    {  56,  69,  81,  94},
    {  56,  69,  81,  94},
    {  53,  65,  77,  89},
    {  53,  65,  77,  89},
    {  51,  62,  73,  85},
    {  51,  62,  73,  85},
    {  48,  59,  69,  80},
    {  48,  59,  69,  80},
    {  46,  56,  66,  76},
    {  46,  56,  66,  76},
    {  43,  53,  63,  72},
    {  43,  53,  63,  72},
    {  41,  50,  59,  69},
    {  41,  50,  59,  69},
    {  39,  48,  56,  65},
    {  39,  48,  56,  65},
    {  37,  45,  54,  62},
    {  37,  45,  54,  62},
    {  35,  43,  51,  59},
    {  35,  43,  51,  59},
    {  33,  41,  48,  56},
    {  33,  41,  48,  56},
    {  32,  39,  46,  53},
    {  32,  39,  46,  53},
    {  30,  37,  43,  50},
    {  30,  37,  43,  50},
    {  29,  35,  41,  48},
    {  29,  35,  41,  48},
    {  27,  33,  39,  45},
    {  27,  33,  39,  45},
    {  26,  31,  37,  43},
    {  26,  31,  37,  43},
    {  24,  30,  35,  41},
    {  24,  30,  35,  41},
    {  23,  28,  33,  39},
    {  23,  28,  33,  39},
    {  22,  27,  32,  37},
    {  22,  27,  32,  37},
    {  21,  26,  30,  35},
    {  21,  26,  30,  35},
    {  20,  24,  29,  33},
    {  20,  24,  29,  33},
    {  19,  23,  27,  31},
    {  19,  23,  27,  31},
    {  18,  22,  26,  30},
    {  18,  22,  26,  30},
    {  17,  21,  25,  28},
    {  17,  21,  25,  28},
    {  16,  20,  23,  27},
    {  16,  20,  23,  27},
    {  15,  19,  22,  25},
    {  15,  19,  22,  25},
    {  14,  18,  21,  24},
    {  14,  18,  21,  24},
    {  14,  17,  20,  23},
    {  14,  17,  20,  23},
    {  13,  16,  19,  22},
    {  13,  16,  19,  22},
    {  12,  15,  18,  21},
    {  12,  15,  18,  21},
    {  12,  14,  17,  20},
    {  12,  14,  17,  20},
    {  11,  14,  16,  19},
    {  11,  14,  16,  19},
    {  11,  13,  15,  18},
    {  11,  13,  15,  18},
    {  10,  12,  15,  17},
    {  10,  12,  15,  17},
    {  10,  12,  14,  16},
    {  10,  12,  14,  16},
    {   9,  11,  13,  15},
    {   9,  11,  13,  15},
    {   9,  11,  12,  14},
    {   9,  11,  12,  14},
    {   8,  10,  12,  14},
    {   8,  10,  12,  14},
    {   8,   9,  11,  13},
    {   8,   9,  11,  13},
    {   7,   9,  11,  12},
    {   7,   9,  11,  12},
    {   7,   9,  10,  12},
    {   7,   9,  10,  12},
    {   7,   8,  10,  11},
    {   7,   8,  10,  11},
    {   6,   8,   9,  11},
    {   6,   8,   9,  11},
    {   6,   7,   9,  10},
    {   6,   7,   9,  10},
    {   6,   7,   8,   9},
    {   6,   7,   8,   9},
    {   2,   2,   2,   2},
    {   2,   2,   2,   2}
};

#define DECL(val) \
    (val) * 2, (val) * 2 + 1

// State transition table for MPS path. HEVC spec 9.3.4.3.2.2
const
Ipp8u transIdxMPSH265[] =
{
    DECL( 1), DECL( 2), DECL( 3), DECL( 4), DECL( 5), DECL( 6), DECL( 7), DECL( 8),
    DECL( 9), DECL(10), DECL(11), DECL(12), DECL(13), DECL(14), DECL(15), DECL(16),
    DECL(17), DECL(18), DECL(19), DECL(20), DECL(21), DECL(22), DECL(23), DECL(24),
    DECL(25), DECL(26), DECL(27), DECL(28), DECL(29), DECL(30), DECL(31), DECL(32),
    DECL(33), DECL(34), DECL(35), DECL(36), DECL(37), DECL(38), DECL(39), DECL(40),
    DECL(41), DECL(42), DECL(43), DECL(44), DECL(45), DECL(46), DECL(47), DECL(48),
    DECL(49), DECL(50), DECL(51), DECL(52), DECL(53), DECL(54), DECL(55), DECL(56),
    DECL(57), DECL(58), DECL(59), DECL(60), DECL(61), DECL(62), DECL(62), DECL(63)
};

// State transition table for LPS path. HEVC spec 9.3.4.3.2.2
const
Ipp8u transIdxLPSH265[] =
{
    1,   0,   DECL( 0), DECL( 1), DECL( 2), DECL( 2), DECL( 4), DECL( 4), DECL( 5),
    DECL( 6), DECL( 7), DECL( 8), DECL( 9), DECL( 9), DECL(11), DECL(11), DECL(12),
    DECL(13), DECL(13), DECL(15), DECL(15), DECL(16), DECL(16), DECL(18), DECL(18),
    DECL(19), DECL(19), DECL(21), DECL(21), DECL(22), DECL(22), DECL(23), DECL(24),
    DECL(24), DECL(25), DECL(26), DECL(26), DECL(27), DECL(27), DECL(28), DECL(29),
    DECL(29), DECL(30), DECL(30), DECL(30), DECL(31), DECL(32), DECL(32), DECL(33),
    DECL(33), DECL(33), DECL(34), DECL(34), DECL(35), DECL(35), DECL(35), DECL(36),
    DECL(36), DECL(36), DECL(37), DECL(37), DECL(37), DECL(38), DECL(38), DECL(63)
};

} // namespace UMC_HEVC_DECODER

#endif //__UMC_H264_DEC_INIT_TABLES_CABAC_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
