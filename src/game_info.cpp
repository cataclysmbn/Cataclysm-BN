#include <algorithm>
#include <sstream>
#include <ranges>

#include "game_info.h"
#include "options.h"
#include "get_version.h"
#include "string_formatter.h"
#include "catalua.h"
#include "cata_utility.h"
#include "cached_options.h"
#include "color.h"
#include "cursesdef.h"
#include "enum_bitset.h"
#include "enum_conversions.h"
#include "filesystem.h"
#include "input.h"
#include "language.h"
#include "mod_manager.h"
#include "output.h"
#include "path_info.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "worldfactory.h"

#if defined(_WIN32)
#   if 1 // HACK: Hack to prevent reordering of #include "platform_win.h" by IWYU
#       include "platform_win.h"
#   endif
#endif

auto game_info::operating_system() -> std::string
{
#if defined(__ANDROID__)
    return "Android";
#elif defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#elif defined(unix) || defined(__unix__) || defined(__unix) || ( defined(__APPLE__) && defined(__MACH__) ) // unix; BSD; MacOs
#if defined(__APPLE__) && defined(__MACH__)
    // The following include is **only** needed for the TARGET_xxx defines below and is only included if both of the above defines are true.
    // The whole function only relying on compiler defines, it is probably more meaningful to include it here and not mingle with the
    // headers at the top of the .cpp file.
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR == 1
    /* iOS in Xcode simulator */
    return "iOS Simulator";
#elif TARGET_OS_IPHONE == 1
    /* iOS on iPhone, iPad, etc. */
    return "iOS";
#elif TARGET_OS_MAC == 1
    /* OSX */
    return "MacOs";
#endif // TARGET_IPHONE_SIMULATOR
#elif defined(BSD)
    return "BSD";
#else
    return "Unix";
#endif // __APPLE__
#else
    return "Unknown";
#endif
}

#if !defined(__CYGWIN__) && !defined (__ANDROID__) && ( defined (__linux__) || defined(unix) || defined(__unix__) || defined(__unix) || ( defined(__APPLE__) && defined(__MACH__) ) || defined(BSD) ) // linux; unix; MacOs; BSD
/** Execute a command with the shell by using `popen()`.
 * @param command The full command to execute.
 * @note The output buffer is limited to 512 characters.
 * @returns The result of the command (only stdout) or an empty string if there was a problem.
 */
static auto shell_exec( const std::string &command ) -> std::string
{
    std::vector<char> buffer( 512 );
    std::string output;
    struct file_closer_deleter {
        void operator()( FILE *f ) const {
            ( void )pclose( f );
        }
    };
    try {
        std::unique_ptr<FILE, file_closer_deleter> pipe( popen( command.c_str(), "r" ) );
        if( pipe ) {
            while( fgets( buffer.data(), buffer.size(), pipe.get() ) != nullptr ) {
                output += buffer.data();
            }
        }
    } catch( ... ) {
        output = "";
    }
    return output;
}
#endif

#if defined (__ANDROID__)
/** Get a precise version number for Android systems.
 * @note see:
 *    - https://stackoverflow.com/a/19777977/507028
 *    - https://github.com/pytorch/cpuinfo/blob/master/test/build.prop/galaxy-s7-us.log
 * @returns If successful, a string containing the Android system version, otherwise an empty string.
 */
static std::string android_version()
{
    std::string output;

    // buffer used for the __system_property_get() function.
    // note: according to android sources, it can't be greater than 92 chars (see 'PROP_VALUE_MAX' define in system_properties.h)
    std::vector<char> buffer( 255 );

    static const std::vector<std::pair<std::string, std::string>> system_properties = {
        // The manufacturer of the product/hardware; e.g. "Samsung", this is different than the carrier.
        { "ro.product.manufacturer", "Manufacturer" },
        // The end-user-visible name for the end product; .e.g. "SAMSUNG-SM-G930A" for a Samsung S7.
        { "ro.product.model", "Model" },
        // The Android system version; e.g. "6.0.1"
        { "ro.build.version.release", "Release" },
        // The internal value used by the underlying source control to represent this build; e.g "G930AUCS4APK1" for a Samsung S7 on 6.0.1.
        { "ro.build.version.incremental", "Incremental" },
    };

    for( const auto &entry : system_properties ) {
        int len = __system_property_get( entry.first.c_str(), &buffer[0] );
        std::string value;
        if( len <= 0 ) {
            // failed to get the property
            value = "<unknown>";
        } else {
            value = std::string( buffer.data() );
        }
        output.append( string_format( "%s: %s; ", entry.second, value ) );
    }
    return output;
}

