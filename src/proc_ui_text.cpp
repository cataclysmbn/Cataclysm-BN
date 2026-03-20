#include "proc_ui_text.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <vector>

#include "output.h"
#include "string_formatter.h"

namespace
{

auto display_label( const proc::counted_label &entry ) -> std::string
{
    if( entry.count <= 1 ) {
        return entry.label;
    }
    return string_format( "%s x%d", entry.label, entry.count );
}

} // namespace

auto proc::group_duplicate_labels( const std::vector<std::string> &labels ) -> std::vector<counted_label>
{
    auto ret = std::vector<counted_label> {};
    for( const auto &label : labels ) {
        const auto iter = std::ranges::find_if( ret, [&]( const counted_label & entry ) {
            return entry.label == label;
        } );
        if( iter == ret.end() ) {
            ret.push_back( counted_label{ .label = label, .count = 1 } );
            continue;
        }
        iter->count++;
    }
    return ret;
}

auto proc::grouped_label_summary( const std::vector<std::string> &labels,
                                  const std::string &empty_label ) -> std::string
{
    const auto grouped = group_duplicate_labels( labels );
    if( grouped.empty() ) {
        return empty_label;
    }

    auto display = std::vector<std::string> {};
    display.reserve( grouped.size() );
    std::ranges::transform( grouped, std::back_inserter( display ), []( const counted_label & entry ) {
        return display_label( entry );
    } );
    return enumerate_as_string( display, enumeration_conjunction::none );
}

auto proc::grouped_label_lines( const std::vector<std::string> &labels ) -> std::vector<std::string>
{
    auto ret = std::vector<std::string> {};
    const auto grouped = group_duplicate_labels( labels );
    ret.reserve( grouped.size() );
    std::ranges::transform( grouped, std::back_inserter( ret ), []( const counted_label & entry ) {
        return string_format( "- %s", display_label( entry ) );
    } );
    return ret;
}
