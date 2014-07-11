/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2014 Intel Corporation. All Rights Reserved.

File Name: d3d11_decode_accelerator.cpp

\* ****************************************************************************** */

#if defined  (MFX_VA)
#if defined  (MFX_D3D11_ENABLED)

#include "d3d11_decode_accelerator.h"
#include "libmfx_core.h"
#include "mfx_utils.h"
#include "umc_vc1_dec_va_defs.h"

#include "umc_va_dxva2.h"
#include "umc_va_dxva2_protected.h"
#include "umc_va_video_processing.h"
#include "libmfx_core_d3d11.h"
#include "mfx_umc_alloc_wrapper.h"


#define DXVA2_VC1PICTURE_PARAMS_EXT_BUFFER 21
#define DXVA2_VC1BITPLANE_EXT_BUFFER       22

static const GUID DXVADDI_Intel_ModeVC1_D =
{ 0xe07ec519, 0xe651, 0x4cd6, { 0xac, 0x84, 0x13, 0x70, 0xcc, 0xee, 0xc8, 0x51 } };
static const GUID DXVADDI_Intel_ModeVC1_D_Advanced = 
{ 0xbcc5db6d, 0xa2b6, 0x4af0, { 0xac, 0xe4, 0xad, 0xb1, 0xf7, 0x87, 0xbc, 0x89 } };

using namespace UMC;



MFXD3D11Accelerator::MFXD3D11Accelerator(ID3D11VideoDevice  *pVideoDevice, 
                                         ID3D11VideoContext *pVideoContext):m_pVideoDevice(pVideoDevice),
                                                                               m_pVideoContext(pVideoContext),
                                                                               m_pVDOView(0),
                                                                               m_pCore(0),
                                                                               m_pDecoder(0)
{
} //MFXD3D11Accelerator::MFXD3D11Accelerator

mfxStatus MFXD3D11Accelerator::CreateVideoAccelerator(mfxU32 hwProfile, const mfxVideoParam *param, D3D11VideoCORE *pCore)
{
    mfxStatus sts;
    HRESULT hres;
    D3D11_VIDEO_DECODER_DESC video_desc;
    m_pCore = pCore;

    video_desc.SampleWidth = param->mfx.FrameInfo.Width;
    video_desc.SampleHeight = param->mfx.FrameInfo.Height;
    video_desc.OutputFormat = mfxDefaultAllocatorD3D11::MFXtoDXGI(param->mfx.FrameInfo.FourCC);//DXGI_FORMAT_NV12;

    m_Profile = (VideoAccelerationProfile)hwProfile;

    D3D11_VIDEO_DECODER_CONFIG video_config = {0}; // !!!!!!!!

    if (IS_PROTECTION_ANY(param->Protected))
    {
        m_protectedVA = new UMC::ProtectedVA(param->Protected);
    }

    sts = GetSuitVideoDecoderConfig(param, &video_desc, &video_config);
    MFX_CHECK_STS(sts);

    m_DecoderGuid = video_desc.Guid;

    hres  = m_pVideoDevice->CreateVideoDecoder(&video_desc, &video_config, &m_pDecoder);
    if (FAILED(hres))
        return MFX_ERR_DEVICE_FAILED;

    m_bH264MVCSupport = IsVaMvcProfile(m_Profile);
    m_isUseStatuReport = true;

    if ((m_Profile & 0xf) == VA_H265)
        m_bH264ShortSlice = 1 == video_config.ConfigBitstreamRaw;
    else
        m_bH264ShortSlice = (2 == video_config.ConfigBitstreamRaw || 4 == video_config.ConfigBitstreamRaw || 6 == video_config.ConfigBitstreamRaw);

    return MFX_ERR_NONE;

} // mfxStatus MFXD3D11Accelerator::CreateVideoAccelerator

