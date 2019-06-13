// Copyright (c) 2014-2018 Intel Corporation
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

#ifndef _MFX_H265_FEI_H
#define _MFX_H265_FEI_H

#include "mfxdefs.h"



//#include "mfxfeih265.h"     /* inherit public API - this file should only contain non-public definitions */
//============================== begin of new mfxfeih265.h ==============================
#include "mfxdefs.h"
#include "mfxstructures.h"

    //============================== begin of patch for mfxstructures.h ==============================
    /* required buffer sizes - returned from call to Query() */
    typedef struct
    {
        mfxU32 size;
        mfxU32 pitch;
        mfxU32 align;
        mfxU32 numBytesInRow;
        mfxU32 numRows;
   
        mfxU32 reserved[3];     /* do not modify */
    } mfxSurfInfoENC;
    //============================== end of patch formfxstructures.h ==============================


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* FEI constants */
enum
{
    MFX_FEI_H265_INTERP_BORDER      = 4,
    MFX_FEI_H265_MAX_NUM_REF_FRAMES = 8,
    MFX_FEI_H265_MAX_INTRA_MODES    = 33,  /* intra angular prediction modes (excluding DC and planar) */
};

/* FEI block sizes */
enum
{
    MFX_FEI_H265_BLK_32x32 = 3,
    MFX_FEI_H265_BLK_32x16 = 4,
    MFX_FEI_H265_BLK_16x32 = 5,
    MFX_FEI_H265_BLK_16x16 = 6,
    MFX_FEI_H265_BLK_16x8  = 7,
    MFX_FEI_H265_BLK_8x16  = 8,
    MFX_FEI_H265_BLK_8x8   = 9,
};

/* FEI operations */
enum
{
    MFX_FEI_H265_OP_NOP             = 0x00,
    MFX_FEI_H265_OP_GPU_COPY_SRC    = 0x01,
    MFX_FEI_H265_OP_GPU_COPY_REF    = 0x02,
    MFX_FEI_H265_OP_INTRA_MODE      = 0x04,
    MFX_FEI_H265_OP_INTRA_DIST      = 0x08,
    MFX_FEI_H265_OP_INTER_ME        = 0x10,
    MFX_FEI_H265_OP_INTERPOLATE     = 0x20,
    MFX_FEI_H265_OP_BIREFINE        = 0x40,
    MFX_FEI_H265_OP_POSTPROC        = 0x80, // both: deblock + sao
    MFX_FEI_H265_OP_DEBLOCK         = 0x100,// deblocking only
};

typedef struct
{
    mfxU16 Dist;
    mfxU16 reserved;
} mfxFEIH265IntraDist;

typedef struct
{
    mfxI16Pair mv0;
    mfxI16Pair mv1;
} mfxFEIH265BirefData;

enum 
{
    MFX_EXTBUFF_FEI_H265_PARAM  =   MFX_MAKEFOURCC('F','E','5','P'),
    MFX_EXTBUFF_FEI_H265_ALLOC  =   MFX_MAKEFOURCC('F','E','5','A'),
    MFX_EXTBUFF_FEI_H265_INPUT  =   MFX_MAKEFOURCC('F','E','5','I'),
    MFX_EXTBUFF_FEI_H265_OUTPUT =   MFX_MAKEFOURCC('F','E','5','O'),
};

typedef struct
{
    mfxExtBuffer       Header;

    mfxU32             MaxCUSize;
    mfxU32             MPMode;
    mfxU32             NumIntraModes;
    mfxU32             BitDepth;
    mfxU32             TargetUsage;

    mfxU16             reserved[20];
} mfxExtFEIH265Param;

typedef struct
{
    mfxExtBuffer       Header;

    mfxU32             FEIOp;
    mfxU32             FrameType;

    mfxHDL             SurfInSrc;
    mfxHDL             SurfInRec;

    mfxU16             reserved[22];
} mfxExtFEIH265Input;

