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

#if defined(_WIN32) || defined(_WIN64)

#include "auxiliary_device.h"
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_ddi_trace.h"

namespace MfxHwH265Encode
{

class AuxiliaryDevice : public ::AuxiliaryDevice
{
public:
    using ::AuxiliaryDevice::Execute;

    template <typename T, typename U>
    HRESULT Execute(mfxU32 func, T& in, U& out)
    {
        return ::AuxiliaryDevice::Execute(func, &in, sizeof(in), &out, sizeof(out));
    }

    template <typename T>
    HRESULT Execute(mfxU32 func, T& in, void*)
    {
        return ::AuxiliaryDevice::Execute(func, &in, sizeof(in), 0, 0);
    }

    template <typename U>
    HRESULT Execute(mfxU32 func, void*, U& out)
    {
        return ::AuxiliaryDevice::Execute(func, 0, 0, &out, sizeof(out));
    }
};


template<class DDI_SPS, class DDI_PPS, class DDI_SLICE>
class D3D9Encoder : public DriverEncoder, DDIHeaderPacker, DDITracer
{
public:
    D3D9Encoder();

    virtual
    ~D3D9Encoder();

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
    MFXCoreInterface*              m_core;
    std::auto_ptr<AuxiliaryDevice> m_auxDevice;

    GUID                 m_guid;
    mfxU32               m_width;
    mfxU32               m_height;
    ENCODE_CAPS_HEVC     m_caps;
    ENCODE_ENC_CTRL_CAPS m_capsQuery;
    ENCODE_ENC_CTRL_CAPS m_capsGet;
    bool                 m_infoQueried;
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
    bool                 m_widi;
#endif
    mfxU32               m_maxSlices;

    DDI_SPS                                     m_sps;
    DDI_PPS                                     m_pps;
    std::vector<DDI_SLICE>                      m_slice;
    std::vector<ENCODE_COMP_BUFFER_INFO>        m_compBufInfo;
    std::vector<D3DDDIFORMAT>                   m_uncompBufInfo;
    std::vector<ENCODE_COMPBUFFERDESC>          m_cbd;
    FeedbackStorage                             m_feedbackUpdate;
    CachedFeedback                              m_feedbackCached;
};

#if defined(PRE_SI_TARGET_PLATFORM_GEN11)
typedef D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT, ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT, ENCODE_SET_SLICE_HEADER_HEVC_REXT> D3D9EncoderREXT;
#endif //defined(PRE_SI_TARGET_PLATFORM_GEN11)
typedef D3D9Encoder<ENCODE_SET_SEQUENCE_PARAMETERS_HEVC, ENCODE_SET_PICTURE_PARAMETERS_HEVC, ENCODE_SET_SLICE_HEADER_HEVC> D3D9EncoderDefault;

}; // namespace MfxHwH265Encode

#endif // #if defined(_WIN32) || defined(_WIN64)