//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010 - 2014 Intel Corporation. All Rights Reserved.
//
//
//*/


#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#include <d3d9.h>
#include "d3d_allocator.h"
#include "d3d11_allocator.h"
#else
#include <stdarg.h>
#include "vaapi_allocator.h"
#endif

#include "sysmem_allocator.h"
#include "transcode_utils.h"

using namespace TranscodingSample;

// parsing defines
#define IS_SEPARATOR(ch)  ((ch) <= ' ' || (ch) == '=')
#define VAL_CHECK(val, argIdx, argName) \
{ \
    if (val) \
    { \
        PrintHelp(NULL, MSDK_STRING("Input argument number %d \"%s\" require more parameters"), argIdx, argName); \
        return MFX_ERR_UNSUPPORTED;\
    } \
}

#define SIZE_CHECK(cond) \
{ \
    if (cond) \
    { \
        PrintHelp(NULL, MSDK_STRING("Buffer is too small")); \
        return MFX_ERR_UNSUPPORTED; \
    } \
}

msdk_tick TranscodingSample::GetTick()
{
    return msdk_time_get_tick();
}

mfxF64 TranscodingSample::GetTime(msdk_tick start)
{
    static msdk_tick frequency = msdk_time_get_frequency();

    return MSDK_GET_TIME(msdk_time_get_tick(), start, frequency);
}

