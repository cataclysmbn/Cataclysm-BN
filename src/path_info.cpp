#include "path_info.h"

#include <clocale>
#include <cstdlib>

#include "enums.h"
#include "filesystem.h"
#include "language.h"
#include "options.h"

#if defined(_WIN32)
#include <windows.h>
#endif
#if defined(__ANDROID__)
#include <jni.h>
#include <SDL_system.h>
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
static std::string find_translated_file( const std::string &path, const std::string &extension,
        const std::string &fallback );

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

void PATH_INFO::init_base_path( std::string path )
{
    base_path_value = as_norm_dir( path );
}

#if defined(__ANDROID__)
// Okay so this fine function right here
// Gets Documents/cataclysm-bn
// And points the user directory to it for android
void PATH_INFO::init_user_dir( std::string dir )
{
    JNIEnv *env = static_cast<JNIEnv *>( SDL_AndroidGetJNIEnv() );
    jobject activity = static_cast<jobject>( SDL_AndroidGetActivity() );

    jclass clazz = env->GetObjectClass( activity );

    // Method signature:
    // ()Ljava/lang/String;
    jmethodID method_id = env->GetMethodID(
                              clazz,
                              "getDocumentsDirectory",
                              "()Ljava/lang/String;"
                          );

    jstring jpath = ( jstring )env->CallObjectMethod( activity, method_id );

    // Convert jstring → std::string
    const char *chars = env->GetStringUTFChars( jpath, nullptr );
    std::string path( chars );
    dir = path + "/cataclysm-bn/";
    user_dir_value = as_norm_dir( dir );

    env->ReleaseStringUTFChars( jpath, chars );

    // Cleanup local refs
    env->DeleteLocalRef( jpath );
    env->DeleteLocalRef( clazz );
    env->DeleteLocalRef( activity );
}
#endif
#if !defined(__ANDROID__)
void PATH_INFO::init_user_dir( std::string dir )
{
    if( dir.empty() ) {
        const char *user_dir;
#if defined(_WIN32)
        user_dir = getenv( "LOCALAPPDATA" );
        // On Windows userdir without dot
        dir = std::string( user_dir ) + "/cataclysm-bn/";
#elif defined(MACOSX)
        user_dir = getenv( "HOME" );
        dir = std::string( user_dir ) + "/Library/Application Support/Cataclysm-BN/";
#elif defined(USE_XDG_DIR)
        user_dir = getenv( "XDG_DATA_HOME" );
        if( user_dir ) {
            dir = std::string( user_dir ) + "/cataclysm-bn/";
        } else {
            user_dir = getenv( "HOME" );
            dir = std::string( user_dir ) + "/.local/share/cataclysm-bn/";
        }
#else
        user_dir = getenv( "HOME" );
        dir = std::string( user_dir ) + "/.cataclysm-bn/";
#endif
    }

    user_dir_value = as_norm_dir( dir );
}
#endif
void PATH_INFO::set_standard_filenames()
{
    // Special: data_dir and gfx_dir
    if( !base_path_value.empty() ) {
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
    const char *user_dir;
    std::string dir;
    if( ( user_dir = getenv( "XDG_CONFIG_HOME" ) ) ) {
        dir = std::string( user_dir ) + "/cataclysm-bn/";
    } else {
        user_dir = getenv( "HOME" );
        dir = std::string( user_dir ) + "/.config/cataclysm-bn/";
    }
    config_dir_value = dir;
#else
    config_dir_value = user_dir_value + "config/";
#endif
    options_value = config_dir_value + "options.json";
    autopickup_value = config_dir_value + "auto_pickup.json";
}

std::string find_translated_file( const std::string &base_path, const std::string &extension,
                                  const std::string &fallback )
{
    std::vector<std::string> opts = get_lang_path_substring( get_language().id );

    for( const std::string &s : opts ) {
        const std::string local_path = base_path + s + extension;
        if( file_exist( local_path ) ) {
            return local_path;
        }
    }

    return fallback;
}

std::string PATH_INFO::autopickup()
{
    return autopickup_value;
}
std::string PATH_INFO::base_colors()
{
    return config_dir_value + "base_colors.json";
}
std::string PATH_INFO::base_path()
{
    return base_path_value;
}
std::string PATH_INFO::colors()
{
    return datadir_value + "raw/" + "colors.json";
}
std::string PATH_INFO::color_templates()
{
    return datadir_value + "raw/" + "color_templates/";
}
std::string PATH_INFO::config_dir()
{
    return config_dir_value;
}
std::string PATH_INFO::custom_colors()
{
    return config_dir_value + "custom_colors.json";
}
std::string PATH_INFO::datadir()
{
    return datadir_value;
}
std::string PATH_INFO::debug()
{
    return config_dir_value + "debug.log";
}
std::string PATH_INFO::defaultsounddir()
{
    return datadir_value + "sound";
}
std::string PATH_INFO::defaulttilejson()
{
    return "tile_config.json";
}
std::string PATH_INFO::defaulttilepng()
{
    return "tinytile.png";
}
std::string PATH_INFO::fontconfig()
{
    return datadir_value + "raw/" + "fonts.json";
}
std::string PATH_INFO::user_fontconfig()
{
    return config_dir_value + "fonts.json";
}
std::string PATH_INFO::fontdir()
{
    return datadir_value + "font/";
}
std::string PATH_INFO::user_fontdir()
{
    return user_dir_value + "font/";
}
std::string PATH_INFO::language_defs_file()
{
    return datadir_value + "raw/" + "languages.json";
}
std::string PATH_INFO::graveyarddir()
{
    return user_dir_value + "graveyard/";
}
std::string PATH_INFO::help()
{
    return datadir_value + "help/" + "texts.json";
}
std::string PATH_INFO::keybindingsdir()
{
    return datadir_value + "raw/keybindings/";
}
std::string PATH_INFO::main_menu_tips()
{
    return datadir_value + "raw/" + "tips.json";
}
std::string PATH_INFO::lastworld()
{
    return config_dir_value + "lastworld.json";
}
std::string PATH_INFO::memorialdir()
{
    return memorialdir_value;
}
std::string PATH_INFO::moddir()
{
    return datadir_value + "mods/";
}
std::string PATH_INFO::options()
{
    return options_value;
}
std::string PATH_INFO::panel_options()
{
    return config_dir_value + "panel_options.json";
}
std::string PATH_INFO::safemode()
{
    return config_dir_value + "safemode.json";
}
std::string PATH_INFO::distraction()
{
    return config_dir_value + "distraction.json";
}
std::string PATH_INFO::savedir()
{
    return savedir_value;
}
std::string PATH_INFO::sokoban()
{
    return datadir_value + "raw/" + "sokoban.txt";
}
std::string PATH_INFO::templatedir()
{
    return user_dir_value + "templates/";
}
std::string PATH_INFO::user_dir()
{
    return user_dir_value;
}
std::string PATH_INFO::user_gfx()
{
    return user_dir_value + "gfx/";
}
std::string PATH_INFO::user_keybindings()
{
    return config_dir_value + "keybindings.json";
}
std::string PATH_INFO::user_moddir()
{
    return user_dir_value + "mods/";
}
std::string PATH_INFO::user_sound()
{
    return user_dir_value + "sound/";
}
std::string PATH_INFO::worldoptions()
{
    return "worldoptions.json";
}
std::string PATH_INFO::crash()
{
    return config_dir_value + "crash.log";
}
std::string PATH_INFO::tileset_conf()
{
    return "tileset.txt";
}
std::string PATH_INFO::mods_replacements()
{
    return datadir_value + "mods/" + "replacements.json";
}
std::string PATH_INFO::mods_dev_default()
{
    return datadir_value + "mods/" + "default.json";
}
std::string PATH_INFO::mods_user_default()
{
    return config_dir_value + "default_mods.json";
}
std::string PATH_INFO::soundpack_conf()
{
    return "soundpack.txt";
}
std::string PATH_INFO::gfxdir()
{
    return gfxdir_value;
}
std::string PATH_INFO::data_sound()
{
    return datadir_value + "sound";
}

std::string PATH_INFO::credits()
{
    return find_translated_file( datadir_value + "credits/", ".credits",
                                 datadir_value + "credits/" + "en.credits" );
}

std::string PATH_INFO::motd()
{
    return find_translated_file( datadir_value + "motd/", ".motd", motd_value );
}

std::string PATH_INFO::title( const holiday )
{
    std::string theme_basepath = datadir_value + "title/";
    std::string theme_extension = ".title";
    std::string theme_fallback = theme_basepath + "en.title";
    return find_translated_file( theme_basepath, theme_extension, theme_fallback );
}

std::string PATH_INFO::names()
{
    return find_translated_file( datadir_value + "names/", ".json",
                                 datadir_value + "names/" + "en.json" );
}

void PATH_INFO::set_datadir( const std::string &datadir )
{
    datadir_value = datadir;
    // Shared dirs
    gfxdir_value = datadir_value + "gfx/";

    // Shared files
    motd_value = datadir_value + "motd/" + "en.motd";
}

void PATH_INFO::set_config_dir( const std::string &config_dir )
{
    config_dir_value = config_dir;
    options_value = config_dir_value + "options.json";
    autopickup_value = config_dir_value + "auto_pickup.json";
}

void PATH_INFO::set_savedir( const std::string &savedir )
{
    savedir_value = savedir;
}

void PATH_INFO::set_memorialdir( const std::string &memorialdir )
{
    memorialdir_value = memorialdir;
}

void PATH_INFO::set_options( const std::string &options )
{
    options_value = options;
}

void PATH_INFO::set_autopickup( const std::string &autopickup )
{
    autopickup_value = autopickup;
}

void PATH_INFO::set_motd( const std::string &motd )
{
    motd_value = motd;
}
