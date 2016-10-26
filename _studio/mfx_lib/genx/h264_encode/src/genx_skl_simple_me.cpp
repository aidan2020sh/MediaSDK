//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#if defined(_WIN32) || defined(_WIN64)

#include <cm.h>
#include <genx_vme.h>

#if defined(CMRT_EMU)
//#include <cm_trace_log.h>
#endif // defined(CMRT_EMU)

#define MBDATA_SIZE     64      //sizeof(SVCPAKObject_t)
#define CURBEDATA_SIZE  160     //sizeof(SVCEncCURBEData_t)

//#define USE_DOWN_SAMPLE_KERNELS

enum
{
    LIST_0 = 0,
    LIST_1 = 1
};

enum
{
    FRW = 0,
    BWD = 1
};

enum
{
    INTRA_MB_FLAG = 0x20
};

#define VME_COPY_DWORD(dst, r, c, src, srcIdx)  \
    (dst.row(r).format<uint4>().select<1, 1>(c) = src.format<uint4>().select<1, 1>(srcIdx))

#define VME_SET_DWORD(dst, r, c, src)           \
    (dst.row(r).format<uint4>().select<1, 1>(c) = src.format<uint4>().select<1, 1>(0))

#define VME_SET_NEIGHBOUR_AVAILABILITY(value)                   \
{                                                               \
    vector<ushort, 1> mask;                                     \
    vector<uint1, 1> leftMask = 0x7c;                           \
    vector<uint1, 1> upperLeftMask = 0x7c;                      \
    vector<uint1, 1> upperMask = 0x7c;                          \
    vector<uint1, 1> upperRightMask = 0x7c;                     \
    vector<uint1, 1> res;                                       \
    /* select left blocks mask */                               \
    mask = leftBlockNonIntra[0];                                \
    leftMask.merge(0x1c, mask);                                 \
    /* select upper-left blocks mask */                         \
    mask = upperLeftBlockNonIntra[0];                           \
    upperLeftMask.merge(0x74, mask);                            \
    /* select upper blocks mask */                              \
    mask = upperBlockNonIntra[0];                               \
    upperMask.merge(0x6c, mask);                                \
    /* select upper-right blocks mask */                        \
    mask = upperRightBlockNonIntra[0];                          \
    upperRightMask.merge(0x78, mask);                           \
    /* create the mask and save */                              \
    res = leftMask & upperMask & upperRightMask & upperLeftMask;\
    value = res[0];                                             \
}

#define VME_SET_NEIGHBOUR_AVAILABILITY_INTER(uni, neighborMbMask)       \
{                                                                       \
    (uni.row(1).format<uint1>().select<1, 1>(29) = neighborMbMask[0]);  \
}

#define GET_MBDATA_GET_INTRA_MODE(obj) \
    ((obj[0] >> 4) & 3)

#define GET_MBDATA_IntraMbFlag(obj) \
    ((obj[1] >> 5) & 1)

#define GET_MBDATA_MV(mv, mbData, listId)       \
    mv = (mbData.format<uint4>()[11 + listId]);

#define GET_MBDATA_GET_REF_INDEX(mbData, listId) \
    ((int1) (mbData.format<uint4>()[2 + listId]))

#define SET_MBDATA_MV(mbData, listId, mv)       \
    (mbData.format<uint4>()[11 + listId]) = mv;

#define VME_CLEAR_UNIInput_IntraCornerSwap(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x7f)

#define VME_SET_CornerNeighborPixel0(p, v) \
    (VME_Input_S1(p, uint1, 1, 7) = v)

#define VME_SET_CornerNeighborPixel1(p, v) \
    (VME_Input_S1(p, uint1, 2, 15) = v)

#define VME_SET_CornerNeighborPixelChroma(p, v) \
    (VME_Input_S1(p, uint2, 2, 10) = v)

#define VME_SET_LeftMbIntraModes(sic, leftMbIntraModes)                         \
{                                                                               \
    sic.row(2).format<uint1>().select<1, 1>(16) = (leftMbIntraModes[2] >> 4) |  \
                                                  (leftMbIntraModes[3] & 0xf0); \
    sic.row(2).format<uint1>().select<1, 1>(17) = (leftMbIntraModes[6] >> 4) |  \
                                                  (leftMbIntraModes[7] & 0xf0); \
}

#define VME_SET_UpperMbIntraModes(sic, upperMbIntraModes)                           \
{                                                                                   \
    sic.row(2).format<uint1>().select<2, 1>(18) = upperMbIntraModes.select<2, 2>(5);\
}

#define VME_INIT_SVC_PACK(mbData) \
    {mbData = 0;}

#define VME_COPY_MB_INTRA_MODE_TYPE(vector, startIdx, srcMatrix)                                            \
    vector[startIdx] = VME_Input_G1(srcMatrix, uint1, 0, 0) & 0x0f8 /* clear InterMbMode & SkipMbFlag */;   \
    vector[startIdx + 1] = (VME_Input_G1(srcMatrix, uint1, 0, 1) & 0x0c0) |                                 \
                           INTRA_MB_FLAG |                                                                  \
                           (VME_Input_G1(srcMatrix, uint1, 0, 2) & 0x01f);                                  \
    /* CodedPatternDC */                                                                                    \
    vector[startIdx + 2] = 0x0e;                                                                            \
    /* num MVs and ExtendedForm, ExtendedForm is ignored. HW always set to 0 and default using Extended form*/ \
    vector[startIdx + 3] = (VME_Input_G1(srcMatrix, uint1, 0, 3) & 0x1f) | 0x80; /* MV Quantity*/
    /*ExtendedForm = (VME_Input_G1(srcMatrix, uint1, 0, 2) & 0x80) |\ // ExtendedForm  */

#define VME_GET_UNIInput_SkipModeEn(p) (VME_Output_S1(p, uint1, 1, 0) & 0x1)

#define VME_GET_UNIInput_FTEnable(p) (VME_Output_S1(p, uint1, 0, 14) & 0x2)

#define VME_SET_UNIInput_BlkSkipEnable(p) (VME_Input_S1(p, uint1, 0, 14) |= 0x08)

#define VME_CLEAR_UNIInput_NonSkipModeAdded(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x0bf)

#define VME_CLEAR_UNIInput_NonSkipMvAdded(p) (VME_Input_S1(p, uint1, 1, 28) &= 0x0df)

#define GET_CURBE_T8x8FlagForInterEn(obj) \
    ((obj[0] >> 7) & 1)

#define GET_CURBE_SubPelMode(obj) \
    ((obj[13] >> 4) & 3)

#define GET_CURBE_FTQSkipEnable(obj) \
    ((obj[14] >> 1) & 1)

#define GET_CURBE_LenSP(obj) \
    (obj[8])

#define GET_CURBE_SliceMacroblockHeight(obj) \
    (obj[16] + 1)

#define GET_CURBE_PictureHeight(obj) \
    (obj[17] + 1)

#define GET_CURBE_PictureWidth(obj) \
    (obj[18])

#define SET_CURBE_PictureHeight(obj, h) \
    (obj[17] = h - 1)

#define SET_CURBE_PictureWidth(obj, w) \
    (obj[18] = w)

#define GET_CURBE_QpPrimeY(obj) \
    (obj[52])

#define GET_CURBE_FTXCoeffThresh(obj) \
    ((obj).format<uint4> ().select<2, 1> (14))

#define GET_CURBE_SkipVal(obj) \
    (obj.format<uint2> ()[64])

#define GET_CURBE_AllFractional(obj) \
    ((obj[145] >> 2) & 1)

#define GET_CURBE_isFwdFrameShortTermRef(obj) \
    ((obj[150] >> 6) & 1)

#define GET_CURBE_CurLayerDQId(obj) \
    (obj[148])

enum
{
    INTRA_TYPES_OFFSET = 4
};

enum
{
    PREDSLICE = 0,
    BPREDSLICE = 1,
    INTRASLICE = 2
};

_GENX_ inline
void SetUpVmeIntra(matrix_ref<uchar, 4, 32> uniIn,
                   matrix_ref<uchar, 4, 32> sicIn,
                   vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                   SurfaceIndex SrcSurfIndex,
                   SurfaceIndex MBDataSurfIndex,
                   uint mbX, uint mbY)
{
    vector<uchar, 8> leftMbIntraModes = 0x22, upperMbIntraModes = 0x22;
    matrix<uint1, 16, 1> leftBlockValues = 0;
    matrix<uint1, 1, 28> topBlocksValues = 0;
    matrix<uint1, 8, 2> chromaLeftBlockValues = 0;
    matrix<uint1, 1, 20> chromaTopBlocksValues = 0;
    vector<ushort, 1> leftBlockNonIntra = 1, upperBlockNonIntra = 1,
                      upperRightBlockNonIntra = 1, upperLeftBlockNonIntra = 1;
    uint x = mbX * 16;
    uint y = mbY * 16;
    int offset = 0;
    const ushort topPresent = ushort(mbY % GET_CURBE_SliceMacroblockHeight(CURBEData));

    // read picture's width in MB units
    uint PicWidthInMB    = GET_CURBE_PictureWidth(CURBEData);
    //uint PicHeightInMBs  = GET_CURBE_PictureHeight(CURBEData);

    uint MbIndex = PicWidthInMB * mbY + mbX;

    cm_wait();

    // read left macroblock info
    if (mbX)
    {
        // read left macroblock intra types
        {
            vector<uchar, MBDATA_SIZE> leftMbInfo;
            vector<ushort, 8> mask;

            // read left macroblock parameters
            offset = (MbIndex - 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, leftMbInfo);

            // copy left macroblock intra types
            leftMbIntraModes.select<4, 1> (0) = leftMbInfo.select<4, 1> (4);
            leftMbIntraModes.select<4, 1> (4) = leftMbInfo.select<4, 1> (8);
            mask = (0 < (leftMbInfo[1] & 0x1f));
            leftMbIntraModes.merge(0x22, mask);
            leftBlockNonIntra = 0;
        }

        // read luminance samples
        {
            matrix<uint1, 16, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y, temp);
            leftBlockValues = temp.select<16, 1, 1, 1> (0, 3);
        }

        // read chrominance samples
        {
            matrix<uint1, 8, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2, temp);
            chromaLeftBlockValues = temp.select<8, 1, 2, 1> (0, 2);
        }
    }
    // read upper macroblock info
    if (topPresent)
    {
        // read upper macroblock intra types
        {
            vector<uchar, MBDATA_SIZE> upperMbInfo;
            vector<ushort, 8> mask;

            // read upper macroblock parameters
            offset = (MbIndex - PicWidthInMB) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperMbInfo);

            // copy upper macroblock intra types
            upperMbIntraModes = upperMbInfo.select<8, 1> (4);
            mask = (0 < (upperMbInfo[1] & 0x1f));
            upperMbIntraModes.merge(0x22, mask);
            upperBlockNonIntra = 0;
        }

        // read neighbor samples
        if (mbX)
        {
            upperLeftBlockNonIntra = 0;

            // read a macroblock  neighbors somewhere in the middle
            if (mbX + 1 < PicWidthInMB)
            {
                upperRightBlockNonIntra = 0;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, topBlocksValues);
            }
            // read a right-most macroblock neighbors
            else
            {
                matrix<uint1, 1, 20> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, temp);
                topBlocksValues.select<1, 1, 20, 1> (0, 0) = temp;
                topBlocksValues.select<1, 1, 8, 1> (0, 20) = temp.row(0)[19];
            }

            // read chrominance samples
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2 - 1, chromaTopBlocksValues);
        }
        // read a left-most macroblock neighbor
        else
        {
            upperRightBlockNonIntra = 0;

            // read luminance samples
            {
                matrix<uint1, 1, 24> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
                topBlocksValues.select<1, 1, 24, 1> (0, 4) = temp;
            }

            // read chrominance samples
            {
                matrix<uint1, 1, 16> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x, y / 2 - 1, temp);
                chromaTopBlocksValues.select<1, 1, 16, 1> (0, 4) = temp;
            }
        }

        {
            vector<ushort, 20> mask = 0;

            mask.select<4, 1> (0) = upperLeftBlockNonIntra[0];
            topBlocksValues.select<1, 1, 20, 1> (0, 0).merge(0, mask);
            chromaTopBlocksValues.merge(0, mask);

            // upper-right pixels can be used only when B macroblock is available.
            mask = upperRightBlockNonIntra[0];
            topBlocksValues.select<1, 1, 8, 1> (0, 20).merge(topBlocksValues.row(0)[19], mask.select<8, 1> (0));
        }
    }

    INIT_VME_UNIINPUT(uniIn);
    INIT_VME_SICINPUT(sicIn);

    // copy dwords from CURBE data into universal VME header
    // register M0
    // M0.0 Reference 0 Delta
    // M0.1 Reference 1 Delta
    // M0.2 Source X/Y
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);
    // M0.3
    VME_COPY_DWORD(uniIn, 0, 3, CURBEData, 3);
    // M0.4 reserved
    // M0.5
    VME_COPY_DWORD(uniIn, 0, 5, CURBEData, 5);
    // M0.6 debug
    // M0.7 debug

    // register M1
    // M1.0/1.1/1.2
    uniIn.row(1).format<uint>().select<3, 1> (0) = CURBEData.format<uint>().select<3, 1> (0);
    VME_CLEAR_UNIInput_SkipModeEn(uniIn);
    // M1.3 Weighted SAD
    // M1.4 Cost center 0 for HSW; MBZ for BDW
    // M1.5 Cost center 1 for HSW; MBZ for BDW
    // M1.6 Fwd/Bwd Block RefID
    // M1.7 various prediction parameters
    VME_COPY_DWORD(uniIn, 1, 7, CURBEData, 7);
    VME_CLEAR_UNIInput_IntraCornerSwap(uniIn);
    VME_SET_NEIGHBOUR_AVAILABILITY((uniIn.row(1).format<uint1>().select<1, 1> (29)));

    // register M2
    // M2.0/2.1/2.2/2.3/2.4
    uniIn.row(2).format<uint>().select<5, 1> (0) = CURBEData.format<uint>().select<5, 1> (8);
    // M2.5 FBR parameters
    // M2.6 SIC Forward Transform Coeff Threshold Matrix
    // M2.7 SIC Forward Transform Coeff Threshold Matrix

    // BDW+
    // M3.0 FWD Cost center 0 Delta X and Y
    // M3.1 BWD Cost center 0 Delta X and Y
    // M3.2 FWD Cost center 1 Delta X and Y
    // M3.3 BWD Cost center 1 Delta X and Y
    // M3.4 FWD Cost center 2 Delta X and Y
    // M3.5 BWD Cost center 2 Delta X and Y
    // M3.6 FWD Cost center 3 Delta X and Y
    // M3.7 BWD Cost center 3 Delta X and Y

    //
    // set up the SIC message
    //

    // register M0
    // M0.0 Ref0 Skip Center 0
    // M0.1 Ref1 Skip Center 0
    // M0.2 Ref0 Skip Center 1
    // M0.3 Ref1 Skip Center 1
    // M0.4 Ref0 Skip Center 2
    // M0.5 Ref1 Skip Center 2
    // M0.6 Ref0 Skip Center 3
    // M0.3 Ref1 Skip Center 3

    // register M1
    // M1.0 ACV Intra 4x4/8x8 mode mask
    // M1.1 AVC Intra Modes
    sicIn.row(1).format<uint>().select<2, 1> (0) = CURBEData.format<uint>().select<2, 1> (30);
    VME_SET_CornerNeighborPixel0(sicIn, topBlocksValues(0, 3));

    // M1.2-1.7 Neighbor pixel Luma values
    sicIn.row(1).format<uint>().select<6, 1> (2) = topBlocksValues.format<uint>().select<6, 1> (1);

    // register M2
    // M2.0-2.3 Neighbor pixel luma values
    sicIn.row(2).format<uint>().select<4, 1> (0) = leftBlockValues.format<uint>().select<4, 1> (0);
    // M2.4 Intra Predictor Mode
    VME_SET_LeftMbIntraModes(sicIn, leftMbIntraModes);
    VME_SET_UpperMbIntraModes(sicIn, upperMbIntraModes);
    // M2.5 Corner pixel chroma value
    VME_SET_CornerNeighborPixelChroma(sicIn, (chromaTopBlocksValues.format<uint2>().select<1, 1>(1)));
    // M2.6 reserved
    // M2.7 Penalties for non-DC modes
    VME_COPY_DWORD(sicIn, 2, 7, CURBEData, 33);

    // register M3
    // M3.0-3.3 Neighbor pixel chroma values
    sicIn.row(3).format<uint>().select<4, 1> (0) = chromaLeftBlockValues.format<uint>().select<4, 1> (0);
    // M3.4-3.7 Neighbor pixel chroma values
    sicIn.row(3).format<uint>().select<4, 1> (4) = chromaTopBlocksValues.format<uint>().select<4, 1> (1);

} // void SetUpVmeIntra(matrix_ref<uchar, 3, 32> uniIn,