void TranscodingSample::PrintHelp(msdk_char *strAppName, msdk_char *strErrorMessage, ...)
{
    msdk_printf(MSDK_STRING("Intel(R) Media SDK Multi Transcoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        va_list args;
        msdk_printf(MSDK_STRING("ERROR: "));
        va_start(args, strErrorMessage);
        msdk_vprintf(strErrorMessage, args);
        va_end(args);
        msdk_printf(MSDK_STRING("\n\n"));
    }

    msdk_printf(MSDK_STRING("Command line parameters\n"));

    if (!strAppName) strAppName = MSDK_STRING("sample_multi_transcode");
    msdk_printf(MSDK_STRING("Usage: %s [options] [--] pipeline-description\n"), strAppName);
    msdk_printf(MSDK_STRING("   or: %s [options] -par ParFile\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Options:\n"));
    //                     ("  ............xx
    msdk_printf(MSDK_STRING("  -?            Print this help and exit\n"));
    msdk_printf(MSDK_STRING("  -p <file-name>\n"));
    msdk_printf(MSDK_STRING("                Collect performance statistics in specified file\n"));
    msdk_printf(MSDK_STRING("  -timeout <seconds>\n"));
    msdk_printf(MSDK_STRING("                Set time to run transcoding in seconds\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (general options):\n"));
    msdk_printf(MSDK_STRING("  -i::h265|h264|mpeg2|vc1|mvc|jpeg <file-name>\n"));
    msdk_printf(MSDK_STRING("                Set input file and decoder type\n"));
    msdk_printf(MSDK_STRING("  -o::h265|h264|mpeg2|mvc|jpeg <file-name>\n"));
    msdk_printf(MSDK_STRING("                Set output file and encoder type\n"));
    msdk_printf(MSDK_STRING("  -sw|-hw|-hw_d3d11\n"));
    msdk_printf(MSDK_STRING("                SDK implementation to use: \n"));
    msdk_printf(MSDK_STRING("                      -hw - platform-specific on default display adapter\n"));
    msdk_printf(MSDK_STRING("                      -hw_d3d11 - platform-specific via d3d11\n"));
    msdk_printf(MSDK_STRING("                      -sw - software (default)\n"));
    msdk_printf(MSDK_STRING("  -async        Depth of asynchronous pipeline. default value 1\n"));
    msdk_printf(MSDK_STRING("  -join         Join session with other session(s), by default sessions are not joined\n"));
    msdk_printf(MSDK_STRING("  -priority     Use priority for join sessions. 0 - Low, 1 - Normal, 2 - High. Normal by default\n"));
    msdk_printf(MSDK_STRING("  -n            Number of frames to transcode \n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (encoding options):\n"));
    msdk_printf(MSDK_STRING("  -b <Kbits per second>\n"));
    msdk_printf(MSDK_STRING("                Encoded bit rate, valid for H.264, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("  -f <frames per second>\n"));
    msdk_printf(MSDK_STRING("                Video frame rate for the whole pipelin, overwrites input stream's framerate is taken\n"));
    msdk_printf(MSDK_STRING("  -u 1|4|7      Target usage: quality (1), balanced (4) or speed (7); valid for H.264, MPEG2 and MVC encoders. Default is balanced\n"));
    msdk_printf(MSDK_STRING("  -q <quality>  Quality parameter for JPEG encoder; in range [1,100], 100 is the best quality\n"));
    msdk_printf(MSDK_STRING("  -l numSlices  Number of slices for encoder; default value 0 \n"));
    msdk_printf(MSDK_STRING("  -la           Use the look ahead bitrate control algorithm (LA BRC) for H.264 encoder. Supported only with -hw option on 4th Generation Intel Core processors. \n"));
    msdk_printf(MSDK_STRING("  -lad depth    Depth parameter for the LA BRC, the number of frames to be analyzed before encoding. In range [10,100]. \n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Pipeline description (vpp options):\n"));
    msdk_printf(MSDK_STRING("  -deinterlace  Forces VPP to deinterlace input stream\n"));
    msdk_printf(MSDK_STRING("  -angle 180    Enables 180 degrees picture rotation user module before encoding\n"));
    msdk_printf(MSDK_STRING("  -opencl       Uses implementation of rotation plugin (enabled with -angle option) through Intel(R) OpenCL\n"));
    msdk_printf(MSDK_STRING("  -w            Destination picture width, invokes VPP resize\n"));
    msdk_printf(MSDK_STRING("  -h            Destination picture height, invokes VPP resize\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("ParFile format:\n"));
    msdk_printf(MSDK_STRING("  ParFile is extension of what can be achieved by setting pipeline in the command\n"));
    msdk_printf(MSDK_STRING("line. For more information on ParFile format see readme-multi-transcode.pdf\n"));
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Examples:\n"));
    msdk_printf(MSDK_STRING("  %s -i::mpeg2 in.mpeg2 -o::h264 out.h264\n"), strAppName);
    msdk_printf(MSDK_STRING("  %s -i::mvc in.mvc -o::mvc out.mvc -w 320 -h 240\n"), strAppName);
}

void TranscodingSample::PrintInfo(mfxU32 session_number, sInputParams* pParams, mfxVersion *pVer)
{
    msdk_char buf[2048];
    MSDK_CHECK_POINTER_NO_RET(pVer);

    if ((MFX_IMPL_AUTO <= pParams->libType) && (MFX_IMPL_HARDWARE4 >= pParams->libType))
    {
        msdk_printf(MSDK_STRING("MFX %s Session %d API ver %d.%d parameters: \n"),
            (MFX_IMPL_SOFTWARE == pParams->libType)? MSDK_STRING("SOFTWARE") : MSDK_STRING("HARDWARE"),
                 session_number,
                 pVer->Major,
                 pVer->Minor);
    }

    if (0 == pParams->DecodeId)
        msdk_printf(MSDK_STRING("Input  video: From parent session\n"));
    else
        msdk_printf(MSDK_STRING("Input  video: %s\n"), CodecIdToStr(pParams->DecodeId).c_str());

    // means that source is parent session
    if (0 == pParams->EncodeId)
        msdk_printf(MSDK_STRING("Output video: To child session\n"));
    else
        msdk_printf(MSDK_STRING("Output video: %s\n"), CodecIdToStr(pParams->EncodeId).c_str());
    if (PrintDllInfo(buf, MSDK_ARRAY_LEN(buf), pParams))
        msdk_printf(MSDK_STRING("MFX dll: %s\n"),buf);
    msdk_printf(MSDK_STRING("\n"));
}

bool TranscodingSample::PrintDllInfo(msdk_char* buf, mfxU32 buf_size, sInputParams* pParams)
{
#if defined(_WIN32) || defined(_WIN64)
    HANDLE   hCurrent = GetCurrentProcess();
    HMODULE *pModules;
    DWORD    cbNeeded;
    int      nModules;
    if (NULL == EnumProcessModules(hCurrent, NULL, 0, &cbNeeded))
        return false;

    nModules = cbNeeded / sizeof(HMODULE);

    pModules = new HMODULE[nModules];
    if (NULL == pModules)
    {
        return false;
    }
    if (NULL == EnumProcessModules(hCurrent, pModules, cbNeeded, &cbNeeded))
    {
        delete []pModules;
        return false;
    }

    for (int i = 0; i < nModules; i++)
    {
        GetModuleFileName(pModules[i], buf, buf_size);
        if (_tcsstr(buf, MSDK_STRING("libmfxhw")) && (MFX_IMPL_SOFTWARE != pParams->libType))
        {
            delete []pModules;
            return true;
        }
        else if (_tcsstr(buf, MSDK_STRING("libmfxsw")) && (MFX_IMPL_SOFTWARE == pParams->libType))
        {
            delete []pModules;
            return true;
        }

    }
    delete []pModules;
    return false;
#else
    return false;
#endif
}

CmdProcessor::CmdProcessor()
{
    m_SessionParamId = 0;
    m_SessionArray.clear();
    m_PerfFILE = NULL;
    m_parName = NULL;
    m_nTimeout = 0;

} //CmdProcessor::CmdProcessor()

CmdProcessor::~CmdProcessor()
{
    m_SessionArray.clear();
    if (m_PerfFILE)
        fclose(m_PerfFILE);

} //CmdProcessor::~CmdProcessor()

void CmdProcessor::PrintParFileName()
{
    if (m_parName && m_PerfFILE)
    {
        msdk_fprintf(m_PerfFILE, MSDK_STRING("Input par file: %s\n\n"), m_parName);
    }
}

mfxStatus CmdProcessor::ParseCmdLine(int argc, msdk_char *argv[])
{
    FILE *parFile = NULL;
    mfxStatus sts = MFX_ERR_UNSUPPORTED;

    if (1 == argc)
    {
       PrintHelp(argv[0], NULL);
       return MFX_ERR_UNSUPPORTED;
    }

    --argc;
    ++argv;

    while (argv[0])
    {
        if (0 == msdk_strcmp(argv[0], MSDK_STRING("-par")))
        {
            if (parFile)
            {
                msdk_printf(MSDK_STRING("error: too many parfiles\n"));
                return MFX_ERR_UNSUPPORTED;
            }
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-par' option\n"));
            }
            m_parName = argv[0];
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-timeout")))
        {
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-timeout' option\n"));
            }
            if (1 != msdk_sscanf(argv[0], MSDK_STRING("%d"), &m_nTimeout))
            {
                msdk_printf(MSDK_STRING("error: -timeout \"%s\" is invalid"), argv[0]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-?")) )
        {
            PrintHelp(argv[0], NULL);
            return MFX_ERR_UNSUPPORTED;
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("-p")))
        {
            if (m_PerfFILE)
            {
                msdk_printf(MSDK_STRING("error: only one performance file is supported"));
                return MFX_ERR_UNSUPPORTED;
            }
            --argc;
            ++argv;
            if (!argv[0]) {
                msdk_printf(MSDK_STRING("error: no argument given for '-p' option\n"));
            }
            MSDK_FOPEN(m_PerfFILE, argv[0], MSDK_STRING("w"));
            if (NULL == m_PerfFILE)
            {
                msdk_printf(MSDK_STRING("error: performance file \"%s\" not found"), argv[0]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[0], MSDK_STRING("--")))
        {
            // just skip separator "--" which delimits cmd options and pipeline settings
            break;
        }
        else
        {
            break;
        }
        --argc;
        ++argv;
    }

    msdk_printf(MSDK_STRING("Intel(R) Media SDK Multi Transcoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    //Read pipeline from par file
    if (m_parName && !argv[0])
    {
        MSDK_FOPEN(parFile, m_parName, MSDK_STRING("r"));
        if (NULL == parFile)
        {
            msdk_printf(MSDK_STRING("error: ParFile \"%s\" not found\n"), m_parName);
            return MFX_ERR_UNSUPPORTED;
        }

        if (NULL != m_parName) msdk_printf(MSDK_STRING("Par file is: %s\n\n"), m_parName);

        sts = ParseParFile(parFile);

        if (MFX_ERR_NONE != sts)
        {
            PrintHelp(argv[0], MSDK_STRING("Command line in par file is invalid"));
            fclose(parFile);
            return sts;
        }

        fclose(parFile);
    }
    //Read pipeline from cmd line
    else if (!argv[0])
    {
        msdk_printf(MSDK_STRING ("error: pipeline description not found\n"));
        return MFX_ERR_UNSUPPORTED;
    }
    else if (argv[0] && m_parName)
    {
         msdk_printf(MSDK_STRING ("error: simultaneously enabling parfile and description pipeline from command line forbidden\n"));
         return MFX_ERR_UNSUPPORTED;
    }
    else
    {
         sts = ParseParamsForOneSession(argc, argv);
         if (MFX_ERR_NONE != sts)
         {
               msdk_printf(MSDK_STRING("error: pipeline description is invalid\n"));
               return sts;
         }
    }

    return sts;

} //mfxStatus CmdProcessor::ParseCmdLine(int argc, msdk_char *argv[])

mfxStatus CmdProcessor::ParseParFile(FILE *parFile)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED;
    if (!parFile)
        return MFX_ERR_UNSUPPORTED;

    mfxU32 currPos = 0;
    mfxU32 lineIndex = 0;

    // calculate file size
    fseek(parFile, 0, SEEK_END);
    mfxU32 fileSize = ftell(parFile) + 1;
    fseek(parFile, 0, SEEK_SET);

    // allocate buffer for parsing
    s_ptr<msdk_char, false> parBuf;
    parBuf.reset(new msdk_char[fileSize]);
    msdk_char *pCur;

    while(currPos < fileSize)
    {
        pCur = /*_fgetts*/msdk_fgets(parBuf.get(), fileSize, parFile);
        if (!pCur)
            return MFX_ERR_NONE;
        while(pCur[currPos] != '\n' && pCur[currPos] != 0)
        {
            currPos++;
            if  (pCur + currPos >= parBuf.get() + fileSize)
                return sts;
        }
        // zero string
        if (!currPos)
            continue;

        sts = TokenizeLine(pCur, currPos);
        if (MFX_ERR_NONE != sts)
            PrintHelp(NULL, MSDK_STRING("Error in par file parameters at line %d"), lineIndex);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        currPos = 0;
        lineIndex++;
    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::ParseParFile(FILE *parFile)

mfxStatus CmdProcessor::TokenizeLine(msdk_char *pLine, mfxU32 length)
{
    mfxU32 i;
    const mfxU8 maxArgNum = 255;
    msdk_char *argv[maxArgNum+1];
    mfxU32 argc = 0;
    s_ptr<msdk_char, false> pMemLine;

    pMemLine.reset(new msdk_char[length+2]);

    msdk_char *pTempLine = pMemLine.get();
    pTempLine[0] = ' ';
    pTempLine++;

    MSDK_MEMCPY_BUF(pTempLine,0 , length*sizeof(msdk_char), pLine, length*sizeof(msdk_char));

    // parse into command streams
    for (i = 0; i < length ; i++)
    {
        // check if separator
        if (IS_SEPARATOR(pTempLine[-1]) && !IS_SEPARATOR(pTempLine[0]))
        {
            argv[argc++] = pTempLine;
            if (argc > maxArgNum)
            {
                PrintHelp(NULL, MSDK_STRING("Too many parameters (reached maximum of %d)"), maxArgNum);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        if (pTempLine[0] == ' ')
        {
            pTempLine[0] = 0;
        }
        pTempLine++;
    }

    // EOL for last parameter
    pTempLine[0] = 0;

    return ParseParamsForOneSession(argc, argv);
}

mfxStatus CmdProcessor::ParseParamsForOneSession(mfxU32 argc, msdk_char *argv[])
{
    mfxStatus sts;
    mfxU32 i;
    mfxU32 skipped = 0;
    TranscodingSample::sInputParams InputParams;
    msdk_char endC;
    if (m_nTimeout)
        InputParams.nTimeout = m_nTimeout;

    if (0 == msdk_strcmp(argv[0], MSDK_STRING("set")))
    {
        if (argc != 3) {
            msdk_printf(MSDK_STRING("error: number of arguments for 'set' options is wrong"));
            return MFX_ERR_UNSUPPORTED;
        }
        sts = ParseOption__set(argv[1], argv[2]);
        return sts;
    }
    for (i = 0; i < argc; i++)
    {
        // process multi-character options
        if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::mpeg2")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_MPEG2;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::h265")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_HEVC;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::h264")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_AVC;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::vc1")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_VC1;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::mvc")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_AVC;
            InputParams.bIsMVC = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::jpeg")))
        {
            // only encode supports
            if (InputParams.eMode == Source)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strSrcFile));
            msdk_strcopy(InputParams.strSrcFile, argv[i]);
            InputParams.DecodeId = MFX_CODEC_JPEG;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::mpeg2")))
        {
            // only decode supports
            if (InputParams.eMode == Sink)
                return MFX_ERR_UNSUPPORTED;

            // In case of MVC only MVC-MVC transcoding is supported
            if (InputParams.bIsMVC)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_strcopy(InputParams.strDstFile, argv[i]);
            InputParams.EncodeId = MFX_CODEC_MPEG2;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::h265")))
        {
            // only decode supports
            if (InputParams.eMode == Sink)
                return MFX_ERR_UNSUPPORTED;

            // In case of MVC only MVC-MVC transcoding is supported
            if (InputParams.bIsMVC)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_strcopy(InputParams.strDstFile, argv[i]);
            InputParams.EncodeId = MFX_CODEC_HEVC;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::h264")))
        {
            // only decode supports
            if (InputParams.eMode == Sink)
                return MFX_ERR_UNSUPPORTED;

            // In case of MVC only MVC-MVC transcoding is supported
            if (InputParams.bIsMVC)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_strcopy(InputParams.strDstFile, argv[i]);
            InputParams.EncodeId = MFX_CODEC_AVC;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::mvc")))
        {
            // only decode supports
            if (InputParams.eMode == Sink)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_strcopy(InputParams.strDstFile, argv[i]);
            InputParams.EncodeId = MFX_CODEC_AVC;
            InputParams.bIsMVC = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::jpeg")))
        {
            // only decode supports
            if (InputParams.eMode == Sink)
                return MFX_ERR_UNSUPPORTED;

            // In case of MVC only MVC-MVC transcoding is supported
            if (InputParams.bIsMVC)
                return MFX_ERR_UNSUPPORTED;

            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            SIZE_CHECK((msdk_strlen(argv[i])+1) > MSDK_ARRAY_LEN(InputParams.strDstFile));
            msdk_strcopy(InputParams.strDstFile, argv[i]);
            InputParams.EncodeId = MFX_CODEC_JPEG;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-sw")))
        {
            InputParams.libType = MFX_IMPL_SOFTWARE;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-hw")))
        {
            InputParams.libType = MFX_IMPL_HARDWARE_ANY;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-hw_d3d11")))
        {
            InputParams.libType = MFX_IMPL_HARDWARE_ANY | MFX_IMPL_VIA_D3D11;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-perf_opt")))
        {
            InputParams.bIsPerf = true;
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-f")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%lf%c"), &InputParams.dFrameRate, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("frameRate \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-b")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%d%c"), &InputParams.nBitRate, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("bitRate \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-u")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nTargetUsage, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING(" \"%s\" target usage is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if(0 == msdk_strcmp(argv[i], MSDK_STRING("-q")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nQuality, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING(" \"%s\" quality is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-w")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nDstWidth, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("width \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-h")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nDstHeight, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("height \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-l")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nSlices, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("numSlices \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-async")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nAsyncDepth, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("async \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-join")))
        {
           InputParams.bIsJoin = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-priority")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.priority, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("priority \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("--min-srf")))
        {
            InputParams.FrameNumberPreference = 0;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("--max-srf")))
        {
            InputParams.FrameNumberPreference = 0xFFFFFFFF;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("--srf")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            InputParams.FrameNumberPreference = msdk_atoi(argv[i]);
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-i::source")))
        {
            if (InputParams.eMode != Native)
                return MFX_ERR_UNSUPPORTED;


            InputParams.eMode = Source;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-o::sink")))
        {
             if (InputParams.eMode != Native)
                return MFX_ERR_UNSUPPORTED;


            InputParams.eMode = Sink;

        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-n")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%d%c"), &InputParams.MaxFrameNumber, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("-n \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-angle")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nRotationAngle, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("-angle \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
            if (InputParams.strPluginDLLPath[0] == '\0') {
                msdk_strcopy(InputParams.strPluginDLLPath, MSDK_CPU_ROTATE_PLUGIN);
            }
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-timeout")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%d%c"), &InputParams.nTimeout, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("-timeout \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
            skipped+=2;
        }

        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-opencl")))
        {
            msdk_strcopy(InputParams.strPluginDLLPath, MSDK_OCL_ROTATE_PLUGIN);
        }

        // output PicStruct
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-deinterlace")))
        {
            InputParams.bEnableDeinterlacing = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-la")))
        {
            InputParams.bLABRC = true;
        }
        else if (0 == msdk_strcmp(argv[i], MSDK_STRING("-lad")))
        {
            VAL_CHECK(i+1 == argc, i, argv[i]);
            i++;
            if (1 != msdk_sscanf(argv[i], MSDK_STRING("%hd%c"), &InputParams.nLADepth, &endC, sizeof (endC)))
            {
                PrintHelp(NULL, MSDK_STRING("look ahead depth \"%s\" is invalid"), argv[i]);
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else
        {
            PrintHelp(NULL, MSDK_STRING("Invalid input argument number %d \"%s\""), i, argv[i]);
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if (skipped < argc)
    {
        sts = VerifyAndCorrectInputParams(InputParams);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
        m_SessionArray.push_back(InputParams);
    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::ParseParamsForOneSession(msdk_char *pLine, mfxU32 length)

mfxStatus CmdProcessor::ParseOption__set(msdk_char* strCodecType, msdk_char* strPluginPath)
{
    mfxU32 type = MSDK_IMPL_USR;
    mfxU32 codecid = 0;
    msdk_char path[MSDK_MAX_FILENAME_LEN] = {0};

    //Parse codec type - decoder or encoder
    if (0 == msdk_strncmp(MSDK_STRING("-i::"), strCodecType, 4))
    {
        type |= MSDK_VDEC;
    }
    else if (0 == msdk_strncmp(MSDK_STRING("-o::"), strCodecType, 4))
    {
        type |= MSDK_VENC;
    }
    else
    {
        msdk_printf(MSDK_STRING("error: incorrect definition codec type (must be -i:: or -o::)\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    //Parse codec id
    if (0 == msdk_strcmp(strCodecType+4, MSDK_STRING("h265")))
    {
        codecid = MFX_CODEC_HEVC;
    }
    else if (0 == msdk_strcmp(strCodecType+4, MSDK_STRING("h264")))
    {
        codecid = MFX_CODEC_AVC;
    }
    else if (0 == msdk_strcmp(strCodecType+4, MSDK_STRING("mpeg2")))
    {
        codecid = MFX_CODEC_MPEG2;
    }
    else if (0 == msdk_strcmp(strCodecType+4, MSDK_STRING("vc1")))
    {
        codecid = MFX_CODEC_VC1;
    }
    else
    {
        msdk_printf(MSDK_STRING("error: codec is unsupported\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    //Path to plugin
    msdk_strcopy(path, strPluginPath);

    return msdkSetPluginPath(type, codecid, path);
};

mfxStatus CmdProcessor::VerifyAndCorrectInputParams(TranscodingSample::sInputParams &InputParams)
{
    if (0 == msdk_strlen(InputParams.strSrcFile) && (InputParams.eMode == Sink || InputParams.eMode == Native))
    {
        PrintHelp(NULL, MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == msdk_strlen(InputParams.strDstFile) && (InputParams.eMode == Source || InputParams.eMode == Native))
    {
        PrintHelp(NULL, MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (MFX_CODEC_JPEG != InputParams.EncodeId && MFX_CODEC_MPEG2 != InputParams.EncodeId && MFX_CODEC_AVC != InputParams.EncodeId && MFX_CODEC_HEVC != InputParams.EncodeId && InputParams.eMode != Sink)
    {
        PrintHelp(NULL, MSDK_STRING("Unknown encoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (MFX_CODEC_MPEG2 != InputParams.DecodeId &&
       MFX_CODEC_AVC != InputParams.DecodeId &&
       MFX_CODEC_HEVC != InputParams.DecodeId &&
       MFX_CODEC_VC1 != InputParams.DecodeId &&
       MFX_CODEC_JPEG != InputParams.DecodeId &&
       InputParams.eMode != Source)
    {
        PrintHelp(NULL, MSDK_STRING("Unknown decoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.nQuality && InputParams.EncodeId && (MFX_CODEC_JPEG != InputParams.EncodeId))
    {
        PrintHelp(NULL, MSDK_STRING("-q option is supported only for JPEG encoder\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((InputParams.nTargetUsage || InputParams.nBitRate) && (MFX_CODEC_JPEG == InputParams.EncodeId))
    {
        PrintHelp(NULL, MSDK_STRING("-b and -u options are supported only for H.264, MPEG2 and MVC encoders. For JPEG encoder use -q\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    // set default values for optional parameters that were not set or were set incorrectly
    if (MFX_TARGETUSAGE_BEST_QUALITY != InputParams.nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != InputParams.nTargetUsage)
    {
        InputParams.nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    // Sample does not support user plugin with '-hw_d3d11'
    if ((MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(InputParams.libType)) && InputParams.nRotationAngle != 0)
    {
        PrintHelp(NULL, MSDK_STRING("Sample does not support user plugin with '-hw_d3d11'\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.bLABRC && !(InputParams.libType & MFX_IMPL_HARDWARE_ANY))
    {
        PrintHelp(NULL, MSDK_STRING("Look ahead BRC is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.bLABRC && (InputParams.EncodeId != MFX_CODEC_AVC) && (InputParams.eMode == Source))
    {
        PrintHelp(NULL, MSDK_STRING("Look ahead BRC is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (InputParams.nLADepth && (InputParams.nLADepth < 10 || InputParams.nLADepth > 100))
    {
        PrintHelp(NULL, MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    return MFX_ERR_NONE;

} //mfxStatus CmdProcessor::VerifyAndCorrectInputParams(TranscodingSample::sInputParams &InputParams)

bool  CmdProcessor::GetNextSessionParams(TranscodingSample::sInputParams &InputParams)
{
    if (!m_SessionArray.size())
        return false;
    if (m_SessionParamId == m_SessionArray.size())
    {
        return false;
    }
    InputParams = m_SessionArray[m_SessionParamId];
    m_SessionParamId++;
    return true;

} //bool  CmdProcessor::GetNextSessionParams(TranscodingSample::sInputParams &InputParams)

// Wrapper on standard allocator for concurrent allocation of
// D3D and system surfaces
GeneralAllocator::GeneralAllocator()
{

};
GeneralAllocator::~GeneralAllocator()
{
};
mfxStatus GeneralAllocator::Init(mfxAllocatorParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

#if defined(_WIN32) || defined(_WIN64)
#if MFX_D3D11_SUPPORT
    D3D11AllocatorParams *d3d11AllocParams = dynamic_cast<D3D11AllocatorParams*>(pParams);
    if (d3d11AllocParams)
        m_D3DAllocator.reset(new D3D11FrameAllocator);
    else
#endif
        m_D3DAllocator.reset(new D3DFrameAllocator);
#endif
#ifdef LIBVA_SUPPORT
    m_D3DAllocator.reset(new vaapiFrameAllocator);
#endif

    m_SYSAllocator.reset(new SysMemFrameAllocator);

    sts = m_D3DAllocator.get()->Init(pParams);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    sts = m_SYSAllocator.get()->Init(0);
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    return sts;
}
mfxStatus GeneralAllocator::Close()
{
    mfxStatus sts = MFX_ERR_NONE;
    sts = m_D3DAllocator.get()->Close();
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

    sts = m_SYSAllocator.get()->Close();
    MSDK_CHECK_RESULT(MFX_ERR_NONE, sts, sts);

   return sts;
}

mfxStatus GeneralAllocator::LockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->Lock(m_D3DAllocator.get(), mid, ptr):
                         m_SYSAllocator.get()->Lock(m_SYSAllocator.get(),mid, ptr);
}
mfxStatus GeneralAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *ptr)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->Unlock(m_D3DAllocator.get(), mid, ptr):
                         m_SYSAllocator.get()->Unlock(m_SYSAllocator.get(),mid, ptr);
}

mfxStatus GeneralAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    return isD3DMid(mid)?m_D3DAllocator.get()->GetHDL(m_D3DAllocator.get(), mid, handle):
                         m_SYSAllocator.get()->GetHDL(m_SYSAllocator.get(), mid, handle);
}

mfxStatus GeneralAllocator::ReleaseResponse(mfxFrameAllocResponse *response)
{
    // try to ReleaseResponse via D3D allocator
    return isD3DMid(response->mids[0])?m_D3DAllocator.get()->Free(m_D3DAllocator.get(),response):
                                       m_SYSAllocator.get()->Free(m_SYSAllocator.get(), response);
}
mfxStatus GeneralAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    mfxStatus sts;
    if (request->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET || request->Type & MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)
    {
        sts = m_D3DAllocator.get()->Alloc(m_D3DAllocator.get(), request, response);
        StoreFrameMids(true, response);
    }
    else
    {
        sts = m_SYSAllocator.get()->Alloc(m_SYSAllocator.get(), request, response);
        StoreFrameMids(false, response);
    }
    return sts;
}
void    GeneralAllocator::StoreFrameMids(bool isD3DFrames, mfxFrameAllocResponse *response)
{
    for (mfxU32 i = 0; i < response->NumFrameActual; i++)
        m_Mids.insert(std::pair<mfxHDL, bool>(response->mids[i], isD3DFrames));
}
bool GeneralAllocator::isD3DMid(mfxHDL mid)
{
    std::map<mfxHDL, bool>::iterator it;
    it = m_Mids.find(mid);
    if (it == m_Mids.end())
        return false; // sys mem allocator will check validity of mid further
    else
        return it->second;
}
