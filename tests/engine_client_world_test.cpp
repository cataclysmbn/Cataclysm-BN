#include "action.h"
#include "activity_type.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catacharset.h"
#include "catch/catch.hpp"
#include "character_functions.h"
#include "client_interaction.h"
#include "color.h"
#include "cursesdef.h"
#include "engine_client_event.h"
#include "engine_client_world.h"
#include "fstream_utils.h"
#include "game.h"
#include "game_session.h"
#include "inventory_ui.h"
#include "itype.h"
#include "json.h"
#include "map/map.h"
#include "map/submap.h"
#include "map_helpers.h"
#include "map_memory.h"
#include "map_perception.h"
#include "monster.h"
#include "npc.h"
#include "options.h"
#include "options_helpers.h"
#include "output.h"
#include "overmap/omdata.h"
#include "overmap/overmap.h"
#include "overmap/overmapbuffer.h"
#include "panels.h"
#include "path_info.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "preload_config.h"
#include "rng.h"
#include "state_helpers.h"
#include "trap.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"
#include "weather/weather.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <nlohmann/json.hpp>
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

/// Switches the native sidebar to the classic layout through its saved options, and back.
struct classic_sidebar_guard {
    std::string before = panel_manager::get_manager().get_current_layout_id();
    classic_sidebar_guard() { use("classic"); }
    classic_sidebar_guard(const classic_sidebar_guard&) = delete;
    auto operator=(const classic_sidebar_guard&) -> classic_sidebar_guard& = delete;
    ~classic_sidebar_guard() { use(before); }
    static auto use(const std::string& id) -> void {
        REQUIRE(write_to_file(PATH_INFO::panel_options(), [&](std::ostream& out) {
            out << R"([{"current_layout_id":")" << id << R"(","layouts":[]}])";
        }));
        panel_manager::get_manager().init();
        REQUIRE(panel_manager::get_manager().get_current_layout_id() == id);
    }
};

/// The rows of text the native panel `name` of the current layout draws for the avatar.
auto native_panel(const std::string& name) -> std::vector<std::string> {
    using namespace std::views;
    namespace ranges = std::ranges;
    auto& layout = panel_manager::get_manager().get_current_layout();
    const auto panel = ranges::find(layout, name, &window_panel::get_name);
    REQUIRE(panel != layout.end());
    const auto window = catacurses::newwin(panel->get_height(), panel->get_width(), point_zero);
    panel->draw(get_avatar(), window);
    const auto row_text = [](const auto& row) {
        return row | transform(&game_client::interaction_canvas_run::text) | join
             | ranges::to<std::string>();
    };
    return game_client::window_canvas({window}).rows | transform(row_text)
         | ranges::to<std::vector>();
}

