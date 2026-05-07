#include "fire_spread_loader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <vector>

#include "cached_options.h"
#include "field.h"
#include "field_type.h"
#include "game_constants.h"
#include "mapbuffer.h"   // also pulls in mapbuffer_registry.h
#include "point.h"
#include "profile.h"
#include "submap.h"

fire_spread_loader fire_loader;

/**
 * Return true if the submap at @p abs_sm_pos in @p mb has at least one live
 * fire field (fd_fire intensity >= 1).
 */
// NOTE: is_field_alive() is non-const in the current codebase, so we need a
// non-const submap reference here even though we only read.
static bool submap_has_fire( submap &sm )
{
    if( sm.field_count == 0 ) {
        return false;
    }
    return std::ranges::any_of( sm.field_cache, [&]( const point_sm_ms & local ) {
        field_entry *fe = sm.get_field( local ).find_field( fd_fire );
        return fe != nullptr && fe->is_field_alive();
    } );
}

void fire_spread_loader::request_for_fire( const std::string &dim, tripoint_abs_sm pos )
{
    ZoneScoped;
    dim_pos_key key{ dim, pos };

    // Already tracked by this loader.
    if( fire_handles_.count( key ) ) {
        return;
    }

    // For positions inside the bubble: always register without applying the cap —
    // they are already loaded and cost no extra memory.  We need the fire_spread
    // request to exist *before* the bubble can shift away and trigger eviction.
    const bool in_bubble = submap_loader.is_properly_requested( dim, pos );
    if( !in_bubble ) {
        // Apply the cap only to genuinely out-of-bubble submaps.
        if( fire_spread_submap_cap <= 0 || loaded_count() >= fire_spread_submap_cap ) {
            return;
        }
    }

    // Request a single quad (radius 0) — always covers full z-pillar.
    load_request_handle h = submap_loader.request_load(
                                load_request_source::fire_spread,
                                dim,
                                pos,
                                0 );
    fire_handles_[key] = h;
}

void fire_spread_loader::prune_disconnected( submap_load_manager &loader )
{
    ZoneScoped;
    TracyPlot( "Fire-Loaded Submaps", static_cast<int64_t>( loaded_count() ) );
    // Cardinal offsets for neighbor checks.
    static const std::array<tripoint, 4> card = {{
            tripoint{ 1, 0, 0 }, tripoint{ -1, 0, 0 },
            tripoint{ 0, 1, 0 }, tripoint{ 0, -1, 0 }
        }
    };

    // ---- 1. Remove handles whose submap no longer has fire ----
    std::vector<dim_pos_key> no_fire;
    for( auto &[key, handle] : fire_handles_ ) {
        mapbuffer &mb = MAPBUFFER_REGISTRY.get( key.first );
        submap *sm = mb.lookup_submap_in_memory( key.second.raw() );
        // sm == nullptr means the submap hasn't been loaded yet — keep the handle
        // so submap_loader.update() gets a chance to load it.  Only prune once
        // the submap is resident in memory but no longer has fire.
        if( sm != nullptr && !submap_has_fire( *sm ) ) {
            no_fire.push_back( key );
        }
    }
    for( const dim_pos_key &key : no_fire ) {
        const auto it = fire_handles_.find( key );
        if( it != fire_handles_.end() ) {
            loader.release_load( it->second );
            fire_handles_.erase( it );
        }
    }

    // ---- 2. Connectivity: flood-fill from properly-loaded submaps ----
    // A fire-loaded submap is reachable if there is a cardinal path
    // through other fire-loaded submaps back to a properly-loaded one.
    std::set<dim_pos_key> reachable;
    std::vector<dim_pos_key> frontier;

    // Seed: fire handles that are directly adjacent to a properly-loaded submap.
    for( const auto &[key, handle] : fire_handles_ ) {
        for( const tripoint &delta : card ) {
            const tripoint_abs_sm nbr{ key.second.raw() + delta };
            if( loader.is_properly_requested( key.first, nbr ) ) {
                if( reachable.insert( key ).second ) {
                    frontier.push_back( key );
                }
                break;
            }
        }
    }

    // BFS: expand through cardinal fire-loaded neighbors.
    while( !frontier.empty() ) {
        const dim_pos_key cur = frontier.back();
        frontier.pop_back();
        for( const tripoint &delta : card ) {
            const dim_pos_key nbr_key{ cur.first, tripoint_abs_sm{ cur.second.raw() + delta } };
            if( fire_handles_.count( nbr_key ) && reachable.insert( nbr_key ).second ) {
                frontier.push_back( nbr_key );
            }
        }
    }

    // Release all fire handles that are not reachable.
    std::vector<dim_pos_key> to_release;
    for( const auto &[key, handle] : fire_handles_ ) {
        if( !reachable.count( key ) ) {
            to_release.push_back( key );
        }
    }
    for( const dim_pos_key &key : to_release ) {
        const auto it = fire_handles_.find( key );
        if( it != fire_handles_.end() ) {
            loader.release_load( it->second );
            fire_handles_.erase( it );
        }
    }
}
