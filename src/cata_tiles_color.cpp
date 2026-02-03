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
    const mutation &mut, const Character &c ) -> color_tint_pair
{
    const mutation_branch &mut_branch = mut.first.obj();
    for( const std::string &mut_type : mut_branch.types ) {
        const std::string controller = tileset_ptr->get_tint_controller( mut_type );
        if( controller.empty() ) {
            continue;
        }
        for( const trait_id &other_mut : c.get_mutations() ) {
            if( !other_mut.obj().types.contains( controller ) ) {
                continue;
            }
            const color_tint_pair *tint = tileset_ptr->get_tint( other_mut.str() );
            if( tint != nullptr ) {
                return *tint;
            }
            // Legacy fallback: extract color from mutation name suffix
            std::string color_name = other_mut.str();
            if( color_name.find( '_' ) == std::string::npos ) {
                return { std::nullopt, std::nullopt };
            }
            color_name = color_name.substr( color_name.rfind( '_' ) + 1 );
            if( color_name == "blond" ) {
                color_name = "yellow";
            } else if( color_name == "gray" ) {
                color_name = "light_gray";
            }
            const nc_color curse_color = get_all_colors().name_to_color( "c_" + color_name );
            if( curse_color == c_unset ) {
                return { std::nullopt, std::nullopt };
            }
            const SDL_Color sdl_tint = static_cast<SDL_Color>( curses_color_to_RGB( curse_color ) );
            return { sdl_tint, sdl_tint };
        }
        break;
    }
    return { std::nullopt, std::nullopt };
}

#endif
