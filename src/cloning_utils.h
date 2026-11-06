#pragma once

#include <array>
#include <string>

#include "mtype.h"
#include "translations.h"
#include "type_id.h"

namespace cloning_utils
{
inline auto specimen_size_class( const mtype_id &specimen_id ) -> int
{
    if( specimen_id.is_null() || !specimen_id.is_valid() ) {
        return -1;
    }
    return static_cast<int>( specimen_id.obj().size );
}

inline auto specimen_required_sample_size( const mtype_id &specimen_id ) -> int
{
    if( specimen_id.is_null() || !specimen_id.is_valid() ) {
        return 0;
    }
    return specimen_size_class( specimen_id ) + 1;
}

/// Resolve the name in the current language without rewriting stored sample data.
inline auto specimen_name( const mtype_id &specimen_id, const std::string &fallback ) -> std::string
{
    if( specimen_id.is_null() || !specimen_id.is_valid() ) {
        return fallback;
    }
    // monster::name() appends personal names after ": "; keep these stored labels intact.
    if( fallback.find( ": " ) != std::string::npos ) {
        return fallback;
    }
    return specimen_id->nname();
}

inline auto specimen_size_class_string( const mtype_id &specimen_id ) -> const char *
{
    static constexpr std::array<const char *, 5> creature_size_strings = {
        translate_marker( "TINY" ),
        translate_marker( "SMALL" ),
        translate_marker( "MEDIUM" ),
        translate_marker( "LARGE" ),
        translate_marker( "HUGE" )
    };
    const auto size_class = specimen_size_class( specimen_id );
    if( size_class >= 0 && size_class < static_cast<int>( creature_size_strings.size() ) ) {
        return _( creature_size_strings[size_class] );
    }
    return _( "UNKNOWN" );
}
} // namespace cloning_utils
