#include "avatar.h"
#include "catch/catch.hpp"
#include "client_interaction.h"
#include "engine_client_component.h"
#include "game.h"
#include "input.h"
#include "item.h"
#include "item_factory.h"
#include "map/map.h"
#include "map_helpers.h"
#include "monster.h"
#include "options.h"
#include "output.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "target_screen.h"
#include "tile_parts.h"

#if defined(CATA_MCP)
#    include "locale_fixture.h"
#    include "locale_scope.h"
#endif

#include <algorithm>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

auto kinds_of(const std::vector<ec::tile_part>& parts) -> std::vector<std::string> {
    auto result = std::vector<std::string>{};
    std::ranges::transform(parts, std::back_inserter(result), &ec::tile_part::kind);
    return result;
}
auto part_of(const std::vector<ec::tile_part>& parts, const std::string& kind) -> ec::tile_part {
    const auto found = std::ranges::find(parts, kind, &ec::tile_part::kind);
    REQUIRE(found != parts.end());
    return *found;
}

auto ready() -> ec::readiness {
    return {.phase = "waiting_for_input",
            .game_ready = true,
            .accepts_interaction_commands = true,
            .accepts_registered_actions = true};
}
auto boundary() -> ec::boundary_state {
    return {
        .id = "boundary:test",
        .epoch = "epoch:test",
        .ready = ready(),
        .interaction = std::nullopt,
        .frame = {.dim = "", .x = 100, .y = 200}};
}

/// A cursor one square east of its source, inside a map 132 squares wide and `range` squares from
/// the source.
auto options_of(const input_context& ctxt, const int range = 10) -> target_screen::screen_options {
    return {
        .title = "Look Around",
        .target =
            {.source = {.x = 60, .y = 60, .z = 0},
             .cursor = {.x = 61, .y = 60, .z = 0},
             .minimum_position =
                 game_client::interaction_position{.x = 0, .y = 0, .z = -OVERMAP_DEPTH},
             .maximum_position =
                 game_client::interaction_position{.x = 131, .y = 131, .z = OVERMAP_HEIGHT},
             .range = range,
             .distance_metric = "square",
             .status = "look",
             .candidates = {},
             .distance = 1},
        .stats = {{.key = "range", .label = "Range", .value = "1", .max = std::to_string(range)}},
        .ctxt = ctxt};
}

auto value_of(const ec::tile_part& part, const std::string& key) -> std::optional<int> {
    const auto found = std::ranges::find(part.values, key, &ec::tile_value::key);
    return found == part.values.end() ? std::nullopt : std::optional{found->value};
}

auto invoked(const std::string& affordance, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args)};
    return event;
}

/// A lit square beside the avatar with a table, two cubes, a stick, blood and a zombie on it.
struct crowded_square {
    tripoint_bub_ms at = {60, 60, 0};
    crowded_square() {
        clear_all_state();
        build_test_map(ter_id("t_floor"));
        g->place_player(at);
        auto& here = get_map();
        here.furn_set(at + tripoint_east, furn_id("f_table"));
        here.add_item_or_charges(at + tripoint_east, item::spawn("test_1kg_cube"));
        here.add_item_or_charges(at + tripoint_east, item::spawn("test_1kg_cube"));
        here.add_item_or_charges(at + tripoint_east, item::spawn("stick"));
        here.add_field(at + tripoint_east, field_type_id("fd_blood"), 1);
        spawn_test_monster("mon_zombie", at + tripoint_east);
        here.invalidate_map_cache(0);
        here.build_map_cache(0, true);
        here.update_visibility_cache(0);
    }
};
} // namespace

