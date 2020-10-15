// Copyright (c) 2011-2019 Intel Corporation
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
#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)

#include <d3d11_1.h>
#include "d3d11_decode_accelerator.h"
#include "libmfx_core.h"
#include "mfx_utils.h"
#include "umc_vc1_dec_va_defs.h"

#include "umc_va_dxva2.h"
#include "umc_va_dxva2_protected.h"
#include "umc_va_video_processing.h"
#include "umc_dynamic_cast.h"

#include "libmfx_core_d3d11.h"
#include "mfx_umc_alloc_wrapper.h"
#include "mfx_common_decode_int.h"

#define DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER 21
#define DXVA2_VC1BITPLANE_EXT_BUFFER       22

static const GUID DXVADDI_Intel_ModeVC1_D =
{ 0xe07ec519, 0xe651, 0x4cd6, { 0xac, 0x84, 0x13, 0x70, 0xcc, 0xee, 0xc8, 0x51 } };
static const GUID DXVADDI_Intel_ModeVC1_D_Advanced =
{ 0xbcc5db6d, 0xa2b6, 0x4af0, { 0xac, 0xe4, 0xad, 0xb1, 0xf7, 0x87, 0xbc, 0x89 } };

using namespace UMC;

MFXD3D11Accelerator::MFXD3D11Accelerator()
    : m_pVideoDevice(nullptr)
    , m_pVideoContext(nullptr)
    , m_pVDOView(0)
    , m_DecoderGuid(GUID_NULL)
    , m_numberSurfaces(128)
{
}

UMC::Status MFXD3D11Accelerator::Init(UMC::VideoAcceleratorParams* params)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXD3D11Accelerator::Init");

    UMC_CHECK_PTR(params);

    UMC_CHECK(params->m_allocator, UMC_ERR_ALLOC);
    m_allocator = params->m_allocator;
    m_numberSurfaces = params->m_iNumberSurfaces;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (IS_PROTECTION_ANY(params->m_protectedVA))
    {
        m_protectedVA = new UMC::ProtectedVA((mfxU16)params->m_protectedVA);
    }
#endif

    auto dx_params = DynamicCast<MFXD3D11AcceleratorParams>(params);
    UMC_CHECK(dx_params, UMC_ERR_INVALID_PARAMS);

    UMC_CHECK(dx_params->m_video_device, UMC_ERR_INVALID_PARAMS);
    m_pVideoDevice = dx_params->m_video_device;

    UMC_CHECK(dx_params->m_video_context, UMC_ERR_INVALID_PARAMS);
    m_pVideoContext = dx_params->m_video_context;

    UMC_CHECK(params->m_pVideoStreamInfo, UMC_ERR_INVALID_PARAMS);
    auto vi = params->m_pVideoStreamInfo;

    D3D11_VIDEO_DECODER_DESC video_desc{};
    video_desc.SampleWidth  = vi->clip_info.width;
    video_desc.SampleHeight = vi->clip_info.height;
    video_desc.OutputFormat = mfxDefaultAllocatorD3D11::MFXtoDXGI(ConvertUMCColorFormatToFOURCC(vi->color_format));

    D3D11_VIDEO_DECODER_CONFIG video_config{};
    auto sts = GetSuitVideoDecoderConfig(&video_desc, &video_config);
    UMC_CHECK_STATUS(sts);

    m_DecoderGuid = video_desc.Guid;
    HRESULT hres;

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (dx_params->m_video_processing)
        video_config.ConfigDecoderSpecific = video_config.ConfigDecoderSpecific | DXVA_DECODE_CONFIG_DOWNSAMPLING_MASK;
#endif

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
        hres  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
        UMC_CHECK(SUCCEEDED(hres), UMC_ERR_FAILED);
    }

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    if (dx_params->m_video_processing)
    {
        // SFC is implemented for AVC,HEVC,VP9 and AV1
        UMC_CHECK(vi->stream_type == UMC::H264_VIDEO ||
                  vi->stream_type == UMC::VP9_VIDEO  ||
                  vi->stream_type == UMC::HEVC_VIDEO ||
                  vi->stream_type == UMC::AV1_VIDEO , UMC_ERR_UNSUPPORTED);

        CComPtr<ID3D11Device1>       device1;
        hres = m_pVideoDevice->QueryInterface(&device1);
        UMC_CHECK(SUCCEEDED(hres), UMC_ERR_FAILED);

        DXGI_COLOR_SPACE_TYPE inputCSC;
        inputCSC = DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;

        D3D11_VIDEO_SAMPLE_DESC outputDesc{};
        outputDesc.Width  = dx_params->m_video_processing->Out.Width;
        outputDesc.Height = dx_params->m_video_processing->Out.Height;
        outputDesc.Format = mfxDefaultAllocatorD3D11::MFXtoDXGI(dx_params->m_video_processing->Out.FourCC);
        outputDesc.ColorSpace = inputCSC;

        CComQIPtr<ID3D11VideoContext1> ctx1(m_pVideoContext);
        MFX_CHECK(ctx1, MFX_ERR_DEVICE_FAILED);
        hres = ctx1->DecoderEnableDownsampling(m_pDecoder, inputCSC, &outputDesc, m_numberSurfaces);
        UMC_CHECK(SUCCEEDED(hres), UMC_ERR_FAILED);
    }
