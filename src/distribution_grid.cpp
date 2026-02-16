#include <unordered_set>
#include <algorithm>
#include <ranges>

#include "cata_algo.h"
#include "character.h"
#include "debug.h"
#include "distribution_grid.h"
#include "coordinate_conversions.h"
#include "active_tile_data.h"
#include "active_tile_data_def.h"
#include "map.h"
#include "mapbuffer.h"
#include "map_iterator.h"
#include "messages.h"
#include "submap.h"
#include "options.h"
#include "overmapbuffer.h"

static distribution_grid empty_grid( {}, MAPBUFFER );

distribution_grid::distribution_grid( const std::vector<tripoint_abs_sm> &global_submap_coords,
                                      mapbuffer &buffer ) :
    submap_coords( global_submap_coords ),
    mb( buffer )
{
    for( const tripoint_abs_sm &sm_coord : submap_coords ) {
        submap *sm = mb.lookup_submap( sm_coord );
        if( sm == nullptr ) {
            // Debugmsg already printed in mapbuffer.cpp
            return;
        }

        for( auto &active : sm->active_furniture ) {
            const tripoint_abs_ms abs_pos = project_combine( sm_coord, active.first );
            contents[sm_coord].emplace_back( active.first, abs_pos );
            flat_contents.emplace_back( abs_pos );
        }
    }
}

bool distribution_grid::empty() const
{
    return contents.empty();
}

distribution_grid::operator bool() const
{
    return !empty() && !submap_coords.empty();
}

void distribution_grid::update( time_point to )
{
    for( const auto &c : contents ) {
        submap *sm = mb.lookup_submap( c.first );
        if( sm == nullptr ) {
            return;
        }

        for( const tile_location &loc : c.second ) {
            auto &active = sm->active_furniture[loc.on_submap];
            if( !active ) {
                debugmsg( "No active furniture at %s", loc.absolute.to_string() );
                contents.clear();
                return;
            }
            active->update( to, loc.absolute, *this );
        }
    }
}

// TODO: Shouldn't be here
#include "vehicle.h"
#include "vehicle_part.h"
static itype_id itype_battery( "battery" );
int distribution_grid::mod_resource( int amt, bool recurse )
{
    std::vector<vehicle *> connected_vehicles;
    for( const auto &c : contents ) {
        for( const tile_location &loc : c.second ) {
            battery_tile *battery = active_tiles::furn_at<battery_tile>( loc.absolute );
            if( battery != nullptr ) {
                int amt_before_battery = amt;
                amt = battery->mod_resource( amt );
                if( cached_amount_here ) {
                    cached_amount_here = *cached_amount_here + amt_before_battery - amt;
                }
                if( amt == 0 ) {
                    return 0;
                }
                continue;
            }

            if( !recurse ) {
                continue;
            }

            vehicle_connector_tile *connector = active_tiles::furn_at<vehicle_connector_tile>( loc.absolute );
            if( connector != nullptr ) {
                for( const tripoint_abs_ms &veh_abs : connector->connected_vehicles ) {
                    vehicle *veh = vehicle::find_vehicle( veh_abs );
                    if( veh == nullptr ) {
                        // TODO: Disconnect
                        debugmsg( "lost vehicle at %s", veh_abs.to_string() );
                        continue;
                    }
                    connected_vehicles.push_back( veh );
                }
            }
        }
    }

    // TODO: Giga ugly. We only charge the first vehicle to get it to use its recursive graph traversal because it's inaccessible from here due to being a template method
    if( !connected_vehicles.empty() ) {
        if( amt > 0 ) {
            amt = connected_vehicles.front()->charge_battery( amt, true );
        } else {
            amt = -connected_vehicles.front()->discharge_battery( -amt, true );
        }
    }

    return amt;
}

int distribution_grid::get_resource( bool recurse ) const
{
    if( !recurse ) {
        if( cached_amount_here ) {
            return *cached_amount_here;
        } else {
            cached_amount_here = 0;
        }
    }
    int res = 0;
    std::vector<vehicle *> connected_vehicles;
    for( const auto &c : contents ) {
        for( const tile_location &loc : c.second ) {
            battery_tile *battery = active_tiles::furn_at<battery_tile>( loc.absolute );
            if( battery != nullptr ) {
                res += battery->get_resource();
                if( !recurse && cached_amount_here ) {
                    cached_amount_here = *cached_amount_here + res;
                }
                continue;
            }

            if( !recurse ) {
                continue;
            }

            vehicle_connector_tile *connector = active_tiles::furn_at<vehicle_connector_tile>( loc.absolute );
            if( connector != nullptr ) {
                for( const tripoint_abs_ms &veh_abs : connector->connected_vehicles ) {
                    vehicle *veh = vehicle::find_vehicle( veh_abs );
                    if( veh == nullptr ) {
                        // TODO: Disconnect
                        debugmsg( "lost vehicle at %s", veh_abs.to_string() );
                        continue;
                    }
                    connected_vehicles.push_back( veh );
                }
            }
        }
    }

    // TODO: Giga ugly. We only charge the first vehicle to get it to use its recursive graph traversal because it's inaccessible from here due to being a template method
    if( !connected_vehicles.empty() ) {
        res = connected_vehicles.front()->fuel_left( itype_battery, true );
    }
    if( !recurse ) {
        cached_amount_here = res;
    }

    return res;
}

