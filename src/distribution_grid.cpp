#include <unordered_set>
#include <algorithm>
#include <climits>
#include <cstdint>
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
#include "messages.h"
#include "submap.h"
#include "options.h"
#include "overmapbuffer.h"
#include "overmapbuffer_registry.h"
#include "profile.h"

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
            battery_tile *battery = active_tiles::furn_at<battery_tile>( loc.absolute, mb );
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

            vehicle_connector_tile *connector = active_tiles::furn_at<vehicle_connector_tile>( loc.absolute,
                                                mb );
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

    if( !recurse || amt == 0 ) {
        return amt;
    }

    // Chain remaining surplus/deficit to grids linked via grid_link_tile portals.
    // recurse=false on remote calls prevents infinite loops.
    std::ranges::for_each( flat_contents, [&]( const tripoint_abs_ms & pos ) {
        if( amt == 0 ) {
            return;
        }
        auto *glt = active_tiles::furn_at<grid_link_tile>( pos, mb );
        if( !glt || !glt->linked || glt->paused ) {
            return;
        }
        auto *remote = get_distribution_grid_tracker_for( glt->target_dim_id );
        if( remote ) {
            amt = remote->grid_at( glt->target_pos ).mod_resource( amt, false );
        }
    } );

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
            battery_tile *battery = active_tiles::furn_at<battery_tile>( loc.absolute, mb );
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

            vehicle_connector_tile *connector = active_tiles::furn_at<vehicle_connector_tile>( loc.absolute,
                                                mb );
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
        return res;
    }

    // Chain to grids linked via grid_link_tile portals.
    // recurse=false on remote calls prevents infinite loops.
    std::ranges::for_each( flat_contents, [&]( const tripoint_abs_ms & pos ) {
        auto *glt = active_tiles::furn_at<grid_link_tile>( pos, mb );
        if( !glt || !glt->linked || glt->paused ) {
            return;
        }
        auto *remote = get_distribution_grid_tracker_for( glt->target_dim_id );
        if( remote ) {
            res += remote->grid_at( glt->target_pos ).get_resource( false );
        }
    } );

    return res;
}

auto distribution_grid::get_power_stat_local() const -> power_stat
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

        if( auto *s = active_tiles::furn_at<solar_tile>( pos, mb ) ) { return to_stat( s->get_power_w() ); }
        if( auto *c = active_tiles::furn_at<charger_tile>( pos, mb ) ) { return to_stat( -c->power ); }
        if( auto *sc = active_tiles::furn_at<steady_consumer_tile>( pos, mb ) ) { return to_stat( -sc->power ); }
        if( auto *vc = active_tiles::furn_at<vehicle_connector_tile>( pos, mb ) ) { return get_vehicle_stats( vc ); }

        return power_stat{};
    };

    return contents
           | std::views::values
           | std::views::join
           | std::views::transform( get_loc_stats )
           | cata::ranges::fold_left( power_stat{}, std::plus<> {} );
}

auto distribution_grid::get_power_stat() const -> power_stat
{
    auto stat = get_power_stat_local();

    // For each active portal link rooted in this grid:
    //  - add this side's upkeep cost as consumption (shown once per side)
    //  - chain to the remote side's local stats (gen/use from remote tiles)
    // Using get_power_stat_local() on the remote prevents double-counting the
    // remote portal tile's own upkeep contribution.
    int portal_upkeep_w = 0;
    power_stat remote_stat{};
    std::ranges::for_each( flat_contents, [&]( const tripoint_abs_ms & pos ) {
        auto *glt = active_tiles::furn_at<grid_link_tile>( pos, mb );
        if( !glt || !glt->linked || glt->paused ) {
            return;
        }
        portal_upkeep_w += grid_link_tile::upkeep_kj;
        auto *remote = get_distribution_grid_tracker_for( glt->target_dim_id );
        if( remote ) {
            remote_stat = remote_stat + remote->grid_at( glt->target_pos ).get_power_stat_local();
        }
    } );

    return stat + remote_stat + power_stat{ .gen_w = 0, .use_w = portal_upkeep_w };
}

