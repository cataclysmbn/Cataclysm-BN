#pragma once

class Character;

enum class night_vision_bonus_level {
    none,
    standard_goggles,
    standard,
    enhanced,
};

namespace character_vision
{

auto active_night_vision_bonus_level( const Character &who ) -> night_vision_bonus_level;
auto sight_range_bonus( night_vision_bonus_level level ) -> float;

} // namespace character_vision
