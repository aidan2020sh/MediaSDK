##******************************************************************************
##  Copyright(C) 2012-2020 Intel Corporation. All Rights Reserved.
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
##  Content: Intel(R) Media SDK projects creation and build
##******************************************************************************

if(NOT DEFINED BUILDER_ROOT)
  set( BUILDER_ROOT ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/builder )
endif()

get_filename_component( MSDK_BUILD_ROOT_MINUS_ONE "${BUILDER_ROOT}/.." ABSOLUTE )

set( MSDK_STUDIO_ROOT  ${MSDK_BUILD_ROOT_MINUS_ONE}/_studio )
set( MSDK_TSUITE_ROOT  ${MSDK_BUILD_ROOT_MINUS_ONE}/_testsuite )
set( MSDK_LIB_ROOT     ${MSDK_STUDIO_ROOT}/mfx_lib )
set( MSDK_UMC_ROOT     ${MSDK_STUDIO_ROOT}/shared/umc )
set( MSDK_SAMPLES_ROOT ${MSDK_BUILD_ROOT_MINUS_ONE}/samples )
set( MSDK_TOOLS_ROOT   ${MSDK_BUILD_ROOT_MINUS_ONE}/tools )
set( MSDK_BUILDER_ROOT ${BUILDER_ROOT} )

function( mfx_include_dirs )
  include_directories (
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_UMC_ROOT}/core/vm/include
    ${MSDK_UMC_ROOT}/core/vm_plus/include
    ${MSDK_UMC_ROOT}/core/umc/include
    ${MSDK_UMC_ROOT}/io/umc_io/include
    ${MSDK_UMC_ROOT}/io/umc_va/include
    ${MSDK_UMC_ROOT}/io/media_buffers/include
    ${MSDK_LIB_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/optimization/h265/include
    ${MSDK_LIB_ROOT}/optimization/h264/include
    ${MSDK_LIB_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/fei/include
    ${MSDK_LIB_ROOT}/fei/h264_la
  )
  if (MFX_DISABLE_SW_FALLBACK)
    include_directories(
      ${CMAKE_HOME_DIRECTORY}/contrib/ipp/include
    )
  else()
    include_directories (
      ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/SafeStringStaticLibrary/include
    )
  endif()
endfunction()

#================================================

add_library(mfx_common_properties INTERFACE)

add_library(mfx_static_lib INTERFACE)
target_include_directories(mfx_static_lib
  INTERFACE
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_LIB_ROOT}/shared/include
    )

target_compile_definitions(mfx_static_lib
  INTERFACE
    $<$<PLATFORM_ID:Windows>:
      MFX_D3D11_ENABLED
      _UNICODE
      UNICODE
    >
    ${API_FLAGS}
    ${WARNING_FLAGS}
  )

target_link_libraries(mfx_static_lib INTERFACE mfx_common_properties)

#================================================

add_library(mfx_shared_lib INTERFACE)
target_link_options(mfx_shared_lib
  INTERFACE
    # $<$<PLATFORM_ID:Windows>:/NODEFAULTLIB:libcmtd.lib>
    $<$<AND:$<PLATFORM_ID:Windows>,$<CONFIG:Debug>>:/NODEFAULTLIB:libcpmt.lib>
  )

target_link_libraries(mfx_shared_lib
  INTERFACE
    $<$<PLATFORM_ID:Windows>:
      dxva2.lib
      D3D11.lib
      DXGI.lib
      d3d9.lib
    >
    mfx_common_properties
  )

target_compile_definitions(mfx_shared_lib
  INTERFACE
    ${API_FLAGS}
    ${WARNING_FLAGS}
  )

#================================================

add_library(mfx_plugin_properties INTERFACE)

target_link_options(mfx_plugin_properties
  INTERFACE
    $<$<PLATFORM_ID:Windows>:
      LINKER:/DEF:${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.def>
    $<$<PLATFORM_ID:Linux>:
      LINKER:--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map>
  )

target_link_libraries(mfx_plugin_properties
  INTERFACE
    umc_va_hw
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)
