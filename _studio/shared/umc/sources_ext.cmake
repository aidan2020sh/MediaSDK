# Copyright (c) 2017-2020 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if(MFX_DISABLE_SW_FALLBACK AND NOT BUILD_VAL_TOOLS)
  return()
endif()

### UMC io va
target_sources(umc_va_hw
  PRIVATE
    io/umc_va/include/umc_hevc_ddi.h
    io/umc_va/include/umc_jpeg_ddi.h
    io/umc_va/include/umc_mvc_ddi.h
    io/umc_va/include/umc_va_dxva2.h
    io/umc_va/include/umc_va_dxva2_protected.h
    io/umc_va/include/umc_win_event_cache.h

    io/umc_va/src/umc_va_dxva2.cpp
    io/umc_va/src/umc_va_dxva2_protected.cpp
    io/umc_va/src/umc_win_event_cache.cpp
  )

### UMC codec asf_spl
add_library(asf_spl STATIC)
set_property(TARGET asf_spl PROPERTY FOLDER "umc")

target_sources(asf_spl
  PRIVATE
    codec/asf_spl/include/umc_asf_fb.h
    codec/asf_spl/include/umc_asf_guids.h
    codec/asf_spl/include/umc_asf_parser.h
    codec/asf_spl/include/umc_asf_spl.h

    codec/asf_spl/src/umc_asf_fb.cpp
    codec/asf_spl/src/umc_asf_parser.cpp
    codec/asf_spl/src/umc_asf_spl.cpp
  )

target_include_directories(asf_spl
  PUBLIC
    codec/asf_spl/include
  )

