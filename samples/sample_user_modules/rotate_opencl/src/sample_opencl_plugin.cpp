//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#include <stdio.h>
#include <stdexcept> // for std::exception on Linux
#include "sample_opencl_plugin.h"

// disable "unreferenced formal parameter" warning - 
// not all formal parameters of interface functions will be used by sample plugin
#pragma warning(disable : 4100) 

//defining module template for generic plugin
#include "mfx_plugin_module.h"
PluginModuleTemplate g_PluginModule = {
    NULL,
    NULL,
    Rotate::CreatePlugin
};

/* Rotate class implementation */
Rotate::Rotate() : 
m_pTasks(NULL),
m_bInited(false),
m_pAlloc(NULL),
m_bIsInOpaque(false),
m_bIsOutOpaque(false),
#if defined(_WIN32) || defined(_WIN64)
m_pD3D9Manager(NULL),
#else
m_pD3D9Manager(NULL),
#endif
m_bOpenCLSurfaceSharing(false)
{
    m_MaxNumTasks = 0;
    
    memset(&m_VideoParam, 0, sizeof(m_VideoParam));
    memset(&m_Param, 0, sizeof(m_Param));

    m_pmfxCore = NULL;
    memset(&m_PluginParam, 0, sizeof(m_PluginParam));
    m_PluginParam.MaxThreadNum = 1;
    m_PluginParam.ThreadPolicy = MFX_THREADPOLICY_SERIAL;
}

Rotate::~Rotate()
{    
    PluginClose();   
    Close();    
}

/* Methods required for integration with Media SDK */
mfxStatus Rotate::PluginInit(mfxCoreInterface *core)
{
    MSDK_CHECK_POINTER(core, MFX_ERR_NULL_PTR);  
    mfxCoreParam core_param;
    mfxStatus sts = core->GetCoreParam(core->pthis, &core_param);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, MFX_ERR_NONE);
    //only 1.8 or above version is supported
    if (core_param.Version.Version < 0x00010008) {
        printf("\nERROR: MSDK v1.8 or above is required to use this plugin \n");
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_SAFE_DELETE(m_pmfxCore);

    m_pmfxCore = new mfxCoreInterface; 
    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_MEMORY_ALLOC);
    *m_pmfxCore = *core;  

    mfxHDL hdl = 0;
#if defined(_WIN32) || defined(_WIN64)
    sts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_D3D9_DEVICE_MANAGER, &hdl);
#else
    sts = m_pmfxCore->GetHandle(m_pmfxCore->pthis, MFX_HANDLE_VA_DISPLAY, &hdl);
#endif

    // SW lib is used if GetHandle return MFX_ERR_NOT_FOUND
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
#if defined(_WIN32) || defined(_WIN64)
    m_pD3D9Manager = reinterpret_cast<IDirect3DDeviceManager9*>(hdl);
#else
    m_pD3D9Manager = reinterpret_cast<VADisplay>(hdl);
#endif

    // if external allocator not set use the one from core interface
    if (!m_pAlloc && m_pmfxCore->FrameAllocator.pthis)
        m_pAlloc = &m_pmfxCore->FrameAllocator;

    return MFX_ERR_NONE;
}

mfxStatus Rotate::PluginClose()
{
    MSDK_SAFE_DELETE(m_pmfxCore); 
    return MFX_ERR_NONE;
}

mfxStatus Rotate::GetPluginParam(mfxPluginParam *par)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);    

    *par = m_PluginParam;

    return MFX_ERR_NONE;
}

