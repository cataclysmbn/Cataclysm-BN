#include "avatar.h"
#include "ballistics.h"
#include "cached_options.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "dispersion.h"
#include "engine_client_presentation.h"
#include "engine_client_world.h"
#include "game.h"
#include "game_constants.h"
#include "item.h"
#include "json.h"
#include "map/field.h"
#include "map/map.h"
#include "map/mapbuffer.h"
#include "map/mapbuffer_registry.h"
#include "map/submap_fields.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "monster.h"
#include "options_helpers.h"
#include "player_activity.h"
#include "projectile.h"
#include "ranged.h"
#include "rng.h"
#include "sounds.h"
#include "state_helpers.h"
#include "timed_event.h"
#include "type_id.h"
#include "units_temperature.h"
#include "weather/weather.h"

#include <algorithm>
#include <iterator>
#include <locale>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace engine_client;

namespace {

constexpr auto you_at = tripoint_bub_ms(62, 62, 0);

/// An empty daylit room with the avatar in its middle; the listener collects until the end of the
/// test.
struct scene {
    scene(): live(test_mode) {
        build_test_map(ter_id("t_floor"));
        calendar::turn = calendar::turn_zero + 12_hours;
        get_weather().weather_id = weather_type_id("clear");
        g->reset_light_level();
        get_avatar().setpos(you_at);
        test_mode = true;
        refresh();
        presentation::collect(true);
        presentation::take();
    }
    ~scene() { presentation::collect(false); }
    scene(const scene&) = delete;
    auto operator=(const scene&) -> scene& = delete;

    /// What the avatar sees after the test changed the map.
    static auto refresh() -> void {
        auto& here = get_map();
        here.invalidate_visibility_caches();
        here.build_map_cache(0);
        map_perception::acquire();
    }

    /// Options survive the reset; the rest of the game does not, so it is cleared before they are
    /// set.
    bool cleared = (clear_all_state(), true);
    restore_on_out_of_scope<bool> live;
    override_option animations{"ANIMATIONS", "true"};
    override_option projectiles{"ANIMATION_PROJECTILES", "true"};
    override_option stepwise{"BULLETS_AS_LASERS", "false"};
    override_option instant{"ANIMATION_DELAY", "0"};
};

auto of_type(const std::vector<presentation_value>& facts, const std::string& type)
    -> std::vector<presentation_value> {
    return facts | std::views::filter([&](const auto& fact) { return fact.type == type; })
         | std::ranges::to<std::vector>();
}

} // namespace

TEST_CASE("damage applied directly to a creature is published", "[engine_client_fact]") {
    const auto room = scene{};
    auto& zombie = spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
    scene::refresh();
    zombie.apply_damage(nullptr, bodypart_id("torso"), 7);
    auto hurt = of_type(presentation::take(), "entity.damaged");
    REQUIRE(hurt.size() == 1);
    CHECK(hurt[0].entity == engine_client::world::entity_id(zombie));
    CHECK(hurt[0].amount == 7);
    CHECK(hurt[0].part.empty());

    // The avatar has body parts, so the part comes along.
    get_avatar().apply_damage(nullptr, bodypart_id("arm_l"), 3);
    hurt = of_type(presentation::take(), "entity.damaged");
    REQUIRE(hurt.size() == 1);
    CHECK(hurt[0].entity == "e:avatar");
    CHECK(hurt[0].part == "arm_l");
    CHECK(hurt[0].amount == 3);
}

TEST_CASE("damage dealt through the creature is published once", "[engine_client_fact]") {
    const auto room = scene{};
    auto& zombie = spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
    scene::refresh();
    auto blow = damage_instance(DT_BASH, 5);
    zombie.deal_damage(nullptr, bodypart_id("torso"), blow);
    CHECK(of_type(presentation::take(), "entity.damaged").size() == 1);
}

