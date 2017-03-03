/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "atlbase.h"

#include "mfx_filter_guid.h"
#include "mfx_filter_externals.h"

#include "mfx_video_dec_proppage.h"

#include "mfxvideo++.h"

#include "base_decoder.h"

#include "frame_constructors.h"

#include "memory_allocator.h"
#include "sysmem_allocator.h"

#define FILTER_PROPERTIES2 { L"Video Decoder Properties", &CLSID_VideoPropertyPage, CVideoDecPropPage::CreateInstance, NULL, NULL }
#define FILTER_PROPERTIES3 { L"About", &CLSID_AboutPropertyPage, CAboutPropPage::CreateInstance, NULL, NULL }

class CDecoderOutputPin;

class CDecVideoFilter : public CTransformFilter, public ISpecifyPropertyPages, public IAboutProperty, public IConfigureVideoDecoder,
                        public IVideoDecoderProperty
{
public:

    DECLARE_IUNKNOWN;
    // Default constructor
    CDecVideoFilter(TCHAR *tszName,LPUNKNOWN punk, const GUID VIDEOCodecGUID, HRESULT *phr,
        mfxU16 APIVerMinor = 1, mfxU16 APIVerMajor = 1);
    // Destructor
    ~CDecVideoFilter(void);

    // Overridden pure virtual functions from CTransformFilter base class
    HRESULT             GetMediaType                        (int iPosition, CMediaType *pMediaType);
    HRESULT             CheckInputType                      (const CMediaType *mtIn);
    HRESULT             DecideBufferSize                    (IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties);

    CBasePin*           GetPin                              (int n);

    HRESULT             StartStreaming                      (void);
    HRESULT             Receive                             (IMediaSample *pSample);
    HRESULT             EndOfStream                         (void);
    HRESULT             StopStreaming                       (void);

    HRESULT             BeginFlush                          (void);
    HRESULT             EndFlush                            (void);

    HRESULT             NewSegment                          (REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

    HRESULT             BreakConnect                        (PIN_DIRECTION dir);
    HRESULT             CheckTransform                      (const CMediaType* mtIn, const CMediaType* mtOut) { return S_OK; };

    STDMETHODIMP        NonDelegatingQueryInterface         (REFIID riid, void **ppv);
    STDMETHODIMP        GetPages                            (CAUUID *pPages);

    //IAboutProperty
    STDMETHODIMP        GetFilterName                       (BSTR* pName) { *pName = m_bstrFilterName; return S_OK; };

    //IConfigureVideoDecoder custom interface
    STDMETHODIMP        GetRunTimeStatistics                (Statistics *statistics);

    //Set stride if it not equal to value, obtained during filters connection
    HRESULT             SetNewStride                        (mfxU32 nNewStride) { m_nPitch = nNewStride; return S_OK; };

    HRESULT             CompleteConnect                     (PIN_DIRECTION direction, IPin *pReceivePin);

protected:

    // Deinitialize filter
    void                Deinitialize                        (void);
    // Deliver decoded surface to next filter
    HRESULT             DeliverSurface                      (mfxFrameSurface1* pSurface);

    HRESULT             ProcessNewResolution ();

    // Set cropping rectangle in sample's media type
    HRESULT             RenewSampleParams                   (mfxFrameSurface1* pSurface, IMediaSample* pSample);

    //Codec could add some custom params
    virtual HRESULT     AttachCustomCodecParams             (mfxVideoParam* pParams) { return S_OK; };

    virtual void        WriteMfxImplToRegistry();

protected:

    CCritSec            m_csLock;

    CComBSTR            m_bstrFilterName;      //filter name

    CBaseDecoder*       m_pDecoder;

    BOOL                m_bStop;               // (bool) graph is stopped
    BOOL                m_bFlushing;

    // minimum required API version
    mfxU16                        m_nAPIVerMinor;
    mfxU16                        m_nAPIVerMajor;

    mfxVideoParam       m_mfxParamsVideo;      // video params
    mfxU32              m_nPitch;

    BYTE                m_pTempBuffer[5];
    mfxU32              m_nTempBufferLen;

    CTimeManager*       m_pTimeManager;

    mfxU32              m_nRequiredFramesNum;  // number of frames required by decoder for normal data processing

    CFrameConstructor*  m_pFrameConstructor;

    MFXFrameAllocator*  m_pFrameAllocator; // frame allocator for base decoder
    BOOL                m_bNeedToDeleteAllocator; // true if allocator is not a COM object

    IDirect3DDeviceManager9* m_pManager;

    CDecoderOutputPin  *m_pDecoderOutput; // custom interface

    // pixel aspect ratio
    mfxU16              m_nPARW;
    mfxU16              m_nPARH;

    BOOL                m_bJoinSession; // try joining decoder's session with a session from downstream filter,
                                         // feature available in Media SDK API 1.1 and higher

    BOOL                m_bLowLatencyMode;

private:
    DISALLOW_COPY_AND_ASSIGN(CDecVideoFilter);
};

class CDecoderOutputPin : public CTransformOutputPin
{
public:
    friend CDecVideoFilter;

    CDecoderOutputPin(CTransformFilter* pTransformFilter, HRESULT* phr);
    virtual ~CDecoderOutputPin();

    STDMETHODIMP QueryAccept(const AM_MEDIA_TYPE* pmt);

    HRESULT InitAllocator(IMemAllocator **ppAlloc);

    HRESULT SetAllocator(IMemAllocator *pAlloc);

    HRESULT NotifyFormatChange(const CMediaType* pmt);

    HRESULT CompleteConnect(IPin *pReceivePin);

    HRESULT DecideAllocator(IMemInputPin *pPin, __deref_out IMemAllocator **ppAlloc);

protected:
    IMemAllocator* m_pOwnAllocator;

    BOOL m_bRequireOwnDSAllocator;

    BOOL m_bOwnDSAllocatorAgreed;

private:
    DISALLOW_COPY_AND_ASSIGN(CDecoderOutputPin);
};