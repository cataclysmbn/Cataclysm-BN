#include "debug.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <regex>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <type_traits>
#include <utility>
#include <vector>

#include "catalua.h"
#include "cata_utility.h"
#include "cached_options.h"
#include "color.h"
#include "cursesdef.h"
#include "enum_bitset.h"
#include "enum_conversions.h"
#include "filesystem.h"
#include "get_version.h"
#include "input.h"
#include "language.h"
#include "mod_manager.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "point.h"
#include "string_utils.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "worldfactory.h"
#include "game_info.h"

#if !defined(_MSC_VER)
#include <sys/time.h>
#endif

#if defined(_WIN32)
#   if 1 // HACK: Hack to prevent reordering of #include "platform_win.h" by IWYU
#       include "platform_win.h"
#   endif
#endif

#if defined(BACKTRACE)
#   if defined(_WIN32)
#       include <dbghelp.h>
#       if defined(LIBBACKTRACE)
#           include <winnt.h>
#       endif
#   elif defined(__ANDROID__)
#       include <unwind.h>
#       include <dlfcn.h>
#   else
#       include <execinfo.h>
#       include <unistd.h>
#   endif
#endif

#if defined(LIBBACKTRACE)
#   include <backtrace.h>
#endif

#if defined(TILES)
#include "sdl_wrappers.h"
#endif // TILES

#if defined(__ANDROID__)
// used by android_version() function for __system_property_get().
#include <sys/system_properties.h>
#endif

#if (defined(__DragonFly__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)) && !defined(BSD)
#define BSD
#endif

// Hack for loading weird saves
bool dont_debugmsg = false;


// Static defines                                                   {{{1
// ---------------------------------------------------------------------

static enum_bitset<DL> debugLevel;
static enum_bitset<DC> debugClass;

/** True during game startup, when debugmsgs cannot be displayed yet. */
static bool buffering_debugmsgs = true;

/** Set to true when any error is logged. */
static bool error_observed = false;

/** If true, debug messages will be captured,
 * used to test debugmsg calls in the unit tests
 */
static bool capturing = false;
/** сaptured debug messages */
static std::string captured;


#if defined(_WIN32) && defined(LIBBACKTRACE)
// Get the image base of a module from its PE header
static uintptr_t get_image_base( const char *const path )
{
    HANDLE file = CreateFile( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL );
    if( file == INVALID_HANDLE_VALUE ) {
        return 0;
    }
    on_out_of_scope close_file( [file]() {
        CloseHandle( file );
    } );

    HANDLE mapping = CreateFileMapping( file, NULL, PAGE_READONLY, 0, 0, NULL );
    if( mapping == NULL ) {
        return 0;
    }
    on_out_of_scope close_mapping( [mapping]() {
        CloseHandle( mapping );
    } );

    LONG nt_header_offset = 0;
    {
        LPVOID dos_header_view = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0, sizeof( IMAGE_DOS_HEADER ) );
        if( dos_header_view == NULL ) {
            return 0;
        }
        on_out_of_scope close_dos_header_view( [dos_header_view]() {
            UnmapViewOfFile( dos_header_view );
        } );

        PIMAGE_DOS_HEADER dos_header = reinterpret_cast<PIMAGE_DOS_HEADER>( dos_header_view );
        if( dos_header->e_magic != IMAGE_DOS_SIGNATURE ) {
            return 0;
        }
        nt_header_offset = dos_header->e_lfanew;
    }

    LPVOID pe_header_view = MapViewOfFile( mapping, FILE_MAP_READ, 0, 0,
                                           nt_header_offset + sizeof( IMAGE_NT_HEADERS ) );
    if( pe_header_view == NULL ) {
        return 0;
    }
    on_out_of_scope close_pe_header_view( [pe_header_view]() {
        UnmapViewOfFile( pe_header_view );
    } );

    PIMAGE_NT_HEADERS nt_header = reinterpret_cast<PIMAGE_NT_HEADERS>(
                                      reinterpret_cast<uintptr_t>( pe_header_view ) + nt_header_offset );
    if( nt_header->Signature != IMAGE_NT_SIGNATURE
        || nt_header->FileHeader.SizeOfOptionalHeader != sizeof( IMAGE_OPTIONAL_HEADER ) ) {
        return 0;
    }
    return nt_header->OptionalHeader.ImageBase;
}
#endif

/**
 * Class for capturing debugmsg,
 * used by capture_debugmsg_during.
 */
class capture_debugmsg
{
    public:
        capture_debugmsg();
        std::string dmsg();
        ~capture_debugmsg();
};

std::string capture_debugmsg_during( const std::function<void()> &func )
{
    capture_debugmsg capture;
    func();
    return capture.dmsg();
}

capture_debugmsg::capture_debugmsg()
{
    capturing = true;
    captured = "";
}

std::string capture_debugmsg::dmsg()
{
    capturing = false;
    return captured;
}

capture_debugmsg::~capture_debugmsg()
{
    capturing = false;
}

bool debug_has_error_been_observed()
{
    return error_observed;
}

struct buffered_prompt_info {
    std::string filename;
    std::string line;
    std::string funcname;
    std::string text;
    bool force;
};

namespace
{

std::set<std::string> ignored_messages;

} // namespace

// debugmsg prompts that could not be shown immediately are buffered and replayed when catacurses::stdscr is available
// need to use method here to ensure `buffered_prompts` vector is initialized single time
static std::vector<buffered_prompt_info> &buffered_prompts()
{
    static std::vector<buffered_prompt_info> buffered_prompts;
    return buffered_prompts;
}

