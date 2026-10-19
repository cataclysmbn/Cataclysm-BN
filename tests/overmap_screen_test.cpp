#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "coordinates.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "output.h"
#    include "overmap/overmap_ui.h"
#    include "overmap/overmapbuffer.h"
#    include "overmap_helpers.h"
#    include "player_helpers.h"
#    include "rng.h"
#    include "state_helpers.h"
#    include "type_id.h"
#    include "uistate.h"

#    include <algorithm>
#    include <cstdlib>
#    include <format>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;
using game_client::interaction_position;

/// The squares the screen publishes around its cursor, which the tests below name.
constexpr auto published_cols = 61;
constexpr auto published_rows = 37;
constexpr auto map_id = "map:overmap";

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        test_mode = false;
        TERMX = 100;
        TERMY = 40;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:overmap",
         .ready =
             {.phase = "waiting_for_input",
              .game_ready = true,
              .accepts_interaction_commands = true,
              .accepts_registered_actions = true}});
    REQUIRE(state.has_value());
    return *state;
}

/// The key a client sends for a registered action.
auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The invoke a client sends for an affordance the screen's loop runs.
auto invoke(const std::string& affordance, game_client::interaction_arguments args = {})
    -> input_event {
    auto event = input_event{};
    event.type = input_event_t::interaction;
    event.interaction = game_client::interaction_event{
        .operation = game_client::interaction_operation::invoke,
        .target_id = affordance,
        .arguments = std::move(args)};
    return event;
}

auto move_to(const tripoint_abs_omt& at) -> input_event {
    return invoke("overmap:cursor",
                  {.position = interaction_position{.x = at.x(), .y = at.y(), .z = at.z()}});
}

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto affordance(const ec::screen_view& screen, const std::string& id) -> const ec::affordance& {
    const auto found = std::ranges::find(screen.affordances, id, &ec::affordance::id);
    INFO(id);
    REQUIRE(found != screen.affordances.end());
    return *found;
}
auto toggle(const ec::screen_view& screen, const std::string& key) -> const ec::panel_stat& {
    const auto& stats = body<ec::panel_body>(part(screen, "overmap:toggles")).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    INFO(key);
    REQUIRE(found != stats.end());
    return *found;
}
auto squares(const ec::screen_view& screen) -> const game_client::interaction_overmap& {
    const auto& map = body<ec::map_body>(part(screen, "map:overmap"));
    REQUIRE(map.overmap);
    return *map.overmap;
}
auto cursor_of(const ec::screen_view& screen) -> interaction_position {
    const auto& map = body<ec::map_body>(part(screen, "map:overmap"));
    REQUIRE(map.target);
    return map.target->cursor;
}

/// The top screen as a client reads it, with the ids of this screen instance left out.
auto published() -> json {
    auto top = json::parse(ec::serialize_interaction_boundary(captured()))["stack"].back();
    REQUIRE_FALSE(top["root"].is_null());
    top.erase("screen_id");
    return top;
}
auto without_words(json value) -> json {
    if (value.is_string()) { return ""; }
    if (!value.is_structured()) { return value; }
    for (auto& member : value) { member = without_words(std::move(member)); }
    return value;
}
void collect_ids(const json& value, std::vector<std::string>& into) {
    if (value.is_object()) {
        for (const auto& [name, member] : value.items()) {
            if ((name == "id" || name == "affordance") && member.is_string()) {
                into.push_back(member.get<std::string>());
            }
            collect_ids(member, into);
        }
    } else if (value.is_array()) {
        for (const auto& member : value) { collect_ids(member, into); }
    }
}

