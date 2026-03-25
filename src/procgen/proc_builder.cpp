#include "procgen/proc_builder.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "item.h"
#include "material.h"
#include "output.h"
#include "procgen/proc_fact.h"
#include "procgen/proc_item.h"
#include "recipe.h"
#include "string_formatter.h"
#include "string_utils.h"

namespace
{

auto role_count_name( const std::string &role, const int count ) -> std::string
{
    return string_format( "%s:%d", role, count );
}

auto split_atom( const std::string &atom ) -> std::pair<std::string, std::string>
{
    const auto pos = atom.find( ':' );
    if( pos == std::string::npos ) {
        return { atom, "" };
    }
    return { atom.substr( 0, pos ), atom.substr( pos + 1 ) };
}

auto quality_match( const proc::part_fact &fact, const std::string &rhs ) -> bool
{
    const auto cmp_pos = rhs.find( ">=" );
    if( cmp_pos == std::string::npos || cmp_pos == 0 ) {
        return false;
    }
    const auto qual = quality_id( rhs.substr( 0, cmp_pos ) );
    const auto level_txt = rhs.substr( cmp_pos + 2 );
    if( level_txt.empty() ) {
        return false;
    }
    auto level = 0;
    const auto parse = std::from_chars( level_txt.data(), level_txt.data() + level_txt.size(), level );
    if( parse.ec != std::errc() || parse.ptr != level_txt.data() + level_txt.size() ) {
        return false;
    }
    return std::ranges::any_of( fact.qual, [&]( const std::pair<const quality_id, int> &entry ) {
        return to_lower_case( entry.first.str() ) == to_lower_case( qual.str() ) && entry.second >= level;
    } );
}

auto is_search_atom( const std::string &query ) -> bool
{
    const auto atom_prefixes = std::vector<std::string> { "tag:", "flag:", "mat:", "itype:", "qual:" };
    return std::ranges::any_of( atom_prefixes, [&]( const std::string & prefix ) {
        return query.starts_with( prefix );
    } );
}

auto split_search_terms( const std::string &query ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    auto input = std::istringstream( to_lower_case( query ) );
    for( auto term = std::string{}; input >> term; ) {
        ret.push_back( term );
    }
    return ret;
}

auto join_none( const std::vector<std::string> &values ) -> std::string
{
    auto ret = std::string{};
    std::ranges::for_each( values, [&]( const std::string & value ) {
        ret += value;
    } );
    return ret;
}

auto slot_by_id( const proc::schema &sch,
                 const proc::slot_id &slot ) -> const proc::slot_data * // *NOPAD*
{
    const auto iter = std::ranges::find_if( sch.slots, [&]( const proc::slot_data & entry ) {
        return entry.id == slot;
    } );
    return iter == sch.slots.end() ? nullptr : &*iter;
}

auto find_fact( const std::vector<proc::part_fact> &facts,
                const proc::part_ix ix ) -> const proc::part_fact * // *NOPAD*
{
    const auto iter = std::ranges::find_if( facts, [&]( const proc::part_fact & fact ) {
        return fact.ix == ix;
    } );
    return iter == facts.end() ? nullptr : &*iter;
}

template<typename Func>
auto average_material_stat( const proc::part_fact &fact, Func fn ) -> int
{
    if( fact.mat.empty() ) {
        return 0;
    }
    auto total = 0;
    std::ranges::for_each( fact.mat, [&]( const material_id & mat ) {
        if( mat.is_valid() ) {
            total += fn( mat.obj() );
        }
    } );
    return total / static_cast<int>( fact.mat.size() );
}

auto fact_density( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.density();
    } );
}

auto fact_bash_resist( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.bash_resist();
    } );
}

auto fact_cut_resist( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.cut_resist();
    } );
}

auto fact_chip_resist( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.chip_resist();
    } );
}

auto fact_bullet_resist( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.bullet_resist();
    } );
}

auto fact_reinforce_bonus( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.reinforces() ? 8 : 0;
    } );
}

auto fact_soft_penalty( const proc::part_fact &fact ) -> int
{
    return average_material_stat( fact, []( const material_type & mat ) {
        return mat.soft() ? 8 : 0;
    } );
}

struct candidate_sort_entry {
    proc::part_ix ix = proc::invalid_part_ix;
    int score = 0;
    int material_rank = 0;
    int uses = 0;
    int mass_g = 0;
    int volume_ml = 0;
    std::string id;
};

