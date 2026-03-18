LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL3

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(LOCAL_PATH)/lua

LOCAL_CPP_FEATURES := exceptions rtti

# Add your application source files here...
FILE_LIST := $(wildcard $(LOCAL_PATH)/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)

FILE_LIST_LUA := $(wildcard $(LOCAL_PATH)/lua/*.c)
LOCAL_SRC_FILES += $(FILE_LIST_LUA:$(LOCAL_PATH)/%=%)

# NOTE: deps.zip must contain SDL3 .so files (SDL3, SDL3_mixer, SDL3_image, SDL3_ttf).
# The Java glue code in android/app/src/main/java/org/libsdl/app/ must be from SDL3's Android port.
# mpg123 is no longer required: SDL3_mixer ships its own audio decoders.
LOCAL_SHARED_LIBRARIES := libhidapi SDL3 SDL3_mixer SDL3_image SDL3_ttf libsqlite3

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -ldl -llog -lz

LOCAL_CFLAGS += -DTILES=1 -DDYNAMIC_ATLAS=1 -DSDL_SOUND=1 -DLUA=1 -DBACKTRACE=1 -Wextra -Wall -fsigned-char -ffast-math

LOCAL_LDFLAGS += $(LOCAL_CFLAGS)

include $(BUILD_SHARED_LIBRARY)
