// Copyright (c) 2003-2018 Intel Corporation
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
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_defs_sw.h"
#include "umc_mpeg2_dec_sw.h"

// turn off the "conversion from 'type1' to 'type2', possible loss of data" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4244)
#endif

using namespace UMC;

#define BlockIntra_mpeg1(dc_past, pDst, Pitch)                    \
  if (ippiDecodeIntra8x8IDCT_MPEG1_1u8u(&video->bs_curr_ptr,      \
                                        &video->bs_bit_offset,    \
                                        &video->decodeIntraSpec,  \
                                        video->cur_q_scale,       \
                                        flag,                     \
                                        dc_past,                  \
                                        pDst,                     \
                                        Pitch))                   \
  {                                                               \
    return UMC_ERR_INVALID_STREAM;                                        \
  }

Status MPEG2VideoDecoderSW::DecodeSlice_MPEG1(VideoContext *video, int task_num)
{
  int32_t pitch_l = video->Y_comp_pitch;
  int32_t pitch_c = video->U_comp_pitch;
  int32_t coded_block_pattern = 0;
  int32_t end_of_macroblock;
  int32_t macroblock_type;
  int32_t macroblock_quant;
  int32_t macroblock_pattern;
  int32_t macroblock_intra;
  int32_t macroblock_address_increment = 0;
  uint32_t code;
  int32_t mb_row_prev = 0, mb_col_prev = 0;

  video->prediction_type = IPPVC_MC_FRAME;

  video->dct_dc_past[0] = 128;
  video->dct_dc_past[1] = 128;
  video->dct_dc_past[2] = 128;

  for (;;) {
    SHOW_BITS(video->bs, 11, code);
    while (code == 0x00f) {
      SKIP_BITS(video->bs, 11); //macroblock_stuffing
      SHOW_BITS(video->bs, 11, code)
    }

    macroblock_address_increment = 0;

    if(code == 0)
      return UMC_OK; // end of slice or bad code. Anyway, stop slice

    while(code == 0x008)
    {
      macroblock_address_increment += 33;//macroblock_escape
      GET_BITS(video->bs, 11, code);
      SHOW_BITS(video->bs,11,code)
    }

    DECODE_VLC(code, video->bs, vlcMBAdressing);
    if (GET_REMAINED_BYTES(video->bs) <= 0)
      return UMC_OK;

    macroblock_address_increment += code;

    if(macroblock_address_increment > 1 || video->m_bNewSlice)
      video->dct_dc_past[0] =
      video->dct_dc_past[1] =
      video->dct_dc_past[2] = 128;

    if(video->m_bNewSlice) {
      video->mb_row = video->slice_vertical_position - 1;
      video->mb_col = macroblock_address_increment - 1;
      video->mb_address_increment = 1;
      mb_row_prev = video->mb_row;
      mb_col_prev = video->mb_col;
      video->m_bNewSlice = false;
    } else {
      video->mb_address_increment = macroblock_address_increment;
      mb_row_prev = video->mb_row;
      mb_col_prev = video->mb_col+1;
      video->mb_col += macroblock_address_increment;

      if(video->mb_col >= sequenceHeader.mb_width[task_num]) {
        video->mb_row += video->mb_col / sequenceHeader.mb_width[task_num];
        if(video->mb_row >= sequenceHeader.mb_height[task_num])
        {
            return UMC_OK;
        }
        video->mb_col %= sequenceHeader.mb_width[task_num];

        // prev points to this or first skipped, could change row in mpeg1
        mb_row_prev += mb_col_prev / sequenceHeader.mb_width[task_num];
        mb_col_prev %= sequenceHeader.mb_width[task_num];
      }

    }

    if(video->mb_address_increment > 1)
    {
      int32_t toskip = video->mb_address_increment-1;
      int32_t row_l = mb_row_prev << 4;
      int32_t col_l = mb_col_prev << 4;
      int32_t nmb;

      if (PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE) {
        video->PMV[0] = 0;
        video->PMV[1] = 0;
      }
      video->vector[0] = video->PMV[0];
      video->vector[1] = video->PMV[1];
      video->vector[2] = video->PMV[2];
      video->vector[3] = video->PMV[3];

      while(toskip>0){
        nmb = sequenceHeader.mb_width[task_num] - (col_l >> 4);
        if (nmb > toskip) nmb = toskip;
        video->offset_l = row_l * pitch_l + col_l;
        video->offset_c = (row_l >> 1) * pitch_c + (col_l >> 1);

        video->mb_address_increment = nmb + 1;

        if (video->macroblock_motion_forward && video->macroblock_motion_backward) {
          mc_mp2_420b_skip(video, task_num);
        } else {
          mc_mp2_420_skip(video, task_num);
        }

        toskip -= nmb;
        if (toskip > 0) {
          col_l = 0;
          row_l += 16;
          if(row_l >= sequenceHeader.mb_height[task_num]*16) {
            return UMC_ERR_INVALID_STREAM;
          }
        }
      }
    }// skipped macroblocks

    if(!PictureHeader[task_num].d_picture)
    {
      DECODE_VLC(macroblock_type, video->bs, vlcMBType[PictureHeader[task_num].picture_coding_type - 1]);
      macroblock_quant          = macroblock_type & 0x10;
      video->macroblock_motion_forward = macroblock_type & 0x08;
      video->macroblock_motion_backward= macroblock_type & 0x04;
      macroblock_pattern        = macroblock_type & 0x02;
      macroblock_intra          = macroblock_type & 0x01;
    }
    else//D picture
    {
      GET_1BIT(video->bs,macroblock_type);
      VM_ASSERT(macroblock_type == 1);
      macroblock_intra           = 1;
      macroblock_quant           = 0;
      video->macroblock_motion_forward = 0;
      video->macroblock_motion_backward= 0;
      macroblock_pattern        = 0;
    }//switch(PictureHeader[task_num].picture_coding_type)

    if(macroblock_quant)
    {
      GET_TO9BITS(video->bs, 5, video->cur_q_scale);
      VM_ASSERT(video->cur_q_scale >= 1);
      if (video->cur_q_scale < 1)
          return UMC_ERR_INVALID_STREAM;
    }

    video->offset_l = (video->mb_row*pitch_l + video->mb_col) << 4;
    video->offset_c = (video->mb_row*pitch_c + video->mb_col) << 3;
    int32_t curr_index = frame_buffer.curr_index[task_num];
    uint8_t *cur_Y_data = frame_buffer.frame_p_c_n[curr_index].Y_comp_data;
    uint8_t *cur_U_data = frame_buffer.frame_p_c_n[curr_index].U_comp_data;
    uint8_t *cur_V_data = frame_buffer.frame_p_c_n[curr_index].V_comp_data;
    cur_Y_data += video->offset_l;
    cur_U_data += video->offset_c;
    cur_V_data += video->offset_c;
    video->blkCurrYUV[0] = cur_Y_data;
    video->blkCurrYUV[1] = cur_U_data;
    video->blkCurrYUV[2] = cur_V_data;

    if(macroblock_intra)
    {
      int32_t flag = (PictureHeader[task_num].d_picture ? 4 : 0);

      video->PMV[0] = 0;
      video->PMV[1] = 0;
      video->PMV[2] = 0;
      video->PMV[3] = 0;

      BlockIntra_mpeg1(&video->dct_dc_past[0], cur_Y_data, pitch_l);
      BlockIntra_mpeg1(&video->dct_dc_past[0], cur_Y_data + 8, pitch_l);
      BlockIntra_mpeg1(&video->dct_dc_past[0], cur_Y_data + 8*pitch_l, pitch_l);
      BlockIntra_mpeg1(&video->dct_dc_past[0], cur_Y_data + 8*pitch_l + 8, pitch_l);
      flag |= 1; // 1=ChromaFlag
      BlockIntra_mpeg1(&video->dct_dc_past[1], cur_U_data, pitch_c);
      BlockIntra_mpeg1(&video->dct_dc_past[2], cur_V_data, pitch_c);

      if (PictureHeader[task_num].d_picture)
      {
        GET_1BIT(video->bs,end_of_macroblock);
        VM_ASSERT(end_of_macroblock);
        if(!end_of_macroblock)
          return UMC_ERR_INVALID_STREAM;
      }
    }
    else
    {
      video->dct_dc_past[0] = 128;
      video->dct_dc_past[1] = 128;
      video->dct_dc_past[2] = 128;

      if (video->macroblock_motion_forward) {
        if (PictureHeader[task_num].full_pel_forward_vector) {
          mc_fullpel_forward(video, task_num);
        } else {
          mc_frame_forward_420(video, task_num);
        }
        if (video->macroblock_motion_backward) {
          if (PictureHeader[task_num].full_pel_backward_vector) {
            mc_fullpel_backward_add(video, task_num);
          } else {
            mc_frame_backward_add_420(video, task_num);
          }
        }
      } else {
        if (video->macroblock_motion_backward) {
          if (PictureHeader[task_num].full_pel_backward_vector) {
            mc_fullpel_backward(video, task_num);
          } else {
            mc_frame_backward_420(video, task_num);
          }
        } else /*if (PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE)*/ {
          RESET_PMV(video->PMV)
            mc_frame_forward0_420(video, task_num);
        }
      }

      if(macroblock_pattern)
      {
        DECODE_VLC(coded_block_pattern, video->bs, vlcMBPattern);
      }

      int32_t blk;
      for (blk = 0; blk < 6; blk++) {
        if (coded_block_pattern & 32) {
          int32_t chromaFlag = blk >> 2;
          int32_t cc = chromaFlag + (blk & chromaFlag);
          IppiDecodeInterSpec_MPEG2 *interSpec;
          interSpec = chromaFlag ? &video->decodeInterSpecChroma : &video->decodeInterSpec;

          ippiDecodeInter8x8IDCTAdd_MPEG1_1u8u(
            &video->bs_curr_ptr,
            &video->bs_bit_offset,
            interSpec,
            video->cur_q_scale,
            video->blkCurrYUV[cc] + video->blkOffsets[0][blk],
            video->blkPitches[0][chromaFlag]);
        }
        coded_block_pattern += coded_block_pattern;
      }
    }
  }

  //return UMC_OK;
}

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER