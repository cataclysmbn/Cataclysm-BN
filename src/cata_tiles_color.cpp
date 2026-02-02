#if defined(TILES)
#include "cata_tiles.h"

#include "map.h"
#include "monster.h"
#include "character.h"
#include "field.h"
#include "color.h"

auto cata_tiles::get_overmap_color(
    const overmapbuffer &, const tripoint_abs_omt & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_terrain_color(
    const ter_t &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_furniture_color(
    const furn_t &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_graffiti_color(
    const map &, const tripoint & )-> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_trap_color(
    const trap &, const map &, tripoint ) -> color_tint_pair
{
    return { std::nullopt, std::nullopt};
}

auto cata_tiles::get_field_color(
    const field &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_item_color(
    const item &i, const map &, const tripoint & ) -> color_tint_pair
{
    return get_item_color( i );
}

auto cata_tiles::get_item_color(
    const item & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_vpart_color(
    const optional_vpart_position &, const map &, const tripoint & )-> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_monster_color(
    const monster &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_character_color(
    const Character &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_effect_color(
    const effect &eff, const Character &c, const map &, const tripoint & ) -> color_tint_pair
{
    return get_effect_color( eff, c );
}

auto cata_tiles::get_effect_color(
    const effect &, const Character & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_bionic_color(
    const bionic &bio, const Character &c, const map &, const tripoint & )-> color_tint_pair
{
    return get_bionic_color( bio, c );
}

auto cata_tiles::get_bionic_color(
    const bionic &, const Character & )-> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_mutation_color(
    const mutation &mut, const Character &c, const map &, const tripoint & )-> color_tint_pair
{
    return get_mutation_color( mut, c );
}

auto cata_tiles::get_mutation_color(
    const mutation &mut, const Character &c )-> color_tint_pair
{
    mutation_branch mutation = mut.first.obj();
    for( const auto &mut_type : mutation.types ) {
        auto controller = tileset_ptr->get_tint_controller( mut_type );
        if( !controller.empty() ) {
            for( const auto &oth_mut : c.get_mutations() ) {
                if( oth_mut.obj().types.contains( controller ) ) {
                    auto tint = tileset_ptr->get_tint( oth_mut.str() );
                    if( tint != nullptr ) {
                        auto sdl_tint = SDL_Color{ tint->r, tint->g, tint->b, tint->a };
                        return { sdl_tint, sdl_tint };
                    } else {
                        auto str = oth_mut.str();
                        if( str.find( "_" ) == std::string::npos ) {
                            break;
                        }
                        str = str.substr( str.rfind( '_' ) + 1 );
                        auto colors = get_all_colors();
                        if( str == "blonde" ) { str = "yellow"; }
                        else if (str == "gray") { str = "light_gray"; }
                        auto curse_color = colors.name_to_color( "c_" + str );
                        if( curse_color == c_unset ) {
                            return { std::nullopt, std::nullopt };
                        }
                        auto tint = curses_color_to_RGB( curse_color );
                        auto sdl_tint = SDL_Color{ tint.r, tint.g, tint.b, tint.a };
                        return { sdl_tint, sdl_tint };
                    }
                    break;
                }
            }
            break;
        }
    }
    return {std::nullopt, std::nullopt};
}

#endif
