#include "path_info.h"

#include <clocale>
#include <cstdlib>

#include "enums.h"
#include "language.h"
#include "options.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__ANDROID__)
#include <jni.h>
#include <SDL3/SDL.h>
#endif

namespace
{

/// Return a locale specific path, or the default path when no translated file exists.
auto find_translated_file( const fs::path &path, const std::string &extension,
                           const fs::path &fallback ) -> fs::path;

auto motd_value = fs::path {};
auto gfxdir_value = fs::path {};
auto config_dir_value = fs::path {};
auto user_dir_value = fs::path {};
auto datadir_value = fs::path {};
auto base_path_value = fs::path {};
auto savedir_value = fs::path {};
auto autopickup_value = fs::path {};
auto options_value = fs::path {};
auto memorialdir_value = fs::path {};

auto env_path( const char *name ) -> fs::path
{
    const auto *value = getenv( name );
    return value == nullptr ? fs::path{} :
           fs::path( value );
}

} // namespace

auto PATH_INFO::init_base_path( fs::path path ) -> void
{
    base_path_value = as_norm_dir( path );
}

#if defined(__ANDROID__)
// Okay so this fine function right here
// Gets Documents/cataclysm-bn
// And points the user directory to it for android
auto PATH_INFO::init_user_dir( fs::path dir ) -> void
{
    if( get_options().android_get_default_setting( "Use Legacy Storage", false ) ) {
        user_dir_value = as_norm_dir( dir );
        return;
    }
    auto *env = static_cast<JNIEnv *>( SDL_GetAndroidJNIEnv() );
    auto activity = static_cast<jobject>( SDL_GetAndroidActivity() );

    auto clazz = env->GetObjectClass( activity );

    // Method signature:
    // ()Ljava/lang/String;
    auto method_id = env->GetMethodID(
                         clazz,
                         "getDocumentsDirectory",
                         "()Ljava/lang/String;"
                     );

    auto jpath = static_cast<jstring>( env->CallObjectMethod( activity, method_id ) );

    // Convert jstring → std::string
    const auto *chars = env->GetStringUTFChars( jpath, nullptr );
    dir = fs::path( chars ) / "cataclysm-bn";
    user_dir_value = as_norm_dir( dir );

    env->ReleaseStringUTFChars( jpath, chars );

    // Cleanup local refs
    env->DeleteLocalRef( jpath );
    env->DeleteLocalRef( clazz );
    env->DeleteLocalRef( activity );
}
#endif
#if !defined(__ANDROID__)
auto PATH_INFO::init_user_dir( fs::path dir ) -> void
{
    if( dir.empty() ) {
#if defined(_WIN32)
        // On Windows userdir without dot
        dir = env_path( "LOCALAPPDATA" ) / "cataclysm-bn";
#elif defined(MACOSX)
        dir = env_path( "HOME" ) / "Library/Application Support/Cataclysm-BN";
#elif defined(USE_XDG_DIR)
        const auto xdg_data_home = env_path( "XDG_DATA_HOME" );
        dir = !xdg_data_home.empty() ? xdg_data_home / "cataclysm-bn" :
              env_path( "HOME" ) / ".local/share/cataclysm-bn";
#else
        dir = env_path( "HOME" ) / ".cataclysm-bn";
#endif
    }

    user_dir_value = as_norm_dir( dir );
}
#endif

auto PATH_INFO::set_standard_filenames() -> void
{
    // Special: data_dir and gfx_dir
    if( !base_path_value.empty() ) {
#if defined(DATA_DIR_PREFIX)
        datadir_value = base_path_value / "share/cataclysm-bn";
        gfxdir_value = datadir_value / "gfx";
#else
        datadir_value = base_path_value / "data";
        gfxdir_value = base_path_value / "gfx";
#endif
    } else {
        datadir_value = "data";
        gfxdir_value = "gfx";
    }

    // Shared dirs

    // Shared files
    motd_value = datadir_value / "motd" / "en.motd";

    savedir_value = user_dir_value / "save";
    memorialdir_value = user_dir_value / "memorial";

#if defined(USE_XDG_DIR)
    const auto xdg_config_home = env_path( "XDG_CONFIG_HOME" );
    config_dir_value = !xdg_config_home.empty() ? xdg_config_home / "cataclysm-bn" :
                       env_path( "HOME" ) / ".config/cataclysm-bn";
#else
    config_dir_value = user_dir_value / "config";
#endif
    options_value = config_dir_value / "options.json";
    autopickup_value = config_dir_value / "auto_pickup.json";
}

namespace
{

auto find_translated_file( const fs::path &base_path, const std::string &extension,
                           const fs::path &fallback ) -> fs::path
{
    const auto opts = get_lang_path_substring( get_language().id );

    for( const auto &s : opts ) {
        const auto local_path = base_path / ( s + extension );
        if( file_exist( local_path ) ) {
            return local_path;
        }
    }

    return fallback;
}

} // namespace

