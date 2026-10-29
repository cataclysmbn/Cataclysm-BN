#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/vehicle_design.h"
#include "game_constants.h"
#include "item.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map_helpers.h"
#include "mod_manager.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "world.h"
#include "worldfactory.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using nlohmann::ordered_json;

auto read_file(const std::filesystem::path& path) -> std::string {
    auto in = std::ifstream(path, std::ios::binary);
    auto text = std::ostringstream{};
    text << in.rdbuf();
    return text.str();
}

/// The vehicle prototype `id` as its core data file has it.
auto core_prototype(const std::string& id) -> ordered_json {
    const auto path = editor::prototype_file(id);
    REQUIRE(path.has_value());
    const auto doc = editor::document::open(*path);
    REQUIRE(doc.has_value());
    const auto found = std::ranges::find_if(doc->objects(), [&](const auto& entry) {
        return entry.value.value("id", "") == id && entry.value.value("type", "") == "vehicle";
    });
    REQUIRE(found != doc->objects().end());
    return found->value;
}

auto built(const ordered_json& prototype) -> editor::built_design {
    auto design = editor::build_design(prototype.dump());
    INFO(prototype.value("id", ""));
    for (const auto& message : design.messages) { INFO(message.text); }
    REQUIRE(design.blueprint);
    return design;
}

/// The prototypes a loaded mod other than the core defines too, which the game builds from both.
auto overridden() -> std::set<std::string> {
    auto ids = std::set<std::string>{};
    for (const auto& mod : world_generator->active_world->info->active_mod_order) {
        if (mod == mod_management::get_default_core_content_pack()) { continue; }
        for (const auto& file : std::filesystem::recursive_directory_iterator(mod->path)) {
            if (file.path().extension() != ".json") { continue; }
            const auto json = ordered_json::parse(read_file(file.path()), nullptr, false);
            for (const auto& entry : json.is_array() ? json : ordered_json::array({json})) {
                if (entry.is_object() && entry.value("type", "") == "vehicle") {
                    ids.insert(entry.value("id", ""));
                }
            }
        }
    }
    return ids;
}

} // namespace

TEST_CASE(
    "every core vehicle prototype opens byte for byte and builds as the game builds it",
    "[editor_vehicle]") {
    const auto skipped = overridden();
    auto prototypes = 0;
    for (const auto& file : std::filesystem::directory_iterator("data/json/vehicles")) {
        const auto doc = editor::document::open(file.path());
        INFO(file.path().generic_string());
        REQUIRE(doc.has_value());
        // An unchanged document saves the bytes it was opened from.
        CHECK(doc->text() == read_file(file.path()));
        for (const auto& entry : doc->objects()) {
            if (entry.value.value("type", "") != "vehicle") { continue; }
            const auto id = vproto_id(entry.value.value("id", ""));
            INFO(id.str());
            if (skipped.contains(id.str())) { continue; }
            REQUIRE(id.is_valid());
            const auto design = editor::build_design(entry.value.dump());
            REQUIRE(design.blueprint);
            CHECK(design.messages.empty());
            const auto& game = *id->blueprint;
            CHECK(design.blueprint->part_count() == game.part_count());
            CHECK(design.blueprint->total_mass() == game.total_mass());
            // Each part the JSON names is one the blueprint holds, in the same order.
            const auto named = editor::design_parts(entry.value);
            REQUIRE(static_cast<int>(named.size()) == game.part_count());
            for (const auto& [index, part] : named | std::views::enumerate) {
                CHECK(game.cpart(static_cast<int>(index)).info().get_id().str() == part.part);
                CHECK(game.cpart(static_cast<int>(index)).mount.xy().raw() == part.mount);
            }
            ++prototypes;
        }
    }
    CHECK(prototypes > 250);
}

TEST_CASE(
    "a mount that refuses a part says which of the game's rules it breaks", "[editor_vehicle]") {
    const auto design = built(core_prototype("bicycle"));
    const auto& bicycle = *design.blueprint;
    const auto refusal = [&](const point& at, const char* const part) {
        return editor::mount_refusal(bicycle, at, vpart_id(part)).value_or("");
    };
    CHECK(refusal({1, 0}, "headlight").empty());
    CHECK(refusal({2, 0}, "headlight") == "Headlight goes on a frame: install a frame here first.");
    CHECK(
        refusal({5, 5}, "xlframe_vertical") == "Extra light frame must touch the vehicle's frame.");
    CHECK(refusal({0, 0}, "xlframe_vertical").find("already") != std::string::npos);
}

