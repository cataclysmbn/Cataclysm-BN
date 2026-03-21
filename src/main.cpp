/* Entry point and main loop for Cataclysm
 */

#include <array>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <filesystem>
#include <vector>
#if defined(_WIN32)
#   include "platform_win.h"
#else
#   include <csignal>
#endif
#include "catalua.h"
#include "color.h"
#include "crash.h"
#include "cursesdef.h"
#include "debug.h"
#include "filesystem.h"
#include "game.h"
#include "game_ui.h"
#include "init.h"
#include "input.h"
#include "language.h"
#include "loading_ui.h"
#include "runtime_handlers.h"
#include "string_formatter.h"
#include "main_menu.h"
#include "mapsharing.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "rng.h"
#include "type_id.h"
#include "ui_manager.h"
#include "path_display.h"
#include "get_version.h"

#if defined(PREFIX)
#   undef PREFIX
#   include "prefix.h"
#endif

class ui_adaptor;

#if defined(TILES)
#   define SDL_MAIN_HANDLED
#   include "sdl_wrappers.h"
#   if defined(_MSC_VER) && defined(USE_VCPKG)
#      include <SDL2/SDL_version.h>
#   else
#      include <SDL_version.h>
#   endif
#endif

#if defined(__ANDROID__)
#include <SDL_filesystem.h>
#include <SDL_keyboard.h>
#include <SDL_system.h>
#include <android/log.h>
#include <unistd.h>

// Taken from: https://codelab.wordpress.com/2014/11/03/how-to-use-standard-output-streams-for-logging-in-android-apps/
// Force Android standard output to adb logcat output

static int pfd[2];
static pthread_t thr;
static const char *tag = "cdda";

static void *thread_func( void * )
{
    ssize_t rdsz;
    char buf[128];
    for( ;; ) {
        if( ( ( rdsz = read( pfd[0], buf, sizeof buf - 1 ) ) > 0 ) ) {
            if( buf[rdsz - 1] == '\n' ) {
                --rdsz;
            }
            buf[rdsz] = 0;  /* add null-terminator */
            __android_log_write( ANDROID_LOG_DEBUG, tag, buf );
        }
    }
    return nullptr;
}

int start_logger( const char *app_name )
{
    tag = app_name;

    /* make stdout line-buffered and stderr unbuffered */
    setvbuf( stdout, nullptr, _IOLBF, 0 );
    setvbuf( stderr, nullptr, _IONBF, 0 );

    /* create the pipe and redirect stdout and stderr */
    pipe( pfd );
    dup2( pfd[1], 1 );
    dup2( pfd[1], 2 );

    /* spawn the logging thread */
    if( pthread_create( &thr, nullptr, thread_func, nullptr ) == -1 ) {
        return -1;
    }
    pthread_detach( thr );
    return 0;
}

#endif //__ANDROID__

#if !defined(_WIN32)
#if defined(TILES)
[[ noreturn ]]
static void signal_handler( int )
{
    exit_handler( 0 );
}
#else
static void signal_handler( int signal )
{
    if( signal == SIGINT ) {
        const int old_timeout = inp_mngr.get_timeout();
        inp_mngr.reset_timeout();
        bool confirmed = query_yn( _( "Really Quit?  All unsaved changes will be lost." ) );
        inp_mngr.set_timeout( old_timeout );
        ui_manager::redraw_invalidated();
        catacurses::doupdate();
        if( !confirmed ) {
            return;
        }
    }
    exit_handler( 0 );
}
#endif //defined(TILES)
#endif //!defined(_WIN32)

/**
 * Report fatal error in a user-friendly way
 * (stderr or a message box, depending on build.)
 */
static void report_fatal_error( const std::string &msg )
{
#if defined(TILES)
    if( test_mode ) {
#endif
        std::cerr << "Cataclysm BN: Fatal error" << '\n' << msg << '\n';
#if defined(TILES)
    } else {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Cataclysm BN: Fatal error",
            msg.c_str(),
            nullptr
        );
    }
#endif
}

namespace
{

struct arg_handler {
    //! Handler function to be invoked when this argument is encountered. The handler will be
    //! called with the number of parameters after the flag was encountered, along with the array
    //! of following parameters. It must return an integer indicating how many parameters were
    //! consumed by the call or -1 to indicate that a required argument was missing.
    using handler_method = std::function<int ( int, const char ** )>;