target_compile_options(asf_spl
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(asf_spl PUBLIC vm_plus PRIVATE media_buffers)
### UMC codec asf_spl

### UMC codec avi_spl
add_library(avi_spl STATIC)
set_property(TARGET avi_spl PROPERTY FOLDER "umc")

target_sources(avi_spl
  PRIVATE
    codec/avi_spl/include/umc_avi_spl_chunk.h
    codec/avi_spl/include/umc_avi_splitter.h
    codec/avi_spl/include/umc_avi_types.h

    codec/avi_spl/src/umc_avi_spl_chunk.cpp
    codec/avi_spl/src/umc_avi_splitter.cpp
  )

target_include_directories(avi_spl
  PUBLIC
    codec/avi_spl/include
  )

target_compile_options(avi_spl
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(avi_spl PUBLIC vm_plus PRIVATE spl_common media_buffers)
### UMC codec avi_spl

### UMC codec spl_common
add_library(spl_common STATIC)
set_property(TARGET spl_common PROPERTY FOLDER "umc")

target_sources(spl_common
  PRIVATE
    codec/spl_common/include/umc_index_spl.h

    codec/spl_common/src/umc_index_spl.cpp
  )

target_include_directories(spl_common
  PUBLIC
    codec/spl_common/include
  )

target_compile_options(spl_common
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(spl_common PUBLIC vm_plus PRIVATE common media_buffers)
### UMC codec spl_common

### UMC codec common
add_library(common STATIC)
set_property(TARGET common PROPERTY FOLDER "umc")

target_sources(common
  PRIVATE
    codec/aac_common/include/aaccmn_adif.h
    codec/aac_common/include/aaccmn_adts.h
    codec/aac_common/include/aaccmn_chmap.h
    codec/aac_common/include/aaccmn_huff.h
    codec/aac_common/include/aac_dec_own.h
    codec/aac_common/include/aac_enc_huff_tables.h
    codec/aac_common/include/aac_enc_own.h
    codec/aac_common/include/aac_enc_search.h
    codec/aac_common/include/aac_filterbank_fp.h
    codec/aac_common/include/aac_sfb_tables.h
    codec/aac_common/include/aac_status.h
    codec/aac_common/include/aac_wnd_tables_fp.h
    codec/aac_common/include/ps_dec_parser.h
    codec/aac_common/include/ps_dec_settings.h
    codec/aac_common/include/ps_dec_struct.h
    codec/aac_common/include/sbr_dec_parser.h
    codec/aac_common/include/sbr_dec_settings.h
    codec/aac_common/include/sbr_dec_struct.h
    codec/aac_common/include/sbr_freq_tabs.h
    codec/aac_common/include/sbr_huff_tabs.h
    codec/aac_common/include/sbr_settings.h
    codec/aac_common/include/sbr_struct.h
    codec/mp3_common/include/mp3dec.h
    codec/mp3_common/include/mp3dec_huftabs.h
    codec/mp3_common/include/mp3dec_own.h
    codec/mp3_common/include/mp3enc.h
    codec/mp3_common/include/mp3enc_hufftables.h
    codec/mp3_common/include/mp3enc_own.h
    codec/mp3_common/include/mp3enc_psychoacoustic.h
    codec/mp3_common/include/mp3enc_tables.h
    codec/mp3_common/include/mp3_alloc_tab.h
    codec/mp3_common/include/mp3_own.h
    codec/mp3_common/include/mp3_status.h
    codec/common/include/align.h
    codec/common/include/audio_codec_params.h
    codec/common/include/bstream.h
    codec/common/include/mp4cmn_config.h
    codec/common/include/mp4cmn_const.h
    codec/common/include/mp4cmn_pce.h
    codec/common/include/umc_aac_decoder_params.h
    codec/common/include/umc_aac_encoder_params.h
    codec/common/include/umc_mp3_decoder_params.h
    codec/common/include/umc_mp3_encoder_params.h

    codec/aac_common/src/aaccmn_adif.c
    codec/aac_common/src/aaccmn_adts.c
    codec/aac_common/src/aaccmn_chmap.c
    codec/aac_common/src/aaccmn_huff.c
    codec/aac_common/src/aac_dec_api.c
    codec/aac_common/src/aac_dec_stream_elements.c
    codec/aac_common/src/aac_enc_elements.c
    codec/aac_common/src/aac_enc_huff_tables.c
    codec/aac_common/src/aac_enc_search.c
    codec/aac_common/src/aac_filterbank_fp.c
    codec/aac_common/src/aac_sfb_tables.c
    codec/aac_common/src/aac_win_tables_fp.c
    codec/aac_common/src/ps_dec_parser.c
    codec/aac_common/src/ps_huff_tabs.c
    codec/aac_common/src/sbr_dec_env_decoding.c
    codec/aac_common/src/sbr_dec_parser.c
    codec/aac_common/src/sbr_dec_reset.c
    codec/aac_common/src/sbr_freq_tabs.c
    codec/aac_common/src/sbr_huff_tabs.c
    codec/aac_common/src/sbr_pow_vec.c
    codec/mp3_common/src/mp3dec_api.c
    codec/mp3_common/src/mp3dec_common.c
    codec/mp3_common/src/mp3dec_layer1.c
    codec/mp3_common/src/mp3dec_layer2.c
    codec/mp3_common/src/mp3dec_layer3.c
    codec/mp3_common/src/mp3enc_bitstream.c
    codec/mp3_common/src/mp3enc_common.c
    codec/mp3_common/src/mp3enc_huff.c
    codec/mp3_common/src/mp3enc_hufftables.c
    codec/mp3_common/src/mp3enc_quantization.c
    codec/mp3_common/src/mp3enc_tables.c
    codec/mp3_common/src/mp3_alloc_tab.c
    codec/mp3_common/src/mp3_common.c
    codec/common/src/aaccmn_cce.c
    codec/common/src/aac_win_tables_int.c
    codec/common/src/bstream.c
    codec/common/src/mp4cmn_config.c
    codec/common/src/mp4cmn_const.c
    codec/common/src/mp4cmn_pce.c
  )

target_include_directories(common
  PUBLIC
    include
    codec/common/include
  PRIVATE
    codec/aac_common/include
    codec/mp3_common/include
  )

target_link_libraries(common PUBLIC vm_plus)
### UMC codec common

### UMC codec demuxer
add_library(demuxer STATIC)
set_property(TARGET demuxer PROPERTY FOLDER "umc")

target_sources(demuxer
  PRIVATE
    codec/demuxer/include/umc_demuxer.h
    codec/demuxer/include/umc_demuxer_defs.h
    codec/demuxer/include/umc_mpeg2ps_parser.h
    codec/demuxer/include/umc_mpeg2ts_parser.h
    codec/demuxer/include/umc_stream_parser.h
    codec/demuxer/include/umc_threaded_demuxer.h

    codec/demuxer/src/umc_demuxer.cpp
    codec/demuxer/src/umc_demuxer_defs.cpp
    codec/demuxer/src/umc_mpeg2ps_parser.cpp
    codec/demuxer/src/umc_mpeg2ts_parser.cpp
    codec/demuxer/src/umc_stream_parser.cpp
    codec/demuxer/src/umc_threaded_demuxer.cpp
  )

target_include_directories(demuxer
  PUBLIC
    codec/demuxer/include
  )

target_compile_options(demuxer
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(demuxer PUBLIC vm_plus IPP::s IPP::vc IPP::dc IPP::i IPP::cc IPP::cv IPP::j IPP::msdk IPP::core PRIVATE media_buffers)
### UMC codec demuxer

### UMC codec vc1_spl
add_library(vc1_spl STATIC)
set_property(TARGET vc1_spl PROPERTY FOLDER "umc")

target_sources(vc1_spl
  PRIVATE
    codec/vc1_spl/include/umc_vc1_spl.h
    codec/vc1_spl/include/umc_vc1_spl_defs.h

    codec/vc1_spl/src/umc_vc1_spl.cpp
  )

target_include_directories(vc1_spl
  PUBLIC
    codec/vc1_spl/include
  )

target_compile_options(vc1_spl
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(vc1_spl PUBLIC vm_plus PRIVATE vc1_common)
### UMC codec vc1_spl

### UMC codec mpeg4_spl
add_library(mpeg4_spl STATIC)
set_property(TARGET mpeg4_spl PROPERTY FOLDER "umc")

target_sources(mpeg4_spl
  PRIVATE
    codec/mpeg4_spl/include/umc_mp4_parser.h
    codec/mpeg4_spl/include/umc_mp4_spl.h
    codec/mpeg4_spl/include/umc_mp4_video_parser.h

    codec/mpeg4_spl/src/umc_mp4_parser.cpp
    codec/mpeg4_spl/src/umc_mp4_spl.cpp
    codec/mpeg4_spl/src/umc_mp4_spl_common.cpp
    codec/mpeg4_spl/src/umc_mp4_video_parser.cpp
  )

target_include_directories(mpeg4_spl
  PUBLIC
  codec/mpeg4_spl/include
  )

target_compile_options(mpeg4_spl
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(mpeg4_spl PUBLIC vm_plus PRIVATE spl_common common media_buffers)

### UMC codec bitrate_control

target_sources(bitrate_control
  PRIVATE
    codec/brc/include/umc_svc_brc.h
    codec/brc/src/umc_svc_brc.cpp
  )

### UMC codec bitrate_control

### UMC codec mpeg4_spl

add_library(scene_analyzer STATIC)
set_property(TARGET scene_analyzer PROPERTY FOLDER "umc")

target_sources(scene_analyzer
  PRIVATE
    codec/scene_analyzer/src/umc_scene_analyzer.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_base.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_frame.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_mb_func.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_p.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_p_func.cpp
    codec/scene_analyzer/src/umc_scene_analyzer_p_func_mb.cpp
    codec/scene_analyzer/src/umc_video_data_scene_info.cpp
  )

#target_compile_definitions (bitrate_control PRIVATE ${API_FLAGS} ${WARNING_FLAGS})
target_include_directories(scene_analyzer
  PUBLIC
    codec/scene_analyzer/include
  )

target_link_libraries(scene_analyzer PUBLIC umc)

### UMC codec color_space_converter
add_subdirectory(codec/color_space_converter)
### UMC codec color_space_converter

### UMC codec vc1_common
add_subdirectory(codec/vc1_common)
### UMC codec vc1_common

add_subdirectory(codec/vc1_enc)
add_subdirectory(codec/vc1_dec)

add_subdirectory(codec/jpeg_common)
add_subdirectory(codec/jpeg_dec)
add_subdirectory(codec/jpeg_enc)

add_subdirectory(codec/h264_enc)
add_subdirectory(codec/h264_dec)
add_subdirectory(codec/h265_dec)
add_subdirectory(codec/h264_spl)
add_subdirectory(codec/mpeg2_enc)
add_subdirectory(codec/mpeg2_dec/hw)
add_subdirectory(codec/mpeg2_dec/sw)

add_subdirectory(codec/me)

add_subdirectory(codec/mp3_common)
add_subdirectory(codec/mp3_dec)
add_subdirectory(codec/aac_common)
add_subdirectory(codec/aac_dec)
add_subdirectory(codec/aac_enc)

### UMC io
add_subdirectory(io/umc_io)
### UMC io

### UMC io media buffers
add_subdirectory(io/media_buffers)
### UMC io media buffers

add_subdirectory(codec)

add_subdirectory(test_suite/spy_test_component/)
add_subdirectory(test_suite/outline_diff/)
