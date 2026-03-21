#include "vehicle_functions.h"

#include <algorithm>
#include <cmath>
#include <ranges>
#include <vector>

#include "ammo.h"
#include "avatar.h"
#include "calendar.h"
#include "debug.h"
#include "game.h"
#include "item.h"
#include "messages.h"
#include "profile.h"
#include "sounds.h"
#include "translations.h"
#include "units.h"
#include "veh_type.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"
#include "vpart_range.h"

static const itype_id fuel_type_battery( "battery" );

namespace
{

/// Calculates the required time to reload based on mass
constexpr auto get_reload_duration( const units::mass weight ) -> time_duration
{
    const auto kg = units::to_kilogram( weight );
    // Minimum 6 seconds, or 2 seconds per kg
    const auto seconds = std::max( 6, static_cast<int>( std::ceil( kg ) * 2 ) );
    return time_duration::from_seconds( seconds );
}

/// Handles the complex state machine for reload delays and cycle starts
/// Returns true if reload can proceed immediately
auto check_reload_timing( item &gun, const item &ammo_item, bool is_ammo_refill, const vehicle &veh,
                          const std::string &turret_name ) -> bool
{
    const auto gun_was_empty = gun.ammo_remaining() == 0;

    // Determine the effective weight for calculation
    // For ammo stacks (integral load), we calculate per round. For magazines, the whole mag.
    const auto effective_weight = is_ammo_refill ?
                                  ( ammo_item.weight() / ammo_item.count() ) :
                                  ammo_item.weight();

    const auto reload_interval = get_reload_duration( effective_weight );

    // 1. Handling empty gun startup delay (Cycle logic)
    if( gun_was_empty ) {
        const int64_t cycle_start = gun.get_var( "autoloader_cycle_start", 0LL );

        // State: First detection of ammo
        if( cycle_start == 0LL ) {
            const int64_t current_turn = to_turns<int64_t>( calendar::turn - calendar::turn_zero );
            gun.set_var( "autoloader_cycle_start", current_turn );

            add_msg( m_debug, "Autoload: Empty gun found ammo, starting cycle at turn %lld", current_turn );

            if( get_avatar().sees( veh.global_pos3() ) ) {
                add_msg( _( "The %1$s's autoloader begins reloading %2$s into %3$s." ),
                         veh.name, ammo_item.tname(), turret_name );
                sfx::play_variant_sound( "reload", "start", sfx::get_heard_volume( veh.global_pos3() ) );
            }
            return false; // Cycle started, wait for next turn(s)
        }

        // State: Waiting for cycle to complete
        const time_point cycle_start_time = time_point::from_turn( static_cast<int>( cycle_start ) );
        if( calendar::turn < cycle_start_time + reload_interval ) {
            // add_msg( m_debug, "Autoload: Cycle in progress... remaining: %ds",
            //          to_seconds<int>( ( cycle_start_time + reload_interval ) - calendar::turn ) );
            return false;
        }

        add_msg( m_debug, "Autoload: Cycle complete, proceeding to reload." );
        return true;
    }

    // 2. Handling standard cooldown (Inter-shot/Inter-mag delay)
    const time_point last_reload = time_point::from_turn(
                                       static_cast<int>( gun.get_var( "last_autoload_turn", 0LL ) )
                                   );

    return calendar::turn >= last_reload + reload_interval;
}

// Executes the physical reload, battery drain, and effects
void perform_reload( vehicle &veh, vehicle_part &cargo_part, item &gun, item &source_item,
                     const std::string &turret_name, int power_cost, bool is_magazine )
{
    const auto item_name = source_item.tname();

    // Action: Move item or rounds
    if( is_magazine ) {
        // Logic to remove old empty mag if present
        if( gun.magazine_current() ) {
            gun.contents.remove_top_items_with( [&]( detached_ptr<item> &&mag ) -> detached_ptr<item> {
                if( mag->is_magazine() )
                {
                    cargo_part.add_item( std::move( mag ) );
                    return detached_ptr<item>(); // Remove from gun
                }
                return std::move( mag ); // Keep in gun (shouldn't happen for mag)
            } );
        }

        // Move new mag
        auto mag = cargo_part.remove_item( source_item );
        gun.put_in( std::move( mag ) );

    } else {
        // Load single round
        gun.ammo_set( source_item.typeId(), gun.ammo_remaining() + 1 );
        if( source_item.charges <= 1 ) {
            cargo_part.remove_item( source_item );
        } else {
            source_item.charges -= 1;
        }
    }

    // State update
    veh.drain( fuel_type_battery, power_cost );
    gun.set_var( "last_autoload_turn", to_turns<int64_t>( calendar::turn - calendar::turn_zero ) );
    gun.set_var( "autoloader_cycle_start", 0LL ); // Reset cycle

    // Feedback
    if( get_avatar().sees( veh.global_pos3() ) ) {
        add_msg( _( "The %1$s's autoloader reloaded %2$s into %3$s." ),
                 veh.name, item_name, turret_name );
        sfx::play_variant_sound( "reload", "end", sfx::get_heard_volume( veh.global_pos3() ) );
    }
}

} // anonymous namespace