TEST_CASE(
    "damage to a creature out of sight or of no size is not published", "[engine_client_fact]") {
    const auto room = scene{};
    auto& here = get_map();
    for (const auto& wall : here.points_in_radius(you_at, 2)) {
        const auto reach =
            std::max(std::abs(wall.x() - you_at.x()), std::abs(wall.y() - you_at.y()));
        if (reach == 2) { here.ter_set(wall, ter_id("t_wall")); }
    }
    auto& hidden = spawn_test_monster("mon_zombie", tripoint_bub_ms(70, 70, 0));
    auto& seen = spawn_test_monster("mon_zombie", you_at + tripoint_east);
    scene::refresh();
    REQUIRE_FALSE(get_avatar().sees(hidden.bub_pos()));
    hidden.apply_damage(nullptr, bodypart_id("torso"), 7);
    seen.apply_damage(nullptr, bodypart_id("torso"), 0);
    CHECK(of_type(presentation::take(), "entity.damaged").empty());
}

namespace {

auto types_of(const std::vector<presentation_value>& facts) -> std::vector<std::string> {
    return facts | std::views::transform(&presentation_value::type)
         | std::ranges::to<std::vector>();
}

auto index_of(
    const std::vector<std::string>& types, const std::string& type, const std::size_t from = 0)
    -> std::size_t {
    return std::ranges::find(types.begin() + from, types.end(), type) - types.begin();
}

auto sting() -> projectile {
    auto proj = projectile{};
    proj.speed = 1000;
    proj.range = 20;
    proj.impact = damage_instance(DT_STAB, 30);
    return proj;
}

/// Fires a projectile from the avatar's square at `target` and returns what was published.
auto shoot(const tripoint_bub_ms& target) -> std::vector<presentation_value> {
    projectile_attack(
        sting(), you_at, target, dispersion_sources{}, &get_avatar(), &null_item_reference());
    return presentation::take();
}

} // namespace

TEST_CASE("a projectile that hits a creature says so before the damage", "[engine_client_fact]") {
    const auto room = scene{};
    const auto at = you_at + tripoint_east * 4;
    auto& zombie = spawn_test_monster("mon_zombie", at);
    scene::refresh();
    const auto facts = shoot(at);
    const auto types = types_of(facts);
    CAPTURE(types);
    const auto hit = index_of(types, "projectile.hit");
    REQUIRE(hit < types.size());
    CHECK(std::ranges::count(types, "projectile.hit") == 1);
    CHECK(facts[hit].at == engine_client::world::position_at(at));
    CHECK(facts[hit].entity == engine_client::world::entity_id(zombie));
    CHECK(types[hit + 1] == "entity.damaged");
    // The flight to the creature came first and carries the same id.
    const auto flight = index_of(types, "projectile.moved");
    CHECK(flight < hit);
    CHECK(facts[flight].id == facts[hit].id);
}

TEST_CASE("a projectile drawn as a line is told before it hits", "[engine_client_fact]") {
    const auto room = scene{};
    const override_option lasers("BULLETS_AS_LASERS", "true");
    const auto at = you_at + tripoint_east * 4;
    spawn_test_monster("mon_zombie", at);
    scene::refresh();
    const auto types = types_of(shoot(at));
    CAPTURE(types);
    REQUIRE(index_of(types, "projectile.hit") < types.size());
    CHECK(index_of(types, "projectile.moved") < index_of(types, "projectile.hit"));
    CHECK(index_of(types, "projectile.hit") + 1 == index_of(types, "entity.damaged"));
}

TEST_CASE(
    "a projectile hit is published whatever the animation options say", "[engine_client_fact]") {
    const auto room = scene{};
    const override_option silent("ANIMATIONS", "false");
    test_mode = false;
    const auto at = you_at + tripoint_east * 4;
    spawn_test_monster("mon_zombie", at);
    scene::refresh();
    const auto facts = shoot(at);
    const auto types = types_of(facts);
    CAPTURE(types);
    const auto hit = index_of(types, "projectile.hit");
    REQUIRE(hit < types.size());
    CHECK(index_of(types, "projectile.moved") < hit);
    CHECK_FALSE(facts[hit].id.empty());
    CHECK(types[hit + 1] == "entity.damaged");
}