mfxStatus Rotate::Submit(const mfxHDL *in, mfxU32 in_num, const mfxHDL *out, mfxU32 out_num, mfxThreadTask *task)
{
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(*in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(*out, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(task, MFX_ERR_NULL_PTR);    
    MSDK_CHECK_NOT_EQUAL(in_num, 1, MFX_ERR_UNSUPPORTED);
    MSDK_CHECK_NOT_EQUAL(out_num, 1, MFX_ERR_UNSUPPORTED);
    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_NOT_INITIALIZED);  
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);  

    mfxFrameSurface1 *surface_in = (mfxFrameSurface1 *)in[0];
    mfxFrameSurface1 *surface_out = (mfxFrameSurface1 *)out[0]; 
    mfxFrameSurface1 *real_surface_in = surface_in;
    mfxFrameSurface1 *real_surface_out = surface_out;

    mfxStatus sts = MFX_ERR_NONE;

    if (m_bIsInOpaque)
    {
        sts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, surface_in, &real_surface_in);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    if (m_bIsOutOpaque)
    {
        sts = m_pmfxCore->GetRealSurface(m_pmfxCore->pthis, surface_out, &real_surface_out);            
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }
    
    // check validity of parameters
    sts = CheckInOutFrameInfo(&real_surface_in->Info, &real_surface_out->Info);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
           
    mfxU32 ind = FindFreeTaskIdx();

    if (ind >= m_MaxNumTasks)
    {
        return MFX_WRN_DEVICE_BUSY; // currently there are no free tasks available
    }
    
    m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &(real_surface_in->Data));
    m_pmfxCore->IncreaseReference(m_pmfxCore->pthis, &(real_surface_out->Data));

    m_pTasks[ind].In = real_surface_in;
    m_pTasks[ind].Out = real_surface_out;
    m_pTasks[ind].bBusy = true;

    switch (m_Param.Angle)
    {
    case 180:
        if (m_bOpenCLSurfaceSharing) {
            m_pTasks[ind].pProcessor = new OpenCLFilterRotator180(&m_OpenCLFilter);
        }
        else {
            m_pTasks[ind].pProcessor = new OpenCLRotator180(m_pOpenCLRotator180Context.get());
        }
        MSDK_CHECK_POINTER(m_pTasks[ind].pProcessor, MFX_ERR_MEMORY_ALLOC);
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    
    m_pTasks[ind].pProcessor->SetAllocator(m_pAlloc);
    m_pTasks[ind].pProcessor->Init(real_surface_in, real_surface_out);
    
    *task = (mfxThreadTask)&m_pTasks[ind];    

    return MFX_ERR_NONE;
}

mfxStatus Rotate::Execute(mfxThreadTask task, mfxU32 uid_p, mfxU32 uid_a)
{    
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_NOT_INITIALIZED);  

    mfxStatus sts = MFX_ERR_NONE;
    RotateTask *current_task = (RotateTask *)task;    

    if (uid_a < 1)
    {
        // there's data to process
        sts = current_task->pProcessor->Process(&m_pChunks[uid_a]);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return MFX_TASK_DONE;
}

mfxStatus Rotate::FreeResources(mfxThreadTask task, mfxStatus sts)
{
    MSDK_CHECK_ERROR(m_bInited, false, MFX_ERR_NOT_INITIALIZED); 
    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_NOT_INITIALIZED);  

    RotateTask *current_task = (RotateTask *)task;
    
    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &(current_task->In->Data));
    m_pmfxCore->DecreaseReference(m_pmfxCore->pthis, &(current_task->Out->Data));
    MSDK_SAFE_DELETE(current_task->pProcessor);
    current_task->bBusy = false;

    return MFX_ERR_NONE;
}

