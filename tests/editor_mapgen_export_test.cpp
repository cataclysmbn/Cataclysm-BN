#include "avatar.h"
#include "catch/catch.hpp"
#include "editor/mapgen_check.h"
#include "editor/mapgen_export.h"
#include "editor/mapgen_stamp.h"
#include "item.h"
#include "map/map.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "monster.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "trap.h"
#include "type_id.h"
#include "units.h"
#include "vehicle/vehicle.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <string>

namespace {

auto abs_at(const tripoint_abs_omt& omt, const point& p) -> tripoint_abs_ms {
    return project_to<coords::ms>(omt) + tripoint_rel_ms(p.x, p.y, 0);
}

auto symbol_at(const nlohmann::ordered_json& object, const point& p) -> std::string {
    return std::string(1, object["rows"][p.y].get<std::string>()[p.x]);
}

} // namespace

TEST_CASE("an exported overmap square maps each kind of square to a symbol", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    for (auto y = 0; y < 24; ++y) {
        for (auto x = 0; x < 24; ++x) {
            const auto at = abs_to_bub(abs_at(omt, point(x, y)));
            here.ter_set(at, ter_id("t_floor"));
            here.furn_set(at, furn_id("f_null"));
        }
    }
    here.ter_set(abs_to_bub(abs_at(omt, point(2, 3))), ter_id("t_wall"));
    here.furn_set(abs_to_bub(abs_at(omt, point(5, 5))), furn_id("f_table"));

    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    REQUIRE(exported);
    const auto& entry = *exported;
    CHECK(entry["type"] == "mapgen");
    CHECK(entry["method"] == "json");
    CHECK(entry["om_terrain"].get<std::string>()
          == get_overmapbuffer(here.get_bound_dimension()).ter(omt)->get_type_id().str());
    const auto& object = entry["object"];
    CHECK(object["rows"].size() == 24);
    CHECK(object["rows"][0].get<std::string>().size() == 24);
    const auto floor = symbol_at(object, point(0, 0));
    const auto wall = symbol_at(object, point(2, 3));
    const auto table = symbol_at(object, point(5, 5));
    CHECK(floor != wall);
    CHECK(floor != table);
    CHECK(wall != table);
    CHECK(object["terrain"][floor] == "t_floor");
    CHECK(object["terrain"][wall] == "t_wall");
    // A table stands on a floor: the symbol maps both.
    CHECK(object["terrain"][table] == "t_floor");
    CHECK(object["furniture"][table] == "f_table");
    CHECK_FALSE(object["furniture"].contains(floor));
}

TEST_CASE("an exported overmap square can be stamped back unchanged", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    here.ter_set(abs_to_bub(abs_at(omt, point(7, 9))), ter_id("t_wall"));
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    REQUIRE(exported);
    here.ter_set(abs_to_bub(abs_at(omt, point(7, 9))), ter_id("t_floor"));
    auto stamped = editor::stamp_mapgen(exported->dump(), {.at = omt});
    REQUIRE(stamped);
    CHECK(here.ter(abs_to_bub(abs_at(omt, point(7, 9)))) == ter_id("t_wall"));
    editor::swap_stamp(*stamped);
}

TEST_CASE("an export refuses an overmap square the map has not loaded", "[editor_mapgen]") {
    clear_all_state();
    const auto far = get_avatar().abs_omt_pos() + point_rel_omt(40, 0);
    const auto exported = editor::export_region(far, point_rel_omt(1, 1), "");
    REQUIRE_FALSE(exported);
    CHECK(exported.error().find("loaded") != std::string::npos);
}

