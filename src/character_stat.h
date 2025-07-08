#pragma once

#include <string>

#include "enum_traits.h"

enum class character_stat : char {
    STRENGTH,
    DEXTERITY,
    INTELLIGENCE,
    PERCEPTION,
    DUMMY_STAT
};

template<>
struct enum_traits<character_stat> {
    static constexpr character_stat last = character_stat::DUMMY_STAT;
};
/**Get translated name of a stat*/
std::string get_stat_name( character_stat Stat );


