//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010 - 2015 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef __SAMPLE_PIPELINE_TRANSCODE_H__
#define __SAMPLE_PIPELINE_TRANSCODE_H__

#include <stddef.h>

#include "mfxplugin.h"
#include "mfxplugin++.h"

#include "vm/atomic_defs.h"
#include "vm/thread_defs.h"

#include <memory>
#include <vector>
#include <list>
#include <ctime>

#include "sample_defs.h"
#include "sample_utils.h"
#include "sample_params.h"
#include "base_allocator.h"
#include "rotate_plugin_api.h"
#include "mfx_multi_vpp.h"

#include "mfxvideo.h"
#include "mfxvideo++.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxla.h"
#include "mfxvp8.h"

#include "hw_device.h"
#include "plugin_loader.h"

#include "vpp_ext_buffers_storage.h"

#if defined(_WIN32) || defined(_WIN64)
#include "decode_render.h"
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define MSDK_CPU_ROTATE_PLUGIN  MSDK_STRING("sample_rotate_plugin.dll")
    #define MSDK_OCL_ROTATE_PLUGIN  MSDK_STRING("sample_plugin_opencl.dll")
#else
    #define MSDK_CPU_ROTATE_PLUGIN  MSDK_STRING("libsample_rotate_plugin.so")
    #define MSDK_OCL_ROTATE_PLUGIN  MSDK_STRING("libsample_plugin_opencl.so")
#endif

namespace TranscodingSample
{
    extern mfxU32 MFX_STDCALL ThranscodeRoutine(void   *pObj);

    enum PipelineMode
    {
        Native = 0,        // means that pipeline is based depends on the cmd parameters (decode/encode/transcode)
        Sink,              // means that pipeline makes decode only and put data to shared buffer
        Source,            // means that pipeline makes vpp + encode and get data from shared buffer
        VppComp,           // means that pipeline makes vpp composition + encode and get data from shared buffer
        VppCompOnly        // means that pipeline makes vpp composition and get data from shared buffer
    };

    struct sVppCompDstRect
    {
        mfxU32 DstX;
        mfxU32 DstY;
        mfxU32 DstW;
        mfxU32 DstH;
    };

    struct __sInputParams
    {
        // session parameters
        bool         bIsJoin;
        mfxPriority  priority;
        // common parameters
        mfxIMPL libType;  // Type of used mediaSDK library
        bool   bIsPerf;   // special performance mode. Use pre-allocated bitstreams, output

        mfxU32 EncodeId; // type of output coded video
        mfxU32 DecodeId; // type of input coded video

        msdk_char  strSrcFile[MSDK_MAX_FILENAME_LEN]; // source bitstream file
        msdk_char  strDstFile[MSDK_MAX_FILENAME_LEN]; // destination bitstream file

        // specific encode parameters
        mfxU16 nTargetUsage;
        mfxF64 dFrameRate;
        mfxF64 dEncoderFrameRate;
        mfxU32 nBitRate;
        mfxU16 nQuality; // quality parameter for JPEG encoder
        mfxU16 nDstWidth;  // destination picture width, specified if resizing required
        mfxU16 nDstHeight; // destination picture height, specified if resizing required

        bool bEnableDeinterlacing;
        mfxU16 DeinterlacingMode;
        int DenoiseLevel;
        int DetailLevel;
        mfxU16 FRCAlgorithm;

        mfxU16 nAsyncDepth; // asyncronous queue

        PipelineMode eMode;
        PipelineMode eModeExt;

        mfxU32 FrameNumberPreference; // how many surfaces user wants
        mfxU32 MaxFrameNumber; // maximum frames for transcoding
        mfxU32 numSurf4Comp;

        mfxU16 nSlices; // number of slices for encoder initialization
        mfxU16 nMaxSliceSize; //maximum size of slice

        // MVC Specific Options
        bool   bIsMVC; // true if Multi-View-Codec is in use
        mfxU32 numViews; // number of views for Multi-View-Codec

        mfxU16 nRotationAngle; // if specified, enables rotation plugin in mfx pipeline
        msdk_char strVPPPluginDLLPath[MSDK_MAX_FILENAME_LEN]; // plugin dll path and name

