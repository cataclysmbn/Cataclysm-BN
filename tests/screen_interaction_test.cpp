#if defined(CATA_MCP)

#    include "avatar.h"
#    include "calendar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "coordinates.h"
#    include "cursesdef.h"
#    include "cursesport.h"
#    include "examine_item_menu.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "map/map.h"
#    include "map_helpers.h"
#    include "output.h"
#    include "player_activity.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <string>

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

auto event_of(game_client::interaction_command command) -> input_event {
    auto input = game_client::input_command{};
    input.interaction = std::move(command);
    const auto result =
        game_client::resolve_input_command(input, game_client::memory::screen_size());
    REQUIRE(result.has_value());
    return *result;
}

auto has_choice(const game_client::interaction_snapshot& snapshot, const std::string& id) -> bool {
    return std::ranges::find(snapshot.choices, id, &game_client::interaction_choice::id)
        != snapshot.choices.end();
}

} // namespace

TEST_CASE(
    "the item examine menu publishes the item and its actions",
    "[client][interaction][mcp][examine]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto& lighter = you.i_add(item::spawn("lighter", calendar::turn));
    auto reads = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        REQUIRE(snapshot.structured);
        CHECK(snapshot.context == "INVENTORY_ITEM");
        CHECK(snapshot.title.find("lighter") != std::string::npos);
        CHECK_FALSE(snapshot.message.empty());
        CHECK(has_choice(snapshot, "action:WIELD"));
        CHECK(has_choice(snapshot, "action:DROP"));
        CHECK(snapshot.allow_cancel);
        if (reads++ == 0) {
            // Wielding through the menu does what the key does and closes the menu.
            return event_of({
                .input_id = snapshot.input_id,
                .operation = game_client::interaction_operation::choose,
                .target_id = "action:WIELD",
            });
        }
        return event_of({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    examine_item_menu::
        run(lighter, [] { return 0; }, [] { return 40; }, examine_item_menu::menu_pos_t::right);
    CHECK(reads == 1);
    CHECK(you.is_wielding(lighter));
}

TEST_CASE(
    "the item examine menu closes on a semantic cancel", "[client][interaction][mcp][examine]") {
    clear_all_state();
    clear_map();
    auto& you = get_avatar();
    clear_character(you, false);
    const auto guard = screen_guard{};
    auto& lighter = you.i_add(item::spawn("lighter", calendar::turn));
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto snapshot = game_client::current_interaction();
        return event_of({
            .input_id = snapshot.input_id,
            .operation = game_client::interaction_operation::cancel,
        });
    });
    CHECK_FALSE(examine_item_menu::run(
        lighter, [] { return 0; }, [] { return 40; }, examine_item_menu::menu_pos_t::right));
    CHECK_FALSE(you.is_wielding(lighter));
}

#endif // CATA_MCP
