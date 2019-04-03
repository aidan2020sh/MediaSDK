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

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include <fstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <utility>

//#include "mfx_h265_defs.h"
#include "mfx_h265_enc_cm_utils.h"

namespace H265Enc {


template<class T> inline void Zero(T &obj) { memset(&obj, 0, sizeof(obj)); }

const char   ME_PROGRAM_NAME[] = "genx_h265_cmcode.isa";

const mfxU32 SEARCHPATHSIZE    = 56;
const mfxU32 BATCHBUFFER_END   = 0x5000000;

CmProgram * ReadProgram(CmDevice * device, std::istream & is)
{
    CmProgram * program = 0;
    char * code = 0;
    std::streamsize size = 0;
    device, is;

#ifndef CMRT_EMU
    is.seekg(0, std::ios::end);
    size = is.tellg();
    is.seekg(0, std::ios::beg);
    if (size == 0)
        throw CmRuntimeError();

    std::vector<char> buf((int)size);
    code = &buf[0];
    is.read(code, size);
    if (is.gcount() != size)
        throw CmRuntimeError();
#endif // CMRT_EMU

    if (device->LoadProgram(code, mfxU32(size), program, "nojitter") != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmProgram * ReadProgram(CmDevice * device, char const * filename)
{
    std::fstream f;
    filename;
#ifndef CMRT_EMU
    f.open(filename, std::ios::binary | std::ios::in);
    if (!f)
        throw CmRuntimeError();
#endif // CMRT_EMU
    return ReadProgram(device, f);
}

CmProgram * ReadProgram(CmDevice * device, const unsigned char* buffer, int len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = device->LoadProgram((void*)buffer, len, program, "nojitter")) != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr)
{
    int result = CM_SUCCESS;
    CmKernel * kernel = 0;

    if ((result = ::CreateKernel(device, program, name, funcptr, kernel)) != CM_SUCCESS)
        throw CmRuntimeError();
    //fprintf(stderr, "Success creating kernel %s\n", name);

    return kernel;
}

void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel, mfxU32 tsWidth,
                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "EnqueueKernel");
    int result = CM_SUCCESS;

