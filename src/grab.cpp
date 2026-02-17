#include "enums.h"
#include "game.h" // IWYU pragma: associated

#include <cstdlib>
#include <algorithm>
#include <numeric>

#include "avatar.h"
#include "character.h"
#include "character_functions.h"
#include "map.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "point.h"
#include "sounds.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "debug.h"
#include "rng.h"
#include "tileray.h"
#include "translations.h"
#include "units.h"

static const efftype_id effect_harnessed( "harnessed" );

namespace
{
auto make_scraping_noise( const tripoint &pos, const int volume ) -> void
{
    sounds::sound( pos, volume, sounds::sound_t::movement,
                   _( "a scraping noise." ), true, "misc", "scraping" );
}

// vehicle movement: strength check. very strong humans can move about 2,000 kg in a wheelbarrow.
auto base_str_req( vehicle *veh )-> int
{
    return veh->total_mass() / 100_kilogram;
}

// alternative strength check, for saner results with weights less than 100 kg
auto offroad_str_req_cap( vehicle *veh )-> int
{
    return veh->total_mass() / 10_kilogram;
}

// determine movecost for terrain touching wheels
auto get_grabbed_vehicle_movecost( vehicle *veh ) -> int
{
    const int str_req = base_str_req( veh );
    const auto &map = get_map();
    const tripoint &vehpos = veh->global_pos3();

    const auto get_wheel_pos = [&]( const int p ) {
        return vehpos + veh->part( p ).precalc[0];
    };

    const auto &wheel_indices = veh->wheelcache;
    return std::accumulate( wheel_indices.begin(), wheel_indices.end(), 0,
    [&]( const int sum, const int p ) {
        const tripoint wheel_pos = get_wheel_pos( p );
        const int mapcost = map.move_cost( wheel_pos, veh );
        const int movecost = str_req / static_cast<int>( wheel_indices.size() ) * mapcost;

        return sum + movecost;
    } );
}

//if vehicle has many or only one wheel (shopping cart), it is as if it had four.
auto get_effective_wheels( vehicle *veh ) -> int
{
    const auto &wheels = veh->wheelcache;

    return ( wheels.size() > 4 || wheels.size() == 1 ) ? 4 : wheels.size();
}

// very strong humans can move about 2,000 kg in a wheelbarrow.
auto get_vehicle_str_requirement( vehicle *veh ) -> int
{
    if( !veh->valid_wheel_config() ) {
        return base_str_req( veh ) * 10;
    }

    //if vehicle is rollable we modify str_req based on a function of movecost per wheel.
    const int all_movecost = get_grabbed_vehicle_movecost( veh );
    // off-road coefficient (always 1.0 on a road, as low as 0.1 off road.)
    const float traction = veh->k_traction(
                               get_map().vehicle_wheel_traction( *veh ) );
    return ( 1 + all_movecost / get_effective_wheels( veh ) ) / traction;
}

} // namespace


