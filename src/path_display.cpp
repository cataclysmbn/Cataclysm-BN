#include "path_info.h"
#include "translations.h"
#include "color.h"
#include "string_formatter.h"
#include "output.h"
#include "path_utils.h"
#include "string_utils.h"
#include "path_display.h"

namespace
{

struct section {
    std::string name, path;
};

auto display_path( const fs::path &path ) -> std::string
{
    return cata_files::path_to_generic_utf8( path );
}

auto create_line_printer( const std::string &base_path )
{
    const std::string colored_base_path = colorize( base_path, c_light_cyan );

    return [&base_path, colored_base_path]( section s ) -> std::string {
        return string_format( "    %s: %s",
                              colorize( s.name, c_yellow ),
                              replace_all( s.path, base_path, colored_base_path ) );
    };
};

auto path_info( const section &title,
                const std::vector<section> &xs ) -> std::string
{
    const std::string result = string_format( "%s: %s\n",
                               colorize( title.name, c_white ),
                               colorize( title.path, c_light_cyan ) );

    const auto printer = create_line_printer( title.path );

    return result + enumerate_as_string( xs.begin(), xs.end(),
                                         printer,
                                         enumeration_conjunction::newline ) + "\n\n";
};

} // namespace

auto user_directory() -> std::string
{
    return path_info( { _( "User Directory" ), display_path( PATH_INFO::user_dir() ) }, {
        { _( "user mods" ), display_path( PATH_INFO::user_moddir() ) },
        { _( "user saves" ), display_path( PATH_INFO::savedir() ) },
        { _( "user sounds" ), display_path( PATH_INFO::user_sound() ) },
        { _( "user graphics" ), display_path( PATH_INFO::user_gfx() ) },
        { _( "user fonts" ), display_path( PATH_INFO::user_fontdir() ) },
        { _( "user memorials" ), display_path( PATH_INFO::memorialdir() ) },
        { _( "user templates" ), display_path( PATH_INFO::templatedir() ) },
        { _( "user graveyard" ), display_path( PATH_INFO::graveyarddir() ) },
    } );
}

auto defaults_directory() -> std::string
{
    const section title = { _( "Defaults Directory" ),
                            PATH_INFO::base_path().empty()
                            ? _( "(Current Working Directory)" )
                            : display_path( PATH_INFO::base_path() )
                          };

    return path_info( title, {
        { _( "data directory" ), colorize( display_path( PATH_INFO::datadir() ), c_white ) },
        { _( "font" ), display_path( PATH_INFO::fontdir() ) },
        { _( "help" ), display_path( PATH_INFO::help() ) },
        { _( "mods" ), display_path( PATH_INFO::moddir() ) },
        { _( "default enabled mods" ), display_path( PATH_INFO::mods_dev_default() ) },
        { _( "replacement mods" ), display_path( PATH_INFO::mods_replacements() ) },
        { _( "color templates" ), display_path( PATH_INFO::color_templates() ) },
        { _( "colors" ), display_path( PATH_INFO::colors() ) },
        { _( "font config" ), display_path( PATH_INFO::fontconfig() ) },
        { _( "language definitions file" ), display_path( PATH_INFO::language_defs_file() ) },
        { _( "sokoban" ), display_path( PATH_INFO::sokoban() ) },
        { _( "main menu tips" ), display_path( PATH_INFO::main_menu_tips() ) },
        { _( "keybindings" ), display_path( PATH_INFO::keybindingsdir() ) },
        { _( "graphics" ), display_path( PATH_INFO::gfxdir() ) },
        { _( "default sound" ), display_path( PATH_INFO::defaultsounddir() ) },
        { _( "sound" ), display_path( PATH_INFO::data_sound() ) },
    } );
}

auto config_directory() -> std::string
{
    return path_info( { _( "Config Directory" ), display_path( PATH_INFO::config_dir() ) }, {
        { _( "debug" ), display_path( PATH_INFO::debug() ) },
        { _( "crash" ), display_path( PATH_INFO::crash() ) },
        { _( "options" ), display_path( PATH_INFO::options() ) },
        { _( "autopickup" ), display_path( PATH_INFO::autopickup() ) },
        { _( "base colors" ), display_path( PATH_INFO::base_colors() ) },
        { _( "custom colors" ), display_path( PATH_INFO::custom_colors() ) },
        { _( "user default mods" ), display_path( PATH_INFO::mods_user_default() ) },
        { _( "distraction" ), display_path( PATH_INFO::distraction() ) },
        { _( "user font config" ), display_path( PATH_INFO::user_fontconfig() ) },
        { _( "user keybindings" ), display_path( PATH_INFO::user_keybindings() ) },
        { _( "last world" ), display_path( PATH_INFO::lastworld() ) },
        { _( "panel options" ), display_path( PATH_INFO::panel_options() ) },
        { _( "safe mode" ), display_path( PATH_INFO::safemode() ) },
    } );
}

auto resolved_game_paths() -> std::string
{
    return enumerate_as_string( std::vector{ user_directory(), defaults_directory(), config_directory() },
                                enumeration_conjunction::newline );
}