void distribution_grid::apply_net_power( int64_t delta_w )
{
    if( delta_w == 0 ) {
        return;
    }
    // Clamp to int range before calling mod_resource().
    constexpr int64_t INT_MAX_64 = static_cast<int64_t>( INT_MAX );
    constexpr int64_t INT_MIN_64 = static_cast<int64_t>( INT_MIN );
    const int clamped = static_cast<int>(
                            std::max( INT_MIN_64, std::min( INT_MAX_64, delta_w ) )
                        );
    mod_resource( clamped );
}

distribution_grid_tracker::distribution_grid_tracker()
    : distribution_grid_tracker( MAPBUFFER, {} )
{}

distribution_grid_tracker::distribution_grid_tracker( mapbuffer &buffer, std::string dim_id )
    : mb( buffer )
    , dimension_id_( std::move( dim_id ) )
{
}

distribution_grid_tracker::~distribution_grid_tracker()
{
    // Release all outstanding load requests so the submap_load_manager
    // doesn't hold stale entries after this tracker is destroyed.
    std::ranges::for_each( export_nodes_, []( const cross_dimension_export_node & n ) {
        if( n.far_load_handle != 0 ) {
            submap_loader.release_load( n.far_load_handle );
        }
        if( n.local_load_handle != 0 ) {
            submap_loader.release_load( n.local_load_handle );
        }
    } );
}

void distribution_grid_tracker::add_export_node( cross_dimension_export_node node,
        bool register_reverse )
{
    // Avoid double-registration: if a node for this source already exists
    // (e.g. from a previous load that wasn't cleaned up), replace it.
    auto existing = std::ranges::find_if( export_nodes_,
    [&]( const cross_dimension_export_node & n ) {
        return n.source_pos == node.source_pos;
    } );
    if( existing != export_nodes_.end() ) {
        submap_loader.release_load( existing->far_load_handle );
        submap_loader.release_load( existing->local_load_handle );
        export_nodes_.erase( existing );
    }

    if( !node.paused ) {
        const int radius = get_option<int>( "POWER_PORTAL_LOAD_RADIUS" );

        // Keep the far end's submap resident so the cross-dimension grid works.
        const auto target_sm = project_to<coords::sm>( node.target_pos );
        const int tz = target_sm.raw().z;
        node.far_load_handle = submap_loader.request_load(
                                   load_request_source::player_base,
                                   node.target_dim_id,
                                   target_sm,
                                   radius,
                                   tz, tz );

        // Keep the LOCAL source submap resident too.  Without this the source
        // submap unloads when the player leaves → on_submap_unloaded removes
        // the export node → far_load_handle released → far end unloads → the
        // link collapses after one turn.
        const auto source_sm = project_to<coords::sm>( node.source_pos );
        const int sz = source_sm.raw().z;
        node.local_load_handle = submap_loader.request_load(
                                     load_request_source::player_base,
                                     dimension_id_,
                                     source_sm,
                                     radius,
                                     sz, sz );
    }

    // Give the link a grace period before the first upkeep check.
    if( node.last_upkeep == calendar::turn_zero ) {
        node.last_upkeep = calendar::turn;
    }

    // Capture reverse-registration fields before moving node.
    const auto reverse_target_dim = node.target_dim_id;
    const auto reverse_target_pos = node.target_pos;
    const auto reverse_source_pos = node.source_pos;
    const auto reverse_paused     = node.paused;

    export_nodes_.push_back( std::move( node ) );

    // Ensure the remote tracker exists and has the matching reverse node.
    // Pass register_reverse=false to prevent infinite recursion.
    if( register_reverse ) {
        auto &remote_tracker = ensure_distribution_grid_tracker_for( reverse_target_dim );
        const auto already = std::ranges::any_of( remote_tracker.get_export_nodes(),
        [&]( const cross_dimension_export_node & n ) {
            return n.source_pos == reverse_target_pos
                   && n.target_pos == reverse_source_pos;
        } );
        if( !already ) {
            cross_dimension_export_node rnode;
            rnode.source_pos    = reverse_target_pos;
            rnode.target_dim_id = dimension_id_;
            rnode.target_pos    = reverse_source_pos;
            rnode.paused        = reverse_paused;
            remote_tracker.add_export_node( std::move( rnode ), false );
        }
    }
}

