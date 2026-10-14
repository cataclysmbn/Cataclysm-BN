#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "locale_fixture.h"
#    include "locale_scope.h"
#    include "main_menu.h"
#    include "state_helpers.h"
#    include "worldfactory.h"

#    include <algorithm>
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

const auto tab_ids = std::vector<
    std::string>{"motd", "new_game", "load", "world", "settings", "help", "credits", "quit"};

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
                    CHECK(tabs.tabs[6].selected);
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

#endif // CATA_MCP