auto contains(const std::string& text, const std::string& part) -> bool {
    return text.find(part) != std::string::npos;
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
    "world capture centers the view on the avatar that moved or shifted the map since the last draw",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto restore_terrain = restore_on_out_of_scope<catacurses::window>(g->w_terrain);
    const auto restore_view = restore_on_out_of_scope<tripoint_bub_ms>(g->ter_view_p);
    g->w_terrain = catacurses::newwin(9, 15, point(2, 1));
    g->ter_view_p = setup.start;
    const auto centered = [&]() {
        const auto state = ec::world::capture_world();
        REQUIRE(state.view);
        REQUIRE(state.avatar);
        CHECK(state.view->min.x == state.avatar->at.x - 7);
        CHECK(state.view->min.y == state.avatar->at.y - 4);
        CHECK(state.view->max.x - state.view->min.x == 14);
        CHECK(state.view->max.y - state.view->min.y == 8);
    };
    SECTION("a step without a draw") {
        centered();
        get_avatar().setpos(setup.start + tripoint_east);
        centered();
    }
    SECTION("a shift of the map without a draw") {
        auto& you = get_avatar();
        const auto old_origin = get_map().get_abs_sub();
        you.setpos(you.abs_pos() + tripoint(3 * SEEX, 0, 0));
        REQUIRE(get_map().get_abs_sub() != old_origin);
        centered();
    }
}

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
    "world capture words a row as the native inventory list prints it", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    REQUIRE_FALSE(you.wield(item::spawn("knife_combat")));
    you.i_add(item::spawn("hammer"));
    you.i_add(item::spawn("hammer"));
    you.i_add(item::spawn("screwdriver"));
    you.i_add(item::spawn("nail", calendar::turn, 30));
    get_map().add_item_or_charges(setup.start, item::spawn("hammer"));
    get_map().add_item_or_charges(setup.start, item::spawn("hammer"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto label = [](const auto& entries, const std::string& id) {
        const auto row = std::ranges::find(entries, id, [](const auto& e) {
            return e.appearance.id;
        });
        REQUIRE(row != entries.end());
        return row->label;
    };
    // The oracle is the first cell the native inventory list prints for the same items.
    const auto native = [](const std::vector<item*>& items) {
        return inventory_selector_preset{}.get_cell_text(inventory_entry{items}, 0);
    };
    const auto carried = [&](const std::string& id) {
        return you.items_with([&](const item& it) { return it.typeId() == itype_id(id); });
    };
    auto ground_hammers = std::vector<item*>{};
    for (auto* const it : get_map().i_at(setup.start)) {
        if (it->typeId() == itype_id("hammer")) { ground_hammers.push_back(it); }
    }
    REQUIRE(carried("hammer").size() == 2);
    REQUIRE(ground_hammers.size() == 2);
    CHECK(label(state.avatar->inventory, "knife_combat")
          == native(carried("knife_combat")) + " (wielded)");
    CHECK(label(state.avatar->inventory, "hammer") == native(carried("hammer")));
    CHECK(label(state.avatar->inventory, "hammer").starts_with("2 "));
    CHECK(label(state.avatar->inventory, "screwdriver") == native(carried("screwdriver")));
    CHECK(label(state.avatar->inventory, "nail") == native(carried("nail")));
    CHECK(label(state.avatar->ground, "hammer") == native(ground_hammers));
    CHECK(label(state.avatar->ground, "hammer").starts_with("2 "));
}

TEST_CASE(
    "world capture labels the sidebar values with the text the native classic panels print",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    REQUIRE_FALSE(you.wield(item::spawn("knife_combat")));
    g->safe_mode = SAFE_MODE_ON;
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto& side = state.avatar->sidebar;
    const auto guard = classic_sidebar_guard{};

    const auto health = native_panel("Health");
    REQUIRE(health.size() >= 7);
    const auto stat = [&](const std::string& id) {
        const auto found = std::ranges::find(state.avatar->stats, id, &ec::avatar_stat::id);
        REQUIRE(found != state.avatar->stats.end());
        REQUIRE_FALSE(found->short_label.empty());
        return found->short_label + " " + found->value;
    };
    CHECK(contains(health[0], stat("strength")));
    CHECK(contains(health[1], stat("dexterity")));
    CHECK(contains(health[2], stat("intelligence")));
    CHECK(contains(health[3], stat("perception")));
    CHECK(contains(health[4], side.focus_label));
    CHECK(contains(health[4], std::to_string(side.focus)));
    REQUIRE(side.safe_mode);
    CHECK(contains(health[4], side.safe_label));
    CHECK(contains(health[5], side.speed_label + " " + std::to_string(side.speed)));
    CHECK(contains(health[5], std::to_string(side.move_counter) + " " + side.move_text));
    CHECK(contains(health[5], side.stamina_label));
    CHECK(contains(health[6], side.temperature.text));
    CHECK(contains(health[6], side.power.label));
    CHECK(contains(health[6], side.power.text));
    // Each limb label sits on its own row; the client draws the health beside it from hp and
    // hp_max.
    for (const auto& [row, limb] : std::views::zip(health, side.limbs)) {
        CHECK(contains(row, limb.label.text));
    }

    const auto location = native_panel("Location");
    REQUIRE_FALSE(location.empty());
    CHECK(location[0].starts_with(side.location.label));
    CHECK(contains(location[0], side.location.text));
    const auto weather = native_panel("Weather");
    REQUIRE_FALSE(weather.empty());
    CHECK(weather[0].starts_with(side.weather.label));
    CHECK(contains(weather[0], side.weather.text));
    const auto weapon = native_panel("Weapon");
    REQUIRE_FALSE(weapon.empty());
    CHECK(weapon[0].starts_with(side.weapon.label));
    CHECK(contains(weapon[0], side.weapon.text));
    const auto time = native_panel("Time");
    REQUIRE_FALSE(time.empty());
    CHECK(contains(time[0], side.season));
    CHECK(contains(time[0], side.day_label + " " + std::to_string(side.day)));
    static_cast<void>(setup);
}

TEST_CASE(
    "world capture leaves the weather unlabelled underground and names the unknown time",
    "[engine_client_world]") {
    const auto setup = make_scene();
    step_to(setup.start + tripoint_below);
    REQUIRE(g->get_levz() < 0);
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto& side = state.avatar->sidebar;
    const auto guard = classic_sidebar_guard{};
    const auto weather = native_panel("Weather");
    REQUIRE_FALSE(weather.empty());
    CHECK(side.weather.label.empty());
    CHECK(weather[0].starts_with(side.weather.text));
    REQUIRE_FALSE(side.has_watch);
    CHECK(contains(native_panel("Time")[0], side.clock));
}

TEST_CASE("world capture gives the color of an item's condition marker", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    auto* shirt = item::spawn_temporary("longshirt");
    shirt->set_damage(shirt->max_damage() / 2);
    const auto marked = [&](const auto& entry) { return entry.appearance.id == "longshirt"; };
    const auto capture = [&] {
        you.i_add(item::spawn(*shirt));
        map_perception::acquire();
        return ec::world::capture_world();
    };
    SECTION("with the health bar the marker and its native color are published") {
        override_option bar("ITEM_HEALTH_BAR", "true");
        const auto state = capture();
        const auto row = std::ranges::find_if(state.avatar->inventory, marked);
        REQUIRE(row != state.avatar->inventory.end());
        REQUIRE(row->durability);
        CHECK(row->durability->symbol == shirt->damage_symbol());
        CHECK(row->durability->color == "c_magenta");
        CHECK(row->name.starts_with(row->durability->symbol + "\u00A0"));
    }
    SECTION("without the health bar the name has no marker to color") {
        override_option bar("ITEM_HEALTH_BAR", "false");
        const auto state = capture();
        const auto row = std::ranges::find_if(state.avatar->inventory, marked);
        REQUIRE(row != state.avatar->inventory.end());
        CHECK_FALSE(row->durability);
    }
    static_cast<void>(setup);
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
    CHECK(sidebar.weapon.text == character_funcs::fmt_wielded_weapon(you));
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
    "world capture names each look by the display name the native look shows",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto floor = setup.start + tripoint_north;
    get_map().ter_set(floor, ter_id("t_floor"));
    get_map().add_item_or_charges(floor, item::spawn("rock"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(floor));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->terrain);
    CHECK(cell->terrain->name == ter_id("t_floor")->name());
    CHECK(cell->terrain->name == "wooden floor");
    REQUIRE_FALSE(cell->items.empty());
    CHECK(cell->items.back().name == item::spawn("rock")->type_name());
}

TEST_CASE(
    "world capture names furniture, traps, fields, vehicle parts and remembered cells",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto furniture = setup.start + tripoint_west;
    const auto trap = setup.start + tripoint_north;
    const auto field = setup.start + tripoint_south;
    const auto part = setup.start + tripoint(-4, 3, 0);
    here.furn_set(furniture, furn_id("f_table"));
    here.trap_set(trap, trap_id("tr_beartrap"));
    get_avatar().add_known_trap(trap, trap_id("tr_beartrap").obj());
    here.add_field(field, field_type_id("fd_fire"), 1);
    here.add_vehicle(vproto_id("bicycle"), part, 0_degrees, 0, 0, false);
    const auto remembered = map_local_to_abs(here, setup.start + tripoint(1, 0, -3));
    get_avatar().memorize_terrain_tile(remembered, "t_floor", 0, 0);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(furniture));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->furniture);
    CHECK(cell->furniture->name == furn_id("f_table")->name());
    cell = find_cell(state, at(trap));
    REQUIRE(cell != nullptr);
    REQUIRE_FALSE(cell->traps.empty());
    CHECK(cell->traps.front().name == trap_id("tr_beartrap")->name());
    cell = find_cell(state, at(field));
    REQUIRE(cell != nullptr);
    REQUIRE_FALSE(cell->fields.empty());
    CHECK(cell->fields.front().appearance.name == field_type_id("fd_fire")->get_name(0));
    cell = find_cell(state, at(part));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->vehicle);
    REQUIRE(cell->vehicle->name);
    CHECK_FALSE(cell->vehicle->name->empty());
    cell = find_cell(
        state, {.dim = "", .x = remembered.x(), .y = remembered.y(), .z = remembered.z()});
    REQUIRE(cell != nullptr);
    REQUIRE(cell->memory);
    CHECK(cell->memory->terrain.name == ter_id("t_floor")->name());
}

TEST_CASE(
    "world capture names the avatar's activity and how much of it is done",
    "[engine_client_world]") {
    make_scene();
    auto& you = get_avatar();
    CHECK_FALSE(ec::world::capture_world().avatar->activity);

    you.assign_activity(activity_id("ACT_WAIT"), 1000);
    you.activity->moves_left = 250;
    const auto state = ec::world::capture_world();
    const auto& activity = state.avatar->activity;
    REQUIRE(activity);
    CHECK(activity->id == "ACT_WAIT");
    CHECK(activity->name == activity_id("ACT_WAIT")->verb().translated());
    CHECK(activity->done == 750);

    // An activity of no set length counts nothing done.
    you.cancel_activity();
    you.assign_activity(activity_id("ACT_WAIT"));
    const auto endless = ec::world::capture_world();
    REQUIRE(endless.avatar->activity);
    CHECK_FALSE(endless.avatar->activity->done);
    you.cancel_activity();
}

TEST_CASE(
    "world capture publishes the colored light a protocol client's square is tinted with",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    calendar::turn = calendar::turn_zero + 1_days;
    g->reset_light_level();
    const auto fire = setup.start + tripoint_west;
    here.add_field(fire, field_type_id("fd_fire"), 1);
    const auto capture = [&](const std::string& enabled) {
        auto state = [&] {
            const auto option = override_option("COLORED_LIGHTING", enabled);
            get_options().save();
            here.invalidate_lightmap_caches();
            map_perception::acquire();
            return ec::world::capture_world();
        }();
        get_options().save();
        return state;
    };

    // The protocol backend lights the map in color as the tiles view does.
    const auto state = capture("true");
    const auto* cell = find_cell(state, at(fire));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->glow);
    CHECK(cell->glow->color == "#ffd08a");
    CHECK(cell->glow->strength > 0);
    CHECK(cell->glow->strength <= 255);
    cell = find_cell(state, at(setup.start));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->glow);
    CHECK(cell->glow->color == "#ffd08a");

    // The option off, nothing is tinted.
    const auto plain = capture("false");
    cell = find_cell(plain, at(fire));
    REQUIRE(cell != nullptr);
    CHECK_FALSE(cell->glow);
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
    "world capture says how much daylight there is and what falls from the sky",
    "[engine_client_world]") {
    make_scene();
    const auto sky = [] {
        const auto state = ec::world::capture_world();
        REQUIRE(state.environment);
        return state.environment->sky;
    };
    const auto noon = sky();
    CHECK(noon.daylight == 100);
    CHECK(noon.precipitation == "none");
    CHECK_FALSE(noon.rain);
    CHECK(noon.dimming == 0);
    calendar::turn = calendar::turn_zero;
    CHECK(sky().daylight < noon.daylight);
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("rain");
    const auto rain = sky();
    CHECK(rain.precipitation == "medium");
    CHECK(rain.rain);
    CHECK(rain.dimming == 30);
    get_weather().weather_id = weather_type_id("snowing");
    const auto snow = sky();
    CHECK(snow.precipitation == "heavy");
    CHECK_FALSE(snow.rain);
    get_weather().weather_id = weather_type_id("clear");
}

