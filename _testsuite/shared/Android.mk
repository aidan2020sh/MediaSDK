LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

MFX_SHARED_FILES = $(addprefix src/, \
    cc_utils.cpp \
    memory_allocator.cpp \
    mfx_io_utils.cpp \
    shared_utils.cpp \
    test_h264_dec_command_line.cpp \
    test_statistics.cpp)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_SHARED_FILES)

LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL)
LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libshared_utils

include $(BUILD_STATIC_LIBRARY)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(MFX_SHARED_FILES)

LOCAL_C_INCLUDES += $(MFX_C_INCLUDES_INTERNAL)
LOCAL_CFLAGS += $(MFX_CFLAGS_INTERNAL) $(MFX_CFLAGS_LUCAS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libshared_utils_lucas

include $(BUILD_STATIC_LIBRARY)