TEST_CASE(
    "a mount refuses a part for the reason the game has, by every rule of its own",
    "[editor_vehicle]") {
    const auto design = built(ordered_json::parse(R"({
        "type": "vehicle", "id": "refusal_test", "name": "Refusal test",
        "parts": [
            { "x": 0, "y": 0, "parts": [ "xlframe_vertical", "board_vertical_left" ] },
            { "x": 1, "y": 0, "parts": [ "xlframe_vertical", "inboard_mirror" ] },
            { "x": 0, "y": 1, "parts": [ "xlframe_vertical", "turret_mount_manual_steel", "m249" ] },
            { "x": 2, "y": 0, "parts": [ "mounted_spare_tire" ] }
        ]
    })"));
    const auto& veh = *design.blueprint;
    const auto said =
        design.messages | std::views::transform(&editor::loader_message::text)
        | std::ranges::to<std::vector>();
    CAPTURE(said);
    REQUIRE(veh.part_count() == 8);
    const auto refusal = [&](const point& at, const char* const part) {
        return editor::mount_refusal(veh, at, vpart_id(part)).value_or("");
    };
    // A protrusion is no frame to build on from.
    CHECK(refusal({3, 0}, "xlframe_vertical").ends_with("must touch the vehicle's frame."));
    CHECK(refusal({0, 0}, "inboard_mirror").find("clear view") != std::string::npos);
    CHECK(refusal({1, 0}, "board_vertical_left").find("block the view") != std::string::npos);
    CHECK(refusal({0, 1}, "turret_mount").find("already has a turret mount") != std::string::npos);
    CHECK(refusal({0, 1}, "controls_turret").find("by hand") != std::string::npos);
    CHECK(refusal({0, 1}, "laser_designator_part").find("by hand") != std::string::npos);
    // Each refusal names what is in the way, never an empty list.
    for (const auto* const part :
         {"inboard_mirror", "board_vertical_left", "turret_mount", "controls_turret"}) {
        for (const auto& at : {point(0, 0), point(1, 0), point(0, 1), point(3, 0), point(5, 5)}) {
            const auto said = refusal(at, part);
            INFO(part << " at " << at.to_string() << ": " << said);
            CHECK(said.find("with .") == std::string::npos);
        }
    }
}

TEST_CASE(
    "mounts alike on the ground share a refusal, whatever stands above them", "[editor_vehicle]") {
    auto design = built(ordered_json::parse(R"({
        "type": "vehicle", "id": "level_refusal_test", "name": "Level refusal test",
        "parts": [ { "x": 0, "y": 0, "parts": [ "xlframe_vertical" ] } ]
    })"));
    auto& veh = *design.blueprint;
    // A frame a level up at 3,0, where the ground level has none.
    REQUIRE(veh.install_part(tripoint_mnt_veh(3, 0, 1), vpart_id("xlframe_vertical"), true) >= 0);
    const auto refusals = editor::mount_refusals(veh, vpart_id("seat"));
    CHECK_FALSE(refusals({0, 0}).has_value());
    const auto above = refusals({3, 0});
    CHECK(above.has_value());
    CHECK(above == editor::mount_refusal(veh, {3, 0}, vpart_id("seat")));
}

TEST_CASE(
    "a part installed on a world vehicle is found where it stands once the map moves away",
    "[editor_vehicle]") {
    clear_map();
    auto& here = get_map();
    const auto origin = here.get_abs_sub();
    const auto cleanup = on_out_of_scope([&] {
        get_map().load(origin, false);
        clear_vehicles();
    });
    auto* const veh = editor::spawn_design(vproto_id("bicycle"), tripoint_bub_ms(60, 60, 0), {});
    REQUIRE(veh != nullptr);
    const auto tally =
        editor::install_live_parts(*veh, {.mounts = {{2, 0}}, .part = "xlframe_vertical"});
    REQUIRE(tally.has_value());
    REQUIRE(tally->done == 1);
    const auto installed = veh->parts_at_relative(tripoint_mnt_veh(2, 0, 0), false);
    REQUIRE(installed.size() == 1);
    const auto at = veh->abs_part_location(veh->cpart(installed.front()));
    // Off the reality bubble, the map buffer finds a vehicle by the squares it indexed.
    here.load(origin + point_rel_sm(MAPSIZE, 0), false);
    REQUIRE_FALSE(here.inbounds(abs_to_map_local(here, at)));
    const auto found = here.get_mapbuffer().veh_at(at);
    REQUIRE(found.has_value());
    CHECK(&found->vehicle() == veh);
    CHECK(static_cast<int>(found->part_index()) == installed.front());
}

