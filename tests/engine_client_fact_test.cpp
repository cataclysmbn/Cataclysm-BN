#include "avatar.h"
#include "ballistics.h"
#include "cached_options.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "dispersion.h"
#include "engine_client_presentation.h"
#include "engine_client_world.h"
#include "game.h"
#include "item.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "monster.h"
#include "options_helpers.h"
#include "projectile.h"
#include "ranged.h"
#include "state_helpers.h"
#include "weather/weather.h"

#include <algorithm>
#include <ranges>
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
    CHECK(index_of(types, "projectile.moved") == types.size());
    const auto hit = index_of(types, "projectile.hit");
    REQUIRE(hit < types.size());
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
