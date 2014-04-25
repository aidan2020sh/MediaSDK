/*//////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2012-2014 Intel Corporation. All Rights Reserved.
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_VP8_VIDEO_DECODE_HW) && (defined(MFX_VA_WIN) || defined(MFX_VA))

#include "mfx_session.h"
#include "mfx_common_decode_int.h"
#include "mfx_vp8_dec_decode_hw.h"
#include "mfx_enc_common.h"

#include "umc_va_base.h"
#include "umc_va_dxva2.h"

#include "vm_sys_info.h"

#ifndef MFX_VA_WIN

#include <va/va.h>
#include <va/va_dec_vp8.h>

#endif

#include <iostream>
#include <sstream>
#include <fstream>

#include "mfx_vp8_dec_decode_common.h"

#ifndef MFX_VA_WIN

mfxStatus VideoDECODEVP8_HW::PackHeaders(mfxBitstream *p_bistream)
{

     mfxStatus sts = MFX_ERR_NONE;

     sFrameInfo info = m_frames.back();

   /////////////////////////////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufPic;
     VAPictureParameterBufferVP8 *picParams
         = (VAPictureParameterBufferVP8*)m_p_video_accelerator->GetCompBuffer(VAPictureParameterBufferType, &compBufPic,
                                                                              sizeof(VAPictureParameterBufferVP8));

     //frame width in pixels
     picParams->frame_width = m_frame_info.frameSize.width;
     //frame height in pixels
     picParams->frame_height = m_frame_info.frameSize.height;

     //specifies the "last" reference frame
     picParams->last_ref_frame = 0xffffffff;

     //specifies the "golden" reference frame
     picParams->golden_ref_frame = 0xffffffff;

     //specifies the "alternate" referrence frame
     picParams->alt_ref_frame = 0xffffffff;

     // specifies the out-of-loop deblocked frame, not used currently
     picParams->out_of_loop_frame = 0xffffffff;

     picParams->pic_fields.value = 0;

     static int refI = 0;


     if (I_PICTURE == m_frame_info.frameType)
     {
         refI = info.currIndex;
         //same as key_frame in bitstream syntax
         picParams->pic_fields.bits.key_frame = 0;

         picParams->last_ref_frame = 0;
         picParams->golden_ref_frame = 0;
         picParams->alt_ref_frame = 0;
         picParams->out_of_loop_frame = 0;

     }
     else // inter frame
     {

         picParams->pic_fields.bits.key_frame = 1;

         picParams->last_ref_frame    =  m_p_video_accelerator->GetSurfaceID(info.lastrefIndex);
         picParams->golden_ref_frame  =  m_p_video_accelerator->GetSurfaceID(info.goldIndex);
         picParams->alt_ref_frame     =  m_p_video_accelerator->GetSurfaceID(info.altrefIndex);

         picParams->out_of_loop_frame =  0x0;
     }

     curr_indx += 1;

     //same as version in bitstream syntax
     picParams->pic_fields.bits.version = m_frame_info.version;

     //same as segmentation_enabled in bitstream syntax
     picParams->pic_fields.bits.segmentation_enabled = m_frame_info.segmentationEnabled;

     //same as update_mb_segmentation_map in bitstream syntax
     picParams->pic_fields.bits.update_mb_segmentation_map = m_frame_info.updateSegmentMap;

     //same as update_segment_feature_data in bitstream syntax
     picParams->pic_fields.bits.update_segment_feature_data = m_frame_info.updateSegmentData;

     //same as filter_type in bitstream syntax
     picParams->pic_fields.bits.filter_type = m_frame_info.loopFilterType;

     //same as sharpness_level in bitstream syntax
     picParams->pic_fields.bits.sharpness_level = m_frame_info.sharpnessLevel;

     //same as loop_filter_adj_enable in bitstream syntax
     picParams->pic_fields.bits.loop_filter_adj_enable = m_frame_info.mbLoopFilterAdjust;

     //same as mode_ref_lf_delta_update in bitstream syntax
     picParams->pic_fields.bits.mode_ref_lf_delta_update =  m_frame_info.modeRefLoopFilterDeltaUpdate;

     //same as sign_bias_golden in bitstream syntax
     picParams->pic_fields.bits.sign_bias_golden = 0;

     //same as sign_bias_alternate in bitstream syntax
     picParams->pic_fields.bits.sign_bias_alternate = 0;

     if (I_PICTURE != m_frame_info.frameType)
     {
         picParams->pic_fields.bits.sign_bias_golden = m_refresh_info.refFrameBiasTable[3];
         picParams->pic_fields.bits.sign_bias_alternate = m_refresh_info.refFrameBiasTable[2];
     }

     //same as mb_no_coeff_skip in bitstream syntax
     picParams->pic_fields.bits.mb_no_coeff_skip = m_frame_info.mbSkipEnabled;

     //see section 11.1 for mb_skip_coefffff
     picParams->pic_fields.bits.mb_skip_coeff = 0;

     //flag to indicate that loop filter should be disabled
     picParams->pic_fields.bits.loop_filter_disable = 0;

//     picParams->LoopFilterDisable = 0;
/*     if (0 == m_frame_info.loopFilterLevel || (2 == m_frame_info.version || 3 == m_frame_info.version))
     {
         picParams->pic_fields.bits.loop_filter_disable = 1;
     }*/

