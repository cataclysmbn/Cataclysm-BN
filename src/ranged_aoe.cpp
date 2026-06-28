#include <queue>

#include "enums.h"
#include "game.h"
#include "line.h"
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
    tripoint_distance( const tripoint_abs_ms &p, int distance_squared )
        : p( p )
        , distance_squared( distance_squared )
    {}
    tripoint_distance( const tripoint_distance & ) = default;
    tripoint_distance &operator = ( const tripoint_distance & ) = default;
    tripoint_abs_ms p;
    int distance_squared;

    // Inverted because it's descending by default
    bool operator<( const tripoint_distance &rhs ) const {
        return rhs.distance_squared < this->distance_squared;
    }
};

struct aoe_flood_node {
    aoe_flood_node() = default;
    aoe_flood_node( tripoint_abs_ms parent, double parent_coverage )
        : parent( parent ), parent_coverage( parent_coverage )
    {}
    aoe_flood_node( const aoe_flood_node & ) = default;
    aoe_flood_node &operator = ( const aoe_flood_node & ) = default;
    tripoint_abs_ms parent = tripoint_abs_ms::min();
    double parent_coverage = 0.0;
};

namespace ranged
{

void execute_shaped_attack( const shape &sh, const projectile &proj, Creature &attacker,
                            item *source_weapon, const vehicle *in_veh )
{
    auto &here = attacker.get_mapbuffer();
    const auto sigdist_to_coverage = []( const double sigdist ) {
        return std::min( 1.0, -sigdist );
    };
    const auto aoe_permeable = [&here]( const tripoint_abs_ms & p ) {
        auto tile = abs_tile_handle::fetch( here, p );
        return tile->passable() ||
               // Necessary evil. TODO: Make map::shoot not evil.
               ( here.is_transparent( p ) && tile->has_flag_ter( TFLAG_PERMEABLE ) );
    };
    // Raw tripoints are awful
    const auto &origin = tripoint_abs_ms( sh.get_origin() );
    std::priority_queue<tripoint_distance> queue;
    std::map<tripoint_abs_ms, aoe_flood_node> open;
    std::set<tripoint_abs_ms> closed;

    for( const auto &child : simulated_tiles_in_radius( here, origin, 1 ) ) {
        double coverage = sigdist_to_coverage( sh.distance_at( child.abs_pos().raw() ) );
        if( coverage > 0.0 && !here.obstructed_by_vehicle_rotation( origin, child.abs_pos() ) ) {
            open[child.abs_pos()] = aoe_flood_node( origin, 1.0 );
            queue.emplace( child.abs_pos(), trig_dist_squared( origin, child.abs_pos() ) );
        }
    }

    open[origin] = aoe_flood_node( origin, 1.0 );

    std::map<tripoint_abs_ms, double> final_coverage;
    while( !queue.empty() ) {
        auto p = queue.top().p;
        auto tile = abs_tile_handle::fetch( here, p );
        queue.pop();
        if( !tile || closed.contains( p ) ) {
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
            g->m.shoot( abs_to_bub( p ), abs_to_bub( p ), proj_copy, false );
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
            for( const auto &child : simulated_tiles_in_radius( here, p, 1 ) ) {
                double coverage = sigdist_to_coverage( sh.distance_at( child.abs_pos().raw() ) );
                if( coverage > 0.0 && !here.obstructed_by_vehicle_rotation( p, child.abs_pos() ) &&
                    !closed.contains( child.abs_pos() ) &&
                    ( !open.contains( child.abs_pos() ) ||
                      open.at( child.abs_pos() ).parent_coverage < current_coverage ) ) {
                    open[child.abs_pos()] = aoe_flood_node( p, current_coverage );
                    queue.emplace( child.abs_pos(), trig_dist_squared( origin, child.abs_pos() ) );
                }
            }

            final_coverage[p] = current_coverage;
        }
    }

    std::map<tripoint_bub_ms, double> drawn_coverage;
    for( const auto &[point, coverage] : final_coverage ) {
        drawn_coverage[abs_to_bub( point )] = coverage;
    }

    draw_cone_aoe( abs_to_bub( origin ), drawn_coverage );

    for( const auto &[point, coverage] : final_coverage ) {
        apply_ammo_trail_effects( point, proj.get_ammo_effects(), coverage );
    }

    // Here and not above because we want the animation first
    // Terrain will be shown damaged, but having it in unknown state would complicate timing the animation
    for( const auto &[point, coverage] : final_coverage ) {
        Creature *critter = g->critter_at( point );
        // Skip friendly creatures within 1 tile of attacker to prevent adjacent friendly fire in AOE
        if( critter != nullptr &&
            attacker.attitude_to( *critter ) == Attitude::A_FRIENDLY &&
            rl_dist( attacker.abs_pos(), point ) <= 1 ) {
            continue;
        }
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
std::map<tripoint_abs_ms, double> expected_coverage( const shape &sh, mapbuffer &here,
        int bash_power )
{
    const auto sigdist_to_coverage = []( const double sigdist ) {
        return std::min( 1.0, -sigdist );
    };
    const auto &origin = tripoint_abs_ms( sh.get_origin() );
    std::priority_queue<tripoint_distance> queue;
    std::map<tripoint_abs_ms, aoe_flood_node> open;
    std::set<tripoint_abs_ms> closed;

    for( const auto &child : simulated_tiles_in_radius( here, origin, 1 ) ) {
        double coverage = sigdist_to_coverage( sh.distance_at( child.abs_pos().raw() ) );
        if( coverage > 0.0 && !here.obstructed_by_vehicle_rotation( origin, child.abs_pos() ) ) {
            open[child.abs_pos()] = aoe_flood_node( origin, 1.0 );
            queue.emplace( child.abs_pos(), trig_dist_squared( origin, child.abs_pos() ) );
        }
    }

    open[origin] = aoe_flood_node( origin, 1.0 );

    std::map<tripoint_abs_ms, double> final_coverage;
    while( !queue.empty() ) {
        auto p = queue.top().p;
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
        const auto &tile = *abs_tile_handle::fetch( here, p );
        if( tile.passable() ||
            // Necessary evil. TODO: Make map::shoot not evil.
            ( here.is_transparent( p ) && tile.has_flag( TFLAG_PERMEABLE ) ) ) {
            // noop
        } else {
            int bash_str = tile.bash_strength();
            int bash_res = tile.bash_resistance();
            if( bash_power < bash_res ) {
                continue;
            }
            int range_width = bash_str - bash_res + 1;
            int fail_width = bash_str - bash_power;
            double fail_chance = static_cast<double>( fail_width ) / ( range_width );
            current_coverage *= 1.0 - std::max( 0.0, fail_chance );
        }

        if( tile.vehicle_part() ) {
            // If a vehicle part is blocking, assume it's indestructible
            continue;
        }

        if( current_coverage > 0.0 ) {
            for( const auto &child : simulated_tiles_in_radius( here, p, 1 ) ) {
                double coverage = sigdist_to_coverage( sh.distance_at( child.abs_pos().raw() ) );
                if( coverage > 0.0 && !here.obstructed_by_vehicle_rotation( p, child.abs_pos() ) &&
                    !closed.contains( child.abs_pos() ) &&
                    ( !open.contains( child.abs_pos() ) ||
                      open.at( child.abs_pos() ).parent_coverage < current_coverage ) ) {
                    open[child.abs_pos()] = aoe_flood_node( p, current_coverage );
                    queue.emplace( child.abs_pos(), trig_dist_squared( origin, child.abs_pos() ) );
                }
            }
            final_coverage[p] = current_coverage;
        }
    }

    final_coverage.erase( origin );
    return final_coverage;
}

} // namespace ranged
