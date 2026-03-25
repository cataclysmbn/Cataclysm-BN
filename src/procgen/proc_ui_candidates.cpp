#include "procgen/proc_ui_candidates.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

#include "string_formatter.h"

namespace
{

auto source_for_ix( const std::vector<proc::candidate_source_entry> &sources,
                    const proc::part_ix ix ) -> const proc::candidate_source_entry * // *NOPAD*
{
    const auto iter = std::ranges::find_if( sources, [&]( const proc::candidate_source_entry &
    source ) {
        return source.fact.ix == ix;
    } );
    return iter == sources.end() ? nullptr : &*iter;
}

} // namespace

auto proc::group_candidate_entries( const std::vector<candidate_label_entry> &entries ) ->
std::vector<grouped_candidate_entry>
{
    auto ret = std::vector<grouped_candidate_entry> {};
    for( const auto &entry : entries ) {
        const auto iter = std::find_if( ret.begin(),
        ret.end(), [&]( const grouped_candidate_entry & grouped ) {
            return grouped.key == entry.key;
        } );
        if( iter == ret.end() ) {
            ret.push_back( grouped_candidate_entry{
                .key = entry.key,
                .label = entry.label,
                .ixs = { entry.ix },
                .total_count = entry.count,
            } );
            continue;
        }
        iter->ixs.push_back( entry.ix );
        iter->total_count += entry.count;
    }
    return ret;
}

auto proc::filter_grouped_candidates( const builder_state &state, const slot_id &slot,
                                      const std::vector<candidate_source_entry> &sources,
                                      const std::string &query ) -> std::vector<grouped_candidate_entry>
{
    auto matches = std::vector<candidate_label_entry> {};
    const auto iter = state.cand.find( slot );
    if( iter == state.cand.end() ) {
        return {};
    }

    std::ranges::for_each( iter->second, [&]( const part_ix ix ) {
        const auto *source = source_for_ix( sources, ix );
        const auto remaining = remaining_uses( state, ix );
        if( source == nullptr || remaining <= 0 ||
        !part_matches_search( source->fact, {
        .name = source->name,
        .where = source->where,
    }, query ) ) {
            return;
        }
        matches.push_back( candidate_label_entry{
            .key = source->label,
            .label = source->label,
            .ix = ix,
            .count = remaining,
        } );
    } );
    return group_candidate_entries( matches );
}

auto proc::grouped_candidate_label( const grouped_candidate_entry &entry ) -> std::string
{
    if( entry.total_count <= 1 ) {
        return entry.label;
    }
    return string_format( "%s x%d", entry.label, entry.total_count );
}

auto proc::first_grouped_candidate_ix( const grouped_candidate_entry &entry ) -> part_ix
{
    if( entry.ixs.empty() ) {
        return invalid_part_ix;
    }
    return entry.ixs.front();
}