/*     if ((picParams->pic_fields.bits.version == 0) || (picParams->pic_fields.bits.version == 1))
     {
         picParams->pic_fields.bits.loop_filter_disable = m_frame_info.loopFilterLevel > 0 ? 1 : 0;
     }*/

     // probabilities of the segment_id decoding tree and same as
     // mb_segment_tree_probs in the spec.
     picParams->mb_segment_tree_probs[0] = m_frame_info.segmentTreeProbabilities[0];
     picParams->mb_segment_tree_probs[1] = m_frame_info.segmentTreeProbabilities[1];
     picParams->mb_segment_tree_probs[2] = m_frame_info.segmentTreeProbabilities[2];

     //Post-adjustment loop filter levels for the 4 segments
     // TO DO


     int baseline_filter_level[VP8_MAX_NUM_OF_SEGMENTS];

     #define SEGMENT_ABSDATA 1
     #define MAX_LOOP_FILTER 63

    // Macroblock level features
    typedef enum
    {
       MB_LVL_ALT_Q = 0,   /* Use alternate Quantizer .... */
       MB_LVL_ALT_LF = 1,  /* Use alternate loop filter value... */
       MB_LVL_MAX = 2      /* Number of MB level features supported */
    } MB_LVL_FEATURES;

     if (m_frame_info.segmentationEnabled)
     {
         for (int i = 0;i < VP8_MAX_NUM_OF_SEGMENTS;i++)
         {
             if (m_frame_info.segmentAbsMode)
             {
                 baseline_filter_level[i] = m_frame_info.segmentFeatureData[MB_LVL_ALT_LF][i];
             }
             else
             {
                 baseline_filter_level[i] = m_frame_info.loopFilterLevel + m_frame_info.segmentFeatureData[MB_LVL_ALT_LF][i];
                 baseline_filter_level[i] = (baseline_filter_level[i] >= 0) ? ((baseline_filter_level[i] <= MAX_LOOP_FILTER) ? baseline_filter_level[i] : MAX_LOOP_FILTER) : 0;  /* Clamp to valid range */
             }
         }
     }
     else
     {
         for (int i = 0; i < VP8_MAX_NUM_OF_SEGMENTS; i++)
         {
             baseline_filter_level[i] = m_frame_info.loopFilterLevel;
         }
     }

     for (int i = 0; i < VP8_MAX_NUM_OF_SEGMENTS; i++)
     {
         picParams->loop_filter_level[i] = baseline_filter_level[i];
     }

     //loop filter deltas for reference frame based MB level adjustment
     picParams->loop_filter_deltas_ref_frame[0] = m_frame_info.refLoopFilterDeltas[0];
     picParams->loop_filter_deltas_ref_frame[1] = m_frame_info.refLoopFilterDeltas[1];
     picParams->loop_filter_deltas_ref_frame[2] = m_frame_info.refLoopFilterDeltas[2];
     picParams->loop_filter_deltas_ref_frame[3] = m_frame_info.refLoopFilterDeltas[3];

     //loop filter deltas for coding mode based MB level adjustment
     picParams->loop_filter_deltas_mode[0] = m_frame_info.modeLoopFilterDeltas[0];
     picParams->loop_filter_deltas_mode[1] = m_frame_info.modeLoopFilterDeltas[1];
     picParams->loop_filter_deltas_mode[2] = m_frame_info.modeLoopFilterDeltas[2];
     picParams->loop_filter_deltas_mode[3] = m_frame_info.modeLoopFilterDeltas[3];

     //same as prob_skip_false in bitstream syntax
     picParams->prob_skip_false = m_frame_info.skipFalseProb;

     //same as prob_intra in bitstream syntax
     picParams->prob_intra = m_frame_info.intraProb;

     //same as prob_last in bitstream syntax
     picParams->prob_last = m_frame_info.lastProb;

     //same as prob_gf in bitstream syntax
     picParams->prob_gf = m_frame_info.goldProb;

     //list of 4 probabilities of the luma intra prediction mode decoding
     //tree and same as y_mode_probs in frame header
     //list of 3 probabilities of the chroma intra prediction mode decoding
     //tree and same as uv_mode_probs in frame header

     const mfxU8 *prob_y_table;
     const mfxU8 *prob_uv_table;

     if (I_PICTURE == m_frame_info.frameType)
     {
         prob_y_table = vp8_kf_mb_mode_y_probs;
         prob_uv_table = vp8_kf_mb_mode_uv_probs;
     }
     else
     {
         prob_y_table = m_frameProbs.mbModeProbY;
         prob_uv_table = m_frameProbs.mbModeProbUV;
     }

     for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_Y - 1; i += 1)
     {
         picParams->y_mode_probs[i] = prob_y_table[i];
     }

     for (Ipp32u i = 0; i < VP8_NUM_MB_MODES_UV - 1; i += 1)
     {
         picParams->uv_mode_probs[i] = prob_uv_table[i];
     }

     //updated mv decoding probabilities and same as mv_probs in frame header
     for (Ipp32u i = 0; i < VP8_NUM_MV_PROBS; i += 1)
     {
         picParams->mv_probs[0][i] = m_frameProbs.mvContexts[0][i];
         picParams->mv_probs[1][i] = m_frameProbs.mvContexts[1][i];
     }

     picParams->bool_coder_ctx.range = m_boolDecoder[VP8_FIRST_PARTITION].range();
     picParams->bool_coder_ctx.value = (m_boolDecoder[VP8_FIRST_PARTITION].value() >> 24) & 0xff;
     picParams->bool_coder_ctx.count = (8 - (m_boolDecoder[VP8_FIRST_PARTITION].bitcount() & 0x7));