TEST_CASE("a vehicle prototype is found in the data file that defines it", "[editor_vehicle]") {
    const auto path = editor::prototype_file("bicycle");
    REQUIRE(path.has_value());
    CHECK(path->filename() == "bikes.json");
    CHECK_FALSE(editor::prototype_file("no_such_vehicle_prototype").has_value());
}

TEST_CASE(
    "installing a part follows the game's mount rules and redraws the blueprint",
    "[editor_vehicle]") {
    const auto bicycle = core_prototype("bicycle");
    const auto design = built(bicycle);
    const auto count = design.blueprint->part_count();

    SECTION("a part the mount takes goes after the parts listed there") {
        const auto next = editor::
            install_part(bicycle, *design.blueprint, {.mount = {1, 0}, .part = "headlight"});
        REQUIRE(next.has_value());
        CHECK(next->at("parts")[1]["parts"].back() == "headlight");
        CHECK(next->at("parts").size() == bicycle.at("parts").size());
        const auto lit = built(*next);
        CHECK(lit.blueprint->part_count() == count + 1);
        // The blueprint shows what the game shows on each mount, now the light at the front.
        CHECK(next->at("blueprint") == ordered_json(editor::blueprint_rows(*lit.blueprint)));
        CHECK(next->at("blueprint")[0].get<std::string>().size() == 3);
    }
    SECTION("a frame beside the vehicle starts a mount of its own and widens the blueprint") {
        const auto next = editor::
            install_part(bicycle, *design.blueprint, {.mount = {2, 0}, .part = "xlframe_vertical"});
        REQUIRE(next.has_value());
        CHECK(next->at("parts").back()
              == ordered_json::parse(R"({"x":2,"y":0,"parts":["xlframe_vertical"]})"));
        const auto rows = next->at("blueprint");
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].get<std::string>().size() == 4);
    }
    SECTION("a second frame on a mount is refused as the game refuses it") {
        const auto next = editor::
            install_part(bicycle, *design.blueprint, {.mount = {0, 0}, .part = "xlframe_vertical"});
        REQUIRE_FALSE(next.has_value());
        CHECK(next.error().find("already") != std::string::npos);
    }
    SECTION("a part away from the vehicle is refused") {
        CHECK_FALSE(
            editor::install_part(bicycle, *design.blueprint, {.mount = {5, 5}, .part = "headlight"})
                .has_value());
    }
    SECTION("an unknown part is refused") {
        const auto next =
            editor::install_part(bicycle, *design.blueprint, {.mount = {1, 0}, .part = "no_part"});
        REQUIRE_FALSE(next.has_value());
        CHECK(next.error() == "no_part is no vehicle part.");
    }
}

TEST_CASE("removing a part follows the game's unmount rules", "[editor_vehicle]") {
    const auto bicycle = core_prototype("bicycle");
    const auto design = built(bicycle);
    const auto parts = editor::design_parts(bicycle);
    const auto index_of = [&](const std::string& part) {
        const auto found = std::ranges::find(parts, part, &editor::design_part::part);
        REQUIRE(found != parts.end());
        return static_cast<std::size_t>(found - parts.begin());
    };

    SECTION("a part nothing rests on goes, and only it") {
        const auto next =
            editor::remove_part(bicycle, *design.blueprint, index_of("basketsm_bike_rear"));
        REQUIRE(next.has_value());
        CHECK(next->at("parts")[2]["parts"].size() == 3);
        CHECK(built(*next).blueprint->part_count() == design.blueprint->part_count() - 1);
    }
    SECTION("a frame with parts on it stays, with the game's reason") {
        const auto next =
            editor::remove_part(bicycle, *design.blueprint, index_of("xlframe_vertical_2"));
        REQUIRE_FALSE(next.has_value());
        CHECK_FALSE(next.error().empty());
    }
    SECTION("a part the blueprint's palette places stays") {
        const auto tanker = core_prototype("afs_water_tanker");
        const auto placed = built(tanker);
        REQUIRE(editor::design_parts(tanker).front().from_palette);
        const auto next = editor::remove_part(tanker, *placed.blueprint, 0);
        REQUIRE_FALSE(next.has_value());
        CHECK(next.error() == "The blueprint's palette places this part; edit the palette.");
    }
}

