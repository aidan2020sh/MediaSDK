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
##  Content: Intel(R) Media SDK Samples projects creation and build
##******************************************************************************

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

# the following options should disable rpath in both build and install cases
set( CMAKE_INSTALL_RPATH "" )
set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
set( CMAKE_SKIP_BUILD_RPATH TRUE )

collect_oses()

if( Linux OR Darwin )
  # If user did not override CMAKE_INSTALL_PREFIX, then set the default prefix
  # to /opt/intel/mediasdk instead of cmake's default
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX /opt/intel/mediasdk CACHE PATH "Install Path Prefix" FORCE )
  endif()

  include( GNUInstallDirs )

  set( common_flags "${common_flags} -DUNIX")

  if( Linux )
    set( common_flags "${common_flags} -D__USE_LARGEFILE64" )
    set( common_flags "${common_flags} -D_FILE_OFFSET_BITS=64" )

    set( common_flags "${common_flags} -DLINUX" )
    set( common_flags "${common_flags} -DLINUX32" )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set( common_flags "${common_flags} -DLINUX64" )
    endif()
  endif()

  if( Darwin )
    set( common_flags "${common_flags} -DOSX" )
    set( common_flags "${common_flags} -DOSX32" )

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set( common_flags "${common_flags} -DOSX64" )
    endif()
  endif()

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${MEDIA_VERSION_STR} 0 1 ver )

    set( common_flags "${common_flags} -DMSDK_BUILD=\\\"$ENV{BUILD_NUMBER}\\\"" )
  endif()

  if (CMAKE_C_COMPILER MATCHES icc)
    set(no_warnings "-Wno-deprecated -Wno-unknown-pragmas -Wno-unused -wd2304")
  else()
    set(no_warnings "-Wno-deprecated-declarations -Wno-unknown-pragmas -Wno-unused")
  endif()

  set(c_warnings "-Wall -Wformat -Wformat-security")
  set(cxx_warnings "${c_warnings} -Wnon-virtual-dtor")

  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -pipe -fPIC ${c_warnings} ${no_warnings} ${common_flags}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -fPIC ${cxx_warnings} ${no_warnings} ${common_flags}")
  append("-fPIE -pie" CMAKE_EXE_LINKER_FLAGS)

  # CACHE + FORCE should be used only here to make sure that this parameters applied globally
  # End user is responsible to adjust configuration parameters further if needed. Here
  # we se only minimal parameters which are really required for the proper configuration build.
  set(CMAKE_C_FLAGS_DEBUG     "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG"   CACHE STRING "" FORCE)
  set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}"          CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG" CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}"        CACHE STRING "" FORCE)

  if (CMAKE_C_COMPILER MATCHES icc)
    disable_werror()
  endif()

  if ( Darwin )
    if (CMAKE_C_COMPILER MATCHES clang)
      set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -v -std=c++11 -stdlib=libc++ ${common_flags}")
    endif()
  endif()

  if (DEFINED CMAKE_FIND_ROOT_PATH)
#    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " CMAKE_C_FLAGS)
#    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " CMAKE_CXX_FLAGS)
    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " LINK_FLAGS)
  endif (DEFINED CMAKE_FIND_ROOT_PATH)

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    append("-m64 -g" CMAKE_C_FLAGS)
    append("-m64 -g" CMAKE_CXX_FLAGS)
    append("-m64 -g" LINK_FLAGS)
  else ()
    append("-m32 -g" CMAKE_C_FLAGS)
    append("-m32 -g" CMAKE_CXX_FLAGS)
    append("-m32 -g" LINK_FLAGS)
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    link_directories(/usr/lib64)
  else()
    link_directories(/usr/lib)
  endif()
elseif( Windows )
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX "C:/Program Files/mediasdk/" CACHE PATH "Install Path Prefix" FORCE )
  endif()
endif( )

if( NOT DEFINED MFX_APPS_DIR)
  set( MFX_APPS_DIR ${CMAKE_INSTALL_FULL_DATADIR}/mfx )
endif()

