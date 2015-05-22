/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2011-2015 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_H264_ENCODE_VAAPI__H
#define __MFX_H264_ENCODE_VAAPI__H

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined (MFX_VA_LINUX)

#include "umc_mutex.h"

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_h264.h>
#include "vaapi_ext_interface.h"

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
//#include <va/vendor/va_intel_fei.h>
//#include <va/vendor/va_intel_statistics.h>
#endif

#include "mfx_h264_encode_interface.h"
#include "mfx_h264_encode_hw_utils.h"

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

#define VAConfigAttribInputTiling  -1  // Inform the app what kind of tiling format supported by driver


mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl);

VAProfile ConvertProfileTypeMFX2VAAPI(mfxU32 type);

mfxStatus SetHRD(
    MfxHwH264Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & hrdBuf_id);

mfxStatus SetPrivateParams(
    MfxHwH264Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & privateParams_id,
    mfxEncodeCtrl const * pCtrl = 0);

void FillConstPartOfPps(
    MfxHwH264Encode::MfxVideoParam const & par,
    VAEncPictureParameterBufferH264 & pps);

mfxStatus SetFrameRate(
    MfxHwH264Encode::MfxVideoParam const & par,
    VADisplay    vaDisplay,
    VAContextID  vaContextEncode,
    VABufferID & frameRateBuf_id);


namespace MfxHwH264Encode
{
    // map feedbackNumber <-> VASurface
    typedef struct
    {
        VASurfaceID surface;
        mfxU32 number;
        mfxU32 idxBs;
        mfxU32 size; // valid only if Surface ID == VA_INVALID_SURFACE (skipped frames)
#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC) || defined(MFX_ENABLE_H264_VIDEO_FEI_ENCPAK)
        VASurfaceID mv;
        VASurfaceID mbstat;
        VASurfaceID mbcode;
#endif
    } ExtVASurface;

    class VAAPIEncoder : public DriverEncoder
    {
    public:
        VAAPIEncoder();

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus CreateAuxilliaryDevice(
            VideoCORE* core,
            GUID       guid,
            mfxU32     width,
            mfxU32     height,
            bool       isTemporal = false);

        virtual
        mfxStatus CreateAccelerationService(
            MfxVideoParam const & par);

        virtual
        mfxStatus Reset(
            MfxVideoParam const & par);

        // empty  for Lin
        virtual
        mfxStatus Register(
            mfxMemId memId,
            D3DDDIFORMAT type);

        // 2 -> 1
        virtual
        mfxStatus Register(
            mfxFrameAllocResponse& response,
            D3DDDIFORMAT type);

        // (mfxExecuteBuffers& data)
        virtual
        mfxStatus Execute(
            mfxHDL          surface,
            DdiTask const & task,
            mfxU32          fieldId,
            PreAllocatedVector const & sei);

        // recomendation from HW
        virtual
        mfxStatus QueryCompBufferInfo(
            D3DDDIFORMAT type,
            mfxFrameAllocRequest& request);

        virtual
        mfxStatus QueryEncodeCaps(
            ENCODE_CAPS& caps);

        virtual
        mfxStatus QueryMbPerSec(
            mfxVideoParam const & par,
            mfxU32              (&mbPerSec)[16]);

        virtual
        mfxStatus QueryInputTilingSupport(
            mfxVideoParam const & par,
            mfxU32               &inputTiling);

        virtual
        mfxStatus QueryStatus(
            DdiTask & task,
            mfxU32    fieldId);

        virtual
        mfxStatus Destroy();

        void ForceCodingFunction (mfxU16 /*codingFunction*/)
        {
            // no need in it on Linux
        }

        virtual
        mfxStatus QueryHWGUID(
            VideoCORE * core,
            GUID        guid,
            bool        isTemporal);

    protected:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        void FillSps( MfxVideoParam const & par, VAEncSequenceParameterBufferH264 & sps);

        VideoCORE*    m_core;
        MfxVideoParam m_videoParam;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay    m_vaDisplay;
        VAContextID  m_vaContextEncode;
        VAConfigID   m_vaConfig;

        // encode params (extended structures)
        VAEncSequenceParameterBufferH264 m_sps;
        VAEncPictureParameterBufferH264  m_pps;
        std::vector<VAEncSliceParameterBufferH264> m_slice;

        // encode buffer to send vaRender()
        VABufferID m_spsBufferId;
        VABufferID m_hrdBufferId;
        VABufferID m_rateParamBufferId; // VAEncMiscParameterRateControl
        VABufferID m_frameRateId; // VAEncMiscParameterFrameRate
        VABufferID m_maxFrameSizeId; // VAEncMiscParameterFrameRate
        VABufferID m_quantizationId;  // VAEncMiscParameterQuantization
        VABufferID m_rirId;           // VAEncMiscParameterRIR
        VABufferID m_privateParamsId; // VAEncMiscParameterPrivate
        VABufferID m_miscParameterSkipBufferId; // VAEncMiscParameterSkipFrame
        VABufferID m_roiBufferId;
        VABufferID m_ppsBufferId;
        VABufferID m_mbqpBufferId;
        VABufferID m_mbNoSkipBufferId;
        std::vector<VABufferID> m_sliceBufferId;