static void debug_error_prompt(
    const char *filename,
    const char *line,
    const char *funcname,
    const char *text,
    bool force )
{
    assert( catacurses::stdscr );
    assert( filename != nullptr );
    assert( line != nullptr );
    assert( funcname != nullptr );
    assert( text != nullptr );

    std::string msg_key( filename );
    msg_key += line;

    if( !force && ignored_messages.contains( msg_key ) ) {
        return;
    }

    std::string formatted_report = [&]() {
        const char *repetition_string =
            _( "Excessive error repetition detected.  Please file a bug report at https://github.com/cataclysmbnteam/Cataclysm-BN/issues" );
        // try to prepend repetition string if we are forcing the display. Right now that's the only reason for this prompt to display.
        std::string pre = force ? string_format(
                              "            %s\n",
                              repetition_string
                          ) : "";
        return string_format(
                   "%s"
                   " DEBUG    : %s\n\n"
                   " FUNCTION : %s\n"
                   " FILE     : %s\n"
                   " LINE     : %s\n"
                   " VERSION  : BN %s\n",
                   pre, text, funcname, filename, line, getVersionString()
               );
    }
    ();

#if defined(BACKTRACE)
    std::string backtrace_instructions =
        string_format(
            _( "See %s for a full stack backtrace" ),
            PATH_INFO::debug()
        );
#endif

    // Create a special debug message UI that does various things to ensure
    // the graphics are correct when the debug message is displayed during a
    // redraw callback.
    ui_adaptor ui( ui_adaptor::debug_message_ui {} );
    const auto init_window = []( ui_adaptor & ui ) {
        ui.position_from_window( catacurses::stdscr );
    };
    init_window( ui );
    ui.on_screen_resize( init_window );
    const std::string message = string_format(
                                    "\n\n" // Looks nicer with some space
                                    " %s\n" // translated user string: error notification
                                    " -----------------------------------------------------------\n"
                                    "%s"
                                    " -----------------------------------------------------------\n"
#if defined(BACKTRACE)
                                    " %s\n" // translated user string: where to find backtrace
#endif
                                    " %s\n" // translated user string: space to continue
                                    " %s\n" // translated user string: ignore key
#if defined(TILES)
                                    " %s\n" // translated user string: copy
#endif // TILES
                                    , _( "An error has occurred!  Written below is the error report:" ),
                                    formatted_report,
#if defined(BACKTRACE)
                                    backtrace_instructions,
#endif
                                    _( "Press <color_white>space bar</color> to continue the game." ),
                                    _( "Press <color_white>I</color> (or <color_white>i</color>) to also ignore this particular message in the future." )
#if defined(TILES)
                                    , _( "Press <color_white>C</color> (or <color_white>c</color>) to copy this message to the clipboard." )
#endif // TILES
                                );
    ui.on_redraw( [&]( const ui_adaptor & ) {
        catacurses::erase();
        fold_and_print( catacurses::stdscr, point_zero, getmaxx( catacurses::stdscr ), c_light_red,
                        "%s", message );
        wnoutrefresh( catacurses::stdscr );
    } );

#if defined(__ANDROID__)
    input_context ctxt( "DEBUG_MSG" );
    ctxt.register_manual_key( 'C' );
    ctxt.register_manual_key( 'I' );
    ctxt.register_manual_key( ' ' );
#endif
    for( bool stop = false; !stop && !dont_debugmsg; ) {
        ui_manager::redraw();
        switch( inp_mngr.get_input_event().get_first_input() ) {
#if defined(TILES)
            case 'c':
            case 'C':
                SDL_SetClipboardText( formatted_report.c_str() );
                break;
#endif // TILES
            case 'i':
            case 'I':
                ignored_messages.insert( msg_key );
            /* fallthrough */
            case ' ':
                stop = true;
                break;
        }
    }
}

void replay_buffered_debugmsg_prompts()
{
    if( !buffering_debugmsgs ) {
        return;
    }
    buffering_debugmsgs = false;
    for( const auto &prompt : buffered_prompts() ) {
        debug_error_prompt(
            prompt.filename.c_str(),
            prompt.line.c_str(),
            prompt.funcname.c_str(),
            prompt.text.c_str(),
            prompt.force
        );
    }
    buffered_prompts().clear();
}

struct time_info {
    int hours;
    int minutes;
    int seconds;
    int mseconds;

    template <typename Stream>
    friend Stream &operator<<( Stream &out, const time_info &t ) {
        using char_t = typename Stream::char_type;
        using base   = std::basic_ostream<char_t>;

        static_assert( std::is_base_of_v<base, Stream>, "" );

        out << std::setfill( '0' );
        out << std::setw( 2 ) << t.hours << ':' << std::setw( 2 ) << t.minutes << ':' <<
            std::setw( 2 ) << t.seconds << '.' << std::setw( 3 ) << t.mseconds;

        return out;
    }
};

static time_info get_time() noexcept;

struct repetition_folder {
    const char *m_filename = nullptr;
    const char *m_line = nullptr;
    const char *m_funcname = nullptr;
    std::string m_text;
    time_info m_time;

    static constexpr time_info timeout = { 0, 0, 0, 100 }; // 100ms timeout
    static constexpr int repetition_threshold = 10000;

    int repeat_count = 0;

    bool test( const char *filename, const char *line, const char *funcname, const std::string &text ) {
        return m_filename == filename &&
               m_line == line &&
               m_funcname == funcname &&
               m_text == text &&
               !timed_out();
    }
    void set_time() {
        m_time = get_time();
    }
    void set( const char *filename, const char *line, const char *funcname, const std::string &text ) {
        m_filename = filename;
        m_line = line;
        m_funcname = funcname;
        m_text = text;

        set_time();

        repeat_count = 0;
    }
    void increment_count() {
        ++repeat_count;
        set_time();
    }
    void reset() {
        m_filename = nullptr;
        m_line = nullptr;
        m_funcname = nullptr;

        m_time = time_info{0, 0, 0, 0};

        repeat_count = 0;
    }