auto distribution_grid::get_power_stat() const -> power_stat
{
    constexpr auto to_stat = []( int net ) {
        return ( net > 0 ? power_stat{ .gen_w = net, .use_w = 0 } : power_stat{ .gen_w = 0, .use_w = -net } );
    };

    auto get_vehicle_stats = [&]( const vehicle_connector_tile * connector ) -> power_stat {
        return connector->connected_vehicles
        | std::views::transform( []( const auto & pos ) { return vehicle::find_vehicle( pos ); } )
        | std::views::filter( []( auto * v ) { return v; } )
        | std::views::transform( [&]( auto * veh ) { return to_stat( veh->net_battery_charge_rate_w() ); } )
        | cata::ranges::fold_left( power_stat{}, std::plus<>{} );
    };

    auto get_loc_stats = [&]( const tile_location & loc ) -> power_stat {
        const auto &pos = loc.absolute;

        if( auto *s = active_tiles::furn_at<solar_tile>( pos ) ) { return to_stat( s->get_power_w() ); }
        if( auto *c = active_tiles::furn_at<charger_tile>( pos ) ) { return to_stat( -c->power ); }
        if( auto *sc = active_tiles::furn_at<steady_consumer_tile>( pos ) ) { return to_stat( -sc->power ); }
        if( auto *vc = active_tiles::furn_at<vehicle_connector_tile>( pos ) ) { return get_vehicle_stats( vc ); }

        return power_stat{};
    };

    return contents
           | std::views::values
           | std::views::join
           | std::views::transform( get_loc_stats )
           | cata::ranges::fold_left( power_stat{}, std::plus<> {} );
}

distribution_grid_tracker::distribution_grid_tracker()
    : distribution_grid_tracker( MAPBUFFER )
{}

distribution_grid_tracker::distribution_grid_tracker( mapbuffer &buffer )
    : mb( buffer )
{
}

distribution_grid &distribution_grid_tracker::make_distribution_grid_at(
    const tripoint_abs_sm &sm_pos )
{
    if( !get_option<bool>( "ELECTRIC_GRID" ) ) {
        return empty_grid;
    }
    const std::set<tripoint_abs_omt> overmap_positions = overmap_buffer.electric_grid_at(
                project_to<coords::omt>( sm_pos ) );
    assert( !overmap_positions.empty() );
    std::vector<tripoint_abs_sm> submap_positions;
    for( const tripoint_abs_omt &omp : overmap_positions ) {
        tripoint_abs_sm tp = project_to<coords::sm>( omp );
        submap_positions.emplace_back( tp + point_zero );
        submap_positions.emplace_back( tp + point_east );
        submap_positions.emplace_back( tp + point_south );
        submap_positions.emplace_back( tp + point_south_east );
    }
    shared_ptr_fast<distribution_grid> dist_grid = make_shared_fast<distribution_grid>
            ( submap_positions, mb );
    for( const tripoint_abs_sm &smp : submap_positions ) {
        shared_ptr_fast<distribution_grid> &old_grid = parent_distribution_grids[smp];
        if( old_grid != dist_grid ) {
            grids_requiring_updates.erase( old_grid );
            old_grid = dist_grid;
        }
    }

    if( !dist_grid->empty() ) {
        grids_requiring_updates.emplace( dist_grid );
    }

    // This ugly expression + lint suppresion are needed to convince clang-tidy
    // that we are, in fact, NOT leaking memory.
    return *parent_distribution_grids[submap_positions.front()];
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
}

// TODO: Ugly, there should be a cleaner way
#include "worldfactory.h"

void distribution_grid_tracker::on_saved()
{
    if( !get_option<bool>( "ELECTRIC_GRID" ) ||
        world_generator->active_world == nullptr ) {
        return;
    }
    tripoint_abs_sm min_bounds( bounds.p_min, -OVERMAP_DEPTH );
    tripoint_abs_sm max_bounds( bounds.p_max, OVERMAP_HEIGHT );
    tripoint_range<tripoint_abs_sm> bounds_range( min_bounds, max_bounds );
    // Remove all grids that are no longer in the bounds
    for( auto iter = parent_distribution_grids.begin(); iter != parent_distribution_grids.end(); ) {
        if( !bounds_range.is_point_inside( iter->first ) ) {
            grids_requiring_updates.erase( iter->second );
            iter = parent_distribution_grids.erase( iter );
        } else {
            ++iter;
        }
    }
    for( const tripoint_abs_sm &sm_pos : bounds_range ) {
        if( !parent_distribution_grids.contains( sm_pos ) ) {
            make_distribution_grid_at( sm_pos );
        }
    }
}

