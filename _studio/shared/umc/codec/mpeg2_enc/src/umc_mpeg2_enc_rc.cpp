//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2005-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_MPEG2_VIDEO_ENCODER)

#include "umc_mpeg2_enc_defs.h"

using namespace UMC;

/* quant scale values table ISO/IEC 13818-2, 7.4.2.2 table 7-6 */
static Ipp32s Val_QScale[2][32] =
{
  /* linear q_scale */
  {0,  2,  4,  6,  8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,
  32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62},
  /* non-linear q_scale */
  {0, 1,  2,  3,  4,  5,  6,  7,  8, 10, 12, 14, 16, 18, 20, 22,
  24, 28, 32, 36, 40, 44, 48, 52, 56, 64, 72, 80, 88, 96,104,112}
};

Ipp32s MPEG2VideoEncoderBase::PictureRateControl(Ipp64s bits_in_headers)
{
  Ipp32s isfield = (picture_structure != MPEG2_FRAME_PICTURE);
  Ipp64f ip_delay;
  Ipp32s q0, q2, sz1;

#ifndef UMC_RESTRICTED_CODE
  //if(encodeInfo.info.bitrate <= 0) {
  //  changeQuant(qscale[picture_coding_type-MPEG2_I_PICTURE]); // changes also intra_dc_precision, q_scale_type
  //  vbv_delay = 0xffff;
  //  return quantiser_scale_value;
  //}
#endif // UMC_RESTRICTED_CODE

  if(encodeInfo.rc_mode == RC_CBR || vbv_delay != 0xffff) {
    vbv_delay = (Ipp32s)((rc_vbv_fullness-bits_in_headers) * 90000.0/encodeInfo.info.bitrate);
  }
  if(vbv_delay < 0)
    vbv_delay = 0; // for a while
  if(vbv_delay > 0xffff)
    vbv_delay = 0xffff; // for a while

  if(progressive_frame) {
    rc_delay = rc_ave_frame_bits;
    if(repeat_first_field) rc_delay += rc_ave_frame_bits;
    if(top_field_first) rc_delay += rc_ave_frame_bits;
  } else {
    rc_delay = rc_ave_frame_bits;
    if(!isfield) rc_delay += rc_ave_frame_bits;
    if(repeat_first_field) rc_delay += rc_ave_frame_bits;
    rc_delay = rc_delay / 2;
  }
  if (!bQuantiserChanged && !bSceneChanged && picture_coding_type != MPEG2_B_PICTURE) {
    ip_delay = rc_delay;
    if(!isfield) {
      rc_delay = rc_ip_delay;
      rc_ip_delay = ip_delay;
    } else if(second_field) {
      rc_delay = rc_ip_delay - rc_delay;
      rc_ip_delay = 2*ip_delay;
    }
  }

  // refresh rate deviation with every new I frame
  if(picture_coding_type == MPEG2_I_PICTURE &&
    m_FirstFrame == 0 && encodeInfo.rc_mode == RC_CBR) {
    Ipp64f ip_tagsize = rc_tagsize[0];
    if(encodeInfo.FieldPicture)
      ip_tagsize = (rc_tagsize[0] + rc_tagsize[1]) * .5; // every second I-field became P-field
    rc_dev =
      encodeInfo.VBV_BufferSize * 16384 / 2 // half of vbv_buffer
      + ip_tagsize / 2                      // top to center length of I (or IP) frame
      - rc_vbv_fullness;
  }

  if ( bQuantiserChanged )
    return quantiser_scale_value;

  // vbv computations for len range
  rc_vbv_max = (Ipp32s)rc_vbv_fullness;
  if (encodeInfo.rc_mode != RC_CBR) {
    if(encodeInfo.rc_mode == RC_UVBR)
      rc_vbv_max = IPP_MAX_32S; // no underflow for unrestricted VBR
    rc_vbv_min = 0; // no overflow in VBR
    q2 = encodeInfo.quant_vbr[picture_coding_type-MPEG2_I_PICTURE];
  } else {
    rc_vbv_min = (Ipp32s)(rc_vbv_fullness - (encodeInfo.VBV_BufferSize*16384 - rc_delay));
    q0 = qscale[picture_coding_type-MPEG2_I_PICTURE];   // proposed from post picture
    q2 = prqscale[picture_coding_type-MPEG2_I_PICTURE]; // last used scale
    sz1 = prsize[picture_coding_type-MPEG2_I_PICTURE];  // last coded size

    // compute newscale again
    // adaptation to current rate deviation
    Ipp64f target_size = rc_tagsize[picture_coding_type-MPEG2_I_PICTURE];
    Ipp32s wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);

    if     (sz1 > 2*wanted_size)   q2 = q2 * 3 / 2 + 1;
    else if(sz1 > wanted_size+100) q2 ++;
    else if(2*sz1 < wanted_size)   q2 = q2 * 3 / 4;
    else if(sz1 < wanted_size-100 && q2>2) q2 --;
    //if(picture_coding_type == MPEG2_I_PICTURE && q2 > 32)
    //  q2 = 32;
    //if(picture_coding_type == MPEG2_P_PICTURE && q2 > 48)
    //  q2 = 48;
    if(rc_dev > 0) {
      q2 = IPP_MAX(q0,q2);
    } else {
      q2 = IPP_MIN(q0,q2);
    }
  }
  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about to change scale code if value changes
  q2 = changeQuant(q2);

  qscale[picture_coding_type-MPEG2_I_PICTURE] = q2;

  return quantiser_scale_value;
}