typedef struct
{
    mfxExtBuffer       Header;

    mfxHDL             SurfIntraMode[4];
    mfxHDL             SurfIntraDist;
    mfxHDL             SurfInterData[64];
    mfxHDL             SurfInterp[3];
    mfxHDL             SurfBirefData[64];

    mfxU16             reserved[24];
} mfxExtFEIH265Output;

typedef struct
{
    mfxExtBuffer       Header;

    mfxSurfInfoENC     IntraMode[4];    /* 4 = intra block sizes (square 4,8,16,32) */
    mfxSurfInfoENC     IntraDist;
    mfxSurfInfoENC     InterData[64];   /* 64 = max number of inter block sizes */
    mfxSurfInfoENC     Interpolate[3];  /* 3 = half-pel planes (H,V,D) */
    mfxSurfInfoENC     BirefData[64];
    mfxSurfInfoENC     SrcRefLuma;      /* luma plane of source and reconstracted reference frames */
    mfxSurfInfoENC     SrcRefChroma;    /* chroma plane of source and reconstracted reference frames */

    mfxU16             reserved[32];
} mfxExtFEIH265Alloc;

#ifdef __cplusplus
}
#endif
//============================== end of new mfxfeih265.h ==============================




#ifdef MFX_VA
#include "cmrt_cross_platform.h"
#else //MFX_VA
class CmSurface2D;
class CmSurface2DUP;
class CmBuffer;
class CmBufferUP;
#endif //MFX_VA

//#define SAVE_FEI_STATE      /* enabling will save FEI parameters and output to txt files for unit testing with sample app */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mfxFEIH265;
typedef void *mfxFEISyncPoint;

/* input parameters - set once at init */
typedef struct
{
    mfxU32 Width;
    mfxU32 Height;
    mfxU32 Padding;
    mfxU32 WidthChroma;
    mfxU32 HeightChroma;
    mfxU32 PaddingChroma;
    mfxU32 MaxCUSize;
    mfxU32 MPMode;
    mfxU32 NumRefFrames;
    mfxU32 NumIntraModes;
    mfxU32 FourCC;
    mfxU32 TargetUsage;
    mfxU32 EnableChromaSao;
    mfxU32 InterpFlag;
} mfxFEIH265Param;

/* basic info for current and reference frames */
typedef struct
{
    void  *YPlane;
    void  *UVPlane;
    mfxU32 YPitch;
    mfxU32 UVPitch;
    mfxU32 YBitDepth;
    mfxU32 PicOrder;
    mfxU32 EncOrder;

    mfxU32 copyFrame;
    mfxHDL surfIn;
} mfxFEIH265Frame;

/* basic info for current and reference frames */
typedef struct
{
//    mfxFEIH265Frame FEIFrameRefBi;
    mfxHDL          InterDataRef0[12];
    mfxHDL          InterDataRef1[12];
} mfxFEIOptParamsBiref;

/* FEI input - update before each call to ProcessFrameAsync */
typedef struct
{
    mfxU32 FEIOp;
    mfxU32 FrameType;
    mfxU32 SaveSyncPoint;

    //mfxFEIH265Frame FEIFrameIn;
    //mfxFEIH265Frame FEIFrameRef;

    union {
        struct {
            mfxHDL surfSrc;
            mfxHDL surfRef;
            double lambda;
        } meArgs;

        struct {
            mfxHDL surfSrc;
            mfxHDL surfRef0;
            mfxHDL surfRef1;
            mfxHDL params;
        } birefArgs;

        struct {
            mfxHDL surfSys;
            mfxHDL surfVid;
        } copyArgs;

        struct {
            mfxHDL inputSurf;    // original pixels
            mfxHDL reconSurfSys; // reconstructed pixels in system memory (input)
            mfxHDL reconSurfVid; // reconstructed pixels in video memory (output)
            mfxHDL cuData;       // input cuData
            mfxHDL saoModes;     // output saoModes
            mfxU8* param;        // postproc param (general for deblock + sao)
        } postprocArgs;
    };

} mfxFEIH265Input;

