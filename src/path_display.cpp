#include "path_info.h"
#include "translations.h"
#include "color.h"
#include "string_formatter.h"
#include "output.h"
#include "string_utils.h"
#include "path_display.h"

namespace
{

struct section {
    std::string name, path;
};

auto maybe_colorize( const std::string &text, const nc_color &color,
                     const bool use_color ) -> std::string
{
    return use_color ? colorize( text, color ) : text;
}

auto path_info( const section &title, const std::vector<section> &xs,
                const bool use_color ) -> std::string
{
    const auto result = string_format( "%s: %s\n",
                                       maybe_colorize( title.name, c_white, use_color ),
                                       maybe_colorize( title.path, c_light_cyan, use_color ) );
    const auto displayed_base_path = maybe_colorize( title.path, c_light_cyan, use_color );
    const auto printer = [&title, &displayed_base_path, use_color]( const section & s ) {
        return string_format( "    %s: %s",
                              maybe_colorize( s.name, c_yellow, use_color ),
                              replace_all( s.path, title.path, displayed_base_path ) );
    };

    return result + enumerate_as_string( xs.begin(), xs.end(), printer,
                                         enumeration_conjunction::newline ) + "\n\n";
}

auto format_user_directory( const bool use_color ) -> std::string
{
    return path_info( { _( "User Directory" ), PATH_INFO::user_dir() }, {
        { _( "user mods" ), PATH_INFO::user_moddir() },
        { _( "user saves" ), PATH_INFO::savedir() },
        { _( "user sounds" ), PATH_INFO::user_sound() },
        { _( "user graphics" ), PATH_INFO::user_gfx() },
        { _( "user fonts" ), PATH_INFO::user_fontdir() },
        { _( "user memorials" ), PATH_INFO::memorialdir() },
        { _( "user templates" ), PATH_INFO::templatedir() },
        { _( "user graveyard" ), PATH_INFO::graveyarddir() },
    }, use_color );
}

auto format_defaults_directory( const bool use_color ) -> std::string
{
    const auto title = section{ _( "Defaults Directory" ),
                                PATH_INFO::base_path().empty()
                                ? _( "(Current Working Directory)" )
                                : PATH_INFO::base_path()
                              };

    return path_info( title, {
        { _( "data directory" ), maybe_colorize( PATH_INFO::datadir(), c_white, use_color ) },
        { _( "font" ), PATH_INFO::fontdir() },
        { _( "help" ), PATH_INFO::help() },
        { _( "mods" ), PATH_INFO::moddir() },
        { _( "default enabled mods" ), PATH_INFO::mods_dev_default() },
        { _( "replacement mods" ), PATH_INFO::mods_replacements() },
        { _( "color templates" ), PATH_INFO::color_templates() },
        { _( "colors" ), PATH_INFO::colors() },
        { _( "font config" ), PATH_INFO::fontconfig() },
        { _( "language definitions file" ), PATH_INFO::language_defs_file() },
        { _( "sokoban" ), PATH_INFO::sokoban() },
        { _( "main menu tips" ), PATH_INFO::main_menu_tips() },
        { _( "keybindings" ), PATH_INFO::keybindingsdir() },
        { _( "graphics" ), PATH_INFO::gfxdir() },
        { _( "default sound" ), PATH_INFO::defaultsounddir() },
        { _( "sound" ), PATH_INFO::data_sound() },
    }, use_color );
}

auto format_config_directory( const bool use_color ) -> std::string
{
    return path_info( { _( "Config Directory" ), PATH_INFO::config_dir() }, {
        { _( "debug" ), PATH_INFO::debug() },
        { _( "crash" ), PATH_INFO::crash() },
        { _( "options" ), PATH_INFO::options() },
        { _( "autopickup" ), PATH_INFO::autopickup() },
        { _( "base colors" ), PATH_INFO::base_colors() },
        { _( "custom colors" ), PATH_INFO::custom_colors() },
        { _( "user default mods" ), PATH_INFO::mods_user_default() },
        { _( "distraction" ), PATH_INFO::distraction() },
        { _( "user font config" ), PATH_INFO::user_fontconfig() },
        { _( "user keybindings" ), PATH_INFO::user_keybindings() },
        { _( "last world" ), PATH_INFO::lastworld() },
        { _( "panel options" ), PATH_INFO::panel_options() },
        { _( "safe mode" ), PATH_INFO::safemode() },
    }, use_color );
}

} // namespace

auto user_directory() -> std::string
{
    return format_user_directory( true );
}

auto defaults_directory() -> std::string
{
    return format_defaults_directory( true );
}

auto config_directory() -> std::string
{
    return format_config_directory( true );
}

auto resolved_game_paths( const bool use_color ) -> std::string
{
    return enumerate_as_string( std::vector{ format_user_directory( use_color ),
                                format_defaults_directory( use_color ),
                                format_config_directory( use_color ) },
                                enumeration_conjunction::newline );
}
