// Copyright (c) 2003-2019 Intel Corporation
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

#include "umc_defs.h"
#if defined (MFX_ENABLE_H264_VIDEO_DECODE)

#ifndef __UMC_H264_RESIDUAL_DECODER_TEMPLATES_H
#define __UMC_H264_RESIDUAL_DECODER_TEMPLATES_H

#include "umc_h264_dec_internal_cabac.h"

namespace UMC
{

#ifdef _MSVC_LANG
// turn off the "code is not reachable from preceding code" remark
#pragma warning(disable : 128)

// turn off the "conditional expression is constant" warning
#pragma warning(disable: 280)
#pragma warning(disable: 4127)
#endif

enum
{
    LUMA_BLOCK_8X8_0            = 0x01,
    LUMA_BLOCK_8X8_1            = 0x02,
    LUMA_BLOCK_8X8_2            = 0x04,
    LUMA_BLOCK_8X8_3            = 0x08,

    CHROMA_AC_BLOCKS            = 0x20,
    CHROMA_DC_AC_BLOCKS         = 0x30
};

template <typename Coeffs, int32_t color_format, int32_t is_field>
class ResidualDecoderCAVLC
{
public:
    typedef Coeffs *  CoeffsPtr;

    virtual ~ResidualDecoderCAVLC() {}

    void DecodeCoefficients8x8_CAVLC(H264SegmentDecoderMultiThreaded * sd)
    {
        unsigned long long blockcbp;
        uint32_t u8x8block = 1;
        int32_t uBlock;
        uint32_t uBlockBit;

        uint32_t uNC;
        uint32_t uAboveIndex;
        uint32_t uLeftIndex;
        Coeffs TempCoeffs[16];
        int16_t sNumCoeff;
        int32_t bFieldDecodeFlag = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);
        CoeffsPtr pCurCoeffs;
        bool advance_to_next_block=false;
        uint8_t bf = sd->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE || bFieldDecodeFlag;
        uint8_t cbp = sd->m_cur_mb.LocalMacroblockInfo->cbp;

        // Initialize blockcbp bits from input cbp (from the bitstream)
        blockcbp = 0;   // no coeffs
        for (uBlock=0; uBlock<5; uBlock++)
        {
            if (cbp & u8x8block)
                blockcbp |= blockcbp_table[uBlock];
            u8x8block <<= 1;
        }

        if (cbp & u8x8block)
        {
            switch(color_format)
            {
            case CHROMA_FORMAT_400:
                break;
            case CHROMA_FORMAT_420:
                blockcbp |= CONST_LL(0x3ff0000) << 1;
                break;
            case CHROMA_FORMAT_422:
                blockcbp |= CONST_LL(0x3ffff0000) << 1;
                break;
            case CHROMA_FORMAT_444:
                blockcbp |= CONST_LL(0x3ffffffff0000) << 1;
                break;
            }
        }

        uBlock      = 1;        // start block loop with first luma 4x4
        uBlockBit   = 2;
        blockcbp  >>= 1;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = 0;

        for (uBlock = 1; uBlock<FIRST_DC_CHROMA; uBlock++)
        {
            uAboveIndex = BlockNumToMBColLuma[uBlock];
            uLeftIndex  = BlockNumToMBRowLuma[uBlock];
            pCurCoeffs = TempCoeffs;

            sNumCoeff = 0;
            if ((blockcbp & 1) != 0)
            {
                uNC = sd->GetBlocksLumaContext(uAboveIndex,uLeftIndex);

                // Get CAVLC-code coefficient info from bitstream. Following call
                // updates pbs, bitOffset, sNumCoeff, sNumTrOnes, TrOneSigns,
                // and uTotalZero and fills CoeffBuf and uRunBeforeBuf.
                memset(TempCoeffs, 0, 16*sizeof(Coeffs));
                sd->m_pBitStream->GetCAVLCInfoLuma(uNC,
                    sd->m_pSliceHeader->scan_idx_end + 1 -
                    sd->m_pSliceHeader->scan_idx_start,
                                                    sNumCoeff,
                                                    &pCurCoeffs,
                                                    bf);
                //copy coeffs from tempbuffer
                for (int32_t i=0;i<16;i++)
                {
                    CoeffsPtr coeffsPtr = (CoeffsPtr)sd->m_pCoeffBlocksWrite;
                    coeffsPtr[hp_scan8x8[bf][((uBlock-1)&3)+i*4]] = TempCoeffs[mp_scan4x4[bf][i]];
                }

                advance_to_next_block = true;
                sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma |= uBlockBit;
            }

            if ((uBlock&3)==0 && advance_to_next_block) //end of 8x8 block
            {
                sd->m_pCoeffBlocksWrite += 64*(sizeof(Coeffs) / sizeof(sd->m_pCoeffBlocksWrite[0]));
                advance_to_next_block=false;
            }
            // Update num coeff storage for predicting future blocks
            sd->m_cur_mb.GetNumCoeffs()->numCoeffs[uLeftIndex * 4 + uAboveIndex] = (uint8_t) sNumCoeff;

            blockcbp >>= 1;
            uBlockBit <<= 1;
        }   // uBlock

        if (color_format == CHROMA_FORMAT_400)
            return;

        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;

        if (sd->m_pSliceHeader->scan_idx_start == 0)
        {
            for (uBlock = FIRST_DC_CHROMA; uBlock < FIRST_AC_CHROMA; uBlock++)
            {
                if ((blockcbp & 1) != 0)
                {
                    switch(color_format)
                    {
                    case CHROMA_FORMAT_420:
                        sd->m_pBitStream->GetCAVLCInfoChroma0(sNumCoeff, (CoeffsPtr*)&sd->m_pCoeffBlocksWrite);
                        break;
                    case CHROMA_FORMAT_422:
                        sd->m_pBitStream->GetCAVLCInfoChroma2(sNumCoeff, (CoeffsPtr*)&sd->m_pCoeffBlocksWrite);
                        break;
                    default:
                        sNumCoeff = 0;
                        VM_ASSERT(false);
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    };

                    sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[uBlock - FIRST_DC_CHROMA] |= (sNumCoeff ? 1 : 0);
                }

                blockcbp >>= 1;
                //  Can't early exit without setting numcoeff for rest of blocks
                if ((blockcbp == 0) && (uBlock == (FIRST_AC_CHROMA - 1)))
                {
                    // no AC chroma coeffs, set chrroma NumCoef buffers to zero and exit
                    uint8_t *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;

                    switch (color_format)
                    {
                    case CHROMA_FORMAT_420:
                        memset(pNumCoeffsArray + 16, 0, sizeof(*pNumCoeffsArray) * 4 * 2);
                        break;

                    case CHROMA_FORMAT_422:
                        memset(pNumCoeffsArray + 16, 0, sizeof(*pNumCoeffsArray) * 8 * 2);
                        break;

                    default:
                        break;
                    }
                    return;
                }
            }   // uBlock
        }

