/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement.
This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
Copyright(c) 2005-2015 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_samples_config.h"

#if defined(_WIN32) || defined(_WIN64)

#include "opencl_filter_dx9.h"
#include "sample_defs.h"

// INTEL DX9 sharing functions declared deprecated in OpenCL 1.2
#pragma warning( disable : 4996 )

#define DX9_MEDIA_SHARING
#define DX_MEDIA_SHARING
#define CL_DX9_MEDIA_SHARING_INTEL_EXT

#include <CL/opencl.h>
#include <CL/cl_d3d9.h>

using std::endl;


clGetDeviceIDsFromDX9INTEL_fn            clGetDeviceIDsFromDX9INTEL             = NULL;
clCreateFromDX9MediaSurfaceINTEL_fn      clCreateFromDX9MediaSurfaceINTEL       = NULL;
clEnqueueAcquireDX9ObjectsINTEL_fn       clEnqueueAcquireDX9ObjectsINTEL        = NULL;
clEnqueueReleaseDX9ObjectsINTEL_fn       clEnqueueReleaseDX9ObjectsINTEL        = NULL;

OpenCLFilterDX9::OpenCLFilterDX9()
{
    for(size_t i=0;i<c_shared_surfaces_num;i++)
    {
        m_hSharedSurfaces[i] = NULL;
        m_pSharedSurfaces[i] = NULL;
    }
}

OpenCLFilterDX9::~OpenCLFilterDX9()
{
    m_SafeD3DDeviceManager.reset();
}

cl_int OpenCLFilterDX9::OCLInit(mfxHDL device)
{
    MSDK_CHECK_POINTER(device, MFX_ERR_NULL_PTR);

    // store IDirect3DDeviceManager9
    try
    {
        m_SafeD3DDeviceManager.reset(new SafeD3DDeviceManager(static_cast<IDirect3DDeviceManager9*>(device)));
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }

    return OpenCLFilterBase::OCLInit(device);
}

cl_int OpenCLFilterDX9::InitSurfaceSharingExtension()
{
    cl_int error = CL_SUCCESS;

    // Check if surface sharing is available
    size_t  len = 0;
    const size_t max_string_size = 1024;
    char extensions[max_string_size];
    error = clGetPlatformInfo(m_clplatform, CL_PLATFORM_EXTENSIONS, max_string_size, extensions, &len);
    log.info() << "OpenCLFilter: Platform extensions: " << extensions << endl;
    if(NULL == strstr(extensions, "cl_intel_dx9_media_sharing"))
    {
        log.error() << "OpenCLFilter: DX9 media sharing not available!" << endl;
        return CL_INVALID_PLATFORM;
    }

    // Hook up the d3d sharing extension functions that we need
    INIT_CL_EXT_FUNC(clGetDeviceIDsFromDX9INTEL);
    INIT_CL_EXT_FUNC(clCreateFromDX9MediaSurfaceINTEL);
    INIT_CL_EXT_FUNC(clEnqueueAcquireDX9ObjectsINTEL);
    INIT_CL_EXT_FUNC(clEnqueueReleaseDX9ObjectsINTEL);

    // Check for success
    if (!clGetDeviceIDsFromDX9INTEL ||
        !clCreateFromDX9MediaSurfaceINTEL ||
        !clEnqueueAcquireDX9ObjectsINTEL ||
        !clEnqueueReleaseDX9ObjectsINTEL)
    {
        log.error() << "OpenCLFilter: Couldn't get all of the media sharing routines" << endl;
        return CL_INVALID_PLATFORM;
    }

    return error;
}

cl_int OpenCLFilterDX9::InitDevice()
{
    log.debug() << "OpenCLFilter: Try to init OCL device" << endl;

    cl_int error = CL_SUCCESS;

    try
    {
        LockedD3DDevice pD3DDevice = m_SafeD3DDeviceManager->LockDevice();
        error = clGetDeviceIDsFromDX9INTEL(m_clplatform, CL_D3D9EX_DEVICE_INTEL, // note - you must use D3D9DeviceEx
                                           pD3DDevice.get(), CL_PREFERRED_DEVICES_FOR_DX9_INTEL, 1, &m_cldevice, NULL);
        if(error) {
            log.error() << "OpenCLFilter: clGetDeviceIDsFromDX9INTEL failed. Error code: " << error << endl;
            return error;
        }

        // Initialize the shared context
        cl_context_properties props[] =
        {
            CL_CONTEXT_D3D9EX_DEVICE_INTEL, (cl_context_properties)pD3DDevice.get(),
            CL_CONTEXT_PLATFORM, (cl_context_properties)m_clplatform,
            CL_CONTEXT_INTEROP_USER_SYNC, (cl_context_properties)CL_TRUE,
            0
        };
        m_clcontext = clCreateContext(props, 1, &m_cldevice, NULL, NULL, &error);
        if(error) {
            log.error() << "OpenCLFilter: clCreateContext failed. Error code: " << error << endl;
            return error;
        }
    }
    catch (const std::exception &ex)
    {
        log.error() << ex.what() << endl;
        return CL_DEVICE_NOT_AVAILABLE;
    }

    log.debug() << "OpenCLFilter: OCL device inited" << endl;

    return error;
}

cl_mem OpenCLFilterDX9::CreateSharedSurface(mfxMemId mid, int nView, bool bIsReadOnly)
{
    mfxHDLPair* mid_pair = static_cast<mfxHDLPair*>(mid);

    IDirect3DSurface9 *surf = (IDirect3DSurface9*)mid_pair->first;
    HANDLE handle = mid_pair->second;

    cl_int error = CL_SUCCESS;
    cl_mem mem = clCreateFromDX9MediaSurfaceINTEL(m_clcontext, bIsReadOnly ? CL_MEM_READ_ONLY : CL_MEM_READ_WRITE,
                                                    surf, handle, nView, &error);
    if (error) {
        log.error() << "clCreateFromDX9MediaSurfaceINTEL failed. Error code: " << error << endl;
        return 0;
    }
    return mem;
}

bool OpenCLFilterDX9::EnqueueAcquireSurfaces(cl_mem* surfaces, int nSurfaces)
{
    cl_int error = clEnqueueAcquireDX9ObjectsINTEL(m_clqueue, nSurfaces, surfaces, 0, NULL, NULL);
    if (error) {
        log.error() << "clEnqueueAcquireDX9ObjectsINTEL failed. Error code: " << error << endl;
        return false;
    }
    return true;
}

bool OpenCLFilterDX9::EnqueueReleaseSurfaces(cl_mem* surfaces, int nSurfaces)
{
    cl_int error = clEnqueueReleaseDX9ObjectsINTEL(m_clqueue, nSurfaces, surfaces, 0, NULL, NULL);
    if (error) {
        log.error() << "clEnqueueReleaseDX9ObjectsINTEL failed. Error code: " << error << endl;
        return false;
    }
    return true;
}

#endif // #if defined(_WIN32) || defined(_WIN64)
