#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "creature.h"
#include "engine_client_presentation.h"
#include "engine_client_world.h"
#include "game.h"
#include "map/map.h"
#include "map_helpers.h"
#include "map_perception.h"
#include "monster.h"
#include "options_helpers.h"
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
