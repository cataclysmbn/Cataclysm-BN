#include "vehicle.h"
#include "detached_ptr.h"
#include "units_mass.h"
#include "vehicle_part.h" // IWYU pragma: associated
#include "vpart_position.h" // IWYU pragma: associated
#include "vpart_range.h" // IWYU pragma: associated

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "active_tile_data_def.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "bionics.h"
#include "cata_utility.h"
#include "character.h"
#include "clzones.h"
#include "coordinate_conversions.h"
#include "coordinates.h"
#include "creature.h"
#include "cuboid_rectangle.h"
#include "debug.h"
#include "distribution_grid.h"
#include "enums.h"
#include "event.h"
#include "event_bus.h"
#include "explosion.h"
#include "faction.h"
#include "field_type.h"
#include "flag.h"
#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "item_group.h"
#include "itype.h"
#include "json.h"
#include "make_static.h"
#include "map.h"
#include "map_iterator.h"
#include "mapbuffer.h"
#include "mapdata.h"
#include "messages.h"
#include "monster.h"
#include "npc.h"
#include "options.h"
#include "output.h"
#include "overmapbuffer.h"
#include "player.h"
#include "player_activity.h"
#include "point_float.h"
#include "rng.h"
#include "sounds.h"
#include "string_formatter.h"
#include "string_id.h"
#include "submap.h"
#include "translations.h"
#include "units_utility.h"
#include "veh_type.h"
#include "vehicle_functions.h"
#include "weather.h"
#include "ui.h"
/*
 * Speed up all those if ( blarg == "structure" ) statements that are used everywhere;
 *   assemble "structure" once here instead of repeatedly later.
 */
static const std::string part_location_structure( "structure" );
static const std::string part_location_center( "center" );
static const std::string part_location_onroof( "on_roof" );

static const itype_id fuel_type_animal( "animal" );
static const itype_id fuel_type_battery( "battery" );
static const itype_id fuel_type_muscle( "muscle" );
static const itype_id fuel_type_plutonium_cell( "plut_cell" );
static const itype_id fuel_type_wind( "wind" );

static const fault_id fault_belt( "fault_engine_belt_drive" );
static const fault_id fault_filter_air( "fault_engine_filter_air" );
static const fault_id fault_filter_fuel( "fault_engine_filter_fuel" );
static const fault_id fault_immobiliser( "fault_engine_immobiliser" );

static const activity_id ACT_VEHICLE( "ACT_VEHICLE" );

static const bionic_id bio_jointservo( "bio_jointservo" );

static const efftype_id effect_harnessed( "harnessed" );

static const itype_id itype_battery( "battery" );
static const itype_id itype_plut_cell( "plut_cell" );
static const itype_id itype_water( "water" );
static const itype_id itype_water_clean( "water_clean" );
static const itype_id itype_water_purifier( "water_purifier" );
static const vpart_id vp_door_lock( "door_lock" );

static const flag_id f_VEHICLE_UNLOCKED( "VEHICLE_UNLOCKED" );
static const flag_id f_VEHICLE_LOCKED( "VEHICLE_LOCKED" );
static const flag_id f_VEHICLE_NO_LOCKS( "VEHICLE_NO_LOCKS" );

static const flag_id f_VEHICLE_NO_HOTWIRE( "VEHICLE_NO_HOTWIRE" );
static const flag_id f_VEHICLE_HOTWIRE( "VEHICLE_HOTWIRE" );

static const std::string str_DOOR_LOCKING( "DOOR_LOCKING" );
static const std::string str_OPENCLOSE_INSIDE( "OPENCLOSE_INSIDE" );

static const std::vector<std::string> vs_NO_HOTWIRING = {
    "MUSCLE_LEGS",
    "MUSCLE_ARMS",
    "ANIMAL_CTRL",
};

static bool is_sm_tile_outside( const tripoint &real_global_pos );
static bool is_sm_tile_over_water( const tripoint &real_global_pos );

static const itype_id fuel_type_mana( "mana" );

// 1 kJ per battery charge
const int bat_energy_j = 1000;

// For reference what each function is supposed to do, see their implementation in
// @ref DefaultRemovePartHandler. Add compatible code for it into @ref MapgenRemovePartHandler,
// if needed.
class RemovePartHandler
{
    public:
        virtual ~RemovePartHandler() = default;

        virtual void unboard( const tripoint &loc ) = 0;
        virtual detached_ptr<item> add_item_or_charges( const tripoint &loc, detached_ptr<item> &&it,
                bool permit_oob ) = 0;
        virtual void set_transparency_cache_dirty( int z ) = 0;
        virtual void set_floor_cache_dirty( int z ) = 0;
        virtual void removed( vehicle &veh, int part ) = 0;
        virtual void spawn_animal_from_part( item &base, const tripoint &loc ) = 0;
};

class DefaultRemovePartHandler : public RemovePartHandler
{
    public:
        ~DefaultRemovePartHandler() override = default;

        void unboard( const tripoint &loc ) override {
            g->m.unboard_vehicle( loc );
        }
        detached_ptr<item> add_item_or_charges( const tripoint &loc, detached_ptr<item> &&it,
                                                bool /*permit_oob*/ ) override {
            return g->m.add_item_or_charges( loc, std::move( it ) );
        }
        void set_transparency_cache_dirty( const int z ) override {
            map &here = get_map();
            here.set_transparency_cache_dirty( z );
            here.set_seen_cache_dirty( tripoint_zero );
        }
        void set_floor_cache_dirty( const int z ) override {
            get_map().set_floor_cache_dirty( z );
        }
        void removed( vehicle &veh, const int part ) override {
            avatar &player_character = get_avatar();
            // If the player is currently working on the removed part, stop them as it's futile now.
            const player_activity &act = *player_character.activity;
            map &here = get_map();
            if( act.id() == ACT_VEHICLE && act.moves_left > 0 && act.values.size() > 6 ) {
                if( veh_pointer_or_null( here.veh_at( tripoint( act.values[0], act.values[1],
                                                      player_character.posz() ) ) ) == &veh ) {
                    if( act.values[6] >= part ) {
                        player_character.cancel_activity();
                        add_msg( m_info, _( "The vehicle part you were working on has gone!" ) );
                    }
                }
            }
            // TODO: maybe do this for all the nearby NPCs as well?

            if( g->u.get_grab_type() == OBJECT_VEHICLE && g->u.grab_point == veh.global_part_pos3( part ) ) {
                if( veh.parts_at_relative( veh.part( part ).mount, false ).empty() ) {
                    add_msg( m_info, _( "The vehicle part you were holding has been destroyed!" ) );
                    g->u.grab( OBJECT_NONE );
                }
            }

            here.dirty_vehicle_list.insert( &veh );
        }
        void spawn_animal_from_part( item &base, const tripoint &loc ) override {
            base.release_monster( loc, 1 );
        }
};

class MapgenRemovePartHandler : public RemovePartHandler
{
    private:
        map &m;

    public:
        MapgenRemovePartHandler( map &m ) : m( m ) { }

        ~MapgenRemovePartHandler() override = default;

        void unboard( const tripoint &/*loc*/ ) override {
            debugmsg( "Tried to unboard during mapgen!" );
            // Ignored. Will almost certainly not be called anyway, because
            // there are no creatures that could have been mounted during mapgen.
        }
        detached_ptr<item> add_item_or_charges( const tripoint &loc, detached_ptr<item> &&it,
                                                bool permit_oob ) override {
            if( !m.inbounds( loc ) ) {
                if( !permit_oob ) {
                    debugmsg( "Tried to put item %s on invalid tile %s during mapgen!",
                              it->tname(), loc.to_string() );
                }
                tripoint copy = loc;
                m.clip_to_bounds( copy );
                assert( m.inbounds( copy ) ); // prevent infinite recursion
                return add_item_or_charges( copy, std::move( it ), false );
            }
            return m.add_item_or_charges( loc, std::move( it ) );
        }
        void set_transparency_cache_dirty( const int /*z*/ ) override {
            // Ignored for now. We don't initialize the transparency cache in mapgen anyway.
        }
        void set_floor_cache_dirty( const int /*z*/ ) override {
            // Ignored for now. We don't initialize the floor cache in mapgen anyway.
        }
        void removed( vehicle &veh, const int /*part*/ ) override {
            // TODO: check if this is necessary, it probably isn't during mapgen
            m.dirty_vehicle_list.insert( &veh );
        }
        void spawn_animal_from_part( item &/*base*/, const tripoint &/*loc*/ ) override {
            debugmsg( "Tried to spawn animal from vehicle part during mapgen!" );
            // Ignored. The base item will not be changed and will spawn as is:
            // still containing the animal.
            // This should not happend during mapgen anyway.
            // TODO: *if* this actually happens: create a spawn point for the animal instead.
        }
};

// Vehicle stack methods.
vehicle_stack::iterator vehicle_stack::erase( vehicle_stack::const_iterator it,
        detached_ptr<item> *out )
{
    return myorigin->remove_item( part_num, std::move( it ), out );
}

void vehicle_stack::insert( detached_ptr<item> &&newitem )
{
    myorigin->add_item( part_num, std::move( newitem ) );
}

detached_ptr<item> vehicle_stack::remove( item *to_remove )
{
    return myorigin->remove_item( part_num, to_remove );
}

units::volume vehicle_stack::max_volume() const
{
    if( myorigin->part_flag( part_num, "CARGO" ) && !myorigin->part( part_num ).is_broken() ) {
        // Set max volume for vehicle cargo to prevent integer overflow
        return std::min( myorigin->part( part_num ).info().size, 10000_liter );
    }
    return 0_ml;
}

// Vehicle class methods.

void vehicle::copy_static_from( const vehicle &source )
{
    //next_hack_id isn't copied
    //parts isn't copied;
    collision_check_points = source.collision_check_points;
    owner = source.owner;
    old_owner = source.old_owner;
    coefficient_air_resistance = source.coefficient_air_resistance;
    coefficient_rolling_resistance = source.coefficient_rolling_resistance;
    coefficient_water_resistance = source.coefficient_water_resistance;
    draft_m = source.draft_m;
    hull_height = source.hull_height;
    hull_area = source.hull_area;
    occupied_points = source.occupied_points;
    alternators = source.alternators;
    engines = source.engines;
    reactors = source.reactors;
    solar_panels = source.solar_panels;
    wind_turbines = source.wind_turbines;
    water_wheels = source.water_wheels;
    sails = source.sails;
    funnels = source.funnels;
    emitters = source.emitters;
    loose_parts = source.loose_parts;
    wheelcache = source.wheelcache;
    rotors = source.rotors;
    propellers = source.propellers;
    wings = source.wings;
    balloons = source.balloons;
    droppers = source.droppers;
    rail_wheelcache = source.rail_wheelcache;
    steering = source.steering;
    speciality = source.speciality;
    floating = source.floating;
    rail_profile = source.rail_profile;
    name = source.name;
    type = source.type;
    relative_parts = source.relative_parts;
    labels = source.labels;
    tags = source.tags;
    fuel_remainder = source.fuel_remainder;
    fuel_used_last_turn = source.fuel_used_last_turn;
    loot_zones = source.loot_zones;
    active_items = source.active_items;
    magic = source.magic;
    summon_time_limit = source.summon_time_limit;
    mass_cache = source.mass_cache;
    pivot_cache = source.pivot_cache;
    mount_max = source.mount_max;
    mount_min = source.mount_min;
    mass_center_precalc = source.mass_center_precalc;
    mass_center_no_precalc = source.mass_center_no_precalc;
    autodrive_local_target = source.autodrive_local_target;
    active_autodrive_controller = source.active_autodrive_controller;
    removed_part_count = source.removed_part_count;
    sm_pos = source.sm_pos;
    alternator_load = source.alternator_load;
    occupied_cache_time = source.occupied_cache_time;
    last_update = source.last_update;
    pos = source.pos;
    velocity = source.velocity;
    cruise_velocity = source.cruise_velocity;
    vertical_velocity = source.vertical_velocity;
    om_id = source.om_id;
    turn_dir = source.turn_dir;
    last_turn = source.last_turn;
    of_turn = source.of_turn;
    of_turn_carry = source.of_turn_carry;
    extra_drag = source.extra_drag;
    last_fluid_check = source.last_fluid_check;
    theft_time = source.theft_time;
    pivot_rotation = source.pivot_rotation;
    front_left = source.front_left;
    front_right = source.front_right;
    tow_data = source.tow_data;
    pivot_anchor = source.pivot_anchor;
    face = source.face;
    move = source.move;
    no_refresh = source.no_refresh;
    pivot_dirty = source.pivot_dirty;
    mass_dirty = source.mass_dirty;
    mass_center_precalc_dirty = source.mass_center_precalc_dirty;
    mass_center_no_precalc_dirty = source.mass_center_no_precalc_dirty;
    coeff_rolling_dirty = source.coeff_rolling_dirty;
    coeff_air_dirty = source.coeff_air_dirty;
    coeff_water_dirty = source.coeff_water_dirty;
    coeff_air_changed = source.coeff_air_changed;
    is_floating = source.is_floating;
    in_water = source.in_water;
    is_flying = source.is_flying;
    requested_z_change = source.requested_z_change;
    attached = source.attached;
    is_autodriving = source.is_autodriving;
    is_following = source.is_following;
    is_patrolling = source.is_patrolling;
    cruise_on = source.cruise_on;
    engine_on = source.engine_on;
    tracking_on = source.tracking_on;
    is_locked = source.is_locked;
    is_alarm_on = source.is_alarm_on;
    camera_on = source.camera_on;
    autopilot_on = source.autopilot_on;
    skidding = source.skidding;
    check_environmental_effects = source.check_environmental_effects;
    insides_dirty = source.insides_dirty;
    is_falling = source.is_falling;
    zones_dirty = source.zones_dirty;
    vehicle_noise = source.vehicle_noise;
}

vehicle::vehicle(
    const vproto_id &type_id, int init_veh_fuel, int init_veh_status, std::optional<bool> locked,
    std::optional<bool> has_keys )
    : type( type_id )
{
    turn_dir = 0_degrees;
    face.init( 0_degrees );
    move.init( 0_degrees );
    of_turn_carry = 0;

    if( !type.str().empty() && type.is_valid() ) {
        const vehicle_prototype &proto = type.obj();
        // Copy the already made vehicle. The blueprint is created when the json data is loaded
        // and is guaranteed to be valid (has valid parts etc.).
        copy_static_from( *proto.blueprint );
        for( vehicle_part &part : proto.blueprint->parts ) {
            parts.emplace_back( part, this );
        }
        refresh_locations_hack();
        init_state( init_veh_fuel, init_veh_status, locked, has_keys );
    }
    precalc_mounts( 0, pivot_rotation[0], pivot_anchor[0] );
    refresh();
}

vehicle::vehicle() : vehicle( vproto_id() )
{
    sm_pos = tripoint_zero;
}

vehicle::~vehicle() = default;

bool vehicle::player_in_control( const Character &who ) const
{
    // Debug switch to prevent vehicles from skidding
    // without having to place the player in them.
    if( tags.contains( "IN_CONTROL_OVERRIDE" ) ) {
        return true;
    }

    const optional_vpart_position vp = g->m.veh_at( who.pos() );
    if( vp && &vp->vehicle() == this &&
        ( ( part_with_feature( vp->part_index(), "CONTROL_ANIMAL", true ) >= 0 &&
            has_engine_type( fuel_type_animal, false ) && has_harnessed_animal() ) ||
          ( part_with_feature( vp->part_index(), VPFLAG_CONTROLS, false ) >= 0 ) ) &&
        who.controlling_vehicle ) {
        return true;
    }

    return remote_controlled( who );
}

bool vehicle::remote_controlled( const Character &who ) const
{
    vehicle *veh = g->remoteveh();
    if( veh != this ) {
        return false;
    }

    for( const vpart_reference &vp : get_avail_parts( "REMOTE_CONTROLS" ) ) {
        if( rl_dist( who.pos(), vp.pos() ) <= 40 ) {
            return true;
        }
    }

    add_msg( m_bad, _( "Lost connection with the vehicle due to distance!" ) );
    g->setremoteveh( nullptr );
    return false;
}

/** Checks all parts to see if frames are missing (as they might be when
 * loading from a game saved before the vehicle construction rules overhaul). */
void vehicle::add_missing_frames()
{
    static const vpart_id frame_id( "frame_vertical" );
    //No need to check the same spot more than once
    std::set<point> locations_checked;
    for( auto &i : parts ) {
        if( locations_checked.contains( i.mount ) ) {
            continue;
        }
        locations_checked.insert( i.mount );

        bool found = false;
        for( auto &elem : parts_at_relative( i.mount, false ) ) {
            if( part_info( elem ).location == part_location_structure ) {
                found = true;
                break;
            }
        }
        if( !found ) {
            // Install missing frame
            parts.emplace_back( frame_id, i.mount, item::spawn( frame_id->item ), this );
            refresh_locations_hack();
        }
    }
}

// Called when loading a vehicle that predates steerable wheels.
// Tries to convert some wheels to steerable versions on the front axle.
void vehicle::add_steerable_wheels()
{
    int axle = INT_MIN;
    std::vector< std::pair<int, vpart_id> > wheels;

    // Find wheels that have steerable versions.
    // Convert the wheel(s) with the largest x value.
    for( const vpart_reference &vp : get_all_parts() ) {
        if( vp.has_feature( "STEERABLE" ) || vp.has_feature( "TRACKED" ) ) {
            // Has a wheel that is inherently steerable
            // (e.g. unicycle, casters), this vehicle doesn't
            // need conversion.
            return;
        }

        if( vp.mount().x < axle ) {
            // there is another axle in front of this
            continue;
        }

        if( vp.has_feature( VPFLAG_WHEEL ) ) {
            vpart_id steerable_id( vp.info().get_id().str() + "_steerable" );
            if( steerable_id.is_valid() ) {
                // We can convert this.
                if( vp.mount().x != axle ) {
                    // Found a new axle further forward than the
                    // existing one.
                    wheels.clear();
                    axle = vp.mount().x;
                }

                wheels.emplace_back( static_cast<int>( vp.part_index() ), steerable_id );
            }
        }
    }

    // Now convert the wheels to their new types.
    for( auto &wheel : wheels ) {
        parts[ wheel.first ].id = wheel.second;
    }
}

void vehicle::init_state( const int init_veh_fuel, const int init_veh_status,
                          const std::optional<bool> locked,
                          const std::optional<bool> has_keys )
{
    // vehicle parts excluding engines are by default turned off
    for( auto &pt : parts ) {
        pt.enabled = pt.base->is_engine();
    }

    bool destroySeats = false;
    bool destroyControls = false;
    bool destroyTank = false;
    bool destroyEngine = false;
    bool destroyTires = false;
    bool blood_covered = false;
    bool blood_inside = false;
    bool lockDoors = false;
    bool needsHotwire = false;
    bool destroyAlarm = false;

    remove_old_owner();
    remove_owner();

    // More realistically it should be -5 days old
    last_update = calendar::start_of_cataclysm;

    // veh_fuel_multiplier is percentage of fuel
    // 0 is empty, 100 is full tank, -1 is random 7% to 35%
    int veh_fuel_mult = init_veh_fuel;
    if( init_veh_fuel == - 1 ) {
        veh_fuel_mult = rng( 1, 7 );
    }
    if( init_veh_fuel > 100 ) {
        veh_fuel_mult = 100;
    }

    // veh_status is initial vehicle damage
    // -1 = light damage (DEFAULT)
    //  0 = undamaged
    //  1 = disabled, destroyed tires OR engine
    const int veh_status = init_veh_status;
    if( init_veh_status == 0 ) {
        // vehicle locked 100%
        lockDoors = needsHotwire = true;
    } else if( init_veh_status == -1 ) {
        // vehicle locked 67%
        lockDoors = needsHotwire = rng( 1, 100 ) <= 67;

        // if locked, 16% chance something damaged
        if( one_in( 6 ) && ( needsHotwire || lockDoors ) ) {
            if( one_in( 3 ) ) {
                destroyTank = true;
            } else if( one_in( 2 ) ) {
                destroyEngine = true;
            } else {
                destroyTires = true;
            }
        }
    } else if( init_veh_status == 1 ) {
        //  seats are destroyed 5%
        destroySeats = rng( 1, 100 ) <= 5;
        // controls are destroyed 10%
        destroyControls = rng( 1, 100 ) <= 10;
        // battery, minireactor or gasoline tank are destroyed 8%
        destroyTank = rng( 1, 100 ) <= 8;
        // engine are destroyed 6%
        destroyEngine = rng( 1, 100 ) <= 6;
        // tires are destroyed 37%
        destroyTires = rng( 1, 100 ) <= 37;
        // locked 34%
        lockDoors = needsHotwire = rng( 1, 100 ) <= 34;

        if( destroyEngine ) {
            veh_fuel_mult += rng( 3, 12 );  // add 3-12% more fuel if engine is destroyed
        }
        if( destroyControls ) {
            veh_fuel_mult += rng( 0, 7 );   // add 0-7% more fuel if controls are destroyed
        }
        if( destroyTires ) {
            veh_fuel_mult += rng( 0, 18 );  // add 0-18% more fuel if tires are destroyed
        }
    }

    //most cars should have a destroyed alarm
    if( !one_in( 3 ) ) {
        destroyAlarm = true;
    }

    // Check Prototype Flags
    if( get_avail_parts( VPFLAG_CONTROLS ).part_count() > 0 ) {
        const auto &proto_flags = type.obj().flags;
        if( has_keys.has_value() ) {
            needsHotwire = !has_keys.value();
        } else if( proto_flags.contains( f_VEHICLE_HOTWIRE ) ) {
            needsHotwire = true;
        } else if( proto_flags.contains( f_VEHICLE_NO_HOTWIRE ) ) {
            needsHotwire = false;
        } else  {
            // No horse-wiring
            for( const auto &vp : get_all_parts() ) {
                if( std::ranges::any_of( vs_NO_HOTWIRING, [&]( const std::string & flag ) { return vp.has_feature( flag );} ) ) {
                    needsHotwire = false;
                    break;
                }
            }
        }
    } else {
        needsHotwire = false;
    }

    is_locked = needsHotwire;

    const auto &proto_flags = type.obj().flags;
    if( locked.has_value() ) {
        lockDoors = locked.value();
    } else if( proto_flags.contains( f_VEHICLE_UNLOCKED ) ) {
        lockDoors = false;
    } else if( proto_flags.contains( f_VEHICLE_LOCKED ) ) {
        lockDoors = true;
    } else {
        lockDoors = lockDoors && get_option<bool>( "VEHICLE_LOCKS" );
    }

    //Provide some variety to non-mint vehicles
    if( veh_status != 0 ) {
        //Leave engine running in some vehicles, if the engine has not been destroyed
        //chance decays from 1 in 4 vehicles on day 0 to 1 in (day + 4) in the future.
        int current_day = std::max( to_days<int>( calendar::turn - calendar::turn_zero ), 0 );
        if( veh_fuel_mult > 0 && !empty( get_avail_parts( "ENGINE" ) ) &&
            one_in( current_day + 4 ) && !destroyEngine && !needsHotwire &&
            has_engine_type_not( fuel_type_muscle, true ) ) {
            engine_on = true;
        }

        auto light_head  = one_in( 20 );
        auto light_whead  = one_in( 20 ); // wide-angle headlight
        auto light_dome  = one_in( 16 );
        auto light_aisle = one_in( 8 );
        auto light_hoverh = one_in( 4 ); // half circle overhead light
        auto light_overh = one_in( 4 );
        auto light_atom  = one_in( 2 );
        for( auto &pt : parts ) {
            if( pt.has_flag( VPFLAG_CONE_LIGHT ) ) {
                pt.enabled = light_head;
            } else if( pt.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ) {
                pt.enabled = light_whead;
            } else if( pt.has_flag( VPFLAG_DOME_LIGHT ) ) {
                pt.enabled = light_dome;
            } else if( pt.has_flag( VPFLAG_AISLE_LIGHT ) ) {
                pt.enabled = light_aisle;
            } else if( pt.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) ) {
                pt.enabled = light_hoverh;
            } else if( pt.has_flag( VPFLAG_CIRCLE_LIGHT ) ) {
                pt.enabled = light_overh;
            } else if( pt.has_flag( VPFLAG_ATOMIC_LIGHT ) ) {
                pt.enabled = light_atom;
            }
        }

        if( one_in( 10 ) ) {
            blood_covered = true;
        }

        if( one_in( 8 ) ) {
            blood_inside = true;
        }

        for( const vpart_reference &vp : get_parts_including_carried( "FRIDGE" ) ) {
            if( one_in( 2 ) ) {
                vp.part().enabled = true;
            }
        }

        for( const vpart_reference &vp : get_parts_including_carried( "FREEZER" ) ) {
            if( one_in( 2 ) ) {
                vp.part().enabled = true;
            }
        }

        for( const vpart_reference &vp : get_parts_including_carried( "WATER_PURIFIER" ) ) {
            vp.part().enabled = true;
        }
    }

    // Install Locks
    if( !proto_flags.contains( f_VEHICLE_NO_LOCKS ) ) {
        std::set<point> doors;
        for( const vpart_reference &vp : get_all_parts() ) {
            if( vp.has_feature( "OPENABLE" ) && vp.has_feature( "BOARDABLE" ) &&
                !vp.has_feature( "CURTAIN" ) ) {
                doors.emplace( vp.mount() );
            }
        }

        for( const auto &door : doors ) {
            const auto idx = install_part( door, vp_door_lock );
            if( idx >= 0 ) {
                // Newly installed part
                parts[idx].enabled = lockDoors;
            } else {
                // Already installed from blueprint
                const auto lock = part_with_feature( door, str_DOOR_LOCKING, true );
                if( idx >= 0 ) {
                    parts[lock].enabled = lockDoors;
                } else {
                    // Already installed from blueprint
                    debugmsg( "Failed to install door locks on vehicle" );
                }
            }
        }
    }

    std::optional<point> blood_inside_pos;
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        vehicle_part &pt = vp.part();

        if( vp.has_feature( VPFLAG_REACTOR ) && one_in( 4 ) ) {
            // De-hardcoded reactors, may or may not start active
            pt.enabled = true;
        }

        if( pt.is_reactor() ) {
            if( veh_fuel_mult == 100 ) { // Mint condition vehicle
                pt.ammo_set( itype_plut_cell, pt.ammo_capacity() );
            } else if( one_in( 2 ) && veh_fuel_mult > 0 ) { // Randomize charge a bit
                pt.ammo_set( itype_plut_cell, pt.ammo_capacity() * ( veh_fuel_mult + rng( 0, 10 ) ) / 100 );
            } else if( one_in( 2 ) && veh_fuel_mult > 0 ) {
                pt.ammo_set( itype_plut_cell, pt.ammo_capacity() * ( veh_fuel_mult - rng( 0, 10 ) ) / 100 );
            } else {
                pt.ammo_set( itype_plut_cell, pt.ammo_capacity() * veh_fuel_mult / 100 );
            }
        }

        if( pt.is_battery() ) {
            if( veh_fuel_mult == 100 ) { // Mint condition vehicle
                pt.ammo_set( itype_battery, pt.ammo_capacity() );
            } else if( one_in( 2 ) && veh_fuel_mult > 0 ) { // Randomize battery ammo a bit
                pt.ammo_set( itype_battery, pt.ammo_capacity() * ( veh_fuel_mult + rng( 0, 10 ) ) / 100 );
            } else if( one_in( 2 ) && veh_fuel_mult > 0 ) {
                pt.ammo_set( itype_battery, pt.ammo_capacity() * ( veh_fuel_mult - rng( 0, 10 ) ) / 100 );
            } else {
                pt.ammo_set( itype_battery, pt.ammo_capacity() * veh_fuel_mult / 100 );
            }
        }

        if( pt.is_tank() && !type->parts[p].fuel.is_null() ) {
            int qty = pt.ammo_capacity() * veh_fuel_mult / 100;
            qty *= std::max( type->parts[p].fuel->stack_size, 1 );
            qty /= to_milliliter( units::legacy_volume_factor );
            pt.ammo_set( type->parts[ p ].fuel, qty );
        } else if( pt.is_fuel_store() && !type->parts[p].fuel.is_null() ) {
            int qty = pt.ammo_capacity() * veh_fuel_mult / 100;
            pt.ammo_set( type->parts[ p ].fuel, qty );
        }

        if( vp.has_feature( "OPENABLE" ) ) { // doors are closed
            if( !pt.open && one_in( 4 ) ) {
                open( p );
                const auto lock = vp.part_with_feature( str_DOOR_LOCKING, true );
                if( lock ) {
                    lock->part().enabled = false;
                }
            }
        }
        if( vp.has_feature( "BOARDABLE" ) ) {   // no passengers
            pt.remove_flag( vehicle_part::passenger_flag );
        }

        // initial vehicle damage
        if( veh_status == 0 ) {
            // Completely mint condition vehicle
            set_hp( pt, vp.info().durability );
        } else {
            //a bit of initial damage :)
            //clamp 4d8 to the range of [8,20]. 8=broken, 20=undamaged.
            const float chance =  get_option<float>( "VEHICLE_DAMAGE" ) ;
            int broken = 8 * chance;
            int unhurt = 20 * chance;
            int roll = dice( 4, 8 );
            if( roll < unhurt ) {
                if( roll <= broken ) {
                    set_hp( pt, 0 );
                    pt.ammo_unset(); //empty broken batteries and fuel tanks
                } else {
                    set_hp( pt, ( roll - broken ) / static_cast<double>( unhurt - broken ) * vp.info().durability );
                }
            } else {
                set_hp( pt, vp.info().durability );
            }

            if( vp.has_feature( VPFLAG_ENGINE ) ) {
                // If possible set an engine fault rather than destroying the engine outright
                if( destroyEngine && pt.faults_potential().empty() ) {
                    set_hp( pt, 0 );
                } else if( destroyEngine || one_in( 3 ) ) {
                    do {
                        pt.fault_set( random_entry( pt.faults_potential() ) );
                    } while( one_in( 3 ) );
                }

            } else if( ( destroySeats && ( vp.has_feature( "SEAT" ) || vp.has_feature( "SEATBELT" ) ) ) ||
                       ( destroyControls && ( vp.has_feature( "CONTROLS" ) || vp.has_feature( "SECURITY" ) ) ) ||
                       ( destroyAlarm && vp.has_feature( "SECURITY" ) ) ) {
                set_hp( pt, 0 );
            }

            // Fuel tanks should be emptied as well
            if( destroyTank && pt.is_fuel_store() ) {
                set_hp( pt, 0 );
                pt.ammo_unset();
            }

            //Solar panels have 25% of being destroyed
            if( vp.has_feature( "SOLAR_PANEL" ) && one_in( 4 ) ) {
                set_hp( pt, 0 );
            }

            // An added 5% chance to bust each windshield
            if( vp.has_feature( "WINDSHIELD" ) && one_in( 20 ) ) {
                set_hp( pt, 0 );
            }

            /* Bloodsplatter the front-end parts. Assume anything with x > 0 is
            * the "front" of the vehicle (since the driver's seat is at (0, 0).
            * We'll be generous with the blood, since some may disappear before
            * the player gets a chance to see the vehicle. */
            if( blood_covered && vp.mount().x > 0 ) {
                if( one_in( 3 ) ) {
                    //Loads of blood. (200 = completely red vehicle part)
                    pt.blood = rng( 200, 600 );
                } else {
                    //Some blood
                    pt.blood = rng( 50, 200 );
                }
            }

            if( blood_inside ) {
                // blood is splattered around (blood_inside_pos),
                // coordinates relative to mount point; the center is always a seat
                if( blood_inside_pos ) {
                    const int distSq = std::pow( blood_inside_pos->x - vp.mount().x, 2 ) +
                                       std::pow( blood_inside_pos->y - vp.mount().y, 2 );
                    if( distSq <= 1 ) {
                        pt.blood = rng( 200, 400 ) - distSq * 100;
                    }
                } else if( vp.has_feature( "SEAT" ) ) {
                    // Set the center of the bloody mess inside
                    blood_inside_pos.emplace( vp.mount() );
                }
            }

            // Potentially bust a single tire if not already wrecking them
            if( !destroyTires && !wheelcache.empty() && one_in( 20 ) ) {
                set_hp( parts[random_entry( wheelcache )], 0 );
            }
        }
        // if there is no key and an alarm part exists
        // and vehicle has immobilizer 50% chance to add additional fault
        if( vp.has_feature( "SECURITY" ) && needsHotwire && pt.is_available() && one_in( 2 ) ) {
            pt.fault_set( fault_immobiliser );
        }
    }

    // destroy a random number of tires, vehicles with more wheels are more likely to survive
    if( destroyTires && !wheelcache.empty() ) {
        int tries = 0;
        int maxtries = wheelcache.size();
        while( valid_wheel_config() && tries < maxtries ) {
            // keep going until either we've ruined all wheels or made one attempt for every wheel
            set_hp( parts[random_entry( wheelcache )], 0 );
            tries++;
        }
    }

    invalidate_mass();
}

void vehicle::activate_magical_follow()
{
    for( vehicle_part &vp : parts ) {
        if( vp.info().fuel_type == fuel_type_mana ) {
            vp.enabled = true;
            is_following = true;
            engine_on = true;
        } else {
            vp.enabled = true;
        }
    }
    refresh();
}

void vehicle::activate_animal_follow()
{
    for( size_t e = 0; e < parts.size(); e++ ) {
        vehicle_part &vp = parts[ e ];
        if( vp.info().fuel_type == fuel_type_animal ) {
            monster *mon = get_pet( e );
            if( mon && mon->has_effect( effect_harnessed ) ) {
                vp.enabled = true;
                is_following = true;
                engine_on = true;
            }
        } else {
            vp.enabled = true;
        }
    }
    refresh();
}

void vehicle::autopilot_patrol()
{
    /** choose one single zone ( multiple zones too complex for now )
     * choose a point at the far edge of the zone
     * the edge chosen is the edge that is smaller, therefore the longer side
     * of the rectangle is the one the vehicle drives mostly parallel too.
     * if its  perfect square then choose a point that is on any edge that the
     * vehicle is not currently at
     * drive to that point.
     * then once arrived, choose a random opposite point of the zone.
     * this should ( in a simple fashion ) cause a patrolling behavior
     * in a criss-cross fashion.
     * in an auto-tractor, this would eventually cover the entire rectangle.
     */
    // if we are close to a waypoint, then return to come back to this function next turn.
    if( autodrive_local_target != tripoint_zero ) {
        if( rl_dist( global_square_location().raw(), autodrive_local_target ) <= 3 ) {
            autodrive_local_target = tripoint_zero;
            return;
        }
        if( !g->m.inbounds( g->m.getlocal( autodrive_local_target ) ) ) {
            autodrive_local_target = tripoint_zero;
            is_patrolling = false;
            return;
        }
        drive_to_local_target( autodrive_local_target, false );
        return;
    }
    zone_manager &mgr = zone_manager::get_manager();
    const auto &zone_src_set = mgr.get_near( zone_type_id( "VEHICLE_PATROL" ),
                               global_square_location().raw(), 60 );
    if( zone_src_set.empty() ) {
        is_patrolling = false;
        return;
    }
    // get corners.
    tripoint min;
    tripoint max;
    for( const tripoint &box : zone_src_set ) {
        if( min == tripoint_zero ) {
            min = box;
            max = box;
            continue;
        }
        min.x = std::min( box.x, min.x );
        min.y = std::min( box.y, min.y );
        min.z = std::min( box.z, min.z );
        max.x = std::max( box.x, max.x );
        max.y = std::max( box.y, max.y );
        max.z = std::max( box.z, max.z );
    }
    const bool x_side = ( max.x - min.x ) < ( max.y - min.y );
    const int point_along = x_side ? rng( min.x, max.x ) : rng( min.y, max.y );
    const tripoint max_tri = x_side ? tripoint( point_along, max.y, min.z ) : tripoint( max.x,
                             point_along,
                             min.z );
    const tripoint min_tri = x_side ? tripoint( point_along, min.y, min.z ) : tripoint( min.x,
                             point_along,
                             min.z );
    tripoint chosen_tri = min_tri;
    if( rl_dist( max_tri, global_square_location().raw() ) >= rl_dist( min_tri,
            global_square_location().raw() ) ) {
        chosen_tri = max_tri;
    }
    autodrive_local_target = chosen_tri;
    drive_to_local_target( autodrive_local_target, false );
}

std::set<point> vehicle::immediate_path( units::angle rotate )
{
    std::set<point> points_to_check;
    const int distance_to_check = 10 + ( velocity / 800 );
    units::angle adjusted_angle = normalize( face.dir() + rotate );
    // clamp to multiples of 15.
    adjusted_angle = round_to_multiple_of( adjusted_angle, 15_degrees );
    tileray collision_vector;
    collision_vector.init( adjusted_angle );
    point top_left_actual = global_pos3().xy() + coord_translate( front_left );
    point top_right_actual = global_pos3().xy() + coord_translate( front_right );
    std::vector<point> front_row = line_to( g->m.getabs( top_left_actual ),
                                            g->m.getabs( top_right_actual ) );
    for( point elem : front_row ) {
        for( int i = 0; i < distance_to_check; ++i ) {
            collision_vector.advance( i );
            point point_to_add = elem + point( collision_vector.dx(), collision_vector.dy() );
            points_to_check.emplace( point_to_add );
        }
    }
    collision_check_points = points_to_check;
    return points_to_check;
}

static int get_turn_from_angle( const units::angle angle, const tripoint &vehpos,
                                const tripoint &target, bool reverse = false )
{
    if( angle > 10.0_degrees && angle <= 45.0_degrees ) {
        return reverse ? 4 : 1;
    } else if( angle > 45.0_degrees && angle <= 90.0_degrees ) {
        return 3;
    } else if( angle > 90.0_degrees && angle < 180.0_degrees ) {
        return reverse ? 1 : 4;
    } else if( angle < -10.0_degrees && angle >= -45.0_degrees ) {
        return reverse ? -4 : -1;
    } else if( angle < -45.0_degrees && angle >= -90.0_degrees ) {
        return -3;
    } else if( angle < -90.0_degrees && angle > -180.0_degrees ) {
        return reverse ? -1 : -4;
        // edge case of being exactly on the button for the target.
        // just keep driving, the next path point will be picked up.
    } else if( ( angle == 180_degrees || angle == -180_degrees ) && vehpos == target ) {
        return 0;
    }
    return 0;
}

void vehicle::drive_to_local_target( const tripoint &target, bool follow_protocol )
{
    if( follow_protocol && g->u.in_vehicle ) {
        stop_autodriving();
        return;
    }
    refresh();
    tripoint vehpos = global_square_location().raw();
    units::angle angle = get_angle_from_targ( target );
    // now we got the angle to the target, we can work out when we are heading towards disaster.
    // Check the tileray in the direction we need to head towards.
    std::set<point> points_to_check = immediate_path( angle );
    bool stop = false;
    for( point pt_elem : points_to_check ) {
        point elem = g->m.getlocal( pt_elem );
        if( stop ) {
            break;
        }
        const optional_vpart_position ovp = g->m.veh_at( tripoint( elem, sm_pos.z ) );
        if( g->m.impassable_ter_furn( tripoint( elem, sm_pos.z ) ) || ( ovp &&
                &ovp->vehicle() != this ) ) {
            stop = true;
            break;
        }
        if( elem == g->u.pos().xy() ) {
            if( follow_protocol || g->u.in_vehicle ) {
                continue;
            } else {
                stop = true;
                break;
            }
        }
        bool its_a_pet = false;
        if( g->critter_at( tripoint( elem, sm_pos.z ) ) ) {
            npc *guy = g->critter_at<npc>( tripoint( elem, sm_pos.z ) );
            if( guy && !guy->in_vehicle ) {
                stop = true;
                break;
            }
            for( const auto &p : parts ) {
                monster *mon = get_pet( index_of_part( &p ) );
                if( mon && mon->pos().xy() == elem ) {
                    its_a_pet = true;
                    break;
                }
            }
            if( !its_a_pet ) {
                stop = true;
                break;
            }
        }
    }
    if( stop ) {
        if( autopilot_on ) {
            sounds::sound( global_pos3(), 30, sounds::sound_t::alert,
                           string_format( _( "the %s emitting a beep and saying \"Obstacle detected!\"" ),
                                          name ) );
        }
        stop_autodriving();
        return;
    }
    int turn_x = get_turn_from_angle( angle, vehpos, target );
    int accel_y = 0;
    // best to cruise around at a safe velocity or 40mph, whichever is lowest
    // accelerate when it dosnt need to turn.
    // when following player, take distance to player into account.
    // we really want to avoid running the player over.
    // If its a helicopter, we dont need to worry about airborne obstacles so much
    // And fuel efficiency is terrible at low speeds.
    int safe_player_follow_speed = 400;
    if( g->u.movement_mode_is( CMM_RUN ) ) {
        safe_player_follow_speed = 800;
    } else if( g->u.movement_mode_is( CMM_CROUCH ) ) {
        safe_player_follow_speed = 200;
    }
    if( follow_protocol ) {
        if( ( ( turn_x > 0 || turn_x < 0 ) && velocity > safe_player_follow_speed ) ||
            rl_dist( vehpos, g->m.getabs( g->u.pos() ) ) < 7 + ( ( mount_max.y * 3 ) + 4 ) ) {
            accel_y = 1;
        }
        if( ( velocity < std::min( safe_velocity(), safe_player_follow_speed ) && turn_x == 0 &&
              rl_dist( vehpos, g->m.getabs( g->u.pos() ) ) > 8 + ( ( mount_max.y * 3 ) + 4 ) ) ||
            velocity < 100 ) {
            accel_y = -1;
        }
    } else {
        if( ( turn_x > 0 || turn_x < 0 ) && velocity > 1000 ) {
            accel_y = 1;
        }
        if( ( velocity < std::min( safe_velocity(), is_aircraft() &&
                                   is_flying_in_air() ? 12000 : 32 * 100 ) && turn_x == 0 ) || velocity < 500 ) {
            accel_y = -1;
        }
        if( is_patrolling && velocity > 400 ) {
            accel_y = 1;
        }
    }
    selfdrive( point( turn_x, accel_y ) );
}

tripoint vehicle::get_autodrive_target()
{
    return autodrive_local_target;
}

units::angle vehicle::get_angle_from_targ( const tripoint &targ )
{
    tripoint vehpos = global_square_location().raw();
    rl_vec2d facevec = face_vec();
    point rel_pos_target = targ.xy() - vehpos.xy();
    rl_vec2d targetvec = rl_vec2d( rel_pos_target.x, rel_pos_target.y );
    // cross product
    double crossy = ( facevec.x * targetvec.y ) - ( targetvec.x * facevec.y );
    // dot product.
    double dotx = ( facevec.x * targetvec.x ) + ( facevec.y * targetvec.y );

    return units::atan2( crossy, dotx );
}

/**
 * Smashes up a vehicle that has already been placed; used for generating
 * very damaged vehicles. Additionally, any spot where two vehicles overlapped
 * (i.e., any spot with multiple frames) will be completely destroyed, as that
 * was the collision point.
 */
void vehicle::smash( map &m, float hp_percent_loss_min, float hp_percent_loss_max,
                     float percent_of_parts_to_affect, point damage_origin, float damage_size )
{
    for( auto &part : parts ) {
        //Skip any parts already mashed up or removed.
        if( part.is_broken() || part.removed ) {
            continue;
        }

        std::vector<int> parts_in_square = parts_at_relative( part.mount, true );
        int structures_found = 0;
        for( auto &square_part_index : parts_in_square ) {
            if( part_info( square_part_index ).location == part_location_structure ||
                part_info( square_part_index ).has_flag( VPFLAG_EXTENDABLE ) ) {
                structures_found++;
            }
        }

        if( structures_found > 1 ) {
            //Destroy everything in the square
            for( int idx : parts_in_square ) {
                mod_hp( parts[ idx ], 0 - parts[ idx ].hp(), DT_BASH );
                parts[ idx ].ammo_unset();
            }
            continue;
        }

        int roll = dice( 1, 1000 );
        int pct_af = ( percent_of_parts_to_affect * 1000.0f );
        if( roll < pct_af ) {
            double dist =  damage_size == 0.0f ? 1.0f :
                           clamp( 1.0f - trig_dist( damage_origin, part.precalc[0].xy() ) /
                                  damage_size, 0.0f, 1.0f );
            //Everywhere else, drop by 10-120% of max HP (anything over 100 = broken)
            if( mod_hp( part, 0 - ( rng_float( hp_percent_loss_min * dist,
                                               hp_percent_loss_max * dist ) *
                                    part.info().durability ), DT_BASH ) ) {
                part.ammo_unset();
            }
        }
    }

    std::unique_ptr<RemovePartHandler> handler_ptr;
    // clear out any duplicated locations
    for( int p = static_cast<int>( parts.size() ) - 1; p >= 0; p-- ) {
        vehicle_part &part = parts[ p ];
        if( part.removed ) {
            continue;
        }
        std::vector<int> parts_here = parts_at_relative( part.mount, true );
        for( int other_i = static_cast<int>( parts_here.size() ) - 1; other_i >= 0; other_i -- ) {
            int other_p = parts_here[ other_i ];
            if( p == other_p ) {
                continue;
            }
            const vpart_info &p_info = part_info( p );
            const vpart_info &other_p_info = part_info( other_p );

            if( p_info.get_id() == other_p_info.get_id() ||
                ( !p_info.location.empty() && p_info.location == other_p_info.location ) ) {
                // Deferred creation of the handler to here so it is only created when actually needed.
                if( !handler_ptr ) {
                    // This is a heuristic: we just assume the default handler is good enough when called
                    // on the main game map. And assume that we run from some mapgen code if called on
                    // another instance.
                    if( g && &g->m == &m ) {
                        handler_ptr = std::make_unique<DefaultRemovePartHandler>();
                    } else {
                        handler_ptr = std::make_unique<MapgenRemovePartHandler>( m );
                    }
                }
                remove_part( other_p, *handler_ptr );
            }
        }
    }
}

int vehicle::lift_strength() const
{
    units::mass mass = total_mass();
    return std::max<std::int64_t>( mass / 10000_gram, 1 );
}

void vehicle::toggle_specific_engine( int e, bool on )
{
    toggle_specific_part( engines[e], on );
}
void vehicle::toggle_specific_part( int p, bool on )
{
    parts[p].enabled = on;
}
bool vehicle::is_engine_type_on( int e, const itype_id &ft ) const
{
    return is_engine_on( e ) && is_engine_type( e, ft );
}

bool vehicle::has_engine_type( const itype_id &ft, const bool enabled ) const
{
    for( size_t e = 0; e < engines.size(); ++e ) {
        if( is_engine_type( e, ft ) && ( !enabled || is_engine_on( e ) ) ) {
            return true;
        }
    }
    return false;
}
bool vehicle::has_engine_type_not( const itype_id &ft, const bool enabled ) const
{
    for( size_t e = 0; e < engines.size(); ++e ) {
        if( !is_engine_type( e, ft ) && ( !enabled || is_engine_on( e ) ) ) {
            return true;
        }
    }
    return false;
}

bool vehicle::has_engine_conflict( const vpart_info *possible_conflict,
                                   std::string &conflict_type ) const
{
    std::vector<std::string> new_excludes = possible_conflict->engine_excludes();
    // skip expensive string comparisons if there are no exclusions
    if( new_excludes.empty() ) {
        return false;
    }

    bool has_conflict = false;

    for( int engine : engines ) {
        std::vector<std::string> install_excludes = part_info( engine ).engine_excludes();
        std::vector<std::string> conflicts;
        std::set_intersection( new_excludes.begin(), new_excludes.end(), install_excludes.begin(),
                               install_excludes.end(), back_inserter( conflicts ) );
        if( !conflicts.empty() ) {
            has_conflict = true;
            conflict_type = conflicts.front();
            break;
        }
    }
    return has_conflict;
}

bool vehicle::is_engine_type( const int e, const itype_id  &ft ) const
{
    return parts[engines[e]].ammo_current().is_null() ? parts[engines[e]].fuel_current() == ft :
           parts[engines[e]].ammo_current() == ft;
}

bool vehicle::is_perpetual_type( const int e ) const
{
    const itype_id  &ft = part_info( engines[e] ).fuel_type;
    //TODO!: push up
    return item::spawn_temporary( ft )->has_flag( flag_PERPETUAL );
}

bool vehicle::is_engine_on( const int e ) const
{
    return parts[ engines[ e ] ].is_available() && is_part_on( engines[ e ] );
}

bool vehicle::is_part_on( const int p ) const
{
    return parts[p].enabled;
}

bool vehicle::is_alternator_on( const int a ) const
{
    auto &alt = parts[ alternators [ a ] ];
    if( alt.is_unavailable() ) {
        return false;
    }

    return std::any_of( engines.begin(), engines.end(), [this, &alt]( int idx ) {
        const auto &eng = parts [ idx ];
        //fuel_left checks that the engine can produce power to be absorbed
        return eng.is_available() && eng.enabled && fuel_left( eng.fuel_current() ) &&
               eng.mount == alt.mount && !eng.faults().contains( fault_belt );
    } );
}

bool vehicle::has_security_working() const
{
    bool found_security = false;
    if( fuel_left( fuel_type_battery ) > 0 ) {
        const auto [c, s] = get_controls_and_security();
        found_security = s >= 0;
    }
    return found_security;
}

void vehicle::backfire( const int e ) const
{
    const int power = part_vpower_w( engines[e], true );
    const tripoint pos = global_part_pos3( engines[e] );
    sounds::sound( pos, 40 + power / 10000, sounds::sound_t::movement,
                   // single space after the exclaimation mark because it does not end the sentence
                   //~ backfire sound
                   string_format( _( "a loud BANG! from the %s" ), // NOLINT(cata-text-style)
                                  parts[ engines[ e ] ].name() ), true, "vehicle", "engine_backfire" );
}

const vpart_info &vehicle::part_info( int index, bool include_removed ) const
{
    if( index < static_cast<int>( parts.size() ) ) {
        if( !parts[index].removed || include_removed ) {
            return parts[index].info();
        }
    }
    return vpart_id::NULL_ID().obj();
}

// engines & alternators all have power.
// engines provide, whilst alternators consume.
int vehicle::part_vpower_w( const int index, const bool at_full_hp ) const
{
    const vehicle_part &vp = parts[ index ];
    int pwr = vp.info().power;
    if( part_flag( index, VPFLAG_ENGINE ) ) {
        if( pwr == 0 ) {
            pwr = vhp_to_watts( vp.base->engine_displacement() );
        }
        if( vp.info().fuel_type == fuel_type_animal ) {
            monster *mon = get_pet( index );
            if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
                // An animal that can carry twice as much weight, can pull a cart twice as hard.
                pwr = mon->get_speed() * mon->get_size() * 3
                      * ( mon->get_mountable_weight_ratio() * 5 );
            } else {
                pwr = 0;
            }
        }
        ///\EFFECT_STR increases power produced for MUSCLE_* vehicles
        pwr += ( g->u.str_cur - 8 ) * part_info( index ).engine_muscle_power_factor();
        /// wind-powered vehicles have differing power depending on wind direction
        if( vp.info().fuel_type == fuel_type_wind ) {
            const weather_manager &weather = get_weather();
            int windpower = weather.windspeed;
            // We're dead in the water.
            if( windpower < 1 ) {
                pwr = 0;
            } else {
                // For gameplay purposes, permit adjusting sails enough to sail upwind so long as it's blowing at all.
                rl_vec2d windvec;
                double raddir = ( ( weather.winddirection + 180 ) % 360 ) * ( M_PI / 180 );
                windvec = windvec.normalized();
                windvec.y = -std::cos( raddir );
                windvec.x = std::sin( raddir );
                rl_vec2d fv = face_vec();
                // We want 0.5 multiplier at 90 degrees instead of 0.0, so add 0.5.
                double dot = windvec.dot_product( fv ) + 0.5;
                // We don't want negatives or over 100% power, however.
                dot = std::min( 1.0, std::max( 0.0, dot ) );
                int windeffectint = static_cast<int>( ( windpower * dot ) * 200 );
                pwr = pwr + windeffectint;
            }
        }
    }

    if( pwr < 0 ) {
        return pwr; // Consumers always draw full power, even if broken
    }
    if( at_full_hp ) {
        return pwr; // Assume full hp
    }
    // Damaged engines give less power, but some engines handle it better
    double health = parts[index].health_percent();
    // dpf is 0 for engines that scale power linearly with damage and
    // provides a floor otherwise
    float dpf = part_info( index ).engine_damaged_power_factor();
    double effective_percent = dpf + ( ( 1 - dpf ) * health );
    return static_cast<int>( pwr * effective_percent );
}

// alternators, solar panels, reactors, and accessories all have epower.
// alternators, solar panels, and reactors provide, whilst accessories consume.
// for motor consumption see @ref vpart_info::energy_consumption instead
int vehicle::part_epower_w( const int index ) const
{
    int e = part_info( index ).epower;
    if( e < 0 ) {
        return e; // Consumers always draw full power, even if broken
    }
    return e * parts[ index ].health_percent();
}

int vehicle::power_to_energy_bat( const int power_w, const time_duration &d ) const
{
    // Integrate constant epower (watts) over time to get units of battery energy
    // Thousands of watts over millions of seconds can happen, so 32-bit int
    // insufficient.
    int64_t energy_j = power_w * to_seconds<int64_t>( d );
    int energy_bat = energy_j / bat_energy_j;
    int sign = power_w >= 0 ? 1 : -1;
    // energy_bat remainder results in chance at additional charge/discharge
    energy_bat += x_in_y( std::abs( energy_j % bat_energy_j ), bat_energy_j ) ? sign : 0;
    return energy_bat;
}

int vehicle::vhp_to_watts( const int power_vhp )
{
    // Convert vhp units (0.5 HP ) to watts
    // Used primarily for calculating battery charge/discharge
    // TODO: convert batteries to use energy units based on watts (watt-ticks?)
    constexpr int conversion_factor = 373; // 373 watts == 1 power_vhp == 0.5 HP
    return power_vhp * conversion_factor;
}

bool vehicle::has_structural_part( point dp ) const
{
    for( const int elem : parts_at_relative( dp, false ) ) {
        if( part_info( elem ).location == part_location_structure &&
            !part_info( elem ).has_flag( "PROTRUSION" ) ) {
            return true;
        }
    }
    return false;
}

bool vehicle::has_structural_or_extendable_part( point dp ) const
{
    for( const int elem : parts_at_relative( dp, false ) ) {
        if( ( part_info( elem ).location == part_location_structure &&
              !part_info( elem ).has_flag( "PROTRUSION" ) ) ||
            part_info( elem ).has_flag( VPFLAG_EXTENDABLE ) ) {
            return true;
        }
    }
    return false;
}

/**
 * Returns whether or not the vehicle has a structural part queued for removal,
 * @return true if a structural is queue for removal, false if not.
 * */
bool vehicle::is_structural_part_removed() const
{
    for( const vpart_reference &vp : get_all_parts() ) {
        if( vp.part().removed && vp.info().location == part_location_structure ) {
            return true;
        }
    }
    return false;
}

/**
 * Returns whether or not the vehicle part with the given id can be mounted in
 * the specified square.
 * @param dp The local coordinate to mount in.
 * @param id The id of the part to install.
 * @return true if the part can be mounted, false if not.
 */
bool vehicle::can_mount( point dp, const vpart_id &id ) const
{
    //The part has to actually exist.
    if( !id.is_valid() ) {
        return false;
    }

    //It also has to be a real part, not the null part
    const vpart_info &part = id.obj();
    if( part.has_flag( "NOINSTALL" ) ) {
        return false;
    }

    const std::vector<int> parts_in_square = parts_at_relative( dp, false );

    //New Subpath for balloon type structures when on the edge
    if( part.has_flag( "EXTENDABLE" ) ) {
        if( parts_in_square.empty() ) {
            // There needs to be parts for these
            if( !parts.empty() ) {
                if( !has_structural_or_extendable_part( dp ) &&
                    !has_structural_or_extendable_part( dp + point_east ) &&
                    !has_structural_or_extendable_part( dp + point_south ) &&
                    !has_structural_or_extendable_part( dp + point_west ) &&
                    !has_structural_or_extendable_part( dp + point_north ) ) {
                    return false;
                }
                return true;
            }
            // If there are no parts, we go on our merry day
        } else {
            return false;
        }
    }
    //First part in an empty square MUST be a structural part
    if( parts_in_square.empty() && part.location != part_location_structure ) {
        return false;
    }
    // If its a part that harnesses animals that don't allow placing on it.
    if( !parts_in_square.empty() && part_info( parts_in_square[0] ).has_flag( "ANIMAL_CTRL" ) ) {
        return false;
    }
    //No other part can be placed on a protrusion
    if( !parts_in_square.empty() && part_info( parts_in_square[0] ).has_flag( "PROTRUSION" ) ) {
        return false;
    }

    //No part type can stack with itself, or any other part in the same slot
    for( const auto &elem : parts_in_square ) {
        const vpart_info &other_part = parts[elem].info();

        //Parts with no location can stack with each other (but not themselves)
        if( part.get_id() == other_part.get_id() ||
            ( !part.location.empty() && part.location == other_part.location ) ) {
            return false;
        }
        // Until we have an interface for handling multiple components with CARGO space,
        // exclude them from being mounted in the same tile.
        if( part.has_flag( "CARGO" ) && other_part.has_flag( "CARGO" ) ) {
            return false;
        }

    }

    // All parts after the first must be installed on or next to an existing part
    // the exception is when a single tile only structural object is being repaired
    if( !parts.empty() ) {
        if( !is_structural_part_removed() &&
            !has_structural_part( dp ) &&
            !has_structural_part( dp + point_east ) &&
            !has_structural_part( dp + point_south ) &&
            !has_structural_part( dp + point_west ) &&
            !has_structural_part( dp + point_north ) ) {
            return false;
        }
    }

    // only one exclusive engine allowed
    std::string empty;
    if( has_engine_conflict( &part, empty ) ) {
        return false;
    }

    // Check all the flags of the part to see if they require other flags
    // If other flags are required check if those flags are present
    for( const std::string &flag : part.get_flags() ) {
        if( !json_flag::get( flag ).requires_flag().empty() ) {
            bool anchor_found = false;
            for( const auto &elem : parts_in_square ) {
                if( part_info( elem ).has_flag( json_flag::get( flag ).requires_flag() ) ) {
                    anchor_found = true;
                }
            }
            if( !anchor_found ) {
                return false;
            }
        }
    }

    //Mirrors cannot be mounted on OPAQUE parts
    if( part.has_flag( "VISION" ) && !part.has_flag( "CAMERA" ) ) {
        for( const auto &elem : parts_in_square ) {
            if( part_info( elem ).has_flag( "OPAQUE" ) ) {
                return false;
            }
        }
    }
    //Opaque parts cannot be mounted on mirrors parts
    if( part.has_flag( "OPAQUE" ) ) {
        for( const auto &elem : parts_in_square ) {
            if( part_info( elem ).has_flag( "VISION" ) &&
                !part_info( elem ).has_flag( "CAMERA" ) ) {
                return false;
            }
        }
    }

    //Turret mounts must NOT be installed on other (modded) turret mounts
    if( part.has_flag( "TURRET_MOUNT" ) ) {
        for( const auto &elem : parts_in_square ) {
            if( part_info( elem ).has_flag( "TURRET_MOUNT" ) ) {
                return false;
            }
        }
    }

    //Don't allow turret controls on manual-only turrets.
    if( part.has_flag( "TURRET_CONTROLS" ) ) {
        for( const auto &elem : parts_in_square ) {
            if( part_info( elem ).has_flag( "MANUAL" ) ) {
                return false;
            }
        }
    }

    //Anything not explicitly denied is permitted
    return true;
}

bool vehicle::can_unmount( const int p ) const
{
    std::string no_reason;
    return can_unmount( p, no_reason );
}

bool vehicle::can_unmount( const int p, std::string &reason ) const
{
    if( p < 0 || p > static_cast<int>( parts.size() ) ) {
        return false;
    }

    // Find all the flags on parts in this tile that require other flags
    const point pt = parts[p].mount;
    std::vector<int> parts_here = parts_at_relative( pt, false );

    if( part_info( p ).has_flag( "NOREMOVE_SECURITY" ) ) {
        const auto [c, s] = get_controls_and_security();
        if( s >= 0 ) {
            reason = string_format( _( "Remove the %1$s %2$s first." ), name, part_info( s ).name() );
            return false;
        }
    }

    const auto no_remove_closed = part_info( p ).has_flag( "NOREMOVE_CLOSED" );
    const auto no_remove_open =  part_info( p ).has_flag( "NOREMOVE_OPEN" );
    for( const auto &elem : parts_here ) {
        const auto is_openable = part_info( elem ).has_flag( "OPENABLE" );
        if( no_remove_closed && is_openable && !parts[elem].open ) {
            reason = string_format( _( "Open the attached %s first." ), part_info( elem ).name() );
            return false;
        }
        if( no_remove_open && is_openable &&  parts[elem].open ) {
            reason = string_format( _( "Close the attached %s first." ), part_info( elem ).name() );
            return false;
        }

        for( const std::string &flag : part_info( elem ).get_flags() ) {
            const auto require_flag = json_flag::get( flag ).requires_flag();
            if( !require_flag.empty() && part_info( p ).has_flag( require_flag ) ) {
                reason = string_format( _( "Remove the attached %s first." ), part_info( elem ).name() );
                return false;
            }
        }
    }

    //Can't remove an animal part if the animal is still contained
    if( parts[p].has_flag( vehicle_part::animal_flag ) ) {
        reason = _( "Remove carried animal first." );
        return false;
    }

    //Structural parts have extra requirements
    if( part_info( p ).location == part_location_structure ||
        part_info( p ).has_flag( VPFLAG_EXTENDABLE ) ) {

        std::vector<int> parts_in_square = parts_at_relative( parts[p].mount, false );
        /* To remove a structural part, there can be only structural parts left
         * in that square (might be more than one in the case of wreckage) */
        for( auto &elem : parts_in_square ) {
            if( part_info( elem ).location != part_location_structure &&
                !part_info( elem ).has_flag( VPFLAG_EXTENDABLE ) ) {
                reason = _( "Remove all other attached parts first." );
                return false;
            }
        }

        //If it's the last part in the square...
        if( parts_in_square.size() == 1 ) {

            /* This is the tricky part: We can't remove a part that would cause
             * the vehicle to 'break into two' (like removing the middle section
             * of a quad bike, for instance). This basically requires doing some
             * breadth-first searches to ensure previously connected parts are
             * still connected. */

            //First, find all the squares connected to the one we're removing
            std::vector<const vehicle_part *> connected_parts;

            for( int i = 0; i < 4; i++ ) {
                const point next = parts[p].mount + point( i < 2 ? ( i == 0 ? -1 : 1 ) : 0,
                                   i < 2 ? 0 : ( i == 2 ? -1 : 1 ) );
                std::vector<int> parts_over_there = parts_at_relative( next, false );
                //Ignore empty squares
                if( !parts_over_there.empty() ) {
                    //Just need one part from the square to track the x/y
                    connected_parts.push_back( &parts[parts_over_there[0]] );
                }
            }

            /* If size = 0, it's the last part of the whole vehicle, so we're OK
             * If size = 1, it's one protruding part (i.e., bicycle wheel), so OK
             * Otherwise, it gets complicated... */
            if( connected_parts.size() > 1 ) {

                /* We'll take connected_parts[0] to be the target part.
                 * Every other part must have some path (that doesn't involve
                 * the part about to be removed) to the target part, in order
                 * for the part to be legally removable. */
                for( const auto &next_part : connected_parts ) {
                    if( !is_connected( *connected_parts[0], *next_part, parts[p] ) ) {
                        //Removing that part would break the vehicle in two
                        reason = _( "Removing this part would split the vehicle." );
                        return false;
                    }
                }

            }

        }
    }
    //Anything not explicitly denied is permitted
    return true;
}

/**
 * Performs a breadth-first search from one part to another, to see if a path
 * exists between the two without going through the excluded part. Used to see
 * if a part can be legally removed.
 * @param to The part to reach.
 * @param from The part to start the search from.
 * @param excluded_part The part that is being removed and, therefore, should not
 *        be included in the path.
 * @return true if a path exists without the excluded part, false otherwise.
 */
bool vehicle::is_connected( const vehicle_part &to, const vehicle_part &from,
                            const vehicle_part &excluded_part ) const
{
    const auto target = to.mount;
    const auto excluded = excluded_part.mount;

    //Breadth-first-search components
    std::list<const vehicle_part *> discovered;
    std::list<const vehicle_part *> searched;

    //We begin with just the start point
    discovered.push_back( &from );

    while( !discovered.empty() ) {
        const vehicle_part &current_part = *discovered.front();
        discovered.pop_front();
        auto current = current_part.mount;

        for( point offset : four_adjacent_offsets ) {
            point next = current + offset;

            if( next == target ) {
                //Success!
                return true;
            } else if( next == excluded ) {
                //There might be a path, but we're not allowed to go that way
                continue;
            }

            std::vector<int> parts_there = parts_at_relative( next, true );

            if( !parts_there.empty() && !parts[ parts_there[ 0 ] ].removed &&
                ( part_info( parts_there[ 0 ] ).location == "structure" ||
                  part_info( parts_there[ 0 ] ).has_flag( VPFLAG_EXTENDABLE ) ) &&
                !part_info( parts_there[ 0 ] ).has_flag( "PROTRUSION" ) ) {
                //Only add the part if we haven't been here before
                bool found = false;
                for( auto &elem : discovered ) {
                    if( elem->mount == next ) {
                        found = true;
                        break;
                    }
                }
                if( !found ) {
                    for( auto &elem : searched ) {
                        if( elem->mount == next ) {
                            found = true;
                            break;
                        }
                    }
                }
                if( !found ) {
                    const vehicle_part &next_part = parts[parts_there[0]];
                    discovered.push_back( &next_part );
                }
            }
        }
        //Now that that's done, we've finished exploring here
        searched.push_back( &current_part );
    }
    //If we completely exhaust the discovered list, there's no path
    return false;
}

/**
 * Installs a part into this vehicle.
 * @param dp The coordinate of where to install the part.
 * @param id The string ID of the part to install. (see vehicle_parts.json)
 * @param force Skip check of whether we can mount the part here.
 * @return false if the part could not be installed, true otherwise.
 */
int vehicle::install_part( point dp, const vpart_id &id, bool force )
{
    if( !( force || can_mount( dp, id ) ) ) {
        return -1;
    }
    detached_ptr<item> obj = item::spawn( id.obj().item );
    int ret = install_part( dp, vehicle_part( id, dp, std::move( obj ), this ) );
    return ret;
}

int vehicle::install_part( point dp, const vpart_id &id, detached_ptr<item> &&obj, bool force )
{
    if( !( force || can_mount( dp, id ) ) ) {
        return -1;
    }

    int ret = install_part( dp, vehicle_part( id, dp, std::move( obj ), this ) );
    return ret;
}

int vehicle::install_part( point dp, vehicle_part &&new_part )
{
    // Should be checked before installing the part
    bool enable = false;
    if( new_part.is_engine() ) {
        enable = true;
    } else {
        // TODO: read toggle groups from JSON
        static const std::vector<std::string> enable_like = {{
                "CONE_LIGHT",
                "CIRCLE_LIGHT",
                "AISLE_LIGHT",
                "AUTOPILOT",
                "DOME_LIGHT",
                "ATOMIC_LIGHT",
                "STEREO",
                "CHIMES",
                "FRIDGE",
                "FREEZER",
                "RECHARGE",
                "PLOW",
                "REAPER",
                "PLANTER",
                "SCOOP",
                "SPACE_HEATER",
                "COOLER",
                "WATER_PURIFIER",
                "ROCKWHEEL",
                "ROADHEAD"
            }
        };

        for( const std::string &flag : enable_like ) {
            if( new_part.info().has_flag( flag ) ) {
                enable = has_part( flag, true );
                break;
            }
        }
    }

    parts.push_back( std::move( new_part ) );
    refresh_locations_hack();
    auto &pt = parts.back();
    pt.set_vehicle_hack( this );

    pt.enabled = enable;

    pt.mount = dp;

    refresh();
    coeff_air_changed = true;
    return parts.size() - 1;
}

bool vehicle::try_to_rack_nearby_vehicle( const std::vector<std::vector<int>> &list_of_racks )
{
    for( const auto &this_bike_rack : list_of_racks ) {
        std::vector<vehicle *> carry_vehs;
        carry_vehs.assign( 4, nullptr );
        vehicle *test_veh = nullptr;
        std::set<tripoint> veh_partial_match;
        std::vector<std::set<tripoint>> partial_matches;
        partial_matches.assign( 4, veh_partial_match );
        for( auto rack_part : this_bike_rack ) {
            tripoint rack_pos = global_part_pos3( rack_part );
            int i = 0;
            for( point offset : four_cardinal_directions ) {
                tripoint search_pos( rack_pos + offset );
                test_veh = veh_pointer_or_null( g->m.veh_at( search_pos ) );
                if( test_veh == nullptr || test_veh == this ) {
                    continue;
                } else if( test_veh != carry_vehs[ i ] ) {
                    carry_vehs[ i ] = test_veh;
                    partial_matches[ i ].clear();
                }
                partial_matches[ i ].insert( search_pos );
                if( partial_matches[ i ] == test_veh->get_points() ) {
                    return merge_rackable_vehicle( test_veh, this_bike_rack );
                }
                ++i;
            }
        }
    }
    return false;
}

bool vehicle::merge_rackable_vehicle( vehicle *carry_veh, const std::vector<int> &rack_parts )
{
    // Mapping between the old vehicle and new vehicle mounting points
    struct mapping {
        // All the parts attached to this mounting point
        std::vector<int> carry_parts_here;

        // the index where the racking part is on the vehicle with the rack
        int rack_part;

        // the mount point we are going to add to the vehicle with the rack
        point carry_mount;

        // the mount point on the old vehicle (carry_veh) that will be destroyed
        point old_mount;
    };
    invalidate_towing( true );
    // By structs, we mean all the parts of the carry vehicle that are at the structure location
    // of the vehicle (i.e. frames)
    std::vector<int> carry_veh_structs = carry_veh->all_parts_at_location( part_location_structure );
    std::vector<mapping> carry_data;
    carry_data.reserve( carry_veh_structs.size() );

    //X is forward/backward, Y is left/right
    std::string axis;
    if( carry_veh_structs.size() == 1 ) {
        axis = "X";
    } else {
        for( auto carry_part : carry_veh_structs ) {
            if( carry_veh->parts[ carry_part ].mount.x || carry_veh->parts[ carry_part ].mount.y ) {
                axis = carry_veh->parts[ carry_part ].mount.x ? "X" : "Y";
            }
        }
    }

    units::angle relative_dir = normalize( carry_veh->face.dir() - face.dir() );
    units::angle relative_180 = units::fmod( relative_dir, 180_degrees );
    units::angle face_dir_180 = normalize( face.dir(), 180_degrees );

    // if the carrier is skewed N/S and the carried vehicle isn't aligned with
    // the carrier, force the carried vehicle to be at a right angle
    if( face_dir_180 >= 45_degrees && face_dir_180 <= 135_degrees ) {
        if( relative_180 >= 45_degrees && relative_180 <= 135_degrees ) {
            if( relative_dir < 180_degrees ) {
                relative_dir = 90_degrees;
            } else {
                relative_dir = 270_degrees;
            }
        }
    }

    // We look at each of the structure parts (mount points, i.e. frames) for the
    // carry vehicle and then find a rack part adjacent to it. If we don't find a rack part,
    // then we can't merge.
    bool found_all_parts = true;
    for( auto carry_part : carry_veh_structs ) {

        // The current position on the original vehicle for this part
        tripoint carry_pos = carry_veh->global_part_pos3( carry_part );

        bool merged_part = false;
        for( int rack_part : rack_parts ) {
            size_t j = 0;
            // There's no mathematical transform from global pos3 to vehicle mount, so search for the
            // carry part in global pos3 after translating
            point carry_mount;
            for( point offset : four_cardinal_directions ) {
                carry_mount = parts[ rack_part ].mount + offset;
                tripoint possible_pos = mount_to_tripoint( carry_mount );
                if( possible_pos == carry_pos ) {
                    break;
                }
                ++j;
            }

            // We checked the adjacent points from the mounting rack and managed
            // to find the current structure part were looking for nearby. If the part was not
            // near this particular rack, we would look at each in the list of rack_parts
            const bool carry_part_next_to_this_rack = j < four_adjacent_offsets.size();
            if( carry_part_next_to_this_rack ) {
                mapping carry_map;
                point old_mount = carry_veh->parts[ carry_part ].mount;
                carry_map.carry_parts_here = carry_veh->parts_at_relative( old_mount, true );
                carry_map.rack_part = rack_part;
                carry_map.carry_mount = carry_mount;
                carry_map.old_mount = old_mount;
                carry_data.push_back( carry_map );
                merged_part = true;
                break;
            }
        }

        // We checked all the racks and could not find a place for this structure part.
        if( !merged_part ) {
            found_all_parts = false;
            break;
        }
    }

    // Now that we have mapped all the parts of the carry vehicle to the vehicle with the rack
    // we can go ahead and merge
    const point mount_zero{};
    if( found_all_parts ) {
        decltype( loot_zones ) new_zones;
        for( const auto &carry_map : carry_data ) {
            std::string offset = string_format( "%s%3d", carry_map.old_mount == mount_zero ? axis : " ",
                                                axis == "X" ? carry_map.old_mount.x : carry_map.old_mount.y );
            std::string unique_id = string_format( "%s%3d%s", offset,
                                                   static_cast<int>( to_degrees( relative_dir ) ),
                                                   carry_veh->name );
            for( int carry_part : carry_map.carry_parts_here ) {
                parts.push_back( std::move( carry_veh->parts[ carry_part ] ) );
                vehicle_part &carried_part = parts.back();
                carried_part.set_vehicle_hack( this );
                carried_part.mount = carry_map.carry_mount;
                carried_part.carry_names.push( unique_id );
                carried_part.enabled = false;
                carried_part.set_flag( vehicle_part::carried_flag );
                //give each carried part a tracked_flag so that we can re-enable overmap tracking on unloading if necessary
                if( carry_veh->tracking_on ) {
                    carried_part.set_flag( vehicle_part::tracked_flag );
                }
                parts[ carry_map.rack_part ].set_flag( vehicle_part::carrying_flag );
                carry_veh->parts[ carry_part ].removed = true;
            }
            refresh_locations_hack();

            const std::pair<std::unordered_multimap<point, zone_data>::iterator, std::unordered_multimap<point, zone_data>::iterator>
            zones_on_point = carry_veh->loot_zones.equal_range( carry_map.old_mount );
            for( std::unordered_multimap<point, zone_data>::const_iterator it = zones_on_point.first;
                 it != zones_on_point.second; ++it ) {
                new_zones.emplace( carry_map.carry_mount, it->second );
            }
        }

        for( auto &new_zone : new_zones ) {
            zone_manager::get_manager().create_vehicle_loot_zone(
                *this, new_zone.first, new_zone.second );
        }

        // Now that we've added zones to this vehicle, we need to make sure their positions
        // update when we next interact with them
        zones_dirty = true;

        //~ %1$s is the vehicle being loaded onto the bicycle rack
        add_msg( _( "You load the %1$s on the rack" ), carry_veh->name );
        map &here = get_map();
        carry_veh->part_removal_cleanup();
        here.dirty_vehicle_list.insert( this );
        here.set_transparency_cache_dirty( sm_pos.z );
        here.set_seen_cache_dirty( tripoint_zero );
        refresh();
    } else {
        //~ %1$s is the vehicle being loaded onto the bicycle rack
        add_msg( m_bad, _( "You can't get the %1$s on the rack" ), carry_veh->name );
    }
    return found_all_parts;
}

/**
 * Mark a part as removed from the vehicle.
 * @return bool true if the vehicle's 0,0 point shifted.
 */
bool vehicle::remove_part( const int p )
{
    DefaultRemovePartHandler handler;
    return remove_part( p, handler );
}

bool vehicle::remove_part( const int p, RemovePartHandler &handler )
{
    // NOTE: Don't access g or g->m or anything from it directly here.
    // Forward all access to the handler.
    // There are currently two implementations of it:
    // - one for normal game play (vehicle is on the main map g->m),
    // - one for mapgen (vehicle is on a temporary map used only during mapgen).
    if( p >= static_cast<int>( parts.size() ) ) {
        debugmsg( "Tried to remove part %d but only %d parts!", p, parts.size() );
        return false;
    }
    if( parts[p].removed ) {
        /* This happens only when we had to remove part, because it was depending on
         * other part (using recursive remove_part() call) - currently curtain
         * depending on presence of window and seatbelt depending on presence of seat.
         */
        return false;
    }

    const tripoint part_loc = global_part_pos3( p );

    // Unboard any entities standing on removed boardable parts
    if( part_flag( p, "BOARDABLE" ) && parts[p].has_flag( vehicle_part::passenger_flag ) ) {
        handler.unboard( part_loc );
    }

    // If `p` has flag `parent_flag`, remove child with flag `child_flag`
    // Returns true if removal occurs
    const auto remove_dependent_part = [&]( const std::string & parent_flag,
    const std::string & child_flag ) {
        if( part_flag( p, parent_flag ) ) {
            int dep = part_with_feature( p, child_flag, false );
            if( dep >= 0 && !magic ) {
                handler.add_item_or_charges( part_loc, parts[dep].properties_to_item(), false );
                remove_part( dep, handler );
                return true;
            }
        }
        return false;
    };

    // if a windshield is removed (usually destroyed) also remove curtains
    // attached to it.
    if( remove_dependent_part( "WINDOW", "CURTAIN" ) || part_flag( p, VPFLAG_OPAQUE ) ) {
        handler.set_transparency_cache_dirty( sm_pos.z );
    }

    if( part_flag( p, VPFLAG_ROOF ) || part_flag( p, VPFLAG_OPAQUE ) ) {
        handler.set_floor_cache_dirty( sm_pos.z + 1 );
    }

    remove_dependent_part( "SEAT", "SEATBELT" );
    remove_dependent_part( "BATTERY_MOUNT", "NEEDS_BATTERY_MOUNT" );

    // Release any animal held by the part
    if( parts[p].has_flag( vehicle_part::animal_flag ) ) {
        //TODO!: check, not sure what's going on here
        item &base = parts[p].get_base();
        handler.spawn_animal_from_part( base, part_loc );
        parts[p].remove_flag( vehicle_part::animal_flag );
    }

    // Update current engine configuration if needed
    if( part_flag( p, "ENGINE" ) && engines.size() > 1 ) {
        bool any_engine_on = false;

        for( auto &e : engines ) {
            if( e != p && is_part_on( e ) ) {
                any_engine_on = true;
                break;
            }
        }

        if( !any_engine_on ) {
            engine_on = false;
            for( auto &e : engines ) {
                toggle_specific_part( e, true );
            }
        }
    }

    //Remove loot zone if Cargo was removed.
    const auto lz_iter = loot_zones.find( parts[p].mount );
    const bool no_zone = lz_iter != loot_zones.end();

    if( no_zone && part_flag( p, "CARGO" ) ) {
        // Using the key here (instead of the iterator) will remove all zones on
        // this mount points regardless of how many there are
        loot_zones.erase( parts[p].mount );
        zones_dirty = true;
    }
    parts[p].removed = true;
    removed_part_count++;

    handler.removed( *this, p );

    point vp_mount = parts[p].mount;
    const auto iter = labels.find( label( vp_mount ) );
    if( iter != labels.end() && parts_at_relative( vp_mount, false ).empty() ) {
        labels.erase( iter );
    }

    for( auto &i : get_items( p ).clear() ) {
        // Note: this can spawn items on the other side of the wall!
        // TODO: fix this ^^
        tripoint dest( part_loc + point( rng( -3, 3 ), rng( -3, 3 ) ) );
        if( !magic ) {
            // This new point might be out of the map bounds.  It's not
            // reasonable to try to spawn it outside the currently valid map,
            // so we pass true here to cause such points to be clamped to the
            // valid bounds without printing an error (as would normally
            // occur).
            handler.add_item_or_charges( dest, std::move( i ), true );
        }
    }
    refresh();
    coeff_air_changed = true;
    return shift_if_needed();
}

void vehicle::part_removal_cleanup()
{
    bool changed = false;
    map &here = get_map();
    for( std::vector<vehicle_part>::iterator it = parts.begin(); it != parts.end(); /* noop */ ) {
        if( it->removed ) {
            auto items = get_items( std::distance( parts.begin(), it ) );
            while( !items.empty() ) {
                items.erase( items.begin() );
            }
            const tripoint &pt = global_part_pos3( *it );
            here.clear_vehicle_point_from_cache( this, pt );
            it = parts.erase( it );
            changed = true;
        } else {
            ++it;
        }
    }
    refresh_locations_hack();
    removed_part_count = 0;
    if( changed || parts.empty() ) {
        refresh();
        if( parts.empty() ) {
            here.destroy_vehicle( this );
            return;
        } else {
            here.add_vehicle_to_cache( this );
        }
    }
    shift_if_needed();
    refresh(); // Rebuild cached indices
    coeff_air_dirty = coeff_air_changed;
    coeff_air_changed = false;
}

void vehicle::remove_carried_flag()
{
    for( vehicle_part &part : parts ) {
        if( part.carry_names.empty() ) {
            // note: we get here if the part was added while the vehicle was carried / mounted. This is not expected.
            // still try to remove the carried flag, if any.
            part.remove_flag( vehicle_part::carried_flag );
        } else {
            part.carry_names.pop();
            if( part.carry_names.empty() ) {
                part.remove_flag( vehicle_part::carried_flag );
            }
        }
    }
}

void vehicle::remove_tracked_flag()
{
    for( vehicle_part &part : parts ) {
        if( part.carry_names.empty() ) {
            part.remove_flag( vehicle_part::tracked_flag );
        } else {
            part.carry_names.pop();
            if( part.carry_names.empty() ) {
                part.remove_flag( vehicle_part::tracked_flag );
            }
        }
    }
}

bool vehicle::remove_carried_vehicle( const std::vector<int> &carried_parts )
{
    if( carried_parts.empty() ) {
        return false;
    }
    std::string veh_record;
    tripoint new_pos3;
    bool x_aligned = false;
    bool tracked_parts =
        false; // we will set this to true if any of the vehicle parts carry a tracked_flag
    for( int carried_part : carried_parts ) {
        //check if selected part carries tracking flag
        if( parts[carried_part].has_flag(
                vehicle_part::tracked_flag ) ) { //this should only need to run once
            tracked_parts = true;
        }
        const auto &carry_names = parts[carried_part].carry_names;
        if( !carry_names.empty() ) {
            std::string id_string = carry_names.top().substr( 0, 1 );
            if( id_string == "X" || id_string == "Y" ) {
                veh_record = carry_names.top();
                new_pos3 = global_part_pos3( carried_part );
                x_aligned = id_string == "X";
                break;
            }
        }
    }
    if( veh_record.empty() ) {
        return false;
    }
    units::angle new_dir =
        normalize( units::from_degrees( std::stoi( veh_record.substr( 4, 3 ) ) ) + face.dir() );
    units::angle host_dir = normalize( face.dir(), 180_degrees );
    // if the host is skewed N/S, and the carried vehicle is going to come at an angle,
    // force it to east/west instead
    if( host_dir >= 45_degrees && host_dir <= 135_degrees ) {
        if( new_dir <= 45_degrees || new_dir >= 315_degrees ) {
            new_dir = 0_degrees;
        } else if( new_dir >= 135_degrees && new_dir <= 225_degrees ) {
            new_dir = 180_degrees;
        }
    }
    vehicle *new_vehicle = g->m.add_vehicle( vproto_id( "none" ), new_pos3, new_dir );
    if( new_vehicle == nullptr ) {
        add_msg( m_debug, "Unable to unload bike rack, host face %d, new_dir %d!",
                 to_degrees( face.dir() ), to_degrees( new_dir ) );
        return false;
    }
    new_vehicle->owner = owner;
    new_vehicle->old_owner = old_owner;
    std::vector<point> new_mounts;
    new_vehicle->name = veh_record.substr( vehicle_part::name_offset );
    for( auto carried_part : carried_parts ) {
        point new_mount;
        std::string mount_str;
        if( !parts[carried_part].carry_names.empty() ) {
            // the mount string should be something like "X 0 0" for ex. We get the first number out of it.
            mount_str = parts[carried_part].carry_names.top().substr( 1, 3 );
        } else {
            // FIX #28712; if we get here it means that a part was added to the bike while the latter was a carried vehicle.
            // This part didn't get a carry_names because those are assigned when the carried vehicle is loaded.
            // We can't be sure to which vehicle it really belongs to, so it will be detached from the vehicle.
            // We can at least inform the player that there's something wrong.
            add_msg( m_warning,
                     _( "A part of the vehicle ('%s') has no containing vehicle's name.  It will be detached from the %s vehicle." ),
                     parts[carried_part].name(),  new_vehicle->name );

            // check if any other parts at the same location have a valid carry name so we can still have a valid mount location.
            for( auto &local_part : parts_at_relative( parts[carried_part].mount, true ) ) {
                if( !parts[local_part].carry_names.empty() ) {
                    mount_str = parts[local_part].carry_names.top().substr( 1, 3 );
                    break;
                }
            }
        }
        if( mount_str.empty() ) {
            add_msg( m_bad,
                     _( "There's not viable mount location on this vehicle: %s. It can't be unloaded from the rack." ),
                     new_vehicle->name );
            return false;
        }
        if( x_aligned ) {
            new_mount.x = std::stoi( mount_str );
        } else {
            new_mount.y = std::stoi( mount_str );
        }
        new_mounts.push_back( new_mount );
    }

    std::vector<vehicle *> new_vehicles;
    new_vehicles.push_back( new_vehicle );
    std::vector<std::vector<int>> carried_vehicles;
    carried_vehicles.push_back( carried_parts );
    std::vector<std::vector<point>> carried_mounts;
    carried_mounts.push_back( new_mounts );
    const bool success = split_vehicles( carried_vehicles, new_vehicles, carried_mounts );
    if( success ) {
        //~ %s is the vehicle being loaded onto the bicycle rack
        add_msg( _( "You unload the %s from the bike rack." ), new_vehicle->name );
        new_vehicle->remove_carried_flag();
        if( tracked_parts ) {
            new_vehicle->toggle_tracking(); //turn on tracking for our newly created vehicle
            new_vehicle->remove_tracked_flag(); //remove our tracking flags now that the vehicle isn't carried
        }
        g->m.dirty_vehicle_list.insert( this );
        part_removal_cleanup();
    } else {
        //~ %s is the vehicle being loaded onto the bicycle rack
        add_msg( m_bad, _( "You can't unload the %s from the bike rack." ), new_vehicle->name );
    }
    return success;
}

// split the current vehicle into up to 3 new vehicles that do not connect to each other
bool vehicle::find_and_split_vehicles( int exclude )
{
    std::vector<int> valid_parts = all_parts_at_location( part_location_structure );
    std::set<int> checked_parts;
    checked_parts.insert( exclude );

    std::vector<std::vector <int>> all_vehicles;

    for( size_t cnt = 0 ; cnt < 4 ; cnt++ ) {
        int test_part = -1;
        for( auto p : valid_parts ) {
            if( parts[ p ].removed ) {
                continue;
            }
            if( !checked_parts.contains( p ) ) {
                test_part = p;
                break;
            }
        }
        if( test_part == -1 || static_cast<size_t>( test_part ) > parts.size() ) {
            break;
        }

        std::queue<std::pair<int, std::vector<int>>> search_queue;

        const auto push_neighbor = [&]( int p, const std::vector<int> &with_p ) {
            std::pair<int, std::vector<int>> data( p, with_p );
            search_queue.push( data );
        };
        auto pop_neighbor = [&]() {
            std::pair<int, std::vector<int>> result = search_queue.front();
            search_queue.pop();
            return result;
        };

        std::vector<int> veh_parts;
        push_neighbor( test_part, parts_at_relative( parts[ test_part ].mount, true ) );
        while( !search_queue.empty() ) {
            std::pair<int, std::vector<int>> test_set = pop_neighbor();
            test_part = test_set.first;
            if( checked_parts.contains( test_part ) ) {
                continue;
            }
            for( auto p : test_set.second ) {
                veh_parts.push_back( p );
            }
            checked_parts.insert( test_part );
            for( point offset : four_adjacent_offsets ) {
                const point dp = parts[test_part].mount + offset;
                std::vector<int> all_neighbor_parts = parts_at_relative( dp, true );
                int neighbor_struct_part = -1;
                for( int p : all_neighbor_parts ) {
                    if( parts[ p ].removed ) {
                        continue;
                    }
                    if( part_info( p ).location == part_location_structure ) {
                        neighbor_struct_part = p;
                        break;
                    }
                }
                if( neighbor_struct_part != -1 ) {
                    push_neighbor( neighbor_struct_part, all_neighbor_parts );
                }
            }
        }
        // don't include the first vehicle's worth of parts
        if( cnt > 0 ) {
            all_vehicles.push_back( veh_parts );
        }
    }

    if( !all_vehicles.empty() ) {
        bool success = split_vehicles( all_vehicles );
        if( success ) {
            // update the active cache
            shift_parts( point_zero );
            return true;
        }
    }
    return false;
}

void vehicle::relocate_passengers( const std::vector<Character *> &passengers )
{
    const auto boardables = get_avail_parts( "BOARDABLE" );
    for( auto *passenger : passengers ) {
        for( const vpart_reference &vp : boardables ) {
            if( vp.part().passenger_id == passenger->getID() ) {
                passenger->setpos( vp.pos() );
            }
        }
    }
}

// Split a vehicle into an old vehicle and one or more new vehicles by moving vehicle_parts
// from one the old vehicle to the new vehicles.
// some of the logic borrowed from remove_part
// skipped the grab, curtain, player activity, and engine checks because they deal
// with pos, not a vehicle pointer
// @param new_vehs vector of vectors of part indexes to move to new vehicles
// @param new_vehicles vector of vehicle pointers containing the new vehicles; if empty, new
// vehicles will be created
// @param new_mounts vector of vector of mount points. must have one vector for every vehicle*
// in new_vehicles, and forces the part indices in new_vehs to be mounted on the new vehicle
// at those mount points
bool vehicle::split_vehicles( const std::vector<std::vector <int>> &new_vehs,
                              const std::vector<vehicle *> &new_vehicles,
                              const std::vector<std::vector <point>> &new_mounts )
{
    bool did_split = false;
    size_t i = 0;
    for( i = 0; i < new_vehs.size(); i ++ ) {
        std::vector<int> split_parts = new_vehs[ i ];
        if( split_parts.empty() ) {
            continue;
        }
        std::vector<point> split_mounts = new_mounts[ i ];
        did_split = true;

        vehicle *new_vehicle = nullptr;
        if( i < new_vehicles.size() ) {
            new_vehicle = new_vehicles[ i ];
        }
        int split_part0 = split_parts.front();
        tripoint new_v_pos3;
        point mnt_offset;

        decltype( labels ) new_labels;
        decltype( loot_zones ) new_zones;
        if( new_vehicle == nullptr ) {
            // make sure the split_part0 is a legal 0,0 part
            if( split_parts.size() > 1 ) {
                for( size_t sp = 0; sp < split_parts.size(); sp++ ) {
                    int p = split_parts[ sp ];
                    if( part_info( p ).location == part_location_structure &&
                        !part_info( p ).has_flag( "PROTRUSION" ) ) {
                        split_part0 = sp;
                        break;
                    }
                }
            }
            new_v_pos3 = global_part_pos3( parts[ split_part0 ] );
            mnt_offset = parts[ split_part0 ].mount;
            new_vehicle = g->m.add_vehicle( vproto_id( "none" ), new_v_pos3, face.dir() );
            if( new_vehicle == nullptr ) {
                // the split part was out of the map bounds.
                continue;
            }
            new_vehicle->owner = owner;
            new_vehicle->old_owner = old_owner;
            new_vehicle->name = name;
            new_vehicle->move = move;
            new_vehicle->turn_dir = turn_dir;
            new_vehicle->velocity = velocity;
            new_vehicle->vertical_velocity = vertical_velocity;
            new_vehicle->cruise_velocity = cruise_velocity;
            new_vehicle->cruise_on = cruise_on;
            new_vehicle->engine_on = engine_on;
            new_vehicle->tracking_on = tracking_on;
            new_vehicle->camera_on = camera_on;
        }
        new_vehicle->last_fluid_check = last_fluid_check;

        std::vector<Character *> passengers;
        for( size_t new_part = 0; new_part < split_parts.size(); new_part++ ) {
            int mov_part = split_parts[ new_part ];
            point cur_mount = parts[ mov_part ].mount;
            point new_mount = cur_mount;
            if( !split_mounts.empty() ) {
                new_mount = split_mounts[ new_part ];
            } else {
                new_mount -= mnt_offset;
            }

            player *passenger = nullptr;
            // Unboard any entities standing on any transferred part
            if( part_flag( mov_part, "BOARDABLE" ) ) {
                passenger = get_passenger( mov_part );
                if( passenger ) {
                    passengers.push_back( passenger );
                }
            }
            // if this part is a towing part, transfer the tow_data to the new vehicle.
            if( part_flag( mov_part, "TOW_CABLE" ) ) {
                if( is_towed() ) {
                    tow_data.get_towed_by()->tow_data.set_towing( tow_data.get_towed_by(), new_vehicle );
                    tow_data.clear_towing();
                } else if( is_towing() ) {
                    tow_data.get_towed()->tow_data.set_towing( new_vehicle, tow_data.get_towed() );
                    tow_data.clear_towing();
                }
            }
            // transfer the vehicle_part to the new vehicle
            new_vehicle->parts.emplace_back( parts[ mov_part ], new_vehicle );
            vehicle_part &np = new_vehicle->parts.back();
            np.mount = new_mount;
            np.set_vehicle_hack( new_vehicle );

            // remove labels associated with the mov_part
            const auto iter = labels.find( label( cur_mount ) );
            if( iter != labels.end() ) {
                std::string label_str = iter->text;
                labels.erase( iter );
                new_labels.insert( label( new_mount, label_str ) );
            }
            // Prepare the zones to be moved to the new vehicle
            const std::pair<std::unordered_multimap<point, zone_data>::iterator, std::unordered_multimap<point, zone_data>::iterator>
            zones_on_point = loot_zones.equal_range( cur_mount );
            for( std::unordered_multimap<point, zone_data>::const_iterator lz_iter = zones_on_point.first;
                 lz_iter != zones_on_point.second; ++lz_iter ) {
                new_zones.emplace( new_mount, lz_iter->second );
            }

            // Erasing on the key removes all the zones from the point at once
            loot_zones.erase( cur_mount );

            // The zone manager will be updated when we next interact with it through get_vehicle_zones
            zones_dirty = true;

            // remove the passenger from the old vehicle
            if( passenger ) {
                parts[ mov_part ].remove_flag( vehicle_part::passenger_flag );
                parts[ mov_part ].passenger_id = character_id();
            }
            // indicate the part needs to be removed from the old vehicle
            parts[ mov_part].removed = true;
            removed_part_count++;
        }

        new_vehicle->refresh_locations_hack();
        // We want to create the vehicle zones after we've setup the parts
        // because we need only to move the zone once per mount, not per part. If we move per
        // part, we will end up with duplicates of the zone per part on the same mount
        for( std::pair<point, zone_data> zone : new_zones ) {
            zone_manager::get_manager().create_vehicle_loot_zone( *new_vehicle, zone.first, zone.second );
        }

        // create_vehicle_loot_zone marks the vehicle as not dirty but since we got these zones
        // in an unknown state from the previous vehicle, we need to let the cache rebuild next
        // time we interact with them
        new_vehicle->zones_dirty = true;

        map &here = get_map();
        here.dirty_vehicle_list.insert( new_vehicle );
        here.set_transparency_cache_dirty( sm_pos.z );
        here.set_seen_cache_dirty( tripoint_zero );
        if( !new_labels.empty() ) {
            new_vehicle->labels = new_labels;
        }

        if( split_mounts.empty() ) {
            new_vehicle->refresh();
        } else {
            // include refresh
            new_vehicle->shift_parts( point_zero - mnt_offset );
        }

        // update the precalc points
        new_vehicle->precalc_mounts( 1, new_vehicle->skidding ?
                                     new_vehicle->turn_dir : new_vehicle->face.dir(),
                                     new_vehicle->pivot_point() );
        new_vehicle->adjust_zlevel( 1, tripoint_zero );
        new_vehicle->shift_zlevel();
        if( !passengers.empty() ) {
            new_vehicle->relocate_passengers( passengers );
        }
    }
    if( did_split ) {
        part_removal_cleanup();
    }
    return did_split;
}

bool vehicle::split_vehicles( const std::vector<std::vector <int>> &new_vehs )
{
    std::vector<vehicle *> null_vehicles;
    std::vector<std::vector <point>> null_mounts;
    std::vector<point> nothing;
    null_vehicles.assign( new_vehs.size(), nullptr );
    null_mounts.assign( new_vehs.size(), nothing );
    return split_vehicles( new_vehs, null_vehicles, null_mounts );
}

item &vehicle::part_base( int p )
{
    return *parts[ p ].base;
}

int vehicle::find_part( const item &it ) const
{
    auto idx = std::find_if( parts.begin(), parts.end(), [&it]( const vehicle_part & e ) {
        return e.base == &it;
    } );
    return idx != parts.end() ? std::distance( parts.begin(), idx ) : INT_MIN;
}

std::vector<detached_ptr<item>> vehicle_part::pieces_for_broken_part() const
{
    const item_group_id &group = info().breaks_into_group;
    // TODO: make it optional? Or use id of empty item group?
    if( !group ) {
        return {};
    }

    return item_group::items_from( group, calendar::turn );
}

std::vector<int> vehicle::parts_at_relative( point dp,
        const bool use_cache ) const
{
    if( !use_cache ) {
        std::vector<int> res;
        for( const vpart_reference &vp : get_all_parts() ) {
            if( vp.mount() == dp && !vp.part().removed ) {
                res.push_back( static_cast<int>( vp.part_index() ) );
            }
        }
        return res;
    } else {
        const auto &iter = relative_parts.find( dp );
        if( iter != relative_parts.end() ) {
            return iter->second;
        } else {
            std::vector<int> res;
            return res;
        }
    }
}

std::optional<vpart_reference> vpart_position::obstacle_at_part() const
{
    const std::optional<vpart_reference> part = part_with_feature( VPFLAG_OBSTACLE, true );
    if( !part ) {
        return std::nullopt; // No obstacle here
    }

    if( part->has_feature( VPFLAG_OPENABLE ) && part->part().open ) {
        return std::nullopt; // Open door here
    }

    return part;
}

std::optional<vpart_reference> vpart_position::part_displayed() const
{
    int part_id = vehicle().part_displayed_at( mount() );
    if( part_id == -1 ) {
        return std::nullopt;
    }
    return vpart_reference( vehicle(), part_id );
}

std::optional<vpart_reference> vpart_position::part_with_feature( const std::string &f,
        const bool unbroken ) const
{
    const int i = vehicle().part_with_feature( part_index(), f, unbroken );
    if( i < 0 ) {
        return std::nullopt;
    }
    return vpart_reference( vehicle(), i );
}

std::optional<vpart_reference> vpart_position::part_with_feature( const vpart_bitflags f,
        const bool unbroken ) const
{
    const int i = vehicle().part_with_feature( part_index(), f, unbroken );
    if( i < 0 ) {
        return std::nullopt;
    }
    return vpart_reference( vehicle(), i );
}

std::optional<vpart_reference> optional_vpart_position::part_with_feature( const std::string &f,
        const bool unbroken ) const
{
    return has_value() ? value().part_with_feature( f, unbroken ) : std::nullopt;
}

std::optional<vpart_reference> optional_vpart_position::part_with_feature( const vpart_bitflags f,
        const bool unbroken ) const
{
    return has_value() ? value().part_with_feature( f, unbroken ) : std::nullopt;
}

std::optional<vpart_reference> optional_vpart_position::obstacle_at_part() const
{
    return has_value() ? value().obstacle_at_part() : std::nullopt;
}

std::optional<vpart_reference> optional_vpart_position::part_displayed() const
{
    return has_value() ? value().part_displayed() : std::nullopt;
}

int vehicle::part_with_feature( int part, vpart_bitflags const flag, bool unbroken ) const
{
    if( part_flag( part, flag ) && ( !unbroken || !parts[part].is_broken() ) ) {
        return part;
    }
    const auto it = relative_parts.find( parts[part].mount );
    if( it != relative_parts.end() ) {
        const std::vector<int> &parts_here = it->second;
        for( auto &i : parts_here ) {
            if( part_flag( i, flag ) && ( !unbroken || !parts[i].is_broken() ) ) {
                return i;
            }
        }
    }
    return -1;
}

int vehicle::part_with_feature( int part, const std::string &flag, bool unbroken ) const
{
    return part_with_feature( parts[part].mount, flag, unbroken );
}

int vehicle::part_with_feature( point pt, const std::string &flag, bool unbroken ) const
{
    std::vector<int> parts_here = parts_at_relative( pt, true );
    for( auto &elem : parts_here ) {
        if( part_flag( elem, flag ) && ( !unbroken || !parts[ elem ].is_broken() ) ) {
            return elem;
        }
    }
    return -1;
}

int vehicle::avail_part_with_feature( int part, vpart_bitflags const flag, bool unbroken ) const
{
    int part_a = part_with_feature( part, flag, unbroken );
    if( ( part_a >= 0 ) && parts[ part_a ].is_available() ) {
        return part_a;
    }
    return -1;
}

int vehicle::avail_part_with_feature( int part, const std::string &flag, bool unbroken ) const
{
    return avail_part_with_feature( parts[ part ].mount, flag, unbroken );
}

int vehicle::avail_part_with_feature( point pt, const std::string &flag,
                                      bool unbroken ) const
{
    int part_a = part_with_feature( pt, flag, unbroken );
    if( ( part_a >= 0 ) && parts[ part_a ].is_available() ) {
        return part_a;
    }
    return -1;
}

bool vehicle::has_part( const std::string &flag, bool enabled ) const
{
    return std::any_of( parts.begin(), parts.end(), [&flag, &enabled]( const vehicle_part & e ) {
        return !e.removed && ( !enabled || e.enabled ) && !e.is_broken() && e.info().has_flag( flag );
    } );
}

bool vehicle::has_part( const vpart_bitflags &flag, bool enabled ) const
{
    return std::any_of( parts.begin(), parts.end(), [&flag, &enabled]( const vehicle_part & e ) {
        return !e.removed && ( !enabled || e.enabled ) && !e.is_broken() && e.info().has_flag( flag );
    } );
}

bool vehicle::has_part( const tripoint &pos, const std::string &flag, bool enabled ) const
{
    const tripoint relative_pos = pos - global_pos3();

    for( const auto &e : parts ) {
        if( e.precalc[0] != relative_pos ) {
            continue;
        }
        if( !e.removed && ( !enabled || e.enabled ) && !e.is_broken() && e.info().has_flag( flag ) ) {
            return true;
        }
    }
    return false;
}

int vehicle::obstacle_at_position( point pos ) const
{
    int i = part_with_feature( pos, "OBSTACLE", true );

    if( i == -1 ) {
        return -1;
    }

    auto &ref = parts[i];

    if( ref.info().has_flag( VPFLAG_OPENABLE ) && ref.open ) {
        return -1;
    }

    return i;
}

int vehicle::opaque_at_position( point pos ) const
{
    int i = part_with_feature( pos, "OPAQUE", true );

    if( i == -1 ) {
        return -1;
    }

    auto &ref = parts[i];

    if( ref.info().has_flag( VPFLAG_OPENABLE ) && ref.open ) {
        return -1;
    }

    return i;
}

std::vector<vehicle_part *> vehicle::get_parts_at( const tripoint &pos, const std::string &flag,
        const part_status_flag condition )
{
    const tripoint relative_pos = pos - global_pos3();
    std::vector<vehicle_part *> res;
    for( auto &e : parts ) {
        if( e.precalc[ 0 ] != relative_pos ) {
            continue;
        }
        if( !e.removed &&
            ( flag.empty() || e.info().has_flag( flag ) ) &&
            ( !( condition & part_status_flag::enabled ) || e.enabled ) &&
            ( !( condition & part_status_flag::working ) || !e.is_broken() ) ) {
            res.push_back( &e );
        }
    }
    return res;
}

std::vector<const vehicle_part *> vehicle::get_parts_at( const tripoint &pos,
        const std::string &flag,
        const part_status_flag condition ) const
{
    const tripoint relative_pos = pos - global_pos3();
    std::vector<const vehicle_part *> res;
    for( const auto &e : parts ) {
        if( e.precalc[ 0 ] != relative_pos ) {
            continue;
        }
        if( !e.removed &&
            ( flag.empty() || e.info().has_flag( flag ) ) &&
            ( !( condition & part_status_flag::enabled ) || e.enabled ) &&
            ( !( condition & part_status_flag::working ) || !e.is_broken() ) ) {
            res.push_back( &e );
        }
    }
    return res;
}

std::optional<std::string> vpart_position::get_label() const
{
    const auto it = vehicle().labels.find( label( mount() ) );
    if( it == vehicle().labels.end() ) {
        return std::nullopt;
    }
    if( it->text.empty() ) {
        // legacy support TODO: change labels into a map and keep track of deleted labels
        return std::nullopt;
    }
    return it->text;
}

void vpart_position::set_label( const std::string &text ) const
{
    auto &labels = vehicle().labels;
    const auto it = labels.find( label( mount() ) );
    // TODO: empty text should remove the label instead of just storing an empty string, see get_label
    if( it == labels.end() ) {
        labels.insert( label( mount(), text ) );
    } else {
        // labels should really be a map
        labels.insert( labels.erase( it ), label( mount(), text ) );
    }
}

int vehicle::next_part_to_close( int p, bool outside ) const
{
    std::vector<int> parts_here = parts_at_relative( parts[p].mount, true );

    // We want reverse, since we close the outermost thing first (curtains), and then the innermost thing (door)
    for( std::vector<int>::reverse_iterator part_it = parts_here.rbegin();
         part_it != parts_here.rend();
         ++part_it ) {

        if( part_flag( *part_it, VPFLAG_OPENABLE )
            && parts[ *part_it ].is_available()
            && parts[*part_it].open == 1
            && ( !outside || !part_flag( *part_it, "OPENCLOSE_INSIDE" ) ) ) {
            return *part_it;
        }
    }
    return -1;
}

int vehicle::next_part_to_open( int p, bool outside ) const
{
    std::vector<int> parts_here = parts_at_relative( parts[p].mount, true );

    // We want forwards, since we open the innermost thing first (curtains), and then the innermost thing (door)
    for( auto &elem : parts_here ) {
        if( part_flag( elem, VPFLAG_OPENABLE ) && parts[ elem ].is_available() && parts[elem].open == 0 &&
            ( !outside || !part_flag( elem, "OPENCLOSE_INSIDE" ) ) ) {
            return elem;
        }
    }
    return -1;
}

vehicle_part_with_feature_range<std::string> vehicle::get_avail_parts( std::string feature ) const
{
    return vehicle_part_with_feature_range<std::string>( const_cast<vehicle &>( *this ),
            std::move( feature ),
            ( part_status_flag::working |
              part_status_flag::available ) );
}

vehicle_part_with_feature_range<vpart_bitflags> vehicle::get_avail_parts(
    const vpart_bitflags feature ) const
{
    return vehicle_part_with_feature_range<vpart_bitflags>( const_cast<vehicle &>( *this ), feature,
            ( part_status_flag::working |
              part_status_flag::available ) );
}

vehicle_part_with_feature_range<std::string> vehicle::get_parts_including_carried(
    std::string feature ) const
{
    return vehicle_part_with_feature_range<std::string>( const_cast<vehicle &>( *this ),
            std::move( feature ), part_status_flag::working );
}

vehicle_part_with_feature_range<vpart_bitflags> vehicle::get_parts_including_carried(
    const vpart_bitflags feature ) const
{
    return vehicle_part_with_feature_range<vpart_bitflags>( const_cast<vehicle &>( *this ), feature,
            part_status_flag::working );
}

vehicle_part_with_feature_range<std::string> vehicle::get_any_parts( std::string feature ) const
{
    return vehicle_part_with_feature_range<std::string>( const_cast<vehicle &>( *this ),
            std::move( feature ), part_status_flag::any );
}

vehicle_part_with_feature_range<vpart_bitflags> vehicle::get_any_parts(
    const vpart_bitflags feature ) const
{
    return vehicle_part_with_feature_range<vpart_bitflags>( const_cast<vehicle &>( *this ), feature,
            part_status_flag::any );
}

vehicle_part_with_feature_range<std::string> vehicle::get_enabled_parts( std::string feature ) const
{
    return vehicle_part_with_feature_range<std::string>( const_cast<vehicle &>( *this ),
            std::move( feature ),
            ( part_status_flag::enabled |
              part_status_flag::working |
              part_status_flag::available ) );
}

vehicle_part_with_feature_range<vpart_bitflags> vehicle::get_enabled_parts(
    const vpart_bitflags feature ) const
{
    return vehicle_part_with_feature_range<vpart_bitflags>( const_cast<vehicle &>( *this ), feature,
            ( part_status_flag::enabled |
              part_status_flag::working |
              part_status_flag::available ) );
}

/**
 *
 * Returns all the parts in the vehicle that are either a structural part or
 * a extendable protusion part
 * @return A list of indices to all parts with the structure location or otherwise standalone
 */
std::vector<int> vehicle::all_standalone_parts() const
{
    std::vector<int> parts_found;
    for( size_t part_index = 0; part_index < parts.size(); ++part_index ) {
        if( ( part_info( part_index ).location == part_location_structure ||
              part_info( part_index ).has_flag( VPFLAG_EXTENDABLE ) ) &&
            !parts[part_index].removed ) {
            parts_found.push_back( part_index );
        }
    }
    return parts_found;
}
/**
 * Returns all parts in the vehicle that exist in the given location slot. If
 * the empty string is passed in, returns all parts with no slot.
 * @param location The location slot to get parts for.
 * @return A list of indices to all parts with the specified location.
 */
std::vector<int> vehicle::all_parts_at_location( const std::string &location ) const
{
    std::vector<int> parts_found;
    for( size_t part_index = 0; part_index < parts.size(); ++part_index ) {
        if( part_info( part_index ).location == location && !parts[part_index].removed ) {
            parts_found.push_back( part_index );
        }
    }
    return parts_found;
}

// another NPC probably removed a part in the time it took to walk here and start the activity.
// as the part index was first "chosen" before the NPC started traveling here.
// therefore the part index is now invalid shifted by one or two ( depending on how many other NPCs working on this vehicle )
// so loop over the part indexes in reverse order to get the next one down that matches the part type we wanted to remove
int vehicle::get_next_shifted_index( int original_index, Character &who )
{
    int ret_index = original_index;
    bool found_shifted_index = false;
    for( std::vector<vehicle_part>::reverse_iterator it = parts.rbegin(); it != parts.rend(); ++it ) {
        if( who.get_value( "veh_index_type" ) == it->info().name() ) {
            ret_index = index_of_part( &*it );
            found_shifted_index = true;
            break;
        }
    }
    if( !found_shifted_index ) {
        // we are probably down to a few parts left, and things get messy here, so an alternative index maybe can't be found
        // if loads of npcs are all removing parts at the same time.
        // if that's the case, just bail out and give up, somebody else is probably doing the job right now anyway.
        return -1;
    } else {
        return ret_index;
    }
}

/**
 * Returns all parts in the vehicle that have the specified flag in their vpinfo and
 * are on the same X-axis or Y-axis as the input part and are contiguous with each other.
 * @param part The part to find adjacent parts to
 * @param flag The flag to match
 * @return A list of lists of indices of all parts sharing the flag and contiguous with the part
 * on the X or Y axis. Returns 0, 1, or 2 lists of indices.
 */
std::vector<std::vector<int>> vehicle::find_lines_of_parts( int part, const std::string &flag )
{
    const auto possible_parts = get_avail_parts( flag );
    std::vector<std::vector<int>> ret_parts;
    if( empty( possible_parts ) ) {
        return ret_parts;
    }

    std::vector<int> x_parts;
    std::vector<int> y_parts;
    vpart_id part_id = part_info( part ).get_id();
    // create vectors of parts on the same X or Y axis
    point target = parts[ part ].mount;
    for( const vpart_reference &vp : possible_parts ) {
        if( vp.part().is_unavailable() ||
            !vp.has_feature( "MULTISQUARE" ) ||
            vp.info().get_id() != part_id )  {
            continue;
        }
        if( vp.mount().x == target.x ) {
            x_parts.push_back( vp.part_index() );
        }
        if( vp.mount().y == target.y ) {
            y_parts.push_back( vp.part_index() );
        }
    }

    if( x_parts.size() > 1 ) {
        std::vector<int> x_ret;
        // sort by Y-axis, since they're all on the same X-axis
        const auto x_sorter = [&]( const int lhs, const int rhs ) {
            return( parts[lhs].mount.y > parts[rhs].mount.y );
        };
        std::sort( x_parts.begin(), x_parts.end(), x_sorter );
        int first_part = 0;
        int prev_y = parts[ x_parts[ 0 ] ].mount.y;
        int i;
        bool found_part = x_parts[ 0 ] == part;
        for( i = 1; static_cast<size_t>( i ) < x_parts.size(); i++ ) {
            // if the Y difference is > 1, there's a break in the run
            if( std::abs( parts[ x_parts[ i ] ].mount.y - prev_y )  > 1 ) {
                // if we found the part, this is the run we wanted
                if( found_part ) {
                    break;
                }
                first_part = i;
            }
            found_part |= x_parts[ i ] == part;
            prev_y = parts[ x_parts[ i ] ].mount.y;
        }
        for( size_t j = first_part; j < static_cast<size_t>( i ); j++ ) {
            x_ret.push_back( x_parts[ j ] );
        }
        ret_parts.push_back( x_ret );
    }
    if( y_parts.size() > 1 ) {
        std::vector<int> y_ret;
        const auto y_sorter = [&]( const int lhs, const int rhs ) {
            return( parts[lhs].mount.x > parts[rhs].mount.x );
        };
        std::sort( y_parts.begin(), y_parts.end(), y_sorter );
        int first_part = 0;
        int prev_x = parts[ y_parts[ 0 ] ].mount.x;
        int i;
        bool found_part = y_parts[ 0 ] == part;
        for( i = 1; static_cast<size_t>( i ) < y_parts.size(); i++ ) {
            if( std::abs( parts[ y_parts[ i ] ].mount.x - prev_x )  > 1 ) {
                if( found_part ) {
                    break;
                }
                first_part = i;
            }
            found_part |= y_parts[ i ] == part;
            prev_x = parts[ y_parts[ i ] ].mount.x;
        }
        for( size_t j = first_part; j < static_cast<size_t>( i ); j++ ) {
            y_ret.push_back( y_parts[ j ] );
        }
        ret_parts.push_back( y_ret );
    }
    if( y_parts.size() == 1 && x_parts.size() == 1 ) {
        ret_parts.push_back( x_parts );
    }
    return ret_parts;
}

bool vehicle::part_flag( int part, const std::string &flag ) const
{
    if( part < 0 || part >= static_cast<int>( parts.size() ) || parts[part].removed ) {
        return false;
    } else {
        return part_info( part ).has_flag( flag );
    }
}

bool vehicle::part_flag( int part, const vpart_bitflags flag ) const
{
    if( part < 0 || part >= static_cast<int>( parts.size() ) || parts[part].removed ) {
        return false;
    } else {
        return part_info( part ).has_flag( flag );
    }
}

int vehicle::part_at( point dp ) const
{
    for( const vpart_reference &vp : get_all_parts() ) {
        if( vp.part().precalc[0].xy() == dp && !vp.part().removed ) {
            return static_cast<int>( vp.part_index() );
        }
    }
    return -1;
}

/**
 * Given a vehicle part which is inside of this vehicle, returns the index of
 * that part. This exists solely because activities relating to vehicle editing
 * require the index of the vehicle part to be passed around.
 * @param part The part to find.
 * @param check_removed Check whether this part can be removed
 * @return The part index, -1 if it is not part of this vehicle.
 */
int vehicle::index_of_part( const vehicle_part *const part, const bool check_removed ) const
{
    if( part != nullptr ) {
        for( const vpart_reference &vp : get_all_parts() ) {
            const vehicle_part &next_part = vp.part();
            if( !check_removed && next_part.removed ) {
                continue;
            }
            if( part->id == next_part.id && part->mount == vp.mount() ) {
                return vp.part_index();
            }
        }
    }
    return -1;
}

/**
 * Returns which part (as an index into the parts list) is the one that will be
 * displayed for the given square. Returns -1 if there are no parts in that
 * square.
 * @param dp The local coordinate.
 * @return The index of the part that will be displayed.
 */
int vehicle::part_displayed_at( point dp ) const
{
    // Z-order is implicitly defined in game::load_vehiclepart, but as
    // numbers directly set on parts rather than constants that can be
    // used elsewhere. A future refactor might be nice but this way
    // it's clear where the magic number comes from.
    const int ON_ROOF_Z = 9;

    std::vector<int> parts_in_square = parts_at_relative( dp, true );

    if( parts_in_square.empty() ) {
        return -1;
    }

    bool in_vehicle = g->u.in_vehicle;
    if( in_vehicle ) {
        // They're in a vehicle, but are they in /this/ vehicle?
        std::vector<int> psg_parts = boarded_parts();
        in_vehicle = false;
        for( auto &psg_part : psg_parts ) {
            if( get_passenger( psg_part ) == &g->u ) {
                in_vehicle = true;
                break;
            }
        }
    }

    int hide_z_at_or_above = ( in_vehicle ) ? ( ON_ROOF_Z ) : INT_MAX;

    int top_part = 0;
    for( size_t index = 1; index < parts_in_square.size(); index++ ) {
        if( ( part_info( parts_in_square[top_part] ).z_order <
              part_info( parts_in_square[index] ).z_order ) &&
            ( part_info( parts_in_square[index] ).z_order <
              hide_z_at_or_above ) ) {
            top_part = index;
        }
    }

    return parts_in_square[top_part];
}

int vehicle::roof_at_part( const int part ) const
{
    std::vector<int> parts_in_square = parts_at_relative( parts[part].mount, true );
    for( const int p : parts_in_square ) {
        if( part_info( p ).location == "on_roof" || part_flag( p, "ROOF" ) ) {
            return p;
        }
    }

    return -1;
}

point vehicle::coord_translate( point p ) const
{
    tripoint q;
    coord_translate( pivot_rotation[0], pivot_anchor[0], p, q );
    return q.xy();
}

const struct {
    float gradient;
    bool flipH;
    bool flipV;
    bool swapXY;
} rotation_info[24] = {
    {static_cast<float>( tan( units::to_radians( 0_degrees ) ) ),  false, false,   false}, //0 degrees
    {static_cast<float>( tan( units::to_radians( 15_degrees ) ) ), false, false,   false},
    {static_cast<float>( tan( units::to_radians( 30_degrees ) ) ), false, false,   false},
    {static_cast<float>( -tan( units::to_radians( 45_degrees ) ) ), true,  false, true}, //45 degrees
    {static_cast<float>( -tan( units::to_radians( 30_degrees ) ) ), true,  false, true},
    {static_cast<float>( -tan( units::to_radians( 15_degrees ) ) ), true,  false, true},
    {static_cast<float>( tan( units::to_radians( 0_degrees ) ) ),  true,  false,   true}, //90 degrees
    {static_cast<float>( tan( units::to_radians( 15_degrees ) ) ), true,  false,   true},
    {static_cast<float>( tan( units::to_radians( 30_degrees ) ) ), true,  false,   true},
    {static_cast<float>( tan( units::to_radians( 45_degrees ) ) ), true,  false,   true}, //135 degrees
    {static_cast<float>( -tan( units::to_radians( 30_degrees ) ) ), true,  true,  false},
    {static_cast<float>( -tan( units::to_radians( 15_degrees ) ) ), true,  true,  false},
    {static_cast<float>( tan( units::to_radians( 0_degrees ) ) ),  true,  true,    false}, //180 degrees
    {static_cast<float>( tan( units::to_radians( 15_degrees ) ) ), true,  true,    false},
    {static_cast<float>( tan( units::to_radians( 30_degrees ) ) ), true,  true,    false},
    {static_cast<float>( -tan( units::to_radians( 45_degrees ) ) ), false, true,  true}, //225 degrees
    {static_cast<float>( -tan( units::to_radians( 30_degrees ) ) ), false, true,  true},
    {static_cast<float>( -tan( units::to_radians( 15_degrees ) ) ), false, true,  true},
    {static_cast<float>( tan( units::to_radians( 0_degrees ) ) ),  false,  true,   true}, //270 degrees
    {static_cast<float>( tan( units::to_radians( 15_degrees ) ) ), false,  true,   true},
    {static_cast<float>( tan( units::to_radians( 30_degrees ) ) ), false,  true,   true},
    {static_cast<float>( tan( units::to_radians( 45_degrees ) ) ), false,  true,   true}, //315 degrees
    {static_cast<float>( -tan( units::to_radians( 30_degrees ) ) ), false,  false, false},
    {static_cast<float>( -tan( units::to_radians( 15_degrees ) ) ), false,  false, false},
};

void vehicle::coord_translate( units::angle dir, point pivot, point p,
                               tripoint &q ) const
{

    int increment = angle_to_increment( dir );
    point relative = p - pivot;
    float skew = std::trunc( relative.x * rotation_info[increment].gradient );

    q.x = relative.x;
    q.y = relative.y + skew;

    if( rotation_info[increment].swapXY ) {
        auto swap = q.x;
        q.x = q.y;
        q.y = swap;
    }
    if( rotation_info[increment].flipH ) {
        q.x = -q.x;
    }
    if( rotation_info[increment].flipV ) {
        q.y = -q.y;
    }
}

void vehicle::coord_translate_reverse( units::angle dir, point pivot, const tripoint &p,
                                       point &q ) const
{
    int increment = angle_to_increment( dir );

    q.x = p.x;
    q.y = p.y;


    if( rotation_info[increment].flipV ) {
        q.y = -q.y;
    }

    if( rotation_info[increment].flipH ) {
        q.x = -q.x;
    }

    if( rotation_info[increment].swapXY ) {
        auto swap = q.x;
        q.x = q.y;
        q.y = swap;
    }

    float skew = std::trunc( q.x * rotation_info[increment].gradient );

    q.y -= skew;

    q += pivot;

}

tripoint vehicle::mount_to_tripoint( point mount ) const
{
    return mount_to_tripoint( mount, point_zero );
}

tripoint vehicle::mount_to_tripoint( point mount, point offset ) const
{
    tripoint mnt_translated;
    coord_translate( pivot_rotation[0], pivot_anchor[ 0 ], mount + offset, mnt_translated );
    return global_pos3() + mnt_translated;
}

point vehicle::tripoint_to_mount( const tripoint &p ) const
{
    tripoint translated = p - global_pos3();

    point result;
    coord_translate_reverse( pivot_rotation[0], pivot_anchor[0], translated, result );

    return result;
}

int vehicle::angle_to_increment( units::angle dir )
{
    int increment = ( std::lround( to_degrees( dir ) ) % 360 ) / 15;
    if( increment < 0 ) {
        increment += 360 / 15;
    }
    return increment;
}

void vehicle::precalc_mounts( int idir, units::angle dir, point pivot )
{
    if( idir < 0 || idir > 1 ) {
        idir = 0;
    }
    std::unordered_map<point, point> mount_to_precalc;
    for( auto &p : parts ) {
        if( p.removed ) {
            continue;
        }
        auto q = mount_to_precalc.find( p.mount );
        if( q == mount_to_precalc.end() ) {
            coord_translate( dir, pivot, p.mount, p.precalc[idir] );
            p.precalc[idir].z = 0;
            mount_to_precalc.insert( { p.mount, p.precalc[idir].xy() } );
        } else {
            p.precalc[idir] = {q->second, 0};
        }
    }
    pivot_anchor[idir] = pivot;
    pivot_rotation[idir] = dir;
}

bool vehicle::check_rotated_intervening( point from, point to,
        bool( *check )( const vehicle *, point ) ) const
{
    point delta = to - from;
    if( abs( delta.x ) <= 1 && abs( delta.y ) <= 1 ) { //Just a normal move
        return true;
    }

    if( !( ( abs( delta.x ) == 2 && abs( delta.y ) == 1 ) || ( abs( delta.x ) == 1 &&
            abs( delta.y ) == 2 ) ) ) { //Check that we're moving like a knight
        debugmsg( "Unexpected movement in rotated vehicle vector:%d,%d", delta.x, delta.y );
        return false;
    }

    if( abs( delta.x ) == 2 ) { //Mostly horizontal move
        point t1 = from + point( delta.x / 2, delta.y );
        if( check( this, t1 ) ) {
            return true;
        }

        point t2 = from + point( delta.x / 2, 0 );
        if( check( this, t2 ) ) {
            return true;
        }

    } else { //Mostly vertical move
        point t1 = from + point( delta.x, delta.y / 2 );
        if( check( this, t1 ) ) {
            return true;
        }

        point t2 = from + point( 0, delta.y / 2 );
        if( check( this, t2 ) ) {
            return true;
        }
    }

    return false;
}

bool vehicle::allowed_light( point from, point to ) const
{
    return check_rotated_intervening( from, to, []( const vehicle * veh, point  p ) {
        return ( veh->opaque_at_position( p ) == -1 );
    } );
}

bool vehicle::allowed_move( point from, point to ) const
{
    return check_rotated_intervening( from, to, []( const vehicle * veh, point  p ) {
        return ( veh->obstacle_at_position( p ) == -1 );
    } );
}

std::vector<int> vehicle::boarded_parts() const
{
    std::vector<int> res;
    for( const vpart_reference &vp : get_avail_parts( VPFLAG_BOARDABLE ) ) {
        if( vp.part().has_flag( vehicle_part::passenger_flag ) ) {
            res.push_back( static_cast<int>( vp.part_index() ) );
        }
    }
    return res;
}

std::vector<rider_data> vehicle::get_riders() const
{
    std::vector<rider_data> res;
    for( const vpart_reference &vp : get_avail_parts( VPFLAG_BOARDABLE ) ) {
        Creature *rider = g->critter_at( vp.pos() );
        if( rider ) {
            rider_data r;
            r.prt = vp.part_index();
            r.psg = rider;
            res.emplace_back( r );
        }
    }
    return res;
}

player *vehicle::get_passenger( int p ) const
{
    p = part_with_feature( p, VPFLAG_BOARDABLE, false );
    if( p >= 0 && parts[p].has_flag( vehicle_part::passenger_flag ) ) {
        return g->critter_by_id<player>( parts[p].passenger_id );
    }
    return nullptr;
}

monster *vehicle::get_pet( int p ) const
{
    p = part_with_feature( p, VPFLAG_BOARDABLE, false );
    if( p >= 0 ) {
        return g->critter_at<monster>( global_part_pos3( p ), true );
    }
    return nullptr;
}

tripoint_abs_ms vehicle::global_square_location() const
{
    return tripoint_abs_ms( get_map().getabs( global_pos3() ) );
}

tripoint_abs_omt vehicle::global_omt_location() const
{
    return project_to<coords::omt>( global_square_location() );
}

tripoint vehicle::global_pos3() const
{
    return sm_to_ms_copy( sm_pos ) + pos;
}

tripoint vehicle::global_part_pos3( const int &index ) const
{
    return global_part_pos3( parts[ index ] );
}

tripoint vehicle::global_part_pos3( const vehicle_part &pt ) const
{
    return global_pos3() + pt.precalc[ 0 ];
}

void vehicle::set_submap_moved( const tripoint &p )
{
    const point_abs_ms old_msp = global_square_location().xy();
    sm_pos = p;
    if( !tracking_on ) {
        return;
    }
    overmap_buffer.move_vehicle( this, old_msp );
}

units::mass vehicle::total_mass() const
{
    if( mass_dirty ) {
        refresh_mass();
    }

    return mass_cache;
}

units::volume vehicle::total_folded_volume() const
{
    units::volume m = 0_ml;
    for( const vpart_reference &vp : get_all_parts() ) {
        if( vp.part().removed ) {
            continue;
        }
        m += vp.info().folded_volume;
    }
    return m;
}

point vehicle::rotated_center_of_mass() const
{
    // TODO: Bring back caching of this point
    calc_mass_center( true );

    return mass_center_precalc;
}

point vehicle::local_center_of_mass() const
{
    if( mass_center_no_precalc_dirty ) {
        calc_mass_center( false );
    }

    return mass_center_no_precalc;
}

point vehicle::pivot_displacement() const
{
    // precalc_mounts always produces a result that puts the pivot point at (0,0).
    // If the pivot point changes, this artificially moves the vehicle, as the position
    // of the old pivot point will appear to move from (posx+0, posy+0) to some other point
    // (posx+dx,posy+dy) even if there is no change in vehicle position or rotation.
    // This method finds that movement so it can be canceled out when actually moving
    // the vehicle.

    // rotate the old pivot point around the new pivot point with the old rotation angle
    tripoint dp;
    coord_translate( pivot_rotation[0], pivot_anchor[1], pivot_anchor[0], dp );
    return dp.xy();
}

int vehicle::fuel_left( const itype_id &ftype, bool recurse ) const
{
    int fl = std::accumulate( parts.begin(), parts.end(), 0, [&ftype]( const int &lhs,
    const vehicle_part & rhs ) {
        return lhs + ( rhs.ammo_current() == ftype ? rhs.ammo_remaining() : 0 );
    } );

    if( recurse && ftype == fuel_type_battery ) {
        using tvr = distribution_graph::traverse_visitor_result;
        auto fuel_counting_visitor = [&fl, &ftype]( vehicle const & veh ) {
            fl += veh.fuel_left( ftype, false );
            return tvr::continue_further;
        };
        auto power_counting_visitor = [&fl]( distribution_grid const & grid ) {
            fl += grid.get_resource( false );
            return tvr::continue_further;
        };

        distribution_graph::traverse( *this, fuel_counting_visitor, power_counting_visitor );
    }

    //muscle engines have infinite fuel
    if( ftype == fuel_type_muscle ) {
        // TODO: Allow NPCs to power those
        const optional_vpart_position vp = g->m.veh_at( g->u.pos() );
        bool player_controlling = player_in_control( g->u );

        //if the engine in the player tile is a muscle engine, and player is controlling vehicle
        if( vp && &vp->vehicle() == this && player_controlling ) {
            const int p = avail_part_with_feature( vp->part_index(), VPFLAG_ENGINE, true );
            if( p >= 0 && is_part_on( p ) && part_info( p ).fuel_type == fuel_type_muscle ) {
                //Broken limbs prevent muscle engines from working
                if( ( part_info( p ).has_flag( "MUSCLE_LEGS" ) && ( g->u.get_working_leg_count() >= 2 ) ) ||
                    ( part_info( p ).has_flag( "MUSCLE_ARMS" ) &&
                      ( g->u.get_working_arm_count() >= 2 ) ) ) {
                    fl += 10;
                }
            }
        }
        // As do any other engine flagged as perpetual
        //TODO!: push up
    } else if( item::spawn_temporary( ftype )->has_flag( flag_PERPETUAL ) ) {
        fl += 10;
    }

    return fl;
}
int vehicle::fuel_left( const int p, bool recurse ) const
{
    return fuel_left( parts[ p ].fuel_current(), recurse );
}

int vehicle::engine_fuel_left( const int e, bool recurse ) const
{
    if( static_cast<size_t>( e ) < engines.size() ) {
        return fuel_left( parts[ engines[ e ] ].fuel_current(), recurse );
    }
    return 0;
}

int vehicle::fuel_capacity( const itype_id &ftype ) const
{
    return std::accumulate( parts.begin(), parts.end(), 0, [&ftype]( const int &lhs,
    const vehicle_part & rhs ) {
        return lhs + ( rhs.ammo_current() == ftype ? rhs.ammo_capacity() : 0 );
    } );
}

int vehicle::drain( const itype_id &ftype, int amount )
{
    if( ftype == fuel_type_battery ) {
        // Batteries get special handling to take advantage of jumper
        // cables -- discharge_battery knows how to recurse properly
        // (including taking cable power loss into account).
        int remnant = discharge_battery( amount, true );

        // discharge_battery returns amount of charges that were not
        // found anywhere in the power network, whereas this function
        // returns amount of charges consumed; simple subtraction.
        return amount - remnant;
    }

    int drained = 0;
    for( auto &p : parts ) {
        if( amount <= 0 ) {
            break;
        }
        if( p.ammo_current() == ftype ) {
            int qty = p.ammo_consume( amount, global_part_pos3( p ) );
            drained += qty;
            amount -= qty;
        }
    }

    invalidate_mass();
    return drained;
}

int vehicle::drain( const int index, int amount )
{
    if( index < 0 || index >= static_cast<int>( parts.size() ) ) {
        debugmsg( "Tried to drain an invalid part index: %d", index );
        return 0;
    }
    vehicle_part &pt = parts[index];
    if( pt.ammo_current() == fuel_type_battery ) {
        return drain( fuel_type_battery, amount );
    }
    if( !pt.is_tank() || !pt.ammo_remaining() ) {
        debugmsg( "Tried to drain something without any liquid: %s amount: %d ammo: %d",
                  pt.name(), amount, pt.ammo_remaining() );
        return 0;
    }

    const int drained = pt.ammo_consume( amount, global_part_pos3( pt ) );
    invalidate_mass();
    return drained;
}

int vehicle::basic_consumption( const itype_id &ftype ) const
{
    int fcon = 0;
    for( size_t e = 0; e < engines.size(); ++e ) {
        if( is_engine_type_on( e, ftype ) ) {
            if( parts[ engines[e] ].ammo_current() == fuel_type_battery &&
                part_epower_w( engines[e] ) >= 0 ) {
                // Electric engine - use epower instead
                fcon -= part_epower_w( engines[e] );

            } else if( !is_perpetual_type( e ) ) {
                fcon += part_vpower_w( engines[e] );
                if( parts[ e ].faults().contains( fault_filter_air ) ) {
                    fcon *= 2;
                }
            }
        }
    }
    return fcon;
}

int vehicle::consumption_per_hour( const itype_id &ftype, int fuel_rate_w ) const
{
    item &fuel = *item::spawn_temporary( ftype );
    if( fuel_rate_w == 0 || fuel.has_flag( flag_PERPETUAL ) || !engine_on ) {
        return 0;
    }

    float j_per_turn = fuel_rate_w;
    float j_per_second = j_per_turn / to_seconds<float>( time_duration::from_turns( 1 ) );
    float kj_per_hour = j_per_second * 3.6f;
    float kj_per_mL = fuel.fuel_energy();

    return kj_per_hour / kj_per_mL;
}

int vehicle::ideal_engine_power( bool safe ) const
{
    int pwr = 0;
    int cnt = 0;

    for( size_t e = 0; e < engines.size(); e++ ) {
        if( is_engine_on( e ) ) {
            int p = engines[e];
            int m2c = safe ? part_info( engines[e] ).engine_m2c() : 100;
            if( parts[ engines[e] ].faults().contains( fault_filter_fuel ) ) {
                m2c *= 0.6;
            }
            pwr += part_vpower_w( p ) * m2c / 100;
            cnt += static_cast<int>( !part_info( p ).has_flag( "E_NO_POWER_DECAY" ) );
        }
    }

    for( size_t a = 0; a < alternators.size(); a++ ) {
        int p = alternators[a];
        if( is_alternator_on( a ) ) {
            pwr += part_vpower_w( p ); // alternators have negative power
        }
    }

    pwr = std::max( 0, pwr );

    if( cnt > 1 ) {
        pwr = pwr * 4 / ( 4 + cnt - 1 );
    }
    return pwr;
}

int vehicle::total_power_w( const bool fueled, const bool safe ) const
{
    int pwr = 0;
    int cnt = 0;

    for( size_t e = 0; e < engines.size(); e++ ) {
        int p = engines[e];
        if( engine_on && is_engine_on( e ) && ( !fueled || engine_fuel_left( e ) ) ) {
            int m2c = safe ? part_info( engines[e] ).engine_m2c() : 100;
            if( parts[ engines[e] ].faults().contains( fault_filter_fuel ) ) {
                m2c *= 0.6;
            }
            pwr += part_vpower_w( p ) * m2c / 100;
            cnt += static_cast<int>( !part_info( p ).has_flag( "E_NO_POWER_DECAY" ) );
        }
    }

    for( size_t a = 0; a < alternators.size(); a++ ) {
        int p = alternators[a];
        if( is_alternator_on( a ) ) {
            pwr += part_vpower_w( p ); // alternators have negative power
        }
    }

    pwr = std::max( 0, pwr );

    if( cnt > 1 ) {
        pwr = pwr * 4 / ( 4 + cnt - 1 );
    }
    return pwr;
}

bool vehicle::is_moving() const
{
    return velocity != 0;
}

bool vehicle::can_use_rails() const
{
    // Do not allow vehicles without rail wheels or with mixed wheels
    return !rail_wheelcache.empty() && wheelcache.size() == rail_wheelcache.size();
}

int vehicle::ground_acceleration( const bool fueled, int at_vel_in_vmi, const bool ideal ) const
{
    if( !( engine_on || skidding ) ) {
        return 0;
    }
    int target_vmiph = std::max( at_vel_in_vmi, std::max( 1000, max_velocity( fueled ) / 4 ) );
    int cmps = vmiph_to_cmps( target_vmiph );
    double weight = to_kilogram( total_mass() );
    if( is_towing() ) {
        vehicle *other_veh = tow_data.get_towed();
        if( other_veh ) {
            weight = weight + to_kilogram( other_veh->total_mass() );
        }
    }
    int engine_power_ratio = ( ideal ? ideal_engine_power() : total_power_w( fueled ) ) / weight;
    int accel_at_vel = 100 * 100 * engine_power_ratio / cmps;
    add_msg( m_debug, "%s: accel at %d vimph is %d", name, target_vmiph,
             cmps_to_vmiph( accel_at_vel ) );
    return cmps_to_vmiph( accel_at_vel );
}

int vehicle::aircraft_acceleration( const bool fueled, int at_vel_in_vmi, const bool ideal ) const
{
    ( void )at_vel_in_vmi;
    if( !( engine_on || is_flying ) ) {
        return 0;
    }
    const double thrust = total_thrust( fueled, ideal );
    if( thrust == 0 ) {
        return 0;
    }
    const int accel_at_vel = 100 * total_thrust( fueled, ideal ) / to_kilogram( total_mass() );
    return cmps_to_vmiph( accel_at_vel );
}

int vehicle::water_acceleration( const bool fueled, int at_vel_in_vmi, const bool ideal ) const
{
    if( !( engine_on || skidding ) ) {
        return 0;
    }
    int target_vmiph = std::max( at_vel_in_vmi, std::max( 1000,
                                 max_water_velocity( fueled ) / 4 ) );
    int cmps = vmiph_to_cmps( target_vmiph );
    double weight = to_kilogram( total_mass() );
    if( is_towing() ) {
        vehicle *other_veh = tow_data.get_towed();
        if( other_veh ) {
            weight = weight + to_kilogram( other_veh->total_mass() );
        }
    }
    int engine_power_ratio = ( ideal ? ideal_engine_power() : total_power_w( fueled ) ) / weight;
    int accel_at_vel = 100 * 100 * engine_power_ratio / cmps;
    add_msg( m_debug, "%s: water accel at %d vimph is %d", name, target_vmiph,
             cmps_to_vmiph( accel_at_vel ) );
    return cmps_to_vmiph( accel_at_vel );
}

// cubic equation solution
// don't use complex numbers unless necessary and it's usually not
// see https://math.vanderbilt.edu/schectex/courses/cubic/ for the gory details
static double simple_cubic_solution( double a, double b, double c, double d )
{
    double p = -b / ( 3 * a );
    double q = p * p * p + ( b * c - 3 * a * d ) / ( 6 * a * a );
    double r = c / ( 3 * a );
    double t = r - p * p;
    double tricky_bit = q * q + t * t * t;
    if( tricky_bit < 0 ) {
        double cr = 1.0 / 3.0; // approximate the cube root of a complex number
        std::complex<double> q_complex( q );
        std::complex<double> tricky_complex( std::sqrt( std::complex<double>( tricky_bit ) ) );
        std::complex<double> term1( std::pow( q_complex + tricky_complex, cr ) );
        std::complex<double> term2( std::pow( q_complex - tricky_complex, cr ) );
        std::complex<double> term_sum( term1 + term2 );

        if( imag( term_sum ) < 2 ) {
            return p + real( term_sum );
        } else {
            debugmsg( "cubic solution returned imaginary values" );
            return 0;
        }
    } else {
        double tricky_final = std::sqrt( tricky_bit );
        double term1_part = q + tricky_final;
        double term2_part = q - tricky_final;
        double term1 = std::cbrt( term1_part );
        double term2 = std::cbrt( term2_part );
        return p + term1 + term2;
    }
}

int vehicle::acceleration( const bool fueled, int at_vel_in_vmi ) const
{
    if( is_watercraft() ) {
        return water_acceleration( fueled, at_vel_in_vmi );
    } else if( is_aircraft() && is_flying ) {
        return aircraft_acceleration( fueled, at_vel_in_vmi );
    }
    return ground_acceleration( fueled, at_vel_in_vmi );
}

int vehicle::current_acceleration( const bool fueled ) const
{
    return acceleration( fueled, std::abs( velocity ) );
}

// Ugly physics below:
// maximum speed occurs when all available thrust is used to overcome air/rolling resistance
// sigma F = 0 as we were taught in Engineering Mechanics 301
// engine power is torque * rotation rate (in rads for simplicity)
// torque / wheel radius = drive force at where the wheel meets the road
// velocity is wheel radius * rotation rate (in rads for simplicity)
// air resistance is -1/2 * air density * drag coeff * cross area * v^2
//        and c_air_drag is -1/2 * air density * drag coeff * cross area
// rolling resistance is mass * GRAVITY_OF_EARTH * rolling coeff * 0.000225 * ( 33.3 + v )
//        and c_rolling_drag is mass * GRAVITY_OF_EARTH * rolling coeff * 0.000225
//        and rolling_constant_to_variable is 33.3
// or by formula:
// max velocity occurs when F_drag = F_wheel
// F_wheel = engine_power / rotation_rate / wheel_radius
// velocity = rotation_rate * wheel_radius
// F_wheel * velocity = engine_power * rotation_rate * wheel_radius / rotation_rate / wheel_radius
// F_wheel * velocity = engine_power
// F_wheel = engine_power / velocity
// F_drag = F_air_drag + F_rolling_drag
// F_air_drag = c_air_drag * velocity^2
// F_rolling_drag = c_rolling_drag * velocity + rolling_constant_to_variable * c_rolling_drag
// engine_power / v = c_air_drag * v^2 + c_rolling_drag * v + 33 * c_rolling_drag
// c_air_drag * v^3 + c_rolling_drag * v^2 + c_rolling_drag * 33.3 * v - engine power = 0
// solve for v with the simplified cubic equation solver
// got it? quiz on Wednesday.
int vehicle::max_ground_velocity( const bool fueled, const bool ideal ) const
{
    int total_engine_w = ( ideal ? ideal_engine_power() : total_power_w( fueled ) );
    double c_rolling_drag = coeff_rolling_drag();
    double max_in_mps = simple_cubic_solution( coeff_air_drag(), c_rolling_drag,
                        c_rolling_drag * vehicles::rolling_constant_to_variable,
                        -total_engine_w );
    add_msg( m_debug, "%s: power %d, c_air %3.2f, c_rolling %3.2f, max_in_mps %3.2f",
             name, total_engine_w, coeff_air_drag(), c_rolling_drag, max_in_mps );
    return mps_to_vmiph( max_in_mps );
}

// the same physics as ground velocity, but there's no rolling resistance so the math is easy
// F_drag = F_water_drag + F_air_drag
// F_drag = c_water_drag * velocity^2 + c_air_drag * velocity^2
// F_drag = ( c_water_drag + c_air_drag ) * velocity^2
// F_prop = engine_power / velocity
// F_prop = F_drag
// engine_power / velocity = ( c_water_drag + c_air_drag ) * velocity^2
// engine_power = ( c_water_drag + c_air_drag ) * velocity^3
// velocity^3 = engine_power / ( c_water_drag + c_air_drag )
// velocity = cube root( engine_power / ( c_water_drag + c_air_drag ) )
int vehicle::max_water_velocity( const bool fueled, const bool ideal ) const
{
    int total_engine_w = ( ideal ? ideal_engine_power() : total_power_w( fueled ) );
    double total_drag = coeff_water_drag() + coeff_air_drag();
    double max_in_mps = std::cbrt( total_engine_w / total_drag );
    add_msg( m_debug, "%s: power %d, c_air %3.2f, c_water %3.2f, water max_in_mps %3.2f",
             name, total_engine_w, coeff_air_drag(), coeff_water_drag(), max_in_mps );
    return mps_to_vmiph( max_in_mps );
}

int vehicle::max_air_velocity( const bool fueled, const bool ideal ) const
{
    const double max_air_mps = std::sqrt( total_thrust( fueled, false, ideal ) / coeff_air_drag() );
    // fly fast at your own risk
    return mps_to_vmiph( max_air_mps );
}

int vehicle::max_velocity( const bool fueled, const bool ideal ) const
{
    if( is_flying && is_aircraft() ) {
        return max_air_velocity( fueled, ideal );
    } else if( is_watercraft() ) {
        return max_water_velocity( fueled, ideal );
    } else {
        return max_ground_velocity( fueled, ideal );
    }
}

int vehicle::max_reverse_velocity( const bool fueled, const bool ideal ) const
{
    int max_vel = max_velocity( fueled, ideal );
    if( has_engine_type( fuel_type_battery, true ) ) {
        // Electric motors can go in reverse as well as forward
        return -max_vel;
    } else {
        // All other motive powers do poorly in reverse
        return -max_vel / 4;
    }
}

// the same physics as max_ground_velocity, but with a smaller engine power
int vehicle::safe_ground_velocity( const bool fueled, const bool ideal ) const
{
    for( size_t e = 0; e < parts.size(); e++ ) {
        const vehicle_part &vp = parts[ e ];
        int animal_vel = 0;
        if( vp.info().fuel_type == fuel_type_animal && engines.size() != 1 ) {
            monster *mon = get_pet( e );
            if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
                int animal_vel_cur = mon->get_speed() * 12;
                if( animal_vel > 0 ) {
                    animal_vel = std::min( animal_vel, animal_vel_cur );
                } else {
                    animal_vel = animal_vel_cur;
                }
            }
        }
        // Cap safe speed at the point where the slowest animal found would start to damage their yoke
        // If damage or weight has pulled max speed lower than this, cap at that instead.
        if( animal_vel > 0 ) {
            return std::min( animal_vel, max_ground_velocity( fueled ) );
        }
    }
    int effective_engine_w = ( ideal ? ideal_engine_power( true ) : total_power_w( fueled, true ) );
    double c_rolling_drag = coeff_rolling_drag();
    double safe_in_mps = simple_cubic_solution( coeff_air_drag(), c_rolling_drag,
                         c_rolling_drag * vehicles::rolling_constant_to_variable,
                         -effective_engine_w );
    return mps_to_vmiph( safe_in_mps );
}

// TODO: Consider some kind of dynamic pressure based safe velocity
// or something simpler maybe
int vehicle::safe_aircraft_velocity( const bool fueled, const bool ideal ) const
{
    const double max_air_mps = std::sqrt( total_thrust( fueled,
                                          true, ideal ) / coeff_air_drag() );
    return std::min( 22501, mps_to_vmiph( max_air_mps ) );
}

// the same physics as max_water_velocity, but with a smaller engine power
int vehicle::safe_water_velocity( const bool fueled, const bool ideal ) const
{
    int total_engine_w = ( ideal ? ideal_engine_power( true ) : total_power_w( fueled, true ) );
    double total_drag = coeff_water_drag() + coeff_air_drag();
    double safe_in_mps = std::cbrt( total_engine_w / total_drag );
    return mps_to_vmiph( safe_in_mps );
}

int vehicle::safe_velocity( const bool fueled ) const
{
    if( is_flying && is_aircraft() ) {
        return safe_aircraft_velocity( fueled );
    } else if( is_watercraft() ) {
        return safe_water_velocity( fueled );
    } else {
        return safe_ground_velocity( fueled );
    }
}

bool vehicle::do_environmental_effects()
{
    bool needed = false;
    // check for smoking parts
    for( const vpart_reference &vp : get_all_parts() ) {
        /* Only lower blood level if:
         * - The part is outside.
         * - The weather is any effect that would cause the player to be wet. */
        if( vp.part().blood > 0 && g->m.is_outside( vp.pos() ) ) {
            needed = true;
            if( get_weather().weather_id->rains &&
                get_weather().weather_id->precip != precip_class::very_light ) {
                vp.part().blood--;
            }
        }
    }
    return needed;
}

void vehicle::spew_field( double joules, int part, field_type_id type, int intensity )
{
    if( rng( 1, 10000 ) > joules ) {
        return;
    }
    point p = parts[part].mount;
    intensity = std::max( joules / 10000, static_cast<double>( intensity ) );
    // Move back from engine/muffler until we find an open space
    while( relative_parts.contains( p ) ) {
        p.x += ( velocity < 0 ? 1 : -1 );
    }
    point q = coord_translate( p );
    const tripoint dest = global_pos3() + tripoint( q, 0 );
    g->m.mod_field_intensity( dest, type, intensity );
}

/**
 * Generate noise or smoke from a vehicle with engines turned on
 * load = how hard the engines are working, from 0.0 until 1.0
 * time = how many seconds to generated smoke for
 */
void vehicle::noise_and_smoke( int load, time_duration time )
{
    static const std::array<std::pair<std::string, int>, 8> sounds = { {
            { translate_marker( "hmm" ), 0 }, { translate_marker( "hummm!" ), 15 },
            { translate_marker( "whirrr!" ), 30 }, { translate_marker( "vroom!" ), 60 },
            { translate_marker( "roarrr!" ), 100 }, { translate_marker( "ROARRR!" ), 140 },
            { translate_marker( "BRRROARRR!" ), 180 }, { translate_marker( "BRUMBRUMBRUMBRUM!" ), INT_MAX }
        }
    };
    const std::string heli_noise = translate_marker( "WUMPWUMPWUMP!" );
    double noise = 0.0;
    double mufflesmoke = 0.0;
    double muffle = 1.0;
    double m = 0.0;
    int exhaust_part = -1;
    for( const vpart_reference &vp : get_avail_parts( "MUFFLER" ) ) {
        m = 1.0 - ( 1.0 - vp.info().bonus / 100.0 ) * vp.part().health_percent();
        if( m < muffle ) {
            muffle = m;
            exhaust_part = static_cast<int>( vp.part_index() );
        }
    }

    bool bad_filter = false;
    bool combustion = false;

    for( size_t e = 0; e < engines.size(); e++ ) {
        int p = engines[e];
        if( is_engine_on( e ) &&  engine_fuel_left( e ) ) {
            // convert current engine load to units of watts/40K
            // then spew more smoke and make more noise as the engine load increases
            int part_watts = part_vpower_w( p, true );
            double max_stress = static_cast<double>( part_watts / 40000.0 );
            double cur_stress = load / 1000.0 * max_stress;
            // idle stress = 1.0 resulting in nominal working engine noise = engine_noise_factor()
            // and preventing noise = 0
            cur_stress = std::max( cur_stress, 1.0 );
            double part_noise = cur_stress * part_info( p ).engine_noise_factor();

            if( part_info( p ).has_flag( "E_COMBUSTION" ) ) {
                combustion = true;
                double health = parts[p].health_percent();
                if( parts[ p ].base->faults.contains( fault_filter_fuel ) ) {
                    health = 0.0;
                }
                if( health < part_info( p ).engine_backfire_threshold() && one_in( 50 + 150 * health ) ) {
                    backfire( e );
                }
                double j = cur_stress * to_turns<int>( time ) * muffle * 1000;

                if( parts[ p ].base->faults.contains( fault_filter_air ) ) {
                    bad_filter = true;
                    j *= j;
                }

                if( ( exhaust_part == -1 ) && engine_on ) {
                    spew_field( j, p, fd_smoke, bad_filter ? fd_smoke.obj().get_max_intensity() : 1 );
                } else {
                    mufflesmoke += j;
                }
                part_noise = ( part_noise + max_stress * 3 + 5 ) * muffle;
            }
            noise = std::max( noise, part_noise ); // Only the loudest engine counts.
        }
    }
    if( !combustion ) {
        return;
    }
    /// TODO: handle other engine types: muscle / animal / wind / coal / ...

    if( exhaust_part != -1 && engine_on ) {
        spew_field( mufflesmoke, exhaust_part, fd_smoke,
                    bad_filter ? fd_smoke.obj().get_max_intensity() : 1 );
    }
    if( is_flying && has_part( VPFLAG_ROTOR ) ) {
        noise *= 2;
    }
    // Cap engine noise to avoid deafening.
    noise = std::min( noise, 100.0 );
    // Even a vehicle with engines off will make noise traveling at high speeds
    noise = std::max( noise, std::fabs( velocity / 500.0 ) );
    int lvl = 0;
    if( one_in( 4 ) && rng( 0, 30 ) < noise ) {
        while( noise > sounds[lvl].second ) {
            lvl++;
        }
    }
    add_msg( m_debug, "VEH NOISE final: %d", static_cast<int>( noise ) );
    vehicle_noise = static_cast<unsigned char>( noise );
    // TODO: other noises for non-rotor aircraft?
    sounds::sound( global_pos3(), noise, sounds::sound_t::movement,
                   _( has_part( VPFLAG_ROTOR ) ? heli_noise : sounds[lvl].first ), true );
}

int vehicle::wheel_area() const
{
    int total_area = 0;
    for( const int &wheel_index : wheelcache ) {
        total_area += parts[ wheel_index ].wheel_area();
    }

    return total_area;
}

float vehicle::average_or_rating() const
{
    if( wheelcache.empty() ) {
        return 0.0f;
    }
    float total_rating = 0;
    for( const int &wheel_index : wheelcache ) {
        total_rating += part_info( wheel_index ).wheel_or_rating();
    }
    return total_rating / wheelcache.size();
}

static double tile_to_width( int tiles )
{
    if( tiles < 1 ) {
        return 0.1;
    } else if( tiles < 6 ) {
        return 0.5 + 0.4 * tiles;
    } else {
        return 2.5 + 0.15 * ( tiles - 5 );
    }
}

static constexpr int minrow = -122;
static constexpr int maxrow = 122;
struct drag_column {
    int pro = minrow;
    int hboard = minrow;
    int fboard = minrow;
    int aisle = minrow;
    int seat = minrow;
    int exposed = minrow;
    int roof = minrow;
    int shield = minrow;
    int turret = minrow;
    int panel = minrow;
    int windmill = minrow;
    int sail = minrow;
    int rotor = minrow;
    int ballon = minrow;
    int last = maxrow;
};

double vehicle::coeff_air_drag() const
{
    if( !coeff_air_dirty ) {
        return coefficient_air_resistance;
    }
    constexpr double c_air_base = 0.25;
    constexpr double c_air_mod = 0.1;
    constexpr double base_height = 1.4;
    constexpr double aisle_height = 0.6;
    constexpr double fullboard_height = 0.5;
    constexpr double roof_height = 0.1;
    constexpr double windmill_height = 0.7;
    constexpr double sail_height = 0.8;
    constexpr double rotor_height = 0.6;

    std::vector<int> structure_indices = all_parts_at_location( part_location_structure );
    int width = mount_max.y - mount_min.y + 1;

    // a mess of lambdas to make the next bit slightly easier to read
    const auto d_exposed = [&]( const vehicle_part & p ) {
        // if it's not inside, it's a center location, and it doesn't need a roof, it's exposed
        if( p.info().location != part_location_center ) {
            return false;
        }
        return !( p.inside || p.info().has_flag( "EXTENDABLE" ) ||
                  p.info().has_flag( "NO_ROOF_NEEDED" ) ||
                  p.info().has_flag( "WINDSHIELD" ) ||
                  p.info().has_flag( "OPENABLE" ) );
    };

    const auto d_protrusion = [&]( std::vector<int> parts_at ) {
        if( parts_at.size() > 1 ) {
            return false;
        } else {
            return parts[ parts_at.front() ].info().has_flag( "PROTRUSION" );
        }
    };
    const auto d_check_min = [&]( int &value, const vehicle_part & p, bool test ) {
        value = std::min( value, test ? p.mount.x - mount_min.x : maxrow );
    };
    const auto d_check_max = [&]( int &value, const vehicle_part & p, bool test ) {
        value = std::max( value, test ? p.mount.x - mount_min.x : minrow );
    };

    // raycast down each column. the least drag vehicle has halfboard, windshield, seat with roof,
    // windshield, halfboard and is twice as long as it is wide.
    // find the first instance of each item and compare against the ideal configuration.
    std::vector<drag_column> drag( width );
    for( int p : structure_indices ) {
        if( parts[ p ].removed ) {
            continue;
        }
        int col = parts[ p ].mount.y - mount_min.y;
        std::vector<int> parts_at = parts_at_relative( parts[ p ].mount, true );
        d_check_min( drag[ col ].pro, parts[ p ], d_protrusion( parts_at ) );
        for( int pa_index : parts_at ) {
            const vehicle_part &pa = parts[ pa_index ];
            d_check_max( drag[ col ].hboard, pa, pa.info().has_flag( "HALF_BOARD" ) );
            d_check_max( drag[ col ].fboard, pa, pa.info().has_flag( "FULL_BOARD" ) );
            d_check_max( drag[ col ].aisle, pa, pa.info().has_flag( "AISLE" ) );
            d_check_max( drag[ col ].shield, pa, pa.info().has_flag( "WINDSHIELD" ) &&
                         pa.is_available() );
            d_check_max( drag[ col ].seat, pa, pa.info().has_flag( "SEAT" ) ||
                         pa.info().has_flag( "BED" ) );
            d_check_max( drag[ col ].turret, pa, pa.info().location == part_location_onroof &&
                         !pa.info().has_flag( "SOLAR_PANEL" ) );
            d_check_max( drag[ col ].roof, pa, pa.info().has_flag( "ROOF" ) );
            d_check_max( drag[ col ].panel, pa, pa.info().has_flag( "SOLAR_PANEL" ) );
            d_check_max( drag[ col ].windmill, pa, pa.info().has_flag( "WIND_TURBINE" ) );
            d_check_max( drag[ col ].rotor, pa, pa.info().has_flag( "ROTOR" ) );
            d_check_max( drag[ col ].ballon, pa, pa.info().has_flag( "BALLOON" ) );
            d_check_max( drag[ col ].sail, pa, pa.info().has_flag( "WIND_POWERED" ) );
            d_check_max( drag[ col ].exposed, pa, d_exposed( pa ) );
            d_check_min( drag[ col ].last, pa, pa.info().has_flag( "LOW_FINAL_AIR_DRAG" ) ||
                         pa.info().has_flag( "HALF_BOARD" ) );
        }
    }
    double height = 0;
    double c_air_drag = 0;
    // tally the results of each row and prorate them relative to vehicle width
    for( drag_column &dc : drag ) {
        // even as m_debug you rarely want to see this
        // add_msg( m_debug, "veh %: pro %d, hboard %d, fboard %d, shield %d, seat %d, roof %d, aisle %d, turret %d, panel %d, exposed %d, last %d\n", name, dc.pro, dc.hboard, dc.fboard, dc.shield, dc.seat, dc.roof, dc.aisle, dc.turret, dc.panel, dc.exposed, dc.last );

        double c_air_drag_c = c_air_base;
        // rams in front of the vehicle mildly worsens air drag
        c_air_drag_c += ( dc.pro > dc.hboard ) ? c_air_mod : 0;
        // not having halfboards in front of any windshields or fullboards moderately worsens
        // air drag
        c_air_drag_c += ( std::max( std::max( dc.hboard, dc.fboard ),
                                    dc.shield ) != dc.hboard ) ? 2 * c_air_mod : 0;
        // not having windshields in front of seats severely worsens air drag
        c_air_drag_c += ( dc.shield < dc.seat ) ? 3 * c_air_mod : 0;
        // missing roofs and open doors severely worsen air drag
        c_air_drag_c += ( dc.exposed > minrow ) ? 3 * c_air_mod : 0;
        // being twice as long as wide mildly reduces air drag
        c_air_drag_c -= ( 2 * ( mount_max.x - mount_min.x ) > width ) ? c_air_mod : 0;
        // trunk doors and halfboards at the tail mildly reduce air drag
        c_air_drag_c -= ( dc.last == mount_min.x ) ? c_air_mod : 0;
        // turrets severely worsen air drag
        c_air_drag_c += ( dc.turret > minrow ) ? 3 * c_air_mod : 0;
        // having a windmill is terrible for your drag
        c_air_drag_c += ( dc.windmill > minrow ) ? 5 * c_air_mod : 0;
        // rotors are not great for drag!
        c_air_drag_c += ( dc.rotor > minrow ) ? 6 * c_air_mod : 0;
        // Neither are balloons
        c_air_drag_c += ( dc.ballon > minrow ) ? 6 * c_air_mod : 0;
        // having a sail is terrible for your drag
        c_air_drag_c += ( dc.sail > minrow ) ? 7 * c_air_mod : 0;
        c_air_drag += c_air_drag_c;
        // vehicles are 1.4m tall
        double c_height = base_height;
        // plus a bit for a roof
        c_height += ( dc.roof > minrow ) ? roof_height : 0;
        // plus a lot for an aisle
        c_height += ( dc.aisle > minrow ) ?  aisle_height : 0;
        // or fullboards
        c_height += ( dc.fboard > minrow ) ? fullboard_height : 0;
        // and a little for anything on the roof
        c_height += ( dc.turret > minrow ) ? 2 * roof_height : 0;
        // solar panels are better than turrets or floodlights, though
        c_height += ( dc.panel > minrow ) ? roof_height : 0;
        // windmills are tall, too
        c_height += ( dc.windmill > minrow ) ? windmill_height : 0;
        c_height += ( dc.rotor > minrow ) ? rotor_height : 0;
        // sails are tall, too
        c_height += ( dc.sail > minrow ) ? sail_height : 0;
        height += c_height;
    }
    constexpr double air_density = 1.29; // kg/m^3
    // prorate per row height and c_air_drag
    height /= width;
    c_air_drag /= width;
    double cross_area = height * tile_to_width( width );
    add_msg( m_debug, "%s: height %3.2fm, width %3.2fm (%d tiles), c_air %3.2f\n", name, height,
             tile_to_width( width ), width, c_air_drag );
    // F_air_drag = c_air_drag * cross_area * 1/2 * air_density * v^2
    // coeff_air_resistance = c_air_drag * cross_area * 1/2 * air_density
    coefficient_air_resistance = std::max( 0.1, c_air_drag * cross_area * 0.5 * air_density );
    coeff_air_dirty = false;
    return coefficient_air_resistance;
}

double vehicle::coeff_rolling_drag() const
{
    if( !coeff_rolling_dirty ) {
        return coefficient_rolling_resistance;
    }
    constexpr double wheel_ratio = 1.25;
    constexpr double base_wheels = 4.0;
    // SAE J2452 measurements are in F_rr = N * C_rr * 0.000225 * ( v + 33.33 )
    // Don't ask me why, but it's the numbers we have. We want N * C_rr * 0.000225 here,
    // and N is mass * accel from gravity (aka weight)
    constexpr double sae_ratio = 0.000225;
    constexpr double newton_ratio = GRAVITY_OF_EARTH * sae_ratio;
    double wheel_factor = 0;
    if( wheelcache.empty() ) {
        wheel_factor = 50;
    } else {
        // should really sum the each wheel's c_rolling_resistance * it's share of vehicle mass
        for( auto wheel : wheelcache ) {
            wheel_factor += parts[ wheel ].info().wheel_rolling_resistance();
        }
        // mildly increasing rolling resistance for vehicles with more than 4 wheels and mildly
        // decrease it for vehicles with less
        wheel_factor *= wheel_ratio /
                        ( base_wheels * wheel_ratio - base_wheels + wheelcache.size() );
    }
    coefficient_rolling_resistance = newton_ratio * wheel_factor * to_kilogram( total_mass() );
    coeff_rolling_dirty = false;
    return coefficient_rolling_resistance;
}

double vehicle::water_hull_height() const
{
    if( coeff_water_dirty ) {
        coeff_water_drag();
    }
    return hull_height;
}

double vehicle::water_draft() const
{
    if( coeff_water_dirty ) {
        coeff_water_drag();
    }
    return draft_m;
}

bool vehicle::can_float() const
{
    if( coeff_water_dirty ) {
        coeff_water_drag();
    }
    int float_force = max_buoyancy() + total_balloon_lift();
    return to_newton( total_mass() ) <= float_force;
}


double vehicle::total_rotor_area() const
{
    return std::accumulate( rotors.begin(), rotors.end(), 0.0,
    [&]( double acc, int rotor ) {
        const double radius{ parts[ rotor ].info().rotor_diameter() / 2.0 };
        return acc + M_PI * std::pow( radius, 2 );
    } );
}

double vehicle::total_propeller_area() const
{
    return std::accumulate( propellers.begin(), propellers.end(), 0.0,
    [&]( double acc, int propeller ) {
        const double radius{ parts[ propeller ].info().propeller_diameter() / 2.0 };
        return acc + M_PI * std::pow( radius, 2 );
    } );
}

// Balloons can lift ~1 kg per m^3
// 1 tile is 1 m^2, but with an undefined height
// So balloon height and balloon weight will be what changes
// Returns a value in newtons
double vehicle::total_balloon_lift() const
{
    return GRAVITY_OF_EARTH * std::accumulate( balloons.begin(), balloons.end(), double{0.0},
    [&]( double acc, int balloon ) {
        const double height{ parts[ balloon ].info().balloon_height() };
        return acc + height;
    } );
}

// Wing Lift
// Based on air being 1kg/m^3
double vehicle::total_wing_lift() const
{
    // Velocity is in mph * 100 (why oh why)
    // Convert to km/h then m/s then m/s ^ 2
    const double kilometerperhour = velocity / 100 * 1.609;
    const double meterpersec = kilometerperhour / 3600 * 1000;
    const double meterpersecsquared = std::pow( meterpersec, 2 );
    return meterpersecsquared * std::accumulate( wings.begin(), wings.end(), double{0.0},
    [&]( double acc, int wing ) {
        const double liftcoff{ parts[ wing ].info().lift_coff() };
        // m^2 area is always 1
        return acc + ( 0.5 * liftcoff );
    } );
}

// constants were converted from imperial to SI goodness
// returns as newton
double vehicle::thrust_of_rotorcraft( const bool fuelled, const bool safe, const bool ideal ) const
{
    constexpr double coefficient = 0.8642;
    constexpr double exponentiation = -0.3107;

    const double rotor_area = total_rotor_area();
    if( rotor_area == 0 ) {
        return 0;
    }
    // take off 15 % due to the imaginary tail rotor power?
    const int engine_power = ( ideal ? ideal_engine_power( safe ) : total_power_w( fuelled, safe ) );
    if( engine_power <= 0 ) {
        return 0;
    }
    const double power_load = engine_power / rotor_area;
    const double lift_thrust = coefficient * engine_power * std::pow( power_load, exponentiation );
    add_msg( m_debug, "lift thrust(N) of %s: %f, rotor area (m^2): %f, engine power (w): %i",
             name, lift_thrust, rotor_area, engine_power );
    return lift_thrust;
}

// constants were converted from imperial to SI goodness
// returns as newton
double vehicle::foward_thrust_of_propellers( const bool fuelled, const bool safe,
        const bool ideal ) const
{
    constexpr double coefficient = 0.8642;
    constexpr double exponentiation = -0.3107;

    const double rotor_area = total_propeller_area();
    if( rotor_area == 0 ) {
        return 0;
    }
    // take off 15 % due to the imaginary tail rotor power?
    const int engine_power = ( ideal ? ideal_engine_power( safe ) : total_power_w( fuelled, safe ) );

    const double power_load = engine_power / rotor_area;
    const double foward_thrust = coefficient * engine_power * std::pow( power_load, exponentiation );
    add_msg( m_debug, "foward thrust(N) of %s: %f, propeller area (m^2): %f, engine power (w): %i",
             name, foward_thrust, rotor_area, engine_power );
    return foward_thrust;
}

// get sum of horizontal thrust from all lifting parts
double vehicle::total_thrust( const bool fuelled, const bool safe, const bool ideal ) const
{
    return thrust_of_rotorcraft( fuelled, safe, ideal ) + foward_thrust_of_propellers( fuelled, safe,
            ideal );
}

// get sum of lift from all lifting parts
double vehicle::total_lift( const bool fuelled, const bool safe, const bool ideal ) const
{
    return thrust_of_rotorcraft( fuelled, safe, ideal ) + total_balloon_lift() + total_wing_lift();
}

int vehicle::get_takeoff_speed() const
{
    const int needed_force = to_newton( total_mass() ) - thrust_of_rotorcraft( true, false, true ) -
                             total_balloon_lift();

    const double liftwithoutspeed = std::accumulate( wings.begin(), wings.end(), double{0.0},
    [&]( double acc, int wing ) {
        const double liftcoff{ parts[ wing ].info().lift_coff() };
        // m^2 area is always 1
        return acc + ( 0.5 * liftcoff );
    } );
    const double needed_met_sec_squared = needed_force / liftwithoutspeed;
    const double needed_met_sec = std::sqrt( needed_met_sec_squared );
    const double needed_km_hour = needed_met_sec / 1000 * 3600;
    std::string speed_type = get_option<std::string>( "USE_METRIC_SPEEDS" );
    if( speed_type == "km/h" ) {
        return ceil( needed_km_hour );
    } else if( speed_type == "mph" ) {
        return ceil( needed_km_hour / 1.609 );
    } else if( speed_type == "t/t" ) {
        return ceil( needed_km_hour / 1.609 / 4 );
    } else {
        return INT_MAX;
    }
}
// For some reason, just checking total lift > 0 doesn't seem to work if the vehicle hasn't been piloted before, which was impacting the design view. This fixes it, and can be used to check if the vehicle HAS lift, but not enough to fly by doing has_lift && !has_sufficient_lift
bool vehicle::has_lift() const
{
    return has_part( VPFLAG_ROTOR ) || has_part( VPFLAG_BALLOON ) || has_part( VPFLAG_WING );
}

bool vehicle::has_sufficient_lift() const
{
    return total_lift( true ) > to_newton( total_mass() );
}

bool vehicle::is_rotorcraft() const
{
    return ( has_part( VPFLAG_ROTOR ) ) && has_sufficient_lift();
}
// requires vehicle to have sufficient rotor lift
bool vehicle::is_aircraft() const
{
    return ( has_part( VPFLAG_ROTOR ) || has_part( VPFLAG_WING ) || has_part( VPFLAG_BALLOON ) )
           && has_sufficient_lift();
}

int vehicle::get_z_change() const
{
    return requested_z_change;
}

bool vehicle::is_flying_in_air() const
{
    return is_flying;
}

void vehicle::set_flying( bool new_flying_value )
{
    is_flying = new_flying_value;
}

bool vehicle::is_watercraft() const
{
    return is_floating || ( in_water && wheelcache.empty() );
}

bool vehicle::is_in_water( bool deep_water ) const
{
    return deep_water ? is_floating : in_water;
}

static constexpr double water_density = 1000.0; // kg/m^3

double vehicle::coeff_water_drag() const
{
    if( !coeff_water_dirty ) {
        return coefficient_water_resistance;
    }
    std::vector<int> structure_indices = all_parts_at_location( part_location_structure );
    if( structure_indices.empty() ) {
        // huh?
        coeff_water_dirty = false;
        hull_height = 0.3;
        draft_m = 1.0;
        return 1250.0;
    }
    double hull_coverage = static_cast<double>( floating.size() ) / structure_indices.size();

    int tile_width = mount_max.y - mount_min.y + 1;
    double width_m = tile_to_width( tile_width );

    // actual area of the hull in m^2 (handles non-rectangular shapes)
    // footprint area in tiles = tile width * tile length
    // effective footprint percent = # of structure tiles / footprint area in tiles
    // actual hull area in m^2 = footprint percent * length in meters * width in meters
    // length in meters = length in tiles
    // actual area in m = # of structure tiles * length in tiles * width in meters /
    //                    ( length in tiles * width in tiles )
    // actual area in m = # of structure tiles * width in meters / width in tiles
    double actual_area_m = width_m * structure_indices.size() / tile_width;

    // effective hull area is actual hull area * hull coverage
    hull_area = actual_area_m * std::max( 0.1, hull_coverage );
    // Treat the hullform as a simple cuboid to calculate displaced depth of
    // water.
    // Apply Archimedes' principle (mass of water displaced is mass of vehicle).
    // area * depth = hull_volume = water_mass / water density
    // water_mass = vehicle_mass
    // area * depth = vehicle_mass / water_density
    // depth = vehicle_mass / water_density / area
    draft_m = to_kilogram( total_mass() ) / water_density / hull_area;
    // increase the streamlining as more of the boat is covered in boat boards
    double c_water_drag = 1.25 - hull_coverage;
    // hull height starts at 0.3m and goes up as you add more boat boards
    hull_height = 0.3 + 0.5 * hull_coverage;
    // F_water_drag = c_water_drag * cross_area * 1/2 * water_density * v^2
    // coeff_water_resistance = c_water_drag * cross_area * 1/2 * water_density
    coefficient_water_resistance = c_water_drag * width_m * draft_m * 0.5 * water_density;
    coeff_water_dirty = false;
    return coefficient_water_resistance;
}

double vehicle::max_buoyancy() const
{
    if( coeff_water_dirty ) {
        coeff_water_drag();
    }
    const double total_volume = hull_area * water_hull_height();
    return total_volume * water_density * GRAVITY_OF_EARTH;
}

float vehicle::k_traction( float wheel_traction_area ) const
{
    if( is_floating ) {
        return can_float() ? 1.0f : -1.0f;
    }
    if( is_flying ) {
        // Dont prematurely kill our flight, we'll fall soon enough
        return ( has_lift() ) ? 1.0f : -1.0f;
    }
    if( is_watercraft() && can_float() ) {
        return 1.0f;
    }

    const float fraction_without_traction = 1.0f - wheel_traction_area / wheel_area();
    if( fraction_without_traction == 0 ) {
        return 1.0f;
    }
    const float mass_penalty = fraction_without_traction * to_kilogram( total_mass() );
    float traction = std::min( 1.0f, wheel_traction_area / mass_penalty );
    add_msg( m_debug, "%s has traction %.2f", name, traction );

    // For now make it easy until it gets properly balanced: add a low cap of 0.1
    return std::max( 0.1f, traction );
}

int vehicle::static_drag( bool actual ) const
{
    return extra_drag + ( actual && !engine_on && !is_towed() ? -1500 : 0 );
}

float vehicle::strain() const
{
    int mv = max_velocity();
    int sv = safe_velocity();
    if( mv <= sv ) {
        mv = sv + 1;
    }
    if( velocity < sv && velocity > -sv ) {
        return 0;
    } else {
        return static_cast<float>( std::abs( velocity ) - sv ) / static_cast<float>( mv - sv );
    }
}

bool vehicle::sufficient_wheel_config() const
{
    if( wheelcache.empty() ) {
        // No wheels!
        return false;
    } else if( wheelcache.size() == 1 ) {
        //Has to be a stable wheel, and one wheel can only support a 1-3 tile vehicle
        if( !part_info( wheelcache.front() ).has_flag( "STABLE" ) ||
            all_parts_at_location( part_location_structure ).size() > 3 ) {
            return false;
        }
    }
    return true;
}

bool vehicle::is_owned_by( const Character &c, bool available_to_take ) const
{
    if( owner.is_null() ) {
        return available_to_take;
    }
    if( !c.get_faction() ) {
        debugmsg( "vehicle::is_owned_by() player %s has no faction", c.disp_name() );
        return false;
    }
    return c.get_faction()->id == get_owner();
}

bool vehicle::is_old_owner( const Character &c, bool available_to_take ) const
{
    if( old_owner.is_null() ) {
        return available_to_take;
    }
    if( !c.get_faction() ) {
        debugmsg( "vehicle::is_old_owner() player %s has no faction", c.disp_name() );
        return false;
    }
    return c.get_faction()->id == get_old_owner();
}

std::string vehicle::get_owner_name() const
{
    if( !g->faction_manager_ptr->get( owner ) ) {
        debugmsg( "vehicle::get_owner_name() vehicle %s has no valid nor null faction id ", disp_name() );
        return _( "no owner" );
    }
    return _( g->faction_manager_ptr->get( owner )->name );
}

bool vehicle::has_owner() const
{
    return !owner.is_null();
}

faction_id vehicle::get_owner() const
{
    return owner;
}

void vehicle::set_owner( const Character &c )
{
    const auto faction = c.get_faction();
    if( !faction ) {
        debugmsg( "vehicle::set_owner() player %s has no valid faction",
                  c.disp_name() );
        return;
    }
    owner = faction->id;
}

void vehicle::set_owner( const faction_id &new_owner )
{
    owner = new_owner;
}

void vehicle::remove_owner()
{
    owner = faction_id::NULL_ID();
}

bool vehicle::has_old_owner() const
{
    return !old_owner.is_null();
}

faction_id vehicle::get_old_owner() const
{
    return old_owner;
}

void vehicle::set_old_owner( const faction_id &temp_owner )
{
    theft_time = calendar::turn;
    old_owner = temp_owner;
}

void vehicle::remove_old_owner()
{
    theft_time = std::nullopt;
    old_owner = faction_id::NULL_ID();
}

bool vehicle::handle_potential_theft( avatar &you, bool check_only, bool prompt )
{
    const bool is_owned_by_player = is_owned_by( you );
    bool has_witnesses;
    if( has_owner() && !is_owned_by_player ) {
        has_witnesses = !avatar_funcs::list_potential_theft_witnesses( you, get_owner() ).empty();
    } else {
        has_witnesses = false;
    }
    // the vehicle is yours, that's fine.
    if( is_owned_by_player ) {
        return true;
        // if There is no owner
        // handle transfer of ownership
    }

    // if we are just checking if we could continue without problems, then the rest is assumed false
    if( check_only ) {
        return false;
    }

    if( !has_owner() ) {
        set_owner( you.get_faction()->id );
        remove_old_owner();
        return true;
        // if there is a marker for having been stolen, but 15 minutes have passed, then officially transfer ownership
    }

    if( !has_witnesses && has_old_owner() ) {
        if( !is_old_owner( you ) && theft_time && calendar::turn - *theft_time > 15_minutes ) {
            set_owner( you.get_faction()->id );
            remove_old_owner();
        }
        return true;
        // No witnesses? then don't need to prompt, we assume the player is in process of stealing it.
        // Ownership transfer checking is handled above, and warnings handled below.
        // This is just to perform interaction with the vehicle without a prompt.
        // It will prompt first-time, even with no witnesses, to inform player it is owned by someone else
        // subsequently, no further prompts, the player should know by then.
    }

    // if we got here, there's some theft occurring
    if( prompt ) {
        if( !query_yn(
                _( "This vehicle belongs to: %s, there may be consequences if you are observed interacting with it, continue?" ),
                _( get_owner_name() ) ) ) {
            return false;
        }
    }
    // set old owner so that we can restore ownership if there are witnesses.
    set_old_owner( get_owner() );
    if( avatar_funcs::handle_theft_witnesses( you, get_owner() ) ) {
        // remove the temporary marker for a successful theft, as it was witnessed.
        remove_old_owner();
    }
    // if we got here, then the action will proceed after the previous warning
    return true;
}

bool vehicle::balanced_wheel_config() const
{
    point min = point_max;
    point max = point_min;
    // find the bounding box of the wheels
    for( auto &w : wheelcache ) {
        const auto &pt = parts[ w ].mount;
        min.x = std::min( min.x, pt.x );
        min.y = std::min( min.y, pt.y );
        max.x = std::max( max.x, pt.x );
        max.y = std::max( max.y, pt.y );
    }

    // Check center of mass inside support of wheels (roughly)
    point com = local_center_of_mass();
    const inclusive_rectangle<point> support( min, max );
    return support.contains( com );
}

bool vehicle::valid_wheel_config() const
{
    return sufficient_wheel_config() && balanced_wheel_config();
}

float vehicle::steering_effectiveness() const
{
    if( is_floating ) {
        // I'M ON A BOAT
        return can_float() ? 1.0f : 0.0f;
    }
    if( is_flying ) {
        // I'M IN THE AIR
        // May need to add a separate check for planes, if/when they happen
        return is_aircraft() ? 1.0f : 0.0f;
    }
    // irksome special case for boats in shallow water
    if( is_watercraft() && can_float() ) {
        return 1.0f;
    }

    if( steering.empty() ) {
        return -1.0f; // No steering installed
    }
    // If the only steering part is an animal harness, with no animal in, it
    // is not steerable.
    const vehicle_part &vp = parts[ steering[0] ];
    if( steering.size() == 1 && vp.info().fuel_type == fuel_type_animal ) {
        monster *mon = get_pet( steering[0] );
        if( mon == nullptr || !mon->has_effect( effect_harnessed ) ) {
            return -2.0f;
        }
    }
    // For now, you just need one wheel working for 100% effective steering.
    // TODO: return something less than 1.0 if the steering isn't so good
    // (unbalanced, long wheelbase, back-heavy vehicle with front wheel steering,
    // etc)
    for( int p : steering ) {
        if( parts[ p ].is_available() ) {
            return 1.0f;
        }
    }

    // We have steering, but it's all broken.
    return 0.0f;
}

float vehicle::handling_difficulty() const
{
    const float steer = std::max( 0.0f, steering_effectiveness() );
    const float ktraction = k_traction( g->m.vehicle_wheel_traction( *this ) );
    const float aligned = std::max( 0.0f, 1.0f - ( face_vec() - dir_vec() ).magnitude() );

    // TestVehicle: perfect steering, moving on road at 100 mph (25 tiles per turn) = 0.0
    // TestVehicle but on grass (0.75 friction) = 2.5
    // TestVehicle but with bad steering (0.5 steer) = 5
    // TestVehicle but on fungal bed (0.5 friction) and bad steering = 10
    // TestVehicle but turned 90 degrees during this turn (0 align) = 10
    const float diff_mod = ( ( 1.0f - steer ) + ( 1.0f - ktraction ) + ( 1.0f - aligned ) );
    return velocity * diff_mod / vehicles::vmiph_per_tile;
}

std::map<itype_id, int> vehicle::fuel_usage() const
{
    std::map<itype_id, int> ret;
    for( size_t i = 0; i < engines.size(); i++ ) {
        // Note: functions with "engine" in name do NOT take part indices
        // TODO: Use part indices and not engine vector indices
        if( !is_engine_on( i ) ) {
            continue;
        }

        const size_t e = engines[ i ];
        const auto &info = part_info( e );
        static const itype_id null_fuel_type( "null" );
        const itype_id &cur_fuel = parts[ e ].fuel_current();
        if( cur_fuel  == null_fuel_type ) {
            continue;
        }

        if( !is_perpetual_type( i ) ) {
            int usage = info.energy_consumption;
            if( parts[ e ].faults().contains( fault_filter_air ) ) {
                usage *= 2;
            }

            ret[ cur_fuel ] += usage;
        }
    }

    return ret;
}

double vehicle::drain_energy( const itype_id &ftype, double energy_j )
{
    // Consumption of battery power is done differently.
    // From all batteries at once and doesn't change mass.
    if( ftype == fuel_type_battery ) {
        // Batteries stored in kilojoules
        const int total_kj_to_drain = static_cast<int>( energy_j / 1000.0 );
        if( total_kj_to_drain <= 0 ) {
            return 0.0;
        }
        const int not_fulfilled = discharge_battery( total_kj_to_drain );
        return static_cast<double>( total_kj_to_drain - not_fulfilled ) * 1000.0;
    }

    double drained = 0.0f;
    for( auto &p : parts ) {
        if( energy_j <= 0.0f ) {
            break;
        }

        const double consumed = p.consume_energy( ftype, energy_j );
        drained += consumed;
        energy_j -= consumed;
    }

    invalidate_mass();
    return drained;
}

void vehicle::consume_fuel( int load, const int t_seconds, bool skip_electric )
{
    double st = strain();
    if( current_acceleration() == 0 ) {
        return;
    }
    for( auto &fuel_pr : fuel_usage() ) {
        auto &ft = fuel_pr.first;
        if( skip_electric && ft == fuel_type_battery ) {
            continue;
        }

        double amnt_precise_j = static_cast<double>( fuel_pr.second ) * t_seconds;
        amnt_precise_j *= load / 1000.0 * ( 1.0 + st * st * 100.0 );
        auto inserted = fuel_used_last_turn.insert( { ft, 0.0f } );
        inserted.first->second += amnt_precise_j;
        double remainder = fuel_remainder[ ft ];
        amnt_precise_j -= remainder;

        if( amnt_precise_j > 0.0f ) {
            fuel_remainder[ ft ] = drain_energy( ft, amnt_precise_j ) - amnt_precise_j;
        } else {
            fuel_remainder[ ft ] = -amnt_precise_j;
        }
    }
    // we want this to update the activity level whenever the engine is running
    if( load > 0 && fuel_left( fuel_type_muscle ) > 0 ) {
        //do this as a function of current load
        // But only if the player is actually there!
        int eff_load = load / 10;
        int mod = 4 * st; // strain
        int base_burn = static_cast<int>( get_option<float>( "PLAYER_BASE_STAMINA_REGEN_RATE" ) ) -
                        3;
        base_burn = std::max( eff_load / 3, base_burn );
        //charge bionics when using muscle engine
        const item &muscle = *item::spawn_temporary( "muscle" );
        for( const bionic_id &bid : g->u.get_bionic_fueled_with( muscle ) ) {
            if( g->u.has_active_bionic( bid ) ) { // active power gen
                // more pedaling = more power
                g->u.mod_power_level( units::from_kilojoule( muscle.fuel_energy() ) * bid->fuel_efficiency *
                                      ( load / 1000.0f ) );
                mod += eff_load / 5;
            } else { // passive power gen
                g->u.mod_power_level( units::from_kilojoule( muscle.fuel_energy() ) * bid->passive_fuel_efficiency *
                                      ( load / 1000.0f ) );
                mod += eff_load / 10;
            }
        }
        // decreased stamina burn scalable with load
        if( g->u.has_active_bionic( bio_jointservo ) ) {
            g->u.mod_power_level( -bio_jointservo->power_trigger * std::max( eff_load / 20, 1 ) );
            mod -= std::max( eff_load / 5, 5 );
        }
        if( one_in( 1000 / load ) && one_in( 10 ) ) {
            g->u.mod_stored_kcal( -10 );
            g->u.mod_thirst( 1 );
            g->u.mod_fatigue( 1 );
        }
        g->u.mod_stamina( -( base_burn + mod ) );
        add_msg( m_debug, "Load: %d", load );
        add_msg( m_debug, "Mod: %d", mod );
        add_msg( m_debug, "Burn: %d", -( base_burn + mod ) );
    }
}

std::vector<vehicle_part *> vehicle::lights( bool active )
{
    std::vector<vehicle_part *> res;
    for( auto &e : parts ) {
        if( ( !active || e.enabled ) && e.is_available() && e.is_light() ) {
            res.push_back( &e );
        }
    }
    return res;
}

int vehicle::total_accessory_epower_w() const
{
    int epower = 0;
    for( const vpart_reference &vp : get_enabled_parts( VPFLAG_ENABLED_DRAINS_EPOWER ) ) {
        epower += vp.info().epower;
    }
    return epower;
}

int vehicle::total_alternator_epower_w() const
{
    int epower = 0;
    if( engine_on ) {
        // If the engine is on, the alternators are working.
        for( size_t p = 0; p < alternators.size(); ++p ) {
            if( is_alternator_on( p ) ) {
                epower += part_epower_w( alternators[p] );
            }
        }
    }
    return epower;
}

int vehicle::total_engine_epower_w() const
{
    int epower = 0;

    // Engines: can both produce (plasma) or consume (gas, diesel) epower.
    // Gas engines require epower to run for ignition system, ECU, etc.
    // Electric motor consumption not included, see @ref vpart_info::energy_consumption
    if( engine_on ) {
        for( size_t e = 0; e < engines.size(); ++e ) {
            if( is_engine_on( e ) ) {
                epower += part_epower_w( engines[e] );
            }
        }
    }

    return epower;
}

int vehicle::total_solar_epower_w() const
{
    int epower_w = 0;
    for( int part : solar_panels ) {
        if( parts[ part ].is_unavailable() ) {
            continue;
        }

        if( !is_sm_tile_outside( g->m.getabs( global_part_pos3( part ) ) ) ) {
            continue;
        }

        epower_w += part_epower_w( part );
    }
    // Weather doesn't change much across the area of the vehicle, so just
    // sample it once.
    const weather_type_id &wtype = current_weather( global_pos3() );
    const float tick_sunlight = incident_sunlight( wtype, calendar::turn );
    double intensity = tick_sunlight / default_daylight_level();
    return epower_w * intensity;
}

int vehicle::total_wind_epower_w() const
{
    map &here = get_map();
    const oter_id &cur_om_ter = overmap_buffer.ter( global_omt_location() );
    const weather_manager &weather = get_weather();
    int epower_w = 0;
    for( int part : wind_turbines ) {
        if( parts[ part ].is_unavailable() ) {
            continue;
        }

        if( !is_sm_tile_outside( here.getabs( global_part_pos3( part ) ) ) ) {
            continue;
        }

        double windpower = get_local_windpower( weather.windspeed, cur_om_ter, global_part_pos3( part ),
                                                weather.winddirection, false );
        if( windpower <= ( weather.windspeed / 10.0 ) ) {
            continue;
        }
        epower_w += part_epower_w( part ) * windpower;
    }
    return epower_w;
}

int vehicle::total_water_wheel_epower_w() const
{
    int epower_w = 0;
    for( int part : water_wheels ) {
        if( parts[ part ].is_unavailable() ) {
            continue;
        }

        if( !is_sm_tile_over_water( g->m.getabs( global_part_pos3( part ) ) ) ) {
            continue;
        }

        epower_w += part_epower_w( part );
    }
    // TODO: river current intensity changes power - flat for now.
    return epower_w;
}

int vehicle::net_battery_charge_rate_w() const
{
    return total_engine_epower_w() + total_alternator_epower_w() + total_accessory_epower_w() +
           total_solar_epower_w() + total_wind_epower_w() + total_water_wheel_epower_w() +
           max_reactor_epower_w();
}

int vehicle::max_reactor_epower_w() const
{
    int epower_w = 0;
    for( int elem : reactors ) {
        epower_w += is_part_on( elem ) ? part_epower_w( elem ) : 0;
    }
    return epower_w;
}

void vehicle::update_alternator_load()
{
    // Update alternator load
    if( engine_on ) {
        int engine_vpower = 0;
        for( size_t e = 0; e < engines.size(); ++e ) {
            if( is_engine_on( e ) && parts[engines[e]].info().has_flag( "E_ALTERNATOR" ) ) {
                engine_vpower += part_vpower_w( engines[e] );
            }
        }
        int alternators_power = 0;
        for( size_t p = 0; p < alternators.size(); ++p ) {
            if( is_alternator_on( p ) ) {
                alternators_power += part_vpower_w( alternators[p] );
            }
        }
        alternator_load =
            engine_vpower
            ? 1000 * ( std::abs( alternators_power ) + std::abs( extra_drag ) ) / engine_vpower
            : 0;
    } else {
        alternator_load = 0;
    }
}

void vehicle::power_parts()
{
    update_alternator_load();
    // Things that drain energy: engines and accessories.
    int engine_epower = total_engine_epower_w();
    int epower = engine_epower + total_accessory_epower_w() + total_alternator_epower_w();

    int delta_energy_bat = power_to_energy_bat( epower, 1_turns );
    int storage_deficit_bat = std::max( 0, fuel_capacity( fuel_type_battery ) -
                                        fuel_left( fuel_type_battery ) - delta_energy_bat );
    // Reactors trigger only on demand. If we'd otherwise run out of power, see
    // if we can spin up the reactors.
    if( !reactors.empty() && storage_deficit_bat > 0 ) {
        // Still not enough surplus epower to fully charge battery
        // Produce additional epower from any reactors
        bool reactor_working = false;
        bool reactor_online = false;
        for( auto &elem : reactors ) {
            // Check whether the reactor is on. If not, move on.
            if( !is_part_on( elem ) ) {
                continue;
            }
            // Keep track whether or not the vehicle has any reactors activated
            reactor_online = true;
            // the amount of energy the reactor generates each turn
            const int gen_energy_bat = power_to_energy_bat( part_epower_w( elem ), 1_turns );
            if( parts[ elem ].is_unavailable() ) {
                continue;
            } else if( parts[ elem ].info().has_flag( STATIC( std::string( "PERPETUAL" ) ) ) ) {
                reactor_working = true;
                delta_energy_bat += std::min( storage_deficit_bat, gen_energy_bat );
            } else if( parts[elem].ammo_remaining() > 0 ) {
                // Efficiency: one unit of fuel is this many units of battery
                // Note: One battery is 1 kJ
                const int efficiency = part_info( elem ).power;
                const int avail_fuel = parts[elem].ammo_remaining() * efficiency;
                const int elem_energy_bat = std::min( gen_energy_bat, avail_fuel );
                // Cap output at what we can achieve and utilize
                const int reactors_output_bat = std::min( elem_energy_bat, storage_deficit_bat );
                // Fuel consumed in actual units of the resource
                int fuel_consumed = reactors_output_bat / efficiency;
                // Remainder has a chance of resulting in more fuel consumption
                fuel_consumed += x_in_y( reactors_output_bat % efficiency, efficiency ) ? 1 : 0;
                parts[ elem ].ammo_consume( fuel_consumed, global_part_pos3( elem ) );
                reactor_working = true;
                delta_energy_bat += reactors_output_bat;
            }
        }

        if( !reactor_working && reactor_online ) {
            // All reactors out of fuel or destroyed
            for( auto &elem : reactors ) {
                parts[ elem ].enabled = false;
            }
            if( player_in_control( g->u ) || g->u.sees( global_pos3() ) ) {
                add_msg( _( "The %s's reactor dies!" ), name );
            }
        }
    }

    int battery_deficit = 0;
    if( delta_energy_bat > 0 ) {
        // store epower surplus in battery
        charge_battery( delta_energy_bat );
    } else if( epower < 0 ) {
        // draw epower deficit from battery
        battery_deficit = discharge_battery( std::abs( delta_energy_bat ) );
    }

    if( battery_deficit != 0 ) {
        // Scoops need a special case since they consume power during actual use
        for( const vpart_reference &vp : get_enabled_parts( "SCOOP" ) ) {
            vp.part().enabled = false;
        }
        // Rechargers need special case since they consume power on demand
        for( const vpart_reference &vp : get_enabled_parts( "RECHARGE" ) ) {
            vp.part().enabled = false;
        }

        for( const vpart_reference &vp : get_enabled_parts( VPFLAG_ENABLED_DRAINS_EPOWER ) ) {
            vehicle_part &pt = vp.part();
            if( pt.info().epower < 0 ) {
                pt.enabled = false;
            }
        }

        is_alarm_on = false;
        camera_on = false;
        if( player_in_control( g->u ) || g->u.sees( global_pos3() ) ) {
            add_msg( _( "The %s's battery dies!" ), name );
        }
        if( engine_epower < 0 ) {
            // Not enough epower to run gas engine ignition system
            engine_on = false;
            if( player_in_control( g->u ) || g->u.sees( global_pos3() ) ) {
                add_msg( _( "The %s's engine dies!" ), name );
            }
        }
    }
}

vehicle *vehicle::find_vehicle( const tripoint &where )
{
    // Is it in the reality bubble?
    tripoint veh_local = g->m.getlocal( where );
    if( const optional_vpart_position vp = g->m.veh_at( veh_local ) ) {
        return &vp->vehicle();
    }

    // Nope. Load up its submap...
    tripoint veh_in_sm = where;
    tripoint veh_sm = ms_to_sm_remain( veh_in_sm );

    auto sm = MAPBUFFER.lookup_submap( veh_sm );
    if( sm == nullptr ) {
        return nullptr;
    }

    for( auto &elem : sm->vehicles ) {
        vehicle *found_veh = elem.get();
        if( veh_in_sm.xy() == found_veh->pos ) {
            return found_veh;
        }
    }

    return nullptr;
}

void vehicle::enumerate_vehicles( std::map<vehicle *, bool> &connected_vehicles,
                                  const std::set<vehicle *> &vehicle_list )
{
    auto enumerate_visitor = [&connected_vehicles]( vehicle & veh ) {
        // Only emplaces if element is not present already.
        connected_vehicles.emplace( &veh, false );
        return distribution_graph::traverse_visitor_result::continue_further;
    };
    for( vehicle *veh : vehicle_list ) {
        // This autovivifies, and also overwrites the value if already present.
        connected_vehicles[veh] = true;
        distribution_graph::traverse( *veh, enumerate_visitor, distribution_graph::noop_visitor_grid );
    }
}

// TODO: It looks out of place in vehicle.cpp
namespace distribution_graph
{

template <bool IsConst,
          typename Vehicle = std::conditional_t<IsConst, const vehicle, vehicle>,
          typename Grid = std::conditional_t<IsConst, const distribution_grid, distribution_grid>>
struct vehicle_or_grid {
    enum class type_t : char {
        vehicle,
        grid
    } type;

    Vehicle *veh = nullptr;
    Grid *grid = nullptr;

    vehicle_or_grid( Vehicle *veh )
        : type( type_t::vehicle )
        , veh( veh )
    {}

    vehicle_or_grid( Grid *grid )
        : type( type_t::grid )
        , grid( grid )
    {}

    bool operator==( const vehicle_or_grid &other ) const {
        return veh == other.veh && grid == other.grid;
    }

    bool operator==( const vehicle *veh ) const {
        return this->veh == veh;
    }

    bool operator==( const distribution_grid *grid ) const {
        return this->grid == grid;
    }
};

template <typename VehFunc, typename GridFunc, typename StartPoint>
void traverse( StartPoint &start,
               VehFunc veh_action, GridFunc grid_action )
{
    using tvr = traverse_visitor_result;
    constexpr bool IsConst = std::is_const_v<StartPoint>;
    struct hash {
        const std::hash<char> char_hash = std::hash<char>();
        const std::hash<size_t> ptr_hash = std::hash<size_t>();
        auto operator()( const vehicle_or_grid<IsConst> &elem ) const {
            return char_hash( static_cast<char>( elem.type ) ) ^
                   ptr_hash(
                       // Because only one of pointers is not nullptr, binary OR would get value of set pointer.
                       reinterpret_cast<size_t>( elem.veh ) | reinterpret_cast<size_t>( elem.grid )
                   );
        }
    };

    // Actually, they are visited elements with unvisited neighbours.
    // Not all connected elements are here.
    std::queue<vehicle_or_grid<IsConst>> connected_elements;
    // For fast checking if we should visit some neighbour.
    std::unordered_set<vehicle_or_grid<IsConst>, hash> visited_elements;
    connected_elements.emplace( &start );
    visited_elements.insert( &start );
    auto &grid_tracker = get_distribution_grid_tracker();

    auto enqueue = [&connected_elements, &visited_elements]( vehicle_or_grid<IsConst> newly_visited ) {
        connected_elements.push( newly_visited );
        visited_elements.insert( newly_visited );
    };
    auto was_already_visited = [&visited_elements]( vehicle_or_grid<IsConst> to_visit ) {
        return visited_elements.count( to_visit ) != 0;
    };

    auto process_vehicle = [&]( const tripoint_abs_ms & target_pos ) {
        auto *veh = vehicle::find_vehicle( target_pos );
        if( veh == nullptr ) {
            debugmsg( "lost vehicle at %s", target_pos.to_string() );
            return tvr::continue_further;
        }

        if( was_already_visited( veh ) ) {
            return tvr::continue_further;
        }

        const tvr result = veh_action( *veh );
        g->u.add_msg_if_player( m_debug, "After remote veh %p",
                                static_cast<void *>( veh ) );

        // We do not need to check neighbours if we stop.
        if( result == tvr::continue_further ) {
            enqueue( veh );
        }

        return result;
    };

    auto process_grid = [&]( const tripoint_abs_ms & target_pos ) {
        auto &grid = grid_tracker.grid_at( target_pos );
        if( !grid ) {
            debugmsg( "lost grid at %s", target_pos.to_string() );
            return tvr::continue_further;
        }

        if( was_already_visited( &grid ) ) {
            return tvr::continue_further;
        }

        const tvr result = grid_action( grid );
        g->u.add_msg_if_player( m_debug, "After remote grid %p",
                                static_cast<void *>( &grid ) );

        // We do not need to check neighbours if we stop.
        if( result == tvr::continue_further ) {
            enqueue( &grid );
        }

        return result;
    };

    while( !connected_elements.empty() ) {
        auto current = connected_elements.front();
        connected_elements.pop();

        if( current.type == vehicle_or_grid<IsConst>::type_t::vehicle ) {
            const vehicle &current_veh = *current.veh;
            for( auto &p : current_veh.loose_parts ) {
                if( !current_veh.part_info( p ).has_flag( "POWER_TRANSFER" ) ) {
                    // Ignore loose parts that aren't power transfer cables
                    continue;
                }

                const tripoint_abs_ms target_pos( current_veh.cpart( p ).target.second );
                if( current_veh.cpart( p ).has_flag( vehicle_part::targets_grid ) ) {
                    if( process_grid( target_pos ) == tvr::stop ) {
                        return;
                    }
                } else {
                    if( process_vehicle( target_pos ) == tvr::stop ) {
                        return;
                    }
                }
            }
        } else {
            // Grids can only be connected to vehicles at the moment
            auto &current_grid = *current.grid;
            for( auto &p : current_grid.get_contents() ) {
                const vehicle_connector_tile *connector = active_tiles::furn_at<vehicle_connector_tile>( p );
                if( connector == nullptr ) {
                    continue;
                }

                for( const tripoint_abs_ms &target_pos : connector->connected_vehicles ) {
                    if( process_vehicle( target_pos ) == tvr::stop ) {
                        return;
                    }
                }
            }
        }
    }
}

} // namespace distribution_graph

int vehicle::charge_battery( int amount, bool include_other_vehicles )
{
    // Key parts by percentage charge level.
    std::multimap<int, vehicle_part *> chargeable_parts;
    for( vehicle_part &p : parts ) {
        if( p.is_available() && p.is_battery() && p.ammo_capacity() > p.ammo_remaining() ) {
            chargeable_parts.insert( { ( p.ammo_remaining() * 100 ) / p.ammo_capacity(), &p } );
        }
    }
    while( amount > 0 && !chargeable_parts.empty() ) {
        // Grab first part, charge until it reaches the next %, then re-insert with new % key.
        auto iter = chargeable_parts.begin();
        int charge_level = iter->first;
        vehicle_part *p = iter->second;
        chargeable_parts.erase( iter );
        // Calculate number of charges to reach the next %, but insure it's at least
        // one more than current charge.
        int next_charge_level = ( ( charge_level + 1 ) * p->ammo_capacity() ) / 100;
        next_charge_level = std::max( next_charge_level, p->ammo_remaining() + 1 );
        int qty = std::min( amount, next_charge_level - p->ammo_remaining() );
        p->ammo_set( fuel_type_battery, p->ammo_remaining() + qty );
        amount -= qty;
        if( p->ammo_capacity() > p->ammo_remaining() ) {
            chargeable_parts.insert( { ( p->ammo_remaining() * 100 ) / p->ammo_capacity(), p } );
        }
    }

    if( amount > 0 && include_other_vehicles ) {
        // still a bit of charge we could send out...
        using tvr = distribution_graph::traverse_visitor_result;
        auto charge_veh = [&amount]( vehicle & veh ) {
            g->u.add_msg_if_player( m_debug, "CHv: %d", amount );
            amount = veh.charge_battery( amount, false );
            return amount > 0 ? tvr::continue_further : tvr::stop;
        };
        auto charge_grid = [&amount]( distribution_grid & grid ) {
            g->u.add_msg_if_player( m_debug, "CHg: %d", amount );
            amount = grid.mod_resource( amount, false );
            return amount > 0 ? tvr::continue_further : tvr::stop;
        };
        distribution_graph::traverse( *this, charge_veh, charge_grid );
    }


    return amount;
}

int vehicle::discharge_battery( int amount, bool recurse )
{
    // Key parts by percentage charge level.
    std::multimap<int, vehicle_part *> dischargeable_parts;
    for( vehicle_part &p : parts ) {
        if( p.is_available() && p.is_battery() && p.ammo_remaining() > 0 ) {
            dischargeable_parts.insert( { ( p.ammo_remaining() * 100 ) / p.ammo_capacity(), &p } );
        }
    }
    while( amount > 0 && !dischargeable_parts.empty() ) {
        // Grab first part, discharge until it reaches the next %, then re-insert with new % key.
        auto iter = std::prev( dischargeable_parts.end() );
        int charge_level = iter->first;
        vehicle_part *p = iter->second;
        dischargeable_parts.erase( iter );
        // Calculate number of charges to reach the previous %.
        int prev_charge_level = ( ( charge_level - 1 ) * p->ammo_capacity() ) / 100;
        prev_charge_level = std::max( 0, prev_charge_level );
        int amount_to_discharge = std::min( p->ammo_remaining() - prev_charge_level, amount );
        p->ammo_consume( amount_to_discharge, global_part_pos3( *p ) );
        amount -= amount_to_discharge;
        if( p->ammo_remaining() > 0 ) {
            dischargeable_parts.insert( { ( p->ammo_remaining() * 100 ) / p->ammo_capacity(), p } );
        }
    }

    if( amount > 0 && recurse ) {
        // need more power!
        using tvr = distribution_graph::traverse_visitor_result;
        auto discharge_vehicle = [&amount]( vehicle & veh ) {
            g->u.add_msg_if_player( m_debug, "CHv: %d", amount );
            amount = veh.discharge_battery( amount, false );
            return amount > 0 ? tvr::continue_further : tvr::stop;
        };
        auto discharge_grid = [&amount]( distribution_grid & grid ) {
            g->u.add_msg_if_player( m_debug, "CHg: %d", amount );
            amount = -grid.mod_resource( -amount, false );
            return amount > 0 ? tvr::continue_further : tvr::stop;
        };
        distribution_graph::traverse( *this, discharge_vehicle, discharge_grid );
    }

    return amount; // non-zero if we weren't able to fulfill demand.
}

void vehicle::do_engine_damage( size_t e, int strain )
{
    strain = std::min( 25, strain );
    if( is_engine_on( e ) && !is_perpetual_type( e ) &&
        engine_fuel_left( e ) && rng( 1, 100 ) < strain ) {
        int dmg = rng( 0, strain * 4 );
        damage_direct( engines[e], dmg );
        if( one_in( 2 ) ) {
            add_msg( _( "Your engine emits a high pitched whine." ) );
        } else {
            add_msg( _( "Your engine emits a loud grinding sound." ) );
        }
    }
}

void vehicle::idle( bool on_map )
{
    power_parts();
    if( engine_on && total_power_w() > 0 ) {
        bool no_electric_power = true;
        int idle_rate = alternator_load;
        if( idle_rate < 10 ) {
            idle_rate = 10;    // minimum idle is 1% of full throttle
        }
        // Helicopters use extra power just to stay in the air
        // 100 means 10% of power
        /*
            TODO: Consider different formula for idling aircraft, may need a formula to determine this
            Possibly something like total lift / total engine power, maybe some factors for hovering efficiency of different types
            Also consider adding a hover efficiency field
        */
        if( is_rotorcraft() && is_flying_in_air() ) {
            const auto rotor_newtons = std::max( 0.0,
                                                 to_newton( total_mass() ) - total_balloon_lift() - total_wing_lift() );
            const auto rotor_capacity = rotor_newtons / thrust_of_rotorcraft( true );
            idle_rate = std::max( 10, int( std::floor( 100 * rotor_capacity ) ) );
            no_electric_power = false;
        }
        if( has_engine_type_not( fuel_type_muscle, true ) ) {
            consume_fuel( idle_rate, to_turns<int>( 1_turns ), no_electric_power );
        }

        if( on_map ) {
            noise_and_smoke( idle_rate, 1_turns );
        }
    } else {
        if( engine_on && g->u.sees( global_pos3() ) &&
            ( has_engine_type_not( fuel_type_muscle, true ) && has_engine_type_not( fuel_type_animal, true ) &&
              has_engine_type_not( fuel_type_wind, true ) && has_engine_type_not( fuel_type_mana, true ) ) ) {
            add_msg( _( "The %s's engine dies!" ), name );
        }
        engine_on = false;
    }

    // Disallow running a planter underground for now
    if( !warm_enough_to_plant( g->u.pos() ) || global_pos3().z < 0 ) {
        for( const vpart_reference &vp : get_enabled_parts( "PLANTER" ) ) {
            if( g->u.sees( global_pos3() ) ) {
                add_msg( _( "The %s's planter turns off due to low temperature." ), name );
            }
            vp.part().enabled = false;
        }
    }

    if( !on_map ) {
        return;
    } else {
        update_time( calendar::turn );
    }

    process_emitters();

    if( has_part( "STEREO", true ) ) {
        play_music();
    }

    if( has_part( "CHIMES", true ) ) {
        play_chimes();
    }

    if( has_part( "CRASH_TERRAIN_AROUND", true ) ) {
        crash_terrain_around();
    }

    if( is_alarm_on ) {
        alarm();
    }

    vehicle_funcs::process_autoloaders( *this );
}

void vehicle::on_move()
{
    if( has_part( "TRANSFORM_TERRAIN", true ) ) {
        transform_terrain();
    }
    if( has_part( "SCOOP", true ) ) {
        operate_scoop();
    }
    if( has_part( "PLANTER", true ) ) {
        operate_planter();
    }
    if( has_part( "REAPER", true ) ) {
        operate_reaper();
    }

    occupied_cache_time = calendar::before_time_starts;
}

void vehicle::slow_leak()
{
    // for each badly damaged tanks (lower than 50% health), leak a small amount
    for( auto &p : parts ) {
        auto health = p.health_percent();
        if( !p.is_leaking() || p.ammo_remaining() <= 0 ) {
            continue;
        }

        auto fuel = p.ammo_current();
        int qty = std::max( ( 0.5 - health ) * ( 0.5 - health ) * p.ammo_remaining() / 10, 1.0 );
        point q = coord_translate( p.mount );
        const tripoint dest = global_pos3() + tripoint( q, 0 );

        if( fuel != fuel_type_battery && !g->m.inbounds( dest ) ) {
            // Don't try to leak off the edge of the world
            continue;
        }

        // damaged batteries self-discharge without leaking, plutonium leaks slurry
        if( fuel != fuel_type_battery && fuel != fuel_type_plutonium_cell ) {
            g->m.add_item_or_charges( dest, item::spawn( fuel, calendar::turn, qty ) );
            p.ammo_consume( qty, global_part_pos3( p ) );
        } else if( fuel == fuel_type_plutonium_cell ) {
            if( p.ammo_remaining() >= PLUTONIUM_CHARGES / 10 ) {
                g->m.add_item_or_charges( dest, item::spawn( "plut_slurry_dense", calendar::turn, qty ) );
                p.ammo_consume( qty * PLUTONIUM_CHARGES / 10, global_part_pos3( p ) );
            } else {
                p.ammo_consume( p.ammo_remaining(), global_part_pos3( p ) );
            }
        } else {
            p.ammo_consume( qty, global_part_pos3( p ) );
        }
    }
}

// total volume of all the things
units::volume vehicle::stored_volume( const int part ) const
{
    return get_items( part ).stored_volume();
}

units::volume vehicle::max_volume( const int part ) const
{
    return get_items( part ).max_volume();
}

units::volume vehicle::free_volume( const int part ) const
{
    return get_items( part ).free_volume();
}

void vehicle::make_inactive( item &target )
{
    auto cargo_parts = get_parts_at( target.position(), "CARGO", part_status_flag::any );
    if( cargo_parts.empty() ) {
        return;
    }
    active_items.remove( &target );
}

void vehicle::make_active( item &target )
{
    if( !target.needs_processing() ) {
        return;
    }
    auto cargo_parts = get_parts_at( target.position(), "CARGO", part_status_flag::any );
    if( cargo_parts.empty() ) {
        return;
    }
    active_items.add( target );
}

detached_ptr<item> vehicle::add_charges( int part, detached_ptr<item> &&itm )
{
    if( !itm->count_by_charges() ) {
        debugmsg( "Add charges was called for an item not counted by charges!" );
        return std::move( itm );
    }
    const int amount = get_items( part ).amount_can_fit( *itm );
    if( amount == 0 ) {
        return std::move( itm );
    }

    detached_ptr<item> itm_copy = item::spawn( *itm );
    itm_copy->charges = amount;
    itm->charges -= amount;
    detached_ptr<item> remaining = add_item( part, std::move( itm_copy ) );
    itm->charges += remaining->charges;
    return itm->charges > 0 ? std::move( itm ) : detached_ptr<item>();
}

detached_ptr<item> vehicle::add_item( vehicle_part &pt, detached_ptr<item> &&obj )
{
    int idx = index_of_part( &pt );
    if( idx < 0 ) {
        debugmsg( "Tried to add item to invalid part" );
        return std::move( obj );
    }
    return add_item( idx, std::move( obj ) );
}

detached_ptr<item> vehicle::add_item( int part, detached_ptr<item> &&itm )
{
    if( part < 0 || part >= static_cast<int>( parts.size() ) ) {
        debugmsg( "int part (%d) is out of range", part );
        return std::move( itm );
    }
    // const int max_weight = ?! // TODO: weight limit, calculation per vpart & vehicle stats, not a hard user limit.
    // add creaking sounds and damage to overloaded vpart, outright break it past a certain point, or when hitting bumps etc
    vehicle_part &p = parts[ part ];
    if( p.is_broken() ) {
        return std::move( itm );
    }

    if( p.base->is_gun() ) {
        if( !itm->is_ammo() || !p.base->ammo_types().contains( itm->ammo_type() ) ) {
            return std::move( itm );
        }
    }
    bool charge = itm->count_by_charges();
    vehicle_stack istack = get_items( part );
    const int to_move = istack.amount_can_fit( *itm );
    if( to_move == 0 || ( charge && to_move < itm->charges ) ) {
        return std::move( itm ); // @add_charges should be used in the latter case
    }
    if( charge ) {
        item *here = istack.stacks_with( *itm );
        if( here ) {
            invalidate_mass();
            if( !here->merge_charges( std::move( itm ) ) ) {
                // NOLINTNEXTLINE(bugprone-use-after-move)
                return std::move( itm );
            } else {
                return detached_ptr<item>();
            }
        }
    }


    if( itm->is_bucket_nonempty() ) {
        itm->contents.spill_contents( global_part_pos3( part ) );
    }
    if( itm->needs_processing() ) {
        active_items.add( *itm );
    }
    p.items.push_back( std::move( itm ) );

    invalidate_mass();
    return detached_ptr<item>();
}

detached_ptr<item> vehicle::remove_item( int part, item *it )
{
    const location_vector<item> &veh_items = parts[part].items;

    const location_vector<item>::const_iterator iter = std::find_if( veh_items.begin(),
    veh_items.end(), [&it]( const item * const & item ) {
        return it == item;
    } );

    if( iter == veh_items.end() ) {
        return detached_ptr<item>();
    }
    detached_ptr<item> det;
    remove_item( part, iter, &det );
    return det;
}

vehicle_stack::iterator vehicle::remove_item( int part, vehicle_stack::const_iterator it,
        detached_ptr<item> *ret )
{
    // remove from the active items cache (if it isn't there does nothing)
    active_items.remove( *it );

    vehicle_stack::iterator iter = parts[part].items.erase( std::move( it ), ret );
    invalidate_mass();
    return iter;
}

vehicle_stack vehicle::get_items( const int part )
{
    const tripoint pos = global_part_pos3( part );
    return vehicle_stack( &parts[part].items, pos.xy(), this, part );
}

vehicle_stack vehicle::get_items( const int part ) const
{
    // HACK: callers could modify items through this
    // TODO: a const version of vehicle_stack is needed
    return const_cast<vehicle *>( this )->get_items( part );
}

void vehicle::place_spawn_items()
{
    if( !type.is_valid() ) {
        return;
    }

    for( const auto &pt : type->parts ) {
        if( pt.with_ammo ) {
            int turret = part_with_feature( pt.pos, "TURRET", true );
            if( turret >= 0 && x_in_y( pt.with_ammo, 100 ) ) {
                parts[ turret ].ammo_set( random_entry( pt.ammo_types ), rng( pt.ammo_qty.first,
                                          pt.ammo_qty.second ) );
            }
        }
    }

    for( const auto &spawn : type.obj().item_spawns ) {
        if( rng( 1, 100 ) <= spawn.chance ) {
            int part = part_with_feature( spawn.pos, "CARGO", false );
            if( part < 0 ) {
                debugmsg( "No CARGO parts at (%d, %d) of %s!", spawn.pos.x, spawn.pos.y, name );

            } else {
                // if vehicle part is broken only 50% of items spawn and they will be variably damaged
                bool broken = parts[ part ].is_broken();
                if( broken && one_in( 2 ) ) {
                    continue;
                }

                std::vector<detached_ptr<item>> created;
                created.reserve( spawn.item_ids.size() );
                for( const itype_id &e : spawn.item_ids ) {
                    created.emplace_back( item::in_its_container( item::spawn( e ) ) );
                }
                for( const item_group_id &e : spawn.item_groups ) {
                    std::vector<detached_ptr<item>> group_items = item_group::items_from( e,
                                                 calendar::start_of_cataclysm );
                    for( auto &spawn_item : group_items ) {
                        created.emplace_back( std::move( spawn_item ) );
                    }
                }

                for( detached_ptr<item> &e : created ) {
                    if( e->is_null() ) {
                        continue;
                    }
                    if( broken && e->mod_damage( rng( 1, e->max_damage() ) ) ) {
                        continue; // we destroyed the item
                    }
                    if( e->is_tool() || e->is_gun() || e->is_magazine() ) {
                        bool spawn_ammo = rng( 0, 99 ) < spawn.with_ammo && e->ammo_remaining() == 0;
                        bool spawn_mag  = rng( 0, 99 ) < spawn.with_magazine && !e->magazine_integral() &&
                                          !e->magazine_current();

                        if( spawn_mag ) {
                            e->put_in( item::spawn( e->magazine_default(), e->birthday() ) );
                        }
                        if( spawn_ammo ) {
                            e->ammo_set( e->ammo_default() );
                        }
                    }
                    add_item( part, std::move( e ) );
                }
            }
        }
    }
}

void vehicle::gain_moves()
{
    fuel_used_last_turn.clear();
    check_falling_or_floating();
    const bool pl_control = player_in_control( g->u );
    if( is_moving() || is_falling ) {
        if( !loose_parts.empty() ) {
            shed_loose_parts();
        }
        of_turn = 1 + of_turn_carry;
        const int vslowdown = slowdown( velocity );
        if( vslowdown > std::abs( velocity ) ) {
            if( cruise_on && cruise_velocity && pl_control ) {
                velocity = velocity > 0 ? 1 : -1;
            } else {
                stop();
            }
        } else if( velocity < 0 ) {
            velocity += vslowdown;
        } else {
            velocity -= vslowdown;
        }
    } else {
        of_turn = .001;
    }
    of_turn_carry = 0;
    // cruise control TODO: enable for NPC?
    if( ( pl_control || is_following || is_patrolling ) && cruise_on && cruise_velocity != velocity ) {
        thrust( ( cruise_velocity ) > velocity ? 1 : -1 );
    }

    // Force off-map vehicles to load by visiting them every time we gain moves.
    // Shouldn't be too expensive if there aren't fifty trillion vehicles in the graph...
    // ...and if there are, it's the player's fault for putting them there.
    distribution_graph::traverse( *this, distribution_graph::noop_visitor_veh,
                                  distribution_graph::noop_visitor_grid );

    if( check_environmental_effects ) {
        check_environmental_effects = do_environmental_effects();
    }

    // turrets which are enabled will try to reload and then automatically fire
    // Turrets which are disabled but have targets set are a special case
    for( auto e : turrets() ) {
        if( e->enabled || e->target.second != e->target.first ) {
            automatic_fire_turret( *e );
        }
    }

    if( velocity < 0 ) {
        beeper_sound();
    }
}

void vehicle::dump_items_from_part( const size_t index )
{
    vehicle_part &vp = parts[ index ];
    for( detached_ptr<item> &e : vp.items.clear() ) {
        g->m.add_item_or_charges( global_part_pos3( vp ), std::move( e ) );
    }
}

bool vehicle::decrement_summon_timer()
{
    if( !summon_time_limit ) {
        return false;
    }
    if( *summon_time_limit <= 0_turns ) {
        for( const vpart_reference &vp : get_all_parts() ) {
            const size_t p = vp.part_index();
            dump_items_from_part( p );
        }
        if( g->u.sees( global_pos3() ) ) {
            add_msg( m_info, _( "Your %s winks out of existence." ), name );
        }
        g->m.destroy_vehicle( this );
        return true;
    } else {
        *summon_time_limit -= 1_turns;
    }
    return false;
}

void vehicle::suspend_refresh()
{
    // disable refresh and cache recalculation
    no_refresh = true;
    mass_dirty = false;
    mass_center_precalc_dirty = false;
    mass_center_no_precalc_dirty = false;
    coeff_rolling_dirty = false;
    coeff_air_dirty = false;
    coeff_water_dirty = false;
    coeff_air_changed = false;
}

void vehicle::enable_refresh()
{
    // force all caches to recalculate
    no_refresh = false;
    mass_dirty = true;
    mass_center_precalc_dirty = true;
    mass_center_no_precalc_dirty = true;
    coeff_rolling_dirty = true;
    coeff_air_dirty = true;
    coeff_water_dirty = true;
    coeff_air_changed = true;
    refresh();
}

/**
 * Refreshes all caches and refinds all parts. Used after the vehicle has had a part added or removed.
 * Makes indices of different part types so they're easy to find. Also calculates power drain.
 */
void vehicle::refresh()
{
    if( no_refresh ) {
        return;
    }

    alternators.clear();
    engines.clear();
    reactors.clear();
    solar_panels.clear();
    wind_turbines.clear();
    sails.clear();
    water_wheels.clear();
    funnels.clear();
    emitters.clear();
    relative_parts.clear();
    loose_parts.clear();
    wheelcache.clear();
    rail_wheelcache.clear();
    rotors.clear();
    wings.clear();
    propellers.clear();
    droppers.clear();
    balloons.clear();
    steering.clear();
    speciality.clear();
    floating.clear();
    alternator_load = 0;
    extra_drag = 0;
    rail_profile.clear();

    // Used to sort part list so it displays properly when examining
    struct sort_veh_part_vector {
        vehicle *veh;
        bool operator()( const int p1, const int p2 ) {
            return veh->part_info( p1 ).list_order < veh->part_info( p2 ).list_order;
        }
    } svpv = { this };

    mount_min.x = 123;
    mount_min.y = 123;
    mount_max.x = -123;
    mount_max.y = -123;

    bool refresh_done = false;

    // Main loop over all vehicle parts.
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        const vpart_info &vpi = vp.info();
        if( vp.part().removed ) {
            continue;
        }
        refresh_done = true;

        // Build map of point -> all parts in that point
        const point pt = vp.mount();
        mount_min.x = std::min( mount_min.x, pt.x );
        mount_min.y = std::min( mount_min.y, pt.y );
        mount_max.x = std::max( mount_max.x, pt.x );
        mount_max.y = std::max( mount_max.y, pt.y );

        // This will keep the parts at point pt sorted
        std::vector<int>::iterator vii = std::lower_bound( relative_parts[pt].begin(),
                                         relative_parts[pt].end(),
                                         static_cast<int>( p ), svpv );
        relative_parts[pt].insert( vii, p );

        if( vpi.has_flag( VPFLAG_FLOATS ) ) {
            floating.push_back( p );
        }

        if( vp.part().is_unavailable() ) {
            continue;
        }
        if( vpi.has_flag( VPFLAG_ALTERNATOR ) ) {
            alternators.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_ENGINE ) ) {
            engines.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_REACTOR ) ) {
            reactors.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_SOLAR_PANEL ) ) {
            solar_panels.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_ROTOR ) ) {
            rotors.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_PROPELLER ) ) {
            propellers.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_WING ) ) {
            wings.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_BALLOON ) ) {
            balloons.push_back( p );
        }

        if( vpi.has_flag( VPFLAG_DROPPER ) ) {
            droppers.push_back( p );
        }
        if( vpi.has_flag( "WIND_TURBINE" ) ) {
            wind_turbines.push_back( p );
        }
        if( vpi.has_flag( "WIND_POWERED" ) ) {
            sails.push_back( p );
        }
        if( vpi.has_flag( "WATER_WHEEL" ) ) {
            water_wheels.push_back( p );
        }
        if( vpi.has_flag( "FUNNEL" ) ) {
            funnels.push_back( p );
        }
        if( vpi.has_flag( "UNMOUNT_ON_MOVE" ) ) {
            loose_parts.push_back( p );
        }
        if( vpi.has_flag( "EMITTER" ) ) {
            emitters.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_WHEEL ) ) {
            wheelcache.push_back( p );
        }
        if( vpi.has_flag( VPFLAG_RAIL ) ) {
            rail_wheelcache.push_back( p );

            const int rail_pos = vp.mount().y;
            const auto it = std::find( rail_profile.cbegin(), rail_profile.cend(), rail_pos );
            if( it == rail_profile.cend() ) {
                rail_profile.push_back( rail_pos );
            }
        }
        if( ( vpi.has_flag( "STEERABLE" ) && part_with_feature( pt, "STEERABLE", true ) != -1 ) ||
            vpi.has_flag( "TRACKED" ) ) {
            // TRACKED contributes to steering effectiveness but
            //  (a) doesn't count as a steering axle for install difficulty
            //  (b) still contributes to drag for the center of steering calculation
            steering.push_back( p );
        }
        if( vpi.has_flag( "SECURITY" ) ) {
            speciality.push_back( p );
        }
        if( vp.part().enabled && vpi.has_flag( "EXTRA_DRAG" ) ) {
            extra_drag += vpi.power;
        }
        if( vpi.has_flag( "EXTRA_DRAG" ) && ( vpi.has_flag( "WIND_TURBINE" ) ||
                                              vpi.has_flag( "WATER_WHEEL" ) ) ) {
            extra_drag += vpi.power;
        }
        if( camera_on && vpi.has_flag( "CAMERA" ) ) {
            vp.part().enabled = true;
        } else if( !camera_on && vpi.has_flag( "CAMERA" ) ) {
            vp.part().enabled = false;
        }
        if( vpi.has_flag( "TURRET" ) && !has_part( global_part_pos3( vp.part() ), "TURRET_CONTROLS" ) ) {
            vp.part().enabled = false;
        }
    }

    front_left.x = mount_max.x;
    front_left.y = mount_min.y;
    front_right = mount_max;

    if( !refresh_done ) {
        mount_min = mount_max = point_zero;
    }

    refresh_position();

    check_environmental_effects = true;
    insides_dirty = true;
    zones_dirty = true;
    invalidate_mass();
}

void vehicle::refresh_position()
{
    if( !parts.empty() ) {
        precalc_mounts( 0, pivot_rotation[0], pivot_anchor[0] );
        if( attached ) {
            adjust_zlevel();
            shift_zlevel();
        }
    }
}

point vehicle::pivot_point() const
{
    if( pivot_dirty ) {
        refresh_pivot();
    }

    return pivot_cache;
}

void vehicle::refresh_pivot() const
{
    // Const method, but messes with mutable fields
    pivot_dirty = false;

    if( wheelcache.empty() || !valid_wheel_config() ) {
        // No usable wheels, use CoM (dragging)
        pivot_cache = local_center_of_mass();
        return;
    }

    // The model here is:
    //
    //  We are trying to rotate around some point (xc,yc)
    //  This produces a friction force / moment from each wheel resisting the
    //  rotation. We want to find the point that minimizes that resistance.
    //
    //  For a given wheel w at (xw,yw), find:
    //   weight(w): a scaling factor for the friction force based on wheel
    //              size, brokenness, steerability/orientation
    //   center_dist: the distance from (xw,yw) to (xc,yc)
    //   centerline_angle: the angle between the X axis and a line through
    //                     (xw,yw) and (xc,yc)
    //
    //  Decompose the force into two components, assuming that the wheel is
    //  aligned along the X axis and we want to apply different weightings to
    //  the in-line vs perpendicular parts of the force:
    //
    //   Resistance force in line with the wheel (X axis)
    //    Fi = weightI(w) * center_dist * sin(centerline_angle)
    //   Resistance force perpendicular to the wheel (Y axis):
    //    Fp = weightP(w) * center_dist * cos(centerline_angle);
    //
    //  Then find the moment that these two forces would apply around (xc,yc)
    //    moment(w) = center_dist * cos(centerline_angle) * Fi +
    //                center_dist * sin(centerline_angle) * Fp
    //
    //  Note that:
    //    cos(centerline_angle) = (xw-xc) / center_dist
    //    sin(centerline_angle) = (yw-yc) / center_dist
    // -> moment(w) = weightP(w)*(xw-xc)^2 + weightI(w)*(yw-yc)^2
    //              = weightP(w)*xc^2 - 2*weightP(w)*xc*xw + weightP(w)*xw^2 +
    //                weightI(w)*yc^2 - 2*weightI(w)*yc*yw + weightI(w)*yw^2
    //
    //  which happily means that the X and Y axes can be handled independently.
    //  We want to minimize sum(moment(w)) due to wheels w=0,1,..., which
    //  occurs when:
    //
    //    sum( 2*xc*weightP(w) - 2*weightP(w)*xw ) = 0
    //     -> xc = (weightP(0)*x0 + weightP(1)*x1 + ...) /
    //             (weightP(0) + weightP(1) + ...)
    //    sum( 2*yc*weightI(w) - 2*weightI(w)*yw ) = 0
    //     -> yc = (weightI(0)*y0 + weightI(1)*y1 + ...) /
    //             (weightI(0) + weightI(1) + ...)
    //
    // so it turns into a fairly simple weighted average of the wheel positions.

    float xc_numerator = 0;
    float xc_denominator = 0;
    float yc_numerator = 0;
    float yc_denominator = 0;

    for( int p : wheelcache ) {
        const auto &wheel = parts[p];

        // TODO: load on tire?
        int contact_area = wheel.wheel_area();
        float weight_i;  // weighting for the in-line part
        float weight_p;  // weighting for the perpendicular part
        if( wheel.is_broken() ) {
            // broken wheels don't roll on either axis
            weight_i = contact_area * 2.0;
            weight_p = contact_area * 2.0;
        } else if( part_with_feature( wheel.mount, "STEERABLE", true ) != -1 ) {
            // Unbroken steerable wheels can handle motion on both axes
            // (but roll a little more easily inline)
            weight_i = contact_area * 0.1;
            weight_p = contact_area * 0.2;
        } else {
            // Regular wheels resist perpendicular motion
            weight_i = contact_area * 0.1;
            weight_p = contact_area;
        }

        xc_numerator += weight_p * wheel.mount.x;
        yc_numerator += weight_i * wheel.mount.y;
        xc_denominator += weight_p;
        yc_denominator += weight_i;
    }

    if( xc_denominator < 0.1 || yc_denominator < 0.1 ) {
        debugmsg( "vehicle::refresh_pivot had a bad weight: xc=%.3f/%.3f yc=%.3f/%.3f",
                  xc_numerator, xc_denominator, yc_numerator, yc_denominator );
        pivot_cache = local_center_of_mass();
    } else {
        pivot_cache.x = std::round( xc_numerator / xc_denominator );
        pivot_cache.y = std::round( yc_numerator / yc_denominator );
    }
}

void vehicle::do_towing_move()
{
    if( !no_towing_slack() || velocity <= 0 ) {
        return;
    }
    bool invalidate = false;
    if( !tow_data.get_towed() ) {
        debugmsg( "tried to do towing move, but no towed vehicle!" );
        invalidate = true;
    }
    const int tow_index = get_tow_part();
    if( tow_index == -1 ) {
        debugmsg( "tried to do towing move, but no tow part" );
        invalidate = true;
    }
    vehicle *towed_veh = tow_data.get_towed();
    if( !towed_veh ) {
        debugmsg( "tried to do towing move, but towed vehicle dosnt exist." );
        invalidate_towing();
        return;
    }
    const int other_tow_index = towed_veh->get_tow_part();
    if( other_tow_index == -1 ) {
        debugmsg( "tried to do towing move but towed vehicle has no towing part" );
        invalidate = true;
    }
    if( towed_veh->global_pos3().z != global_pos3().z ) {
        // how the hellicopter did this happen?
        // yes, this can happen when towing over a bridge (see #47293)
        invalidate = true;
        add_msg( m_info, _( "A towing cable snaps off of %s." ), tow_data.get_towed()->disp_name() );
    }
    if( invalidate ) {
        invalidate_towing( true );
        return;
    }
    const tripoint tower_tow_point = g->m.getabs( global_part_pos3( tow_index ) );
    const tripoint towed_tow_point = g->m.getabs( towed_veh->global_part_pos3( other_tow_index ) );
    // same as above, but where the pulling vehicle is pulling from
    units::angle towing_veh_angle = towed_veh->get_angle_from_targ( tower_tow_point );
    const bool reverse = towed_veh->tow_data.tow_direction == TOW_BACK;
    int accel_y = 0;
    tripoint vehpos = global_square_location().raw();
    int turn_x = get_turn_from_angle( towing_veh_angle, vehpos, tower_tow_point, reverse );
    if( rl_dist( towed_tow_point, tower_tow_point ) < 6 ) {
        accel_y = reverse ? -1 : 1;
    }
    if( towed_veh->velocity <= velocity && rl_dist( towed_tow_point, tower_tow_point ) >= 7 ) {
        accel_y = reverse ? 1 : -1;
    }
    if( rl_dist( towed_tow_point, tower_tow_point ) >= 12 ) {
        towed_veh->velocity = velocity * 1.8;
        if( reverse ) {
            towed_veh->velocity = -towed_veh->velocity;
        }
    } else {
        towed_veh->velocity = reverse ? -velocity : velocity;
    }
    if( towed_veh->tow_data.tow_direction == TOW_FRONT ) {
        towed_veh->selfdrive( point( turn_x, accel_y ) );
    } else if( towed_veh->tow_data.tow_direction == TOW_BACK ) {
        accel_y = 10;
        towed_veh->selfdrive( point( turn_x, accel_y ) );
    } else {
        towed_veh->skidding = true;
        std::vector<tripoint> lineto = line_to( g->m.getlocal( towed_tow_point ),
                                                g->m.getlocal( tower_tow_point ) );
        tripoint nearby_destination;
        if( lineto.size() >= 2 ) {
            nearby_destination = lineto[1];
        } else {
            nearby_destination = tower_tow_point;
        }
        const int destination_delta_x = g->m.getlocal( tower_tow_point ).x - nearby_destination.x;
        const int destination_delta_y = g->m.getlocal( tower_tow_point ).y - nearby_destination.y;
        const int destination_delta_z = towed_veh->global_pos3().z;
        const tripoint move_destination( clamp( destination_delta_x, -1, 1 ),
                                         clamp( destination_delta_y, -1, 1 ),
                                         clamp( destination_delta_z, -1, 1 ) );
        g->m.move_vehicle( *towed_veh, move_destination, towed_veh->face );
        towed_veh->move = tileray( point( destination_delta_x, destination_delta_y ) );
    }

}

bool vehicle::is_external_part( const tripoint &part_pt ) const
{
    for( const tripoint &elem : g->m.points_in_radius( part_pt, 1 ) ) {
        const optional_vpart_position vp = g->m.veh_at( elem );
        if( !vp ) {
            return true;
        }
        if( vp && &vp->vehicle() != this ) {
            return true;
        }
    }
    return false;
}

bool vehicle::is_towing() const
{
    bool ret = false;
    if( !tow_data.get_towed() ) {
        return ret;
    } else {
        if( !tow_data.get_towed()->tow_data.get_towed_by() ) {
            debugmsg( "vehicle %s is towing, but the towed vehicle has no tower defined", name );
            return ret;
        }
        ret = true;
    }
    return ret;
}

bool vehicle::is_towed() const
{
    bool ret = false;
    if( !tow_data.get_towed_by() ) {
        return ret;
    } else {
        if( !tow_data.get_towed_by()->tow_data.get_towed() ) {
            debugmsg( "vehicle %s is marked as towed, but the tower vehicle has no towed defined", name );
            return ret;
        }
        ret = true;
    }
    return ret;
}

int vehicle::get_tow_part() const
{
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        if( vp.part().removed ) {
            continue;
        }

        if( part_with_feature( p, "TOW_CABLE", true ) >= 0 && vp.part().is_available() ) {
            return p;
        }
    }
    return -1;
}

bool vehicle::has_tow_attached() const
{
    bool ret = false;
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        if( vp.part().removed ) {
            continue;
        }

        if( part_with_feature( p, "TOW_CABLE", true ) >= 0 && vp.part().is_available() ) {
            ret = true;
            break;
        }
    }
    return ret;
}

void vehicle::set_tow_directions()
{
    const int length = mount_max.x - mount_min.x + 1;
    const point mount_of_tow = parts[get_tow_part()].mount;
    const point normalized_tow_mount = point( std::abs( mount_of_tow.x - mount_min.x ),
                                       std::abs( mount_of_tow.y - mount_min.y ) );
    if( length >= 3 ) {
        const int trisect = length / 3;
        if( normalized_tow_mount.x <= trisect ) {
            tow_data.tow_direction = TOW_BACK;
        } else if( normalized_tow_mount.x > trisect && normalized_tow_mount.x <= trisect * 2 ) {
            tow_data.tow_direction = TOW_SIDE;
        } else {
            tow_data.tow_direction = TOW_FRONT;
        }
    } else {
        // its a small vehicle, no danger if it flips around.
        tow_data.tow_direction = TOW_FRONT;
    }
}

bool towing_data::set_towing( vehicle *tower_veh, vehicle *towed_veh )
{
    if( !towed_veh || !tower_veh ) {
        return false;
    }
    towed_veh->tow_data.towed_by = tower_veh;
    tower_veh->tow_data.towing = towed_veh;
    tower_veh->set_tow_directions();
    towed_veh->set_tow_directions();
    return true;
}

void vehicle::invalidate_towing( bool first_vehicle )
{
    if( !is_towing() && !is_towed() ) {
        return;
    }
    vehicle *other_veh = nullptr;
    if( is_towing() ) {
        other_veh = tow_data.get_towed();
    } else if( is_towed() ) {
        other_veh = tow_data.get_towed_by();
    }
    if( other_veh && first_vehicle ) {
        other_veh->invalidate_towing();
    }
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        if( vp.part().removed ) {
            continue;
        }

        if( part_with_feature( p, "TOW_CABLE", true ) >= 0 ) {
            if( first_vehicle ) {
                vehicle_part *part = &parts[part_with_feature( p, "TOW_CABLE", true )];
                g->m.add_item_or_charges( global_part_pos3( *part ),  part->properties_to_item() );
            }
            //TODO!: check part removal in general, what happens to their base?
            remove_part( part_with_feature( p, "TOW_CABLE", true ) );
            break;
        }
    }
    tow_data.clear_towing();
}

// to be called on the towed vehicle
bool vehicle::tow_cable_too_far() const
{
    if( !tow_data.get_towed_by() ) {
        debugmsg( "checking tow cable length on a vehicle that has no towing vehicle" );
        return false;
    }
    int index = get_tow_part();
    if( index == -1 ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    tripoint towing_point = g->m.getabs( global_part_pos3( index ) );
    if( !tow_data.get_towed_by()->tow_data.get_towed() ) {
        debugmsg( "vehicle %s has data for a towing vehicle, but that towing vehicle does not have %s listed as towed",
                  disp_name(), disp_name() );
        return false;
    }
    int other_index = tow_data.get_towed_by()->get_tow_part();
    if( other_index == -1 ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    tripoint towed_point = g->m.getabs( tow_data.get_towed_by()->global_part_pos3( other_index ) );
    if( towing_point == tripoint_zero || towed_point == tripoint_zero ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    return rl_dist( towing_point, towed_point ) >= 25;
}

// the towing cable only starts pulling at a certain distance between the vehicles
// to be called on the towing vehicle
bool vehicle::no_towing_slack() const
{
    if( !tow_data.get_towed() ) {
        return false;
    }
    int index = get_tow_part();
    if( index == -1 ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    tripoint towing_point = g->m.getabs( global_part_pos3( index ) );
    if( !tow_data.get_towed()->tow_data.get_towed_by() ) {
        debugmsg( "vehicle %s has data for a towed vehicle, but that towed vehicle does not have %s listed as tower",
                  disp_name(), disp_name() );
        return false;
    }
    int other_index = tow_data.get_towed()->get_tow_part();
    if( other_index == -1 ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    tripoint towed_point = g->m.getabs( tow_data.get_towed()->global_part_pos3( other_index ) );
    if( towing_point == tripoint_zero || towed_point == tripoint_zero ) {
        debugmsg( "towing data exists but no towing part" );
        return false;
    }
    return rl_dist( towing_point, towed_point ) >= 8;

}

void vehicle::remove_remote_part( int part_num )
{
    if( parts[part_num].has_flag( vehicle_part::targets_grid ) ) {
        vehicle_connector_tile *connector =
            active_tiles::furn_at<vehicle_connector_tile>( tripoint_abs_ms( parts[part_num].target.second ) );
        if( connector != nullptr ) {
            auto &vehs = connector->connected_vehicles;
            auto iter = std::find( vehs.begin(), vehs.end(), tripoint_abs_ms( g->m.getabs( global_pos3() ) ) );
            if( iter != vehs.end() ) {
                vehs.erase( iter );
            }
        }
        return;
    }
    auto veh = find_vehicle( parts[part_num].target.second );

    // If the target vehicle is still there, ask it to remove its part
    if( veh != nullptr ) {
        const tripoint local_abs = g->m.getabs( global_part_pos3( part_num ) );

        for( size_t j = 0; j < veh->loose_parts.size(); j++ ) {
            int remote_partnum = veh->loose_parts[j];
            auto remote_part = &veh->parts[remote_partnum];

            if( veh->part_flag( remote_partnum, "POWER_TRANSFER" ) && remote_part->target.first == local_abs ) {
                veh->remove_part( remote_partnum );
                return;
            }
        }
    }
}

void vehicle::shed_loose_parts()
{
    // remove_part rebuilds the loose_parts vector, when all of those parts have been removed,
    // it will stay empty.
    while( !loose_parts.empty() ) {
        const int elem = loose_parts.front();
        if( part_flag( elem, "POWER_TRANSFER" ) ) {
            remove_remote_part( elem );
        }
        if( is_towing() || is_towed() ) {
            vehicle *other_veh = is_towing() ? tow_data.get_towed() : tow_data.get_towed_by();
            if( other_veh ) {
                other_veh->remove_part( other_veh->part_with_feature( other_veh->get_tow_part(), "TOW_CABLE",
                                        true ) );
                other_veh->tow_data.clear_towing();
            }
            tow_data.clear_towing();
        }
        auto part = &parts[elem];
        if( !magic ) {
            g->m.add_item_or_charges( global_part_pos3( *part ), part->properties_to_item() );
        }

        remove_part( elem );
    }
}

bool vehicle::enclosed_at( const tripoint &pos )
{
    refresh_insides();
    std::vector<vehicle_part *> parts_here = get_parts_at( pos, "BOARDABLE",
            part_status_flag::working );
    if( !parts_here.empty() ) {
        return parts_here.front()->inside;
    }
    return false;
}

void vehicle::refresh_insides()
{
    if( !insides_dirty ) {
        return;
    }
    insides_dirty = false;
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        if( vp.part().removed ) {
            continue;
        }
        /* If there's no roof, or there is a roof but it's broken, it's outside.
         * (Use short-circuiting && so broken frames don't screw this up) */
        if( !( part_with_feature( p, "ROOF", true ) >= 0 && vp.part().is_available() ) ) {
            vp.part().inside = false;
            continue;
        }

        // inside if not otherwise
        parts[p].inside = true;
        // let's check four neighbor parts
        for( point offset : four_adjacent_offsets ) {
            point near_mount = parts[ p ].mount + offset;
            std::vector<int> parts_n3ar = parts_at_relative( near_mount, true );
            // if we aren't covered from sides, the roof at p won't save us
            bool cover = false;
            for( auto &j : parts_n3ar ) {
                // another roof -- cover
                if( part_flag( j, "ROOF" ) && parts[ j ].is_available() ) {
                    cover = true;
                    break;
                } else if( part_flag( j, "OBSTACLE" ) && parts[ j ].is_available() ) {
                    // found an obstacle, like board or windshield or door
                    if( parts[j].inside || ( part_flag( j, "OPENABLE" ) && parts[j].open ) ) {
                        // door and it's open -- can't cover
                        continue;
                    }
                    cover = true;
                    break;
                }
                //Otherwise keep looking, there might be another part in that square
            }
            if( !cover ) {
                vp.part().inside = false;
                break;
            }
        }
    }
}

bool vpart_position::is_inside() const
{
    // TODO: this is a bit of a hack as refresh_insides has side effects
    // this should be called elsewhere and not in a function that intends to just query
    // it's also a no-op if the insides are up to date.
    vehicle().refresh_insides();
    return vehicle().part( part_index() ).inside;
}

void vehicle::unboard_all()
{
    std::vector<int> bp = boarded_parts();
    for( auto &i : bp ) {
        g->m.unboard_vehicle( global_part_pos3( i ) );
    }
}

int vehicle::damage( int p, int dmg, damage_type type, bool aimed )
{
    if( dmg < 1 ) {
        return dmg;
    }

    std::vector<int> pl = parts_at_relative( parts[p].mount, true );
    if( pl.empty() ) {
        // We ran out of non removed parts at this location already.
        return dmg;
    }

    if( !aimed ) {
        bool found_obs = false;
        for( auto &i : pl ) {
            if( part_flag( i, "OBSTACLE" ) &&
                ( !part_flag( i, "OPENABLE" ) || !parts[i].open ) ) {
                found_obs = true;
                break;
            }
        }

        if( !found_obs ) { // not aimed at this tile and no obstacle here -- fly through
            return dmg;
        }
    }

    int target_part = part_info( p ).rotor_diameter() ? p : random_entry( pl );

    // door motor mechanism is protected by closed doors
    if( part_flag( target_part, "DOOR_MOTOR" ) ) {
        // find the most strong openable that is not open
        int strongest_door_part = -1;
        int strongest_door_durability = INT_MIN;
        for( int part : pl ) {
            if( part_flag( part, "OPENABLE" ) && !parts[part].open ) {
                int door_durability = part_info( part ).durability;
                if( door_durability > strongest_door_durability ) {
                    strongest_door_part = part;
                    strongest_door_durability = door_durability;
                }
            }
        }

        // if we found a closed door, target it instead of the door_motor
        if( strongest_door_part != -1 ) {
            target_part = strongest_door_part;
        }
    }

    int damage_dealt;

    int armor_part = part_with_feature( p, "ARMOR", true );
    if( armor_part < 0 ) {
        // Not covered by armor -- damage part
        damage_dealt = damage_direct( target_part, dmg, type );
    } else {
        // Covered by armor -- hit both armor and part, but reduce damage by armor's reduction
        int protection = part_info( armor_part ).damage_reduction.type_resist( type );
        // Parts on roof aren't protected
        bool overhead = part_flag( target_part, "ROOF" ) || part_info( target_part ).location == "on_roof";
        // Calling damage_direct may remove the damaged part
        // completely, therefore the other index (target_part) becomes
        // wrong if target_part > armor_part.
        // Damaging the part with the higher index first is save,
        // as removing a part only changes indices after the
        // removed part.
        if( armor_part < target_part ) {
            damage_direct( target_part, overhead ? dmg : dmg - protection, type );
            damage_dealt = damage_direct( armor_part, dmg, type );
        } else {
            damage_dealt = damage_direct( armor_part, dmg, type );
            damage_direct( target_part, overhead ? dmg : dmg - protection, type );
        }
    }

    return damage_dealt;
}

void vehicle::damage_all( int dmg1, int dmg2, damage_type type, point impact )
{
    if( dmg2 < dmg1 ) {
        std::swap( dmg1, dmg2 );
    }
    if( dmg1 < 1 ) {
        return;
    }
    const float damage_min = std::abs( dmg1 );
    const float damage_max = std::abs( dmg2 );
    add_msg( m_debug, "Shock damage to vehicle of %.2f to %.2f", damage_min, damage_max );
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t p = vp.part_index();
        const vpart_info &shockpart = part_info( p );
        int distance = 1 + square_dist( vp.mount(), impact );
        if( distance > 1 ) {
            int net_dmg = rng( dmg1, dmg2 ) / ( distance * distance );
            if( shockpart.location != part_location_structure ||
                !shockpart.has_flag( "PROTRUSION" ) ) {
                if( shockpart.has_flag( "SHOCK_IMMUNE" ) ) {
                    net_dmg = 0;
                    continue;
                }
                int shock_absorber = part_with_feature( p, "SHOCK_ABSORBER", true );
                if( shock_absorber >= 0 ) {
                    net_dmg = std::max( 0.0f, net_dmg - ( parts[ shock_absorber ].info().bonus ) -
                                        shockpart.damage_reduction.type_resist( type ) );
                }
                if( shockpart.has_flag( "SHOCK_RESISTANT" ) ) {
                    float damage_resist = 0;
                    for( const int elem : all_parts_at_location( shockpart.location ) ) {
                        //Original intent was to find the frame that the part was mounted on and grab that objects resistance, but instead we will go with half the largest damage resist in the stack.
                        damage_resist = std::max( damage_resist, part_info( elem ).damage_reduction.type_resist( type ) );
                    }
                    damage_resist = damage_resist / 2;

                    add_msg( m_debug, "%1s inherited %.1f damage resistance!", shockpart.name(), damage_resist );
                    net_dmg = std::max( 0.0f, net_dmg - damage_resist );
                }
            }
            if( net_dmg > part_info( p ).damage_reduction.type_resist( type ) ) {
                damage_direct( p, net_dmg, type );
                add_msg( m_debug, _( "%1s took %.1f damage from shock." ), part_info( p ).name(), 1.0f * net_dmg );
            }

        }
    }
}

/**
 * Shifts all parts of the vehicle by the given amounts, and then shifts the
 * vehicle itself in the opposite direction. The end result is that the vehicle
 * appears to have not moved. Useful for re-zeroing a vehicle to ensure that a
 * (0, 0) part is always present.
 * @param delta How much to shift along each axis
 */
void vehicle::shift_parts( point delta )
{
    for( auto &elem : parts ) {
        elem.mount -= delta;
    }

    decltype( labels ) new_labels;
    for( auto &l : labels ) {
        new_labels.insert( label( l - delta, l.text ) );
    }
    labels = new_labels;

    decltype( loot_zones ) new_zones;
    for( auto const &z : loot_zones ) {
        new_zones.emplace( z.first - delta, z.second );
    }
    loot_zones = new_zones;

    pivot_anchor[0] -= delta;
    refresh();
    //Need to also update the map after this
    g->m.reset_vehicle_cache( );
}

/**
 * Detect if the vehicle is currently missing a 0,0 part, and
 * adjust if necessary.
 * @return bool true if the shift was needed.
 */
bool vehicle::shift_if_needed()
{
    std::vector<int> vehicle_origin = parts_at_relative( point_zero, true );
    if( !vehicle_origin.empty() && !parts[ vehicle_origin[ 0 ] ].removed ) {
        // Shifting is not needed.
        return false;
    }
    //Find a frame, any frame, to shift to
    for( const vpart_reference &vp : get_all_parts() ) {
        if( vp.info().location == "structure"
            && !vp.has_feature( "PROTRUSION" )
            && !vp.part().removed ) {
            shift_parts( vp.mount() );
            refresh();
            return true;
        }
    }
    // There are only parts with PROTRUSION left, choose one of them.
    for( const vpart_reference &vp : get_all_parts() ) {
        if( !vp.part().removed ) {
            shift_parts( vp.mount() );
            refresh();
            return true;
        }
    }
    return false;
}

int vehicle::break_off( int p, int dmg )
{
    /* Already-destroyed part - chance it could be torn off into pieces.
     * Chance increases with damage, and decreases with part max durability
     * (so lights, etc are easily removed; frames and plating not so much) */
    if( rng( 0, part_info( p ).durability / 10 ) >= dmg ) {
        return dmg;
    }
    const tripoint pos = global_part_pos3( p );
    const auto scatter_parts = [&]( const vehicle_part & pt ) {
        for( detached_ptr<item> &piece : pt.pieces_for_broken_part() ) {
            // inside the loop, so each piece goes to a different place
            // TODO: this may spawn items behind a wall
            const tripoint where = random_entry( g->m.points_in_radius( pos, SCATTER_DISTANCE ) );
            // TODO: balance audit, ensure that less pieces are generated than one would need
            // to build the component (smash a vehicle box that took 10 lumps of steel,
            // find 12 steel lumps scattered after atom-smashing it with a tree trunk)
            if( !magic ) {
                g->m.add_item_or_charges( where, std::move( piece ) );
            }
        }
    };
    if( part_info( p ).location == part_location_structure ) {
        // For structural parts, remove other parts first
        std::vector<int> parts_in_square = parts_at_relative( parts[p].mount, true );
        for( int index = parts_in_square.size() - 1; index >= 0; index-- ) {
            // Ignore the frame being destroyed
            if( parts_in_square[index] == p ) {
                continue;
            }

            if( parts[ parts_in_square[ index ] ].is_broken() ) {
                // Tearing off a broken part - break it up
                if( g->u.sees( pos ) ) {
                    add_msg( m_bad, _( "The %s's %s breaks into pieces!" ), name,
                             parts[ parts_in_square[ index ] ].name() );
                }
                scatter_parts( parts[parts_in_square[index]] );
            } else {
                // Intact (but possibly damaged) part - remove it in one piece
                if( g->u.sees( pos ) ) {
                    add_msg( m_bad, _( "The %1$s's %2$s is torn off!" ), name,
                             parts[ parts_in_square[ index ] ].name() );
                }
                if( !magic ) {
                    g->m.add_item_or_charges( pos, parts[parts_in_square[index]].properties_to_item() );
                }
            }
            remove_part( parts_in_square[index] );
        }
        // After clearing the frame, remove it.
        if( g->u.sees( pos ) ) {
            add_msg( m_bad, _( "The %1$s's %2$s is destroyed!" ), name, parts[ p ].name() );
        }
        scatter_parts( parts[p] );
        remove_part( p );
        find_and_split_vehicles( p );
    } else {
        //Just break it off
        if( g->u.sees( pos ) ) {
            add_msg( m_bad, _( "The %1$s's %2$s is destroyed!" ), name, parts[ p ].name() );
        }

        scatter_parts( parts[p] );
        remove_part( p );
    }

    return dmg;
}

bool vehicle::explode_fuel( int p, damage_type type )
{
    const itype_id &ft = part_info( p ).fuel_type;
    item &fuel = *item::spawn_temporary( ft );
    if( !fuel.has_explosion_data() ) {
        return false;
    }
    fuel_explosion data = fuel.get_explosion_data();

    if( parts[ p ].is_broken() ) {
        leak_fuel( parts[ p ] );
    }

    int explosion_chance = type == DT_HEAT ? data.explosion_chance_hot : data.explosion_chance_cold;
    if( one_in( explosion_chance ) ) {
        g->events().send<event_type::fuel_tank_explodes>( name );
        const int pow = 120 * ( 1 - std::exp( data.explosion_factor / -5000 *
                                              ( parts[p].ammo_remaining() * data.fuel_size_factor ) ) );
        //debugmsg( "damage check dmg=%d pow=%d amount=%d", dmg, pow, parts[p].amount );

        explosion_handler::explosion( global_part_pos3( p ), nullptr, pow, 0.7, data.fiery_explosion );
        mod_hp( parts[p], 0 - parts[ p ].hp(), DT_HEAT );
        parts[p].ammo_unset();
    }

    return true;
}

unsigned int vehicle::hits_to_destroy( int p, int dmg, damage_type type ) const
{
    const int armor_part = part_with_feature( p, VPFLAG_ARMOR, true );
    const bool is_armor_considered = !(
                                         armor_part < 0 ||
                                         part_flag( p, VPFLAG_ROOF ) ||
                                         part_info( p ).location == "on_roof"
                                     );


    const int part_hp = parts[p].hp();
    const int part_damage_reduction = part_info( p ).damage_reduction.type_resist( type );
    const int part_threshold_damage = std::clamp( part_info( p ).durability / 10, 1, 20 );

    const int part_dmg_without_armor = dmg - part_damage_reduction;

    // Easiest case: part will not get destroyed, period
    if( part_dmg_without_armor <= 0 ||
        ( type != DT_TRUE &&
          part_dmg_without_armor < part_threshold_damage ) ) {
        return 0;
    }

    // Easy case: part unprotected and will be destroyed
    if( !is_armor_considered ) {
        const int part_htd = part_hp / part_dmg_without_armor +
                             ( part_hp % part_dmg_without_armor > 0 );
        return part_htd;
    }

    const int armor_hp = parts[armor_part].hp();
    const int armor_damage_reduction = part_info( armor_part ).damage_reduction.type_resist( type );
    const int armor_threshold_damage = std::clamp( part_info( armor_part ).durability / 10, 1, 20 );
    const int armor_dmg = dmg - armor_damage_reduction;

    // First, determine how long armor will remain for
    const int armor_htd = armor_dmg <= 0 || ( type != DT_TRUE && armor_dmg < armor_threshold_damage ) ?
                          INT_MAX :
                          armor_hp / armor_dmg + ( armor_hp % armor_dmg > 0 );

    const int part_dmg_with_armor = part_dmg_without_armor - armor_damage_reduction;
    // How long will the part remain with armor unbroken?
    const int part_htd_with_armor = ( part_dmg_with_armor <= 0 ||
                                      ( type != DT_TRUE && part_dmg_with_armor < part_threshold_damage ) ) ?
                                    INT_MAX :
                                    part_hp / part_dmg_with_armor + ( part_hp % part_dmg_with_armor  > 0 );

    // Part gets destroyed before armor does
    if( part_htd_with_armor <= armor_htd ) {
        return part_htd_with_armor;
    }

    // Armor gets destroyed before part does
    const int part_hp_after_armor = part_hp - armor_htd * std::max( part_dmg_with_armor, 0 );
    const int part_htd_after_armor = part_hp_after_armor / part_dmg_without_armor +
                                     ( part_hp_after_armor % part_dmg_without_armor  > 0 );

    return armor_htd + part_htd_after_armor;
}

int vehicle::damage_direct( int p, int dmg, damage_type type )
{
    map &here = get_map();
    // Make sure p is within range and hasn't been removed already
    if( ( static_cast<size_t>( p ) >= parts.size() ) || parts[p].removed ||
        !here.inbounds( global_part_pos3( p ) ) ) {
        return dmg;
    }
    // If auto-driving and damage happens, bail out
    if( is_autodriving ) {
        stop_autodriving();
    }
    here.set_memory_seen_cache_dirty( global_part_pos3( p ) );
    if( parts[p].is_broken() ) {
        return break_off( p, dmg );
    }

    int tsh = std::min( 20, part_info( p ).durability / 10 );
    if( dmg < tsh && type != DT_TRUE ) {
        if( type == DT_HEAT && parts[p].is_fuel_store() ) {
            explode_fuel( p, type );
        }

        return 0;
    }

    dmg -= std::min<int>( dmg, part_info( p ).damage_reduction.type_resist( type ) );
    int dres = dmg - parts[p].hp();
    if( mod_hp( parts[ p ], 0 - dmg, type ) ) {
        insides_dirty = true;
        pivot_dirty = true;

        // destroyed parts lose any contained fuels, battery charges or ammo
        leak_fuel( parts [ p ] );

        for( auto &e : parts[p].items.clear() ) {
            g->m.add_item_or_charges( global_part_pos3( p ), std::move( e ) );
        }

        invalidate_mass();
        coeff_air_changed = true;

        // refresh cache in case the broken part has changed the status
        refresh();
    }

    if( parts[p].is_fuel_store() ) {
        explode_fuel( p, type );
    } else if( parts[ p ].is_broken() && part_flag( p, "UNMOUNT_ON_DAMAGE" ) ) {
        here.spawn_item( global_part_pos3( p ), part_info( p ).item, 1, 0, calendar::turn );
        monster *mon = get_pet( p );
        if( mon != nullptr && mon->has_effect( effect_harnessed ) ) {
            mon->remove_effect( effect_harnessed );
        }
        if( part_flag( p, "TOW_CABLE" ) ) {
            invalidate_towing( true );
        } else {
            remove_part( p );
        }
    }

    return std::max( dres, 0 );
}

void vehicle::leak_fuel( vehicle_part &pt )
{
    // only liquid fuels from non-empty tanks can leak out onto map tiles
    if( !pt.is_tank() || pt.ammo_remaining() <= 0 ) {
        return;
    }

    // leak in random directions but prefer closest tiles and avoid walls or other obstacles
    std::vector<tripoint> tiles = closest_points_first( global_part_pos3( pt ), 1 );
    tiles.erase( std::remove_if( tiles.begin(), tiles.end(), []( const tripoint & e ) {
        return !g->m.passable( e );
    } ), tiles.end() );

    // leak up to 1/3 of remaining fuel per iteration and continue until the part is empty
    const itype *fuel = &*pt.ammo_current();
    while( !tiles.empty() && pt.ammo_remaining() ) {
        int qty = pt.ammo_consume( rng( 0, std::max( pt.ammo_remaining() / 3, 1 ) ),
                                   global_part_pos3( pt ) );
        if( qty > 0 ) {
            g->m.add_item_or_charges( random_entry( tiles ), item::spawn( fuel, calendar::turn, qty ) );
        }
    }

    pt.ammo_unset();
}

std::map<itype_id, int> vehicle::fuels_left() const
{
    std::map<itype_id, int> result;
    for( const auto &p : parts ) {
        if( p.is_fuel_store() && !p.ammo_current().is_null() ) {
            result[ p.ammo_current() ] += p.ammo_remaining();
        }
    }
    return result;
}

bool vehicle::is_foldable() const
{
    for( const vpart_reference &vp : get_all_parts() ) {
        if( !vp.has_feature( "FOLDABLE" ) ) {
            return false;
        }
    }
    return true;
}

bool vehicle::restore( const std::string &data )
{
    std::istringstream veh_data( data );
    try {
        JsonIn json( veh_data );
        parts.clear();
        json.read( parts );
    } catch( const JsonError &e ) {
        debugmsg( "Error restoring vehicle: %s", e.c_str() );
        return false;
    }
    for( vehicle_part &part : parts ) {
        part.hack_id = get_next_hack_id();
    }
    refresh_locations_hack();
    refresh();
    face.init( 0_degrees );
    turn_dir = 0_degrees;
    turn( 0_degrees );
    precalc_mounts( 0, pivot_rotation[0], pivot_anchor[0] );
    precalc_mounts( 1, pivot_rotation[1], pivot_anchor[1] );
    last_update = calendar::turn;
    return true;
}

std::set<tripoint> &vehicle::get_points( const bool force_refresh )
{
    if( force_refresh || occupied_cache_time != calendar::turn ) {
        occupied_cache_time = calendar::turn;
        occupied_points.clear();
        for( const auto &p : parts ) {
            occupied_points.insert( global_part_pos3( p ) );
        }
    }

    return occupied_points;
}

vehicle_part &vpart_reference::part() const
{
    assert( part_index() < static_cast<size_t>( vehicle().part_count() ) );
    return vehicle().part( part_index() );
}

const vpart_info &vpart_reference::info() const
{
    return part().info();
}

player *vpart_reference::get_passenger() const
{
    return vehicle().get_passenger( part_index() );
}

point vpart_position::mount() const
{
    return vehicle().part( part_index() ).mount;
}

tripoint vpart_position::pos() const
{
    return vehicle().global_part_pos3( part_index() );
}

bool vpart_reference::has_feature( const std::string &f ) const
{
    return info().has_flag( f );
}

bool vpart_reference::has_feature( const vpart_bitflags f ) const
{
    return info().has_flag( f );
}

static bool is_sm_tile_over_water( const tripoint &real_global_pos )
{

    const tripoint smp = ms_to_sm_copy( real_global_pos );
    const point p( modulo( real_global_pos.x, SEEX ), modulo( real_global_pos.y, SEEY ) );
    auto sm = MAPBUFFER.lookup_submap( smp );
    if( sm == nullptr ) {
        debugmsg( "is_sm_tile_outside(): couldn't find submap %d,%d,%d", smp.x, smp.y, smp.z );
        return false;
    }

    if( p.x < 0 || p.x >= SEEX || p.y < 0 || p.y >= SEEY ) {
        debugmsg( "err %d,%d", p.x, p.y );
        return false;
    }

    return ( sm->get_ter( p ).obj().has_flag( TFLAG_CURRENT ) ||
             sm->get_furn( p ).obj().has_flag( TFLAG_CURRENT ) );
}

static bool is_sm_tile_outside( const tripoint &real_global_pos )
{

    const tripoint smp = ms_to_sm_copy( real_global_pos );
    const point p( modulo( real_global_pos.x, SEEX ), modulo( real_global_pos.y, SEEY ) );
    auto sm = MAPBUFFER.lookup_submap( smp );
    if( sm == nullptr ) {
        debugmsg( "is_sm_tile_outside(): couldn't find submap %d,%d,%d", smp.x, smp.y, smp.z );
        return false;
    }

    if( p.x < 0 || p.x >= SEEX || p.y < 0 || p.y >= SEEY ) {
        debugmsg( "err %d,%d", p.x, p.y );
        return false;
    }

    return !( sm->get_ter( p ).obj().has_flag( TFLAG_INDOORS ) ||
              sm->get_furn( p ).obj().has_flag( TFLAG_INDOORS ) );
}

void vehicle::update_time( const time_point &update_to )
{
    const time_point update_from = last_update;
    if( update_to < update_from ) {
        // Special case going backwards in time - that happens
        last_update = update_to;
        return;
    }

    if( update_to >= update_from && update_to - update_from < 1_minutes ) {
        // We don't need to check every turn
        return;
    }
    time_duration elapsed = update_to - last_update;
    last_update = update_to;

    if( sm_pos.z < 0 ) {
        return;
    }

    // Weather stuff, only for z-levels >= 0
    // TODO: Have it wash cars from blood?
    if( funnels.empty() && solar_panels.empty() && wind_turbines.empty() && water_wheels.empty() ) {
        return;
    }
    // Get one weather data set per vehicle, they don't differ much across vehicle area
    const weather_sum accum_weather = sum_conditions( update_from, update_to,
                                      global_square_location().raw() );
    // make some reference objects to use to check for reload
    const item *water = item::spawn_temporary( "water" );
    const item *water_clean = item::spawn_temporary( "water_clean" );

    for( int idx : funnels ) {
        const auto &pt = parts[idx];

        // we need an unbroken funnel mounted on the exterior of the vehicle
        if( pt.is_unavailable() || !is_sm_tile_outside( g->m.getabs( global_part_pos3( pt ) ) ) ) {
            continue;
        }

        // we need an empty tank (or one already containing water) below the funnel
        auto tank = std::find_if( parts.begin(), parts.end(), [&]( const vehicle_part & e ) {
            return pt.mount == e.mount && e.is_tank() &&
                   ( e.can_reload( water ) || e.can_reload( water_clean ) );
        } );

        if( tank == parts.end() ) {
            continue;
        }

        double area = std::pow( pt.info().size / units::legacy_volume_factor, 2 ) * M_PI;
        int qty = roll_remainder( funnel_charges_per_turn( area, accum_weather.rain_amount ) );
        int c_qty = qty + ( tank->can_reload( water_clean ) ?  tank->ammo_remaining() : 0 );
        int cost_to_purify = c_qty * itype_water_purifier->charges_to_use();

        if( qty > 0 ) {
            if( has_part( global_part_pos3( pt ), "WATER_PURIFIER", true ) &&
                ( fuel_left( itype_battery, true ) > cost_to_purify ) ) {
                tank->ammo_set( itype_water_clean, c_qty );
                discharge_battery( cost_to_purify );
            } else {
                tank->ammo_set( itype_water, tank->ammo_remaining() + qty );
            }
            invalidate_mass();
        }
    }

    if( !solar_panels.empty() ) {
        int epower_w = 0;
        for( int part : solar_panels ) {
            if( parts[ part ].is_unavailable() ) {
                continue;
            }

            if( !is_sm_tile_outside( g->m.getabs( global_part_pos3( part ) ) ) ) {
                continue;
            }

            epower_w += part_epower_w( part );
        }
        double intensity = accum_weather.sunlight / default_daylight_level() / to_turns<double>( elapsed );
        int energy_bat = power_to_energy_bat( epower_w * intensity, elapsed );
        if( energy_bat > 0 ) {
            add_msg( m_debug, "%s got %d kJ energy from solar panels", name, energy_bat );
            charge_battery( energy_bat );
        }
    }
    if( !wind_turbines.empty() ) {
        // TODO: use accum_weather wind data to backfill wind turbine
        // generation capacity.
        int epower_w = total_wind_epower_w();
        int energy_bat = power_to_energy_bat( epower_w, elapsed );
        if( energy_bat > 0 ) {
            add_msg( m_debug, "%s got %d kJ energy from wind turbines", name, energy_bat );
            charge_battery( energy_bat );
        }
    }
    if( !water_wheels.empty() ) {
        int epower_w = total_water_wheel_epower_w();
        int energy_bat = power_to_energy_bat( epower_w, elapsed );
        if( energy_bat > 0 ) {
            add_msg( m_debug, "%s got %d kJ energy from water wheels", name, energy_bat );
            charge_battery( energy_bat );
        }
    }
}

void vehicle::process_emitters()
{
    // Parts emitting fields
    for( int idx : emitters ) {
        const vehicle_part &pt = parts[idx];
        if( pt.is_unavailable() || !pt.enabled ) {
            continue;
        }
        for( const emit_id &e : pt.info().emissions ) {
            g->m.emit_field( global_part_pos3( pt ), e );
        }
    }
}

void vehicle::invalidate_mass()
{
    mass_dirty = true;
    mass_center_precalc_dirty = true;
    mass_center_no_precalc_dirty = true;
    // Anything that affects mass will also affect the pivot
    pivot_dirty = true;
    coeff_rolling_dirty = true;
    coeff_water_dirty = true;
}

void vehicle::refresh_mass() const
{
    calc_mass_center( true );
}

void vehicle::calc_mass_center( bool use_precalc ) const
{
    units::quantity<float, units::mass::unit_type> xf;
    units::quantity<float, units::mass::unit_type> yf;
    units::mass m_total = 0_gram;
    for( const vpart_reference &vp : get_all_parts() ) {
        const size_t i = vp.part_index();
        if( vp.part().removed ) {
            continue;
        }

        units::mass m_part = 0_gram;
        units::mass m_part_items = 0_gram;
        m_part += vp.part().base->weight();
        const int weight_modifier = vp.part().info().weight_modifier;
        const int cargo_weight_modifier = vp.part().info().cargo_weight_modifier;
        if( weight_modifier != 100 ) {
            m_part *= static_cast<float>( weight_modifier ) / 100.0f;
        }
        for( const auto &j : get_items( i ) ) {
            m_part_items += j->weight();
        }
        if( cargo_weight_modifier != 100 ) {
            m_part_items *= static_cast<float>( cargo_weight_modifier ) / 100.0f;
        }
        m_part += m_part_items;

        if( vp.has_feature( VPFLAG_BOARDABLE ) && vp.part().has_flag( vehicle_part::passenger_flag ) ) {
            const player *p = get_passenger( i );
            // Sometimes flag is wrongly set, don't crash!
            m_part += p != nullptr ? p->get_weight() : 0_gram;
        }

        if( use_precalc ) {
            xf += vp.part().precalc[0].x * m_part;
            yf += vp.part().precalc[0].y * m_part;
        } else {
            xf += vp.mount().x * m_part;
            yf += vp.mount().y * m_part;
        }

        m_total += m_part;
    }

    mass_cache = m_total;
    mass_dirty = false;

    const float x = xf / mass_cache;
    const float y = yf / mass_cache;
    if( use_precalc ) {
        mass_center_precalc.x = std::round( x );
        mass_center_precalc.y = std::round( y );
        mass_center_precalc_dirty = false;
    } else {
        mass_center_no_precalc.x = std::round( x );
        mass_center_no_precalc.y = std::round( y );
        mass_center_no_precalc_dirty = false;
    }
}

bounding_box vehicle::get_bounding_box( )
{
    int min_x = INT_MAX;
    int max_x = INT_MIN;
    int min_y = INT_MAX;
    int max_y = INT_MIN;

    set_facing( turn_dir );

    int i_use = 0;
    for( const tripoint &p : get_points( true ) ) {
        const point pt = parts[part_at( p.xy() )].precalc[i_use].xy();
        if( pt.x < min_x ) {
            min_x = pt.x;
        }
        if( pt.x > max_x ) {
            max_x = pt.x;
        }
        if( pt.y < min_y ) {
            min_y = pt.y;
        }
        if( pt.y > max_y ) {
            max_y = pt.y;
        }
    }
    bounding_box b;
    b.p1 = point( min_x, min_y );
    b.p2 = point( max_x, max_y );
    return b;
}

vehicle_part_range vehicle::get_all_parts() const
{
    return vehicle_part_range( const_cast<vehicle &>( *this ) );
}

int vehicle::part_count() const
{
    return static_cast<int>( parts.size() );
}

vehicle_part &vehicle::part( int part_num )
{
    return parts[part_num];
}

const vehicle_part &vehicle::cpart( int part_num ) const
{
    return const_cast<vehicle_part &>( parts[part_num] );
}

bool vehicle::valid_part( int part_num ) const
{
    return part_num >= 0 && part_num < static_cast<int>( parts.size() );
}

std::set<int> vehicle::advance_precalc_mounts( point new_pos, const tripoint &src )
{
    map &here = get_map();
    std::set<int> smzs;
    for( vehicle_part &prt : parts ) {
        here.clear_vehicle_point_from_cache( this, src + prt.precalc[0] );
        prt.precalc[0] = prt.precalc[1];

        smzs.insert( prt.precalc[0].z );
    }

    pivot_anchor[0] = pivot_anchor[1];
    pivot_rotation[0] = pivot_rotation[1];
    pos = new_pos;

    // Invalidate vehicle's point cache
    occupied_cache_time = calendar::before_time_starts;
    return smzs;
}

bool vehicle::refresh_zones()
{
    if( zones_dirty ) {
        decltype( loot_zones ) new_zones;
        for( auto const &z : loot_zones ) {
            zone_data zone = z.second;
            //Get the global position of the first cargo part at the relative coordinate

            const int part_idx = part_with_feature( z.first, "CARGO", false );
            if( part_idx == -1 ) {
                debugmsg( "Could not find cargo part at %d,%d on vehicle %s for loot zone.  Removing loot zone.",
                          z.first.x, z.first.y, this->name );

                // If this loot zone refers to a part that no longer exists at this location, then its unattached somehow.
                // By continuing here and not adding to new_zones, we effectively remove it
                continue;
            }
            tripoint zone_pos = global_part_pos3( part_idx );
            zone_pos = g->m.getabs( zone_pos );
            //Set the position of the zone to that part
            zone.set_position( std::pair<tripoint, tripoint>( zone_pos, zone_pos ), false );
            new_zones.emplace( z.first, zone );
        }
        loot_zones = new_zones;
        zones_dirty = false;
        return true;
    }
    return false;
}

template<>
bool vehicle_part_with_feature_range<std::string>::matches( const size_t part ) const
{
    const vehicle_part &vp = this->vehicle().part( part );
    return vp.info().has_flag( feature_ ) &&
           !vp.removed &&
           ( !( part_status_flag::working & required_ ) || !vp.is_broken() ) &&
           ( !( part_status_flag::available & required_ ) || vp.is_available() ) &&
           ( !( part_status_flag::enabled & required_ ) || vp.enabled );
}

template<>
bool vehicle_part_with_feature_range<vpart_bitflags>::matches( const size_t part ) const
{
    const vehicle_part &vp = this->vehicle().part( part );
    return vp.info().has_flag( feature_ ) &&
           !vp.removed &&
           ( !( part_status_flag::working & required_ ) || !vp.is_broken() ) &&
           ( !( part_status_flag::available & required_ ) || vp.is_available() ) &&
           ( !( part_status_flag::enabled & required_ ) || vp.enabled );
}

bool vehicle::is_loaded() const
{
    return attached && get_map().inbounds( global_pos3() );
}

void vehicle::refresh_locations_hack()
{
    for( vehicle_part &part : parts ) {
        part.refresh_locations_hack( this );
    }
}

vehicle_part &vehicle::get_part_hack( int id )
{
    for( vehicle_part &part : parts ) {
        if( part.hack_id == id ) {
            return part;
        }
    }
    debugmsg( "Could not find part via hack id" );
    return parts[0];
}

int vehicle::get_part_id_hack( int id )
{
    int i = 0;
    for( vehicle_part &part : parts ) {
        if( part.hack_id == id ) {
            return i;
        }
        i++;
    }
    debugmsg( "Could not find part id via hack id" );
    return -1;
}

// The mythical invokation to summon the cargo part on the same tile
vehicle_part *vehicle::get_cargo_part( vehicle_part *part )
{
    vehicle_part *vp = nullptr;
    int vpr = part_with_feature( part->mount, "CARGO", false );
    if( vpr != -1 ) {
        vp = &parts[ vpr ];
    }
    return vp;
}
bool vehicle::has_item_stored( vehicle_part *part )
{
    vehicle_part *vp = get_cargo_part( part );
    if( vp ) {
        return stored_volume( index_of_part( vp ) ) > 0_ml;
    }
    return false;
}

void vehicle::item_dropper_drop( std::vector<vehicle_part *> droppers, bool single )
{
    if( single ) {
        vehicle_part *part = get_cargo_part( droppers[0] );
        std::vector<std::string> option_names;
        std::vector<item *> options;
        for( item *it : part->items ) {
            option_names.push_back( it->display_name() );
            options.push_back( it );
        }
        const int idx = uilist( _( "Drop which item?" ), option_names );
        if( idx < 0 ) {
            return;
        }
        map &here = get_map();
        tripoint pos = global_part_pos3( index_of_part( part ) );
        while( here.has_flag_ter_or_furn( TFLAG_NO_FLOOR, pos ) ) {
            pos.z -= 1;
        }
        item *dropper = options[idx];
        if( dropper->get_use( "transform" ) ) {
            g->u.invoke_item( dropper, "transform" );
        }
        g->m.add_item_or_charges( pos, part->remove_item( *dropper ) );
    } else {
        for( vehicle_part *d : droppers ) {
            vehicle_part *part = get_cargo_part( d );
            map &here = get_map();
            if( part ) {
                tripoint pos = global_part_pos3( index_of_part( part ) );
                while( here.has_flag_ter_or_furn( TFLAG_NO_FLOOR, pos ) ) {
                    pos.z -= 1;
                }
                // DANGER: DO NOT PUT THIS IN THE FOR LOOP
                const std::vector<item *> items = part->get_items();
                for( item *it : items ) {
                    if( it->get_use( "transform" ) ) {
                        g->u.invoke_item( it, "transform" );
                    }
                    g->m.add_item_or_charges( pos, part->remove_item( *it ) );
                }
            }
        }
    }
}

void vehicle::item_dropper_drop_single( bool single )
{
    std::vector<std::string> option_names;
    std::vector<vehicle_part *> options;

    // Find all droppers that are loaded
    for( int idx : droppers ) {
        vehicle_part *d = &parts[ idx ];
        if( has_item_stored( d ) ) {
            option_names.push_back( d->name() );
            options.push_back( d );
        }
    }

    // Select one
    if( options.empty() ) {
        add_msg( m_warning, _( "None of the droppers are loaded." ) );
        return;
    }
    const int idx = uilist( _( "Drop from which dropper?" ), option_names );
    if( idx < 0 ) {
        return;
    }
    vehicle_part *dropper = options[idx];

    std::vector<vehicle_part *> droppers;
    droppers.push_back( dropper );
    item_dropper_drop( droppers, single );
}

void vehicle::item_dropper_drop_all( )
{
    std::vector<vehicle_part *> ret;

    // Find all droppers that are loaded
    for( int idx : droppers ) {
        vehicle_part *d = &parts[ idx ];
        if( has_item_stored( d ) ) {
            ret.push_back( d );
        }
    }

    item_dropper_drop( ret, false );
}