#endif

    m_bH264MVCSupport = GuidProfile::IsMVCGUID(m_DecoderGuid);
    m_isUseStatuReport = true;
    m_bH264ShortSlice = GuidProfile::isShortFormat((m_Profile & 0xf) == VA_H265, video_config.ConfigBitstreamRaw);
    m_H265ScalingListScanOrder = (video_config.Config4GroupedCoefs & 0x80000000) ? 0 : 1;

    return UMC_OK;

} // mfxStatus MFXD3D11Accelerator::CreateVideoAccelerator

UMC::Status MFXD3D11Accelerator::GetSuitVideoDecoderConfig(D3D11_VIDEO_DECODER_DESC* video_desc, D3D11_VIDEO_DECODER_CONFIG* pConfig)
{
    UMC_CHECK_PTR(video_desc);
    UMC_CHECK_PTR(pConfig);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXD3D11Accelerator::GetSuitVideoDecoderConfig");

    for (mfxU32 k = 0; k < GuidProfile::GetGuidProfileSize(); k++)
    {
        if ((m_Profile & (VA_ENTRY_POINT | VA_CODEC)) != (GuidProfile::GetGuidProfile(k)->profile  & (VA_ENTRY_POINT | VA_CODEC)))
            continue;

#ifndef OPEN_SOURCE
    {
        if ((m_Profile & VA_PRIVATE_DDI_MODE) &&
            !GuidProfile::GetGuidProfile(k)->IsIntelCustomGUID())
            continue;
    }
#endif

        video_desc->Guid = GuidProfile::GetGuidProfile(k)->guid;

        mfxU32  count;
        HRESULT hr;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
            hr = m_pVideoDevice->GetVideoDecoderConfigCount(video_desc, &count);
        }
        if (FAILED(hr)) // guid can be absent. It is ok
            continue;

        bool isHEVCGUID = (m_Profile & 0xf) == VA_H265;

        mfxI32 idxConfig = -1;
        for (mfxU32 i = 0; i < count; i++)
        {
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfig");
                hr = m_pVideoDevice->GetVideoDecoderConfig(video_desc, i, pConfig);
            }
            if (FAILED(hr))
                continue;

            if (CheckDXVAConfig<D3D11_VIDEO_DECODER_CONFIG>(m_Profile, pConfig, GetProtectedVA()) == false)
                continue;

            if (idxConfig == -1)
                idxConfig = i;

            bool isShort = GuidProfile::isShortFormat(isHEVCGUID, pConfig->ConfigBitstreamRaw);
            if (GuidProfile::GetGuidProfile(k)->profile & VA_LONG_SLICE_MODE)
            {
                if (!isShort)
                {
                    idxConfig = i;
                }
            }
            else
            {
                if (isShort)
                    idxConfig = i;
            }
        }

        if (idxConfig != -1)
        {
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfig");
                hr = m_pVideoDevice->GetVideoDecoderConfig(video_desc, idxConfig, pConfig);
            }
            UMC_CHECK(SUCCEEDED(hr), UMC_ERR_FAILED);

            return UMC_OK;
        }
    }

    return UMC_OK;

} //mfxStatus MFXD3D11Accelerator::GetSuitVideoDecoderConfig