TEST_CASE(
    "a look square is told fact by fact, each fact composed from the map",
    "[target_screen][look]") {
    const auto square = crowded_square{};
    const auto parts = ec::tile_parts(square.at + tripoint_east);
    // The facts come in the order the native view prints them.
    const auto kinds = kinds_of(parts);
    const auto first = [&](const std::string& kind) {
        return std::ranges::distance(kinds.begin(), std::ranges::find(kinds, kind));
    };
    CHECK(kinds.front() == "location");
    CHECK(first("location") < first("terrain"));
    CHECK(first("terrain") < first("furniture"));
    CHECK(first("furniture") < first("field"));
    CHECK(first("field") < first("creature"));
    CHECK(first("creature") < first("item"));
    CHECK(part_of(parts, "furniture").text.contains("table"));
    CHECK(part_of(parts, "creature").text.contains("zombie"));
    CHECK(part_of(parts, "creature").id == "mon_zombie");
    // The terrain and what is on it carry their identity and numbers, not only words.
    CHECK(part_of(parts, "furniture").id == "f_table");
    CHECK(value_of(part_of(parts, "move_cost"), "move_cost")
          == get_map().move_cost(square.at + tripoint_east) * 50);
    CHECK(value_of(part_of(parts, "field"), "intensity") == 1);
    CHECK(part_of(parts, "field").id == "fd_blood");
    CHECK(value_of(part_of(parts, "item"), "count") == 2);
    // Equal items are told once, with how many.
    CHECK(
        std::ranges::count_if(
            parts,
            [](const auto& part) { return part.kind == "item" && part.text.contains("cube"); })
        == 1);
    CHECK(std::ranges::any_of(parts, [](const auto& part) {
        return part.kind == "item" && part.text.contains("[2]");
    }));
    // No window or wrap: every fact is one text with no line break of its own.
    CHECK(std::ranges::none_of(parts, [](const auto& part) {
        return part.text.contains('\n') || part.text.empty();
    }));
}

TEST_CASE("a creature is told fact by fact, with its numbers typed", "[target_screen][look]") {
    const auto square = crowded_square{};
    const auto parts = ec::tile_parts(square.at + tripoint_east);
    const auto zombie = g->critter_at<monster>(square.at + tripoint_east);
    REQUIRE(zombie != nullptr);
    const auto kinds = kinds_of(parts);
    for (const auto* kind : {"hp", "creature", "attitude", "senses", "speed"}) {
        CAPTURE(kind);
        CHECK(std::ranges::contains(kinds, kind));
    }
    CHECK(value_of(part_of(parts, "hp"), "hp") == zombie->get_hp());
    CHECK(value_of(part_of(parts, "hp"), "max") == zombie->get_hp_max());
    // Every fact stands alone: the long description of the creature is not one of them.
    CHECK(std::ranges::none_of(parts, [](const auto& part) { return !part.detail.empty(); }));
    // The avatar is a creature too, and tells its own HP and name.
    const auto self = ec::tile_parts(square.at);
    CHECK(part_of(self, "creature").text.contains("You"));
    CHECK(value_of(part_of(self, "hp"), "max") == get_avatar().get_hp_max());
}

TEST_CASE(
    "a square with more items than the wire lists says there are more", "[target_screen][look]") {
    const auto square = crowded_square{};
    auto& here = get_map();
    auto names = std::set<std::string>{};
    for (const auto* type : item_controller->all()) {
        auto spawned = item::spawn(type->get_id());
        if (names.insert(spawned->tname()).second) { here.add_item(square.at, std::move(spawned)); }
        if (names.size() == 14) { break; }
    }
    REQUIRE(names.size() == 14);
    const auto parts = ec::tile_parts(square.at);
    const auto listed = std::ranges::count(kinds_of(parts), "item");
    CHECK(listed == 12);
    CHECK(value_of(part_of(parts, "more_items"), "more") == 2);
}

TEST_CASE("a square the avatar cannot see is only said to be unseen", "[target_screen][look]") {
    const auto square = crowded_square{};
    // Outside the map the game holds, nothing is seen and nothing is heard.
    const auto parts = ec::tile_parts({-50, -50, 0});
    REQUIRE(parts.size() == 1);
    CHECK(parts.front().kind == "visibility");
    CHECK(parts.front().text.contains("Unseen"));
}

TEST_CASE(
    "the target screen carries the cursor, its stats and the one move affordance",
    "[target_screen][engine_client_component]") {
    auto ctxt = input_context{"TARGET"};
    ctxt.register_action("SELECT");
    const auto view = target_screen::screen_of(options_of(ctxt));
    REQUIRE(view.root);
    CHECK(view.root->id == target_screen::panel_id);
    REQUIRE(view.root->children.size() == 1);
    const auto& map = view.root->children.front();
    CHECK(map.id == target_screen::map_id);
    REQUIRE(std::holds_alternative<ec::map_body>(map.body));
    REQUIRE(view.affordances.size() == 1);
    const auto& move = view.affordances.front();
    CHECK(move.id == target_screen::move_id);
    CHECK(move.args.pos.has_value());
    // A square names a place, so the move is checked against the whole boundary.
    CHECK(ec::scope_of(move) == ec::expect_scope::boundary);
    CHECK(map.affordances == std::vector<std::string>{target_screen::move_id});
}

