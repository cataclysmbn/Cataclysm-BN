#include "avatar.h"
#include "ballistics.h"
#include "cached_options.h"
#include "cata_utility.h"
#include "catch/catch.hpp"
#include "dispersion.h"
#include "engine_client_event.h"
#include "engine_client_presentation.h"
#include "engine_client_world.h"
#include "explosion.h"
#include "explosion_queue.h"
#include "map/map.h"
#include "map_helpers.h"
#include "options.h"
#include "options_helpers.h"
#include "output.h"
#include "projectile.h"
#include "state_helpers.h"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <ranges>
#include <set>
#include <string>

using namespace engine_client;

namespace {
auto pos(const int x, const int y = 0) -> position { return {.dim = "", .x = x, .y = y, .z = 0}; }
} // namespace

namespace {
/// Collects what the native animation entry points report while `action` runs.
template <typename Action> auto facts_of(Action&& action) -> std::vector<presentation_value> {
    presentation::collect(true);
    presentation::take();
    action();
    auto facts = presentation::take();
    presentation::collect(false);
    return facts;
}
auto types_of(const std::vector<presentation_value>& facts) -> std::vector<std::string> {
    return facts | std::views::transform(&presentation_value::type)
         | std::ranges::to<std::vector>();
}
} // namespace

TEST_CASE("a fired projectile publishes the squares it flies through", "[engine_client_event]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    const override_option animations("ANIMATIONS", "true");
    const override_option animate("ANIMATION_PROJECTILES", "true");
    const override_option stepwise("BULLETS_AS_LASERS", "false");
    auto& shooter = get_avatar();
    const auto from = tripoint_bub_ms(2, 2, 0);
    const auto to = tripoint_bub_ms(9, 2, 0);
    shooter.setpos(from);
    auto proj = projectile{};
    proj.speed = 1000;
    proj.range = 20;
    const auto facts = facts_of([&] {
        projectile_attack(proj, from, to, dispersion_sources{}, &shooter, nullptr);
    });
    REQUIRE_FALSE(facts.empty());
    CHECK(std::ranges::all_of(facts, [](const auto& fact) {
        return fact.type == "projectile.moved";
    }));
    CHECK(facts.front().cells.size() == 1);
    CHECK(facts.front().appearance->kind == "projectile");
    CHECK(facts.front().duration_ms
          == static_cast<std::uint64_t>(get_option<int>("ANIMATION_DELAY")));
    const auto xs =
        facts | std::views::transform([](const auto& fact) { return fact.cells.front().x; })
        | std::ranges::to<std::vector>();
    CHECK(std::ranges::is_sorted(xs));
    CHECK(std::ranges::adjacent_find(xs, std::equal_to<>{}) == xs.end());
    CHECK(std::ranges::all_of(facts, [&](const auto& fact) {
        return fact.id == facts.front().id;
    }));
}

TEST_CASE("a shot drawn as a line publishes its whole path at once", "[engine_client_event]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    const override_option animations("ANIMATIONS", "true");
    const override_option animate("ANIMATION_PROJECTILES", "true");
    const override_option lasers("BULLETS_AS_LASERS", "true");
    auto& shooter = get_avatar();
    const auto from = tripoint_bub_ms(2, 2, 0);
    shooter.setpos(from);
    auto proj = projectile{};
    proj.speed = 1000;
    proj.range = 20;
    const auto facts = facts_of([&] {
        projectile_attack(
            proj, from, tripoint_bub_ms(9, 2, 0), dispersion_sources{}, &shooter, nullptr);
    });
    REQUIRE(facts.size() == 1);
    CHECK(facts.front().type == "projectile.moved");
    CHECK(facts.front().cells.size() >= 5);
    CHECK(facts.front().cells.front().x == world::position_at(from).x + 1);
    CHECK(std::ranges::none_of(facts.front().cells, [](const auto& cell) {
        return cell == world::position_at(tripoint_bub_ms::zero());
    }));
}

