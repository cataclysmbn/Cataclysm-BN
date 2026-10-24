#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "editor/editor_access.h"
#    include "editor/editor_session.h"
#    include "engine_client_component.h"
#    include "filesystem.h"
#    include "game.h"
#    include "gamemode.h"
#    include "gamemode_editor.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "main_menu.h"
#    include "map/map.h"
#    include "state_helpers.h"
#    include "type_id.h"
#    include "world.h"
#    include "worldfactory.h"

#    include <algorithm>
#    include <memory>
#    include <ranges>
#    include <string>
#    include <variant>
#    include <vector>

namespace {

using namespace client_screen_test;
namespace ec = engine_client;

template <typename Body> auto body(const ec::component& value) -> const Body& {
    REQUIRE(std::holds_alternative<Body>(value.body));
    return std::get<Body>(value.body);
}

/// The banner and the tab strip the main menu is made of.
struct menu_parts {
    const ec::text_body& banner;
    const ec::tabs_body& tabs;
    const ec::component& content;
};
auto parts_of(const ec::screen_view& top) -> menu_parts {
    REQUIRE(top.root);
    const auto& root = *top.root;
    REQUIRE(std::holds_alternative<ec::split_body>(root.body));
    REQUIRE(root.children.size() == 2);
    const auto& tabs = root.children.back();
    return {body<ec::text_body>(root.children.front()), body<ec::tabs_body>(tabs),
            tabs.children.front()};
}

auto find(const ec::screen_view& top, const std::string& id) -> const ec::affordance* {
    const auto found = std::ranges::find(top.affordances, id, &ec::affordance::id);
    return found == top.affordances.end() ? nullptr : &*found;
}

/// The main menu forgets the world and the character the tests run in; this puts both back.
struct menu_restore {
    std::string world_name = world_generator->active_world->info->world_name;
    std::string avatar_name = get_avatar().name;
    ~menu_restore() {
        world_generator->set_active_world(world_generator->get_world(world_name));
        get_avatar() = avatar();
        get_avatar().create(character_type::NOW);
        get_avatar().name = avatar_name;
    }
};

/// `game::handle_action` and the game mode are private; the explicit instantiations reach them
/// without changing the game's headers, as client_interaction_test.cpp does.
struct handle_action_member {
    using type = auto (game::*)() -> bool;
    friend auto native_member(handle_action_member) -> type;
};
template <typename Tag, typename Tag::type Member> struct native_test_member {
    friend auto native_member(Tag /*tag*/) -> typename Tag::type { return Member; }
};
struct mode_member {
    using type = std::unique_ptr<special_game> game::*;
    friend auto native_member(mode_member) -> type;
};
template struct native_test_member<handle_action_member, &game::handle_action>;
template struct native_test_member<mode_member, &game::gamemode>;

const auto tab_ids = std::vector<std::string>{
    "motd", "new_game", "load", "world", "editor", "settings", "help", "credits", "quit"};

} // namespace