    bool timed_out() {
        const time_info now = get_time();

        const int now_raw = now.mseconds + 1000 * now.seconds + 60000 * now.minutes + 3600000 * now.hours;
        const int old_raw = m_time.mseconds + 1000 * m_time.seconds + 60000 * m_time.minutes + 3600000 *
                            m_time.hours;

        const int timeout_raw = timeout.mseconds + 1000 * timeout.seconds + 60000 * timeout.minutes +
                                3600000 * timeout.hours;

        return ( now_raw - old_raw ) > timeout_raw;
    }
};

static repetition_folder rep_folder;
static void output_repetitions( std::ostream &out );

void realDebugmsg( const char *filename, const char *line, const char *funcname,
                   const DL debug_level, const std::string &text )
{
    assert( filename != nullptr );
    assert( line != nullptr );
    assert( funcname != nullptr );

    if( capturing ) {
        captured += text;
    } else {
        if( !rep_folder.test( filename, line, funcname, text ) ) {
            *detail::realDebugLog( debug_level, DC::DebugMsg, filename, line, funcname ) << text;
            rep_folder.set( filename, line, funcname, text );
        } else {
            rep_folder.increment_count();
        }
    }

    if( test_mode ) {
        return;
    }

    // Show excessive repetition prompt once per excessive set
    bool excess_repetition = rep_folder.repeat_count == repetition_folder::repetition_threshold;

    if( buffering_debugmsgs ) {
        buffered_prompts().push_back( {filename, line, funcname, text, false } );
        if( excess_repetition ) {
            buffered_prompts().push_back( {filename, line, funcname, text, true } );
        }
        return;
    }

    debug_error_prompt( filename, line, funcname, text.c_str(), false );
    if( excess_repetition ) {
        debug_error_prompt( filename, line, funcname, text.c_str(), true );
        // Do not count this prompt when considering repetition folding
        // Might look weird in the log if the repetitions end exactly after this prompt is displayed.
        rep_folder.set_time();
    }
}

// Normal functions                                                 {{{1
// ---------------------------------------------------------------------

template<typename E>
static std::string fmt_mask( const enum_bitset<E> &mask )
{
    if( mask.test_all() ) {
        return "ALL";
    } else if( !mask.test_any() ) {
        return "NONE";
    } else {
        std::stringstream ss;
        ss << "[";
        bool first = true;
        for( size_t i = 0; i < enum_bitset<E>::size(); i++ ) {
            E &&e = static_cast<E>( i );
            if( !mask.test( e ) ) {
                continue;
            }
            if( first ) {
                first = false;
            } else {
                ss << " ";
            }
            ss << io::enum_to_string<E>( e );
        }
        ss << "]";
        return ss.str();
    }
}

void setDebugLogLevels( const enum_bitset<DL> &mask, bool silent )
{
    if( mask == debugLevel ) {
        return;
    }
    if( !silent ) {
        DebugLog( DL::Info, DC::Main ) << "Set debug levels to: " << fmt_mask( mask );
    }
    debugLevel = mask;
}

void setDebugLogClasses( const enum_bitset<DC> &mask, bool silent )
{
    if( mask == debugClass ) {
        return;
    }
    if( !silent ) {
        DebugLog( DL::Info, DC::Main ) << "Set debug classes to: " << fmt_mask( mask );
    }
    debugClass = mask;
}

static bool checkDebugLevelClass( DL lev, DC cl )
{
    if( lev == DL::Error || cl == DC::DebugMsg ) {
        return true;
    } else {
        return debugClass.test( cl ) && debugLevel.test( lev );
    }
}

void debug_reset_ignored_messages()
{
    ignored_messages.clear();
}

// Debug only                                                       {{{1
// ---------------------------------------------------------------------

// Debug Includes                                                   {{{2
// ---------------------------------------------------------------------

// Null OStream                                                     {{{2
// ---------------------------------------------------------------------

class NullStream : public std::ostream
{
    public:
        NullStream() : std::ostream( nullptr ) {}
        NullStream( const NullStream & ) = delete;
        NullStream( NullStream && ) = delete;
};

// DebugFile OStream Wrapper                                        {{{2
// ---------------------------------------------------------------------

#if defined(_MSC_VER)
static time_info get_time() noexcept
{
    SYSTEMTIME time {};

    GetLocalTime( &time );

    return time_info { static_cast<int>( time.wHour ), static_cast<int>( time.wMinute ),
                       static_cast<int>( time.wSecond ), static_cast<int>( time.wMilliseconds )
                     };
}
#else
static time_info get_time() noexcept
{
    timeval tv;
    gettimeofday( &tv, nullptr );

    const auto tt      = time_t {tv.tv_sec};
    const auto current = localtime( &tt );

    return time_info { current->tm_hour, current->tm_min, current->tm_sec,
                       static_cast<int>( std::lround( tv.tv_usec / 1000.0 ) )
                     };
}
#endif

struct DebugFile {
    DebugFile();
    ~DebugFile();
    void init( DebugOutput, const std::string &filename );
    void deinit();
    std::ostream &get_file();

    // Using shared_ptr for the type-erased deleter support, not because
    // it needs to be shared.
    std::shared_ptr<std::ostream> file;
    std::string filename;
};

// DebugFile OStream Wrapper                                        {{{2
// ---------------------------------------------------------------------

// needs to be inside the method to ensure it's initialized (and only once)
// NOTE: using non-local static variables (defined at top level in cpp file) here is wrong,
// because DebugLog (that uses them) might be called from the constructor of some non-local static entity
// during dynamic initialization phase, when non-local static variables here are
// only zero-initialized
static DebugFile &debugFile()
{
    static DebugFile debugFile;
    return debugFile;
}

DebugFile::DebugFile() = default;

DebugFile::~DebugFile()
{
    deinit();
}

