#include "avatar.h"
#include "calendar.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "debug.h"
#include "game.h"
#include "iexamine.h"
#include "init.h"
#include "item.h"
#include "map/auto_forage.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "state_helpers.h"
#include "type_id.h"

#include <cstdint>
#include <string>

/// Tests the Lua side of terrain/furniture examine and seed harvest: a
/// `"lua:NAME"` examine_action dispatches to `game.examine_functions` with the
/// terrain and furniture ids, a seed `harvest_action` dispatches to
/// `game.harvest_functions` instead of the hardcoded harvest, and a harvest
/// callback that leaves the grown plant gets the default transform. The
/// callbacks are added to the live `game` tables and removed afterwards, so the
/// test doesn't reset Lua state that other tests rely on.
TEST_CASE("lua_examine_and_harvest_callbacks", "[lua][examine]") {
    clear_all_state();
    sol::state& lua = DynamicDataLoader::get_instance().lua->lua;
    const auto cleanup = on_out_of_scope([&]() {
        lua.safe_script(R"(
            game.examine_functions["test_lua_examine"] = nil
            game.examine_functions["test_lua_examine_furn"] = nil
            game.harvest_functions["test_lua_harvest"] = nil
            test_data = nil
        )");
        clear_all_state();
    });

    auto test_data = lua.create_table();
    lua.globals()["test_data"] = test_data;

    const auto script = lua.safe_script(
        R"(
        game.examine_functions["test_lua_examine"] = function( params )
            test_data.examine_ter_called = true
            test_data.examine_ter_terrain = params.terrain
            test_data.examine_ter_furniture = params.furniture
            test_data.examine_ter_user = params.user ~= nil
            test_data.examine_ter_pos = params.pos ~= nil
        end
        game.examine_functions["test_lua_examine_furn"] = function( params )
            test_data.examine_furn_called = true
            test_data.examine_furn_terrain = params.terrain
            test_data.examine_furn_furniture = params.furniture
        end
        game.harvest_functions["test_lua_harvest"] = function( params )
            test_data.harvest_called = true
            test_data.harvest_user = params.user ~= nil
            test_data.harvest_pos = params.pos ~= nil
            test_data.harvest_seed = params.seed ~= nil
            test_data.harvest_from_activity = params.from_activity
        end
    )",
        sol::script_pass_on_error);
    REQUIRE(script.valid());

    avatar& who = get_avatar();
    map& here = get_map();
    const tripoint_bub_ms pos(60, 60, 0);
    who.setpos(map_local_to_abs(here, pos));

    SECTION("terrain examine_action lua dispatches to game.examine_functions") {
        here.ter_set(pos, ter_str_id("t_test_lua_examine").id());
        here.examine(who, pos);

        CHECK(test_data.get<bool>("examine_ter_called"));
        CHECK(test_data.get<std::string>("examine_ter_terrain") == "t_test_lua_examine");
        CHECK_FALSE(test_data.get<sol::optional<std::string>>("examine_ter_furniture").has_value());
        CHECK(test_data.get<bool>("examine_ter_user"));
        CHECK(test_data.get<bool>("examine_ter_pos"));
    }

    SECTION("copy-from keeps an inherited lua examine_action") {
        // The copy sets no examine_action of its own, so it must keep both the
        // lua_examine function and the callback id of its parent.
        here.ter_set(pos, ter_str_id("t_test_lua_examine_copy").id());
        here.examine(who, pos);

        CHECK(test_data.get<bool>("examine_ter_called"));
        CHECK(test_data.get<std::string>("examine_ter_terrain") == "t_test_lua_examine_copy");
    }

    SECTION("furniture examine_action lua reports furniture and terrain ids") {
        here.ter_set(pos, ter_str_id("t_dirt").id());
        here.furn_set(pos, furn_str_id("f_test_lua_examine").id());
        here.examine(who, pos);

        CHECK(test_data.get<bool>("examine_furn_called"));
        CHECK(test_data.get<std::string>("examine_furn_terrain") == "t_dirt");
        CHECK(test_data.get<std::string>("examine_furn_furniture") == "f_test_lua_examine");
    }

    SECTION("seed harvest_action lua replaces the hardcoded harvest") {
        here.furn_set(pos, f_null);
        here.add_item(pos, item::spawn(itype_id("test_lua_harvest_seed"), calendar::turn));

        iexamine::harvest_plant(who, pos, false);

        CHECK(test_data.get<bool>("harvest_called"));
        CHECK(test_data.get<bool>("harvest_user"));
        CHECK(test_data.get<bool>("harvest_pos"));
        CHECK(test_data.get<bool>("harvest_seed"));
        CHECK_FALSE(test_data.get<bool>("harvest_from_activity"));
        // The Lua callback owns the harvest entirely: the C++ generic harvest
        // must not have consumed or transformed the seed.
        REQUIRE(here.i_at(pos).size() == 1);
        CHECK(here.i_at(pos).only_item().typeId() == itype_id("test_lua_harvest_seed"));
    }

    SECTION("a harvest callback that leaves the plant gets the default transform") {
        // The test callback doesn't touch the tile. Left alone, the grown
        // plant would stay harvestable and farm zones would harvest it forever.
        here.furn_set(pos, furn_str_id("f_plant_harvest").id());
        REQUIRE(here.has_flag_furn("GROWTH_HARVEST", pos));
        here.add_item(pos, item::spawn(itype_id("test_lua_harvest_seed"), calendar::turn));

        const auto message = capture_debugmsg_during([&]() {
            iexamine::harvest_plant(who, pos, true);
        });

        CHECK(test_data.get<bool>("harvest_called"));
        CHECK(test_data.get<bool>("harvest_from_activity"));
        CHECK(message.find("left a harvestable plant") != std::string::npos);
        CHECK_FALSE(here.has_flag_furn("GROWTH_HARVEST", pos));
        // The fallback clears the tile like the hardcoded harvest, so the
        // seed item the callback forgot does not stay behind.
        CHECK(here.i_at(pos).empty());
    }

    SECTION("forage_class is loaded from json") {
        CHECK(ter_str_id("t_marloss").id().obj().forage_classes == forage_class::bushes);
        CHECK(ter_str_id("t_marloss_tree").id().obj().forage_classes == forage_class::trees);
        CHECK(
            ter_str_id("t_tree_apple").id().obj().forage_classes
            == static_cast<forage_class>(
                static_cast<uint8_t>(forage_class::bushes)
                | static_cast<uint8_t>(forage_class::trees)));
        CHECK(ter_str_id("t_tree_walnut").id().obj().forage_classes == forage_class::trees);
        CHECK(furn_str_id("f_chamomile").id().obj().forage_classes == forage_class::flowers);
        CHECK(ter_str_id("t_test_lua_examine").id().obj().forage_classes == forage_class::bushes);
        CHECK(furn_str_id("f_test_lua_examine").id().obj().forage_classes == forage_class::none);
        // No forage_class: the legacy harvest action gives the old default, so
        // mods that predate forage_class keep auto-foraging.
        CHECK(ter_str_id("t_test_legacy_forage").id().obj().forage_classes == forage_class::trees);
        // copy-from that replaces the examine_action drops the inherited
        // class: a harvested pine has nothing to forage.
        CHECK(ter_str_id("t_tree_pine").id().obj().forage_classes == forage_class::trees);
        CHECK(ter_str_id("t_tree_pine_harvested").id().obj().forage_classes == forage_class::none);
    }

    SECTION("a forage class without an examine action warns at load") {
        // t_test_forage_none sets a forage class but no examine action, so
        // the "none" examine would print "That is a X." on every auto-forage
        // step. The data check must flag it.
        const auto message = capture_debugmsg_during([&]() {
            ter_str_id("t_test_forage_none").id().obj().check();
        });
        CHECK(message.find("can never be foraged") != std::string::npos);
        // Sanity: the check stays silent for a properly forageable terrain.
        const auto quiet = capture_debugmsg_during([&]() {
            ter_str_id("t_test_lua_examine").id().obj().check();
        });
        CHECK(quiet.empty());
    }
}


