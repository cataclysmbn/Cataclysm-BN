#include "avatar.h"
#include "calendar.h"
#include "cata_utility.h"
#include "catalua_impl.h"
#include "catch/catch.hpp"
#include "character_id.h"
#include "event.h"
#include "event_bus.h"
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

/// Tests the rest of the payload rules of `gapi.send_event`: events without
/// fields (nil payload), `bool`/`int` fields, numeric character ids, and every
/// check that must raise a Lua error before anything is sent (invalid ids,
/// non-integer numbers, events reserved for the game). A recorder on the event
/// bus counts what was actually sent, so a failed call must add nothing.
TEST_CASE("gapi.send_event checks payload values before sending", "[lua][event]") {
    clear_all_state();
    const auto cleanup = on_out_of_scope([]() { clear_all_state(); });

    event_recorder recorder;
    get_event_bus().subscribe(&recorder);

    auto lua = make_lua_state();
    lua["test_avatar"] = &get_avatar();
    lua["test_avatar_id"] = get_avatar().getID().get_value();
    const auto result = lua.safe_script(
        R"(
        -- An event without fields takes no payload.
        gapi.send_event( "opens_portal" )
        -- bool and int fields, plus enum and id fields as strings.
        gapi.send_event( "avatar_moves", {
            mount = "mon_zombie",
            terrain = "t_dirt",
            movement_mode = "run",
            underwater = true,
            z = -2,
        } )
        -- A plain integer character id.
        gapi.send_event( "crosses_marloss_threshold", { character = test_avatar_id } )

        -- Each failing call must raise an error with the expected reason, so
        -- the check cannot pass for an unrelated error.
        local function fails_with( reason, ... )
            local ok, err = pcall( gapi.send_event, ... )
            assert( not ok, reason .. ": call did not fail" )
            assert( string.find( tostring( err ), reason, 1, true ),
                    reason .. ": wrong error: " .. tostring( err ) )
        end
        local function moves( fields )
            local payload = { mount = "mon_zombie", terrain = "t_dirt",
                              movement_mode = "run", underwater = true, z = 0 }
            for k, v in pairs( fields ) do payload[k] = v end
            return payload
        end
        -- Events with fields need them.
        fails_with( "requires the fields", "avatar_moves" )
        -- Ids and enum values must exist.
        fails_with( "invalid value 'not_an_item'", "consumes_marloss_item",
                    { character = test_avatar, itype = "not_an_item" } )
        fails_with( "invalid value 'fly'", "avatar_moves", moves( { movement_mode = "fly" } ) )
        -- Character ids must be integers.
        fails_with( "unsupported Lua value type", "crosses_marloss_threshold", { character = "abc" } )
        fails_with( "unsupported Lua value type", "crosses_marloss_threshold", { character = 1.5 } )
        -- int fields reject fractions, bool fields reject numbers.
        fails_with( "unsupported Lua value type", "avatar_moves", moves( { z = 0.5 } ) )
        fails_with( "unsupported Lua value type", "avatar_moves", moves( { underwater = 1 } ) )
        -- Only the game sends these.
        fails_with( "sent by the game", "game_start", { avatar_id = test_avatar } )
        fails_with( "sent by the game", "game_over", { is_suicide = false, last_words = "" } )
    )",
        sol::script_pass_on_error);
    // Surface the script's own assert() text, which names the step that broke.
    if (!result.valid()) { FAIL(result.get<sol::error>().what()); }

    REQUIRE(recorder.events.size() == 3);
    CHECK(recorder.events[0].type() == event_type::opens_portal);
    CHECK(recorder.events[0].time() == calendar::turn);
    CHECK(recorder.events[1].type() == event_type::avatar_moves);
    CHECK(recorder.events[1].get<bool>("underwater"));
    CHECK(recorder.events[1].get<int>("z") == -2);
    CHECK(
        recorder.events[1].get<character_movemode>("movement_mode") == character_movemode::CMM_RUN);
    CHECK(recorder.events[2].type() == event_type::crosses_marloss_threshold);
    CHECK(recorder.events[2].get<character_id>("character") == get_avatar().getID());
}
