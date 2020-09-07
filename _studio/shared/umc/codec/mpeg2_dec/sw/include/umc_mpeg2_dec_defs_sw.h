// Copyright (c) 2003-2020 Intel Corporation
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

#pragma once
#include "umc_defs.h"
#if defined (MFX_ENABLE_MPEG2_VIDEO_DECODE)

#include "umc_mpeg2_dec_defs.h"
#include <ippi.h>
#include <ippvc.h>
#include "umc_structures.h"
#include "umc_memory_allocator.h"
#include "umc_mpeg2_dec_bstream.h"

#include <vector>

#define MISMATCH_INTRA
//#define USE_INTRINSICS
//#define IPP_DIRECT_CALLS
//#define MPEG2_USE_REF_IDCT

#if (defined(IPP_DIRECT_CALLS_W7) || defined(IPP_DIRECT_CALLS_A6))&&(!defined(_WIN32_WCE))
#define IPP_DIRECT_CALLS
#endif


namespace UMC
{

#define IPPVC_MC_FRAME     0x2
#define IPPVC_MC_FIELD     0x1
#define IPPVC_MC_16X8      0x2
#define IPPVC_MC_DP        0x3

#define IPPVC_DCT_FIELD    0x0
#define IPPVC_DCT_FRAME    0x1

#define IPPVC_MB_INTRA     0x1
#define IPPVC_MB_PATTERN   0x2
#define IPPVC_MB_BACKWARD  0x4
#define IPPVC_MB_FORWARD   0x8
#define IPPVC_MB_QUANT     0x10

#define ROW_CHROMA_SHIFT_420 3
#define ROW_CHROMA_SHIFT_422 4
#define COPY_CHROMA_MB_420 ippiCopy8x8_8u_C1R
#define COPY_CHROMA_MB_422 ippiCopy8x16_8u_C1R

///////////////////////

} // namespace UMC

#ifdef _MSVC_LANG
#pragma warning(default: 4324)
#endif

/*******************************************************/

#ifdef __cplusplus
extern "C" {
#endif

struct mp2_VLCTable
{
  int32_t max_bits;
  int32_t bits_table0;
  int32_t bits_table1;
  uint32_t threshold_table0;
  int16_t *table0;
  int16_t *table1;
};

#define ippiCopy8x16_8u_C1R(pSrc, srcStep, pDst, dstStep)                   \
  ippiCopy8x8_8u_C1R(pSrc, srcStep, pDst, dstStep);                         \
  ippiCopy8x8_8u_C1R(pSrc + 8*(srcStep), srcStep, pDst + 8*(dstStep), dstStep);

#define ippiCopy8x16HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding)  \
  ippiCopy8x8HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding);        \
  ippiCopy8x8HP_8u_C1R(pSrc + 8 * srcStep, srcStep, pDst + 8 * dstStep, dstStep, acc, rounding);

#define ippiInterpolateAverage8x16_8u_C1IR(pSrc, srcStep, pDst, dstStep, acc, rounding)  \
  ippiInterpolateAverage8x8_8u_C1IR(pSrc, srcStep, pDst, dstStep, acc, rounding);        \
  ippiInterpolateAverage8x8_8u_C1IR(pSrc + 8 * srcStep, srcStep, pDst + 8 * dstStep, dstStep, acc, rounding);

/********************************************************************************/

typedef void (*ownvc_CopyHP_8u_C1R_func) (const uint8_t *pSrc, int32_t srcStep, uint8_t *pDst, int32_t dstStep);
typedef void (*ownvc_AverageHP_8u_C1R_func) (const uint8_t *pSrc, int32_t srcStep, uint8_t *pDst, int32_t dstStep);

/********************************************************************************/

#ifndef IPP_DIRECT_CALLS

#define FUNC_COPY_HP(W, H, pSrc, srcStep, pDst, dstStep, acc, rounding) \
  ippiCopy##W##x##H##HP_8u_C1R(pSrc, srcStep, pDst, dstStep, acc, rounding)

#define FUNC_AVE_HP(W, H, pSrc, srcStep, pDst, dstStep, mcType, roundControl) \
  ippiInterpolateAverage##W##x##H##_8u_C1IR(pSrc, srcStep, pDst, dstStep, mcType, roundControl);

#define FUNC_AVE_HP_B(W, H, pSrcRefF, srcStepF, mcTypeF, pSrcRefB, srcStepB, mcTypeB, \
                      pDst, dstStep, roundControl) \
  FUNC_COPY_HP(W, H, pSrcRefF, srcStepF, pDst, dstStep, mcTypeF, roundControl); \
  FUNC_AVE_HP(W, H, pSrcRefB, srcStepB, pDst, dstStep, mcTypeB, roundControl)

#else

extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x4HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x8HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy16x8HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy16x16HP_8u_C1R[4];
extern const ownvc_CopyHP_8u_C1R_func ownvc_Copy8x16HP_8u_C1R[4];

extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x4HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x8HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average16x8HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average16x16HP_8u_C1R[4];
extern const ownvc_AverageHP_8u_C1R_func ownvc_Average8x16HP_8u_C1R[4];

#define FUNC_COPY_HP(W, H, pSrc, srcStep, pDst, dstStep, acc, rounding) \
  ownvc_Copy##W##x##H##HP_8u_C1R[acc](pSrc, srcStep, pDst, dstStep);

#define FUNC_AVE_HP(W, H, pSrc, srcStep, pDst, dstStep, mcType, roundControl) \
  ownvc_Average##W##x##H##HP_8u_C1R[mcType](pSrc, srcStep, pDst, dstStep);

#define FUNC_AVE_HP_B(W, H, pSrcRefF, srcStepF, mcTypeF, pSrcRefB, srcStepB, mcTypeB, \
                      pDst, dstStep, roundControl) \
  FUNC_COPY_HP(W, H, pSrcRefF, srcStepF, pDst, dstStep, mcTypeF, roundControl); \
  FUNC_AVE_HP(W, H, pSrcRefB, srcStepB, pDst, dstStep, mcTypeB, roundControl)

#endif /* IPP_DIRECT_CALLS */

/********************************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif

/*******************************************************/

#define HP_FLAG_MC(flag, x, y) \
  flag = ((x + x) & 2) | (y & 1); \
  flag = (flag << 2)

#define HP_FLAG_CP(flag, x, y) \
  flag = (x & 1) | ((y + y) & 2)

#define HP_FLAG_AV  HP_FLAG_CP

#define CHECK_OFFSET_L(offs, pitch, hh) \
  if (offs < 0 || (int32_t)(offs+(hh-1)*(pitch)+15) > (int32_t)video->pic_size) \
{ \
  return UMC_ERR_INVALID_STREAM; \
}

#define CALC_OFFSETS_FRAME_420(offs_l, offs_c, flag_l, flag_c, xl, yl, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl/2; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FULLPEL(offs_l, offs_c, xl, yl, pitch_l, pitch_c) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl/2; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
}

#define CALC_OFFSETS_FIELD_420(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl/2; \
\
  offs_l = video->offset_l + ((yl &~ 1) + field_sel)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + ((yc &~ 1) + field_sel)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELDX_420(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl/2; \
  offs_l = ((yl &~ 1) + 2*video->row_l + field_sel)*pitch_l + (xl >> 1) + video->col_l; \
  offs_c = ((yc &~ 1) + 2*video->row_c + field_sel)*pitch_c + (xc >> 1) + video->col_c; \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FRAME_422(offs_l, offs_c, flag_l, flag_c, xl, yl, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl; \
\
  offs_l = video->offset_l + (yl >> 1)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + (yc >> 1)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELD_422(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl; \
\
  offs_l = video->offset_l + ((yl &~ 1) + field_sel)*pitch_l + (xl >> 1); \
  offs_c = video->offset_c + ((yc &~ 1) + field_sel)*pitch_c + (xc >> 1); \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define CALC_OFFSETS_FIELDX_422(offs_l, offs_c, flag_l, flag_c, xl, yl, field_sel, HP_FLAG) \
{ \
  int32_t xc = xl/2; \
  int32_t yc = yl; \
  offs_l = ((yl &~ 1) + 2*video->row_l + field_sel)*pitch_l + (xl >> 1) + video->col_l; \
  offs_c = ((yc &~ 1) + 2*video->row_c + field_sel)*pitch_c + (xc >> 1) + video->col_c; \
  HP_FLAG(flag_l, xl, yl); \
  HP_FLAG(flag_c, xc, yc); \
}

#define MCZERO_FRAME(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 16, ref_Y_data1,pitch_l,     \
                          cur_Y_data,                   \
                          pitch_l, flag1, 0);           \
  FUNC_##ADD##_HP(8, 8, ref_U_data1,pitch_c,     \
                              cur_U_data,               \
                              pitch_c, flag2, 0);      \
  FUNC_##ADD##_HP(8, 8, ref_V_data1,pitch_c,     \
                              cur_V_data,               \
                              pitch_c, flag2, 0);

#define MCZERO_FRAME_422(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 16, ref_Y_data1,pitch_l,     \
                          cur_Y_data,                   \
                          pitch_l, flag1, 0);           \
  FUNC_##ADD##_HP(8, 16, ref_U_data1,pitch_c,     \
                              cur_U_data,               \
                              pitch_c, flag2, 0);      \
  FUNC_##ADD##_HP(8, 16, ref_V_data1,pitch_c,     \
                              cur_V_data,               \
                              pitch_c, flag2, 0);

#define MCZERO_FIELD0(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data1, pitch_l2,      \
                      cur_Y_data, pitch_l2,            \
                      flag1, 0);                        \
  FUNC_##ADD##_HP(8, 4, ref_U_data1, pitch_c2,     \
                      cur_U_data, pitch_c2,       \
                      flag2, 0);                    \
  FUNC_##ADD##_HP(8, 4, ref_V_data1, pitch_c2,     \
                      cur_V_data, pitch_c2,       \
                      flag2, 0);

