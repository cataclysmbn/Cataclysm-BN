#include "../src/map/map.h"
#include "catch/catch.hpp"
#include "debug.h"
#include "game.h"
#include "item_stack.h"
#include "map/utils/map_utils.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("take_down_deployed_furniture_keeps_furniture_vars", "[iexamine][deployed_furniture]") {
    clear_all_state();
    auto& here = get_map();
    here.ter_set(bub_test_origin(), ter_id("t_floor"));
    here.furn_set(bub_test_origin(), furn_id("f_cardboard_box"));
    here.i_clear(bub_test_origin());
    here.furn_vars(bub_test_origin())->set("test_var", "kept");

    const auto debug_msg = capture_debugmsg_during([&]() {
        map_funcs::take_down_deployed_furniture(bub_test_origin(), bub_test_origin());
    });

    CHECK(debug_msg.empty());
    CHECK(here.furn(bub_test_origin()) == f_null);
    auto dropped_items = here.i_at(bub_test_origin());
    REQUIRE(dropped_items.size() == 1);
    const auto& dropped_item = dropped_items.only_item();
    CHECK(dropped_item.typeId() == itype_id("box_large"));
    CHECK(dropped_item.get_var("test_var") == "kept");
}
