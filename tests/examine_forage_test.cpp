#include "avatar.h"
#include "calendar.h"
#include "catalua.h"
#include "catalua_impl.h"
#include "catalua_sol.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "iexamine.h"
#include "init.h"
#include "item.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "state_helpers.h"
#include "type_id.h"

#include <cstdint>
#include <string>

TEST_CASE("lua_examine_and_harvest_callbacks", "[lua][examine]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    auto& state = *DynamicDataLoader::get_instance().lua;
    cata::init_global_state_tables(state, {});
    sol::state& lua = state.lua;

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
    }
}