void DebugFile::deinit()
{
    if( file && file.get() != &std::cerr ) {
        output_repetitions( *file );
        *file << get_time() << " : Log shutdown.\n";
        *file << "-----------------------------------------\n\n";
    }
    file.reset();
}

std::ostream &DebugFile::get_file()
{
    if( !file ) {
        file = std::make_shared<std::ostringstream>();
    }
    return *file;
}

void DebugFile::init( DebugOutput output_mode, const std::string &filename )
{
    std::shared_ptr<std::ostringstream> str_buffer = std::dynamic_pointer_cast<std::ostringstream>
            ( file );

    switch( output_mode ) {
        case DebugOutput::std_err:
            file = std::shared_ptr<std::ostream>( &std::cerr, null_deleter() );
            break;
        case DebugOutput::file: {
            this->filename = filename;
            const std::string oldfile = filename + ".prev";
            bool rename_failed = false;
            struct stat buffer;
            if( stat( filename.c_str(), &buffer ) == 0 ) {
                // Continue with the old log file if it's smaller than 1 MiB
                if( buffer.st_size >= 1024 * 1024 ) {
                    rename_failed = !rename_file( filename, oldfile );
                }
            }
            file = std::make_shared<std::ofstream>(
                       filename.c_str(), std::ios::out | std::ios::app );
            *file << "\n\n-----------------------------------------\n";
            *file << get_time() << " : Starting log.\n";
            DebugLog( DL::Info, DC::Main ) << "Cataclysm BN version " << getVersionString() << " " <<
                                           game_info::bitness_string();
            if( rename_failed ) {
                DebugLog( DL::Info, DC::Main ) << "Moving the previous log file to "
                                               << oldfile << " failed.\n"
                                               << "Check the file permissions.  This "
                                               "program will continue to use the "
                                               "previous log file.";
            }
        }
        break;
        default:
            std::cerr << "Unexpected debug output mode " << static_cast<int>( output_mode )
                      << '\n';
            return;
    }

    if( str_buffer && file ) {
        *file << str_buffer->str();
    }
}

void setupDebug( DebugOutput output_mode )
{
    setDebugLogLevels( enum_bitset<DL>().set( DL::Info ).set( DL::Warn ).set( DL::Error ), true );
    setDebugLogClasses( enum_bitset<DC>().set( DC::Main ).set( DC::DebugMsg ), true );
    debugFile().init( output_mode, PATH_INFO::debug() );
}

void deinitDebug()
{
    debugFile().deinit();
}

// OStream Operators                                                {{{2
// ---------------------------------------------------------------------

namespace io
{
template<>
std::string enum_to_string<DL>( DL x )
{
    switch( x ) {
        // *INDENT-OFF*
        case DL::Info: return "INFO";
        case DL::Warn: return "WARNING";
        case DL::Error: return "ERROR";
        case DL::Debug: return "DEBUG";
        // *INDENT-ON*
        case DL::Num:
            break;
    }
    debugmsg( "Invalid debug level" );
    abort();
}

template<>
std::string enum_to_string<DC>( DC x )
{
    switch( x ) {
        // *INDENT-OFF*
        case DC::DebugMsg: return "DEBUGMSG";
        case DC::DebugModeMsg: return "DMODE";
        case DC::Game: return "GAME";
        case DC::Main: return "MAIN";
        case DC::Map: return "MAP";
        case DC::MapGen: return "MAPGEN";
        case DC::MapMem: return "MAPMEM";
        case DC::NPC: return "NPC";
        case DC::SDL: return "SDL";
        case DC::Lua: return "LUA";
        // *INDENT-ON*
        case DC::Num:
            break;
    }
    debugmsg( "Invalid debug class" );
    abort();
}
} // namespace io

#if defined(BACKTRACE)
#if !defined(_WIN32) && !defined(__CYGWIN__) && !defined(__ANDROID__) && !defined(LIBBACKTRACE)
// Verify that a string is safe for passing as an argument to addr2line.
// In particular, we want to avoid any characters of significance to the shell.
static bool debug_is_safe_string( const char *start, const char *finish )
{
    static constexpr char safe_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                         "abcdefghijklmnopqrstuvwxyz"
                                         "01234567890_./-+";
    using std::begin;
    using std::end;
    const auto is_safe_char =
    [&]( char c ) {
        auto in_safe = std::find( begin( safe_chars ), end( safe_chars ), c );
        return c && in_safe != end( safe_chars );
    };
    return std::all_of( start, finish, is_safe_char );
}

static std::string debug_resolve_binary( const std::string &binary, std::ostream &out )
{
    if( binary.find( '/' ) != std::string::npos ) {
        // The easy case, where we have a path to the binary
        return binary;
    }
    // If the binary name has no slashes then it was found via PATH
    // lookup, and we need to do the same to pass the correct name
    // to addr2line.  An alternative would be to use /proc/self/exe,
    // but that's Linux-specific.
    // Obviously this will not work in all situations, but it will
    // usually do the right thing.
    const char *path = std::getenv( "PATH" );
    if( !path ) {
        // Should be impossible, but I want to avoid segfaults
        // in the crash handler.
        out << "    backtrace: PATH not set\n";
        return binary;
    }

    for( const std::string &path_elem : string_split( path, ':' ) ) {
        if( path_elem.empty() ) {
            continue;
        }
        std::string candidate = path_elem + "/" + binary;
        if( 0 == access( candidate.c_str(), X_OK ) ) {
            return candidate;
        }
    }

    return binary;
}