/// What the native window shows in cell (x, y): its glyph and the two colour indexes.
struct native_cell {
    std::string glyph;
    int foreground = 0;
    int background = 0;
    auto operator==(const native_cell&) const -> bool = default;
};
auto native_cell_at(const catacurses::window& window, const int x, const int y) -> native_cell {
    const auto& cell = window.get<cata_cursesport::WINDOW>()->line[y].chars[x];
    return {.glyph = cell.ch.empty() ? " " : cell.ch,
            .foreground = static_cast<int>(cell.FG),
            .background = static_cast<int>(cell.BG)};
}
auto trimmed(std::string text) -> std::string {
    while (!text.empty() && text.back() == ' ') { text.pop_back(); }
    return text;
}

/// A world whose overmap the code alone makes, with the toggles at their defaults and a player
/// who knows the squares around.
struct fixed_world {
    tripoint_abs_omt center;
    // The state is cleared before the guard turns off test mode: clearing eats, which asks.
    explicit fixed_world() {
        clear_all_state();
        center = get_avatar().abs_omt_pos();
        overmap_helpers::lay_terrain(center);
        uistate.overmap_show_overlays = true;
        uistate.overmap_blinking = false;
        uistate.overmap_show_land_use_codes = false;
        uistate.overmap_show_forest_trails = true;
        uistate.overmap_show_map_notes = true;
        uistate.overmap_show_hordes = false;
        uistate.overmap_show_city_labels = false;
        get_avatar().omt_path = {};
    }
    ~fixed_world() {
        game_client::set_overmap_tiles(false);
        clear_all_state();
    }
};

} // namespace

