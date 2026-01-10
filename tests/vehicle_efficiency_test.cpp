#include "catch/catch.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "enums.h"
#include "game.h"
#include "item.h"
#include "itype.h"
#include "line.h"
#include "map.h"
#include "map_helpers.h"
#include "point.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "string_formatter.h"
#include "test_statistics.h"
#include "type_id.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

using efficiency_stat = statistics<int>;

const efftype_id effect_blind( "blind" );

static void clear_game( const ter_id &terrain )
{
    // Set to turn 0 to prevent solars from producing power
    calendar::turn = calendar::turn_zero;
    clear_avatar();
    clear_creatures();
    clear_npcs();
    clear_vehicles();

    // Move player somewhere safe
    REQUIRE_FALSE( g->u.in_vehicle );
    g->u.setpos( tripoint_zero );
    // Blind the player to avoid needless drawing-related overhead
    g->u.add_effect( effect_blind, 365_days, bodypart_str_id::NULL_ID() );

    build_test_map( terrain );
}

// Returns how much fuel did it provide
// But contains only fuels actually used by engines
static std::map<itype_id, int> set_vehicle_fuel( vehicle &v, const float veh_fuel_mult )
{
    // First we need to find the fuels to set
    // That is, fuels actually used by some engine
    std::set<itype_id> actually_used;
    for( const vpart_reference vp : v.get_all_parts() ) {
        vehicle_part &pt = vp.part();
        if( pt.is_engine() ) {
            actually_used.insert( pt.info().fuel_type );
            pt.enabled = true;
        } else {
            // Disable all parts that use up power or electric cars become non-deterministic
            pt.enabled = false;
        }
    }

    // We ignore battery when setting fuel because it uses designated "tanks"
    actually_used.erase( itype_id( "battery" ) );

    // Currently only one liquid fuel supported
    REQUIRE( actually_used.size() <= 1 );
    itype_id liquid_fuel = itype_id::NULL_ID();
    for( const itype_id &ft : actually_used ) {
        if( ft->phase == LIQUID ) {
            liquid_fuel = ft;
            break;
        }
    }

    // Set fuel to a given percentage
    // Batteries are special cased because they aren't liquid fuel
    std::map<itype_id, int> ret;
    for( const vpart_reference vp : v.get_all_parts() ) {
        vehicle_part &pt = vp.part();

        if( pt.is_battery() ) {
            pt.ammo_set( itype_id( "battery" ), pt.ammo_capacity() * veh_fuel_mult );
            ret[ itype_id( "battery" ) ] += pt.ammo_capacity() * veh_fuel_mult;
        } else if( pt.is_tank() && !liquid_fuel.is_null() ) {
            float qty = pt.ammo_capacity() * veh_fuel_mult;
            qty *= std::max( liquid_fuel->stack_size, 1 );
            qty /= to_milliliter( units::legacy_volume_factor );
            pt.ammo_set( liquid_fuel, qty );
            ret[ liquid_fuel ] += qty;
        } else {
            pt.ammo_unset();
        }
    }

    // We re-add battery because we want it accounted for, just not in the section above
    actually_used.insert( itype_id( "battery" ) );
    for( auto iter = ret.begin(); iter != ret.end(); ) {
        if( iter->second <= 0 || !actually_used.contains( iter->first ) ) {
            iter = ret.erase( iter );
        } else {
            ++iter;
        }
    }
    return ret;
}

