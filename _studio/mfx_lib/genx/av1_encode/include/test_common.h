// Copyright (c) 2014-2019 Intel Corporation
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

#include "tuple"

typedef char mfxI8;
typedef unsigned char mfxU8;
typedef short mfxI16;
typedef unsigned short mfxU16;
typedef int mfxI32;
typedef unsigned int mfxU32;
typedef __int64 mfxI64;
typedef unsigned __int64 mfxU64;
typedef double mfx64F;
typedef float mfx32F;

typedef struct {
    mfxI16  x;
    mfxI16  y;
} mfxI16Pair;

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define ABS(a)    (((a) < 0) ? (-(a)) : (a))

#define CLIPVAL(VAL, MINVAL, MAXVAL) MAX(MINVAL, MIN(MAXVAL, VAL))
#define CHECK_CM_ERR(ERR) if ((ERR) != CM_SUCCESS) { printf("FAILED at file: %s, line: %d, cmerr: %d\n", __FILE__, __LINE__, ERR); return FAILED; }
#define CHECK_ERR(ERR) if ((ERR) != PASSED) { printf("FAILED at file: %s, line: %d\n", __FILE__, __LINE__); return (ERR); }
#define DIVUP(a, b) ((a+b-1)/b)
#define ROUNDUP(a, b) DIVUP(a,b)*b


#define BORDER          4
#define WIDTHB (WIDTH + BORDER*2)
#define HEIGHTB (HEIGHT + BORDER*2)

enum { PASSED, FAILED };

#if !defined CMRT_EMU
//#define TEST_8K
#ifdef TEST_8K
const mfxI32 WIDTH  = 8192;
const mfxI32 HEIGHT = 4096;
const mfxI8 YUV_NAME[] = "C:/yuv/8K/rally_8192x4096_30.yuv";
#else
const mfxI32 WIDTH  = 1920;
const mfxI32 HEIGHT = 1088;
const mfxI8 YUV_NAME[] = "C:/yuv/1080p/BasketballDrive_1920x1080p_500_50.yuv";
#endif
#else   // CMRT_EMU
const mfxI32 WIDTH  = 416;
const mfxI32 HEIGHT = 240;
const mfxI8 YUV_NAME[] = "C:/yuv/JCTVC-G1200/240p/RaceHorses_416x240p_300_30.yuv";
#endif

struct ParamCopyCPUToGPUStride {
    ParamCopyCPUToGPUStride(CmSurface2D *gpumem_, const uint8_t *cpumem_, int32_t pitch_) : gpumem(gpumem_), cpumem(cpumem_), pitch(pitch_) {}
    CmSurface2D *gpumem;
    const uint8_t *cpumem;
    int32_t       pitch;
};

inline mfxU64 GetAccurateGpuTime(CmQueue *queue, CmTask *task, CmThreadSpace *ts)
{
    CmEvent *e = NULL;
    mfxU64 mintime = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++)
            queue->Enqueue(task, e, ts);
        e->WaitForTaskFinished();
        mfxU64 time;
        e->GetExecutionTime(time);
        mintime = MIN(mintime, time);
    }
    return mintime;
}

inline std::tuple<mfxU64,mfxU64,mfxU64> GetAccurateGpuTime(CmQueue *queue, const ParamCopyCPUToGPUStride &copyParam, CmTask *task, CmThreadSpace *ts)
{
    CmEvent *e1 = NULL, *e2 = NULL;
    mfxU64 mintimeTot = mfxU64(-1);
    mfxU64 mintime1 = mfxU64(-1);
    mfxU64 mintime2 = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++) {
            queue->EnqueueCopyCPUToGPUFullStride(copyParam.gpumem, copyParam.cpumem, copyParam.pitch, 0, 0, e1);
            queue->Enqueue(task, e2, ts);
        }
        e2->WaitForTaskFinished();
        mfxU64 time1, time2;
        e1->GetExecutionTime(time1);
        e2->GetExecutionTime(time2);
        if (mintimeTot > time1 + time2) {
            mintimeTot = time1 + time2;
            mintime1 = time1;
            mintime2 = time2;
        }
    }
    return std::make_tuple(mintime1, mintime2, mintimeTot);
}

