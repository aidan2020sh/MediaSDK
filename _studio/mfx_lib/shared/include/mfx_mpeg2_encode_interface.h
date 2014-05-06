    /*///// /////////////////////////////////////////////////////////////////////////
    //
    //                  INTEL CORPORATION PROPRIETARY INFORMATION
    //     This software is supplied under the terms of a license agreement or
    //     nondisclosure agreement with Intel Corporation and may not be copied
    //     or disclosed except in accordance with the terms of that agreement.
    //          Copyright(c) 2011 - 2013 Intel Corporation. All Rights Reserved.
    //
    */

#include "mfx_common.h"

#if defined(MFX_VA)
#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)

#ifndef __MFX_MPEG2_ENCODE_INTERFACE__H
#define __MFX_MPEG2_ENCODE_INTERFACE__H

#include <vector>
#include <assert.h>
#include "mfxdefs.h"
#include "mfxvideo++int.h"

#include "mfx_mpeg2_enc_common_hw.h"

#if   defined(MFX_VA_WIN)
    #include "encoding_ddi.h"
    #include "encoder_ddi.hpp"
#elif defined(MFX_VA_LINUX)
    #include "mfx_h264_encode_struct_vaapi.h"
    #include <va/va_enc_mpeg2.h>
#endif

namespace MfxHwMpeg2Encode
{
#ifdef PAVP_SUPPORT
    class AesCounter : public mfxAES128CipherCounter
    {
    public:
        AesCounter();

        void Init(mfxExtPAVPOption const & opt);

        bool Increment();

        bool IsWrapped() const { return m_wrapped; }

        void ResetWrappedFlag() { m_wrapped = false; }

    private:
        mfxExtPAVPOption m_opt;
        bool m_wrapped;
    };
#endif
    class Encryption
    {
    public:
        bool                   m_bEncryptionMode;
#ifdef PAVP_SUPPORT
        UINT                    m_CounterAutoIncrement;
        D3DAES_CTR_IV           m_InitialCounter;
        PAVP_ENCRYPTION_MODE    m_PavpEncryptionMode;
        AesCounter              m_aesCounter;
#endif 
        Encryption()
            : m_bEncryptionMode (false)
#ifdef PAVP_SUPPORT
            , m_aesCounter()
#endif
        {
#ifdef PAVP_SUPPORT
            memset (&m_InitialCounter,0,sizeof(m_InitialCounter)) ;
            memset (&m_PavpEncryptionMode,0,sizeof(m_PavpEncryptionMode));
            m_CounterAutoIncrement = 0;
#endif
        }
        void Init(const mfxVideoParamEx_MPEG2* par, mfxU32 funcId)
        {
            m_bEncryptionMode = false;
            par;funcId;

 #ifdef PAVP_SUPPORT
            if (par->mfxVideoParams.Protected && (funcId == ENCODE_ENC_PAK_ID))
            {
                m_bEncryptionMode = true;
                m_CounterAutoIncrement  = par->sExtPAVPOption.CounterIncrement;
                m_InitialCounter.IV     = par->sExtPAVPOption.CipherCounter.IV;
                m_InitialCounter.Count  = par->sExtPAVPOption.CipherCounter.Count;
                m_PavpEncryptionMode.eEncryptionType    = par->sExtPAVPOption.EncryptionType;
                m_PavpEncryptionMode.eCounterMode    = par->sExtPAVPOption.CounterType;
                m_aesCounter.Init(par->sExtPAVPOption);
            }
            else
            {               
                m_CounterAutoIncrement  = 0;
                m_InitialCounter.IV     = 0;
                m_InitialCounter.Count  = 0;
                m_PavpEncryptionMode.eEncryptionType = PAVP_ENCRYPTION_NONE;
                m_PavpEncryptionMode.eCounterMode    = PAVP_COUNTER_TYPE_A;    
            }
#endif         
        }
    };
    struct ExecuteBuffers
    {
        ExecuteBuffers()
            : m_pSlice(0)
            , m_pMBs (0)
            , m_pSurface(0)
            // , m_pSurfacePair(0)
            , m_idxMb(DWORD(-1))
            , m_nSlices(0)
            , m_nMBs(0)
            , m_bAddSPS (0)
            , m_bExternalCurrFrame(0)
            , m_GOPPictureSize(0)
            , m_GOPRefDist(0)
            , m_GOPOptFlag(0)
            , m_encrypt()
            , m_fFrameRate(0)           
        {
            memset(&m_caps, 0, sizeof(m_caps));
            memset(&m_sps,  0, sizeof(m_sps));
            memset(&m_pps,  0, sizeof(m_pps));
            memset(&m_quantMatrix, 0, sizeof(m_quantMatrix));
        }