#elif defined(BSD)

/** Get a precise version number for BSD systems.
 * @note The code shells-out to call `uname -a`.
 * @returns If successful, a string containing the Linux system version, otherwise an empty string.
 */
static std::string bsd_version()
{
    std::string output;
    output = shell_exec( "uname -a" );
    if( !output.empty() ) {
        // remove trailing '\n', if any.
        output.erase( std::remove( output.begin(), output.end(), '\n' ),
                      output.end() );
    }
    return output;
}

#elif defined(__linux__)

/** Get a precise version number for Linux systems.
 * @note The code shells-out to call `lsb_release -a`.
 * @returns If successful, a string containing the Linux system version, otherwise an empty string.
 */
static auto linux_version() -> std::string
{
    std::string output;
    output = shell_exec( "lsb_release -a" );
    if( !output.empty() ) {
        // replace '\n' and '\t' in output.
        static const std::vector<std::pair<std::string, std::string>> to_replace = {
            {"\n", "; "},
            {"\t", " "}, // NOLINT(cata-text-style)
        };
        for( const auto &e : to_replace ) {
            std::string::size_type pos;
            while( ( pos = output.find( e.first ) ) != std::string::npos ) {
                output.replace( pos, e.first.length(), e.second );
            }
        }
    }
    return output;
}

#elif defined(__APPLE__) && defined(__MACH__) && !defined(BSD)

/** Get a precise version number for MacOs systems.
 * @note The code shells-out to call `sw_vers` with various options.
 * @returns If successful, a string containing the MacOS system version, otherwise an empty string.
 */
static std::string mac_os_version()
{
    std::string output;
    static const std::vector<std::pair<std::string,  std::string>> commands = {
        { "sw_vers -productName", "Name" },
        { "sw_vers -productVersion", "Version" },
        { "sw_vers -buildVersion", "Build" },
    };

    for( const auto &entry : commands ) {
        std::string command_result = shell_exec( entry.first );
        if( command_result.empty() ) {
            command_result = "<unknown>";
        } else {
            // remove trailing '\n', if any.
            command_result.erase( std::remove( command_result.begin(), command_result.end(), '\n' ),
                                  command_result.end() );
        }
        output.append( string_format( "%s: %s; ", entry.second, command_result ) );
    }
    return output;
}

#elif defined (_WIN32)

/** Get a precise version number for Windows systems.
 * @note Since Windows 10 all version-related APIs lie about the underlying system if the application is not Manifested (see VerifyVersionInfoA
 *     or GetVersionEx documentation for further explanation). In this function we use the registry or the native RtlGetVersion which both
 *     report correct versions and are compatible down to XP.
 * @returns If successful, a string containing the Windows system version number, otherwise an empty string.
 */
