#include "avatar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "character.h"
#include "creature.h"
#include "dispersion.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "player_helpers.h"
#include "ranged.h"
#include "state_helpers.h"
#include "type_id.h"

#include <vector>

static constexpr tripoint_bub_ms shooter_pos(60, 60, 0);

TEST_CASE("prone_stance_movement_costs", "[stance][prone][movement]") {
    clear_all_state();
    avatar& shooter = g->u;
    g->place_player(shooter_pos);

    GIVEN("character standing") {
        shooter.set_movement_mode(character_movemode::CMM_WALK);
        REQUIRE(shooter.movement_mode_is(character_movemode::CMM_WALK));

        WHEN("standing -> crouch") {
            shooter.set_moves(200);
            shooter.set_movement_mode(character_movemode::CMM_CROUCH);
            THEN("costs 100 moves") { CHECK(shooter.get_moves() == 100); }
            AND_WHEN("crouch -> stand") {
                shooter.set_moves(200);
                shooter.set_movement_mode(character_movemode::CMM_WALK);
                THEN("costs 100 moves") { CHECK(shooter.get_moves() == 100); }
            }
        }

        WHEN("standing -> prone") {
            shooter.set_moves(400);
            shooter.set_movement_mode(character_movemode::CMM_PRONE);
            THEN("costs 300 moves") { CHECK(shooter.get_moves() == 100); }
            AND_WHEN("prone -> stand") {
                shooter.set_moves(400);
                shooter.set_movement_mode(character_movemode::CMM_WALK);
                THEN("costs 300 moves") { CHECK(shooter.get_moves() == 100); }
            }
        }
    }

    GIVEN("character crouching") {
        shooter.set_movement_mode(character_movemode::CMM_CROUCH);
        REQUIRE(shooter.movement_mode_is(character_movemode::CMM_CROUCH));

        WHEN("crouch -> prone") {
            shooter.set_moves(400);
            shooter.set_movement_mode(character_movemode::CMM_PRONE);
            THEN("costs 200 moves") { CHECK(shooter.get_moves() == 200); }
            AND_WHEN("prone -> crouch") {
                shooter.set_moves(400);
                shooter.set_movement_mode(character_movemode::CMM_CROUCH);
                THEN("costs 200 moves") { CHECK(shooter.get_moves() == 200); }
            }
        }
    }

    GIVEN("character prone") {
        shooter.set_movement_mode(character_movemode::CMM_PRONE);
        REQUIRE(shooter.movement_mode_is(character_movemode::CMM_PRONE));

        WHEN("toggle_prone_mode returns to walk") {
            shooter.set_moves(400);
            shooter.toggle_prone_mode();
            THEN("costs 300 moves to stand") {
                CHECK(shooter.get_moves() == 100);
                CHECK(shooter.movement_mode_is(character_movemode::CMM_WALK));
            }
        }
    }

    GIVEN("character driving a vehicle") {
        shooter.set_movement_mode(character_movemode::CMM_WALK);
        shooter.controlling_vehicle = true;

        WHEN("trying to go prone while driving") {
            shooter.set_movement_mode(character_movemode::CMM_PRONE);
            THEN("cannot go prone while driving") {
                CHECK(shooter.movement_mode_is(character_movemode::CMM_WALK));
            }
        }
        shooter.controlling_vehicle = false;
    }
}

TEST_CASE("prone_stance_ranged_effects", "[stance][prone][ranged]") {
    clear_all_state();
    avatar& shooter = g->u;
    g->place_player(shooter_pos);
    build_test_map(ter_id("t_dirt"));

    GIVEN("a rifle with bipod") {
        arm_character(shooter, "ar15", {"bipod"});

        item const& wp = shooter.primary_weapon();

        shooter.set_movement_mode(character_movemode::CMM_WALK);
        double const walk_dispersion = ranged::get_weapon_dispersion(shooter, wp).max();

        shooter.set_movement_mode(character_movemode::CMM_CROUCH);
        double const crouch_dispersion = ranged::get_weapon_dispersion(shooter, wp).max();

        shooter.set_movement_mode(character_movemode::CMM_PRONE);
        double const prone_dispersion = ranged::get_weapon_dispersion(shooter, wp).max();

        THEN("prone dispersion is lower than crouch dispersion") {
            REQUIRE(walk_dispersion > 0.0);
            REQUIRE(prone_dispersion < crouch_dispersion);
            REQUIRE(crouch_dispersion < walk_dispersion);
        }
    }

    GIVEN("a MOUNTED_GUN weapon (M2HB Browning)") {
        // Manual setup: belt50 is pre-loaded ("count": 100), so arm_character's
        // reload-via-default-ammo path fails (magazine is already full).
        shooter.remove_primary_weapon();
        detached_ptr<item> det = item::spawn("m2browning");
        item& gun = *det;
        shooter.i_add(std::move(det));
        shooter.wield(gun);

        WHEN("standing without mountable terrain nearby") {
            shooter.set_movement_mode(character_movemode::CMM_WALK);
            THEN("can_use_heavy_weapon returns false") {
                REQUIRE_FALSE(ranged::can_use_heavy_weapon(shooter, get_map(), shooter.bub_pos()));
            }
        }

        WHEN("crouching without mountable terrain nearby") {
            shooter.set_movement_mode(character_movemode::CMM_CROUCH);
            THEN("can_use_heavy_weapon returns false") {
                REQUIRE_FALSE(ranged::can_use_heavy_weapon(shooter, get_map(), shooter.bub_pos()));
            }
        }

        WHEN("prone") {
            shooter.set_movement_mode(character_movemode::CMM_PRONE);
            THEN("can_use_heavy_weapon returns true") {
                REQUIRE(ranged::can_use_heavy_weapon(shooter, get_map(), shooter.bub_pos()));
            }
        }
    }

    GIVEN("a medium-sized character") {
        WHEN("standing") {
            shooter.set_movement_mode(character_movemode::CMM_WALK);
            THEN("target size is medium (0.5)") {
                CHECK(shooter.ranged_target_size() == Approx(0.5).margin(0.001));
            }
        }
        WHEN("crouching") {
            shooter.set_movement_mode(character_movemode::CMM_CROUCH);
            THEN("target size is small (0.25)") {
                CHECK(shooter.ranged_target_size() == Approx(0.25).margin(0.001));
            }
        }
        WHEN("prone") {
            shooter.set_movement_mode(character_movemode::CMM_PRONE);
            THEN("target size is tiny (0.1)") {
                CHECK(shooter.ranged_target_size() == Approx(0.1).margin(0.001));
            }
        }
    }
}