TEST_CASE(
    "world capture says how far the sun and the moon have crossed the sky and the moon's phase",
    "[engine_client_world]") {
    make_scene();
    const auto sky = [] {
        const auto state = ec::world::capture_world();
        REQUIRE(state.environment);
        return state.environment->sky;
    };
    const auto noon = sky();
    REQUIRE(noon.sun);
    CHECK(*noon.sun > 30);
    CHECK(*noon.sun < 70);
    CHECK_FALSE(noon.moon);
    const auto day = calendar::turn;
    calendar::turn = sunrise(day);
    CHECK(sky().sun == 0);
    calendar::turn = sunset(day) - 1_seconds;
    CHECK(sky().sun == 100);
    calendar::turn = sunset(day);
    CHECK_FALSE(sky().sun);
    CHECK(sky().moon == 0);
    calendar::turn = calendar::turn_zero;
    const auto midnight = sky();
    CHECK_FALSE(midnight.sun);
    REQUIRE(midnight.moon);
    CHECK(*midnight.moon > 30);
    CHECK(*midnight.moon < 70);
    CHECK(midnight.moon_phase == "new");
    calendar::turn = calendar::turn_zero + calendar::season_length() / 2;
    CHECK(sky().moon_phase != "new");
}

TEST_CASE(
    "world capture gives a hurt creature's health as the native look-around bar shows it",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& zombie = spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 1, 0));
    map_perception::acquire();
    CHECK_FALSE(ec::world::capture_world().entities.begin()->second.health);

    zombie.set_hp(zombie.get_hp_max() * 2 / 5);
    const auto state = ec::world::capture_world();
    REQUIRE(state.entities.size() == 1);
    const auto& health = state.entities.begin()->second.health;
    REQUIRE(health);
    CHECK(health->fifths == 2);
    CHECK(health->color
          == get_all_colors().get_name(
              get_hp_bar(zombie.get_hp(), zombie.get_hp_max(), true).second));
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
    // The schema allows ten.
    CHECK(overlay->looks_like.size() <= 10);
    // Without tints in the client's tileset no key is published; with them, the keys among them.
    CHECK(overlay->tint_keys.empty());
    ec::world::set_tint_ids({"scarf_fur"});
    const auto keyed = ec::world::capture_world();
    const auto with_keys =
        std::ranges::find(keyed.avatar->overlays, "worn_scarf_fur", &ec::look::id);
    REQUIRE(with_keys != keyed.avatar->overlays.end());
    CHECK(with_keys->tint_keys == std::vector<std::string>{"scarf_fur"});
    ec::world::set_tint_ids({});

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
    // The part the native view displays gives the paint, which is the part's own color.
    auto& here = get_map();
    const auto vp = here.veh_at(setup.start + tripoint(-4, 3, 0));
    REQUIRE(vp);
    vp->vehicle()
        .part(vp.part_displayed()->part_index())
        .set_color(RGBColor{}, RGBColor{0x44, 0x55, 0x66, 255});
    map_perception::acquire();
    const auto painted = ec::world::capture_world();
    const auto* painted_cell = find_cell(painted, at(setup.start + tripoint(-4, 3, 0)));
    REQUIRE(painted_cell != nullptr);
    REQUIRE(painted_cell->vehicle);
    REQUIRE(painted_cell->vehicle->tint);
    CHECK(painted_cell->vehicle->tint->fg == "#445566");
    CHECK(painted_cell->vehicle->tint->bg.empty());
}

TEST_CASE("world capture marks the squares that have a roof over them", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    // A square is under a roof when no open sky reaches it through the 3x3 above it, so the roof
    // covers a block.
    const auto indoors = setup.start + tripoint(-3, 0, 0);
    for (const auto y : std::views::iota(-2, 3)) {
        for (const auto x : std::views::iota(-5, 0)) {
            here.ter_set(setup.start + tripoint(x, y, 1), ter_id("t_floor"));
        }
    }
    // A hole in the floor under the roof is under it too.
    const auto hole = indoors + tripoint_south;
    here.ter_set(hole, ter_id("t_open_air"));
    here.ter_set(hole + tripoint_below, ter_id("t_floor"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(find_cell(state, at(indoors)) != nullptr);
    CHECK(find_cell(state, at(indoors))->roofed);
    REQUIRE(find_cell(state, at(hole)) != nullptr);
    CHECK(find_cell(state, at(hole))->below.has_value());
    CHECK(find_cell(state, at(hole))->roofed);
    REQUIRE(find_cell(state, at(setup.start + tripoint(-3, 5, 0))) != nullptr);
    CHECK_FALSE(find_cell(state, at(setup.start + tripoint(-3, 5, 0)))->roofed);
    CHECK_FALSE(find_cell(state, at(setup.start))->roofed);
}

TEST_CASE("world capture marks a square holding a corpse that can rise", "[engine_client_world]") {
    const auto setup = make_scene();
    const auto grave = setup.start + tripoint_east;
    get_map().add_item_or_charges(grave, item::make_corpse(mtype_id("mon_zombie"), calendar::turn));
    get_map().add_item_or_charges(setup.start + tripoint_west, item::spawn("rock"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(find_cell(state, at(grave)) != nullptr);
    CHECK(find_cell(state, at(grave))->reviving);
    CHECK_FALSE(find_cell(state, at(setup.start + tripoint_west))->reviving);
    CHECK_FALSE(find_cell(state, at(setup.start))->reviving);
}

TEST_CASE(
    "world capture tells how far down open air in sight on the avatar's level is drawn from",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    // A platform one level up over the room, ringed by open air.
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-6, 7)) {
            const auto platform = std::abs(x) <= 1 && std::abs(y) <= 1;
            here.ter_set(setup.start + tripoint(x, y, 1),
                         ter_id(platform ? "t_floor" : "t_open_air"));
        }
    }
    const auto air = setup.start + tripoint(-3, 0, 1);
    step_to(setup.start + tripoint_above);
    REQUIRE(map_perception::visible_at(here, air));
    REQUIRE(map_perception::visible_at(here, air + tripoint_below));
    const auto above = ec::world::capture_world();
    const auto* through = find_cell(above, at(air));
    REQUIRE(through != nullptr);
    CHECK(through->known == ec::knowledge::visible);
    CHECK(through->below == 1);
    CHECK_FALSE(through->terrain);
    // The floor seen through it is published as itself.
    const auto* floor = find_cell(above, at(air + tripoint_below));
    REQUIRE(floor != nullptr);
    REQUIRE(floor->terrain);
    CHECK(floor->terrain->id == "t_floor");
    CHECK_FALSE(floor->below);
    REQUIRE(find_cell(above, at(setup.start + tripoint_above)) != nullptr);
    CHECK_FALSE(find_cell(above, at(setup.start + tripoint_above))->below);
    // A glass roof has no floor either: the remembered floor under it is drawn through it, and it
    // keeps its terrain.
    const auto roof = setup.start + tripoint(3, 0, 1);
    here.ter_set(roof, ter_id("t_glass_roof"));
    get_avatar()
        .memorize_terrain_tile(map_local_to_abs(here, roof + tripoint_below), "t_floor", 0, 0);
    map_perception::acquire();
    const auto glazed = ec::world::capture_world();
    const auto* glass = find_cell(glazed, at(roof));
    REQUIRE(glass != nullptr);
    CHECK(glass->below == 1);
    REQUIRE(glass->terrain);
    CHECK(glass->terrain->id == "t_glass_roof");
    here.ter_set(roof, ter_id("t_open_air"));
    // Smoke over the open air draws over the floor seen through it.
    here.add_field(air, field_type_id("fd_smoke"), 1);
    const auto smoky = ec::world::capture_world();
    const auto* hazed = find_cell(smoky, at(air));
    REQUIRE(hazed != nullptr);
    CHECK(hazed->below == 1);
    CHECK_FALSE(hazed->fields.empty());
    here.remove_field(air, field_type_id("fd_smoke"));
    // Off the avatar's level open air is unknown again.
    step_to(setup.start);
    CHECK(find_cell(ec::world::capture_world(), at(air)) == nullptr);
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-6, 7)) {
            here.ter_set(setup.start + tripoint(x, y, 1), ter_id("t_open_air"));
        }
    }
}

