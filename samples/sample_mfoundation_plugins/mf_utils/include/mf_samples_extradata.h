/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013 Intel Corporation. All Rights Reserved.

*********************************************************************************/
#pragma once

#include "mf_guids.h"
#include "mf_utils.h"
#include "sample_utils.h"
#include <map>

#define MF_TL_PRINT_ATTRIBUTES MF_TL_GENERAL
inline void PrintAttributes(IMFAttributes *p)
{
#ifndef MFX_TRACE_DISABLE
    MFX_LTRACE_P(MF_TL_PRINT_ATTRIBUTES, p);
    if (NULL == p)
    {
        return;
    }
    UINT32 nCount = 0;
    if (FAILED(p->GetCount(&nCount)))
    {
        nCount = 0;
    }
    MFX_LTRACE_I(MF_TL_PRINT_ATTRIBUTES, nCount);
    for (UINT32 i = 0; i < nCount; i++)
    {
        //MFX_LTRACE_I(MF_TL_PRINT_ATTRIBUTES, i);
        GUID guidKey = {0};
        HRESULT hr = p->GetItemByIndex(i, &guidKey, NULL);
        if (SUCCEEDED(hr))
        {
            if (MFSampleExtension_Token == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_Token")
            else if (MFSampleExtension_CleanPoint == guidKey)
            {
                UINT32 nValue = 0;
                p->GetUINT32(MFSampleExtension_CleanPoint, &nValue);
                MFX_LTRACE_1(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_CleanPoint=", MFX_TRACE_FORMAT_I, nValue);
            }
            else if (MFSampleExtension_LongTermReferenceFrameInfo == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_LongTermReferenceFrameInfo")
            else if (MFSampleExtension_BottomFieldFirst == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_BottomFieldFirst")
            else if (MFSampleExtension_Discontinuity == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_Discontinuity")
            else if (MFSampleExtension_Interlaced == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_Interlaced")
            else if (MFSampleExtension_DecodeTimestamp == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_DecodeTimestamp")
            else if (MFSampleExtension_FrameCorruption == guidKey)
                MFX_LTRACE_S(MF_TL_PRINT_ATTRIBUTES, "MFSampleExtension_FrameCorruption")
            else
                MFX_LTRACE_GUID(MF_TL_PRINT_ATTRIBUTES, guidKey);
        }
        else
        {
            MFX_LTRACE_D(MF_TL_PRINT_ATTRIBUTES, hr);
        }
    }
#else
    UNREFERENCED_PARAMETER(p);
#endif
}

class IMFSampleExtradata : public IUnknown
{
public:
    IMFSampleExtradata() : m_nRefCount(0), m_nSampleDuration(0)
    {
    }

    virtual ~IMFSampleExtradata() {}

    HRESULT SetAttributes(IMFAttributes *pSource)
    {
        HRESULT hr = S_OK;
        if (NULL == pSource)
        {
            m_pAttributes.Release();
        }
        else if (pSource != m_pAttributes)
        {
            UINT32 nCount = 0;
            if (FAILED(pSource->GetCount(&nCount)))
            {
                nCount = 0;
            }
            if (nCount > 0)
            {
                CComPtr<IMFAttributes> pCopy;
                hr = MFCreateAttributes(&pCopy, nCount);
                if (SUCCEEDED(hr))
                {
                    hr = pSource->CopyAllItems(pCopy);
                }
                if (SUCCEEDED(hr))
                {
                    pCopy->DeleteItem(MFSampleExtension_DecodeTimestamp);
                    m_pAttributes = pCopy;
                }
                ATLASSERT(SUCCEEDED(hr));
            }
        }
        return hr;
    }

    HRESULT SetSampleDuration(IMFSample *pSample)
    {
        HRESULT hr = S_OK;

        if (NULL == pSample)
        {
            m_nSampleDuration = 0;
        }
        else
        {
            pSample->GetSampleDuration(&m_nSampleDuration);
        }

        return hr;
    }

    REFERENCE_TIME GetSampleDuration() { return m_nSampleDuration; }

    CComPtr<IMFAttributes> GetAttributes() { return m_pAttributes; }

    /*------------------------------------------------------------------------------*/
    // IUnknown methods

    STDMETHODIMP_(ULONG) AddRef(void)
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
        return InterlockedIncrement(&m_nRefCount);
    }

    STDMETHODIMP_(ULONG) Release(void)
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
        ULONG uCount = InterlockedDecrement(&m_nRefCount);
        MFX_LTRACE_I(MF_TL_GENERAL, uCount);
        if (uCount == 0) delete this;
        // For thread safety, return a temporary variable.
        return uCount;
    }

    STDMETHODIMP QueryInterface(REFIID iid, void** ppv)
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_GENERAL);
        if (!ppv) return E_POINTER;
        if ((iid == IID_IUnknown))
        {
            *ppv = this;
        }
        return S_OK;
    }

protected:
    // Reference count, critical section
    long   m_nRefCount;

    CComPtr<IMFAttributes> m_pAttributes;
    REFERENCE_TIME         m_nSampleDuration;
private:
    DISALLOW_COPY_AND_ASSIGN(IMFSampleExtradata);
};

//mfxFrameData, mfxEncodeCtrl, mfxBitstream
class MFSampleExtradataTransport
{
public:
    HRESULT Send(mfxU32 nFrameOrder, IMFSampleExtradata *pData)
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
        MFX_LTRACE_I(MF_TL_GENERAL, nFrameOrder);
        HRESULT hr = S_OK;
        //if (m_arrData.find(nFrameOrder) )
        ATLASSERT(m_arrData.end() == m_arrData.find(nFrameOrder));
        try
        {
            m_arrData[nFrameOrder] = pData;
        }
        catch (...)
        {
            ATLASSERT(!"m_arrData[nFrameOrder] = pData;");
            hr = E_OUTOFMEMORY;
        }
        return hr;
    }
    
    CComPtr<IMFSampleExtradata> Receive(mfxU32 nFrameOrder)
    {
        MFX_AUTO_LTRACE_FUNC(MF_TL_NOTES);
        MFX_LTRACE_I(MF_TL_NOTES, nFrameOrder);
        CComPtr<IMFSampleExtradata> pResult = NULL;
        ExtradataMap::iterator it = m_arrData.find(nFrameOrder);
        if (m_arrData.end() == it)
        {
            //workaround for case when MSDK doesn't provide FrameOrder with output bitstream
            //if (_UI32_MAX == nFrameOrder) 
            //    it = m_arrData.begin();
            //else
                ATLASSERT(m_arrData.end() != it);
        }
        if (m_arrData.end() != it)
        {
            pResult = it->second;
            m_arrData.erase(it);
        }
        return pResult;
    }

    void Clear()
    {
        m_arrData.clear();
    };
protected:
    //typedef std::map<mfxU32, CAdapt<CComPtr<IMFSampleExtradata>>> ExtradataMap;
    typedef std::map<mfxU32, CComPtr<IMFSampleExtradata>> ExtradataMap;
    ExtradataMap m_arrData; //FrameOrder -> Extradata
};

/*--------------------------------------------------------------------*/
