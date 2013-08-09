/*
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"
#ifdef MFX_ENABLE_H264_VIDEO_ENCODE_HW

#include <fstream>
#include <algorithm>
#include <string>
#include <stdexcept> /* for std exceptions on Linux/Android */

#include "libmfx_core_interface.h"

#include "cmrt_cross_platform.h"
#include "mfx_h264_encode_cm_defs.h"
#include "mfx_h264_encode_cm.h"
#include "mfx_h264_encode_hw_utils.h"
#include "genx_hsw_simple_me_isa.h"


namespace
{

using MfxHwH264Encode::CmRuntimeError;

const char   ME_PROGRAM_NAME[] = "genx_hsw_simple_me.isa";
const mfxU32 SEARCHPATHSIZE    = 56;
const mfxU32 BATCHBUFFER_END   = 0x5000000;

CmProgram * ReadProgram(CmDevice * device, const mfxU8 * buffer, size_t len)
{
    int result = CM_SUCCESS;
    CmProgram * program = 0;

    if ((result = ::ReadProgram(device, program, buffer, (mfxU32)len)) != CM_SUCCESS)
        throw CmRuntimeError();

    return program;
}

CmKernel * CreateKernel(CmDevice * device, CmProgram * program, char const * name, void * funcptr)
{
    int result = CM_SUCCESS;
    CmKernel * kernel = 0;

    if ((result = ::CreateKernel(device, program, name, funcptr, kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    return kernel;
}

SurfaceIndex & GetIndex(CmSurface2D * surface)
{
    SurfaceIndex * index = 0;
    int result = surface->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}


SurfaceIndex & GetIndex(CmBuffer * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

SurfaceIndex const & GetIndex(CmBufferUP * buffer)
{
    SurfaceIndex * index = 0;
    int result = buffer->GetIndex(index);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
    return *index;
}

void Read(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->ReadSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

void Write(CmBuffer * buffer, void * buf, CmEvent * e = 0)
{
    int result = CM_SUCCESS;
    if ((result = buffer->WriteSurface(reinterpret_cast<unsigned char *>(buf), e)) != CM_SUCCESS)
        throw CmRuntimeError();
}

};


namespace MfxHwH264Encode
{

CmDevice * TryCreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    mfxU32 versionPlaceholder = 0;
    if (version == 0)
        version = &versionPlaceholder;

    CmDevice * device = 0;

    int result = CM_SUCCESS;
    if (core->GetVAType() == MFX_HW_D3D9)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D9Interface * d3dIface = QueryCoreInterface<D3D9Interface>(core, MFXICORED3D_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D9DeviceManager())) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_D3D11)
    {
#if defined(_WIN32) || defined(_WIN64)
        D3D11Interface * d3dIface = QueryCoreInterface<D3D11Interface>(core, MFXICORED3D11_GUID);
        if (d3dIface == 0)
            return 0;
        if ((result = ::CreateCmDevice(device, *version, d3dIface->GetD3D11Device())) != CM_SUCCESS)
            return 0;
#endif
    }
    else if (core->GetVAType() == MFX_HW_VAAPI)
    {
        throw std::logic_error("GetDeviceManager not implemented on Linux for Look Ahead");
    }

    return device;
}

CmDevice * CreateCmDevicePtr(VideoCORE * core, mfxU32 * version)
{
    CmDevice * device = TryCreateCmDevicePtr(core, version);
    if (device == 0)
        throw CmRuntimeError();
    return device;
}

CmDevicePtr::CmDevicePtr(CmDevice * device)
    : m_device(device)
{
}

CmDevicePtr::~CmDevicePtr()
{
    Reset(0);
}

void CmDevicePtr::Reset(CmDevice * device)
{
    if (m_device)
    {
        int result = ::DestroyCmDevice(m_device);
        result; assert(result == CM_SUCCESS);
    }
    m_device = device;
}

CmDevice * CmDevicePtr::operator -> ()
{
    return m_device;
}

CmDevicePtr & CmDevicePtr::operator = (CmDevice * device)
{
    Reset(device);
    return *this;
}

CmDevicePtr::operator CmDevice * ()
{
    return m_device;
}


CmSurface::CmSurface()
: m_device(0)
, m_surface(0)
{
}

CmSurface::CmSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface)
: m_device(0)
, m_surface(0)
{
    Reset(device, d3dSurface);
}

CmSurface::CmSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
: m_device(0)
, m_surface(0)
{
    Reset(device, width, height, fourcc);
}

CmSurface::~CmSurface()
{
    Reset(0, 0);
}

CmSurface2D * CmSurface::operator -> ()
{
    return m_surface;
}

CmSurface::operator CmSurface2D * ()
{
    return m_surface;
}

void CmSurface::Reset(CmDevice * device, IDirect3DSurface9 * d3dSurface)
{
    CmSurface2D * newSurface = CreateSurface(device, d3dSurface);

    if (m_device && m_surface)
    {
        int result = m_device->DestroySurface(m_surface);
        result; assert(result == CM_SUCCESS);
    }

    m_device  = device;
    m_surface = newSurface;
}

void CmSurface::Reset(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    CmSurface2D * newSurface = CreateSurface(device, width, height, D3DFORMAT(fourcc));

    if (m_device && m_surface)
    {
        int result = m_device->DestroySurface(m_surface);
        result; assert(result == CM_SUCCESS);
    }

    m_device  = device;
    m_surface = newSurface;
}

SurfaceIndex const & CmSurface::GetIndex()
{
    return ::GetIndex(m_surface);
}

void CmSurface::Read(void * buf, CmEvent * e)
{
    int result = m_surface->ReadSurface(reinterpret_cast<unsigned char *>(buf), e);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}

void CmSurface::Write(void * buf, CmEvent * e)
{
    int result = m_surface->WriteSurface(reinterpret_cast<unsigned char *>(buf), e);
    if (result != CM_SUCCESS)
        throw CmRuntimeError();
}


CmSurfaceVme75::CmSurfaceVme75()
: m_device(0)
, m_index(0)
{
}

CmSurfaceVme75::CmSurfaceVme75(CmDevice * device, SurfaceIndex * index)
: m_device(0)
, m_index(0)
{
    Reset(device, index);
}

CmSurfaceVme75::~CmSurfaceVme75()
{
    Reset(0, 0);
}

void CmSurfaceVme75::Reset(CmDevice * device, SurfaceIndex * index)
{
    if (m_device && m_index)
    {
        int result = m_device->DestroyVmeSurfaceG7_5(m_index);
        result; assert(result == CM_SUCCESS);
    }

    m_device = device;
    m_index  = index;
}

SurfaceIndex const * CmSurfaceVme75::operator & () const
{
    return m_index;
}

CmSurfaceVme75::operator SurfaceIndex ()
{
    return *m_index;
}


CmBuf::CmBuf()
: m_device(0)
, m_buffer(0)
{
}

CmBuf::CmBuf(CmDevice * device, mfxU32 size)
: m_device(0)
, m_buffer(0)
{
    Reset(device, size);
}

CmBuf::~CmBuf()
{
    Reset(0, 0);
}

CmBuffer * CmBuf::operator -> ()
{
    return m_buffer;
}

CmBuf::operator CmBuffer * ()
{
    return m_buffer;
}

void CmBuf::Reset(CmDevice * device, mfxU32 size)
{
    CmBuffer * buffer = (device && size) ? CreateBuffer(device, size) : 0;

    if (m_device && m_buffer)
    {
        int result = m_device->DestroySurface(m_buffer);
        result; assert(result == CM_SUCCESS);
    }

    m_device = device;
    m_buffer = buffer;
}

SurfaceIndex const & CmBuf::GetIndex() const
{
    return ::GetIndex(m_buffer);
}

void CmBuf::Read(void * buf, CmEvent * e) const
{
    ::Read(m_buffer, buf, e);
}

void CmBuf::Write(void * buf, CmEvent * e) const
{
    ::Write(m_buffer, buf, e);
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

CmSurface2D * CreateSurface(CmDevice * device, IDirect3DSurface9 * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && d3dSurface && (result = device->CreateSurface2D(d3dSurface, cmSurface)) != CM_SUCCESS)
        throw CmRuntimeError(); 
    return cmSurface;
}


CmSurface2D * CreateSurface2DSubresource(CmDevice * device, ID3D11Texture2D * d3dSurface)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    mfxU32 cmSurfaceCount = 1;
    if (device && d3dSurface && (result = device->CreateSurface2DSubresource(d3dSurface, 1, &cmSurface, cmSurfaceCount)) != CM_SUCCESS)
        throw CmRuntimeError();
    return cmSurface;
}


CmSurface2D * CreateSurface(CmDevice * device, mfxHDL nativeSurface, eMFXVAType vatype)
{
    switch (vatype)
    {
    case MFX_HW_D3D9:
        return CreateSurface(device, (IDirect3DSurface9 *)nativeSurface);
    case MFX_HW_D3D11:
        return CreateSurface2DSubresource(device, (ID3D11Texture2D *)nativeSurface);
    default:
        throw CmRuntimeError();
    }
}


CmSurface2D * CreateSurface(CmDevice * device, mfxU32 width, mfxU32 height, mfxU32 fourcc)
{
    int result = CM_SUCCESS;
    CmSurface2D * cmSurface = 0;
    if (device && (result = device->CreateSurface2D(width, height, CM_SURFACE_FORMAT(fourcc), cmSurface)) != CM_SUCCESS)
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


CmContext::CmContext()
: m_device(0)
, m_program(0)
, m_queue(0)
{
/*
    Flog = fopen("HmeLog.txt", "wb");
    FdsSurf = fopen("dsSurf.yuv", "wb");
*/
}

namespace
{
    mfxU32 Map44LutValueBack(mfxU32 val)
    {
        mfxU32 base  = val & 0xf;
        mfxU32 shift = (val >> 4) & 0xf;
        assert((base << shift) < (1 << 12)); // encoded value must fit in 12-bits (Bspec)
        return base << shift;
    }

    const mfxU8 QP_LAMBDA[40] = // pow(2, qp/6)
    {
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 4, 4, 4, 5, 6,
        6, 7, 8, 9,10,11,13,14,
        16,18,20,23,25,29,32,36,
        40,45,51,57,64,72,81,91
    };

    void SetLutMv(mfxVMEUNIIn const & costs, mfxU32 lutMv[65])
    {
        lutMv[0]  = Map44LutValueBack(costs.MvCost[0]);
        lutMv[1]  = Map44LutValueBack(costs.MvCost[1]);
        lutMv[2]  = Map44LutValueBack(costs.MvCost[2]);
        lutMv[4]  = Map44LutValueBack(costs.MvCost[3]);
        lutMv[8]  = Map44LutValueBack(costs.MvCost[4]);
        lutMv[16] = Map44LutValueBack(costs.MvCost[5]);
        lutMv[32] = Map44LutValueBack(costs.MvCost[6]);
        lutMv[64] = Map44LutValueBack(costs.MvCost[7]);
        lutMv[3]  = (lutMv[4] + lutMv[2]) >> 1;
        for (mfxU32 i = 5; i < 8; i++)
            lutMv[i] = lutMv[ 4] + ((lutMv[ 8] - lutMv[ 4]) * (i -  4) >> 2);
        for (mfxU32 i = 9; i < 16; i++)
            lutMv[i] = lutMv[ 8] + ((lutMv[16] - lutMv[ 8]) * (i -  8) >> 3);
        for (mfxU32 i = 17; i < 32; i++)
            lutMv[i] = lutMv[16] + ((lutMv[32] - lutMv[16]) * (i - 16) >> 4);
        for (mfxU32 i = 33; i < 64; i++)
            lutMv[i] = lutMv[32] + ((lutMv[64] - lutMv[32]) * (i - 32) >> 5);
    }

/*
    mfxU16 GetVmeMvCost(
        mfxU32 const         lutMv[65],
        SVCPAKObject const & mb,
        mfxI16Pair const     mv[32])
    {
        mfxU32 diffx = abs(mb.costCenter0X - mv[0].x) >> 2;
        mfxU32 diffy = abs(mb.costCenter0Y - mv[0].y) >> 2;
        mfxU32 costx = diffx > 64 ? lutMv[64] + ((diffx - 64) >> 2) : lutMv[diffx];
        mfxU32 costy = diffy > 64 ? lutMv[64] + ((diffy - 64) >> 2) : lutMv[diffy];
        return mfxU16(IPP_MIN(0x3ff, costx + costy));
    }
*/

    mfxU16 GetVmeMvCostP(
        mfxU32 const         lutMv[65],
        SVCPAKObject const & mb)
    {
        mfxU32 diffx = abs(mb.costCenter0X - mb.mv[0].x) >> 2;
        mfxU32 diffy = abs(mb.costCenter0Y - mb.mv[0].y) >> 2;
        mfxU32 costx = diffx > 64 ? lutMv[64] + ((diffx - 64) >> 2) : lutMv[diffx];
        mfxU32 costy = diffy > 64 ? lutMv[64] + ((diffy - 64) >> 2) : lutMv[diffy];
        return mfxU16(IPP_MIN(0x3ff, costx + costy));
    }

    mfxU16 GetVmeMvCostB(
        mfxU32 const         lutMv[65],
        SVCPAKObject const & mb)
    {
        mfxU32 diffx0 = abs(mb.costCenter0X - mb.mv[0].x) >> 2;
        mfxU32 diffy0 = abs(mb.costCenter0Y - mb.mv[0].y) >> 2;
        mfxU32 diffx1 = abs(mb.costCenter1X - mb.mv[1].x) >> 2;
        mfxU32 diffy1 = abs(mb.costCenter1Y - mb.mv[1].y) >> 2;
        mfxU32 costx0 = diffx0 > 64 ? lutMv[64] + ((diffx0 - 64) >> 2) : lutMv[diffx0];
        mfxU32 costy0 = diffy0 > 64 ? lutMv[64] + ((diffy0 - 64) >> 2) : lutMv[diffy0];
        mfxU32 costx1 = diffx1 > 64 ? lutMv[64] + ((diffx1 - 64) >> 2) : lutMv[diffx1];
        mfxU32 costy1 = diffy1 > 64 ? lutMv[64] + ((diffy1 - 64) >> 2) : lutMv[diffy1];
        mfxU32 mvCost0 = (IPP_MIN(0x3ff, costx0 + costy0));
        mfxU32 mvCost1 = (IPP_MIN(0x3ff, costx1 + costy1));
        return mfxU16(mvCost0 + mvCost1);
    }

    mfxU8 GetMaxMvsPer2Mb(mfxU32 level)
    {
        return level < 30 ? 126 : (level == 30 ? 32 : 16);
    }

    mfxU32 GetMaxMvLenY(mfxU32 level)
    {
        return level < 11
            ? 63
            : level < 21
                ? 127
                : level < 31
                    ? 255
                    : 511;
    }

    using std::min;
    using std::max;

    mfxU8 Map44LutValue(mfxU32 v, mfxU8 max)
    {
        if(v == 0)        return 0;

        mfxI16 D = (mfxI16)(log((double)v)/log(2.)) - 3;
        if(D < 0)    D = 0;
        mfxI8 ret = (mfxU8)((D << 4) + (int)((v + (D == 0 ? 0 : (1<<(D-1)))) >> D));

        ret =  (ret & 0xf) == 0 ? (ret | 8) : ret;

        if(((ret&15)<<(ret>>4)) > ((max&15)<<(max>>4)))        ret = max;
        return ret;
    }

    void SetCosts(
        mfxVMEUNIIn & costs,
        mfxU32        frameType,
        mfxU32        qp,
        mfxU32        intraSad,
        mfxU32        ftqBasedSkip)
    {
        mfxU16 lambda = QP_LAMBDA[max(0, mfxI32(qp - 12))];

        float had_bias = (intraSad == 3) ? 1.67f : 2.0f;

        Zero(costs);

        //costs.ModeCost[LUTMODE_INTRA_NONPRED] = Map44LutValue((mfxU16)(lambda * 5 * had_bias), 0x6f);
        //costs.ModeCost[LUTMODE_INTRA_16x16]   = 0;
        //costs.ModeCost[LUTMODE_INTRA_8x8]     = Map44LutValue((mfxU16)(lambda * 1.5 * had_bias), 0x8f);
        //costs.ModeCost[LUTMODE_INTRA_4x4]     = Map44LutValue((mfxU16)(lambda * 14 * had_bias), 0x8f);
        costs.ModeCost[LUTMODE_INTRA_NONPRED] = 0;//Map44LutValue((mfxU16)(lambda * 3.5 * had_bias), 0x6f);
        costs.ModeCost[LUTMODE_INTRA_16x16]   = Map44LutValue((mfxU16)(lambda * 10  * had_bias), 0x8f);
        costs.ModeCost[LUTMODE_INTRA_8x8]     = Map44LutValue((mfxU16)(lambda * 14  * had_bias), 0x8f);
        costs.ModeCost[LUTMODE_INTRA_4x4]     = Map44LutValue((mfxU16)(lambda * 35  * had_bias), 0x8f);

        if (frameType & MFX_FRAMETYPE_P)
        {
            costs.ModeCost[LUTMODE_INTRA_NONPRED] = 0;//Map44LutValue((mfxU16)(lambda * 3.5 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTRA_16x16]   = Map44LutValue((mfxU16)(lambda * 10  * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTRA_8x8]     = Map44LutValue((mfxU16)(lambda * 14  * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTRA_4x4]     = Map44LutValue((mfxU16)(lambda * 35  * had_bias), 0x8f);
            
            costs.ModeCost[LUTMODE_INTER_16x16] = Map44LutValue((mfxU16)(lambda * 2.75 * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTER_16x8]  = Map44LutValue((mfxU16)(lambda * 4.25 * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTER_8x8q]  = Map44LutValue((mfxU16)(lambda * 1.32 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTER_8x4q]  = Map44LutValue((mfxU16)(lambda * 2.32 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTER_4x4q]  = Map44LutValue((mfxU16)(lambda * 3.32 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_REF_ID]      = Map44LutValue((mfxU16)(lambda * 2    * had_bias), 0x6f);

            costs.MvCost[0] = Map44LutValue((mfxU16)(lambda * 0.5 * had_bias), 0x6f);
            costs.MvCost[1] = Map44LutValue((mfxU16)(lambda * 2 * had_bias), 0x6f);
            costs.MvCost[2] = Map44LutValue((mfxU16)(lambda * 2.5 * had_bias), 0x6f);
            costs.MvCost[3] = Map44LutValue((mfxU16)(lambda * 4.5 * had_bias), 0x6f);
            costs.MvCost[4] = Map44LutValue((mfxU16)(lambda * 5 * had_bias), 0x6f);
            costs.MvCost[5] = Map44LutValue((mfxU16)(lambda * 6 * had_bias), 0x6f);
            costs.MvCost[6] = Map44LutValue((mfxU16)(lambda * 7 * had_bias), 0x6f);
            costs.MvCost[7] = Map44LutValue((mfxU16)(lambda * 7.5 * had_bias), 0x6f);
        }
        else if (frameType & MFX_FRAMETYPE_B)
        {
            costs.ModeCost[LUTMODE_INTRA_NONPRED] = 0;//Map44LutValue((mfxU16)(lambda * 3.5 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTRA_16x16]   = Map44LutValue((mfxU16)(lambda * 17  * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTRA_8x8]     = Map44LutValue((mfxU16)(lambda * 20  * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTRA_4x4]     = Map44LutValue((mfxU16)(lambda * 40  * had_bias), 0x8f);

            costs.ModeCost[LUTMODE_INTER_16x16] = Map44LutValue((mfxU16)(lambda * 3    * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTER_16x8]  = Map44LutValue((mfxU16)(lambda * 6    * had_bias), 0x8f);
            costs.ModeCost[LUTMODE_INTER_8x8q]  = Map44LutValue((mfxU16)(lambda * 3.25 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTER_8x4q]  = Map44LutValue((mfxU16)(lambda * 4.25 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTER_4x4q]  = Map44LutValue((mfxU16)(lambda * 5.25 * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_INTER_BWD]   = Map44LutValue((mfxU16)(lambda * 1    * had_bias), 0x6f);
            costs.ModeCost[LUTMODE_REF_ID]      = Map44LutValue((mfxU16)(lambda * 2    * had_bias), 0x6f);

            costs.MvCost[0] = Map44LutValue((mfxU16)(lambda * 0 * had_bias), 0x6f);
            costs.MvCost[1] = Map44LutValue((mfxU16)(lambda * 1 * had_bias), 0x6f);
            costs.MvCost[2] = Map44LutValue((mfxU16)(lambda * 1 * had_bias), 0x6f);
            costs.MvCost[3] = Map44LutValue((mfxU16)(lambda * 3 * had_bias), 0x6f);
            costs.MvCost[4] = Map44LutValue((mfxU16)(lambda * 5 * had_bias), 0x6f);
            costs.MvCost[5] = Map44LutValue((mfxU16)(lambda * 6 * had_bias), 0x6f);
            costs.MvCost[6] = Map44LutValue((mfxU16)(lambda * 7 * had_bias), 0x6f);
            costs.MvCost[7] = Map44LutValue((mfxU16)(lambda * 8 * had_bias), 0x6f);
        }

        if (ftqBasedSkip & 1)
        {
            //mfxU32 idx = (qp + 1) >> 1;

            //const mfxU8 FTQ25[26] =
            //{
            //    0,0,0,0,
            //    1,3,6,8,11,
            //    13,16,19,22,26,
            //    30,34,39,44,50,
            //    56,62,69,77,85,
            //    94,104
            //};

            costs.FTXCoeffThresh[0] =
            costs.FTXCoeffThresh[1] =
            costs.FTXCoeffThresh[2] =
            costs.FTXCoeffThresh[3] =
            costs.FTXCoeffThresh[4] =
            costs.FTXCoeffThresh[5] = 0;//FTQ25[idx];
            costs.FTXCoeffThresh_DC = 0;//FTQ25[idx];
        }
    }

    const mfxU16 Dist10[26] =
    {
        0,0,0,0,2,
        4,7,11,17,25,
        35,50,68,91,119,
        153,194,241,296,360,
        432,513,604,706,819,
        944
    };

    const mfxU16 Dist25[26] =
    {
        0,0,0,0,
        12,32,51,69,87,
        107,129,154,184,219,
        260,309,365,431,507,
        594,694,806,933,1074,
        1232,1406
    };

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
                memcpy(&spath.IMESearchPath0to31[0], &SingleSU[0], 32*sizeof(mfxU8));
                maxNumSU = 1;
                break;
            case 3:
                memcpy(&spath.IMESearchPath0to31[0], &RasterScan_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &RasterScan_48x40[32], 24*sizeof(mfxU8));
                break;
            case 4:
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
                break;
            case 5:
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
                maxNumSU = 16;
                break;
            case 6:
            default:
                memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
                break;
            }
        }
        else
        {
            if (meMethod == 6)
            {
                memcpy(&spath.IMESearchPath0to31[0], &Diamond[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &Diamond[32], 24*sizeof(mfxU8));
            }
            else
            {
                memcpy(&spath.IMESearchPath0to31[0], &FullSpiral_48x40[0], 32*sizeof(mfxU8));
                memcpy(&spath.IMESearchPath32to55[0], &FullSpiral_48x40[32], 24*sizeof(mfxU8));
            }
        }

        return maxNumSU;
    }

};


CmContext::CmContext(
    MfxVideoParam const & video,
    CmDevice *            cmDevice)
{
    Setup(video, cmDevice);
}


void CmContext::Setup(
    MfxVideoParam const & video,
    CmDevice *            cmDevice)
{
    m_video  = video;
    m_device = cmDevice;

    if (m_device->CreateQueue(m_queue) != CM_SUCCESS)
        throw CmRuntimeError();

    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);

    widthLa = video.calcParam.widthLa;
    heightLa = video.calcParam.heightLa;
    LaScaleFactor = extDdi->LaScaleFactor;
    m_program = ReadProgram(m_device, genx_hsw_simple_me, SizeOf(genx_hsw_simple_me));

    m_kernelI = CreateKernel(m_device, m_program, "SVCEncMB_I", (void *)SVCEncMB_I);
    m_kernelP = CreateKernel(m_device, m_program, "SVCEncMB_P", (void *)SVCEncMB_P);
    m_kernelB = CreateKernel(m_device, m_program, "SVCEncMB_B", (void *)SVCEncMB_B);

    //m_kernelDownSample4X = CreateKernel(m_device, m_program, "DownSampleMB4X", (void *)DownSampleMB4X);
    //m_kernelDownSample2X = CreateKernel(m_device, m_program, "DownSampleMB2X", (void *)DownSampleMB2X);

    m_curbeData.Reset(m_device, sizeof(SVCEncCURBEData));
    m_nullBuf.Reset(m_device, 4);

    SetCosts(m_costsI, MFX_FRAMETYPE_I, 26, 2, 3);
    SetCosts(m_costsP, MFX_FRAMETYPE_P, 26, 2, 3);
    SetCosts(m_costsB, MFX_FRAMETYPE_B, 26, 2, 3);
    SetLutMv(m_costsP, m_lutMvP);
    SetLutMv(m_costsB, m_lutMvB);
}

CmEvent * CmContext::EnqueueKernel(
    CmKernel *            kernel,
    unsigned int          tsWidth,
    unsigned int          tsHeight,
    CM_DEPENDENCY_PATTERN tsPattern)
{
    int result = CM_SUCCESS;

    if ((result = kernel->SetThreadCount(tsWidth * tsHeight)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmThreadSpace * cmThreadSpace = 0;
    if ((result = m_device->CreateThreadSpace(tsWidth, tsHeight, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    cmThreadSpace->SelectThreadDependencyPattern(tsPattern);

    CmTask * cmTask = 0;
    if ((result = m_device->CreateTask(cmTask)) != CM_SUCCESS)
        throw CmRuntimeError();

    if ((result = cmTask->AddKernel(kernel)) != CM_SUCCESS)
        throw CmRuntimeError();

    CmEvent * e = 0;
    if ((result = m_queue->Enqueue(cmTask, e, cmThreadSpace)) != CM_SUCCESS)
        throw CmRuntimeError();

    m_device->DestroyThreadSpace(cmThreadSpace);
    m_device->DestroyTask(cmTask);

    return e;
}

CmEvent * CmContext::RunVme(
    DdiTask const & task,
    mfxU32          qp)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "RunVme");
    qp = 26;

    CmKernel * kernelPreMe = SelectKernelPreMe(task.m_type[0]);

    SVCEncCURBEData curbeData;
    SetCurbeData(curbeData, task, qp);
    Write(task.m_cmCurbe, &curbeData);

    mfxU32 numMbColsLa = widthLa / 16;
    mfxU32 numMbRowsLa = heightLa / 16;

    if (LaScaleFactor > 1)
    {
        SetKernelArg(kernelPreMe,
            GetIndex(task.m_cmCurbe), GetIndex(task.m_cmRaw), GetIndex(task.m_cmRawLa), *task.m_cmRefsLa,
            GetIndex(task.m_cmMb), task.m_cmRefMb ? GetIndex(task.m_cmRefMb) : GetIndex(m_nullBuf));
    } else
    {
        SetKernelArg(kernelPreMe,
            GetIndex(task.m_cmCurbe), GetIndex(m_nullBuf), GetIndex(task.m_cmRaw), *task.m_cmRefs,
            GetIndex(task.m_cmMb), task.m_cmRefMb ? GetIndex(task.m_cmRefMb) : GetIndex(m_nullBuf));
    }

    CmEvent * e = 0;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Enqueue ME kernel");
        e = EnqueueKernel(kernelPreMe, numMbColsLa, numMbRowsLa, CM_WAVEFRONT26);
    }

    return e;
}

bool CmContext::QueryVme(
    DdiTask const & task,
    CmEvent *       e)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryVme");

    CM_STATUS status = CM_STATUS_QUEUED;
    if (e->GetStatus(status) != CM_SUCCESS)
        throw CmRuntimeError();
    if (status != CM_STATUS_FINISHED)
        return false;

    SVCPAKObject * cmMb = (SVCPAKObject *)task.m_cmMbSys;
    VmeData *      cur  = task.m_vmeData;

    { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Compensate costs");
        mfxVMEUNIIn const & costs = SelectCosts(task.m_type[0]);
        for (size_t i = 0; i < cur->mb.size(); i++)
        {
            SVCPAKObject & mb = cmMb[i];

            if (mb.IntraMbFlag)
            {
                mfxU16 bitCostLambda = mfxU16(Map44LutValueBack(costs.ModeCost[LUTMODE_INTRA_16x16]));
                assert(mb.intraCost >= bitCostLambda);
                mb.dist = mb.intraCost - bitCostLambda;
            }
            else
            {
                if (mb.MbType5Bits != MBTYPE_BP_L0_16x16 &&
                    mb.MbType5Bits != MBTYPE_B_L1_16x16  &&
                    mb.MbType5Bits != MBTYPE_B_Bi_16x16)
                    assert(0);

                mfxU32 modeCostLambda = Map44LutValueBack(costs.ModeCost[LUTMODE_INTER_16x16]);
                mfxU32 mvCostLambda   = (task.m_type[0] & MFX_FRAMETYPE_P)
                    ? GetVmeMvCostP(m_lutMvP, mb) : GetVmeMvCostB(m_lutMvB, mb);
                mfxU16 bitCostLambda = mfxU16(IPP_MIN(mb.interCost, modeCostLambda + mvCostLambda));
                mb.dist = mb.interCost - bitCostLambda;
            }
        }
    }


    { MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "Convert mb data");
        mfxExtPpsHeader const * extPps = GetExtBuffer(m_video);

        cur->intraCost = 0;
        cur->interCost = 0;

        for (size_t i = 0; i < cur->mb.size(); i++)
        {
            cur->mb[i].intraCost     = cmMb[i].intraCost;
            cur->mb[i].interCost     = IPP_MIN(cmMb[i].intraCost, cmMb[i].interCost);
            cur->mb[i].intraMbFlag   = cmMb[i].IntraMbFlag;
            cur->mb[i].skipMbFlag    = cmMb[i].SkipMbFlag;
            cur->mb[i].mbType        = cmMb[i].MbType5Bits;
            cur->mb[i].subMbShape    = cmMb[i].SubMbShape;
            cur->mb[i].subMbPredMode = cmMb[i].SubMbPredMode;
            cur->mb[i].w1            = mfxU8(extPps->weightedBipredIdc == 2 ? CalcBiWeight(task, 0, 0) : 32);
            cur->mb[i].w0            = mfxU8(64 - cur->mb[i].w1);
            cur->mb[i].costCenter0.x = cmMb[i].costCenter0X;
            cur->mb[i].costCenter0.y = cmMb[i].costCenter0Y;
            cur->mb[i].costCenter1.x = cmMb[i].costCenter1X;
            cur->mb[i].costCenter1.y = cmMb[i].costCenter1Y;
            cur->mb[i].dist          = cmMb[i].dist;
            cur->mb[i].propCost      = 0;

            Copy(cur->mb[i].lumaCoeffSum, cmMb[i].lumaCoeffSum);
            Copy(cur->mb[i].lumaCoeffCnt, cmMb[i].lumaCoeffCnt);
            Copy(cur->mb[i].mv, cmMb[i].mv);

            cur->intraCost += cur->mb[i].intraCost;
            cur->interCost += cur->mb[i].interCost;
        }
    }

    return true;
}

#if 0
void CmContext::DownSample(const SurfaceIndex& surf, const SurfaceIndex& surf4X)
{
    mfxU32 numMbCols = m_video.mfx.FrameInfo.Width >> 4;
    mfxU32 numMbRows = m_video.mfx.FrameInfo.Height >> 4;

    // DownSample source
    SetKernelArg(m_kernelDownSample, surf, surf4X);
    CmEvent * e = EnqueueKernel(m_kernelDownSample, numMbCols, numMbRows, CM_NONE_DEPENDENCY);
    //e->WaitForTaskFinished();
    CM_STATUS status;
    do {
        int res = e->GetStatus(status);
        if (res != CM_SUCCESS)
            throw CmRuntimeError();
    } while (status != CM_STATUS_FINISHED);
}

void CmContext::RunVme(
    mfxU32              qp,
    IDirect3DSurface9 * cur,
    IDirect3DSurface9 * fwd, //0 for I frame
    IDirect3DSurface9 * bwd, //0 for P/I frame
    SVCPAKObject *      mb,  //transfer L0 data
    mfxU32 biWeight,
    CmSurface&  cur4X,
    CmSurface&  fwd4X,
    CmSurface&  bwd4X
    )  //transfer L0 data
{
    mfxU32 numMbCols = m_video.mfx.FrameInfo.Width >> 4;
    mfxU32 numMbRows = m_video.mfx.FrameInfo.Height >> 4;
    mfxU32 numMb = numMbCols*numMbRows;
    mfxU16 frameType = MFX_FRAMETYPE_B;

    if(!bwd)
    {
        frameType = MFX_FRAMETYPE_P;
        if(!fwd)
            frameType = MFX_FRAMETYPE_I;
    }

    SVCEncCURBEData curbeData;
    SetCurbeData(curbeData, frameType, qp, biWeight);
    m_curbeData.Write(&curbeData);

    if (curbeData.UseHMEPredictor && (frameType & MFX_FRAMETYPE_PB))
    {
        //fprintf(stderr,"frametype=%d\n", frameType);
            CmKernel * kernelHme = SelectKernelHme(frameType);

            CmSurface2D * fwdRefsArr4X[1] = { (CmSurface2D*)fwd4X  };
            CmSurface2D * bwdRefsArr4X[1] = { (CmSurface2D*)bwd4X };
            CmSurface2D ** fwdRefs4X = fwd ? fwdRefsArr4X : 0;
            CmSurface2D ** bwdRefs4X = bwd ? bwdRefsArr4X : 0;
            
            CmSurfaceVme75 refs4X(m_device, CreateVmeSurfaceG75(m_device, cur4X, fwdRefs4X, bwdRefs4X, !!fwd, !!bwd));

            SetKernelArg(kernelHme,
                m_curbeData.GetIndex(), refs4X, m_hme.GetIndex());

            /*CmEvent * e = */EnqueueKernel(kernelHme, numMbCols >> 2, numMbRows >> 2, CM_NONE_DEPENDENCY);
            //e->WaitForTaskFinished();
/* // No need to wait here because all kernels runs in queue sequentially, just wait last kernel
            CM_STATUS status;
            do {
                int res = e->GetStatus(status);
                if (res != CM_SUCCESS)
                    throw CmRuntimeError();
            } while (status != CM_STATUS_FINISHED);
*/
#if 0
            /* print HME vectors */
            {
                FILE* fvec = fopen("fvec.txt","a+b");
                mfxI16Pair *mv4X = new mfxI16Pair[numMbCols * numMbRows * 2];
                m_hme.Read(mv4X);
                fprintf(fvec, "HME: frame %i type %i\n", 0, frameType);
                for (size_t i = 0; i < numMbRows; i++)
                {
                    for (size_t j = 0; j < numMbCols; j++)
                    {
                        fprintf(fvec, "[%3i,%3i:%3i,%3i]",
                            mv4X[i*(numMbCols*2)+2*j].x, mv4X[i*(numMbCols*2)+2*j].y, 
                            mv4X[i*(numMbCols*2)+2*j+1].x, mv4X[i*(numMbCols*2)+2*j+1].y);
                    }
                    fprintf(fvec,"\n");
                }
                delete[] mv4X;
                fclose(fvec);
            }
#endif

    }

    CmSurface source(m_device, cur);
    CmSurface forward(m_device, fwd);
    CmSurface backward(m_device, bwd);

    CmSurface2D * fwdRefsArr[1] = { forward  };
    CmSurface2D * bwdRefsArr[1] = { backward };
    CmSurface2D ** fwdRefs = fwd ? fwdRefsArr : 0;
    CmSurface2D ** bwdRefs = bwd ? bwdRefsArr : 0;

    CmSurfaceVme75 refs(m_device, CreateVmeSurfaceG75(m_device, source, fwdRefs, bwdRefs, !!fwd, !!bwd));

    CmKernel * kernel = SelectKernel(frameType);
#if 0
    ArrayDpbFrame const & dpb = task.m_dpb[0];
    ArrayU8x33    const & l0  = task.m_list0[0];
    RefMbData * fwdMb = l0.Size() > 0 ? FindByPoc(m_mbData, dpb[l0[0] & 127].m_poc[0]) : 0;
#endif
    RefMbData curMb;
    curMb.mb.Reset(m_device, numMb * sizeof(SVCPAKObject));

    SetKernelArg(kernel,
        m_curbeData.GetIndex(), source.GetIndex(), refs,
        curMb.mb.GetIndex(), m_hme.GetIndex(),
        m_nullBuf.GetIndex()//(fwdMb ? fwdMb->mb : m_nullBuf).GetIndex(),
    );


    {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "EnqueueKernel and GetStatus");
    CmEvent * e = EnqueueKernel(kernel, numMbCols, numMbRows, CM_WAVEFRONT26);

    //e->WaitForTaskFinished();
    CM_STATUS status;
    do {
        int res = e->GetStatus(status);
        if (res != CM_SUCCESS)
            throw CmRuntimeError();
    } while (status != CM_STATUS_FINISHED);

    UINT64 time;
    e->GetExecutionTime(time);
    }

    //printf("\rstatus %d time %.2f ms\n", status, mfxF64(time / 1000.));

    {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "ReadSurface");
    curMb.mb.Read(mb);
    }

    for (size_t i = 0; i < numMb; i++)
    {
        if (mb[i].IntraMbFlag)
        {
            mfxU16 bitCostLambda = mfxU16(Map44LutValueBack(curbeData.ModeCost_1));
            assert(mb[i].intraCost >= bitCostLambda);
            mb[i].dist = mb[i].intraCost - bitCostLambda;
        }
        else
        {
#if 0
            if (mb[i].MbType5Bits != MBTYPE_BP_L0_16x16 &&
                mb[i].MbType5Bits != MBTYPE_B_L1_16x16  &&
                mb[i].MbType5Bits != MBTYPE_B_Bi_16x16)
                assert(0);
#endif
            mfxU32 modeCostLambda = Map44LutValueBack(curbeData.ModeCost_8);
            mfxU32 mvCostLambda   = (frameType & MFX_FRAMETYPE_P)
                ? GetVmeMvCostP(m_lutMvP, mb[i]) : GetVmeMvCostB(m_lutMvB, mb[i]);
            mfxU16 bitCostLambda = mfxU16(IPP_MIN(mb[i].interCost, modeCostLambda + mvCostLambda));
            mb[i].dist = mb[i].interCost - bitCostLambda;
        }
    }


//    Update(m_mbData, task);
}

#endif
CmKernel * CmContext::SelectKernelPreMe(mfxU32 frameType)
{
    switch (frameType & MFX_FRAMETYPE_IPB)
    {
        case MFX_FRAMETYPE_I: return m_kernelI;
        case MFX_FRAMETYPE_P: return m_kernelP;
        case MFX_FRAMETYPE_B: return m_kernelB;
        default: throw CmRuntimeError();
    }
}

CmKernel * CmContext::SelectKernelDownSample(mfxU16 LaScaleFactor)
{
    switch (LaScaleFactor)
    {
        case 2: return m_kernelDownSample2X;
        case 4: return m_kernelDownSample4X;
        default: throw CmRuntimeError();
    }
}

mfxVMEUNIIn & CmContext::SelectCosts(mfxU32 frameType)
{
    switch (frameType & MFX_FRAMETYPE_IPB)
    {
        case MFX_FRAMETYPE_I: return m_costsI;
        case MFX_FRAMETYPE_P: return m_costsP;
        case MFX_FRAMETYPE_B: return m_costsB;
        default: throw CmRuntimeError();
    }
}


void CmContext::SetCurbeData(
    SVCEncCURBEData & curbeData,
    DdiTask const &   task,
    mfxU32            qp)
{
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);
    //mfxExtCodingOption const *    extOpt = GetExtBuffer(m_video);

    mfxU32 interSad       = 2; // 0-sad,2-haar
    mfxU32 intraSad       = 2; // 0-sad,2-haar
    mfxU32 ftqBasedSkip   = 3; //3;
    mfxU32 blockBasedSkip = 1;
    mfxU32 qpIdx          = (qp + 1) >> 1;
    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxU32 skipVal = (task.m_type[0] & MFX_FRAMETYPE_P) ? (Dist10[qpIdx]) : (Dist25[qpIdx]);
    if (!blockBasedSkip)
        skipVal *= 3;
    else if (!transformFlag)
        skipVal /= 2;

    mfxVMEUNIIn costs = {0};
    SetCosts(costs, task.m_type[0], qp, intraSad, ftqBasedSkip);

    mfxVMEIMEIn spath;
    SetSearchPath(spath, task.m_type[0], meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = !(task.m_type[0] & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = (GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F;
    curbeData.BiWeight              = ((task.m_type[0] & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? CalcBiWeight(task, 0, 0) : 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (task.m_type[0] & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 3; // all modes
    curbeData.SkipType              = 0;//!!(task.m_type[0] & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = !(task.m_type[0] & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(task.m_type[0] & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (task.m_type[0] & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
/*
    curbeData.SliceMacroblockHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureWidth          = m_video.mfx.FrameInfo.Width / 16;
*/
    curbeData.SliceMacroblockHeightMinusOne = widthLa / 16 - 1;
    curbeData.PictureHeightMinusOne = heightLa / 16 - 1;
    curbeData.PictureWidth          = widthLa / 16;
    //DW5
    curbeData.RefWidth              = (task.m_type[0] & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (task.m_type[0] & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 2|4; //no4x4 and no8x8 //transformFlag ? 0 : 2/*no8x8*/;
    curbeData.NonSkipZMvAdded       = !!(task.m_type[0] & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = !!(task.m_type[0] & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = !(task.m_type[0] & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(task.m_type[0] & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = costs.ModeCost[LUTMODE_INTRA_NONPRED];
    curbeData.ModeCost_1            = costs.ModeCost[LUTMODE_INTRA_16x16];
    curbeData.ModeCost_2            = costs.ModeCost[LUTMODE_INTRA_8x8];
    curbeData.ModeCost_3            = costs.ModeCost[LUTMODE_INTRA_4x4];
    //DW9
    curbeData.ModeCost_4            = costs.ModeCost[LUTMODE_INTER_16x8];
    curbeData.ModeCost_5            = costs.ModeCost[LUTMODE_INTER_8x8q];
    curbeData.ModeCost_6            = costs.ModeCost[LUTMODE_INTER_8x4q];
    curbeData.ModeCost_7            = costs.ModeCost[LUTMODE_INTER_4x4q];
    //DW10
    curbeData.ModeCost_8            = costs.ModeCost[LUTMODE_INTER_16x16];
    curbeData.ModeCost_9            = costs.ModeCost[LUTMODE_INTER_BWD];
    curbeData.RefIDCost             = costs.ModeCost[LUTMODE_REF_ID];
    curbeData.ChromaIntraModeCost   = costs.ModeCost[LUTMODE_INTRA_CHROMA];
    //DW11
    curbeData.MvCost_0              = costs.MvCost[0];
    curbeData.MvCost_1              = costs.MvCost[1];
    curbeData.MvCost_2              = costs.MvCost[2];
    curbeData.MvCost_3              = costs.MvCost[3];
    //DW12
    curbeData.MvCost_4              = costs.MvCost[4];
    curbeData.MvCost_5              = costs.MvCost[5];
    curbeData.MvCost_6              = costs.MvCost[6];
    curbeData.MvCost_7              = costs.MvCost[7];
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp;
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = costs.FTXCoeffThresh_DC;
    curbeData.FTXCoeffThresh_1                = costs.FTXCoeffThresh[0];
    curbeData.FTXCoeffThresh_2                = costs.FTXCoeffThresh[1];
    //DW15
    curbeData.FTXCoeffThresh_3                = costs.FTXCoeffThresh[2];
    curbeData.FTXCoeffThresh_4                = costs.FTXCoeffThresh[3];
    curbeData.FTXCoeffThresh_5                = costs.FTXCoeffThresh[4];
    curbeData.FTXCoeffThresh_6                = costs.FTXCoeffThresh[5];
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
    curbeData.IntraChromaModeMask             = 1; // 0; !sergo: 1 means Luma only
    curbeData.IntraComputeType                = 0;
    //DW32
    curbeData.SkipVal                         = skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 36;
    curbeData.IntraNonDCPenalty8x8            = 12;
    curbeData.IntraNonDCPenalty4x4            = 4;
    //DW34
    curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 1;  //  0;  !sergo
    curbeData.HMERefWindowsCombiningThreshold = (task.m_type[0] & MFX_FRAMETYPE_B) ? 8 : 16; //  0;  !sergo (should be =8 for B frames)
    curbeData.CheckAllFractionalEnable        = 0;
    //DW37
    curbeData.CurLayerDQId                    = extDdi->LaScaleFactor;  // 0; !sergo use 8 bit as LaScaleFactor
    curbeData.TemporalId                      = 0;
    curbeData.NoInterLayerPredictionFlag      = 1;
    curbeData.AdaptivePredictionFlag          = 0;
    curbeData.DefaultBaseModeFlag             = 0;
    curbeData.AdaptiveResidualPredictionFlag  = 0;
    curbeData.DefaultResidualPredictionFlag   = 0;
    curbeData.AdaptiveMotionPredictionFlag    = 0;
    curbeData.DefaultMotionPredictionFlag     = 0;
    curbeData.TcoeffLevelPredictionFlag       = 0;
    curbeData.UseHMEPredictor                 = 0;  //!!IsOn(extDdi->Hme);
    curbeData.SpatialResChangeFlag            = 0;
    curbeData.isFwdFrameShortTermRef          = task.m_list0[0].Size() > 0 && !task.m_dpb[0][task.m_list0[0][0] & 127].m_longterm;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;
}

void CmContext::SetCurbeData(
    SVCEncCURBEData & curbeData,
    mfxU16            frameType,
    mfxU32            qp,
    mfxU32 biWeight)
{

    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);
    //mfxExtCodingOption const *    extOpt = GetExtBuffer(m_video);

    mfxU32 interSad       = 2; // 0-sad,2-haar
    mfxU32 intraSad       = 2; // 0-sad,2-haar
    mfxU32 ftqBasedSkip   = 3; //3;
    mfxU32 blockBasedSkip = 1;
    mfxU32 qpIdx          = (qp + 1) >> 1;
    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxU32 skipVal = (frameType & MFX_FRAMETYPE_P) ? (Dist10[qpIdx]) : (Dist25[qpIdx]);
    if (!blockBasedSkip)
        skipVal *= 3;
    else if (!transformFlag)
        skipVal /= 2;

    mfxVMEUNIIn costs = {0};
    SetCosts(costs, frameType, qp, intraSad, ftqBasedSkip);

    mfxVMEIMEIn spath;
    SetSearchPath(spath, frameType, meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = !(frameType & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = (GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F;
    curbeData.BiWeight              = ((frameType & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? biWeight : 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (frameType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 3; // all modes
    curbeData.SkipType              = !!(frameType & MFX_FRAMETYPE_B); //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = !(frameType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (frameType & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceMacroblockHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureWidth          = m_video.mfx.FrameInfo.Width / 16;
    //DW5
    curbeData.RefWidth              = (frameType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (frameType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 2|4; //no4x4 and no8x8 //transformFlag ? 0 : 2/*no8x8*/;
    curbeData.NonSkipZMvAdded       = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = !(frameType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(frameType & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = costs.ModeCost[LUTMODE_INTRA_NONPRED];
    curbeData.ModeCost_1            = costs.ModeCost[LUTMODE_INTRA_16x16];
    curbeData.ModeCost_2            = costs.ModeCost[LUTMODE_INTRA_8x8];
    curbeData.ModeCost_3            = costs.ModeCost[LUTMODE_INTRA_4x4];
    //DW9
    curbeData.ModeCost_4            = costs.ModeCost[LUTMODE_INTER_16x8];
    curbeData.ModeCost_5            = costs.ModeCost[LUTMODE_INTER_8x8q];
    curbeData.ModeCost_6            = costs.ModeCost[LUTMODE_INTER_8x4q];
    curbeData.ModeCost_7            = costs.ModeCost[LUTMODE_INTER_4x4q];
    //DW10
    curbeData.ModeCost_8            = costs.ModeCost[LUTMODE_INTER_16x16];
    curbeData.ModeCost_9            = costs.ModeCost[LUTMODE_INTER_BWD];
    curbeData.RefIDCost             = costs.ModeCost[LUTMODE_REF_ID];
    curbeData.ChromaIntraModeCost   = costs.ModeCost[LUTMODE_INTRA_CHROMA];
    //DW11
    curbeData.MvCost_0              = costs.MvCost[0];
    curbeData.MvCost_1              = costs.MvCost[1];
    curbeData.MvCost_2              = costs.MvCost[2];
    curbeData.MvCost_3              = costs.MvCost[3];
    //DW12
    curbeData.MvCost_4              = costs.MvCost[4];
    curbeData.MvCost_5              = costs.MvCost[5];
    curbeData.MvCost_6              = costs.MvCost[6];
    curbeData.MvCost_7              = costs.MvCost[7];
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp;
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = costs.FTXCoeffThresh_DC;
    curbeData.FTXCoeffThresh_1                = costs.FTXCoeffThresh[0];
    curbeData.FTXCoeffThresh_2                = costs.FTXCoeffThresh[1];
    //DW15
    curbeData.FTXCoeffThresh_3                = costs.FTXCoeffThresh[2];
    curbeData.FTXCoeffThresh_4                = costs.FTXCoeffThresh[3];
    curbeData.FTXCoeffThresh_5                = costs.FTXCoeffThresh[4];
    curbeData.FTXCoeffThresh_6                = costs.FTXCoeffThresh[5];
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
    curbeData.IntraComputeType                = 0;
    //DW32
    curbeData.SkipVal                         = skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 36;
    curbeData.IntraNonDCPenalty8x8            = 12;
    curbeData.IntraNonDCPenalty4x4            = 4;
    //DW34
    curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 1;
    curbeData.HMERefWindowsCombiningThreshold = (frameType & MFX_FRAMETYPE_B) ? 8 : 16;;
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
    curbeData.UseHMEPredictor                 = 1;
    curbeData.SpatialResChangeFlag            = 0;
    curbeData.isFwdFrameShortTermRef          = 1;//task.m_list0[0].Size() > 0 && !task.m_dpb[0][task.m_list0[0][0] & 127].m_longterm;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;

#if 0
    mfxExtCodingOptionDDI const * extDdi = GetExtBuffer(m_video);
    //mfxExtCodingOption const *    extOpt = GetExtBuffer(m_video);

    mfxU32 interSad       = 2; // 0-sad,2-haar
    mfxU32 intraSad       = 2; // 0-sad,2-haar
    mfxU32 ftqBasedSkip   = 0; //3;
    mfxU32 blockBasedSkip = 1;
    mfxU32 qpIdx          = (qp + 1) >> 1;
    mfxU32 transformFlag  = 0;//(extOpt->IntraPredBlockSize > 1);
    mfxU32 meMethod       = 6;

    mfxU32 skipVal = (frameType & MFX_FRAMETYPE_P) ? (Dist10[qpIdx]) : (Dist25[qpIdx]);
    if (!blockBasedSkip)
        skipVal *= 3;
    else if (!transformFlag)
        skipVal /= 2;
//    fprintf(stderr,"skipVal=%d\n", skipVal);

    mfxVMEUNIIn costs = {0};
    SetCosts(costs, frameType, qp, intraSad, ftqBasedSkip);

    mfxVMEIMEIn spath;
    SetSearchPath(spath, frameType, meMethod);

    //init CURBE data based on Image State and Slice State. this CURBE data will be
    //written to the surface and sent to all kernels.
    Zero(curbeData);

    //DW0
    curbeData.SkipModeEn            = !(frameType & MFX_FRAMETYPE_I);
    curbeData.AdaptiveEn            = 1;
    curbeData.BiMixDis              = 0;
    curbeData.EarlyImeSuccessEn     = 0;
    curbeData.T8x8FlagForInterEn    = 1;
    curbeData.EarlyImeStop          = 0;
    //DW1
    curbeData.MaxNumMVs             = (GetMaxMvsPer2Mb(m_video.mfx.CodecLevel) >> 1) & 0x3F;
    curbeData.BiWeight              = ((frameType & MFX_FRAMETYPE_B) && extDdi->WeightedBiPredIdc == 2) ? biWeight : 32;
    curbeData.UniMixDisable         = 0;
    //DW2
    curbeData.MaxNumSU              = 57;
    curbeData.LenSP                 = 16;
    //DW3
    curbeData.SrcSize               = 0;
    curbeData.MbTypeRemap           = 0;
    curbeData.SrcAccess             = 0;
    curbeData.RefAccess             = 0;
    curbeData.SearchCtrl            = (frameType & MFX_FRAMETYPE_B) ? 7 : 0;
    curbeData.DualSearchPathOption  = 0;
    curbeData.SubPelMode            = 3; // all modes
    curbeData.SkipType              = 0;//!!(frameType & MFX_FRAMETYPE_B);  //for B 0-16x16, 1-8x8
    curbeData.DisableFieldCacheAllocation = 0;
    curbeData.InterChromaMode       = 0;
    curbeData.FTQSkipEnable         = !(frameType & MFX_FRAMETYPE_I);
    curbeData.BMEDisableFBR         = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.BlockBasedSkipEnabled = blockBasedSkip;
    curbeData.InterSAD              = interSad;
    curbeData.IntraSAD              = intraSad;
    curbeData.SubMbPartMask         = (frameType & MFX_FRAMETYPE_I) ? 0 : 0x7e; // only 16x16 for Inter
    //DW4
    curbeData.SliceMacroblockHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureHeightMinusOne = m_video.mfx.FrameInfo.Height / 16 - 1;
    curbeData.PictureWidth          = m_video.mfx.FrameInfo.Width / 16;
    //DW5
    curbeData.RefWidth              = (frameType & MFX_FRAMETYPE_B) ? 32 : 48;
    curbeData.RefHeight             = (frameType & MFX_FRAMETYPE_B) ? 32 : 40;
    //DW6
    curbeData.BatchBufferEndCommand = BATCHBUFFER_END;
    //DW7
    curbeData.IntraPartMask         = 2|4; //no4x4 and no8x8 //transformFlag ? 0 : 2/*no8x8*/;
    curbeData.NonSkipZMvAdded       = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.NonSkipModeAdded      = !!(frameType & MFX_FRAMETYPE_P);
    curbeData.IntraCornerSwap       = 0;
    curbeData.MVCostScaleFactor     = 0;
    curbeData.BilinearEnable        = 0;
    curbeData.SrcFieldPolarity      = 0;
    curbeData.WeightedSADHAAR       = 0;
    curbeData.AConlyHAAR            = 0;
    curbeData.RefIDCostMode         = !(frameType & MFX_FRAMETYPE_I);
    curbeData.SkipCenterMask        = !!(frameType & MFX_FRAMETYPE_P);
    //DW8
    curbeData.ModeCost_0            = costs.ModeCost[LUTMODE_INTRA_NONPRED];
    curbeData.ModeCost_1            = costs.ModeCost[LUTMODE_INTRA_16x16];
    curbeData.ModeCost_2            = costs.ModeCost[LUTMODE_INTRA_8x8];
    curbeData.ModeCost_3            = costs.ModeCost[LUTMODE_INTRA_4x4];
    //DW9
    curbeData.ModeCost_4            = costs.ModeCost[LUTMODE_INTER_16x8];
    curbeData.ModeCost_5            = costs.ModeCost[LUTMODE_INTER_8x8q];
    curbeData.ModeCost_6            = costs.ModeCost[LUTMODE_INTER_8x4q];
    curbeData.ModeCost_7            = costs.ModeCost[LUTMODE_INTER_4x4q];
    //DW10
    curbeData.ModeCost_8            = costs.ModeCost[LUTMODE_INTER_16x16];
    curbeData.ModeCost_9            = costs.ModeCost[LUTMODE_INTER_BWD];
    curbeData.RefIDCost             = costs.ModeCost[LUTMODE_REF_ID];
    curbeData.ChromaIntraModeCost   = costs.ModeCost[LUTMODE_INTRA_CHROMA];
    //DW11
    curbeData.MvCost_0              = costs.MvCost[0];
    curbeData.MvCost_1              = costs.MvCost[1];
    curbeData.MvCost_2              = costs.MvCost[2];
    curbeData.MvCost_3              = costs.MvCost[3];
    //DW12
    curbeData.MvCost_4              = costs.MvCost[4];
    curbeData.MvCost_5              = costs.MvCost[5];
    curbeData.MvCost_6              = costs.MvCost[6];
    curbeData.MvCost_7              = costs.MvCost[7];
    //DW13
    curbeData.QpPrimeY              = qp;
    curbeData.QpPrimeCb             = qp; //should we use here chroma QP?
    curbeData.QpPrimeCr             = qp;
    curbeData.TargetSizeInWord      = 0xff;
    //DW14
    curbeData.FTXCoeffThresh_DC               = costs.FTXCoeffThresh_DC;
    curbeData.FTXCoeffThresh_1                = costs.FTXCoeffThresh[0];
    curbeData.FTXCoeffThresh_2                = costs.FTXCoeffThresh[1];
    //DW15
    curbeData.FTXCoeffThresh_3                = costs.FTXCoeffThresh[2];
    curbeData.FTXCoeffThresh_4                = costs.FTXCoeffThresh[3];
    curbeData.FTXCoeffThresh_5                = costs.FTXCoeffThresh[4];
    curbeData.FTXCoeffThresh_6                = costs.FTXCoeffThresh[5];
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
    curbeData.IntraComputeType                = 1;
    //DW32
    curbeData.SkipVal                         = skipVal;
    curbeData.MultipredictorL0EnableBit       = 0xEF;
    curbeData.MultipredictorL1EnableBit       = 0xEF;
    //DW33
    curbeData.IntraNonDCPenalty16x16          = 36;
    curbeData.IntraNonDCPenalty8x8            = 12;
    curbeData.IntraNonDCPenalty4x4            = 4;
    //DW34
    curbeData.MaxVmvR                         = GetMaxMvLenY(m_video.mfx.CodecLevel) * 4;
    //DW35
    curbeData.PanicModeMBThreshold            = 0xFF;
    curbeData.SmallMbSizeInWord               = 0xFF;
    curbeData.LargeMbSizeInWord               = 0xFF;
    //DW36
    curbeData.HMECombineOverlap               = 0;
    curbeData.HMERefWindowsCombiningThreshold = 0;
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
    curbeData.UseHMEPredictor                 = 0;
    curbeData.SpatialResChangeFlag            = 0;
    curbeData.isFwdFrameShortTermRef          = 1;//no longterms //task.m_list0[0].Size() > 0 && !task.m_dpb[0][task.m_list0[0][0] & 127].m_longterm;
    //DW38
    curbeData.ScaledRefLayerLeftOffset        = 0;
    curbeData.ScaledRefLayerRightOffset       = 0;
    //DW39
    curbeData.ScaledRefLayerTopOffset         = 0;
    curbeData.ScaledRefLayerBottomOffset      = 0;
#endif

}
}

#endif // MFX_ENABLE_H264_VIDEO_ENCODE_HW
