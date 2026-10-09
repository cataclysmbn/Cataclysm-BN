#if defined(__ANDROID__)
#    include "options.h"

#    include <SDL3/SDL_system.h>
#    include <jni.h>

auto options_manager::android_get_default_setting(const char* name, const bool fallback) -> bool {
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    const auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (!env || !activity) { return fallback; }
    const auto type = env->GetObjectClass(activity);
    const auto method = env->GetMethodID(type, "getDefaultSetting", "(Ljava/lang/String;Z)Z");
    const auto setting_name = env->NewStringUTF(name);
    const auto result = env->CallBooleanMethod(activity, method, setting_name, fallback);
    env->DeleteLocalRef(setting_name);
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
    return result;
}
#endif
