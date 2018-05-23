//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined(_WIN32) || defined(_WIN64)

#include "mfx_h265_encode_hw_d3d9.h"
#include "mfx_h265_encode_hw_d3d11.h"


#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

namespace MfxHwH265Encode
{
template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::D3D11Encoder()
    : m_core(0)
    , m_guid()
    , m_width(0)
    , m_height(0)
    , m_caps()
    , m_capsQuery()
    , m_capsGet()
    , m_infoQueried(false)
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    , m_pavp(false)
    , m_widi(false)
#endif
    , m_maxSlices(0)
    , m_sps()
    , m_pps()
#if defined(MFX_ENABLE_MFE)
    , m_StreamInfo()
    , m_pMfeAdapter(NULL)
#endif
#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
    , DDITracer(std::is_same<DDI_SPS, ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT>::value ? ENCODER_REXT 
#if defined(MFX_ENABLE_HEVCE_SCC)
        : std::is_same<DDI_SPS, ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_SCC>::value ? ENCODER_SCC
#endif
        : ENCODER_DEFAULT)
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
{
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::~D3D11Encoder()
{
    Destroy();
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::CreateAuxilliaryDevice(
    MFXCoreInterface * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAuxilliaryDevice");
    m_core = core;
    mfxStatus sts = MFX_ERR_NONE;
    ID3D11Device* device;
    D3D11_VIDEO_DECODER_DESC    desc = {};
    D3D11_VIDEO_DECODER_CONFIG  config = {};
    HRESULT hr;
    mfxU32 count = 0;

    m_guid      = guid;
    m_width     = width;
    m_height    = height;

    // [0] Get device/context
    if (!m_vcontext)
    {
        sts = core->GetHandle(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        if (sts == MFX_ERR_NOT_FOUND)
            sts = core->CreateAccelerationDevice(MFX_HANDLE_D3D11_DEVICE, (mfxHDL*)&device);

        MFX_CHECK_STS(sts);
        MFX_CHECK(device, MFX_ERR_DEVICE_FAILED);

        device->GetImmediateContext(&m_context);
        MFX_CHECK(m_context, MFX_ERR_DEVICE_FAILED);

        m_vdevice  = device;
        m_vcontext = m_context;
        MFX_CHECK(m_vdevice, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(m_vcontext, MFX_ERR_DEVICE_FAILED);
    }
    if(guid != DXVA2_Intel_MFE)
    {
        // [1] Query supported decode profiles
        {
            bool isFound = false;

            UINT profileCount;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfileCount");
                profileCount = m_vdevice->GetVideoDecoderProfileCount();
            }
            assert( profileCount > 0 );

            for (UINT i = 0; i < profileCount; i ++)
            {
                GUID profileGuid;

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderProfile");
                    hr = m_vdevice->GetVideoDecoderProfile(i, &profileGuid);
                }
                MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

                if (m_guid == profileGuid)
                {
                    isFound = true;
                    break;
                }
            }
            MFX_CHECK(isFound, MFX_ERR_UNSUPPORTED);
        }

        // [2] Query the supported encode functions
        {
            desc.SampleWidth  = m_width;
            desc.SampleHeight = m_height;
            desc.OutputFormat = DXGI_FORMAT_NV12;
            desc.Guid         = m_guid;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "GetVideoDecoderConfigCount");
                hr = m_vdevice->GetVideoDecoderConfigCount(&desc, &count);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

        // [3] CreateVideoDecoder
        {
            desc.SampleWidth  = m_width;
            desc.SampleHeight = m_height;
            desc.OutputFormat = DXGI_FORMAT_NV12;
            desc.Guid         = m_guid;

    #if !defined(MFX_PROTECTED_FEATURE_DISABLE)
            config.ConfigDecoderSpecific         = m_widi ? (ENCODE_ENC_PAK | ENCODE_WIDI) : ENCODE_ENC_PAK;
            config.guidConfigBitstreamEncryption = m_pavp ? DXVA2_INTEL_PAVP : DXVA_NoEncrypt;
    #else
            config.ConfigDecoderSpecific         = ENCODE_ENC_PAK;
            config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
    #endif
            if (!!m_vdecoder)
                m_vdecoder.Release();

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "CreateVideoDecoder");
                hr = m_vdevice->CreateVideoDecoder(&desc, &config, &m_vdecoder);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

        // [4] Query the encoding device capabilities
        {
            D3D11_VIDEO_DECODER_EXTENSION ext = {};
            ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
            ext.pPrivateInputData = 0;
            ext.PrivateInputDataSize = 0;
            ext.pPrivateOutputData = &m_caps;
            ext.PrivateOutputDataSize = sizeof(m_caps);
            ext.ResourceCount = 0;
            ext.ppResourceList = 0;

            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
                hr = DecoderExtension(ext);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
    }
#if defined(MFX_ENABLE_MFE)
    else
    {
        m_pMfeAdapter = CreatePlatformMFEEncoder(core);
        if (m_pMfeAdapter == NULL)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        sts = m_pMfeAdapter->Create(m_vdevice, m_vcontext);
        MFX_CHECK_STS(sts);
        ENCODE_CAPS_HEVC * caps = (ENCODE_CAPS_HEVC *)m_pMfeAdapter->GetCaps(DDI_CODEC_HEVC);
        if(caps == NULL)
            return MFX_ERR_UNDEFINED_BEHAVIOR;
        m_caps = *caps;
        m_vdecoder = m_pMfeAdapter->GetVideoDecoder();
        if (m_vdecoder == NULL)
            return MFX_ERR_UNDEFINED_BEHAVIOR;

    }
#endif
    Trace(m_guid, 0);
    Trace(m_caps, 0);

    sts = HardcodeCaps(m_caps, core);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::CreateAccelerationService(MfxVideoParam const & par)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::CreateAccelerationService");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    HRESULT hr;
    D3D11_VIDEO_DECODER_EXTENSION ext = {};

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    if (   m_pavp != (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP)
        || m_widi != par.WiDi)
    {
        // Have to create new device

        m_widi = par.WiDi;
        m_pavp = (par.Protected == MFX_PROTECTION_PAVP || par.Protected == MFX_PROTECTION_GPUCP_PAVP);

        mfxStatus sts = CreateAuxilliaryDevice(m_core, m_guid, par.mfx.FrameInfo.Width, par.mfx.FrameInfo.Height);
        MFX_CHECK_STS(sts);

        if (m_pavp)
        {
            const mfxExtPAVPOption& PAVP = par.m_ext.PAVP;

            D3D11_AES_CTR_IV      initialCounter = { PAVP.CipherCounter.IV, PAVP.CipherCounter.Count };
            PAVP_ENCRYPTION_MODE  encryptionMode = { PAVP.EncryptionType,   PAVP.CounterType         };
            ENCODE_ENCRYPTION_SET encryptSet     = { PAVP.CounterIncrement, &initialCounter, &encryptionMode};

            D3D11_VIDEO_DECODER_EXTENSION encryptParam = {};
            encryptParam.Function = ENCODE_ENCRYPTION_SET_ID;
            encryptParam.pPrivateInputData     = &encryptSet;
            encryptParam.PrivateInputDataSize  = sizeof(ENCODE_ENCRYPTION_SET);

            hr = DecoderExtension(encryptParam);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
    }
#endif
    /* not used
    ext.Function = ENCODE_ENC_CTRL_CAPS_ID;
    ext.pPrivateOutputData = &m_capsQuery;
    ext.PrivateOutputDataSize = sizeof(m_capsQuery);

    hr = DecoderExtension(ext);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    ext.Function = ENCODE_ENC_CTRL_GET_ID;
    ext.pPrivateOutputData = &m_capsGet;
    ext.PrivateOutputDataSize = sizeof(m_capsGet);

    hr = DecoderExtension(ext);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    */
#if defined(MFX_ENABLE_MFE)
    if (m_pMfeAdapter != NULL)
    {
        m_StreamInfo.CodecId = DDI_CODEC_HEVC;
        m_pMfeAdapter->Join(par.m_ext.mfeParam, m_StreamInfo, false);
    }
#endif
    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_caps, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_maxSlices = CeilDiv(par.m_ext.HEVCParam.PicHeightInLumaSamples, par.LCUSize) * CeilDiv(par.m_ext.HEVCParam.PicWidthInLumaSamples, par.LCUSize);
    m_maxSlices = Min(m_maxSlices, (mfxU32)MAX_SLICES);

    mfxStatus sts = D3DXCommonEncoder::Init(m_core);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Reset(MfxVideoParam const & par, bool bResetBRC)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);

    FillSpsBuffer(par, m_caps, m_sps);
    FillPpsBuffer(par, m_caps, m_pps);
    FillSliceBuffer(par, m_sps, m_pps, m_slice);

    DDIHeaderPacker::Reset(par);
    m_cbd.resize(MAX_DDI_BUFFERS + MaxPackedHeaders());

    m_sps.bResetBRC = bResetBRC;

    return MFX_ERR_NONE;
}

mfxU8 convertDX9TypeToDX11Type(mfxU8 type)
{
    switch(type)
    {
    case D3DDDIFMT_INTELENCODE_SPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA;
    case D3DDDIFMT_INTELENCODE_PPSDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA;
    case D3DDDIFMT_INTELENCODE_SLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA;
    case D3DDDIFMT_INTELENCODE_QUANTDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_QUANTDATA;
    case D3DDDIFMT_INTELENCODE_BITSTREAMDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA;
    case D3DDDIFMT_INTELENCODE_MBDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBDATA;
    case D3DDDIFMT_INTELENCODE_SEIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SEIDATA;
    case D3DDDIFMT_INTELENCODE_VUIDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VUIDATA;
    case D3DDDIFMT_INTELENCODE_VMESTATE:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMESTATE;
    case D3DDDIFMT_INTELENCODE_VMEINPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEINPUT;
    case D3DDDIFMT_INTELENCODE_VMEOUTPUT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_VMEOUTPUT;
    case D3DDDIFMT_INTELENCODE_EFEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_EFEDATA;
    case D3DDDIFMT_INTELENCODE_STATDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_STATDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA;
    case D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA;
    case D3DDDIFMT_INTELENCODE_MBQPDATA:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA;
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    case D3DDDIFMT_INTELENCODE_SYNCOBJECT:
        return D3D11_DDI_VIDEO_ENCODER_BUFFER_SYNCOBJECT;
#endif
    default:
        break;
    }
    return 0xFF;
}

mfxU32 convertDXGIFormatToMFXFourCC(DXGI_FORMAT format)
{
    switch(format)
    {
    case DXGI_FORMAT_NV12: return MFX_FOURCC_NV12;
    case DXGI_FORMAT_P010: return MFX_FOURCC_P010;
    case DXGI_FORMAT_YUY2: return MFX_FOURCC_YUY2;
    case DXGI_FORMAT_Y210: return MFX_FOURCC_P210;
    case DXGI_FORMAT_P8  : return MFX_FOURCC_P8;
    default:
        return 0;
    }
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    HRESULT hr;

    type = (D3DDDIFORMAT)convertDX9TypeToDX11Type((mfxU8)type);


    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT cnt = {};

        D3D11_VIDEO_DECODER_EXTENSION ext = {};
        ext.Function = ENCODE_FORMAT_COUNT_ID;
        ext.pPrivateOutputData = &cnt;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMAT_COUNT);


        hr = DecoderExtension(ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(cnt.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(cnt.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats = {};
        encodeFormats.CompressedBufferInfoSize  = (int)(m_compBufInfo.size() * sizeof(ENCODE_COMP_BUFFER_INFO));
        encodeFormats.UncompressedFormatSize    = (int)(m_uncompBufInfo.size() * sizeof(D3DDDIFORMAT));
        encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];

        Zero(ext);
        ext.Function = ENCODE_FORMATS_ID;
        ext.pPrivateOutputData = &encodeFormats;
        ext.PrivateOutputDataSize = sizeof(ENCODE_FORMATS);

        hr = DecoderExtension(ext);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        MFX_CHECK(encodeFormats.CompressedBufferInfoSize > 0, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(encodeFormats.UncompressedFormatSize > 0, MFX_ERR_DEVICE_FAILED);

        m_infoQueried = true;
    }

    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }

    MFX_CHECK(i < m_compBufInfo.size(), MFX_ERR_DEVICE_FAILED);

    request.Info.Width  = m_compBufInfo[i].CreationWidth;
    request.Info.Height = m_compBufInfo[i].CreationHeight;
    request.Info.FourCC = convertDXGIFormatToMFXFourCC((DXGI_FORMAT)m_compBufInfo[i].CompressedFormats);

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryEncodeCaps(ENCODE_CAPS_HEVC & caps)
{
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_CHECK_WITH_ASSERT(m_core, MFX_ERR_NOT_INITIALIZED);

    mfxFrameAllocator & fa = m_core->FrameAllocator();
    std::vector<mfxHDLPair> & queue = (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA) ? m_bsQueue : m_reconQueue;

    queue.resize(response.NumFrameActual);

    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxHDLPair handlePair = {};

        mfxStatus sts = fa.GetHDL(fa.pthis, response.mids[i], (mfxHDL*)&handlePair);
        MFX_CHECK_STS(sts);

        queue[i] = handlePair;
    }

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        ENCODE_QUERY_STATUS_PARAM_TYPE fbType = m_pps.bEnableSliceLevelReport ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME;
        m_feedbackPool.Reset(response.NumFrameActual, fbType, m_maxSlices);
    }

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    m_EventCache.reset(new EventCache());
    m_EventCache->Init(response.NumFrameActual);
#endif

    return MFX_ERR_NONE;
}

#define ADD_CBD(id, buf, num)\
    assert(executeParams.NumCompBuffers < m_cbd.size());\
    m_cbd[executeParams.NumCompBuffers].CompressedBufferType = (D3DFORMAT)(id); \
    m_cbd[executeParams.NumCompBuffers].DataSize = (UINT)(sizeof(buf) * (num)); \
    m_cbd[executeParams.NumCompBuffers].pCompBuffer = &buf; \
    executeParams.NumCompBuffers++;

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::ExecuteImpl(Task const & task, mfxHDLPair pair)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::Execute");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    ENCODE_PACKEDHEADER_DATA * pPH = 0;
    ENCODE_INPUT_DESC ein = {};
    ENCODE_EXECUTE_PARAMS executeParams = {};
#if defined(MFX_SKIP_FRAME_SUPPORT)
    HevcSkipMode skipMode(task.m_SkipMode);
#endif

    ID3D11Resource * surface = static_cast<ID3D11Resource *>(pair.first);
    UINT subResourceIndex = (UINT)(UINT_PTR)(pair.second);

    executeParams.pCompressedBuffers = &m_cbd[0];
    Zero(m_cbd);
    
    if (!m_sps.bResetBRC)
        m_sps.bResetBRC = task.m_resetBRC;

    FillPpsBuffer(task, m_caps, m_pps, m_dirtyRects);
    FillSliceBuffer(task, m_sps, m_pps, m_slice);

    mfxU32 RES_ID_BS  = 0;
    mfxU32 RES_ID_RAW = 1;
    mfxU32 RES_ID_REF = 2;
    mfxU32 RES_ID_REC = RES_ID_REF + task.m_idxRec;

    std::vector<ID3D11Resource*> resourceList(RES_ID_REF + m_reconQueue.size());

    resourceList[RES_ID_BS ] = (ID3D11Resource*)m_bsQueue[task.m_idxBs].first;
    resourceList[RES_ID_RAW] = surface;

    for (mfxU32 i = 0; i < m_reconQueue.size(); i ++)
        resourceList[RES_ID_REF + i] = (ID3D11Resource*)m_reconQueue[i].first;

#if defined(MFX_ENABLE_MFE)
    if (m_pMfeAdapter != NULL)
    {
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_MULTISTREAMS, m_StreamInfo, 1);
    }
#endif
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_SPSDATA,          m_sps,      1);
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PPSDATA,          m_pps,      1);
    {
        // attach resources to PPSDATA
        ein.ArraSliceOriginal = subResourceIndex;
        ein.IndexOriginal     = RES_ID_RAW;
        ein.ArraySliceRecon   = (UINT)(size_t(m_reconQueue[task.m_idxRec].second));
        ein.IndexRecon        = RES_ID_REC;
        m_cbd[executeParams.NumCompBuffers - 1].pReserved = &ein;
    }

    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_SLICEDATA,        m_slice[0], m_slice.size());
    ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_BITSTREAMDATA,    RES_ID_BS,  1);


