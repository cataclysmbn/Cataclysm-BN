#include "batch_turns.h"

#include <algorithm>
#include <climits>
#include <cstdint>
#include <ranges>

#include "calendar.h"
#include "distribution_grid.h"
#include "field.h"
#include "field_type.h"
#include "item.h"
#include "submap.h"
#include "units.h"
#include "vehicle.h"

// ---------------------------------------------------------------------------
// Internal helper
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// batch_turns_field
// ---------------------------------------------------------------------------
// NOTE: Divergence from tick_submap (intentional approximation)
// ---------------------------------------------------------------
// tick_submap (world_tick_interval_ == 1) uses the canonical stochastic
// dice-roll decay from map::process_fields_in_submap():
//
//   if( fdata.half_life > 0 && dice(2, age) > half_life )
//       intensity--;
//
// batch_turns_field (world_tick_interval_ > 1) uses compute_field_decay(),
// a deterministic linear approximation: one intensity drop per half_life
// turns elapsed.  Over large N the expected number of drops is the same,
// but individual fields will not follow the exact probabilistic curve.
//
// This divergence is accepted for performance: the stochastic path requires
// one dice-roll per field per elapsed turn, whereas the deterministic path
// requires only integer division.  Fields with very short half-lives (<10 t)
// may decay slightly faster or slower than the stochastic path at low N,
// but the long-run averages converge.

void batch_turns_field( submap &sm, int n )
{
    if( n <= 0 || sm.field_count == 0 ) {
        return;
    }
    n = std::min( n, MAX_CATCHUP_FIELDS );

    for( int x = 0; x < SEEX; ++x ) {
        for( int y = 0; y < SEEY; ++y ) {
            field &curfield = sm.get_field( { x, y } );
            if( !curfield.displayed_field_type() ) {
                continue;
            }
            for( auto it = curfield.begin(); it != curfield.end(); ) {
                field_entry &cur = it->second;

                // Dead entries clean up on next normal tick; leave them alone.
                if( !cur.is_field_alive() ) {
                    ++it;
                    continue;
                }

                const field_type &fdata = cur.get_field_type().obj();
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
        }
    }
}

// ---------------------------------------------------------------------------
// batch_turns_items
// ---------------------------------------------------------------------------

void batch_turns_items( submap &sm, int n )
{
    if( n <= 0 || sm.active_items.empty() ) {
        return;
    }
    n = std::min( n, MAX_CATCHUP_ITEMS );

    std::ranges::for_each(
        sm.active_items.get() | std::views::filter( []( const item *it ) {
            return it != nullptr && it->has_explicit_turn_timer();
        } ),
        [n]( item *it ) {
            it->advance_timer( n );
        }
    );
}

// ---------------------------------------------------------------------------
// batch_turns_vehicle
// ---------------------------------------------------------------------------

void batch_turns_vehicle( vehicle &veh, int n )
{
    if( n <= 0 ) {
        return;
    }
    n = std::min( n, MAX_CATCHUP_VEHICLE );

    // net_battery_charge_rate_w() returns watts (positive = charging, negative = discharging).
    // Each game turn is 1 second, so watts == watt-turns per turn.
    const int net_per_turn = veh.net_battery_charge_rate_w();
    if( net_per_turn == 0 ) {
        return;
    }

    const int64_t total = static_cast<int64_t>( net_per_turn ) * n;
    // Clamp to int range for the charge/discharge API.
    constexpr int64_t INT_MAX_64 = static_cast<int64_t>( INT_MAX );
    if( total > 0 ) {
        const int charge = static_cast<int>( std::min( total, INT_MAX_64 ) );
        veh.charge_battery( charge, /*include_other_vehicles=*/false );
    } else {
        const int discharge = static_cast<int>( std::min( -total, INT_MAX_64 ) );
        veh.discharge_battery( discharge, /*recurse=*/false );
    }
}

// ---------------------------------------------------------------------------
// batch_turns_distribution_grid
// ---------------------------------------------------------------------------

void batch_turns_distribution_grid( distribution_grid &grid, int n )
{
    if( n <= 0 ) {
        return;
    }
    n = std::min( n, MAX_CATCHUP_GRID );

    const auto stat = grid.get_power_stat();
    // Positive means net generation (charge batteries), negative means net drain.
    const int64_t net_per_turn = static_cast<int64_t>( stat.gen_w ) -
                                 static_cast<int64_t>( stat.use_w );
    if( net_per_turn == 0 ) {
        return;
    }

    const int64_t delta = net_per_turn * static_cast<int64_t>( n );
    grid.apply_net_power( delta );
}

// ---------------------------------------------------------------------------
// run_submap_batch_turns
// ---------------------------------------------------------------------------

void run_submap_batch_turns( submap &sm, int n )
{
    if( n <= 0 ) {
        return;
    }
    batch_turns_field( sm, n );
    batch_turns_items( sm, n );
    for( const auto &veh_ptr : sm.vehicles ) {
        if( veh_ptr ) {
            batch_turns_vehicle( *veh_ptr, n );
        }
    }
}
