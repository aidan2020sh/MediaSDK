LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))) \
                   $(addprefix src/vm/, $(notdir $(wildcard $(LOCAL_PATH)/src/vm/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_LIBVA)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_LIBVA)

LOCAL_HEADER_LIBRARIES := \
    $(MFX_HEADER_LIBRARIES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libsample_common

include $(BUILD_STATIC_LIBRARY)
