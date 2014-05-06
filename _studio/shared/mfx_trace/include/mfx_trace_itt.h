/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2012-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_trace_itt.h

Purpose: contains definition data for file MFX tracing.

*********************************************************************************/

#ifndef __MFX_TRACE_ITT_LOG_H__
#define __MFX_TRACE_ITT_LOG_H__

#include "mfx_trace.h"

#ifdef MFX_TRACE_ENABLE_ITT

/*------------------------------------------------------------------------------*/
/*
// trace registry options and parameters
#define MFX_TRACE_TEXTLOG_REG_FILE_NAME MFX_TRACE_STRING("TextLog")
#define MFX_TRACE_TEXTLOG_REG_SUPPRESS  MFX_TRACE_STRING("TextLogSuppress")
#define MFX_TRACE_TEXTLOG_REG_PERMIT    MFX_TRACE_STRING("TextLogPermit")

// defines suppresses of the output (where applicable)
enum
{
    MFX_TRACE_TEXTLOG_SUPPRESS_FILE_NAME     = 0x01,
    MFX_TRACE_TEXTLOG_SUPPRESS_LINE_NUM      = 0x02,
    MFX_TRACE_TEXTLOG_SUPPRESS_CATEGORY      = 0x04,
    MFX_TRACE_TEXTLOG_SUPPRESS_LEVEL         = 0x08,
    MFX_TRACE_TEXTLOG_SUPPRESS_FUNCTION_NAME = 0x10
};
*/
/*------------------------------------------------------------------------------*/

mfxTraceU32 MFXTraceITT_Init(const mfxTraceChar *filename, mfxTraceU32 output_mode);

mfxTraceU32 MFXTraceITT_SetLevel(mfxTraceChar* category,
                               mfxTraceLevel level);

mfxTraceU32 MFXTraceITT_DebugMessage(mfxTraceStaticHandle *static_handle,
                                   const char *file_name, mfxTraceU32 line_num,
                                   const char *function_name,
                                   mfxTraceChar* category, mfxTraceLevel level,
                                   const char *message,
                                   const char *format, ...);

mfxTraceU32 MFXTraceITT_vDebugMessage(mfxTraceStaticHandle *static_handle,
                                    const char *file_name, mfxTraceU32 line_num,
                                    const char *function_name,
                                    mfxTraceChar* category, mfxTraceLevel level,
                                    const char *message,
                                    const char *format, va_list args);

mfxTraceU32 MFXTraceITT_BeginTask(mfxTraceStaticHandle *static_handle,
                                const char *file_name, mfxTraceU32 line_num,
                                const char *function_name,
                                mfxTraceChar* category, mfxTraceLevel level,
                                const char *task_name, mfxTraceTaskHandle *task_handle,
                                const void *task_params);

mfxTraceU32 MFXTraceITT_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTraceTaskHandle *task_handle);

mfxTraceU32 MFXTraceITT_Close(void);

#endif // #ifdef MFX_TRACE_ENABLE_TEXTLOG
#endif // #ifndef __MFX_TRACE_ITT_LOG_H__
