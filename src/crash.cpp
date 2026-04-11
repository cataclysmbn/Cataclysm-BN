#include "crash.h"
#include "sdl_wrappers.h"

#if defined(BACKTRACE)

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>

#if defined(_WIN32)
#if 1 // HACK: Hack to prevent reordering of #include "platform_win.h" by IWYU
#include "platform_win.h"
#endif
#include <dbghelp.h>
#endif

#include "debug.h"
#include "get_version.h"
#include "path_info.h"

// signal handlers are expected to have C linkage, and only use the
// common subset of C & C++

#if defined(_WIN32)
// Ensures only the first crash path (exception filter or signal handler) logs and dumps.
static std::atomic_flag g_crash_logged{};
// Set by windows_exception_filter; used by dump_to() and debug_write_backtrace().
static EXCEPTION_POINTERS *g_exception_info = nullptr;
#endif

extern "C" {

#if defined(_WIN32)
    static void dump_to( const char *file )
    {
        HANDLE handle = CreateFile( file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr );
        MINIDUMP_EXCEPTION_INFORMATION mdei = {};
        MINIDUMP_EXCEPTION_INFORMATION *mdei_ptr = nullptr;
        if( g_exception_info ) {
            mdei.ThreadId          = GetCurrentThreadId();
            mdei.ExceptionPointers = g_exception_info;
            mdei.ClientPointers    = FALSE;
            mdei_ptr               = &mdei;
        }
        MiniDumpWriteDump( GetCurrentProcess(),
                           GetCurrentProcessId(),
                           handle,
                           static_cast<MINIDUMP_TYPE>( MiniDumpNormal | MiniDumpWithUnloadedModules ),
                           mdei_ptr, nullptr, nullptr );
        CloseHandle( handle );
    }
#endif

    static void log_crash( const char *type, const char *msg )
    {
        // This implementation is not technically async-signal-safe for many
        // reasons, including the memory allocations and the SDL message box.
        // But it should usually work in practice, unless for example the
        // program segfaults inside malloc.

        // Flush the debug log before writing the crash report so that any
        // messages written just before the crash are on disk.
        flush_debug_log();

        const std::string crash_log_file = PATH_INFO::crash();
#if defined(_WIN32)
        const std::string minidump_file = crash_log_file + ".dmp";
        dump_to( minidump_file.c_str() );
#endif
        std::ostringstream log_text;
#if defined(__ANDROID__)
        // At this point, Android JVM is already doomed
        // No further UI interaction (including the SDL message box)
        // Show a dialogue at next launch
        log_text << "VERSION: " << getVersionString()
                 << '\n' << type << ' ' << msg;
#else
        log_text << "The program has crashed."
                 << "\nSee the log file for a stack trace."
                 << "\nCRASH LOG FILE: " << crash_log_file
#if defined(_WIN32)
                 << "\nMINIDUMP FILE:  " << minidump_file
                 << "\n(Attach both files when reporting this crash)"
#endif
                 << "\nVERSION: " << getVersionString()
                 << "\nTYPE: " << type
                 << "\nMESSAGE: " << msg;
#if defined(TILES)
        if( SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "Error",
                                      log_text.str().c_str(), nullptr ) != 0 ) {
            log_text << "Error creating SDL message box: " << SDL_GetError() << '\n';
        }
#endif
#endif
        log_text << "\nSTACK TRACE:\n";
        debug_write_backtrace( log_text );
        std::cerr << log_text.str();
        FILE *file = fopen( crash_log_file.c_str(), "w" );
        if( file ) {
            fwrite( log_text.str().data(), 1, log_text.str().size(), file );
            fclose( file );
        }
#if defined(__ANDROID__)
        // Create a placeholder dummy file "config/crash.log.prompt"
        // to let the app show a dialog box at next start
        file = fopen( ( crash_log_file + ".prompt" ).c_str(), "w" );
        if( file ) {
            fwrite( "0", 1, 1, file );
            fclose( file );
        }
#endif
    }

    static void signal_handler( int sig )
    {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
        signal( sig, SIG_DFL );
#pragma GCC diagnostic pop
#if defined(_WIN32)
        if( g_crash_logged.test_and_set() ) {
            raise( sig );
            return;
        }
#endif
        const char *msg;
        switch( sig ) {
            case SIGSEGV:
                msg = "SIGSEGV: Segmentation fault";
                break;
            case SIGILL:
                msg = "SIGILL: Illegal instruction";
                break;
            case SIGABRT:
                msg = "SIGABRT: Abnormal termination";
                break;
            case SIGFPE:
                msg = "SIGFPE: Arithmetical error";
                break;
            default:
                return;
        }
        log_crash( "Signal", msg );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::signal( SIGABRT, SIG_DFL );
#pragma GCC diagnostic pop
        abort();
    }
} // extern "C"

#if defined(_WIN32)
static LONG WINAPI windows_exception_filter( EXCEPTION_POINTERS *exception_info )
{
    if( g_crash_logged.test_and_set() ) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    g_exception_info = exception_info;
    set_crash_exception_context( exception_info ? exception_info->ContextRecord : nullptr );
    const char *msg = "Unknown exception";
    if( exception_info && exception_info->ExceptionRecord ) {
        switch( exception_info->ExceptionRecord->ExceptionCode ) {
            case EXCEPTION_ACCESS_VIOLATION:
                msg = "EXCEPTION_ACCESS_VIOLATION: Access violation";
                break;
            case EXCEPTION_ILLEGAL_INSTRUCTION:
                msg = "EXCEPTION_ILLEGAL_INSTRUCTION: Illegal instruction";
                break;
            case EXCEPTION_STACK_OVERFLOW:
                msg = "EXCEPTION_STACK_OVERFLOW: Stack overflow";
                break;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
                msg = "EXCEPTION_INT_DIVIDE_BY_ZERO: Integer divide by zero";
                break;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:
                msg = "EXCEPTION_FLT_DIVIDE_BY_ZERO: Float divide by zero";
                break;
            default:
                break;
        }
    }
    log_crash( "Signal", msg );
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

[[noreturn]] static void crash_terminate_handler()
{
    // TODO: thread-safety?
    const char *type;
    const char *msg;
    try {
        auto &&ex = std::current_exception(); // *NOPAD*
        if( ex ) {
            std::rethrow_exception( ex );
        } else {
            type = msg = "Unexpected termination";
        }
    } catch( const std::exception &e ) {
        type = typeid( e ).name();
        msg = e.what();
        // call here to avoid `msg = e.what()` going out of scope
        log_crash( type, msg );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
        std::signal( SIGABRT, SIG_DFL );
#pragma GCC diagnostic pop
        abort();
    } catch( ... ) {
        type = "Unknown exception";
        msg = "Not derived from std::exception";
    }
    log_crash( type, msg );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wzero-as-null-pointer-constant"
    std::signal( SIGABRT, SIG_DFL );
#pragma GCC diagnostic pop
    abort();
}

void init_crash_handlers()
{
    for( auto sig : {
             SIGSEGV, SIGILL, SIGABRT, SIGFPE
         } ) {

        std::signal( sig, signal_handler );
    }
    std::set_terminate( crash_terminate_handler );
#if defined(_WIN32)
    // Registered last so any SetUnhandledExceptionFilter call made internally
    // by the CRT's signal() machinery does not overwrite ours.
    SetUnhandledExceptionFilter( windows_exception_filter );
#endif
}

#else // !BACKTRACE

void init_crash_handlers()
{
}

#endif
