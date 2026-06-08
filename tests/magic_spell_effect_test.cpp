#include "catch/catch.hpp"

#include <sstream>

#include "json.h"
#include "magic.h"
#include "magic_spell_effect_helpers.h"
#include "npc.h"
#include "player_helpers.h"
#include "state_helpers.h"

TEST_CASE( "line_attack", "[magic]" )
{
    clear_all_state();
    // manually construct a testable spell
    std::istringstream str(
        "  {\n"
        "    \"id\": \"test_line_spell\",\n"
        "    \"name\": { \"str\": \"Test Line Spell\" },\n"
        "    \"description\": \"Spews a line of magic\",\n"
        "    \"valid_targets\": [ \"ground\" ],\n"
        "    \"damage_type\": \"true\",\n"
        "    \"min_range\": 5,\n"
        "    \"max_range\": 5,\n"
        "    \"effect\": \"line_attack\",\n"
        "    \"min_aoe\": 0,\n"
        "    \"max_aoe\": 0,\n"
        "    \"flags\": [ \"VERBAL\", \"NO_HANDS\", \"NO_LEGS\" ]\n"
        "  }\n" );

    JsonIn in( str );
    JsonObject obj( in );
    spell_type::load_spell( obj, "" );

    spell sp( spell_id( "test_line_spell" ) );

    // set up Character to test with, only need position
    npc &c = spawn_npc( point_bub_ms::zero(), "test_talker" );
    clear_character( c );
    c.setpos( tripoint_bub_ms::zero() );

    // target point 5 tiles east of zero
    tripoint_bub_ms target = tripoint_bub_ms( 5, 0, 0 );

    // Ensure that AOE=0 spell covers the 5 tiles along vector towards target
    SECTION( "aoe=0" ) {
        const std::set<tripoint_bub_ms> reference( { tripoint_bub_ms::east(), tripoint_bub_ms( 2, 0, 0 ), tripoint_bub_ms( 3, 0, 0 ), tripoint_bub_ms( 4, 0, 0 ), tripoint_bub_ms( 5, 0, 0 ) } );

        std::set<tripoint_bub_ms> targets = calculate_spell_effect_area( sp, target,
                                            spell_effect::spell_effect_line, c, true );

        CHECK( reference == targets );
    }
}