_GENX_ inline
void SetUpPakDataISlice(vector_ref<uchar, MBDATA_SIZE> MBData,
/*
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
*/
                        matrix_ref<uchar, 7, 32> uniOut,
                        uint mbX,
                        uint mbY)
{
    uint PicWidthInMB, PicHeightInMBs;       //picture width/height in MB unit

    MBData = 0;

/*
    // read picture's width in MB units
    PicWidthInMB    = GET_CURBE_PictureWidth(CURBEData);
    PicHeightInMBs  = GET_CURBE_PictureHeight(CURBEData);
*/

    // DW0 MB flags and modes
    VME_COPY_MB_INTRA_MODE_TYPE(MBData, 0, uniOut);

    // DW1 & DW2 intra luma modes
    MBData.format<uint>().select<2, 1> (1) = uniOut.row(0).format<uint>().select<2, 1> (4);

    // DW3 intra macroblock struct (ChromaIntraPredMode & IntraPredAvailFlags)
    MBData.format<uint4>()[3] = VME_Output_S1(uniOut, uint4, 0, 6) & 0x0ff;

    // DW4 intra cost
    uint2 distIntra;
    VME_GET_UNIOutput_BestIntraDistortion(uniOut, distIntra);
    MBData.format<uint2>()[8] = distIntra;
    MBData.format<uint2>()[9] = distIntra;

} // void SetUpPakDataISlice(vector_ref<uchar, MBDATA_SIZE> MBData,

extern "C" _GENX_MAIN_  void
DownSampleMB2X(SurfaceIndex SurfIndex,
               SurfaceIndex Surf2XIndex)
{
    // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 5; // src 32x32
    uint iy = mbY << 5;
    uint ox2x = mbX << 4; // dst 16x16
    uint oy2x = mbY << 4;

    matrix<uint1,32,32> inMb;
    matrix<uint1,16,16> outMb2x;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 16, inMb.select<16,1,16,1>(16,16));

    matrix<uint2,16,32> sum16x32_16;

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb2x);
}

extern "C" _GENX_MAIN_  void
DownSampleMB4X(SurfaceIndex SurfIndex,
               SurfaceIndex Surf4XIndex)
{   // Luma only
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint ix = mbX << 6; // src 64x64
    uint iy = mbY << 6;
    uint ox2x = mbX << 4; // dst 16x16
    uint oy2x = mbY << 4;

    matrix<uint1,32,32> inMb;
    matrix<uint1,32,32> outMb2x;
    matrix<uint1,16,16> outMb4x;
    matrix<uint2,16,32> sum16x32_16;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy +  0, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy +  0, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 16, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy +  0, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy +  0, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 16, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(0,16) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 32, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 32, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 48, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 48, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(16,0) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 32, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 32, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 48, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 48, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(16,16) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    sum16x32_16.select<16,1,32,1>(0,0) = outMb2x.select<16,2,32,1>(0,0) + outMb2x.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb4x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    write_plane(Surf4XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb4x);
}

_GENX_ inline
void DownSampleMB2Xf(SurfaceIndex SurfIndex,
                     SurfaceIndex Surf2XIndex,
                     uint mbX, uint mbY)
{   // Luma only
    uint ix = mbX << 5; // src 32x32
    uint iy = mbY << 5;
    uint ox2x = mbX << 4; // dst 16x16
    uint oy2x = mbY << 4;

    matrix<uint1,32,32> inMb;
    matrix<uint1,16,16> outMb2x;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 16, inMb.select<16,1,16,1>(16,16));

    matrix<uint2,16,32> sum16x32_16;

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    write_plane(Surf2XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb2x);
}

_GENX_ inline
void DownSampleMB4Xf(SurfaceIndex SurfIndex,
                     SurfaceIndex Surf4XIndex,
                     uint mbX, uint mbY)
{   // Luma only
    uint ix = mbX << 6; // src 64x64
    uint iy = mbY << 6;
    uint ox2x = mbX << 4; // dst 16x16
    uint oy2x = mbY << 4;

    matrix<uint1,32,32> inMb;
    matrix<uint1,32,32> outMb2x;
    matrix<uint1,16,16> outMb4x;
    matrix<uint2,16,32> sum16x32_16;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy +  0, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy +  0, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 16, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy +  0, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy +  0, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 16, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 16, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(0,16) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 32, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 32, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix +  0, iy + 48, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 16, iy + 48, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(16,0) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 32, inMb.select<16,1,16,1>(0,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 32, inMb.select<16,1,16,1>(0,16));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 32, iy + 48, inMb.select<16,1,16,1>(16,0));
    read_plane(SurfIndex, GENX_SURFACE_Y_PLANE, ix + 48, iy + 48, inMb.select<16,1,16,1>(16,16));

    sum16x32_16.select<16,1,32,1>(0,0) = inMb.select<16,2,32,1>(0,0) + inMb.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb2x.select<16,1,16,1>(16,16) = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    sum16x32_16.select<16,1,32,1>(0,0) = outMb2x.select<16,2,32,1>(0,0) + outMb2x.select<16,2,32,1>(1,0);
    sum16x32_16.select<16,1,16,1>(0,0) = sum16x32_16.select<16,1,16,2>(0,0) + sum16x32_16.select<16,1,16,2>(0,1);

    sum16x32_16.select<16,1,16,1>(0,0) += 2;
    outMb4x = sum16x32_16.select<16,1,16,1>(0,0) >> 2;

    write_plane(Surf4XIndex, GENX_SURFACE_Y_PLANE, ox2x, oy2x, outMb4x);
}

extern "C" _GENX_MAIN_  void
SVCEncMB_I(SurfaceIndex CurbeDataSurfIndex,
           SurfaceIndex SrcSurfIndexRaw,
           SurfaceIndex SrcSurfIndex,
           SurfaceIndex VMEInterPredictionSurfIndex,
           SurfaceIndex MBDataSurfIndex,
           SurfaceIndex /*FwdFrmMBDataSurfIndex*/)
{
//    cm_wait();
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    vector<uchar, CURBEDATA_SIZE> CURBEData;
    read(CurbeDataSurfIndex,0,CURBEData.select<128,1>());
    read(CurbeDataSurfIndex,128,CURBEData.select<32,1>(128));

    // read picture's width in MB units
    uint PicWidthInMB = GET_CURBE_PictureWidth(CURBEData);
    uint MbIndex = PicWidthInMB * mbY + mbX;
    
    int offset;

#ifndef USE_DOWN_SAMPLE_KERNELS
    // down scale
    uint LaScaleFactor = GET_CURBE_CurLayerDQId(CURBEData);
    if (LaScaleFactor == 2)
    {
        DownSampleMB2Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
    } else if (LaScaleFactor == 4)
    {
        DownSampleMB4Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
    }
#endif

    // declare parameters for VME
    matrix<uchar, 4, 32> uniIn;
    matrix<uchar, 4, 32> sicIn;
    matrix<uchar, 7, 32> best_uniOut;

    SetUpVmeIntra(uniIn, sicIn, CURBEData, SrcSurfIndex, MBDataSurfIndex, mbX, mbY);

    // call intra/skip motion estimation
    run_vme_sic(uniIn, sicIn, VMEInterPredictionSurfIndex, best_uniOut);

    //
    // write the result
    //

    vector<uchar, MBDATA_SIZE> MBData;

    // pack the VME result
    SetUpPakDataISlice(MBData/*, CURBEData*/, best_uniOut, mbX, mbY);

    // write back updated MB data
    offset = MbIndex * MBDATA_SIZE;
    write(MBDataSurfIndex, offset, MBData.select<MBDATA_SIZE, 1>(0));

    cm_fence();
}

_GENX_ inline
void GetMedianVector(vector_ref<int2, 2> mvMedian,
                     vector<int2, 2> mvA,
                     vector<int2, 2> mvB,
                     vector<int2, 2> mvC)
{
    vector<int2, 2> tmpMvA;
    vector<ushort, 2> maskA, maskB, maskC;

    tmpMvA = mvA;
    maskA = (mvA > mvB);
    maskB = (mvB > mvC);
    maskC = (mvC > mvA);
    mvA.merge(mvB, maskA);
    mvB.merge(mvC, maskB);
    mvC.merge(tmpMvA, maskC);

    mvMedian = mvA ^ mvB ^ mvC;

} // void GetMedianVector(vector_ref<int2, 2> mvMedian,

