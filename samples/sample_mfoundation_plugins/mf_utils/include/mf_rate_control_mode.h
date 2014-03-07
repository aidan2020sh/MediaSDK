/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once
#include "mf_param_brc_multiplier.h"

class MFRateControlMode
{
public:

    MFRateControlMode(mfxVideoParam *p_MfxVideoParam = NULL):
        m_eMode(eAVEncCommonRateControlMode_CBR),
        m_nQualityPercent(0),
        m_nQualityVsSpeed(0),
        m_pMfxVideoParam(p_MfxVideoParam)
    {
    }

    virtual ~MFRateControlMode(void)
    {

    }

    eAVEncCommonRateControlMode GetMode() const
    {
        return m_eMode;
    }

    HRESULT GetMode(ULONG &nMode) const
    {
        HRESULT hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
        mfxU16 nMfxRateControlMethodTest = mf_ms2mfx_rate_control(m_eMode);
        ATLASSERT(MFX_RATECONTROL_UNKNOWN != nMfxRateControlMethodTest); // should not happen
        if (MFX_RATECONTROL_UNKNOWN != nMfxRateControlMethodTest)
        {
            nMode = m_eMode;
            hr = S_OK;
        }
        return hr;
    }

    HRESULT SetMode(ULONG nMode)
    {
        HRESULT hr = S_OK;
        hr = E_INVALIDARG;
        mfxU16 nMfxRateControlMethod = mf_ms2mfx_rate_control(nMode);
        if (MFX_RATECONTROL_UNKNOWN != nMfxRateControlMethod)
        {
            m_eMode = (eAVEncCommonRateControlMode)nMode;
            if (NULL != m_pMfxVideoParam)
            {
                m_pMfxVideoParam->mfx.RateControlMethod = nMfxRateControlMethod;
            }
            hr = S_OK;
        }
        return hr;
    }

    ULONG GetQualityPercent() const
    {
        return m_nQualityPercent;
    }

    HRESULT GetQualityPercent(ULONG &nQualityPercent) const
    {
        HRESULT hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
        if (IsApplicable(&CODECAPI_AVEncCommonQuality))
        {
            mfxU16 nMfxQpTest = mf_ms_quality_percent2mfx_qp(m_nQualityPercent);
            if (nMfxQpTest > 0)
            {
                nQualityPercent = m_nQualityPercent;
                hr = S_OK;
            }
        }
        return hr;
    }

    HRESULT SetQualityPercent(ULONG nQualityPercent)
    {
        HRESULT hr = E_INVALIDARG;
        if (IsApplicable(&CODECAPI_AVEncCommonQuality))// Also checked in IsModifiable/SetValue
        {
            mfxU16 nMfxQp = mf_ms_quality_percent2mfx_qp(nQualityPercent);
            if (nMfxQp > 0)
            {
                m_nQualityPercent = nQualityPercent;
                if (NULL != m_pMfxVideoParam)
                {
                    m_pMfxVideoParam->mfx.QPI = nMfxQp;
                    m_pMfxVideoParam->mfx.QPP = nMfxQp;
                    m_pMfxVideoParam->mfx.QPB = nMfxQp;
                }
                hr = S_OK;
            }
        }
        return hr;
    }

    ULONG GetMeanBitRate() const
    {
        return m_nMeanBitRate;
    }

    void SetMeanBitRate(ULONG nMeanBitRate)
    {
        m_nMeanBitRate = nMeanBitRate;
        if (NULL != m_pMfxVideoParam)
        {
            MfxSetTargetKbps(m_pMfxVideoParam->mfx, m_nMeanBitRate/1000); //bps = 1/1000 Kbps
        }
    }

    ULONG GetMaxBitRate() const
    {
        return m_nMaxBitRate;
    }

    void SetMaxBitRate(ULONG nMaxBitRate)
    {
        m_nMaxBitRate = nMaxBitRate;
        if (NULL != m_pMfxVideoParam)
        {
            MfxSetMaxKbps(m_pMfxVideoParam->mfx, m_nMaxBitRate/1000); //bps = 1/1000 Kbps
        }
    }