static std::string windows_version()
{
    std::string output;
    HKEY handle_key;
    bool success = RegOpenKeyExA( HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)",
                                  0,
                                  KEY_QUERY_VALUE, &handle_key ) == ERROR_SUCCESS;
    if( success ) {
        DWORD value_type;
        constexpr DWORD c_buffer_size = 512;
        std::vector<BYTE> byte_buffer( c_buffer_size );
        DWORD buffer_size = c_buffer_size;
        DWORD major_version = 0;
        success = RegQueryValueExA( handle_key, "CurrentMajorVersionNumber", nullptr, &value_type,
                                    &byte_buffer[0], &buffer_size ) == ERROR_SUCCESS && value_type == REG_DWORD;
        if( success ) {
            major_version = *reinterpret_cast<const DWORD *>( &byte_buffer[0] );
            output.append( std::to_string( major_version ) );
        }
        if( success ) {
            buffer_size = c_buffer_size;
            success = RegQueryValueExA( handle_key, "CurrentMinorVersionNumber", nullptr, &value_type,
                                        &byte_buffer[0], &buffer_size ) == ERROR_SUCCESS && value_type == REG_DWORD;
            if( success ) {
                const DWORD minor_version = *reinterpret_cast<const DWORD *>( &byte_buffer[0] );
                output.append( "." );
                output.append( std::to_string( minor_version ) );
            }
        }
        if( success ) {
            buffer_size = c_buffer_size;
            success = RegQueryValueExA( handle_key, "CurrentBuildNumber", nullptr, &value_type, &byte_buffer[0],
                                        &buffer_size ) == ERROR_SUCCESS && value_type == REG_SZ;

            if( success ) {
                output.append( "." );
                output.append( std::string( reinterpret_cast<char *>( &byte_buffer[0] ) ) );
            }
            if( success ) {
                buffer_size = c_buffer_size;
                success = RegQueryValueExA( handle_key, "UBR", nullptr, &value_type, &byte_buffer[0],
                                            &buffer_size ) == ERROR_SUCCESS && value_type == REG_DWORD;
                if( success ) {
                    output.append( "." );
                    output.append( std::to_string( *reinterpret_cast<const DWORD *>( &byte_buffer[0] ) ) );
                }
            }

            // Applies to both Windows 10 and Windows 11
            if( major_version == 10 ) {
                buffer_size = c_buffer_size;
                // present in Windows 10 version >= 20H2 (aka 2009) and Windows 11
                success = RegQueryValueExA( handle_key, "DisplayVersion", nullptr, &value_type, &byte_buffer[0],
                                            &buffer_size ) == ERROR_SUCCESS && value_type == REG_SZ;

                if( !success ) {
                    // only accurate in Windows 10 version <= 2009
                    buffer_size = c_buffer_size;
                    success = RegQueryValueExA( handle_key, "ReleaseId", nullptr, &value_type, &byte_buffer[0],
                                                &buffer_size ) == ERROR_SUCCESS && value_type == REG_SZ;
                }

                if( success ) {
                    output.append( " (" );
                    output.append( std::string( reinterpret_cast<char *>( byte_buffer.data() ) ) );
                    output.append( ")" );
                }
            }
        }

        RegCloseKey( handle_key );
    }

    if( !success ) {
#if defined (__MINGW32__) || defined (__MINGW64__) || defined (__CYGWIN__) || defined (MSYS2)
        output = "MINGW/CYGWIN/MSYS2 on unknown Windows version";
#else
        output.clear();
        using RtlGetVersion = LONG( WINAPI * )( PRTL_OSVERSIONINFOW );
        const HMODULE handle_ntdll = GetModuleHandleA( "ntdll" );
        if( handle_ntdll != nullptr ) {
            // Use union-based type-punning to convert function pointer
            // type without gcc warnings.
            union {
                RtlGetVersion p;
                FARPROC q;
            } rtl_get_version_func;
            rtl_get_version_func.q = GetProcAddress( handle_ntdll, "RtlGetVersion" );
            if( rtl_get_version_func.p != nullptr ) {
                RTL_OSVERSIONINFOW os_version_info = RTL_OSVERSIONINFOW();
                os_version_info.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
                if( rtl_get_version_func.p( &os_version_info ) == 0 ) { // NT_STATUS_SUCCESS = 0
                    output.append( string_format( "%i.%i %i", os_version_info.dwMajorVersion,
                                                  os_version_info.dwMinorVersion, os_version_info.dwBuildNumber ) );
                }
            }
        }
#endif
    }
    return output;
}
#endif // Various OS define tests