static std::optional<uintptr_t> debug_compute_load_offset(
    const std::string &binary, const std::string &symbol,
    const std::string &offset_within_symbol_s, void *address, std::ostream &out )
{
    // I don't know a good way to compute this offset.  This
    // seems to work, but I'm not sure how portable it is.
    //
    // backtrace_symbols has provided the address of a symbol as loaded
    // in memory.  We use nm to compute the address of the same symbol
    // in the binary file, and take the difference of the two.
    //
    // There are platform-specific functions which can do similar
    // things (e.g. dladdr1 in GNU libdl) but this approach might
    // perhaps be more portable and adds no link-time dependencies.

    uintptr_t offset_within_symbol = std::stoull( offset_within_symbol_s, nullptr, 0 );
    std::string string_sought = " " + symbol;

    // We need to try calling nm in two different ways, because one
    // works for executables and the other for libraries.
    const char *nm_variants[] = { "nm", "nm -D" };
    for( const char *nm_variant : nm_variants ) {
        std::ostringstream cmd;
        cmd << nm_variant << ' ' << binary << " 2>&1";
        FILE *nm = popen( cmd.str().c_str(), "re" );
        if( !nm ) {
            out << "    backtrace: popen(nm) failed: " << strerror( errno ) << "\n";
            return std::nullopt;
        }

        char buf[1024];
        while( fgets( buf, sizeof( buf ), nm ) ) {
            std::string line( buf );
            while( !line.empty() && isspace( line.end()[-1] ) ) {
                line.erase( line.end() - 1 );
            }
            if( line.ends_with( string_sought ) ) {
                std::istringstream line_is( line );
                uintptr_t symbol_address;
                line_is >> std::hex >> symbol_address;
                if( line_is ) {
                    pclose( nm );
                    return reinterpret_cast<uintptr_t>( address ) -
                           ( symbol_address + offset_within_symbol );
                }
            }
        }

        pclose( nm );
    }

    return std::nullopt;
}
#endif

#if defined(LIBBACKTRACE)
// wrap libbacktrace to use std::function instead of function pointers
using bt_error_callback = std::function<void( const char *, int )>;
using bt_full_callback = std::function<int( uintptr_t, const char *, int, const char * )>;
using bt_syminfo_callback = std::function<void( uintptr_t, const char *, uintptr_t, uintptr_t )>;

static backtrace_state *bt_create_state( const char *const filename, const int threaded,
        const bt_error_callback &cb )
{
    return backtrace_create_state( filename, threaded,
    []( void *const data, const char *const msg, const int errnum ) {
        const bt_error_callback &cb = *reinterpret_cast<const bt_error_callback *>( data );
        cb( msg, errnum );
    },
    const_cast<bt_error_callback *>( &cb ) );
}


#if !defined(_WIN32)
static int bt_full( backtrace_state *const state, int skip, const bt_full_callback &cb_full,
                    const bt_error_callback &cb_error )
{
    using cb_pair = std::pair<const bt_full_callback &, const bt_error_callback &>;
    cb_pair cb { cb_full, cb_error };
    return backtrace_full( state, skip,
                           // backtrace callback
                           []( void *const data, const uintptr_t pc, const char *const filename,
    const int lineno, const char *const function ) -> int {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        return cb.first( pc, filename, lineno, function );
    },
    // error callback
    []( void *const data, const char *const msg, const int errnum ) {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        cb.second( msg, errnum );
    },
    &cb );
}
#else
static int bt_pcinfo( backtrace_state *const state, const uintptr_t pc,
                      const bt_full_callback &cb_full, const bt_error_callback &cb_error )
{
    using cb_pair = std::pair<const bt_full_callback &, const bt_error_callback &>;
    cb_pair cb { cb_full, cb_error };
    return backtrace_pcinfo( state, pc,
                             // backtrace callback
                             []( void *const data, const uintptr_t pc, const char *const filename,
    const int lineno, const char *const function ) -> int {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        return cb.first( pc, filename, lineno, function );
    },
    // error callback
    []( void *const data, const char *const msg, const int errnum ) {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        cb.second( msg, errnum );
    },
    &cb );
}

static int bt_syminfo( backtrace_state *const state, const uintptr_t addr,
                       const bt_syminfo_callback &cb_syminfo, const bt_error_callback cb_error )
{
    using cb_pair = std::pair<const bt_syminfo_callback &, const bt_error_callback &>;
    cb_pair cb { cb_syminfo, cb_error };
    return backtrace_syminfo( state, addr,
                              // syminfo callback
                              []( void *const data, const uintptr_t pc, const char *const symname,
    const uintptr_t symval, const uintptr_t symsize ) {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        cb.first( pc, symname, symval, symsize );
    },
    // error callback
    []( void *const data, const char *const msg, const int errnum ) {
        cb_pair &cb = *reinterpret_cast<cb_pair *>( data );
        cb.second( msg, errnum );
    },
    &cb );
}
#endif
#endif

#if defined(_WIN32)
class sym_init
{
    public:
        sym_init() {
            SymInitialize( GetCurrentProcess(), nullptr, TRUE );
        }

        ~sym_init() {
            SymCleanup( GetCurrentProcess() );
        }
};
static std::unique_ptr<sym_init> sym_init_;

constexpr int module_path_len = 512;
// on some systems the number of frames to capture have to be less than 63 according to the documentation
constexpr int bt_cnt = 62;
constexpr int max_name_len = 512;
// ( max_name_len - 1 ) because SYMBOL_INFO already contains a TCHAR
constexpr int sym_size = sizeof( SYMBOL_INFO ) + ( max_name_len - 1 ) * sizeof( TCHAR );
static char mod_path[module_path_len];
static PVOID bt[bt_cnt];
static struct {
    alignas( SYMBOL_INFO ) char storage[sym_size];
} sym_storage;
static SYMBOL_INFO &sym = reinterpret_cast<SYMBOL_INFO &>( sym_storage );
#if defined(LIBBACKTRACE)
struct backtrace_module_info_t {
    backtrace_state *state = nullptr;
    uintptr_t image_base = 0;
};
static std::map<DWORD64, backtrace_module_info_t> bt_module_info_map;
#endif
#elif !defined(__ANDROID__) && !defined(LIBBACKTRACE)
constexpr int bt_cnt = 20;
static void *bt[bt_cnt];
#endif

