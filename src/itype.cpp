#include "itype.h"

#include <cstdlib>

#include "catalua_icallback_actor.h"
#include "debug.h"
#include "item.h"
#include "make_static.h"
#include "player.h"
#include "ret_val.h"
#include "translations.h"
#include "relic.h"
#include "skill.h"
#include "options.h"

struct tripoint;

std::string gunmod_location::name() const
{
    // Yes, currently the name is just the translated id.
    return _( _id );
}

namespace io
{
template<>
std::string enum_to_string<condition_type>( condition_type data )
{
    switch( data ) {
        case condition_type::FLAG:
            return "FLAG";
        case condition_type::VITAMIN:
            return "VITAMIN";
        case condition_type::COMPONENT_ID:
            return "COMPONENT_ID";
        case condition_type::num_condition_types:
            break;
    }
    debugmsg( "Invalid condition_type" );
    abort();
}
} // namespace io

itype::itype()
{
    melee.fill( 0 );
    attacks["DEFAULT"] = attack_statblock();
}

itype::~itype() = default;

int itype::damage_min() const
{
    return count_by_charges() ? 0 : damage_min_;
}

int itype::damage_max() const
{
    return count_by_charges() ? 0 : damage_max_;
}

std::string itype::get_item_type_string() const
{
    if( tool ) {
        return "TOOL";
    } else if( comestible ) {
        return "FOOD";
    } else if( container ) {
        return "CONTAINER";
    } else if( armor ) {
        return "ARMOR";
    } else if( book ) {
        return "BOOK";
    } else if( gun ) {
        return "GUN";
    } else if( bionic ) {
        return "BIONIC";
    } else if( ammo ) {
        return "AMMO";
    }
    return "misc";
}

std::string itype::nname( unsigned int quantity ) const
{
    // Always use singular form for liquids.
    // (Maybe gases too?  There are no gases at the moment)
    if( phase == LIQUID ) {
        quantity = 1;
    }
    return name.translated( quantity );
}

const itype_id &itype::get_id() const
{
    return id;
}

bool itype::count_by_charges() const
{
    return stackable_ || ammo || comestible;
}

int itype::charges_default() const
{
    if( tool ) {
        return tool->def_charges;
    } else if( comestible ) {
        return comestible->def_charges;
    } else if( ammo ) {
        return ammo->def_charges;
    }
    return count_by_charges() ? 1 : 0;
}

int itype::charges_to_use() const
{
    if( tool ) {
        return tool->charges_per_use;
    }
    return 1;
}

int itype::charge_factor() const
{
    return tool ? tool->charge_factor : 1;
}

int itype::maximum_charges() const
{
    if( tool ) {
        return tool->max_charges;
    }
    return 0;
}

int itype::charges_per_volume( const units::volume &vol ) const
{
    if( volume == 0_ml ) {
        // TODO: items should not have 0 volume at all!
        return item::INFINITE_CHARGES;
    }
    return ( count_by_charges() ? stack_size : 1 ) * vol / volume;
}

// Members of iuse struct, which is slowly morphing into a class.
bool itype::has_use() const
{
    return !use_methods.empty();
}

bool itype::has_flag( const flag_id &flag ) const
{
    return item_tags.contains( flag );
}

const itype::FlagsSetType &itype::get_flags() const
{
    return item_tags;
}

bool itype::can_use( const std::string &iuse_name ) const
{
    return get_use( iuse_name ) != nullptr;
}

const use_function *itype::get_use( const std::string &iuse_name ) const
{
    const auto iter = use_methods.find( iuse_name );
    return iter != use_methods.end() ? &iter->second : nullptr;
}

void itype::tick( player &p, item &it, const tripoint &pos ) const
{
    // If istate_callbacks defines on_tick, use it instead of legacy use_methods loop
    if( istate_callbacks && istate_callbacks->has_on_tick() ) {
        istate_callbacks->call_on_tick( p, it, pos );
        return;
    }
    // Legacy fallback: tick via use_methods (iuse tick_func)
    // Maybe should move charge decrementing here?
    for( auto &method : use_methods ) {
        method.second.call( p, it, true, pos );
    }
}

int itype::invoke( player &p, item &it, const tripoint &pos ) const
{
    if( !has_use() ) {
        return 0;
    }
    return invoke( p, it, pos, use_methods.begin()->first );
}

int itype::invoke( player &p, item &it, const tripoint &pos, const std::string &iuse_name ) const
{
    const use_function *use = get_use( iuse_name );
    if( use == nullptr ) {
        debugmsg( "Tried to invoke %s on a %s, which doesn't have this use_function",
                  iuse_name, nname( 1 ) );
        return 0;
    }

    const auto ret = use->can_call( p, it, false, pos );

    if( !ret.success() ) {
        p.add_msg_if_player( m_info, ret.str() );
        return 0;
    }
    // used for grenades and such, to increase kill count
    // invoke is called a first time with transform, when the explosive item is activated
    // then a second time with draw explosion
    // the player responsible of the explosion is the one that activated the object
    if( iuse_name == "transform" ) {
        //TODO!: put this back to a safe reference once players are added
        it.activated_by = &p;
    }

    return use->call( p, it, false, pos );
}

bool itype::can_have_charges() const
{
    if( count_by_charges() ) {
        return true;
    }
    if( tool && tool->max_charges > 0 ) {
        return true;
    }
    if( gun && gun->clip > 0 ) {
        return true;
    }
    if( has_flag( STATIC( flag_id( "CAN_HAVE_CHARGES" ) ) ) ) {
        return true;
    }
    return false;
}

bool itype::is_seed() const
{
    return !!seed;
}


time_duration islot_seed::get_plant_epoch() const
{
    const int scaling = get_option<int>( "GROWTH_SCALING" );
    // incorporate growth time scaling option
    if( scaling == 0 ) {
        // If scaling factor is not set, scale growth time based on
        // current season length relative to the default of 14 days
        return grow * calendar::season_ratio() / 3;
    }
    // Otherwise apply the explicitly set scaling value
    // Also note that seed->grow is the time it takes from seeding to harvest, this is
    // divided by 3 to get the time it takes from one plant state to the next.
    return grow * scaling / 300.0;
}