typedef struct _FEIOutput {
    /* ranked list of IntraMaxModes modes for each block size */
    mfxU32                IntraMaxModes;
    mfxU32              * IntraModes4x4;
    mfxU32              * IntraModes8x8;
    mfxU32              * IntraModes16x16;
    mfxU32              * IntraModes32x32;
    mfxU32                IntraPitch4x4;
    mfxU32                IntraPitch8x8;
    mfxU32                IntraPitch16x16;
    mfxU32                IntraPitch32x32;

    /* intra distortion estiamates (16x16 grid) */
    mfxU32                IntraPitch;
    mfxFEIH265IntraDist * IntraDist;

    /* motion vector estimates for multiple block sizes */
    mfxU32                PitchDist[64];
    mfxU32                PitchMV[64];
    mfxU32              * Dist[16][64];
    mfxI16Pair          * MV[16][64];
    
    /* half-pel interpolated planes - 4-pixel border added on all sides */
    mfxU32                InterpolateWidth;
    mfxU32                InterpolateHeight;
    mfxU32                InterpolatePitch[3];
    mfxU8               * Interp[16][3];

#ifdef SAVE_FEI_STATE
    /* intra processing pads to multiple of 16 if needed */
    mfxU32                PaddedWidth;
    mfxU32                PaddedHeight;
#endif
} mfxFEIH265Output;

enum mfxFEIH265SurfaceType
{
    MFX_FEI_H265_SURFTYPE_INVALID = -1,

    MFX_FEI_H265_SURFTYPE_INPUT = 0,
    MFX_FEI_H265_SURFTYPE_RECON,
    MFX_FEI_H265_SURFTYPE_UP,
    MFX_FEI_H265_SURFTYPE_OUTPUT,
    MFX_FEI_H265_SURFTYPE_OUTPUT_BUFFER,
};

typedef struct
{
    CmSurface2D   *bufLuma10bit;
    CmSurface2D   *bufChromaRext;
    CmSurface2D   *bufOrigNv12;
    CmSurface2D   *bufDown2x;
    CmSurface2D   *bufDown4x;
    CmSurface2D   *bufDown8x;
    CmSurface2D   *bufDown16x;
    CmBuffer      *meControl;
    CmBuffer      *postprocParam;
    double         lambda;
    bool           meControlInited;
} mfxFEIH265InputSurface;

typedef struct
{
    CmSurface2D   *bufLuma10bit;
    CmSurface2D   *bufChromaRext;
    CmSurface2D   *bufOrigNv12;
    CmSurface2D   *bufDown2x;
    CmSurface2D   *bufDown4x;
    CmSurface2D   *bufDown8x;
    CmSurface2D   *bufDown16x;
    CmSurface2D   *bufInterpMerged;
} mfxFEIH265ReconSurface;

typedef struct
{
    CmSurface2DUP *luma;
    CmSurface2DUP *chroma;
} mfxFEIH265SurfaceUp;

typedef struct
{
    unsigned char *sysMem;
    CmSurface2DUP *bufOut;
} mfxFEIH265OutputSurface;

typedef struct
{
    unsigned char *sysMem;
    CmBufferUP    *bufOut;
} mfxFEIH265OutputBuffer;

typedef struct
{
    mfxFEIH265SurfaceType surfaceType;

    union {
        mfxFEIH265InputSurface   sIn;
        mfxFEIH265ReconSurface   sRec;
        mfxFEIH265SurfaceUp      sUp;
        mfxFEIH265OutputSurface  sOut;
        mfxFEIH265OutputBuffer   sBuf;
    };
} mfxFEIH265Surface;

/* must correspond with public API! */
enum UnsupportedBlockSizes {
    MFX_FEI_H265_BLK_8x4_US   = 10,
    MFX_FEI_H265_BLK_4x8_US   = 11,

    /* only used internally */
    MFX_FEI_H265_BLK_256x256 = 0,
    MFX_FEI_H265_BLK_128x128 = 1,
    MFX_FEI_H265_BLK_64x64 = 2,

    /* public API allocates 64 slots so this could be increased later (update SyncCurrent()) */
    MFX_FEI_H265_BLK_MAX      = 12,
};