auto blade_material_rank( const proc::part_fact &fact ) -> int
{
    if( std::ranges::find( fact.mat, material_id( "steel" ) ) != fact.mat.end() ||
        std::ranges::find( fact.mat, material_id( "iron" ) ) != fact.mat.end() ) {
        return 3;
    }
    if( std::ranges::find( fact.mat, material_id( "bone" ) ) != fact.mat.end() ) {
        return 2;
    }
    if( std::ranges::find( fact.mat, material_id( "wood" ) ) != fact.mat.end() ) {
        return 1;
    }
    return 0;
}

auto weapon_role_score( const std::string &role, const proc::part_fact &fact ) -> int
{
    const auto density = fact_density( fact );
    const auto bash_resist = fact_bash_resist( fact );
    const auto cut_resist = fact_cut_resist( fact );
    const auto chip_resist = fact_chip_resist( fact );
    const auto bullet_resist = fact_bullet_resist( fact );
    const auto reinforce_bonus = fact_reinforce_bonus( fact );
    const auto soft_penalty = fact_soft_penalty( fact );

    if( role == "blade" ) {
        return density / 8 + cut_resist * 6 + chip_resist * 3 + bullet_resist * 2 +
               reinforce_bonus * 5 - soft_penalty * 4 + fact.mass_g / 120;
    }
    if( role == "guard" ) {
        return bash_resist * 5 + chip_resist * 3 + cut_resist * 2 + density / 10 +
               reinforce_bonus * 4 - soft_penalty * 3 + fact.mass_g / 180;
    }
    if( role == "handle" ) {
        return bash_resist * 3 + chip_resist * 2 + density / 14 + reinforce_bonus * 2 -
               soft_penalty * 2 + fact.mass_g / 220;
    }
    if( role == "grip" ) {
        return bash_resist * 3 + cut_resist * 2 + chip_resist * 2 + density / 12 +
               reinforce_bonus * 2 - soft_penalty * 3 + fact.mass_g / 260;
    }
    if( role == "reinforcement" ) {
        return reinforce_bonus * 8 + cut_resist * 3 + chip_resist * 3 + bash_resist * 2 +
               density / 10 - soft_penalty * 2 + fact.mass_g / 140;
    }
    return bash_resist * 2 + cut_resist * 2 + chip_resist * 2 + density / 16 +
           reinforce_bonus * 2 - soft_penalty * 2 + fact.mass_g / 200;
}

auto build_candidate_sort_entry( const proc::slot_data &slot,
                                 const proc::part_fact &fact ) -> candidate_sort_entry
{
    auto score = weapon_role_score( slot.role, fact );
    if( slot.role == "blade" &&
        std::ranges::find( fact.mat, material_id( "bone" ) ) != fact.mat.end() ) {
        score += 8;
    }
    return candidate_sort_entry{
        .ix = fact.ix,
        .score = score,
        .material_rank = slot.role == "blade" ? blade_material_rank( fact ) : 0,
        .uses = fact.uses,
        .mass_g = fact.mass_g,
        .volume_ml = fact.volume_ml,
        .id = fact.id.str(),
    };
}

auto sort_weapon_slot_candidates( const proc::slot_data &slot,
                                  const std::vector<proc::part_fact> &facts,
                                  std::vector<proc::part_ix> &slot_facts ) -> void
{
    auto ranked = std::vector<candidate_sort_entry> {};
    ranked.reserve( slot_facts.size() );
    std::ranges::for_each( slot_facts, [&]( const proc::part_ix ix ) {
        const auto *fact = find_fact( facts, ix );
        if( fact == nullptr ) {
            return;
        }
        ranked.push_back( build_candidate_sort_entry( slot, *fact ) );
    } );

    const auto prefer_material_rank = slot.role == "blade";
    std::ranges::sort( ranked, [&]( const candidate_sort_entry & lhs,
    const candidate_sort_entry & rhs ) {
        if( prefer_material_rank && lhs.material_rank != rhs.material_rank ) {
            return lhs.material_rank > rhs.material_rank;
        }
        if( lhs.score != rhs.score ) {
            return lhs.score > rhs.score;
        }
        if( !prefer_material_rank && lhs.material_rank != rhs.material_rank ) {
            return lhs.material_rank > rhs.material_rank;
        }
        if( lhs.uses != rhs.uses ) {
            return lhs.uses > rhs.uses;
        }
        if( lhs.mass_g != rhs.mass_g ) {
            return lhs.mass_g > rhs.mass_g;
        }
        if( lhs.volume_ml != rhs.volume_ml ) {
            return lhs.volume_ml > rhs.volume_ml;
        }
        if( lhs.id != rhs.id ) {
            return lhs.id < rhs.id;
        }
        return lhs.ix < rhs.ix;
    } );

    slot_facts.clear();
    slot_facts.reserve( ranked.size() );
    std::ranges::transform( ranked, std::back_inserter( slot_facts ),
    []( const candidate_sort_entry & entry ) {
        return entry.ix;
    } );
}

