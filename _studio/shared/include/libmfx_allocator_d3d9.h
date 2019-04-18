// Copyright (c) 2007-2018 Intel Corporation
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
#if defined  (MFX_VA_WIN)
#ifndef _LIBMFX_ALLOCATOR_D3D9_H_
#define _LIBMFX_ALLOCATOR_D3D9_H_

// disable the "nonstandard extension used : nameless struct/union" warning
// dxva2api.h warning
#pragma warning(disable: 4201)



#include <d3d9.h>
#include <dxva2api.h>


#define D3DFMT_BRGB4  (D3DFORMAT)MAKEFOURCC('B', 'R', 'G', 'B')
#define D3DFMT_IRW0  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '0') /**< @brief Bayer Pattern: BGGR, Bit Depth: 10/12 [16-bit aligned]  */
#define D3DFMT_IRW1  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '1') /**< @brief Bayer Pattern: RGGB, Bit Depth: 10/12 [16-bit aligned]  */
#define D3DFMT_IRW2  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '2') /**< @brief Bayer Pattern: GRBG, Bit Depth: 10/12 [16-bit aligned]  */
#define D3DFMT_IRW3  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '3') /**< @brief Bayer Pattern: GBRG, Bit Depth: 10/12 [16-bit aligned]  */
#define D3DFMT_IRW4  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '4') /**< @brief Bayer Pattern: BGGR, Bit Depth: 8  */
#define D3DFMT_IRW5  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '5') /**< @brief Bayer Pattern: RGGB, Bit Depth: 8  */
#define D3DFMT_IRW6  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '6') /**< @brief Bayer Pattern: GRBG, Bit Depth: 8  */
#define D3DFMT_IRW7  (D3DFORMAT)MAKEFOURCC('I', 'R', 'W', '7') /**< @brief Bayer Pattern: GBRG, Bit Depth: 8  */

#include "libmfx_allocator.h"

// Internal Allocators 
namespace mfxDefaultAllocatorD3D9
{

    mfxStatus AllocFramesHW(mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    mfxStatus LockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr);
    mfxStatus GetHDLHW(mfxHDL pthis, mfxMemId mid, mfxHDL *handle);
    mfxStatus UnlockFrameHW(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr=0);
    mfxStatus FreeFramesHW(mfxHDL pthis, mfxFrameAllocResponse *response);

    mfxStatus SetFrameData(const D3DSURFACE_DESC &desc, const D3DLOCKED_RECT &LockedRect, mfxFrameData *ptr);

    class mfxWideHWFrameAllocator : public  mfxBaseWideFrameAllocator
    {
    public:
        std::vector<IDirect3DSurface9*> m_SrfQueue;
        mfxWideHWFrameAllocator(mfxU16 type, IDirectXVideoDecoderService *pExtDirectXVideoService);
        virtual ~mfxWideHWFrameAllocator(void){};
        IDirectXVideoDecoderService *pDirectXVideoService;
    };
}

#endif
#endif