TEST_CASE(
    "the main menu publishes its banner, tabs and entries and its tab affordance picks a tab",
    "[client][interaction][mcp][screens][main_menu]") {
    clear_all_state();
    const auto restore = menu_restore{};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] { main_menu{}.opening_screen(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            switch (stage++) {
                case 0: {
                    CHECK(top.role == "screen");
                    const auto [banner, tabs, content] = parts_of(top);
                    CHECK(banner.role == "banner");
                    CHECK_FALSE(banner.markup.empty());
                    CHECK((tabs.tabs | std::views::transform(&ec::tab_entry::id)
                           | std::ranges::to<std::vector>())
                          == tab_ids);
                    CHECK(std::ranges::count(tabs.tabs, true, &ec::tab_entry::selected) == 1);
                    CHECK(tabs.select == "menu:tab");
                    // The selected tab lists its entries, which have an affordance of their own.
                    const auto& entries = body<ec::list_body>(content);
                    CHECK_FALSE(entries.rows.empty());
                    CHECK(entries.activate == "menu:entry");
                    CHECK(find(top, "menu:entry") != nullptr);
                    CHECK(find(top, "QUIT")->role == "cancel");
                    // The arrows move the menu's cursor: no action is named for a map.
                    CHECK(std::ranges::none_of(top.affordances, [](const auto& action) {
                        return action.label.find("Pan") != std::string::npos;
                    }));
                    CHECK(find(top, "UP")->label == "Move cursor up");
                    // No key stands in for a button.
                    CHECK(find(top, "ANY_INPUT") == nullptr);

                    // A tab the menu does not have is refused, and a tab it has is run.
                    const auto state = ec::capture_boundary(
                        {.epoch = "epoch:test",
                         .ready = {.phase = "waiting_for_input",
                                   .accepts_interaction_commands = true,
                                   .accepts_registered_actions = true}});
                    REQUIRE(state);
                    CHECK(
                        ec::resolve_invoke(
                            *state, {.affordance_id = "menu:tab", .args = {.row = "tab:nothing"}})
                            .error()
                        == ec::error::invalid_arguments);
                    return invoke_event(top, "menu:tab", {.row = "credits"});
                }
                case 1: {
                    const auto [banner, tabs, content] = parts_of(top);
                    CHECK(tabs.tabs[7].selected);
                    CHECK(std::ranges::count(tabs.tabs, true, &ec::tab_entry::selected) == 1);
                    // The credits are text, so there are no entries to pick.
                    CHECK_FALSE(body<ec::text_body>(content).markup.empty());
                    CHECK(find(top, "menu:entry") == nullptr);
                    return invoke_event(top, "menu:tab", {.row = "quit"});
                }
                case 2: {
                    // The quit tab asks before it leaves.
                    CHECK(top.context == "YESNO");
                    return run_action("YES");
                }
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the main menu's editor tab offers the map editor",
    "[client][interaction][mcp][screens][main_menu]") {
    clear_all_state();
    const auto restore = menu_restore{};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] { main_menu{}.opening_screen(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            switch (stage++) {
                case 0:
                    return invoke_event(top, "menu:tab", {.row = "editor"});
                case 1: {
                    const auto [banner, tabs, content] = parts_of(top);
                    const auto shown = std::ranges::find(tabs.tabs, true, &ec::tab_entry::selected);
                    REQUIRE(shown != tabs.tabs.end());
                    CHECK(shown->id == "editor");
                    CHECK(shown->label == "Editor");
                    const auto& rows = body<ec::list_body>(content).rows;
                    REQUIRE(rows.size() == 3);
                    CHECK(rows[0].id == "editor:map");
                    CHECK(rows[0].label == "Map editor");
                    CHECK_FALSE(rows[0].description.empty());
                    CHECK(rows[1].id == "editor:vehicle");
                    CHECK(rows[1].label == "Vehicle editor");
                    CHECK_FALSE(rows[1].description.empty());
                    CHECK(rows[2].id == "editor:save");
                    CHECK(rows[2].label == "Load save");
                    CHECK_FALSE(rows[2].description.empty());
                    CHECK(find(top, "menu:entry") != nullptr);
                    return invoke_event(top, "menu:tab", {.row = "quit"});
                }
                case 2:
                    CHECK(top.context == "YESNO");
                    return run_action("YES");
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 3);
}

TEST_CASE(
    "the main menu is the same at any viewport size and in another language",
    "[client][interaction][mcp][screens][main_menu][engine_client_component]") {
    clear_all_state();
    const auto restore = menu_restore{};
    const auto guard = screen_guard{};
    const auto fixture = locale_fixture::korean_catalogue{};
    auto stage = 0;
    open_stack([&] { main_menu{}.opening_screen(); },
               [&](const auto& /*snapshot*/, const auto& top) {
                   if (stage++ == 0) {
                       const auto [narrow, wide] = at_both_sizes();
                       CHECK(narrow.dump() == wide.dump());
                       const auto english = top_json();
                       const auto korean = [&] {
                           const auto scope = localization::locale_scope(locale_fixture::korean);
                           return top_json();
                       }();
                       CHECK(without_words(english) == without_words(korean));
                       return invoke_event(top, "menu:tab", {.row = "quit"});
                   }
                   return run_action("YES");
               });
    CHECK(stage == 2);
}

TEST_CASE(
    "cancelling the editor's mod picker returns to the main menu and makes no world",
    "[client][interaction][mcp][screens][main_menu][editor]") {
    clear_all_state();
    REQUIRE_FALSE(world_generator->has_world("EDITOR"));
    const auto restore = menu_restore{};
    const auto guard = screen_guard{};
    auto stage = 0;
    open_stack(
        [&] { main_menu{}.opening_screen(); },
        [&](const auto& /*snapshot*/, const auto& top) {
            switch (stage++) {
                case 0:
                    return invoke_event(top, "menu:tab", {.row = "editor"});
                case 1:
                    return invoke_event(top, "menu:entry", {.row = "editor:map"});
                case 2: {
                    // The game's own mod list comes first, with the mods of the default list
                    // active.
                    CHECK(top.context == "MODMANAGER_DIALOG");
                    CHECK(ec::find_component(*top.root, "worldgen:mods:active:rows") != nullptr);
                    return run_action("QUIT");
                }
                case 3:
                    CHECK(top.context == "YESNO");
                    return run_action("NO");
                case 4: {
                    const auto [banner, tabs, content] = parts_of(top);
                    const auto shown = std::ranges::find(tabs.tabs, true, &ec::tab_entry::selected);
                    REQUIRE(shown != tabs.tabs.end());
                    CHECK(shown->id == "editor");
                    CHECK_FALSE(world_generator->has_world("EDITOR"));
                    return invoke_event(top, "menu:tab", {.row = "quit"});
                }
                case 5:
                    return run_action("YES");
                default:
                    FAIL("unexpected extra screen " << top.context);
            }
            return input_event{};
        });
    CHECK(stage == 6);
    CHECK_FALSE(world_generator->has_world("EDITOR"));
}

TEST_CASE(
    "the main menu's load save entry lists the saves of the worlds and leaves them alone",
    "[client][interaction][mcp][screens][main_menu][editor]") {
    clear_all_state();
    const auto restore = menu_restore{};
    const auto guard = screen_guard{};
    // The menu finds the saves on disk: a save of the player alone is enough to list.
    get_avatar().name = "Editor Mode Probe";
    get_avatar().set_save_id(get_avatar().name);
    const auto world_name = g->get_active_world()->info->world_name;
    const auto save = save_t::from_save_id(get_avatar().name);
    REQUIRE(g->get_active_world()->write_to_player_file(SAVE_EXTENSION, [](std::ostream& out) {
        g->serialize(out);
    }));
    const auto file =
        g->get_active_world()->info->folder_path() + "/" + save.base_path() + SAVE_EXTENSION;
    const auto cleanup = on_out_of_scope([&] { remove_file(file); });
    auto stage = 0;
    open_stack([&] { main_menu{}.opening_screen(); },
               [&](const auto& snapshot, const auto& top) {
                   switch (stage++) {
                       case 0:
                           return invoke_event(top, "menu:tab", {.row = "editor"});
                       case 1:
                           return invoke_event(top, "menu:entry", {.row = "editor:save"});
                       case 2: {
                           const auto saved =
                               std::ranges::find_if(snapshot.choices, [&](const auto& choice) {
                                   return choice.label == world_name + ": Editor Mode Probe";
                               });
                           CHECK(saved != snapshot.choices.end());
                           return cancel(snapshot);
                       }
                       case 3:
                           return invoke_event(top, "menu:tab", {.row = "quit"});
                       case 4:
                           return run_action("YES");
                       default:
                           FAIL("unexpected extra screen " << top.context);
                   }
                   return input_event{};
               });
    CHECK(stage == 5);
    CHECK_FALSE(g->has_gametype());
}

TEST_CASE(
    "a save opened in editor mode gets the editor on top and the avatar stays where it was",
    "[client][interaction][mcp][screens][editor]") {
    clear_all_state();
    const auto guard = screen_guard{};
    editor::reset_editor_session();
    get_avatar().unset_mutation(trait_id("DEBUG_EDITOR"));
    const auto expected = get_avatar().abs_pos();
    auto& mode = g.get()->*native_member(mode_member{});
    mode = std::make_unique<editor_save_game>();
    const auto cleanup = on_out_of_scope([&] {
        mode.reset();
        get_avatar().unset_mutation(trait_id("DEBUG_EDITOR"));
        editor::reset_editor_session();
    });
    CHECK_FALSE(editor::editor_denial().empty());
    REQUIRE(mode->init());
    CHECK(editor::editor_denial().empty());
    CHECK_FALSE(g->has_gametype());

    auto kinds = std::vector<std::string>{};
    game_client::memory::set_input_provider([&](const int /*timeout*/) {
        kinds.push_back(top_screen_now().kind);
        return run_action("QUIT");
    });
    (g.get()->*native_member(handle_action_member{}))();
    REQUIRE(kinds.size() == 1);
    CHECK(kinds.front() == "editor");
    CHECK(get_avatar().abs_pos() == expected);
    // It opens the editor once, then asks for inputs as any game does.
    CHECK_FALSE(mode->next_action());
}

#endif // CATA_MCP