    const char *flag;  //!< The commandline parameter to handle (e.g., "--seed").
    const char *param_documentation;  //!< Human readable description of this arguments parameter.
    const char *documentation;  //!< Human readable documentation for this argument.
    const char *help_group; //!< Section of the help message in which to include this argument.
    handler_method handler;  //!< The callback to be invoked when this argument is encountered.
};

void printHelpMessage( const arg_handler *first_pass_arguments, size_t num_first_pass_arguments,
                       const arg_handler *second_pass_arguments, size_t num_second_pass_arguments );
}  // namespace

#if defined(USE_WINMAIN)
int APIENTRY WinMain( HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */,
                      LPSTR /* lpCmdLine */, int /* nCmdShow */ )
{
    int argc = __argc;
    char **argv = __argv;
#elif defined(__ANDROID__)
extern "C" int SDL_main( int argc, char **argv ) {
#else
int main( int argc, char *argv[] )
{
#endif
    init_crash_handlers();
    int seed = time( nullptr );
    bool verifyexit = false;
    bool check_mods = false;
    std::filesystem::path lua_doc_output_path;
    std::filesystem::path lua_types_output_path;
    std::string dump;
    dump_mode dmode = dump_mode::TSV;
    std::vector<std::string> opts;
    std::string world; /** if set try to load first save in this world on startup */

#if defined(__ANDROID__)
    // Start the standard output logging redirector
    start_logger( "cdda" );

    // On Android first launch, we copy all data files from the APK into the app's writeable folder so std::io stuff works.
    // Use the external storage so it's publicly modifiable data (so users can mess with installed data, save games etc.)
    std::string external_storage_path( SDL_AndroidGetExternalStoragePath() );

    PATH_INFO::init_base_path( external_storage_path );
#else
    // Set default file paths
#if defined(PREFIX)
    PATH_INFO::init_base_path( std::string( PREFIX ) );
#else
    PATH_INFO::init_base_path( "" );
#endif
#endif

#if defined(__ANDROID__)
    PATH_INFO::init_user_dir( external_storage_path );
#else
#   if defined(USE_HOME_DIR) || defined(USE_XDG_DIR)
    PATH_INFO::init_user_dir( "" );
#   else
    PATH_INFO::init_user_dir( "." );
#   endif
#endif
    PATH_INFO::set_standard_filenames();

    MAP_SHARING::setDefaults();
    {
        const char *section_default = nullptr;
        const char *section_map_sharing = "Map sharing";
        const char *section_user_directory = "User directories";
        const std::array<arg_handler, 15> first_pass_arguments = {{
                {
                    "--seed", "<string of letters and or numbers>",
                    "Sets the random number generator's seed value",
                    section_default,
                    [&seed]( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        const unsigned char *hash_input = reinterpret_cast<const unsigned char *>( params[0] );
                        seed = djb2_hash( hash_input );
                        return 1;
                    }
                },
                {
                    "--jsonverify", nullptr,
                    "Checks the BN json files",
                    section_default,
                    [&verifyexit]( int, const char ** ) -> int {
                        verifyexit = true;
                        return 0;
                    }
                },
                {
                    "--check-mods", "[mods…]",
                    "Checks the json files belonging to BN mods",
                    section_default,
                    [&check_mods, &opts]( int n, const char *params[] ) -> int {
                        check_mods = true;
                        test_mode = true;
                        for( int i = 0; i < n; ++i )
                        {
                            opts.emplace_back( params[ i ] );
                        }
                        return 0;
                    }
                },
                {
                    "--dump-stats", "<what> [mode = TSV] [opts…]",
                    "Dumps item stats",
                    section_default,
                    [&dump, &dmode, &opts]( int n, const char *params[] ) -> int {
                        if( n < 1 )
                        {
                            return -1;
                        }
                        test_mode = true;
                        dump = params[ 0 ];
                        for( int i = 2; i < n; ++i )
                        {
                            opts.emplace_back( params[ i ] );
                        }
                        if( n >= 2 )
                        {
                            if( !strcmp( params[ 1 ], "TSV" ) ) {
                                dmode = dump_mode::TSV;
                                return 0;
                            } else if( !strcmp( params[ 1 ], "HTML" ) ) {
                                dmode = dump_mode::HTML;
                                return 0;
                            } else {
                                return -1;
                            }
                        }
                        return 0;
                    }
                },
                {
                    "--world", "<name>",
                    "Load world",
                    section_default,
                    [&world]( int n, const char *params[] ) -> int {
                        if( n < 1 )
                        {
                            return -1;
                        }
                        world = params[0];
                        return 1;
                    }
                },
                {
                    "--basepath", "<path>",
                    "Base path for all game data subdirectories",
                    section_default,
                    []( int num_args, const char **params )
                    {
                        if( num_args < 1 ) {
                            return -1;
                        }
                        PATH_INFO::init_base_path( params[0] );
                        PATH_INFO::set_standard_filenames();
                        return 1;
                    }
                },
                {
                    "--shared", nullptr,
                    "Activates the map-sharing mode",
                    section_map_sharing,
                    []( int, const char ** ) -> int {
                        MAP_SHARING::setSharing( true );
                        MAP_SHARING::setCompetitive( true );
                        MAP_SHARING::setWorldmenu( false );
                        return 0;
                    }
                },
                {
                    "--username", "<name>",
                    "Instructs map-sharing code to use this name for your character.",
                    section_map_sharing,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        MAP_SHARING::setUsername( params[0] );
                        return 1;
                    }
                },
                {
                    "--addadmin", "<username>",
                    "Instructs map-sharing code to use this name for your character and give you "
                    "access to the cheat functions.",
                    section_map_sharing,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        MAP_SHARING::addAdmin( params[0] );
                        return 1;
                    }
                },
                {
                    "--adddebugger", "<username>",
                    "Informs map-sharing code that you're running inside a debugger",
                    section_map_sharing,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        MAP_SHARING::addDebugger( params[0] );
                        return 1;
                    }
                },
                {
                    "--competitive", nullptr,
                    "Instructs map-sharing code to disable access to the in-game cheat functions",
                    section_map_sharing,
                    []( int, const char ** ) -> int {
                        MAP_SHARING::setCompetitive( true );
                        return 0;
                    }
                },
                {
                    "--userdir", "<path>",
                    // NOLINTNEXTLINE(cata-text-style): the dot is not a period
                    "Base path for user-overrides to files from the ./data directory and named below",
                    section_user_directory,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::init_user_dir( params[0] );
                        PATH_INFO::set_standard_filenames();
                        return 1;
                    }
                },
                {
                    "--dont-debugmsg", nullptr,
                    "If set, no debug messages will be printed",
                    section_default,
                    []( int, const char ** ) -> int {
                        dont_debugmsg = true;
                        return 0;
                    }
                },
                {
                    "--lua-doc", "<output path>",
                    "Generate Lua docs to given path and exit",
                    section_default,
                    [&]( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        test_mode = true;
                        lua_doc_output_path = params[0];
                        return 0;
                    }
                },
                {
                    "--lua-types", "<output path>",
                    "Generate Lua types to given path and exit",
                    section_default,
                    [&]( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        test_mode = true;
                        lua_types_output_path = params[0];
                        return 0;
                    }
                }
            }
        };

        // The following arguments are dependent on one or more of the previous flags and are run
        // in a second pass.
        const std::array<arg_handler, 8> second_pass_arguments = {{
                {
                    "--worldmenu", nullptr,
                    "Enables the world menu in the map-sharing code",
                    section_map_sharing,
                    []( int, const char ** ) -> int {
                        MAP_SHARING::setWorldmenu( true );
                        return true;
                    }
                },
                {
                    "--datadir", "<directory name>",
                    "Sub directory from which game data is loaded",
                    nullptr,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_datadir( params[0] );
                        return 1;
                    }
                },
                {
                    "--savedir", "<directory name>",
                    "Subdirectory for game saves",
                    section_user_directory,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_savedir( params[0] );
                        return 1;
                    }
                },
                {
                    "--configdir", "<directory name>",
                    "Subdirectory for game configuration",
                    section_user_directory,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_config_dir( params[0] );
                        return 1;
                    }
                },
                {
                    "--memorialdir", "<directory name>",
                    "Subdirectory for memorials",
                    section_user_directory,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_memorialdir( params[0] );
                        return 1;
                    }
                },
                {
                    "--optionfile", "<filename>",
                    "Name of the options file within the configdir",
                    section_user_directory,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_options( params[0] );
                        return 1;
                    }
                },
                {
                    "--autopickupfile", "<filename>",
                    "Name of the autopickup options file within the configdir",
                    nullptr,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_autopickup( params[0] );
                        return 1;
                    }
                },
                {
                    "--motdfile", "<filename>",
                    "Name of the message of the day file within the motd directory",
                    nullptr,
                    []( int num_args, const char **params ) -> int {
                        if( num_args < 1 )
                        {
                            return -1;
                        }
                        PATH_INFO::set_motd( params[0] );
                        return 1;
                    }
                },
            }
        };

        // Process CLI arguments.
        const size_t num_first_pass_arguments =
            sizeof( first_pass_arguments ) / sizeof( first_pass_arguments[0] );
        const size_t num_second_pass_arguments =
            sizeof( second_pass_arguments ) / sizeof( second_pass_arguments[0] );
        int saved_argc = --argc; // skip program name
        const char **saved_argv = const_cast<const char **>( ++argv );
        bool asked_game_path = false;
        while( argc ) {
            if( !strcmp( argv[0], "--help" ) ) {
                printHelpMessage( first_pass_arguments.data(), num_first_pass_arguments,
                                  second_pass_arguments.data(), num_second_pass_arguments );
                return 0;
            } else if( !strcmp( argv[0], "--version" ) ) {
                cata_printf( "%s\n", getVersionString() );
                return 0;
            } else if( !strcmp( argv[0], "--paths" ) ) {
                asked_game_path = true;
                argc--;
                argv++;
            } else {
                bool arg_handled = false;
                for( size_t i = 0; i < num_first_pass_arguments; ++i ) {
                    auto &arg_handler = first_pass_arguments[i];
                    if( !strcmp( argv[0], arg_handler.flag ) ) {
                        argc--;
                        argv++;
                        int args_consumed = arg_handler.handler( argc, const_cast<const char **>( argv ) );
                        if( args_consumed < 0 ) {
                            cata_printf( "Failed parsing parameter '%s'\n", *( argv - 1 ) );
                            exit( 1 );
                        }
                        argc -= args_consumed;
                        argv += args_consumed;
                        arg_handled = true;
                        break;
                    }
                }
                // Skip other options.
                if( !arg_handled ) {
                    --argc;
                    ++argv;
                }
            }
        }
        while( saved_argc ) {
            bool arg_handled = false;
            for( size_t i = 0; i < num_second_pass_arguments; ++i ) {
                auto &arg_handler = second_pass_arguments[i];
                if( !strcmp( saved_argv[0], arg_handler.flag ) ) {
                    --saved_argc;
                    ++saved_argv;
                    int args_consumed = arg_handler.handler( saved_argc, saved_argv );
                    if( args_consumed < 0 ) {
                        cata_printf( "Failed parsing parameter '%s'\n", *( argv - 1 ) );
                        exit( 1 );
                    }
                    saved_argc -= args_consumed;
                    saved_argv += args_consumed;
                    arg_handled = true;
                    break;
                }
            }
            // Ignore unknown options.
            if( !arg_handled ) {
                --saved_argc;
                ++saved_argv;
            }
        }
        if( asked_game_path ) {
            cata_printf( remove_color_tags( resolved_game_paths() ) );
            return 0;
        }
    }

    std::string current_path = std::filesystem::current_path().string();

    if( !dir_exist( PATH_INFO::datadir() ) ) {
        std::string msg = string_format(
                              "Can't find data directory \"%s\"\n"
                              "Current path: \"%s\"\n"
                              "Please ensure the current working directory is correct.\n"
                              "Perhaps you meant to start \"cataclysm-launcher\"?\n",
                              PATH_INFO::datadir(),
                              current_path
                          );
        report_fatal_error( msg );
        exit( 1 );
    }

    const auto check_dir_good = [&current_path]( const std::string & dir ) {
        if( !assure_dir_exist( dir ) ) {
            std::string msg = string_format(
                                  "Can't open or create \"%s\"\n"
                                  "Current path: \"%s\"\n"
                                  "Please ensure you have write permission.\n",
                                  dir.c_str(),
                                  current_path
                              );
            report_fatal_error( msg );
            exit( 1 );
        }
        if( !can_write_to_dir( dir ) ) {
            std::string msg = string_format(
                                  "Can't write to \"%s\"\n"
                                  "Current path: \"%s\"\n"
                                  "Please ensure you have write permission and free storage space.\n",
                                  dir.c_str(),
                                  current_path
                              );
            report_fatal_error( msg );
            exit( 1 );
        }
    };

