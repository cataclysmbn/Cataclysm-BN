#include "batch_turns.h"

#include <algorithm>
#include <bitset>
#include <cstddef>
#include <ranges>

#include "profile.h"

#include "calendar.h"
#include "game_constants.h"
#include "field.h"
#include "field_type.h"
#include "item.h"
#include "submap.h"
#include "vehicle.h"

/**
 * Compute the expected number of intensity drops after adding @p elapsed_turns
 * of age to a field entry that currently has @p current_age and @p half_life.
 *
 * Uses a deterministic linear approximation of the probabilistic dice-based
 * decay: on average, one intensity level is lost every half_life turns.
 * The remaining age after the drops is written back into *out_remaining_age.
 *
 * Returns the number of drops (clamped so it never exceeds current_intensity).
 */
static int compute_field_decay( int current_intensity, int half_life_turns,
                                int current_age_turns, int elapsed_turns,
                                int &out_remaining_age )
{
    if( half_life_turns <= 0 || current_intensity <= 0 ) {
        out_remaining_age = current_age_turns + elapsed_turns;
        return 0;
    }
    const int total_age = current_age_turns + elapsed_turns;
    const int drops     = total_age / half_life_turns;
    out_remaining_age   = total_age % half_life_turns;
    return std::min( drops, current_intensity );
}

// Uses deterministic linear decay (one intensity drop per half_life turns)
// instead of the stochastic dice-roll in process_fields_in_submap.
// Accepted trade-off for performance; long-run averages converge.

void batch_turns_field( submap &sm, int n )
{
    ZoneScoped;
    if( n <= 0 || sm.field_count == 0 ) {
        return;
    }

    std::ranges::for_each( sm.field_cache, [&]( const point_sm_ms & local ) {
        field &curfield = sm.get_field( local );
        if( !curfield.displayed_field_type() ) {
            return;
        }
        for( auto it = curfield.begin(); it != curfield.end(); ) {
            field_entry &cur = it->second;

            // Dead entries clean up on next normal tick; leave them alone.
            if( !cur.is_field_alive() ) {
                ++it;
                continue;
            }

            const field_type &fdata = cur.get_field_type().obj();

            // Fire fields are never batch-decayed; they require real simulation
            // to avoid instant-kill and unsimulated structural damage on load.
            if( fdata.has_fire ) {
                ++it;
                continue;
            }

            if( fdata.half_life > 0_turns ) {
                const int hl           = to_turns<int>( fdata.half_life );
                const int current_age  = to_turns<int>( cur.get_field_age() );
                const int intensity    = cur.get_field_intensity();
                int remaining_age      = 0;
                const int drops        = compute_field_decay( intensity, hl,
                                         current_age, n, remaining_age );
                if( drops > 0 ) {
                    cur.set_field_intensity( intensity - drops );
                    cur.set_field_age( time_duration::from_turns( remaining_age ) );

                    if( !cur.is_field_alive() ) {
                        --sm.field_count;
                        curfield.remove_field( it++ );
                        continue;
                    }
                } else {
                    // Just age the field without decaying.
                    cur.mod_field_age( time_duration::from_turns( n ) );
                }
            } else {
                // No half-life: simply age the field.
                cur.mod_field_age( time_duration::from_turns( n ) );
            }
            ++it;
        }
    } );

    // Compact + deduplicate — mirrors the same fix in process_fields_in_submap.
    std::bitset<SEEX *SEEY> seen;
    sm.field_cache.erase(
    std::ranges::remove_if( sm.field_cache, [&]( const point_sm_ms & local ) {
        if( !sm.get_field( local ).displayed_field_type() ) {
            return true;
        }
        const auto idx = static_cast<std::size_t>( local.x() + local.y() * SEEX );
        if( seen.test( idx ) ) {
            return true;
        }
        seen.set( idx );
        return false;
    } ).begin(),
    sm.field_cache.end()
    );
}

void batch_turns_items( submap &sm, int n )
{
    ZoneScoped;
    if( n <= 0 || sm.active_items.empty() ) {
        return;
    }

    std::ranges::for_each(
    sm.active_items.get() | std::views::filter( []( const item * it ) {
        return it != nullptr && it->has_explicit_turn_timer();
    } ),
    [n]( item * it ) {
        it->advance_timer( n );
    }
    );
}

void run_submap_batch_turns( submap &sm, int n )
{
    ZoneScoped;
    TracyPlot( "Batch Turns N", static_cast<int64_t>( n ) );
    if( n <= 0 ) {
        return;
    }
    batch_turns_field( sm, n );
    batch_turns_items( sm, n );
    for( const auto &veh_ptr : sm.vehicles ) {
        if( veh_ptr ) {
            veh_ptr->update_time( calendar::turn );
        }
    }
}
