/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.
//
*/

#pragma once

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_ENCODE_HW

#include <vector>
#include <assert.h>

#include "cmrt_cross_platform.h"

class CmDevice;
class CmBuffer;
class CmBufferUP;
class CmSurface2D;
class CmEvent;
class CmQueue;
class CmProgram;
class CmKernel;
class SurfaceIndex;
class CmThreadSpace;
class CmTask;

namespace MfxHwH264Encode
{
class DdiTask;
class MfxVideoParam;
struct SVCEncCURBEData;
struct SVCPAKObject;

struct mfxVMEUNIIn
{
    mfxU16  FTXCoeffThresh_DC;
    mfxU8   FTXCoeffThresh[6];
    mfxU8   MvCost[8];
    mfxU8   ModeCost[12];
};


class CmRuntimeError : public std::exception
{
public:
    CmRuntimeError() : std::exception() { assert(!"CmRuntimeError"); }
};


class CmDevicePtr
{
public:
    explicit CmDevicePtr(CmDevice * device = 0);

    ~CmDevicePtr();

    void Reset(CmDevice * device);

    CmDevice * operator -> ();

    CmDevicePtr & operator = (CmDevice * device);

    operator CmDevice * ();

private:
    CmDevicePtr(CmDevicePtr const &);
    void operator = (CmDevicePtr const &);

    CmDevice * m_device;
};


class CmSurface
{
public:
    CmSurface();

    CmSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface);

    CmSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

    ~CmSurface();

    CmSurface2D * operator -> ();

    operator CmSurface2D * ();

    void Reset(CmDevice * device, IDirect3DSurface9 * d3dSurface);

    void Reset(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

    SurfaceIndex const & GetIndex();

    void Read(void * buf, CmEvent * e = 0);

    void Write(void * buf, CmEvent * e = 0);

private:
    CmDevice *      m_device;
    CmSurface2D *   m_surface;
};

class CmSurfaceVme75
{
public:
    CmSurfaceVme75();

    CmSurfaceVme75(CmDevice * device, SurfaceIndex * index);

    ~CmSurfaceVme75();

    void Reset(CmDevice * device, SurfaceIndex * index);

    SurfaceIndex const * operator & () const;

    operator SurfaceIndex ();

private:
    CmDevice *      m_device;
    SurfaceIndex *  m_index;
};

class CmBuf
{
public:
    CmBuf();

    CmBuf(CmDevice * device, mfxU32 size);

    ~CmBuf();

    CmBuffer * operator -> ();

    operator CmBuffer * ();

    void Reset(CmDevice * device, mfxU32 size);

    SurfaceIndex const & GetIndex() const;

    void Read(void * buf, CmEvent * e = 0) const;

    void Write(void * buf, CmEvent * e = 0) const;

private:
    CmDevice *  m_device;
    CmBuffer *  m_buffer;
};

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version = 0);

void DestroyGlobalCmDevice(CmDevice * device);

CmBuffer * CreateBuffer(CmDevice * device, mfxU32 size);

CmBufferUP * CreateBuffer(CmDevice * device, mfxU32 size, void * mem);

CmSurface2D * CreateSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface);

CmSurface2D * CreateSurface(CmDevice * device, ID3D11Texture2D * d3dSurface);

CmSurface2D * CreateSurface2DSubresource(CmDevice * device, ID3D11Texture2D * d3dSurface);

CmSurface2D * CreateSurface(CmDevice * device, mfxHDL nativeSurface, eMFXVAType vatype);

CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc);

SurfaceIndex * CreateVmeSurfaceG75(
    CmDevice *      device,
    CmSurface2D *   source,
    CmSurface2D **  fwdRefs,
    CmSurface2D **  bwdRefs,
    mfxU32          numFwdRefs,
    mfxU32          numBwdRefs);

template <class T0>
void SetKernelArg(CmKernel * kernel, T0 const & arg)
{
    kernel->SetKernelArg(0, sizeof(T0), &arg);
}

