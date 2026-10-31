#include "../src/map/map.h"
#include "../src/vehicle/vpart_position.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "pickup.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/vehicle.h"

#include <algorithm>

static auto count_pickup_items(
    const pickup::nearby_pickup_items& pickup_items, const itype_id& type) -> int {
    return std::ranges::count_if(pickup_items.items, [&type](const item_stack::iterator& iter) {
        return (*iter)->typeId() == type;
    });
}

TEST_CASE("nearby pickup finds items on all adjacent ground tiles", "[pickup]") {
    clear_all_state();

    map& here = get_map();
    g->u.setpos(test_origin);

    for (const tripoint_bub_ms& pos : here.points_in_radius(bub_test_origin(), 2)) {
        here.i_clear(pos);
    }

    here.add_item_or_charges(bub_test_origin(), item::spawn("rock"));
    here.add_item_or_charges(bub_test_origin() + tripoint_east, item::spawn("stick"));
    here.add_item_or_charges(bub_test_origin() + tripoint_east * 2, item::spawn("jeans"));

    const auto pickup_items = pickup::nearby_items_for_pickup(bub_test_origin());

    CHECK(pickup_items.has_ground_items);
    CHECK(pickup_items.items.size() == 2);
    CHECK(count_pickup_items(pickup_items, itype_id("rock")) == 1);
    CHECK(count_pickup_items(pickup_items, itype_id("stick")) == 1);
    CHECK(count_pickup_items(pickup_items, itype_id("jeans")) == 0);
}

TEST_CASE("nearby pickup finds adjacent vehicle cargo", "[pickup][vehicle]") {
    clear_all_state();

    map& here = get_map();
    g->u.setpos(test_origin);
    const auto cart_pos_abs = test_origin + tripoint_rel_ms::east();

    auto* const cart =
        here.add_vehicle(vproto_id("shopping_cart"), abs_to_bub(cart_pos_abs), 0_degrees, 0, 0);
    REQUIRE(cart != nullptr);

    const std::optional<vpart_reference> cargo =
        here.veh_at(abs_to_bub(cart_pos_abs)).part_with_feature("CARGO", true);
    REQUIRE(cargo);
    cargo->vehicle().get_items(cargo->part_index()).clear();
    REQUIRE_FALSE(cargo->vehicle().add_item(cargo->part(), item::spawn("jeans")));

    const auto pickup_items = pickup::nearby_items_for_pickup(bub_test_origin());

    CHECK_FALSE(pickup_items.has_ground_items);
    CHECK(pickup_items.items.size() == 1);
    CHECK(count_pickup_items(pickup_items, itype_id("jeans")) == 1);
}
