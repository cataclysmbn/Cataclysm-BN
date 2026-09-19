#pragma once

class monster;

namespace monster_hallucination
{

inline constexpr auto expiry_one_in = 25;

/// Whether a stalled zero-speed hallucination needs the lifecycle expiry fallback.
auto needs_lifecycle_expiry( const monster &critter ) -> bool;

} // namespace monster_hallucination
