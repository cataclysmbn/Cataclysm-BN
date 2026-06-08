#pragma once

#include <array>

#include "mtype.h"
#include "type_id.h"

namespace cloning_utils
{
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
