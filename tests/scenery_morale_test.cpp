#include "catch/catch.hpp"

#include "avatar.h"
#include "calendar.h"
#include "map_helpers.h"
#include "morale_types.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"

static const efftype_id effect_scenery_cooldown( "scenery_cooldown" );

TEST_CASE( "sunrise_morale_applied_outdoors", "[scenery_morale]" )
{
    clear_all_state();

    avatar &dummy = get_avatar();
    build_test_map( ter_id( "t_grass" ) );

    const time_point t_sunrise = sunrise( calendar::turn_zero );
    set_time( t_sunrise + 30_minutes );

    dummy.update_morale();
    CHECK( dummy.has_morale( MORALE_SUNRISE ) );
    CHECK( dummy.get_morale( MORALE_SUNRISE ) == 10 );
    CHECK( dummy.has_effect( effect_scenery_cooldown ) );

    // Cooldown should prevent re-triggering.
    dummy.clear_morale();
    dummy.update_morale();
    CHECK_FALSE( dummy.has_morale( MORALE_SUNRISE ) );
}

TEST_CASE( "sunset_morale_applied_outdoors", "[scenery_morale]" )
{
    clear_all_state();

    avatar &dummy = get_avatar();
    build_test_map( ter_id( "t_grass" ) );

    const time_point t_sunset = sunset( calendar::turn_zero );
    set_time( t_sunset + 30_minutes );

    dummy.update_morale();
    CHECK( dummy.has_morale( MORALE_SUNSET ) );
    CHECK( dummy.get_morale( MORALE_SUNSET ) == 10 );
    CHECK( dummy.has_effect( effect_scenery_cooldown ) );
}
