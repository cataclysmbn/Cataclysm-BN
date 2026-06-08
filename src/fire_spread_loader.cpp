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

namespace
{

const auto cardinal_offsets = std::to_array( { tripoint_east, tripoint_west, tripoint_north, tripoint_south } );

/**
 * Return true if the submap at @p abs_sm_pos in @p mb has at least one live
 * fire field (fd_fire intensity >= 1).
 */
// NOTE: is_field_alive() is non-const in the current codebase, so we need a
// non-const submap reference here even though we only read.
auto submap_has_fire( submap &sm ) -> bool
{
    if( sm.field_count == 0 ) {
        return false;
    }
    return std::ranges::any_of( sm.field_cache, [&]( const point_sm_ms & local ) {
        auto *fe = sm.get_field( local ).find_field( fd_fire );
        return fe != nullptr && fe->is_field_alive();
    } );
}

} // namespace

auto fire_spread_loader::request_for_fire( const std::string &dim, tripoint_abs_sm pos ) -> void
{
    ZoneScoped;
    const auto key = dim_pos_key{ dim, pos };

    // Already tracked by this loader.
    if( fire_handles_.count( key ) ) {
        return;
    }

    // For positions inside the bubble: always register without applying the cap —
    // they are already loaded and cost no extra memory.  We need the fire_spread
    // request to exist *before* the bubble can shift away and trigger eviction.
    const auto in_bubble = submap_loader.is_properly_requested( dim, pos );
    if( !in_bubble ) {
        const auto adjacent_to_proper = std::ranges::any_of( cardinal_offsets,
        [&]( const auto & delta ) {
            const auto nbr = pos + delta;
            return submap_loader.is_properly_requested( dim, nbr );
        } );
        const auto adjacent_to_tracked = std::ranges::any_of( cardinal_offsets,
        [&]( const auto & delta ) {
            const auto nbr_key = dim_pos_key{ dim, pos + delta };
            return fire_handles_.contains( nbr_key );
        } );
        if( !adjacent_to_proper && !adjacent_to_tracked ) {
            return;
        }

        // Apply the cap only to genuinely out-of-bubble submaps.
        if( fire_spread_submap_cap <= 0 ||
            out_of_bubble_loaded_count() >= fire_spread_submap_cap ) {
            return;
        }
    }

    // Request a single omt (radius 0) — always covers full z-pillar.
    const auto h = submap_loader.request_load(
                       load_request_source::fire_spread,
                       dim,
                       pos,
                       0 );
    fire_handles_[key] = h;
}

auto fire_spread_loader::prune_disconnected( submap_load_manager &loader ) -> void
{
    ZoneScoped;
    TracyPlot( "Fire-Loaded Submaps", static_cast<int64_t>( loaded_count() ) );

    // ---- 1. Remove no-fire boundary handles only after their adjacent fire dies ----
    auto live_fire = std::set<dim_pos_key> {};
    auto no_fire = std::vector<dim_pos_key> {};
    std::ranges::for_each( fire_handles_, [&]( const auto & entry ) {
        const auto &key = entry.first;
        auto &mb = MAPBUFFER_REGISTRY.get( key.first );
        auto *sm = mb.lookup_submap_in_memory( key.second );
        // sm == nullptr means the submap hasn't been loaded yet — keep the handle
        // so submap_loader.update() gets a chance to load it.  Only prune once
        // the submap is resident in memory.
        if( sm == nullptr ) {
            return;
        }
        if( submap_has_fire( *sm ) ) {
            live_fire.insert( key );
        } else {
            no_fire.push_back( key );
        }
    } );

    const auto adjacent_to_live_fire = [&]( const dim_pos_key & key ) {
        return std::ranges::any_of( cardinal_offsets, [&]( const auto & delta ) {
            const auto nbr_key = dim_pos_key{ key.first, key.second + delta };
            return live_fire.contains( nbr_key );
        } );
    };
    std::ranges::for_each( no_fire, [&]( const dim_pos_key & key ) {
        if( adjacent_to_live_fire( key ) ) {
            return;
        }
        const auto it = fire_handles_.find( key );
        if( it != fire_handles_.end() ) {
            loader.release_load( it->second );
            fire_handles_.erase( it );
        }
    } );

    // ---- 2. Connectivity: flood-fill from properly-loaded submaps ----
    // A fire-loaded submap is reachable if there is a cardinal path
    // through other fire-loaded submaps back to a properly-loaded one.
    auto reachable = std::set<dim_pos_key> {};
    auto frontier = std::vector<dim_pos_key> {};

    // Seed: fire handles that are either properly loaded themselves or directly
    // adjacent to a properly-loaded submap.
    std::ranges::for_each( fire_handles_, [&]( const auto & entry ) {
        const auto &key = entry.first;
        if( loader.is_properly_requested( key.first, key.second ) ) {
            if( reachable.insert( key ).second ) {
                frontier.push_back( key );
            }
            return;
        }
        const auto adjacent_to_proper = std::ranges::any_of( cardinal_offsets,
        [&]( const auto & delta ) {
            const auto nbr = key.second + delta;
            return loader.is_properly_requested( key.first, nbr );
        } );
        if( adjacent_to_proper && reachable.insert( key ).second ) {
            frontier.push_back( key );
        }
    } );

    // BFS: expand through cardinal fire-loaded neighbors.
    while( !frontier.empty() ) {
        const auto cur = frontier.back();
        frontier.pop_back();
        for( const auto &delta : cardinal_offsets ) {
            const auto nbr_key = dim_pos_key{ cur.first, cur.second + delta };
            if( fire_handles_.contains( nbr_key ) && reachable.insert( nbr_key ).second ) {
                frontier.push_back( nbr_key );
            }
        }
    }

    // Release all fire handles that are not reachable.
    auto to_release = std::vector<dim_pos_key> {};
    std::ranges::for_each( fire_handles_, [&]( const auto & entry ) {
        const auto &key = entry.first;
        if( !reachable.contains( key ) ) {
            to_release.push_back( key );
        }
    } );
    for( const auto &key : to_release ) {
        const auto it = fire_handles_.find( key );
        if( it != fire_handles_.end() ) {
            loader.release_load( it->second );
            fire_handles_.erase( it );
        }
    }
}

auto fire_spread_loader::clear( submap_load_manager &loader ) -> void
{
    std::ranges::for_each( fire_handles_, [&]( const auto & entry ) {
        loader.release_load( entry.second );
    } );
    fire_handles_.clear();
}

auto fire_spread_loader::out_of_bubble_loaded_count() const -> int
{
    return static_cast<int>( std::ranges::count_if( fire_handles_, []( const auto & entry ) {
        return !submap_loader.is_properly_requested( entry.first.first, entry.first.second );
    } ) );
}
