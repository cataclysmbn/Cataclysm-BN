#include "proc_ui_candidates.h"

#include <algorithm>
#include <string>
#include <vector>

#include "string_formatter.h"

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