#define MCZERO_FIELD0_422(ADD, ref_Y_data1, ref_U_data1, ref_V_data1, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data1, pitch_l2,      \
                      cur_Y_data, pitch_l2,            \
                      flag1, 0);                        \
  FUNC_##ADD##_HP(8, 8, ref_U_data1, pitch_c2,     \
                      cur_U_data, pitch_c2,       \
                      flag2, 0);                    \
  FUNC_##ADD##_HP(8, 8, ref_V_data1, pitch_c2,     \
                      cur_V_data, pitch_c2,       \
                      flag2, 0);

#define MCZERO_FIELD1(ADD, ref_Y_data2, ref_U_data2, ref_V_data2, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data2, pitch_l2,       \
                      cur_Y_data + pitch_l, pitch_l2,   \
                      flag1, 0);                         \
  FUNC_##ADD##_HP(8, 4, ref_U_data2, pitch_c2,          \
                      cur_U_data + pitch_c, pitch_c2, \
                      flag2, 0);                         \
  FUNC_##ADD##_HP(8, 4, ref_V_data2, pitch_c2,          \
                      cur_V_data + pitch_c, pitch_c2, \
                      flag2, 0);

#define MCZERO_FIELD1_422(ADD, ref_Y_data2, ref_U_data2, ref_V_data2, flag1, flag2) \
  FUNC_##ADD##_HP(16, 8, ref_Y_data2, pitch_l2,       \
                      cur_Y_data + pitch_l, pitch_l2,   \
                      flag1, 0);                         \
  FUNC_##ADD##_HP(8, 8, ref_U_data2, pitch_c2,          \
                      cur_U_data + pitch_c, pitch_c2, \
                      flag2, 0);                         \
  FUNC_##ADD##_HP(8, 8, ref_V_data2, pitch_c2,          \
                      cur_V_data + pitch_c, pitch_c2, \
                      flag2, 0);


#define COPY_PMV(adst,asrc) {                                   \
  for(unsigned int nn=0; nn<sizeof(adst)/sizeof(int32_t); nn++)  \
    ((int32_t*)adst)[nn] = ((int32_t*)asrc)[nn];                  \
}

#define RECONSTRUCT_INTRA_MB(BITSTREAM, NUM_BLK, DCT_TYPE)               \
{                                                                        \
  int32_t *pitch = video->blkPitches[DCT_TYPE];                                  \
  int32_t *offsets = video->blkOffsets[DCT_TYPE];                                \
  int32_t curr_index = frame_buffer.curr_index[task_num];               \
  uint8_t* yuv[3] = {                                                      \
    frame_buffer.frame_p_c_n[curr_index].Y_comp_data + video->offset_l,  \
    frame_buffer.frame_p_c_n[curr_index].U_comp_data + video->offset_c,  \
    frame_buffer.frame_p_c_n[curr_index].V_comp_data + video->offset_c   \
  };                                                                     \
  int32_t blk;                                                            \
  IppiDecodeIntraSpec_MPEG2 *intraSpec = &video->decodeIntraSpec;        \
                                                                         \
  for (blk = 0; blk < NUM_BLK; blk++) {                                  \
    IppStatus sts;                                                       \
    int32_t chromaFlag, cc;                                               \
    chromaFlag = blk >> 2;                                               \
    cc = chromaFlag + (blk & chromaFlag);                                \
    CHR_SPECINTRA_##NUM_BLK                                              \
                                                                         \
    sts = ippiDecodeIntra8x8IDCT_MPEG2_1u8u(                             \
      &BITSTREAM##_curr_ptr,                                             \
      &BITSTREAM##_bit_offset,                                           \
      intraSpec,                                                         \
      video->cur_q_scale,                                                \
      chromaFlag,                                                        \
      &video->dct_dc_past[cc],                                           \
      yuv[cc] + offsets[blk],                                            \
      pitch[chromaFlag]);                                                \
    if(sts != ippStsOk)                                                  \
      return sts;                                                        \
  }                                                                      \
}

#define CHR_SPECINTRA_6
#define CHR_SPECINTRA_8 intraSpec = chromaFlag ? &video->decodeIntraSpecChroma : &video->decodeIntraSpec; \

#define RECONSTRUCT_INTRA_MB_420(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTRA_MB(BITSTREAM, 6, DCT_TYPE)

#define RECONSTRUCT_INTRA_MB_422(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTRA_MB(BITSTREAM, 8, DCT_TYPE)


#define DECODE_MBPATTERN_6(code, BITSTREAM, vlcMBPattern)                \
  DECODE_VLC(code, BITSTREAM, vlcMBPattern)

#define DECODE_MBPATTERN_8(code, BITSTREAM, vlcMBPattern)                \
{                                                                        \
  int32_t cbp_1;                                                          \
  DECODE_VLC(code, BITSTREAM, vlcMBPattern);                             \
  GET_TO9BITS(video->bs, 2, cbp_1);                                      \
  code = (code << 2) | cbp_1;                                            \
}