/*     {
         static int i = 0;
         std::stringstream ss;
         ss << "/home/user/ivanenko/mfxdump/pp" << i;
         std::ofstream ofs(ss.str().c_str(), std::ofstream::binary);
         i++;
         ofs.write((char*)picParams, sizeof(VAPictureParameterBufferVP8));
         ofs.flush();
         ofs.close();
     } */

     compBufPic->SetDataSize(sizeof(VAPictureParameterBufferVP8));

     //////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufCp;
     VAProbabilityDataBufferVP8 *coeffProbs = (VAProbabilityDataBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VAProbabilityBufferType, &compBufCp, sizeof(VAProbabilityDataBufferVP8));

     std::copy(reinterpret_cast<const char*>(m_frameProbs.coeff_probs), 
               reinterpret_cast<const char*>(m_frameProbs.coeff_probs) + sizeof(m_frameProbs.coeff_probs), 
               reinterpret_cast<char*>(coeffProbs));

/*     {
         static int i = 0;
         std::stringstream ss;
         ss << "/home/user/ivanenko/mfxdump/pd" << i;
         std::ofstream ofs(ss.str().c_str(), std::ofstream::binary);
         i++;
         ofs.write((char*)coeffProbs, sizeof(VAProbabilityDataBufferVP8));
         ofs.flush();
         ofs.close();
     } */