inline mfxU64 GetAccurateGpuTime_ThreadGroup(CmQueue *queue, CmTask *task, CmThreadGroupSpace* tgs)
{
    CmEvent *e = NULL;
    mfxU64 mintime = mfxU64(-1);
    for (int i=0; i<10; i++) {
        for (int j=0; j<10; j++)
            queue->EnqueueWithGroup(task, e, tgs);
        e->WaitForTaskFinished();
        mfxU64 time;
        e->GetExecutionTime(time);
        mintime = MIN(mintime, time);
    }
    return mintime;
}

struct VmeSearchPath // sizeof=58
{
    mfxU8 sp[56];
    mfxU8 maxNumSu;
    mfxU8 lenSp;
};

struct Me2xControl_old // sizeof=64
{
    VmeSearchPath searchPath;
    mfxU8  reserved[2];
    mfxU16 width;
    mfxU16 height;
};

struct MeControl // sizeof=120
{
    mfxU8  longSp[32];          // 0 B
    mfxU8  shortSp[32];         // 32 B
    mfxU8  longSpLenSp;         // 64 B
    mfxU8  longSpMaxNumSu;      // 65 B
    mfxU8  shortSpLenSp;        // 66 B
    mfxU8  shortSpMaxNumSu;     // 67 B
    mfxU16 width;               // 34 W
    mfxU16 height;              // 35 W
    mfxU8  mvCost[5][8];         // 72 B (1x), 80 B (2x), 88 B (4x), 96 B (8x) 104 B (16x)
    mfxU8  mvCostScaleFactor[5]; // 112 B (1x), 113 B (2x), 114 B (4x), 115 B (8x) 116 B (16x)
    mfxU8  reserved[3];          // 117 B
};

inline void SetupMeControl_old(Me2xControl_old &ctrl, int width, int height)
{
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

    memcpy(ctrl.searchPath.sp, Diamond, sizeof(ctrl.searchPath));
    ctrl.searchPath.lenSp = 16;
    ctrl.searchPath.maxNumSu = 57;
    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
}

inline void SetupMeControlShortPath_old(Me2xControl_old &ctrl, int width, int height)
{
    const mfxU8 SmallPath[3] = { 0x0F, 0xF0, 0x01 };
    memcpy(ctrl.searchPath.sp, SmallPath, sizeof(SmallPath));
    ctrl.searchPath.lenSp = 4;
    ctrl.searchPath.maxNumSu = 9;
    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;
}


inline mfxU8 ToU4U4(mfxU16 val) {
    if (val > 4095)
        val = 4095;
    mfxU16 shift = 0;
    mfxU16 base = val;
    mfxU16 rem = 0;
    while (base > 15) {
        rem += (base & 1) << shift;
        base = (base >> 1);
        shift++;
    }
    base += (rem << 1 >> shift);
    return (mfxU8)(base | (shift << 4));
}


inline void SetupMeControl(MeControl &ctrl, int width, int height, double lambda)
{
    const mfxU8 Diamond[56] = {
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

    memcpy(ctrl.longSp, Diamond, MIN(sizeof(Diamond), sizeof(ctrl.longSp)));
    ctrl.longSpLenSp = 16;
    ctrl.longSpMaxNumSu = 57;

    const mfxU8 ShortPath[4] = {
        0x0F, 0xF0, 0x01, 0x00
    };

    memcpy(ctrl.shortSp, ShortPath, MIN(sizeof(ShortPath), sizeof(ctrl.shortSp)));
    ctrl.shortSpLenSp = 4;
    ctrl.shortSpMaxNumSu = 9;

    ctrl.width = (mfxU16)width;
    ctrl.height = (mfxU16)height;

    const mfxU8 MvBits[4][8] = { // mvCostScale = qpel, hpel, ipel 2pel
        { 1, 4, 5, 6, 8, 10, 12, 14 },
        { 1, 5, 6, 8, 10, 12, 14, 16 },
        { 1, 6, 8, 10, 12, 14, 16, 18 },
        { 1, 8, 10, 12, 14, 16, 18, 20 }
    };

    ctrl.mvCostScaleFactor[0] = 2; // int-pel cost table precision
    ctrl.mvCostScaleFactor[1] = 2;
    ctrl.mvCostScaleFactor[2] = 2;
    ctrl.mvCostScaleFactor[3] = 2;
    ctrl.mvCostScaleFactor[4] = 2;

    const mfxU8 *mvBits = MvBits[ctrl.mvCostScaleFactor[0]];
    for (int32_t i = 0; i < 8; i++)
        ctrl.mvCost[0][i] = ToU4U4(mfxU16(0.5 + lambda * mvBits[i]));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[1]];
    //for (int32_t i = 0; i < 8; i++)
    //    ctrl.mvCost[1][i] = ToU4U4(mfxU16(0.5 + lambda /  4 * (1 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[2]];
    //for (int32_t i = 0; i < 8; i++)
    //    ctrl.mvCost[2][i] = ToU4U4(mfxU16(0.5 + lambda / 16 * (2 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[3]];
    //for (int32_t i = 0; i < 8; i++)
    //    ctrl.mvCost[3][i] = ToU4U4(mfxU16(0.5 + lambda / 64 * (3 * (i > 0) + mvBits[i])));

    //mvBits = MvBits[ctrl.mvCostScaleFactor[4]];
    //for (int32_t i = 0; i < 8; i++)
    //    ctrl.mvCost[4][i] = ToU4U4(mfxU16(0.5 + lambda / 64 * (4 * (i > 0) + mvBits[i])));

    memset(ctrl.mvCost, 0, sizeof(ctrl.mvCost));

    mfxU8 MvCostHumanFriendly[5][8];
    for (int32_t i = 0; i < 5; i++)
        for (int32_t j = 0; j < 8; j++)
            MvCostHumanFriendly[i][j] = (ctrl.mvCost[i][j] & 0xf) << ((ctrl.mvCost[i][j] >> 4) & 0xf);
}

