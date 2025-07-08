#include "vehicle.h"
#include "vehicle_part.h" // IWYU pragma: associated

#include <algorithm>
#include <cassert>
#include <cmath>
#include <memory>
#include <set>

#include "avatar.h"
#include "color.h"
#include "debug.h"
#include "enums.h"
#include "flag.h"
#include "flat_set.h"
#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "itype.h"
#include "locations.h"
#include "map.h"
#include "messages.h"
#include "npc.h"
#include "string_formatter.h"
#include "translations.h"
#include "value_ptr.h"
#include "veh_type.h"
#include "vpart_position.h"
#include "weather.h"

static const itype_id fuel_type_battery( "battery" );
static const itype_id fuel_type_none( "null" );

static const itype_id itype_battery( "battery" );
static const itype_id itype_muscle( "muscle" );

/*-----------------------------------------------------------------------------
 *                              VEHICLE_PART
 *-----------------------------------------------------------------------------*/
vehicle_part::vehicle_part( vehicle *veh )
    : id( vpart_id::NULL_ID() ), hack_id( veh->get_next_hack_id() ),
      base( new vehicle_base_item_location( veh, hack_id ) ),
      items( new vehicle_item_location( veh, hack_id ) ) {}

//This is a bit of a hack until vehicles are GOs
vehicle_part::vehicle_part()
    : id( vpart_id::NULL_ID() ), base( new fake_item_location() ), items( new fake_item_location() ) {}

vehicle_part::vehicle_part( const vpart_id &vp, point dp, detached_ptr<item> &&obj, vehicle *veh )
    : mount( dp ), id( vp ),
      base( new vehicle_base_item_location( veh, hack_id ) ),
      items( new vehicle_item_location( veh, hack_id ) )
{
    base = std::move( obj );

    // Mark base item as being installed as a vehicle part
    base->set_flag( flag_VEHICLE );

    if( base->typeId() != vp->item ) {
        debugmsg( "incorrect vehicle part item, expected: %s, received: %s",
                  vp->item.c_str(), base->typeId().c_str() );
    }
}

void vehicle_part::set_vehicle_hack( vehicle *veh )
{
    hack_id = veh->get_next_hack_id();
    refresh_locations_hack( veh );
}

void vehicle_part::refresh_locations_hack( vehicle *veh )
{
    base.set_loc_hack( new vehicle_base_item_location( veh, hack_id ) );
    items.set_loc_hack( new vehicle_item_location( veh, hack_id ) );
}

void vehicle_part::copy_static_from( const vehicle_part &source )
{
    carry_names = source.carry_names;
    mount = source.mount;
    precalc = source.precalc;
    blood = source.blood;
    inside = source.inside;
    removed = source.removed;
    enabled = source.enabled;
    flags = source.flags;
    passenger_id = source.passenger_id;
    open = source.open;
    direction = source.direction;
    proxy_part_id = source.proxy_part_id;
    proxy_sym = source.proxy_sym;
    target = source.target;
    id = source.id;
    info_cache = source.info_cache;
    ammo_pref = source.ammo_pref;
    crew_id = source.crew_id;
    hack_id = source.hack_id;
}

//TODO!: This is a bit scuffed and will be until vehicles are game objects.
vehicle_part::vehicle_part( const vehicle_part &source, vehicle *veh ) : vehicle_part( veh )
{
    copy_static_from( source );
    base = item::spawn( *source.base );
    for( const item * const &it : source.items ) {
        items.push_back( item::spawn( *it ) );
    }
}

vehicle_part::vehicle_part( vehicle_part &&source ) : vehicle_part()
{
    copy_static_from( source );
    base = source.base.release();
    for( detached_ptr<item> &it : source.items.clear() ) {
        items.push_back( std::move( it ) );
    }
}

vehicle_part &vehicle_part::operator=( vehicle_part &&source )
{
    copy_static_from( source );
    base = source.base.release();
    items.clear();
    for( detached_ptr<item> &it : source.items.clear() ) {
        items.push_back( std::move( it ) );
    }
    return *this;
}