        uBlockBit = 2;

        int32_t colorFactor;

        switch(color_format)
        {
        case CHROMA_FORMAT_420:
            colorFactor = 1;
            break;
        case CHROMA_FORMAT_422:
            colorFactor = 2;
            break;
        case CHROMA_FORMAT_444:
            colorFactor = 4;
            break;
        default:
            colorFactor = 0;
            VM_ASSERT(false);
        };

        if (sd->m_pSliceHeader->scan_idx_end > 0)
        {
            int32_t tmpx = sd->m_pSliceHeader->scan_idx_start;
            int32_t tmpy = sd->m_pSliceHeader->scan_idx_end;
            int32_t iMaxNum;

            if (tmpx < 1)
            {
                iMaxNum = tmpy;
                tmpy = tmpy - tmpx + 1;
                tmpx = 1;
            }
            else
            {
                iMaxNum = tmpy - tmpx + 1;
            }

            sd->m_pBitStream->SetIdx(tmpx, tmpy);

            for (uBlock = FIRST_AC_CHROMA; uBlock < FIRST_AC_CHROMA + 8*colorFactor; uBlock++)
            {
                if (uBlock == (FIRST_AC_CHROMA + 4*colorFactor))
                    uBlockBit = 2;

                uAboveIndex = BlockNumToMBColChromaAC[color_format][uBlock-FIRST_AC_CHROMA];
                uLeftIndex  = BlockNumToMBRowChromaAC[color_format][uBlock-FIRST_AC_CHROMA];
                uint32_t addval = uBlock >= (FIRST_AC_CHROMA + 4*colorFactor) ?
                    (FIRST_AC_CHROMA + 4*colorFactor - 3) : 16;
                sNumCoeff = 0;
                if ((blockcbp & 1) != 0)
                {
                    switch(color_format)
                    {
                    case CHROMA_FORMAT_420:
                        uNC = sd->GetBlocksChromaContextBMEH(uAboveIndex, uLeftIndex, uBlock >= (FIRST_AC_CHROMA + 4*colorFactor));
                        break;
                    case CHROMA_FORMAT_422:
                        uNC = sd->GetBlocksChromaContextH2(uAboveIndex, uLeftIndex, uBlock >= (FIRST_AC_CHROMA + 4*colorFactor));
                        break;
                    case CHROMA_FORMAT_444:
                        uNC = sd->GetBlocksChromaContextH4(uAboveIndex, uLeftIndex, uBlock >= (FIRST_AC_CHROMA + 4*colorFactor));
                        break;
                    default:
                        uNC = 0;
                        VM_ASSERT(false);
                        throw h264_exception(UMC_ERR_INVALID_STREAM);
                    };

                    int32_t scan_idx_start = sd->m_pSliceHeader->scan_idx_start;
                    if (scan_idx_start < 1)
                        scan_idx_start = 1;

                    int32_t maxNumCoeff = sd->m_pSliceHeader->scan_idx_end + 1 - scan_idx_start;

                    // Get CAVLC-code coefficient info from bitstream. Following call
                    // updates pbs, bitOffset, sNumCoeff, sNumTrOnes, TrOneSigns,
                    // and uTotalZero and fills CoeffBuf and uRunBeforeBuf.
                    sd->m_pBitStream->GetCAVLCInfoLuma(uNC,
                        maxNumCoeff,
                        sNumCoeff,
                        (CoeffsPtr*)&sd->m_pCoeffBlocksWrite,
                        bf);

                    sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[uBlock >= (FIRST_AC_CHROMA + 4*colorFactor)]
                        |= (sNumCoeff ? uBlockBit : 0);
                }
                // Update num coeff storage for predicting future blocks
                sd->m_cur_mb.GetNumCoeffs()->numCoeffs[uLeftIndex * 2 * (1 + (int32_t)(color_format == CHROMA_FORMAT_444))
                    + uAboveIndex + addval] = (uint8_t)sNumCoeff;

                blockcbp >>= 1;
                uBlockBit <<= 1;
            }   // uBlock

        }
        // update buffer position pointer
    } // void DecodeCoefficients8x8_CAVLC(H264SegmentDecoderMultiThreaded * sd)

#define SET_TO_ZERO_COEFFS_NUMBER(x_pos, y_pos) \
    { \
        pNumCoeffsArray[(y_pos) * 4 + x_pos] = (uint8_t) 0; \
        pNumCoeffsArray[(y_pos) * 4 + x_pos + 1] = (uint8_t) 0; \
        pNumCoeffsArray[(y_pos + 1) * 4 + x_pos] = (uint8_t) 0; \
        pNumCoeffsArray[(y_pos + 1) * 4 + x_pos + 1] = (uint8_t) 0; \
    }

#define DECODE_EXTERNAL_LUMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /*  to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVCLContext; \
        iVCLContext = sd->GetBlocksLumaContextExternal(); \
        /* decode block coeffs */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVCLContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr *) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[(y_pos) * 4 + x_pos] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_TOP_LUMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /*  to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVCLContext; \
        iVCLContext = sd->GetBlocksLumaContextTop(x_pos, pNumCoeffsArray[x_pos - 1]); \
        /* decode block coeffs */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVCLContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr *) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[(y_pos) * 4 + x_pos] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_LEFT_LUMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /*  to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVCLContext; \
        iVCLContext = sd->GetBlocksLumaContextLeft(y_pos, pNumCoeffsArray[(y_pos - 1) * 4]); \
        /* decode block coeffs */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVCLContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr *) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[(y_pos) * 4 + x_pos] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_INTERNAL_LUMA_BLOCK_CAVLC(block_num, raster_block_num) \
    { \
        int16_t iCoeffsNumber; \
        /*  to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVCLContext; \
        iVCLContext = sd->GetBlocksLumaContextInternal(raster_block_num, pNumCoeffsArray); \
        /* decode block coeffs */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVCLContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr *) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[raster_block_num] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_CHROMA_DC_BLOCK_CAVLC(component) \
    { \
        int16_t iCoeffsNumber; \
        /* decode chroma DC coefficients */ \
        switch (color_format) \
        { \
        case CHROMA_FORMAT_420: \
            sd->m_pBitStream->GetCAVLCInfoChroma0(iCoeffsNumber, \
                   (CoeffsPtr*) &sd->m_pCoeffBlocksWrite); \
            break; \
        case CHROMA_FORMAT_422: \
            sd->m_pBitStream->GetCAVLCInfoChroma2(iCoeffsNumber, \
                   (CoeffsPtr*) &sd->m_pCoeffBlocksWrite); \
            break; \
        default: \
            iCoeffsNumber = 0; \
            VM_ASSERT(false); \
            throw h264_exception(UMC_ERR_INVALID_STREAM); \
        } \
        /* update final CBP */ \
        uFinalCBP[component] = (iCoeffsNumber) ? (1) : (0); \
    }

