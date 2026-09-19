#include "monster_hallucination.h"

#include "monster.h"

namespace monster_hallucination
{

auto needs_lifecycle_expiry( const monster &critter ) -> bool
{
    return !critter.is_dead() && critter.is_hallucination() && critter.get_moves() <= 0 &&
           critter.get_speed() <= 0;
}

} // namespace monster_hallucination