#if !defined(_WIN32) && !defined(__ANDROID__) && !defined(LIBBACKTRACE)
static void write_demangled_frame( std::ostream &out, const char *frame )
{
#if defined(__linux__)
    // ./cataclysm(_ZN4game13handle_actionEv+0x47e8) [0xaaaae91e80fc]
    static const std::regex symbol_regex( R"(^(.*)\((.*)\+(0x?[a-f0-9]*)\)\s\[(0x[a-f0-9]+)\]$)" );
    std::cmatch match_result;
    if( std::regex_search( frame, match_result, symbol_regex ) && match_result.size() == 5 ) {
        std::csub_match file_name = match_result[1];
        std::csub_match raw_symbol_name = match_result[2];
        std::csub_match offset = match_result[3];
        std::csub_match address = match_result[4];
        out << "\n    " << file_name.str() << "(" << demangle( raw_symbol_name.str().c_str() ) << "+" <<
            offset.str() << ") [" << address.str() << "]";
    } else {
        out << "\n    " << frame;
    }
#elif defined(MACOSX)
    //1   cataclysm-bn-tiles                     0x0000000102ba2244 _ZL9log_crashPKcS0_ + 608
    static const std::regex symbol_regex( R"(^(.*)(0x[a-f0-9]{16})\s(.*)\s\+\s([0-9]+)$)" );
    std::cmatch match_result;
    if( std::regex_search( frame, match_result, symbol_regex ) && match_result.size() == 5 ) {
        std::csub_match prefix = match_result[1];
        std::csub_match address = match_result[2];
        std::csub_match raw_symbol_name = match_result[3];
        std::csub_match offset = match_result[4];
        out << "\n    " << prefix.str() << address.str() << ' ' << demangle( raw_symbol_name.str().c_str() )
            << " + " << offset.str();
    } else {
        out << "\n    " << frame;
    }
#elif defined(BSD)
    static const std::regex symbol_regex( R"(^(0x[a-f0-9]+)\s<(.*)\+(0?x?[a-f0-9]*)>\sat\s(.*)$)" );
    std::cmatch match_result;
    if( std::regex_search( frame, match_result, symbol_regex ) && match_result.size() == 5 ) {
        std::csub_match address = match_result[1];
        std::csub_match raw_symbol_name = match_result[2];
        std::csub_match offset = match_result[3];
        std::csub_match file_name = match_result[4];
        out << "\n    " << address.str() << " <" << demangle( raw_symbol_name.str().c_str() ) << "+" <<
            offset.str() << "> at " << file_name.str();
    } else {
        out << "\n    " << frame;
    }
#else
    out << "\n    " << frame;
#endif
}
#endif // !defined(_WIN32) && !defined(__ANDROID__)


