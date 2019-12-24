// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.


#ifndef _MFX_AV1_ENC_CM_FEI_H
#define _MFX_AV1_ENC_CM_FEI_H

#include "mfx_av1_fei.h"
#include "mfx_av1_enc_cm_utils.h"
#include "cmrt_cross_platform.h"

#pragma warning(disable: 4351)

namespace AV1Enc {

enum AV1VMEMode {
    VME_MODE_SMALL = 0,
    VME_MODE_LARGE,
    VME_MODE_REFINE,
};

struct PicBufGpu {
    int32_t       encOrder;
    CmSurface2D *bufOrig;
    CmSurface2D *bufDown2x;
    CmSurface2D *bufDown4x;
    CmSurface2D *bufDown8x;
    CmSurface2D *bufDown16x;

    CmSurface2DUP *bufHPelH;
    CmSurface2DUP *bufHPelV;
    CmSurface2DUP *bufHPelD;
};

class AV1CmCtx {

private:
    /* internal helper functions */
    mfxStatus CopyInputFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in);
    mfxStatus CopyReconFrameToGPU(CmEvent **lastEvent, mfxFEIH265Input *in);

    void LoadPrograms(GPU_PLATFORM hwType);
    void SetupKernels();

    /* Cm elements */
    CmDevice  * device;
    CmQueue   * queue;
    CmProgram * programPrepareSrc;
    CmProgram * programHmeMe32;
    CmProgram * programMe16Refine32x32;
#if NEWMVPRED
    CmProgram * programMePu;
#endif
    CmProgram * programMd;
    CmProgram * programMdPass2;

#if !USE_HWPAK_RESTRICT
    CmProgram * programInterpDecision;
#else
    CmProgram * programInterpDecisionSingle;
#endif

#if GPU_VARTX
    CmProgram * programVarTxDecision;
#endif // GPU_VARTX
    CmProgram * programAv1Intra;
    CmProgram * programRefine64x64;
    CmEvent   * lastEventSaved;
    mfxU32      saveSyncPoint;

    CmSurface2D * newMvPred[4];
    CmSurface2D * mePuScratchPad;

    /* Cm kernels to load  */
    AV1Enc::Kernel kernelPrepareSrc;
    AV1Enc::Kernel kernelPrepareRef;
    //AV1Enc::Kernel kernelMeIntra;
    AV1Enc::Kernel kernelGradient;
    AV1Enc::Kernel kernelMe[8];
    AV1Enc::Kernel kernelMd[8];
#if GPU_VARTX
    AV1Enc::Kernel kernelVarTx;
#endif // GPU_VARTX
#if NEWMVPRED
    AV1Enc::Kernel kernelMePu;
#endif
    AV1Enc::Kernel kernelBiRefine;
    AV1Enc::Kernel kernelRefine64x64;
    AV1Enc::Kernel kernelRefine32x32sad;
    AV1Enc::Kernel kernelInterpolateFrame;
    AV1Enc::Kernel kernelDeblock;
    AV1Enc::Kernel kernelFullPostProc;

    /* set once at init */
    mfxU32 sourceWidth;
    mfxU32 sourceHeight;
    mfxU32 width;
    mfxU32 height;
    mfxU32 padding;
    mfxU32 widthChroma;
    mfxU32 heightChroma;
    mfxU32 paddingChroma;
    mfxU32 numRefFrames;
    mfxU32 numIntraModes;
    mfxU32 fourcc;
    mfxU32 bppShift;
    mfxU32 targetUsage;
    mfxU32 enableChromaSao;
    mfxU32 enableInterp;
    mfxI32 numSlices;
    mfxI32 hmeSliceStart[8 + 1];
    mfxI32 mdSliceStart[8 + 1];
    mfxI32 md2SliceStart[8 + 1];
    mfxU32 sliceHeight;
    mfxU32 rectParts;

    /* derived from initialization parameters */
    mfxU32 width32;
    mfxU32 height32;
    mfxU32 width2x;
    mfxU32 height2x;
    mfxU32 width4x;
    mfxU32 height4x;
    mfxU32 width8x;
    mfxU32 height8x;
    mfxU32 width16x;
    mfxU32 height16x;

    mfxU32 interpWidth;
    mfxU32 interpHeight;
    mfxU32 interpBlocksW;
    mfxU32 interpBlocksH;

    mfxU32 numRefBufs;
    AV1VMEMode vmeMode;

public:
    /* called from C wrapper (public) */
    void * AllocateSurface(mfxFEIH265SurfaceType surfaceType, void *sysMem1, void *sysMem2, mfxSurfInfoENC *surfInfo);
    mfxStatus FreeSurface(mfxFEIH265Surface *s);
    mfxStatus FreeBuffer(mfxFEIH265Surface *s);

    mfxStatus AllocateCmResources(mfxFEIH265Param *param, void *core);
    void FreeCmResources(void);
    void * RunVme(mfxFEIH265Input *feiIn, mfxExtFEIH265Output *feiOut);
    mfxStatus SyncCurrent(void *syncp, mfxU32 wait);
    mfxStatus DestroySavedSyncPoint(void *syncp);

    /* zero-init all state variables */
    AV1CmCtx() :
        device(),
        queue(),
        programPrepareSrc(),
        programHmeMe32(),
        programMe16Refine32x32(),
#if NEWMVPRED
        programMePu(),
#endif
        programMd(),
        programMdPass2(),

#if !USE_HWPAK_RESTRICT
        programInterpDecision(),
#else
        programInterpDecisionSingle(),
#endif

#if GPU_VARTX
        programVarTxDecision(),
#endif // GPU_VARTX
        programAv1Intra(),
        programRefine64x64(),
        lastEventSaved(),
        saveSyncPoint(),

        kernelPrepareSrc(),
        kernelPrepareRef(),
        //kernelMeIntra(),
        kernelGradient(),
#if GPU_VARTX
        kernelVarTx(),
#endif // GPU_VARTX
#if NEWMVPRED
        kernelMePu(),
#endif
        kernelBiRefine(),
        kernelRefine64x64(),
        kernelRefine32x32sad(),
        kernelInterpolateFrame(),
        kernelDeblock(),
        kernelFullPostProc(),
        sourceWidth(0),
        sourceHeight(0),
        width(),
        height(),
        padding(0),
        widthChroma(0),
        heightChroma(0),
        paddingChroma(0),
        numRefFrames(),
        numIntraModes(),
        fourcc(),
        bppShift(0),
        targetUsage(),
        enableChromaSao(),
        enableInterp(),
        numSlices(1),
        sliceHeight(0),
        rectParts(0),
        width32(),
        height32(),
        width2x(),
        height2x(),
        width4x(),
        height4x(),
        width8x(),
        height8x(),
        width16x(),
        height16x(),
        interpWidth(),
        interpHeight(),
        interpBlocksW(),
        interpBlocksH(),
        numRefBufs(),
        vmeMode()
    {
        memset(newMvPred, 0, sizeof(newMvPred));
        memset(kernelMe, 0, sizeof(kernelMe));
        memset(kernelMd, 0, sizeof(kernelMd));
        memset(hmeSliceStart, 0, sizeof(hmeSliceStart));
        memset(mdSliceStart, 0, sizeof(mdSliceStart));
        memset(md2SliceStart, 0, sizeof(md2SliceStart));
    }

    ~AV1CmCtx() { }
};

}

#endif /* _MFX_H265_ENC_CM_FEI_H */
