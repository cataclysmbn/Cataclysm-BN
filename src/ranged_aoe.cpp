#include <queue>

#include "game.h"
#include "map.h"
#include "map_iterator.h"
#include "projectile.h"
#include "ranged.h"
#include "rng.h"
#include "shape.h"
#include "vehicle.h"
#include "vehicle_part.h"
#include "vpart_position.h"

#include "explosion.h"

struct tripoint_distance {
    tripoint_distance( const tripoint &p, int distance_squared )
        : p( p )
        , distance_squared( distance_squared )
    {}
    tripoint_distance( const tripoint_distance & ) = default;
    tripoint_distance &operator = ( const tripoint_distance & ) = default;
    tripoint p;
    int distance_squared;

    // Inverted because it's descending by default
    bool operator<( const tripoint_distance &rhs ) const {
        return rhs.distance_squared < this->distance_squared;
    }
};

struct aoe_flood_node {
    aoe_flood_node() = default;
    aoe_flood_node( tripoint parent, double parent_coverage )
        : parent( parent ), parent_coverage( parent_coverage )
    {}
    aoe_flood_node( const aoe_flood_node & ) = default;
    aoe_flood_node &operator = ( const aoe_flood_node & ) = default;
    tripoint parent = tripoint_min;
    double parent_coverage = 0.0;
};