/* Custom methods */
mfxStatus Rotate::Init(mfxVideoParam *mfxParam)
{ 
    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR); 
    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    // whether we use d3d memory on
    int bd3d[2] = { 0,       // input
                    0};      // output

    m_VideoParam = *mfxParam;

    // map opaque surfaces array in case of opaque surfaces
    m_bIsInOpaque = (m_VideoParam.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY) ? true : false;
    m_bIsOutOpaque = (m_VideoParam.IOPattern & MFX_IOPATTERN_OUT_OPAQUE_MEMORY) ? true : false;
    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;

    if (m_bIsInOpaque || m_bIsOutOpaque)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)GetExtBuffer(m_VideoParam.ExtParam, 
            m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // check existence of corresponding allocs
    if ((m_bIsInOpaque && ! pluginOpaqueAlloc->In.Surfaces) || (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces))
       return MFX_ERR_INVALID_VIDEO_PARAM;        
   
    if (m_bIsInOpaque)
    {
        sts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, pluginOpaqueAlloc->In.NumSurface, 
            pluginOpaqueAlloc->In.Type, pluginOpaqueAlloc->In.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);

        bd3d[0] = pluginOpaqueAlloc->In.Type & 
            (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET);
    }
    else
    {
        bd3d[0] = m_VideoParam.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY;
    }

    if (m_bIsOutOpaque)
    {
        sts = m_pmfxCore->MapOpaqueSurface(m_pmfxCore->pthis, pluginOpaqueAlloc->Out.NumSurface, 
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);

        bd3d[1] = pluginOpaqueAlloc->Out.Type & 
            (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET);
    }
    else
    {
        bd3d[1] = m_VideoParam.IOPattern & MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }

    m_MaxNumTasks = 1;

    m_pTasks = new RotateTask [m_MaxNumTasks];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);
    memset(m_pTasks, 0, sizeof(RotateTask) * m_MaxNumTasks);

    m_NumChunks = 1;
    m_pChunks = new DataChunk [m_NumChunks];
    MSDK_CHECK_POINTER(m_pChunks, MFX_ERR_MEMORY_ALLOC);
    memset(m_pChunks, 0, sizeof(DataChunk) * m_NumChunks);

    // divide frame into data chunks
    mfxU32 num_lines_in_chunk = mfxParam->vpp.In.CropH / m_NumChunks; // integer division
    mfxU32 remainder_lines = mfxParam->vpp.In.CropH % m_NumChunks; // get remainder
    // remaining lines are distributed among first chunks (+ extra 1 line each)
    for (mfxU32 i = 0; i < m_NumChunks; i++)
    {
        m_pChunks[i].StartLine = (i == 0) ? 0 : m_pChunks[i-1].EndLine + 1; 
        m_pChunks[i].EndLine = (i < remainder_lines) ? (i + 1) * num_lines_in_chunk : (i + 1) * num_lines_in_chunk - 1;
    }

    // enable surface sharing in case both input and output are d3d surfaces
    if (bd3d[0] && bd3d[1])
        m_bOpenCLSurfaceSharing = true;

    if (m_bOpenCLSurfaceSharing)
    {
        // init OpenCLFilter    
        cl_int error = CL_SUCCESS;
#if defined(_WIN32) || defined(_WIN64)
        error = m_OpenCLFilter.AddKernel(OpenCLFilter::readFile("ocl_rotate.cl").c_str(), "rotate_Y", "rotate_UV", D3DFMT_NV12);
        if (error) return MFX_ERR_DEVICE_FAILED;
        error = m_OpenCLFilter.OCLInit(m_pD3D9Manager);
#else
        error = m_OpenCLFilter.AddKernel(OpenCLFilter::readFile("ocl_rotate.cl").c_str(), "rotate_Y", "rotate_UV", VA_RT_FORMAT_YUV420);
        if (error) return MFX_ERR_DEVICE_FAILED;
        error = m_OpenCLFilter.OCLInit(&m_pD3D9Manager);
#endif

        if (error) 
        {
            error = CL_SUCCESS;
            std::cout << "\nWARNING: Initializing plugin with dx9 sharing failed" << std::endl;
            m_bOpenCLSurfaceSharing = false; // try init plugin without sharing
        }
        else
        {
            error = m_OpenCLFilter.SelectKernel(0);
            if (error) return MFX_ERR_DEVICE_FAILED;
        }

    }

    if (!m_bOpenCLSurfaceSharing)
    {
        try {
            m_pOpenCLRotator180Context.reset(new OpenCLRotator180Context(OpenCLFilter::readFile("ocl_rotate.cl").c_str()));
        }
        catch (const std::exception &err) {
            std::cout << err.what() << std::endl;
            return MFX_ERR_DEVICE_FAILED;
        }
    }

    if (m_bOpenCLSurfaceSharing) {
        msdk_printf(MSDK_STRING("info: using GPU OpenCL processing\n"));
    } else {
        msdk_printf(MSDK_STRING("info: using CPU OpenCL processing\n"));
    }

    m_bInited = true;

    return MFX_ERR_NONE;
}

mfxStatus Rotate::SetAuxParams(void* auxParam, int auxParamSize)
{
    RotateParam *pRotatePar = (RotateParam *)auxParam;
    MSDK_CHECK_POINTER(pRotatePar, MFX_ERR_NULL_PTR); 
    // check validity of parameters
    mfxStatus sts = CheckParam(&m_VideoParam, pRotatePar);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    m_Param = *pRotatePar;
    return MFX_ERR_NONE;
}