mfxStatus MFXD3D11Accelerator::GetSuitVideoDecoderConfig(const mfxVideoParam            *param,                  
                                                         D3D11_VIDEO_DECODER_DESC *video_desc,  
                                                         D3D11_VIDEO_DECODER_CONFIG     *pConfig) 
{
    param;

    for (mfxU32 k = 0; k < GuidProfile::GetGuidProfileSize(); k++)
    {
        if ((m_Profile & (VA_ENTRY_POINT | VA_CODEC)) != GuidProfile::GetGuidProfile(k)->profile)
            continue;

        video_desc->Guid = GuidProfile::GetGuidProfile(k)->guid;

        mfxU32  count;
        HRESULT hr = m_pVideoDevice->GetVideoDecoderConfigCount(video_desc, &count);
        if (FAILED(hr)) // guid can be absent. It is ok
            continue;

        Ipp32u isHEVCGUID = (m_Profile & 0xf) == VA_H265;

        mfxI32 idxConfig = -1;
        for (mfxU32 i = 0; i < count; i++)
        {
            hr = m_pVideoDevice->GetVideoDecoderConfig(video_desc, i, pConfig);
            if (FAILED(hr))
                continue;

            if (CheckDXVAConfig<D3D11_VIDEO_DECODER_CONFIG>(m_Profile, pConfig, GetProtectedVA()) == false)
                continue;

            if (idxConfig == -1)
                idxConfig = i;

            if (isHEVCGUID)
            {
                if (2 == pConfig->ConfigBitstreamRaw)
                { // prefer long mode
                    idxConfig = i;
                }
            }
            else
            {
                if (2 == pConfig->ConfigBitstreamRaw || 4 == pConfig->ConfigBitstreamRaw || 6 == pConfig->ConfigBitstreamRaw)
                { // short mode
                    idxConfig = i;
                }
            }
        }

        if (idxConfig != -1)
        {
            hr = m_pVideoDevice->GetVideoDecoderConfig(video_desc, idxConfig, pConfig);
            if (FAILED(hr))
                return MFX_ERR_DEVICE_FAILED;

            return MFX_ERR_NONE;
        }
    }

    return MFX_ERR_UNSUPPORTED;

} //mfxStatus MFXD3D11Accelerator::GetSuitVideoDecoderConfig

void* MFXD3D11Accelerator::GetCompBuffer(Ipp32s buffer_type, UMC::UMCVACompBuffer **buf, Ipp32s size, Ipp32s index)
{
    size; index;
    UMC_CHECK(buffer_type >= 0, NULL);
    UMC_CHECK(buffer_type < MAX_BUFFER_TYPES, NULL);
    UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[buffer_type];

    if (!pCompBuffer->GetPtr())
    {
        void *pBuffer = NULL;
        UINT uBufferSize = 0;
        HRESULT hr = m_pVideoContext->GetDecoderBuffer(m_pDecoder, MapDXVAToD3D11BufType(buffer_type), &uBufferSize,  &pBuffer);
        if (FAILED(hr))
        {
            vm_trace_x(hr);
            VM_ASSERT(SUCCEEDED(hr));
            return NULL;
        }

        pCompBuffer->type = buffer_type;
        pCompBuffer->SetBufferPointer((Ipp8u*)pBuffer, uBufferSize);
        pCompBuffer->SetDataSize(0);

        m_bufferOrder.push_back(buffer_type);
    }

    if (buf)
    {
        *buf = pCompBuffer;
    }

    return pCompBuffer->GetPtr();

} // void* MFXD3D11Accelerator::GetCompBuffer(Ipp32s buffer_type, UMC::UMCVACompBuffer **buf, Ipp32s size, Ipp32s index)

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
    case D3DDDIFMT_INTEL_JPEGDECODE_PPSDATA:
    case D3DDDIFMT_INTEL_JPEGDECODE_QUANTDATA:
    case D3DDDIFMT_INTEL_JPEGDECODE_HUFFTBLDATA:
    case D3DDDIFMT_INTEL_JPEGDECODE_SCANDATA:
        return (D3D11_VIDEO_DECODER_BUFFER_TYPE)(DXVABufType - 1);

    default:
        return (D3D11_VIDEO_DECODER_BUFFER_TYPE)-1;
    }
} // D3D11_VIDEO_DECODER_BUFFER_TYPE MFXD3D11Accelerator::MapDXVAToD3D11BufType(const Ipp32s DXDVABufType)