#if !defined(__ANDROID__)
void debug_write_backtrace( std::ostream &out )
{
#if defined(LIBBACKTRACE)
    auto bt_full_print = [&out]( const uintptr_t pc, const char *const filename,
    const int lineno, const char *const function ) -> int {
        std::string file = filename ? filename : "[unknown src]";
        size_t src = file.find( "/src/" );
        if( src != std::string::npos )
        {
            file.erase( 0, src );
            file = "…" + file;
        }
        out << "\n    0x" << std::hex << pc << std::dec
            << "    " << file << ":" << lineno
            << "    " << ( function ? demangle( function ) : "[unknown func]" );
        return 0;
    };
#endif

#if defined(_WIN32)
    if( !sym_init_ ) {
        sym_init_ = std::make_unique<sym_init>();
    }
    sym.SizeOfStruct = sizeof( SYMBOL_INFO );
    sym.MaxNameLen = max_name_len;
    // libbacktrace's own backtrace capturing doesn't seem to work on Windows
    const USHORT num_bt = CaptureStackBackTrace( 0, bt_cnt, bt, nullptr );
    const HANDLE proc = GetCurrentProcess();
    for( USHORT i = 0; i < num_bt; ++i ) {
        DWORD64 off;
        out << "\n  #" << i;
        out << "\n    (dbghelp: ";
        if( SymFromAddr( proc, reinterpret_cast<DWORD64>( bt[i] ), &off, &sym ) ) {
            out << demangle( sym.Name ) << "+0x" << std::hex << off << std::dec;
        }
        out << "@" << bt[i];
        const DWORD64 mod_base = SymGetModuleBase64( proc, reinterpret_cast<DWORD64>( bt[i] ) );
        if( mod_base ) {
            out << "[";
            const DWORD mod_len = GetModuleFileName( reinterpret_cast<HMODULE>( mod_base ), mod_path,
                                  module_path_len );
            // mod_len == module_path_len means insufficient buffer
            if( mod_len > 0 && mod_len < module_path_len ) {
                const char *mod_name = mod_path + mod_len;
                for( ; mod_name > mod_path && *( mod_name - 1 ) != '\\'; --mod_name ) {
                }
                out << mod_name;
            } else {
                out << "0x" << std::hex << mod_base << std::dec;
            }
            out << "+0x" << std::hex << reinterpret_cast<uintptr_t>( bt[i] ) - mod_base <<
                std::dec << "]";
        }
        out << "), ";
#if defined(LIBBACKTRACE)
        backtrace_module_info_t bt_module_info;
        if( mod_base ) {
            const auto it = bt_module_info_map.find( mod_base );
            if( it != bt_module_info_map.end() ) {
                bt_module_info = it->second;
            } else {
                const DWORD mod_len = GetModuleFileName( reinterpret_cast<HMODULE>( mod_base ), mod_path,
                                      module_path_len );
                if( mod_len > 0 && mod_len < module_path_len ) {
                    bt_module_info.state = bt_create_state( mod_path, 0,
                                                            // error callback
                    [&out]( const char *const msg, const int errnum ) {
                        out << "\n    (backtrace_create_state failed: errno = " << errnum
                            << ", msg = " << ( msg ? msg : "[no msg]" ) << "),";
                    } );
                    bt_module_info.image_base = get_image_base( mod_path );
                    if( bt_module_info.image_base == 0 ) {
                        out << "\n    (cannot locate image base),";
                    }
                } else {
                    out << "\n    (executable path exceeds " << module_path_len << " chars),";
                }
                bt_module_info_map.emplace( mod_base, bt_module_info );
            }
        } else {
            out << "\n    (unable to get module base address),";
        }
        if( bt_module_info.state && bt_module_info.image_base != 0 ) {
            const uintptr_t de_aslr_pc = reinterpret_cast<uintptr_t>( bt[i] ) - mod_base +
                                         bt_module_info.image_base;
            bt_syminfo( bt_module_info.state, de_aslr_pc,
                        // syminfo callback
                        [&out]( const uintptr_t pc, const char *const symname,
            const uintptr_t symval, const uintptr_t ) {
                out << "\n    (libbacktrace: " << ( symname ? symname : "[unknown symbol]" )
                    << "+0x" << std::hex << pc - symval << std::dec
                    << "@0x" << std::hex << pc << std::dec
                    << "),";
            },
            // error callback
            [&out]( const char *const msg, const int errnum ) {
                out << "\n    (backtrace_syminfo failed: errno = " << errnum
                    << ", msg = " << ( msg ? msg : "[no msg]" )
                    << "),";
            } );
            bt_pcinfo( bt_module_info.state, de_aslr_pc, bt_full_print,
                       // error callback
            [&out]( const char *const msg, const int errnum ) {
                out << "\n    (backtrace_pcinfo failed: errno = " << errnum
                    << ", msg = " << ( msg ? msg : "[no msg]" )
                    << "),";
            } );
        }
#endif
    }
    out << "\n";
#else
#   if defined(LIBBACKTRACE)
    auto bt_error = [&out]( const char *err_msg, int errnum ) {
        out << "\n    libbacktrace error " << errnum << ": " << err_msg;
    };
    static backtrace_state *bt_state = bt_create_state( nullptr, 0, bt_error );
    if( bt_state ) {
        bt_full( bt_state, 0, bt_full_print, bt_error );
        out << '\n';
    } else {
        out << "\n\n    Failed to initialize libbacktrace\n";
    }
#   elif defined(__CYGWIN__)
    // BACKTRACE is not supported under CYGWIN!
    ( void ) out;
#   else
    int count = backtrace( bt, bt_cnt );
    char **funcNames = backtrace_symbols( bt, count );
    for( int i = 0; i < count; ++i ) {
        write_demangled_frame( out, funcNames[i] );
    }
    out << "\n\n    Attempting to repeat stack trace using debug symbols…\n";
    // Try to print the backtrace again, but this time using addr2line
    // to extract debug info and thus get a more detailed / useful
    // version.  If addr2line is not available this will just fail,
    // which is fine.
    //
    // To make this fast, we need to call addr2line with as many
    // addresses as possible in each commandline.  To that end, we track
    // the binary of the frame and issue a command whenever that
    // changes.
    std::vector<uintptr_t> addresses;
    std::map<std::string, uintptr_t> load_offsets;
    std::string last_binary_name;

    auto call_addr2line = [&out, &load_offsets]( const std::string & binary,
    const std::vector<uintptr_t> &addresses ) {
        const auto load_offset_it = load_offsets.find( binary );
        const uintptr_t load_offset = ( load_offset_it == load_offsets.end() ) ? 0 :
                                      load_offset_it->second;

        std::ostringstream cmd;
        cmd.imbue( std::locale::classic() );
        cmd << "addr2line -i -e " << binary << " -f -C" << std::hex;
        for( uintptr_t address : addresses ) {
            cmd << " 0x" << ( address - load_offset );
        }
        cmd << " 2>&1";
        FILE *addr2line = popen( cmd.str().c_str(), "re" );
        if( addr2line == nullptr ) {
            out << "    backtrace: popen(addr2line) failed\n";
            return false;
        }
        char buf[1024];
        while( fgets( buf, sizeof( buf ), addr2line ) ) {
            out.write( "    ", 4 );
            // Strip leading directories for source file path
            char search_for[] = "/src/";
            auto buf_end = buf + strlen( buf );
            auto src = std::find_end( buf, buf_end,
                                      search_for, search_for + strlen( search_for ) );
            if( src == buf_end ) {
                src = buf;
            } else {
                out << "…";
            }
            out.write( src, strlen( src ) );
        }
        if( 0 != pclose( addr2line ) ) {
            // Most likely reason is that addr2line is not installed, so
            // in this case we give up and don't try any more frames.
            out << "    backtrace: addr2line failed\n";
            return false;
        }
        return true;
    };

    for( int i = 0; i < count; ++i ) {
        // An example string from backtrace_symbols is
        //   ./cataclysm-bn-tiles(_Z21debug_write_backtraceRSo+0x3d) [0x55ddebfa313d]
        // From that we need to extract the binary name, the symbol
        // name, and the offset within the symbol.  We don't need to
        // extract the address (the last thing) because that's already
        // available in bt.

        auto funcName = funcNames[i];
        assert( funcName ); // To appease static analysis
        const auto funcNameEnd = funcName + std::strlen( funcName );
        const auto binaryEnd = std::find( funcName, funcNameEnd, '(' );
        if( binaryEnd == funcNameEnd ) {
            out << "    backtrace: Could not extract binary name from line\n";
            continue;
        }

        if( !debug_is_safe_string( funcName, binaryEnd ) ) {
            out << "    backtrace: Binary name not safe\n";
            continue;
        }

        std::string binary_name( funcName, binaryEnd );
        binary_name = debug_resolve_binary( binary_name, out );

        // For each binary we need to determine its offset relative to
        // its natural load address in order to undo ASLR and pass the
        // correct addresses to addr2line
        auto load_offset = load_offsets.find( binary_name );
        if( load_offset == load_offsets.end() ) {
            const auto symbolNameStart = binaryEnd + 1;
            const auto symbolNameEnd = std::find( symbolNameStart, funcNameEnd, '+' );
            const auto offsetEnd = std::find( symbolNameStart, funcNameEnd, ')' );

            if( symbolNameEnd < offsetEnd && offsetEnd < funcNameEnd ) {
                const auto offsetStart = symbolNameEnd + 1;
                std::string symbol_name( symbolNameStart, symbolNameEnd );
                std::string offset_within_symbol( offsetStart, offsetEnd );

                std::optional<uintptr_t> offset =
                    debug_compute_load_offset( binary_name, symbol_name, offset_within_symbol,
                                               bt[i], out );
                if( offset ) {
                    load_offsets.emplace( binary_name, *offset );
                }
            }
        }

        if( !last_binary_name.empty() && binary_name != last_binary_name ) {
            // We have reached the end of the sequence of addresses
            // within this binary, so call addr2line before proceeding
            // to the next binary.
            if( !call_addr2line( last_binary_name, addresses ) ) {
                addresses.clear();
                break;
            }

            addresses.clear();
        }

        last_binary_name = binary_name;
        addresses.push_back( reinterpret_cast<uintptr_t>( bt[i] ) );
    }

    if( !addresses.empty() ) {
        call_addr2line( last_binary_name, addresses );
    }
    free( funcNames );
#   endif
#endif
}
#endif
#endif

