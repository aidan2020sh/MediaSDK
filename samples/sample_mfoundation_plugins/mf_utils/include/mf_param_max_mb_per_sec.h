/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mf_mfx_params_interface.h"

class MFParamMaxMBperSec: public MFParamsWorker
{
public:
    MFParamMaxMBperSec(MFParamsManager *pManager)
        : MFParamsWorker(pManager),
          m_Value(0)
    {
        memset(&m_TempVideoParamIn, 0, sizeof(m_TempVideoParamIn));
        m_TempVideoParamIn.mfx.CodecId = MFX_CODEC_AVC;

        //dafault values:
        m_TempVideoParamIn.mfx.CodecLevel = MFX_LEVEL_AVC_4;
        m_TempVideoParamIn.mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
        m_TempVideoParamIn.mfx.GopRefDist = 1; //no B frames

        m_TempVideoParamIn.NumExtParam = 0;
        m_TempVideoParamIn.ExtParam = NULL;
        m_arrTempEncExtBufIn.UpdateExtParam(m_TempVideoParamIn);

        mfxExtEncoderCapability *pExtEncoderCapability = m_arrTempEncExtBufIn.Find<mfxExtEncoderCapability>();
        if (NULL == pExtEncoderCapability)
        {
            //hr = PrepareExtBuf(m_TempVideoParamIn, m_arrTempEncExtBufIn, pExtEncoderCapability);
        }
    }

    virtual ~MFParamMaxMBperSec()
    {
    };

    //MFParamsWorker::

    virtual bool IsEnabled() const { return true; }

    virtual HRESULT UpdateMaxMBperSec(MFXVideoENCODE* pmfxENC, mfxU16 CodecLevel, mfxU16 TargetUsage, mfxU16 GopRefDist, UINT32 &nValue)
    {
        HRESULT hr = S_OK;

        SetAffectingProperties(CodecLevel, TargetUsage, GopRefDist);

        if (0 == m_Value)
        {
            hr = QueryMaxMBperSec(pmfxENC);
            ATLASSERT(SUCCEEDED(hr));
        }
        if (SUCCEEDED(hr))
            hr = GetMaxMBperSec(nValue);

        return hr;
    }

    HRESULT GetMaxMBperSec(UINT32 &nValue) const
    {
        HRESULT hr = S_OK;
        if (0 == m_Value || m_Value > 0x0FFFFFFF) //  MFT should set upper 4 bits to 0)
        {
            ATLASSERT(m_Value > 0);
            ATLASSERT(m_Value <= 0x0FFFFFFF);
            hr = E_FAIL;
        }
        else
            nValue = m_Value;
        return hr;
    }

//TODO: remove this by implementing local syncronized copies of mfxVideoParam in MFParamsWorker
    void SetAffectingProperties(mfxU16 CodecLevel, mfxU16 TargetUsage, mfxU16 GopRefDist)
    {
        if (0 != CodecLevel && m_TempVideoParamIn.mfx.CodecLevel != CodecLevel)
        {
            m_TempVideoParamIn.mfx.CodecLevel = CodecLevel;
            m_Value = 0;
        }

        if (0 != TargetUsage && m_TempVideoParamIn.mfx.TargetUsage != TargetUsage)
        {
            m_TempVideoParamIn.mfx.TargetUsage = TargetUsage;
            m_Value = 0;
        }
        if (0 != GopRefDist && m_TempVideoParamIn.mfx.GopRefDist != GopRefDist)
        {
            m_TempVideoParamIn.mfx.GopRefDist = GopRefDist;
            m_Value = 0;
        }
    }

    STDMETHOD(QueryMaxMBperSec)(MFXVideoENCODE* pmfxENC)
    {
        CHECK_POINTER(pmfxENC, E_POINTER);
        HRESULT hr = S_OK;

        if (NULL == m_arrTempEncExtBufIn.FindOrCreate<mfxExtEncoderCapability>())
        {
            hr = E_OUTOFMEMORY;
        }
        m_arrTempEncExtBufIn.UpdateExtParam(m_TempVideoParamIn);

        //prepare out param for Query
        mfxVideoParam tempVideoParamOut = {0};
        MFExtBufCollection arrTempEncExtBufOut;
        mfxExtEncoderCapability *pExtEncoderCapabilityOut = NULL;
        if (SUCCEEDED(hr))
        {
            memset(&tempVideoParamOut, 0, sizeof(tempVideoParamOut));
            tempVideoParamOut.mfx.CodecId = m_TempVideoParamIn.mfx.CodecId;
            pExtEncoderCapabilityOut = arrTempEncExtBufOut.Create<mfxExtEncoderCapability>();
            if (NULL == pExtEncoderCapabilityOut)
            {
                hr = E_OUTOFMEMORY;
            }
            arrTempEncExtBufOut.UpdateExtParam(tempVideoParamOut);
        }

        if (SUCCEEDED(hr))
        {

            mfxStatus sts = pmfxENC->Query(&m_TempVideoParamIn, &tempVideoParamOut);
            if (MFX_ERR_NONE != sts)
            {
                MFX_LTRACE_I(MF_TL_DEBUG, sts);
                hr = E_FAIL;
            }
        }

        if (SUCCEEDED(hr))
        {
            MFX_LTRACE_I(MF_TL_DEBUG, pExtEncoderCapabilityOut->MBPerSec);
            m_Value = pExtEncoderCapabilityOut->MBPerSec;
        }
        return hr;
    }

protected:
    UINT32 m_Value;

    mfxVideoParam m_TempVideoParamIn;
    MFExtBufCollection m_arrTempEncExtBufIn;
};