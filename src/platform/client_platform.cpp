#include "platform/client_platform.h"

#include "client_backend.h"
#include "cursesdef.h"
#include "debug.h"
#include "filesystem.h"
#include "input.h"
#include "output.h"
#include "path_info.h"
#include "runtime_handlers.h"
#include "ui_manager.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#if defined(PREFIX)
#    undef PREFIX
#    include "prefix.h"
#endif
#if defined(CATA_SDL)
#    include "compute/gpu_platform.h"
#    include "platform/sdl_video.h"

#    include <SDL3/SDL.h>
#endif
#if defined(__ANDROID__)
#    include <SDL3/SDL_system.h>
#    include <android/log.h>
#    include <pthread.h>
#    include <unistd.h>
namespace {
auto log_pipe = std::array<int, 2>{};
auto log_tag = "cdda";

auto forward_android_logs(void* /*context*/) -> void* {
    auto buffer = std::array<char, 128>{};
    for (;;) {
        auto size = read(log_pipe[0], buffer.data(), buffer.size() - 1);
        if (size < 0 && errno == EINTR) { continue; }
        if (size <= 0) { break; }
        if (buffer[size - 1] == '\n') { --size; }
        buffer[size] = '\0';
        __android_log_write(ANDROID_LOG_DEBUG, log_tag, buffer.data());
    }
    close(log_pipe[0]);
    return nullptr;
}

auto start_logger(const char* name) -> void {
    log_tag = name;
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    if (pipe(log_pipe.data()) != 0) { return; }
    auto thread = pthread_t{};
    if (pthread_create(&thread, nullptr, forward_android_logs, nullptr) != 0) {
        close(log_pipe[0]);
        close(log_pipe[1]);
        return;
    }
    dup2(log_pipe[1], STDOUT_FILENO);
    dup2(log_pipe[1], STDERR_FILENO);
    close(log_pipe[1]);
    pthread_detach(thread);
}
} // namespace

#endif
#if !defined(_WIN32)
#    include <csignal>
namespace {
auto immediate_signal_exit = false;

auto signal_handler(int signal) -> void {
    if (!immediate_signal_exit && signal == SIGINT) {
        const auto old_timeout = inp_mngr.get_timeout();
        inp_mngr.reset_timeout();
        const auto confirmed = query_yn(_("Really Quit?  All unsaved changes will be lost."));
        inp_mngr.set_timeout(old_timeout);
        ui_manager::redraw_invalidated();
        catacurses::doupdate();
        if (!confirmed) { return; }
    }
    exit_handler(0);
}
} // namespace
#endif

namespace client_platform {
auto initialize_paths(const bool redirect_logs) -> void {
#if defined(__ANDROID__)
    // Start the standard output logging redirector
    if (redirect_logs) { start_logger("cdda"); }

    // On Android first launch, we copy all data files from the APK into the app's writeable folder
    // so std::io stuff works. Use the external storage so it's publicly modifiable data (so users
    // can mess with installed data, save games etc.)
    const auto external_storage_path = std::string(SDL_GetAndroidExternalStoragePath());

    PATH_INFO::init_base_path(external_storage_path);
#else
    (void)redirect_logs;
    // Set default file paths
#    if defined(PREFIX)
    PATH_INFO::init_base_path(std::string(PREFIX));
#    else
    PATH_INFO::init_base_path("");
#    endif
#endif

#if defined(__ANDROID__)
    PATH_INFO::init_user_dir(external_storage_path);
#else
#    if defined(USE_HOME_DIR) || defined(USE_XDG_DIR)
    PATH_INFO::init_user_dir("");
#    else
    PATH_INFO::init_user_dir(".");
#    endif
#endif
    PATH_INFO::set_standard_filenames();
}
auto migrate_user_files(const std::function<auto(const std::string&)->void>& check_dir_good)
    -> void {
#if defined(__ANDROID__)
    if (!dir_exist(PATH_INFO::user_dir())) {
        check_dir_good(PATH_INFO::user_dir());
        const auto external_storage_path = std::string(SDL_GetAndroidExternalStoragePath());
        if (dir_exist(external_storage_path + "/config")) {
            std::filesystem::
                copy(external_storage_path + "/config", PATH_INFO::user_dir() + "config",
                     std::filesystem::copy_options::recursive);
            std::filesystem::
                copy(external_storage_path + "/font", PATH_INFO::user_dir() + "font",
                     std::filesystem::copy_options::recursive);
            std::filesystem::
                copy(external_storage_path + "/gfx", PATH_INFO::user_dir() + "gfx",
                     std::filesystem::copy_options::recursive);
            std::filesystem::
                copy(external_storage_path + "/save", PATH_INFO::user_dir() + "save",
                     std::filesystem::copy_options::recursive);
            std::filesystem::
                copy(external_storage_path + "/sound", PATH_INFO::user_dir() + "sound",
                     std::filesystem::copy_options::recursive);
            std::filesystem::
                copy(external_storage_path + "/templates", PATH_INFO::user_dir() + "templates",
                     std::filesystem::copy_options::recursive);
        }
    }
#endif
#if !defined(__ANDROID__)
    (void)check_dir_good;
#endif
}
auto install_quit_handler(const bool confirm) -> void {
#if !defined(_WIN32)
    immediate_signal_exit = !confirm;
    using signal_action = struct sigaction;
    auto handler = signal_action{};
    handler.sa_handler = signal_handler;
    sigemptyset(&handler.sa_mask);
    handler.sa_flags = 0;
    sigaction(SIGINT, &handler, nullptr);
#else
    (void)confirm;
#endif
}
auto initialize_sdl_services(const sdl_services& services) -> bool {
#if defined(CATA_SDL)
    if (!services.video && !services.audio) { return true; }
    if (services.video && services.headless) {
        if (use_offscreen_video_driver_for_headless_sdl() == offscreen_sdl_hint_result::failed) {
            DebugLog(DL::Warn, DC::Main)
                << "Cannot select offscreen SDL driver: " << SDL_GetError();
        }
    }
    auto flags = SDL_InitFlags{0};
    if (services.video) { flags |= SDL_INIT_VIDEO; }
    if (services.audio) { flags |= SDL_INIT_AUDIO; }
    if (!SDL_Init(flags)) {
        DebugLog(DL::Error, DC::Main) << "SDL_Init failed: " << SDL_GetError();
        return false;
    }
    atexit(SDL_Quit);
#else
    (void)services;
#endif
    return true;
}
auto initialize_compute() -> void {
#if defined(CATA_SDL)
    cata_gpu::init();
    atexit(cata_gpu::shutdown);
#endif
}
} // namespace client_platform
