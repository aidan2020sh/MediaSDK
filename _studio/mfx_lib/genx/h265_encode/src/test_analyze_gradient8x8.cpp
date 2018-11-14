// Copyright (c) 2014-2018 Intel Corporation
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

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_hevce_analyze_gradient8x8_hsw_isa.h"
#include "../include/genx_hevce_analyze_gradient8x8_bdw_isa.h"

#ifdef CMRT_EMU
extern "C"
void AnalyzeGradient8x8(SurfaceIndex SURF_SRC, SurfaceIndex SURF_GRADIENT);
#endif //CMRT_EMU

namespace {
int RunGpu(const mfxU8 *inData, mfxU16 *outData8x8);
int RunCpu(const mfxU8 *inData, mfxU32 *outData, mfxU32 blockSize); // mfxU32 for CPU to test mfxU16 overflow on GPU
int Compare(mfxU16 *outDataGpu, mfxU32 *outDataCpu, mfxU32 blockSize);
};

int main()
{
    mfxI32 outData8x8Size = WIDTH * HEIGHT * 40 / 64;
    mfxU16 *output8x8Gpu = new mfxU16[outData8x8Size];
    mfxU32 *output8x8Cpu = new mfxU32[outData8x8Size];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    mfxU8 *input = new mfxU8[WIDTH * HEIGHT];
    mfxI32 bytesRead = fread(input, 1, WIDTH * HEIGHT, f);
    if (bytesRead != WIDTH * HEIGHT)
        return printf("FAILED to read frame from yuv file\n"), 1;
    fclose(f);

    res = RunGpu(input, output8x8Gpu);
    CHECK_ERR(res);

    res = RunCpu(input, output8x8Cpu, 8);
    CHECK_ERR(res);

    res = Compare(output8x8Gpu, output8x8Cpu, 8);
    CHECK_ERR(res);

    delete [] input;
    delete [] output8x8Gpu;
    delete [] output8x8Cpu;

    return printf("passed\n"), 0;
}

namespace {

int RunGpu(const mfxU8 *inData, mfxU16 *outData8x8)
{
    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_hevce_analyze_gradient8x8_hsw, sizeof(genx_hevce_analyze_gradient8x8_hsw), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(AnalyzeGradient8x8), kernel);
    CHECK_CM_ERR(res);

    CmSurface2D *input = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, input);
    CHECK_CM_ERR(res);

    res = input->WriteSurface(inData, NULL);
    CHECK_CM_ERR(res);

    size_t output8x8Size = WIDTH * HEIGHT * 40 / 64 * sizeof(mfxU16);
    mfxU16 *output8x8Sys = (mfxU16 *)CM_ALIGNED_MALLOC(output8x8Size, 0x1000);
    CmBufferUP *output8x8Cm = 0;
    res = device->CreateBufferUP(output8x8Size, output8x8Sys, output8x8Cm);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInput = 0;
    SurfaceIndex *idxOutput4x4Cm = 0;
    SurfaceIndex *idxOutput8x8Cm = 0;
    res = input->GetIndex(idxInput);
    CHECK_CM_ERR(res);
    res = output8x8Cm->GetIndex(idxOutput8x8Cm);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxInput), idxInput);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxOutput8x8Cm), idxOutput8x8Cm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(WIDTH), &WIDTH);
    CHECK_CM_ERR(res);

    mfxU32 tsWidth = WIDTH / 8;
    mfxU32 tsHeight = HEIGHT / 8;
    res = kernel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);

    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);

    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);

    res = task->AddKernel(kernel);
    CHECK_CM_ERR(res);

    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);

    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    memcpy(outData8x8, output8x8Sys, output8x8Size);

#ifndef CMRT_EMU
    printf("TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    queue->DestroyEvent(e);
    device->DestroyBufferUP(output8x8Cm);
    CM_ALIGNED_FREE(output8x8Sys);
    device->DestroySurface(input);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
    ::DestroyCmDevice(device);

    return PASSED;
}

mfxU8 FindNormalAngle(mfxI32 dx, mfxI32 dy)
{
    static const mfxU8 MODE[257] = {
         2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
         3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
         4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
         5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
         6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,
         7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,  7,
         8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
         9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
        10, 10, 10, 10, 10, 10, 10, 10, 10,
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    };

    if (dx == 0 && dy == 0)
        return 0;

    if (abs(dx) >= abs(dy)) {
        mfxI32 tabIdx = (dy * 128 + dx / 2) / dx;
        return MODE[tabIdx + 128];
    }
    else {
        mfxI32 tabIdx = (dx * 128 + dy / 2) / dy;
        return 36 - MODE[tabIdx + 128];
    }
}

mfxU8 GetPel(const mfxU8 *frame, mfxI32 x, mfxI32 y)
{
    x = CLIPVAL(x, 0, WIDTH - 1);
    y = CLIPVAL(y, 0, HEIGHT - 1);
    return frame[y * WIDTH + x];
}