TEST_CASE(
    "the overmap screen publishes the squares, cursor, facts and actions of its context",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(3, 0), "C:cache");
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(-4, 2), "plain note");
    you.omt_path = {world.center + point_east, world.center + point_east * 2};
    auto step = 0;

    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.root);
        CHECK(screen.role == "screen");
        CHECK(screen.context == "OVERMAP");
        CHECK(screen.title == "Overmap");
        const auto& om = squares(screen);
        const auto cursor = cursor_of(screen);
        const auto center =
            interaction_position{world.center.x(), world.center.y(), world.center.z()};
        // The squares come with the cursor in the middle, whatever the terminal is.
        CHECK(om.cols == published_cols);
        CHECK(om.rows == published_rows);
        REQUIRE(om.glyphs.size() == static_cast<std::size_t>(published_cols * published_rows));
        CHECK(om.foreground.size() == om.glyphs.size());
        CHECK(om.background.size() == om.glyphs.size());
        CHECK(om.origin.x == cursor.x - published_cols / 2);
        CHECK(om.origin.y == cursor.y - published_rows / 2);
        CHECK(om.origin.z == cursor.z);
        CHECK(om.player == center);

        // Every square is what the native window draws there: the glyph and both colours. The
        // window also draws the cursor and its crosshair over four squares, which a client draws
        // itself.
        const auto* const window = g->w_overmap.get<cata_cursesport::WINDOW>();
        const auto left = om.origin.x - (cursor.x - window->width / 2);
        const auto top = om.origin.y - (cursor.y - window->height / 2);
        auto differing = 0;
        for (const auto y : std::views::iota(0, om.rows)) {
            for (const auto x : std::views::iota(0, om.cols)) {
                const auto at = static_cast<std::size_t>(y * om.cols + x);
                const auto drawn = native_cell_at(g->w_overmap, x + left, y + top);
                const auto from_cursor = std::pair{x - om.cols / 2, y - om.rows / 2};
                if (std::abs(from_cursor.first) <= 1 && std::abs(from_cursor.second) <= 1) {
                    continue;
                }
                // The note of the cursor square is boxed over the corner, and said in words below.
                if (!om.cursor_note.empty() && x < 10 && y < 4) { continue; }
                const auto same =
                    drawn
                    == native_cell{
                        .glyph = om.glyphs[at],
                        .foreground = om.foreground[at],
                        .background = om.background[at]};
                if (!same) { UNSCOPED_INFO(std::format("square ({}, {}) differs", x, y)); }
                differing += same ? 0 : 1;
            }
        }
        CHECK(differing == 0);

        switch (step++) {
            case 0: {
                CHECK(cursor == center);
                CHECK(om.glyphs[static_cast<std::size_t>((om.rows / 2) * om.cols + om.cols / 2)]
                      == "@");
                // The facts the sidebar names, in plain words and without the window's frame or
                // hints.
                const auto sidebar_row = trimmed(std::ranges::fold_left(
                    std::views::iota(3, 28) | std::views::transform([&](const int x) {
                        return native_cell_at(g->w_omlegend, x, 1).glyph;
                    }),
                    std::string{}, std::plus<>{}));
                CHECK_FALSE(om.cursor_description.empty());
                CHECK(om.cursor_description == sidebar_row);
                CHECK(om.cursor_here == "You are here.");
                CHECK(om.cursor_note.empty());
                CHECK(om.route_text == "Route of 2 squares.");
                CHECK(std::ranges::contains(
                    om.legend,
                    std::format("LEVEL {}, {}, {}", world.center.z(), world.center.x(),
                                world.center.y())));
                CHECK(std::ranges::none_of(om.legend, [](const auto& line) {
                    return line.contains("│") || line.contains("Press") || line.contains("pan.");
                }));
                // The notes carry their visible text, not the symbol and colour that prefix it.
                REQUIRE(om.notes.size() == 2);
                CHECK(std::ranges::any_of(om.notes, [&](const auto& note) {
                    return note.text == "cache"
                        && note.position
                               == interaction_position{
                                   world.center.x() + 3, world.center.y(), world.center.z()};
                }));
                CHECK(std::ranges::any_of(om.notes, [](const auto& note) {
                    return note.text == "plain note";
                }));
                REQUIRE(om.path.size() == 2);
                CHECK(om.path.front().x == world.center.x() + 1);
                // The toggles say whether they are on.
                CHECK(toggle(screen, "TOGGLE_MAP_NOTES").value == "On");
                CHECK(toggle(screen, "TOGGLE_HORDES").value == "Off");
                CHECK(toggle(screen, "TOGGLE_LAND_USE_CODES").value == "Off");
                // The cursor is moved by an affordance of its own; the context's actions follow.
                const auto& move = affordance(screen, "overmap:cursor");
                REQUIRE(move.args.pos);
                CHECK(move.args.pos->unit == "overmap");
                CHECK_FALSE(move.label.empty());
                CHECK(ec::scope_of(move) == ec::expect_scope::boundary);
                CHECK(affordance(screen, "QUIT").role == "cancel");
                CHECK(affordance(screen, "CHOOSE_DESTINATION").enabled);
                CHECK(affordance(screen, "LEVEL_UP").enabled);
                CHECK(affordance(screen, "CREATE_NOTE").enabled);
                CHECK(affordance(screen, "SEARCH").enabled);
                // A square the bounds allow is taken; one outside them is refused.
                const auto near = ec::position{.x = cursor.x + 3, .y = cursor.y - 2, .z = cursor.z};
                CHECK(ec::resolve_invoke(
                          state, {.affordance_id = "overmap:cursor", .args = {.pos = near}})
                          .has_value());
                const auto far = ec::position{.x = 1 << 30, .y = 0, .z = 0};
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "overmap:cursor", .args = {.pos = far}})
                        .error()
                    == ec::error::invalid_arguments);
                CHECK(ec::resolve_invoke(state, {.affordance_id = "overmap:cursor", .args = {}})
                          .error()
                      == ec::error::invalid_arguments);
                return move_to(world.center + point(3, -2));
            }
            case 1:
                // The cursor moved and took the squares with it; the player did not move.
                CHECK(cursor == interaction_position{center.x + 3, center.y - 2, center.z});
                CHECK(om.player == center);
                CHECK(om.cursor_here.empty());
                CHECK(om.route_text == "Route of 2 squares.");
                CHECK(om.glyphs[static_cast<std::size_t>(
                          (om.rows / 2 + 2) * om.cols + om.cols / 2 - 3)]
                      == "@");
                return move_to(world.center + point(3, 0));
            case 2:
                // The note under the cursor is said in words.
                CHECK(om.cursor_note == "Note: cache");
                return move_to(world.center + point(3, -2) + tripoint_rel_omt(0, 0, 1));
            case 3:
                CHECK(cursor.z == center.z + 1);
                CHECK(om.origin.z == center.z + 1);
                return act("TOGGLE_MAP_NOTES");
            case 4:
                CHECK(toggle(screen, "TOGGLE_MAP_NOTES").value == "Off");
                CHECK(toggle(screen, "TOGGLE_MAP_NOTES").tone.empty());
                return act("QUIT");
            default:
                FAIL("the overmap outlived its quit");
                return act("QUIT");
        }
    });
    ui::omap::display();
    CHECK(step == 5);
}