struct PostProcParam
{
    Ipp8u  tabBeta[52];            // +0 B
    Ipp16u Width;                  // +26 W
    Ipp16u Height;                 // +27 W
    Ipp16u PicWidthInCtbs;         // +28 W
    Ipp16u PicHeightInCtbs;        // +29 W

    Ipp16s tcOffset;               // +30 W
    Ipp16s betaOffset;             // +31 W
    Ipp8u  chromaQp[58];           // +64 B
    Ipp8u  crossSliceBoundaryFlag; // +122 B
    Ipp8u  crossTileBoundaryFlag;  // +123 B
    Ipp8u  TULog2MinSize;          // +124 B
    Ipp8u  MaxCUDepth;             // +125 B
    Ipp8u  Log2MaxCUSize;          // +126 B
    Ipp8u  Log2NumPartInCU;        // +127 B
    Ipp8u  tabTc[54];              // +128 B
    Ipp8u  MaxCUSize;              // +182 B
    Ipp8u  chromaFormatIdc;        // +183 B

    Ipp8u  alignment0[8];          // +184 B
    Ipp32s list0[16];              // +48 DW
    Ipp32s list1[16];              // +64 DW
    Ipp8u  scan2z[16];             // +320 B
    // sao extension
    Ipp32f m_rdLambda;             // +84 DW
    Ipp32s SAOChromaFlag;          // +85 DW
    Ipp32s enableBandOffset;       // +86 DW
    //Ipp8u reserved[4];             // +87 DW
    // kernel SaoStatChroma writes stats starting from this offset
    Ipp32s offsetChroma;           // +87 DW
    // tile/slice restriction (avail LeftAbove)
    Ipp8u availLeftAbove[196];     // +88 DW
};


/* FEI functions - LIB interface */
// VideoCore* is needed for multisession mode to have its own CmDevice in each session (number of Cm surfases is restricted)
mfxStatus H265FEI_Init(mfxFEIH265 *feih265, mfxFEIH265Param *param, void *core = NULL);
mfxStatus H265FEI_Close(mfxFEIH265 feih265);
mfxStatus H265FEI_ProcessFrameAsync(mfxFEIH265 feih265, mfxFEIH265Input *in, mfxExtFEIH265Output *out, mfxFEISyncPoint *syncp);
mfxStatus H265FEI_SyncOperation(mfxFEIH265 feih265, mfxFEISyncPoint syncp, mfxU32 wait);
mfxStatus H265FEI_DestroySavedSyncPoint(mfxFEIH265 feih265, mfxFEISyncPoint syncp);

mfxStatus H265FEI_GetSurfaceDimensions(void *core, mfxU32 width, mfxU32 height, mfxExtFEIH265Alloc *extAlloc);
mfxStatus H265FEI_GetSurfaceDimensions_new(void *core, mfxFEIH265Param *param, mfxExtFEIH265Alloc *extAlloc);
mfxStatus H265FEI_GetSurfaceDimensions_Initialized(mfxFEIH265 feih265, mfxFEIH265Param *param, mfxExtFEIH265Alloc *extAlloc);
mfxStatus H265FEI_AllocateSurfaceUp(mfxFEIH265 feih265, mfxU8 *sysMemLu, mfxU8 *sysMemCh, mfxHDL *pInSurf);
mfxStatus H265FEI_AllocateInputSurface(mfxFEIH265 feih265, mfxHDL *pInSurf);
mfxStatus H265FEI_AllocateReconSurface(mfxFEIH265 feih265, mfxHDL *pRecSurf);
mfxStatus H265FEI_AllocateOutputSurface(mfxFEIH265 feih265, mfxU8 *sysMem, mfxSurfInfoENC *surfInfo, mfxHDL *pOutSurf);
mfxStatus H265FEI_AllocateOutputBuffer(mfxFEIH265 feih265, mfxU8 *sysMem, mfxU32 size, mfxHDL *pOutBuf);
mfxStatus H265FEI_FreeSurface(mfxFEIH265 feih265, mfxHDL pSurf);

#ifdef __cplusplus
}
#endif

#endif  /* _MFX_H265_FEI_H */