namespace ranged
{

void execute_shaped_attack( const shape &sh, const projectile &proj, Creature &attacker,
                            item *source_weapon, const vehicle *in_veh )
{
    map &here = get_map();
    const auto sigdist_to_coverage = []( const double sigdist ) {
        return std::min( 1.0, -sigdist );
    };
    const auto aoe_permeable = [&here]( const tripoint & p ) {
        return here.passable( p ) ||
               // Necessary evil. TODO: Make map::shoot not evil.
               ( here.is_transparent( p ) && here.has_flag_ter( TFLAG_PERMEABLE, p ) );
    };
    const tripoint &origin = sh.get_origin();
    std::priority_queue<tripoint_distance> queue;
    std::map<tripoint, aoe_flood_node> open;
    std::set<tripoint> closed;

    for( const tripoint &child : here.points_in_radius( origin, 1 ) ) {
        double coverage = sigdist_to_coverage( sh.distance_at( child ) );
        if( coverage > 0.0 && !get_map().obstructed_by_vehicle_rotation( origin, child ) ) {
            open[child] = aoe_flood_node( origin, 1.0 );
            queue.emplace( child, trig_dist_squared( origin, child ) );
        }
    }

    open[origin] = aoe_flood_node( origin, 1.0 );

    std::map<tripoint, double> final_coverage;
    while( !queue.empty() ) {
        tripoint p = queue.top().p;
        queue.pop();
        if( closed.contains( p ) || !here.inbounds( p ) ) {
            continue;
        }
        closed.insert( p );
        double parent_coverage = open.at( p ).parent_coverage;
        if( parent_coverage <= 0.0 ) {
            continue;
        }

        double current_coverage = parent_coverage;
        bool firing_over_veh = false;
        if( in_veh != nullptr ) {
            const optional_vpart_position other = here.veh_at( p );
            if( in_veh == veh_pointer_or_null( other ) ) {
                // Don't blast a vehicle with its own turret
                firing_over_veh = true;
            }
        }
        if( aoe_permeable( p ) || firing_over_veh ) {
            // noop
        } else {
            projectile proj_copy = proj;
            // Origin and target are same point so AoE can bypass cover mechanics
            here.shoot( p, p, proj_copy, false );
            // There should be a nicer way than rechecking after shoot
            if( !aoe_permeable( p ) && !firing_over_veh ) {
                continue;
            }

            float total_dmg = proj_copy.impact.total_damage();
            float old_total_dmg = proj.impact.total_damage();
            if( old_total_dmg != total_dmg ) {
                current_coverage *= std::min( 1.0f, old_total_dmg / total_dmg );
            }
        }

        if( current_coverage > 0.0 ) {
            for( const tripoint &child : here.points_in_radius( p, 1 ) ) {
                double coverage = sigdist_to_coverage( sh.distance_at( child ) );
                if( coverage > 0.0 && !get_map().obstructed_by_vehicle_rotation( p, child ) &&
                    !closed.contains( child ) &&
                    ( !open.contains( child ) || open.at( child ).parent_coverage < current_coverage ) ) {
                    open[child] = aoe_flood_node( p, current_coverage );
                    queue.emplace( child, trig_dist_squared( origin, child ) );
                }
            }

            final_coverage[p] = current_coverage;
        }
    }

    draw_cone_aoe( origin, final_coverage );

    // Here and not above because we want the animation first
    // Terrain will be shown damaged, but having it in unknown state would complicate timing the animation
    for( const auto &[point, coverage] : final_coverage ) {
        Creature *critter = g->critter_at( point );
        if( critter != nullptr ) {
            dealt_projectile_attack atk;
            atk.end_point = point;
            atk.hit_critter = critter;
            atk.proj = proj;
            atk.missed_by = rng_float( 0.15, 0.45 );
            critter->deal_projectile_attack( &attacker, source_weapon, atk );
        }
    }
}

// TODO: Make this not a CTRL+C+V
std::map<tripoint, double> expected_coverage( const shape &sh, const map &here, int bash_power )
{
    const auto sigdist_to_coverage = []( const double sigdist ) {
        return std::min( 1.0, -sigdist );
    };
    const tripoint &origin = sh.get_origin();
    std::priority_queue<tripoint_distance> queue;
    std::map<tripoint, aoe_flood_node> open;
    std::set<tripoint> closed;

    for( const tripoint &child : here.points_in_radius( origin, 1 ) ) {
        double coverage = sigdist_to_coverage( sh.distance_at( child ) );
        if( coverage > 0.0 && !get_map().obstructed_by_vehicle_rotation( origin, child ) ) {
            open[child] = aoe_flood_node( origin, 1.0 );
            queue.emplace( child, trig_dist_squared( origin, child ) );
        }
    }

    open[origin] = aoe_flood_node( origin, 1.0 );

    std::map<tripoint, double> final_coverage;
    while( !queue.empty() ) {
        tripoint p = queue.top().p;
        queue.pop();
        if( closed.contains( p ) ) {
            continue;
        }
        closed.insert( p );
        double parent_coverage = open.at( p ).parent_coverage;
        if( parent_coverage <= 0.0 ) {
            continue;
        }

        double current_coverage = parent_coverage;
        if( here.passable( p ) ||
            // Necessary evil. TODO: Make map::shoot not evil.
            ( here.is_transparent( p ) && here.has_flag_ter( TFLAG_PERMEABLE, p ) ) ) {
            // noop
        } else {
            int bash_str = here.bash_strength( p );
            int bash_res = here.bash_resistance( p );
            if( bash_power < bash_res ) {
                continue;
            }
            int range_width = bash_str - bash_res + 1;
            int fail_width = bash_str - bash_power;
            double fail_chance = static_cast<double>( fail_width ) / ( range_width );
            current_coverage *= 1.0 - std::max( 0.0, fail_chance );
        }

        if( here.veh_at( p ) ) {
            // If a vehicle part is blocking, assume it's indestructible
            continue;
        }

        if( current_coverage > 0.0 ) {
            for( const tripoint &child : here.points_in_radius( p, 1 ) ) {
                double coverage = sigdist_to_coverage( sh.distance_at( child ) );
                if( coverage > 0.0 && !get_map().obstructed_by_vehicle_rotation( p, child ) &&
                    !closed.contains( child ) &&
                    ( !open.contains( child ) || open.at( child ).parent_coverage < current_coverage ) ) {
                    open[child] = aoe_flood_node( p, current_coverage );
                    queue.emplace( child, trig_dist_squared( origin, child ) );
                }
            }
            final_coverage[p] = current_coverage;
        }
    }

    final_coverage.erase( origin );
    return final_coverage;
}

} // namespace ranged
