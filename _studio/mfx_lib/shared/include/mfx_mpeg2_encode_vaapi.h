/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//       Copyright(c) 2013-2015 Intel Corporation. All Rights Reserved.
//
*/

#ifndef __MFX_MPEG2_ENCODE_VAAPI__H
#define __MFX_MPEG2_ENCODE_VAAPI__H

#include "mfx_common.h"

#if defined (MFX_VA_LINUX)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#include <vector>
#include <assert.h>

#include "mfx_h264_encode_struct_vaapi.h"
#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_mpeg2.h>
#include "vaapi_ext_interface.h"

#include "mfx_ext_buffers.h"
#include "mfxpcp.h"

#include "mfx_mpeg2_enc_common_hw.h"
#include "mfx_mpeg2_encode_interface.h"
#include "umc_muxer.h"


namespace MfxHwMpeg2Encode
{

    class VAAPIEncoder : public DriverEncoder
    {
    public:
        enum { MAX_SLICES = 128 };

        explicit VAAPIEncoder(VideoCORE* core);

        virtual
        ~VAAPIEncoder();

        virtual
        mfxStatus QueryEncodeCaps(ENCODE_CAPS & caps);

        virtual
        mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId);

        virtual
        mfxStatus CreateContext(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId);

        virtual
        mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0);                

        virtual
        mfxStatus Close();

        virtual
        bool      IsFullEncode() const { return true; }

        virtual
        mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse);

        virtual
        mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers);

        virtual
        mfxStatus FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt);

        virtual
        mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers);

    private:
        VAAPIEncoder(const VAAPIEncoder&); // no implementation
        VAAPIEncoder& operator=(const VAAPIEncoder&); // no implementation

        mfxStatus QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest, ExecuteBuffers* pExecuteBuffers);
        mfxStatus CreateCompBuffers  (ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames);
        mfxStatus CreateMBDataBuffer  (mfxU32 numRefFrames);
        mfxStatus CreateBSBuffer      (mfxU32 numRefFrames, ExecuteBuffers* pExecuteBuffers);

        mfxStatus GetBuffersInfo();
        mfxStatus QueryMbDataLayout();
        mfxStatus Init(ENCODE_FUNC func, ExecuteBuffers* pExecuteBuffers);
        mfxStatus FillSlices(ExecuteBuffers* pExecuteBuffers);
        mfxStatus FillMiscParameterBuffer(ExecuteBuffers* pExecuteBuffers);
        mfxStatus FillUserDataBuffer(mfxU8 *pUserData, mfxU32 userDataLen);
        mfxStatus FillVideoSignalInfoBuffer(ExecuteBuffers* pExecuteBuffers);
        mfxStatus FillMBQPBuffer(ExecuteBuffers* pExecuteBuffers, mfxU8* mbqp, mfxU32 numMB);
        mfxStatus FillSkipFrameBuffer(mfxU8 skipFlag);

        mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 func, mfxU8* pUserData, mfxU32 userDataLen);        
        mfxStatus Register (const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type);
        mfxI32    GetRecFrameIndex (mfxMemId memID);
        mfxI32    GetRawFrameIndex (mfxMemId memIDe, bool bAddFrames);    


        VideoCORE*                          m_core;

        // encoder specific. can be encapsulated by auxDevice class
        VADisplay                           m_vaDisplay;
        VAContextID                         m_vaContextEncode;
        VAConfigID                          m_vaConfig;
        VAEncSequenceParameterBufferMPEG2   m_vaSpsBuf;
        VABufferID                          m_spsBufferId;
        VAEncPictureParameterBufferMPEG2    m_vaPpsBuf;
        VABufferID                          m_ppsBufferId;
        VABufferID                          m_qmBufferId;
        VAEncSliceParameterBufferMPEG2      m_sliceParam[MAX_SLICES];
        VABufferID                          m_sliceParamBufferId[MAX_SLICES];  /* Slice level parameter, multil slices */
        int                                 m_numSliceGroups;
        int                                 m_codedbufISize;
        int                                 m_codedbufPBSize;

        VAEncMiscParameterBuffer           *m_pMiscParamsFps;
        VAEncMiscParameterBuffer           *m_pMiscParamsPrivate;
        VAEncMiscParameterBuffer           *m_pMiscParamsSeqInfo;
        VAEncMiscParameterBuffer           *m_pMiscParamsSkipFrame;
        

        VABufferID                          m_miscParamFpsId;
        VABufferID                          m_miscParamPrivateId;
        VABufferID                          m_miscParamSeqInfoId;
        VABufferID                          m_miscParamSkipFrameId;
        VABufferID                          m_packedUserDataParamsId;
        VABufferID                          m_packedUserDataId;
        VABufferID                          m_mbqpBufferId;
        std::vector<VAEncQpBufferH264>      m_mbqpDataBuffer;


        int                                 m_vbvBufSize;
        mfxU16                              m_initFrameWidth;
        mfxU16                              m_initFrameHeight;

        std::vector<ENCODE_COMP_BUFFER_INFO> m_compBufInfo;
        std::vector<D3DDDIFORMAT> m_uncompBufInfo;
        ENCODE_MBDATA_LAYOUT m_layout;
        
        mfxFeedback                         m_feedback;
        std::vector<ExtVASurface>           m_bsQueue;
        std::vector<ExtVASurface>           m_reconQueue;


        mfxFrameAllocResponse               m_allocResponseMB;
        mfxFrameAllocResponse               m_allocResponseBS;
        mfxRecFrames                        m_recFrames;
        mfxRawFrames                        m_rawFrames;

        UMC::Mutex                          m_guard;

#ifdef MPEG2_ENC_HW_PERF
        vm_time lock_MB_data_time[3];
        vm_time copy_MB_data_time[3];
#endif 
    }; // class VAAPIEncoder


}; // namespace

#endif // defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)
#endif // defined (MFX_VA_LINUX)
#endif // __MFX_MPEG2_ENCODE_VAAPI__H
/* EOF */
