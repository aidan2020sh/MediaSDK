// Copyright (c) 2004-2019 Intel Corporation
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

#include "mfx_common.h"
#if defined (MFX_ENABLE_MP3_AUDIO_DECODE)

#include "umc_defs.h"

#ifndef _MFX_MP3_DEC_DECODE_H_
#define _MFX_MP3_DEC_DECODE_H_

#include "mfxaudio.h"
#include "mfxaudio++int.h"
#include "mp3dec_own.h"
#include "umc_audio_decoder.h"
#include "umc_mp3_decoder.h"
#include "umc_mutex.h"

#include "ippcore.h"
#include "ipps.h"
#include "mfx_task.h"
#include "mfxpcp.h"

#include <queue>
#include <list>
#include <memory>

#define MAX_MP3_INPUT_DATA_SIZE 4096

namespace UMC
{
    class AudioData;
    class MP3Decoder;
    class MP3DecoderParams; 
} // namespace UMC



class AudioDECODE;
class CommonCORE;
class AudioDECODEMP3 : public AudioDECODE
{
public:
    static mfxStatus Query(AudioCORE *core, mfxAudioParam *in, mfxAudioParam *out);
    static mfxStatus QueryIOSize(AudioCORE *core, mfxAudioParam *par, mfxAudioAllocRequest *request);
    static mfxStatus DecodeHeader(AudioCORE *core, mfxBitstream *bs, mfxAudioParam* par);

    AudioDECODEMP3(AudioCORE *core, mfxStatus * sts);
    virtual ~AudioDECODEMP3(void);

    mfxStatus Init(mfxAudioParam *par);
    virtual mfxStatus Reset(mfxAudioParam *par);
    virtual mfxStatus Close(void);

    virtual mfxStatus GetAudioParam(mfxAudioParam *par);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *buffer_out);
    virtual mfxStatus DecodeFrameCheck(mfxBitstream *bs, mfxAudioFrame *buffer_out,
        MFX_ENTRY_POINT *pEntryPoint);
    virtual mfxStatus DecodeFrame(mfxBitstream *bs, mfxAudioFrame *buffer_out);

protected:
    virtual mfxStatus ConstructFrame(mfxBitstream *in, mfxBitstream *out, unsigned int *p_RawFrameSize);

    static mfxStatus FillAudioParamMP3(MP3Dec_com* res, mfxAudioParam *out);


    static mfxStatus MP3ECODERoutine(void *pState, void *pParam, mfxU32 threadNumber, mfxU32 callNumber);
    static mfxStatus MP3CompleteProc(void *pState, void *pParam, mfxStatus taskRes);

    mfxStatus CopyBitstream(mfxBitstream& bs, const mfxU8* ptr, mfxU32 bytes);
    void MoveBitstreamData(mfxBitstream& bs, mfxU32 offset);
    // UMC decoder 
    std::unique_ptr<UMC::MP3Decoder>  m_pMP3AudioDecoder;
    UMC::MediaData        mInData;
    UMC::MediaData        mOutData;

    mfxBitstream m_frame;

    mfxAudioParam m_vPar;

    AudioCORE * m_core;
    CommonCORE *m_CommonCore;
    eMFXPlatform m_platform;
    bool    m_isInit;

    UMC::Mutex m_mGuard;
    mfxU32 m_nUncompFrameSize;
};

#endif // _MFX_AAC_DEC_DECODE_H_
#endif