#define DECODE_EXTERNAL_CHROMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /* to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVLCContext; \
        switch (color_format) \
        { \
        case CHROMA_FORMAT_420: \
            iVLCContext = sd->GetBlocksChromaContextBMEHExternal(iComponent); \
            break; \
        case CHROMA_FORMAT_422: \
            iVLCContext = sd->GetBlocksChromaContextH2(x_pos, y_pos, iComponent); \
            break; \
        default: \
            iVLCContext = 0; \
            VM_ASSERT(false); \
            throw h264_exception(UMC_ERR_INVALID_STREAM); \
        }; \
        /* decode chrominance coefficients */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVLCContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr*) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP[iComponent] |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[block_num] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_TOP_CHROMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /* to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVLCContext; \
        switch (color_format) \
        { \
        case CHROMA_FORMAT_420: \
            iVLCContext = sd->GetBlocksChromaContextBMEHTop(x_pos, \
                                                            pNumCoeffsArray[0], \
                                                            iComponent); \
            break; \
        case CHROMA_FORMAT_422: \
            iVLCContext = sd->GetBlocksChromaContextH2(x_pos, y_pos, iComponent); \
            break; \
        default: \
            iVLCContext = 0; \
            VM_ASSERT(false); \
            throw h264_exception(UMC_ERR_INVALID_STREAM);   \
        }; \
        /* decode chrominance coefficients */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVLCContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr*) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP[iComponent] |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[block_num] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_LEFT_CHROMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /* to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVLCContext; \
        switch (color_format) \
        { \
        case CHROMA_FORMAT_420: \
            iVLCContext = sd->GetBlocksChromaContextBMEHLeft(y_pos, \
                                                             pNumCoeffsArray[0], \
                                                             iComponent); \
            break; \
        case CHROMA_FORMAT_422: \
            iVLCContext = sd->GetBlocksChromaContextH2(x_pos, y_pos, iComponent); \
            break; \
        default: \
            iVLCContext = 0; \
            VM_ASSERT(false); \
            throw h264_exception(UMC_ERR_INVALID_STREAM);   \
        }; \
        /* decode chrominance coefficients */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVLCContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr*) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP[iComponent] |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[block_num] = (uint8_t) iCoeffsNumber; \
    }