void distribution_grid_tracker::remove_export_node( const tripoint_abs_ms &source_pos )
{
    auto it = std::ranges::find_if( export_nodes_,
    [&]( const cross_dimension_export_node & n ) {
        return n.source_pos == source_pos;
    } );
    if( it != export_nodes_.end() ) {
        submap_loader.release_load( it->far_load_handle );
        submap_loader.release_load( it->local_load_handle );
        export_nodes_.erase( it );
    }
}

/// Sync the grid_link_tile's paused flag with the export node's state.
static void sync_glt_paused( mapbuffer &buf, const tripoint_abs_ms &pos, bool paused )
{
    tripoint_abs_sm sm_abs;
    point_sm_ms sm_pt;
    std::tie( sm_abs, sm_pt ) = project_remain<coords::sm>( pos );
    submap *sm = buf.lookup_submap( sm_abs );
    if( sm == nullptr ) {
        return;
    }
    const auto it = sm->active_furniture.find( sm_pt );
    if( it == sm->active_furniture.end() ) {
        return;
    }
    grid_link_tile *glt = dynamic_cast<grid_link_tile *>( it->second.get() );
    if( glt != nullptr ) {
        glt->paused = paused;
    }
}

void distribution_grid_tracker::pause_export_node( const tripoint_abs_ms &source_pos )
{
    auto it = std::ranges::find_if( export_nodes_,
    [&]( const cross_dimension_export_node & n ) {
        return n.source_pos == source_pos;
    } );
    if( it != export_nodes_.end() && !it->paused ) {
        submap_loader.release_load( it->far_load_handle );
        submap_loader.release_load( it->local_load_handle );
        it->far_load_handle = 0;
        it->local_load_handle = 0;
        it->paused = true;
        sync_glt_paused( mb, source_pos, true );
    }
}

void distribution_grid_tracker::resume_export_node( const tripoint_abs_ms &source_pos )
{
    auto it = std::ranges::find_if( export_nodes_,
    [&]( const cross_dimension_export_node & n ) {
        return n.source_pos == source_pos;
    } );
    if( it != export_nodes_.end() && it->paused ) {
        const int radius = get_option<int>( "POWER_PORTAL_LOAD_RADIUS" );

        const auto target_sm = project_to<coords::sm>( it->target_pos );
        const int tz = target_sm.raw().z;
        it->far_load_handle = submap_loader.request_load(
                                  load_request_source::player_base,
                                  it->target_dim_id,
                                  target_sm,
                                  radius,
                                  tz, tz );

        const auto source_sm = project_to<coords::sm>( it->source_pos );
        const int sz = source_sm.raw().z;
        it->local_load_handle = submap_loader.request_load(
                                    load_request_source::player_base,
                                    dimension_id_,
                                    source_sm,
                                    radius,
                                    sz, sz );
        it->paused = false;
        sync_glt_paused( mb, source_pos, false );
    }
}

