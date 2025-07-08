#pragma once

/**
 *      debugmsg(msg, ...)
 * varg-style functions: the first argument is the format (see printf),
 * the other optional arguments must match that format.
 * The message is formatted and printed on screen to player, they must
 * acknowledge the message by pressing SPACE. This can be quite annoying, so
 * avoid this function if possible.
 * The message is also logged with DebugLog, a separate call to DebugLog
 * is not needed.
 *
 *      DebugLog(lev, cl)  and  DebugLogFL(lev, cl)
 * The main debugging system. It uses debug levels (how critical/important
 * the message is) and debug classes (which part of the code it comes from,
 * e.g. mapgen, SDL, npcs). This allows disabling debug classes and levels
 * (e.g. only write SDL messages, but no npc or map related ones).
 * It returns a reference to an output stream. Simply write your message into
 * that stream (using the << operators).
 * DebugLog always returns a stream that starts on a new line. Don't add a
 * newline at the end of your debug message, as it is automatically appended
 * once the reference goes out of scope.
 * If the specific debug level or class have been disabled, the message is
 * actually discarded, otherwise it is written to a log file.
 * Errors are logged regardless of whether debug class is enabled.
 * DebugLogFL version also logs source file name and line number.
 * If a single source file contains mostly messages for the same debug class
 * (e.g. mapgen.cpp), create and use the macro dbg.
 *
 *      dbg(lev)
 * Usually a single source contains only debug messages for a single debug class
 * (e.g. mapgen.cpp contains only messages for DC::MapGen, npcmove.cpp only DC::NPC).
 * Those files contain a macro at top:
@code
#define dbg(x) DebugLogFL((x), DC::NPC)
@endcode
 * It allows to call the debug system and just supply the debug level, the debug
 * class is automatically inserted as it is the same for the whole file. Also this
 * adds the file name and the line of the statement to the debug message.
 * This can be replicated in any source file, just copy the above macro and change
 * DC::NPC to the debug class to use. Don't add this macro to a header file
 * as the debug class is source file specific.
 * As dbg calls DebugLog, it returns the stream reference, its usage is the same.
 */

// Includes                                                         {{{1
// ---------------------------------------------------------------------
#include <iostream>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <functional>

#include "string_formatter.h"
template<typename T> struct enum_traits;
template<typename E> class enum_bitset;

#define STRING2(x) #x
#define STRING(x) STRING2(x)

#if defined(__GNUC__)
#define __FUNCTION_NAME__ __PRETTY_FUNCTION__
#else
#define __FUNCTION_NAME__ __func__
#endif


// Enumerations                                                     {{{1
// ---------------------------------------------------------------------

/** Debug level */
enum class DL : int {
    /**
     * Debug information (default: disabled).
     */
    Debug,
    /**
     * Information (default: enabled).
     */
    Info,
    /**
     * Warning (default: enabled).
     */
    Warn,
    /**
     * Error (default: enabled).
     * Errors are logged regardless of whether debug class is enabled
     * and have backtrace appended to them.
     */
    Error,
    /**
     * Unused
     */
    Num,
};

template <>
struct enum_traits<DL> {
    static constexpr DL last = DL::Num;
};

/** Debug class */
enum class DC : int {
    /** (internal) Messages from realDebugmsg */
    DebugMsg,
    /** (internal) Debug-type messages from message log */
    DebugModeMsg,
    /** Main game class */
    Game,
    /**
     * Generic messages related to game startup and operation.
     * Enabled by default.
     */
    Main,
    /** Related to map and mapbuffer (map.cpp, mapbuffer.cpp) */
    Map,
    /** Mapgen (mapgen*.cpp), also overmap.cpp */
    MapGen,
    /** Related to tile memory (map_memory.cpp) */
    MapMem,
    /** npcs*.cpp */
    NPC,
    /** SDL & tiles & anything graphical & sound */
    SDL,
    /** Output from Lua */
    Lua,
    /** Unused */
    Num,
};

template <>
struct enum_traits<DC> {
    static constexpr DC last = DC::Num;
};

enum class DebugOutput {
    std_err,
    file,
};

