// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "umc_defs.h"

#if defined (UMC_ENABLE_DSOUND_AUDIO_RENDER)

#include <vm_debug.h>
#include "ipps.h"
#include "vm_debug.h"
#include "dsound_buf.h"

UMC::Status UMC::DSBuffer::Init(const HWND hWnd,
                                const Ipp32u dwBufferSize,
                                const Ipp16u wChannels,
                                const Ipp32u dwFrequency,
                                const Ipp16u wBytesPerSample)
{
    Status umcRes = UMC_OK;

    VM_ASSERT(0 != dwBufferSize);
    VM_ASSERT(0 != wChannels);
    VM_ASSERT(0 != dwFrequency);
    VM_ASSERT(0 != wBytesPerSample);

    Close();

    m_dwInitedBufferSize    = dwBufferSize;
    m_wInitedChannels       = wChannels;
    m_dwInitedFrequency     = dwFrequency;
    m_wInitedBytesPerSample = wBytesPerSample;
    if (UMC_OK == umcRes)
    {
        m_hWnd = hWnd;
        if (NULL == m_hWnd) {    umcRes = UMC_ERR_FAILED;    }
    }

    if (UMC_OK == umcRes)
    {
        HRESULT hRes = DirectSoundCreate8( NULL, &m_pDS, NULL );
        if (FAILED(hRes))
        {
            vm_debug_trace(VM_DEBUG_ERROR, VM_STRING("Failed to initialize DirectSound 8 @DirectSoundRender"));
            umcRes = UMC_ERR_FAILED;
        }
    }

    if (UMC_OK == umcRes && FAILED(m_pDS->SetCooperativeLevel(m_hWnd, DSSCL_PRIORITY)))
    {    umcRes = UMC_ERR_FAILED;}

    DSBUFFERDESC dsbd;
    // Get the primary buffer
    if (UMC_OK == umcRes)
    {
        ippsZero_8u( (Ipp8u*)&dsbd, sizeof(DSBUFFERDESC) );
        dsbd.dwSize        = sizeof(DSBUFFERDESC);
        dsbd.dwFlags       = DSBCAPS_PRIMARYBUFFER;
        dsbd.dwBufferBytes = 0;
        dsbd.lpwfxFormat   = NULL;

        if ( FAILED(m_pDS->CreateSoundBuffer( &dsbd, &m_pDSBPrimary, NULL ) ) )
        {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to create DirectSound primary buffer @Open"));
            umcRes = UMC_ERR_ALLOC;
        }
    }

    //    Set primary buffer format
    WAVEFORMATEX wfx;
    if (UMC_OK == umcRes)
    {
        ippsZero_8u( (Ipp8u*)&wfx, sizeof(WAVEFORMATEX) );
        wfx.wFormatTag      = WAVE_FORMAT_PCM;
        wfx.nChannels       = wChannels;
        wfx.nSamplesPerSec  = dwFrequency;
        wfx.wBitsPerSample  = (Ipp16u)(wBytesPerSample << 3);
        wfx.nBlockAlign     = (Ipp16u) (wfx.wBitsPerSample / 8 * wfx.nChannels);
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

        if ( FAILED(m_pDSBPrimary->SetFormat(&wfx) ) )
        {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to set PCM format for DirectSound primary buffer @Open"));
            umcRes = UMC_ERR_OPEN_FAILED;
        }
    }

    //    Create Secondary buffer
    if (UMC_OK == umcRes)
    {
        dsbd.dwSize        = sizeof(DSBUFFERDESC);
        dsbd.dwFlags       = DSBCAPS_GLOBALFOCUS|DSBCAPS_CTRLVOLUME|
                             DSBCAPS_CTRLFREQUENCY|DSBCAPS_GETCURRENTPOSITION2;
        dsbd.dwBufferBytes = dwBufferSize;
        dsbd.lpwfxFormat   = &wfx;

        if ( FAILED(m_pDS->CreateSoundBuffer( &dsbd, &m_pDSBSecondary, NULL )))
        {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to create DirectSound secondary buffer @Open"));
            umcRes = UMC_ERR_ALLOC;
        }
    }

    if (UMC_OK == umcRes)
    {
        void *lock1_ptr, *lock2_ptr;
        Ipp32u lock1_bytes, lock2_bytes;
        if (FAILED(m_pDSBSecondary->Lock(0, m_dwDSBufferSize,
                                         &lock1_ptr, (LPDWORD)&lock1_bytes,
                                         &lock2_ptr, (LPDWORD)&lock2_bytes,
                                         DSBLOCK_ENTIREBUFFER)))
        {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to lock DirectSound secondary buffer @Open"));
            umcRes = UMC_ERR_OPEN_FAILED;
        }
        else
        {
            ippsZero_8u((Ipp8u*)lock1_ptr,lock1_bytes);
            m_pDSBSecondary->Unlock(lock1_ptr,lock1_bytes,lock2_ptr,0);
        }
    }

    DSCAPS   dscaps = {0};
    if (UMC_OK == umcRes)
    {
        ippsZero_8u( (Ipp8u*)&dscaps, sizeof(DSCAPS) );
        dscaps.dwSize = sizeof(DSCAPS);
        if (FAILED(m_pDS->GetCaps(&dscaps)))
        {    umcRes = UMC_ERR_FAILED;    }
    }

    if(UMC_OK == umcRes)
    {
        m_dfCompensation = (dscaps.dwFlags & DSCAPS_EMULDRIVER) ? 0.020 : 0.005;
        m_dfNorm = 1.0/dwFrequency/wBytesPerSample/wChannels;
        m_dwNextWriteOffset = 0;
    }

    if (UMC_OK == umcRes)
    {
        m_dwDSBufferSize = dwBufferSize;
        m_bPausedWaitingData = true;
    }
    else
    {    Close();    }
    return umcRes;
}