// Probably because there are too many nested #if..#else..#endif in this file
// NDK compiler doesn't understand #if defined(__ANDROID__)..#else..#endif
// So write as two separate #if blocks
#if defined(__ANDROID__)

// The following Android backtrace code was originally written by Eugene Shapovalov
// on https://stackoverflow.com/questions/8115192/android-ndk-getting-the-backtrace
struct android_backtrace_state {
    void **current;
    void **end;
};

static _Unwind_Reason_Code unwindCallback( struct _Unwind_Context *context, void *arg )
{
    android_backtrace_state *state = static_cast<android_backtrace_state *>( arg );
    uintptr_t pc = _Unwind_GetIP( context );
    if( pc ) {
        if( state->current == state->end ) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void *>( pc );
        }
    }
    return _URC_NO_REASON;
}

void debug_write_backtrace( std::ostream &out )
{
    const size_t max = 50;
    void *buffer[max];
    android_backtrace_state state = {buffer, buffer + max};
    _Unwind_Backtrace( unwindCallback, &state );
    const std::size_t count = state.current - buffer;
    // Start from 1: skip debug_write_backtrace ourselves
    for( size_t idx = 1; idx < count && idx < max; ++idx ) {
        const void *addr = buffer[idx];
        Dl_info info;
        if( dladdr( addr, &info ) && info.dli_sname ) {
            out << "#" << std::setw( 2 ) << idx << ": " << addr << " " << demangle( info.dli_sname ) << "\n";
        }
    }
}
#endif

void output_repetitions( std::ostream &out )
{
    // Need to complete the folding
    if( rep_folder.repeat_count > 0 ) {
        if( rep_folder.repeat_count > 1 ) {
            out << "[ Previous repeated " << ( rep_folder.repeat_count - 1 ) << " times ]";
        }
        out << '\n';
        out << rep_folder.m_time << " ";
        // repetition folding is only done through realDebugmsg
        out << io::enum_to_string<DL>( DL::Error ) << " ";
        out << io::enum_to_string<DC>( DC::DebugMsg ) << " ";
        out << ": ";
        out << rep_folder.m_filename << ":" << rep_folder.m_line << " [" << rep_folder.m_funcname << "] " <<
            rep_folder.m_text << '\n';
        rep_folder.reset();
    }
}

detail::DebugLogGuard::~DebugLogGuard()
{
    *s << '\n';
}

detail::DebugLogGuard detail::realDebugLog( DL lev, DC cl, const char *filename,
        const char *line, const char *funcname )
{
    if( lev == DL::Error ) {
        error_observed = true;
    }

    if( checkDebugLevelClass( lev, cl ) ) {
        std::ostream &out = debugFile().get_file();

        output_repetitions( out );

        out << get_time() << " ";
        out << io::enum_to_string<DL>( lev ) << " ";
        if( cl != DC::Main ) {
            out << io::enum_to_string<DC>( cl ) << " ";
        }
        out << ": ";
        if( filename ) {
            out << filename;
            if( line ) {
                out << ":" << line;
            }
            if( funcname ) {
                out << " ";
            } else {
                out << ": ";
            }
        }
        if( funcname ) {
            out << "[" << funcname << "] ";
        }

        // Backtrace on error.
#if defined(BACKTRACE)
        // Push the first retrieved value back by a second so it won't match.
        static time_t next_backtrace = time( nullptr ) - 1;
        time_t now = time( nullptr );
        if( lev == DL::Error && now >= next_backtrace ) {
            out << "(error message will follow backtrace)";
            debug_write_backtrace( out );
            time_t after = time( nullptr );
            // Cool down for 60s between backtrace emissions.
            next_backtrace = after + 60;
            out << "Backtrace emission took " << after - now << " seconds." << '\n';
            cata::debug_write_lua_backtrace( out );
            out << "(continued from above) " << io::enum_to_string( lev ) << ": ";
        }
#endif

        return DebugLogGuard( out );
    }

    static NullStream null_stream;
    return DebugLogGuard( null_stream );
}
