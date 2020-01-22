// Copyright (c) 2012-2019 Intel Corporation
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
#include "../include/genx_av1_me_common.h"

#if !defined(target_gen7_5) && !defined(target_gen8) && !defined(target_gen9) && !defined(target_gen10) && !defined(target_gen11) && !defined(target_gen11lp) && !defined(target_gen12) && !defined(CMRT_EMU)
#error One of macro should be defined: target_gen7_5, target_gen8, target_gen9, target_gen10, target_gen11, target_gen11lp, target_gen12
#endif

#ifdef target_gen7_5
typedef matrix<uchar, 3, 32> UniIn;
#else
typedef matrix<uchar, 4, 32> UniIn;
#endif

static const int2 MAX_MV_LEN_IN_PELS = (16383 >> 1) >> 3;

extern "C" _GENX_MAIN_
void MeP16_4MV(SurfaceIndex SURF_CONTROL, SurfaceIndex SURF_SRC_AND_REF, SurfaceIndex SURF_DIST16x16,
           SurfaceIndex SURF_DIST16x8, SurfaceIndex SURF_DIST8x16, SurfaceIndex SURF_DIST8x8,
           /*SurfaceIndex SURF_DIST8x4, SurfaceIndex SURF_DIST4x8, */SurfaceIndex SURF_MV16x16,
           SurfaceIndex SURF_MV16x8, SurfaceIndex SURF_MV8x16, SurfaceIndex SURF_MV8x8/*,
           SurfaceIndex SURF_MV8x4, SurfaceIndex SURF_MV4x8*/, int rectParts, int start_mbX, int start_mbY)
{
    uint mbX = get_thread_origin_x() + start_mbX;
    uint mbY = get_thread_origin_y() + start_mbY;
    uint x = mbX * 16;
    uint y = mbY * 16;

    vector<uchar, 64> control;
    read(SURF_CONTROL, 0, control);

    uint1 maxNumSu = control.format<uint1>()[56];
    uint1 lenSp = control.format<uint1>()[57];
    uint2 width = control.format<uint2>()[30];
    uint2 height = control.format<uint2>()[31];

    uint2 picWidthInMB = width >> 4;
    uint mbIndex = picWidthInMB * mbY + mbX;

    // read MB record data
    UniIn uniIn = 0;
    matrix<uchar, 9, 32> imeOut;
    matrix<uchar, 2, 32> imeIn = 0;
    matrix<uchar, 4, 32> fbrIn;

    // declare parameters for VME
    matrix<uint4, 16, 2> costs = 0;
    vector<int2, 2> mvPred = 0;
    read(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, mvPred); // these pred MVs will be updated later here

    // load search path
    imeIn = SLICE1(control, 0, 64);

    // M0.2
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);

    // M0.3 various prediction parameters
///    VME_SET_DWORD(uniIn, 0, 3, 0x76a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x76: 8x8,16x16
    VME_SET_DWORD(uniIn, 0, 3, 0x70a40000); // BMEDisableFBR=1 InterSAD=2 SubMbPartMask=0x70: 8x8,8x16,16x8,16x16 (rect subparts are for FEI are on)
    // M1.1 MaxNumMVs
    VME_SET_UNIInput_MaxNumMVs(uniIn, 32);
    // M0.5 Reference Window Width & Height
    VME_SET_UNIInput_RefW(uniIn, 48);
    VME_SET_UNIInput_RefH(uniIn, 40);

    // M0.0 Ref0X, Ref0Y
    vector_ref<int2, 2> sourceXY = uniIn.row(0).format<int2>().select<2,1>(4);
    vector<uint1, 2> widthHeight;
    widthHeight[0] = (height >> 4) - 1;
    widthHeight[1] = (width >> 4);
    vector_ref<int1, 2> searchWindow = uniIn.row(0).format<int1>().select<2,1>(22);
    vector_ref<int2, 2> ref0XY = uniIn.row(0).format<int2>().select<2,1>(0);
    SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY, MAX_MV_LEN_IN_PELS);

    // M1.0-3 Search path parameters & start centers & MaxNumMVs again!!!
    VME_SET_UNIInput_AdaptiveEn(uniIn);
    VME_SET_UNIInput_T8x8FlagForInterEn(uniIn);
    VME_SET_UNIInput_MaxNumMVs(uniIn, 0x3f);
    VME_SET_UNIInput_MaxNumSU(uniIn, maxNumSu);
    VME_SET_UNIInput_LenSP(uniIn, lenSp);

    // M1.2 Start0X, Start0Y
    vector<int1, 2> start0 = searchWindow;
    start0 = ((start0 - 16) >> 3) & 0x0f;
    uniIn.row(1)[10] = start0[0] | (start0[1] << 4);

    uniIn.row(1)[6] = 0x20;
    uniIn.row(1)[31] = 0x1;

    vector<int2,2>  ref0       = uniIn.row(0).format<int2>().select<2, 1> (0);
