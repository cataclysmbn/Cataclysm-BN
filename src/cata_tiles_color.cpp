#if defined(TILES)
#include "cata_tiles.h"

#include "map.h"
#include "monster.h"
#include "character.h"
#include "field.h"

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
    const item &, const map &, const tripoint & ) -> color_tint_pair
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
    const effect &, const Character &, const map &, const tripoint & ) -> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_bionic_color(
    const bionic &, const Character &, const map &, const tripoint & )-> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

auto cata_tiles::get_mutation_color(
    const mutation &, const Character &, const map &, const tripoint & )-> color_tint_pair
{
    return {std::nullopt, std::nullopt};
}

#endif