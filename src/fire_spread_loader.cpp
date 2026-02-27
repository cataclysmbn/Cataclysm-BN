#include "fire_spread_loader.h"

#include <array>

#include "field.h"
#include "field_type.h"
#include "game_constants.h"
#include "mapbuffer.h"   // also pulls in mapbuffer_registry.h
#include "point.h"
#include "submap.h"

fire_spread_loader fire_loader;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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
    for( int x = 0; x < SEEX; ++x ) {
        for( int y = 0; y < SEEY; ++y ) {
            field &fld = sm.get_field( { x, y } );
            field_entry *fe = fld.find_field( fd_fire );
            if( fe != nullptr && fe->is_field_alive() ) {
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// fire_spread_loader public API
// ---------------------------------------------------------------------------

void fire_spread_loader::request_for_fire( const std::string &dim, tripoint_abs_sm pos )
{
    // Respect the global ceiling.
    if( loaded_count() >= FIRE_SPREAD_CAP ) {
        return;
    }

    dim_pos_key key{ dim, pos };

    // Already tracked by this loader.
    if( fire_handles_.count( key ) ) {
        return;
    }

    // Already covered by a non-fire_spread (properly-loaded) request — no
    // need to add a weaker fire-spread request on top.
    if( submap_loader.is_properly_requested( dim, pos ) ) {
        return;
    }

    // Request a single submap (radius 0) at the given z-level.
    const int z = pos.z();
    load_request_handle h = submap_loader.request_load(
                                load_request_source::fire_spread,
                                dim,
                                pos,
                                0,   // radius 0 → single submap
                                z,
                                z
                            );
    fire_handles_[key] = h;
}

void fire_spread_loader::prune_disconnected( submap_load_manager &loader )
{
    // Cardinal offsets for neighbor checks.
    static const std::array<tripoint, 4> card = {{
            tripoint{ 1, 0, 0 }, tripoint{ -1, 0, 0 },
            tripoint{ 0, 1, 0 }, tripoint{ 0, -1, 0 }
        }
    };

    std::vector<dim_pos_key> to_release;

    for( auto &[key, handle] : fire_handles_ ) {
        const std::string &dim = key.first;
        const tripoint_abs_sm &pos = key.second;

        // ---- 1. Check if the submap still has fire ----
        mapbuffer &mb = MAPBUFFER_REGISTRY.get( dim );
        submap *sm = mb.lookup_submap_in_memory( pos.raw() );
        if( sm == nullptr || !submap_has_fire( *sm ) ) {
            to_release.push_back( key );
            continue;
        }

        // ---- 2. Connectivity invariant ----
        // The fire-loaded submap must have at least one cardinal neighbour
        // that is properly requested (reality_bubble / player_base / script).
        bool connected = false;
        for( const tripoint &delta : card ) {
            const tripoint_abs_sm nbr{ pos.raw() + delta };
            if( loader.is_properly_requested( dim, nbr ) ) {
                connected = true;
                break;
            }
        }
        if( !connected ) {
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
