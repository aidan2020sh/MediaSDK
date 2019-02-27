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

#ifndef _SAMPLE_H265_FEI_H_
#define _SAMPLE_H265_FEI_H_

#include <stdio.h>

#include "mfxfeih265.h"
#include "sample_defs.h"

#if defined (LINUX) && defined (__GNUC__)
#include <malloc.h>
#include <stdlib.h>
#define fopen_s(ptr, name, m) (((*(ptr)) = fopen((name), (m))) == NULL)
static __inline void *_aligned_malloc(int size, int align) { void *ptr; return (posix_memalign(&ptr, align, size) ? NULL: ptr); }
#define _aligned_free(ptr) free(ptr)
#endif

#ifndef MAX
#define MAX(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)            (((a) < (b)) ? (a) : (b))
#endif

#define DEF_MAX_CU_SIZE     32
#define DEF_MP_MODE         3
#define DEF_NUM_REF_FRAMES  4
#define DEF_NUM_ANG_CAND    1

typedef struct _SampleParams {
    mfxU32 Width;
    mfxU32 Height;
    mfxU32 NFrames;
    mfxU32 WriteFiles;

    mfxU32 PaddedWidth;
    mfxU32 PaddedHeight;

    FILE  *ParamsFile;
    FILE  *SourceFile;
    FILE  *ReconFile;

    mfxU32 MaxCUSize;
    mfxU32 MPMode;
    mfxU32 NumRefFrames;
    mfxU32 NumIntraModes;

    mfxChar PluginPath[MSDK_MAX_FILENAME_LEN];
} SampleParams;

typedef struct _FrameInfo {
    mfxU32  EncOrder;
    mfxU32  PicOrder;
    mfxU32  FrameType;

    mfxU32  RefNum;
    mfxU32  RefEncOrder[MFX_FEI_H265_MAX_NUM_REF_FRAMES + 1];
    mfxU32  RefPicOrder[MFX_FEI_H265_MAX_NUM_REF_FRAMES + 1];

} FrameInfo;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    mfxU32 FEIOp;
    mfxU32 FrameType;
    mfxU32 RefIdx;

    /* input frame */
    mfxU8* YPlaneSrc;
    mfxI32 YPitchSrc;
    mfxI32 PicOrderSrc;
    mfxI32 EncOrderSrc;

    /* reference frame */
    mfxU8* YPlaneRef;
    mfxI32 YPitchRef;
    mfxI32 PicOrderRef;
    mfxI32 EncOrderRef;

} ProcessInfo;


/* file input (file/frame parameters, input/recon YUV) */
void SetFileDefaults(SampleParams *sp);
int ReadFileParams(SampleParams *sp);
void SetFrameDefaults(int encOrder, FrameInfo *fi);
int ReadFrameParams(FILE *infileParams, FrameInfo *fi);
int LoadFrame(FILE *infileYUV, int width, int height, int paddedWidth, int paddedHeight, int frameNum, unsigned char *srcBuf);

/* file output */
void OpenOutputFiles(void);
void CloseOutputFiles(void);
void WriteFrameIntraModes(mfxFEIH265Output *feiOut);
void WriteFrameIntraDist(mfxFEIH265Output *feiOut);
void WriteFrameInterLarge(mfxFEIH265Output *feiOut, int refIdx, int blockSize);
void WriteFrameInterSmall(mfxFEIH265Output *feiOut, int refIdx, int blockSize);
void WriteFrameInterpolate(mfxFEIH265Output *feiOut, int refIdx);

#endif /* _SAMPLE_H265_FEI_H_ */