TEST_CASE("removing parts together keeps the vehicle in one piece", "[editor_vehicle]") {
    /// A design of a frame on each of `mounts`.
    const auto frames = [](const std::vector<point>& mounts) {
        auto parts = ordered_json::array();
        for (const auto& at : mounts) {
            parts.push_back({{"x", at.x}, {"y", at.y}, {"parts", {"frame_cross"}}});
        }
        return ordered_json{
            {"id", "frame_square"}, {"type", "vehicle"}, {"name", "Square"}, {"parts", parts}};
    };
    const auto mounts_left = [](const ordered_json& prototype) {
        return editor::design_parts(prototype) | std::views::transform(&editor::design_part::mount)
             | std::ranges::to<std::set>();
    };

    SECTION("of two opposite corners of a square, the second stays to hold the others together") {
        const auto done =
            editor::remove_parts(frames({{0, 0}, {1, 0}, {0, 1}, {1, 1}}), {{0, 0}, {1, 1}});
        REQUIRE(done.has_value());
        CHECK(done->tally.done == 1);
        CHECK(done->tally.refused == 1);
        CHECK(done->tally.reason == "Removing this part would split the vehicle.");
        CHECK(mounts_left(done->prototype) == std::set<point>{{1, 0}, {0, 1}, {1, 1}});
    }
    SECTION("the middle and a corner of a square go, the rest joined the long way around") {
        const auto square = frames(
            {{0, 0}, {1, 0}, {2, 0}, {0, 1}, {1, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}});
        const auto done = editor::remove_parts(square, {{1, 1}, {0, 0}});
        REQUIRE(done.has_value());
        CHECK(done->tally.done == 2);
        CHECK(done->tally.refused == 0);
        CHECK(mounts_left(done->prototype)
              == std::set<point>{{1, 0}, {2, 0}, {0, 1}, {2, 1}, {0, 2}, {1, 2}, {2, 2}});
    }
    SECTION("the blueprint is drawn again from what is left") {
        auto row = frames({{0, 0}, {1, 0}, {2, 0}});
        row["blueprint"] = ordered_json::array({"###"});
        const auto done = editor::remove_parts(row, {{2, 0}});
        REQUIRE(done.has_value());
        CHECK(done->prototype.at("blueprint")
              == ordered_json(editor::blueprint_rows(*built(done->prototype).blueprint)));
        CHECK(done->prototype.at("blueprint")[0].get<std::string>().size() == 2);
    }
}

TEST_CASE("a part moves within its mount's install order as the game allows", "[editor_vehicle]") {
    const auto bicycle = core_prototype("bicycle");
    const auto listed = [](const ordered_json& prototype) {
        return prototype.at("parts")[0]["parts"].get<std::vector<std::string>>();
    };

    SECTION("a part goes before the one installed before it on its mount") {
        const auto next = editor::move_part(bicycle, 2, -1);
        REQUIRE(next.has_value());
        CHECK(
            listed(*next)
            == std::vector<
                std::string>{"xlframe_vertical_2", "horn_bicycle", "saddle_pedal", "foot_pedals"});
        CHECK(built(*next).messages.empty());
    }
    SECTION("the last part on a mount goes no later") {
        const auto next = editor::move_part(bicycle, 3, 1);
        REQUIRE_FALSE(next.has_value());
        CHECK(next.error() == "The part is already at that end of its mount.");
    }
    SECTION("a part the blueprint's palette places stays") {
        const auto tanker = core_prototype("afs_water_tanker");
        const auto next = editor::move_part(tanker, 0, 1);
        REQUIRE_FALSE(next.has_value());
        CHECK(next.error() == "The blueprint's palette places this part; edit the palette.");
    }
}

