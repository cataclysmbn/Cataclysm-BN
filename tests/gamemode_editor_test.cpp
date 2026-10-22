#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "game.h"
#include "game_constants.h"
#include "gamemode.h"
#include "map/map.h"
#include "mod_manager.h"
#include "options.h"
#include "overmap/overmapbuffer.h"
#include "state_helpers.h"
#include "type_id.h"
#include "world.h"
#include "worldfactory.h"

#include <optional>
#include <string>

TEST_CASE(
    "the main-menu editor game starts without a character on an open field at noon",
    "[gamemode][editor]") {
    clear_all_state();
    const auto origin = get_map().get_abs_sub();
    const auto name = get_avatar().name;
    const auto restore = on_out_of_scope([&] {
        g->load_map(origin);
        get_primary_overmapbuffer().clear();
        clear_all_state();
        get_avatar().name = name;
    });
    const auto mode = get_special_game(special_game_type::EDITOR);
    REQUIRE(mode->id() == special_game_type::EDITOR);
    REQUIRE(mode->init());

    const auto& you = get_avatar();
    for (const auto* const trait : {"DEBUG_EDITOR", "DEBUG_CLOAK", "DEBUG_NODMG", "DEBUG_LS"}) {
        INFO(trait);
        CHECK(you.has_trait(trait_id(trait)));
    }
    auto& overmap = get_primary_overmapbuffer();
    const auto omt = you.abs_omt_pos();
    CHECK(overmap.ter(omt) == oter_id("field"));
    CHECK(overmap.ter(omt + tripoint_rel_omt(0, 0, 1)) == oter_id("open_air"));
    CHECK(overmap.seen(omt + tripoint_rel_omt(5, 5, 0)));
    CHECK_FALSE(is_night(calendar::turn));
    // The avatar stands in the middle of the map, not at its edge.
    CHECK(you.bub_pos().xy() == point_bub_ms(g_half_mapsize_x, g_half_mapsize_y));

    // The editor opens once, at the first input, instead of asking for one.
    CHECK(mode->next_action() == std::optional(ACTION_EDITOR));
    CHECK_FALSE(mode->next_action());
}

TEST_CASE(
    "the editor world has the default mods and is deleted when its world ends",
    "[gamemode][editor][worldfactory]") {
    auto* const made = world_generator->make_new_world(special_game_type::EDITOR);
    REQUIRE(made != nullptr);
    const auto cleanup = on_out_of_scope([name = made->world_name] {
        world_generator->delete_world(name, true);
    });
    CHECK(made->world_name == "EDITOR");
    CHECK(made->WORLD_OPTIONS["WORLD_END"].getValue() == "delete");
    CHECK(made->active_mod_order == world_generator->get_mod_manager().get_default_mods());
    // The main menu opens it again rather than making a second one.
    CHECK(world_generator->make_new_world(special_game_type::EDITOR) == made);
}
