#include "language.h"

#include <algorithm>
#include <fstream>

#if defined(_WIN32)
#  if 1 // Prevent IWYU reordering platform_win.h below mmsystem.h
#    include "platform_win.h"
#  endif
#  include "mmsystem.h"
#endif

#if defined(MACOSX)
#  include <CoreFoundation/CFLocale.h>
#  include <CoreFoundation/CoreFoundation.h>
#endif

#include "cached_options.h"
#include "cata_libintl.h"
#include "catacharset.h"
#include "debug.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "json.h"
#include "mod_manager.h"
#include "name.h"
#include "options.h"
#include "path_info.h"
#include "string_utils.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"
#include "worldfactory.h"

#define dbg(x) DebugLog((x), DC::Main)

std::string to_valid_language( const std::string &lang );
void update_global_locale();

static std::string sys_c_locale;
static std::string sys_cpp_locale;

// System user preferred UI language (nullptr if failed to detect)
static language_info const *system_language = nullptr;

// Cached current game language.
// Unlike USE_LANG option value, it's synchronized with whatever language
// gettext should be using.
// May be nullptr if language hasn't been set yet.
static language_info const *current_language = nullptr;

static language_info fallback_language = { "en", R"(English)", "en_US.UTF-8", { "n" }, "", { 1033 } };

std::vector<language_info> lang_options = { fallback_language };

static language_info const *get_lang_info( const std::string &lang )
{
    for( const language_info &li : lang_options ) {
        if( li.id == lang ) {
            return &li;
        }
    }
    return &fallback_language;
}

const std::vector<language_info> &list_available_languages()
{
    return lang_options;
}

// Names depend on the language settings. They are loaded from different files
// based on the currently used language. If that changes, we have to reload the
// names.
static void reload_names()
{
    Name::clear();
    Name::load_from_file( PATH_INFO::names() );
}

#if defined(MACOSX)
static std::string getSystemUILang()
{
    // Get the user's language list (in order of preference)
    CFArrayRef langs = CFLocaleCopyPreferredLanguages();
    if( CFArrayGetCount( langs ) == 0 ) {
        return "";
    }

    CFStringRef lang = static_cast<CFStringRef>( CFArrayGetValueAtIndex( langs, 0 ) );
    const char *lang_code_raw_fast = CFStringGetCStringPtr( lang, kCFStringEncodingUTF8 );
    std::string lang_code;
    if( lang_code_raw_fast ) { // fast way, probably it's never works
        lang_code = lang_code_raw_fast;
    } else { // fallback to slow way
        CFIndex length = CFStringGetLength( lang ) + 1;
        std::vector<char> lang_code_raw_slow( length, '\0' );
        bool success = CFStringGetCString( lang, lang_code_raw_slow.data(), length, kCFStringEncodingUTF8 );
        if( !success ) {
            return "";
        }
        lang_code = lang_code_raw_slow.data();
    }

    // Convert to the underscore format expected by gettext
    std::replace( lang_code.begin(), lang_code.end(), '-', '_' );

    for( const language_info &info : lang_options ) {
        if( !info.osx.empty() && lang_code.starts_with( info.osx ) ) {
            return info.id;
        }
    }

    return to_valid_language( lang_code );
}
#endif // MACOSX

std::string to_valid_language( const std::string &lang )
{
    if( lang.empty() ) {
        return lang;
    }
    for( const language_info &info : lang_options ) {
        if( lang.starts_with( info.id ) ) {
            return info.id;
        }
    }
    const size_t p = lang.find( '_' );
    if( p != std::string::npos ) {
        std::string lang2 = lang.substr( 0, p );
        for( const language_info &info : lang_options ) {
            if( lang2.starts_with( info.id ) ) {
                return info.id;
            }
        }
    }
    return "";
}

#if defined(_WIN32)
static std::string getLangFromLCID( const int &lcid )
{
    for( const language_info &info : lang_options ) {
        for( int lang_lcid : info.lcids ) {
            if( lang_lcid == lcid ) {
                return info.id;
            }
        }
    }
    return "";
}