TEST_CASE("a prototype the game cannot read reports why", "[editor_vehicle]") {
    auto broken = core_prototype("bicycle");
    broken["parts"][0]["parts"][0] = "no_such_part";
    const auto design = editor::build_design(broken.dump());
    REQUIRE(design.blueprint);
    REQUIRE_FALSE(design.messages.empty());
    CHECK(design.messages.front().text.find("no_such_part") != std::string::npos);
    CHECK_FALSE(editor::build_design(R"({"type": "vehicle"})").blueprint);
}

TEST_CASE("a prototype's vehicle tells its speed, power, mass and cargo", "[editor_vehicle]") {
    const auto design = built(core_prototype("motorcycle_cross"));
    const auto stats = editor::design_stats(*design.blueprint);
    const auto keys =
        stats | std::views::transform(&engine_client::panel_stat::key)
        | std::ranges::to<std::vector>();
    CHECK(keys == std::vector<std::string>{"speed", "power", "mass", "cargo_volume"});
    CHECK(stats[1].value != "0");
    CHECK(std::stoi(stats[0].max) > 0);
}

TEST_CASE(
    "a vehicle's design exported, spawned and exported again is the same design",
    "[editor_vehicle]") {
    clear_map();
    const auto original = core_prototype("car");
    const auto restore = on_out_of_scope([&] {
        REQUIRE(editor::apply_design(original.dump()).has_value());
        clear_vehicles();
    });
    const auto spawn = [](const tripoint_bub_ms& at) {
        auto* const veh =
            editor::spawn_design(vproto_id("car"), at, {.facing = 0, .fuel = 50, .condition = -1});
        REQUIRE(veh != nullptr);
        return veh;
    };
    auto* const first = spawn(tripoint_bub_ms(30, 30, 0));
    const auto exported = editor::export_design(*first, "car");
    CHECK(exported["name"].get<std::string>() == first->name);
    CHECK(exported["blueprint"] == ordered_json(editor::blueprint_rows(*first)));
    auto listed = std::size_t{0};
    for (const auto& entry : exported["parts"]) { listed += entry["parts"].size(); }
    CHECK(static_cast<int>(listed) == first->part_count());
    // A design, not a state: no damage, fill level or cargo.
    const auto text = exported.dump();
    for (const auto* const state : {"hp", "damage", "items", "ammo_qty", "\"ammo\""}) {
        INFO(state);
        CHECK(text.find(state) == std::string::npos);
    }
    CHECK(text.find("\"fuel\"") != std::string::npos);

    REQUIRE(editor::apply_design(text).has_value());
    auto* const second = spawn(tripoint_bub_ms(30, 50, 0));
    CHECK(editor::export_design(*second, "car") == exported);
}

TEST_CASE("a vehicle copied through its saved state is the same vehicle", "[editor_vehicle]") {
    clear_map();
    const auto cleanup = on_out_of_scope([] { clear_vehicles(); });
    auto& here = get_map();
    auto* const source = editor::spawn_design(
        vproto_id("car"), tripoint_bub_ms(30, 30, 0), {.facing = 90, .fuel = 50, .condition = -1});
    REQUIRE(source != nullptr);
    source->name = "Copied car";
    const auto cargo = source->get_avail_parts(VPFLAG_CARGO);
    REQUIRE_FALSE(cargo.empty());
    const auto trunk = cargo.begin()->part_index();
    source->add_item(trunk, item::spawn(itype_id("rock")));
    REQUIRE_FALSE(source->get_items(trunk).empty());
    const auto state = editor::vehicle_state(*source);
    const auto vehicles = here.get_vehicles().size();

    SECTION("placed elsewhere, it saves as the original does but for its place") {
        const auto copy = editor::place_vehicle_state(state, tripoint_bub_ms(30, 50, 0));
        REQUIRE(copy.has_value());
        CHECK(here.get_vehicles().size() == vehicles + 1);
        const auto placed = [](const std::string& text) {
            auto saved = ordered_json::parse(text);
            for (const auto* const key : {"posx", "posy", "om_id", "last_update_turn"}) {
                saved.erase(key);
            }
            return saved;
        };
        CHECK(placed(editor::vehicle_state(**copy)) == placed(state));
        CHECK((*copy)->name == "Copied car");
        CHECK(here.veh_at(tripoint_bub_ms(30, 50, 0))->vehicle().name == "Copied car");
    }
    SECTION("placed over the original, it is refused and leaves nothing behind") {
        const auto copy = editor::place_vehicle_state(state, tripoint_bub_ms(30, 30, 0));
        REQUIRE_FALSE(copy.has_value());
        CHECK(copy.error() == "Something is in the way of the vehicle there.");
        CHECK(here.get_vehicles().size() == vehicles);
    }
    SECTION("text that is no saved vehicle is refused") {
        const auto copy = editor::place_vehicle_state("{}", tripoint_bub_ms(30, 50, 0));
        REQUIRE_FALSE(copy.has_value());
        CHECK(copy.error() == "That is no saved vehicle.");
        CHECK(here.get_vehicles().size() == vehicles);
    }
    SECTION("a saved vehicle the loader refuses is refused in words") {
        const auto copy = editor::
            place_vehicle_state(R"({"parts": [], "velocity": "fast"})", tripoint_bub_ms(30, 50, 0));
        REQUIRE_FALSE(copy.has_value());
        CHECK_FALSE(copy.error().empty());
        CHECK(copy.error().find("Json error") == std::string::npos);
        CHECK(copy.error().find("<unknown source file>") == std::string::npos);
        CHECK(here.get_vehicles().size() == vehicles);
    }
}

