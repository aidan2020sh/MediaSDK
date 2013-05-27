/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2009-2012 Intel Corporation. All Rights Reserved.
//
//
//          MJPEG encoder DXVA2
//
*/

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE)

#include "mfx_mjpeg_encode_hw_utils.h"
#include "libmfx_core_factory.h"
#include "libmfx_core_interface.h"
#include "jpegbase.h"
#include "mfx_enc_common.h"

//#include "libmfx_core_d3d9.h"
#include "mfx_mjpeg_encode_d3d9.h"
#include "libmfx_core_interface.h"

#include "mfx_mjpeg_encode_hw_utils.h"

#include "umc_va_dxva2_protected.h"

using namespace MfxHwMJpegEncode;

D3D9Encoder::D3D9Encoder()
: m_core(0)
, m_pAuxDevice(0)
, m_pDdiData(0)
, m_counter(0)
, m_infoQueried(false)
{
}

D3D9Encoder::~D3D9Encoder()
{
    Destroy();
}

mfxStatus D3D9Encoder::CreateAuxilliaryDevice(
    VideoCORE * core,
    GUID        guid,
    mfxU32      width,
    mfxU32      height,
    bool        isTemporal)
{
    m_core = core;

    D3D9Interface *pID3D = QueryCoreInterface<D3D9Interface>(m_core, MFXICORED3D_GUID);
    MFX_CHECK(pID3D, MFX_ERR_DEVICE_FAILED);


    IDirectXVideoDecoderService *service = 0;
    mfxStatus sts = pID3D->GetD3DService(mfxU16(width), mfxU16(height), &service, isTemporal);
    MFX_CHECK_STS(sts);
    MFX_CHECK(service, MFX_ERR_DEVICE_FAILED);

    AuxiliaryDevice *pAuxDevice = new AuxiliaryDevice();
    sts = pAuxDevice->Initialize(0, service);
    if(sts != MFX_ERR_NONE)
    {
        delete pAuxDevice;
        return sts;
    }

    sts = pAuxDevice->IsAccelerationServiceExist(guid);
    if(sts != MFX_ERR_NONE)
    {
        delete pAuxDevice;
        return sts;
    }

    memset(&m_caps, 0, sizeof(m_caps));
    HRESULT hr = pAuxDevice->Execute(AUXDEV_QUERY_ACCEL_CAPS, &guid, sizeof(guid), &m_caps, sizeof(m_caps));
    if(!SUCCEEDED(hr))
    {
        delete pAuxDevice;
        return MFX_ERR_DEVICE_FAILED;
    }

    m_guid   = guid;
    m_width  = width;
    m_height = height;
    m_pAuxDevice = pAuxDevice;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::CreateAccelerationService(mfxVideoParam const & par)
{
    par;
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    DXVADDI_VIDEODESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.SampleWidth  = m_width;
    desc.SampleHeight = m_height;
    desc.Format       = D3DDDIFMT_NV12;

    ENCODE_CREATEDEVICE encodeCreateDevice;
    memset(&encodeCreateDevice, 0, sizeof(encodeCreateDevice));
    encodeCreateDevice.pVideoDesc     = &desc;
    encodeCreateDevice.CodingFunction = ENCODE_PAK;
    encodeCreateDevice.EncryptionMode = DXVA_NoEncrypt;

    HRESULT hr = m_pAuxDevice->Execute(AUXDEV_CREATE_ACCEL_SERVICE, &m_guid, sizeof(m_guid), &encodeCreateDevice, sizeof(encodeCreateDevice));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    memset(&m_capsQuery, 0, sizeof(m_capsQuery));
    hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_CAPS_ID, (void*)0, 0, &m_capsQuery, sizeof(m_capsQuery));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    memset(&m_capsGet, 0, sizeof(m_capsGet));
    hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_GET_ID, (void*)0, 0, &m_capsGet, sizeof(m_capsGet));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Reset(mfxVideoParam const & par)
{
    mfxStatus sts;

    if (!m_pDdiData)
    {
        m_pDdiData = new MfxHwMJpegEncode::ExecuteBuffers;
    }
  
    sts = m_pDdiData->Init(&par);
    MFX_CHECK_STS(sts);

    m_counter = 0;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest& request)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    if (!m_infoQueried)
    {
        ENCODE_FORMAT_COUNT encodeFormatCount;
        encodeFormatCount.CompressedBufferInfoCount = 0;
        encodeFormatCount.UncompressedFormatCount   = 0;

        HRESULT hr = m_pAuxDevice->Execute(ENCODE_FORMAT_COUNT_ID, (void*)0, 0, &encodeFormatCount, sizeof(encodeFormatCount));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
        m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

        ENCODE_FORMATS encodeFormats;
        encodeFormats.CompressedBufferInfoSize = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
        encodeFormats.UncompressedFormatSize   = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
        encodeFormats.pCompressedBufferInfo    = &m_compBufInfo[0];
        encodeFormats.pUncompressedFormats     = &m_uncompBufInfo[0];

        hr = m_pAuxDevice->Execute(ENCODE_FORMATS_ID, (void*)0, 0, &encodeFormats, sizeof(encodeFormats));
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
    request.Info.FourCC = m_compBufInfo[i].CompressedFormats;

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncodeCaps(ENCODE_CAPS_JPEG & caps)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    caps = m_caps;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryEncCtrlCaps(ENCODE_ENC_CTRL_CAPS& caps)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    caps = m_capsQuery;
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::SetEncCtrlCaps(
                                     ENCODE_ENC_CTRL_CAPS const & caps)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    m_capsGet = caps; // DDI spec: "The application should use the same structure
                      // returned in a previous ENCODE_ENC_CTRL_GET_ID command."

    HRESULT hr = m_pAuxDevice->Execute(ENCODE_ENC_CTRL_SET_ID, &m_capsGet, sizeof(m_capsGet));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Register( mfxMemId     /*memId*/, D3DDDIFORMAT /*type*/)
{
    return MFX_ERR_NONE;

} // mfxStatus D3D9Encoder::Register(...)

mfxStatus D3D9Encoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    EmulSurfaceRegister surfaceReg;
    memset(&surfaceReg, 0, sizeof(surfaceReg));
    surfaceReg.type = type;
    surfaceReg.num_surf = response.NumFrameActual;

    MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);
    for (mfxU32 i = 0; i < response.NumFrameActual; i++)
    {
        mfxStatus sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *)&surfaceReg.surface[i]);
        MFX_CHECK_STS(sts);
        MFX_CHECK(surfaceReg.surface[i] != 0, MFX_ERR_UNSUPPORTED);
    }

    HRESULT hr = m_pAuxDevice->BeginFrame(surfaceReg.surface[0], &surfaceReg);
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    m_pAuxDevice->EndFrame(0);

    if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {
        // Reserve space for feedback reports.
        m_feedbackUpdate.resize(response.NumFrameActual);
        m_feedbackCached.Reset(response.NumFrameActual);
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Execute(DdiTask &task, mfxHDL surface)
{
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);
    ExecuteBuffers *pExecuteBuffers = m_pDdiData;

    const mfxU32 NumCompBuffer = 16;
    ENCODE_COMPBUFFERDESC encodeCompBufferDesc[NumCompBuffer];
    memset(&encodeCompBufferDesc, 0, sizeof(encodeCompBufferDesc));

    ENCODE_EXECUTE_PARAMS encodeExecuteParams;
    memset(&encodeExecuteParams, 0, sizeof(encodeExecuteParams));
    encodeExecuteParams.NumCompBuffers = 0;
    encodeExecuteParams.pCompressedBuffers = encodeCompBufferDesc;

    // FIXME: need this until production driver moves to DDI 0.87
    encodeExecuteParams.pCipherCounter = 0;
    encodeExecuteParams.PavpEncryptionMode.eCounterMode = 0;
    encodeExecuteParams.PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;

    UINT& bufCnt = encodeExecuteParams.NumCompBuffers;

    task.m_statusReportNumber = (m_counter ++) << 8;
    pExecuteBuffers->m_pps.StatusReportFeedbackNumber = task.m_statusReportNumber;

    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_PPSDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_pps));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_pps;
    bufCnt++;

    mfxU32 bitstream = task.m_idxBS;
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(bitstream));
    encodeCompBufferDesc[bufCnt].pCompBuffer = &bitstream;
    bufCnt++;

    mfxU16 i = 0;

    for (i = 0; i < pExecuteBuffers->m_pps.NumQuantTable; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_QUANTDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_dqt_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dqt_list[i];
        bufCnt++;
    }

    for (i = 0; i < pExecuteBuffers->m_pps.NumCodingTable; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_HUFFTBLDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_dht_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_dht_list[i];
        bufCnt++;
    }

    for (i = 0; i < pExecuteBuffers->m_pps.NumScan; i++)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTEL_JPEGENCODE_SCANDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(sizeof(pExecuteBuffers->m_scan_list[i]));
        encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_scan_list[i];
        bufCnt++;
    }

    if (pExecuteBuffers->m_payload_data_present)
    {
        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PAYLOADDATA;
        encodeCompBufferDesc[bufCnt].DataSize = mfxU32(pExecuteBuffers->m_payload.size);
        encodeCompBufferDesc[bufCnt].pCompBuffer = (void*)pExecuteBuffers->m_payload.data;
        bufCnt++;
    }

    try
    {
        HRESULT hr = m_pAuxDevice->BeginFrame((IDirect3DSurface9 *)surface, 0);
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        hr = m_pAuxDevice->Execute(ENCODE_ENC_PAK_ID, &encodeExecuteParams, sizeof(encodeExecuteParams));
        MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        HANDLE handle;
        m_pAuxDevice->EndFrame(&handle);
    }
    catch (...)
    {
        return MFX_ERR_DEVICE_FAILED;
    }

    return MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::QueryStatus(DdiTask & task)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "QueryStatus");
    MFX_CHECK_WITH_ASSERT(m_pAuxDevice, MFX_ERR_NOT_INITIALIZED);

    // After SNB once reported ENCODE_OK for a certain feedbackNumber
    // it will keep reporting ENCODE_NOTAVAILABLE for same feedbackNumber.
    // As we won't get all bitstreams we need to cache all other statuses. 

    // first check cache.
    const ENCODE_QUERY_STATUS_PARAMS* feedback = m_feedbackCached.Hit(task.m_statusReportNumber);

    // if task is not in cache then query its status
    if (feedback == 0 || feedback->bStatus != ENCODE_OK)
    {
        try
        {
            HRESULT hr = m_pAuxDevice->Execute(
                ENCODE_QUERY_STATUS_ID,
                (void *)0,
                0,
                &m_feedbackUpdate[0],
                (mfxU32)m_feedbackUpdate.size() * sizeof(m_feedbackUpdate[0]));
            MFX_CHECK(hr != D3DERR_WASSTILLDRAWING, MFX_WRN_DEVICE_BUSY);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }
        catch (...)
        {
            return MFX_ERR_DEVICE_FAILED;
        }

        // Put all with ENCODE_OK into cache.
        m_feedbackCached.Update(m_feedbackUpdate);

        feedback = m_feedbackCached.Hit(task.m_statusReportNumber);
        MFX_CHECK(feedback != 0, MFX_ERR_DEVICE_FAILED);
    }

    switch (feedback->bStatus)
    {
    case ENCODE_OK:
        task.m_bsDataLength = feedback->bitstreamSize;
        m_feedbackCached.Remove(task.m_statusReportNumber);
        return MFX_ERR_NONE;

    case ENCODE_NOTREADY:
        return MFX_WRN_DEVICE_BUSY;

    case ENCODE_NOTAVAILABLE:
    case ENCODE_ERROR:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }
}