TEST_CASE("an exported overmap square stamps back its traps and fields", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    const auto trapped = abs_to_bub(abs_at(omt, point(4, 4)));
    const auto bloody = abs_to_bub(abs_at(omt, point(6, 6)));
    here.trap_set(trapped, trap_id("tr_beartrap"));
    here.add_field(bloody, field_type_id("fd_blood"), 2);
    here.add_field(bloody, field_type_id("fd_bile"), 1);
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    REQUIRE(exported);
    here.remove_trap(trapped);
    here.remove_field(bloody, field_type_id("fd_blood"));
    here.remove_field(bloody, field_type_id("fd_bile"));
    auto stamped = editor::stamp_mapgen(exported->dump(), {.at = omt});
    REQUIRE(stamped);
    CHECK(here.tr_at(trapped).loadid == trap_id("tr_beartrap"));
    CHECK(here.get_field_intensity(bloody, field_type_id("fd_blood")) == 2);
    CHECK(here.get_field_intensity(bloody, field_type_id("fd_bile")) == 1);
    CHECK(here.tr_at(abs_to_bub(abs_at(omt, point(5, 4)))).is_null());
    CHECK(here.get_field_intensity(abs_to_bub(abs_at(omt, point(5, 6))), field_type_id("fd_blood"))
          == 0);
    editor::swap_stamp(*stamped);
}

TEST_CASE("an exported region of overmap squares stamps back as one entry", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    const auto east = omt + point_rel_omt(1, 0);
    here.ter_set(abs_to_bub(abs_at(east, point(3, 5))), ter_id("t_wall"));
    const auto exported = editor::export_region(omt, point_rel_omt(2, 1), "");
    REQUIRE(exported);
    const auto& entry = *exported;
    auto& overmap = get_overmapbuffer(here.get_bound_dimension());
    CHECK(entry["om_terrain"]
          == nlohmann::ordered_json::array({nlohmann::ordered_json::array(
              {overmap.ter(omt)->get_type_id().str(), overmap.ter(east)->get_type_id().str()})}));
    CHECK(entry["object"]["rows"].size() == 24);
    CHECK(entry["object"]["rows"][0].get<std::string>().size() == 48);
    here.ter_set(abs_to_bub(abs_at(east, point(3, 5))), ter_id("t_floor"));
    auto stamped = editor::stamp_mapgen(entry.dump(), {.at = omt});
    REQUIRE(stamped);
    CHECK(here.ter(abs_to_bub(abs_at(east, point(3, 5)))) == ter_id("t_wall"));
    editor::swap_stamp(*stamped);
}

TEST_CASE(
    "exported squares that cross a submap edge become a nested chunk of their size",
    "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    for (auto y = 4; y < 7; ++y) {
        for (auto x = 10; x < 15; ++x) {
            here.ter_set(abs_to_bub(abs_at(omt, point(x, y))), ter_id("t_floor"));
            here.furn_set(abs_to_bub(abs_at(omt, point(x, y))), furn_id("f_null"));
        }
    }
    here.ter_set(abs_to_bub(abs_at(omt, point(13, 6))), ter_id("t_wall"));
    // Just outside the squares: left out.
    here.ter_set(abs_to_bub(abs_at(omt, point(15, 6))), ter_id("t_wall"));

    const auto exported =
        editor::export_chunk(abs_at(omt, point(10, 4)), point_rel_ms(5, 3), "", "zz_chunk");
    REQUIRE(exported);
    const auto& entry = *exported;
    CHECK(entry["type"] == "mapgen");
    CHECK(entry["nested_mapgen_id"] == "zz_chunk");
    CHECK_FALSE(entry.contains("om_terrain"));
    const auto& object = entry["object"];
    CHECK(object["mapgensize"] == nlohmann::ordered_json::array({5, 3}));
    REQUIRE(object["rows"].size() == 3);
    CHECK(object["rows"][0].get<std::string>().size() == 5);
    const auto wall = symbol_at(object, point(3, 2));
    CHECK(object["terrain"][wall] == "t_wall");
    CHECK(symbol_at(object, point(4, 2)) == symbol_at(object, point(0, 0)));
    CHECK(editor::check_mapgen(entry.dump()).empty());
}

