// Copyright (c) 2012-2018 Intel Corporation
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

#pragma warning(disable: 4127)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)
#pragma warning(disable: 4189)
#pragma warning(disable: 4505)
#include <cm/cm.h>
#include <cm/cmtl.h>
#include <cm/genx_vme.h>
#include "../include/utility_genx.h"
#include "../include/genx_hevce_analyze_gradient_common.h"

using namespace cmut;

extern "C" _GENX_MAIN_  void
AnalyzeGradient3(SurfaceIndex SURF_SRC,
                 SurfaceIndex SURF_GRADIENT_4x4,
                 SurfaceIndex SURF_GRADIENT_8x8,
                 uint4        width)
{
    enum {
        W = 16,
        H = 16,
        HISTSIZE = sizeof(uint2)* 40,
        HISTBLOCKSIZE = 8 / 4 * HISTSIZE,
    };

    static_assert(W % 8 == 0 && H % 8 == 0, "Width and Height must be multiple of 8.");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8;
    const int yBase = y * H;
    const int xBase = x * W;

    const int histLineSize = (HISTSIZE / 4) * width;
    const int nextLine = histLineSize - HISTBLOCKSIZE;

#pragma unroll
    for (int yBlk8 = 0; yBlk8 < H; yBlk8 += 8) {
#pragma unroll
        for (int xBlk8 = 0; xBlk8 < W; xBlk8 += 8) {
            histogram8x8 = 0;
            ReadGradient(SURF_SRC, xBase + xBlk8, yBase + yBlk8, dx, dy);
            Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);

            uint offset = ((yBase + yBlk8) >> 2) * histLineSize + (xBase + xBlk8) * (HISTSIZE / 4);
#pragma unroll
            for (int yBlk4 = 0; yBlk4 < 8; yBlk4 += 4) {
#pragma unroll
                for (int xBlk4 = 0; xBlk4 < 8; xBlk4 += 4) {

                    histogram4x4 = 0;
                    Histogram_iselect(ang.select<4, 1, 4, 1>(yBlk4, xBlk4), amp.select<4, 1, 4, 1>(yBlk4, xBlk4), histogram4x4.select<histogram4x4.SZ, 1>(0));

                    histogram8x8 += histogram4x4;

                    write(SURF_GRADIENT_4x4, offset + 0, cmut_slice<32>(histogram4x4, 0));
                    write(SURF_GRADIENT_4x4, offset + 64, cmut_slice<8>(histogram4x4, 32));
                    offset += HISTSIZE;
                }
                offset += nextLine;
            }

            offset = ((width / 8) * ((yBase + yBlk8) >> 3) + ((xBase + xBlk8) >> 3)) * HISTSIZE;
            write(SURF_GRADIENT_8x8, offset + 0, cmut_slice<32>(histogram8x8, 0));
            write(SURF_GRADIENT_8x8, offset + 64, cmut_slice<8>(histogram8x8, 32));
        }
    }
}