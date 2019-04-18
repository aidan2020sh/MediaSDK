// Copyright (c) 2011-2018 Intel Corporation
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

#include "functions_common.h"
#include "ipps.h"
#include "ippi.h"


IppStatus cppiConvert_16s8u_C1R(const Ipp16s *pSrc, Ipp32s iSrcStride, Ipp32u iSrcBitsPerSample,
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize size)
{
    Ipp32u iWidth  = size.width;
    Ipp32u iHeight = size.height;
    Ipp32u iRound  = (1 << (iSrcBitsPerSample - 8 - 1));
    Ipp32u x, y;

    for (y = 0; y < iHeight; y++)
    {
        for (x = 0; x < iWidth; x++)
        {
            pDst[x] = (Ipp8u)((pSrc[x] + iRound) >> (iSrcBitsPerSample - 8));
        }
        pSrc = (Ipp16s*)((Ipp8u*)pSrc + iSrcStride);
        pDst = pDst + iDstStride;
    }

    return ippStsNoErr;
}

IppStatus cppiConvert_8u16s_C1R(const Ipp8u *pSrc, Ipp32s iSrcStride, Ipp32u iDstBitsPerSample,
    Ipp16s *pDst, Ipp32s iDstStride, IppiSize size)
{
    Ipp32u iWidth  = size.width;
    Ipp32u iHeight = size.height;
//    Ipp32u iRound  = (1 << (iDstBitsPerSample - 8 - 1));
    Ipp32u x, y;

    for (y = 0; y < iHeight; y++)
    {
        for (x = 0; x < iWidth; x++)
        {
            pDst[x] = (Ipp16s)((pSrc[x]) << (iDstBitsPerSample - 8));
        }
        pSrc = (Ipp8u*)pSrc + iSrcStride;
        pDst = (Ipp16s*)((Ipp8u*)pDst + iDstStride);
    }

    return ippStsNoErr;
}

IppStatus cppiGrayToBGR_8u_P1C3R(const Ipp8u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    Ipp8u *pSrcRow, *pDstRow;
    Ipp32s i, j;

    for (i = 0; i < srcSize.height; i++)
    {
        pSrcRow = (Ipp8u*)&pSrc[iSrcStride*i];
        pDstRow = (Ipp8u*)&pDst[iDstStride*i];
        for (j = 0; j < srcSize.width; j++, pDstRow+=3)
        {
            pDstRow[0] = pDstRow[1] = pDstRow[2] = pSrcRow[j];
        }
    }

    return ippStsNoErr;
}

IppStatus cppiGrayToYCbCr420_8u_P1P3R(const Ipp8u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst[3], Ipp32s iDstStride[3], IppiSize srcSize)
{
    Ipp8u *pSrcRow, *pDstRow;
    Ipp32s i;

    for (i = 0; i < srcSize.height; i++)
    {
        pSrcRow = (Ipp8u*)&pSrc[iSrcStride*i];
        pDstRow = (Ipp8u*)&pDst[0][iDstStride[0]*i];
        ippsCopy_8u(pSrcRow, pDstRow, srcSize.width);
    }

    ippsSet_8u(128, pDst[1], iDstStride[1]*srcSize.height/2);
    ippsSet_8u(128, pDst[2], iDstStride[2]*srcSize.height/2);

    return ippStsNoErr;
}

IppStatus cppiYCbCrToGray_8u_P3P1R(const Ipp8u *pSrc[3], Ipp32s iSrcStride[3],
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    Ipp8u *pSrcRow, *pDstRow;
    Ipp32s i;

    for (i = 0; i < srcSize.height; i++)
    {
        pSrcRow = (Ipp8u*)&pSrc[0][iSrcStride[0]*i];
        pDstRow = (Ipp8u*)&pDst[iDstStride*i];
        ippsCopy_8u(pSrcRow, pDstRow, srcSize.width);
    }

    return ippStsNoErr;
}