auto role_count( const std::vector<proc::craft_pick> &picks, const proc::slot_id &slot ) -> int
{
    return static_cast<int>( std::ranges::count_if( picks, [&]( const proc::craft_pick & pick ) {
        return pick.slot == slot;
    } ) );
}

auto has_material( const proc::part_fact &fact, const material_id &id ) -> bool
{
    return std::ranges::find( fact.mat, id ) != fact.mat.end();
}

auto has_itype( const std::vector<proc::part_fact> &facts, const itype_id &id ) -> bool
{
    return std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
        return fact.id == id;
    } );
}

auto join_sentences( const std::vector<std::string> &sentences ) -> std::string
{
    auto ret = std::string {};
    std::ranges::for_each( sentences, [&]( const std::string & sentence ) {
        if( sentence.empty() ) {
            return;
        }
        if( !ret.empty() ) {
            ret += ' ';
        }
        ret += sentence;
    } );
    return ret;
}

auto primary_material_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "steel" ) ) || has_material( fact, material_id( "iron" ) );
    } ) ) {
        return "metal";
    }
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "bone" ) );
    } ) ) {
        return "bone";
    }
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "wood" ) );
    } ) ) {
        return "wooden";
    }
    return "simple";
}

auto sword_base_description( const std::string &name ) -> std::string
{
    if( name == "nail sword" ) {
        return "A rough wooden sword stiffened with driven nails.";
    }
    if( name == "crude sword" ) {
        return "A crude sword pieced together from wood and scavenged scrap.";
    }
    if( name == "hand-forged sword" ) {
        return "A serviceable sword built around a forged metal blade.";
    }
    if( name == "bone sword" ) {
        return "A rough sword built around a sharpened bone blade.";
    }
    if( name == "2-by-sword" ) {
        return "A club-like sword carved from a sturdy length of wood.";
    }
    return "A makeshift sword assembled from scavenged parts.";
}

auto sword_guard_phrase( const std::vector<proc::part_fact> &guard_facts ) -> std::string
{
    if( guard_facts.empty() ) {
        return "no guard";
    }
    const auto material = primary_material_name( guard_facts );
    if( material == "metal" ) {
        return "a metal guard";
    }
    if( material == "bone" ) {
        return "a bone guard";
    }
    if( material == "wooden" ) {
        return "a wooden guard";
    }
    return "a simple guard";
}

auto sword_grip_phrase( const std::vector<proc::part_fact> &grip_facts ) -> std::string
{
    if( grip_facts.empty() ) {
        return {};
    }
    if( has_itype( grip_facts, itype_id( "leather" ) ) || std::ranges::any_of( grip_facts,
    [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "leather" ) );
    } ) ) {
        return "a leather-wrapped grip";
    }
    if( has_itype( grip_facts, itype_id( "rag" ) ) || std::ranges::any_of( grip_facts,
    [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "cotton" ) );
    } ) ) {
        return "a rag-wrapped grip";
    }
    return "a wrapped grip";
}

auto sword_hilt_sentence( const std::vector<proc::part_fact> &guard_facts,
                          const std::vector<proc::part_fact> &grip_facts ) -> std::string
{
    const auto guard_phrase = sword_guard_phrase( guard_facts );
    const auto grip_phrase = sword_grip_phrase( grip_facts );
    if( grip_phrase.empty() ) {
        return string_format( "The hilt uses %s.", guard_phrase );
    }
    return string_format( "The hilt uses %s and %s.", guard_phrase, grip_phrase );
}