// encoded size check, vbv computation, qscale code adaptation
// returns positive bitcount when vbv overflow,
//         negative bitcount when vbv underflow, 0 if OK
Ipp32s MPEG2VideoEncoderBase::PostPictureRateControl(Ipp64s bits_encoded)
{
  Ipp32s isfield = (picture_structure != MPEG2_FRAME_PICTURE);
  Ipp64s vbv_size = encodeInfo.VBV_BufferSize * 16384;
  Ipp32s ret = 0;
#ifndef UMC_RESTRICTED_CODE
//{
//  static Ipp32s i = 0;
//  FILE* fout = fopen("c:/tv/z_m2q.csv","at");
//  if(fout) {
//    if(i==0)
//      fprintf(fout,"frame number,type,bits,qscale,rc deviation,vbv_delay\n");
//    fprintf(fout,"%d,%d,%d,%d,%d,%d\n",
//      i,picture_coding_type,(Ipp32s)bits_encoded,qscale[picture_coding_type-MPEG2_I_PICTURE],
//      (Ipp32s)rc_dev,(Ipp32s)vbv_delay);
//    fclose(fout);
//  }
//  i++;
//}
#endif // UMC_RESTRICTED_CODE
  prsize[picture_coding_type-MPEG2_I_PICTURE] = (Ipp32s)(bits_encoded << isfield);
  prqscale[picture_coding_type-MPEG2_I_PICTURE] = qscale[picture_coding_type-MPEG2_I_PICTURE];

#ifndef UMC_RESTRICTED_CODE
  //if(encodeInfo.info.bitrate <= 0) {
  //  return 0;
  //}

  /*printf("Pic:%d, str:%d, sc:%3d, vbvh:%7d, vbvl:%7d, bits:%6d, delay:%6d, rcdev:%7d\n",
    picture_coding_type, picture_structure, qscale[picture_coding_type-MPEG2_I_PICTURE],
    (Ipp32s)(vbv_size-rc_vbv_fullness), (Ipp32s)(rc_vbv_fullness - bits_encoded),
    (Ipp32s)bits_encoded, (Ipp32s)rc_delay, (Ipp32s)rc_dev);*/
#endif // UMC_RESTRICTED_CODE

  rc_vbv_fullness = rc_vbv_fullness - bits_encoded;
  rc_vbv_fullness += rc_delay; //
  if(encodeInfo.rc_mode != RC_CBR && rc_vbv_fullness >= vbv_size) {
    rc_vbv_fullness = (Ipp64f) vbv_size;
    vbv_delay = 0xffff; // input
  }

  if(encodeInfo.rc_mode != RC_CBR)
    return 0;

  Ipp32s cur_qscale = qscale[picture_coding_type-MPEG2_I_PICTURE];
  Ipp64f target_size = rc_tagsize[picture_coding_type-MPEG2_I_PICTURE];
  rc_dev += bits_encoded - (isfield ? target_size/2 : target_size);
  Ipp32s wanted_size = (Ipp32s)(target_size - rc_dev / 3 * target_size / rc_tagsize[0]);
  Ipp32s newscale;
  Ipp32s del_sc;

  newscale = cur_qscale;
  wanted_size >>= isfield;
  if(bits_encoded > wanted_size) newscale ++;
  if(bits_encoded > 2*wanted_size) newscale = cur_qscale * 3 / 2 + 1;
  //if(picture_coding_type == MPEG2_I_PICTURE && newscale > 32)
  //  newscale = 32;
  //if(picture_coding_type == MPEG2_P_PICTURE && newscale > 48)
  //  newscale = 48;
  if(bits_encoded < wanted_size && cur_qscale>2) newscale --;
  if(2*bits_encoded < wanted_size) newscale = cur_qscale * 3 / 4;
  // this call is used to accept small changes in value, which are mapped to the same code
  // changeQuant bothers about to change scale code if value changes
  newscale = changeQuant(newscale);

  if(picture_coding_type == MPEG2_I_PICTURE) {
    if( newscale+1 > qscale[1] ) qscale[1] = newscale+1;
    if( newscale+2 > qscale[2] ) qscale[2] = newscale+2;
  } else if(picture_coding_type == MPEG2_P_PICTURE) {
    if( newscale < qscale[0] ) {
      del_sc = qscale[0] - newscale;
      qscale[0] -= del_sc/2;
      newscale = qscale[0];
      newscale = changeQuant(newscale);
    }
    if( newscale+1 > qscale[2] ) qscale[2] = newscale+1;
  } else {
    if( newscale < qscale[1] ) {
      del_sc = qscale[1] - newscale;
      qscale[1] -= del_sc/2;
      newscale = qscale[1];
      newscale = changeQuant(newscale);
      if( qscale[1] < qscale[0] ) qscale[0] = qscale[1];
    }
  }

  qscale[picture_coding_type-MPEG2_I_PICTURE] = newscale;

  if(rc_vbv_fullness > vbv_size) {
    ret = (Ipp32s)(rc_vbv_fullness - vbv_size);
  }
  if(rc_vbv_fullness < rc_delay) {
    ret = (Ipp32s)(rc_vbv_fullness - rc_delay);
  }

  return ret;
}

