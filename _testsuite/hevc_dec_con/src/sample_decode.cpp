//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.
//
#include "pipeline_decode.h"
#include <sstream>
#include <stdexcept>

#pragma warning(disable: 4297)

mfxStatus MFX_CDECL MFXVideoUSER_Load(mfxSession session, const mfxPluginUID *uid, mfxU32 version)
{
    throw std::runtime_error("No MFXVideoUSER_Load in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    throw std::runtime_error("No MFXVideoVPP_Query in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest request[2])
{
    throw std::runtime_error("No MFXVideoVPP_QueryIOSurf in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_Init(mfxSession session, mfxVideoParam *par)
{
    throw std::runtime_error("No MFXVideoVPP_Init in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_Reset(mfxSession session, mfxVideoParam *par)
{
    throw std::runtime_error("No MFXVideoVPP_Reset in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_Close(mfxSession session)
{
    throw std::runtime_error("No MFXVideoVPP_Close in standalone app.");
}


mfxStatus MFX_CDECL MFXVideoVPP_GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    throw std::runtime_error("No MFXVideoVPP_GetVideoParam in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_GetVPPStat(mfxSession session, mfxVPPStat *stat)
{
    throw std::runtime_error("No MFXVideoVPP_GetVPPStat in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsync(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *out, mfxExtVppAuxData *aux, mfxSyncPoint *syncp)
{
    throw std::runtime_error("No MFXVideoVPP_RunFrameVPPAsync in standalone app.");
}

mfxStatus MFX_CDECL MFXVideoVPP_RunFrameVPPAsyncEx(mfxSession session, mfxFrameSurface1 *in, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out, mfxSyncPoint *syncp)
{
    throw std::runtime_error("No MFXVideoVPP_RunFrameVPPAsyncEx in standalone app.");
}

#pragma warning(default: 4297)


void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage)
{
//    msdk_printf(MSDK_STRING("Intel(R) Media SDK Decoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("Intel(R) Media SDK HEVC to YUV decoder\n\n"));
    msdk_printf(MSDK_STRING("Usage: %s -i InputHEVCBitstream -o OutputYUVFile\n"), strAppName);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    } 

//    msdk_printf(MSDK_STRING("Usage: %s <codecid> [<options>] -i InputBitstream\n"), strAppName);
//    msdk_printf(MSDK_STRING("   or: %s <codecid> [<options>] -i InputBitstream -r\n"), strAppName);
//    msdk_printf(MSDK_STRING("   or: %s <codecid> [<options>] -i InputBitstream -o OutputYUVFile\n"), strAppName);
//    msdk_printf(MSDK_STRING("\n"));
//    msdk_printf(MSDK_STRING("Supported codecs (<codecid>):\n"));
//    msdk_printf(MSDK_STRING("   <codecid>=h264|mpeg2|vc1|mvc|jpeg - built-in Media SDK codecs\n"));
//    msdk_printf(MSDK_STRING("   <codecid>=h265                    - in-box Media SDK plugins (may require separate downloading and installation)\n"));
//#if defined(_WIN32) || defined(_WIN64)
//    msdk_printf(MSDK_STRING("   <codecid>=vp8                     - Media SDK sample user-decoder plugin (requires '-p' option to be functional)\n"));
//#endif
//    msdk_printf(MSDK_STRING("\n"));
//    msdk_printf(MSDK_STRING("Work models:\n"));
//    msdk_printf(MSDK_STRING("  1. Performance model: decoding on MAX speed, no rendering, no YUV dumping (no -r or -o option)\n"));
//    msdk_printf(MSDK_STRING("  2. Rendering model: decoding with rendering on the screen (-r option)\n"));
//    msdk_printf(MSDK_STRING("  3. Dump model: decoding with YUV dumping (-o option)\n"));
//    msdk_printf(MSDK_STRING("\n"));
//    msdk_printf(MSDK_STRING("Options:\n"));
//    msdk_printf(MSDK_STRING("   [-hw]                   - use platform specific SDK implementation, if not specified software implementation is used\n"));
//    msdk_printf(MSDK_STRING("   [-p /path/to/plugin]    - path to decoder plugin (optional for Media SDK in-box plugins, required for user-decoder ones)\n"));
//#if D3D_SURFACES_SUPPORT
//    msdk_printf(MSDK_STRING("   [-d3d]                  - work with d3d9 surfaces\n"));
//    msdk_printf(MSDK_STRING("   [-d3d11]                - work with d3d11 surfaces\n"));
//    msdk_printf(MSDK_STRING("   [-r]                    - render decoded data in a separate window \n"));
//    msdk_printf(MSDK_STRING("   [-wall w h n m f t tmo] - same as -r, and positioned rendering window in a particular cell on specific monitor \n"));
//    msdk_printf(MSDK_STRING("       w                   - number of columns of video windows on selected monitor\n"));
//    msdk_printf(MSDK_STRING("       h                   - number of rows of video windows on selected monitor\n"));
//    msdk_printf(MSDK_STRING("       n(0,.,w*h-1)        - order of video window in table that will be rendered\n"));
//    msdk_printf(MSDK_STRING("       m(0,1..)            - monitor id \n"));
//    msdk_printf(MSDK_STRING("       f                   - rendering framerate\n"));
//    msdk_printf(MSDK_STRING("       t(0/1)              - enable/disable window's title\n"));
//    msdk_printf(MSDK_STRING("       tmo                 - timeout for -wall option\n"));
//#endif
//#if defined(LIBVA_SUPPORT)
//    msdk_printf(MSDK_STRING("   [-vaapi]                - work with vaapi surfaces\n"));
//    msdk_printf(MSDK_STRING("   [-r]                    - render decoded data in a separate window \n"));
//#endif
//    msdk_printf(MSDK_STRING("   [-low_latency]          - configures decoder for low latency mode (supported only for H.264 and JPEG codec)\n"));
//    msdk_printf(MSDK_STRING("   [-calc_latency]         - calculates latency during decoding and prints log (supported only for H.264 and JPEG codec)\n"));
//#if defined(_WIN32) || defined(_WIN64)
//    msdk_printf(MSDK_STRING("   [-jpeg_rotate n]        - rotate jpeg frame n degrees \n"));
//    msdk_printf(MSDK_STRING("       n(90,180,270)       - number of degrees \n"));
//
//    msdk_printf(MSDK_STRING("\nFeatures: \n"));
//    msdk_printf(MSDK_STRING("   Press 1 to toggle fullscreen rendering on/off\n"));
//#endif
//    msdk_printf(MSDK_STRING("\n"));
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
    msdk_char* strArgument = MSDK_STRING("");

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    
    pParams->videoType = MFX_CODEC_HEVC;
    pParams->bUseHWLib = false;
    pParams->mode = MODE_FILE_DUMP;

    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        //if (MSDK_CHAR('-') != strInput[i][0])
        //{
        //    if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mpeg2")))
        //    {
        //        pParams->videoType = MFX_CODEC_MPEG2;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h264")))
        //    {
        //        pParams->videoType = MFX_CODEC_AVC;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("h265")))
        //    {
        //        pParams->videoType = MFX_CODEC_HEVC;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("vc1")))
        //    {
        //        pParams->videoType = MFX_CODEC_VC1;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("mvc")))
        //    {
        //        pParams->videoType = MFX_CODEC_AVC;
        //        pParams->bIsMVC = true;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("jpeg")))
        //    {
        //        pParams->videoType = MFX_CODEC_JPEG;
        //    }
        //    else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("vp8")))
        //    {
        //        pParams->videoType = CODEC_VP8;
        //    }
        //    else
        //    {
        //        PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
        //        return MFX_ERR_UNSUPPORTED;
        //    }
        //    continue;
        //}

//        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-hw")))
//        {
//            pParams->bUseHWLib = true;
//        }
//#if D3D_SURFACES_SUPPORT
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
//        {
//            pParams->memType = D3D9_MEMORY;
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")))
//        {
//            pParams->memType = D3D11_MEMORY;
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-r")))
//        {
//            pParams->mode = MODE_RENDERING;
//            // use d3d9 rendering by default
//            if (SYSTEM_MEMORY == pParams->memType)
//                pParams->memType = D3D9_MEMORY;
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-wall")))
//        {
//            if(i + 7 >= nArgNum)
//            {
//                PrintHelp(strInput[0], MSDK_STRING("Not enough parameters for -wall key"));
//                return MFX_ERR_UNSUPPORTED;
//            }
//            // use d3d9 rendering by default
//            if (SYSTEM_MEMORY == pParams->memType)
//                pParams->memType = D3D9_MEMORY;
//
//            pParams->mode = MODE_RENDERING;
//
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallW);
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallH);
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallCell);
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallMonitor);
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallFPS);
//            
//            int nTitle;
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &nTitle);
//
//            pParams->bWallNoTitle = 0 == nTitle;
//            
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nWallTimeout);
//        }
//#endif
//#if defined(LIBVA_SUPPORT)
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-vaapi")))
//        {
//            pParams->memType = D3D9_MEMORY;
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-r")))
//        {
//            pParams->memType = D3D9_MEMORY;
//            pParams->mode = MODE_RENDERING;
//        }
//#endif
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-low_latency")))
//        {            
//            switch (pParams->videoType)
//            {
//                case MFX_CODEC_HEVC:
//                case MFX_CODEC_AVC:
//                case MFX_CODEC_JPEG:
//                {
//                    pParams->bLowLat = true;
//                    if (!pParams->bIsMVC)
//                        break;
//                }
//                default:
//                {
//                     PrintHelp(strInput[0], MSDK_STRING("-low_latency mode is suppoted only for H.264 and JPEG codecs"));
//                     return MFX_ERR_UNSUPPORTED;
//                }
//            }
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-jpeg_rotate")))
//        {
//            if(MFX_CODEC_JPEG != pParams->videoType)
//                return MFX_ERR_UNSUPPORTED;
//
//            if(i + 1 >= nArgNum)
//            {
//                PrintHelp(strInput[0], MSDK_STRING("Not enough parameters for -jpeg_rotate key"));
//                return MFX_ERR_UNSUPPORTED;
//            }
//
//            msdk_sscanf(strInput[++i], MSDK_STRING("%d"), &pParams->nRotation);
//            if((pParams->nRotation != 90)&&(pParams->nRotation != 180)&&(pParams->nRotation != 270))
//            {
//                PrintHelp(strInput[0], MSDK_STRING("-jpeg_rotate is supported only for 90, 180 and 270 angles"));
//                return MFX_ERR_UNSUPPORTED;
//            }
//        }
//        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-calc_latency")))
//        {            
//            switch (pParams->videoType)
//            {
//                case MFX_CODEC_HEVC:
//                case MFX_CODEC_AVC:
//                case MFX_CODEC_JPEG:
//                {
//                    pParams->bCalLat = true;
//                    if (!pParams->bIsMVC)               
//                        break;
//                }
//                default:
//                {
//                     PrintHelp(strInput[0], MSDK_STRING("-calc_latency mode is suppoted only for H.264 and JPEG codecs"));
//                     return MFX_ERR_UNSUPPORTED;
//                }
//            }
//        }
//        else // 1-character options
        {
            switch (strInput[i][1])
            {
            //case MSDK_CHAR('p'):
            //    GET_OPTION_POINTER(strArgument);
            //    msdk_strcopy(pParams->strPluginPath, strArgument);
            //    break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->mode = MODE_FILE_DUMP;
                msdk_strcopy(pParams->strDstFile, strArgument);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                {
                    std::basic_stringstream<msdk_char> stream;
                    stream << MSDK_STRING("Unknown option: ") << strInput[i];
                    PrintHelp(strInput[0], stream.str().c_str());
                    return MFX_ERR_UNSUPPORTED;
                }
            }
        }
    }

    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        msdk_printf(MSDK_STRING("error: source file name was not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->mode == MODE_FILE_DUMP) && (0 == msdk_strlen(pParams->strDstFile)))
    {
        msdk_printf(MSDK_STRING("error: destination file name was not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    //if (MFX_CODEC_MPEG2 != pParams->videoType && 
    //    MFX_CODEC_AVC   != pParams->videoType && 
    //    MFX_CODEC_HEVC  != pParams->videoType &&
    //    MFX_CODEC_VC1   != pParams->videoType &&
    //    MFX_CODEC_JPEG  != pParams->videoType &&
    //    CODEC_VP8   != pParams->videoType)
    //{
    //    PrintHelp(strInput[0], MSDK_STRING("Unknown codec"));
    //    return MFX_ERR_UNSUPPORTED;
    //}

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, TCHAR *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams        Params;   // input parameters from command line
    CDecodingPipeline   Pipeline; // pipeline for decoding, includes input file reader, decoder and output file writer

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    //if (Params.bIsMVC)
    //    Pipeline.SetMultiView();

    sts = Pipeline.Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);   

    // print stream info 
    Pipeline.PrintInfo(); 

    msdk_printf(MSDK_STRING("Decoding started\n"));

    for (;;)
    {
        sts = Pipeline.RunDecoding();

        if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
            {
                //msdk_printf(MSDK_STRING("\nERROR: Incompatible video parameters detected. Recovering...\n"));
                msdk_printf(MSDK_STRING("\nERROR: Stream parameters changed\n"));
            }
            else
            {
                //msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned unexpected error. Recovering...\n"));
                sts = Pipeline.ResetDevice();
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            }           

            sts = Pipeline.ResetDecoder(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);            
            continue;
        }        
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }
    
    msdk_printf(MSDK_STRING("\nDecoding finished\n"));

    return 0;
}
