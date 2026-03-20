#include "proc_builder.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "material.h"
#include "output.h"
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
    if( cmp_pos == std::string::npos ) {
        return false;
    }
    const auto qual = quality_id( rhs.substr( 0, cmp_pos ) );
    const auto level = std::stoi( rhs.substr( cmp_pos + 2 ) );
    const auto iter = fact.qual.find( qual );
    return iter != fact.qual.end() && iter->second >= level;
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

auto fact_has_tag( const proc::part_fact &fact, const std::string &tag ) -> bool
{
    return std::ranges::find( fact.tag, tag ) != fact.tag.end();
}

auto facts_have_tag( const std::vector<proc::part_fact> &facts, const std::string &tag ) -> bool
{
    return std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
        return fact_has_tag( fact, tag );
    } );
}

auto sandwich_condiment_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    if( !facts_have_tag( facts, "cond" ) ) {
        return {};
    }

    static const auto named_condiments = std::array<std::pair<itype_id, std::string>, 9> {{
            { itype_id( "mustard" ), "mustard sandwich" },
            { itype_id( "ketchup" ), "ketchup sandwich" },
            { itype_id( "mayonnaise" ), "mayonnaise sandwich" },
            { itype_id( "horseradish" ), "horseradish sandwich" },
            { itype_id( "butter" ), "butter sandwich" },
            { itype_id( "sauerkraut" ), "sauerkraut sandwich" },
            { itype_id( "soysauce" ), "soy sauce sandwich" },
            { itype_id( "sauce_pesto" ), "pesto sandwich" },
            { itype_id( "sauce_red" ), "red sauce sandwich" },
        }
    };
    const auto named_condiment = std::ranges::find_if( named_condiments, [&]( const auto & entry ) {
        return std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
            return fact.id == entry.first;
        } );
    } );
    if( named_condiment != named_condiments.end() ) {
        return named_condiment->second;
    }
    return "sauce sandwich";
}

auto sandwich_vegetable_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) { return fact.id == itype_id( "cucumber" ); } ) ) {
        return "cucumber sandwich";
    }
    return "vegetable sandwich";
}

auto sword_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "steel" ) ) || has_material( fact, material_id( "iron" ) );
    } ) ) {
        return "forged sword";
    }
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "bone" ) );
    } ) ) {
        return "bone sword";
    }
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return has_material( fact, material_id( "wood" ) );
    } ) ) {
        return "wood sword";
    }
    return "sword";
}

auto sandwich_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    const auto has_fish = std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
        return has_material( fact, material_id( "fish" ) ) || fact.id.str().contains( "fish" );
    } );
    const auto has_meat = facts_have_tag( facts, "meat" );
    const auto has_cheese = facts_have_tag( facts, "cheese" );
    const auto has_veg = facts_have_tag( facts, "veg" );
    const auto has_cond = facts_have_tag( facts, "cond" );

    if( has_fish ) {
        return "fish sandwich";
    }
    if( has_meat && has_cheese && has_veg && has_cond ) {
        return "deluxe sandwich";
    }
    if( has_meat ) {
        return "meat sandwich";
    }
    if( has_cheese ) {
        return "cheese sandwich";
    }
    if( has_veg ) {
        return sandwich_vegetable_name( facts );
    }
    if( has_cond ) {
        return sandwich_condiment_name( facts );
    }
    return "sandwich";
}

auto stew_name( const std::vector<proc::part_fact> &facts ) -> std::string
{
    if( std::ranges::any_of( facts, [&]( const proc::part_fact & fact ) {
    return std::ranges::find( fact.tag, "meat" ) != fact.tag.end();
    } ) ) {
        return "meat stew";
    }
    return "vegetable stew";
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
    blob.name = sword_name( facts );
    return blob;
}

auto preview_blob( const proc::schema &sch, const std::vector<proc::part_fact> &facts,
                   const std::vector<proc::craft_pick> &picks ) -> proc::fast_blob
{
    auto blob = basic_preview( sch, facts, picks );
    if( sch.id == proc::schema_id( "sandwich" ) ) {
        blob.name = sandwich_name( facts );
        return blob;
    }
    if( sch.id == proc::schema_id( "stew" ) ) {
        blob.name = stew_name( facts );
        return blob;
    }
    if( sch.id == proc::schema_id( "sword" ) || sch.cat == "weapon" ) {
        return sword_preview( sch, facts, picks );
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
    if( lhs == "tag" ) {
        return std::ranges::find( fact.tag, rhs ) != fact.tag.end();
    }
    if( lhs == "flag" ) {
        return std::ranges::find( fact.flag, flag_id( rhs ) ) != fact.flag.end();
    }
    if( lhs == "mat" ) {
        return std::ranges::find( fact.mat, material_id( rhs ) ) != fact.mat.end();
    }
    if( lhs == "itype" ) {
        return fact.id == itype_id( rhs );
    }
    if( lhs == "qual" ) {
        return quality_match( fact, rhs );
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
                return lcmatch( entry, term );
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
