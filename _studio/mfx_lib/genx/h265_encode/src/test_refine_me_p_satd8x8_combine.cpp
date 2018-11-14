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
#include <vector>
#include <algorithm>
#include "../include/test_common.h"
#include "../include/genx_hevce_refine_me_p_satd8x8_combine_bdw_isa.h"
#include "../include/genx_hevce_refine_me_p_satd8x8_combine_hsw_isa.h"
#include "../include/genx_hevce_interpolate_frame_bdw_isa.h"
#include "../include/genx_hevce_interpolate_frame_hsw_isa.h"

#ifdef CMRT_EMU
extern "C"
void InterpolateFrameWithBorder(SurfaceIndex SURF_FPEL, SurfaceIndex SURF_HPEL_HORZ,
                      SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG);
extern "C"
void RefineMePCombineSATD8x8(SurfaceIndex SURF_MBDIST, SurfaceIndex SURF_MV,
                    SurfaceIndex SURF_SRC, SurfaceIndex SURF_REF, SurfaceIndex SURF_HPEL_HORZ,
                    SurfaceIndex SURF_HPEL_VERT, SurfaceIndex SURF_HPEL_DIAG);
#endif //CMRT_EMU

#define MODE_SIZE       3
#define GPU_SADSIZE     2 

const mfxI32 BLOCK_W = 16;
const mfxI32 BLOCK_H = 16;
#define INTERPOLATE_KERNEL_NAME InterpolateFrameWithBorder
#define REFINEME_KERNEL_NAME RefineMePCombineSATD8x8


struct OutputData
{
    mfxU32 sad[16];
};

namespace {
int RunGpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData);
int RunCpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData);
}

