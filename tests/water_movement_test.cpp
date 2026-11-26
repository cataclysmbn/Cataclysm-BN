#include "../src/map/map.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "game.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

#include <memory>

TEST_CASE("avatar diving", "[diving][!mayfail]") {
    const ter_id t_water_dp("t_water_dp");
    const ter_id t_water_cube("t_water_cube");
    const ter_id t_lake_bed("t_lake_bed");

    clear_all_state();
    build_water_test_map(t_water_dp, t_water_cube, t_lake_bed);
    map& here = get_map();

    clear_avatar();
    Character& dummy = get_player_character();

    REQUIRE(here.ter(bub_test_origin()) == t_water_dp);
    REQUIRE(here.ter(bub_test_origin() + tripoint_rel_ms::below()) == t_water_cube);
    REQUIRE(here.ter(bub_test_origin() + tripoint_rel_ms(0, 0, -2)) == t_lake_bed);

    GIVEN("avatar is above water at z0") {
        dummy.set_underwater(false);
        dummy.setpos(test_origin);

        WHEN("avatar dives down") {
            g->vertical_move(-1, false);

            THEN("avatar is underwater at z0") {
                REQUIRE(dummy.abs_pos() == test_origin);
                REQUIRE(here.ter(bub_test_origin()) == ter_id("t_water_dp"));
                REQUIRE(dummy.is_underwater());
            }
        }

        WHEN("avatar swims up") {
            g->vertical_move(1, false);

            THEN("avatar is not underwater at z0") {
                REQUIRE(dummy.abs_pos() == test_origin);
                REQUIRE(here.ter(bub_test_origin()) == ter_id("t_water_dp"));
                REQUIRE(!dummy.is_underwater());
            }
        }
    }

    GIVEN("avatar is underwater at z0") {
        dummy.set_underwater(true);
        dummy.setpos(test_origin);

        WHEN("avatar dives down") {
            g->vertical_move(-1, false);

            THEN("avatar is underwater at z-1") {
                const auto expected_abs = test_origin + tripoint_rel_ms::below();
                const auto expected_local = abs_to_bub(expected_abs);
                const auto direct_terrain = here.get_mapbuffer().get_ter(expected_abs);
                REQUIRE(dummy.abs_pos() == expected_abs);
                INFO("map origin "
                     << here.get_abs_sub() << " player " << dummy.abs_pos() << " expected "
                     << expected_abs << " local " << expected_local << " actual terrain "
                     << here.ter(expected_local).id().str() << " direct terrain "
                     << (direct_terrain ? direct_terrain->id().str() : "<missing>"));
                REQUIRE(here.ter(expected_local) == ter_id("t_water_cube"));
                REQUIRE(dummy.is_underwater());
            }
        }

        WHEN("avatar swims up") {
            g->vertical_move(1, false);

            THEN("avatar is not underwater at z0") {
                REQUIRE(dummy.bub_pos() == bub_test_origin());
                REQUIRE(here.ter(bub_test_origin()) == ter_id("t_water_dp"));
                REQUIRE(!dummy.is_underwater());
            }
        }
    }

    GIVEN("avatar is underwater at z-1") {
        dummy.set_underwater(true);
        dummy.setpos(test_origin + tripoint_rel_ms::below());

        WHEN("avatar dives down") {
            g->vertical_move(-1, false);

            THEN("avatar is underwater at z-2") {
                const auto expected_abs = test_origin + tripoint_rel_ms(0, 0, -2);
                const auto expected_local = abs_to_bub(expected_abs);
                REQUIRE(dummy.abs_pos() == expected_abs);
                REQUIRE(here.ter(expected_local) == ter_id("t_lake_bed"));
                REQUIRE(dummy.is_underwater());
            }
        }

        WHEN("avatar swims up") {
            g->vertical_move(1, false);

            THEN("avatar is underwater at z0") {
                REQUIRE(dummy.bub_pos() == bub_test_origin());
                REQUIRE(here.ter(bub_test_origin()) == ter_id("t_water_dp"));
                REQUIRE(dummy.is_underwater());
            }
        }
    }

    GIVEN("avatar is underwater at z-2") {
        dummy.set_underwater(true);
        dummy.setpos(test_origin + tripoint_rel_ms(0, 0, -2));

        WHEN("avatar dives down") {
            g->vertical_move(-1, false);

            THEN("avatar is underwater at z-2") {
                const auto expected_abs = test_origin + tripoint_rel_ms(0, 0, -2);
                REQUIRE(dummy.abs_pos() == expected_abs);
                REQUIRE(here.ter(abs_to_bub(expected_abs)) == ter_id("t_lake_bed"));
                REQUIRE(dummy.is_underwater());
            }
        }

        WHEN("avatar swims up") {
            g->vertical_move(1, false);

            THEN("avatar is underwater at z-1") {
                const auto expected_abs = test_origin + tripoint_rel_ms::below();
                REQUIRE(dummy.abs_pos() == expected_abs);
                REQUIRE(here.ter(abs_to_bub(expected_abs)) == ter_id("t_water_cube"));
                REQUIRE(dummy.is_underwater());
            }
        }
    }

    dummy.setpos(test_origin);
}