inline const char *to_cstring(GPU_PLATFORM platform)
{
    switch (platform) {
    default:
    case PLATFORM_INTEL_UNKNOWN: return "unknown";
    case PLATFORM_INTEL_SNB:     return "SNB";
    case PLATFORM_INTEL_IVB:     return "IVB";
    case PLATFORM_INTEL_HSW:     return "HSW";
    case PLATFORM_INTEL_BDW:     return "BDW";
    case PLATFORM_INTEL_VLV:     return "VLV";
    case PLATFORM_INTEL_CHV:     return "CHV";
    case PLATFORM_INTEL_SKL:     return "SKL";
    case PLATFORM_INTEL_BXT:     return "BXT";
    case PLATFORM_INTEL_CNL:     return "CNL";
    case PLATFORM_INTEL_KBL:     return "KBL";
    }
}

inline const char *to_cstring(GPU_GT_PLATFORM gt_platform)
{
    switch (gt_platform) {
    default:
    case PLATFORM_INTEL_GT_UNKNOWN: return "unknown";
    case PLATFORM_INTEL_GT1:        return "GT1";
    case PLATFORM_INTEL_GT2:        return "GT2";
    case PLATFORM_INTEL_GT3:        return "GT3";
    case PLATFORM_INTEL_GT4:        return "GT4";
    case PLATFORM_INTEL_GT1_5:      return "GT1_5";
    }
}

inline int print_hw_caps()
{
    mfxU32 version = 0;
    CmDevice *device = nullptr;
    int32_t res = ::CreateCmDevice(device, version);
    if (res != CM_SUCCESS)
        printf("CreateCmDevice failed with %d\n", res);
    CHECK_CM_ERR(res);

    GPU_PLATFORM hw_type;
    GPU_GT_PLATFORM gt_platform;
    unsigned int hw_thread_count;
    size_t size;

    size = sizeof(unsigned int);
    res = device->GetCaps(CAP_GPU_PLATFORM, size, &hw_type);
    if (res != CM_SUCCESS)
        printf("GetCaps(CAP_GPU_PLATFORM) failed with %d\n", res);
    CHECK_CM_ERR(res);
    size = sizeof(unsigned int);
    res = device->GetCaps(CAP_GT_PLATFORM, size, &gt_platform);
    if (res != CM_SUCCESS)
        printf("GetCaps(CAP_GT_PLATFORM) failed with %d\n", res);
    CHECK_CM_ERR(res);
    size = sizeof(unsigned int);
    res = device->GetCaps(CAP_HW_THREAD_COUNT, size, &hw_thread_count);
    if (res != CM_SUCCESS)
        printf("GetCaps(CAP_HW_THREAD_COUNT) failed with %d\n", res);
    CHECK_CM_ERR(res);

    printf("Running on %s %s with %u HW threads\n", to_cstring(hw_type), to_cstring(gt_platform), hw_thread_count);

    ::DestroyCmDevice(device);
    return PASSED;
}
