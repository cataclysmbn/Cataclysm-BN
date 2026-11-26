#include "../src/map/map.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "item.h"
#include "map_helpers.h"
#include "monster.h"
#include "state_helpers.h"
#include "type_id.h"

#include <memory>

namespace {

const auto effect_downed = efftype_id("downed");

} // namespace

TEST_CASE("creature_in_field", "[monster],[field]") {
    clear_all_state();
    static const tripoint_bub_ms target_location{5, 5, 0};
    map& here = get_map();
    GIVEN("An acid field") {
        here.add_field(target_location, field_type_id("fd_acid"));
        WHEN("a monster stands on it") {
            monster& test_monster = spawn_test_monster("mon_zombie", target_location);
            REQUIRE(test_monster.get_hp() == test_monster.get_hp_max());
            THEN("the monster takes damage") {
                here.creature_in_field(test_monster);
                CHECK(test_monster.get_hp() < test_monster.get_hp_max());
            }
        }
        WHEN("A monster in a vehicle stands in it") {
            here.add_vehicle(vproto_id("handjack"), target_location, 0_degrees);
            monster& test_monster = spawn_test_monster("mon_zombie", target_location);
            REQUIRE(test_monster.get_hp() == test_monster.get_hp_max());
            THEN("the monster doesn't take damage") {
                here.creature_in_field(test_monster);
                CHECK(test_monster.get_hp() == test_monster.get_hp_max());
            }
        }
    }
}

TEST_CASE("noslip clothing prevents field-based slipping", "[avatar],[field]") {
    clear_all_state();

    auto& here = get_map();
    auto& you = get_avatar();
    const auto field_test_fd_slip = field_type_id("test_fd_slip");
    you.setpos(tripoint_bub_ms(5, 5, 0));
    // Moving the avatar recenters the map, so read its local position afterward.
    const auto target_location = you.bub_pos();

    SECTION("slippery fields down the avatar without noslip footwear") {
        REQUIRE(here.add_field(target_location, field_test_fd_slip));

        here.creature_in_field(you, /*movement_only=*/true);

        CHECK(you.has_effect(effect_downed));
    }

    SECTION("noslip footwear blocks the downed effect from slippery fields") {
        REQUIRE_FALSE(you.wear_item(item::spawn("test_noslip_boots"), false));
        REQUIRE(here.add_field(target_location, field_test_fd_slip));

        here.creature_in_field(you, /*movement_only=*/true);

        CHECK_FALSE(you.has_effect(effect_downed));
    }

    SECTION("noslip enchantments block the downed effect from slippery fields") {
        REQUIRE_FALSE(you.wear_item(item::spawn("test_socks_of_noslip"), false));
        REQUIRE(here.add_field(target_location, field_test_fd_slip));

        here.creature_in_field(you, /*movement_only=*/true);

        CHECK_FALSE(you.has_effect(effect_downed));
    }
}

TEST_CASE("waiting in slippery fields does not knock characters down", "[avatar],[field]") {
    clear_all_state();
    auto& here = get_map();
    auto& you = get_avatar();
    you.setpos(tripoint_bub_ms(5, 5, 0));
    const auto origin = you.abs_pos();
    const auto destination = origin + tripoint_rel_ms(1, 0, 0);
    REQUIRE(
        here.add_field(you.bub_pos() + tripoint_rel_ms(1, 0, 0), field_type_id("test_fd_slip")));
    CHECK_FALSE(you.has_effect(effect_downed));

    g->place_player(abs_to_bub(destination));
    REQUIRE(you.has_effect(effect_downed));
    you.remove_effect(effect_downed);

    for (const auto turn : {1, 2, 3}) {
        INFO("Waiting turn " << turn);
        here.creature_in_field(you);
        CHECK_FALSE(you.has_effect(effect_downed));
    }

    g->place_player(abs_to_bub(origin));
    g->place_player(abs_to_bub(destination));
    CHECK(you.has_effect(effect_downed));
}
