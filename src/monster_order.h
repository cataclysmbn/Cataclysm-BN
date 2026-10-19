#pragma once

#include <algorithm>
#include <utility>
#include <vector>

class monster;

/// A monster paired with its distance to the avatar.
using monster_at_distance = std::pair<int, monster *>;

/// Orders `monsters` nearest first; monsters at one distance keep the order they are listed in, so
/// the order does not depend on where they live in memory.
inline auto order_by_distance( std::vector<monster_at_distance> &monsters ) -> void
{
    std::ranges::stable_sort( monsters, {}, &monster_at_distance::first );
}