// Returns the lowest percentage of fuel left
// i.e. 1 means no fuel was used, 0 means at least one dry tank
static float fuel_percentage_left( vehicle &v, const std::map<itype_id, int> &started_with )
{
    std::map<itype_id, int> fuel_amount;
    std::set<itype_id> consumed_fuels;
    for( const vpart_reference vp : v.get_all_parts() ) {
        vehicle_part &pt = vp.part();

        if( ( pt.is_battery() || pt.is_reactor() || pt.is_tank() ) &&
            !pt.ammo_current().is_null() ) {
            fuel_amount[ pt.ammo_current() ] += pt.ammo_remaining();
        }

        if( pt.is_engine() && !pt.info().fuel_type.is_null() ) {
            consumed_fuels.insert( pt.info().fuel_type );
        }
    }

    float left = 1.0f;
    for( const auto &type : consumed_fuels ) {
        const auto iter = started_with.find( type );
        // Weird - we started without this fuel
        float fuel_amt_at_start = iter != started_with.end() ? iter->second : 0.0f;
        REQUIRE( fuel_amt_at_start != 0.0f );
        left = std::min( left, static_cast<float>( fuel_amount[type] ) / fuel_amt_at_start );
    }

    return left;
}

const float fuel_level = 0.1f;
const int cycle_limit = 100;

// Algorithm goes as follows:
// Clear map
// Spawn a vehicle
// Set its fuel up to some percentage - remember exact fuel counts that were set here
// Drive it for a while, always moving it back to start point every turn to avoid it going off the bubble
// When moving back, record the sum of the tiles moved so far
// Repeat that for a set number of turns or until all fuel is drained
// Compare saved percentage (set before) to current percentage
// Rescale the recorded number of tiles based on fuel percentage left
// (i.e. 0% fuel left means no scaling, 50% fuel left means double the effective distance)
// Return the rescaled number
static int test_efficiency( const vproto_id &veh_id, int &expected_mass,
                            const ter_id &terrain,
                            const int reset_velocity_turn, const int target_distance,
                            const bool smooth_stops = false, const bool test_mass = true,
                            const bool in_reverse = false )
{
    int min_dist = target_distance * 0.99;
    int max_dist = target_distance * 1.01;
    clear_game( terrain );

    const tripoint map_starting_point( 60, 60, 0 );
    map &here = get_map();
    vehicle *veh_ptr = here.add_vehicle( veh_id, map_starting_point, -90_degrees, 0, 0 );

    REQUIRE( veh_ptr != nullptr );
    if( veh_ptr == nullptr ) {
        return 0;
    }

    vehicle &veh = *veh_ptr;

    // Remove all items from cargo to normalize weight.
    for( const vpart_reference vp : veh.get_all_parts() ) {
        veh_ptr->get_items( vp.part_index() ).clear();
        vp.part().ammo_consume( vp.part().ammo_remaining(), vp.pos() );
    }
    for( const vpart_reference vp : veh.get_avail_parts( "OPENABLE" ) ) {
        veh.close( vp.part_index() );
    }

    veh.refresh_insides();

    if( test_mass ) {
        CHECK( to_gram( veh.total_mass() ) == expected_mass );
    }
    expected_mass = to_gram( veh.total_mass() );
    veh.check_falling_or_floating();
    REQUIRE( !veh.is_in_water() );
    const auto &starting_fuel = set_vehicle_fuel( veh, fuel_level );
    // This is ugly, but improves accuracy: compare the result of fuel approx function
    // rather than the amount of fuel we actually requested
    const float starting_fuel_per = fuel_percentage_left( veh, starting_fuel );
    REQUIRE( std::abs( starting_fuel_per - 1.0f ) < 0.001f );

    const tripoint starting_point = veh.global_pos3();
    veh.tags.insert( "IN_CONTROL_OVERRIDE" );
    veh.engine_on = true;

    const int sign = in_reverse ? -1 : 1;
    const int target_velocity = sign * std::min( 50 * 100, veh.safe_ground_velocity( false ) );
    veh.cruise_velocity = target_velocity;
    // If we aren't testing repeated cold starts, start the vehicle at cruising velocity.
    // Otherwise changing the amount of fuel in the tank perturbs the test results.
    if( reset_velocity_turn == -1 ) {
        veh.velocity = target_velocity;
    }
    int reset_counter = 0;
    int tiles_travelled = 0;
    int cycles_left = cycle_limit;
    bool accelerating = true;
    CHECK( veh.safe_velocity() > 0 );
    while( veh.engine_on && veh.safe_velocity() > 0 && cycles_left > 0 ) {
        cycles_left--;
        here.vehmove();
        veh.idle( true );
        // If the vehicle starts skidding, the effects become random and test is RUINED
        REQUIRE( !veh.skidding );
        for( const tripoint &pos : veh.get_points() ) {
            REQUIRE( here.ter( pos ) );
        }
        // How much it moved
        tiles_travelled += square_dist( starting_point, veh.global_pos3() );
        // Bring it back to starting point to prevent it from leaving the map
        const tripoint displacement = starting_point - veh.global_pos3();
        here.displace_vehicle( veh, displacement );
        if( reset_velocity_turn < 0 ) {
            continue;
        }

        reset_counter++;
        if( reset_counter > reset_velocity_turn ) {
            if( smooth_stops ) {
                accelerating = !accelerating;
                veh.cruise_velocity = accelerating ? target_velocity : 0;
            } else {
                veh.velocity = 0;
                veh.last_turn = 0_degrees;
                veh.of_turn_carry = 0;
            }
            reset_counter = 0;
        }
    }

    float fuel_left = fuel_percentage_left( veh, starting_fuel );
    REQUIRE( starting_fuel_per - fuel_left > 0.0001f );
    const float fuel_percentage_used = fuel_level * ( starting_fuel_per - fuel_left );
    int adjusted_tiles_travelled = tiles_travelled / fuel_percentage_used;
    if( target_distance >= 0 ) {
        INFO( veh.name );
        CHECK( adjusted_tiles_travelled >= min_dist );
        CHECK( adjusted_tiles_travelled <= max_dist );
    }

    return adjusted_tiles_travelled;
}

