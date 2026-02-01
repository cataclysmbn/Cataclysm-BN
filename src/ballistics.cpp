#include "ballistics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "animation.h"
#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#if defined(TILES)
#   include "cata_tiles.h"
#   include "sdltiles.h"
#endif // TILES
#include "cata_utility.h" // for normal_cdf
#include "creature.h"
#include "damage.h"
#include "debug.h"
#include "dispersion.h"
#include "enums.h"
#include "explosion_queue.h"
#include "game.h"
#include "item.h"
#include "line.h"
#include "map.h"
#include "messages.h"
#include "monster.h"
#include "mtype.h"
#include "options.h"
#include "projectile.h"
#include "rng.h"
#include "sounds.h"
#include "translations.h"
#include "trap.h"
#include "type_id.h"
#include "units.h"
#include "visitable.h"
#include "vpart_position.h"

static const ammo_effect_str_id ammo_effect_ACT_ON_RANGED_HIT( "ACT_ON_RANGED_HIT" );
static const ammo_effect_str_id ammo_effect_BOUNCE( "BOUNCE" );
static const ammo_effect_str_id ammo_effect_BURST( "BURST" );
static const ammo_effect_str_id ammo_effect_DRAW_AS_LINE( "DRAW_AS_LINE" );
static const ammo_effect_str_id ammo_effect_HEAVY_HIT( "HEAVY_HIT" );
static const ammo_effect_str_id ammo_effect_JET( "JET" );
static const ammo_effect_str_id ammo_effect_MUZZLE_SMOKE( "MUZZLE_SMOKE" );
static const ammo_effect_str_id ammo_effect_NO_EMBED( "NO_EMBED" );
static const ammo_effect_str_id ammo_effect_NO_ITEM_DAMAGE( "NO_ITEM_DAMAGE" );
static const ammo_effect_str_id ammo_effect_NO_OVERSHOOT( "NO_OVERSHOOT" );
static const ammo_effect_str_id ammo_effect_NO_PENETRATE_OBSTACLES( "NO_PENETRATE_OBSTACLES" );
static const ammo_effect_str_id ammo_effect_NULL_SOURCE( "NULL_SOURCE" );
static const ammo_effect_str_id ammo_effect_SHATTER_SELF( "SHATTER_SELF" );
static const ammo_effect_str_id ammo_effect_STREAM( "STREAM" );
static const ammo_effect_str_id ammo_effect_STREAM_BIG( "STREAM_BIG" );
static const ammo_effect_str_id ammo_effect_TANGLE( "TANGLE" );
static const ammo_effect_str_id ammo_effect_THROWN( "THROWN" );

static const efftype_id effect_bounced( "bounced" );

static const std::string flag_LIQUID( "LIQUID" );
static const std::string flag_THIN_OBSTACLE( "THIN_OBSTACLE" );

static const flag_id flag_FLY_STRAIGHT( "FLY_STRAIGHT" );

