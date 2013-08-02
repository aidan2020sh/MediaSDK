//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_optimization.h"

#if defined (MFX_TARGET_OPTIMIZATION_PX)

#include "ippvc.h"
#include "mfx_h265_defs.h"

namespace MFX_HEVC_ENCODER
{

    //---------------------------------------------------------
    // aya: reference SAD. not used. IPP instead
    //---------------------------------------------------------
    IppStatus __STDCALL h265_SAD_reference_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s width,
        Ipp32s height)
    {
        for (int j = 0; j < height; j++)
        {
            for (int i = 0; i < width; i++)
            {
                Ipp32s var = pSrcCur[i + j*srcCurStep] - pSrcRef[i + j*srcRefStep];
                *pDst += abs(var);
                //cost += (var*var >> 2);
            }
        }

        return ippStsNoErr;

    } // IppStatus __STDCALL h265_SAD_reference_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD4x16_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 4, 16);

        Ipp32s sad[2] = {0};
        ippiSAD4x8_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = srcCurStep*8;
        Ipp32s offset2 = srcRefStep*8;
        ippiSAD4x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD4x16_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD8x32_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 8, 32);

        Ipp32s sad[2] = {0};
        ippiSAD8x16_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = srcCurStep*16;
        Ipp32s offset2 = srcRefStep*16;
        ippiSAD8x16_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD8x32_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD12x16_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 12, 16);

        Ipp32s sad[3] = {0};
        ippiSAD8x16_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = 8;
        Ipp32s offset2 = 8;
        ippiSAD4x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        offset1 = srcCurStep*8 + 8;
        offset2 = srcRefStep*8 + 8;
        ippiSAD4x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 2, 0);

        *pDst = sad[0] + sad[1] + sad[2];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD12x16_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD16x4_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 16, 4);

        Ipp32s sad[2] = {0};
        ippiSAD8x4_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = 8;
        Ipp32s offset2 = 8;
        ippiSAD8x4_8u32s_C1R(pSrcCur+offset1, srcCurStep, pSrcRef+offset2, srcRefStep, sad + 1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD16x4_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD16x12_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 16, 12);

        Ipp32s sad[3] = {0};
        ippiSAD16x8_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = srcCurStep*8;
        Ipp32s offset2 = srcRefStep*8;
        ippiSAD8x4_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        offset1 += 8;
        offset2 += 8;
        ippiSAD8x4_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 2, 0);

        *pDst = sad[0] + sad[1] + sad[2];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD16x12_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD16x32_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 16, 32);

        Ipp32s sad[2] = {0};
        ippiSAD16x16_8u32s(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);

        Ipp32s offset1 = srcCurStep*16;
        Ipp32s offset2 = srcRefStep*16;
        ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad+1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD16x32_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD16x64_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 16, 64);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 4; j++ )
        {
            //for( int i = 0; i < 2; i++ )
            {
                offset1 = srcCurStep*16*j;
                offset2 = srcRefStep*16*j;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }


        return ippStsNoErr;

    } // void __STDCALL h265_SAD16x64_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD24x32_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 24, 32);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad[4] = {0};

        ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 0, 0);

        offset1 = srcCurStep*16;
        offset2 = srcRefStep*16;
        ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        offset1 = 16;
        offset2 = 16;
        ippiSAD8x16_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 2, 0);

        offset1 = srcCurStep*16 + 16;
        offset2 = srcRefStep*16 + 16;
        ippiSAD8x16_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 3, 0);

        *pDst = sad[0] + sad[1] + sad[2] + sad[3];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD24x32_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD32x8_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 32, 8);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad[2] = {0};

        ippiSAD16x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 0, 0);

        offset1 = 16;
        offset2 = 16;
        ippiSAD16x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD32x8_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD32x16_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 32, 16);

        Ipp32s sad[2] = {0};
        ippiSAD16x16_8u32s(pSrcCur, srcCurStep, pSrcRef, srcRefStep, sad, 0);
        ippiSAD16x16_8u32s(pSrcCur + 16, srcCurStep, pSrcRef + 16, srcRefStep, sad+1, 0);

        *pDst = sad[0] + sad[1];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD32x16_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD32x24_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 32, 24);

        Ipp32s sad[4] = {0};
        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;

        ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 0, 0);

        offset1 = 16;
        offset2 = 16;
        ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 1, 0);

        offset1 = srcCurStep*16;
        offset2 = srcRefStep*16;
        ippiSAD16x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 2, 0);

        offset1 += 16;
        offset2 += 16;
        ippiSAD16x8_8u32s_C1R(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, sad + 3, 0);

        *pDst = sad[0] + sad[1] + sad[2] + sad[3];

        return ippStsNoErr;

    } // void __STDCALL h265_SAD32x24_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD32x32_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 32, 32);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 2; j++ )
        {
            for( int i = 0; i < 2; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD32x32_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD32x64_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 32, 64);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        for( int j = 0; j < 4; j++ )
        {
            for( int i = 0; i < 2; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD32x64_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD48x64_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 48, 64);
        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 4; j++ )
        {
            for( int i = 0; i < 3; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD48x64_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD64x16_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 64, 16);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int i = 0; i < 4; i++ )
        {
            //for( int i = 0; i < 3; i++ )
            {
                offset1 = 16*i;
                offset2 = 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD64x16_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD64x32_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 64, 32);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 2; j++ )
        {
            for( int i = 0; i < 4; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD64x32_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD64x48_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 64, 48);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 3; j++ )
        {
            for( int i = 0; i < 4; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD64x48_8u32s_C1R(...)


    IppStatus __STDCALL h265_SAD64x64_8u32s_C1R(
        const Ipp8u* pSrcCur,
        int srcCurStep,
        const Ipp8u* pSrcRef,
        int srcRefStep,
        Ipp32s* pDst,
        Ipp32s mc)
    {
        H265ENC_UNREFERENCED_PARAMETER(mc);

        //h265_SAD_reference_8u32s_C1R(pSrcCur, srcCurStep, pSrcRef, srcRefStep, pDst, 64, 64);

        Ipp32s offset1 = 0;
        Ipp32s offset2 = 0;
        Ipp32s sad = 0;
        *pDst = 0;
        for( int j = 0; j < 4; j++ )
        {
            for( int i = 0; i < 4; i++ )
            {
                offset1 = srcCurStep*16*j + 16*i;
                offset2 = srcRefStep*16*j + 16*i;
                sad = 0;
                ippiSAD16x16_8u32s(pSrcCur + offset1, srcCurStep, pSrcRef + offset2, srcRefStep, &sad, 0);
                *pDst += sad;
            }
        }

        return ippStsNoErr;

    } // void __STDCALL h265_SAD64x64_8u32s_C1R(...)


    /*typedef IppStatus (__STDCALL *SADfunc8u)(
        const Ipp8u* pSrcCur,
        Ipp32s srcCurStep,
        const Ipp8u* pSrcRef,
        Ipp32s srcRefStep,
        Ipp32s* pDst,
        Ipp32s mcType);*/

    //                         [0,  Mx4,  Mx8,  Mx12,  Mx16,  Mx20, Mx24, Mx28, Mx32,  Mx36, Mx40, Mx44, Mx48, Mx52, Mx56, Mx60, Mx64]
    //SADfunc8u SAD_4xN_8u[]  = {NULL, NULL, (SADfunc8u)ippiSAD4x8_8u32s_C1R, NULL, (SADfunc8u)h265_SAD4x16_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    //SADfunc8u SAD_8xN_8u[]  = {NULL, (SADfunc8u)ippiSAD8x4_8u32s_C1R, (SADfunc8u)ippiSAD8x8_8u32s_C1R, NULL, (SADfunc8u)ippiSAD8x16_8u32s_C1R, NULL, NULL, NULL, (SADfunc8u)h265_SAD8x32_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    //SADfunc8u SAD_12xN_8u[] = {NULL, NULL, NULL, NULL, (SADfunc8u)h265_SAD12x16_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    //SADfunc8u SAD_16xN_8u[] = {NULL, (SADfunc8u)h265_SAD16x4_8u32s_C1R, (SADfunc8u)ippiSAD16x8_8u32s_C1R, (SADfunc8u)h265_SAD16x12_8u32s_C1R, (SADfunc8u)ippiSAD16x16_8u32s, NULL, NULL, NULL, (SADfunc8u)h265_SAD16x32_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (SADfunc8u)h265_SAD16x64_8u32s_C1R};
    //SADfunc8u SAD_24xN_8u[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,    (SADfunc8u)h265_SAD24x32_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    //SADfunc8u SAD_32xN_8u[] = {NULL, NULL, (SADfunc8u)h265_SAD32x8_8u32s_C1R, NULL, (SADfunc8u)h265_SAD32x16_8u32s_C1R, NULL, (SADfunc8u)h265_SAD32x24_8u32s_C1R, NULL,  (SADfunc8u)h265_SAD32x32_8u32s_C1R, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (SADfunc8u)h265_SAD32x64_8u32s_C1R};
    //SADfunc8u SAD_48xN_8u[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, (SADfunc8u)h265_SAD48x64_8u32s_C1R};
    //SADfunc8u SAD_64xN_8u[] = {NULL, NULL, NULL, NULL, (SADfunc8u)h265_SAD64x16_8u32s_C1R, NULL, NULL, NULL, (SADfunc8u)h265_SAD64x32_8u32s_C1R, NULL, NULL, NULL, (SADfunc8u)h265_SAD64x48_8u32s_C1R, NULL, NULL, NULL, (SADfunc8u)h265_SAD64x64_8u32s_C1R};

    ////                       [0,  4xN,  8xN,  12xN,  16xN,  20xN, 24xN, 28xN, 32xN,  36xN, 40xN, 44xN, 48xN, 52xN, 56xN, 60xN, 64xN]
    //SADfunc8u* SAD_8u[17] = {NULL, SAD_4xN_8u, SAD_8xN_8u, SAD_12xN_8u, SAD_16xN_8u, NULL, SAD_24xN_8u, NULL, SAD_32xN_8u, NULL, NULL, NULL, SAD_48xN_8u, NULL, NULL, NULL, SAD_64xN_8u};

    //IppStatus ippiSAD_General_8u32s_C1R(
    //    const Ipp8u* pSrc,
    //    Ipp32s srcStep,
    //    const Ipp8u* pRef,
    //    Ipp32s refStep,
    //    Ipp32s width,
    //    Ipp32s height,
    //    Ipp32s* pSAD)
    //{
    //    IppStatus sts = ippStsNoErr;

    //    SADfunc8u p_func = SAD_8u[width >> 2][height >> 2];
    //    
    //    p_func(pSrc, srcStep, pRef, refStep, pSAD, 0);//mc_type???

    //    return sts;

    //} // IppStatus ippiSAD_General_8u32s_C1R(...)

    /*IppStatus h265_SAD_MxN_special_IPP_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY, int* sad)
    {
        return SAD_8u[SizeX >> 2][SizeY >> 2](image, stride, ref, 64, sad, 0);
    }*/

    int h265_SAD_MxN_special_8u(const unsigned char *image,  const unsigned char *ref, int stride, int SizeX, int SizeY)
    {
        return h265_SAD_MxN_general_8u(image, stride, ref, 64, SizeX, SizeY);
    }

    int h265_SAD_MxN_general_8u(const unsigned char *image,  int stride_img, const unsigned char *ref, int stride_ref, int SizeX, int SizeY)
    {
        int sad = 0;

        if (SizeX == 4)
        {
            if(SizeY == 4) { 
                ; 
            }
            if(SizeY == 8) { ippiSAD4x8_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else           { h265_SAD4x16_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
        }
        else if (SizeX == 8)
        {
            if(SizeY ==  4) { ippiSAD8x4_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY ==  8) { ippiSAD8x8_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 16) { ippiSAD8x16_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else            { h265_SAD8x32_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
        }
        else if (SizeX == 12)
        {
           h265_SAD12x16_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);
        }
        else if (SizeX == 16)
        {
            if(SizeY ==  4) { h265_SAD16x4_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY ==  8) { ippiSAD16x8_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 12) { h265_SAD16x12_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 16) { ippiSAD16x16_8u32s(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 32) { h265_SAD16x32_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else            { h265_SAD16x64_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
        }
        else if (SizeX == 24)
        {
            h265_SAD24x32_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);
        }
        else if (SizeX == 32)
        {
            if(SizeY ==  8) { h265_SAD32x8_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
            else if(SizeY == 16) { h265_SAD32x16_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 24) { h265_SAD32x24_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0); }
            else if(SizeY == 32) { h265_SAD32x32_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
            else            { h265_SAD32x64_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
        }
        else if (SizeX == 48)
        {
            h265_SAD48x64_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);
        }
        else if (SizeX == 64)
        {
            if(SizeY == 16) { h265_SAD64x16_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
            else if(SizeY == 32) { h265_SAD64x32_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
            else if(SizeY == 48) { h265_SAD64x48_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
            else            { h265_SAD64x64_8u32s_C1R(image, stride_img, ref, stride_ref, &sad, 0);}
        }
        

        return sad;
    }

} // end namespace MFX_HEVC_ENCODER

#endif // #if defined (MFX_TARGET_OPTIMIZATION_SSE4) || defined(MFX_TARGET_OPTIMIZATION_AVX2)
#endif // MFX_ENABLE_H265_VIDEO_ENCODE
/* EOF */