#define RECONSTRUCT_INTER_MB(BITSTREAM, NUM_BLK, DCT_TYPE)               \
{                                                                        \
  IppiDecodeInterSpec_MPEG2 *interSpec = &video->decodeInterSpec;        \
  int32_t cur_q_scale = video->cur_q_scale;                               \
  int32_t *pitch = video->blkPitches[DCT_TYPE];                           \
  int32_t *offsets = video->blkOffsets[DCT_TYPE];                         \
  int32_t mask = 1 << (NUM_BLK - 1);                                      \
  int32_t code;                                                           \
  int32_t blk;                                                            \
                                                                         \
  DECODE_MBPATTERN_##NUM_BLK(code, BITSTREAM, vlcMBPattern);             \
                                                                         \
  for (blk = 0; blk < NUM_BLK; blk++) {                                  \
    if (code & mask) {                                                   \
      IppStatus sts;                                                     \
      int32_t chromaFlag = blk >> 2;                                      \
      int32_t cc = chromaFlag + (blk & chromaFlag);                       \
      CHR_SPECINTER_##NUM_BLK                                                 \
                                                                         \
      sts = ippiDecodeInter8x8IDCTAdd_MPEG2_1u8u(                        \
        &BITSTREAM##_curr_ptr,                                           \
        &BITSTREAM##_bit_offset,                                         \
        interSpec,                                                       \
        cur_q_scale,                                                     \
        video->blkCurrYUV[cc] + offsets[blk],                            \
        pitch[chromaFlag]);                                              \
      if(sts != ippStsOk)                                                \
        return sts;                                                      \
    }                                                                    \
    code += code;                                                        \
  }                                                                      \
}

#define CHR_SPECINTER_6
#define CHR_SPECINTER_8 interSpec = chromaFlag ? &video->decodeInterSpecChroma : &video->decodeInterSpec;

#define RECONSTRUCT_INTER_MB_420(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTER_MB(BITSTREAM, 6, DCT_TYPE)

#define RECONSTRUCT_INTER_MB_422(BITSTREAM, DCT_TYPE) \
  RECONSTRUCT_INTER_MB(BITSTREAM, 8, DCT_TYPE)

#define UPDATE_MV(val, mcode, S, task_num) \
  if(PictureHeader[task_num].r_size[S]) { \
    GET_TO9BITS(video->bs,PictureHeader[task_num].r_size[S], residual); \
    if(mcode < 0) { \
        val += ((mcode + 1) << PictureHeader[task_num].r_size[S]) - (residual + 1); \
        if(val < PictureHeader[task_num].low_in_range[S]) \
          val += PictureHeader[task_num].range[S]; \
    } else { \
        val += ((mcode - 1) << PictureHeader[task_num].r_size[S]) + (residual + 1); \
        if(val > PictureHeader[task_num].high_in_range[S]) \
          val -= PictureHeader[task_num].range[S]; \
    } \
  } else { \
    val += mcode; \
    if(val < PictureHeader[task_num].low_in_range[S]) \
      val += PictureHeader[task_num].range[S]; \
    else if(val > PictureHeader[task_num].high_in_range[S]) \
      val -= PictureHeader[task_num].range[S]; \
  }
#ifdef FIX_CUST_BUG_24419
#define VECTOR_BOUNDS_CORRECT_X(vX,task_num)\
    if(video->mb_col*16 + (vX >> 1) < 0) {          \
        vX = vX; }                            \
    if((video->mb_col + 1) * 16 + (vX >> 1) > sequenceHeader.mb_width[task_num] * 16) { \
        vX = (sequenceHeader.mb_width[task_num] - video->mb_col - 1) * 16 - 1;  \
  }
#endif

#ifdef FIX_CUST_BUG_24419
#define DECODE_MV(BS, R, S, vectorX, vectorY, task_num)           \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video, task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
                                                        \
  /* Decode y vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R + 1], S + 1, video, task_num);        \
  }                                                     \
  vectorY = video->PMV[R + 1];

#define DECODE_MV_FIELD(BS, R, S, vectorX, vectorY, task_num)     \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video,task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (int16_t)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (int16_t)(vectorY << 1)


#define DECODE_MV_FULLPEL(BS, R, S, vectorX, vectorY, task_num)   \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  vectorX = (int16_t)(video->PMV[R] >> 1);               \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorX, S, video, task_num);                      \
  }                                                     \
  VECTOR_BOUNDS_CORRECT_X(vectorX,task_num);            \
  video->PMV[R] = (int16_t)(vectorX << 1);               \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (int16_t)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (int16_t)(vectorY << 1)
#else
#define DECODE_MV(BS, R, S, vectorX, vectorY, task_num)           \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video, task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
                                                        \
  /* Decode y vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R + 1], S + 1, video, task_num);        \
  }                                                     \
  vectorY = video->PMV[R + 1];

#define DECODE_MV_FIELD(BS, R, S, vectorX, vectorY, task_num)     \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&video->PMV[R], S, video,task_num);                \
  }                                                     \
  vectorX = video->PMV[R];                              \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (int16_t)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (int16_t)(vectorY << 1)


#define DECODE_MV_FULLPEL(BS, R, S, vectorX, vectorY, task_num)   \
  /* R = 2*(2*r + s); S = 2*s */                        \
  /* Decode x vector */                                 \
  vectorX = (int16_t)(video->PMV[R] >> 1);               \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorX, S, video, task_num);                      \
  }                                                     \
  video->PMV[R] = (int16_t)(vectorX << 1);               \
                                                        \
  /* Decode y vector */                                 \
  vectorY = (int16_t)(video->PMV[R + 1] >> 1);           \
  if (IS_NEXTBIT1(BS)) {                                \
    SKIP_BITS(BS, 1)                                    \
  } else {                                              \
    update_mv(&vectorY, S + 1, video, task_num);                  \
  }                                                     \
  video->PMV[R + 1] = (int16_t)(vectorY << 1)