/// Tests the AUTO_FORAGING decision used by `game::place_player`: which
/// option harvests which forage class, that terrain is checked for bushes and
/// trees while furniture is checked for flowers, and that terrain wins.
TEST_CASE("auto_forage_target_follows_option_and_forage_class", "[examine]") {
    const auto none = forage_class::none;
    const auto bushes = forage_class::bushes;
    const auto trees = forage_class::trees;
    const auto flowers = forage_class::flowers;

    CHECK(pick_auto_forage_target(bushes, none, "off") == auto_forage_target::none);
    CHECK(pick_auto_forage_target(bushes, none, "bushes") == auto_forage_target::terrain);
    CHECK(pick_auto_forage_target(bushes, none, "trees") == auto_forage_target::none);
    CHECK(pick_auto_forage_target(trees, none, "trees") == auto_forage_target::terrain);
    CHECK(pick_auto_forage_target(trees, none, "both") == auto_forage_target::terrain);
    CHECK(pick_auto_forage_target(none, flowers, "flowers") == auto_forage_target::furniture);
    CHECK(pick_auto_forage_target(none, flowers, "bushes") == auto_forage_target::none);
    CHECK(pick_auto_forage_target(none, flowers, "both") == auto_forage_target::furniture);
    // Flowers count only on furniture, bushes and trees only on terrain.
    CHECK(pick_auto_forage_target(flowers, none, "both") == auto_forage_target::none);
    CHECK(pick_auto_forage_target(none, trees, "both") == auto_forage_target::none);
    // Terrain wins when both match.
    CHECK(pick_auto_forage_target(bushes, flowers, "both") == auto_forage_target::terrain);
}
