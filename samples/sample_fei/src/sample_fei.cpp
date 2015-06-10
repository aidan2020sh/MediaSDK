//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//

#include <memory>
#include "pipeline_fei.h"
#include "pipeline_user_fei.h"

mfxStatus CheckOptions(sInputParams* pParams);

void PrintHelp(msdk_char *strAppName, msdk_char *strErrorMessage)
{
    msdk_printf(MSDK_STRING("AVC FEI Encoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);

    if (strErrorMessage)
    {
        msdk_printf(MSDK_STRING("Error: %s\n"), strErrorMessage);
    }

    msdk_printf(MSDK_STRING("Usage: %s [<options>] -i InputYUVFile -o OutputEncodedFile -w width -h height\n"), strAppName);
    msdk_printf(MSDK_STRING("\n"));
    msdk_printf(MSDK_STRING("Only AVC is supported for FEI encoding.\n"));
    msdk_printf(MSDK_STRING("Options: \n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff] - input stream is interlaced, top|bottom field first, if not specified progressive is expected\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-b bitRate] - encoded bit rate (KBits per second), valid for H.264, H.265, MPEG2 and MVC encoders \n"));
    msdk_printf(MSDK_STRING("   [-u speed|quality|balanced] - target usage, valid for H.264, H.265, MPEG2 and MVC encoders\n"));
    msdk_printf(MSDK_STRING("   [-q quality] - quality parameter for JPEG encoder. In range [1,100]. 100 is the best quality. \n"));
    msdk_printf(MSDK_STRING("   [-n number] - number of frames to process\n"));
    msdk_printf(MSDK_STRING("   [-r distance] - Distance between I- or P- key frames (1 means no B-frames) (0 - by default(I frames))\n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (1(default) means I-frames only)\n"));
    msdk_printf(MSDK_STRING("   [-l numSlices] - number of slices \n"));
    msdk_printf(MSDK_STRING("   [-x numRefs]   - number of reference frames \n"));
    msdk_printf(MSDK_STRING("   [-qp qp_value] - QP value for frames\n"));
    msdk_printf(MSDK_STRING("   [-preenc] - use extended FEI interface PREENC (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("   [-encpak] - use extended FEI interface ENC+PAK (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("   [-enc_and_pak] - use extended FEI interface ENC only and PAK only (separate calls)\n"));
    msdk_printf(MSDK_STRING("   [-enc] - use extended FEI interface ENC (only)\n"));
    msdk_printf(MSDK_STRING("   [-pak] - use extended FEI interface PAK (only)\n"));
    msdk_printf(MSDK_STRING("   [-mbctrl file] - use the input to set MB control for FEI (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mbsize] - with this options size control fields will be used from MB control structure (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mvin file] - use this input to set MV predictor for FEI\n"));
    msdk_printf(MSDK_STRING("   [-mvout file] - use this input to set MV predictor for FEI\n"));
    msdk_printf(MSDK_STRING("   [-mbcode file] - file to output per MB information (structure mfxExtFeiPakMBCtrl) for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbstat file] - file to output per MB distortions for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbqp file] - file to input per MB QPs the same for each frame\n"));
    msdk_printf(MSDK_STRING("   [-pass_headers] - pass SPS, PPS and Slice headers to Media SDK instead of default one\n"));
    msdk_printf(MSDK_STRING("   [-8x8stat] - set 8x8 block for statistic report, default is 16x16 (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-search_window value] - specifies one of the predefined search path and window size. In range [1,8] (0 is default).\n"));
    msdk_printf(MSDK_STRING("   					     If non-zero value specified: -ref_window_w / _h, -len_sp, -max_len_sp are ignored\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_w width] - width of search region (should be multiple of 4), maximum allowed search window w*h is 2048 for\n"));
    msdk_printf(MSDK_STRING("   				 		one direction and 1048 for bidirectional search\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_h height] - height of search region (should be multiple of 4), maximum allowed is 32\n"));
    msdk_printf(MSDK_STRING("   [-len_sp length] - defines number of search units in search path. In range [1,63]\n"));
    msdk_printf(MSDK_STRING("   [-search_path value] - defines shape of search path. In range [1,2]\n"));
    msdk_printf(MSDK_STRING("   [-sub_mb_part_mask mask_hex] - specifies which partitions should be excluded from search. 0x00 - enable all (default is 0x77)\n"));

    // user module options
    msdk_printf(MSDK_STRING("\n"));
}

#define GET_OPTION_POINTER(PTR)         \
{                                       \
    if (2 == msdk_strlen(strInput[i]))      \
    {                                   \
        i++;                            \
        if (strInput[i][0] == MSDK_CHAR('-'))  \
        {                               \
            i = i - 1;                  \
        }                               \
        else                            \
        {                               \
            PTR = strInput[i];          \
        }                               \
    }                                   \
    else                                \
    {                                   \
        PTR = strInput[i] + 2;          \
    }                                   \
}

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, sInputParams* pParams)
{
    msdk_char* strArgument = MSDK_STRING("");
    msdk_char* stopCharacter;

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], NULL);
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

    msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING(PLUGIN_NAME));

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        // process multi-character options
        if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            i++;
            pParams->nDstWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            i++;
            pParams->nDstHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encpak")))
        {
            pParams->bENCPAK = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-enc_and_pak")))
        {
            pParams->bENCoPAKo = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-enc")))
        {
            pParams->bOnlyENC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pak")))
        {
            pParams->bOnlyPAK = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc")))
        {
            pParams->bPREENC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvin")))
        {
            pParams->mvinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvout")))
        {
            pParams->mvoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbcode")))
        {
            pParams->mbcodeoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbstat")))
        {
            pParams->mbstatoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbqp")))
        {
            //GET_OPTION_POINTER(strArgument);
            pParams->mbQpFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbsize")))
        {
            pParams->bMBSize = true;
        }

#if 0
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvout")))
        {
            //GET_OPTION_POINTER(strArgument);
            pParams->mvoutFile = strInput[i+1];
            i++;
        }
#endif
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbctrl")))
        {
            //GET_OPTION_POINTER(strArgument);
            pParams->mbctrinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pParams->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pParams->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-angle")))
        {
            i++;
            pParams->nRotationAngle = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qp")))
        {
            i++;
            pParams->QP = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-opencl")))
        {
#if defined(_WIN32) || defined(_WIN64)
            msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING("sample_plugin_opencl.dll"));
#else
            msdk_strcopy(pParams->strPluginDLLPath, MSDK_STRING("libsample_plugin_opencl.so"));
#endif
            pParams->nRotationAngle = 180;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-la")))
        {
            pParams->bLABRC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-lad")))
        {
            i++;
            pParams->nLADepth = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
#if D3D_SURFACES_SUPPORT
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d")))
        {
            pParams->memType = D3D9_MEMORY;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-d3d11")))
        {
            pParams->memType = D3D11_MEMORY;
        }
#endif
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pass_headers")))
        {
            //i++;
            pParams->bPassHeaders = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-8x8stat")))
        {
            pParams->Enable8x8Stat = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_window")))
        {
            i++;
            pParams->SearchWindow = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_w")))
        {
            i++;
            pParams->RefWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_h")))
        {
            i++;
            pParams->RefHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-len_sp")))
        {
            i++;
            pParams->LenSP = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_path")))
        {
            i++;
            pParams->SearchPath = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sub_mb_part_mask")))
        {
            i++;
            pParams->SubMBPartMask = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('u'):
                GET_OPTION_POINTER(strArgument);
                pParams->nTargetUsage = StrToTargetUsage(strArgument);
                break;
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pParams->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pParams->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('f'):
                GET_OPTION_POINTER(strArgument);
                pParams->dFrameRate = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('n'):
                GET_OPTION_POINTER(strArgument);
                pParams->nNumFrames = (mfxU32)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('g'):
                GET_OPTION_POINTER(strArgument);
                pParams->gopSize = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('r'):
                GET_OPTION_POINTER(strArgument);
                pParams->refDist = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('b'):
                GET_OPTION_POINTER(strArgument);
                pParams->nBitRate = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('l'):
                GET_OPTION_POINTER(strArgument);
                pParams->numSlices = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('x'):
                GET_OPTION_POINTER(strArgument);
                pParams->numRef = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                msdk_strcopy(pParams->strSrcFile, strArgument);
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pParams->dstFileBuff.push_back(strArgument);
                break;
            case MSDK_CHAR('q'):
                GET_OPTION_POINTER(strArgument);
                pParams->nQuality = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                PrintHelp(strInput[0], MSDK_STRING("Unknown options"));
            }
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pParams->strSrcFile))
    {
        PrintHelp(strInput[0], MSDK_STRING("Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if ((pParams->dstFileBuff.empty() ) &&
        (pParams->bENCoPAKo || pParams->bOnlyPAK) )
    {
        PrintHelp(strInput[0], MSDK_STRING("Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    };

    if (0 == pParams->nWidth || 0 == pParams->nHeight)
    {
        PrintHelp(strInput[0], MSDK_STRING("-w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    // check parameters validity
    if (pParams->nRotationAngle != 0 && pParams->nRotationAngle != 180)
    {
        PrintHelp(strInput[0], MSDK_STRING("Angles other than 180 degrees are not supported."));
        return MFX_ERR_UNSUPPORTED; // other than 180 are not supported
    }

    if (pParams->nQuality && (MFX_CODEC_JPEG != pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-q option is supported only for JPEG encoder"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->nTargetUsage || pParams->nBitRate) && (MFX_CODEC_JPEG == pParams->CodecId))
    {
        PrintHelp(strInput[0], MSDK_STRING("-u and -b options are supported only for H.264, MPEG2 and MVC encoders. For JPEG encoder use -q"));
        return MFX_ERR_UNSUPPORTED;
    }

    // set default values for optional parameters that were not set or were set incorrectly
    mfxU32 nviews = (mfxU32)pParams->srcFileBuff.size();
    if ((nviews <= 1) || (nviews > 2))
    {
        pParams->numViews = 1;
    }
    else
    {
        pParams->numViews = nviews;
    }

    if (MFX_TARGETUSAGE_BEST_QUALITY != pParams->nTargetUsage && MFX_TARGETUSAGE_BEST_SPEED != pParams->nTargetUsage)
    {
        pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
    }

    if (pParams->dFrameRate <= 0)
    {
        pParams->dFrameRate = 30;
    }

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pParams->nDstWidth == 0)
    {
        pParams->nDstWidth = pParams->nWidth;
    }

    if (pParams->nDstHeight == 0)
    {
        pParams->nDstHeight = pParams->nHeight;
    }

    // calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
    if (pParams->nBitRate == 0)
    {
        pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
            pParams->nDstHeight, pParams->dFrameRate);
    }

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pParams->ColorFormat)
    {
        pParams->ColorFormat = MFX_FOURCC_YV12;
    }

    if (!pParams->nPicStruct)
    {
        pParams->nPicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if ((pParams->bLABRC || pParams->nLADepth) && (!pParams->bUseHWLib))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with -hw option!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pParams->bLABRC || pParams->nLADepth) && (pParams->CodecId != MFX_CODEC_AVC))
    {
        PrintHelp(strInput[0], MSDK_STRING("Look ahead BRC is supported only with H.264 encoder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->nLADepth && (pParams->nLADepth < 10 || pParams->nLADepth > 100))
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -lad parameter, must be in range [10, 100]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SearchWindow < 0 || pParams->SearchWindow > 8)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -search_window parameter, must be in range [0, 8]!"));
        return MFX_ERR_UNSUPPORTED; // valid values 0-8
    }

    bool bEnableBiPred = (pParams->bENCPAK || pParams->bENCoPAKo || pParams->bOnlyENC) && (pParams->refDist > 1);

    if (pParams->RefHeight <= 0 || pParams->RefWidth <= 0 || pParams->RefHeight % 4 != 0 || pParams->RefWidth % 4 != 0 ||
    		pParams->RefHeight * pParams->RefWidth > (bEnableBiPred ? 1024 : 2048)){
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of search window size. -ref_window_h, -ref_window_w parameters, must be multiple of 4!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->LenSP < 1 || pParams->LenSP > 63)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -len_sp parameter, must be in range [1, 63]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SearchPath < 1 || pParams->SearchPath > 2)
    {
        PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -search_path parameter, must be in range [1, 2]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pParams->SubMBPartMask >= 0x7f ){
    	PrintHelp(strInput[0], MSDK_STRING("Unsupported value of -sub_mb_part_mask parameter, 0x7f disables all partitions!"));
    	return MFX_ERR_UNSUPPORTED;
    }

    // not all options are supported if rotate plugin is enabled
    if (pParams->nRotationAngle == 180 && (
        MFX_PICSTRUCT_PROGRESSIVE != pParams->nPicStruct ||
        pParams->nDstWidth != pParams->nWidth ||
        pParams->nDstHeight != pParams->nHeight ||
        (pParams->memType & D3D11_MEMORY) ||
        pParams->bLABRC ||
        pParams->nLADepth))
    {
        PrintHelp(strInput[0], MSDK_STRING("Some of the command line options are not supported with rotation plugin!"));
        return MFX_ERR_UNSUPPORTED;
    }
    /* One slice by default */
    if (0 == pParams->numSlices)
        pParams->numSlices = 1;

    /* One Ref by default */
    if (0 == pParams->numRef)
        pParams->numRef = 1;

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, msdk_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    sInputParams Params = {};   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    Params.CodecId = MFX_CODEC_AVC;    //only AVC is supported
    Params.nNumFrames = 0; //unlimited
    Params.refDist = 1; //only I frames
    Params.gopSize = 1; //only I frames
    Params.bENCPAK   = false; //default value
    Params.bENCoPAKo = false; //default value
    Params.bOnlyENC  = false; //default value
    Params.bOnlyPAK  = false; //default value
    Params.bPREENC   = false; //default value
    Params.Enable8x8Stat = false; //default value
    Params.mvinFile      = NULL;
    Params.mvoutFile     = NULL;
    Params.mbctrinFile   = NULL;
    Params.mbstatoutFile = NULL;
    Params.mbcodeoutFile = NULL;
    Params.mbQpFile      = NULL;
    Params.bMBSize = false;
    Params.memType = D3D9_MEMORY; //only HW memory is supported
    Params.QP = 26; //default qp value
    Params.bUseHWLib = true;
    Params.SearchWindow  = 0;
    Params.RefWidth      = 32;//48;
    Params.RefHeight     = 32;//40;
    Params.LenSP		 = 57;
    Params.SearchPath	 = 1;
    Params.SubMBPartMask = 0x77;

    sts = ParseInputString(argv, (mfxU8)argc, &Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = CheckOptions(&Params);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline.reset((Params.nRotationAngle) ? (CEncodingPipeline*)new CUserPipeline : new CEncodingPipeline);

    MSDK_CHECK_POINTER(pPipeline.get(), MFX_ERR_MEMORY_ALLOC);

    sts = pPipeline->Init(&Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            sts = pPipeline->ResetDevice();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

            sts = pPipeline->ResetMFXComponents(&Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);
            break;
        }
    }

    pPipeline->Close();
    msdk_printf(MSDK_STRING("\nProcessing finished\n"));

    return 0;
}

mfxStatus CheckOptions(sInputParams* pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
    if(pParams->bENCPAK && pParams->dstFileBuff.size() == 0){
        fprintf(stderr,"Output bitstream file should be specified for ENC+PAK (-o)\n");
        sts = MFX_ERR_UNSUPPORTED;
    }
    return sts;
}