namespace
{

void drop_or_embed_projectile( dealt_projectile_attack &attack )
{
    auto &proj = attack.proj;
    detached_ptr<item> drop = proj.unset_drop();
    if( !drop ) {
        return;
    }
    item &drop_item = *drop;
    if( drop_item.is_null() ) {
        return;
    }

    const tripoint &pt = attack.end_point;

    if( proj.has_effect( ammo_effect_SHATTER_SELF ) ) {
        // Drop the contents, not the thrown item
        if( g->u.sees( pt ) ) {
            add_msg( _( "The %s shatters!" ), drop_item.tname() );
        }

        // copies the drop item to spill the contents
        drop_item.spill_contents( pt );

        // TODO: Non-glass breaking
        // TODO: Wine glass breaking vs. entire sheet of glass breaking
        sounds::sound( pt, 16, sounds::sound_t::combat, _( "glass breaking!" ), false, "bullet_hit",
                       "hit_glass" );
        return;
    }

    if( proj.has_effect( ammo_effect_BURST ) ) {
        // Drop the contents, not the thrown item
        if( g->u.sees( pt ) ) {
            add_msg( _( "The %s bursts!" ), drop_item.tname() );
        }

        // copies the drop item to spill the contents
        drop_item.spill_contents( pt );

        // TODO: Sound
        return;
    }

    auto *mon = dynamic_cast<monster *>( attack.hit_critter );

    // We can only embed in monsters
    const bool mon_there = mon != nullptr && !mon->is_dead_state();
    // And if we actually want to embed
    bool embed = mon_there && !proj.has_effect( ammo_effect_NO_EMBED ) &&
                 !proj.has_effect( ammo_effect_TANGLE );
    // Don't embed in small creatures
    if( embed ) {
        const creature_size critter_size = mon->get_size();
        const units::volume vol = drop_item.volume();
        embed = embed && ( critter_size > creature_size::tiny || vol < 250_ml );
        embed = embed && ( critter_size > creature_size::small || vol < 500_ml );
        // And if we deal enough damage
        // Item volume bumps up the required damage too
        embed = embed &&
                ( attack.dealt_dam.type_damage( DT_CUT ) / 2 ) +
                attack.dealt_dam.type_damage( DT_STAB ) >
                attack.dealt_dam.type_damage( DT_BASH ) +
                vol * 3 / 250_ml + rng( 0, 5 );
    }

    if( embed ) {
        mon->add_item( std::move( drop ) );
        if( g->u.sees( *mon ) ) {
            add_msg( _( "The %1$s embeds in %2$s!" ), drop_item.tname(), mon->disp_name() );
        }
    } else {
        bool do_drop = true;
        // monsters that are able to be tied up will store the item another way
        // see monexamine.cpp tie_or_untie()
        // if they aren't friendly they will try and break out of the net/bolas/lasso
        // players and NPCs just get the downed effect, and item is dropped.
        // TODO: storing the item on player until they recover from downed
        if( proj.has_effect( ammo_effect_TANGLE ) && mon_there ) {
            do_drop = false;
        }
        if( proj.has_effect( ammo_effect_ACT_ON_RANGED_HIT ) ) {
            // Don't drop if it exploded
            drop = item::process( std::move( drop ), nullptr, attack.end_point, true );
        }

        map &here = get_map();
        if( drop && do_drop ) {
            here.add_item_or_charges( attack.end_point, std::move( drop ) );
        }

        if( proj.has_effect( ammo_effect_HEAVY_HIT ) ) {
            if( here.has_flag( flag_LIQUID, pt ) ) {
                sounds::sound( pt, 10, sounds::sound_t::combat, _( "splash!" ), false, "bullet_hit", "hit_water" );
            } else {
                sounds::sound( pt, 8, sounds::sound_t::combat, _( "thud." ), false, "bullet_hit", "hit_wall" );
            }
            const trap &tr = here.tr_at( pt );
            if( tr.triggered_by_item( drop_item ) ) {
                tr.trigger( pt, nullptr, &drop_item );
            }
        }
    }
}

auto blood_trail_len( int damage ) -> size_t
{
    if( damage > 50 ) {
        return 3;
    } else if( damage > 20 ) {
        return 2;
    } else if( damage > 0 ) {
        return 1;
    }
    return 0;
}
} // namespace


auto projectile_attack_roll( const dispersion_sources &dispersion, double range,
                             double target_size ) -> projectile_attack_aim
{
    projectile_attack_aim aim;

    // dispersion is a measure of the dispersion of shots due to the gun + shooter characteristics
    // i.e. it is independent of any particular shot

    // shot_dispersion is the actual dispersion for this particular shot, i.e.
    // the error angle between where the shot was aimed and where this one actually went
    // NB: some cases pass dispersion == 0 for a "never misses" shot e.g. bio_magnet,
    aim.dispersion = dispersion.roll();

    // an isosceles triangle is formed by the intended and actual target tiles
    aim.missed_by_tiles = iso_tangent( range, units::from_arcmin( aim.dispersion ) );

    // fraction we missed a monster target by (0.0 = perfect hit, 1.0 = miss)
    if( target_size > 0.0 ) {
        aim.missed_by = std::min( 1.0, aim.missed_by_tiles / target_size );
    } else {
        // Special case 0 size targets, just to be safe from 0.0/0.0 NaNs
        aim.missed_by = 1.0;
    }

    return aim;
}

