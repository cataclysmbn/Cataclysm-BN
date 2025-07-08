#pragma once

#include <optional>
#include <string>

namespace game_info
{
/** Return the name of the current operating system. */
auto operating_system() -> std::string;
/** Return a detailed version of the operating system; e.g. "Ubuntu 18.04" or "(Windows) 10 1809". */
auto operating_system_version() -> std::string;
/** Return the "bitness" of the game (not necessarily of the operating system); either: 64, 32 or nullopt. */
auto bitness() -> std::optional<int>;
/** Return the "bitness" string of the game (not necessarily of the operating system); either: 64-bit, 32-bit or Unknown. */
auto bitness_string() -> std::string;
/** Return the game version, as in the entry screen. */
auto game_version() -> std::string;
/** Return the underlying graphics version used by the game; either Tiles or Curses. */
auto graphics_version() -> std::string;
/** Return a list of the loaded mods, including the mod full name and its id name in brackets, e.g. "Dark Days Ahead [dda]". */
auto mods_loaded() -> std::string;
/** Generate a game report, including the information returned by all of the other functions. */
auto game_report() -> std::string;
/** Current world save file version */
auto save_file_version() -> std::string;
} // namespace game_info