D3D11_VIDEO_DECODER_BUFFER_TYPE MFXD3D11Accelerator::MapDXVAToD3D11BufType(const Ipp32s DXVABufType) const
{
    switch(DXVABufType)
    {
    case DXVA_MACROBLOCK_CONTROL_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_MACROBLOCK_CONTROL;
    case DXVA_RESIDUAL_DIFFERENCE_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_RESIDUAL_DIFFERENCE;
    case DXVA_INVERSE_QUANTIZATION_MATRIX_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_INVERSE_QUANTIZATION_MATRIX;
    case DXVA_SLICE_CONTROL_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_SLICE_CONTROL;
    case DXVA_BITSTREAM_DATA_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_BITSTREAM;
    case DXVA_PICTURE_DECODE_BUFFER:
        return D3D11_VIDEO_DECODER_BUFFER_PICTURE_PARAMETERS;
    case DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER:
        return (D3D11_VIDEO_DECODER_BUFFER_TYPE)(DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER - 1);
    case DXVA2_VC1BITPLANE_EXT_BUFFER:
        return (D3D11_VIDEO_DECODER_BUFFER_TYPE)(DXVA2_VC1BITPLANE_EXT_BUFFER - 1);
    }

    int const codec = m_Profile & VA_CODEC;
    switch (codec)
    {
    case VA_JPEG:
        switch (DXVABufType)
        {
        case D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA:
        case D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA:
        case D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA:
        case D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA:    return (D3D11_VIDEO_DECODER_BUFFER_TYPE)(DXVABufType - 1);
        }
        break;

    case VA_H265:
        switch (DXVABufType)
        {
        case D3DDDIFMT_INTEL_HEVC_SUBSET:            return (D3D11_VIDEO_DECODER_BUFFER_TYPE)(DXVABufType - 1);
        }
        break;
    }

    return (D3D11_VIDEO_DECODER_BUFFER_TYPE)-1;
} // D3D11_VIDEO_DECODER_BUFFER_TYPE MFXD3D11Accelerator::MapDXVAToD3D11BufType(const Ipp32s DXDVABufType)

Status  MFXD3D11Accelerator::BeginFrame(Ipp32s index)
{
    HRESULT hr = S_OK;
    mfxHDLPair Pair;

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "MFXD3D11Accelerator::BeginFrame");
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "MFXD3D11Accelerator::m_allocator->GetFrameHandle");
        if (UMC_OK != m_allocator->GetFrameHandle(index, &Pair))
            return UMC_ERR_DEVICE_FAILED;
    }
    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC OutputDesc;
    OutputDesc.DecodeProfile = m_DecoderGuid;
    OutputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    OutputDesc.Texture2D.ArraySlice = (UINT)(size_t)Pair.second;
    {

        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "m_pVDOView.Release()");
        m_pVDOView.Release();
    }

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoderOutputView");
        hr = m_pVideoDevice->CreateVideoDecoderOutputView((ID3D11Resource *)Pair.first,
            &OutputDesc,
            &m_pVDOView);
    }

    if( SUCCEEDED( hr ) )
    {
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderBeginFrame");
            hr = m_pVideoContext->DecoderBeginFrame(m_pDecoder, m_pVDOView, 0, NULL);
        }
        if SUCCEEDED( hr )
            return UMC_OK;
    }
    
    return UMC_ERR_DEVICE_FAILED;

} // mfxStatus  MFXD3D11Accelerator::BeginFrame(Ipp32s index)

Status MFXD3D11Accelerator::EndFrame(void *handle)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXD3D11Accelerator::EndFrame");
    std::for_each(std::begin(m_bufferOrder), std::end(m_bufferOrder),
        [this](Ipp32s type)
        { ReleaseBuffer(type);  }
    );

    m_bufferOrder.clear();

    HRESULT hr;
    handle;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
        hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
    }
    if (SUCCEEDED(hr))
    {
        //Sleep(100);
        return UMC_OK;
    }

     return UMC_ERR_DEVICE_FAILED;

} //Status MFXD3D11Accelerator::EndFrame(void *handle)

Status MFXD3D11Accelerator::GetCompBufferInternal(UMCVACompBuffer* buffer)
{
    VM_ASSERT(buffer);

    Ipp32s const type = buffer->GetType();

    void* data = NULL;
    UINT buffer_size = 0;
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "GetCompBufferInternal");
    HRESULT hr = m_pVideoContext->GetDecoderBuffer(m_pDecoder, MapDXVAToD3D11BufType(type), &buffer_size,  &data);
    if (FAILED(hr))
    {
        vm_trace_x(hr);
        VM_ASSERT(SUCCEEDED(hr));

        return UMC_ERR_FAILED;
    }

    buffer->SetBufferPointer(reinterpret_cast<Ipp8u*>(data), buffer_size);
    buffer->SetDataSize(0);

    return UMC_OK;
}

Status MFXD3D11Accelerator::ReleaseBufferInternal(UMCVACompBuffer* buffer)
{
    VM_ASSERT(buffer);

    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "ReleaseDecoderBuffer");
    Ipp32s const type = buffer->GetType();
    HRESULT hr = m_pVideoContext->ReleaseDecoderBuffer(m_pDecoder, MapDXVAToD3D11BufType(type));
    if (FAILED(hr))
        return UMC_ERR_DEVICE_FAILED;

    buffer->SetBufferPointer(NULL, 0);
    return UMC_OK;

} // Status MFXD3D11Accelerator::ReleaseBuffer(Ipp32s type)

