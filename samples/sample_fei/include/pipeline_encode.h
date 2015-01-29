//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2005-2014 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __PIPELINE_ENCODE_H__
#define __PIPELINE_ENCODE_H__

#include "sample_defs.h"
//#include "mfx_plugin_uids.h"
#include "hw_device.h"

#include <mfxfei.h>

#ifdef D3D_SURFACES_SUPPORT
#pragma warning(disable : 4201)
#endif

#include "sample_utils.h"
#include "base_allocator.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"

#include <vector>
#include <memory>

#if defined(_WIN32) || defined(_WIN64)
    #define PLUGIN_NAME "sample_rotate_plugin.dll"
#else
    #define PLUGIN_NAME "libsample_rotate_plugin.so"
#endif

enum {
    MVC_DISABLED          = 0x0,
    MVC_ENABLED           = 0x1,
    MVC_VIEWOUTPUT        = 0x2,    // 2 output bitstreams
};

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

struct sInputParams
{
    mfxU16 nTargetUsage;
    mfxU32 CodecId;
    mfxU32 ColorFormat;
    mfxU16 nPicStruct;
    mfxU16 nWidth; // source picture width
    mfxU16 nHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU16 nBitRate;
    mfxU16 nQuality; // quality parameter for JPEG encoder
    mfxU32 nNumFrames;
    mfxI32 refDist; //number of frames to next I,P
    mfxI32 gopSize; //number of frames to next I
    mfxU8  QP;
    mfxU16 numSlices;
    mfxU16 numRef;

    mfxU32 numViews; // number of views for Multi-View Codec

    mfxU16 nDstWidth; // destination picture width, specified if resizing required
    mfxU16 nDstHeight; // destination picture height, specified if resizing required

    MemType memType;
    bool bUseHWLib; // true if application wants to use HW MSDK library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    std::vector<msdk_char*> srcFileBuff;
    std::vector<msdk_char*> dstFileBuff;

    mfxU32  HEVCPluginVersion;
    mfxU8 nRotationAngle; // if specified, enables rotation plugin in mfx pipeline
    msdk_char strPluginDLLPath[MSDK_MAX_FILENAME_LEN]; // plugin dll path and name

    bool bLABRC; // use look ahead bitrate control algorithm
    bool bENCPAK;
    bool bPREENC;
    bool bMBSize;
    msdk_char* mvinFile;
    msdk_char* mbctrinFile;
    msdk_char* mvoutFile;
    msdk_char* mbcodeoutFile;
    msdk_char* mbstatoutFile;
    msdk_char* mbQpFile;
    mfxU16 nLADepth; // depth of the look ahead bitrate control  algorithm
};

//for PreEnc reordering
struct iTask{
    mfxENCInput in;
    mfxENCOutput out;
    mfxU16 frameType;
    mfxU32 frameDisplayOrder;
    mfxSyncPoint EncSyncP;
    mfxI32 encoded;
    iTask* prevTask;
    iTask* nextTask;
};

struct sTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;
    std::list<mfxSyncPoint> DependentVppTasks;
    CSmplBitstreamWriter *pWriter;

    sTask();
    mfxStatus WriteBitstream();
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    virtual ~CEncTaskPool();

    virtual mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL);
    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxStatus SynchronizeFirstTask();
    virtual void Close();

protected:
    sTask* m_pTasks;
    mfxU32 m_nPoolSize;
    mfxU32 m_nTaskBufferStart;

    MFXVideoSession* m_pmfxSession;

    virtual mfxU32 GetFreeTaskIndex();
};

/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CEncodingPipeline
{
public:
    CEncodingPipeline();
    virtual ~CEncodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams);
    virtual mfxStatus Run();
    virtual void Close();
    virtual mfxStatus ResetMFXComponents(sInputParams* pParams);
    virtual mfxStatus ResetDevice();

    void SetMultiView();
    void SetNumView(mfxU32 numViews) { m_nNumView = numViews; }
    virtual void  PrintInfo();

