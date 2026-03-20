#include "proc_fact.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

#include "item.h"
#include "itype.h"
#include "proc_item.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

auto has_material( const item &it, const material_id &id ) -> bool
{
    return std::ranges::find( it.made_of(), id ) != it.made_of().end();
}

auto is_comestible_candidate( const item &it ) -> bool
{
    return it.is_comestible();
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
    return id_contains_any( id, std::array<std::string_view, 13> {
        "sandwich", "burger", "hotdog", "pizza", "taco", "quesadilla", "stew",
        "soup", "curry", "salad", "pie", "burrito", "fries"
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

auto is_sandwich_cheese( const item &it ) -> bool
{
    return it.typeId().str().contains( "cheese" );
}

auto normalized_hp( const item &it ) -> float
{
    const auto max_damage = it.max_damage();
    if( max_damage <= 0 ) {
        return 1.0f;
    }

    const auto cur_damage = std::clamp( it.damage(), 0, max_damage );
    return static_cast<float>( max_damage - cur_damage ) / static_cast<float>( max_damage );
}

auto default_tags( const item &it ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    const auto &id = it.typeId().str();
    const auto finished_dish = is_finished_dish( it );
    const auto raw_ingredient_candidate = is_comestible_candidate( it ) && !finished_dish;
    if( finished_dish ) {
        ret.push_back( "dish" );
    }
    if( raw_ingredient_candidate &&
        ( ( !it.count_by_charges() &&
            has_material( it, material_id( "wheat" ) ) ) ||
          id.contains( "bread" ) || id.contains( "bun" ) || id.contains( "bagel" ) ||
          id.contains( "toast" ) || id.contains( "roll" ) || id.contains( "tortilla" ) ||
          id.contains( "biscuit" ) || id.contains( "hardtack" ) ) ) {
        ret.push_back( "bread" );
    }
    if( raw_ingredient_candidate && is_sandwich_cheese( it ) ) {
        ret.push_back( "cheese" );
    }
    if( raw_ingredient_candidate && has_material( it, material_id( "veggy" ) ) &&
        !is_sandwich_spread( it ) ) {
        ret.push_back( "veg" );
    }
    if( raw_ingredient_candidate && is_sandwich_spread( it ) ) {
        ret.push_back( "cond" );
    }
    if( raw_ingredient_candidate &&
        ( has_material( it, material_id( "flesh" ) ) || has_material( it, material_id( "hflesh" ) ) ||
          has_material( it, material_id( "iflesh" ) ) || has_material( it, material_id( "fish" ) ) ) ) {
        ret.push_back( "meat" );
    }
    return ret;
}

auto default_flags( const item &it ) -> std::vector<flag_id>
{
    auto ret = std::vector<flag_id> {};
    const auto &flags = it.get_flags();
    ret.reserve( flags.size() );
    std::ranges::copy( flags, std::back_inserter( ret ) );
    return ret;
}

auto default_qualities( const item &it ) -> std::map<quality_id, int>
{
    auto ret = std::map<quality_id, int> {};
    std::ranges::for_each( it.type->qualities, [&]( const std::pair<quality_id, int> &entry ) {
        ret.emplace( entry.first, entry.second );
    } );
    return ret;
}

auto default_vitamins( const item &it ) -> std::map<vitamin_id, int>
{
    if( !it.is_comestible() ) {
        return {};
    }
    return it.get_comestible()->default_nutrition.vitamins;
}

} // namespace

auto proc::normalize_part_fact( const item &it, const proc::normalize_options &opts ) -> part_fact
{
    auto fact = part_fact{};
    const auto full_charges = it.count_by_charges() ? std::max( it.charges, 1 ) : 1;
    const auto selected_charges = it.count_by_charges() ?
                                  std::clamp( opts.charges > 0 ? opts.charges : 1, 1, full_charges ) : 0;
    const auto uses = std::max( opts.uses, 1 );
    const auto scale = it.count_by_charges() ?
                       static_cast<double>( selected_charges ) / static_cast<double>( full_charges ) : 1.0;

    fact.ix = opts.ix;
    fact.id = it.typeId();
    fact.tag = default_tags( it );
    fact.flag = default_flags( it );
    fact.qual = default_qualities( it );
    fact.mat = it.made_of();
    fact.vit = default_vitamins( it );
    fact.mass_g = static_cast<int>( std::lround( static_cast<double>( units::to_gram( it.weight() ) ) *
                                    scale ) );
    fact.volume_ml = static_cast<int>( std::lround( static_cast<double>( units::to_milliliter(
                                           it.volume() ) ) *
                                       scale ) );
    fact.kcal = it.is_comestible() ? it.get_comestible()->default_nutrition.kcal *
                std::max( selected_charges, 1 ) : 0;
    fact.hp = normalized_hp( it );
    fact.chg = selected_charges;
    fact.uses = uses;
    std::ranges::for_each( fact.vit, [&]( std::pair<const vitamin_id, int> &entry ) {
        entry.second *= std::max( selected_charges, 1 );
    } );
    if( const auto payload = proc::read_payload( it ) ) {
        fact.proc = proc::payload_json( *payload );
    }
    return fact;
}

auto proc::normalize_part_facts( const std::vector<const item *> &items ) -> std::vector<part_fact>
{
    auto ret = std::vector<part_fact> {};
    ret.reserve( items.size() );
    auto ix = part_ix{ 0 };
    std::ranges::for_each( items, [&]( const item * it ) {
        if( it == nullptr ) {
            return;
        }
        ret.push_back( normalize_part_fact( *it, { .ix = ix } ) );
        ix++;
    } );
    return ret;
}