TEST_CASE("a projectile that hits a wall names the wall's square", "[engine_client_fact]") {
    const auto room = scene{};
    const auto wall = you_at + tripoint_east * 3;
    get_map().ter_set(wall, ter_id("t_wall"));
    scene::refresh();
    const auto facts = shoot(wall + tripoint_east * 2);
    const auto types = types_of(facts);
    CAPTURE(types);
    const auto hit = index_of(types, "projectile.hit");
    REQUIRE(hit < types.size());
    CHECK(facts[hit].at == engine_client::world::position_at(wall));
    CHECK(facts[hit].entity.empty());
    CHECK(index_of(types, "entity.damaged") == types.size());
}

TEST_CASE("a projectile out of sight hits nothing the client is told of", "[engine_client_fact]") {
    const auto room = scene{};
    auto& here = get_map();
    for (const auto& wall : here.points_in_radius(you_at, 2)) {
        const auto reach =
            std::max(std::abs(wall.x() - you_at.x()), std::abs(wall.y() - you_at.y()));
        if (reach == 2) { here.ter_set(wall, ter_id("t_wall")); }
    }
    const auto from = tripoint_bub_ms(70, 70, 0);
    const auto at = tripoint_bub_ms(75, 70, 0);
    spawn_test_monster("mon_zombie", at);
    scene::refresh();
    REQUIRE_FALSE(get_avatar().sees(at));
    projectile_attack(
        sting(), from, at, dispersion_sources{}, &get_avatar(), &null_item_reference());
    CHECK(of_type(presentation::take(), "projectile.hit").empty());
}

TEST_CASE("a thrown item's hit carries the id of its throw", "[engine_client_fact]") {
    const auto room = scene{};
    const auto wall = you_at + tripoint_east * 4;
    get_map().ter_set(wall, ter_id("t_wall"));
    scene::refresh();
    auto& you = get_avatar();
    you.set_moves(1000);
    ranged::throw_item(you, wall, item::spawn("rock"), std::nullopt);
    const auto facts = presentation::take();
    const auto types = types_of(facts);
    CAPTURE(types);
    const auto thrown = index_of(types, "item.thrown");
    const auto flight = index_of(types, "projectile.moved", thrown);
    const auto hit = index_of(types, "projectile.hit", flight);
    const auto landed = index_of(types, "item.landed", hit);
    REQUIRE(landed < types.size());
    CHECK(facts[hit].id == facts[thrown].id);
    CHECK(facts[flight].id == facts[thrown].id);
    CHECK(facts[landed].id == facts[thrown].id);
}

namespace {

/// The avatar hears what is queued now, the way the turn order lets it.
auto listen() -> std::vector<presentation_value> {
    get_map().batch_flood_fill_sounds();
    sounds::process_sound_markers(&get_avatar());
    return of_type(presentation::take(), "sound.heard");
}

auto make_noise(
    const tripoint_bub_ms& at, const short volume, const std::string& description,
    const sounds::sound_t category = sounds::sound_t::alarm) -> void {
    sounds::reset_sounds();
    sounds::sound(
        {.volume = volume,
         .origin = at,
         .category = category,
         .description = description,
         .movement_noise = true});
}

/// Walls two squares around the avatar, so that what lies outside is out of sight.
auto wall_in() -> void {
    auto& here = get_map();
    for (const auto& wall : here.points_in_radius(you_at, 2)) {
        const auto reach =
            std::max(std::abs(wall.x() - you_at.x()), std::abs(wall.y() - you_at.y()));
        if (reach == 2) { here.ter_set(wall, ter_id("t_wall")); }
    }
    scene::refresh();
}

} // namespace

TEST_CASE(
    "a sound the avatar sees the source of is heard without a marker",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    const auto at = you_at + tripoint_east * 3;
    make_noise(at, 80, "Tick.");
    const auto heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].text == "Tick.");
    CHECK(heard[0].amount > 0);
    CHECK_FALSE(heard[0].at.has_value());
    // The native message names no direction for a source in sight; the distance is still counted.
    CHECK_FALSE(heard[0].from.has_value());
    CHECK(heard[0].distance == 3);
}

TEST_CASE(
    "a sound the message log describes is flagged logged and one it skips is not",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    make_noise(you_at + tripoint_east * 3, 80, "Tick.");
    auto heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].logged);
    // Combat noise at the avatar's own square is never described by the native log.
    make_noise(you_at, 80, "a thud", sounds::sound_t::combat);
    heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK_FALSE(heard[0].logged);
}

