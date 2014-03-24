/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H265_VIDEO_DECODER)

#include "umc_h265_dec_ipplevel.h"
#include "ippvc.h"
#include "ipps.h"
#include <memory.h>
#include <algorithm>

/*
THIS FILE IS A TEMPORAL SOLUTION AND IT WILL BE REMOVED AS SOON AS THE NEW FUNCTIONS ARE ADDED
*/
namespace UMC_HEVC_DECODER
{


#define ABS(a)          (((a) < 0) ? (-(a)) : (a))
#define max(a, b)       (((a) > (b)) ? (a) : (b))
#define min(a, b)       (((a) < (b)) ? (a) : (b))
#define ClampVal(x)  (ClampTbl[256 + (x)])
#define clipd1(x, limit) min(limit, max(x,-limit))

#define ClampTblLookup(x, y) ClampVal((x) + clipd1((y),256))
#define ClampTblLookupNew(x) ClampVal((x))

#define _IPP_ARCH_IA32    1
#define _IPP_ARCH_IA64    2
#define _IPP_ARCH_EM64T   4
#define _IPP_ARCH_XSC     8
#define _IPP_ARCH_LRB     16
#define _IPP_ARCH_LP32    32
#define _IPP_ARCH_LP64    64

#define _IPP_ARCH    _IPP_ARCH_IA32

#define _IPP_PX 0
#define _IPP_M6 1
#define _IPP_A6 2
#define _IPP_W7 4
#define _IPP_T7 8
#define _IPP_V8 16
#define _IPP_P8 32
#define _IPP_G9 64

#define _IPP _IPP_PX

#define _IPP32E_PX _IPP_PX
#define _IPP32E_M7 32
#define _IPP32E_U8 64
#define _IPP32E_Y8 128
#define _IPP32E_E9 256

#define _IPP32E _IPP32E_PX

#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#define IPP_BADARG_RET( expr, ErrCode )\
            {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#define IPP_BAD_SIZE_RET( n )\
            IPP_BADARG_RET( (n)<=0, ippStsSizeErr )

#define IPP_BAD_STEP_RET( n )\
            IPP_BADARG_RET( (n)<=0, ippStsStepErr )

#define IPP_BAD_PTR1_RET( ptr )\
            IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

#define IPP_BAD_PTR2_RET( ptr1, ptr2 )\
            IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))), ippStsNullPtrErr)

#define IPP_BAD_PTR3_RET( ptr1, ptr2, ptr3 )\
            IPP_BADARG_RET(((NULL==(ptr1))||(NULL==(ptr2))||(NULL==(ptr3))),\
                                                     ippStsNullPtrErr)

#define IPP_BAD_PTR4_RET( ptr1, ptr2, ptr3, ptr4 )\
                {IPP_BAD_PTR2_RET( ptr1, ptr2 ); IPP_BAD_PTR2_RET( ptr3, ptr4 )}

#define IPP_BAD_RANGE_RET( val, low, high )\
     IPP_BADARG_RET( ((val)<(low) || (val)>(high)), ippStsOutOfRangeErr)

#define __ALIGN16 __declspec (align(16))


/* Define NULL pointer value */
#ifndef NULL
#ifdef  __cplusplus
#define NULL    0
#else
#define NULL    ((void *)0)
#endif
#endif

#define read_data_through_boundary_table_8u read_data_through_boundary_table_8u_pxmx
typedef void (*pH264Interpolation_8u) (H265InterpolationParams_8u *pParams);
extern pH264Interpolation_8u read_data_through_boundary_table_8u_pxmx[16];

#define read_data_through_boundary_table_16u read_data_through_boundary_table_16u_pxmx
typedef void (*pH264Interpolation_16u) (H265InterpolationParams_16u *pParams);
extern pH264Interpolation_16u read_data_through_boundary_table_16u_pxmx[16];

#define read_data_through_boundary_table_nv12_8u read_data_through_boundary_table_nv12_8u_pxmx
extern pH264Interpolation_8u read_data_through_boundary_table_nv12_8u_pxmx[16];

#define read_data_through_boundary_table_nv12_16u read_data_through_boundary_table_nv12_16u_pxmx
extern pH264Interpolation_16u read_data_through_boundary_table_nv12_16u_pxmx[16];