auto sword_reinforcement_sentence( const std::vector<proc::part_fact> &reinforcement_facts ) ->
std::string
{
    const auto has_nails = has_itype( reinforcement_facts, itype_id( "nail" ) );
    const auto has_scrap = has_itype( reinforcement_facts, itype_id( "scrap" ) );
    if( has_nails && has_scrap ) {
        return "Driven nails and scrap reinforcement add stiffness at the cost of weight.";
    }
    if( has_nails ) {
        return "Driven nails add stiffness and a little puncturing power.";
    }
    if( has_scrap ) {
        return "Scrap reinforcement adds weight and stiffness.";
    }
    return {};
}

auto matching_slot_uses( const proc::schema &sch, const proc::part_fact &fact ) -> int
{
    auto uses = 0;
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        if( proc::matches_slot( slot, fact ) ) {
            uses += std::max( slot.max, 1 );
        }
    } );
    return uses;
}

auto picked_facts_for_role( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                            const std::vector<proc::craft_pick> &picks,
                            const std::string &role ) -> std::vector<proc::part_fact>
{
    auto ret = std::vector<proc::part_fact> {};
    std::ranges::for_each( picks, [&]( const proc::craft_pick & pick ) {
        const auto *slot = slot_by_id( sch, pick.slot );
        const auto *fact = find_fact( facts, pick.ix );
        if( slot == nullptr || fact == nullptr || slot->role != role ) {
            return;
        }
        ret.push_back( *fact );
    } );
    return ret;
}

struct sword_variant_data {
    std::string name;
    itype_id result = itype_id::NULL_ID();
};

auto sword_variant( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                    const std::vector<proc::craft_pick> &picks ) -> sword_variant_data
{
    const auto blade_facts = picked_facts_for_role( sch, facts, picks, "blade" );
    const auto reinforcement_facts = picked_facts_for_role( sch, facts, picks, "reinforcement" );

    if( has_itype( reinforcement_facts, itype_id( "nail" ) ) ) {
        return { .name = "nail sword", .result = itype_id( "sword_nail" ) };
    }
    if( has_itype( reinforcement_facts, itype_id( "scrap" ) ) ||
        has_itype( blade_facts, itype_id( "scrap" ) ) ) {
        return { .name = "crude sword", .result = itype_id( "sword_crude" ) };
    }
    if( std::ranges::any_of( blade_facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "steel" ) ) || has_material( fact,
                material_id( "iron" ) );
    } ) ) {
        return { .name = "hand-forged sword", .result = itype_id( "sword_metal" ) };
    }
    if( std::ranges::any_of( blade_facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "bone" ) );
    } ) ) {
        return { .name = "bone sword", .result = itype_id( "sword_bone" ) };
    }
    if( std::ranges::any_of( blade_facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "wood" ) );
    } ) ) {
        return { .name = "2-by-sword", .result = itype_id( "sword_wood" ) };
    }
    return { .name = "sword", .result = itype_id( "proc_sword_generic" ) };
}

auto sword_name( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                 const std::vector<proc::craft_pick> &picks ) -> std::string
{
    return sword_variant( sch, facts, picks ).name;
}

auto sword_description( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                        const std::vector<proc::craft_pick> &picks,
                        const std::string &name ) -> std::string
{
    const auto guard_facts = picked_facts_for_role( sch, facts, picks, "guard" );
    const auto grip_facts = picked_facts_for_role( sch, facts, picks, "grip" );
    const auto reinforcement_facts = picked_facts_for_role( sch, facts, picks, "reinforcement" );
    return join_sentences( {
        sword_base_description( name ),
        sword_hilt_sentence( guard_facts, grip_facts ),
        sword_reinforcement_sentence( reinforcement_facts ),
    } );
}

auto basic_preview( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                    const std::vector<proc::craft_pick> &picks ) -> proc::fast_blob
{
    auto blob = proc::fast_blob{};
    auto role_counts = std::vector<std::string> {};
    std::ranges::for_each( sch.slots, [&]( const proc::slot_data & slot ) {
        const auto count = role_count( picks, slot.id );
        if( count > 0 ) {
            role_counts.push_back( role_count_name( slot.role, count ) );
        }
    } );
    std::ranges::for_each( facts, [&]( const proc::part_fact & fact ) {
        blob.mass_g += fact.mass_g;
        blob.volume_ml += fact.volume_ml;
        blob.kcal += fact.kcal;
        std::ranges::for_each( fact.vit, [&]( const std::pair<const vitamin_id, int> &entry ) {
            blob.vit[entry.first] += entry.second;
        } );
    } );
    blob.name = join_none( role_counts );
    return blob;
}

