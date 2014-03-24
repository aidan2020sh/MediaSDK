/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include "stdio.h"
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4201)
#include "cm_rt.h"
#pragma warning(pop)
#include "../include/test_common.h"
#include "../include/genx_h265_cmcode_isa.h"

#ifdef CMRT_EMU
extern "C"
void RefineMeP32x32(SurfaceIndex SURF_MBDIST_32x32, SurfaceIndex SURF_MBDATA_2X,
                    SurfaceIndex SURF_SRC_1X, SurfaceIndex SURF_REF_1X, mfxU32 picWidth);
#endif //CMRT_EMU

const mfxI32 BLOCK_W = 32;
const mfxI32 BLOCK_H = 32;
#define KERNEL_NAME RefineMeP32x32


struct OutputData
{
    mfxU32 sad[16];
};

namespace {
int RunGpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData);
int RunCpu(const mfxU8 *src, const mfxU8 *ref, const mfxI16Pair *mv, OutputData *outData);
}

int TestRefineMeP32x32()
{
    mfxI32 numBlocksHor = (WIDTH + BLOCK_W - 1) / BLOCK_W;
    mfxI32 numBlocksVer = (HEIGHT + BLOCK_H - 1) / BLOCK_H;
    OutputData *outputGpu = new OutputData[numBlocksHor * numBlocksVer];
    OutputData *outputCpu = new OutputData[numBlocksHor * numBlocksVer];
    mfxU8  *src = new mfxU8[WIDTH * HEIGHT];
    mfxU8  *ref = new mfxU8[WIDTH * HEIGHT];
    mfxI16Pair *mv = new mfxI16Pair[numBlocksHor * numBlocksVer];
    mfxI32 res = PASSED;

    FILE *f = fopen(YUV_NAME, "rb");
    if (!f)
        return FAILED;
    if (fread(src, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read first frame
        return FAILED;
    if (fseek(f, WIDTH * HEIGHT / 2, SEEK_CUR) != 0) // skip chroma
        return FAILED;
    if (fread(ref, 1, WIDTH * HEIGHT, f) != WIDTH * HEIGHT) // read second frame
        return FAILED;
    fclose(f);

    // fill motion vector field
    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            mv[y * numBlocksHor + x].x = (mfxI16)((x - numBlocksHor / 2) * 2);
            mv[y * numBlocksHor + x].y = (mfxI16)((y - numBlocksVer / 2) * 2);
        }
    }

    res = RunGpu(src, ref, mv, outputGpu);
    CHECK_ERR(res);

    res = RunCpu(src, ref, mv, outputCpu);
    CHECK_ERR(res);

    // compare output
    for (mfxI32 y = 0; y < numBlocksVer; y++) {
        for (mfxI32 x = 0; x < numBlocksHor; x++) {
            mfxU32 *sadGpu = outputGpu[y * numBlocksHor + x].sad;
            mfxU32 *sadCpu = outputCpu[y * numBlocksHor + x].sad;
            for (mfxI32 i = 0; i < 9; i++) {
                if (sadGpu[i] != sadCpu[i]) {
                    printf("bad sad value (%d != %d) for idx %d for block (x,y)=(%d,%d)\n",
                           sadGpu[i], sadCpu[i], i, x, y);
                    return FAILED;
                }
            }
        }
    }

    delete [] outputGpu;
    delete [] outputCpu;
    delete [] src;
    delete [] ref;
    delete [] mv;

    return PASSED;
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

    CmProgram *program = 0;
    res = device->LoadProgram((void *)genx_h265_cmcode, sizeof(genx_h265_cmcode), program);
    CHECK_CM_ERR(res);

    CmKernel *kernel = 0;
    res = device->CreateKernel(program, CM_KERNEL_FUNCTION(KERNEL_NAME), kernel);
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

    CmSurface2D *inMv = 0;
    res = device->CreateSurface2D(numBlocksHor * sizeof(mfxI16Pair), numBlocksVer, CM_SURFACE_FORMAT_A8, inMv);
    CHECK_CM_ERR(res);
    res = inMv->WriteSurface((const mfxU8 *)mv, NULL);
    CHECK_CM_ERR(res);

    mfxU32 outputWidth = numBlocksHor * sizeof(OutputData);
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

    SurfaceIndex *idxInMv = 0;
    res = inMv->GetIndex(idxInMv);
    CHECK_CM_ERR(res);

    SurfaceIndex *idxOutputCm = 0;
    res = outputCm->GetIndex(idxOutputCm);
    CHECK_CM_ERR(res);

    res = kernel->SetKernelArg(0, sizeof(*idxOutputCm), idxOutputCm);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(1, sizeof(*idxInMv), idxInMv);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(2, sizeof(*idxInSrc), idxInSrc);
    CHECK_CM_ERR(res);
    res = kernel->SetKernelArg(3, sizeof(*idxInRef), idxInRef);
    CHECK_CM_ERR(res);

    res = kernel->SetThreadCount(numBlocksHor * numBlocksVer);
    CHECK_CM_ERR(res);

    CmThreadSpace * threadSpace = 0;
    res = device->CreateThreadSpace(numBlocksHor, numBlocksVer, threadSpace);
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

    device->DestroyThreadSpace(threadSpace);
    device->DestroyTask(task);
    
    res = e->WaitForTaskFinished();
    CHECK_CM_ERR(res);

    mfxU64 time;
    e->GetExecutionTime(time);
    printf("TIME=%.3f ms\n", time / 1000000.0);

    for (mfxI32 y = 0; y < numBlocksVer; y++)
        memcpy(outData + y * numBlocksHor, outputSys + y * outputPitch, outputWidth);

    queue->DestroyEvent(e);
    device->DestroySurface2DUP(outputCm);
    CM_ALIGNED_FREE(outputSys);
    device->DestroySurface(inMv);
    device->DestroySurface(inRef);
    device->DestroySurface(inSrc);
    device->DestroyKernel(kernel);
    device->DestroyProgram(program);
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

mfxU32 CalcSad(const mfxU8 *blockSrc, const mfxU8 *blockRef, mfxI32 blockW, mfxI32 blockH)
{
    mfxU32 sad = 0;
    for (mfxI32 y = 0; y < blockH; y++, blockSrc += blockW, blockRef += blockW)
        for (mfxI32 x = 0; x < blockW; x++)
            sad += abs(blockSrc[x] - blockRef[x]);
    return sad;
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

    for (mfxI32 yBlk = 0; yBlk < numBlocksVer; yBlk++, outData += numBlocksHor) {
        for (mfxI32 xBlk = 0; xBlk < numBlocksHor; xBlk++) {
            CopyBlock(src, xBlk * BLOCK_W, yBlk * BLOCK_H, BLOCK_W, BLOCK_H, blockSrc);

            mfxI16Pair hpelPoint = mv[yBlk * numBlocksHor + xBlk];
            hpelPoint.x = (mfxI16)(hpelPoint.x + xBlk * BLOCK_W * 4);
            hpelPoint.y = (mfxI16)(hpelPoint.y + yBlk * BLOCK_H * 4);

            for (mfxU32 sadIdx = 0; sadIdx < 9; sadIdx++) {
                mfxI16 dx = (mfxI16)(sadIdx % 3 - 1);
                mfxI16 dy = (mfxI16)(sadIdx / 3 - 1);
                mfxI16Pair qpelPoint = { hpelPoint.x + dx, hpelPoint.y + dy };

                InterpolateBlock(ref, qpelPoint, BLOCK_W, BLOCK_H, blockRef);
                outData[xBlk].sad[sadIdx] = CalcSad(blockSrc, blockRef, BLOCK_W, BLOCK_H);
            }
        }
    }

    return PASSED;
}

};