void distribution_grid_tracker::on_changed( const tripoint_abs_ms &p )
{
    tripoint_abs_sm sm_pos = project_to<coords::sm>( p );
    // TODO: If not in bounds, just drop the grid, rebuild lazily
    if( parent_distribution_grids.contains( sm_pos ) ||
        bounds.contains( sm_pos.xy() ) ) {
        // TODO: Don't rebuild, update
        make_distribution_grid_at( sm_pos );
    }
}

void distribution_grid_tracker::on_options_changed()
{
    on_saved();
}

distribution_grid &distribution_grid_tracker::grid_at( const tripoint_abs_ms &p )
{
    tripoint_abs_sm sm_pos = project_to<coords::sm>( p );
    auto iter = parent_distribution_grids.find( sm_pos );
    if( iter != parent_distribution_grids.end() ) {
        return *iter->second;
    }

    // This is ugly for the const case
    return make_distribution_grid_at( sm_pos );
}

const distribution_grid &distribution_grid_tracker::grid_at( const tripoint_abs_ms &p ) const
{
    return const_cast<const distribution_grid &>(
               const_cast<distribution_grid_tracker *>( this )->grid_at( p ) );
}

std::uintptr_t distribution_grid_tracker::debug_grid_id( const tripoint_abs_omt &omp ) const
{
    tripoint_abs_sm sm_pos = project_to<coords::sm>( omp );
    auto iter = parent_distribution_grids.find( sm_pos );
    if( iter != parent_distribution_grids.end() ) {
        distribution_grid *ret = iter->second.get();
        return reinterpret_cast<std::uintptr_t>( ret );
    } else {
        return 0;
    }
}

void grid_furn_transform_queue::apply( mapbuffer &mb, distribution_grid_tracker &grid_tracker,
                                       Character &u, map &m )
{
    for( const auto &qt : queue ) {
        tripoint_abs_sm p_abs_sm;
        point_sm_ms p_within_sm;
        std::tie( p_abs_sm, p_within_sm ) = project_remain<coords::sm>( qt.p );

        submap *sm = mb.lookup_submap( p_abs_sm );
        if( sm == nullptr ) {
            return;
        }

        const furn_t &old_t = sm->get_furn( p_within_sm.raw() ).obj();
        const furn_t &new_t = qt.id.obj();
        const tripoint pos_local = m.getlocal( qt.p.raw() );

        if( m.inbounds( pos_local ) ) {
            m.furn_set( pos_local, qt.id );
        } else {
            sm->set_furn( p_within_sm.raw(), qt.id );
        }

        if( !qt.msg.empty() ) {
            if( u.sees( pos_local ) ) {
                add_msg( "%s", _( qt.msg ) );
            }
        }

        // TODO: this is copy-pasted from map.cpp
        if( old_t.active ) {
            sm->active_furniture.erase( p_within_sm );
            // TODO: Only for g->m? Observer pattern?
            grid_tracker.on_changed( qt.p );
        }
        if( new_t.active ) {
            active_tile_data *atd = new_t.active->clone();
            atd->set_last_updated( calendar::turn );
            sm->active_furniture[p_within_sm].reset( atd );
            grid_tracker.on_changed( qt.p );
        }
    }
}

std::string grid_furn_transform_queue::to_string() const
{
    std::string ret;
    size_t i = 0;
    for( const transform_queue_entry &q : queue ) {
        ret += string_format( "% 2d: %s %s \"%s\"\n", i, q.p.to_string(), q.id, q.msg );
        i++;
    }
    return ret;
}

void distribution_grid_tracker::update( time_point to )
{
    for( const shared_ptr_fast<distribution_grid> &grid : grids_requiring_updates ) {
        grid->update( to );
    }
    transform_queue.apply( mb, *this, get_player_character(), get_map() );
    transform_queue.clear();
}

void distribution_grid_tracker::load( half_open_rectangle<point_abs_sm> area )
{
    bounds = area;
    on_saved();
}

void distribution_grid_tracker::load( const map &m )
{
    point_abs_sm p_min( m.get_abs_sub().xy() );
    point_abs_sm p_max( p_min + point( m.getmapsize(), m.getmapsize() ) );
    load( half_open_rectangle<point_abs_sm>( p_min, p_max ) );
}