vehicle_part::operator bool() const
{
    return id != vpart_id::NULL_ID();
}

item &vehicle_part::get_base() const
{
    return *base;
}

detached_ptr<item> vehicle_part::set_base( detached_ptr<item> &&new_base )
{
    return base.swap( std::move( new_base ) );
}

detached_ptr<item> vehicle_part::properties_to_item() const
{
    //TODO!: the big check
    detached_ptr<item> tmp = item::spawn( *base );
    tmp->unset_flag( flag_VEHICLE );

    // Cables get special handling: their target coordinates need to remain
    // stored, and if a cable actually drops, it should be half-connected.
    // Except grid-connected ones, for now.
    if( tmp->has_flag( flag_CABLE_SPOOL ) ) {
        auto data = cable_connection_data::make_data( *tmp );
        if( !data || tmp->has_flag( flag_TOW_CABLE ) ) {
            tmp->reset_cable();
        } else {
            if( data->intermap_connection() ) {
                if( data->con1.point.raw() == target.first ) {
                    data->unset_con2( tmp.get() );
                } else if( data->con2.point.raw() == target.first ) {
                    data->unset_con1( tmp.get() );
                } else {
                    tmp->reset_cable();
                }
                if( !has_flag( targets_grid ) ) {
                    map &here = get_map();
                    const tripoint local_pos = here.getlocal( target.first );
                    if( !here.veh_at( local_pos ) ) {
                        // That vehicle ain't there no more.
                        tmp->set_flag( flag_NO_DROP );
                    }
                }
            } else if( data->character_connected() ) {
                if( auto *const con = data->get_nonchar_connection() ) {
                    data->unset_con( tmp.get(), *con );
                } else {
                    tmp->reset_cable();
                }
            } else {
                tmp->reset_cable();
            }
        }
    }

    // force rationalization of damage values to the middle value of each damage level so
    // that parts will stack nicely
    tmp->set_damage( tmp->damage_level( 4 ) * itype::damage_scale );
    return tmp;
}

void vehicle_part::add_item( detached_ptr<item> &&item )
{
    items.push_back( std::move( item ) );
}

std::string vehicle_part::name( bool with_prefix ) const
{
    auto res = info().name();

    if( base->engine_displacement() > 0 ) {
        res.insert( 0, string_format( _( "%2.1fL " ), base->engine_displacement() / 100.0 ) );

    } else if( wheel_diameter() > 0 ) {
        res.insert( 0, string_format( _( "%d\" " ), wheel_diameter() ) );
    }

    if( base->is_faulty() ) {
        res += _( " (faulty)" );
    }

    if( base->has_var( "contained_name" ) ) {
        res += string_format( _( " holding %s" ), base->get_var( "contained_name" ) );
    }

    if( is_leaking() ) {
        res += _( " (draining)" );
    }

    if( with_prefix ) {
        res.insert( 0, colorize( base->damage_symbol(), base->damage_color() ) + " " );
    }
    return res;
}

int vehicle_part::hp() const
{
    const int dur = info().durability;
    if( base->max_damage() > 0 ) {
        return dur - dur * base->damage() / base->max_damage();
    } else {
        return dur;
    }
}

int vehicle_part::damage() const
{
    return base->damage();
}

int vehicle_part::max_damage() const
{
    return base->max_damage();
}

int vehicle_part::damage_level( int max ) const
{
    return base->damage_level( max );
}

double vehicle_part::health_percent() const
{
    return 1.0 - static_cast<double>( base->damage() ) / base->max_damage();
}

double vehicle_part::damage_percent() const
{
    return static_cast<double>( base->damage() ) / base->max_damage();
}

/** parts are considered broken at zero health */
bool vehicle_part::is_broken() const
{
    return base->count_by_charges() ? false : base->damage() >= base->max_damage();
}

bool vehicle_part::is_unavailable( const bool carried ) const
{
    return is_broken() || ( has_flag( carried_flag ) && carried );
}

bool vehicle_part::is_available( const bool carried ) const
{
    return !is_unavailable( carried );
}

