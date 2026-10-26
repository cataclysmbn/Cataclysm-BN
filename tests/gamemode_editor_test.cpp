#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/catalog.h"
#include "game.h"
#include "game_constants.h"
#include "gamemode.h"
#include "init.h"
#include "loading_ui.h"
#include "map/map.h"
#include "mod_manager.h"
#include "options.h"
#include "overmap/overmapbuffer.h"
#include "overmap/overmapbuffer_registry.h"
#include "state_helpers.h"
#include "type_id.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

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

TEST_CASE(
    "an editor world made with a chosen mod loads the mod after its dependency",
    "[gamemode][editor][worldfactory]") {
    const auto core = mod_management::get_default_core_content_pack();
    auto* const made =
        world_generator->make_new_world(special_game_type::EDITOR, {mod_id("desertpack")});
    REQUIRE(made != nullptr);
    const auto cleanup = on_out_of_scope([name = made->world_name] {
        world_generator->delete_world(name, true);
    });
    const auto& order = made->active_mod_order;
    const auto dependency = std::ranges::find(order, core);
    const auto chosen = std::ranges::find(order, mod_id("desertpack"));
    REQUIRE(dependency != order.end());
    REQUIRE(chosen != order.end());
    CHECK(dependency < chosen);
    // Opening the editor again with other mods replaces the first choice instead of keeping it.
    CHECK(world_generator->make_new_world(special_game_type::EDITOR, {core}) == made);
    CHECK(made->active_mod_order == std::vector{core});
}

TEST_CASE(
    "the mod an editor world was made with has its terrain in the editor's catalog",
    "[gamemode][editor][editor_catalog][worldfactory]") {
    const auto has_sand = [] {
        return editor::search_catalog(
                   {.kind = editor::catalog_kind::terrain, .filter = "t_sand_singing"})
                   .total
            == 1;
    };
    REQUIRE_FALSE(has_sand());
    auto* const original = world_generator->active_world->info;
    auto* const made =
        world_generator->make_new_world(special_game_type::EDITOR, {mod_id("desertpack")});
    REQUIRE(made != nullptr);
    const auto reload = [](WORLDINFO* const info) {
        world_generator->set_active_world(info);
        // As game::setup does: overmaps point into the region settings the reload replaces.
        for_each_overmapbuffer([](const dimension_id& /*dimension*/, overmapbuffer& buffer) {
            buffer.clear();
        });
        auto ui = loading_ui(false);
        init::load_world_modfiles(ui, g->get_active_world(), SAVE_ARTIFACTS);
    };
    const auto cleanup = on_out_of_scope([&, name = made->world_name] {
        reload(original);
        world_generator->delete_world(name, true);
        clear_all_state();
    });
    reload(made);
    CHECK(has_sand());
}