TEST_CASE(
    "world capture gives open air in sight over ground the avatar neither sees nor remembers a depth and no ground",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    // A platform one level up over the room, ringed by open air, with a wall at ground level under
    // its edge.
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-6, 7)) {
            const auto platform = std::abs(x) <= 1 && std::abs(y) <= 1;
            here.ter_set(setup.start + tripoint(x, y, 1),
                         ter_id(platform ? "t_floor" : "t_open_air"));
        }
    }
    here.ter_set(setup.start + tripoint(-2, 0, 0), ter_id("t_wall"));
    const auto air = setup.start + tripoint(-3, 0, 1);
    const auto hidden = air + tripoint_below;
    step_to(setup.start + tripoint_above);
    get_avatar().clear_map_memory();
    REQUIRE(map_perception::visible_at(here, air));
    REQUIRE_FALSE(map_perception::visible_at(here, hidden));
    REQUIRE(get_avatar().get_terrain_tile(map_local_to_abs(here, hidden)).tile.empty());
    const auto state = ec::world::capture_world();
    const auto* through = find_cell(state, at(air));
    REQUIRE(through != nullptr);
    // How far down the first floor is, so that the client can fill the column; nothing of the
    // ground itself.
    CHECK(through->below == 1);
    CHECK(through->known == ec::knowledge::visible);
    CHECK(find_cell(state, at(hidden)) == nullptr);
    // Once the avatar remembers the ground, that is drawn instead.
    get_avatar().memorize_tile(map_local_to_abs(here, hidden), "t_floor", 0, 0);
    const auto remembered = ec::world::capture_world();
    REQUIRE(find_cell(remembered, at(air)) != nullptr);
    CHECK(find_cell(remembered, at(air))->below == 1);
    const auto* ground = find_cell(remembered, at(hidden));
    REQUIRE(ground != nullptr);
    CHECK(ground->known == ec::knowledge::remembered);
}

TEST_CASE(
    "world capture never gives a remembered square a live vehicle part", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    auto& you = get_avatar();
    const auto car_at = setup.start + tripoint(-5, -4, 0);
    REQUIRE(here.add_vehicle(vproto_id("golf_cart"), car_at, 0_degrees, 0, 0, false) != nullptr);
    REQUIRE(here.veh_at(car_at));
    here.board_vehicle(car_at, &you);
    you.setpos(car_at);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    // A remembered square carries its memory only; what is perceived live is a visible square.
    for (const auto& [position, entry] : state.cells) {
        CAPTURE(position.x, position.y, position.z);
        if (entry.known == ec::knowledge::remembered) {
            CHECK(entry.memory);
            CHECK_FALSE(entry.vehicle);
        }
        if (entry.vehicle && !entry.light) { CHECK(entry.known == ec::knowledge::visible); }
    }
}