#endif

#define DECODE_QUANTIZER_SCALE(BS, Q_SCALE) \
{                                           \
  int32_t _q_scale;                             \
  GET_TO9BITS(video->bs, 5, _q_scale)       \
  if (_q_scale < 1) {                       \
    return UMC_ERR_INVALID_STREAM;                  \
  }                                         \
  Q_SCALE = q_scale[PictureHeader[task_num].q_scale_type][_q_scale]; \
}

#define DECODE_MB_INCREMENT(BS, macroblock_address_increment)         \
{                                                                     \
  int32_t cc;                                                          \
  SHOW_BITS(BS, 11, cc)                                               \
                                                                      \
  if(cc == 0) {                                                       \
    return UMC_OK; /* end of slice or bad code. Anyway, stop slice */ \
  }                                                                   \
                                                                      \
  macroblock_address_increment = 0;                                   \
  while(cc == 8)                                                      \
  {                                                                   \
    macroblock_address_increment += 33; /* macroblock_escape */       \
    GET_BITS(BS, 11, cc);                                             \
    SHOW_BITS(BS, 11, cc)                                             \
  }                                                                   \
  DECODE_VLC(cc, BS, vlcMBAdressing);                                 \
  macroblock_address_increment += cc;                                 \
  macroblock_address_increment--;                                     \
  if (static_cast<unsigned int>(macroblock_address_increment) > static_cast<unsigned int>(sequenceHeader.mb_width[task_num] - video->mb_col)) { \
    macroblock_address_increment = sequenceHeader.mb_width[task_num] - video->mb_col;       \
  }                                                                               \
}

////////////////////////////////////////////////////////////////////

#define APPLY_SIGN(val, sign)  \
  val += sign;                 \
  if (val > 2047) val = 2047; /* with saturation */ \
  val ^= sign

#define SAT(val) \
  if (val > 2047) val = 2047;   \
  if (val < -2048) val = -2048;

#ifdef LOCAL_BUFFERS
#define DEF_BLOCK(NAME) \
  int16_t NAME[64];
#else
#define DEF_BLOCK(NAME) \
  int16_t *NAME = pQuantSpec->NAME;
#endif

#define MP2_FUNC(type, name, arg)  type name arg

#ifdef IPP_DIRECT_CALLS
#ifdef __cplusplus
extern "C" {
#endif
extern void dct_8x8_inv_2x2_16s(int16_t* pSrc, int16_t* pDst);
extern void dct_8x8_inv_4x4_16s(int16_t* pSrc, int16_t* pDst);
extern void dct_8x8_inv_16s(int16_t* pSrc, int16_t* pDst);
extern void dct_8x8_inv_16s8uR(int16_t* pSrc, uint8_t* pDst, int32_t dstStep);
extern void ownvc_Add8x8_16s8u_C1IRS(const int16_t* pSrc, int32_t srcStep, uint8_t* pSrcDst, int32_t srcDstStep);
#ifdef __cplusplus
} /* extern "C" */
#endif
#define FUNC_DCT8x8      dct_8x8_inv_16s
#define FUNC_DCT4x4      dct_8x8_inv_4x4_16s
#define FUNC_DCT2x2      dct_8x8_inv_2x2_16s
#define FUNC_DCT8x8Intra dct_8x8_inv_16s8uR
#define FUNC_ADD8x8      ownvc_Add8x8_16s8u_C1IRS
#else
#define FUNC_DCT8x8      ippiDCT8x8Inv_16s_C1
#define FUNC_DCT4x4      ippiDCT8x8Inv_4x4_16s_C1
#define FUNC_DCT2x2      ippiDCT8x8Inv_2x2_16s_C1
#define FUNC_DCT8x8Intra ippiDCT8x8Inv_16s8u_C1R
#define FUNC_ADD8x8      ippiAdd8x8_16s8u_C1IRS
#endif

void IDCTAdd_1x1to8x8(int32_t val, uint8_t* y, int32_t step);
void IDCTAdd_1x4to8x8(const int16_t* x, uint8_t* y, int32_t step);
void Pack8x8(int16_t* x, uint8_t* y, int32_t step);


