//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2006-2016 Intel Corporation. All Rights Reserved.
//

#ifndef __UMC_VA_LINUX_H__
#define __UMC_VA_LINUX_H__

#include "umc_va_base.h"

#ifdef UMC_VA_LINUX

#include "umc_mutex.h"
#include "umc_event.h"

namespace UMC
{

#define UMC_VA_LINUX_INDEX_UNDEF -1

/* VACompBuffer --------------------------------------------------------------*/

class VACompBuffer : public UMCVACompBuffer
{
public:
    // constructor
    VACompBuffer(void);
    // destructor
    virtual ~VACompBuffer(void);

    // UMCVACompBuffer methods
    virtual void SetNumOfItem(Ipp32s num) { m_NumOfItem = num; };

    // VACompBuffer methods
    virtual Status SetBufferInfo   (Ipp32s _type, Ipp32s _id, Ipp32s _index = -1);
    virtual Status SetDestroyStatus(bool _destroy);

    virtual Ipp32s GetIndex(void)    { return m_index; }
    virtual Ipp32s GetID(void)       { return m_id; }
    virtual Ipp32s GetNumOfItem(void){ return m_NumOfItem; }
    virtual bool   NeedDestroy(void) { return m_bDestroy; }

protected:
    Ipp32s m_NumOfItem; //number of items in buffer
    Ipp32s m_index;
    Ipp32s m_id;
    bool   m_bDestroy;
};

/* LinuxVideoAcceleratorParams -----------------------------------------------*/

class LinuxVideoAcceleratorParams : public VideoAcceleratorParams
{
    DYNAMIC_CAST_DECL(LinuxVideoAcceleratorParams, VideoAcceleratorParams);

public:

    LinuxVideoAcceleratorParams(void)
    {
        m_Display            = NULL;
        m_bComputeVAFncsInfo = false;
        m_pConfigId          = NULL;
        m_pContext           = NULL;
        m_pKeepVAState       = NULL;
        m_CreateFlags        = VA_PROGRESSIVE;
    }

    VADisplay     m_Display;
    bool          m_bComputeVAFncsInfo;
    VAConfigID*   m_pConfigId;
    VAContextID*  m_pContext;
    bool*         m_pKeepVAState;
    int           m_CreateFlags;
};

/* LinuxVideoAccelerator -----------------------------------------------------*/

enum lvaFrameState
{
    lvaBeforeBegin = 0,
    lvaBeforeEnd   = 1,
    lvaNeedUnmap   = 2
};

class LinuxVideoAccelerator : public VideoAccelerator
{
    DYNAMIC_CAST_DECL(LinuxVideoAccelerator, VideoAccelerator);
public:
    // constructor
    LinuxVideoAccelerator (void);
    // destructor
    virtual ~LinuxVideoAccelerator(void);

    // VideoAccelerator methods
    virtual Status Init         (VideoAcceleratorParams* pInfo);
    virtual Status Close        (void);
    virtual Status BeginFrame   (Ipp32s FrameBufIndex);
    // gets buffer from cache if it exists or HW otherwise, buffers will be released in EndFrame
    virtual void* GetCompBuffer(Ipp32s buffer_type, UMCVACompBuffer **buf, Ipp32s size, Ipp32s index);
    virtual Status Execute      (void);
    virtual Status EndFrame     (void*);
    virtual Ipp32s GetSurfaceID (Ipp32s idx);

    // NOT implemented functions:
    virtual Status ReleaseBuffer(Ipp32s /*type*/)
    { return UMC_OK; };

    virtual Status ExecuteExtensionBuffer(void* /*x*/) { return UMC_ERR_UNSUPPORTED;}
    virtual Status ExecuteStatusReportBuffer(void* /*x*/, Ipp32s /*y*/)  { return UMC_ERR_UNSUPPORTED;}
    virtual Status SyncTask(Ipp32s index, void * error = NULL);
    virtual Status QueryTaskStatus(Ipp32s index, void * status, void * error);
    virtual bool IsIntelCustomGUID() const { return false;}
    virtual GUID GetDecoderGuid(){return m_guidDecoder;};
    virtual void GetVideoDecoder(void** /*handle*/) {};

protected:
    // VideoAcceleratorExt methods
    virtual Status AllocCompBuffers(void);
    virtual VACompBuffer* GetCompBufferHW(Ipp32s type, Ipp32s size, Ipp32s index = -1);

    // LinuxVideoAccelerator methods
    Ipp16u GetDecodingError();

    void SetTraceStrings(Ipp32u umc_codec);

protected:
    VADisplay     m_dpy;
    VAConfigID*   m_pConfigId;
    VAContextID*  m_pContext;
    bool*         m_pKeepVAState;
    lvaFrameState m_FrameState;

    Ipp32s   m_NumOfFrameBuffers;
    Ipp32u   m_uiCompBuffersNum;
    Ipp32u   m_uiCompBuffersUsed;
    vm_mutex m_SyncMutex;
    VACompBuffer** m_pCompBuffers;

    const char * m_sDecodeTraceStart;
    const char * m_sDecodeTraceEnd;

    GUID m_guidDecoder;
};

}; // namespace UMC

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

    extern UMC::Status va_to_umc_res(VAStatus va_res);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // #ifdef UMC_VA_LINUX

#endif // #ifndef __UMC_VA_LINUX_H__