static efficiency_stat find_inner(
    const std::string &type, int &expected_mass, const std::string &terrain, const int delay,
    const bool smooth, const bool test_mass = false, const bool in_reverse = false )
{
    efficiency_stat efficiency;
    for( int i = 0; i < 10; i++ ) {
        efficiency.add( test_efficiency( vproto_id( type ), expected_mass, ter_id( terrain ),
                                         delay, -1, smooth, test_mass, in_reverse ) );
    }
    return efficiency;
}

static void print_stats( const efficiency_stat &st )
{
    if( st.min() == st.max() ) {
        cata_printf( "All results %d.\n", st.min() );
    } else {
        cata_printf( "Min %d, Max %d, Midpoint %f.\n", st.min(), st.max(),
                     ( st.min() + st.max() ) / 2.0 );
    }
}

static void print_efficiency(
    const std::string &type, int expected_mass, const std::string &terrain, const int delay,
    const bool smooth )
{
    cata_printf( "Testing %s on %s with %s: ",
                 type.c_str(), terrain.c_str(), ( delay < 0 ) ? "no resets" : "resets every 5 turns" );
    print_stats( find_inner( type, expected_mass, terrain, delay, smooth ) );
}

static void find_efficiency( const std::string &type )
{
    SECTION( "finding efficiency of " + type ) {
        print_efficiency( type, 0,  "t_pavement", -1, false );
        print_efficiency( type, 0, "t_dirt", -1, false );
        print_efficiency( type, 0, "t_pavement", 5, false );
        print_efficiency( type, 0, "t_dirt", 5, false );
    }
}

static int avg_from_stat( const efficiency_stat &st )
{
    const int ugly_integer = ( st.min() + st.max() ) / 2.0;
    // Round to 4 most significant places
    const int magnitude = std::max<int>( 0, std::floor( std::log10( ugly_integer ) ) );
    const int precision = std::max<int>( 1, std::round( std::pow( 10.0, magnitude - 3 ) ) );
    return ugly_integer - ugly_integer % precision;
}

