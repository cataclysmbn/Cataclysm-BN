#include "catch/catch.hpp"
#include "editor/vehicle_check.h"
#include "editor/vehicle_design.h"
#include "point.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

struct mount_parts {
    point at;
    std::vector<ordered_json> parts;
};

/// A go-kart: frames five long and three wide, wheels on hubs at the corners, steerable at the
/// front, a diesel engine, a tank of diesel, a car battery and a seat with controls; `changed`
/// mounts hold their parts instead.
auto kart(const std::vector<mount_parts>& changed = {}) -> ordered_json {
    auto mounts = std::vector<mount_parts>{};
    for (auto y = -1; y <= 1; ++y) {
        for (auto x = -2; x <= 2; ++x) {
            mounts.push_back({.at = {x, y}, .parts = {"frame_vertical"}});
        }
    }
    const auto add = [&](const point& at, const std::vector<ordered_json>& parts) {
        auto& entry = *std::ranges::find(mounts, at, &mount_parts::at);
        std::ranges::copy(parts, std::back_inserter(entry.parts));
    };
    add({2, -1}, {"wheel_mount_medium_steerable", "wheel"});
    add({2, 1}, {"wheel_mount_medium_steerable", "wheel"});
    add({-2, -1}, {"wheel_mount_medium", "wheel"});
    add({-2, 1}, {"wheel_mount_medium", "wheel"});
    add({1, 0}, {"diesel_engine_i6"});
    add({-1, 0}, {{{"part", "tank"}, {"fuel", "diesel"}}});
    add({1, 1}, {"battery_car"});
    add({0, 0}, {"seat", "controls"});
    for (const auto& [at, parts] : changed) {
        std::ranges::find(mounts, at, &mount_parts::at)->parts = parts;
    }
    auto listed = ordered_json::array();
    for (const auto& [at, parts] : mounts) {
        listed.push_back({{"x", at.x}, {"y", at.y}, {"parts", parts}});
    }
    return {{"type", "vehicle"}, {"id", "check_kart"}, {"name", "Kart"}, {"parts", listed}};
}

auto checks_of(const ordered_json& prototype) -> std::vector<editor::drive_check> {
    const auto design = editor::build_design(prototype.dump());
    for (const auto& message : design.messages) { INFO(message.text); }
    REQUIRE(design.blueprint);
    const auto parts = editor::design_parts(prototype);
    return editor::drive_checks(*design.blueprint, [&](const int index) {
        const auto at = ordered_json::json_pointer(parts[index].pointer);
        const auto& entry = prototype[at];
        return itype_id(entry.is_object() ? entry.value("fuel", "null") : "null");
    });
}

auto check_of(const std::vector<editor::drive_check>& checks, const std::string& key)
    -> editor::drive_check {
    const auto found = std::ranges::find(checks, key, &editor::drive_check::key);
    INFO(key);
    REQUIRE(found != checks.end());
    return *found;
}

/// The design `fix` makes of `prototype`, which then passes the check `key`.
auto fixed(const ordered_json& prototype, const std::string& key) -> ordered_json {
    const auto failed = check_of(checks_of(prototype), key);
    CHECK_FALSE(failed.ok);
    REQUIRE(failed.fix != editor::drive_fix::none);
    CHECK_FALSE(failed.action.empty());
    const auto next = editor::fixed_design(prototype, editor::read_part_catalog(), failed.fix);
    INFO((next ? std::string() : next.error()));
    REQUIRE(next.has_value());
    CHECK(check_of(checks_of(*next), key).ok);
    return *next;
}

auto parts_at(const ordered_json& prototype, const point& at) -> std::vector<std::string> {
    return editor::design_parts(prototype)
         | std::views::filter([&](const editor::design_part& part) { return part.mount == at; })
         | std::views::transform(&editor::design_part::part) | std::ranges::to<std::vector>();
}

const auto frame = std::vector<ordered_json>{"frame_vertical"};

} // namespace

TEST_CASE("a go-kart with all it needs is ready to drive", "[editor_vehicle_check]") {
    const auto checks = checks_of(kart());
    for (const auto& entry : checks) {
        INFO(entry.key << ": " << entry.text);
        CHECK(entry.ok);
    }
    CHECK(editor::ready_to_drive(checks));
    CHECK(std::ranges::contains(checks, "power", &editor::drive_check::key));
}

TEST_CASE("a kart with no wheels gets wheels and hubs at its corners", "[editor_vehicle_check]") {
    const auto bare = kart(
        {{.at = {2, -1}, .parts = frame},
         {.at = {2, 1}, .parts = frame},
         {.at = {-2, -1}, .parts = frame},
         {.at = {-2, 1}, .parts = frame}});
    CHECK(check_of(checks_of(bare), "wheels").text == "It has no wheels.");
    const auto next = fixed(bare, "wheels");
    CHECK(check_of(checks_of(next), "steering").ok);
    // The wheel the game's vehicles install most, on its hub.
    const auto front = parts_at(next, {2, -1});
    REQUIRE(front.size() == 3);
    CHECK(front[1] == "wheel_mount_medium_steerable");
    CHECK(vpart_id(front[2])->has_flag(VPFLAG_WHEEL));
    const auto back = parts_at(next, {-2, 1});
    REQUIRE(back.size() == 3);
    CHECK(back[1] == "wheel_mount_medium");
    CHECK(back[2] == front[2]);
}

