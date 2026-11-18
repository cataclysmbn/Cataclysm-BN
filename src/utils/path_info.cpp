#include "path_info.h"

#include "enums.h"
#include "filesystem.h"
#include "language.h"
#include "options.h"
#include "title_screen.h"

#include <clocale>
#include <cstdlib>

#if defined(_WIN32)
#    include <windows.h>
#endif
#if defined(__ANDROID__)
#    include <SDL3/SDL.h>
#    include <jni.h>
#endif
/**
 * Return a locale specific path, or if there is no path for the current
 * locale, return the default path.
 * @param path The local path is based on that value.
 * @param extension File name extension, is automatically added to the path
 * of the translated file. Can be empty, but must otherwise include the
 * initial '.', e.g. ".json"
 * @param fallback The path of the fallback filename.
 * It is used if no translated file can be found.
 */
static auto find_translated_file(
    const std::string& path, const std::string& extension, const std::string& fallback)
    -> std::string;

static std::string motd_value;
static std::string gfxdir_value;
static std::string config_dir_value;
static std::string user_dir_value;
static std::string datadir_value;
static std::string base_path_value;
static std::string savedir_value;
static std::string autopickup_value;
static std::string options_value;
static std::string memorialdir_value;

void PATH_INFO::init_base_path(std::string path) { base_path_value = as_norm_dir(path); }

#if defined(__ANDROID__)
// Okay so this fine function right here
// Gets Documents/cataclysm-bn
// And points the user directory to it for android
void PATH_INFO::init_user_dir(std::string dir) {
    if (get_options().android_get_default_setting("Use Legacy Storage", false)) {
        user_dir_value = as_norm_dir(dir);
        return;
    }
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());

    jclass clazz = env->GetObjectClass(activity);

    // Method signature:
    // ()Ljava/lang/String;
    jmethodID method_id = env->GetMethodID(clazz, "getDocumentsDirectory", "()Ljava/lang/String;");

    jstring jpath = (jstring)env->CallObjectMethod(activity, method_id);

    // Convert jstring → std::string
    const char* chars = env->GetStringUTFChars(jpath, nullptr);
    std::string path(chars);
    dir = path + "/cataclysm-bn/";
    user_dir_value = as_norm_dir(dir);

    env->ReleaseStringUTFChars(jpath, chars);

    // Cleanup local refs
    env->DeleteLocalRef(jpath);
    env->DeleteLocalRef(clazz);
    env->DeleteLocalRef(activity);
}
#endif
#if !defined(__ANDROID__)
void PATH_INFO::init_user_dir(std::string dir) {
    if (dir.empty()) {
        const char* user_dir;
#    if defined(_WIN32)
        user_dir = getenv("LOCALAPPDATA");
        // On Windows userdir without dot
        dir = std::string(user_dir) + "/cataclysm-bn/";
#    elif defined(MACOSX)
        user_dir = getenv("HOME");
        dir = std::string(user_dir) + "/Library/Application Support/Cataclysm-BN/";
#    elif defined(USE_XDG_DIR)
        user_dir = getenv("XDG_DATA_HOME");
        if (user_dir) {
            dir = std::string(user_dir) + "/cataclysm-bn/";
        } else {
            user_dir = getenv("HOME");
            dir = std::string(user_dir) + "/.local/share/cataclysm-bn/";
        }
#    else
        user_dir = getenv("HOME");
        dir = std::string(user_dir) + "/.cataclysm-bn/";
#    endif
    }

    user_dir_value = as_norm_dir(dir);
}
#endif
void PATH_INFO::set_standard_filenames() {
    // Special: data_dir and gfx_dir
    if (!base_path_value.empty()) {
#if defined(DATA_DIR_PREFIX)
        datadir_value = base_path_value + "share/cataclysm-bn/";
        gfxdir_value = datadir_value + "gfx/";
#else
        datadir_value = base_path_value + "data/";
        gfxdir_value = base_path_value + "gfx/";
#endif
    } else {
        datadir_value = "data/";
        gfxdir_value = "gfx/";
    }

    // Shared dirs

    // Shared files
    motd_value = datadir_value + "motd/" + "en.motd";

    savedir_value = user_dir_value + "save/";
    memorialdir_value = user_dir_value + "memorial/";

#if defined(USE_XDG_DIR)
    const char* user_dir;
    std::string dir;
    if ((user_dir = getenv("XDG_CONFIG_HOME"))) {
        dir = std::string(user_dir) + "/cataclysm-bn/";
    } else {
        user_dir = getenv("HOME");
        dir = std::string(user_dir) + "/.config/cataclysm-bn/";
    }
    config_dir_value = dir;
#else
    config_dir_value = user_dir_value + "config/";
#endif
    options_value = config_dir_value + "options.json";
    autopickup_value = config_dir_value + "auto_pickup.json";
}

auto find_translated_file(
    const std::string& base_path, const std::string& extension, const std::string& fallback)
    -> std::string {
    std::vector<std::string> opts = get_lang_path_substring(get_language().id);

    for (const std::string& s : opts) {
        const std::string local_path = base_path + s + extension;
        if (file_exist(local_path)) { return local_path; }
    }

    return fallback;
}