mfxStatus D3D9Encoder::UpdateBitstream(
    mfxMemId       MemId,
    DdiTask      & task)
{
    mfxU8      * bsData    = task.bs->Data + task.bs->DataOffset + task.bs->DataLength;
    IppiSize     roi       = {task.m_bsDataLength, 1};
    mfxFrameData bitstream = {0};

    m_core->LockFrame(MemId, &bitstream);
    MFX_CHECK(bitstream.Y != 0, MFX_ERR_LOCK_MEMORY);

    IppStatus sts = ippiCopyManaged_8u_C1R(
        (Ipp8u *)bitstream.Y, task.m_bsDataLength,
        bsData, task.m_bsDataLength,
        roi, IPP_NONTEMPORAL_LOAD);
    assert(sts == ippStsNoErr);

    task.bs->DataLength += task.m_bsDataLength;
    m_core->UnlockFrame(MemId, &bitstream);

    return (sts != ippStsNoErr) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
}

mfxStatus D3D9Encoder::Destroy()
{
    mfxStatus sts = MFX_ERR_NONE;

    if (m_pAuxDevice)
    {
        sts = m_pAuxDevice->ReleaseAccelerationService();
        m_pAuxDevice->Release();
        delete m_pAuxDevice;
        m_pAuxDevice = 0;
    }
    if (m_pDdiData)
    {
        m_pDdiData->Close();
        delete m_pDdiData;
        m_pDdiData = 0;
    }

    return sts;
}

#endif // (MFX_ENABLE_MJPEG_VIDEO_ENCODE)
/* EOF */