TEST_CASE(
    "the avatar's own shout is flagged logged because its yell message already shows it",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    sounds::reset_sounds();
    get_avatar().shout("hey");
    const auto heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].text == "yourself yell \"hey\"");
    CHECK(heard[0].logged);
}

TEST_CASE(
    "a sound behind a wall is placed on a square the avatar cannot see",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    wall_in();
    const auto at = you_at + tripoint_east * 5;
    REQUIRE_FALSE(get_avatar().sees(at));
    make_noise(at, 90, "a muffled crash");
    const auto heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK(heard[0].text == "a muffled crash");
    // The marker is the fuzzed square the native game draws, never a square in view.
    REQUIRE(heard[0].at.has_value());
    const auto marker = *heard[0].at;
    CHECK(std::abs(marker.x - engine_client::world::position_at(at).x) <= 3);
    CHECK(std::abs(marker.y - engine_client::world::position_at(at).y) <= 3);
}

TEST_CASE(
    "a sound out of sight is published with the direction and distance its message names",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    wall_in();
    const auto at = you_at + tripoint_east * 5;
    REQUIRE_FALSE(get_avatar().sees(at));
    make_noise(at, 90, "a muffled crash");
    const auto heard = listen();
    REQUIRE(heard.size() == 1);
    // The oracle is the native message: "From the %1$s you hear %2$s".
    const auto native = direction_from(you_at, at);
    REQUIRE(heard[0].from.has_value());
    CHECK(heard[0].from->id == "east");
    CHECK(heard[0].from->name == direction_name(native));
    CHECK(heard[0].distance == 5);
}

TEST_CASE(
    "a sound at the avatar's own square names no direction and no distance",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    make_noise(you_at, 80, "a thud", sounds::sound_t::combat);
    const auto heard = listen();
    REQUIRE(heard.size() == 1);
    CHECK_FALSE(heard[0].from.has_value());
    CHECK(heard[0].distance == 0);
}

TEST_CASE(
    "a sound too faint, too far or to a deaf avatar is not heard",
    "[engine_client_fact][engine_client_sound]") {
    const auto room = scene{};
    make_noise(you_at + tripoint_east * 40, 30, "a whisper");
    CHECK(listen().empty());

    make_noise(you_at + tripoint_east * 3, 80, "Tick.");
    get_avatar().add_effect(efftype_id("deaf"), 10_minutes);
    REQUIRE(get_avatar().is_deaf());
    CHECK(listen().empty());
}

TEST_CASE(
    "a creature stepping in view is published with where it came from",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    const auto from = you_at + tripoint_east * 3;
    auto& zombie = spawn_test_monster("mon_zombie", from);
    scene::refresh();
    presentation::take();
    zombie.setpos(from + tripoint_east);
    const auto moved = of_type(presentation::take(), "entity.moved");
    REQUIRE(moved.size() == 1);
    CHECK(moved[0].entity == engine_client::world::entity_id(zombie));
    CHECK(moved[0].origin == engine_client::world::position_at(from));
    CHECK(moved[0].at == engine_client::world::position_at(from + tripoint_east));
    CHECK(moved[0].kind == "walk");

    zombie.setpos(from + tripoint_east * 5);
    const auto thrown = of_type(presentation::take(), "entity.moved");
    REQUIRE(thrown.size() == 1);
    CHECK(thrown[0].kind == "propelled");
}

TEST_CASE(
    "a creature coming into view or leaving it by its own step is told once",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    wall_in();
    const auto inside = you_at + tripoint_east;
    const auto outside = you_at + tripoint_east * 5;
    auto& zombie = spawn_test_monster("mon_zombie", outside);
    scene::refresh();
    REQUIRE_FALSE(get_avatar().sees(outside));
    presentation::take();
    // Out of sight both before and after: nothing.
    zombie.setpos(outside + tripoint_north);
    CHECK(presentation::take().empty());
    // Into sight: told where it is now, not where it came from.
    zombie.setpos(inside);
    auto facts = presentation::take();
    REQUIRE(facts.size() == 1);
    CHECK(facts[0].type == "entity.moved");
    CHECK_FALSE(facts[0].origin.has_value());
    CHECK(facts[0].at == engine_client::world::position_at(inside));
    // Out of sight again: gone, and no longer a creature the client follows.
    zombie.setpos(outside);
    facts = presentation::take();
    REQUIRE(facts.size() == 1);
    CHECK(facts[0].type == "entity.gone");
    CHECK(facts[0].kind == "lost_sight");
    CHECK(facts[0].entity == engine_client::world::entity_id(zombie));
    CHECK(presentation::take().empty());
}