TEST_CASE(
    "world capture names the types of a mutation overlay and the mutations of a character by type",
    "[engine_client_world]") {
    make_scene();
    auto& you = get_avatar();
    you.toggle_trait(trait_id("hair_black"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    CHECK(state.avatar->traits.at("hair_color") == std::vector<std::string>{"hair_black"});
    const auto hair =
        std::ranges::find(state.avatar->overlays, "mutation_hair_black", &ec::look::id);
    if (hair != state.avatar->overlays.end()) {
        REQUIRE(hair->mutation);
        CHECK(hair->mutation->id == "hair_black");
        CHECK(std::ranges::count(hair->mutation->types, "hair_color") == 1);
    }
    // Worn items are not mutations and carry no types.
    for (const auto& overlay : state.avatar->overlays) {
        if (overlay.id && overlay.id->starts_with("worn_")) { CHECK_FALSE(overlay.mutation); }
    }
}

TEST_CASE(
    "world capture lists the mod tilesets of the game data with the tilesets they are for",
    "[engine_client_world]") {
    make_scene();
    const auto state = ec::world::capture_world();
    REQUIRE(state.environment);
    const auto& files = state.environment->mod_tilesets;
    const auto vehicles = std::ranges::find_if(files, [](const auto& file) {
        return file.path.ends_with("external_tileset/vehicle_parts.json");
    });
    REQUIRE(vehicles != files.end());
    CHECK(std::ranges::count(vehicles->compatibility, "UNDEAD_PEOPLE_BASE") == 1);
    CHECK(vehicles->index == 1);
    // Paths are the engine's own, relative to the game folder it runs from.
    CHECK(vehicles->base.ends_with("json"));
    CHECK_FALSE(vehicles->path.starts_with("/"));
}

TEST_CASE(
    "world capture marks a vehicle cargo part that holds items and no other part",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto car_at = setup.start + tripoint(-5, -4, 0);
    auto* veh = here.add_vehicle(vproto_id("golf_cart"), car_at, 0_degrees, 0, 0, false);
    REQUIRE(veh != nullptr);
    auto loaded = std::optional<tripoint_bub_ms>{};
    auto empty = std::optional<tripoint_bub_ms>{};
    for (const auto& part : veh->get_all_parts()) {
        const auto position = part.pos();
        const auto vp = here.veh_at(position);
        const auto cargo = vp ? vp.part_with_feature("CARGO", true) : std::nullopt;
        if (cargo && !loaded) {
            veh->add_item(cargo->part_index(), item::spawn("rock"));
            loaded = position;
        } else if (!vp.part_with_feature("CARGO", true) && !empty) {
            empty = position;
        }
    }
    REQUIRE(loaded);
    REQUIRE(empty);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* with_items = find_cell(state, at(*loaded));
    const auto* without = find_cell(state, at(*empty));
    REQUIRE(with_items != nullptr);
    REQUIRE(without != nullptr);
    CHECK(with_items->cargo);
    CHECK_FALSE(without->cargo);
}

TEST_CASE(
    "world capture keys terrain, furniture, monsters and parts by the tints the client has",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto ground = here.ter(setup.start);
    const auto flag = *ground->get_flags().begin();
    here.furn_set(setup.start + tripoint_east, furn_id("f_chair"));
    const auto car_at = setup.start + tripoint(-5, -4, 0);
    REQUIRE(here.add_vehicle(vproto_id("golf_cart"), car_at, 0_degrees, 0, 0, false) != nullptr);
    map_perception::acquire();
    CHECK(find_cell(ec::world::capture_world(), at(setup.start))->terrain->tint_keys.empty());
    ec::world::set_tint_ids({flag, ground.id().str(), "f_chair", "seat"});
    const auto state = ec::world::capture_world();
    ec::world::set_tint_ids({});
    // The flags come first, then the id.
    const auto& floor = find_cell(state, at(setup.start))->terrain->tint_keys;
    CHECK(floor == (std::vector<std::string>{flag, ground.id().str()}));
    const auto& chair = find_cell(state, at(setup.start + tripoint_east))->furniture->tint_keys;
    CHECK(std::ranges::count(chair, "f_chair") == 1);
    // A vehicle part is keyed by its flags and id, a part with none the client knows by nothing.
    const auto* part = find_cell(state, at(car_at));
    REQUIRE(part != nullptr);
    REQUIRE(part->vehicle);
    CHECK(part->vehicle->tint_keys.size() <= 1);
}

TEST_CASE(
    "world capture publishes the tint an item's own variables ask for", "[engine_client_world]") {
    const auto setup = make_scene();
    auto thing = item::spawn("jeans");
    thing->set_var("tint_color", RGBColor{255, 48, 48, 255});
    thing->set_var("tint_blend_mode_fg", std::string("multiply"));
    thing->set_var("tint_brightness", 0.5f);
    get_map().add_item_or_charges(setup.start + tripoint_east, std::move(thing));
    get_map().add_item_or_charges(setup.start + tripoint_west, item::spawn("jeans"));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto& dyed = find_cell(state, at(setup.start + tripoint_east))->items.back();
    REQUIRE(dyed.tint_vars);
    CHECK(dyed.tint_vars->fg.color == "#ff3030");
    CHECK(dyed.tint_vars->bg.color == "#ff3030");
    CHECK(dyed.tint_vars->fg.blend_mode == "multiply");
    CHECK(dyed.tint_vars->bg.blend_mode.empty());
    CHECK(dyed.tint_vars->fg.brightness == 0.5f);
    CHECK_FALSE(find_cell(state, at(setup.start + tripoint_west))->items.back().tint_vars);
}

TEST_CASE(
    "world capture publishes the tint variables of terrain and furniture and the tint of a field",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto furn_at = setup.start + tripoint_east;
    here.furn_set(furn_at, furn_id("f_chair"));
    here.ter_vars(setup.start)->set("tint_color_fg", RGBColor{16, 32, 48, 255});
    here.furn_vars(furn_at)->set("tint_brightness", 0.5f);
    here.add_field(setup.start + tripoint_west, field_type_id("fd_acid_water"), 1);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto& floor = *find_cell(state, at(setup.start))->terrain;
    REQUIRE(floor.tint_vars);
    CHECK(floor.tint_vars->fg.color == "#102030");
    CHECK(floor.tint_vars->bg.color.empty());
    const auto& chair = *find_cell(state, at(furn_at))->furniture;
    REQUIRE(chair.tint_vars);
    CHECK(chair.tint_vars->fg.brightness == 0.5f);
    CHECK_FALSE(find_cell(state, at(setup.start + tripoint_west))->terrain->tint_vars);
    const auto& field = find_cell(state, at(setup.start + tripoint_west))->fields.back().appearance;
    REQUIRE(field.tint);
    CHECK(field.tint->bg == field.tint->fg);
    CHECK_FALSE(field.tint->fg.empty());
}

TEST_CASE("world capture tells which fields hide the items under them", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto web_at = setup.start + tripoint_west;
    const auto acid_at = setup.start + tripoint_east;
    here.add_field(web_at, field_type_id("fd_web"), 1);
    here.add_field(acid_at, field_type_id("fd_acid_water"), 1);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    // The native view draws no items under a web, and those under acid water as usual.
    CHECK(find_cell(state, at(web_at))->fields.back().hides_items);
    CHECK_FALSE(find_cell(state, at(acid_at))->fields.back().hides_items);
}

namespace {
/// The partition wall seen, then left out of sight and captured, so a capture cache holds it as
/// memory.
struct remembered_wall {
    tripoint_bub_ms where;
    tripoint_abs_ms abs;
};

auto remember_wall() -> remembered_wall {
    const auto setup = make_scene();
    // A second layer of wall on the east side hides the partition from the doorway side.
    for (const auto dy : std::views::iota(-6, 5)) {
        get_map().ter_set(setup.start + tripoint(3, dy, 0), ter_id("t_wall"));
    }
    map_perception::acquire();
    const auto wall = setup.start + tripoint(2, -2, 0);
    REQUIRE(get_avatar().sees(wall));
    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE_FALSE(get_avatar().sees(wall));
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(wall));
    REQUIRE(cell != nullptr);
    REQUIRE(cell->known == ec::knowledge::remembered);
    REQUIRE(cell->memory);
    return {.where = wall, .abs = map_local_to_abs(get_map(), wall)};
}

auto memory_now(const remembered_wall& wall) -> std::optional<ec::memory_layers> {
    const auto state = ec::world::capture_world();
    const auto* cell = find_cell(state, at(wall.where));
    return cell == nullptr ? std::nullopt : cell->memory;
}
} // namespace

TEST_CASE(
    "a remembered terrain changed in memory shows in the next capture", "[engine_client_world]") {
    const auto wall = remember_wall();
    CHECK(memory_now(wall)->terrain.id == "t_wall");
    get_avatar().memorize_terrain_tile(wall.abs, "t_floor", 0, 0);
    CHECK(memory_now(wall)->terrain.id == "t_floor");
}

TEST_CASE(
    "a memorized overlay appears and is forgotten in the next capture", "[engine_client_world]") {
    const auto wall = remember_wall();
    CHECK_FALSE(memory_now(wall)->overlay);
    get_avatar().memorize_tile(wall.abs, "f_bench", 0, 0);
    REQUIRE(memory_now(wall)->overlay);
    CHECK(memory_now(wall)->overlay->id == "f_bench");
    get_avatar().clear_memorized_overlay(wall.abs);
    CHECK_FALSE(memory_now(wall)->overlay);
}

TEST_CASE(
    "a memorized symbol changes the next capture of a square without a terrain",
    "[engine_client_world]") {
    const auto wall = remember_wall();
    get_avatar().clear_memorized_tile(wall.abs);
    CHECK_FALSE(memory_now(wall));
    get_avatar().memorize_tile(wall.abs, "f_bench", 0, 0);
    get_avatar().memorize_symbol(wall.abs, 'B');
    REQUIRE(memory_now(wall));
    CHECK(memory_now(wall)->terrain.glyph == "B");
    get_avatar().memorize_symbol(wall.abs, 'C');
    CHECK(memory_now(wall)->terrain.glyph == "C");
}

TEST_CASE(
    "forgetting one square or all of memory shows in the next capture", "[engine_client_world]") {
    const auto wall = remember_wall();
    const auto other = wall.where + tripoint_south;
    const auto known = [&] {
        const auto state = ec::world::capture_world();
        return find_cell(state, at(other)) != nullptr;
    };
    REQUIRE(known());
    get_avatar().clear_memorized_tile(wall.abs);
    CHECK_FALSE(memory_now(wall));
    CHECK(known());
    get_avatar().clear_map_memory();
    CHECK_FALSE(known());
}

namespace {

/// Every memorized square of the avatar's levels after a walk through the scene under `accel`.
auto walked_memory(const preload_config::compute_accel accel, const time_duration time_of_day)
    -> std::string {
    const auto previous = preload_config::get_compute_accel();
    preload_config::set_compute_accel(accel);
    const auto restore = on_out_of_scope([&] { preload_config::set_compute_accel(previous); });
    const auto setup = make_scene();
    calendar::turn = calendar::turn_zero + time_of_day;
    g->reset_light_level();
    get_map().invalidate_map_cache(0);
    get_avatar().clear_map_memory();
    map_perception::acquire();
    for (const auto step : std::views::iota(1, 6)) {
        step_to(setup.start + tripoint(step - 1, step, 0));
    }
    auto& you = get_avatar();
    auto& here = get_map();
    auto out = std::ostringstream{};
    for (const auto z : std::views::iota(-OVERMAP_DEPTH, OVERMAP_HEIGHT + 1)) {
        for (const auto y : std::views::iota(0, here.getmapsize() * SEEY)) {
            for (const auto x : std::views::iota(0, here.getmapsize() * SEEX)) {
                const auto abs = map_local_to_abs(here, tripoint_bub_ms(x, y, z));
                const auto terrain = you.get_terrain_tile(abs);
                if (!terrain.tile.empty()) {
                    out << x << ',' << y << ',' << z << ':' << terrain.tile << '/'
                        << you.get_memorized_tile(abs).tile << ';';
                }
            }
        }
    }
    return out.str();
}

} // namespace

TEST_CASE(
    "map memory after a walk is the same under CPU and GPU compute", "[engine_client_world]") {
    const auto time_of_day = GENERATE(12_hours, 0_hours, 6_hours);
    CAPTURE(time_of_day);
    const auto cpu = walked_memory(preload_config::compute_accel::cpu, time_of_day);
    const auto gpu = walked_memory(preload_config::compute_accel::gpu_software, time_of_day);
    REQUIRE_FALSE(cpu.empty());
    CHECK(cpu == gpu);
}

TEST_CASE(
    "world capture publishes the sidebar layout the native panel manager holds",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto guard = classic_sidebar_guard{};
    const auto right = override_option("SIDEBAR_POSITION", "right");
    auto& manager = panel_manager::get_manager();
    const auto layout = ec::world::capture_world().avatar->sidebar.layout;
    CHECK(layout.id == "classic");
    CHECK(layout.side == get_option<std::string>("SIDEBAR_POSITION"));
    CHECK(layout.width == manager.get_current_layout().begin()->get_width());

    // The order is the manager's, and the panels it does not list outside a vehicle are left out.
    const auto ids = [](const auto& panels) {
        return panels | std::views::transform(&ec::sidebar_panel::id)
             | std::ranges::to<std::vector>();
    };
    auto native = std::vector<std::string>{};
    for (const auto& panel : manager.get_current_layout()) {
        if (panel.render()) { native.push_back(panel.get_name()); }
    }
    REQUIRE(native.size() >= 3);
    CHECK(ids(layout.panels) == native);
    CHECK(std::ranges::count(ids(layout.panels), "Vehicle") == 0);
    CHECK(std::ranges::count(ids(layout.panels), "Compass") == 1);

    const auto find = [&](const ec::sidebar_layout& in, const std::string& id) {
        const auto found = std::ranges::find(in.panels, id, &ec::sidebar_panel::id);
        REQUIRE(found != in.panels.end());
        return *found;
    };
    // Only the panel that takes the rest of the column has no height.
    CHECK(find(layout, "Log").height == std::nullopt);
    CHECK(find(layout, "Health").height.value_or(0) > 0);
    // The label is the name the native panel options print.
    for (const auto& panel : layout.panels) { CHECK(panel.label == _(panel.id)); }

    // Switching a panel off in the native panel options shows on the next capture.
    auto& compass =
        *std::ranges::find(manager.get_current_layout(), "Compass", &window_panel::get_name);
    const auto was = compass.toggle;
    CHECK(find(layout, "Compass").enabled == was);
    compass.toggle = !was;
    const auto switched = ec::world::capture_world().avatar->sidebar.layout;
    compass.toggle = was;
    CHECK(find(switched, "Compass").enabled == !was);
    CHECK(ids(switched.panels) == ids(layout.panels));
}

TEST_CASE(
    "world capture publishes the side and width of the native sidebar", "[engine_client_world]") {
    const auto setup = make_scene();
    const auto guard = classic_sidebar_guard{};
    auto& manager = panel_manager::get_manager();
    const auto side = GENERATE("left", "right");
    const auto option = override_option("SIDEBAR_POSITION", side);
    const auto layout = ec::world::capture_world().avatar->sidebar.layout;
    CHECK(layout.side == side);
    CHECK(layout.width == manager.get_current_layout().begin()->get_width());
    CHECK(layout.width
          == (std::string(side) == "left" ? manager.get_width_left() : manager.get_width_right()));
    CHECK(layout.width > 0);
}

TEST_CASE(
    "heading_of names a direction by its horizontal part and level", "[engine_client_world]") {
    using ec::heading;
    const auto id = [](const direction dir) { return ec::world::heading_of(dir).id; };
    CHECK(id(direction::NORTH) == "north");
    CHECK(id(direction::SOUTHEAST) == "southeast");
    CHECK(id(direction::ABOVEWEST) == "west_above");
    CHECK(id(direction::BELOWNORTHEAST) == "northeast_below");
    CHECK(id(direction::CENTER) == "center");
    CHECK(id(direction::ABOVECENTER) == "above");
    CHECK(id(direction::BELOWCENTER) == "below");
    // The name is the native one, so the message log and the wire agree.
    for (const auto dir : {direction::EAST, direction::ABOVESOUTH, direction::BELOWCENTER}) {
        CHECK(ec::world::heading_of(dir).name == direction_name(dir));
    }
    // Every direction has its own id.
    auto seen = std::set<std::string>{};
    for (const auto raw : std::views::iota(0u, static_cast<unsigned>(direction::last))) {
        seen.insert(id(static_cast<direction>(raw)));
    }
    CHECK(seen.size() == 27);
}

TEST_CASE(
    "world capture lists the creatures the native compass counts, by direction",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& you = get_avatar();
    // Two to the north (the farther one first), one to the west, one behind the partition wall.
    auto& far_north = spawn_test_monster("mon_zombie", setup.start + tripoint(0, -4, 0));
    auto& near_north = spawn_test_monster("mon_zombie", setup.start + tripoint(0, -2, 0));
    spawn_test_monster("mon_zombie", setup.start + tripoint(-3, 0, 0));
    const auto& hidden = spawn_test_monster("mon_zombie", setup.start + tripoint(5, 0, 0));
    map_perception::acquire();
    REQUIRE_FALSE(you.sees(hidden));
    g->mon_info_update();
    const auto native = you.get_mon_visible().visible_count_by_dir;
    REQUIRE(std::ranges::fold_left(native, 0, std::plus<>{}) == 3);

    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    const auto& compass = state.avatar->compass;
    // The native counts index the headings clockwise from north, then the avatar's own column.
    const auto order = std::vector<std::string>{
        "north",     "northeast", "east",      "southeast", "south",
        "southwest", "west",      "northwest", "center"};
    for (const auto index : std::views::iota(std::size_t{0}, order.size())) {
        const auto& name = order[index];
        const auto found = std::ranges::find(compass, name, [](const auto& d) {
            return d.toward.id;
        });
        CAPTURE(name);
        CHECK((found == compass.end() ? 0 : static_cast<int>(found->creatures.size()))
              == native[index]);
    }
    CHECK(compass.size() == 2);
    CHECK(compass[0].toward.id == "north");
    CHECK(compass[0].toward.name == direction_name(direction::NORTH));
    CHECK(compass[0].label == _("North:"));
    REQUIRE(compass[0].creatures.size() == 2);
    // Nearest first, with the id, look and attitude the entity of the same creature has.
    CHECK(compass[0].creatures[0].distance == 2);
    CHECK(compass[0].creatures[1].distance == 4);
    for (const auto& creature : compass[0].creatures) {
        REQUIRE(state.entities.contains(creature.id));
        const auto& entity = state.entities.at(creature.id);
        CHECK(creature.appearance == entity.appearance);
        CHECK(creature.name == entity.name);
        CHECK(creature.attitude == entity.attitude);
    }
    CHECK(state.entities.at(compass[0].creatures[0].id).at == at(near_north.bub_pos()));
    CHECK(state.entities.at(compass[0].creatures[1].id).at == at(far_north.bub_pos()));
    CHECK(compass[1].toward.id == "west");
    CHECK(compass[1].label == _("West:"));
    CHECK(compass[1].creatures.front().distance == 3);

    // Nothing the native compass hides: from the east side only the zombie behind the wall is in
    // sight.
    step_to(setup.start + tripoint(4, 5, 0));
    REQUIRE(you.sees(hidden));
    REQUIRE_FALSE(you.sees(far_north));
    g->mon_info_update();
    REQUIRE(
        std::ranges::fold_left(you.get_mon_visible().visible_count_by_dir, 0, std::plus<>{}) == 1);
    const auto moved = ec::world::capture_world();
    REQUIRE(moved.entities.size() == 1);
    REQUIRE(moved.avatar->compass.size() == 1);
    REQUIRE(moved.avatar->compass[0].creatures.size() == 1);
    CHECK(moved.avatar->compass[0].creatures[0].id == moved.entities.begin()->first);
}

TEST_CASE("world capture lists an NPC on the native compass count", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& guy = spawn_npc(setup.start + tripoint(0, 3, 0), "test_talker");
    map_perception::acquire();
    REQUIRE(get_avatar().sees(guy));
    g->mon_info_update();
    REQUIRE(get_avatar().get_mon_visible().visible_count_by_dir[4] == 1);
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar->compass.size() == 1);
    CHECK(state.avatar->compass[0].toward.id == "south");
    REQUIRE(state.avatar->compass[0].creatures.size() == 1);
    const auto& listed = state.avatar->compass[0].creatures[0];
    CHECK(listed.appearance.kind == "npc");
    CHECK(listed.distance == 3);
    CHECK(listed.attitude == Creature::attitude_raw_string(guy.attitude_to(get_avatar())));
    CHECK(listed.name == guy.get_name());
}

