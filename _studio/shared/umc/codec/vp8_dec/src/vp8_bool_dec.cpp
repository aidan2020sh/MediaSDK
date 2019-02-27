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

#include "umc_defs.h"

#if defined (UMC_ENABLE_VP8_VIDEO_DECODER)

#include "vp8_dec_defs.h"
#include "umc_vp8_decoder.h"


using namespace UMC;

namespace UMC
{
/*
static const uint8_t vp8_range_normalization_shift[64] = 
{
  7, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};*/


Status VP8VideoDecoder::InitBooleanDecoder(uint8_t *pBitStream, int32_t dataSize, int32_t dec_number)
{
  if(0 == pBitStream)
    return UMC_ERR_NULL_PTR; 

  if (dec_number >= VP8_MAX_NUMBER_OF_PARTITIONS)
    return UMC_ERR_INVALID_PARAMS;

  if (dataSize < 1)
    return UMC_ERR_INVALID_PARAMS;


  dataSize = MFX_MIN(dataSize, 2);

  vp8BooleanDecoder *pBooldec = &m_BoolDecoder[dec_number];

  pBooldec->pData     = pBitStream;
  pBooldec->data_size = dataSize;
  pBooldec->range     = 255;
  pBooldec->bitcount  = 0;
  pBooldec->value     = 0;

  for(uint8_t i = 0; i < dataSize; i++)
    pBooldec->value = (pBooldec->value <<  (8*i)) | (pBooldec->pData[i]);

  pBooldec->pData    += dataSize;

  return UMC_OK;
}


Status VP8VideoDecoder::UpdateSegmentation(vp8BooleanDecoder *pBooldec)
{
  uint32_t bits;

  int32_t i;
  int32_t j;
  int32_t res;

  bits = DecodeValue_Prob128(pBooldec, 2);

  m_FrameInfo.updateSegmentMap  = (uint8_t)bits >> 1;
  m_FrameInfo.updateSegmentData = (uint8_t)bits & 1;

  if (m_FrameInfo.updateSegmentData)
  {
    VP8_DECODE_BOOL_PROB128(pBooldec, m_FrameInfo.segmentAbsMode);
    UMC_SET_ZERO(m_FrameInfo.segmentFeatureData);

    for (i = 0; i < VP8_NUM_OF_MB_FEATURES; i++)
    {
      for (j = 0; j < VP8_MAX_NUM_OF_SEGMENTS; j++)
      {
        VP8_DECODE_BOOL_PROB128(pBooldec, bits);

        if (bits)
        {
          bits = DecodeValue_Prob128(pBooldec, 8-i); // 7 bits for ALT_QUANT, 6 - for ALT_LOOP_FILTER; +sign
          res = bits >> 1;

          if (bits & 1)
            res = -res;

          m_FrameInfo.segmentFeatureData[i][j] = (int8_t)res;
        }
      }
    }
  }

  if (m_FrameInfo.updateSegmentMap)
  {
    for (i = 0; i < VP8_NUM_OF_SEGMENT_TREE_PROBS; i++)
    {
      VP8_DECODE_BOOL_PROB128(pBooldec, bits);

      if (bits)
        bits = DecodeValue_Prob128(pBooldec, 8);
      else
        bits = 255;

      m_FrameInfo.segmentTreeProbabilities[i] = (uint8_t)bits;
    }
  }

  return UMC_OK;
} // VP8VideoDecoder::UpdateSegmentation()



Status VP8VideoDecoder::UpdateLoopFilterDeltas(vp8BooleanDecoder *pBooldec)
{
  uint8_t  bits;
  int32_t i;
  int32_t res;

  for (i = 0; i < VP8_NUM_OF_REF_FRAMES; i++)
  {
    VP8_DECODE_BOOL(pBooldec, 128, bits);
    if (bits)
    {
      bits = DecodeValue_Prob128(pBooldec, 7);
      res = bits >> 1;

      if (bits & 1)
        res = -res;

      m_FrameInfo.refLoopFilterDeltas[i] = (int8_t)res;
    }
  }

  for (i = 0; i < VP8_NUM_OF_MODE_LF_DELTAS; i++)
  {
    VP8_DECODE_BOOL(pBooldec, 128, bits);
    if (bits)
    {
      bits = DecodeValue_Prob128(pBooldec, 7);
      res = bits >> 1;

      if (bits & 1)
        res = -res;

      m_FrameInfo.modeLoopFilterDeltas[i] = (int8_t)res;
    }
  }

  return UMC_OK;
} // VP8VideoDecoder::UpdateLoopFilterDeltas()



int32_t vp8_ReadTree(vp8BooleanDecoder *pBooldec, const int8_t *pTree, const uint8_t *pProb)
{
  int32_t i;
  uint8_t bit;
  for (i = 0;;)
  {
    VP8_DECODE_BOOL(pBooldec, pProb[i >> 1], bit);
    i = pTree[i + bit];

    if (i <= 0)
      break;
  }

  return -i;
} // vp8_ReadTree



uint8_t VP8VideoDecoder::DecodeValue_Prob128(vp8BooleanDecoder *pBooldec, uint32_t numbits)
{
  int32_t n;

  uint8_t val = 0;
  uint8_t bit = 0;

  for(n = numbits - 1; n >= 0; n--)
  {
    VP8_DECODE_BOOL(pBooldec,128, bit);
    val = val << 1 | bit;
  }

  return val;
}


uint8_t VP8VideoDecoder::DecodeValue(vp8BooleanDecoder *pBooldec, uint8_t prob, uint32_t numbits)
{
  int32_t n;

  uint8_t val = 0;
  uint8_t bit = 0;

  for(n = numbits - 1; n >= 0; n--)
  {
    VP8_DECODE_BOOL(pBooldec, prob, bit);
    val = val << 1 | bit;
  }

  return val;
}


} // namespace UMC
#endif // UMC_ENABLE_VP8_VIDEO_DECODER