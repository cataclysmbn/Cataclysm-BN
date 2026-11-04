#if defined(CATA_MCP)

#    include "catch/catch.hpp"
#    include "client_screen_support.h"
#    include "editmap.h"
#    include "engine_client_component.h"
#    include "map_helpers.h"
#    include "state_helpers.h"

#    include <string>

namespace {
namespace ec = engine_client;
using namespace client_screen_test;
} // namespace

TEST_CASE(
    "the map editor stays one screen while its cursor moves",
    "[editmap_screen][engine_client_component]") {
    clear_all_state();
    build_test_map(ter_id("t_floor"));
    const auto guard = screen_guard{};
    auto step = 0;
    auto first = std::string{};
    open_stack([] { editmap{}.edit(); },
               [&](const snapshot_t& /*snapshot*/, const ec::screen_view& screen) {
                   switch (step++) {
                       case 0:
                           CHECK(screen.kind == "editmap");
                           first = screen.screen_id;
                           return run_action("RIGHT");
                       case 1:
                           // A key queued against the screen still lands after the cursor moved.
                           CHECK(screen.screen_id == first);
                           return run_action("QUIT");
                       default:
                           FAIL("the editor outlived its quit");
                           return run_action("QUIT");
                   }
               });
    CHECK(step == 2);
}

#endif // CATA_MCP