template<typename PixType, Ipp32s chromaMult>
void memset_with_mult(PixType *pDst, const PixType* nVal, Ipp32s nNum)
{
    if (chromaMult == 2)
    {
        for (Ipp32s i = 0; i < nNum; i ++ )
        {
            pDst[2*i] = nVal[0];
            pDst[2*i + 1] = nVal[1];
        }
    }
    else
    {
        for (Ipp32s i = 0; i < nNum; i++)
        {
            pDst[i] = nVal[0];
        }
    }
}

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateBoundaryLumaBlock_H264(Ipp32s iOverlappingType, InterpolationStruct *pParams, PixType *temporary_buffer)
{
    PixType *pTmp = temporary_buffer;
    Ipp32s tmpStep = 128; /* !!! pitch is fixed*/

    /* read data into temporal buffer */
    {
        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        if (sizeof(PixType) == 1)
        {
            read_data_through_boundary_table_8u[iOverlappingType]((H265InterpolationParams_8u*)pParams);
        }
        else
        {
            read_data_through_boundary_table_16u[iOverlappingType]((H265InterpolationParams_16u*)pParams);
        }

        pParams->pSrc = pTmp +
                        ((pParams->iType & 0x0c) ? (1) : (0)) * 3 * tmpStep +
                        ((pParams->iType & 0x03) ? (1) : (0)) * 3;
        pParams->srcStep = tmpStep;
    }

    return ippStsNoErr;
}

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateLumaBlock_H265Internal(InterpolationStruct *interpolateInfo, PixType *temporary_buffer)
{
    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        /* set the current source pointer */
        interpolateInfo->pSrc += interpolateInfo->pointBlockPos.y * interpolateInfo->srcStep + interpolateInfo->pointBlockPos.x;
        return ippStsNoOperation;
    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = interpolateInfo->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            hFraction = pointVector.x & 3;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                interpolateInfo->xPos = interpolateInfo->pointBlockPos.x + (pointVector.x >>= 2) - iAddition * 3;
                interpolateInfo->dataWidth = interpolateInfo->blockWidth + iAddition * 8;
            }

            /* prepare vertical vector values */
            vFraction = pointVector.y & 3;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                interpolateInfo->yPos = interpolateInfo->pointBlockPos.y + (pointVector.y >>= 2) - iAddition * 3;
                interpolateInfo->dataHeight = interpolateInfo->blockHeight + iAddition * 8;
            }

            interpolateInfo->iType = (vFraction << 2) | hFraction;
        }


        /*  call the appropriate function
            we select the function using the following rules:
            zero bit means x position of left data boundary lie out of frame,
            first bit means x position of right data boundary lie out of frame,
            second bit means y position of top data boundary lie out of frame,
            third bit means y position of bottom data boundary lie out of frame */
        iOverlappingType = 0;
        iOverlappingType |= (0 > interpolateInfo->xPos ? 1 : 0) << 0;
        iOverlappingType |= ((interpolateInfo->frameSize.width < interpolateInfo->xPos + interpolateInfo->dataWidth) ? 1 : 0) << 1;
        iOverlappingType |= (0 > interpolateInfo->yPos ? 1 : 0) << 2;
        iOverlappingType |= ((interpolateInfo->frameSize.height < interpolateInfo->yPos + interpolateInfo->dataHeight) ? 1 : 0) << 3;

        /* call appropriate function */
        if (0 == iOverlappingType)
        {
            /* set the current source pointer */
            interpolateInfo->pSrc += (interpolateInfo->pointBlockPos.y + pointVector.y) * interpolateInfo->srcStep + (interpolateInfo->pointBlockPos.x + pointVector.x);
            return ippStsNoOperation;
        }
        else
        {
            /* save additional parameters */
            /* there is something wrong, try to work through a temporal buffer */
            return ippiInterpolateBoundaryLumaBlock_H264<PixType, InterpolationStruct> (iOverlappingType, interpolateInfo, temporary_buffer);
        }
    }
}

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateBoundaryChromaBlock_NV12_H264(Ipp32s iOverlappingType, InterpolationStruct *pParams, PixType *temporary_buffer)
{
    PixType *pTmp = temporary_buffer;
    Ipp32s tmpStep = 128;

    /* read data into temporal buffer */
    {
        pParams->pDst = pTmp;
        pParams->dstStep = tmpStep;

        if (sizeof(PixType) == 1)
        {
            read_data_through_boundary_table_nv12_8u[iOverlappingType]((H265InterpolationParams_8u*)pParams);
        }
        else
        {
            read_data_through_boundary_table_nv12_16u[iOverlappingType]((H265InterpolationParams_16u*)pParams);
        }

        pParams->pSrc = pTmp +
                        ((pParams->iType & 0x02) ? (1) : (0)) * 1 * tmpStep +
                        ((pParams->iType & 0x01) ? (1) : (0)) * 2;
        pParams->srcStep = tmpStep;
    }

    return ippStsNoErr;
}

template<typename PixType, typename InterpolationStruct>
IppStatus ippiInterpolateChromaBlock_H264Internal(InterpolationStruct *interpolateInfo, PixType *temporary_buffer)
{
    /* the most probably case - just copying */
    if (0 == (interpolateInfo->pointVector.x | interpolateInfo->pointVector.y))
    {
        /* set the current source pointer */
        interpolateInfo->pSrc += interpolateInfo->pointBlockPos.y * interpolateInfo->srcStep + interpolateInfo->pointBlockPos.x*2;
    }
    /* prepare interpolation type and data dimensions */
    else
    {
        Ipp32s iOverlappingType;
        IppiPoint pointVector = interpolateInfo->pointVector;

        {
            Ipp32s hFraction, vFraction;

            /* prepare horizontal vector values */
            interpolateInfo->hFraction = hFraction = interpolateInfo->pointVector.x & 7;
            {
                Ipp32s iAddition;

                iAddition = (hFraction) ? (1) : (0);
                interpolateInfo->xPos = interpolateInfo->pointBlockPos.x + (pointVector.x >>= 3) - iAddition;
                interpolateInfo->dataWidth = interpolateInfo->blockWidth + iAddition*4;
            }

            /* prepare vertical vector values */
            interpolateInfo->vFraction = vFraction = pointVector.y & 7;
            {
                Ipp32s iAddition;

                iAddition = (vFraction) ? (1) : (0);
                interpolateInfo->yPos = interpolateInfo->pointBlockPos.y + (pointVector.y >>= 3) - iAddition;
                interpolateInfo->dataHeight = interpolateInfo->blockHeight + iAddition*4;
            }

            interpolateInfo->iType = ((vFraction ? 1 : 0) << 1) | (hFraction ? 1 : 0);
        }


        /*  call the appropriate function
            we select the function using the following rules:
            zero bit means x position of left data boundary lie out of frame,
            first bit means x position of right data boundary lie out of frame,
            second bit means y position of top data boundary lie out of frame,
            third bit means y position of bottom data boundary lie out of frame */
        iOverlappingType = 0;
        iOverlappingType |= ((0 > interpolateInfo->xPos) ? 1 : 0) << 0;
        iOverlappingType |= ((interpolateInfo->frameSize.width < interpolateInfo->xPos + interpolateInfo->dataWidth) ? 1 : 0) << 1;
        iOverlappingType |= ((0 > interpolateInfo->yPos) ? 1 : 0) << 2;
        iOverlappingType |= ((interpolateInfo->frameSize.height < interpolateInfo->yPos + interpolateInfo->dataHeight) ? 1 : 0) << 3;

        /* call appropriate function */
        if (0 == iOverlappingType)
        {
            size_t iRefOffset;

            /* set the current reference offset */
            iRefOffset = (interpolateInfo->pointBlockPos.y + pointVector.y) * interpolateInfo->srcStep + (interpolateInfo->pointBlockPos.x*2 + pointVector.x*2);

            /* set the current source pointer */
            interpolateInfo->pSrc += iRefOffset;
        }
        else
        {
            /* there is something wrong, try to work through a temporal buffer */
            return ippiInterpolateBoundaryChromaBlock_NV12_H264<PixType, InterpolationStruct>(iOverlappingType, interpolateInfo, temporary_buffer);
        }
    }

    return ippStsNoOperation;
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_none(InterpolationStruct *pParams)
{
    /* touch unreferenced parameter(s) */
    IPP_UNREFERENCED_PARAMETER(pParams);
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        for (i = 0; i < pParams->dataHeight; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], (pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_left_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;

        Ipp32s iIndentLeft = -pParams->xPos;
        Ipp32s iIndentRight = pParams->frameSize.width - pParams->xPos;

        for (Ipp32s i = 0; i < pParams->dataHeight; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
        MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
        memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], (pParams->dataWidth - iIndent));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_left_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;

        Ipp32s iIndentLeft = -pParams->xPos;
        Ipp32s iIndentRight = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
        MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
        memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);
            
            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_bottom_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* copy existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_bottom_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_bottom_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], (pParams->dataWidth - iIndent));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_bottom_left_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + pParams->yPos * pParams->srcStep;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        Ipp32s iIndentLeft = -pParams->xPos;
        Ipp32s iIndentRight = pParams->frameSize.width - pParams->xPos;

        /* create existing lines */
        for (i = pParams->yPos; i < pParams->frameSize.height; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_bottom_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        Ipp32s i;

        /* clone upper row */
        for (i = pParams->yPos; i < 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
        }
        /* copy remain row(s) */
        for (i = 0; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        /* clone the latest line */
        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_bottom_left_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);

    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = -pParams->xPos;

        /* create upper row */
        memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
        MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->frameSize.height; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndent);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndent, pSrc, chromaMult*(pParams->dataWidth - iIndent)*sizeof(PixType));

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_bottom_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc + chromaMult*pParams->xPos;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;
        Ipp32s iIndent;

        iIndent = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
        memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], (pParams->dataWidth - iIndent));

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->frameSize.height; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*iIndent*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndent, &pSrc[chromaMult*(iIndent - 1)], pParams->dataWidth - iIndent);

            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