        sPluginParams decoderPluginParams;
        sPluginParams encoderPluginParams;

        mfxU32 nTimeout; // how long transcoding works in seconds
        mfxU32 nFPS; // limit transcoding to the number of frames per second

        bool bLABRC; // use look ahead bitrate control algorithm
        mfxU16 nLADepth; // depth of the look ahead bitrate control  algorithm
        bool bEnableExtLA;
        bool bEnableBPyramid;

        bool bOpenCL;

        mfxU16 nVppCompDstX;
        mfxU16 nVppCompDstY;
        mfxU16 nVppCompDstW;
        mfxU16 nVppCompDstH;

        mfxU32 DecoderFourCC;
        mfxU32 EncoderFourCC;

        sVppCompDstRect* pVppCompDstRects;

        bool bUseOpaqueMemory;
        mfxU16 nRenderColorForamt; /*0 NV12 - default, 1 is ARGB*/
        CHWDevice             *m_hwdev;
    };

    struct sInputParams: public __sInputParams
    {
        sInputParams();
        void Reset();
    };

    struct PreEncAuxBuffer
    {
       mfxEncodeCtrl     encCtrl;
       mfxU16            Locked;
       mfxENCInput       encInput;
       mfxENCOutput      encOutput;
    };

    struct ExtendedSurface
    {
        mfxFrameSurface1 *pSurface;
        PreEncAuxBuffer  *pCtrl;
        mfxSyncPoint      Syncp;
    };

    struct ExtendedBS
    {
        ExtendedBS(): IsFree(true), Syncp(NULL), pCtrl(NULL)
        {
            MSDK_ZERO_MEMORY(Bitstream);
        };
        bool IsFree;
        mfxBitstream Bitstream;
        mfxSyncPoint Syncp;
        PreEncAuxBuffer* pCtrl;
    };

    class ExtendedBSStore
    {
    public:
        explicit ExtendedBSStore(mfxU32 size)
        {
            m_pExtBS.resize(size);
        };
        virtual ~ExtendedBSStore()
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
                MSDK_SAFE_DELETE_ARRAY(m_pExtBS[i].Bitstream.Data);
            m_pExtBS.clear();

        };
        ExtendedBS* GetNext()
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
            {
                if (m_pExtBS[i].IsFree)
                {
                    m_pExtBS[i].IsFree = false;
                    return &m_pExtBS[i];
                }
            }
            return NULL;
        };
        void Release(ExtendedBS* pBS)
        {
            for (mfxU32 i=0; i < m_pExtBS.size(); i++)
            {
                if (&m_pExtBS[i] == pBS)
                {
                    m_pExtBS[i].IsFree = true;
                    return;
                }
            }
            return;
        };
    protected:
        std::vector<ExtendedBS> m_pExtBS;

    private:
        DISALLOW_COPY_AND_ASSIGN(ExtendedBSStore);
    };

    class CTranscodingPipeline;
    // thread safety buffer heterogeneous pipeline
    // only for join sessions
    class SafetySurfaceBuffer
    {
    public:
        struct SurfaceDescriptor
        {
            ExtendedSurface   ExtSurface;
            mfxU32            Locked;
        };

        SafetySurfaceBuffer(SafetySurfaceBuffer *pNext);
        virtual ~SafetySurfaceBuffer();

        void              AddSurface(ExtendedSurface Surf);
        mfxStatus         GetSurface(ExtendedSurface &Surf);
        mfxStatus         ReleaseSurface(mfxFrameSurface1* pSurf);

        SafetySurfaceBuffer               *m_pNext;

    protected:

        MSDKMutex                 m_mutex;
        std::list<SurfaceDescriptor>       m_SList;
    private:
        DISALLOW_COPY_AND_ASSIGN(SafetySurfaceBuffer);
    };

    // External bitstream processing support
    class BitstreamProcessor
    {
    public:
        BitstreamProcessor() {};
        virtual ~BitstreamProcessor() {};
        virtual mfxStatus PrepareBitstream() = 0;
        virtual mfxStatus GetInputBitstream(mfxBitstream **pBitstream) = 0;
        virtual mfxStatus ProcessOutputBitstream(mfxBitstream* pBitstream) = 0;
    };

    class FileBitstreamProcessor : public BitstreamProcessor
    {
    public:
        FileBitstreamProcessor();
        virtual ~FileBitstreamProcessor();
        virtual mfxStatus Init(msdk_char  *pStrSrcFile, msdk_char  *pStrDstFile);
        virtual mfxStatus PrepareBitstream() {return MFX_ERR_NONE;}
        virtual mfxStatus GetInputBitstream(mfxBitstream **pBitstream);
        virtual mfxStatus ProcessOutputBitstream(mfxBitstream* pBitstream);

    protected:
        std::auto_ptr<CSmplBitstreamReader> m_pFileReader;
        // for performance options can be zero
        std::auto_ptr<CSmplBitstreamWriter> m_pFileWriter;
        mfxBitstream m_Bitstream;
    private:
        DISALLOW_COPY_AND_ASSIGN(FileBitstreamProcessor);
    };

    // Bitstream processing with reset input and output files support
    class FileBitstreamProcessor_WithReset : public FileBitstreamProcessor
    {
    public:
        virtual mfxStatus Init(msdk_char *pStrSrcFile, msdk_char *pStrDstFile);
        virtual mfxStatus ResetInput();
        virtual mfxStatus ResetOutput();
    protected:
        std::vector<msdk_char> m_pSrcFile;
        std::vector<msdk_char> m_pDstFile;
    };

    // Bitstream is external via BitstreamProcessor
    class CTranscodingPipeline
    {
    public:
        CTranscodingPipeline();
        virtual ~CTranscodingPipeline();

        virtual mfxStatus Init(sInputParams *pParams,
                               MFXFrameAllocator *pMFXAllocator,
                               void* hdl,
                               CTranscodingPipeline *pParentPipeline,
                               SafetySurfaceBuffer  *pBuffer,
                               BitstreamProcessor   *pBSProc);

        // frames allocation is suspended for heterogeneous pipeline
        virtual mfxStatus CompleteInit();
        virtual void      Close();
        virtual mfxStatus Join(MFXVideoSession *pChildSession);
        virtual mfxStatus Run();
        virtual mfxStatus FlushLastFrames(){return MFX_ERR_NONE;}

        mfxU32 GetProcessFrames() {return m_nProcessedFramesNum;}

        bool   GetJoiningFlag() {return m_bIsJoinSession;}

        mfxStatus QueryMFXVersion(mfxVersion *version)
        { MSDK_CHECK_POINTER(m_pmfxSession.get(), MFX_ERR_NULL_PTR); return m_pmfxSession->QueryVersion(version); };

    protected:
        virtual mfxStatus CheckRequiredAPIVersion(mfxVersion& version, sInputParams *pParams);
        virtual mfxStatus CheckExternalBSProcessor(BitstreamProcessor   *pBSProc);

        virtual mfxStatus Decode();
        virtual mfxStatus Encode();
        virtual mfxStatus Transcode();
        virtual mfxStatus DecodeOneFrame(ExtendedSurface *pExtSurface);
        virtual mfxStatus DecodeLastFrame(ExtendedSurface *pExtSurface);
        virtual mfxStatus VPPOneFrame(ExtendedSurface *pSurfaceIn, ExtendedSurface *pExtSurface);
        virtual mfxStatus EncodeOneFrame(ExtendedSurface *pExtSurface, mfxBitstream *pBS);
        virtual mfxStatus PreEncOneFrame(ExtendedSurface *pInSurface, ExtendedSurface *pOutSurface);

        virtual mfxStatus DecodePreInit(sInputParams *pParams);
        virtual mfxStatus VPPPreInit(sInputParams *pParams);
        virtual mfxStatus EncodePreInit(sInputParams *pParams);
        virtual mfxStatus PreEncPreInit(sInputParams *pParams);

        mfxVideoParam GetDecodeParam();
        mfxExtOpaqueSurfaceAlloc GetDecOpaqueAlloc() const { return m_pmfxVPP.get() ? m_VppOpaqueAlloc: m_DecOpaqueAlloc; };

        mfxExtMVCSeqDesc GetDecMVCSeqDesc() const {return m_MVCSeqDesc;};

        // alloc frames for all component
        mfxStatus AllocFrames(mfxFrameAllocRequest  *pRequest, bool isDecAlloc);
        mfxStatus AllocFrames();

        mfxStatus CorrectPreEncAuxPool(mfxU32 num_frames_in_pool);
        mfxStatus AllocPreEncAuxPool();

        // need for heterogeneous pipeline
        mfxStatus CalculateNumberOfReqFrames(mfxFrameAllocRequest  &pRequestDecOut, mfxFrameAllocRequest  &pRequestVPPOut);
        void      CorrectNumberOfAllocatedFrames(mfxFrameAllocRequest  *pRequest);
        void      FreeFrames();

        void      FreePreEncAuxPool();

        mfxFrameSurface1* GetFreeSurface(bool isDec);
        PreEncAuxBuffer*  GetFreePreEncAuxBuffer();

        // parameters configuration functions
        mfxStatus InitDecMfxParams(sInputParams *pInParams);
        mfxStatus InitVppMfxParams(sInputParams *pInParams);
        mfxStatus InitEncMfxParams(sInputParams *pInParams);
        mfxStatus InitPluginMfxParams(sInputParams *pInParams);
        mfxStatus InitPreEncMfxParams(sInputParams *pInParams);

        mfxStatus AllocAndInitVppDoNotUse(sInputParams *pInParams);
        mfxStatus AllocMVCSeqDesc();
        mfxStatus InitOpaqueAllocBuffers();

        void FreeVppDoNotUse();
        void FreeMVCSeqDesc();

        mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
        mfxStatus PutBS();

        void NoMoreFramesSignal(ExtendedSurface &DecExtSurface);
        mfxStatus AddLaStreams(mfxU16 width, mfxU16 height);

        void LockPreEncAuxBuffer(PreEncAuxBuffer* pBuff);
        void UnPreEncAuxBuffer  (PreEncAuxBuffer* pBuff);


        mfxBitstream        *m_pmfxBS;  // contains encoded input data

        mfxVersion m_Version; // real API version with which library is initialized

        std::auto_ptr<MFXVideoSession>  m_pmfxSession;
        std::auto_ptr<MFXVideoDECODE>   m_pmfxDEC;
        std::auto_ptr<MFXVideoENCODE>   m_pmfxENC;
        std::auto_ptr<MFXVideoMultiVPP> m_pmfxVPP; // either VPP or VPPPlugin which wraps [VPP]-Plugin-[VPP] pipeline
        std::auto_ptr<MFXVideoENC>      m_pmfxPreENC;
        std::auto_ptr<MFXVideoUSER>     m_pUserDecoderModule;
        std::auto_ptr<MFXVideoUSER>     m_pUserEncoderModule;
        std::auto_ptr<MFXVideoUSER>     m_pUserEncModule;
        std::auto_ptr<MFXPlugin>        m_pUserDecoderPlugin;
        std::auto_ptr<MFXPlugin>        m_pUserEncoderPlugin;
        std::auto_ptr<MFXPlugin>        m_pUserEncPlugin;

        mfxFrameAllocResponse           m_mfxDecResponse;  // memory allocation response for decoder
        mfxFrameAllocResponse           m_mfxEncResponse;  // memory allocation response for encoder

        MFXFrameAllocator              *m_pMFXAllocator;
        void*                           m_hdl; // Diret3D device manager

        mfxU32                          m_numEncoders;

#if defined(_WIN32) || defined(_WIN64)
        CDecodeD3DRender*               m_hwdev4Rendering;
#else
        CHWDevice*                      m_hwdev4Rendering;
#endif

        typedef std::vector<mfxFrameSurface1*> SurfPointersArray;
        SurfPointersArray  m_pSurfaceDecPool;
        SurfPointersArray  m_pSurfaceEncPool;
        mfxU16 m_EncSurfaceType; // actual type of encoder surface pool
        mfxU16 m_DecSurfaceType; // actual type of decoder surface pool

        typedef std::vector<PreEncAuxBuffer>    PreEncAuxArray;
        PreEncAuxArray                          m_pPreEncAuxPool;

        // transcoding pipeline specific
        typedef std::list<ExtendedBS*>       BSList;
        BSList  m_BSPool;

        mfxVideoParam                  m_mfxDecParams;
        mfxVideoParam                  m_mfxEncParams;
        mfxVideoParam                  m_mfxVppParams;
        mfxVideoParam                  m_mfxPluginParams;
        bool                           m_bIsVpp; // true if there's VPP in the pipeline
        bool                           m_bIsPlugin; //true if there's Plugin in the pipeline
        RotateParam                    m_RotateParam;
        mfxVideoParam                  m_mfxPreEncParams;
        mfxU32                         m_nTimeout;
        // various external buffers
        // for disabling VPP algorithms
        mfxExtVPPDoNotUse m_VppDoNotUse;
        // for MVC decoder and encoder configuration
        mfxExtMVCSeqDesc m_MVCSeqDesc;
        bool m_bOwnMVCSeqDescMemory; // true if the pipeline owns memory allocated for MVCSeqDesc structure fields

        mfxExtVPPComposite       m_VppCompParams;

        mfxExtLAControl          m_ExtLAControl;
        // for setting MaxSliceSize
        mfxExtCodingOption2      m_CodingOption2;

        // HEVC
        mfxExtHEVCParam          m_ExtHEVCParam;

        // for opaque memory
        mfxExtOpaqueSurfaceAlloc m_EncOpaqueAlloc;
        mfxExtOpaqueSurfaceAlloc m_VppOpaqueAlloc;
        mfxExtOpaqueSurfaceAlloc m_DecOpaqueAlloc;
        mfxExtOpaqueSurfaceAlloc m_PluginOpaqueAlloc;
        mfxExtOpaqueSurfaceAlloc m_PreEncOpaqueAlloc;

        // external parameters for each component are stored in a vector
        CVPPExtBuffersStorage        m_VppExtParamsStorage;
        std::vector<mfxExtBuffer*> m_EncExtParams;
        std::vector<mfxExtBuffer*> m_DecExtParams;
        std::vector<mfxExtBuffer*> m_PluginExtParams;
        std::vector<mfxExtBuffer*> m_PreEncExtParams;

        mfxU16         m_AsyncDepth;
        mfxU32         m_nProcessedFramesNum;

        bool           m_bIsJoinSession;

        bool           m_bDecodeEnable;
        bool           m_bEncodeEnable;
        mfxU32         m_nVPPCompEnable;

        bool           m_bUseOpaqueMemory; // indicates if opaque memory is used in the pipeline

        mfxSyncPoint   m_LastDecSyncPoint;

        SafetySurfaceBuffer   *m_pBuffer;
        CTranscodingPipeline  *m_pParentPipeline;

        mfxFrameAllocRequest   m_Request;
        bool                   m_bIsInit;

        std::auto_ptr<ExtendedBSStore>        m_pBSStore;

        mfxU32                                m_FrameNumberPreference;
        mfxU32                                m_MaxFramesForTranscode;

        // pointer to already extended bs processor
        BitstreamProcessor                   *m_pBSProcessor;

        msdk_tick m_nReqFrameTime; // time required to transcode one frame

    private:
        DISALLOW_COPY_AND_ASSIGN(CTranscodingPipeline);

    };

    struct ThreadTranscodeContext
    {
        // Pointer to the session's pipeline
        std::auto_ptr<CTranscodingPipeline> pPipeline;
        // Pointer to bitstream handling object
        BitstreamProcessor *pBSProcessor;
        // Session implementation type
        mfxIMPL implType;

        // Session's starting status
        mfxStatus startStatus;
        // Session's working time
        mfxF64 working_time;

        // Number of processed frames
        mfxU32 numTransFrames;
        // Status of the finished session
        mfxStatus transcodingSts;
    };


}

#endif