TEST_CASE(
    "world capture leaves the compass out when the avatar sees nobody", "[engine_client_world]") {
    const auto setup = make_scene();
    spawn_test_monster("mon_zombie", setup.start + tripoint(5, 0, 0));
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    REQUIRE(state.avatar);
    CHECK(state.entities.empty());
    CHECK(state.avatar->compass.empty());
}

TEST_CASE(
    "world capture marks terrain the data flags as a wall when seen and when remembered",
    "[engine_client_world]") {
    const auto setup = make_scene();
    const auto below = map_local_to_abs(get_map(), setup.start + tripoint(1, 0, -3));
    get_avatar().memorize_terrain_tile(below, "t_wall", 0, 0);
    const auto state = ec::world::capture_world();
    const auto* wall = find_cell(state, at(setup.start + tripoint(2, 0, 0)));
    const auto* floor = find_cell(state, at(setup.start));
    const auto* remembered =
        find_cell(state, {.dim = "", .x = below.x(), .y = below.y(), .z = below.z()});
    REQUIRE(wall != nullptr);
    REQUIRE(floor != nullptr);
    REQUIRE(remembered != nullptr);
    REQUIRE(wall->terrain->id == "t_wall");
    // The wire form: a wall terrain says so, other terrain leaves it out.
    const auto cells = nlohmann::json::parse(
        ec::serialize_cells({.cells = {*wall, *floor, *remembered}}))["cells"];
    REQUIRE(cells.size() == 3);
    CHECK(cells[0]["terrain"].value("wall", false));
    CHECK_FALSE(cells[1]["terrain"].contains("wall"));
    CHECK(cells[2]["memory"]["terrain"].value("wall", false));
}