        VABufferID m_packedAudHeaderBufferId;
        VABufferID m_packedAudBufferId;
        VABufferID m_packedSpsHeaderBufferId;
        VABufferID m_packedSpsBufferId;
        VABufferID m_packedPpsHeaderBufferId;
        VABufferID m_packedPpsBufferId;
        VABufferID m_packedSeiHeaderBufferId;
        VABufferID m_packedSeiBufferId;
        VABufferID m_packedSkippedSliceHeaderBufferId;
        VABufferID m_packedSkippedSliceBufferId;
        std::vector<VABufferID> m_packeSliceHeaderBufferId;
        std::vector<VABufferID> m_packedSliceBufferId;
        std::vector<VABufferID> m_vaFeiMBStatId;
        std::vector<VABufferID> m_vaFeiMVOutId;
        std::vector<VABufferID> m_vaFeiMCODEOutId;

        std::vector<ExtVASurface> m_feedbackCache;
        std::vector<ExtVASurface> m_bsQueue;
        std::vector<ExtVASurface> m_reconQueue;

        mfxU32 m_width;
        mfxU32 m_height;
        mfxU32 m_userMaxFrameSize;  // current MaxFrameSize from user.
        mfxU32 m_mbbrc;
        ENCODE_CAPS m_caps;
/*
 * Current RollingIntraRefresh state, as it came throught the task state and passing to DDI in PPS
 * for Windows we keep it here to send update by VAMapBuffer as happend.
 */
        IntraRefreshState m_RIRState;

        mfxU32            m_curTrellisQuantization;   // mapping in accordance with libva
        mfxU32            m_newTrellisQuantization;   // will be sent through config.

        std::vector<VAEncROI> m_arrayVAEncROI;

        static const mfxU32 MAX_CONFIG_BUFFERS_COUNT = 26 + 5; //added FEI buffers

        UMC::Mutex m_guard;
        HeaderPacker m_headerPacker;

        // SkipFlag
        enum { NO_SKIP, NORMAL_MODE, PAVP_MODE};

        mfxU8  m_numSkipFrames;
        mfxU32 m_sizeSkipFrames;
        mfxU32 m_skipMode;
        bool m_isENCPAK;

        VAEncMiscParameterRateControl  m_vaBrcPar;
        VAEncMiscParameterFrameRate    m_vaFrameRate;
        std::vector<VAEncQpBufferH264> m_mbqp_buffer;
        std::vector<mfxU8>             m_mb_noskip_buffer;
    };

    //extend encoder to FEI interface
#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
    class VAAPIFEIPREENCEncoder : public VAAPIEncoder
    {
    public:
        VAAPIFEIPREENCEncoder();

        virtual
        ~VAAPIFEIPREENCEncoder();

        virtual mfxStatus CreateAccelerationService(MfxVideoParam const & par);
        virtual mfxStatus Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type);
        virtual mfxStatus Execute(mfxHDL surface, DdiTask const & task,
                mfxU32 fieldId, PreAllocatedVector const & sei);
        virtual mfxStatus QueryStatus(DdiTask & task, mfxU32 fieldId);
        virtual mfxStatus Destroy();

    protected:
        //helper functions
        mfxStatus CreatePREENCAccelerationService(MfxVideoParam const & par);

        mfxI32 m_codingFunction;

        VABufferID m_statParamsId;
        std::vector<VABufferID> m_statMVId;
        std::vector<VABufferID> m_statOutId;

        std::vector<ExtVASurface> m_statFeedbackCache;
        std::vector<ExtVASurface> m_inputQueue;
    };
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)    
    class VAAPIFEIENCEncoder : public VAAPIEncoder
    {
    public:
        VAAPIFEIENCEncoder();

        virtual
        ~VAAPIFEIENCEncoder();

        virtual mfxStatus CreateAccelerationService(MfxVideoParam const & par);
        virtual mfxStatus Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type);
        virtual mfxStatus Execute(mfxHDL surface, DdiTask const & task,
                mfxU32 fieldId, PreAllocatedVector const & sei);
        virtual mfxStatus QueryStatus(DdiTask & task, mfxU32 fieldId);
        virtual mfxStatus Destroy();

    protected:
        //helper functions
        mfxStatus CreateENCAccelerationService(MfxVideoParam const & par);

        mfxI32 m_codingFunction;

        VABufferID m_statParamsId;
        VABufferID m_statMVId;
        VABufferID m_statOutId;

        std::vector<ExtVASurface> m_statFeedbackCache;
        std::vector<ExtVASurface> m_inputQueue;

        VABufferID m_codedBufferId;
    };
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PAK) && defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)
    class VAAPIFEIPAKEncoder : public VAAPIEncoder
    {
    public:
        VAAPIFEIPAKEncoder();

        virtual
        ~VAAPIFEIPAKEncoder();

        virtual mfxStatus CreateAccelerationService(MfxVideoParam const & par);
        virtual mfxStatus Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type);
        virtual mfxStatus Execute(mfxHDL surface, DdiTask const & task,
                mfxU32 fieldId, PreAllocatedVector const & sei);
        virtual mfxStatus QueryStatus(DdiTask & task, mfxU32 fieldId);
        virtual mfxStatus Destroy();

    protected:
        //helper functions
        mfxStatus CreatePAKAccelerationService(MfxVideoParam const & par);

        mfxI32 m_codingFunction;

        VABufferID m_statParamsId;
        VABufferID m_statMVId;
        VABufferID m_statOutId;

        std::vector<ExtVASurface> m_statFeedbackCache;
        std::vector<ExtVASurface> m_inputQueue;

        VABufferID m_codedBufferId[2];
    };
#endif


}; // namespace

#endif // MFX_ENABLE_H264_VIDEO_ENCODE && (MFX_VA_LINUX)
#endif // __MFX_H264_ENCODE_VAAPI__H
/* EOF */