Ipp32s MPEG2VideoEncoderBase::InitRateControl(Ipp32s BitRate)
{
  Ipp64f ppb; //pixels per bit (~ density)
  Ipp32s i;
  Ipp64f nr;
  Ipp64f gopw;
  Ipp64f u_len, ip_weight;

  encodeInfo.info.bitrate = BitRate;

  if (encodeInfo.info.bitrate <= 0) { // no bitrate given - UVBR
    encodeInfo.rc_mode = RC_UVBR;
    // some big value
    encodeInfo.info.bitrate = encodeInfo.info.clip_info.width*encodeInfo.info.clip_info.height / 8 * 400;
    vbv_delay = 0xffff; // unused
  }
  if(encodeInfo.rc_mode != RC_CBR) {
    encodeInfo.quant_vbr[0] =
    qscale[0] = ((encodeInfo.quant_vbr[0] > 0) ? encodeInfo.quant_vbr[0] : 4);
    encodeInfo.quant_vbr[1] =
    qscale[1] = ((encodeInfo.quant_vbr[1] > 0) ? encodeInfo.quant_vbr[1] : (qscale[0]+1));
    encodeInfo.quant_vbr[2] =
    qscale[2] = ((encodeInfo.quant_vbr[2] > 0) ? encodeInfo.quant_vbr[2] : (qscale[1]+1));
  }

  // if 0 - could be no rc
  rc_ave_frame_bits = encodeInfo.info.bitrate/encodeInfo.info.framerate;
  rc_ip_delay = rc_ave_frame_bits;
  // we want ideal case - I frame to be centered in vbv buffer, compute start pos
  if(encodeInfo.VBV_BufferSize <= (Ipp32s)(rc_ave_frame_bits / 16384 * 4) ) { // avoid too small
    encodeInfo.VBV_BufferSize = (Ipp32s)(rc_ave_frame_bits / 16384 * 8);
  }
  encodeInfo.VBV_BufferSize = IPP_MIN(encodeInfo.VBV_BufferSize, (encodeInfo.mpeg1 ? 0x3fe : 0x3fffe));
  encodeInfo.m_SuggestedOutputSize = 2 * encodeInfo.VBV_BufferSize * (16384/8); // in bytes
  encodeInfo.m_SuggestedOutputSize = align_value<Ipp32u>(encodeInfo.m_SuggestedOutputSize);

  // one can vary weights, can be added to API
  if(encodeInfo.rc_mode == RC_CBR) {
    rc_weight[0] = 120;
    rc_weight[1] = 50;
    rc_weight[2] = 25;
    ip_weight = rc_weight[0];
    if(encodeInfo.FieldPicture)
      ip_weight = (rc_weight[0] + rc_weight[1]) * .5; // every second I-field became P-field
    nr = encodeInfo.gopSize/encodeInfo.IPDistance;
    gopw = (encodeInfo.IPDistance-1)*rc_weight[2]*nr + rc_weight[1]*(nr-1) + ip_weight;
    u_len = rc_ave_frame_bits * encodeInfo.gopSize / gopw;
    rc_tagsize[0] = u_len * rc_weight[0];
    rc_tagsize[1] = u_len * rc_weight[1];
    rc_tagsize[2] = u_len * rc_weight[2];

    rc_vbv_fullness =
      encodeInfo.VBV_BufferSize * 16384 / 2 +      // half of vbv_buffer
      u_len * ip_weight / 2 +                      // top to center length of I frame
      (encodeInfo.IPDistance-1) * (rc_ave_frame_bits - rc_tagsize[2]); // first gop has no M-1 B frames: add'em

    vbv_delay = (Ipp32s)(rc_vbv_fullness*90000.0/encodeInfo.info.bitrate); // bits to clocks

    rc_dev = 0; // deviation from ideal bitrate (should be Ipp32f or renewed)

    Ipp64f rrel = gopw / (rc_weight[0] * encodeInfo.gopSize);
    ppb = encodeInfo.info.clip_info.width*encodeInfo.info.clip_info.height*encodeInfo.info.framerate/encodeInfo.info.bitrate * (block_count-2) / (6-2);

    qscale[0] = (Ipp32s)(6.0 * rrel * ppb); // numbers are empiric
    qscale[1] = (Ipp32s)(9.0 * rrel * ppb);
    qscale[2] = (Ipp32s)(12.0 * rrel * ppb);
  } else {
    rc_vbv_fullness = encodeInfo.VBV_BufferSize * 16384; // full buffer
    vbv_delay = (Ipp32s)(rc_vbv_fullness*90000.0/encodeInfo.info.bitrate); // bits to clocks
  }
  for(i=0; i<3; i++) {
    if     (qscale[i]< 1) qscale[i]= 1;
    else if(qscale[i]>63) qscale[i]=63; // can be even more
    if(prqscale[i] == 0) prqscale[i] = qscale[i];
    if(prsize[i]   == 0) prsize[i] = (Ipp32s)rc_tagsize[i]; // for first iteration
  }
  return 0;
}