template<typename PixType, typename InterpolationStruct, Ipp32s chromaMult>
void read_data_through_boundary_top_bottom_left_right_px(InterpolationStruct *pParams)
{
    /* normalize data position */
    if (-pParams->xPos >= pParams->dataWidth)
        pParams->xPos = -(pParams->dataWidth - 1);
    if (-pParams->yPos >= pParams->dataHeight)
        pParams->yPos = -(pParams->dataHeight - 1);
    if (pParams->xPos >= pParams->frameSize.width)
        pParams->xPos = pParams->frameSize.width - 1;
    if (pParams->yPos >= pParams->frameSize.height)
        pParams->yPos = pParams->frameSize.height - 1;

    /* preread data into temporal buffer */
    {
        const PixType *pSrc = pParams->pSrc;
        PixType *pDst = pParams->pDst;
        PixType *pTmp = pParams->pDst;
        Ipp32s i;

        Ipp32s iIndentLeft = -pParams->xPos;
        Ipp32s iIndentRight = pParams->frameSize.width - pParams->xPos;

        /* create upper row */
        memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
        MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
        memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);

        pDst += pParams->dstStep;
        pSrc += pParams->srcStep;

        /* clone upper row */
        for (i = pParams->yPos + 1; i <= 0; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pTmp, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }

        /* create remain row(s) */
        for (i = 1; i < pParams->frameSize.height; i += 1)
        {
            memset_with_mult<PixType, chromaMult>(pDst, pSrc, iIndentLeft);
            MFX_INTERNAL_CPY(pDst + chromaMult*iIndentLeft, pSrc, chromaMult*(pParams->frameSize.width)*sizeof(PixType));
            memset_with_mult<PixType, chromaMult>(pDst + chromaMult*iIndentRight, &pSrc[chromaMult*(iIndentRight - iIndentLeft - 1)], pParams->dataWidth - iIndentRight);
            
            pDst += pParams->dstStep;
            pSrc += pParams->srcStep;
        }

        pSrc = pDst - pParams->dstStep;
        for (i = pParams->frameSize.height; i < pParams->dataHeight + pParams->yPos; i += 1)
        {
            MFX_INTERNAL_CPY(pDst, pSrc, chromaMult*pParams->dataWidth*sizeof(PixType));
            pDst += pParams->dstStep;
        }
    }
}