        mfxStatus Init (const mfxVideoParamEx_MPEG2* par, mfxU32 funcId, bool bAllowBRC = false);
        mfxStatus Close();
        
        mfxStatus InitPictureParameters(mfxFrameParamMPEG2* pParams, mfxU32 frameNum);
        mfxStatus InitPictureParameters(mfxFrameCUC* pCUC);
        void      InitFramesSet(mfxMemId curr, bool bExternal, mfxMemId rec, mfxMemId ref_0,mfxMemId ref_1);
        mfxStatus InitSliceParameters(mfxFrameCUC* pCUC);
        mfxStatus InitSliceParameters(mfxU8 qp);
        mfxStatus GetMBParameters(mfxFrameCUC* pCUC);
        mfxStatus SetMBParameters(mfxFrameCUC* pCUC);
 
        ENCODE_ENC_CTRL_CAPS                    m_caps;  

        ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2    m_sps;
        ENCODE_SET_PICTURE_PARAMETERS_MPEG2     m_pps;
        ENCODE_SET_SLICE_HEADER_MPEG2*          m_pSlice;
        ENCODE_ENC_MB_DATA_MPEG2*               m_pMBs;
#if defined (MFX_VA_WIN)
        ENCODE_SET_PICTURE_QUANT                m_quantMatrix;
#else
        VAIQMatrixBufferMPEG2                   m_quantMatrix;
#endif

        mfxHDL                                    m_pSurface;
        mfxHDLPair                                m_pSurfacePair;

        DWORD                                   m_idxMb;
        DWORD                                   m_idxBs;
        DWORD                                   m_nSlices;
        DWORD                                   m_nMBs;
        DWORD                                   m_bAddSPS;

        mfxMemId                                m_RecFrameMemID;
        mfxMemId                                m_RefFrameMemID[2];
        mfxMemId                                m_CurrFrameMemID;
        bool                                    m_bExternalCurrFrame;
        bool                                    m_bOutOfRangeMV;
        bool                                    m_bErrMBType;
        bool                                    m_bUseRawFrames;
        USHORT                                  m_GOPPictureSize;
        UCHAR                                   m_GOPRefDist;
        UCHAR                                   m_GOPOptFlag;

        Encryption                              m_encrypt;
        Ipp64f                                  m_fFrameRate;

    };


    class DriverEncoder
    {
    public:
        //explicit D3D9Encoder(VideoCORE* core);
        virtual ~DriverEncoder(){}

        virtual mfxStatus QueryEncodeCaps(ENCODE_CAPS & caps) = 0;
        virtual mfxStatus Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId) = 0;        
        virtual mfxStatus CreateContext(ExecuteBuffers* /*pExecuteBuffers*/, mfxU32 /*numRefFrames*/, mfxU32 /*funcId*/) { return MFX_ERR_NONE; }
        
        virtual mfxStatus Execute(ExecuteBuffers* pExecuteBuffers, mfxU8* pUserData = 0, mfxU32 userDataLen = 0) = 0;                

        virtual bool      IsFullEncode() const = 0;

        virtual mfxStatus Close() = 0;

        virtual mfxStatus RegisterRefFrames(const mfxFrameAllocResponse* pResponse) = 0;

        virtual mfxStatus FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers) = 0;

        virtual mfxStatus FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt) = 0;

        virtual mfxStatus SetFrames (ExecuteBuffers* pExecuteBuffers) = 0;
    };

    DriverEncoder* CreatePlatformMpeg2Encoder( VideoCORE* core );    
    
}; // namespace

#endif //#ifndef __MFX_MPEG2_ENCODE_INTERFACE__H
#endif //(MFX_ENABLE_MPEG2_VIDEO_ENCODE) || defined(MFX_ENABLE_MPEG2_VIDEO_ENC)
#endif // MFX_VA
/* EOF */