#if defined(__ANDROID__)
    if( !dir_exist( PATH_INFO::user_dir() ) ) {
        check_dir_good( PATH_INFO::user_dir() );
        std::string external_storage_path( SDL_AndroidGetExternalStoragePath() );
        if( dir_exist( external_storage_path + "/config" ) ) {
            std::filesystem::copy( external_storage_path + "/config", PATH_INFO::user_dir() + "config",
                                   std::filesystem::copy_options::recursive );
            std::filesystem::copy( external_storage_path + "/font", PATH_INFO::user_dir() + "font",
                                   std::filesystem::copy_options::recursive );
            std::filesystem::copy( external_storage_path + "/gfx", PATH_INFO::user_dir() + "gfx",
                                   std::filesystem::copy_options::recursive );
            std::filesystem::copy( external_storage_path + "/save", PATH_INFO::user_dir() + "save",
                                   std::filesystem::copy_options::recursive );
            std::filesystem::copy( external_storage_path + "/sound", PATH_INFO::user_dir() + "sound",
                                   std::filesystem::copy_options::recursive );
            std::filesystem::copy( external_storage_path + "/templates", PATH_INFO::user_dir() + "templates",
                                   std::filesystem::copy_options::recursive );
        }
    }
#endif
    check_dir_good( PATH_INFO::user_dir() );
    check_dir_good( PATH_INFO::config_dir() );
    check_dir_good( PATH_INFO::savedir() );

    setupDebug( DebugOutput::file );

    if( !init_language_system() ) {
        exit_handler( -999 );
    }