TEST_CASE("a kart whose wheels cannot steer gets steerable front hubs", "[editor_vehicle_check]") {
    const auto stiff = kart(
        {{.at = {2, -1}, .parts = {"frame_vertical", "wheel_mount_medium", "wheel"}},
         {.at = {2, 1}, .parts = {"frame_vertical", "wheel_mount_medium", "wheel"}}});
    CHECK(check_of(checks_of(stiff), "steering").text == "No wheel can steer.");
    const auto next = fixed(stiff, "steering");
    CHECK(parts_at(next, {2, 1})
          == std::vector<std::string>{"frame_vertical", "wheel_mount_medium_steerable", "wheel"});
    // The back wheels stay as they were.
    CHECK(parts_at(next, {-2, 1})
          == std::vector<std::string>{"frame_vertical", "wheel_mount_medium", "wheel"});
}

TEST_CASE(
    "a diesel kart with an empty tank gets the tank filled with diesel", "[editor_vehicle_check]") {
    const auto empty = kart({{.at = {-1, 0}, .parts = {"frame_vertical", "tank"}}});
    CHECK(check_of(checks_of(empty), "fuel").text == "No tank holds diesel for the engine.");
    CHECK(check_of(checks_of(empty), "fuel").action == "Fill the tanks with diesel");
    const auto next = fixed(empty, "fuel");
    CHECK(next["parts"][6]["parts"][1] == ordered_json{{"part", "tank"}, {"fuel", "diesel"}});

    // With no tank at all, one comes, full of diesel.
    const auto tankless = kart({{.at = {-1, 0}, .parts = frame}});
    CHECK_FALSE(check_of(checks_of(tankless), "fuel").ok);
    fixed(tankless, "fuel");
}

TEST_CASE("a diesel kart with no battery gets one", "[editor_vehicle_check]") {
    const auto flat = kart({{.at = {1, 1}, .parts = frame}});
    CHECK(check_of(checks_of(flat), "battery").text == "The engine needs a battery to start.");
    fixed(flat, "battery");
}

TEST_CASE("a kart with controls but no seat gets a seat under them", "[editor_vehicle_check]") {
    const auto standing = kart({{.at = {0, 0}, .parts = {"frame_vertical", "controls"}}});
    CHECK(
        check_of(checks_of(standing), "controls").text == "No seat has controls to drive it from.");
    const auto next = fixed(standing, "controls");
    CHECK(check_of(checks_of(next), "controls").ok);

    // No seat and no controls: both come on one square.
    const auto empty = kart({{.at = {0, 0}, .parts = frame}});
    fixed(empty, "controls");
}

TEST_CASE(
    "a kart with no engine is not ready, and its fix only points at engines",
    "[editor_vehicle_check]") {
    const auto pushed = kart({{.at = {1, 0}, .parts = frame}});
    const auto checks = checks_of(pushed);
    CHECK_FALSE(editor::ready_to_drive(checks));
    CHECK(check_of(checks, "engine").fix == editor::drive_fix::show_engines);
    // Nothing else depends on an engine it lacks.
    CHECK_FALSE(std::ranges::contains(checks, "fuel", &editor::drive_check::key));
    CHECK_FALSE(std::ranges::contains(checks, "power", &editor::drive_check::key));
    CHECK(editor::fixed_design(pushed, editor::read_part_catalog(), editor::drive_fix::show_engines)
          == pushed);
}

TEST_CASE(
    "a wheel's hub is the one the game's vehicles use, steerable at the front",
    "[editor_vehicle_check]") {
    const auto catalog = editor::read_part_catalog();
    const auto bare = kart(
        {{.at = {2, -1}, .parts = frame},
         {.at = {-2, -1}, .parts = frame},
         {.at = {0, -1}, .parts = frame}});
    const auto design = editor::build_design(bare.dump());
    REQUIRE(design.blueprint);
    const auto hub = [&](const point& at) {
        return editor::prerequisite_of(catalog, *design.blueprint, {.mount = at, .part = "wheel"})
            .value_or(vpart_id::NULL_ID());
    };
    CHECK(hub({2, -1}) == vpart_id("wheel_mount_medium_steerable"));
    CHECK(hub({-2, -1}) == vpart_id("wheel_mount_medium"));
    // The middle is no front.
    CHECK(hub({0, -1}) == vpart_id("wheel_mount_medium"));
    // A mount that has its hub needs none; a square with no frame takes none.
    CHECK(hub({2, 1}) == vpart_id::NULL_ID());
    CHECK(hub({4, 0}) == vpart_id::NULL_ID());
    // A part that needs nothing first.
    CHECK_FALSE(
        editor::prerequisite_of(catalog, *design.blueprint, {.mount = {0, -1}, .part = "seat"}));
}

TEST_CASE(
    "a part search ranks the whole name, then a word of it, the rest, the id",
    "[editor_vehicle_check]") {
    CHECK(editor::search_rank("wheel", "wheel", "Wheel") == 0);
    CHECK(editor::search_rank("wheel hub assembly", "wheel_mount_medium", "wheel") == 1);
    CHECK(editor::search_rank("vehicle tank (60L)", "tank", "tank") == 1);
    CHECK(editor::search_rank("flywheel", "flywheel", "wheel") == 2);
    CHECK(editor::search_rank("frame", "frame_vertical", "vertical") == 3);
    CHECK_FALSE(editor::search_rank("frame", "frame_vertical", "wheel"));
}
