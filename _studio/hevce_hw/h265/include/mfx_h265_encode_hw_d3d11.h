//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2017 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#if defined(_WIN32) || defined(_WIN64)

#include "auxiliary_device.h"
#include "mfx_h265_encode_hw_ddi.h"
#include <d3d11.h>
#include <atlbase.h>

namespace MfxHwH265Encode
{

template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
class D3D11Encoder : public DriverEncoder, DDIHeaderPacker, DDITracer
{
public:
    D3D11Encoder();

    virtual
    ~D3D11Encoder();

    virtual
    mfxStatus CreateAuxilliaryDevice(
        MFXCoreInterface * core,
        GUID        guid,
        mfxU32      width,
        mfxU32      height);

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par);

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par, bool bResetBRC);

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type);

    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDL surface);

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request);

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps);


    virtual
    mfxStatus QueryStatus(
        Task & task);

    virtual
    mfxStatus Destroy();

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut)
    {
        return DDIHeaderPacker::PackHeader(task, nut);
    }

private:
    MFXCoreInterface*                           m_core;
    GUID                                        m_guid;
    mfxU32                                      m_width;
    mfxU32                                      m_height;

    CComPtr<ID3D11DeviceContext>                m_context;
    CComPtr<ID3D11VideoDecoder>                 m_vdecoder;
    CComQIPtr<ID3D11VideoDevice>                m_vdevice;
    CComQIPtr<ID3D11VideoContext>               m_vcontext;

    ENCODE_CAPS_HEVC                            m_caps;
    ENCODE_ENC_CTRL_CAPS                        m_capsQuery;
    ENCODE_ENC_CTRL_CAPS                        m_capsGet;
    bool                                        m_infoQueried;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    bool                                        m_pavp;
    bool                                        m_widi;
#endif
    mfxU32                                      m_maxSlices;

    DDI_SPS                                     m_sps;
    DDI_PPS                                     m_pps;
    std::vector<DDI_SLICE>                      m_slice;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
    std::vector<ENCODE_COMPBUFFERDESC>          m_cbd;
    std::vector<mfxHDLPair>                     m_reconQueue;
    std::vector<mfxHDLPair>                     m_bsQueue;
    FeedbackStorage                             m_feedbackPool;

    std::vector<ENCODE_RECT>                    m_dirtyRects;

    HRESULT DecoderExtension(D3D11_VIDEO_DECODER_EXTENSION const & ext);
};

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
typedef D3D11Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT, ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT, ENCODE_SET_SLICE_HEADER_HEVC_REXT> D3D11EncoderREXT;
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
typedef D3D11Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC, ENCODE_SET_PICTURE_PARAMETERS_HEVC, ENCODE_SET_SLICE_HEADER_HEVC> D3D11EncoderDefault;

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)
#endif