/* declare functions for handle boundary cases */
pH264Interpolation_8u read_data_through_boundary_table_8u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_left_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_right_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_left_right_px<Ipp8u, H265InterpolationParams_8u, 1>,

    &read_data_through_boundary_top_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_left_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_right_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_left_right_px<Ipp8u, H265InterpolationParams_8u, 1>,

    &read_data_through_boundary_bottom_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_bottom_left_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_bottom_right_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_bottom_left_right_px<Ipp8u, H265InterpolationParams_8u, 1>,

    &read_data_through_boundary_top_bottom_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_bottom_left_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_bottom_right_px<Ipp8u, H265InterpolationParams_8u, 1>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp8u, H265InterpolationParams_8u, 1>
};

pH264Interpolation_8u read_data_through_boundary_table_nv12_8u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_left_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_right_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_left_right_px<Ipp8u, H265InterpolationParams_8u, 2>,

    &read_data_through_boundary_top_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_left_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_right_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_left_right_px<Ipp8u, H265InterpolationParams_8u, 2>,

    &read_data_through_boundary_bottom_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_bottom_left_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_bottom_right_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_bottom_left_right_px<Ipp8u, H265InterpolationParams_8u, 2>,

    &read_data_through_boundary_top_bottom_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_bottom_left_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_bottom_right_px<Ipp8u, H265InterpolationParams_8u, 2>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp8u, H265InterpolationParams_8u, 2>};

pH264Interpolation_16u read_data_through_boundary_table_16u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_left_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_right_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_left_right_px<Ipp16u, H265InterpolationParams_16u, 1>,

    &read_data_through_boundary_top_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_left_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_right_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_left_right_px<Ipp16u, H265InterpolationParams_16u, 1>,

    &read_data_through_boundary_bottom_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_bottom_left_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_bottom_right_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_bottom_left_right_px<Ipp16u, H265InterpolationParams_16u, 1>,

    &read_data_through_boundary_top_bottom_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_bottom_left_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_bottom_right_px<Ipp16u, H265InterpolationParams_16u, 1>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp16u, H265InterpolationParams_16u, 1>
};

pH264Interpolation_16u read_data_through_boundary_table_nv12_16u_pxmx[16] =
{
    &read_data_through_boundary_none<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_left_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_right_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_left_right_px<Ipp16u, H265InterpolationParams_16u, 2>,

    &read_data_through_boundary_top_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_left_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_right_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_left_right_px<Ipp16u, H265InterpolationParams_16u, 2>,

    &read_data_through_boundary_bottom_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_bottom_left_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_bottom_right_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_bottom_left_right_px<Ipp16u, H265InterpolationParams_16u, 2>,

    &read_data_through_boundary_top_bottom_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_bottom_left_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_bottom_right_px<Ipp16u, H265InterpolationParams_16u, 2>,
    &read_data_through_boundary_top_bottom_left_right_px<Ipp16u, H265InterpolationParams_16u, 2>
};

IppStatus ippiInterpolateLumaBlock_H265(H265InterpolationParams_8u *interpolateInfo, Ipp8u *temporary_buffer)
{
    return ippiInterpolateLumaBlock_H265Internal<Ipp8u, H265InterpolationParams_8u>(interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateChromaBlock_H264(H265InterpolationParams_8u *interpolateInfo, Ipp8u *temporary_buffer)
{
    return ippiInterpolateChromaBlock_H264Internal<Ipp8u, H265InterpolationParams_8u>(interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateLumaBlock_H265(H265InterpolationParams_8u *interpolateInfo, Ipp16u *temporary_buffer)
{
    return ippiInterpolateLumaBlock_H265Internal<Ipp16u, H265InterpolationParams_16u>((H265InterpolationParams_16u*)interpolateInfo, temporary_buffer);
}

IppStatus ippiInterpolateChromaBlock_H264(H265InterpolationParams_8u *interpolateInfo, Ipp16u *temporary_buffer)
{
    return ippiInterpolateChromaBlock_H264Internal<Ipp16u, H265InterpolationParams_16u>((H265InterpolationParams_16u*)interpolateInfo, temporary_buffer);
}


} /*namespace UMC_HEVC_DECODER*/

#endif // UMC_ENABLE_H265_VIDEO_DECODER