TEST_CASE(
    "the overmap carries tile ids only for a client that renders tiles",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    auto tiles = false;
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto& om = squares(ec::top_screen(captured()));
        if (!tiles) {
            CHECK(om.tiles.empty());
            CHECK(om.looks_like.empty());
        } else {
            REQUIRE(om.tiles.size() == om.glyphs.size());
            CHECK(std::ranges::none_of(om.tiles, [](const auto& tile) { return tile.id.empty(); }));
            const auto at = [&](const int x, const int y) {
                return om.tiles[static_cast<std::size_t>(y * om.cols + x)].id;
            };
            // The square under the player is known; the one the world leaves unseen is not.
            CHECK(at(om.cols / 2, om.rows / 2) == "forest_thick");
            CHECK(std::ranges::contains(
                om.tiles, "unknown_terrain", &game_client::interaction_overmap_tile::id));
            CHECK(om.looks_like.contains(at(om.cols / 2, om.rows / 2)));
        }
        return act("QUIT");
    });
    for (const auto wants_tiles : {false, true}) {
        tiles = wants_tiles;
        game_client::set_overmap_tiles(wants_tiles);
        ui::omap::display();
    }
    CHECK(reads == 2);
}

TEST_CASE(
    "the overmap screen is the same at any viewport size and has the same ids in another language",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto saved_x = TERMX;
        const auto saved_y = TERMY;
        TERMX = 80;
        TERMY = 24;
        const auto narrow = published();
        TERMX = 200;
        TERMY = 60;
        const auto wide = published();
        TERMX = saved_x;
        TERMY = saved_y;
        CHECK(narrow.dump() == wide.dump());

        const auto korean = [] {
            const auto scope = localization::locale_scope(locale_fixture::korean);
            return published();
        }();
        auto english_ids = std::vector<std::string>{};
        auto korean_ids = std::vector<std::string>{};
        collect_ids(narrow, english_ids);
        collect_ids(korean, korean_ids);
        CHECK(english_ids == korean_ids);
        CHECK(without_words(narrow) == without_words(korean));
        return act("QUIT");
    });
    ui::omap::display();
    CHECK(reads == 1);
}

TEST_CASE(
    "the overmap screen reads the model, not its window, and draws no random number",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(2, 1), "C:cache");
    get_avatar().omt_path = {world.center + point_east};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto rng_before = rng_get_engine();
        const auto drawn = published();
        CHECK(rng_get_engine() == rng_before);
        // The native windows are blank, and the screen says what it said.
        catacurses::werase(g->w_overmap);
        catacurses::werase(g->w_omlegend);
        CHECK(published().dump() == drawn.dump());
        return act("QUIT");
    });
    ui::omap::display();
    CHECK(reads == 1);
}