int main()
{
    mfxI32 numBlocksHor = (WIDTH + BLOCK_W - 1) / BLOCK_W;
    mfxI32 numBlocksVer = (HEIGHT + BLOCK_H - 1) / BLOCK_H;
    OutputData *outputGpu = new OutputData[numBlocksHor * numBlocksVer * GPU_SADSIZE];
    OutputData *outputCpu = new OutputData[numBlocksHor * numBlocksVer * MODE_SIZE];
    mfxU8  *src = new mfxU8[WIDTH * HEIGHT];
    mfxU8  *ref = new mfxU8[WIDTH * HEIGHT];
    mfxI16Pair *mv = new mfxI16Pair[numBlocksHor * numBlocksVer * MODE_SIZE];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return printf("FAILED to open yuv file\n"), 1;
    if (fread(src, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read first frame
        return printf("FAILED to read first frame from yuv file\n"), 1;
    if (fseek(f, WIDTH * HEIGHT / 2, SEEK_CUR) != 0) // skip chroma
        return printf("FAILED to skip chroma\n"), 1;
    if (fread(ref, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read second frame
        return printf("FAILED to read second frame from yuv file\n"), 1;
    fclose(f);

    // fill motion vector field
    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++){
            for (mfxI32 mode = 0; mode < MODE_SIZE; mode++){
                mfxI32 index = (y * numBlocksHor + x) * MODE_SIZE + mode;
                mv[index].x = (mfxI16)((x + mode - numBlocksHor / 2) * 2);
                mv[index].y = (mfxI16)((y + mode - numBlocksVer / 2) * 2);
            }
        }
    }

    res = RunGpu(src, ref, mv, outputGpu);
    CHECK_ERR(res);

    res = RunCpu(src, ref, mv, outputCpu);
    CHECK_ERR(res);

    // compare output
    for (mfxI32 y = 0; y < numBlocksVer-1; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            for (mfxI32 mode = 0; mode < MODE_SIZE; mode++) {
                mfxU32 indexCPU = (y * numBlocksHor + x)*MODE_SIZE + mode;
                mfxU32 indexGPU = (y * numBlocksHor + x)*GPU_SADSIZE;
                mfxU32 *sadGpu = outputGpu[indexGPU].sad + mode * 9;
                mfxU32 *sadCpu = outputCpu[indexCPU].sad;
                for (mfxI32 i = 0; i < 9; i++) {
                    if (sadGpu[i] != sadCpu[i]) {
                        printf("bad sad value (%d != %d) for idx %d for block (x,y)=(%d,%d), mode %d\n",
                               sadGpu[i], sadCpu[i], i, x, y, mode);
                        return 1;
                    }
                }
            }
        }
    }

    delete [] outputGpu;
    delete [] outputCpu;
    delete [] src;
    delete [] ref;
    delete [] mv;

    return printf("passed\n"), 0;
}

namespace {
int RunGpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData)
{
    mfxI32 numBlocksHor = (WIDTH + BLOCK_W - 1) / BLOCK_W;
    mfxI32 numBlocksVer = (HEIGHT + BLOCK_H - 1) / BLOCK_H;

    mfxU32 version = 0;
    CmDevice *device = 0;
    mfxI32 res = ::CreateCmDevice(device, version);
    CHECK_CM_ERR(res);

    CmProgram *programRefine = 0;
    res = device->LoadProgram((void *)genx_hevce_refine_me_p_satd8x8_combine_hsw, sizeof(genx_hevce_refine_me_p_satd8x8_combine_hsw), programRefine);
    CHECK_CM_ERR(res);

    CmProgram *programInterpFrame = 0;
    res = device->LoadProgram((void *)genx_hevce_interpolate_frame_hsw, sizeof(genx_hevce_interpolate_frame_hsw), programInterpFrame);
    CHECK_CM_ERR(res);

    CmKernel *kernelQpel = 0;
    res = device->CreateKernel(programRefine, CM_KERNEL_FUNCTION(REFINEME_KERNEL_NAME), kernelQpel);
    CHECK_CM_ERR(res);

    CmKernel *kernelHpel = 0;
    res = device->CreateKernel(programInterpFrame, CM_KERNEL_FUNCTION(InterpolateFrameWithBorder), kernelHpel);
    CHECK_CM_ERR(res);

    CmSurface2D *inSrc = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inSrc);
    CHECK_CM_ERR(res);
    res = inSrc->WriteSurface(src, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *inRef = 0;
    res = device->CreateSurface2D(WIDTH, HEIGHT, CM_SURFACE_FORMAT_A8, inRef);
    CHECK_CM_ERR(res);
    res = inRef->WriteSurface(ref, NULL);
    CHECK_CM_ERR(res);

    CmSurface2D *refH = 0;
    res = device->CreateSurface2D(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_A8, refH);
    CHECK_CM_ERR(res);
    CmSurface2D *refV = 0;
    res = device->CreateSurface2D(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_A8, refV);
    CHECK_CM_ERR(res);
    CmSurface2D *refD = 0;
    res = device->CreateSurface2D(WIDTHB, HEIGHTB, CM_SURFACE_FORMAT_A8, refD);
    CHECK_CM_ERR(res);

    CmSurface2D *inMv = 0;
    res = device->CreateSurface2D(numBlocksHor * sizeof(mfxI16Pair) * MODE_SIZE, numBlocksVer, CM_SURFACE_FORMAT_A8, inMv);
    CHECK_CM_ERR(res);
    res = inMv->WriteSurface((const mfxU8 *)mv, NULL);
    CHECK_CM_ERR(res);

    mfxU32 outputWidth = numBlocksHor * GPU_SADSIZE * sizeof(OutputData);
    mfxU32 outputPitch = 0;
    mfxU32 outputSize = 0;
    res = device->GetSurface2DInfo(outputWidth, numBlocksVer, CM_SURFACE_FORMAT_P8, outputPitch, outputSize);
    CHECK_CM_ERR(res);
    mfxU8 *outputSys = (mfxU8 *)CM_ALIGNED_MALLOC(outputSize, 0x1000);
    CmSurface2DUP *outputCm = 0;
    res = device->CreateSurface2DUP(outputWidth, numBlocksVer, CM_SURFACE_FORMAT_P8, outputSys, outputCm);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxInSrc = 0;
    res = inSrc->GetIndex(idxInSrc);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxInRef = 0;
    res = inRef->GetIndex(idxInRef);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxRefH = 0;
    res = refH->GetIndex(idxRefH);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxRefV = 0;
    res = refV->GetIndex(idxRefV);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxRefD = 0;
    res = refD->GetIndex(idxRefD);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxInMv = 0;
    res = inMv->GetIndex(idxInMv);
    CHECK_CM_ERR(res);
    SurfaceIndex *idxOutputCm = 0;
    res = outputCm->GetIndex(idxOutputCm);
    CHECK_CM_ERR(res);

    res = kernelHpel->SetKernelArg(0, sizeof(SurfaceIndex), idxInRef);
    CHECK_CM_ERR(res);
    res = kernelHpel->SetKernelArg(1, sizeof(SurfaceIndex), idxRefH);
    CHECK_CM_ERR(res);
    res = kernelHpel->SetKernelArg(2, sizeof(SurfaceIndex), idxRefV);
    CHECK_CM_ERR(res);
    res = kernelHpel->SetKernelArg(3, sizeof(SurfaceIndex), idxRefD);
    CHECK_CM_ERR(res);

    res = kernelQpel->SetKernelArg(0, sizeof(SurfaceIndex), idxOutputCm);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(1, sizeof(SurfaceIndex), idxInMv);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(2, sizeof(SurfaceIndex), idxInSrc);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(3, sizeof(SurfaceIndex), idxInRef);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(4, sizeof(SurfaceIndex), idxRefH);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(5, sizeof(SurfaceIndex), idxRefV);
    CHECK_CM_ERR(res);
    res = kernelQpel->SetKernelArg(6, sizeof(SurfaceIndex), idxRefD);
    CHECK_CM_ERR(res);

    const mfxU16 BlockW = 8;
    const mfxU16 BlockH = 8;
    mfxU32 tsWidth = (WIDTHB + BlockW - 1) / BlockW;
    mfxU32 tsHeight = (HEIGHTB + BlockH - 1) / BlockH;
    res = kernelHpel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);
    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);
    CmTask * task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelHpel);
    CHECK_CM_ERR(res);
    CmQueue *queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
    CmEvent * e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);
    queue->DestroyEvent(e);

