#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "iuse_software_kitten.h"
#    include "iuse_software_lightson.h"
#    include "iuse_software_minesweeper.h"
#    include "iuse_software_snake.h"
#    include "iuse_software_sokoban.h"
#    include "mcp_session.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <array>
#    include <deque>
#    include <map>
#    include <ranges>
#    include <string>
#    include <vector>

namespace {

using namespace client_screen_test;
namespace ec = engine_client;

auto canvas_of(const ec::screen_view& top) -> const game_client::interaction_canvas& {
    REQUIRE(top.root);
    REQUIRE(std::holds_alternative<game_client::interaction_canvas>(top.root->body));
    return std::get<game_client::interaction_canvas>(top.root->body);
}

auto lines_of(const ec::screen_view& top) -> std::vector<std::string> {
    auto lines = std::vector<std::string>{};
    for (const auto& row : canvas_of(top).rows) {
        auto line = std::string{};
        for (const auto& run : row) { line += run.text; }
        lines.push_back(line);
    }
    return lines;
}

auto text_of(const ec::screen_view& top) -> std::string {
    auto text = std::string{};
    for (const auto& line : lines_of(top)) { text += line + '\n'; }
    return text;
}

auto count_of(const ec::screen_view& top, const char glyph) -> std::ptrdiff_t {
    const auto text = text_of(top);
    return std::ranges::count(text, glyph);
}

auto has_run(const ec::screen_view& top, const std::string& text, const int foreground) -> bool {
    return std::ranges::any_of(canvas_of(top).rows, [&](const auto& row) {
        return std::ranges::any_of(row, [&](const auto& run) {
            return run.text == text && run.foreground == foreground;
        });
    });
}

auto has_affordance(const ec::screen_view& top, const std::string& id) -> bool {
    return std::ranges::contains(top.affordances | std::views::transform(&ec::affordance::id), id);
}

auto action(const std::string& id) -> input_event { return event_of({.action = id}); }

auto tick() -> input_event {
    auto result = input_event{};
    result.type = input_event_t::timeout;
    return result;
}

} // namespace

