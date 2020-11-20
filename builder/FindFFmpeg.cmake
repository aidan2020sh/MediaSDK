##******************************************************************************
##  Copyright(C) 2014-2020 Intel Corporation. All Rights Reserved.
##
##  The source code, information  and  material ("Material") contained herein is
##  owned  by Intel Corporation or its suppliers or licensors, and title to such
##  Material remains  with Intel Corporation  or its suppliers or licensors. The
##  Material  contains proprietary information  of  Intel or  its  suppliers and
##  licensors. The  Material is protected by worldwide copyright laws and treaty
##  provisions. No  part  of  the  Material  may  be  used,  copied, reproduced,
##  modified, published, uploaded, posted, transmitted, distributed or disclosed
##  in any way  without Intel's  prior  express written  permission. No  license
##  under  any patent, copyright  or  other intellectual property rights  in the
##  Material  is  granted  to  or  conferred  upon  you,  either  expressly,  by
##  implication, inducement,  estoppel or  otherwise.  Any  license  under  such
##  intellectual  property  rights must  be express  and  approved  by  Intel in
##  writing.
##
##  *Third Party trademarks are the property of their respective owners.
##
##  Unless otherwise  agreed  by Intel  in writing, you may not remove  or alter
##  this  notice or  any other notice embedded  in Materials by Intel or Intel's
##  suppliers or licensors in any way.
##
##******************************************************************************
##  Content: Intel(R) Media SDK Samples projects creation and build
##******************************************************************************

if (Linux)

  pkg_check_modules(PKG_LIBVAUTIL libavutil>=52.38.100)
  pkg_check_modules(PKG_LIBAVCODEC libavcodec>=55.18.102)
  pkg_check_modules(PKG_LIBAVFORMAT libavformat>=55.12.100)

  if(PKG_LIBVAUTIL_FOUND AND
    PKG_LIBAVCODEC_FOUND AND
    PKG_LIBAVFORMAT_FOUND)
    set( FFMPEG_FOUND TRUE )
      message( STATUS "FFmpeg headers and libraries were found." )
  endif()

elseif (Windows)

  if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set( ffmpeg_arch x64 )
  else()
    set( ffmpeg_arch win32)
  endif()

  set ( windows_ffmpeg_hint ENV{INTELMEDIASDK_FFMPEG_ROOT}/lib/${ffmpeg_arch} )

  find_library(LIBAVUTIL_LIBRARY NAMES libavutil HINTS ${windows_ffmpeg_hint})
  find_library(LIBAVCODEC_LIBRARY NAMES libavcodec HINTS ${windows_ffmpeg_hint})
  find_library(LIBAVFORMAT_LIBRARY NAMES libavformat HINTS ${windows_ffmpeg_hint})

  if(LIBAVUTIL_LIBRARY_FOUND AND
     LIBAVCODEC_LIBRARY_FOUND AND
     LIBAVFORMAT_LIBRARY)
     set( FFMPEG_FOUND TRUE )

     foreach (lib_name avutil avcodec avformat)
       add_library(${lib_name} STATIC IMPORTED)
       set_target_properties(${lib_name} PROPERTIES IMPORTED_LOCATION "${windows_ffmpeg_hint}/${lib_name}.lib")
       target_include_directories(${lib_name} INTERFACE $ENV{INTELMEDIASDK_FFMPEG_ROOT}/include)
     endforeach()

     message( STATUS "FFmpeg headers and libraries were found." )
  endif()

else ( )
   message( STATUS "FFmpeg headers and libraries were not searched at all." )
endif()

if(NOT DEFINED FFMPEG_FOUND)
  message( STATUS "FFmpeg headers and libraries were not found (optional). The following will not be built: sample_spl_mux." )
endif()