distribution_grid &distribution_grid_tracker::make_distribution_grid_at(
    const tripoint_abs_sm &sm_pos )
{
    ZoneScoped;
    if( !get_option<bool>( "ELECTRIC_GRID" ) ) {
        static distribution_grid empty_grid( {}, MAPBUFFER );
        return empty_grid;
    }
    const std::set<tripoint_abs_omt> overmap_positions = get_overmapbuffer(
                dimension_id_ ).electric_grid_at(
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

std::array<tripoint_abs_sm, 4> distribution_grid_tracker::get_submaps_for_omt(
    tripoint_abs_omt omt_pos )
{
    // An OMT at omt_pos contains exactly 4 submaps: SW, SE, NW, NE corners.
    const tripoint_abs_sm base = project_to<coords::sm>( omt_pos );
    return { {
            base,
            base + point_east,
            base + point_south,
            base + point_south_east
        }
    };
}

std::array<tripoint_abs_omt, 5> distribution_grid_tracker::get_omt_and_cardinal_neighbors(
    tripoint_abs_omt omt_pos )
{
    // The OMT itself plus the 4 cardinal neighbors (no diagonals: connections
    // run along cardinal axes only).
    return { {
            omt_pos,
            omt_pos + point_north,
            omt_pos + point_south,
            omt_pos + point_west,
            omt_pos + point_east
        }
    };
}

void distribution_grid_tracker::on_submap_loaded( const tripoint_abs_sm &pos,
        const std::string &dim_id )
{
    ZoneScoped;
    // Each tracker only manages submaps for its own dimension.
    // Without this, trackers would process events for foreign dimensions.
    if( dim_id != dimension_id_ ) {
        return;
    }
    tracked_submaps_.insert( pos );
    make_distribution_grid_at( pos );

    // Scan newly-loaded submap for grid_link_tile active furniture and register
    // their export nodes so the link is live immediately on load.
    submap *sm = mb.lookup_submap( pos );
    if( sm == nullptr ) {
        return;
    }
    std::ranges::for_each( sm->active_furniture, [&]( const auto & kv ) {
        const grid_link_tile *glt = dynamic_cast<const grid_link_tile *>( kv.second.get() );
        if( glt == nullptr || !glt->linked ) {
            return;
        }
        const tripoint_abs_ms abs_pos = project_combine( pos, kv.first );
        cross_dimension_export_node node;
        node.source_pos    = abs_pos;
        node.target_dim_id = glt->target_dim_id;
        node.target_pos    = glt->target_pos;
        node.paused        = glt->paused;
        // register_reverse=false: we may be inside submap_loader iteration,
        // so we can't safely create new trackers or add listeners.
        // The reverse node will be registered when the remote tracker is
        // created/ensured (via ensure_distribution_grid_tracker_for replay).
        add_export_node( std::move( node ), false );
    } );
}

void distribution_grid_tracker::on_submap_unloaded( const tripoint_abs_sm &pos,
        const std::string &dim_id )
{
    ZoneScoped;
    if( dim_id != dimension_id_ ) {
        return;
    }

    // Remove export nodes whose source tile is in this submap before eviction.
    // The submap is still resident at this point so we can scan active_furniture.
    submap *sm = mb.lookup_submap( pos );
    if( sm != nullptr ) {
        std::ranges::for_each( sm->active_furniture, [&]( const auto & kv ) {
            const grid_link_tile *glt = dynamic_cast<const grid_link_tile *>( kv.second.get() );
            if( glt != nullptr && glt->linked ) {
                const tripoint_abs_ms abs_pos = project_combine( pos, kv.first );
                remove_export_node( abs_pos );
            }
        } );
    }

    tracked_submaps_.erase( pos );

    // One OMT spans 4 submaps; invalidate all 4 to avoid stale grid entries.
    const tripoint_abs_omt omt_pos = project_to<coords::omt>( pos );
    for( const tripoint_abs_sm &smp : get_submaps_for_omt( omt_pos ) ) {
        auto it = parent_distribution_grids.find( smp );
        if( it != parent_distribution_grids.end() ) {
            grids_requiring_updates.erase( it->second );
            parent_distribution_grids.erase( it );
        }
    }
    // The remaining tracked submaps of this OMT (if any) will re-register their
    // grid on the next on_changed() call or make_distribution_grid_at() call.
}

void distribution_grid_tracker::on_changed( const tripoint_abs_ms &p )
{
    ZoneScoped;
    const tripoint_abs_sm sm_pos = project_to<coords::sm>( p );
    // 3D check: only process if this submap is actually loaded.
    // The old code used a 2D bounds rectangle which would fire spuriously for
    // unloaded z-levels whose XY happened to fall inside the bubble.
    if( !tracked_submaps_.contains( sm_pos ) ) {
        return;
    }
    const tripoint_abs_omt omt_pos = project_to<coords::omt>( sm_pos );
    // Defer the actual rebuild to flush_dirty_omts() at the start of the next
    // update() tick.  Multiple on_changed() calls within the same tick that
    // hit the same 5-OMT cluster will therefore trigger only one rebuild per OMT.
    for( const tripoint_abs_omt &omt : get_omt_and_cardinal_neighbors( omt_pos ) ) {
        dirty_omts_.insert( omt );
    }
}

void distribution_grid_tracker::flush_dirty_omts()
{
    if( dirty_omts_.empty() ) {
        return;
    }
    ZoneScoped;
    TracyPlot( "Dirty OMTs", static_cast<int64_t>( dirty_omts_.size() ) );
    for( const tripoint_abs_omt &omt : dirty_omts_ ) {
        for( const tripoint_abs_sm &smp : get_submaps_for_omt( omt ) ) {
            if( tracked_submaps_.contains( smp ) ) {
                make_distribution_grid_at( smp );
                break;  // one call per OMT is sufficient
            }
        }
    }
    dirty_omts_.clear();
}

void distribution_grid_tracker::clear()
{
    // Preserve export nodes: they represent persistent cross-dimension links
    // that must survive dimension switches.  Only the grid topology (tracked
    // submaps, distribution grids) needs to be reset here.
    tracked_submaps_.clear();
    parent_distribution_grids.clear();
    grids_requiring_updates.clear();
    dirty_omts_.clear();

    // Re-register submaps that export nodes keep loaded.  These submaps are
    // still resident (held by load handles) but won't receive on_submap_loaded
    // events since they never left the simulated set.
    std::ranges::for_each( export_nodes_, [&]( const cross_dimension_export_node & n ) {
        if( !n.paused ) {
            const auto sm = project_to<coords::sm>( n.source_pos );
            tracked_submaps_.insert( sm );
            make_distribution_grid_at( sm );
        }
    } );
}

void distribution_grid_tracker::on_options_changed()
{
    // Rebuild all tracked grids from scratch (e.g. ELECTRIC_GRID option toggled).
    // Clear dirty_omts_ first — the full rebuild makes any pending deferred
    // changes obsolete.
    parent_distribution_grids.clear();
    grids_requiring_updates.clear();
    dirty_omts_.clear();
    for( const tripoint_abs_sm &sm_pos : tracked_submaps_ ) {
        make_distribution_grid_at( sm_pos );
    }
}

distribution_grid &distribution_grid_tracker::grid_at( const tripoint_abs_ms &p )
{
    // Flush any deferred on_changed() rebuilds before querying so that callers
    // always see an up-to-date grid topology.  flush_dirty_omts() returns
    // immediately when the dirty set is empty, so this is free in the common case.
    flush_dirty_omts();
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
            // Something transforming on a non-existant map...?
            return;
        }

        const furn_t &old_t = sm->get_furn( p_within_sm.raw() ).obj();
        const furn_t &new_t = qt.id.obj();
        const tripoint pos_local = m.getlocal( qt.p.raw() );

        if( !qt.msg.empty() ) {
            if( u.sees( pos_local ) ) {
                add_msg( "%s", _( qt.msg ) );
            }
        }

        if( m.inbounds( pos_local ) ) {
            m.furn_set( pos_local, qt.id );
            return;
        }

        // Something is transforming from an unloaded map...?

        // TODO: this is copy-pasted from map.cpp
        sm->set_furn( p_within_sm.raw(), qt.id );
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
    ZoneScoped;
    flush_dirty_omts();
    for( const shared_ptr_fast<distribution_grid> &grid : grids_requiring_updates ) {
        grid->update( to );
    }
    transform_queue.apply( mb, *this, get_player_character(), get_map() );
    transform_queue.clear();
}

