/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_trace_itt.cpp

Mapping of mfxTraceStaticHandle:
    itt1 - void* - string handle for the task

Mapping of mfxTraceTaskHandle:
    itt1 - UINT32 - task is traced

*********************************************************************************/

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_ITT
extern "C"
{
#include <ittnotify.h>
#include <stdio.h>
#include "mfx_trace_utils.h"
#include "mfx_trace_textlog.h"


#ifdef _WIN32

    //changing wchar functions to char ones
#undef __itt_string_handle_create
#define __itt_string_handle_create __itt_string_handle_createA

#undef __itt_domain_create
#define __itt_domain_create __itt_domain_createA

#endif

static __itt_domain* GetDomain(){
    static __itt_domain* pTheDomain  = __itt_domain_create("MediaSDK");
    return pTheDomain;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_Init(const mfxTraceChar * /*filename*/ , mfxTraceU32 output_mode)
{
    if (!(output_mode & MFX_TRACE_OUTPUT_ITT)) return 1;
    else return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_Close(void)
{
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_SetLevel(mfxTraceChar* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_DebugMessage(mfxTraceStaticHandle* /*static_handle*/,
                                   const char * /*file_name*/, mfxTraceU32 /*line_num*/,
                                   const char * /*function_name*/,
                                   mfxTraceChar* /*category*/, mfxTraceLevel /*level*/,
                                   const char * /*message*/, const char * /*format*/, ...)
{
    mfxTraceU32 res = 0;
    return res;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_vDebugMessage(mfxTraceStaticHandle* //static_handle
                                      ,const char * //file_name
                                      ,mfxTraceU32 //line_num
                                      ,const char * //function_name
                                      ,mfxTraceChar* //category
                                      ,mfxTraceLevel //level
                                      ,const char * //message
                                      ,const char * //format
                                      ,va_list //args
                                      )
{
    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_BeginTask(mfxTraceStaticHandle *static_handle
                                ,const char * //file_name
                                ,mfxTraceU32 //line_num
                                ,const char * //function_name
                                ,mfxTraceChar* //category
                                ,mfxTraceLevel level
                                ,const char * task_name
                                ,mfxTraceTaskHandle *handle
                                ,const void * /*task_params*/)
{
    if (!static_handle || !handle) return 1;

    if (MFX_TRACE_LEVEL_API == level ||
        MFX_TRACE_LEVEL_INTERNAL_VTUNE == level)
    {
        // cache string handle across task instances
        if (NULL == static_handle->itt1.ptr)
        {
            static_handle->itt1.ptr = __itt_string_handle_create(task_name);
        }

        // task is traced
        handle->itt1.uint32 = 1;

        __itt_task_begin(GetDomain(), __itt_null, __itt_null,
            (__itt_string_handle*)static_handle->itt1.ptr);
    }

    return 0;
}

/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_EndTask(mfxTraceStaticHandle * //static_handle
                                ,mfxTraceTaskHandle *handle
                                )
{
    if (!handle) return 1;

    if (1 == handle->itt1.uint32) __itt_task_end(GetDomain());

    return 0;
}

} // extern "C"
#endif // #ifdef MFX_TRACE_ENABLE_ITT