    if ((result = kernel->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmThreadSpace * cmThreadSpace = 0;
    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);

    CmTask * cmTask = 0;
    if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    device->DestroyThreadSpace(cmThreadSpace);
    device->DestroyTask(cmTask);
}

//void EnqueueKernel(CmDevice *device, CmQueue *queue, CmKernel *kernel0, CmKernel *kernel1, mfxU32 tsWidth,
//                   mfxU32 tsHeight, CmEvent *&event, CM_DEPENDENCY_PATTERN tsPattern)
//{
//    int result = CM_SUCCESS;
//
//    if ((result = kernel0->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//    if ((result = kernel1->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);
//
//    CmTask * cmTask = 0;
//    if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel0)) != CM_SUCCESS)
//        throw CmRuntimeError();
//    if ((result = cmTask->AddKernel(kernel1)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    device->DestroyTask(cmTask);
//}

//void EnqueueKernel(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth,
//                   mfxU32 tsHeight, CmEvent *&event)
//{
//    int result = CM_SUCCESS;
//
//    if ((result = kernel->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
//
//    //CmTask * cmTask = 0;
//    //if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//    //    throw CmRuntimeError();\
//
//    if (cmTask)
//        cmTask->Reset();
//    else
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    //device->DestroyTask(cmTask);
//}

//void EnqueueKernelLight(CmDevice *device, CmQueue *queue, CmTask * cmTask, CmKernel *kernel, mfxU32 tsWidth,
//                        mfxU32 tsHeight, CmEvent *&event)
//{
//    // for kernel reusing; ThreadCount should be set in caller before SetArgs (Cm restriction!!!)
//
//    int result = CM_SUCCESS;
//
//    CmThreadSpace * cmThreadSpace = 0;
//    if ((result = device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    cmThreadSpace->SelectThreadDependencyPattern(CM_NONE_DEPENDENCY);
//
//    //CmTask * cmTask = 0;
//    //if ((result = device->CreateTask(cmTask)) != CM_SUCCESS)
//    //    throw CmRuntimeError();\
//
//    if (cmTask)
//        cmTask->Reset();
//    else
//        throw CmRuntimeError();
//
//    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    if (event != NULL && event != CM_NO_EVENT)
//        queue->DestroyEvent(event);
//
//    if ((result = queue->Enqueue(cmTask, event, cmThreadSpace)) != CM_SUCCESS)
//        throw CmRuntimeError();
//
//    device->DestroyThreadSpace(cmThreadSpace);
//    //device->DestroyTask(cmTask);
//}

void EnqueueCopyCPUToGPU(CmQueue *queue, CmSurface2D *surface, const void *sysMem, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyCPUToGPU(surface, (const mfxU8 *)sysMem, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyGPUToCPU(CmQueue *queue, CmSurface2D *surface, void *sysMem, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyGPUToCPU(surface, (mfxU8 *)sysMem, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyCPUToGPUStride(CmQueue *queue, CmSurface2D *surface, const void *sysMem,
                               mfxU32 pitch, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyCPUToGPUFullStride(surface, (const mfxU8 *)sysMem, pitch, 0, 0, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void EnqueueCopyGPUToCPUStride(CmQueue *queue, CmSurface2D *surface, void *sysMem,
                               mfxU32 pitch, CmEvent *&event)
{
    if (event != NULL && event != CM_NO_EVENT)
        queue->DestroyEvent(event);

    int result = CM_SUCCESS;
    if ((result = queue->EnqueueCopyGPUToCPUFullStride(surface, (mfxU8 *)sysMem, pitch, 0, 0, event)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Read(CmBuffer * buffer, void * buf, CmEvent * e)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}
#ifdef CM_4_0
    #define TASKNUMALLOC 3
#else
    #define TASKNUMALLOC 0
#endif
CmDevice * TryCreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    mfxCoreParam par;
    mfxStatus sts = core->GetCoreParam(&par);

    if (sts != MFX_ERR_NONE)
        return 0;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
    mfxU32 impl = par.Impl & 0x0700;
    if (impl == MFX_IMPL_VIA_D3D9)
    {
#if defined(_WIN32) || defined(_WIN64)
        IDirect3DDeviceManager9 * d3dIface;
        sts = core->GetHandle(MFX_HANDLE_D3D9_DEVICE_MANAGER, (mfxHDL*)&d3dIface);
        if (sts != MFX_ERR_NONE || !d3dIface)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface,(TASKNUMALLOC<<4)+1)) != CM_SUCCESS)
            return 0;
#endif  // #if defined(_WIN32) || defined(_WIN64)
    }
    else if (impl & MFX_IMPL_VIA_D3D11)
    {
#if defined(_WIN32) || defined(_WIN64)
        ID3D11Device * d3dIface;
        sts = core->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&d3dIface);
        if (sts != MFX_ERR_NONE || !d3dIface)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface,(TASKNUMALLOC<<4)+1)) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (impl & MFX_IMPL_VIA_VAAPI)
    {
        //throw std::logic_error("GetDeviceManager not implemented on Linux");
#if defined(MFX_VA_LINUX)
        VADisplay display;
        mfxStatus res = core->GetHandle(MFX_HANDLE_VA_DISPLAY, (mfxHDL*)&display); // == MFX_HANDLE_RESERVED2
        if (res != MFX_ERR_NONE || !display)
            return 0;

        if ((result = ::CreateCmDevice(device, *version, display)) != CM_SUCCESS)
            return 0;
#endif
    }

    return device;
}

CmDevice * CreateCmDevicePtr(MFXCoreInterface * core, mfxU32 * version)
{
    /* return 0 instead of throwing exception to allow failing gracefully */
    CmDevice * device = TryCreateCmDevicePtr(core, version);
#if 0
    if (device == 0)
        throw CmRuntimeError();
#endif
    return device;
}


CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer;
    int result = device->CreateBuffer(size, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem)
{
    CmBufferUP * buffer;
    int result = device->CreateBufferUP(size, mem, buffer);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return buffer;
}

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}

CmSurface2DUP * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc, void * mem)
{
    int result = CM_SUCCESS;
    CmSurface2DUP * cmSurface = 0;
    if (device && (result = device->CreateSurface2DUP(width, height, CM_SURFACE_FORMAT(fourcc), mem, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


SurfaceIndex * CreateVmeSurfaceG75(
    CmDevice *      device,
    CmSurface2D *   source,
    CmSurface2D **  fwdRefs,
    CmSurface2D **  bwdRefs,
    mfxU32          numFwdRefs,
    mfxU32          numBwdRefs)
{
    if (numFwdRefs == 0)
        fwdRefs = 0;
    if (numBwdRefs == 0)
        bwdRefs = 0;

    int result = CM_SUCCESS;
    SurfaceIndex * index;
    if ((result = device->CreateVmeSurfaceG7_5(source, fwdRefs, bwdRefs, numFwdRefs, numBwdRefs, index)) != CM_SUCCESS)
        throw CmRuntimeError();
    return index;
}

mfxU32 SetSearchPath(
    mfxVMEIMEIn & spath,
    mfxU32        frameType,
    mfxU32        meMethod)
{
    mfxU32 maxNumSU = SEARCHPATHSIZE + 1;

    if (frameType & MFX_FRAMETYPE_P)
    {
        switch (meMethod)
        {
        case 2:
            std::copy(SingleSU, SingleSU + 32, spath.IMESearchPath0to31);
            maxNumSU = 1;
            break;
        case 3:
            std::copy(RasterScan_48x40, RasterScan_48x40 + 32, spath.IMESearchPath0to31);
            std::copy(RasterScan_48x40 + 32, std::end(RasterScan_48x40), spath.IMESearchPath32to55);
            break;
        case 4:
            std::copy(FullSpiral_48x40, FullSpiral_48x40 + 32, spath.IMESearchPath0to31);
            std::copy(FullSpiral_48x40 + 32, std::end(FullSpiral_48x40), spath.IMESearchPath32to55);
            break;
        case 5:
            std::copy(FullSpiral_48x40, FullSpiral_48x40 + 32, spath.IMESearchPath0to31);
            std::copy(FullSpiral_48x40 + 32, std::end(FullSpiral_48x40), spath.IMESearchPath32to55);
            maxNumSU = 16;
            break;
        case 6:
        default:
            std::copy(Diamond, Diamond + 32, spath.IMESearchPath0to31);
            std::copy(Diamond + 32, std::end(Diamond), spath.IMESearchPath32to55);
            break;
        }
    }
    else
    {
        if (meMethod == 6)
        {
            std::copy(Diamond, Diamond + 32, spath.IMESearchPath0to31);
            std::copy(Diamond + 32, std::end(Diamond), spath.IMESearchPath32to55);
        }
        else
        {
            std::copy(FullSpiral_48x40, FullSpiral_48x40 + 32, spath.IMESearchPath0to31);
            std::copy(FullSpiral_48x40 + 32, std::end(FullSpiral_48x40), spath.IMESearchPath32to55);
        }
    }

    return maxNumSU;
}

mfxU8 ToU4U4(mfxU16 val) {
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
    return mfxU8(base | (shift << 4));
}

void SetupMeControl(MeControl &ctrl, int width, int height, double lambda)
{
    const std::size_t minLongSize = std::min(sizeof(Diamond) / sizeof(Diamond[0]), sizeof(ctrl.longSp) / sizeof(ctrl.longSp[0]));
    std::copy(Diamond, Diamond + minLongSize, ctrl.longSp);
    ctrl.longSpLenSp = 16;
    ctrl.longSpMaxNumSu = 57;

    const mfxU8 ShortPath[4] = {
        0x0F, 0xF0, 0x01, 0x00
    };

    const std::size_t minShortSize = std::min(sizeof(ShortPath) / sizeof(ShortPath[0]), sizeof(ctrl.shortSp) / sizeof(ctrl.shortSp[0]));
    std::copy(ShortPath, ShortPath + minShortSize, ctrl.shortSp);
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
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[0][i] = ToU4U4(mfxU16(0.5 + lambda/2 * mvBits[i]));

    mvBits = MvBits[ctrl.mvCostScaleFactor[1]];
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[1][i] = ToU4U4(mfxU16(0.5 + lambda/2 /  4 * (1 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[2]];
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[2][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 16 * (2 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[3]];
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[3][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 64 * (3 * (i > 0) + mvBits[i])));

    mvBits = MvBits[ctrl.mvCostScaleFactor[4]];
    for (Ipp32s i = 0; i < 8; i++)
        ctrl.mvCost[4][i] = ToU4U4(mfxU16(0.5 + lambda/2 / 64 * (4 * (i > 0) + mvBits[i])));

    //memset(ctrl.mvCost, 0, sizeof(ctrl.mvCost));

    mfxU8 MvCostHumanFriendly[5][8];
    for (Ipp32s i = 0; i < 5; i++)
        for (Ipp32s j = 0; j < 8; j++)
            MvCostHumanFriendly[i][j] = (ctrl.mvCost[i][j] & 0xf) << ((ctrl.mvCost[i][j] >> 4) & 0xf);
}

void SetCurbeData(
    H265EncCURBEData & curbeData,
    mfxU32             PicType,
    mfxU32             qp,
    mfxU32             width,
    mfxU32             height)
{
    mfxU32 interSad       = 0; // 0-sad,2-haar
    mfxU32 intraSad       = 0; // 0-sad,2-haar
    //mfxU32 ftqBasedSkip   = 0; //3;
    mfxU32 blockBasedSkip = 0;//1;
///    mfxU32 qpIdx          = (qp + 1) >> 1;
///    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxVMEIMEIn spath;
    SetSearchPath(spath, PicType, meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = 0x3F/*(GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F*/;
    curbeData.BiWeight              = /*((task.m_frameType & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? CalcBiWeight(task, 0, 0) :*/ 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (PicType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 0;//3; // all modes
    curbeData.SkipType              = 0; //!!(task.m_frameType & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(PicType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (PicType & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceHeightMinusOne   = height / 16 - 1;
    curbeData.PictureHeightMinusOne = height / 16 - 1;
    curbeData.PictureWidth          = width / 16;
    curbeData.Log2MvScaleFactor     = 0;
    curbeData.Log2MbScaleFactor     = 0;
    //DW5
    curbeData.RefWidth              = (PicType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (PicType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 0; // all partitions enabled
    curbeData.NonSkipZMvAdded       = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = 0;//!!(PicType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = 0;//!(PicType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(PicType & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = 0;
    curbeData.ModeCost_1            = 0;
    curbeData.ModeCost_2            = 0;
    curbeData.ModeCost_3            = 0;
    //DW9
    curbeData.ModeCost_4            = 0;
    curbeData.ModeCost_5            = 0;
    curbeData.ModeCost_6            = 0;
    curbeData.ModeCost_7            = 0;
    //DW10
    curbeData.ModeCost_8            = 0;
    curbeData.ModeCost_9            = 0;
    curbeData.RefIDCost             = 0;
    curbeData.ChromaIntraModeCost   = 0;
    //DW11
    curbeData.MvCost_0              = 0;
    curbeData.MvCost_1              = 0;
    curbeData.MvCost_2              = 0;
    curbeData.MvCost_3              = 0;
    //DW12
    curbeData.MvCost_4              = 0;
    curbeData.MvCost_5              = 0;
    curbeData.MvCost_6              = 0;
    curbeData.MvCost_7              = 0;
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp;
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = 0;
    curbeData.FTXCoeffThresh_1                = 0;
    curbeData.FTXCoeffThresh_2                = 0;
    //DW15
    curbeData.FTXCoeffThresh_3                = 0;
    curbeData.FTXCoeffThresh_4                = 0;
    curbeData.FTXCoeffThresh_5                = 0;
    curbeData.FTXCoeffThresh_6                = 0;
    //DW16
    curbeData.IMESearchPath0                  = spath.IMESearchPath0to31[0];
    curbeData.IMESearchPath1                  = spath.IMESearchPath0to31[1];
    curbeData.IMESearchPath2                  = spath.IMESearchPath0to31[2];
    curbeData.IMESearchPath3                  = spath.IMESearchPath0to31[3];
    //DW17
    curbeData.IMESearchPath4                  = spath.IMESearchPath0to31[4];
    curbeData.IMESearchPath5                  = spath.IMESearchPath0to31[5];
    curbeData.IMESearchPath6                  = spath.IMESearchPath0to31[6];
    curbeData.IMESearchPath7                  = spath.IMESearchPath0to31[7];
    //DW18
    curbeData.IMESearchPath8                  = spath.IMESearchPath0to31[8];
    curbeData.IMESearchPath9                  = spath.IMESearchPath0to31[9];
    curbeData.IMESearchPath10                 = spath.IMESearchPath0to31[10];
    curbeData.IMESearchPath11                 = spath.IMESearchPath0to31[11];
    //DW19
    curbeData.IMESearchPath12                 = spath.IMESearchPath0to31[12];
    curbeData.IMESearchPath13                 = spath.IMESearchPath0to31[13];
    curbeData.IMESearchPath14                 = spath.IMESearchPath0to31[14];
    curbeData.IMESearchPath15                 = spath.IMESearchPath0to31[15];
    //DW20
    curbeData.IMESearchPath16                 = spath.IMESearchPath0to31[16];
    curbeData.IMESearchPath17                 = spath.IMESearchPath0to31[17];
    curbeData.IMESearchPath18                 = spath.IMESearchPath0to31[18];
    curbeData.IMESearchPath19                 = spath.IMESearchPath0to31[19];
    //DW21
    curbeData.IMESearchPath20                 = spath.IMESearchPath0to31[20];
    curbeData.IMESearchPath21                 = spath.IMESearchPath0to31[21];
    curbeData.IMESearchPath22                 = spath.IMESearchPath0to31[22];
    curbeData.IMESearchPath23                 = spath.IMESearchPath0to31[23];
    //DW22
    curbeData.IMESearchPath24                 = spath.IMESearchPath0to31[24];
    curbeData.IMESearchPath25                 = spath.IMESearchPath0to31[25];
    curbeData.IMESearchPath26                 = spath.IMESearchPath0to31[26];
    curbeData.IMESearchPath27                 = spath.IMESearchPath0to31[27];
    //DW23
    curbeData.IMESearchPath28                 = spath.IMESearchPath0to31[28];
    curbeData.IMESearchPath29                 = spath.IMESearchPath0to31[29];
    curbeData.IMESearchPath30                 = spath.IMESearchPath0to31[30];
    curbeData.IMESearchPath31                 = spath.IMESearchPath0to31[31];
    //DW24
    curbeData.IMESearchPath32                 = spath.IMESearchPath32to55[0];
    curbeData.IMESearchPath33                 = spath.IMESearchPath32to55[1];
    curbeData.IMESearchPath34                 = spath.IMESearchPath32to55[2];
    curbeData.IMESearchPath35                 = spath.IMESearchPath32to55[3];
    //DW25
    curbeData.IMESearchPath36                 = spath.IMESearchPath32to55[4];
    curbeData.IMESearchPath37                 = spath.IMESearchPath32to55[5];
    curbeData.IMESearchPath38                 = spath.IMESearchPath32to55[6];
    curbeData.IMESearchPath39                 = spath.IMESearchPath32to55[7];
    //DW26
    curbeData.IMESearchPath40                 = spath.IMESearchPath32to55[8];
    curbeData.IMESearchPath41                 = spath.IMESearchPath32to55[9];
    curbeData.IMESearchPath42                 = spath.IMESearchPath32to55[10];
    curbeData.IMESearchPath43                 = spath.IMESearchPath32to55[11];
    //DW27
    curbeData.IMESearchPath44                 = spath.IMESearchPath32to55[12];
    curbeData.IMESearchPath45                 = spath.IMESearchPath32to55[13];
    curbeData.IMESearchPath46                 = spath.IMESearchPath32to55[14];
    curbeData.IMESearchPath47                 = spath.IMESearchPath32to55[15];
    //DW28
    curbeData.IMESearchPath48                 = spath.IMESearchPath32to55[16];
    curbeData.IMESearchPath49                 = spath.IMESearchPath32to55[17];
    curbeData.IMESearchPath50                 = spath.IMESearchPath32to55[18];
    curbeData.IMESearchPath51                 = spath.IMESearchPath32to55[19];
    //DW29
    curbeData.IMESearchPath52                 = spath.IMESearchPath32to55[20];
    curbeData.IMESearchPath53                 = spath.IMESearchPath32to55[21];
    curbeData.IMESearchPath54                 = spath.IMESearchPath32to55[22];
    curbeData.IMESearchPath55                 = spath.IMESearchPath32to55[23];
    //DW30
    curbeData.Intra4x4ModeMask                = 0;
    curbeData.Intra8x8ModeMask                = 0;
    //DW31
    curbeData.Intra16x16ModeMask              = 0;
    curbeData.IntraChromaModeMask             = 0;
    curbeData.IntraComputeType                = 1; // luma only
    //DW32
    curbeData.SkipVal                         = 0;//skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 0;//36;
    curbeData.IntraNonDCPenalty8x8            = 0;//12;
    curbeData.IntraNonDCPenalty4x4            = 0;//4;
    //DW34
    //curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    curbeData.MaxVmvR                         = 0x7FFF;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 1;  //  0;  !sergo
    curbeData.HMERefWindowsCombiningThreshold = (PicType & MFX_FRAMETYPE_B) ? 8 : 16; //  0;  !sergo (should be =8 for B frames)
    curbeData.CheckAllFractionalEnable        = 0;
    //DW37
    curbeData.CurLayerDQId                    = 0;
    curbeData.TemporalId                      = 0;
    curbeData.NoInterLayerPredictionFlag      = 1;
    curbeData.AdaptivePredictionFlag          = 0;
    curbeData.DefaultBaseModeFlag             = 0;
    curbeData.AdaptiveResidualPredictionFlag  = 0;
    curbeData.DefaultResidualPredictionFlag   = 0;
    curbeData.AdaptiveMotionPredictionFlag    = 0;
    curbeData.DefaultMotionPredictionFlag     = 0;
    curbeData.TcoeffLevelPredictionFlag       = 0;
    curbeData.UseRawMePredictor               = 1;
    curbeData.SpatialResChangeFlag            = 0;
///    curbeData.isFwdFrameShortTermRef          = task.m_list0[0].Size() > 0 && !task.m_dpb[0][task.m_list0[0][0] & 127].m_longterm;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;
}

Kernel::Kernel() : m_device(), m_task(), m_numKernels(0), kernel(m_kernel[0]) { Zero(m_kernel); Zero(m_ts); }

void Kernel::AddKernel(CmDevice *device, CmProgram *program, const char *name,
                       mfxU32 tsWidth, mfxU32 tsHeight, CM_DEPENDENCY_PATTERN tsPattern, bool addSync)
{
    assert(m_device == NULL || m_device == device);
    assert(m_numKernels < 16);

    mfxU32 idx = m_numKernels++;

    m_kernel[idx] = CreateKernel(device, program, name, NULL);
    if (m_kernel[idx]->SetThreadCount(tsWidth * tsHeight) != CM_SUCCESS)
        throw CmRuntimeError();
    if (device->CreateThreadSpace(tsWidth, tsHeight, m_ts[idx]) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_ts[idx]->SelectThreadDependencyPattern(tsPattern) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_task == NULL)
        if (device->CreateTask(m_task) != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_numKernels > 1 && addSync)
        if (m_task->AddSync() != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_task->AddKernel(m_kernel[idx]) != CM_SUCCESS)
        throw CmRuntimeError();
    if (m_numKernels == 2)
        if (m_kernel[0]->AssociateThreadSpace(m_ts[0]) != CM_SUCCESS)
            throw CmRuntimeError();
    if (m_numKernels > 1) {
        if (m_kernel[idx]->AssociateThreadSpace(m_ts[idx]) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

void Kernel::Destroy()
{
    if (m_device) {
        for (mfxU32 i = 0; i < m_numKernels; i++) {
            m_device->DestroyThreadSpace(m_ts[i]);
            m_ts[i] = NULL;
            m_device->DestroyKernel(m_kernel[i]);
            m_kernel[i] = NULL;
        }
        m_device->DestroyTask(m_task);
        m_task = NULL;
        m_device = NULL;
    }
}

void Kernel::Enqueue(CmQueue *queue, CmEvent *&e)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_API, "Enqueue");
    if (e != NULL && e != CM_NO_EVENT)
        queue->DestroyEvent(e);
    if (m_numKernels == 1) {
        if (queue->Enqueue(m_task, e, m_ts[0]) != CM_SUCCESS)
            throw CmRuntimeError();
    } else {
        if (queue->Enqueue(m_task, e) != CM_SUCCESS)
            throw CmRuntimeError();
    }
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
