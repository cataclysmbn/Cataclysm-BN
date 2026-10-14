#if defined(CATA_MCP)

#    include "armor_layers.h"
#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_command.h"
#    include "client_input.h"
#    include "client_interaction.h"
#    include "client_memory.h"
#    include "client_memory_scope.h"
#    include "cursesdef.h"
#    include "engine_client_component.h"
#    include "engine_client_contract.h"
#    include "examine_item_menu.h"
#    include "game.h"
#    include "game_constants.h"
#    include "input.h"
#    include "item.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "map_helpers.h"
#    include "output.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <nlohmann/json.hpp>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using json = nlohmann::json;

struct screen_guard {
    const game_client::memory::scoped_state memory;
    bool old_test_mode = test_mode;
    int old_termx = TERMX;
    int old_termy = TERMY;
    int old_full_screen_width = FULL_SCREEN_WIDTH;
    int old_full_screen_height = FULL_SCREEN_HEIGHT;

    screen_guard() {
        clear_all_state();
        clear_map();
        clear_character(get_avatar(), false);
        get_avatar().moves = 100;
        test_mode = false;
        TERMX = 140;
        TERMY = 50;
        FULL_SCREEN_WIDTH = 80;
        FULL_SCREEN_HEIGHT = 24;
        game_client::memory::resize(TERMX, TERMY);
        catacurses::stdscr = catacurses::newwin(TERMY, TERMX, point_zero);
        catacurses::newscr = catacurses::newwin(TERMY, TERMX, point_zero);
    }

    ~screen_guard() {
        game_client::memory::set_input_provider({});
        TERMX = old_termx;
        TERMY = old_termy;
        FULL_SCREEN_WIDTH = old_full_screen_width;
        FULL_SCREEN_HEIGHT = old_full_screen_height;
        test_mode = old_test_mode;
    }
};

/// Two equal shirts, jeans and a hoodie, worn in that order.
void dress(avatar& you) {
    for (const auto* const id : {"tshirt", "tshirt", "jeans", "hoodie"}) {
        you.wear_item(item::spawn(id), false);
    }
}

const auto shirt_a = std::string{"worn:tshirt:0"};
const auto shirt_b = std::string{"worn:tshirt:1"};
const auto jeans_id = std::string{"worn:jeans:0"};
const auto hoodie_id = std::string{"worn:hoodie:0"};