#ifndef CMRT_EMU
    printf("InterpFrame TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);
#endif //CMRT_EMU

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);

    tsWidth  = (WIDTH  + BLOCK_W - 1) / BLOCK_W;
    tsHeight = (HEIGHT + BLOCK_H - 1) / BLOCK_H;
    res = kernelQpel->SetThreadCount(tsWidth * tsHeight);
    CHECK_CM_ERR(res);
    threadSpace = 0;
    res = device->CreateThreadSpace(tsWidth, tsHeight, threadSpace);
    CHECK_CM_ERR(res);
    res = threadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
    CHECK_CM_ERR(res);
    task = 0;
    res = device->CreateTask(task);
    CHECK_CM_ERR(res);
    res = task->AddKernel(kernelQpel);
    CHECK_CM_ERR(res);
    queue = 0;
    res = device->CreateQueue(queue);
    CHECK_CM_ERR(res);
    e = 0;
    res = queue->Enqueue(task, e, threadSpace);
    CHECK_CM_ERR(res);
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);
    queue->DestroyEvent(e);

    for (mfxI32 y = 0; y < numBlocksVer; y++)
        memcpy(outData + y * numBlocksHor * GPU_SADSIZE, outputSys + y * outputPitch, outputWidth);

    printf("Refine TIME=%.3f ms ", GetAccurateGpuTime(queue, task, threadSpace) / 1000000.0);

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);

    device->DestroySurface2DUP(outputCm);
    CM_ALIGNED_FREE(outputSys);
    device->DestroySurface(inMv);
    device->DestroySurface(inRef);
    device->DestroySurface(refH);
    device->DestroySurface(refV);
    device->DestroySurface(refD);
    device->DestroySurface(inSrc);
    device->DestroyKernel(kernelHpel);
    device->DestroyKernel(kernelQpel);
    device->DestroyProgram(programInterpFrame);
    device->DestroyProgram(programRefine);
    ::DestroyCmDevice(device);

    return PASSED;
}

template <class T>
T GetPel(const T *frame, mfxU32 pitch, mfxI32 w, mfxI32 h, mfxI32 x, mfxI32 y)
{
    x = CLIPVAL(x, 0, w - 1);
    y = CLIPVAL(y, 0, h - 1);
    return frame[y * pitch + x];
}

template <class T>
mfxI16 InterpolateHor(const T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx - 1, inty) + 5 * GetPel(p, pitch, w, h, intx + 0, inty)
           -GetPel(p, pitch, w, h, intx + 2, inty) + 5 * GetPel(p, pitch, w, h, intx + 1, inty);
}

template <class T>
mfxI16 InterpolateVer(T * p, mfxU32 pitch, mfxU32 w, mfxU32 h, mfxI32 intx, mfxI32 inty)
{
    return -GetPel(p, pitch, w, h, intx, inty - 1) + 5 * GetPel(p, pitch, w, h, intx, inty + 0)
           -GetPel(p, pitch, w, h, intx, inty + 2) + 5 * GetPel(p, pitch, w, h, intx, inty + 1);
}