TEST_CASE(
    "a world vehicle is edited in place without the game's requirements and put back from its "
    "saved state",
    "[editor_vehicle]") {
    clear_map();
    const auto cleanup = on_out_of_scope([] { clear_vehicles(); });
    auto* veh = editor::spawn_design(
        vproto_id("bicycle"), tripoint_bub_ms(30, 30, 0), {.facing = 0, .fuel = 0, .condition = 0});
    REQUIRE(veh != nullptr);
    const auto before = editor::vehicle_state(*veh);
    const auto count = veh->part_count();
    const auto index_of = [&](const std::string& id) {
        const auto all = std::views::iota(0, veh->part_count());
        const auto found = std::ranges::find_if(all, [&](const int index) {
            return veh->cpart(index).info().get_id().str() == id;
        });
        REQUIRE(found != all.end());
        return *found;
    };

    SECTION("a part goes on a mount the game takes it on, and not on one it refuses") {
        const auto lit = editor::install_live_part(*veh, {.mount = {1, 0}, .part = "headlight"});
        REQUIRE(lit.has_value());
        CHECK(veh->part_count() == count + 1);
        CHECK(veh->cpart(*lit).info().get_id().str() == "headlight");
        const auto refused =
            editor::install_live_part(*veh, {.mount = {0, 0}, .part = "xlframe_vertical"});
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().find("already") != std::string::npos);
        CHECK(veh->part_count() == count + 1);
    }
    SECTION("a part nothing rests on comes off, a frame with parts on it stays") {
        CHECK_FALSE(editor::remove_live_part(*veh, index_of("xlframe_vertical_2")).has_value());
        CHECK(veh->part_count() == count);
        const auto basket = index_of("basketsm_bike_rear");
        CHECK(editor::remove_live_part(*veh, basket).has_value());
        CHECK(veh->part_count() == count - 1);
        CHECK_FALSE(editor::remove_live_part(*veh, count + 5).has_value());
    }
    SECTION("a part's health is set in percent") {
        REQUIRE(editor::set_part_health(*veh, 0, 50).has_value());
        CHECK(veh->cpart(0).health_percent() == Approx(0.5).margin(0.02));
        CHECK_FALSE(editor::set_part_health(*veh, 0, 101).has_value());
    }
    SECTION("cargo goes in a cargo part only") {
        const auto basket = index_of("basketsm_bike_rear");
        REQUIRE(editor::add_part_cargo(*veh, basket, "rock").has_value());
        CHECK(veh->get_items(basket).size() == 1);
        CHECK_FALSE(editor::add_part_cargo(*veh, basket, "no_such_item").has_value());
        CHECK_FALSE(editor::add_part_cargo(*veh, 0, "rock").has_value());
    }
    SECTION("the saved state puts the vehicle back as it was, where it was") {
        REQUIRE(editor::install_live_part(*veh, {.mount = {1, 0}, .part = "headlight"}));
        veh->name = "Changed";
        const auto at = veh->bub_ms_location();
        const auto restored = editor::restore_vehicle_state(*veh, before);
        REQUIRE(restored.has_value());
        veh = *restored;
        CHECK(veh->part_count() == count);
        CHECK(veh->name == "Bicycle");
        CHECK(veh->bub_ms_location() == at);
        CHECK(get_map().get_vehicles().size() == 1);
    }
}