TEST_CASE(
    "snake publishes its board with colors and is redrawn by every timed tick",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    auto first_length = std::ptrdiff_t{};
    open_stack([&] { snake_game{}.start_game(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   CHECK(top.title == "S N A K E");
                   CHECK(canvas_of(top).rows.size() == 24);
                   CHECK(canvas_of(top).cols == 80);
                   CHECK(text_of(top).find("Score: 0") != std::string::npos);
                   CHECK(top.refresh_ms > 0);
                   if (stage++ == 0) {
                       first_length = count_of(top, '#');
                       CHECK(first_length >= 1);
                       return tick();
                   }
                   if (stage < 4) { return tick(); }
                   // Each timed tick grew the drawn body by one cell.
                   CHECK(count_of(top, '#') == first_length + 3);
                   CHECK(has_run(top, "*", 9));
                   return action("QUIT");
               });
    CHECK(stage == 4);
}

TEST_CASE(
    "a timed mini-game's session input wait lasts the screen's refresh",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto seen = false;
    open_stack([&] { snake_game{}.start_game(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   seen = true;
                   // The session ends the native wait itself, as the native input wait would.
                   CHECK(top.refresh_ms > 0);
                   CHECK(bn::mcp::refresh_wait_ms() == top.refresh_ms);
                   return action("QUIT");
               });
    CHECK(seen);
}

TEST_CASE(
    "snake stops announcing a refresh once the game is over",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto over = false;
    auto turns = 0;
    // Grow past five cells, then turn back into the body: up, left, down.
    const auto script = std::vector<std::string>{"", "", "", "", "", "", "UP", "LEFT", "DOWN"};
    open_stack([&] { snake_game{}.start_game(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   if (text_of(top).find("TOTAL SCORE") != std::string::npos) {
                       over = true;
                       CHECK(top.refresh_ms == 0);
                       return action("QUIT");
                   }
                   const auto& step = script.at(turns++);
                   return step.empty() ? tick() : action(step);
               });
    CHECK(over);
}

TEST_CASE(
    "minesweeper publishes the border and the field and a flag appears after the flag key",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] { minesweeper_game{}.start_game(); },
        [&](const auto& snapshot, const auto& top) {
            if (top.context != "MINESWEEPER") {
                const auto beginner = std::ranges::
                    find(snapshot.choices, "Beginner", &game_client::interaction_choice::label);
                REQUIRE(beginner != snapshot.choices.end());
                return choose(snapshot, beginner->id);
            }
            CHECK(top.refresh_ms == 0);
            CHECK(text_of(top).find("Minesweeper") != std::string::npos);
            if (stage++ == 0) {
                CHECK(count_of(top, '#') == 64);
                CHECK(count_of(top, '!') == 0);
                return action("FLAG");
            }
            CHECK(count_of(top, '#') == 63);
            CHECK(has_run(top, "!", 11));
            return action("QUIT");
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "lights on publishes the board and a toggle changes it",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    auto before = std::string{};
    open_stack([&] { lightson_game{}.start_game(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   CHECK(text_of(top).find("Lights on!") != std::string::npos);
                   if (stage++ == 0) {
                       before = text_of(top);
                       return action("TOGGLE_SPACE");
                   }
                   CHECK(text_of(top) != before);
                   return action("QUIT");
               });
    CHECK(stage == 2);
}

TEST_CASE(
    "sokoban publishes its level and a move key changes the move count",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack([&] { sokoban_game{}.start_game(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   CHECK(top.title == "Sokoban");
                   CHECK(text_of(top).find("Level: 1/") != std::string::npos);
                   if (stage++ == 0) {
                       CHECK(text_of(top).find("Moves: 0") != std::string::npos);
                       return action("UP");
                   }
                   return action("QUIT");
               });
    CHECK(stage == 2);
}

TEST_CASE(
    "robotfindskitten publishes its instructions and then its field after a key",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] {
            auto findkitten = robot_finds_kitten{};
            static_cast<void>(findkitten);
        },
        [&](const auto& /*snapshot*/, const auto& top) {
            CHECK(top.title == "robotfindskitten v22July2008");
            if (stage++ == 0) {
                CHECK(text_of(top).find("Press any key to start.") != std::string::npos);
                REQUIRE(has_affordance(top, "ANY_INPUT"));
                return action("ANY_INPUT");
            }
            CHECK(count_of(top, '#') >= 1);
            return action("QUIT");
        });
    CHECK(stage == 2);
}

TEST_CASE(
    "robotfindskitten routes any key without an action to its invalid command line",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] {
            auto findkitten = robot_finds_kitten{};
            static_cast<void>(findkitten);
        },
        [&](const auto& /*snapshot*/, const auto& top) {
            if (stage++ == 0) { return action("ANY_INPUT"); }
            if (stage == 2) {
                CHECK(text_of(top).find("Invalid command") == std::string::npos);
                REQUIRE(has_affordance(top, "ANY_INPUT"));
                return action("ANY_INPUT");
            }
            CHECK(text_of(top).find("Invalid command") != std::string::npos);
            return action("QUIT");
        });
    CHECK(stage == 3);
}

namespace {

struct cell_position {
    int x;
    int y;
    auto operator<=>(const cell_position&) const = default;
};

/// The next move toward a free square beside an object not yet touched, as the action to send.
/// The field starts at canvas column 1, row 1 (inside the border) and its first three rows are the
/// status area.
auto next_probe(const std::vector<std::string>& lines, std::vector<cell_position>& touched)
    -> std::string {
    constexpr auto first_row = 3;
    const auto at = [&](const cell_position p) {
        const auto& line = lines.at(p.y + 1);
        return static_cast<std::size_t>(p.x + 1) < line.size() ? line[p.x + 1] : ' ';
    };
    auto robot = cell_position{-1, -1};
    auto objects = std::vector<cell_position>{};
    for (auto y = first_row; y < 20; ++y) {
        for (auto x = 0; x < 60; ++x) {
            const auto glyph = at({x, y});
            if (glyph == '#') {
                robot = {x, y};
            } else if (glyph != ' ') {
                objects.push_back({x, y});
            }
        }
    }
    constexpr auto moves = std::array<std::pair<const char*, cell_position>, 4>{
        {{"UP", {0, -1}}, {"DOWN", {0, 1}}, {"LEFT", {-1, 0}}, {"RIGHT", {1, 0}}}};
    for (const auto& [name, step] : moves) {
        const auto beside = cell_position{robot.x + step.x, robot.y + step.y};
        if (std::ranges::contains(objects, beside) && !std::ranges::contains(touched, beside)) {
            touched.push_back(beside);
            return name;
        }
    }
    // Breadth-first search over free squares for one beside an untouched object.
    auto first_move = std::map<cell_position, std::string>{{robot, ""}};
    auto queue = std::deque<cell_position>{robot};
    while (!queue.empty()) {
        const auto here = queue.front();
        queue.pop_front();
        for (const auto& [name, step] : moves) {
            const auto next = cell_position{here.x + step.x, here.y + step.y};
            if (next.x < 0 || next.x >= 60 || next.y < first_row || next.y >= 20
                || first_move.contains(next) || at(next) != ' ') {
                continue;
            }
            first_move[next] = here == robot ? name : first_move.at(here);
            const auto beside_untouched = std::ranges::any_of(moves, [&](const auto& m) {
                const auto o = cell_position{next.x + m.second.x, next.y + m.second.y};
                return std::ranges::contains(objects, o) && !std::ranges::contains(touched, o);
            });
            if (beside_untouched) { return first_move.at(next); }
            queue.push_back(next);
        }
    }
    return "";
}

} // namespace

TEST_CASE(
    "robotfindskitten publishes each frame of its ending animation before the last",
    "[client][interaction][mcp][screens][minigames]") {
    clear_all_state();
    const auto guard = screen_guard{};
    auto touched = std::vector<cell_position>{};
    auto frames = std::vector<std::string>{};
    auto acknowledged = false;
    auto started = false;
    open_stack(
        [&] {
            auto findkitten = robot_finds_kitten{};
            static_cast<void>(findkitten);
        },
        [&](const auto& /*snapshot*/, const auto& top) {
            if (!started) {
                started = true;
                return action("ANY_INPUT");
            }
            if (top.refresh_ms > 0) {
                frames.push_back(text_of(top));
                return tick();
            }
            if (text_of(top).find("You found kitten") != std::string::npos) {
                acknowledged = true;
                CHECK(has_affordance(top, "ANY_INPUT"));
                return action("ANY_INPUT");
            }
            const auto move = next_probe(lines_of(top), touched);
            REQUIRE(!move.empty());
            return action(move);
        });
    CHECK(acknowledged);
    REQUIRE(frames.size() == 5);
    CHECK(std::ranges::none_of(frames, [](const auto& f) {
        return f.find("You found kitten") != std::string::npos;
    }));
    CHECK(frames[3].find("<3<3<3") == std::string::npos);
    CHECK(frames[4].find("<3<3<3") != std::string::npos);
    CHECK(frames[0] != frames[1]);
}

#endif // CATA_MCP