itype_id vehicle_part::fuel_current() const
{
    if( is_engine() ) {
        if( ammo_pref.is_null() ) {
            return info().fuel_type != itype_muscle ? info().fuel_type : itype_id::NULL_ID();
        } else {
            return ammo_pref;
        }
    }

    return itype_id::NULL_ID();
}

bool vehicle_part::fuel_set( const itype_id &fuel )
{
    if( is_engine() ) {
        for( const itype_id &avail : info().engine_fuel_opts() ) {
            if( fuel == avail ) {
                ammo_pref = fuel;
                return true;
            }
        }
    }
    return false;
}

itype_id vehicle_part::ammo_current() const
{
    if( is_battery() ) {
        return itype_battery;
    }

    if( is_tank() && !base->contents.empty() ) {
        return base->contents.front().typeId();
    }

    if( is_fuel_store( false ) || is_turret() ) {
        return base->ammo_current();
    }

    return itype_id::NULL_ID();
}

int vehicle_part::ammo_capacity() const
{
    if( is_tank() ) {
        return ammo_current()->charges_per_volume( base->get_container_capacity() );
    }

    if( is_fuel_store( false ) || is_turret() ) {
        return base->ammo_capacity();
    }

    return 0;
}

int vehicle_part::ammo_remaining() const
{
    if( is_tank() ) {
        return base->contents.empty() ? 0 : base->contents.back().charges;
    }

    if( is_fuel_store( false ) || is_turret() ) {
        return base->ammo_remaining();
    }

    return 0;
}

int vehicle_part::ammo_set( const itype_id &ammo, int qty )
{
    const itype *liquid = &*ammo;

    // We often check if ammo is set to see if tank is empty, if qty == 0 don't set ammo
    if( is_tank() && liquid->phase >= LIQUID && qty != 0 ) {
        base->contents.clear_items();
        const auto stack = units::legacy_volume_factor / std::max( liquid->stack_size, 1 );
        const int limit = units::from_milliliter( ammo_capacity() ) / stack;
        base->put_in( item::spawn( ammo, calendar::turn, qty > 0 ? std::min( qty, limit ) : limit ) );
        return qty;
    }

    if( is_turret() ) {
        base->ammo_set( ammo, qty );
        return base->ammo_remaining();
    }

    if( is_fuel_store() ) {
        base->ammo_set( ammo, qty >= 0 ? qty : ammo_capacity() );
        return base->ammo_remaining();
    }

    return -1;
}

void vehicle_part::ammo_unset()
{
    if( is_tank() ) {
        base->contents.clear_items();
    } else if( is_fuel_store() ) {
        base->ammo_unset();
    }
}

int vehicle_part::ammo_consume( int qty, const tripoint &pos )
{
    if( is_tank() && !base->contents.empty() ) {
        const int res = std::min( ammo_remaining(), qty );
        item &liquid = base->contents.back();
        liquid.charges -= res;
        if( liquid.charges == 0 ) {
            base->contents.clear_items();
        }
        return res;
    }
    return base->ammo_consume( qty, pos );
}

double vehicle_part::consume_energy( const itype_id &ftype, double energy_j )
{
    if( base->contents.empty() || !is_fuel_store() ) {
        return 0.0f;
    }

    item &fuel = base->contents.back();
    if( fuel.typeId() == ftype ) {
        assert( fuel.is_fuel() );
        // convert energy density in MJ/L to J/ml
        const double energy_p_mL = fuel.fuel_energy() * 1000;
        const int ml_to_use = static_cast<int>( std::floor( energy_j / energy_p_mL ) );
        int charges_to_use = fuel.charges_per_volume( ml_to_use * 1_ml );

        if( !charges_to_use ) {
            return 0.0;
        }
        if( charges_to_use >= fuel.charges ) {
            charges_to_use = fuel.charges;
            base->contents.clear_items();
        } else {
            fuel.charges -= charges_to_use;
        }
        //TODO!: push up
        item &fuel_consumed = *item::spawn_temporary( ftype, calendar::turn, charges_to_use );
        return energy_p_mL * units::to_milliliter<int>( fuel_consumed.volume( true ) );
    }
    return 0.0;
}

