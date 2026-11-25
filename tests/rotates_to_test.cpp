#include "catch/catch.hpp"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "state_helpers.h"

#include <climits>
#include <sstream>

/// Load real JSON fixtures to check inheritance, clearing and rejection of unknown groups.
TEST_CASE("rotation_groups_load_and_inherit", "[rotates_to]") {
    const auto& row = furn_str_id("f_test_rotates_to").obj();
    const auto& cleared = furn_str_id("f_test_rotates_to_empty").obj();
    const auto& extended = furn_str_id("f_test_rotates_to_extended").obj();
    REQUIRE(row.connect_groups);
    REQUIRE(row.connect_to_groups);
    REQUIRE(row.rotate_to_groups);
    CHECK(row.connect_to_groups->count() == 1);
    CHECK(row.rotate_to_groups->count() == 1);
    CHECK(cleared.rotate_to_groups->none());
    CHECK(cleared.connect_to_groups == row.connect_to_groups);
    CHECK(extended.connect_groups->count() == 2);
    CHECK(extended.connect_to_groups->none());
    CHECK(extended.rotate_to_groups == row.rotate_to_groups);
    CHECK_FALSE(furn_str_id("f_counter").obj().connect_to_groups);
    auto invalid = row;
    auto input = std::istringstream(R"({"rotates_to":["undeclared_rotation_group"]})");
    auto json = JsonIn(input);
    CHECK_THROWS(invalid.load_connections(json.get_object()));
}

/// Query a native map to verify target types, legacy groups and construction overrides.
TEST_CASE("rotation_targets_include_furniture_and_respect_overrides", "[rotates_to]") {
    clear_all_state();
    clear_map();
    auto& here = get_map();
    const auto p = tripoint_bub_ms(60, 60, 0);
    const auto& info = furn_str_id("f_test_rotates_to").obj();
    const auto target = ter_str_id("t_test_rotation_target").id();
    here.ter_set(p + point_east, target);
    here.furn_set(p + point_west, furn_str_id("f_test_rotation_target"));
    here.furn_set(p + point_south, furn_str_id("f_test_rotates_to"));
    here.furn_set(p + point_north, furn_str_id("f_test_legacy_rotation_member"));
    here.build_map_cache(0, true);
    const auto& groups = *info.rotate_to_groups;
    CHECK(here.get_known_rotates_to(p, groups) == 2);
    CHECK(here.get_known_rotates_to_f(p, groups) == 6);
    CHECK(here.get_known_connections_f(p, *info.connect_to_groups) == 1);
    CHECK(here.get_known_connections_f(p, TERCONN_COUNTER) == 8);
    CHECK(here.get_known_rotates_to(p, {}) == CHAR_MAX);
    CHECK(here.get_known_rotates_to_f(p, groups, {{p + point_east, t_grass}}) == 4);
    CHECK(here.get_known_rotates_to_f(
              p, groups, {}, {{p + point_north, furn_str_id("f_test_rotation_target").id()}})
          == 14);
}

#if defined(TILES)
#    include "cata_tiles.h"
#    include "sdl_geometry.h"
#    include "sdltiles.h"
#    include "tile_helpers.h"

namespace {
class rotation_probe: public cata_tiles {
public:
    using cata_tiles::cata_tiles;
    using cata_tiles::get_rotation_and_subtile;
    using cata_tiles::get_tile_values_with_ter;
};
} // namespace

/// Check all masks so facing cannot change a row into a different connection shape.
TEST_CASE("rotation_decoder_preserves_legacy_and_connection_ends", "[rotates_to][tiles]") {
    const auto fixture = tile_context_fixture(true);
    REQUIRE(fixture.valid());
    GeometryRenderer_Ptr geometry = std::make_unique<DefaultGeometryRenderer>();
    auto probe = rotation_probe(get_sdl_renderer(), geometry);
    // Before rotates_to, all sixteen BN connection masks had these frame indices.
    constexpr int legacy[16] = {0, 0, 1, 0, 3, 3, 1, 0, 2, 0, 1, 1, 2, 3, 2, 0};
    for (auto mask = 0; mask < 16; ++mask) {
        auto rotation = -1, shape = -1;
        probe.get_rotation_and_subtile(mask, rotation, shape);
        CHECK(rotation == legacy[mask]);
        for (auto target = 0; target < 16; ++target) {
            auto rotated = -1, new_shape = -1;
            probe.get_rotation_and_subtile(mask, rotated, new_shape, target);
            CHECK(new_shape == shape);
            if (mask == 1 || mask == 2 || mask == 4 || mask == 8) {
                CHECK(rotated % 4 == rotation);
            } else if (mask == 6 || mask == 9) {
                CHECK(rotated % 2 == rotation);
            } else if (mask != 0) {
                CHECK(rotated == rotation);
            }
        }
    }
    // A north-south run can face either side without becoming an east-west run.
    auto west = -1, east = -1, shape = -1;
    probe.get_rotation_and_subtile(9, west, shape, 4);
    probe.get_rotation_and_subtile(9, east, shape, 2);
    CHECK(west == 0);
    CHECK(east == 2);
}

/// Use ASCIITiles to verify the new metadata is independent of UltiCa pack names.
TEST_CASE("rotation_groups_work_without_a_tileset_name_gate", "[rotates_to][tiles]") {
    clear_all_state();
    clear_map();
    const auto fixture = tile_context_fixture(true);
    REQUIRE(fixture.valid());
    GeometryRenderer_Ptr geometry = std::make_unique<DefaultGeometryRenderer>();
    auto probe = rotation_probe(get_sdl_renderer(), geometry);
    probe.load_tileset("ASCIITiles", {}, false, true, false);
    auto& here = get_map();
    const auto p = tripoint_bub_ms(60, 60, 0);
    const auto row = furn_str_id("f_test_rotates_to").id();
    here.furn_set(p, row);
    here.furn_set(p + point_north, row);
    here.furn_set(p + point_south, row);
    const int neighbors[4] = {row.to_i(), 0, 0, row.to_i()};
    auto before = -1, after = -1, shape = -1;
    here.ter_set(p + point_west, ter_str_id("t_test_rotation_target"));
    here.build_map_cache(0, true);
    probe.get_tile_values_with_ter(p, row.to_i(), neighbors, shape, before);
    here.ter_set(p + point_west, t_grass);
    here.ter_set(p + point_east, ter_str_id("t_test_rotation_target"));
    here.invalidate_map_cache(0);
    here.build_map_cache(0, true);
    probe.get_tile_values_with_ter(p, row.to_i(), neighbors, shape, after);
    CHECK(before == 0);
    CHECK(after == 2);
}
#endif