static int print_test_strings( const std::string &type, bool in_reverse = false )
{
    int expected_mass = 0;
    int v1 = avg_from_stat( find_inner( type, expected_mass, "t_pavement", -1, false, false,
                                        in_reverse ) );
    int expm = expected_mass;
    int v2 = avg_from_stat( find_inner( type, expected_mass, "t_dirt", -1, false, false,
                                        in_reverse ) );
    int v3 = avg_from_stat( find_inner( type, expected_mass, "t_pavement", 5, false, false,
                                        in_reverse ) );
    int v4 = avg_from_stat( find_inner( type, expected_mass, "t_dirt", 5, false, false,
                                        in_reverse ) );

    cata_printf(
        "    test_vehicle( \"%s\", %d, %d, %d, %d, %d%s );\n",
        type, expm, v1, v2, v3, v4, in_reverse ? ", 0, 0, true" : ""
    );

    return v1;
}

static void test_vehicle(
    std::string type, int expected_mass,
    const int pavement_target, const int dirt_target,
    const int pavement_target_w_stops, const int dirt_target_w_stops,
    const int pavement_target_smooth_stops = 0, const int dirt_target_smooth_stops = 0,
    const bool in_reverse = false )
{
    SECTION( type + " on pavement" ) {
        test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_pavement" ), -1,
                         pavement_target, false, true, in_reverse );
    }
    SECTION( type + " on dirt" ) {
        test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_dirt" ), -1,
                         dirt_target, false, true, in_reverse );
    }
    SECTION( type + " on pavement, full stop every 5 turns" ) {
        test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_pavement" ), 5,
                         pavement_target_w_stops, false, true, in_reverse );
    }
    SECTION( type + " on dirt, full stop every 5 turns" ) {
        test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_dirt" ), 5,
                         dirt_target_w_stops, false, true, in_reverse );
    }
    if( pavement_target_smooth_stops > 0 ) {
        SECTION( type + " on pavement, alternating 5 turns of acceleration and 5 turns of decceleration" ) {
            test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_pavement" ), 5,
                             pavement_target_smooth_stops, true, true, in_reverse );
        }
    }
    if( dirt_target_smooth_stops > 0 ) {
        SECTION( type + " on dirt, alternating 5 turns of acceleration and 5 turns of decceleration" ) {
            test_efficiency( vproto_id( type ), expected_mass, ter_id( "t_dirt" ), 5,
                             dirt_target_smooth_stops, true, true, in_reverse );
        }
    }
}

std::vector<std::string> vehs_to_test = {{
        "beetle",
        "car",
        "car_sports",
        "electric_car",
        "suv",
        "motorcycle",
        "quad_bike",
        "scooter",
        "superbike",
        "ambulance",
        "fire_engine",
        "fire_truck",
        "truck_swat",
        "tractor_plow",
        "apc",
        "humvee",
        "road_roller",
        "golf_cart"
    }
};

/** This isn't a test per se, it executes this code to
 * determine the current state of vehicle efficiency.
 **/
TEST_CASE( "vehicle_find_efficiency", "[.]" )
{
    clear_all_state();
    for( const std::string &veh : vehs_to_test ) {
        find_efficiency( veh );
    }
}

/** This is even less of a test. It generates C++ lines for the actual test below */
TEST_CASE( "make_vehicle_efficiency_case", "[.]" )
{
    clear_all_state();
    const float acceptable = 1.25;
    std::map<std::string, int> forward_distance;
    for( const std::string &veh : vehs_to_test ) {
        const int in_forward = print_test_strings( veh );
        forward_distance[ veh ] = in_forward;
    }
    printf( "\n    // in reverse\n" );
    for( const std::string &veh : vehs_to_test ) {
        const int in_reverse = print_test_strings( veh, true );
        CHECK( in_reverse < ( acceptable * forward_distance[ veh ] ) );
    }
}

