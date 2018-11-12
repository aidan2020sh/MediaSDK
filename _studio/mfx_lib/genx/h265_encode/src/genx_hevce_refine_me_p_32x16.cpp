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
#include "..\include\genx_hevce_refine_me_p_common_old.h"


extern "C" _GENX_MAIN_
void RefineMeP32x16(SurfaceIndex SURF_MBDIST_32x16, SurfaceIndex SURF_MV32X16,
                    SurfaceIndex SURF_SRC_1X, SurfaceIndex SURF_REF_1X)
{
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector< uint,9 > sad32x16;
    vector< int2,2 > mv32x16;
    read(SURF_MV32X16, mbX * MVDATA_SIZE, mbY, mv32x16);

    uint x = mbX * 32;
    uint y = mbY * 16;

    QpelRefine(32, 16, SURF_SRC_1X, SURF_REF_1X, x, y, mv32x16[0], mv32x16[1], sad32x16);

    write(SURF_MBDIST_32x16, mbX * MBDIST_SIZE, mbY, SLICE1(sad32x16, 0, 8));   // 32bytes is max until BDW
    write(SURF_MBDIST_32x16, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad32x16, 8, 1));
}
