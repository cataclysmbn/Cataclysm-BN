#pragma once

#include "enum_traits.h"
#include "enums.h"

template<>
struct enum_traits<artifact_natural_property> {
    static constexpr artifact_natural_property last = ARTPROP_MAX;
};
