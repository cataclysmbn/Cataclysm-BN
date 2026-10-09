#include "catch/catch.hpp"
#include "item.h"
#include "map_item_stack.h"
#include "state_helpers.h"

TEST_CASE("map_item_stack_groups_keep_their_own_example", "[map_item_stack]") {
    clear_all_state();
    const auto& near_item = *item::spawn_temporary("rock");
    const auto& far_item = *item::spawn_temporary("rock");
    const auto near_pos = tripoint_rel_ms(1, 0, 0);
    const auto far_pos = tripoint_rel_ms(5, 0, 0);

    auto stack = map_item_stack(&near_item, near_pos);
    stack.add_at_pos(&far_item, far_pos);

    REQUIRE(stack.vIG.size() == 2);
    CHECK(stack.vIG[0].example == &near_item);
    CHECK(stack.vIG[1].example == &far_item);
}
