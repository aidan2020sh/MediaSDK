# Purpose:
#   Defines include paths, compilation flags, etc. to build Media SDK targets.
#
# Defined variables:
#   MFX_CFLAGS - common flags for all targets
#   MFX_CFLAGS_LIBVA - LibVA support flags (to build apps with or without LibVA support)
#   MFX_INCLUDES - common include paths for all targets
#   MFX_INCLUDES_LIBVA - include paths to LibVA headers
#   MFX_LDFLAGS - common link flags for all targets

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================
# Common definitions

MFX_CFLAGS := -DANDROID

# Android version preference:
ifneq ($(filter 9 9.% P ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_P
endif
ifneq ($(filter 8.% O ,$(PLATFORM_VERSION)),)
  ifneq ($(filter 8.0.%,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_O
  else
    MFX_ANDROID_VERSION:= MFX_O_MR1
  endif
endif

ifeq ($(MFX_ANDROID_PLATFORM),)
  MFX_ANDROID_PLATFORM:=$(TARGET_BOARD_PLATFORM)
endif

# Passing Android-dependency information to the code
MFX_CFLAGS += \
  -DMFX_ANDROID_VERSION=$(MFX_ANDROID_VERSION) \
  -DMFX_ANDROID_PLATFORM=$(MFX_ANDROID_PLATFORM) \
  -include mfx_android_config.h

# Setting version information for the binaries
ifeq ($(MFX_VERSION),)
  MFX_VERSION = "6.0.010"
endif

# We need to freeze Media SDK API to 1.26 on Android O
# because there is used old version of LibVA 2.0
ifneq ($(filter MFX_O MFX_O_MR1, $(MFX_ANDROID_VERSION)),)
  MFX_CFLAGS += -DMFX_VERSION=1026
else ifneq ($(filter MFX_P, $(MFX_ANDROID_VERSION)),)
  # CPLib PAVP implementation
  # It requires minimum API version 1.30
  MFX_CFLAGS += \
    -DMFX_ENABLE_CPLIB \
    -DMFX_VERSION=1030
else
  MFX_CFLAGS += \
    -DMFX_ENABLE_CPLIB \
    -DMFX_VERSION=1033
endif

MFX_CFLAGS += \
  -DMFX_FILE_VERSION=\"`echo $(MFX_VERSION) | cut -f 1 -d.``date +.%-y.%-m.%-d`\" \
  -DMFX_PRODUCT_VERSION=\"$(MFX_VERSION)\"

#  Security
MFX_CFLAGS += \
  -fstack-protector \
  -fPIE -fPIC -pie \
  -O2 -D_FORTIFY_SOURCE=2 \
  -Wformat -Wformat-security \
  -fexceptions -frtti

ifeq ($(ENABLE_MAX_NUM_REORDER_FRAMES_OUTPUT),true)
  MFX_CFLAGS += -DENABLE_MAX_NUM_REORDER_FRAMES_OUTPUT
endif

# LibVA support.
MFX_CFLAGS_LIBVA := -DLIBVA_SUPPORT -DLIBVA_ANDROID_SUPPORT

ifneq ($(filter $(MFX_ANDROID_VERSION), MFX_O),)
  MFX_CFLAGS_LIBVA += -DANDROID_O
endif

# Setting usual paths to include files
MFX_INCLUDES := $(LOCAL_PATH)/include

MFX_INCLUDES_LIBVA := $(TARGET_OUT_HEADERS)/libva

# Setting usual link flags
MFX_LDFLAGS := \
  -z noexecstack \
  -z relro -z now

# Setting vendor
LOCAL_MODULE_OWNER := intel

# Moving executables to proprietary location
LOCAL_PROPRIETARY_MODULE := true

# =============================================================================

# Definitions specific to Media SDK internal things (do not apply for samples)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs_internal.mk