auto PATH_INFO::autopickup() -> std::string { return autopickup_value; }
auto PATH_INFO::base_colors() -> std::string { return config_dir_value + "base_colors.json"; }
auto PATH_INFO::base_path() -> std::string { return base_path_value; }
auto PATH_INFO::colors() -> std::string { return datadir_value + "raw/" + "colors.json"; }
auto PATH_INFO::color_templates() -> std::string {
    return datadir_value + "raw/" + "color_templates/";
}
auto PATH_INFO::config_dir() -> std::string { return config_dir_value; }
auto PATH_INFO::custom_colors() -> std::string { return config_dir_value + "custom_colors.json"; }
auto PATH_INFO::datadir() -> std::string { return datadir_value; }
auto PATH_INFO::debug() -> std::string { return config_dir_value + "debug.log"; }
auto PATH_INFO::defaultsounddir() -> std::string { return datadir_value + "sound"; }
auto PATH_INFO::defaulttilejson() -> std::string { return "tile_config.json"; }
auto PATH_INFO::defaulttilepng() -> std::string { return "tinytile.png"; }
auto PATH_INFO::fontconfig() -> std::string { return datadir_value + "raw/" + "fonts.json"; }
auto PATH_INFO::user_fontconfig() -> std::string { return config_dir_value + "fonts.json"; }
auto PATH_INFO::fontdir() -> std::string { return datadir_value + "font/"; }
auto PATH_INFO::user_fontdir() -> std::string { return user_dir_value + "font/"; }
auto PATH_INFO::language_defs_file() -> std::string {
    return datadir_value + "raw/" + "languages.json";
}
auto PATH_INFO::graveyarddir() -> std::string { return user_dir_value + "graveyard/"; }
auto PATH_INFO::help() -> std::string { return datadir_value + "help/" + "texts.json"; }
auto PATH_INFO::keybindingsdir() -> std::string { return datadir_value + "raw/keybindings/"; }
auto PATH_INFO::main_menu_tips() -> std::string { return datadir_value + "raw/" + "tips.json"; }
auto PATH_INFO::lastworld() -> std::string { return config_dir_value + "lastworld.json"; }
auto PATH_INFO::memorialdir() -> std::string { return memorialdir_value; }
auto PATH_INFO::moddir() -> std::string { return datadir_value + "mods/"; }
auto PATH_INFO::options() -> std::string { return options_value; }
auto PATH_INFO::panel_options() -> std::string { return config_dir_value + "panel_options.json"; }
auto PATH_INFO::preload() -> std::string { return config_dir_value + "preload.json"; }
auto PATH_INFO::safemode() -> std::string { return config_dir_value + "safemode.json"; }
auto PATH_INFO::distraction() -> std::string { return config_dir_value + "distraction.json"; }
auto PATH_INFO::savedir() -> std::string {
#if defined(__ANDROID__)
    const auto load_from_external =
        get_options().has_option("LOAD_FROM_EXTERNAL") && get_option<bool>("LOAD_FROM_EXTERNAL");
    if (load_from_external) {
        return base_path_value + "/save/";
    } else {
        return savedir_value;
    }
#else
    return savedir_value;
#endif
}
auto PATH_INFO::shaders() -> std::string { return datadir_value + "shaders/"; }
auto PATH_INFO::sokoban() -> std::string { return datadir_value + "raw/" + "sokoban.txt"; }
auto PATH_INFO::templatedir() -> std::string { return user_dir_value + "templates/"; }
auto PATH_INFO::user_dir() -> std::string { return user_dir_value; }
auto PATH_INFO::user_gfx() -> std::string { return user_dir_value + "gfx/"; }
auto PATH_INFO::user_keybindings() -> std::string { return config_dir_value + "keybindings.json"; }
auto PATH_INFO::user_moddir() -> std::string { return user_dir_value + "mods/"; }
auto PATH_INFO::user_sound() -> std::string { return user_dir_value + "sound/"; }
auto PATH_INFO::worldoptions() -> std::string { return "worldoptions.json"; }
auto PATH_INFO::crash() -> std::string { return config_dir_value + "crash.log"; }
auto PATH_INFO::tileset_conf() -> std::string { return "tileset.txt"; }
auto PATH_INFO::mods_replacements() -> std::string {
    return datadir_value + "mods/" + "replacements.json";
}
auto PATH_INFO::mods_dev_default() -> std::string {
    return datadir_value + "mods/" + "default.json";
}
auto PATH_INFO::mods_user_default() -> std::string {
    return config_dir_value + "default_mods.json";
}
auto PATH_INFO::soundpack_conf() -> std::string { return "soundpack.txt"; }
auto PATH_INFO::gfxdir() -> std::string { return gfxdir_value; }
auto PATH_INFO::data_sound() -> std::string { return datadir_value + "sound"; }

auto PATH_INFO::credits() -> std::string {
    return find_translated_file(
        datadir_value + "credits/", ".credits", datadir_value + "credits/" + "en.credits");
}

auto PATH_INFO::motd() -> std::string {
    return find_translated_file(datadir_value + "motd/", ".motd", motd_value);
}

auto PATH_INFO::title(const holiday) -> std::string { return title_screen::resolve_path(); }

auto PATH_INFO::names() -> std::string {
    return find_translated_file(
        datadir_value + "names/", ".json", datadir_value + "names/" + "en.json");
}

void PATH_INFO::set_datadir(const std::string& datadir) {
    datadir_value = datadir;
    // Shared dirs
    gfxdir_value = datadir_value + "gfx/";

    // Shared files
    motd_value = datadir_value + "motd/" + "en.motd";
}

void PATH_INFO::set_config_dir(const std::string& config_dir) {
    config_dir_value = config_dir;
    options_value = config_dir_value + "options.json";
    autopickup_value = config_dir_value + "auto_pickup.json";
}

void PATH_INFO::set_savedir(const std::string& savedir) { savedir_value = savedir; }

void PATH_INFO::set_memorialdir(const std::string& memorialdir) { memorialdir_value = memorialdir; }

void PATH_INFO::set_options(const std::string& options) { options_value = options; }

void PATH_INFO::set_autopickup(const std::string& autopickup) { autopickup_value = autopickup; }

void PATH_INFO::set_motd(const std::string& motd) { motd_value = motd; }