template <class T0>
void SetKernelArgLast(CmKernel * kernel, T0 const & arg, unsigned int index)
{
    kernel->SetKernelArg(index, sizeof(T0), &arg);
}

template <class T0, class T1>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1)
{
    SetKernelArg(kernel, arg0);
    SetKernelArgLast(kernel, arg1, 1);
}

template <class T0, class T1, class T2>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2)
{
    SetKernelArg(kernel, arg0, arg1);
    SetKernelArgLast(kernel, arg2, 2);
}

template <class T0, class T1, class T2, class T3>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3)
{
    SetKernelArg(kernel, arg0, arg1, arg2);
    SetKernelArgLast(kernel, arg3, 3);
}

template <class T0, class T1, class T2, class T3, class T4>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3);
    SetKernelArgLast(kernel, arg4, 4);
}

template <class T0, class T1, class T2, class T3, class T4, class T5>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4);
    SetKernelArgLast(kernel, arg5, 5);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5);
    SetKernelArgLast(kernel, arg6, 6);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
    SetKernelArgLast(kernel, arg7, 7);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7);
    SetKernelArgLast(kernel, arg8, 8);
}

template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
void SetKernelArg(CmKernel * kernel, T0 const & arg0, T1 const & arg1, T2 const & arg2, T3 const & arg3, T4 const & arg4, T5 const & arg5, T6 const & arg6, T7 const & arg7, T8 const & arg8, T9 const & arg9)
{
    SetKernelArg(kernel, arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
    SetKernelArgLast(kernel, arg9, 9);
}


struct RefMbData
{
    RefMbData() : used(false), poc(0), mb(), mv(), mv4X() {}

    bool   used;
    mfxU32 poc;
    CmBuf  mb;
    CmBuf  mv;
    CmBuf  mv4X;
};

class CmContext
{
public:
    CmContext();

    CmContext(
        MfxVideoParam const & video,
        CmDevice *            cmDevice);

    void Setup(
        MfxVideoParam const & video,
        CmDevice *            cmDevice);

    CmEvent* EnqueueKernel(
        CmKernel *            kernel,
        unsigned int          tsWidth,
        unsigned int          tsHeight,
        CM_DEPENDENCY_PATTERN tsPattern);

    CmEvent * RunVme(
        DdiTask const & task,
        mfxU32          qp);

    bool QueryVme(
        DdiTask const & task,
        CmEvent *       e);

    int DestroyEvent( CmEvent*& e ){ if(m_queue) return m_queue->DestroyEvent(e); else return 0; } 

protected:
    CmKernel * SelectKernelPreMe(mfxU32 frameType);
    CmKernel * SelectKernelDownSample(mfxU16 LaScaleFactor);
    mfxVMEUNIIn & SelectCosts(mfxU32 frameType);

    void SetCurbeData(
        SVCEncCURBEData & curbeData,
        DdiTask const &   task,
        mfxU32            qp);

    void SetCurbeData(
        SVCEncCURBEData & curbeData,
        mfxU16            frameType,
        mfxU32            qp,
        mfxU32 biWeight);

private:
    mfxVideoParam m_video;

    CmDevice *  m_device;
    CmQueue*    m_queue;
    CmProgram * m_program;
    CmKernel *  m_kernelI;
    CmKernel *  m_kernelP;
    CmKernel *  m_kernelB;
    CmBuf       m_curbeData;
    CmBuf       m_nullBuf;
    mfxU32      m_lutMvP[65];
    mfxU32      m_lutMvB[65];
    mfxVMEUNIIn m_costsI;
    mfxVMEUNIIn m_costsP;
    mfxVMEUNIIn m_costsB;

    CmKernel *  m_kernelDownSample4X;
    CmKernel *  m_kernelDownSample2X;

    mfxU16      widthLa;
    mfxU16      heightLa;
    mfxU16      LaScaleFactor;
};

}
#endif // MFX_ENABLE_H264_VIDEO_ENCODE_HW