mfxU8 ShiftAndSat(mfxI16 val, int shift)
{
    short offset = 1 << (shift - 1);
    return (mfxU8)CLIPVAL((val + offset) >> shift, 0, 255);
}

mfxU8 InterpolatePel(mfxU8 const * p, mfxI32 x, mfxI32 y)
{
    int intx = x >> 2;
    int inty = y >> 2;
    int dx = x & 3;
    int dy = y & 3;
    const int W = WIDTH;
    const int H = HEIGHT;
    short val = 0;

    if (dy == 0 && dx == 0) {
        val = GetPel(p, W, W, H, intx, inty);

    } else if (dy == 0) {
        val = InterpolateHor(p, W, W, H, intx, inty);
        val = ShiftAndSat(val, 3);
        if (dx == 1)
            val = ShiftAndSat(GetPel(p, W, W, H, intx + 0, inty) + val, 1);
        else if (dx == 3)
            val = ShiftAndSat(GetPel(p, W, W, H, intx + 1, inty) + val, 1);

    } else if (dx == 0) {
        val = InterpolateVer(p, W, W, H, intx, inty);
        val = ShiftAndSat(val, 3);
        if (dy == 1)
            val = ShiftAndSat(GetPel(p, W, W, H, intx, inty + 0) + val, 1);
        else if (dy == 3)
            val = ShiftAndSat(GetPel(p, W, W, H, intx, inty + 1) + val, 1);

    } else if (dx == 2) {
        short tmp[4] = {
            InterpolateHor(p, W, W, H, intx, inty - 1),
            InterpolateHor(p, W, W, H, intx, inty + 0),
            InterpolateHor(p, W, W, H, intx, inty + 1),
            InterpolateHor(p, W, W, H, intx, inty + 2)
        };
        val = InterpolateVer(tmp, 1, 1, 4, 0, 1);
        val = ShiftAndSat(val, 6);

        if (dy == 1)
            val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
        else if (dy == 3)
            val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);

    } else if (dy == 2) {
        short tmp[4] = {
            InterpolateVer(p, W, W, H, intx - 1, inty),
            InterpolateVer(p, W, W, H, intx + 0, inty),
            InterpolateVer(p, W, W, H, intx + 1, inty),
            InterpolateVer(p, W, W, H, intx + 2, inty)
        };
        val = InterpolateHor(tmp, 0, 4, 1, 1, 0);
        val = ShiftAndSat(val, 6);

        if (dx == 1)
            val = ShiftAndSat(ShiftAndSat(tmp[1], 3) + val, 1);
        else if (dx == 3)
            val = ShiftAndSat(ShiftAndSat(tmp[2], 3) + val, 1);
    }
    else {
        short hor = InterpolateHor(p, W, W, H, intx, inty + (dy >> 1));
        short ver = InterpolateVer(p, W, W, H, intx + (dx >> 1), inty);
        val = ShiftAndSat(ShiftAndSat(hor, 3) + ShiftAndSat(ver, 3), 1);
    }

    return (mfxU8)val;
}


void InterpolateBlock(const mfxU8 *ref, mfxI16Pair point, mfxI32 blockW, mfxI32 blockH, mfxU8 *block)
{
    for (mfxI32 y = 0; y < blockH; y++, block += blockW)
        for (mfxI32 x = 0; x < blockW; x++)
            block[x] = InterpolatePel(ref, point.x + x * 4, point.y + y * 4);
}

