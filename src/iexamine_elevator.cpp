#include <algorithm>
#include <optional>

#include "cata_algo.h"
#include "game.h"
#include "iexamine.h"
#include "mapdata.h"
#include "flood_fill.h"
#include "output.h"
#include "omdata.h"
#include "overmapbuffer.h"
#include "player.h"
#include "coordinates.h"
#include "map.h"
#include "point.h"
#include "ui.h"
#include "vpart_range.h"
#include "vehicle_part.h"
#include "point_rotate.h"

namespace
{

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto move_item( map &here, const tripoint &src, const tripoint &dest ) -> void
{
    map_stack items_src = here.i_at( src );
    map_stack items_dest = here.i_at( dest );

    items_src.move_all_to( &items_dest );
}

namespace elevator
{

using tiles = std::vector<tripoint>;

auto here( const Character &you ) -> elevator::tiles
{
    const auto &here = get_map();
    const auto is_elevator = [&here]( const tripoint & pos ) -> bool { return here.has_flag( TFLAG_ELEVATOR, pos ); };

    std::unordered_set<tripoint> visited;
    return ff::point_flood_fill_4_connected( you.pos(), visited, is_elevator );
}

auto dest( const elevator::tiles &elevator_here,
           const tripoint &sm_orig,
           int turns,
           int movez ) -> elevator::tiles
{
    elevator::tiles tiles;
    std::ranges::transform( elevator_here, std::back_inserter( tiles ),
    [turns, &sm_orig, movez]( tripoint const & p ) {
        return rotate_point_sm( { p.xy(), movez }, sm_orig, turns );
    } );

    return tiles;
}

/// allow using misaligned elevators.
/// doesn't prevent you being stuck in the wall tho cause i was lazy
auto find_elevators_nearby( const tripoint &pos ) -> std::optional<tripoint>
{
    constexpr int max_misalign = 3;
    map &here = get_map();

    for( const auto &p : closest_points_first( pos, max_misalign ) ) {
        if( here.has_flag( TFLAG_ELEVATOR, p ) ) {
            return p;
        }
    }
    return {};
}

auto choose_floor( const tripoint &examp, const tripoint_abs_omt &this_omt,
                   const tripoint &sm_orig ) -> int
{
    constexpr int retval_offset = 10000; // workaround for uilist retval autoassign when retval == -1
    const auto this_floor = _( " (this floor)" );

    uilist choice;
    choice.title = _( "Please select destination floor" );
    for( int z = OVERMAP_HEIGHT; z >= -OVERMAP_DEPTH; z-- ) {
        const tripoint_abs_omt that_omt{ this_omt.xy(), z };
        const int turns = get_rot_turns( this_omt, that_omt );
        const tripoint zp =
            rotate_point_sm( { examp.xy(), z }, sm_orig, turns );

        if( !find_elevators_nearby( zp ) ) {
            continue;
        }
        const std::string omt_name = overmap_buffer.ter_existing( that_omt )->get_name();
        const auto floor_name = z == examp.z ? this_floor : "";
        const std::string name = string_format( "%3iF %s%s", z, omt_name, floor_name );

        choice.addentry( z + retval_offset, z != examp.z, MENU_AUTOASSIGN, name );
    }
    choice.query();
    return choice.ret - retval_offset;
}

enum class overlap_status { outside, inside, overlap };

auto vehicle_status( const wrapped_vehicle &veh, const elevator::tiles &tiles ) -> overlap_status
{
    const auto &ps = veh.v->get_all_parts();
    const int all_vparts_count = ps.part_count();
    const int vparts_inside = std::count_if( ps.begin(), ps.end(), [&]( const vpart_reference & vp ) {
        const tripoint p = veh.pos + vp.part().precalc[0];
        const auto eit = std::ranges::find( tiles, p );
        return eit != tiles.cend();
    } );

    if( vparts_inside == all_vparts_count ) {
        return overlap_status::inside;
    } else if( vparts_inside == 0 ) {
        return overlap_status::outside;
    } else {
        return overlap_status::overlap;
    }
}

struct elevator_vehicles {
    bool blocking = false;
    std::vector<vehicle *> v;
};

auto vehicles_on( const elevator::tiles &tiles ) -> elevator_vehicles
{
    const VehicleList vehs = get_map().get_vehicles();
    std::vector<vehicle *> ret;

    for( const wrapped_vehicle &veh : vehs ) {
        const auto status = vehicle_status( veh, tiles );
        if( status == overlap_status::overlap ) {
            return { true, { veh.v } };
        }
        if( status == overlap_status::inside ) {
            ret.push_back( veh.v );
        }
    }
    return { false, ret };
}

auto warn_blocking( const elevator_vehicles &vehs, std::string_view location ) -> void
{
    const auto &first_veh_name = vehs.v.front()->name;
    popup( string_format( _( "%1$s %2$s is blocking the elevator." ), first_veh_name, location ) );
}

auto move_creatures_away( const elevator::tiles &dest ) -> void
{
    map &here = get_map();

    const auto is_movable = [&]( const tripoint & candidate ) {
        return !here.has_flag( TFLAG_ELEVATOR, candidate )
               && here.passable( candidate )
               && !g->critter_at( candidate );
    };

    for( Creature &critter : g->all_creatures() ) {
        const tripoint local_pos = here.getlocal( here.getglobal( critter.pos() ) );

        const auto eit = std::ranges::find( dest, local_pos );
        if( eit == dest.cend() ) {
            continue;
        }

        const auto xs = closest_points_first( *eit, 10 );
        const auto candidate = std::ranges::find_if( xs, is_movable );
        if( candidate == xs.end() ) {
            continue;
        }
        critter.setpos( *candidate );
    }
}

auto move_items( const elevator::tiles &from, const elevator::tiles &dest ) -> void
{
    using size_type = elevator::tiles::size_type;
    map &here = get_map();

    // oh how i wish i could use zip here
    for( size_type i = 0; i < from.size(); i++ ) {
        const tripoint &src = from[i];
        move_item( here, src, dest[i] );
    }
}

auto move_creatures( const elevator::tiles &from, const elevator::tiles &dest ) -> void
{
    for( Creature &critter : g->all_creatures() ) {
        const auto eit = std::ranges::find( from, critter.pos() );
        if( eit != from.cend() ) {
            critter.setpos( dest[ std::distance( from.cbegin(), eit ) ] );
        }
    }
}

auto move_vehicles( const elevator_vehicles &vehs, const tripoint &sm_orig, int movez,
                    int turns ) -> void
{
    map &here = get_map();

    for( vehicle *v : vehs.v ) {
        const tripoint p = rotate_point_sm( { v->global_pos3().xy(), movez }, sm_orig, turns );
        here.displace_vehicle( *v, p - v->global_pos3() );
        v->turn( turns * 90_degrees );
        v->face = tileray( v->turn_dir );
        v->precalc_mounts( 0, v->turn_dir, v->pivot_anchor[0] );
    }
    here.reset_vehicle_cache();
}

auto move_player( player &p, const int movez, tripoint_abs_ms old_abs_pos ) -> void
{
    map &here = get_map();

    g->vertical_shift( movez );
    // yes, this is inefficient, but i'm lazy
    elevator::find_elevators_nearby( p.pos() )
    .transform( []( const tripoint & pos ) -> point { return g->place_player( pos ); } );

    cata_event_dispatch::avatar_moves( *p.as_avatar(), here, old_abs_pos.raw() );
}

} //namespace elevator

} // namespace