mfxStatus Rotate::Close()
{
    if (!m_bInited)
        return MFX_ERR_NONE;
    
    memset(&m_Param, 0, sizeof(RotateParam));

    MSDK_SAFE_DELETE_ARRAY(m_pTasks); 

    mfxStatus sts = MFX_ERR_NONE;

    mfxExtOpaqueSurfaceAlloc* pluginOpaqueAlloc = NULL;
    
    if (m_bIsInOpaque || m_bIsOutOpaque)
    {
        pluginOpaqueAlloc = (mfxExtOpaqueSurfaceAlloc*)
            GetExtBuffer(m_VideoParam.ExtParam, m_VideoParam.NumExtParam, MFX_EXTBUFF_OPAQUE_SURFACE_ALLOCATION);
        MSDK_CHECK_POINTER(pluginOpaqueAlloc, MFX_ERR_INVALID_VIDEO_PARAM);
    }

    // check existence of corresponding allocs
    if ((m_bIsInOpaque && ! pluginOpaqueAlloc->In.Surfaces) || (m_bIsOutOpaque && !pluginOpaqueAlloc->Out.Surfaces))
        return MFX_ERR_INVALID_VIDEO_PARAM;        

    MSDK_CHECK_POINTER(m_pmfxCore, MFX_ERR_NULL_PTR);
    if (m_bIsInOpaque)
    {
        sts = m_pmfxCore->UnmapOpaqueSurface(m_pmfxCore->pthis, pluginOpaqueAlloc->In.NumSurface, 
            pluginOpaqueAlloc->In.Type, pluginOpaqueAlloc->In.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    if (m_bIsOutOpaque)
    {
        sts = m_pmfxCore->UnmapOpaqueSurface(m_pmfxCore->pthis, pluginOpaqueAlloc->Out.NumSurface, 
            pluginOpaqueAlloc->Out.Type, pluginOpaqueAlloc->Out.Surfaces);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_MEMORY_ALLOC);
    }

    m_bInited = false;

    return MFX_ERR_NONE;
}

mfxStatus Rotate::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *in, mfxFrameAllocRequest *out)
{
    MSDK_CHECK_POINTER(par, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(out, MFX_ERR_NULL_PTR);

    in->Info = par->vpp.In;
    in->NumFrameMin = 1;
    in->NumFrameSuggested = in->NumFrameMin + par->AsyncDepth;

    out->Info = par->vpp.Out;
    out->NumFrameMin = 1;
    out->NumFrameSuggested = out->NumFrameMin + par->AsyncDepth;

    return MFX_ERR_NONE;
}

mfxStatus Rotate::SetAllocator(mfxFrameAllocator * pAlloc)
{
    m_pAlloc = pAlloc;
    return MFX_ERR_NONE;
}

mfxStatus Rotate::SetHandle(mfxHandleType type, mfxHDL hdl)
{
#if defined(_WIN32) || defined(_WIN64)
    if (MFX_HANDLE_D3D9_DEVICE_MANAGER == type)
    {        
        m_pD3D9Manager = reinterpret_cast<IDirect3DDeviceManager9 *>(hdl);
    }
#else
    if (MFX_HANDLE_VA_DISPLAY == type)
    {        
        m_pD3D9Manager = reinterpret_cast<VADisplay>(hdl);
    }
#endif
    return MFX_ERR_NONE;
}

/* Internal methods */
mfxU32 Rotate::FindFreeTaskIdx()
{   
    mfxU32 i;
    for (i = 0; i < m_MaxNumTasks; i++)
    {
        if (false == m_pTasks[i].bBusy)
        {
            break;
        }
    }

    return i;
}

mfxStatus Rotate::CheckParam(mfxVideoParam *mfxParam, RotateParam *pRotatePar)
{
    MSDK_CHECK_POINTER(mfxParam, MFX_ERR_NULL_PTR);      

    mfxInfoVPP *pParam = &mfxParam->vpp;    

    // only NV12 color format is supported
    if (MFX_FOURCC_NV12 != pParam->In.FourCC || MFX_FOURCC_NV12 != pParam->Out.FourCC)
    {
        return MFX_ERR_UNSUPPORTED;
    }    

    return MFX_ERR_NONE;
}

mfxStatus Rotate::CheckInOutFrameInfo(mfxFrameInfo *pIn, mfxFrameInfo *pOut)
{
    MSDK_CHECK_POINTER(pIn, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pOut, MFX_ERR_NULL_PTR);

    if (pIn->CropW != m_VideoParam.vpp.In.CropW || pIn->CropH != m_VideoParam.vpp.In.CropH || 
        pIn->FourCC != m_VideoParam.vpp.In.FourCC ||
        pOut->CropW != m_VideoParam.vpp.Out.CropW || pOut->CropH != m_VideoParam.vpp.Out.CropH || 
        pOut->FourCC != m_VideoParam.vpp.Out.FourCC)
    {
        return MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return MFX_ERR_NONE;
}

/* Processor class implementation */
Processor::Processor() 
    : m_pIn(NULL)
    , m_pOut(NULL)
    , m_pAlloc(NULL)
{
}

Processor::~Processor()
{
}

mfxStatus Processor::SetAllocator(mfxFrameAllocator *pAlloc)
{
    m_pAlloc = pAlloc;
    return MFX_ERR_NONE;
}

mfxStatus Processor::Init(mfxFrameSurface1 *frame_in, mfxFrameSurface1 *frame_out)
{
    MSDK_CHECK_POINTER(frame_in, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(frame_out, MFX_ERR_NULL_PTR);

    m_pIn = frame_in;
    m_pOut = frame_out;    

    return MFX_ERR_NONE;        
}

mfxStatus Processor::LockFrame(mfxFrameSurface1 *frame)
{
    MSDK_CHECK_POINTER(frame, MFX_ERR_NULL_PTR);
    //double lock impossible
    if (frame->Data.Y != 0 && frame->Data.MemId !=0)
        return MFX_ERR_UNSUPPORTED;
    //no allocator used, no need to do lock
    if (frame->Data.Y != 0)
        return MFX_ERR_NONE;
    //lock required
    MSDK_CHECK_POINTER(m_pAlloc, MFX_ERR_NULL_PTR);
    return m_pAlloc->Lock(m_pAlloc->pthis, frame->Data.MemId, &frame->Data);
}

mfxStatus Processor::UnlockFrame(mfxFrameSurface1 *frame)
{
    MSDK_CHECK_POINTER(frame, MFX_ERR_NULL_PTR);
    //unlock not possible, no allocator used
    if (frame->Data.Y != 0 && frame->Data.MemId ==0)
        return MFX_ERR_NONE;
    //already unlocked
    if (frame->Data.Y == 0)
        return MFX_ERR_NONE;
    //unlock required
    MSDK_CHECK_POINTER(m_pAlloc, MFX_ERR_NULL_PTR);
    return m_pAlloc->Unlock(m_pAlloc->pthis, frame->Data.MemId, &frame->Data);
}

/* 180 degrees rotator class implementation */
OpenCLFilterRotator180::OpenCLFilterRotator180(OpenCLFilter *pOpenCLFilter) 
    : Processor()
    , m_pOpenCLFilter(pOpenCLFilter)
{
}

OpenCLFilterRotator180::~OpenCLFilterRotator180()
{
}

mfxStatus OpenCLFilterRotator180::Process(DataChunk * /*chunk*/)
{
    mfxStatus sts = MFX_ERR_NONE;
#if defined(_WIN32) || defined(_WIN64)
    IDirect3DSurface9 *in = 0;
    IDirect3DSurface9 *out = 0;
#else
    VASurfaceID* in = NULL;
    VASurfaceID* out = NULL;
#endif
    sts = m_pAlloc->GetHDL(m_pAlloc->pthis, m_pIn->Data.MemId, reinterpret_cast<mfxHDL *>(&in));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    sts = m_pAlloc->GetHDL(m_pAlloc->pthis, m_pOut->Data.MemId, reinterpret_cast<mfxHDL *>(&out));
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    cl_int error = m_pOpenCLFilter->ProcessSurface(m_pIn->Info.Width, m_pIn->Info.Height, in, out);
    if (error) return MFX_ERR_DEVICE_FAILED;

    return MFX_ERR_NONE;
}

OpenCLRotator180Context::OpenCLRotator180Context(const std::string &program_src)
{
    cl_int err = CL_SUCCESS;
    try {
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);
        if (platforms.size() == 0) {
            throw cl::Error(CL_INVALID_PLATFORM, "Couldn't find a platform");
        }

        // find intel platform
        for (std::vector<cl::Platform>::const_iterator it = platforms.begin();
             it != platforms.end(); ++it)
        {
            if (it->getInfo<CL_PLATFORM_NAME>().find("Intel") != std::string::npos)
            {
                m_platform = *it;
                break;
            }
        }

        // create context and device
        cl_context_properties properties[] = 
           { CL_CONTEXT_PLATFORM, (cl_context_properties)m_platform(), 0};
        m_context = cl::Context(CL_DEVICE_TYPE_DEFAULT, properties); 
     
        std::vector<cl::Device> devices = m_context.getInfo<CL_CONTEXT_DEVICES>();
        m_device = devices[0];
     
        // load and build kernels
        cl::Program::Sources source(1,
            std::make_pair(program_src.c_str(),program_src.size()));
        m_program = cl::Program(m_context, source);
        m_program.build(devices);

        m_kernelY = cl::Kernel(m_program, "rotate_Y_packed", &err);
        m_kernelUV = cl::Kernel(m_program, "rotate_UV_packed", &err);
     
        // create command queue
        m_queue = cl::CommandQueue(m_context, m_device, 0, &err);
    }
    catch (const cl::Error &err) {
        throw std::runtime_error(
            "OpenCL error: " 
            + std::string(err.what())
            + "(" + toString(err.err()) + ")");
    }
}

void OpenCLRotator180Context::CreateBuffers(const cl::size_t<3> &Y_size, 
                                            const cl::size_t<3> &UV_size)
{
    const cl::ImageFormat imf(CL_RGBA, CL_UNSIGNED_INT8);
    if (!m_InY())
        m_InY   = cl::Image2D(m_context, CL_MEM_READ_ONLY, imf,  Y_size[0],  Y_size[1]); 
    if (!m_InUV())
        m_InUV  = cl::Image2D(m_context, CL_MEM_READ_ONLY, imf, UV_size[0], UV_size[1]); 
    if (!m_OutY())
        m_OutY  = cl::Image2D(m_context, CL_MEM_WRITE_ONLY, imf,  Y_size[0],  Y_size[1]); 
    if (!m_OutUV())
        m_OutUV = cl::Image2D(m_context, CL_MEM_WRITE_ONLY, imf, UV_size[0], UV_size[1]); 
}

void OpenCLRotator180Context::SetKernelArgs()
{
    m_kernelY .setArg(0, m_InY());
    m_kernelY .setArg(1, m_OutY());
    m_kernelUV.setArg(0, m_InUV());
    m_kernelUV.setArg(1, m_OutUV());
}

void OpenCLRotator180Context::Rotate(size_t width, size_t height,
                                     size_t pitchIn, size_t pitchOut,
                                     void *pInY,  void *pInUV,
                                     void *pOutY, void *pOutUV)
{
    if (!pInY || !pInUV || !pOutY || !pOutUV)
        throw cl::Error(CL_INVALID_VALUE);

    cl::size_t<3> origin  = make_size_t(0, 0, 0);
    cl::size_t<3> Y_size  = make_size_t(width / 4, height, 1);
    cl::size_t<3> UV_size = make_size_t(width / 4, height/2, 1);

    CreateBuffers(Y_size, UV_size);

    SetKernelArgs();

    m_queue.enqueueWriteImage(m_InY, 0, origin, Y_size, pitchIn, 0, pInY);
    m_queue.enqueueWriteImage(m_InUV, 0, origin, UV_size, pitchIn, 0, pInUV);

    m_queue.enqueueNDRangeKernel(m_kernelY, cl::NullRange, 
            cl::NDRange(Y_size[0],Y_size[1]),
            cl::NDRange(1,1));
    m_queue.enqueueNDRangeKernel(m_kernelUV, cl::NullRange, 
            cl::NDRange(UV_size[0],UV_size[1]),
            cl::NDRange(1,1));

    m_queue.enqueueReadImage(m_OutY, 0, origin, Y_size, pitchOut, 0, pOutY);
    m_queue.enqueueReadImage(m_OutUV, 0, origin, UV_size, pitchOut, 0, pOutUV);

    m_queue.finish();
}

OpenCLRotator180::OpenCLRotator180(OpenCLRotator180Context *pOpenCLRotator180Context)
    : m_pOpenCLRotator180Context(pOpenCLRotator180Context)
{
    if (NULL == m_pOpenCLRotator180Context)
        throw std::runtime_error("NULL OpenCLRotator180Context");
}

OpenCLRotator180::~OpenCLRotator180()
{
}

mfxStatus OpenCLRotator180::Process(DataChunk * /*chunk*/)
{
    mfxStatus sts = MFX_ERR_NONE;
    if (MFX_ERR_NONE != (sts = LockFrame(m_pIn))) return sts;
    if (MFX_ERR_NONE != (sts = LockFrame(m_pOut)))
    {
        UnlockFrame(m_pIn);
        return sts;
    }

    try {
        m_pOpenCLRotator180Context->Rotate(m_pIn->Info.Width, m_pIn->Info.Height, 
                                           m_pIn->Data.Pitch, m_pOut->Data.Pitch,
                                           m_pIn->Data.Y, m_pIn->Data.UV,
                                           m_pOut->Data.Y, m_pOut->Data.UV);
    }
    catch (const cl::Error &/*err*/) {
        sts = MFX_ERR_DEVICE_FAILED;
    }

    UnlockFrame(m_pIn);
    UnlockFrame(m_pOut);
    return sts;
}
