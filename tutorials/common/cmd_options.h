// Copyright (c) 2019-2020 Intel Corporation
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

#pragma once

#include <stdio.h>
#include "mfxdefs.h"
#include "mfxstructures.h"

#define OPTION_IMPL             0x001
#define OPTION_ADAPTER          0x002
#define OPTION_GEOMETRY         0x004
#define OPTION_BITRATE          0x008
#define OPTION_FRAMERATE        0x010
#define OPTION_MEASURE_LATENCY  0x020

#define OPTIONS_DECODE \
    (OPTION_IMPL | OPTION_ADAPTER)

#define OPTIONS_ENCODE \
    (OPTION_IMPL | OPTION_ADAPTER | OPTION_GEOMETRY | OPTION_BITRATE | OPTION_FRAMERATE)

#define OPTIONS_VPP \
    (OPTION_IMPL | OPTION_ADAPTER | OPTION_GEOMETRY)

#define OPTIONS_TRANSCODE \
    (OPTION_IMPL | OPTION_ADAPTER | OPTION_BITRATE | OPTION_FRAMERATE)

#define MSDK_MAX_PATH 280

struct CmdOptionsCtx {
    // bitmask of the options accepted by the program
    unsigned int options;
    // program name, if not set will be corrected to argv[0]
    const char* program;
    // function to print program usage, can be NULL
    void (*usage)(CmdOptionsCtx* ctx);
};

struct CmdOptionsValues {
    mfxIMPL impl; // OPTION_IMPL
    mfxU16  adapterType; // OPTION_ADAPTER
    mfxU32  adapterNum;

    char SourceName[MSDK_MAX_PATH]; // OPTION_FSOURCE
    char SinkName[MSDK_MAX_PATH];   // OPTION_FSINK

    mfxU16 Width; // OPTION_GEOMETRY
    mfxU16 Height;

    mfxU16 Bitrate; // OPTION_BITRATE

    mfxU16 FrameRateN; // OPTION_FRAMERATE
    mfxU16 FrameRateD;

    bool MeasureLatency; // OPTION_MEASURE_LATENCY
    bool c10bit; // The flag for HEVC 10bit processing

    bool VideoMemory;
    mfxU32 VppOutFOURCC;
    mfxU16 VppOutCrop;
    char ParFile[MSDK_MAX_PATH];
    mfxU16 VppHwPath; // HW path selection
};

struct CmdOptions {
    CmdOptionsCtx ctx;
    CmdOptionsValues values;
};

void ParseOptions(int argc, char** argv, CmdOptions* options);
