#include "coordinates.h"
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
#include "vehicle_grab.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"
#include "debug.h"
#include "rng.h"
#include "tileray.h"
#include "translations.h"
#include "units.h"

static const efftype_id effect_harnessed( "harnessed" );

namespace
{
auto make_scraping_noise( const tripoint_bub_ms &pos, const int volume ) -> void
{
    sound_event se;
    se.origin = pos;
    se.volume = volume;
    se.category = sounds::sound_t::movement;
    se.movement_noise = true;
    se.description = _( "a scraping noise." );
    se.id = "misc";
    se.variant = "scraping";
    sounds::sound( se );
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

    const auto &wheel_indices = veh->wheelcache;
    return std::accumulate( wheel_indices.begin(), wheel_indices.end(), 0,
    [&]( const int sum, const int p ) {
        const auto wheel_pos = veh->bub_part_location( p );
        const int mapcost = map.move_cost( wheel_pos, veh );
        const int movecost = str_req / static_cast<int>( wheel_indices.size() ) * mapcost;

        return sum + movecost;
    } );
}

//if vehicle has many or only one wheel (shopping cart), it is as if it had four.
auto get_effective_wheels( vehicle *veh ) -> int
{
    const auto &wheels = veh->wheelcache;

    return ( wheels.size() > 4 || wheels.size() == 1 || wheels.size() == 0 ) ? 4 : wheels.size();
}

// very strong humans can move about 2,000 kg in a wheelbarrow.
auto get_vehicle_str_requirement( vehicle *veh ) -> int
{
    // Really as it is floating behind you it shouldn't be too hard to do slowly
    // If one can do 2000 in a wheelbarrow
    // 400kg blimp at 20 str should be easy
    if( veh->has_sufficient_lift( true ) ) {
        return base_str_req( veh ) / 50;
    } else if( !veh->valid_wheel_config() ) {
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


bool game::grabbed_veh_move( const tripoint_rel_ms &dp )
{
    const auto grabbed_vehicle_target = vehicle_grab_target_at( m, u.bub_pos() + u.grab_point );
    if( !grabbed_vehicle_target ) {
        add_msg( m_info, _( "No vehicle at grabbed point." ) );
        u.grab( OBJECT_NONE );
        return false;
    }
    const auto &grabbed_vehicle_vp = grabbed_vehicle_target->vp;
    u.grab_point = grabbed_vehicle_target->pos - u.bub_pos();
    auto *grabbed_vehicle = &grabbed_vehicle_vp.vehicle();
    if( !grabbed_vehicle ||
        !grabbed_vehicle->handle_potential_theft( u ) ) {
        return false;
    }
    const auto grabbed_part = static_cast<int>( grabbed_vehicle_vp.part_index() );
    for( const vpart_reference &part : grabbed_vehicle->get_all_parts() ) {
        auto *mon = grabbed_vehicle->get_pet( static_cast<int>( part.part_index() ) );
        if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
            add_msg( m_info, _( "You cannot move this vehicle whilst your %s is harnessed!" ),
                     mon->get_name() );
            u.grab( OBJECT_NONE );
            return false;
        }
    }
    const vehicle *veh_under_player = veh_pointer_or_null( m.veh_at( u.bub_pos() ) );
    if( grabbed_vehicle == veh_under_player ) {
        u.grab_point = -dp;
        return false;
    }

    const auto player_next_pos = tripoint_bub_ms( u.bub_pos() + dp );
    const auto horizontal_dp = tripoint_rel_ms( dp.xy(), 0 );
    const auto horizontal_grab = tripoint_rel_ms( u.grab_point.xy(), 0 );
    auto dp_veh = -horizontal_grab;
    const auto prev_grab = u.grab_point;
    auto next_grab = u.grab_point;

    bool zigzag = false;

    if( horizontal_dp == horizontal_grab ) {
        // We are pushing in the direction of vehicle
        dp_veh = horizontal_dp;
    } else if( std::abs( horizontal_dp.x() + dp_veh.x() ) != 2 &&
               std::abs( horizontal_dp.y() + dp_veh.y() ) != 2 ) {
        // Not actually moving the vehicle, don't do the checks
        u.grab_point = prev_grab - dp;
        return false;
    } else if( ( horizontal_dp.x() == prev_grab.x() || horizontal_dp.y() == prev_grab.y() ) &&
               next_grab.x() != 0 && next_grab.y() != 0 ) {
        // Zig-zag (or semi-zig-zag) pull: player is diagonal to vehicle
        // and moves away from it, but not directly away
        dp_veh.x() = horizontal_dp.x() == -dp_veh.x() ? 0 : dp_veh.x();
        dp_veh.y() = horizontal_dp.y() == -dp_veh.y() ? 0 : dp_veh.y();

        next_grab = -dp_veh;
        zigzag = true;
    } else {
        // We are pulling the vehicle
        next_grab = -horizontal_dp;
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
        if( !grabbed_vehicle->valid_wheel_config() && !grabbed_vehicle->has_sufficient_lift( true ) ) {
            make_scraping_noise( grabbed_vehicle->bub_ms_location(), std::min( 80, str_req * 2 ) );
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
    const auto get_move_dir = [&]( const tripoint_rel_ms & dir, const tripoint_rel_ms & from ) {
        tileray mdir;

        mdir.init( dir.xy() );
        grabbed_vehicle->turn( mdir.dir() - grabbed_vehicle->face.dir() );
        grabbed_vehicle->face = grabbed_vehicle->turn_dir;
        grabbed_vehicle->precalc_mounts( 1, mdir.dir(), grabbed_vehicle->pivot_point() );

        // Grabbed part has to stay at distance 1 to the player
        // and in roughly the same direction.
        const auto new_part_pos = grabbed_vehicle->bub_ms_location() +
                                  grabbed_vehicle->part( grabbed_part ).precalc[ 1 ];
        const auto expected_pos = player_next_pos + from;
        const auto actual_dir = tripoint_rel_ms( expected_pos.xy() - new_part_pos.xy(), 0 );

        grabbed_vehicle->adjust_zlevel( 1, actual_dir );

        // Set player location to illegal value so it can't collide with vehicle.
        const auto player_prev = u.bub_pos();
        u.setpos( tripoint_bub_ms::zero() );
        std::vector<veh_collision> colls;
        const bool failed = grabbed_vehicle->collision( colls, actual_dir, true );
        u.setpos( player_prev );
        if( !colls.empty() ) {
            blocker_name = colls.front().target_name;
        }
        return failed ? tripoint_rel_ms::zero() : actual_dir;
    };

    // First try the move as intended
    // But if that fails and the move is a zig-zag, try to recover:
    // Try to place the vehicle in the position player just left rather than "flattening" the zig-zag
    tripoint_rel_ms final_dp_veh = get_move_dir( dp_veh, next_grab );
    if( final_dp_veh == tripoint_rel_ms::zero() && zigzag ) {
        final_dp_veh = get_move_dir( -tripoint_rel_ms( prev_grab.xy(), 0 ), -horizontal_dp );
    }

    if( final_dp_veh == tripoint_rel_ms::zero() ) {
        add_msg( _( "The %s collides with %s." ), grabbed_vehicle->name, blocker_name );
        u.grab_point = prev_grab;
        return true;
    }

    m.displace_vehicle( *grabbed_vehicle, final_dp_veh );

    if( grabbed_vehicle ) {
        grabbed_vehicle->shift_zlevel();
        grabbed_vehicle->check_falling_or_floating();
    } else {
        debugmsg( "Grabbed vehicle disappeared" );
        return false;
    }

    u.grab_point = grabbed_vehicle->bub_part_location( grabbed_part ) - player_next_pos;

    for( const auto p : grabbed_vehicle->wheelcache ) {
        if( one_in( 2 ) ) {
            const auto wheel_p = grabbed_vehicle->bub_part_location( p );
            grabbed_vehicle->handle_trap( wheel_p, p );
        }
    }

    return false;

}
