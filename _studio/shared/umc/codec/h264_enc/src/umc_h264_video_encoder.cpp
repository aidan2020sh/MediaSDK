// Copyright (c) 2004-2019 Intel Corporation
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
#if defined(MFX_ENABLE_H264_VIDEO_ENCODE)

#if defined _OPENMP
#include "omp.h"
#endif // _OPENMP

#include "vm_sys_info.h"
#include "umc_par_file_util.h"

#include "umc_memory_allocator.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_defs.h"
#include "umc_h264_tables.h"
#include "umc_h264_deblocking_tools.h"
#include "vm_debug.h"
#include "umc_par_file_util.h"
#include "umc_h264_to_ipp.h"

namespace UMC
{

VideoEncoder *CreateH264Encoder()
{
    VideoEncoder *enc;
    vm_debug_trace(VM_DEBUG_INFO, VM_STRING("CreateH264Encoder()"));
    enc = new H264VideoEncoder;
    return enc;
}

H264VideoEncoder::H264VideoEncoder()
{
    m_CurrEncoderType = H264_VIDEO_ENCODER_NONE;
    m_pEncoder_8u_16s = NULL;  // For 8 bit video.

#if defined BITDEPTH_9_12
    m_pEncoder_16u_32s = NULL; // For 9-12 bit video.
#endif // BITDEPTH_9_12
    m_pBrc = NULL;
}

H264VideoEncoder::~H264VideoEncoder()
{
    if(m_pEncoder_8u_16s != NULL) {
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
        H264ENC_CALL_DELETE(H264CoreEncoder, m_pEncoder_8u_16s);
#undef H264ENC_MAKE_NAME
        m_pEncoder_8u_16s = NULL;
    }
#if defined BITDEPTH_9_12
    if(m_pEncoder_16u_32s != NULL) {
#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s
        H264ENC_CALL_DELETE(H264CoreEncoder, m_pEncoder_16u_32s);
#undef H264ENC_MAKE_NAME
        m_pEncoder_16u_32s = NULL;
    }
#endif // BITDEPTH_9_12
    m_pBrc = NULL;
}

Status H264VideoEncoder::Init(BaseCodecParams *init)
{
    Status res = UMC_OK;

    //Init base class (needed for external memory allocator)
    if( (res = BaseCodec::Init( init )) != UMC_OK ) return res;

#if defined BITDEPTH_9_12
    H264EncoderParams *info = DynamicCast<H264EncoderParams, BaseCodecParams> (init);
    if(info == NULL) {
        // 8 bits
        m_CurrEncoderType = H264_VIDEO_ENCODER_8U_16S;

#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
        H264ENC_CALL_NEW(res, H264CoreEncoder, m_pEncoder_8u_16s);
#undef H264ENC_MAKE_NAME

        if (res != UMC_OK)
            return(res);

        res = H264CoreEncoder_SetBRC_8u16s(m_pEncoder_8u_16s, m_pBrc);
        if (res != UMC_OK)
          return(res);

        return H264CoreEncoder_Init_8u16s(m_pEncoder_8u_16s, init, m_pMemoryAllocator);
    }
    if(info->bit_depth_luma > 8 || info->bit_depth_chroma > 8 ||
        (info->aux_format_idc && info->bit_depth_aux > 8)) {
        // 16 bits
            m_CurrEncoderType = H264_VIDEO_ENCODER_16U_32S;

#define H264ENC_MAKE_NAME(NAME) NAME##_16u32s
            H264ENC_CALL_NEW(res, H264CoreEncoder, m_pEncoder_16u_32s);
#undef H264ENC_MAKE_NAME

            if (res != UMC_OK)
                return(res);

            res = H264CoreEncoder_SetBRC_16u32s(m_pEncoder_16u_32s, m_pBrc);
            if (res != UMC_OK)
              return(res);

            return H264CoreEncoder_Init_16u32s(m_pEncoder_16u_32s, init, m_pMemoryAllocator);
    }
#endif // BITDEPTH_9_12
    // 8 bits
    m_CurrEncoderType = H264_VIDEO_ENCODER_8U_16S;

#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
    H264ENC_CALL_NEW(res, H264CoreEncoder, m_pEncoder_8u_16s);
#undef H264ENC_MAKE_NAME

    if (res != UMC_OK)
        return(res);

    res = H264CoreEncoder_SetBRC_8u16s(m_pEncoder_8u_16s, m_pBrc);
    if (res != UMC_OK)
      return(res);

    return H264CoreEncoder_Init_8u16s(m_pEncoder_8u_16s, init, m_pMemoryAllocator);
}

Status H264VideoEncoder::GetFrame(MediaData * /*in*/, MediaData * /*out*/)
{
    return UMC_ERR_UNSUPPORTED;
}

Status H264VideoEncoder::GetInfo(BaseCodecParams *info)
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_GetInfo_8u16s(m_pEncoder_8u_16s, info);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_GetInfo_16u32s(m_pEncoder_16u_32s, info);
#endif // BITDEPTH_9_12
        default:
            return(UMC::UMC_ERR_NOT_INITIALIZED);
    }
}

