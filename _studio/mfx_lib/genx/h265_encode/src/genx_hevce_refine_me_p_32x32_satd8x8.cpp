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
#include "../include/genx_hevce_refine_me_p_common.h"

_GENX_ matrix<uint1, SATD_BLOCKH, SATD_BLOCKW> g_m_src;
_GENX_ matrix<uint1, SATD_BLOCKH, SATD_BLOCKW> g_m_avg;

extern "C" _GENX_MAIN_
void RefineMeP32x32Satd8x8(
    SurfaceIndex SURF_MBDIST,
    SurfaceIndex SURF_MV,
    SurfaceIndex SURF_SRC,
    SurfaceIndex SURF_REF,
    SurfaceIndex SURF_HPEL_HORZ,
    SurfaceIndex SURF_HPEL_VERT,
    SurfaceIndex SURF_HPEL_DIAG)
{
    enum
    {
        CHUNKW = 32,
        CHUNKH = 32,
        BLOCKW = SATD_BLOCKW,
        BLOCKH = SATD_BLOCKH
    };

    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();

    vector< uint, 9 > sad;
    vector< int2,2 > mv;
    read(SURF_MV, mbX * MVDATA_SIZE, mbY, mv);

    uint x = mbX * CHUNKW;
    uint y = mbY * CHUNKH;

    sad = QpelRefinementSATD<CHUNKW, CHUNKH, BLOCKW, BLOCKH, MODE_SATD8>(SURF_SRC, SURF_REF, SURF_HPEL_HORZ, SURF_HPEL_VERT, SURF_HPEL_DIAG, x, y, mv[0], mv[1]);

    write(SURF_MBDIST, mbX * MBDIST_SIZE, mbY, SLICE1(sad, 0, 8));   // 32bytes is max until BDW
    write(SURF_MBDIST, mbX * MBDIST_SIZE + 32, mbY, SLICE1(sad, 8, 1));
}