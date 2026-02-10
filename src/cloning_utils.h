#pragma once

#include <array>
#include <exception>
#include <iterator>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

#include "character.h"
#include "flag_trait.h"
#include "mtype.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "type_id.h"

namespace cloning_utils
{
inline const trait_flag_str_id trait_flag_NO_CLONE( "NO_CLONE" );
inline const trait_flag_str_id trait_flag_BG_SURVIVAL_STORY( "BG_SURVIVAL_STORY" );

inline auto specimen_size_class( const mtype_id &specimen_id ) -> int
{
    if( specimen_id.is_null() ) {
        return 0;
    }
    return static_cast<int>( specimen_id.obj().size );
}

inline auto specimen_required_sample_size( const mtype_id &specimen_id ) -> int
{
    if( specimen_id.is_null() ) {
        return 0;
    }
    return specimen_size_class( specimen_id ) + 1;
}
inline auto specimen_required_sample_size( const std::string &specimen ) -> int
{
    if( specimen.rfind( "npc:", 0 ) == 0 ) {
        return 1;
    }
    return specimen_required_sample_size( mtype_id( specimen ) );
}
inline auto specimen_stats_to_string( const Character &source ) -> std::string
{
    return string_format( "%d,%d,%d,%d", source.get_str_base(), source.get_dex_base(),
                          source.get_int_base(), source.get_per_base() );
}
inline auto specimen_stats_from_string( const std::string &stats ) -> std::array<int, 4>
{
    std::array<int, 4> result{};
    if( stats.empty() ) {
        return result;
    }
    const std::vector<std::string> parts = string_split( stats, ',' );
    constexpr size_t stat_count = result.size();
    for( size_t idx = 0; idx < parts.size() && idx < stat_count; ++idx ) {
        if( parts[idx].empty() ) {
            continue;
        }
        try {
            result[idx] = std::stoi( parts[idx] );
        } catch( const std::exception & ) {
            result[idx] = 0;
        }
    }
    return result;
}
inline auto specimen_age_to_string( const Character &source ) -> std::string
{
    return string_format( "%d", source.base_age() );
}
inline auto specimen_age_from_string( const std::string &age ) -> int
{
    if( age.empty() ) {
        return 0;
    }
    try {
        return std::stoi( age );
    } catch( const std::exception & ) {
        return 0;
    }
}
inline auto specimen_height_to_string( const Character &source ) -> std::string
{
    return string_format( "%d", source.base_height() );
}
inline auto specimen_height_from_string( const std::string &height ) -> int
{
    if( height.empty() ) {
        return 0;
    }
    try {
        return std::stoi( height );
    } catch( const std::exception & ) {
        return 0;
    }
}
inline auto specimen_gender_to_string( const Character &source ) -> std::string
{
    return source.male ? "male" : "female";
}
inline auto specimen_gender_from_string( const std::string &gender ) -> bool
{
    return gender == "male";
}
inline auto specimen_mutations_to_string( const Character &source ) -> std::string
{
    const std::vector<trait_id> muts = source.get_mutations();
    if( muts.empty() ) {
        return std::string();
    }
    std::vector<std::string> names;
    names.reserve( muts.size() );
    std::ranges::for_each( muts, [&]( const trait_id &trait ) {
        const mutation_branch &trait_data = trait.obj();
        if( trait_data.flags.contains( trait_flag_NO_CLONE ) ) {
            return;
        }
        if( trait_data.flags.contains( trait_flag_BG_SURVIVAL_STORY ) ) {
            return;
        }
        if( trait_data.profession ) {
            return;
        }
        names.push_back( trait.str() );
    } );
    return join( names, "," );
}
inline auto specimen_mutations_from_string( const std::string &mutations ) -> std::vector<trait_id>
{
    std::vector<trait_id> result;
    if( mutations.empty() ) {
        return result;
    }
    const std::vector<std::string> parts = string_split( mutations, ',' );
    result.reserve( parts.size() );
    std::ranges::for_each( parts, [&]( const std::string &part ) {
        if( part.empty() ) {
            return;
        }
        const trait_id tid( part );
        if( !tid.is_valid() ) {
            return;
        }
        const mutation_branch &trait_data = tid.obj();
        if( trait_data.flags.contains( trait_flag_BG_SURVIVAL_STORY ) ) {
            return;
        }
        if( trait_data.profession ) {
            return;
        }
        result.push_back( tid );
    } );
    return result;
}
inline auto specimen_size_class_string( const mtype_id &specimen_id ) -> const char *
{
    static constexpr std::array<const char *, 5> creature_size_strings = {
        "TINY",
        "SMALL",
        "MEDIUM",
        "LARGE",
        "HUGE"
    };
    const auto size_class = specimen_size_class( specimen_id );
    if( size_class >= 0 && size_class < static_cast<int>( creature_size_strings.size() ) ) {
        return creature_size_strings[size_class];
    }
    return "UNKNOWN";
}
} // namespace cloning_utils
