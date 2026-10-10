#include "action.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catch/catch.hpp"
#include "character_functions.h"
#include "color.h"
#include "engine_client_event.h"
#include "engine_client_world.h"
#include "game.h"
#include "game_session.h"
#include "itype.h"
#include "json.h"
#include "map/map.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "map_memory.h"
#include "map_perception.h"
#include "monster.h"
#include "output.h"
#include "overmap/omdata.h"
#include "overmap/overmap.h"
#include "overmap/overmapbuffer.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "rng.h"
#include "state_helpers.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"
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

/// A cell without the connection shape of its terrain, which also changes when a neighbour becomes
/// known.
auto without_shape(ec::cell value) -> ec::cell {
    const auto strip = [](ec::look& look) {
        look.subtile.reset();
        look.rotation.reset();
    };
    if (value.terrain) { strip(*value.terrain); }
    if (value.memory) { strip(value.memory->terrain); }
    return value;
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
        if (old == before.cells.end() || without_shape(old->second) != without_shape(cell)) {
            changed.insert(p);
        }
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
    "world capture lists what the avatar wields, wears, carries and stands on",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    REQUIRE_FALSE(you.wield(item::spawn("knife_combat")));
    REQUIRE_FALSE(you.wear_item(item::spawn("scarf_fur"), false));
    you.i_add(item::spawn("hammer"));
    you.i_add(item::spawn("hammer"));
    get_map().add_item_or_charges(setup.start, item::spawn("hammer"));
    get_map().add_item_or_charges(setup.start, item::spawn("hammer"));
    get_map().add_item_or_charges(setup.start, item::spawn("screwdriver"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto slots = [](const auto& entries) {
        return entries | std::views::transform([](const auto& entry) {
                   return entry.slot.value_or("") + ":" + entry.name + ":"
                        + std::to_string(entry.count.value_or(0));
               })
             | std::ranges::to<std::vector>();
    };
    const auto carried = slots(state.avatar->inventory);
    CHECK(
        std::ranges::count(carried, "wielded:" + item::spawn("knife_combat")->display_name() + ":1")
        == 1);
    CHECK(std::ranges::count_if(carried, [](const auto& row) { return row.starts_with("worn:"); })
          >= 1);
    CHECK(std::ranges::count(carried, "carried:" + item::spawn("hammer")->display_name() + ":2")
          == 1);
    CHECK_FALSE(std::ranges::any_of(carried, [](const auto& row) {
        return row.find('<') != std::string::npos;
    }));
    const auto ground = slots(state.avatar->ground);
    CHECK(
        ground
        == std::vector<std::string>{
            ":" + item::spawn("hammer")->display_name() + ":2",
            ":" + item::spawn("screwdriver")->display_name() + ":1"});
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

TEST_CASE(
    "world capture keeps seen furniture as remembered overlay along a walk out of sight",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto benches =
        std::views::iota(-5, -1)
        | std::views::transform([&](const int dx) { return setup.start + tripoint(dx, -4, 0); })
        | std::ranges::to<std::vector>();
    for (const auto& bench : benches) { get_map().furn_set(bench, furn_id("f_bench")); }
    map_perception::acquire();
    const auto start_state = ec::world::capture_world();
    for (const auto& bench : benches) {
        const auto* seen = find_cell(start_state, at(bench));
        REQUIRE(seen != nullptr);
        REQUIRE(seen->known == ec::knowledge::visible);
        REQUIRE(seen->furniture);
    }

    // Every square of the walk from the room's middle to behind the partition.
    for (const auto step : std::views::iota(1, 6)) {
        step_to(setup.start + tripoint(step - 1, step, 0));
        const auto state = ec::world::capture_world();
        for (const auto& bench : benches) {
            CAPTURE(step, bench);
            const auto* cell = find_cell(state, at(bench));
            REQUIRE(cell != nullptr);
            if (cell->known == ec::knowledge::visible) {
                CHECK(cell->furniture);
            } else {
                REQUIRE(cell->memory);
                REQUIRE(cell->memory->overlay);
                CHECK(cell->memory->overlay->id == "f_bench");
            }
        }
    }
    const auto end_state = ec::world::capture_world();
    const auto* last = find_cell(end_state, at(benches.front()));
    CHECK(last->known == ec::knowledge::remembered);
}

TEST_CASE(
    "world capture draws a wall run with its connected line glyph, seen or remembered",
    "[engine_client_world]") {
    const auto setup = make_scene();
    // A second layer of wall on the east side hides the partition from the doorway side.
    for (const auto dy : std::views::iota(-6, 5)) {
        get_map().ter_set(setup.start + tripoint(3, dy, 0), ter_id("t_wall"));
    }
    map_perception::acquire();
    const auto wall = setup.start + tripoint(2, -2, 0);
    const auto seen_state = ec::world::capture_world();
    const auto* seen = find_cell(seen_state, at(wall));
    REQUIRE(seen != nullptr);
    REQUIRE(seen->known == ec::knowledge::visible);
    REQUIRE(seen->terrain);
    CHECK(seen->terrain->glyph == LINE_XOXO_S);

    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE_FALSE(get_avatar().sees(wall));
    const auto remembered_state = ec::world::capture_world();
    const auto* remembered = find_cell(remembered_state, at(wall));
    REQUIRE(remembered != nullptr);
    REQUIRE(remembered->known == ec::knowledge::remembered);
    REQUIRE(remembered->memory);
    CHECK(remembered->memory->terrain.glyph == LINE_XOXO_S);

    // A corner joins the walls to its east and south.
    for (const auto& p : {tripoint(-3, 4, 0), tripoint(-2, 4, 0), tripoint(-3, 5, 0)}) {
        get_map().ter_set(setup.start + p, ter_id("t_wall"));
    }
    step_to(setup.start);
    const auto corner_state = ec::world::capture_world();
    const auto* corner = find_cell(corner_state, at(setup.start + tripoint(-3, 4, 0)));
    REQUIRE(corner != nullptr);
    REQUIRE(corner->terrain);
    CHECK(corner->terrain->glyph == LINE_OXXO_S);
}

TEST_CASE(
    "world view is the terrain window and exactly the squares a click selects",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto shown = [&](const int cols, const int rows) {
        g->w_terrain = catacurses::newwin(rows, cols, point_zero);
        g->ter_view_p = setup.start;
        return ec::world::capture_world();
    };

    SECTION("a 40x25 window reaches squares a 36x24 one cannot") {
        const auto state = shown(40, 25);
        REQUIRE(state.view);
        CHECK(state.view->min == at(setup.start + tripoint(-20, -12, 0)));
        CHECK(state.view->max == at(setup.start + tripoint(19, 12, 0)));
        const auto far_east = setup.start + tripoint(19, 0, 0);
        CHECK(g->click_cell_of(far_east));
        CHECK_FALSE(g->click_cell_of(far_east + tripoint_east));
        CHECK_FALSE(g->click_cell_of(setup.start + tripoint(0, 13, 0)));
        CHECK(g->click_cell_of(setup.start + tripoint(0, 12, 0)));

        const auto native = shown(36, 24);
        REQUIRE(native.view);
        CHECK(native.view->max == at(setup.start + tripoint(17, 11, 0)));
        CHECK_FALSE(g->click_cell_of(far_east));
    }
    SECTION("without a terrain window there is no view and no click target") {
        g->w_terrain = {};
        const auto state = ec::world::capture_world();
        CHECK_FALSE(state.view);
        CHECK_FALSE(g->click_cell_of(setup.start));
    }
    g->w_terrain = {};
}

TEST_CASE("world capture carries the native sidebar values", "[engine_client_world]") {
    make_scene();
    auto& you = get_avatar();
    you.set_pain(30);
    you.focus_pool = 77;
    you.set_part_hp_cur(bodypart_id("arm_l"), 0);
    you.add_effect(efftype_id("disabled"), 1_hours, bodypart_str_id("arm_l"), true);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto& sidebar = state.avatar->sidebar;
    const auto pain = you.get_pain_description();
    CHECK(sidebar.pain.text == pain.first);
    CHECK(sidebar.pain.color == get_all_colors().get_name(pain.second));
    CHECK(sidebar.hunger.text == you.get_hunger_description().first);
    CHECK(sidebar.thirst.text == you.get_thirst_description().first);
    CHECK(sidebar.fatigue.text == you.get_fatigue_description().first);
    CHECK(sidebar.focus == 77);
    CHECK(sidebar.stamina == you.get_stamina());
    CHECK(sidebar.stamina_max == you.get_stamina_max());
    CHECK(sidebar.speed == you.get_speed());
    CHECK(sidebar.move_mode == "walk");
    CHECK(sidebar.weapon == character_funcs::fmt_wielded_weapon(you));
    CHECK(sidebar.location.text == ACTIVE_OVERMAP_BUFFER.ter(you.abs_omt_pos())->get_name());
    CHECK(sidebar.limbs.size() == you.get_all_body_parts(true).size());
    const auto arm = std::ranges::find(sidebar.limbs, "arm_l", &ec::sidebar_limb::id);
    REQUIRE(arm != sidebar.limbs.end());
    CHECK(arm->broken);
    CHECK(arm->hp == 0);
    CHECK(arm->color == "c_dark_gray");
    const auto head = std::ranges::find(sidebar.limbs, "head", &ec::sidebar_limb::id);
    REQUIRE(head != sidebar.limbs.end());
    CHECK_FALSE(head->broken);
    CHECK(head->hp == you.get_part_hp_cur(bodypart_id("head")));
    you.set_pain(0);
    const auto calm = ec::world::capture_world();
    CHECK(calm.avatar->sidebar.pain.text != sidebar.pain.text);
}

TEST_CASE(
    "world capture publishes the native tile selection of terrain, seen or remembered",
    "[engine_client_world]") {
    const auto setup = make_scene();
    // A second layer of wall on the east side hides the partition from the doorway side.
    for (const auto dy : std::views::iota(-6, 5)) {
        get_map().ter_set(setup.start + tripoint(3, dy, 0), ter_id("t_wall"));
    }
    map_perception::acquire();
    const auto wall = setup.start + tripoint(2, -2, 0);
    const auto state = ec::world::capture_world();

    // Native get_rotation_and_subtile: a wall with wall to its north and south is an edge,
    // rotation 0.
    const auto* seen = find_cell(state, at(wall));
    REQUIRE(seen != nullptr);
    REQUIRE(seen->terrain);
    CHECK(seen->terrain->subtile == "edge");
    CHECK(seen->terrain->rotation == 0);
    // Floor surrounded by the same floor is the multitile center.
    const auto* floor = find_cell(state, at(setup.start + tripoint_west));
    REQUIRE(floor != nullptr);
    CHECK(floor->terrain->subtile == "center");
    CHECK(floor->terrain->rotation == 0);
    // A lit midday room reports the native lit_level, lit or brighter.
    REQUIRE(floor->light);
    CHECK(*floor->light >= static_cast<int>(lit_level::LIT));
    CHECK_FALSE(floor->terrain->facing);

    // The memorized layer keeps the shape the avatar saw.
    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE_FALSE(get_avatar().sees(wall));
    const auto remembered_state = ec::world::capture_world();
    const auto* remembered = find_cell(remembered_state, at(wall));
    REQUIRE(remembered != nullptr);
    REQUIRE(remembered->known == ec::knowledge::remembered);
    CHECK_FALSE(remembered->light);
    CHECK(remembered->memory->terrain.subtile == "edge");
    CHECK(remembered->memory->terrain.rotation == 0);
}

TEST_CASE(
    "world capture names the looks_like chain of the game data nearest first",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto road = setup.start + tripoint_south;
    get_map().ter_set(road, ter_id("t_pavement_hw_air"));
    get_map().ter_set(setup.start + tripoint_north, ter_id("t_floor"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(road));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->terrain);
    const auto& chain = cell->terrain->looks_like;
    REQUIRE_FALSE(chain.empty());
    CHECK(chain.front() == ter_id("t_pavement_hw_air")->looks_like);
    CHECK(chain.size() <= 10);
    for (const auto index : std::views::iota(std::size_t{1}, chain.size())) {
        CHECK(chain[index] == ter_str_id(chain[index - 1])->looks_like);
    }
    // Floor has no fallback.
    CHECK(find_cell(state, at(setup.start + tripoint_north))->terrain->looks_like.empty()
          == ter_id("t_floor")->looks_like.empty());
}

TEST_CASE(
    "world capture lists the uppermost item last and names a corpse by its monster",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto p = setup.start + tripoint_east;
    auto& here = get_map();
    here.add_item_or_charges(p, item::make_corpse(mtype_id("mon_zombie"), calendar::turn));
    here.add_item_or_charges(p, item::spawn("rock"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(p));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->items.size() == 2);
    CHECK(cell->items.back().id == here.maptile_at(p).get_uppermost_item().typeId().str());
    const auto corpse = std::ranges::find(cell->items, "corpse", &ec::look::id);
    REQUIRE(corpse != cell->items.end());
    CHECK(corpse->tile == "corpse_mon_zombie");
    REQUIRE_FALSE(corpse->looks_like.empty());
    CHECK(corpse->looks_like.front() == "corpse");
    // Two items make a stack, which the displayed (last) item reports.
    CHECK(cell->items.back().stack == 2);
    CHECK_FALSE(cell->items.front().stack);
}

TEST_CASE(
    "world capture names the sprite of creatures, the avatar and the season",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& zombie = spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 1, 0));
    zombie.facing = FD_LEFT;
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.entities.size() == 1);
    const auto& monster = *state.entities.begin()->second.appearance;
    CHECK(monster.facing == "left");
    const auto& zombie_entity = state.entities.begin()->second;
    CHECK(
        zombie_entity.attitude == Creature::attitude_raw_string(zombie.attitude_to(get_avatar())));
    CHECK(zombie_entity.aware
          == (zombie.sees(get_avatar()) && !get_avatar().has_trait(trait_id("INATTENTIVE"))));
    CHECK(monster.id == "mon_zombie");
    CHECK(monster.looks_like.empty() == zombie.type->looks_like.empty());

    REQUIRE(state.avatar);
    REQUIRE(state.avatar->appearance);
    CHECK(state.avatar->appearance->kind == "avatar");
    CHECK(state.avatar->appearance->tile == (get_avatar().male ? "player_male" : "player_female"));
    CHECK(state.avatar->appearance->facing == (get_avatar().facing == FD_LEFT ? "left" : "right"));

    REQUIRE(state.environment);
    auto seasons = std::set<std::string>{};
    for (const auto quarter : std::views::iota(0, 4)) {
        calendar::turn = calendar::turn_zero + calendar::season_length() * quarter;
        seasons.insert(ec::world::capture_world().environment->season);
    }
    CHECK(seasons == std::set<std::string>{"spring", "summer", "autumn", "winter"});
}