#ifdef MPEG2_USE_REF_IDCT

extern "C" {
void Reference_IDCT(int16_t *block, int16_t *out, int32_t step);
//#define Reference_IDCT(in, out, step) ippiDCT8x8Inv_16s_C1R(in, out, 2*(step))
} /* extern "C" */

#define IDCT_INTER(SRC, NUM, BUFF, DST, STEP)        \
  Reference_IDCT(SRC, BUFF, 8);                      \
  FUNC_ADD8x8(BUFF, 16, DST, STEP)

#define IDCT_INTER1(val, idct, pSrcDst, srcDstStep)  \
  SAT(val);                                          \
  for (k = 0; k < 64; k++) pDstBlock[k] = 0;         \
  pDstBlock[0] = val;                                \
  mask = 1 ^ val;                                    \
  pDstBlock[63] ^= mask & 1;                         \
  IDCT_INTER(pDstBlock, 0, idct, pSrcDst, srcDstStep)

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)       \
  int32_t ii, jj;                                    \
  SRC[0] -= 1024;                                   \
  Reference_IDCT(SRC, BUFF, 8);                     \
  for(ii = 0; ii < 8; ii++) {                       \
    for(jj = 0; jj < 8; jj++) {                     \
      BUFF[ii * 8 + jj] += 128;                     \
    }                                               \
  }                                                 \
  Pack8x8(BUFF, DST, STEP)

#define IDCT_INTRA1(val, idct, pSrcDst, srcDstStep)  \
  SAT(val);                                          \
  for (k = 0; k < 64; k++) pDstBlock[k] = 0;         \
  pDstBlock[0] = val;                                \
  mask = 1 ^ val;                                    \
  pDstBlock[63] ^= mask & 1;                         \
  IDCT_INTRA(pDstBlock, 0, idct, pSrcDst, srcDstStep)

#else /* MPEG2_USE_REF_IDCT */

#ifdef USE_INTRINSICS
#define IDCT_INTER_1x4(SRC, NUM, DST, STEP)        \
  if (NUM < 4 && !SRC[1]) {                        \
    IDCTAdd_1x4to8x8(SRC, DST, STEP);              \
  } else
#else
#define IDCT_INTER_1x4(SRC, NUM, DST, STEP)
#endif

#define IDCT_INTER(SRC, NUM, BUFF, DST, STEP)      \
  if (NUM < 10) {                                  \
    if (!NUM) {                                    \
      IDCTAdd_1x1to8x8(SRC[0], DST, STEP);         \
    } else                                         \
    IDCT_INTER_1x4(SRC, NUM, DST, STEP)            \
    /*if (NUM < 2) {                                 \
      FUNC_DCT2x2(SRC, BUFF);                      \
      FUNC_ADD8x8(BUFF, 16, DST, STEP);            \
    } else*/ {                                       \
      FUNC_DCT4x4(SRC, BUFF);                      \
      FUNC_ADD8x8(BUFF, 16, DST, STEP);            \
    }                                              \
  } else {                                         \
    FUNC_DCT8x8(SRC, BUFF);                        \
    FUNC_ADD8x8(BUFF, 16, DST, STEP);              \
  }

#define IDCT_INTER1(val, idct, pSrcDst, srcDstStep) \
  ippiAddC8x8_16s8u_C1IR((int16_t)((val + 4) >> 3), pSrcDst, srcDstStep)

#ifdef USE_INTRINSICS

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)                   \
  if (NUM < 10) {                                               \
    if (!NUM) {                                                 \
      ippiDCT8x8Inv_AANTransposed_16s8u_C1R(SRC, DST, STEP, 0); \
    } else {                                                    \
      FUNC_DCT4x4(SRC, BUFF);                                   \
      Pack8x8(BUFF, DST, STEP);                                 \
    }                                                           \
  } else {                                                      \
    FUNC_DCT8x8Intra(SRC, DST, STEP);                           \
  }

#else

#define IDCT_INTRA(SRC, NUM, BUFF, DST, STEP)                   \
  if (!NUM) {                                                   \
    ippiDCT8x8Inv_AANTransposed_16s8u_C1R(SRC, DST, STEP, 0);   \
  } else {                                                      \
    FUNC_DCT8x8Intra(SRC, DST, STEP);                           \
  }

#endif

#define IDCT_INTRA1(val, idct, pSrcDst, srcDstStep) \
  pDstBlock[0] = (int16_t)val; \
  ippiDCT8x8Inv_AANTransposed_16s8u_C1R(pDstBlock, pSrcDst, srcDstStep, 0)

#endif // MPEG2_USE_REF_IDCT

/***************************************************************/

#ifndef __MPEG2_FUNC__
#define __MPEG2_FUNC__

#include "ippdefs.h"

#define MP2_API( type,name,arg )        extern type name arg;

#define IPPVC_ZIGZAG_SCAN           0
#define IPPVC_ALT_SCAN              1
#define IPPVC_MPEG1_STREAM          2
#define IPPVC_LEAVE_QUANT_UNCHANGED 4
#define IPPVC_LEAVE_SCAN_UNCHANGED  8

typedef DecodeIntraSpec_MPEG2 IppiDecodeIntraSpec_MPEG2;
typedef DecodeInterSpec_MPEG2 IppiDecodeInterSpec_MPEG2;

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDecodeInter8x8IDCTAdd_MPEG2_1u8u
//
//  Purpose:
//    Performs VLC decoding of DCT coefficients for one inter 8x8 block,
//    dequantization of coefficients, inverse DCT and addition of resulted
//    8x8 block to destination.
//
//  Parameters:
//    ppBitStream      Pointer to the pointer to the current byte in
//                     the bitstream, it is updated after block decoding.
//    pBitOffset       Pointer to the bit position in the byte pointed by
//                     *ppBitStream, it is updated after block decoding.
//                     Must be in the range [0, 7].
//    pQuantSpec       Pointer to the structure IppiDecodeInterSpec_MPEG2
//    QP               Quantization parameter.
//    pSrcDst          Pointer to the 8x8 block in the destination image
//    srcDstStep       Step through the destination image
//
//  Returns:
//    ippStsNoErr        No error.
//    ippStsNullPtrErr   One of the specified pointers is NULL.
//    ippStsVLCErr       An illegal code is detected through the
//                       stream processing.
*/

MP2_API(IppStatus, ippiDecodeInter8x8IDCTAdd_MPEG1_1u8u, (
    uint8_t**                            ppBitStream,
    int32_t*                            pBitOffset,
    IppiDecodeInterSpec_MPEG2*         pQuantSpec,
    int32_t                             QP,
    uint8_t*                             pSrcDst,
    int32_t                             srcDstStep))

