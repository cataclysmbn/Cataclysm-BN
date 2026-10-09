#pragma once
#include <functional>
#include <string>

namespace client_platform {
struct sdl_services {
    bool video = true;
    bool audio = false;
    bool headless = false;
};
auto initialize_paths(bool redirect_logs) -> void;
auto migrate_user_files(const std::function<auto(const std::string&)->void>& check_dir_good)
    -> void;
auto install_quit_handler(bool confirm) -> void;
auto initialize_sdl_services(const sdl_services& services) -> bool;
auto initialize_compute() -> void;
} // namespace client_platform

auto run_game(int argc, char* argv[]) -> int;