#if MFX_EXTBUFF_CU_QP_ENABLE
    if (task.m_bCUQPMap)
    {
        mfxU32 idxCUQp  = task.m_idxCUQp;
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_MBQPDATA, idxCUQp,  1);
    }
#endif

    if (task.m_insertHeaders & INSERT_AUD)
    {
        pPH = PackHeader(task, AUD_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_VPS)
    {
        pPH = PackHeader(task, VPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_SPS)
    {
        pPH = PackHeader(task, SPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if (task.m_insertHeaders & INSERT_PPS)
    {
        pPH = PackHeader(task, PPS_NUT); assert(pPH);
        ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
    }

    if ((task.m_insertHeaders & INSERT_SEI) || (task.m_ctrl.NumPayload > 0))
    {
        pPH = PackHeader(task, PREFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
        }
    }

    for (mfxU32 i = 0; i < m_slice.size(); i ++)
    {
#if defined(MFX_SKIP_FRAME_SUPPORT)
        if (skipMode.NeedSkipSliceGen())
        {
            // pack skip slice
            pPH = PackSkippedSlice(task, i, &m_slice[i].SliceQpDeltaBitOffset); assert(pPH);
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA, *pPH, 1);
            if (!skipMode.NeedDriverCall())
            {
                // copy packed sliced into bitstream

                //ENCODE_QUERY_STATUS_PARAMS feedback = { task.m_statusReportNumber, 0, };
                mfxFrameData bs = { 0 };

                FrameLocker lock(m_core, task.m_midBs);
                assert(bs.Y);

                mfxU8 *  bsDataStart = bs.Y;
                mfxU8 *  bsEnd = bs.Y + m_width * m_height;
                mfxU8 *  bsDataEnd = bsDataStart;

                for (UINT i = 0; i < executeParams.NumCompBuffers; i++)
                {
                    if (m_cbd[i].CompressedBufferType == (D3DDDIFORMAT)D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA)
                    {
                        ENCODE_PACKEDHEADER_DATA const & data = *(ENCODE_PACKEDHEADER_DATA*)m_cbd[i].pCompBuffer;
                        mfxU8 * sbegin = data.pData + data.DataOffset;
                        bsDataEnd += AddEmulationPreventionAndCopy(sbegin, data.DataLength, bsDataEnd, bsEnd, !!m_pps.bEmulationByteInsertion);
                    }
                }
                //feedback.bitstreamSize = mfxU32(bsDataEnd - bsDataStart);
            }

        }
        else
#endif //defined(MFX_SKIP_FRAME_SUPPORT)
        {
            pPH = PackSliceHeader(task, i, &m_slice[i].SliceQpDeltaBitOffset
                ,!!(m_pps.MaxSliceSizeInBytes)
#if defined(PRE_SI_TARGET_PLATFORM_GEN10)
                , &m_slice[i].SliceSAOFlagBitOffset
                , &m_slice[i].BitLengthSliceHeaderStartingPortion
                , &m_slice[i].SliceHeaderByteOffset
                , &m_slice[i].PredWeightTableBitOffset
                , &m_slice[i].PredWeightTableBitLength
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN10)
            ); assert(pPH);
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDSLICEDATA, *pPH, 1);
        }
    }

    /*if (task.m_ctrl.NumPayload > 0)
    {
        pPH = PackHeader(task, SUFFIX_SEI_NUT); assert(pPH);

        if (pPH->DataLength)
        {
            ADD_CBD(D3D11_DDI_VIDEO_ENCODER_BUFFER_PACKEDHEADERDATA, *pPH, 1);
        }
    }*/

    try
    {
#ifdef HEADER_PACKING_TEST
        surface;
        ENCODE_QUERY_STATUS_PARAMS fb = {task.m_statusReportNumber,};
        FrameLocker bs(m_core, task.m_midBs);

        for (mfxU32 i = 0; i < executeParams.NumCompBuffers; i ++)
        {
            pPH = (ENCODE_PACKEDHEADER_DATA*)executeParams.pCompressedBuffers[i].pCompBuffer;

            if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA)
            {
                memcpy(bs.Y + fb.bitstreamSize, pPH->pData, pPH->DataLength);
                fb.bitstreamSize += pPH->DataLength;
            }
            else if (executeParams.pCompressedBuffers[i].CompressedBufferType == D3DDDIFMT_INTELENCODE_PACKEDSLICEDATA)
            {
                mfxU32 sz = m_width * m_height * 3 / 2 - fb.bitstreamSize;
                HeaderPacker::PackRBSP(bs.Y + fb.bitstreamSize, pPH->pData, sz, CeilDiv(pPH->DataLength, 8));
                fb.bitstreamSize += sz;
            }
        }

        m_feedbackPool[0] = fb;
        m_feedbackPool.Update();

#else
#if defined(MFX_SKIP_FRAME_SUPPORT)
        if (skipMode.NeedDriverCall())
        {
            HRESULT hr;
            D3D11_VIDEO_DECODER_EXTENSION ext = {};

            m_pps.SkipFrameFlag = skipMode.GetMode() != 0? UCHAR(skipMode.GetMode()) : 0;
            m_pps.NumSkipFrames = 0;
            m_pps.SizeSkipFrames = 0;


            ext.Function              = ENCODE_ENC_PAK_ID;
            ext.pPrivateInputData     = &executeParams;
            ext.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
            ext.ResourceCount         = (UINT)resourceList.size();
            ext.ppResourceList        = &resourceList[0];
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
                hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
                hr = m_vcontext->DecoderEndFrame(m_vdecoder);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
#else

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        {
            // allocate the event
            Task & task1 = const_cast<Task &>(task);
            task1.m_GpuEvent.m_gpuComponentId = GPU_COMPONENT_ENCODE;
            m_EventCache->GetEvent(task1.m_GpuEvent.gpuSyncEvent);

            D3D11_VIDEO_DECODER_EXTENSION decoderExtParams = { 0 };
            decoderExtParams.Function = DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE;
            decoderExtParams.pPrivateInputData = &task1.m_GpuEvent;
            decoderExtParams.PrivateInputDataSize = sizeof(task1.m_GpuEvent);
            decoderExtParams.pPrivateOutputData = NULL;
            decoderExtParams.PrivateOutputDataSize = 0;
            decoderExtParams.ResourceCount = 0;
            decoderExtParams.ppResourceList = NULL;

            HRESULT hr;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "SendGpuEventHandle");
                hr = DecoderExtension(decoderExtParams);
            }
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
#endif

        HRESULT hr;
        D3D11_VIDEO_DECODER_EXTENSION ext = {};

        ext.Function              = ENCODE_ENC_PAK_ID;
        ext.pPrivateInputData     = &executeParams;
        ext.PrivateInputDataSize  = sizeof(ENCODE_EXECUTE_PARAMS);
        ext.ResourceCount         = (UINT)resourceList.size();
        ext.ppResourceList        = &resourceList[0];
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderExtension");
            hr = DecoderExtension(ext);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "DecoderEndFrame");
            hr = m_vcontext->DecoderEndFrame(m_vdecoder);
        }
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
#endif //MFX_SKIP_FRAME_SUPPORT
#if defined(MFX_ENABLE_MFE)
        if (m_pMfeAdapter != NULL)
        {
            //for pre-si set to 1 hour
            mfxU32 timeout = 3600000000;// task.m_mfeTimeToWait;
            mfxStatus sts = m_pMfeAdapter->Submit(m_StreamInfo, (task.m_flushMfe ? 0 : timeout), false);
            if (sts != MFX_ERR_NONE)
                return sts;
        }
