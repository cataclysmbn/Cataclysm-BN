#pragma once

#include "filesystem.h"

#include <string>

enum class holiday : int;

namespace PATH_INFO
{
auto init_base_path( fs::path path ) -> void;
auto init_user_dir( fs::path dir ) -> void;
auto set_standard_filenames() -> void;

auto autopickup() -> fs::path;
auto base_colors() -> fs::path;
auto base_path() -> fs::path;
auto colors() -> fs::path;
auto color_templates() -> fs::path;
auto config_dir() -> fs::path;
auto custom_colors() -> fs::path;
auto datadir() -> fs::path;
auto debug() -> fs::path;
auto defaultsounddir() -> fs::path;
auto defaulttilejson() -> fs::path;
auto defaulttilepng() -> fs::path;
auto fontconfig() -> fs::path;
auto user_fontconfig() -> fs::path;
auto fontdir() -> fs::path;
auto user_fontdir() -> fs::path;
auto language_defs_file() -> fs::path;
auto graveyarddir() -> fs::path;
auto help() -> fs::path;
auto keybindingsdir() -> fs::path;
auto main_menu_tips() -> fs::path;
auto lastworld() -> fs::path;
auto memorialdir() -> fs::path;
auto moddir() -> fs::path;
auto options() -> fs::path;
auto panel_options() -> fs::path;
auto safemode() -> fs::path;
auto distraction() -> fs::path;
auto savedir() -> fs::path;
auto sokoban() -> fs::path;
auto templatedir() -> fs::path;
auto user_dir() -> fs::path;
auto user_keybindings() -> fs::path;
auto user_moddir() -> fs::path;
auto worldoptions() -> fs::path;
auto crash() -> fs::path;
auto tileset_conf() -> fs::path;
auto gfxdir() -> fs::path;
auto user_gfx() -> fs::path;
auto data_sound() -> fs::path;
auto user_sound() -> fs::path;
auto mods_replacements() -> fs::path;
auto mods_dev_default() -> fs::path;
auto mods_user_default() -> fs::path;
auto soundpack_conf() -> fs::path;

auto credits() -> fs::path;
auto motd() -> fs::path;
auto title( holiday current_holiday ) -> fs::path;
auto names() -> fs::path;

auto set_datadir( const fs::path &datadir ) -> void;
auto set_config_dir( const fs::path &config_dir ) -> void;
auto set_savedir( const fs::path &savedir ) -> void;
auto set_memorialdir( const fs::path &memorialdir ) -> void;
auto set_options( const fs::path &options ) -> void;
auto set_autopickup( const fs::path &autopickup ) -> void;
auto set_motd( const fs::path &motd ) -> void;

} // namespace PATH_INFO