bool vehicle_part::can_reload( const item *obj ) const
{
    // first check part is not destroyed and can contain ammo
    if( !is_fuel_store() ) {
        return false;
    }

    if( obj != nullptr && !obj->is_null() ) {
        const itype_id obj_type = obj->typeId();
        if( is_reactor() ) {
            return base->is_reloadable_with( obj_type );
        }

        // forbid filling tanks with solids or non-material things
        if( is_tank() && ( obj->made_of( SOLID ) || obj->made_of( PNULL ) ) ) {
            return false;
        }
        // forbid putting liquids, gasses, and plasma in things that aren't tanks
        else if( !obj->made_of( SOLID ) && !is_tank() ) {
            return false;
        }
        // prevent mixing of different ammo
        if( !ammo_current().is_null() && ammo_current() != obj_type ) {
            return false;
        }
        // For storage with set type, prevent filling with different types
        if( info().fuel_type != fuel_type_none && info().fuel_type != obj_type ) {
            return false;
        }
        // don't fill magazines with inappropriate fuel
        if( !is_tank() && !base->is_reloadable_with( obj_type ) ) {
            return false;
        }
    }

    return ammo_remaining() < ammo_capacity();
}

void vehicle_part::process_contents( const tripoint &pos, const bool e_heater )
{
    // for now we only care about processing food containers since things like
    // fuel don't care about temperature yet
    if( base->is_food_container() ) {
        temperature_flag flag = temperature_flag::TEMP_NORMAL;
        if( e_heater ) {
            flag = temperature_flag::TEMP_HEATER;
        }

        const vpart_info vpinfo = info();
        if( enabled && vpinfo.has_flag( VPFLAG_FRIDGE ) ) {
            flag = temperature_flag::TEMP_FRIDGE;
        } else if( enabled && vpinfo.has_flag( VPFLAG_FREEZER ) ) {
            flag = temperature_flag::TEMP_FREEZER;
        }

        base = item::process( base.release(), nullptr, pos, false, flag );
    }
}

detached_ptr<item> vehicle_part::fill_with( detached_ptr<item> &&liquid, int qty )
{
    if( !is_tank() || !can_reload( &*liquid ) ) {
        return std::move( liquid );
    }

    return base->fill_with( std::move( liquid ), qty );
}

const std::set<fault_id> &vehicle_part::faults() const
{
    return base->faults;
}

std::set<fault_id> vehicle_part::faults_potential() const
{
    return base->faults_potential();
}

bool vehicle_part::fault_set( const fault_id &f )
{
    if( !faults_potential().contains( f ) ) {
        return false;
    }
    base->faults.insert( f );
    return true;
}

int vehicle_part::wheel_area() const
{
    return info().wheel_area();
}

/** Get wheel diameter (inches) or return 0 if part is not wheel */
int vehicle_part::wheel_diameter() const
{
    return base->is_wheel() ? base->type->wheel->diameter : 0;
}

/** Get wheel width (inches) or return 0 if part is not wheel */
int vehicle_part::wheel_width() const
{
    return base->is_wheel() ? base->type->wheel->width : 0;
}

npc *vehicle_part::crew() const
{
    if( is_broken() || !crew_id.is_valid() ) {
        return nullptr;
    }

    npc *const res = g->critter_by_id<npc>( crew_id );
    if( !res ) {
        return nullptr;
    }
    return res->is_player_ally() ? res : nullptr;
}

bool vehicle_part::set_crew( const npc &who )
{
    if( who.is_dead_state() || !( who.is_walking_with() || who.is_player_ally() ) ) {
        return false;
    }
    if( is_broken() || ( !is_seat() && !is_turret() ) ) {
        return false;
    }
    crew_id = who.getID();
    return true;
}

void vehicle_part::unset_crew()
{
    crew_id = character_id();
}

void vehicle_part::reset_target( const tripoint &pos )
{
    target.first = pos;
    target.second = pos;
}

bool vehicle_part::is_engine() const
{
    return info().has_flag( VPFLAG_ENGINE );
}