IppStatus cppiSwapChannes_C3_I(Ipp8u *pSrc, Ipp32s iSrcStride, IppiSize srcSize)
{
    Ipp8u iTemp;
    Ipp32s i, j;

    for(i = 0; i < srcSize.height; i++)
    {
        for(j = 0; j < srcSize.width; j++)
        {
            iTemp = pSrc[3*j + 0];
            pSrc[3*j + 0] = pSrc[3*j + 2];
            pSrc[3*j + 2] = iTemp;
        }
        pSrc += iSrcStride;
    }

    return ippStsNoErr;
}

IppStatus cppiSwapChannes_C4_I(Ipp8u *pSrc, Ipp32s iSrcStride, IppiSize srcSize)
{
    Ipp8u iTemp;
    Ipp32s i, j;

    for(i = 0; i < srcSize.height; i++)
    {
        for(j = 0; j < srcSize.width; j++)
        {
            iTemp = pSrc[4*j + 0];
            pSrc[4*j + 0] = pSrc[4*j + 2];
            pSrc[4*j + 2] = iTemp;
        }
        pSrc += iSrcStride;
    }

    return ippStsNoErr;
}

IppStatus cppiBGR_8u_AC4C3(const Ipp8u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    Ipp32s i, j;

    for(i = 0; i < srcSize.height; i++)
    {
        for(j = 0; j < srcSize.width; j++)
        {
            pDst[3*j + 0] = pSrc[4*j + 0];
            pDst[3*j + 1] = pSrc[4*j + 1];
            pDst[3*j + 2] = pSrc[4*j + 2];
        }
        pSrc += iSrcStride;
        pDst += iDstStride;
    }

    return ippStsNoErr;
}

IppStatus cppiBGR_8u_C3AC4(const Ipp8u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    Ipp32s i, j;

    for (i = 0; i < srcSize.height; i++)
    {
        for (j = 0; j < srcSize.width; j++)
        {
            pDst[4*j + 0] = pSrc[3*j + 0];
            pDst[4*j + 1] = pSrc[3*j + 1];
            pDst[4*j + 2] = pSrc[3*j + 2];
            pDst[4*j + 3] = 0;
        }
        pSrc += iSrcStride;
        pDst += iDstStride;
    }

    return ippStsNoErr;
}

IppStatus cppiBGR555ToBGR_16u8u_C3(const Ipp16u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst, Ipp32s iDstStride,IppiSize srcSize)
{
    Ipp32s i, j;

    for (i = 0; i < srcSize.height; i++)
    {
        for (j = 0; j < srcSize.width; j++)
        {
            pDst[3*j + 2] = (Ipp8u)((pSrc[j] & 0x7c00) >> 7);
            pDst[3*j + 1] = (Ipp8u)((pSrc[j] & 0x03e0) >> 2);
            pDst[3*j + 0] = (Ipp8u)((pSrc[j] & 0x001f) << 3);
        }
        pSrc = (Ipp16u *) ((Ipp8u *) pSrc + iSrcStride);
        pDst += iDstStride;
    }

    return ippStsNoErr;
}

IppStatus cppiBGR565ToBGR_16u8u_C3(const Ipp16u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    Ipp32s i, j;

    for (i = 0; i < srcSize.height; i++)
    {
        for (j = 0; j < srcSize.width; j++)
        {
            pDst[3*j + 2] = (Ipp8u)((pSrc[j] & 0xf800) >> 8);
            pDst[3*j + 1] = (Ipp8u)((pSrc[j] & 0x07e0) >> 3);
            pDst[3*j + 0] = (Ipp8u)((pSrc[j] & 0x001f) << 3);
        }
        pSrc = (Ipp16u *) ((Ipp8u *) pSrc + iSrcStride);
        pDst += iDstStride;
    }

    return ippStsNoErr;
}

