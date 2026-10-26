#include "avatar.h"
#include "catch/catch.hpp"
#include "editor/mapgen_export.h"
#include "editor/mapgen_stamp.h"
#include "map/map.h"
#include "overmap/omdata.h"
#include "overmap/overmapbuffer.h"
#include "state_helpers.h"
#include "trap.h"
#include "type_id.h"

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

    const auto exported = editor::export_omt(omt);
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
    const auto exported = editor::export_omt(omt);
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
    const auto exported = editor::export_omt(far);
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
    const auto exported = editor::export_omt(omt);
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
