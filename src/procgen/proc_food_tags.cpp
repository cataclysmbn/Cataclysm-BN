#include "procgen/proc_food_tags.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

#include "flag.h"
#include "item.h"
#include "itype.h"
#include "procgen/proc_item.h"

namespace
{

auto has_material( const item &it, const material_id &id ) -> bool
{
    return std::ranges::find( it.made_of(), id ) != it.made_of().end();
}

auto add_tag( std::vector<std::string> &tags, const std::string &tag ) -> void
{
    if( std::ranges::find( tags, tag ) == tags.end() ) {
        tags.push_back( tag );
    }
}

template<size_t N>
auto id_contains_any( const std::string &id,
                      const std::array<std::string_view, N> &needles ) -> bool
{
    return std::ranges::any_of( needles, [&]( const std::string_view needle ) {
        return id.contains( needle );
    } );
}

auto is_finished_dish( const item &it ) -> bool
{
    if( !it.is_comestible() ) {
        return false;
    }

    if( proc::read_payload( it ) ) {
        return true;
    }

    const auto &id = it.typeId().str();
    if( id.starts_with( "mre_" ) ) {
        return true;
    }

    return id_contains_any( id, std::array<std::string_view, 15> {
        "sandwich", "burger", "hotdog", "pizza", "taco", "quesadilla", "stew",
        "soup", "curry", "salad", "pie", "burrito", "fries", "omelette", "omelet"
    } );
}

auto is_sandwich_spread( const item &it ) -> bool
{
    const auto &id = it.typeId().str();
    return has_material( it, material_id( "honey" ) ) || id == "syrup" ||
    id_contains_any( id, std::array<std::string_view, 9> {
        "mustard", "ketchup", "mayo", "butter", "horseradish", "sauerkraut", "soysauce",
        "sauce", "jam"
    } );
}

auto is_sandwich_sauce( const item &it ) -> bool
{
    const auto &id = it.typeId().str();
    return id == "ketchup" || id == "mustard" || id == "mayonnaise" || id == "horseradish" ||
           id == "sauerkraut" || id == "soysauce" || id == "sauce_pesto" || id == "sauce_red";
}

auto is_trail_mix_nut( const item &it ) -> bool
{
    return has_material( it, material_id( "nut" ) );
}

auto is_trail_mix_dried_fruit( const item &it ) -> bool
{
    if( !has_material( it, material_id( "fruit" ) ) ) {
        return false;
    }
    const auto &id = it.typeId().str();
    return id.contains( "dry" ) || id.contains( "dried" ) || id.contains( "leather" ) ||
           id.contains( "cranberr" );
}

auto is_trail_mix_sweet( const item &it ) -> bool
{
    const auto &id = it.typeId().str();
    return id.contains( "chocolate" ) || id == "candy" || id == "candy2" || id == "maltballs";
}

auto is_sandwich_cheese( const item &it ) -> bool
{
    return it.typeId().str().contains( "cheese" );
}

auto is_sandwich_bread( const item &it ) -> bool
{
    if( has_material( it, material_id( "flesh" ) ) || has_material( it, material_id( "hflesh" ) ) ||
        has_material( it, material_id( "iflesh" ) ) || has_material( it, material_id( "fish" ) ) ) {
        return false;
    }

    const auto &id = it.typeId().str();
    if( id_contains_any( id, std::array<std::string_view, 9> {
    "sweetbread", "pancake", "waffle", "cracker", "pretzel", "cookie", "brownie", "donut", "cake"
} ) ) {
        return false;
    }

    return id.contains( "bread" ) || id.contains( "bun" ) || id.contains( "bagel" ) ||
           id.contains( "roll" ) || id.contains( "tortilla" ) || id.contains( "biscuit" ) ||
           id.contains( "hardtack" ) || id.contains( "brioche" );
}

auto is_meat_ingredient( const item &it ) -> bool
{
    return has_material( it, material_id( "flesh" ) ) || has_material( it, material_id( "hflesh" ) ) ||
           has_material( it, material_id( "iflesh" ) ) || has_material( it, material_id( "fish" ) );
}

auto is_fish_ingredient( const item &it ) -> bool
{
    if( has_material( it, material_id( "fish" ) ) ) {
        return true;
    }

    const auto &id = it.typeId().str();
    return id == "fish" || id.starts_with( "fish_" ) || id.contains( "_fish" );
}

auto is_egg_ingredient( const item &it ) -> bool
{
    return has_material( it, material_id( "egg" ) );
}

auto is_fruit_ingredient( const item &it ) -> bool
{
    return has_material( it, material_id( "fruit" ) );
}

auto is_dairy_ingredient( const item &it ) -> bool
{
    return has_material( it, material_id( "milk" ) );
}

auto is_wheat_ingredient( const item &it ) -> bool
{
    return has_material( it, material_id( "wheat" ) );
}

auto is_noodle_ingredient( const item &it ) -> bool
{
    return id_contains_any( it.typeId().str(), std::array<std::string_view, 5> {
        "noodle", "spaghetti", "macaroni", "ramen", "udon"
    } );
}

auto is_broth_ingredient( const item &it ) -> bool
{
    const auto &id = it.typeId().str();
    return id == "broth" || id == "broth_bone";
}

auto is_liquid_ingredient( const item &it ) -> bool
{
    return it.made_of( phase_id::LIQUID );
}

auto add_food_category_tags( std::vector<std::string> &tags, const item &it,
                             const bool raw_ingredient_candidate,
                             const bool solid_ingredient_candidate ) -> void
{
    if( !raw_ingredient_candidate ) {
        return;
    }

    add_tag( tags, "ingredient" );
    add_tag( tags, is_liquid_ingredient( it ) ? "liquid" : "solid" );

    if( it.has_flag( flag_RAW ) ) {
        add_tag( tags, "raw" );
    }
    if( is_wheat_ingredient( it ) ) {
        add_tag( tags, "grain" );
        add_tag( tags, "wheat" );
    }
    if( is_fruit_ingredient( it ) ) {
        add_tag( tags, "fruit" );
    }
    if( is_dairy_ingredient( it ) ) {
        add_tag( tags, "dairy" );
    }
    if( is_trail_mix_nut( it ) ) {
        add_tag( tags, "nut" );
    }
    if( is_egg_ingredient( it ) ) {
        add_tag( tags, "egg" );
    }
    if( is_broth_ingredient( it ) ) {
        add_tag( tags, "broth" );
    }
    if( is_noodle_ingredient( it ) ) {
        add_tag( tags, "noodle" );
    }
    if( is_sandwich_sauce( it ) ) {
        add_tag( tags, "sauce" );
    }
    if( is_sandwich_spread( it ) && !is_sandwich_sauce( it ) ) {
        add_tag( tags, "spread" );
    }
    if( solid_ingredient_candidate && is_meat_ingredient( it ) ) {
        add_tag( tags, "meat" );
    }
    if( solid_ingredient_candidate && is_fish_ingredient( it ) ) {
        add_tag( tags, "fish" );
    }
    if( is_sandwich_bread( it ) ) {
        add_tag( tags, "bread" );
    }
    if( solid_ingredient_candidate && is_sandwich_cheese( it ) ) {
        add_tag( tags, "cheese" );
    }
    if( solid_ingredient_candidate && has_material( it, material_id( "veggy" ) ) &&
        !is_sandwich_spread( it ) ) {
        add_tag( tags, "veg" );
    }
    if( is_sandwich_spread( it ) ) {
        add_tag( tags, "cond" );
    }
}

} // namespace

auto proc::food_tags( const item &it ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    const auto finished_dish = is_finished_dish( it );
    const auto raw_ingredient_candidate = it.is_comestible() && !finished_dish;
    const auto solid_ingredient_candidate = raw_ingredient_candidate && !it.made_of( phase_id::LIQUID );

    if( finished_dish ) {
        add_tag( ret, "dish" );
    }
    add_food_category_tags( ret, it, raw_ingredient_candidate, solid_ingredient_candidate );
    if( solid_ingredient_candidate && is_trail_mix_nut( it ) ) {
        add_tag( ret, "trail_nut" );
    }
    if( solid_ingredient_candidate && is_trail_mix_dried_fruit( it ) ) {
        add_tag( ret, "trail_dried" );
        add_tag( ret, "dried" );
    }
    if( solid_ingredient_candidate && is_trail_mix_sweet( it ) ) {
        add_tag( ret, "trail_sweet" );
    }
    return ret;
}
