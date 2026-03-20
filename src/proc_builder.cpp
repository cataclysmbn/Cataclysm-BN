#include "proc_builder.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <ranges>
#include <string>
#include <vector>

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

auto join_none( const std::vector<std::string> &values ) -> std::string
{
    auto ret = std::string{};
    std::ranges::for_each( values, [&]( const std::string & value ) {
        ret += value;
    } );
    return ret;
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

auto proc::selected_facts( const builder_state &state ) -> std::vector<part_fact>
{
    auto ret = std::vector<part_fact> {};
    std::ranges::for_each( state.chosen, [&]( const std::pair<const slot_id, std::vector<part_ix>>
    &entry ) {
        std::ranges::for_each( entry.second, [&]( const part_ix ix ) {
            const auto iter = std::ranges::find_if( state.facts, [&]( const part_fact & fact ) {
                return fact.ix == ix;
            } );
            if( iter != state.facts.end() ) {
                ret.push_back( *iter );
            }
        } );
    } );
    return ret;
}

auto proc::rebuild_fast( const builder_state &state ) -> fast_blob
{
    auto blob = fast_blob{};
    auto role_counts = std::vector<std::string> {};
    std::ranges::for_each( state.chosen, [&]( const std::pair<const slot_id, std::vector<part_ix>>
    &entry ) {
        role_counts.push_back( role_count_name( entry.first.str(),
                                                static_cast<int>( entry.second.size() ) ) );
    } );
    const auto facts = selected_facts( state );
    std::ranges::for_each( facts, [&]( const part_fact & fact ) {
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

auto proc::build_state( const schema &sch, const std::vector<part_fact> &facts ) -> builder_state
{
    auto state = builder_state{};
    state.id = sch.id;
    state.facts = facts;
    state.cand = build_candidates( sch, facts );
    state.fast = rebuild_fast( state );
    return state;
}

auto proc::add_pick( builder_state &state, const schema &sch, const slot_id &slot,
                     const part_ix ix ) -> bool
{
    const auto slot_iter = std::ranges::find_if( sch.slots, [&]( const slot_data & entry ) {
        return entry.id == slot;
    } );
    if( slot_iter == sch.slots.end() ) {
        return false;
    }

    auto &picked = state.chosen[slot];
    if( !slot_iter->rep && std::ranges::find( picked, ix ) != picked.end() ) {
        return false;
    }
    if( slot_iter->max > 0 && static_cast<int>( picked.size() ) >= slot_iter->max ) {
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
    const auto hash = std::hash<std::string> {}( string_format( "%s|%d|%d|%d|%s", sch.id.str(),
                      blob.mass_g,
                      blob.volume_ml, blob.kcal, joined ) );
    return string_format( "%s:%08x", sch.id.str(), static_cast<unsigned int>( hash & 0xffffffffU ) );
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
    return std::ranges::any_of( texts, [&]( const std::string & entry ) {
        return lcmatch( entry, needle );
    } );
}