namespace vehicle_funcs
{

auto try_autoload_turret( vehicle &veh, vehicle_part &pt ) -> bool
{
    ZoneScoped;

    if( pt.info().has_flag( "USE_TANKS" ) ) {
        return false;
    }

    item &gun = pt.get_base();
    const auto ammo_capacity = gun.ammo_capacity();
    const auto needs_reload = gun.magazine_integral()
                              ? ( ammo_capacity > 0 && gun.ammo_remaining() < ammo_capacity )
                              : !gun.ammo_sufficient();
    if( !needs_reload ) {
        gun.set_var( "autoloader_cycle_start", 0LL );
        return false;
    }

    // Identify Autoloader part and power
    const auto parts_here = veh.parts_at_relative( pt.mount, false );
    const auto autoloader_it = std::ranges::find_if( parts_here, [&veh]( int idx ) {
        const vehicle_part &vp = veh.cpart( idx );
        return !vp.is_broken() && vp.info().has_flag( "AUTOLOADER" );
    } );

    if( autoloader_it == parts_here.end() ) {
        return false;
    }

    const vehicle_part &autoloader_part = veh.cpart( *autoloader_it );
    const int autoloader_power = -autoloader_part.info().epower;

    if( veh.fuel_left( fuel_type_battery, true ) < autoloader_power ) {
        return false;
    }

    // Maintain "Last Ammo" state to detect empty transitions
    const bool gun_is_empty = gun.ammo_remaining() == 0;
    const int64_t last_ammo_check = gun.get_var( "autoloader_last_ammo", -1LL );

    if( gun_is_empty && last_ammo_check != 0LL ) {
        gun.set_var( "autoloader_last_ammo", 0LL );
        gun.set_var( "autoloader_cycle_start", 0LL );
        add_msg( m_debug, "try_autoload_turret: Gun became empty, reset cycle" );
    } else if( !gun_is_empty ) {
        gun.set_var( "autoloader_last_ammo", gun.ammo_remaining() );
    }

    // Search cargo for compatible items
    for( const vpart_reference &vpr : veh.get_any_parts( VPFLAG_CARGO ) ) {
        vehicle_part &cargo_part = vpr.part();
        if( cargo_part.is_broken() ) { continue; }

        for( item *it : cargo_part.get_items() ) {
            const bool is_mag = !gun.magazine_integral() && it->is_magazine();
            const bool is_ammo = gun.magazine_integral() && it->is_ammo();

            // Check compatibility
            if( !is_mag && !is_ammo ) { continue; }
            if( !gun.can_reload_with( it->typeId() ) ) { continue; }
            if( is_mag && it->ammo_remaining() <= 0 ) { continue; }

            // Check timing/state (Side effect: may set cycle_start)
            if( !check_reload_timing( gun, *it, is_ammo, veh, pt.name() ) ) {
                // If we found a compatible item but are waiting, stop looking to avoid
                // finding another item and starting a parallel cycle or confusion.
                return false;
            }

            // Perform reload
            perform_reload( veh, cargo_part, gun, *it, pt.name(), autoloader_power, is_mag );
            return true;
        }
    }

    return false;
}

void process_autoloaders( vehicle &veh )
{
    ZoneScoped;

    for( const int i : std::views::iota( 0, veh.part_count() ) ) {
        auto &autoloader = veh.part( i );

        if( autoloader.is_unavailable() || !autoloader.info().has_flag( "AUTOLOADER" ) ) {
            continue;
        }

        const int power_cost = -autoloader.info().epower;
        if( veh.fuel_left( fuel_type_battery, true ) < power_cost ) {
            continue;
        }

        // Find associated turret
        const auto parts_here = veh.parts_at_relative( autoloader.mount, false );
        auto turret_it = std::ranges::find_if( parts_here,
        [&veh]( int idx ) {
            const auto &p = veh.cpart( idx );
            return !p.is_broken() && p.is_turret();
        } );

        if( turret_it != parts_here.end() ) {
            try_autoload_turret( veh, veh.part( *turret_it ) );
        }
    }
}

} // namespace vehicle_funcs
