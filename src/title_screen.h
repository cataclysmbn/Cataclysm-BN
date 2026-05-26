#pragma once

#include <string>
#include <vector>

#include "options.h"

namespace title_screen
{

inline constexpr auto option_id = "TITLE_SCREEN";
inline constexpr auto default_option_id = "default";
inline constexpr auto random_option_id = "random";

auto get_options() -> std::vector<options_manager::id_and_option>;
auto get_all_options() -> std::vector<options_manager::id_and_option>;
auto resolve_path() -> std::string;

} // namespace title_screen
