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

#ifndef __PIPELINE_DECODE_H__
#define __PIPELINE_DECODE_H__

#include "sample_defs.h"

#if D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>
#endif

#include <vector>
#include "hw_device.h"
#include "camera_render.h"
#include <memory>

#include "sample_camera_utils.h"

#include "sample_defs.h"
#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

#include "vm/thread_defs.h"

// The only supported padding size is 8
#define CAMERA_PADDING_SIZE 8

#define align(value) ((0x10) * ( (value) / (0x10) + (((value) % (0x10)) ? 1 : 0)))

#define align_32(value) ((0x20) * ( (value) / (0x20) + (((value) % (0x20)) ? 1 : 0)))

class CCameraPipeline
{
public:

    CCameraPipeline();
    virtual ~CCameraPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus Reset(sInputParams *pParams);

    void SetExtBuffersFlag()       { m_bIsExtBuffers = true; }
    void SetRenderingFlag()        { m_bIsRender = true; }
    void SetOutputfileFlag(bool b) { m_bOutput = b; }
    virtual void PrintInfo();

    mfxU32 GetNumberProcessedFrames() {return m_nFrameIndex;}

    mfxStatus PrepareInputSurfaces();

protected:
    CBmpWriter*             m_pBmpWriter;
    CRawVideoReader*        m_pRawFileReader;
    CRawVideoWriter*        m_pARGB16FileWriter;
    mfxU32                  m_nInputFileIndex;
    mfxU32                  m_nFrameIndex; // index of processed frame
    mfxU32                  m_nFrameLimit; // limit number of frames to proceed

    MFXVideoSession     m_mfxSession;
    MFXVideoVPP*        m_pmfxVPP;
    mfxVideoParam       m_mfxVideoParams;
    std::auto_ptr<MFXVideoUSER>  m_pUserModule;
    std::auto_ptr<MFXPlugin> m_pCamera_plugin;
    mfxPluginUID        m_UID_Camera;

    mfxExtCamGammaCorrection      m_GammaCorrection;
    mfxExtCamBlackLevelCorrection m_BlackLevelCorrection;
    mfxExtCamWhiteBalance         m_WhiteBalance;
    mfxExtCamColorCorrection3x3   m_CCM;
    mfxExtCamPipeControl     m_PipeControl;
    mfxExtCamPadding         m_Padding;

    std::vector<mfxExtBuffer *> m_ExtBuffers;

    MFXFrameAllocator*      m_pMFXd3dAllocator;
    mfxAllocatorParams*     m_pmfxd3dAllocatorParams;
    MFXFrameAllocator*      m_pMFXsysAllocator;
    mfxAllocatorParams*     m_pmfxsysAllocatorParams;

    MFXFrameAllocator*      m_pMFXAllocatorIn;
    mfxAllocatorParams*     m_pmfxAllocatorParamsIn;
    MFXFrameAllocator*      m_pMFXAllocatorOut;
    mfxAllocatorParams*     m_pmfxAllocatorParamsOut;
    MemType                 m_memType;         // memory type of surfaces to use
    MemType                 m_memTypeIn;         // memory type of surfaces to use
    MemType                 m_memTypeOut;         // memory type of surfaces to use
    bool                    m_bExternalAllocIn;  // use memory allocator as external for Media SDK
    bool                    m_bExternalAllocOut;  // use memory allocator as external for Media SDK
    mfxFrameSurface1*       m_pmfxSurfacesIn; // frames array
    mfxFrameSurface1*       m_pmfxSurfacesOut; // frames array
    mfxFrameAllocResponse   m_mfxResponseIn;  // memory allocation response
    mfxFrameAllocResponse   m_mfxResponseOut;  // memory allocation response

    mfxFrameAllocResponse   m_mfxResponseAux;  // memory allocation response
    mfxFrameSurface1*       m_pmfxSurfacesAux; // frames array

    mfxFrameAllocRequest    m_lastAllocRequest[2];

    bool                    m_bIsExtBuffers; // indicates if external buffers were allocated
    bool                    m_bIsRender; // enables rendering mode
    bool                    m_bOutput; // enables/disables output file
    bool                    m_bEnd;

    mfxI32                 m_alphaValue;
    mfxU32                 m_BayerType;

    mfxU32                 m_numberOfResets;
    mfxU32                 m_resetCnt;
    mfxU32                 m_resetInterval;

    CHWDevice               *m_hwdev;
#if D3D_SURFACES_SUPPORT
    CCameraD3DRender         m_d3dRender;
#endif
    virtual mfxStatus InitMfxParams(sInputParams *pParams);

    // function for allocating a specific external buffer
    template <typename Buffer>
    mfxStatus AllocateExtBuffer();
    virtual void DeleteExtBuffers();

    virtual void AttachExtParam();

    virtual mfxStatus AllocAndInitCamGammaCorrection(sInputParams *pParams);
    virtual mfxStatus AllocAndInitCamWhiteBalance(sInputParams *pParams);
    virtual mfxStatus AllocAndInitCamCCM(sInputParams *pParams);
    virtual mfxStatus AllocAndInitCamBlackLevelCorrection(sInputParams *pParams);
    virtual void FreeCamGammaCorrection();

    virtual mfxStatus CreateAllocator();
    virtual mfxStatus CreateHWDevice();
    virtual mfxStatus AllocFrames();
    virtual mfxStatus ReallocFrames(mfxVideoParam *par);
    virtual void DeleteFrames();
    virtual void DeleteAllocator();

    //virtual mfxStatus PrepareInSurface(mfxFrameSurface1  **ppSurface, mfxU32 indx = 0);
};

#endif // __PIPELINE_DECODE_H__