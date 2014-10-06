//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
#include "pipeline_camera.h"
#include <sstream>

void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("Intel(R) Camera Expert SDK Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    }

    msdk_printf(MSDK_STRING("Usage: %s [Options] -p PathToCameraPlugin -i InputFileNameBase -o OutputFileNameBase [numberOfFilesToDump]\n"), strAppName);
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-plugin_version ver]                                - define camera plugin version\n"));
    msdk_printf(MSDK_STRING("   [-a asyncDepth] / [-asyncDepth asyncDepth]          - set async depth, default %d \n"), CAM_SAMPLE_ASYNC_DEPTH);
    msdk_printf(MSDK_STRING("   [-b bitDepth] / [-bitDepth bitDepth]                - set bit depth, default 10 \n"));
    msdk_printf(MSDK_STRING("   [-f format] / [-format format]                      - input Bayer format: rggb, bggr, grbg or gbrg\n"));
    msdk_printf(MSDK_STRING("   [-of format] / [-outFormat format]                  - output format: of argb16 or 16 meaning 16 bit ARGB, 8 bit ARGB otherwise\n"));
    msdk_printf(MSDK_STRING("   [-ng] / [-noGamma]                                  - no gamma correction\n"));
    msdk_printf(MSDK_STRING("   [-bdn] / [-bayerDenoise]                            - bayer denoise on\n"));
    msdk_printf(MSDK_STRING("   [-w width] / [-width width]                         - input width, default 4096\n"));
    msdk_printf(MSDK_STRING("   [-h height] / [-height height]                      - input height, default 2160\n"));
    msdk_printf(MSDK_STRING("   [-n numFrames] / [-numFramesToProcess numFrames]    - number of frames to process\n"));
    msdk_printf(MSDK_STRING("   [-alpha alpha]                                      - write value to alpha channel of output surface \n"));
    msdk_printf(MSDK_STRING("   [-pd] / [-padding]                                  - do input surface padding \n"));
    msdk_printf(MSDK_STRING("   [-resetInterval resetInterval]                      - reset interval in frames, default 7 \n"));
    msdk_printf(MSDK_STRING("   [-reset -i ... -o ... -f ... -w ... -h ... ]        -  params to be used after next reset.\n"));
    msdk_printf(MSDK_STRING("       Only the 5 listed above are supported, if a param is not set here, the originally set value is used. \n"));
    msdk_printf(MSDK_STRING("       There can be any number of resets, applied in order of appearance in the command line, \n"));
    msdk_printf(MSDK_STRING("           after resetInterval frames are processed with the current params  \n"));
    msdk_printf(MSDK_STRING("   [-sys]                                              - output to system memory\n"));
#if D3D_SURFACES_SUPPORT
    msdk_printf(MSDK_STRING("   [-d3d]                                              - output to d3d9 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11]                                            - output to d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3di]                                             - input in d3d9 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-d3d11i]                                           - input in to d3d11 surfaces\n"));
    msdk_printf(MSDK_STRING("   [-r]  / [-render]                                   - render output in a separate window \n"));
    msdk_printf(MSDK_STRING("   [-wall w h n m f t tmo]                             - same as -r, and positioned rendering window in a particular cell on specific monitor \n"));
    msdk_printf(MSDK_STRING("       w               - number of columns of video windows on selected monitor\n"));
    msdk_printf(MSDK_STRING("       h               - number of rows of video windows on selected monitor\n"));
    msdk_printf(MSDK_STRING("       n(0,.,w*h-1)    - order of video window in table that will be rendered\n"));
    msdk_printf(MSDK_STRING("       m(0,1..)        - monitor id \n"));
    msdk_printf(MSDK_STRING("       f               - rendering framerate\n"));
    msdk_printf(MSDK_STRING("       t(0/1)          - enable/disable window's title\n"));
    msdk_printf(MSDK_STRING("       tmo             - timeout for -wall option\n"));
#endif
#if defined(_WIN32) || defined(_WIN64)
    msdk_printf(MSDK_STRING("\nFeatures: \n"));
    msdk_printf(MSDK_STRING("   Press 1 to toggle fullscreen rendering on/off\n"));
#endif
    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)        \
{                                      \
    if (2 == msdk_strlen(strInput[i]))     \
    {                                  \
        i++;                           \
        if (strInput[i][0] == MSDK_CHAR('-')) \
        {                              \
            i = i - 1;                 \
        }                              \
        else                           \
        {                              \
            PTR = strInput[i];         \
        }                              \
    }                                  \
    else                               \
    {                                  \
        PTR = strInput[i] + 2;         \
    }                                  \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    sResetParams resPar;

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        // multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-plugin_version")))
        {
            msdk_opt_read(strInput[++i], pParams->CameraPluginVersion);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
        {
            //pParams->bUseHWLib = true;
            // no SW anyway
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3do")))
        {
            pParams->memTypeOut = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11o")))
        {
#if MFX_D3D11_SUPPORT
            pParams->memTypeOut  = D3D11_MEMORY;
#else
            pParams->memTypeIn = pParams->memTypeOut  = D3D9_MEMORY;
#endif
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3di")))
        {
            pParams->memTypeIn = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11i")))
        {
#if MFX_D3D11_SUPPORT
            pParams->memTypeIn = D3D11_MEMORY;
#else
            pParams->memTypeIn = D3D9_MEMORY;
#endif
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sys")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-syso")))
        {
            pParams->memTypeOut = SYSTEM_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-r")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-render")))
        {
            pParams->bRendering = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-a")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-asyncDepth")))
        {
            msdk_opt_read(strInput[++i], pParams->asyncDepth);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-numFramesToProcess")))
        {
            msdk_opt_read(strInput[++i], pParams->nFramesToProceed);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ng")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-noGamma")))
        {
            pParams->bGamma = false;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bdn")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-bayerDenoise")))
        {
            pParams->bBayerDenoise = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pd")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-padding")))
        {
            pParams->bDoPadding = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-i")))
        {
            msdk_strcopy(pParams->strSrcFile, strInput[++i]);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-o")))
        {
            msdk_strcopy(pParams->strDstFile, strInput[++i]);
            pParams->bOutput = true;
            if (i + 1 < nArgNum)  {
                int n;
                if (msdk_opt_read(strInput[++i], n) == MFX_ERR_NONE) {
                    pParams->maxNumBmpFiles = n;
                }
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-f")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-format")))
        {
            i++;
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("bggr")))
                pParams->bayerType     = MFX_CAM_BAYER_BGGR;
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("rggb")))
                pParams->bayerType     = MFX_CAM_BAYER_RGGB;
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("grbg")))
                pParams->bayerType     = MFX_CAM_BAYER_GRBG;
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("gbrg")))
                pParams->bayerType     = MFX_CAM_BAYER_GBRG;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-b")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-bitDepth")))
        {
            msdk_opt_read(strInput[++i], pParams->bitDepth);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-of")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-outFormat")))
        {
            i++;
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("argb16")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("16")))
                pParams->frameInfo[VPP_OUT].FourCC = MFX_FOURCC_ARGB16;
            else
                pParams->frameInfo[VPP_OUT].FourCC = MFX_FOURCC_RGB4;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-w")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-width")))
        {
            msdk_opt_read(strInput[++i], pParams->frameInfo[VPP_IN].nWidth);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-h")) || 0 == msdk_strcmp(strInput[i], MSDK_STRING("-height")))
        {
            msdk_opt_read(strInput[++i], pParams->frameInfo[VPP_IN].nHeight);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-alpha")))
        {
            msdk_opt_read(strInput[++i], pParams->alphaValue);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-reset")))
        {
            resPar.bayerType  = pParams->bayerType;
            msdk_strcopy(resPar.strSrcFile, pParams->strSrcFile);
            msdk_strcopy(resPar.strDstFile, pParams->strDstFile);
            resPar.width = pParams->frameInfo[VPP_IN].nWidth;
            resPar.height = pParams->frameInfo[VPP_IN].nHeight;
            i++;
            for (;i < nArgNum; i++)
            {
                if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-f")))
                {
                    i++;
                    if (0 == msdk_strcmp(strInput[i], MSDK_STRING("bggr")))
                        resPar.bayerType     = MFX_CAM_BAYER_BGGR;
                    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("rggb")))
                        resPar.bayerType     = MFX_CAM_BAYER_RGGB;
                    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("grbg")))
                        resPar.bayerType     = MFX_CAM_BAYER_GRBG;
                    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("gbrg")))
                        resPar.bayerType     = MFX_CAM_BAYER_GBRG;
                }
                else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-w")))
                {
                    msdk_opt_read(strInput[++i], resPar.width);
                }
                else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-h")))
                {
                    msdk_opt_read(strInput[++i], resPar.height);
                }
                else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-i")))
                {
                    msdk_strcopy(resPar.strSrcFile, strInput[++i]);
                }
                else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-o")))
                {
                    msdk_strcopy(resPar.strDstFile, strInput[++i]);
                }
                else
                {
                    i--;
                    break;
                }
            }
            pParams->resetParams.push_back(resPar);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-resetInterval")))
        {
            msdk_opt_read(strInput[++i], pParams->resetInterval);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-?")))
        {
            PrintHelp(strInput[0], NULL);
            return MFX_ERR_UNSUPPORTED;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-wall")))
        {
            if(i + 7 >= nArgNum)
            {
                PrintHelp(strInput[0], MSDK_STRING("Not enough parameters for -wall key"));
                return MFX_ERR_UNSUPPORTED;
            }
            pParams->memType = D3D9_MEMORY;
            pParams->bRendering = true;

            msdk_opt_read(strInput[++i], pParams->nWallW);
            msdk_opt_read(strInput[++i], pParams->nWallH);
            msdk_opt_read(strInput[++i], pParams->nWallCell);
            msdk_opt_read(strInput[++i], pParams->nWallMonitor);
            msdk_opt_read(strInput[++i], pParams->nWallFPS);

            int nTitle;
            msdk_opt_read(strInput[++i], nTitle);

            pParams->bWallNoTitle = 0 == nTitle;

           msdk_opt_read(strInput[++i], pParams->nWallTimeout);
        }
#endif
        else // 1-character options
        {
            std::basic_stringstream<msdk_char> stream;
            stream << MSDK_STRING("Unknown option: ") << strInput[i];
            PrintHelp(strInput[0], stream.str().c_str());
            return MFX_ERR_UNSUPPORTED;
        }
    }

    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (0 == msdk_strlen(pParams->strDstFile))
    {
        pParams->bOutput = false;
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams      Params;   // input parameters from command line
    CCameraPipeline   Pipeline; // pipeline for decoding, includes input file reader, decoder and output file writer

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = Pipeline.Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    // print stream info
    Pipeline.PrintInfo();

    msdk_printf(MSDK_STRING("Camera pipe started\n"));

    LARGE_INTEGER timeBegin, timeEnd, m_Freq;
    QueryPerformanceFrequency(&m_Freq);

    QueryPerformanceCounter(&timeBegin);

    int resetNum = 0;
    for (;;) {
        sts = Pipeline.Run();
        if (MFX_WRN_VIDEO_PARAM_CHANGED == sts) {
            sInputParams *pParams = &Params;
            if (resetNum >= (int)Params.resetParams.size())
                break;
            msdk_strcopy(Params.strSrcFile, Params.resetParams[resetNum].strSrcFile);
            msdk_strcopy(pParams->strDstFile, Params.resetParams[resetNum].strDstFile);
            pParams->frameInfo[VPP_OUT].nWidth = pParams->frameInfo[VPP_IN].nWidth = (mfxU16)Params.resetParams[resetNum].width;
            pParams->frameInfo[VPP_OUT].nHeight = pParams->frameInfo[VPP_IN].nHeight = (mfxU16)Params.resetParams[resetNum].height;
            pParams->frameInfo[VPP_IN].CropW = pParams->frameInfo[VPP_IN].nWidth;
            pParams->frameInfo[VPP_IN].CropH = pParams->frameInfo[VPP_IN].nHeight;
            pParams->frameInfo[VPP_OUT].CropW = pParams->frameInfo[VPP_OUT].nWidth;
            pParams->frameInfo[VPP_OUT].CropH = pParams->frameInfo[VPP_OUT].nHeight;
            pParams->bayerType     = Params.resetParams[resetNum].bayerType;

            //pParams->frameInfo[VPP_OUT].FourCC = MFX_FOURCC_ARGB16;
            //pParams->memTypeIn = pParams->memTypeOut = SYSTEM_MEMORY;

            sts = Pipeline.Reset(&Params);
            if (sts == MFX_ERR_INCOMPATIBLE_VIDEO_PARAM)
            {
                Pipeline.Close();
                sts = Pipeline.Init(&Params);
            }
            if (sts != MFX_ERR_NONE)
                break;
            resetNum++;
        } else
            break;
    }


    QueryPerformanceCounter(&timeEnd);


    double time = ((double)timeEnd.QuadPart - (double)timeBegin.QuadPart)/ (double)m_Freq.QuadPart;

    int frames = Pipeline.GetNumberProcessedFrames();

    _tprintf(_T("Total frames %d \n"), frames);
    _tprintf(_T("Total time   %.2lf sec\n"), time);
    _tprintf(_T("Total FPS    %.2lf fps\n"), frames/time);

    //Pipeline.Close();

    if(MFX_ERR_ABORTED != sts)
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    msdk_printf(MSDK_STRING("\nCamera pipe finished\n"));

    return 0;
}