UMC::Status UMC::DSBuffer::GetPlayPos(Ipp32u& rdwPos)
{
    if (!m_pDSBSecondary)
    {
        return UMC_ERR_FAILED;
    }
    m_MutAccess.Lock();
    Status umcRes = (m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;
    if (UMC_OK == umcRes &&
        FAILED(m_pDSBSecondary->GetCurrentPosition((LPDWORD)&rdwPos, NULL)))
    {
        rdwPos = 0xFFFFFFFF;
        umcRes = UMC_ERR_FAILED;
    }
    m_MutAccess.Unlock();
    return umcRes;
}

UMC::Status UMC::DSBuffer::GetWritePos(Ipp32u& rdwPos)
{
    if (!m_pDSBSecondary)
    {
        return UMC_ERR_FAILED;
    }
    m_MutAccess.Lock();
    Status umcRes = (m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;
    if (UMC_OK == umcRes &&
        FAILED(m_pDSBSecondary->GetCurrentPosition(NULL, (LPDWORD)&rdwPos)))
    {
        rdwPos = 0xFFFFFFFF;
        umcRes = UMC_ERR_FAILED;
    }
    m_MutAccess.Unlock();
    return umcRes;
}

UMC::Status UMC::DSBuffer::CopyDataToBuffer(Ipp8u* pucData,
                                            Ipp32u dwLength,
                                            Ipp32u& rdwBytesWrote)
{
    void *lock1_ptr = NULL;
    void *lock2_ptr = NULL;
    Ipp32u lock1_bytes = 0;
    Ipp32u lock2_bytes = 0;

    HRESULT hRes = DS_OK;

    if (!m_pDSBSecondary)
    {
        return UMC_ERR_FAILED;
    }

    if (dwLength == 0)
    {
      return UMC_OK;
    }

    m_MutAccess.Lock();

    Status umcRes = (NULL != m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;

    if (UMC_OK == umcRes) {
        hRes = m_pDSBSecondary->Lock(m_dwNextWriteOffset,
                                     IPP_MIN(dwLength,m_dwDSBufferSize),
                                     &lock1_ptr, (LPDWORD)&lock1_bytes,
                                     &lock2_ptr, (LPDWORD)&lock2_bytes, 0);
        if (DSERR_BUFFERLOST == hRes) {
            m_pDSBSecondary->Restore();
            hRes = m_pDSBSecondary->Lock(m_dwNextWriteOffset,
                                         IPP_MIN(dwLength,m_dwDSBufferSize),
                                         &lock1_ptr, (LPDWORD)&lock1_bytes,
                                         &lock2_ptr, (LPDWORD)&lock2_bytes, 0);
        }
        if (DS_OK != hRes) {    umcRes = UMC_ERR_FAILED;    }
    }

    if (UMC_OK == umcRes)
    {
        ippsCopy_8u(pucData, (Ipp8u*)lock1_ptr, lock1_bytes);
        hRes = m_pDSBSecondary->Unlock(lock1_ptr, lock1_bytes, lock2_ptr,0);
        if (DS_OK != hRes) {    umcRes = UMC_ERR_FAILED;    }
    }

    if (UMC_OK == umcRes) {
        m_dwNextWriteOffset = (m_dwNextWriteOffset + lock1_bytes) % m_dwDSBufferSize;
        rdwBytesWrote = lock1_bytes;
    }

/*
    if (UMC_OK == umcRes && m_bPausedWaitingData &&
        (m_dwDSBufferSize /2 < m_dwNextWriteOffset ||
        (0 == m_dwNextWriteOffset && 0 != rdwBytesWrote)))
*/
    if (UMC_OK == umcRes && m_bPausedWaitingData && rdwBytesWrote)
    {
        if (FAILED(m_pDSBSecondary->Play( 0, 0, DSBPLAY_LOOPING)))
        {    umcRes = UMC_ERR_FAILED;    }
        else
        {    m_bPausedWaitingData = false;    }
    }
    m_MutAccess.Unlock();

    return umcRes;
}

UMC::Status UMC::DSBuffer::CopyZerosToBuffer(Ipp32u dwLength)
{
    void *lock1_ptr = NULL;
    void *lock2_ptr = NULL;
    Ipp32u lock1_bytes = 0;
    Ipp32u lock2_bytes = 0;

    HRESULT hRes = DS_OK;

    if (!m_pDSBSecondary) {
        return UMC_ERR_FAILED;
    }

    if (dwLength == 0)
      dwLength = m_dwDSBufferSize;

    m_MutAccess.Lock();

    Status umcRes = (NULL != m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;

    if (UMC_OK == umcRes) {
        hRes = m_pDSBSecondary->Lock(m_dwNextWriteOffset,
                                     IPP_MIN(dwLength,m_dwDSBufferSize),
                                     &lock1_ptr, (LPDWORD)&lock1_bytes,
                                     &lock2_ptr, (LPDWORD)&lock2_bytes, 0);
        if (DSERR_BUFFERLOST == hRes) {
            m_pDSBSecondary->Restore();
            hRes = m_pDSBSecondary->Lock(m_dwNextWriteOffset,
                                         IPP_MIN(dwLength,m_dwDSBufferSize),
                                         &lock1_ptr, (LPDWORD)&lock1_bytes,
                                         &lock2_ptr, (LPDWORD)&lock2_bytes, 0);
        }
        if (DS_OK != hRes) {    umcRes = UMC_ERR_FAILED;    }
    }

    if (UMC_OK == umcRes) {
        ippsZero_8u((Ipp8u*)lock1_ptr, lock1_bytes);
        hRes = m_pDSBSecondary->Unlock(lock1_ptr, lock1_bytes, lock2_ptr,0);
        if (DS_OK != hRes) {    umcRes = UMC_ERR_FAILED;    }
    }

    if (UMC_OK == umcRes) {
        m_dwNextWriteOffset =  (m_dwNextWriteOffset + lock1_bytes) % m_dwDSBufferSize;
    }

    m_MutAccess.Unlock();

    return umcRes;
}

UMC::Status UMC::DSBuffer::Pause(bool bPause)
{
    if (!m_pDSBSecondary)
    {
        return UMC_ERR_FAILED;
    }
    m_MutAccess.Lock();
    Status umcRes = (NULL != m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;

    if (UMC_OK == umcRes)
    {
        if (bPause)
        {
            if (FAILED(m_pDSBSecondary->Stop()))
            {
                umcRes = UMC_ERR_FAILED;
            }
        }
        else
        {
            if (FAILED(m_pDSBSecondary->Play( 0, 0, DSBPLAY_LOOPING)))
            {
                umcRes = UMC_ERR_FAILED;
            }
        }
    }
    m_MutAccess.Unlock();
    return umcRes;
}

Ipp32f UMC::DSBuffer::SetVolume(Ipp32f fVolume)
{
    if (!m_pDSBSecondary)
    {
        return 0;
    }
    m_MutAccess.Lock();
    LONG previous_volume;
    m_pDSBSecondary->GetVolume(&previous_volume);
    fVolume = IPP_MIN(1,IPP_MAX(0,fVolume));
    HRESULT dr = m_pDSBSecondary->SetVolume((LONG)(fVolume*(DSBVOLUME_MAX-DSBVOLUME_MIN)+DSBVOLUME_MIN));
    switch (dr) {
    case DSERR_CONTROLUNAVAIL:
    case DSERR_GENERIC:
    case DSERR_INVALIDPARAM:
    case DSERR_PRIOLEVELNEEDED:
        dr = 1;
        break;
    }
    m_MutAccess.Unlock();
    return (Ipp32f)(previous_volume-DSBVOLUME_MIN)/(DSBVOLUME_MAX-DSBVOLUME_MIN);
}

Ipp32f UMC::DSBuffer::GetVolume()
{
    LONG previous_volume;
    if (!m_pDSBSecondary)
    {
        return 0;
    }
    m_MutAccess.Lock();
    m_pDSBSecondary->GetVolume(&previous_volume);
    m_MutAccess.Unlock();
    return (Ipp32f)(previous_volume-DSBVOLUME_MIN)/(DSBVOLUME_MAX-DSBVOLUME_MIN);
}

UMC::Status UMC::DSBuffer::Reset()
{
     if (!m_pDSBSecondary)
    {
        return UMC_ERR_FAILED;
    }
    m_MutAccess.Lock();
    Status umcRes = (m_pDSBSecondary) ? UMC_OK : UMC_ERR_NOT_INITIALIZED;

    if (UMC_OK == umcRes) {    umcRes = Pause(true);    }

    void *lock1_ptr = NULL;
    void *lock2_ptr = NULL;
    Ipp32u lock1_bytes = 0;
    Ipp32u lock2_bytes = 0;

    if (UMC_OK == umcRes) {
        if (FAILED(m_pDSBSecondary->Lock(0, m_dwDSBufferSize,
                                         &lock1_ptr, (LPDWORD)&lock1_bytes,
                                         &lock2_ptr, (LPDWORD)&lock2_bytes,
                                         DSBLOCK_ENTIREBUFFER)))
        {
            vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to lock DirectSound secondary buffer @Open"));
            umcRes = UMC_ERR_FAILED;
        }
    }

    if (UMC_OK == umcRes) {
        ippsZero_8u((Ipp8u*)lock1_ptr,lock1_bytes);
        m_pDSBSecondary->Unlock(lock1_ptr, lock1_bytes, lock2_ptr, 0);
        m_bPausedWaitingData = true;
        m_pDSBSecondary->SetCurrentPosition(0);
        m_dwNextWriteOffset = 0;
    }
    m_MutAccess.Unlock();
    return umcRes;
}

void UMC::DSBuffer::Close()
{
    if (!m_pDSBSecondary)
    {
        return;
    }
    m_MutAccess.Lock();
    if (m_pDSBPrimary)
        m_pDSBPrimary->Play(0,0,DSBPLAY_LOOPING);

    if (m_pDSBSecondary) {
        Pause(true);
        m_pDSBSecondary->Release();
        m_pDSBSecondary = NULL;
    }

    if (m_pDSBPrimary) {
        m_pDSBPrimary->Release();
        m_pDSBPrimary = NULL;
    }

    if (m_pDS) {
        m_pDS->Release();
        m_pDS = NULL;
    }
    m_dwDSBufferSize = 0;
    m_dwNextWriteOffset = 0;
    m_dfNorm = 1;
    m_dfCompensation = 0;
    m_hWnd = NULL;
    m_bPausedWaitingData = false;

    m_MutAccess.Unlock();
}

UMC::Status UMC::DSBuffer::DynamicSetParams(const Ipp16u wChannels,
                                            const Ipp32u dwFrequency,
                                            const Ipp16u /*wBytesPerSample*/)
{
    UMC::Status umcRes = UMC::UMC_OK;
    WAVEFORMATEX wfx;
    ippsZero_8u( (Ipp8u*)&wfx, sizeof(WAVEFORMATEX) );

    m_pDSBPrimary->GetFormat(&wfx, sizeof(WAVEFORMATEX), NULL);
    Pause(true);

    if ((wfx.nChannels == wChannels) && (dwFrequency != wfx.nSamplesPerSec))
    {
        if ( FAILED(m_pDSBSecondary->SetFrequency(dwFrequency) ) )
        {
            vm_debug_trace(VM_DEBUG_NONE, VM_STRING("Failed to set Frequency for DirectSound buffer @DynamicSetParams"));
            umcRes = UMC_ERR_OPEN_FAILED;
        }
        m_dfNorm = 1.0/dwFrequency/m_wInitedBytesPerSample/wChannels;
    }
    else
    {        
        {
            m_MutAccess.Lock();
            
            if (m_pDSBSecondary) {
                m_pDSBSecondary->Release();
                m_pDSBSecondary = NULL;
            }
            
            DSBUFFERDESC dsbd;

            //    Set primary buffer format
            WAVEFORMATEX wfx;
            if (UMC_OK == umcRes)
            {
                ippsZero_8u( (Ipp8u*)&wfx, sizeof(WAVEFORMATEX) );
                wfx.wFormatTag      = WAVE_FORMAT_PCM;
                wfx.nChannels       = wChannels;
                wfx.nSamplesPerSec  = dwFrequency;
                wfx.wBitsPerSample  = (Ipp16u)(m_wInitedBytesPerSample << 3);
                wfx.nBlockAlign     = (Ipp16u) (wfx.wBitsPerSample / 8 * wfx.nChannels);
                wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

                if( FAILED(m_pDSBPrimary->SetFormat(&wfx) ) )
                {
                    vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to set PCM format for DirectSound primary buffer @Open"));
                    umcRes = UMC_ERR_OPEN_FAILED;
                }

            }
            //    Create Secondary buffer
            if (UMC_OK == umcRes)
            {
                ippsZero_8u( (Ipp8u*)&dsbd, sizeof(DSBUFFERDESC) );
                dsbd.dwSize        = sizeof(DSBUFFERDESC);
                dsbd.dwFlags       = DSBCAPS_STICKYFOCUS|DSBCAPS_CTRLVOLUME|
                                     DSBCAPS_CTRLFREQUENCY|DSBCAPS_GETCURRENTPOSITION2;
                dsbd.dwBufferBytes = m_dwInitedBufferSize;
                dsbd.lpwfxFormat   = &wfx;

                if ( FAILED(m_pDS->CreateSoundBuffer( &dsbd, &m_pDSBSecondary, NULL )))
                {
                    vm_debug_trace(VM_DEBUG_ALL, VM_STRING("Failed to create DirectSound secondary buffer @Open"));
                    umcRes = UMC_ERR_ALLOC;
                }
            }

            m_dfNorm = 1.0/dwFrequency/m_wInitedBytesPerSample/wChannels;
            m_MutAccess.Unlock();
        }
    }
    Pause(false);
    return umcRes;
}

#endif // UMC_ENABLE_DSOUND_AUDIO_RENDER