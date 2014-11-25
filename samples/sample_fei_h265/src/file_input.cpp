/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014 Intel Corporation. All Rights Reserved.
//
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample_fei_h265.h"

/* disable warning for fscanf (use because fscanf_s not supported on Linux) */
#if defined (_WIN32) || defined (_WIN64)
#pragma warning(disable : 4996)
#endif

void SetFileDefaults(SampleParams *sp)
{
    /* use defaults if not set from the command line */
    if (!sp->MaxCUSize)
        sp->MaxCUSize = DEF_MAX_CU_SIZE;
    if (!sp->MPMode)
        sp->MPMode = DEF_MP_MODE;

    sp->NumRefFrames  = DEF_NUM_REF_FRAMES;
    sp->NumIntraModes = DEF_NUM_ANG_CAND;
}

int ReadFileParams(SampleParams *sp)
{
    int nRead;

    nRead = fscanf(sp->ParamsFile, "Width=%d\n",         &sp->PaddedWidth);
    nRead = fscanf(sp->ParamsFile, "Height=%d\n",        &sp->PaddedHeight);
    nRead = fscanf(sp->ParamsFile, "MaxCUSize=%d\n",     &sp->MaxCUSize);
    nRead = fscanf(sp->ParamsFile, "MPMode=%d\n",        &sp->MPMode);
    nRead = fscanf(sp->ParamsFile, "NumRefFrames=%d\n",  &sp->NumRefFrames);
    nRead = fscanf(sp->ParamsFile, "NumIntraModes=%d\n", &sp->NumIntraModes);
    nRead = fscanf(sp->ParamsFile, "\n");

    return nRead;
}

/* default frame order if parameters not supplied
 * Encode order:  0 | 1 2 3 4 5 6 7 8 |  9 10 11 12 13 14 15 16 | ...
 * Display order: 0 | 4 2 1 3 8 6 5 7 | 12 10  9 11 16 14 13 15 | ...
 * Frame type:    I | P B B B P B B B |  P  B  B  B  P  B  B  B | ...
 */

static const int DefPicOffset[8] = { +3, +0, -2, -1, +3, +0, -2, -1 };
static const int DefFrameType[8] = {MFX_FRAMETYPE_P, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B, MFX_FRAMETYPE_P, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B, MFX_FRAMETYPE_B};

static int EncToPic(int encOrder)
{
    return (encOrder == 0 ? 0 : encOrder + DefPicOffset[(encOrder - 1) & 0x07]);
}

void SetFrameDefaults(int encOrder, FrameInfo *fi)
{
    unsigned int i;

    fi->EncOrder  = encOrder;
    fi->PicOrder  = EncToPic(encOrder);
    fi->FrameType = (encOrder == 0 ? MFX_FRAMETYPE_I : DefFrameType[(encOrder - 1) & 0x07]);

    /* default - ref list uses frame N-1 and frame N-2
     * no real difference in how P and B frames are processed - each FEI inter call takes one source frame and one ref frame
     */
    fi->RefNum = (fi->FrameType == MFX_FRAMETYPE_B || fi->FrameType == MFX_FRAMETYPE_P ? 2 : 0);
    fi->RefNum = MIN(fi->EncOrder, 2);
    for (i = 0; i < fi->RefNum; i++) {
        fi->RefEncOrder[i] = fi->EncOrder - 1 - i;
        fi->RefPicOrder[i] = EncToPic(fi->RefEncOrder[i]);
    }
}

int ReadFrameParams(FILE *infileParams, FrameInfo *fi)
{
    int nRead;
    unsigned int i;

    nRead = fscanf(infileParams, "NewFrame\n");
    nRead = fscanf(infileParams, "FrameType = %d\n", &fi->FrameType);
    nRead = fscanf(infileParams, "EncOrder = %d\n",  &fi->EncOrder);
    nRead = fscanf(infileParams, "PicOrder = %d\n",  &fi->PicOrder);

    nRead = fscanf(infileParams, "RefNum = %d\n", &fi->RefNum);
    nRead = fscanf(infileParams, "RefEncOrder = ");
    for (i = 0; i < fi->RefNum; i++)
        nRead = fscanf(infileParams, "%d,", &fi->RefEncOrder[i]);
    nRead = fscanf(infileParams, "\n");
    nRead = fscanf(infileParams, "RefPicOrder = ");
    for (i = 0; i < fi->RefNum; i++)
        nRead = fscanf(infileParams, "%d,", &fi->RefPicOrder[i]);
    nRead = fscanf(infileParams, "\n");

    nRead = fscanf(infileParams, "EndFrame\n\n");

    return nRead;
}

int LoadFrame(FILE *infileYUV, int width, int height, int paddedWidth, int paddedHeight, int frameNum, unsigned char *srcBuf)
{
    int i, nRead;

    /* YUV420, only read Y plane, zero out padding areas and U/V */
    memset(srcBuf, 0, paddedWidth*paddedHeight*3/2);
    fseek(infileYUV, (width*height*3/2) * frameNum, SEEK_SET);

    nRead = 0;
    for (i = 0; i < height; i++)
        nRead += (int)fread(srcBuf + i*paddedWidth, 1, width, infileYUV);

    if (nRead < width*height)
        return -1;

    return 0;
}