TEST_CASE(
    "a creature the avatar loses sight of by turning a corner is gone",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    const auto at = you_at + tripoint_east * 4;
    auto& zombie = spawn_test_monster("mon_zombie", at);
    scene::refresh();
    presentation::take();
    get_map().ter_set(you_at + tripoint_east * 2, ter_id("t_wall"));
    get_map().ter_set(you_at + tripoint_east * 2 + tripoint_north, ter_id("t_wall"));
    get_map().ter_set(you_at + tripoint_east * 2 + tripoint_south, ter_id("t_wall"));
    scene::refresh();
    REQUIRE_FALSE(get_avatar().sees(zombie));
    // The three walls changed too; what matters here is the creature.
    const auto facts = of_type(presentation::take(), "entity.gone");
    REQUIRE(facts.size() == 1);
    CHECK(facts[0].kind == "lost_sight");
    CHECK(facts[0].entity == engine_client::world::entity_id(zombie));
}

TEST_CASE(
    "a creature that dies or is removed is told with its reason",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    auto& dying = spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
    auto& leaving = spawn_test_monster("mon_zombie", you_at + tripoint_east * 4);
    scene::refresh();
    presentation::take();
    const auto dying_id = engine_client::world::entity_id(dying);
    const auto leaving_id = engine_client::world::entity_id(leaving);
    dying.die(nullptr);
    auto facts = presentation::take();
    REQUIRE(facts.size() >= 1);
    CHECK(facts[0].type == "entity.gone");
    CHECK(facts[0].kind == "died");
    CHECK(facts[0].entity == dying_id);
    g->remove_zombie(leaving);
    g->cleanup_dead();
    facts = of_type(presentation::take(), "entity.gone");
    REQUIRE(facts.size() == 1);
    CHECK(facts[0].kind == "removed");
    CHECK(facts[0].entity == leaving_id);
}

TEST_CASE(
    "a fact outlives the creature it was taken from",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    auto& zombie = spawn_test_monster("mon_zombie", you_at + tripoint_east * 3);
    scene::refresh();
    presentation::take();
    const auto id = engine_client::world::entity_id(zombie);
    zombie.apply_damage(nullptr, bodypart_id("torso"), 7);
    zombie.die(nullptr);
    // The monster object is freed before anyone reads what it did.
    g->cleanup_dead();
    REQUIRE(g->critter_at<monster>(you_at + tripoint_east * 3) == nullptr);
    const auto facts = presentation::take();
    const auto hurt = of_type(facts, "entity.damaged");
    const auto gone = of_type(facts, "entity.gone");
    REQUIRE(hurt.size() == 1);
    CHECK(hurt[0].entity == id);
    CHECK(hurt[0].amount == 7);
    REQUIRE(gone.size() == 1);
    CHECK(gone[0].entity == id);
    CHECK(gone[0].kind == "died");
}

TEST_CASE(
    "a busy turn publishes no more creature moves than the bound",
    "[engine_client_fact][engine_client_creature]") {
    const auto room = scene{};
    const auto from = you_at + tripoint_east * 3;
    auto& zombie = spawn_test_monster("mon_zombie", from);
    scene::refresh();
    presentation::take();
    for (auto step = 0; step < 400; ++step) {
        zombie.setpos(step % 2 == 0 ? from + tripoint_east : from);
    }
    CHECK(of_type(presentation::take(), "entity.moved").size()
          == presentation::maximum_ambient_facts);
    // The next take starts afresh.
    zombie.setpos(from + tripoint_east);
    CHECK(of_type(presentation::take(), "entity.moved").size() == 1);
}

