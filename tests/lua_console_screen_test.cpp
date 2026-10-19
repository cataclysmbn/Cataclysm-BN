#if defined(CATA_MCP)

#    include "catalua_console.h"
#    include "catalua_log.h"
#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "engine_client_component.h"
#    include "state_helpers.h"

#    include <string>
#    include <variant>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;

auto text_of(const ec::screen_view& screen, const std::string& id) -> const ec::text_body& {
    REQUIRE(screen.root);
    const auto* const found = ec::find_component(*screen.root, id);
    INFO(id);
    REQUIRE(found != nullptr);
    REQUIRE(std::holds_alternative<ec::text_body>(found->body));
    return std::get<ec::text_body>(found->body);
}

} // namespace

TEST_CASE(
    "the Lua console shows its log oldest first, as a terminal, and the input it holds",
    "[lua_console_screen][engine_client_component]") {
    clear_all_state();
    cata::get_lua_log_instance().clear();
    cata::get_lua_log_instance().add(cata::LuaLogLevel::Input, "print('hi')");
    cata::get_lua_log_instance().add(cata::LuaLogLevel::Error, "boom");
    const auto guard = screen_guard{};
    auto step = 0;
    open_stack([] { cata::show_lua_console_impl(); },
               [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
                   if (step++ == 0) {
                       CHECK(screen.kind == "lua_console");
                       const auto& log = text_of(screen, "lua_console:log");
                       CHECK(log.role == "terminal_output");
                       const auto input = log.markup.find("> print('hi')");
                       const auto error = log.markup.find("<color_c_red>boom</color>");
                       REQUIRE(input != std::string::npos);
                       REQUIRE(error != std::string::npos);
                       CHECK(input < error);
                       CHECK(text_of(screen, "lua_console:input").markup.empty());
                   }
                   return run_action("QUIT");
               });
    CHECK(step == 1);
    cata::get_lua_log_instance().clear();
}

#endif // CATA_MCP