Status  MFXD3D11Accelerator::BeginFrame(Ipp32s index)
{
    HRESULT hr = S_OK;
    mfxHDLPair Pair;



    if (MFX_ERR_NONE != m_pCore->GetDX11Handle(index, &Pair))
        return UMC_ERR_DEVICE_FAILED;
    


    D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC OutputDesc;
    OutputDesc.DecodeProfile = m_DecoderGuid;
    OutputDesc.ViewDimension = D3D11_VDOV_DIMENSION_TEXTURE2D;
    OutputDesc.Texture2D.ArraySlice = (UINT)(size_t)Pair.second;
    SAFE_RELEASE(m_pVDOView);


    hr = m_pVideoDevice->CreateVideoDecoderOutputView((ID3D11Resource *)Pair.first, 
                                                      &OutputDesc,
                                                      &m_pVDOView);


    if( SUCCEEDED( hr ) )
    {
        hr = m_pVideoContext->DecoderBeginFrame(m_pDecoder, m_pVDOView, 0, NULL);
        if SUCCEEDED( hr )
            return UMC_OK;
    }
    
    return UMC_ERR_DEVICE_FAILED;

} // mfxStatus  MFXD3D11Accelerator::BeginFrame(Ipp32s index)


Status MFXD3D11Accelerator::EndFrame(void *handle)
{
    HRESULT hr;
    handle;
    hr = m_pVideoContext->DecoderEndFrame(m_pDecoder);
    if SUCCEEDED(hr)
    {
        //Sleep(100);
        return UMC_OK;
    }

     return UMC_ERR_DEVICE_FAILED;

} //Status MFXD3D11Accelerator::EndFrame(void *handle)

Status MFXD3D11Accelerator::ReleaseBuffer(Ipp32s type)
{
    HRESULT hr = S_OK;
    UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[type];

    hr = m_pVideoContext->ReleaseDecoderBuffer(m_pDecoder, MapDXVAToD3D11BufType(type));
    if (FAILED(hr))
        return UMC_ERR_DEVICE_FAILED;
    
    pCompBuffer->SetBufferPointer(NULL, 0);
    return UMC_OK;

} // Status MFXD3D11Accelerator::ReleaseBuffer(Ipp32s type)

Status MFXD3D11Accelerator::Execute()
{
    D3D11_VIDEO_DECODER_BUFFER_DESC pSentBuffer[MAX_BUFFER_TYPES];
    int n = 0;
    HRESULT hr;

    ZeroMemory(pSentBuffer, sizeof(pSentBuffer));

    for (Ipp32u j = 0; j < m_bufferOrder.size(); ++j)
    {
        Ipp32u i = m_bufferOrder[j];

        UMCVACompBuffer *pCompBuffer = &m_pCompBuffer[i];
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
        ReleaseBuffer(i);
    }
    
    {
        //MFX_AUTO_LTRACE_WITHID(MFX_TRACE_LEVEL_DXVA, "DXVA2_DecodeExecute");
         hr = m_pVideoContext->SubmitDecoderBuffers(m_pDecoder, (UINT)m_bufferOrder.size(), pSentBuffer);

    }
    m_bufferOrder.clear();

    if (FAILED(hr))
        return UMC_ERR_DEVICE_FAILED;

    return UMC_OK;

} // Status MFXD3D11Accelerator::Execute()


Status MFXD3D11Accelerator::ExecuteStatusReportBuffer(void * buffer, Ipp32s size)
{
    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION dec_ext;
    
    memset(&dec_ext, 0, sizeof(D3D11_VIDEO_DECODER_EXTENSION));

    dec_ext.Function = 7;
    dec_ext.ppResourceList = 0;
    dec_ext.PrivateInputDataSize = 0;
    dec_ext.pPrivateOutputData =  buffer;
    dec_ext.PrivateOutputDataSize = size;
    dec_ext.ppResourceList = 0;


    hr = m_pVideoContext->DecoderExtension(m_pDecoder, &dec_ext);

    if (FAILED(hr))
        return UMC::UMC_ERR_DEVICE_FAILED;

    return UMC_OK;
}

Status MFXD3D11Accelerator::Close()
{
    SAFE_RELEASE(m_pVDOView);

    delete m_protectedVA;
    m_protectedVA = 0;

    delete m_videoProcessingVA;
    m_videoProcessingVA = 0;

    Status sts = VideoAccelerator::Close();
    return sts;
}

bool MFXD3D11Accelerator::IsIntelCustomGUID() const
{
    const GUID & guid = m_DecoderGuid;
    return (guid == sDXVA2_Intel_ModeVC1_D_Super || guid == sDXVA2_Intel_EagleLake_ModeH264_VLD_NoFGT || guid == sDXVA_Intel_ModeH264_VLD_MVC || guid == DXVA_Intel_ModeHEVC_VLD_MainProfile ||
        guid == DXVA_Intel_ModeHEVC_VLD_Main10Profile);
}


#endif
#endif