void iexamine::elevator( player &p, const tripoint &examp )
{
    map &here = get_map();
    const tripoint_abs_ms old_abs_pos = here.getglobal( p.pos() );
    const tripoint_abs_omt this_omt = project_to<coords::omt>( here.getglobal( examp ) );
    const tripoint sm_orig = here.getlocal( project_to<coords::ms>( this_omt ) );

    const auto elevator_here = elevator::here( p );
    const auto vehs = elevator::vehicles_on( elevator_here );
    if( vehs.blocking ) {
        return warn_blocking( vehs, _( "here" ) );
    }

    const int movez = elevator::choose_floor( examp, this_omt, sm_orig );
    if( movez < -OVERMAP_DEPTH ) {
        return;
    }

    const tripoint_abs_omt that_omt{ this_omt.xy(), movez };
    const int turns = get_rot_turns( this_omt, that_omt );

    const auto elevator_dest = elevator::dest( elevator_here, sm_orig, turns, movez );
    const auto vehicles_dest = elevator::vehicles_on( elevator_dest );
    if( !vehicles_dest.v.empty() ) {
        return warn_blocking( vehicles_dest, _( "at the destination floor" ) );
    }

    elevator::move_creatures_away( elevator_dest );
    elevator::move_items( elevator_here, elevator_dest );
    elevator::move_creatures( elevator_here, elevator_dest );
    elevator::move_vehicles( vehs, sm_orig, movez, turns );
    elevator::move_player( p, movez, old_abs_pos );
}
