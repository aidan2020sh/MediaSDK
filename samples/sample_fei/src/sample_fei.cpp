/******************************************************************************\
Copyright (c) 2005-2016, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "pipeline_fei.h"

mfxStatus CheckOptions(AppConfig* pConfig);
mfxStatus CheckDRCParams(AppConfig* pConfig);

void PrintHelp(msdk_char *strAppName, const msdk_char *strErrorMessage)
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
    msdk_printf(MSDK_STRING("   [-i::h264|mpeg2|vc1] - set input encoded file and decoder type\n"));
    msdk_printf(MSDK_STRING("   [-nv12] - input is in NV12 color format, if not specified YUV420 is expected\n"));
    msdk_printf(MSDK_STRING("   [-tff|bff|mixed] - input stream is interlaced, top|bottom field first, if not specified progressive is expected.\n"));
    msdk_printf(MSDK_STRING("                    -mixed means that picture structre should be obtained from the input stream\n"));
    msdk_printf(MSDK_STRING("   [-single_field_processing] - single-field coding mode, one call for each field, tff/bff option required\n"));
    msdk_printf(MSDK_STRING("   [-bref] - arrange B frames in B pyramid reference structure\n"));
    msdk_printf(MSDK_STRING("   [-nobref] - do not use B-pyramid (by default the decision is made by library)\n"));
    msdk_printf(MSDK_STRING("   [-idr_interval size] - idr interval, default 0 means every I is an IDR, 1 means every other I frame is an IDR etc (default is infinite)\n"));
    msdk_printf(MSDK_STRING("   [-f frameRate] - video frame rate (frames per second)\n"));
    msdk_printf(MSDK_STRING("   [-n number] - number of frames to process\n"));
    msdk_printf(MSDK_STRING("   [-timeout seconds] - set time to run processing in seconds\n"));
    msdk_printf(MSDK_STRING("   [-r (-GopRefDist) distance] - Distance between I- or P- key frames (1 means no B-frames) (0 - by default(I frames))\n"));
    msdk_printf(MSDK_STRING("   [-g size] - GOP size (1(default) means I-frames only)\n"));
    msdk_printf(MSDK_STRING("   [-l numSlices] - number of slices \n"));
    msdk_printf(MSDK_STRING("   [-x (-NumRefFrame) numRefs] - number of reference frames \n"));
    msdk_printf(MSDK_STRING("   [-qp qp_value] - QP value for frames (default is 26)\n"));
    msdk_printf(MSDK_STRING("   [-num_active_P numRefs] - number of maximum allowed references for P frames (up to 4(default))\n"));
    msdk_printf(MSDK_STRING("   [-num_active_BL0 numRefs] - number of maximum allowed backward references for B frames (up to 4(default))\n"));
    msdk_printf(MSDK_STRING("   [-num_active_BL1 numRefs] - number of maximum allowed forward references for B frames (up to 2(default) for interlaced, 1(default) for progressive)\n"));
    msdk_printf(MSDK_STRING("   [-gop_opt closed|strict] - GOP optimization flags (can be used together)\n"));
    msdk_printf(MSDK_STRING("   [-trellis value] - bitfield: 0 = default, 1 = off, 2 = on for I frames, 4 = on for P frames, 8 = on for B frames (ENCODE only) (default is 0)\n"));
    msdk_printf(MSDK_STRING("   [-preenc ds_strength] - use extended FEI interface PREENC (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("                            if ds_strength parameter is missed or less than 2, PREENC is used on the full resolution\n"));
    msdk_printf(MSDK_STRING("                            otherwise PREENC is used on downscaled (by VPP resize in ds_strength times) surfaces\n"));
    msdk_printf(MSDK_STRING("   [-encode] - use extended FEI interface ENC+PAK (FEI ENCODE) (RC is forced to constant QP)\n"));
    msdk_printf(MSDK_STRING("   [-encpak] - use extended FEI interface ENC only and PAK only (separate calls)\n"));
    msdk_printf(MSDK_STRING("   [-enc] - use extended FEI interface ENC (only)\n"));
    msdk_printf(MSDK_STRING("   [-pak] - use extended FEI interface PAK (only)\n"));
    msdk_printf(MSDK_STRING("   [-reset_start] - set start frame No. of Dynamic Resolution change, please indicate the new resolution with -dstw -dsth\n"));
    msdk_printf(MSDK_STRING("   [-reset_end]   - specifies the end of current Dynamic Resolution Change related options\n"));
    msdk_printf(MSDK_STRING("   [-profile decimal] - set AVC profile (default is AVC high)\n"));
    msdk_printf(MSDK_STRING("   [-level decimal] - set AVC level (default is 41)\n"));
    msdk_printf(MSDK_STRING("   [-EncodedOrder] - use internal logic for reordering, reading from files (mvin, mbqp) will be in encoded order (default is display; ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-DecodedOrder] - output in decoded order (useful to dump streamout data in DecodedOrder). WARNING: all FEI interfaces expects frames to come in DisplayOrder.\n"));
    msdk_printf(MSDK_STRING("   [-mbctrl file] - use the input to set MB control for FEI (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mbsize] - with this options size control fields will be used from MB control structure (only ENC+PAK)\n"));
    msdk_printf(MSDK_STRING("   [-mvin file] - use this input to set MV predictor for FEI. PREENC and ENC (ENCODE) expect different structures\n"));
    msdk_printf(MSDK_STRING("   [-repack_preenc_mv] - use this in pair with -mvin to import preenc MVout directly\n"));
    msdk_printf(MSDK_STRING("   [-mvout file] - use this to output MV predictors\n"));
    msdk_printf(MSDK_STRING("   [-mbcode file] - file to output per MB information (structure mfxExtFeiPakMBCtrl) for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbstat file] - file to output per MB distortions for each frame\n"));
    msdk_printf(MSDK_STRING("   [-mbqp file] - file to input per MB QPs the same for each frame\n"));
    msdk_printf(MSDK_STRING("   [-repackctrl file] - file to input max encoded frame size,number of pass and delta qp for each frame(ENCODE only)\n "));
    msdk_printf(MSDK_STRING("   [-streamout file] - dump decode streamout structures\n"));
    msdk_printf(MSDK_STRING("   [-sys] - use system memory for surfaces (ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-8x8stat] - set 8x8 block for statistic report, default is 16x16 (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-search_window value] - specifies one of the predefined search path and window size. In range [1,8] (5 is default).\n"));
    msdk_printf(MSDK_STRING("                            If non-zero value specified: -ref_window_w / _h, -len_sp are ignored\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_w width] - width of search region (should be multiple of 4), maximum allowed search window w*h is 2048 for\n"));
    msdk_printf(MSDK_STRING("                            one direction and 1024 for bidirectional search\n"));
    msdk_printf(MSDK_STRING("   [-ref_window_h height] - height of search region (should be multiple of 4), maximum allowed is 32\n"));
    msdk_printf(MSDK_STRING("   [-len_sp length] - defines number of search units in search path. In range [1,63] (default is 57)\n"));
    msdk_printf(MSDK_STRING("   [-search_path value] - defines shape of search path. 0 -full, 1- diamond.\n"));
    msdk_printf(MSDK_STRING("   [-sub_mb_part_mask mask_hex] - specifies which partitions should be excluded from search (default is 0x00 - enable all)\n"));
    msdk_printf(MSDK_STRING("   [-sub_pel_mode mode_hex] - specifies sub pixel precision for motion estimation 0x00-0x01-0x03 integer-half-quarter (default is 0x03)\n"));
    msdk_printf(MSDK_STRING("   [-intra_part_mask mask_hex] - specifies what blocks and sub-blocks partitions are enabled for intra prediction (default is 0)\n"));
    msdk_printf(MSDK_STRING("   [-intra_SAD] - specifies intra distortion adjustment. 0x00 - none, 0x02 - Haar transform (default)\n"));
    msdk_printf(MSDK_STRING("   [-inter_SAD] - specifies inter distortion adjustment. 0x00 - none, 0x02 - Haar transform (default)\n"));
    msdk_printf(MSDK_STRING("   [-adaptive_search] - enables adaptive search\n"));
    msdk_printf(MSDK_STRING("   [-forward_transform] - enables forward transform. Additional stat is calculated and reported, QP required (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-repartition_check] - enables additional sub pixel and bidirectional refinements (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-multi_pred_l0] - MVs from neighbor MBs will be used as predictors for L0 prediction list (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-multi_pred_l1] - MVs from neighbor MBs will be used as predictors for L1 prediction list (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-adjust_distortion] - if enabled adds a cost adjustment to distortion, default is RAW distortion (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-n_mvpredictors_l0 num] - number of MV predictors for l0 list, up to 4 is supported (default is 1) (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-n_mvpredictors_l1 num] - number of MV predictors for l1 list, up to 4 is supported (default is 0) (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("   [-preenc_mvpredictors_l0 bit] - enable/disable l0 predictor (default is to use if l0 reference exists) (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-preenc_mvpredictors_l1 bit] - enable/disable l1 predictor (default is to use if l1 reference exists) (PREENC only)\n"));
    msdk_printf(MSDK_STRING("   [-colocated_mb_distortion] - provides the distortion between the current and the co-located MB. It has performance impact (ENC, ENCODE)\n"));
    msdk_printf(MSDK_STRING("                                do not use it, unless it is necessary\n"));
    msdk_printf(MSDK_STRING("   [-dblk_idc value] - value of DisableDeblockingIdc (default is 0), in range [0,2]\n"));
    msdk_printf(MSDK_STRING("   [-dblk_alpha value] - value of SliceAlphaC0OffsetDiv2 (default is 0), in range [-6,6]\n"));
    msdk_printf(MSDK_STRING("   [-dblk_beta value] - value of SliceBetaOffsetDiv2 (default is 0), in range [-6,6]\n"));
    msdk_printf(MSDK_STRING("   [-chroma_qpi_offset first_offset] - first offset used for chroma qp in range [-12, 12] (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-s_chroma_qpi_offset second_offset] - second offset used for chroma qp in range [-12, 12] (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-constrained_intra_pred_flag] - use constrained intra prediction (default is off, used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-transform_8x8_mode_flag] - enables 8x8 transform, by default only 4x4 is used (used in PPS, pass_headers should be set)\n"));
    msdk_printf(MSDK_STRING("   [-dstw width]  - destination picture width, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-dsth height] - destination picture height, invokes VPP resizing\n"));
    msdk_printf(MSDK_STRING("   [-perf] - switch on performance mode (disabled file operations, simplified predictors repacking)\n"));
    msdk_printf(MSDK_STRING("   [-rawref] - use raw frames for reference instead of reconstructed frames (ENCODE only)\n"));
    msdk_printf(MSDK_STRING("   [-n_surf_input n] - specify number of surfaces that would be allocated for input frames\n"));
    msdk_printf(MSDK_STRING("   [-n_surf_recon n] - specify number of surfaces that would be allocated for reconstruct frames (ENC or/and PAK)\n"));

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

mfxStatus ParseInputString(msdk_char* strInput[], mfxU8 nArgNum, AppConfig* pConfig)
{
    const msdk_char* strArgument = MSDK_STRING("");
    msdk_char* stopCharacter;

    bool bRefWSizeSpecified = false, bAlrShownHelp = false, bBRefSet = false, bParseDRC = false;

    if (1 == nArgNum)
    {
        PrintHelp(strInput[0], MSDK_STRING("ERROR: Not enough input parameters"));
        return MFX_ERR_UNSUPPORTED;
    }

    MSDK_CHECK_POINTER(pConfig, MFX_ERR_NULL_PTR);

    // parse command line parameters
    for (mfxU8 i = 1; i < nArgNum; i++)
    {
        MSDK_CHECK_POINTER(strInput[i], MFX_ERR_NULL_PTR);

        // process multi-character options
        if (0 == msdk_strncmp(MSDK_STRING("-i::"), strInput[i], msdk_strlen(MSDK_STRING("-i::"))))
        {
            pConfig->bDECODE = true;

            mfxStatus sts = StrFormatToCodecFormatFourCC(strInput[i] + 4, pConfig->DecodeId);
            if (sts != MFX_ERR_NONE)
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Failed to extract decodeId from input stream"));
                return MFX_ERR_UNSUPPORTED;
            }
            i++;

            if (msdk_strlen(strInput[i]) < sizeof(pConfig->strSrcFile)){
                msdk_opt_read(strInput[i], pConfig->strSrcFile);
            }
            else{
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Too long input filename (limit is 1023 characters)!"));
                return MFX_ERR_UNSUPPORTED;
            }

            switch (pConfig->DecodeId)
            {
            case MFX_CODEC_MPEG2:
            case MFX_CODEC_AVC:
            case MFX_CODEC_VC1:
                break;
            default:
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported encoded input (only AVC, MPEG2, VC1 is supported)"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encode")))
        {
            pConfig->bENCODE = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-encpak")))
        {
            pConfig->bENCPAK = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-enc")))
        {
            pConfig->bOnlyENC = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-pak")))
        {
            pConfig->bOnlyPAK = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc")))
        {
            pConfig->bPREENC = true;

            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], pConfig->preencDSstrength))
            {
                pConfig->preencDSstrength = 0;
                i--;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-profile")))
        {
            i++;
            pConfig->CodecProfile = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-level")))
        {
            i++;
            pConfig->CodecLevel = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-EncodedOrder")))
        {
            pConfig->EncodedOrder = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-DecodedOrder")))
        {
            pConfig->DecodedOrder = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvin")))
        {
            pConfig->mvinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-repack_preenc_mv")))
        {
            pConfig->bRepackPreencMV = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mvout")))
        {
            pConfig->mvoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbcode")))
        {
            pConfig->mbcodeoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbstat")))
        {
            pConfig->mbstatoutFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbqp")))
        {
            pConfig->mbQpFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-repackctrl")))
        {
            pConfig->repackctrlFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbsize")))
        {
            pConfig->bMBSize = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mbctrl")))
        {
            pConfig->mbctrinFile = strInput[i+1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-streamout")))
        {
            pConfig->bDECODESTREAMOUT = true;
            pConfig->decodestreamoutFile = strInput[i + 1];
            i++;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nv12")))
        {
            pConfig->ColorFormat = MFX_FOURCC_NV12;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-tff")))
        {
            pConfig->nPicStruct = MFX_PICSTRUCT_FIELD_TFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bff")))
        {
            pConfig->nPicStruct = MFX_PICSTRUCT_FIELD_BFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-mixed")))
        {
            pConfig->nPicStruct = MFX_PICSTRUCT_UNKNOWN;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-single_field_processing")))
        {
            pConfig->bFieldProcessingMode = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-bref")))
        {
            bBRefSet = true;
            pConfig->bRefType = MFX_B_REF_PYRAMID;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-nobref")))
        {
            bBRefSet = true;
            pConfig->bRefType = MFX_B_REF_OFF;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-idr_interval")))
        {
            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], pConfig->nIdrInterval))
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: IdrInterval is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-qp")))
        {
            i++;
            pConfig->QP = (mfxU8)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-num_active_P")))
        {
            i++;
            pConfig->NumRefActiveP = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-num_active_BL0")))
        {
            i++;
            pConfig->NumRefActiveBL0 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-num_active_BL1")))
        {
            i++;
            pConfig->NumRefActiveBL1 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-gop_opt")))
        {
            i++;
            if (0 == msdk_strcmp(strInput[i], MSDK_STRING("closed")))
            {
                pConfig->GopOptFlag |= MFX_GOP_CLOSED;
            }
            else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("strict")))
            {
                pConfig->GopOptFlag |= MFX_GOP_STRICT;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-trellis")))
        {
            i++;
            pConfig->Trellis = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sys")))
        {
            pConfig->bUseHWmemory = false;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-8x8stat")))
        {
            pConfig->Enable8x8Stat = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-forward_transform")))
        {
            pConfig->FTEnable = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-adaptive_search")))
        {
            pConfig->AdaptiveSearch = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-repartition_check")))
        {
            pConfig->RepartitionCheckEnable = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-multi_pred_l0")))
        {
            pConfig->MultiPredL0 = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-multi_pred_l1")))
        {
            pConfig->MultiPredL1 = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-adjust_distortion")))
        {
            pConfig->DistortionType = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-colocated_mb_distortion")))
        {
            pConfig->ColocatedMbDistortion = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_window")))
        {
            i++;
            pConfig->SearchWindow = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_w")))
        {
            i++;
            pConfig->RefWidth = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bRefWSizeSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-ref_window_h")))
        {
            i++;
            pConfig->RefHeight = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
            bRefWSizeSpecified = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-len_sp")))
        {
            i++;
            pConfig->LenSP = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-search_path")))
        {
            i++;
            pConfig->SearchPath = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sub_mb_part_mask")))
        {
            i++;
            pConfig->SubMBPartMask = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-sub_pel_mode")))
        {
            i++;
            pConfig->SubPelMode = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-intra_part_mask")))
        {
            i++;
            pConfig->IntraPartMask = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-intra_SAD")))
        {
            i++;
            pConfig->IntraSAD = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-inter_SAD")))
        {
            i++;
            pConfig->InterSAD = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 16);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n_mvpredictors_l0")))
        {
            i++;
            pConfig->bNPredSpecified_l0 = true;
            pConfig->NumMVPredictors[0] = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n_mvpredictors_l1")))
        {
            i++;
            pConfig->bNPredSpecified_l1 = true;
            pConfig->NumMVPredictors[1] = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc_mvpredictors_l0")))
        {
            i++;
            pConfig->bPreencPredSpecified_l0 = true;
            pConfig->PreencMVPredictors[0] = !!msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-preenc_mvpredictors_l1")))
        {
            i++;
            pConfig->bPreencPredSpecified_l1 = true;
            pConfig->PreencMVPredictors[1] = !!msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_idc")))
        {
            i++;
            pConfig->DisableDeblockingIdc = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_alpha")))
        {
            i++;
            pConfig->SliceAlphaC0OffsetDiv2 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dblk_beta")))
        {
            i++;
            pConfig->SliceBetaOffsetDiv2 = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-chroma_qpi_offset")))
        {
            i++;
            pConfig->ChromaQPIndexOffset = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-s_chroma_qpi_offset")))
        {
            i++;
            pConfig->SecondChromaQPIndexOffset = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-constrained_intra_pred_flag")))
        {
            pConfig->ConstrainedIntraPredFlag = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-transform_8x8_mode_flag")))
        {
            pConfig->Transform8x8ModeFlag = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dstw")))
        {
            mfxU16 wdt;
            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], wdt))
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Destination picture Width is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }

            if (pConfig->bDynamicRC && bParseDRC)
            {
                pConfig->DRCqueue.back().target_w = wdt;
            }
            else
            {
                pConfig->nDstWidth = wdt;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-dsth")))
        {
            mfxU16 hgt;
            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], hgt))
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Destination picture Width is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }

            if (pConfig->bDynamicRC && bParseDRC)
            {
                pConfig->DRCqueue.back().target_h = hgt;
            }
            else
            {
                pConfig->nDstHeight = hgt;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-timeout")))
        {
            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], pConfig->nTimeout))
            {
                PrintHelp(strInput[0], MSDK_STRING("Timeout is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-perf")))
        {
            pConfig->bPerfMode = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-reset_start")))
        {
            if (bParseDRC)
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: -reset_end of previous DRC block didn't found"));
                return MFX_ERR_UNSUPPORTED;
            }

            bParseDRC = true;
            mfxU32 resetFrame;

            if (!strInput[++i] || MFX_ERR_NONE != msdk_opt_read(strInput[i], resetFrame))
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Reset start frame is invalid"));
                return MFX_ERR_UNSUPPORTED;
            }

            pConfig->bDynamicRC = true;
            pConfig->DRCqueue.push_back(DRCblock(resetFrame));
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-reset_end")))
        {
            if (!(pConfig->bDynamicRC || bParseDRC))
            {
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Please Set -reset_start"));
                return MFX_ERR_UNSUPPORTED;
            }
            bParseDRC = false;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-rawref")))
        {
            pConfig->bRawRef = true;
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-GopRefDist")))
        {
            i++;
            pConfig->refDist = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-NumRefFrame")))
        {
            i++;
            pConfig->numRef = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n_surf_input")))
        {
            i++;
            pConfig->nInputSurf = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else if (0 == msdk_strcmp(strInput[i], MSDK_STRING("-n_surf_recon")))
        {
            i++;
            pConfig->nReconSurf = (mfxU16)msdk_strtol(strInput[i], &stopCharacter, 10);
        }
        else // 1-character options
        {
            switch (strInput[i][1])
            {
            case MSDK_CHAR('w'):
                GET_OPTION_POINTER(strArgument);
                pConfig->nWidth = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('h'):
                GET_OPTION_POINTER(strArgument);
                pConfig->nHeight = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('f'):
                GET_OPTION_POINTER(strArgument);
                pConfig->dFrameRate = (mfxF64)msdk_strtod(strArgument, &stopCharacter);
                break;
            case MSDK_CHAR('n'):
                GET_OPTION_POINTER(strArgument);
                pConfig->nNumFrames = (mfxU32)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('g'):
                GET_OPTION_POINTER(strArgument);
                pConfig->gopSize = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('r'):
                GET_OPTION_POINTER(strArgument);
                pConfig->refDist = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('l'):
                GET_OPTION_POINTER(strArgument);
                pConfig->numSlices = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('x'):
                GET_OPTION_POINTER(strArgument);
                pConfig->numRef = (mfxU16)msdk_strtol(strArgument, &stopCharacter, 10);
                break;
            case MSDK_CHAR('i'):
                GET_OPTION_POINTER(strArgument);
                if (msdk_strlen(strArgument) < sizeof(pConfig->strSrcFile)){
                    msdk_strcopy(pConfig->strSrcFile, strArgument);
                }else{
                    PrintHelp(strInput[0], MSDK_STRING("ERROR: Too long input filename (limit is 1023 characters)!"));
                    return MFX_ERR_UNSUPPORTED;
                }
                break;
            case MSDK_CHAR('o'):
                GET_OPTION_POINTER(strArgument);
                pConfig->dstFileBuff.push_back(strArgument);
                break;
            case MSDK_CHAR('?'):
                PrintHelp(strInput[0], NULL);
                return MFX_ERR_UNSUPPORTED;
            default:
                if (!bAlrShownHelp){
                    msdk_printf(MSDK_STRING("\nWARNING: Unknown option %s\n\n"), strInput[i]);
                    PrintHelp(strInput[0], NULL);
                    bAlrShownHelp = true;
                }else {
                    msdk_printf(MSDK_STRING("\nWARNING: Unknown option %s\n\n"), strInput[i]);
                }
            }
        }
    }

    bool allowedFEIpipeline = ( pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              (!pConfig->bPREENC &&  pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              (!pConfig->bPREENC && !pConfig->bOnlyENC &&  pConfig->bOnlyPAK && !pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              (!pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  &&  pConfig->bENCODE) ||
                              (!pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK &&  pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              //(pConfig->bPREENC &&  pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              ( pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  &&  pConfig->bENCODE) ||
                              ( pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK &&  pConfig->bENCPAK  && !pConfig->bENCODE) ||
                              (!pConfig->bPREENC && !pConfig->bOnlyENC && !pConfig->bOnlyPAK && !pConfig->bENCPAK  && !pConfig->bENCODE && pConfig->bDECODESTREAMOUT);

    if (!allowedFEIpipeline)
    {
        if (bAlrShownHelp){
            msdk_printf(MSDK_STRING("\nERROR: Unsupported pipeline!\n"));

            msdk_printf(MSDK_STRING("Supported pipelines are:\n"));
            msdk_printf(MSDK_STRING("PREENC\n"));
            msdk_printf(MSDK_STRING("ENC\n"));
            msdk_printf(MSDK_STRING("PAK\n"));
            msdk_printf(MSDK_STRING("ENCODE\n"));
            msdk_printf(MSDK_STRING("ENC + PAK (ENCPAK)\n"));
            //msdk_printf(MSDK_STRING("PREENC + ENC\n"));
            msdk_printf(MSDK_STRING("PREENC + ENCODE\n"));
            msdk_printf(MSDK_STRING("PREENC + ENC + PAK (PREENC + ENCPAK)\n"));
        } else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported pipeline!\nSupported pipelines are:\nPREENC\nENC\nPAK\nENCODE\nENC + PAK (ENCPAK)\nPREENC + ENC\nPREENC + ENCODE\nPREENC + ENC + PAK (PREENC + ENCPAK)\n"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((!pConfig->bDECODE || pConfig->DecodeId != MFX_CODEC_AVC) && pConfig->bDECODESTREAMOUT)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("ERROR: Decode streamout requires AVC encoded stream at input"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Decode streamout requires AVC encoded stream at input\n"));

        return MFX_ERR_UNSUPPORTED;
    }

    if (!!pConfig->preencDSstrength)
    {
        switch (pConfig->preencDSstrength)
        {
        case 1:
            pConfig->preencDSstrength = 0;
            break;
        case 2:
        case 4:
        case 8:
            break;
        default:
            if (bAlrShownHelp)
                msdk_printf(MSDK_STRING("\nERROR: Unsupported strength of PREENC downscaling!\n"));
            else
                PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported strength of PREENC downscaling!"));
            return MFX_ERR_UNSUPPORTED;
        }
    }

    // check if all mandatory parameters were set
    if (0 == msdk_strlen(pConfig->strSrcFile))
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Source file name not found\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Source file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pConfig->dstFileBuff.empty() ) &&
        (pConfig->bENCPAK || pConfig->bOnlyPAK) )
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Destination file name not found\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Destination file name not found"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((0 == pConfig->nWidth || 0 == pConfig->nHeight) && !pConfig->bDECODE)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: -w, -h must be specified\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: -w, -h must be specified"));
        return MFX_ERR_UNSUPPORTED;
    }

    if ((pConfig->nWidth || pConfig->nHeight) && pConfig->bDECODE)
    {
        msdk_printf(MSDK_STRING("\nWARNING: input width and height will be taken from input stream (-w -h settings ignored)\n"));
    }

    if (pConfig->dFrameRate <= 0)
    {
        pConfig->dFrameRate = 30;
    }

    // if no destination picture width or height wasn't specified set it to the source picture size
    if (pConfig->nDstWidth == 0)
    {
        pConfig->nDstWidth = pConfig->nWidth;
    }

    if (pConfig->nDstHeight == 0)
    {
        pConfig->nDstHeight = pConfig->nHeight;
    }

    // set VPP flag after determination of all parameters
    pConfig->bVPP = pConfig->nWidth != pConfig->nDstWidth || pConfig->nHeight != pConfig->nDstHeight || pConfig->bDynamicRC;

    // if nv12 option wasn't specified we expect input YUV file in YUV420 color format
    if (!pConfig->ColorFormat)
    {
        pConfig->ColorFormat = MFX_FOURCC_YV12;
    }

    // Check references lists limits
    if (pConfig->NumRefActiveP == 0)
    {
        pConfig->NumRefActiveP = MaxNumActiveRefP;
    }
    else if (pConfig->NumRefActiveP > MaxNumActiveRefP)
    {
        pConfig->NumRefActiveP = MaxNumActiveRefP;
        msdk_printf(MSDK_STRING("\nWARNING: Unsupported number of P frame references, adjusted to maximum (%d)\n"), MaxNumActiveRefP);
    }

    if (pConfig->NumRefActiveBL0 == 0)
    {
        pConfig->NumRefActiveBL0 = MaxNumActiveRefBL0;
    }
    else if (pConfig->NumRefActiveBL0 > MaxNumActiveRefBL0)
    {
        pConfig->NumRefActiveBL0 = MaxNumActiveRefBL0;
        msdk_printf(MSDK_STRING("\nWARNING: Unsupported number of B frame backward references, adjusted to maximum (%d)\n"), MaxNumActiveRefBL0);
    }

    if (pConfig->NumRefActiveBL1 == 0)
    {
        pConfig->NumRefActiveBL1 = (pConfig->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i;
    }
    else if (pConfig->NumRefActiveBL1 > ((pConfig->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i))
    {
        pConfig->NumRefActiveBL1 = (pConfig->nPicStruct == MFX_PICSTRUCT_PROGRESSIVE) ? MaxNumActiveRefBL1 : MaxNumActiveRefBL1_i;
        msdk_printf(MSDK_STRING("\nWARNING: Unsupported number of B frame forward references, adjusted to maximum (%d (%d for interlaced))\n"), MaxNumActiveRefBL1, MaxNumActiveRefBL1_i);
    }

    if (pConfig->nPicStruct == MFX_PICSTRUCT_UNKNOWN
        && (!pConfig->bDECODE || pConfig->bENCPAK || pConfig->bOnlyPAK || pConfig->bOnlyENC || pConfig->bDynamicRC))
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Sample does not support this combination of options in mixed picstructs mode!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Sample does not support this combination of options in mixed picstructs mode!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SearchWindow > 8)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -search_window parameter, must be in range [0, 8]!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -search_window parameter, must be in range [0, 8]!"));
        return MFX_ERR_UNSUPPORTED; // valid values 0-8
    }

    if (bRefWSizeSpecified && pConfig->SearchWindow != 0){
        msdk_printf(MSDK_STRING("\nWARNING: -search_window specified, -ref_window_w and -ref_window_h will be ignored.\n"));
        switch (pConfig->SearchWindow){
        case 1: msdk_printf(MSDK_STRING("Search window size is 24x24 (4 SUs); Path - Tiny\n\n"));
            break;
        case 2: msdk_printf(MSDK_STRING("Search window size is 28x28 (9 SUs); Path - Small\n\n"));
            break;
        case 3: msdk_printf(MSDK_STRING("Search window size is 48x40 (16 SUs); Path - Diamond\n\n"));
            break;
        case 4: msdk_printf(MSDK_STRING("Search window size is 48x40 (32 SUs); Path - Large Diamond\n\n"));
            break;
        case 5: msdk_printf(MSDK_STRING("Search window size is 48x40 (48 SUs); Path - Exhaustive\n\n"));
            break;
        case 6: msdk_printf(MSDK_STRING("Search window size is 64x32 (16 SUs); Path - Horizontal Diamond\n\n"));
            break;
        case 7: msdk_printf(MSDK_STRING("Search window size is 64x32 (32 SUs); Path - Horizontal Large Diamond\n\n"));
            break;
        case 8: msdk_printf(MSDK_STRING("Search window size is 64x32 (48 SUs); Path - Horizontal Exhaustive\n\n"));
            break;
        default:
            break;
        }
    }

    if ((pConfig->SearchWindow != 0) && (pConfig->RefHeight == 0 || pConfig->RefWidth == 0 || pConfig->RefHeight % 4 != 0 || pConfig->RefWidth % 4 != 0 ||
         pConfig->RefHeight*pConfig->RefWidth > 2048))
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of search window size. -ref_window_h, -ref_window_w parameters, must be multiple of 4!\n"
                                    "For bi-prediction window w*h must less than 1024 and less 2048 otherwise.\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of search window size. -ref_window_h, -ref_window_w parameters, must be multiple of 4!\n"
                                               "For bi-prediction window w*h must less than 1024 and less 2048 otherwise."));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->LenSP < 1 || pConfig->LenSP > 63)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -len_sp parameter, must be in range [1, 63]!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -len_sp parameter, must be in range [1, 63]!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SearchPath > 1)
    {
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -search_path parameter, must be 0 or 1!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -search_path parameter, must be be 0 or 1!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SubMBPartMask >= 0x7f ){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -sub_mb_part_mask parameter, 0x7f disables all partitions!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -sub_mb_part_mask parameter, 0x7f disables all partitions!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SubPelMode != 0x00 && pConfig->SubPelMode != 0x01 && pConfig->SubPelMode != 0x03){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -sub_pel_mode parameter, must be 0x00/0x01/0x03!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -sub_pel_mode parameter, must be 0x00/0x01/0x03!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->IntraPartMask >= 0x07){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -inra_part_mask parameter, 0x07 disables all partitions!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -inra_part_mask parameter, 0x07 disables all partitions!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->IntraSAD != 0x02 && pConfig->IntraSAD != 0x00){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -intra_SAD, must be 0x00 or 0x02!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -intra_SAD, must be 0x00 or 0x02!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->InterSAD != 0x02 && pConfig->InterSAD != 0x00){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value of -inter_SAD, must be 0x00 or 0x02!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value of -inter_SAD, must be 0x00 or 0x02!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->NumMVPredictors[0] > MaxFeiEncMVPNum || pConfig->NumMVPredictors[1] > MaxFeiEncMVPNum){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported value number of MV predictors (4 is maximum)!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported value number of MV predictors (4 is maximum)!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->Trellis > (MFX_TRELLIS_I | MFX_TRELLIS_P | MFX_TRELLIS_B)){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported trellis value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported trellis value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->DisableDeblockingIdc > 2){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported DisableDeblockingIdc value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported DisableDeblockingIdc value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SliceAlphaC0OffsetDiv2 > 6 || pConfig->SliceAlphaC0OffsetDiv2 < -6){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported SliceAlphaC0OffsetDiv2 value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported SliceAlphaC0OffsetDiv2 value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SliceBetaOffsetDiv2 > 6 || pConfig->SliceBetaOffsetDiv2 < -6){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported SliceBetaOffsetDiv2 value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported SliceBetaOffsetDiv2 value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->DecodedOrder && pConfig->bDECODE && (pConfig->bPREENC || pConfig->bENCPAK || pConfig->bOnlyENC || pConfig->bOnlyPAK || pConfig->bENCODE)){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: DecodedOrder turned on, all FEI interfaces in sample_fei expects frame in DisplayOrder!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: DecodedOrder turned on, all FEI interfaces in sample_fei expects frame in DisplayOrder!"));
        return MFX_ERR_UNSUPPORTED;
    }

    /* temporary adjustment */
    if ((pConfig->bENCPAK || pConfig->bOnlyENC || pConfig->bOnlyPAK) && pConfig->bRefType != MFX_B_REF_OFF)
    {
        if (!bBRefSet)   pConfig->bRefType = MFX_B_REF_OFF;
    }

    if (pConfig->ChromaQPIndexOffset > 12 || pConfig->ChromaQPIndexOffset < -12){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported ChromaQPIndexOffset value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported ChromaQPIndexOffset value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->SecondChromaQPIndexOffset > 12 || pConfig->SecondChromaQPIndexOffset < -12){
        if (bAlrShownHelp)
            msdk_printf(MSDK_STRING("\nERROR: Unsupported SecondChromaQPIndexOffset value!\n"));
        else
            PrintHelp(strInput[0], MSDK_STRING("ERROR: Unsupported SecondChromaQPIndexOffset value!"));
        return MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->EncodedOrder && pConfig->bENCODE && pConfig->nNumFrames == 0){
        msdk_printf(MSDK_STRING("\nWARNING: without number of frames setting (-n) last frame of FEI ENCODE in Encoded Order\n"));
        msdk_printf(MSDK_STRING("           could be non-bitexact with last frame in FEI ENCODE with Display Order!\n"));
    }

    if (pConfig->bPerfMode && (pConfig->mvinFile || pConfig->mvoutFile || pConfig->mbctrinFile || pConfig->mbstatoutFile || pConfig->mbcodeoutFile || pConfig->mbQpFile || pConfig->repackctrlFile || pConfig->decodestreamoutFile))
    {
        msdk_printf(MSDK_STRING("\nWARNING: All file operations would be ignored in performance mode!\n"));

        pConfig->mvinFile            = NULL;
        pConfig->mvoutFile           = NULL;
        pConfig->mbctrinFile         = NULL;
        pConfig->mbstatoutFile       = NULL;
        pConfig->mbcodeoutFile       = NULL;
        pConfig->mbQpFile            = NULL;
        pConfig->repackctrlFile      = NULL;
        pConfig->decodestreamoutFile = NULL;
    }

    if (pConfig->bENCODE || pConfig->bENCPAK || pConfig->bOnlyENC || pConfig->bOnlyPAK){
        if (!pConfig->CodecProfile)
            pConfig->CodecProfile = 100; // MFX_PROFILE_AVC_HIGH

        if (!pConfig->CodecLevel)
            pConfig->CodecLevel = 41;    // MFX_LEVEL_AVC_41
    }

    /* One slice by default */
    if (0 == pConfig->numSlices)
        pConfig->numSlices = 1;

    /* One Ref by default */
    if (0 == pConfig->numRef)
        pConfig->numRef = 1;

    if (pConfig->nPicStruct != MFX_PICSTRUCT_PROGRESSIVE && pConfig->numRef == 1){
        msdk_printf(MSDK_STRING("\nWARNING: minimal number of references on interlaced content is 2!\n"));
        msdk_printf(MSDK_STRING("           Current number of references extended.\n"));

        pConfig->numRef = 2;
    }

    return MFX_ERR_NONE;
}