auto sword_preview( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                    const std::vector<proc::craft_pick> &picks ) -> proc::fast_blob
{
    auto blob = basic_preview( sch, facts, picks );
    auto blade_mass = 0;
    auto handle_mass = 0;
    auto support_mass = 0;
    auto edge_score = 0;
    auto point_score = 0;
    auto bash_score = 0;
    auto dur_score = 0;

    std::ranges::for_each( picks, [&]( const proc::craft_pick & pick ) {
        const auto *slot = slot_by_id( sch, pick.slot );
        const auto *fact = find_fact( facts, pick.ix );
        if( slot == nullptr || fact == nullptr ) {
            return;
        }
        const auto density = fact_density( *fact );
        const auto bash_resist = fact_bash_resist( *fact );
        const auto cut_resist = fact_cut_resist( *fact );
        const auto chip_resist = fact_chip_resist( *fact );
        const auto bullet_resist = fact_bullet_resist( *fact );
        const auto reinforce_bonus = fact_reinforce_bonus( *fact );
        const auto soft_penalty = fact_soft_penalty( *fact );
        if( slot->role == "blade" ) {
            blade_mass += fact->mass_g;
            edge_score += density + cut_resist * 2 + chip_resist + reinforce_bonus - soft_penalty;
            point_score += density + cut_resist + bullet_resist + chip_resist - soft_penalty;
        } else if( slot->role == "reinforcement" ) {
            support_mass += fact->mass_g;
            bash_score += bash_resist + density / 2 + reinforce_bonus - soft_penalty / 2;
            edge_score += density / 2 + cut_resist + reinforce_bonus * 2 - soft_penalty;
            point_score += density / 3 + cut_resist + reinforce_bonus - soft_penalty;
        } else if( slot->role == "handle" || slot->role == "grip" ) {
            handle_mass += fact->mass_g;
            bash_score += bash_resist + density + reinforce_bonus - soft_penalty / 2;
        } else {
            support_mass += fact->mass_g;
            bash_score += bash_resist + density + reinforce_bonus - soft_penalty / 2;
        }
        dur_score += bash_resist + chip_resist * 2 + reinforce_bonus - soft_penalty;
    } );

    blob.melee.bash = std::max( 1, blob.mass_g / 220 + bash_score / 4 + support_mass / 180 );
    blob.melee.cut = std::max( 0, blade_mass / 55 + edge_score / 2 );
    blob.melee.stab = std::max( 0, blade_mass / 80 + point_score / 3 + handle_mass / 300 );
    blob.melee.to_hit = std::clamp( ( edge_score + bash_score ) / 18 - blob.mass_g / 900, -2, 4 );
    blob.melee.dur = std::max( 1, dur_score + blob.mass_g / 250 );
    blob.name = sword_name( sch, facts, picks );
    blob.description = sword_description( sch, facts, picks, blob.name );
    return blob;
}

auto preview_blob( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                   const std::vector<proc::craft_pick> &picks ) -> proc::fast_blob
{
    auto blob = sch.id == proc::schema_id( "sword" ) || sch.cat == "weapon" ?
                sword_preview( sch, facts, picks ) : basic_preview( sch, facts, picks );
    if( !sch.lua.full.empty() || !sch.lua.name.empty() ) {
        return proc::run_full( sch, facts, blob, { .picks = picks } ).data;
    }
    return blob;
}

} // namespace

auto proc::builder_state::picks_for( const slot_id &slot ) const -> const
std::vector<part_ix>& // *NOPAD*
{
    static const auto empty = std::vector<part_ix> {};
    const auto iter = chosen.find( slot );
    return iter == chosen.end() ? empty : iter->second;
}

