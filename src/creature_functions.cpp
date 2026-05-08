#include <algorithm>
#include <set>
#include <vector>

#include "creature_functions.h"
#include "avatar.h"
#include "game.h"
#include "map.h"
#include "map_iterator.h"
#include "line.h"
#include "vehicle.h"
#include "monster.h"
#include "npc.h"
#include "vpart_position.h"


namespace
{

// Helper function to check if potential area of effect of a weapon overlaps vehicle
// Maybe TODO: If this is too slow, precalculate a bounding box and clip the tested area to it
// TODO: make tripoint_range (and other iterators) to be range-compatible
auto overlaps_vehicle( const std::set<tripoint> &veh_area, const tripoint &pos,
                       const int area ) -> bool
{
    for( const tripoint &tmp : tripoint_range<tripoint>( pos - tripoint( area, area, 0 ),
            pos + tripoint( area - 1, area - 1, 0 ) ) ) {
        if( veh_area.contains( tmp ) ) {
            return true;
        }
    }
    return false;
}


} // namespace

namespace creature_functions
{

auto auto_find_hostile_target(
    const Creature &creature,
    const auto_find_hostile_target_option &option
) -> std::expected<std::reference_wrapper<Creature>, int>
{
    constexpr int hostile_adj = 2; // Priority bonus for hostile targets
    Creature *target = nullptr;
    auto &u = get_avatar(); // Could easily protect something that isn't the player
    // iff check triggers at this distance
    const int iff_dist = ( option.range + option.area ) * 3 / 2 + 6;
    // iff safety margin (degrees). less accuracy, more paranoia
    units::angle iff_hangle = units::from_degrees( 15 + option.area );
    float best_target_rating = -1.0f; // bigger is better
    int boo_hoo = 0;         // how many targets were passed due to IFF. Tragically.
    bool self_area_iff = false; // Need to check if the target is near the vehicle we're a part of
    map &here = get_map();
    vehicle *in_veh = creature.is_fake()
                      ? veh_pointer_or_null( here.veh_at( creature.pos() ) ) : nullptr;

    struct iff_guard_creature {
        const Creature *critter = nullptr;
        int dist = 0;
        bool area_iff = false;
        bool angle_iff = true;
        units::angle angle = {};
        units::angle iff_hangle = {};
    };

    auto protected_creatures = std::vector<iff_guard_creature> {};
    for( Creature *const critter : g->get_creatures_if( [&]( const Creature & other ) {
    return &other != &creature && creature.attitude_to( other ) == Attitude::A_FRIENDLY;
    } ) ) {
        const auto critter_dist = rl_dist( creature.pos(), critter->pos() );
        // Skip IFF for adjacent friendlies if weapon is safe (bullets/rockets protected by ballistics).
        // Always apply IFF for weapons with dangerous trails (lasers) even when adjacent.
        if( critter_dist >= iff_dist || ( !option.trail && critter_dist <= 1 ) ||
            !creature.sees( *critter ) ) {
            continue;
        }

        auto guard = iff_guard_creature{
            .critter = critter,
            .dist = critter_dist,
            .area_iff = option.area > 0,
            .angle = coord_to_angle( creature.pos(), critter->pos() ),
            .iff_hangle = iff_hangle,
        };

        // Occupants inside the same vehicle are safe from the turret's direct line of fire,
        // but still need AoE protection.
        const optional_vpart_position vp = here.veh_at( critter->pos() );
        if( in_veh && veh_pointer_or_null( vp ) == in_veh && vp->is_inside() ) {
            guard.angle_iff = false;
        } else if( critter_dist < 3 ) {
            // granularity increases with proximity
            guard.iff_hangle = critter_dist == 2 ? 30_degrees : 60_degrees;
        }

        protected_creatures.push_back( guard );
    }

    if( option.area > 0 && in_veh != nullptr ) {
        self_area_iff = true;
    }

    std::vector<Creature *> targets = g->get_creatures_if( [&]( const Creature & critter ) {
        if( critter.is_monster() ) {
            // friendly to the player, not a target for us
            return static_cast<const monster *>( &critter )->friendly == 0;
        }
        if( critter.is_npc() ) {
            // friendly to the player, not a target for us
            return static_cast<const npc *>( &critter )->guaranteed_hostile();
        }
        // TODO: what about g->u?
        return false;
    } );
    for( auto &m : targets ) {
        if( !creature.sees( *m ) ) {
            // can't see nor sense it
            if( creature.is_fake() && in_veh ) {
                // If turret in the vehicle then
                // Hack: trying yo avoid turret LOS blocking by frames bug by trying to see target from vehicle boundary
                // Or turret wallhack for turret's car
                // TODO: to visibility checking another way, probably using 3D FOV
                std::vector<tripoint> path_to_target = line_to( creature.pos(), m->pos() );
                path_to_target.insert( path_to_target.begin(), creature.pos() );

                // Getting point on vehicle boundaries and on line between target and turret
                bool continueFlag = true;
                do {
                    const optional_vpart_position vp = here.veh_at( path_to_target.back() );
                    vehicle *const veh = vp ? &vp->vehicle() : nullptr;
                    if( in_veh == veh ) {
                        continueFlag = false;
                    } else {
                        path_to_target.pop_back();
                    }
                } while( continueFlag );

                tripoint oldPos = creature.pos();
                const_cast<Creature &>( creature ).setpos(
                    path_to_target.back() ); //Temporary moving targeting npc on vehicle boundary postion
                bool seesFromVehBound = creature.sees( *m ); // And look from there
                const_cast<Creature &>( creature ).setpos( oldPos );
                if( !seesFromVehBound ) { continue; }
            } else { continue; }
        }
        int dist = rl_dist( creature.pos(), m->pos() ) + 1; // rl_dist can be 0
        if( dist > option.range + 1 || dist < option.area ) {
            // Too near or too far
            continue;
        }
        // Prioritize big, armed and hostile stuff
        float mon_rating = m->power_rating();
        float target_rating = mon_rating / dist;
        if( mon_rating + hostile_adj <= 0 ) {
            // We wouldn't attack it even if it was hostile
            continue;
        }

        if( in_veh != nullptr && veh_pointer_or_null( here.veh_at( m->pos() ) ) == in_veh ) {
            // No shooting stuff on vehicle we're a part of
            continue;
        }
        const auto target_angle = coord_to_angle( creature.pos(), m->pos() );
        const auto blocked_by_friendly = std::ranges::any_of( protected_creatures,
        [&]( const iff_guard_creature & guard ) {
            if( guard.area_iff && rl_dist( guard.critter->pos(), m->pos() ) <= option.area ) {
                return true;
            }
            if( !guard.angle_iff ) {
                return false;
            }

            const auto diff = units::fabs( guard.angle - target_angle );
            return ( diff + guard.iff_hangle > 360_degrees || diff < guard.iff_hangle ) &&
                   ( dist * 3 / 2 + 6 > guard.dist );
        } );
        if( !blocked_by_friendly && ( ( mon_rating + hostile_adj ) / dist <= best_target_rating ) ) {
            // "Would we skip the target even if it was hostile?"
            // Helps avoid (possibly expensive) attitude calculation
            continue;
        }
        if( m->attitude_to( u ) == Attitude::A_HOSTILE ) {
            target_rating = ( mon_rating + hostile_adj ) / dist;
            if( blocked_by_friendly ) {
                boo_hoo++;
                continue;
            }
        } else if( blocked_by_friendly ) {
            continue;
        }
        if( target_rating <= best_target_rating || target_rating <= 0 ) {
            continue; // Handle this late so that boo_hoo++ can happen
        }
        // Expensive check for proximity to vehicle
        if( self_area_iff && overlaps_vehicle( in_veh->get_points(), m->pos(), option.area ) ) {
            continue;
        }

        target = m;
        best_target_rating = target_rating;
    }
    return target ? std::expected<std::reference_wrapper<Creature>, int>( *target )
           : std::unexpected( boo_hoo );
}


} // namespace creature_functions
