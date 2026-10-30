#include "catch/catch.hpp"
#include "editor/vehicle_design.h"
#include "point.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

struct mount_part {
    point at;
    std::string part;
};

/// A design of one part on each of `mounts`.
auto design_of(const std::vector<mount_part>& mounts) -> ordered_json {
    auto listed = ordered_json::array();
    for (const auto& [at, part] : mounts) {
        listed.push_back({{"x", at.x}, {"y", at.y}, {"parts", {part}}});
    }
    return {{"type", "vehicle"}, {"id", "shape_test"}, {"name", "Shapes"}, {"parts", listed}};
}

/// The part on the mount `at` of `prototype`.
auto part_at(const ordered_json& prototype, const point& at) -> std::string {
    const auto parts = editor::design_parts(prototype);
    const auto found = std::ranges::find(parts, at, &editor::design_part::mount);
    REQUIRE(found != parts.end());
    return found->part;
}

auto joined(const std::vector<mount_part>& before, const std::vector<mount_part>& after)
    -> ordered_json {
    return editor::joined_shapes(design_of(before), design_of(after), editor::read_part_catalog());
}

} // namespace

TEST_CASE("frames installed in a line join along it", "[editor_vehicle]") {
    const auto next = joined(
        {},
        {{.at = {0, 0}, .part = "frame_vertical"},
         {.at = {0, 1}, .part = "frame_vertical"},
         {.at = {0, 2}, .part = "frame_vertical"}});
    // Side by side, across the vehicle.
    CHECK(part_at(next, {0, 0}) == "frame_horizontal");
    CHECK(part_at(next, {0, 1}) == "frame_horizontal");
    CHECK(part_at(next, {0, 2}) == "frame_horizontal");
    // Front to back, along it.
    const auto along = joined(
        {},
        {{.at = {0, 0}, .part = "frame_horizontal"}, {.at = {1, 0}, .part = "frame_horizontal"}});
    CHECK(part_at(along, {0, 0}) == "frame_vertical");
    CHECK(part_at(along, {1, 0}) == "frame_vertical");
}

TEST_CASE("frames join their corners and crossings", "[editor_vehicle]") {
    const auto next = joined(
        {},
        {{.at = {0, 0}, .part = "frame_vertical"},
         {.at = {1, 0}, .part = "frame_vertical"},
         {.at = {0, 1}, .part = "frame_vertical"},
         {.at = {-1, 0}, .part = "frame_vertical"},
         {.at = {0, -1}, .part = "frame_vertical"}});
    CHECK(part_at(next, {0, 0}) == "frame_cross");
    // Front, and right: the corner that joins them.
    const auto corner = joined(
        {},
        {{.at = {0, 0}, .part = "frame_vertical"},
         {.at = {1, 0}, .part = "frame_vertical"},
         {.at = {0, 1}, .part = "frame_vertical"}});
    CHECK(part_at(corner, {0, 0}) == "frame_sw");
}

TEST_CASE(
    "a frame beside the one installed joins it, and one picked by hand stays", "[editor_vehicle]") {
    // A lone frame takes any shape, so it joins what comes beside it.
    const auto lone = joined(
        {{.at = {0, 0}, .part = "frame_horizontal"}},
        {{.at = {0, 0}, .part = "frame_horizontal"}, {.at = {1, 0}, .part = "frame_vertical"}});
    CHECK(part_at(lone, {0, 0}) == "frame_vertical");
    // Front to back, but picked across: it stays so when a frame comes beside it.
    const auto picked = std::vector<mount_part>{
        {.at = {0, 0}, .part = "frame_horizontal"}, {.at = {1, 0}, .part = "frame_vertical"}};
    auto more = picked;
    more.push_back({.at = {0, 1}, .part = "frame_vertical"});
    const auto kept = joined(picked, more);
    CHECK(part_at(kept, {0, 0}) == "frame_horizontal");
    CHECK(part_at(kept, {0, 1}) == "frame_horizontal");
    // A mount no change came near keeps its shape.
    CHECK(part_at(kept, {1, 0}) == "frame_vertical");
}

TEST_CASE("frames rejoin when one beside them goes", "[editor_vehicle]") {
    const auto next = joined(
        {{.at = {0, 0}, .part = "frame_sw"},
         {.at = {1, 0}, .part = "frame_vertical"},
         {.at = {0, 1}, .part = "frame_horizontal"}},
        {{.at = {0, 0}, .part = "frame_sw"}, {.at = {1, 0}, .part = "frame_vertical"}});
    CHECK(part_at(next, {0, 0}) == "frame_vertical");
}

TEST_CASE("parts without line shapes keep theirs", "[editor_vehicle]") {
    const auto next = joined({}, {{.at = {0, 0}, .part = "seat"}, {.at = {1, 0}, .part = "seat"}});
    CHECK(part_at(next, {0, 0}) == "seat");
    CHECK(part_at(next, {1, 0}) == "seat");
}