TEST_CASE(
    "world capture raises closed doors and windows in a wall's line and leaves open doors flat",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto door = setup.start + tripoint(2, -1, 0);
    const auto window = setup.start + tripoint(2, 1, 0);
    const auto open_door = setup.start + tripoint(2, -2, 0);
    here.ter_set(door, ter_id("t_door_c"));
    here.ter_set(window, ter_id("t_window"));
    here.ter_set(open_door, ter_id("t_door_o"));
    here.build_map_cache(0);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto wall_of = [&](const tripoint_bub_ms& p) {
        const auto* cell = find_cell(state, at(p));
        REQUIRE(cell != nullptr);
        REQUIRE(cell->terrain);
        return cell->terrain->wall;
    };
    CHECK(wall_of(door));
    CHECK(wall_of(window));
    CHECK_FALSE(wall_of(open_door));
    CHECK_FALSE(wall_of(setup.start));
}

TEST_CASE(
    "world capture gives furniture and terrain the coverage of their data and leaves none out",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto table = setup.start + tripoint_west;
    const auto shrub = setup.start + tripoint(-2, 0, 0);
    here.furn_set(table, furn_id("f_table"));
    here.ter_set(shrub, ter_id("t_shrub"));
    REQUIRE(furn_id("f_table")->coverage > 0);
    REQUIRE(ter_id("t_shrub")->coverage > 0);
    REQUIRE(ter_id("t_floor")->coverage == 0);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* tabled = find_cell(state, at(table));
    const auto* shrubbed = find_cell(state, at(shrub));
    const auto* floor = find_cell(state, at(setup.start));
    REQUIRE(tabled != nullptr);
    REQUIRE(shrubbed != nullptr);
    REQUIRE(floor != nullptr);
    const auto cells = nlohmann::json::parse(
        ec::serialize_cells({.cells = {*tabled, *shrubbed, *floor}}))["cells"];
    REQUIRE(cells.size() == 3);
    CHECK(cells[0]["furniture"].value("coverage", 0) == furn_id("f_table")->coverage);
    CHECK(cells[1]["terrain"].value("coverage", 0) == ter_id("t_shrub")->coverage);
    // No coverage is the default, left out.
    CHECK_FALSE(cells[2]["terrain"].contains("coverage"));
}

TEST_CASE(
    "world capture says what form a door, a fence, a plant, furniture and a field stand in",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto square = [&](const int x, const int y) { return setup.start + tripoint(x, y, 0); };
    here.ter_set(square(-2, -1), ter_id("t_door_c"));
    here.ter_set(square(-2, 0), ter_id("t_door_o"));
    here.ter_set(square(-2, 1), ter_id("t_door_locked"));
    here.ter_set(square(-1, -2), ter_id("t_fence"));
    here.ter_set(square(0, -2), ter_id("t_shrub"));
    here.furn_set(square(1, -2), furn_id("f_chair"));
    here.furn_set(square(1, -1), furn_id("f_table"));
    here.furn_set(square(1, 1), furn_id("f_bed"));
    here.add_field(square(-1, 2), field_type_id("fd_fire"), 1);
    here.add_field(square(0, 2), field_type_id("fd_smoke"), 1);
    here.build_map_cache(0);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto cell_at = [&](const int x, const int y) {
        const auto* cell = find_cell(state, at(square(x, y)));
        REQUIRE(cell != nullptr);
        return nlohmann::json::parse(ec::serialize_cells({.cells = {*cell}}))["cells"][0];
    };
    const auto terrain_form = [&](const int x, const int y) {
        return cell_at(x, y)["terrain"].value("form", std::string{});
    };
    const auto furniture_form = [&](const int x, const int y) {
        return cell_at(x, y)["furniture"].value("form", std::string{});
    };
    const auto field_form = [&](const int x, const int y) {
        return cell_at(x, y)["fields"].back()["look"].value("form", std::string{});
    };
    CHECK(terrain_form(-2, -1) == "door");
    CHECK(terrain_form(-2, 0) == "door");
    CHECK(terrain_form(-2, 1) == "door");
    CHECK(terrain_form(-1, -2) == "fence");
    CHECK(terrain_form(0, -2) == "plant");
    CHECK(furniture_form(1, -2) == "seat");
    CHECK(furniture_form(1, -1) == "table");
    CHECK(furniture_form(1, 1) == "bed");
    CHECK(field_form(-1, 2) == "fire");
    CHECK(field_form(0, 2) == "gas");
    // Plain floor has no form, left out.
    CHECK_FALSE(cell_at(0, 0)["terrain"].contains("form"));
}

