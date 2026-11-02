#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "map/field.h"
#include "map/map.h"
#include "map_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("test_fixture_establishes_absolute_runtime_state", "[test_fixture]") {
    clear_all_state();

    auto& here = get_map();
    auto& you = get_avatar();

    CHECK(you.abs_pos() == test_origin);
    CHECK(you.bub_pos() == abs_to_bub(test_origin));
    CHECK(here.get_abs_sub() == test_map_origin());
    CHECK(here.getmapsize() == g_mapsize);
    CHECK(g_mapsize == 2 * g_half_mapsize + 1);
    CHECK(g_mapsize_x == SEEX * g_mapsize);
    CHECK(g_mapsize_y == SEEY * g_mapsize);
    CHECK(here.get_bound_dimension() == g->get_current_dimension_id());
    CHECK(here.has_active_load_region());
}

TEST_CASE("test_fixture_reset_clears_vertical_state_after_movement", "[test_fixture]") {
    clear_all_state();

    auto& here = get_map();
    auto& you = get_avatar();
    const auto shifted_abs = test_origin + tripoint_rel_ms::east() * SEEX;
    const auto vertical_abs = test_origin_at_z(1);

    you.setpos(shifted_abs);
    CHECK(you.abs_pos() == shifted_abs);
    CHECK(here.get_abs_sub()
          == reality_bubble_origin_from_player(shifted_abs, g_reality_bubble_size).xy());
    CHECK(here.get_abs_sub() != test_map_origin());

    here.ter_set(abs_to_bub(vertical_abs), ter_id("t_floor"));
    REQUIRE(here.add_field(abs_to_bub(vertical_abs), field_type_id("fd_fire")));
    here.add_item(abs_to_bub(vertical_abs), item::spawn("rock"));
    REQUIRE_FALSE(here.i_at(abs_to_bub(vertical_abs)).empty());

    clear_all_state();

    CHECK(you.abs_pos() == test_origin);
    CHECK(here.get_abs_sub() == test_map_origin());
    CHECK(here.get_bound_dimension() == g->get_current_dimension_id());
    CHECK(here.has_active_load_region());
    CHECK(here.get_field(abs_to_bub(vertical_abs), field_type_id("fd_fire")) == nullptr);
    CHECK(here.i_at(abs_to_bub(vertical_abs)).empty());
}
