//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2015-2018 Intel Corporation. All Rights Reserved.
//
#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#pragma once

#include "mfx_common.h"

#include "umc_mutex.h"

#include <va/va.h>
#include <va/va_enc_hevc.h>
#include "vaapi_ext_interface.h"

#include "mfx_h265_encode_hw_utils.h"
#include "mfx_h265_encode_hw_ddi.h"
#include "mfx_h265_encode_hw_ddi_trace.h"
#include <vector>
#include <map>
#include <algorithm>

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

namespace MfxHwH265Encode
{

class VABuffersHandler
{
public:
    enum VA_BUFFER_STORAGE_ID
    {
          VABID_SPS = 0
        , VABID_PPS
        , VABID_Slice
        , VABID_HRD
        , VABID_RateParam
        , VABID_BRCParallel
        , VABID_FrameRate
        , VABID_QualityLevel
        , VABID_MaxFrameSize
#if defined (MFX_EXTBUFF_GPU_HANG_ENABLE)
        , VABID_TriggerCodecHang
#endif
        , VABID_QpBuffer

        , VABID_PACKED_AUD_H
        , VABID_PACKED_AUD
        , VABID_PACKED_VPS_H
        , VABID_PACKED_VPS
        , VABID_PACKED_SPS_H
        , VABID_PACKED_SPS
        , VABID_PACKED_PPS_H
        , VABID_PACKED_PPS
        , VABID_PACKED_SEI_H
        , VABID_PACKED_SEI
        , VABID_PACKED_Slice_H
        , VABID_PACKED_Slice
        , VABID_PACKED_SkippedSlice_H
        , VABID_PACKED_SkippedSlice
        , VABID_PACKED_SkipBuffer

        , VABID_ROI

        , VABID_END_OF_LIST // Remain this item last in the list
    };

    VABuffersHandler()
    {
        m_pool.resize(1, 0);
        m_vaDisplay = 0;
    }

    ~VABuffersHandler()
    {
        VABuffersDestroy();
    }

    VABufferID& VABuffersNew(mfxU32 id, mfxU32 pool, mfxU32 num);
    void VABuffersDestroy();
    void VABuffersDestroyPool(mfxU32 pool);

    inline VABufferID& VABufferNew(mfxU32 id, mfxU32 pool) { return  VABuffersNew(id, pool, 1); }
    inline VABufferID* VABuffersBegin(mfxU32 pool) { return &(*_PoolBegin(pool)); }
    inline VABufferID* VABuffersEnd  (mfxU32 pool) { return &(*_PoolEnd  (pool)); }


protected:
    VADisplay                   m_vaDisplay;

private:
    std::vector<size_t>         m_pool;
    std::map<mfxU32, mfxU32>    m_poolMap;
    std::vector<VABufferID>     m_buf;
    std::vector<mfxU32>         m_id;

    void _CheckPool(mfxU32 pool);
    inline std::vector<VABufferID>::iterator _PoolBegin(mfxU32 pool) { _CheckPool(pool); return m_buf.begin() + m_pool[m_poolMap[pool]]; };
    inline std::vector<VABufferID>::iterator _PoolEnd  (mfxU32 pool) { _CheckPool(pool); return m_buf.begin() + m_pool[m_poolMap[pool] + 1]; };
};

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type);

mfxStatus SetHRD(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id);

mfxStatus SetQualityLevelParams(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & qualityParams_id);

void FillConstPartOfPps(
    MfxHwH265Encode::MfxVideoParam const & par,
    VAEncPictureParameterBufferHEVC & pps);

mfxStatus SetFrameRate(
    MfxHwH265Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id);

mfxStatus SetSkipFrame(
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID&  skipParam_id,
    mfxU8 skipFlag,
    mfxU8 numSkipFrames,
    mfxU32 sizeSkipFrames);

    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;
        mfxU32 size;
    } ExtVASurface;

#if MFX_EXTBUFF_CU_QP_ENABLE
    class CUQPMap
    {
    public:
        mfxU32                            m_width;
        mfxU32                            m_height;
        mfxU32                            m_pitch;
        mfxU32                            m_h_aligned;

        mfxU32                            m_block_width;
        mfxU32                            m_block_height;
        std::vector<mfxI8>                m_buffer;

        CUQPMap():
            m_width(0),
            m_height(0),
            m_pitch(0),
            m_h_aligned(0),
            m_block_width(0),
            m_block_height(0) {}

            void Init(mfxU32 picWidthInLumaSamples, mfxU32 picHeightInLumaSamples);
    };
#endif
    class VAAPIEncoder : public DriverEncoder, protected DDIHeaderPacker, protected VABuffersHandler
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE * core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height,
            MfxVideoParam const & par);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par, bool bBRCReset);

        virtual
        mfxStatus Register(
            mfxMemId memId,
            D3DDDIFORMAT type);

        // 2 -> 1
        virtual
        mfxStatus Register(
            mfxFrameAllocResponse& response,
            D3DDDIFORMAT type);

        virtual
        mfxStatus Execute(
            Task            const & task,
            mfxHDLPair          pair);

        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS_HEVC& caps);

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32(&mbPerSec)[16]);

        virtual
        mfxStatus QueryStatus(Task & task);

        virtual
        mfxStatus Destroy();

        virtual
        ENCODE_PACKEDHEADER_DATA* PackHeader(Task const & task, mfxU32 nut)
        {
            return DDIHeaderPacker::PackHeader(task, nut);
        }

        virtual
        VAEntrypoint GetVAEntryPoint(MfxVideoParam const & par)
        {
#if (MFX_VERSION >= 1025)
            return (IsOn(par.mfx.LowPower) && par.m_platform >= MFX_HW_CNL) ?
                    VAEntrypointEncSliceLP : VAEntrypointEncSlice;
#else
            return VAEntrypointEncSlice;
#endif
        }

    protected:
        virtual
        mfxStatus ConfigureExtraVAattribs(std::vector<VAConfigAttrib> & /* attrib */)
        {
            return MFX_ERR_NONE;
        }

        virtual
        mfxStatus CheckExtraVAattribs(std::vector<VAConfigAttrib> & /* attrib */)
        {
            return MFX_ERR_NONE;
        }

        VAAPIEncoder(const VAAPIEncoder&);
        VAAPIEncoder& operator=(const VAAPIEncoder&);

        void FillSps(MfxVideoParam const & par, VAEncSequenceParameterBufferHEVC & sps);

        VideoCORE*    m_core;
        MfxVideoParam m_videoParam;
        mfxU8  m_numSkipFrames;
        mfxU32 m_sizeSkipFrames;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        VAEncSequenceParameterBufferHEVC m_sps;
        VAEncPictureParameterBufferHEVC  m_pps;
        std::vector<VAEncSliceParameterBufferHEVC> m_slice;

        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_bsQueue;
        std::vector<ExtVASurface> m_reconQueue;


        mfxU32 m_width;
        mfxU32 m_height;
        ENCODE_CAPS_HEVC m_caps;

#if MFX_EXTBUFF_CU_QP_ENABLE
        CUQPMap    m_cuqpMap;
#endif
        std::vector<VAEncROI> m_arrayVAEncROI;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 26 + 5;

        UMC::Mutex m_guard;
        HeaderPacker m_headerPacker;
    };
}
#endif