bool vehicle_part::is_light() const
{
    const auto &vp = info();
    return vp.has_flag( VPFLAG_CONE_LIGHT ) ||
           vp.has_flag( VPFLAG_WIDE_CONE_LIGHT ) ||
           vp.has_flag( VPFLAG_HALF_CIRCLE_LIGHT ) ||
           vp.has_flag( VPFLAG_CIRCLE_LIGHT ) ||
           vp.has_flag( VPFLAG_AISLE_LIGHT ) ||
           vp.has_flag( VPFLAG_DOME_LIGHT ) ||
           vp.has_flag( VPFLAG_ATOMIC_LIGHT );
}

bool vehicle_part::is_fuel_store( bool skip_broke ) const
{
    if( skip_broke && is_broken() ) {
        return false;
    }
    return is_tank() || base->is_magazine() || is_reactor();
}

bool vehicle_part::is_tank() const
{
    return base->is_watertight_container();
}

bool vehicle_part::is_battery() const
{
    return base->is_magazine() && base->ammo_types().contains( ammotype( "battery" ) );
}

bool vehicle_part::is_reactor() const
{
    return info().has_flag( VPFLAG_REACTOR );
}

bool vehicle_part::is_leaking() const
{
    return  health_percent() <= 0.5 && ( is_tank() || is_battery() || is_reactor() );
}

bool vehicle_part::is_turret() const
{
    return base->is_gun();
}

bool vehicle_part::is_seat() const
{
    return info().has_flag( "SEAT" );
}

const vpart_info &vehicle_part::info() const
{
    if( !info_cache ) {
        info_cache = &id.obj();
    }
    return *info_cache;
}

void vehicle::set_hp( vehicle_part &pt, int qty )
{
    if( qty == pt.info().durability || pt.info().durability <= 0 ) {
        pt.base->set_damage( 0 );

    } else if( qty == 0 ) {
        pt.base->set_damage( pt.base->max_damage() );

    } else {
        pt.base->set_damage( pt.base->max_damage() - pt.base->max_damage() * qty / pt.info().durability );
    }
}

bool vehicle::mod_hp( vehicle_part &pt, int qty, damage_type dt )
{
    if( pt.info().durability > 0 ) {
        return pt.base->mod_damage( -( pt.base->max_damage() * qty / pt.info().durability ), dt );
    } else {
        return false;
    }
}

bool vehicle::can_enable( const vehicle_part &pt, bool alert ) const
{
    if( std::ranges::none_of( parts, [&pt]( const vehicle_part & e ) {
    return &e == &pt;
} ) || pt.removed ) {
        debugmsg( "Cannot enable removed or non-existent part" );
    }

    if( pt.is_broken() ) {
        return false;
    }

    // Disallow running a planter underground for now
    if( pt.info().has_flag( "PLANTER" ) && ( !warm_enough_to_plant( g->u.pos() ) ||
            global_pos3().z < 0 ) ) {
        if( alert ) {
            add_msg( m_bad, _( "It is too cold to plant anything now." ) );
        }
        return false;
    }

    // TODO: check fuel for combustion engines

    if( pt.info().epower < 0 && fuel_left( fuel_type_battery, true ) <= 0 ) {
        if( alert ) {
            add_msg( m_bad, _( "Insufficient power to enable %s" ), pt.name() );
        }
        return false;
    }

    return true;
}

bool vehicle::assign_seat( vehicle_part &pt, const npc &who )
{
    if( !pt.is_seat() || !pt.set_crew( who ) ) {
        return false;
    }

    // NPC's can only be assigned to one seat in the vehicle
    for( auto &e : parts ) {
        if( &e == &pt ) {
            // skip this part
            continue;
        }

        if( e.is_seat() ) {
            const npc *n = e.crew();
            if( n && n->getID() == who.getID() ) {
                e.unset_crew();
            }
        }
    }

    return true;
}

std::string vehicle_part::carried_name() const
{
    if( carry_names.empty() ) {
        return std::string();
    }
    return carry_names.top().substr( name_offset );
}