IppStatus cppiYCbCr41PTo420_8u_P1P3(const Ipp8u *pSrc, Ipp32s iSrcStride,
    Ipp8u *pDst[3], Ipp32s iDstStride[3], IppiSize srcSize)
{
    Ipp8u *pDstY = pDst[0];
    Ipp8u *pDstU = pDst[1];
    Ipp8u *pDstV = pDst[2];
    Ipp32s iDstStepY = iDstStride[0];
    Ipp32s iDstStepU = iDstStride[1];
    Ipp32s iDstStepV = iDstStride[2];
    Ipp32s x, y;

    for(y = 0; y < srcSize.height; y++)
    {
        for(x = 0; x < srcSize.width/8; x++)
        {
            pDstY[8*x + 0] = pSrc[12*x + 1];
            pDstY[8*x + 1] = pSrc[12*x + 3];
            pDstY[8*x + 2] = pSrc[12*x + 5];
            pDstY[8*x + 3] = pSrc[12*x + 7];
            pDstY[8*x + 4] = pSrc[12*x + 8];
            pDstY[8*x + 5] = pSrc[12*x + 9];
            pDstY[8*x + 6] = pSrc[12*x + 10];
            pDstY[8*x + 7] = pSrc[12*x + 11];

            if(!(y & 1))
            {
                pDstU[4*x + 0] = pSrc[12*x + 0];
                pDstU[4*x + 1] = pSrc[12*x + 0];
                pDstU[4*x + 2] = pSrc[12*x + 4];
                pDstU[4*x + 3] = pSrc[12*x + 4];
                pDstV[4*x + 0] = pSrc[12*x + 2];
                pDstV[4*x + 1] = pSrc[12*x + 2];
                pDstV[4*x + 2] = pSrc[12*x + 6];
                pDstV[4*x + 3] = pSrc[12*x + 6];
            }
        }

        pSrc  += iSrcStride;
        pDstY += iDstStepY;
        if(y & 1)
        {
            pDstU += iDstStepU;
            pDstV += iDstStepV;
        }
    }

    return ippStsNoErr;
}

IppStatus cppiYCbCr420To41P_8u_P3P1(const Ipp8u *pSrc[3], Ipp32s iSrcStride[3],
    Ipp8u *pDst, Ipp32s iDstStride, IppiSize srcSize)
{
    const Ipp8u *pSrcY = pSrc[0];
    const Ipp8u *pSrcU = pSrc[1];
    const Ipp8u *pSrcV = pSrc[2];
    Ipp32s iSrcStepY = iSrcStride[0];
    Ipp32s iSrcStepU = iSrcStride[1];
    Ipp32s iSrcStepV = iSrcStride[2];
    Ipp32s x, y;

    for (y = 0; y < srcSize.height; y++)
    {
        for (x = 0; x < srcSize.width/8; x++)
        {
            pDst[12*x + 0]  = pSrcU[4*x + 0];
            pDst[12*x + 1]  = pSrcY[8*x + 0];
            pDst[12*x + 2]  = pSrcV[4*x + 0];
            pDst[12*x + 3]  = pSrcY[8*x + 1];
            pDst[12*x + 4]  = pSrcU[4*x + 2];
            pDst[12*x + 5]  = pSrcY[8*x + 2];
            pDst[12*x + 6]  = pSrcV[4*x + 2];
            pDst[12*x + 7]  = pSrcY[8*x + 3];
            pDst[12*x + 8]  = pSrcY[8*x + 4];
            pDst[12*x + 9]  = pSrcY[8*x + 5];
            pDst[12*x + 10] = pSrcY[8*x + 6];
            pDst[12*x + 11] = pSrcY[8*x + 7];
        }

        pDst  += iDstStride;
        pSrcY += iSrcStepY;
        if(y & 1)
        {
            pSrcU += iSrcStepU;
            pSrcV += iSrcStepV;
        }
    }

    return ippStsNoErr;
}