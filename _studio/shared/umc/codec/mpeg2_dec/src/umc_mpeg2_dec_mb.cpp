/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2003-2013 Intel Corporation. All Rights Reserved.
//
*/

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_DECODER)

#include "umc_mpeg2_dec_base.h"

#pragma warning(disable: 4244)

using namespace UMC;

Status MPEG2VideoDecoderBase::DecodeSlice_FrameI_420(IppVideoContext *video, int task_num)
{
    Ipp32s dct_type = 0;

    Ipp32s pitch_l = video->Y_comp_pitch;
    Ipp32s pitch_c = video->U_comp_pitch;

    Ipp32s macroblock_type = 0, macroblock_address_increment = 0;
    Ipp32s load_dct_type;

    video->dct_dc_past[0] =
    video->dct_dc_past[1] =
    video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

    video->mb_row = video->slice_vertical_position - 1;
    video->mb_col = -1;

    if (PictureHeader[task_num].picture_structure == FRAME_PICTURE) 
    {
        dct_type = 0;
        load_dct_type = !PictureHeader[task_num].frame_pred_frame_dct;
        video->offset_l = (video->mb_col << 4) + pitch_l * (video->mb_row << 4);
        video->offset_c = (video->mb_col << 3) + pitch_c * (video->mb_row << ROW_CHROMA_SHIFT_420);

#ifndef UMC_VA_DXVA
        if (video->offset_l > (Ipp32s)(pitch_l * (video->Y_comp_height - 16)))
        {
            return UMC_ERR_INVALID_STREAM;
        }

        if (video->offset_c > (Ipp32s)(pitch_c * ((video->Y_comp_height >> 1)-8)))
        {
            return UMC_ERR_INVALID_STREAM;
        }
#endif
    } 
    else 
    {
        dct_type = 2;
        load_dct_type = 0;
        video->offset_l = (video->mb_col << 4) + 2 * pitch_l * (video->mb_row << 4);
        video->offset_c = (video->mb_col << 3) + 2 * pitch_c * (video->mb_row << ROW_CHROMA_SHIFT_420);
        
        if (PictureHeader[task_num].picture_structure == BOTTOM_FIELD) 
        {
            video->offset_l += pitch_l;
            video->offset_c += pitch_c;
        }

        #ifndef UMC_VA_DXVA 
            if (PictureHeader[task_num].picture_structure == TOP_FIELD)
            {
                if (video->offset_l > (Ipp32s)(pitch_l * (video->Y_comp_height - 32)))
                {
                    return UMC_ERR_INVALID_STREAM;
                }

                if (video->offset_c > (Ipp32s)(pitch_c * ((video->Y_comp_height >> 1) - 16)))
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }
            else
            {
                if (video->offset_l > (Ipp32s)(pitch_l * (video->Y_comp_height - 32)+pitch_l))
                {
                    return UMC_ERR_INVALID_STREAM;
                }

                if (video->offset_c > (Ipp32s)(pitch_c * ((video->Y_comp_height >> 1) - 16)+pitch_c))
                {
                    return UMC_ERR_INVALID_STREAM;
                }
            }
        #endif
    }

    for ( ; ; ) 
    {
        Ipp32s remained = GET_REMAINED_BYTES(video->bs);
        
        if (remained == 0) 
        {
            return UMC_OK;
        } 
        else if (remained < 0) 
        {
            return UMC_ERR_INVALID_STREAM;
        }
    
        video->mb_col++;
        video->offset_l += 16;
        video->offset_c += 8;

        if (IS_NEXTBIT1(video->bs)) 
        {
            SKIP_BITS(video->bs, 1)
        } 
        else 
        {
            DECODE_MB_INCREMENT(video->bs, macroblock_address_increment);
            video->mb_col += macroblock_address_increment;
            video->offset_l += macroblock_address_increment << 4;
            video->offset_c += macroblock_address_increment << 3;
        }

        #if defined UMC_VA_DXVA && defined ELK
            if (pack_w. m_va)
            {
                break;
            }
        #endif

        if (video->mb_col >= sequenceHeader.mb_width[task_num]) 
        {
            return UMC_ERR_INVALID_STREAM;
        }

        DECODE_VLC(macroblock_type, video->bs, vlcMBType[0]);

        if (load_dct_type) 
        {
            GET_1BIT(video->bs, dct_type);
        }

        if (macroblock_type & IPPVC_MB_QUANT)
        {
            DECODE_QUANTIZER_SCALE(video->bs, video->cur_q_scale);
        }

        if (PictureHeader[task_num].concealment_motion_vectors)
        {
            if (PictureHeader[task_num].picture_structure != FRAME_PICTURE) 
            {
                SKIP_BITS(video->bs, 1);
            }
            
            mv_decode(0, 0, video, task_num);
            SKIP_BITS(video->bs, 1);
        }

        RECONSTRUCT_INTRA_MB_420(video->bs, dct_type);
    }

#if defined UMC_VA_DXVA && defined ELK
    if (pack_w. m_va)
    {
        pack_w.pSliceInfo->wHorizontalPosition   = (WORD)video->mb_col;
        pack_w.pSliceInfo->wNumberMBsInSlice     = (WORD)(sequenceHeader.mb_width[task_num] - pack_w.pSliceInfo->wHorizontalPosition);

        if (video->mb_col > 0 && pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
            pack_w.pSliceInfo[-1].wNumberMBsInSlice  = (WORD)(video->mb_col - pack_w.pSliceInfo[-1].wHorizontalPosition);
    }
    
    return UMC_OK;
#endif

} // Status MPEG2VideoDecoderBase::DecodeSlice_FrameI_420(IppVideoContext *video, int task_num)

Status MPEG2VideoDecoderBase::DecodeSlice_FramePB_420(IppVideoContext *video, int task_num)
{
    Ipp32s dct_type = 0;
    Ipp32s macroblock_type;
    Ipp32s macroblock_address_increment = 0;

    video->prediction_type = IPPVC_MC_FRAME;

    video->dct_dc_past[0] =
    video->dct_dc_past[1] =
    video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

    video->mb_row = video->slice_vertical_position - 1;
    video->mb_col = -1;

    video->offset_l = (video->mb_col << 4) + video->Y_comp_pitch * (video->mb_row << 4);
    video->offset_c = (video->mb_col << 3) + video->U_comp_pitch * (video->mb_row << ROW_CHROMA_SHIFT_420);

#ifndef UMC_VA_DXVA 
    if (video->offset_l > (Ipp32s)(video->Y_comp_pitch * (video->Y_comp_height - 16)))
    {
        return UMC_ERR_INVALID_STREAM;
    }

    if (video->offset_c > (Ipp32s)(video->U_comp_pitch * ((video->Y_comp_height >> 1)-8)))
    {
        return UMC_ERR_INVALID_STREAM;
    }
#endif

    for (;;) 
    {
        Ipp32s remained = GET_REMAINED_BYTES(video->bs);

        if (remained == 0) 
        {
            return UMC_OK;
        }
        else if (remained < 0) 
        {
            return UMC_ERR_INVALID_STREAM;
        }

        video->mb_col++;
        video->offset_l += 16;
        video->offset_c += 8;

        if (IS_NEXTBIT1(video->bs))
        { // increment=1

        #if defined UMC_VA_DXVA && defined ELK
            if (pack_w.m_va)
            {
                break;
            }
        #endif

            SKIP_BITS(video->bs, 1)
        }
        else
        {
            DECODE_MB_INCREMENT(video->bs, macroblock_address_increment);

            #if defined UMC_VA_DXVA && defined ELK
                if (pack_w.m_va)
                    break;
            #endif
          
            video->dct_dc_past[0] =
            video->dct_dc_past[1] =
            video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

            // skipped macroblocks
            if (video->mb_col > 0)
            {
                Ipp32s pitch_l = video->Y_comp_pitch;
                Ipp32s pitch_c = video->U_comp_pitch;
                Ipp32s offset_l = video->offset_l;
                Ipp32s offset_c = video->offset_c;                  
                Ipp32s id_his_new;
                Ipp32s prev_index = frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index;

                if (PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE) 
                {
                    RESET_PMV(video->PMV)
                    id_his_new = 1;
                } 
                else 
                {
                    id_his_new = 0;
                    video->prediction_type = IPPVC_MC_FRAME;
                    COPY_PMV(video->vector, video->PMV);
                    
                    if (!video->macroblock_motion_backward) 
                    {
                        if (!video->PMV[0] && !video->PMV[1]) 
                        {
                            id_his_new = 2;
                        }
                    } 
                    else if (!video->macroblock_motion_forward) 
                    {
                        if (!video->PMV[2] && !video->PMV[3]) 
                        {
                            id_his_new = 3;
                            prev_index = frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index;
                        }
                    }
                }

                if (id_his_new) 
                {
                    Ipp32s curr_index = frame_buffer.curr_index[task_num];
                    Ipp8u *ref_Y_data = frame_buffer.frame_p_c_n[prev_index].Y_comp_data;
                    Ipp8u *ref_U_data = frame_buffer.frame_p_c_n[prev_index].U_comp_data;
                    Ipp8u *ref_V_data = frame_buffer.frame_p_c_n[prev_index].V_comp_data;
                    Ipp8u *cur_Y_data = frame_buffer.frame_p_c_n[curr_index].Y_comp_data;
                    Ipp8u *cur_U_data = frame_buffer.frame_p_c_n[curr_index].U_comp_data;
                    Ipp8u *cur_V_data = frame_buffer.frame_p_c_n[curr_index].V_comp_data;

                    IppiSize roi = {16*macroblock_address_increment, 16};
                    ippiCopy_8u_C1R(ref_Y_data + offset_l, pitch_l, cur_Y_data + offset_l, pitch_l, roi);
                    roi.height = 1 << ROW_CHROMA_SHIFT_420;
                    roi.width >>= 1;
                    ippiCopy_8u_C1R(ref_U_data + offset_c, pitch_c, cur_U_data + offset_c, pitch_c, roi);
                    ippiCopy_8u_C1R(ref_V_data + offset_c, pitch_c, cur_V_data + offset_c, pitch_c, roi);
                } 
                else 
                {
                    video->mb_address_increment = macroblock_address_increment + 1;
                    if (video->macroblock_motion_forward && video->macroblock_motion_backward) 
                    {
                        mc_mp2_420b_skip(video, task_num);
                    } 
                    else 
                    {
                        mc_mp2_420_skip(video, task_num);
                    }
                }
            } // skipped macroblocks

            video->mb_col += macroblock_address_increment;
            video->offset_l += macroblock_address_increment << 4;
            video->offset_c += macroblock_address_increment << 3;
        }
    
        if (video->mb_col >= sequenceHeader.mb_width[task_num]) 
        {
            return UMC_ERR_INVALID_STREAM;
        }

        DECODE_VLC(macroblock_type, video->bs, vlcMBType[PictureHeader[task_num].picture_coding_type - 1]);

        video->macroblock_motion_forward = macroblock_type & IPPVC_MB_FORWARD;
        video->macroblock_motion_backward= macroblock_type & IPPVC_MB_BACKWARD;

        if (macroblock_type & IPPVC_MB_INTRA)
        {
            if (!PictureHeader[task_num].frame_pred_frame_dct) 
            {
                GET_1BIT(video->bs, dct_type);
            }

            if (macroblock_type & IPPVC_MB_QUANT)
            {
                DECODE_QUANTIZER_SCALE(video->bs, video->cur_q_scale);
            }

            if (!PictureHeader[task_num].concealment_motion_vectors)
            {
                RESET_PMV(video->PMV)
            }
            else
            {
                video->prediction_type = IPPVC_MC_FRAME;

                mv_decode(0, 0, video, task_num);
                video->PMV[4] = video->PMV[0];
                video->PMV[5] = video->PMV[1];

                SKIP_BITS(video->bs,1);
            }

            RECONSTRUCT_INTRA_MB_420(video->bs, dct_type);

            continue;
        }

        video->dct_dc_past[0] =
        video->dct_dc_past[1] =
        video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

        if (!PictureHeader[task_num].frame_pred_frame_dct) 
        {
            if (video->macroblock_motion_forward || video->macroblock_motion_backward) 
            {
                GET_TO9BITS(video->bs, 2, video->prediction_type);
            }
            if (macroblock_type & IPPVC_MB_PATTERN) 
            {
                GET_1BIT(video->bs, dct_type);
            }
        }

        if (macroblock_type & IPPVC_MB_QUANT) 
        {
            DECODE_QUANTIZER_SCALE(video->bs, video->cur_q_scale);
        }

        Ipp32s curr_index = frame_buffer.curr_index[task_num];
        Ipp8u *cur_Y_data = frame_buffer.frame_p_c_n[curr_index].Y_comp_data;
        Ipp8u *cur_U_data = frame_buffer.frame_p_c_n[curr_index].U_comp_data;
        Ipp8u *cur_V_data = frame_buffer.frame_p_c_n[curr_index].V_comp_data;
        video->blkCurrYUV[0] = cur_Y_data + video->offset_l;
        video->blkCurrYUV[1] = cur_U_data + video->offset_c;
        video->blkCurrYUV[2] = cur_V_data + video->offset_c;

        if (video->macroblock_motion_forward) 
        {
            if (video->prediction_type == IPPVC_MC_DP) 
            {
                mc_dualprime_frame_420(video, task_num);
            } 
            else 
            {
                mc_frame_forward_420(video, task_num);

                if (video->macroblock_motion_backward) 
                {
                    mc_frame_backward_add_420(video, task_num);
                }
            }
        }
        else 
        {
            if (video->macroblock_motion_backward) 
            {
                mc_frame_backward_420(video, task_num);
            } 
            else 
            {
                RESET_PMV(video->PMV)
                mc_frame_forward0_420(video, task_num);
            }
        }

        if (macroblock_type & IPPVC_MB_PATTERN) 
        {
            RECONSTRUCT_INTER_MB_420(video->bs, dct_type);
        }
    }
#if (defined UMC_VA_DXVA && defined ELK) || defined(UMC_VA_LINUX)
  if(pack_w. m_va)
  {
#if defined UMC_VA_DXVA
    pack_w.pSliceInfo->wHorizontalPosition   = (WORD)macroblock_address_increment;
    pack_w.pSliceInfo->wNumberMBsInSlice     = (WORD)(sequenceHeader.mb_width[task_num] - pack_w.pSliceInfo->wHorizontalPosition);

        if (pack_w.pSliceInfo->wHorizontalPosition  > 0 && pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
        pack_w.pSliceInfo[-1].wNumberMBsInSlice  = (WORD)(macroblock_address_increment - pack_w.pSliceInfo[-1].wHorizontalPosition);
#elif defined UMC_VA_LINUX
    // need to add logic?
#endif
  }
  return UMC_OK;
#endif

}//DecodeSlice_FramePB_420

Status MPEG2VideoDecoderBase::DecodeSlice_FieldPB_420(IppVideoContext *video, int task_num)
{
  Ipp32s macroblock_type;
  Ipp32s macroblock_address_increment = 0;
  Ipp32s pitch_l = video->Y_comp_pitch;
  Ipp32s pitch_c = video->U_comp_pitch;

  video->prediction_type = IPPVC_MC_FIELD;

  video->dct_dc_past[0] =
  video->dct_dc_past[1] =
  video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

  video->mb_row = video->slice_vertical_position - 1;
  video->mb_col = -1;

  video->row_l  = video->mb_row << 4;
  video->col_l  = video->mb_col << 4;

  video->row_c  = video->mb_row << ROW_CHROMA_SHIFT_420;
  video->col_c  = video->mb_col << 3;

  video->offset_l = 2 * video->row_l * pitch_l + video->col_l;
  video->offset_c = 2 * video->row_c * pitch_c + video->col_c;

#ifndef UMC_VA_DXVA
  if (PictureHeader[task_num].picture_structure == TOP_FIELD)
  {
      if(video->offset_l > (Ipp32s)(pitch_l * (video->Y_comp_height - 32)))
        return UMC_ERR_INVALID_STREAM;
      if(video->offset_c > (Ipp32s)(pitch_c * ((video->Y_comp_height >> 1) - 16)))
        return UMC_ERR_INVALID_STREAM;
  }
  else
  {
      if(video->offset_l > (Ipp32s)(pitch_l * (video->Y_comp_height - 32)+pitch_l))
        return UMC_ERR_INVALID_STREAM;
      if(video->offset_c > (Ipp32s)(pitch_c * ((video->Y_comp_height >> 1) - 16)+pitch_c))
        return UMC_ERR_INVALID_STREAM;
  }
#endif

  //odd_pitch = pitch for bottom field (odd strings)
  if(PictureHeader[task_num].picture_structure == BOTTOM_FIELD)
  {
    video->offset_l += pitch_l;
    video->offset_c += pitch_c;
  }

  for (;;) {
    //if (GET_REMAINED_BYTES(video->bs) <= 0) {
    //  return UMC_ERR_INVALID_STREAM;
    //}
    int remained = GET_REMAINED_BYTES(video->bs);
    if (remained == 0) {
      return UMC_OK;
    } else if (remained < 0) {
      return UMC_ERR_INVALID_STREAM;
    }

    video->mb_col++;
    video->offset_l += 16;
    video->offset_c += 8;
    video->col_l += 16;
    video->col_c += 8;

    if (IS_NEXTBIT1(video->bs)) { // increment=1
#if (defined UMC_VA_DXVA && defined ELK) || defined(UMC_VA_LINUX)
    if(pack_w. m_va)
        break;
#endif
        SKIP_BITS(video->bs, 1)
    } else {
      DECODE_MB_INCREMENT(video->bs, macroblock_address_increment);
#if (defined UMC_VA_DXVA && defined ELK) || defined(UMC_VA_LINUX)
    if(pack_w. m_va)
        break;
#endif
      video->dct_dc_past[0] =
      video->dct_dc_past[1] =
      video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

      // skipped macroblocks
      if (video->mb_col > 0)
      {
        Ipp32s pitch_l1 = video->Y_comp_pitch * 2;
        Ipp32s pitch_c1 = video->U_comp_pitch * 2;
        Ipp32s offset_l = video->offset_l;
        Ipp32s offset_c = video->offset_c;
        Ipp32s id_his_new;
        Ipp32s prev_index = frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].prev_index;

        if (PictureHeader[task_num].picture_coding_type == MPEG2_P_PICTURE) {
          RESET_PMV(video->PMV)
          id_his_new = 1;
        } else {
          id_his_new = 0;
          video->prediction_type = IPPVC_MC_FIELD;
          COPY_PMV(video->vector, video->PMV);
          if (!video->macroblock_motion_backward) {
            if (!video->PMV[0] && !video->PMV[1]) {
              id_his_new = 4;
            }
          } else if (!video->macroblock_motion_forward) {
            if (!video->PMV[2] && !video->PMV[3]) {
              id_his_new = 5;
              prev_index = frame_buffer.frame_p_c_n[frame_buffer.curr_index[task_num]].next_index;
            }
          }
        }

        if (id_his_new) {
          Ipp32s curr_index = frame_buffer.curr_index[task_num];
          Ipp8u *ref_Y_data = frame_buffer.frame_p_c_n[prev_index].Y_comp_data;
          Ipp8u *ref_U_data = frame_buffer.frame_p_c_n[prev_index].U_comp_data;
          Ipp8u *ref_V_data = frame_buffer.frame_p_c_n[prev_index].V_comp_data;
          Ipp8u *cur_Y_data = frame_buffer.frame_p_c_n[curr_index].Y_comp_data;
          Ipp8u *cur_U_data = frame_buffer.frame_p_c_n[curr_index].U_comp_data;
          Ipp8u *cur_V_data = frame_buffer.frame_p_c_n[curr_index].V_comp_data;
          IppiSize roi = {16*macroblock_address_increment, 16};
          ippiCopy_8u_C1R(ref_Y_data + offset_l, pitch_l1, cur_Y_data + offset_l, pitch_l1, roi);
          roi.height = 1 << ROW_CHROMA_SHIFT_420;
          roi.width >>= 1;
          ippiCopy_8u_C1R(ref_U_data + offset_c, pitch_c1, cur_U_data + offset_c, pitch_c1, roi);
          ippiCopy_8u_C1R(ref_V_data + offset_c, pitch_c1, cur_V_data + offset_c, pitch_c1, roi);
        } else {
          video->mb_address_increment = macroblock_address_increment + 1;
          if (video->macroblock_motion_forward && video->macroblock_motion_backward) {
            mc_mp2_420b_skip(video, task_num);
          } else {
            mc_mp2_420_skip(video, task_num);
          }
        }
      } // skipped macroblocks
      video->mb_col += macroblock_address_increment;
      video->col_l = video->mb_col << 4;
      video->col_c = video->mb_col << 3;
      video->offset_l += macroblock_address_increment << 4;
      video->offset_c += macroblock_address_increment << 3;
    }
    if (video->mb_col >= sequenceHeader.mb_width[task_num]) {
      return UMC_ERR_INVALID_STREAM;
    }

    DECODE_VLC(macroblock_type, video->bs, vlcMBType[PictureHeader[task_num].picture_coding_type - 1]);

    video->macroblock_motion_forward = macroblock_type & IPPVC_MB_FORWARD;
    video->macroblock_motion_backward= macroblock_type & IPPVC_MB_BACKWARD;

    if(macroblock_type & IPPVC_MB_INTRA)
    {
      if(macroblock_type & IPPVC_MB_QUANT)
      {
        DECODE_QUANTIZER_SCALE(video->bs, video->cur_q_scale);
      }

      if(!PictureHeader[task_num].concealment_motion_vectors)
      {
        RESET_PMV(video->PMV)
      }
      else
      {
        //Ipp32s field_sel;
        //Ipp32s code;

        video->prediction_type = IPPVC_MC_FIELD;
        //GET_1BIT(video->bs, field_sel);
        SKIP_BITS(video->bs,1);//field_sel

        mv_decode(0, 0, video, task_num);
        video->PMV[4] = video->PMV[0];
        video->PMV[5] = video->PMV[1];

        //GET_1BIT(video->bs, code);
        SKIP_BITS(video->bs,1);
      }

      RECONSTRUCT_INTRA_MB_420(video->bs, 2);
      continue;
    }//intra

    video->dct_dc_past[0] =
    video->dct_dc_past[1] =
    video->dct_dc_past[2] = (Ipp16s)PictureHeader[task_num].curr_reset_dc;

    if (video->macroblock_motion_forward || video->macroblock_motion_backward) {
      GET_TO9BITS(video->bs, 2, video->prediction_type);
    }

    if(macroblock_type & IPPVC_MB_QUANT) {
      DECODE_QUANTIZER_SCALE(video->bs, video->cur_q_scale);
    }

    Ipp32s curr_index = frame_buffer.curr_index[task_num];
    Ipp8u *cur_Y_data = frame_buffer.frame_p_c_n[curr_index].Y_comp_data;
    Ipp8u *cur_U_data = frame_buffer.frame_p_c_n[curr_index].U_comp_data;
    Ipp8u *cur_V_data = frame_buffer.frame_p_c_n[curr_index].V_comp_data;
    video->blkCurrYUV[0] = cur_Y_data + video->offset_l;
    video->blkCurrYUV[1] = cur_U_data + video->offset_c;
    video->blkCurrYUV[2] = cur_V_data + video->offset_c;

    if (video->macroblock_motion_forward) {
      if (video->prediction_type == IPPVC_MC_DP) {
        mc_dualprime_field_420(video, task_num);
      } else {
        mc_field_forward_420(video, task_num);
        if (video->macroblock_motion_backward) {
          mc_field_backward_add_420(video, task_num);
        }
      }
    } else {
      if (video->macroblock_motion_backward) {
        mc_field_backward_420(video, task_num);
      } else {
        RESET_PMV(video->PMV)
        mc_field_forward0_420(video, task_num);
      }
    }

    if (macroblock_type & IPPVC_MB_PATTERN) {
      RECONSTRUCT_INTER_MB_420(video->bs, 2);
    }
  }
#if (defined UMC_VA_DXVA && defined ELK) || defined(UMC_VA_LINUX)
  if(pack_w. m_va)
  {
#if defined UMC_VA_DXVA
    pack_w.pSliceInfo->wHorizontalPosition   = (WORD)macroblock_address_increment;
    pack_w.pSliceInfo->wNumberMBsInSlice     = (WORD)(sequenceHeader.mb_width[task_num] - pack_w.pSliceInfo->wHorizontalPosition);

    if(pack_w.pSliceInfo->wHorizontalPosition > 0 && pack_w.pSliceInfoBuffer < pack_w.pSliceInfo)
        pack_w.pSliceInfo[-1].wNumberMBsInSlice  = (WORD)(macroblock_address_increment - pack_w.pSliceInfo[-1].wHorizontalPosition);
#elif defined UMC_VA_LINUX
    // add logic?
#endif
  }
  return UMC_OK;
#endif

}//DecodeSlice_FieldPB_420

#endif // UMC_ENABLE_MPEG2_VIDEO_DECODER
