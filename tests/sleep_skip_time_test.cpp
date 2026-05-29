#include "catch/catch.hpp"

#include "avatar.h"
#include "calendar.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "mtype.h"
#include "options_helpers.h"
#include "point.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle.h"
#include "vehicle_part.h"

static const efftype_id effect_sleep( "sleep" );
static const itype_id fuel_type_battery( "battery" );
static const mtype_id mon_zombie( "mon_zombie" );

namespace
{

auto run_sleep_turn( const time_duration &duration ) -> time_duration
{
    avatar &you = g->u;
    const time_point before_sleep = calendar::turn;
    you.fall_asleep( duration );
    REQUIRE( you.has_effect( effect_sleep ) );
    CHECK_FALSE( g->do_turn() );
    return calendar::turn - before_sleep;
}

}

TEST_CASE( "sleep skip time advances to wake up when safe", "[sleep][perf]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    clear_creatures();
    override_option sleep_skip_time( "SLEEP_SKIP_TIME", "true" );
    g->u.remove_value( "sleep_skip_time_disabled" );
    calendar::turn = calendar::turn_zero + 1_days;

    const time_duration elapsed = run_sleep_turn( 2_hours );

    CHECK( elapsed >= 2_hours - 1_turns );
    CHECK_FALSE( g->u.has_effect( effect_sleep ) );
}

TEST_CASE( "sleep skip time falls back when hostiles are nearby", "[sleep][perf]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    clear_creatures();
    override_option sleep_skip_time( "SLEEP_SKIP_TIME", "true" );
    g->u.remove_value( "sleep_skip_time_disabled" );
    calendar::turn = calendar::turn_zero + 1_days;

    avatar &you = g->u;
    REQUIRE( g->place_critter_at( mon_zombie, you.pos() + tripoint_east ) != nullptr );

    const time_duration elapsed = run_sleep_turn( 2_hours );

    CHECK( elapsed < 2_hours );
    CHECK( elapsed >= 1_turns );
}

TEST_CASE( "sleep skip time processes rotting and charging effects", "[sleep][perf]" )
{
    clear_all_state();
    build_test_map( ter_id( "t_pavement" ) );
    clear_creatures();
    override_option sleep_skip_time( "SLEEP_SKIP_TIME", "true" );
    g->u.remove_value( "sleep_skip_time_disabled" );
    calendar::turn = calendar::turn_zero + 2_days;

    map &here = get_map();
    const tripoint item_pos = g->u.pos();
    detached_ptr<item> rotting_item = item::spawn( "meat_cooked" );
    rotting_item->mod_rot( 7_days );
    here.add_item_or_charges( item_pos, std::move( rotting_item ), false );

    const tripoint vehicle_origin = tripoint( 10, 10, 0 );
    vehicle *veh_ptr = here.add_vehicle( vproto_id( "recharge_test" ), vehicle_origin, 0_degrees, 100,
                                         0 );
    REQUIRE( veh_ptr != nullptr );
    auto cargo_part_index = veh_ptr->part_with_feature( point_zero, "CARGO", true );
    REQUIRE( cargo_part_index >= 0 );

    auto chargers = veh_ptr->get_parts_at( vehicle_origin, "RECHARGE", part_status_flag::available );
    REQUIRE( chargers.size() == 1 );
    chargers.front()->enabled = true;

    detached_ptr<item> battery_item = item::spawn( "light_battery_cell" );
    battery_item->ammo_unset();
    REQUIRE( battery_item->has_flag( flag_RECHARGE ) );
    veh_ptr->add_item( veh_ptr->part( cargo_part_index ), std::move( battery_item ) );

    run_sleep_turn( 1_hours );

    CHECK( here.i_at( item_pos ).empty() );
    auto cargo_items = veh_ptr->get_items( cargo_part_index );
    REQUIRE( cargo_items.size() == 1 );
    CHECK( cargo_items.only_item().ammo_remaining() > 0 );
    CHECK( veh_ptr->fuel_left( fuel_type_battery ) > 0 );
}
