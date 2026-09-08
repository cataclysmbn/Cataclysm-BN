#include "catch/catch.hpp"
#include "debug.h"
#include "game.h"
#include "item_stack.h"
#include "map.h"
#include "map/utils/map_utils.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("take_down_deployed_furniture_keeps_furniture_vars", "[iexamine][deployed_furniture]") {
    clear_all_state();
    auto& here = get_map();
    const auto pos = bub_test_origin();
    const auto pos_abs = map_local_to_abs(here, pos);
    here.ter_set(pos, ter_id("t_floor"));
    here.furn_set(pos, furn_id("f_cardboard_box"));
    here.i_clear(pos);
    here.get_mapbuffer().furn_vars(pos_abs)->set("test_var", "kept");
    CAPTURE(here.get_mapbuffer().furn_vars(pos_abs)->get("test_var"));

    const auto debug_msg = capture_debugmsg_during([&]() {
        map_funcs::take_down_deployed_furniture(here.get_mapbuffer(), pos_abs, pos_abs);
    });

    CHECK(debug_msg.empty());
    CHECK(here.furn(pos) == f_null);
    auto* dropped_items = here.get_mapbuffer().get_items(pos_abs);
    REQUIRE(dropped_items != nullptr);
    REQUIRE(dropped_items->size() == 1);
    const auto& dropped_item = *dropped_items->front();
    CHECK(dropped_item.typeId() == itype_id("box_large"));
    CHECK(dropped_item.get_var("test_var") == "kept");
}