Status MFXD3D11Accelerator::Execute()
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXD3D11Accelerator::Execute");
    D3D11_VIDEO_DECODER_BUFFER_DESC pSentBuffer[MAX_BUFFER_TYPES];
    int n = 0;
    HRESULT hr;

    ZeroMemory(pSentBuffer, sizeof(pSentBuffer));

    for (Ipp32s const type : m_bufferOrder)
    {
        UMCVACompBuffer const* pCompBuffer = FindBuffer(type);
        UMC_CHECK(pCompBuffer, UMC_ERR_FAILED);
        if (!pCompBuffer->GetPtr()) continue;
        if (!pCompBuffer->GetBufferSize()) continue;

        int FirstMb = 0, NumOfMB = 0;

        if (pCompBuffer->FirstMb > 0) FirstMb = pCompBuffer->FirstMb;
        if (pCompBuffer->NumOfMB > 0) NumOfMB = pCompBuffer->NumOfMB;

        pSentBuffer[n].BufferType = MapDXVAToD3D11BufType(pCompBuffer->type);
        pSentBuffer[n].NumMBsInBuffer = NumOfMB;
        pSentBuffer[n].DataSize = pCompBuffer->GetDataSize();
        pSentBuffer[n].BufferIndex = 0;
        pSentBuffer[n].DataOffset = 0;
        pSentBuffer[n].FirstMBaddress = FirstMb;
        pSentBuffer[n].Width = 0;
        pSentBuffer[n].Height = 0;
        pSentBuffer[n].Stride = 0;
        pSentBuffer[n].ReservedBits = 0;
        pSentBuffer[n].pIV = pCompBuffer->GetPVPStatePtr();
        pSentBuffer[n].IVSize = pCompBuffer->GetPVPStateSize();
        pSentBuffer[n].PartialEncryption = FALSE;
        n++;

        ReleaseBuffer(type);
    }

    {
        //MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_EXTCALL, "DXVA2_DecodeExecute");
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "SubmitDecoderBuffers");
            hr = m_pVideoContext->SubmitDecoderBuffers(m_pDecoder, (UINT)m_bufferOrder.size(), pSentBuffer);
        }

    }

    m_bufferOrder.clear();

    if (FAILED(hr))
        return UMC_ERR_DEVICE_FAILED;

    return UMC_OK;

} // Status MFXD3D11Accelerator::Execute()

Status MFXD3D11Accelerator::ExecuteExtensionBuffer(void * buffer)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "MFXD3D11Accelerator::ExecuteExtensionBuffer");

    UMC_CHECK_PTR(buffer);

    HRESULT hr;
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");

        auto ext = reinterpret_cast<D3D11_VIDEO_DECODER_EXTENSION*>(buffer);
        hr = m_pVideoContext->DecoderExtension(m_pDecoder, ext);
    }

    return
        SUCCEEDED(hr) ? UMC_OK : UMC_ERR_DEVICE_FAILED;
}

Status MFXD3D11Accelerator::Close()
{
    m_pVideoDevice  = nullptr;
    m_pVideoContext = nullptr;

    m_pDecoder.Release();
    m_pVDOView.Release();

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    delete m_protectedVA;
    m_protectedVA = 0;
#endif

#ifndef MFX_DEC_VIDEO_POSTPROCESS_DISABLE
    delete m_videoProcessingVA;
    m_videoProcessingVA = 0;
#endif

    return DXAccelerator::Close();
}

bool MFXD3D11Accelerator::IsIntelCustomGUID() const
{
    return GuidProfile::IsIntelCustomGUID(m_DecoderGuid);
}

UMC::Status MFXD3D11Accelerator::GetVideoDecoderDriverHandle(HANDLE* handle)
{
    UMC_CHECK_PTR(handle);
    UMC_CHECK(m_pDecoder, UMC_ERR_NOT_INITIALIZED);

    return SUCCEEDED(m_pDecoder->GetDriverHandle(handle)) ?
        UMC_OK : UMC_ERR_UNSUPPORTED;
}

Status MFXD3D11Accelerator::ExecuteExtension(int function, ExtensionData const& data)
{
    D3D11_VIDEO_DECODER_EXTENSION ext{ UINT(function) };
    ext.pPrivateInputData     = data.input.first;
    ext.PrivateInputDataSize  = UINT(data.input.second);
    ext.pPrivateOutputData    = data.output.first;
    ext.PrivateOutputDataSize = UINT(data.output.second);
    ext.ResourceCount         = UINT(data.resources.size());
    ext.ppResourceList        = ext.ResourceCount ? (ID3D11Resource**)data.resources.data() : nullptr;

    return ExecuteExtensionBuffer(&ext);
}

#endif
#endif
