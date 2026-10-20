#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "color.h"
#    include "coordinates.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "output.h"
#    include "overmap/overmap.h"
#    include "overmap/overmap_ui.h"
#    include "overmap/overmapbuffer.h"
#    include "overmap_helpers.h"
#    include "player_helpers.h"
#    include "state_helpers.h"
#    include "type_id.h"
#    include "uistate.h"

#    include <algorithm>
#    include <array>
#    include <cstdint>
#    include <format>
#    include <functional>
#    include <string>
#    include <utility>
#    include <vector>

/// Pins what the native overmap prints, so that sharing its cell and sidebar facts with the engine
/// client keeps the player's screen byte-equal: every cell of the map window with its glyph and
/// colours, and the sidebar's rows. The terrain under the window is set square by square, so the
/// pin does not depend on what the generator makes.
namespace {

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

/// The key a client sends for a registered action.
auto act(const std::string& action) -> input_event {
    auto input = game_client::input_command{};
    input.action = action;
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

/// The window as rows of glyph cells, and a digest of every cell's glyph and two colour indexes.
struct drawn {
    std::vector<std::vector<std::string>> glyphs;
    std::uint64_t digest = 0;
};

auto drawn_of(const catacurses::window& window) -> drawn {
    const auto* const native = window.get<cata_cursesport::WINDOW>();
    auto result = drawn{.digest = 14695981039346656037ULL};
    const auto mix = [&](const std::uint64_t value) {
        result.digest = (result.digest ^ value) * 1099511628211ULL;
    };
    for (const auto& row : native->line) {
        auto line = std::vector<std::string>{};
        for (const auto& cell : row.chars) {
            line.push_back(cell.ch.empty() ? " " : cell.ch);
            for (const auto byte : cell.ch) { mix(static_cast<unsigned char>(byte)); }
            mix(static_cast<std::uint64_t>(cell.FG));
            mix(static_cast<std::uint64_t>(cell.BG));
        }
        result.glyphs.push_back(std::move(line));
    }
    return result;
}

auto trimmed(std::string text) -> std::string {
    while (!text.empty() && text.back() == ' ') { text.pop_back(); }
    return text;
}

/// The sidebar's coordinate row, which starts with its first words.
auto level_row(const std::vector<std::string>& sidebar) -> std::string {
    const auto found = std::ranges::find_if(sidebar, [](const auto& row) {
        return row.starts_with("│LEVEL ");
    });
    return found == sidebar.end() ? std::string{} : found->substr(0, found->find(" or "));
}
auto level_of(const tripoint_abs_omt& at) -> std::string {
    return std::format("│LEVEL {}, {}, {}", at.z(), at.x(), at.y());
}

/// The sidebar rows above the key hints, which the plain pin has.
auto facts_of(const std::vector<std::string>& sidebar) -> std::vector<std::string> {
    const auto end = std::ranges::find(sidebar, "│Use movement keys to pan.");
    return {sidebar.begin(), end};
}

struct frame {
    drawn map;
    std::vector<std::string> sidebar;
};

/// The map window and the sidebar rows of each input read of one open overmap.
auto frames_of(const std::vector<std::string>& keys) -> std::vector<frame> {
    auto frames = std::vector<frame>{};
    auto next = std::size_t{0};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        auto shown = frame{.map = drawn_of(g->w_overmap)};
        for (const auto& row : drawn_of(g->w_omlegend).glyphs) {
            const auto text = trimmed(std::ranges::fold_left(row, std::string{}, std::plus<>{}));
            if (text != "│") { shown.sidebar.push_back(text); }
        }
        frames.push_back(std::move(shown));
        return act(next < keys.size() ? keys[next++] : "QUIT");
    });
    ui::omap::display();
    return frames;
}

/// The cities an overmap had before the fixture set them aside.
struct held_cities {
    overmap* om;
    std::vector<city> cities;
};

struct fixed_world {
    tripoint_abs_omt center;
    /// Generated cities depend on the seed and name the squares near them, so only the cities a
    /// case lays exist while it runs.
    std::vector<held_cities> held;
    explicit fixed_world() {
        clear_all_state();
        center = get_avatar().abs_omt_pos();
        // The closest city is looked for one overmap around the square.
        for (auto y = -1; y <= 1; ++y) {
            for (auto x = -1; x <= 1; ++x) {
                const auto near = center.xy() + point(x, y) * OMAPX;
                auto* const om = ACTIVE_OVERMAP_BUFFER.get_existing_om_global(near).om;
                if (om != nullptr) {
                    held.push_back({.om = om, .cities = std::exchange(om->cities, {})});
                }
            }
        }
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
        clear_all_state();
        for (auto& [om, cities] : held) { om->cities = std::move(cities); }
    }
};

} // namespace

TEST_CASE(
    "the native overmap draws the same window and sidebar from the squares it shows",
    "[overmap][native_pin][mcp]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(3, 0), "C:cache");
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(-4, 2), "plain note");
    you.omt_path = {world.center + point_east, world.center + point_east * 2};
    ACTIVE_OVERMAP_BUFFER.toggle_path(world.center + point(0, 3));

    SECTION("plain overlays") {
        const auto frames = frames_of({});
        REQUIRE(frames.size() == 1);
        const auto& map = frames.front().map;
        const auto cols = map.glyphs.front().size();
        INFO(std::format("{} columns, {} rows", cols, map.glyphs.size()));
        const auto& center_row = map.glyphs[map.glyphs.size() / 2];
        // The player's own square, the first step of the route and the glyph note.
        CHECK(center_row[cols / 2] == "@");
        CHECK(center_row[cols / 2 + 1] == "!");
        CHECK(center_row[cols / 2 + 3] == "C");
        CHECK(map.digest == 0xc0191e572c767f87ULL);
        CHECK(level_row(frames.front().sidebar) == level_of(world.center));
        auto sidebar = frames.front().sidebar;
        REQUIRE(sidebar.size() == 28);
        CHECK(sidebar.back().starts_with(level_of(world.center)));
        sidebar.pop_back();
        CHECK(
            sidebar
            == std::vector<std::string>{
                "│F forest",
                "│Use movement keys to pan.",
                "│Press W to preview route.",
                "│Press again to confirm.",
                "│PPAGE or < - Go Up",
                "│NPAGE or > - Go Down",
                "│0 - Center On Character",
                "│/ - Search",
                "│N - Create/Edit Note",
                "│D - Delete Note",
                "│L - List Notes",
                "│M - View Missions",
                "│G - Toggle Map Notes",
                "│B - Toggle Blinking",
                "│O - Toggle Overlays",
                "│A - Toggle Land Use Codes",
                "│C - Toggle City Labels",
                "│H - Toggle Hordes",
                "│E - Toggle Explored",
                "│P - Mark/unmark Travel Path",
                "│f - Toggle Fast Scroll",
                "│T - Toggle Forest Trails",
                "│w - Toggle Visible Weather",
                "│. - Toggle Map Defaulting",
                "│to Level 0",
                "│c - Toggle Custom Waypoint",
                "│? - Display keybindings"});
    }

    SECTION("land use codes, a moved cursor and a hidden note") {
        uistate.overmap_show_land_use_codes = true;
        uistate.overmap_show_map_notes = false;
        const auto frames = frames_of({"RIGHT", "RIGHT"});
        REQUIRE(frames.size() == 3);
        CHECK(frames[0].map.digest == 0xd137208964d5d88fULL);
        CHECK(frames[1].map.digest == 0xfdce65e30963d717ULL);
        CHECK(frames[2].map.digest == 0xff4bf3d68729a342ULL);
        CHECK(level_row(frames[2].sidebar) == level_of(world.center + point_east * 2));
    }
}


TEST_CASE(
    "the native overmap draws city and note labels, a mission marker and its distance",
    "[overmap][native_pin][mcp]") {
    const auto world = fixed_world{};
    const auto guard = screen_guard{};
    uistate.overmap_show_city_labels = true;
    overmap_helpers::lay_city(world.center, point(-9, 4), "Testville");
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(8, -5), "LABEL:Depot");
    ACTIVE_OVERMAP_BUFFER.set_seen(world.center + point(8, -5), true);
    ACTIVE_OVERMAP_BUFFER.add_note(world.center + point(2, 2), "C:cache");

    SECTION("a target beyond the window") {
        overmap_helpers::lay_mission(world.center + point(200, 3));
        const auto frames = frames_of({});
        REQUIRE(frames.size() == 1);
        const auto text = [&](const drawn& map) {
            return std::ranges::
                fold_left(map.glyphs[map.glyphs.size() / 2 + 4], std::string{}, std::plus<>{});
        };
        CHECK(text(frames.front().map).contains("Testville"));
        CHECK(frames.front().map.digest == 0x4797df5b5571c084ULL);
        CHECK(
            facts_of(frames.front().sidebar)
            == std::vector<std::string>{
                "│F forest in northeast", "│  Testville",
                "│Distance to active mission:", "│200 tiles"});
        CHECK(level_row(frames.front().sidebar) == level_of(world.center));
    }

    SECTION("a target on the square of the cursor") {
        overmap_helpers::lay_mission(world.center + point(2, 0));
        const auto frames = frames_of({"RIGHT", "RIGHT"});
        REQUIRE(frames.size() == 3);
        CHECK(frames[0].map.digest == 0xa8a7cb686515f642ULL);
        CHECK(frames[2].map.digest == 0xc04e54f8745bfd6aULL);
        CHECK(
            facts_of(frames[2].sidebar)
            == std::vector<std::string>{
                "│F forest in east Testville", "│Distance to active mission:", "│0 tiles",
                "│Missing Caravan"});
        CHECK(level_row(frames[2].sidebar) == level_of(world.center + point_east * 2));
    }
}

#endif