auto projectile_attack( const projectile &proj_arg, const tripoint &source,
                        const tripoint &target_arg, const dispersion_sources &dispersion,
                        Creature *origin, item *source_weapon, const vehicle *in_veh ) -> dealt_projectile_attack
{
    const bool do_animation = get_option<bool>( "ANIMATION_PROJECTILES" );

    double range = rl_dist( source, target_arg );

    Creature *target_critter = g->critter_at( target_arg );
    map &here = get_map();
    const double target_size = target_critter != nullptr ?
                               target_critter->ranged_target_size() :
                               here.ranged_target_size( target_arg );
    projectile_attack_aim const aim = projectile_attack_roll( dispersion, range, target_size );

    // TODO: move to-hit roll back in here

    auto attack = dealt_projectile_attack {
        .proj = proj_arg,
        .hit_critter = nullptr,
        .dealt_dam = dealt_damage_instance(),
        .end_point = source,
        .missed_by = aim.missed_by,
    };

    // No suicidal shots
    if( source == target_arg ) {
        debugmsg( "Projectile_attack targeted own square." );
        return attack;
    }

    projectile &proj = attack.proj;

    const bool stream = proj.has_effect( ammo_effect_STREAM ) ||
                        proj.has_effect( ammo_effect_STREAM_BIG ) ||
                        proj.has_effect( ammo_effect_JET );
    const char bullet = stream ? '#' : '*';
    const bool no_item_damage = proj.has_effect( ammo_effect_NO_ITEM_DAMAGE );
    const bool do_draw_line = proj.has_effect( ammo_effect_DRAW_AS_LINE ) ||
                              get_option<bool>( "BULLETS_AS_LASERS" );
    const bool null_source = proj.has_effect( ammo_effect_NULL_SOURCE );
    // Determines whether it can penetrate obstacles
    const bool is_bullet = proj_arg.speed >= 200 &&
                           !proj.has_effect( ammo_effect_NO_PENETRATE_OBSTACLES );

    const auto is_thrown = proj.has_effect( ammo_effect_THROWN );
    const auto *thrown_item = proj.get_drop();
    auto custom_bullet_sprite = std::string{};
#if defined(TILES)
    if( tilecontext ) {
        const auto set_sprite_from_lookup = [&]( const std::string & candidate, TILE_CATEGORY category ) {
            if( !custom_bullet_sprite.empty() ) { return; }
            auto lookup = tilecontext->find_tile_looks_like( candidate, category );
            if( lookup ) { custom_bullet_sprite = lookup->id(); }
        };

        const auto set_sprite_from_item = [&]( const item & it, const bool allow_item_fallback ) {
            const auto id = it.typeId().str();
            set_sprite_from_lookup( "animation_bullet_" + id, C_BULLET );
            if( allow_item_fallback ) { set_sprite_from_lookup( id, C_ITEM ); }
        };

        if( thrown_item ) { set_sprite_from_item( *thrown_item, is_thrown ); }

        if( custom_bullet_sprite.empty() && source_weapon ) {
            const auto ammo_type = source_weapon->ammo_current();
            if( !ammo_type.is_null() ) {
                set_sprite_from_lookup( "animation_bullet_" + ammo_type.str(), C_BULLET );
            }
        }
    }
#endif // TILES

    // If we were targetting a tile rather than a monster, don't overshoot
    // Unless the target was a wall, then we are aiming high enough to overshoot
    const bool no_overshoot = proj.has_effect( ammo_effect_NO_OVERSHOOT ) ||
                              ( g->critter_at( target_arg ) == nullptr && here.passable( target_arg ) );

    double extend_to_range = no_overshoot ? range : proj_arg.range;

    tripoint target = target_arg;
    std::vector<tripoint> trajectory;
    std::vector<std::pair<monster, const dealt_projectile_attack>> hit_monsters;

    if( aim.missed_by_tiles >= 1.0 ) {
        // We missed enough to target a different tile
        const double dx = target_arg.x - source.x;
        const double dy = target_arg.y - source.y;
        units::angle rad = units::atan2( dy, dx );

        // cap wild misses at +/- 30 degrees
        const auto dispersion_angle =
            std::min( units::from_arcmin( aim.dispersion ), 30_degrees );
        rad += ( one_in( 2 ) ? 1 : -1 ) * dispersion_angle;

        // TODO: This should also represent the miss on z axis
        const int offset = std::min<int>( range, std::sqrt( aim.missed_by_tiles ) );
        int new_range = no_overshoot ?
                        range + rng( -offset, offset ) :
                        rng( range - offset, proj_arg.range );
        new_range = std::max( new_range, 1 );

        target.x = source.x + roll_remainder( new_range * cos( rad ) );
        target.y = source.y + roll_remainder( new_range * sin( rad ) );

        if( target == source ) {
            target.x = source.x + sgn( dx );
            target.y = source.y + sgn( dy );
        }

        // Don't extend range further, miss here can mean hitting the ground near the target
        range = rl_dist( source, target );
        extend_to_range = range;

        sfx::play_variant_sound( "bullet_hit", "hit_wall", sfx::get_heard_volume( target ),
                                 sfx::get_heard_angle( target ) );
        // TODO: Z dispersion
        // If we missed, just draw a straight line.
        trajectory = line_to( source, target );
    } else {
        // Go around obstacles a little if we're on target.
        trajectory = here.find_clear_path( source, target );
    }

    add_msg( m_debug, "missed_by_tiles: %.2f; missed_by: %.2f; target (orig/hit): %d,%d,%d/%d,%d,%d",
             aim.missed_by_tiles, aim.missed_by,
             target_arg.x, target_arg.y, target_arg.z,
             target.x, target.y, target.z );

    // Trace the trajectory, doing damage in order
    tripoint &tp = attack.end_point;
    tripoint prev_point = source;

    // Add the first point to the trajectory
    trajectory.insert( trajectory.begin(), source );

    static const emit_id muzzle_smoke( "emit_smaller_smoke_plume" );
    if( proj.has_effect( ammo_effect_MUZZLE_SMOKE ) ) {
        here.emit_field( trajectory.front(), muzzle_smoke );
    }

    if( !no_overshoot && range < extend_to_range ) {
        // Continue line is very "stiff" when the original range is short
        // TODO: Make it use a more distant point for more realistic extended lines
        std::vector<tripoint> trajectory_extension = continue_line( trajectory,
                extend_to_range - range );
        trajectory.reserve( trajectory.size() + trajectory_extension.size() );
        trajectory.insert( trajectory.end(), trajectory_extension.begin(), trajectory_extension.end() );
    }
    // Range can be 0
    size_t traj_len = trajectory.size();
    while( traj_len > 0 && rl_dist( source, trajectory[traj_len - 1] ) > proj_arg.range ) {
        --traj_len;
    }

    // Non-ballistic physical projectiles lose range if they overpenetrate.
    const bool is_projectile_modify_overpenetration =
        proj.impact.type_damage( DT_BASH ) > 0 ||
        proj.impact.type_damage( DT_CUT ) > 0 ||
        proj.impact.type_damage( DT_STAB ) > 0;

    // Arrow, sling or the like; 0.75x or 0.5x penalty depending on which damagetype was highest.
    const float projectile_overpenetration_modifier =
        ( proj.impact.type_damage( DT_CUT ) + proj.impact.type_damage( DT_STAB )
          >= proj.impact.type_damage( DT_BASH ) ) ? 0.75f : 0.5f;

    // Bullets, lasers, or other projectiles; 0.9x range penalty but no additional damage penalty.
    const float overpenetration_modifier = is_projectile_modify_overpenetration
                                           ? projectile_overpenetration_modifier
                                           : 0.9f;

    constexpr float projectile_skip_multiplier = 0.1f;

    // Randomize the skip so that bursts look nicer
    int projectile_skip_calculation = range * projectile_skip_multiplier;
    int projectile_skip_current_frame = rng( 0, projectile_skip_calculation );
    bool has_momentum = true;
    for( size_t i = 1; i < traj_len && ( has_momentum || stream ); ++i ) {
        prev_point = tp;
        tp = trajectory[i];

        if( tp.z != prev_point.z ) {
            tripoint floor1 = prev_point;
            tripoint floor2 = tp;

            if( floor1.z < floor2.z ) {
                floor1.z++;
            } else {
                floor2.z++;
            }
            // We only stop the bullet if there are two floors in a row
            // this allow the shooter to shoot adjacent enemies from rooftops.
            if( here.has_floor( floor1 ) && here.has_floor( floor2 ) ) {
                // Currently strictly no shooting through floor
                // TODO: Bash the floor
                tp = prev_point;
                traj_len = --i;
                break;
            }
        }

        // Drawing the bullet uses player g->u, and not player p, because it's drawn
        // relative to YOUR position, which may not be the gunman's position.
        if( do_animation && !do_draw_line ) {
            // TODO: Make this draw thrown item/launched grenade/arrow
            if( projectile_skip_current_frame >= projectile_skip_calculation ) {
                g->draw_bullet( tp, static_cast<int>( i ), trajectory, bullet, custom_bullet_sprite );
                projectile_skip_current_frame = 0;
                // If we missed recalculate the skip factor so they spread out.
                projectile_skip_calculation =
                    std::max( static_cast<size_t>( range ), i ) * projectile_skip_multiplier;
            } else {
                projectile_skip_current_frame++;
            }
        }

        if( in_veh != nullptr ) {
            const optional_vpart_position other = here.veh_at( tp );
            if( in_veh == veh_pointer_or_null( other ) && other->is_inside() ) {
                // Turret is on the roof and can't hit anything inside
                continue;
            }
        }

        Creature *critter = g->critter_at( tp );
        if( origin == critter ) {
            // No hitting self with "weird" attacks.
            critter = nullptr;
        }

        // Skip friendly creatures within 1 tile of shooter to prevent adjacent friendly fire
        if( critter != nullptr && origin != nullptr &&
            origin->attitude_to( *critter ) == Attitude::A_FRIENDLY &&
            rl_dist( source, tp ) <= 1 ) {
            critter = nullptr;
        }

        auto *mon = dynamic_cast<monster *>( critter );
        // ignore non-point-blank digging targets (since they are underground)
        if( mon != nullptr && mon->digging() &&
            rl_dist( source, tp ) > 1 ) {
            critter = nullptr;
            mon = nullptr;
        }

        // Reset hit critter from the last iteration
        attack.hit_critter = nullptr;

        // If we shot us a monster...
        // TODO: add size effects to accuracy
        // If there's a monster in the path of our bullet, and either our aim was true,
        //  OR it's not the monster we were aiming at and we were lucky enough to hit it
        double cur_missed_by = aim.missed_by;

        // unintentional hit on something other than our actual target
        // don't re-roll for the actual target, we already decided on a missed_by value for that
        // at the start, misses should stay as misses
        if( critter != nullptr && tp != target_arg ) {
            // Unintentional hit
            cur_missed_by = std::max( rng_float( 0.1, 1.5 - aim.missed_by ) /
                                      critter->ranged_target_size(), 0.4 );
        }

        if( here.obstructed_by_vehicle_rotation( prev_point, tp ) ) {
            //We're firing through an impassible gap in a rotated vehicle, randomly hit one of the two walls
            tripoint rand = tp;
            if( one_in( 2 ) ) {
                rand.x = prev_point.x;
            } else {
                rand.y = prev_point.y;
            }
            if( in_veh == nullptr || veh_pointer_or_null( here.veh_at( rand ) ) != in_veh ) {
                here.shoot( source, rand, proj, false );
                if( proj.impact.total_damage() <= 0 ) {
                    //If the projectile stops here move it back a square so it doesn't end up inside the vehicle
                    traj_len = i - 1;
                    tp = prev_point;
                    break;
                }
            }
        }

        // Penalize damage and/or range on overpenetration.
        auto apply_overpenetration_penalty = [&]( bool modify_damage ) {
            traj_len *= overpenetration_modifier;
            if( modify_damage ) {
                proj.impact.mult_damage( overpenetration_modifier );
                add_msg( m_debug, "Projectile damage and range *= %.1f", overpenetration_modifier );
            } else {
                add_msg( m_debug, "Projectile range *= %.1f", overpenetration_modifier );
            }
        };

        if( critter != nullptr && cur_missed_by < 1.0 ) {
            if( in_veh != nullptr && veh_pointer_or_null( here.veh_at( tp ) ) == in_veh &&
                critter->is_player() ) {
                // Turret either was aimed by the player (who is now ducking) and shoots from above
                // Or was just IFFing, giving lots of warnings and time to get out of the line of fire
                continue;
            }
            attack.missed_by = cur_missed_by;
            if( mon != nullptr ) {
                mon->deal_projectile_attack( null_source ? nullptr : origin, source_weapon, attack, true );
            } else {
                critter->deal_projectile_attack( null_source ? nullptr : origin, source_weapon, attack );
            }
            // Critter can still dodge the projectile
            // In this case hit_critter won't be set
            if( attack.hit_critter != nullptr ) {
                if( mon != nullptr ) {
                    hit_monsters.push_back( std::make_pair( *mon, attack ) );
                }
                const size_t bt_len = blood_trail_len( attack.dealt_dam.total_damage() );
                if( bt_len > 0 ) {
                    const tripoint &dest = move_along_line( tp, trajectory, bt_len );
                    here.add_splatter_trail( critter->bloodType(), tp, dest );
                }
                sfx::do_projectile_hit( *attack.hit_critter );
                has_momentum = proj.impact.total_damage() > 0 && is_bullet;

                apply_overpenetration_penalty( is_projectile_modify_overpenetration );
            } else {
                attack.missed_by = aim.missed_by;
            }
        } else if( in_veh != nullptr && veh_pointer_or_null( here.veh_at( tp ) ) == in_veh ) {
            // Don't do anything, especially don't call map::shoot as this would damage the vehicle
        } else {
            // Track damage before processing so we'll know if we actually hit any cover.
            const float dmg_before_penetration = proj.impact.total_damage();
            here.shoot( source, tp, proj, !no_item_damage && tp == target );
            const float dmg_after_penetration = proj.impact.total_damage();
            has_momentum = dmg_after_penetration > 0;
            // We lost momentum from hitting something, penalize range.
            if( dmg_before_penetration > dmg_after_penetration ) {
                apply_overpenetration_penalty( is_projectile_modify_overpenetration );
            }
        }
        if( !has_momentum && here.impassable( tp ) &&
            !here.has_flag( flag_THIN_OBSTACLE, tp ) ) {
            // Flamethrowers go through bars but not wall
            traj_len = i;
            break;
        }
    }
    if( do_animation && do_draw_line && traj_len > 2 ) {
        trajectory.erase( trajectory.begin() );
        trajectory.resize( traj_len-- );
        auto should_rotate = is_thrown && thrown_item &&
                             !thrown_item->has_flag( flag_FLY_STRAIGHT );
        draw_line_of( {
            .p = tp,
            .points = trajectory,
            .sprite = custom_bullet_sprite,
            .rotate = should_rotate,
        } );
    }

    if( here.impassable( tp ) ) {
        tp = prev_point;
    }

    drop_or_embed_projectile( attack );

    apply_ammo_effects( tp, proj.get_ammo_effects(), origin );
    const auto &expl = proj.get_custom_explosion();
    if( expl ) {
        explosion_handler::explosion( tp, expl, origin );
    }

    // TODO: Move this outside now that we have hit point in return values?
    if( proj.has_effect( ammo_effect_BOUNCE ) ) {
        // Add effect so the shooter is not targeted itself.
        if( origin && !origin->has_effect( effect_bounced ) ) {
            origin->add_effect( effect_bounced, 1_turns );
        }
        Creature *mon_ptr = g->get_creature_if( [&]( const Creature & z ) {
            // search for creatures in radius 4 around impact site
            if( rl_dist( z.pos(), tp ) <= 4 &&
                here.sees( z.pos(), tp, -1 ) ) {
                // don't hit targets that have already been hit
                if( !z.has_effect( effect_bounced ) ) {
                    return true;
                }
            }
            return false;
        } );
        if( mon_ptr ) {
            Creature &z = *mon_ptr;
            add_msg( _( "The attack bounced to %s!" ), z.get_name() );
            z.add_effect( effect_bounced, 1_turns );
            projectile_attack( proj, tp, z.pos(), dispersion, origin, source_weapon, in_veh );
            sfx::play_variant_sound( "fire_gun", "bio_lightning_tail",
                                     sfx::get_heard_volume( z.pos() ), sfx::get_heard_angle( z.pos() ) );
        }
    }
    explosion_handler::get_explosion_queue().execute();
    const size_t num_hit = hit_monsters.size();
    for( size_t i = 0; i < num_hit; ++i ) {
        auto nextattack = hit_monsters[i];
        monster attackedmon = nextattack.first;
        const dealt_projectile_attack attacked_proj = nextattack.second;
        // Copy from monster::on_hit
        attackedmon.type->sp_defense( attackedmon, origin, &attacked_proj );
    }
    return attack;
}

namespace ranged
{

auto hit_chance( const dispersion_sources &dispersion, double range, double target_size,
                 double missed_by ) -> double
{
    if( range <= 0 ) {
        return 1.0;
    }

    const double missed_by_tiles = missed_by * target_size;

    //          T = (2*D**2 * (1 - cos V)) ** 0.5   (from iso_tangent)
    //      cos V = 1 - T**2 / (2*D**2)
    const double cosV = 1 - missed_by_tiles * missed_by_tiles / ( 2 * range * range );
    const double needed_dispersion = ( cosV < -1.0 ? M_PI : acos( cosV ) ) * 180 * 60 / M_PI;

    return normal_cdf( needed_dispersion, dispersion.avg(), dispersion.avg() / 2 );
}

} // namespace ranged
