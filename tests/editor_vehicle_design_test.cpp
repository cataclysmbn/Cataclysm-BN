#include "cata_utility.h"
#include "catch/catch.hpp"
#include "editor/document.h"
#include "editor/vehicle_design.h"
#include "item.h"
#include "map/map.h"
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
        CHECK(next.error().find("cannot be mounted at 0,0") != std::string::npos);
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
        CHECK(refused.error().find("cannot be mounted at 0,0") != std::string::npos);
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
    CHECK_FALSE(editor::set_part_fuel(*veh, tank, {.fuel = "rock", .amount = 10}).has_value());
    CHECK_FALSE(editor::set_part_fuel(*veh, 0, {.fuel = fuel, .amount = 10}).has_value());
}