TEST_CASE(
    "the target move is refused outside the cursor's bounds and range",
    "[target_screen][engine_client_component]") {
    auto ctxt = input_context{"TARGET"};
    auto range = 10;
    const auto screen = ec::screen_scope(
        ctxt, {.view = [&](const ec::boundary_state&) {
            return target_screen::screen_of(options_of(ctxt, range));
        }});
    const auto state = boundary();
    // The boundary's frame puts the bubble's origin at (100, 200).
    const auto move = [&](const int x, const int y, const int z = 0, std::string dim = {}) {
        return ec::resolve_invoke(
            state,
            {.affordance_id = target_screen::move_id,
             .args = {
                 .pos = ec::position{.dim = std::move(dim), .x = x + 100, .y = y + 200, .z = z}}});
    };
    // The range is measured from the source, which is at (60, 60).
    CHECK(move(70, 60));
    CHECK(move(71, 60).error() == ec::error::invalid_arguments);
    CHECK(move(60, 49).error() == ec::error::invalid_arguments);
    range = 500;
    CHECK(move(0, 60));
    CHECK(move(131, 60));
    CHECK(move(-1, 60).error() == ec::error::invalid_arguments);
    CHECK(move(132, 60).error() == ec::error::invalid_arguments);
    CHECK(move(60, -1).error() == ec::error::invalid_arguments);
    CHECK(move(60, 132).error() == ec::error::invalid_arguments);
    CHECK(move(60, 60, OVERMAP_HEIGHT));
    CHECK(move(60, 60, OVERMAP_HEIGHT + 1).error() == ec::error::invalid_arguments);
    CHECK(move(60, 60, -OVERMAP_DEPTH - 1).error() == ec::error::invalid_arguments);
    // Another dimension has no square of this bubble.
    CHECK(move(60, 60, 0, "other").error() == ec::error::invalid_arguments);
}

TEST_CASE(
    "the target move is refused without a square", "[target_screen][engine_client_component]") {
    auto ctxt = input_context{"TARGET"};
    const auto screen = ec::screen_scope(
        ctxt, {.view = [&](const ec::boundary_state&) {
            return target_screen::screen_of(options_of(ctxt));
        }});
    const auto state = boundary();
    CHECK(ec::resolve_invoke(
        state,
        {.affordance_id = target_screen::move_id,
         .args = {.pos = ec::position{.dim = "", .x = 162, .y = 260, .z = 0}}}));
    CHECK(ec::resolve_invoke(state, {.affordance_id = target_screen::move_id}).error()
          == ec::error::invalid_arguments);
    CHECK(
        ec::resolve_invoke(
            state,
            {.affordance_id = target_screen::move_id,
             .args = {.row = "target:1", .pos = ec::position{.dim = "", .x = 1, .y = 1, .z = 0}}})
            .error()
        == ec::error::invalid_arguments);
}

TEST_CASE("a cursor move is read from the recorded invoke only", "[target_screen]") {
    const auto where = game_client::interaction_position{.x = 4, .y = 5, .z = -1};
    CHECK(target_screen::moved_to(invoked(target_screen::move_id, {.position = where}))
          == tripoint_bub_ms(4, 5, -1));
    CHECK_FALSE(target_screen::moved_to(invoked("target:other", {.position = where})));
    CHECK_FALSE(target_screen::moved_to(invoked(target_screen::move_id)));
    CHECK_FALSE(target_screen::moved_to(input_event{}));
}

TEST_CASE("a candidate id comes from where it is and what it is", "[target_screen]") {
    const auto at = tripoint_bub_ms{3, 4, 0};
    CHECK(target_screen::candidate_id(at, true) == target_screen::candidate_id(at, true));
    CHECK(target_screen::candidate_id(at, true) != target_screen::candidate_id(at, false));
    CHECK(target_screen::candidate_id(at, true) != target_screen::candidate_id({4, 3, 0}, true));
}

#if defined(CATA_MCP)
namespace {
auto without_words(json value) -> json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}
} // namespace

TEST_CASE(
    "a look square has the same facts in another language and its words are the catalogue's",
    "[target_screen][look][engine_client_component]") {
    const auto fixture = locale_fixture::korean_catalogue{};
    const auto square = crowded_square{};
    const auto shape = [&] {
        auto result = json::array();
        for (const auto& part : ec::tile_parts(square.at + tripoint_east)) {
            result.push_back(json::parse(ec::to_json(part).dump()));
        }
        return result;
    };
    const auto english = shape();
    const auto korean = [&] {
        const auto scope = localization::locale_scope(locale_fixture::korean);
        return shape();
    }();
    CHECK(without_words(english) == without_words(korean));
    CHECK(english.dump().find("-ko") == std::string::npos);
    CHECK(korean.dump().find("Move cost-ko: ") != std::string::npos);
}
#endif