set( MFX_SAMPLES_INSTALL_BIN_DIR ${MFX_APPS_DIR}/samples )
set( MFX_SAMPLES_INSTALL_LIB_DIR ${MFX_APPS_DIR}/samples )

set( MFX_TOOLS_INSTALL_BIN_DIR ${MFX_APPS_DIR}/tools )
set( MFX_TOOLS_INSTALL_LIB_DIR ${MFX_APPS_DIR}/tools )

if( NOT DEFINED MFX_PLUGINS_DIR )
  set( MFX_PLUGINS_DIR ${CMAKE_INSTALL_FULL_LIBDIR}/mfx )
endif( )

if( NOT DEFINED MFX_PLUGINS_CONF_DIR )
  set( MFX_PLUGINS_CONF_DIR ${CMAKE_INSTALL_FULL_DATADIR}/mfx )
endif( )

if( NOT DEFINED MFX_MODULES_DIR )
  set( MFX_MODULES_DIR ${CMAKE_INSTALL_FULL_LIBDIR} )
endif( )

message( STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" )

# Some font definitions: colors, bold text, etc.
if(NOT Windows)
  string(ASCII 27 Esc)
  set(EndColor   "${Esc}[m")
  set(BoldColor  "${Esc}[1m")
  set(Red        "${Esc}[31m")
  set(BoldRed    "${Esc}[1;31m")
  set(Green      "${Esc}[32m")
  set(BoldGreen  "${Esc}[1;32m")
endif()

# Usage: report_targets( "Description for the following targets:" [targets] )
# Note: targets list is optional
function(report_targets description )
  message("")
  message("${ARGV0}")
  foreach(target ${ARGV1})
    message("  ${target}")
  endforeach()
  message("")
endfunction()

# Permits to accumulate strings in some variable for the delayed output
function(report_add_target var target)
  set(${ARGV0} ${${ARGV0}} ${ARGV1} CACHE INTERNAL "" FORCE)
endfunction()

# Defined OS name and versions and build info and build commit
if(APPLE)
  set(MFX_SYSTEM "MAC")
elseif(UNIX)
  execute_process(
    COMMAND lsb_release -i
    OUTPUT_VARIABLE OUTPUT
    ERROR_VARIABLE ERROR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(NOT ERROR)
    string(REPLACE  "Distributor ID:	" ""  MFX_LINUX_NAME "${OUTPUT}")
    if(NOT "${OUTPUT}" STREQUAL "${MFX_LINUX_NAME}")
      set(MFX_SYSTEM "${MFX_LINUX_NAME}")
    endif()
  endif()
elseif(WIN32)
  set(MFX_SYSTEM "Windows")
endif()

execute_process(
  COMMAND getconf GNU_LIBC_VERSION
  OUTPUT_VARIABLE OUTPUT
  ERROR_VARIABLE ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT ERROR AND UNIX)
  set(MFX_GLIBC ${OUTPUT})
endif()

if( API_USE_LATEST )
  set(API_VER_MODIF "${API_VERSION}+")
else()
  set(API_VER_MODIF "${API_VERSION}")
endif()

if( MFX_SYSTEM )
  set( BUILD_INFO "${MFX_SYSTEM} ${CMAKE_SYSTEM_VERSION} | ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
else()
  set( BUILD_INFO "${CMAKE_SYSTEM} ${CMAKE_SYSTEM_VERSION} | ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}" )
endif()

if(UNIX AND MFX_GLIBC)
  set( BUILD_INFO "${BUILD_INFO} | ${MFX_GLIBC}")
endif()

git_describe( git_commit )

set( version_flags "${version_flags} -DMFX_BUILD_INFO=\"\\\"${BUILD_INFO}\"\\\"" )
set( version_flags "${version_flags} -DMFX_API_VERSION=\\\"${API_VER_MODIF}\\\"" )
set( version_flags "${version_flags} -DMFX_GIT_COMMIT=\\\"${git_commit}\\\"" )
set( version_flags "${version_flags} -DMEDIA_VERSION_STR=\\\"${MEDIA_VERSION_STR}\\\"" )

set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${version_flags}" )
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${version_flags}" )