_GENX_ inline
void GetNeighbourParamP(matrix_ref<uint1, 16, 1> leftBlockValues,
                        matrix_ref<uint1, 1, 28> topBlocksValues,
                        matrix_ref<uint1, 8, 2> chromaLeftBlockValues,
                        matrix_ref<uint1, 1, 20> chromaTopBlocksValues,
                        vector_ref<uchar, 8> leftMbIntraModes,
                        vector_ref<uchar, 8> upperMbIntraModes,
                        vector_ref<uchar, 1> neighborMbMask,
                        vector_ref<int2, 2> mvPred,
                        vector_ref<int2, 2> skipMvPred,
                        SurfaceIndex SrcSurfIndex,
                        SurfaceIndex MBDataSurfIndex,
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                        uint x, uint y, uint mbIndex, uint PicWidthInMB)
{
    vector<int2, 2> mvA, mvB, mvC, tmp, equalMV;
    vector<ushort, 2> equalMask, equalNum = 0;
    vector<uint4, 1> refIdxA, refIdxB, refIdxC, refIdxD;
    // neighbors mask
    vector<uint1, 1> leftMask = 0;
    vector<uint1, 1> upperMask = 0;
    vector<uint1, 1> upperRightMask = 0;
    vector<uint1, 1> upperLeftMask = 0;
    const uint mbX = x / 16;
    const uint mbY = y / 16;
    const uint1 topPresent = mbY % GET_CURBE_SliceMacroblockHeight(CURBEData);

    // reset output(s)
    leftBlockValues = 0;
    topBlocksValues = 0;
    chromaLeftBlockValues = 0;
    chromaTopBlocksValues = 0;
    leftMbIntraModes = 0x2222;
    upperMbIntraModes = 0x2222;
    neighborMbMask = 0;
    mvPred = 0;
    skipMvPred = 0;

    // there are no neighbors
    if (0 == (x | topPresent))
    {
        return;
    }

    cm_wait();
    // read left macroblock parameters
    refIdxA = -1;
    mvA = 0;
    if (mbX)
    {
        vector<uchar, MBDATA_SIZE> leftMbData;
        vector<short, 8> intraMbFlagMask;
        uint1 intraMbFlag, intraMbMode;
        int offset;

        // read intra types
        offset = (mbIndex - 1) * MBDATA_SIZE;
        read(DWALIGNED(MBDataSurfIndex), offset, leftMbData);
        intraMbMode = GET_MBDATA_GET_INTRA_MODE(leftMbData);
        intraMbFlag = GET_MBDATA_IntraMbFlag(leftMbData);
        intraMbFlagMask = (intraMbFlag & (0 < intraMbMode));
        leftMbIntraModes.merge(leftMbData.select<8, 1> (INTRA_TYPES_OFFSET), intraMbFlagMask);
        leftMask = 0x60;

        // read motion vector
        GET_MBDATA_MV(tmp.format<uint4>()[0], leftMbData, LIST_0);

        // read reference index
        refIdxA = GET_MBDATA_GET_REF_INDEX(leftMbData, 0);

        // check if the macroblock has forward motion vector.
        // In P slice we need to only check for not-INTRA type.
        mvA.merge(tmp, (intraMbFlag - 1));
        refIdxA.merge(uint(-1), intraMbFlag);

        // get the equal motion vector, if reference is zero and macroblock is
        // not-INTRA
        equalMask = (0 == refIdxA[0]);
        equalMV.merge(tmp, equalMask);
        equalNum += equalMask;
    }

    // read upper macroblock parameters
    refIdxB = -1;
    mvB = 0;
    refIdxC = -1;
    mvC = 0;

    if (topPresent)
    {
        vector<uchar, MBDATA_SIZE> upperMbData;
        vector<short, 8> intraMbFlagMask;
        uint1 intraMbFlag, intraMbMode, intraMbFlagD = 0;
        int offset;

        // read intra types
        offset = (mbIndex - PicWidthInMB) * MBDATA_SIZE;
        read(DWALIGNED(MBDataSurfIndex), offset, upperMbData);
        intraMbMode = GET_MBDATA_GET_INTRA_MODE(upperMbData);
        intraMbFlag = GET_MBDATA_IntraMbFlag(upperMbData);
        intraMbFlagMask = (intraMbFlag & (0 < intraMbMode));
        upperMbIntraModes.merge(upperMbData.select<8, 1> (INTRA_TYPES_OFFSET), intraMbFlagMask);
        upperMask = 0x10;

        // read motion vector
        GET_MBDATA_MV(tmp.format<uint4>()[0], upperMbData, LIST_0);

        // read reference index
        refIdxB = GET_MBDATA_GET_REF_INDEX(upperMbData, 0);

        // check if the macroblock has forward motion vector.
        // In P slice we need to only check for not-INTRA type.
        mvB.merge(tmp, (intraMbFlag - 1));
        refIdxB.merge(uint(-1), intraMbFlag);

        // get the equal motion vector, if reference is zero and macroblock is
        // not-INTRA
        equalMask = (0 == refIdxB[0]);
        equalMV.merge(tmp, equalMask);
        equalNum += equalMask;

        // read macroblock type for upper left neighbor
        if (mbX)
        {
            vector<uchar, MBDATA_SIZE> upperLeftMbData;
            // read intra types
            offset = (mbIndex - PicWidthInMB - 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperLeftMbData);
            intraMbFlagD = GET_MBDATA_IntraMbFlag(upperLeftMbData);
            upperLeftMask = 0x08;

            // read reference index
            refIdxD = GET_MBDATA_GET_REF_INDEX(upperLeftMbData, 0);
        }

        // read third macroblock
        if (mbX + 1 < PicWidthInMB)
        {
            vector<uchar, MBDATA_SIZE> upperRightMbData;
            // read intra types
            offset = (mbIndex - PicWidthInMB + 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperRightMbData);
            intraMbFlag = GET_MBDATA_IntraMbFlag(upperRightMbData);
            upperRightMask = 0x04;

            // read motion vector
            GET_MBDATA_MV(tmp.format<uint4>()[0], upperRightMbData, LIST_0);

            // read reference index
            refIdxC = GET_MBDATA_GET_REF_INDEX(upperRightMbData, 0);

            // check if the macroblock has forward motion vector.
            // In P slice we need to only check for not-INTRA type.
            mvC.merge(tmp, (intraMbFlag - 1));
            refIdxC.merge(uint(-1), intraMbFlag);

            // get the equal motion vector, if reference is zero and macroblock is
            // not-INTRA
            equalMask = (0 == refIdxC[0]);
            equalMV.merge(tmp, equalMask);
            equalNum += equalMask;
        }
        // read fourth macroblock
        else if (mbX)
        {
            vector<uchar, MBDATA_SIZE> upperLeftMbData;

            // read motion vector
            offset = (mbIndex - PicWidthInMB - 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperLeftMbData);

            GET_MBDATA_MV(tmp.format<uint4>()[0], upperLeftMbData, LIST_0);

            // check if the macroblock has forward motion vector.
            // In P slice we need to only check for not-INTRA type.
            mvC.merge(tmp, (intraMbFlagD - 1));
            refIdxC.merge(uint(-1), refIdxD, intraMbFlagD);

            // get the equal motion vector, if reference is zero and macroblock is
            // not-INTRA
            equalMask = (0 == refIdxC[0]);
            equalMV.merge(tmp, equalMask);
            equalNum += equalMask;
        }
    }

    // make neighbor's mask for intra prediction
    neighborMbMask = leftMask | upperMask | upperLeftMask | upperRightMask;

    //
    // read planes
    //
    if (leftMask[0])
    {
        // read luminance samples
        {
            matrix<uint1, 16, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y, temp);
            leftBlockValues = temp.select<16, 1, 1, 1> (0, 3);
        }

        // read chrominance samples
        {
            matrix<uint1, 8, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2, temp);
            chromaLeftBlockValues = temp.select<8, 1, 2, 1> (0, 2);
        }
    }

    if (upperMask[0])
    {
        if (upperLeftMask[0])
        {
            // read luminance samples
            if (upperRightMask[0])
            {
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, topBlocksValues);
            }
            else
            {
                matrix<uint1, 1, 20> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, temp);
                topBlocksValues.select<1, 1, 20, 1> (0, 0) = temp;
                topBlocksValues.select<1, 1, 8, 1> (0, 20) = temp.row(0)[19];
            }

            // read chrominance samples
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2 - 1, chromaTopBlocksValues);
        }
        else
        {
            // read luminance samples
            if (upperRightMask[0])
            {
                matrix<uint1, 1, 24> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
                topBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                topBlocksValues.select<1, 1, 24, 1> (0, 4) = temp;
            }
            else
            {
                matrix<uint1, 1, 16> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
                topBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                topBlocksValues.select<1, 1, 16, 1> (0, 4) = temp;
                topBlocksValues.select<1, 1, 8, 1> (0, 20) = temp.row(0)[15];
            }

            // read chrominance samples
            {
                matrix<uint1, 1, 16> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x, y / 2 - 1, temp);
                chromaTopBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                chromaTopBlocksValues.select<1, 1, 16, 1> (0, 4) = temp;
            }
        }
    }

    //
    // calculate motion vector predictor
    //

    // select median motion vector
    {
        vector<ushort, 2> maskA, maskB, maskC;

        GetMedianVector(mvPred, mvA, mvB, mvC);

        // there is a condition in the standard, that if only one neighbor has
        // the same refIdx like the current block, the vector from this neighbor
        // block must be used for prediction.
        equalMask = (1 == equalNum);
        mvPred.merge(equalMV, equalMask);

        skipMvPred = mvPred;
        // skip vector prediction is not zero if and only if A and B macroblocks
        // are exists and both of them have non-zero reference indices and vectors.
        maskA = ((0 == mbX) || (0 == topPresent));
        skipMvPred.merge(0, maskA);
        maskA = (0 == (refIdxA[0] | mvA[0] | mvA[1]));
        skipMvPred.merge(0, maskA);
        maskB = (0 == (refIdxB[0] | mvB[0] | mvB[1]));
        skipMvPred.merge(0, maskB);
    }

} // void GetNeighbourParamP(vector_ref<uchar, 8> leftMbIntraModes,

#define VME_COPY_MB_INTER_MODE_TYPE(vector, startIdx, uniOut)           \
    /* InterMbMode, SkipMbFlag, IntraMbMode, FieldMbPolarityFlag */     \
    vector[startIdx] = VME_Input_G1(uniOut, uint1, 0, 0) & 0x0cf;       \
    /* MbType5Bits, IntraMbFlag, FieldMbFlag, Transform8x8Flag */       \
    vector[startIdx + 1] = VME_Input_G1(uniOut, uint1, 0, 1);           \
    {                                                                   \
        U8 mbSubShape;                                                  \
        uint1 value;                                                    \
        VME_GET_UNIOutput_SubMbShape(uniOut, mbSubShape);               \
        value = vector[startIdx + 1] | (CURBEData[0] & 0x080);          \
        transformMask = (0 == mbSubShape);                              \
        vector.select<1, 1> (startIdx + 1).merge(value, transformMask); \
    }                                                                   \
    /* CodedPatternDC */                                                \
    vector[startIdx + 2] = 0x0e;                                        \
    /* num MVs and ExtendedForm, ExtendedForm is ignored. HW always set to 0 and default using Extended form*/  \
    vector[startIdx + 3] = (VME_Input_G1(uniOut, uint1, 0, 3) & 0x1f) | 0x80; /* MV Quantity*/
    /*ExtendedForm = (VME_Input_G1(srcMatrix, uint1, 0, 2) & 0x80)  |\ // ExtendedForm*/

#define _VME_GET_UNIOutput_InterDistortion(uniOut, v)   \
    (v = VME_Output_S1(uniOut, uint2, 0, 4))

#define VME_SET_UNIOutput_InterDistortion(uniOut, v)    \
    (VME_Input_S1(uniOut, uint2, 0, 4) = v)

#define VME_SET_IMEOutput_Rec0_16x16_Distortion(imeOut, v)  \
    (VME_Input_S1(imeOut, uint2, 0, 8) = v)

#define VME_SET_IMEOutput_Rec0_Distortions(imeOut, v)   \
    (VME_Input_S1(imeOut, uint4, 0, 0) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 1) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 2) = v);            \
    (VME_Input_S1(imeOut, uint4, 0, 3) = v)

#define VME_SET_IMEOutput_Rec1_16x16_Distortion(imeOut, v)  \
    (VME_Input_S1(imeOut, uint2, 2, 8) = v)

#define VME_SET_IMEOutput_Rec1_Distortions(imeOut, v)   \
    (VME_Input_S1(imeOut, uint4, 2, 0) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 1) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 2) = v);            \
    (VME_Input_S1(imeOut, uint4, 2, 3) = v)

#define VME_SET_UNIOutput_BestIntraDistortion(uniOut, v)    \
    (VME_Input_S1(uniOut, uint2, 0, 6) = v)

#define VME_SET_IMEInput_Distortions(p, v)          \
    (p.row(2).format<uint4>().select<4, 1>(0) = v); \
    (p.row(4).format<uint4>().select<4, 1>(0) = v)

#define VME_SET_UNIOutput_SkipRawDistortion(p, v) (VME_Input_S1(p, uint2, 0, 5) = v)

#define VME_SET_UNIInput_SkipModeEnable(obj, value) \
    (obj.row(1)[0] = ((obj.row(1)[0] & 0x0fe) | (value & 1)))


_GENX_ inline
void SetRef(matrix_ref<uchar, 4, 32> uniIn,
            vector_ref<int2, 2> Ref,
            vector<int2, 2> Predictor,
            vector_ref<int1, 2> Search,
            vector_ref<int1, 2> PicSize,
            int2 maxMvLenY)
{
    vector<int2, 2> Width = (Search - 16) >> 1;
    vector<int2, 2> MaxMvLen;
    vector<int2, 2> RefSize;
    vector<short, 2> mask;
    vector<int2, 2> res, otherRes;

    // set up parameters
    MaxMvLen[0] = 512;
    MaxMvLen[1] = maxMvLenY / 4;
    RefSize[0] = PicSize[1] * 16;
    RefSize[1] = (PicSize[0] + 1) * 16;

    /* fields and MBAFF are not supported */

    // remove quater pixel fraction
    Predictor >>= 2;

    //
    // set the reference position
    //

    Ref = Predictor;
    Ref[1] &= -2;
    Ref -= Width;

    res = MaxMvLen - Width;
    mask = (Predictor > res);
    otherRes = MaxMvLen - (Search - 16);
    Ref.merge(otherRes, mask);

    res = -res;
    mask = (Predictor < res);
    otherRes = -MaxMvLen;
    Ref.merge(otherRes, mask);

    //
    // saturate to reference dimension
    //

    res = Ref + uniIn.row(0). format<int2>().select<2, 1> (4);
    mask = (RefSize <= res);
    otherRes = ((RefSize - 1) & ~3) - uniIn.row(0). format<int2>().select<2, 1> (4);
    Ref.merge(otherRes, mask);

    res = Ref + uniIn.row(0). format<int2>().select<2, 1> (4) + Search;
    mask = (0 >= res);
    otherRes = ((-Search + 5) & ~3) - uniIn.row(0). format<int2>().select<2, 1> (4);
    Ref.merge(otherRes, mask);

    Ref[1] &= -2;

} // void SetRef(matrix_ref<uchar, 3, 32> uniIn,

enum
{
    MbMode_16x16 = 0,
    MbMode_16x8 = 1,
    MbMode_8x16 = 2,
    MbMode_8x8 = 3
};

_GENX_ inline
void SetUpVmePSlice(matrix_ref<uchar, 4, 32> uniIn,
                    matrix_ref<uchar, 4, 32> sicIn,
                    matrix_ref<uint4, 16, 2> Costs,
                    SurfaceIndex SrcSurfIndex,
                    SurfaceIndex MBDataSurfIndex,
                    vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                    uint x, uint y, uint MbIndex,
                    vector_ref<int2, 2> mvPred)
{
    vector<uchar, 8> leftMbIntraModes, upperMbIntraModes;
    vector<uchar, 1> neighborMbMask;
    vector<int2, 2> skipMvPred;
    matrix<uint1, 16, 1> leftBlockValues;
    matrix<uint1, 1, 28> topBlocksValues;
    matrix<uint1, 8, 2> chromaLeftBlockValues;
    matrix<uint1, 1, 20> chromaTopBlocksValues;
    uint PicWidthInMB;

    // read picture's width in MB units
    PicWidthInMB = GET_CURBE_PictureWidth(CURBEData);

    GetNeighbourParamP(leftBlockValues,
                       topBlocksValues,
                       chromaLeftBlockValues,
                       chromaTopBlocksValues,
                       leftMbIntraModes,
                       upperMbIntraModes,
                       neighborMbMask,
                       mvPred,
                       skipMvPred,
                       SrcSurfIndex,
                       MBDataSurfIndex,
                       CURBEData,
                       x, y, MbIndex, PicWidthInMB);

    // copy dwords from CURBE data into universal VME header
    // register M0
    // M0.0 Reference 0 Delta
    // M0.1 Reference 1 Delta
    // M0.2 Source X/Y
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);
    // M0.3
    VME_COPY_DWORD(uniIn, 0, 3, CURBEData, 3);
    // M0.4 reserved
    // M0.5
    VME_COPY_DWORD(uniIn, 0, 5, CURBEData, 5);
    SetRef(uniIn,
           uniIn.row(0).format<int2>().select<2, 1> (0),
           mvPred,
           uniIn.row(0).format<int1>().select<2, 1> (22),
           CURBEData.format<int1>().select<2, 1> (17),
           CURBEData.format<int2>()[68]);
    // M0.6 debug
    // M0.7 debug

    // register M1
    // M1.0/1.1/1.2 Search path parameters & start centers
    uniIn.row(1).format<uint4> ().select<3, 1> (0) = CURBEData.format<uint4> ().select<3, 1> (0);
    {
        vector<uint1, 2> Start0;

        Start0 = uniIn.row(0).format<int1>().select<2, 1> (22);
        Start0 = ((Start0 - 16) >> 3) & 0x0f;
        uniIn.row(1)[10] = Start0[0] | (Start0[1] << 4);
    }
    // M1.3 Weighted SAD (not used for HSW)
    // M1.4 Cost center 0 for HSW; MBZ for BDW
    // M1.5 Cost center 1 for HSW; MBZ for BDW
    // M1.6 Fwd/Bwd Block RefID (used in B slices only)
    // M1.7 various prediction parameters
    VME_COPY_DWORD(uniIn, 1, 7, CURBEData, 7);
    VME_CLEAR_UNIInput_IntraCornerSwap(uniIn);
    VME_SET_NEIGHBOUR_AVAILABILITY_INTER(uniIn, neighborMbMask);

    // register M2
    // M2.0
    // M2.5 FBR parameters
    // M2.6/2.7 SIC Forward Transform Coeff Threshold Matrix
    uniIn.row(2).format<uint4> ().select<2, 1> (6) = CURBEData.format<uint4> ().select<2, 1> (14);

    // BDW+
    // register M3
    // M3.0 FWD Cost center 0 Delta X and Y
    VME_COPY_DWORD(uniIn, 3, 0, mvPred, 0);
    // M3.1 BWD Cost center 0 Delta X and Y
    // M3.2 FWD Cost center 1 Delta X and Y
    // M3.3 BWD Cost center 1 Delta X and Y
    // M3.4 FWD Cost center 2 Delta X and Y
    // M3.5 BWD Cost center 2 Delta X and Y
    // M3.6 FWD Cost center 3 Delta X and Y
    // M3.7 BWD Cost center 3 Delta X and Y

    //
    // initialize SIC input
    //

    // register M0
    // 0.0 Ref0 Skip Center 0 Delta XY
    VME_SET_DWORD(sicIn, 0, 0, skipMvPred);
    // 0.1 Ref1 Skip Center 0 Delta XY (not used in P slices)
    // 0.2 Ref0 Skip Center 1 Delta XY (not used in P slices)
    // 0.3 Ref1 Skip Center 1 Delta XY (not used in P slices)
    // 0.4 Ref0 Skip Center 2 Delta XY (not used in P slices)
    // 0.5 Ref1 Skip Center 2 Delta XY (not used in P slices)
    // 0.6 Ref0 Skip Center 3 Delta XY (not used in P slices)
    // 0.7 Ref1 Skip Center 3 Delta XY (not used in P slices)

    // register M1
    // M1.0 ACV Intra 4x4/8x8 mode mask
    sicIn.row(1).format<uint4> ().select<2, 1> (0) = CURBEData.format<uint4> ().select<2, 1> (30);
    VME_SET_CornerNeighborPixel0(sicIn, topBlocksValues(0, 3));

    // M1.2/1.3/1.4/1.5/1.6/1.7 Neighbor pixel Luma values
    sicIn.row(1).format<uint4> ().select<6, 1> (2) = topBlocksValues.format<uint4> ().select<6, 1> (1);

    // register M2
    // M2.0/2.1/2.2/2.3 Neighbor pixel luma values
    sicIn.row(2).format<uint4> ().select<4, 1> (0) = leftBlockValues.format<uint4> ().select<4, 1> (0);

    // M2.4 Intra Predictor Mode
    VME_SET_LeftMbIntraModes(sicIn, leftMbIntraModes);
    VME_SET_UpperMbIntraModes(sicIn, upperMbIntraModes);
    // M2.5 Corner pixel chroma value
    VME_SET_CornerNeighborPixelChroma(sicIn, (chromaTopBlocksValues.format<uint2>().select<1, 1>(1)));
    // M2.6 reserved
    // M2.7 Penalties for non-DC modes
    VME_COPY_DWORD(sicIn, 2, 7, CURBEData, 33);

    // register M3
    // M3.0/3.1/3.2/3.3 Neighbor pixel chroma values
    sicIn.row(3).format<uint4> ().select<4, 1> (0) = chromaLeftBlockValues.format<uint4> ().select<4, 1> (0);
    // M3.4/3.5/3.6/3.7 Neighbor pixel chroma values
    sicIn.row(3).format<uint4> ().select<4, 1> (4) = chromaTopBlocksValues.format<uint4> ().select<4, 1> (1);

    // set costs
    Costs.row(0).format<int2> ().select<2, 1> (0) = mvPred;

} // void SetUpVmePSlice(matrix_ref<uchar, 4, 32> uniIn,