auto proc::matches_atom( const part_fact &fact, const std::string &atom ) -> bool
{
    const auto [lhs, rhs] = split_atom( atom );
    const auto normalized_rhs = to_lower_case( rhs );
    if( lhs == "tag" ) {
        return std::ranges::any_of( fact.tag, [&]( const std::string & tag ) {
            return to_lower_case( tag ) == normalized_rhs;
        } );
    }
    if( lhs == "flag" ) {
        return std::ranges::any_of( fact.flag, [&]( const flag_id & flag ) {
            return to_lower_case( flag.str() ) == normalized_rhs;
        } );
    }
    if( lhs == "mat" ) {
        return std::ranges::any_of( fact.mat, [&]( const material_id & mat ) {
            return to_lower_case( mat.str() ) == normalized_rhs;
        } );
    }
    if( lhs == "itype" ) {
        return to_lower_case( fact.id.str() ) == normalized_rhs;
    }
    if( lhs == "qual" ) {
        return quality_match( fact, normalized_rhs );
    }
    return false;
}

auto proc::matches_slot( const slot_data &slot, const part_fact &fact ) -> bool
{
    const auto allow_ok = slot.ok.empty() ||
    std::ranges::any_of( slot.ok, [&]( const std::string & atom ) {
        return matches_atom( fact, atom );
    } );
    if( !allow_ok ) {
        return false;
    }
    return !std::ranges::any_of( slot.no, [&]( const std::string & atom ) {
        return matches_atom( fact, atom );
    } );
}

auto proc::build_candidates( const schema &sch,
                             const std::vector<part_fact> &facts ) -> std::map<slot_id, std::vector<part_ix>>
{
    auto ret = std::map<slot_id, std::vector<part_ix>> {};
    std::ranges::for_each( sch.slots, [&]( const slot_data & slot ) {
        auto slot_facts = std::vector<part_ix> {};
        std::ranges::for_each( facts, [&]( const part_fact & fact ) {
            if( matches_slot( slot, fact ) ) {
                slot_facts.push_back( fact.ix );
            }
        } );
        if( sch.id == proc::schema_id( "sword" ) || sch.cat == "weapon" ) {
            sort_weapon_slot_candidates( slot, facts, slot_facts );
        }
        ret.emplace( slot.id, std::move( slot_facts ) );
    } );
    return ret;
}

auto proc::pick_count( const builder_state &state, const part_ix ix ) -> int
{
    auto total = 0;
    std::ranges::for_each( state.chosen, [&]( const std::pair<const slot_id, std::vector<part_ix>>
    &entry ) {
        total += static_cast<int>( std::ranges::count( entry.second, ix ) );
    } );
    return total;
}

auto proc::remaining_uses( const builder_state &state, const part_ix ix ) -> int
{
    const auto *fact = find_fact( state.facts, ix );
    if( fact == nullptr ) {
        return 0;
    }
    return std::max( fact->uses - pick_count( state, ix ), 0 );
}

auto proc::filter_available_candidates( const builder_state &state,
                                        const std::vector<part_ix> &candidates ) -> std::vector<part_ix>
{
    auto ret = std::vector<part_ix> {};
    ret.reserve( candidates.size() );
    std::ranges::for_each( candidates, [&]( const part_ix ix ) {
        if( remaining_uses( state, ix ) > 0 ) {
            ret.push_back( ix );
        }
    } );
    return ret;
}

auto proc::slot_can_meet_minimum( const builder_state &state, const schema &sch,
                                  const slot_id &slot ) -> bool
{
    const auto *slot_data = slot_by_id( sch, slot );
    if( slot_data == nullptr ) {
        return false;
    }
    if( slot_data->min <= 0 ) {
        return true;
    }
    const auto iter = state.cand.find( slot );
    if( iter == state.cand.end() ) {
        return false;
    }

    auto total_uses = 0;
    std::ranges::for_each( iter->second, [&]( const part_ix ix ) {
        total_uses += remaining_uses( state, ix );
    } );
    return total_uses >= slot_data->min;
}

auto proc::slot_complete( const builder_state &state, const schema &sch,
                          const slot_id &slot ) -> bool
{
    const auto *slot_data = slot_by_id( sch, slot );
    if( slot_data == nullptr ) {
        return false;
    }
    return static_cast<int>( state.picks_for( slot ).size() ) >= slot_data->min;
}

auto proc::complete( const builder_state &state, const schema &sch ) -> bool
{
    return std::ranges::all_of( sch.slots, [&]( const slot_data & slot ) {
        return slot_complete( state, sch, slot.id );
    } );
}

