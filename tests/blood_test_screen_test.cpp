#if defined(CATA_MCP)

#    include "avatar.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "player_helpers.h"
#    include "state_helpers.h"

#    include <algorithm>
#    include <string>
#    include <variant>
#    include <vector>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;

auto lines_of(const ec::screen_view& screen) -> std::vector<ec::panel_stat> {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, "blood_test");
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::panel_body>(found->body));
    return std::get<ec::panel_body>(found->body).stats;
}

} // namespace

TEST_CASE(
    "blood test results list each effect the analysis finds and how good it is",
    "[blood_test_screen][engine_client_component]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto guard = screen_guard{};
    auto shown = std::vector<std::vector<ec::panel_stat>>{};
    const auto analyse = [&] {
        open_stack([&] { you.conduct_blood_analysis(); },
                   [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
                       CHECK(screen.kind == "blood_test");
                       CHECK(screen.title == "Blood Test Results");
                       shown.push_back(lines_of(screen));
                       return run_action("CONFIRM");
                   });
    };
    analyse();
    you.add_effect(efftype_id("stung"), 10_minutes);
    you.add_effect(efftype_id("antitoxin"), 10_minutes);
    analyse();
    REQUIRE(shown.size() == 2);
    REQUIRE(shown[0].size() == 1);
    CHECK(shown[0][0].key == "none");
    CHECK(shown[0][0].label == "No effects.");
    const auto& found = shown[1];
    const auto line = [&](const std::string& key) {
        const auto at = std::ranges::find(found, key, &ec::panel_stat::key);
        INFO(key);
        REQUIRE(at != found.end());
        return *at;
    };
    CHECK(found.size() == 2);
    CHECK(line("effect:stung").label == "Stung");
    CHECK(line("effect:stung").tone == "bad");
    CHECK(line("effect:antitoxin").label == "Antitoxin");
    CHECK(line("effect:antitoxin").tone == "good");
}

#endif // CATA_MCP