Ipp32s MPEG2VideoEncoderBase::mapQuant(Ipp32s quant_value)
{
  Ipp32s qs_type, qs_code;
  if(encodeInfo.mpeg1 || (quant_value > 7 && quant_value <= 62)) {
    qs_type = 0;
    qs_code = (quant_value + 1) >> 1;
  } else { // non-linear quantizer
    qs_type = 1;
    if(quant_value <= 8) qs_code = quant_value;
    else /* if(quant_value > 62) */ qs_code = 25+((quant_value-64+4)>>3);
  }
  if(qs_code < 1) qs_code = 1;
  if(qs_code > 31) qs_code = 31;
  return Val_QScale[qs_type][qs_code];
}

Ipp32s MPEG2VideoEncoderBase::changeQuant(Ipp32s quant_value)
{
  //Ipp32s curq = m_fpar.quant_scale_value;
  Ipp32s curq = quantiser_scale_value;

  if(quant_value == quantiser_scale_value) return quantiser_scale_value;
  if(encodeInfo.mpeg1 || (quant_value > 7 && quant_value <= 62)) {
    q_scale_type = 0;
    quantiser_scale_code = (quant_value + 1) >> 1;
  } else { // non-linear quantizer
    q_scale_type = 1;
    if(quant_value <= 8) quantiser_scale_code = quant_value;
    else if(quant_value > 62) quantiser_scale_code = 25+((quant_value-64+4)>>3);
  }
  if(quantiser_scale_code < 1) quantiser_scale_code = 1;
  if(quantiser_scale_code > 31) quantiser_scale_code = 31;
  quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];
  if(quantiser_scale_value == curq) {
    if(quant_value > curq)
    {
      if(quantiser_scale_code == 31) return quantiser_scale_value;
      else quantiser_scale_code ++;
    }
    if(quant_value < curq)
    {
      if(quantiser_scale_code == 1) return quantiser_scale_value;
      else quantiser_scale_code --;
    }
    quantiser_scale_value = Val_QScale[q_scale_type][quantiser_scale_code];
  }

  if(encodeInfo.mpeg1 || quantiser_scale_value >= 8)
    intra_dc_precision = 0;
  else if(quantiser_scale_value >= 4)
    intra_dc_precision = 1;
  else
    intra_dc_precision = 2;
  // only for High profile
  if(encodeInfo.profile == 1 && quantiser_scale_value == 1)
    intra_dc_precision = 3;

  //Ipp32s qq = quantiser_scale_value*quantiser_scale_value;
  varThreshold = quantiser_scale_value;// + qq/32;
  meanThreshold = quantiser_scale_value * 8;// + qq/32;
  sadThreshold = (quantiser_scale_value + 0) * 8;// + qq/16;

  return quantiser_scale_value;
}
#ifndef UMC_RESTRICTED_CODE
//void MPEG2VideoEncoderBase::AdjustSearchRange(Ipp32s B_count, Ipp32s direction)
//{
//  Ipp32s rmax[2];
//  Ipp32s rangeO[2], rangeI[2];
//  const Ipp32s r = 4;
//  Ipp32s co[2], ci[2], cnz[2], i, c;
//  Ipp32s dx=0, dxF=0, dxB, dxFB, dx2;
//  Ipp32s dy=0, dyF=0, dyB, dyFB, dy2;
//  Ipp32s sz=0, szF=0, szB, szFB, sz2;
//  Ipp32s den=0, denF=0, denB, denFB;
//  Ipp32s type = 0;
//
//  if(direction == 1) return;
//  if(picture_coding_type != MPEG2_P_PICTURE) return;
//
//  rmax[0] = pMotionData[0].searchRange[0][0];
//  rmax[1] = pMotionData[0].searchRange[0][1];
//  rangeO[0] = 2*rmax[0] - r;
//  rangeO[1] = 2*rmax[1] - r;
//  rangeI[0] = 2*rmax[0] >> 1;
//  rangeI[1] = 2*rmax[1] >> 1;
//
//  co[0] = co[1] = ci[0] = ci[1] = cnz[0] = cnz[1] = 0;
//  for (i = 0; i < MBcount; i ++) {
//    if (!(pMBInfo[i].mb_type & MB_INTRA)) {
//      dx=0, dxF=0;
//      dy=0, dyF=0;
//      sz=0, szF=0;
//      den=0, denF=0;
//      type = 0;
//      if(picture_coding_type == MPEG2_P_PICTURE || (pMBInfo[i].mb_type & MB_FORWARD)){
//        dxF= pMBInfo[i].MV[0][0].x;
//        dyF = pMBInfo[i].MV[0][0].y;
//        szF = dxF*dxF + dyF*dyF;
//        if(pMBInfo[i].prediction_type == MC_FIELD) {
//          dx2 = pMBInfo[i].MV[1][0].x;
//          dy2 = pMBInfo[i].MV[1][0].y;
//          sz2 = dx2*dx2 + dy2*dy2;
//          if(szF < sz2) {
//            dxF = dx2;
//            dyF = dy2;
//            szF = sz2;
//          }
//        }
//        type = MB_FORWARD;
//        den = denF = (picture_coding_type == MPEG2_P_PICTURE) ? P_distance : B_count;
//        dx = dxF;
//        dy = dyF;
//        sz = szF;
//      }
//      if(picture_coding_type == MPEG2_B_PICTURE) {
//        if(pMBInfo[i].mb_type & MB_BACKWARD) { // B
//          //Ipp32s B_countR = P_distance - B_count;
//          dxB = pMBInfo[i].MV[0][1].x;
//          dyB = pMBInfo[i].MV[0][1].y;
//          szB = dxB*dxB + dyB*dyB;
//          denB = P_distance - B_count;
//          if(pMBInfo[i].prediction_type == MC_FIELD) {
//            dx2 = pMBInfo[i].MV[1][1].x;
//            dy2 = pMBInfo[i].MV[1][1].y;
//            sz2 = dx2*dx2 + dy2*dy2;
//            if(szB < sz2) {
//              dxB = dx2;
//              dyB = dy2;
//              szB = sz2;
//            }
//          }
//          if(type != MB_FORWARD || szF*denB*denB < szB*denF*denF) {
//            sz = szB;
//            den = denB;
//            type = MB_BACKWARD;
//            dx = dxB;
//            dy = dyB;
//          }
//          if(pMBInfo[i].mb_type & MB_FORWARD) { // FB
//            dxFB = dxF - dxB;
//            dyFB = dyF - dyB;
//            szFB = dxFB*dxFB + dyFB*dyFB;
//            denFB = P_distance;
//            if(sz*denFB*denFB < szFB*den*den) {
//              den = denFB;
//              type = MB_BACKWARD | MB_FORWARD;
//              sz = szFB;
//              dx = dxFB;
//              dy = dyFB;
//            }
//          }
//        }
//      }
//      //if(dx == 0 && dy == 0) continue;
//      dx = abs(dx);
//      dy = abs(dy);
//      if (dx * P_distance >= rangeO[0]*den) co[0] ++;
//      if (dx * P_distance >= rangeI[0]*den) ci[0] ++;
//      if (dy * P_distance >= rangeO[1]*den) co[1] ++;
//      if (dy * P_distance >= rangeI[1]*den) ci[1] ++;
//      if(dx != 0) cnz[0] ++;
//      if(dy != 0) cnz[1] ++;
//    }
//  }
//
//  for(c=0; c<2; c++) {
//    if (co[c] * 10 >= cnz[c]) {
//      if (rmax[c] <= 64) {
//        rmax[c] *= 2;
//      }
//    } else if (ci[c] * 10 <= cnz[c]) {
//      if (rmax[c] > 4) {
//        rmax[c] /= 2;
//      }
//    }
//    pMotionData[0].searchRange[0][c] = rmax[c];
//    Ipp32s req_f_code = 1;
//    while ((4<<req_f_code) < rmax[c]) {
//      req_f_code++;
//    }
//    pMotionData[0].f_code[0][c] = req_f_code;
//  }
//}
#endif // UMC_RESTRICTED_CODE

#endif // UMC_ENABLE_MPEG2_VIDEO_ENCODER
