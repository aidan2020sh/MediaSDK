//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2011-2018 Intel Corporation. All Rights Reserved.
//

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)
#ifndef _D3D11_DECODE_ACCELERATOR_H_
#define _D3D11_DECODE_ACCELERATOR_H_

#define UMC_VA_DXVA

#include <atlbase.h>
#include <d3d11.h>


#include "umc_va_dxva2.h"
#include "mfxvideo.h"

class  D3D11VideoCORE;
class mfx_UMC_FrameAllocator;

class MFXD3D11Accelerator : public UMC::DXAccelerator
{

public:
    MFXD3D11Accelerator(ID3D11VideoDevice  *pVideoDevice,
                        ID3D11VideoContext *pVideoContext);

    virtual ~MFXD3D11Accelerator()
    {
        Close();
    };

    // I/F between core and accelerator
    UMC::Status Init(UMC::VideoAcceleratorParams*) override
    { return UMC::UMC_ERR_UNSUPPORTED; };

    // I/F between UMC decoders and accelerator
    UMC::Status BeginFrame(Ipp32s index) override;
    UMC::Status Execute() override;
    UMC::Status ExecuteExtensionBuffer(void * buffer) override;
    UMC::Status ExecuteStatusReportBuffer(void * buffer, Ipp32s size) override;
    UMC::Status QueryTaskStatus(Ipp32s , void *, void *) override
    { return UMC::UMC_ERR_UNSUPPORTED;}
    UMC::Status EndFrame(void * handle = 0) override;

    bool IsIntelCustomGUID() const override;

    HRESULT GetVideoDecoderDriverHandle(HANDLE *pDriverHandle) {return m_pDecoder->GetDriverHandle(pDriverHandle);};
    // Will use this function instead of previos two: FindConfiguration, Init
    mfxStatus CreateVideoAccelerator(mfxU32 hwProfile, const mfxVideoParam *param, UMC::FrameAllocator *allocator);

    void GetVideoDecoder(void **handle)
    {
        *handle = m_pDecoder;
    };

    virtual UMC::Status Close();

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC_DECODE
protected:

    UMC::Status RegisterGpuEvent(GPU_SYNC_EVENT_HANDLE &ev) override;
#endif

private:

    mfxStatus GetSuitVideoDecoderConfig(const mfxVideoParam            *param,        //in
                                        D3D11_VIDEO_DECODER_DESC *video_desc,   //in
                                        D3D11_VIDEO_DECODER_CONFIG     *pConfig);     //out


    D3D11_VIDEO_DECODER_BUFFER_TYPE MapDXVAToD3D11BufType(const Ipp32s DXVABufType) const;


    UMC::Status GetCompBufferInternal(UMC::UMCVACompBuffer*) override;
    UMC::Status ReleaseBufferInternal(UMC::UMCVACompBuffer*) override;

private:

    ID3D11VideoDevice               *m_pVideoDevice;
    ID3D11VideoContext              *m_pVideoContext;

    // we own video decoder, let using com pointer
    CComPtr<ID3D11VideoDecoder>      m_pDecoder;

    // current decoder
    GUID                              m_DecoderGuid;

    ID3D11VideoDecoderOutputView *m_pVDOView;
};


#endif
#endif
#endif
