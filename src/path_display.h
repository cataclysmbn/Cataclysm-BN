#pragma once

#include <string>

auto resolved_game_paths( bool use_color = true ) -> std::string;
auto user_directory() -> std::string;
auto defaults_directory() -> std::string;
auto config_directory() -> std::string;


