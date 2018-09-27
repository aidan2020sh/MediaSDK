//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h264_encode_struct_vaapi.h"
#include "mfxplugin++.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_bs.h"
#if defined(MFX_ENABLE_MFE) && defined(MFX_VA_WIN)
#include "mfx_mfe_adapter_dxva.h"
#endif

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
#include "mfx_win_event_cache.h"
#endif

#include <memory>
#include <vector>

//#define HEADER_PACKING_TEST
#if defined(_DEBUG) && defined(_WIN32)
    #define DDI_TRACE
#endif

#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#define D3DDDIFMT_YU12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('Y', 'U', '1', '2'))

namespace MfxHwH265Encode
{

// GUIDs from DDI for HEVC Encoder spec 0.956
static const GUID DXVA2_Intel_Encode_HEVC_Main =
{ 0x28566328, 0xf041, 0x4466, { 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b } };

static const GUID DXVA2_Intel_Encode_HEVC_Main10 =
{ 0x6b4a94db, 0x54fe, 0x4ae1, { 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x00, 0x46, 0x00 } };

static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main =
{ 0xb8b28e0c, 0xecab, 0x4217, { 0x8c, 0x82, 0xea, 0xaa, 0x97, 0x55, 0xaa, 0xf0 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main10 =
{ 0x8732ecfd, 0x9747, 0x4897, { 0xb4, 0x2a, 0xe5, 0x34, 0xf9, 0xff, 0x2b, 0x7a } };

static const GUID DXVA2_Intel_Encode_HEVC_Main422 =
{ 0x056a6e36, 0xf3a8, 0x4d00, { 0x96, 0x63, 0x7e, 0x94, 0x30, 0x35, 0x8b, 0xf9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main422_10 =
{ 0xe139b5ca, 0x47b2, 0x40e1, { 0xaf, 0x1c, 0xad, 0x71, 0xa6, 0x7a, 0x18, 0x36 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444 =
{ 0x5415a68c, 0x231e, 0x46f4, { 0x87, 0x8b, 0x5e, 0x9a, 0x22, 0xe9, 0x67, 0xe9 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444_10 =
{ 0x161be912, 0x44c2, 0x49c0, { 0xb6, 0x1e, 0xd9, 0x46, 0x85, 0x2b, 0x32, 0xa1 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422 =
{ 0xcee393ab, 0x1030, 0x4f7b, { 0x8d, 0xbc, 0x55, 0x62, 0x9c, 0x72, 0xf1, 0x7e } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main422_10 =
{ 0x580da148, 0xe4bf, 0x49b1, { 0x94, 0x3b, 0x42, 0x14, 0xab, 0x05, 0xa6, 0xff } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444 =
{ 0x87b2ae39, 0xc9a5, 0x4c53, { 0x86, 0xb8, 0xa5, 0x2d, 0x7e, 0xdb, 0xa4, 0x88 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_Main444_10 =
{ 0x10e19ac8, 0xbf39, 0x4443, { 0xbe, 0xc3, 0x1b, 0x0c, 0xbf, 0xe4, 0xc7, 0xaa } };

#ifdef PRE_SI_TARGET_PLATFORM_GEN12
static const GUID DXVA2_Intel_Encode_HEVC_Main12 =
{ 0xd6d6bc4f, 0xd51a, 0x4712, { 0x97, 0xe8, 0x75, 0x9, 0x17, 0xc8, 0x60, 0xfd } };
static const GUID DXVA2_Intel_Encode_HEVC_Main422_12 =
{ 0x7fef652d, 0x3233, 0x44df, { 0xac, 0xf7, 0xec, 0xfb, 0x58, 0x4d, 0xab, 0x35 } };
static const GUID DXVA2_Intel_Encode_HEVC_Main444_12 =
{ 0xf8fa34b7, 0x93f5, 0x45a4, { 0xbf, 0xc0, 0x38, 0x17, 0xce, 0xe6, 0xbb, 0x93 } };

// GUIDs from DDI for HEVC Encoder spec 0.991
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main =
{ 0x2dec00c7, 0x21ee, 0x4bf8, { 0x8f, 0x0e, 0x77, 0x3f, 0x11, 0xf1, 0x26, 0xa2 } };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10 =
{ 0xc35153a0, 0x23c0, 0x4a81, { 0xb3, 0xbb, 0x6a, 0x13, 0x26, 0xf2, 0xb7, 0x6b} };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444 =
{ 0xa33fd0ec, 0xa9d3, 0x4c21, { 0x92, 0x76, 0xc2, 0x41, 0xcc, 0x90, 0xf6, 0xc7} };
static const GUID DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10 =
{ 0x310e59d2, 0x7ea4, 0x47bb, { 0xb3, 0x19, 0x50, 0x0e, 0x78, 0x85, 0x53, 0x36} };
#endif

GUID GetGUID(MfxVideoParam const & par);

const GUID GuidTable[3][3][3] =
{
    // LowPower = OFF
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main,
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_Encode_HEVC_Main10,
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444_10
        },
        // BitDepthLuma = 12
        {
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
            /*420*/ DXVA2_Intel_Encode_HEVC_Main12,
            /*422*/ DXVA2_Intel_Encode_HEVC_Main422_12,
            /*444*/ DXVA2_Intel_Encode_HEVC_Main444_12
#endif
        }
    },
    // LowPower = ON, SCC = OFF
    {
        // BitDepthLuma = 8
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main,
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_Main10,
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_Main422_10,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_Main444_10
        },
        // BitDepthLuma = 12
        {
        }
    },
#if defined(MFX_ENABLE_HEVCE_SCC)
    // LowPower = ON, SCC = ON
    {
        // BitDepthLuma = 8
        {
            //DXVA2_Intel_Encode_HEVC_Main,
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main,
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444
        },
        // BitDepthLuma = 10
        {
            /*420*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10,
            /*422*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main10,
            /*444*/ DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main444_10
        },
        // BitDepthLuma = 12
        {
        }
    }

#endif
};
#if defined(MFX_ENABLE_MFE) && defined(MFX_VA_WIN)
MFEDXVAEncoder* CreatePlatformMFEEncoder(VideoCORE* core);
#endif
mfxStatus HardcodeCaps(ENCODE_CAPS_HEVC& caps, VideoCORE* pCore);

class DriverEncoder;

typedef enum tagENCODER_TYPE
{
    ENCODER_DEFAULT = 0
    , ENCODER_REXT
} ENCODER_TYPE;

DriverEncoder* CreatePlatformH265Encoder(VideoCORE* core, ENCODER_TYPE type = ENCODER_DEFAULT);
mfxStatus QueryHwCaps(VideoCORE* core, GUID guid, ENCODE_CAPS_HEVC & caps, MfxVideoParam const & par);
mfxStatus QueryMbProcRate(VideoCORE* core, mfxVideoParam const & par, mfxU32(&mbPerSec)[16], const MfxVideoParam * in);
mfxStatus CheckHeaders(MfxVideoParam const & par, ENCODE_CAPS_HEVC const & caps);

#if MFX_EXTBUFF_CU_QP_ENABLE
mfxStatus FillCUQPDataDDI(Task& task, MfxVideoParam &par, VideoCORE& core, mfxFrameInfo &CUQPFrameInfo);
#endif

enum
{
    MAX_DDI_BUFFERS = 4, //sps, pps, slice, bs
};

class DriverEncoder
{
public:

    virtual ~DriverEncoder(){}

    virtual
    mfxStatus CreateAuxilliaryDevice(
                    VideoCORE * core,
                    GUID        guid,
                    mfxU32      width,
                    mfxU32      height,
                    MfxVideoParam const & par) = 0;

    virtual
    mfxStatus CreateAccelerationService(
        MfxVideoParam const & par) = 0;

    virtual
    mfxStatus Reset(
        MfxVideoParam const & par, bool bResetBRC) = 0;

    virtual
    mfxStatus Register(
        mfxFrameAllocResponse & response,
        D3DDDIFORMAT            type) = 0;

    virtual
    mfxStatus Execute(
        Task const &task,
        mfxHDLPair pair) = 0;

    virtual
    mfxStatus QueryCompBufferInfo(
        D3DDDIFORMAT           type,
        mfxFrameAllocRequest & request) = 0;

    virtual
    mfxStatus QueryEncodeCaps(
        ENCODE_CAPS_HEVC & caps) = 0;

    virtual
    mfxStatus QueryMbPerSec(
        mfxVideoParam const & par,
        mfxU32(&mbPerSec)[16]) = 0;

    virtual
    mfxStatus QueryStatus(
        Task & task ) = 0;

    virtual
    mfxStatus Destroy() = 0;

    virtual
    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut) = 0;

    // Functions below are used in HEVC FEI encoder

    virtual mfxStatus PreSubmitExtraStage(Task const & /*task*/)
    {
        return MFX_ERR_NONE;
    }

    virtual mfxStatus PostQueryExtraStage()
    {
        return MFX_ERR_NONE;
    }
#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
    std::unique_ptr<EventCache> m_EventCache;
#endif
};

class DDIHeaderPacker
{
public:
    DDIHeaderPacker();
    ~DDIHeaderPacker();

    void Reset(MfxVideoParam const & par);

    void ResetPPS(MfxVideoParam const & par);

    ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut);
    ENCODE_PACKEDHEADER_DATA* PackSliceHeader(Task const & task,
        mfxU32 id,
        mfxU32* qpd_offset,
        bool dyn_slice_size = false,
        mfxU32* sao_offset = 0,
        mfxU16* ssh_start_len = 0,
        mfxU32* ssh_offset = 0,
        mfxU32* pwt_offset = 0,
        mfxU32* pwt_length = 0);
    ENCODE_PACKEDHEADER_DATA* PackSkippedSlice(Task const & task, mfxU32 id, mfxU32* qpd_offset);

    inline mfxU32 MaxPackedHeaders() { return (mfxU32)m_buf.size(); }

private:
    std::vector<ENCODE_PACKEDHEADER_DATA>           m_buf;
    std::vector<ENCODE_PACKEDHEADER_DATA>::iterator m_cur;
    HeaderPacker m_packer;

    void NewHeader();
};

#if defined(_WIN32) || defined(_WIN64)

class FeedbackStorage
{
public:
    typedef ENCODE_QUERY_STATUS_PARAMS Feedback;

    FeedbackStorage()
        : m_pool_size(0)
        , m_fb_size(0)
        , m_type(QUERY_STATUS_PARAM_FRAME)
    {
    }

    inline ENCODE_QUERY_STATUS_PARAMS& operator[] (size_t i) const
    {
        return *(ENCODE_QUERY_STATUS_PARAMS*)&m_buf[i * m_fb_size];
    }

    inline size_t size() const
    {
        return m_pool_size;
    }

    inline mfxU32 feedback_size()
    {
        return m_fb_size;
    }

    void Reset(mfxU16 cacheSize, ENCODE_QUERY_STATUS_PARAM_TYPE fbType, mfxU32 maxSlices);

    const Feedback* Get(mfxU32 feedbackNumber) const;

    mfxStatus Update();

    inline void CacheFeedback(Feedback *fb_dst, Feedback *fb_src);

    mfxStatus Remove(mfxU32 feedbackNumber);

private:
    std::vector<mfxU8> m_buf;
    std::vector<mfxU8> m_buf_cache;
    std::vector<mfxU16> m_ssizes;
    std::vector<mfxU16> m_ssizes_cache;
    mfxU32 m_fb_size;
    mfxU16 m_pool_size;
    ENCODE_QUERY_STATUS_PARAM_TYPE m_type;
};

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps);

void FillPpsBuffer(
    Task const & task,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC & pps,
    std::vector<ENCODE_RECT> & dirtyRects);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC> & slice);

void FillSpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & /*caps*/,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT & sps);

void FillPpsBuffer(
    MfxVideoParam const & par,
    ENCODE_CAPS_HEVC const & caps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT & pps);

void FillSliceBuffer(
    MfxVideoParam const & par,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & sps,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

void FillSliceBuffer(
    Task const & task,
    ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & /*sps*/,
    ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & /*pps*/,
    std::vector<ENCODE_SET_SLICE_HEADER_HEVC_REXT> & slice);

#endif //defined(_WIN32) || defined(_WIN64)

}; // namespace MfxHwH265Encode
#endif
