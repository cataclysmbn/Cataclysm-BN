#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "engine_client_event.h"
#include "engine_client_world.h"
#include "game.h"
#include "game_session.h"
#include "json.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_memory.h"
#include "map_perception.h"
#include "monster.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "rng.h"
#include "state_helpers.h"
#include "weather/weather.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace {
namespace ec = engine_client;

/// A closed lit room around the avatar split by a wall with one doorway, in solid rock.
struct scene {
    tripoint_bub_ms start;
    tripoint_bub_ms behind_wall;
};

auto make_scene() -> scene {
    clear_all_state();
    // Earlier cases may have shown the avatar this area; the scene asserts what it alone reveals.
    get_avatar().clear_map_memory();
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    auto& here = get_map();
    auto& you = get_avatar();
    const auto start = tripoint_bub_ms(60, 60, 0);
    for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            const auto inside = std::abs(x - start.x()) <= 6 && std::abs(y - start.y()) <= 6;
            here.ter_set(tripoint_bub_ms(x, y, 0), ter_id(inside ? "t_floor" : "t_rock"));
        }
    }
    for (const auto y : std::views::iota(-6, 7)) {
        here.ter_set(start + tripoint(2, y, 0), ter_id("t_wall"));
    }
    here.ter_set(start + tripoint(2, 5, 0), ter_id("t_floor"));
    you.setpos(start);
    map_perception::acquire();
    return {.start = start, .behind_wall = start + tripoint(4, -2, 0)};
}

auto step_to(const tripoint_bub_ms& to) -> void {
    get_avatar().setpos(to);
    map_perception::acquire();
}

auto at(const tripoint_bub_ms& p) -> ec::position {
    const auto abs = map_local_to_abs(get_map(), p);
    return {.dim = "", .x = abs.x(), .y = abs.y(), .z = abs.z()};
}

auto find_cell(const ec::world_state& state, const ec::position& p) -> const ec::cell* {
    const auto found = state.cells.find(p);
    return found == state.cells.end() ? nullptr : &found->second;
}

/// Independent oracle: ordinary sight and raw stored memory at z 0, no capture code involved.
auto oracle() -> std::map<ec::position, std::string> {
    auto result = std::map<ec::position, std::string>{};
    auto& here = get_map();
    const auto& you = get_avatar();
    for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
        for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
            const auto p = tripoint_bub_ms(x, y, 0);
            if (you.sees(p)) {
                result[at(p)] = "visible:" + here.ter(p).id().str();
            } else if (const auto remembered = you.get_terrain_tile(map_local_to_abs(here, p));
                       !remembered.tile.empty()) {
                result[at(p)] = "remembered:" + remembered.tile;
            }
        }
    }
    return result;
}

auto observed_state() -> std::string {
    auto& you = get_avatar();
    auto out = std::ostringstream{};
    auto json = JsonOut(out);
    you.serialize(json);
    auto& here = get_map();
    for (const auto z : std::views::iota(-OVERMAP_DEPTH, OVERMAP_HEIGHT + 1)) {
        for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
            for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
                const auto abs = map_local_to_abs(here, tripoint_bub_ms(x, y, z));
                out << you.get_memorized_symbol(abs) << you.get_terrain_tile(abs).tile
                    << you.get_memorized_tile(abs).tile << ';';
            }
        }
    }
    out << to_turn<int>(calendar::turn);
    return out.str();
}

struct subscribe_size {
    std::size_t bytes = 0;
    std::size_t parts = 0;
    std::size_t cells = 0;
    bool ordered = true;
};