#if defined(TILES)
    SDL_version compiled;
    SDL_VERSION( &compiled );
    DebugLog( DL::Info, DC::Main ) << "SDL version used during compile is "
                                   << static_cast<int>( compiled.major ) << "."
                                   << static_cast<int>( compiled.minor ) << "."
                                   << static_cast<int>( compiled.patch );

    SDL_version linked;
    SDL_GetVersion( &linked );
    DebugLog( DL::Info, DC::Main ) << "SDL version used during linking and in runtime is "
                                   << static_cast<int>( linked.major ) << "."
                                   << static_cast<int>( linked.minor ) << "."
                                   << static_cast<int>( linked.patch );
#endif

#if !defined(TILES)
    get_options().init();
    get_options().load();
    get_options().save();
    set_language(); // Have to set locale before initializing ncurses
#endif

    // in test mode don't initialize curses to avoid escape sequences being inserted into output stream
    if( !test_mode ) {
        try {
            // set minimum FULL_SCREEN sizes
            FULL_SCREEN_WIDTH = 80;
            FULL_SCREEN_HEIGHT = 24;
            catacurses::init_interface();
        } catch( const std::exception &err ) {
            // can't use any curses function as it has not been initialized
            std::cerr << "Error while initializing the interface: " << err.what() << '\n';
            DebugLog( DL::Error, DC::Main ) << "Error while initializing the interface: " << err.what();
            return 1;
        }
    }

