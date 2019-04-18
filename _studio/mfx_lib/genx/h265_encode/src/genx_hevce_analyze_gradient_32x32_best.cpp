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

extern "C" _GENX_MAIN_ void
AnalyzeGradient32x32Best(SurfaceIndex SURF_SRC,
                         SurfaceIndex SURF_GRADIENT_4x4,
                         SurfaceIndex SURF_GRADIENT_8x8,
                         SurfaceIndex SURF_GRADIENT_16x16,
                         SurfaceIndex SURF_GRADIENT_32x32,
                         uint4        width)
{
    InitGlobalVariables();
    enum {
        W = 32,
        H = 32,
        MODESIZE = sizeof(uint4),
    };

    static_assert(W % 32 == 0 && H % 32 == 0, "Width and Height must be multiple of 32");

    uint x = get_thread_origin_x();
    uint y = get_thread_origin_y();

    matrix<uint1, 6, 8> data;
    matrix<int2, 8, 8> dx, dy;
    matrix<uint2, 8, 8> amp;
    matrix<uint2, 8, 8> ang;
    vector<uint2, 40> histogram4x4;
    vector<uint2, 40> histogram8x8;
    vector<uint4, 40> histogram16x16;
    vector<uint4, 40> histogram32x32=0;
    const int yBase = y * H;
    const int xBase = x * W;
    uint offset;

    matrix<uint4, 8, 8> output4x4 = 0;
    matrix<uint4, 4, 4> output8x8 = 0;
    matrix<uint4, 2, 2> output16x16 = 0;
    matrix<uint4, 1, 1> output32x32 = 0;
    int yBlk16 = 0;
    int xBlk16 = 0;
//#pragma unroll
    for (yBlk16 = 0; yBlk16 < H; yBlk16 += 16) 
    {
//#pragma unroll
        for (xBlk16 = 0; xBlk16 < W; xBlk16 += 16) 
        {
            histogram16x16 = 0;
            int xBlk8 = 0;
            int yBlk8 = 0;
#pragma unroll
            for (yBlk8 = 0; yBlk8 < 16; yBlk8 += 8) 
            {
#pragma unroll
                for (xBlk8 = 0; xBlk8 < 16; xBlk8 += 8) 
                {
                    ReadGradient(SURF_SRC, xBase + (xBlk16 + xBlk8), yBase + (yBlk16 + yBlk8), dx, dy);
                    Gradiant2AngAmp_MaskAtan2(dx, dy, ang, amp);

                    histogram8x8 = 0;

#pragma unroll
                    for (int yBlk4 = 0; yBlk4 < 8; yBlk4 += 4) {
#pragma unroll
                        for (int xBlk4 = 0; xBlk4 < 8; xBlk4 += 4) {

                            histogram4x4 = 0;
                            Histogram_iselect(ang.select<4, 1, 4, 1>(yBlk4, xBlk4), amp.select<4, 1, 4, 1>(yBlk4, xBlk4), histogram4x4.select<histogram4x4.SZ, 1>(0));

                            histogram8x8 += histogram4x4;
                            FindBestMod(histogram4x4.select<36, 1>(0), output4x4((yBlk16 + yBlk8 + yBlk4) >> 2, (xBlk16 + xBlk8 + xBlk4) >> 2));
                        }
                    }

                    histogram16x16 += histogram8x8;
                    FindBestMod(histogram8x8.select<36, 1>(0), output8x8((yBlk16 + yBlk8) >> 3, (xBlk16 + xBlk8) >> 3));
                }
            }
            histogram32x32 += histogram16x16;
            FindBestMod(histogram16x16.select<36, 1>(0), output16x16(yBlk16 >> 4, xBlk16 >> 4));
        }
    }

    write(SURF_GRADIENT_4x4, x * (W / 4 * MODESIZE), y * (H / 4), output4x4);

    write(SURF_GRADIENT_8x8, x * (W / 8 * MODESIZE), y * (H / 8), output8x8);

    write(SURF_GRADIENT_16x16, x * (W / 16 * MODESIZE), y * (H / 16), output16x16);

    FindBestMod(histogram32x32.select<36, 1>(0), output32x32(0, 0));
    write(SURF_GRADIENT_32x32, x * (W / 32 * MODESIZE), y * (H / 32), output32x32);
}