const H264PicParamSet* H264VideoEncoder::GetPicParamSet()
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_GetPicParamSet_8u16s(m_pEncoder_8u_16s);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_GetPicParamSet_16u32s(m_pEncoder_16u_32s);
#endif // BITDEPTH_9_12
        default:
            return(NULL);
    }
}

const H264SeqParamSet* H264VideoEncoder::GetSeqParamSet()
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_GetSeqParamSet_8u16s(m_pEncoder_8u_16s);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_GetSeqParamSet_16u32s(m_pEncoder_16u_32s);
#endif // BITDEPTH_9_12
        default:
            return(NULL);
    }
}

Status H264VideoEncoder::Close()
{
    Status sts;
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            sts = H264CoreEncoder_Close_8u16s(m_pEncoder_8u_16s);
            break;
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            sts = H264CoreEncoder_Close_16u32s(m_pEncoder_16u_32s);
            break;
#endif // BITDEPTH_9_12
        default:
            return(UMC::UMC_ERR_NOT_INITIALIZED);
    }
    BaseCodec::Close();
    return sts;
}

Status H264VideoEncoder::Reset()
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_Reset_8u16s(m_pEncoder_8u_16s);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_Reset_16u32s(m_pEncoder_16u_32s);
#endif // BITDEPTH_9_12
        default:
            return(UMC::UMC_ERR_NOT_INITIALIZED);
    }
}

Status H264VideoEncoder::SetParams(BaseCodecParams* params)
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_SetParams_8u16s(m_pEncoder_8u_16s, params);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_SetParams_16u32s(m_pEncoder_16u_32s, params);
#endif // BITDEPTH_9_12
        default:
            return(UMC::UMC_ERR_NOT_INITIALIZED);
    }
}

VideoData* H264VideoEncoder::GetReconstructedFrame()
{
    switch(m_CurrEncoderType) {
        case H264_VIDEO_ENCODER_8U_16S:
            return H264CoreEncoder_GetReconstructedFrame_8u16s(m_pEncoder_8u_16s);
#if defined BITDEPTH_9_12
        case H264_VIDEO_ENCODER_16U_32S:
            return H264CoreEncoder_GetReconstructedFrame_16u32s(m_pEncoder_8u_16s);
#endif // BITDEPTH_9_12
        default:
            return NULL;
    }
}

Status H264EncoderParams::ReadParamFile(const vm_char * /*FileName*/)
{
#if 0 // Klocwork doesn't accept scanf (scanf_s is required)
#define SSCANF(N, X) if (N != vm_string_sscanf X) { vm_file_close(InputFile); return UMC_ERR_INVALID_STREAM; }

    vm_file* InputFile;
    vm_char line[256];
    Ipp32s arg0, arg1, arg2, arg3;
    Ipp32s frame_rate_code;

    InputFile = vm_file_open(FileName, VM_STRING("r"));
    if (!InputFile)
    {
        return UMC_ERR_OPEN_FAILED;
    }

    umc_file_fgets_ascii(line, 254, InputFile); // sequence name
    umc_file_fgets_ascii(line, 254, InputFile);
    //if (SrcFileName) SSCANF(1, (line, VM_STRING("%s"), SrcFileName));

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &numFramesToEncode));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"),
        &key_frame_controls.method,
        &key_frame_controls.interval,
        &key_frame_controls.idr_interval
        ));

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(2, (line, VM_STRING("%d %d"), &B_frame_rate, &treat_B_as_reference));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"), &num_ref_frames, &num_ref_to_start_code_B_slice, &arg2));
    num_slices = (Ipp16s)arg2;
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(2, (line, VM_STRING("%d %d"), &arg0, &arg1));
    profile_idc = (H264_PROFILE_IDC)arg0;
    level_idc = (Ipp8s)arg1;

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &info.clip_info.width));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &info.clip_info.height));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &frame_rate_code));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"),
        &chroma_format_idc, &bit_depth_luma, &bit_depth_chroma));
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(5, (line, VM_STRING("%d %d %d %d %d"),
        &aux_format_idc, &bit_depth_aux, &alpha_incr_flag, &alpha_opaque_value, &alpha_transparent_value));

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(5, (line,VM_STRING("%d %d %d %d %d"),
        &arg0, &arg1, &arg2, &arg3, &info.bitrate));

    rate_controls.method = (H264_Rate_Control_Method) arg0;
    rate_controls.quantI = (Ipp8s) arg1;
    rate_controls.quantP = (Ipp8s) arg2;
    rate_controls.quantB = (Ipp8s) arg3;

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(4, (line, VM_STRING("%d %d %d %d"),
        &mv_search_method,
        &me_split_mode,
        &me_search_x,
        &me_search_y
        ));

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"),
        &use_weighted_pred,
        &use_weighted_bipred,
        &use_implicit_weighted_bipred
        ));

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(2, (line, VM_STRING("%d %d"), &arg0, &use_direct_inference));
    direct_pred_mode = (Ipp8s) arg0;
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"),
        &arg0,
        &deblocking_filter_alpha,
        &deblocking_filter_beta));
    deblocking_filter_idc = (Ipp8s) arg0;
    deblocking_filter_alpha = deblocking_filter_alpha&~1; // must be even, since a value div2 is coded.
    deblocking_filter_beta = deblocking_filter_beta&~1;

    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %d"), &arg0, &use_default_scaling_matrix, &qpprime_y_zero_transform_bypass_flag));
    transform_8x8_mode_flag = arg0 ? true : false;
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &arg0)); //Old design
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &arg1));  //Old design
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(2, (line, VM_STRING("%d %d"), &arg0, &arg1));
    entropy_coding_mode = (Ipp8s)arg0; cabac_init_idc = (Ipp8s)arg1;
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(1, (line, VM_STRING("%d"), &coding_type));