#if defined(TILES)
    if( test_mode ) {
        get_options().init();
        get_options().load();
    }
    set_language();
#endif

    rng_set_engine_seed( seed );

    g = std::make_unique<game>();
    // First load and initialize everything that does not
    // depend on the mods.
    try {
        g->load_static_data();
        if( verifyexit ) {
            exit_handler( 0 );
        }
        if( !dump.empty() ) {
            init_colors();
            exit( g->dump_stats( dump, dmode, opts ) ? 0 : 1 );
        }
        if( check_mods ) {
            init_colors();
            loading_ui ui( false );
            const std::vector<mod_id> mods( opts.begin(), opts.end() );
            if( init::check_mods_for_errors( ui, mods ) ) {
                exit( 0 );
            } else {
                exit( 1 );
            }
        }
    } catch( const std::exception &err ) {
        debugmsg( "%s", err.what() );
        exit_handler( -999 );
    }

    // Now we do the actual game.

    game_ui::init_ui();

    catacurses::curs_set( 0 ); // Invisible cursor here, because MAPBUFFER.load() is crash-prone

#if !defined(_WIN32)
    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = signal_handler;
    sigemptyset( &sigIntHandler.sa_mask );
    sigIntHandler.sa_flags = 0;
    sigaction( SIGINT, &sigIntHandler, nullptr );
