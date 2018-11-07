LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES := \
    $(MFX_INCLUDES) \
    $(MFX_INCLUDES_LIBVA) \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_common/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/sample_plugins/rotate_cpu/include

LOCAL_CFLAGS := \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_LIBVA)

LOCAL_HEADER_LIBRARIES := libmfx_headers
LOCAL_STATIC_LIBRARIES := libsample_common libmfx
LOCAL_SHARED_LIBRARIES := libdl libva libva-android

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := sample_decode
LOCAL_MODULE_STEM_32 := sample_decode32
LOCAL_MODULE_STEM_64 := sample_decode64

include $(BUILD_EXECUTABLE)