namespace {

auto abs_cell(const tripoint_bub_ms& at) -> position {
    return engine_client::world::position_at(at);
}

/// The last intensity told for each square, for a field type.
auto told_intensity(const std::vector<presentation_value>& facts, const std::string& field)
    -> std::map<position, std::uint64_t> {
    auto result = std::map<position, std::uint64_t>{};
    for (const auto& fact : facts) {
        if (fact.type == "field.changed" && fact.field == field) {
            result[*fact.at] = fact.amount.value();
        }
    }
    return result;
}

} // namespace

TEST_CASE(
    "bashing a door in view publishes the terrain it became",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    auto& here = get_map();
    const auto door = you_at + tripoint_east * 2;
    here.ter_set(door, ter_id("t_door_c"));
    scene::refresh();
    presentation::take();
    here.bash(door, 200);
    const auto changed = of_type(presentation::take(), "terrain.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].at == abs_cell(door));
    CHECK(changed[0].previous->id == "t_door_c");
    CHECK(changed[0].appearance->id == here.ter(door).id().str());
}

TEST_CASE(
    "furniture set or cleared in view is published", "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    auto& here = get_map();
    const auto at = you_at + tripoint_east * 2;
    here.furn_set(at, furn_id("f_chair"));
    auto changed = of_type(presentation::take(), "furniture.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].previous->id == "f_null");
    CHECK(changed[0].appearance->id == "f_chair");
    here.furn_set(at, furn_id("f_chair"));
    here.furn_set(at, furn_id("f_null"));
    changed = of_type(presentation::take(), "furniture.changed");
    // Setting what is already there changes nothing.
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].appearance->id == "f_null");
}

TEST_CASE(
    "a change out of sight or to nothing is not published",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    wall_in();
    auto& here = get_map();
    const auto hidden = you_at + tripoint_east * 5;
    REQUIRE_FALSE(get_avatar().sees(hidden));
    presentation::take();
    here.ter_set(hidden, ter_id("t_dirt"));
    here.add_field(hidden, field_type_id("fd_smoke"), 2);
    // The same terrain again is no change.
    here.ter_set(you_at, here.ter(you_at));
    CHECK(presentation::take().empty());
}

