#include "procgen/proc_ui_text.h"

#include <algorithm>
#include <cctype>
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

auto trim_copy( const std::string &text ) -> std::string
{
    const auto first = std::ranges::find_if_not( text, []( const unsigned char ch ) {
        return std::isspace( ch ) != 0;
    } );
    if( first == text.end() ) {
        return {};
    }
    const auto last = std::ranges::find_if_not( text.rbegin(),
    text.rend(), []( const unsigned char ch ) {
        return std::isspace( ch ) != 0;
    } ).base();
    return std::string( first, last );
}

auto join_sections( const std::vector<std::string> &sections ) -> std::string
{
    auto ret = std::string {};
    std::ranges::for_each( sections, [&]( const std::string & section ) {
        if( section.empty() ) {
            return;
        }
        if( !ret.empty() ) {
            ret += "  ";
        }
        ret += section;
    } );
    return ret;
}

} // namespace

auto proc::group_duplicate_labels( const std::vector<std::string> &labels ) ->
std::vector<counted_label>
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

auto proc::builder_readiness_label( const proc::builder_readiness readiness ) -> std::string
{
    switch( readiness ) {
        case proc::builder_readiness::missing_required_slots:
            return _( "Status: [ MISSING REQUIRED SLOTS ]" );
        case proc::builder_readiness::missing_recipe_requirements:
            return _( "Status: [ MISSING TOOLS OR QUALITIES ]" );
        case proc::builder_readiness::ready_to_craft:
            return _( "Status: [ READY TO CRAFT ]" );
    }
    return _( "Status: [ UNKNOWN ]" );
}

auto proc::compact_requirement_text( const std::string &text ) -> std::string
{
    auto sections = std::vector<std::string> {};
    auto header = std::string {};
    auto entries = std::vector<std::string> {};
    const auto flush_section = [&]() {
        if( header.empty() ) {
            return;
        }
        auto section = header;
        if( !entries.empty() ) {
            if( section.ends_with( ':' ) ) {
                section += ' ';
            } else {
                section += ": ";
            }
        }
        for( auto i = size_t{ 0 }; i < entries.size(); i++ ) {
            if( i > 0 ) {
                section += "; ";
            }
            section += entries[i];
        }
        sections.push_back( trim_copy( section ) );
        header.clear();
        entries.clear();
    };

    auto current = std::string {};
    for( const char ch : text ) {
        if( ch == '\n' ) {
            const auto line = trim_copy( current );
            current.clear();
            if( line.empty() ) {
                continue;
            }
            if( line.ends_with( ':' ) ) {
                if( !header.empty() && header != line ) {
                    flush_section();
                }
                header = line;
                continue;
            }
            if( header.empty() ) {
                sections.push_back( line );
                continue;
            }
            entries.push_back( line );
            continue;
        }
        current += ch;
    }

    const auto last_line = trim_copy( current );
    if( !last_line.empty() ) {
        if( last_line.ends_with( ':' ) ) {
            if( !header.empty() && header != last_line ) {
                flush_section();
            }
            header = last_line;
        } else if( header.empty() ) {
            sections.push_back( last_line );
        } else {
            entries.push_back( last_line );
        }
    }

    flush_section();
    return join_sections( sections );
}