TEST_CASE("a tank's fuel is set to an amount of what it holds", "[editor_vehicle]") {
    clear_map();
    const auto cleanup = on_out_of_scope([] { clear_vehicles(); });
    auto* const veh = editor::spawn_design(
        vproto_id("car"), tripoint_bub_ms(30, 30, 0), {.facing = 0, .fuel = 50, .condition = 0});
    REQUIRE(veh != nullptr);
    const auto all = std::views::iota(0, veh->part_count());
    const auto found = std::ranges::find_if(all, [&](const int index) {
        return veh->cpart(index).is_tank() && !veh->cpart(index).ammo_current().is_null();
    });
    REQUIRE(found != all.end());
    const auto tank = *found;
    const auto fuel = veh->cpart(tank).ammo_current().str();
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = fuel, .amount = 100}).has_value());
    CHECK(veh->cpart(tank).ammo_remaining() == 100);
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = fuel, .amount = 0}).has_value());
    CHECK(veh->cpart(tank).ammo_remaining() == 0);
    // An emptied tank is filled again with what it held, which it no longer names.
    const auto none = veh->cpart(tank).ammo_current().str();
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = none, .amount = 10}).has_value());
    CHECK(veh->cpart(tank).ammo_current().str() == fuel);
    CHECK(veh->cpart(tank).ammo_remaining() == 10);
    // What it held before never wins over what it holds now, filled some other way.
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = fuel, .amount = 0}).has_value());
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = "", .amount = 0}).has_value());
    veh->part(tank).ammo_set(itype_id("water_clean"), 50);
    REQUIRE(editor::set_part_fuel(*veh, tank, {.fuel = "", .amount = 20}).has_value());
    CHECK(veh->cpart(tank).ammo_current().str() == "water_clean");
    CHECK(veh->cpart(tank).ammo_remaining() == 20);
    // Emptied some other way, it names no fuel left over from before.
    veh->part(tank).ammo_unset();
    const auto refilled = editor::set_part_fuel(*veh, tank, {.fuel = "", .amount = 20});
    REQUIRE_FALSE(refilled.has_value());
    CHECK(refilled.error() == "Choose the Fuel type first.");
    CHECK_FALSE(editor::set_part_fuel(*veh, tank, {.fuel = "rock", .amount = 10}).has_value());
    CHECK_FALSE(editor::set_part_fuel(*veh, 0, {.fuel = fuel, .amount = 10}).has_value());
}

TEST_CASE("a step names a reorder only when it keeps every mounted part", "[editor_vehicle]") {
    const auto before = core_prototype("bicycle");
    const auto label = [&](const ordered_json& after) {
        return editor::design_step_label(
            {.object = 0,
             .patch = ordered_json::diff(before, after),
             .before = before,
             .after = after});
    };
    auto without_front = before;
    without_front["parts"].erase(1);
    CHECK(label(without_front)
          == "Remove 3 parts (bicycle wheel, extra light frame, light wheel mount (steerable))");
    auto with_two = before;
    with_two["parts"].push_back({{"x", 2}, {"y", 0}, {"parts", {"xlframe_vertical", "headlight"}}});
    CHECK(label(with_two) == "Install 2 parts");
    auto with_frames = before;
    with_frames["parts"].push_back({{"x", 2}, {"y", 0}, {"parts", {"xlframe_vertical"}}});
    with_frames["parts"].push_back({{"x", 3}, {"y", 0}, {"parts", {"xlframe_vertical"}}});
    CHECK(label(with_frames) == "Install extra light frame ×2");
    auto reordered = before;
    std::swap(reordered["parts"][0]["parts"][2], reordered["parts"][0]["parts"][3]);
    CHECK(label(reordered) == "Reorder parts at 0,0");
    // A part traded for another is no reorder.
    auto traded = before;
    traded["parts"][0]["parts"][2] = "horn_car";
    CHECK(label(traded).find("Reorder") == std::string::npos);
}