_GENX_ inline
void LoadCosts(matrix_ref<uchar, 4, 32> uniIn,
               vector_ref<uchar, CURBEDATA_SIZE> CURBEData)
{

    // copy prepared costs from the CURBE data
    uniIn.row(2).format<uint4>().select<5, 1> (0) = CURBEData.format<uint4>().select<5, 1> (8);

} // void LoadCosts(matrix_ref<uchar, 4, 32> uniIn,

_GENX_ inline
void LoadSearchPath(matrix_ref<uchar, 6, 32> imeIn,
                    vector_ref<uchar, CURBEDATA_SIZE> CURBEData)
{

    // copy prepared search pathes from the CURBE data
    imeIn.format<uint4, 6, 8>().select<2, 1, 8, 1> (0, 0) = CURBEData.format<uint4, 5, 8>().select<2, 1, 8, 1> (2, 0);

} // void LoadSearchPath(matrix_ref<uchar, 6, 32> imeIn,

_GENX_ inline
void PrepareFractionalCall(matrix_ref<uchar, 4, 32> uniIn,
                           matrix_ref<uchar, 4, 32> fbrIn,
                           matrix_ref<uchar, 7, 32> uniOut,
                           uint1 CheckBiDir)
{
    U8 FBRMbMode, FBRSubMbShape, FBRSubPredMode;

    // copy mb mode
    VME_GET_UNIOutput_InterMbMode(uniOut, FBRMbMode);
    VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);

    // copy sub mb shape
    VME_GET_UNIOutput_SubMbShape(uniOut, FBRSubMbShape);
    VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, FBRSubMbShape);

    // copy sub mb prediction modes
    VME_GET_UNIOutput_SubMbPredMode(uniOut, FBRSubPredMode);
    VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);

    if (!CheckBiDir || FBRSubMbShape)
        VME_SET_UNIInput_BMEDisableFBR(uniIn);
    else
        VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);

    // copy MVs
    fbrIn = uniOut.select<4, 1, 32, 1> (1, 0);

} // void PrepareFractionalCall(matrix_ref<uchar, 4, 32> uniIn,

template <int sliceType>
_GENX_ inline
void CheckAllFractional(matrix_ref<uchar, 3, 32> uniIn,
                        matrix_ref<uchar, 7, 32> best_uniOut,
                        matrix_ref<uchar, 4, 32> best_imeOutIn,
                        SurfaceIndex VMEInterPredictionSurfIndex,
                        matrix_ref<uint4, 16, 2> Costs,
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData)
{
    uint1 best_uniOut_MbSubShape;
    matrix<uchar, 11, 32> best_imeOut;

    best_imeOut.select<4, 1, 32, 1> (7, 0) = best_imeOutIn;

    if (GET_CURBE_AllFractional(CURBEData))
    {
        U8 mode;
        uchar FBRMbMode, FBRSubMbShape, FBRSubPredMode;

        VME_GET_UNIOutput_InterMbMode(best_uniOut, mode);

        matrix<uchar, 4, 32> fbrIn;
        matrix<uchar, 7, 32> uniOut;

        // BMEDisableFBR: BPREDSLICE -> 0, PREDSLICE -> 1
        if (sliceType == BPREDSLICE)
            VME_CLEAR_UNIInput_BMEDisableFBR(uniIn);

        FBRSubMbShape = 0;
        VME_SET_UNIInput_FBRSubMBShapeInput(uniIn, FBRSubMbShape);

        if (mode != 0)//check 16x16
        {
            FBRMbMode = MbMode_16x16;
            VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);
            FBRSubPredMode = 0;
            if (sliceType == BPREDSLICE)
            {
                uint2 best_imeOut_RecordDst16_0;
                uint2 best_imeOut_RecordDst16_1;

                VME_GET_IMEOutput_Rec0_16x16_Distortion(best_imeOut, best_imeOut_RecordDst16_0);
                VME_GET_IMEOutput_Rec1_16x16_Distortion(best_imeOut, best_imeOut_RecordDst16_1);

                // get 16x16 distortions
                if (best_imeOut_RecordDst16_0 > best_imeOut_RecordDst16_1)
                    FBRSubPredMode = 1;
            }
            VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);
            // clone motion vectors
            fbrIn.format<uint4, 4, 8>().select<4, 1, 4, 2> (0, 0) = best_imeOut.row(7).format<uint4>()[5];
            fbrIn.format<uint4, 4, 8>().select<4, 1, 4, 2> (0, 1) = best_imeOut.row(9).format<uint4>()[5];

            uint1 fwdRefId, bwdRefId;
            VME_GET_IMEOutput_Rec0_16x16_RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_16x16_RefID(best_imeOut, bwdRefId);

            VME_SET_UNIInput_Blk0RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk1RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk2RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk3RefID(uniIn, fwdRefId, bwdRefId);

            // set cost centers
            uniIn.row(1).format<uint4>().select<1, 1>(4) = Costs.row(fwdRefId)[0];
            uniIn.row(1).format<uint4>().select<1, 1>(5) = Costs.row(bwdRefId)[1];

            // make fraction motion search
            matrix<uchar, 3, 32> uniIn2 = uniIn;
            run_vme_fbr(uniIn2, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, uniOut);

            uint2 uniOut_DistInter, best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(uniOut, uniOut_DistInter);
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);
            if(uniOut_DistInter < best_uniOut_DistInter)
            {
                best_uniOut = uniOut;
            }
        }
        if (mode != 1)//check 16x8
        {
            FBRMbMode = MbMode_16x8;
            VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);
            FBRSubPredMode = 0;
            if (sliceType == BPREDSLICE)
            {
                uint2 best_imeOut_RecordDst_0;
                uint2 best_imeOut_RecordDst_1;

                // get 16x8 distortions
                VME_GET_IMEOutput_Rec0_16x8_0Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_16x8_0Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x05;

                // get 16x8 distortions
                VME_GET_IMEOutput_Rec0_16x8_1Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_16x8_1Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x50;
            }
            VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);
            // clone motion vectors
            fbrIn.format<uint4, 4, 8>().select<2, 1, 4, 2> (0, 0) = best_imeOut.row(8).format<uint4>()[0];
            fbrIn.format<uint4, 4, 8>().select<2, 1, 4, 2> (2, 0) = best_imeOut.row(8).format<uint4>()[1];
            fbrIn.format<uint4, 4, 8>().select<2, 1, 4, 2> (0, 1) = best_imeOut.row(10).format<uint4>()[0];
            fbrIn.format<uint4, 4, 8>().select<2, 1, 4, 2> (2, 1) = best_imeOut.row(10).format<uint4>()[1];

            uint1 fwdRefId, bwdRefId;

            VME_GET_IMEOutput_Rec0_16x8_1RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_16x8_1RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk2RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk3RefID(uniIn, fwdRefId, bwdRefId);

            VME_GET_IMEOutput_Rec0_16x8_0RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_16x8_0RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk0RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk1RefID(uniIn, fwdRefId, bwdRefId);

            // set cost centers
            uniIn.row(1).format<uint4>().select<1, 1>(4) = Costs.row(fwdRefId)[0];
            uniIn.row(1).format<uint4>().select<1, 1>(5) = Costs.row(bwdRefId)[1];

            // make fraction motion search
            matrix<uchar, 3, 32> uniIn2 = uniIn;
            run_vme_fbr(uniIn2, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, uniOut);

            uint2 uniOut_DistInter, best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(uniOut, uniOut_DistInter);
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);
            if(uniOut_DistInter < best_uniOut_DistInter)
            {
                best_uniOut = uniOut;
            }
        }
        if (mode != 2)//check 8x16
        {
            FBRMbMode = MbMode_8x16;
            VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);
            FBRSubPredMode = 0;
            if (sliceType == BPREDSLICE)
            {
                uint2 best_imeOut_RecordDst_0;
                uint2 best_imeOut_RecordDst_1;

                // get 8x16 distortions
                VME_GET_IMEOutput_Rec0_8x16_0Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x16_0Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x11;

                // get 8x16 distortions
                VME_GET_IMEOutput_Rec0_8x16_1Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x16_1Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x44;
            }
            VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);
            // clone motion vectors
            fbrIn.format<uint4, 4, 8>().select<2, 2, 4, 2> (0, 0) = best_imeOut.row(8).format<uint4>()[2];
            fbrIn.format<uint4, 4, 8>().select<2, 2, 4, 2> (1, 0) = best_imeOut.row(8).format<uint4>()[3];
            fbrIn.format<uint4, 4, 8>().select<2, 2, 4, 2> (0, 1) = best_imeOut.row(10).format<uint4>()[2];
            fbrIn.format<uint4, 4, 8>().select<2, 2, 4, 2> (1, 1) = best_imeOut.row(10).format<uint4>()[3];

            uint1 fwdRefId, bwdRefId;

            VME_GET_IMEOutput_Rec0_8x16_1RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x16_1RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk1RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk3RefID(uniIn, fwdRefId, bwdRefId);

            VME_GET_IMEOutput_Rec0_8x16_0RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x16_0RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk0RefID(uniIn, fwdRefId, bwdRefId);
            VME_SET_UNIInput_Blk2RefID(uniIn, fwdRefId, bwdRefId);

            // set cost centers
            uniIn.row(1).format<uint4>().select<1, 1>(4) = Costs.row(fwdRefId)[0];
            uniIn.row(1).format<uint4>().select<1, 1>(5) = Costs.row(bwdRefId)[1];

            // make fraction motion search
            matrix<uchar, 3, 32> uniIn2 = uniIn;
            run_vme_fbr(uniIn2, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, uniOut);

            uint2 uniOut_DistInter, best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(uniOut, uniOut_DistInter);
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);
            if(uniOut_DistInter < best_uniOut_DistInter)
            {
                best_uniOut = uniOut;
            }
        }
        VME_GET_UNIOutput_SubMbShape(best_uniOut, best_uniOut_MbSubShape);
        if ((mode != 3) || (best_uniOut_MbSubShape != 0)) // 8x8 no minors
        {
            FBRMbMode = MbMode_8x8;
            VME_SET_UNIInput_FBRMbModeInput(uniIn, FBRMbMode);
            FBRSubPredMode = 0;
            if (sliceType == BPREDSLICE)
            {
                uint2 best_imeOut_RecordDst_0;
                uint2 best_imeOut_RecordDst_1;

                // get 8x8 distortions
                VME_GET_IMEOutput_Rec0_8x8_0Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x8_0Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x01;

                // get 8x8 distortions
                VME_GET_IMEOutput_Rec0_8x8_1Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x8_1Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x04;

                // get 8x8 distortions
                VME_GET_IMEOutput_Rec0_8x8_2Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x8_2Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x10;

                // get 8x8 distortions
                VME_GET_IMEOutput_Rec0_8x8_3Distortion(best_imeOut, best_imeOut_RecordDst_0);
                VME_GET_IMEOutput_Rec1_8x8_3Distortion(best_imeOut, best_imeOut_RecordDst_1);
                if (best_imeOut_RecordDst_0 > best_imeOut_RecordDst_1)
                    FBRSubPredMode |= 0x40;
            }
            VME_SET_UNIInput_FBRSubPredModeInput(uniIn, FBRSubPredMode);

            // clone motion vectors
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (0, 0) = best_imeOut.row(8).format<uint4>()[4];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (1, 0) = best_imeOut.row(8).format<uint4>()[5];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (2, 0) = best_imeOut.row(8).format<uint4>()[6];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (3, 0) = best_imeOut.row(8).format<uint4>()[7];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (0, 1) = best_imeOut.row(10).format<uint4>()[4];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (1, 1) = best_imeOut.row(10).format<uint4>()[5];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (2, 1) = best_imeOut.row(10).format<uint4>()[6];
            fbrIn.format<uint4, 4, 8>().select<1, 1, 4, 2> (3, 1) = best_imeOut.row(10).format<uint4>()[7];

            uint1 fwdRefId, bwdRefId;

            VME_GET_IMEOutput_Rec0_8x8_1RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x8_1RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk1RefID(uniIn, fwdRefId, bwdRefId);

            VME_GET_IMEOutput_Rec0_8x8_2RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x8_2RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk2RefID(uniIn, fwdRefId, bwdRefId);

            VME_GET_IMEOutput_Rec0_8x8_3RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x8_3RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk3RefID(uniIn, fwdRefId, bwdRefId);

            VME_GET_IMEOutput_Rec0_8x8_0RefID(best_imeOut, fwdRefId);
            VME_GET_IMEOutput_Rec1_8x8_0RefID(best_imeOut, bwdRefId);
            VME_SET_UNIInput_Blk0RefID(uniIn, fwdRefId, bwdRefId);

            // set cost centers
            uniIn.row(1).format<uint4>().select<1, 1>(4) = Costs.row(fwdRefId)[0];
            uniIn.row(1).format<uint4>().select<1, 1>(5) = Costs.row(bwdRefId)[1];

            // make fraction motion search
            matrix<uchar, 3, 32> uniIn2 = uniIn;
            run_vme_fbr(uniIn2, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, uniOut);

            uint2 uniOut_DistInter, best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(uniOut, uniOut_DistInter);
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);
            if(uniOut_DistInter < best_uniOut_DistInter)
            {
                best_uniOut = uniOut;
            }
        }
    }

} // void CheckAllFractional(matrix_ref<uchar, 3, 32> uniIn,