auto game_info::operating_system_version() -> std::string
{
#if defined(__ANDROID__)
    return android_version();
#elif defined(BSD)
    return bsd_version();
#elif defined(__linux__)
    return linux_version();
#elif defined(__APPLE__) && defined(__MACH__) && !defined(BSD)
    return mac_os_version();
#elif defined(_WIN32)
    return windows_version();
#else
    return "<unknown>";
#endif
}

auto game_info::bitness() -> std::optional<int>
{
    if( sizeof( void * ) == 8 ) {
        return 64;
    }

    if( sizeof( void * ) == 4 ) {
        return 32;
    }

    return std::nullopt;
}

auto game_info::bitness_string() -> std::string
{
    auto b = bitness();
    if( b && *b == 32 ) {
        return "32-bit";
    }
    if( b && *b == 64 ) {
        return "64-bit";
    }
    return "Unknown";
}

auto game_info::game_version() -> std::string
{
    return getVersionString();
}

auto game_info::graphics_version() -> std::string
{
#if defined(TILES)
    return "Tiles";
#else
    return "Curses";
#endif
}

auto game_info::save_file_version() -> std::string
{
    const auto &world = world_generator->active_world;
    if( world == nullptr ) {
        return "No active world";
    }
    const auto *info = world->info;
    if( info == nullptr ) {
        return "No world info";
    }
    switch( info->world_save_format ) {
        case save_format::V1:
            return "V1";
        case save_format::V2_COMPRESSED_SQLITE3:
            return "V2 (compressed with sqlite 3)";
    }
    return "No save format";
}

auto game_info::mods_loaded() -> std::string
{
    if( world_generator->active_world == nullptr ) {
        return "No active world";
    }

    const std::vector<mod_id> &mod_ids = world_generator->active_world->info->active_mod_order;
    if( mod_ids.empty() ) {
        return "No loaded mods";
    }

    // e.g. "Bright Nights [bn]".
    const auto mod_names =
    std::views::transform( mod_ids, []( const auto mod ) { return string_format( "%s [%s]", remove_color_tags( mod->name_raw() ), mod->ident.str() ); } )
    | std::ranges::to<std::vector>();

    return join( mod_names, ",\n    " ); // note: 4 spaces for a slight offset.
}

auto game_info::game_report() -> std::string
{
    std::string os_version = operating_system_version();
    if( os_version.empty() ) {
        os_version = "<unknown>";
    }
    std::stringstream report;

    std::string lang = get_option<std::string>( "USE_LANG" );
    std::string lang_translated;
    for( const language_info &info : list_available_languages() ) {
        if( lang == info.id ) {
            lang_translated = info.name;
            break;
        }
    }

    // Note: We shorten 'Lua API' to 'LAPI' so that 'Lua' word does not show up
    //       in every issue out there and pollute GitHub issue search results.
    report <<
           "- OS: " << operating_system() << "\n" <<
           "    - OS Version: " << os_version << "\n" <<
           "- Game Version: " << game_version() << " [" << bitness_string() << "]\n" <<
           "- Graphics Version: " << graphics_version() << "\n" <<
           "- LAPI Version: " << cata::get_lapi_version_string() << "\n" <<
           "- Save File Version: " << save_file_version() << "\n" <<
           "- Game Language: " << lang_translated << " [" << lang << "]\n" <<
           "- Mods loaded: [\n    " << mods_loaded() << "\n]\n";

    return report.str();
}

auto get_os_bitness() -> std::optional<int>
{
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetNativeSystemInfo( &si );
    switch( si.wProcessorArchitecture ) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return 64;

        case PROCESSOR_ARCHITECTURE_INTEL:
            return 32;

        default:
            // FIXME: other architectures?
            break;
    }
#elif defined(__linux__) && !defined(__ANDROID__)
    std::string output;
    output = shell_exec( "getconf LONG_BIT" );

    if( !output.empty() ) {
        // remove trailing '\n', if any.
        output.erase( std::remove( output.begin(), output.end(), '\n' ),
                      output.end() );
    }

    if( output == "64" ) {
        return 64;
    } else if( output == "32" ) {
        return 32;
    }
#endif
    // FIXME: osx, android
    return std::nullopt;
}