#if defined H264_LOG
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(3, (line, VM_STRING("%d %d %s"), &m_QualitySpeed, &quant_opt_level, &m_log_file));
#endif
    umc_file_fgets_ascii(line, 254, InputFile); SSCANF(2, (line, VM_STRING("%d %d"), &m_QualitySpeed, &quant_opt_level));

    vm_file_close(InputFile);

    switch(frame_rate_code)
    {
    case 0:
        info.framerate = 30.000; break;
    case 1:
        info.framerate = 15.000; break;
    case 2:
        info.framerate = 24.000; break;
    case 3:
        info.framerate = 25.000; break;
    case 4:
        info.framerate = 30.000; break;
    case 5:
        info.framerate = 30.000; break;
    case 6:
        info.framerate = 50.000; break;
    case 7:
        info.framerate = 60.000; break;
    case 8:
        info.framerate = 60.000; break;
    default:
        info.framerate = 30.000;
        break;
    }
    return UMC_OK;
#else // Klocwork doesn't accept scanf (scanf_s is required)
    return UMC_ERR_FAILED;
#endif // Klocwork doesn't accept scanf (scanf_s is required)
}

H264EncoderParams::H264EncoderParams() : rate_controls(), m_SeqParamSet(), m_PicParamSet()
{
    key_frame_controls.method    = H264_KFCM_INTERVAL;
    key_frame_controls.interval  = 250; // for safety
    key_frame_controls.idr_interval = 0;
    B_frame_rate = 0;
    treat_B_as_reference = 1;
    num_ref_frames = 1;
    num_ref_to_start_code_B_slice = 1;
    num_slices = 0;  // Autoselect
    profile_idc = H264_MAIN_PROFILE;
    level_idc = 0;  //Autoselect
    chroma_format_idc = 1; // YUV 420.
    aspect_ratio_idc = 0;
    frame_crop_x = 0;
    frame_crop_w = 0;
    frame_crop_y = 0;
    frame_crop_h = 0;
    use_reset_refpiclist_for_intra = false;
    bit_depth_luma = 8;
    bit_depth_chroma = 8;
    aux_format_idc = 0;
    bit_depth_aux = 8;
    alpha_incr_flag = 0;
    alpha_opaque_value = 0;
    alpha_transparent_value = 0;
    rate_controls.method = H264_RCM_VBR;
    rate_controls.quantI = 20;
    rate_controls.quantP = 20;
    rate_controls.quantB = 20;
    info.bitrate = 2222222;
    mv_search_method = 2;
    mv_subpel_search_method = 2;
    me_split_mode = 0;
    me_search_x = 8;
    me_search_y = 8;
    max_mv_length_x = 127;
    max_mv_length_y = 127;
    use_weighted_pred = 0;
    use_weighted_bipred = 0;
    use_implicit_weighted_bipred = 0;
    direct_pred_mode = 0;
    use_direct_inference = 1;
    deblocking_filter_idc          = DEBLOCK_FILTER_ON;    // 0 is "on". 1 - "off"
    deblocking_filter_alpha        = 2;
    deblocking_filter_beta         = 2;
    transform_8x8_mode_flag = 1;
    use_default_scaling_matrix = 0;
    qpprime_y_zero_transform_bypass_flag =0;
    entropy_coding_mode = 1;
    cabac_init_idc = 1;
    coding_type = 0;
    m_do_weak_forced_key_frames = false;
    write_access_unit_delimiters = 0;
    use_transform_for_intra_decision = true;
    m_Analyse_on = 0;
    m_Analyse_restrict = 0;
    m_Analyse_ex = 0;
    numFramesToEncode = 0;
    m_QualitySpeed = 0;
    quant_opt_level = 0;
    use_ext_sps = 0;
    use_ext_pps = 0;
    m_ext_SPS_id = 0;
    m_ext_PPS_id = 0;
    m_ext_constraint_flags[0] = 0;
    m_ext_constraint_flags[1] = 0;
    m_ext_constraint_flags[2] = 0;
    m_ext_constraint_flags[3] = 0;
    m_ext_constraint_flags[4] = 0;
    m_ext_constraint_flags[5] = 0;
    reset_encoding_pipeline = true;
    is_resol_changed = true;
    m_SeqParamSet.profile_idc = H264_MAIN_PROFILE;

#if defined H264_LOG
    m_log = 0;
    *m_log_file = 0;
#endif
    m_is_mvc_profile = false;
    m_is_base_view = false;
    m_viewOutput = false;
}

} // end namespace UMC