template <int sliceType>
_GENX_ inline
void DoInterFramePrediction(SurfaceIndex VMEInterPredictionSurfIndex,
                            matrix_ref<uchar, 4, 32> uniIn,
                            matrix_ref<uchar, 6, 32> imeIn,
                            matrix_ref<uint4, 16, 2> Costs,
                            matrix_ref<uchar, 7, 32> best_uniOut,
                            vector_ref<uint1, 1> direct8x8pattern,
                            vector_ref<uchar, CURBEDATA_SIZE> CURBEData)
{
    matrix<uchar, 4, 32> fbrIn = 0;
    matrix<uchar, 4, 32> best_imeOut = 0;

    VME_SET_UNIOutput_InterDistortion(best_uniOut, -1);
    VME_SET_IMEOutput_Rec0_16x16_Distortion(best_imeOut, -1);
    VME_SET_IMEOutput_Rec0_Distortions(best_imeOut, -1);
    VME_SET_IMEOutput_Rec1_16x16_Distortion(best_imeOut, -1);
    VME_SET_IMEOutput_Rec1_Distortions(best_imeOut, -1);

    direct8x8pattern = 0;

    if (sliceType == PREDSLICE)
    {
        {
            /* multiple prediction is not supported yet
            UpdateChecked(uniIn, 0); */

            {
                matrix<uchar, 9, 32> temp = 0;
                vector<short, 2> ref0;
                vector<ushort, 16> costCenter;
                matrix<uchar, 4, 32> uniIn0 = uniIn;
                matrix<uchar, 2, 32> imeIn0 = imeIn.select<2, 1, 32, 1> (0, 0);

                ref0 = uniIn.row(0).format<short> ().select<2, 1> (0);
                costCenter = uniIn.row(3).format<ushort> ().select<16, 1> (0);
                run_vme_ime(uniIn0, imeIn0, VME_STREAM_OUT, VME_SEARCH_SINGLE_REF_SINGLE_REC_SINGLE_START, VMEInterPredictionSurfIndex, ref0, NULL, costCenter, temp);
                best_uniOut = temp.select<7, 1, 32, 1> (0, 0);
                best_imeOut.select<2, 1, 32, 1> (0, 0) = temp.select<2, 1, 32, 1> (7, 0);
            }

            PrepareFractionalCall(uniIn, fbrIn, best_uniOut, 0);

            uchar FBRMbMode, FBRSubMbShape, FBRSubPredMode;
            // copy mb mode
            FBRMbMode = VME_GET_UNIInput_FBRMbModeInput(uniIn);
            // copy sub mb shape
            FBRSubMbShape = VME_GET_UNIInput_FBRSubMBShapeInput(uniIn);
            // copy sub mb prediction modes
            FBRSubPredMode = VME_GET_UNIInput_FBRSubPredModeInput(uniIn);

            if (GET_CURBE_SubPelMode(CURBEData))
            {
                matrix<uchar, 4, 32> uniIn0 = uniIn;
                matrix<uchar, 7, 32> temp;
                run_vme_fbr(uniIn0, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, temp);
                best_uniOut = temp;
            }

            /* multiple predictions are not used
            CheckAdditionalPred(uniIn, imeIn, best_uniOut, best_imeOut);*/
            /* there is only one reference
            MultiRefCheck(in_SliceMBData, &uniIn, &imeIn, uMB, &best_uniOut, &best_imeOut, 0, 0);*/
        }
    }
    else if (sliceType == BPREDSLICE)
    {
        {
            /* multiple prediction is not supprted yet
            UpdateChecked(&uniIn, 0);  UpdateChecked(&uniIn, 1);*/

            {
                matrix<uchar, 11, 32> temp;
                vector<short, 2> ref0, ref1;
                vector<ushort, 16> costCenter;
                matrix<uchar, 4, 32> uniIn0 = uniIn;
                matrix<uchar, 2, 32> imeIn0 = imeIn.select<2, 1, 32, 1> (0, 0);

                ref0 = uniIn.row(0).format<short> ().select<2, 1> (0);
                ref1 = uniIn.row(0).format<short> ().select<2, 1> (2);
                costCenter = uniIn.row(3).format<ushort> ().select<16, 1> (0);
                run_vme_ime(uniIn0, imeIn0, VME_STREAM_OUT, VME_SEARCH_DUAL_REF_DUAL_REC, VMEInterPredictionSurfIndex, ref0, ref1, costCenter, temp);
                best_uniOut = temp.select<7, 1, 32, 1> (0, 0);
                best_imeOut.select<4, 1, 32, 1> (0, 0) = temp.select<4, 1, 32, 1> (7, 0);
            }

            PrepareFractionalCall(uniIn, fbrIn, best_uniOut, 1);

            uchar FBRMbMode, FBRSubMbShape, FBRSubPredMode;
            // copy mb mode
            FBRMbMode = VME_GET_UNIInput_FBRMbModeInput(uniIn);
            // copy sub mb shape
            FBRSubMbShape = VME_GET_UNIInput_FBRSubMBShapeInput(uniIn);
            // copy sub mb prediction modes
            FBRSubPredMode = VME_GET_UNIInput_FBRSubPredModeInput(uniIn);

            if (GET_CURBE_SubPelMode(CURBEData))
            {
                matrix<uchar, 4, 32> uniIn0 = uniIn;
                matrix<uchar, 7, 32> temp;
                run_vme_fbr(uniIn0, fbrIn, VMEInterPredictionSurfIndex, FBRMbMode, FBRSubMbShape, FBRSubPredMode, temp);
                best_uniOut = temp;
            }

            /* multiple predictions are not used
            status |= CheckAdditionalPred(&uniIn, &imeIn, &best_uniOut, &best_imeOut);*/
            /* there is only one reference
            if (CheckMultiRef)
                status |= MultiRefBiCheck(in_SliceMBData, &uniIn, &imeIn, uMB, &best_uniOut, &best_imeOut, 0, 0, 0);*/
        }
    }

/*
    if (GET_CURBE_SubPelMode(CURBEData))
    {
        CheckAllFractional<sliceType> (uniIn, best_uniOut, best_imeOut,
                                       VMEInterPredictionSurfIndex, Costs, CURBEData);
*/

} // void DoInterFramePrediction(SurfaceIndex VMEInterPredictionSurfIndex,

_GENX_ inline
void SetUpPakDataPSlice(matrix_ref<uchar, 4, 32> uniIn,
                        matrix_ref<uchar, 7, 32> uniOut,
                        vector_ref<uchar, MBDATA_SIZE> MBData,
                        U8 /*direct8x8pattern*/,
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                        U32 x,
                        U32 y,
                        uint MbIndex)
{
    vector<uint4, 3> intraPredParam, interPredParam = 0;
    vector<short, 4> intraMask;
    vector<short, 1> transformMask;
    uint2 distInter, distIntra, distSkip;
    vector<uint1, 4> intraMbType;
    uint1 intraMbFlag;

    VME_INIT_SVC_PACK(MBData);

    // set the mask to select INTRA data
    VME_GET_UNIOutput_BestInterDistortion(uniOut, distInter);
    VME_GET_UNIOutput_BestIntraDistortion(uniOut, distIntra);
    VME_GET_UNIOutput_SkipRawDistortion(uniOut, distSkip);
    intraMbFlag = (distInter > distIntra);
    intraMask = intraMbFlag;

    // DW0 MB flags and modes
    VME_COPY_MB_INTRA_MODE_TYPE(intraMbType, 0, uniOut);
    VME_COPY_MB_INTER_MODE_TYPE(MBData, 0, uniOut);
    MBData.select<4, 1> (0).merge(intraMbType, intraMask);
    MBData.select<1, 1> (0).merge(MBData[0] | 4, distSkip != 65535);

    // DW1-3 intra luma modes & intra macroblock struct
    intraPredParam = uniOut.row(0).format<uint4>().select<3, 1> (4);
    intraPredParam[2] = intraPredParam[2] & 0x0ff;

    // DW1-3 inter luma modes & inter macroblock struct
    interPredParam.format<uint>().select<2, 1> (0) = uniOut.row(0).select<2, 1> (25);
    interPredParam[1] = uniOut.row(6).format<uint4>()[0] & 0x0f0f0f0f;
    interPredParam[2] = uint(-1);

    // select correct macroblock parameters
    MBData.format<uint4>().select<3, 1> (1) = interPredParam;
    MBData.format<uint4>().select<3, 1> (1).merge(intraPredParam, intraMask.select<3, 1> (0));

    // DW4 costs
    MBData.format<uint2>()[8] = distIntra;
    MBData.format<uint2>()[9] = distInter;

    // DW6-7 sum luma coeffs
    MBData.format<uint2>().select<4, 1>(12) = uniOut.row(6).format<uint2>().select<4, 1>(4);

    // DW8 num nz luma coeffs
    MBData.format<uint1>().select<4, 1>(32) = uniOut.row(6).format<uint1>().select<4, 1>(4);

    // DW9-10 cost centers like in HSW (to support multiref in BDW one needs to extend MBData)
    MBData.format<uint2>().select<1, 1>(18) = VME_GET_UNIInput_FWDCostCenter0_X(uniIn);
    MBData.format<uint2>().select<1, 1>(19) = VME_GET_UNIInput_FWDCostCenter0_Y(uniIn);
    MBData.format<uint2>().select<1, 1>(20) = VME_GET_UNIInput_BWDCostCenter0_X(uniIn);
    MBData.format<uint2>().select<1, 1>(21) = VME_GET_UNIInput_BWDCostCenter0_Y(uniIn);

    // copy motion vectors
    SET_MBDATA_MV(MBData, LIST_0, uniOut.row(1).format<uint4>()[0]);

} // void SetUpPakDataPSlice(matrix_ref<uchar, 4, 32> uniIn,

extern "C" _GENX_MAIN_ void
SVCEncMB_P(SurfaceIndex CurbeDataSurfIndex,
           SurfaceIndex SrcSurfIndexRaw,
           SurfaceIndex SrcSurfIndex,
           SurfaceIndex VMEInterPredictionSurfIndex,
           SurfaceIndex MBDataSurfIndex,
           SurfaceIndex /*FwdFrmMBDataSurfIndex*/)
{
//    cm_wait();
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint x = mbX * 16;
    uint y = mbY * 16;

    int offset = 0;
    uint PicWidthInMB, PicHeightInMBs;       //picture width/height in MB unit

    vector<uchar, CURBEDATA_SIZE> CURBEData;
    read(CurbeDataSurfIndex,0,CURBEData.select<128,1>());
    read(CurbeDataSurfIndex,128,CURBEData.select<32,1>(128));
    // read picture's width in MB units
    PicWidthInMB    = GET_CURBE_PictureWidth(CURBEData);
    PicHeightInMBs  = GET_CURBE_PictureHeight(CURBEData);

    uint MbIndex = PicWidthInMB * mbY + mbX;

    vector<uchar, 1> direct8x8pattern = 0;
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 7, 32> best_uniOut = 0;

    {
        // declare parameters for VME
        matrix<uchar, 6, 32> imeIn = 0;
        matrix<uchar, 4, 32> sicIn = 0;
        matrix<uchar, 7, 32> sic_uniOut = 0;
        matrix<uint4, 16, 2> Costs = 0;
        vector<int2, 2> mvPred = 0;

        vector<int2, 2> ref0;
        U8 ftqSkip = 0;

        // down scale
        uint LaScaleFactor = GET_CURBE_CurLayerDQId(CURBEData);
        if (LaScaleFactor == 2)
        {
            DownSampleMB2Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
        } else if (LaScaleFactor == 4)
        {
            DownSampleMB4Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
        }
        SetUpVmePSlice(uniIn, sicIn, Costs,
                       SrcSurfIndex, MBDataSurfIndex,
                       CURBEData,
                       x, y, MbIndex,
                       mvPred);
        ftqSkip = VME_GET_UNIInput_FTEnable(uniIn);

        VME_SET_UNIOutput_BestIntraDistortion(best_uniOut, -1);
        VME_SET_UNIOutput_InterDistortion(best_uniOut, -1);
        VME_SET_UNIOutput_SkipRawDistortion(best_uniOut, -1);

        {
            LoadCosts(uniIn, CURBEData);
            LoadSearchPath(imeIn, CURBEData);

            // call intra/skip motion estimation
            run_vme_sic(uniIn, sicIn, VMEInterPredictionSurfIndex, sic_uniOut);

            vector<uint2, 1> Coeffs = cm_sum<uint2>(sic_uniOut.row(6).format<uint1>().select<4, 1> (4));
            vector<uint2, 1> sic_uniOut_DistSkip;
            vector<uint2, 1> sic_uniOut_DistInter;
            uint SkipModeEn = VME_GET_UNIInput_SkipModeEn(uniIn);
            uint1 ifCondition;

            // get acceptable skip value
            uint2 SkipVal = GET_CURBE_SkipVal(CURBEData);

            VME_GET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip[0]);
            _VME_GET_UNIOutput_InterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            Coeffs.merge(ushort(-1), SkipModeEn - 1);
            sic_uniOut_DistSkip.merge(ushort(-1), SkipModeEn - 1);
            sic_uniOut_DistInter.merge(ushort(-1), SkipModeEn - 1);

            ifCondition = (ftqSkip && Coeffs[0]) ||
                          (!ftqSkip && sic_uniOut_DistSkip[0] > SkipVal);

            sic_uniOut_DistSkip.merge(ushort(-1), ifCondition);

            VME_SET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip[0]);
            VME_SET_UNIOutput_InterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            if ((!VME_GET_UNIInput_SkipModeEn(uniIn)) ||
                ifCondition)
            {
                // Calculate AVC INTER prediction costs
                DoInterFramePrediction<PREDSLICE> (VMEInterPredictionSurfIndex, uniIn, imeIn, Costs,
                                                   best_uniOut, direct8x8pattern,
                                                   CURBEData);
            }

            VME_GET_UNIOutput_BestInterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            uint2 sic_uniOut_DistIntra;
            VME_GET_UNIOutput_BestIntraDistortion(sic_uniOut, sic_uniOut_DistIntra);
            VME_SET_UNIOutput_BestIntraDistortion(best_uniOut, sic_uniOut_DistIntra);

            VME_GET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip);
            VME_SET_UNIOutput_SkipRawDistortion(best_uniOut, sic_uniOut_DistSkip);

            uint2 best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);

            if ((sic_uniOut_DistIntra < best_uniOut_DistInter) ||
                (!ftqSkip && sic_uniOut_DistSkip[0] < SkipVal) ||
                (ftqSkip && !Coeffs[0]) ||
                (sic_uniOut_DistInter[0] < best_uniOut_DistInter))
            {
                if (ftqSkip && !Coeffs[0])
                {
                    VME_SET_UNIOutput_SkipRawDistortion(sic_uniOut, 0);
                }
                direct8x8pattern = 0xf;
                best_uniOut = sic_uniOut;

            }
            else
            {
                vector<uint1, 4> lumanzc      = sic_uniOut.row(6).format<uint1>().select<4, 1>(4);
                vector<uint1, 2> chromanzc    = sic_uniOut.row(6).format<uint1>().select<2, 1>(16);
                vector<uint2, 4> lumacoeffs   = sic_uniOut.row(6).format<uint2>().select<4, 1>(4);
                vector<uint2, 2> chromacoeffs = sic_uniOut.row(6).format<uint2>().select<2, 1>(10);
                best_uniOut.row(6).format<uint1>().select<4, 1>(4)  = lumanzc;
                best_uniOut.row(6).format<uint1>().select<2, 1>(16) = chromanzc;
                best_uniOut.row(6).format<uint2>().select<4, 1>(4)  = lumacoeffs;
                best_uniOut.row(6).format<uint2>().select<2, 1>(10) = chromacoeffs;
            }
        }
    }

    //
    // write the result
    //

    vector<uchar, MBDATA_SIZE> MBData;

    // pack the VME result
    SetUpPakDataPSlice(uniIn, best_uniOut, MBData, direct8x8pattern[0],
                       CURBEData, x, y, MbIndex);

    // write back updated MB data
    offset = MbIndex * MBDATA_SIZE;
    write(MBDataSurfIndex, offset, MBData);

    cm_fence();
}