TEST_CASE(
    "a read of overmap squares leaves the cursor, the rng and the overmaps alone",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(3, 0), "C:cache");
    get_avatar().omt_path = {world.center + point_east};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        const auto screen = ec::top_screen(captured());
        const auto& om = squares(screen);
        const auto cursor = cursor_of(screen);
        const auto rng_before = rng_get_engine();

        // A box around the player, a few squares of it outside the published window's reach.
        const auto min = world.center + point(-4, -2);
        const auto max = world.center + point(5, 3);
        const auto read = overmap_ui::known_squares(min, max, false);
        CHECK(read.cols == 10);
        CHECK(read.rows == 6);
        CHECK(read.origin == interaction_position{min.x(), min.y(), min.z()});
        CHECK(read.glyphs.size() == 60);
        CHECK(read.player == om.player);
        CHECK(read.path == om.path);
        CHECK(read.tiles.empty());
        // The squares are the screen's own, where the two overlap; the player marker is the
        // screen's overlay and shows in both.
        auto compared = 0;
        for (const auto y : std::views::iota(0, read.rows)) {
            for (const auto x : std::views::iota(0, read.cols)) {
                const auto there = static_cast<std::size_t>(
                    (min.y() + y - om.origin.y) * om.cols + (min.x() + x - om.origin.x));
                const auto here = static_cast<std::size_t>(y * read.cols + x);
                ++compared;
                CHECK(read.glyphs[here] == om.glyphs[there]);
                CHECK(read.foreground[here] == om.foreground[there]);
                CHECK(read.background[here] == om.background[there]);
            }
        }
        CHECK(compared == 60);
        CHECK(std::ranges::any_of(read.notes, [](const auto& note) {
            return note.text == "cache";
        }));

        // Squares no overmap holds are read as unknown, and no overmap is made for them.
        const auto far = world.center + point(180 * 40, 180 * 40);
        const auto om_of = [&](const tripoint_abs_omt& at) {
            return ACTIVE_OVERMAP_BUFFER.get_existing(project_to<coords::om>(at.xy()));
        };
        REQUIRE(om_of(far) == nullptr);
        const auto elsewhere = overmap_ui::known_squares(far, far + point(2, 2), true);
        CHECK(om_of(far) == nullptr);
        CHECK(elsewhere.cols == 3);
        CHECK(std::ranges::all_of(elsewhere.glyphs, [](const auto& glyph) {
            return glyph == "#";
        }));
        CHECK(std::ranges::all_of(elsewhere.tiles, [](const auto& tile) {
            return tile.id == "unknown_terrain";
        }));
        CHECK(rng_get_engine() == rng_before);
        // The controller's cursor did not move with the reads.
        CHECK(cursor_of(ec::top_screen(captured())) == cursor);
        return act("QUIT");
    });
    ui::omap::display();
    CHECK(reads == 1);
}

