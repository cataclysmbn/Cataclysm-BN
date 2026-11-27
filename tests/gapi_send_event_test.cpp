#include "avatar.h"
#include "cata_utility.h"
#include "catalua_impl.h"
#include "catch/catch.hpp"
#include "character_id.h"
#include "event.h"
#include "event_bus.h"
#include "init.h"
#include "state_helpers.h"
#include "type_id.h"

#include <vector>

namespace {
/// Records every event the bus dispatches while subscribed, so the test can
/// inspect what `gapi.send_event` sent: event type, time and payload fields.
class event_recorder: public event_subscriber {
public:
    void notify(const cata::event& e) override { events.push_back(e); }

    std::vector<cata::event> events;
};
} // namespace

/// Tests for the `gapi.send_event` Lua binding (`src/catalua_bindings_game.cpp`)
/// which dispatches game events by name so JSON statistics and achievements keep
/// working when the triggering logic is implemented in Lua (the Mycus/Marloss
/// port fires `consumes_marloss_item`, `crosses_marloss_threshold` and
/// `crosses_mycus_threshold` this way). An event_recorder subscribes to the
/// game's event bus, the Lua script sends those three events in every accepted
/// payload form, and the test verifies the C++ events arrive with the right
/// type and field values; the pcall assertions pin the error cases (unknown
/// event name, missing or unknown fields must raise Lua errors).
TEST_CASE("gapi.send_event dispatches game events by name", "[lua][event]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    event_recorder recorder;
    get_event_bus().subscribe(&recorder);

    auto lua = make_lua_state();
    lua["test_avatar"] = &get_avatar();
    const auto result = lua.safe_script(
        R"(
        assert( type( test_avatar ) == "userdata", "avatar type is " .. type( test_avatar ) )
        -- Table payload, and the bare-Character shorthand.
        gapi.send_event( "crosses_marloss_threshold", test_avatar )
        gapi.send_event( "crosses_mycus_threshold", { character = test_avatar } )
        gapi.send_event( "consumes_marloss_item",
                         { character = test_avatar, itype = "marloss_berry" } )
        -- A character_id also identifies the character.
        gapi.send_event( "crosses_marloss_threshold", test_avatar:getID() )

        -- Invalid uses must raise Lua errors and send nothing.
        assert( not pcall( gapi.send_event, "not_an_event", test_avatar ) )
        assert( not pcall( gapi.send_event, "consumes_marloss_item", test_avatar ) )
        assert( not pcall( gapi.send_event, "consumes_marloss_item",
                           { character = test_avatar } ) )
        assert( not pcall( gapi.send_event, "consumes_marloss_item",
                           { character = test_avatar, itype = "marloss_berry", bogus = true } ) )
    )",
        sol::script_pass_on_error);
    // Surface the script's own assert() text, which names the step that broke.
    if (!result.valid()) { FAIL(result.get<sol::error>().what()); }

    const auto avatar_id = get_avatar().getID();
    REQUIRE(recorder.events.size() == 4);
    CHECK(recorder.events[0].type() == event_type::crosses_marloss_threshold);
    CHECK(recorder.events[0].get<character_id>("character") == avatar_id);
    CHECK(recorder.events[1].type() == event_type::crosses_mycus_threshold);
    CHECK(recorder.events[1].get<character_id>("character") == avatar_id);
    CHECK(recorder.events[2].type() == event_type::consumes_marloss_item);
    CHECK(recorder.events[2].get<character_id>("character") == avatar_id);
    CHECK(recorder.events[2].get<itype_id>("itype") == itype_id("marloss_berry"));
    CHECK(recorder.events[3].type() == event_type::crosses_marloss_threshold);
    CHECK(recorder.events[3].get<character_id>("character") == avatar_id);
}