auto captured() -> ec::boundary_state {
    const auto state = ec::capture_boundary(
        {.epoch = "epoch:armor",
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

auto part(const ec::screen_view& screen, const std::string& id) -> const ec::component& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    return *found;
}
auto has_part(const ec::screen_view& screen, const std::string& id) -> bool {
    return screen.root && ec::find_component(*screen.root, id) != nullptr;
}
template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}
auto rows(const ec::screen_view& screen, const std::string& id) -> const std::vector<ec::row>& {
    return body<ec::list_body>(part(screen, id)).rows;
}
auto ids_of(const std::vector<ec::row>& list) -> std::vector<std::string> {
    return list | std::views::transform(&ec::row::id) | std::ranges::to<std::vector>();
}
auto worn_ids(const ec::screen_view& screen) -> std::vector<std::string> {
    return ids_of(rows(screen, "armor:worn:rows"));
}
auto text_of(const ec::screen_view& screen, const std::string& id) -> const std::string& {
    return body<ec::text_body>(part(screen, id)).markup;
}
auto stat_of(const ec::screen_view& screen, const std::string& panel, const std::string& key)
    -> std::string {
    const auto& stats = body<ec::panel_body>(part(screen, panel)).stats;
    const auto found = std::ranges::find(stats, key, &ec::panel_stat::key);
    INFO(panel << " " << key);
    REQUIRE(found != stats.end());
    return found->value;
}
auto column_of(const ec::row& row, const std::string& key) -> const std::string& {
    const auto found = std::ranges::find(row.columns, key, &game_client::interaction_column::key);
    REQUIRE(found != row.columns.end());
    return found->value;
}
auto row_of(const std::vector<ec::row>& list, const std::string& id) -> const ec::row& {
    const auto found = std::ranges::find(list, id, &ec::row::id);
    INFO(id);
    REQUIRE(found != list.end());
    return *found;
}

/// The top screen as a client reads it, with the ids of this screen instance left out.
auto published() -> json {
    auto top = json::parse(ec::serialize_interaction_boundary(captured()))["stack"].back();
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

} // namespace

TEST_CASE(
    "the armor layers screen lists the worn items by identity and a picked one takes the cursor",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "SORT_ARMOR");
        switch (step++) {
            case 0: {
                CHECK(worn_ids(screen)
                      == std::vector<std::string>{shirt_a, shirt_b, jeans_id, hoodie_id});
                CHECK(screen.focus_component == "armor:worn:rows");
                CHECK(screen.focus_row == shirt_a);
                const auto& jeans = row_of(rows(screen, "armor:worn:rows"), jeans_id);
                CHECK(jeans.label.find("jeans") != std::string::npos);
                CHECK(jeans.label.find("<color") == std::string::npos);
                CHECK(column_of(jeans, "hidden") == "No");
                CHECK_FALSE(column_of(jeans, "storage").empty());
                CHECK(jeans.look.has_value());
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "armor:pick", .args = {.row = "worn:none:0"}})
                        .error()
                    == ec::error::invalid_arguments);
                CHECK(ec::resolve_invoke(
                          state, {.affordance_id = "armor:pick", .args = {.row = hoodie_id}})
                          .has_value());
                return invoke("armor:pick", {.row = hoodie_id});
            }
            case 1:
                CHECK(screen.focus_row == hoodie_id);
                CHECK(text_of(screen, "armor:notes").find("worn over your other clothes")
                      != std::string::npos);
                return act("QUIT");
            default:
                FAIL("the armor screen outlived its quit");
                return act("QUIT");
        }
    });
    show_armor_layers_ui(you);
    CHECK(step == 2);
}

TEST_CASE(
    "the armor layers screen filters by a picked body part, named by its id",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        switch (step++) {
            case 0: {
                const auto& tabs = body<ec::tabs_body>(part(screen, "armor:tabs"));
                REQUIRE(tabs.tabs.size() > 2);
                CHECK(tabs.tabs.front().id == "part:all");
                CHECK(tabs.tabs.front().selected);
                CHECK(std::ranges::any_of(tabs.tabs, [](const auto& tab) {
                    return tab.id == "part:leg_l";
                }));
                CHECK(tabs.select == "armor:part");
                CHECK_FALSE(text_of(screen, "armor:notes").empty());
                CHECK(
                    ec::resolve_invoke(
                        state, {.affordance_id = "armor:part", .args = {.row = "part:nowhere"}})
                        .error()
                    == ec::error::invalid_arguments);
                return invoke("armor:part", {.row = "part:leg_l"});
            }
            case 1: {
                CHECK(worn_ids(screen) == std::vector<std::string>{jeans_id});
                CHECK(screen.focus_row == jeans_id);
                const auto& tabs = body<ec::tabs_body>(part(screen, "armor:tabs"));
                const auto selected = std::ranges::find(tabs.tabs, true, &ec::tab_entry::selected);
                REQUIRE(selected != tabs.tabs.end());
                CHECK(selected->id == "part:leg_l");
                // The total protection of the part is shown only with a part picked.
                CHECK_FALSE(stat_of(screen, "armor:worn", "bash").empty());
                return invoke("armor:part", {.row = "part:all"});
            }
            case 2:
                CHECK(worn_ids(screen).size() == 4);
                CHECK(body<ec::panel_body>(part(screen, "armor:worn")).stats.empty());
                return act("QUIT");
            default:
                FAIL("unexpected native read");
                return act("QUIT");
        }
    });
    show_armor_layers_ui(you);
    CHECK(step == 3);
}