/// Whole subscribe payload: header plus every part, counting the cells the parts carry.
auto measure_subscribe(const ec::world_state& world) -> subscribe_size {
    auto value = ec::snapshot{};
    value.value.world = world;
    const auto wire = ec::serialize_snapshot(value);
    REQUIRE(wire);
    auto result = subscribe_size{.bytes = wire->header.size(), .parts = wire->parts.size()};
    for (const auto index : std::views::iota(std::size_t{0}, result.parts)) {
        const auto& text = wire->parts[index];
        result.bytes += text.size();
        auto input = std::istringstream{text};
        auto json = JsonIn{input};
        auto part = json.get_object();
        part.allow_omitted_members();
        result.ordered =
            result.ordered && part.get_int("index") == static_cast<int>(index)
            && part.get_bool("last") == (index + 1 == result.parts);
        result.cells += part.get_array("cells").size();
    }
    return result;
}
} // namespace

TEST_CASE(
    "world capture lists only known cells, visible with live facts", "[engine_client_world]") {
    const auto setup = make_scene();
    const auto state = ec::world::capture_world();

    const auto* floor = find_cell(state, at(setup.start + tripoint_east));
    REQUIRE(floor != nullptr);
    CHECK(floor->known == ec::knowledge::visible);
    REQUIRE(floor->terrain);
    CHECK(floor->terrain->id == "t_floor");
    CHECK(floor->terrain->kind == "terrain");
    CHECK_FALSE(floor->terrain->glyph.empty());
    CHECK_FALSE(floor->terrain->color.empty());
    CHECK_FALSE(floor->memory);

    CHECK(find_cell(state, at(setup.behind_wall)) == nullptr);
    REQUIRE(state.coverage);
    CHECK(state.coverage->min.z == -OVERMAP_DEPTH);
    CHECK(state.coverage->max.z == OVERMAP_HEIGHT);
    REQUIRE(state.avatar);
    CHECK(state.avatar->at == at(setup.start));
    CHECK(state.avatar->id == "e:avatar");
    REQUIRE(state.environment);
    CHECK(state.environment->weather == "clear");
    CHECK(state.entities.empty());
    const auto expected = oracle();
    CHECK(state.cells.size() == expected.size());
    for (const auto& [p, cell] : state.cells) {
        const auto found = expected.find(p);
        REQUIRE(found != expected.end());
        const auto known = cell.known == ec::knowledge::visible ? "visible:" : "remembered:";
        const auto id = cell.terrain ? cell.terrain->id : cell.memory->terrain.id;
        CHECK(found->second == known + id.value_or(""));
    }
}

TEST_CASE(
    "world capture after one avatar step differs exactly by the changed cells",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto before = ec::world::capture_world();
    const auto before_oracle = oracle();
    step_to(setup.start + tripoint(0, 3, 0));
    const auto after = ec::world::capture_world();
    const auto after_oracle = oracle();

    auto expected = std::set<ec::position>{};
    for (const auto& [p, value] : after_oracle) {
        const auto old = before_oracle.find(p);
        if (old == before_oracle.end() || old->second != value) { expected.insert(p); }
    }
    for (const auto& [p, value] : before_oracle) {
        if (!after_oracle.contains(p)) { expected.insert(p); }
    }
    REQUIRE_FALSE(expected.empty());

    auto changed = std::set<ec::position>{};
    for (const auto& [p, cell] : after.cells) {
        const auto old = before.cells.find(p);
        if (old == before.cells.end() || old->second != cell) { changed.insert(p); }
    }
    for (const auto& [p, cell] : before.cells) {
        if (!after.cells.contains(p)) { changed.insert(p); }
    }
    CHECK(changed == expected);
    CHECK(after.avatar->at == at(setup.start + tripoint(0, 3, 0)));
}

TEST_CASE("world capture of a remembered cell carries only memory", "[engine_client_world]") {
    const auto setup = make_scene();
    const auto corner = setup.start + tripoint(-5, -5, 0);
    REQUIRE(find_cell(ec::world::capture_world(), at(corner))->known == ec::knowledge::visible);

    // Stand beyond the partition, out of line of sight of that corner.
    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE_FALSE(get_avatar().sees(corner));
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(corner));
    REQUIRE(cell != nullptr);
    CHECK(cell->known == ec::knowledge::remembered);
    CHECK_FALSE(cell->terrain);
    REQUIRE(cell->memory);
    CHECK(cell->memory->terrain.id == "t_floor");
    CHECK_FALSE(cell->memory->terrain.glyph.empty());
}

