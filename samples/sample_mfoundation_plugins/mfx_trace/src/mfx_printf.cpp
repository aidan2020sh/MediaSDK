/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_printf.cpp

Mapping of mfxTraceStaticHandle:
    fd1 - char* - file name
    fd2 - UINT32 - line number
    fd3 - char* - function name
    fd4 - char* - task name

Mapping of mfxTaskHandle:
    n/a

*********************************************************************************/

#include <stdio.h>

#include "mfx_printf.h"

//#define DEBUG_PRINT
#ifdef DEBUG_PRINT
#include <atltrace.h>
#endif
/*------------------------------------------------------------------------------*/

UINT32 g_PrintfSuppress = MFX_TRACE_PRINTF_SUPPRESS_FILE_NAME |
                          MFX_TRACE_PRINTF_SUPPRESS_LINE_NUM |
                          MFX_TRACE_PRINTF_SUPPRESS_LEVEL;
FILE* g_mfxTracePrintfFile = NULL;

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_GetRegistryParams(void)
{
    UINT32 sts = 0;
    HRESULT hr = S_OK;
    UINT32 value = 0;

    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           _T(MFX_TRACE_REG_PARAMS_PATH),
                                                           _T(MFX_TRACE_PRINTF_REG_SUPPRESS),
                                                           &value)))
    {
        g_PrintfSuppress = value;
    }
    if (SUCCEEDED(hr) && SUCCEEDED(mfx_trace_get_reg_dword(MFX_TRACE_REG_ROOT,
                                                           _T(MFX_TRACE_REG_PARAMS_PATH),
                                                           _T(MFX_TRACE_PRINTF_REG_PERMIT),
                                                           &value)))
    {
        g_PrintfSuppress &= ~value;
    }
    return sts;
}
/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_Init(const TCHAR *filename, UINT32 output_mode)
{
    UINT32 sts = 0;
    if (!(output_mode & MFX_TRACE_OMODE_FILE)) return 1;
    if (!filename) return 1;

    sts = MFXTracePrintf_Close();
    if (!sts) sts = MFXTracePrintf_GetRegistryParams();
    if (!sts)
    {
        if (!g_mfxTracePrintfFile)
        {
            if (!_tcscmp(filename, _T("stdout"))) g_mfxTracePrintfFile = stdout;
            else g_mfxTracePrintfFile = mfx_trace_tfopen(filename, L"a");
        }
        if (!g_mfxTracePrintfFile) return 1;
    }
    return sts;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_Close(void)
{
    g_PrintfSuppress = MFX_TRACE_PRINTF_SUPPRESS_FILE_NAME |
                       MFX_TRACE_PRINTF_SUPPRESS_LINE_NUM |
                       MFX_TRACE_PRINTF_SUPPRESS_LEVEL;
    if (g_mfxTracePrintfFile)
    {
        fclose(g_mfxTracePrintfFile);
        g_mfxTracePrintfFile = NULL;
    }
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_SetLevel(TCHAR* /*category*/, mfxTraceLevel /*level*/)
{
    return 1;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_DebugMessage(mfxTraceStaticHandle* static_handle,
                                   const char *file_name, UINT32 line_num,
                                   const char *function_name,
                                   TCHAR* category, mfxTraceLevel level,
                                   const char *message, const char *format, ...)
{
    UINT32 res = 0;
    va_list args;

    va_start(args, format);
    res = MFXTracePrintf_vDebugMessage(static_handle,
                                       file_name , line_num,
                                       function_name,
                                       category, level,
                                       message, format, args);
    va_end(args);
    return res;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_vDebugMessage(mfxTraceStaticHandle* /*static_handle*/,
                                    const char *file_name, UINT32 line_num,
                                    const char *function_name,
                                    TCHAR* category, mfxTraceLevel level,
                                    const char *message,
                                    const char *format, va_list args)
{
#ifndef DEBUG_PRINT
    if (!g_mfxTracePrintfFile) return 1;
#endif
    size_t len = MFX_TRACE_MAX_LINE_LENGTH;
    char str[MFX_TRACE_MAX_LINE_LENGTH] = {0}, *p_str = str;

    if (file_name && !(g_PrintfSuppress & MFX_TRACE_PRINTF_SUPPRESS_FILE_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s: ", file_name);
    }
    if (line_num && !(g_PrintfSuppress & MFX_TRACE_PRINTF_SUPPRESS_LINE_NUM))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%d: ", line_num);
    }
    if (category && !(g_PrintfSuppress & MFX_TRACE_PRINTF_SUPPRESS_CATEGORY))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%S: ", category);
    }
    if (!(g_PrintfSuppress & MFX_TRACE_PRINTF_SUPPRESS_LEVEL))
    {
        p_str = mfx_trace_sprintf(p_str, len, "LEV_%d: ", level);
    }
    if (function_name && !(g_PrintfSuppress & MFX_TRACE_PRINTF_SUPPRESS_FUNCTION_NAME))
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s: ", function_name);
    }
    if (message)
    {
        p_str = mfx_trace_sprintf(p_str, len, "%s", message);
    }
    if (format)
    {
        p_str = mfx_trace_vsprintf(p_str, len, format, args);
    }
    {
        p_str = mfx_trace_sprintf(p_str, len, "\n");
    }
#ifdef DEBUG_PRINT
    ATLTRACE2("%s", str);
    if (!g_mfxTracePrintfFile) return 1;
#endif
    fprintf(g_mfxTracePrintfFile, "%s", str);
    fflush(g_mfxTracePrintfFile);
    return 0;
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_BeginTask(mfxTraceStaticHandle *static_handle,
                                const char *file_name, UINT32 line_num,
                                const char *function_name,
                                TCHAR* category, mfxTraceLevel level,
                                const char *task_name, mfxTaskHandle* /*task_handle*/)
{
    if (static_handle)
    {
        static_handle->fd1.str    = (char*)file_name;
        static_handle->fd2.uint32 = line_num;
        static_handle->fd3.str    = (char*)function_name;
        static_handle->fd4.str    = (char*)task_name;
    }
    return MFXTracePrintf_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": +": "+");
}

/*------------------------------------------------------------------------------*/

UINT32 MFXTracePrintf_EndTask(mfxTraceStaticHandle *static_handle,
                              mfxTaskHandle* /*task_handle*/)
{
    if (!static_handle) return 1;

    char *file_name = NULL, *function_name = NULL, *task_name = NULL;
    UINT32 line_num = 0;
    TCHAR* category = NULL;
    mfxTraceLevel level = MFX_TRACE_LEVEL_DEFAULT;

    if (static_handle)
    {
        category      = static_handle->category;
        level         = static_handle->level;

        file_name     = static_handle->fd1.str;
        line_num      = static_handle->fd2.uint32;
        function_name = static_handle->fd3.str;
        task_name     = static_handle->fd4.str;
    }
    return MFXTracePrintf_DebugMessage(static_handle,
                                       file_name, line_num,
                                       function_name,
                                       category, level,
                                       task_name, (task_name)? ": -": "-");
}
