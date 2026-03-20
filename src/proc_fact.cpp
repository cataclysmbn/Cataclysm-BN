#include "proc_fact.h"

#include <algorithm>
#include <string>

#include "item.h"
#include "itype.h"
#include "proc_item.h"
#include "units_mass.h"
#include "units_volume.h"

namespace
{

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
    if( id.starts_with( "bread" ) ) {
        ret.push_back( "bread" );
    }
    if( id.starts_with( "cheese" ) ) {
        ret.push_back( "cheese" );
    }
    if( id == "lettuce" || id == "tomato" ) {
        ret.push_back( "veg" );
    }
    if( id == "mustard" || id == "mustard_powder" ) {
        ret.push_back( "cond" );
    }
    if( id.starts_with( "meat_" ) ) {
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

auto proc::normalize_part_fact( const item &it, const part_ix ix ) -> part_fact
{
    auto fact = part_fact{};
    fact.ix = ix;
    fact.id = it.typeId();
    fact.tag = default_tags( it );
    fact.flag = default_flags( it );
    fact.qual = default_qualities( it );
    fact.mat = it.made_of();
    fact.vit = default_vitamins( it );
    fact.mass_g = static_cast<int>( units::to_gram( it.weight() ) );
    fact.volume_ml = units::to_milliliter( it.volume() );
    fact.kcal = it.is_comestible() ? it.get_comestible()->default_nutrition.kcal : 0;
    fact.hp = normalized_hp( it );
    fact.chg = it.count_by_charges() ? it.charges : 0;
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
        ret.push_back( normalize_part_fact( *it, ix ) );
        ix++;
    } );
    return ret;
}