#if defined(_WIN32) || defined(_WIN64)
int _tmain(int argc, msdk_char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
    AppConfig Config;   // input parameters from command line
    std::auto_ptr<CEncodingPipeline>  pPipeline;

    mfxStatus sts = MFX_ERR_NONE; // return value check

    sts = ParseInputString(argv, (mfxU8)argc, &Config);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = CheckOptions(&Config);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    sts = CheckDRCParams(&Config);
    MSDK_CHECK_PARSE_RESULT(sts, MFX_ERR_NONE, 1);

    pPipeline.reset(new CEncodingPipeline(&Config));

    sts = pPipeline->Init();
    MSDK_CHECK_STATUS(sts, "pPipeline->Init failed");

    pPipeline->PrintInfo();

    msdk_printf(MSDK_STRING("Processing started\n"));

    msdk_tick startTime;
    msdk_tick frequency = msdk_time_get_frequency();
    startTime = msdk_time_get_tick();

    for (;;)
    {
        sts = pPipeline->Run();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));
            sts = pPipeline->ResetDevice();
            MSDK_CHECK_STATUS(sts, "pPipeline->ResetDevice failed");

            sts = pPipeline->ResetMFXComponents(true);
            MSDK_CHECK_STATUS(sts, "pPipeline->ResetMFXComponents failed");

            continue;
        }
        else
        {
            MSDK_CHECK_STATUS(sts, "pPipeline->Run failed");
            break;
        }
    }

    pPipeline->Close();
    msdk_printf(MSDK_STRING("\nProcessing finished after %.2f sec \n"), MSDK_GET_TIME(msdk_time_get_tick(), startTime, frequency));

    return 0;
}