    ULONG GetBufferSize() const
    {
        return m_nBufferSize;
    }

    void SetBufferSize(ULONG nBufferSize)
    {
        m_nBufferSize = nBufferSize;
        if (NULL != m_pMfxVideoParam)
        {
            MfxSetBufferSizeInKB(m_pMfxVideoParam->mfx, m_nBufferSize/8000); //bit = 1/8000 KB
        }
    }

    ULONG GetQualityVsSpeed() const
    {
        return m_nQualityVsSpeed;
    }

    HRESULT GetQualityVsSpeed(ULONG &nQualityVsSpeed) const
    {
        HRESULT hr = VFW_E_CODECAPI_NO_CURRENT_VALUE;
        mfxU16 nMfxTargetUsageTest = mf_ms_quality_vs_speed2mfx_target_usage(m_nQualityVsSpeed);
        ATLASSERT(MFX_TARGETUSAGE_UNKNOWN != nMfxTargetUsageTest); // should not happen
        if (MFX_TARGETUSAGE_UNKNOWN != nMfxTargetUsageTest)
        {
            nQualityVsSpeed = m_nQualityVsSpeed;
            hr = S_OK;
        }
        return hr;
    }

    HRESULT SetQualityVsSpeed(ULONG nQualityVsSpeed)
    {
        HRESULT hr = E_INVALIDARG;
        mfxU16 nMfxTargetUsage = mf_ms_quality_vs_speed2mfx_target_usage(nQualityVsSpeed);
        ATLASSERT(MFX_TARGETUSAGE_UNKNOWN != nMfxTargetUsage);
        if(MFX_TARGETUSAGE_UNKNOWN != nMfxTargetUsage)
        {
            m_nQualityVsSpeed = nQualityVsSpeed;
            if (NULL != m_pMfxVideoParam)
            {
                m_pMfxVideoParam->mfx.TargetUsage = nMfxTargetUsage;
                //TODO: move code here:
                //HandleNewMessage(mtRequireUpdateMaxMBperSec, this); // ignore return value
            }
            hr = S_OK;
        }
        return hr;
    }

    bool IsApplicable(const GUID* Api) const
    {
        bool bResult = false;
        if (NULL != Api)
        {
            if    ( (CODECAPI_AVEncVideoEncodeQP == *Api) ||
                    (CODECAPI_AVEncCommonQuality == *Api) )
            {
                //Allowed for Quality RateControlMode only
                bResult = (eAVEncCommonRateControlMode_Quality == m_eMode);
            }
            else if (CODECAPI_AVEncCommonMeanBitRate == *Api)
            {
                //Allowed for PeakConstrainedVBR, UnconstrainedVBR and CBR RateControlModes only
                bResult = ( (eAVEncCommonRateControlMode_PeakConstrainedVBR == m_eMode) ||
                            (eAVEncCommonRateControlMode_UnconstrainedVBR == m_eMode) ||
                            (eAVEncCommonRateControlMode_CBR == m_eMode) );
            }
            else if (CODECAPI_AVEncCommonMaxBitRate == *Api)
            {
                //Allowed for PeakConstrainedVBR RateControlMode only
                bResult = (eAVEncCommonRateControlMode_PeakConstrainedVBR == m_eMode);
            }
            else if (CODECAPI_AVEncCommonBufferSize == *Api)
            {
                //Allowed for PeakConstrainedVBR and CBR RateControlModes only
                bResult = ( (eAVEncCommonRateControlMode_PeakConstrainedVBR == m_eMode) ||
                            (eAVEncCommonRateControlMode_CBR == m_eMode) );
            }
        }
        return bResult;
    }

protected:
    eAVEncCommonRateControlMode m_eMode;
    ULONG                       m_nQualityPercent;
    ULONG                       m_nQualityVsSpeed;
    ULONG                       m_nMeanBitRate;
    ULONG                       m_nMaxBitRate;
    ULONG                       m_nBufferSize;
    mfxVideoParam               *m_pMfxVideoParam;
};