TEST_CASE(
    "the overmap screen publishes the labels, mission target and note heading the native one prints",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    uistate.overmap_show_city_labels = true;
    overmap_helpers::lay_city(world.center, point(-9, 4), "Testville");
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(8, -5), "LABEL:Depot");
    ACTIVE_OVERMAP_BUFFER.set_seen(world.center + point(8, -5), true);
    // A city on a square the player has not seen is not a label.
    overmap_helpers::lay_city(world.center, point(-20, -9), "Hidden");
    ACTIVE_OVERMAP_BUFFER.set_seen(world.center + point(-20, -9), false);
    overmap_helpers::lay_mission(world.center + point(200, 3));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        const auto& om = squares(screen);
        const auto label_at = [&](const point& offset) {
            const auto at = world.center + offset;
            return std::ranges::
                find(om.labels, interaction_position{at.x(), at.y(), at.z()},
                     &game_client::interaction_overmap_label::position);
        };
        switch (step++) {
            case 0: {
                const auto city = label_at(point(-9, 4));
                REQUIRE(city != om.labels.end());
                CHECK(city->text == "Testville");
                // The colours are those the native window prints the name in.
                const auto* const window = g->w_overmap.get<cata_cursesport::WINDOW>();
                const auto printed = native_cell_at(
                    g->w_overmap, city->position.x - (world.center.x() - window->width / 2),
                    city->position.y - (world.center.y() - window->height / 2));
                CHECK(city->foreground == printed.foreground);
                CHECK(city->background == printed.background);
                const auto note = label_at(point(8, -5));
                REQUIRE(note != om.labels.end());
                CHECK(note->text == "Depot");
                CHECK(label_at(point(-20, -9)) == om.labels.end());
                REQUIRE(om.mission_target);
                CHECK(*om.mission_target
                      == interaction_position{
                          world.center.x() + 200, world.center.y() + 3, world.center.z()});
                CHECK(om.notes_title == "List Notes");
                // The sidebar's own words about the mission, as the native sidebar prints them.
                CHECK(
                    om.legend
                    == std::vector<std::string>{
                        "Distance to active mission:", "200 tiles",
                        std::format("LEVEL {}, {}", world.center.z(),
                                    overmap_ui::fmt_omt_coords(world.center))});
                return act("TOGGLE_CITY_LABELS");
            }
            case 1:
                CHECK(om.labels.empty());
                return act("TOGGLE_OVERLAYS");
            case 2:
                // The marker is drawn only while the overlays are.
                CHECK_FALSE(om.mission_target);
                return act("QUIT");
            default:
                FAIL("the overmap outlived its quit");
                return act("QUIT");
        }
    });
    ui::omap::display();
    CHECK(step == 3);
}

TEST_CASE(
    "a map target in overmap squares says so whatever the mode of its map",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        ++reads;
        auto state = captured();
        auto stack = std::make_shared<std::vector<ec::screen_view>>(*state.stack);
        // The component is the copy's own, so that a mode other than `overmap` can be tried on it.
        auto& map = const_cast<ec::component&>(part(stack->back(), map_id));
        std::get<ec::map_body>(map.body).mode = "world";
        state.stack = std::move(stack);
        const auto top = json::parse(ec::serialize_interaction_boundary(state))["stack"].back();
        CHECK(top["root"]["children"][0]["target"]["unit"] == "omt");
        return act("QUIT");
    });
    ui::omap::display();
    CHECK(reads == 1);
}

TEST_CASE(
    "the overmap search publishes the result it points at and the cursor follows the next one",
    "[overmap_screen][engine_client_component]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(3, 0), "Qzxv near");
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(-6, 0), "Qzxv far");
    const auto stat_of = [](const ec::screen_view& screen, const std::string& key) {
        const auto& stats = body<ec::panel_body>(part(screen, "overmap:search")).stats;
        const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
        INFO(key);
        REQUIRE(found != stats.end());
        return *found;
    };
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        if (snapshot.field) {
            auto input = game_client::input_command{};
            input.interaction = game_client::interaction_command{
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::fill,
                .target_id = snapshot.field->id,
                .value = "Qzxv",
                .submit = true};
            const auto result =
                game_client::resolve_input_command(input, game_client::memory::screen_size());
            REQUIRE(result.has_value());
            return *result;
        }
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return act("SEARCH");
            case 1:
                CHECK(screen.kind == "overmap_search");
                CHECK(stat_of(screen, "search").value == "Qzxv");
                CHECK(stat_of(screen, "result").value == "1");
                CHECK(stat_of(screen, "result").max == "2");
                CHECK(stat_of(screen, "direction").value == "3 E");
                return act("NEXT_TAB");
            case 2:
                CHECK(stat_of(screen, "result").value == "2");
                CHECK(stat_of(screen, "direction").value == "6 W");
                return act("CONFIRM");
            case 3: {
                const auto far = world.center + point(-6, 0);
                CHECK(cursor_of(screen) == interaction_position{far.x(), far.y(), far.z()});
                return act("QUIT");
            }
            default:
                FAIL("the overmap outlived its quit");
                return act("QUIT");
        }
    });
    ui::omap::display();
    CHECK(step == 4);
}

#endif