TEST_CASE(
    "world capture reports memory on every z level and never a never-seen cell",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto below = map_local_to_abs(get_map(), setup.start + tripoint(1, 0, -3));
    get_avatar().memorize_terrain_tile(below, "t_floor", 0, 0);
    const auto state = ec::world::capture_world();
    const auto* cell =
        find_cell(state, {.dim = "", .x = below.x(), .y = below.y(), .z = below.z()});
    REQUIRE(cell != nullptr);
    CHECK(cell->known == ec::knowledge::remembered);
    CHECK(cell->memory->terrain.id == "t_floor");
    CHECK(std::ranges::none_of(state.cells, [&](const auto& entry) {
        return entry.first.z == below.z() && entry.first.x == below.x() + 1;
    }));
}

TEST_CASE(
    "world entity ids are minted on disclosure and leave with sight", "[engine_client_world]") {
    const auto setup = make_scene();
    // The partition wall hides the east side from the start.
    auto& hidden_zombie = spawn_test_monster("mon_zombie", setup.start + tripoint(5, 0, 0));
    auto& visible_zombie = spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 1, 0));
    map_perception::acquire();
    REQUIRE(get_avatar().sees(visible_zombie));
    REQUIRE_FALSE(get_avatar().sees(hidden_zombie));

    const auto before = ec::world::capture_world();
    REQUIRE(before.entities.size() == 1);
    const auto id = before.entities.begin()->first;
    const auto& seen = before.entities.begin()->second;
    CHECK(id.starts_with("e:"));
    CHECK(id != "e:avatar");
    REQUIRE(seen.appearance);
    CHECK(seen.appearance->kind == "monster");
    CHECK(seen.appearance->id == "mon_zombie");
    CHECK(seen.at == at(visible_zombie.bub_pos()));

    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE_FALSE(get_avatar().sees(visible_zombie));
    const auto after = ec::world::capture_world();
    // The zombie east of the partition is disclosed now, with a fresh id; the other one is gone.
    REQUIRE(after.entities.size() == 1);
    CHECK_FALSE(after.entities.contains(id));
    CHECK(after.entities.begin()->second.at == at(hidden_zombie.bub_pos()));

    // Coming back into view keeps the same opaque id.
    step_to(setup.start);
    const auto again = ec::world::capture_world();
    REQUIRE(again.entities.contains(id));
}

TEST_CASE(
    "world entity id is not reused by a new creature at the same place", "[engine_client_world]") {
    const auto setup = make_scene();
    const auto spot = setup.start + tripoint(-3, 1, 0);
    spawn_test_monster("mon_zombie", spot);
    map_perception::acquire();
    const auto first = ec::world::capture_world();
    REQUIRE(first.entities.size() == 1);
    clear_creatures();
    spawn_test_monster("mon_zombie", spot);
    map_perception::acquire();
    const auto second = ec::world::capture_world();
    REQUIRE(second.entities.size() == 1);
    CHECK(second.entities.begin()->first != first.entities.begin()->first);
}

TEST_CASE("world capture is passive for avatar, memory and RNG", "[engine_client_world]") {
    const auto setup = make_scene();
    spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 1, 0));
    map_perception::acquire();
    const auto engine = rng_get_engine();
    const auto state_before = observed_state();
    const auto first = ec::world::capture_world();
    const auto second = ec::world::capture_world();
    CHECK(first == second);
    CHECK(rng_get_engine() == engine);
    CHECK(observed_state() == state_before);
}

