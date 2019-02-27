// Copyright (c) 2014-2018 Intel Corporation
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
#include "mfx_common_int.h"

#include "umc_defs.h"

#ifndef _MFX_AV1_DEC_DECODE_H_
#define _MFX_AV1_DEC_DECODE_H_

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)

class mfx_UMC_FrameAllocator;

namespace UMC
{
    class VideoDecoderParams;
}

namespace UMC_AV1_DECODER
{
    class AV1Decoder;
    class AV1DecoderFrame;
    class AV1DecoderParams;
}

using UMC_AV1_DECODER::AV1DecoderFrame;

class VideoDECODEAV1
    : public VideoDECODE
{
    struct TaskInfo
    {
        mfxFrameSurface1 *surface_work;
        mfxFrameSurface1 *surface_out;
    };

public:

    VideoDECODEAV1(VideoCORE*, mfxStatus*);
    virtual ~VideoDECODEAV1();

    static mfxStatus Query(VideoCORE*, mfxVideoParam* in, mfxVideoParam* out);
    static mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam*, mfxFrameAllocRequest*);
    static mfxStatus DecodeHeader(VideoCORE*, mfxBitstream*, mfxVideoParam*);

    mfxStatus Init(mfxVideoParam*);
    mfxStatus Reset(mfxVideoParam*);
    mfxStatus Close();
    mfxTaskThreadingPolicy GetThreadingPolicy();

    mfxStatus GetVideoParam(mfxVideoParam*);
    mfxStatus GetDecodeStat(mfxDecodeStat*);
    mfxStatus DecodeFrameCheck(mfxBitstream*, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, MFX_ENTRY_POINT*);
    mfxStatus SetSkipMode(mfxSkipMode);
    mfxStatus GetPayload(mfxU64* time_stamp, mfxPayload*);

    mfxStatus SubmitFrame(mfxBitstream*, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, mfxThreadTask*);
    mfxStatus QueryFrame(mfxThreadTask);

private:

    static mfxStatus FillVideoParam(VideoCORE*, UMC_AV1_DECODER::AV1DecoderParams const*, mfxVideoParam*);
    static mfxStatus DecodeRoutine(void* state, void* param, mfxU32, mfxU32);
    static mfxStatus CompleteProc(void*, void* param, mfxStatus);

    mfxStatus SubmitFrame(mfxBitstream* bs, mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out);

    AV1DecoderFrame* GetFrameToDisplay();
    void FillOutputSurface(mfxFrameSurface1* surface_work, mfxFrameSurface1** surface_out, AV1DecoderFrame*);

    mfxFrameSurface1* GetOriginalSurface(mfxFrameSurface1* surface)
    {
        VM_ASSERT(m_core);

        return m_opaque ?
            m_core->GetNativeSurface(surface) : surface;
    }

    mfxStatus DecodeFrame(mfxFrameSurface1 *surface_out, AV1DecoderFrame* pFrame);

private:

    VideoCORE*                                   m_core;
    eMFXPlatform                                 m_platform;

    UMC::Mutex                                   m_guard;
    std::unique_ptr<mfx_UMC_FrameAllocator>      m_allocator;
    std::unique_ptr<UMC_AV1_DECODER::AV1Decoder> m_decoder;

    bool                                         m_opaque;
    bool                                         m_first_run;

    mfxVideoParamWrapper                         m_init_video_par;
    mfxVideoParamWrapper                         m_video_par;

    mfxFrameAllocResponse                        m_response;
};

#endif // MFX_ENABLE_AV1_VIDEO_DECODE

#endif // _MFX_AV1_DEC_DECODE_H_