#include "catch/catch.hpp"
#include "monster.h"
#include "monster_order.h"

#include <array>
#include <vector>

TEST_CASE("monsters at one distance keep the order they are listed in", "[monster][lod]") {
    // Listed from the highest address down, so an order by address reverses them.
    auto critters = std::array<monster, 3>{};
    auto order = std::vector<monster_at_distance>{
        {7, &critters[2]}, {3, &critters[1]}, {7, &critters[1]}, {7, &critters[0]}};
    order_by_distance(order);
    CHECK(order
          == std::vector<monster_at_distance>{
              {3, &critters[1]}, {7, &critters[2]}, {7, &critters[1]}, {7, &critters[0]}});
}