TEST_CASE("an exported overmap square places its items and monsters", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    const auto heap = abs_to_bub(abs_at(omt, point(8, 8)));
    for (auto count = 0; count < 3; ++count) {
        here.add_item(heap, item::spawn(itype_id("rock"), calendar::turn));
    }
    const auto lair = abs_to_bub(abs_at(omt, point(10, 10)));
    spawn_test_monster("mon_zombie", lair);
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    REQUIRE(exported);
    const auto& object = (*exported)["object"];
    CHECK(object["place_item"]
          == nlohmann::ordered_json::parse(R"([{"item":"rock","x":8,"y":8,"amount":3}])"));
    CHECK(object["place_monster"]
          == nlohmann::ordered_json::parse(R"([{"monster":"mon_zombie","x":10,"y":10}])"));
    here.i_clear(heap);
    auto stamped = editor::stamp_mapgen(exported->dump(), {.at = omt});
    REQUIRE(stamped);
    // Rocks count by charges: the three stamp back as one pile of three.
    auto rocks = 0;
    for (const auto* const it : here.i_at(heap)) {
        rocks += it->typeId() == itype_id("rock") ? it->count() : 0;
    }
    CHECK(rocks == 3);
    auto offset = point_sm_ms{};
    const auto* const sm = here.get_submap_at(lair, offset);
    CHECK(std::ranges::any_of(sm->spawns, [&](const spawn_point& spawn) {
        return spawn.type == mtype_id("mon_zombie") && spawn.pos == offset;
    }));
    editor::swap_stamp(*stamped);
}

TEST_CASE("an exported pet stays friendly", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    spawn_test_monster("mon_dog", abs_to_bub(abs_at(omt, point(12, 2)))).friendly = -1;
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    REQUIRE(exported);
    CHECK((*exported)["object"]["place_monster"]
          == nlohmann::ordered_json::parse(
              R"([{"monster":"mon_dog","x":12,"y":2,"friendly":true}])"));
}

TEST_CASE(
    "an exported overmap square places its vehicles and NPCs by their prototypes",
    "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    REQUIRE(here.add_vehicle(
        vproto_id("bicycle"), abs_to_bub(abs_at(omt, point(2, 20))), 90_degrees, 0, 0));
    spawn_npc(abs_to_bub(abs_at(omt, point(20, 3))), "test_talker");
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    clear_vehicles();
    REQUIRE(exported);
    const auto& object = (*exported)["object"];
    // The design and where it stands, not its state: it spawns intact.
    CHECK(object["place_vehicles"]
          == nlohmann::ordered_json::parse(
              R"([{"vehicle":"bicycle","x":2,"y":20,"rotation":90,"chance":100,"status":0}])"));
    CHECK(object["place_npcs"]
          == nlohmann::ordered_json::parse(R"([{"class":"test_talker","x":20,"y":3}])"));
}

TEST_CASE(
    "an exported vehicle stands where its origin mount is, and a vehicle without a design is left "
    "out",
    "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    auto* const car = here.add_vehicle(
        vproto_id("car"), abs_to_bub(abs_at(omt, point(10, 10))), 90_degrees, 0, 0);
    REQUIRE(car != nullptr);
    // A driven vehicle turns about a pivot other than its origin mount.
    car->set_pivot(tripoint_mnt_veh(1, 1, 0));
    const auto origin = car->mount_to_abs(tripoint_mnt_veh::zero()) - abs_at(omt, point_zero);
    REQUIRE(car->abs_ms_location() != car->mount_to_abs(tripoint_mnt_veh::zero()));
    REQUIRE(here.add_vehicle(
        vproto_id("custom"), abs_to_bub(abs_at(omt, point(2, 2))), 0_degrees, 0, 0));
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "");
    clear_vehicles();
    REQUIRE(exported);
    CHECK(
        (*exported)["object"]["place_vehicles"]
        == nlohmann::ordered_json::array(
            {{{"vehicle", "car"},
              {"x", origin.x()},
              {"y", origin.y()},
              {"rotation", 90},
              {"chance", 100},
              {"status", 0}}}));
}