#ifdef target_gen7_5
    vector<uint2,4> costCenter = uniIn.row(1).format<uint2>().select<4, 1> (8);
#else
    vector<uint2,16> costCenter = uniIn.row(3).format<uint2>().select<16, 1> (0);
#endif

    run_vme_ime(uniIn, imeIn,
        VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
        SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);

    vector<int2,2> mv16 = imeOut.row(7).format<int2>().select<2,1>(10);
    matrix<int2,2,4> mv8 = imeOut.row(8).format<int2>().select<8,1>(8); // 4 MVs

    vector<int2,2> diff = cm_abs<int2>(mvPred);
    diff = diff > 16;
    if (diff.any()) {
        uint2 dist16 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8 = imeOut.row(7).format<ushort>().select<4,1>(4);

        mvPred = 0;
        SetRef(sourceXY, mvPred, searchWindow, widthHeight, ref0XY, MAX_MV_LEN_IN_PELS);
        run_vme_ime(uniIn, imeIn,
            VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START,
            SURF_SRC_AND_REF, ref0XY, NULL, costCenter, imeOut);

        uint2 dist16_0 = imeOut.row(7).format<ushort>()[8];
        vector<uint2,4> dist8_0 = imeOut.row(7).format<ushort>().select<4,1>(4);
        vector<int2,2> mv16_0 = imeOut.row(7).format<int2>().select<2,1>(10);
        matrix<int2,2,4> mv8_0 = imeOut.row(8).format<int2>().select<8,1>(8);

        mv16.format<uint4>().merge(mv16_0.format<uint4>(), dist16_0 < dist16);
        mv8.format<uint4>().merge(mv8_0.format<uint4>(), dist8_0 < dist8);
    }

    //DebugUniOutput<9>(imeOut);

    VME_SET_UNIInput_SubPelMode(uniIn, 3);
    VME_SET_UNIInput_BMEDisableFBR(uniIn);
    SLICE(fbrIn.format<uint>(), 1, 16, 2) = 0; // zero L1 motion vectors

    matrix<uchar, 7, 32> fbrOut16x16;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<4, 1, 4, 2>(0, 0) = mv16.format<uint4>()[0]; // motion vectors 16x16
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 0, 0, 0, fbrOut16x16);

    matrix<uchar, 7, 32> fbrOut8x8;
    VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(0, 0) = mv8.format<uint>()[0]; // motion vectors 8x8_0
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(1, 0) = mv8.format<uint>()[1]; // motion vectors 8x8_1
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(2, 0) = mv8.format<uint>()[2]; // motion vectors 8x8_2
    fbrIn.format<uint, 4, 8>().select<1, 1, 4, 2>(3, 0) = mv8.format<uint>()[3]; // motion vectors 8x8_3
    run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, 0, 0, fbrOut8x8);

    // distortions
    // 16x16
    matrix<uint, 1, 1> dist16x16 = SLICE(fbrOut16x16.row(5).format<ushort>(), 0, 1, 1);
    write(SURF_DIST16x16, mbX * DIST_SIZE, mbY, dist16x16);
    // 8x8
    matrix<uint, 2, 2> dist8x8 = SLICE(fbrOut8x8.row(5).format<ushort>(), 0, 4, 4);
    write(SURF_DIST8x8, mbX * DIST_SIZE * 2, mbY * 2, dist8x8);

    // motion vectors
    // 16x16
    write(SURF_MV16x16, mbX * MVDATA_SIZE, mbY, SLICE(fbrOut16x16.format<uint>(), 8, 1, 1));
    // 8x8
    matrix<uint, 2, 2> mv8x8 = SLICE(fbrOut8x8.format<uint>(), 8, 4, 8);
    write(SURF_MV8x8,   mbX * MVDATA_SIZE * 2, mbY * 2, mv8x8);


    if (rectParts)
    {
        matrix<uchar, 7, 32> fbrOut16x8;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 1);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[0]; // motion vectors 16x8_0
        fbrIn.format<uint, 4, 8>().select<2, 1, 4, 2>(2, 0) = imeOut.row(8).format<uint>()[1]; // motion vectors 16x8_1
        run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 1, 0, 0, fbrOut16x8);

        matrix<uchar, 7, 32> fbrOut8x16;
        VME_SET_UNIInput_FBRMbModeInput(uniIn, 2);
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, 0);
        VME_SET_UNIInput_FBRSubPredModeInput(uniIn, 0);
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(0, 0) = imeOut.row(8).format<uint>()[2]; // motion vectors 8x16_0
        fbrIn.format<uint, 4, 8>().select<2, 2, 4, 2>(1, 0) = imeOut.row(8).format<uint>()[3]; // motion vectors 8x16_1
        run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 2, 0, 0, fbrOut8x16);

        // distortions
        // 16x8
        matrix<uint, 2, 1> dist16x8 = SLICE(fbrOut16x8.row(5).format<ushort>(), 0, 2, 8);
        write(SURF_DIST16x8, mbX * DIST_SIZE, mbY * 2, dist16x8);
        // 8x16
        matrix<uint, 1, 2> dist8x16 = SLICE(fbrOut8x16.row(5).format<ushort>(), 0, 2, 4);
        write(SURF_DIST8x16, mbX * DIST_SIZE * 2, mbY, dist8x16);

        // motion vectors
        // 16x8
        matrix<uint, 2, 1> mv16x8 = SLICE(fbrOut16x8.format<uint>(), 8, 2, 16);
        write(SURF_MV16x8,  mbX * MVDATA_SIZE, mbY * 2, mv16x8);
        // 8x16
        matrix<uint, 1, 2> mv8x16 = SLICE(fbrOut8x16.format<uint>(), 8, 2, 8);
        write(SURF_MV8x16,  mbX * MVDATA_SIZE * 2, mbY, mv8x16);

        //matrix<uchar, 7, 32> fbrOut8x4;
        //subMbShape = 1 + 4 + 16 + 64;
        //VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
        //VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
        //VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);
        //run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut8x4);

        //matrix<uchar, 7, 32> fbrOut4x8;
        //subMbShape = (1 + 4 + 16 + 64) << 1;
        //VME_SET_UNIInput_FBRMbModeInput(uniIn, 3);
        //VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, subMbShape);
        //VME_SET_UNIInput_FBRSubPredModeInput(uniIn, subMbPredMode);
        //run_vme_fbr(uniIn, fbrIn, SURF_SRC_AND_REF, 3, subMbShape, subMbPredMode, fbrOut4x8);

        //// 8x4
        //matrix<uint, 4, 2> dist8x4 = SLICE(fbrOut8x4.row(5).format<ushort>(), 0, 8, 2);
        //write(SURF_DIST8x4, mbX * DIST_SIZE * 2, mbY * 4, dist8x4);
        //// 4x8
        //matrix<uint, 2, 4> dist4x8;
        //dist4x8.row(0) = SLICE(fbrOut4x8.row(5).format<ushort>(), 0, 4, 4);
        //dist4x8.row(1) = SLICE(fbrOut4x8.row(5).format<ushort>(), 1, 4, 4);
        //write(SURF_DIST4x8, mbX * DIST_SIZE * 4, mbY * 2, dist4x8);
        //// 8x4
        //matrix<uint, 4, 2> mv8x4 = SLICE(fbrOut8x4.format<uint>(), 8, 8, 4);
        //write(SURF_MV8x4,   mbX * MVDATA_SIZE * 2, mbY * 4, mv8x4);
        //// 4x8
        //matrix<uint, 2, 4> mv4x8;
        //SLICE(mv4x8.format<uint>(), 0, 4, 2) = SLICE(fbrOut4x8.format<uint>(),  8, 4, 8);
        //SLICE(mv4x8.format<uint>(), 1, 4, 2) = SLICE(fbrOut4x8.format<uint>(), 10, 4, 8);
        //write(SURF_MV4x8, mbX * MVDATA_SIZE * 4, mbY * 2, mv4x8);
    }
}