TEST_CASE(
    "world capture lists facts on a cell and the avatar's inventory", "[engine_client_world]") {
    const auto setup = make_scene();
    get_map().add_item_or_charges(setup.start + tripoint_east, item::spawn("rock"));
    get_map().furn_set(setup.start + tripoint_west, furn_id("f_table"));
    get_avatar().i_add(item::spawn("rock"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* east = find_cell(state, at(setup.start + tripoint_east));
    REQUIRE(east->items.size() == 1);
    CHECK(east->items.front().id == "rock");
    const auto* west = find_cell(state, at(setup.start + tripoint_west));
    REQUIRE(west->furniture);
    CHECK(west->furniture->id == "f_table");
    REQUIRE(state.avatar);
    CHECK(std::ranges::any_of(state.avatar->inventory, [](const auto& entry) {
        return entry.appearance.id == "rock";
    }));
}

TEST_CASE(
    "world snapshot size is recorded and splits into several parts", "[engine_client_world]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    game_session::set_running(true);
    map_perception::acquire();
    const auto started = std::chrono::steady_clock::now();
    const auto state = ec::world::capture_world();
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto sent = measure_subscribe(state);
    WARN("unmodified test map: "
         << state.cells.size() << " cells, " << sent.bytes << " bytes in " << sent.parts
         << " parts, capture "
         << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << " ms");
    CHECK(sent.parts >= 2);
    CHECK(sent.ordered);
    CHECK(sent.cells == state.cells.size());
}

TEST_CASE(
    "world route is the native click preview and confirming starts auto-move",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto target = setup.start + tripoint(0, 4, 0);

    auto act = ACTION_NULL;
    CHECK_FALSE(g->try_get_left_click_action(act, target));
    const auto preview = g->get_destination_preview();
    REQUIRE(preview.size() == 4);
    const auto planned = ec::world::capture_world();
    REQUIRE(planned.route.size() == preview.size());
    CHECK(planned.route.back() == at(target));
    for (const auto index : std::views::iota(std::size_t{0}, preview.size())) {
        CHECK(planned.route[index] == at(preview[index]));
    }
    CHECK(planned.avatar->at == at(setup.start));
    CHECK_FALSE(get_avatar().has_destination());

    SECTION("a click elsewhere replaces the plan") {
        CHECK_FALSE(g->try_get_left_click_action(act, setup.start + tripoint(0, -3, 0)));
        const auto replaced = ec::world::capture_world();
        REQUIRE_FALSE(replaced.route.empty());
        CHECK(replaced.route.back() == at(setup.start + tripoint(0, -3, 0)));
    }
    SECTION("the same square again starts native auto-move and clears the plan") {
        CHECK(g->try_get_left_click_action(act, target));
        CHECK(act == get_movement_action_from_delta(tripoint_rel_ms(0, 1, 0), iso_rotate::yes));
        CHECK(get_avatar().has_destination());
        CHECK(ec::world::capture_world().route.empty());
    }
    get_avatar().clear_destination();
}

TEST_CASE(
    "world route is empty for a click on or beyond a closed door, as for the native click",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto door = setup.start + tripoint(2, 5, 0);
    get_map().ter_set(door, ter_id("t_door_c"));
    const auto click_from = [&](const tripoint_bub_ms& from, const tripoint_bub_ms& to) {
        step_to(from);
        auto act = ACTION_NULL;
        const auto second = g->try_get_left_click_action(act, to);
        const auto route = ec::world::capture_world().route;
        CHECK_FALSE(second);
        CHECK(act == ACTION_NULL);
        CHECK(get_avatar().bub_pos() == from);
        CHECK_FALSE(get_avatar().has_destination());
        get_avatar().clear_destination();
        return route;
    };
    // The legacy avatar pathfinder routes through no closed door, so the engine plans nothing
    // and the click is a no-op: adjacent door, distant door, and a square only the door reaches.
    CHECK(click_from(setup.start + tripoint(1, 5, 0), door).empty());
    CHECK(click_from(setup.start, door).empty());
    CHECK(click_from(setup.start, setup.start + tripoint(4, 5, 0)).empty());
}