auto PATH_INFO::autopickup() -> fs::path { return autopickup_value; }
auto PATH_INFO::base_colors() -> fs::path { return config_dir_value / "base_colors.json"; }
auto PATH_INFO::base_path() -> fs::path { return base_path_value; }
auto PATH_INFO::colors() -> fs::path { return datadir_value / "raw" / "colors.json"; }
auto PATH_INFO::color_templates() -> fs::path { return datadir_value / "raw" / "color_templates"; }
auto PATH_INFO::config_dir() -> fs::path { return config_dir_value; }
auto PATH_INFO::custom_colors() -> fs::path { return config_dir_value / "custom_colors.json"; }
auto PATH_INFO::datadir() -> fs::path { return datadir_value; }
auto PATH_INFO::debug() -> fs::path { return config_dir_value / "debug.log"; }
auto PATH_INFO::defaultsounddir() -> fs::path { return datadir_value / "sound"; }
auto PATH_INFO::defaulttilejson() -> fs::path { return "tile_config.json"; }
auto PATH_INFO::defaulttilepng() -> fs::path { return "tinytile.png"; }
auto PATH_INFO::fontconfig() -> fs::path { return datadir_value / "raw" / "fonts.json"; }
auto PATH_INFO::user_fontconfig() -> fs::path { return config_dir_value / "fonts.json"; }
auto PATH_INFO::fontdir() -> fs::path { return datadir_value / "font"; }
auto PATH_INFO::user_fontdir() -> fs::path { return user_dir_value / "font"; }
auto PATH_INFO::language_defs_file() -> fs::path { return datadir_value / "raw" / "languages.json"; }
auto PATH_INFO::graveyarddir() -> fs::path { return user_dir_value / "graveyard"; }
auto PATH_INFO::help() -> fs::path { return datadir_value / "help" / "texts.json"; }
auto PATH_INFO::keybindingsdir() -> fs::path { return datadir_value / "raw" / "keybindings"; }
auto PATH_INFO::main_menu_tips() -> fs::path { return datadir_value / "raw" / "tips.json"; }
auto PATH_INFO::lastworld() -> fs::path { return config_dir_value / "lastworld.json"; }
auto PATH_INFO::memorialdir() -> fs::path { return memorialdir_value; }
auto PATH_INFO::moddir() -> fs::path { return datadir_value / "mods"; }
auto PATH_INFO::options() -> fs::path { return options_value; }
auto PATH_INFO::panel_options() -> fs::path { return config_dir_value / "panel_options.json"; }
auto PATH_INFO::safemode() -> fs::path { return config_dir_value / "safemode.json"; }
auto PATH_INFO::distraction() -> fs::path { return config_dir_value / "distraction.json"; }

auto PATH_INFO::savedir() -> fs::path
{
#if defined(__ANDROID__)
    return get_option<bool>( "LOAD_FROM_EXTERNAL" ) ? base_path_value / "save" : savedir_value;
#else
    return savedir_value;
#endif
}

auto PATH_INFO::sokoban() -> fs::path { return datadir_value / "raw" / "sokoban.txt"; }
auto PATH_INFO::templatedir() -> fs::path { return user_dir_value / "templates"; }
auto PATH_INFO::user_dir() -> fs::path { return user_dir_value; }
auto PATH_INFO::user_gfx() -> fs::path { return user_dir_value / "gfx"; }
auto PATH_INFO::user_keybindings() -> fs::path { return config_dir_value / "keybindings.json"; }
auto PATH_INFO::user_moddir() -> fs::path { return user_dir_value / "mods"; }
auto PATH_INFO::user_sound() -> fs::path { return user_dir_value / "sound"; }
auto PATH_INFO::worldoptions() -> fs::path { return "worldoptions.json"; }
auto PATH_INFO::crash() -> fs::path { return config_dir_value / "crash.log"; }
auto PATH_INFO::tileset_conf() -> fs::path { return "tileset.txt"; }
auto PATH_INFO::mods_replacements() -> fs::path { return datadir_value / "mods" / "replacements.json"; }
auto PATH_INFO::mods_dev_default() -> fs::path { return datadir_value / "mods" / "default.json"; }
auto PATH_INFO::mods_user_default() -> fs::path { return config_dir_value / "default_mods.json"; }
auto PATH_INFO::soundpack_conf() -> fs::path { return "soundpack.txt"; }
auto PATH_INFO::gfxdir() -> fs::path { return gfxdir_value; }
auto PATH_INFO::data_sound() -> fs::path { return datadir_value / "sound"; }

auto PATH_INFO::credits() -> fs::path
{
    return find_translated_file( datadir_value / "credits", ".credits",
                                 datadir_value / "credits" / "en.credits" );
}

auto PATH_INFO::motd() -> fs::path
{
    return find_translated_file( datadir_value / "motd", ".motd", motd_value );
}

auto PATH_INFO::title( const holiday ) -> fs::path
{
    const auto theme_basepath = datadir_value / "title";
    const auto theme_extension = std::string( ".title" );
    const auto theme_fallback = theme_basepath / "en.title";
    return find_translated_file( theme_basepath, theme_extension, theme_fallback );
}

auto PATH_INFO::names() -> fs::path
{
    return find_translated_file( datadir_value / "names", ".json",
                                 datadir_value / "names" / "en.json" );
}

auto PATH_INFO::set_datadir( const fs::path &datadir ) -> void
{
    datadir_value = datadir;
    // Shared dirs
    gfxdir_value = datadir_value / "gfx";

    // Shared files
    motd_value = datadir_value / "motd" / "en.motd";
}

auto PATH_INFO::set_config_dir( const fs::path &config_dir ) -> void
{
    config_dir_value = config_dir;
    options_value = config_dir_value / "options.json";
    autopickup_value = config_dir_value / "auto_pickup.json";
}

auto PATH_INFO::set_savedir( const fs::path &savedir ) -> void
{
    savedir_value = savedir;
}

auto PATH_INFO::set_memorialdir( const fs::path &memorialdir ) -> void
{
    memorialdir_value = memorialdir;
}

auto PATH_INFO::set_options( const fs::path &options ) -> void
{
    options_value = options;
}

auto PATH_INFO::set_autopickup( const fs::path &autopickup ) -> void
{
    autopickup_value = autopickup;
}

auto PATH_INFO::set_motd( const fs::path &motd ) -> void
{
    motd_value = motd;
}