_GENX_ inline
void GetNeighbourParamB(matrix_ref<uint1, 16, 1> leftBlockValues,
                        matrix_ref<uint1, 1, 28> topBlocksValues,
                        matrix_ref<uint1, 8, 2> chromaLeftBlockValues,
                        matrix_ref<uint1, 1, 20> chromaTopBlocksValues,
                        vector_ref<uchar, 8> leftMbIntraModes,
                        vector_ref<uchar, 8> upperMbIntraModes,
                        vector_ref<uchar, 1> neighborMbMask,
                        matrix_ref<int2, 2, 6> MV,
                        matrix_ref<uint1, 2, 3> refIdx,
                        SurfaceIndex SrcSurfIndex,
                        SurfaceIndex MBDataSurfIndex,
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                        uint x, uint y, uint mbIndex, uint mbWidth)
{
    matrix<int2, 2, 2> mvA, mvB, mvC, tmp, equalMV;
    matrix<uint2, 2, 2> equalMask, equalNum = 0;
    matrix<uint4, 2, 1> refIdxA, refIdxB, refIdxC, refIdxD, tmpRefIdx;
    // neighbors mask
    vector<uint1, 1> leftMask = 0;
    vector<uint1, 1> upperMask = 0;
    vector<uint1, 1> upperRightMask = 0;
    vector<uint1, 1> upperLeftMask = 0;
    const uint mbX = x / 16;
    const uint mbY = y / 16;
    const uint1 topPresent = mbY % GET_CURBE_SliceMacroblockHeight(CURBEData);

    // reset output(s)
    leftBlockValues = 0;
    topBlocksValues = 0;
    chromaLeftBlockValues = 0;
    chromaTopBlocksValues = 0;
    leftMbIntraModes = 0x2222;
    upperMbIntraModes = 0x2222;
    neighborMbMask = 0;
    refIdx = -1;
    MV = 0;

    // there are no neighbors
    if (0 == (x | topPresent))
    {
        return;
    }

    cm_wait();

    // read left macroblock parameters
    refIdxA = -1;
    mvA = 0;
    if (mbX)
    {
        vector<uchar, MBDATA_SIZE> leftMbData;
        vector<short, 8> intraMbFlagMask;
        uint1 intraMbFlag, intraMbMode;
        matrix<uint2, 2, 2> mvMask = 0;
        int offset;

        // read intra types
        offset = (mbIndex - 1) * MBDATA_SIZE;
        read(DWALIGNED(MBDataSurfIndex), offset, leftMbData);
        intraMbMode = GET_MBDATA_GET_INTRA_MODE(leftMbData);
        intraMbFlag = GET_MBDATA_IntraMbFlag(leftMbData);
        intraMbFlagMask = (intraMbFlag & (0 < intraMbMode));
        leftMbIntraModes.merge(leftMbData.select<8, 1> (INTRA_TYPES_OFFSET), intraMbFlagMask);
        leftMask = 0x60;

        // read motion vector
        GET_MBDATA_MV(tmp.format<uint4>()[0], leftMbData, LIST_0);
        GET_MBDATA_MV(tmp.format<uint4>()[1], leftMbData, LIST_1);

        // read reference index
        tmpRefIdx.row(FRW) = GET_MBDATA_GET_REF_INDEX(leftMbData, LIST_0);
        tmpRefIdx.row(BWD) = GET_MBDATA_GET_REF_INDEX(leftMbData, LIST_1);

        // check if the macroblock has motion vectors.
        // In B slice we need to check for not-INTRA type and correct reference.
        refIdxA.merge(tmpRefIdx, (intraMbFlag - 1));
        mvMask = (0 <= refIdxA.format<int2> ());
        mvA.merge(tmp, mvMask);

        // get the equal motion vector, if reference is zero and macroblock is
        // not-INTRA
        equalMask.row(FRW) = (0 == refIdxA.row(FRW)[0]);
        equalMask.row(BWD) = (0 == refIdxA.row(BWD)[0]);
        equalMV.merge(tmp, equalMask);
        equalNum += equalMask;
    }

    // read upper macroblock parameters
    refIdxB = -1;
    mvB = 0;
    refIdxC = -1;
    mvC = 0;
    if (topPresent)
    {
        vector<uchar, MBDATA_SIZE> upperMbData;
        vector<short, 8> intraMbFlagMask;
        uint1 intraMbFlag, intraMbMode, intraMbFlagD = 0;
        matrix<uint2, 2, 2> mvMask = 0;
        int offset;

        // read intra types
        offset = (mbIndex - mbWidth) * MBDATA_SIZE;
        read(DWALIGNED(MBDataSurfIndex), offset, upperMbData);
        intraMbMode = GET_MBDATA_GET_INTRA_MODE(upperMbData);
        intraMbFlag = GET_MBDATA_IntraMbFlag(upperMbData);
        intraMbFlagMask = (intraMbFlag & (0 < intraMbMode));
        upperMbIntraModes.merge(upperMbData.select<8, 1> (INTRA_TYPES_OFFSET), intraMbFlagMask);
        upperMask = 0x10;

        // read motion vector
        GET_MBDATA_MV(tmp.format<uint4>()[0], upperMbData, LIST_0);
        GET_MBDATA_MV(tmp.format<uint4>()[1], upperMbData, LIST_1);

        // read reference index
        tmpRefIdx.row(FRW) = GET_MBDATA_GET_REF_INDEX(upperMbData, LIST_0);
        tmpRefIdx.row(BWD) = GET_MBDATA_GET_REF_INDEX(upperMbData, LIST_1);

        // check if the macroblock has motion vectors.
        // In B slice we need to check for not-INTRA type and correct reference.
        refIdxB.merge(tmpRefIdx, (intraMbFlag - 1));
        mvMask = (0 <= refIdxB.format<int2> ());
        mvB.merge(tmp, mvMask);

        // get the equal motion vector, if reference is zero and macroblock is
        // not-INTRA
        equalMask.row(FRW) = (0 == refIdxB.row(FRW)[0]);
        equalMask.row(BWD) = (0 == refIdxB.row(BWD)[0]);
        equalMV.merge(tmp, equalMask);
        equalNum += equalMask;

        // read macroblock type for upper left neighbor
        if (mbX)
        {
            vector<uchar, MBDATA_SIZE> upperLeftMbData;

            // read intra types
            offset = (mbIndex - mbWidth - 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperLeftMbData);
            intraMbFlagD = GET_MBDATA_IntraMbFlag(upperLeftMbData);
            upperLeftMask = 0x08;

            // read reference index
            tmpRefIdx.row(FRW) = GET_MBDATA_GET_REF_INDEX(upperLeftMbData, LIST_0);
            tmpRefIdx.row(BWD) = GET_MBDATA_GET_REF_INDEX(upperLeftMbData, LIST_1);

            refIdxD.merge(tmpRefIdx, (intraMbFlagD - 1));
        }

        // read third macroblock
        if (mbX + 1 < mbWidth)
        {
            vector<uchar, MBDATA_SIZE> upperRightMbData;

            // read intra types
            offset = (mbIndex - mbWidth + 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperRightMbData);
            intraMbFlag = GET_MBDATA_IntraMbFlag(upperRightMbData);
            upperRightMask = 0x04;

            // read motion vector
            GET_MBDATA_MV(tmp.format<uint4>()[0], upperRightMbData, LIST_0);
            GET_MBDATA_MV(tmp.format<uint4>()[1], upperRightMbData, LIST_1);

            // read reference index
            tmpRefIdx.row(FRW) = GET_MBDATA_GET_REF_INDEX(upperRightMbData, LIST_0);
            tmpRefIdx.row(BWD) = GET_MBDATA_GET_REF_INDEX(upperRightMbData, LIST_1);

            // check if the macroblock has motion vectors.
            // In B slice we need to check for not-INTRA type and correct reference.
            refIdxC.merge(tmpRefIdx, (intraMbFlag - 1));
            mvMask = (0 <= refIdxC.format<int2> ());
            mvC.merge(tmp, mvMask);

            // get the equal motion vector, if reference is zero and macroblock is
            // not-INTRA
            equalMask.row(FRW) = (0 == refIdxC.row(FRW)[0]);
            equalMask.row(BWD) = (0 == refIdxC.row(BWD)[0]);
            equalMV.merge(tmp, equalMask);
            equalNum += equalMask;
        }
        // read fourth macroblock
        else if (mbX)
        {
            vector<uchar, MBDATA_SIZE> upperLeftMbData;

            // read motion vector
            offset = (mbIndex - mbWidth - 1) * MBDATA_SIZE;
            read(DWALIGNED(MBDataSurfIndex), offset, upperLeftMbData);

            GET_MBDATA_MV(tmp.format<uint4>()[0], upperLeftMbData, LIST_0);
            GET_MBDATA_MV(tmp.format<uint4>()[1], upperLeftMbData, LIST_1);

            // check if the macroblock has motion vectors.
            // In B slice we need to check for not-INTRA type and correct reference.
            refIdxC.merge(refIdxD, uint(-1), (intraMbFlagD - 1));
            mvMask = (0 <= refIdxC.format<int2> ());
            mvC.merge(tmp, mvMask);

            // get the equal motion vector, if reference is zero and macroblock is
            // not-INTRA
            equalMask.row(FRW) = (0 == refIdxC.row(FRW)[0]);
            equalMask.row(BWD) = (0 == refIdxC.row(BWD)[0]);
            equalMV.merge(tmp, equalMask);
            equalNum += equalMask;
        }
    }

    // make neighbor's mask for intra prediction
    neighborMbMask = leftMask | upperMask | upperLeftMask | upperRightMask;

    MV.select<2, 1, 2, 1> (0, 0) = mvA;
    MV.select<2, 1, 2, 1> (0, 2) = mvB;
    MV.select<2, 1, 2, 1> (0, 4) = mvC;
    refIdx.select<2, 1, 1, 1> (0, 0) = refIdxA;
    refIdx.select<2, 1, 1, 1> (0, 1) = refIdxB;
    refIdx.select<2, 1, 1, 1> (0, 2) = refIdxC;

    //
    // read planes
    //
    if (leftMask[0])
    {
        // read luminance samples
        {
            matrix<uint1, 16, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y, temp);
            leftBlockValues = temp.select<16, 1, 1, 1> (0, 3);
        }

        // read chrominance samples
        {
            matrix<uint1, 8, 4> temp;
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2, temp);
            chromaLeftBlockValues = temp.select<8, 1, 2, 1> (0, 2);
        }
    }

    if (upperMask[0])
    {
        if (upperLeftMask[0])
        {
            // read luminance samples
            if (upperRightMask[0])
            {
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, topBlocksValues);
            }
            else
            {
                matrix<uint1, 1, 20> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x - 4, y - 1, temp);
                topBlocksValues.select<1, 1, 20, 1> (0, 0) = temp;
                topBlocksValues.select<1, 1, 8, 1> (0, 20) = temp.row(0)[19];
            }

            // read chrominance samples
            read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x - 4, y / 2 - 1, chromaTopBlocksValues);
        }
        else
        {
            // read luminance samples
            if (upperRightMask[0])
            {
                matrix<uint1, 1, 24> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
                topBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                topBlocksValues.select<1, 1, 24, 1> (0, 4) = temp;
            }
            else
            {
                matrix<uint1, 1, 16> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_Y_PLANE, x, y - 1, temp);
                topBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                topBlocksValues.select<1, 1, 16, 1> (0, 4) = temp;
                topBlocksValues.select<1, 1, 8, 1> (0, 20) = temp.row(0)[15];
            }

            // read chrominance samples
            {
                matrix<uint1, 1, 16> temp;
                read_plane(SrcSurfIndex, GENX_SURFACE_UV_PLANE, x, y / 2 - 1, temp);
                chromaTopBlocksValues.select<1, 1, 4, 1> (0, 0) = 0;
                chromaTopBlocksValues.select<1, 1, 16, 1> (0, 4) = temp;
            }
        }
    }

} // void GetNeighbourParamB(vector_ref<uchar, 8> leftMbIntraModes,