#endif

    DebugLog( DL::Info, DC::Main ) << "LAPI version: " << cata::get_lapi_version_string();
    cata::startup_lua_test();

    if( !lua_doc_output_path.empty() || !lua_types_output_path.empty() ) {
        init_colors();
        const auto doc_script = std::filesystem::path{PATH_INFO::datadir()} / "raw" / "generate_docs.lua";
        const auto types_script = std::filesystem::path{PATH_INFO::datadir()} / "raw" /
                                  "generate_types.lua";

        if( !lua_doc_output_path.empty() ) {
            const bool doc_result = cata::generate_lua_docs( doc_script, lua_doc_output_path );
            if( !doc_result ) {
                cata_printf( "Lua doc: Failed.\n" );
                return 1;
            }
            cata_printf( "Lua doc: Success.\n" );
        }
        if( !lua_types_output_path.empty() ) {
            const bool types_result = cata::generate_lua_docs( types_script, lua_types_output_path );
            if( !types_result ) {
                cata_printf( "Lua types: Failed.\n" );
                return 1;
            }
            cata_printf( "Lua types: Success.\n" );
        }
        return 0;
    }

    prompt_select_lang_on_startup();
    replay_buffered_debugmsg_prompts();

    while( true ) {
        if( !world.empty() ) {
            if( !g->load( world ) ) {
                break;
            }
            world.clear(); // ensure quit returns to opening screen

        } else {
            main_menu menu;
            if( !menu.opening_screen() ) {
                break;
            }
        }

        shared_ptr_fast<ui_adaptor> ui = g->create_or_get_main_ui_adaptor();
        options_manager::cache_balance_options();
        while( !g->do_turn() );
    }

    exit_handler( -999 );
    return 0;
}

namespace
{
void printHelpMessage( const arg_handler *first_pass_arguments,
                       size_t num_first_pass_arguments,
                       const arg_handler *second_pass_arguments,
                       size_t num_second_pass_arguments )
{

    // Group all arguments by help_group.
    std::multimap<std::string, const arg_handler *> help_map;
    for( size_t i = 0; i < num_first_pass_arguments; ++i ) {
        std::string help_group;
        if( first_pass_arguments[i].help_group ) {
            help_group = first_pass_arguments[i].help_group;
        }
        help_map.insert( std::make_pair( help_group, &first_pass_arguments[i] ) );
    }
    for( size_t i = 0; i < num_second_pass_arguments; ++i ) {
        std::string help_group;
        if( second_pass_arguments[i].help_group ) {
            help_group = second_pass_arguments[i].help_group;
        }
        help_map.insert( std::make_pair( help_group, &second_pass_arguments[i] ) );
    }

    cata_printf( R"(Info:
--help
    print this message and exit
--version
    print the version and exit
--paths
    print the paths used by the game and exit

Command line parameters:
)" );
    std::string current_help_group;
    auto it = help_map.begin();
    auto it_end = help_map.end();
    for( ; it != it_end; ++it ) {
        if( it->first != current_help_group ) {
            current_help_group = it->first;
            cata_printf( "\n%s\n", current_help_group.c_str() );
        }

        const arg_handler *handler = it->second;
        cata_printf( "%s", handler->flag );
        if( handler->param_documentation ) {
            cata_printf( " %s", handler->param_documentation );
        }
        cata_printf( "\n" );
        if( handler->documentation ) {
            cata_printf( "    %s\n", handler->documentation );
        }
    }
}
}  // namespace
