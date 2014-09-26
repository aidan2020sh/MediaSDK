//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#include "mfxdefs.h"
#include "mfxstructures.h"
#include "mfxvideo.h"
#include "mfxplugin.h"
#include "mfxcamera.h"

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <atlbase.h>

#include <initguid.h>
#pragma warning(disable : 4201) // Disable annoying DX warning
#include <d3d9.h>
#include <dxva2api.h>


#define  CHECK_POINTER(P, ...)               {if (!(P)) {return __VA_ARGS__;}}
#define  CHECK_RESULT(P, X, ERR)       {if ((X) > (P)) {return ERR;}}
#define  IGNORE_MFX_STS(P, X)                {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define  BREAK_ON_ERROR(P)                   {if (MFX_ERR_NONE != (P)) break;}
#define  VPP_WAIT_INTERVAL 60000
#define  MSDK_SAFE_RELEASE(X)            {if (X) { X->Release(); X = NULL; }}

#define  VPP_IN  0
#define  VPP_OUT 1

// Parameters need to be specified by Application
//#define  RAW_BIT_DEPTH       12

//#define  RAW_FRAME_WIDTH    4096
//#define  RAW_FRAME_HEIGHT   2160

//#define  RAW_BAYER_TYPE  MFX_CAM_BAYER_BGGR
//#define  RAW_BAYER_TYPE  MFX_CAM_BAYER_RGGB
//#define  RAW_BAYER_TYPE  MFX_CAM_BAYER_GRBG
//#define  RAW_BAYER_TYPE  MFX_CAM_BAYER_GBRG

typedef struct _ownFrameInfo
{
  mfxU16  nWidth;
  mfxU16  nHeight;
  // ROI
  mfxU16  CropX;
  mfxU16  CropY;
  mfxU16  CropW;
  mfxU16  CropH;

  mfxU32 FourCC;
  mfxU8  PicStruct;
  mfxF64 dFrameRate;

} sOwnFrameInfo;


#define MAX_FILENAME_LEN 1024 

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};


struct sInputParams
{

    sOwnFrameInfo frameInfo[2];  // [0] - in, [1] - out

    mfxU32  rawType;
    mfxU32  bitDepth;
	mfxU32  raw_width;
	mfxU32  raw_height;
    bool    bOutput; // if renderer is enabled, possibly no need in output file
    bool    bPostProcess; // if renderer is enabled, possibly no need in output file
    bool    bSwitchRequired;
    mfxI32  maxNumBmpFiles;
    MemType memType;
    MemType memTypeIn;
    MemType memTypeOut;
    MemType memPostPTypeIn;
    MemType memPostPTypeOut;
    mfxU32  CameraPluginVersion;

    mfxU32  nFramesToProceed;

    char  strSrcFile[MAX_FILENAME_LEN];
    char  strDstFile[MAX_FILENAME_LEN];
    char  strPluginPath[MAX_FILENAME_LEN];

    mfxU32 fileFramePitchBytes;
    mfxU32 fileFrameHeight;
    mfxU16 *pRawData;

    mfxSession   mfx_session;
    mfxSession   mfx_session_postprocess;
    mfxPluginUID UID_Camera;
    mfxVideoParam    mfx_videoParams;
    mfxVideoParam    mfx_videoPostProcessParams;

    mfxFrameSurface1  mfx_surfaces[2]; // frames array
    mfxFrameSurface1  mfx_surfaces_postprocess[2]; // frames array
    

    mfxExtCamGammaCorrection m_GammaCorrection;
    mfxExtCamPipeControl     m_PipeControl;
    mfxExtCamPadding         m_Padding; 

    mfxExtBuffer *m_ExtBuffers[3];

    void *pMemoryAllocated;


    mfxFrameAllocator mfxAllocator;
    mfxFrameAllocator mfxAllocator_postProc;
    mfxHDL deviceHandle;
    mfxFrameAllocResponse   mfxResponseIn;  // memory allocation response
    mfxFrameAllocResponse   mfxResponseOut;  // memory allocation response
    mfxFrameAllocResponse   mfxResponseOut_postprocess;  // memory allocation response

	bool bOutARGB16;

};



// =================================================================
// DirectX functionality required to manage D3D surfaces
//

// Create DirectX 9 device context
// - Required when using D3D surfaces.
// - D3D Device created and handed to Intel Media SDK
// - Intel graphics device adapter id will be determined automatically (does not have to be primary),
//   but with the following caveats:
//     - Device must be active. Normally means a monitor has to be physically attached to device
//     - Device must be enabled in BIOS. Required for the case when used together with a discrete graphics card
//     - For switchable graphics solutions (mobile) make sure that Intel device is the active device
mfxStatus CreateHWDevice(mfxSession session, mfxHDL* deviceHandle, HWND hWnd, bool bCreateSharedHandles = false);
void CleanupHWDevice();
IDirect3DDevice9Ex* GetDevice();

mfxStatus simple_alloc(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
mfxStatus simple_lock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_unlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_gethdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus simple_free(mfxHDL pthis, mfxFrameAllocResponse *response);


// dx11
mfxStatus CreateHWDevice_dx11(mfxSession session, mfxHDL* deviceHandle, HWND hWnd);
void CleanupHWDevice_dx11();
#define WILL_READ  0x1000
#define WILL_WRITE 0x2000

mfxStatus simple_alloc_dx11(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
mfxStatus simple_lock_dx11(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_unlock_dx11(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
mfxStatus simple_gethdl_dx11(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
mfxStatus simple_free_dx11(mfxHDL pthis, mfxFrameAllocResponse *response);