static std::string getSystemUILang()
{
    return getLangFromLCID( GetUserDefaultUILanguage() );
}
#elif !defined(MACOSX)
// Linux / Android
static std::string getSystemUILang()
{
    std::string ret;

    const char *language = getenv( "LANGUAGE" );
    if( language && language[0] != '\0' ) {
        ret = language;
    } else {
        const char *loc = setlocale( LC_MESSAGES, nullptr );
        if( loc != nullptr ) {
            ret = loc;
        }
    }

    if( ret == "C" || ret.starts_with( "C." ) ) {
        ret = "en";
    }

    return to_valid_language( ret );
}
#endif // _WIN32 / !MACOSX

void set_language()
{
    // Step 1. Choose language id
    std::string lang_opt = get_option<std::string>( "USE_LANG" );
    if( lang_opt.empty() ) {
        if( system_language ) {
            lang_opt = system_language->id;
            current_language = system_language;
        } else {
            lang_opt = fallback_language.id;
            current_language = &fallback_language;
        }
    } else {
        current_language = get_lang_info( lang_opt );
    }

    dbg( DL::Info ) << "Language set to '" << lang_opt << "'";

    // Step 2. Setup locale
    update_global_locale();

    // Step 3. Load translations for game and, possibly, mods
    l10n_data::reload_catalogues();

    // Step 4. Finalize
    reload_names();
}

static std::vector<language_info> load_languages( const std::string &filepath )
{
    std::vector<language_info> ret;
    try {
        std::ifstream stream( filepath, std::ios_base::binary );
        if( !stream.is_open() ) {
            throw std::runtime_error( string_format( "File '%s' not found", filepath ) );
        }
        JsonIn json( stream );
        JsonArray arr = json.get_array();
        for( const JsonObject &obj : arr ) {
            language_info info;
            info.id = obj.get_string( "id" );
            info.name = obj.get_string( "name" );
            info.locale = obj.get_string( "locale" );
            info.genders = obj.get_string_array( "genders" );
            info.osx = obj.get_string( "osx", "" );
            info.lcids = obj.get_int_array( "lcids" );
            ret.push_back( info );
        }
    } catch( const std::exception &e ) {
        debugmsg( "[lang] Failed to read language definitions: %s", e.what() );
        return std::vector<language_info>();
    }

    // Sanity check genders
    const std::vector<std::string> all_genders = {{"f", "m", "n"}};

    for( language_info &info : ret ) {
        for( const std::string &g : info.genders ) {
            if( std::ranges::find( all_genders, g ) == all_genders.end() ) {
                debugmsg( "Unexpected gender '%s' in grammatical gender list for language '%d'",
                          g, info.id );
            }
        }
        if( info.genders.empty() ) {
            info.genders.emplace_back( "n" );
        }
    }

    return ret;
}

bool init_language_system()
{
    // OS X does not populate locale env vars correctly
    // (they usually default to "C") so don't bother
    // trying to set the locale based on them.
#if !defined(MACOSX)
    if( setlocale( LC_ALL, "" ) == nullptr ) {
        dbg( DL::Warn ) << "Error while setlocale(LC_ALL, '').";
    } else {
#endif
        try {
            std::locale::global( std::locale( "" ) );
        } catch( const std::exception & ) {
            // if user default locale retrieval isn't implemented by system
            try {
                // default to basic C locale
                std::locale::global( std::locale::classic() );
            } catch( const std::exception &err ) {
                dbg( DL::Error ) << err.what();
                return false;
            }
        }
#if !defined(MACOSX)
    }
#endif

    sys_c_locale = setlocale( LC_ALL, nullptr );
    sys_cpp_locale = std::locale().name();
    dbg( DL::Info ) << "C locale on startup: '" << sys_c_locale << "'";
    dbg( DL::Info ) << "C++ locale on startup: '" << sys_cpp_locale << "'";

    lang_options = load_languages( PATH_INFO::language_defs_file() );
    if( lang_options.empty() ) {
        lang_options = { fallback_language };
    }

    std::string lang = getSystemUILang();
    if( lang.empty() ) {
        system_language = nullptr;
        dbg( DL::Warn ) << "Failed to detect system UI language.";
    } else {
        system_language = get_lang_info( lang );
        dbg( DL::Info ) << "Detected system UI language as '" << lang << "'";
    }

    return true;
}