protected:
    std::pair<CSmplBitstreamWriter *,
              CSmplBitstreamWriter *> m_FileWriters;
    CSmplYUVReader m_FileReader;
    CEncTaskPool m_TaskPool;
    mfxU16 m_nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
    mfxI32 m_refDist;
    mfxI32 m_gopSize;

    MFXVideoSession m_mfxSession;
    MFXVideoENCODE* m_pmfxENCPAK;
    MFXVideoENC* m_pmfxPREENC;
    MFXVideoVPP* m_pmfxVPP;

    mfxVideoParam m_mfxEncParams;
    mfxVideoParam m_mfxVppParams;
    sInputParams  m_encpakParams;

    mfxU16 m_MVCflags; // MVC codec is in use

    std::auto_ptr<MFXPlugin> m_pHEVC_plugin;
    std::auto_ptr<MFXVideoUSER>  m_pUserModule;
    //const msdkPluginUID*     m_pUID;

    MFXFrameAllocator* m_pMFXAllocator;
    mfxAllocatorParams* m_pmfxAllocatorParams;
    MemType m_memType;
    bool m_bExternalAlloc; // use memory allocator as external for Media SDK

    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameSurface1* m_pVppSurfaces; // frames array for vpp input
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocResponse m_VppResponse;  // memory allocation response for vpp

    mfxU32 m_nNumView;

    // for disabling VPP algorithms
    mfxExtVPPDoNotUse m_VppDoNotUse;
    // for MVC encoder and VPP configuration
    mfxExtCodingOption m_CodingOption;
    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;
    mfxExtFeiParam  m_encpakInit;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_VppExtParams;
    std::vector<mfxExtBuffer*> m_EncExtParams;

    std::list<iTask*> inputTasks; //used in PreENC

    CHWDevice *m_hwdev;

    virtual mfxStatus InitMfxEncParams(sInputParams *pParams);
    virtual mfxStatus InitMfxVppParams(sInputParams *pParams);

    virtual mfxStatus InitFileWriters(sInputParams *pParams);
    virtual void FreeFileWriters();
    virtual mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    virtual mfxStatus AllocAndInitVppDoNotUse();
    virtual void FreeVppDoNotUse();

    virtual mfxStatus CreateAllocator();
    virtual void DeleteAllocator();

    virtual mfxStatus CreateHWDevice();
    virtual void DeleteHWDevice();

    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();

    virtual mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    virtual mfxStatus GetFreeTask(sTask **ppTask);
    virtual mfxU16 GetFrameType(mfxU32 pos);

    virtual mfxStatus SynchronizeFirstTask();

    iTask* findFrameToEncode();
    void initFrameParams(iTask* eTask);

    mfxEncodeCtrl* ctr;

    //FEI buffers
    mfxEncodeCtrl feiCtrl;
    mfxExtFeiEncFrameCtrl feiEncCtrl[2];
    mfxExtFeiEncMVPredictors feiEncMVPredictors[2];
    mfxExtFeiEncMBCtrl feiEncMBCtrl[2];
    mfxExtFeiEncMBStat feiEncMbStat[2];
    mfxExtFeiEncMV feiEncMV[2];
    mfxExtFeiEncQP feiEncMbQp[2];
    mfxExtFeiPakMBCtrl feiEncMBCode[2];

    int numExtInParams;
    int numExtOutParams;
    mfxExtBuffer * inBufs[4];
    mfxExtBuffer * outBufs[4];

    int numExtOutParamsPreEnc;
    int numExtInParamsPreEnc;
    mfxExtBuffer * inBufsPreEnc[4];
    mfxExtBuffer * outBufsPreEnc[2];
    mfxExtBuffer * outBufsPreEncI[2];

    mfxExtFeiPreEncCtrl preENCCtr[2];
    mfxExtFeiPreEncMVPredictors mvPreds[2];
    mfxExtFeiEncQP qps[2];
    mfxExtFeiPreEncMV mvs[2];
    mfxExtFeiPreEncMBStat mbdata[2];

    bool disableMVoutPreENC;
    bool disableMBStatPreENC;
};

#endif // __PIPELINE_ENCODE_H__