auto proc::selected_picks( const builder_state &state,
                           const schema &sch ) -> std::vector<craft_pick>
{
    auto ret = std::vector<craft_pick> {};
    std::ranges::for_each( sch.slots, [&]( const slot_data & slot ) {
        const auto &picked = state.picks_for( slot.id );
        std::ranges::for_each( picked, [&]( const part_ix ix ) {
            ret.push_back( craft_pick{ .slot = slot.id, .ix = ix } );
        } );
    } );
    return ret;
}

auto proc::selected_facts( const builder_state &state ) -> std::vector<part_fact>
{
    auto ret = std::vector<part_fact> {};
    if( !state.sch.id.is_null() ) {
        const auto picks = selected_picks( state, state.sch );
        std::ranges::for_each( picks, [&]( const craft_pick & pick ) {
            if( const auto *fact = find_fact( state.facts, pick.ix ) ) {
                ret.push_back( *fact );
            }
        } );
        return ret;
    }

    std::ranges::for_each( state.chosen, [&]( const std::pair<const slot_id, std::vector<part_ix>>
    &entry ) {
        std::ranges::for_each( entry.second, [&]( const part_ix ix ) {
            if( const auto *fact = find_fact( state.facts, ix ) ) {
                ret.push_back( *fact );
            }
        } );
    } );
    return ret;
}

auto proc::rebuild_fast( const builder_state &state ) -> fast_blob
{
    const auto facts = selected_facts( state );
    if( state.sch.id.is_null() ) {
        return fast_blob{};
    }
    return preview_blob( state.sch, facts, selected_picks( state, state.sch ) );
}

auto proc::preview_result_override( const schema &sch, const std::vector<part_fact> &facts,
                                    const std::vector<craft_pick> &picks ) -> std::optional<itype_id>
{
    if( sch.id != schema_id( "sword" ) ) {
        return std::nullopt;
    }
    return sword_variant( sch, facts, picks ).result;
}

auto proc::debug_part_fact( const schema &sch, const item &it,
                            const part_ix ix ) -> std::optional<part_fact>
{
    auto fact = normalize_part_fact( it, {
        .ix = ix,
        .charges = it.count_by_charges() ? 1 : 0,
        .uses = 1,
    } );
    const auto uses = matching_slot_uses( sch, fact );
    if( uses <= 0 ) {
        return std::nullopt;
    }
    fact.uses = uses;
    return fact;
}

auto proc::build_state( const schema &sch, const std::vector<part_fact> &facts ) -> builder_state
{
    auto state = builder_state{};
    state.id = sch.id;
    state.sch = sch;
    state.facts = facts;
    state.cand = build_candidates( sch, facts );
    state.fast = rebuild_fast( state );
    return state;
}

auto proc::add_pick( builder_state &state, const schema &sch, const slot_id &slot,
                     const part_ix ix ) -> bool
{
    const auto *slot_data = slot_by_id( sch, slot );
    if( slot_data == nullptr ) {
        return false;
    }
    const auto cand_iter = state.cand.find( slot );
    if( cand_iter == state.cand.end() ||
        std::ranges::find( cand_iter->second, ix ) == cand_iter->second.end() ) {
        return false;
    }
    const auto *fact = find_fact( state.facts, ix );
    if( fact == nullptr || pick_count( state, ix ) >= fact->uses ) {
        return false;
    }

    auto &picked = state.chosen[slot];
    if( !slot_data->rep && std::ranges::find( picked, ix ) != picked.end() ) {
        return false;
    }
    if( slot_data->max > 0 && static_cast<int>( picked.size() ) >= slot_data->max ) {
        return false;
    }

    picked.push_back( ix );
    state.fast = rebuild_fast( state );
    return true;
}

auto proc::remove_pick( builder_state &state, const slot_id &slot, const part_ix ix ) -> bool
{
    const auto iter = state.chosen.find( slot );
    if( iter == state.chosen.end() ) {
        return false;
    }
    const auto erase_iter = std::ranges::find( iter->second, ix );
    if( erase_iter == iter->second.end() ) {
        return false;
    }
    iter->second.erase( erase_iter );
    state.fast = rebuild_fast( state );
    return true;
}

auto proc::remove_last_pick( builder_state &state, const slot_id &slot ) -> bool
{
    const auto iter = state.chosen.find( slot );
    if( iter == state.chosen.end() || iter->second.empty() ) {
        return false;
    }
    iter->second.pop_back();
    state.fast = rebuild_fast( state );
    return true;
}