void prompt_select_lang_on_startup()
{
    if( !get_option<std::string>( "USE_LANG" ).empty() || system_language ) {
        return;
    }

    std::string res;

    if( lang_options.empty() ) {
        res = fallback_language.id;
    } else if( lang_options.size() == 1 ) {
        res = lang_options[0].id;
    } else {
        uilist sm;
        sm.allow_cancel = false;
        sm.text = _( "Select your language" );
        for( size_t i = 0; i < lang_options.size(); i++ ) {
            sm.addentry( i, true, MENU_AUTOASSIGN, lang_options[i].name );
        }
        sm.query();
        res = lang_options[sm.ret].id;
    }

    get_options().get_option( "USE_LANG" ).setValue( res );
    get_options().save();

    set_language();
}

const language_info &get_language()
{
    if( current_language ) {
        return *current_language;
    } else {
        return fallback_language;
    }
}

void update_global_locale()
{
#if defined(_WIN32)
    // Use the ANSI code page 1252 to work around some language output bugs.
    if( setlocale( LC_ALL, ".1252" ) == nullptr ) {
        dbg( DL::Warn ) << "Error while setlocale(LC_ALL, '.1252').";
    }
#else // _WIN32
    std::string lang = ::get_option<std::string>( "USE_LANG" );

    bool set_user = false;
    if( lang.empty() ) {
        // Restore user locale
        set_user = true;
    } else {
        // Try specific locale
        try {
            std::locale::global( std::locale( get_lang_info( lang )->locale ) );
        } catch( std::runtime_error &e ) {
            // Try fairly neutral UTF-8 locale
            try {
                std::locale::global( std::locale( fallback_language.locale ) );
            } catch( std::runtime_error &e ) {
                // No choice left
                set_user = true;
            }
        }
    }
    if( set_user ) {
        setlocale( LC_ALL, sys_c_locale.c_str() );
        try {
            std::locale::global( std::locale( sys_cpp_locale ) );
        } catch( std::runtime_error &e ) {
            std::locale::global( std::locale::classic() );
        }
    }

#endif // _WIN32

    dbg( DL::Info ) << "C locale set to '" << setlocale( LC_ALL, nullptr ) << "'";
    dbg( DL::Info ) << "C++ locale set to '" << std::locale().name() << "'";
}

std::vector<std::string> get_lang_path_substring( const std::string &lang_id )
{
    std::vector<std::string> ret;

    const size_t p = lang_id.find( '_' );
    if( p == std::string::npos ) {
        // Dialect-agnostic id ('en', 'fr', 'de', etc.)
        ret.push_back( lang_id );
    } else {
        // Id with dialect specified ('en_US', 'fr_FR', etc.)
        // First try loading exact resource, then try dialect-agnostic resource.
        std::string lang_only = lang_id.substr( 0, p );
        ret.push_back( lang_id );
        ret.push_back( lang_only );
    }
    return ret;
}

bool translations_exists_for_lang( const std::string &lang_id )
{

    std::vector<std::string> opts = get_lang_path_substring( lang_id );
    for( const std::string &s : opts ) {
        std::string path = PATH_INFO::base_path() + "lang/mo/" + s + "/LC_MESSAGES/cataclysm-bn.mo";
        if( file_exist( path ) ) {
            return true;
        }
    }
    return false;
}

bool localized_comparator::operator()( const std::string &l, const std::string &r ) const
{
    // We need different implementations on each platform.  MacOS seems to not
    // support localized comparison of strings via the standard library at all,
    // so resort to MacOS-specific solution.  Windows cannot be expected to be
    // using a UTF-8 locale (whereas our strings are always UTF-8) and so we
    // must convert to wstring for comparison there.  Linux seems to work as
    // expected on regular strings; no workarounds needed.
    // See https://github.com/CleverRaven/Cataclysm-DDA/pull/40041 for further
    // discussion.
#if defined(MACOSX)
    CFStringRef lr = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, l.c_str(),
                     kCFStringEncodingUTF8, kCFAllocatorNull );
    CFStringRef rr = CFStringCreateWithCStringNoCopy( kCFAllocatorDefault, r.c_str(),
                     kCFStringEncodingUTF8, kCFAllocatorNull );
    bool result = CFStringCompare( lr, rr, kCFCompareLocalized ) < 0;
    CFRelease( lr );
    CFRelease( rr );
    return result;