TEST_CASE(
    "a field added and removed in view tells its intensity",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    auto& here = get_map();
    const auto at = you_at + tripoint_east * 2;
    const auto smoke = field_type_id("fd_smoke");
    REQUIRE(here.add_field(at, smoke, 3));
    auto changed = of_type(presentation::take(), "field.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].field == "fd_smoke");
    CHECK(changed[0].amount == 3);
    here.remove_field(at, smoke);
    changed = of_type(presentation::take(), "field.changed");
    REQUIRE(changed.size() == 1);
    CHECK(changed[0].amount == 0);
}

TEST_CASE(
    "a field turn is told by what it changed, once per square",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    auto& here = get_map();
    const auto gas = field_type_id("fd_toxic_gas");
    const auto fading = you_at + tripoint_east * 2;
    const auto leaving = you_at + tripoint_east * 3;
    const auto arriving = you_at + tripoint_east * 4;
    const auto unchanged = you_at + tripoint_east * 5;
    for (const auto& at : {fading, leaving, unchanged}) { REQUIRE(here.add_field(at, gas, 3)); }
    presentation::take();
    {
        const auto watch = presentation::field_watch{};
        // In place, as the field turn does: no map call sees it.
        here.get_field(fading, gas)->set_field_intensity(1);
        // Map calls made during the turn are told by the watch, not one by one.
        here.remove_field(leaving, gas);
        REQUIRE(here.add_field(arriving, gas, 2));
    }
    const auto facts = of_type(presentation::take(), "field.changed");
    CHECK(facts.size() == 3);
    const auto told = told_intensity(facts, "fd_toxic_gas");
    CHECK(told.at(abs_cell(fading)) == 1);
    CHECK(told.at(abs_cell(leaving)) == 0);
    CHECK(told.at(abs_cell(arriving)) == 2);
    CHECK_FALSE(told.contains(abs_cell(unchanged)));
}

TEST_CASE(
    "fields that fade during a waited stretch are told",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    const auto keep_bubble =
        override_option("REALITY_BUBBLE_SIZE", std::to_string(g_reality_bubble_size));
    const override_option no_autosave("AUTOSAVE", "false");
    const override_option no_mobile("ACTIVITY_MOBILE_BUBBLE_SIZE", "0");
    const override_option no_idle("ACTIVITY_IDLE_BUBBLE_SIZE", "0");
    const override_option no_underground("UNDERGROUND_BUBBLE_SIZE", "0");
    const override_option no_vehicle("VEHICLE_BUBBLE_SIZE", "0");
    const override_option no_combat("COMBAT_BUBBLE_SIZE", "0");
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });
    g->timed_events = timed_event_manager{};
    get_weather().nextweather = calendar::turn + 10_minutes;
    get_weather().temperature = 18_c;
    get_weather().clear_temp_cache();
    g->m.invalidate_map_cache(g->get_levz());
    g->m.build_map_cache(g->get_levz(), true);
    auto& here = get_map();
    const auto smoke = field_type_id("fd_smoke");
    const auto at = you_at + tripoint_east * 2;
    REQUIRE(here.add_field(at, smoke, 2));
    here.get_field(at, smoke)->set_field_age(10_hours);
    g->new_game = false;
    get_avatar().set_moves(100);
    get_avatar().assign_activity(activity_id("ACT_WAIT"), to_moves<int>(30_seconds), 0);
    REQUIRE(get_avatar().activity);
    presentation::take();
    const auto start = calendar::turn;
    CHECK_FALSE(g->do_turn());
    CHECK(calendar::turn == start + 30_seconds);
    const auto told = told_intensity(presentation::take(), "fd_smoke");
    REQUIRE(told.contains(abs_cell(at)));
    CHECK(told.at(abs_cell(at)) == static_cast<std::uint64_t>(here.get_field_intensity(at, smoke)));
}

TEST_CASE(
    "a turn that changes more squares than the bound publishes the first of them",
    "[engine_client_fact][engine_client_map]") {
    const auto room = scene{};
    auto& here = get_map();
    auto changed = std::size_t{};
    for (const auto& at : here.points_in_radius(you_at, 10)) {
        if (at != you_at) {
            here.ter_set(at, ter_id("t_dirt"));
            ++changed;
        }
    }
    REQUIRE(changed > presentation::maximum_ambient_facts);
    CHECK(of_type(presentation::take(), "terrain.changed").size()
          == presentation::maximum_ambient_facts);
}

namespace {

/// Whether the animation options are on, as the player chooses; a client listens or not on top.
struct scenario_options {
    bool animations = true;
    bool listening = true;
};

struct scenario_run {
    std::vector<presentation_value> facts;
    std::string rng;
    std::string state;
};

auto avatar_save() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    get_avatar().serialize(json);
    return stream.str();
}

/// What gameplay owns around the scenario: the squares, the monsters and the avatar.
auto authority() -> std::string {
    auto stream = std::ostringstream{};
    auto json = JsonOut(stream);
    auto& here = get_map();
    json.start_object();
    json.member("cells");
    json.start_array();
    for (const auto& pos : here.points_in_radius(you_at, 8)) {
        json.start_array();
        json.write(pos);
        json.write(here.ter(pos).id().str());
        json.write(here.furn(pos).id().str());
        for (const auto& [id, entry] : here.field_at(pos)) {
            json.write(id.id().str());
            json.write(entry.get_field_intensity());
        }
        json.end_array();
    }
    json.end_array();
    json.member("monsters");
    json.start_array();
    for (const auto& mon : g->all_monsters()) { mon.serialize(json); }
    json.end_array();
    json.member("avatar");
    get_avatar().serialize(json);
    json.end_object();
    return stream.str();
}