_GENX_ inline
void ComputeBDirectMV(uint4 x, uint4 y, uint4 MbIndex,
                      matrix_ref<int2, 2, 2> mv_direct,
                      vector_ref<int1, 2> directRefIdx,
                      matrix_ref<int2, 2, 6> MV,
                      matrix<uint1, 2, 3> refIdx,
                      SurfaceIndex FwdFrmMBDataSurfIndex,
                      vector_ref<uchar, CURBEDATA_SIZE> CURBEData)
{
    matrix<int2, 2, 2> mvMedian, mvFinal;
    vector<short, 2> mvMask;
    vector<uint1, 4> mvDirectMask;
    const uint1 topPresent = (y / 16) % GET_CURBE_SliceMacroblockHeight(CURBEData);

    // reset the vector
    mv_direct = 0;
    directRefIdx = 0;

    // nothing to do for first macroblock
    if (0 == (x | topPresent))
    {
        return;
    }

    directRefIdx[LIST_0] = cm_reduced_min<uint1> (refIdx.row(LIST_0));
    directRefIdx[LIST_1] = cm_reduced_min<uint1> (refIdx.row(LIST_1));

    if (-1 == directRefIdx.format<int2> ()[0])
    {
        directRefIdx = 0;
        return;
    }

    short isNotMoving = 0;

    // set 'is not moving' flags
    if (GET_CURBE_isFwdFrameShortTermRef(CURBEData) == 1)
    {
        vector<short, 4> fwdRefMV;
        vector<uint1, MBDATA_SIZE> colMbData;
        int offset;

        // read 'colocated' MB info
        offset = MbIndex * MBDATA_SIZE;
        read(DWALIGNED(FwdFrmMBDataSurfIndex), offset, colMbData);

        // read 'colocated' motion vectors
        GET_MBDATA_MV(fwdRefMV.format<uint4> ()(0), colMbData, LIST_0);
        GET_MBDATA_MV(fwdRefMV.format<uint4> ()(1), colMbData, LIST_1);

        fwdRefMV += 1;

        // set flags when vectors lie in [-1..1]
        mvDirectMask = (2 >= fwdRefMV.format<uint2> ());
        fwdRefMV.format<uint2> () = mvDirectMask;

        // remove flags if reference is non-zero
        mvDirectMask.select<2, 1> (0) = (0 != GET_MBDATA_GET_REF_INDEX(colMbData, LIST_0));
        mvDirectMask.select<2, 1> (2) = ((-1 != GET_MBDATA_GET_REF_INDEX(colMbData, LIST_0)) ||
                                         (0 != GET_MBDATA_GET_REF_INDEX(colMbData, LIST_1)));
        fwdRefMV.merge(0, mvDirectMask);

        // remove flags if macroblock is intra
        mvDirectMask = GET_MBDATA_IntraMbFlag(colMbData);
        fwdRefMV.merge(0, mvDirectMask.select<4, 1> (0));

        fwdRefMV.select<2, 2>(0) &= fwdRefMV.select<2, 2>(1);   // both X and Y are not zero
        isNotMoving = fwdRefMV(0) | fwdRefMV(2);
    }

    mvMask = ((0 < x) && (0 == topPresent));
    MV.row(LIST_0).format<uint4> ().select<2, 1> (1).merge(MV.row(LIST_0).format<uint4> ()[0], mvMask);
    MV.row(LIST_1).format<uint4> ().select<2, 1> (1).merge(MV.row(LIST_1).format<uint4> ()[0], mvMask);
    refIdx.row(LIST_0).select<2, 1> (1).merge(refIdx.row(LIST_0)[0], mvMask);
    refIdx.row(LIST_1).select<2, 1> (1).merge(refIdx.row(LIST_1)[0], mvMask);

    // calculate the median vector
    GetMedianVector(mvMedian.row(LIST_0),
                    MV.row(LIST_0).select<2, 1> (0),
                    MV.row(LIST_0).select<2, 1> (2),
                    MV.row(LIST_0).select<2, 1> (4));
    GetMedianVector(mvMedian.row(LIST_1),
                    MV.row(LIST_1).select<2, 1> (0),
                    MV.row(LIST_1).select<2, 1> (2),
                    MV.row(LIST_1).select<2, 1> (4));
    mvFinal = mvMedian;

    // set L0 motion vectors
    mvMask = (refIdx.row(LIST_0)[0] != directRefIdx[LIST_0] && refIdx.row(LIST_0)[1] != directRefIdx[LIST_0] && refIdx.row(LIST_0)[2] == directRefIdx(LIST_0));
    mvFinal.row(LIST_0).merge(MV.row(LIST_0).select<2, 1> (4), mvMask);
    mvMask = (refIdx.row(LIST_0)[0] != directRefIdx[LIST_0] && refIdx.row(LIST_0)[1] == directRefIdx[LIST_0] && refIdx.row(LIST_0)[2] != directRefIdx[LIST_0]);
    mvFinal.row(LIST_0).merge(MV.row(LIST_0).select<2, 1> (2), mvMask);
    mvMask = (refIdx.row(LIST_0)[0] == directRefIdx[LIST_0] && refIdx.row(LIST_0)[1] != directRefIdx[LIST_0] && refIdx.row(LIST_0)[2] != directRefIdx[LIST_0]);
    mvFinal.row(LIST_0).merge(MV.row(LIST_0).select<2, 1> (0), mvMask);

    mv_direct.row(LIST_0).format<uint4> () = mvFinal.row(LIST_0).format<uint4> ()[0];

    mvDirectMask.select<1, 1> (0) = (directRefIdx[LIST_0] < 0);
    mv_direct.row(LIST_0).format<uint4> ().merge(0, mvDirectMask.select<1, 1> (0));
    mvDirectMask.select<1, 1> (0) = (directRefIdx[LIST_0] == 0);
    mvDirectMask.select<1, 1> (0).merge(isNotMoving, mvDirectMask.select<1, 1> (0));
    mv_direct.row(LIST_0).format<uint4> ().merge(0, mvDirectMask.select<1, 1> (0));

    // set L1 motion vectors
    mvMask = (refIdx.row(LIST_1)[0] != directRefIdx[LIST_1] && refIdx.row(LIST_1)[1] != directRefIdx[LIST_1] && refIdx.row(LIST_1)[2] == directRefIdx(LIST_1));
    mvFinal.row(LIST_1).merge(MV.row(LIST_1).select<2, 1> (4), mvMask);
    mvMask = (refIdx.row(LIST_1)[0] != directRefIdx[LIST_1] && refIdx.row(LIST_1)[1] == directRefIdx[LIST_1] && refIdx.row(LIST_1)[2] != directRefIdx[LIST_1]);
    mvFinal.row(LIST_1).merge(MV.row(LIST_1).select<2, 1> (2), mvMask);
    mvMask = (refIdx.row(LIST_1)[0] == directRefIdx[LIST_1] && refIdx.row(LIST_1)[1] != directRefIdx[LIST_1] && refIdx.row(LIST_1)[2] != directRefIdx[LIST_1]);
    mvFinal.row(LIST_1).merge(MV.row(LIST_1).select<2, 1> (0), mvMask);

    mv_direct.row(LIST_1).format<uint4> () = mvFinal.row(LIST_1).format<uint4> ()[0];

    mvDirectMask.select<1, 1> (0) = (directRefIdx[LIST_1] < 0);
    mv_direct.row(LIST_1).format<uint4> ().merge(0, mvDirectMask.select<1, 1> (0));
    mvDirectMask.select<1, 1> (0) = (directRefIdx[LIST_1] == 0);
    mvDirectMask.select<1, 1> (0).merge(isNotMoving, mvDirectMask.select<1, 1> (0));
    mv_direct.row(LIST_1).format<uint4> ().merge(0, mvDirectMask.select<1, 1> (0));

} // void ComputeBDirectMV(SurfaceIndex MVDataSurfIndex,

_GENX_ inline
void SetUpVmeBSlice(matrix_ref<uchar, 4, 32> uniIn,
                    matrix_ref<uchar, 4, 32> sicIn,
                    matrix_ref<uint4, 16, 2> Costs,
                    SurfaceIndex SrcSurfIndex,
                    SurfaceIndex MBDataSurfIndex,
                    SurfaceIndex FwdFrmMBDataSurfIndex,
                    vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                    uint x, uint y, uint MbIndex,
                    matrix_ref<int2, 2, 2> mvPred,
                    vector_ref<int1, 2> InitRefIdx)
{
    vector<uchar, 8> leftMbIntraModes, upperMbIntraModes;
    vector<uchar, 1> neighborMbMask;
    matrix<uint1, 16, 1> leftBlockValues;
    matrix<uint1, 1, 28> topBlocksValues;
    matrix<uint1, 8, 2> chromaLeftBlockValues;
    matrix<uint1, 1, 20> chromaTopBlocksValues;
    matrix<int2, 2, 6> MV;
    matrix<uint1, 2, 3> refIdx;
    uint mbWidth;

    matrix<int2, 2, 2> B_DirectMV;
    vector<int1, 2> B_DirectMVRefIdx;
    vector<short, 2> refMask;
    vector_ref<int2, 2> AveL0 = mvPred.row(FRW);
    vector_ref<int2, 2> AveL1 = mvPred.row(BWD);

    // read picture's width in MB units
    mbWidth = GET_CURBE_PictureWidth(CURBEData);

    GetNeighbourParamB(leftBlockValues,
                       topBlocksValues,
                       chromaLeftBlockValues,
                       chromaTopBlocksValues,
                       leftMbIntraModes,
                       upperMbIntraModes,
                       neighborMbMask,
                       MV,
                       refIdx,
                       SrcSurfIndex,
                       MBDataSurfIndex,
                       CURBEData,
                       x, y, MbIndex, mbWidth);

    ComputeBDirectMV(x, y, MbIndex,
                     B_DirectMV, B_DirectMVRefIdx,
                     MV, refIdx,
                     FwdFrmMBDataSurfIndex,
                     CURBEData);
    InitRefIdx = 0;
    refMask = (0 <= B_DirectMVRefIdx);
    InitRefIdx.merge(B_DirectMVRefIdx, refMask);

    AveL0[0] = B_DirectMV.row(FRW)[0];
    AveL0[1] = B_DirectMV.row(FRW)[1];

    AveL1[0] = B_DirectMV.row(BWD)[0];
    AveL1[1] = B_DirectMV.row(BWD)[1];

    // copy dwords from CURBE data into universal VME header
    // register M0
    // M0.0 Reference 0 Delta
    // M0.1 Reference 1 Delta
    // M0.2 Source X/Y
    VME_SET_UNIInput_SrcX(uniIn, x);
    VME_SET_UNIInput_SrcY(uniIn, y);
    // M0.3
    VME_COPY_DWORD(uniIn, 0, 3, CURBEData, 3);
    // M0.4 reserved
    // M0.5
    VME_COPY_DWORD(uniIn, 0, 5, CURBEData, 5);
    SetRef(uniIn,
           uniIn.row(0).format<int2>().select<2, 1> (0),
           AveL0,
           uniIn.row(0).format<int1>().select<2, 1> (22),
           CURBEData.format<int1>().select<2, 1> (17),
           CURBEData.format<int2>()[68]);
    SetRef(uniIn,
           uniIn.row(0).format<int2>().select<2, 1> (2),
           AveL1,
           uniIn.row(0).format<int1>().select<2, 1> (22),
           CURBEData.format<int1>().select<2, 1> (17),
           CURBEData.format<int2>()[68]);
    // M0.6 debug
    // M0.7 debug

    // register M1
    // M1.0/1.1/1.2 Search path parameters & start centers
    uniIn.row(1).format<uint4> ().select<3, 1> (0) = CURBEData.format<uint4> ().select<3, 1> (0);
    {
        vector<uint1, 2> Start0;

        Start0 = uniIn.row(0).format<int1>().select<2, 1> (22);
        Start0 = ((Start0 - 16) >> 3) & 0x0f;
        uniIn.row(1)[10] = Start0[0] | (Start0[1] << 4);
        uniIn.row(1)[11] = uniIn.row(1)[10];
    }
    // M1.3 Weighted SAD (not used for HSW)
    // M1.4 Cost center 0 for HSW; MBZ for BDW
    // M1.5 Cost center 1 for HSW; MBZ for BDW
    // M1.6 Fwd/Bwd Block RefID (used in B slices only)
    // M1.7 various prediction parameters
    VME_COPY_DWORD(uniIn, 1, 7, CURBEData, 7);
    VME_CLEAR_UNIInput_IntraCornerSwap(uniIn);
    VME_SET_NEIGHBOUR_AVAILABILITY_INTER(uniIn, neighborMbMask);
    // update 'skip center enables' variable
    {
        vector<uint1, 1> skipCenterEnables, mask;
        skipCenterEnables = 0;
        mask = (0 <= B_DirectMVRefIdx[0]);
        skipCenterEnables.merge(0x55, mask);
        uniIn.row(1)[31] = skipCenterEnables[0];
        skipCenterEnables = 0;
        mask = (0 <= B_DirectMVRefIdx[1]);
        skipCenterEnables.merge(0xaa, mask);
        uniIn.row(1)[31] |= skipCenterEnables[0];
    }

    // register M2
    // M2.0/2.1/2.2/2.3/2.4
    // M2.5 FBR parameters
    // M2.6/2.7 SIC Forward Transform Coeff Threshold Matrix
    uniIn.row(2).format<uint4> ().select<2, 1> (6) = CURBEData.format<uint4> ().select<2, 1> (14);

    // BDW+
    // register M3
    // M3.0 FWD Cost center 0 Delta X and Y
    VME_COPY_DWORD(uniIn, 3, 0, AveL0, 0);
    // M3.1 BWD Cost center 0 Delta X and Y
    VME_COPY_DWORD(uniIn, 3, 1, AveL1, 0);
    // M3.2 FWD Cost center 1 Delta X and Y
    // M3.3 BWD Cost center 1 Delta X and Y
    // M3.4 FWD Cost center 2 Delta X and Y
    // M3.5 BWD Cost center 2 Delta X and Y
    // M3.6 FWD Cost center 3 Delta X and Y
    // M3.7 BWD Cost center 3 Delta X and Y

    //
    // initialize SIC input
    //

    // register M0
    // 0.0 Ref0 Skip Center 0 Delta XY
    VME_COPY_DWORD(sicIn, 0, 0, B_DirectMV.row(0), 0);
    // 0.1 Ref1 Skip Center 0 Delta XY
    VME_COPY_DWORD(sicIn, 0, 1, B_DirectMV.row(1), 0);
/*
    // 0.2 Ref0 Skip Center 1 Delta XY
    VME_COPY_DWORD(sicIn, 0, 2, B_DirectMV.row(0), 5);
    // 0.3 Ref1 Skip Center 1 Delta XY
    VME_COPY_DWORD(sicIn, 0, 3, B_DirectMV.row(1), 5);
    // 0.4 Ref0 Skip Center 2 Delta XY
    VME_COPY_DWORD(sicIn, 0, 4, B_DirectMV.row(0), 10);
    // 0.5 Ref1 Skip Center 2 Delta XY
    VME_COPY_DWORD(sicIn, 0, 5, B_DirectMV.row(1), 10);
    // 0.6 Ref0 Skip Center 3 Delta XY
    VME_COPY_DWORD(sicIn, 0, 6, B_DirectMV.row(0), 15);
    // 0.7 Ref1 Skip Center 3 Delta XY
    VME_COPY_DWORD(sicIn, 0, 7, B_DirectMV.row(1), 15);
*/

    // register M1
    // M1.0/1.1 ACV Intra 4x4/8x8 mode mask & intra modes
    sicIn.row(1).format<uint4> ().select<2, 1> (0) = CURBEData.format<uint4> ().select<2, 1> (30);
    VME_SET_CornerNeighborPixel0(sicIn, topBlocksValues(0, 3));

    // M1.2/1.3/1.4/1.5/1.6/1.7 Neighbor pixel Luma values
    sicIn.row(1).format<uint4> ().select<6, 1> (2) = topBlocksValues.format<uint4> ().select<6, 1> (1);

    // register M2
    // M2.0/2.1/2.2/2.3 Neighbor pixel luma values
    sicIn.row(2).format<uint4> ().select<4, 1> (0) = leftBlockValues.format<uint4> ().select<4, 1> (0);

    // M2.4 Intra Predictor Mode
    VME_SET_LeftMbIntraModes(sicIn, leftMbIntraModes);
    VME_SET_UpperMbIntraModes(sicIn, upperMbIntraModes);
    // M2.5 Corner pixel chroma value
    VME_SET_CornerNeighborPixelChroma(sicIn, (chromaTopBlocksValues.format<uint2>().select<1, 1>(1)));
    // M2.6 reserved
    // M2.7 Penalties for non-DC modes
    VME_COPY_DWORD(sicIn, 2, 7, CURBEData, 33);

    // register M3
    // M3.0/3.1/3.2/3.3 Neighbor pixel chroma values
    sicIn.row(3).format<uint4> ().select<4, 1> (0) = chromaLeftBlockValues.format<uint4> ().select<4, 1> (0);
    // M3.4/3.5/3.6/3.7 Neighbor pixel chroma values
    sicIn.row(3).format<uint4> ().select<4, 1> (4) = chromaTopBlocksValues.format<uint4> ().select<4, 1> (1);

    // set costs
    Costs.row(0).format<int2> ().select<2, 1> (0) = AveL0;
    Costs.row(0).format<int2> ().select<2, 1> (2) = AveL1;

} // void SetUpVmeBSlice(matrix_ref<uchar, 4, 32> uniIn,

