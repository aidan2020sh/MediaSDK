# Purpose:
#   Defines include paths, compilation flags, etc. specific to Android OS.
#
# Defined variables:
#   MFX_C_INCLUDES_LIBVA - include paths to LibVA headers
#   MFX_C_INCLUDES_OMX - include paths to OMX headers (all needed for plug-ins)

MFX_C_INCLUDES_LIBVA := \
  $(TARGET_OUT_HEADERS)/libva

MFX_C_INCLUDES_OMX := \
  frameworks/native/include/media/openmax \
  frameworks/native/include/media/hardware

UFO_ENABLE_GEN ?= gen7

# libpavp.h


ifeq ($(MFX_OMX_PAVP),true)
  ifneq ($(filter $(MFX_ANDROID_VERSION), MFX_MCG_JB MFX_MCG_KK MFX_MCG_LD MFX_MM),)
    MFX_C_INCLUDES_OMX += \
      $(TOP)/vendor/intel/hardware/PRIVATE/ufo/inc/libpavp \
      $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)/x86 \
      $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)/include/libpavp \
      $(TOP)/vendor/intel/ufo/$(UFO_ENABLE_GEN)_dev/include/libpavp
  endif
endif