/// A shot, a bashed door, a step, a wound, drifting gas, a noise and a death, in one turn.
auto run_scenario(const scenario_options& options, const std::string& start) -> scenario_run {
    rng_set_engine_seed(424242);
    clear_all_state();
    {
        auto stream = std::istringstream{start};
        auto json = JsonIn{stream};
        get_avatar().deserialize(json);
    }
    build_test_map(ter_id("t_floor"));
    calendar::turn = calendar::turn_zero + 12_hours;
    get_weather().weather_id = weather_type_id("clear");
    g->reset_light_level();
    sounds::reset_sounds();
    get_avatar().setpos(you_at);
    auto& here = get_map();
    const auto door = you_at + tripoint_north * 3;
    here.ter_set(door, ter_id("t_door_c"));
    scene::refresh();
    auto& target = spawn_test_monster("mon_zombie", you_at + tripoint_east * 4);
    auto& walker = spawn_test_monster("mon_zombie", you_at + tripoint_west * 3);
    scene::refresh();
    const auto live = restore_on_out_of_scope(test_mode);
    test_mode = options.animations;
    const override_option animations("ANIMATIONS", options.animations ? "true" : "false");
    const override_option projectiles("ANIMATION_PROJECTILES", "true");
    const override_option stepwise("BULLETS_AS_LASERS", "false");
    const override_option instant("ANIMATION_DELAY", "0");
    presentation::collect(options.listening);
    presentation::take();
    projectile_attack(sting(), you_at, target.bub_pos(), dispersion_sources{}, &get_avatar(),
                      &null_item_reference());
    here.bash(door, 200);
    walker.setpos(you_at + tripoint_west * 2);
    walker.apply_damage(nullptr, bodypart_id("torso"), 5);
    const auto gas = field_type_id("fd_toxic_gas");
    here.add_field(you_at + tripoint_south * 2, gas, 3);
    {
        const auto watch = presentation::field_watch{};
        auto* sm = here.get_submap_at(you_at);
        const auto abs_sm = project_to<coords::sm>(map_local_to_abs(here, you_at));
        process_fields_in_submap(get_avatar().get_dimension(), *sm, abs_sm, MAPBUFFER);
    }
    make_noise(you_at + tripoint_east * 3, 90, "a crash");
    here.batch_flood_fill_sounds();
    sounds::process_sound_markers(&get_avatar());
    walker.die(&get_avatar());
    auto result = scenario_run{.facts = presentation::take()};
    presentation::collect(false);
    auto engine = std::ostringstream{};
    engine.imbue(std::locale::classic());
    engine << rng_get_engine();
    result.rng = engine.str();
    result.state = authority();
    return result;
}

} // namespace

TEST_CASE(
    "listening leaves the facts' gameplay and RNG untouched",
    "[engine_client_fact][engine_client_equality]") {
    clear_all_state();
    const auto start = avatar_save();
    for (const auto animations : {true, false}) {
        CAPTURE(animations);
        const auto bare = run_scenario({.animations = animations, .listening = false}, start);
        const auto again = run_scenario({.animations = animations, .listening = false}, start);
        CHECK(again.state == bare.state);
        CHECK(again.rng == bare.rng);
        const auto heard = run_scenario({.animations = animations, .listening = true}, start);
        CHECK(bare.facts.empty());
        CHECK(heard.rng == bare.rng);
        CHECK(heard.state == bare.state);
    }
}

TEST_CASE(
    "every family of fact fires in a turn whatever the animation options say",
    "[engine_client_fact][engine_client_equality]") {
    clear_all_state();
    const auto start = avatar_save();
    const auto animated = run_scenario({.animations = true}, start);
    const auto silenced = run_scenario({.animations = false}, start);
    const auto told = types_of(animated.facts);
    CAPTURE(told);
    for (const auto* type :
         {"projectile.moved", "projectile.hit", "entity.damaged", "terrain.changed", "entity.moved",
          "entity.gone", "field.changed", "sound.heard"}) {
        CHECK(index_of(told, type) < told.size());
    }
    // The scenario animates with test_mode on, which silences the game's own sound calls, so sound
    // cues differ.
    const auto unsounded = [](const std::vector<std::string>& types) {
        return types | std::views::filter([](const auto& type) {
                   return !type.starts_with("sound.") || type == "sound.heard";
               })
             | std::ranges::to<std::vector>();
    };
    CHECK(unsounded(types_of(silenced.facts)) == unsounded(told));
}
