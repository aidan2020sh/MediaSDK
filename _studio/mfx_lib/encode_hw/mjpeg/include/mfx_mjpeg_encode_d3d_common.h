// Copyright (c) 2011-2019 Intel Corporation
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

#ifndef __MFX_MJPEG_ENCODE_D3D_COMMON_H
#define __MFX_MJPEG_ENCODE_D3D_COMMON_H

#include "mfx_common.h"

#if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && defined (MFX_VA_WIN)

#include "encoding_ddi.h"
#include "mfx_mjpeg_encode_interface.h"

namespace MfxHwMJpegEncode
{
    class D3DXCommonEncoder : public DriverEncoder
    {
    public:
        D3DXCommonEncoder();

        virtual ~D3DXCommonEncoder();

#ifdef MFX_ENABLE_HW_BLOCKING_TASK_SYNC
        mfxStatus InitCommonEnc(VideoCORE *pCore);
#endif

        virtual mfxStatus QueryStatus(DdiTask & task);

        virtual mfxStatus Execute(DdiTask &task, mfxHDL surface);

    protected:
        // async call
        virtual mfxStatus QueryStatusAsync(DdiTask & task) = 0;

        // sync call
        virtual mfxStatus WaitTaskSync(DdiTask & task, mfxU32 timeOutMs);

        virtual mfxStatus ExecuteImpl(DdiTask &task, mfxHDL surface) = 0;

    private:
        D3DXCommonEncoder(D3DXCommonEncoder const &); // no implementation
        D3DXCommonEncoder & operator =(D3DXCommonEncoder const &); // no implementation

    };
}; // namespace

#endif // #if defined (MFX_ENABLE_MJPEG_VIDEO_ENCODE) && (MFX_VA_WIN)
#endif // __MFX_MJPEG_ENCODE_D3D_COMMON_H
/* EOF */