bool game::grabbed_veh_move( const tripoint &dp )
{
    const optional_vpart_position grabbed_vehicle_vp = m.veh_at( u.pos() + u.grab_point );
    if( !grabbed_vehicle_vp ) {
        add_msg( m_info, _( "No vehicle at grabbed point." ) );
        u.grab( OBJECT_NONE );
        return false;
    }
    vehicle *grabbed_vehicle = &grabbed_vehicle_vp->vehicle();
    if( !grabbed_vehicle ||
        !grabbed_vehicle->handle_potential_theft( u ) ) {
        return false;
    }
    const int grabbed_part = grabbed_vehicle_vp->part_index();
    for( int part_index = 0; part_index < grabbed_vehicle->part_count(); ++part_index ) {
        monster *mon = grabbed_vehicle->get_pet( part_index );
        if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
            add_msg( m_info, _( "You cannot move this vehicle whilst your %s is harnessed!" ),
                     mon->get_name() );
            u.grab( OBJECT_NONE );
            return false;
        }
    }
    const vehicle *veh_under_player = veh_pointer_or_null( m.veh_at( u.pos() ) );
    if( grabbed_vehicle == veh_under_player ) {
        u.grab_point = -dp;
        return false;
    }

    tripoint dp_veh = -u.grab_point;
    const tripoint prev_grab = u.grab_point;
    tripoint next_grab = u.grab_point;

    bool zigzag = false;

    if( dp == prev_grab ) {
        // We are pushing in the direction of vehicle
        dp_veh = dp;
    } else if( std::abs( dp.x + dp_veh.x ) != 2 && std::abs( dp.y + dp_veh.y ) != 2 ) {
        // Not actually moving the vehicle, don't do the checks
        u.grab_point = -( dp + dp_veh );
        return false;
    } else if( ( dp.x == prev_grab.x || dp.y == prev_grab.y ) &&
               next_grab.x != 0 && next_grab.y != 0 ) {
        // Zig-zag (or semi-zig-zag) pull: player is diagonal to vehicle
        // and moves away from it, but not directly away
        dp_veh.x = dp.x == -dp_veh.x ? 0 : dp_veh.x;
        dp_veh.y = dp.y == -dp_veh.y ? 0 : dp_veh.y;

        next_grab = -dp_veh;
        zigzag = true;
    } else {
        // We are pulling the vehicle
        next_grab = -dp;
    }

    // Make sure the mass and pivot point are correct
    grabbed_vehicle->invalidate_mass();

    //vehicle movement: strength check. very strong humans can move about 2,000 kg in a wheelbarrow.
    // int str_req = grabbed_vehicle->total_mass() / 100_kilogram; //strength required to move vehicle.
    // for smaller vehicles, offroad_str_req_cap sanity-checks our results.
    int str_req = std::min( get_vehicle_str_requirement( grabbed_vehicle ),
                            offroad_str_req_cap( grabbed_vehicle ) );
    int str = character_funcs::get_lift_strength_with_helpers( u );
    add_msg( m_debug, "str_req: %d", str_req );

    //final strength check and outcomes
    ///\EFFECT_STR determines ability to drag vehicles
    if( str_req <= str ) {
        if( !grabbed_vehicle->valid_wheel_config() ) {
            make_scraping_noise( grabbed_vehicle->global_pos3(), str_req * 2 );
        }

        //calculate exertion factor and movement penalty
        ///\EFFECT_STR increases speed of dragging vehicles
        u.moves -= 400 * str_req / std::max( 1, str );
        ///\EFFECT_STR decreases stamina cost of dragging vehicles
        u.mod_stamina( -200 * str_req / std::max( 1, str ) );
        const int ex = dice( 1, 6 ) - 1 + str_req;
        if( ex > str + 1 ) {
            // Pain and movement penalty if exertion exceeds character strength
            add_msg( m_bad, _( "You strain yourself to move the %s!" ), grabbed_vehicle->name );
            u.moves -= 200;
            u.mod_pain( 1 );
        } else if( ex >= str ) {
            // Movement is slow if exertion nearly equals character strength
            add_msg( _( "It takes some time to move the %s." ), grabbed_vehicle->name );
            u.moves -= 200;
        }
    } else {
        u.moves -= 100;
        add_msg( m_bad, _( "You lack the strength to move the %s" ), grabbed_vehicle->name );
        return true;
    }

    std::string blocker_name = _( "errors in movement code" );
    const auto get_move_dir = [&]( const tripoint & dir, const tripoint & from ) {
        tileray mdir;

        mdir.init( dir.xy() );
        grabbed_vehicle->turn( mdir.dir() - grabbed_vehicle->face.dir() );
        grabbed_vehicle->face = grabbed_vehicle->turn_dir;
        grabbed_vehicle->precalc_mounts( 1, mdir.dir(), grabbed_vehicle->pivot_point() );

        // Grabbed part has to stay at distance 1 to the player
        // and in roughly the same direction.
        const tripoint new_part_pos = grabbed_vehicle->global_pos3() +
                                      grabbed_vehicle->part( grabbed_part ).precalc[ 1 ];
        const tripoint expected_pos = u.pos() + dp + from;
        const tripoint actual_dir = expected_pos - new_part_pos;

        grabbed_vehicle->adjust_zlevel( 1, dp );

        // Set player location to illegal value so it can't collide with vehicle.
        const tripoint player_prev = u.pos();
        u.setpos( tripoint_zero );
        std::vector<veh_collision> colls;
        const bool failed = grabbed_vehicle->collision( colls, actual_dir, true );
        u.setpos( player_prev );
        if( !colls.empty() ) {
            blocker_name = colls.front().target_name;
        }
        return failed ? tripoint_zero : actual_dir;
    };

    // First try the move as intended
    // But if that fails and the move is a zig-zag, try to recover:
    // Try to place the vehicle in the position player just left rather than "flattening" the zig-zag
    tripoint final_dp_veh = get_move_dir( dp_veh, next_grab );
    if( final_dp_veh == tripoint_zero && zigzag ) {
        final_dp_veh = get_move_dir( -prev_grab, -dp );
        next_grab = -dp;
    }

    if( final_dp_veh == tripoint_zero ) {
        add_msg( _( "The %s collides with %s." ), grabbed_vehicle->name, blocker_name );
        u.grab_point = prev_grab;
        return true;
    }

    u.grab_point = next_grab;

    m.displace_vehicle( *grabbed_vehicle, final_dp_veh );

    if( grabbed_vehicle ) {
        grabbed_vehicle->shift_zlevel();
        grabbed_vehicle->check_falling_or_floating();
    } else {
        debugmsg( "Grabbed vehicle disappeared" );
        return false;
    }

    for( int p : grabbed_vehicle->wheelcache ) {
        if( one_in( 2 ) ) {
            tripoint wheel_p = grabbed_vehicle->global_part_pos3( grabbed_part );
            grabbed_vehicle->handle_trap( wheel_p, p );
        }
    }

    return false;

}