TEST_CASE("an explosion publishes its start, shaped blast rings and end", "[engine_client_event]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    const auto at = tripoint_bub_ms(10, 10, 0);
    const auto facts = facts_of([&] {
        explosion_handler::explosion(at, {.damage = 50, .radius = 4.0f}, nullptr);
        explosion_handler::get_explosion_queue().execute();
    });
    const auto types = types_of(facts);
    REQUIRE_FALSE(types.empty());
    CHECK(types.front() == "explosion.started");
    CHECK(types.back() == "explosion.ended");
    const auto blasted = std::ranges::count(types, "explosion.blast");
    CHECK(blasted > 1); // one ring per logical time step
    CHECK(std::ranges::all_of(facts, [&](const auto& fact) {
        return fact.id == facts.front().id;
    }));
    CHECK(facts.front().radius == 4);
    CHECK(facts.front().tile == "explosion");
    CHECK(facts.front().at.has_value());
    auto squares = std::set<position>{};
    for (const auto& fact : facts) {
        if (fact.type == "explosion.blast") {
            squares.insert(fact.cells.begin(), fact.cells.end());
        }
    }
    CHECK(squares.size() > 9);
}

TEST_CASE("combat text publishes its colored segments", "[engine_client_event]") {
    clear_all_state();
    const auto facts = facts_of([&] {
        SCT.add(point(5, 6), direction::NORTH, "-12", m_bad, " crit", m_good);
    });
    REQUIRE(facts.size() == 1);
    CHECK(facts.front().type == "combat_text.shown");
    REQUIRE(facts.front().segments.size() == 2);
    CHECK(facts.front().segments[0].text == "-12");
    CHECK(facts.front().segments[0].color != facts.front().segments[1].color);
}

TEST_CASE(
    "presentation facts are transient, ordered and serialized with their duration",
    "[engine_client_event]") {
    auto initial = state_value{.interaction = {.id = "boundary:1", .ready = {.phase = "menu"}}};
    initial.world.coverage = bounds{.min = pos(0, -5), .max = pos(10, 5)};
    auto created = event_stream::create("epoch:a", initial);
    REQUIRE(created);
    auto stream = std::move(*created);
    auto receiver = stream.current();
    const auto shot = stream.publish_presentation(
        {.fact =
             {.type = "projectile.moved",
              .id = "projectile:1",
              .cells = {pos(1), pos(2)},
              .appearance = look{.kind = "projectile", .glyph = "*", .color = "red"},
              .duration_ms = 20}});
    const auto bang = stream.publish_presentation(
        {.fact =
             {.type = "explosion.started",
              .id = "explosion:1",
              .at = pos(2),
              .radius = 3,
              .color = "red",
              .duration_ms = 40}});
    REQUIRE(shot);
    REQUIRE(bang);
    CHECK(bang->value().sequence == shot->value().sequence + 1);
    CHECK(bang->value().revision == stream.current().at.revision);
    CHECK(apply_batch(receiver, {.epoch = "epoch:a", .events = {*shot, *bang}}));
    CHECK(receiver.at.revision == 0);
    const auto wire = nlohmann::json::parse(
        serialize_events({.epoch = "epoch:a", .events = {*shot}}));
    CHECK(wire["events"][0]["display"]["duration_ms"] == 20);
    CHECK(wire["events"][0]["data"]["path"].size() == 2);
}

TEST_CASE("the native animation options silence the facts outside tests", "[engine_client_event]") {
    clear_all_state();
    const auto at = tripoint_bub_ms(10, 10, 0);
    const auto text = [] { SCT.add(point(5, 6), direction::NORTH, "-1", m_bad); };
    const auto bang = [&] { explosion_handler::draw_explosion(at, 2, c_red, "explosion"); };
    {
        const auto live = restore_on_out_of_scope(test_mode);
        test_mode = false;
        const override_option animations("ANIMATIONS", "false");
        CHECK(facts_of(text).empty());
        CHECK(facts_of(bang).empty());
    }
    {
        const auto live = restore_on_out_of_scope(test_mode);
        test_mode = false;
        const override_option animations("ANIMATIONS", "true");
        const override_option sct("ANIMATION_SCT", "false");
        CHECK(facts_of(text).empty());
        CHECK(facts_of(bang).size() == 2);
    }
    SCT.vSCT.clear();
    // Tests force the animations on, so every other case sees its facts.
    CHECK(facts_of(text).size() == 1);
}