TEST_CASE(
    "the armor layers screen moves a grabbed item with the cursor and keeps its identity",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("armor:pick", {.row = jeans_id});
            case 1:
                return act("MOVE_ARMOR");
            case 2:
                CHECK(row_of(rows(screen, "armor:worn:rows"), jeans_id).selected);
                // A grabbed item moves with the cursor: picking another row does not move the
                // cursor.
                return invoke("armor:pick", {.row = hoodie_id});
            case 3:
                CHECK(screen.focus_row == jeans_id);
                return act("UP");
            case 4:
                CHECK(worn_ids(screen)
                      == std::vector<std::string>{shirt_a, jeans_id, shirt_b, hoodie_id});
                CHECK(screen.focus_row == jeans_id);
                CHECK(row_of(rows(screen, "armor:worn:rows"), jeans_id).selected);
                return act("MOVE_ARMOR");
            case 5:
                CHECK_FALSE(row_of(rows(screen, "armor:worn:rows"), jeans_id).selected);
                return act("QUIT");
            default:
                FAIL("unexpected native read");
                return act("QUIT");
        }
    });
    show_armor_layers_ui(you);
    CHECK(step == 6);
}

TEST_CASE(
    "the armor layers screen sorts the worn items into layer order",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        switch (step++) {
            case 0:
                return invoke("armor:pick", {.row = hoodie_id});
            case 1:
                return act("MOVE_ARMOR");
            case 2:
            case 3:
            case 4:
                return act("UP");
            case 5:
                CHECK(worn_ids(screen).front() == hoodie_id);
                return act("MOVE_ARMOR");
            case 6:
                return act("SORT_ARMOR");
            case 7:
                CHECK(worn_ids(screen).back() == hoodie_id);
                return act("QUIT");
            default:
                FAIL("unexpected native read");
                return act("QUIT");
        }
    });
    show_armor_layers_ui(you);
    CHECK(step == 8);
}

TEST_CASE(
    "the armor layers screen publishes the item's figures, its notes and the encumbrance it adds",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    dress(you);
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        CHECK(body<ec::panel_body>(part(screen, "armor:detail")).title.find("t-shirt")
              != std::string::npos);
        CHECK(stat_of(screen, "armor:properties", "coverage") == "50");
        CHECK(stat_of(screen, "armor:properties", "warmth") == "5");
        CHECK(stat_of(screen, "armor:protection", "bash") == "1");
        // Two equal shirts on one layer cost encumbrance together.
        CHECK(text_of(screen, "armor:notes").find("adding encumbrance") != std::string::npos);
        const auto& layers = rows(screen, "armor:layers:rows");
        const auto& header = row_of(layers, "cover:part:torso");
        CHECK(header.header);
        CHECK_FALSE(header.selectable);
        const auto& layer = row_of(layers, "layer:part:torso:" + shirt_a);
        CHECK(column_of(layer, "encumbrance") == "0+");
        CHECK(column_of(row_of(layers, "layer:part:torso:" + hoodie_id), "encumbrance") == "12");
        const auto& encumbrance = rows(screen, "armor:encumbrance:rows");
        const auto& torso = row_of(encumbrance, "encumbrance:part:torso");
        CHECK(column_of(torso, "encumbrance") == "12");
        CHECK(column_of(torso, "layering") == "2");
        CHECK(torso.color == string_from_color(c_green));
        CHECK(
            row_of(encumbrance, "encumbrance:part:head").color == string_from_color(c_light_gray));
        return act("QUIT");
    });
    show_armor_layers_ui(you);
}

TEST_CASE(
    "the armor layers screen says when nothing is worn and ignores hiding a sprite then",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        if (step++ == 0) {
            CHECK(rows(screen, "armor:worn:rows").empty());
            CHECK_FALSE(text_of(screen, "armor:worn:none").empty());
            CHECK_FALSE(text_of(screen, "armor:detail").empty());
            CHECK(screen.focus_row.empty());
            return act("TOGGLE_CLOTH");
        }
        return act("QUIT");
    });
    show_armor_layers_ui(get_avatar());
    CHECK(step == 2);
}