TEST_CASE(
    "world capture lists a character's overlays with the tileset ids to try in the native order",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    you.wear_item(item::spawn("scarf_fur"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto overlay = std::ranges::find(state.avatar->overlays, "worn_scarf_fur", &ec::look::id);
    REQUIRE(overlay != state.avatar->overlays.end());
    CHECK(overlay->kind == "overlay");
    const auto gender = you.male ? "overlay_male_" : "overlay_female_";
    CHECK(overlay->tile == std::string(gender) + "worn_scarf_fur");
    REQUIRE_FALSE(overlay->looks_like.empty());
    CHECK(overlay->looks_like.front() == "overlay_worn_scarf_fur");
    // Then the same pair for what the item looks like, until the chain ends.
    const auto& parent = itype_id("scarf_fur")->looks_like;
    if (parent.is_valid() && !parent.is_empty()) {
        CHECK(overlay->looks_like.size() >= 3);
        CHECK(overlay->looks_like[1] == std::string(gender) + "worn_" + parent.str());
    }
    CHECK(overlay->looks_like.size() <= 20);

    // Monsters wear nothing.
    spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 1, 0));
    map_perception::acquire();
    const auto with_zombie = ec::world::capture_world();
    REQUIRE(with_zombie.entities.size() == 1);
    CHECK(with_zombie.entities.begin()->second.overlays.empty());
}