using namespace UMC;

namespace UMC_H264_ENCODER {

#ifdef STORE_PICLIST
FILE *refpic;
#endif

//////////////////////////////////////////////////////////////////////////////
// InitDistScaleFactor
//  Calculates the scaling factor used for B slice temporal motion vector
//  scaling and for B slice bidir predictin weighting using the picordercnt
//  values from the current and both reference frames, saving the result
//  to the DistScaleFactor array for future use. The array is initialized
//  with out of range values whenever a bitstream unit is received that
//  might invalidate the data (for example a B slice header resulting in
//  modified reference picture lists). For scaling, the list1 [0] entry
//    is always used.
//////////////////////////////////////////////////////////////////////////////
#define CalculateDSF(index)                                                     \
    /* compute scaling ratio for temporal direct and implicit weighting*/   \
    tb = picCntCur - picCntRef0;    /* distance from previous */            \
    td = picCntRef1 - picCntRef0;    /* distance between ref0 and ref1 */   \
    \
    /* special rule: if td is 0 or if L0 is long-term reference, use */     \
    /* L0 motion vectors and equal weighting.*/                             \
    if (td == 0 ||                                                          \
        H264ENC_MAKE_NAME(H264EncoderFrame_isLongTermRef1)(                 \
            pRefPicList0[index],                                            \
            core_enc->m_field_index)){                                      \
        /* These values can be used directly in scaling calculations */     \
        /* to get back L0 or can use conditional test to choose L0.    */   \
        curr_slice->DistScaleFactor[L0Index][L1Index] = 128;    /* for equal weighting    */    \
        curr_slice->DistScaleFactorMV[L0Index][L1Index] = 256;                                  \
    }else{                                                                  \
        \
        tb = MAX(-128,tb);                                                  \
        tb = MIN(127,tb);                                                   \
        td = MAX(-128,td);                                                  \
        td = MIN(127,td);                                                   \
        \
        VM_ASSERT(td != 0);                                                    \
        \
        tx = (16384 + abs(td/2))/td;                                        \
        \
        DistScaleFactor = (tb*tx + 32)>>6;                                  \
        DistScaleFactor = MAX(-1024, DistScaleFactor);                      \
        DistScaleFactor = MIN(1023, DistScaleFactor);                       \
        \
        if (DistScaleFactor < -256 || DistScaleFactor > 512)                \
            curr_slice->DistScaleFactor[L0Index][L1Index] = 128;    /* equal weighting     */   \
        else                                                                \
            curr_slice->DistScaleFactor[L0Index][L1Index] = DistScaleFactor;                    \
        \
        curr_slice->DistScaleFactorMV[L0Index][L1Index] = DistScaleFactor;                      \
    }

#define PIXBITS 8
#include "umc_h264_video_encoder_tmpl.cpp.h"
#undef PIXBITS

#if defined BITDEPTH_9_12

#define PIXBITS 16
#include "umc_h264_video_encoder_tmpl.cpp.h"
#undef PIXBITS

#endif // BITDEPTH_9_12

} //namespace UMC_H264_ENCODER

#endif //MFX_ENABLE_H264_VIDEO_ENCODE

