LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# =============================================================================

MFX_PIPELINE_CFLAGS := \
    $(MFX_CFLAGS_INTERNAL) \
    $(MFX_CFLAGS_LIBVA) \
    -Wno-date-time

MFX_PIPELINE_INCLUDES += \
    $(MFX_INCLUDES_INTERNAL) \
    $(MFX_INCLUDES_LIBVA) \
    $(MFX_HOME)/mdp_msdk-lib/api/opensource/mfx_dispatch/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/spl_common/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/avi_spl/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/codec/demuxer/include \
    $(MFX_HOME)/mdp_msdk-lib/_studio/shared/umc/test_suite/spy_test_component/include \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/shared/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/deprecated/sample_spl_mux/api \
    $(TOP)/frameworks/native/libs/nativewindow/include

MFX_PLAYER_INCLUDES += \
    $(MFX_INCLUDES_INTERNAL) \
    $(MFX_INCLUDES_LIBVA) \
    $(MFX_HOME)/mdp_msdk-lib/_testsuite/shared/include \
    $(MFX_HOME)/mdp_msdk-lib/samples/deprecated/sample_spl_mux/api \
    $(TOP)/frameworks/native/libs/nativewindow/include

ifeq ($(BOARD_USES_GRALLOC1),true)
    MFX_PIPELINE_INCLUDES += $(INTEL_MINIGBM)/cros_gralloc
endif

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES := $(MFX_PIPELINE_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := $(MFX_PIPELINE_CFLAGS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_HEADER_LIBRARIES := \
    libmfx_headers \
    libgui_headers \
    libui_headers \
    libhardware_headers \
    libnativebase_headers \
    gl_headers

LOCAL_STATIC_LIBRARIES := \
    android.hidl.token@1.0-utils \
    android.hardware.graphics.bufferqueue@1.0 \
    libmath \
    libarect

ifneq ($(filter MFX_O MFX_O_MR1, $(MFX_ANDROID_VERSION)),)
  LOCAL_STATIC_LIBRARIES += android.hardware.graphics.common@1.0
else
  LOCAL_STATIC_LIBRARIES += android.hardware.graphics.common@1.1
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(filter-out src/mfx_player.cpp, \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp))))

LOCAL_C_INCLUDES := $(MFX_PIPELINE_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := \
    $(MFX_PIPELINE_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_HEADER_LIBRARIES := \
    libmfx_headers \
    libgui_headers \
    libui_headers \
    libhardware_headers \
    libnativebase_headers \
    gl_headers

LOCAL_STATIC_LIBRARIES := \
    android.hidl.token@1.0-utils \
    android.hardware.graphics.bufferqueue@1.0 \
    libmath \
    libarect

ifneq ($(filter MFX_O MFX_O_MR1, $(MFX_ANDROID_VERSION)),)
  LOCAL_STATIC_LIBRARIES += android.hardware.graphics.common@1.0
else
  LOCAL_STATIC_LIBRARIES += android.hardware.graphics.common@1.1
endif

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_pipeline_lucas

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, mfx_player.cpp)

LOCAL_C_INCLUDES := $(MFX_PLAYER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := $(MFX_PIPELINE_CFLAGS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_LDFLAGS := $(MFX_LDFLAGS)

LOCAL_HEADER_LIBRARIES := libmfx_headers
LOCAL_STATIC_LIBRARIES := \
    libmfx \
    libmfx_pipeline \
    libshared_utils \
    libdispatch_trace \
    libsample_spl_mux_dispatcher \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged \
    libmfx_trace_hw \
    libsafec \
    libippvc_l \
    libippcc_l \
    libippcp_l \
    libippdc_l \
    libippi_l \
    libipps_l \
    libippcore_l

LOCAL_SHARED_LIBRARIES := libdl libva libva-android libgui libui libutils

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_player
LOCAL_MODULE_STEM_32 := mfx_player32
LOCAL_MODULE_STEM_64 := mfx_player64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, mfx_player.cpp)

LOCAL_C_INCLUDES := $(MFX_PLAYER_INCLUDES)
LOCAL_C_INCLUDES_32 := $(MFX_INCLUDES_INTERNAL_32)
LOCAL_C_INCLUDES_64 := $(MFX_INCLUDES_INTERNAL_64)

LOCAL_CFLAGS := \
    $(MFX_PIPELINE_CFLAGS) \
    $(MFX_CFLAGS_LUCAS)
LOCAL_CFLAGS_32 := $(MFX_CFLAGS_INTERNAL_32)
LOCAL_CFLAGS_64 := $(MFX_CFLAGS_INTERNAL_64)

LOCAL_LDFLAGS := $(MFX_LDFLAGS)

LOCAL_HEADER_LIBRARIES := libmfx_headers
LOCAL_STATIC_LIBRARIES := \
    libmfx \
    libmfx_pipeline_lucas \
    libshared_utils_lucas \
    libdispatch_trace \
    libsample_spl_mux_dispatcher \
    libumc_codecs_merged \
    libumc_io_merged_sw \
    libumc_core_merged \
    libmfx_trace_hw \
    libsafec \
    libippvc_l \
    libippcc_l \
    libippcp_l \
    libippdc_l \
    libippi_l \
    libipps_l \
    libippcore_l

LOCAL_SHARED_LIBRARIES := libdl libva libva-android libgui libui libutils

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_player_pipeline

include $(BUILD_SHARED_LIBRARY)