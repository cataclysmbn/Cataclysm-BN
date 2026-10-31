#pragma once

#include "enums.h"

class JsonObject;

/// Hunting capability of a trap type, read from the trap's optional `hunting` object.
struct trap_hunting_data {
    /// Smallest prey this trap can hold.
    creature_size min_size = creature_size::tiny;
    /// Largest prey this trap can hold.
    creature_size max_size = creature_size::small;
    /// Multiplier applied to the catch rate.
    double efficiency = 1.0;

    auto load( const JsonObject &jo ) -> void;
    auto can_hold( creature_size size ) const -> bool { return size >= min_size && size <= max_size; }
};