#define DECODE_INTERNAL_CHROMA_BLOCK_CAVLC(x_pos, y_pos, block_num) \
    { \
        int16_t iCoeffsNumber; \
        /* to be honest, VLCContext is an average of coeffs numbers \
            of neighbouring blocks */ \
        int32_t iVLCContext; \
        switch (color_format) \
        { \
        case CHROMA_FORMAT_420: \
            iVLCContext = sd->GetBlocksChromaContextBMEHInternal(block_num, \
                                                                 pNumCoeffsArray); \
            break; \
        case CHROMA_FORMAT_422: \
            iVLCContext = sd->GetBlocksChromaContextH2(x_pos, y_pos, iComponent); \
            break; \
        default: \
            iVLCContext = 0; \
            VM_ASSERT(false); \
            throw h264_exception(UMC_ERR_INVALID_STREAM);   \
        }; \
        /* decode chrominance coefficients */ \
        sd->m_pBitStream->GetCAVLCInfoLuma(iVLCContext, \
                                                    iMaxNum, \
                                                    iCoeffsNumber, \
                                                    (CoeffsPtr*) &sd->m_pCoeffBlocksWrite, \
                                                    bf); \
        /* update final CBP */ \
        uFinalCBP[iComponent] |= (iCoeffsNumber) ? (1 << (block_num + 1)) : (0); \
        /* update a num coeff storage for a prediction of future blocks */ \
        pNumCoeffsArray[block_num] = (uint8_t) iCoeffsNumber; \
    }

    void DecodeCoefficients16x16_CAVLC(H264SegmentDecoderMultiThreaded * sd)
    {
        uint32_t iDCCBP = 0;

        // set field flag
        int32_t field_flag = (pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo) || is_field) ? 1 : 0;
        uint8_t scan_idx_start = sd->m_pSliceHeader->scan_idx_start;
        uint8_t scan_idx_end = sd->m_pSliceHeader->scan_idx_end;

        // decode luma DC block
        if (scan_idx_start == 0)
        {
            int16_t sNumCoeff;
            uint32_t uNC = 0;

            sd->m_pBitStream->SetIdx(0, 15);

            uNC = sd->GetDCBlocksLumaContext();
            sd->m_pBitStream->GetCAVLCInfoLuma(uNC,
                                    16,
                                    sNumCoeff,
                                    (CoeffsPtr *)&sd->m_pCoeffBlocksWrite,
                                    field_flag);

            if (sNumCoeff)
                iDCCBP = D_CBP_LUMA_DC;

            scan_idx_start++;
            scan_idx_end++;
        }

        sd->m_pBitStream->SetIdx(scan_idx_start, scan_idx_end);

        DecodeCoefficients4x4_CAVLC(sd, 15);

        // save final CBP
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma |= iDCCBP;
    } // void DecodeCoefficients16x16_CAVLC(H264SegmentDecoderMultiThreaded * sd)

    void DecodeCoefficients4x4_CAVLC(H264SegmentDecoderMultiThreaded * sd,
                                       int32_t iMaxNum = 16)
    {
        int32_t bFieldDecodeFlag = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo);
        int32_t bf = sd->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE || bFieldDecodeFlag;
        uint32_t cbp = sd->m_cur_mb.LocalMacroblockInfo->cbp;

        //
        // decode luminance blocks
        //
        {
            uint32_t uFinalCBP;
            uint8_t *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;

            if (iMaxNum == 16)
            {
                /* Not 16x16 AC */
                iMaxNum = sd->m_pSliceHeader->scan_idx_end + 1 -
                          sd->m_pSliceHeader->scan_idx_start;

            }
            else
            {
                int32_t scan_idx_start = sd->m_pSliceHeader->scan_idx_start;
                if (scan_idx_start < 1)
                    scan_idx_start = 1;

                iMaxNum = sd->m_pSliceHeader->scan_idx_end + 1 - scan_idx_start;
            }

            uFinalCBP = 0;
            if (cbp & LUMA_BLOCK_8X8_0)
            {
                DECODE_EXTERNAL_LUMA_BLOCK_CAVLC(0, 0, 0)
                DECODE_TOP_LUMA_BLOCK_CAVLC(1, 0, 1)
                DECODE_LEFT_LUMA_BLOCK_CAVLC(0, 1, 2)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(3, 5)
            }
            else
            {
                SET_TO_ZERO_COEFFS_NUMBER(0, 0)
            }

            if (cbp & LUMA_BLOCK_8X8_1)
            {
                DECODE_TOP_LUMA_BLOCK_CAVLC(2, 0, 4)
                DECODE_TOP_LUMA_BLOCK_CAVLC(3, 0, 5)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(6, 6)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(7, 7)
            }
            else
            {
                SET_TO_ZERO_COEFFS_NUMBER(2, 0)
            }

            if (cbp & LUMA_BLOCK_8X8_2)
            {
                DECODE_LEFT_LUMA_BLOCK_CAVLC(0, 2, 8)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(9, 9)
                DECODE_LEFT_LUMA_BLOCK_CAVLC(0, 3, 10)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(11, 13)
            }
            else
            {
                SET_TO_ZERO_COEFFS_NUMBER(0, 2)
            }

            if (cbp & LUMA_BLOCK_8X8_3)
            {
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(12, 10)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(13, 11)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(14, 14)
                DECODE_INTERNAL_LUMA_BLOCK_CAVLC(15, 15)
            }
            else
            {
                SET_TO_ZERO_COEFFS_NUMBER(2, 2)
            }

            // save final CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = uFinalCBP;
        }

        //
        // decode chrominance blocks
        //

        if (color_format != CHROMA_FORMAT_400 && (cbp & CHROMA_DC_AC_BLOCKS))
        {
            uint32_t uFinalCBP[2];

            uFinalCBP[0] = uFinalCBP[1] = 0;

            if (sd->m_pSliceHeader->scan_idx_start == 0)
            {
                // decode DC blocks
                DECODE_CHROMA_DC_BLOCK_CAVLC(0)
                DECODE_CHROMA_DC_BLOCK_CAVLC(1)
            }

            if ((cbp & CHROMA_AC_BLOCKS) && (sd->m_pSliceHeader->scan_idx_end > 0))
            {
                int32_t iComponent;

                //pNumCoeffsArray += iComponent * 4 + 16;
                uint8_t *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs + 16;

                int32_t tmpx = sd->m_pSliceHeader->scan_idx_start;
                int32_t tmpy = sd->m_pSliceHeader->scan_idx_end;

                if (tmpx < 1)
                {
                    iMaxNum = tmpy;
                    tmpy = tmpy - tmpx + 1;
                    tmpx = 1;
                }
                else
                {
                    iMaxNum = tmpy - tmpx + 1;
                }

                sd->m_pBitStream->SetIdx(tmpx, tmpy);

                for (iComponent = 0; iComponent < 2; iComponent += 1)
                {
                    switch (color_format)
                    {
                    case CHROMA_FORMAT_420:

                        DECODE_EXTERNAL_CHROMA_BLOCK_CAVLC(0, 0, 0)
                        DECODE_TOP_CHROMA_BLOCK_CAVLC(1, 0, 1)
                        DECODE_LEFT_CHROMA_BLOCK_CAVLC(0, 1, 2)
                        DECODE_INTERNAL_CHROMA_BLOCK_CAVLC(1, 1, 3)
                        pNumCoeffsArray += 4;
                        break;

                    case CHROMA_FORMAT_422:
                        DECODE_EXTERNAL_CHROMA_BLOCK_CAVLC(0, 0, 0)
                        DECODE_TOP_CHROMA_BLOCK_CAVLC(1, 0, 1)
                        DECODE_LEFT_CHROMA_BLOCK_CAVLC(0, 1, 2)
                        DECODE_INTERNAL_CHROMA_BLOCK_CAVLC(1, 1, 3)
                        DECODE_LEFT_CHROMA_BLOCK_CAVLC(0, 2, 4)
                        DECODE_INTERNAL_CHROMA_BLOCK_CAVLC(1, 2, 5)
                        DECODE_LEFT_CHROMA_BLOCK_CAVLC(0, 3, 6)
                        DECODE_INTERNAL_CHROMA_BLOCK_CAVLC(1, 3, 7)
                        pNumCoeffsArray += 8;
                        break;

                    default:
                        break;
                    }
                }
            }

            // set the empty CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = uFinalCBP[0];
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = uFinalCBP[1];
        }
        else
        {
            // set the empty CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
        }

        // set zero values to a num coeffs storage
        if (color_format != CHROMA_FORMAT_400 && (0 == (cbp & CHROMA_AC_BLOCKS)) )
        {
            uint8_t *pNumCoeffsArray = sd->m_cur_mb.GetNumCoeffs()->numCoeffs;

            switch (color_format)
            {
            case CHROMA_FORMAT_420:
                memset(pNumCoeffsArray + 16, 0, sizeof(*pNumCoeffsArray) * 4 * 2);
                break;

            case CHROMA_FORMAT_422:
                memset(pNumCoeffsArray + 16, 0, sizeof(*pNumCoeffsArray) * 8 * 2);
                break;
            default:
                break;
            }
        }
    } // void DecodeCoefficients4x4_CAVLC(H264SegmentDecoderMultiThreaded * sd,
};

template <typename Coeffs, int32_t color_format, int32_t is_field>
class ResidualDecoderCABAC
{
public:
    typedef Coeffs *  CoeffsPtr;

    virtual ~ResidualDecoderCABAC() {}

    void DecodeCoefficients8x8_CABAC(H264SegmentDecoderMultiThreaded * sd)
    {
        uint8_t cbp = sd->m_cur_mb.LocalMacroblockInfo->cbp;
        uint32_t uBlockBit;
        CoeffsPtr pPosCoefbuf = (CoeffsPtr)sd->m_pCoeffBlocksWrite;

        uint32_t i, j;
        int32_t top_bit     = 1;
        int32_t left_bit    = 1;
        int32_t def_bit = sd->m_cur_mb.GlobalMacroblockInfo->mbtype == MBTYPE_INTRA;
        const uint32_t *ctxBase;
        const int32_t *single_scan;
        int32_t iMBAbove, iMBLeft;
        uint32_t ctxIdxInc, iCtxBase;
        bool field_flag = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo) ||
            sd->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE;
        H264DecoderBlockNeighboursInfo* pN = &sd->m_cur_mb.CurrentBlockNeighbours;

        //this code need additional checking
        if (field_flag)
        {
            ctxBase = ctxIdxOffset8x8FieldCoded;
            single_scan = hp_scan8x8[1];
        } else {
            ctxBase = ctxIdxOffset8x8FrameCoded;
            single_scan = hp_scan8x8[0];
        }

        uint32_t &cbp4x4_luma = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma;
        cbp4x4_luma = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
        uBlockBit = (2+4+8+16);//4 bit set

        // luma coefficients
        for (i = 0; i < 4; i++)
        {
            if (cbp & mask_bit[i]) // are there any coeff in current block
            {
                //set bits for current block
                sd->m_pBitStream->ResidualBlock8x8_CABAC( field_flag,
                                                    single_scan, pPosCoefbuf);

                cbp4x4_luma |= uBlockBit;
                pPosCoefbuf += 64;
            }

            uBlockBit <<= 4;
        }

