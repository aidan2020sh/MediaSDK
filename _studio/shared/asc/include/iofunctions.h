//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2017-2018 Intel Corporation. All Rights Reserved.
//
#ifndef _IOFUNCTIONS_H_
#define _IOFUNCTIONS_H_

#include "asc_structures.h"
#include "asc.h"
using namespace ns_asc;

#ifdef _MSVC_LANG
#pragma warning(disable:4505)
#endif

static mfxF64 TimeMeasurement(LARGE_INTEGER start, LARGE_INTEGER stop, LARGE_INTEGER frequency) {
    return((stop.QuadPart - start.QuadPart) * mfxF64(1000.0) / frequency.QuadPart);
}

#ifdef _MSVC_LANG
#pragma warning(default:4505)
#endif

void TimeStart(ASCTime* timer);
void TimeStart(ASCTime* timer, int index);
void TimeStop(ASCTime* timer);
mfxF64 CatchTime(ASCTime *timer, const char* message);
mfxF64 CatchTime(ASCTime *timer, int index, const char* message);
mfxF64 CatchTime(ASCTime *timer, int indexInit, int indexEnd, const char* message);

void imageInit(ASCYUV *buffer);
void nullifier(ASCimageData *Buffer);
void ImDetails_Init(ASCImDetails *Rdata);
mfxStatus ASCTSCstat_Init(ASCTSCstat **logic);


#endif //_IOFUNCTIONS_H_