mfxStatus CheckOptions(AppConfig* pConfig)
{
    mfxStatus sts = MFX_ERR_NONE;
    if(pConfig->bENCODE && pConfig->dstFileBuff.size() == 0) {
        fprintf(stderr,"ERROR: Output bitstream file should be specified for ENC+PAK (-o)\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (!pConfig->bENCODE && !pConfig->bUseHWmemory) {
        fprintf(stderr, "ERROR: Only ENCODE supports SW memory\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    if (pConfig->bFieldProcessingMode &&
        !((pConfig->nPicStruct == MFX_PICSTRUCT_FIELD_BFF) || (pConfig->nPicStruct == MFX_PICSTRUCT_FIELD_TFF)))
    {
        fprintf(stderr, "ERROR: Field Processing mode works only with interlaced content (TFF or BFF)\n");
        sts = MFX_ERR_UNSUPPORTED;
    }

    return sts;
}

bool CompareFrameOrder(const DRCblock& left, const DRCblock& right)
{
    return left.start_frame < right.start_frame;
}

mfxStatus CheckDRCParams(AppConfig* pConfig)
{
    if (!pConfig->bDynamicRC)
    {
        return MFX_ERR_NONE;
    }

    /*
    if (pConfig->bENCPAK || pConfig->bOnlyENC || pConfig->bOnlyPAK || pConfig->bPREENC)
    {
        fprintf(stderr, "ERROR: Only ENCODE supports Dynamic Resolution Change\n");
        return MFX_ERR_UNSUPPORTED;
    } */

    for (mfxU32 i = 0; i < pConfig->DRCqueue.size(); ++i)
    {
        if (!(pConfig->DRCqueue[i].target_w * pConfig->DRCqueue[i].target_w))
        {
            fprintf(stderr, "ERROR: Incomplete DRC parameters for frame %d\n", pConfig->DRCqueue[i].start_frame);
            return MFX_ERR_UNSUPPORTED;
        }
    }

    std::sort(pConfig->DRCqueue.begin(), pConfig->DRCqueue.end(), CompareFrameOrder);

    for (mfxU32 i = 0; i < pConfig->DRCqueue.size() - 1; ++i)
    {
        if (pConfig->DRCqueue[i].start_frame == pConfig->DRCqueue[i + 1].start_frame)
        {
            fprintf(stderr, "ERROR: Incomplete DRC parameters, two identical resolution change points specified\n");
            return MFX_ERR_UNSUPPORTED;
        }
    }

    mfxU32 max_num_mb = 0;
    // In mixed picstructs case progressive frame holds maximum MBs
    mfxU32 wdt, hgt, n_fields = (pConfig->nPicStruct & MFX_PICSTRUCT_PROGRESSIVE) || pConfig->nPicStruct == MFX_PICSTRUCT_UNKNOWN ? 1 : 2;

    for (mfxU32 i = 0; i < pConfig->DRCqueue.size(); ++i)
    {
        wdt = MSDK_ALIGN16(pConfig->DRCqueue[i].target_w);
        hgt = (n_fields == 1) ? MSDK_ALIGN16(pConfig->DRCqueue[i].target_h) : MSDK_ALIGN32(pConfig->DRCqueue[i].target_h);
        max_num_mb = (std::max)(max_num_mb, ((wdt*hgt) >> 8) / n_fields);
    }

    pConfig->PipelineCfg.numMB_drc_max = max_num_mb;

    return MFX_ERR_NONE;
}
