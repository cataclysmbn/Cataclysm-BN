#include "../src/calendar.h"
#include "../src/cata_utility.h"
#include "../src/item.h"
#include "../src/map/mapbuffer.h"
#include "../src/map/mapbuffer_registry.h"
#include "../src/map/submap.h"
#include "../src/options.h"
#include "../src/type_id.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "state_helpers.h"

#include <memory>

TEST_CASE( "plants advance growth while their submap is simulated", "[map][plant]" )
{
    clear_all_state();
    const auto cleanup = on_out_of_scope( [] { clear_all_state(); } );

    // 1% scaling: a 10 day crop (seed_tomato) has an epoch of 48 minutes per stage.
    get_options().get_option( "GROWTH_SCALING" ).setValue( 1 );
    // set_age clamps birthdays to turn_zero, so advance the clock first.
    calendar::turn += 2_days;

    auto &buffer = MAPBUFFER_REGISTRY.get( mapbuffer_registry::primary_dimension_id() );
    const auto pos = tripoint_abs_sm( 100, 100, 0 );
    auto sm = std::make_unique<submap>( pos, buffer.get_dimension_id() );
    sm->set_all_ter( ter_id( "t_dirt" ) );
    const auto tile = point_sm_ms( 3, 3 );
    sm->set_furn( tile, furn_id( "f_plant_seed" ) );
    auto seed = item::spawn( "seed_tomato" );
    seed->set_age( 4_hours );
    sm->get_items( tile ).push_back( std::move( seed ) );
    auto *sm_ptr = sm.get();
    REQUIRE( buffer.add_submap( pos, sm ) );

    // Each call advances at most one stage, same as grow_plant on actualization.
    buffer.grow_plants_in_submap( pos );
    CHECK( sm_ptr->get_furn( tile ) == furn_id( "f_plant_seedling" ) );

    buffer.grow_plants_in_submap( pos );
    CHECK( sm_ptr->get_furn( tile ) == furn_id( "f_plant_mature" ) );

    buffer.grow_plants_in_submap( pos );
    CHECK( sm_ptr->get_furn( tile ) == furn_id( "f_plant_harvest" ) );

    // Harvested plants do not advance further.
    buffer.grow_plants_in_submap( pos );
    CHECK( sm_ptr->get_furn( tile ) == furn_id( "f_plant_harvest" ) );
}