#elif defined(_WIN32)
    return ( *this )( utf8_to_wstr( l ), utf8_to_wstr( r ) );
#else
    return std::locale()( l, r );
#endif
}

bool localized_comparator::operator()( const std::wstring &l, const std::wstring &r ) const
{
#if defined(MACOSX)
    return ( *this )( wstr_to_utf8( l ), wstr_to_utf8( r ) );
#else
    return std::locale()( l, r );
#endif
}

// ==============================================================================================
// Translation files management
// ==============================================================================================

using cata_libintl::trans_library;
using cata_libintl::trans_catalogue;

namespace l10n_data
{
static trans_library trans_lib_singleton;
const trans_library &get_library()
{
    return trans_lib_singleton;
}

static void set_library( trans_library lib )
{
    trans_lib_singleton = std::move( lib );
    invalidate_translations();
}

static void add_cat_if_exists( std::vector<trans_catalogue> &list, const std::string &lang_id,
                               const std::string &path_start, const std::string &path_end )
{
    std::vector<std::string> opts = get_lang_path_substring( lang_id );
    for( const std::string &s : opts ) {
        std::string path = path_start + s + path_end;
        if( !file_exist( path ) ) {
            continue;
        }
        try {
            list.push_back( trans_catalogue::load_from_file( path ) );
        } catch( const std::runtime_error &err ) {
            debugmsg( "Failed to load translation catalogue '%s': %s", path, err.what() );
        }
        break;
    }
}

static void add_mod_catalogue_if_exists( std::vector<trans_catalogue> &list,
        const std::string &lang_id, const std::string &mod_path )
{
    add_cat_if_exists( list, lang_id, mod_path + "/lang/", ".mo" );
}

static void add_base_catalogue( std::vector<trans_catalogue> &list, const std::string &lang_id )
{
    // TODO: split source code strings from data strings
    //       and load data translations from separate file(s)
    add_cat_if_exists( list, lang_id,
                       PATH_INFO::base_path() + "lang/mo/",
                       "/LC_MESSAGES/cataclysm-bn.mo"
                     );
}

static bool add_mod_catalogues( std::vector<trans_catalogue> &list, const std::string &lang_id )
{
    if( !world_generator || !world_generator->active_world ) {
        return false;
    }

    const std::vector<mod_id> &mods = world_generator->active_world->info->active_mod_order;
    for( const mod_id &mod : mods ) {
        add_mod_catalogue_if_exists( list, lang_id, mod->path );
    }
    return true;
}

void reload_catalogues()
{
    std::vector<trans_catalogue> list;
    add_base_catalogue( list, get_language().id );
    add_mod_catalogues( list, get_language().id );
    set_library( trans_library::create( std::move( list ) ) );
}

static bool mod_catalogues_loaded = false;

void unload_catalogues()
{
    mod_catalogues_loaded = false;
    set_library( trans_library::create( {} ) );
}

void load_mod_catalogues()
{
    assert( !mod_catalogues_loaded );
    std::vector<trans_catalogue> list;
    add_base_catalogue( list, get_language().id );
    mod_catalogues_loaded = add_mod_catalogues( list, get_language().id );
    set_library( trans_library::create( std::move( list ) ) );
}

void unload_mod_catalogues()
{
    if( !mod_catalogues_loaded ) {
        return;
    }

    mod_catalogues_loaded = false;
    std::vector<trans_catalogue> list;
    add_base_catalogue( list, get_language().id );
    set_library( trans_library::create( std::move( list ) ) );
}

} // namespace l10n_data

void translatable_mod_info::update()
{
    language_version = detail::get_current_language_version();

    // First, try base game's translation file (for in-repo mods)
    name_tr = _( name_raw_ );
    description_tr = _( description_raw );

    if( name_tr != name_raw_ || description_tr != description_raw ) {
        return;
    }

    // For 3rd-party mods, try that mod's translation file
    std::vector<trans_catalogue> list;
    l10n_data::add_mod_catalogue_if_exists( list, get_language().id, mod_path );
    if( list.empty() ) {
        return;
    }
    trans_library lib = trans_library::create( std::move( list ) );

    name_tr = lib.get( name_raw_.c_str() );
    description_tr = lib.get( description_raw.c_str() );
}