TEST_CASE("a turret is loaded with rounds it fires and emptied", "[editor_vehicle]") {
    clear_map();
    const auto cleanup = on_out_of_scope([] { clear_vehicles(); });
    auto* const veh = editor::spawn_design(
        vproto_id("armored_car"), tripoint_bub_ms(30, 30, 0), {.facing = 0, .fuel = 50});
    REQUIRE(veh != nullptr);
    const auto all = std::views::iota(0, veh->part_count());
    const auto found = std::ranges::find_if(all, [&](const int index) {
        return veh->cpart(index).is_turret();
    });
    REQUIRE(found != all.end());
    const auto turret = *found;
    REQUIRE(editor::set_part_fuel(*veh, turret, {.fuel = "556", .amount = 20}).has_value());
    CHECK(veh->cpart(turret).ammo_current().str() == "556");
    CHECK(veh->cpart(turret).ammo_remaining() == 20);
    REQUIRE(editor::set_part_fuel(*veh, turret, {.fuel = "556", .amount = 0}).has_value());
    CHECK(veh->cpart(turret).ammo_remaining() == 0);
    // An emptied turret loads the rounds it held again.
    const auto none = veh->cpart(turret).ammo_current().str();
    REQUIRE(editor::set_part_fuel(*veh, turret, {.fuel = none, .amount = 5}).has_value());
    CHECK(veh->cpart(turret).ammo_current().str() == "556");
    CHECK(veh->cpart(turret).ammo_remaining() == 5);
    REQUIRE(editor::set_part_fuel(*veh, turret, {.fuel = "556", .amount = 0}).has_value());
    // A machine gun fires no pistol rounds.
    CHECK_FALSE(editor::set_part_fuel(*veh, turret, {.fuel = "9mm", .amount = 10}).has_value());
}

TEST_CASE(
    "the vehicle editor files only frames under Frames, and every working part by what it does",
    "[editor_vehicle]") {
    const auto category = [](const char* const id) { return editor::category_of(*vpart_id(id)); };
    for (const auto& part : vpart_info::get_all()) {
        if (editor::category_of(part) != editor::part_category::frames) { continue; }
        INFO(part.get_id().str());
        CHECK(part.location == "structure");
        CHECK_FALSE(part.has_flag("PROTRUSION"));
        CHECK_FALSE(part.has_flag("EXTENDABLE"));
        CHECK_FALSE(part.has_flag("ENGINE"));
        CHECK_FALSE(part.has_flag("ANIMAL_CTRL"));
        CHECK_FALSE(part.has_flag("PROPELLER"));
    }
    CHECK(category("frame_vertical") == editor::part_category::frames);
    CHECK(category("seed_drill_advanced") == editor::part_category::other);
    CHECK(category("ram_steel") == editor::part_category::other);
    CHECK(category("jet_thruster") == editor::part_category::engines);
    CHECK(category("tank_30gal_drum") == editor::part_category::fuel);
}

TEST_CASE(
    "the vehicle editor names parts of one name apart without saying a word of the name again",
    "[editor_vehicle]") {
    const auto parts = std::vector<const vpart_info*>{
        &*vpart_id("wheel_mount_light_steerable"), &*vpart_id("wheel_mount_light_steerable_wood"),
        &*vpart_id("frame_vertical"), &*vpart_id("frame_horizontal")};
    const auto labels = editor::part_labels(parts);
    CHECK(labels[0] == "light wheel mount (steerable)");
    CHECK(labels[1] == "light wheel mount (steerable) (wood)");
    CHECK(labels[2] == "frame (vertical)");
    CHECK(labels[3] == "frame (horizontal)");
    // Only a whole word of the name goes unsaid: "a" and "ab" are no words of "frame".
    auto short_word = *vpart_id("frame_vertical");
    short_word.id = vpart_id("frame_a");
    auto longer_word = short_word;
    longer_word.id = vpart_id("frame_ab");
    CHECK(editor::part_labels({&short_word, &longer_word})
          == std::vector<std::string>{"frame (a)", "frame (ab)"});
    // Every label of a palette reads differently.
    const auto& all = vpart_info::get_all();
    const auto every = editor::part_labels(
        all | std::views::transform([](const vpart_info& part) { return &part; })
        | std::ranges::to<std::vector>());
    auto sorted = every;
    std::ranges::sort(sorted);
    CHECK(std::ranges::adjacent_find(sorted) == sorted.end());
}