        // chroma 2x2 DC coeff
        if (cbp > 15)
        {
            if (pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo))
            {
                single_scan = mp_scan4x4[1];
            }
            else
            {
                if (sd->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE)
                    single_scan = mp_scan4x4[1];
                else
                    single_scan = mp_scan4x4[0];
            }

            // chroma 2x2 DC coeff
            if (field_flag)
                ctxBase = ctxIdxOffset4x4FieldCoded;
            else
                ctxBase = ctxIdxOffset4x4FrameCoded;

            uint32_t numOfCoeffs = 0;
            switch (color_format)
            {
            case CHROMA_FORMAT_400:
                break;
            case CHROMA_FORMAT_420:
                numOfCoeffs = 4;
                break;
            case CHROMA_FORMAT_422:
                numOfCoeffs = 8;
                break;
            case CHROMA_FORMAT_444:
                numOfCoeffs = 16;
                break;
            };

            if (sd->m_pSliceHeader->scan_idx_start == 0)
            {
                iCtxBase = ctxBase[CODED_BLOCK_FLAG] +
                    ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_DC_LEVELS + color_format];

                iMBAbove = pN->mb_above.mb_num;
                iMBLeft = pN->mbs_left[0].mb_num;

                for (i = 0; i < 2; i++)
                {
                    if (0 <= iMBAbove)
                        top_bit = sd->m_mbinfo.mbs[iMBAbove].cbp4x4_chroma[i] & 1;
                    else
                        top_bit = def_bit;

                    if (0 <= iMBLeft)
                        left_bit = sd->m_mbinfo.mbs[iMBLeft].cbp4x4_chroma[i] & 1;
                    else
                        left_bit = def_bit;

                    ctxIdxInc = (top_bit<<1) + left_bit;

                    if (sd->m_pBitStream->DecodeSingleBin_CABAC(iCtxBase + ctxIdxInc))
                    {
                        const int32_t * sing_scan = 0;
                        switch (color_format)
                        {
                        case CHROMA_FORMAT_400:
                            break;
                        case CHROMA_FORMAT_420:
                            sing_scan = 0;
                            break;
                        case CHROMA_FORMAT_422:
                            sing_scan = ChromaDC422RasterScan;
                            break;
                        case CHROMA_FORMAT_444:
                            sing_scan = mp_scan4x4[0];
                            break;
                        }

                        BitStreamColorSpecific<Coeffs, color_format>::ResidualChromaDCBlock_CABAC(
                            ctxBase,
                            sing_scan,
                            pPosCoefbuf, sd->m_pBitStream);

                        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[i] = 1;
                        pPosCoefbuf += numOfCoeffs;
                    }
                }
            }

            // chroma AC coeff, all zero from start_scan
            if (cbp > 31  && (sd->m_pSliceHeader->scan_idx_end > 0) && color_format != CHROMA_FORMAT_400)
            {
                iCtxBase = ctxBase[CODED_BLOCK_FLAG] +
                            ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_AC_LEVELS];

                for (j = 0; j < 2; j++)//plane
                {
                    uint32_t &cbp4x4_chroma = sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[j];
                    uint32_t addition = 16 + numOfCoeffs*j;
                    uBlockBit = 2;
                    for (i = 0; i < numOfCoeffs; i++, uBlockBit <<= 1)//block
                    {
                        int32_t raster_order_block = block_subblock_mapping[i];

                        int32_t bit = i + 1;

                        top_bit     = def_bit;
                        left_bit    = def_bit;

                        //--- get bits from neighbouring blocks ---
                        if (sb_y[color_format][i])
                        {
                            if (color_format == CHROMA_FORMAT_444)
                                top_bit = BIT_CHECK(cbp4x4_chroma, block_subblock_mapping[raster_order_block - 4] + 1);
                            else
                                top_bit = BIT_CHECK(cbp4x4_chroma, bit - 2);
                        }
                        else
                        {
                            iMBAbove = pN->mb_above_chroma[0].mb_num;

                            if (0 <= iMBAbove)
                            {
                                if (color_format == CHROMA_FORMAT_444)
                                    top_bit = BIT_CHECK(sd->m_mbinfo.mbs[iMBAbove].cbp4x4_chroma[j],
                                        block_subblock_mapping[pN->mb_above_chroma[j].block_num + sb_x[3][i] - addition] + 1);
                                else
                                    top_bit = BIT_CHECK(sd->m_mbinfo.mbs[iMBAbove].cbp4x4_chroma[j],
                                        pN->mb_above_chroma[j].block_num + sb_x[color_format][i] - addition + 1);
                            }
                        }

                        if (sb_x[color_format][i])
                        {
                            if (color_format == CHROMA_FORMAT_444)
                                left_bit = BIT_CHECK(cbp4x4_chroma, block_subblock_mapping[raster_order_block - 1] + 1);
                            else
                                left_bit = BIT_CHECK(cbp4x4_chroma, bit - 1);
                        }
                        else
                        {
                            iMBLeft = pN->mbs_left_chroma[j][sb_y[color_format][i]].mb_num;

                            if (0 <= iMBLeft)
                            {
                                if (color_format == CHROMA_FORMAT_444)
                                    left_bit = BIT_CHECK(sd->m_mbinfo.mbs[iMBLeft].cbp4x4_chroma[j],
                                        block_subblock_mapping[pN->mbs_left_chroma[j][sb_y[3][i]].block_num - addition] + 1);
                                else
                                    left_bit = BIT_CHECK(sd->m_mbinfo.mbs[iMBLeft].cbp4x4_chroma[j],
                                        pN->mbs_left_chroma[j][sb_y[color_format][i]].block_num - addition + 1);
                            }
                        }

                        ctxIdxInc = (top_bit<<1) + left_bit;

                        if (sd->m_pBitStream->DecodeSingleBin_CABAC(iCtxBase + ctxIdxInc))
                        {
                            sd->m_pBitStream->ResidualBlock4x4_CABAC (BLOCK_CHROMA_AC_LEVELS,
                                                        ctxBase,
                                                        single_scan,
                                                        pPosCoefbuf,
                                                        sd->m_pSliceHeader->scan_idx_end);

                            cbp4x4_chroma |= uBlockBit;

                            pPosCoefbuf += 16;
                        }
                    }
                }
            }
        }

        // update buffer position pointer
        sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon)pPosCoefbuf;
    }

    inline
    int32_t CheckCBP(int32_t iCBP, int32_t iBlockNum)
    {
        return (iCBP & iBlockCBPMask[iBlockNum]) ? (1) : (0);

    } // int32_t CheckCBP(int32_t iCBP, int32_t iBlockNum)

    inline
    int32_t CheckCBPChroma(int32_t iCBP, int32_t iBlockNum)
    {
        return (iCBP & iBlockCBPMaskChroma[iBlockNum]) ? (1) : (0);

    } // int32_t CheckCBPChroma(int32_t iCBP, int32_t iBlockNum)