mfxU32 CalcSatd8x8OneBlock(const mfxU8 *blockSrc, const mfxU8 *blockRef, mfxI32 pitch)
{
    short diff[8][8];
    mfxU32 satd = 0;

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++)
            diff[i][j] = (short)(blockSrc[j] - blockRef[j]);
        blockSrc += pitch;
        blockRef += pitch;
    }


    for (int i = 0; i < 8; i++) {
        int t0 = diff[i][0] + diff[i][4];
        int t4 = diff[i][0] - diff[i][4];
        int t1 = diff[i][1] + diff[i][5];
        int t5 = diff[i][1] - diff[i][5];
        int t2 = diff[i][2] + diff[i][6];
        int t6 = diff[i][2] - diff[i][6];
        int t3 = diff[i][3] + diff[i][7];
        int t7 = diff[i][3] - diff[i][7];
        int s0 = t0 + t2;
        int s2 = t0 - t2;
        int s1 = t1 + t3;
        int s3 = t1 - t3;
        int s4 = t4 + t6;
        int s6 = t4 - t6;
        int s5 = t5 + t7;
        int s7 = t5 - t7;
        diff[i][0] = s0 + s1;
        diff[i][1] = s0 - s1;
        diff[i][2] = s2 + s3;
        diff[i][3] = s2 - s3;
        diff[i][4] = s4 + s5;
        diff[i][5] = s4 - s5;
        diff[i][6] = s6 + s7;
        diff[i][7] = s6 - s7;
    }
    for (int i = 0; i < 8; i++) {
        int t0 = diff[0][i] + diff[4][i];
        int t4 = diff[0][i] - diff[4][i];
        int t1 = diff[1][i] + diff[5][i];
        int t5 = diff[1][i] - diff[5][i];
        int t2 = diff[2][i] + diff[6][i];
        int t6 = diff[2][i] - diff[6][i];
        int t3 = diff[3][i] + diff[7][i];
        int t7 = diff[3][i] - diff[7][i];
        int s0 = t0 + t2;
        int s2 = t0 - t2;
        int s1 = t1 + t3;
        int s3 = t1 - t3;
        int s4 = t4 + t6;
        int s6 = t4 - t6;
        int s5 = t5 + t7;
        int s7 = t5 - t7;
        satd += abs(s0 + s1);
        satd += abs(s0 - s1);
        satd += abs(s2 + s3);
        satd += abs(s2 - s3);
        satd += abs(s4 + s5);
        satd += abs(s4 - s5);
        satd += abs(s6 + s7);
        satd += abs(s6 - s7);
    }

    return satd;
}

mfxU32 CalcSatd8x8(const mfxU8 *blockSrc, const mfxU8 *blockRef, mfxI32 blockW, mfxI32 blockH)
{
    mfxU32 satd = 0;
    for (mfxI32 y = 0; y < blockH; y += 8, blockSrc += 8 * blockW, blockRef += 8 * blockW)
        for (mfxI32 x = 0; x < blockW; x += 8)
            satd += CalcSatd8x8OneBlock(blockSrc + x, blockRef + x, blockW);
    return satd;
}

void CopyBlock(const mfxU8 *src, mfxI32 x, mfxI32 y, mfxI32 blockW, mfxI32 blockH, mfxU8 *block)
{
    for (mfxI32 yy = 0; yy < blockH; yy++, block += blockW)
        for (mfxI32 xx = 0; xx < blockW; xx++)
            block[xx] = GetPel(src, WIDTH, WIDTH, HEIGHT, x + xx, y + yy); 
}

int RunCpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData)
{
    mfxI32 numBlocksHor = (WIDTH + BLOCK_W - 1) / BLOCK_W;
    mfxI32 numBlocksVer = (HEIGHT + BLOCK_H - 1) / BLOCK_H;
    mfxU8 blockSrc[BLOCK_W * BLOCK_H];
    mfxU8 blockRef[BLOCK_W * BLOCK_H];

    for (mfxI32 yBlk = 0; yBlk < numBlocksVer; yBlk++, outData += numBlocksHor * MODE_SIZE) {
        for (mfxI32 xBlk = 0; xBlk < numBlocksHor; xBlk++) {
            for (mfxI32 mode = 0; mode < MODE_SIZE; mode++){
                CopyBlock(src, xBlk * BLOCK_W, yBlk * BLOCK_H, BLOCK_W, BLOCK_H, blockSrc);

                mfxI16Pair hpelPoint = mv[(yBlk * numBlocksHor + xBlk) * MODE_SIZE + mode];
                hpelPoint.x = (mfxI16)(hpelPoint.x + (xBlk * BLOCK_W) * 4);
                hpelPoint.y = (mfxI16)(hpelPoint.y + (yBlk * BLOCK_H) * 4);

                for (mfxU32 sadIdx = 0; sadIdx < 9; sadIdx++) {
                    mfxI16 dx = (mfxI16)(sadIdx % 3 - 1);
                    mfxI16 dy = (mfxI16)(sadIdx / 3 - 1);
                    mfxI16Pair qpelPoint = { hpelPoint.x + dx, hpelPoint.y + dy };

                    InterpolateBlock(ref, qpelPoint, BLOCK_W, BLOCK_H, blockRef);
                    outData[xBlk * MODE_SIZE + mode].sad[sadIdx] = CalcSatd8x8(blockSrc, blockRef, BLOCK_W, BLOCK_H);
                }
            }
        }
    }

    return PASSED;
}

};