TEST_CASE(
    "world capture marks a window as a window and no other wall as one", "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto window = setup.start + tripoint(2, 1, 0);
    const auto door = setup.start + tripoint(2, -1, 0);
    here.ter_set(window, ter_id("t_window"));
    here.ter_set(door, ter_id("t_door_c"));
    here.build_map_cache(0);
    map_perception::acquire();
    const auto state = ec::world::capture_world();
    const auto* glazed = find_cell(state, at(window));
    const auto* closed = find_cell(state, at(door));
    const auto* wall = find_cell(state, at(setup.start + tripoint(2, 0, 0)));
    REQUIRE(glazed != nullptr);
    REQUIRE(closed != nullptr);
    REQUIRE(wall != nullptr);
    const auto cells = nlohmann::json::parse(
        ec::serialize_cells({.cells = {*glazed, *closed, *wall}}))["cells"];
    REQUIRE(cells.size() == 3);
    // A window still stands in the wall's line, and says it is a window.
    CHECK(cells[0]["terrain"].value("wall", false));
    CHECK(cells[0]["terrain"].value("window", false));
    CHECK_FALSE(cells[1]["terrain"].contains("window"));
    CHECK_FALSE(cells[2]["terrain"].contains("window"));
}

TEST_CASE(
    "world capture draws a vehicle's roof only over the level the view looks at",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto restore_terrain = restore_on_out_of_scope<catacurses::window>(g->w_terrain);
    const auto restore_offset = restore_on_out_of_scope<tripoint_rel_ms>(get_avatar().view_offset);
    g->w_terrain = catacurses::newwin(9, 15, point(2, 1));
    // A platform one level up, ringed by open air, over a roofed car.
    for (const auto y : std::views::iota(-8, 9)) {
        for (const auto x : std::views::iota(-8, 9)) {
            const auto platform = std::abs(x) <= 1 && std::abs(y) <= 1;
            here.ter_set(setup.start + tripoint(x, y, 1),
                         ter_id(platform ? "t_floor" : "t_open_air"));
        }
    }
    // Room for the car west of the scene.
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-14, -6)) {
            here.ter_set(setup.start + tripoint(x, y, 0), ter_id("t_floor"));
        }
    }
    auto* const car = here.add_vehicle(
        vproto_id("car"), setup.start + tripoint(-6, 2, 0), 0_degrees, 0, 0, false);
    REQUIRE(car != nullptr);
    step_to(setup.start + tripoint_above);
    // The avatar looks down a level: the car stands on the level shown, so no part of it is its
    // roof.
    get_avatar().view_offset.z() = -1;
    map_perception::acquire();
    const auto lower = ec::world::capture_world();
    auto shown = 0;
    for (const auto& square : car->get_points()) {
        const auto* cell = find_cell(lower, at(abs_to_map_local(here, square)));
        if (cell == nullptr || !cell->vehicle) { continue; }
        ++shown;
        CAPTURE(square);
        CHECK(cell->vehicle->id != std::optional<std::string>{"roof"});
    }
    CHECK(shown > 0);
    // From the platform the car lies below the level shown: its roof is drawn, except where
    // a creature the avatar sees stands, as the native view draws the part and the creature on it.
    get_avatar().view_offset.z() = 0;
    const auto roofed = [&] {
        map_perception::acquire();
        const auto state = ec::world::capture_world();
        auto squares = std::vector<tripoint_bub_ms>{};
        for (const auto& square : car->get_points()) {
            const auto local = abs_to_map_local(here, square);
            const auto* cell = find_cell(state, at(local));
            if (cell != nullptr && cell->vehicle
                && cell->vehicle->id == std::optional<std::string>{"roof"}) {
                squares.push_back(local);
            }
        }
        return squares;
    };
    const auto from_above = roofed();
    REQUIRE_FALSE(from_above.empty());
    // A creature the avatar does not see, as one inside the car, leaves the roof drawn, so its look
    // does not give it away.
    REQUIRE(from_above.size() > 1);
    const auto& hidden = spawn_test_monster("mon_zombie", from_above.front());
    REQUIRE_FALSE(get_avatar().sees(hidden));
    CHECK(roofed() == from_above);
    auto& seen = spawn_test_monster("mon_zombie", from_above.back());
    seen.hallucination = true;
    REQUIRE(get_avatar().sees(seen));
    const auto stood_on = roofed();
    CHECK_FALSE(std::ranges::contains(stood_on, from_above.back()));
    CHECK(stood_on.size() + 1 == from_above.size());
    here.destroy_vehicle(car);
    for (const auto y : std::views::iota(-8, 9)) {
        for (const auto x : std::views::iota(-8, 9)) {
            here.ter_set(setup.start + tripoint(x, y, 1), ter_id("t_open_air"));
        }
    }
}

TEST_CASE(
    "world capture shows the level the view looks at, with what open air on it is drawn from",
    "[engine_client_world]") {
    const auto setup = make_scene();
    auto& here = get_map();
    const auto restore_terrain = restore_on_out_of_scope<catacurses::window>(g->w_terrain);
    const auto restore_offset = restore_on_out_of_scope<tripoint_rel_ms>(get_avatar().view_offset);
    g->w_terrain = catacurses::newwin(9, 15, point(2, 1));
    // A platform one level up over the room, ringed by open air.
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-6, 7)) {
            const auto platform = std::abs(x) <= 1 && std::abs(y) <= 1;
            here.ter_set(setup.start + tripoint(x, y, 1),
                         ter_id(platform ? "t_floor" : "t_open_air"));
        }
    }
    const auto air = setup.start + tripoint(-3, 0, 1);
    const auto level_of = [](const ec::world_state& state) {
        REQUIRE(state.view);
        CHECK(state.view->min.z == state.view->max.z);
        return state.view->min.z;
    };
    SECTION("from the platform, on the avatar's level and a level down") {
        step_to(setup.start + tripoint_above);
        REQUIRE(map_perception::visible_at(here, air));
        const auto own = ec::world::capture_world();
        CHECK(level_of(own) == air.z());
        const auto* through = find_cell(own, at(air));
        REQUIRE(through != nullptr);
        CHECK(through->below == 1);
        get_avatar().view_offset.z() = -1;
        const auto lower = ec::world::capture_world();
        CHECK(level_of(lower) == setup.start.z());
        // Open air on the avatar's level is no longer looked through.
        CHECK(find_cell(lower, at(air)) == nullptr);
    }
    SECTION("from the room, a level up as the look cursor moved there") {
        step_to(setup.start);
        REQUIRE(map_perception::visible_at(here, air));
        CHECK(find_cell(ec::world::capture_world(), at(air)) == nullptr);
        get_avatar().view_offset.z() = 1;
        const auto upper = ec::world::capture_world();
        CHECK(level_of(upper) == air.z());
        const auto* through = find_cell(upper, at(air));
        REQUIRE(through != nullptr);
        CHECK(through->below == 1);
    }
    for (const auto y : std::views::iota(-6, 7)) {
        for (const auto x : std::views::iota(-6, 7)) {
            here.ter_set(setup.start + tripoint(x, y, 1), ter_id("t_open_air"));
        }
    }
}