TEST_CASE(
    "an export reuses the symbols of a palette that map a kind of square alone",
    "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    for (auto y = 0; y < 24; ++y) {
        for (auto x = 0; x < 24; ++x) {
            const auto at = abs_to_bub(abs_at(omt, point(x, y)));
            here.ter_set(at, ter_id("t_floor"));
            here.furn_set(at, furn_id("f_null"));
        }
    }
    here.ter_set(abs_to_bub(abs_at(omt, point(2, 3))), ter_id("t_brick_wall"));
    here.furn_set(abs_to_bub(abs_at(omt, point(5, 5))), furn_id("f_table"));

    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), "lab_workspace_palette");
    REQUIRE(exported);
    const auto& object = (*exported)["object"];
    CHECK(object["palettes"] == nlohmann::ordered_json::array({"lab_workspace_palette"}));
    CHECK(symbol_at(object, point(0, 0)) == ".");
    CHECK(symbol_at(object, point(2, 3)) == "#");
    CHECK_FALSE(object["terrain"].contains("."));
    CHECK_FALSE(object["terrain"].contains("#"));
    // The palette maps "t" to a table alone, without the floor under it: the export's own symbol.
    const auto table = symbol_at(object, point(5, 5));
    CHECK(table != "t");
    CHECK(object["terrain"][table] == "t_floor");
    CHECK(object["furniture"][table] == "f_table");

    here.ter_set(abs_to_bub(abs_at(omt, point(2, 3))), ter_id("t_floor"));
    auto stamped = editor::stamp_mapgen(exported->dump(), {.at = omt});
    REQUIRE(stamped);
    CHECK(here.ter(abs_to_bub(abs_at(omt, point(2, 3)))) == ter_id("t_brick_wall"));
    CHECK(here.furn(abs_to_bub(abs_at(omt, point(5, 5)))) == furn_id("f_table"));
    editor::swap_stamp(*stamped);
}

TEST_CASE("an export refuses a palette the game does not have", "[editor_mapgen]") {
    clear_all_state();
    const auto exported =
        editor::export_region(get_avatar().abs_omt_pos(), point_rel_omt(1, 1), "no_such_palette");
    REQUIRE_FALSE(exported);
    CHECK(exported.error().find("no_such_palette") != std::string::npos);
}

TEST_CASE("an export leaves a palette symbol that maps one of several choices", "[editor_mapgen]") {
    clear_all_state();
    const auto omt = get_avatar().abs_omt_pos();
    auto& here = get_map();
    for (auto y = 0; y < 24; ++y) {
        for (auto x = 0; x < 24; ++x) {
            const auto at = abs_to_bub(abs_at(omt, point(x, y)));
            here.ter_set(at, ter_id(x < 12 ? "t_dirt" : "t_floor"));
            here.furn_set(at, furn_id("f_null"));
        }
    }
    const auto exported = editor::export_region(omt, point_rel_omt(1, 1), " test_export_either ");
    REQUIRE(exported);
    const auto& object = (*exported)["object"];
    CHECK(object["palettes"] == nlohmann::ordered_json::array({"test_export_either"}));
    CHECK(symbol_at(object, point(20, 0)) == "b");
    // "a" is grass or dirt as the palette's distribution picks: dirt gets a symbol of its own.
    const auto dirt = symbol_at(object, point(0, 0));
    CHECK(dirt != "a");
    CHECK(object["terrain"][dirt] == "t_dirt");
}

TEST_CASE(
    "an export keeps the first of two symbols of the open entry that map the same kind",
    "[editor_mapgen]") {
    const auto exported = nlohmann::ordered_json::parse(R"({
        "rows": [ "ab" ], "terrain": { "a": "t_dirt", "b": "t_floor" }
    })");
    const auto dirt = [](const std::string& symbol) {
        return editor::legend_symbol{
            .symbol = symbol, .lines = {{.layer = "terrain", .value = R"("t_dirt")"}}};
    };
    const auto kept = editor::keep_symbols(exported, {dirt(","), dirt(".")});
    CHECK(kept["rows"] == nlohmann::ordered_json::array({",b"}));
    CHECK(kept["terrain"] == nlohmann::ordered_json::parse(R"({ ",": "t_dirt", "b": "t_floor" })"));
}