auto proc::fast_fp( const schema &sch, const fast_blob &blob,
                    const std::vector<part_fact> &facts ) -> std::string
{
    auto ids = std::vector<std::string> {};
    ids.reserve( facts.size() );
    std::ranges::for_each( facts, [&]( const part_fact & fact ) {
        ids.push_back( string_format( "%s:%d:%d", fact.id.str(), fact.chg,
                                      static_cast<int>( fact.hp * 1000 ) ) );
    } );
    std::ranges::sort( ids );
    const auto joined = join_none( ids );
    const auto hash = std::hash<std::string> {}( string_format( "%s|%d|%d|%d|%d|%d|%d|%d|%d|%s",
                      sch.id.str(),
                      blob.mass_g,
                      blob.volume_ml, blob.kcal, blob.melee.bash, blob.melee.cut, blob.melee.stab,
                      blob.melee.to_hit, blob.melee.dur, joined ) );
    return string_format( "%s:%08x", sch.id.str(), static_cast<unsigned int>( hash & 0xffffffffU ) );
}

auto proc::part_search_texts( const part_fact &fact,
                              const proc::part_search_options &opts ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> { opts.name, fact.id.str(), string_format( "itype:%s", fact.id.str() ),
                                          opts.where
                                        };
    std::ranges::for_each( fact.tag, [&]( const std::string & tag ) {
        ret.push_back( tag );
        ret.push_back( string_format( "tag:%s", tag ) );
    } );
    std::ranges::for_each( fact.mat, [&]( const material_id & mat ) {
        ret.push_back( mat.str() );
        ret.push_back( string_format( "mat:%s", mat.str() ) );
    } );
    std::ranges::for_each( fact.flag, [&]( const flag_id & flag ) {
        ret.push_back( flag.str() );
        ret.push_back( string_format( "flag:%s", flag.str() ) );
    } );
    std::ranges::for_each( fact.qual, [&]( const std::pair<const quality_id, int> &entry ) {
        ret.push_back( entry.first.str() );
        ret.push_back( string_format( "qual:%s", entry.first.str() ) );
        ret.push_back( string_format( "qual:%s>=%d", entry.first.str(), entry.second ) );
    } );
    return ret;
}

auto proc::part_matches_search( const part_fact &fact, const proc::part_search_options &opts,
                                const std::string &txt ) -> bool
{
    if( txt.empty() ) {
        return true;
    }

    const auto texts = part_search_texts( fact, opts );
    const auto terms = split_search_terms( txt );
    return std::ranges::all_of( terms, [&]( const std::string & term ) {
        if( is_search_atom( term ) ) {
            return matches_atom( fact, term ) || std::ranges::any_of( texts, [&]( const std::string & entry ) {
                return to_lower_case( entry ) == term;
            } );
        }
        return std::ranges::any_of( texts, [&]( const std::string & entry ) {
            return lcmatch( entry, term );
        } );
    } );
}

auto proc::recipe_search_texts( const recipe &rec ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> { rec.result_name( true ) };
    if( !rec.is_proc() ) {
        return ret;
    }

    if( !rec.builder_name().translated().empty() ) {
        ret.push_back( rec.builder_name().translated() );
    }
    if( !rec.builder_desc().translated().empty() ) {
        ret.push_back( rec.builder_desc().translated() );
    }
    if( proc::has( rec.proc_id() ) ) {
        const auto &sch = proc::get( rec.proc_id() );
        std::ranges::for_each( sch.slots, [&]( const slot_data & slot ) {
            ret.push_back( slot.role );
            std::ranges::copy( slot.ok, std::back_inserter( ret ) );
        } );
    }
    return ret;
}

auto proc::recipe_matches_search( const recipe &rec, const std::string &txt ) -> bool
{
    auto needle = txt;
    if( needle.starts_with( "c:" ) ) {
        needle = needle.substr( 2 );
    }
    const auto texts = recipe_search_texts( rec );
    const auto terms = split_search_terms( needle );
    return std::ranges::all_of( terms, [&]( const std::string & term ) {
        return std::ranges::any_of( texts, [&]( const std::string & entry ) {
            return lcmatch( entry, term );
        } );
    } );
}
