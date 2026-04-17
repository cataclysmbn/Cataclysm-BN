#include "catch/catch.hpp"

#include <array>

#include "damage.h"
#include "type_id.h"
#include "state_helpers.h"
#include "veh_type.h"

TEST_CASE( "verify_copy_from_gets_damage_reduction", "[vehicle]" )
{
    clear_all_state();
    // Picking halfboard_horizontal as a vpart which is likely to remain
    // defined via copy-from, and which should have non-zero damage reduction.
    const vpart_info &vp = vpart_id( "halfboard_horizontal" ).obj();
    CHECK( vp.damage_reduction.type_resist( DT_BASH ) != 0 );
}

TEST_CASE( "vehicle_part_utility_slots_are_not_anywhere", "[vehicle]" )
{
    clear_all_state();

    CHECK( vpart_id( "folding_seat" ).obj().location == "center" );
    CHECK( vpart_id( "bike_rack" ).obj().location == "on_roof" );
    CHECK( vpart_id( "water_purifier" ).obj().location == "on_ceiling" );
    CHECK( vpart_id( "NBC_seal" ).obj().location == "on_ceiling" );
    CHECK( vpart_id( "airship_balloon" ).obj().location == "on_roof" );
}