#endif
#endif
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    m_sps.bResetBRC = 0;

    return MFX_ERR_NONE;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::QueryStatusAsync(Task & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "D3D11Encoder::QueryStatusAsync");
    MFX_CHECK_WITH_ASSERT(m_vdecoder, MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses.

    // first check cache.
#if defined(MFX_SKIP_FRAME_SUPPORT)
    HevcSkipMode skipMode(task.m_SkipMode);
    if (!skipMode.NeedDriverCall())
        return MFX_ERR_NONE;
#endif
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackPool.Get(task.m_statusReportNumber);

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        ENCODE_QUERY_STATUS_PARAMS_DESCR feedbackDescr = {};

        feedbackDescr.StatusParamType = m_pps.bEnableSliceLevelReport ? QUERY_STATUS_PARAM_SLICE : QUERY_STATUS_PARAM_FRAME;
        feedbackDescr.SizeOfStatusParamStruct = (feedbackDescr.StatusParamType == QUERY_STATUS_PARAM_SLICE) ? sizeof(ENCODE_QUERY_STATUS_SLICE_PARAMS) : sizeof(ENCODE_QUERY_STATUS_PARAMS);
#if defined(MFX_ENABLE_MFE)
        feedbackDescr.StreamID = m_StreamInfo.StreamId;
#endif
        for (;;)
        {
            HRESULT hr;

            try
            {
                D3D11_VIDEO_DECODER_EXTENSION ext = {};

                ext.Function              = ENCODE_QUERY_STATUS_ID;
                ext.pPrivateInputData     = &feedbackDescr;
                ext.PrivateInputDataSize  = sizeof(feedbackDescr);
                ext.pPrivateOutputData    = &m_feedbackPool[0];
                ext.PrivateOutputDataSize = mfxU32(m_feedbackPool.size() * m_feedbackPool.feedback_size());

                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_EXTCALL, "ENCODE_QUERY_STATUS_ID");
                    hr = DecoderExtension(ext);
                }
            }
            catch (...)
            {
                return MFX_ERR_DEVICE_FAILED;
            }

            //if (hr == E_INVALIDARG && feedbackDescr.StatusParamType == QUERY_STATUS_PARAM_SLICE)
            //{
            //    feedbackDescr.StatusParamType = QUERY_STATUS_PARAM_FRAME;
            //    feedbackDescr.SizeOfStatusParamStruct = sizeof(ENCODE_QUERY_STATUS_PARAMS);
            //    continue;
            //}

            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
            break;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackPool.Update();

        feedback = m_feedbackPool.Get(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {

    case ENCODE_OK_WITH_MISMATCH:
        assert(!"slice sizes buffer is too small");
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize;

#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
        if (m_widi && m_caps.HWCounterAutoIncrementSupport)
        {
            task.m_aes_counter.Count = feedback->aes_counter.Counter;
            task.m_aes_counter.IV    = feedback->aes_counter.IV;
        }
#endif

        if (m_pps.bEnableSliceLevelReport)
        {
#if defined (MFX_ENABLE_HEVCE_UNITS_INFO)
            mfxExtEncodedUnitsInfo* pUnitsInfo = ExtBuffer::Get(*task.m_bs);
            if (pUnitsInfo)
            {
                mfxU16 *pSize = ((ENCODE_QUERY_STATUS_SLICE_PARAMS*)feedback)->SliceSizes;
                mfxU16 i = pUnitsInfo->NumUnitsEncoded, j = 0;

                while (i < (pUnitsInfo->NumUnitsAlloc) && (j < feedback->NumberSlices))
                    pUnitsInfo->UnitInfo[i++].Size = pSize[j++];

                pUnitsInfo->NumUnitsEncoded += feedback->NumberSlices;
            }
#endif
        }

        m_feedbackPool.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        Trace(*feedback, 0);
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
HRESULT D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext)
{
    HRESULT hr;

    Trace(">>Function", ext.Function);

    if (!ext.pPrivateOutputData || ext.pPrivateInputData)
        Trace(ext, 0); //input

    hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);

    if (ext.pPrivateOutputData)
        Trace(ext, 1); //output

    Trace("<<HRESULT", hr);

    return hr;
}

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
mfxStatus D3D11Encoder<DDI_SPS, DDI_PPS, DDI_SLICE>::Destroy()
{
    return MFX_ERR_NONE;
}

template class D3D11Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC, ENCODE_SET_PICTURE_PARAMETERS_HEVC, ENCODE_SET_SLICE_HEADER_HEVC>;

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
template class D3D11Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT, ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT, ENCODE_SET_SLICE_HEADER_HEVC_REXT>;
#endif

#if defined(MFX_ENABLE_HEVCE_SCC)
template class D3D11Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_SCC, ENCODE_SET_PICTURE_PARAMETERS_HEVC_SCC, ENCODE_SET_SLICE_HEADER_HEVC>;
#endif
}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif
