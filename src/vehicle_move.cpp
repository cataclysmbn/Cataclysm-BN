#include "vehicle.h"
#include "vehicle_part.h" // IWYU pragma: associated
#include "vehicle_move.h" // IWYU pragma: associated

#include <cassert>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <optional>
#include <ostream>
#include <set>

#include "avatar.h"
#include "bodypart.h"
#include "creature.h"
#include "debug.h"
#include "enums.h"
#include "explosion.h"
#include "game.h"
#include "int_id.h"
#include "item.h"
#include "itype.h"
#include "map.h"
#include "map_iterator.h"
#include "mapdata.h"
#include "material.h"
#include "math_defines.h"
#include "messages.h"
#include "monster.h"
#include "options.h"
#include "player.h"
#include "point_float.h"
#include "rng.h"
#include "sounds.h"
#include "string_id.h"
#include "translations.h"
#include "trap.h"
#include "units_angle.h"
#include "units_utility.h"
#include "veh_type.h"
#include "vpart_position.h"
#include "vpart_range.h"

#define dbg(x) DebugLogFL((x),DC::Map)

static const itype_id fuel_type_muscle( "muscle" );
static const itype_id fuel_type_animal( "animal" );
static const itype_id fuel_type_battery( "battery" );

static const skill_id skill_driving( "driving" );

static const trait_id trait_DEFT( "DEFT" );
static const trait_id trait_PROF_SKATER( "PROF_SKATER" );


static const efftype_id effect_harnessed( "harnessed" );
static const efftype_id effect_pet( "pet" );
static const efftype_id effect_stunned( "stunned" );

static const std::string part_location_structure( "structure" );

// tile height in meters
static const float tile_height = 4;
// miles per hour to vehicle 100ths of miles per hour
static const int mi_to_vmi = 100;
// meters per second to miles per hour
static const float mps_to_miph = 2.23694f;
// Conversion constant for Impulse Ns to damage for vehicle collisions. Fine tune to desired damage.
static const float imp_conv_const = 0.1;
// Inverse conversion constant for impulse to damage
static const float imp_conv_const_inv = 1 / imp_conv_const;
// Conversion constant for 100ths of miles per hour to meters per second
constexpr float velocity_constant = 0.0044704;

// convert m/s to vehicle 100ths of a mile per hour
int mps_to_vmiph( double mps )
{
    return mps * mps_to_miph * mi_to_vmi;
}

// convert vehicle 100ths of a mile per hour to m/s
double vmiph_to_mps( int vmiph )
{
    return vmiph * velocity_constant;
}

int cmps_to_vmiph( int cmps )
{
    return cmps * mps_to_miph;
}

int vmiph_to_cmps( int vmiph )
{
    return vmiph / mps_to_miph;
}
// Conversion of impulse Ns to damage for vehicle collision purposes.
float impulse_to_damage( float impulse )
{
    return impulse * imp_conv_const;
}

// Convert damage back to impulse Ns
float damage_to_impulse( float damage )
{
    return damage * imp_conv_const_inv;
}

int vehicle::slowdown( int at_velocity ) const
{
    double mps = vmiph_to_mps( std::abs( at_velocity ) );

    // slowdown due to air resistance is proportional to square of speed
    double f_total_drag = std::abs( coeff_air_drag() * mps * mps );
    if( is_watercraft() ) {
        // same with water resistance
        f_total_drag += coeff_water_drag() * mps * mps;
    } else if( !is_falling && !is_flying ) {
        double f_rolling_drag = coeff_rolling_drag() * ( vehicles::rolling_constant_to_variable + mps );
        if( vehicle_movement::is_on_rails( get_map(), *this ) ) {
            // vehicles on rails don't skid
            f_total_drag += f_rolling_drag;
        } else {
            // increase rolling resistance by up to 25x if the vehicle is skidding at right angle to facing
            const double skid_factor = 1.0 + 24.0 * std::abs( units::sin( face.dir() - move.dir() ) );
            f_total_drag += f_rolling_drag * skid_factor;
        }
    }
    double accel_slowdown = f_total_drag / to_kilogram( total_mass() );
    // converting m/s^2 to vmiph/s
    float slowdown = mps_to_vmiph( accel_slowdown );
    if( is_towing() ) {
        vehicle *other_veh = tow_data.get_towed();
        if( other_veh ) {
            slowdown += other_veh->slowdown( at_velocity );
        }
    }
    if( slowdown < 0 ) {
        debugmsg( "vehicle %s has negative drag slowdown %d\n", name, slowdown );
    }
    add_msg( m_debug, "%s at %d vimph, f_drag %3.2f, drag accel %.1f vmiph - extra drag %d",
             name, at_velocity, f_total_drag, slowdown, static_drag() );
    // plows slow rolling vehicles, but not falling or floating vehicles
    if( !( is_falling || is_floating || is_flying ) ) {
        slowdown -= static_drag();
    }

    return std::max( 1.0f, slowdown );
}

void vehicle::thrust( int thd, int z )
{
    //if vehicle is stopped, set target direction to forward.
    //ensure it is not skidding. Set turns used to 0.
    if( !is_moving() && z == 0 ) {
        turn_dir = face.dir();
        stop();
    }
    bool pl_ctrl = player_in_control( get_player_character() );

    // No need to change velocity if there are no wheels
    if( ( in_water && can_float() ) || ( is_aircraft() && ( z != 0 || is_flying ) ) ) {
        // we're good
    } else if( is_floating && !can_float() ) {
        stop();
        if( pl_ctrl ) {
            add_msg( _( "The %s is too leaky!" ), name );
        }
        return;
    } else if( !valid_wheel_config()  && z == 0 ) {
        stop();
        if( pl_ctrl ) {
            add_msg( _( "The %s doesn't have enough wheels to move!" ), name );
        }
        return;
    }
    // Accelerate (true) or brake (false)
    bool thrusting = true;
    if( velocity ) {
        int sgn = ( velocity < 0 ) ? -1 : 1;
        thrusting = ( sgn == thd );
    }

    // TODO: Pass this as an argument to avoid recalculating
    float traction = k_traction( get_map().vehicle_wheel_traction( *this ) );
    int accel = current_acceleration() * traction;
    // Somehow accel is not clamped to current_acceleration in the air
    // Everywhere else and there is no need for clamps
    if( is_flying && ( has_part( VPFLAG_WING ) || has_part( VPFLAG_BALLOON ) )
        && !( has_part( VPFLAG_PROPELLER ) || has_part( VPFLAG_ROTOR ) ) ) {
        accel = 0;
    }
    if( accel < 200 && velocity > 0 && is_towing() ) {
        if( pl_ctrl ) {
            add_msg( _( "The %s struggles to pull the %s on this surface!" ), name,
                     tow_data.get_towed()->name );
        }
        return;
    }
    // Both balloons and wings can "glide", making themselves have times where they wont have engine power
    // As they can be without propellers
    if( thrusting && accel == 0 ) {
        if( z != 0 && is_aircraft() ) {
            requested_z_change = z;
            return;
        } else if( is_aircraft() ) {
            return;
        } else if( pl_ctrl ) {
            add_msg( _( "The %s is too heavy for its engine(s)!" ), name );
            return;
        }
    }
    const int max_vel = traction * max_velocity();
    // maximum braking is 20 mph/s, assumes high friction tires
    const int max_brake = 20 * 100;
    //pos or neg if accelerator or brake
    int vel_inc = ( accel + ( thrusting ? 0 : max_brake ) ) * thd;
    // Reverse is only 60% acceleration, unless an electric motor is in use
    if( thd == -1 && thrusting && !has_engine_type( fuel_type_battery, true ) ) {
        vel_inc = .6 * vel_inc;
    }

    //find ratio of used acceleration to maximum available, returned in tenths of a percent
    //so 1000 = 100% and 453 = 45.3%
    int load;
    // Keep exact cruise control speed
    if( cruise_on && accel != 0 ) {
        int effective_cruise = std::min( cruise_velocity, max_vel );
        if( thd > 0 ) {
            vel_inc = std::min( vel_inc, effective_cruise - velocity );
        } else {
            vel_inc = std::max( vel_inc, effective_cruise - velocity );
        }
        if( thrusting ) {
            load = 1000 * std::abs( vel_inc ) / accel;
        } else {
            // brakes provide 20 mph/s of slowdown and the rest is engine braking
            // TODO: braking depends on wheels, traction, driver skill
            load = 1000 * std::max( 0, std::abs( vel_inc ) - max_brake ) / accel;
        }
    } else {
        load = ( thrusting ? 1000 : 0 );
    }
    // rotorcraft need to spend +5% (in addition to idle) of load to fly, +20% (in addition to idle) to ascend
    if( is_aircraft() && ( z > 0 || is_flying_in_air() ) ) {
        const auto rotor_newtons = std::max( 0.0,
                                             to_newton( total_mass() ) - total_balloon_lift() - total_wing_lift() );
        const auto rotor_capacity = rotor_newtons / thrust_of_rotorcraft( true );
        if( is_rotorcraft() ) {
            thrusting = true;
            load = std::max( load, int( std::floor( ( z > 0 ? 200 : 50 ) * rotor_capacity ) ) );
        } else {
            thrusting = true;
        }
    }
    if( thrusting && z != 0 && is_aircraft() ) {
        requested_z_change = z;
    }

    // only consume resources if engine accelerating
    if( load >= 1 && thrusting ) {
        // abort engine things if engines are not operational
        // not if there is no acceleration though
        if( total_power_w() <= 0 || !engine_on || accel == 0 ) {
            if( pl_ctrl && !is_aircraft() ) {
                if( total_power_w( false ) <= 0 ) {
                    add_msg( m_info, _( "The %s doesn't have an engine!" ), name );
                } else if( has_engine_type( fuel_type_muscle, true ) ) {
                    add_msg( m_info, _( "The %s's mechanism is out of reach!" ), name );
                } else if( !engine_on ) {
                    add_msg( _( "The %s's engine isn't on!" ), name );
                } else if( traction < 0.01f ) {
                    add_msg( _( "The %s is stuck." ), name );
                } else {
                    add_msg( _( "The %s's engine emits a sneezing sound." ), name );
                }
            }
        } else {
            if( pl_ctrl ) {
                // Driving skill reduces fuel consumption.
                const float skill = get_player_character().get_skill_level( skill_driving );
                const float skill_cost = std::max( 0.75f, ( 100.0f - ( skill * 2.5f ) ) / 100.0f );
                // Up to 25% reduction at max skill, cap at idle rate.
                const int load_cap = 10;
                load = std::max( static_cast<int>( load * skill_cost ), load_cap );
            }
            //make noise and consume fuel
            noise_and_smoke( load );
            consume_fuel( load, 1 );
            //break the engines a bit, if going too fast.
            int strn = static_cast<int>( strain() * strain() * 100 );
            for( size_t e = 0; e < engines.size(); e++ ) {
                do_engine_damage( e, strn );
            }
        }
    }

    //wheels aren't facing the right way to change velocity properly
    //lower down, since engines should be getting damaged anyway
    if( skidding ) {
        return;
    }

    //change vehicles velocity
    if( ( velocity > 0 && velocity + vel_inc < 0 ) || ( velocity < 0 && velocity + vel_inc > 0 ) ) {
        //velocity within braking distance of 0
        stop();
    } else {
        // Increase velocity up to max_vel or min_vel, but not above.
        const int min_vel = max_reverse_velocity();
        if( vel_inc > 0 ) {
            // Don't allow braking by accelerating (could happen with damaged engines)
            velocity = std::max( velocity, std::min( velocity + vel_inc, max_vel ) );
        } else {
            velocity = std::min( velocity, std::max( velocity + vel_inc, min_vel ) );
        }
    }
    // If you are going faster than the animal can handle, harness is damaged
    // Animal may come free ( and possibly hit by vehicle )
    for( size_t e = 0; e < parts.size(); e++ ) {
        const vehicle_part &vp = parts[ e ];
        if( vp.info().fuel_type == fuel_type_animal && engines.size() != 1 ) {
            monster *mon = get_pet( e );
            if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
                if( velocity > mon->get_speed() * 12 ) {
                    add_msg( m_bad, _( "Your %s is not fast enough to keep up with the %s" ), mon->get_name(), name );
                    int dmg = rng( 0, 10 );
                    damage_direct( e, dmg );
                }
            }
        }
    }
}

void vehicle::cruise_thrust( int amount )
{
    if( amount == 0 ) {
        return;
    }
    int safe_vel = safe_velocity();
    int max_vel = autopilot_on ? safe_velocity() : max_velocity();
    int max_rev_vel = max_reverse_velocity();

    //if the safe velocity is between the cruise velocity and its next value, set to safe velocity
    if( ( cruise_velocity < safe_vel && safe_vel < ( cruise_velocity + amount ) ) ||
        ( cruise_velocity > safe_vel && safe_vel > ( cruise_velocity + amount ) ) ) {
        cruise_velocity = safe_vel;
    } else {
        if( amount < 0 && ( cruise_velocity == safe_vel || cruise_velocity == max_vel ) ) {
            // If coming down from safe_velocity or max_velocity decrease by one so
            // the rounding below will drop velocity to a multiple of amount.
            cruise_velocity += -1;
        } else if( amount > 0 && cruise_velocity == max_rev_vel ) {
            // If increasing from max_rev_vel, do the opposite.
            cruise_velocity += 1;
        } else {
            // Otherwise just add the amount.
            cruise_velocity += amount;
        }
        // Integer round to lowest multiple of amount.
        // The result is always equal to the original or closer to zero,
        // even if negative
        cruise_velocity = ( cruise_velocity / std::abs( amount ) ) * std::abs( amount );
    }
    // Can't have a cruise speed faster than max speed
    // or reverse speed faster than max reverse speed.
    if( cruise_velocity > max_vel ) {
        cruise_velocity = max_vel;
    } else if( cruise_velocity < max_rev_vel ) {
        cruise_velocity = max_rev_vel;
    }
}

void vehicle::turn( units::angle deg )
{
    if( deg == 0_degrees ) {
        return;
    }
    if( velocity < 0 && !::get_option<bool>( "REVERSE_STEERING" ) ) {
        deg = -deg;
    }
    last_turn = deg;
    turn_dir = normalize( turn_dir + deg );
    // quick rounding the turn dir to a multiple of 15
    turn_dir = round_to_multiple_of( turn_dir, 15_degrees );
}

void vehicle::stop( bool update_cache )
{
    velocity = 0;
    skidding = false;
    move = face;
    last_turn = 0_degrees;
    of_turn_carry = 0;
    if( !update_cache ) {
        return;
    }
    map &here = get_map();
    for( const tripoint &p : get_points() ) {
        here.set_memory_seen_cache_dirty( p );
    }
}

bool vehicle::collision( std::vector<veh_collision> &colls,
                         const tripoint &dp,
                         bool just_detect, bool bash_floor )
{

    /*
     * Big TODO:
     * Rewrite this function so that it has "pre-collision" phase (detection)
     *  and "post-collision" phase (applying damage).
     * Then invoke the functions cyclically (pre-post-pre-post-...) until
     *  velocity == 0 or no collision happens.
     * Make all post-collisions in a given phase use the same momentum.
     *
     * How it works right now: find the first obstacle, then ram it over and over
     *  until either the obstacle is removed or the vehicle stops.
     * Bug: when ramming a critter without enough force to send it flying,
     *  the vehicle will phase into it.
     */

    if( dp.z != 0 && ( dp.x != 0 || dp.y != 0 ) ) {
        // Split into horizontal + vertical
        return collision( colls, tripoint( dp.xy(), 0 ), just_detect, bash_floor ) ||
               collision( colls, tripoint( 0,    0,    dp.z ), just_detect, bash_floor );
    }

    if( dp.z == -1 && !bash_floor ) {
        // First check current level, then the one below if current had no collisions
        // Bash floors on the current one, but not on the one below.
        if( collision( colls, tripoint_zero, just_detect, true ) ) {
            return true;
        }
    }

    const bool vertical = bash_floor || dp.z != 0;
    const int &coll_velocity = vertical ? vertical_velocity : velocity;
    // Skip collisions when there is no apparent movement, except verticially moving rotorcraft.
    if( coll_velocity == 0 && !is_aircraft() ) {
        just_detect = true;
    }

    const int velocity_before = coll_velocity;
    int lowest_velocity = coll_velocity;
    const int sign_before = sgn( velocity_before );
    bool empty = true;
    for( int p = 0; static_cast<size_t>( p ) < parts.size(); p++ ) {
        const vpart_info &info = part_info( p );
        if( ( info.location != part_location_structure && !info.has_flag( VPFLAG_EXTENDABLE )
              && info.rotor_diameter() == 0 ) || parts[ p ].removed ) {
            continue;
        }
        empty = false;
        // Coordinates of where part will go due to movement (dx/dy/dz)
        //  and turning (precalc[1])
        const tripoint dsp = global_pos3() + dp + parts[p].precalc[1];
        veh_collision coll = part_collision( p, dsp, just_detect, bash_floor );
        if( coll.type == veh_coll_nothing ) {
            continue;
        }

        colls.push_back( coll );

        if( just_detect ) {
            // DO insert the first collision so we can tell what was it
            return true;
        }

        const int velocity_after = coll_velocity;
        // A hack for falling vehicles: restore the velocity so that it hits at full force everywhere
        // TODO: Make this more elegant
        if( vertical ) {
            if( velocity_before < 0 ) {
                lowest_velocity = std::max( lowest_velocity, coll_velocity );
            } else {
                lowest_velocity = std::min( lowest_velocity, coll_velocity );
            }
            vertical_velocity = velocity_before;
        } else if( sgn( velocity_after ) != sign_before ) {
            // Sign of velocity inverted, collisions would be in wrong direction
            break;
        }
    }

    if( vertical ) {
        vertical_velocity = lowest_velocity;
        if( vertical_velocity == 0 ) {
            is_falling = false;
        }
    }

    if( empty ) {
        // HACK: Hack for dirty vehicles that didn't yet get properly removed
        veh_collision fake_coll;
        fake_coll.type = veh_coll_other;
        colls.push_back( fake_coll );
        velocity = 0;
        vertical_velocity = 0;
        add_msg( m_debug, "Collision check on a dirty vehicle %s", name );
        return true;
    }

    return !colls.empty();
}

// A helper to make sure mass and density is always calculated the same way
static void terrain_collision_data( const tripoint &p, bool bash_floor,
                                    float &mass, float &density, float &elastic )
{
    elastic = 0.30;
    map &here = get_map();
    // Just a rough rescale for now to obtain approximately equal numbers
    const int bash_min = here.bash_resistance( p, bash_floor );
    const int bash_max = here.bash_strength( p, bash_floor );
    mass = ( bash_min + bash_max ) / 2.0;
    density = bash_min;
}

veh_collision vehicle::part_collision( int part, const tripoint &p,
                                       bool just_detect, bool bash_floor )
{
    // Vertical collisions need to be handled differently
    // All collisions have to be either fully vertical or fully horizontal for now
    const bool vert_coll = bash_floor || p.z != sm_pos.z;
    Character &player_character = get_player_character();
    const bool pl_ctrl = player_in_control( player_character );
    Creature *critter = g->critter_at( p, true );
    player *ph = dynamic_cast<player *>( critter );

    Creature *driver = pl_ctrl ? &player_character : nullptr;

    // If in a vehicle assume it's this one
    if( ph != nullptr && ph->in_vehicle ) {
        critter = nullptr;
        ph = nullptr;
    }

    map &here = get_map();
    const optional_vpart_position ovp = here.veh_at( p );
    // Disable vehicle/critter collisions when bashing floor
    // TODO: More elegant code
    const bool is_veh_collision = !bash_floor && ovp && &ovp->vehicle() != this;
    const bool is_body_collision = !bash_floor && critter != nullptr;

    veh_collision ret;
    ret.type = veh_coll_nothing;
    ret.part = part;

    // Vehicle collisions are a special case. just return the collision.
    // The map takes care of the dynamic stuff.
    if( is_veh_collision ) {
        ret.type = veh_coll_veh;
        //"imp" is too simplistic for vehicle-vehicle collisions
        ret.target = &ovp->vehicle();
        ret.target_part = ovp->part_index();
        ret.target_name = ovp->vehicle().disp_name();
        return ret;
    }

    // Typical rotor tip speed in MPH * 100.
    int rotor_velocity = 45600;
    // Non-vehicle collisions can't happen when the vehicle is not moving
    int &coll_velocity = ( part_info( part ).rotor_diameter() == 0 ) ?
                         ( vert_coll ? vertical_velocity : velocity ) :
                         rotor_velocity;
    if( !just_detect && coll_velocity == 0 ) {
        return ret;
    }

    if( is_body_collision ) {
        // critters on a BOARDABLE part in this vehicle aren't colliding
        if( ovp && ( &ovp->vehicle() == this ) && get_pet( ovp->part_index() ) ) {
            return ret;
        }
        // we just ran into a fish, so move it out of the way
        if( here.has_flag( "SWIMMABLE", critter->pos() ) ) {
            tripoint end_pos = critter->pos();
            tripoint start_pos;
            const units::angle angle =
                move.dir() + 45_degrees * ( parts[part].mount.x > pivot_point().x ? -1 : 1 );
            std::set<tripoint> &cur_points = get_points( true );
            // push the animal out of way until it's no longer in our vehicle and not in
            // anyone else's position
            while( g->critter_at( end_pos, true ) ||
                   cur_points.contains( end_pos ) ) {
                start_pos = end_pos;
                calc_ray_end( angle, 2, start_pos, end_pos );
            }
            critter->setpos( end_pos );
            return ret;
        }
    }

    // Damage armor before damaging any other parts
    // Actually target, not just damage - spiked plating will "hit back", for example
    const int armor_part = part_with_feature( ret.part, VPFLAG_ARMOR, true );
    if( armor_part >= 0 ) {
        ret.part = armor_part;
    }
    // Damage modifier, pre-divided by 100. 1 is full collision damage, 1.5 is 50% bonus, etc.
    int dmg_mod = ( part_info( ret.part ).dmg_mod ) / 100;
    // Failsafe incase of wierdness.
    if( dmg_mod == 0 ) {
        dmg_mod = 1;
    }
    // Let's calculate type of collision & mass of object we hit
    float mass2 = 0;
    // e = 0 -> plastic collision
    // e = 1 -> inelastic collision
    float e = 0.3;

    //part density
    float part_dens = 0;

    if( is_body_collision ) {
        // Check any monster/NPC/player on the way
        // body
        ret.type = veh_coll_body;
        ret.target = critter;
        e = 0.30;
        part_dens = 15;
        mass2 = units::to_kilogram( critter->get_weight() );
        ret.target_name = critter->disp_name();
    } else if( ( bash_floor && here.is_bashable_ter_furn( p, true ) ) ||
               ( here.is_bashable_ter_furn( p, false ) && here.move_cost_ter_furn( p ) != 2 &&
                 // Don't collide with tiny things, like flowers, unless we have a wheel in our space.
                 ( part_with_feature( ret.part, VPFLAG_WHEEL, true ) >= 0 ||
                   !here.has_flag_ter_or_furn( "TINY", p ) ) &&
                 // Protrusions don't collide with short terrain.
                 // Tiny also doesn't, but it's already excluded unless there's a wheel present.
                 !( part_with_feature( ret.part, "PROTRUSION", true ) >= 0 &&
                    here.has_flag_ter_or_furn( "SHORT", p ) ) &&
                 // These are bashable, but don't interact with vehicles.
                 !here.has_flag_ter_or_furn( "NOCOLLIDE", p ) &&
                 // Do not collide with track tiles if we can use rails
                 !( here.has_flag_ter_or_furn( TFLAG_RAIL, p ) && this->can_use_rails() ) ) ) {
        // Movecost 2 indicates flat terrain like a floor, no collision there.
        ret.type = veh_coll_bashable;
        terrain_collision_data( p, bash_floor, mass2, part_dens, e );
        ret.target_name = here.disp_name( p );
    } else if( here.impassable_ter_furn( p ) ||
               ( bash_floor && !here.has_flag( TFLAG_NO_FLOOR, p ) ) ) {
        // not destructible
        ret.type = veh_coll_other;
        mass2 = 1000;
        e = 0.10;
        part_dens = 80;
        ret.target_name = here.disp_name( p );
    }

    if( ret.type == veh_coll_nothing || just_detect ) {
        // Hit nothing or we aren't actually hitting
        return ret;
    }
    stop_autodriving();
    // Calculate mass AFTER checking for collision
    //  because it involves iterating over all cargo
    // Rotors only use rotor mass in calculation.
    const float mass = ( part_info( part ).rotor_diameter() > 0 ) ?
                       to_kilogram( parts[ part ].base->weight() ) : to_kilogram( total_mass() );

    // No longer calculating damage based on deformation energy.
    // Damage to each object is based upon force applied from change in momentum

    //Finds vehicle part density using part material
    const material_id_list &mats = part_info( ret.part ).item->materials;
    float vpart_dens = 0;
    if( !mats.empty() ) {
        for( auto &mat_id : mats ) {
            vpart_dens += mat_id.obj().density();
        }
        // average
        vpart_dens /= mats.size();
    }

    //Calculates density factor. Used as a bad stand in to determine deformation distance.
    //Ranges from 0.1 -> 100, measured in cm. A density difference of 100 is needed for the full value.
    float density_factor = std::abs( part_dens - vpart_dens );
    density_factor = clamp( density_factor, 0.1f, 100.0f );

    //Deformation distance of the collision, measured in meters. A bad approximation for a value we dont have that would take intensive simulation to determine.
    // 0.001 -> 1 meter. Left modifiable so that armor or other parts can affect it.
    float deformation_distance = density_factor / 100;

    bool smashed = true;
    const std::string snd = _( "smash!" );
    float part_dmg = 0;
    float obj_dmg = 0;
    // Calculate stun time of car
    time_duration time_stunned = 0_turns;
    float vel1_a = 0;
    float vel2_a = 0;
    float impulse_veh = 0;
    float impulse_obj = 0;
    int critter_health = 0;

    const int prev_velocity = coll_velocity;
    const int vel_sign = sgn( coll_velocity );
    // Velocity of the object we're hitting
    // Assuming it starts at 0, but we'll probably hit it many times
    // in one collision, so accumulate the velocity gain from each hit.
    float vel2 = 0.0f;
    do {
        smashed = false;
        // Velocity of vehicle for calculations
        // Changed from mph to m/s, because mixing unit systems is a nono
        const float vel1 = vmiph_to_mps( coll_velocity );
        // Velocity of car after collision
        vel1_a = ( mass * vel1 + mass2 * vel2 + e * mass2 * ( vel2 - vel1 ) ) /
                 ( mass + mass2 );
        // Velocity of object 2 after collision
        vel2_a = ( mass * vel1 + mass2 * vel2 + e * mass * ( vel1 - vel2 ) ) / ( mass + mass2 );

        // Impulse of vehicle part from collision. Measured in newton seconds (Ns)
        // Calculated as the change in momentum due to the collision
        impulse_veh = std::abs( mass * ( vel1_a - vel1 ) );
        // Impulse of the impacted object from collision. Measured in newton seconds (Ns)
        // Calculated as the change in momentum due to the collision.
        impulse_obj = std::abs( mass2 * ( vel2_a - vel2 ) );
        // Due to conservation of momentum, both of these values should be equal. If not, something odd has happened and physics will be wonky. Small threshold for rounding errors.
        if( std::abs( impulse_obj - impulse_veh ) > 5 ) {
            add_msg( m_debug,
                     "Conservation of momentum violated, impulse values between object and vehicle are not equal! " );
            if( std::fabs( vel1_a ) < std::fabs( vel1 ) ) {
                // Lower vehicle's speed to prevent infinite loops
                coll_velocity = mps_to_vmiph( vel1_a ) * 0.9;
            }
            if( std::fabs( vel2_a ) > std::fabs( vel2 ) ) {
                vel2 = vel2_a;
            }
            // this causes infinite loop
            if( mass2 == 0 ) {
                mass2 = 1;
            }
            continue;
        }

        // Damage calculation
        // Maximum damage for vehicle part to take
        const float bash_max = here.bash_strength( p, bash_floor );


        // Damage for vehicle-part
        // Always if no critters, otherwise if critter is real
        if( critter == nullptr || !critter->is_hallucination() ) {

            part_dmg = impulse_to_damage( impulse_veh );
            if( bash_max != 0 ) {
                part_dmg = std::min( bash_max / dmg_mod, part_dmg );
            }

            //add_msg( m_debug, "Part collision damage: %.2f", part_dmg );
        } else {
            part_dmg = 0;
            add_msg( m_debug, "Part damage 0, critter assumed to be hallucination" );
        }
        // Damage for object.
        obj_dmg = impulse_to_damage( impulse_obj ) * dmg_mod;
        ret.target_name = here.disp_name( p );
        add_msg( m_debug, _( "%1s collided with %2s!" ), name, ret.target_name );
        add_msg( m_debug,
                 "Vehicle mass of %.2f Kg with a Pre-Collision Velocity of %d vmph, collision object mass of %.2f Kg, with a Velocity of %.2f mph. predicted deformation distance is %.2f meters.",
                 mass, prev_velocity, mass2, vel2, deformation_distance );
        add_msg( m_debug,
                 "Vehicle impulse of %.2f Ns resulted in Part collision damage %.2f of a maximum of %.2f, Object impulse of %.2f resulted in damage of %.2f (dmg mod %0.2i)",
                 impulse_veh, part_dmg, bash_max, impulse_obj, obj_dmg, dmg_mod );
        if( ret.type == veh_coll_bashable ) {
            // Something bashable -- use map::bash to determine outcome
            // NOTE: Floor bashing disabled for balance reasons
            //       Floor values are still used to set damage dealt to vehicle
            smashed = here.is_bashable_ter_furn( p, false ) &&
                      here.bash_resistance( p, bash_floor ) <= obj_dmg &&
                      here.bash( p, obj_dmg, false, false, false, this ).success;
            if( smashed ) {
                //Experimental: only use as much energy as was required to destroy the obstacle, and recalc impulse and momentum off of that.
                //Recalculate vel1_a from new impulse. Remember that the impulse from a collision is technically negative, reducing speed.
                float old_veh_imp = impulse_veh;
                impulse_veh = damage_to_impulse( std::min( part_dmg, bash_max / dmg_mod ) );
                add_msg( m_debug, "Terrain collision impulse recovery of %.2f Ns", old_veh_imp - impulse_veh );
                if( ( vel1 - ( impulse_veh / mass ) ) < vel1 ) {
                    vel1_a = ( vel1 - ( impulse_veh / mass ) );
                    add_msg( m_debug, "Post collision velocity recovered to %.2f m/s", vel1_a );
                }
                add_msg( m_debug, _( "%1s smashed %2s!" ), name, ret.target_name );

                if( here.is_bashable_ter_furn( p, bash_floor ) ) {
                    // There's new terrain there to smash
                    smashed = false;
                    terrain_collision_data( p, bash_floor, mass2, part_dens, e );
                    ret.target_name = here.disp_name( p );
                } else if( here.impassable_ter_furn( p ) ) {
                    // There's new terrain there, but we can't smash it!
                    smashed = false;
                    ret.type = veh_coll_other;
                    mass2 = 1000;
                    e = 0.10;
                    part_dens = 80;
                    ret.target_name = here.disp_name( p );
                }
            }
        } else if( ret.type == veh_coll_body ) {

            // We know critter is set for this type.  Assert to inform static
            // analysis.
            assert( critter );

            // No blood from hallucinations
            if( !critter->is_hallucination() ) {
                // Get critter health for determining max damage to apply
                critter_health = critter->get_hp_max();
                if( part_flag( ret.part, "SHARP" ) ) {
                    parts[ret.part].blood += ( 20 + obj_dmg ) * 5;
                } else if( obj_dmg > rng( 10, 30 ) ) {
                    parts[ret.part].blood += ( 10 + obj_dmg / 2 ) * 5;
                }

                check_environmental_effects = true;
            }

            time_stunned = time_duration::from_turns( ( rng( 0, obj_dmg ) > 10 ) + ( rng( 0, obj_dmg ) > 40 ) );
            if( time_stunned > 0_turns ) {
                critter->add_effect( effect_stunned, time_stunned );
            }

            if( ph != nullptr ) {
                ph->hitall( obj_dmg, 40, driver );
            } else {
                const int armor = part_flag( ret.part, "SHARP" ) ?
                                  critter->get_armor_cut( bodypart_id( "torso" ) ) :
                                  critter->get_armor_bash( bodypart_id( "torso" ) );
                obj_dmg = std::max( 0.0f, obj_dmg - armor );
                critter->apply_damage( driver, bodypart_id( "torso" ), obj_dmg );

                // Limit vehicle damage to the max health of the critter, attenuated by the damage modifier.
                part_dmg = std::min( ( critter_health * 1.0f ) / dmg_mod, part_dmg );

                add_msg( m_debug, "Critter collision! %1s was hit by %2s", critter->disp_name(), name );
                add_msg( m_debug, "Vehicle of %.2f Kg at %2.f vmph impacted Critter of %.2f Kg at %.2f vmph", mass,
                         vel1, mass2, vel2 );
                add_msg( m_debug,
                         "Vehicle received impulse of %.2f Nm dealing %.2f damage of maximum %.2f, Critter received impulse of %.2f Nm dealing %.2f damage. ",
                         impulse_veh, part_dmg, impulse_obj, obj_dmg );
                //attempt to recover unspent collision energy
                // Remember that the impulse from a collision is technically negative, reducing speed.
                impulse_veh = damage_to_impulse( part_dmg );
                if( ( vel1 - ( impulse_veh / mass ) ) < vel1 ) {
                    vel1_a = ( vel1 - ( impulse_veh / mass ) );
                    add_msg( m_debug, "Post collision velocity recovered to %.2f m/s", vel1_a );
                }

            }

            // Don't fling if vertical - critter got smashed into the ground
            if( !vert_coll ) {
                if( std::fabs( vel2_a ) > 10.0f ||
                    std::fabs( e * mass * vel1_a ) > std::fabs( mass2 * ( 10.0f - vel2_a ) ) ) {
                    const units::angle angle = rng_float( -60_degrees, 60_degrees );
                    // Also handle the weird case when we don't have enough force
                    // but still have to push (in such case compare momentum)
                    const float push_force = std::max<float>( std::fabs( vel2_a ), 10.1f );
                    // move.dir is where the vehicle is facing. If velocity is negative,
                    // we're moving backwards and have to adjust the angle accordingly.
                    const units::angle angle_sum =
                        angle + move.dir() + ( vel2_a > 0 ? 0_degrees : 180_degrees );
                    g->fling_creature( critter, angle_sum, push_force );
                } else if( std::fabs( vel2_a ) > std::fabs( vel2 ) ) {
                    vel2 = vel2_a;
                } else {
                    // Vehicle's momentum isn't big enough to push the critter
                    velocity = 0;
                    break;
                }

                if( critter->is_dead_state() ) {
                    smashed = true;
                } else if( critter != nullptr ) {
                    // Only count critter as pushed away if it actually changed position
                    smashed = critter->pos() != p;
                }
            }
        }

        if( critter == nullptr || !critter->is_hallucination() ) {
            coll_velocity = mps_to_vmiph( vel1_a * ( smashed ? 1 : 0.9 ) );
        }
        // Stop processing when sign inverts, not when we reach 0
    } while( !smashed && sgn( coll_velocity ) == vel_sign );

    // Apply special effects from collision.
    if( critter != nullptr ) {
        if( !critter->is_hallucination() ) {
            if( pl_ctrl ) {
                if( time_stunned > 0_turns ) {
                    //~ 1$s - vehicle name, 2$s - part name, 3$s - NPC or monster
                    add_msg( m_warning, _( "Your %1$s's %2$s rams into %3$s and stuns it!" ),
                             name, parts[ ret.part ].name(), ret.target_name );
                } else {
                    //~ 1$s - vehicle name, 2$s - part name, 3$s - NPC or monster
                    add_msg( m_warning, _( "Your %1$s's %2$s rams into %3$s!" ),
                             name, parts[ ret.part ].name(), ret.target_name );
                }
            }

            if( part_flag( ret.part, "SHARP" ) ) {
                critter->bleed();
            } else {
                sounds::sound( p, 20, sounds::sound_t::combat, snd, false, "smash_success", "hit_vehicle" );
            }
        }
    } else {
        if( pl_ctrl ) {
            if( !snd.empty() ) {
                //~ 1$s - vehicle name, 2$s - part name, 3$s - collision object name, 4$s - sound message
                add_msg( m_warning, _( "Your %1$s's %2$s rams into %3$s with a %4$s" ),
                         name, parts[ ret.part ].name(), ret.target_name, snd );
            } else {
                //~ 1$s - vehicle name, 2$s - part name, 3$s - collision object name
                add_msg( m_warning, _( "Your %1$s's %2$s rams into %3$s." ),
                         name, parts[ ret.part ].name(), ret.target_name );
            }
        }

        sounds::sound( p, smashed ? 80 : 50, sounds::sound_t::combat, snd, false, "smash_success",
                       "hit_vehicle" );
    }

    if( smashed && !vert_coll ) {
        int turn_amount = rng( 1, 3 ) * std::sqrt( static_cast<double>( part_dmg ) );
        turn_amount /= 15;
        if( turn_amount < 1 ) {
            turn_amount = 1;
        }
        turn_amount *= 15;
        if( turn_amount > 120 ) {
            turn_amount = 120;
        }
        int turn_roll = rng( 0, 100 );
        // Probability of skidding increases with higher delta_v
        if( turn_roll < std::abs( ( prev_velocity - coll_velocity ) / 100.0f * 2.0f ) ) {
            //delta_v = vel1 - vel1_a
            //delta_v = 50 mph -> 100% probability of skidding
            //delta_v = 25 mph -> 50% probability of skidding
            skidding = true;
            turn( units::from_degrees( one_in( 2 ) ? turn_amount : -turn_amount ) );
        }
    }

    ret.imp = impulse_to_damage( impulse_veh );
    return ret;
}

void vehicle::handle_trap( const tripoint &p, int part )
{
    int pwh = part_with_feature( part, VPFLAG_WHEEL, true );
    if( pwh < 0 ) {
        return;
    }
    map &here = get_map();
    Character &player_character = get_player_character();

    const trap &tr = here.tr_at( p );
    const trap_id t = tr.loadid;

    if( t == tr_null ) {
        // If the trap doesn't exist, we can't interact with it, so just return
        return;
    }
    vehicle_handle_trap_data veh_data = tr.vehicle_data;

    if( veh_data.is_falling ) {
        return;
    }

    const bool seen = player_character.sees( p );
    const bool known = player_character.knows_trap( p );
    if( seen ) {
        if( known ) {
            //~ %1$s: name of the vehicle; %2$s: name of the related vehicle part; %3$s: trap name
            add_msg( m_bad, _( "The %1$s's %2$s runs over %3$s." ), name, parts[ part ].name(), tr.name() );
        } else {
            add_msg( m_bad, _( "The %1$s's %2$s runs over something." ), name, parts[ part ].name() );
        }
    }

    if( veh_data.chance >= rng( 1, 100 ) ) {
        if( veh_data.sound_volume > 0 ) {
            sounds::sound( p, veh_data.sound_volume, sounds::sound_t::combat, veh_data.sound, false,
                           veh_data.sound_type, veh_data.sound_variant );
        }
        if( veh_data.do_explosion ) {
            explosion_handler::explosion( p, nullptr, veh_data.damage, 0.5f, false, veh_data.shrapnel );
        } else {
            // Hit the wheel directly since it ran right over the trap.
            damage_direct( pwh, veh_data.damage );
        }
        bool still_has_trap = true;
        if( veh_data.remove_trap || veh_data.do_explosion ) {
            here.remove_trap( p );
            still_has_trap = false;
        }
        for( const auto &it : veh_data.spawn_items ) {
            int cnt = roll_remainder( it.second );
            if( cnt > 0 ) {
                here.spawn_item( p, it.first, cnt );
            }
        }
        if( veh_data.set_trap ) {
            here.trap_set( p, veh_data.set_trap.id() );
            still_has_trap = true;
        }
        if( still_has_trap ) {
            const trap &tr = here.tr_at( p );
            if( seen || known ) {
                // known status has been reset by map::trap_set()
                player_character.add_known_trap( p, tr );
            }
            if( seen && !known ) {
                // hard to miss!
                const std::string direction = direction_name( direction_from( player_character.pos(), p ) );
                add_msg( _( "You've spotted a %1$s to the %2$s!" ), tr.name(), direction );
            }
        }
    }
}

bool vehicle::has_harnessed_animal() const
{
    for( size_t e = 0; e < parts.size(); e++ ) {
        const vehicle_part &vp = parts[ e ];
        if( vp.info().fuel_type == fuel_type_animal ) {
            monster *mon = get_pet( e );
            if( mon && mon->has_effect( effect_harnessed ) && mon->has_effect( effect_pet ) ) {
                return true;
            }
        }
    }
    return false;
}

void vehicle::selfdrive( point p )
{
    if( !is_towed() && !magic ) {
        for( size_t e = 0; e < parts.size(); e++ ) {
            const vehicle_part &vp = parts[ e ];
            if( vp.info().fuel_type == fuel_type_animal ) {
                monster *mon = get_pet( e );
                if( !mon || !mon->has_effect( effect_harnessed ) || !mon->has_effect( effect_pet ) ) {
                    is_following = false;
                    return;
                }
            }
        }
    }
    units::angle turn_delta = 15_degrees * p.x;
    const float handling_diff = handling_difficulty();
    if( turn_delta != 0_degrees ) {
        float eff = steering_effectiveness();
        if( eff == -2 ) {
            return;
        }

        if( eff < 0 ) {
            return;
        }

        if( eff == 0 ) {
            return;
        }
        turn( turn_delta );
    }
    if( p.y != 0 ) {
        int thr_amount = 100 * ( std::abs( velocity ) < 2000 ? 4 : 5 );
        if( cruise_on ) {
            cruise_thrust( -p.y * thr_amount );
        } else {
            thrust( -p.y );
        }
    }
    // TODO: Actually check if we're on land on water (or disable water-skidding)
    if( skidding && valid_wheel_config() ) {
        ///\EFFECT_DEX increases chance of regaining control of a vehicle

        ///\EFFECT_DRIVING increases chance of regaining control of a vehicle
        if( handling_diff * rng( 1, 10 ) < 15 ) {
            velocity = static_cast<int>( forward_velocity() );
            skidding = false;
            move.init( turn_dir );
        }
    }
}

bool vehicle::check_is_heli_landed()
{
    // @TODO - when there are chasms that extend below z-level 0 - perhaps the heli
    // will be able to descend into them but for now, assume z-level-0 == the ground.
    if( ( global_pos3().z == 0 || !get_map().has_flag_ter_or_furn( TFLAG_NO_FLOOR, global_pos3() ) ) &&
        !get_map().has_flag_ter_or_furn( TFLAG_DEEP_WATER, global_pos3() ) ) {
        is_flying = false;
        return true;
    }
    return false;
}

bool vehicle::check_heli_descend( Character &who )
{
    if( !is_aircraft() ) {
        debugmsg( "A vehicle is somehow flying without being an aircraft" );
        return true;
    }
    map &here = get_map();
    for( const tripoint &pt : get_points( true ) ) {
        tripoint below( pt.xy(), pt.z - 1 );
        if( here.has_zlevels() && ( pt.z < -OVERMAP_DEPTH ||
                                    !here.has_flag_ter_or_furn( TFLAG_NO_FLOOR, pt ) ) ) {
            who.add_msg_if_player( _( "You are already landed!" ) );
            return false;
        }
        const optional_vpart_position ovp = here.veh_at( below );
        if( here.impassable_ter_furn( below ) || here.has_flag_ter_or_furn( TFLAG_RAMP_DOWN, below ) ||
            ovp || g->critter_at( below ) ) {
            who.add_msg_if_player( m_bad,
                                   _( "It would be unsafe to try and land when there are obstacles below you." ) );
            return false;
        }
    }
    return true;

}

// Rename this?
bool vehicle::check_heli_ascend( Character &who )
{
    if( !is_aircraft() ) {
        debugmsg( "A vehicle is somehow flying without being an aircraft" );
        return true;
    }
    if( !is_flying_in_air() && check_on_ramp() ) {
        who.add_msg_if_player( m_bad,
                               _( "It would be unsafe to try and take off from an uneven surface." ) );
        return false;
    }
    map &here = get_map();
    for( const tripoint &pt : get_points( true ) ) {
        tripoint above( pt.xy(), pt.z + 1 );
        if( !here.inbounds_z( above.z ) ) {
            who.add_msg_if_player( m_bad, _( "It would be unsafe to try and ascend further." ) );
            return false;
        }
        bool has_ceiling = !here.has_flag_ter( TFLAG_NO_FLOOR, above );
        bool has_blocking_ter_furn = here.impassable_ter_furn( above );
        bool has_veh = here.veh_at( above ).has_value();
        bool has_critter = g->critter_at( above );
        if( has_ceiling || has_blocking_ter_furn || has_veh || has_critter ) {
            direction obstacle_direction = direction_from( ( pt - who.pos() ).xy() );
            const std::string direction_string = direction_name( obstacle_direction );
            std::string blocker_string;
            if( has_ceiling ) {
                blocker_string = _( "ceiling" );
            } else if( has_blocking_ter_furn ) {
                blocker_string = here.ter( above )->movecost == 0 ? here.tername( above ) : here.furnname( above );
            } else if( has_veh ) {
                blocker_string = here.veh_at( above )->vehicle().disp_name();
            } else if( has_critter ) {
                blocker_string = g->critter_at( above )->disp_name();
            } else {
                blocker_string = "BUGS";
            }
            if( obstacle_direction == direction::CENTER ) {
                who.add_msg_if_player( m_bad, _( "Your ascent is blocked by %s directly above you." ),
                                       blocker_string );
            } else {
                who.add_msg_if_player( m_bad, _( "Your ascent is blocked by %s to your %s." ), blocker_string,
                                       direction_string );
            }
            return false;
        }
    }
    return true;
}

void vehicle::pldrive( Character &driver, point p, int z )
{
    if( z != 0 && is_aircraft() ) {
        driver.moves = std::min( driver.moves, 0 );
        thrust( 0, z );
    }
    units::angle turn_delta = 15_degrees * p.x;
    const float handling_diff = handling_difficulty();
    if( turn_delta != 0_degrees ) {
        float eff = steering_effectiveness();
        if( eff == -2 ) {
            driver.add_msg_if_player( m_info,
                                      _( "You cannot steer an animal-drawn vehicle with no animal harnessed." ) );
            return;
        }

        if( eff < 0 ) {
            driver.add_msg_if_player( m_info,
                                      _( "This vehicle has no steering system installed, you can't turn it." ) );
            return;
        }

        if( eff == 0 ) {
            driver.add_msg_if_player( m_bad, _( "The steering is completely broken!" ) );
            return;
        }

        // If you've got more moves than speed, it's most likely time stop
        // Let's get rid of that
        driver.moves = std::min( driver.moves, driver.get_speed() );

        ///\EFFECT_DEX reduces chance of losing control of vehicle when turning

        ///\EFFECT_PER reduces chance of losing control of vehicle when turning

        ///\EFFECT_DRIVING reduces chance of losing control of vehicle when turning
        float skill = std::min( 10.0f, driver.get_skill_level( skill_driving ) +
                                ( driver.get_dex() + driver.get_per() ) / 10.0f );
        float penalty = rng_float( 0.0f, handling_diff ) - skill;
        int cost;
        if( penalty > 0.0f ) {
            // At 10 penalty (rather hard to get), we're taking 4 turns per turn
            cost = 100 * ( 1.0f + penalty / 2.5f );
        } else {
            // At 10 skill, with a perfect vehicle, we could turn up to 3 times per turn
            cost = std::max( driver.get_speed(), 100 ) * ( 1.0f - ( -penalty / 10.0f ) * 2 / 3 );
        }

        if( penalty > skill || cost > 400 ) {
            driver.add_msg_if_player( m_warning, _( "You fumble with the %s's controls." ), name );
            // Anything from a wasted attempt to 2 turns in the intended direction
            turn_delta *= rng( 0, 2 );
            // Also wastes next turn
            cost = std::max( cost, driver.moves + 100 );
        } else if( one_in( 10 ) ) {
            // Don't warn all the time or it gets spammy
            if( cost >= driver.get_speed() * 2 ) {
                driver.add_msg_if_player( m_warning, _( "It takes you a very long time to steer that vehicle!" ) );
            } else if( cost >= driver.get_speed() * 1.5f ) {
                driver.add_msg_if_player( m_warning, _( "It takes you a long time to steer that vehicle!" ) );
            }
        }

        turn( turn_delta );

        // At most 3 turns per turn, because otherwise it looks really weird and jumpy
        driver.moves -= std::max( cost, driver.get_speed() / 3 + 1 );
    }

    if( p.y != 0 ) {
        int thr_amount = 100 * ( std::abs( velocity ) < 2000 ? 4 : 5 );
        if( cruise_on ) {
            cruise_thrust( -p.y * thr_amount );
        } else {
            thrust( -p.y );
            driver.moves = std::min( driver.moves, 0 );
        }
    }

    // TODO: Actually check if we're on land on water (or disable water-skidding)
    // Only check for recovering from a skid if we did active steering (not cruise control).
    if( skidding && ( p.x != 0 || ( p.y != 0 && !cruise_on ) ) && valid_wheel_config() ) {
        ///\EFFECT_DEX increases chance of regaining control of a vehicle

        ///\EFFECT_DRIVING increases chance of regaining control of a vehicle
        if( handling_diff * rng( 1, 10 ) <
            driver.dex_cur + driver.get_skill_level( skill_driving ) * 2 ) {
            driver.add_msg_if_player( _( "You regain control of the %s." ), name );
            driver.as_player()->practice( skill_driving, velocity / 5 );
            velocity = static_cast<int>( forward_velocity() );
            skidding = false;
            move.init( turn_dir );
        }
    }
}

// A chance to stop skidding if moving in roughly the faced direction
void vehicle::possibly_recover_from_skid()
{
    if( last_turn > 13_degrees ) {
        // Turning on the initial skid is delayed, so move==face, initially. This filters out that case.
        return;
    }

    rl_vec2d mv = move_vec();
    rl_vec2d fv = face_vec();
    float dot = mv.dot_product( fv );
    // Threshold of recovery is Gaussianesque.

    if( std::fabs( dot ) * 100 > dice( 9, 20 ) ) {
        add_msg( _( "The %s recovers from its skid." ), name );
        // face_vec takes over.
        skidding = false;
        // Wheels absorb horizontal velocity.
        velocity *= dot;
        if( dot < -.8 ) {
            // Pointed backwards, velo-wise.
            // Move backwards.
            velocity *= -1;
        }

        move = face;
    }
}

// if not skidding, move_vec == face_vec, mv <dot> fv == 1, velocity*1 is returned.
float vehicle::forward_velocity() const
{
    rl_vec2d mv = move_vec();
    rl_vec2d fv = face_vec();
    float dot = mv.dot_product( fv );
    return velocity * dot;
}

rl_vec2d vehicle::velo_vec() const
{
    rl_vec2d ret;
    if( skidding ) {
        ret = move_vec();
    } else {
        ret = face_vec();
    }
    ret = ret.normalized();
    ret = ret * velocity;
    return ret;
}

static inline rl_vec2d angle_to_vec( units::angle angle )
{
    return rl_vec2d( units::cos( angle ), units::sin( angle ) );
}

// normalized.
rl_vec2d vehicle::move_vec() const
{
    return angle_to_vec( move.dir() );
}

// normalized.
rl_vec2d vehicle::face_vec() const
{
    return angle_to_vec( face.dir() );
}

rl_vec2d vehicle::dir_vec() const
{
    return angle_to_vec( turn_dir );
}
// Takes delta_v in m/s, returns collision factor. Ranges from 1 at 0m/s to 0.3 at approx ~60mph.
// Changed from e min of 0.1 as this is a nearly perfectly plastic collision, which is not common outside of vehicles with engineered crumple zones. Cata vehicles dont have crumple zones.
float get_collision_factor( const float delta_v )
{
    if( std::abs( delta_v ) <= 26.8224 ) {
        return ( 1 - ( 0.7 * std::abs( delta_v ) ) / 26.8224 );
    } else {
        return 0.3;
    }
}

vehicle *vehicle::act_on_map()
{
    const tripoint pt = global_pos3();
    map &here = get_map();
    if( !here.inbounds( pt ) ) {
        dbg( DL::Info ) << "stopping out-of-map vehicle at global pos " << pt;
        stop( false );
        of_turn = 0;
        is_falling = false;
        return this;
    }
    if( decrement_summon_timer() ) {
        return nullptr;
    }
    Character &player_character = get_player_character();
    const bool pl_ctrl = player_in_control( player_character );
    // TODO: Remove this hack, have vehicle sink a z-level
    if( is_floating && !can_float() ) {
        if( has_sufficient_lift() ) {
            is_floating = false;
            is_flying = true;
        } else {
            add_msg( m_bad, _( "Your %s sank." ), name );
            if( pl_ctrl ) {
                unboard_all();
            }
            if( g->remoteveh() == this ) {
                g->setremoteveh( nullptr );
            }

            here.on_vehicle_moved( sm_pos.z );
            // Destroy vehicle (sank to nowhere)
            here.destroy_vehicle( this );
            return nullptr;
        }
    }

    // It needs to fall when it has no support OR was falling before
    //  so that vertical collisions happen.
    const bool should_fall = is_falling || vertical_velocity != 0;

    // TODO: Saner diagonal movement, so that you can jump off cliffs properly
    // The ratio of vertical to horizontal movement should be vertical_velocity/velocity
    //  for as long as of_turn doesn't run out.
    if( should_fall ) {
        // Convert from 100*mph to m/s
        const float old_vel = vmiph_to_mps( vertical_velocity );
        // Formula is v_2 = sqrt( 2*d*g + v_1^2 )
        // Note: That drops the sign
        const float new_vel = -std::sqrt( 2 * tile_height * GRAVITY_OF_EARTH + old_vel * old_vel );
        vertical_velocity = mps_to_vmiph( new_vel );
        is_falling = true;
    } else {
        // Not actually falling, was just marked for fall test
        is_falling = false;
    }

    // Low enough for bicycles to go in reverse.
    // If the movement is due to a change in z-level, i.e a helicopter then the lateral movement will often be zero.
    if( !should_fall && std::abs( velocity ) < 20 && requested_z_change == 0 ) {
        stop();
        of_turn -= .321f;
        return this;
    }

    const float wheel_traction_area = here.vehicle_wheel_traction( *this );
    const float traction = k_traction( wheel_traction_area );
    if( traction < 0.001f ) {
        of_turn = 0;
        if( !should_fall ) {
            stop();
            if( floating.empty() ) {
                add_msg( m_info, _( "Your %s can't move on this terrain." ), name );
            } else {
                add_msg( m_info, _( "Your %s is beached." ), name );
            }
            return this;
        }
    }
    const float turn_cost = vehicles::vmiph_per_tile / std::max<float>( 0.0001f, std::abs( velocity ) );

    // Can't afford it this turn?
    // Low speed shouldn't prevent vehicle from falling, though
    bool falling_only = false;
    if( turn_cost >= of_turn && ( ( !is_flying && requested_z_change == 0 ) || !is_aircraft() ) ) {
        if( !should_fall ) {
            of_turn_carry = of_turn;
            of_turn = 0;
            return this;
        }
        falling_only = true;
    }

    // Decrease of_turn if falling+moving, but not when it's lower than move cost
    if( !falling_only ) {
        of_turn -= turn_cost;
    }

    const bool can_use_rails = this->can_use_rails();
    const bool is_on_rails = vehicle_movement::is_on_rails( here, *this );
    if( one_in( 10 ) ) {
        bool controlled = false;
        // It can even be a NPC, but must be at the controls
        for( int boarded : boarded_parts() ) {
            if( part_with_feature( boarded, VPFLAG_CONTROLS, true ) >= 0 ) {
                controlled = true;
                player *passenger = get_passenger( boarded );
                if( passenger != nullptr ) {
                    passenger->practice( skill_driving, 1 );
                }
            }
        }

        // Eventually send it skidding if no control
        // But not if it's remotely controlled, is in water or is on rails
        if( !controlled && !pl_ctrl && !is_floating && !is_on_rails && !is_flying &&
            requested_z_change == 0 ) {
            skidding = true;
        }
    }

    if( skidding && one_in( 4 ) ) {
        // Might turn uncontrollably while skidding
        turn( one_in( 2 ) ? -15_degrees : 15_degrees );
    }

    if( should_fall ) {
        // TODO: Insert a (hard) driving test to stop this from happening
        skidding = true;
    }

    vehicle_movement::rail_processing_result rpres;
    if( can_use_rails && !falling_only ) {
        rpres = vehicle_movement::process_movement_on_rails( here, *this );
    }
    if( rpres.do_turn ) {
        turn_dir = rpres.turn_dir;
    }

    // The direction we're moving
    tileray mdir;
    if( skidding || should_fall ) {
        // If skidding, it's the move vector
        // Same for falling - no air control
        mdir = move;
    } else if( turn_dir != face.dir() && ( !is_on_rails || rpres.do_turn ) ) {
        // Driver turned vehicle, get turn_dir
        mdir.init( turn_dir );
    } else {
        // Not turning, keep face.dir
        mdir = face;
    }

    tripoint dp;
    if( std::abs( velocity ) >= 20 && !falling_only ) {
        mdir.advance( velocity < 0 ? -1 : 1 );
        if( rpres.do_shift ) {
            dp.x = rpres.shift_amount.x;
            dp.y = rpres.shift_amount.y;
        } else {
            dp.x = mdir.dx();
            dp.y = mdir.dy();
        }
    }

    if( should_fall ) {
        dp.z = -1;
        is_flying = false;
    } else {
        dp.z = requested_z_change;
        requested_z_change = 0;
        if( dp.z > 0 && is_aircraft() ) {
            is_flying = true;
        }
    }

    return here.move_vehicle( *this, dp, mdir );
}

void vehicle::shift_zlevel()
{
    map &here = get_map();
    int center = part_at( point_zero );

    int z_shift = 0;
    if( center == -1 ) {
        //no center part, fall back to slower terrain check
        auto global_center = mount_to_tripoint( point_zero );
        if( here.has_flag( TFLAG_RAMP_DOWN, global_center ) ) {
            z_shift = -1;
        } else if( here.has_flag( TFLAG_RAMP_UP, global_center ) ) {
            z_shift = 1;
        }
    } else {
        z_shift = parts[center].precalc[0].z;
    }

    if( z_shift != 0 ) {
        here.shift_vehicle_z( *this, z_shift );
    }
}

bool vehicle::check_on_ramp( int idir, const tripoint &offset ) const
{
    for( auto &prt : get_all_parts() ) {
        tripoint partPoint = global_pos3() + offset + prt.part().precalc[idir];

        if( g->m.has_flag( TFLAG_RAMP_UP, partPoint ) || g->m.has_flag( TFLAG_RAMP_DOWN, partPoint ) ) {
            return true;
        }
    }
    return false;
}

void vehicle::adjust_zlevel( int idir, const tripoint &offset )
{
    //We don't need to do anything if we're not on a ramp
    //unless position 0 is outside the vehicle then there may be a ramp in between
    if( part_at( point_zero ) != -1 && !check_on_ramp( idir, offset ) ) {
        return;
    }

    // when a vehicle part enters the low end of a down ramp, or the high end of an up ramp,
    // it immediately translates down or up a z-level, respectively, ending up on the low
    // end of an up ramp or high end of a down ramp, respectively.  The two ends are set
    // past each other, like so:
    // (side view)  z+1   Rdh RDl
    //              z+0   RUh Rul
    // A vehicle moving left to right on z+1 drives down to z+0 by entering the ramp down low end.
    // A vehicle moving right to left on z+0 drives up to z+1 by entering the ramp up high end.
    // A vehicle moving left to right on z+0 should ideally collide into a wall before entering
    //   the ramp up high end, but even if it does, it briefly transitions to z+1 before returning
    //   to z0 by entering the ramp down low end.
    // A vehicle moving right to left on z+1 drives down to z+0 by entering the ramp down low end,
    //   then immediately returns to z+1 by entering the ramp up high end.
    // When a vehicle's central point transitions a z-level via a ramp, all other pre-calc points
    // make the opposite transition, so that points that were above an ascending pivot point are
    // now level with it, and parts that were level with an ascending pivot point are now below
    // it.

    auto &m = get_map();
    tripoint global_pos = global_pos3();

    tripoint new_center = global_pos + offset;

    if( m.has_flag( TFLAG_RAMP_DOWN, new_center ) ) {
        new_center.z--;
    } else if( m.has_flag( TFLAG_RAMP_UP, new_center ) ) {
        new_center.z++;
    }

    std::map<point, int> z_cache;

    //Draw a line from the center to each part, going up and down ramps as we do
    for( auto &prt : get_all_parts() ) {
        tripoint part_point = global_pos + offset + prt.part().precalc[idir];

        auto cache_entry = z_cache.find( part_point.xy() );
        if( cache_entry != z_cache.end() ) {
            prt.part().precalc[idir].z = cache_entry->second;
            continue;
        }

        tripoint line = new_center;
        while( line.xy() != part_point.xy() ) {
            if( line.x < part_point.x ) {
                line.x++;
            } else if( line.x > part_point.x ) {
                line.x--;
            }
            if( line.y < part_point.y ) {
                line.y++;
            } else if( line.y > part_point.y ) {
                line.y--;
            }
            if( m.has_flag( TFLAG_RAMP_UP, line ) ) {
                line.z += 1;
            }
            if( m.has_flag( TFLAG_RAMP_DOWN, line ) ) {
                line.z -= 1;
            }
        }
        prt.part().precalc[idir].z = line.z - global_pos.z;
        z_cache[part_point.xy()] = line.z - global_pos.z;
    }
}

void vehicle::check_falling_or_floating()
{
    // TODO: Make the vehicle "slide" towards its center of weight
    //  when it's not properly supported
    const auto &pts = get_points( true );
    if( pts.empty() ) {
        // Dirty vehicle with no parts
        is_falling = false;
        is_floating = false;
        in_water = false;
        is_flying = false;
        return;
    }

    map &here = get_map();
    is_falling = here.has_zlevels();

    if( is_flying && is_aircraft() ) {
        is_falling = false;
    } else {
        is_flying = false;
    }

    size_t deep_water_tiles = 0;
    size_t water_tiles = 0;
    for( const tripoint &p : pts ) {
        if( is_falling ) {
            tripoint below( p.xy(), p.z - 1 );
            is_falling &= here.has_flag_ter_or_furn( TFLAG_NO_FLOOR, p ) &&
                          ( p.z > -OVERMAP_DEPTH ) && !here.supports_above( below );
        }
        deep_water_tiles += here.has_flag( TFLAG_DEEP_WATER, p ) ? 1 : 0;
        water_tiles += here.has_flag( TFLAG_SWIMMABLE, p ) ? 1 : 0;
    }

    if( is_falling && is_aircraft() ) {
        is_falling = false;
        is_flying = true;
    }

    // floating if 2/3rds of the vehicle is in deep water
    is_floating = !is_flying && ( 3 * deep_water_tiles >= 2 * pts.size() );
    // in_water if 1/2 of the vehicle is in water at all
    in_water =  2 * water_tiles >= pts.size();
}

float map::vehicle_wheel_traction( const vehicle &veh,
                                   const bool ignore_movement_modifiers /*=false*/ ) const
{
    if( veh.is_in_water( true ) ) {
        return veh.can_float() ? 1.0f : -1.0f;
    }
    if( veh.is_in_water() && veh.is_watercraft() && veh.can_float() ) {
        return 1.0f;
    }

    const auto &wheel_indices = veh.wheelcache;
    int num_wheels = wheel_indices.size();
    if( num_wheels == 0 ) {
        // TODO: Assume it is digging in dirt
        // TODO: Return something that could be reused for dragging
        return 0.0f;
    }

    float traction_wheel_area = 0.0f;

    if( vehicle_movement::is_on_rails( *this, veh ) ) {
        // Vehicles on rails are considered to have all of their wheels on rails
        for( int p : veh.rail_wheelcache ) {
            traction_wheel_area += veh.cpart( p ).wheel_area();
        }
        return traction_wheel_area;
    }

    for( int p : wheel_indices ) {
        const tripoint &pp = veh.global_part_pos3( p );
        const int wheel_area = veh.cpart( p ).wheel_area();

        const auto &tr = ter( pp ).obj();
        // Deep water and air
        if( tr.has_flag( TFLAG_DEEP_WATER ) || tr.has_flag( TFLAG_NO_FLOOR ) ) {
            // No traction from wheel in water or air
            continue;
        }

        int move_mod = move_cost_ter_furn( pp );
        if( move_mod == 0 ) {
            // Vehicle locked in wall
            // Shouldn't happen, but does
            return 0.0f;
        }

        for( const auto &terrain_mod : veh.part_info( p ).wheel_terrain_mod() ) {
            if( !tr.has_flag( terrain_mod.first ) ) {
                move_mod += terrain_mod.second;
                break;
            }
        }

        // Ignore the movement modifier if needed.
        if( ignore_movement_modifiers ) {
            move_mod = 2;
        }

        traction_wheel_area += 2.0 * wheel_area / move_mod;
    }

    return traction_wheel_area;
}

units::angle map::shake_vehicle( vehicle &veh, const int velocity_before,
                                 const units::angle direction )
{
    const int d_vel = std::abs( veh.velocity - velocity_before ) / 100;

    std::vector<rider_data> riders = veh.get_riders();

    units::angle coll_turn = 0_degrees;
    for( const rider_data &r : riders ) {
        const int ps = r.prt;
        Creature *rider = r.psg;
        if( rider == nullptr ) {
            debugmsg( "throw passenger: empty passenger at part %d", ps );
            continue;
        }

        const tripoint part_pos = veh.global_part_pos3( ps );
        if( rider->pos() != part_pos ) {
            debugmsg( "throw passenger: passenger at %d,%d,%d, part at %d,%d,%d",
                      rider->posx(), rider->posy(), rider->posz(),
                      part_pos.x, part_pos.y, part_pos.z );
            veh.part( ps ).remove_flag( vehicle_part::passenger_flag );
            continue;
        }

        player *psg = dynamic_cast<player *>( rider );
        monster *pet = dynamic_cast<monster *>( rider );

        bool throw_from_seat = false;
        int move_resist = 1;
        if( psg ) {
            ///\EFFECT_STR reduces chance of being thrown from your seat when not wearing a seatbelt
            move_resist = psg->str_cur * 150 + 500;
            if( veh.part( ps ).info().has_flag( "SEAT_REQUIRES_BALANCE" ) ) {
                // Much harder to resist being thrown on a skateboard-like vehicle.
                // Penalty mitigated by Deft and Skater.
                int resist_penalty = 500;
                if( psg->has_trait( trait_PROF_SKATER ) ) {
                    resist_penalty -= 150;
                }
                if( psg->has_trait( trait_DEFT ) ) {
                    resist_penalty -= 150;
                }
                move_resist -= resist_penalty;
            }
        } else {
            int pet_resist = 0;
            if( pet != nullptr ) {
                pet_resist = static_cast<int>( to_kilogram( pet->get_weight() ) * 200 );
            }
            move_resist = std::max( 100, pet_resist );
        }
        if( veh.part_with_feature( ps, VPFLAG_SEATBELT, true ) == -1 ) {
            ///\EFFECT_STR reduces chance of being thrown from your seat when not wearing a seatbelt
            throw_from_seat = d_vel * rng( 80, 120 ) > move_resist;
        }

        // Damage passengers if d_vel is too high
        if( !throw_from_seat && ( 10 * d_vel ) > 6 * rng( 50, 100 ) ) {
            const int dmg = d_vel * rng( 70, 100 ) / 400;
            if( psg ) {
                psg->hurtall( dmg, nullptr );
                psg->add_msg_player_or_npc( m_bad,
                                            _( "You take %d damage by the power of the impact!" ),
                                            _( "<npcname> takes %d damage by the power of the "
                                               "impact!" ),  dmg );
            } else {
                pet->apply_damage( nullptr, bodypart_id( "torso" ), dmg );
            }
        }

        if( psg && veh.player_in_control( *psg ) ) {
            const int lose_ctrl_roll = rng( 0, d_vel );
            ///\EFFECT_DEX reduces chance of losing control of vehicle when shaken

            ///\EFFECT_DRIVING reduces chance of losing control of vehicle when shaken
            if( lose_ctrl_roll > psg->dex_cur * 2 + psg->get_skill_level( skill_driving ) * 3 ) {
                psg->add_msg_player_or_npc( m_warning,
                                            _( "You lose control of the %s." ),
                                            _( "<npcname> loses control of the %s." ), veh.name );
                int turn_amount = rng( 1, 3 ) * std::sqrt( std::abs( veh.velocity ) ) / 30;
                if( turn_amount < 1 ) {
                    turn_amount = 1;
                }
                units::angle turn_angle = std::min( turn_amount * 15_degrees, 120_degrees );
                coll_turn = one_in( 2 ) ? turn_angle : -turn_angle;
            }
        }

        if( throw_from_seat ) {
            if( psg ) {
                psg->add_msg_player_or_npc( m_bad,
                                            _( "You are hurled from the %s's seat by "
                                               "the power of the impact!" ),
                                            _( "<npcname> is hurled from the %s's seat by "
                                               "the power of the impact!" ), veh.name );
                unboard_vehicle( part_pos );
            } else if( get_player_character().sees( part_pos ) ) {
                add_msg( m_bad, _( "%s is hurled from %s's by the power of the impact!" ),
                         pet->disp_name( false, true ), veh.name );
            }
            ///\EFFECT_STR reduces distance thrown from seat in a vehicle impact
            g->fling_creature( rider, direction + rng_float( -30_degrees, 30_degrees ),
                               std::max( 10, d_vel - move_resist / 100 ) );
        }
    }

    return coll_turn;
}

namespace vehicle_movement
{
static bool scan_rails_from_veh_internal(
    const map &m,
    const vehicle &veh,
    tripoint scan_initial_pos,
    point veh_plus_y_vec,
    point scan_vec )
{
    for( size_t rail_id = 0; rail_id < veh.rail_profile.size(); rail_id++ ) {
        int rail_y_rel_to_pivot = veh.rail_profile[rail_id] - veh.pivot_point().y;
        tripoint scan_pos = scan_initial_pos + rail_y_rel_to_pivot * veh_plus_y_vec;
        for( int step = 0; step < 3; step++ ) {
            tripoint p = scan_pos + scan_vec * step;
            bool rail_here = m.has_flag_ter_or_furn( TFLAG_RAIL, p );
            if( !rail_here ) {
                // Terrain is not a rail
                return false;
            }
        }
    }
    return true;
}

// Get number of rotations of identity vector
static inline int get_num_cw_rots_of_ray_delta( point v )
{
    if( v == point_north_east ) {
        return 0;
    } else if( v == point_south_east ) {
        return 1;
    } else if( v == point_south_west ) {
        return 2;
    } else {
        return 3;
    }
}

static bool scan_rails_at_shift(
    const map &m,
    const vehicle &veh,
    int velocity_sign,
    units::angle dir,
    int shift_sign,
    tripoint *shift_amt = nullptr )
{
    point ray_delta;
    {
        tileray ray( dir );
        ray.advance( 1 );
        ray_delta.x = ray.dx();
        ray_delta.y = ray.dy();
    }
    if( ray_delta.x != 0 && ray_delta.y != 0 ) {
        // We can't cleanly map diagonally oriented vehicles to rail turns.
        // As such, treat the vehicle as if it can have either skew at the same time.
        int num_cw_rots = get_num_cw_rots_of_ray_delta( ray_delta );
        point rd_l = point_north.rotate( num_cw_rots );
        point rd_r = point_east.rotate( num_cw_rots );

        point vyp_l = point_east.rotate( num_cw_rots );
        point vyp_r = point_south.rotate( num_cw_rots );

        tripoint scan_start = veh.global_pos3();
        if( shift_sign > 0 ) {
            scan_start += rd_r * velocity_sign;
        } else if( shift_sign < 0 ) {
            scan_start += rd_l * velocity_sign;
        }

        point scan_vec = ray_delta * velocity_sign;

        bool scan_res_l = scan_rails_from_veh_internal( m, veh, scan_start, vyp_l, scan_vec );
        bool scan_res_r = scan_rails_from_veh_internal( m, veh, scan_start, vyp_r, scan_vec );

        if( scan_res_l || scan_res_r ) {
            if( shift_amt ) {
                *shift_amt = scan_start - veh.global_pos3();
            }
            return true;
        }
    } else {
        point veh_plus_y_vec = ray_delta.rotate( 1 );
        point scan_vec = ray_delta * velocity_sign;
        tripoint scan_start = veh.global_pos3();
        if( shift_sign != 0 ) {
            scan_start += scan_vec + veh_plus_y_vec * shift_sign;
        }
        if( scan_rails_from_veh_internal( m, veh, scan_start, veh_plus_y_vec, scan_vec ) ) {
            if( shift_amt ) {
                *shift_amt = scan_start - veh.global_pos3();
            }
            return true;
        }
    }
    return false;
}

static inline rail_processing_result make_none()
{
    return rail_processing_result();
}

static inline rail_processing_result make_turn( units::angle a )
{
    rail_processing_result res;
    res.do_turn = true;
    res.turn_dir = a;
    return res;
}

static inline rail_processing_result make_shift( tripoint dp )
{
    rail_processing_result res;
    res.do_shift = true;
    res.shift_amount = dp;
    return res;
}

rail_processing_result process_movement_on_rails( const map &m, const vehicle &veh )
{
    int face_dir_degrees = std::round( units::to_degrees( veh.face.dir() ) );
    int face_dir_snapped = ( face_dir_degrees / 45 ) * 45;

    units::angle dir_straight = normalize( units::from_degrees( face_dir_snapped ) );
    units::angle dir_left = normalize( dir_straight - 45_degrees );
    units::angle dir_right = normalize( dir_straight + 45_degrees );

    int vel_sign = veh.velocity > 0 ? 1 : -1;

    bool can_go_straight = scan_rails_at_shift( m, veh, vel_sign, dir_straight, 0 );
    bool can_turn_left = scan_rails_at_shift( m, veh, vel_sign, dir_left, 0 );
    bool can_turn_right = scan_rails_at_shift( m, veh, vel_sign, dir_right, 0 );

    bool can_go_backwards = scan_rails_at_shift( m, veh, -vel_sign, dir_straight, 0 );

    tripoint shift_amount_right;
    tripoint shift_amount_left;

    bool can_shift_right = scan_rails_at_shift( m, veh, vel_sign, dir_straight, vel_sign,
                           &shift_amount_right );
    bool can_shift_left = scan_rails_at_shift( m, veh, vel_sign, dir_straight, -vel_sign,
                          &shift_amount_left );

    bool is_on_rails = face_dir_degrees == face_dir_snapped && ( can_go_straight || can_go_backwards );

    // Appraise possible vehicle orientations
    if( !is_on_rails ) {
        // The vehicle is derailed, attempt to get back on rails
        if( can_go_straight ) {
            return make_turn( dir_straight );
        }
    } else {
        if( veh.face.dir() == veh.turn_dir ) {
            // Automatic movement - prefer going straight.
            if( can_go_straight ) {
                return make_none();
            } else if( can_turn_left ) {
                return make_turn( dir_left );
            } else if( can_shift_left ) {
                return make_shift( shift_amount_left );
            } else if( can_turn_right ) {
                return make_turn( dir_right );
            } else if( can_shift_right ) {
                return make_shift( shift_amount_right );
            }
        } else {
            // Manual movement - prefer going in turn direction.
            units::angle dir_delta = normalize( veh.turn_dir - veh.face.dir() );
            if( dir_delta < 180_degrees ) {
                // Trying to turn right
                if( can_turn_right ) {
                    return make_turn( dir_right );
                } else if( can_shift_right ) {
                    return make_shift( shift_amount_right );
                } else if( can_go_straight ) {
                    return make_none();
                } else if( can_turn_left ) {
                    return make_turn( dir_left );
                } else if( can_shift_left ) {
                    return make_shift( shift_amount_left );
                }
            } else {
                // Trying to turn left
                if( can_turn_left ) {
                    return make_turn( dir_left );
                } else if( can_shift_left ) {
                    return make_shift( shift_amount_left );
                } else if( can_go_straight ) {
                    return make_none();
                } else if( can_turn_right ) {
                    return make_turn( dir_right );
                } else if( can_shift_right ) {
                    return make_shift( shift_amount_right );
                }
            }
        }
    }
    return make_none();
}

bool is_on_rails( const map &m, const vehicle &veh )
{
    if( !veh.can_use_rails() ) {
        // Must be rail-worthy
        return false;
    }

    int face_dir_degrees = std::round( units::to_degrees( veh.face.dir() ) );
    int face_dir_snapped = ( face_dir_degrees / 45 ) * 45;

    if( face_dir_degrees != face_dir_snapped ) {
        // When moving on rails, can only rotate in 45-degree increment
        return false;
    }

    // Must have valid rail segment in front of or behind us
    units::angle dir_straight = normalize( units::from_degrees( face_dir_snapped ) );
    return scan_rails_at_shift( m, veh, 1, dir_straight, 0 ) ||
           scan_rails_at_shift( m, veh, -1, dir_straight, 0 );
}

} // namespace vehicle_movement