TEST_CASE(
    "world capture gives a vehicle part its sprite id, part state and four-way facing",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto place = [&](const units::angle facing) {
        auto& here = get_map();
        if (auto* old =
                here.veh_at(setup.start + tripoint(-4, 3, 0))
                    ? &here.veh_at(setup.start + tripoint(-4, 3, 0))->vehicle()
                    : nullptr) {
            here.destroy_vehicle(old);
        }
        here.add_vehicle(
            vproto_id("bicycle"), setup.start + tripoint(-4, 3, 0), facing, 0, 0, false);
        map_perception::acquire();
        const auto state = ec::world::capture_world();
        const auto vp = here.veh_at(setup.start + tripoint(-4, 3, 0));
        REQUIRE(vp);
        const auto* cell = find_cell(state, at(setup.start + tripoint(-4, 3, 0)));
        REQUIRE(cell != nullptr);
        REQUIRE(cell->vehicle);
        return *cell->vehicle;
    };
    const auto east = place(0_degrees);
    REQUIRE(east.id);
    CHECK(east.tile == "vp_" + *east.id);
    CHECK(east.rotation == 3);
    CHECK(east.subtile == "center");
    for (const auto& id : east.looks_like) { CHECK(id.starts_with("vp_")); }
    // Facing south turns the part two quarter turns from east-facing 3: native 3 - dir4.
    CHECK(place(90_degrees).rotation == 2);
}
