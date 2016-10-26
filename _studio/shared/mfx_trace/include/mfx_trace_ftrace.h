//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015 Intel Corporation. All Rights Reserved.
//

#ifndef __MFX_TRACE_FTRACE_LOG_H__
#define __MFX_TRACE_FTRACE_LOG_H__

#include "mfx_trace.h"

#if defined(MFX_TRACE_ENABLE_FTRACE) && defined(LINUX32)


mfxTraceU32 MFXTraceFtrace_Init(const mfxTraceChar *filename, mfxTraceU32 output_mode);

mfxTraceU32 MFXTraceFtrace_SetLevel(mfxTraceChar* category,
    mfxTraceLevel level);

mfxTraceU32 MFXTraceFtrace_DebugMessage(mfxTraceStaticHandle *static_handle,
    const char *file_name, mfxTraceU32 line_num,
    const char *function_name,
    mfxTraceChar* category, mfxTraceLevel level,
    const char *message,
    const char *format, ...);

mfxTraceU32 MFXTraceFtrace_vDebugMessage(mfxTraceStaticHandle *static_handle,
    const char *file_name, mfxTraceU32 line_num,
    const char *function_name,
    mfxTraceChar* category, mfxTraceLevel level,
    const char *message,
    const char *format, va_list args);

mfxTraceU32 MFXTraceFtrace_BeginTask(mfxTraceStaticHandle *static_handle,
    const char *file_name, mfxTraceU32 line_num,
    const char *function_name,
    mfxTraceChar* category, mfxTraceLevel level,
    const char *task_name, mfxTraceTaskHandle *task_handle,
    const void *task_params);

mfxTraceU32 MFXTraceFtrace_EndTask(mfxTraceStaticHandle *static_handle,
    mfxTraceTaskHandle *task_handle);

mfxTraceU32 MFXTraceFtrace_Close(void);

#endif // defined(MFX_TRACE_ENABLE_FTRACE) && defined(LINUX32)
#endif // #ifndef __MFX_TRACE_FTRACE_LOG_H__