// TODO:
// Amount of fuel needed to reach safe speed.
// Amount of cruising range for a fixed amount of fuel.
// Fix test for electric vehicles
TEST_CASE( "vehicle_efficiency", "[vehicle] [engine]" )
{
    clear_all_state();
    test_vehicle( "beetle", 818837, 431300, 338700, 95610, 68060 );
    test_vehicle( "car", 1125629, 617500, 386100, 52730, 25170 );
    test_vehicle( "car_sports", 1157382, 352600, 267600, 36790, 22350 );
    test_vehicle( "electric_car", 879098, 183880, 127125, 13410, 8705 );
    test_vehicle( "suv", 1325297, 1163000, 614130, 85540, 32000 );
    test_vehicle( "motorcycle", 163085, 120300, 99930, 63320, 50810 );
    test_vehicle( "quad_bike", 264465, 116100, 116100, 46770, 46770 );
    test_vehicle( "scooter", 57587, 233500, 233500, 167900, 167900 );
    test_vehicle( "superbike", 244085, 109800, 65300, 41780, 24070 );
    test_vehicle( "ambulance", 1854071, 613400, 504700, 77700, 57139 );
    test_vehicle( "fire_engine", 2257115, 1938615, 1819475, 394660, 363895 );
    test_vehicle( "fire_truck", 6319523, 410700, 83850, 19080, 4063 );
    test_vehicle( "truck_swat", 5906166, 674453, 134446, 29610, 7604 );
    test_vehicle( "tractor_plow", 725658, 681200, 681200, 132400, 132400 );
    test_vehicle( "apc", 5797694, 2091626, 2091626, 110600, 110657 );
    test_vehicle( "humvee", 5516216, 768000, 565000, 25620, 17450 );
    test_vehicle( "road_roller", 8831804, 602500, 147100, 22760, 6925 );
    test_vehicle( "golf_cart", 319630, 49585, 47185, 22700, 12745 );

    // in reverse
    test_vehicle( "beetle", 818837, 58970, 58870, 44560, 43060, 0, 0, true );
    test_vehicle( "car", 1125629, 76060, 76060, 44230, 24870, 0, 0, true );
    test_vehicle( "car_sports", 1157382, 353200, 268000, 35200, 19540, 0, 0, true );
    test_vehicle( "electric_car", 879098, 133100, 72520, 8140, 3390, 0, 0, true );
    test_vehicle( "suv", 1325297, 112000, 111800, 66880, 31670, 0, 0, true );
    test_vehicle( "motorcycle", 163085, 19980, 19030, 15490, 14890, 0, 0, true );
    test_vehicle( "quad_bike", 264465, 19650, 19650, 15440, 15440, 0, 0, true );
    test_vehicle( "scooter", 57587, 62440, 62440, 47990, 47990, 0, 0, true );
    test_vehicle( "superbike", 244085, 18320, 10570, 13070, 8497, 0, 0, true );
    test_vehicle( "ambulance", 1854071, 58460, 57780, 42480, 39130, 0, 0, true );
    test_vehicle( "fire_engine", 2257115, 258000, 257800, 179800, 173300, 0, 0, true );
    test_vehicle( "fire_truck", 6319523, 58480, 58640, 18600, 4471, 0, 0, true );
    test_vehicle( "truck_swat", 5906166, 129300, 130100, 29350, 7668, 0, 0, true );
    test_vehicle( "tractor_plow", 725658, 72240, 72240, 53610, 53610, 0, 0, true );
    test_vehicle( "apc", 5830459, 418700, 419400, 107300, 74330, 0, 0, true );
    test_vehicle( "humvee", 5531381, 89940, 89940, 25780, 9086, 0, 0, true );
    test_vehicle( "road_roller", 8831804, 97490, 97690, 22880, 6606, 0, 0, true );
    test_vehicle( "golf_cart", 319630, 37140, 11510, 14110, 4450, 0, 0, true );
}