float atan2_fast(float y, float x)
{
    double a0;
    double a1;
    float atan2;
    const float CM_CONST_PI = 3.14159f;
    const float M_DBL_EPSILON = 0.00001f;

    if (y >= 0) {
        a0 = CM_CONST_PI * 0.5;
        a1 = 0;
    }
    else
    {
        a0 = CM_CONST_PI * 1.5;
        a1 = CM_CONST_PI * 2;
    }

    if (x < 0) {
        a1 = CM_CONST_PI;
    }

    float xy = x * y;
    float x2 = x * x;
    float y2 = y * y;

    a0 -= (xy / (y2 + 0.28f * x2 + M_DBL_EPSILON));
    a1 += (xy / (x2 + 0.28f * y2 + M_DBL_EPSILON));

    if (y2 <= x2) {
        atan2 = a1;
    }
    else {
        atan2 = a0;
    }

    //atan2 = matrix<float,R,C>(atan2, (flags & SAT));
    return atan2;
}

void BuildHistogram2(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
                    mfxU32 *histogram)
{
    for (mfxI32 y = blockY; y < blockY + blockSize; y++) {
        for (mfxI32 x = blockX; x < blockX + blockSize; x++) {
            mfxI16 dy = GetPel(frame, x-1, y-1) + 2 * GetPel(frame, x-1, y+0) + GetPel(frame, x-1, y+1)
                       -GetPel(frame, x+1, y-1) - 2 * GetPel(frame, x+1, y+0) - GetPel(frame, x+1, y+1);
            mfxI16 dx = GetPel(frame, x-1, y+1) + 2 * GetPel(frame, x+0, y+1) + GetPel(frame, x+1, y+1)
                       -GetPel(frame, x-1, y-1) - 2 * GetPel(frame, x+0, y-1) - GetPel(frame, x+1, y-1);
            //mfxU8  ang = FindNormalAngle(dx, dy);

            float fdx = dx;
            float fdy = dy;
            float y2 = fdy * fdy;
            float x2 = fdx * fdx;
            float angf;

            if (dx == 0 && dy == 0) {
                angf = 0;
            }
            else {
                angf = atan2_fast(fdy, fdx);
            }

            const float CM_CONST_PI = 3.14159f;
            if (y2 > x2 && dy > 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy >= 0) angf += CM_CONST_PI;
            if (y2 <= x2 && dx > 0 && dy < 0) angf -= CM_CONST_PI;

            angf -= 0.75 * CM_CONST_PI;
            mfxI8 ang2 = angf * 10.504226 + 2;
            if (dx == 0 && dy == 0) ang2 = 0;

            mfxU16 amp = (mfxI16)(abs(dx) + abs(dy));
            histogram[ang2] += amp;
        }
    }
}

void BuildHistogram(const mfxU8 *frame, mfxI32 blockX, mfxI32 blockY, mfxI32 blockSize,
                    mfxU32 *histogram)
{
    for (mfxI32 y = blockY; y < blockY + blockSize; y++) {
        for (mfxI32 x = blockX; x < blockX + blockSize; x++) {
            mfxI16 dy = GetPel(frame, x-1, y-1) + 2 * GetPel(frame, x-1, y+0) + GetPel(frame, x-1, y+1)
                       -GetPel(frame, x+1, y-1) - 2 * GetPel(frame, x+1, y+0) - GetPel(frame, x+1, y+1);
            mfxI16 dx = GetPel(frame, x-1, y+1) + 2 * GetPel(frame, x+0, y+1) + GetPel(frame, x+1, y+1)
                       -GetPel(frame, x-1, y-1) - 2 * GetPel(frame, x+0, y-1) - GetPel(frame, x+1, y-1);
            mfxU8  ang = FindNormalAngle(dx, dy);
            mfxU16 amp = (mfxI16)(abs(dx) + abs(dy));
            histogram[ang] += amp;
        }
    }
}

int RunCpu(const mfxU8 *inData, mfxU32 *outData, mfxU32 blockSize)
{
    for (mfxI32 y = 0; y < HEIGHT / blockSize; y++, outData += (WIDTH / blockSize) * 40) {
        for (mfxI32 x = 0; x < WIDTH / blockSize; x++) {
            memset(outData + x * 40, 0, sizeof(*outData) * 40);
            BuildHistogram2(inData, x * blockSize, y * blockSize, blockSize, outData + x * 40);
        }
    }

    return PASSED;
}


int Compare(mfxU16 *outDataGpu, mfxU32 *outDataCpu, mfxU32 blockSize)
{
    for (mfxI32 y = 0; y < HEIGHT / blockSize; y++) {
        for (mfxI32 x = 0; x < WIDTH / blockSize; x++) {
            mfxU16 *histogramGpu = outDataGpu + (y * (WIDTH / blockSize) + x) * 40;
            mfxU32 *histogramCpu = outDataCpu + (y * (WIDTH / blockSize) + x) * 40;
            for (mfxI32 mode = 0; mode < 35; mode++) {
                if (histogramGpu[mode] != histogramCpu[mode]) {
                    printf("bad histogram value (%d != %d) for mode %d for block%dx%d (x,y)=(%d,%d)\n",
                           histogramGpu[mode], histogramCpu[mode], mode, blockSize, blockSize, x, y);
                    return FAILED;
                }
            }
        }
    }
    return PASSED;
}
};