#define debugmsg_of(debug_level, ...) realDebugmsg(__FILE__, STRING(__LINE__), __FUNCTION_NAME__, debug_level, __VA_ARGS__)

/**
 * Debug message of level DL::Error and class DC::DebugMsg, also includes the source
 * file name and line, uses varg style arguments, the first argument must be
 * a printf style format string.
 */
#define debugmsg(...) debugmsg_of(DL::Error, __VA_ARGS__)
// Don't use this, use debugmsg instead.
void realDebugmsg( const char *filename, const char *line, const char *funcname,
                   const DL debug_level, const std::string &text );
template<typename ...Args>
inline void realDebugmsg( const char *const filename, const char *const line,
                          const char *const funcname, const DL debug_level, const char *const mes, Args &&... args )
{
    return realDebugmsg( filename, line, funcname, debug_level, string_format( mes,
                         std::forward<Args>( args )... ) );
}

// A fatal error for use in constexpr functions
// This exists for compatibility reasons.  On gcc 5.3 we need a
// different implementation that is messier.
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=67371
// Pass a placeholder return value to be used on gcc 5.3 (it won't
// actually be returned, it's just needed for the type), and then
// args as if to debugmsg for the remaining args.
#if defined(__GNUC__) && __GNUC__ < 6
#define constexpr_fatal(ret, ...) \
    do { return false ? ( ret ) : ( abort(), ( ret ) ); } while(false)
#else
#define constexpr_fatal(ret, ...) \
    do { debugmsg(__VA_ARGS__); abort(); } while(false)
#endif

/** Initializes the debugging system, called exactly once from main() */
void setupDebug( DebugOutput );
/** Opposite of setupDebug, shuts the debugging system down. */
void deinitDebug();

// Function Declarations                                            {{{1
// ---------------------------------------------------------------------
/**
 * Set debug levels that should be logged.
 */
void setDebugLogLevels( const enum_bitset<DL> &mask, bool silent = false );

/**
 * Set debug classes that should be logged.
 */
void setDebugLogClasses( const enum_bitset<DC> &mask, bool silent = false );

/**
 * @return true if any error has been logged in this run.
 */
bool debug_has_error_been_observed();

/**
 * Reset any ignored debug messages
 */
void debug_reset_ignored_messages();

/**
 * Capturing debug messages during func execution,
 * used to test debugmsg calls in the unit tests
 * @return std::string debugmsg
 */
std::string capture_debugmsg_during( const std::function<void()> &func );

/**
 * Should be called after catacurses::stdscr is initialized.
 * If catacurses::stdscr is available, shows all buffered debugmsg prompts.
 */
void replay_buffered_debugmsg_prompts();

/**
 * Retrieves OS bitness (32, 64 or nullopt if detection failed)
 */
std::optional<int> get_os_bitness();

// Debug Only                                                       {{{1
// ---------------------------------------------------------------------

namespace detail
{
/**
 * Debug log guard.
 *
 * Appends newline and flushes the log when goes out of scope.
 * Dereference to get the underlying stream.
 */
class DebugLogGuard
{
        std::ostream *s;
    public:
        explicit DebugLogGuard( std::ostream &s ) : s( &s ) {}
        ~DebugLogGuard();

        std::ostream &operator*() {
            return *s;
        }
        const std::ostream &operator*() const {
            return *s;
        }
};

DebugLogGuard realDebugLog( DL lev, DC cl, const char *filename,
                            const char *line, const char *funcname );
} // namespace detail

/**
 * DebugLog. See documentation at the top of the file.
 */
#define DebugLog(lev, cl) *detail::realDebugLog(lev, cl, nullptr, nullptr, nullptr)

/**
 * Like DebugLog, but includes source file name and line number.
 */
#define DebugLogFL(lev, cl) *detail::realDebugLog(lev, cl, __FILE__, STRING(__LINE__), nullptr)

#if defined(BACKTRACE)
/**
 * Write a stack backtrace to the given ostream
 */
void debug_write_backtrace( std::ostream &out );
#endif

// vim:tw=72:sw=4:fdm=marker:fdl=0:

