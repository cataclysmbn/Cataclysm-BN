#pragma once

#include <algorithm>
#include <utility>
#include <vector>

class monster;

/// A monster paired with its distance to the avatar.
using monster_at_distance = std::pair<int, monster *>;

/// Orders `monsters` nearest first.
inline auto order_by_distance( std::vector<monster_at_distance> &monsters ) -> void
{
    std::ranges::sort( monsters );
}