TEST_CASE(
    "the armor layers screen is the same at any viewport size and has the same ids in another language",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    dress(get_avatar());
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
    show_armor_layers_ui(get_avatar());
    CHECK(reads == 1);
}

TEST_CASE(
    "the armor layers screen publishes no window text", "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    dress(get_avatar());
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto screen = ec::top_screen(captured());
        for (const auto* const list :
             {"armor:worn:rows", "armor:layers:rows", "armor:encumbrance:rows"}) {
            for (const auto& row : rows(screen, list)) {
                CHECK(row.label.find("<color") == std::string::npos);
                CHECK(row.id.find(row.label) == std::string::npos);
            }
        }
        return act("QUIT");
    });
    show_armor_layers_ui(get_avatar());
}

namespace {
auto examine_rows(const ec::screen_view& screen) -> const std::vector<ec::row>& {
    return rows(screen, "examine:actions");
}
} // namespace

TEST_CASE(
    "the item examine menu lists its actions by their id, a selected one takes the cursor and a run one runs",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    auto& you = get_avatar();
    auto& shirt = you.i_add(item::spawn("tshirt"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        const auto state = captured();
        const auto screen = ec::top_screen(state);
        REQUIRE(screen.context == "INVENTORY_ITEM");
        CHECK(screen.title.find("t-shirt") != std::string::npos);
        CHECK(screen.focus_component == "examine:actions");
        CHECK(text_of(screen, "examine:info").find("Coverage") != std::string::npos);
        if (step == 1) {
            // Selecting only moves the cursor: nothing ran, and the menu is still open.
            CHECK(screen.focus_row == "READ");
            CHECK_FALSE(shirt.is_favorite);
            ++step;
            return invoke("examine:run", {.row = "FAVORITE_ADD"});
        }
        REQUIRE(step++ == 0);
        CHECK(screen.focus_row == "ACTIVATE");
        const auto ids = ids_of(examine_rows(screen));
        CHECK(std::ranges::find(ids, "WEAR") != ids.end());
        CHECK(std::ranges::find(ids, "FAVORITE_ADD") != ids.end());
        CHECK(std::ranges::find(ids, "FAVORITE_REMOVE") == ids.end());
        CHECK(row_of(examine_rows(screen), "WEAR").color == string_from_color(c_light_green));
        CHECK(row_of(examine_rows(screen), "UNLOAD").color == string_from_color(c_light_gray));
        CHECK_FALSE(row_of(examine_rows(screen), "WEAR").label.empty());
        CHECK(
            ec::resolve_invoke(
                state, {.affordance_id = "examine:select", .args = {.row = "NOT_AN_ACTION"}})
                .error()
            == ec::error::invalid_arguments);
        CHECK(
            ec::resolve_invoke(
                state, {.affordance_id = "examine:run", .args = {.row = "NOT_AN_ACTION"}})
                .error()
            == ec::error::invalid_arguments);
        return invoke("examine:select", {.row = "READ"});
    });
    const auto stays_open = examine_item_menu::
        run(shirt, [] { return 50; }, [] { return 40; }, examine_item_menu::menu_pos_t::left);
    CHECK(step == 2);
    CHECK(shirt.is_favorite);
    CHECK_FALSE(stays_open);
}

TEST_CASE(
    "the item examine menu moves its cursor with the native keys and is the same at any viewport size",
    "[armor_screen][engine_client_component]") {
    const auto guard = screen_guard{};
    const auto fixture_ko = locale_fixture::korean_catalogue{};
    auto& shirt = get_avatar().i_add(item::spawn("tshirt"));
    auto step = 0;
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        if (step++ == 0) {
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
            return act("DOWN");
        }
        const auto screen = ec::top_screen(captured());
        CHECK(screen.focus_row == "READ");
        return act("QUIT");
    });
    CHECK_FALSE(examine_item_menu::run(
        shirt, [] { return 50; }, [] { return 40; }, examine_item_menu::menu_pos_t::right));
    CHECK(step == 2);
}

#endif