#define DECODE_EXTERNAL_LUMA_BLOCK_CABAC(x_pos, y_pos, block_number) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrA; \
            mbAddrA = pN->mbs_left[y_pos]; \
            if (-1 < mbAddrA.mb_num) \
            { \
                condTermFlagA = CheckCBP(sd->m_mbinfo.mbs[mbAddrA.mb_num].cbp4x4_luma, \
                                         mbAddrA.block_num); \
            } \
            else \
                condTermFlagA = defTermFlag; \
        } \
        /* create flag for B macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrB; \
            mbAddrB = pN->mb_above; \
            if (-1 < mbAddrB.mb_num) \
            { \
                condTermFlagB = (sd->m_mbinfo.mbs[mbAddrB.mb_num].cbp4x4_luma & 0x0800) ? \
                                (1) : \
                                (0); \
            } \
            else \
                condTermFlagB = defTermFlag; \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(ctxBlockCat, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_TOP_LUMA_BLOCK_CABAC(x_pos, y_pos, block_number, left_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            condTermFlagA = (uFinalCBP & (1 << (left_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create flag for B macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrB; \
            mbAddrB = pN->mb_above; \
            if (-1 < mbAddrB.mb_num) \
            { \
                condTermFlagB = (sd->m_mbinfo.mbs[mbAddrB.mb_num].cbp4x4_luma & (0x0800 << (block_number))) ? \
                                (1) : \
                                (0); \
            } \
            else \
                condTermFlagB = defTermFlag; \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(ctxBlockCat, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_LEFT_LUMA_BLOCK_CABAC(x_pos, y_pos, block_number, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrA; \
            mbAddrA = pN->mbs_left[y_pos]; \
            if (-1 < mbAddrA.mb_num) \
            { \
                condTermFlagA = CheckCBP(sd->m_mbinfo.mbs[mbAddrA.mb_num].cbp4x4_luma, \
                                         mbAddrA.block_num); \
            } \
            else \
                condTermFlagA = defTermFlag; \
        } \
        /* create flag for B macroblock */ \
        { \
            condTermFlagB = (uFinalCBP & (1 << (top_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(ctxBlockCat, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_INTERNAL_LUMA_BLOCK_CABAC(x_pos, y_pos, block_number, left_block_num, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            condTermFlagA = (uFinalCBP & (1 << (left_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create flag for B macroblock */ \
        { \
            condTermFlagB = (uFinalCBP & (1 << (top_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(ctxBlockCat, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_CHROMA_DC_BLOCK_CABAC(component) \
    { \
        int32_t iNumCoeffs = (4 << (3 & (color_format - 1))); \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrA; \
            mbAddrA = pN->mbs_left[0]; \
            if (-1 < mbAddrA.mb_num) \
                condTermFlagA = (sd->m_mbinfo.mbs[mbAddrA.mb_num].cbp4x4_chroma[component] & 1); \
            else \
                condTermFlagA = defTermFlag; \
        } \
        /* create flag for B macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrB; \
            mbAddrB = pN->mb_above; \
            if (-1 < mbAddrB.mb_num) \
                condTermFlagB = (sd->m_mbinfo.mbs[mbAddrB.mb_num].cbp4x4_chroma[component] & 1); \
            else \
                condTermFlagB = defTermFlag; \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            const int32_t * pDCScan; \
            switch (color_format) \
            { \
            case CHROMA_FORMAT_422: \
                pDCScan = ChromaDC422RasterScan; \
                break; \
            case CHROMA_FORMAT_444: \
                pDCScan = mp_scan4x4[0]; \
                break; \
            default: \
                pDCScan = 0; \
                break; \
            } \
            BitStreamColorSpecific<Coeffs, color_format>::ResidualChromaDCBlock_CABAC(ctxBase, \
                                                                                      pDCScan, \
                                                                                      pPosCoefbuf, \
                                                                                      sd->m_pBitStream); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += iNumCoeffs; \
            /* update final CBP */ \
            uFinalCBP[component] = 1; \
        } \
        else \
            uFinalCBP[component] = 0; \
    }

#define DECODE_EXTERNAL_CHROMA_BLOCK_CABAC(x_pos, y_pos, block_number, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrA; \
            mbAddrA = pN->mbs_left_chroma[iComponent][y_pos]; \
            if (-1 < mbAddrA.mb_num) \
            { \
                condTermFlagA = CheckCBPChroma(sd->m_mbinfo.mbs[mbAddrA.mb_num].cbp4x4_chroma[iComponent], \
                                               mbAddrA.block_num - iBlocksBefore); \
            } \
            else \
                condTermFlagA = defTermFlag; \
        } \
        /* create flag for B macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrB; \
            mbAddrB = pN->mb_above_chroma[iComponent]; \
            if (-1 < mbAddrB.mb_num) \
            { \
                condTermFlagB = (sd->m_mbinfo.mbs[mbAddrB.mb_num].cbp4x4_chroma[iComponent] & (1 << (top_block_num + 1))) ? \
                                (1) : \
                                (0); \
            } \
            else \
                condTermFlagB = defTermFlag; \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(BLOCK_CHROMA_AC_LEVELS, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP[iComponent] |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_TOP_CHROMA_BLOCK_CABAC(x_pos, y_pos, block_number, left_block_num, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            condTermFlagA = (uFinalCBP[iComponent] & (1 << (left_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create flag for B macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrB; \
            mbAddrB = pN->mb_above_chroma[iComponent]; \
            if (-1 < mbAddrB.mb_num) \
            { \
                condTermFlagB = (sd->m_mbinfo.mbs[mbAddrB.mb_num].cbp4x4_chroma[iComponent] & (1 << (top_block_num + 1))) ? \
                                (1) : \
                                (0); \
            } \
            else \
                condTermFlagB = defTermFlag; \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(BLOCK_CHROMA_AC_LEVELS, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP[iComponent] |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_LEFT_CHROMA_BLOCK_CABAC(x_pos, y_pos, block_number, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            H264DecoderBlockLocation mbAddrA; \
            mbAddrA = pN->mbs_left_chroma[iComponent][y_pos]; \
            if (-1 < mbAddrA.mb_num) \
            { \
                condTermFlagA = CheckCBPChroma(sd->m_mbinfo.mbs[mbAddrA.mb_num].cbp4x4_chroma[iComponent], \
                                               mbAddrA.block_num - iBlocksBefore); \
            } \
            else \
                condTermFlagA = defTermFlag; \
        } \
        /* create flag for B macroblock */ \
        { \
            condTermFlagB = (uFinalCBP[iComponent] & (1 << (top_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(BLOCK_CHROMA_AC_LEVELS, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP[iComponent] |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

#define DECODE_INTERNAL_CHROMA_BLOCK_CABAC(x_pos, y_pos, block_number, left_block_num, top_block_num) \
    { \
        int32_t ctxIdxInc; \
        int32_t condTermFlagA, condTermFlagB; \
        /* create flag for A macroblock */ \
        { \
            condTermFlagA = (uFinalCBP[iComponent] & (1 << (left_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create flag for B macroblock */ \
        { \
            condTermFlagB = (uFinalCBP[iComponent] & (1 << (top_block_num + 1))) ? \
                            (1) : \
                            (0); \
        } \
        /* create context increment */ \
        ctxIdxInc = condTermFlagA + 2 * condTermFlagB; \
        /* decode block coeffs */ \
        if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag + ctxIdxInc)) \
        { \
            sd->m_pBitStream->ResidualBlock4x4_CABAC(BLOCK_CHROMA_AC_LEVELS, \
                                                     ctxBase, \
                                                     pScan, \
                                                     pPosCoefbuf, \
                                                     iMaxNum); \
            /* update final CBP */ \
            uFinalCBP[iComponent] |= (1 << (block_number + 1)); \
            /* update coefficient buffer pointer */ \
            pPosCoefbuf += 16; \
        } \
    }

    void DecodeCoefficients16x16_CABAC(H264SegmentDecoderMultiThreaded * sd)
    {
        uint32_t iDCCBP = 0;

        const uint32_t *ctxBase;
        const int32_t *pScan;
        bool field_flag = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo) ||
                          sd->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE;

        H264DecoderBlockNeighboursInfo *pN = &sd->m_cur_mb.CurrentBlockNeighbours;

        //this code need additional checking
        if (field_flag)
        {
            ctxBase = ctxIdxOffset4x4FieldCoded;
            pScan = mp_scan4x4[1];
        }
        else
        {
            ctxBase = ctxIdxOffset4x4FrameCoded;
            pScan = mp_scan4x4[0];
        }

        if (sd->m_pSliceHeader->scan_idx_start == 0)
        {

            int32_t iMBAbove, iMBLeft;
            int32_t condTermFlagA, condTermFlagB;
            uint32_t ctxIdxInc, ctxCodedBlockFlag;

            iMBAbove = pN->mb_above.mb_num;
            iMBLeft  = pN->mbs_left[0].mb_num;

            // create flag for A macroblock
            if (0 <= iMBLeft)
                condTermFlagA = (uint32_t) (sd->m_mbinfo.mbs[iMBLeft].cbp4x4_luma & 1);
            else
                condTermFlagA = 1;

            // create flag for B macroblock
            if (0 <= iMBAbove)
                condTermFlagB = (uint32_t) (sd->m_mbinfo.mbs[iMBAbove].cbp4x4_luma & 1);
            else
                condTermFlagB = 1;

            // create context increment
            ctxIdxInc = condTermFlagA + 2 * condTermFlagB;

            // select context for coded block flag
            ctxCodedBlockFlag = ctxBase[CODED_BLOCK_FLAG] +
                                ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_LUMA_DC_LEVELS];

            // set bits for current block
            if (sd->m_pBitStream->DecodeSingleBin_CABAC(ctxCodedBlockFlag +
                                                        ctxIdxInc))
            {
                typedef Coeffs * CoeffsPtr;
                CoeffsPtr pPosCoefbuf = (CoeffsPtr)sd->m_pCoeffBlocksWrite;

                sd->m_pBitStream->ResidualBlock4x4_CABAC(BLOCK_LUMA_DC_LEVELS,
                                                         ctxBase,
                                                         pScan,
                                                         pPosCoefbuf,
                                                         15);
                iDCCBP = 1;
                // update buffer position pointer
                pPosCoefbuf += 16;
                sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon) pPosCoefbuf;

            }
        }

        DecodeCoefficients4x4_CABAC(sd, 14);

        // save final CBP
        sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma |= iDCCBP;
    } // void DecodeCoefficients16x16_CABAC(H264SegmentDecoderMultiThreaded * sd)

    void DecodeCoefficients4x4_CABAC(H264SegmentDecoderMultiThreaded * sd,
                                       int32_t iMaxNum = 15)
    {
        CoeffsPtr pPosCoefbuf = (CoeffsPtr)sd->m_pCoeffBlocksWrite;

        uint32_t cbp = sd->m_cur_mb.LocalMacroblockInfo->cbp;
        const int32_t *pScan;
        bool field_flag = pGetMBFieldDecodingFlag(sd->m_cur_mb.GlobalMacroblockInfo) ||
                          sd->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE;
        H264DecoderBlockNeighboursInfo *pN = &sd->m_cur_mb.CurrentBlockNeighbours;

        int32_t defTermFlag;
        const uint32_t *ctxBase;
        uint32_t ctxBlockCat;
        uint32_t ctxCodedBlockFlag;

        // set default bit
        defTermFlag = (sd->m_cur_mb.GlobalMacroblockInfo->mbtype < MBTYPE_PCM) ? (1) : (0);

        // select context for block data
        ctxBlockCat = (14 == iMaxNum) ? (BLOCK_LUMA_AC_LEVELS) : (BLOCK_LUMA_LEVELS);

        iMaxNum = sd->m_pSliceHeader->scan_idx_end;

        // this code need additional checking
        if (field_flag)
        {
            ctxBase = ctxIdxOffset4x4FieldCoded;
            pScan = mp_scan4x4[1];
        }
        else
        {
            ctxBase = ctxIdxOffset4x4FrameCoded;
            pScan = mp_scan4x4[0];
        }

        // select context for coded block flag
        ctxCodedBlockFlag = (ctxBase[CODED_BLOCK_FLAG] +
                             ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][ctxBlockCat]);


        //
        // decode luminance blocks
        //
        {
            uint32_t uFinalCBP = 0;

            if (cbp & LUMA_BLOCK_8X8_0)
            {
                DECODE_EXTERNAL_LUMA_BLOCK_CABAC(0, 0, 0)
                DECODE_TOP_LUMA_BLOCK_CABAC(1, 0, 1, 0)
                DECODE_LEFT_LUMA_BLOCK_CABAC(0, 1, 2, 0)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(1, 1, 3, 2, 1)
            }

            if (cbp & LUMA_BLOCK_8X8_1)
            {
                DECODE_TOP_LUMA_BLOCK_CABAC(2, 0, 4, 1)
                DECODE_TOP_LUMA_BLOCK_CABAC(3, 0, 5, 4)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(2, 1, 6, 3, 4)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(3, 1, 7, 6, 5)
            }

            if (cbp & LUMA_BLOCK_8X8_2)
            {
                DECODE_LEFT_LUMA_BLOCK_CABAC(0, 2, 8, 2)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(1, 2, 9, 8, 3)
                DECODE_LEFT_LUMA_BLOCK_CABAC(0, 3, 10, 8)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(1, 3, 11, 10, 9)
            }

            if (cbp & LUMA_BLOCK_8X8_3)
            {
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(2, 2, 12, 9, 6)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(3, 2, 13, 12, 7)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(2, 3, 14, 11, 12)
                DECODE_INTERNAL_LUMA_BLOCK_CABAC(3, 3, 15, 14, 13)
            }

            // save final CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_luma = uFinalCBP;
        }

        //
        // decode chrominance blocks
        //

        if (cbp & CHROMA_DC_AC_BLOCKS)
        {
            uint32_t uFinalCBP[2];

            uFinalCBP[0] = uFinalCBP[1] = 0;

            if (sd->m_pSliceHeader->scan_idx_start == 0)
            {
                // select new context
                ctxCodedBlockFlag = ctxBase[CODED_BLOCK_FLAG] +
                                    ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_DC_LEVELS +
                                                                       color_format];

                // decode DC blocks
                DECODE_CHROMA_DC_BLOCK_CABAC(0)
                DECODE_CHROMA_DC_BLOCK_CABAC(1)
            }

            if ((cbp & CHROMA_AC_BLOCKS) && (sd->m_pSliceHeader->scan_idx_end > 0))
            {
                int32_t iComponent;

                // select new context
                ctxCodedBlockFlag = ctxBase[CODED_BLOCK_FLAG] +
                                    ctxIdxBlockCatOffset[CODED_BLOCK_FLAG][BLOCK_CHROMA_AC_LEVELS];

                for (iComponent = 0; iComponent < 2; iComponent += 1)
                {
                    int32_t iBlocksBefore = 16 + iComponent * (2 << color_format);

                    switch (color_format)
                    {
                    case CHROMA_FORMAT_420:
                        DECODE_EXTERNAL_CHROMA_BLOCK_CABAC(0, 0, 0, 2)
                        DECODE_TOP_CHROMA_BLOCK_CABAC(1, 0, 1, 0, 3)
                        DECODE_LEFT_CHROMA_BLOCK_CABAC(0, 1, 2, 0)
                        DECODE_INTERNAL_CHROMA_BLOCK_CABAC(1, 1, 3, 2, 1)
                        break;

                    case CHROMA_FORMAT_422:
                        DECODE_EXTERNAL_CHROMA_BLOCK_CABAC(0, 0, 0, 6)
                        DECODE_TOP_CHROMA_BLOCK_CABAC(1, 0, 1, 0, 7)
                        DECODE_LEFT_CHROMA_BLOCK_CABAC(0, 1, 2, 0)
                        DECODE_INTERNAL_CHROMA_BLOCK_CABAC(1, 1, 3, 2, 1)
                        DECODE_LEFT_CHROMA_BLOCK_CABAC(0, 2, 4, 2)
                        DECODE_INTERNAL_CHROMA_BLOCK_CABAC(1, 2, 5, 4, 3)
                        DECODE_LEFT_CHROMA_BLOCK_CABAC(0, 3, 6, 4)
                        DECODE_INTERNAL_CHROMA_BLOCK_CABAC(1, 3, 7, 6, 5)
                        break;

                    default:
                        break;
                    }
                }
            }

            // set the empty CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = uFinalCBP[0];
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = uFinalCBP[1];
        }
        else
        {
            // set the empty CBP
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[0] = 0;
            sd->m_cur_mb.LocalMacroblockInfo->cbp4x4_chroma[1] = 0;
        }

        // update buffer position pointer
        sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon) pPosCoefbuf;
    } // void DecodeCoefficients4x4_CABAC(H264SegmentDecoderMultiThreaded * sd)
};

template <typename Coeffs, typename PlaneY, typename PlaneUV, int32_t color_format, int32_t is_field>
class ResidualDecoderPCM
{
public:
    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;
    typedef Coeffs *  CoeffsPtr;

    virtual ~ResidualDecoderPCM(){}

    ///////////////////////////////////////////////////////////////////////////////
    // decodePCMCoefficients
    //
    // Extracts raw coefficients from bitstream by:
    //  a) byte aligning bitstream pointer
    //  b) copying bitstream pointer to m_pCoeffBlocksWrite
    //  c) advancing bitstream pointer by 256+128 bytes
    //
    //    Also initializes NumCoef buffers for correct use in future MBs.
    //
    /////////////////////////////
    // Decode the coefficients for a PCM macroblock, placing them
    // in m_pCoeffBlocksBuf.
    void DecodeCoefficients_PCM(H264SegmentDecoderMultiThreaded * sd)
    {
        static const uint32_t num_coeffs[4] = {256,384,512,768};

        uint32_t length = num_coeffs[color_format];
        // number of raw coeff bits
        // to write pointer to non-aligned m_pCoeffBlocksWrite
        sd->m_cur_mb.LocalMacroblockInfo->QP = 0;

        PlanePtrY pCoeffBlocksWrite_Y = reinterpret_cast<PlanePtrY> (sd->m_pCoeffBlocksWrite);

        if (sd->m_pPicParamSet->entropy_coding_mode)
        {
            sd->m_pBitStream->TerminateDecode_CABAC();
        }
        else
        {
            sd->m_pBitStream->AlignPointerRight();
        }

        for (uint32_t i = 0; i < 256; i++)
        {
            pCoeffBlocksWrite_Y[i] = (PlaneY) sd->m_pBitStream->GetBits(sd->bit_depth_luma);
        }

        sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon)((uint8_t*)sd->m_pCoeffBlocksWrite +
                        256*sizeof(PlaneY));

        sd->m_pBitStream->CheckBSLeft();

        if (color_format != CHROMA_FORMAT_400)
        {
            PlanePtrUV pCoeffBlocksWrite_UV = (PlanePtrUV) (sd->m_pCoeffBlocksWrite);
            for (uint32_t i = 0; i < length - 256; i++)
            {
                pCoeffBlocksWrite_UV[i] = (PlaneUV) sd->m_pBitStream->GetBits(sd->bit_depth_chroma);
            }

            sd->m_pCoeffBlocksWrite = (UMC::CoeffsPtrCommon)((uint8_t*)sd->m_pCoeffBlocksWrite +
                            (length - 256)*sizeof(PlaneUV));
        }

        memset(sd->m_cur_mb.GetNumCoeffs()->numCoeffs, 16, sizeof(H264DecoderMacroblockCoeffsInfo));//set correct numcoeffs

        if (sd->m_pPicParamSet->entropy_coding_mode)
        {
            sd->m_pBitStream->InitializeDecodingEngine_CABAC();
        }
        else
        {
            sd->m_pBitStream->AlignPointerRight();
        }
    } // void DecodeCoefficients_PCM(uint8_t color_format)
};

} // namespace UMC

#endif // __UMC_H264_RESIDUAL_DECODER_TEMPLATES_H
#endif // MFX_ENABLE_H264_VIDEO_DECODE