MP2_API(IppStatus, ippiDecodeInter8x8IDCTAdd_MPEG2_1u8u, (
    uint8_t**                            ppBitStream,
    int32_t*                            pBitOffset,
    IppiDecodeInterSpec_MPEG2*         pQuantSpec,
    int32_t                             QP,
    uint8_t*                             pSrcDst,
    int32_t                             srcDstStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDecodeIntra8x8IDCT_MPEG2_1u8u
//
//  Purpose:
//    Performs VLC decoding of DCT coefficients for one intra 8x8 block,
//    dequantization of coefficients, inverse DCT and storing of resulted
//    8x8 block to destination.
//
//  Parameters:
//    ppBitStream      Pointer to the pointer to the current byte in
//                     the bitstream, it is updated after block decoding.
//    pBitOffset       Pointer to the bit position in the byte pointed by
//                     *ppBitStream, it is updated after block decoding.
//                     Must be in the range [0, 7].
//    pQuantSpec       Pointer to the structure IppiDecodeIntraSpec_MPEG2
//    QP               Quantization parameter.
//    blockType        Indicates the type of block, takes one of the following
//                     values:
//                         IPPVC_BLOCK_LUMA - for luma blocks,
//                         IPPVC_BLOCK_CHROMA - for chroma blocks
//                     And in case of MPEG1 D-type block, IPPVC_BLOCK_MPEG1_DTYPE
//                     have to be added to IPPVC_BLOCK_LUMA or IPPVC_BLOCK_CHROMA.
//    pDCPred          Pointer to the value to be added to the DC coefficient
//    pDst             Pointer to the 8x8 block in the destination image
//    dstStep          Step through the destination image
//
//  Returns:
//    ippStsNoErr        No error.
//    ippStsNullPtrErr   One of the specified pointers is NULL.
//    ippStsVLCErr       An illegal code is detected through the
//                       stream processing.
*/

MP2_API(IppStatus, ippiDecodeIntra8x8IDCT_MPEG1_1u8u, (
    uint8_t**                            ppBitStream,
    int32_t*                            pBitOffset,
    IppiDecodeIntraSpec_MPEG2*         pQuantSpec,
    int32_t                             QP,
    int32_t                             blockType,
    int16_t*                            pDCPred,
    uint8_t*                             pDst,
    int32_t                             dstStep))

MP2_API(IppStatus, ippiDecodeIntra8x8IDCT_MPEG2_1u8u, (
    uint8_t**                            ppBitStream,
    int32_t*                            pBitOffset,
    IppiDecodeIntraSpec_MPEG2*         pQuantSpec,
    int32_t                             QP,
    int32_t                             blockType,
    int16_t*                            pDCPred,
    uint8_t*                             pDst,
    int32_t                             dstStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDecodeIntraInit_MPEG2
//    ippiDecodeInterInit_MPEG2
//
//  Purpose:
//    Initialized IppiDecodeIntraSpec_16s(IppiDecodeInterSpec_MPEG2) structure.
//    If pQuantMatrix is NULL this means default quantization matrix.
//
//  Parameters:
//    pQuantMatrix   Pointer to the quantization matrix size of 64.
//    scan           Type of the scan, takes one of the following values:
//                       IPPVC_SCAN_ZIGZAG, indicating the classical zigzag scan,
//                       IPPVC_SCAN_VERTICAL - alternate-vertical scan
//    intraVLCFormat 0 or 1, defines one of two VLC tables for decoding intra blocks
//    intraShiftDC   Integer value for shifting intra DC coefficient after VLC decoding
//    pSpec          Pointer to the structure IppiDecodeIntraSpec_16s or
//                   IppiDecodeInterSpec_MPEG2 which will initialized.
//
//  Returns:
//    ippStsNoErr        No error.
//    ippStsNullPtrErr   Indicates an error when pointer pSpec is NULL.
*/

MP2_API(IppStatus, ippiDecodeIntraInit_MPEG2, (
    const uint8_t*                 pQuantMatrix,
    int32_t                       scan,
    int32_t                       intraVLCFormat,
    int32_t                       intraShiftDC,
    IppiDecodeIntraSpec_MPEG2*   pSpec))

MP2_API(IppStatus, ippiDecodeInterInit_MPEG2, (
    const uint8_t*                 pQuantMatrix,
    int32_t                       flag,
    IppiDecodeInterSpec_MPEG2*   pSpec))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDecodeIntraGetSize_MPEG2
//    ippiDecodeInterGetSize_MPEG2
//
//  Purpose:
//    Return size of IppiDecodeIntraSpec_MPEG2 or IppiDecodeInterSpec_MPEG2.
//
//  Parameters:
//    pSpecSize Pointer to the resulting size of the structure
//    IppiDecodeIntraSpec_MPEG2 or IppiDecodeInterSpec_MPEG2.
//
//  Returns:
//    ippStsNoErr        No error.
//    ippStsNullPtrErr   Indicates an error when pointer pSpecSize is NULL.
*/

MP2_API(IppStatus, ippiDecodeIntraGetSize_MPEG2, (
    int32_t* pSpecSize))

MP2_API(IppStatus, ippiDecodeInterGetSize_MPEG2, (
    int32_t* pSpecSize))

#endif /* __MPEG2_FUNC__ */

/***************************************************************/

extern const uint16_t MPEG2_VLC_TAB1[];
extern const uint8_t  MPEG2_DCSIZE_TAB[];

#define UHBITS(code, nbits) (((uint32_t)(code)) >> (32 - (nbits)))
#define SHBITS(code, nbits) (((int32_t)(code)) >> (32 - (nbits)))

#define TAB1_OFFSET_10BIT 248
#define TAB1_OFFSET_15BIT 360
#define TAB1_OFFSET_16BIT 408
#define TAB1_OFFSET_8BIT_INTRA 432
#define TAB1_OFFSET_10BIT_INTRA 680

#define UNPACK_VLC1(tab_val, run, val, len) \
{ \
  uint32_t _tab_val = tab_val; \
  run = _tab_val & 0x1f; \
  len = (_tab_val >> 5) & 0xf; \
  val = (_tab_val >> 9); \
}

#define UNPACK_VLC2(tab_val, run, val, len) \
{ \
  uint32_t _tab_val = tab_val; \
  run = _tab_val & 0x1f; \
  len = (_tab_val >> 5) & 0xf; \
  val = (_tab_val >> 10); \
}

#define DECODE_DC(val)                                    \
{                                                         \
  const uint8_t *pTab;                                      \
  int32_t dct_dc_size;                                     \
  SHOW_HI17BITS(BS, code);                                \
  pTab = MPEG2_DCSIZE_TAB + (chromaFlag << 6);            \
  if (code < 0xf8000000) {                                \
    tbl = pTab[UHBITS(code, 5)];                          \
    dct_dc_size = tbl & 0xF;                              \
    len = tbl >> 4;                                       \
    if (dct_dc_size) {                                    \
      EXPAND_17BITS(BS, code);                            \
      code <<= len;                                       \
      val = UHBITS(code, dct_dc_size) - UHBITS(SHBITS(~code, 1), dct_dc_size); \
      SKIP_BITS(BS, len + dct_dc_size);                   \
    } else {                                              \
      val = 0;                                            \
      SKIP_BITS(BS, len);                                 \
    }                                                     \
  } else {                                                \
    tbl = pTab[32 + (UHBITS(code, 10) & 0x1f)];           \
    dct_dc_size = tbl & 0xF;                              \
    len = tbl >> 4;                                       \
    EXPAND_17BITS(BS, code);                              \
    EXPAND_25BITS(BS, code);                              \
    code <<= len;                                         \
    val = UHBITS(code, dct_dc_size) - UHBITS(SHBITS(~code, 1), dct_dc_size); \
    SKIP_BITS(BS, len + dct_dc_size);                     \
  }                                                       \
}

/***************************************************************/

#define COPY_BITSTREAM(DST_BS, SRC_BS) \
  DST_BS##_curr_ptr = SRC_BS##_curr_ptr; \
  DST_BS##_bit_offset = SRC_BS##_bit_offset;

#endif // MFX_ENABLE_MPEG2_VIDEO_DECODE