_GENX_ inline
void SetUpPakDataBSlice(matrix_ref<uchar, 4, 32> uniIn,
                        matrix_ref<uchar, 7, 32> uniOut,
                        vector_ref<uchar, MBDATA_SIZE> MBData,
                        U8 direct8x8pattern,
                        vector_ref<uchar, CURBEDATA_SIZE> CURBEData,
                        U32 x,
                        U32 y,
                        uint MbIndex)
{
    vector<uint4, 3> intraPredParam, interPredParam = 0;
    vector<short, 4> mask, intraMask;
    vector<short, 1> transformMask;
    uint2 distInter, distIntra, distSkip;
    vector<uint1, 4> intraMbType;
    uint2 in_uniOut_MbSubPredMode;
    vector<uint1, 4> MbSubPredMode;
    uint1 MbMode;
    uint1 intraMbFlag;

    VME_INIT_SVC_PACK(MBData);

    // set the mask to select INTRA data
    VME_GET_UNIOutput_BestInterDistortion(uniOut, distInter);
    VME_GET_UNIOutput_BestIntraDistortion(uniOut, distIntra);
    VME_GET_UNIOutput_SkipRawDistortion(uniOut, distSkip);
    intraMbFlag = (distInter > distIntra);
    intraMask = intraMbFlag;

    // DW0 MB flags and modes
    VME_COPY_MB_INTRA_MODE_TYPE(intraMbType, 0, uniOut);
    VME_COPY_MB_INTER_MODE_TYPE(MBData, 0, uniOut);
    MBData.select<4, 1> (0).merge(intraMbType, intraMask);
    MBData.select<1, 1> (0).merge(MBData[0] | 4, distSkip != 65535);

    // DW1-3 intra luma modes & intra macroblock struct
    intraPredParam = uniOut.row(0).format<uint4>().select<3, 1> (4);
    intraPredParam[2] = intraPredParam[2] & 0x0ff;

    // DW1-3 inter luma modes & inter macroblock struct
    interPredParam.format<uint1>().select<2, 1> (0) = uniOut.row(0).select<2, 1> (25);
    interPredParam[1] = uniOut.row(6).format<uint4>()[0] & 0x0f0f0f0f;
    interPredParam[2] = (uniOut.row(6).format<uint4>()[0] >> 4) & 0x0f0f0f0f;

    // correct reference indices
    VME_GET_UNIOutput_SubMbPredMode(uniOut, in_uniOut_MbSubPredMode);
    VME_GET_UNIOutput_InterMbMode(uniOut, MbMode);
    MbSubPredMode[0] = in_uniOut_MbSubPredMode & 3;
    MbSubPredMode[1] = (in_uniOut_MbSubPredMode >> 2) & 3;
    MbSubPredMode[2] = (in_uniOut_MbSubPredMode >> 4) & 3;
    MbSubPredMode[3] = (in_uniOut_MbSubPredMode >> 6) & 3;
    mask = (0 == MbMode);
    MbSubPredMode.select<3, 1> (1).merge(MbSubPredMode[0], mask.select<3, 1> (0));
    mask = (1 == MbMode);
    MbSubPredMode.select<2, 1> (2).merge(MbSubPredMode[1], mask.select<2, 1> (0));
    MbSubPredMode.select<2, 1> (0).merge(MbSubPredMode[0], mask.select<2, 1> (0));
    mask = (2 == MbMode);
    MbSubPredMode.select<2, 1> (2).merge(MbSubPredMode.select<2, 1> (0), mask.select<2, 1> (0));
    mask = (0 != (MbSubPredMode & 1));
    interPredParam.format<uint1> ().select<4, 1> (4).merge(uchar(-1), mask);
    mask = (0 == (MbSubPredMode & 3));
    interPredParam.format<uint1> ().select<4, 1> (8).merge(uchar(-1), mask);

    // select correct macroblock parameters
    MBData.format<uint4>().select<3, 1> (1) = interPredParam;
    MBData.format<uint4>().select<3, 1> (1).merge(intraPredParam, intraMask.select<3, 1> (0));

    // DW4 costs
    MBData.format<uint2>()[8] = distIntra;
    MBData.format<uint2>()[9] = distInter;

    // DW6-7 sum luma coeffs
    MBData.format<uint2>().select<4, 1>(12) = uniOut.row(6).format<uint2>().select<4, 1>(4);

    // DW8 num nz luma coeffs
    MBData.format<uint1>().select<4, 1>(32) = uniOut.row(6).format<uint1>().select<4, 1>(4);

    // DW9-10 cost centers like in HSW (to support multiref in BDW one needs to extend MBData)
    MBData.format<uint2>().select<1, 1>(18) = VME_GET_UNIInput_FWDCostCenter0_X(uniIn);
    MBData.format<uint2>().select<1, 1>(19) = VME_GET_UNIInput_FWDCostCenter0_Y(uniIn);
    MBData.format<uint2>().select<1, 1>(20) = VME_GET_UNIInput_BWDCostCenter0_X(uniIn);
    MBData.format<uint2>().select<1, 1>(21) = VME_GET_UNIInput_BWDCostCenter0_Y(uniIn);

    // copy motion vectors
    SET_MBDATA_MV(MBData, LIST_0, uniOut.row(1).format<uint4>()[0]);
    SET_MBDATA_MV(MBData, LIST_1, uniOut.row(1).format<uint4>()[1]);

} // void SetUpPakDataBSlice(matrix_ref<uchar, 4, 32> uniIn,

extern "C" _GENX_MAIN_  void
SVCEncMB_B(SurfaceIndex CurbeDataSurfIndex,
           SurfaceIndex SrcSurfIndexRaw,
           SurfaceIndex SrcSurfIndex,
           SurfaceIndex VMEInterPredictionSurfIndex,
           SurfaceIndex MBDataSurfIndex,
           SurfaceIndex FwdFrmMBDataSurfIndex)
{
//    cm_wait();
    uint mbX = get_thread_origin_x();
    uint mbY = get_thread_origin_y();
    uint x = mbX * 16;
    uint y = mbY * 16;

    int offset = 0;
    uint PicWidthInMB, PicHeightInMBs;       //picture width/height in MB unit

    vector<uchar, CURBEDATA_SIZE> CURBEData;
    read(CurbeDataSurfIndex,0,CURBEData.select<128,1>());
    read(CurbeDataSurfIndex,128,CURBEData.select<32,1>(128));
    // read picture's width in MB units
    PicWidthInMB    = GET_CURBE_PictureWidth(CURBEData);
    PicHeightInMBs  = GET_CURBE_PictureHeight(CURBEData);

    uint MbIndex = PicWidthInMB * mbY + mbX;

    vector<uint1, 1> direct8x8pattern = 0;
    matrix<uchar, 4, 32> uniIn = 0;
    matrix<uchar, 7, 32> best_uniOut = 0;

    {
        // declare parameters for VME
        matrix<uchar, 6, 32> imeIn = 0;
        matrix<uchar, 4, 32> sicIn = 0;
        matrix<uchar, 7, 32> sic_uniOut = 0;
        matrix<uint4, 16, 2> Costs = 0;
        matrix<int2, 2, 2> mvPred = 0;
        vector<int1, 2> InitRefIdx = -1;

        vector<int2, 2> ref0;
        U8 ftqSkip = 0;

        // down scale
        uint LaScaleFactor = GET_CURBE_CurLayerDQId(CURBEData);
        if (LaScaleFactor == 2)
        {
            DownSampleMB2Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
        } else if (LaScaleFactor == 4)
        {
            DownSampleMB4Xf(SrcSurfIndexRaw, SrcSurfIndex, mbX, mbY);
        }
        SetUpVmeBSlice(uniIn, sicIn, Costs,
                       SrcSurfIndex, MBDataSurfIndex,
                       FwdFrmMBDataSurfIndex,
                       CURBEData,
                       x, y, MbIndex,
                       mvPred,
                       InitRefIdx);
        ftqSkip = VME_GET_UNIInput_FTEnable(uniIn);
//        fprintf(stderr,"ftq:%d\n", ftqSkip);

        VME_SET_UNIOutput_BestIntraDistortion(best_uniOut, -1);
        VME_SET_UNIOutput_InterDistortion(best_uniOut, -1);
        VME_SET_UNIOutput_SkipRawDistortion(best_uniOut, -1);

        {
            LoadCosts(uniIn, CURBEData);
            LoadSearchPath(imeIn, CURBEData);

            // call intra/skip motion estimation
            run_vme_sic(uniIn, sicIn, VMEInterPredictionSurfIndex, sic_uniOut);

            vector<uint2, 1> Coeffs = cm_sum<uint2>(sic_uniOut.row(6).format<uint1>().select<4, 1> (4));
            vector<uint2, 1> sic_uniOut_DistSkip;
            vector<uint2, 1> sic_uniOut_DistInter;
            uint SkipModeEn = VME_GET_UNIInput_SkipModeEn(uniIn);
            uint1 ifCondition;

            // get acceptable skip value
            uint2 SkipVal = GET_CURBE_SkipVal(CURBEData);

            VME_GET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip[0]);
            _VME_GET_UNIOutput_InterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            Coeffs.merge(ushort(-1), SkipModeEn - 1);
            sic_uniOut_DistSkip.merge(ushort(-1), SkipModeEn - 1);
            sic_uniOut_DistInter.merge(ushort(-1), SkipModeEn - 1);

            ifCondition = (ftqSkip && Coeffs[0]) ||
                          (!ftqSkip && sic_uniOut_DistSkip[0] > SkipVal);

            sic_uniOut_DistSkip.merge(ushort(-1), ifCondition);

            VME_SET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip[0]);
            VME_SET_UNIOutput_InterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            if ((!VME_GET_UNIInput_SkipModeEn(uniIn)) ||
                ifCondition)
            {
                // Calculate AVC INTER prediction costs
                DoInterFramePrediction<BPREDSLICE> (VMEInterPredictionSurfIndex, uniIn, imeIn,
                                                    Costs, best_uniOut, direct8x8pattern, CURBEData);
            }

            VME_GET_UNIOutput_BestInterDistortion(sic_uniOut, sic_uniOut_DistInter[0]);

            uint2 sic_uniOut_DistIntra;
            VME_GET_UNIOutput_BestIntraDistortion(sic_uniOut, sic_uniOut_DistIntra);
            VME_SET_UNIOutput_BestIntraDistortion(best_uniOut, sic_uniOut_DistIntra);

            VME_GET_UNIOutput_SkipRawDistortion(sic_uniOut, sic_uniOut_DistSkip);
            VME_SET_UNIOutput_SkipRawDistortion(best_uniOut, sic_uniOut_DistSkip);

            uint2 best_uniOut_DistInter;
            VME_GET_UNIOutput_BestInterDistortion(best_uniOut, best_uniOut_DistInter);

            if ((sic_uniOut_DistIntra < best_uniOut_DistInter) ||
                (!ftqSkip && sic_uniOut_DistSkip[0] < SkipVal) ||
                (ftqSkip && !Coeffs[0]) ||
                (sic_uniOut_DistInter[0] < best_uniOut_DistInter))
            {
                if (ftqSkip && !Coeffs[0])
                {
                    VME_SET_UNIOutput_SkipRawDistortion(sic_uniOut, 0);
                }
                direct8x8pattern = 0xf;
//  sergo: test!!!
#if 0
                best_uniOut = sic_uniOut;
#else
                best_uniOut.row(0) = sic_uniOut.row(0);
                best_uniOut.row(5) = sic_uniOut.row(5);
                best_uniOut.row(6) = sic_uniOut.row(6);
#endif
            }
            else
            {
                vector<uint1, 4> lumanzc      = sic_uniOut.row(6).format<uint1>().select<4, 1>(4);
                vector<uint1, 2> chromanzc    = sic_uniOut.row(6).format<uint1>().select<2, 1>(16);
                vector<uint2, 4> lumacoeffs   = sic_uniOut.row(6).format<uint2>().select<4, 1>(4);
                vector<uint2, 2> chromacoeffs = sic_uniOut.row(6).format<uint2>().select<2, 1>(10);
                best_uniOut.row(6).format<uint1>().select<4, 1>(4)  = lumanzc;
                best_uniOut.row(6).format<uint1>().select<2, 1>(16) = chromanzc;
                best_uniOut.row(6).format<uint2>().select<4, 1>(4)  = lumacoeffs;
                best_uniOut.row(6).format<uint2>().select<2, 1>(10) = chromacoeffs;
            }
        }
    }

    //
    // write the result
    //
    vector<uchar, MBDATA_SIZE> MBData;

    // pack the VME result
    SetUpPakDataBSlice(uniIn, best_uniOut, MBData, direct8x8pattern[0],
                       CURBEData, x, y, MbIndex);

    // write back updated MB data
    offset = MbIndex * MBDATA_SIZE;
    write(MBDataSurfIndex, offset, MBData);

    cm_fence();
}

/****************************************************************************************/

#endif // #if defined(_WIN32) || defined(_WIN64)
