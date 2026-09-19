#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "safe_reference.h"
#include "state_helpers.h"

TEST_CASE("Food that rots away does not leak memory", "[rot][ownership]") {
    const auto restore_time = restore_on_out_of_scope(calendar::turn);
    const auto cleanup = on_out_of_scope(clear_all_state);
    clear_all_state();
    calendar::turn = calendar::start_of_cataclysm + 91_days;
    const auto pos = tripoint_bub_ms(65, 65, 0);
    auto food = item::spawn("test_rot_food", calendar::turn, 1);
    auto food_ref = cache_reference<item>(food.get());
    get_map().add_item(pos, std::move(food));
    REQUIRE(food_ref);
    food_ref->set_relative_rot(3.0);
    auto callback_was_called = false;
    food_ref->attempt_detach([&](auto&& value) {
        callback_was_called = true;
        return std::move(value);
    });
    CHECK_FALSE(callback_was_called);
    CHECK(get_map().i_at(pos).empty());
    INFO("Food removed from the map must also be destroyed");
    CHECK_FALSE(food_ref);
}
