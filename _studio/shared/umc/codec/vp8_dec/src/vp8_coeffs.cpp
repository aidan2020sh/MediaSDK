// Copyright (c) 2014-2018 Intel Corporation
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


#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "umc_vp8_decoder.h"

using namespace UMC;

namespace UMC
{

void VP8VideoDecoder::UpdateCoeffProbabilitites(void)
{
  int32_t i;
  int32_t j;
  int32_t k;
  int32_t l;

  vp8BooleanDecoder *pBoolDec = &m_BoolDecoder[0];

  for (i = 0; i < VP8_NUM_COEFF_PLANES; i++)
  {
    for (j = 0; j < VP8_NUM_COEFF_BANDS; j++)
    {
      for (k = 0; k < VP8_NUM_LOCAL_COMPLEXITIES; k++)
      {
        for (l = 0; l < VP8_NUM_COEFF_NODES; l++)
        {
          uint8_t flag;
          uint8_t prob = vp8_coeff_update_probs[i][j][k][l];

          VP8_DECODE_BOOL(pBoolDec, prob, flag);

          if (flag)
            m_FrameProbs.coeff_probs[i][j][k][l] = (uint8_t)DecodeValue(pBoolDec,128, 8);
        }
      }
    }
  }
} // VP8VideoDecoder::UpdateCoeffProbabilitites()



void VP8VideoDecoder::DecodeMbRow(vp8BooleanDecoder *pBoolDec, int32_t mb_row)
{
  uint32_t mb_col        = 0;
  uint32_t nonzeroMb;
  vp8_MbInfo* currMb   = 0;

  vp8BooleanDecoder* bool_dec = pBoolDec;
  memset(m_FrameInfo.blContextLeft, 0, 9);


  for(mb_col = 0; mb_col < m_FrameInfo.mbPerRow; mb_col++)
  {
    currMb = &m_pMbInfo[mb_row * m_FrameInfo.mbStep + mb_col];

    if(0 == currMb->skipCoeff)
    {
      nonzeroMb = DecodeMbCoeffs(bool_dec, mb_row, mb_col);

      if(!nonzeroMb)
      {
        currMb->skipCoeff = 1;// ??? if skipCoeff == 0 and no non-zero
                                          // coeffs in Mb we set skipCoeff=1 manualy (but need or not???)
      }
    }
    else
    {
      // reset blContextLeft/blContextUp
      if(currMb->mode != VP8_MV_SPLIT && currMb->mode != VP8_MB_B_PRED)
      {
        memset(m_FrameInfo.blContextLeft,          0, 9);
        memset(m_FrameInfo.blContextUp + 9*mb_col, 0, 9);
      }
      else
      {
        memset(m_FrameInfo.blContextLeft,          0, 9-1); // not need to set Y2 to "0" in this case????
        memset(m_FrameInfo.blContextUp + 9*mb_col, 0, 9-1);

      }
    }
  }
} //  VP8VideoDecoder::DecodeMbRow(()


// bits 0-3 - up; bits 4-7 - left
static const uint8_t vp8_blockContexts[25] = 
{
  (8 << 4) | 8,
  (0 << 4) | 0, (0 << 4) | 1, (0 << 4) | 2, (0 << 4) | 3,
  (1 << 4) | 0, (1 << 4) | 1, (1 << 4) | 2, (1 << 4) | 3,
  (2 << 4) | 0, (2 << 4) | 1, (2 << 4) | 2, (2 << 4) | 3,
  (3 << 4) | 0, (3 << 4) | 1, (3 << 4) | 2, (3 << 4) | 3,

  (4 << 4) | 4, (4 << 4) | 5,
  (5 << 4) | 4, (5 << 4) | 5,

  (6 << 4) | 6, (6 << 4) | 7,
  (7 << 4) | 6, (7 << 4) | 7
};

static const int32_t vp8_coefBands[16] =
{
  0, 1, 2, 3,
  6, 4, 5, 6,
  6, 6, 6, 6,
  6, 6, 6, 7
};

static const int32_t vp8_zigzag_scan[16] =
{
  0, 1,  4,  8,
  5, 2,  3,  6,
  9, 12, 13, 10,
  7, 11, 14, 15
};


#define VP8_GET_SIGNED_COEFF(ret, absval) \
{ \
  uint32_t split, split256; \
  split    = (range + 1) >> 1; \
  split256 = (split << 8); \
  if (value < split256) \
  { \
    range = split; \
    ret   = absval; \
  } \
  else \
  { \
    range -= split; \
    value -= split256; \
    ret    = -(absval); \
  } \
  { \
    bitcount++; \
    range <<= 1; \
    value <<= 1; \
    if (bitcount == 8) \
    { \
      bitcount = 0; \
      value   |= *pData; \
      pData++; \
    } \
  } \
}


#define VP8_DECODE_COEFF_EXTRA(prob, res, extra) \
{ \
  uint32_t split, split256; \
  split    = 1 + (((range - 1) * (prob)) >> 8); \
  split256 = (split << 8); \
  if (value < split256) \
  { \
    range = split; \
  } \
  else \
  { \
    range -= split; \
    value -= split256; \
    res   += (extra); \
  } \
  if (range < 128) \
  { \
    uint32_t shift = vp8_range_normalization_shift[range >> 1]; \
    range   <<= shift; \
    value   <<= shift; \
    bitcount += shift; \
    if (bitcount >= 8) \
    { \
      bitcount -= 8; \
      value    |= (*pData) << bitcount; \
      pData++; \
    } \
  } \
}

#define VP8_COEFF_CAT1_PROB 159

#define VP8_COEFF_CAT2_PROB2 165
#define VP8_COEFF_CAT2_PROB1 145

#define VP8_COEFF_CAT3_PROB3 173
#define VP8_COEFF_CAT3_PROB2 148
#define VP8_COEFF_CAT3_PROB1 140

#define VP8_COEFF_CAT4_PROB4 176
#define VP8_COEFF_CAT4_PROB3 155
#define VP8_COEFF_CAT4_PROB2 140
#define VP8_COEFF_CAT4_PROB1 135

#define VP8_COEFF_CAT5_PROB5 180
#define VP8_COEFF_CAT5_PROB4 157
#define VP8_COEFF_CAT5_PROB3 141
#define VP8_COEFF_CAT5_PROB2 134
#define VP8_COEFF_CAT5_PROB1 130

const uint8_t vp8_coeff_cat5_probs[5] = 
{
  180, 157, 141, 134, 130
};

const uint8_t vp8_coeff_cat6_probs[11] = 
{
  254, 254, 243, 230, 196, 177, 153, 140, 133, 130, 129
};


// check if coeffs are initially zeroed !!! ???
int32_t VP8VideoDecoder::DecodeMbCoeffs(vp8BooleanDecoder *pBoolDec, int32_t mb_row, int32_t mb_col)
{
  uint8_t      *pCtxtUp    = m_FrameInfo.blContextUp + mb_col*9;
  uint8_t      *pCtxtLeft  = m_FrameInfo.blContextLeft;
  vp8_MbInfo *pMb        = &m_pMbInfo[mb_row*m_FrameInfo.mbStep + mb_col];
  int16_t     *pCoeffs;
  uint8_t       (*pppProb)[VP8_NUM_LOCAL_COMPLEXITIES][VP8_NUM_COEFF_NODES];
  uint8_t      *pProb;
  int32_t      bl;
  uint32_t      i;
  uint32_t      firstCoeff = 0;
  int32_t      firstBl;
  uint32_t      j;
  int32_t      nonzeroMb  = 0;

  // for local bool_decoder
  uint32_t  range    = pBoolDec->range;
  uint32_t  value    = pBoolDec->value;
  int32_t  bitcount = pBoolDec->bitcount;
  uint8_t  *pData    = pBoolDec->pData;
  uint8_t   bit;

  UMC_SET_ZERO(pMb->numNNZ);

  if (pMb->mode != VP8_MV_SPLIT && pMb->mode != VP8_MB_B_PRED)
  {
    // Y2 subblock is present
    pppProb    = m_FrameProbs.coeff_probs[1];
    firstBl    = 0;
  }
  else
  {
    // no Y2 subblock
    firstBl    = 1;
    pppProb    = m_FrameProbs.coeff_probs[3];
  }

  pCoeffs = &pMb->coeffs[firstBl << 4];

  for (bl = firstBl; bl < 25; bl++)
  {
    uint8_t  ctxtInd     = vp8_blockContexts[bl];
    uint8_t *pcUp        = pCtxtUp   + (ctxtInd & 0xF);
    uint8_t *pcLeft      = pCtxtLeft + (ctxtInd >> 4);
    int32_t prevNotZero = 1;
    int32_t coef;

    ctxtInd = (*pcUp ? 1 : 0) + (*pcLeft ? 1 : 0);

    for (i = firstCoeff; i < 16; i++)
    {
      pProb = pppProb[vp8_coefBands[i]][ctxtInd];
      if (prevNotZero)
      {
        VP8_DECODE_BOOL_LOCAL(pProb[VP8_COEFF_NODE_EOB], bit);
        if (bit == 0)
          break;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[VP8_COEFF_NODE_0], bit);
      if (bit == 0)
      {
        prevNotZero = 0;

        ctxtInd = 0;

        continue;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[VP8_COEFF_NODE_1], bit);
      if (bit == 0)
      {
        VP8_GET_SIGNED_COEFF(coef, 1);
        pCoeffs[vp8_zigzag_scan[i]] = (int16_t)coef;

        ctxtInd     = 1;
        prevNotZero = 1;

        continue;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[3], bit);
      if (bit == 0)
      {
        VP8_DECODE_BOOL_LOCAL(pProb[4], bit);
        if (bit == 0)
        {
          VP8_GET_SIGNED_COEFF(coef, 2);
        }
        else
        {
          VP8_DECODE_BOOL_LOCAL(pProb[5], bit);
          VP8_GET_SIGNED_COEFF(coef, (3 + bit));
        }

        pCoeffs[vp8_zigzag_scan[i]] = (int16_t)coef;

        ctxtInd     = 2;
        prevNotZero = 1;

        continue;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[6], bit);
      if (bit == 0)
      {
        VP8_DECODE_BOOL_LOCAL(pProb[7], bit);
        if (bit == 0)
        {
          coef = 5;
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT1_PROB, coef, 1);
        }
        else
        {
          coef = 7;
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT2_PROB2, coef, 2);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT2_PROB1, coef, 1);
        }

        VP8_GET_SIGNED_COEFF(coef, coef);
        pCoeffs[vp8_zigzag_scan[i]] = (int16_t)coef;

        ctxtInd     = 2;
        prevNotZero = 1;

        continue;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[8], bit);
      if (bit == 0)
      {
        VP8_DECODE_BOOL_LOCAL(pProb[9], bit);
        if (bit == 0)
        {
          coef = 11;
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT3_PROB3, coef, 4);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT3_PROB2, coef, 2);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT3_PROB1, coef, 1);
        }
        else
        {
          coef = 19;
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT4_PROB4, coef, 8);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT4_PROB3, coef, 4);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT4_PROB2, coef, 2);
          VP8_DECODE_COEFF_EXTRA(VP8_COEFF_CAT4_PROB1, coef, 1);
        }

        VP8_GET_SIGNED_COEFF(coef, coef);
        pCoeffs[vp8_zigzag_scan[i]] = (int16_t)coef;

        ctxtInd     = 2;
        prevNotZero = 1;

        continue;
      }

      VP8_DECODE_BOOL_LOCAL(pProb[10], bit);
      if (bit == 0)
      {
        coef = 35;

        for (j = 0; j < 5; j++)
        {
          VP8_DECODE_COEFF_EXTRA(vp8_coeff_cat5_probs[j], coef, (16 >> j));
        }
      }
      else
      {
        coef = 67;
        for (j = 0; j < 11; j++)
        {
          VP8_DECODE_COEFF_EXTRA(vp8_coeff_cat6_probs[j], coef, (1024 >> j));
        }
      }

      VP8_GET_SIGNED_COEFF(coef, coef);
      pCoeffs[vp8_zigzag_scan[i]] = (int16_t)coef;

      ctxtInd     = 2;
      prevNotZero = 1;
    }

    pCoeffs   += 16;
    bit        = (i != firstCoeff);
    *pcUp      = *pcLeft = bit;
    nonzeroMb |= bit;

    pMb->numNNZ[bl] = uint8_t((bit != 0) ? i : 0);

    if (bl == 0)
    {
      ctxtInd    = 0; // ?????? not sure of it
      firstCoeff = 1;
      pppProb = m_FrameProbs.coeff_probs[0]; // Y after Y2
    }

    if (bl == 16)
    {
      ctxtInd    = 0;
      firstCoeff = 0;
      pppProb    = m_FrameProbs.coeff_probs[2]; // U or V
    }
  }

  pBoolDec->range    = range;
  pBoolDec->value    = value;
  pBoolDec->bitcount = bitcount;
  pBoolDec->pData    = pData;

  return nonzeroMb;
} // VP8VideoDecoder::DecodeMbCoeffs()


} // namespace UMC
#endif // MFX_ENABLE_VP8_VIDEO_DECODE