//     compBufCp->SetDataSize(sizeof(Ipp8u) * 4 * 8 * 3 * 11);
     compBufCp->SetDataSize(sizeof(VAProbabilityDataBufferVP8));

     //////////////////////////////////////////////////////////////////
     UMCVACompBuffer* compBufQm;
     VAIQMatrixBufferVP8 *qmTable = (VAIQMatrixBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VAIQMatrixBufferType, &compBufQm, sizeof(VAIQMatrixBufferVP8));
     
     if (m_frame_info.segmentationEnabled == 0)
     {

         // when segmentation is disabled, use the first entry 0 for the quantization values
         qmTable->quantization_index[0][1] = (unsigned char)m_quantInfo.ydcQ[0];
         qmTable->quantization_index[0][0] = (unsigned char)m_quantInfo.yacQ[0];
         qmTable->quantization_index[0][4] = (unsigned char)m_quantInfo.uvdcQ[0];
         qmTable->quantization_index[0][5] = (unsigned char)m_quantInfo.uvacQ[0];
         qmTable->quantization_index[0][2] = (unsigned char)m_quantInfo.y2dcQ[0];
         qmTable->quantization_index[0][3] = (unsigned char)m_quantInfo.y2acQ[0];
     }
     else
     {

         for (Ipp32u i = 0; i < 4; i += 1)
         {
             qmTable->quantization_index[i][1] = (unsigned char)m_quantInfo.ydcQ[i];
             qmTable->quantization_index[i][0] = (unsigned char)m_quantInfo.yacQ[i];
             qmTable->quantization_index[i][4] = (unsigned char)m_quantInfo.uvdcQ[i];
             qmTable->quantization_index[i][5] = (unsigned char)m_quantInfo.uvacQ[i];
             qmTable->quantization_index[i][2] = (unsigned char)m_quantInfo.y2dcQ[i];
             qmTable->quantization_index[i][3] = (unsigned char)m_quantInfo.y2acQ[i];
         }
     }

/*     {
         static int i = 0;
         std::stringstream ss;
         ss << "/home/user/ivanenko/mfxdump/iq" << i;
         std::ofstream ofs(ss.str().c_str(), std::ofstream::binary);
         i++;
         ofs.write((char*)qmTable, sizeof(VAIQMatrixBufferVP8));
         ofs.flush();
         ofs.close();
     } */

     compBufQm->SetDataSize(sizeof(VAIQMatrixBufferVP8));

     //////////////////////////////////////////////////////////////////

     Ipp32u offset = 0;

     if (I_PICTURE == m_frame_info.frameType)
         offset = 10;
     else
         offset = 3;

     Ipp32s size = p_bistream->DataLength;

     UMCVACompBuffer* compBufSlice;

     VASliceParameterBufferVP8 *sliceParams
         = (VASliceParameterBufferVP8*)m_p_video_accelerator->
             GetCompBuffer(VASliceParameterBufferType, &compBufSlice, sizeof(VASliceParameterBufferVP8));

     #ifdef ANDROID

     // number of bytes in the slice data buffer for the partitions
     sliceParams->slice_data_size = (Ipp32s)size - offset;

     //offset to the first byte of partition data
     sliceParams->slice_data_offset = 0;

     //see VA_SLICE_DATA_FLAG_XXX definitions
     sliceParams->slice_data_flag = VA_SLICE_DATA_FLAG_ALL;

     #endif

     //offset to the first bit of MB from the first byte of partition data
     sliceParams->macroblock_offset = m_frame_info.entropyDecSize;

     // Partitions
     sliceParams->num_of_partitions = m_frame_info.numPartitions;

     sliceParams->partition_size[0] = m_frame_info.firstPartitionSize;

     for (Ipp32s i = 1; i < m_frame_info.numPartitions + 1; i += 1)
     {
         sliceParams->partition_size[i] = m_frame_info.partitionSize[i - 1];
     }

/*     {
         static int i = 0;
         std::stringstream ss;
         ss << "/home/user/ivanenko/mfxdump/sp" << i;
         std::ofstream ofs(ss.str().c_str(), std::ofstream::binary);
         i++;
         ofs.write((char*)sliceParams, sizeof(VASliceParameterBufferVP8));
         ofs.flush();
         ofs.close();
     } */

     compBufSlice->SetDataSize(sizeof(VASliceParameterBufferVP8));

     UMCVACompBuffer* compBufBs;
     Ipp8u *bistreamData = (Ipp8u *)m_p_video_accelerator->GetCompBuffer(VASliceDataBufferType, &compBufBs, p_bistream->DataLength - offset);

     Ipp8u *pBuffer = (Ipp8u*) p_bistream->Data;

     std::copy(pBuffer + offset, pBuffer + size, bistreamData);

/*     {
         static int i = 0;
         std::stringstream ss;
         ss << "/home/user/ivanenko/mfxdump/sd" << i;
         std::ofstream ofs(ss.str().c_str(), std::ofstream::binary);
         i++;
         ofs.write((char*)(pBuffer + offset), size - offset);
         ofs.flush();
         ofs.close();
     } */

     compBufBs->SetDataSize((Ipp32s)size - offset);

     //////////////////////////////////////////////////////////////////

     return sts;

} // Status VP8VideoDecoderHardware::PackHeaders(MediaData* src)

#endif

#endif