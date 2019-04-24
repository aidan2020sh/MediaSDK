// Copyright (c) 2018-2019 Intel Corporation
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

#pragma once

#include "umc_defs.h"

#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE) || defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_va_base.h"

#if defined(_WIN32) || defined(_WIN64)

namespace UMC
{ class MediaData; }

namespace UMC_MPEG2_DECODER
{
    class MPEG2DecoderFrameInfo;
    class MPEG2DecoderFrame;
    class Packer
    {

    public:

        Packer(UMC::VideoAccelerator * va);
        virtual ~Packer();

        virtual UMC::Status GetStatusReport(void* pStatusReport, size_t size) = 0;
        virtual UMC::Status SyncTask(MPEG2DecoderFrame*, void * error) = 0;
        virtual UMC::Status QueryTaskStatus(uint32_t index, void * status, void * error) = 0;

        virtual void BeginFrame() = 0;
        virtual void EndFrame() = 0;

        virtual void PackAU(MPEG2DecoderFrame const&, uint8_t) = 0;

        static Packer* CreatePacker(UMC::VideoAccelerator * va);

    protected:

        UMC::VideoAccelerator *m_va;
    };


    class PackerDXVA
        : public Packer
    {

    public:

        PackerDXVA(UMC::VideoAccelerator * va);

        UMC::Status GetStatusReport(void * /*pStatusReport*/, size_t /*size*/)
        { return UMC::UMC_OK; }

        // Synchronize task
        UMC::Status SyncTask(MPEG2DecoderFrame* frame, void * error)
        { return m_va->SyncTask(frame->GetMemID(), error); }

        UMC::Status QueryTaskStatus(uint32_t index, void * status, void * error)
        { return m_va->QueryTaskStatus(index, status, error); }

        void BeginFrame();
        void EndFrame();

        // Pack the whole picture
        void PackAU(MPEG2DecoderFrame const&, uint8_t) override;

    protected:
        // Pack picture lice parameters
        void PackPicParams(const MPEG2DecoderFrame&, uint8_t);
        // Pack matrix parameters
        void PackQmatrix(const MPEG2DecoderFrameInfo&);
        // Pack slice parameters
        void PackSliceParams(const MPEG2DecoderFrameInfo&);

    private:
        void CreateSliceDataBuffer(const MPEG2DecoderFrameInfo&);
        void CreateSliceParamBuffer(const MPEG2DecoderFrameInfo&);
    };


}
#endif // defined(_WIN32) || defined(_WIN64)
#endif // MFX_ENABLE_MPEG2_VIDEO_DECODE